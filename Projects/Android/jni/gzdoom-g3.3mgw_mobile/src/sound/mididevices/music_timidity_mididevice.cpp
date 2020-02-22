/*
** music_timidity_mididevice.cpp
** Provides access to TiMidity as a generic MIDI device.
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
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

#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "w_wad.h"
#include "v_text.h"
#include "timidity/timidity.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Internal TiMidity MIDI device --------------------------------------------

namespace Timidity { struct Renderer; }

class TimidityMIDIDevice : public SoftSynthMIDIDevice
{
public:
	TimidityMIDIDevice(const char *args, int samplerate);
	~TimidityMIDIDevice();
	
	int Open(MidiCallback, void *userdata);
	void PrecacheInstruments(const uint16_t *instruments, int count);
	FString GetStats();
	int GetDeviceType() const override { return MDEV_GUS; }
	
protected:
	Timidity::Renderer *Renderer;
	
	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
};


// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// TimidityMIDIDevice Constructor
//
//==========================================================================

TimidityMIDIDevice::TimidityMIDIDevice(const char *args, int samplerate)
	: SoftSynthMIDIDevice(samplerate, 11025, 65535)
{
	Renderer = new Timidity::Renderer((float)SampleRate, args);
}

//==========================================================================
//
// TimidityMIDIDevice Destructor
//
//==========================================================================

TimidityMIDIDevice::~TimidityMIDIDevice()
{
	Close();
	if (Renderer != nullptr)
	{
		delete Renderer;
	}
}

//==========================================================================
//
// TimidityMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int TimidityMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	int ret = OpenStream(2, 0, callback, userdata);
	if (ret == 0)
	{
		Renderer->Reset();
	}
	return ret;
}

//==========================================================================
//
// TimidityMIDIDevice :: PrecacheInstruments
//
// Each entry is packed as follows:
//   Bits 0- 6: Instrument number
//   Bits 7-13: Bank number
//   Bit    14: Select drum set if 1, tone bank if 0
//
//==========================================================================

void TimidityMIDIDevice::PrecacheInstruments(const uint16_t *instruments, int count)
{
	for (int i = 0; i < count; ++i)
	{
		Renderer->MarkInstrument((instruments[i] >> 7) & 127, instruments[i] >> 14, instruments[i] & 127);
	}
	Renderer->load_missing_instruments();
}

//==========================================================================
//
// TimidityMIDIDevice :: HandleEvent
//
//==========================================================================

void TimidityMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	Renderer->HandleEvent(status, parm1, parm2);
}

//==========================================================================
//
// TimidityMIDIDevice :: HandleLongEvent
//
//==========================================================================

void TimidityMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
	Renderer->HandleLongMessage(data, len);
}

//==========================================================================
//
// TimidityMIDIDevice :: ComputeOutput
//
//==========================================================================

void TimidityMIDIDevice::ComputeOutput(float *buffer, int len)
{
	Renderer->ComputeOutput(buffer, len);
	for (int i = 0; i < len * 2; i++) buffer[i] *= 0.7f;
}

//==========================================================================
//
// TimidityMIDIDevice :: GetStats
//
//==========================================================================

FString TimidityMIDIDevice::GetStats()
{
	FString dots;
	FString out;
	int i, used;

	std::lock_guard<std::mutex> lock(CritSec);
	for (i = used = 0; i < Renderer->voices; ++i)
	{
		int status = Renderer->voice[i].status;

		if (!(status & Timidity::VOICE_RUNNING))
		{
			dots << TEXTCOLOR_PURPLE".";
		}
		else
		{
			used++;
			if (status & Timidity::VOICE_SUSTAINING)
			{
				dots << TEXTCOLOR_BLUE;
			}
			else if (status & Timidity::VOICE_RELEASING)
			{
				dots << TEXTCOLOR_ORANGE;
			}
			else if (status & Timidity::VOICE_STOPPING)
			{
				dots << TEXTCOLOR_RED;
			}
			else
			{
				dots << TEXTCOLOR_GREEN;
			}
			if (!Renderer->voice[i].eg1.env.bUpdating)
			{
				dots << "+";
			}
			else
			{
				dots << ('0' + Renderer->voice[i].eg1.gf1.stage);
			}
		}
	}
	CritSec.unlock();
	out.Format(TEXTCOLOR_YELLOW"%3d/%3d ", used, Renderer->voices);
	out += dots;
	if (Renderer->cut_notes | Renderer->lost_notes)
	{
		out.AppendFormat(TEXTCOLOR_RED" %d/%d", Renderer->cut_notes, Renderer->lost_notes);
	}
	return out;
}

//==========================================================================
//
//
//
//==========================================================================

MIDIDevice *CreateTimidityMIDIDevice(const char *args, int samplerate)
{
	return new TimidityMIDIDevice(args, samplerate);
}

