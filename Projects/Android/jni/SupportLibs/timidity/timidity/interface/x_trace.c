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

    x_trace.c - Trace window drawing for X11 based systems
        based on code by Yoshishige Arai <ryo2@on.rim.or.jp>
        modified by Yair Kalvariski <cesium2@gmail.com>
*/

#include "x_trace.h"
#include <stdlib.h>
#include "timer.h"

#ifdef HAVE_LIBXFT
#include <X11/Xft/Xft.h>
#endif

enum {
  CL_C,		/* column 0 = channel */
  CL_VE,	/* column 1 = velocity */
  CL_VO,	/* column 2 = volume */
  CL_EX,	/* column 3 = expression */
  CL_PR,	/* column 4 = program */
  CL_PA,	/* column 5 = panning */
  CL_PI,	/* column 6 = pitch bend */
  CL_IN,	/* column 7 = instrument name */
  KEYBOARD,
  TCOLUMN,
  CL_BA = 4,	/* column 5 = bank */
  CL_BA_MSB,	/* column 6 = bank_lsb */
  CL_BA_LSB,	/* column 7 = bank_msg */
  CL_RE,	/* column 8 = reverb */
  CL_CH,	/* column 9 = chorus */
  KEYBOARD2,
  T2COLUMN
};

#define MAX_GRADIENT_COLUMN CL_CH+1

typedef struct {
  int y;
  int l;
} KeyL;

typedef struct {
  KeyL k[3];
  int xofs;
  Pixel col;
} ThreeL;

typedef struct {
  const int		col;	/* column number */
  const char		**cap;	/* caption strings array */
  const int		*w;  	/* column width array */
  const int		*ofs;	/* column offset array */
} Tplane;

typedef struct {
  GC gradient_gc[MAX_GRADIENT_COLUMN];
  Pixmap gradient_pixmap[MAX_GRADIENT_COLUMN];
  Boolean gradient_set[MAX_GRADIENT_COLUMN];
  XColor x_boxcolor;
  RGBInfo rgb;
} GradData;

typedef struct {
  int is_drum[MAX_TRACE_CHANNELS];
  int8 c_flags[MAX_TRACE_CHANNELS];
  int8 v_flags[MAX_TRACE_CHANNELS];
  int16 cnote[MAX_TRACE_CHANNELS];
  int16 ctotal[MAX_TRACE_CHANNELS];
  int16 cvel[MAX_TRACE_CHANNELS];
  int16 reverb[MAX_TRACE_CHANNELS];  
  Channel channel[MAX_TRACE_CHANNELS];
  char *inst_name[MAX_TRACE_CHANNELS];

  unsigned int last_voice, tempo, timeratio, xaw_i_voices;
  int pitch, poffset;
  const char *key_cache;

  Display *disp;
  Window trace;
  unsigned int depth, plane, multi_part, visible_channels, voices_width;
  Pixel barcol[MAX_TRACE_CHANNELS];
  Pixmap layer[2];
  GC gcs, gct, gc_xcopy;
  Boolean g_cursor_is_in;
  tconfig *cfg;
  const char *title;
  GradData *grad;

#ifdef HAVE_LIBXFT
  XftDraw *xft_trace, *xft_trace_foot, *xft_trace_inst;
  XftFont *trfont, *ttfont;
  XftColor xft_capcolor, xft_textcolor;
  Pixmap xft_trace_foot_pixmap, xft_trace_inst_pixmap;
#else
  int foot_width;
  short title_font_ascent;
#endif /* HAVE_LIBXFT */
} PanelInfo;

#define gcs Panel->gcs
#define gct Panel->gct
#define gc_xcopy Panel->gc_xcopy
#define plane Panel->plane
#define layer Panel->layer
#define gradient_gc Panel->grad->gradient_gc
#define gradient_pixmap Panel->grad->gradient_pixmap
#define gradient_set Panel->grad->gradient_set
#define grgb Panel->grad->rgb
#define x_boxcolor Panel->grad->x_boxcolor

#ifdef HAVE_LIBXFT
#define trace_font	Panel->trfont
#define ttitle_font	Panel->ttfont

#define COPY_PIXEL(dest, src) do { \
  XColor _x_; \
  _x_.pixel = src; \
  XQueryColor(disp, DefaultColormap(disp, 0), &_x_); \
  dest.color.red = _x_.red; dest.color.green = _x_.green; \
  dest.color.blue = _x_.blue; dest.color.alpha = 0xffff; dest.pixel = src; \
} while(0)

#ifdef X_HAVE_UTF8_STRING
#define TraceDrawStr(x,y,buf,len,color) do { \
  XftColor xftcolor; \
  COPY_PIXEL(xftcolor, color); \
  XftDrawStringUtf8(Panel->xft_trace, &xftcolor, trace_font, \
                    x, y, (FcChar8 *)buf, len); \
} while(0)

#define TitleDrawStr(x,y,buf,len,color) do { \
  XftColor xftcolor; \
  COPY_PIXEL(xftcolor, color); \
  XftDrawStringUtf8(Panel->xft_trace, &xftcolor, ttitle_font, \
                    x, y, (FcChar8 *)buf, len); \
} while(0)
#else
#define TraceDrawStr(x,y,buf,len,color) do { \
  XftColor xftcolor; \
  COPY_PIXEL(xftcolor, color); \
  XftDrawString8(Panel->xft_trace, &xftcolor, trace_font, \
                 x, y, (FcChar8 *)buf, len);\
} while(0)

#define TitleDrawStr(x,y,buf,len,color) do { \
  XftColor xftcolor; \
  COPY_PIXEL(xftcolor, color); \
  XftDrawString8(Panel->xft_trace, &xftcolor, ttitle_font, \
                 x, y, (FcChar8 *)buf, len); \
} while(0)
#endif /* X_HAVE_UTF8_STRING */

#else
#define trace_font	Panel->cfg->c_trace_font
#define ttitle_font	Panel->cfg->c_title_font

#define TraceDrawStr(x,y,buf,len,color) do { \
  XSetForeground(disp, gct, color); \
  XmbDrawString(disp, Panel->trace, trace_font, gct, x, y, buf, len); \
} while(0)

#define TitleDrawStr(x,y,buf,len,color) do { \
  XSetForeground(disp, gct, color); \
  XmbDrawString(disp, Panel->trace, ttitle_font, gct, x, y, buf, len); \
} while(0)
#endif /* HAVE_LIBXFT */

static PanelInfo *Panel;
static ThreeL *keyG = NULL;

static const char *caption[TCOLUMN] =
{"ch", "  vel", " vol", "expr", "prog", "pan", "pit", " instrument",
 "          keyboard"};
static const char *caption2[T2COLUMN] =
{"ch", "  vel", " vol", "expr", "bnk", "msb", "lsb", "reverb", "chorus",
 "          keyboard"};

static const int BARH_SPACE[TCOLUMN] = {22, 60, 40, 36, 36, 36, 30, 106, 304};
#define BARH_OFS0	(TRACE_HOFS)
#define BARH_OFS1	(BARH_OFS0+22)
#define BARH_OFS2	(BARH_OFS1+60)
#define BARH_OFS3	(BARH_OFS2+40)
#define BARH_OFS4	(BARH_OFS3+36)
#define BARH_OFS5	(BARH_OFS4+36)
#define BARH_OFS6	(BARH_OFS5+36)
#define BARH_OFS7	(BARH_OFS6+30)
#define BARH_OFS8	(BARH_OFS7+106)
static const int bar0ofs[TCOLUMN+1] = {BARH_OFS0, BARH_OFS1, BARH_OFS2, BARH_OFS3,
  BARH_OFS4, BARH_OFS5, BARH_OFS6, BARH_OFS7, BARH_OFS8};

static const int BARH2_SPACE[T2COLUMN] = {22, 60, 40, 36, 36, 36, 36, 50,
                                          50, 304};
#define BARH2_OFS0	(TRACE_HOFS)
#define BARH2_OFS1	(BARH2_OFS0+22)
#define BARH2_OFS2	(BARH2_OFS1+60)
#define BARH2_OFS3	(BARH2_OFS2+40)
#define BARH2_OFS4	(BARH2_OFS3+36)
#define BARH2_OFS5	(BARH2_OFS4+36)
#define BARH2_OFS6	(BARH2_OFS5+36)
#define BARH2_OFS7	(BARH2_OFS6+36)
#define BARH2_OFS8	(BARH2_OFS7+50)
#define BARH2_OFS9	(BARH2_OFS8+50)
static const int bar1ofs[T2COLUMN+1] = {BARH2_OFS0, BARH2_OFS1, BARH2_OFS2, BARH2_OFS3,
  BARH2_OFS4, BARH2_OFS5, BARH2_OFS6, BARH2_OFS7, BARH2_OFS8, BARH2_OFS9};

static const Tplane pl[] = {
  {TCOLUMN, caption, BARH_SPACE, bar0ofs},
  {T2COLUMN, caption2, BARH2_SPACE, bar1ofs},
};

#define KEY_NUM 111
#define BARSCALE2 0.31111	/* velocity scale   (60-4)/180 */
#define BARSCALE3 0.28125	/* volume scale     (40-4)/128 */
#define BARSCALE4 0.25		/* expression scale (36-4)/128 */
#define BARSCALE5 0.359375	/* reverb scale     (50-4)/128 */

#define FLAG_NOTE_OFF	1
#define FLAG_NOTE_ON	2
#define FLAG_BANK	1
#define FLAG_PROG	2
#define FLAG_PROG_ON	4
#define FLAG_PAN	8
#define FLAG_SUST	16
#define FLAG_BENDT	32

#define VISIBLE_CHANNELS Panel->visible_channels
#define VISLOW Panel->multi_part
#define XAWLIMIT(ch) ((VISLOW <= (ch)) && ((ch) < (VISLOW+VISIBLE_CHANNELS)))

#define disp		Panel->disp

#define boxcolor	Panel->cfg->box_color
#define capcolor	Panel->cfg->caption_color
#define chocolor	Panel->cfg->cho_color
#define expcolor	Panel->cfg->expr_color
#define pancolor	Panel->cfg->pan_color
#define playcolor	Panel->cfg->play_color
#define revcolor	Panel->cfg->rev_color
#define rimcolor	Panel->cfg->rim_color
#define suscolor	Panel->cfg->sus_color
#define textcolor	Panel->cfg->common_fgcolor
#define textbgcolor	Panel->cfg->text_bgcolor
#define tracecolor	Panel->cfg->trace_bgcolor
#define volcolor	Panel->cfg->volume_color

#define gradient_bar	Panel->cfg->gradient_bar
#define black		Panel->cfg->black_key_color
#define white		Panel->cfg->white_key_color

#define trace_height_raw	Panel->cfg->trace_height
#define trace_height_nf		(Panel->cfg->trace_height - TRACE_FOOT)
#define trace_width		Panel->cfg->trace_width

#define UNTITLED_STR	Panel->cfg->untitled
/* Privates */

static int bitcount(int);
static void ctl_channel_note(int, int, int);
static void drawBar(int, int, int, int, Pixel);
static void drawKeyboardAll(Drawable, GC);
static void draw1Note(int, int, int);
static void drawProg(int, int, Boolean);
static void drawPan(int, int, Boolean);
static void draw1Chan(int, int, char);
static void drawVol(int, int);
static void drawExp(int, int);
static void drawPitch(int, int);
static void drawInstname(int, char *);
static void drawDrumPart(int, int);
static void drawBank(int, int, int, int);
static void drawReverb(int, int);
static void drawChorus(int, int);
static void drawFoot(Boolean);
static void drawVoices(void);
static void drawMute(int, int);
static int getdisplayinfo(RGBInfo *);
static int sftcount(int *);

static int bitcount(int d) {
  int rt = 0;

  while ((d & 0x01) == 0x01) {
    d >>= 1;
    rt++;
  }
  return rt;
}

static int sftcount(int *mask) {
  int rt = 0;

  while ((*mask & 0x01) == 0) {
    (*mask) >>= 1;
    rt++;
  }
  return rt;
}

static int getdisplayinfo(RGBInfo *rgb) {
  XWindowAttributes xvi;
  XGetWindowAttributes(disp, Panel->trace, &xvi);

  if ((rgb != NULL) && (xvi.depth >= 16)) {
    rgb->Red_depth = (xvi.visual)->red_mask;
    rgb->Green_depth = (xvi.visual)->green_mask;
    rgb->Blue_depth = (xvi.visual)->blue_mask;
    rgb->Red_sft = sftcount(&(rgb->Red_depth));
    rgb->Green_sft = sftcount(&(rgb->Green_depth));
    rgb->Blue_sft = sftcount(&(rgb->Blue_depth));
    rgb->Red_depth = bitcount(rgb->Red_depth);
    rgb->Green_depth = bitcount(rgb->Green_depth);
    rgb->Blue_depth = bitcount(rgb->Blue_depth);
  }
  return xvi.depth;
}

static void drawBar(int ch, int len, int xofs, int column, Pixel color) {
  static Pixel column1color0 = 0;

  XGCValues gv;
  int col, i, screen;
  XColor x_color;

  ch -= VISLOW;
  screen = DefaultScreen(disp);
  if ((16 <= Panel->depth) && (gradient_bar)) {
    gv.fill_style = FillTiled;
    gv.fill_rule = WindingRule;

    if (column < MAX_GRADIENT_COLUMN) {
      col = column;
      if (column == 1) {
        if (gradient_set[0] == 0) {
          column1color0 = color;
          col = 0;
        }
        else if ((gradient_set[1] == 0) && (column1color0 != color)) {
          col = 1;
        }
        else {
          if (column1color0 == color) col = 0;
          else col = 1;
        }
      }
      if (gradient_set[col] == 0) {
        unsigned long pxl;
        int r, g, b;

        gradient_pixmap[col] = XCreatePixmap(disp, Panel->trace,
                      BARH2_SPACE[column], 1, DefaultDepth(disp, screen));
        x_color.pixel = color;
        XQueryColor(disp, DefaultColormap(disp, 0), &x_color);
        for (i=0;i<BARH2_SPACE[column];i++) {
          r = x_boxcolor.red +
            (x_color.red - x_boxcolor.red) * i / BARH2_SPACE[column];
          g = x_boxcolor.green +
            (x_color.green - x_boxcolor.green) * i / BARH2_SPACE[column];
          b = x_boxcolor.blue + 
            (x_color.blue - x_boxcolor.blue) * i / BARH2_SPACE[column];
          if (r<0) r = 0;
          if (g<0) g = 0;
          if (b<0) b = 0;
          r >>= 8;
          g >>= 8;
          b >>= 8;
          if (r>255) r = 255;
          if (g>255) g = 255;
          if (b>255) b = 255;
          pxl  = (r >> (8-grgb.Red_depth)) << grgb.Red_sft;
          pxl |= (g >> (8-grgb.Green_depth)) << grgb.Green_sft;
          pxl |= (b >> (8-grgb.Blue_depth)) << grgb.Blue_sft;
          XSetForeground(disp, gct, pxl);
          XDrawPoint(disp, gradient_pixmap[col], gct, i, 0);
        }
        gv.tile = gradient_pixmap[col];
        gradient_gc[col] = XCreateGC(disp, Panel->trace,
                                     GCFillStyle | GCFillRule | GCTile, &gv);
        gradient_set[col] = 1;
      }
      XSetForeground(disp, gct, boxcolor);
      XFillRectangle(disp, Panel->trace, gct,
                     xofs+len+2, CHANNEL_HEIGHT(ch)+2,
                     pl[plane].w[column] - len -4, BAR_HEIGHT);
      gv.ts_x_origin = xofs + 2 - BARH2_SPACE[column] + len;
      XChangeGC(disp, gradient_gc[col], GCTileStipXOrigin, &gv);
      XFillRectangle(disp, Panel->trace, gradient_gc[col],
                     xofs+2, CHANNEL_HEIGHT(ch)+2,
                     len, BAR_HEIGHT);
    }
  }
  else {
    XSetForeground(disp, gct, boxcolor);
    XFillRectangle(disp, Panel->trace, gct,
                   xofs+len+2, CHANNEL_HEIGHT(ch)+2,
                   pl[plane].w[column] - len - 4, BAR_HEIGHT);
    XSetForeground(disp, gct, color);
    XFillRectangle(disp, Panel->trace, gct,
                   xofs+2, CHANNEL_HEIGHT(ch)+2,
                   len, BAR_HEIGHT);
  }
}

static void drawProg(int ch, int val, Boolean do_clean) {
  char s[4];

  ch -= VISLOW;
  if (do_clean == True) {
    XSetForeground(disp, gct, boxcolor);
    XFillRectangle(disp,Panel->trace,gct,
                   pl[plane].ofs[CL_PR]+2,CHANNEL_HEIGHT(ch)+2,
                   pl[plane].w[CL_PR]-4,BAR_HEIGHT);
  }
  snprintf(s, sizeof(s), "%3d", val);
  TraceDrawStr(pl[plane].ofs[CL_PR]+5, CHANNEL_HEIGHT(ch)+BAR_HEIGHT-1,
               s, 3, textcolor);
}

static void drawPan(int ch, int val, Boolean setcolor) {
  int ap, bp;
  int x;
  XPoint pp[3];

  if (val < 0) return;

  ch -= VISLOW;
  if (setcolor == True) {
    XSetForeground(disp, gct, boxcolor);
    XFillRectangle(disp, Panel->trace, gct,
                   pl[plane].ofs[CL_PA]+2, CHANNEL_HEIGHT(ch)+2,
                   pl[plane].w[CL_PA]-4, BAR_HEIGHT);
    XSetForeground(disp, gct, pancolor);
  }
  x = pl[plane].ofs[CL_PA] + 3;
  ap = 31 * val/127;
  bp = 31 - ap - 1;
  pp[0].x = ap + x; pp[0].y = 12 + BAR_SPACE*(ch+1);
  pp[1].x = bp + x; pp[1].y = 8 + BAR_SPACE*(ch+1);
  pp[2].x = bp + x; pp[2].y = 16 + BAR_SPACE*(ch+1);
  XFillPolygon(disp, Panel->trace, gct, pp, 3,
               (int)Nonconvex, (int)CoordModeOrigin);
}

static void draw1Chan(int ch, int val, char cmd) {
  if ((cmd == '*') || (cmd == '&'))
    drawBar(ch, (int)(val*BARSCALE2), pl[plane].ofs[CL_VE],
             CL_VE, Panel->barcol[ch]);
}

static void drawVol(int ch, int val) {
  drawBar(ch, (int)(val*BARSCALE3), pl[plane].ofs[CL_VO], CL_VO, volcolor);
}

static void drawExp(int ch, int val) {
  drawBar(ch, (int)(val*BARSCALE4), pl[plane].ofs[CL_EX], CL_EX, expcolor);
}

static void drawReverb(int ch, int val) {
  drawBar(ch, (int)(val*BARSCALE5), pl[plane].ofs[CL_RE], CL_RE, revcolor);
}

static void drawChorus(int ch, int val) {
  drawBar(ch, (int)(val*BARSCALE5), pl[plane].ofs[CL_CH], CL_CH, chocolor);
}

static void drawPitch(int ch, int val) {
  char *s;

  ch -= VISLOW;
  XSetForeground(disp, gct, boxcolor);
  XFillRectangle(disp,Panel->trace,gct,
                 pl[plane].ofs[CL_PI]+2,CHANNEL_HEIGHT(ch)+2,
                 pl[plane].w[CL_PI] -4,BAR_HEIGHT);
  if (val != 0) {
    if (val < 0) {
      s = "=";
    } else {
      if (val == 0x2000) s = "*";
      else if (val > 0x3000) s = ">>";
      else if (val > 0x2000) s = ">";
      else if (val > 0x1000) s = "<";
      else s = "<<";
    }
    TraceDrawStr(pl[plane].ofs[CL_PI]+4, CHANNEL_HEIGHT(ch)+16, s, strlen(s),
                 Panel->barcol[9]);
  }
}

static void drawDrumPart(int ch, int is_drum) {

  if (plane != 0) return;
  if (is_drum) Panel->barcol[ch] = Panel->cfg->drumvelocity_color;
  else         Panel->barcol[ch] = Panel->cfg->velocity_color;
}

static void draw1Note(int ch, int note, int flag) {
  int i, j;
  XSegment dot[3];

  j = note - 9;
  if (j < 0) return;
  ch -= VISLOW;
  if (flag == '*') {
    XSetForeground(disp, gct, playcolor);
  } else if (flag == '&') {
    XSetForeground(disp, gct,
                   ((keyG[j].col == black)?suscolor:Panel->barcol[0]));
  } else {
    XSetForeground(disp, gct, keyG[j].col);
  }
  for(i=0; i<3; i++) {
    dot[i].x1 = keyG[j].xofs + i;
    dot[i].y1 = CHANNEL_HEIGHT(ch) + keyG[j].k[i].y;
    dot[i].x2 = dot[i].x1;
    dot[i].y2 = dot[i].y1 + keyG[j].k[i].l;
  }
  XDrawSegments(disp, Panel->trace, gct, dot, 3);
}

static void ctl_channel_note(int ch, int note, int velocity) {
  if (velocity == 0) {
    if (note == Panel->cnote[ch])
      Panel->v_flags[ch] = FLAG_NOTE_OFF;
    Panel->cvel[ch] = 0;
  } else if (velocity > Panel->cvel[ch]) {
    Panel->cvel[ch] = velocity;
    Panel->cnote[ch] = note;
    Panel->ctotal[ch] = velocity * Panel->channel[ch].volume *
      Panel->channel[ch].expression / (127*127);
    Panel->v_flags[ch] = FLAG_NOTE_ON;
  }
}

static void drawKeyboardAll(Drawable pix, GC gc) {
  int i, j;
  XSegment dot[3];

  XSetForeground(disp, gc, tracecolor);
  XFillRectangle(disp, pix, gc, 0, 0, BARH_OFS8, BAR_SPACE);
  XSetForeground(disp, gc, boxcolor);
  XFillRectangle(disp, pix, gc, BARH_OFS8, 0,
                 trace_width-BARH_OFS8+1, BAR_SPACE);
  for(i=0; i<KEY_NUM; i++) {
    XSetForeground(disp, gc, keyG[i].col);
    for(j=0; j<3; j++) {
      dot[j].x1 = keyG[i].xofs + j;
      dot[j].y1 = keyG[i].k[j].y;
      dot[j].x2 = dot[j].x1;
      dot[j].y2 = dot[j].y1 + keyG[i].k[j].l;
    }
    XDrawSegments(disp, pix, gc, dot, 3);
  }
}

static void drawBank(int ch, int bank, int lsb, int msb) {
  char s[4];

  ch -= VISLOW;
  XSetForeground(disp, gct, boxcolor);
  XFillRectangle(disp, Panel->trace, gct,
                 pl[plane].ofs[CL_BA]+2, CHANNEL_HEIGHT(ch)+2,
                 pl[plane].w[CL_BA]-4, BAR_HEIGHT);
  XFillRectangle(disp, Panel->trace, gct,
                 pl[plane].ofs[CL_BA_MSB]+2, CHANNEL_HEIGHT(ch)+2,
                 pl[plane].w[CL_BA_MSB]-4, BAR_HEIGHT);
  XFillRectangle(disp, Panel->trace, gct,
                 pl[plane].ofs[CL_BA_LSB]+2, CHANNEL_HEIGHT(ch)+2,
                 pl[plane].w[CL_BA_LSB]-4, BAR_HEIGHT);
  snprintf(s, sizeof(s), "%3d", bank);
  TraceDrawStr(pl[plane].ofs[CL_BA]+6, CHANNEL_HEIGHT(ch)+BAR_HEIGHT-1,
               s, strlen(s), textcolor);
  snprintf(s, sizeof(s), "%3d", msb);
  TraceDrawStr(pl[plane].ofs[CL_BA_MSB]+6, CHANNEL_HEIGHT(ch)+BAR_HEIGHT-1,
               s, strlen(s), textcolor);
  snprintf(s, sizeof(s), "%3d", lsb);
  TraceDrawStr(pl[plane].ofs[CL_BA_LSB]+6, CHANNEL_HEIGHT(ch)+BAR_HEIGHT-1,
               s, strlen(s), textcolor);
}

static const char * calcPitch(void)
{
  int i, pitch;
  static const char *keysig_name[] = {
    "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F ", "C ",
    "G ", "D ", "A ", "E ", "B ", "F#", "C#", "G#",
    "D#", "A#"
  };

  pitch = Panel->pitch + ((Panel->pitch < 8) ? 7 : -6);
  if (Panel->poffset > 0)
    for (i = 0; i < Panel->poffset; i++)
      pitch += (pitch > 10) ? -5 : 7;
  else
    for (i = 0; i < -Panel->poffset; i++)
      pitch += (pitch < 7) ? 5 : -7;

  return keysig_name[pitch];
}

#ifdef HAVE_LIBXFT
static void drawFoot(Boolean PitchChanged) {
  char *p, s[4096];
  int l;

  if (PitchChanged) Panel->key_cache = calcPitch();
  if (Panel->pitch < 8) p = "Maj";
  else p = "Min";
  l = snprintf(s, sizeof(s),
               "Voices %3d/%d  Tempo %d/%3d%%  Key %s %s (%+03d)   %s",
               Panel->last_voice, Panel->xaw_i_voices,
               Panel->tempo*Panel->timeratio/100, Panel->timeratio,
               Panel->key_cache, p, Panel->poffset, Panel->title);
  if (l >= sizeof(s) || (l < 0)) l = sizeof(s) - 1;

 /*
  * In the Xft path we draw to a pixmap, and then copy that to screen.
  * Reason is to avoid "flashing" in the footer, as it tends to change often
  * (number of voices playing fluctuates) and Xft may be too slow otherwise.
  */
  XFillRectangle(disp, Panel->xft_trace_foot_pixmap, gcs, 0, 0,
                 trace_width, TRACE_FOOT-2);
#ifdef X_HAVE_UTF8_STRING
  XftDrawStringUtf8(Panel->xft_trace_foot, &Panel->xft_capcolor, ttitle_font,
                    FOOT_HOFS, ttitle_font->ascent, (FcChar8 *)s, l);
#else
  XftDrawString8(Panel->xft_trace_foot, &Panel->xft_capcolor, ttitle_font,
                 FOOT_HOFS, ttitle_font->ascent, (FcChar8 *)s, l);
#endif
  XCopyArea(disp, (Drawable)Panel->xft_trace_foot_pixmap, (Drawable)Panel->trace,
            gcs, 0, 0, trace_width, TRACE_FOOT-2, 0, trace_height_nf+2);
}

static void drawVoices(void) {
  char s[20];
  int l;
  XGlyphInfo extents;

  l = snprintf(s, sizeof(s), "Voices %3d/%d ",
               Panel->last_voice, Panel->xaw_i_voices);
  if ((l >= sizeof(s)) || (l < 0)) l = sizeof(s) - 1;
#ifdef X_HAVE_UTF8_STRING
  XftTextExtentsUtf8(disp, ttitle_font, (FcChar8 *)s, l, &extents);
#else
  XftTextExtents8(disp, ttitle_font, (FcChar8 *)s, l, &extents);
#endif
  if (Panel->voices_width < extents.width) {
    drawFoot(False);
  } else {
    XFillRectangle(disp, Panel->xft_trace_foot_pixmap, gcs, 0, 0,
                   Panel->voices_width, TRACE_FOOT-2);
#ifdef X_HAVE_UTF8_STRING
    XftDrawStringUtf8(Panel->xft_trace_foot, &Panel->xft_capcolor,
                      ttitle_font, FOOT_HOFS, ttitle_font->ascent,
                      (FcChar8 *)s, l);
#else
    XftDrawString8(Panel->xft_trace_foot, &Panel->xft_capcolor,
                   ttitle_font, FOOT_HOFS, ttitle_font->ascent,
                   (FcChar8 *)s, l);
#endif
    XCopyArea(disp, (Drawable)Panel->xft_trace_foot_pixmap, (Drawable)Panel->trace,
              gcs, 0, 0, Panel->voices_width, TRACE_FOOT-2, 0, trace_height_nf+2);
  }
  Panel->voices_width = extents.width;
}

static void drawInstname(int ch, char *name) {
  int len;

  ch -= VISLOW;
  XSetForeground(disp, gct, boxcolor);
  XFillRectangle(disp, Panel->xft_trace_inst_pixmap, gct,
                 0, 0, pl[plane].w[CL_IN]-4, BAR_HEIGHT);
  len = strlen(name);
#ifdef X_HAVE_UTF8_STRING
  XftDrawStringUtf8(Panel->xft_trace_inst, (Panel->is_drum[ch+VISLOW]) ?
                    &Panel->xft_capcolor : &Panel->xft_textcolor,
                    trace_font, 0, trace_font->ascent,
                    (FcChar8 *)name, len);
#else
  XftDrawString8(Panel->xft_trace_inst, (Panel->is_drum[ch+VISLOW]) ?
                 &Panel->xft_capcolor : &Panel->xft_textcolor,
                 trace_font, 0, trace_font->ascent,
                 (FcChar8 *)name, len);
#endif
  XCopyArea(disp, (Drawable)Panel->xft_trace_inst_pixmap,
            (Drawable)Panel->trace, gct, 0, 0, pl[plane].w[CL_IN]-4,
            BAR_HEIGHT, pl[plane].ofs[CL_IN]+2, CHANNEL_HEIGHT(ch)+2);
}

#else /* HAVE_LIBXFT */

static void drawFoot(Boolean PitchChanged) {
  char *p, s[4096];
  int l, w;

  if (PitchChanged) Panel->key_cache = calcPitch();
  if (Panel->pitch < 8) p = "Maj";
  else p = "Min";

  l = snprintf(s, sizeof(s),
               "Voices %3d/%d  Tempo %d/%3d%%  Key %s %s (%+03d)   %s",
               Panel->last_voice, Panel->xaw_i_voices,
               Panel->tempo*Panel->timeratio/100, Panel->timeratio,
               Panel->key_cache, p, Panel->poffset, Panel->title);
  if (l >= sizeof(s) || (l < 0)) l = sizeof(s) - 1;

  w = XmbTextEscapement(ttitle_font, s, l);
  if (w < Panel->foot_width) {
    XSetForeground(disp, gct, tracecolor);
    /*
     * w is reliable enough to detect changes in width used on screen,
     * but can't be trusted enough to clear only w-Panel->foot_width width.
     */ 
    XFillRectangle(disp, Panel->trace, gct, 0,
                   trace_height_nf+2, trace_width,
                   TRACE_FOOT-2);
    XmbDrawString(disp, Panel->trace, ttitle_font, gcs, 2,
                  trace_height_nf+Panel->title_font_ascent, s, l);
  } else {
    XmbDrawImageString(disp, Panel->trace, ttitle_font, gcs, 2,
                       trace_height_nf+Panel->title_font_ascent, s, l);
  }
  Panel->foot_width = w;
}

static void drawVoices(void) {
  char s[20];
  int l, w;

  l = snprintf(s, sizeof(s), "Voices %3d/%d ",
               Panel->last_voice, Panel->xaw_i_voices);
  if ((l >= sizeof(s)) || (l < 0)) l = sizeof(s) - 1;
  w = XmbTextEscapement(ttitle_font, s, l);
  if (Panel->voices_width < w) {
    drawFoot(False);
  } else {
    XmbDrawImageString(disp, Panel->trace, ttitle_font, gcs, 2,
                       trace_height_nf+Panel->title_font_ascent, s, l);
  }
  Panel->voices_width = w;
}

static void drawInstname(int ch, char *name) {
  int len;

  if (plane != 0) return;
  ch -= VISLOW;
  len = strlen(name);
  XSetForeground(disp, gct, boxcolor);
  XFillRectangle(disp, Panel->trace, gct,
                 pl[plane].ofs[CL_IN]+2, CHANNEL_HEIGHT(ch)+2,
                 pl[plane].w[CL_IN]-4, BAR_HEIGHT);
  TraceDrawStr(pl[plane].ofs[CL_IN]+4, CHANNEL_HEIGHT(ch)+15,
               name, len, (Panel->is_drum[ch+VISLOW])?capcolor:textcolor);
}
#endif /* HAVE_LIBXFT */

static void drawMute(int ch, int mute) {
  char s[16];

  if (mute != 0) {
    SET_CHANNELMASK(channel_mute, ch);
    if (!XAWLIMIT(ch)) return;
    /*
     * If we drew the number using textbgcolor, it would only look clear in
     * the non-Xft case, since AA may prevent the exact same pixels being used.
     */
    XSetForeground(disp, gct, textbgcolor);
    XFillRectangle(disp, Panel->trace, gct, pl[plane].ofs[CL_C]+2,
                   CHANNEL_HEIGHT(ch-VISLOW)+2, pl[plane].w[CL_C]-4, BAR_HEIGHT);
  } else {
    UNSET_CHANNELMASK(channel_mute, ch);
    if (!XAWLIMIT(ch)) return;
    /* timidity internals counts from 0. timidity ui counts from 1 */
    ch++;
    snprintf(s, sizeof(s), "%2d", ch);
    TraceDrawStr(pl[plane].ofs[CL_C]+2, CHANNEL_HEIGHT(ch-VISLOW)-5, s, 2, textcolor);
  }
}

/* End of privates */

int handleTraceinput(char *local_buf) {
  char c;
  int ch, i, n;

#define EXTRACT_CH(s,off) do { \
  ch = atoi(s+off); \
  local_buf = strchr(s, CH_END_TOKEN) - off; \
} while(0)

  switch (local_buf[0]) {
  case MT_NOTE:
    {
      int note;

      EXTRACT_CH(local_buf, 1);
      c = *(local_buf+2);
      note = (*(local_buf+3) - '0') * 100 + (*(local_buf+4) - '0') * 10 +
              *(local_buf+5) - '0';
      n = atoi(local_buf+6);
      if ((c == '*') || (c == '&')) {
        Panel->c_flags[ch] |= FLAG_PROG_ON;
      } else {
        Panel->c_flags[ch] &= ~FLAG_PROG_ON; n = 0;
      }
      ctl_channel_note(ch, note, n);
      if (!XAWLIMIT(ch)) break;
      draw1Note(ch, note, c);
      draw1Chan(ch, Panel->ctotal[ch], c);
    }
    break;
  case MT_UPDATE_TIMER:       /* update timer */
    {
      static double last_time = 0;
      double d, t;
      Bool need_flush;
      double delta_time;

      t = get_current_calender_time();
      d = t - last_time;
      if (d > 1) d = 1;
      delta_time = d / TRACE_UPDATE_TIME;
      last_time = t;
      need_flush = False;
      for(i=0; i<MAX_TRACE_CHANNELS; i++)
        if (Panel->v_flags[i] != 0) {
          if (Panel->v_flags[i] == FLAG_NOTE_OFF) {
            Panel->ctotal[i] -= DELTA_VEL * delta_time;
            if (Panel->ctotal[i] <= 0) {
              Panel->ctotal[i] = 0;
              Panel->v_flags[i] = 0;
            }
            if (XAWLIMIT(i)) draw1Chan(i, Panel->ctotal[i], '*');
            need_flush = True;
          } else {
            Panel->v_flags[i] = 0;
          }
        }
      if (need_flush) XFlush(disp);
    }
    break;
  case MT_VOICES:
    c = *(local_buf+1);
    n = atoi(local_buf+2);
    if (c == MTV_TOTAL_VOICES) {
      if (Panel->xaw_i_voices != n) {
        Panel->xaw_i_voices = n;
        drawVoices();
      }
    }
    else
      if (Panel->last_voice != n) {
        Panel->last_voice = n;
        drawVoices();
      }
    break;
  case MT_REDRAW_TRACE:
    redrawTrace(True);
    break;
  case MT_MUTE:
    EXTRACT_CH(local_buf, 1);
    n = atoi(local_buf+2);
    drawMute(ch, n);
    break;
  case MT_INST_NAME:
    EXTRACT_CH(local_buf, 1);
    strlcpy(Panel->inst_name[ch], (char *)&local_buf[2], INST_NAME_SIZE);
    if (!XAWLIMIT(ch)) break;
    drawInstname(ch, Panel->inst_name[ch]);
    break;
  case MT_IS_DRUM:
    EXTRACT_CH(local_buf, 1);
    Panel->is_drum[ch] = *(local_buf+2) - 'A';
    if (!XAWLIMIT(ch)) break;
    drawDrumPart(ch, Panel->is_drum[ch]);
    break;
  case MT_PANEL_INFO:
    c = *(local_buf+1);
    EXTRACT_CH(local_buf, 2);
    n = atoi(local_buf+3);
    switch(c) {
    case MTP_PANNING:
      Panel->channel[ch].panning = n;
      Panel->c_flags[ch] |= FLAG_PAN;
      if (plane || !XAWLIMIT(ch)) break;
      drawPan(ch, n, True);
      break;
    case MTP_PITCH_BEND:
      Panel->channel[ch].pitchbend = n;
      Panel->c_flags[ch] |= FLAG_BENDT;
      if (plane || !XAWLIMIT(ch)) break;
      drawPitch(ch, n);
      break;
    case MTP_TONEBANK:
      Panel->channel[ch].bank = n & 0xff;
      Panel->channel[ch].bank_lsb = (n >> 8) & 0xff;
      Panel->channel[ch].bank_msb = (n >> 16) & 0xff;
      if (!plane || (!XAWLIMIT(ch))) break;
      drawBank(ch, Panel->channel[ch].bank, Panel->channel[ch].bank_lsb,
               Panel->channel[ch].bank_msb);
      break;
    case MTP_REVERB:
      Panel->reverb[ch] = n;
      if (!plane || !XAWLIMIT(ch)) break;
      drawReverb(ch, n);
      break;
    case MTP_CHORUS:
      Panel->channel[ch].chorus_level = n;
      if (!plane || !XAWLIMIT(ch)) break;
      drawChorus(ch, n);
      break;
    case MTP_SUSTAIN:
      Panel->channel[ch].sustain = n;
      Panel->c_flags[ch] |= FLAG_SUST;
      break;
    case MTP_PROGRAM:
      Panel->channel[ch].program = n;
      Panel->c_flags[ch] |= FLAG_PROG;
      if (!XAWLIMIT(ch)) break;
      drawProg(ch, n, True);
      break;
    case MTP_EXPRESSION:
      Panel->channel[ch].expression = n;
      ctl_channel_note(ch, Panel->cnote[ch], Panel->cvel[ch]);
      if (!XAWLIMIT(ch)) break;
      drawExp(ch, n);
      break;
    case MTP_VOLUME:
      Panel->channel[ch].volume = n;
      ctl_channel_note(ch, Panel->cnote[ch], Panel->cvel[ch]);
      if (!XAWLIMIT(ch)) break;
      drawVol(ch, n);
      break;
    }
    break;
  case MT_TEMPO:
    i = atoi(local_buf+1);
    n = (int) (500000/ (double)i * 120 + 0.5);
    if (Panel->tempo != n) {
      Panel->tempo = n;
      drawFoot(False);
    }
    break;
  case MT_RATIO:
    n = atoi(local_buf+1);
    if (Panel->timeratio != n) {
      Panel->timeratio = n;
      drawFoot(False);
    }
    break;
  case MT_PITCH_OFFSET:
    n = atoi(local_buf+1);
    if (Panel->poffset != n) {
      Panel->poffset = n;
      drawFoot(True);
    }
    break;
  case MT_PITCH:
    n = atoi(local_buf+1);
    if (Panel->pitch != n) {
      Panel->pitch = n;
      drawFoot(True);
    }
    break;
  default:
    return -1;
  }
  return 0;
}

void redrawTrace(Boolean draw) {
  int i;
  char s[3];

  for(i=0; i<VISIBLE_CHANNELS; i++) {
    XGCValues gv;

    gv.tile = layer[plane];
    gv.ts_x_origin = 0;
    gv.ts_y_origin = CHANNEL_HEIGHT(i);
    XChangeGC(disp, gc_xcopy, GCTile|GCTileStipXOrigin|GCTileStipYOrigin, &gv);
    XFillRectangle(disp, Panel->trace, gc_xcopy,
                   0, CHANNEL_HEIGHT(i), trace_width, BAR_SPACE);
  }
  XSetForeground(disp, gct, capcolor);
  XDrawLine(disp, Panel->trace, gct, BARH_OFS0, trace_height_nf,
            trace_width-1, trace_height_nf);

  for(i=VISLOW+1; i<VISLOW+VISIBLE_CHANNELS+1; i++) {
    snprintf(s, sizeof(s), "%2d", i);
    if (IS_SET_CHANNELMASK(channel_mute, i-1))
      TraceDrawStr(pl[plane].ofs[CL_C]+2, CHANNEL_HEIGHT(i-VISLOW)-5,
                   s, 2, textbgcolor);
    else
      TraceDrawStr(pl[plane].ofs[CL_C]+2, CHANNEL_HEIGHT(i-VISLOW)-5,
                   s, 2, textcolor);
  }

  if (Panel->g_cursor_is_in) {
    XSetForeground(disp, gct, capcolor);
    XFillRectangle(disp, Panel->trace, gct, 0, 0, trace_width, TRACE_HEADER);
  }
  redrawCaption(Panel->g_cursor_is_in);

  drawFoot(True);
  if (draw) {
    for(i=VISLOW; i<VISLOW+VISIBLE_CHANNELS; i++) {
      if ((Panel->ctotal[i] != 0) && (Panel->c_flags[i] & FLAG_PROG_ON))
        draw1Chan(i, Panel->ctotal[i], '*');
      drawProg(i, Panel->channel[i].program, False);
      drawVol(i, Panel->channel[i].volume);
      drawExp(i, Panel->channel[i].expression);
      if (plane) {
        drawBank(i, Panel->channel[i].bank, Panel->channel[i].bank_lsb,
                 Panel->channel[i].bank_msb);
        drawReverb(i, Panel->reverb[i]);
        drawChorus(i, Panel->channel[i].chorus_level);
      } else {
        drawPitch(i, Panel->channel[i].pitchbend);
        drawInstname(i, Panel->inst_name[i]);
      }
    }
    if (!plane) {
      XSetForeground(disp, gct, pancolor);
      for(i=VISLOW; i<VISLOW+VISIBLE_CHANNELS; i++) {
        if (Panel->c_flags[i] & FLAG_PAN)
          drawPan(i, Panel->channel[i].panning, False);
      }
      XSetForeground(disp, gct, textcolor);
    }
  }
}

void redrawCaption(Boolean cursor_is_in) {
  const char *p;
  int i;

  Panel->g_cursor_is_in = cursor_is_in;
  if (cursor_is_in) {
    XSetForeground(disp, gct, capcolor);
    XFillRectangle(disp, Panel->trace, gct, 0, 0, trace_width, TRACE_HEADER);
    XSetBackground(disp, gct, expcolor);
    for(i=0; i<pl[plane].col; i++) {
      p = pl[plane].cap[i];
      TitleDrawStr(pl[plane].ofs[i]+4, BAR_HEIGHT, p, strlen(p), tracecolor);
    }
  } else {
    XSetForeground(disp, gct, tracecolor);
    XFillRectangle(disp, Panel->trace, gct, 0, 0, trace_width, TRACE_HEADER);
    XSetBackground(disp, gct, tracecolor);
    for(i=0; i<pl[plane].col; i++) {
      p = pl[plane].cap[i];
      TitleDrawStr(pl[plane].ofs[i]+4, BAR_HEIGHT, p, strlen(p), capcolor);
    }
  }
}

void initStatus(void) {
  int i;

  for(i=0; i<MAX_TRACE_CHANNELS; i++) {
    Panel->c_flags[i] = 0;
    Panel->channel[i].bank = 0;
    Panel->channel[i].chorus_level = 0;
    Panel->channel[i].expression = 0;
    Panel->channel[i].panning = -1;
    Panel->channel[i].pitchbend = 0;
    Panel->channel[i].program = 0;
    Panel->channel[i].sustain = 0;
    Panel->channel[i].volume = 0;
    Panel->cnote[i] = 0;
    Panel->cvel[i] = 0;
    Panel->ctotal[i] = 0;
    Panel->is_drum[i] = 0;
    *(Panel->inst_name[i]) = '\0';
    Panel->reverb[i] = 0;
    Panel->v_flags[i] = 0;
  }
  Panel->multi_part = 0;
  Panel->pitch = 0;
  Panel->poffset = 0;
  Panel->tempo = 100;
  Panel->timeratio = 100;
  Panel->last_voice = 0;
}

void scrollTrace(int direction) {
  if (direction > 0) {
    if (Panel->multi_part < (MAX_TRACE_CHANNELS - 2*VISIBLE_CHANNELS))
      Panel->multi_part += VISIBLE_CHANNELS;
    else if (Panel->multi_part < (MAX_TRACE_CHANNELS - VISIBLE_CHANNELS))
      Panel->multi_part = MAX_TRACE_CHANNELS - VISIBLE_CHANNELS;
    else
      Panel->multi_part = 0;
  } else {
    if (Panel->multi_part > VISIBLE_CHANNELS)
      Panel->multi_part -= VISIBLE_CHANNELS;
    else if (Panel->multi_part > 0)
      Panel->multi_part = 0;
    else 
      Panel->multi_part = MAX_TRACE_CHANNELS - VISIBLE_CHANNELS;
  }
  redrawTrace(True);
}

void toggleTracePlane(Boolean draw) {
  plane ^= 1;
  redrawTrace(draw);
}

int getLowestVisibleChan(void) {
  return Panel->multi_part;
}

int getVisibleChanNum(void) {
  return Panel->visible_channels;
}

void initTrace(Display *dsp, Window trace, char *title, tconfig *cfg) {
#ifdef HAVE_LIBXFT
  char *font_name;
#else
  char **ml;
  XFontStruct **fs_list;
#endif
  int i, j, k, tmpi, w, screen;
  unsigned long gcmask;
  XGCValues gv;

  Panel = (PanelInfo *)safe_malloc(sizeof(PanelInfo));
  Panel->trace = trace;
  if (!strcmp(title, "(null)")) Panel->title = (char *)UNTITLED_STR;
  else Panel->title = title;
  Panel->cfg = cfg;
  plane = 0;
  Panel->g_cursor_is_in = False;
  disp = dsp;
  screen = DefaultScreen(disp);
  Panel->key_cache = "C ";
  if (gradient_bar) {
    Panel->grad = (GradData *)safe_malloc(sizeof(GradData));
    Panel->depth = getdisplayinfo(&Panel->grad->rgb);
    for (i=0; i<MAX_GRADIENT_COLUMN; i++) gradient_set[i] = 0;
    x_boxcolor.pixel = boxcolor;
    XQueryColor(disp, DefaultColormap(disp, 0), &x_boxcolor);
  } else {
    Panel->grad = NULL;
    Panel->depth = getdisplayinfo(NULL);
  }

  for(i=0; i<MAX_TRACE_CHANNELS; i++) {
    if (ISDRUMCHANNEL(i)) {
      Panel->is_drum[i] = 1;
      Panel->barcol[i] = cfg->drumvelocity_color;
    } else {
      Panel->barcol[i] = cfg->velocity_color;
    }
    Panel->inst_name[i] = (char *)safe_malloc(sizeof(char) * INST_NAME_SIZE);
  }
  initStatus();
  Panel->xaw_i_voices = 0;
  Panel->visible_channels = (cfg->trace_height - TRACE_HEADER - TRACE_FOOT) /
                            BAR_SPACE;
  if (Panel->visible_channels > MAX_CHANNELS)
    Panel->visible_channels = MAX_CHANNELS;
  else if (Panel->visible_channels < 1)
    Panel->visible_channels = 1;
  /* This prevents empty space between the trace foot and the channel bars. */
  cfg->trace_height = Panel->visible_channels * BAR_SPACE +
                      TRACE_HEADER + TRACE_FOOT;

  Panel->voices_width = 0;
#ifdef HAVE_LIBXFT
  if ((XftInit(NULL) != FcTrue) || (XftInitFtLibrary() != FcTrue)) {
    fprintf(stderr, "Xft can't init font library!\n");
    exit(1);
  }

  font_name = XBaseFontNameListOfFontSet(Panel->cfg->c_title_font);
  ttitle_font = XftFontOpenXlfd(disp, screen, font_name);
  if (ttitle_font == NULL)
    ttitle_font = XftFontOpenName(disp, screen, font_name);
  if (ttitle_font == NULL) {
    fprintf(stderr, "can't load font %s\n", font_name);
    exit(1);
  }

  font_name = XBaseFontNameListOfFontSet(Panel->cfg->c_trace_font);
  trace_font = XftFontOpenXlfd(disp, screen, font_name);
  if (trace_font == NULL)
    trace_font = XftFontOpenName(disp, screen, font_name);
  if (trace_font == NULL) {
    fprintf(stderr, "can't load font %s\n", font_name);
    exit(1);
  }

  Panel->xft_trace = XftDrawCreate(disp, (Drawable)Panel->trace,
                                   DefaultVisual(disp, screen),
                                   DefaultColormap(disp, screen));
  Panel->xft_trace_foot_pixmap = XCreatePixmap (disp, (Drawable)Panel->trace,
                                                trace_width, TRACE_FOOT-2,
                                                Panel->depth);
  Panel->xft_trace_foot = XftDrawCreate(disp,
                                        (Drawable)Panel->xft_trace_foot_pixmap,
                                        DefaultVisual(disp, screen),
                                        DefaultColormap(disp, screen));
  Panel->xft_trace_inst_pixmap = XCreatePixmap (disp, (Drawable)Panel->trace,
                                                pl[plane].w[CL_IN]-4,
                                                BAR_HEIGHT, Panel->depth);
  Panel->xft_trace_inst = XftDrawCreate(disp,
                                        (Drawable)Panel->xft_trace_inst_pixmap,
                                        DefaultVisual(disp, screen),
                                        DefaultColormap(disp, screen));

  COPY_PIXEL (Panel->xft_capcolor, capcolor);
  COPY_PIXEL (Panel->xft_textcolor, textcolor);

  gcmask = GCForeground;
  gv.foreground = tracecolor;
  gcs = XCreateGC(disp, RootWindow(disp, screen), gcmask, &gv);
#else
  Panel->foot_width = trace_width;

  j = XFontsOfFontSet(ttitle_font, &fs_list, &ml);
  Panel->title_font_ascent = fs_list[0]->ascent;
  for (i=1; i<j; i++) {
    if (Panel->title_font_ascent < fs_list[i]->ascent)
      Panel->title_font_ascent = fs_list[i]->ascent;
  }

  gcmask = GCForeground | GCBackground;
  gv.foreground = capcolor;
  gv.background = tracecolor;
  gv.plane_mask = 1;
  gcs = XCreateGC(disp, RootWindow(disp, screen), gcmask, &gv);
#endif /* HAVE_LIBXFT */

  gv.fill_style = FillTiled;
  gv.fill_rule = WindingRule;
  gc_xcopy = XCreateGC(disp, RootWindow(disp, screen),
                       GCFillStyle | GCFillRule, &gv);
  gct = XCreateGC(disp, RootWindow(disp, screen), 0, NULL);

  if (keyG == NULL) keyG = (ThreeL *)safe_malloc(sizeof(ThreeL) * KEY_NUM);
  for(i=0, j=BARH_OFS8+1; i<KEY_NUM; i++) {
    tmpi = i%12;
    switch (tmpi) {
    case 0:
    case 5:
    case 10:
      keyG[i].k[0].y = 11; keyG[i].k[0].l = 7;
      keyG[i].k[1].y = 2; keyG[i].k[1].l = 16;
      keyG[i].k[2].y = 11; keyG[i].k[2].l = 7;
      keyG[i].col = white;
      break;
    case 2:
    case 7:
      keyG[i].k[0].y = 11; keyG[i].k[0].l = 7;
      keyG[i].k[1].y = 2; keyG[i].k[1].l = 16;
      keyG[i].k[2].y = 2; keyG[i].k[2].l = 16;
      keyG[i].col = white;
      break;
    case 3:
    case 8:
      j += 2;
      keyG[i].k[0].y = 2; keyG[i].k[0].l = 16;
      keyG[i].k[1].y = 2; keyG[i].k[1].l = 16;
      keyG[i].k[2].y = 11; keyG[i].k[2].l = 7;
      keyG[i].col = white;
      break;
    default:  /* black key */
      keyG[i].k[0].y = 2; keyG[i].k[0].l = 8;
      keyG[i].k[1].y = 2; keyG[i].k[1].l = 8;
      keyG[i].k[2].y = 2; keyG[i].k[2].l = 8;
      keyG[i].col = black;
      break;
    }
    keyG[i].xofs = j; j += 2;
  }

  /* draw on template pixmaps that includes one channel row */
  for(i=0; i<2; i++) {
    layer[i] = XCreatePixmap(disp, Panel->trace, trace_width, BAR_SPACE,
                             DefaultDepth(disp, screen));
    drawKeyboardAll(layer[i], gct);
    XSetForeground(disp, gct, capcolor);
    XDrawLine(disp, layer[i], gct, 0, 0, trace_width, 0);
    XDrawLine(disp, layer[i], gct, 0, 0, 0, BAR_SPACE);
    XDrawLine(disp, layer[i], gct, trace_width-1, 0, trace_width-1, BAR_SPACE);

    for(j=0; j < pl[i].col-1; j++) {
      tmpi = TRACE_HOFS; w = pl[i].w[j];
      for(k=0; k<j; k++) tmpi += pl[i].w[k];
      tmpi = pl[i].ofs[j];
      XSetForeground(disp, gct, capcolor);
      XDrawLine(disp, layer[i], gct, tmpi+w, 0, tmpi+w, BAR_SPACE);
      XSetForeground(disp, gct, rimcolor);
      XDrawLine(disp, layer[i], gct, tmpi+w-2, 2, tmpi+w-2, BAR_HEIGHT+1);
      XDrawLine(disp, layer[i], gct, tmpi+2, BAR_HEIGHT+2, tmpi+w-2,
                BAR_HEIGHT+2);
      XSetForeground(disp, gct, j?boxcolor:textbgcolor);
      XFillRectangle(disp, layer[i], gct, tmpi+2, 2, w-4, BAR_HEIGHT);
    }
  }
}

void uninitTrace(void) {
  int i;

  XFreePixmap(disp, layer[0]); XFreePixmap(disp, layer[1]);
#ifdef HAVE_LIBXFT
  XftDrawDestroy(Panel->xft_trace);
  XftDrawDestroy(Panel->xft_trace_foot);
  XftDrawDestroy(Panel->xft_trace_inst);
  XFreePixmap(disp, Panel->xft_trace_foot_pixmap);
  XFreePixmap(disp, Panel->xft_trace_inst_pixmap);
  XftFontClose(disp, ttitle_font);
  XftFontClose(disp, trace_font);
#endif
  if (Panel->grad != NULL) for (i=0; i<MAX_GRADIENT_COLUMN; i++) {
    if (gradient_set[i]) {
      XFreePixmap(disp, gradient_pixmap[i]);
      XFreeGC(disp, gradient_gc[i]);
    }
  }
  XFreeGC(disp, gcs); XFreeGC(disp, gct); XFreeGC(disp, gc_xcopy); 

  for (i=0; i<MAX_TRACE_CHANNELS; i++) free(Panel->inst_name[i]);
  free(Panel->grad); free(Panel); free(keyG);
}
