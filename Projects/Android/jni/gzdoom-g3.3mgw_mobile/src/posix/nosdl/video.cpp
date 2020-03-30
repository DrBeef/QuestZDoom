/*
** sdlvideo.cpp
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include "doomtype.h"

#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "v_palette.h"
#include "video.h"
#include "swrenderer/r_swrenderer.h"
#include "version.h"
#include <QzDoom/VrCommon.h>


// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct MiniModeInfo
{
	uint16_t Width, Height;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;
extern bool GUICapture;

EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_maxfps)
EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, vid_vsync)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Int, vid_adapter, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR (Int, vid_displaybits, 32, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR (Bool, vid_forcesurface, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR (Float, rgamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma (Gamma);
	}
}
CUSTOM_CVAR (Float, ggamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma (Gamma);
	}
}
CUSTOM_CVAR (Float, bgamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (screen != NULL)
	{
		screen->SetGamma (Gamma);
	}
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

//static cycle_t BlitCycles;
//static cycle_t SDLFlipCycles;

// CODE --------------------------------------------------------------------

// FrameBuffer implementation -----------------------------------------------

NoSDLFB::NoSDLFB (int width, int height, bool bgra, bool fullscreen/*, SDL_Window *oldwin*/)
	: NoSDLBaseFB (width, height, bgra)
{
	int i;
	
	NeedPalUpdate = false;
	NeedGammaUpdate = false;
	UpdatePending = false;
	NotPaletted = false;
	FlashAmount = 0;


//	Renderer = NULL;
//	Texture = NULL;

	for (i = 0; i < 256; i++)
	{
		GammaTable[0][i] = GammaTable[1][i] = GammaTable[2][i] = i;
	}

	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);
	UpdateColors ();

#ifdef __APPLE__
	SetVSync (vid_vsync);
#endif
}


NoSDLFB::~NoSDLFB ()
{
}

bool NoSDLFB::IsValid ()
{
	return DFrameBuffer::IsValid();
}

int NoSDLFB::GetPageCount ()
{
	return 1;
}

bool NoSDLFB::Lock (bool buffered)
{
	return DSimpleCanvas::Lock ();
}

bool NoSDLFB::Relock ()
{
	return DSimpleCanvas::Lock ();
}

void NoSDLFB::Unlock ()
{
	--LockCount;
}

void NoSDLFB::Update ()
{
}

void NoSDLFB::UpdateColors ()
{
}

PalEntry *NoSDLFB::GetPalette ()
{
	return SourcePalette;
}

void NoSDLFB::UpdatePalette ()
{
	NeedPalUpdate = true;
}

bool NoSDLFB::SetGamma (float gamma)
{
	Gamma = gamma;
	NeedGammaUpdate = true;
	return true;
}

bool NoSDLFB::SetFlash (PalEntry rgb, int amount)
{
	Flash = rgb;
	FlashAmount = amount;
	NeedPalUpdate = true;
	return true;
}

void NoSDLFB::GetFlash (PalEntry &rgb, int &amount)
{
	rgb = Flash;
	amount = FlashAmount;
}

// Q: Should I gamma adjust the returned palette?
void NoSDLFB::GetFlashedPalette (PalEntry pal[256])
{
}

void NoSDLFB::SetFullscreen (bool fullscreen)
{
}

bool NoSDLFB::IsFullscreen ()
{
	return true;
}

void NoSDLFB::ResetSDLRenderer ()
{
}

void NoSDLFB::SetVSync (bool vsync)
{
}

void NoSDLFB::ScaleCoordsFromWindow(int16_t &x, int16_t &y)
{
	uint32_t w, h;
    QzDoom_GetScreenRes(&w, &h);


	// Detect if we're doing scaling in the Window and adjust the mouse
	// coordinates accordingly. This could be more efficent, but I
	// don't think performance is an issue in the menus.
	if(IsFullscreen())
	{
		int realw = w, realh = h;
		ScaleWithAspect (realw, realh, SCREENWIDTH, SCREENHEIGHT);
		if (realw != SCREENWIDTH || realh != SCREENHEIGHT)
		{
			double xratio = (double)SCREENWIDTH/realw;
			double yratio = (double)SCREENHEIGHT/realh;
			if (realw < w)
			{
				x = (x - (w - realw)/2)*xratio;
				y *= yratio;
			}
			else
			{
				y = (y - (h - realh)/2)*yratio;
				x *= xratio;
			}
		}
	}
	else
	{
		x = (int16_t)(x*Width/w);
		y = (int16_t)(y*Height/h);
	}
}

// each platform has its own specific version of this function.
void I_SetWindowTitle(const char* caption)
{
}

