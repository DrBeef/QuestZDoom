//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
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
//		Implements special effects:
//		Texture animation, height or lighting changes
//		 according to adjacent sectors, respective
//		 utility functions, etc.
//		Line Tag handling. Line and Sector triggers.
//		Implements donut linedef triggers
//		Initializes and implements BOOM linedef triggers for
//			Scrollers/Conveyors
//			Friction
//			Wind/Current
//
//-----------------------------------------------------------------------------

/* For code that originates from ZDoom the following applies:
**
**---------------------------------------------------------------------------
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <stdlib.h>

#include "templates.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "g_level.h"
#include "gstrings.h"
#include "events.h"

#include "i_system.h"
#include "m_argv.h"
#include "m_random.h"
#include "m_bbox.h"
#include "w_wad.h"

#include "p_local.h"
#include "p_spec.h"
#include "p_blockmap.h"
#include "p_lnspec.h"
#include "p_terrain.h"
#include "p_acs.h"
#include "p_3dmidtex.h"

#include "g_game.h"

#include "s_sound.h"
#include "sc_man.h"
#include "gi.h"
#include "statnums.h"
#include "g_level.h"
#include "v_font.h"
#include "a_sharedglobal.h"
#include "serializer.h"
#include "a_keys.h"
#include "c_dispatch.h"
#include "r_sky.h"
#include "d_player.h"
#include "p_maputl.h"
#include "p_blockmap.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#ifndef NO_EDATA
#include "edata.h"
#endif
#include "vm.h"

// State.
#include "r_state.h"

#include "c_console.h"

#include "r_data/r_interpolate.h"

static FRandom pr_playerinspecialsector ("PlayerInSpecialSector");

EXTERN_CVAR(Bool, cl_predict_specials)


// killough 3/7/98: Initialize generalized scrolling
void P_SpawnScrollers();
static void P_SpawnFriction ();		// phares 3/16/98
void P_SpawnPushers ();		// phares 3/20/98


// [RH] Check dmflags for noexit and respond accordingly
bool CheckIfExitIsGood (AActor *self, level_info_t *info)
{
	cluster_info_t *cluster;

	// The world can always exit itself.
	if (self == NULL)
		return true;

	// We must kill all monsters to exit the level.
	if ((dmflags2 & DF2_KILL_MONSTERS) && level.killed_monsters != level.total_monsters)
		return false;

	// Is this a deathmatch game and we're not allowed to exit?
	if ((deathmatch || alwaysapplydmflags) && (dmflags & DF_NO_EXIT))
	{
		P_DamageMobj (self, self, self, TELEFRAG_DAMAGE, NAME_Exit);
		return false;
	}
	// Is this a singleplayer game and the next map is part of the same hub and we're dead?
	if (self->health <= 0 &&
		!multiplayer &&
		info != NULL &&
		info->cluster == level.cluster &&
		(cluster = FindClusterInfo(level.cluster)) != NULL &&
		cluster->flags & CLUSTER_HUB)
	{
		return false;
	}
	if (deathmatch && gameaction != ga_completed)
	{
		Printf ("%s exited the level.\n", self->player->userinfo.GetName());
	}
	return true;
}


//
// UTILITIES
//

//============================================================================
//
// P_ActivateLine
//
//============================================================================

bool P_ActivateLine (line_t *line, AActor *mo, int side, int activationType, DVector3 *optpos)
{
	int lineActivation;
	INTBOOL repeat;
	INTBOOL buttonSuccess;
	uint8_t special;

	if (!P_TestActivateLine (line, mo, side, activationType, optpos))
	{
		return false;
	}

	// [MK] Use WorldLinePreActivated to decide if activation should continue
	bool shouldactivate = true;
	E_WorldLinePreActivated(line, mo, activationType, &shouldactivate);
	if ( !shouldactivate ) return false;

	bool remote = (line->special != 7 && line->special != 8 && (line->special < 11 || line->special > 14));
	if (line->locknumber > 0 && !P_CheckKeys (mo, line->locknumber, remote)) return false;

	lineActivation = line->activation;
	repeat = line->flags & ML_REPEAT_SPECIAL;
	buttonSuccess = false;
	buttonSuccess = P_ExecuteSpecial(line->special,
					line, mo, side == 1, line->args[0],
					line->args[1], line->args[2],
					line->args[3], line->args[4]);

	// [MK] Fire up WorldLineActivated
	if ( buttonSuccess ) E_WorldLineActivated(line, mo, activationType);

	special = line->special;
	if (!repeat && buttonSuccess)
	{ // clear the special on non-retriggerable lines
		line->special = 0;
	}

	if (buttonSuccess)
	{
		if (activationType == SPAC_Use || activationType == SPAC_Impact || activationType == SPAC_Push)
		{
			P_ChangeSwitchTexture (line->sidedef[0], repeat, special);
		}
	}
	// some old WADs use this method to create walls that change the texture when shot.
	else if (activationType == SPAC_Impact &&					// only for shootable triggers
		(level.flags2 & LEVEL2_DUMMYSWITCHES) &&				// this is only a compatibility setting for an old hack!
		!repeat &&												// only non-repeatable triggers
		(special<Generic_Floor || special>Generic_Crusher) &&	// not for Boom's generalized linedefs
		special &&												// not for lines without a special
		tagManager.LineHasID(line, line->args[0]) &&							// Safety check: exclude edited UDMF linedefs or ones that don't map the tag to args[0]
		line->args[0] &&										// only if there's a tag (which is stored in the first arg)
		P_FindFirstSectorFromTag (line->args[0]) == -1)			// only if no sector is tagged to this linedef
	{
		P_ChangeSwitchTexture (line->sidedef[0], repeat, special);
		line->special = 0;
	}
// end of changed code
	if (developer >= DMSG_SPAMMY && buttonSuccess)
	{
		Printf ("Line special %d activated on line %i\n", special, line->Index());
	}
	return true;
}

DEFINE_ACTION_FUNCTION(_Line, Activate)
{
	PARAM_SELF_STRUCT_PROLOGUE(line_t);
	PARAM_POINTER(mo, AActor);
	PARAM_INT(side);
	PARAM_INT(activationType);
	ACTION_RETURN_BOOL(P_ActivateLine(self, mo, side, activationType, NULL));
}

DEFINE_ACTION_FUNCTION(_Line, RemoteActivate)
{
	PARAM_SELF_STRUCT_PROLOGUE(line_t);
	PARAM_POINTER(mo, AActor);
	PARAM_INT(side);
	PARAM_INT(activationType);
	PARAM_FLOAT(optx);
	PARAM_FLOAT(opty);
	PARAM_FLOAT(optz);
	DVector3 optpos = DVector3(optx, opty, optz);
	ACTION_RETURN_BOOL(P_ActivateLine(self, mo, side, activationType, &optpos));
}

//============================================================================
//
// P_TestActivateLine
//
//============================================================================

bool P_TestActivateLine (line_t *line, AActor *mo, int side, int activationType, DVector3 *optpos)
{
 	int lineActivation = line->activation;

	if (line->flags & ML_FIRSTSIDEONLY && side == 1)
	{
		return false;
	}

	if (lineActivation & SPAC_UseThrough)
	{
		lineActivation |= SPAC_Use;
	}
	else if (line->special == Teleport &&
		(lineActivation & SPAC_Cross) &&
		activationType == SPAC_PCross &&
		mo != NULL &&
		mo->flags & MF_MISSILE)
	{ // Let missiles use regular player teleports
		lineActivation |= SPAC_PCross;
	}
	// BOOM's generalized line types that allow monster use can actually be
	// activated by anything except projectiles.
	if (lineActivation & SPAC_AnyCross)
	{
		lineActivation |= SPAC_Cross|SPAC_MCross;
	}
	if (activationType == SPAC_Use || activationType == SPAC_UseBack)
	{
		if (!P_CheckSwitchRange(mo, line, side, optpos))
		{
			return false;
		}
	}

	if (activationType == SPAC_Use && (lineActivation & SPAC_MUse) && !mo->player && mo->flags4 & MF4_CANUSEWALLS)
	{
		return true;
	}
	if (activationType == SPAC_Push && (lineActivation & SPAC_MPush) && !mo->player && mo->flags2 & MF2_PUSHWALL)
	{
		return true;
	}
	if ((lineActivation & activationType) == 0)
	{
		if (activationType != SPAC_MCross || lineActivation != SPAC_Cross)
		{ 
			return false;
		}
	}
	if (activationType == SPAC_AnyCross && (lineActivation & activationType))
	{
		return true;
	}
	if (mo && !mo->player &&
		!(mo->flags & MF_MISSILE) &&
		!(line->flags & ML_MONSTERSCANACTIVATE) &&
		(activationType != SPAC_MCross || (!(lineActivation & SPAC_MCross))))
	{ 
		// [RH] monsters' ability to activate this line depends on its type
		// In Hexen, only MCROSS lines could be activated by monsters. With
		// lax activation checks, monsters can also activate certain lines
		// even without them being marked as monster activate-able. This is
		// the default for non-Hexen maps in Hexen format.
		if (!(level.flags2 & LEVEL2_LAXMONSTERACTIVATION))
		{
			return false;
		}
		if ((activationType == SPAC_Use || activationType == SPAC_Push)
			&& (line->flags & ML_SECRET))
			return false;		// never open secret doors

		bool noway = true;

		switch (activationType)
		{
		case SPAC_Use:
		case SPAC_Push:
			switch (line->special)
			{
			case Door_Raise:
				if (line->args[0] == 0 && line->args[1] < 64)
					noway = false;
				break;
			case Teleport:
			case Teleport_NoFog:
				noway = false;
			}
			break;

		case SPAC_MCross:
			if (!(lineActivation & SPAC_MCross))
			{
				switch (line->special)
				{
				case Door_Raise:
					if (line->args[1] >= 64)
					{
						break;
					}
				case Teleport:
				case Teleport_NoFog:
				case Teleport_Line:
				case Plat_DownWaitUpStayLip:
				case Plat_DownWaitUpStay:
					noway = false;
				}
			}
			else noway = false;
			break;

		default:
			noway = false;
		}
		return !noway;
	}
	if (activationType == SPAC_MCross && !(lineActivation & SPAC_MCross) &&
		!(line->flags & ML_MONSTERSCANACTIVATE))
	{
		return false;
	}
	return true;
}

//============================================================================
//
// P_PredictLine
//
//============================================================================

bool P_PredictLine(line_t *line, AActor *mo, int side, int activationType)
{
	int lineActivation;
	INTBOOL buttonSuccess;
	uint8_t special;

	// Only predict a very specifc section of specials
	if (line->special != Teleport_Line &&
		line->special != Teleport)
	{
		return false;
	}

	if (!P_TestActivateLine(line, mo, side, activationType) || !cl_predict_specials)
	{
		return false;
	}

	if (line->locknumber > 0) return false;
	lineActivation = line->activation;
	buttonSuccess = false;
	buttonSuccess = P_ExecuteSpecial(line->special,
		line, mo, side == 1, line->args[0],
		line->args[1], line->args[2],
		line->args[3], line->args[4]);

	special = line->special;

	// end of changed code
	if (developer >= DMSG_SPAMMY && buttonSuccess)
	{
		Printf("Line special %d predicted on line %i\n", special, line->Index());
	}
	return true;
}

//
// P_PlayerInSpecialSector
// Called every tic frame
//	that the player origin is in a special sector
//
void P_PlayerInSpecialSector (player_t *player, sector_t * sector)
{
	if (sector == NULL)
	{
		// Falling, not all the way down yet?
		sector = player->mo->Sector;
		if (!player->mo->isAtZ(sector->LowestFloorAt(player->mo))
			&& !player->mo->waterlevel)
		{
			return;
		}
	}

	// Has hit ground.
	AActor *ironfeet;

	// [RH] Apply any customizable damage
	if (sector->damageamount > 0)
	{
		// Allow subclasses. Better would be to implement it as armor and let that reduce
		// the damage as part of the normal damage procedure. Unfortunately, I don't have
		// different damage types yet, so that's not happening for now.
		for (ironfeet = player->mo->Inventory; ironfeet != NULL; ironfeet = ironfeet->Inventory)
		{
			if (ironfeet->IsKindOf(NAME_PowerIronFeet))
				break;
		}

		if (sector->Flags & SECF_ENDGODMODE) player->cheats &= ~CF_GODMODE;
		if ((ironfeet == NULL || pr_playerinspecialsector() < sector->leakydamage))
		{
			if (sector->Flags & SECF_HAZARD)
			{
				player->hazardcount += sector->damageamount;
				player->hazardtype = sector->damagetype;
				player->hazardinterval = sector->damageinterval;
			}
			else if (level.time % sector->damageinterval == 0)
			{
				if (!(player->cheats & (CF_GODMODE|CF_GODMODE2))) P_DamageMobj(player->mo, NULL, NULL, sector->damageamount, sector->damagetype);
				if ((sector->Flags & SECF_ENDLEVEL) && player->health <= 10 && (!deathmatch || !(dmflags & DF_NO_EXIT)))
				{
					G_ExitLevel(0, false);
				}
				if (sector->Flags & SECF_DMGTERRAINFX)
				{
					P_HitWater(player->mo, player->mo->Sector, player->mo->Pos(), false, true, true);
				}
			}
		}
	}
	else if (sector->damageamount < 0)
	{
		if (level.time % sector->damageinterval == 0)
		{
			P_GiveBody(player->mo, -sector->damageamount, 100);
		}
	}

	if (sector->isSecret())
	{
		sector->ClearSecret();
		P_GiveSecret(player->mo, true, true, sector->Index());
	}
}

//============================================================================
//
// P_SectorDamage
//
//============================================================================

static void DoSectorDamage(AActor *actor, sector_t *sec, int amount, FName type, PClassActor *protectClass, int flags)
{
	if (!(actor->flags & MF_SHOOTABLE))
		return;

	if (!(flags & DAMAGE_NONPLAYERS) && actor->player == NULL)
		return;

	if (!(flags & DAMAGE_PLAYERS) && actor->player != NULL)
		return;

	if (!(flags & DAMAGE_IN_AIR) && !actor->isAtZ(sec->floorplane.ZatPoint(actor)) && !actor->waterlevel)
		return;

	if (protectClass != NULL)
	{
		if (actor->FindInventory(protectClass, !!(flags & DAMAGE_SUBCLASSES_PROTECT)))
			return;
	}

	int dflags = (flags & DAMAGE_NO_ARMOR) ? DMG_NO_ARMOR : 0;
	P_DamageMobj (actor, NULL, NULL, amount, type, dflags);
}

void P_SectorDamage(int tag, int amount, FName type, PClassActor *protectClass, int flags)
{
	FSectorTagIterator itr(tag);
	int secnum;
	while ((secnum = itr.Next()) >= 0)
	{
		AActor *actor, *next;
		sector_t *sec = &level.sectors[secnum];

		// Do for actors in this sector.
		for (actor = sec->thinglist; actor != NULL; actor = next)
		{
			next = actor->snext;
			DoSectorDamage(actor, sec, amount, type, protectClass, flags);
		}
		// If this is a 3D floor control sector, also do for anything in/on
		// those 3D floors.
		for (unsigned i = 0; i < sec->e->XFloor.attached.Size(); ++i)
		{
			sector_t *sec2 = sec->e->XFloor.attached[i];

			for (actor = sec2->thinglist; actor != NULL; actor = next)
			{
				next = actor->snext;
				// Only affect actors touching the 3D floor
				double z1 = sec->floorplane.ZatPoint(actor);
				double z2 = sec->ceilingplane.ZatPoint(actor);
				if (z2 < z1)
				{
					// Account for Vavoom-style 3D floors
					double zz = z1;
					z1 = z2;
					z2 = zz;
				}
				if (actor->Top() > z1)
				{
					// If DAMAGE_IN_AIR is used, anything not beneath the 3D floor will be
					// damaged (so, anything touching it or above it). Other 3D floors between
					// the actor and this one will not stop this effect.
					if ((flags & DAMAGE_IN_AIR) || !actor->isAbove(z2))
					{
						// Here we pass the DAMAGE_IN_AIR flag to disable the floor check, since it
						// only works with the real sector's floor. We did the appropriate height checks
						// for 3D floors already.
						DoSectorDamage(actor, NULL, amount, type, protectClass, flags | DAMAGE_IN_AIR);
					}
				}
			}
		}
	}
}

//============================================================================
//
// P_GiveSecret
//
//============================================================================

CVAR(Bool, showsecretsector, false, 0)
CVAR(Bool, cl_showsecretmessage, true, CVAR_ARCHIVE)

void P_GiveSecret(AActor *actor, bool printmessage, bool playsound, int sectornum)
{
	if (actor != NULL)
	{
		if (actor->player != NULL)
		{
			actor->player->secretcount++;
		}
		int retval = 1;
		IFVIRTUALPTR(actor, AActor, OnGiveSecret)
		{
			VMValue params[] = { actor, printmessage, playsound };
			VMReturn ret;
			ret.IntAt(&retval);
			VMCall(func, params, countof(params), &ret, 1);
		}
		if (retval && cl_showsecretmessage && actor->CheckLocalView())
		{
			if (printmessage)
			{
				if (!showsecretsector || sectornum < 0) C_MidPrint(SmallFont, GStrings["SECRETMESSAGE"]);
				else
				{
					FString s = GStrings["SECRETMESSAGE"];
					s.AppendFormat(" (Sector %d)", sectornum);
					C_MidPrint(SmallFont, s);
				}
			}
			if (playsound) S_Sound (CHAN_AUTO | CHAN_UI, "misc/secret", 1, ATTN_NORM);
		}
	}
	level.found_secrets++;
}

DEFINE_ACTION_FUNCTION(FLevelLocals, GiveSecret)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(activator, AActor);
	PARAM_BOOL(printmessage);
	PARAM_BOOL(playsound);
	P_GiveSecret(activator, printmessage, playsound, -1);
	return 0;
}

//============================================================================
//
// P_PlayerOnSpecialFlat
//
//============================================================================

void P_PlayerOnSpecialFlat (player_t *player, int floorType)
{
	if (Terrains[floorType].DamageAmount &&
		!(level.time & Terrains[floorType].DamageTimeMask))
	{
		AActor *ironfeet = NULL;

		if (Terrains[floorType].AllowProtection)
		{
			auto pitype = PClass::FindActor(NAME_PowerIronFeet);
			for (ironfeet = player->mo->Inventory; ironfeet != NULL; ironfeet = ironfeet->Inventory)
			{
				if (ironfeet->IsKindOf (pitype))
					break;
			}
		}

		int damage = 0;
		if (ironfeet == NULL)
		{
			damage = P_DamageMobj (player->mo, NULL, NULL, Terrains[floorType].DamageAmount,
				Terrains[floorType].DamageMOD);
		}
		if (damage > 0 && Terrains[floorType].Splash != -1)
		{
			S_Sound (player->mo, CHAN_AUTO,
				Splashes[Terrains[floorType].Splash].NormalSplashSound, 1,
				ATTN_IDLE);
		}
	}
}



//
// P_UpdateSpecials
// Animate planes, scroll walls, etc.
//
EXTERN_CVAR (Float, timelimit)

void P_UpdateSpecials ()
{
	// LEVEL TIMER
	if (deathmatch && timelimit)
	{
		if (level.maptime >= (int)(timelimit * TICRATE * 60))
		{
			Printf ("%s\n", GStrings("TXT_TIMELIMIT"));
			G_ExitLevel(0, false);
		}
	}
}



//
// SPECIAL SPAWNING
//

CUSTOM_CVAR (Bool, forcewater, false, CVAR_ARCHIVE|CVAR_SERVERINFO)
{
	if (gamestate == GS_LEVEL)
	{
		for (auto &sec : level.sectors)
		{
			sector_t *hsec = sec.GetHeightSec();
			if (hsec && !(hsec->MoreFlags & SECMF_UNDERWATER))
			{
				if (self)
				{
					hsec->MoreFlags |= SECMF_FORCEDUNDERWATER;
				}
				else
				{
					hsec->MoreFlags &= ~SECMF_FORCEDUNDERWATER;
				}
			}
		}
	}
}

class DLightTransfer : public DThinker
{
	DECLARE_CLASS (DLightTransfer, DThinker)

	DLightTransfer() {}
public:
	DLightTransfer (sector_t *srcSec, int target, bool copyFloor);
	void Serialize(FSerializer &arc);
	void Tick ();

protected:
	static void DoTransfer (int level, int target, bool floor);

	sector_t *Source;
	int TargetTag;
	bool CopyFloor;
	short LastLight;
};

IMPLEMENT_CLASS(DLightTransfer, false, false)

void DLightTransfer::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("lastlight", LastLight)
		("source", Source)
		("targettag", TargetTag)
		("copyfloor", CopyFloor);
}

DLightTransfer::DLightTransfer (sector_t *srcSec, int target, bool copyFloor)
{
	int secnum;

	Source = srcSec;
	TargetTag = target;
	CopyFloor = copyFloor;
	DoTransfer (LastLight = srcSec->lightlevel, target, copyFloor);

	if (copyFloor)
	{
		FSectorTagIterator itr(target);
		while ((secnum = itr.Next()) >= 0)
			level.sectors[secnum].ChangeFlags(sector_t::floor, 0, PLANEF_ABSLIGHTING);
	}
	else
	{
		FSectorTagIterator itr(target);
		while ((secnum = itr.Next()) >= 0)
			level.sectors[secnum].ChangeFlags(sector_t::ceiling, 0, PLANEF_ABSLIGHTING);
	}
	ChangeStatNum (STAT_LIGHTTRANSFER);
}

void DLightTransfer::Tick ()
{
	int light = Source->lightlevel;

	if (light != LastLight)
	{
		LastLight = light;
		DoTransfer (light, TargetTag, CopyFloor);
	}
}

void DLightTransfer::DoTransfer (int llevel, int target, bool floor)
{
	int secnum;

	if (floor)
	{
		FSectorTagIterator itr(target);
		while ((secnum = itr.Next()) >= 0)
			level.sectors[secnum].SetPlaneLight(sector_t::floor, llevel);
	}
	else
	{
		FSectorTagIterator itr(target);
		while ((secnum = itr.Next()) >= 0)
			level.sectors[secnum].SetPlaneLight(sector_t::ceiling, llevel);
	}
}


class DWallLightTransfer : public DThinker
{
	enum
	{
		WLF_SIDE1=1,
		WLF_SIDE2=2,
		WLF_NOFAKECONTRAST=4
	};

	DECLARE_CLASS (DWallLightTransfer, DThinker)
	DWallLightTransfer() {}
public:
	DWallLightTransfer (sector_t *srcSec, int target, uint8_t flags);
	void Serialize(FSerializer &arc);
	void Tick ();

protected:
	static void DoTransfer (short level, int target, uint8_t flags);

	sector_t *Source;
	int TargetID;
	short LastLight;
	uint8_t Flags;
};

IMPLEMENT_CLASS(DWallLightTransfer, false, false)

void DWallLightTransfer::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("lastlight", LastLight)
		("source", Source)
		("targetid", TargetID)
		("flags", Flags);
}

DWallLightTransfer::DWallLightTransfer (sector_t *srcSec, int target, uint8_t flags)
{
	int linenum;
	int wallflags;

	Source = srcSec;
	TargetID = target;
	Flags = flags;
	DoTransfer (LastLight = srcSec->GetLightLevel(), target, Flags);

	if (!(flags & WLF_NOFAKECONTRAST))
	{
		wallflags = WALLF_ABSLIGHTING;
	}
	else
	{
		wallflags = WALLF_ABSLIGHTING | WALLF_NOFAKECONTRAST;
	}

	FLineIdIterator itr(target);
	while ((linenum = itr.Next()) >= 0)
	{
		if (flags & WLF_SIDE1 && level.lines[linenum].sidedef[0] != NULL)
		{
			level.lines[linenum].sidedef[0]->Flags |= wallflags;
		}

		if (flags & WLF_SIDE2 && level.lines[linenum].sidedef[1] != NULL)
		{
			level.lines[linenum].sidedef[1]->Flags |= wallflags;
		}
	}
	ChangeStatNum(STAT_LIGHTTRANSFER);
}

void DWallLightTransfer::Tick ()
{
	short light = sector_t::ClampLight(Source->lightlevel);

	if (light != LastLight)
	{
		LastLight = light;
		DoTransfer (light, TargetID, Flags);
	}
}

void DWallLightTransfer::DoTransfer (short lightlevel, int target, uint8_t flags)
{
	int linenum;

	FLineIdIterator itr(target);
	while ((linenum = itr.Next()) >= 0)
	{
		line_t *line = &level.lines[linenum];

		if (flags & WLF_SIDE1 && line->sidedef[0] != NULL)
		{
			line->sidedef[0]->SetLight(lightlevel);
		}

		if (flags & WLF_SIDE2 && line->sidedef[1] != NULL)
		{
			line->sidedef[1]->SetLight(lightlevel);
		}
	}
}

//-----------------------------------------------------------------------------
//
// Portals
//
//-----------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Upper stacks go in the top sector. Lower stacks go in the bottom sector.

static void SetupFloorPortal (AActor *point)
{
	NActorIterator it (NAME_LowerStackLookOnly, point->tid);
	sector_t *Sector = point->Sector;
	auto skyv = it.Next();
	if (skyv != nullptr)
	{
		skyv->target = point;
		if (Sector->GetAlpha(sector_t::floor) == 1.)
			Sector->SetAlpha(sector_t::floor, clamp(point->args[0], 0, 255) / 255.);

		Sector->Portals[sector_t::floor] = P_GetStackPortal(skyv, sector_t::floor);
	}
}

static void SetupCeilingPortal (AActor *point)
{
	NActorIterator it (NAME_UpperStackLookOnly, point->tid);
	sector_t *Sector = point->Sector;
	auto skyv = it.Next();
	if (skyv != nullptr)
	{
		skyv->target = point;
		if (Sector->GetAlpha(sector_t::ceiling) == 1.)
			Sector->SetAlpha(sector_t::ceiling, clamp(point->args[0], 0, 255) / 255.);

		Sector->Portals[sector_t::ceiling] = P_GetStackPortal(skyv, sector_t::ceiling);
	}
}

void P_SetupPortals()
{
	TThinkerIterator<AActor> it("StackPoint");
	AActor *pt;
	TArray<AActor *> points;

	while ((pt = it.Next()))
	{
		FName nm = pt->GetClass()->TypeName;
		if (nm == NAME_UpperStackLookOnly)
		{
			SetupFloorPortal(pt);
		}
		else if (nm == NAME_LowerStackLookOnly)
		{
			SetupCeilingPortal(pt);
		}
		pt->special1 = 0;
		points.Push(pt);
	}
	// the semantics here are incredibly lax so the final setup can only be done once all portals have been created,
	// because later stackpoints will happily overwrite info in older ones, if there are multiple links.
	for (auto &s : level.sectorPortals)
	{
		if (s.mType == PORTS_STACKEDSECTORTHING && s.mSkybox)
		{
			for (auto &ss : level.sectorPortals)
			{
				if (ss.mType == PORTS_STACKEDSECTORTHING && ss.mSkybox == s.mSkybox->target)
				{
					s.mPartner = unsigned((&ss) - &level.sectorPortals[0]);
				}
			}
		}
	}
	// Now we can finally set the displacement and delete the stackpoint reference.
	for (auto &s : level.sectorPortals)
	{
		if (s.mType == PORTS_STACKEDSECTORTHING && s.mSkybox)
		{
			s.mDisplacement = s.mSkybox->Pos() - s.mSkybox->target->Pos();
			s.mSkybox = nullptr;
		}
	}
}

static void SetPortal(sector_t *sector, int plane, unsigned pnum, double alpha)
{
	// plane: 0=floor, 1=ceiling, 2=both
	if (plane > 0)
	{
		if (sector->GetPortalType(sector_t::ceiling) == PORTS_SKYVIEWPOINT)
		{
			sector->Portals[sector_t::ceiling] = pnum;
			if (sector->GetAlpha(sector_t::ceiling) == 1.)
				sector->SetAlpha(sector_t::ceiling, alpha);

			if (level.sectorPortals[pnum].mFlags & PORTSF_SKYFLATONLY)
				sector->SetTexture(sector_t::ceiling, skyflatnum);
		}
	}
	if (plane == 2 || plane == 0)
	{
		if (sector->GetPortalType(sector_t::floor) == PORTS_SKYVIEWPOINT)
		{
			sector->Portals[sector_t::floor] = pnum;
		}
		if (sector->GetAlpha(sector_t::floor) == 1.)
			sector->SetAlpha(sector_t::floor, alpha);

		if (level.sectorPortals[pnum].mFlags & PORTSF_SKYFLATONLY)
			sector->SetTexture(sector_t::floor, skyflatnum);
	}
}

static void CopyPortal(int sectortag, int plane, unsigned pnum, double alpha, bool tolines)
{
	int s;
	FSectorTagIterator itr(sectortag);
	while ((s = itr.Next()) >= 0)
	{
		SetPortal(&level.sectors[s], plane, pnum, alpha);
	}

	for (auto &line : level.lines)
	{
		// Check if this portal needs to be copied to other sectors
		// This must be done here to ensure that it gets done only after the portal is set up
		if (line.special == Sector_SetPortal &&
			line.args[1] == 1 &&
			(line.args[2] == plane || line.args[2] == 3) &&
			line.args[3] == sectortag)
		{
			if (line.args[0] == 0)
			{
				SetPortal(line.frontsector, plane, pnum, alpha);
			}
			else
			{
				FSectorTagIterator itr(line.args[0]);
				while ((s = itr.Next()) >= 0)
				{
					SetPortal(&level.sectors[s], plane, pnum, alpha);
				}
			}
		}
		if (tolines && line.special == Sector_SetPortal &&
			line.args[1] == 5 &&
			line.args[3] == sectortag)
		{
			if (line.args[0] == 0)
			{
				line.portaltransferred = pnum;
			}
			else
			{
				FLineIdIterator itr(line.args[0]);
				while ((s = itr.Next()) >= 0)
				{
					level.lines[s].portaltransferred = pnum;
				}
			}
		}
	}
}


void P_SpawnPortal(line_t *line, int sectortag, int plane, int bytealpha, int linked)
{
	if (plane < 0 || plane > 2 || (linked && plane == 2)) return;
	for (auto &oline : level.lines)
	{
		// We must look for the reference line with a linear search unless we want to waste the line ID for it
		// which is not a good idea.
		if (oline.special == Sector_SetPortal &&
			oline.args[0] == sectortag &&
			oline.args[1] == linked &&
			oline.args[2] == plane &&
			oline.args[3] == 1)
		{
			// beware of overflows.
			DVector2 pos1 = line->v1->fPos() + line->Delta() / 2;
			DVector2 pos2 = oline.v1->fPos() + oline.Delta() / 2;
			unsigned pnum = P_GetPortal(linked ? PORTS_LINKEDPORTAL : PORTS_PORTAL, plane, line->frontsector, oline.frontsector, pos2 - pos1);
			CopyPortal(sectortag, plane, pnum, bytealpha / 255., false);
			return;
		}
	}
}

// This searches the viewpoint's sector
// for a skybox line special, gets its tag and transfers the skybox to all tagged sectors.
void P_SpawnSkybox(AActor *origin)
{
	sector_t *Sector = origin->Sector;
	if (Sector == NULL)
	{
		Printf("Sector not initialized for SkyCamCompat\n");
		origin->Sector = Sector = P_PointInSector(origin->Pos());
	}
	if (Sector)
	{
		for(auto refline : Sector->Lines)
		{
			if (refline->special == Sector_SetPortal && refline->args[1] == 2)
			{
				// We found the setup linedef for this skybox, so let's use it for our init.
				unsigned pnum = P_GetSkyboxPortal(origin);
				CopyPortal(refline->args[0], refline->args[2], pnum, 0, true);
				return;
			}
		}
	}
}



//
// P_SetSectorDamage
//
// Sets damage properties for one sector. Allows combination of original specials with explicit use of the damage properties
//

static void P_SetupSectorDamage(sector_t *sector, int damage, int interval, int leakchance, FName type, int flags)
{
	// Only set if damage is not yet initialized. This ensures that UDMF takes precedence over sector specials.
	if (sector->damageamount == 0)
	{
		sector->damageamount = damage;
		sector->damageinterval = MAX(1, interval);
		sector->leakydamage = leakchance;
		sector->damagetype = type;
		sector->Flags = (sector->Flags & ~SECF_DAMAGEFLAGS) | (flags & SECF_DAMAGEFLAGS);
	}
}

//
// P_InitSectorSpecial
//
// Sets up everything derived from 'sector->special' for one sector
// ('fromload' is necessary to allow conversion upon savegame load.)
//
void P_SpawnLights(sector_t *sector);

void P_InitSectorSpecial(sector_t *sector, int special)
{
	// [RH] All secret sectors are marked with a BOOM-ish bitfield
	if (sector->special & SECRET_MASK)
	{
		sector->Flags |= SECF_SECRET | SECF_WASSECRET;
		level.total_secrets++;
	}
	if (sector->special & FRICTION_MASK)
	{
		sector->Flags |= SECF_FRICTION;
	}
	if (sector->special & PUSH_MASK)
	{
		sector->Flags |= SECF_PUSH;
	}
	if ((sector->special & DAMAGE_MASK) == 0x100)
	{
		P_SetupSectorDamage(sector, 5, 32, 0, NAME_Fire, 0);
	}
	else if ((sector->special & DAMAGE_MASK) == 0x200)
	{
		P_SetupSectorDamage(sector, 10, 32, 0, NAME_Slime, 0);
	}
	else if ((sector->special & DAMAGE_MASK) == 0x300)
	{
		P_SetupSectorDamage(sector, 20, 32, 5, NAME_Slime, 0);
	}
	sector->special &= 0xff;

	// [RH] Normal DOOM special or BOOM specialized?
	bool keepspecial = false;
	P_SpawnLights(sector);
	switch (sector->special)
	{
	case dLight_Strobe_Hurt:
		P_SetupSectorDamage(sector, 20, 32, 5, NAME_Slime, 0);
		break;

	case dDamage_Hellslime:
		P_SetupSectorDamage(sector, 10, 32, 0, NAME_Slime, 0);
		break;

	case dDamage_Nukage:
		P_SetupSectorDamage(sector, 5, 32, 0, NAME_Slime, 0);
		break;

	case dSector_DoorCloseIn30:
		Create<DDoor>(sector, DDoor::doorWaitClose, 2, 0, 0, 30 * TICRATE);
		break;
			
	case dDamage_End:
		P_SetupSectorDamage(sector, 20, 32, 256, NAME_None, SECF_ENDGODMODE|SECF_ENDLEVEL);
		break;

	case dSector_DoorRaiseIn5Mins:
		Create<DDoor> (sector, DDoor::doorWaitRaise, 2, TICRATE*30/7, 0, 5*60*TICRATE);
		break;

	case dFriction_Low:
		sector->friction = FRICTION_LOW;
		sector->movefactor = 0x269/65536.;
		sector->Flags |= SECF_FRICTION;
		break;

	case dDamage_SuperHellslime:
		P_SetupSectorDamage(sector, 20, 32, 5, NAME_Slime, 0);
		break;

	case dDamage_LavaWimpy:
		P_SetupSectorDamage(sector, 5, 16, 256, NAME_Fire, SECF_DMGTERRAINFX);
		break;

	case dDamage_LavaHefty:
		P_SetupSectorDamage(sector, 8, 16, 256, NAME_Fire, SECF_DMGTERRAINFX);
		break;

	case dScroll_EastLavaDamage:
		P_SetupSectorDamage(sector, 5, 16, 256, NAME_Fire, SECF_DMGTERRAINFX);
		P_CreateScroller(EScroll::sc_floor, -4., 0, -1, sector->Index(), 0);
		keepspecial = true;
		break;

	case hDamage_Sludge:
		P_SetupSectorDamage(sector, 4, 32, 0, NAME_Slime, 0);
		break;

	case sLight_Strobe_Hurt:
		P_SetupSectorDamage(sector, 5, 32, 0, NAME_Slime, 0);
		break;

	case sDamage_Hellslime:
		P_SetupSectorDamage(sector, 2, 32, 0, NAME_Slime, SECF_HAZARD);
		break;

	case Damage_InstantDeath:
		// Strife's instant death sector
		P_SetupSectorDamage(sector, TELEFRAG_DAMAGE, 1, 256, NAME_InstantDeath, 0);
		break;

	case sDamage_SuperHellslime:
		P_SetupSectorDamage(sector, 4, 32, 0, NAME_Slime, SECF_HAZARD);
		break;

	case Sector_Hidden:
		sector->MoreFlags |= SECMF_HIDDEN;
		break;

	case Sector_Heal:
		// CoD's healing sector
		P_SetupSectorDamage(sector, -1, 32, 0, NAME_None, 0);
		break;

	case Sky2:
		sector->sky = PL_SKYFLAT;
		break;

	default:
		if (sector->special >= Scroll_North_Slow &&
			sector->special <= Scroll_SouthWest_Fast)
		{ // Hexen scroll special
			static const int8_t hexenScrollies[24][2] =
			{
				{  0,  1 }, {  0,  2 }, {  0,  4 },
				{ -1,  0 }, { -2,  0 }, { -4,  0 },
				{  0, -1 }, {  0, -2 }, {  0, -4 },
				{  1,  0 }, {  2,  0 }, {  4,  0 },
				{  1,  1 }, {  2,  2 }, {  4,  4 },
				{ -1,  1 }, { -2,  2 }, { -4,  4 },
				{ -1, -1 }, { -2, -2 }, { -4, -4 },
				{  1, -1 }, {  2, -2 }, {  4, -4 }
			};

			
			int i = sector->special - Scroll_North_Slow;
			double dx = hexenScrollies[i][0] / 2.;
			double dy = hexenScrollies[i][1] / 2.;
			P_CreateScroller(EScroll::sc_floor, dx, dy, -1, sector->Index(), 0);
		}
		else if (sector->special >= Carry_East5 &&
					sector->special <= Carry_East35)
		{ // Heretic scroll special
			// Only east scrollers also scroll the texture
			P_CreateScroller(EScroll::sc_floor,
				-0.5 * (1 << ((sector->special & 0xff) - Carry_East5)),	0, -1, sector->Index(), 0);
		}
		keepspecial = true;
		break;
	}
	if (!keepspecial) sector->special = 0;
}

//
// P_SpawnSpecials
//
// After the map has been loaded, scan for specials that spawn thinkers
//

void P_SpawnSpecials (void)
{
	P_SetupPortals();

	for (auto &sec : level.sectors)
	{
		if (sec.special == 0)
			continue;

		P_InitSectorSpecial(&sec, sec.special);
	}

#ifndef NO_EDATA
	ProcessEDSectors();
#endif

	
	// Init other misc stuff

	P_SpawnScrollers(); // killough 3/7/98: Add generalized scrollers
	P_SpawnFriction();	// phares 3/12/98: New friction model using linedefs
	P_SpawnPushers();	// phares 3/20/98: New pusher model using linedefs

	TThinkerIterator<AActor> it2("SkyCamCompat");
	AActor *pt2;
	while ((pt2 = it2.Next()))
	{
		P_SpawnSkybox(pt2);
	}

	for (auto &line : level.lines)
	{
		switch (line.special)
		{
			int s;
			sector_t *sec;

		// killough 3/7/98:
		// support for drawn heights coming from different sector
		case Transfer_Heights:
			{
				sec = line.frontsector;
				
				if (line.args[1] & 2)
				{
					sec->MoreFlags |= SECMF_FAKEFLOORONLY;
				}
				if (line.args[1] & 4)
				{
					sec->MoreFlags |= SECMF_CLIPFAKEPLANES;
				}
				if (line.args[1] & 8)
				{
					sec->MoreFlags |= SECMF_UNDERWATER;
				}
				else if (forcewater)
				{
					sec->MoreFlags |= SECMF_FORCEDUNDERWATER;
				}
				if (line.args[1] & 16)
				{
					sec->MoreFlags |= SECMF_IGNOREHEIGHTSEC;
				}
				else level.HasHeightSecs = true;
				if (line.args[1] & 32)
				{
					sec->MoreFlags |= SECMF_NOFAKELIGHT;
				}
				FSectorTagIterator itr(line.args[0]);
				while ((s = itr.Next()) >= 0)
				{
					level.sectors[s].heightsec = sec;
					sec->e->FakeFloor.Sectors.Push(&level.sectors[s]);
					level.sectors[s].MoreFlags |= (sec->MoreFlags & SECMF_IGNOREHEIGHTSEC);	// copy this to the destination sector for easier checking.
					level.sectors[s].AdjustFloorClip();
				}
				break;
			}

		// killough 3/16/98: Add support for setting
		// floor lighting independently (e.g. lava)
		case Transfer_FloorLight:
			Create<DLightTransfer> (line.frontsector, line.args[0], true);
			break;

		// killough 4/11/98: Add support for setting
		// ceiling lighting independently
		case Transfer_CeilingLight:
			Create<DLightTransfer> (line.frontsector, line.args[0], false);
			break;

		// [Graf Zahl] Add support for setting lighting
		// per wall independently
		case Transfer_WallLight:
			Create<DWallLightTransfer> (line.frontsector, line.args[0], line.args[1]);
			break;

		case Sector_Attach3dMidtex:
			P_Attach3dMidtexLinesToSector(line.frontsector, line.args[0], line.args[1], !!line.args[2]);
			break;

		case Sector_SetLink:
			if (line.args[0] == 0)
			{
				P_AddSectorLinks(line.frontsector, line.args[1], line.args[2], line.args[3]);
			}
			break;

		case Sector_SetPortal:
			// arg 0 = sector tag
			// arg 1 = type
			//	- 0: normal (handled here)
			//	- 1: copy (handled by the portal they copy)
			//	- 2: EE-style skybox (handled by the camera object)
			//  - 3: EE-style flat portal (GZDoom HW renderer only for now)
			//  - 4: EE-style horizon portal (GZDoom HW renderer only for now)
			//  - 5: copy portal to line (GZDoom HW renderer only for now)
			//  - 6: linked portal
			//	other values reserved for later use
			// arg 2 = 0:floor, 1:ceiling, 2:both
			// arg 3 = 0: anchor, 1: reference line
			// arg 4 = for the anchor only: alpha
			if ((line.args[1] == 0 || line.args[1] == 6) && line.args[3] == 0)
			{
				P_SpawnPortal(&line, line.args[0], line.args[2], line.args[4], line.args[1]);
			}
			else if (line.args[1] == 3 || line.args[1] == 4)
			{
				unsigned pnum = P_GetPortal(line.args[1] == 3 ? PORTS_PLANE : PORTS_HORIZON, line.args[2], line.frontsector, NULL, { 0,0 });
				CopyPortal(line.args[0], line.args[2], pnum, 0, true);
			}
			break;

		case Line_SetPortal:
			P_SpawnLinePortal(&line);
			break;

		// [RH] ZDoom Static_Init settings
		case Static_Init:
			switch (line.args[1])
			{
			case Init_Gravity:
				{
					double grav = line.Delta().Length() / 100.;
					FSectorTagIterator itr(line.args[0]);
					while ((s = itr.Next()) >= 0)
						level.sectors[s].gravity = grav;
				}
				break;

			//case Init_Color:
			// handled in P_LoadSideDefs2()

			case Init_Damage:
				{
					int damage = int(line.Delta().Length());
					FSectorTagIterator itr(line.args[0]);
					while ((s = itr.Next()) >= 0)
					{
						sector_t *sec = &level.sectors[s];
						sec->damageamount = damage;
						sec->damagetype = NAME_None;
						if (sec->damageamount < 20)
						{
							sec->leakydamage = 0;
							sec->damageinterval = 32;
						}
						else if (sec->damageamount < 50)
						{
							sec->leakydamage = 5;
							sec->damageinterval = 32;
						}
						else
						{
							sec->leakydamage = 256;
							sec->damageinterval = 1;
						}
					}
				}
				break;

			case Init_SectorLink:
				if (line.args[3] == 0)
					P_AddSectorLinksByID(line.frontsector, line.args[0], line.args[2]);
				break;

			// killough 10/98:
			//
			// Support for sky textures being transferred from sidedefs.
			// Allows scrolling and other effects (but if scrolling is
			// used, then the same sector tag needs to be used for the
			// sky sector, the sky-transfer linedef, and the scroll-effect
			// linedef). Still requires user to use F_SKY1 for the floor
			// or ceiling texture, to distinguish floor and ceiling sky.

			case Init_TransferSky:
				{
					FSectorTagIterator itr(line.args[0]);
					while ((s = itr.Next()) >= 0)
						level.sectors[s].sky = (line.Index() + 1) | PL_SKYFLAT;
					break;
				}
			}
			break;
		}
	}
	// [RH] Start running any open scripts on this map
	FBehavior::StaticStartTypedScripts (SCRIPT_Open, NULL, false);
}

////////////////////////////////////////////////////////////////////////////
//
// FRICTION EFFECTS
//
// phares 3/12/98: Start of friction effects

// As the player moves, friction is applied by decreasing the x and y
// velocity values on each tic. By varying the percentage of decrease,
// we can simulate muddy or icy conditions. In mud, the player slows
// down faster. In ice, the player slows down more slowly.
//
// The amount of friction change is controlled by the length of a linedef
// with type 223. A length < 100 gives you mud. A length > 100 gives you ice.
//
// Also, each sector where these effects are to take place is given a
// new special type _______. Changing the type value at runtime allows
// these effects to be turned on or off.
//
// Sector boundaries present problems. The player should experience these
// friction changes only when his feet are touching the sector floor. At
// sector boundaries where floor height changes, the player can find
// himself still 'in' one sector, but with his feet at the floor level
// of the next sector (steps up or down). To handle this, Thinkers are used
// in icy/muddy sectors. These thinkers examine each object that is touching
// their sectors, looking for players whose feet are at the same level as
// their floors. Players satisfying this condition are given new friction
// values that are applied by the player movement code later.

//
// killough 8/28/98:
//
// Completely redid code, which did not need thinkers, and which put a heavy
// drag on CPU. Friction is now a property of sectors, NOT objects inside
// them. All objects, not just players, are affected by it, if they touch
// the sector's floor. Code simpler and faster, only calling on friction
// calculations when an object needs friction considered, instead of doing
// friction calculations on every sector during every tic.
//
// Although this -might- ruin Boom demo sync involving friction, it's the only
// way, short of code explosion, to fix the original design bug. Fixing the
// design bug in Boom's original friction code, while maintaining demo sync
// under every conceivable circumstance, would double or triple code size, and
// would require maintenance of buggy legacy code which is only useful for old
// demos. Doom demos, which are more important IMO, are not affected by this
// change.
//
// [RH] On the other hand, since I've given up on trying to maintain demo
//		sync between versions, these considerations aren't a big deal to me.
//
/////////////////////////////
//
// Initialize the sectors where friction is increased or decreased

static void P_SpawnFriction(void)
{
	line_t *l = &level.lines[0];

	for (unsigned i = 0 ; i < level.lines.Size() ; i++,l++)
	{
		if (l->special == Sector_SetFriction)
		{
			int length;

			if (l->args[1])
			{	// [RH] Allow setting friction amount from parameter
				length = l->args[1] <= 200 ? l->args[1] : 200;
			}
			else
			{
				length = int(l->Delta().Length());
			}

			P_SetSectorFriction (l->args[0], length, false);
			l->special = 0;
		}
	}
}

void P_SetSectorFriction (int tag, int amount, bool alterFlag)
{
	int s;
	double friction, movefactor;

	// An amount of 100 should result in a friction of
	// ORIG_FRICTION (0xE800)
	friction = ((0x1EB8 * amount) / 0x80 + 0xD001) / 65536.;

	// killough 8/28/98: prevent odd situations
	friction = clamp(friction, 0., 1.);

	// The following check might seem odd. At the time of movement,
	// the move distance is multiplied by 'friction/0x10000', so a
	// higher friction value actually means 'less friction'.
	movefactor = FrictionToMoveFactor(friction);

	FSectorTagIterator itr(tag);
	while ((s = itr.Next()) >= 0)
	{
		// killough 8/28/98:
		//
		// Instead of spawning thinkers, which are slow and expensive,
		// modify the sector's own friction values. Friction should be
		// a property of sectors, not objects which reside inside them.
		// Original code scanned every object in every friction sector
		// on every tic, adjusting its friction, putting unnecessary
		// drag on CPU. New code adjusts friction of sector only once
		// at level startup, and then uses this friction value.

		level.sectors[s].friction = friction;
		level.sectors[s].movefactor = movefactor;
		if (alterFlag)
		{
			// When used inside a script, the sectors' friction flags
			// can be enabled and disabled at will.
			if (friction == ORIG_FRICTION)
			{
				level.sectors[s].Flags &= ~SECF_FRICTION;
			}
			else
			{
				level.sectors[s].Flags |= SECF_FRICTION;
			}
		}
	}
}

double FrictionToMoveFactor(double friction)
{
	double movefactor;

	// [RH] Twiddled these values so that velocity on ice (with
	//		friction 0xf900) is the same as in Heretic/Hexen.
	if (friction >= ORIG_FRICTION)	// ice
									//movefactor = ((0x10092 - friction)*(0x70))/0x158;
		movefactor = (((0x10092 - friction * 65536) * 1024) / 4352 + 568) / 65536.;
	else
		movefactor = (((friction*65536. - 0xDB34)*(0xA)) / 0x80) / 65536.;

	// killough 8/28/98: prevent odd situations
	if (movefactor < 1 / 2048.)
		movefactor = 1 / 2048.;

	return movefactor;
}
