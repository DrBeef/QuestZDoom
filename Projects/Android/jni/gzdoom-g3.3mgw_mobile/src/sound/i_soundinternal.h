#ifndef __SNDINT_H
#define __SNDINT_H

#include <stdio.h>

#include "doomtype.h"
#include "vectors.h"
#include "tarray.h"

class FileReader;

// For convenience, this structure matches FMOD_REVERB_PROPERTIES.
// Since I can't very well #include system-specific stuff in the
// main game files, I duplicate it here.
struct REVERB_PROPERTIES
{                
	int			 Instance;
    int			 Environment;
    float        EnvSize;
    float        EnvDiffusion;
    int          Room;
    int          RoomHF;
    int          RoomLF;
    float        DecayTime;
    float        DecayHFRatio;
    float        DecayLFRatio;
    int          Reflections;
    float        ReflectionsDelay;
    float        ReflectionsPan0;
	float        ReflectionsPan1;
	float        ReflectionsPan2;
    int          Reverb;
    float        ReverbDelay;
    float        ReverbPan0;
	float        ReverbPan1;
	float        ReverbPan2;
    float        EchoTime;
    float        EchoDepth;
    float        ModulationTime;
    float        ModulationDepth;
    float        AirAbsorptionHF;
    float        HFReference;
    float        LFReference;
    float        RoomRolloffFactor;
    float        Diffusion;
    float        Density;
    unsigned int Flags;
};

#define REVERB_FLAGS_DECAYTIMESCALE        0x00000001
#define REVERB_FLAGS_REFLECTIONSSCALE      0x00000002
#define REVERB_FLAGS_REFLECTIONSDELAYSCALE 0x00000004
#define REVERB_FLAGS_REVERBSCALE           0x00000008
#define REVERB_FLAGS_REVERBDELAYSCALE      0x00000010
#define REVERB_FLAGS_DECAYHFLIMIT          0x00000020
#define REVERB_FLAGS_ECHOTIMESCALE         0x00000040
#define REVERB_FLAGS_MODULATIONTIMESCALE   0x00000080

struct ReverbContainer
{
	ReverbContainer *Next;
	const char *Name;
	uint16_t ID;
	bool Builtin;
	bool Modified;
	REVERB_PROPERTIES Properties;
	bool SoftwareWater;
};

struct SoundListener
{
	FVector3 position;
	FVector3 velocity;
	float angle;
	bool underwater;
	bool valid;
	ReverbContainer *Environment;
};

// Default rolloff information.
struct FRolloffInfo
{
	int RolloffType;
	float MinDistance;
	union { float MaxDistance; float RolloffFactor; };
};

struct SoundHandle
{
	void *data;

	bool isValid() const { return data != NULL; }
	void Clear() { data = NULL; }

	bool operator==(const SoundHandle &rhs) const
	{ return data == rhs.data; }
	bool operator!=(const SoundHandle &rhs) const
	{ return !(*this == rhs); }
};

struct FISoundChannel
{
	void		*SysChannel;	// Channel information from the system interface.
	uint64_t	StartTime;		// Sound start time in DSP clocks.

	// The sound interface doesn't use these directly but it needs to pass them to a
	// callback that can't be passed a sound channel pointer
	FRolloffInfo Rolloff;
	float		DistanceScale;
	float		DistanceSqr;
	bool		ManualRolloff;
	int			ChanFlags;
};


void FindLoopTags(FileReader &fr, uint32_t *start, bool *startass, uint32_t *end, bool *endass);


enum SampleType
{
    SampleType_UInt8,
    SampleType_Int16
};
enum ChannelConfig
{
    ChannelConfig_Mono,
    ChannelConfig_Stereo
};

const char *GetSampleTypeName(enum SampleType type);
const char *GetChannelConfigName(enum ChannelConfig chan);

struct SoundDecoder
{
    virtual void getInfo(int *samplerate, ChannelConfig *chans, SampleType *type) = 0;

    virtual size_t read(char *buffer, size_t bytes) = 0;
    virtual TArray<uint8_t> readAll();
    virtual bool seek(size_t ms_offset, bool ms, bool mayrestart) = 0;
    virtual size_t getSampleOffset() = 0;
    virtual size_t getSampleLength() { return 0; }

    SoundDecoder() { }
    virtual ~SoundDecoder() { }

protected:
    virtual bool open(FileReader &reader) = 0;
    friend class SoundRenderer;

private:
    // Make non-copyable
    SoundDecoder(const SoundDecoder &rhs);
    SoundDecoder& operator=(const SoundDecoder &rhs);
};

enum EMidiDevice
{
	MDEV_DEFAULT = -1,
	MDEV_MMAPI = 0,
	MDEV_OPL = 1,
	MDEV_SNDSYS = 2,
	MDEV_TIMIDITY = 3,
	MDEV_FLUIDSYNTH = 4,
	MDEV_GUS = 5,
	MDEV_WILDMIDI = 6,
	MDEV_ADL = 7,
	MDEV_OPN = 8,

	MDEV_COUNT
};

class MusInfo;
struct MusPlayingInfo
{
	FString name;
	MusInfo *handle;
	int   baseorder;
	bool  loop;
};



#endif
