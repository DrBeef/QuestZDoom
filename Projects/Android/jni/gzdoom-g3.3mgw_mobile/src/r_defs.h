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
//		Refresh/rendering module, shared data struct definitions.
//
//-----------------------------------------------------------------------------


#ifndef __R_DEFS_H__
#define __R_DEFS_H__

#include "doomdef.h"
#include "templates.h"
#include "m_bbox.h"
#include "dobjgc.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
// SECTORS do store MObjs anyway.
struct FLightNode;
struct FGLSection;
class FSerializer;
struct FPortal;
struct FSectorPortal;
struct FLinePortal;
struct seg_t;
struct sector_t;
class AActor;

#define MAXWIDTH 12000
#define MAXHEIGHT 5000

const uint16_t NO_INDEX = 0xffffu;
const uint32_t NO_SIDE = 0xffffffffu;

// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
enum
{
	SIL_NONE,
	SIL_BOTTOM,
	SIL_TOP,
	SIL_BOTH
};

struct FDisplacement;

//
// INTERNAL MAP TYPES
//	used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
//	like some DOOM-alikes ("wt", "WebView") did.
//
enum
{
	VERTEXFLAG_ZCeilingEnabled = 0x01,
	VERTEXFLAG_ZFloorEnabled   = 0x02
};
struct vertexdata_t
{
	double zCeiling, zFloor;
	uint32_t flags;
};

struct vertex_t
{
	DVector2 p;

	void set(fixed_t x, fixed_t y)
	{
		p.X = x / 65536.;
		p.Y = y / 65536.;
	}

	void set(double x, double y)
	{
		p.X = x;
		p.Y = y;
	}

	void set(const DVector2 &pos)
	{
		p = pos;
	}

	double fX() const
	{
		return p.X;
	}

	double fY() const
	{
		return p.Y;
	}

	fixed_t fixX() const
	{
		return FLOAT2FIXED(p.X);
	}

	fixed_t fixY() const
	{
		return FLOAT2FIXED(p.Y);
	}

	DVector2 fPos() const
	{
		return p;
	}

	int Index() const;


	angle_t viewangle;	// precalculated angle for clipping
	int angletime;		// recalculation time for view angle
	bool dirty;			// something has changed and needs to be recalculated
	int numheights;
	int numsectors;
	sector_t ** sectors;
	float * heightlist;

	vertex_t()
	{
		p = { 0,0 };
		angletime = 0;
		viewangle = 0;
		dirty = true;
		numheights = numsectors = 0;
		sectors = NULL;
		heightlist = NULL;
	}

	~vertex_t()
	{
		if (sectors != nullptr) delete[] sectors;
		if (heightlist != nullptr) delete[] heightlist;
	}

	bool operator== (const vertex_t &other)
	{
		return p == other.p;
	}

	bool operator!= (const vertex_t &other)
	{
		return p != other.p;
	}

	void clear()
	{
		p.Zero();
	}

	angle_t GetClipAngle();
};

// Forward of LineDefs, for Sectors.
struct line_t;

class player_t;
class FScanner;
class FBitmap;
struct FCopyInfo;
class DInterpolation;

enum
{
	UDMF_Line,
	UDMF_Side,
	UDMF_Sector,
	UDMF_Thing
};


struct FUDMFKey
{
	enum
	{
		UDMF_Int,
		UDMF_Float,
		UDMF_String
	};

	FName Key;
	int Type;
	int IntVal;
	double FloatVal;
	FString StringVal;

	FUDMFKey()
	{
	}

	FUDMFKey& operator =(int val)
	{
		Type = UDMF_Int;
		IntVal = val;
		FloatVal = val;
		StringVal = "";
		return *this;
	}

	FUDMFKey& operator =(double val)
	{
		Type = UDMF_Float;
		IntVal = int(val);
		FloatVal = val;
		StringVal = "";
		return *this;
	}

	FUDMFKey& operator =(const FString &val)
	{
		Type = UDMF_String;
		IntVal = (int)strtoll(val.GetChars(), NULL, 0);
		FloatVal = strtod(val.GetChars(), NULL);
		StringVal = val;
		return *this;
	}

};

class FUDMFKeys : public TArray<FUDMFKey>
{
	bool mSorted = false;
public:
	void Sort();
	FUDMFKey *Find(FName key);
};

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
class DSectorEffect;
struct sector_t;
struct FRemapTable;

enum
{
	SECSPAC_Enter		= 1<< 0,	// Trigger when player enters
	SECSPAC_Exit		= 1<< 1,	// Trigger when player exits
	SECSPAC_HitFloor	= 1<< 2,	// Trigger when player hits floor
	SECSPAC_HitCeiling	= 1<< 3,	// Trigger when player hits ceiling
	SECSPAC_Use			= 1<< 4,	// Trigger when player uses
	SECSPAC_UseWall		= 1<< 5,	// Trigger when player uses a wall
	SECSPAC_EyesDive	= 1<< 6,	// Trigger when player eyes go below fake floor
	SECSPAC_EyesSurface = 1<< 7,	// Trigger when player eyes go above fake floor
	SECSPAC_EyesBelowC	= 1<< 8,	// Trigger when player eyes go below fake ceiling
	SECSPAC_EyesAboveC	= 1<< 9,	// Trigger when player eyes go above fake ceiling
	SECSPAC_HitFakeFloor= 1<<10,	// Trigger when player hits fake floor
	SECSPAC_DamageFloor = 1<<11,	// Trigger when floor is damaged
	SECSPAC_DamageCeiling=1<<12,	// Trigger when ceiling is damaged
	SECSPAC_DeathFloor	= 1<<13,	// Trigger when floor has 0 hp
	SECSPAC_DeathCeiling= 1<<14,	// Trigger when ceiling has 0 hp
	SECSPAC_Damage3D	= 1<<15,	// Trigger when controlled 3d floor is damaged
	SECSPAC_Death3D		= 1<<16		// Trigger when controlled 3d floor has 0 hp
};

struct secplane_t
{
	// the plane is defined as a*x + b*y + c*z + d = 0
	// ic is 1/c, for faster Z calculations

//private:
	DVector3 normal;
	double  D, negiC;	// negative iC because that also saves a negation in all methods using this.
public:
	friend FSerializer &Serialize(FSerializer &arc, const char *key, secplane_t &p, secplane_t *def);

	void set(double aa, double bb, double cc, double dd)
	{
		normal.X = aa;
		normal.Y = bb;
		normal.Z = cc;
		D = dd;
		negiC = -1 / cc;
	}

	void setD(double dd)
	{
		D = dd;
	}

	double fC() const
	{
		return normal.Z;
	}
	double fD() const
	{
		return D;
	}
	
	bool isSlope() const
	{
		return !normal.XY().isZero();
	}

	const DVector3 &Normal() const
	{
		return normal;
	}

	// Returns < 0 : behind; == 0 : on; > 0 : in front
	int PointOnSide(const DVector3 &pos) const
	{
		double v = (normal | pos) + D;
		return v < -EQUAL_EPSILON ? -1 : v > EQUAL_EPSILON ? 1 : 0;
	}

	// Returns the value of z at (0,0) This is used by the 3D floor code which does not handle slopes
	double Zat0() const
	{
		return negiC*D;
	}

	// Returns the value of z at (x,y)
	fixed_t ZatPoint(fixed_t x, fixed_t y) const = delete;	// it is not allowed to call this.

	// Returns the value of z at (x,y) as a double
	double ZatPoint (double x, double y) const
	{
		return (D + normal.X*x + normal.Y*y) * negiC;
	}

	double ZatPoint(const DVector2 &pos) const
	{
		return (D + normal.X*pos.X + normal.Y*pos.Y) * negiC;
	}

	double ZatPoint(const FVector2 &pos) const
	{
		return (D + normal.X*pos.X + normal.Y*pos.Y) * negiC;
	}

	double ZatPoint(const vertex_t *v) const
	{
		return (D + normal.X*v->fX() + normal.Y*v->fY()) * negiC;
	}

	// Returns the value of z at vertex v if d is equal to dist
	double ZatPointDist(const vertex_t *v, double dist)
	{
		return (dist + normal.X*v->fX() + normal.Y*v->fY()) * negiC;
	}
	// Flips the plane's vertical orientiation, so that if it pointed up,
	// it will point down, and vice versa.
	void FlipVert ()
	{
		normal = -normal;
		D = -D;
		negiC = -negiC;
	}

	// Returns true if 2 planes are the same
	bool operator== (const secplane_t &other) const
	{
		return normal == other.normal && D == other.D;
	}

	// Returns true if 2 planes are different
	bool operator!= (const secplane_t &other) const
	{
		return normal != other.normal || D != other.D;
	}

	// Moves a plane up/down by hdiff units
	void ChangeHeight(double hdiff)
	{
		D = D - hdiff * normal.Z;
	}

	// Moves a plane up/down by hdiff units
	double GetChangedHeight(double hdiff) const
	{
		return D - hdiff * normal.Z;
	}

	// Returns how much this plane's height would change if d were set to oldd
	double HeightDiff(double oldd) const
	{
		return (D - oldd) * negiC;
	}

	// Returns how much this plane's height would change if d were set to oldd
	double HeightDiff(double oldd, double newd) const
	{
		return (newd - oldd) * negiC;
	}

	double PointToDist(const DVector2 &xy, double z) const
	{
		return -(normal.X * xy.X + normal.Y * xy.Y + normal.Z * z);
	}

	double PointToDist(const vertex_t *v, double z) const
	{
		return -(normal.X * v->fX() + normal.Y * v->fY() + normal.Z * z);
	}

	void SetAtHeight(double height, int ceiling)
	{
		normal.X = normal.Y = 0;
		if (ceiling)
		{
			normal.Z = -1;
			negiC = 1;
			D = height;
		}
		else
		{
			normal.Z = 1;
			negiC = -1;
			D = -height;
		}
	}

	bool CopyPlaneIfValid (secplane_t *dest, const secplane_t *opp) const;
	inline double ZatPoint(const AActor *ac) const;

};

#include "p_3dfloors.h"
struct subsector_t;
struct sector_t;
struct side_t;
extern bool gl_plane_reflection_i;

// Ceiling/floor flags
enum
{
	PLANEF_ABSLIGHTING	= 1,	// floor/ceiling light is absolute, not relative
	PLANEF_BLOCKED		= 2,	// can not be moved anymore.
	PLANEF_ADDITIVE		= 4,	// rendered additive

	// linked portal stuff
	PLANEF_NORENDER		= 8,
	PLANEF_NOPASS		= 16,
	PLANEF_BLOCKSOUND	= 32,
	PLANEF_DISABLED		= 64,
	PLANEF_OBSTRUCTED	= 128,	// if the portal plane is beyond the sector's floor or ceiling.
	PLANEF_LINKED		= 256	// plane is flagged as a linked portal
};

// Internal sector flags
enum
{
	SECMF_FAKEFLOORONLY		= 2,	// when used as heightsec in R_FakeFlat, only copies floor
	SECMF_CLIPFAKEPLANES	= 4,	// as a heightsec, clip planes to target sector's planes
	SECMF_NOFAKELIGHT		= 8,	// heightsec does not change lighting
	SECMF_IGNOREHEIGHTSEC	= 16,	// heightsec is only for triggering sector actions
	SECMF_UNDERWATER		= 32,	// sector is underwater
	SECMF_FORCEDUNDERWATER	= 64,	// sector is forced to be underwater
	SECMF_UNDERWATERMASK	= 32+64,
	SECMF_DRAWN				= 128,	// sector has been drawn at least once
	SECMF_HIDDEN			= 256,	// Do not draw on textured automap
	SECMF_OVERLAPPING		= 512,	// floor and ceiling overlap and require special renderer action.
	SECMF_NOSKYWALLS		= 1024,	// Do not draw "sky walls"
};

enum
{
	SECF_SILENT			= 1,	// actors in sector make no noise
	SECF_NOFALLINGDAMAGE= 2,	// No falling damage in this sector
	SECF_FLOORDROP		= 4,	// all actors standing on this floor will remain on it when it lowers very fast.
	SECF_NORESPAWN		= 8,	// players can not respawn in this sector
	SECF_FRICTION		= 16,	// sector has friction enabled
	SECF_PUSH			= 32,	// pushers enabled
	SECF_SILENTMOVE		= 64,	// Sector movement makes mo sound (Eternity got this so this may be useful for an extended cross-port standard.) 
	SECF_DMGTERRAINFX	= 128,	// spawns terrain splash when inflicting damage
	SECF_ENDGODMODE		= 256,	// getting damaged by this sector ends god mode
	SECF_ENDLEVEL		= 512,	// ends level when health goes below 10
	SECF_HAZARD			= 1024,	// Change to Strife's delayed damage handling.
	SECF_NOATTACK		= 2048,	// monsters cannot start attacks in this sector.

	SECF_WASSECRET		= 1 << 30,	// a secret that was discovered
	SECF_SECRET			= 1 << 31,	// a secret sector

	SECF_DAMAGEFLAGS = SECF_ENDGODMODE|SECF_ENDLEVEL|SECF_DMGTERRAINFX|SECF_HAZARD,
	SECF_NOMODIFY = SECF_SECRET|SECF_WASSECRET,	// not modifiable by Sector_ChangeFlags
	SECF_SPECIALFLAGS = SECF_DAMAGEFLAGS|SECF_FRICTION|SECF_PUSH,	// these flags originate from 'special and must be transferrable by floor thinkers
};

enum
{
	PL_SKYFLAT = 0x40000000
};

struct FDynamicColormap;


struct FLinkedSector
{
	sector_t *Sector;
	int Type;
};


// this substructure contains a few sector properties that are stored in dynamic arrays
// These must not be copied by R_FakeFlat etc. or bad things will happen.
struct extsector_t
{
	// Boom sector transfer information
	struct fakefloor
	{
		TArray<sector_t *> Sectors;
	} FakeFloor;

	// 3DMIDTEX information
	struct midtex
	{
		struct plane
		{
			TArray<sector_t *> AttachedSectors;		// all sectors containing 3dMidtex lines attached to this sector
			TArray<line_t *> AttachedLines;			// all 3dMidtex lines attached to this sector
		} Floor, Ceiling;
	} Midtex;

	// Linked sector information
	struct linked
	{
		struct plane
		{
			TArray<FLinkedSector> Sectors;
		} Floor, Ceiling;
	} Linked;

	// 3D floors
	struct xfloor
	{
		TDeletingArray<F3DFloor *>		ffloors;		// 3D floors in this sector
		TArray<lightlist_t>				lightlist;		// 3D light list
		TArray<sector_t*>				attached;		// 3D floors attached to this sector
	} XFloor;

	TArray<vertex_t *> vertices;
};

struct FTransform
{
	// killough 3/7/98: floor and ceiling texture offsets
	double xOffs, yOffs, baseyOffs;

	// [RH] floor and ceiling texture scales
	double xScale, yScale;

	// [RH] floor and ceiling texture rotation
	DAngle Angle, baseAngle;

	finline bool operator == (const FTransform &other) const
	{
		return xOffs == other.xOffs && yOffs + baseyOffs == other.yOffs + other.baseyOffs &&
			xScale == other.xScale && yScale == other.yScale && Angle + baseAngle == other.Angle + other.baseAngle;
	}
	finline bool operator != (const FTransform &other) const
	{
		return !(*this == other);
	}

};

struct secspecial_t
{
	FName damagetype;		// [RH] Means-of-death for applied damage
	int damageamount;			// [RH] Damage to do while standing on floor
	short special;
	short damageinterval;	// Interval for damage application
	short leakydamage;		// chance of leaking through radiation suit
	int Flags;
};

FSerializer &Serialize(FSerializer &arc, const char *key, secspecial_t &spec, secspecial_t *def);

enum class EMoveResult { ok, crushed, pastdest };

struct sector_t
{
	// Member functions

private:
	bool MoveAttached(int crush, double move, int floorOrCeiling, bool resetfailed, bool instant = false);
public:
	EMoveResult MoveFloor(double speed, double dest, int crush, int direction, bool hexencrush, bool instant = false);
	EMoveResult MoveCeiling(double speed, double dest, int crush, int direction, bool hexencrush);

	inline EMoveResult MoveFloor(double speed, double dest, int direction)
	{
		return MoveFloor(speed, dest, -1, direction, false);
	}

	inline EMoveResult MoveCeiling(double speed, double dest, int direction)
	{
		return MoveCeiling(speed, dest, -1, direction, false);
	}

	bool IsLinked(sector_t *other, bool ceiling) const;

	sector_t *NextSpecialSector (int type, sector_t *prev) const;		// [RH]
	void RemoveForceField();
	int Index() const;

	void AdjustFloorClip () const;
	void SetColor(PalEntry pe, int desat);
	void SetFade(PalEntry pe);
	void SetFogDensity(int dens);
	void ClosestPoint(const DVector2 &pos, DVector2 &out) const;

	int GetFloorLight() const;
	int GetCeilingLight() const;

	sector_t *GetHeightSec() const
	{
		return (MoreFlags & SECMF_IGNOREHEIGHTSEC)? nullptr : heightsec;
	}

	double GetFriction(int plane = sector_t::floor, double *movefac = NULL) const;
	bool TriggerSectorActions(AActor *thing, int activation);

	DInterpolation *SetInterpolation(int position, bool attach);

	FSectorPortal *ValidatePortal(int which);
	void CheckPortalPlane(int plane);


	enum
	{
		floor,
		ceiling,
		// only used for specialcolors array
		walltop,
		wallbottom,
		sprites
	};

	struct splane
	{
		FTransform xform;
		int Flags;
		int Light;
		double alpha;
		double TexZ;
		PalEntry GlowColor;
		float GlowHeight;
		FTextureID Texture;
	};


	splane planes[2];

	void SetXOffset(int pos, double o)
	{
		planes[pos].xform.xOffs = o;
	}

	void AddXOffset(int pos, double o)
	{
		planes[pos].xform.xOffs += o;
	}

	double GetXOffset(int pos) const
	{
		return planes[pos].xform.xOffs;
	}

	void SetYOffset(int pos, double o)
	{
		planes[pos].xform.yOffs = o;
	}

	void AddYOffset(int pos, double o)
	{
		planes[pos].xform.yOffs += o;
	}

	double GetYOffset(int pos, bool addbase = true) const
	{
		if (!addbase)
		{
			return planes[pos].xform.yOffs;
		}
		else
		{
			return planes[pos].xform.yOffs + planes[pos].xform.baseyOffs;
		}
	}

	void SetXScale(int pos, double o)
	{
		planes[pos].xform.xScale = o;
	}

	double GetXScale(int pos) const
	{
		return planes[pos].xform.xScale;
	}

	void SetYScale(int pos, double o)
	{
		planes[pos].xform.yScale = o;
	}

	double GetYScale(int pos) const
	{
		return planes[pos].xform.yScale;
	}

	void SetAngle(int pos, DAngle o)
	{
		planes[pos].xform.Angle = o;
	}

	DAngle GetAngle(int pos, bool addbase = true) const
	{
		if (!addbase)
		{
			return planes[pos].xform.Angle;
		}
		else
		{
			return planes[pos].xform.Angle + planes[pos].xform.baseAngle;
		}
	}

	void SetBase(int pos, double y, DAngle o)
	{
		planes[pos].xform.baseyOffs = y;
		planes[pos].xform.baseAngle = o;
	}

	void SetAlpha(int pos, double o)
	{
		planes[pos].alpha = o;
	}

	double GetAlpha(int pos) const
	{
		return planes[pos].alpha;
	}

	int GetFlags(int pos) const
	{
		return planes[pos].Flags;
	}

	// like the previous one but masks out all flags which are not relevant for rendering.
	int GetVisFlags(int pos) const
	{
		return planes[pos].Flags & ~(PLANEF_BLOCKED | PLANEF_NOPASS | PLANEF_BLOCKSOUND | PLANEF_LINKED);
	}

	void ChangeFlags(int pos, int And, int Or)
	{
		planes[pos].Flags &= ~And;
		planes[pos].Flags |= Or;
	}

	int GetPlaneLight(int pos) const 
	{
		return planes[pos].Light;
	}

	void SetPlaneLight(int pos, int level)
	{
		planes[pos].Light = level;
	}

	double GetGlowHeight(int pos)
	{
		return planes[pos].GlowHeight;
	}

	PalEntry GetGlowColor(int pos)
	{
		return planes[pos].GlowColor;
	}

	void SetGlowHeight(int pos, float height)
	{
		planes[pos].GlowHeight = height;
	}

	void SetGlowColor(int pos, PalEntry color)
	{
		planes[pos].GlowColor = color;
	}

	FTextureID GetTexture(int pos) const
	{
		return planes[pos].Texture;
	}

	void SetTexture(int pos, FTextureID tex, bool floorclip = true)
	{
		FTextureID old = planes[pos].Texture;
		planes[pos].Texture = tex;
		if (floorclip && pos == floor && tex != old) AdjustFloorClip();
	}

	double GetPlaneTexZ(int pos) const
	{
		return planes[pos].TexZ;
	}

	void SetPlaneTexZQuick(int pos, double val)	// For the *FakeFlat functions which do not need to have the overlap checked.
	{
		planes[pos].TexZ = val;
	}

	void SetPlaneTexZ(int pos, double val, bool dirtify = false)	// This mainly gets used by init code. The only place where it must set the vertex to dirty is the interpolation code.
	{
		planes[pos].TexZ = val;
		if (dirtify) SetAllVerticesDirty();
		CheckOverlap();
	}

	void ChangePlaneTexZ(int pos, double val)
	{
		planes[pos].TexZ += val;
		SetAllVerticesDirty();
		CheckOverlap();
	}

	static inline short ClampLight(int level)
	{
		return (short)clamp(level, SHRT_MIN, SHRT_MAX);
	}

	void ChangeLightLevel(int newval)
	{
		lightlevel = ClampLight(lightlevel + newval);
	}

	void SetLightLevel(int newval)
	{
		lightlevel = ClampLight(newval);
	}

	int GetLightLevel() const
	{
		return lightlevel;
	}

	secplane_t &GetSecPlane(int pos)
	{
		return pos == floor? floorplane:ceilingplane;
	}

	bool isSecret() const
	{
		return !!(Flags & SECF_SECRET);
	}

	bool wasSecret() const
	{
		return !!(Flags & SECF_WASSECRET);
	}

	void ClearSecret()
	{
		Flags &= ~SECF_SECRET;
	}

	void ClearSpecial()
	{
		// clears all variables that originate from 'special'. Used for sector type transferring thinkers
		special = 0;
		damageamount = 0;
		damageinterval = 0;
		damagetype = NAME_None;
		leakydamage = 0;
		Flags &= ~SECF_SPECIALFLAGS;
	}

	void SetSpecialColor(int slot, int r, int g, int b)
	{
		SpecialColors[slot] = PalEntry(255, r, g, b);
	}

	void SetSpecialColor(int slot, PalEntry rgb)
	{
		rgb.a = 255;
		SpecialColors[slot] = rgb;
	}

	void SetAdditiveColor(int slot, PalEntry rgb)
	{
		rgb.a = 255;
		AdditiveColors[slot] = rgb;
	}

	inline bool PortalBlocksView(int plane);
	inline bool PortalBlocksSight(int plane);
	inline bool PortalBlocksMovement(int plane);
	inline bool PortalBlocksSound(int plane);
	inline bool PortalIsLinked(int plane);

	void ClearPortal(int plane)
	{
		Portals[plane] = 0;
		portals[plane] = nullptr;
	}

	FSectorPortal *GetPortal(int plane);
	double GetPortalPlaneZ(int plane);
	DVector2 GetPortalDisplacement(int plane);
	int GetPortalType(int plane);
	int GetOppositePortalGroup(int plane);
	void CheckOverlap();

	void SetVerticesDirty()
	{
		for (unsigned i = 0; i < e->vertices.Size(); i++) e->vertices[i]->dirty = true;
	}

	void SetAllVerticesDirty()
	{
		SetVerticesDirty();
		for (unsigned i = 0; i < e->FakeFloor.Sectors.Size(); i++) e->FakeFloor.Sectors[i]->SetVerticesDirty();
		for (unsigned i = 0; i < e->XFloor.attached.Size(); i++) e->XFloor.attached[i]->SetVerticesDirty();
	}

	int GetTerrain(int pos) const;

	void TransferSpecial(sector_t *model);
	void GetSpecial(secspecial_t *spec);
	void SetSpecial(const secspecial_t *spec);
	bool PlaneMoving(int pos);

	inline double HighestCeilingAt(AActor *a, sector_t **resultsec = NULL);
	inline double LowestFloorAt(AActor *a, sector_t **resultsec = NULL);

	bool isClosed() const
	{
		return floorplane.Normal() == -ceilingplane.Normal() && floorplane.D == -ceilingplane.D;
	}

	// Member variables
	double		CenterFloor() const { return floorplane.ZatPoint(centerspot); }
	double		CenterCeiling() const { return ceilingplane.ZatPoint(centerspot); }

	// [RH] store floor and ceiling planes instead of heights
	secplane_t	floorplane, ceilingplane;

	// [RH] give floor and ceiling even more properties
	PalEntry SpecialColors[5];
	PalEntry AdditiveColors[5];
	FColormap Colormap;

private:
	FDynamicColormap *_ColorMap;	// [RH] Per-sector colormap

public:
	// just a helper for refactoring 
	FDynamicColormap *GetColorMap()
	{
		return _ColorMap;
	}

	void CopyColors(sector_t *other)
	{
		memcpy(SpecialColors, other->SpecialColors, sizeof(SpecialColors));
		Colormap = other->Colormap;

		_ColorMap = other->_ColorMap;
	}



	TObjPtr<AActor*> SoundTarget;

	short		special;
	short		lightlevel;
	short		seqType;		// this sector's sound sequence

	int			sky;
	FName	SeqName;		// Sound sequence name. Setting seqType non-negative will override this.

	DVector2	centerspot;		// origin for any sounds played by the sector
	int 		validcount;		// if == validcount, already checked
	AActor* 	thinglist;		// list of mobjs in sector

	// killough 8/28/98: friction is a sector property, not an mobj property.
	// these fields used to be in AActor, but presented performance problems
	// when processed as mobj properties. Fix is to make them sector properties.
	double		friction, movefactor;

	int			terrainnum[2];

	// thinker_t for reversable actions
	TObjPtr<DSectorEffect*> floordata;			// jff 2/22/98 make thinkers on
	TObjPtr<DSectorEffect*> ceilingdata;			// floors, ceilings, lighting,
	TObjPtr<DSectorEffect*> lightingdata;		// independent of one another

	enum
	{
		CeilingMove,
		FloorMove,
		CeilingScroll,
		FloorScroll
	};
	TObjPtr<DInterpolation*> interpolations[4];

	int prevsec;		// -1 or number of sector for previous step
	int nextsec;		// -1 or number of next step sector
	uint8_t 		soundtraversed;	// 0 = untraversed, 1,2 = sndlines -1
	// jff 2/26/98 lockout machinery for stairbuilding
	int8_t stairlock;	// -2 on first locked -1 after thinker done 0 normally

	TStaticPointedArray<line_t *> Lines;

	// killough 3/7/98: support flat heights drawn at another sector's heights
	sector_t *heightsec;		// other sector, or NULL if no other sector
	FLightNode *				lighthead;

	uint32_t bottommap, midmap, topmap;	// killough 4/4/98: dynamic colormaps
										// [RH] these can also be blend values if
										//		the alpha mask is non-zero

	// list of mobjs that are at least partially in the sector
	// thinglist is a subset of touching_thinglist
	struct msecnode_t *touching_thinglist;				// phares 3/14/98
	struct msecnode_t *sectorportal_thinglist;				// for cross-portal rendering.
	struct msecnode_t *touching_renderthings; // this is used to allow wide things to be rendered not only from their main sector.

	double gravity;			// [RH] Sector gravity (1.0 is normal)
	FName damagetype;		// [RH] Means-of-death for applied damage
	int damageamount;			// [RH] Damage to do while standing on floor
	short damageinterval;	// Interval for damage application
	short leakydamage;		// chance of leaking through radiation suit

	uint16_t ZoneNumber;	// [RH] Zone this sector belongs to
	uint16_t MoreFlags;		// [RH] Internal sector flags
	uint32_t Flags;		// Sector flags

	// [RH] Action specials for sectors. Like Skull Tag, but more
	// flexible in a Bloody way. SecActTarget forms a list of actors
	// joined by their tracer fields. When a potential sector action
	// occurs, SecActTarget's TriggerAction method is called.
	TObjPtr<AActor*> SecActTarget;

	// [RH] The portal or skybox to render for this sector.
	unsigned Portals[2];
	int PortalGroup;

	int							sectornum;			// for comparing sector copies

	extsector_t	*				e;		// This stores data that requires construction/destruction. Such data must not be copied by R_FakeFlat.

	// GL only stuff starts here
	float						reflect[2];

	bool						transdoor;			// For transparent door hacks
	int							subsectorcount;		// list of subsectors
	double						transdoorheight;	// for transparent door hacks
	subsector_t **				subsectors;
	FPortal *					portals[2];			// floor and ceiling portals

	enum
	{
		vbo_fakefloor = floor+2,
		vbo_fakeceiling = ceiling+2,
	};

	int				vboindex[4];	// VBO indices of the 4 planes this sector uses during rendering
#ifdef USE_GL_HW_BUFFERS
    double			vboheight[MAX_HW_BUFFERS][2];	// Last calculated height for the 2 planes of this actual sector
#else
	double			vboheight[2];	// Last calculated height for the 2 planes of this actual sector
#endif
	int				vbocount[2];	// Total count of vertices belonging to this sector's planes

	float GetReflect(int pos) { return gl_plane_reflection_i? reflect[pos] : 0; }
#ifdef USE_GL_HW_BUFFERS
	bool VBOHeightcheck(int buffer,int pos) const { return vboheight[buffer][pos] == GetPlaneTexZ(pos); }
	#define VBOHeightcheck(X) VBOHeightcheck(GLRenderer->VtxBuff,X)
	#define vboheight vboheight[GLRenderer->VtxBuff]
#else
	bool VBOHeightcheck(int pos) const { return vboheight[pos] == GetPlaneTexZ(pos); }
#endif
	FPortal *GetGLPortal(int plane) { return portals[plane]; }

	enum
	{
		INVALIDATE_PLANES = 1,
		INVALIDATE_OTHER = 2
	};

	// [ZZ] these are for destructible sectors.
	//      default is 0, which means no special behavior
	int				healthfloor;
	int				healthceiling;
	int				health3d;
	int				healthfloorgroup;
	int				healthceilinggroup;
	int				health3dgroup;

};

struct ReverbContainer;
struct zone_t
{
	ReverbContainer *Environment;
};


//
// The SideDef.
//

class DBaseDecal;

enum
{
	WALLF_ABSLIGHTING	 = 1,	// Light is absolute instead of relative
	WALLF_NOAUTODECALS	 = 2,	// Do not attach impact decals to this wall
	WALLF_NOFAKECONTRAST = 4,	// Don't do fake contrast for this wall in side_t::GetLightLevel
	WALLF_SMOOTHLIGHTING = 8,   // Similar to autocontrast but applies to all angles.
	WALLF_CLIP_MIDTEX	 = 16,	// Like the line counterpart, but only for this side.
	WALLF_WRAP_MIDTEX	 = 32,	// Like the line counterpart, but only for this side.
	WALLF_POLYOBJ		 = 64,	// This wall belongs to a polyobject.
	WALLF_LIGHT_FOG      = 128,	// This wall's Light is used even in fog.
};

struct side_t
{
	enum ETexpart
	{
		top=0,
		mid=1,
		bottom=2,
		none = 1,	// this is just for clarification in a mapping table
	};
	enum EColorSlot
	{
		walltop = 0,
		wallbottom = 1,
	};
	struct part
	{
		enum EPartFlags
		{
			NoGradient = 1,
			FlipGradient = 2,
			ClampGradient = 4,
			UseOwnSpecialColors = 8,
			UseOwnAdditiveColor = 16,
		};
		double xOffset;
		double yOffset;
		double xScale;
		double yScale;
		TObjPtr<DInterpolation*> interpolation;
		FTextureID texture;
		int flags;
		PalEntry SpecialColors[2];
		PalEntry AdditiveColor;

		void InitFrom(const part &other)
		{
			if (texture.isNull()) texture = other.texture;
			if (0.0 == xOffset) xOffset = other.xOffset;
			if (0.0 == yOffset) yOffset = other.yOffset;
			if (1.0 == xScale && 0.0 != other.xScale) xScale = other.xScale;
			if (1.0 == yScale && 0.0 != other.yScale) yScale = other.yScale;
		}
	};

	sector_t*	sector;			// Sector the SideDef is facing.
	DBaseDecal*	AttachedDecals;	// [RH] Decals bound to the wall
	part		textures[3];
	line_t		*linedef;
	uint32_t	LeftSide, RightSide;	// [RH] Group walls into loops
	uint16_t	TexelLength;
	int16_t		Light;
	uint8_t		Flags;
	int			UDMFIndex;		// needed to access custom UDMF fields which are stored in loading order.
	FLightNode * lighthead;		// all dynamic lights that may affect this wall

	int GetLightLevel (bool foggy, int baselight, bool is3dlight=false, int *pfakecontrast_usedbygzdoom=NULL) const;

	void SetLight(int16_t l)
	{
		Light = l;
	}

	FTextureID GetTexture(int which) const
	{
		return textures[which].texture;
	}
	void SetTexture(int which, FTextureID tex)
	{
		textures[which].texture = tex;
	}

	void SetTextureXOffset(int which, double offset)
	{
		textures[which].xOffset = offset;;
	}
	
	void SetTextureXOffset(double offset)
	{
		textures[top].xOffset =
		textures[mid].xOffset =
		textures[bottom].xOffset = offset;
	}

	double GetTextureXOffset(int which) const
	{
		return textures[which].xOffset;
	}

	void AddTextureXOffset(int which, double delta)
	{
		textures[which].xOffset += delta;
	}

	void SetTextureYOffset(int which, double offset)
	{
		textures[which].yOffset = offset;
	}

	void SetTextureYOffset(double offset)
	{
		textures[top].yOffset =
		textures[mid].yOffset =
		textures[bottom].yOffset = offset;
	}

	double GetTextureYOffset(int which) const
	{
		return textures[which].yOffset;
	}

	void AddTextureYOffset(int which, double delta)
	{
		textures[which].yOffset += delta;
	}

	void SetTextureXScale(int which, double scale)
	{
		textures[which].xScale = scale == 0 ? 1. : scale;
	}

	void SetTextureXScale(double scale)
	{
		textures[top].xScale = textures[mid].xScale = textures[bottom].xScale = scale == 0 ? 1. : scale;
	}

	double GetTextureXScale(int which) const
	{
		return textures[which].xScale;
	}

	void MultiplyTextureXScale(int which, double delta)
	{
		textures[which].xScale *= delta;
	}

	void SetTextureYScale(int which, double scale)
	{
		textures[which].yScale = scale == 0 ? 1. : scale;
	}

	void SetTextureYScale(double scale)
	{
		textures[top].yScale = textures[mid].yScale = textures[bottom].yScale = scale == 0 ? 1. : scale;
	}

	double GetTextureYScale(int which) const
	{
		return textures[which].yScale;
	}

	void MultiplyTextureYScale(int which, double delta)
	{
		textures[which].yScale *= delta;
	}

	void SetSpecialColor(int which, int slot, int r, int g, int b)
	{
		textures[which].SpecialColors[slot] = PalEntry(255, r, g, b);
	}

	void SetSpecialColor(int which, int slot, PalEntry rgb)
	{
		rgb.a = 255;
		textures[which].SpecialColors[slot] = rgb;
	}

	// Note that the sector being passed in here may not be the actual sector this sidedef belongs to
	// (either for polyobjects or FakeFlat'ed temporaries.)
	PalEntry GetSpecialColor(int which, int slot, sector_t *frontsector) const
	{
		auto &part = textures[which];
		if (part.flags & part::NoGradient) slot = 0;
		if (part.flags & part::FlipGradient) slot ^= 1;
		return (part.flags & part::UseOwnSpecialColors) ? part.SpecialColors[slot] : frontsector->SpecialColors[sector_t::walltop + slot];
	}

	void EnableAdditiveColor(int which, bool enable)
	{
		int flag = enable ? part::UseOwnAdditiveColor : 0;
		if (enable)
		{
			textures[which].flags |= flag;
		}
		else
		{
			textures[which].flags &= (~flag);
		}
	}

	void SetAdditiveColor(int which, PalEntry rgb)
	{
		rgb.a = 255;
		textures[which].AdditiveColor = rgb;
	}

	PalEntry GetAdditiveColor(int which, sector_t *frontsector) const
	{
		if (textures[which].flags & part::UseOwnAdditiveColor) {
			return textures[which].AdditiveColor;
		}
		else
		{
			return frontsector->AdditiveColors[sector_t::walltop]; // Used as additive color for all walls
		}
	}

	DInterpolation *SetInterpolation(int position);
	void StopInterpolation(int position);

	vertex_t *V1() const;
	vertex_t *V2() const;

	int Index() const;

	//For GL

	seg_t **segs;	// all segs belonging to this sidedef in ascending order. Used for precise rendering
	int numsegs;

};

enum AutomapLineStyle : int
{
	AMLS_Default,
	AMLS_OneSided,
	AMLS_TwoSided,
	AMLS_FloorDiff,
	AMLS_CeilingDiff,
	AMLS_ExtraFloor,
	AMLS_Special,
	AMLS_Secret,
	AMLS_NotSeen,
	AMLS_Locked,
	AMLS_IntraTeleport,
	AMLS_InterTeleport,
	AMLS_UnexploredSecret,
	AMLS_Portal,

	AMLS_COUNT
};

struct line_t
{
	vertex_t	*v1, *v2;	// vertices, from v1 to v2
	DVector2	delta;		// precalculated v2 - v1 for side checking
	uint32_t	flags;
	uint32_t	activation;	// activation type
	int			special;
	int			args[5];	// <--- hexen-style arguments (expanded to ZDoom's full width)
	double		alpha;		// <--- translucency (0=invisibile, FRACUNIT=opaque)
	side_t		*sidedef[2];
	double		bbox[4];	// bounding box, for the extent of the LineDef.
	sector_t	*frontsector, *backsector;
	int 		validcount;	// if == validcount, already checked
	int			locknumber;	// [Dusk] lock number for special
	unsigned	portalindex;
	unsigned	portaltransferred;
	AutomapLineStyle automapstyle;
	int			health;		// [ZZ] for destructible geometry (0 = no special behavior)
	int			healthgroup; // [ZZ] this is the "destructible object" id

	DVector2 Delta() const
	{
		return delta;
	}

	void setDelta(double x, double y)
	{
		delta = { x, y };
	}

	void setAlpha(double a)
	{
		alpha = a;
	}

	FSectorPortal *GetTransferredPortal();

	inline FLinePortal *getPortal() const;
	inline bool isLinePortal() const;
	inline bool isVisualPortal() const;
	inline line_t *getPortalDestination() const;
	inline int getPortalAlignment() const;
	inline bool hitSkyWall(AActor* mo) const;

	int Index() const;
};

inline vertex_t *side_t::V1() const
{
	return this == linedef->sidedef[0] ? linedef->v1 : linedef->v2;
}

inline vertex_t *side_t::V2() const
{
	return this == linedef->sidedef[0] ? linedef->v2 : linedef->v1;
}

// phares 3/14/98
//
// Sector list node showing all sectors an object appears in.
//
// There are two threads that flow through these nodes. The first thread
// starts at touching_thinglist in a sector_t and flows through the m_snext
// links to find all mobjs that are entirely or partially in the sector.
// The second thread starts at touching_sectorlist in a AActor and flows
// through the m_tnext links to find all sectors a thing touches. This is
// useful when applying friction or push effects to sectors. These effects
// can be done as thinkers that act upon all objects touching their sectors.
// As an mobj moves through the world, these nodes are created and
// destroyed, with the links changed appropriately.
//
// For the links, NULL means top or end of list.

struct msecnode_t
{
	sector_t			*m_sector;	// a sector containing this object
	AActor				*m_thing;	// this object
	struct msecnode_t	*m_tprev;	// prev msecnode_t for this thing
	struct msecnode_t	*m_tnext;	// next msecnode_t for this thing
	struct msecnode_t	*m_sprev;	// prev msecnode_t for this sector
	struct msecnode_t	*m_snext;	// next msecnode_t for this sector
	bool visited;	// killough 4/4/98, 4/7/98: used in search algorithms
};

// use the same memory layout as msecnode_t so both can be used from the same freelist.
struct portnode_t
{
	FLinePortal			*m_sector;	// a portal containing this object (no, this isn't a sector, but if we want to use templates it needs the same variable names as msecnode_t.)
	AActor				*m_thing;	// this object
	struct portnode_t	*m_tprev;	// prev msecnode_t for this thing
	struct portnode_t	*m_tnext;	// next msecnode_t for this thing
	struct portnode_t	*m_sprev;	// prev msecnode_t for this portal
	struct portnode_t	*m_snext;	// next msecnode_t for this portal
	bool visited;
};

struct FPolyNode;
struct FMiniBSP;

//
// The LineSeg.
//
struct seg_t
{
	vertex_t*	v1;
	vertex_t*	v2;
	
	side_t* 	sidedef;
	line_t* 	linedef;

	// Sector references. Could be retrieved from linedef, too.
	sector_t*		frontsector;
	sector_t*		backsector;		// NULL for one-sided lines

	seg_t*			PartnerSeg;
	subsector_t*	Subsector;

	float			sidefrac;		// relative position of seg's ending vertex on owning sidedef

	int Index() const;
};

//extern seg_t *segs;


//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs indicating the visible walls that
// define (all or some) sides of a convex BSP leaf.
//

enum
{
	SSECF_DEGENERATE = 1,
	SSECMF_DRAWN = 2,
	SSECF_POLYORG = 4,
};

struct FPortalCoverage
{
	uint32_t *		subsectors;
	int			sscount;
};

struct subsector_t
{
	sector_t	*sector;
	FPolyNode	*polys;
	FMiniBSP	*BSP;
	seg_t		*firstline;
	sector_t	*render_sector;
	uint32_t	numlines;
	int			flags;

	void BuildPolyBSP();
	int Index() const;
	// subsector related GL data
	FLightNode *	lighthead;	// Light nodes (blended and additive)
	int				validcount;
	short			mapsection;
	char			hacked;			// 1: is part of a render hack
									// 2: has one-sided walls
	FPortalCoverage	portalcoverage[2];
};


	

//
// BSP node.
//
struct node_t
{
	// Partition line.
	fixed_t		x;
	fixed_t		y;
	fixed_t		dx;
	fixed_t		dy;
	union
	{
		float	bbox[2][4];		// Bounding box for each child.
		fixed_t	nb_bbox[2][4];	// Used by nodebuilder.
	};
	float		len;
	union
	{
		void	*children[2];	// If bit 0 is set, it's a subsector.
		int		intchildren[2];	// Used by nodebuilder.
	};

	int Index() const;
};


// An entire BSP tree.

struct FMiniBSP
{
	bool bDirty;

	TArray<node_t> Nodes;
	TArray<seg_t> Segs;
	TArray<subsector_t> Subsectors;
	TArray<vertex_t> Verts;
};

//
// OTHER TYPES
//

typedef uint8_t lighttable_t;	// This could be wider for >8 bit display.

//----------------------------------------------------------------------------------
//
// The playsim can use different nodes than the renderer so this is
// not the same as R_PointInSubsector
//
//----------------------------------------------------------------------------------
subsector_t *P_PointInSubsector(double x, double y);

inline sector_t *P_PointInSector(const DVector2 &pos)
{
	return P_PointInSubsector(pos.X, pos.Y)->sector;
}

inline sector_t *P_PointInSector(double X, double Y)
{
	return P_PointInSubsector(X, Y)->sector;
}

inline sector_t *P_PointInSectorXY(double X, double Y)	// This is for the benefit of unambiguously looking up this function's address
{
	return P_PointInSubsector(X, Y)->sector;
}

inline bool FBoundingBox::inRange(const line_t *ld) const
{
	return Left() < ld->bbox[BOXRIGHT] &&
		Right() > ld->bbox[BOXLEFT] &&
		Top() > ld->bbox[BOXBOTTOM] &&
		Bottom() < ld->bbox[BOXTOP];
}


inline void FColormap::CopyFrom3DLight(lightlist_t *light)
{
	CopyLight(light->extra_colormap);
	if (light->caster && (light->caster->flags&FF_FADEWALLS) && light->extra_colormap.FadeColor != 0)
	{
		CopyFog(light->extra_colormap);
	}
}

double FindLowestFloorSurrounding(const sector_t *sec, vertex_t **v);
double FindHighestFloorSurrounding(const sector_t *sec, vertex_t **v);
double FindNextHighestFloor(const sector_t *sec, vertex_t **v);
double FindNextLowestFloor(const sector_t *sec, vertex_t **v);
double FindLowestCeilingSurrounding(const sector_t *sec, vertex_t **v);			// jff 2/04/98
double FindHighestCeilingSurrounding(const sector_t *sec, vertex_t **v);			// jff 2/04/98
double FindNextLowestCeiling(const sector_t *sec, vertex_t **v);					// jff 2/04/98
double FindNextHighestCeiling(const sector_t *sec, vertex_t **v);					// jff 2/04/98
int FindMinSurroundingLight (const sector_t *sec, int max);
double FindHighestFloorPoint(const sector_t *sec, vertex_t **v);

double FindShortestTextureAround(sector_t *sector);					// jff 2/04/98
double FindShortestUpperAround(sector_t *sector);					// jff 2/04/98
sector_t *FindModelFloorSector(sector_t *sec, double floordestheight);		// jff 2/04/98
sector_t *FindModelCeilingSector(sector_t *sec, double floordestheight);		// jff 2/04/98
double FindLowestCeilingPoint(const sector_t *sec, vertex_t **v);

double NextHighestCeilingAt(sector_t *sec, double x, double y, double bottomz, double topz, int flags = 0, sector_t **resultsec = NULL, F3DFloor **resultffloor = NULL);
double NextLowestFloorAt(sector_t *sec, double x, double y, double z, int flags = 0, double steph = 0, sector_t **resultsec = NULL, F3DFloor **resultffloor = NULL);

// This setup is to allow the VM call directily into the implementation.
// With a member function this may be subject to OS implementation details, e.g. on Windows 32 bit members use a different calling convention than regular functions.
void RemoveForceField(sector_t *sec);
int PlaneMoving(sector_t *sector, int pos);
void TransferSpecial(sector_t *self, sector_t *model);
void GetSpecial(sector_t *self, secspecial_t *spec);
void SetSpecial(sector_t *self, const secspecial_t *spec);
int GetTerrain(const sector_t *, int pos);
void CheckPortalPlane(sector_t *sector, int plane);
void AdjustFloorClip(const sector_t *sector);
void SetColor(sector_t *sector, int color, int desat);
void SetFade(sector_t *sector, int color);
int GetFloorLight(const sector_t *);
int GetCeilingLight(const sector_t *);
double GetFriction(const sector_t *self, int plane, double *movefac);
double HighestCeilingAt(sector_t *sec, double x, double y, sector_t **resultsec = nullptr);
double LowestFloorAt(sector_t *sec, double x, double y, sector_t **resultsec = nullptr);

inline void sector_t::RemoveForceField() { return ::RemoveForceField(this); }
inline bool sector_t::PlaneMoving(int pos) { return !!::PlaneMoving(this, pos); }
inline void sector_t::TransferSpecial(sector_t *model) { return ::TransferSpecial(this, model); }
inline void sector_t::GetSpecial(secspecial_t *spec) { ::GetSpecial(this, spec); }
inline void sector_t::SetSpecial(const secspecial_t *spec) { ::SetSpecial(this, spec); }
inline int sector_t::GetTerrain(int pos) const { return ::GetTerrain(this, pos); }
inline void sector_t::CheckPortalPlane(int plane) { return ::CheckPortalPlane(this, plane); }
inline void sector_t::AdjustFloorClip() const { ::AdjustFloorClip(this); }
inline void sector_t::SetColor(PalEntry pe, int desat) { ::SetColor(this, pe, desat); }
inline void sector_t::SetFade(PalEntry pe) { ::SetFade(this, pe); }
inline int sector_t::GetFloorLight() const { return ::GetFloorLight(this); }
inline int sector_t::GetCeilingLight() const { return ::GetCeilingLight(this); }
inline double sector_t::GetFriction(int plane, double *movefac) const { return ::GetFriction(this, plane, movefac); }


#endif
