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
#include "adlmidi/adlmidi.h"
#include "i_soundfont.h"

class ADLMIDIDevice : public SoftSynthMIDIDevice
{
	struct ADL_MIDIPlayer *Renderer;
public:
	ADLMIDIDevice(const char *args);
	~ADLMIDIDevice();
	
	int Open(MidiCallback, void *userdata);
	int GetDeviceType() const override { return MDEV_ADL; }
	
protected:
	
	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
	
private:
	int LoadCustomBank(const char *bankfile);
};


enum
{
	ME_NOTEOFF = 0x80,
	ME_NOTEON = 0x90,
	ME_KEYPRESSURE = 0xA0,
	ME_CONTROLCHANGE = 0xB0,
	ME_PROGRAM = 0xC0,
	ME_CHANNELPRESSURE = 0xD0,
	ME_PITCHWHEEL = 0xE0
};

CUSTOM_CVAR(Int, adl_chips_count, 6, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_ADL)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CUSTOM_CVAR(Int, adl_emulator_id, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_ADL)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CUSTOM_CVAR(Bool, adl_run_at_pcm_rate, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_ADL)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CUSTOM_CVAR(Bool, adl_fullpan, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_ADL)
	{
		MIDIDeviceChanged(-1, true);
	}
}


CUSTOM_CVAR(Int, adl_bank, 14, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_ADL)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CUSTOM_CVAR(Bool, adl_use_custom_bank, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_ADL)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CUSTOM_CVAR(String, adl_custom_bank, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (adl_use_custom_bank && currSong != nullptr && currSong->GetDeviceType() == MDEV_ADL)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CUSTOM_CVAR(Int, adl_volume_model, ADLMIDI_VolumeModel_DMX, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_ADL)
	{
		MIDIDeviceChanged(-1, true);
	}
}

//==========================================================================
//
// ADLMIDIDevice Constructor
//
//==========================================================================

ADLMIDIDevice::ADLMIDIDevice(const char *args)
	:SoftSynthMIDIDevice(44100)
{
	Renderer = adl_init(44100);	// todo: make it configurable
	if (Renderer != nullptr)
	{
		adl_switchEmulator(Renderer, (int)adl_emulator_id);
		adl_setRunAtPcmRate(Renderer, (int)adl_run_at_pcm_rate);
		if(!LoadCustomBank(adl_custom_bank))
			adl_setBank(Renderer, (int)adl_bank);
		adl_setNumChips(Renderer, (int)adl_chips_count);
		adl_setVolumeRangeModel(Renderer, (int)adl_volume_model);
		adl_setSoftPanEnabled(Renderer, (int)adl_fullpan);
	}
}

//==========================================================================
//
// ADLMIDIDevice Destructor
//
//==========================================================================

ADLMIDIDevice::~ADLMIDIDevice()
{
	Close();
	if (Renderer != nullptr)
	{
		adl_close(Renderer);
	}
}

//==========================================================================
//
// ADLMIDIDevice :: LoadCustomBank
//
// Loads a custom WOPL bank for libADLMIDI. Returns 1 when bank has been
// loaded, otherwise, returns 0 when custom banks are disabled or failed
//
//==========================================================================

int ADLMIDIDevice::LoadCustomBank(const char *bankfile)
{
	if(!adl_use_custom_bank)
		return 0;
	auto info = sfmanager.FindSoundFont(bankfile, SF_WOPL);
	if(info == nullptr)
		return 0;
	bankfile = info->mFilename.GetChars();
	return (adl_openBankFile(Renderer, bankfile) == 0);
}


//==========================================================================
//
// ADLMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int ADLMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	int ret = OpenStream(2, 0, callback, userdata);
	if (ret == 0)
	{
		adl_rt_resetState(Renderer);
	}
	return ret;
}

//==========================================================================
//
// ADLMIDIDevice :: HandleEvent
//
//==========================================================================

void ADLMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	int command = status & 0xF0;
	int chan	= status & 0x0F;

	switch (command)
	{
	case ME_NOTEON:
		adl_rt_noteOn(Renderer, chan, parm1, parm2);
		break;

	case ME_NOTEOFF:
		adl_rt_noteOff(Renderer, chan, parm1);
		break;

	case ME_KEYPRESSURE:
		adl_rt_noteAfterTouch(Renderer, chan, parm1, parm2);
		break;

	case ME_CONTROLCHANGE:
		adl_rt_controllerChange(Renderer, chan, parm1, parm2);
		break;

	case ME_PROGRAM:
		adl_rt_patchChange(Renderer, chan, parm1);
		break;

	case ME_CHANNELPRESSURE:
		adl_rt_channelAfterTouch(Renderer, chan, parm1);
		break;

	case ME_PITCHWHEEL:
		adl_rt_pitchBendML(Renderer, chan, parm2, parm1);
		break;
	}
}

//==========================================================================
//
// ADLMIDIDevice :: HandleLongEvent
//
//==========================================================================

void ADLMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
}

static const ADLMIDI_AudioFormat audio_output_format =
{
	ADLMIDI_SampleType_F32,
	sizeof(float),
	2 * sizeof(float)
};

//==========================================================================
//
// ADLMIDIDevice :: ComputeOutput
//
//==========================================================================

void ADLMIDIDevice::ComputeOutput(float *buffer, int len)
{
	ADL_UInt8* left = reinterpret_cast<ADL_UInt8*>(buffer);
	ADL_UInt8* right = reinterpret_cast<ADL_UInt8*>(buffer + 1);
	auto result = adl_generateFormat(Renderer, len * 2, left, right, &audio_output_format);
	for(int i=0; i < result; i++)
	{
		buffer[i] *= 3.5f;
	}
}

//==========================================================================
//
//
//
//==========================================================================

MIDIDevice *CreateADLMIDIDevice(const char *args)
{
	return new ADLMIDIDevice(args);
}


