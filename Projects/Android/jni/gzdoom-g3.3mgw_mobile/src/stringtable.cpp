/*
** stringtable.cpp
** Implements the FStringTable class
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

#include <string.h>
#include <stddef.h>

#include "stringtable.h"
#include "cmdlib.h"
#include "m_swap.h"
#include "w_wad.h"
#include "i_system.h"
#include "sc_man.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "gi.h"
#include "c_cvars.h"
#include "gstrings.h"

EXTERN_CVAR(String, language)

void FStringTable::LoadStrings ()
{
	int lastlump, lump;

	lastlump = 0;

	while ((lump = Wads.FindLump ("LANGUAGE", &lastlump)) != -1)
	{
		LoadLanguage (lump);
	}
	UpdateLanguage();
}

void FStringTable::LoadLanguage (int lumpnum)
{
	bool errordone = false;
	TArray<uint32_t> activeMaps;
	FScanner sc(lumpnum);
	sc.SetCMode (true);
	while (sc.GetString ())
	{
		if (sc.Compare ("["))
		{ // Process language identifiers
			activeMaps.Clear();
			sc.MustGetString ();
			do
			{
				size_t len = sc.StringLen;
				if (len != 2 && len != 3)
				{
					if (len == 1 && sc.String[0] == '~')
					{
						// deprecated and ignored
						sc.ScriptMessage("Deprecated option '~' found in language list");
						sc.MustGetString ();
						continue;
					}
					if (len == 1 && sc.String[0] == '*')
					{
						activeMaps.Push(global_table);
					}
					else if (len == 7 && stricmp (sc.String, "default") == 0)
					{
						activeMaps.Push(default_table);
					}
					else
					{
						sc.ScriptError ("The language code must be 2 or 3 characters long.\n'%s' is %lu characters long.",
							sc.String, len);
					}
				}
				else
				{
					activeMaps.Push(MAKE_ID(tolower(sc.String[0]), tolower(sc.String[1]), tolower(sc.String[2]), 0));
				}
				sc.MustGetString ();
			} while (!sc.Compare ("]"));
		}
		else
		{ // Process string definitions.
			if (activeMaps.Size() == 0)
			{
				// LANGUAGE lump is bad. We need to check if this is an old binary
				// lump and if so just skip it to allow old WADs to run which contain
				// such a lump.
				if (!sc.isText())
				{
					if (!errordone) Printf("Skipping binary 'LANGUAGE' lump.\n"); 
					errordone = true;
					return;
				}
				sc.ScriptError ("Found a string without a language specified.");
			}

			bool skip = false;
			if (sc.Compare("$"))
			{
				sc.MustGetStringName("ifgame");
				sc.MustGetStringName("(");
				sc.MustGetString();
				if (sc.Compare("strifeteaser"))
				{
					skip |= (gameinfo.gametype != GAME_Strife) || !(gameinfo.flags & GI_SHAREWARE);
				}
				else
				{
					skip |= !sc.Compare(GameTypeName());
				}
				sc.MustGetStringName(")");
				sc.MustGetString();

			}

			FName strName (sc.String);
			sc.MustGetStringName ("=");
			sc.MustGetString ();
			FString strText (sc.String, ProcessEscapes (sc.String));
			sc.MustGetString ();
			while (!sc.Compare (";"))
			{
				ProcessEscapes (sc.String);
				strText += sc.String;
				sc.MustGetString ();
			}
			if (!skip)
			{
				// Insert the string into all relevant tables.
				for (auto map : activeMaps)
				{
					allStrings[map].Insert(strName, strText);
				}
			}
		}
	}
}

void FStringTable::UpdateLanguage()
{
	size_t langlen = strlen(language);

	int LanguageID = (langlen < 2 || langlen > 3) ?
		MAKE_ID('e', 'n', 'u', '\0') :
		MAKE_ID(language[0], language[1], language[2], '\0');

	currentLanguageSet.Clear();

	auto checkone = [&](uint32_t lang_id)
	{
		auto list = allStrings.CheckKey(lang_id);
		if (list && currentLanguageSet.FindEx([&](const auto &element) { return element.first == lang_id; }) == currentLanguageSet.Size())
			currentLanguageSet.Push(std::make_pair(lang_id, list));
	};

	checkone(dehacked_table);
	checkone(global_table);
	checkone(LanguageID);
	checkone(LanguageID & MAKE_ID(0xff, 0xff, 0, 0));
	checkone(default_table);
}

// Replace \ escape sequences in a string with the escaped characters.
size_t FStringTable::ProcessEscapes (char *iptr)
{
	char *sptr = iptr, *optr = iptr, c;

	while ((c = *iptr++) != '\0')
	{
		if (c == '\\')
		{
			c = *iptr++;
			if (c == 'n')
				c = '\n';
			else if (c == 'c')
				c = TEXTCOLOR_ESCAPE;
			else if (c == 'r')
				c = '\r';
			else if (c == 't')
				c = '\t';
			else if (c == '\n')
				continue;
		}
		*optr++ = c;
	}
	*optr = '\0';
	return optr - sptr;
}

bool FStringTable::exists(const char *name)
{
	// Checks if the given key exists in any one of the default string tables that are valid for all languages.
	// To replace IWAD content this condition must be true.
	if (name == nullptr || *name == 0)
	{
		return false;
	}
	FName nm(name, true);
	if (nm != NAME_None)
	{
		uint32_t defaultStrings[] = { default_table, global_table, dehacked_table };

		for (auto mapid : defaultStrings)
		{
			auto map = allStrings.CheckKey(mapid);
			if (map)
			{
				auto item = map->CheckKey(nm);
				if (item) return true;
			}
		}
	}
	return false;
}

// Finds a string by name and returns its value
const char *FStringTable::GetString(const char *name, uint32_t *langtable) const
{
	if (name == nullptr || *name == 0)
	{
		return nullptr;
	}
	FName nm(name, true);
	if (nm != NAME_None)
	{
		for (auto map : currentLanguageSet)
		{
			auto item = map.second->CheckKey(nm);
			if (item)
			{
				if (langtable) *langtable = map.first;
				return item->GetChars();
			}
		}
	}
	return nullptr;
}

// Finds a string by name in a given language
const char *FStringTable::GetLanguageString(const char *name, uint32_t langtable) const
{
	if (name == nullptr || *name == 0)
	{
		return nullptr;
	}
	FName nm(name, true);
	if (nm != NAME_None)
	{
		auto map = allStrings.CheckKey(langtable);
		if (map == nullptr) return nullptr;
		auto item = map->CheckKey(nm);
		if (item)
		{
			return item->GetChars();
		}
	}
	return nullptr;
}

// Finds a string by name and returns its value. If the string does
// not exist, returns the passed name instead.
const char *FStringTable::operator() (const char *name) const
{
	const char *str = operator[] (name);
	return str ? str : name;
}


// Find a string with the same exact text. Returns its name.
const char *StringMap::MatchString (const char *string) const
{
	StringMap::ConstIterator it(*this);
	StringMap::ConstPair *pair;

	while (it.NextPair(pair))
	{
		if (pair->Value.CompareNoCase(string) == 0)
		{
			return pair->Key.GetChars();
		}
	}
	return nullptr;
}

CCMD(printlocalized)
{
	if (argv.argc() > 1)
	{
		if (argv.argc() > 2)
		{
			FString lang = argv[2];
			lang.ToLower();
			if (lang.Len() >= 2)
			{
				Printf("%s\n", GStrings.GetLanguageString(argv[1], MAKE_ID(lang[0], lang[1], lang[2], 0)));
				return;
			}
		}
		Printf("%s\n", GStrings(argv[1]));
	}

}
