//-----------------------------------------------------------------------------
//
// Copyright 1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//

// cmdlib.c (mostly borrowed from the Q2 source)

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#if !defined(__sun)
#include <fts.h>
#endif
#endif
#include "doomtype.h"
#include "cmdlib.h"
#include "i_system.h"
#include "v_text.h"
#include "sc_man.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

/*
progdir will hold the path up to the game directory, including the slash

  f:\quake\
  /raid/quake/

gamedir will hold progdir + the game directory (id1, id2, etc)

  */

FString progdir;

//==========================================================================
//
// IsSeperator
//
// Returns true if the character is a path seperator.
//
//==========================================================================

static inline bool IsSeperator (int c)
{
	if (c == '/')
		return true;
#ifdef WIN32
	if (c == '\\' || c == ':')
		return true;
#endif
	return false;
}

//==========================================================================
//
// FixPathSeperator
//
// Convert backslashes to forward slashes.
//
//==========================================================================

void FixPathSeperator (char *path)
{
	while (*path)
	{
		if (*path == '\\')
			*path = '/';
		path++;
	}
}

//==========================================================================
//
// copystring
//
// Replacement for strdup that uses new instead of malloc.
//
//==========================================================================

char *copystring (const char *s)
{
	char *b;
	if (s)
	{
		size_t len = strlen (s) + 1;
		b = new char[len];
		memcpy (b, s, len);
	}
	else
	{
		b = new char[1];
		b[0] = '\0';
	}
	return b;
}

//==========================================================================
//
// ReplaceString
//
// Do not use in new code.
//
//==========================================================================

void ReplaceString (char **ptr, const char *str)
{
	if (*ptr)
	{
		if (*ptr == str)
			return;
		delete[] *ptr;
	}
	*ptr = copystring (str);
}

/*
=============================================================================

						MISC FUNCTIONS

=============================================================================
*/


//==========================================================================
//
// FileExists
//
// Returns true if the given path exists and is a readable file.
//
//==========================================================================

bool FileExists (const char *filename)
{
	bool isdir;
	bool res = DirEntryExists(filename, &isdir);
	return res && !isdir;
}

//==========================================================================
//
// DirExists
//
// Returns true if the given path exists and is a directory.
//
//==========================================================================

bool DirExists(const char *filename)
{
	bool isdir;
	bool res = DirEntryExists(filename, &isdir);
	return res && isdir;
}

//==========================================================================
//
// DirEntryExists
//
// Returns true if the given path exists, be it a directory or a file.
//
//==========================================================================

bool DirEntryExists(const char *pathname, bool *isdir)
{
	if (isdir) *isdir = false;
	if (pathname == NULL || *pathname == 0)
		return false;

#ifndef _WIN32
	struct stat info;
	bool res = stat(pathname, &info) == 0;
#else
	// Windows must use the wide version of stat to preserve non-standard paths.
	auto wstr = WideString(pathname);
	struct _stat64i32 info;
	bool res = _wstat64i32(wstr.c_str(), &info) == 0;
#endif
	if (isdir) *isdir = !!(info.st_mode & S_IFDIR);
	return res;
}

//==========================================================================
//
// DirEntryExists
//
// Returns true if the given path exists, be it a directory or a file.
//
//==========================================================================

bool GetFileInfo(const char* pathname, size_t *size, time_t *time)
{
	if (pathname == NULL || *pathname == 0)
		return false;

#ifndef _WIN32
	struct stat info;
	bool res = stat(pathname, &info) == 0;
#else
	// Windows must use the wide version of stat to preserve non-standard paths.
	auto wstr = WideString(pathname);
	struct _stat64i32 info;
	bool res = _wstat64i32(wstr.c_str(), &info) == 0;
#endif
	if (!res || (info.st_mode & S_IFDIR)) return false;
	if (size) *size = info.st_size;
	if (time) *time = info.st_mtime;
	return res;
}

//==========================================================================
//
// DefaultExtension		-- FString version
//
// Appends the extension to a pathname if it does not already have one.
//
//==========================================================================

void DefaultExtension (FString &path, const char *extension)
{
	const char *src = &path[int(path.Len())-1];

	while (src != &path[0] && !IsSeperator(*src))
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	path += extension;
}


//==========================================================================
//
// ExtractFilePath
//
// Returns the directory part of a pathname.
//
// FIXME: should include the slash, otherwise
// backing to an empty path will be wrong when appending a slash
//
//==========================================================================

FString ExtractFilePath (const char *path)
{
	const char *src;

	src = path + strlen(path) - 1;

//
// back up until a \ or the start
//
	while (src != path && !IsSeperator(*(src-1)))
		src--;

	return FString(path, src - path);
}

//==========================================================================
//
// ExtractFileBase
//
// Returns the file part of a pathname, optionally including the extension.
//
//==========================================================================

FString ExtractFileBase (const char *path, bool include_extension)
{
	const char *src, *dot;

	src = path + strlen(path) - 1;

	if (src >= path)
	{
		// back up until a / or the start
		while (src != path && !IsSeperator(*(src-1)))
			src--;

		// Check for files with drive specification but no path
#if defined(_WIN32)
		if (src == path && src[0] != 0)
		{
			if (src[1] == ':')
				src += 2;
		}
#endif

		if (!include_extension)
		{
			dot = src;
			while (*dot && *dot != '.')
			{
				dot++;
			}
			return FString(src, dot - src);
		}
		else
		{
			return FString(src);
		}
	}
	return FString();
}


//==========================================================================
//
// ParseHex
//
//==========================================================================

int ParseHex (const char *hex, FScriptPosition *sc)
{
	const char *str;
	int num;

	num = 0;
	str = hex;

	while (*str)
	{
		num <<= 4;
		if (*str >= '0' && *str <= '9')
			num += *str-'0';
		else if (*str >= 'a' && *str <= 'f')
			num += 10 + *str-'a';
		else if (*str >= 'A' && *str <= 'F')
			num += 10 + *str-'A';
		else {
			if (!sc) Printf ("Bad hex number: %s\n",hex);
			else sc->Message(MSG_WARNING, "Bad hex number: %s", hex);
			return 0;
		}
		str++;
	}

	return num;
}

//==========================================================================
//
// IsNum
//
// [RH] Returns true if the specified string is a valid decimal number
//
//==========================================================================

bool IsNum (const char *str)
{
	while (*str)
	{
		if (((*str < '0') || (*str > '9')) && (*str != '-'))
		{
			return false;
		}
		str++;
	}
	return true;
}

//==========================================================================
//
// CheckWildcards
//
// [RH] Checks if text matches the wildcard pattern using ? or *
//
//==========================================================================

bool CheckWildcards (const char *pattern, const char *text)
{
	if (pattern == NULL || text == NULL)
		return true;

	while (*pattern)
	{
		if (*pattern == '*')
		{
			char stop = tolower (*++pattern);
			while (*text && tolower(*text) != stop)
			{
				text++;
			}
			if (*text && tolower(*text) == stop)
			{
				if (CheckWildcards (pattern, text++))
				{
					return true;
				}
				pattern--;
			}
		}
		else if (*pattern == '?' || tolower(*pattern) == tolower(*text))
		{
			pattern++;
			text++;
		}
		else
		{
			return false;
		}
	}
	return (*pattern | *text) == 0;
}

//==========================================================================
//
// FormatGUID
//
// [RH] Print a GUID to a text buffer using the standard format.
//
//==========================================================================

void FormatGUID (char *buffer, size_t buffsize, const GUID &guid)
{
	mysnprintf (buffer, buffsize, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
		(uint32_t)guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1],
		guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5],
		guid.Data4[6], guid.Data4[7]);
}

//==========================================================================
//
// myasctime
//
// [RH] Returns the current local time as ASCII, even if it's too early
//
//==========================================================================

const char *myasctime ()
{
	static char readabletime[50];
	time_t clock;
	struct tm *lt;

	time (&clock);
	lt = localtime (&clock);
	if (lt != NULL)
	{
		strftime(readabletime, 50, "%F %T", lt);
		return readabletime;
	}
	else
	{
		return "Unknown\n";
	}
}

//==========================================================================
//
// CreatePath
//
// Creates a directory including all levels necessary
//
//==========================================================================
#ifdef _WIN32
void DoCreatePath(const char *fn)
{
	char drive[_MAX_DRIVE];
#ifndef __MINGW32__
	char dir[_MAX_DIR];
	_splitpath_s(fn, drive, sizeof drive, dir, sizeof dir, nullptr, 0, nullptr, 0);

	if ('\0' == *dir)
	{
		// Root/current/parent directory always exists
		return;
	}

	char path[PATH_MAX];
	_makepath_s(path, sizeof path, drive, dir, nullptr, nullptr);

	if ('\0' == *path)
	{
		// No need to process empty relative path
		return;
	}

	// Remove trailing path separator(s)
	for (size_t i = strlen(path); 0 != i; --i)
	{
		char& lastchar = path[i - 1];

		if ('/' == lastchar || '\\' == lastchar)
		{
			lastchar = '\0';
		}
		else
		{
			break;
		}
	}

	// Create all directories for given path
	if ('\0' != *path)
	{
		DoCreatePath(path);
#ifdef _WIN32
		auto wpath = WideString(path);
		_wmkdir(wpath.c_str());
#else
		_mkdir(path);
#endif
	}
#else
	char path[PATH_MAX];
	char p[PATH_MAX];
	int i;

	_splitpath(fn,drive,path,NULL,NULL);
	_makepath(p,drive,path,NULL,NULL);
	i=(int)strlen(p);
	if (p[i-1]=='/' || p[i-1]=='\\') p[i-1]=0;
	if (*path) DoCreatePath(p);
	_mkdir(p);
#endif
}

void CreatePath(const char *fn)
{
	char c = fn[strlen(fn)-1];

	if (c != '\\' && c != '/') 
	{
		FString name(fn);
		name += '/';
		DoCreatePath(name);
	}
	else
	{
		DoCreatePath(fn);
	}
}
#else
void CreatePath(const char *fn)
{
	char *copy, *p;
 
	if (fn[0] == '/' && fn[1] == '\0')
	{
		return;
	}
	p = copy = strdup(fn);
	do
	{
		p = strchr(p + 1, '/');
		if (p != NULL)
		{
			*p = '\0';
		}
		if (!DirEntryExists(copy) && mkdir(copy, 0755) == -1)
		{
			// failed
			free(copy);
			return;
		}
		if (p != NULL)
		{
			*p = '/';
		}
	} while (p);
	free(copy);
}
#endif

//==========================================================================
//
// strbin	-- In-place version
//
// [RH] Replaces the escape sequences in a string with actual escaped characters.
// This operation is done in-place. The result is the new length of the string.
//
//==========================================================================

int strbin (char *str)
{
	char *start = str;
	char *p = str, c;
	int i;

	while ( (c = *p++) ) {
		if (c != '\\') {
			*str++ = c;
		} else {
			switch (*p) {
				case 'a':
					*str++ = '\a';
					break;
				case 'b':
					*str++ = '\b';
					break;
				case 'c':
					*str++ = '\034';	// TEXTCOLOR_ESCAPE
					break;
				case 'f':
					*str++ = '\f';
					break;
				case 'n':
					*str++ = '\n';
					break;
				case 't':
					*str++ = '\t';
					break;
				case 'r':
					*str++ = '\r';
					break;
				case 'v':
					*str++ = '\v';
					break;
				case '?':
					*str++ = '\?';
					break;
				case '\n':
					break;
				case 'x':
				case 'X':
					c = 0;
					for (i = 0; i < 2; i++)
					{
						p++;
						if (*p >= '0' && *p <= '9')
							c = (c << 4) + *p-'0';
						else if (*p >= 'a' && *p <= 'f')
							c = (c << 4) + 10 + *p-'a';
						else if (*p >= 'A' && *p <= 'F')
							c = (c << 4) + 10 + *p-'A';
						else
						{
							p--;
							break;
						}
					}
					*str++ = c;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = *p - '0';
					for (i = 0; i < 2; i++)
					{
						p++;
						if (*p >= '0' && *p <= '7')
							c = (c << 3) + *p - '0';
						else
						{
							p--;
							break;
						}
					}
					*str++ = c;
					break;
				default:
					*str++ = *p;
					break;
			}
			p++;
		}
	}
	*str = 0;
	return int(str - start);
}

//==========================================================================
//
// strbin1	-- String-creating version
//
// [RH] Replaces the escape sequences in a string with actual escaped characters.
// The result is a new string.
//
//==========================================================================

FString strbin1 (const char *start)
{
	FString result;
	const char *p = start;
	char c;
	int i;

	while ( (c = *p++) ) {
		if (c != '\\') {
			result << c;
		} else {
			switch (*p) {
				case 'a':
					result << '\a';
					break;
				case 'b':
					result << '\b';
					break;
				case 'c':
					result << '\034';	// TEXTCOLOR_ESCAPE
					break;
				case 'f':
					result << '\f';
					break;
				case 'n':
					result << '\n';
					break;
				case 't':
					result << '\t';
					break;
				case 'r':
					result << '\r';
					break;
				case 'v':
					result << '\v';
					break;
				case '?':
					result << '\?';
					break;
				case '\n':
					break;
				case 'x':
				case 'X':
					c = 0;
					for (i = 0; i < 2; i++)
					{
						p++;
						if (*p >= '0' && *p <= '9')
							c = (c << 4) + *p-'0';
						else if (*p >= 'a' && *p <= 'f')
							c = (c << 4) + 10 + *p-'a';
						else if (*p >= 'A' && *p <= 'F')
							c = (c << 4) + 10 + *p-'A';
						else
						{
							p--;
							break;
						}
					}
					result << c;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = *p - '0';
					for (i = 0; i < 2; i++)
					{
						p++;
						if (*p >= '0' && *p <= '7')
							c = (c << 3) + *p - '0';
						else
						{
							p--;
							break;
						}
					}
					result << c;
					break;
				default:
					result << *p;
					break;
			}
			p++;
		}
	}
	return result;
}

//==========================================================================
//
// CleanseString
//
// Does some mild sanity checking on a string: If it ends with an incomplete
// color escape, the escape is removed.
//
//==========================================================================

char *CleanseString(char *str)
{
	char *escape = strrchr(str, TEXTCOLOR_ESCAPE);
	if (escape != NULL)
	{
		if (escape[1] == '\0')
		{
			*escape = '\0';
		}
		else if (escape[1] == '[')
		{
			char *close = strchr(escape + 2, ']');
			if (close == NULL)
			{
				*escape = '\0';
			}
		}
	}
	return str;
}

//==========================================================================
//
// ExpandEnvVars
//
// Expands environment variable references in a string. Intended primarily
// for use with IWAD search paths in config files.
//
//==========================================================================

FString ExpandEnvVars(const char *searchpathstring)
{
	static const char envvarnamechars[] =
		"01234567890"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"_"
		"abcdefghijklmnopqrstuvwxyz";

	if (searchpathstring == NULL)
		return FString("");

	const char *dollar = strchr(searchpathstring, '$');
	if (dollar == NULL)
	{
		return FString(searchpathstring);
	}

	const char *nextchars = searchpathstring;
	FString out = FString(searchpathstring, dollar - searchpathstring);
	while ( (dollar != NULL) && (*nextchars != 0) )
	{
		size_t length = strspn(dollar + 1, envvarnamechars);
		if (length != 0)
		{
			FString varname = FString(dollar + 1, length);
			if (stricmp(varname, "progdir") == 0)
			{
				out += progdir;
			}
			else
			{
				char *varvalue = getenv(varname);
				if ( (varvalue != NULL) && (strlen(varvalue) != 0) )
				{
					out += varvalue;
				}
			}
		}
		else
		{
			out += '$';
		}
		nextchars = dollar + length + 1;
		dollar = strchr(nextchars, '$');
		if (dollar != NULL)
		{
			out += FString(nextchars, dollar - nextchars);
		}
	}
	if (*nextchars != 0)
	{
		out += nextchars;
	}
	return out;
}

//==========================================================================
//
// NicePath
//
// Handles paths with leading ~ characters on Unix as well as environment
// variable substitution. On Windows, this is identical to ExpandEnvVars.
//
//==========================================================================

FString NicePath(const char *path)
{
#ifdef _WIN32
	return ExpandEnvVars(path);
#else
	if (path == NULL || *path == '\0')
	{
		return FString("");
	}
	if (*path != '~')
	{
		return ExpandEnvVars(path);
	}

	passwd *pwstruct;
	const char *slash;

	if (path[1] == '/' || path[1] == '\0')
	{ // Get my home directory
		pwstruct = getpwuid(getuid());
		slash = path + 1;
	}
	else
	{ // Get somebody else's home directory
		slash = strchr(path, '/');
		if (slash == NULL)
		{
			slash = path + strlen(path);
		}
		FString who(path, slash - path);
		pwstruct = getpwnam(who);
	}
	if (pwstruct == NULL)
	{
		return ExpandEnvVars(path);
	}
	FString where(pwstruct->pw_dir);
	if (*slash != '\0')
	{
		where += ExpandEnvVars(slash);
	}
	return where;
#endif
}


//==========================================================================
//
// ScanDirectory
//
//==========================================================================

void ScanDirectory(TArray<FFileList> &list, const char *dirpath)
{
	findstate_t find;
	FString dirmatch;

	dirmatch << dirpath << "*";

	auto handle = I_FindFirst(dirmatch.GetChars(), &find);
	if (handle == ((void*)(-1)))
	{
		I_Error("Could not scan '%s': %s\n", dirpath, strerror(errno));
	}
	else
	{
		do
		{
			auto attr = I_FindAttr(&find);
			if (attr & FA_HIDDEN)
			{
				// Skip hidden files and directories. (Prevents SVN bookkeeping
				// info from being included.)
				continue;
			}
			auto fn = I_FindName(&find);

			if (attr & FA_DIREC)
			{
				if (fn[0] == '.' &&
					(fn[1] == '\0' ||
					(fn[1] == '.' && fn[2] == '\0')))
				{
					// Do not record . and .. directories.
					continue;
				}

				FFileList *fl = &list[list.Reserve(1)];
				fl->Filename << dirpath << fn;
				fl->isDirectory = true;
				FString newdir = fl->Filename;
				newdir << "/";
				ScanDirectory(list, newdir);
			}
			else
			{
				FFileList *fl = &list[list.Reserve(1)];
				fl->Filename << dirpath << fn;
				fl->isDirectory = false;
			}
		} 
		while (I_FindNext(handle, &find) == 0);
		I_FindClose(handle);
	}
}


//==========================================================================
//
//
//
//==========================================================================

bool IsAbsPath(const char *name)
{
    if (IsSeperator(name[0])) return true;
#ifdef _WIN32
    /* [A-Za-z]: (for Windows) */
    if (isalpha(name[0]) && name[1] == ':')    return true;
#endif /* _WIN32 */
    return 0;
}
