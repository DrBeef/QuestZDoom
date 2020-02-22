/*
** music_gme.cpp
** General game music player, using Game Music Emu for decoding.
**
**---------------------------------------------------------------------------
** Copyright 2009 Randy Heit
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

// Uncomment if you are using the DLL version of GME.
//#define GME_DLL

#include "i_musicinterns.h"
#include "c_cvars.h"
#include <gme/gme.h>
#include <mutex>
#include "v_text.h"
#include "files.h"
#include "templates.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class GMESong : public StreamSong
{
public:
	GMESong(Music_Emu *emu, int sample_rate);
	~GMESong();
	bool SetSubsong(int subsong);
	void Play(bool looping, int subsong);
	FString GetStats();

protected:
	std::mutex CritSec;
	Music_Emu *Emu;
	gme_info_t *TrackInfo;
	int SampleRate;
	int CurrTrack;

	bool StartTrack(int track, bool getcritsec=true);
	bool GetTrackInfo();
	int CalcSongLength();
	void GMEDepthChanged(float val);

	static bool Read(SoundStream *stream, void *buff, int len, void *userdata);
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Currently not used.
CVAR (Float, spc_amp, 1.875f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR(Float, gme_stereodepth, 0.f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr)
		currSong->GMEDepthChanged(self);
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// GME_CheckFormat
//
//==========================================================================

const char *GME_CheckFormat(uint32_t id)
{
	return gme_identify_header(&id);
}

//==========================================================================
//
// GME_OpenSong
//
//==========================================================================

MusInfo *GME_OpenSong(FileReader &reader, const char *fmt)
{
	gme_type_t type;
	gme_err_t err;
	uint8_t *song;
	Music_Emu *emu;
	int sample_rate;
	
	type = gme_identify_extension(fmt);
	if (type == NULL)
	{
		return NULL;
	}
	sample_rate = (int)GSnd->GetOutputRate();
	emu = gme_new_emu(type, sample_rate);
	if (emu == nullptr)
	{
		return nullptr;
	}

    auto fpos = reader.Tell();
	auto len = reader.GetLength();
    song = new uint8_t[len];
    if (reader.Read(song, len) != len)
    {
        delete[] song;
        gme_delete(emu);
        reader.Seek(fpos, FileReader::SeekSet);
        return nullptr;
    }

	err = gme_load_data(emu, song, (long)len);
    delete[] song;

	if (err != nullptr)
	{
		Printf("Failed loading song: %s\n", err);
		gme_delete(emu);
        reader.Seek(fpos, FileReader::SeekSet);
		return nullptr;
	}
	gme_set_stereo_depth(emu, clamp(*gme_stereodepth, 0.f, 1.f));
	gme_set_fade(emu, -1); // Enable infinite loop

#if GME_VERSION >= 0x602
	gme_set_autoload_playback_limit(emu, 0);
#endif // GME_VERSION >= 0x602

	return new GMESong(emu, sample_rate);
}

//==========================================================================
//
// GMESong - Constructor
//
//==========================================================================

GMESong::GMESong(Music_Emu *emu, int sample_rate)
{
	Emu = emu;
	SampleRate = sample_rate;
	CurrTrack = 0;
	TrackInfo = NULL;
	m_Stream = GSnd->CreateStream(Read, 32*1024, 0, sample_rate, this);
}

//==========================================================================
//
// GMESong - Destructor
//
//==========================================================================

GMESong::~GMESong()
{
	Stop();
	if (m_Stream != NULL)
	{
		delete m_Stream;
		m_Stream = NULL;
	}
	if (TrackInfo != NULL)
	{
		gme_free_info(TrackInfo);
	}
	if (Emu != NULL)
	{
		gme_delete(Emu);
	}
}


//==========================================================================
//
// GMESong :: GMEDepthChanged
//
//==========================================================================

void GMESong::GMEDepthChanged(float val)
{
	if (Emu != nullptr)
		gme_set_stereo_depth(Emu, clamp(val, 0.f, 1.f));
}


//==========================================================================
//
// GMESong :: Play
//
//==========================================================================

void GMESong::Play(bool looping, int track)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;
	if (StartTrack(track) && m_Stream->Play(looping, 1))
	{
		m_Status = STATE_Playing;
	}
}

//==========================================================================
//
// GMESong :: SetSubsong
//
//==========================================================================

bool GMESong::SetSubsong(int track)
{
	if (CurrTrack == track)
	{
		return false;
	}
	return StartTrack(track);
}

//==========================================================================
//
// GMESong :: StartTrack
//
//==========================================================================

bool GMESong::StartTrack(int track, bool getcritsec)
{
	gme_err_t err;

	if (getcritsec)
	{
		std::lock_guard<std::mutex> lock(CritSec);
		err = gme_start_track(Emu, track);
	}
	else
	{
		err = gme_start_track(Emu, track);
	}
	if (err != NULL)
	{
		Printf("Could not start track %d: %s\n", track, err);
		return false;
	}
	CurrTrack = track;
	GetTrackInfo();
	if (!m_Looping)
	{
		gme_set_fade(Emu, CalcSongLength());
	}
	return true;
}

//==========================================================================
//
// GMESong :: GetStats
//
//==========================================================================

FString GMESong::GetStats()
{
	FString out;

	if (TrackInfo != NULL)
	{
		int time = gme_tell(Emu);
		out.Format(
			"Track: " TEXTCOLOR_YELLOW "%d" TEXTCOLOR_NORMAL
			"  Time:" TEXTCOLOR_YELLOW "%3d:%02d:%03d" TEXTCOLOR_NORMAL
			"  System: " TEXTCOLOR_YELLOW "%s" TEXTCOLOR_NORMAL,
			CurrTrack,
			time/60000,
			(time/1000) % 60,
			time % 1000,
			TrackInfo->system);
	}
	return out;
}

//==========================================================================
//
// GMESong :: GetTrackInfo
//
//==========================================================================

bool GMESong::GetTrackInfo()
{
	gme_err_t err;

	if (TrackInfo != NULL)
	{
		gme_free_info(TrackInfo);
		TrackInfo = NULL;
	}
	err = gme_track_info(Emu, &TrackInfo, CurrTrack);
	if (err != NULL)
	{
		Printf("Could not get track %d info: %s\n", CurrTrack, err);
		return false;
	}
	return true;
}

//==========================================================================
//
// GMESong :: CalcSongLength
//
//==========================================================================

int GMESong::CalcSongLength()
{
	if (TrackInfo == NULL)
	{
		return 150000;
	}
	if (TrackInfo->length > 0)
	{
		return TrackInfo->length;
	}
	if (TrackInfo->loop_length > 0)
	{
		return TrackInfo->intro_length + TrackInfo->loop_length * 2;
	}
	return 150000;
}

//==========================================================================
//
// GMESong :: Read													STATIC
//
//==========================================================================

bool GMESong::Read(SoundStream *stream, void *buff, int len, void *userdata)
{
	gme_err_t err;
	GMESong *song = (GMESong *)userdata;

	std::lock_guard<std::mutex> lock(song->CritSec);
	if (gme_track_ended(song->Emu))
	{
		if (song->m_Looping)
		{
			song->StartTrack(song->CurrTrack, false);
		}
		else
		{
			memset(buff, 0, len);
			return false;
		}
	}
	err = gme_play(song->Emu, len >> 1, (short *)buff);
	return (err == NULL);
}
