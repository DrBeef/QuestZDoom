//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Ticker.
//
//-----------------------------------------------------------------------------

#include <float.h>
#include "p_local.h"
#include "p_effect.h"
#include "c_console.h"
#include "b_bot.h"
#include "s_sound.h"
#include "doomstat.h"
#include "sbar.h"
#include "r_data/r_interpolate.h"
#include "i_sound.h"
#include "d_player.h"
#include "g_level.h"
#include "r_renderer.h"
#include "r_utility.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "events.h"
#include "actorinlines.h"

extern gamestate_t wipegamestate;

//==========================================================================
//
// P_CheckTickerPaused
//
// Returns true if the ticker should be paused. In that case, it also
// pauses sound effects and possibly music. If the ticker should not be
// paused, then it returns false but does not unpause anything.
//
//==========================================================================

bool P_CheckTickerPaused ()
{
	// pause if in menu or console and at least one tic has been run
	if ( !netgame
		 && gamestate != GS_TITLELEVEL
		 && ((menuactive != MENU_Off && menuactive != MENU_OnNoPause) ||
			 ConsoleState == c_down || ConsoleState == c_falling)
		 && !demoplayback
		 && !demorecording
		 && players[consoleplayer].viewz != NO_VALUE
		 && wipegamestate == gamestate)
	{
		S_PauseSound (!(level.flags2 & LEVEL2_PAUSE_MUSIC_IN_MENUS), false);
		return true;
	}
	return false;
}

//
// P_Ticker
//
void P_Ticker (void)
{
	int i;

	interpolator.UpdateInterpolations ();
	r_NoInterpolate = true;

	if (!demoplayback)
	{
		// This is a separate slot from the wipe in D_Display(), because this
		// is delayed slightly due to latency. (Even on a singleplayer game!)
//		GSnd->SetSfxPaused(!!playerswiping, 2);
	}

	// run the tic
	if (paused || P_CheckTickerPaused())
	{
		// This must run even when the game is paused to catch changes from netevents before the frame is rendered.
		TThinkerIterator<AActor> it;
		AActor* ac;

		while ((ac = it.Next()))
		{
			if (ac->flags8 & MF8_RECREATELIGHTS)
			{
				ac->flags8 &= ~MF8_RECREATELIGHTS;
				ac->SetDynamicLights();
			}
		}
		return;
	}

	DPSprite::NewTick();

	// [RH] Frozen mode is only changed every 4 tics, to make it work with A_Tracer().
	if ((level.maptime & 3) == 0)
	{
		if (bglobal.changefreeze)
		{
			level.frozenstate ^= 2;
			bglobal.freeze ^= 1;
			bglobal.changefreeze = 0;
		}
	}

	// [BC] Do a quick check to see if anyone has the freeze time power. If they do,
	// then don't resume the sound, since one of the effects of that power is to shut
	// off the music.
	for (i = 0; i < MAXPLAYERS; i++ )
	{
		if (playeringame[i] && players[i].timefreezer != 0)
			break;
	}

	if ( i == MAXPLAYERS )
		S_ResumeSound (false);

	P_ResetSightCounters (false);
	R_ClearInterpolationPath();

	// Reset all actor interpolations for all actors before the current thinking turn so that indirect actor movement gets properly interpolated.
	TThinkerIterator<AActor> it;
	AActor *ac;

	while ((ac = it.Next()))
	{
		ac->ClearInterpolation();
	}

	// Since things will be moving, it's okay to interpolate them in the renderer.
	r_NoInterpolate = false;

	P_ThinkParticles();	// [RH] make the particles think

	for (i = 0; i<MAXPLAYERS; i++)
		if (playeringame[i])
			P_PlayerThink (&players[i]);

	// [ZZ] call the WorldTick hook
	E_WorldTick();
	StatusBar->CallTick ();		// [RH] moved this here
	level.Tick ();			// [RH] let the level tick
	DThinker::RunThinkers ();

	//if added by MC: Freeze mode.
	if (!level.isFrozen())
	{
		P_UpdateSpecials ();
	}

	it = TThinkerIterator<AActor>();

	// Set dynamic lights at the end of the tick, so that this catches all changes being made through the last frame.
	while (ac = it.Next())
	{
		if (ac->flags8 & MF8_RECREATELIGHTS && Renderer != nullptr)
		{
			ac->flags8 &= ~MF8_RECREATELIGHTS;
			ac->SetDynamicLights();
		}
		// This was merged from P_RunEffects to eliminate the costly duplicate ThinkerIterator loop.
		// [RH] Run particle effects
		if (players[consoleplayer].camera != nullptr && !level.isFrozen())
		{
			int pnum = players[consoleplayer].camera->Sector->Index() * level.sectors.Size();
			if ((ac->effects || ac->fountaincolor))
			{
				// Only run the effect if the actor is potentially visible
				int rnum = pnum + ac->Sector->Index();
				if (level.rejectmatrix.Size() == 0 || !(level.rejectmatrix[rnum>>3] & (1 << (rnum & 7))))
					P_RunEffect(ac, ac->effects);
			}
		}
	}

	// for par times
	level.time++;
	level.maptime++;
	level.totaltime++;
	if (players[consoleplayer].mo != NULL) {
		if (players[consoleplayer].mo->Vel.Length() > level.max_velocity) { level.max_velocity = players[consoleplayer].mo->Vel.Length(); }
		level.avg_velocity += (players[consoleplayer].mo->Vel.Length() - level.avg_velocity) / level.maptime;
	}
}
