// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2002-2016 Christoph Oelckers
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
** gl_light.cpp
** Light level / fog management / dynamic lights
**
**/

#include "gl/system/gl_system.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/data/gl_data.h"
#include "gl/renderer/gl_colormap.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/shaders/gl_shader.h"
#include "gl/scene/gl_portal.h"
#include "c_dispatch.h"
#include "p_local.h"
#include "g_level.h"
#include "r_sky.h"
#include "g_levellocals.h"

// externally settable lighting properties
static float distfogtable[2][256];	// light to fog conversion table for black fog

CVAR(Int, gl_weaponlight, 0, CVAR_ARCHIVE); // Default to 0 for VR

CUSTOM_CVAR(Bool, gl_enhanced_nightvision, false, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	// The fixed colormap state needs to be reset because if this happens when
	// a shader is set to CM_LITE or CM_TORCH it won't register the change in behavior caused by this CVAR.
	if (GLRenderer != nullptr && GLRenderer->mShaderManager != nullptr)
	{
		GLRenderer->mShaderManager->ResetFixedColormap();
	}
}
CVAR(Bool, gl_brightfog, false, CVAR_ARCHIVE);
CVAR(Bool, gl_lightadditivesurfaces, false, CVAR_ARCHIVE);



//==========================================================================
//
// Sets up the fog tables
//
//==========================================================================

CUSTOM_CVAR (Int, gl_distfog, 70, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	for (int i=0;i<256;i++)
	{

		if (i<164)
		{
			distfogtable[0][i]= (gl_distfog>>1) + (gl_distfog)*(164-i)/164;
		}
		else if (i<230)
		{											    
			distfogtable[0][i]= (gl_distfog>>1) - (gl_distfog>>1)*(i-164)/(230-164);
		}
		else distfogtable[0][i]=0;

		if (i<128)
		{
			distfogtable[1][i]= 6.f + (gl_distfog>>1) + (gl_distfog)*(128-i)/48;
		}
		else if (i<216)
		{											    
			distfogtable[1][i]= (216.f-i) / ((216.f-128.f)) * gl_distfog / 10;
		}
		else distfogtable[1][i]=0;
	}
}

CUSTOM_CVAR(Int,gl_fogmode,1,CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (self>2) self=2;
	if (self<0) self=0;
}

CUSTOM_CVAR(Int, gl_lightmode, 2,CVAR_ARCHIVE|CVAR_NOINITCALL) // Use Standard Sector Lighting for VR
{
	int newself = self;
	if (newself > 8) newself = 16;	// use 8 and 16 for software lighting to avoid conflicts with the bit mask
	else if (newself > 4) newself = 8;
	else if (newself < 0) newself = 0;
	if (self != newself) self = newself;
	glset.lightmode = newself;
}




//==========================================================================
//
// Sets render state to draw the given render style
// includes: Texture mode, blend equation and blend mode
//
//==========================================================================

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be)
{
	static int blendstyles[] = { GL_ZERO, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA };
	static int renderops[] = { 0, GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1 };

	int srcblend = blendstyles[style.SrcAlpha&3];
	int dstblend = blendstyles[style.DestAlpha&3];
	int blendequation = renderops[style.BlendOp&15];
	int texturemode = drawopaque? TM_OPAQUE : TM_MODULATE;

	if (style.Flags & STYLEF_RedIsAlpha)
	{
		texturemode = TM_REDTOALPHA;
	}
	else if (style.Flags & STYLEF_ColorIsFixed)
	{
		texturemode = TM_MASK;
	}
	else if (style.Flags & STYLEF_InvertSource)
	{
		// The only place where InvertSource is used is for inverted sprites with the infrared powerup.
		texturemode = TM_INVERSE;
	}

	if (blendequation == -1)
	{
		srcblend = GL_DST_COLOR;
		dstblend = GL_ONE_MINUS_SRC_ALPHA;
		blendequation = GL_FUNC_ADD;
	}

	if (allowcolorblending && srcblend == GL_SRC_ALPHA && dstblend == GL_ONE && blendequation == GL_FUNC_ADD)
	{
#ifdef __MOBILE__ // Not avaliable for gles1 :(
		if( gl.glesVer > 1 )
#endif
		srcblend = GL_SRC_COLOR;
	}

	*tm = texturemode;
	*be = blendequation;
	*sb = srcblend;
	*db = dstblend;
}

#ifdef __MOBILE__
EXTERN_CVAR(Float, vid_brightness)
#endif
//==========================================================================
//
// Get current light level
//
//==========================================================================

int gl_CalcLightLevel(int lightlevel, int rellight, bool weapon, int blendfactor)
{
	int light;

	if (lightlevel <= 0) return 0;

	bool darklightmode = (glset.lightmode & 2) || (glset.lightmode >= 8 && blendfactor > 0);

	if (darklightmode && lightlevel < 192 && !weapon) 
	{
		if (lightlevel > 100)
		{
			light = xs_CRoundToInt(192.f - (192 - lightlevel)* 1.87f);
			if (light + rellight < 20)
			{
				light = 20 + (light + rellight - 20) / 5;
			}
			else
			{
				light += rellight;
			}
		}
		else
		{
			light = (lightlevel + rellight) / 5;
		}

	}
	else
	{
		light=lightlevel+rellight;
	}
#ifdef __MOBILE__ // Hook in brightness slider to this also, taken from old renderer
	if (gl.glesVer < 3)
	{
	    int brightness = vid_brightness * 255;
	    if (light<brightness && glset.lightmode != 8)		// ambient clipping only if not using software lighting model.
	    {
	        light = brightness;
	        if (rellight<0) rellight>>=1;
	    }
	}
#endif
	return clamp(light, 0, 255);
}

//==========================================================================
//
// Get current light color
//
//==========================================================================

static PalEntry gl_CalcLightColor(int light, PalEntry pe, int blendfactor)
{
	int r,g,b;

	if (blendfactor == 0)
	{
		if (glset.lightmode >= 8)
		{
			return pe;
		}

		r = pe.r * light / 255;
		g = pe.g * light / 255;
		b = pe.b * light / 255;
	}
	else
	{
		// This is what Legacy does with colored light in 3D volumes. No, it doesn't really make sense...
		// It also doesn't translate well to software style lighting.
		int mixlight = light * (255 - blendfactor);

		r = (mixlight + pe.r * blendfactor) / 255;
		g = (mixlight + pe.g * blendfactor) / 255;
		b = (mixlight + pe.b * blendfactor) / 255;
	}
	return PalEntry(255, uint8_t(r), uint8_t(g), uint8_t(b));
}

//==========================================================================
//
// set current light color
//
//==========================================================================
void gl_SetColor(int sectorlightlevel, int rellight, bool fullbright, const FColormap &cm, float alpha, bool weapon)
{ 
	if (fullbright)
	{
		gl_RenderState.SetColorAlpha(0xffffff, alpha, 0);
		gl_RenderState.SetSoftLightLevel(255);
	}
	else
	{
		int hwlightlevel = gl_CalcLightLevel(sectorlightlevel, rellight, weapon, cm.BlendFactor);
		PalEntry pe = gl_CalcLightColor(hwlightlevel, cm.LightColor, cm.BlendFactor);
		gl_RenderState.SetColorAlpha(pe, alpha, cm.Desaturation);
		gl_RenderState.SetSoftLightLevel(gl_ClampLight(sectorlightlevel + rellight), cm.BlendFactor);
	}
}

//==========================================================================
//
// calculates the current fog density
//
//	Rules for fog:
//
//  1. If bit 4 of gl_lightmode is set always use the level's fog density. 
//     This is what Legacy's GL render does.
//	2. black fog means no fog and always uses the distfogtable based on the level's fog density setting
//	3. If outside fog is defined and the current fog color is the same as the outside fog
//	   the engine always uses the outside fog density to make the fog uniform across the level.
//	   If the outside fog's density is undefined it uses the level's fog density and if that is
//	   not defined it uses a default of 70.
//	4. If a global fog density is specified it is being used for all fog on the level
//	5. If none of the above apply fog density is based on the light level as for the software renderer.
//
//==========================================================================

float gl_GetFogDensity(int lightlevel, PalEntry fogcolor, int sectorfogdensity, int blendfactor)
{
	float density;

	int lightmode = glset.lightmode;
	if (lightmode >= 8 && blendfactor > 0) lightmode = 2;	// The blendfactor feature does not work with software-style lighting.

	if (lightmode & 4)
	{
		// uses approximations of Legacy's default settings.
		density = level.fogdensity ? level.fogdensity : 18;
	}
	else if (sectorfogdensity != 0)
	{
		// case 1: Sector has an explicit fog density set.
		density = sectorfogdensity;
	}
	else if ((fogcolor.d & 0xffffff) == 0)
	{
		// case 2: black fog
		if ((lightmode < 8 || blendfactor > 0) && !(level.flags3 & LEVEL3_NOLIGHTFADE))
		{
			density = distfogtable[glset.lightmode != 0][gl_ClampLight(lightlevel)];
		}
		else
		{
			density = 0;
		}
	}
	else if (level.outsidefogdensity != 0 && APART(level.info->outsidefog) != 0xff && (fogcolor.d & 0xffffff) == (level.info->outsidefog & 0xffffff))
	{
		// case 3. outsidefogdensity has already been set as needed
		density = level.outsidefogdensity;
	}
	else  if (level.fogdensity != 0)
	{
		// case 4: level has fog density set
		density = level.fogdensity;
	}
	else if (lightlevel < 248)
	{
		// case 5: use light level
		density = clamp<int>(255 - lightlevel, 30, 255);
	}
	else
	{
		density = 0.f;
	}
	return density;
}


//==========================================================================
//
// Check if the current linedef is a candidate for a fog boundary
//
// Requirements for a fog boundary:
// - front sector has no fog
// - back sector has fog
// - at least one of both does not have a sky ceiling.
//
//==========================================================================

bool gl_CheckFog(sector_t *frontsector, sector_t *backsector)
{
	if (frontsector == backsector) return false;	// there can't be a boundary if both sides are in the same sector.

	// Check for fog boundaries. This needs a few more checks for the sectors

	PalEntry fogcolor = frontsector->Colormap.FadeColor;

	if ((fogcolor.d & 0xffffff) == 0)
	{
		return false;
	}
	else if (fogcolor.a != 0)
	{
	}
	else if (level.outsidefogdensity != 0 && APART(level.info->outsidefog) != 0xff && (fogcolor.d & 0xffffff) == (level.info->outsidefog & 0xffffff))
	{
	}
	else  if (level.fogdensity!=0 || (glset.lightmode & 4))
	{
		// case 3: level has fog density set
	}
	else 
	{
		// case 4: use light level
		if (frontsector->lightlevel >= 248) return false;
	}

	fogcolor = backsector->Colormap.FadeColor;

	if ((fogcolor.d & 0xffffff) == 0)
	{
	}
	else if (level.outsidefogdensity != 0 && APART(level.info->outsidefog) != 0xff && (fogcolor.d & 0xffffff) == (level.info->outsidefog & 0xffffff))
	{
		return false;
	}
	else  if (level.fogdensity!=0 || (glset.lightmode & 4))
	{
		// case 3: level has fog density set
		return false;
	}
	else 
	{
		// case 4: use light level
		if (backsector->lightlevel < 248) return false;
	}

	// in all other cases this might create more problems than it solves.
	return ((frontsector->GetTexture(sector_t::ceiling)!=skyflatnum || 
			 backsector->GetTexture(sector_t::ceiling)!=skyflatnum));
}

//==========================================================================
//
// Lighting stuff 
//
//==========================================================================

void gl_SetShaderLight(float level, float olight)
{
	const float MAXDIST = 256.f;
	const float THRESHOLD = 96.f;
	const float FACTOR = 0.75f;

	if (level > 0)
	{
		float lightdist, lightfactor;
			
		if (olight < THRESHOLD)
		{
			lightdist = (MAXDIST/2) + (olight * MAXDIST / THRESHOLD / 2);
			olight = THRESHOLD;
		}
		else lightdist = MAXDIST;

		lightfactor = 1.f + ((olight/level) - 1.f) * FACTOR;
		if (lightfactor == 1.f) lightdist = 0.f;	// save some code in the shader
		gl_RenderState.SetLightParms(lightfactor, lightdist);
	}
	else
	{
		gl_RenderState.SetLightParms(1.f, 0.f);
	}
}


//==========================================================================
//
// Sets the fog for the current polygon
//
//==========================================================================

void gl_SetFog(int lightlevel, int rellight, bool fullbright, const FColormap *cmap, bool isadditive)
{
	PalEntry fogcolor;
	float fogdensity;

	if (level.flags&LEVEL_HASFADETABLE)
	{
		fogdensity=70;
		fogcolor=0x808080;
	}
	else if (cmap != NULL && !fullbright)
	{
		fogcolor = cmap->FadeColor;
		fogdensity = gl_GetFogDensity(lightlevel, fogcolor, cmap->FogDensity, cmap->BlendFactor);
		fogcolor.a=0;
	}
	else
	{
		fogcolor = 0;
		fogdensity = 0;
	}

	// Make fog a little denser when inside a skybox
	if (GLPortal::inskybox) fogdensity+=fogdensity/2;


	// no fog in enhanced vision modes!
	if (fogdensity==0 || gl_fogmode == 0)
	{
		gl_RenderState.EnableFog(false);
		gl_RenderState.SetFog(0,0);
	}
	else
	{
		if ((glset.lightmode == 2 || (glset.lightmode >= 8 && cmap->BlendFactor > 0)) && fogcolor == 0)
		{
			float light = gl_CalcLightLevel(lightlevel, rellight, false, cmap->BlendFactor);
			gl_SetShaderLight(light, lightlevel);
		}
		else
		{
			gl_RenderState.SetLightParms(1.f, 0.f);
		}

		// For additive rendering using the regular fog color here would mean applying it twice
		// so always use black
		if (isadditive)
		{
			fogcolor=0;
		}

		gl_RenderState.EnableFog(true);
		gl_RenderState.SetFog(fogcolor, fogdensity);

		// Korshun: fullbright fog like in software renderer.
		if (glset.lightmode >= 8 && cmap->BlendFactor == 0 && glset.brightfog && fogdensity != 0 && fogcolor != 0)
		{
			gl_RenderState.SetSoftLightLevel(255);
		}
	}
}
