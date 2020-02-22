/*
** textures.h
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
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
*/

#ifndef __TEXTURES_H
#define __TEXTURES_H

#include "doomtype.h"
#include "vectors.h"
#include "v_palette.h"
#include "v_video.h"
#include "colormatcher.h"
#include "r_data/renderstyle.h"
#include "r_data/r_translate.h"
#include <vector>

// 15 because 0th texture is our texture
#define MAX_CUSTOM_HW_SHADER_TEXTURES 15

enum MaterialShaderIndex
{
	SHADER_Default,
	SHADER_Warp1,
	SHADER_Warp2,
	SHADER_Brightmap,
	SHADER_Specular,
	SHADER_SpecularBrightmap,
	SHADER_PBR,
	SHADER_PBRBrightmap,
	SHADER_NoTexture,
	SHADER_BasicFuzz,
	SHADER_SmoothFuzz,
	SHADER_SwirlyFuzz,
	SHADER_TranslucentFuzz,
	SHADER_JaggedFuzz,
	SHADER_NoiseFuzz,
	SHADER_SmoothNoiseFuzz,
	SHADER_SoftwareFuzz,
	FIRST_USER_SHADER
};

struct UserShaderDesc
{
	FString shader;
	MaterialShaderIndex shaderType;
	FString defines;
	bool disablealphatest = false;
};

extern TArray<UserShaderDesc> usershaders;


struct FloatRect
{
	float left,top;
	float width,height;


	void Offset(float xofs,float yofs)
	{
		left+=xofs;
		top+=yofs;
	}
	void Scale(float xfac,float yfac)
	{
		left*=xfac;
		width*=xfac;
		top*=yfac;
		height*=yfac;
	}
};


class FBitmap;
struct FRemapTable;
struct FCopyInfo;
class FScanner;

// Texture IDs
class FTextureManager;
class FTerrainTypeArray;
class FGLTexture;
class FMaterial;

class FNullTextureID : public FTextureID
{
public:
	FNullTextureID() : FTextureID(0) {}
};

//
// Animating textures and planes
//
// [RH] Expanded to work with a Hexen ANIMDEFS lump
//

struct FAnimDef
{
	FTextureID 	BasePic;
	uint16_t	NumFrames;
	uint16_t	CurFrame;
	uint8_t	AnimType;
	bool	bDiscrete;			// taken out of AnimType to have better control
	uint64_t	SwitchTime;			// Time to advance to next frame
	struct FAnimFrame
	{
		uint32_t	SpeedMin;		// Speeds are in ms, not tics
		uint32_t	SpeedRange;
		FTextureID	FramePic;
	} Frames[1];
	enum
	{
		ANIM_Forward,
		ANIM_Backward,
		ANIM_OscillateUp,
		ANIM_OscillateDown,
		ANIM_Random
	};

	void SetSwitchTime (uint64_t mstime);
};

struct FSwitchDef
{
	FTextureID PreTexture;		// texture to switch from
	FSwitchDef *PairDef;		// switch def to use to return to PreTexture
	uint16_t NumFrames;		// # of animation frames
	bool QuestPanel;	// Special texture for Strife mission
	int Sound;			// sound to play at start of animation. Changed to int to avoiud having to include s_sound here.
	struct frame		// Array of times followed by array of textures
	{					//   actual length of each array is <NumFrames>
		uint16_t TimeMin;
		uint16_t TimeRnd;
		FTextureID Texture;
	} frames[1];
};

struct FDoorAnimation
{
	FTextureID BaseTexture;
	FTextureID *TextureFrames;
	int NumTextureFrames;
	FName OpenSound;
	FName CloseSound;
};

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures, and we compose
// textures from the TEXTURE1/2 lists of patches.
struct patch_t
{ 
	int16_t			width;			// bounding box size 
	int16_t			height; 
	int16_t			leftoffset; 	// pixels to the left of origin 
	int16_t			topoffset;		// pixels below the origin 
	uint32_t 			columnofs[];	// only [width] used
	// the [0] is &columnofs[width] 
};

// All FTextures present their data to the world in 8-bit format, but if
// the source data is something else, this is it.
enum FTextureFormat : uint32_t
{
	TEX_Pal,
	TEX_Gray,
	TEX_RGB,		// Actually ARGB
	/*
	TEX_DXT1,
	TEX_DXT2,
	TEX_DXT3,
	TEX_DXT4,
	TEX_DXT5,
	*/
	TEX_Count
};

class FNativeTexture;



// Base texture class
class FTexture
{
public:
	static FTexture *CreateTexture(const char *name, int lumpnum, ETextureType usetype);
	static FTexture *CreateTexture(int lumpnum, ETextureType usetype);
	virtual ~FTexture ();

	int16_t LeftOffset, TopOffset;

	uint8_t WidthBits, HeightBits;

	DVector2 Scale;

	int SourceLump;
	FTextureID id;

	FTexture *CustomShaderTextures[MAX_CUSTOM_HW_SHADER_TEXTURES] = { nullptr }; // Custom texture maps for custom hardware shaders

	FString Name;
	ETextureType UseType;	// This texture's primary purpose

	uint8_t bNoDecals:1;		// Decals should not stick to texture
	uint8_t bNoRemap0:1;		// Do not remap color 0 (used by front layer of parallax skies)
	uint8_t bWorldPanning:1;	// Texture is panned in world units rather than texels
	uint8_t bMasked:1;			// Texture (might) have holes
	uint8_t bAlphaTexture:1;	// Texture is an alpha channel without color information
	uint8_t bHasCanvas:1;		// Texture is based off FCanvasTexture
	uint8_t bWarped:2;			// This is a warped texture. Used to avoid multiple warps on one texture
	uint8_t bComplex:1;		// Will be used to mark extended MultipatchTextures that have to be
							// fully composited before subjected to any kind of postprocessing instead of
							// doing it per patch.
	uint8_t bMultiPatch:2;		// This is a multipatch texture (we really could use real type info for textures...)
	uint8_t bKeepAround:1;		// This texture was used as part of a multi-patch texture. Do not free it.

	uint16_t Rotations;
	int16_t SkyOffset;


	struct Span
	{
		uint16_t TopOffset;
		uint16_t Length;	// A length of 0 terminates this column
	};

	// Returns a single column of the texture
	virtual const uint8_t *GetColumn(FRenderStyle style, unsigned int column, const Span **spans_out) = 0;

	// Returns a single column of the texture, in BGRA8 format
	virtual const uint32_t *GetColumnBgra(unsigned int column, const Span **spans_out);

	// Returns the whole texture, stored in column-major order
	virtual const uint8_t *GetPixels(FRenderStyle style) = 0;

	// Returns the whole texture, stored in column-major order, in BGRA8 format
	virtual const uint32_t *GetPixelsBgra();

	// Returns true if GetPixelsBgra includes mipmaps
	virtual bool Mipmapped() { return true; }

	virtual int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate=0, FCopyInfo *inf = NULL);
	int CopyTrueColorTranslated(FBitmap *bmp, int x, int y, int rotate, PalEntry *remap, FCopyInfo *inf = NULL);
	virtual bool UseBasePalette();
	virtual int GetSourceLump() { return SourceLump; }
	virtual FTexture *GetRedirect(bool wantwarped);
	virtual FTexture *GetRawTexture();		// for FMultiPatchTexture to override

	virtual void Unload ();

	// Returns the native pixel format for this image
	virtual FTextureFormat GetFormat();

	// Returns a native 3D representation of the texture
	FNativeTexture *GetNative(FTextureFormat fmt, bool wrapping);

	// Frees the native 3D representation of the texture
	void KillNative();

	// Fill the native texture buffer with pixel data for this image
	virtual void FillBuffer(uint8_t *buff, int pitch, int height, FTextureFormat fmt);

	int GetWidth () { return Width; }
	int GetHeight () { return Height; }

	int GetScaledWidth () { int foo = int((Width * 2) / Scale.X); return (foo >> 1) + (foo & 1); }
	int GetScaledHeight () { int foo = int((Height * 2) / Scale.Y); return (foo >> 1) + (foo & 1); }
	double GetScaledWidthDouble () { return Width / Scale.X; }
	double GetScaledHeightDouble () { return Height / Scale.Y; }
	double GetScaleY() const { return Scale.Y; }

	int GetScaledLeftOffset () { int foo = int((LeftOffset * 2) / Scale.X); return (foo >> 1) + (foo & 1); }
	int GetScaledTopOffset () { int foo = int((TopOffset * 2) / Scale.Y); return (foo >> 1) + (foo & 1); }
	double GetScaledLeftOffsetDouble() { return LeftOffset / Scale.X; }
	double GetScaledTopOffsetDouble() { return TopOffset / Scale.Y; }
	virtual void ResolvePatches() {}

	virtual void SetFrontSkyLayer();

	void CopyToBlock (uint8_t *dest, int dwidth, int dheight, int x, int y, int rotate, const uint8_t *translation, FRenderStyle style);

	// Returns true if the next call to GetPixels() will return an image different from the
	// last call to GetPixels(). This should be considered valid only if a call to CheckModified()
	// is immediately followed by a call to GetPixels().
	virtual bool CheckModified (FRenderStyle style);

	static void InitGrayMap();

	void CopySize(FTexture *BaseTexture)
	{
		Width = BaseTexture->GetWidth();
		Height = BaseTexture->GetHeight();
		TopOffset = BaseTexture->TopOffset;
		LeftOffset = BaseTexture->LeftOffset;
		WidthBits = BaseTexture->WidthBits;
		HeightBits = BaseTexture->HeightBits;
		Scale = BaseTexture->Scale;
		WidthMask = (1 << WidthBits) - 1;
	}

	void SetScaledSize(int fitwidth, int fitheight);
	void SetScale(const DVector2 &scale)
	{
		Scale = scale;
	}

	PalEntry GetSkyCapColor(bool bottom);
	static PalEntry averageColor(const uint32_t *data, int size, int maxout);

protected:
	uint16_t Width, Height, WidthMask;
	static uint8_t GrayMap[256];
	FNativeTexture *Native[TEX_Count] = { nullptr };	// keep a slot for each type, because some render modes do not work with the base texture
	uint8_t *GetRemap(FRenderStyle style, bool srcisgrayscale = false)
	{
		if (style.Flags & STYLEF_RedIsAlpha)
		{
			return translationtables[TRANSLATION_Standard][srcisgrayscale ? STD_Gray : STD_Grayscale]->Remap;
		}
		else
		{
			return srcisgrayscale ? GrayMap : GPalette.Remap;
		}
	}

	uint8_t RGBToPalettePrecise(bool wantluminance, int r, int g, int b, int a = 255)
	{
		if (wantluminance)
		{
			return (uint8_t)Luminance(r, g, b) * a / 255;
		}
		else
		{
			return ColorMatcher.Pick(r, g, b);
		}
	}

	uint8_t RGBToPalette(bool wantluminance, int r, int g, int b, int a = 255)
	{
		if (wantluminance)
		{
			// This is the same formula the OpenGL renderer uses for grayscale textures with an alpha channel.
			return (uint8_t)(Luminance(r, g, b) * a / 255);
		}
		else
		{
			return a < 128? 0 : RGB256k.RGB[r >> 2][g >> 2][b >> 2];
		}
	}

	uint8_t RGBToPalette(bool wantluminance, PalEntry pe, bool hasalpha = true)
	{
		return RGBToPalette(wantluminance, pe.r, pe.g, pe.b, hasalpha? pe.a : 255);
	}

	uint8_t RGBToPalette(FRenderStyle style, int r, int g, int b, int a = 255)
	{
		return RGBToPalette(!!(style.Flags & STYLEF_RedIsAlpha), r, g, b, a);
	}

	FTexture (const char *name = NULL, int lumpnum = -1);

	Span **CreateSpans (const uint8_t *pixels) const;
	void FreeSpans (Span **spans) const;
	void CalcBitSize ();
	void CopyInfo(FTexture *other)
	{
		CopySize(other);
		bNoDecals = other->bNoDecals;
		Rotations = other->Rotations;
		gl_info = other->gl_info;
		gl_info.Brightmap = NULL;
		gl_info.Normal = NULL;
		gl_info.Specular = NULL;
		gl_info.Metallic = NULL;
		gl_info.Roughness = NULL;
		gl_info.AmbientOcclusion = NULL;
		gl_info.areas = NULL;
	}

	std::vector<uint32_t> PixelsBgra;

	void GenerateBgraFromBitmap(const FBitmap &bitmap);
	void CreatePixelsBgraWithMipmaps();
	void GenerateBgraMipmaps();
	void GenerateBgraMipmapsFast();
	int MipmapLevels() const;

private:
	bool bSWSkyColorDone = false;
	PalEntry FloorSkyColor;
	PalEntry CeilingSkyColor;

public:
	static void FlipSquareBlock (uint8_t *block, int x, int y);
	static void FlipSquareBlockBgra (uint32_t *block, int x, int y);
	static void FlipSquareBlockRemap (uint8_t *block, int x, int y, const uint8_t *remap);
	static void FlipNonSquareBlock (uint8_t *blockto, const uint8_t *blockfrom, int x, int y, int srcpitch);
	static void FlipNonSquareBlockBgra (uint32_t *blockto, const uint32_t *blockfrom, int x, int y, int srcpitch);
	static void FlipNonSquareBlockRemap (uint8_t *blockto, const uint8_t *blockfrom, int x, int y, int srcpitch, const uint8_t *remap);

	friend class FNativeTexture;
	friend class OpenGLSWFrameBuffer;

public:

	struct MiscGLInfo
	{
		FMaterial *Material[2];
		FGLTexture *SystemTexture[2];
		FTexture *Brightmap;
		FTexture *Normal;						// Normal map texture
		FTexture *Specular;						// Specular light texture for the diffuse+normal+specular light model
		FTexture *Metallic;						// Metalness texture for the physically based rendering (PBR) light model
		FTexture *Roughness;					// Roughness texture for PBR
		FTexture *AmbientOcclusion;				// Ambient occlusion texture for PBR
		float Glossiness;
		float SpecularLevel;
		PalEntry GlowColor;
		int GlowHeight;
		FloatRect *areas;
		int areacount;
		int shaderindex;
		float shaderspeed;
		int mIsTransparent:2;
		bool bGlowing:1;						// Texture glows
		bool bAutoGlowing : 1;					// Glow info is determined from texture image.
		bool bFullbright:1;						// always draw fullbright
		bool bSkybox:1;							// This is a skybox
		char bBrightmapChecked:1;				// Set to 1 if brightmap has been checked
		bool bDisableFullbright:1;				// This texture will not be displayed as fullbright sprite
		bool bNoFilter:1;
		bool bNoCompress:1;
		bool bNoExpand:1;

		MiscGLInfo() throw ();
		~MiscGLInfo();
	};
	MiscGLInfo gl_info;

	void GetGlowColor(float *data);
	bool isGlowing() { return gl_info.bGlowing; }
	bool isFullbright() { return gl_info.bFullbright; }
	void CreateDefaultBrightmap();
	bool FindHoles(const unsigned char * buffer, int w, int h);
	static bool SmoothEdges(unsigned char * buffer,int w, int h);
	void CheckTrans(unsigned char * buffer, int size, int trans);
	bool ProcessData(unsigned char * buffer, int w, int h, bool ispatch);
	int CheckRealHeight();
};

class FxAddSub;
// Texture manager
class FTextureManager
{
	friend class FxAddSub;	// needs access to do a bounds check on the texture ID.
public:
	FTextureManager ();
	~FTextureManager ();

	// Get texture without translation
//private:
	FTexture *operator[] (FTextureID texnum)
	{
		if ((unsigned)texnum.GetIndex() >= Textures.Size()) return NULL;
		return Textures[texnum.GetIndex()].Texture;
	}
	FTexture *operator[] (const char *texname)
	{
		FTextureID texnum = GetTexture (texname, ETextureType::MiscPatch);
		if (!texnum.Exists()) return NULL;
		return Textures[texnum.GetIndex()].Texture;
	}
	FTexture *ByIndex(int i)
	{
		if (unsigned(i) >= Textures.Size()) return NULL;
		return Textures[i].Texture;
	}
	FTexture *FindTexture(const char *texname, ETextureType usetype = ETextureType::MiscPatch, BITFIELD flags = TEXMAN_TryAny);

	// Get texture with translation
	FTexture *operator() (FTextureID texnum, bool withpalcheck=false)
	{
		if ((size_t)texnum.texnum >= Textures.Size()) return NULL;
		int picnum = Translation[texnum.texnum];
		if (withpalcheck)
		{
			picnum = PalCheck(picnum).GetIndex();
		}
		return Textures[picnum].Texture;
	}
	FTexture *operator() (const char *texname)
	{
		FTextureID texnum = GetTexture (texname, ETextureType::MiscPatch);
		if (texnum.texnum == -1) return NULL;
		return Textures[Translation[texnum.texnum]].Texture;
	}

	FTexture *ByIndexTranslated(int i)
	{
		if (unsigned(i) >= Textures.Size()) return NULL;
		return Textures[Translation[i]].Texture;
	}
//public:

	FTextureID PalCheck(FTextureID tex);

	enum
	{
		TEXMAN_TryAny = 1,
		TEXMAN_Overridable = 2,
		TEXMAN_ReturnFirst = 4,
		TEXMAN_AllowSkins = 8,
		TEXMAN_ShortNameOnly = 16,
		TEXMAN_DontCreate = 32
	};

	enum
	{
		HIT_Wall = 1,
		HIT_Flat = 2,
		HIT_Sky = 4,
		HIT_Sprite = 8,

		HIT_Columnmode = HIT_Wall|HIT_Sky|HIT_Sprite
	};

	FTextureID CheckForTexture (const char *name, ETextureType usetype, BITFIELD flags=TEXMAN_TryAny);
	FTextureID GetTexture (const char *name, ETextureType usetype, BITFIELD flags=0);
	int ListTextures (const char *name, TArray<FTextureID> &list, bool listall = false);

	void AddTexturesLump (const void *lumpdata, int lumpsize, int deflumpnum, int patcheslump, int firstdup=0, bool texture1=false);
	void AddTexturesLumps (int lump1, int lump2, int patcheslump);
	void AddGroup(int wadnum, int ns, ETextureType usetype);
	void AddPatches (int lumpnum);
	void AddHiresTextures (int wadnum);
	void LoadTextureDefs(int wadnum, const char *lumpname);
	void ParseTextureDef(int remapLump);
	void ParseXTexture(FScanner &sc, ETextureType usetype);
	void SortTexturesByType(int start, int end);
	bool AreTexturesCompatible (FTextureID picnum1, FTextureID picnum2);

	FTextureID CreateTexture (int lumpnum, ETextureType usetype=ETextureType::Any);	// Also calls AddTexture
	FTextureID AddTexture (FTexture *texture);
	FTextureID GetDefaultTexture() const { return DefaultTexture; }

	void LoadTextureX(int wadnum);
	void AddTexturesForWad(int wadnum);
	void Init();
	void DeleteAll();

	// Replaces one texture with another. The new texture will be assigned
	// the same name, slot, and use type as the texture it is replacing.
	// The old texture will no longer be managed. Set free true if you want
	// the old texture to be deleted or set it false if you want it to
	// be left alone in memory. You will still need to delete it at some
	// point, because the texture manager no longer knows about it.
	// This function can be used for such things as warping textures.
	void ReplaceTexture (FTextureID picnum, FTexture *newtexture, bool free);

	void UnloadAll ();

	int NumTextures () const { return (int)Textures.Size(); }

	void UpdateAnimations (uint64_t mstime);
	int GuesstimateNumTextures ();

	FSwitchDef *FindSwitch (FTextureID texture);
	FDoorAnimation *FindAnimatedDoor (FTextureID picnum);

private:

	// texture counting
	int CountTexturesX ();
	int CountLumpTextures (int lumpnum);

	// Build tiles
	void AddTiles (const FString &pathprefix, const void *, int translation);
	//int CountBuildTiles ();
	void InitBuildTiles ();

	// Animation stuff
	FAnimDef *AddAnim (FAnimDef *anim);
	void FixAnimations ();
	void InitAnimated ();
	void InitAnimDefs ();
	FAnimDef *AddSimpleAnim (FTextureID picnum, int animcount, uint32_t speedmin, uint32_t speedrange=0);
	FAnimDef *AddComplexAnim (FTextureID picnum, const TArray<FAnimDef::FAnimFrame> &frames);
	void ParseAnim (FScanner &sc, ETextureType usetype);
	FAnimDef *ParseRangeAnim (FScanner &sc, FTextureID picnum, ETextureType usetype, bool missing);
	void ParsePicAnim (FScanner &sc, FTextureID picnum, ETextureType usetype, bool missing, TArray<FAnimDef::FAnimFrame> &frames);
	void ParseWarp(FScanner &sc);
	void ParseCameraTexture(FScanner &sc);
	FTextureID ParseFramenum (FScanner &sc, FTextureID basepicnum, ETextureType usetype, bool allowMissing);
	void ParseTime (FScanner &sc, uint32_t &min, uint32_t &max);
	FTexture *Texture(FTextureID id) { return Textures[id.GetIndex()].Texture; }
	void SetTranslation (FTextureID fromtexnum, FTextureID totexnum);
	void ParseAnimatedDoor(FScanner &sc);

	void InitPalettedVersions();

	// Switches

	void InitSwitchList ();
	void ProcessSwitchDef (FScanner &sc);
	FSwitchDef *ParseSwitchDef (FScanner &sc, bool ignoreBad);
	void AddSwitchPair (FSwitchDef *def1, FSwitchDef *def2);

	struct TextureHash
	{
		FTexture *Texture;
		int HashNext;
	};
	enum { HASH_END = -1, HASH_SIZE = 1027 };
	TArray<TextureHash> Textures;
	TArray<int> Translation;
	int HashFirst[HASH_SIZE];
	FTextureID DefaultTexture;
	TArray<int> FirstTextureForFile;
	TMap<int,int> PalettedVersions;		// maps from normal -> paletted version
	TArray<TArray<uint8_t> > BuildTileData;

	TArray<FSwitchDef *> mSwitchDefs;
	TArray<FDoorAnimation> mAnimatedDoors;

public:
	TArray<FAnimDef *> mAnimations;

	short sintable[2048];	// for texture warping
	enum
	{
		SINMASK = 2047
	};
};

// base class for everything that can be used as a world texture. 
// This intermediate class encapsulates the buffers for the software renderer.
class FWorldTexture : public FTexture
{
protected:
	uint8_t *Pixeldata[2] = { nullptr, nullptr };
	Span **Spandata[2] = { nullptr, nullptr };
	uint8_t PixelsAreStatic = 0;	// can be set by subclasses which provide static pixel buffers.

	FWorldTexture(const char *name = nullptr, int lumpnum = -1);
	~FWorldTexture();

	const uint8_t *GetColumn(FRenderStyle style, unsigned int column, const Span **spans_out) override;
	const uint8_t *GetPixels(FRenderStyle style) override;
	void Unload() override;
	virtual uint8_t *MakeTexture(FRenderStyle style) = 0;
	void FreeAllSpans();
};

// A texture that doesn't really exist
class FDummyTexture : public FTexture
{
public:
	FDummyTexture ();
	const uint8_t *GetColumn(FRenderStyle style, unsigned int column, const Span **spans_out) override;
	const uint8_t *GetPixels(FRenderStyle style) override;
	void SetSize (int width, int height);
};

// A texture that returns a wiggly version of another texture.
class FWarpTexture : public FWorldTexture
{
public:
	FWarpTexture (FTexture *source, int warptype);
	~FWarpTexture ();
	void Unload() override;

	virtual int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate=0, FCopyInfo *inf = NULL) override;
	const uint32_t *GetPixelsBgra() override;
	bool CheckModified (FRenderStyle) override;

	float GetSpeed() const { return Speed; }
	int GetSourceLump() { return SourcePic->GetSourceLump(); }
	void SetSpeed(float fac) { Speed = fac; }
	FTexture *GetRedirect(bool wantwarped);

	uint64_t GenTime[2] = { 0, 0 };
	uint64_t GenTimeBgra = 0;
	float Speed = 1.f;
	int WidthOffsetMultiplier, HeightOffsetMultiplier;  // [mxd]
protected:
	FTexture *SourcePic;

	uint8_t *MakeTexture (FRenderStyle style) override;
	int NextPo2 (int v); // [mxd]
	void SetupMultipliers (int width, int height); // [mxd]
};

// A texture that can be drawn to.
class DSimpleCanvas;
class AActor;

class FCanvasTexture : public FTexture
{
public:
	FCanvasTexture (const char *name, int width, int height);
	~FCanvasTexture ();

	const uint8_t *GetColumn(FRenderStyle style, unsigned int column, const Span **spans_out);
	const uint8_t *GetPixels (FRenderStyle style);
	const uint32_t *GetPixelsBgra() override;
	void Unload ();
	bool CheckModified (FRenderStyle) override;
	void NeedUpdate() { bNeedsUpdate=true; }
	void SetUpdated() { bNeedsUpdate = false; bDidUpdate = true; bFirstUpdate = false; }
	DSimpleCanvas *GetCanvas() { return Canvas; }
	DSimpleCanvas *GetCanvasBgra() { return CanvasBgra; }
	bool Mipmapped() override { return false; }
	void MakeTexture (FRenderStyle style);
	void MakeTextureBgra ();

protected:

	DSimpleCanvas *Canvas = nullptr;
	DSimpleCanvas *CanvasBgra = nullptr;
	uint8_t *Pixels = nullptr;
	uint32_t *PixelsBgra = nullptr;
	Span DummySpans[2];
	bool bNeedsUpdate = true;
	bool bDidUpdate = false;
	bool bPixelsAllocated = false;
	bool bPixelsAllocatedBgra = false;
public:
	bool bFirstUpdate;


	friend struct FCanvasTextureInfo;
};

extern FTextureManager TexMan;

struct FTexCoordInfo
{
	int mRenderWidth;
	int mRenderHeight;
	int mWidth;
	FVector2 mScale;
	FVector2 mTempScale;
	bool mWorldPanning;

	float FloatToTexU(float v) const { return v / mRenderWidth; }
	float FloatToTexV(float v) const { return v / mRenderHeight; }
	float RowOffset(float ofs) const;
	float TextureOffset(float ofs) const;
	float TextureAdjustWidth() const;
	void GetFromTexture(FTexture *tex, float x, float y);
};


#endif


