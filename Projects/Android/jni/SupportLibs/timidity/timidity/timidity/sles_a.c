#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "timer.h"
#include "instrum.h"
#include "playmidi.h"
#include "miditrace.h"
#include <android/log.h>
#include "opensl_io.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <pthread.h>
#include <stdlib.h>
#define BUFFERFRAMES 1024
#define VECSAMPS_MONO 64
#define VECSAMPS_STEREO 128
#define SR 48000
static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 bytes);
static int acntl(int request, void *arg);

OPENSL_STREAM  *p;
int samps, i, j;
float outbuffer[VECSAMPS_STEREO];
/* export the playback mode */
#define dpm sles_play_mode

PlayMode dpm = {
    48000,
    PE_16BIT|PE_SIGNED,
    PF_PCM_STREAM/*|PF_CAN_TRACE/*|PF_BUFF_FRAGM_OPT*/,
    -1,
    {0},
    "Android SLES Audio", 's',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};
static int open_output(void)
{
	__android_log_print(ANDROID_LOG_DEBUG, "TIMIDITY", "It begins");
	   int include_enc, exclude_enc;
	include_enc = 0;
    exclude_enc = PE_ULAW|PE_ALAW|PE_BYTESWAP; /* They can't mean these */
    /*if(dpm.encoding & PE_16BIT)
	include_enc |= PE_SIGNED;
    else
	exclude_enc |= PE_SIGNED;*/
   // dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);
	//dpm.rate=48000;
	
  
  p = android_OpenAudioDevice(48000,0,2,10000);
  //if(p == NULL) return; 
  //on = 1;
	return 0;
}
static int output_data(char* buf, int32 nbytes)
{
	//__android_log_print(ANDROID_LOG_DEBUG, "TIMIDITY", "Pushing");
	android_AudioOut(p, buf, (int)nbytes);
	
}
static void close_output(void)
{
	__android_log_print(ANDROID_LOG_DEBUG, "TIMIDITY", "Death");
	android_CloseAudioDevice(p);
	finishHim();
}
static int acntl(int request, void *arg)
{
	//__android_log_print(ANDROID_LOG_DEBUG, "TIMIDITY", "Why");
	if(request!=12)
		controller(request);
	switch(request)
	{

	case PM_REQ_GETQSIZ:
		return -1;
	case PM_REQ_GETFRAGSIZ:
		*((int *)arg) = 1000;
		break;
	case PM_REQ_GETFILLED:
		*((int *)arg) = 1000-android_getOutputBuffer(p);
		break;
	}
	return 0; // We do nothing
}
