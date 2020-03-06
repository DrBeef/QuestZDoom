/*
** i_main.cpp
** System-specific startup code. Eventually calls D_DoomMain.
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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

//#include <SDL.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <new>
#include <sys/param.h>
#include <locale.h>

#include "doomerrors.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "c_console.h"
#include "errors.h"
#include "version.h"
#include "w_wad.h"
#include "g_level.h"
#include "g_levellocals.h"
#include "r_state.h"
#include "cmdlib.h"
#include "r_utility.h"
#include "doomstat.h"
#include "vm.h"
#include "atterm.h"

// MACROS ------------------------------------------------------------------

// The maximum number of functions that can be registered with atterm.
#define MAX_TERMS	64

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern "C" int cc_install_handlers(int, char**, int, int*, const char*, int(*)(char*, char*));

#ifdef __APPLE__
void Mac_I_FatalError(const char* errortext);
#endif

#ifdef __linux__
void Linux_I_FatalError(const char* errortext);
#endif

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern volatile int game_running;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The command line arguments.
FArgs *Args;

// PRIVATE DATA DEFINITIONS ------------------------------------------------


// CODE --------------------------------------------------------------------

void exit_handler(int dummy) {
        game_running = 0;
}

static void NewFailure ()
{
    I_FatalError ("Failed to allocate memory from system heap");
}

static int DoomSpecificInfo (char *buffer, char *end)
{
	const char *arg;
	int size = end-buffer-2;
	int i, p;

	p = 0;
	p += snprintf (buffer+p, size-p, GAMENAME" version %s (%s)\n", GetVersionString(), GetGitHash());
#ifdef __VERSION__
	p += snprintf (buffer+p, size-p, "Compiler version: %s\n", __VERSION__);
#endif
	p += snprintf (buffer+p, size-p, "\nCommand line:");
	for (i = 0; i < Args->NumArgs(); ++i)
	{
		p += snprintf (buffer+p, size-p, " %s", Args->GetArg(i));
	}
	p += snprintf (buffer+p, size-p, "\n");
	
	for (i = 0; (arg = Wads.GetWadName (i)) != NULL; ++i)
	{
		p += snprintf (buffer+p, size-p, "\nWad %d: %s", i, arg);
	}

	if (gamestate != GS_LEVEL && gamestate != GS_TITLELEVEL)
	{
		p += snprintf (buffer+p, size-p, "\n\nNot in a level.");
	}
	else
	{
		p += snprintf (buffer+p, size-p, "\n\nCurrent map: %s", level.MapName.GetChars());

		if (!viewactive)
		{
			p += snprintf (buffer+p, size-p, "\n\nView not active.");
		}
		else
		{
			p += snprintf (buffer+p, size-p, "\n\nviewx = %f", r_viewpoint.Pos.X);
			p += snprintf (buffer+p, size-p, "\nviewy = %f", r_viewpoint.Pos.Y);
			p += snprintf (buffer+p, size-p, "\nviewz = %f", r_viewpoint.Pos.Z);
			p += snprintf (buffer+p, size-p, "\nviewangle = %f", r_viewpoint.Angles.Yaw.Degrees);
		}
	}
	buffer[p++] = '\n';
	buffer[p++] = '\0';

	return p;
}

void I_StartupJoysticks();
void I_ShutdownJoysticks();

#ifdef __ANDROID__

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"JNITouchControlsUtils", __VA_ARGS__))

int main_android (int argc, char **argv)
{
#else
int main (int argc, char **argv)
{
#endif
#if !defined (__APPLE__) && !defined (__ANDROID__)
	{
		int s[4] = { SIGSEGV, SIGILL, SIGFPE, SIGBUS };
		cc_install_handlers(argc, argv, 4, s, GAMENAMELOWERCASE "-crash.log", DoomSpecificInfo);
	}
#endif // !__APPLE__

	printf(GAMENAME" %s - %s - SDL version\nCompiled on %s\n",
		GetVersionString(), GetGitTime(), __DATE__);

	seteuid (getuid ());
    std::set_new_handler (NewFailure);

	// Set LC_NUMERIC environment variable in case some library decides to
	// clear the setlocale call at least this will be correct.
	// Note that the LANG environment variable is overridden by LC_*
	setenv ("LC_NUMERIC", "C", 1);

	setlocale (LC_ALL, "C");

	if (SDL_Init (0) < 0)
	{
		fprintf (stderr, "Could not initialize SDL:\n%s\n", SDL_GetError());
		return -1;
	}
	atterm (SDL_Quit);

	printf("\n");
	
    try
    {
		Args = new FArgs(argc, argv);

		/*
		  killough 1/98:

		  This fixes some problems with exit handling
		  during abnormal situations.

		  The old code called I_Quit() to end program,
		  while now I_Quit() is installed as an exit
		  handler and exit() is called to exit, either
		  normally or abnormally. Seg faults are caught
		  and the error handler is used, to prevent
		  being left in graphics mode or having very
		  loud SFX noise because the sound card is
		  left in an unstable state.
		*/

		atexit (call_terms);
		atterm (I_Quit);
		/*
		  Register signal handlers to interrupt D_DoomMain and D_DoomLoop, allowing
		  call_terms() to be invoked at the conclusion of the main thread/quit menu
		  rather than at exit. The atexit() call can remain to handle edge cases
		  where a signal cannot be intercepted, such as Alt+F4 or closing the window
		  via the GUI.

		  Fixes segmentation fault on exit when using the KMSDRM SDL video driver.
		*/
		signal(SIGINT, exit_handler);
		signal(SIGTERM, exit_handler);

		// Should we even be doing anything with progdir on Unix systems?
		char program[PATH_MAX];
		if (realpath (argv[0], program) == NULL)
			strcpy (program, argv[0]);
		char *slash = strrchr (program, '/');
		if (slash != NULL)
		{
			*(slash + 1) = '\0';
			progdir = program;
		}
		else
		{
			progdir = "./";
		}

		I_StartupJoysticks();
		C_InitConsole (80*8, 25*8, false);
		D_DoomMain ();
    }
    catch (std::exception &error)
    {
		I_ShutdownJoysticks();

		const char *const message = error.what();

		if (strcmp(message, "NoRunExit"))
		{
			if (CVMAbortException::stacktrace.IsNotEmpty())
			{
				Printf("%s", CVMAbortException::stacktrace.GetChars());
			}
#ifdef __ANDROID__
        	LOGI("FATAL ERROR: %s",  message);
#endif
			if (batchrun)
			{
				Printf("%s\n", message);
			}
			else
			{
#ifdef __APPLE__
				Mac_I_FatalError(message);
#endif // __APPLE__

#ifdef __linux__
				Linux_I_FatalError(message);
#endif // __linux__
			}
		}

		exit (-1);
    }
    catch (...)
    {
		call_terms ();
		throw;
    }
    call_terms();
    return 0;
}
