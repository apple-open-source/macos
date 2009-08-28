/* 
 * Copyright (C) 1997-2002 Kare Sjolander <kare@speech.kth.se>
 *
 * This file is part of the Snack Sound Toolkit.
 * The latest version can be found at http://www.speech.kth.se/snack/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define NDEFCOLS 256
#define FRAMESIZE 262144

typedef struct SnackItemInfo {
  int    fftlen;
  int    winlen;
  float  spacing;
  float  *hamwin;
  float  preemph;
  int    BufPos;
  int    RestartPos;
  short  *frame[100];
  int    nfrms;
  int    frlen;
  float  **blocks;
  Pixmap pixmap;
  int    nfft;
  int    fftmax;
  int    fftmin;
  int    debug;
  int    samprate;
  int    encoding;
  int    nchannels;
  int    channel;
  int    channelSet;
  float  abmax;
  double bright;
  double contrast;
  double topfrequency;
  int    limit;
  double gridTspacing;
  int    gridFspacing;
  double pixpsec;
  int    ncolors;
  XColor **xcolor;
  int    subsample;
  XColor *gridcolor;
  int    depth;
  Visual *visual;
  Display *display;
  unsigned long *pixelmap;
  float  xUnderSamp;
  int xTot;
  int doneSpeg;
  char *fcname;
  int storeType;
  Sound *sound;
  int ssmp;
  int validStart;
  Tcl_Obj *cmdPtr;
  int computing;
  int skip;
  SnackWindowType windowType;
  SnackWindowType windowTypeSet;

} SnackItemInfo;

#define CONF_WIDTH 1
#define CONF_PPS 2
#define CONF_WIDTH_PPS 3

extern int  CheckFFTlen(Tcl_Interp *interp, int fftlen);

extern int  CheckWinlen(Tcl_Interp *interp, int winlen, int fftlen);

extern int  CheckLPCorder(Tcl_Interp *interp, int lpcorder);

#if !defined(WIN) && !defined(MAC) && !defined(MAC_OSX_TK)
#define TkPutImage(colors, ncolors, display, pixels, gc, image, \
		   destx, desty, srcx, srcy, width, height) \
        XPutImage(                  display, pixels, gc, image, \
		   destx, desty, srcx, srcy, width, height);
#endif

#if defined(WIN) || defined(MAC) || defined(MAC_OSX_TK)
#  define XFree(data) {if ((data) != NULL) ckfree((char *) (data));}
#endif

#if defined MAC
#  include <tclMacMath.h>
#  define hypot hypotd

  extern double hypot(double x, double y);
#endif

#define OptSpecified(option) (configSpecs[option].specFlags & TK_CONFIG_OPTION_SPECIFIED)

#ifdef __cplusplus
}
#endif
