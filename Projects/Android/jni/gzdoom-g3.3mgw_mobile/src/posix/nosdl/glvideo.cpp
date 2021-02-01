/*
** sdlglvideo.cpp
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Christoph Oelckers et.al.
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
#include "m_argv.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "version.h"
#include "c_console.h"

#include "videomodes.h"
#include "glvideo.h"
#include "video.h"
#include "gl/system/gl_system.h"
#include "r_defs.h"
#include "gl/gl_functions.h"
//#include "gl/gl_intern.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/shaders/gl_shader.h"
#include "gl/utility/gl_templates.h"
#include "gl/textures/gl_material.h"
#include "gl/system/gl_cvars.h"

#include <QzDoom/VrCommon.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;
// extern int vid_renderer;

EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_adapter)
EXTERN_CVAR (Int, vid_displaybits)
EXTERN_CVAR (Int, vid_renderer)
EXTERN_CVAR (Int, vid_maxfps)
EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Int, gl_hardware_buffers)


DFrameBuffer *CreateGLSWFrameBuffer(int width, int height, bool bgra, bool fullscreen);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Bool, gl_debug, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}
#ifdef __arm__
CUSTOM_CVAR(Bool, vid_glswfb, false, CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}
CUSTOM_CVAR(Bool, gl_es, false, CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}
#else
CUSTOM_CVAR(Bool, vid_glswfb, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}
CUSTOM_CVAR(Bool, gl_es, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

NoSDLGLVideo::NoSDLGLVideo (int parm)
{
	IteratorBits = 0;
}

NoSDLGLVideo::~NoSDLGLVideo ()
{
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
}

void NoSDLGLVideo::StartModeIterator (int bits, bool fs)
{
	IteratorMode = 0;
	IteratorBits = bits;
}

bool NoSDLGLVideo::NextMode (int *width, int *height, bool *letterbox)
{
	if (IteratorBits != 8)
		return false;

	if ((unsigned)IteratorMode < sizeof(VideoModes)/sizeof(VideoModes[0]))
	{
		*width = VideoModes[IteratorMode].width;
		*height = VideoModes[IteratorMode].height;
		++IteratorMode;
		return true;
	}
	return false;
}

extern "C" int QzDoom_GetRefresh();

DFrameBuffer *NoSDLGLVideo::CreateFrameBuffer (int width, int height, bool bgra, bool fullscreen, DFrameBuffer *old)
{
	static int retry = 0;
	static int owidth, oheight;
	
	PalEntry flashColor;
//	int flashAmount;

	if (old != NULL)
	{
		delete old;
	}
	else
	{
		flashColor = 0;
//		flashAmount = 0;
	}
	
	NoSDLBaseFB *fb;

    Printf("HW buffers = %d\n", (int)gl_hardware_buffers);
    fb = new OpenGLFrameBuffer(0, width, height, 32, QzDoom_GetRefresh(), true);

	retry = 0;
	return fb;
}

void NoSDLGLVideo::SetWindowedScale (float scale)
{
}

bool NoSDLGLVideo::SetResolution (int width, int height, int bits)
{
	// FIXME: Is it possible to do this without completely destroying the old
	// interface?
#ifndef NO_GL

	if (GLRenderer != NULL) GLRenderer->FlushTextures();
	I_ShutdownGraphics();

	Video = new NoSDLGLVideo(0);
	if (Video == NULL) I_FatalError ("Failed to initialize display");

#if (defined(WINDOWS)) || defined(WIN32)
	bits=32;
#else
	bits=24;
#endif
	
	V_DoModeSetup(width, height, bits);
#endif
	return true;	// We must return true because the old video context no longer exists.
}

//==========================================================================
//
// 
//
//==========================================================================
#ifdef __MOBILE__
extern "C" extern int glesLoad;
#endif

void NoSDLGLVideo::SetupPixelFormat(bool allowsoftware, int multisample, const int *glver)
{
		
#ifdef __MOBILE__

	int major,min;

	const char *version = Args->CheckValue("-glversion");
	if( !strcmp(version, "gles1") )
	{
		glesLoad = 1;
		major = 1;
		min = 0;
	}
	else if ( !strcmp(version, "gles2") )
	{
		glesLoad = 2;
        major = 2;
        min = 0;
	}
    else if ( !strcmp(version, "gles3") )
	{
		glesLoad = 3;
		major = 3;
		min = 1;
	}
#endif

}


// FrameBuffer implementation -----------------------------------------------

NoSDLGLFB::NoSDLGLFB (void *, int width, int height, int, int, bool fullscreen, bool bgra)
	: NoSDLBaseFB (width, height, bgra)
{
}

NoSDLGLFB::~NoSDLGLFB ()
{
}


void NoSDLGLFB::InitializeState() 
{
}

void NoSDLGLFB::SetGammaTable(uint16_t *tbl)
{
}

void NoSDLGLFB::ResetGammaTable()
{
}

bool NoSDLGLFB::Lock(bool buffered)
{
	m_Lock++;
	return true;
}

bool NoSDLGLFB::Lock () 
{ 	
	return Lock(false); 
}

void NoSDLGLFB::Unlock () 	
{ 
	--m_Lock;
}

bool NoSDLGLFB::IsLocked () 
{ 
	return m_Lock>0;// true;
}

bool NoSDLGLFB::IsFullscreen ()
{
	return true;
}


bool NoSDLGLFB::IsValid ()
{
	return DFrameBuffer::IsValid();
}

void NoSDLGLFB::SetVSync( bool vsync )
{
}

void NoSDLGLFB::NewRefreshRate ()
{
}

void NoSDLGLFB::SwapBuffers()
{
	//No swapping required
}

int NoSDLGLFB::GetClientWidth()
{
	uint32_t w, h;
    QzDoom_GetScreenRes(&w, &h);
	int width = w;
	return width;
}

int NoSDLGLFB::GetClientHeight()
{
	uint32_t w, h;
    QzDoom_GetScreenRes(&w, &h);
	int height = h;
	return height;
}

void NoSDLGLFB::ScaleCoordsFromWindow(int16_t &x, int16_t &y)
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
