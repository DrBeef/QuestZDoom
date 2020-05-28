/*
** compatibility.cpp
** Handles compatibility flags for maps that are unlikely to be updated.
**
**---------------------------------------------------------------------------
** Copyright 2009 Randy Heit
** All rights reserved.
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
** This file is for maps that have been rendered broken by bug fixes or other
** changes that seemed minor at the time, and it is unlikely that the maps
** will be changed. If you are making a map and you know it needs a
** compatibility option to play properly, you are advised to specify so with
** a MAPINFO.
*/

// HEADER FILES ------------------------------------------------------------

#include "compatibility.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"
#include "c_dispatch.h"
#include "gi.h"
#include "g_level.h"
#include "p_lnspec.h"
#include "p_tags.h"
#include "r_state.h"
#include "w_wad.h"
#include "textures.h"
#include "g_levellocals.h"
#include "vm.h"
#include "actor.h"
#include "types.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct FCompatOption
{
	const char *Name;
	uint32_t CompatFlags;
	int WhichSlot;
};

enum
{
	SLOT_COMPAT,
	SLOT_COMPAT2,
	SLOT_BCOMPAT
};

enum
{
	CP_END,
	CP_CLEARFLAGS,
	CP_SETFLAGS,
	CP_SETSPECIAL,
	CP_CLEARSPECIAL,
	CP_SETACTIVATION,
	CP_SETSECTOROFFSET,
	CP_SETSECTORSPECIAL,
	CP_SETWALLYSCALE,
	CP_SETWALLTEXTURE,
	CP_SETTHINGZ,
	CP_SETTAG,
	CP_SETTHINGFLAGS,
	CP_SETVERTEX,
	CP_SETTHINGSKILLS,
	CP_SETSECTORTEXTURE,
	CP_SETSECTORLIGHT,
	CP_SETLINESECTORREF,
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------
extern TArray<FMapThing> MapThingsConverted;
extern bool ForceNodeBuild;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

TMap<FMD5Holder, FCompatValues, FMD5HashTraits> BCompatMap;

CVAR (Bool, sv_njnoautolevelcompat, false, CVAR_SERVERINFO | CVAR_LATCH)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FCompatOption Options[] =
{
	{ "setslopeoverflow",		BCOMPATF_SETSLOPEOVERFLOW, SLOT_BCOMPAT },
	{ "resetplayerspeed",		BCOMPATF_RESETPLAYERSPEED, SLOT_BCOMPAT },
	{ "vileghosts",				BCOMPATF_VILEGHOSTS, SLOT_BCOMPAT },
	{ "ignoreteleporttags",		BCOMPATF_BADTELEPORTERS, SLOT_BCOMPAT },
	{ "rebuildnodes",			BCOMPATF_REBUILDNODES, SLOT_BCOMPAT },
	{ "linkfrozenprops",		BCOMPATF_LINKFROZENPROPS, SLOT_BCOMPAT },
	{ "floatbob",				BCOMPATF_FLOATBOB, SLOT_BCOMPAT },
	{ "noslopeid",				BCOMPATF_NOSLOPEID, SLOT_BCOMPAT },
	{ "clipmidtex",				BCOMPATF_CLIPMIDTEX, SLOT_BCOMPAT },

	// list copied from g_mapinfo.cpp
	{ "shorttex",				COMPATF_SHORTTEX, SLOT_COMPAT },
	{ "stairs",					COMPATF_STAIRINDEX, SLOT_COMPAT },
	{ "limitpain",				COMPATF_LIMITPAIN, SLOT_COMPAT },
	{ "nopassover",				COMPATF_NO_PASSMOBJ, SLOT_COMPAT },
	{ "notossdrops",			COMPATF_NOTOSSDROPS, SLOT_COMPAT },
	{ "useblocking", 			COMPATF_USEBLOCKING, SLOT_COMPAT },
	{ "nodoorlight",			COMPATF_NODOORLIGHT, SLOT_COMPAT },
	{ "ravenscroll",			COMPATF_RAVENSCROLL, SLOT_COMPAT },
	{ "soundtarget",			COMPATF_SOUNDTARGET, SLOT_COMPAT },
	{ "dehhealth",				COMPATF_DEHHEALTH, SLOT_COMPAT },
	{ "trace",					COMPATF_TRACE, SLOT_COMPAT },
	{ "dropoff",				COMPATF_DROPOFF, SLOT_COMPAT },
	{ "boomscroll",				COMPATF_BOOMSCROLL, SLOT_COMPAT },
	{ "invisibility",			COMPATF_INVISIBILITY, SLOT_COMPAT },
	{ "silentinstantfloors",	COMPATF_SILENT_INSTANT_FLOORS, SLOT_COMPAT },
	{ "sectorsounds",			COMPATF_SECTORSOUNDS, SLOT_COMPAT },
	{ "missileclip",			COMPATF_MISSILECLIP, SLOT_COMPAT },
	{ "crossdropoff",			COMPATF_CROSSDROPOFF, SLOT_COMPAT },
	{ "wallrun",				COMPATF_WALLRUN, SLOT_COMPAT },		// [GZ] Added for CC MAP29
	{ "anybossdeath",			COMPATF_ANYBOSSDEATH, SLOT_COMPAT },// [GZ] Added for UAC_DEAD
	{ "mushroom",				COMPATF_MUSHROOM, SLOT_COMPAT },
	{ "mbfmonstermove",			COMPATF_MBFMONSTERMOVE, SLOT_COMPAT },
	{ "corpsegibs",				COMPATF_CORPSEGIBS, SLOT_COMPAT },
	{ "noblockfriends",			COMPATF_NOBLOCKFRIENDS, SLOT_COMPAT },
	{ "spritesort",				COMPATF_SPRITESORT, SLOT_COMPAT },
	{ "hitscan",				COMPATF_HITSCAN, SLOT_COMPAT },
	{ "lightlevel",				COMPATF_LIGHT, SLOT_COMPAT },
	{ "polyobj",				COMPATF_POLYOBJ, SLOT_COMPAT },
	{ "maskedmidtex",			COMPATF_MASKEDMIDTEX, SLOT_COMPAT },
	{ "badangles",				COMPATF2_BADANGLES, SLOT_COMPAT2 },
	{ "floormove",				COMPATF2_FLOORMOVE, SLOT_COMPAT2 },
	{ "soundcutoff",			COMPATF2_SOUNDCUTOFF, SLOT_COMPAT2 },
	{ "pointonline",			COMPATF2_POINTONLINE, SLOT_COMPAT2 },
	{ "multiexit",				COMPATF2_MULTIEXIT, SLOT_COMPAT2 },
	{ "teleport",				COMPATF2_TELEPORT, SLOT_COMPAT2 },
	{ "disablepushwindowcheck",	COMPATF2_PUSHWINDOW, SLOT_COMPAT2 },
	{ "checkswitchrange",		COMPATF2_CHECKSWITCHRANGE, SLOT_COMPAT2 },
	{ "explode1",				COMPATF2_EXPLODE1, SLOT_COMPAT2 },
	{ "explode2",				COMPATF2_EXPLODE2, SLOT_COMPAT2 },
	{ "railing",				COMPATF2_RAILING, SLOT_COMPAT2 },
	{ "scriptwait",				COMPATF2_SCRIPTWAIT, SLOT_COMPAT2 },
	{ NULL, 0, 0 }
};

static const char *const LineSides[] =
{
	"Front", "Back", NULL
};

static const char *const WallTiers[] =
{
	"Top", "Mid", "Bot", NULL
};

static const char *const SectorPlanes[] =
{
	"floor", "ceil", NULL
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// ParseCompatibility
//
//==========================================================================

void ParseCompatibility()
{
	TArray<FMD5Holder> md5array;
	FMD5Holder md5;
	FCompatValues flags;
	int i, x;
	unsigned int j;

	BCompatMap.Clear();

	// The contents of this file are not cumulative, as it should not
	// be present in user-distributed maps.
	FScanner sc(Wads.GetNumForFullName("compatibility.txt"));

	while (sc.GetString())	// Get MD5 signature
	{
		do
		{
			if (strlen(sc.String) != 32)
			{
				sc.ScriptError("MD5 signature must be exactly 32 characters long");
			}
			for (i = 0; i < 32; ++i)
			{
				if (sc.String[i] >= '0' && sc.String[i] <= '9')
				{
					x = sc.String[i] - '0';
				}
				else
				{
					sc.String[i] |= 'a' ^ 'A';
					if (sc.String[i] >= 'a' && sc.String[i] <= 'f')
					{
						x = sc.String[i] - 'a' + 10;
					}
					else
					{
						x = 0;
						sc.ScriptError("MD5 signature must be a hexadecimal value");
					}
				}
				if (!(i & 1))
				{
					md5.Bytes[i / 2] = x << 4;
				}
				else
				{
					md5.Bytes[i / 2] |= x;
				}
			}
			md5array.Push(md5);
			sc.MustGetString();
		} while (!sc.Compare("{"));
		memset(flags.CompatFlags, 0, sizeof(flags.CompatFlags));
		flags.ExtCommandIndex = ~0u;
		while (sc.GetString())
		{
			if ((i = sc.MatchString(&Options[0].Name, sizeof(*Options))) >= 0)
			{
				flags.CompatFlags[Options[i].WhichSlot] |= Options[i].CompatFlags;
			}
			else
			{
				sc.UnGet();
				break;
			}
		}
		sc.MustGetStringName("}");
		for (j = 0; j < md5array.Size(); ++j)
		{
			BCompatMap[md5array[j]] = flags;
		}
		md5array.Clear();
	}
}

//==========================================================================
//
// CheckCompatibility
//
//==========================================================================

FName CheckCompatibility(MapData *map)
{
	FMD5Holder md5;
	FCompatValues *flags;

	ii_compatflags = 0;
	ii_compatflags2 = 0;
	ib_compatflags = 0;

	// When playing Doom IWAD levels force COMPAT_SHORTTEX and COMPATF_LIGHT.
	// I'm not sure if the IWAD maps actually need COMPATF_LIGHT but it certainly does not hurt.
	// TNT's MAP31 also needs COMPATF_STAIRINDEX but that only gets activated for TNT.WAD.
	if (Wads.GetLumpFile(map->lumpnum) == Wads.GetIwadNum() && (gameinfo.flags & GI_COMPATSHORTTEX) && level.maptype == MAPTYPE_DOOM)
	{
		ii_compatflags = COMPATF_SHORTTEX|COMPATF_LIGHT;
		if (gameinfo.flags & GI_COMPATSTAIRS) ii_compatflags |= COMPATF_STAIRINDEX;
	}

	map->GetChecksum(md5.Bytes);

	flags = BCompatMap.CheckKey(md5);

	FString hash;

	for (size_t j = 0; j < sizeof(md5.Bytes); ++j)
	{
		hash.AppendFormat("%02X", md5.Bytes[j]);
	}

	if (developer >= DMSG_NOTIFY)
	{
		Printf("MD5 = %s", hash.GetChars());
		if (flags != NULL)
		{
			Printf(", cflags = %08x, cflags2 = %08x, bflags = %08x\n",
				flags->CompatFlags[SLOT_COMPAT], flags->CompatFlags[SLOT_COMPAT2], flags->CompatFlags[SLOT_BCOMPAT]);
		}
		else
		{
			Printf("\n");
		}
	}

	if (flags != NULL)
	{
		ii_compatflags |= flags->CompatFlags[SLOT_COMPAT];
		ii_compatflags2 |= flags->CompatFlags[SLOT_COMPAT2];
		ib_compatflags |= flags->CompatFlags[SLOT_BCOMPAT];
	}

	// Reset i_compatflags
	compatflags.Callback();
	compatflags2.Callback();
	// Set floatbob compatibility for all maps with an original Hexen MAPINFO.
	if (level.flags2 & LEVEL2_HEXENHACK)
	{
		ib_compatflags |= BCOMPATF_FLOATBOB;
	}
	return FName(hash, true);	// if this returns NAME_None it means there is no scripted compatibility handler.
}

//==========================================================================
//
// PostProcessLevel
//
//==========================================================================

class DLevelPostProcessor : public DObject
{
	DECLARE_ABSTRACT_CLASS(DLevelPostProcessor, DObject)
};

IMPLEMENT_CLASS(DLevelPostProcessor, true, false);

void PostProcessLevel(FName checksum)
{
	auto lc = Create<DLevelPostProcessor>();

	if (sv_njnoautolevelcompat)
	{
		Printf("Warning: auto level compatibility disabled. Severe problems could arise.\n");
		return;
	}

	for (auto cls : PClass::AllClasses)
	{
		if (cls->IsDescendantOf(RUNTIME_CLASS(DLevelPostProcessor)))
		{
			PFunction *const func = dyn_cast<PFunction>(cls->FindSymbol("Apply", false));
			if (func == nullptr)
			{
				Printf("Missing 'Apply' method in class '%s', level compatibility object ignored\n", cls->TypeName.GetChars());
				continue;
			}

			auto argTypes = func->Variants[0].Proto->ArgumentTypes;
			if (argTypes.Size() != 3 || argTypes[1] != TypeName || argTypes[2] != TypeString)
			{
				Printf("Wrong signature of 'Apply' method in class '%s', level compatibility object ignored\n", cls->TypeName.GetChars());
				continue;
			}

			VMValue param[] = { lc, checksum.GetIndex(), &level.MapName };
			VMCall(func->Variants[0].Implementation, param, 3, nullptr, 0);
		}
	}
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, OffsetSectorPlane)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_INT(sector);
	PARAM_INT(planeval);
	PARAM_FLOAT(delta);

	if ((unsigned)sector < level.sectors.Size())
	{
		sector_t *sec = &level.sectors[sector];
		secplane_t& plane = sector_t::floor == planeval? sec->floorplane : sec->ceilingplane;
		plane.ChangeHeight(delta);
		sec->ChangePlaneTexZ(planeval, delta);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, ClearSectorTags)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_INT(sector);
	tagManager.RemoveSectorTags(sector);
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, AddSectorTag)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_INT(sector);
	PARAM_INT(tag);

	if ((unsigned)sector < level.sectors.Size())
	{
		tagManager.AddSectorTag(sector, tag);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, ClearLineIDs)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_INT(line);
	tagManager.RemoveLineIDs(line);
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, AddLineID)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_INT(line);
	PARAM_INT(tag);

	if ((unsigned)line < level.lines.Size())
	{
		tagManager.AddLineID(line, tag);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingCount)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	ACTION_RETURN_INT(MapThingsConverted.Size());
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, AddThing)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(ednum);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_INT(angle);
	PARAM_UINT(skills);
	PARAM_UINT(flags);

	auto &things = MapThingsConverted;
	const unsigned newindex = things.Size();
	things.Resize(newindex + 1);

	auto &newthing = things.Last();
	memset(&newthing, 0, sizeof newthing);

	newthing.Gravity = 1;
	newthing.SkillFilter = skills;
	newthing.ClassFilter = 0xFFFF;
	newthing.RenderStyle = STYLE_Count;
	newthing.Alpha = -1;
	newthing.Health = 1;
	newthing.FloatbobPhase = -1;
	newthing.pos.X = x;
	newthing.pos.Y = y;
	newthing.pos.Z = z;
	newthing.angle = angle;
	newthing.EdNum = ednum;
	newthing.info = DoomEdMap.CheckKey(ednum);
	newthing.flags = flags;

	ACTION_RETURN_INT(newindex);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingEdNum)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const int ednum = thing < MapThingsConverted.Size()
		? MapThingsConverted[thing].EdNum : 0;
	ACTION_RETURN_INT(ednum);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingEdNum)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_UINT(ednum);

	if (thing < MapThingsConverted.Size())
	{
		auto &mti = MapThingsConverted[thing];
		mti.EdNum = ednum;
		mti.info = DoomEdMap.CheckKey(ednum);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingPos)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const DVector3 pos = thing < MapThingsConverted.Size()
		? MapThingsConverted[thing].pos
		: DVector3(0, 0, 0);
	ACTION_RETURN_VEC3(pos);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingXY)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);

	if (thing < MapThingsConverted.Size())
	{
		auto& pos = MapThingsConverted[thing].pos;
		pos.X = x;
		pos.Y = y;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingZ)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_FLOAT(z);

	if (thing < MapThingsConverted.Size())
	{
		MapThingsConverted[thing].pos.Z = z;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingAngle)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const int angle = thing < MapThingsConverted.Size()
		? MapThingsConverted[thing].angle : 0;
	ACTION_RETURN_INT(angle);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingAngle)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_INT(angle);

	if (thing < MapThingsConverted.Size())
	{
		MapThingsConverted[thing].angle = angle;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingSkills)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const int skills = thing < MapThingsConverted.Size()
		? MapThingsConverted[thing].SkillFilter : 0;
	ACTION_RETURN_INT(skills);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingSkills)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_UINT(skillmask);

	if (thing < MapThingsConverted.Size())
	{
		MapThingsConverted[thing].SkillFilter = skillmask;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingFlags)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const unsigned flags = thing < MapThingsConverted.Size()
		? MapThingsConverted[thing].flags : 0;
	ACTION_RETURN_INT(flags);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingFlags)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_UINT(flags);

	if (thing < MapThingsConverted.Size())
	{
		MapThingsConverted[thing].flags = flags;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingSpecial)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const int special = thing < MapThingsConverted.Size()
		? MapThingsConverted[thing].special : 0;
	ACTION_RETURN_INT(special);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingSpecial)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_INT(special);

	if (thing < MapThingsConverted.Size())
	{
		MapThingsConverted[thing].special = special;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingArgument)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_UINT(index);

	const int argument = index < 5 && thing < MapThingsConverted.Size()
		? MapThingsConverted[thing].args[index] : 0;
	ACTION_RETURN_INT(argument);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingStringArgument)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const FName argument = thing < MapThingsConverted.Size()
		? MapThingsConverted[thing].arg0str : NAME_None;
	ACTION_RETURN_INT(argument);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingArgument)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_UINT(index);
	PARAM_INT(value);

	if (index < 5 && thing < MapThingsConverted.Size())
	{
		MapThingsConverted[thing].args[index] = value;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingStringArgument)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_INT(value);

	if (thing < MapThingsConverted.Size())
	{
		MapThingsConverted[thing].arg0str = ENamedName(value);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetThingID)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);

	const int id = thing < MapThingsConverted.Size()
		? MapThingsConverted[thing].thingid : 0;
	ACTION_RETURN_INT(id);
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetThingID)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(thing);
	PARAM_INT(id);

	if (thing < MapThingsConverted.Size())
	{
		MapThingsConverted[thing].thingid = id;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetVertex)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(vertex);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);

	if (vertex < level.vertexes.Size())
	{
		level.vertexes[vertex].p = DVector2(x, y);
	}
	ForceNodeBuild = true;
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetLineVertexes)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(lineidx);
	PARAM_UINT(vertexidx1);
	PARAM_UINT(vertexidx2);

	if (lineidx < level.lines.Size() &&
		vertexidx1 < level.vertexes.Size() &&
		vertexidx2 < level.vertexes.Size())
	{
		line_t *line = &level.lines[lineidx];
		vertex_t *vertex1 = &level.vertexes[vertexidx1];
		vertex_t *vertex2 = &level.vertexes[vertexidx2];

		line->v1 = vertex1;
		line->v2 = vertex2;
	}
	ForceNodeBuild = true;
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, FlipLineSideRefs)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(lineidx);

	if (lineidx < level.lines.Size())
	{
		line_t *line = &level.lines[lineidx];
		side_t *side1 = line->sidedef[1];
		side_t *side2 = line->sidedef[0];

		if (!!side1 && !!side2) // don't flip single-sided lines
		{
			sector_t *frontsector = line->sidedef[1]->sector;
			sector_t *backsector = line->sidedef[0]->sector;
			line->sidedef[0] = side1;
			line->sidedef[1] = side2;
			line->frontsector = frontsector;
			line->backsector = backsector;
		}
	}
	ForceNodeBuild = true;
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, SetLineSectorRef)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_UINT(lineidx);
	PARAM_UINT(sideidx);
	PARAM_UINT(sectoridx);

	if (   sideidx < 2
		&& lineidx < level.lines.Size()
		&& sectoridx < level.sectors.Size())
	{
		line_t *line = &level.lines[lineidx];
		side_t *side = line->sidedef[sideidx];
		side->sector = &level.sectors[sectoridx];
		if (sideidx == 0) line->frontsector = side->sector;
		else line->backsector = side->sector;
	}
	ForceNodeBuild = true;
	return 0;
}

DEFINE_ACTION_FUNCTION(DLevelPostProcessor, GetDefaultActor)
{
	PARAM_SELF_PROLOGUE(DLevelPostProcessor);
	PARAM_NAME(actorclass);
	ACTION_RETURN_OBJECT(GetDefaultByName(actorclass));
}


//==========================================================================
//
// CCMD mapchecksum
//
//==========================================================================

CCMD (mapchecksum)
{
	MapData *map;
	uint8_t cksum[16];

	if (argv.argc() < 2)
	{
		Printf("Usage: mapchecksum <map> ...\n");
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		map = P_OpenMapData(argv[i], true);
		if (map == NULL)
		{
			Printf("Cannot load %s as a map\n", argv[i]);
		}
		else
		{
			map->GetChecksum(cksum);
			const char *wadname = Wads.GetWadName(Wads.GetLumpFile(map->lumpnum));
			delete map;
			for (size_t j = 0; j < sizeof(cksum); ++j)
			{
				Printf("%02X", cksum[j]);
			}
			Printf(" // %s %s\n", wadname, argv[i]);
		}
	}
}

//==========================================================================
//
// CCMD hiddencompatflags
//
//==========================================================================

CCMD (hiddencompatflags)
{
	Printf("%08x %08x %08x\n", ii_compatflags, ii_compatflags2, ib_compatflags);
}

