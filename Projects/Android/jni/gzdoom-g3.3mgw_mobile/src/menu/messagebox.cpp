/*
** messagebox.cpp
** Confirmation, notification screns
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
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

#include <ctype.h>
#include "menu/menu.h"
#include "d_event.h"
#include "d_gui.h"
#include "v_video.h"
#include "v_text.h"
#include "d_main.h"
#include "gstrings.h"
#include "gi.h"
#include "i_video.h"
#include "st_start.h"
#include "c_dispatch.h"
#include "g_game.h"
#include "vm.h"

EXTERN_CVAR (Bool, saveloadconfirmation) // [mxd]
EXTERN_CVAR (Bool, quicksaverotation)

CVAR(Bool, m_quickexit, false, CVAR_ARCHIVE)

typedef void(*hfunc)();
DEFINE_ACTION_FUNCTION(DMessageBoxMenu, CallHandler)
{
	PARAM_PROLOGUE;
	PARAM_POINTERTYPE(Handler, hfunc);
	Handler();
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

DMenu *CreateMessageBoxMenu(DMenu *parent, const char *message, int messagemode, bool playsound, FName action = NAME_None, hfunc handler = nullptr)
{
	auto c = PClass::FindClass(gameinfo.MessageBoxClass);
	if (!c->IsDescendantOf(NAME_MessageBoxMenu)) c = PClass::FindClass(NAME_MessageBoxMenu);
	auto p = c->CreateNew();
	FString namestr = message;

	IFVIRTUALPTRNAME(p, NAME_MessageBoxMenu, Init)
	{
		VMValue params[] = { p, parent, &namestr, messagemode, playsound, action.GetIndex(), reinterpret_cast<void*>(handler) };
		VMCall(func, params, countof(params), nullptr, 0);
		return (DMenu*)p;
	}
	return nullptr;
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (menu_quit)
{	// F10
	if (m_quickexit)
	{
		ST_Endoom();
	}

	M_StartControlPanel (true);

	const size_t messageindex = static_cast<size_t>(gametic) % gameinfo.quitmessages.Size();
	FString EndString;
	const char *msg = gameinfo.quitmessages[messageindex];
	if (msg[0] == '$')
	{
		if (msg[1] == '*')
		{
			EndString = GStrings(msg + 2);
		}
		else
		{
			EndString.Format("%s\n\n%s", GStrings(msg + 1), GStrings("DOSY"));
		}
	}
	else EndString = gameinfo.quitmessages[messageindex];

	DMenu *newmenu = CreateMessageBoxMenu(CurrentMenu, EndString, 0, false, NAME_None, []()
	{
		if (!netgame)
		{
			if (gameinfo.quitSound.IsNotEmpty())
			{
				S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.quitSound, snd_menuvolume, ATTN_NONE);
				I_WaitVBL(105);
			}
		}
		ST_Endoom();
	});


	M_ActivateMenu(newmenu);
}



//=============================================================================
//
//
//
//=============================================================================

void ActivateEndGameMenu()
{
	FString tempstring = GStrings(netgame ? "NETEND" : "ENDGAME");
	DMenu *newmenu = CreateMessageBoxMenu(CurrentMenu, tempstring, 0, false, NAME_None, []()
	{
		M_ClearMenus();
		if (!netgame)
		{
			if (demorecording)
				G_CheckDemoStatus();
			D_StartTitle();
		}
	});

	M_ActivateMenu(newmenu);
}

CCMD (menu_endgame)
{	// F7
	if (!usergame)
	{
		S_Sound (CHAN_VOICE, CHANF_UI, "menu/invalid", snd_menuvolume, ATTN_NONE);
		return;
	}
		
	//M_StartControlPanel (true);
	S_Sound (CHAN_VOICE, CHANF_UI, "menu/activate", snd_menuvolume, ATTN_NONE);

	ActivateEndGameMenu();
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (quicksave)
{	// F6
	if (!usergame || (players[consoleplayer].health <= 0 && !multiplayer))
	{
		S_Sound (CHAN_VOICE, CHANF_UI, "menu/invalid", snd_menuvolume, ATTN_NONE);
		return;
	}

	if (gamestate != GS_LEVEL)
		return;

	doquicksave = true;

	// If the quick save rotation is enabled, it handles the save slot.
	if (quicksaverotation)
	{
		G_DoQuickSave();
		return;
	}

	if (savegameManager.quickSaveSlot == NULL || savegameManager.quickSaveSlot == (FSaveGameNode*)1)
	{
		S_Sound(CHAN_VOICE, CHANF_UI, "menu/activate", snd_menuvolume, ATTN_NONE);
		M_StartControlPanel(false);
		// signal that whatever gets saved should be the new quicksave
		savegameManager.quickSaveSlot = (FSaveGameNode*)1;
		M_SetMenu(NAME_Savegamemenu);
		return;
	}

	// [mxd]. Just save the game, no questions asked.
	if (!saveloadconfirmation)
	{
		G_SaveGame(savegameManager.quickSaveSlot->Filename.GetChars(), savegameManager.quickSaveSlot->SaveTitle.GetChars());
		return;
	}

	S_Sound(CHAN_VOICE, CHANF_UI, "menu/activate", snd_menuvolume, ATTN_NONE);

	FString tempstring = GStrings("QSPROMPT");
	tempstring.Substitute("%s", savegameManager.quickSaveSlot->SaveTitle.GetChars());

	DMenu *newmenu = CreateMessageBoxMenu(CurrentMenu, tempstring, 0, false, NAME_None, []()
	{
		G_SaveGame(savegameManager.quickSaveSlot->Filename.GetChars(), savegameManager.quickSaveSlot->SaveTitle.GetChars());
		S_Sound(CHAN_VOICE, CHANF_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
		M_ClearMenus();
	});

	M_ActivateMenu(newmenu);
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (quickload)
{	// F9
	if (netgame)
	{
		M_StartControlPanel(true);
		M_StartMessage (GStrings("QLOADNET"), 1);
		return;
	}
		
	if (savegameManager.quickSaveSlot == NULL || savegameManager.quickSaveSlot == (FSaveGameNode*)1)
	{
		M_StartControlPanel(true);
		M_StartMessage (GStrings("QSAVESPOT"), 1);
		return;
	}

	// [mxd]. Just load the game, no questions asked.
	if (!saveloadconfirmation)
	{
		G_LoadGame(savegameManager.quickSaveSlot->Filename.GetChars());
		return;
	}
	FString tempstring = GStrings("QLPROMPT");
	tempstring.Substitute("%s", savegameManager.quickSaveSlot->SaveTitle.GetChars());

	M_StartControlPanel(true);

	DMenu *newmenu = CreateMessageBoxMenu(CurrentMenu, tempstring, 0, false, NAME_None, []()
	{
		G_LoadGame(savegameManager.quickSaveSlot->Filename.GetChars());
		S_Sound(CHAN_VOICE, CHANF_UI, "menu/dismiss", snd_menuvolume, ATTN_NONE);
		M_ClearMenus();
	});
	M_ActivateMenu(newmenu);
}

//=============================================================================
//
//
//
//=============================================================================

void M_StartMessage(const char *message, int messagemode, FName action)
{
	if (CurrentMenu == NULL) 
	{
		// only play a sound if no menu was active before
		M_StartControlPanel(menuactive == MENU_Off);
	}
	DMenu *newmenu = CreateMessageBoxMenu(CurrentMenu, message, messagemode, false, action);
	newmenu->mParentMenu = CurrentMenu;
	M_ActivateMenu(newmenu);
}

DEFINE_ACTION_FUNCTION(DMenu, StartMessage)
{
	PARAM_PROLOGUE;
	PARAM_STRING(msg);
	PARAM_INT(mode);
	PARAM_NAME(action);
	M_StartMessage(msg, mode, action);
	return 0;
}

UNSAFE_CCMD (quickunsetslot)
{
	if (savegameManager.quickSaveSlot != NULL) savegameManager.quickSaveSlot = (FSaveGameNode*)1;
	Printf("Quicksave slot unset.\n");
}
