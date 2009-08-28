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
#include <math.h>
#define USE_OLD_CANVAS /* To keep Tk8.3 happy */
#include "tk.h"
#include "jkCanvItems.h"
#include <string.h>

/*
 * Wave item structure
 */

typedef struct WaveItem  {

  Tk_Item header;
  Tk_Canvas canvas;
  double x, y;
  Tk_Anchor anchor;
  double *x0;
  double *y0;
  double *x1;
  double *y1;
  XColor *fg;
  Pixmap fillStipple;
  GC gc;
  char *newSoundName;
  char *soundName;
  Sound *sound;
  int channel;
  int channelSet;
  int nchannels;
  int samprate;
  int encoding;
  float **blocks;
  int bufPos;
  double limit;
  int subSample;
  double pixpsec;
  int height;
  int width;
  int widthSet;
  int startSmp;
  int endSmp;
  int ssmp;
  int esmp;
  int zeroLevel;
  int frame;
  int id;
  int mode;
  int subSampleInt;
  char *channelStr;
  int debug;
  int storeType;
  char *preCompFile;
  struct WaveItem *preWI;
  Sound *preSound;
  int preCompInvalid;
  int validStart;
  char *progressCmd;
  Tcl_Obj *cmdPtr;
  Tcl_Interp *interp;
  int trimstart;
  float maxv;
  float minv;
  int remove; /* remove for 2.1 */

} WaveItem;

Tk_CustomOption waveTagsOption = { (Tk_OptionParseProc *) NULL,
				   (Tk_OptionPrintProc *) NULL,
				   (ClientData) NULL };

typedef enum {
  OPTION_ANCHOR,
  OPTION_TAGS,
  OPTION_SOUND,
  OPTION_HEIGHT,
  OPTION_WIDTH,
  OPTION_PIXPSEC,
  OPTION_START,
  OPTION_END,
  OPTION_FILL,
  OPTION_STIPPLE,
  OPTION_ZEROLEVEL,
  OPTION_FRAME,
  OPTION_LIMIT,
  OPTION_SUBSAMPLE,
  OPTION_CHANNEL,
  OPTION_PRECOMPWAVE,
  OPTION_PROGRESS,
  OPTION_TRIMSTART
} ConfigSpec;

static Tk_ConfigSpec configSpecs[] = {

  {TK_CONFIG_ANCHOR, "-anchor", (char *) NULL, (char *) NULL,
   "nw", Tk_Offset(WaveItem, anchor), TK_CONFIG_DONT_SET_DEFAULT},

  {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
   (char *) NULL, 0, TK_CONFIG_NULL_OK, &waveTagsOption},
  
  {TK_CONFIG_STRING, "-sound", (char *) NULL, (char *) NULL,
   "", Tk_Offset(WaveItem, newSoundName), TK_CONFIG_NULL_OK},
  
  {TK_CONFIG_INT, "-height", (char *) NULL, (char *) NULL,
   "100", Tk_Offset(WaveItem, height), 0},
  
  {TK_CONFIG_PIXELS, "-width", (char *) NULL, (char *) NULL,
   "378", Tk_Offset(WaveItem, widthSet), 0},
  
  {TK_CONFIG_DOUBLE, "-pixelspersecond", "pps", (char *) NULL,
   "250.0", Tk_Offset(WaveItem, pixpsec), 0},

  {TK_CONFIG_INT, "-start", (char *) NULL, (char *) NULL,
   "0", Tk_Offset(WaveItem, startSmp), 0},
  
  {TK_CONFIG_INT, "-end", (char *) NULL, (char *) NULL,
   "-1", Tk_Offset(WaveItem, endSmp), 0},
  
  {TK_CONFIG_COLOR, "-fill", (char *) NULL, (char *) NULL,
   "black", Tk_Offset(WaveItem, fg), TK_CONFIG_NULL_OK},
  
  {TK_CONFIG_BITMAP, "-stipple", (char *) NULL, (char *) NULL,
   (char *) NULL, Tk_Offset(WaveItem, fillStipple), TK_CONFIG_NULL_OK},

  {TK_CONFIG_BOOLEAN, "-zerolevel", "zerolevel", (char *) NULL,
   "yes", Tk_Offset(WaveItem, zeroLevel), TK_CONFIG_NULL_OK},

  {TK_CONFIG_BOOLEAN, "-frame", (char *) NULL, (char *) NULL,
   "no", Tk_Offset(WaveItem, frame), TK_CONFIG_NULL_OK},

  {TK_CONFIG_DOUBLE, "-limit", (char *) NULL, (char *) NULL,
   "-1.0", Tk_Offset(WaveItem, limit), 0},
  
  {TK_CONFIG_INT, "-subsample", (char *) NULL, (char *) NULL,
   "1", Tk_Offset(WaveItem, subSampleInt), TK_CONFIG_NULL_OK},

  {TK_CONFIG_STRING, "-channel", (char *) NULL, (char *) NULL,
   "-1", Tk_Offset(WaveItem, channelStr), TK_CONFIG_NULL_OK},

  {TK_CONFIG_STRING, "-shapefile", (char *) NULL, (char *) NULL,
   "", Tk_Offset(WaveItem, preCompFile), TK_CONFIG_NULL_OK},

  {TK_CONFIG_STRING, "-progress", (char *) NULL, (char *) NULL,
   "", Tk_Offset(WaveItem, progressCmd), TK_CONFIG_NULL_OK},

  {TK_CONFIG_INT, "-trimstart", (char *) NULL, (char *) NULL,
   "0", Tk_Offset(WaveItem, trimstart), 0},

  /* To be removed for 2.1 */
  {TK_CONFIG_INT, "-tround", (char *) NULL, (char *) NULL,
   "0", Tk_Offset(WaveItem, remove), 0},

  {TK_CONFIG_INT, "-debug", (char *) NULL, (char *) NULL,
   "0", Tk_Offset(WaveItem, debug), 0},

  {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
   (char *) NULL, 0, 0}

};

/*
 * Protos
 */

  static void   ComputeWaveBbox(Tk_Canvas canvas, WaveItem *wavePtr);

  static int    ComputeWaveCoords(Tk_Item *itemPtr);
  
  static int    ConfigureWave(Tcl_Interp *interp, Tk_Canvas canvas, 
			      Tk_Item *itemPtr, int argc,
			      char **argv, int flags);

  static int    CreateWave(Tcl_Interp *interp, Tk_Canvas canvas,
			   struct Tk_Item *itemPtr,
			   int argc, char **argv);

  static void   DeleteWave(Tk_Canvas canvas, Tk_Item *itemPtr,
			   Display *display);

  static void   DisplayWave(Tk_Canvas canvas, Tk_Item *itemPtr,
			    Display *display, Drawable dst,
			    int x, int y, int width, int height);

  static void   ScaleWave(Tk_Canvas canvas, Tk_Item *itemPtr,
			  double originX, double originY,
			  double scaleX, double scaleY);

  static void   TranslateWave(Tk_Canvas canvas, Tk_Item *itemPtr,
			      double deltaX, double deltaY);
  
  static int    WaveCoords(Tcl_Interp *interp, Tk_Canvas canvas,
			   Tk_Item *itemPtr, int argc, char **argv);
  
  static int    WaveToArea(Tk_Canvas canvas, Tk_Item *itemPtr,
			   double *rectPtr);
  
  static double WaveToPoint(Tk_Canvas canvas, Tk_Item *itemPtr,
			    double *coords);
  
  static int    WaveToPS(Tcl_Interp *interp, Tk_Canvas canvas,
			 Tk_Item *itemPtr, int prepass);

/*
 * Wave item type
 */

Tk_ItemType snackWaveType = {
  "waveform",
  sizeof(WaveItem),
  CreateWave,
  configSpecs,
  ConfigureWave,
  WaveCoords,
  DeleteWave,
  DisplayWave,
  0,
  WaveToPoint,
  WaveToArea,
  WaveToPS,
  ScaleWave,
  TranslateWave,
  (Tk_ItemIndexProc *) NULL,
  (Tk_ItemCursorProc *) NULL,
  (Tk_ItemSelectionProc *) NULL,
  (Tk_ItemInsertProc *) NULL,
  (Tk_ItemDCharsProc *) NULL,
  (Tk_ItemType *) NULL
};

static int
CreateWave(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr,
	   int argc, char **argv)
{
  WaveItem *wavePtr = (WaveItem *) itemPtr;

  if (argc < 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     Tk_PathName(Tk_CanvasTkwin(canvas)), " create ",
		     itemPtr->typePtr->name, " x y ?opts?\"", (char *) NULL);
    return TCL_ERROR;
  }

  wavePtr->canvas = canvas;
  wavePtr->anchor = TK_ANCHOR_NW;
  wavePtr->x0 = NULL;
  wavePtr->y0 = NULL;
  wavePtr->x1 = NULL;
  wavePtr->y1 = NULL;
  wavePtr->fg = None;
  wavePtr->fillStipple = None;
  wavePtr->gc = None;
  wavePtr->newSoundName = NULL;
  wavePtr->soundName = NULL;
  wavePtr->sound = NULL;
  wavePtr->pixpsec = 250.0;
  wavePtr->height = 100;
  wavePtr->width = -1;
  wavePtr->widthSet = 378;
  wavePtr->startSmp = 0;
  wavePtr->endSmp = -1;
  wavePtr->ssmp = 0;
  wavePtr->esmp = -1;
  wavePtr->id = 0;
  wavePtr->mode = CONF_WIDTH;
  wavePtr->zeroLevel = 1;
  wavePtr->frame = 0;
  wavePtr->channelStr = NULL;
  wavePtr->channel = -1;
  wavePtr->channelSet = -1;
  wavePtr->nchannels = 1;
  wavePtr->samprate = 16000;
  wavePtr->encoding = LIN16;
  wavePtr->bufPos = 0;
  wavePtr->limit = -1.0;
  wavePtr->subSampleInt = 1;
  wavePtr->subSample = 1;
  wavePtr->preCompFile = NULL;
  wavePtr->preSound = NULL;
  wavePtr->preWI = NULL;
  wavePtr->preCompInvalid = 0;
  wavePtr->validStart = 0;
  wavePtr->progressCmd = NULL;
  wavePtr->cmdPtr = NULL;
  wavePtr->interp = interp;
  wavePtr->trimstart = 0;
  wavePtr->maxv = 0.0f;
  wavePtr->minv = 0.0f;
  wavePtr->debug = 0;
  wavePtr->x = 0;
  wavePtr->y = 0;

  if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &wavePtr->x) != TCL_OK) ||
      (Tk_CanvasGetCoord(interp, canvas, argv[1], &wavePtr->y) != TCL_OK))
    return TCL_ERROR;
  
  if (ConfigureWave(interp, canvas, itemPtr, argc-2, argv+2, 0) == TCL_OK)
    return TCL_OK;

  DeleteWave(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
  return TCL_ERROR;
}

static void
WaveMaxMin(WaveItem *wavePtr, SnackLinkedFileInfo *info, int start, int stop,
	   float *maxi, float *mini)
{
  int i, j, allFlag = 0;
  float maxval = -8388608.0, minval = 8388607.0, val;
  int nchan = wavePtr->nchannels, chan = wavePtr->channel;
  int inc = nchan * wavePtr->subSample;

  if (start < 0 || stop > wavePtr->bufPos - 1 || stop == 0 ||
      (wavePtr->blocks[0] == NULL && wavePtr->storeType == SOUND_IN_MEMORY)) {
    if (wavePtr->encoding == LIN8OFFSET) {
      *maxi = 128.0;
      *mini = 128.0;
    } else {
      *maxi = 0.0;
      *mini = 0.0;
    }
    return;
  }
  if (chan == -1) {
    allFlag = 1;
    chan = 0;
  }

  start = start * wavePtr->nchannels + chan;
  stop  = stop  * wavePtr->nchannels + chan + wavePtr->nchannels - 1;

  for (i = start; i <= stop; i += inc) {
    if (wavePtr->storeType == SOUND_IN_MEMORY) {
      val = FSAMPLE(wavePtr, i);
      if (allFlag) {
	for (j = 1; j < nchan; j++) {
	  val += FSAMPLE(wavePtr, i + j);
	}
	val = val / nchan;
      }
    } else {
      val = GetSample(info, i);	
      if (allFlag) {
	for (j = 1; j < nchan; j++) {
	  val += GetSample(info, i + j);
	}
	val = val / nchan;
      }
    }
    if (val > maxval) {
      maxval = val;
    }
    if (val < minval) {
      minval = val;
    }
  }
  if (wavePtr->limit > 0.0) {
    if (maxval > wavePtr->limit) {
      maxval = (float) wavePtr->limit;
    }
    if (minval < -wavePtr->limit) {
      minval = (float) -wavePtr->limit;
    }
  }
  *maxi = maxval;
  *mini = minval;
}

static int
WaveCoords(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
	   char **argv)
{
  WaveItem *wPtr = (WaveItem *) itemPtr;
  char xc[TCL_DOUBLE_SPACE], yc[TCL_DOUBLE_SPACE];

  if (argc == 0) {
    Tcl_PrintDouble(interp, wPtr->x, xc);
    Tcl_PrintDouble(interp, wPtr->y, yc);
    Tcl_AppendResult(interp, xc, " ", yc, (char *) NULL);
  } else if (argc == 2) {
    if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &wPtr->x) != TCL_OK) ||
	(Tk_CanvasGetCoord(interp, canvas, argv[1], &wPtr->y) != TCL_OK)) {
      return TCL_ERROR;
    }
    ComputeWaveBbox(canvas, wPtr);
  } else {
    char buf[80];

    sprintf(buf, "wrong # coordinates: expected 0 or 2, got %d", argc);
    Tcl_SetResult(interp, buf, TCL_VOLATILE);

    return TCL_ERROR;
  }

  return TCL_OK;
}

/*#define WIDEWAVE 100000*/

static int
ComputeWaveCoords(Tk_Item *itemPtr)
{
  WaveItem *wavePtr = (WaveItem *) itemPtr;
  int i = 0;
  float maxv, minv, abmax;
  int yh = wavePtr->height / 2;
  int nPoints = wavePtr->width;
  SnackLinkedFileInfo info;
  Tcl_Interp *interp;
  int usePre = 0;

  if (wavePtr->debug > 1) Snack_WriteLog("  Enter ComputeWaveCoords\n"); 

  if (wavePtr->x0 != NULL) {
    ckfree((char *) wavePtr->x0);
  }
  if (wavePtr->y0 != NULL) {
    ckfree((char *) wavePtr->y0);
  }
  if (wavePtr->x1 != NULL) {
    ckfree((char *) wavePtr->x1);
  }
  if (wavePtr->y1 != NULL) {
    ckfree((char *) wavePtr->y1);
  }
  wavePtr->x0 = (double *) ckalloc(sizeof(double) * nPoints);
  wavePtr->y0 = (double *) ckalloc(sizeof(double) * nPoints);
  wavePtr->x1 = (double *) ckalloc(sizeof(double) * nPoints);
  wavePtr->y1 = (double *) ckalloc(sizeof(double) * nPoints);

  if (wavePtr->sound == NULL) {
    for (i = 0; i < nPoints; i++) {
      wavePtr->x0[i] = (double) i;
      wavePtr->y0[i] = (double) yh;
      wavePtr->x1[i] = (double) i;
      wavePtr->y1[i] = (double) yh;
    }
    return TCL_OK;
  }

  maxv = wavePtr->sound->maxsamp;
  minv = wavePtr->sound->minsamp;
  abmax = wavePtr->sound->abmax;
  interp = wavePtr->sound->interp;

  if (wavePtr->preCompFile != NULL && wavePtr->sound->readStatus != READ) {
    char *type = NULL;

    if (wavePtr->preSound != NULL) {
      wavePtr->preSound->fcname = NULL;
      Snack_DeleteSound(wavePtr->preSound);
    }
    wavePtr->preSound = Snack_NewSound(200, LIN8, wavePtr->sound->nchannels);
    if (wavePtr->preSound != NULL) {
      wavePtr->preSound->fcname = wavePtr->preCompFile;
      type = LoadSound(wavePtr->preSound, interp, NULL, 0, -1);
      if (wavePtr->preWI != NULL) ckfree((char *)wavePtr->preWI);
      wavePtr->preWI = (WaveItem *) ckalloc(sizeof(WaveItem));
      if (wavePtr->preWI != NULL) {
	wavePtr->preWI->nchannels = wavePtr->preSound->nchannels;
	wavePtr->preWI->channel = 0;
	wavePtr->preWI->subSample = 1;
	wavePtr->preWI->bufPos = wavePtr->preSound->length;
	wavePtr->preWI->blocks = wavePtr->preSound->blocks;
	wavePtr->preWI->storeType = SOUND_IN_MEMORY;
	wavePtr->preWI->encoding = LIN8;
	wavePtr->preWI->limit = wavePtr->limit;
      }
    }

    if ((type == NULL || wavePtr->preCompInvalid) && wavePtr->preSound!=NULL) {

      /* Compute and store wave */

      int nStore = (int) (200.0 * wavePtr->sound->length 
			  / wavePtr->sound->samprate);
      int j;
      int tmp = wavePtr->channel;
      
      maxv = 0.0f;
      minv = 0.0f;
      if (wavePtr->debug > 2) Snack_WriteLog("    Saving computed waveform\n");
      wavePtr->preCompInvalid = 0;
      Snack_ResizeSoundStorage(wavePtr->preSound, nStore);
      wavePtr->preSound->length = nStore;
      if (wavePtr->cmdPtr != NULL) {
	Snack_ProgressCallback(wavePtr->cmdPtr, interp,
			       "Computing waveform", 0.0);
      }

      if (wavePtr->storeType != SOUND_IN_MEMORY) {
	if (OpenLinkedFile(wavePtr->sound, &info) != TCL_OK) {
	  for (i = 0; i < nPoints; i++) {
	    wavePtr->x0[i] = (double) i;
	    wavePtr->y0[i] = (double) yh;
	    wavePtr->x1[i] = (double) i;
	    wavePtr->y1[i] = (double) yh;
	  }
	  if (wavePtr->cmdPtr != NULL) {
	    Snack_ProgressCallback(wavePtr->cmdPtr, interp,
				   "Computing waveform", 1.0);
	  }
	  return TCL_OK;
	}
      }
      for (i = 0; i < nStore / 2; i++) {
	for (j = 0; j < wavePtr->sound->nchannels; j++) {
	  float fraq = (float) wavePtr->sound->length / (nStore / 2);
	  int start = (int) (i     * fraq);
	  int stop  = (int) ((i+1) * fraq);
	  float wtop, wbot;

	  wavePtr->channel = j;
	  WaveMaxMin(wavePtr, &info, start, stop, &wtop, &wbot);
	  
	  if (maxv < wtop) maxv = wtop;
	  if (minv > wbot) minv = wbot;
	  
	  switch (wavePtr->encoding) {
	  case LIN16:
	  case MULAW:
	  case ALAW:
	    wtop = wtop / 256.0f;
	    wbot = wbot / 256.0f;
	    break;
	  case LIN24:
	    wtop = wtop / 65536.0f;
	    wbot = wbot / 65536.0f;
	    break;
	  case LIN32:
	    wtop = wtop / 16777216.0f;
	    wbot = wbot / 16777216.0f;
	    break;
	  case SNACK_FLOAT:
	    wtop = (wtop / abmax) * 128.0f;
	    wbot = (wbot / abmax) * 128.0f;
	    break;
	  case LIN8OFFSET:
	    wtop -= 128.0f;
	    wbot -= 128.0f;
	    break;
	  case LIN8:
	    break;
	  }
	  Snack_SetSample(wavePtr->preSound, j, i*2,   (char) wtop);
	  Snack_SetSample(wavePtr->preSound, j, i*2+1, (char) wbot);
	  if (j == 0 && (wavePtr->cmdPtr != NULL) && ((i % 1000) == 999)) {
	    int res = Snack_ProgressCallback(wavePtr->cmdPtr, interp,
			     "Computing waveform", (double) i/(nStore/2));
	    if (res != TCL_OK) {
	      if (wavePtr->debug > 2) {
		Snack_WriteLog("    Aborting ComputeWaveCoords\n"); 
	      }
	      for (;i < nStore / 2; i++) {
		for (j = 0; j < wavePtr->sound->nchannels; j++) {
		  Snack_SetSample(wavePtr->preSound, j, i*2,   (char) 0);
		  Snack_SetSample(wavePtr->preSound, j, i*2+1, (char) 0);
		}
	      }
	      break;
	    }
	  }
	}
      }
      if (wavePtr->cmdPtr != NULL) {
	Snack_ProgressCallback(wavePtr->cmdPtr, interp,
			       "Computing waveform", 1.0);
      }
      if (SaveSound(wavePtr->preSound, interp, wavePtr->preCompFile, NULL,
		    0, NULL, 0, wavePtr->preSound->length, AIFF_STRING) ==
	  TCL_ERROR) {
	if (wavePtr->debug > 2) Snack_WriteLog("    Failed saving waveform\n");
	wavePtr->preCompFile = NULL;
      }
      wavePtr->preWI->bufPos = wavePtr->preSound->length;
      wavePtr->preWI->blocks = wavePtr->preSound->blocks;
      if (wavePtr->storeType != SOUND_IN_MEMORY) {
	CloseLinkedFile(&info);
      }
      wavePtr->channel = tmp;
    }

    if (wavePtr->preSound != NULL && wavePtr->preWI != NULL) {

      /* Use precomputed wave */

      float left  = ((float) wavePtr->ssmp / wavePtr->sound->length) *
	wavePtr->preSound->length;
      float right = ((float) wavePtr->esmp / wavePtr->sound->length) *
	wavePtr->preSound->length;
      float fraq  = (right - left) / (nPoints * 2);

      if (fraq > 1.0) {
	usePre = 1;
	switch (wavePtr->encoding) {
	case LIN16:
	case MULAW:
	case ALAW:
	  maxv = maxv / 256.0f;
	  minv = minv / 256.0f;
	  break;
	case LIN24:
	  maxv = maxv / 65536.0f;
	  minv = minv / 65536.0f;
	  break;
	case LIN32:
	  maxv = maxv / 16777216.0f;
	  minv = minv / 16777216.0f;
	  break;
	case SNACK_FLOAT:
	  maxv = (maxv / abmax) * 128.0f;
	  minv = (minv / abmax) * 128.0f;
	  break;
	case LIN8OFFSET:
	  maxv -= 128.0f;
	  minv -= 128.0f;
	  break;
	case LIN8:
	  break;
	}

	if (wavePtr->debug > 2) {
	  Snack_WriteLog("    Using precomputed waveform\n");
	}

	wavePtr->preWI->channel = wavePtr->channel;
	for (i = 0; i < nPoints; i++) {
	  int start = (int) (left + 2*(i * fraq));
	  int stop  = (int) (left + 2*(i+1)*fraq);
	  float wtop, wbot;

	  WaveMaxMin(wavePtr->preWI, NULL, start, stop, &wtop, &wbot);

	  if (maxv < wtop) maxv = wtop;
	  if (minv > wbot) minv = wbot;

	  wavePtr->x0[i] = i;
	  wavePtr->x1[i] = i;
	  if (i > 0 && wavePtr->y1[i-1] <= wtop) {
	    wavePtr->y0[i] = wtop;
	    wavePtr->y1[i] = wbot;
	  } else {
	    wavePtr->y0[i] = wbot;
	    wavePtr->y1[i] = wtop;
	  }
	}

	if (wavePtr->encoding == LIN8OFFSET) {
	  maxv += 128.0f;
	  minv += 128.0f;
	}
      } else {
	usePre = 0;
      }
    }
  }

  if (!usePre) {

    if (wavePtr->debug > 2) {
      Snack_WriteLog("    Default waveform computation\n");
    }

    if (wavePtr->storeType != SOUND_IN_MEMORY) {
      if (OpenLinkedFile(wavePtr->sound, &info) != TCL_OK) {
	for (i = 0; i < nPoints; i++) {
	  wavePtr->x0[i] = (double) i;
	  wavePtr->y0[i] = (double) yh;
	  wavePtr->x1[i] = (double) i;
	  wavePtr->y1[i] = (double) yh;
	}
	  return TCL_OK;
      }
    }

    for (i = 0; i < nPoints; i++) {
      float fraq = (float) (wavePtr->esmp - wavePtr->ssmp) / nPoints;
      int start = wavePtr->ssmp + (int) (i     * fraq) - wavePtr->validStart;
      int stop  = wavePtr->ssmp + (int) ((i+1) * fraq) - wavePtr->validStart;
      float wtop, wbot;

      if (wavePtr->trimstart == 1) {
	start = (int)(wavePtr->subSample*ceil((float)start/wavePtr->subSample));
      }

      WaveMaxMin(wavePtr, &info, start, stop, &wtop, &wbot);

      if (maxv < wtop) maxv = wtop;
      if (minv > wbot) minv = wbot;

      if (wavePtr->encoding == LIN8OFFSET) {
	wtop -= 128.0f;
	wbot -= 128.0f;
      }

      wavePtr->x0[i] = i;
      wavePtr->x1[i] = i;
      if (i > 0 && wavePtr->y1[i-1] <= wtop) {
	wavePtr->y0[i] = wtop;
	wavePtr->y1[i] = wbot;
      } else {
	wavePtr->y0[i] = wbot;
	wavePtr->y1[i] = wtop;
      }
    }
    if (wavePtr->storeType != SOUND_IN_MEMORY) {
      CloseLinkedFile(&info);
    }
  }

  if (maxv > wavePtr->sound->maxsamp) {
    wavePtr->sound->maxsamp = maxv;
  }
  if (minv < wavePtr->sound->minsamp) {
    wavePtr->sound->minsamp = minv;
  }

  if (wavePtr->limit > 0) {
    maxv = (float) wavePtr->limit;
    minv = (float) -wavePtr->limit;
  }
  if (wavePtr->encoding == LIN8OFFSET) {
    maxv -= 128.0f;
    minv -= 128.0f;
  }

  wavePtr->maxv = maxv;
  wavePtr->minv = minv;

  ComputeWaveBbox(wavePtr->canvas, wavePtr);

  if (usePre) {
    switch (wavePtr->encoding) {
    case LIN16:
    case MULAW:
    case ALAW:
      maxv = maxv * 256.0f;
      minv = minv * 256.0f;
      break;
    case LIN24:
      maxv = maxv * 65536.0f;
      minv = minv * 65536.0f;
      break;
    case LIN32:
      maxv = maxv / 16777216.0f;
      minv = minv / 16777216.0f;
      break;
    case SNACK_FLOAT:
      maxv = maxv / 128.0f;
      minv = minv / 128.0f;
      break;
    case LIN8OFFSET:
      maxv += 128.0f;
      minv += 128.0f;
      break;
    case LIN8:
      break;
    }
    if (maxv > wavePtr->sound->maxsamp) {
      wavePtr->sound->maxsamp = maxv;
    }
    if (minv < wavePtr->sound->minsamp) {
      wavePtr->sound->minsamp = minv;
    }
  }

  if (wavePtr->debug > 1) Snack_WriteLog("  Exit ComputeWaveCoords\n"); 

  return TCL_OK;
}

#define NSAMPLES 100000

static void
UpdateWave(ClientData clientData, int flag)
{
  WaveItem *wavePtr = (WaveItem *) clientData;
  Sound *s = wavePtr->sound;

  if (wavePtr->debug > 1) Snack_WriteLogInt("  Enter UpdateWave", flag);

  if (wavePtr->canvas == NULL || wavePtr->sound == NULL) return;

  if (flag == SNACK_DESTROY_SOUND) {
    wavePtr->sound = NULL;
    if (wavePtr->id) Snack_RemoveCallback(s, wavePtr->id);
    wavePtr->id = 0;
    return;
  }

  Tk_CanvasEventuallyRedraw(wavePtr->canvas,
			    wavePtr->header.x1, wavePtr->header.y1, 
			    wavePtr->header.x2, wavePtr->header.y2);

  wavePtr->blocks = s->blocks;
  wavePtr->bufPos = s->length;
  wavePtr->storeType = s->storeType;

  if ((flag == SNACK_MORE_SOUND) || (wavePtr->endSmp < 0)) {
    wavePtr->esmp = wavePtr->bufPos - 1;
  }
  
  if (wavePtr->esmp > wavePtr->bufPos - 1)
    wavePtr->esmp = wavePtr->bufPos - 1;
  
  if (wavePtr->endSmp > 0) 
    wavePtr->esmp = wavePtr->endSmp;
  
  if (wavePtr->endSmp > wavePtr->bufPos - 1)
    wavePtr->esmp = wavePtr->bufPos - 1;
  
  wavePtr->ssmp = wavePtr->startSmp;
  
  if (wavePtr->ssmp > wavePtr->esmp)
    wavePtr->ssmp = wavePtr->esmp;

  wavePtr->samprate = s->samprate;
  wavePtr->encoding = s->encoding;
  wavePtr->nchannels = s->nchannels;

  wavePtr->channel = wavePtr->channelSet;
  if (wavePtr->nchannels == 1) {
    wavePtr->channel = 0;
  }

  if (wavePtr->mode == CONF_WIDTH) {
    if (wavePtr->esmp != wavePtr->ssmp) {
      wavePtr->pixpsec = (double) wavePtr->width * wavePtr->samprate /
	(wavePtr->esmp - wavePtr->ssmp);
    }
  }
  else if (wavePtr->mode == CONF_PPS) {

    wavePtr->width = (int)((wavePtr->esmp - wavePtr->ssmp) * 
			   wavePtr->pixpsec / wavePtr->samprate/* + 0.5*/);
  } 
  else if (wavePtr->mode == CONF_WIDTH_PPS) {
    wavePtr->ssmp = (int) (wavePtr->esmp - wavePtr->width *
			   wavePtr->samprate / wavePtr->pixpsec);
  }

  if (wavePtr->subSampleInt == 0) {
    if (wavePtr->esmp - wavePtr->ssmp > NSAMPLES) { 
      wavePtr->subSample = (wavePtr->esmp - wavePtr->ssmp) / NSAMPLES;
    } else {
      wavePtr->subSample = 1;
    }
  } else {
    wavePtr->subSample = wavePtr->subSampleInt;
  }

  wavePtr->preCompInvalid = 1;
  wavePtr->validStart = s->validStart;

  if (ComputeWaveCoords((Tk_Item *)wavePtr) != TCL_OK) {
    return;
  }
  Tk_CanvasEventuallyRedraw(wavePtr->canvas,
			    wavePtr->header.x1, wavePtr->header.y1,
			    wavePtr->header.x2, wavePtr->header.y2);

  if (wavePtr->debug > 1) {
    Snack_WriteLogInt("  Exit UpdateWave", wavePtr->width);
  }
}

static int
ConfigureWave(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
	      int argc, char **argv, int flags)
{
  WaveItem *wavePtr = (WaveItem *) itemPtr;
  Sound *s = wavePtr->sound;
  Tk_Window tkwin = Tk_CanvasTkwin(canvas);
  XGCValues gcValues;
  GC newGC;
  unsigned long mask;
  int doCompute = 0, oldMode;
  int i, j;

  if (argc == 0) return TCL_OK;

  if (Tk_ConfigureWidget(interp, tkwin, configSpecs, argc,
			 (CONST84 char **)argv,
			 (char *) wavePtr, flags) != TCL_OK) return TCL_ERROR;

  if (wavePtr->debug > 1) Snack_WriteLog("  Enter ConfigureWave\n");

  for (i = 0; configSpecs[i].type != TK_CONFIG_END; i++) {
    for (j = 0; j < argc; j += 2) {
      if (strncmp(argv[j], configSpecs[i].argvName, strlen(argv[j])) == 0) {
	configSpecs[i].specFlags |= TK_CONFIG_OPTION_SPECIFIED;
	break;
      }
    }
  }

#if defined(MAC)
  for (i = 0; i < argc; i++) {
    if (strncmp(argv[i], "-anchor", strlen(argv[i])) == 0) {
      i++;
      if (strcmp(argv[i], "ne") == 0) {
	wavePtr->anchor = 1;
      } else if (strcmp(argv[i], "nw") == 0) {
	wavePtr->anchor = 7;
      } else if (strcmp(argv[i], "n") == 0) {
	wavePtr->anchor = 0;
      } else if (strcmp(argv[i], "e") == 0) {
	wavePtr->anchor = 2;
      } else if (strcmp(argv[i], "se") == 0) {
	wavePtr->anchor = 3;
      } else if (strcmp(argv[i], "sw") == 0) {
	wavePtr->anchor = 5;
      } else if (strcmp(argv[i], "s") == 0) {
	wavePtr->anchor = 4;
      } else if (strcmp(argv[i], "w") == 0) {
	wavePtr->anchor = 6;
      } else if (strncmp(argv[i], "center", strlen(argv[i])) == 0) {
	wavePtr->anchor = 8;
      }
      break;
    }
  }
#endif

  if (OptSpecified(OPTION_SOUND)) {
    if (wavePtr->newSoundName == NULL) {
      wavePtr->sound = NULL;
      if (wavePtr->id) Snack_RemoveCallback(s, wavePtr->id);
      wavePtr->id = 0;
    } else {
      if ((s = Snack_GetSound(interp, wavePtr->newSoundName)) == NULL) {
	return TCL_ERROR;
      }
      if (s->storeType == SOUND_IN_CHANNEL) {
	Tcl_AppendResult(interp, wavePtr->newSoundName, 
			 " can not be linked to a channel", (char *) NULL);
	return TCL_ERROR;
      }
      if (s->storeType == SOUND_IN_FILE) {
	s->itemRefCnt++;
      }
      wavePtr->sound = s;
      if (wavePtr->soundName == NULL) {
	wavePtr->soundName = ckalloc(strlen(wavePtr->newSoundName)+1);
	strcpy(wavePtr->soundName, wavePtr->newSoundName);
      }
      if (strcmp(wavePtr->soundName, wavePtr->newSoundName) != 0) {
	Sound *t = Snack_GetSound(interp, wavePtr->soundName);
	ckfree(wavePtr->soundName);
	wavePtr->soundName = ckalloc(strlen(wavePtr->newSoundName)+1);
	strcpy(wavePtr->soundName, wavePtr->newSoundName);
	wavePtr->width = 0;
	wavePtr->ssmp    = 0;
	wavePtr->esmp    = -1;
	Snack_RemoveCallback(t, wavePtr->id);
	wavePtr->id = 0;
      }
      
      if (!wavePtr->id)
	wavePtr->id = Snack_AddCallback(s, UpdateWave, (int *)wavePtr);
      
      wavePtr->blocks = s->blocks;
      wavePtr->bufPos = s->length;
      wavePtr->samprate = s->samprate;
      wavePtr->encoding = s->encoding;
      wavePtr->nchannels = s->nchannels;
      wavePtr->storeType = s->storeType;
    }
    doCompute = 1;
  }
  wavePtr->esmp = wavePtr->endSmp;

  if (wavePtr->endSmp < 0)
    wavePtr->esmp = wavePtr->bufPos - 1;

  if (wavePtr->endSmp > wavePtr->bufPos - 1)
    wavePtr->esmp = wavePtr->bufPos - 1;

  if (wavePtr->startSmp > wavePtr->endSmp && wavePtr->endSmp >= 0)
    wavePtr->startSmp = wavePtr->endSmp;

  if (wavePtr->startSmp < 0)
    wavePtr->startSmp = 0;

  wavePtr->ssmp = wavePtr->startSmp;

  if (wavePtr->ssmp > wavePtr->esmp)
    wavePtr->ssmp = wavePtr->esmp;

  if (OptSpecified(OPTION_START)) {
    doCompute = 1;
  }

  if (OptSpecified(OPTION_END)) {
    doCompute = 1;
  }

  if (OptSpecified(OPTION_LIMIT)) {
    doCompute = 1;
  }

  if (OptSpecified(OPTION_SUBSAMPLE)) {
    doCompute = 1;
  }

  oldMode = wavePtr->mode;
  if (OptSpecified(OPTION_PIXPSEC) && OptSpecified(OPTION_WIDTH)) {
    wavePtr->mode = CONF_WIDTH_PPS;
    doCompute = 1;
  }
  else if (OptSpecified(OPTION_PIXPSEC)) {
    wavePtr->mode = CONF_PPS;
    doCompute = 1;
  }
  else if (OptSpecified(OPTION_WIDTH)) {
    wavePtr->mode = CONF_WIDTH;
  }

  if (oldMode != wavePtr->mode) {
    doCompute = 1;
  }
  
  if (wavePtr->width != wavePtr->widthSet) {
    wavePtr->width = wavePtr->widthSet;
    doCompute = 1;
  }
  
  if (wavePtr->mode == CONF_WIDTH_PPS) {
    if (OptSpecified(OPTION_END) && !OptSpecified(OPTION_START)) {
      wavePtr->ssmp = (int) (wavePtr->esmp - wavePtr->width *
			     wavePtr->samprate / wavePtr->pixpsec);
    } else {
      wavePtr->esmp = (int) (wavePtr->ssmp + wavePtr->width *
			     wavePtr->samprate / wavePtr->pixpsec);
    }
  }
  else if (wavePtr->mode == CONF_PPS) {
    wavePtr->width = (int)((wavePtr->esmp - wavePtr->ssmp) * 
			   wavePtr->pixpsec / wavePtr->samprate/* + 0.5*/);
  }
  else if (wavePtr->mode == CONF_WIDTH) {
    if (wavePtr->esmp != wavePtr->ssmp) {
      wavePtr->pixpsec = (double) wavePtr->width * wavePtr->samprate /
	(wavePtr->esmp - wavePtr->ssmp);
    }
  }

  if (OptSpecified(OPTION_PRECOMPWAVE)) {
    wavePtr->preCompInvalid = 0;
    doCompute = 1;
  }

  if (OptSpecified(OPTION_CHANNEL)) {
    if (GetChannel(interp, wavePtr->channelStr, wavePtr->nchannels, 
		   &wavePtr->channelSet) != TCL_OK) {
      return TCL_ERROR;
    }
    doCompute = 1;
  }
  wavePtr->channel = wavePtr->channelSet;
  if (wavePtr->nchannels == 1) {
    wavePtr->channel = 0;
  }

  if (OptSpecified(OPTION_PROGRESS)) {
    if (wavePtr->progressCmd != NULL) {
      wavePtr->cmdPtr = Tcl_NewStringObj(wavePtr->progressCmd, -1);
      Tcl_IncrRefCount(wavePtr->cmdPtr);
    } else {
      if (wavePtr->cmdPtr != NULL) {
	Tcl_DecrRefCount(wavePtr->cmdPtr);
	wavePtr->cmdPtr = NULL;
      }
    }
  }

  if (wavePtr->subSampleInt == 0) {
    if (wavePtr->esmp - wavePtr->ssmp > NSAMPLES) { 
      wavePtr->subSample = (wavePtr->esmp - wavePtr->ssmp) / NSAMPLES;
    } else {
      wavePtr->subSample = 1;
    }
  } else {
    wavePtr->subSample = wavePtr->subSampleInt;
  }

  if (wavePtr->trimstart == 1 && wavePtr->width > 0) {
    int len = wavePtr->esmp - wavePtr->ssmp;
    double fraq = (double) len / wavePtr->width;
    
    if (fraq > 0.0) {
      wavePtr->ssmp = (int) (fraq * floor(wavePtr->ssmp/fraq));
      wavePtr->esmp = wavePtr->ssmp + len;
    }
    if (wavePtr->esmp > wavePtr->bufPos - 1)
      wavePtr->esmp = wavePtr->bufPos - 1;
  }


  if (wavePtr->fg == NULL) {
    newGC = None;
  } else {
    gcValues.foreground = wavePtr->fg->pixel;
    gcValues.line_width = 1;
    mask = GCForeground|GCLineWidth;
    if (wavePtr->fillStipple != None) {
      gcValues.stipple = wavePtr->fillStipple;
      gcValues.fill_style = FillStippled;
      mask |= GCStipple|GCFillStyle;
    }
    newGC = Tk_GetGC(tkwin, mask, &gcValues);
    gcValues.line_width = 0;
  }
  if (wavePtr->gc != None) {
    Tk_FreeGC(Tk_Display(tkwin), wavePtr->gc);
  }
  wavePtr->gc = newGC;
  
  ComputeWaveBbox(canvas, wavePtr);

  if (doCompute) {
    if (ComputeWaveCoords(itemPtr) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  for (i = 0; configSpecs[i].type != TK_CONFIG_END; i++) {
    configSpecs[i].specFlags &= ~TK_CONFIG_OPTION_SPECIFIED;
  }
  
  if (wavePtr->debug > 1)
    Snack_WriteLogInt("  Exit ConfigureWave", wavePtr->width);

  return TCL_OK;
}

static void
DeleteWave(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display)
{
  WaveItem *wavePtr = (WaveItem *) itemPtr;

  if ((wavePtr->id) &&
      (Snack_GetSound(wavePtr->interp, wavePtr->soundName) != NULL)) {
    Snack_RemoveCallback(wavePtr->sound, wavePtr->id);
  }

  if (wavePtr->soundName != NULL) ckfree(wavePtr->soundName);

  if (wavePtr->x0 != NULL) ckfree((char *) wavePtr->x0);
  if (wavePtr->y0 != NULL) ckfree((char *) wavePtr->y0);
  if (wavePtr->x1 != NULL) ckfree((char *) wavePtr->x1);
  if (wavePtr->y1 != NULL) ckfree((char *) wavePtr->y1);

  if (wavePtr->fg != NULL) Tk_FreeColor(wavePtr->fg);

  if (wavePtr->fillStipple != None) Tk_FreeBitmap(display, wavePtr->fillStipple);

  if (wavePtr->gc != None) Tk_FreeGC(display, wavePtr->gc);

  if (wavePtr->preWI != NULL) ckfree((char *)wavePtr->preWI);

  if (wavePtr->preSound != NULL) Snack_DeleteSound(wavePtr->preSound);

  if (wavePtr->sound != NULL) {
    if (wavePtr->sound->storeType == SOUND_IN_FILE) {
      wavePtr->sound->itemRefCnt--;
    }
  }

  if (wavePtr->cmdPtr != NULL) Tcl_DecrRefCount(wavePtr->cmdPtr);
}

static void
ComputeWaveBbox(Tk_Canvas canvas, WaveItem *wavePtr)
{
  int width = wavePtr->width;
  int height = wavePtr->height;
  int x = (int) (wavePtr->x + ((wavePtr->x >= 0) ? 0.5 : - 0.5));
  int y = (int) (wavePtr->y + ((wavePtr->y >= 0) ? 0.5 : - 0.5));
  
  switch (wavePtr->anchor) {
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

  wavePtr->header.x1 = x;
  wavePtr->header.y1 = y;
  wavePtr->header.x2 = x + width;
  wavePtr->header.y2 = y + height;
}

static void
DisplayWave(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display,
	    Drawable drawable, int x, int y, int width, int height)
{
  WaveItem *wavePtr = (WaveItem *) itemPtr;
  int i;
  int xo = wavePtr->header.x1;
  int yo = wavePtr->header.y1;
  int ym = wavePtr->height / 2;
  int dx = max(x - xo, 0);
  float scale = 1000000.0f;
  XPoint fpts[5];

  if (wavePtr->debug > 1) Snack_WriteLogInt("  Enter DisplayWave", width);

  if (wavePtr->height == 0) return;

  if (wavePtr->gc == None) return;

  if (wavePtr->fillStipple != None)
    Tk_CanvasSetStippleOrigin(canvas, wavePtr->gc);

  if (wavePtr->height > 2) {
    scale = 2 * ((wavePtr->maxv > -wavePtr->minv)
		 ? wavePtr->maxv :
		 -wavePtr->minv) / (float)(wavePtr->height - 2);
  }
  if (scale < 0.00001f) {
    scale = 0.00001f;
  }

  if (dx + width > wavePtr->width) {
    width = wavePtr->width - dx;
  }
  if (dx > 0) {
    dx--;
    if (width < wavePtr->width - dx) width++;
    if (width < wavePtr->width - dx) width++;
  }
  for (i = dx; i < dx + width; i++) {
    Tk_CanvasDrawableCoords(canvas, xo + wavePtr->x0[i],
			    yo + ym - wavePtr->y0[i] / scale,
			    &fpts[0].x, &fpts[0].y);
    Tk_CanvasDrawableCoords(canvas, xo + wavePtr->x1[i],
			    yo + ym - wavePtr->y1[i] / scale,
			    &fpts[1].x, &fpts[1].y);
    Tk_CanvasDrawableCoords(canvas, xo + wavePtr->x1[i]+1,
			    yo + ym - wavePtr->y1[i] / scale,
			    &fpts[2].x, &fpts[2].y);
    XDrawLines(display, drawable, wavePtr->gc, fpts, 3, CoordModeOrigin);
  }
  
  if (wavePtr->zeroLevel) {
    Tk_CanvasDrawableCoords(canvas, (double) xo, 
			    (double) (yo + wavePtr->height / 2),
			    &fpts[0].x, &fpts[0].y);
    Tk_CanvasDrawableCoords(canvas, (double) (xo + wavePtr->width - 1),
			    (double) (yo + wavePtr->height / 2),
			    &fpts[1].x, &fpts[1].y);
    XDrawLines(display, drawable, wavePtr->gc, fpts, 2, CoordModeOrigin);
  }
  
  if (wavePtr->frame) {
    Tk_CanvasDrawableCoords(canvas, (double) xo, (double) yo,
			    &fpts[0].x, &fpts[0].y);
    Tk_CanvasDrawableCoords(canvas, (double) (xo + wavePtr->width - 1), 
			    (double) yo,
			    &fpts[1].x, &fpts[1].y);
    Tk_CanvasDrawableCoords(canvas, (double) (xo + wavePtr->width - 1),
			    (double) (yo + wavePtr->height - 1),
			    &fpts[2].x, &fpts[2].y);
    Tk_CanvasDrawableCoords(canvas, (double) xo, 
			    (double) (yo + wavePtr->height - 1),
			    &fpts[3].x, &fpts[3].y);
    Tk_CanvasDrawableCoords(canvas, (double) xo, (double) yo,
			    &fpts[4].x, &fpts[4].y);
    XDrawLines(display, drawable, wavePtr->gc, fpts, 5, CoordModeOrigin);
  }

  if (wavePtr->debug > 1) Snack_WriteLog("  Exit DisplayWave\n");
}

static double
WaveToPoint(Tk_Canvas canvas, Tk_Item *itemPtr, double *coords)
{
  WaveItem *wavePtr = (WaveItem *) itemPtr;
  double dx = 0.0, dy = 0.0;
  double x1 = wavePtr->header.x1;
  double y1 = wavePtr->header.y1;
  double x2 = wavePtr->header.x2;
  double y2 = wavePtr->header.y2;
  
  if (coords[0] < x1)
    dx = x1 - coords[0];
  else if (coords[0] > x2)
    dx = coords[0] - x2;
  else
    dx = 0;

  if (coords[1] < y1)
    dy = y1 - coords[1];
  else if (coords[1] > y2)
    dy = coords[1] - y2;
  else
    dy = 0;
  
  return hypot(dx, dy);
}

static int
WaveToArea(Tk_Canvas canvas, Tk_Item *itemPtr, double *rectPtr)
{
  WaveItem *wavePtr = (WaveItem *) itemPtr;

  if ((rectPtr[2] <= wavePtr->header.x1) ||
      (rectPtr[0] >= wavePtr->header.x2) ||
      (rectPtr[3] <= wavePtr->header.y1) ||
      (rectPtr[1] >= wavePtr->header.y2))
    return -1;

  if ((rectPtr[0] <= wavePtr->header.x1) &&
      (rectPtr[1] <= wavePtr->header.y1) &&
      (rectPtr[2] >= wavePtr->header.x2) &&
      (rectPtr[3] >= wavePtr->header.y2))
    return 1;
 
  return 0;
}

static void
ScaleWave(Tk_Canvas canvas, Tk_Item *itemPtr, double ox, double oy,
	  double sx, double sy)
{
  WaveItem *wavePtr = (WaveItem *) itemPtr;
  double *x0 = wavePtr->x0;
  double *y0 = wavePtr->y0;
  double *x1 = wavePtr->x1;
  double *y1 = wavePtr->y1;
  int i;
  
  for (i = 0; i < wavePtr->width; i++) {
    x0[i] = ox + sx * (x0[i] - ox);
    y0[i] = oy + sy * (y0[i] - oy);
    x1[i] = ox + sx * (x1[i] - ox);
    y1[i] = oy + sy * (y1[i] - oy);
  }
  wavePtr->width  = (int) (sx * wavePtr->width) + 1;
  wavePtr->height = (int) (sy * wavePtr->height);
  if (wavePtr->bufPos > 0)
    wavePtr->pixpsec = (double) wavePtr->width * wavePtr->samprate /
      wavePtr->bufPos;

  ComputeWaveBbox(canvas, wavePtr);
}

static void
TranslateWave(Tk_Canvas canvas, Tk_Item *itemPtr, double dx, double dy)
{
  WaveItem *wavePtr = (WaveItem *) itemPtr;

  wavePtr->x += dx;
  wavePtr->y += dy;
  ComputeWaveBbox(canvas, wavePtr);
}

static int
WaveToPS(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int prepass)
{
  WaveItem *wavePtr = (WaveItem *) itemPtr;
  double  *x0 = wavePtr->x0;
  double  *y0 = wavePtr->y0;
  double  *x1 = wavePtr->x1;
  double  *y1 = wavePtr->y1;
  int i;
  char buffer[100];
  int xo = wavePtr->header.x1;
  int yo = wavePtr->header.y1;
  float scale = 1000000.0f;

  if (wavePtr->fg == NULL) {
    return TCL_OK;
  }
  if (wavePtr->height > 2) {
    scale = 2 * ((wavePtr->maxv > -wavePtr->minv)
		 ? wavePtr->maxv :
		 -wavePtr->minv) / (float)(wavePtr->height - 2);
  }
  if (scale < 0.00001f) {
    scale = 0.00001f;
  }

  Tcl_AppendResult(interp, "%% WAVE BEGIN\n", (char *) NULL);
  
  for (i = 0; i < wavePtr->width; i++) {
    sprintf(buffer,
	    "%.1f %.1f moveto\n%.1f %.1f lineto\n",
	    x0[i] + xo, Tk_CanvasPsY(canvas, (double) 
				     (-y0[i]/scale + yo+ wavePtr->height / 2)),
	    x1[i] + xo, Tk_CanvasPsY(canvas, (double) 
				     (-y1[i]/scale + yo+ wavePtr->height / 2)));
    Tcl_AppendResult(interp, buffer, (char *) NULL);
    if ((double)(wavePtr->esmp - wavePtr->ssmp)/wavePtr->width < 1.0) {
      sprintf(buffer, "%.1f %.1f lineto\n",
	      x1[i] + xo + 1, Tk_CanvasPsY(canvas, (double) 
		   (-y1[i]/scale + yo+ wavePtr->height / 2)));
      Tcl_AppendResult(interp, buffer, (char *) NULL);
    }
  }

  if (wavePtr->zeroLevel) {
    sprintf(buffer, "%.1f %.1f moveto\n", (double) xo,
	    Tk_CanvasPsY(canvas, (double) (yo + wavePtr->height / 2)));
    Tcl_AppendResult(interp, buffer, (char *) NULL);

    sprintf(buffer, "%.1f %.1f lineto\n", (double) xo + wavePtr->width - 1,
	    Tk_CanvasPsY(canvas, (double) (yo + wavePtr->height / 2)));
    Tcl_AppendResult(interp, buffer, (char *) NULL);
  }

  if (wavePtr->frame) {
    sprintf(buffer, "%.1f %.1f moveto\n", (double) xo, Tk_CanvasPsY(canvas, (double) yo));
    Tcl_AppendResult(interp, buffer, (char *) NULL);

    sprintf(buffer, "%.1f %.1f lineto\n", (double) xo + wavePtr->width - 1,
	    Tk_CanvasPsY(canvas, (double) yo));
    Tcl_AppendResult(interp, buffer, (char *) NULL);

    sprintf(buffer, "%.1f %.1f lineto\n", (double) xo + wavePtr->width - 1,
	    Tk_CanvasPsY(canvas, (double) (yo + wavePtr->height - 1)));
    Tcl_AppendResult(interp, buffer, (char *) NULL);

    sprintf(buffer, "%.1f %.1f lineto\n", (double) xo,
	    Tk_CanvasPsY(canvas, (double) (yo + wavePtr->height - 1)));
    Tcl_AppendResult(interp, buffer, (char *) NULL);

    sprintf(buffer, "%.1f %.1f lineto\n", (double) xo,
	    Tk_CanvasPsY(canvas, (double) yo));
    Tcl_AppendResult(interp, buffer, (char *) NULL);
  }

  Tcl_AppendResult(interp, "1 setlinewidth\n", (char *) NULL);
  Tcl_AppendResult(interp, "0 setlinecap\n0 setlinejoin\n", (char *) NULL);
  if (Tk_CanvasPsColor(interp, canvas, wavePtr->fg) != TCL_OK) {
    return TCL_ERROR;
  };
  if (wavePtr->fillStipple != None) {
    Tcl_AppendResult(interp, "StrokeClip ", (char *) NULL);
    if (Tk_CanvasPsStipple(interp, canvas, wavePtr->fillStipple) != TCL_OK) {
      return TCL_ERROR;
    }
  } else {
    Tcl_AppendResult(interp, "stroke\n", (char *) NULL);
  }
  
  Tcl_AppendResult(interp, "%% WAVE END\n", (char *) NULL);

  return TCL_OK;
}
