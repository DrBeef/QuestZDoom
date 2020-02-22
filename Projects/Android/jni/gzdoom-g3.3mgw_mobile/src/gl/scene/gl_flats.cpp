// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
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
** gl_flat.cpp
** Flat rendering
**
*/

#include "gl/system/gl_system.h"
#include "a_sharedglobal.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_utility.h"
#include "g_level.h"
#include "doomstat.h"
#include "d_player.h"
#include "portal.h"
#include "templates.h"
#include "g_levellocals.h"
#include "actorinlines.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/shaders/gl_shader.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/renderer/gl_quaddrawer.h"

#ifdef _DEBUG
CVAR(Int, gl_breaksec, -1, 0)
#endif
//==========================================================================
//
// Sets the texture matrix according to the plane's texture positioning
// information
//
//==========================================================================

void gl_SetPlaneTextureRotation(const GLSectorPlane * secplane, FMaterial * gltexture)
{
	// only manipulate the texture matrix if needed.
	if (!secplane->Offs.isZero() ||
		secplane->Scale.X != 1. || secplane->Scale.Y != 1 ||
		secplane->Angle != 0 ||
		gltexture->TextureWidth() != 64 ||
		gltexture->TextureHeight() != 64)
	{
		float uoffs = secplane->Offs.X / gltexture->TextureWidth();
		float voffs = secplane->Offs.Y / gltexture->TextureHeight();

		float xscale1 = secplane->Scale.X;
		float yscale1 = secplane->Scale.Y;
		if (gltexture->tex->bHasCanvas)
		{
			yscale1 = 0 - yscale1;
		}
		float angle = -secplane->Angle;

		float xscale2 = 64.f / gltexture->TextureWidth();
		float yscale2 = 64.f / gltexture->TextureHeight();

		gl_RenderState.mTextureMatrix.loadIdentity();
		gl_RenderState.mTextureMatrix.scale(xscale1, yscale1, 1.0f);
		gl_RenderState.mTextureMatrix.translate(uoffs, voffs, 0.0f);
		gl_RenderState.mTextureMatrix.scale(xscale2, yscale2, 1.0f);
		gl_RenderState.mTextureMatrix.rotate(angle, 0.0f, 0.0f, 1.0f);
		gl_RenderState.EnableTextureMatrix(true);
	}
}



//==========================================================================
//
// Flats 
//
//==========================================================================
extern FDynLightData lightdata;

void GLFlat::SetupSubsectorLights(int pass, subsector_t * sub, int *dli)
{
	Plane p;

	if (renderstyle == STYLE_Add && !glset.lightadditivesurfaces) return;	// no lights on additively blended surfaces.

	if (dli != NULL && *dli != -1)
	{
		gl_RenderState.ApplyLightIndex(GLRenderer->mLights->GetIndex(*dli));
		(*dli)++;
		return;
	}

	lightdata.Clear();
	FLightNode * node = sub->lighthead;
	while (node)
	{
		FDynamicLight * light = node->lightsource;
			
		if (!light->IsActive())
		{
			node=node->nextLight;
			continue;
		}
		iter_dlightf++;

		// we must do the side check here because gl_SetupLight needs the correct plane orientation
		// which we don't have for Legacy-style 3D-floors
		double planeh = plane.plane.ZatPoint(light->Pos);
		if (gl_lights_checkside && ((planeh<light->Z() && ceiling) || (planeh>light->Z() && !ceiling)))
		{
			node = node->nextLight;
			continue;
		}

		p.Set(plane.plane);
		gl_GetLight(sub->sector->PortalGroup, p, light, false, lightdata);
		node = node->nextLight;
	}

	int d = GLRenderer->mLights->UploadLights(lightdata);
	if (pass == GLPASS_LIGHTSONLY)
	{
		GLRenderer->mLights->StoreIndex(d);
	}
	else
	{
		gl_RenderState.ApplyLightIndex(d);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void GLFlat::DrawSubsector(subsector_t * sub)
{
	if (gl.buffermethod != BM_DEFERRED)
	{
#ifdef NO_VBO
        if(gl.novbo)
        {
            glBegin(GL_TRIANGLE_FAN);
            for(unsigned int k=0; k<sub->numlines; k++)
            {
                vertex_t *vt = sub->firstline[k].v1;
                glTexCoord2f(vt->fX()/64.f, -vt->fY()/64.f);
                float zc = plane.plane.ZatPoint(vt) + dz;
                glVertex3f(vt->fX(), zc, vt->fY());
            }
            glEnd();
    	}
    	else
    	{
#endif
		FFlatVertex *ptr = GLRenderer->mVBO->GetBuffer();
		for (unsigned int k = 0; k < sub->numlines; k++)
		{
			vertex_t *vt = sub->firstline[k].v1;
			ptr->x = vt->fX();
			ptr->z = plane.plane.ZatPoint(vt) + dz;
			ptr->y = vt->fY();
			ptr->u = vt->fX() / 64.f;
			ptr->v = -vt->fY() / 64.f;
			ptr++;
		}
		GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_FAN);
#ifdef NO_VBO
        }
#endif
	}
	else
	{
		// if we cannot access the buffer, use the quad drawer as fallback by splitting the subsector into quads.
		// Trying to get this into the vertex buffer in the processing pass is too costly and this is only used for render hacks.
		FQuadDrawer qd;
		unsigned int vi[4];

		vi[0] = 0;
		for (unsigned int i = 0; i < sub->numlines - 2; i += 2)
		{
			for (unsigned int j = 1; j < 4; j++)
			{
				vi[j] = MIN(i + j, sub->numlines - 1);
			}
			for (unsigned int x = 0; x < 4; x++)
			{
				vertex_t *vt = sub->firstline[vi[x]].v1;
				qd.Set(x, vt->fX(), plane.plane.ZatPoint(vt) + dz, vt->fY(), vt->fX() / 64.f, -vt->fY() / 64.f);
			}
			qd.Render(GL_TRIANGLE_FAN);
		}
	}

	flatvertices += sub->numlines;
	flatprimitives++;
}


//==========================================================================
//
// this is only used by LM_DEFERRED
//
//==========================================================================

void GLFlat::ProcessLights(bool istrans)
{
	dynlightindex = GLRenderer->mLights->GetIndexPtr();

	// Draw the subsectors belonging to this sector
	for (int i=0; i<sector->subsectorcount; i++)
	{
		subsector_t * sub = sector->subsectors[i];
		if (gl_drawinfo->ss_renderflags[sub->Index()]&renderflags || istrans)
		{
			SetupSubsectorLights(GLPASS_LIGHTSONLY, sub);
		}
	}

	// Draw the subsectors assigned to it due to missing textures
	if (!(renderflags&SSRF_RENDER3DPLANES))
	{
		gl_subsectorrendernode * node = (renderflags&SSRF_RENDERFLOOR)?
			gl_drawinfo->GetOtherFloorPlanes(sector->sectornum) :
			gl_drawinfo->GetOtherCeilingPlanes(sector->sectornum);

		while (node)
		{
			SetupSubsectorLights(GLPASS_LIGHTSONLY, node->sub);
			node = node->next;
		}
	}
}


//==========================================================================
//
//
//
//==========================================================================

void GLFlat::DrawSubsectors(int pass, bool processlights, bool istrans)
{
	int dli = dynlightindex;

	gl_RenderState.Apply();
#ifdef NO_VBO
    if(gl.novbo)
        vboindex = -1;
#endif
	if (vboindex >= 0)
	{
		int index = vboindex;
		for (int i=0; i<sector->subsectorcount; i++)
		{
			subsector_t * sub = sector->subsectors[i];
				
			if (gl_drawinfo->ss_renderflags[sub->Index()]&renderflags || istrans)
			{
				if (processlights) SetupSubsectorLights(GLPASS_ALL, sub, &dli);
				drawcalls.Clock();
				glDrawArrays(GL_TRIANGLE_FAN, index, sub->numlines);
				drawcalls.Unclock();
				flatvertices += sub->numlines;
				flatprimitives++;
			}
			index += sub->numlines;
		}
	}
	else
	{
		// Draw the subsectors belonging to this sector
		// (can this case even happen?)
		for (int i=0; i<sector->subsectorcount; i++)
		{
			subsector_t * sub = sector->subsectors[i];
			if (gl_drawinfo->ss_renderflags[sub->Index()]&renderflags || istrans)
			{
				if (processlights) SetupSubsectorLights(GLPASS_ALL, sub, &dli);
				DrawSubsector(sub);
			}
		}
	}

	// Draw the subsectors assigned to it due to missing textures
	if (!(renderflags&SSRF_RENDER3DPLANES))
	{
		gl_subsectorrendernode * node = (renderflags&SSRF_RENDERFLOOR)?
			gl_drawinfo->GetOtherFloorPlanes(sector->sectornum) :
			gl_drawinfo->GetOtherCeilingPlanes(sector->sectornum);

		while (node)
		{
			if (processlights) SetupSubsectorLights(GLPASS_ALL, node->sub, &dli);
			DrawSubsector(node->sub);
			node = node->next;
		}
	}
#ifdef NO_VBO
    if(gl.novbo)
        GLRenderer->mVBO->BindVBO();
#endif
}


//==========================================================================
//
// special handling for skyboxes which need texture clamping.
// This will find the bounding rectangle of the sector and just
// draw one single polygon filling that rectangle with a clamped
// texture.
//
//==========================================================================

void GLFlat::DrawSkyboxSector(int pass, bool processlights)
{

	float minx = FLT_MAX, miny = FLT_MAX;
	float maxx = -FLT_MAX, maxy = -FLT_MAX;

	for (auto ln : sector->Lines)
	{
		float x = ln->v1->fX();
		float y = ln->v1->fY();
		if (x < minx) minx = x;
		if (y < miny) miny = y;
		if (x > maxx) maxx = x;
		if (y > maxy) maxy = y;
		x = ln->v2->fX();
		y = ln->v2->fY();
		if (x < minx) minx = x;
		if (y < miny) miny = y;
		if (x > maxx) maxx = x;
		if (y > maxy) maxy = y;
	}

	float z = plane.plane.ZatPoint(0., 0.) + dz;
	static float uvals[] = { 0, 0, 1, 1 };
	static float vvals[] = { 1, 0, 0, 1 };
	int rot = -xs_FloorToInt(plane.Angle / 90.f);

	FQuadDrawer qd;

	qd.Set(0, minx, z, miny, uvals[rot & 3], vvals[rot & 3]);
	qd.Set(1, minx, z, maxy, uvals[(rot + 1) & 3], vvals[(rot + 1) & 3]);
	qd.Set(2, maxx, z, maxy, uvals[(rot + 2) & 3], vvals[(rot + 2) & 3]);
	qd.Set(3, maxx, z, miny, uvals[(rot + 3) & 3], vvals[(rot + 3) & 3]);
	qd.Render(GL_TRIANGLE_FAN);

	flatvertices += 4;
	flatprimitives++;
}


//==========================================================================
//
//
//
//==========================================================================
void GLFlat::Draw(int pass, bool trans)	// trans only has meaning for GLPASS_LIGHTSONLY
{
	int rel = getExtraLight();

#ifdef _DEBUG
	if (sector->sectornum == gl_breaksec)
	{
		int a = 0;
	}
#endif

	gl_RenderState.SetNormal(plane.plane.Normal().X, plane.plane.Normal().Z, plane.plane.Normal().Y);

	switch (pass)
	{
	case GLPASS_PLAIN:			// Single-pass rendering
	case GLPASS_ALL:			// Same, but also creates the dynlight data.
		mDrawer->SetColor(lightlevel, rel, Colormap,1.0f);
		mDrawer->SetFog(lightlevel, rel, &Colormap, false);
		gl_RenderState.SetAddColor(AddColor | 0xff000000);
		if (!gltexture->tex->isFullbright())
			gl_RenderState.SetObjectColor(FlatColor | 0xff000000);
		if (sector->special != GLSector_Skybox)
		{
			gl_RenderState.SetMaterial(gltexture, CLAMP_NONE, 0, -1, false);
			gl_SetPlaneTextureRotation(&plane, gltexture);
			DrawSubsectors(pass, (pass == GLPASS_ALL || dynlightindex > -1), false);
			gl_RenderState.EnableTextureMatrix(false);
		}
		else
		{
			gl_RenderState.SetMaterial(gltexture, CLAMP_XY, 0, -1, false);
			DrawSkyboxSector(pass, (pass == GLPASS_ALL || dynlightindex > -1));
		}
		gl_RenderState.SetObjectColor(0xffffffff);
		break;

	case GLPASS_LIGHTSONLY:
		if (!trans || gltexture)
		{
			ProcessLights(trans);
		}
		break;

	case GLPASS_TRANSLUCENT:
		if (renderstyle==STYLE_Add) gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE);
		mDrawer->SetColor(lightlevel, rel, Colormap, alpha);
		mDrawer->SetFog(lightlevel, rel, &Colormap, false);
		if (!gltexture || !gltexture->tex->isFullbright())
			gl_RenderState.SetObjectColor(FlatColor | 0xff000000);
		if (!gltexture)
		{
			gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
			gl_RenderState.EnableTexture(false);
			DrawSubsectors(pass, false, true);
			gl_RenderState.EnableTexture(true);
		}
		else 
		{
			if (!gltexture->GetTransparent()) gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_threshold);
			else gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
			gl_RenderState.SetMaterial(gltexture, CLAMP_NONE, 0, -1, false);
			gl_SetPlaneTextureRotation(&plane, gltexture);
			DrawSubsectors(pass, !gl.legacyMode && (gl.lightmethod == LM_DIRECT || dynlightindex > -1), true);
			gl_RenderState.EnableTextureMatrix(false);
		}
		if (renderstyle==STYLE_Add) gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl_RenderState.SetObjectColor(0xffffffff);
		break;

	case GLPASS_LIGHTTEX:
	case GLPASS_LIGHTTEX_ADDITIVE:
	case GLPASS_LIGHTTEX_FOGGY:
		DrawLightsCompat(pass);
		break;

	case GLPASS_TEXONLY:
		gl_RenderState.SetMaterial(gltexture, CLAMP_NONE, 0, -1, false);
		gl_SetPlaneTextureRotation(&plane, gltexture);
		DrawSubsectors(pass, false, false);
		gl_RenderState.EnableTextureMatrix(false);
		break;
	}
	gl_RenderState.SetAddColor(0);
}


//==========================================================================
//
// GLFlat::PutFlat
//
// Checks texture, lighting and translucency settings and puts this
// plane in the appropriate render list.
//
//==========================================================================
inline void GLFlat::PutFlat(bool fog)
{
	int list;

	if (mDrawer->FixedColormap) 
	{
		Colormap.Clear();
	}
	if (gl.legacyMode)
	{
		if (PutFlatCompat(fog)) return;
	}
	if (renderstyle!=STYLE_Translucent || alpha < 1.f - FLT_EPSILON || fog || gltexture == NULL)
	{
		// translucent 3D floors go into the regular translucent list, translucent portals go into the translucent border list.
		list = (renderflags&SSRF_RENDER3DPLANES) ? GLDL_TRANSLUCENT : GLDL_TRANSLUCENTBORDER;
	}
	else if (gltexture->GetTransparent())
	{
		if (stack)
		{
			list = GLDL_TRANSLUCENTBORDER;
		}
		else if ((renderflags&SSRF_RENDER3DPLANES) && !plane.plane.isSlope())
		{
			list = GLDL_TRANSLUCENT;
		} 
		else 
		{
			list = GLDL_PLAINFLATS;
		}
	}
	else
	{
		bool masked = gltexture->isMasked() && ((renderflags&SSRF_RENDER3DPLANES) || stack);
		list = masked ? GLDL_MASKEDFLATS : GLDL_PLAINFLATS;
	}
	dynlightindex = -1;	// make sure this is always initialized to something proper.
	gl_drawinfo->drawlists[list].AddFlat (this);
}

//==========================================================================
//
// This draws one flat 
// The passed sector does not indicate the area which is rendered. 
// It is only used as source for the plane data.
// The whichplane boolean indicates if the flat is a floor(false) or a ceiling(true)
//
//==========================================================================

void GLFlat::Process(sector_t * model, int whichplane, bool fog)
{
	plane.GetFromSector(model, whichplane);
	if (whichplane != int(ceiling))
	{
		// Flip the normal if the source plane has a different orientation than what we are about to render.
		plane.plane.FlipVert();
	}

	if (!fog)
	{
		gltexture=FMaterial::ValidateTexture(plane.texture, false, true);
		if (!gltexture) return;
		if (gltexture->tex->isFullbright()) 
		{
			Colormap.MakeWhite();
			lightlevel=255;
		}
	}
	else 
	{
		gltexture = NULL;
		lightlevel = abs(lightlevel);
	}

	// get height from vplane
	if (whichplane == sector_t::floor && sector->transdoor) dz = -1;
	else dz = 0;

	z = plane.plane.ZatPoint(0.f, 0.f);
	
	PutFlat(fog);
	rendered_flats++;
}

//==========================================================================
//
// Sets 3D floor info. Common code for all 4 cases 
//
//==========================================================================

void GLFlat::SetFrom3DFloor(F3DFloor *rover, bool top, bool underside)
{
	F3DFloor::planeref & plane = top? rover->top : rover->bottom;

	// FF_FOG requires an inverted logic where to get the light from
	lightlist_t *light = P_GetPlaneLight(sector, plane.plane, underside);
	lightlevel = gl_ClampLight(*light->p_lightlevel);
	
	if (rover->flags & FF_FOG)
	{
		Colormap.LightColor = light->extra_colormap.FadeColor;
		FlatColor = 0xffffffff;
		AddColor = 0;
	}
	else
	{
		Colormap.CopyFrom3DLight(light);
		FlatColor = *plane.flatcolor;
		// AddColor = sector->SpecialColors[sector_t::add];
	}

	alpha = rover->alpha/255.0f;
	renderstyle = rover->flags&FF_ADDITIVETRANS? STYLE_Add : STYLE_Translucent;
	if (plane.model->VBOHeightcheck(plane.isceiling))
	{
		vboindex = plane.vindex;
	}
	else
	{
		vboindex = -1;
	}
}

//==========================================================================
//
// Process a sector's flats for rendering
// This function is only called once per sector.
// Subsequent subsectors are just quickly added to the ss_renderflags array
//
//==========================================================================

void GLFlat::ProcessSector(sector_t * frontsector)
{
	lightlist_t * light;
	FSectorPortal *port;

#ifdef _DEBUG
	if (frontsector->sectornum == gl_breaksec)
	{
		int a = 0;
	}
#endif

	// Get the real sector for this one.
	sector = &level.sectors[frontsector->sectornum];
	extsector_t::xfloor &x = sector->e->XFloor;
	dynlightindex = -1;

	uint8_t &srf = gl_drawinfo->sectorrenderflags[sector->sectornum];

	//
	//
	//
	// do floors
	//
	//
	//
	if (frontsector->floorplane.ZatPoint(r_viewpoint.Pos) <= r_viewpoint.Pos.Z)
	{
		// process the original floor first.

		srf |= SSRF_RENDERFLOOR;

		lightlevel = gl_ClampLight(frontsector->GetFloorLight());
		Colormap = frontsector->Colormap;
		FlatColor = frontsector->SpecialColors[sector_t::floor];
		AddColor = frontsector->AdditiveColors[sector_t::floor];
		port = frontsector->ValidatePortal(sector_t::floor);
		if ((stack = (port != NULL)))
		{
			if (port->mType == PORTS_STACKEDSECTORTHING)
			{
				gl_drawinfo->AddFloorStack(sector);	// stacked sector things require visplane merging.
			}
			alpha = frontsector->GetAlpha(sector_t::floor);
		}
		else
		{
			alpha = 1.0f - frontsector->GetReflect(sector_t::floor);
		}

		if (alpha != 0.f && frontsector->GetTexture(sector_t::floor) != skyflatnum)
		{
			if (frontsector->VBOHeightcheck(sector_t::floor))
			{
				vboindex = frontsector->vboindex[sector_t::floor];
			}
			else
			{
				vboindex = -1;
			}

			ceiling = false;
			renderflags = SSRF_RENDERFLOOR;

			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sector, &frontsector->floorplane, false);
				if ((!(sector->GetFlags(sector_t::floor)&PLANEF_ABSLIGHTING) || light->lightsource == NULL)
					&& (light->p_lightlevel != &frontsector->lightlevel))
				{
					lightlevel = gl_ClampLight(*light->p_lightlevel);
				}

				Colormap.CopyFrom3DLight(light);
			}
			renderstyle = STYLE_Translucent;
			Process(frontsector, sector_t::floor, false);
		}
	}

	//
	//
	//
	// do ceilings
	//
	//
	//
	if (frontsector->ceilingplane.ZatPoint(r_viewpoint.Pos) >= r_viewpoint.Pos.Z)
	{
		// process the original ceiling first.

		srf |= SSRF_RENDERCEILING;

		lightlevel = gl_ClampLight(frontsector->GetCeilingLight());
		Colormap = frontsector->Colormap;
		FlatColor = frontsector->SpecialColors[sector_t::ceiling];
		AddColor = frontsector->AdditiveColors[sector_t::ceiling];
		port = frontsector->ValidatePortal(sector_t::ceiling);
		if ((stack = (port != NULL)))
		{
			if (port->mType == PORTS_STACKEDSECTORTHING)
			{
				gl_drawinfo->AddCeilingStack(sector);
			}
			alpha = frontsector->GetAlpha(sector_t::ceiling);
		}
		else
		{
			alpha = 1.0f - frontsector->GetReflect(sector_t::ceiling);
		}

		if (alpha != 0.f && frontsector->GetTexture(sector_t::ceiling) != skyflatnum)
		{
			if (frontsector->VBOHeightcheck(sector_t::ceiling))
			{
				vboindex = frontsector->vboindex[sector_t::ceiling];
			}
			else
			{
				vboindex = -1;
			}

			ceiling = true;
			renderflags = SSRF_RENDERCEILING;

			if (x.ffloors.Size())
			{
				light = P_GetPlaneLight(sector, &sector->ceilingplane, true);

				if ((!(sector->GetFlags(sector_t::ceiling)&PLANEF_ABSLIGHTING))
					&& (light->p_lightlevel != &frontsector->lightlevel))
				{
					lightlevel = gl_ClampLight(*light->p_lightlevel);
				}
				Colormap.CopyFrom3DLight(light);
			}
			renderstyle = STYLE_Translucent;
			Process(frontsector, sector_t::ceiling, false);
		}
	}

	//
	//
	//
	// do 3D floors
	//
	//
	//

	stack = false;
	if (x.ffloors.Size())
	{
		player_t * player = players[consoleplayer].camera->player;

		renderflags = SSRF_RENDER3DPLANES;
		srf |= SSRF_RENDER3DPLANES;
		// 3d-floors must not overlap!
		double lastceilingheight = sector->CenterCeiling();	// render only in the range of the
		double lastfloorheight = sector->CenterFloor();		// current sector part (if applicable)
		F3DFloor * rover;
		int k;

		// floors are ordered now top to bottom so scanning the list for the best match
		// is no longer necessary.

		ceiling = true;
		Colormap = frontsector->Colormap;
		for (k = 0; k < (int)x.ffloors.Size(); k++)
		{
			rover = x.ffloors[k];

			if ((rover->flags&(FF_EXISTS | FF_RENDERPLANES | FF_THISINSIDE)) == (FF_EXISTS | FF_RENDERPLANES))
			{
				if (rover->flags&FF_FOG && mDrawer->FixedColormap) continue;
				if (!rover->top.copied && rover->flags&(FF_INVERTPLANES | FF_BOTHPLANES))
				{
					double ff_top = rover->top.plane->ZatPoint(sector->centerspot);
					if (ff_top < lastceilingheight)
					{
						if (r_viewpoint.Pos.Z <= rover->top.plane->ZatPoint(r_viewpoint.Pos))
						{
							SetFrom3DFloor(rover, true, !!(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;
							Process(rover->top.model, rover->top.isceiling, !!(rover->flags&FF_FOG));
						}
						lastceilingheight = ff_top;
					}
				}
				if (!rover->bottom.copied && !(rover->flags&FF_INVERTPLANES))
				{
					double ff_bottom = rover->bottom.plane->ZatPoint(sector->centerspot);
					if (ff_bottom < lastceilingheight)
					{
						if (r_viewpoint.Pos.Z <= rover->bottom.plane->ZatPoint(r_viewpoint.Pos))
						{
							SetFrom3DFloor(rover, false, !(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;
							Process(rover->bottom.model, rover->bottom.isceiling, !!(rover->flags&FF_FOG));
						}
						lastceilingheight = ff_bottom;
						if (rover->alpha < 255) lastceilingheight += EQUAL_EPSILON;
					}
				}
			}
		}

		ceiling = false;
		for (k = x.ffloors.Size() - 1; k >= 0; k--)
		{
			rover = x.ffloors[k];

			if ((rover->flags&(FF_EXISTS | FF_RENDERPLANES | FF_THISINSIDE)) == (FF_EXISTS | FF_RENDERPLANES))
			{
				if (rover->flags&FF_FOG && mDrawer->FixedColormap) continue;
				if (!rover->bottom.copied && rover->flags&(FF_INVERTPLANES | FF_BOTHPLANES))
				{
					double ff_bottom = rover->bottom.plane->ZatPoint(sector->centerspot);
					if (ff_bottom > lastfloorheight || (rover->flags&FF_FIX))
					{
						if (r_viewpoint.Pos.Z >= rover->bottom.plane->ZatPoint(r_viewpoint.Pos))
						{
							SetFrom3DFloor(rover, false, !(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;

							if (rover->flags&FF_FIX)
							{
								lightlevel = gl_ClampLight(rover->model->lightlevel);
								Colormap = rover->GetColormap();
							}

							Process(rover->bottom.model, rover->bottom.isceiling, !!(rover->flags&FF_FOG));
						}
						lastfloorheight = ff_bottom;
					}
				}
				if (!rover->top.copied && !(rover->flags&FF_INVERTPLANES))
				{
					double ff_top = rover->top.plane->ZatPoint(sector->centerspot);
					if (ff_top > lastfloorheight)
					{
						if (r_viewpoint.Pos.Z >= rover->top.plane->ZatPoint(r_viewpoint.Pos))
						{
							SetFrom3DFloor(rover, true, !!(rover->flags&FF_FOG));
							Colormap.FadeColor = frontsector->Colormap.FadeColor;
							Process(rover->top.model, rover->top.isceiling, !!(rover->flags&FF_FOG));
						}
						lastfloorheight = ff_top;
						if (rover->alpha < 255) lastfloorheight -= EQUAL_EPSILON;
					}
				}
			}
		}
	}
}

