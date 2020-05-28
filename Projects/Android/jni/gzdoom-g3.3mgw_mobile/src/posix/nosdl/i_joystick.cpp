/*
** i_joystick.cpp
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
#include "doomdef.h"
#include "version.h"
#include "templates.h"
#include "m_joy.h"

// Very small deadzone so that floating point magic doesn't happen
#define MIN_DEADZONE 0.000001f

CUSTOM_CVAR(Bool, joy_background, false, CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

CVAR (Bool, use_mouse,				false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG) // not used, present for scripts only

class NoSDLInputJoystickManager
{
public:
	NoSDLInputJoystickManager()
	{
	}
	~NoSDLInputJoystickManager()
	{
	}

	void AddAxes(float axes[5])
	{
	}

	void GetDevices(TArray<IJoystickConfig *> &sticks)
	{
	}

	void ProcessInput() const
	{
	}
};

static NoSDLInputJoystickManager *JoystickManager;

void I_StartupJoysticks()
{
	JoystickManager = new NoSDLInputJoystickManager();
}
void I_ShutdownJoysticks()
{
	if(JoystickManager)
	{
		delete JoystickManager;
	}
}

void I_GetJoysticks(TArray<IJoystickConfig *> &sticks)
{
	sticks.Clear();

	JoystickManager->GetDevices(sticks);
}

void I_GetAxes(float axes[NUM_JOYAXIS])
{
	for (int i = 0; i < NUM_JOYAXIS; ++i)
	{
		axes[i] = 0;
	}
	if (use_joystick)
	{
		JoystickManager->AddAxes(axes);
	}
}

void I_ProcessJoysticks()
{
	if (use_joystick)
		JoystickManager->ProcessInput();
}

IJoystickConfig *I_UpdateDeviceList()
{
	return NULL;
}
