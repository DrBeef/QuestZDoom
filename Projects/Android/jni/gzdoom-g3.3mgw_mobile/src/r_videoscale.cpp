// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2017 Magnus Norddahl
// Copyright(C) 2018 Rachael Alexanderson
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

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
	v_ScaleTable vScaleTable[NUMSCALEMODES] =
	{
		//	isValid,	isLinear,	GetScaledWidth(),									            	GetScaledHeight(),										        	isScaled43, isCustom
		{ true,			false,		[](uint32_t Width)->uint32_t { return Width; },		        	[](uint32_t Height)->uint32_t { return Height; },	        		false,  	false   },	// 0  - Native
		{ true,			true,		[](uint32_t Width)->uint32_t { return Width; },			        [](uint32_t Height)->uint32_t { return Height; },	        		false,  	false   },	// 1  - Native (Linear)
		{ true,			false,		[](uint32_t Width)->uint32_t { return 320; },		            	[](uint32_t Height)->uint32_t { return 200; },			        	true,   	false   },	// 2  - 320x200
		{ true,			false,		[](uint32_t Width)->uint32_t { return 640; },		            	[](uint32_t Height)->uint32_t { return 400; },				        true,   	false   },	// 3  - 640x400
		{ true,			true,		[](uint32_t Width)->uint32_t { return 1280; },		            	[](uint32_t Height)->uint32_t { return 800; },	        			true,   	false   },	// 4  - 1280x800		
		{ true,			true,		[](uint32_t Width)->uint32_t { return vid_scale_customwidth; },	[](uint32_t Height)->uint32_t { return vid_scale_customheight; },	true,   	true    },	// 5  - Custom
		{ true,			false,		[](uint32_t Width)->uint32_t { return 160; },		            	[](uint32_t Height)->uint32_t { return 200; },			        	true,   	false   },	// 6  - 160x200
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

CUSTOM_CVAR(Float, vid_scalefactor, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	setsizeneeded = true;
	if (self < 0.05 || self > 2.0)
		self = 1.0;
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
	if (vid_scalemode == 6)
		vid_scalefactor = 1.0;
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
	if (vid_scalemode == 6)
		vid_scalefactor = 1.0;
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
	Printf("Current Scale: %f\n", (float)(vid_scalefactor));
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

CCMD (vid_scaletowidth)
{
	if (R_CalcsShouldBeBlocked())
		return;	

	if (argv.argc() > 1)
		vid_scalefactor = (float)((double)vid_scalefactor * (double)atof(argv[1]) / (double)DisplayWidth);

	R_ShowCurrentScaling();
}

CCMD (vid_scaletoheight)
{
	if (R_CalcsShouldBeBlocked())
		return;	

	if (argv.argc() > 1)
		vid_scalefactor = (float)((double)vid_scalefactor * (double)atof(argv[1]) / (double)DisplayHeight);

	R_ShowCurrentScaling();
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
