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
** gl_weapon.cpp
** Weapon sprite drawing
**
*/

#include "gl/system/gl_system.h"
#include "sbar.h"
#include "r_utility.h"
#include "v_video.h"
#include "doomstat.h"
#include "d_player.h"
#include "g_level.h"
#include "g_levellocals.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/models/gl_models.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"
#include "gl/renderer/gl_quaddrawer.h"
#include "gl/stereo3d/gl_stereo3d.h"

EXTERN_CVAR (Bool, r_drawplayersprites)
EXTERN_CVAR(Float, transsouls)
EXTERN_CVAR(Int, gl_fuzztype)
EXTERN_CVAR (Bool, r_deathcamera)
EXTERN_CVAR(Int, r_PlayerSprites3DMode)
EXTERN_CVAR(Float, gl_fatItemWidth)

enum PlayerSprites3DMode
{
	CROSSED,
	BACK_ONLY,
	ITEM_ONLY,
	FAT_ITEM,
};


//==========================================================================
//
// R_DrawPSprite
//
//==========================================================================

void GLSceneDrawer::DrawPSprite (player_t * player,DPSprite *psp, float sx, float sy, bool hudModelStep, int OverrideShader, bool alphatexture)
{
	float			fU1,fV1;
	float			fU2,fV2;
	float			tx;
	float			x1,y1,x2,y2;
	float			scale;
	float			scalex;
	float			ftexturemid;
	
	// [BB] In the HUD model step we just render the model and break out. 
	if ( hudModelStep )
	{
		gl_RenderHUDModel(psp, sx, sy);
		return;
	}

	// decide which patch to use
	bool mirror;
	FTextureID lump = sprites[psp->GetSprite()].GetSpriteFrame(psp->GetFrame(), 0, 0., &mirror);
	if (!lump.isValid()) return;

	FMaterial * tex = FMaterial::ValidateTexture(lump, true, false);
	if (!tex) return;

	gl_RenderState.SetMaterial(tex, CLAMP_XY_NOMIP, psp->Flags & PSPF_PLAYERTRANSLATED ? psp->Owner->mo->Translation : 0, OverrideShader, alphatexture);

	float vw = (float)viewwidth;
	float vh = (float)viewheight;

	FloatRect r;
	tex->GetSpriteRect(&r);

	// calculate edges of the shape
	scalex = (320.0f / (240.0f * r_viewwindow.WidescreenRatio)) * vw / 320;

	tx = (psp->Flags & PSPF_MIRROR) ? ((160 - r.width) - (sx + r.left)) : (sx - (160 - r.left));
	x1 = tx * scalex + vw/2;
	if (x1 > vw)	return; // off the right side
	x1 += viewwindowx;

	tx += r.width;
	x2 = tx * scalex + vw / 2;
	if (x2 < 0) return; // off the left side
	x2 += viewwindowx;

	// killough 12/98: fix psprite positioning problem
	ftexturemid = 100.f - sy - r.top - psp->GetYAdjust(screenblocks >= 11);

	AActor * wi=player->ReadyWeapon;
	scale = (SCREENHEIGHT*vw) / (SCREENWIDTH * 200.0f);
	y1 = viewwindowy + vh / 2 - (ftexturemid * scale);
	y2 = y1 + (r.height * scale) + 1;


	if (!(mirror) != !(psp->Flags & (PSPF_FLIP)))
	{
		fU2 = tex->GetSpriteUL();
		fV1 = tex->GetSpriteVT();
		fU1 = tex->GetSpriteUR();
		fV2 = tex->GetSpriteVB();
	}
	else
	{
		fU1 = tex->GetSpriteUL();
		fV1 = tex->GetSpriteVT();
		fU2 = tex->GetSpriteUR();
		fV2 = tex->GetSpriteVB();
		
	}

	if ((tex->GetTransparent() || OverrideShader != -1) && !s3d::Stereo3DMode::getCurrentMode().RenderPlayerSpritesCrossed())
	{
		gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	}
	gl_RenderState.Apply();

	if (r_PlayerSprites3DMode != ITEM_ONLY && r_PlayerSprites3DMode != FAT_ITEM)
	{
		FQuadDrawer qd;
		qd.Set(0, x1, y1, 0, fU1, fV1);
		qd.Set(1, x1, y2, 0, fU1, fV2);
		qd.Set(2, x2, y1, 0, fU2, fV1);
		qd.Set(3, x2, y2, 0, fU2, fV2);
		qd.Render(GL_TRIANGLE_STRIP);
	}

	if (psp->GetID() == PSP_WEAPON && s3d::Stereo3DMode::getCurrentMode().RenderPlayerSpritesCrossed())
	{
		if (r_PlayerSprites3DMode == BACK_ONLY)
			return;

		FState* spawn = wi->FindState(NAME_Spawn);

		lump = sprites[spawn->sprite].GetSpriteFrame(0, 0, 0., &mirror);
		if (!lump.isValid()) return;

		tex = FMaterial::ValidateTexture(lump, true, false);
		if (!tex) return;

		gl_RenderState.SetMaterial(tex, CLAMP_XY_NOMIP, 0, OverrideShader, alphatexture);

		float z1 = 0.0f;
		float z2 = (y2 - y1) * MIN(3, tex->GetWidth() / tex->GetHeight());

		if (!(mirror) != !(psp->Flags & PSPF_FLIP))
		{
			fU2 = tex->GetSpriteUL();
			fV1 = tex->GetSpriteVT();
			fU1 = tex->GetSpriteUR();
			fV2 = tex->GetSpriteVB();
		}
		else
		{
			fU1 = tex->GetSpriteUL();
			fV1 = tex->GetSpriteVT();
			fU2 = tex->GetSpriteUR();
			fV2 = tex->GetSpriteVB();
		}

		if (r_PlayerSprites3DMode == FAT_ITEM)
		{
			x1 = vw / 2 + (x1 - vw / 2) * gl_fatItemWidth;
			x2 = vw / 2 + (x2 - vw / 2) * gl_fatItemWidth;

			float inc = (x2 - x1) / 12.0f;
			for (float x = x1; x <= x2; x += inc)
			{
				FQuadDrawer qd2;
				qd2.Set(0, x, y1, -z1, fU1, fV1);
				qd2.Set(1, x, y2, -z1, fU1, fV2);
				qd2.Set(2, x, y1, -z2, fU2, fV1);
				qd2.Set(3, x, y2, -z2, fU2, fV2);
				qd2.Render(GL_TRIANGLE_STRIP);
			}
		}
		else
		{
			float crossAt;
			if (r_PlayerSprites3DMode == ITEM_ONLY)
			{
				crossAt = 0.0f;
				sy = 0.0f;
			}
			else
			{
				sy = y2 - y1;
				crossAt = sy * 0.25f;
			}

			y1 -= crossAt;
			y2 -= crossAt;

			FQuadDrawer qd2;
			qd2.Set(0, vw / 2 - crossAt, y1, -z1, fU1, fV1);
			qd2.Set(1, vw / 2 + sy / 2, y2, -z1, fU1, fV2);
			qd2.Set(2, vw / 2 - crossAt, y1, -z2, fU2, fV1);
			qd2.Set(3, vw / 2 + sy / 2, y2, -z2, fU2, fV2);
			qd2.Render(GL_TRIANGLE_STRIP);

			FQuadDrawer qd3;
			qd3.Set(0, vw / 2 + crossAt, y1, -z1, fU1, fV1);
			qd3.Set(1, vw / 2 - sy / 2, y2, -z1, fU1, fV2);
			qd3.Set(2, vw / 2 + crossAt, y1, -z2, fU2, fV1);
			qd3.Set(3, vw / 2 - sy / 2, y2, -z2, fU2, fV2);
			qd3.Render(GL_TRIANGLE_STRIP);
		}
	}

	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.5f);
}

//==========================================================================
//
//
//
//==========================================================================

static bool isBright(DPSprite *psp)
{
	if (psp != nullptr && psp->GetState() != nullptr)
	{
		bool disablefullbright = false;
		FTextureID lump = sprites[psp->GetSprite()].GetSpriteFrame(psp->GetFrame(), 0, 0., nullptr);
		if (lump.isValid())
		{
			FMaterial * tex = FMaterial::ValidateTexture(lump, false, false);
			if (tex)
				disablefullbright = tex->tex->gl_info.bDisableFullbright;
		}
		return psp->GetState()->GetFullbright() && !disablefullbright;
	}
	return false;
}

void GLSceneDrawer::SetupWeaponLight()
{
	weapondynlightindex.Clear();

	AActor *camera = r_viewpoint.camera;
	AActor * playermo = players[consoleplayer].camera;
	player_t * player = playermo->player;

	// this is the same as in DrawPlayerSprites below
	if (!player ||
		!r_drawplayersprites ||
		!camera->player ||
		(player->cheats & CF_CHASECAM) ||
		(r_deathcamera && camera->health <= 0))
		return;

	for (DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		if (psp->GetState() != nullptr)
		{
			// set the lighting parameters
			if (vr_dynlights && GLRenderer->mLightCount && FixedColormap == CM_DEFAULT && gl_light_sprites)
			{
				FSpriteModelFrame *smf = playermo->player->ReadyWeapon ? FindModelFrame(playermo->player->ReadyWeapon->GetClass(), psp->GetState()->sprite, psp->GetState()->GetFrame(), false) : nullptr;
				if (smf)
				{
					weapondynlightindex[psp] = gl_SetDynModelLight(playermo, -1);
				}
			}
		}
	}
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void GLSceneDrawer::DrawPlayerSprites(sector_t * viewsector, bool hudModelStep)
{
	bool brightflash = false;
	unsigned int i;
	int lightlevel=0;
	FColormap cm;
	sector_t * fakesec;
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	AActor *camera = r_viewpoint.camera;

	// this is the same as the software renderer
	if (!player ||
		!r_drawplayersprites ||
		!camera->player ||
		(player->cheats & CF_CHASECAM) || 
		(r_deathcamera && camera->health <= 0) ||
		viewsector == nullptr)
		return;

	if (!hudModelStep)
	{
		s3d::Stereo3DMode::getCurrentMode().AdjustPlayerSprites();
	}

	float bobx, boby, wx, wy;
	DPSprite *weapon;

	P_BobWeapon(camera->player, &bobx, &boby, r_viewpoint.TicFrac);

	// Interpolate the main weapon layer once so as to be able to add it to other layers.
	if ((weapon = camera->player->FindPSprite(PSP_WEAPON)) != nullptr)
	{
		if (weapon->firstTic)
		{
			wx = weapon->x;
			wy = weapon->y;
		}
		else
		{
			wx = weapon->oldx + (weapon->x - weapon->oldx) * r_viewpoint.TicFrac;
			wy = weapon->oldy + (weapon->y - weapon->oldy) * r_viewpoint.TicFrac;
		}
	}
	else
	{
		wx = 0;
		wy = 0;
	}

	if (FixedColormap) 
	{
		lightlevel=255;
		cm.Clear();
		fakesec = viewsector;
	}
	else
	{
		fakesec = gl_FakeFlat(viewsector, in_area, false);

		// calculate light level for weapon sprites
		lightlevel = gl_ClampLight(fakesec->lightlevel);

		// calculate colormap for weapon sprites
		if (viewsector->e->XFloor.ffloors.Size() && !(level.flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING))
		{
			TArray<lightlist_t> & lightlist = viewsector->e->XFloor.lightlist;
			for(i=0;i<lightlist.Size();i++)
			{
				double lightbottom;

				if (i<lightlist.Size()-1) 
				{
					lightbottom=lightlist[i+1].plane.ZatPoint(r_viewpoint.Pos);
				}
				else 
				{
					lightbottom=viewsector->floorplane.ZatPoint(r_viewpoint.Pos);
				}

				if (lightbottom<player->viewz) 
				{
					cm = lightlist[i].extra_colormap;
					lightlevel = gl_ClampLight(*lightlist[i].p_lightlevel);
					break;
				}
			}
		}
		else 
		{
			cm=fakesec->Colormap;
			if (level.flags3 & LEVEL3_NOCOLOREDSPRITELIGHTING) cm.ClearColor();
		}

		lightlevel = gl_CalcLightLevel(lightlevel, getExtraLight(), true, 0);

		if (glset.lightmode >= 8 || lightlevel < 92)
		{
			// Korshun: the way based on max possible light level for sector like in software renderer.
			float min_L = 36.0 / 31.0 - ((lightlevel / 255.0) * (63.0 / 31.0)); // Lightlevel in range 0-63
			if (min_L < 0)
				min_L = 0;
			else if (min_L > 1.0)
				min_L = 1.0;

			lightlevel = (1.0 - min_L) * 255;
		}
		else
		{
			lightlevel = (2 * lightlevel + 255) / 3;
		}
		lightlevel = gl_CheckSpriteGlow(viewsector, lightlevel, playermo->Pos());

	}
	
	// Korshun: fullbright fog in opengl, render weapon sprites fullbright (but don't cancel out the light color!)
	if (glset.brightfog && ((level.flags&LEVEL_HASFADETABLE) || cm.FadeColor != 0))
	{
		lightlevel = 255;
	}

	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);

	// hack alert! Rather than changing everything in the underlying lighting code let's just temporarily change
	// light mode here to draw the weapon sprite.
	int oldlightmode = glset.lightmode;
	if (glset.lightmode >= 8) glset.lightmode = 2;

	for(DPSprite *psp = player->psprites; psp != nullptr && psp->GetID() < PSP_TARGETCENTER; psp = psp->GetNext())
	{
		auto rs = psp->GetRenderStyle(playermo->RenderStyle, playermo->Alpha);

		visstyle_t vis;
		float trans = 0.f;

		vis.RenderStyle = STYLE_Count;
		vis.Alpha = rs.second;
		vis.Invert = false;
		playermo->AlterWeaponSprite(&vis);

		FRenderStyle RenderStyle;

		if (!(psp->Flags & PSPF_FORCEALPHA)) trans = vis.Alpha;

		if (vis.RenderStyle != STYLE_Count && !(psp->Flags & PSPF_FORCESTYLE))
		{
			RenderStyle = vis.RenderStyle;
		}
		else
		{
			RenderStyle = rs.first;
		}
		if (RenderStyle.BlendOp == STYLEOP_None) continue;

		if (vis.Invert)
		{
			// this only happens for Strife's inverted weapon sprite
			RenderStyle.Flags |= STYLEF_InvertSource;
		}

		// Set the render parameters

		int OverrideShader = -1;
		if (RenderStyle.BlendOp == STYLEOP_Fuzz)
		{
			if (gl_fuzztype != 0 && !gl.legacyMode)
			{
				// Todo: implement shader selection here
				RenderStyle = LegacyRenderStyles[STYLE_Translucent];
				OverrideShader = SHADER_NoTexture + gl_fuzztype;
				trans = 0.99f;	// trans may not be 1 here
			}
			else
			{
				RenderStyle.BlendOp = STYLEOP_Shadow;
			}
		}

		gl_SetRenderStyle(RenderStyle, false, false);

		if (RenderStyle.Flags & STYLEF_TransSoulsAlpha)
		{
			trans = transsouls;
		}
		else if (RenderStyle.Flags & STYLEF_Alpha1)
		{
			trans = 1.f;
		}
		else if (trans == 0.f)
		{
			trans = vis.Alpha;
		}

		PalEntry ThingColor = (playermo->RenderStyle.Flags & STYLEF_ColorIsFixed) ? playermo->fillcolor : 0xffffff;
		ThingColor.a = 255;

		// now draw the different layers of the weapon.
		// For stencil render styles brightmaps need to be disabled.
		gl_RenderState.EnableBrightmap(!(RenderStyle.Flags & STYLEF_ColorIsFixed));

		const bool bright = isBright(psp);
		const PalEntry finalcol = bright
			? ThingColor
			: ThingColor.Modulate(viewsector->SpecialColors[sector_t::sprites]);
		gl_RenderState.SetObjectColor(finalcol);

		if (psp->GetState() != nullptr) 
		{
			FColormap cmc = cm;
			int ll = lightlevel;
			if (bright)
			{
				if (fakesec == viewsector || in_area != area_below)	
				{
					cmc.MakeWhite();
				}
				else
				{
					// under water areas keep most of their color for fullbright objects
					cmc.LightColor.r = (3 * cmc.LightColor.r + 0xff) / 4;
					cmc.LightColor.g = (3*cmc.LightColor.g + 0xff)/4;
					cmc.LightColor.b = (3*cmc.LightColor.b + 0xff)/4;
				}
				ll = 255;
			}
			// set the lighting parameters
			if (RenderStyle.BlendOp == STYLEOP_Shadow)
			{
				gl_RenderState.SetColor(0.2f, 0.2f, 0.2f, 0.33f, cmc.Desaturation);
			}
			else
			{
				if (vr_dynlights && GLRenderer->mLightCount && FixedColormap == CM_DEFAULT && gl_light_sprites)
				{
					FSpriteModelFrame *smf = playermo->player->ReadyWeapon ? FindModelFrame(playermo->player->ReadyWeapon->GetClass(), psp->GetSprite(), psp->GetState()->GetFrame(), false) : nullptr;
					if (smf)
						gl_SetDynModelLight(playermo, weapondynlightindex[psp]);
					else
						gl_SetDynSpriteLight(playermo, NULL);
				}
				SetColor(ll, 0, cmc, trans, true);
			}
			if (playermo->Sector)
			{
				gl_RenderState.SetAddColor(playermo->Sector->AdditiveColors[sector_t::sprites] | 0xff000000);
			}
			else
			{
				gl_RenderState.SetAddColor(0);
			}

			if (psp->firstTic)
			{ // Can't interpolate the first tic.
				psp->firstTic = false;
				psp->oldx = psp->x;
				psp->oldy = psp->y;
			}

			float sx = psp->oldx + (psp->x - psp->oldx) * r_viewpoint.TicFrac;
			float sy = psp->oldy + (psp->y - psp->oldy) * r_viewpoint.TicFrac;

			if (psp->Flags & PSPF_ADDBOB)
			{
				sx += (psp->Flags & PSPF_MIRROR) ? -bobx : bobx;
				sy += boby;
			}

			if (psp->Flags & PSPF_ADDWEAPON && psp->GetID() != PSP_WEAPON)
			{
				sx += wx;
				sy += wy;
			}


			DrawPSprite(player, psp, sx, sy, hudModelStep, OverrideShader, !!(RenderStyle.Flags & STYLEF_RedIsAlpha));
		}
	}

	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.SetAddColor(0);
	gl_RenderState.SetDynLight(0, 0, 0);
	gl_RenderState.EnableBrightmap(false);
	glset.lightmode = oldlightmode;

	if (!hudModelStep)
	{
		s3d::Stereo3DMode::getCurrentMode().UnAdjustPlayerSprites();
	}
}

//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void GLSceneDrawer::DrawTargeterSprites()
{
	AActor * playermo=players[consoleplayer].camera;
	player_t * player=playermo->player;
	
	if(!player || playermo->renderflags&RF_INVISIBLE || !r_drawplayersprites ||
		GLRenderer->mViewActor!=playermo) return;

	gl_RenderState.EnableBrightmap(false);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_RenderState.AlphaFunc(GL_GEQUAL,gl_mask_sprite_threshold);
	gl_RenderState.BlendEquation(GL_FUNC_ADD);
	gl_RenderState.ResetColor();
	gl_RenderState.SetTextureMode(TM_MODULATE);

	// The Targeter's sprites are always drawn normally.
	for (DPSprite *psp = player->FindPSprite(PSP_TARGETCENTER); psp != nullptr; psp = psp->GetNext())
	{
		if (psp->GetState() != nullptr) DrawPSprite(player, psp, psp->x, psp->y, false, 0, false);
	}
}
