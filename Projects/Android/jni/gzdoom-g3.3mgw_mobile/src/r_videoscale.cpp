/*---------------------------------------------------------------------------
**
** Copyright(C) 2017 Magnus Norddahl
** Copyright(C) 2017-2020 Rachael Alexanderson
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

#include <math.h>
#include "c_dispatch.h"
#include "c_cvars.h"
#include "v_video.h"
#include "templates.h"

#define NUMSCALEMODES 7

extern bool setsizeneeded;
extern int currentrenderer;
#ifdef _WIN32
extern int currentcanvas;
#else
int currentcanvas = -1;
#endif

EXTERN_CVAR(Int, vid_aspect)

CUSTOM_CVAR(Int, vid_scale_customwidth, 320, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 160)
		self = 160;
	setsizeneeded = true;
}
CUSTOM_CVAR(Int, vid_scale_customheight, 200, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 100)
		self = 100;
	setsizeneeded = true;
}
CVAR(Bool, vid_scale_customlinear, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Bool, vid_scale_customstretched, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	setsizeneeded = true;
}

namespace
{
	struct v_ScaleTable
	{
		bool isValid;
		bool isLinear;
		uint32_t(*GetScaledWidth)(uint32_t Width);
		uint32_t(*GetScaledHeight)(uint32_t Height);
		bool isScaled43;
		bool isCustom;
	};

	float v_MinimumToFill()
	{
		// sx = screen x dimension, sy = same for y
		float sx = (float)screen->GetWidth(), sy = (float)screen->GetHeight();
		static float lastsx = 0., lastsy = 0., result = 0.;
		if (lastsx != sx || lastsy != sy)
		{
			if (sx <= 0. || sy <= 0.)
				return 1.; // prevent x/0 error
			// set absolute minimum scale to fill the entire screen but get as close to 320x200 as possible
			float ssx = (float)320. / sx, ssy = (float)200. / sy;
			result = (ssx < ssy) ? ssy : ssx;
			lastsx = sx;
			lastsy = sy;
		}
		return result;
	}
	inline uint32_t v_mfillX()
	{
		return screen ? (uint32_t)((float)screen->GetWidth() * v_MinimumToFill()) : 640;
	}
	inline uint32_t v_mfillY()
	{
		return screen ? (uint32_t)((float)screen->GetHeight() * v_MinimumToFill()) : 400;
	}

	v_ScaleTable vScaleTable[NUMSCALEMODES] =
	{
		//	isValid,	isLinear,	GetScaledWidth(),									            	GetScaledHeight(),										        	isScaled43, isCustom
		{ true,			false,		[](uint32_t Width)->uint32_t { return Width; },		        	[](uint32_t Height)->uint32_t { return Height; },	        		false,  	false   },	// 0  - Native
		{ true,			true,		[](uint32_t Width)->uint32_t { return Width; },			        [](uint32_t Height)->uint32_t { return Height; },	        		false,  	false   },	// 1  - Native (Linear)
		{ true,			false,		[](uint32_t Width)->uint32_t { return 320; },		            	[](uint32_t Height)->uint32_t { return 200; },			        	true,   	false   },	// 2  - 320x200
		{ true,			false,		[](uint32_t Width)->uint32_t { return 640; },		            	[](uint32_t Height)->uint32_t { return 400; },				        true,   	false   },	// 3  - 640x400
		{ true,			true,		[](uint32_t Width)->uint32_t { return 1280; },		            	[](uint32_t Height)->uint32_t { return 800; },	        			true,   	false   },	// 4  - 1280x800		
		{ true,			true,		[](uint32_t Width)->uint32_t { return vid_scale_customwidth; },	[](uint32_t Height)->uint32_t { return vid_scale_customheight; },	true,   	true    },	// 5  - Custom
		{ true,			false,		[](uint32_t Width)->uint32_t { return v_mfillX(); },				[](uint32_t Height)->uint32_t { return v_mfillY(); },				false,		false   },	// 6  - Minimum Scale to Fill Entire Screen
	};
	bool isOutOfBounds(int x)
	{
		return (x < 0 || x >= NUMSCALEMODES || vScaleTable[x].isValid == false);
	}
	bool isClassicSoftware()
	{
		return (currentrenderer == 0 && currentcanvas == 0);
	}
}

void R_ShowCurrentScaling();
CUSTOM_CVAR(Float, vid_scalefactor, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	setsizeneeded = true;
	if (self < 0.05 || self > 2.0)
		self = 1.0;
	if (self != 1.0)
		R_ShowCurrentScaling();
}

CUSTOM_CVAR(Int, vid_scalemode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	setsizeneeded = true;
	if (isOutOfBounds(self))
		self = 0;
}

CUSTOM_CVAR(Bool, vid_cropaspect, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	setsizeneeded = true;
}

bool ViewportLinearScale()
{
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	// hack - use custom scaling if in "custom" mode
	if (vScaleTable[vid_scalemode].isCustom)
		return vid_scale_customlinear;
	// vid_scalefactor > 1 == forced linear scale
	return (vid_scalefactor > 1.0) ? true : vScaleTable[vid_scalemode].isLinear;
}

int ViewportScaledWidth(int width, int height)
{
	if (isClassicSoftware())
	{
		vid_scalemode = 0;
		vid_scalefactor = 1.0;
	}
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	if (vid_cropaspect && height > 0)
		width = ((float)width/height > ActiveRatio(width, height)) ? (int)(height * ActiveRatio(width, height)) : width;
	return (int)MAX((int32_t)160, (int32_t)(vid_scalefactor * vScaleTable[vid_scalemode].GetScaledWidth(width)));
}

int ViewportScaledHeight(int width, int height)
{
	if (isClassicSoftware())
	{
		vid_scalemode = 0;
		vid_scalefactor = 1.0;
	}
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	if (vid_cropaspect && height > 0)
		height = ((float)width/height < ActiveRatio(width, height)) ? (int)(width / ActiveRatio(width, height)) : height;
	return (int)MAX((int32_t)100, (int32_t)(vid_scalefactor * vScaleTable[vid_scalemode].GetScaledHeight(height)));
}

bool ViewportIsScaled43()
{
	if (isClassicSoftware())
		vid_scalemode = 0;
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	// hack - use custom scaling if in "custom" mode
	if (vScaleTable[vid_scalemode].isCustom)
		return vid_scale_customstretched;
	return vScaleTable[vid_scalemode].isScaled43;
}

void R_ShowCurrentScaling()
{
	int x1 = screen->GetWidth(), y1 = screen->GetHeight(), x2 = ViewportScaledWidth(x1, y1), y2 = ViewportScaledHeight(x1, y1);
	Printf("Current vid_scalefactor: %f\n", (float)(vid_scalefactor));
	Printf("Real resolution: %i x %i\nEmulated resolution: %i x %i\n", x1, y1, x2, y2);
}

bool R_CalcsShouldBeBlocked()
{
	if (isClassicSoftware())
	{
		Printf("This command is not available for the classic software renderer.\n");
		return true;
	}
	if (vid_scalemode < 0 || vid_scalemode > 1)
	{
		Printf("vid_scalemode should be 0 or 1 before using this command.\n");
		return true;
	}
	if (vid_aspect != 0 && vid_cropaspect == true)
	{   // just warn ... I'm not going to fix this, it's a pretty niche condition anyway.
        Printf("Warning: Using this command while vid_aspect is not 0 will yield results based on FULL screen geometry, NOT cropped!.\n");
		return false;
	}
	return false;	
}

CCMD (vid_showcurrentscaling)
{
	R_ShowCurrentScaling();
}

CCMD (vid_scaletowidth)
{
	if (R_CalcsShouldBeBlocked())
		return;	

	if (argv.argc() > 1)
	{
		// the following enables the use of ViewportScaledWidth to get the proper dimensions in custom scale modes
		vid_scalefactor = 1;
		vid_scalefactor = (float)((double)atof(argv[1]) / ViewportScaledWidth(screen->GetWidth(), screen->GetHeight()));
	}
}

CCMD (vid_scaletoheight)
{
	if (R_CalcsShouldBeBlocked())
		return;	

	if (argv.argc() > 1)
	{
		vid_scalefactor = 1;
		vid_scalefactor = (float)((double)atof(argv[1]) / ViewportScaledHeight(screen->GetWidth(), screen->GetHeight()));
	}
}

inline bool atob(char* I)
{
    if (stricmp (I, "true") == 0 || stricmp (I, "1") == 0)
        return true;
    return false;
}

CCMD (vid_setscale)
{
    if (argv.argc() > 2)
    {
        vid_scale_customwidth = atoi(argv[1]);
        vid_scale_customheight = atoi(argv[2]);
        if (argv.argc() > 3)
        {
            vid_scale_customlinear = atob(argv[3]);
            if (argv.argc() > 4)
            {
                vid_scale_customstretched = atob(argv[4]);
            }
        }
        vid_scalemode = 5;
		vid_scalefactor = 1.0;
    }
    else
    {
        Printf("Usage: vid_setscale <x> <y> [bool linear] [bool long-pixel-shape]\nThis command will create a custom viewport scaling mode.\n");
    }
}
