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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA

    xaw_c.c - XAW Interface from
        Tomokazu Harada <harada@prince.pe.u-tokyo.ac.jp>
        Yoshishige Arai <ryo2@on.rim.or.jp>
        Yair Kalvariski <cesium2@gmail.com>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#if defined(TIME_WITH_SYS_TIME)
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif /* TIME_WITH_SYS_TIME */
#ifndef NO_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif /* NO_STRING_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "timidity.h"
#include "aq.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "readmidi.h"
#include "output.h"
#include "controls.h"
#include "miditrace.h"
#include "timer.h"
#include "xaw.h"

extern void timidity_init_aq_buff(void); /* From timidity/timidity.c */
static int cmsg(int, int, char *, ...);
static int ctl_blocking_read(int32 *);
static void ctl_close(void);
static void ctl_event(CtlEvent *);
static int ctl_read(int32 *);
static int ctl_open(int, int);
static int ctl_pass_playing_list(int, char **);
ControlMode *interface_a_loader(void);

static void a_pipe_open(void);
static int a_pipe_ready(void);
int a_pipe_read(char *, size_t);
int a_pipe_nread(char *, size_t);
void a_pipe_sync(void);
void a_pipe_write(const char *, ...);
static void a_pipe_write_buf(const char *, int);
static void a_pipe_write_msg(char *);
static void a_pipe_write_msg_nobr(char *);
extern void a_start_interface(int);

static void ctl_current_time(int, int);
static void ctl_drumpart(int, int);
static void ctl_event(CtlEvent *);
static void ctl_expression(int, int);
static void ctl_keysig(int);
static void ctl_key_offset(int);
static void ctl_lyric(int);
static void ctl_master_volume(int);
static void ctl_max_voices(int);
static void ctl_mute(int, int);
static void ctl_note(int, int, int, int);
static void ctl_panning(int, int);
static void ctl_pause(int, int);
static void ctl_pitch_bend(int, int);
static void ctl_program(int, int, const char *, uint32);
static void ctl_reset(void);
static void ctl_sustain(int, int);
static void ctl_tempo(int);
static void ctl_timeratio(int);
static void ctl_total_time(int);
static void ctl_refresh(void);
static void ctl_volume(int, int);
static void set_otherinfo(int, int, char);
static void shuffle(int, int *);
static void query_outputs(void);
static void update_indicator(void);
static void xaw_add_midi_file(char *);
static void xaw_delete_midi_file(int);
static void xaw_output_flist(const char *);
#define BANKS(ch) (channel[ch].bank | (channel[ch].bank_lsb << 8) | (channel[ch].bank_msb << 16))

static double indicator_last_update = 0;
#define EXITFLG_QUIT 1
#define EXITFLG_AUTOQUIT 2
static int reverb_type;
/*
 * Unfortunate hack, forced because opt_reverb_control has been overloaded,
 * and is no longer 0 or 1. Now it's in the range 0..3, so when reenabling
 * reverb and disabling it, we need to know the original value.
 */
static int exitflag = 0, randomflag = 0, repeatflag = 0, selectflag = 0,
           current_no = 0, xaw_ready = 0, number_of_files, *file_table;
static int pipe_in_fd, pipe_out_fd;
static char **list_of_files, **titles;
static int active[MAX_XAW_MIDI_CHANNELS];
/*
 * When a midi channel has played a note, this is set to 1. Otherwise, it's 0.
 * This is used to screen out channels which are not used from the GUI.
 */
extern PlayMode *play_mode, *target_play_mode;
static PlayMode *olddpm = NULL;

/**********************************************/
/* export the interface functions */

#define ctl xaw_control_mode

ControlMode ctl = {
    "XAW interface", 'a',
    "xaw",
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

static char local_buf[PIPE_LENGTH];

/***********************************************************************/
/* Put controls on the pipe                                            */
/***********************************************************************/
#define CMSG_MESSAGE 16

static int cmsg(int type, int verbosity_level, char *fmt, ...) {
  va_list ap;
  char *buff;
  MBlockList pool;

  if (((type == CMSG_TEXT) || (type == CMSG_INFO) || (type == CMSG_WARNING)) &&
      (ctl.verbosity<verbosity_level))
    return 0;

  va_start(ap, fmt);

  if (!xaw_ready) {
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, NLS);
    va_end(ap);
    return 0;
  }

  init_mblock(&pool);
  buff = (char *)new_segment(&pool, MIN_MBLOCK_SIZE);
  vsnprintf(buff, MIN_MBLOCK_SIZE, fmt, ap);
  a_pipe_write_msg(buff);
  reuse_mblock(&pool);

  va_end(ap);
  return 0;
}

/*ARGSUSED*/
static void
ctl_current_time(int sec, int v) {
  static int previous_sec = -1, last_v = -1;

  if (sec != previous_sec) {
    previous_sec = sec;
    a_pipe_write("%c%d", M_CUR_TIME, sec);
  }
  if (!ctl.trace_playing || midi_trace.flush_flag || (v == -1)) return;
  if (last_v != v) {
    last_v = v;
    a_pipe_write("%c%c%d", MT_VOICES, MTV_LAST_VOICES_NUM, v);
  }
}

static void
ctl_total_time(int tt_i) {
  static int last_time = -1;

  ctl_current_time(0, 0);
  if (last_time != tt_i) {
    last_time = tt_i;
    a_pipe_write("%c%d", M_TOTAL_TIME, tt_i);
  }
  a_pipe_write("%c%d", M_SET_MODE, play_system_mode);
}

static void
ctl_master_volume(int mv) {
  a_pipe_write("%c%03d", M_VOLUME, mv);
}

static void
ctl_volume(int ch, int val) {
  if ((!ctl.trace_playing) || (ch >= MAX_XAW_MIDI_CHANNELS)) return;
  a_pipe_write("%c%c%d%c%d", MT_PANEL_INFO, MTP_VOLUME, ch,
               CH_END_TOKEN, val);
}

static void
ctl_expression(int ch, int val) {
  if ((!ctl.trace_playing) || (ch >= MAX_XAW_MIDI_CHANNELS)) return;
  a_pipe_write("%c%c%d%c%d", MT_PANEL_INFO, MTP_EXPRESSION, ch,
               CH_END_TOKEN, val);
}

static void
ctl_panning(int ch, int val) {
  if ((!ctl.trace_playing) || (ch >= MAX_XAW_MIDI_CHANNELS)) return;
  a_pipe_write("%c%c%d%c%d", MT_PANEL_INFO, MTP_PANNING, ch,
               CH_END_TOKEN, val);
}

static void
ctl_sustain(int ch, int val) {
  if ((!ctl.trace_playing) || (ch >= MAX_XAW_MIDI_CHANNELS)) return;
  a_pipe_write("%c%c%d%c%d", MT_PANEL_INFO, MTP_SUSTAIN, ch,
               CH_END_TOKEN, val);
}

static void
ctl_pitch_bend(int ch, int val) {
  if ((!ctl.trace_playing) || (ch >= MAX_XAW_MIDI_CHANNELS)) return;
  a_pipe_write("%c%c%d%c%d", MT_PANEL_INFO, MTP_PITCH_BEND, ch,
               CH_END_TOKEN, val);
}

#ifdef WIDGET_IS_LABEL_WIDGET
static void
ctl_lyric(int lyricid) {
  char *lyric;
  static int lyric_col = 0;
  static char lyric_buf[PIPE_LENGTH];

  lyric = event2string(lyricid);
  if (lyric != NULL) {
    if (lyric[0] == ME_KARAOKE_LYRIC) {
      if ((lyric[1] == '/') || (lyric[1] == '\\')) {
        strlcpy(lyric_buf, lyric + 2, sizeof(lyric_buf));
        a_pipe_write_msg(lyric_buf);
        lyric_col = strlen(lyric_buf);
      }
      else if (lyric[1] == '@') {
        if (lyric[2] == 'L')
          snprintf(lyric_buf, sizeof(lyric_buf), "Language: %s", lyric + 3);
        else if (lyric[2] == 'T')
          snprintf(lyric_buf, sizeof(lyric_buf), "Title: %s", lyric + 3);
        else
          strlcpy(lyric_buf, lyric + 1, sizeof(lyric_buf));
        a_pipe_write_msg(lyric_buf);
        lyric_col = 0;
      }
      else {
        strlcpy(lyric_buf + lyric_col, lyric + 1,
                  sizeof(lyric_buf) - lyric_col);
        a_pipe_write_msg(lyric_buf);
        lyric_col += strlen(lyric + 1);
      }
    }
    else {
      lyric_col = 0;
      a_pipe_write_msg_nobr(lyric + 1);
    }
  }
}
#else
static void
ctl_lyric(int lyricid) {
  char *lyric;

  lyric = event2string(lyricid);
  if (lyric != NULL) {
    if (lyric[0] == ME_KARAOKE_LYRIC) {
      if ((lyric[1] == '/') || (lyric[1] == '\\')) { 
        lyric[1] = '\n';
        a_pipe_write_msg_nobr(lyric + 1);
      }
      else if (lyric[1] == '@') {
        if (lyric[2] == 'L')
          snprintf(local_buf, sizeof(local_buf), "Language: %s", lyric + 3);
        else if (lyric[2] == 'T')
          snprintf(local_buf, sizeof(local_buf), "Title: %s", lyric + 3);
        else
          strlcpy(local_buf, lyric + 1, sizeof(local_buf));
        a_pipe_write_msg(local_buf);
      }
      else a_pipe_write_msg_nobr(lyric + 1);
    }
    else a_pipe_write_msg_nobr(lyric + 1);
  }
}
#endif /* WIDGET_IS_LABEL_WIDGET */

/*ARGSUSED*/
static int
ctl_open(int using_stdin, int using_stdout) {
  ctl.opened = 1;

  set_trace_loop_hook(update_indicator);

  /* The child process won't come back from this call  */
  a_pipe_open();

  return 0;
}

static void
ctl_close(void) {
  if (ctl.opened) {
    a_pipe_write("%c", M_QUIT);
    ctl.opened = 0;
    xaw_ready = 0;
  }
}

static void
xaw_add_midi_file(char *additional_path) {
  char *files[1], **ret;
  int i, nfiles, nfit, *local_len = NULL;
  char *p;

  files[0] = additional_path;
  nfiles = 1;
  ret = expand_file_archives(files, &nfiles);
  if (ret == NULL) return;
  titles = (char **)safe_realloc(titles,
             (number_of_files + nfiles) * sizeof(char *));
  list_of_files = (char **)safe_realloc(list_of_files,
                   (number_of_files + nfiles) * sizeof(char *));
  if (nfiles > 0) local_len = (int *)safe_malloc(nfiles * sizeof(int));
  for (i=0, nfit=0;i<nfiles;i++) {
      if (check_midi_file(ret[i]) >= 0) {
          p = strrchr(ret[i], '/');
          if (p == NULL) p = ret[i]; else p++;
          titles[number_of_files+nfit] =
            (char *)safe_malloc(sizeof(char) * (strlen(p) +  9));
          list_of_files[number_of_files + nfit] = safe_strdup(ret[i]);
          local_len[nfit] = sprintf(titles[number_of_files + nfit],
                                  "%d. %s", number_of_files + nfit + 1, p);
          nfit++;
      }
  }
  if (nfit > 0) {
      file_table = (int *)safe_realloc(file_table,
                                     (number_of_files + nfit) * sizeof(int));
      for(i = number_of_files; i < number_of_files + nfit; i++)
          file_table[i] = i;
      number_of_files += nfit;
      a_pipe_write("%c%d", M_FILE_LIST, nfit);
      for (i=0;i<nfit;i++)
          a_pipe_write_buf(titles[number_of_files - nfit + i], local_len[i]);
  }
  free(local_len);
  free(ret[0]);
  free(ret);
}

static void
xaw_delete_midi_file(int delete_num) {
    int i;
    char *p;

    if (delete_num < 0) {
        for(i=0;i<number_of_files;i++){
            free(list_of_files[i]);
            free(titles[i]);
        }
        list_of_files = NULL; titles = NULL;
        file_table = (int *)safe_realloc(file_table, 1*sizeof(int));
        file_table[0] = 0;
        number_of_files = 0;
        current_no = 0;
    } else {
        free(titles[delete_num]); titles[delete_num] = NULL;
        for(i=delete_num; i<number_of_files-1; i++) {
            list_of_files[i] = list_of_files[i+1];
            p = strchr(titles[i+1], '.');
            titles[i] = (char *)safe_realloc(titles[i],
                                  strlen(titles[i+1]) * sizeof(char));
            sprintf(titles[i], "%d%s", i+1, p);
        }
        if (number_of_files > 0) number_of_files--;
        if ((current_no >= delete_num) && (delete_num)) current_no--;
    }
}

static void
xaw_output_flist(const char *filename) {
  int i;
  char *temp = safe_strdup(filename);

  a_pipe_write("%c%d %s", M_SAVE_PLAYLIST, number_of_files, temp);
  free(temp);
  for(i=0;i<number_of_files;i++) {
    a_pipe_write("%s", list_of_files[i]);
  }
}

/*ARGSUSED*/
static int
ctl_blocking_read(int32 *valp) {
  int n;

  a_pipe_read(local_buf, sizeof(local_buf));
  for (;;) {
    switch (local_buf[0]) {
      case S_ADD_TO_PLAYLIST:
        xaw_add_midi_file(local_buf + 1);
        return RC_NONE;
      case S_DEL_CUR_PLAYLIST:
        xaw_delete_midi_file(-1);
        return RC_QUIT;
      case S_PREV: return RC_REALLY_PREVIOUS;
      case S_BACK:
        *valp = (int32)(play_mode->rate * 10);
        return RC_BACK;
      case S_SET_CHORUS:
        n = atoi(local_buf + 1);
        opt_chorus_control = n;
        return RC_QUIT;
      case S_SET_RANDOM:
        randomflag = atoi(local_buf + 1);
        return RC_QUIT;
      case S_DEL_FROM_PLAYLIST:
        n = atoi(local_buf + 1);
        xaw_delete_midi_file(n);
        return RC_NONE;
      case S_SET_OPTIONS:
        n = atoi(local_buf + 1);
        opt_modulation_wheel = n & MODUL_BIT;
        opt_portamento = n & PORTA_BIT;
        opt_nrpn_vibrato = n & NRPNV_BIT;
        opt_reverb_control = !!(n & REVERB_BIT) * reverb_type;
        opt_channel_pressure = n & CHPRESSURE_BIT;
        opt_overlap_voice_allow = n & OVERLAPV_BIT;
        opt_trace_text_meta_event = n & TXTMETA_BIT;
        return RC_QUIT;
      case S_PLAY_FILE:
        selectflag = atoi(local_buf + 1);
        return RC_QUIT;
      case S_FWD:
        *valp = (int32)(play_mode->rate * 10);
        return RC_FORWARD;
      case S_TOGGLE_SPEC: return RC_TOGGLE_SNDSPEC;
      case S_PLAY: return RC_LOAD_FILE;
      case S_TOGGLE_PAUSE: return RC_TOGGLE_PAUSE;
      case S_STOP: return RC_QUIT;
      case S_NEXT: return RC_NEXT;
      case S_SET_REPEAT:
        repeatflag = atoi(local_buf + 1);
        return RC_NONE;
      case S_SET_TIME:
        n = atoi(local_buf + 1);
        *valp = n * play_mode->rate;
        return RC_JUMP;
      case S_SET_MUTE:
        n = atoi(local_buf + 1);
        *valp = (int32)n;
        return RC_TOGGLE_MUTE;
      case S_DEC_VOL:
        *valp = (int32)1;
        return RC_VOICEDECR;
      case S_INC_VOL:
        *valp = (int32)1;
        return RC_VOICEINCR;
      case S_TOGGLE_AUTOQUIT:
        exitflag ^= EXITFLG_AUTOQUIT;
        return RC_NONE;
      case S_SAVE_PLAYLIST:
        xaw_output_flist(local_buf + 1);
        return RC_NONE;
      case S_ENABLE_TRACE:
        ctl.trace_playing = 1;
        if (local_buf[1] == ST_RESET) return RC_SYNC_RESTART;
        return RC_NONE;
      case S_SET_VOL_BEFORE_PLAYING:
        n = atoi(local_buf + 1);
        if (n < 0) n = 0;
        if (n > MAXVOLUME) n = MAXVOLUME;
        amplification = n;
        return RC_NONE;
      case S_SET_VOL:
        n = atoi(local_buf + 1);
        if (n < 0) n = 0;
        if (n > MAXVOLUME) n = MAXVOLUME;
        *valp = (int32)(n-amplification);
        return RC_CHANGE_VOLUME;
      case S_INC_PITCH:
        *valp = (int32)1;
        return RC_KEYUP;
      case S_DEC_PITCH:
        *valp = (int32)-1;
        return RC_KEYDOWN;
      case S_INC_SPEED:
        *valp = (int32)1;
        return RC_SPEEDUP;
      case S_DEC_SPEED:
        *valp = (int32)1;
        return RC_SPEEDDOWN;
      case S_SET_SOLO:
        n = atoi(local_buf + 1);
        *valp = (int32)n;
        return RC_SOLO_PLAY;
      case S_SET_RECORDING:
        if (olddpm == NULL) {
          PlayMode **ii;
          char id = *(local_buf + 1), *p;

          target_play_mode = NULL;
          for (ii = play_mode_list; *ii != NULL; ii++)
            if ((*ii)->id_character == id) 
              target_play_mode = *ii;
          if (target_play_mode == NULL) goto z1error;
          p = strchr(local_buf, ' ');
          target_play_mode->name = safe_strdup(p + 1);
          target_play_mode->rate = atoi(local_buf + 2);
          if (target_play_mode->open_output() == -1) {
            free(target_play_mode->name);
            goto z1error;
          }
          play_mode->close_output();
          olddpm = play_mode;
          /*
           * playmidi.c won't change play_mode when a file is not played,
           * so to be certain, we do this ourselves.
           */
          play_mode = target_play_mode;
          /*
           * Reset aq to match the new output's paramteres.
           */
          aq_setup();
          timidity_init_aq_buff();

          a_pipe_write(CHECKPOST "1");
          return RC_OUTPUT_CHANGED;
        }
z1error:
        a_pipe_write(CHECKPOST "1E");
        return RC_NONE;
      case S_STOP_RECORDING:
        if (olddpm != NULL) {
          play_mode->close_output();
          if (*(local_buf + 1) == SR_USER_STOP) a_pipe_write(CHECKPOST "2S");
          target_play_mode = olddpm;
          if (target_play_mode->open_output() == -1) return RC_NONE;
          free(play_mode->name);
          play_mode = target_play_mode;
          olddpm = NULL;
          aq_setup();
          timidity_init_aq_buff();

          return RC_OUTPUT_CHANGED;
        }
        return RC_NONE;
      case S_SET_PLAYMODE:
       {
          PlayMode **ii;
          char id = *(local_buf + 1);

          if (play_mode->id_character == id) goto z3error;
          if (olddpm != NULL) goto z3error;

          target_play_mode = NULL;
          for (ii = play_mode_list; *ii != NULL; ii++)
            if ((*ii)->id_character == id)
              target_play_mode = *ii;
          if (target_play_mode == NULL) goto z3error;
          play_mode->close_output();
          if (target_play_mode->open_output() == -1) {
            play_mode->open_output();
            goto z3error;
          }
          play_mode = target_play_mode;
          a_pipe_write(CHECKPOST "3");
          return RC_OUTPUT_CHANGED;
       }
z3error:
       a_pipe_write(CHECKPOST "3E");
       return RC_NONE;
      case S_QUIT:
        free(file_table);
        for (n=0; n<number_of_files; n++) {
          free(titles[n]); free(list_of_files[n]);
        }
      default : exitflag |= EXITFLG_QUIT; return RC_QUIT;
    }
  }
}

static int
ctl_read(int32 *valp) {
  if (a_pipe_ready() <= 0) return RC_NONE;
  return ctl_blocking_read(valp);
}

static void
shuffle(int n, int *a) {
  int i, j, tmp;

  for (i=0;i<n;i++) {
    j = int_rand(n);
    tmp = a[i];
    a[i] = a[j];
    a[j] = tmp;
  }
}

static void
query_outputs(void) {
  PlayMode **ii; unsigned char c;

  for (ii = play_mode_list; *ii != NULL; ii++) {
    if ((*ii)->flag & PF_FILE_OUTPUT) c = M_FILE_OUTPUT;
    else c = M_DEVICE_OUTPUT;
    if (*ii == play_mode) {
      if (c == M_DEVICE_OUTPUT) c = M_DEVICE_CUR_OUTPUT;
      else c = M_FILE_CUR_OUTPUT;
    }
    a_pipe_write("%c%c %s", c, (*ii)->id_character, (*ii)->id_name);
  }
  a_pipe_write(CHECKPOST "0");
}

static int
ctl_pass_playing_list(int init_number_of_files, char **init_list_of_files) {
  int command = RC_NONE, i, j;
  int32 val;
  char *p;

  for (i=0; i<MAX_XAW_MIDI_CHANNELS; i++) active[i] = 0;
  reverb_type = opt_reverb_control?opt_reverb_control:DEFAULT_REVERB;
  /* Wait prepare 'interface' */
  a_pipe_read(local_buf, sizeof(local_buf));
  if (strcmp("READY", local_buf)) return 0;
  xaw_ready = 1;

  a_pipe_write("%c", M_CHECKPOST);

  a_pipe_write("%d",
  (opt_modulation_wheel<<MODUL_N)
     | (opt_portamento<<PORTA_N)
     | (opt_nrpn_vibrato<<NRPNV_N)
     | (!!(opt_reverb_control)<<REVERB_N)
     | (opt_channel_pressure<<CHPRESSURE_N)
     | (opt_overlap_voice_allow<<OVERLAPV_N)
     | (opt_trace_text_meta_event<<TXTMETA_N));
  a_pipe_write("%d", opt_chorus_control);

  query_outputs();

  /* Make title string */
  titles = (char **)safe_malloc(init_number_of_files * sizeof(char *));
  list_of_files = (char **)safe_malloc(init_number_of_files * sizeof(char *));
  for (i=0, j=0;i<init_number_of_files;i++) {
    if (check_midi_file(init_list_of_files[i]) >= 0) {
      p = strrchr(init_list_of_files[i], '/');
      if (p == NULL) p = safe_strdup(init_list_of_files[i]);
      else p++;
      list_of_files[j] = safe_strdup(init_list_of_files[i]);
      titles[j] = (char *)safe_malloc(sizeof(char) * (strlen(p) +  9));
      sprintf(titles[j], "%d. %s", j+1, p);
      j++;
    }
  }
  number_of_files = j;
  titles = (char **)safe_realloc(titles, number_of_files * sizeof(char *));
  list_of_files = (char **)safe_realloc(list_of_files,
                                         number_of_files * sizeof(char *));

  /* Send title string */
  a_pipe_write("%d", number_of_files);
  for (i=0;i<number_of_files;i++)
    a_pipe_write("%s", titles[i]);

  /* Make the table of play sequence */
  file_table = (int *)safe_malloc(number_of_files * sizeof(int));
  for (i=0;i<number_of_files;i++) file_table[i] = i;

  /* Draw the title of the first file */
  current_no = 0;
  if (number_of_files != 0) {
    a_pipe_write("%c%s", M_TITLE, titles[file_table[0]]);
    command = ctl_blocking_read(&val);
  }

  /* Main loop */
  for (;;) {
    /* Play file */
    if ((command == RC_LOAD_FILE) && (number_of_files != 0)) {
      char *title;
      a_pipe_write("%c%s", M_LISTITEM, titles[file_table[current_no]]);
      if ((title = get_midi_title(list_of_files[file_table[current_no]]))
            == NULL)
        title = list_of_files[file_table[current_no]];
      a_pipe_write("%c%s", M_TITLE, title);
      command = play_midi_file(list_of_files[file_table[current_no]]);
    } else {
      if (command == RC_CHANGE_VOLUME) { };
      if (command == RC_JUMP) { };
      if (command == RC_TOGGLE_SNDSPEC) { };
      /* Quit timidity*/
      if (exitflag & EXITFLG_QUIT) return 0;
      /* Stop playing */
      if (command == RC_QUIT) {
        a_pipe_write("%c00:00", M_TOTAL_TIME);
        /* Shuffle the table */
        if (randomflag) {
          if (number_of_files == 0) {
            randomflag = 0;
            continue;
          }
          current_no = 0;
          if (randomflag == 1) {
            shuffle(number_of_files, file_table);
            randomflag = 0;
            command = RC_LOAD_FILE;
            continue;
          }
          randomflag = 0;
          for (i=0;i<number_of_files;i++) file_table[i] = i;
          a_pipe_write("%c%s", M_LISTITEM, titles[file_table[current_no]]);
        }
        /* Play the selected file */
        if (selectflag) {
          for (i=0;i<number_of_files;i++)
            if (file_table[i] == selectflag-1) break;
          if (i != number_of_files) current_no = i;
          selectflag = 0;
          command = RC_LOAD_FILE;
          continue;
        }
        /* After the all file played */
      } else if ((command == RC_TUNE_END) || (command == RC_ERROR)) {
        if (current_no+1 < number_of_files) {
          if (olddpm != NULL) {
            command = RC_QUIT;
            a_pipe_write(CHECKPOST "2");
          } else {
            current_no++;
            command = RC_LOAD_FILE;
            continue;
          }
        } else if (exitflag & EXITFLG_AUTOQUIT) {
          return 0;
          /* Repeat */
        } else if (repeatflag) {
          current_no = 0;
          command = RC_LOAD_FILE;
          continue;
          /* Off the play button */
        } else {
          if (olddpm != NULL) a_pipe_write(CHECKPOST "2");
          a_pipe_write("%c", M_PLAY_END);
        }
        /* Play the next */
      } else if (command == RC_NEXT) {
        if (current_no+1 < number_of_files) current_no++;
        command = RC_LOAD_FILE;
        continue;
        /* Play the previous */
      } else if (command == RC_REALLY_PREVIOUS) {
        if (current_no > 0) current_no--;
        command = RC_LOAD_FILE;
        continue;
      }
      command = ctl_blocking_read(&val);
    }
  }
}

/* ------ Pipe handlers ----- */

static void
a_pipe_open(void) {
  int cont_inter[2], inter_cont[2];

  if ((pipe(cont_inter) < 0) || (pipe(inter_cont) < 0)) exit(1);

  if (fork() == 0) {
    close(cont_inter[1]);
    close(inter_cont[0]);
    pipe_in_fd = cont_inter[0];
    pipe_out_fd = inter_cont[1];
    a_start_interface(pipe_in_fd);
  }
  close(cont_inter[0]);
  close(inter_cont[1]);
  pipe_in_fd = inter_cont[0];
  pipe_out_fd = cont_inter[1];
}

void
a_pipe_write(const char *fmt, ...) {
  static char local_buf[PIPE_LENGTH];
  int len;
  va_list ap;
  ssize_t dummy;

  va_start(ap, fmt);
  len = vsnprintf(local_buf, sizeof(local_buf), fmt, ap);
  if ((len < 0) || (len > PIPE_LENGTH))
    dummy = write(pipe_out_fd, local_buf, PIPE_LENGTH);
  else
    dummy = write(pipe_out_fd, local_buf, len);
  dummy += write(pipe_out_fd, "\n", 1);
  va_end(ap);
}

static void
a_pipe_write_buf(const char *buf, int len) {
  ssize_t dummy;
  if ((len < 0) || (len > PIPE_LENGTH))
    dummy = write(pipe_out_fd, buf, PIPE_LENGTH);
  else
    dummy = write(pipe_out_fd, buf, len);
  dummy += write(pipe_out_fd, "\n", 1);
}

static int
a_pipe_ready(void) {
  fd_set fds;
  static struct timeval tv;
  int cnt;

  FD_ZERO(&fds);
  FD_SET(pipe_in_fd, &fds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  if ((cnt = select(pipe_in_fd+1, &fds, NULL, NULL, &tv)) < 0) return -1;
  return (cnt > 0) && (FD_ISSET(pipe_in_fd, &fds) != 0);
}

int
a_pipe_read(char *buf, size_t bufsize) {
  size_t i;

  bufsize--;
  for (i=0;i<bufsize;i++) {
    ssize_t len = read(pipe_in_fd, buf+i, 1);
    if (len != 1) {
      perror("CONNECTION PROBLEM WITH XAW PROCESS");
      exit(1);
    }
    if (buf[i] == '\n') break;
  }
  buf[i] = '\0';
  return 0;
}

int
a_pipe_nread(char *buf, size_t n) {
    ssize_t i, j = 0;

    if (n <= 0) return 0;
    while ((i = read(pipe_in_fd, buf + j, n - j)) > 0) j += i;
    return j;
}

void
a_pipe_sync(void) {
  fsync(pipe_out_fd);
  fsync(pipe_in_fd);
  usleep(100000);
}

static void
a_pipe_write_msg(char *msg) {
    size_t msglen;
    ssize_t dummy;
    char buf[2 + sizeof(size_t)], *p, *q;

    /* convert '\r' to '\n', but strip '\r' from '\r\n' */
    p = q = msg;
    while (*q != '\0') {
      if (*q != '\r') *p++ = *q++;
      else if (*(++q) != '\n') *p++ = '\n';
    }
    *p = '\0';

    msglen = strlen(msg) + 1; /* +1 for '\n' */
    buf[0] = M_LYRIC;
    buf[1] = '\n';

    memcpy(buf + 2, &msglen, sizeof(size_t));
    dummy  = write(pipe_out_fd, buf, sizeof(buf));
    dummy += write(pipe_out_fd, msg, msglen - 1);
    dummy += write(pipe_out_fd, "\n", 1);
}

static void
a_pipe_write_msg_nobr(char *msg) {
    size_t msglen;
    ssize_t dummy;
    char buf[2 + sizeof(size_t)], *p, *q;

    /* convert '\r' to '\n', but strip '\r' from '\r\n' */
    p = q = msg;
    while (*q != '\0') {
      if (*q != '\r') *p++ = *q++;
      else if (*(++q) != '\n') *p++ = '\n';
    }
    *p = '\0';

    msglen = strlen(msg);
    buf[0] = M_LYRIC;
    buf[1] = '\n';

    memcpy(buf + 2, &msglen, sizeof(size_t));
    dummy  = write(pipe_out_fd, buf, sizeof(buf));
    dummy += write(pipe_out_fd, msg, msglen);
}

static void
ctl_note(int status, int ch, int note, int velocity) {
  char c;

  if (ch >= MAX_XAW_MIDI_CHANNELS) return;
  if(!ctl.trace_playing || midi_trace.flush_flag) return;

  switch (status) {
  case VOICE_ON:
    c = '*';
    break;
  case VOICE_SUSTAINED:
    c = '&';
    break;
  case VOICE_FREE:
  case VOICE_DIE:
  case VOICE_OFF:
  default:
    c = '.';
    break;
  }
  a_pipe_write("%c%d%c%c%03d%d", MT_NOTE, ch, CH_END_TOKEN,
               c, (unsigned char)note, velocity);

  if (active[ch] == 0) {
    active[ch] = 1;
    ctl_program(ch, channel[ch].program, channel_instrum_name(ch), BANKS(ch));
  }
}

static void
ctl_program(int ch, int val, const char *comm, uint32 banks) {
  if ((!ctl.trace_playing) || (ch >= MAX_XAW_MIDI_CHANNELS)) return;

  if ((channel[ch].program == DEFAULT_PROGRAM) && (active[ch] == 0) &&
      (!ISDRUMCHANNEL(ch))) return;
  active[ch] = 1;
  if (!IS_CURRENT_MOD_FILE) val += progbase;
  a_pipe_write("%c%c%d%c%d", MT_PANEL_INFO, MTP_PROGRAM, ch,
               CH_END_TOKEN, val);
  a_pipe_write("%c%c%d%c%d", MT_PANEL_INFO, MTP_TONEBANK, ch,
               CH_END_TOKEN, banks);
  if (comm != NULL) {
    a_pipe_write("%c%d%c%s", MT_INST_NAME, ch, CH_END_TOKEN,
                 (!strlen(comm) && (ISDRUMCHANNEL(ch)))? "<drum>":comm);
  }
}

static void
ctl_drumpart(int ch, int is_drum) {
  if ((!ctl.trace_playing) || (ch >= MAX_XAW_MIDI_CHANNELS)) return;

  a_pipe_write("%c%d%c%c", MT_IS_DRUM, ch, CH_END_TOKEN, is_drum+'A');
}

static void
ctl_pause(int is_paused, int time) {
  if (is_paused) {
    a_pipe_write("%c1", M_PAUSE);
  } else { 
    ctl_current_time(time, -1);
    a_pipe_write("%c0", M_PAUSE);
  }
}

static void
ctl_max_voices(int voices) {
  static int last_voices = -1;

  if (last_voices != voices) {
    last_voices = voices;
    a_pipe_write("%c%c%d", MT_VOICES, MTV_TOTAL_VOICES, voices);
  }
}

static void
ctl_event(CtlEvent *e) {
  switch(e->type) {
    case CTLE_CURRENT_TIME:
      ctl_current_time((int)e->v1, (int)e->v2);
      break;
    case CTLE_NOTE:
      ctl_note((int)e->v1, (int)e->v2, (int)e->v3, (int)e->v4);
      break;
    case CTLE_PLAY_START:
      ctl_total_time((int)e->v1 / play_mode->rate);
      break;
    case CTLE_TEMPO:
      ctl_tempo((int)e->v1);
      break;
    case CTLE_TIME_RATIO:
      ctl_timeratio((int)e->v1);
      break;
    case CTLE_PROGRAM:
      ctl_program((int)e->v1, (int)e->v2, (char *)e->v3, (uint32)e->v4);
      break;
    case CTLE_DRUMPART:
      ctl_drumpart((int)e->v1, (int)e->v2);
      break;
    case CTLE_VOLUME:
      ctl_volume((int)e->v1, (int)e->v2);
      break;
    case CTLE_EXPRESSION:
      ctl_expression((int)e->v1, (int)e->v2);
      break;
    case CTLE_PANNING:
      ctl_panning((int)e->v1, (int)e->v2);
      break;
    case CTLE_SUSTAIN:
      ctl_sustain((int)e->v1, (int)e->v2);
      break;
    case CTLE_PITCH_BEND:
      ctl_pitch_bend((int)e->v1, (int)e->v2);
      break;
    case CTLE_MOD_WHEEL:
      ctl_pitch_bend((int)e->v1, e->v2 ? -1 : 0x2000);
      break;
    case CTLE_CHORUS_EFFECT:
      set_otherinfo((int)e->v1, (int)e->v2, MTP_CHORUS);
      break;
    case CTLE_REVERB_EFFECT:
      set_otherinfo((int)e->v1, (int)e->v2, MTP_REVERB);
      break;
    case CTLE_LYRIC:
      ctl_lyric((int)e->v1);
      break;
    case CTLE_MASTER_VOLUME:
      ctl_master_volume((int)e->v1);
      break;
    case CTLE_REFRESH:
      ctl_refresh();
      break;
    case CTLE_RESET:
      ctl_reset();
      break;
    case CTLE_MUTE:
      ctl_mute((int)e->v1, (int)e->v2);
      break;
    case CTLE_KEYSIG:
      ctl_keysig((int)e->v1);
      break;
    case CTLE_KEY_OFFSET:
      ctl_key_offset((int)e->v1);
      break;
    case CTLE_PAUSE:
      ctl_pause((int)e->v1, (int)e->v2);
      break;
    case CTLE_MAXVOICES:
      ctl_max_voices((int)e->v1);
      break;
    case CTLE_LOADING_DONE:
      a_pipe_write("%c%d", M_LOADING_DONE, (int)e->v2);
      break;
#if 0
    case CTLE_PLAY_END:
    case CTLE_METRONOME:
    case CTLE_TEMPER_KEYSIG:
    case CTLE_TEMPER_TYPE:
    case CTLE_SPEANA:
    case CTLE_NOW_LOADING:
    case CTLE_GSLCD:
#endif
    default:
      break;
  }
}

static void
ctl_refresh(void) { }

static void
set_otherinfo(int ch, int val, char c) {
  if ((!ctl.trace_playing) || (ch >= MAX_XAW_MIDI_CHANNELS)) return;
  a_pipe_write("%c%c%d%c%d", MT_PANEL_INFO, c, ch, CH_END_TOKEN, val);
}

static void
ctl_reset(void) {
  int i;

  if (!ctl.trace_playing) return;

  indicator_last_update = get_current_calender_time();
  ctl_tempo((int)current_play_tempo);
  ctl_timeratio((int)(100 / midi_time_ratio + 0.5));
  ctl_keysig((int)current_keysig);
  ctl_key_offset(note_key_offset);
  ctl_max_voices(voices);
  for (i=0; i<MAX_XAW_MIDI_CHANNELS; i++) {
    active[i] = 0;
    if (ISDRUMCHANNEL(i)) {
      if (opt_reverb_control) set_otherinfo(i, get_reverb_level(i), MTP_REVERB);
    } else {
      if (opt_reverb_control) set_otherinfo(i, get_reverb_level(i), MTP_REVERB);
      if (opt_chorus_control) set_otherinfo(i, get_chorus_level(i), MTP_CHORUS);
    }
    ctl_program(i, channel[i].program, channel_instrum_name(i), BANKS(i));
    ctl_volume(i, channel[i].volume);
    ctl_expression(i, channel[i].expression);
    ctl_panning(i, channel[i].panning);
    ctl_sustain(i, channel[i].sustain);
    if ((channel[i].pitchbend == 0x2000) && (channel[i].mod.val > 0))
      ctl_pitch_bend(i, -1);
    else
      ctl_pitch_bend(i, channel[i].pitchbend);
  }
  a_pipe_write("%c", MT_REDRAW_TRACE);
}

static void
update_indicator(void) {
  double t, diff;

  if (!ctl.trace_playing) return;
  t = get_current_calender_time();
  diff = t - indicator_last_update;
  if (diff > XAW_UPDATE_TIME) {
     a_pipe_write("%c", MT_UPDATE_TIMER);
     indicator_last_update = t;
  }
}

static void
ctl_mute(int ch, int mute) {
  if ((!ctl.trace_playing) || (ch >= MAX_XAW_MIDI_CHANNELS)) return;
  a_pipe_write("%c%d%c%d", MT_MUTE, ch, CH_END_TOKEN, mute);
  return;
}

static void
ctl_tempo(int tempo) {
  a_pipe_write("%c%d", MT_TEMPO, tempo);
  return;
}


static void
ctl_timeratio(int ratio) {
  a_pipe_write("%c%d", MT_RATIO, ratio);
  return;
}

static void
ctl_keysig(int keysig) {
  a_pipe_write("%c%d", MT_PITCH, keysig);
  return;
}

static void
ctl_key_offset(int offset) {
  a_pipe_write("%c%d", MT_PITCH_OFFSET, offset);
  return;
}

/*
 * interface_<id>_loader();
 */
ControlMode *
interface_a_loader(void) {
    return &ctl;
}
