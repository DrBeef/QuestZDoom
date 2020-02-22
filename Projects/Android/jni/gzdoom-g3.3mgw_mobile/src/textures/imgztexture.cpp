/*
** imgztexture.cpp
** Texture class for IMGZ style images
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
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
**
*/

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "v_palette.h"
#include "v_video.h"
#include "bitmap.h"
#include "textures/textures.h"
#include "r_data/r_translate.h"

bool checkIMGZPalette(FileReader &file);

//==========================================================================
//
// An IMGZ image (mostly just crosshairs)
// [RH] Just a format I invented to avoid WinTex's palette remapping
// when I wanted to insert some alpha maps.
//
//==========================================================================

class FIMGZTexture : public FWorldTexture
{
	struct ImageHeader
	{
		uint8_t Magic[4];
		uint16_t Width;
		uint16_t Height;
		int16_t LeftOffset;
		int16_t TopOffset;
		uint8_t Compression;
		uint8_t Reserved[11];
	};

	bool isalpha = true;

public:
	FIMGZTexture (int lumpnum, uint16_t w, uint16_t h, int16_t l, int16_t t, bool isalpha);
	uint8_t *MakeTexture (FRenderStyle style) override;
	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf) override;

	bool UseBasePalette() override { return !isalpha; }
	FTextureFormat GetFormat() override { return isalpha ? TEX_RGB : TEX_Pal; } // should be TEX_Gray instead of TEX_RGB. Maybe later when all is working.
};


//==========================================================================
//
//
//
//==========================================================================

FTexture *IMGZTexture_TryCreate(FileReader & file, int lumpnum)
{
	uint32_t magic = 0;
	uint16_t w, h;
	int16_t l, t;
	bool ispalette;

	file.Seek(0, FileReader::SeekSet);
	if (file.Read(&magic, 4) != 4) return NULL;
	if (magic != MAKE_ID('I','M','G','Z')) return NULL;
	w = file.ReadUInt16();
	h = file.ReadUInt16();
	l = file.ReadInt16();
	t = file.ReadInt16();
	ispalette = checkIMGZPalette(file);
	return new FIMGZTexture(lumpnum, w, h, l, t, !ispalette);
}

//==========================================================================
//
//
//
//==========================================================================

FIMGZTexture::FIMGZTexture (int lumpnum, uint16_t w, uint16_t h, int16_t l, int16_t t, bool _isalpha)
	: FWorldTexture(NULL, lumpnum)
{
	Wads.GetLumpName (Name, lumpnum);
	Width = w;
	Height = h;
	LeftOffset = l;
	TopOffset = t;
	isalpha = _isalpha;
	CalcBitSize ();
}

//==========================================================================
//
//
//
//==========================================================================

uint8_t *FIMGZTexture::MakeTexture (FRenderStyle style)
{
	FMemLump lump = Wads.ReadLump (SourceLump);
	const ImageHeader *imgz = (const ImageHeader *)lump.GetMem();
	const uint8_t *data = (const uint8_t *)&imgz[1];

	if (Width != 0xFFFF)
	{
		Width = LittleShort(imgz->Width);
		Height = LittleShort(imgz->Height);
		LeftOffset = LittleShort(imgz->LeftOffset);
		TopOffset = LittleShort(imgz->TopOffset);
	}

	uint8_t *dest_p;
	int dest_adv = Height;
	int dest_rew = Width * Height - 1;

	CalcBitSize ();
	auto Pixels = new uint8_t[Width*Height];
	dest_p = Pixels;

	const uint8_t *remap = GetRemap(style, isalpha);

	// Convert the source image from row-major to column-major format and remap it
	if (!imgz->Compression)
	{
		for (int y = Height; y != 0; --y)
		{
			for (int x = Width; x != 0; --x)
			{
				*dest_p = remap[*data];
				dest_p += dest_adv;
				data++;
			}
			dest_p -= dest_rew;
		}
	}
	else
	{
		// IMGZ compression is the same RLE used by IFF ILBM files
		int runlen = 0, setlen = 0;
		uint8_t setval = 0;  // Shut up, GCC

		for (int y = Height; y != 0; --y)
		{
			for (int x = Width; x != 0; )
			{
				if (runlen != 0)
				{
					*dest_p = remap[*data];
					dest_p += dest_adv;
					data++;
					x--;
					runlen--;
				}
				else if (setlen != 0)
				{
					*dest_p = setval;
					dest_p += dest_adv;
					x--;
					setlen--;
				}
				else
				{
					int8_t code = *data++;
					if (code >= 0)
					{
						runlen = code + 1;
					}
					else if (code != -128)
					{
						setlen = (-code) + 1;
						setval = remap[*data++];
					}
				}
			}
			dest_p -= dest_rew;
		}
	}
	return Pixels;
}

//==========================================================================
//
//
//
//==========================================================================

int FIMGZTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	if (!isalpha) return FTexture::CopyTrueColorPixels(bmp, x, y, rotate, inf);
	else return CopyTrueColorTranslated(bmp, x, y, rotate, translationtables[TRANSLATION_Standard][STD_Grayscale]->Palette, inf);
}

