/*
** hardware.cpp
** Somewhat OS-independant interface to the screen, mouse, keyboard, and stick
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include <signal.h>
#include <time.h>

#include "version.h"
#include "hardware.h"
#include "i_video.h"
#include "i_system.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "video.h"
#include "v_text.h"
#include "doomstat.h"
#include "m_argv.h"
#include "glvideo.h"
#include "r_renderer.h"
#include "swrenderer/r_swrenderer.h"

EXTERN_CVAR (Bool, ticker)
EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Bool, swtruecolor)
EXTERN_CVAR (Float, vid_winscale)

IVideo *Video;

extern int NewWidth, NewHeight, NewBits, DisplayBits;
bool V_DoModeSetup (int width, int height, int bits);
void I_RestartRenderer();

int currentrenderer;

// [ZDoomGL]
CUSTOM_CVAR (Int, vid_renderer, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	// 0: Software renderer
	// 1: OpenGL renderer

	if (self != currentrenderer)
	{
		switch (self)
		{
		case 0:
			Printf("Switching to software renderer...\n");
			break;
		case 1:
			Printf("Switching to OpenGL renderer...\n");
			break;
		default:
			Printf("Unknown renderer (%d).  Falling back to software renderer...\n", (int) vid_renderer);
			self = 0; // make sure to actually switch to the software renderer
			break;
		}
		Printf("You must restart " GAMENAME " to switch the renderer\n");
	}
}

void I_ShutdownGraphics ()
{
	if (screen)
	{
		DFrameBuffer *s = screen;
		screen = NULL;
		delete s;
	}
	if (Video)
		delete Video, Video = NULL;
}

void I_InitGraphics ()
{
	UCVarValue val;

	val.Bool = !!Args->CheckParm ("-devparm");
	ticker.SetGenericRepDefault (val, CVAR_Bool);
	
	currentrenderer = vid_renderer;
	Video = new NoSDLGLVideo(0);
	
	if (Video == NULL)
		I_FatalError ("Failed to initialize display");

	Video->SetWindowedScale (vid_winscale);

	currentrenderer = vid_renderer;
}

void I_DeleteRenderer()
{
	if (Renderer != NULL) delete Renderer;
}

void I_CreateRenderer()
{
	currentrenderer = vid_renderer;
	if (Renderer == NULL)
	{
		if (currentrenderer==1) Renderer = gl_CreateInterface();
		else Renderer = new FSoftwareRenderer;
	}
}


/** Remaining code is common to Win32 and Linux **/

// VIDEO WRAPPERS ---------------------------------------------------------

DFrameBuffer *I_SetMode (int &width, int &height, DFrameBuffer *old)
{
	return Video->CreateFrameBuffer (width, height, swtruecolor, true, old);
}

bool I_CheckResolution (int width, int height, int bits)
{
	int twidth, theight;

	Video->StartModeIterator (bits, screen ? screen->IsFullscreen() : fullscreen);
	while (Video->NextMode (&twidth, &theight, NULL))
	{
		if (width == twidth && height == theight)
			return true;
	}
	return false;
}

void I_ClosestResolution (int *width, int *height, int bits)
{
    return;
}

//==========================================================================
//
// SetFPSLimit
//
// Initializes an event timer to fire at a rate of <limit>/sec. The video
// update will wait for this timer to trigger before updating.
//
// Pass 0 as the limit for unlimited.
// Pass a negative value for the limit to use the value of vid_maxfps.
//
//==========================================================================

EXTERN_CVAR(Int, vid_maxfps);
EXTERN_CVAR(Bool, cl_capfps);

#if !defined(__APPLE__) && !defined(__OpenBSD__)

static void FPSLimitNotify(sigval val)
{
}

void I_SetFPSLimit(int limit)
{
}
#endif

CUSTOM_CVAR (Int, vid_maxfps, 200, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (vid_maxfps < TICRATE && vid_maxfps != 0)
	{
		vid_maxfps = TICRATE;
	}
	else if (vid_maxfps > 1000)
	{
		vid_maxfps = 1000;
	}
	else if (cl_capfps == 0)
	{
		I_SetFPSLimit(vid_maxfps);
	}
}

extern int NewWidth, NewHeight, NewBits, DisplayBits;

CUSTOM_CVAR(Bool, swtruecolor, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	// Strictly speaking this doesn't require a mode switch, but it is the easiest
	// way to force a CreateFramebuffer call without a lot of refactoring.
	if (currentrenderer == 0)
	{
		NewWidth = screen->VideoWidth;
		NewHeight = screen->VideoHeight;
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}

CUSTOM_CVAR (Bool, fullscreen, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	NewWidth = screen->VideoWidth;
	NewHeight = screen->VideoHeight;
	NewBits = DisplayBits;
	setmodeneeded = true;
}

CUSTOM_CVAR (Float, vid_winscale, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 1.f)
	{
		self = 1.f;
	}
	else if (Video)
	{
		Video->SetWindowedScale (self);
		NewWidth = screen->VideoWidth;
		NewHeight = screen->VideoHeight;
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}

CCMD (vid_listmodes)
{
	Printf ("Just.. dont!\n");
}

CCMD (vid_currentmode)
{
	Printf ("%dx%dx%d\n", DisplayWidth, DisplayHeight, DisplayBits);
}
