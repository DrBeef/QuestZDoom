// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_setup.cpp
** Initializes the data structures required by the GL renderer to handle
** a level
**
**/

#include "doomtype.h"
#include "colormatcher.h"
#include "i_system.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "c_dispatch.h"
#include "r_sky.h"
#include "sc_man.h"
#include "w_wad.h"
#include "gi.h"
#include "g_level.h"
#include "a_sharedglobal.h"
#include "g_levellocals.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_clipper.h"
#include "gl/scene/gl_portal.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/utility/gl_clock.h"
#include "gl/gl_functions.h"

struct FPortalID
{
	DVector2 mDisplacement;

	// for the hash code
	operator intptr_t() const { return (FLOAT2FIXED(mDisplacement.X) >> 8) + (FLOAT2FIXED(mDisplacement.Y) << 8); }
	bool operator != (const FPortalID &other) const
	{
		return mDisplacement != other.mDisplacement;
	}
};

struct FPortalSector
{
	sector_t *mSub;
	int mPlane;
};

typedef TArray<FPortalSector> FPortalSectors;

typedef TMap<FPortalID, FPortalSectors> FPortalMap;

TArray<FPortal *> glSectorPortals;
TArray<FGLLinePortal*> linePortalToGL;
TArray<FGLLinePortal> glLinePortals;

//==========================================================================
//
//
//
//==========================================================================

GLSectorStackPortal *FPortal::GetRenderState()
{
	if (glportal == NULL) glportal = new GLSectorStackPortal(this);
	return glportal;
}

//==========================================================================
//
// this is left as fixed_t because the nodes also are, it makes no sense
// to convert this without converting the nodes as well.
//
//==========================================================================

struct FCoverageVertex
{
	fixed_t x, y;

	bool operator !=(FCoverageVertex &other)
	{
		return x != other.x || y != other.y;
	}
};

struct FCoverageLine
{
	FCoverageVertex v[2];
};

struct FCoverageBuilder
{
	subsector_t *target;
	TArray<int> collect;
	FCoverageVertex center;

	//==========================================================================
	//
	//
	//
	//==========================================================================

	FCoverageBuilder(subsector_t *sub)
	{
		target = sub;
	}

	//==========================================================================
	//
	// GetIntersection
	//
	// adapted from P_InterceptVector
	//
	//==========================================================================

	bool GetIntersection(FCoverageVertex *v1, FCoverageVertex *v2, node_t *bsp, FCoverageVertex *v)
	{
		double frac;
		double num;
		double den;

		double v2x = (double)v1->x;
		double v2y = (double)v1->y;
		double v2dx = (double)(v2->x - v1->x);
		double v2dy = (double)(v2->y - v1->y);
		double v1x = (double)bsp->x;
		double v1y = (double)bsp->y;
		double v1dx = (double)bsp->dx;
		double v1dy = (double)bsp->dy;
			
		den = v1dy*v2dx - v1dx*v2dy;

		if (den == 0)
			return false;		// parallel
		
		num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
		frac = num / den;

		if (frac < 0. || frac > 1.) return false;

		v->x = xs_RoundToInt(v2x + frac * v2dx);
		v->y = xs_RoundToInt(v2y + frac * v2dy);
		return true;
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	double PartitionDistance(FCoverageVertex *vt, node_t *node)
	{	
		return fabs(double(-node->dy) * (vt->x - node->x) + double(node->dx) * (vt->y - node->y)) / (node->len * 65536.);
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	int PointOnSide(FCoverageVertex *vt, node_t *node)
	{	
		return R_PointOnSide(vt->x, vt->y, node);
	}

	//==========================================================================
	//
	// adapted from polyobject splitter
	//
	//==========================================================================

	void CollectNode(void *node, TArray<FCoverageVertex> &shape)
	{
		static TArray<FCoverageLine> lists[2];
		const double COVERAGE_EPSILON = 6.;	// same epsilon as the node builder

		if (!((size_t)node & 1))  // Keep going until found a subsector
		{
			node_t *bsp = (node_t *)node;

			int centerside = R_PointOnSide(center.x, center.y, bsp);

			lists[0].Clear();
			lists[1].Clear();
			for(unsigned i=0;i<shape.Size(); i++)
			{
				FCoverageVertex *v1 = &shape[i];
				FCoverageVertex *v2 = &shape[(i+1) % shape.Size()];
				FCoverageLine vl = {{*v1, *v2}};

				double dist_v1 = PartitionDistance(v1, bsp);
				double dist_v2 = PartitionDistance(v2, bsp);

				if(dist_v1 <= COVERAGE_EPSILON)
				{
					if (dist_v2 <= COVERAGE_EPSILON)
					{
						lists[centerside].Push(vl);
					}
					else
					{
						int side = PointOnSide(v2, bsp);
						lists[side].Push(vl);
					}
				}
				else if (dist_v2 <= COVERAGE_EPSILON)
				{
					int side = PointOnSide(v1, bsp);
					lists[side].Push(vl);
				}
				else 
				{
					int side1 = PointOnSide(v1, bsp);
					int side2 = PointOnSide(v2, bsp);

					if(side1 != side2)
					{
						// if the partition line crosses this seg, we must split it.

						FCoverageVertex vert;

						if (GetIntersection(v1, v2, bsp, &vert))
						{
							lists[0].Push(vl);
							lists[1].Push(vl);
							lists[side1].Last().v[1] = vert;
							lists[side2].Last().v[0] = vert;
						}
						else
						{
							// should never happen
							lists[side1].Push(vl);
						}
					}
					else 
					{
						// both points on the same side.
						lists[side1].Push(vl);
					}
				}
			}
			if (lists[1].Size() == 0)
			{
				CollectNode(bsp->children[0], shape);
			}
			else if (lists[0].Size() == 0)
			{
				CollectNode(bsp->children[1], shape);
			}
			else
			{
				// copy the static arrays into local ones
				TArray<FCoverageVertex> locallists[2];

				for(int l=0;l<2;l++)
				{
					for (unsigned i=0;i<lists[l].Size(); i++)
					{
						locallists[l].Push(lists[l][i].v[0]);
						unsigned i1= (i+1)%lists[l].Size();
						if (lists[l][i1].v[0] != lists[l][i].v[1])
						{
							locallists[l].Push(lists[l][i].v[1]);
						}
					}
				}

				CollectNode(bsp->children[0], locallists[0]);
				CollectNode(bsp->children[1], locallists[1]);
			}
		}
		else
		{
			// we reached a subsector so we can link the node with this subsector
			subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
			collect.Push(int(sub->Index()));
		}
	}
};

//==========================================================================
//
// Calculate portal coverage for a single subsector
//
//==========================================================================

void gl_BuildPortalCoverage(FPortalCoverage *coverage, subsector_t *subsector, const DVector2 &displacement)
{
	TArray<FCoverageVertex> shape;
	double centerx=0, centery=0;

	shape.Resize(subsector->numlines);
	for(unsigned i=0; i<subsector->numlines; i++)
	{
		centerx += (shape[i].x = FLOAT2FIXED(subsector->firstline[i].v1->fX() + displacement.X));
		centery += (shape[i].y = FLOAT2FIXED(subsector->firstline[i].v1->fY() + displacement.Y));
	}

	FCoverageBuilder build(subsector);
	build.center.x = xs_CRoundToInt(centerx / subsector->numlines);
	build.center.y = xs_CRoundToInt(centery / subsector->numlines);

	build.CollectNode(level.HeadNode(), shape);
	coverage->subsectors = new uint32_t[build.collect.Size()]; 
	coverage->sscount = build.collect.Size();
	memcpy(coverage->subsectors, &build.collect[0], build.collect.Size() * sizeof(uint32_t));
}

//==========================================================================
//
// portal initialization
//
//==========================================================================

static void CollectPortalSectors(FPortalMap &collection)
{
	for (auto &sec : level.sectors)
	{
		for (int j = 0; j < 2; j++)
		{
			int ptype = sec.GetPortalType(j);
			if (ptype== PORTS_STACKEDSECTORTHING || ptype == PORTS_PORTAL || ptype == PORTS_LINKEDPORTAL)	// only offset-displacing portal types
			{
				FPortalID id = { sec.GetPortalDisplacement(j) };

				FPortalSectors &sss = collection[id];
				FPortalSector ss = { &sec, j };
				sss.Push(ss);
			}
		}
	}
}

void gl_InitPortals()
{
	FPortalMap collection;

	if (level.nodes.Size() == 0) return;

	
	CollectPortalSectors(collection);
	glSectorPortals.Clear();

	FPortalMap::Iterator it(collection);
	FPortalMap::Pair *pair;
	int c = 0;
	int planeflags = 0;
	while (it.NextPair(pair))
	{
		for(unsigned i=0;i<pair->Value.Size(); i++)
		{
			if (pair->Value[i].mPlane == sector_t::floor) planeflags |= 1;
			else if (pair->Value[i].mPlane == sector_t::ceiling) planeflags |= 2;
		}
		for (int i=1;i<=2;i<<=1)
		{
			// add separate glSectorPortals for floor and ceiling.
			if (planeflags & i)
			{
				FPortal *portal = new FPortal;
				portal->mDisplacement = pair->Key.mDisplacement;
				portal->plane = (i==1? sector_t::floor : sector_t::ceiling);	/**/
				portal->glportal = NULL;
				glSectorPortals.Push(portal);
				for(unsigned j=0;j<pair->Value.Size(); j++)
				{
					sector_t *sec = pair->Value[j].mSub;
					int plane = pair->Value[j].mPlane;
					if (portal->plane == plane)
					{
						for(int k=0;k<sec->subsectorcount; k++)
						{
							subsector_t *sub = sec->subsectors[k];
							gl_BuildPortalCoverage(&sub->portalcoverage[plane], sub, pair->Key.mDisplacement);
						}
						sec->portals[plane] = portal;
					}
				}
			}
		}
	}

	// Now group the line glSectorPortals (each group must be a continuous set of colinear linedefs with no gaps)
	glLinePortals.Clear();
	linePortalToGL.Clear();
	TArray<int> tempindex;

	tempindex.Reserve(linePortals.Size());
	memset(&tempindex[0], -1, linePortals.Size() * sizeof(int));

	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		auto port = linePortals[i];
		bool gotsome;

		if (tempindex[i] == -1)
		{
			tempindex[i] = glLinePortals.Size();
			line_t *pSrcLine = linePortals[i].mOrigin;
			line_t *pLine = linePortals[i].mDestination;
			FGLLinePortal &glport = glLinePortals[glLinePortals.Reserve(1)];
			glport.lines.Push(&linePortals[i]);

			// We cannot do this grouping for non-linked glSectorPortals because they can be changed at run time.
			if (linePortals[i].mType == PORTT_LINKED && pLine != nullptr)
			{
				glport.v1 = pLine->v1;
				glport.v2 = pLine->v2;
				do
				{
					// now collect all other colinear lines connected to this one. We run this loop as long as it still finds a match
					gotsome = false;
					for (unsigned j = 0; j < linePortals.Size(); j++)
					{
						if (tempindex[j] == -1)
						{
							line_t *pSrcLine2 = linePortals[j].mOrigin;
							line_t *pLine2 = linePortals[j].mDestination;
							// angular precision is intentionally reduced to 32 bit BAM to account for precision problems (otherwise many not perfectly horizontal or vertical glSectorPortals aren't found here.)
							unsigned srcang = pSrcLine->Delta().Angle().BAMs();
							unsigned dstang = pLine->Delta().Angle().BAMs();
							if ((pSrcLine->v2 == pSrcLine2->v1 && pLine->v1 == pLine2->v2) ||
								(pSrcLine->v1 == pSrcLine2->v2 && pLine->v2 == pLine2->v1))
							{
								// The line connects, now check the translation
								unsigned srcang2 = pSrcLine2->Delta().Angle().BAMs();
								unsigned dstang2 = pLine2->Delta().Angle().BAMs();
								if (srcang == srcang2 && dstang == dstang2)
								{
									// The lines connect and  both source and destination are colinear, so this is a match
									gotsome = true;
									tempindex[j] = tempindex[i];
									if (pLine->v1 == pLine2->v2) glport.v1 = pLine2->v1;
									else glport.v2 = pLine2->v2;
									glport.lines.Push(&linePortals[j]);
								}
							}
						}
					}
				} while (gotsome);
			}
		}
	}
	linePortalToGL.Resize(linePortals.Size());
	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		linePortalToGL[i] = &glLinePortals[tempindex[i]];
		/*
		Printf("portal at line %d translates to GL portal %d, range = %f,%f to %f,%f\n",
			int(linePortals[i].mOrigin - lines), tempindex[i], linePortalToGL[i]->v1->fixX() / 65536., linePortalToGL[i]->v1->fixY() / 65536., linePortalToGL[i]->v2->fixX() / 65536., linePortalToGL[i]->v2->fixY() / 65536.);
		*/
	}
}

CCMD(dumpportals)
{
	for(unsigned i=0;i<glSectorPortals.Size(); i++)
	{
		double xdisp = glSectorPortals[i]->mDisplacement.X;
		double ydisp = glSectorPortals[i]->mDisplacement.Y;
		Printf(PRINT_LOG, "Portal #%d, %s, displacement = (%f,%f)\n", i, glSectorPortals[i]->plane==0? "floor":"ceiling",
			xdisp, ydisp);
		Printf(PRINT_LOG, "Coverage:\n");
		for(auto &sub : level.subsectors)
		{
			FPortal *port = sub.render_sector->GetGLPortal(glSectorPortals[i]->plane);
			if (port == glSectorPortals[i])
			{
				Printf(PRINT_LOG, "\tSubsector %d (%d):\n\t\t", sub.Index(), sub.render_sector->sectornum);
				for(unsigned k = 0;k< sub.numlines; k++)
				{
					Printf(PRINT_LOG, "(%.3f,%.3f), ",	sub.firstline[k].v1->fX() + xdisp, sub.firstline[k].v1->fY() + ydisp);
				}
				Printf(PRINT_LOG, "\n\t\tCovered by subsectors:\n");
				FPortalCoverage *cov = &sub.portalcoverage[glSectorPortals[i]->plane];
				for(int l = 0;l< cov->sscount; l++)
				{
					subsector_t *csub = &level.subsectors[cov->subsectors[l]];
					Printf(PRINT_LOG, "\t\t\t%5d (%4d): ", cov->subsectors[l], csub->render_sector->sectornum);
					for(unsigned m = 0;m< csub->numlines; m++)
					{
						Printf(PRINT_LOG, "(%.3f,%.3f), ",	csub->firstline[m].v1->fX(), csub->firstline[m].v1->fY());
					}
					Printf(PRINT_LOG, "\n");
				}
			}
		}
	}
}
