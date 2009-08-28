/* 
 * Copyright (C) 1997-2005 Kare Sjolander <kare@speech.kth.se>
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

#include "tcl.h"
#include "snack.h"
#include <stdio.h>
#include <stdlib.h>
#define USE_OLD_CANVAS /* To keep Tk8.3 happy */
#include "tk.h"
#include "jkCanvItems.h"
#include <string.h>
#include <math.h>
#ifdef MAC
#include <Xlib.h>
#include <Xutil.h>
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#ifndef Solaris
#  ifndef TkPutImage
EXTERN void TkPutImage _ANSI_ARGS_((unsigned long *colors,
				    int ncolors, Display* display, Drawable d,
				    GC gc, XImage* image, int src_x, int src_y,
				    int dest_x, int dest_y, unsigned int width,
				    unsigned int height));
#  endif
#endif

#define SNACK_DEFAULT_SPEGWINTYPE      SNACK_WIN_HAMMING
#define SNACK_DEFAULT_SPEGWINTYPE_NAME "hamming"

/*
 * Spectrogram item structure
 */

typedef struct SpectrogramItem  {

  Tk_Item header;
  Tk_Canvas canvas;
  double x, y;
  Tk_Anchor anchor;
  char *newSoundName;
  char *soundName;
  Sound *sound;
  SnackItemInfo si;
  int height;
  int width;
  int oldheight;
  int oldwidth;
  int startSmp;
  int endSmp;
  int ssmp;
  int esmp;
  int id;
  int mode;
  GC copyGC;
  double bright;
  double contrast;
  char *channelstr;
  double topFrequency;
  int infft;
  char *progressCmd;
  char *windowTypeStr;
  Tcl_Interp *interp;
  double preemph;

} SpectrogramItem;

float xfft[NMAX];

static int ParseColorMap(ClientData clientData, Tcl_Interp *interp,
			 Tk_Window tkwin, CONST84 char *value, char *recordPtr,
			 int offset);

static char *PrintColorMap(ClientData clientData, Tk_Window tkwin,
			   char *recordPtr, int offset,
			   Tcl_FreeProc **freeProcPtr);

Tk_CustomOption spegTagsOption = { (Tk_OptionParseProc *) NULL,
				   (Tk_OptionPrintProc *) NULL,
				   (ClientData) NULL };

static Tk_CustomOption colorMapOption = { ParseColorMap,
					  PrintColorMap, 
					  (ClientData) NULL};

typedef enum {
  OPTION_ANCHOR,
  OPTION_TAGS,
  OPTION_SOUND,
  OPTION_HEIGHT,
  OPTION_WIDTH,
  OPTION_FFTLEN,
  OPTION_WINLEN,
  OPTION_PREEMP,
  OPTION_PIXPSEC,
  OPTION_START,
  OPTION_END,
  OPTION_BRIGHTNESS,
  OPTION_CONTRAST,
  OPTION_TOPFREQUENCY,
  OPTION_GRIDTSPACING,
  OPTION_GRIDFSPACING,
  OPTION_CHANNEL,
  OPTION_COLORMAP,
  OPTION_PROGRESS,
  OPTION_GRIDCOLOR,
  OPTION_WINTYPE
} ConfigSpec;

static Tk_ConfigSpec configSpecs[] = {

  {TK_CONFIG_ANCHOR, "-anchor", (char *) NULL, (char *) NULL,
   "nw", Tk_Offset(SpectrogramItem, anchor), TK_CONFIG_DONT_SET_DEFAULT},
  
  {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
   (char *) NULL, 0, TK_CONFIG_NULL_OK, &spegTagsOption},
  
  {TK_CONFIG_STRING, "-sound", (char *) NULL, (char *) NULL,
   "", Tk_Offset(SpectrogramItem, newSoundName), TK_CONFIG_NULL_OK},
  
  {TK_CONFIG_INT, "-height", (char *) NULL, (char *) NULL,
   "128", Tk_Offset(SpectrogramItem, height), 0},
  
  {TK_CONFIG_INT, "-width", (char *) NULL, (char *) NULL,
   "378", Tk_Offset(SpectrogramItem, width), 0},
  
  {TK_CONFIG_INT, "-fftlength", (char *) NULL, (char *) NULL,
   "256", Tk_Offset(SpectrogramItem, si.fftlen), 0},
  
  {TK_CONFIG_INT, "-winlength", (char *) NULL, (char *) NULL,
   "128", Tk_Offset(SpectrogramItem, si.winlen), 0},
  
  {TK_CONFIG_DOUBLE, "-preemphasisfactor", (char *) NULL, (char *) NULL,
   "0.97", Tk_Offset(SpectrogramItem, preemph), 0},
  
  {TK_CONFIG_DOUBLE, "-pixelspersecond", "pps", (char *) NULL,
   "250.0", Tk_Offset(SpectrogramItem, si.pixpsec), 0},
  
  {TK_CONFIG_INT, "-start", (char *) NULL, (char *) NULL,
   "0", Tk_Offset(SpectrogramItem, startSmp), 0},
  
  {TK_CONFIG_INT, "-end", (char *) NULL, (char *) NULL,
   "-1", Tk_Offset(SpectrogramItem, endSmp), 0},
  
  {TK_CONFIG_DOUBLE, "-brightness", (char *) NULL, (char *) NULL,
   "0.0", Tk_Offset(SpectrogramItem, bright), 0},

  {TK_CONFIG_DOUBLE, "-contrast", (char *) NULL, (char *) NULL,
   "0.0", Tk_Offset(SpectrogramItem, contrast), 0},

  {TK_CONFIG_DOUBLE, "-topfrequency", (char *) NULL, (char *) NULL,
   "0.0", Tk_Offset(SpectrogramItem, topFrequency), 0},
  
  {TK_CONFIG_DOUBLE, "-gridtspacing", (char *) NULL, (char *) NULL,
   "0.0", Tk_Offset(SpectrogramItem, si.gridTspacing), 0},

  {TK_CONFIG_INT, "-gridfspacing", (char *) NULL, (char *) NULL,
   "0", Tk_Offset(SpectrogramItem, si.gridFspacing), 0},
  
  {TK_CONFIG_STRING, "-channel", (char *) NULL, (char *) NULL,
   "-1", Tk_Offset(SpectrogramItem, channelstr), TK_CONFIG_NULL_OK},

  {TK_CONFIG_CUSTOM, "-colormap", (char *) NULL, (char *) NULL,
   "", Tk_Offset(SpectrogramItem, si.xcolor), TK_CONFIG_NULL_OK,
   &colorMapOption},

  {TK_CONFIG_STRING, "-progress", (char *) NULL, (char *) NULL,
   "", Tk_Offset(SpectrogramItem, progressCmd), TK_CONFIG_NULL_OK},

  {TK_CONFIG_COLOR, "-gridcolor", (char *) NULL, (char *) NULL,
   "red", Tk_Offset(SpectrogramItem, si.gridcolor), TK_CONFIG_NULL_OK},

  {TK_CONFIG_STRING, "-windowtype", (char *) NULL, (char *) NULL,
   SNACK_DEFAULT_SPEGWINTYPE_NAME, Tk_Offset(SpectrogramItem, windowTypeStr),
   0},

  {TK_CONFIG_INT, "-debug", (char *) NULL, (char *) NULL,
   "0", Tk_Offset(SpectrogramItem, si.debug), 0},
  
  {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
   (char *) NULL, 0, 0}

};

/*
 * Protos
 */

static void   ComputeSpectrogramBbox(Tk_Canvas canvas,
				     SpectrogramItem *spegPtr);

static int    ConfigureSpectrogram(Tcl_Interp *interp, Tk_Canvas canvas,
				   Tk_Item *itemPtr, int argc,
				   char **argv, int flags);

static int    CreateSpectrogram(Tcl_Interp *interp, Tk_Canvas canvas,
				struct Tk_Item *itemPtr,
				int argc, char **argv);

static void   DeleteSpectrogram(Tk_Canvas canvas, Tk_Item *itemPtr,
				Display *display);

static void   DisplaySpectrogram(Tk_Canvas canvas, Tk_Item *itemPtr,
				 Display *display, Drawable dst,
				 int x, int y, int width, int height);

static void   ScaleSpectrogram(Tk_Canvas canvas, Tk_Item *itemPtr,
			       double originX, double originY,
			       double scaleX, double scaleY);

static int    SpectrogramCoords(Tcl_Interp *interp, Tk_Canvas canvas,
				Tk_Item *itemPtr, int argc, char **argv);

static int    SpectrogramToArea(Tk_Canvas canvas, Tk_Item *itemPtr,
				double *rectPtr);

static double SpectrogramToPoint(Tk_Canvas canvas, Tk_Item *itemPtr,
				 double *coordPtr);

static int    SpectrogramToPS(Tcl_Interp *interp, Tk_Canvas canvas,
			      Tk_Item *itemPtr, int prepass);

static void   TranslateSpectrogram(Tk_Canvas canvas, Tk_Item *itemPtr,
				   double deltaX, double deltaY);

static int    ComputeSpeg(SnackItemInfo *siPtr, int nfft);

static void   DrawSpeg(SnackItemInfo *siPtr, Display* display,
		       GC gc, int width, int height, int x, int w, int pos);

/*
 * Spectrogram item type
 */

Tk_ItemType snackSpectrogramType = {
  "spectrogram",
  sizeof(SpectrogramItem),
  CreateSpectrogram,
  configSpecs,
  ConfigureSpectrogram,
  SpectrogramCoords,
  DeleteSpectrogram,
  DisplaySpectrogram,
  0,
  SpectrogramToPoint,
  SpectrogramToArea,
  SpectrogramToPS,
  ScaleSpectrogram,
  TranslateSpectrogram,
  (Tk_ItemIndexProc *) NULL,
  (Tk_ItemCursorProc *) NULL,
  (Tk_ItemSelectionProc *) NULL,
  (Tk_ItemInsertProc *) NULL,
  (Tk_ItemDCharsProc *) NULL,
  (Tk_ItemType *) NULL
};

static int
CreateSpectrogram(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
		  int argc, char **argv)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) itemPtr; 
  Tk_Window tkwin = Tk_CanvasTkwin(canvas);
 
  if (argc < 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     Tk_PathName(Tk_CanvasTkwin(canvas)), " create ",
		     itemPtr->typePtr->name, " x y ?opts?\"", (char *) NULL);
    return TCL_ERROR;
  }

  spegPtr->canvas = canvas;
  spegPtr->anchor = TK_ANCHOR_NW;
  spegPtr->newSoundName = NULL;
  spegPtr->soundName = NULL;
  spegPtr->sound = NULL;
  spegPtr->height = 128;
  spegPtr->width = 378;
  spegPtr->startSmp = 0;
  spegPtr->endSmp = -1;
  spegPtr->ssmp = 0;
  spegPtr->esmp = -1;
  spegPtr->id = 0;
  spegPtr->mode = CONF_WIDTH;
  spegPtr->bright = 0.0;
  spegPtr->contrast = 0.0;
  spegPtr->si.fftlen = 256;
  spegPtr->si.winlen = 128;
  spegPtr->si.spacing = 64.0;
  spegPtr->preemph = 0.97;
  spegPtr->si.frame[0] = (short *) ckalloc(2*FRAMESIZE);
  spegPtr->si.nfrms = 1;
  spegPtr->si.frlen = FRAMESIZE;
  spegPtr->si.RestartPos = 0;
  spegPtr->si.fftmax = -10000;
  spegPtr->si.fftmin = 10000;
  spegPtr->si.debug = 1;
  spegPtr->si.hamwin = (float *) ckalloc(NMAX * sizeof(float));
  spegPtr->si.BufPos = 0;
  spegPtr->si.abmax = 0.0f;
  spegPtr->si.bright = 60.0;
  spegPtr->si.contrast = 2.3;
  spegPtr->si.pixpsec = 250.0;
  spegPtr->si.gridTspacing = 0.0;
  spegPtr->si.gridFspacing = 0;
  spegPtr->channelstr = NULL;
  spegPtr->si.channel = -1;
  spegPtr->si.channelSet = -1;
  spegPtr->si.nchannels = 1;
  spegPtr->si.ncolors = 0;
  spegPtr->si.xcolor = NULL;
  spegPtr->si.pixmap = None;
  spegPtr->si.gridcolor = None;
  spegPtr->si.depth = Tk_Depth(tkwin);
  spegPtr->si.visual = Tk_Visual(tkwin);
  spegPtr->si.pixelmap = NULL;
  spegPtr->si.display = Tk_Display(tkwin);
  spegPtr->topFrequency = 0.0;
  spegPtr->si.xUnderSamp = 1.0;
  spegPtr->si.xTot = 0;
  spegPtr->infft = 0;
  spegPtr->si.doneSpeg = 0;
  spegPtr->si.validStart = 0;
  spegPtr->progressCmd = NULL;
  spegPtr->si.cmdPtr = NULL;
  spegPtr->si.windowType = SNACK_DEFAULT_SPEGWINTYPE;
  spegPtr->si.windowTypeSet = SNACK_DEFAULT_SPEGWINTYPE;
  spegPtr->windowTypeStr = NULL;
  spegPtr->interp = interp;

  /*  spegPtr->si.computing = 0;*/

  if (spegPtr->si.frame[0] == NULL) {
    Tcl_AppendResult(interp, "Couldn't allocate fft buffer!", NULL);
    return TCL_ERROR;
  }

  if (spegPtr->si.hamwin == NULL) {
    Tcl_AppendResult(interp, "Couldn't allocate analysis window buffer!",
		     NULL);
    ckfree((char *) spegPtr->si.frame[0]);
    return TCL_ERROR;
  }

  if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &spegPtr->x) != TCL_OK)
      || (Tk_CanvasGetCoord(interp, canvas, argv[1], &spegPtr->y) != TCL_OK))
    return TCL_ERROR;
  
  if (ConfigureSpectrogram(interp, canvas, itemPtr, argc-2, argv+2, 0) != TCL_OK) {
    DeleteSpectrogram(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
  }
  return TCL_OK;
}

static int
SpectrogramCoords(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
		  int argc, char **argv)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) itemPtr;
  char xc[TCL_DOUBLE_SPACE], yc[TCL_DOUBLE_SPACE]; 
 
  if (argc == 0) {
    Tcl_PrintDouble(interp, spegPtr->x, xc);
    Tcl_PrintDouble(interp, spegPtr->y, yc);
    Tcl_AppendResult(interp, xc, " ", yc, (char *) NULL);
  } else if (argc == 2) {
    if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &spegPtr->x) != TCL_OK) ||
	(Tk_CanvasGetCoord(interp, canvas, argv[1], &spegPtr->y) != TCL_OK)) {
      return TCL_ERROR;
    }
    ComputeSpectrogramBbox(canvas, spegPtr);
  } else {
    char buf[80];

    sprintf(buf, "wrong # coordinates: expected 0 or 2, got %d", argc);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);

    return TCL_ERROR;
  }
  return TCL_OK;
}

static void
UpdateSpeg(ClientData clientData, int flag)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) clientData;
  Sound *s = spegPtr->sound;
  int nfft = 0;

  if (spegPtr->si.debug > 1) Snack_WriteLogInt("  Enter UpdateSpeg", flag);

  if (spegPtr->canvas == NULL) return;

  if (flag == SNACK_DESTROY_SOUND) {
    spegPtr->sound = NULL;
    if (spegPtr->id) Snack_RemoveCallback(s, spegPtr->id);
    spegPtr->id = 0;
    return;
  }

  Tk_CanvasEventuallyRedraw(spegPtr->canvas,
			    spegPtr->header.x1, spegPtr->header.y1,
			    spegPtr->header.x2, spegPtr->header.y2);

  spegPtr->si.blocks = s->blocks;
  spegPtr->si.BufPos = s->length;
  spegPtr->si.storeType = s->storeType;

  if ((flag == SNACK_MORE_SOUND) || (spegPtr->endSmp < 0)) {
    spegPtr->esmp = spegPtr->si.BufPos - 1;
  }

  if (spegPtr->endSmp > 0) 
    spegPtr->esmp = spegPtr->endSmp;
  /*  
  if (spegPtr->endSmp < 0) 
    spegPtr->esmp = spegPtr->si.BufPos - 1;
    */
  if (spegPtr->esmp > spegPtr->si.BufPos - 1) 
    spegPtr->esmp = spegPtr->si.BufPos - 1;

  if (spegPtr->endSmp > spegPtr->si.BufPos - 1)
    spegPtr->esmp = spegPtr->si.BufPos - 1;
  
  spegPtr->ssmp = spegPtr->startSmp;
  
  if (spegPtr->ssmp > spegPtr->esmp)
    spegPtr->ssmp = spegPtr->esmp;

  spegPtr->si.channel = spegPtr->si.channelSet;
  if (spegPtr->si.nchannels == 1) {
    spegPtr->si.channel = 0;
  }

  if (flag == SNACK_NEW_SOUND) {
    spegPtr->si.samprate = s->samprate;
    spegPtr->si.encoding = s->encoding;
    spegPtr->si.nchannels = s->nchannels;
    spegPtr->si.abmax = s->abmax;
    spegPtr->si.fftmax = -10000;
    spegPtr->si.fftmin = 10000;
    spegPtr->si.RestartPos = spegPtr->ssmp;
    spegPtr->si.nfft = 0;
    spegPtr->si.xTot = 0;
  }
  
  if (spegPtr->topFrequency <= 0.0) {
    spegPtr->si.topfrequency = spegPtr->si.samprate / 2.0;
  } else if (spegPtr->topFrequency > spegPtr->si.samprate / 2.0) {
    spegPtr->si.topfrequency = spegPtr->si.samprate / 2.0;
  } else {
    spegPtr->si.topfrequency = spegPtr->topFrequency;
  }

  if (flag == SNACK_NEW_SOUND && spegPtr->mode == CONF_WIDTH) {
    spegPtr->infft = (int)((spegPtr->esmp - spegPtr->ssmp) /spegPtr->si.spacing);
    nfft = (int)(((spegPtr->esmp - spegPtr->ssmp)
		  - spegPtr->si.fftlen / 2) / spegPtr->si.spacing);
  } else if (flag == SNACK_NEW_SOUND && spegPtr->mode == CONF_PPS){
    spegPtr->infft = (int)((spegPtr->esmp - spegPtr->ssmp) / spegPtr->si.spacing);
    nfft = (int)(((spegPtr->esmp - spegPtr->ssmp)
		  - spegPtr->si.fftlen / 2) / spegPtr->si.spacing);
  } else if (flag == SNACK_NEW_SOUND && spegPtr->mode == CONF_WIDTH_PPS) {
    spegPtr->ssmp = (int) (spegPtr->esmp - spegPtr->width *
			   spegPtr->si.samprate / spegPtr->si.pixpsec);
    spegPtr->si.RestartPos = spegPtr->ssmp;
    spegPtr->infft = (int)((spegPtr->esmp - spegPtr->ssmp) / 
			   spegPtr->si.spacing);
    nfft = (int)(((spegPtr->esmp - spegPtr->ssmp)
		  - spegPtr->si.fftlen / 2) / spegPtr->si.spacing);
  } else {
    spegPtr->infft = (int)(s->length / spegPtr->si.spacing);
    nfft = (int)((s->length  - spegPtr->si.RestartPos
		  - spegPtr->si.fftlen / 2) / spegPtr->si.spacing);
  }

  spegPtr->si.validStart = s->validStart;

  if (nfft > 0) {
    int n = ComputeSpeg(&spegPtr->si, nfft);
    if (n < 0) return;
    spegPtr->si.RestartPos += (int)(n * spegPtr->si.spacing);
    
    if (spegPtr->mode == CONF_WIDTH) {
      if (spegPtr->esmp != spegPtr->ssmp) {
	spegPtr->si.pixpsec = (float) spegPtr->width * spegPtr->si.samprate /
	  (spegPtr->esmp - spegPtr->ssmp);
      }
      spegPtr->si.xUnderSamp = (float) spegPtr->infft / spegPtr->width;
      DrawSpeg(&spegPtr->si, spegPtr->si.display, spegPtr->copyGC,
	       spegPtr->width, spegPtr->height, 0, spegPtr->width, 0);
    }
    else if (spegPtr->mode == CONF_PPS) {
      spegPtr->width = (int)((spegPtr->esmp - spegPtr->ssmp) * 
			     spegPtr->si.pixpsec / spegPtr->si.samprate);
      if (spegPtr->width > 32767) {
	spegPtr->width = 32767;
      }
      if (spegPtr->si.pixmap != None) {
	Tk_FreePixmap(spegPtr->si.display, spegPtr->si.pixmap);
	spegPtr->si.pixmap = None;
      }
      if (spegPtr->si.pixmap == None) {
	if (spegPtr->width > 0) {
	  Tk_Window w = Tk_CanvasTkwin(spegPtr->canvas);
	  
	  spegPtr->oldwidth = spegPtr->width;
	  spegPtr->oldheight = spegPtr->height;
	  spegPtr->si.pixmap = Tk_GetPixmap(Tk_Display(w),
					    RootWindow(Tk_Display(w), Tk_ScreenNumber(w)),
					    spegPtr->width, spegPtr->height, Tk_Depth(w));
	}
      }
      DrawSpeg(&spegPtr->si, spegPtr->si.display, spegPtr->copyGC,
	       spegPtr->width, spegPtr->height, 0, spegPtr->width, 0);
    }
    else if (spegPtr->mode == CONF_WIDTH_PPS) {
      int virtx = (int) (spegPtr->si.nfft / spegPtr->si.xUnderSamp);
      int dx = virtx - (int)((spegPtr->si.nfft - n) / spegPtr->si.xUnderSamp);
      spegPtr->ssmp = (int) (spegPtr->esmp - spegPtr->width *
			     spegPtr->si.samprate / spegPtr->si.pixpsec);
      
      XCopyArea(spegPtr->si.display, spegPtr->si.pixmap, spegPtr->si.pixmap,
		spegPtr->copyGC, dx, 0, spegPtr->width - dx, spegPtr->height,
		0, 0);
      DrawSpeg(&spegPtr->si, spegPtr->si.display, spegPtr->copyGC,
	       spegPtr->width, spegPtr->height, spegPtr->width - dx, dx,
	       spegPtr->si.nfft - n - 1);
    }
  }

  ComputeSpectrogramBbox(spegPtr->canvas, spegPtr);
  
  Tk_CanvasEventuallyRedraw(spegPtr->canvas,
			    spegPtr->header.x1, spegPtr->header.y1,
			    spegPtr->header.x2, spegPtr->header.y2);

  if (spegPtr->si.debug > 1) {
    Snack_WriteLogInt("  Exit UpdateSpeg", spegPtr->width);
  }
}

static int
ConfigureSpectrogram(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
		     int argc, char **argv, int flags)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) itemPtr;
  Sound *s = spegPtr->sound;
  Tk_Window tkwin = Tk_CanvasTkwin(canvas);
  XGCValues gcValues;
  int doCompute = 0;
  int i, j;
  
  if (argc == 0) return TCL_OK;

  /*  if (spegPtr->si.computing) return TCL_OK;*/

  if (Tk_ConfigureWidget(interp, tkwin, configSpecs, argc,
			 (CONST84 char **)argv, 
			 (char *) spegPtr, flags) != TCL_OK) return TCL_ERROR;

  if (spegPtr->si.debug > 1) Snack_WriteLog("  Enter ConfigureSpeg\n");
  
  for (i = 0; configSpecs[i].type != TK_CONFIG_END; i++) {
    for (j = 0; j < argc; j += 2) {
      if (strncmp(argv[j], configSpecs[i].argvName, strlen(argv[j])) == 0) {
	configSpecs[i].specFlags |= TK_CONFIG_OPTION_SPECIFIED;
	break;
      }
    }
  }
  
#if defined(MAC) || defined(MAC_OSX_TCL)
  for (i = 0; i < argc; i++) {
    int l = strlen(argv[i]);
    if (l && strncmp(argv[i], "-anchor", l) == 0) {
      i++;
      if (strcmp(argv[i], "ne") == 0) {
	spegPtr->anchor = 1;
      } else if (strcmp(argv[i], "nw") == 0) {
	spegPtr->anchor = 7;
      } else if (strcmp(argv[i], "n") == 0) {
	spegPtr->anchor = 0;
      } else if (strcmp(argv[i], "e") == 0) {
	spegPtr->anchor = 2;
      } else if (strcmp(argv[i], "se") == 0) {
	spegPtr->anchor = 3;
      } else if (strcmp(argv[i], "sw") == 0) {
	spegPtr->anchor = 5;
      } else if (strcmp(argv[i], "s") == 0) {
	spegPtr->anchor = 4;
      } else if (strcmp(argv[i], "w") == 0) {
	spegPtr->anchor = 6;
      } else if (strncmp(argv[i], "center", strlen(argv[i])) == 0) {
	spegPtr->anchor = 8;
      }
      break;
    }
  }
#endif

  if (CheckFFTlen(interp, spegPtr->si.fftlen) != TCL_OK) return TCL_ERROR;

  if (CheckWinlen(interp, spegPtr->si.winlen, spegPtr->si.fftlen) != TCL_OK)
    return TCL_ERROR;

  if (OptSpecified(OPTION_SOUND)) {
    if (spegPtr->newSoundName == NULL) {
      spegPtr->sound = NULL;
      if (spegPtr->id) Snack_RemoveCallback(s, spegPtr->id);
      spegPtr->id = 0;
      spegPtr->si.BufPos = 0;
      doCompute = 1;
    } else {
      if ((s = Snack_GetSound(interp, spegPtr->newSoundName)) == NULL) {
	return TCL_ERROR;
      }
      if (s->storeType == SOUND_IN_CHANNEL) {
	Tcl_AppendResult(interp, spegPtr->newSoundName, 
			 " can not be linked to a channel", (char *) NULL);
	return TCL_ERROR;
      }
      if (s->storeType == SOUND_IN_FILE) {
	s->itemRefCnt++;
      }
      spegPtr->sound = s;
      if (spegPtr->soundName == NULL) {
	spegPtr->soundName = ckalloc(strlen(spegPtr->newSoundName)+1);
	strcpy(spegPtr->soundName, spegPtr->newSoundName);
      }
      if (strcmp(spegPtr->soundName, spegPtr->newSoundName) != 0) {
	Sound *t = Snack_GetSound(interp, spegPtr->soundName);
	ckfree(spegPtr->soundName);
	spegPtr->soundName = ckalloc(strlen(spegPtr->newSoundName)+1);
	strcpy(spegPtr->soundName, spegPtr->newSoundName);
	spegPtr->ssmp = 0;
	spegPtr->esmp = -1;
	Snack_RemoveCallback(t, spegPtr->id);
	spegPtr->id = 0;
      }
      if (!spegPtr->id) 
	spegPtr->id = Snack_AddCallback(s, UpdateSpeg, (int *)spegPtr);
      spegPtr->si.blocks = s->blocks;
      spegPtr->si.BufPos = s->length;
      spegPtr->si.samprate = s->samprate;
      spegPtr->si.encoding = s->encoding;
      spegPtr->si.nchannels = s->nchannels;
      spegPtr->si.abmax = s->abmax;
      spegPtr->si.storeType = s->storeType;
      spegPtr->si.sound = spegPtr->sound;
      doCompute = 1;
    }
  }

  spegPtr->copyGC = Tk_GetGC(tkwin, 0, &gcValues);

  spegPtr->esmp = spegPtr->endSmp;

  if (spegPtr->endSmp < 0) 
    spegPtr->esmp = spegPtr->si.BufPos - 1;

  if (spegPtr->endSmp > spegPtr->si.BufPos - 1)
    spegPtr->esmp = spegPtr->si.BufPos - 1;

  if (spegPtr->startSmp > spegPtr->endSmp && spegPtr->endSmp >= 0)
    spegPtr->startSmp = spegPtr->endSmp;

  if (spegPtr->startSmp < 0)
    spegPtr->startSmp = 0;

  spegPtr->ssmp = spegPtr->startSmp;

  if (spegPtr->ssmp > spegPtr->esmp)
    spegPtr->ssmp = spegPtr->esmp;

  spegPtr->si.preemph = (float) spegPtr->preemph;

  if (OptSpecified(OPTION_START)) {
    doCompute = 1;
  }

  if (OptSpecified(OPTION_END)) {
    doCompute = 1;
  }

  if (OptSpecified(OPTION_WINLEN)) {
    doCompute = 1;
  }

  if (OptSpecified(OPTION_FFTLEN)) {
    doCompute = 1;
  }
  
  if (OptSpecified(OPTION_PIXPSEC) && OptSpecified(OPTION_WIDTH)) {
    spegPtr->mode = CONF_WIDTH_PPS;
  }
  else if (OptSpecified(OPTION_PIXPSEC)) {
    spegPtr->mode = CONF_PPS;
  }
  else if (OptSpecified(OPTION_WIDTH)) {
    spegPtr->mode = CONF_WIDTH;
  }
  
  if (spegPtr->mode == CONF_WIDTH_PPS) {
    if (OptSpecified(OPTION_END) && !OptSpecified(OPTION_START)) {
      spegPtr->ssmp = (int) (spegPtr->esmp - spegPtr->width *
			     spegPtr->si.samprate / spegPtr->si.pixpsec);
    } else {
      spegPtr->esmp = (int) (spegPtr->ssmp + spegPtr->width *
			     spegPtr->si.samprate / spegPtr->si.pixpsec);
      if (spegPtr->esmp > spegPtr->si.BufPos - 1) {
	spegPtr->esmp = spegPtr->si.BufPos - 1;
      }
    }
    doCompute = 1;
  }
  else if (spegPtr->mode == CONF_PPS) {
    spegPtr->width = (int)((spegPtr->esmp - spegPtr->ssmp) * 
			   spegPtr->si.pixpsec / spegPtr->si.samprate);
  }
  else if (spegPtr->mode == CONF_WIDTH) {
    if (spegPtr->esmp != spegPtr->ssmp) {
      spegPtr->si.pixpsec = (float) spegPtr->width * spegPtr->si.samprate /
	(spegPtr->esmp - spegPtr->ssmp);
    }
  }

  if (spegPtr->width > 32767) {
    spegPtr->width = 32767;
    spegPtr->esmp = (int) (spegPtr->ssmp + spegPtr->width *
			   spegPtr->si.samprate / spegPtr->si.pixpsec);
  }

  if (OptSpecified(OPTION_HEIGHT)) {
  }

  if (OptSpecified(OPTION_BRIGHTNESS)) {
    if (spegPtr->bright > 100.0) {
      spegPtr->bright = 100.0;
    } else if (spegPtr->bright < -100.0) {
      spegPtr->bright = -100.0;
    }
    spegPtr->si.bright = spegPtr->bright * 0.3 + 60.0;
  }

  if (OptSpecified(OPTION_CONTRAST)) {
    if (spegPtr->contrast > 100.0) {
      spegPtr->contrast = 100.0;
    } else if (spegPtr->contrast < -100.0) {
      spegPtr->contrast = -100.0;
    }
    if (spegPtr->contrast >= 0.0) {
      spegPtr->si.contrast = 2.3 + spegPtr->contrast * 0.04;
    } else {
      spegPtr->si.contrast = 2.3 + spegPtr->contrast * 0.023;
    }
  }

  if (spegPtr->topFrequency <= 0.0) {
    spegPtr->si.topfrequency = spegPtr->si.samprate / 2.0;
  } else if (spegPtr->topFrequency > spegPtr->si.samprate / 2.0) {
    spegPtr->si.topfrequency = spegPtr->si.samprate / 2.0;
  } else {
    spegPtr->si.topfrequency = spegPtr->topFrequency;
  }

  if (OptSpecified(OPTION_CHANNEL)) {
    if (GetChannel(interp, spegPtr->channelstr, spegPtr->si.nchannels,
		   &spegPtr->si.channelSet) != TCL_OK) {
      return TCL_ERROR;
    }
    doCompute = 1;
  }
  spegPtr->si.channel = spegPtr->si.channelSet;
  if (spegPtr->si.nchannels == 1) {
    spegPtr->si.channel = 0;
  }

  /*  if (OptSpecified(OPTION_PROGRESS)) {
    spegPtr->si.cmdPtr = Tcl_NewStringObj(spegPtr->progressCmd, -1);
    Tcl_IncrRefCount(spegPtr->si.cmdPtr);
  }*/

  if (OptSpecified(OPTION_WINTYPE)) {
    if (GetWindowType(interp, spegPtr->windowTypeStr,
		      &spegPtr->si.windowTypeSet)
	!= TCL_OK) {
      return TCL_ERROR;
    }
    doCompute = 1;
  }
  spegPtr->si.windowType = spegPtr->si.windowTypeSet;

  if (doCompute) {
    int nfft, n;

    spegPtr->si.nfft = 0;
    spegPtr->si.spacing = (float)(spegPtr->si.samprate / spegPtr->si.pixpsec);
    nfft = (int)((spegPtr->esmp - spegPtr->ssmp) / spegPtr->si.spacing);
    spegPtr->si.xUnderSamp = 1.0;
    spegPtr->si.RestartPos = spegPtr->ssmp;
    spegPtr->si.fftmax = -10000;
    spegPtr->si.fftmin = 10000;
    spegPtr->si.ssmp = spegPtr->ssmp;

    n = ComputeSpeg(&spegPtr->si, nfft);

    if (n < 0) return TCL_OK;
    spegPtr->infft = nfft;
  }

  if (spegPtr->si.pixmap != None && 
      (spegPtr->width != spegPtr->oldwidth ||
       spegPtr->height != spegPtr->oldheight)) {
    Tk_FreePixmap(spegPtr->si.display, spegPtr->si.pixmap);
    spegPtr->si.pixmap = None;
  }
  if (spegPtr->si.pixmap == None) {
    if (spegPtr->width > 0 && spegPtr->height > 0) {
      spegPtr->oldwidth = spegPtr->width;
      spegPtr->oldheight = spegPtr->height;
      spegPtr->si.pixmap = Tk_GetPixmap(Tk_Display(tkwin), 
			      RootWindow(Tk_Display(tkwin), Tk_ScreenNumber(tkwin)),
			      spegPtr->width, spegPtr->height, Tk_Depth(tkwin));
    }
  }
  
  if (spegPtr->mode == CONF_WIDTH_PPS) {
    if (spegPtr->esmp != spegPtr->ssmp) {
      spegPtr->si.xUnderSamp = spegPtr->infft /
	               ((spegPtr->esmp - spegPtr->ssmp)
		       * (float) spegPtr->si.pixpsec / spegPtr->si.samprate);
    }
  }
  else if (spegPtr->mode == CONF_PPS) {
    if (spegPtr->width > 0) {
      spegPtr->si.xUnderSamp = (float) spegPtr->infft / spegPtr->width;
    }
  }
  else if (spegPtr->mode == CONF_WIDTH) {
    if (spegPtr->width > 0) {
      spegPtr->si.xUnderSamp = (float) spegPtr->infft / spegPtr->width;
    }
  }
  
  spegPtr->si.xTot = 0;
  DrawSpeg(&spegPtr->si, Tk_Display(tkwin), spegPtr->copyGC,
	   spegPtr->width, spegPtr->height, 0, spegPtr->width, 0);
  ComputeSpectrogramBbox(canvas, spegPtr);

  for (i = 0; configSpecs[i].type != TK_CONFIG_END; i++) {
    configSpecs[i].specFlags &= ~TK_CONFIG_OPTION_SPECIFIED;
  }
  
  if (spegPtr->si.debug > 1) Snack_WriteLog("  Exit ConfigureSpeg\n");

  return TCL_OK;
}

static void
DeleteSpectrogram(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) itemPtr;
  int i;

  if ((spegPtr->id) &&
      (Snack_GetSound(spegPtr->interp, spegPtr->soundName) != NULL)) {
    Snack_RemoveCallback(spegPtr->sound, spegPtr->id);
  }

  if (spegPtr->soundName != NULL) ckfree(spegPtr->soundName);

  if (spegPtr->si.hamwin != NULL) ckfree((char *) spegPtr->si.hamwin);

  for (i = 0; i < spegPtr->si.nfrms; i++) {
    ckfree((char *)spegPtr->si.frame[i]);
  }
  
  for (i = 0; i < spegPtr->si.ncolors; i++) {
    Tk_FreeColor(spegPtr->si.xcolor[i]);
  }

  if (spegPtr->si.gridcolor != NULL) Tk_FreeColor(spegPtr->si.gridcolor);

  if (spegPtr->si.pixmap != None) {
    Tk_FreePixmap(spegPtr->si.display, spegPtr->si.pixmap);
  }

  if (spegPtr->sound != NULL) {
    if (spegPtr->sound->storeType == SOUND_IN_FILE) {
      spegPtr->sound->itemRefCnt--;
    }
  }
}

static void
ComputeSpectrogramBbox(Tk_Canvas canvas, SpectrogramItem *spegPtr)
{
  int width = spegPtr->width;
  int height = spegPtr->height;
  int x = (int) (spegPtr->x + ((spegPtr->x >= 0) ? 0.5 : - 0.5));
  int y = (int) (spegPtr->y + ((spegPtr->y >= 0) ? 0.5 : - 0.5));
  
  switch (spegPtr->anchor) {
  case TK_ANCHOR_N:
    x -= width/2;
    break;
  case TK_ANCHOR_NE:
    x -= width;
    break;
  case TK_ANCHOR_E:
    x -= width;
    y -= height/2;
    break;
  case TK_ANCHOR_SE:
    x -= width;
    y -= height;
    break;
  case TK_ANCHOR_S:
    x -= width/2;
    y -= height;
    break;
  case TK_ANCHOR_SW:
    y -= height;
    break;
  case TK_ANCHOR_W:
    y -= height/2;
    break;
  case TK_ANCHOR_NW:
    break;
  case TK_ANCHOR_CENTER:
    x -= width/2;
    y -= height/2;
    break;
  }

  spegPtr->header.x1 = x;
  spegPtr->header.y1 = y;
  spegPtr->header.x2 = x + width;
  spegPtr->header.y2 = y + height;
}

static void
DisplaySpectrogram(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display, 
		   Drawable drawable, int x, int y, int width, int height)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) itemPtr;
  short drawableX, drawableY;
  int xcoord = 0;

  if (spegPtr->si.debug > 1) Snack_WriteLogInt("  Enter DisplaySpeg", width);

  if (spegPtr->width == 0 || spegPtr->height == 0) return;
  /*  if (spegPtr->si.computing) return;*/

  Tk_CanvasDrawableCoords(canvas, (double) spegPtr->header.x1,
			  (double) spegPtr->header.y1, &drawableX, &drawableY);
  
  if (x < spegPtr->header.x1) {
    xcoord = 0;
  } else {
    xcoord =  x - spegPtr->header.x1;
  }
  if (width > spegPtr->width) {
    width = spegPtr->width;
  }
  
  XCopyArea(display, spegPtr->si.pixmap, drawable, spegPtr->copyGC, xcoord, 0,
	    width, spegPtr->height, drawableX+xcoord, drawableY);

  if (spegPtr->si.debug > 1) Snack_WriteLog("  Exit DisplaySpeg\n");
}

static double
SpectrogramToPoint(Tk_Canvas canvas, Tk_Item *itemPtr, double *coordPtr)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) itemPtr;
  double dx = 0.0, dy = 0.0;
  double x1 = spegPtr->header.x1;
  double y1 = spegPtr->header.y1;
  double x2 = spegPtr->header.x2;
  double y2 = spegPtr->header.y2;
  
  if (coordPtr[0] < x1)
    dx = x1 - coordPtr[0];
  else if (coordPtr[0] > x2)
    dx = coordPtr[0] - x2;

  if (coordPtr[1] < y1)
    dy = y1 - coordPtr[1];
  else if (coordPtr[1] > y2)
    dy = coordPtr[1] - y2;
  
  return hypot(dx, dy);
}

static int
SpectrogramToArea(Tk_Canvas canvas, Tk_Item *itemPtr, double *rectPtr)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) itemPtr;

  if ((rectPtr[2] <= spegPtr->header.x1) ||
      (rectPtr[0] >= spegPtr->header.x2) ||
      (rectPtr[3] <= spegPtr->header.y1) ||
      (rectPtr[1] >= spegPtr->header.y2))
    return -1;

  if ((rectPtr[0] <= spegPtr->header.x1) &&
      (rectPtr[1] <= spegPtr->header.y1) &&
      (rectPtr[2] >= spegPtr->header.x2) &&
      (rectPtr[3] >= spegPtr->header.y2))
    return 1;
 
  return 0;
}

static void
ScaleSpectrogram(Tk_Canvas canvas, Tk_Item *itemPtr, double ox, double oy, 
		 double sx, double sy)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) itemPtr;
  
  spegPtr->x = ox + sx * (spegPtr->x - ox);
  spegPtr->y = oy + sy * (spegPtr->y - oy);

  spegPtr->width  = (int) (sx * spegPtr->width);
  spegPtr->height = (int) (sy * spegPtr->height);

  if (spegPtr->si.BufPos > 0)
    spegPtr->si.pixpsec = spegPtr->width * spegPtr->si.samprate /
                       (spegPtr->esmp - spegPtr->ssmp);

  ComputeSpectrogramBbox(canvas, spegPtr);
}

static void
TranslateSpectrogram(Tk_Canvas canvas, Tk_Item *itemPtr, double dx, double dy)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) itemPtr;
  
  spegPtr->x += dx;
  spegPtr->y += dy;
  ComputeSpectrogramBbox(canvas, spegPtr);
}

#define FFTBUF2(i) *(spegPtr->si.frame[(i)>>18] + ((i)&(FRAMESIZE-1)))

static int
SpectrogramToPS(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr,
		int prepass)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) itemPtr;
  char buffer[100];
  int x, y, i, j, noColor = 1;
  int nbins = spegPtr->si.fftlen / 2;
  int height = 2 * spegPtr->height;
  int width = 2 * spegPtr->width;
  int nfft = (int)(((spegPtr->esmp - spegPtr->ssmp)) / spegPtr->si.spacing);
  short v[NMAX + 1];
  unsigned char *imageR, *imageG, *imageB;

  /*  if (width == 0 || height == 0) return TCL_OK;*/

  for (i = 0; i < spegPtr->si.ncolors; i++) {
    if ((spegPtr->si.xcolor[i]->red != spegPtr->si.xcolor[i]->green) ||
	(spegPtr->si.xcolor[i]->red != spegPtr->si.xcolor[i]->blue)) {
      noColor = 0;
    }
  }
  if ((spegPtr->si.gridcolor->red != spegPtr->si.gridcolor->green) ||
      (spegPtr->si.gridcolor->red != spegPtr->si.gridcolor->blue)) {
    noColor = 0;
  }

  if ((imageR = (unsigned char *) ckalloc(width * height)) == NULL)
    return TCL_ERROR;
  if ((imageG = (unsigned char *) ckalloc(width * height)) == NULL)
    return TCL_ERROR;
  if ((imageB = (unsigned char *) ckalloc(width * height)) == NULL)
    return TCL_ERROR;

  for (i = 0; i < width; i++) {
    int top = (int) ((1.0 - (spegPtr->si.topfrequency / 
			     (spegPtr->si.samprate/2))) * nbins);
    float yscale = (float) (nbins - top)/ height;
    float xscale = (float) (nfft - 1) / width;
    float k = (float) (spegPtr->si.contrast * spegPtr->si.ncolors /
		       (spegPtr->si.fftmax - spegPtr->si.fftmin));
    float fx = xscale * i;
    int ix  = (int) fx;
    float deltax = fx - ix;
    int p = ix     * nbins;
    int q = (ix + 1) * nbins;
    
    for (j = 0; j < nbins; j++) {
      
      if (nfft >= width) { /* subsample in x direction */
	v[j] = (short) (k * (FFTBUF2(p) - spegPtr->si.fftmin - spegPtr->si.bright));
	p++;
      } else {             /* interpolate in x direction */
	v[j] = (short) (k * ((FFTBUF2(p) - spegPtr->si.fftmin - spegPtr->si.bright) + 
			     deltax * (FFTBUF2(q) - FFTBUF2(p))));
	p++; q++;
      }
    }
    v[nbins] = v[nbins - 1];
    for (j = 0; j < height; j++) {
      int c;
      float fy = yscale * j;
      int iy  = (int) fy;
      
      if (height <= nbins) /* subsample in y direction */
	c = v[iy];
      else                /* interpolate in y direction */
	c = (int) (v[iy] + (fy - iy) * (v[iy + 1] - v[iy]));
      
      if (c >= spegPtr->si.ncolors) {
	c = spegPtr->si.ncolors - 1;
      }
      if (c < 0) {
	c = 0;
      }

      imageR[i + width * (height - j - 1)] = spegPtr->si.xcolor[c]->red >> 8;
      imageG[i + width * (height - j - 1)] = spegPtr->si.xcolor[c]->green >> 8;
      imageB[i + width * (height - j - 1)] = spegPtr->si.xcolor[c]->blue >> 8;
    }
  }
  
  if ((spegPtr->si.gridFspacing > 0) && (spegPtr->si.gridTspacing > 0.0)) {
    float i, j;
    float di = (float) (spegPtr->si.pixpsec * spegPtr->si.gridTspacing);
    float dj = (float) ((float) height / (spegPtr->si.topfrequency /
					  (float) spegPtr->si.gridFspacing));
    int k = 0;
    
    for (j = (float) height - dj; j > 0.0; j -= dj) {
      for (i = di; i < (float) width; i += di) {
	for (k = -5; k <= 5; k++) {
	  imageR[(int) (i+k) + width * (int) j] = spegPtr->si.gridcolor->red >> 8;
	  imageG[(int) (i+k) + width * (int) j] = spegPtr->si.gridcolor->green >> 8;
	  imageB[(int) (i+k) + width * (int) j] = spegPtr->si.gridcolor->blue >> 8;
	  imageR[(int) i + width * (int) (j+k)] = spegPtr->si.gridcolor->red >> 8;
	  imageG[(int) i + width * (int) (j+k)] = spegPtr->si.gridcolor->green >> 8;
	  imageB[(int) i + width * (int) (j+k)] = spegPtr->si.gridcolor->blue >> 8;
	}
      }
    }
    
  } else if (spegPtr->si.gridFspacing > 0) {
    float i, j;
    float dj = (float) ((float) height / (spegPtr->si.topfrequency /
					  (float) spegPtr->si.gridFspacing));
    
    for (i = 0.0; i < (float) width; i++) {
      for (j = (float) height - dj; j > 0.0; j -= dj) {
	imageR[(int) i + width * (int) j] = spegPtr->si.gridcolor->red >> 8;
	imageG[(int) i + width * (int) j] = spegPtr->si.gridcolor->green >> 8;
	imageB[(int) i + width * (int) j] = spegPtr->si.gridcolor->blue >> 8;
      }
    }
  } else if (spegPtr->si.gridTspacing > 0.0) {
    float i, j;
    float di = (float) (spegPtr->si.pixpsec * spegPtr->si.gridTspacing);
    
    for (i = di; i < (float) width; i += di) {
      for (j = 0.0; j < (float) height; j++) {
	imageR[(int) i + width * (int) j] = spegPtr->si.gridcolor->red >> 8;
	imageG[(int) i + width * (int) j] = spegPtr->si.gridcolor->green >> 8;
	imageB[(int) i + width * (int) j] = spegPtr->si.gridcolor->blue >> 8;
      }
    }
  }

  Tcl_AppendResult(interp, "%% SPEG BEGIN\n", (char *) NULL);

  sprintf(buffer, "/pix %d string def\n%d %f translate\n", width,
	  spegPtr->header.x1, Tk_CanvasPsY(canvas,(double)spegPtr->header.y2));
  Tcl_AppendResult(interp, buffer, (char *) NULL);

  sprintf(buffer, "%d %d scale\n", width/2, height/2);
  Tcl_AppendResult(interp, buffer, (char *) NULL);

  sprintf(buffer, "%d %d 8\n", width, height);
  Tcl_AppendResult(interp, buffer, (char *) NULL);

  sprintf(buffer, "[%d 0 0 %d 0 %d]\n", width, -height, height);
  Tcl_AppendResult(interp, buffer, (char *) NULL);

  if (noColor) {
    Tcl_AppendResult(interp, "{currentfile pix readhexstring pop}\nimage\n",
		     (char *) NULL);
    
    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
	sprintf(buffer, "%.2x", imageR[x + width * y]);
	Tcl_AppendResult(interp, buffer, (char *) NULL);
      }
      Tcl_AppendResult(interp, "\n", (char *) NULL);
    }
  } else {
    Tcl_AppendResult(interp, "{currentfile pix readhexstring pop}\n",
		     "false 3 colorimage\n", (char *) NULL);
  
    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
	sprintf(buffer, "%.2x%.2x%.2x", imageR[x + width * y],
		imageG[x + width * y], imageB[x + width * y]);
	Tcl_AppendResult(interp, buffer, (char *) NULL);
      }
      Tcl_AppendResult(interp, "\n", (char *) NULL);
    }
  }

  Tcl_AppendResult(interp, "%% SPEG END\n", (char *) NULL);

  ckfree((char *) imageR);
  ckfree((char *) imageG);
  ckfree((char *) imageB);

  return TCL_OK;
}

#define FFTBUF(i) *(siPtr->frame[(i)>>18] + ((i)&(FRAMESIZE-1)))
#define LAGOM 16384.0f

static int
ComputeSpeg(SnackItemInfo *siPtr, int nfft)
{
  int i = 0, j;
  float spacing  = siPtr->spacing;
  int   fftlen   = siPtr->fftlen;
  int   winlen   = siPtr->winlen;
  int   fftmax   = siPtr->fftmax;
  int   fftmin   = siPtr->fftmin;
  float preemph  = siPtr->preemph;
  int RestartPos = siPtr->RestartPos - siPtr->validStart;
  int encoding   = siPtr->encoding;
  int storeType  = siPtr->storeType;
  int        ret = nfft;
  int       flag = 0;
  float g = 1.0;
  SnackLinkedFileInfo info;

  if (siPtr->debug > 2) Snack_WriteLogInt("    Enter ComputeSpeg", nfft);

  if (storeType != SOUND_IN_MEMORY) {
    if (OpenLinkedFile(siPtr->sound, &info) != TCL_OK) {
      return(0);
    }
  }

  if (winlen > fftlen)  /* should not happen */
    winlen = fftlen;

  Snack_InitFFT(fftlen);
  Snack_InitWindow(siPtr->hamwin, winlen, fftlen, siPtr->windowType);

  siPtr->doneSpeg = 0;
  /*  siPtr->computing = 1;*/

  while (siPtr->frlen <= ((nfft + siPtr->nfft) * fftlen / 2)) {
    if ((siPtr->frame[siPtr->nfrms] = (short *) ckalloc(2*FRAMESIZE)) == NULL)
      return 0;
    siPtr->frlen += FRAMESIZE;
    if (siPtr->debug > 3) {
      Snack_WriteLogInt("      Alloced frame", siPtr->nfrms);
    }
    siPtr->nfrms++;
  }

  if (siPtr->abmax > 0.0 && siPtr->abmax < LAGOM) {
    g = LAGOM / siPtr->abmax;
  }
  if (encoding == LIN8OFFSET || encoding == LIN8) {
    if (g == 1.0 && storeType != SOUND_IN_MEMORY) {
      g = 256.0;
    }
  }

  /*  if (siPtr->cmdPtr != NULL) {
    Snack_ProgressCallback(siPtr->cmdPtr, siPtr->sound->interp,
			   "Computing spectrogram", 0.0);
  }*/

  for (j = 0; j < nfft; j++) {
    if ((RestartPos + (int)(j * spacing) - fftlen / 2) >= 0 &&
	(RestartPos + (int)(j * spacing) + fftlen - winlen / 2 +
	 siPtr->nchannels) < siPtr->BufPos) {

      if (storeType == SOUND_IN_MEMORY) {
	if (siPtr->nchannels == 1 || siPtr->channel != -1) {
	  int p = (RestartPos + (int)(j * spacing) - winlen / 2) * 
	    siPtr->nchannels + siPtr->channel;
	  
	  for (i = 0; i < fftlen; i++) {
	    xfft[i] = (float) ((FSAMPLE(siPtr, p + siPtr->nchannels)
				- preemph * FSAMPLE(siPtr, p)) *
			       siPtr->hamwin[i]) * g;
	    p += siPtr->nchannels;
	  }
	  flag = 1;
	} else {
	  int c;
	  
	  for (i = 0; i < fftlen; i++) {
	    xfft[i] = 0.0;
	  }
	  for (c = 0; c < siPtr->nchannels; c++) {
	    int p = (RestartPos + (int)(j * spacing) - winlen / 2) * 
	      siPtr->nchannels + c;
	    
	    for (i = 0; i < fftlen; i++) {
	      xfft[i] += (float) ((FSAMPLE(siPtr, p + siPtr->nchannels)
				   - preemph * FSAMPLE(siPtr, p))
				  * siPtr->hamwin[i]) * g;
	      p += siPtr->nchannels;
	    }
	    flag = 1;
	  }
	  for (i = 0; i < fftlen; i++) {
	    xfft[i] /= siPtr->nchannels;
	  }
	}
      } else { /* storeType != SOUND_IN_MEMORY */
	if (siPtr->nchannels == 1 || siPtr->channel != -1) {
	  int p = (RestartPos + (int)(j * spacing) - winlen / 2) * 
	    siPtr->nchannels + siPtr->channel;
	  
	  for (i = 0; i < fftlen; i++) {
	    xfft[i] = (float) ((GetSample(&info, p + siPtr->nchannels)
				- preemph * GetSample(&info, p)) *
			       siPtr->hamwin[i]) * g;
	    p += siPtr->nchannels;
	  }
	  flag = 1;
	} else {
	  int c;
	  
	  for (i = 0; i < fftlen; i++) {
	    xfft[i] = 0.0;
	  }
	  for (c = 0; c < siPtr->nchannels; c++) {
	    int p = (RestartPos + (int)(j * spacing) - winlen / 2) * 
	      siPtr->nchannels + c;
	    
	    for (i = 0; i < fftlen; i++) {
	      xfft[i] += (float) ((GetSample(&info, p + siPtr->nchannels)
				   - preemph * GetSample(&info, p))
				  * siPtr->hamwin[i]) * g;
	      p += siPtr->nchannels;
	    }
	    flag = 1;
	  }
	  for (i = 0; i < fftlen; i++) {
	    xfft[i] /= siPtr->nchannels;
	  }
	}
      }
    } else {
      if (flag) ret--;
      for (i = 0; i < fftlen; i++) xfft[i] = (float) 0.0;
    }
    
    Snack_DBPowerSpectrum(xfft);
    
    for (i = 0; i < fftlen / 2; i++) {
      short tmp = (short) (xfft[i] +.5);
      int ind = (j + siPtr->nfft) * fftlen / 2 + i;
      /*
      if (siPtr->debug > 2 && i==0) Snack_WriteLogInt("  in", tmp);
      if (siPtr->debug > 2 && i==0) Snack_WriteLogInt("  hm", (int)xfft[i]);
      */
#if !defined(WIN)
      if (tmp == 0 && (int)xfft[i] < -200) {
	tmp = fftmin;
      }
#endif
      FFTBUF(ind) = tmp;
      if (tmp < fftmin) fftmin = tmp;
      if (tmp > fftmax) fftmax = tmp;
    }
    /*    Tcl_DoOneEvent(TCL_DONT_WAIT);*/
    if (siPtr->doneSpeg) return(-1);
    /*    if ((siPtr->cmdPtr != NULL) && ((j % 100) == (100-1))) {
      Snack_ProgressCallback(siPtr->cmdPtr, siPtr->sound->interp,
			     "Computing spectrogram", (double) j/nfft);
    }*/
  } /* for...*/
  siPtr->doneSpeg = 1;
  siPtr->fftmax = fftmax;
  siPtr->fftmin = fftmin;
  siPtr->nfft += ret;

  if (storeType != SOUND_IN_MEMORY) {
    CloseLinkedFile(&info);
  }

  /*  if (siPtr->cmdPtr != NULL) {
    Snack_ProgressCallback(siPtr->cmdPtr, siPtr->sound->interp,
			   "Computing spectrogram", 1.0);
  }
  siPtr->computing = 0;*/

  if (siPtr->debug > 2) {
    Snack_WriteLogInt("    Exit ComputeSpeg", siPtr->fftmin);
  }

  return ret;
}

#if defined(MAC) || defined(MAC_OSX_TCL)
int MacPutPixel(XImage *image, int x, int y, unsigned long pixel)
{
  /*
   * Define "NBBY" (number of bits per byte) if it's not already defined.
   */
  
#ifndef NBBY
#   define NBBY 8
#endif
  
  unsigned char *destPtr = (unsigned char *)&(image->data[(y *
	       image->bytes_per_line) + ((x * image->bits_per_pixel) / NBBY)]);
  
  if (image->bits_per_pixel == 32) {
    destPtr[0] = 0;
    destPtr[1] = (unsigned char) ((pixel >> 16) & 0xff);
    destPtr[2] = (unsigned char) ((pixel >> 8) & 0xff);
    destPtr[3] = (unsigned char) (pixel & 0xff);
  }
  return 0;
}
#endif

#define MAX_PIXELS 65536

static void
DrawSpeg(SnackItemInfo *siPtr, Display* disp, GC gc, int width, int height,
	 int drawX, int drawW, int fpos)
{
  int i, j, len, nCols, xStart = drawX, xEnd, doWidth, bytesPerLine;
  short v[NMAX + 1];
  int nbins = siPtr->fftlen / 2;
  XImage *ximage;
  unsigned char *linePtr, *bytePtr;
  unsigned long *pixelmap = siPtr->pixelmap;
  unsigned long gridpixel = siPtr->gridcolor->pixel;
  int ncolors = siPtr->ncolors;
  int depth = siPtr->depth;

  if (siPtr->debug > 2) Snack_WriteLogInt("    Enter DrawSpeg", drawW);

  if (height == 0) return;

  if (siPtr->pixelmap != NULL && siPtr->gridcolor != None) {
    siPtr->pixelmap[siPtr->ncolors] = siPtr->gridcolor->pixel;
  }

  if (siPtr->fftmax == siPtr->fftmin) siPtr->fftmax++;

  if (siPtr->nfft >= 0) {
    nCols = (MAX_PIXELS + height - 1) / height;
    if (nCols < 1) {
      nCols = 1;
    }
    if (nCols > drawW) {
      nCols = drawW;
    }

    ximage = XCreateImage(disp, siPtr->visual, depth, ZPixmap, 0,
			  (char *) NULL, nCols, height, 32, 0);
    if (ximage == NULL) return;
#if defined(MAC) || defined(MAC_OSX_TCL)
    ximage->f.put_pixel = MacPutPixel;
#endif

    if (depth >= 24) {
      len = (nCols + 3) * height * depth / 6;
    } else {
      len = (nCols + 3) * height * depth / 8;
    }

    ximage->data = ckalloc(len);

    if (ximage->data == NULL) {
      XFree((char *) ximage);
      return;
    }
    bytesPerLine = ((ximage->bits_per_pixel * nCols + 31) >> 3) & ~3;
    doWidth = drawW;

    for (; doWidth > 0; doWidth -= nCols) {
      float xscale = siPtr->xUnderSamp;
      int fftmin = siPtr->fftmin;
      double offset = siPtr->bright + fftmin;
      float k = (float) (siPtr->contrast * siPtr->ncolors /
			 (siPtr->fftmax - fftmin));
      if (nCols > doWidth) {
	nCols = doWidth;
      }
      xEnd = xStart + nCols;
      for (i = xStart; i < xEnd; i++) {
	float yscale = ((float)siPtr->topfrequency * nbins /
			(siPtr->samprate / 2)) / height; 
	float fx = xscale * i;
	int ix  = (int) fx;
	float deltax = fx - ix;
	int p = (ix + fpos) * nbins;
	int q = p + nbins;

	if (drawX > 0) {
	  p = (ix - (int)(xscale * xStart) + fpos) * nbins;
	  q = p + nbins;
	}

	if ((p / nbins) < 0 || (p / nbins) >= siPtr->nfft) {
	  for (j = 0; j < height; j++) {
#if !defined(WIN) && !defined(MAC) && !defined(MAC_OSX_TCL)
	    XPutPixel(ximage, (int) i - xStart, (int) j, pixelmap[0]);
#else
	    if (depth == 8) {
	      XPutPixel(ximage, (int) i - xStart, (int) j, 0);
	    } else {
	      XPutPixel(ximage, (int) i - xStart, (int) j, pixelmap[0]);
	    }
#endif
	  }
	  continue;
	}
	
	linePtr = (unsigned char *) (ximage->data + i - xStart + bytesPerLine * (height-1));
	
	bytePtr = linePtr;
	
	if (siPtr->nfft >= width) { /* subsample in x direction */
	  for (j = 0; j < nbins; j++) {
	    v[j] = (short) (k * (FFTBUF(p) - offset));
	    p++;
	  }
	} else {             /* interpolate in x direction */
	  for (j = 0; j < nbins; j++) {
	    short fftp = FFTBUF(p);
	    v[j] = (short) (k * ((fftp - offset) + 
				 deltax * (FFTBUF(q) - fftp)));
	    p++; q++;
	  }
	}
	v[nbins] = v[nbins - 1];
	for (j = 0; j < height; j++) {
	  int c;
	  float fy = yscale * j;
	  int iy  = (int) fy;
	  
	  if (height <= nbins) /* subsample in y direction */
	    c = v[iy];
	  else                /* interpolate in y direction */
	    c = (int) (v[iy] + (fy - iy) * (v[iy + 1] - v[iy]));

	  if (c >= ncolors) {
	    c = ncolors - 1;
	  }
	  if (c < 0) {
	    c = 0;
	  }
	  
	  switch (depth) {
	  case 8:
#if !defined(WIN) && !defined(MAC) && !defined(MAC_OSX_TCL)
	    *bytePtr = (unsigned char) pixelmap[c];
#else
	    *bytePtr = (unsigned char) c;
#endif
	    break;
	    
	  default:
	    XPutPixel(ximage, i - xStart, height - j - 1, pixelmap[c]);
	  }
	  bytePtr -= bytesPerLine;
	}
      }

      if ((siPtr->gridFspacing > 0) && (siPtr->gridTspacing > 0.0)) {
	float i, j;
	float di = (float) siPtr->pixpsec * (float) siPtr->gridTspacing;
	float dj = (height / ((float)siPtr->topfrequency / siPtr->gridFspacing));
	int k = 0;
	int xleft = width - siPtr->xTot - drawW;
	int xcoord;
	
	for (i = xleft + di; i < (float) width; i += di) {
	  for (k = -5; k <= 5; k++) {
	    if ((int)(i+k) >= xStart && (int)(i+k) < xEnd) {

              xcoord = (int) (i+k) - xStart;
	      for (j = (float) height - dj; j > 0.0; j -= dj) {
#if !defined(WIN) && !defined(MAC) && !defined(MAC_OSX_TCL)
		XPutPixel(ximage, xcoord, (int) j, gridpixel);
#else
		if (depth == 8) {
		  XPutPixel(ximage, xcoord, (int) j, ncolors);
		} else {
		  XPutPixel(ximage, xcoord, (int) j, gridpixel);
		}
#endif
	      }
	    }
	  }
	  if ((int) i >= xStart && (int) i < xEnd) {
	    for (j = (float) height - dj; j > 0.0; j -= dj) {
	      for (k = -5; k <= 5; k++) {
		if ((int)(j+k) >= 0 && (int)(j+k) < height) {
#if !defined(WIN) && !defined(MAC) && !defined(MAC_OSX_TCL)
		  XPutPixel(ximage, (int) i - xStart, (int) (j+k), gridpixel);
#else
		  if (depth == 8) {
		    XPutPixel(ximage, (int) i - xStart, (int) (j+k), ncolors);
		  } else {
		    XPutPixel(ximage, (int) i - xStart, (int) (j+k),gridpixel);
		  }
#endif
		}
	      }
	    }
	  }
	}
      } else if (siPtr->gridFspacing > 0) {
	float i, j;
	float dj = (height / ((float)siPtr->topfrequency/siPtr->gridFspacing));
	
	for (i = 0.0; i < (float) width; i++) {
	  if (i >= xStart && i < xEnd) {
	    for (j = (float) height - dj; j > 0.0; j -= dj) {
#if !defined(WIN) && !defined(MAC) && !defined(MAC_OSX_TCL)
	      XPutPixel(ximage, (int) i - xStart, (int) j, gridpixel);
#else
	      if (depth == 8) {
		XPutPixel(ximage, (int) i - xStart, (int) j, ncolors);
	      } else {
		XPutPixel(ximage, (int) i - xStart, (int) j, gridpixel);
	      }
#endif
	    }
	  }
	}
      } else if (siPtr->gridTspacing > 0.0) {
	float i, j;
	float di = (float) siPtr->pixpsec * (float) siPtr->gridTspacing;
	int xleft = width - siPtr->xTot - drawW;

	for (i = xleft + di; i < (float) width; i += di) {
	  if (i >= xStart && i < xEnd) {
	    for (j = 0.0; j < (float) height; j++) {
#if !defined(WIN) && !defined(MAC) && !defined(MAC_OSX_TCL)
	      XPutPixel(ximage, (int) i - xStart, (int) j, gridpixel);
#else
	      if (depth == 8) {
		XPutPixel(ximage, (int) i - xStart, (int) j, ncolors);
	      } else {
		XPutPixel(ximage, (int) i - xStart, (int) j, gridpixel);
	      }
#endif
	    }
	  }
	}
      }
      TkPutImage(siPtr->pixelmap, siPtr->ncolors + 1, disp, siPtr->pixmap,
		 gc, ximage, 0, 0, xStart, 0, nCols, height);
      xStart = xEnd;
    }
    ckfree(ximage->data);
    XFree((char *) ximage);
  }

  if (drawX == 0) {
    siPtr->xTot = 0;
  } else {
    siPtr->xTot += drawW;
  }

  if (siPtr->debug > 2) Snack_WriteLog("    Exit Drawspeg\n");
}

static int
ParseColorMap(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
	      CONST84 char *value, char *recordPtr, int offset)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) recordPtr;
  int argc, i;
  CONST84 char **argv = NULL;

  if (Tcl_SplitList(interp, value, &argc, &argv) != TCL_OK) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "bad color map \"", value,
		     "\": must be list with at least two colors", 
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (argc == 1) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "bad color map \"", value,
		     "\": must be list with at least two colors",
		     (char *) NULL);
    if (argv != NULL) {
      ckfree((char *) argv);
    }
    return TCL_ERROR;
  }
  
  for (i = 0; i < spegPtr->si.ncolors; i++) {
    Tk_FreeColor(spegPtr->si.xcolor[i]);
  }
  
  if (argc == 0) {
    spegPtr->si.ncolors = NDEFCOLS;
  } else {
    spegPtr->si.ncolors = argc;
  }
  
  spegPtr->si.xcolor = (XColor **) ckalloc(spegPtr->si.ncolors * sizeof(XColor*));

  if (spegPtr->si.xcolor == NULL) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "Not enough memory to allocate colormap", NULL);
    if (argv != NULL) {
      ckfree((char *) argv);
    }
    return TCL_ERROR;
  }

  spegPtr->si.pixelmap = (unsigned long *) ckalloc((spegPtr->si.ncolors + 1) * sizeof(unsigned long));

  if (spegPtr->si.pixelmap == NULL) {
    ckfree((char *) spegPtr->si.xcolor);
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "Not enough memory to allocate pixelmap", NULL);
    if (argv != NULL) {
      ckfree((char *) argv);
    }
    return TCL_ERROR;
  }

  if (argc == 0) {
    for (i = 0; i < spegPtr->si.ncolors; i++) {
      XColor xcol;
      
      xcol.flags = DoRed | DoGreen | DoBlue;
      xcol.red   = 65535 - (i * 65535 / (spegPtr->si.ncolors - 1));
      xcol.green = 65535 - (i * 65535 / (spegPtr->si.ncolors - 1));
      xcol.blue  = 65535 - (i * 65535 / (spegPtr->si.ncolors - 1));
      spegPtr->si.xcolor[i] = Tk_GetColorByValue(Tk_MainWindow(interp), &xcol);
      spegPtr->si.pixelmap[i] = spegPtr->si.xcolor[i]->pixel;
    }
  } else {
    for (i = 0; i < spegPtr->si.ncolors; i++) {
      spegPtr->si.xcolor[i] = Tk_GetColor(interp, Tk_MainWindow(interp), argv[i]);
      if (spegPtr->si.xcolor[i] == NULL) {
	ckfree((char *) spegPtr->si.xcolor);
	ckfree((char *) spegPtr->si.pixelmap);
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "unknown color name \"", argv[i],
			 "\"", (char *) NULL);
	if (argv != NULL) {
	  ckfree((char *) argv);
	}
	return TCL_ERROR;
      }
      spegPtr->si.pixelmap[i] = spegPtr->si.xcolor[i]->pixel;
    }
  }

  ckfree((char *) argv);

  return TCL_OK;
}

static char*
PrintColorMap(ClientData clientData, Tk_Window tkwin, char *recordPtr,
	      int offset, Tcl_FreeProc **freeProcPtr)
{
  SpectrogramItem *spegPtr = (SpectrogramItem *) recordPtr;
  char *buffer;
  int i, j = 0;

  *freeProcPtr = TCL_DYNAMIC;
  buffer = (char *) ckalloc(spegPtr->si.ncolors * 20);
  for (i = 0; i < spegPtr->si.ncolors; i++) {
    j += (int) sprintf(&buffer[j], "%s ", Tk_NameOfColor(spegPtr->si.xcolor[i]));
  }
  sprintf(&buffer[j], "\n");
  return buffer;
}
