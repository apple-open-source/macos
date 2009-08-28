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

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <string.h>
#include "tcl.h"
#include "snack.h"

extern int wop, rop;

extern int
ParseSoundCmd(ClientData cdata, Tcl_Interp *interp, int objc,
	      Tcl_Obj *CONST objv[], char** namep, Sound** sp);

extern int littleEndian;

int
Snack_AddCallback(Sound *s, updateProc *proc, ClientData cd)
{
  jkCallback *cb = (jkCallback *) ckalloc(sizeof(jkCallback));

  if (cb == NULL) return(-1);
  cb->proc = proc;
  cb->clientData = cd;
  if (s->firstCB != NULL) {
    cb->id = s->firstCB->id + 1;
  } else {
    cb->id = 1;
  }
  cb->next = s->firstCB;
  s->firstCB = cb;

  if (s->debug > 1) { Snack_WriteLogInt("  Snack_AddCallback", cb->id); }

  return(cb->id);
}

void
Snack_RemoveCallback(Sound *s, int id)
{
  jkCallback *cb = s->firstCB, **pp = &s->firstCB, *cbGoner = NULL;

  if (s->debug > 1) Snack_WriteLogInt("  Snack_RemoveCallback", id);

  if (id == -1) return;

  while (cb != NULL) {
    if (cb->id == id) {
      cbGoner = cb;
      cb = cb->next;
      *pp = cb;
      ckfree((char *)cbGoner);
      return;
    } else {
      pp = &cb->next;
      cb = cb->next;
    }
  }
}

void
Snack_ExecCallbacks(Sound *s, int flag)
{
  jkCallback *cb;

  if (s->debug > 1) Snack_WriteLog("  Enter Snack_ExecCallbacks\n");

  for (cb = s->firstCB; cb != NULL; cb = cb->next) {
    if (s->debug > 2) Snack_WriteLogInt("    Executing callback", cb->id);
    (cb->proc)(cb->clientData, flag);
    if (s->debug > 2) Snack_WriteLog("    callback done\n");
  }

  if (s->changeCmdPtr != NULL) {
    Tcl_Obj *cmd = NULL;

    cmd = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(s->interp, cmd, s->changeCmdPtr);

    if (flag == SNACK_NEW_SOUND) {
      Tcl_ListObjAppendElement(s->interp, cmd, Tcl_NewStringObj("New", -1));
    } else if (flag == SNACK_DESTROY_SOUND) {
      Tcl_ListObjAppendElement(s->interp, cmd, Tcl_NewStringObj("Destroyed",
								-1));
    } else {
      Tcl_ListObjAppendElement(s->interp, cmd, Tcl_NewStringObj("More", -1));
    }
    Tcl_Preserve((ClientData) s->interp);
    if (Tcl_GlobalEvalObj(s->interp, cmd) != TCL_OK) {
      Tcl_AddErrorInfo(s->interp, "\n    (\"command\" script)");
      Tcl_BackgroundError(s->interp);
    }
    Tcl_Release((ClientData) s->interp);
  }
}

void
Snack_GetExtremes(Sound *s, SnackLinkedFileInfo *info, int start, int end,
		  int chan, float *pmax, float *pmin)
{
  int i, inc;
  float maxs, mins;

  if (s->length == 0) {
    if (s->encoding == LIN8OFFSET) {
      *pmax = 128.0f;
      *pmin = 128.0f;
    } else {
      *pmax = 0.0f;
      *pmin = 0.0f;
    }
    return;
  }
  
  if (chan == -1) {
    inc = 1;
    chan = 0;
  } else {
    inc = s->nchannels;
  }

  start = start * s->nchannels + chan;
  end   = end * s->nchannels + chan;

  switch (s->encoding) {
  case LIN8OFFSET:
    maxs = 0.0f;
    mins = 255.0f;
    break;
  case LIN8:
    maxs = -128.0f;
    mins = 127.0f;
    break;
  case LIN24:
  case LIN24PACKED:
    maxs = -8388608.0f;
    mins = 8388607.0f;
    break;
  case LIN32:
    maxs = -2147483648.0f;
    mins = 2147483647.0f;
    break;
  default:
    maxs = -32768.0f;
    mins = 32767.0f;
  }

  if (s->precision == SNACK_SINGLE_PREC) {
    if (s->storeType == SOUND_IN_MEMORY) {
      for (i = start; i <= end; i += inc) {
	float tmp = FSAMPLE(s, i);
	if (tmp > maxs) {
	  maxs = tmp;
	}
	if (tmp < mins) {
	  mins = tmp;
	}
      }
    } else {
      for (i = start; i <= end; i += inc) {
	float tmp = GetSample(info, i);
	if (tmp > maxs) {
	  maxs = tmp;
	}
	if (tmp < mins) {
	  mins = tmp;
	}
      }
    }
  } else {
    if (s->storeType == SOUND_IN_MEMORY) {
      for (i = start; i <= end; i += inc) {
	float tmp = (float) DSAMPLE(s, i);
	if (tmp > maxs) {
	  maxs = tmp;
	}
	if (tmp < mins) {
	  mins = tmp;
	}
      }
    } else {
      for (i = start; i <= end; i += inc) {
	float tmp = GetSample(info, i);
	if (tmp > maxs) {
	  maxs = tmp;
	}
	if (tmp < mins) {
	  mins = tmp;
	}
      }
    }
  }
  if (maxs < mins) {
    maxs = mins;
  }
  if (mins > maxs) {
    mins = maxs;
  }

  *pmax = maxs;
  *pmin = mins;
}

void
Snack_UpdateExtremes(Sound *s, int start, int end, int flag)
{
  float maxs, mins, newmax, newmin;

  if (flag == SNACK_NEW_SOUND) {
    s->maxsamp = -32768.0f;
    s->minsamp =  32767.0f;
  }

  maxs = s->maxsamp;
  mins = s->minsamp;

  Snack_GetExtremes(s, NULL, start, end - 1, -1, &newmax, &newmin);

  if (newmax > maxs) {
    s->maxsamp = newmax;
  } else {
    s->maxsamp = maxs;
  }
  if (newmin < mins) {
    s->minsamp = newmin;
  } else {
    s->minsamp = mins;
  }
  if (s->maxsamp > -s->minsamp)
    s->abmax = s->maxsamp;
  else
    s->abmax = -s->minsamp;
}

short
Snack_SwapShort(short s)
{
  char tc, *p;

  p = (char *) &s;
  tc = *p;
  *p = *(p+1);
  *(p+1) = tc;
  
  return(s);
}

long
Snack_SwapLong(long l)
{
  char tc, *p;

  p = (char *) &l;
  tc = *p;
  *p = *(p+3);
  *(p+3) = tc;

  tc = *(p+1);
  *(p+1) = *(p+2);
  *(p+2) = tc;
  
  return(l);
}

float
Snack_SwapFloat(float f)
{
  char tc, *p;

  p = (char *) &f;
  tc = *p;
  *p = *(p+3);
  *(p+3) = tc;

  tc = *(p+1);
  *(p+1) = *(p+2);
  *(p+2) = tc;
  
  return(f);
}

double
Snack_SwapDouble(double d)
{
  char tc, *p;

  p = (char *) &d;
  tc = *p;
  *p = *(p+7);
  *(p+7) = tc;

  tc = *(p+1);
  *(p+1) = *(p+6);
  *(p+6) = tc;

  tc = *(p+2);
  *(p+2) = *(p+5);
  *(p+5) = tc;

  tc = *(p+3);
  *(p+3) = *(p+4);
  *(p+4) = tc;
  
  return(d);
}

extern struct Snack_FileFormat *snackFileFormats;

void
Snack_DeleteSound(Sound *s)
{
  jkCallback *currCB;
  Snack_FileFormat *ff;

  if (s->debug > 1) {
    Snack_WriteLog("  Enter Snack_DeleteSound\n");
  }

  Snack_ResizeSoundStorage(s, 0);
  ckfree((char *) s->blocks);
  if (s->storeType == SOUND_IN_FILE && s->linkInfo.linkCh != NULL) {
    CloseLinkedFile(&s->linkInfo);
  }

  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
    if (strcmp(s->fileType, ff->name) == 0) {
      if (ff->freeHeaderProc != NULL) {
	(ff->freeHeaderProc)(s);
      }
    }
  }

  if (s->fcname != NULL) {
    ckfree((char *)s->fcname);
  }
  if (s->filterName != NULL) {
    ckfree(s->filterName);
  }

  Snack_ExecCallbacks(s, SNACK_DESTROY_SOUND);
  currCB = s->firstCB;
  while (currCB != NULL) {
    if (s->debug > 1) Snack_WriteLogInt("  Freed callback", currCB->id);
    ckfree((char *)currCB);
    currCB = currCB->next;
  }

  if (s->changeCmdPtr != NULL) {
    Tcl_DecrRefCount(s->changeCmdPtr);
  }

  if (s->cmdPtr != NULL) {
    Tcl_DecrRefCount(s->cmdPtr);
  }

  if (s->debug > 1) {
    Snack_WriteLog("  Sound object freed\n");
  }

  ckfree((char *) s);
}

int
Snack_ResizeSoundStorage(Sound *s, int len)
{
  int neededblks, i, blockSize, sampSize;

  if (s->debug > 1) Snack_WriteLogInt("  Enter ResizeSoundStorage", len);

  if (s->precision == SNACK_SINGLE_PREC) {
    blockSize = FBLKSIZE;
    sampSize = sizeof(float);
  } else {
    blockSize = DBLKSIZE;
    sampSize = sizeof(double);
  }
  neededblks = 1 + (len * s->nchannels - 1) / blockSize;
  
  if (len == 0) {
    neededblks = 0;
    s->exact = 0;
  }

  if (neededblks > s->maxblks) {
    void *tmp = ckrealloc((char *)s->blocks, neededblks * sizeof(float*));

    if (tmp == NULL) {
      if (s->debug > 2) Snack_WriteLogInt("    realloc failed", neededblks);
      return TCL_ERROR;
    }
    s->maxblks = neededblks;
    s->blocks = (float **)tmp;
  }
  
  if (s->maxlength == 0 && len * s->nchannels < blockSize) {
    
    /* Allocate exactly as much as needed. */

    if (s->debug > 2) Snack_WriteLogInt("    Allocating minimal block",
					len*s->nchannels * sizeof(float));

    s->exact = len * s->nchannels * sampSize;
    if ((s->blocks[0] = (float *) ckalloc(s->exact)) == NULL) {
      return TCL_ERROR;
    }
    i = 1;
    s->maxlength = len;
  } else if (neededblks > s->nblks) {
    float *tmp = s->blocks[0];

    if (s->debug > 2) {
      Snack_WriteLogInt("    Allocating full block(s)", neededblks - s->nblks);
    }

    /* Do not count exact block, needs to be re-allocated */
    if (s->exact > 0) {
      s->nblks = 0;
    }

    for (i = s->nblks; i < neededblks; i++) {
      if ((s->blocks[i] = (float *) ckalloc(CBLKSIZE)) == NULL) {
	break;
      }
    }
    if (i < neededblks) {
      if (s->debug > 2) Snack_WriteLogInt("    block alloc failed", i);
      for (--i; i >= s->nblks; i--) {
	ckfree((char *) s->blocks[i]);
      }
      return TCL_ERROR;
    }

    /* Copy and de-allocate any exact block */
    if (s->exact > 0) {
      memcpy(s->blocks[0], tmp, s->exact);
      ckfree((char *) tmp);
      s->exact = 0;
    }

    s->maxlength = neededblks * blockSize / s->nchannels;
  } else if (neededblks == 1 && s->exact > 0) {

    /* Reallocate to one full block */

    float *tmp = (float *) ckalloc(CBLKSIZE);

    if (s->debug > 2) {
      Snack_WriteLogInt("    Reallocating full block", CBLKSIZE);
    }

    if (tmp != NULL) {
      memcpy(tmp, s->blocks[0], s->exact);
      ckfree((char *) s->blocks[0]);
      s->blocks[0] = tmp;
      s->maxlength = blockSize / s->nchannels;
    }
    s->exact = 0;
  }
  
  if (neededblks < s->nblks) {
    for (i = neededblks; i < s->nblks; i++) {
      ckfree((char *) s->blocks[i]);
    }
    s->maxlength = neededblks * blockSize / s->nchannels;
  }

  s->nblks = neededblks;

  if (s->debug > 1) Snack_WriteLogInt("  Exit ResizeSoundStorage", neededblks);

  return TCL_OK;
}

char *encs[] = { "", "Lin16", "Alaw", "Mulaw", "Lin8offset", "Lin8",
		  "Lin24", "Lin32", "Float", "Double", "Lin24packed" };

int
GetChannels(Tcl_Interp *interp, Tcl_Obj *obj, int *nchannels)
{
  int length, val;
  char *str = Tcl_GetStringFromObj(obj, &length);

  if (strncasecmp(str, "MONO", length) == 0) {
    *nchannels = SNACK_MONO;
    return TCL_OK;
  }
  if (strncasecmp(str, "STEREO", length) == 0) {
    *nchannels = SNACK_STEREO;
    return TCL_OK;
  }
  if (strncasecmp(str, "QUAD", length) == 0) {
    *nchannels = SNACK_QUAD;
    return TCL_OK;
  }
  if (Tcl_GetIntFromObj(interp, obj, &val) != TCL_OK) return TCL_ERROR;
  if (val < 1) {
    Tcl_AppendResult(interp, "Number of channels must be >= 1", NULL);
    return TCL_ERROR;
  }
  *nchannels = val;
  return TCL_OK;
}

int
GetEncoding(Tcl_Interp *interp, Tcl_Obj *obj, int *encoding, int *sampsize)
{
  int length;
  char *str = Tcl_GetStringFromObj(obj, &length);

  if (strncasecmp(str, "LIN16", length) == 0) {
    *encoding = LIN16;
    *sampsize = 2;
  } else if (strncasecmp(str, "LIN24", length) == 0) {
    *encoding = LIN24;
    *sampsize = 4;
  } else if (strncasecmp(str, "LIN24PACKED", length) == 0) {
    *encoding = LIN24PACKED;
    *sampsize = 3;
  } else if (strncasecmp(str, "LIN32", length) == 0) {
    *encoding = LIN32;
    *sampsize = 4;
  } else if (strncasecmp(str, "FLOAT", length) == 0) {
    *encoding = SNACK_FLOAT;
    *sampsize = 4;
  } else if (strncasecmp(str, "DOUBLE", length) == 0) {
    *encoding = SNACK_DOUBLE;
    *sampsize = 8;
  } else if (strncasecmp(str, "ALAW", length) == 0) {
    *encoding = ALAW;
    *sampsize = 1;
  } else if (strncasecmp(str, "MULAW", length) == 0) {
    *encoding = MULAW;
    *sampsize = 1;
  } else if (strncasecmp(str, "LIN8", length) == 0) {
    *encoding = LIN8;
    *sampsize = 1;
  } else if (strncasecmp(str, "LIN8OFFSET", length) == 0) {
    *encoding = LIN8OFFSET;
    *sampsize = 1;
  } else {
    Tcl_AppendResult(interp, "Unknown encoding", NULL);
    return TCL_ERROR;
  }
  return TCL_OK;
}

void
SwapIfBE(Sound *s)
{
  if (littleEndian) {
    s->swap = 0;
  } else {
    s->swap = 1;
  }
}

void
SwapIfLE(Sound *s)
{
  if (littleEndian) {
    s->swap = 1;
  } else {
    s->swap = 0;
  }
}

static int
infoCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  Tcl_Obj *objs[8];

  objs[0] = Tcl_NewIntObj(s->length);
  objs[1] = Tcl_NewIntObj(s->samprate);
  if (s->encoding == SNACK_FLOAT) {
    objs[2] = Tcl_NewDoubleObj((double)s->maxsamp);
    objs[3] = Tcl_NewDoubleObj((double)s->minsamp);
  } else {
    objs[2] = Tcl_NewIntObj((int)s->maxsamp);
    objs[3] = Tcl_NewIntObj((int)s->minsamp);
  }
  objs[4] = Tcl_NewStringObj(encs[s->encoding], -1);
  objs[5] = Tcl_NewIntObj(s->nchannels);
  objs[6] = Tcl_NewStringObj(s->fileType, -1);
  objs[7] = Tcl_NewIntObj(s->headSize);

  Tcl_SetObjResult(interp, Tcl_NewListObj(8, objs));
  return TCL_OK;
}

static int
maxCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int startpos = 0, endpos = s->length - 1, arg, channel = -1;
  float maxsamp, minsamp;
  SnackLinkedFileInfo info;
  static CONST84 char *subOptionStrings[] = {
    "-start", "-end", "-channel", NULL
  };
  enum subOptions {
    START, END, CHANNEL
  };

  for (arg = 2; arg < objc; arg+=2) {
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
    case CHANNEL:
      {
	char *str = Tcl_GetStringFromObj(objv[arg+1], NULL);
	if (GetChannel(interp, str, s->nchannels, &channel) != TCL_OK) {
	  return TCL_ERROR;
	  break;
	}
      }
    }
  }
  if (endpos < 0) endpos = s->length - 1;

  if (startpos < 0 || (startpos >= s->length && startpos > 0)) {
    Tcl_AppendResult(interp, "Start value out of bounds", NULL);
    return TCL_ERROR;
  }
  if (endpos >= s->length) {
    Tcl_AppendResult(interp, "End value out of bounds", NULL);
    return TCL_ERROR;
  }

  if (objc == 2) {
    if (s->encoding == SNACK_FLOAT) {
      Tcl_SetObjResult(interp, Tcl_NewDoubleObj((double)s->maxsamp));
    } else {
      Tcl_SetObjResult(interp, Tcl_NewIntObj((int)s->maxsamp));
    }
  } else {
    if (s->storeType != SOUND_IN_MEMORY) {
      OpenLinkedFile(s, &info);
    }
    Snack_GetExtremes(s, &info, startpos, endpos, channel, &maxsamp, &minsamp);
    if (s->storeType != SOUND_IN_MEMORY) {
      CloseLinkedFile(&info);
    }
    if (s->encoding == SNACK_FLOAT) {
      Tcl_SetObjResult(interp, Tcl_NewDoubleObj((double)maxsamp));
    } else {
      Tcl_SetObjResult(interp, Tcl_NewIntObj((int)maxsamp));
    }
  }

  return TCL_OK;
}

static int
minCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int startpos = 0, endpos = s->length - 1, arg, channel = -1;
  float maxsamp, minsamp;
  SnackLinkedFileInfo info;
  static CONST84 char *subOptionStrings[] = {
    "-start", "-end", "-channel", NULL
  };
  enum subOptions {
    START, END, CHANNEL
  };

  for (arg = 2; arg < objc; arg+=2) {
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
    case CHANNEL:
      {
	char *str = Tcl_GetStringFromObj(objv[arg+1], NULL);
	if (GetChannel(interp, str, s->nchannels, &channel) != TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    }
  }
  if (endpos < 0) endpos = s->length - 1;

  if (startpos < 0 || (startpos >= s->length && startpos > 0)) {
    Tcl_AppendResult(interp, "Start value out of bounds", NULL);
    return TCL_ERROR;
  }
  if (endpos >= s->length) {
    Tcl_AppendResult(interp, "End value out of bounds", NULL);
    return TCL_ERROR;
  }

  if (objc == 2) {
    if (s->encoding == SNACK_FLOAT) {
      Tcl_SetObjResult(interp, Tcl_NewDoubleObj((double)s->minsamp));
    } else {
      Tcl_SetObjResult(interp, Tcl_NewIntObj((int)s->minsamp));
    }
  } else {
    if (s->storeType != SOUND_IN_MEMORY) {
      OpenLinkedFile(s, &info);
    }
    Snack_GetExtremes(s, &info, startpos, endpos, channel, &maxsamp, &minsamp);
    if (s->storeType != SOUND_IN_MEMORY) {
      CloseLinkedFile(&info);
    }
    if (s->encoding == SNACK_FLOAT) {
      Tcl_SetObjResult(interp, Tcl_NewDoubleObj((double)minsamp));
    } else {
      Tcl_SetObjResult(interp, Tcl_NewIntObj((int)minsamp));
    }
  }

  return TCL_OK;
}

static int
changedCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "changed new|more");
    return TCL_ERROR;
  }
  if (s->storeType == SOUND_IN_MEMORY) {
    Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
  }
  if (objc > 2) {
    char *string = Tcl_GetStringFromObj(objv[2], NULL);
	
    if (strcasecmp(string, "new") == 0) {
      Snack_ExecCallbacks(s, SNACK_NEW_SOUND);
      return TCL_OK;
    }
    if (strcasecmp(string, "more") == 0) {
      Snack_ExecCallbacks(s, SNACK_MORE_SOUND);
      return TCL_OK;
    }
    Tcl_AppendResult(interp, "unknow option, must be new or more",
		     (char *) NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

static int
destroyCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  char *string = Tcl_GetStringFromObj(objv[0], NULL);
  int debug = s->debug;

  if (debug > 0) Snack_WriteLog("Enter destroyCmd\n");

  if (s->writeStatus == WRITE) {
    s->destroy = 1;
  }
  s->length = 0;
  if (wop == IDLE) {
    Snack_StopSound(s, interp);
  }
  Tcl_DeleteHashEntry(Tcl_FindHashEntry(s->soundTable, string));

  Tcl_DeleteCommand(interp, string);

  /*
    The sound command and associated Sound struct are now deallocated
    because SoundDeleteCmd has been called as a result of Tcl_DeleteCommand().
   */

  if (debug > 0) Snack_WriteLog("Exit destroyCmd\n");

  return TCL_OK;
}

int
flushCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "flush only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  Snack_StopSound(s, interp);
  Snack_ResizeSoundStorage(s, 0);
  s->length  = 0;
  s->maxsamp = 0.0f;
  s->minsamp = 0.0f;
  s->abmax   = 0.0f;
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);

  return TCL_OK;
}

static int
configureCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int arg, filearg = 0, newobjc;
  Tcl_Obj **newobjv = NULL;
  static CONST84 char *optionStrings[] = {
    "-load", "-file", "-channel", "-rate", "-frequency", "-channels",
    "-encoding", "-format", "-byteorder", "-buffersize", "-skiphead",
    "-guessproperties", "-precision", "-changecommand", "-fileformat",
    "-debug", NULL
  };
  enum options {
    OPTLOAD, OPTFILE, CHANNEL, RATE, FREQUENCY, CHANNELS, ENCODING, FORMAT,
    BYTEORDER, BUFFERSIZE, SKIPHEAD, GUESSPROPS, PRECISION, CHGCMD, FILEFORMAT,
    OPTDEBUG
  };
  Snack_FileFormat *ff;
  
  if (s->debug > 0) { Snack_WriteLog("Enter configureCmd\n"); }

  Snack_RemoveOptions(objc-2, objv+2, optionStrings, &newobjc,
		      (Tcl_Obj **) &newobjv);
  if (newobjc > 0) {
    for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
      if (strcmp(s->fileType, ff->name) == 0) {
	if (ff->configureProc != NULL) {
	  if ((ff->configureProc)(s, interp, objc, objv)) return TCL_OK;
	}
      }
    }
  }
  for (arg = 0; arg <newobjc; arg++) {
    Tcl_DecrRefCount(newobjv[arg]);
  }
  ckfree((char *)newobjv);

  if (objc == 2) { /* get all options */
    Tcl_Obj *objs[6];
    
    objs[0] = Tcl_NewIntObj(s->length);
    objs[1] = Tcl_NewIntObj(s->samprate);
    if (s->encoding == SNACK_FLOAT) {
      objs[2] = Tcl_NewDoubleObj((double)s->maxsamp);
      objs[3] = Tcl_NewDoubleObj((double)s->minsamp);
    } else {
      objs[2] = Tcl_NewIntObj((int)s->maxsamp);
      objs[3] = Tcl_NewIntObj((int)s->minsamp);
    }
    objs[4] = Tcl_NewStringObj(encs[s->encoding], -1);
    objs[5] = Tcl_NewIntObj(s->nchannels);
    
    Tcl_SetObjResult(interp, Tcl_NewListObj(6, objs));

    return TCL_OK;
  } else if (objc == 3) { /* get option */
    int index;

    if (Tcl_GetIndexFromObj(interp, objv[2], optionStrings, "option", 0,
			    &index) != TCL_OK) {
      return TCL_ERROR;
    }

    switch ((enum options) index) {
    case OPTLOAD:
      {
	if (s->storeType == SOUND_IN_MEMORY) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj(s->fcname, -1));
	} else {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
	}
	break;
      }
    case OPTFILE:
      {
	if (s->storeType == SOUND_IN_FILE) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj(s->fcname, -1));
	} else {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
	}
	break;
      }
    case CHANNEL:
      {
	if (s->storeType == SOUND_IN_CHANNEL) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj(s->fcname, -1));
	} else {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
	}
	break;
      }
    case RATE:
    case FREQUENCY:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(s->samprate));
	break;
      }
    case CHANNELS:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(s->nchannels));
	break;
      }
    case ENCODING:
    case FORMAT:
      {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(encs[s->encoding], -1));
	break;
      }
    case BYTEORDER:
      if (s->sampsize > 1) {
	if (littleEndian) {
	  if (s->swap) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("bigEndian", -1));
	  } else {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("littleEndian", -1));
	  }
	} else {
	  if (s->swap) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("littleEndian", -1));
	  } else {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("bigEndian", -1));
	  }
	}
      } else {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
      }
      break;
    case BUFFERSIZE:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(s->buffersize));
	break;
      }
    case SKIPHEAD:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(s->skipBytes));
	break;
      }
    case GUESSPROPS:
      break;
    case PRECISION:
      {
	if (s->precision == SNACK_DOUBLE_PREC) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("double", -1));
	} else {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("single", -1));
	}
	break;
      }
    case CHGCMD:
      {
	Tcl_SetObjResult(interp, s->changeCmdPtr);
	break;
      }
    case FILEFORMAT:
      {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(s->fileType, -1));
	break;
      }
    case OPTDEBUG:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(s->debug));
	break;
      }
    }
  } else { /* set option */

    s->guessEncoding = -1;
    s->guessRate = -1;

    for (arg = 2; arg < objc; arg+=2) {
      int index;

      if (Tcl_GetIndexFromObj(interp, objv[arg], optionStrings, "option", 0,
			      &index) != TCL_OK) {
	return TCL_ERROR;
      }
      
      if (arg + 1 == objc) {
	Tcl_AppendResult(interp, "No argument given for ",
			 optionStrings[index], " option", (char *) NULL);
	return TCL_ERROR;
      }
    
      switch ((enum options) index) {
      case OPTLOAD:
	{
	  filearg = arg + 1;
	  s->storeType = SOUND_IN_MEMORY;
	  break;
	}
      case OPTFILE:
	{
	  filearg = arg + 1;
	  s->storeType = SOUND_IN_FILE;
	  break;
	}
      case CHANNEL:
	{
	  filearg = arg + 1;
	  s->storeType = SOUND_IN_CHANNEL;
	  break;
	}
      case RATE:
      case FREQUENCY:
	{
	  if (Tcl_GetIntFromObj(interp, objv[arg+1], &s->samprate) != TCL_OK)
	    return TCL_ERROR;
	  s->guessRate = 0;
	  break;
	}
      case CHANNELS:
	{
	  int oldn = s->nchannels;

	  if (GetChannels(interp, objv[arg+1], &s->nchannels) != TCL_OK)
	    return TCL_ERROR;
	  if (oldn != s->nchannels) {
	    s->length = s->length * oldn / s->nchannels;
	  }
	  break;
	}
      case ENCODING:
      case FORMAT:
	{
	  if (GetEncoding(interp, objv[arg+1], &s->encoding, &s->sampsize) \
	      != TCL_OK) {
	    return TCL_ERROR;
	  }
	  s->guessEncoding = 0;
	  break;
	}
      case BYTEORDER:
	{
	  int length;
	  char *str = Tcl_GetStringFromObj(objv[arg+1], &length);
	  if (strncasecmp(str, "littleEndian", length) == 0) {
	    SwapIfBE(s);
	  } else if (strncasecmp(str, "bigEndian", length) == 0) {
	    SwapIfLE(s);
	  } else {
	    Tcl_AppendResult(interp, "-byteorder option should be bigEndian",
			     " or littleEndian", NULL);
	    return TCL_ERROR;
	  }
	  s->guessEncoding = 0;
	  break;
	}
      case BUFFERSIZE:
	{
	  if (Tcl_GetIntFromObj(interp, objv[arg+1], &s->buffersize) != TCL_OK)
	    return TCL_ERROR;   
	  break;
	}
      case SKIPHEAD: 
	{
	  if (Tcl_GetIntFromObj(interp, objv[arg+1], &s->skipBytes) != TCL_OK)
	    return TCL_ERROR;
	  break;
	}
      case GUESSPROPS:
	{
	  int guessProps;
	  if (Tcl_GetBooleanFromObj(interp, objv[arg+1], &guessProps) !=TCL_OK)
	    return TCL_ERROR;
	  if (guessProps) {
	    if (s->guessEncoding == -1) s->guessEncoding = 1;
	    if (s->guessRate == -1) s->guessRate = 1;
	  }
	  break;
	}
      case PRECISION:
	{
	  int length;
	  char *str = Tcl_GetStringFromObj(objv[arg+1], &length);
	  if (strncasecmp(str, "double", length) == 0) {
	    s->precision = SNACK_DOUBLE_PREC;
	  } else if (strncasecmp(str, "single", length) == 0) {
	    s->precision = SNACK_SINGLE_PREC;
	  } else {
	    Tcl_AppendResult(interp, "-precision option should be single",
			     " or double", NULL);
	    return TCL_ERROR;
	  }
	  break;
	}
      case CHGCMD:
	{
	  if (s->changeCmdPtr != NULL) {
	    Tcl_DecrRefCount(s->changeCmdPtr);
	  }
	  s->changeCmdPtr = Tcl_DuplicateObj(objv[arg+1]);
	  Tcl_IncrRefCount(s->changeCmdPtr);
	  break;
	}
      case FILEFORMAT:
	{
	  if (strlen(Tcl_GetStringFromObj(objv[arg+1], NULL)) > 0) {
	    if (GetFileFormat(interp, objv[arg+1], &s->fileType) != TCL_OK) {
	      return TCL_ERROR;
	    }
	    s->forceFormat = 1;
	  }
	  break;
      }
      case OPTDEBUG:
	{
	  if (arg+1 == objc) {
	    Tcl_AppendResult(interp, "No debug flag given", NULL);
	    return TCL_ERROR;
	  }
	  if (Tcl_GetIntFromObj(interp, objv[arg+1], &s->debug) != TCL_OK) {
	    return TCL_ERROR;
	  }
	  break;
	}
      }
    }
    if (s->guessEncoding == -1) s->guessEncoding = 0;
    if (s->guessRate == -1) s->guessRate = 0;

    if (filearg > 0) {
      if (Tcl_IsSafe(interp)) {
	Tcl_AppendResult(interp, "can not read sound file in a safe",
			 " interpreter", (char *) NULL);
	return TCL_ERROR;
      }
      if (SetFcname(s, interp, objv[filearg]) != TCL_OK) {
	return TCL_ERROR;
      }
    }

    if (filearg > 0 && strlen(s->fcname) > 0) {
      if (s->storeType == SOUND_IN_MEMORY) {
	char *type = LoadSound(s, interp, NULL, 0, -1);
	
	if (type == NULL) {
	  return TCL_ERROR;
	}
	Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
      } else if (s->storeType == SOUND_IN_FILE) {
	Snack_FileFormat *ff;

	if (s->linkInfo.linkCh != NULL) {
	  CloseLinkedFile(&s->linkInfo);
	  s->linkInfo.linkCh = NULL;
	}
	for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
	  if (strcmp(s->fileType, ff->name) == 0) {
	    if (ff->freeHeaderProc != NULL) {
	      (ff->freeHeaderProc)(s);
	    }
	  }
	}
	if (GetHeader(s, interp, NULL) != TCL_OK) {
	  s->fileType = NameGuessFileType(s->fcname);
	}
	Snack_ResizeSoundStorage(s, 0);
	if (s->encoding == LIN8OFFSET) {
	  s->maxsamp = 128.0f;
	  s->minsamp = 128.0f;
	} else {
	  s->maxsamp = 0.0f;
	  s->minsamp = 0.0f;
	}
      } else if (s->storeType == SOUND_IN_CHANNEL) {
	int mode = 0;

	Snack_ResizeSoundStorage(s, 0);
	s->rwchan = Tcl_GetChannel(interp, s->fcname, &mode);
	if (!(mode & TCL_READABLE)) {
	  s->rwchan = NULL;
	}
	if (s->rwchan != NULL) {
	  Tcl_SetChannelOption(interp, s->rwchan, "-translation", "binary");
#ifdef TCL_81_API
	  Tcl_SetChannelOption(interp, s->rwchan, "-encoding", "binary");
#endif
	}
      }
    }
    if (filearg > 0 && strlen(s->fcname) == 0) {
      if (s->storeType == SOUND_IN_FILE) {
	s->length = 0;
      }
    }
    Snack_ExecCallbacks(s, SNACK_NEW_SOUND);
  }
  if (s->debug > 0) { Snack_WriteLog("Exit configureCmd\n"); }

  return TCL_OK;
}

static int
cgetCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  static CONST84 char *optionStrings[] = {
    "-load", "-file", "-channel", "-rate", "-frequency", "-channels",
    "-encoding", "-format", "-byteorder", "-buffersize", "-skiphead",
    "-guessproperties", "-precision", "-changecommand", "-fileformat",
    "-debug", NULL
  };
  enum options {
    OPTLOAD, OPTFILE, CHANNEL, RATE, FREQUENCY, CHANNELS, ENCODING, FORMAT,
    BYTEORDER, BUFFERSIZE, SKIPHEAD, GUESSPROPS, PRECISION, CHGCMD, FILEFORMAT,
    OPTDEBUG
  };

  if (objc == 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "cget option");
    return TCL_ERROR;
  } else if (objc == 3) { /* get option */
    int index;

    if (Tcl_GetIndexFromObj(interp, objv[2], optionStrings, "option", 0,
			    &index) != TCL_OK) {
      return TCL_ERROR;
    }

    switch ((enum options) index) {
    case OPTLOAD:
      {
	if (s->storeType == SOUND_IN_MEMORY) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj(s->fcname, -1));
	} else {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
	}
	break;
      }
    case OPTFILE:
      {
	if (s->storeType == SOUND_IN_FILE) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj(s->fcname, -1));
	} else {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
	}
	break;
      }
    case CHANNEL:
      {
	if (s->storeType == SOUND_IN_CHANNEL) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj(s->fcname, -1));
	} else {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
	}
	break;
      }
    case RATE:
    case FREQUENCY:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(s->samprate));
	break;
      }
    case CHANNELS:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(s->nchannels));
	break;
      }
    case ENCODING:
    case FORMAT:
      {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(encs[s->encoding], -1));
	break;
      }
    case BYTEORDER:
      if (s->sampsize > 1) {
	if (littleEndian) {
	  if (s->swap) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("bigEndian", -1));
	  } else {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("littleEndian", -1));
	  }
	} else {
	  if (s->swap) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("littleEndian", -1));
	  } else {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("bigEndian", -1));
	  }
	}
      } else {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
      }
      break;
    case BUFFERSIZE:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(s->buffersize));
	break;
      }
    case SKIPHEAD:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(s->skipBytes));
	break;
      }
    case GUESSPROPS:
      break;
    case CHGCMD:
      {
	Tcl_SetObjResult(interp, s->changeCmdPtr);
	break;
      }
    case PRECISION:
      {
	if (s->precision == SNACK_DOUBLE_PREC) {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("double", -1));
	} else {
	  Tcl_SetObjResult(interp, Tcl_NewStringObj("single", -1));
	}
	break;
      }
    case FILEFORMAT:
      {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(s->fileType, -1));
	break;
      }
    case OPTDEBUG:
      {
	Tcl_SetObjResult(interp, Tcl_NewIntObj(s->debug));
	break;
      }
    }
  }

  return TCL_OK;
}

int filterSndCmd(Sound *s, Tcl_Interp *interp, int objc,
		 Tcl_Obj *CONST objv[]);

#define NSOUNDCOMMANDS   45
#define MAXSOUNDCOMMANDS 100

static int nSoundCommands   = NSOUNDCOMMANDS;
static int maxSoundCommands = MAXSOUNDCOMMANDS;

CONST84 char *sndCmdNames[MAXSOUNDCOMMANDS] = {
  "play",
  "read",
  "record",
  "stop",
  "write",

  "data",
  "crop",
  "info",
  "length",
  "current_position",

  "max",
  "min",
  "sample",
  "changed",
  "copy",

  "append",
  "concatenate",
  "insert",
  "cut",
  "destroy",

  "flush",
  "configure",
  "cget",
  "pause",
  "convert",

  "dBPowerSpectrum",
  "pitch",
  "reverse",
  "shape",
  "datasamples",

  "filter",
  "swap",
  "power",
  "formant",
  "speatures",

  "an",
  "mix",
  "stretch",
  "co",
  "powerSpectrum",

  "vp",
  "join",
  "lastIndex",
  "fit",
  "ina",

  NULL
};

/* NOTE: NSOUNDCOMMANDS needs updating when new commands are added. */

soundCmd *sndCmdProcs[MAXSOUNDCOMMANDS] = {
  playCmd,
  readCmd,
  recordCmd,
  stopCmd,
  writeCmd,
  dataCmd,
  cropCmd,
  infoCmd,
  lengthCmd,
  current_positionCmd,
  maxCmd,
  minCmd,
  sampleCmd,
  changedCmd,
  copyCmd,
  appendCmd,
  concatenateCmd,
  insertCmd,
  cutCmd,
  destroyCmd,
  flushCmd,
  configureCmd,
  cgetCmd,
  pauseCmd,
  convertCmd,
  dBPowerSpectrumCmd,
  pitchCmd,
  reverseCmd,
  shapeCmd,
  dataSamplesCmd,
  filterSndCmd,
  swapCmd,
  powerCmd,
  formantCmd,
  speaturesCmd,
  alCmd,
  mixCmd,
  stretchCmd,
  ocCmd,
  powerSpectrumCmd,
  vpCmd,
  joinCmd,
  lastIndexCmd,
  fitCmd,
  inaCmd
};

soundDelCmd *sndDelCmdProcs[MAXSOUNDCOMMANDS] = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

#ifdef __cplusplus
extern "C"
#endif
int
SoundCmd(ClientData clientData, Tcl_Interp *interp, int objc,
	 Tcl_Obj *CONST objv[])
{
  register Sound *s = (Sound *) clientData;
  int index;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "option ?args?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], sndCmdNames, "option", 0,
			  &index) != TCL_OK) {
    return TCL_ERROR;
  }

  return((sndCmdProcs[index])(s, interp, objc, objv)); 
}

Sound *
Snack_NewSound(int rate, int encoding, int nchannels)
{
  Sound *s = (Sound *) ckalloc(sizeof(Sound));

  if (s == NULL) {
    return NULL;
  }

  /* Default sound specifications */

  s->samprate = rate;
  s->encoding = encoding;
  if (s->encoding == LIN16) {
    s->sampsize = 2;
  } else if (s->encoding == LIN24 || s->encoding == LIN32
	     || s->encoding == SNACK_FLOAT) {
    s->sampsize = 4;
  } else if (s->encoding == LIN24PACKED) {
    s->sampsize = 3;
  } else {
    s->sampsize = 1;
  }
  if (s->encoding == LIN8OFFSET) {
    s->maxsamp = 128.0f;
    s->minsamp = 128.0f;
  } else {
    s->maxsamp = 0.0f;
    s->minsamp = 0.0f;
  }
  s->nchannels = nchannels;
  s->length    = 0;
  s->maxlength = 0;
  s->abmax     = 0.0f;
  s->readStatus = IDLE;
  s->writeStatus = IDLE;
  s->firstCB   = NULL;
  s->fileType  = RAW_STRING;
  s->tmpbuf    = NULL;
  s->swap      = 0;
  s->headSize  = 0;
  s->skipBytes = 0;
  s->storeType = SOUND_IN_MEMORY;
  s->fcname    = NULL;
  s->interp    = NULL;
  s->cmdPtr    = NULL;
  s->blocks    = (float **) ckalloc(MAXNBLKS * sizeof(float*));
  if (s->blocks == NULL) {
    ckfree((char *) s);
    return NULL;
  }
  s->blocks[0] = NULL;
  s->maxblks   = MAXNBLKS;
  s->nblks     = 0;
  s->exact     = 0;
  s->precision = SNACK_SINGLE_PREC;
  s->blockingPlay = 0;
  s->debug     = 0;
  s->destroy   = 0;
  s->guessEncoding = 0;
  s->guessRate = 0;
  s->rwchan     = NULL;
  s->firstNRead = 0;
  s->buffersize = 0;
  s->forceFormat = 0;
  s->itemRefCnt = 0;
  s->validStart = 0;
  s->linkInfo.linkCh = NULL;
  s->linkInfo.eof = 0;
  s->inByteOrder = SNACK_NATIVE;
  s->devStr = NULL;
  s->soundTable = NULL;
  s->filterName = NULL;
  s->extHead    = NULL;
  s->extHeadType = 0;
  s->extHead2   = NULL;
  s->extHead2Type = 0;
  s->loadOffset = 0;
  s->changeCmdPtr = NULL;
  s->userFlag   = 0;
  s->userData   = NULL;

  return s;
}

void
CleanSound(Sound *s, Tcl_Interp *interp, char *name)
{
  Snack_DeleteSound(s);
  Tcl_DeleteHashEntry(Tcl_FindHashEntry(s->soundTable, name));
}

extern int defaultSampleRate;

int
ParseSoundCmd(ClientData cdata, Tcl_Interp *interp, int objc,
	      Tcl_Obj *CONST objv[], char** namep, Sound** sp)
{
  Sound *s;
  int arg, arg1, filearg = 0, flag;
  static int id = 0;
  int samprate = defaultSampleRate, nchannels = 1;
  int encoding = LIN16, sampsize = 2;
  int storeType = -1, guessEncoding = -1, guessRate = -1;
  int forceFormat = -1, skipBytes = -1, buffersize = -1;
  int guessProps = 0, swapIfBE = -1, debug = -1, precision = -1;
  char *fileType = NULL;
  static char ids[20];
  char *name;
  Tcl_HashTable *hTab = (Tcl_HashTable *) cdata;
  Tcl_HashEntry *hPtr;
  int length = 0;
  char *string = NULL;
  Tcl_Obj *cmdPtr = NULL;
  static CONST84 char *optionStrings[] = {
    "-load", "-file", "-rate", "-frequency", "-channels", "-encoding",
    "-format", "-channel", "-byteorder", "-buffersize", "-skiphead",
    "-guessproperties", "-fileformat", "-precision", "-changecommand",
    "-debug", NULL
  };
  enum options {
    OPTLOAD, OPTFILE, RATE, FREQUENCY, CHANNELS, ENCODING, FORMAT, CHANNEL,
    BYTEORDER, BUFFERSIZE, SKIPHEAD, GUESSPROPS, FILEFORMAT, 
    PRECISION, CHGCMD, OPTDEBUG
  };

  if (objc > 1) {
    string = Tcl_GetStringFromObj(objv[1], &length);
  }
  if ((objc == 1) || (string[0] == '-')) {
    do {
      sprintf(ids, "sound%d", ++id);
    } while (Tcl_FindHashEntry(hTab, ids) != NULL);
    name = ids;
    arg1 = 1;
  } else {
    name = string;
    arg1 = 2;
  }
  *namep = name;

  hPtr = Tcl_FindHashEntry(hTab, name);
  if (hPtr != NULL) {
    Sound *t = (Sound *) Tcl_GetHashValue(hPtr);
    Snack_StopSound(t, interp);
    Tcl_DeleteCommand(interp, name);
  }

  for (arg = arg1; arg < objc; arg += 2) {
    int index;

    if (Tcl_GetIndexFromObj(interp, objv[arg], optionStrings, "option", 0,
			    &index) != TCL_OK) {
      return TCL_ERROR;
    }

    if (arg + 1 == objc) {
      Tcl_AppendResult(interp, "No argument given for ",
		       optionStrings[index], " option", (char *) NULL);
      return TCL_ERROR;
    }
    
    switch ((enum options) index) {
    case OPTLOAD:
      {
	if (arg+1 == objc) {
	  Tcl_AppendResult(interp, "No filename given", NULL);
	  return TCL_ERROR;
	}
	filearg = arg + 1;
	storeType = SOUND_IN_MEMORY;
	break;
      }
    case OPTFILE:
      {
	if (arg+1 == objc) {
	  Tcl_AppendResult(interp, "No filename given", NULL);
	  return TCL_ERROR;
	}
	filearg = arg + 1;
	storeType = SOUND_IN_FILE;
	break;
      }
    case RATE:
    case FREQUENCY:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &samprate) != TCL_OK) {
	  return TCL_ERROR;
	}
	guessRate = 0;
	break;
      }
    case CHANNELS:
      {
	if (GetChannels(interp, objv[arg+1], &nchannels) != TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    case ENCODING:
    case FORMAT:
      {
	if (GetEncoding(interp, objv[arg+1], &encoding, &sampsize) != TCL_OK) {
	  return TCL_ERROR;
	}
	guessEncoding = 0;
	break;
      }
    case CHANNEL:
      {
	if (arg+1 == objc) {
	  Tcl_AppendResult(interp, "No channel name given", NULL);
	  return TCL_ERROR;
	}
	filearg = arg + 1;
	storeType = SOUND_IN_CHANNEL;
	break;
      }
    case OPTDEBUG:
      {
	if (arg+1 == objc) {
	  Tcl_AppendResult(interp, "No debug flag given", NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &debug) != TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    case FILEFORMAT:
      {
	if (strlen(Tcl_GetStringFromObj(objv[arg+1], NULL)) > 0) {
	  if (GetFileFormat(interp, objv[arg+1], &fileType) != TCL_OK) {
	    return TCL_ERROR;
	  }
	  forceFormat = 1;
	}
	break;
      }
    case BYTEORDER:
      {
	char *str = Tcl_GetStringFromObj(objv[arg+1], &length);
	if (strncasecmp(str, "littleEndian", length) == 0) {
	  swapIfBE = 1;
	} else if (strncasecmp(str, "bigEndian", length) == 0) {
	  swapIfBE = 0;
	} else {
	  Tcl_AppendResult(interp, "-byteorder option should be bigEndian or littleEndian", NULL);
	  return TCL_ERROR;
	}
	guessEncoding = 0;
	break;
      }
    case BUFFERSIZE:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &buffersize) != TCL_OK)
	  return TCL_ERROR;   
	break;
      }

    case SKIPHEAD: 
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &skipBytes) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case GUESSPROPS:
      {
	if (Tcl_GetBooleanFromObj(interp, objv[arg+1], &guessProps) !=TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case PRECISION:
      {
	char *str = Tcl_GetStringFromObj(objv[arg+1], &length);
	if (strncasecmp(str, "double", length) == 0) {
	  precision = SNACK_DOUBLE_PREC;
	} else if (strncasecmp(str, "single", length) == 0) {
	  precision = SNACK_SINGLE_PREC;
	} else {
	  Tcl_AppendResult(interp, "-precision option should be single",
			   " or double", NULL);
	  return TCL_ERROR;
	}
	break;
      }
    case CHGCMD:
      {
	char *str = Tcl_GetStringFromObj(objv[arg+1], NULL);

	if (strlen(str) > 0) {
	  cmdPtr = Tcl_DuplicateObj(objv[arg+1]);
	  Tcl_IncrRefCount(cmdPtr);
	}
	break;
      }
    }
  }
  
  if ((*sp = s = Snack_NewSound(samprate, encoding, nchannels)) == NULL) {
    Tcl_AppendResult(interp, "Could not allocate new sound!", NULL);
    return TCL_ERROR;
  }

  hPtr = Tcl_CreateHashEntry(hTab, name, &flag);
  Tcl_SetHashValue(hPtr, (ClientData) s);
  s->soundTable = hTab;

  if (guessProps) {
    if (guessEncoding == -1) {
      s->guessEncoding = 1;
    }
    if (guessRate == -1) {
      s->guessRate = 1;
    }
  }
  if (storeType != -1) {
    s->storeType = storeType;
  }
  if (buffersize != -1) {
    s->buffersize = buffersize;
  }
  if (skipBytes != -1) {
    s->skipBytes = skipBytes;
  }
  if (debug != -1) {
    s->debug = debug;
  }
  if (fileType != NULL) {
    s->fileType = fileType;
  }
  if (forceFormat != -1) {
    s->forceFormat = forceFormat;
  }
  if (precision != -1) {
    s->precision = precision;
  }
  if (swapIfBE == 0) {
    SwapIfLE(s);
  }
  if (swapIfBE == 1) {
    SwapIfBE(s);
  }
  if (cmdPtr != NULL) {
    s->changeCmdPtr = cmdPtr;
  }

  /*  s->fcname = strdup(name); */
  s->interp = interp;
  
  if (filearg > 0) {
    if (Tcl_IsSafe(interp)) {
      Tcl_AppendResult(interp, "can not read sound file in a safe interpreter",
		       (char *) NULL);
      CleanSound(s, interp, name);
      return TCL_ERROR;
    }
    if (SetFcname(s, interp, objv[filearg]) != TCL_OK) {
      CleanSound(s, interp, name);
      return TCL_ERROR;
    }
  }

  if (filearg > 0 && strlen(s->fcname) > 0) {
    if (s->storeType == SOUND_IN_MEMORY) {
      char *type = LoadSound(s, interp, NULL, 0, -1);
      
      if (type == NULL) {
	CleanSound(s, interp, name);
	return TCL_ERROR;
      }
      Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
    } else if (s->storeType == SOUND_IN_FILE) {
      if (GetHeader(s, interp, NULL) != TCL_OK) {
	s->fileType = NameGuessFileType(s->fcname);
      }
      if (s->encoding == LIN8OFFSET) {
	s->maxsamp = 128.0f;
	s->minsamp = 128.0f;
      } else {
	s->maxsamp = 0.0f;
	s->minsamp = 0.0f;
      }
    } else if (s->storeType == SOUND_IN_CHANNEL) {
      int mode = 0;

      s->rwchan = Tcl_GetChannel(interp, s->fcname, &mode);
      if (!(mode & TCL_READABLE)) {
	s->rwchan = NULL;
      }
      if (s->rwchan != NULL) {
	Tcl_SetChannelOption(interp, s->rwchan, "-translation", "binary");
#ifdef TCL_81_API
	Tcl_SetChannelOption(interp, s->rwchan, "-encoding", "binary");
#endif
      }
    }
  }

  return TCL_OK;
}

static void
SoundDeleteCmd(ClientData clientData)
{
  register Sound *s = (Sound *) clientData;
  int i;

  if (s->debug > 1) {
    Snack_WriteLog("  Sound obj cmd deleted\n");
  }
  if (s->destroy == 0) {
    Snack_StopSound(s, s->interp);
  }
  for (i = 0; i < nSoundCommands; i++) {
    if (sndDelCmdProcs[i] != NULL) {
      (sndDelCmdProcs[i])(s);
    }
  }
  if (s->destroy == 0 || wop == IDLE) {
    Snack_DeleteSound(s);
  }
}

int
Snack_SoundCmd(ClientData cdata, Tcl_Interp *interp, int objc,
	       Tcl_Obj *CONST objv[])
{
  char *name;
  Sound *s = NULL;

  if (ParseSoundCmd(cdata, interp, objc, objv, &name, &s) != TCL_OK ) {
    return TCL_ERROR;
  }

  Tcl_CreateObjCommand(interp, name, SoundCmd, (ClientData) s,
		       (Tcl_CmdDeleteProc *) SoundDeleteCmd); 
  
  Tcl_SetObjResult(interp, Tcl_NewStringObj(name, -1));

  return TCL_OK;
}

extern Tcl_HashTable *filterHashTable;

Sound *
Snack_GetSound(Tcl_Interp *interp, char *name)
{
  Tcl_CmdInfo infoPtr;
  Tcl_HashEntry *hPtr = Tcl_FindHashEntry(filterHashTable, name);
 
  if (hPtr != NULL || Tcl_GetCommandInfo(interp, name, &infoPtr) == 0) {
    Tcl_AppendResult(interp, name, " : no such sound", (char *) NULL);
    return NULL;
  }

  return (Sound *)infoPtr.objClientData;
}

void
Snack_SoundDeleteCmd(ClientData clientData)
{
  if (clientData != NULL) {
    Tcl_DeleteHashTable((Tcl_HashTable *) clientData);
    ckfree((char *) clientData);
  }
}

extern int nAudioCommands;
extern int maxAudioCommands;
extern audioDelCmd *audioDelCmdProcs[];
extern audioCmd *audioCmdProcs[];
extern char *audioCmdNames[];

extern int nMixerCommands;
extern int maxMixerCommands;
extern mixerDelCmd *mixerDelCmdProcs[];
extern mixerCmd *mixerCmdProcs[];
extern char *mixerCmdNames[];

int
Snack_AddSubCmd(int snackCmd, char *cmdName, Snack_CmdProc *cmdProc,
		Snack_DelCmdProc *delCmdProc)
{
  int i;

  switch(snackCmd) {
  case SNACK_SOUND_CMD:
    if (nSoundCommands < maxSoundCommands) {
      for (i = 0; i < nSoundCommands; i++) {
	if (strcmp(sndCmdNames[i], cmdName) == 0) break;
      }
      sndCmdNames[i] = cmdName;
      sndCmdProcs[i] = (soundCmd *)cmdProc;
      sndDelCmdProcs[i] = (soundDelCmd *)delCmdProc;
      if (i == nSoundCommands) nSoundCommands++;
    }
    break;
  case SNACK_AUDIO_CMD:
    if (nAudioCommands < maxAudioCommands) {
      for (i = 0; i < nAudioCommands; i++) {
	if (strcmp(audioCmdNames[i], cmdName) == 0) break;
      }
      audioCmdNames[i] = cmdName;
      audioCmdProcs[i] = (audioCmd *)cmdProc;
      audioDelCmdProcs[i] = (audioDelCmd *)delCmdProc;
      if (i == nAudioCommands) nAudioCommands++;
    }
    break;
  case SNACK_MIXER_CMD:
    if (nMixerCommands < maxMixerCommands) {
      for (i = 0; i < nMixerCommands; i++) {
	if (strcmp(mixerCmdNames[i], cmdName) == 0) break;
      }
      mixerCmdNames[i] = cmdName;
      mixerCmdProcs[i] = (mixerCmd *)cmdProc;
      mixerDelCmdProcs[i] = (mixerDelCmd *)delCmdProc;
      if (i == nMixerCommands) nMixerCommands++;
    }
    break;
  }

  return TCL_OK;
}

int
SetFcname(Sound *s, Tcl_Interp *interp, Tcl_Obj *obj)
{
  int length;
  char *str = Tcl_GetStringFromObj(obj, &length);

  if (s->fcname != NULL) {
    ckfree((char *)s->fcname);
  }
  if ((s->fcname = (char *) ckalloc((unsigned) (length + 1))) == NULL) {
    Tcl_AppendResult(interp, "Could not allocate name buffer!", NULL);
    return TCL_ERROR;
  }
  strcpy(s->fcname, str);

  return TCL_OK;
}

int
Snack_ProgressCallback(Tcl_Obj *cmdPtr, Tcl_Interp *interp, char *type, 
		      double fraction)
{
  if (cmdPtr != NULL) {
    Tcl_Obj *cmd = NULL;
    int res;

    cmd = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(interp, cmd, cmdPtr);
    Tcl_ListObjAppendElement(interp, cmd, Tcl_NewStringObj(type,-1));
    Tcl_ListObjAppendElement(interp, cmd, Tcl_NewDoubleObj(fraction));
    Tcl_Preserve((ClientData) interp);
    res = Tcl_GlobalEvalObj(interp, cmd);
    Tcl_Release((ClientData) interp);
    return res;
  }
  return TCL_OK;
}

int
Snack_PlatformIsLittleEndian()
{
  return(littleEndian);
}
