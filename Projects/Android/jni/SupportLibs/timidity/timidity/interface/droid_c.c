/*
   TiMidity++ -- MIDI to WAVE converter and player
   Copyright (C) 1999-2004 Masanao Izumo <iz@onicos.co.jp>
   Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

   dumb_c.c
   Minimal control mode -- no interaction, just prints out messages.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
//#include <android/log.h>
#include "timidity.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"

#include "helper.h"

#define CTL_STATUS_UPDATE -98
#define CTL_STATUS_INIT -99

static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_read(int32 *valp);
static int cmsg(int type, int verbosity_level, char *fmt, ...);
static void ctl_total_time(long tt);
static void ctl_file_name(char *name);
static void ctl_current_time(int ct, int v);
static void ctl_lyric(int lyricid);
static void ctl_max_voices(int mv);
static void ctl_key_offset(int offset);
static void ctl_event(CtlEvent *e);
static void ctl_program(int ch, int prog, char *comm, unsigned int banks);
static int ctl_pass_playing_list(int number_of_files, char *list_of_files[]);
static char lyric_buf[300]={0};
static int lyric_col = 2;
char *curr_list_of_files[1];
int curr_number_of_files;
int droid_rc=0;
int droid_arg=0;
/**********************************/
/* export the interface functions */

#define ctl droid_control_mode

ControlMode ctl=
{
	"droid interface", 'r',
	"droid",
	1,0,0,
	0,
	ctl_open,
	ctl_close,
	ctl_pass_playing_list,
	ctl_read,
	NULL,
	cmsg,
	ctl_event
};

/*
 * interface_<id>_loader();
 */
/*ARGSUSED*/
static int ctl_open(int using_stdin, int using_stdout)
{
	ctl.opened=1;
	return 0;
}
static void ctl_close(void)
{
	ctl.opened=0;
}
static void ctl_total_time(long tt)
{
	int mins, secs;
	if (ctl.trace_playing)
	{
		secs=(int)(tt/play_mode->rate);
		mins=secs/60;
		secs-=mins*60;
		cmsg(CMSG_INFO, VERB_NORMAL,
				"Total playing time: %3d min %02d s", mins, secs);
	}
	setMaxTime((int)(tt/play_mode->rate));
}

static void ctl_file_name(char *name)
{
	if (ctl.verbosity>=0 || ctl.trace_playing)
		cmsg(CMSG_INFO, VERB_NORMAL, "Playing %s", name);
}

static void ctl_current_time(int secs, int v)
{
	setCurrTime(secs, v);
}
static void ctl_max_voices(int voices) {
	sendMaxVoice(voices);
}
static void ctl_lyric(int lyricid)
{
	char *lyric;

	lyric = event2string(lyricid);
	if(lyric != NULL)
	{
		if(lyric[0] == ME_KARAOKE_LYRIC)
		{
			if(lyric[1] == '/' || lyric[1] == '\\')
			{
				lyric_buf[0] = 'N';
				lyric_buf[1] = ' ';
				snprintf(lyric_buf + 2, sizeof (lyric_buf) - 2, "%s", lyric + 2);
				setCurrLyric(lyric_buf);
				lyric_col = strlen(lyric + 2) + 2;
			}
			else if(lyric[1] == '@')
			{
				lyric_buf[0] = 'Q';
				lyric_buf[1] = ' ';
				if(lyric[2] == 'L')
					snprintf(lyric_buf + 2, sizeof (lyric_buf) - 2, "Language: %s", lyric + 3);
				else if(lyric[2] == 'T')
					snprintf(lyric_buf + 2, sizeof (lyric_buf) - 2, "Title: %s", lyric + 3);
				else
					snprintf(lyric_buf + 2, sizeof (lyric_buf) - 2, "%s", lyric + 1);
				setCurrLyric(lyric_buf);
			}
			else
			{
				lyric_buf[0] = 'L';
				lyric_buf[1] = ' ';
				snprintf(lyric_buf + lyric_col, sizeof (lyric_buf) - lyric_col, "%s", lyric + 1);
				setCurrLyric(lyric_buf);
				lyric_col += strlen(lyric + 1);
			}
		}
		else
		{
			if(lyric[0] == ME_CHORUS_TEXT || lyric[0] == ME_INSERT_TEXT)
				lyric_col = 0;
			snprintf(lyric_buf + lyric_col, sizeof (lyric_buf) - lyric_col, "%s", lyric + 1);
			setCurrLyric(lyric_buf);
		}
	}
}
static void ctl_program(int ch, int prog, char *comm, unsigned int banks)
{
	setProgram(ch, prog);
}
static void ctl_drumpart(int ch, int isdrum)
{
	setDrum(ch, isdrum);
}
static void ctl_volume(int ch, int vol)
{
	setVol(ch, vol);
}
static void ctl_tempo(int t, int tr)
{
	static int lasttempo = CTL_STATUS_UPDATE;
	static int lastratio = CTL_STATUS_UPDATE;

	if (t == CTL_STATUS_UPDATE)
		t = lasttempo;
	else
		lasttempo = t;
	if (tr == CTL_STATUS_UPDATE)
		tr = lastratio;
	else
		lastratio = tr;

	sendTempo(t, tr);
}
static void ctl_key_offset(int offset) {
	sendKey(offset);
	return;
}
static void ctl_event(CtlEvent *e)
{
	switch(e->type)
	{
		case CTLE_NOW_LOADING:
			ctl_file_name((char *)e->v1);
			break;
		case CTLE_PLAY_START:
			ctl_total_time(e->v1);
			break;
		case CTLE_DRUMPART:
			ctl_drumpart((int)e->v1, (int)e->v2);
			break;
		case CTLE_CURRENT_TIME:
			ctl_current_time((int)e->v1, (int)e->v2);
			break;
		case CTLE_MAXVOICES:
			ctl_max_voices((int)e->v1);
			break;
		case CTLE_TEMPO:
			ctl_tempo((int) e->v1, CTL_STATUS_UPDATE);
			break;
		case CTLE_TIME_RATIO:
			ctl_tempo(CTL_STATUS_UPDATE, (int) e->v1);
			break;
		case CTLE_VOLUME:
			ctl_volume((int)e->v1, (int)e->v2);
			break;
		case CTLE_PROGRAM:
			ctl_program((int)e->v1, (int)e->v2, (char *)e->v3, (unsigned int)e->v4);
			break;
		case CTLE_KEY_OFFSET:
			ctl_key_offset((int)e->v1);
			break;
#ifndef CFG_FOR_SF
		case CTLE_LYRIC:
			ctl_lyric((int)e->v1);
			break;
#endif
	}
}
/*ARGSUSED*/
static int ctl_read(int32 *valp)
{
	int ret;
	if(droid_rc){ 
		ret=droid_rc; 
		*valp=(int32)droid_arg;
		droid_rc=RC_NONE; droid_arg=0; 
		return ret;
	}
	return RC_NONE;
}

int play_list(int number_of_files, char *list_of_files[])
{
	lyric_col=2;
	memset(&lyric_buf, 0, 300);
	return ctl_pass_playing_list(number_of_files, list_of_files);
}
static int ctl_pass_playing_list(int number_of_files, char *list_of_files[]) {
	if(number_of_files)
		return play_midi_file(list_of_files[0]);
	return 0;

}
static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
	if(getVerbosity() < verbosity_level)
	{
		return 0;
	}
	va_list ap;
	va_start(ap, fmt);
	char *buffer;
	vasprintf(&buffer, fmt, ap);
	andro_timidity_cmsg_print(buffer);
	free(buffer);
	va_end(ap);
	return 0;
}
ControlMode *interface_r_loader(void)
{
	return &ctl;
}
