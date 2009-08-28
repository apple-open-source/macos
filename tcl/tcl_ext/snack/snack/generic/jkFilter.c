/* 
 * Copyright (C) 2001-2002 Kare Sjolander <kare@speech.kth.se>
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

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <string.h>
#include "tcl.h"
#include "snack.h"

Snack_FilterType *snackFilterTypes = NULL;

Tcl_HashTable *filterHashTable;

extern float floatBuffer[];

int
filterSndCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	     Tcl_Obj *CONST objv[])
{
  register Sound *s = (Sound *) clientData;
  int arg, i, inSize, outSize, drain = 1, startpos = 0, endpos = -1, len;
  int fs, fi, es, ei;
  char *string = NULL;
  Tcl_HashEntry *hPtr;
  Snack_Filter f;
  Snack_StreamInfo si;
  static CONST84 char *subOptionStrings[] = {
    "-start", "-end", "-continuedrain", "-progress", NULL
  };
  enum subOptions {
    START, END, DRAIN, PROGRESS
  };

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "filter only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "filter filterCmd");
    return TCL_ERROR;
  }

  if (s->cmdPtr != NULL) {
    Tcl_DecrRefCount(s->cmdPtr);
    s->cmdPtr = NULL;
  }

  for (arg = 3; arg < objc; arg += 2) {
    int index;

    if (Tcl_GetIndexFromObj(interp, objv[arg], subOptionStrings,
			    "option", 0, &index) != TCL_OK) {
      return TCL_ERROR;
    }

    if (arg + 1 == objc) {
      Tcl_AppendResult(interp, "No argument given for ",
		       subOptionStrings[index], " option", (char *) NULL);
      return TCL_ERROR;
    }
    
    switch ((enum subOptions) index) {
    case START:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &startpos) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case END:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &endpos) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case DRAIN:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &drain) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case PROGRESS:
      {
	char *str = Tcl_GetStringFromObj(objv[arg+1], NULL);

	if (strlen(str) > 0) {
	  Tcl_IncrRefCount(objv[arg+1]);
	  s->cmdPtr = objv[arg+1];
	}
	break;
      }
    }
  }

  if (startpos < 0) startpos = 0;
  if (endpos > (s->length - 1) || endpos == -1)
    endpos = s->length - 1;
  if (startpos > endpos && endpos != -1) return TCL_OK;
  len = endpos - startpos + 1;

  string = Tcl_GetStringFromObj(objv[2], NULL);

  hPtr = Tcl_FindHashEntry(filterHashTable, string);
  if (hPtr != NULL) {
    f = (Snack_Filter) Tcl_GetHashValue(hPtr);
  } else {
    Tcl_AppendResult(interp, "No such filter: ", string, (char *) NULL);
    return TCL_ERROR;
  }
  Snack_StopSound(s, interp);

  si = (Snack_StreamInfo) ckalloc(sizeof(SnackStreamInfo));

  si->streamWidth = s->nchannels;
  si->outWidth    = s->nchannels;
  si->rate        = s->samprate;

  Snack_ProgressCallback(s->cmdPtr, interp, "Filtering sound", 0.0);

  (f->startProc)(f, si);

  len *= s->nchannels;
  fi = (startpos * s->nchannels) >> FEXP;
  fs = (startpos * s->nchannels) - (fi << FEXP);
  ei = (endpos * s->nchannels) >> FEXP;
  es = (endpos * s->nchannels) - (ei << FEXP);
  
  if (len > 0) {
    for (i = fi; i <= ei; i++) {
      int res;
      
      if (i > fi) fs = 0;
      if (i < ei) {
	inSize  = min(len, (FBLKSIZE - fs) / s->nchannels);
	outSize = min(len, (FBLKSIZE - fs) / s->nchannels);
      } else {
	inSize  = (es - fs) / s->nchannels + 1;
	outSize = (es - fs) / s->nchannels + 1;
      }
      
      (f->flowProc)(f, si, &s->blocks[i][fs], &s->blocks[i][fs],
		    &inSize, &outSize);
      res = Snack_ProgressCallback(s->cmdPtr, interp, "Filtering sound",
				   (float) (i - fi) / (ei - fi + 1));
      if (res != TCL_OK) {
	return TCL_ERROR;
      }
    }
  }
  while (drain) {
    int j;
    
    inSize = 0;
    outSize = PBSIZE;
    (f->flowProc)(f, si, floatBuffer, floatBuffer, &inSize, &outSize);

    if (endpos + outSize + 1 > s->length) {
      if (Snack_ResizeSoundStorage(s, endpos + outSize + 1) != TCL_OK) {
	return TCL_ERROR;
      }
      for (i = s->length; i < endpos + outSize + 1; i++) {
	FSAMPLE(s, i) = 0.0f;
      }
    }

    for (i = endpos + 1, j = 0; j < min(outSize, PBSIZE); i++, j++) {
      FSAMPLE(s, i) = FSAMPLE(s, i) + floatBuffer[j];
    }
    if (endpos + outSize + 1 > s->length) {
  /*Snack_PutSoundData(s, i, &floatBuffer[j], (endpos+outSize+1)-s->length);*/
      s->length = endpos + outSize + 1;
    }
    drain = 0;
  }

  Snack_ProgressCallback(s->cmdPtr, interp, "Filtering sound", 1.0);

  ckfree((char *) si);

  Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);

  return TCL_OK;
}

int
filterObjCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	     Tcl_Obj *CONST objv[])
{
  Snack_Filter f = (Snack_Filter) clientData;
  int length = 0;
  char *string = NULL;
  Tcl_HashEntry *hPtr;
  
  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "cmd");
    return TCL_ERROR;
  }
  string = Tcl_GetStringFromObj(objv[1], &length);

  if (strncmp("configure", string, length) == 0) {
    if ((f->configProc)(f, interp, objc-2, &objv[2]) != TCL_OK) {
      return TCL_ERROR;
    }
  } else if (strncmp("destroy", string, length) == 0) {
    string = Tcl_GetStringFromObj(objv[0], &length);
    hPtr = Tcl_FindHashEntry(filterHashTable, string);
    if (hPtr != NULL) {
      Tcl_DeleteCommand(interp, string);
      Tcl_DeleteHashEntry(hPtr);
    }
    if (f->freeProc != NULL) {
      (f->freeProc)(f);
    }
  } else {
    Tcl_AppendResult(interp, "bad option \"", string, "\": must be configure, "
		     "destroy or ...", (char *) NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

int
Snack_FilterCmd(ClientData cdata, Tcl_Interp *interp, int objc,
		Tcl_Obj *CONST objv[])
{
  Snack_FilterType *sf;
  Snack_Filter new = NULL;
  int flag;
  static int id = 0;
  static char ids[80];
  char *name;
  Tcl_HashTable *hTab = (Tcl_HashTable *) cdata;
  Tcl_HashEntry *hPtr;
  int length = 0;
  char *string = NULL;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "type");
    return TCL_ERROR;
  }
  string = Tcl_GetStringFromObj(objv[1], &length);

  do {
    sprintf(ids, "%s%d", string, ++id);
  } while (Tcl_FindHashEntry(hTab, ids) != NULL);
  name = ids;

  hPtr = Tcl_FindHashEntry(hTab, name);
  if (hPtr != NULL) {
    Tcl_DeleteCommand(interp, name);
  }
  
  for (sf = snackFilterTypes; sf != NULL; sf = sf->nextPtr) {
    if (strcmp(string, sf->name) == 0) {
      if ((new = (sf->createProc)(interp, objc-2, &objv[2])) == (Snack_Filter) NULL) return TCL_ERROR;
      break;
    }
  }
  if (sf == NULL) {
    Tcl_AppendResult(interp, "No such filter type: ", string, NULL);
    return TCL_ERROR;
  }
  new->configProc = sf->configProc;
  new->startProc  = sf->startProc;
  new->flowProc   = sf->flowProc;
  new->freeProc   = sf->freeProc;
  new->si         = NULL;
  new->prev       = NULL;
  new->next       = NULL;

  hPtr = Tcl_CreateHashEntry(hTab, name, &flag);
  Tcl_SetHashValue(hPtr, (ClientData) new);

  Tcl_CreateObjCommand(interp, name, filterObjCmd, (ClientData) new,
		       (Tcl_CmdDeleteProc *) NULL); 
  
  Tcl_SetObjResult(interp, Tcl_NewStringObj(name, -1));

  filterHashTable = hTab;

  return TCL_OK;
}

void
Snack_FilterDeleteCmd(ClientData clientData)
{
}

/*
typedef struct lowpassFilter {
  configProc *configProc;
  flowProc   *flowProc;
  Snack_Filter prev, next;
  double     dataRatio;
  double center;
  double last;
} lowpassFilter;

typedef struct lowpassFilter *lowpassFilter_t;

Snack_Filter
lowpassCreateProc(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  gainFilter_t sf;
  
  sf = (gainFilter_t) ckalloc(sizeof(gainFilter));
  
  if (gainConfigProc((Snack_Filter) sf, interp, objc, objv) != TCL_OK) {
    ckfree((char *) sf);
    return (Snack_Filter) NULL;
  }

  return (Snack_Filter) sf;
}

int
lowpassConfigProc(Snack_Filter f, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
  lowpassFilter_t lf = (lowpassFilter_t) f;
  int arg, arg1 = 0;
  double val;
  static char *optionStrings[] = {
    "-factor", NULL
  };
  enum options {
    FACTOR
  };

  if (objc != 0 && objc != 2) {
    Tcl_WrongNumArgs(interp, 0, objv, "configure -factor value");
    return TCL_ERROR;
  }

  for (arg = arg1; arg < objc; arg += 2) {
    int index;

    if (Tcl_GetIndexFromObj(interp, objv[arg], optionStrings, "option", 0,
			    &index) != TCL_OK) {
      return TCL_ERROR;
    }

    switch ((enum options) index) {
    case FACTOR:
      {
	if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &val) != TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    }
  }

  lf->center= val;
  lf->last = 0.0;
  lf->prev = (Snack_Filter) NULL;
  lf->next = (Snack_Filter) NULL;

  return TCL_OK;
}

int
lowpassFlowProc(Snack_Filter f, Snack_StreamInfo si, float *in, float *out,
		int frames)
{
  lowpassFilter_t lpf = (lowpassFilter_t) f;
  int i;
  double insmp = 0.0, outsmp, last;
  double a = 6.28318530718 * lpf->center / 16000.0;
  double b = exp(-a / 16000.0);

  last = lpf->last;
  for (i = 0; i < frames; i++) {
    insmp = (double) in[i];
    outsmp = insmp * a + last * b;
    last = insmp;
    out[i] = (float) (0.4 * outsmp);
  }
  lpf->last = last;

  return(frames);
}

Snack_FilterType snackLowpassType = {
  "lowpass",
  lowpassCreateProc,
  lowpassConfigProc,
  lowpassFlowProc,
  NULL,
  (Snack_FilterType *) NULL
};
*/

struct mapFilter {
  configProc *configProc;
  startProc  *startProc;
  flowProc   *flowProc;
  freeProc   *freeProc;
  Tcl_Interp *interp;
  Snack_Filter prev, next;
  Snack_StreamInfo si;
  double     dataRatio;
  int        reserved[4];
  /* private members */
  int        nm;
  float      *m;
  int        ns;
  float      *s;
  int        width;
} mapFilter;

typedef struct mapFilter *mapFilter_t;

int
mapConfigProc(Snack_Filter f, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
  mapFilter_t mf = (mapFilter_t) f;
  int i;
  double val;

  if (objc > mf->nm) {
    ckfree((char *) mf->m);
    mf->m = (float *) ckalloc(sizeof(float) * objc);
    mf->nm = objc;
  }
  for (i = 0; i < objc; i++) {
    if (Tcl_GetDoubleFromObj(interp, objv[i], &val) != TCL_OK) {
      return TCL_ERROR;
    }
    mf->m[i] = (float) val;
  }

  if (objc == 1 && mf->nm > 1 && mf->width > 0) {
    /* Special case, duplicate m[0] on the diagonal */
    for (i = 0; i < mf->nm; i = i + mf->width + 1) {
      mf->m[i] = mf->m[0];
    }
  }

  return TCL_OK;
}

Snack_Filter
mapCreateProc(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  mapFilter_t mf;
  
  mf = (mapFilter_t) ckalloc(sizeof(mapFilter));
  mf->nm = objc;
  if ((mf->m = (float *) ckalloc(sizeof(float) * objc)) == NULL) {
    return (Snack_Filter) NULL;
  }
  mf->ns = 0;
  mf->s = NULL;
  mf->width = 0;

  if (mapConfigProc((Snack_Filter) mf, interp, objc, objv) != TCL_OK) {
    ckfree((char *) mf->m);
    ckfree((char *) mf);
    return (Snack_Filter) NULL;
  }

  return (Snack_Filter) mf;
}

int
mapStartProc(Snack_Filter f, Snack_StreamInfo si)
{
  mapFilter_t mf = (mapFilter_t) f;
  int n;
  
  if (mf->nm < si->outWidth * si->streamWidth) {
    int needed = si->outWidth * si->streamWidth;
    float *tmp = (float *) ckalloc(sizeof(float) * needed);

    for (n = 0; n < mf->nm; n++) {
      tmp[n] = mf->m[n];
    }
    for (; n < needed; n++) {
      tmp[n] = 0.0f;
    }
    if (mf->nm == 1) { /* Special case, duplicate m[0] on the diagonal */
      for (n = si->streamWidth + 1; n < needed; n = n + si->streamWidth + 1) {
	tmp[n] = mf->m[0];
      }
    }
    ckfree((char *) mf->m);
    mf->nm = needed;
    mf->m = tmp;
  }
  if (mf->ns < si->streamWidth) {
    mf->ns = si->streamWidth;
    if (mf->s != NULL) {
      ckfree((char *) mf->s);
    }
    mf->s = (float *) ckalloc(sizeof(float) * mf->ns);
  }

  /* Stream width can change dynamically, remember what was allocated above */

  mf->width = si->streamWidth;

  return TCL_OK;
}

int
mapFlowProc(Snack_Filter f, Snack_StreamInfo si, float *in, float *out,
	    int *inFrames, int *outFrames)
{
  mapFilter_t mf = (mapFilter_t) f;
  int i = 0, fr, wi, n, j, k, jstart;
  float tmp;
  
  for (fr = 0; fr < *inFrames; fr++) {
    n = 0;
    jstart = i;
    for (wi = 0; wi < si->outWidth; wi++) {
      tmp = 0.0f;
      j = jstart;
      for (k = 0; k < mf->width; k++) {
	tmp += (in[j++] * mf->m[n++]);
      }
      mf->s[wi] = tmp;
    }
    for (wi = 0; wi < si->outWidth; wi++) {
      out[i++] = mf->s[wi];
    }
    i += (si->streamWidth - si->outWidth);
  }

  *outFrames = *inFrames;

  return TCL_OK;
}

void
mapFreeProc(Snack_Filter f)
{
  mapFilter_t mf = (mapFilter_t) f;

  if (mf->m != NULL) {
    ckfree((char *) mf->m);
  }
  if (mf->s != NULL) {
    ckfree((char *) mf->s);
  }
  ckfree((char *) mf);
}

Snack_FilterType snackMapType = {
  "map",
  mapCreateProc,
  mapConfigProc,
  mapStartProc,
  mapFlowProc,
  mapFreeProc,
  (Snack_FilterType *) NULL
};

#define NMAXECHOS 10

struct echoFilter {
  configProc *configProc;
  startProc  *startProc;
  flowProc   *flowProc;
  freeProc   *freeProc;
  Tcl_Interp *interp;
  Snack_Filter prev, next;
  Snack_StreamInfo si;
  double     dataRatio;
  int        reserved[4];
  /* private members */
  int        cnt;
  int        numDelays;
  float      *buf;
  float      inGain;
  float      outGain;
  float      delay[NMAXECHOS];
  float      decay[NMAXECHOS];
  int        nsmp[NMAXECHOS];
  int        nmax;
  int        rest;
} echoFilter;

typedef struct echoFilter *echoFilter_t;

int
echoConfigProc(Snack_Filter f, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
  echoFilter_t rf = (echoFilter_t) f;
  int i;
  double val;

  /* Arguments need to be at least four and also come in pairs */

  if (objc < 4 || objc % 2) {
    Tcl_WrongNumArgs(interp, 0, objv, "echo inGain outGain delay1 decay1 ...");
    return TCL_ERROR;
  }

  if (Tcl_GetDoubleFromObj(interp, objv[0], &val) != TCL_OK) {
    return TCL_ERROR;
  }
  rf->inGain = (float) val;
  if (Tcl_GetDoubleFromObj(interp, objv[1], &val) != TCL_OK) {
    return TCL_ERROR;
  }
  rf->outGain = (float) val;

  rf->numDelays = 0;

  for (i = 2; i < objc; i += 2) {
    if (Tcl_GetDoubleFromObj(interp, objv[i], &val) != TCL_OK) {
      return TCL_ERROR;
    }
    if (val < 0.0) {
      Tcl_AppendResult(interp, "Delay must be positive", NULL);
      return TCL_ERROR;
    }
    rf->delay[i/2-1] = (float) val;
    if (Tcl_GetDoubleFromObj(interp, objv[i+1], &val) != TCL_OK) {
      return TCL_ERROR;
    }
    if (val < 0.0) {
      Tcl_AppendResult(interp, "Decay must be positive", NULL);
      return TCL_ERROR;
    }
    if (val > 1.0) {
      Tcl_AppendResult(interp, "Decay must be < 1.0", NULL);
      return TCL_ERROR;
    }
    rf->decay[i/2-1] = (float) val;
    rf->numDelays++;
  }

  if (rf->buf != NULL && rf->si != NULL) {
    int nmax = 0;

    for (i = 0; i < rf->numDelays; i++) {
      rf->nsmp[i] = (int) (rf->delay[i] * rf->si->rate / 1000.0) *
	rf->si->outWidth;
      if (rf->nsmp[i] > nmax) {
	nmax = rf->nsmp[i];
      }
    }

    if (nmax != rf->nmax) {
      float *tmpbuf = (float *) ckalloc(sizeof(float) * nmax);
      
      for (i = 0; i < rf->nmax; i++) {
	if (i == nmax) break;
	tmpbuf[i] = rf->buf[rf->cnt];
	rf->cnt = (rf->cnt+1) % rf->nmax;
      }
      for (; i < nmax; i++) {
	tmpbuf[i] = 0.0f;
      }
      ckfree((char *) rf->buf);
      rf->buf = tmpbuf;
      if (nmax < rf->nmax) {
	rf->cnt = nmax - 1;
      } else {
	rf->cnt = rf->nmax;
      }
      /*
      if (nmax < rf->nmax) {
	for (i = nmax - 1; i >= 0; i--) {
	  tmpbuf[i] = rf->buf[rf->cnt];
	  rf->cnt = (rf->cnt+rf->nmax+1) % rf->nmax;
	}
	rf->cnt = 0;
      } else {
	for (i = 0; i < rf->nmax; i++) {
	  tmpbuf[i] = rf->buf[rf->cnt];
	  rf->cnt = (rf->cnt+1) % rf->nmax;
	}
	for (; i < nmax; i++) {
	tmpbuf[i] = 0.0f;
	}
	rf->cnt = rf->nmax;
      }
      ckfree((char *) rf->buf);
      rf->buf = tmpbuf;
      */
      rf->nmax = nmax;
      rf->rest = nmax;
    }
  }

  return TCL_OK;
}

Snack_Filter
echoCreateProc(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  echoFilter_t rf;

  rf = (echoFilter_t) ckalloc(sizeof(echoFilter));
  rf->nmax = 0;
  rf->numDelays = 0;
  rf->buf = NULL;

  if (echoConfigProc((Snack_Filter) rf, interp, objc, objv) != TCL_OK) {
    ckfree((char *) rf);
    return (Snack_Filter) NULL;
  }

  return (Snack_Filter) rf;
}

int
echoStartProc(Snack_Filter f, Snack_StreamInfo si)
{
  echoFilter_t rf = (echoFilter_t) f;
  int i;

  if (rf->buf == NULL) {
    rf->nmax = 0;
    for (i = 0; i < rf->numDelays; i++) {
      rf->nsmp[i] = (int) (rf->delay[i] * si->rate / 1000.0) * si->outWidth;
      if (rf->nsmp[i] > rf->nmax) {
	rf->nmax = rf->nsmp[i];
      }
    }

    rf->buf = (float *) ckalloc(sizeof(float) * rf->nmax);
  }
  for (i = 0; i < rf->nmax; i++) {
    rf->buf[i] = 0.0f;
  }
  rf->cnt = 0;
  rf->rest = rf->nmax;

  return TCL_OK;
}

int
echoFlowProc(Snack_Filter f, Snack_StreamInfo si, float *in, float *out,
	    int *inFrames, int *outFrames)
{
  echoFilter_t rf = (echoFilter_t) f;
  int i, j, c;
  float tmp;

  /* Process *inFrames number samples, algorithm from SoX */

  for (i = 0; i < *inFrames; i++) {
    for (c = 0; c < si->outWidth; c++) {
      int index = i * si->outWidth + c;
      tmp = in[index] * rf->inGain;
      for (j = 0; j < rf->numDelays; j++) {
	tmp += rf->buf[(rf->cnt + rf->nmax - rf->nsmp[j]) % rf->nmax] * 
	  rf->decay[j];
      }
      tmp *= rf->outGain;
      rf->buf[rf->cnt] = in[index];
      out[index] = tmp;
      rf->cnt = (rf->cnt+1) % rf->nmax;
    }
  }

  /* If input exhausted start draining out delayed samples */

  if (*inFrames < *outFrames) {
    for (i = *inFrames; i < *outFrames; i++) {
      for (c = 0; c < si->outWidth; c++) {
	tmp = 0.0f;
	for (j = 0; j < rf->numDelays; j++) {
	  tmp += rf->buf[(rf->cnt + rf->nmax - rf->nsmp[j]) % rf->nmax] *
	    rf->decay[j];
	}
	tmp *= rf->outGain;
	rf->buf[rf->cnt] = 0.0f;
	out[i * si->outWidth + c] = tmp;
	rf->cnt = (rf->cnt+1) % rf->nmax;
	rf->rest--;
	if (rf->rest < 0) break;
      }
      if (rf->rest < 0) break;
    }

    if  (i < *outFrames) { /* last invocation prepare for next usage */
      *outFrames = i;
      for (j = 0; j < rf->nmax; j++) {
	rf->buf[j] = 0.0f;
      }
    }
  }

  return TCL_OK;
}

void
echoFreeProc(Snack_Filter f)
{
  echoFilter_t rf = (echoFilter_t) f;

  if (rf->buf != NULL) {
    ckfree((char *) rf->buf);
  }
  ckfree((char *) rf);
}

Snack_FilterType snackEchoType = {
  "echo",
  echoCreateProc,
  echoConfigProc,
  echoStartProc,
  echoFlowProc,
  echoFreeProc,
  (Snack_FilterType *) NULL
};

#define NMAXREVERBS 10

struct reverbFilter {
  configProc *configProc;
  startProc  *startProc;
  flowProc   *flowProc;
  freeProc   *freeProc;
  Tcl_Interp *interp;
  Snack_Filter prev, next;
  Snack_StreamInfo si;
  double     dataRatio;
  int        reserved[4];
  /* private members */
  int        cnt;
  int        numDelays;
  float      *buf;
  float      inGain;
  float      outGain;
  float      time;
  float      delay[NMAXREVERBS];
  float      decay[NMAXREVERBS];
  int        nsmp[NMAXREVERBS];
  int        nmax;
  float      pl, ppl, pppl;
} reverbFilter;

typedef struct reverbFilter *reverbFilter_t;

int
reverbConfigProc(Snack_Filter f, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
  reverbFilter_t rf = (reverbFilter_t) f;
  int i;
  double val;

  /* Arguments need to be at least three */

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 0, objv, "reverb outGain time delay1 ...");
    return TCL_ERROR;
  }

  if (Tcl_GetDoubleFromObj(interp, objv[0], &val) != TCL_OK) {
    return TCL_ERROR;
  }
  rf->outGain = (float) val;
  if (Tcl_GetDoubleFromObj(interp, objv[1], &val) != TCL_OK) {
    return TCL_ERROR;
  }
  rf->time = (float) val;
  rf->inGain = 1.0f;
  rf->numDelays = 0;

  for (i = 2; i < objc; i++) {
    if (Tcl_GetDoubleFromObj(interp, objv[i], &val) != TCL_OK) {
      return TCL_ERROR;
    }
    if (val < 0.0) {
      Tcl_AppendResult(interp, "Delay must be positive", NULL);
      return TCL_ERROR;
    }
    rf->delay[i-2] = (float) val;
    rf->numDelays++;
  }

  if (rf->buf != NULL && rf->si != NULL) {
    int nmax = 0;

    for (i = 0; i < rf->numDelays; i++) {
      rf->nsmp[i] = (int) (rf->delay[i] * rf->si->rate / 1000.0) *
	rf->si->outWidth;
      if (rf->nsmp[i] > nmax) {
	nmax = rf->nsmp[i];
      }
      rf->decay[i] = (float) pow(10.0, (-3.0 * rf->delay[i] / rf->time));
    }
    rf->pppl = rf->ppl = rf->pl = 32767.0f;
    for (i = 0; i < rf->numDelays; i++)
      rf->inGain *= (1.0f - (rf->decay[i] * rf->decay[i]));

    if (nmax != rf->nmax) {
      float *tmpbuf = (float *) ckalloc(sizeof(float) * nmax);
      
      for (i = 0; i < rf->nmax; i++) {
	if (i == nmax) break;
	tmpbuf[i] = rf->buf[rf->cnt];
	rf->cnt = (rf->cnt+1) % rf->nmax;
      }
      for (; i < nmax; i++) {
	tmpbuf[i] = 0.0f;
      }
      ckfree((char *) rf->buf);
      rf->buf = tmpbuf;
      if (nmax < rf->nmax) {
	rf->cnt = nmax - 1;
      } else {
	rf->cnt = rf->nmax;
      }
      /*
      if (nmax < rf->nmax) {
	for (i = nmax - 1; i >= 0; i--) {
	  tmpbuf[i] = rf->buf[rf->cnt];
	  rf->cnt = (rf->cnt+rf->nmax+1) % rf->nmax;
	}
	rf->cnt = 0;
      } else {
	for (i = 0; i < rf->nmax; i++) {
	  tmpbuf[i] = rf->buf[rf->cnt];
	  rf->cnt = (rf->cnt+1) % rf->nmax;
	}
	for (; i < nmax; i++) {
	tmpbuf[i] = 0.0f;
	}
	rf->cnt = rf->nmax;
      }
      ckfree((char *) rf->buf);
      rf->buf = tmpbuf;
      */
      rf->nmax = nmax;
    }
  }

  return TCL_OK;
}

Snack_Filter
reverbCreateProc(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  reverbFilter_t rf;

  rf = (reverbFilter_t) ckalloc(sizeof(reverbFilter));
  rf->nmax = 0;
  rf->numDelays = 0;
  rf->buf = NULL;

  if (reverbConfigProc((Snack_Filter) rf, interp, objc, objv) != TCL_OK) {
    ckfree((char *) rf);
    return (Snack_Filter) NULL;
  }

  return (Snack_Filter) rf;
}

int
reverbStartProc(Snack_Filter f, Snack_StreamInfo si)
{
  reverbFilter_t rf = (reverbFilter_t) f;
  int i;

  if (rf->buf == NULL) {
    rf->nmax = 0;
    for (i = 0; i < rf->numDelays; i++) {
      rf->nsmp[i] = (int) (rf->delay[i] * si->rate / 1000.0) * si->outWidth;
      if (rf->nsmp[i] > rf->nmax) {
	rf->nmax = rf->nsmp[i];
      }
      rf->decay[i] = (float) pow(10.0, (-3.0 * rf->delay[i] / rf->time));
    }
    rf->pppl = rf->ppl = rf->pl = 32767.0f;
    for (i = 0; i < rf->numDelays; i++)
      rf->inGain *= (1.0f - (rf->decay[i] * rf->decay[i]));

    rf->buf = (float *) ckalloc(sizeof(float) * rf->nmax);
    for (i = 0; i < rf->nmax; i++) {
      rf->buf[i] = 0.0f;
    }
  }
  rf->cnt = 0;

  return TCL_OK;
}

int
reverbFlowProc(Snack_Filter f, Snack_StreamInfo si, float *in, float *out,
	    int *inFrames, int *outFrames)
{
  reverbFilter_t rf = (reverbFilter_t) f;
  int i, j, c;
  float tmp;

  /* Process *inFrames number samples, algorithm from SoX */

  for (i = 0; i < *inFrames; i++) {
    for (c = 0; c < si->outWidth; c++) {
      int index = i * si->outWidth + c;
      tmp = in[index] * rf->inGain;
      for (j = 0; j < rf->numDelays; j++) {
	tmp += rf->buf[(rf->cnt + rf->nmax - rf->nsmp[j]) % rf->nmax] * 
	  rf->decay[j];
      }
      rf->buf[rf->cnt] = tmp;
      tmp *= rf->outGain;
      out[index] = tmp;
      rf->cnt = (rf->cnt+1) % rf->nmax;
    }
  }

  /* If input exhausted start draining out delayed samples */

  if (*inFrames < *outFrames) {
    for (i = *inFrames; i < *outFrames; i++) {
      for (c = 0; c < si->outWidth; c++) {
	tmp = 0.0f;
	for (j = 0; j < rf->numDelays; j++) {
	  tmp += rf->buf[(rf->cnt + rf->nmax - rf->nsmp[j]) % rf->nmax] *
	    rf->decay[j];
	}
	rf->buf[rf->cnt] = tmp;
	tmp *= rf->outGain;
	out[i * si->outWidth + c] = tmp;
	rf->cnt = (rf->cnt+1) % rf->nmax;

	rf->pppl = rf->ppl;
	rf->ppl = rf->pl;
	rf->pl = tmp;

	if (fabs(rf->pl)+fabs(rf->ppl)+fabs(rf->pppl) < 10.0f) break;
      }
      if (fabs(rf->pl)+fabs(rf->ppl)+fabs(rf->pppl) < 10.0f) break;
    }

    if  (i < *outFrames) { /* last invocation prepare for next usage */
      *outFrames = i;
      for (j = 0; j < rf->nmax; j++) {
	rf->buf[j] = 0.0f;
      }
    }
  }

  return TCL_OK;
}

void
reverbFreeProc(Snack_Filter f)
{
  reverbFilter_t rf = (reverbFilter_t) f;

  if (rf->buf != NULL) {
    ckfree((char *) rf->buf);
  }
  ckfree((char *) rf);
}

Snack_FilterType snackReverbType = {
  "reverb",
  reverbCreateProc,
  reverbConfigProc,
  reverbStartProc,
  reverbFlowProc,
  reverbFreeProc,
  (Snack_FilterType *) NULL
};

struct composeFilter {
  configProc *configProc;
  startProc  *startProc;
  flowProc   *flowProc;
  freeProc   *freeProc;
  Tcl_Interp *interp;
  Snack_Filter prev, next;
  Snack_StreamInfo si;
  double     dataRatio;
  int        reserved[4];
  /* private members */
  Snack_Filter ff, lf;
} composeFilter;

typedef struct composeFilter *composeFilter_t;

int
composeConfigProc(Snack_Filter f, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
  composeFilter_t cf = (composeFilter_t) f;
  int i;
  char *string = NULL;
  Tcl_HashEntry *hPtr;
  Snack_Filter curr, last;

  /* Need at least two filters to be composed */

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 0, objv, "compose filter1 filter2 ...");
    return TCL_ERROR;
  }

  for (i = 0; i < objc; i++) {
    string = Tcl_GetStringFromObj(objv[i], NULL);
    hPtr = Tcl_FindHashEntry(filterHashTable, string);
    if (hPtr == NULL) {
      Tcl_AppendResult(interp, "No such filter: ", string, (char *) NULL);
      return TCL_ERROR;
    }
  }

  string = Tcl_GetStringFromObj(objv[0], NULL);
  hPtr = Tcl_FindHashEntry(filterHashTable, string);
  cf->ff = (Snack_Filter) Tcl_GetHashValue(hPtr);
  curr = last = cf->ff;

  string = Tcl_GetStringFromObj(objv[objc-1], NULL);
  hPtr = Tcl_FindHashEntry(filterHashTable, string);
  cf->lf = (Snack_Filter) Tcl_GetHashValue(hPtr);

  for (i = 1; i < objc-1; i++) {
    string = Tcl_GetStringFromObj(objv[i], NULL);
    hPtr = Tcl_FindHashEntry(filterHashTable, string);
    if (hPtr != NULL) {
      curr = (Snack_Filter) Tcl_GetHashValue(hPtr);
      curr->prev = last;
      last->next = curr;
      last = last->next;
    }
  }
  curr->next = cf->lf;
  cf->lf->prev = cf->ff;

  return TCL_OK;
}

Snack_Filter
composeCreateProc(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  composeFilter_t cf;
  
  cf = (composeFilter_t) ckalloc(sizeof(composeFilter));

  if (composeConfigProc((Snack_Filter) cf, interp, objc, objv) != TCL_OK) {
    ckfree((char *) cf);
    return (Snack_Filter) NULL;
  }

  return (Snack_Filter) cf;
}

int
composeStartProc(Snack_Filter f, Snack_StreamInfo si)
{
  composeFilter_t cf = (composeFilter_t) f;
  Snack_Filter pf = cf->ff;

  while (pf != NULL) {
    pf->si = si;
    (pf->startProc)(pf, si);
    pf = pf->next;   
  }

  return TCL_OK;
}

int
composeFlowProc(Snack_Filter f, Snack_StreamInfo si, float *in, float *out,
	    int *inFrames, int *outFrames)
{
  composeFilter_t cf = (composeFilter_t) f;
  Snack_Filter pf = cf->ff;
  int inSize  = *inFrames;
  int outSize = *outFrames;

  while (pf != NULL) {
    (pf->flowProc)(pf, si, in, out, &inSize, &outSize);
    inSize = outSize;
    pf = pf->next;   
  }

  if (outSize < *outFrames) {
  }

  *outFrames = outSize;

  return TCL_OK;
}

void
composeFreeProc(Snack_Filter f)
{
  composeFilter_t cf = (composeFilter_t) f;

  ckfree((char *) cf);
}

Snack_FilterType snackComposeType = {
  "compose",
  composeCreateProc,
  composeConfigProc,
  composeStartProc,
  composeFlowProc,
  composeFreeProc,
  (Snack_FilterType *) NULL
};

struct fadeFilter {
  configProc *configProc;
  startProc  *startProc;
  flowProc   *flowProc;
  freeProc   *freeProc;
  Tcl_Interp *interp;
  Snack_Filter prev, next;
  Snack_StreamInfo si;
  double     dataRatio;
  int        reserved[4];
  /* private members */
  int        in;
  int        type;
  float      msLength;
  int        length;
  int        pos;
  float      floor;
} fadeFilter;

typedef struct fadeFilter *fadeFilter_t;
#define LINEAR 0
#define EXPONENTIAL 1
#define LOGARITHMIC 2

int
fadeConfigProc(Snack_Filter f, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
  fadeFilter_t mf = (fadeFilter_t) f;
  char *typestr;
  double val;

  if (objc == 3 || objc == 4) {
    typestr = Tcl_GetStringFromObj(objv[0], NULL);
    if (strcasecmp(typestr, "in") == 0) {
      mf->in = 1;
    } else if (strcasecmp(typestr, "out") == 0) {
      mf->in = 0;
    } else {
      Tcl_SetResult(interp, "bad fade direction, must be in or out",
		    TCL_STATIC);
      return TCL_ERROR;
    }
    
    typestr = Tcl_GetStringFromObj(objv[1], NULL);
    if (strncasecmp(typestr, "linear", 3) == 0) {
      mf->type = LINEAR;
    } else if (strncasecmp(typestr, "exponential", 3) == 0) {
      mf->type = EXPONENTIAL;
    } else if (strncasecmp(typestr, "logarithmic", 3) == 0) {
      mf->type = LOGARITHMIC;
    } else {
      Tcl_SetResult(interp,
	    "bad fade type, must be linear, exponential, or logarithmic",
		    TCL_STATIC);
      return TCL_ERROR;
    }
    
    if (Tcl_GetDoubleFromObj(interp, objv[2], &val) != TCL_OK) {
      return TCL_ERROR;
    }
    mf->msLength = (float) val;

    if (objc == 4) {
      if (Tcl_GetDoubleFromObj(interp, objv[3], &val) != TCL_OK) {
	return TCL_ERROR;
      }
      mf->floor = (float) val;
    }

  } else {
    
    /* Arguments need to be at least three */
    
    Tcl_WrongNumArgs(interp, 0, objv, "fade direction type length");
    return TCL_ERROR;
  }
  
  return TCL_OK;
}

Snack_Filter
fadeCreateProc(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  fadeFilter_t mf;
  
  mf = (fadeFilter_t) ckalloc(sizeof(fadeFilter));
  mf->floor = 0.0;

  if (fadeConfigProc((Snack_Filter) mf, interp, objc, objv) != TCL_OK) {
    ckfree((char *) mf);
    return (Snack_Filter) NULL;
  }
  
  return (Snack_Filter) mf;
}

int
fadeStartProc(Snack_Filter f, Snack_StreamInfo si)
{
  fadeFilter_t mf = (fadeFilter_t) f;
  mf->length = (int) (mf->msLength * si->rate / 1000.0);
  mf->pos = 0;
  
  return TCL_OK;
}

#define EULER 2.7182818284590452354 
#define EXP_MINUS_1 0.36787944117

int
fadeFlowProc(Snack_Filter f, Snack_StreamInfo si, float *in, float *out,
	    int *inFrames, int *outFrames)
{
  fadeFilter_t mf = (fadeFilter_t) f;
  int i = 0, fr, wi;
  float factor = 1.0;
  
  for (fr = 0; fr < *inFrames; fr++) {
    if (mf->pos < mf->length) {
      switch (mf->type) {
      case LINEAR:
	if (mf->in) {
	  factor = (float) ((1.0 - mf->floor) * mf->pos / (mf->length - 1) + mf->floor);
	} else {
	  factor = (float) (1.0 - ((1.0 - mf->floor) * mf->pos / (mf->length - 1)));
	}
	break;
      case EXPONENTIAL:
	if (mf->in) {
	  factor = (float) (1.0 - mf->floor) * exp(-10.0+10.0 * mf->pos/(mf->length-1)) +mf->floor;
	} else {
	  factor = (float) (1.0 - mf->floor) * exp(-10.0 * mf->pos/(mf->length-1)) + mf->floor;
	}
	break;
      case LOGARITHMIC:
	if (mf->in) {
	  factor = (float) (1.0 - mf->floor) * (0.5 + 0.5 *
				    log(EXP_MINUS_1 + (EULER - EXP_MINUS_1)
				  * (float) mf->pos / (mf->length-1))) + mf->floor;
	} else {
	  factor = (float) (1.0 - mf->floor) * (0.5 + 0.5 * 
				    log(EXP_MINUS_1 + (EULER - EXP_MINUS_1)
			      * (1.0-(float) mf->pos / (mf->length-1)))) + mf->floor;
	}
	break;
      }
    } else {
      factor = 1.0;
    }
    for (wi = 0; wi < si->outWidth; wi++) {
      out[i] = factor * in[i];
      i++;
    }
    mf->pos++;
  }

  *outFrames = *inFrames;

  return TCL_OK;
}

void
fadeFreeProc(Snack_Filter f)
{
  fadeFilter_t mf = (fadeFilter_t) f;

  ckfree((char *) mf);
}

Snack_FilterType snackFadeType = {
  "fade",
  fadeCreateProc,
  fadeConfigProc,
  fadeStartProc,
  fadeFlowProc,
  fadeFreeProc,
  (Snack_FilterType *) NULL
};

void
Snack_CreateFilterType(Snack_FilterType *typePtr)
{
  Snack_FilterType *typePtr2, *prevPtr;

  /*
   * If there's already a filter type with the given name, remove it.
   */
  
  for (typePtr2 = snackFilterTypes, prevPtr = NULL; typePtr2 != NULL;
       prevPtr = typePtr2, typePtr2 = typePtr2->nextPtr) {
    if (strcmp(typePtr2->name, typePtr->name) == 0) {
      if (prevPtr == NULL) {
	snackFilterTypes = typePtr2->nextPtr;
      } else {
	prevPtr->nextPtr = typePtr2->nextPtr;
      }
      break;
    }
  }
  typePtr->nextPtr = snackFilterTypes;
  snackFilterTypes = typePtr;
}

extern void createSynthesisFilters();
extern void createIIRFilter();

void
SnackCreateFilterTypes(Tcl_Interp *interp)
{
  snackFilterTypes = &snackComposeType;
  snackComposeType.nextPtr = NULL;
  Snack_CreateFilterType(&snackMapType);
  Snack_CreateFilterType(&snackEchoType);
  Snack_CreateFilterType(&snackReverbType);
  Snack_CreateFilterType(&snackFadeType);
  createSynthesisFilters();
  createIIRFilter();
  /*  Snack_CreateFilterType(&snackLowpassType);*/
}
