/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
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

    xaw_p.h - defines for communiction between server and client for Xaw interface.
       written by Yair Kalvariski (cesium2@gmail.com)
*/

#ifndef _XAW_P_H_
#define _XAW_P_H_

typedef enum {
  M_PAUSE = 'A',
  M_TITLE = 'e',
  M_LISTITEM = 'E',
  M_LYRIC = 'L',
  M_LOADING_DONE = 'l',
  M_SET_MODE = 'm',
  M_PLAY_END = 'O',
  M_QUIT = 'Q',
  M_SAVE_PLAYLIST = 's',
  M_CUR_TIME = 't',
  M_TOTAL_TIME = 'T',
  M_VOLUME = 'V',
  M_FILE_LIST = 'X',
  M_CHECKPOST = 'Z', /* Should match CHECKPOST below */

/* Used only when querying outputs at start */
  M_FILE_OUTPUT = 'F',
  M_FILE_CUR_OUTPUT = 'f',
  M_DEVICE_OUTPUT = 'O',
  M_DEVICE_CUR_OUTPUT = 'o'
} toclient_t;

#define CHECKPOST "Z"

/* Used by x_trace only, should be different from those above in toclient_t */
typedef enum {
  MT_REDRAW_TRACE = 'R',
  MT_VOICES = 'v',
    MTV_LAST_VOICES_NUM = 'l',
    MTV_TOTAL_VOICES = 'L',
  MT_MUTE = 'M',
  MT_NOTE = 'Y',
  MT_INST_NAME = 'I',
  MT_IS_DRUM = 'i',
  MT_UPDATE_TIMER = 'U',
  MT_PITCH_OFFSET = 'o',
  MT_PITCH = 'p',
  MT_PANEL_INFO = 'P',
    MTP_PANNING = 'A',
    MTP_PITCH_BEND = 'B',
    MTP_TONEBANK = 'b',
    MTP_REVERB = 'r',
    MTP_CHORUS = 'c',
    MTP_SUSTAIN = 'S',
    MTP_PROGRAM = 'P',
    MTP_EXPRESSION = 'E',
    MTP_VOLUME = 'V',
  MT_RATIO = 'q',
  MT_TEMPO = 'r'
} totracer_t;

/* Sent back to the interface control */
typedef enum {
  S_DEL_CUR_PLAYLIST = 'A',
  S_PREV = 'B',
  S_BACK = 'b',
  S_SET_CHORUS = 'C',
  S_SET_RANDOM = 'D',
  S_DEL_FROM_PLAYLIST = 'd',
  S_SET_OPTIONS = 'E',
  S_FWD = 'f',
  S_TOGGLE_SPEC = 'g',
  S_PLAY_FILE = 'L',
  S_SET_MUTE = 'M',
  S_NEXT = 'N',
  S_INC_VOL = 'o',
  S_DEC_VOL = 'O',
  S_PLAY = 'P',
  S_SET_PLAYMODE = 'p',
  S_TOGGLE_AUTOQUIT = 'q',
  S_SET_REPEAT = 'R',
  S_STOP = 'S',
  S_SAVE_PLAYLIST = 's',
  S_ENABLE_TRACE = 't',
    ST_RESET = 'R',
  S_SET_TIME = 'T',
  S_SET_VOL_BEFORE_PLAYING = 'v',
  S_SET_VOL = 'V',
  S_QUIT = 'Q',
  S_TOGGLE_PAUSE = 'U',
  S_SET_RECORDING = 'W',
  S_STOP_RECORDING = 'w',
    SR_USER_STOP = 'S',
  S_ADD_TO_PLAYLIST = 'X',
  S_INC_PITCH = '+',
  S_DEC_PITCH = '-',
  S_INC_SPEED = '>',
  S_DEC_SPEED = '<',
  S_SET_SOLO = '.'
} toserver_t;

#define CH_END_TOKEN '|'
#endif /* _XAW_P_H_ */
