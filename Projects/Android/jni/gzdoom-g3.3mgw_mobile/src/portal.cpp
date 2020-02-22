//
//-----------------------------------------------------------------------------
//
// Copyright 2016 ZZYZX, Christoph Oelckers, et. al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:
// Everything that has to do with portals
// (both of the line and sector variety)
//
//-----------------------------------------------------------------------------

#include "p_local.h"
#include "p_blockmap.h"
#include "p_lnspec.h"
#include "c_cvars.h"
#include "m_bbox.h"
#include "p_tags.h"
#include "v_text.h"
#include "a_sharedglobal.h"
#include "i_system.h"
#include "c_dispatch.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "p_checkposition.h"
#include "math/cmath.h"
#include "g_levellocals.h"
#include "vm.h"

// simulation recurions maximum
CVAR(Int, sv_portal_recursions, 4, CVAR_ARCHIVE|CVAR_SERVERINFO)

FDisplacementTable Displacements;
FPortalBlockmap PortalBlockmap;

TArray<FLinePortal> linePortals;
TArray<FLinePortal*> linkedPortals;	// only the linked portals, this is used to speed up looking for them in P_CollectConnectedGroups.


DEFINE_FIELD(FSectorPortal, mType);
DEFINE_FIELD(FSectorPortal, mFlags);
DEFINE_FIELD(FSectorPortal, mPartner);
DEFINE_FIELD(FSectorPortal, mPlane);
DEFINE_FIELD(FSectorPortal, mOrigin);
DEFINE_FIELD(FSectorPortal, mDestination);
DEFINE_FIELD(FSectorPortal, mDisplacement);
DEFINE_FIELD(FSectorPortal, mPlaneZ);
DEFINE_FIELD(FSectorPortal, mSkybox);

//============================================================================
//
// This is used to mark processed portals for some collection functions.
//
//============================================================================

struct FPortalBits
{
	TArray<uint32_t> data;

	void setSize(int num)
	{
		data.Resize((num + 31) / 32);
		clear();
	}

	void clear()
	{
		memset(&data[0], 0, data.Size()*sizeof(uint32_t));
	}

	void setBit(int group)
	{
		data[group >> 5] |= (1 << (group & 31));
	}

	int getBit(int group)
	{
		return data[group >> 5] & (1 << (group & 31));
	}
};

//============================================================================
//
// BuildBlockmap
//
//============================================================================

static void BuildBlockmap()
{
	auto bmapwidth = level.blockmap.bmapwidth;
	auto bmapheight = level.blockmap.bmapheight;

	PortalBlockmap.Clear();
	PortalBlockmap.Create(bmapwidth, bmapheight);
	for (int y = 0; y < bmapheight; y++)
	{
		for (int x = 0; x < bmapwidth; x++)
		{
			int *list = level.blockmap.GetLines(x, y);
			FPortalBlock &block = PortalBlockmap(x, y);

			while (*list != -1)
			{
				line_t *ld = &level.lines[*list++];
				FLinePortal *port = ld->getPortal();
				if (port && port->mType != PORTT_VISUAL)
				{
					PortalBlockmap.containsLines = true;
					block.portallines.Push(ld);
					block.neighborContainsLines = true;
					if (ld->getPortal()->mType == PORTT_LINKED) block.containsLinkedPortals = true;
					if (x > 0) PortalBlockmap(x - 1, y).neighborContainsLines = true;
					if (y > 0) PortalBlockmap(x, y - 1).neighborContainsLines = true;
					if (x < PortalBlockmap.dx - 1) PortalBlockmap(x + 1, y).neighborContainsLines = true;
					if (y < PortalBlockmap.dy - 1) PortalBlockmap(x, y + 1).neighborContainsLines = true;
				}
				else
				{

					bool yes = ld->frontsector->PortalIsLinked(sector_t::ceiling) || ld->frontsector->PortalIsLinked(sector_t::floor);
					if (ld->backsector)
					{
						yes |= ld->backsector->PortalIsLinked(sector_t::ceiling) || ld->backsector->PortalIsLinked(sector_t::floor);
					}
					block.containsLinkedPortals |= yes;
					PortalBlockmap.hasLinkedSectorPortals |= yes;

				}
			}
		}
	}
}

//===========================================================================
//
// FLinePortalTraverse :: AddLineIntercepts.
//
// Similar to AddLineIntercepts but checks the portal blockmap for line-to-line portals
//
//===========================================================================

void FLinePortalTraverse::AddLineIntercepts(int bx, int by)
{
	if (by < 0 || by >= PortalBlockmap.dy || bx < 0 || bx >= PortalBlockmap.dx) return;

	FPortalBlock &block = PortalBlockmap(bx, by);

	for (unsigned i = 0; i<block.portallines.Size(); i++)
	{
		line_t *ld = block.portallines[i];
		double frac;
		divline_t dl;

		if (ld->validcount == validcount) continue;	// already processed

		if (P_PointOnDivlineSide (ld->v1->fPos(), &trace) ==
			P_PointOnDivlineSide (ld->v2->fPos(), &trace))
		{
			continue;		// line isn't crossed
		}
		P_MakeDivline (ld, &dl);
		if (P_PointOnDivlineSide(trace.x, trace.y, &dl) != 0 ||
			P_PointOnDivlineSide(trace.x + trace.dx, trace.y + trace.dy, &dl) != 1)
		{
			continue;		// line isn't crossed from the front side
		}

		// hit the line
		P_MakeDivline(ld, &dl);
		frac = P_InterceptVector(&trace, &dl);
		if (frac < 0 || frac > 1.) continue;	// behind source

		intercept_t newintercept;

		newintercept.frac = frac;
		newintercept.isaline = true;
		newintercept.done = false;
		newintercept.d.line = ld;
		intercepts.Push(newintercept);
	}
}

//============================================================================
//
// finds the destination for a line portal for spawning
//
//============================================================================

static line_t *FindDestination(line_t *src, int tag)
{
	if (tag)
	{
		int lineno = -1;
		FLineIdIterator it(tag);

		while ((lineno = it.Next()) >= 0)
		{
			if (&level.lines[lineno] != src)
			{
				return &level.lines[lineno];
			}
		}
	}
	return nullptr;
}


//============================================================================
//
//
//
//============================================================================

static void SetRotation(FLinePortal *port)
{
	if (port != nullptr && port->mDestination != nullptr)
	{
		if (port->mType != PORTT_LINKED)
		{
			line_t *dst = port->mDestination;
			line_t *line = port->mOrigin;
			DAngle angle = dst->Delta().Angle() - line->Delta().Angle() + 180.;
			port->mSinRot = sindeg(angle.Degrees);	// Here precision matters so use the slower but more precise versions.
			port->mCosRot = cosdeg(angle.Degrees);
			port->mAngleDiff = angle;
			if ((line->sidedef[0]->Flags & WALLF_POLYOBJ) || (dst->sidedef[0]->Flags & WALLF_POLYOBJ))
			{
				port->mFlags |= PORTF_POLYOBJ;
			}
			else
			{
				port->mFlags &= ~PORTF_POLYOBJ;
			}
		}
		else
		{
			// Linked portals have no angular difference.
			port->mSinRot = 0.;
			port->mCosRot = 1.;
			port->mAngleDiff = 0.;
		}
	}
}

//============================================================================
//
// Spawns a single line portal
//
//============================================================================

void P_SpawnLinePortal(line_t* line)
{
	// portal destination is special argument #0
	line_t* dst = nullptr;

	if (line->args[2] >= PORTT_VISUAL && line->args[2] <= PORTT_LINKED)
	{
		dst = FindDestination(line, line->args[0]);

		line->portalindex = linePortals.Reserve(1);
		FLinePortal *port = &linePortals.Last();

		memset(port, 0, sizeof(FLinePortal));
		port->mOrigin = line;
		port->mDestination = dst;
		port->mType = uint8_t(line->args[2]);	// range check is done above.

		if (port->mType == PORTT_LINKED)
		{
			// Linked portals have no z-offset ever.
			port->mAlign = PORG_ABSOLUTE;
		}
		else
		{
			port->mAlign = uint8_t(line->args[3] >= PORG_ABSOLUTE && line->args[3] <= PORG_CEILING ? line->args[3] : PORG_ABSOLUTE);
			if (port->mType == PORTT_INTERACTIVE && port->mAlign != PORG_ABSOLUTE)
			{
				// Due to the way z is often handled, these pose a major issue for parts of the code that needs to transparently handle interactive portals.
				Printf(TEXTCOLOR_RED "Warning: z-offsetting not allowed for interactive portals. Changing line %d to teleport-portal!\n", line->Index());
				port->mType = PORTT_TELEPORT;
			}
		}
		port->mDefFlags = port->mType == PORTT_VISUAL   ? PORTF_VISIBLE :
		                  port->mType == PORTT_TELEPORT ? PORTF_TYPETELEPORT :
		                                                  PORTF_TYPEINTERACTIVE;
	}
	else if (line->args[2] == PORTT_LINKEDEE && line->args[0] == 0)
	{
		// EE-style portals require that the first line ID is identical and the first arg of the two linked linedefs are 0 and 1 respectively.

		int mytag = tagManager.GetFirstLineID(line);

		for (auto &ln : level.lines)
		{
			if (tagManager.GetFirstLineID(&ln) == mytag && ln.args[0] == 1 && ln.special == Line_SetPortal)
			{
				line->portalindex = linePortals.Reserve(1);
				FLinePortal *port = &linePortals.Last();

				memset(port, 0, sizeof(FLinePortal));
				port->mOrigin = line;
				port->mDestination = &ln;
				port->mType = PORTT_LINKED;
				port->mAlign = PORG_ABSOLUTE;
				port->mDefFlags = PORTF_TYPEINTERACTIVE;

				// we need to create the backlink here, too.
				ln.portalindex = linePortals.Reserve(1);
				port = &linePortals.Last();

				memset(port, 0, sizeof(FLinePortal));
				port->mOrigin = &ln;
				port->mDestination = line;
				port->mType = PORTT_LINKED;
				port->mAlign = PORG_ABSOLUTE;
				port->mDefFlags = PORTF_TYPEINTERACTIVE;
			}
		}
	}
	else
	{
		// undefined type
		return;
	}
}

//============================================================================
//
// Update a line portal's state after all have been spawned
//
//============================================================================

void P_UpdatePortal(FLinePortal *port)
{
	if (port->mType != PORTT_VISUAL && port->mOrigin->backsector == nullptr && !(port->mOrigin->sidedef[0]->Flags & WALLF_POLYOBJ))
	{
		Printf(TEXTCOLOR_RED "Warning: Traversable portals must have a back-sector and empty space behind them (or be on a polyobject)! Changing line %d to visual-portal!\n", port->mOrigin->Index());
		port->mType = PORTT_VISUAL;
		port->mDefFlags &= ~(PORTF_PASSABLE | PORTF_SOUNDTRAVERSE | PORTF_INTERACTIVE);
	}

	if (port->mDestination == nullptr)
	{
		// Portal has no destination: switch it off
		port->mFlags = 0;
	}
	else if (port->mDestination->getPortalDestination() != port->mOrigin)
	{
		//portal doesn't link back. This will be a simple teleporter portal.
		port->mFlags = port->mDefFlags & ~PORTF_INTERACTIVE;
		if (port->mType == PORTT_LINKED)
		{
			// this is illegal. Demote the type to TELEPORT
			port->mType = PORTT_TELEPORT;
			port->mDefFlags &= ~PORTF_INTERACTIVE;
			Printf(TEXTCOLOR_RED "Warning: linked portal did not have matching reverse portal. Changing line %d to teleport-portal!\n", port->mOrigin->Index());
		}
	}
	else
	{
		port->mFlags = port->mDefFlags;
		if (port->mType == PORTT_LINKED)
		{
			if (linePortals[port->mDestination->portalindex].mType != PORTT_LINKED)
			{
				port->mType = PORTT_INTERACTIVE;	// linked portals must be two-way.
			}
			else
			{
				port->mDisplacement = port->mDestination->v2->fPos() - port->mOrigin->v1->fPos();
			}
		}
 	}

	// Cache the angle between the two linedefs, for rotating.
	SetRotation(port);
}

//============================================================================
//
// Collect a separate list of linked portals so that these can be
// processed faster without the simpler types interfering.
//
//============================================================================

void P_CollectLinkedPortals()
{
	linkedPortals.Clear();
	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		FLinePortal * port = &linePortals[i];
		if (port->mType == PORTT_LINKED)
		{
			linkedPortals.Push(port);
		}
	}
}

//============================================================================
//
// Post-process all line portals
//
//============================================================================

void P_FinalizePortals()
{
	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		FLinePortal * port = &linePortals[i];
		P_UpdatePortal(port);
	}
	P_CollectLinkedPortals();
	BuildBlockmap();
	P_CreateLinkedPortals();
}

//============================================================================
//
// Change the destination of a portal
//
//============================================================================

static bool ChangePortalLine(line_t *line, int destid)
{
	if (line->portalindex >= linePortals.Size()) return false;
	FLinePortal *port = &linePortals[line->portalindex];
	if (port->mType == PORTT_LINKED) return false;	// linked portals cannot be changed.
	if (destid == 0) port->mDestination = nullptr;
	port->mDestination = FindDestination(line, destid);
	if (port->mDestination == nullptr)
	{
		port->mFlags = 0;
	}
	else if (port->mType == PORTT_INTERACTIVE)
	{
		FLinePortal *portd = port->mDestination->portalindex < linePortals.Size()? &linePortals[port->mDestination->portalindex] : nullptr;
		if (portd != nullptr && portd->mType == PORTT_INTERACTIVE && portd->mDestination == line)
		{
			// this is a 2-way interactive portal
			port->mFlags = port->mDefFlags | PORTF_INTERACTIVE;
			portd->mFlags = portd->mDefFlags | PORTF_INTERACTIVE;
		}
		else
		{
			port->mFlags = port->mDefFlags;
		}
		SetRotation(portd);
	}
	else
	{
		port->mFlags = port->mDefFlags;
	}
	SetRotation(port);
	return true;
}


//============================================================================
//
// Change the destination of a group of portals
//
//============================================================================

bool P_ChangePortal(line_t *ln, int thisid, int destid)
{
	int lineno;

	if (thisid == 0) return ChangePortalLine(ln, destid);
	FLineIdIterator it(thisid);
	bool res = false;
	while ((lineno = it.Next()) >= 0)
	{
		res |= ChangePortalLine(&level.lines[lineno], destid);
	}
	return res;
}

//============================================================================
//
// clears all portal data for a new level start
//
//============================================================================

void P_ClearPortals()
{
	Displacements.Create(1);
	linePortals.Clear();
	linkedPortals.Clear();
	level.sectorPortals.Resize(2);
	// The first entry must always be the default skybox. This is what every sector gets by default.
	memset(&level.sectorPortals[0], 0, sizeof(level.sectorPortals[0]));
	level.sectorPortals[0].mType = PORTS_SKYVIEWPOINT;
	level.sectorPortals[0].mFlags = PORTSF_SKYFLATONLY;
	// The second entry will be the default sky. This is for forcing a regular sky through the skybox picker
	memset(&level.sectorPortals[1], 0, sizeof(level.sectorPortals[0]));
	level.sectorPortals[1].mType = PORTS_SKYVIEWPOINT;
	level.sectorPortals[1].mFlags = PORTSF_SKYFLATONLY;
}

//============================================================================
//
// check if this line is between portal and the viewer. clip away if it is.
//
//============================================================================

inline int P_GetLineSide(const DVector2 &pos, const line_t *line)
{
	double v = (pos.Y - line->v1->fY()) * line->Delta().X + (line->v1->fX() - pos.X) * line->Delta().Y;
	return v < -1. / 65536. ? -1 : v > 1. / 65536 ? 1 : 0;
}

bool P_ClipLineToPortal(line_t* line, line_t* portal, DVector2 view, bool partial, bool samebehind)
{
	int behind1 = P_GetLineSide(line->v1->fPos(), portal);
	int behind2 = P_GetLineSide(line->v2->fPos(), portal);

	if (behind1 == 0 && behind2 == 0)
	{
		// The line is parallel to the portal and cannot possibly be visible.
		return true;
	}
	// If one point lies on the same straight line than the portal, the other vertex will determine sidedness alone.
	else if (behind2 == 0) behind2 = behind1;
	else if (behind1 == 0) behind1 = behind2;

	if (behind1 > 0 && behind2 > 0)
	{
		// The line is behind the portal, i.e. between viewer and portal line, and must be rejected
		return true;
	}
	else if (behind1 < 0 && behind2 < 0)
	{
		// The line is in front of the portal, i.e. the portal is between viewer and line. This line must not be rejected
		return false;
	}
	else
	{
		// The line intersects with the portal straight, so we need to do another check to see how both ends of the portal lie in relation to the viewer.
		int viewside = P_GetLineSide(view, line); 
		int p1side = P_GetLineSide(portal->v1->fPos(), line);
		int p2side = P_GetLineSide(portal->v2->fPos(), line);
		// Do the same handling of points on the portal straight as above.
		if (p1side == 0) p1side = p2side;
		else if (p2side == 0) p2side = p1side;
		// If the portal is on the other side of the line than the viewpoint, there is no possibility to see this line inside the portal.
		return (p1side == p2side && viewside != p1side);
	}
}

//============================================================================
//
// Translates a coordinate by a portal's displacement
//
//============================================================================

void P_TranslatePortalXY(line_t* src, double& x, double& y)
{
	if (!src) return;
	FLinePortal *port = src->getPortal();
	if (!port) return;
	if (port->mFlags & PORTF_POLYOBJ) SetRotation(port);	// update the angle for polyportals.

	// offsets from line
	double nposx = x - src->v1->fX();
	double nposy = y - src->v1->fY();

	// Rotate position along normal to match exit linedef
	double tx = nposx * port->mCosRot - nposy * port->mSinRot;
	double ty = nposy * port->mCosRot + nposx * port->mSinRot;

	tx += port->mDestination->v2->fX();
	ty += port->mDestination->v2->fY();

	x = tx;
	y = ty;
}

//============================================================================
//
// Translates a velocity vector by a portal's displacement
//
//============================================================================

void P_TranslatePortalVXVY(line_t* src, double &velx, double &vely)
{
	if (!src) return;
	FLinePortal *port = src->getPortal();
	if (!port) return;
	if (port->mFlags & PORTF_POLYOBJ) SetRotation(port);	// update the angle for polyportals.

	double orig_velx = velx;
	double orig_vely = vely;
	velx = orig_velx * port->mCosRot - orig_vely * port->mSinRot;
	vely = orig_vely * port->mCosRot + orig_velx * port->mSinRot;
}

//============================================================================
//
// Translates an angle by a portal's displacement
//
//============================================================================

void P_TranslatePortalAngle(line_t* src, DAngle& angle)
{
	if (!src) return;
	FLinePortal *port = src->getPortal();
	if (!port) return;
	if (port->mFlags & PORTF_POLYOBJ) SetRotation(port);	// update the angle for polyportals.
	angle = (angle + port->mAngleDiff).Normalized360();
}

//============================================================================
//
// Translates a z-coordinate by a portal's displacement
//
//============================================================================

void P_TranslatePortalZ(line_t* src, double& z)
{
	// args[2] = 0 - no adjustment
	// args[2] = 1 - adjust by floor difference
	// args[2] = 2 - adjust by ceiling difference

	// This cannot be precalculated because heights may change.
	line_t *dst = src->getPortalDestination();
	switch (src->getPortalAlignment())
	{
	case PORG_FLOOR:
		z = z - src->frontsector->floorplane.ZatPoint(src->v1) + dst->frontsector->floorplane.ZatPoint(dst->v2);
		return;

	case PORG_CEILING:
		z = z - src->frontsector->ceilingplane.ZatPoint(src->v1) + dst->frontsector->ceilingplane.ZatPoint(dst->v2);
		return;

	default:
		return;
	}
}

//============================================================================
//
// P_GetSkyboxPortal
//
// Gets a portal for a SkyViewpoint
// If none exists yet, it will create a new one.
//
//============================================================================

unsigned P_GetSkyboxPortal(AActor *actor)
{
	if (actor == nullptr) return 1;	// this means a regular sky.
	for (unsigned i = 0;i<level.sectorPortals.Size();i++)
	{
		if (level.sectorPortals[i].mSkybox == actor) return i;
	}
	unsigned i = level.sectorPortals.Reserve(1);
	memset(&level.sectorPortals[i], 0, sizeof(level.sectorPortals[i]));
	level.sectorPortals[i].mType = PORTS_SKYVIEWPOINT;
	level.sectorPortals[i].mFlags = actor->GetClass()->IsDescendantOf("SkyCamCompat") ? 0 : PORTSF_SKYFLATONLY;
	level.sectorPortals[i].mSkybox = actor;
	level.sectorPortals[i].mDestination = actor->Sector;
	return i;
}

DEFINE_ACTION_FUNCTION(FSectorPortal, GetSkyboxPortal)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(actor, AActor);
	ACTION_RETURN_INT(P_GetSkyboxPortal(actor));
}
//============================================================================
//
// P_GetPortal
//
// Creates a portal struct for a linedef-based portal
//
//============================================================================

unsigned P_GetPortal(int type, int plane, sector_t *from, sector_t *to, const DVector2 &displacement)
{
	unsigned i = level.sectorPortals.Reserve(1);
	memset(&level.sectorPortals[i], 0, sizeof(level.sectorPortals[i]));
	level.sectorPortals[i].mType = type;
	level.sectorPortals[i].mPlane = plane;
	level.sectorPortals[i].mOrigin = from;
	level.sectorPortals[i].mDestination = to;
	level.sectorPortals[i].mDisplacement = displacement;
	level.sectorPortals[i].mPlaneZ = type == PORTS_LINKEDPORTAL? from->GetPlaneTexZ(plane) : FLT_MAX;
	return i;
}

//============================================================================
//
// P_GetStackPortal
//
// Creates a portal for a stacked sector thing
//
//============================================================================

unsigned P_GetStackPortal(AActor *point, int plane)
{
	unsigned i = level.sectorPortals.Reserve(1);
	memset(&level.sectorPortals[i], 0, sizeof(level.sectorPortals[i]));
	level.sectorPortals[i].mType = PORTS_STACKEDSECTORTHING;
	level.sectorPortals[i].mPlane = plane;
	level.sectorPortals[i].mOrigin = point->target->Sector;
	level.sectorPortals[i].mDestination = point->Sector;
	level.sectorPortals[i].mPlaneZ = FLT_MAX;
	level.sectorPortals[i].mSkybox = point;
	return i;
}


//============================================================================
//
// P_GetOffsetPosition
//
// Offsets a given coordinate if the trace from the origin crosses an 
// interactive line-to-line portal.
//
//============================================================================

DVector2 P_GetOffsetPosition(double x, double y, double dx, double dy)
{
	DVector2 dest(x + dx, y + dy);
	if (PortalBlockmap.containsLines)
	{
		double actx = x, acty = y;
		// Try some easily discoverable early-out first. If we know that the trace cannot possibly find a portal, this saves us from calling the traverser completely for vast parts of the map.
		if (dx < 128 && dy < 128)
		{
			int blockx = level.blockmap.GetBlockX(actx);
			int blocky = level.blockmap.GetBlockY(acty);
			if (blockx < 0 || blocky < 0 || blockx >= PortalBlockmap.dx || blocky >= PortalBlockmap.dy || !PortalBlockmap(blockx, blocky).neighborContainsLines) return dest;
		}

		bool repeat;
		do
		{
			FLinePortalTraverse it;
			it.init(actx, acty, dx, dy, PT_ADDLINES|PT_DELTA);
			intercept_t *in;

			repeat = false;
			while ((in = it.Next()))
			{
				// hit a portal line.
				line_t *line = in->d.line;
				FLinePortal *port = line->getPortal();

				// Teleport portals are intentionally ignored since skipping this stuff is their entire reason for existence.
				if (port->mFlags & PORTF_INTERACTIVE)
				{
					DVector2 hit = it.InterceptPoint(in);

					if (port->mType == PORTT_LINKED)
					{
						// optimized handling for linked portals where we only need to add an offset.
						hit += port->mDisplacement;
						dest += port->mDisplacement;
					}
					else
					{
						// interactive ones are more complex because the vector may be rotated.
						// Note: There is no z-translation here, there's just too much code in the engine that wouldn't be able to handle interactive portals with a height difference.
						P_TranslatePortalXY(line, hit.X, hit.Y);
						P_TranslatePortalXY(line, dest.X, dest.Y);
					}
					// update the fields, end this trace and restart from the new position
					dx = dest.X - hit.X;
					dy = dest.Y - hit.Y;
					actx = hit.X;
					acty = hit.Y;
					repeat = true;
				}

				break;
			}
		} while (repeat);
	}
	return dest;
}

static void GetOffsetPosition(double x, double y, double dx, double dy, DVector2 *result)
{
	*result = P_GetOffsetPosition(x, y, dx, dy);
}

//============================================================================
//
// CollectSectors
//
// Collects all sectors that are connected to any sector belonging to a portal
// because they all will need the same displacement values
//
//============================================================================

static bool CollectSectors(int groupid, sector_t *origin)
{
	if (origin->PortalGroup != 0) return false;	// already processed
	origin->PortalGroup = groupid;

	TArray<sector_t *> list(16);
	list.Push(origin);

	for (unsigned i = 0; i < list.Size(); i++)
	{
		sector_t *sec = list[i];

		for (auto line : sec->Lines)
		{
			sector_t *other = line->frontsector == sec ? line->backsector : line->frontsector;
			if (other != nullptr && other != sec && other->PortalGroup != groupid)
			{
				other->PortalGroup = groupid;
				list.Push(other);
			}
		}
	}
	return true;
}


//============================================================================
//
// AddDisplacementForPortal
//
// Adds the displacement for one portal to the displacement array
// (one version for sector to sector plane, one for line to line portals)
//
// Note: Despite the similarities to Eternity's equivalent this is
// original code!
//
//============================================================================

static void AddDisplacementForPortal(FSectorPortal *portal)
{
	int thisgroup = portal->mOrigin->PortalGroup;
	int othergroup = portal->mDestination->PortalGroup;
	if (thisgroup == othergroup)
	{
		Printf("Portal between sectors %d and %d has both sides in same group and will be disabled\n", portal->mOrigin->sectornum, portal->mDestination->sectornum);
		portal->mType = PORTS_PORTAL;
		return;
	}
	if (thisgroup <= 0 || thisgroup >= Displacements.size || othergroup <= 0 || othergroup >= Displacements.size)
	{
		Printf("Portal between sectors %d and %d has invalid group and will be disabled\n", portal->mOrigin->sectornum, portal->mDestination->sectornum);
		portal->mType = PORTS_PORTAL;
		return;
	}

	FDisplacement & disp = Displacements(thisgroup, othergroup);
	if (!disp.isSet)
	{
		disp.pos = portal->mDisplacement;
		disp.isSet = true;
	}
	else
	{
		if (disp.pos != portal->mDisplacement)
		{
			Printf("Portal between sectors %d and %d has displacement mismatch and will be disabled\n", portal->mOrigin->sectornum, portal->mDestination->sectornum);
			portal->mType = PORTS_PORTAL;
			return;
		}
	}
}


static void AddDisplacementForPortal(FLinePortal *portal)
{
	int thisgroup = portal->mOrigin->frontsector->PortalGroup;
	int othergroup = portal->mDestination->frontsector->PortalGroup;
	if (thisgroup == othergroup)
	{
		Printf("Portal between lines %d and %d has both sides in same group\n", portal->mOrigin->Index(), portal->mDestination->Index());
		portal->mType = linePortals[portal->mDestination->portalindex].mType = PORTT_TELEPORT;
		return;
	}
	if (thisgroup <= 0 || thisgroup >= Displacements.size || othergroup <= 0 || othergroup >= Displacements.size)
	{
		Printf("Portal between lines %d and %d has invalid group\n", portal->mOrigin->Index(), portal->mDestination->Index());
		portal->mType = linePortals[portal->mDestination->portalindex].mType = PORTT_TELEPORT;
		return;
	}

	FDisplacement & disp = Displacements(thisgroup, othergroup);
	if (!disp.isSet)
	{
		disp.pos = portal->mDisplacement;
		disp.isSet = true;
	}
	else
	{
		if (disp.pos != portal->mDisplacement)
		{
			Printf("Portal between lines %d and %d has displacement mismatch\n", portal->mOrigin->Index(), portal->mDestination->Index());
			portal->mType = linePortals[portal->mDestination->portalindex].mType = PORTT_TELEPORT;
			return;
		}
	}
}

//============================================================================
//
// ConnectGroups
//
// Do the indirect connections. This loop will run until it cannot find any new connections
//
//============================================================================

static bool ConnectGroups()
{
	// Now 
	uint8_t indirect = 1;
	bool bogus = false;
	bool changed;
	do
	{
		changed = false;
		for (int x = 1; x < Displacements.size; x++)
		{
			for (int y = 1; y < Displacements.size; y++)
			{
				FDisplacement &dispxy = Displacements(x, y);
				if (dispxy.isSet)
				{
					for (int z = 1; z < Displacements.size; z++)
					{
						FDisplacement &dispyz = Displacements(y, z);
						if (dispyz.isSet)
						{
							FDisplacement &dispxz = Displacements(x, z);
							if (dispxz.isSet)
							{
								if (dispxy.pos.X + dispyz.pos.X != dispxz.pos.X || dispxy.pos.Y + dispyz.pos.Y != dispxz.pos.Y)
								{
									bogus = true;
								}
							}
							else
							{
								dispxz.pos = dispxy.pos + dispyz.pos;
								dispxz.isSet = true;
								dispxz.indirect = indirect;
								changed = true;
							}
						}
					}
				}
			}
		}
		indirect++;
	} while (changed);
	return bogus;
}


//============================================================================
//
// P_CreateLinkedPortals
//
// Creates the data structures needed for linked portals
// Removes portals from sloped sectors (as they cannot work on them)
// Group all sectors connected to one side of the portal
// Caclculate displacements between all created groups.
//
// Portals with the same offset but different anchors will not be merged.
//
//============================================================================

void P_CreateLinkedPortals()
{
	TArray<FSectorPortal *> orgs;
	int id = 1;
	bool bogus = false;

	for(auto &s : level.sectorPortals)
	{
		if (s.mType == PORTS_LINKEDPORTAL)
		{
			orgs.Push(&s);
		}
	}
	id = 1;
	if (orgs.Size() != 0)
	{
		for (auto &sec : level.sectors)
		{
			for (int j = 0; j < 2; j++)
			{
				if (sec.GetPortalType(j) == PORTS_LINKEDPORTAL)
				{
					secplane_t &plane = j == 0 ? sec.floorplane : sec.ceilingplane;
					if (plane.isSlope())
					{
						// The engine cannot deal with portals on a sloped plane.
						sec.ClearPortal(j);
						Printf("Portal on %s of sector %d is sloped and will be disabled\n", j == 0 ? "floor" : "ceiling", sec.sectornum);
					}
				}
			}
		}

		// Group all sectors, starting at each portal origin.
		for (unsigned i = 0; i < orgs.Size(); i++)
		{
			if (CollectSectors(id, orgs[i]->mOrigin)) id++;
			if (CollectSectors(id, orgs[i]->mDestination)) id++;
		}
	}
	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		if (linePortals[i].mType == PORTT_LINKED)
		{
			if (linePortals[i].mDestination == nullptr)
			{
				Printf("Linked portal on line %d is unconnected and will be disabled\n", linePortals[i].mOrigin->Index());
				linePortals[i].mOrigin->portalindex = UINT_MAX;
				linePortals[i].mType = PORTT_VISUAL;
			}
			else
			{
				if (CollectSectors(id, linePortals[i].mOrigin->frontsector)) id++;
				if (CollectSectors(id, linePortals[i].mDestination->frontsector)) id++;
			}
		}
	}

	Displacements.Create(id);
	// Check for leftover sectors that connect to a portal
	for (auto &sec : level.sectors)
	{
		for (int j = 0; j < 2; j++)
		{
			if (sec.GetPortalType(j) == PORTS_LINKEDPORTAL && sec.PortalGroup == 0)
			{
				auto p = sec.GetPortal(j);
				CollectSectors(p->mOrigin->PortalGroup, &sec);
			}
		}
	}
	for (unsigned i = 0; i < orgs.Size(); i++)
	{
		AddDisplacementForPortal(orgs[i]);
	}
	for (unsigned i = 0; i < linePortals.Size(); i++)
	{
		if (linePortals[i].mType == PORTT_LINKED)
		{
			AddDisplacementForPortal(&linePortals[i]);
		}
	}

	for (int x = 1; x < Displacements.size; x++)
	{
		for (int y = x + 1; y < Displacements.size; y++)
		{
			FDisplacement &dispxy = Displacements(x, y);
			FDisplacement &dispyx = Displacements(y, x);
			if (dispxy.isSet && dispyx.isSet &&
				(dispxy.pos.X != -dispyx.pos.X || dispxy.pos.Y != -dispyx.pos.Y))
			{
				int sec1 = -1, sec2 = -1;
				for (unsigned i = 0; i < level.sectors.Size() && (sec1 == -1 || sec2 == -1); i++)
				{
					if (sec1 == -1 && level.sectors[i].PortalGroup == x)  sec1 = i;
					if (sec2 == -1 && level.sectors[i].PortalGroup == y)  sec2 = i;
				}
				Printf("Link offset mismatch between sectors %d and %d\n", sec1, sec2);
				bogus = true;
			}
			// mark everything that connects to a one-sided line
			for (auto &line : level.lines)
			{
				if (line.backsector == nullptr && line.frontsector->PortalGroup == 0)
				{
					CollectSectors(-1, line.frontsector);
				}
			}
			// and now print a message for everything that still wasn't processed.
			for (auto &sec : level.sectors)
			{
				if (sec.PortalGroup == 0)
				{
					Printf("Unable to assign sector %d to any group. Possibly self-referencing\n", sec.sectornum);
				}
				else if (sec.PortalGroup == -1) sec.PortalGroup = 0;
			}
		}
	}
	bogus |= ConnectGroups();
	if (bogus)
	{
		// todo: disable all portals whose offsets do not match the associated groups
	}

	// reject would just get in the way when checking sight through portals.
	if (Displacements.size > 1)
	{
		level.rejectmatrix.Reset();
	}
	// finally we must flag all planes which are obstructed by the sector's own ceiling or floor.
	for (auto &sec : level.sectors)
	{
		sec.CheckPortalPlane(sector_t::floor);
		sec.CheckPortalPlane(sector_t::ceiling);
		// set a flag on each line connecting to a plane portal sector. This is used to reduce the amount of checks in P_CheckSight.
		if (sec.PortalIsLinked(sector_t::floor) || sec.PortalIsLinked(sector_t::ceiling))
		{
			for(auto ln : sec.Lines)
			{
				ln->flags |= ML_PORTALCONNECT;
			}
		}
		if (sec.PortalIsLinked(sector_t::ceiling) && sec.PortalIsLinked(sector_t::floor))
		{
			double cz = sec.GetPortalPlaneZ(sector_t::ceiling);
			double fz = sec.GetPortalPlaneZ(sector_t::floor);
			if (cz < fz)
			{
				// This is a fatal condition. We have to remove one of the two portals. Choose the one that doesn't match the current plane
				Printf("Error in sector %d: Ceiling portal at z=%f is below floor portal at z=%f\n", sec.sectornum, cz, fz);
				double cp = -sec.ceilingplane.fD();
				double fp = sec.floorplane.fD();
				if (cp < fp || fz == fp)
				{
					sec.ClearPortal(sector_t::ceiling);
				}
				else
				{
					sec.ClearPortal(sector_t::floor);
				}
			}
		}
		// mark all sector planes that check out ok for everything.
		if (sec.PortalIsLinked(sector_t::floor)) sec.planes[sector_t::floor].Flags |= PLANEF_LINKED;
		if (sec.PortalIsLinked(sector_t::ceiling)) sec.planes[sector_t::ceiling].Flags |= PLANEF_LINKED;
	}
	if (linkedPortals.Size() > 0)
	{
		// We need to relink all actors that may touch a linked line portal
		TThinkerIterator<AActor> it;
		AActor *actor;
		while ((actor = it.Next()))
		{
			if (!(actor->flags & MF_NOBLOCKMAP))
			{
				FPortalGroupArray check(FPortalGroupArray::PGA_NoSectorPortals);
				P_CollectConnectedGroups(actor->Sector->PortalGroup, actor->Pos(), actor->Top(), actor->radius, check);
				if (check.Size() > 0)
				{
					FLinkContext ctx;
					actor->UnlinkFromWorld(&ctx);
					actor->LinkToWorld(&ctx);
				}
			}
		}
	}
}


//============================================================================
//
// Collect all portal groups this actor would occupy at the given position
// This is used to determine which parts of the map need to be checked.
//
//============================================================================

bool P_CollectConnectedGroups(int startgroup, const DVector3 &position, double upperz, double checkradius, FPortalGroupArray &out)
{
	// Keep this temporary work stuff static. This function can never be called recursively
	// and this would have to be reallocated for each call otherwise.
	static FPortalBits processMask;
	static TArray<FLinePortal*> foundPortals;
	static TArray<int> groupsToCheck;

	bool retval = false;
	out.inited = true;

	processMask.setSize(Displacements.size);
	if (Displacements.size == 1)
	{
		processMask.setBit(startgroup);
		return false;
	}

	if (linkedPortals.Size() != 0)
	{
		processMask.clear();
		foundPortals.Clear();

		int thisgroup = startgroup;
		processMask.setBit(thisgroup);
		//out.Add(thisgroup);

		for (unsigned i = 0; i < linkedPortals.Size(); i++)
		{
			line_t *ld = linkedPortals[i]->mOrigin;
			int othergroup = ld->frontsector->PortalGroup;
			FDisplacement &disp = Displacements(thisgroup, othergroup);
			if (!disp.isSet) continue;	// no connection.

			FBoundingBox box(position.X + disp.pos.X, position.Y + disp.pos.Y, checkradius);

			if (!box.inRange(ld) || box.BoxOnLineSide(linkedPortals[i]->mOrigin) != -1) continue;	// not touched
			foundPortals.Push(linkedPortals[i]);
		}
		bool foundone = true;
		while (foundone)
		{
			foundone = false;
			for (int i = foundPortals.Size() - 1; i >= 0; i--)
			{
				if (processMask.getBit(foundPortals[i]->mOrigin->frontsector->PortalGroup) &&
					!processMask.getBit(foundPortals[i]->mDestination->frontsector->PortalGroup))
				{
					processMask.setBit(foundPortals[i]->mDestination->frontsector->PortalGroup);
					out.Add(foundPortals[i]->mDestination->frontsector->PortalGroup);
					foundone = true;
					retval = true;
					foundPortals.Delete(i);
				}
			}
		}
	}
	if (out.method != FPortalGroupArray::PGA_NoSectorPortals)
	{
		sector_t *sec = P_PointInSector(position);
		sector_t *wsec = sec;
		while (!wsec->PortalBlocksMovement(sector_t::ceiling) && upperz > wsec->GetPortalPlaneZ(sector_t::ceiling))
		{
			int othergroup = wsec->GetOppositePortalGroup(sector_t::ceiling);
			DVector2 pos = Displacements.getOffset(startgroup, othergroup) + position;
			if (processMask.getBit(othergroup)) break;
			processMask.setBit(othergroup);
			out.Add(othergroup | FPortalGroupArray::UPPER);
			wsec = P_PointInSector(pos);	// get upper sector at the exact spot we want to check and repeat
			retval = true;
		}
		wsec = sec;
		while (!wsec->PortalBlocksMovement(sector_t::floor) && position.Z < wsec->GetPortalPlaneZ(sector_t::floor))
		{
			int othergroup = wsec->GetOppositePortalGroup(sector_t::floor);
			DVector2 pos = Displacements.getOffset(startgroup, othergroup) + position;
			if (processMask.getBit(othergroup)) break;
			processMask.setBit(othergroup);
			out.Add(othergroup | FPortalGroupArray::LOWER);
			wsec = P_PointInSector(pos);	// get lower sector at the exact spot we want to check and repeat
			retval = true;
		}
		if (out.method == FPortalGroupArray::PGA_Full3d && PortalBlockmap.hasLinkedSectorPortals)
		{
			groupsToCheck.Clear();
			groupsToCheck.Push(startgroup);
			int thisgroup = startgroup;
			for (unsigned i = 0; i < groupsToCheck.Size();i++)
			{
				DVector2 disp = Displacements.getOffset(startgroup, thisgroup & ~FPortalGroupArray::FLAT);
				FBoundingBox box(position.X + disp.X, position.Y + disp.Y, checkradius);
				FBlockLinesIterator it(box);
				line_t *ld;
				while ((ld = it.Next()))
				{
					if (!box.inRange(ld) || box.BoxOnLineSide(ld) != -1)
						continue;

					if (!(thisgroup & FPortalGroupArray::LOWER))
					{
						for (int s = 0; s < 2; s++)
						{
							sector_t *sec = s ? ld->backsector : ld->frontsector;
							if (sec && !(sec->PortalBlocksMovement(sector_t::ceiling)))
							{
								if (sec->GetPortalPlaneZ(sector_t::ceiling) < upperz)
								{
									int grp = sec->GetOppositePortalGroup(sector_t::ceiling);
									if (!(processMask.getBit(grp)))
									{
										processMask.setBit(grp);
										groupsToCheck.Push(grp | FPortalGroupArray::UPPER);
									}
								}
							}
						}
					}
					if (!(thisgroup & FPortalGroupArray::UPPER))
					{
						for (int s = 0; s < 2; s++)
						{
							sector_t *sec = s ? ld->backsector : ld->frontsector;
							if (sec && !(sec->PortalBlocksMovement(sector_t::floor)))
							{
								if (sec->GetPortalPlaneZ(sector_t::floor) > position.Z)
								{
									int grp = sec->GetOppositePortalGroup(sector_t::floor);
									if (!(processMask.getBit(grp)))
									{
										processMask.setBit(grp);
										groupsToCheck.Push(grp | FPortalGroupArray::LOWER);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return retval;
}


//============================================================================
//
// print the group link table to the console
//
//============================================================================

CCMD(dumplinktable)
{
	for (int x = 1; x < Displacements.size; x++)
	{
		for (int y = 1; y < Displacements.size; y++)
		{
			FDisplacement &disp = Displacements(x, y);
			Printf("%c%c(%6d, %6d)", TEXTCOLOR_ESCAPE, 'C' + disp.indirect, int(disp.pos.X), int(disp.pos.Y));
		}
		Printf("\n");
	}
}




