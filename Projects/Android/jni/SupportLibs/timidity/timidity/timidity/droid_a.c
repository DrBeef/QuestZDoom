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

#include "helper.h"

static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static int output_data(char *buf, int32 bytes);
static int acntl(int request, void *arg);
int position = 0;
int samples = 0;
//int samples_helper=0;
/* export the playback mode */
#define dpm droid_play_mode

PlayMode dpm = {
	DEFAULT_RATE,
    PE_SIGNED,
    PF_PCM_STREAM|PF_CAN_TRACE|PF_BUFF_FRAGM_OPT,
    -1,
    {0},
    "Android JNI Audio", 'd',
    NULL,
    open_output,
    close_output,
    output_data,
    acntl
};
static int open_output(void)
{
	position = 0;
	samples=0;
	   int include_enc, exclude_enc;
	include_enc = 0;
	if(getMono())
	{	
		dpm.encoding |= PE_MONO;
	}
	if(getSixteen())
	{
		dpm.encoding |= PE_16BIT;
	}
    exclude_enc = PE_ULAW|PE_ALAW|PE_BYTESWAP; /* They can't mean these */
    if(dpm.encoding & PE_16BIT)
	include_enc |= PE_SIGNED;
    else
	exclude_enc |= PE_SIGNED;
    dpm.encoding = validate_encoding(dpm.encoding, include_enc, exclude_enc);
	dpm.rate=getSampleRate();
	return 0;
}
static int output_data(char* buf, int32 nbytes)
{
	//__android_log_print(ANDROID_LOG_DEBUG, "TIMIDITY", "Pushing");
	position+=(int)nbytes;
	samples+=(int)nbytes;
	return nativePush(buf, nbytes);
}
static void close_output(void)
{
	position = 0;
	samples=0;
	//finishAE();
}
static int acntl(int request, void *arg)
{
			//char b1[256];
		//char b2[256];
	//int i;
	if(request!=PM_REQ_GETFILLABLE&&request!=PM_REQ_GETFILLED)
		controller(request);
	switch(request)
	{
	case PM_REQ_PLAY_START:
		samples=0;
		position=0;
		break;
	case PM_REQ_GETQSIZ:
		*((int *)arg) = dpm.rate*4; 
		return 0;
		break;
	case PM_REQ_GETFRAGSIZ:
		*((int *)arg) = dpm.rate*2;
		break;
	case PM_REQ_GETFILLABLE:
		*((int *)arg) = dpm.rate*4;
		break;
	case PM_REQ_GETSAMPLES:
		//*((int *)arg) = getNumber();
		*((int *)arg) = (samples/(((dpm.encoding&PE_16BIT)?2:1)*((dpm.encoding&PE_MONO)?1:2)));
		break;
	case PM_REQ_DISCARD:
		samples=0;
		//samples_helper=getBuffer();
		//flushIt();
		//discard();
		break;
	case PM_REQ_FLUSH:
		//samples_helper=getBuffer();
		//flushIt();
		//samples=0;
		samples=0;
		//discard();
		break;
	case PM_REQ_GETFILLED:

		//sprintf(b1, "%d", position);
		//sprintf(b2, "%i", getBuffer());
		//__android_log_print(ANDROID_LOG_DEBUG, "TIMIDITY", b1);
		//__android_log_print(ANDROID_LOG_DEBUG, "TIMIDITY", b2);
		/*i = samples_helper;
		if(!(dpm.encoding & PE_MONO)) i >>= 1;
		if(dpm.encoding & PE_16BIT) i >>= 1;
		*((int *)arg) = position-g;*/
		*((int *)arg) = position-(getBuffer()+5);
		break;
	case PM_REQ_PLAY_END:
		position=0;
		samples=0;
		finishAE();
		break;
	}
	return 0; // We do nothing
}
