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
//      Put all global state variables here.
//
//-----------------------------------------------------------------------------



#include "stringtable.h"
#include "doomstat.h"
#include "c_cvars.h"
#include "i_system.h"
#include "g_level.h"
#include "p_local.h"
#include "g_levellocals.h"

int SaveVersion;

// Localizable strings
FStringTable	GStrings;

// Game speed
EGameSpeed		GameSpeed = SPEED_Normal;

// Show developer messages if true.
CVAR (Int, developer, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// [RH] Feature control cvars
CVAR (Bool, var_friction, true, CVAR_SERVERINFO);

CVAR (Bool, alwaysapplydmflags, false, CVAR_SERVERINFO);

CUSTOM_CVAR (Float, teamdamage, 0.f, CVAR_SERVERINFO)
{
	level.teamdamage = self;
}

CUSTOM_CVAR (String, language, "auto", CVAR_ARCHIVE|CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
{
	GStrings.UpdateLanguage();
	if (level.info != NULL) level.LevelName = level.info->LookupLevelName();
}

// [RH] Network arbitrator
int Net_Arbitrator = 0;

int NextSkill = -1;

int SinglePlayerClass[MAXPLAYERS];

bool ToggleFullscreen;
int BorderTopRefresh;

FString LumpFilterIWAD;
