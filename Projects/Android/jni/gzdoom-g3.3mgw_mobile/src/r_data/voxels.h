#ifndef __RES_VOXEL_H
#define __RES_VOXEL_H

#include "doomdef.h"

// [RH] Voxels from Build

#define MAXVOXMIPS 5

struct kvxslab_t
{
	uint8_t		ztop;			// starting z coordinate of top of slab
	uint8_t		zleng;			// # of bytes in the color array - slab height
	uint8_t		backfacecull;	// low 6 bits tell which of 6 faces are exposed
	uint8_t		col[1/*zleng*/];// color data from top to bottom
};

struct kvxslab_bgra_t
{
	uint32_t	ztop;			// starting z coordinate of top of slab
	uint32_t	zleng;			// # of bytes in the color array - slab height
	uint32_t	backfacecull;	// low 6 bits tell which of 6 faces are exposed
	uint32_t	col[1/*zleng*/];// color data from top to bottom
};

struct FVoxelMipLevel
{
	FVoxelMipLevel();
	~FVoxelMipLevel();

	int			SizeX;
	int			SizeY;
	int			SizeZ;
	DVector3	Pivot;
	int			*OffsetX;
	short		*OffsetXY;
	uint8_t		*SlabData;
	TArray<uint32_t> SlabDataBgra;
};

struct FVoxel
{
	TArray<uint8_t> Palette;
	int LumpNum;
	int NumMips;
	int VoxelIndex;			// Needed by GZDoom
	FVoxelMipLevel Mips[MAXVOXMIPS];

	void CreateBgraSlabData();
	void Remap();
	void RemovePalette();
};

struct FVoxelDef
{
	FVoxel *Voxel;
	int PlacedSpin;			// degrees/sec to spin actors without MF_DROPPED set
	int DroppedSpin;		// degrees/sec to spin actors with MF_DROPPED set
	int VoxeldefIndex;		// Needed by GZDoom
	double		Scale;
	DAngle		AngleOffset;// added to actor's angle to compensate for wrong-facing voxels
};

extern TDeletingArray<FVoxel *> Voxels;	// used only to auto-delete voxels on exit.
extern TDeletingArray<FVoxelDef *> VoxelDefs;

FVoxel *R_LoadKVX(int lumpnum);
FVoxelDef *R_LoadVoxelDef(int lumpnum, int spin);
void R_InitVoxels();

#endif
