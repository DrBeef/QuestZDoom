/*==============================================================================
Gain DSP Plugin Example
Copyright (c), Firelight Technologies Pty, Ltd 2004-2015.

This example shows how to create a simple gain DSP effect.
==============================================================================*/

#ifdef WIN32
    #define _CRT_SECURE_NO_WARNINGS
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fmod.hpp"

#ifdef WIN32
    #define _CRT_SECURE_NO_WARNINGS
#endif

extern "C" {
    F_DECLSPEC F_DLLEXPORT FMOD_DSP_DESCRIPTION* F_STDCALL FMODGetDSPDescription();
}

const float FMOD_GAIN_PARAM_GAIN_MIN     = -80.0f;
const float FMOD_GAIN_PARAM_GAIN_MAX     = 10.0f;
const float FMOD_GAIN_PARAM_GAIN_DEFAULT = 0.0f;

#define FMOD_GAIN_RAMPCOUNT 256

enum
{
    FMOD_GAIN_PARAM_GAIN = 0,
    FMOD_GAIN_PARAM_INVERT,
    FMOD_GAIN_NUM_PARAMETERS
};

#define DECIBELS_TO_LINEAR(__dbval__)  ((__dbval__ <= FMOD_GAIN_PARAM_GAIN_MIN) ? 0.0f : powf(10.0f, __dbval__ / 20.0f))
#define LINEAR_TO_DECIBELS(__linval__) ((__linval__ <= 0.0f) ? FMOD_GAIN_PARAM_GAIN_MIN : 20.0f * log10f((float)__linval__))

FMOD_RESULT F_CALLBACK FMOD_Gain_dspcreate       (FMOD_DSP_STATE *dsp_state);
FMOD_RESULT F_CALLBACK FMOD_Gain_dsprelease      (FMOD_DSP_STATE *dsp_state);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspreset        (FMOD_DSP_STATE *dsp_state);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspread         (FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int *outchannels);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparamfloat(FMOD_DSP_STATE *dsp_state, int index, float value);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparamint  (FMOD_DSP_STATE *dsp_state, int index, int value);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparambool (FMOD_DSP_STATE *dsp_state, int index, FMOD_BOOL value);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparamdata (FMOD_DSP_STATE *dsp_state, int index, void *data, unsigned int length);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparamfloat(FMOD_DSP_STATE *dsp_state, int index, float *value, char *valuestr);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparamint  (FMOD_DSP_STATE *dsp_state, int index, int *value, char *valuestr);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparambool (FMOD_DSP_STATE *dsp_state, int index, FMOD_BOOL *value, char *valuestr);
FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparamdata (FMOD_DSP_STATE *dsp_state, int index, void **value, unsigned int *length, char *valuestr);
FMOD_RESULT F_CALLBACK FMOD_Gain_shouldiprocess  (FMOD_DSP_STATE *dsp_state, FMOD_BOOL inputsidle, unsigned int length, FMOD_CHANNELMASK inmask, int inchannels, FMOD_SPEAKERMODE speakermode);

static FMOD_DSP_PARAMETER_DESC p_gain;
static FMOD_DSP_PARAMETER_DESC p_invert;

FMOD_DSP_PARAMETER_DESC *FMOD_Gain_dspparam[FMOD_GAIN_NUM_PARAMETERS] =
{
    &p_gain,
    &p_invert
};

FMOD_DSP_DESCRIPTION FMOD_Gain_Desc =
{
    FMOD_PLUGIN_SDK_VERSION,
    "FMOD Gain",    // name
    0x00010000,     // plug-in version
    1,              // number of input buffers to process
    1,              // number of output buffers to process
    FMOD_Gain_dspcreate,
    FMOD_Gain_dsprelease,
    FMOD_Gain_dspreset,
    FMOD_Gain_dspread,
    0,
    0,
    FMOD_GAIN_NUM_PARAMETERS,
    FMOD_Gain_dspparam,
    FMOD_Gain_dspsetparamfloat,
    0, // FMOD_Gain_dspsetparamint,
    FMOD_Gain_dspsetparambool,
    0, // FMOD_Gain_dspsetparamdata,
    FMOD_Gain_dspgetparamfloat,
    0, // FMOD_Gain_dspgetparamint,
    FMOD_Gain_dspgetparambool,
    0, // FMOD_Gain_dspgetparamdata,
    FMOD_Gain_shouldiprocess,
    0
};

extern "C"
{

F_DECLSPEC F_DLLEXPORT FMOD_DSP_DESCRIPTION* F_STDCALL FMODGetDSPDescription()
{
	static float gain_mapping_values[] = { -80, -50, -30, -10, 10 };
	static float gain_mapping_scale[] = { 0, 2, 4, 7, 11 };

    FMOD_DSP_INIT_PARAMDESC_FLOAT_WITH_MAPPING(p_gain, "Gain", "dB", "Gain in dB. -80 to 10. Default = 0", FMOD_GAIN_PARAM_GAIN_DEFAULT, gain_mapping_values, gain_mapping_scale);
    FMOD_DSP_INIT_PARAMDESC_BOOL(p_invert, "Invert", "", "Invert signal. Default = off", false, 0);
    return &FMOD_Gain_Desc;
}

}

class FMODGainState
{
public:
    FMODGainState();

    void process(float *inbuffer, float *outbuffer, unsigned int length, int channels);
    void reset();
    void setGain(float);
    void setInvert(bool);
    float gain() const { return LINEAR_TO_DECIBELS(m_invert ? -m_target_gain : m_target_gain); }
    FMOD_BOOL invert() const { return m_invert; }

private:
    float m_target_gain;
    float m_current_gain;
    int   m_ramp_samples_left;
    bool  m_invert;
};

FMODGainState::FMODGainState()
{
    m_target_gain = DECIBELS_TO_LINEAR(FMOD_GAIN_PARAM_GAIN_DEFAULT);
    m_invert = 0;
    reset();
}

void FMODGainState::process(float *inbuffer, float *outbuffer, unsigned int length, int channels)
{
    // Note: buffers are interleaved
    float gain = m_current_gain;

    if (m_ramp_samples_left)
    {
        float target = m_target_gain;
        float delta = (target - gain) / m_ramp_samples_left;
        while (length)
        {
            if (--m_ramp_samples_left)
            {
                gain += delta;
                for (int i = 0; i < channels; ++i)
                {
                    *outbuffer++ = *inbuffer++ * gain;
                }
            }
            else
            {
                gain = target;
                break;
            }
            --length;
        }
    }

    unsigned int samples = length * channels;
    while (samples--)
    {
        *outbuffer++ = *inbuffer++ * gain;
    }

    m_current_gain = gain;
}

void FMODGainState::reset()
{
    m_current_gain = m_target_gain;
    m_ramp_samples_left = 0;
}

void FMODGainState::setGain(float gain)
{
    m_target_gain = m_invert ? -DECIBELS_TO_LINEAR(gain) : DECIBELS_TO_LINEAR(gain);
    m_ramp_samples_left = FMOD_GAIN_RAMPCOUNT;
}

void FMODGainState::setInvert(bool invert)
{
    if (invert != m_invert)
    {
        m_target_gain = -m_target_gain;
        m_ramp_samples_left = FMOD_GAIN_RAMPCOUNT;
    }
    m_invert = invert;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspcreate(FMOD_DSP_STATE *dsp_state)
{
    dsp_state->plugindata = (FMODGainState *)FMOD_DSP_STATE_MEMALLOC(dsp_state, sizeof(FMODGainState), FMOD_MEMORY_NORMAL, "FMODGainState");
    if (!dsp_state->plugindata)
    {
        return FMOD_ERR_MEMORY;
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dsprelease(FMOD_DSP_STATE *dsp_state)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;
    FMOD_DSP_STATE_MEMFREE(dsp_state, state, FMOD_MEMORY_NORMAL, "FMODGainState");
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspread(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int * /*outchannels*/)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;
    state->process(inbuffer, outbuffer, length, inchannels); // input and output channels count match for this effect
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspreset(FMOD_DSP_STATE *dsp_state)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;
    state->reset();
    return FMOD_OK;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparamfloat(FMOD_DSP_STATE *dsp_state, int index, float value)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;

    switch (index)
    {
    case FMOD_GAIN_PARAM_GAIN:
        state->setGain(value);
        return FMOD_OK;
    }

    return FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparamfloat(FMOD_DSP_STATE *dsp_state, int index, float *value, char *valuestr)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;

    switch (index)
    {
    case FMOD_GAIN_PARAM_GAIN:
        *value = state->gain();
        if (valuestr) sprintf(valuestr, "%.1f dB", state->gain());
        return FMOD_OK;
    }

    return FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspsetparambool(FMOD_DSP_STATE *dsp_state, int index, FMOD_BOOL value)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;

    switch (index)
    {
      case FMOD_GAIN_PARAM_INVERT:
        state->setInvert(value ? true : false);
        return FMOD_OK;
    }

    return FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_dspgetparambool(FMOD_DSP_STATE *dsp_state, int index, FMOD_BOOL *value, char *valuestr)
{
    FMODGainState *state = (FMODGainState *)dsp_state->plugindata;

    switch (index)
    {
    case FMOD_GAIN_PARAM_INVERT:
        *value = state->invert();
        if (valuestr) sprintf(valuestr, state->invert() ? "Inverted" : "Off" );
        return FMOD_OK;
    }

    return FMOD_ERR_INVALID_PARAM;
}

FMOD_RESULT F_CALLBACK FMOD_Gain_shouldiprocess(FMOD_DSP_STATE * /*dsp_state*/, FMOD_BOOL inputsidle, unsigned int /*length*/, FMOD_CHANNELMASK /*inmask*/, int /*inchannels*/, FMOD_SPEAKERMODE /*speakermode*/)
{
    if (inputsidle)
    {
        return FMOD_ERR_DSP_DONTPROCESS;
    }

    return FMOD_OK;
}
