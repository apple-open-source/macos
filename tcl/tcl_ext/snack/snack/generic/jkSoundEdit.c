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

#include <math.h>
#include <string.h>
#include "snack.h"

TCL_DECLARE_MUTEX(myMutex)

void
SnackCopySamples(Sound *dest, int to, Sound *src, int from, int len)
{
  if (dest->storeType == SOUND_IN_MEMORY) {
    int sn, si, dn, di, tot = 0, blklen;
    
    to   *= src->nchannels;
    from *= src->nchannels;
    len  *= src->nchannels;

    if (dest == src && from < to) {
      tot = len;
      if (dest->precision == SNACK_SINGLE_PREC) {
	while (tot > 0) {
	  sn = (from + tot) >> FEXP;
	  si = (from + tot) - (sn << FEXP);
	  dn = (to   + tot) >> FEXP;
	  di = (to   + tot) - (dn << FEXP);
	  
	  if (di == 0) {
	    blklen = si;
	  } else if (si == 0) {
	    blklen = di;
	  } else { 
	    blklen = min(si, di);
	  }
	  
	  blklen = min(blklen, tot);
	  si -= blklen;
	  di -= blklen;
	  
	  if (si < 0) {
	    si = FBLKSIZE + si;
	    sn--;
	  }
	  if (di < 0) {
	    di = FBLKSIZE + di;
	    dn--;
	  }
          if (sn >= src->nblks) {
	    /* Reached end of allocated blocks for src */
	    return;
          }
          if (dn >= dest->nblks) {
	    /* Reached end of allocated blocks for dest */
	    return;
          }
	  memmove(&dest->blocks[dn][di],
		  &src->blocks[sn][si], 
		  blklen*sizeof(float));
	  tot -= blklen;
	}
      } else {
	while (tot > 0) {
	  sn = (from + tot) >> DEXP;
	  si = (from + tot) - (sn << DEXP);
	  dn = (to   + tot) >> DEXP;
	  di = (to   + tot) - (dn << DEXP);
	  
	  if (di == 0) {
	    blklen = si;
	  } else if (si == 0) {
	    blklen = di;
	  } else { 
	    blklen = min(si, di);
	  }
	  
	  blklen = min(blklen, tot);
	  si -= blklen;
	  di -= blklen;
	  
	  if (si < 0) {
	    si = DBLKSIZE + si;
	    sn--;
	  }
	  if (di < 0) {
	    di = DBLKSIZE + di;
	    dn--;
	  }
          if (sn >= src->nblks) {
	    /* Reached end of allocated blocks for src */
	    return;
          }
          if (dn >= dest->nblks) {
	    /* Reached end of allocated blocks for dest */
	    return;
          }
	  memmove(&((double **)dest->blocks)[dn][di],
		  &((double **)src->blocks)[sn][si], 
		  blklen*sizeof(double));
	  tot -= blklen;
	}
      }
    } else {
      if (dest->precision == SNACK_SINGLE_PREC) {
	while (tot < len) {
	  sn = (from + tot) >> FEXP;
	  si = (from + tot) - (sn << FEXP);
	  dn = (to   + tot) >> FEXP;
	  di = (to   + tot) - (dn << FEXP);
	  blklen = min(FBLKSIZE - si, FBLKSIZE - di);
	  blklen = min(blklen, len - tot);
          if (sn >= src->nblks) {
	    /* Reached end of allocated blocks for src */
	    return;
          }
          if (dn >= dest->nblks) {
	    /* Reached end of allocated blocks for dest */
	    return;
          }
	  memmove(&dest->blocks[dn][di],
		  &src->blocks[sn][si], 
		  blklen*sizeof(float));
	  tot += blklen;
	}
      } else {
	while (tot < len) {
	  sn = (from + tot) >> DEXP;
	  si = (from + tot) - (sn << DEXP);
	  dn = (to   + tot) >> DEXP;
	  di = (to   + tot) - (dn << DEXP);
	  blklen = min(DBLKSIZE - si, DBLKSIZE - di);
	  blklen = min(blklen, len - tot);
          if (sn >= src->nblks) {
	    /* Reached end of allocated blocks for src */
	    return;
          }
          if (dn >= dest->nblks) {
	    /* Reached end of allocated blocks for dest */
	    return;
          }
	  memmove(&((double **)dest->blocks)[dn][di],
		  &((double **)src->blocks)[sn][si], 
		  blklen*sizeof(double));
	  tot += blklen;
	}
      }
    }
  } else if (dest->storeType == SOUND_IN_FILE) {
  }
}

void
SnackSwapSoundBuffers(Sound *s1, Sound *s2)
{
  int tmpInt;
  float **tmpBlocks;
 
  tmpBlocks  = s1->blocks;
  s1->blocks = s2->blocks;
  s2->blocks = tmpBlocks;

  tmpInt    = s1->nblks;
  s1->nblks = s2->nblks;
  s2->nblks = tmpInt;

  tmpInt    = s1->exact;
  s1->exact = s2->exact;
  s2->exact = tmpInt;

  tmpInt        = s1->maxlength;
  s1->maxlength = s2->maxlength;
  s2->maxlength = tmpInt;
}

void
Snack_PutSoundData(Sound *s, int pos, void *buf, int nSamples)
{
  int dn, di, tot = 0, blklen;

  if (s->storeType != SOUND_IN_MEMORY) {
    return;
  }

  if (s->precision == SNACK_SINGLE_PREC) {
    while (tot < nSamples) {
      dn = (pos + tot) >> FEXP;
      di = (pos + tot) - (dn << FEXP);
      blklen = min(FBLKSIZE - di, nSamples - tot);
      if (dn >= s->nblks) {
	/* Reached end of allocated blocks for s */
	return;
      }
      memmove(&s->blocks[dn][di], &((float *)buf)[tot],
	      blklen * sizeof(float));
      tot += blklen;
    }
  } else {
    while (tot < nSamples) {
      dn = (pos + tot) >> DEXP;
      di = (pos + tot) - (dn << DEXP);
      blklen = min(DBLKSIZE - di, nSamples - tot);
      if (dn >= s->nblks) {
	/* Reached end of allocated blocks for s */
	return;
      }
      memmove(&((double **)s->blocks)[dn][di], &((double *)buf)[tot],
	      blklen * sizeof(double));
      tot += blklen;
    }
  }
}

void
Snack_GetSoundData(Sound *s, int pos, void *buf, int nSamples)
{
  int sn, si, tot = 0, blklen;

  if (s->storeType == SOUND_IN_MEMORY) {
    if (s->precision == SNACK_SINGLE_PREC) {
      while (tot < nSamples) {
	sn = (pos + tot) >> FEXP;
	si = (pos + tot) - (sn << FEXP);
	blklen = min(FBLKSIZE - si, nSamples - tot);
        if (sn >= s->nblks) {
	  /* Reached end of allocated blocks for s */
	  return;
        }
	memmove(&((float *)buf)[tot], &s->blocks[sn][si],
		blklen * sizeof(float));
	tot += blklen;
      }
    } else {
      while (tot < nSamples) {
	sn = (pos + tot) >> DEXP;
	si = (pos + tot) - (sn << DEXP);
	blklen = min(DBLKSIZE - si, nSamples - tot);
        if (sn >= s->nblks) {
	  /* Reached end of allocated blocks for s */
	  return;
        }
	memmove(&((double *)buf)[tot], &((double **)s->blocks)[sn][si],
		blklen * sizeof(double));
	tot += blklen;
      }
    }
  } else if (s->storeType == SOUND_IN_FILE) {
    int i;

    if (s->linkInfo.linkCh == NULL) {
      OpenLinkedFile(s, &s->linkInfo);
    }

    for (i = 0; i < nSamples; i++) {
      if (s->precision == SNACK_SINGLE_PREC) {
	((float *)buf)[i] = (float) GetSample(&s->linkInfo, pos+i);
      } else {
	((double *)buf)[i] = (double) GetSample(&s->linkInfo, pos+i);
      }
    }
  }
}

int
lengthCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int arg, len, type = 0, newlen = -1, i;
  char *string = NULL;

  if (s->debug > 0) { Snack_WriteLog("Enter lengthCmd\n"); }

  if (objc >= 3) {
    for (arg = 2; arg < objc; arg++) {
      string = Tcl_GetStringFromObj(objv[arg], &len);
      
      if (strncmp(string, "-units", len) == 0) {
	string = Tcl_GetStringFromObj(objv[arg+1], &len);
	if (strncasecmp(string, "seconds", len) == 0) type = 1;
	if (strncasecmp(string, "samples", len) == 0) type = 0;
	arg++;
      } else if (Tcl_GetIntFromObj(interp, objv[2], &newlen) != TCL_OK) {
	return TCL_ERROR;
      }
    }
  }
  
  if (newlen < 0) {
    if (type == 0) {
      Tcl_SetObjResult(interp, Tcl_NewIntObj(s->length));
    } else {
      Tcl_SetObjResult(interp, Tcl_NewDoubleObj((float)s->length/s->samprate));
    }
  } else {
    if (s->storeType != SOUND_IN_MEMORY) {
      Tcl_AppendResult(interp, "setting sound length only works with",
		       " in-memory sounds", (char *) NULL);
      return TCL_ERROR;
    }
    if (type == 1) {
      newlen *= s->samprate;
    }
    if (newlen > s->length) {
      if (Snack_ResizeSoundStorage(s, newlen) != TCL_OK) {
	return TCL_ERROR;
      }
      for (i = s->length * s->nchannels; i < newlen * s->nchannels; i++) {
	switch (s->encoding) {
	case LIN16:
	case LIN24:
	case LIN32:
	case ALAW:
	case MULAW:
	case LIN8:
	case SNACK_FLOAT:
	  if (s->precision == SNACK_SINGLE_PREC) {
	    FSAMPLE(s, i) = 0.0f;
	  } else {
	    DSAMPLE(s, i) = 0.0;
	  }
	  break;
	case LIN8OFFSET:
	  if (s->precision == SNACK_SINGLE_PREC) {
	    FSAMPLE(s, i) = 128.0f;
	  } else {
	    DSAMPLE(s, i) = 128.0;
	  }
	  break;
	}
      }
    }
    s->length = newlen;
    Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
    Snack_ExecCallbacks(s, SNACK_NEW_SOUND);
  }

  if (s->debug > 0) { Snack_WriteLog("Exit lengthCmd\n"); }

  return TCL_OK;
}

int
lastIndexCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  if (s->debug > 0) { Snack_WriteLog("Enter lastIndexCmd\n"); }

  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "lastIndex");
    return TCL_ERROR;
  }

  Tcl_SetObjResult(interp, Tcl_NewIntObj(s->length - 1));

  if (s->debug > 0) { Snack_WriteLog("Exit lastIndexCmd\n"); }

  return TCL_OK;
}

int
insertCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  Sound *ins;
  int inspoint, arg, startpos = 0, endpos = -1;
  char *string;
  static CONST84 char *subOptionStrings[] = {
    "-start", "-end", NULL
  };
  enum subOptions {
    START, END
  };

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "insert only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }
      
  if (objc < 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "insert sound sample");
    return TCL_ERROR;
  }

  string = Tcl_GetStringFromObj(objv[2], NULL);

  if ((ins = Snack_GetSound(interp, string)) == NULL) {
    return TCL_ERROR;
  }

  if (Tcl_GetIntFromObj(interp, objv[3], &inspoint) != TCL_OK) {
    return TCL_ERROR;
  }

  if (inspoint < 0 || inspoint > s->length) {
    Tcl_AppendResult(interp, "Insertion point out of bounds", NULL);
    return TCL_ERROR;
  }
      
  if (s->encoding != ins->encoding || s->nchannels != ins->nchannels) {
    Tcl_AppendResult(interp, "Sound format differs: ", string, NULL);
    return TCL_ERROR;
  }

  for (arg = 4; arg < objc; arg += 2) {
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
    }
  }
  if (startpos < 0) startpos = 0;
  if (endpos >= (ins->length - 1) || endpos == -1)
    endpos = ins->length - 1;
  if (startpos > endpos) return TCL_OK;

  if (Snack_ResizeSoundStorage(s, s->length + ins->length) != TCL_OK) {
    return TCL_ERROR;
  }
  SnackCopySamples(s, inspoint + endpos - startpos + 1, s, inspoint,
		   s->length - inspoint);
  SnackCopySamples(s, inspoint, ins, startpos, endpos - startpos + 1);
  s->length += (endpos - startpos + 1);
  Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);

  return TCL_OK;
}

int
cropCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int startpos, endpos, totlen;

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "crop only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (objc != 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "crop start end");
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp, objv[2], &startpos) != TCL_OK) return TCL_ERROR;
  if (Tcl_GetIntFromObj(interp, objv[3], &endpos) != TCL_OK) return TCL_ERROR;
      
  if ((endpos >= s->length - 1) || endpos < 0)
    endpos = s->length - 1;
  if (startpos >= endpos) return TCL_OK;
  if (startpos < 0) startpos = 0;
  totlen = endpos - startpos + 1;

  SnackCopySamples(s, 0, s, startpos, totlen);
  s->length = totlen;      
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);

  return TCL_OK;
}

int
copyCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int arg, startpos = 0, endpos = -1;
  Sound *master;
  char *string;
  static CONST84 char *subOptionStrings[] = {
    "-start", "-end", NULL
  };
  enum subOptions {
    START, END
  };

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "copy only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "copy sound");
    return TCL_ERROR;
  }
      
  string = Tcl_GetStringFromObj(objv[2], NULL);

  if ((master = Snack_GetSound(interp, string)) == NULL) {
    return TCL_ERROR;
  }

  if (master->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "can only copy from in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
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
    }
  }
  if (startpos < 0) startpos = 0;
  if (endpos >= (master->length - 1) || endpos == -1)
    endpos = master->length - 1;
  if (startpos > endpos) return TCL_OK;

  s->samprate = master->samprate;
  s->encoding = master->encoding;
  s->sampsize = master->sampsize;
  s->nchannels = master->nchannels;
  s->length = endpos - startpos + 1;
  if (Snack_ResizeSoundStorage(s, s->length) != TCL_OK) {
    return TCL_ERROR;
  }
  SnackCopySamples(s, 0, master, startpos, s->length);
  s->maxsamp = master->maxsamp;
  s->minsamp = master->minsamp;
  s->abmax = master->abmax;
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);

  return TCL_OK;
}

int
mixCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int arg, startpos = 0, endpos = -1, i, c, j;
  double mixscale = 1.0, prescale = 1.0;
  Sound *mixsnd;
  char *string;
  static CONST84 char *subOptionStrings[] = {
    "-start", "-end", "-mixscaling", "-prescaling", "-progress", NULL
  };
  enum subOptions {
    START, END, MIXSCALE, PRESCALE, PROGRESS
  };

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "mix only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "mix sound");
    return TCL_ERROR;
  }
      
  string = Tcl_GetStringFromObj(objv[2], NULL);

  if ((mixsnd = Snack_GetSound(interp, string)) == NULL) {
    return TCL_ERROR;
  }

  if (mixsnd->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "can only mix from in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (s->encoding != mixsnd->encoding || s->nchannels != mixsnd->nchannels) {
    Tcl_AppendResult(interp, "Sound format differs: ", string, NULL);
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
    case MIXSCALE:
      {
	if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &mixscale) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case PRESCALE:
      {
	if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &prescale) != TCL_OK)
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
  if (endpos >= (s->length - 1) || endpos == -1)
    endpos = s->length - 1;
  if (startpos > endpos) return TCL_OK;
  if (endpos - startpos + 1 > mixsnd->length)
    endpos = startpos + mixsnd->length - 1;

  Snack_ProgressCallback(s->cmdPtr, interp, "Mixing sound", 0.0);

  for (i = startpos, j = 0; i <= endpos; i++, j++) {
    for (c = 0; c < s->nchannels; c++) {
      float outsmp = (float) (prescale * FSAMPLE(s, (i * s->nchannels + c)) +
   		          mixscale * FSAMPLE(mixsnd, (j * s->nchannels + c)));
      if (outsmp > 32767.0f) {
	outsmp = 32767.0f;
      }
      if (outsmp < -32768.0f) {
	outsmp = -32768.0;
      }
      FSAMPLE(s, (i * s->nchannels + c)) = outsmp;
    }
    if ((i % 100000) == 99999) {
      int res = Snack_ProgressCallback(s->cmdPtr, interp,
				       "Mixing sound", 
				       (double) i / (endpos - startpos));
      if (res != TCL_OK) {
	return TCL_ERROR;
      }
    }
  }

  Snack_ProgressCallback(s->cmdPtr, interp, "Mixing sound", 1.0);

  Snack_UpdateExtremes(s, startpos, endpos, SNACK_NEW_SOUND);
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);

  return TCL_OK;
}

int
appendCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  Sound *t, *dummy;
  int arg, startpos = 0, endpos = -1, length = 0;
  char *filetype, *str;
  static CONST84 char *subOptionStrings[] = {
    "-rate", "-frequency", "-skiphead", "-byteorder", "-channels",
    "-encoding", "-format", "-start", "-end", "-fileformat",
    "-guessproperties", NULL
  };
  enum subOptions {
    RATE, FREQUENCY, SKIPHEAD, BYTEORDER, CHANNELS, ENCODING, FORMAT,
    START, END, FILEFORMAT,
    GUESSPROPS
  };

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "append variable");
    return TCL_ERROR;
  }
  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "append only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if ((t = Snack_NewSound(s->samprate, s->encoding, s->nchannels)) == NULL) {
    Tcl_AppendResult(interp, "Couldn't allocate new sound!", NULL);
    return TCL_ERROR;
  }
  t->debug = s->debug;
  t->guessEncoding = -1;
  t->guessRate = -1;
  t->swap = 0;

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
    case RATE:
    case FREQUENCY:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &t->samprate) != TCL_OK) {
	  return TCL_ERROR;
	}
	t->guessRate = 0;
	break;
      }
    case SKIPHEAD:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &t->skipBytes) != TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    case BYTEORDER:
      {
	int length;
	char *str = Tcl_GetStringFromObj(objv[arg+1], &length);
	    
	if (strncasecmp(str, "littleEndian", length) == 0) {
	  SwapIfBE(t);
	} else if (strncasecmp(str, "bigEndian", length) == 0) {
	  SwapIfLE(t);
	} else {
	  Tcl_AppendResult(interp, "-byteorder option should be bigEndian or littleEndian", NULL);
	  return TCL_ERROR;
	}
	t->guessEncoding = 0;
	break;
      }
    case CHANNELS:
      {
	if (GetChannels(interp, objv[arg+1], &t->nchannels) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case ENCODING:
    case FORMAT:
      {
	if (GetEncoding(interp, objv[arg+1], &t->encoding, &t->sampsize)
	    != TCL_OK)
	  return TCL_ERROR;
	t->guessEncoding = 0;
	break;
      }
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
    case FILEFORMAT:
      {
	if (strlen(Tcl_GetStringFromObj(objv[arg+1], NULL)) > 0) {
	  if (GetFileFormat(interp, objv[arg+1], &t->fileType) != TCL_OK)
	    return TCL_ERROR;
	  t->forceFormat = 1;
	}
	break;
      }
    case GUESSPROPS:
      {
	int guessProps;
	if (Tcl_GetBooleanFromObj(interp, objv[arg+1], &guessProps) != TCL_OK)
	  return TCL_ERROR;
	if (guessProps) {
	  if (t->guessEncoding == -1) t->guessEncoding = 1;
	  if (t->guessRate == -1) t->guessRate = 1;
	}
	break;
      }
    }
  }
  if (t->guessEncoding == -1) t->guessEncoding = 0;
  if (t->guessRate == -1) t->guessRate = 0;
  if (startpos < 0) startpos = 0;
  if (startpos > endpos && endpos != -1) return TCL_OK;

  str = Tcl_GetStringFromObj(objv[2], &length);
  if (length < 10 && (dummy = Snack_GetSound(interp, str)) != NULL) {
    Tcl_AppendResult(interp, "You must use the concatenate command instead",
		     NULL);
    return TCL_ERROR;
  }


  filetype = LoadSound(t, interp, objv[2], startpos, endpos);
  if (filetype == NULL) {
    Snack_DeleteSound(t);
    return TCL_ERROR;
  }
  if (s->encoding != t->encoding || s->nchannels != t->nchannels) {
    Snack_DeleteSound(t);
    Tcl_AppendResult(interp, "Sound format differs: ", NULL);
    return TCL_ERROR;
  }

  if (Snack_ResizeSoundStorage(s, s->length + t->length) != TCL_OK) {
    return TCL_ERROR;
  }
  SnackCopySamples(s, s->length, t, 0, t->length);
  s->length += t->length;
  Snack_UpdateExtremes(s, s->length - t->length, s->length, SNACK_MORE_SOUND);
  Snack_ExecCallbacks(s, SNACK_MORE_SOUND);
  Snack_DeleteSound(t);

  return TCL_OK;
}

int
concatenateCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  Sound *app;
  char *string;
  int i, arg, offset = 0, smoothjoin = 0;
  static CONST84 char *subOptionStrings[] = {
    "-smoothjoin", NULL
  };
  enum subOptions {
    SMOOTH
  };

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, 
		     "concatenate only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "concatenate sound");
    return TCL_ERROR;
  }
      
  string = Tcl_GetStringFromObj(objv[2], NULL);

  if ((app = Snack_GetSound(interp, string)) == NULL) {
    return TCL_ERROR;
  }

  if (s->encoding != app->encoding || s->nchannels != app->nchannels) {
    Tcl_AppendResult(interp, "Sound format differs: ", string, NULL);
    return TCL_ERROR;
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
    case SMOOTH:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &smoothjoin) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    }
  }
 
  if (s->length < smoothjoin) {
    Tcl_AppendResult(interp, "First sound is too short", NULL);
    return TCL_ERROR;
  }
  if (app->length < 2 * smoothjoin) {
    Tcl_AppendResult(interp, "Second sound is too short", NULL);
    return TCL_ERROR;
  }
  if (smoothjoin > 0) {
    offset = 80;
    if (s->length < offset) offset = s->length-1;
    for (i = 0; i < offset; i++) {
      float z = (float) ((0.5 + 160.0 / 2 - 1 - i) * 3.141592653589793 / 160);
      float win = (float) exp(-3.0 * z * z);
      
      FSAMPLE(s, s->length-offset+i) = (float) ((1.0-win) *
	FSAMPLE(s, s->length-offset+i) + win * FSAMPLE(app, i));
    }
  } else {
    offset = 0;
  }

  if (Snack_ResizeSoundStorage(s, s->length + app->length -offset) != TCL_OK) {
    return TCL_ERROR;
  }
  SnackCopySamples(s, s->length, app, offset, app->length - offset);
  Snack_UpdateExtremes(s, s->length, s->length + app->length - offset,
		       SNACK_MORE_SOUND);
  s->length += (app->length - offset);
  Snack_ExecCallbacks(s, SNACK_MORE_SOUND);

  return TCL_OK;
}

int
cutCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int start, end;

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "cut only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }
      
  if (objc != 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "cut start end");
    return TCL_ERROR;
  }

  if (Tcl_GetIntFromObj(interp, objv[2], &start) != TCL_OK) {
    return TCL_ERROR;
  }

  if (Tcl_GetIntFromObj(interp, objv[3], &end) != TCL_OK) {
    return TCL_ERROR;
  }

  if (start < 0 || start > s->length - 1) {
    Tcl_AppendResult(interp, "Start point out of bounds", NULL);
    return TCL_ERROR;
  }

  if (end < start || end > s->length - 1) {
    Tcl_AppendResult(interp, "End point out of bounds", NULL);
    return TCL_ERROR;
  }

  SnackCopySamples(s, start, s, end + 1, s->length - end - 1);
  s->length = s->length - (end - start + 1);
  Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);

  return TCL_OK;
}

int
Lowpass(Sound *s, Tcl_Interp *interp, int rate, int hirate)
{
  int c, i;
  float outsmp;
  double insmp = 0.0, last;
  double a = 6.28318530718 * rate / hirate;
  double b = exp(-a / (double) hirate);
  double out;

  for (c = 0; c < s->nchannels; c++) {
    last = 0.0;
    for (i = 0; i < s->length; i++) {
      insmp = (double) FSAMPLE(s, (i * s->nchannels + c));
      
      out = insmp * a + last * b;
      last = insmp;
      outsmp = (float) (0.4 * out);

      if (outsmp > 32767.0f) {
	outsmp = 32767.0f;
      }
      if (outsmp < -32768.0f) {
	outsmp = -32768.0;
      }

      FSAMPLE(s, (i * s->nchannels + c)) = outsmp;
      
      if ((i % 100000) == 99999) {
	int res = Snack_ProgressCallback(s->cmdPtr, interp,
					 "Converting rate",
		       0.5 + 0.5 * ((double) (i + c * s->length)
		         / (s->length * s->nchannels)));
	if (res != TCL_OK) {
	  return TCL_ERROR;
	}
      }
    }
  }

  return TCL_OK;
}

static int
Resample(Sound *s, Sound *t, Tcl_Interp *interp)
{
  int i, j, c, res, pos;
  float leftsmp = 0.0, rightsmp;
  double f, frac, dj;

  frac = (double) s->samprate / (double) t->samprate;

  Snack_ProgressCallback(s->cmdPtr, interp, "Converting rate", 0.0);
  for (c = 0; c < s->nchannels; c++) {

    for (i = 0; i < t->length; i++) {

      dj = frac * i; 
      j = (int) dj;
      f = dj - j;
      
      pos = j * s->nchannels + c;
      leftsmp  = FSAMPLE(s, pos);
      rightsmp = FSAMPLE(s, pos + s->nchannels);

      FSAMPLE(t, (i * s->nchannels + c)) = (float) (leftsmp * (1.0 - f) + rightsmp * f);

      if ((i % 100000) == 99999) {
	int res = Snack_ProgressCallback(s->cmdPtr, interp,
					 "Converting rate",
		(0.5 * (i + c * t->length)) / (t->length * s->nchannels));
	if (res != TCL_OK) {
	  Snack_DeleteSound(t);
	  return TCL_ERROR;
	}
      }
    }
  }
  res = Lowpass(t, interp, (int) (0.425 * min(t->samprate, s->samprate)),
		s->samprate);
  if (res != TCL_OK) {
    return TCL_ERROR;
  }
  Snack_ProgressCallback(s->cmdPtr, interp, "Converting rate", 1.0);

  return TCL_OK;
}

int
convertCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int arg, i, j;
  int samprate = s->samprate, nchannels = s->nchannels;
  int encoding = s->encoding, sampsize = s->sampsize;
  int snchan = s->nchannels;
  Sound *t = NULL;
  static CONST84 char *subOptionStrings[] = {
    "-rate", "-frequency", "-channels", "-encoding", "-format",
    "-progress", NULL
  };
  enum subOptions {
    RATE, FREQUENCY, CHANNELS, ENCODING, FORMAT, PROGRESS
  };

  if (s->debug > 0) { Snack_WriteLog("Enter convertCmd\n"); }

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "convert only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (objc < 4) {
    Tcl_WrongNumArgs(interp, 1, objv, "convert option value");
    return TCL_ERROR;
  }
  
  if (s->cmdPtr != NULL) {
    Tcl_DecrRefCount(s->cmdPtr);
    s->cmdPtr = NULL;
  }
  
  for (arg = 2; arg < objc; arg += 2) {
    int index;

    if (Tcl_GetIndexFromObj(interp, objv[arg], subOptionStrings,
			    "option", 0, &index) != TCL_OK) {
      return TCL_ERROR;
    }

    switch ((enum subOptions) index) {
    case RATE:
    case FREQUENCY:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &samprate) != TCL_OK)
	  return TCL_ERROR;
	if (samprate < 1) {
	  Tcl_AppendResult(interp, "Rate must be > 1", NULL);
	  return TCL_ERROR;
	}
	break;
      }
    case CHANNELS:
      {
	if (GetChannels(interp, objv[arg+1], &nchannels) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case ENCODING:
    case FORMAT:
      {
	if (GetEncoding(interp, objv[arg+1], &encoding, &sampsize) != TCL_OK)
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

  if ((t = Snack_NewSound(samprate, encoding, s->nchannels)) == NULL) {
    Tcl_AppendResult(interp, "Couldn't allocate temporary sound!", NULL);
    return TCL_ERROR;
  }
  t->debug  = s->debug;
  t->length = (int) (s->length * (float) samprate / s->samprate);
  if (Snack_ResizeSoundStorage(t, t->length) != TCL_OK) {
    Tcl_AppendResult(interp, "Couldn't allocate temporary sound!", NULL);
    return TCL_ERROR;
  }
  if (samprate != s->samprate) {
    if (s->length > 0) {
      if (Resample(s, t, interp) != TCL_OK) {
	Snack_DeleteSound(t);
	return TCL_ERROR;
      }
      SnackSwapSoundBuffers(s, t);
    }
    s->length = t->length;
    s->samprate = t->samprate;
  }
  if (Snack_ResizeSoundStorage(t, t->length * nchannels) != TCL_OK) {
    Tcl_AppendResult(interp, "Couldn't allocate temporary sound!", NULL);
    return TCL_ERROR;
  }
  t->nchannels = nchannels;
  
  if (encoding != s->encoding) {
    Snack_ProgressCallback(s->cmdPtr, interp, "Converting encoding", 0.0);
    for (i = 0; i < s->length * snchan; i++) {
      float value = 0.0;

      switch (s->encoding) {
      case LIN16:
      case SNACK_FLOAT:
      case ALAW:
      case MULAW:
	value = FSAMPLE(s, i);
	break;
      case LIN32:
	value = FSAMPLE(s, i) / 65536.0f;
	break;
      case LIN24:
	value = FSAMPLE(s, i) / 256.0f;
	break;
      case LIN8OFFSET:
	value = (FSAMPLE(s, i) - 128.0f) * 256.0f;
	break;
      case LIN8:
	value = FSAMPLE(s, i) * 256.0f;
	break;
      }

      switch (encoding) {
      case LIN16:
      case SNACK_FLOAT:
	FSAMPLE(t, i) = value;
	break;
      case LIN32:
	FSAMPLE(t, i) = value * 65536.0f;
	break;
      case LIN24:
	FSAMPLE(t, i) = value * 256.0f;
	break;
      case ALAW:
	FSAMPLE(t, i) = (float) Snack_Alaw2Lin(Snack_Lin2Alaw((short)value));
	break;
      case MULAW:
	FSAMPLE(t, i) = (float) Snack_Mulaw2Lin(Snack_Lin2Mulaw((short)value));
	break;
      case LIN8OFFSET:
	FSAMPLE(t, i) = (value / 256.0f) + 128.0f;
	break;
      case LIN8:
	FSAMPLE(t, i) = value / 256.0f;
	break;
      }

      if ((i % 100000) == 99999) {
	int res = Snack_ProgressCallback(s->cmdPtr, interp,
					 "Converting encoding",
				 (double) i / (s->length * snchan));
	if (res != TCL_OK) {
	  Snack_DeleteSound(t);
	  return TCL_ERROR;
	}
      }
    }
    Snack_ProgressCallback(s->cmdPtr, interp, "Converting encoding", 1.0);
    SnackSwapSoundBuffers(s, t);
    s->encoding = t->encoding;
    s->sampsize = t->sampsize;
  }

  if (nchannels != snchan) {
    if (nchannels > 1 && snchan > 1) {
      Tcl_AppendResult(interp, "Can only convert n -> 1 or 1 -> n channels",
		       (char *) NULL);
      Snack_DeleteSound(t);
      return TCL_ERROR;
    }
    Snack_ProgressCallback(s->cmdPtr, interp, "Converting channels", 0.0);
    if (nchannels == 1) {
      for (i = 0; i < s->length; i++) {
	float value = 0.0f;
	
	for (j = 0; j < snchan; j++) {
	  value += FSAMPLE(s, i * snchan + j);
	}
	value = value / (float) snchan;
	
	FSAMPLE(t, i) = value;

	if ((i % 100000) == 99999) {
	  int res = Snack_ProgressCallback(s->cmdPtr, interp,
					   "Converting channels", 
					   (double) i / s->length);
	  if (res != TCL_OK) {
	    Snack_DeleteSound(t);
	    return TCL_ERROR;
	  }
	}
      }
    }
    if (snchan == 1) {
      for (i = s->length - 1; i >= 0; i--) {
	for (j = 0; j < nchannels; j++) {
	  FSAMPLE(t, i * nchannels + j) = FSAMPLE(s, i);
	}
	if ((i % 100000) == 99999) {
	  int res = Snack_ProgressCallback(s->cmdPtr, interp, "Converting channels",
					   (double) (s->length-i)/ s->length);
	  if (res != TCL_OK) {
	    Snack_DeleteSound(t);
	    return TCL_ERROR;
	  }
	}
      }
    }
    Snack_ProgressCallback(s->cmdPtr, interp, "Converting channels", 1.0);
    SnackSwapSoundBuffers(s, t);
    s->nchannels = t->nchannels;
  }
  Snack_DeleteSound(t);
  Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);

  if (s->debug > 0) { Snack_WriteLog("Exit convertCmd\n"); }

  return TCL_OK;
}

int
reverseCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int arg, startpos = 0, endpos = -1, i, j, c;
  static CONST84 char *subOptionStrings[] = {
    "-start", "-end", "-progress", NULL
  };
  enum subOptions {
    START, END, PROGRESS
  };

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "reverse only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (s->cmdPtr != NULL) {
    Tcl_DecrRefCount(s->cmdPtr);
    s->cmdPtr = NULL;
  }

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "reverse");
    return TCL_ERROR;
  }
      
  for (arg = 2; arg < objc; arg += 2) {
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
  if (endpos >= (s->length - 1) || endpos == -1)
    endpos = s->length - 1;
  if (startpos > endpos) return TCL_OK;

  if (s->writeStatus == WRITE) {
    Snack_StopSound(s, interp);
  }

  Snack_ProgressCallback(s->cmdPtr, interp, "Reversing sound", 0.0);

  for (i = startpos, j = endpos; i <= startpos + (endpos - startpos) / 2;
       i++, j--) {
    for (c = 0; c < s->nchannels; c++) {
      float swap = FSAMPLE(s, i * s->nchannels + c);
      FSAMPLE(s, i * s->nchannels + c) = FSAMPLE(s, j * s->nchannels + c);
      FSAMPLE(s, j * s->nchannels + c) = swap;
      if ((i % 100000) == 99999) {
	int res = Snack_ProgressCallback(s->cmdPtr, interp, "Reversing sound",
			  (double) i / (startpos + (endpos - startpos) / 2));
	if (res != TCL_OK) {
	  return TCL_ERROR;
	}
      }
    }
  }

  Snack_ProgressCallback(s->cmdPtr, interp, "Reversing sound", 1.0);

  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);
  
  return TCL_OK;
}

int
sampleCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int i, n, val;
  double fval;
  char buf[20];

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "sample index ?val? ...");
    return TCL_ERROR;
  }

  if (Tcl_GetIntFromObj(interp, objv[2], &i) != TCL_OK) return TCL_ERROR;
  if (i < 0 || i >= s->length) {
    Tcl_AppendResult(interp, "Index out of bounds", NULL);
    return TCL_ERROR;
  }
  if (objc > 3 && objc > s->nchannels + 3) {
    Tcl_AppendResult(interp, "Too many samples given", NULL);
    return TCL_ERROR;
  }

  i *= s->nchannels;

  if (objc < 4) {
    if (s->storeType != SOUND_IN_MEMORY && s->linkInfo.linkCh == NULL) {
      OpenLinkedFile(s, &s->linkInfo);
    }
    
    for (n = 0; n < s->nchannels; n++, i++) {
      switch (s->encoding) {
      case LIN16:
      case LIN32:
      case LIN24:
      case ALAW:
      case MULAW:
      case LIN8OFFSET:
      case LIN8:
	if (s->storeType == SOUND_IN_MEMORY) {
	  if (s->precision == SNACK_SINGLE_PREC) {
	    sprintf(buf, "%d", (int) FSAMPLE(s, i));
	  } else {
	    sprintf(buf, "%d", (int) DSAMPLE(s, i));
	  }
	} else {
	  sprintf(buf, "%d", (int) GetSample(&s->linkInfo, i));
	}
	break;
      case SNACK_FLOAT:
      case SNACK_DOUBLE:
	if (s->storeType == SOUND_IN_MEMORY) {
	  if (s->precision == SNACK_SINGLE_PREC) {
	    sprintf(buf, "%f", FSAMPLE(s, i));
	  } else {
	    sprintf(buf, "%.12f", DSAMPLE(s, i));
	  }
	} else {
	  sprintf(buf, "%f", GetSample(&s->linkInfo, i));
	}
	break;
      }
      if (n < s->nchannels - 1) {
	Tcl_AppendResult(interp, buf, " ", NULL);
      } else {
	Tcl_AppendResult(interp, buf, NULL);
      }
    }
  } else {
    if (s->storeType != SOUND_IN_MEMORY) {
      Tcl_AppendResult(interp, "setting sample values only works with in-memory sounds", (char *) NULL);
      return TCL_ERROR;
    }
    for (n = 3; n < 3 + s->nchannels; n++, i++) {
      char *str;
      int len;

      if (n >= objc) break;
      str = Tcl_GetStringFromObj(objv[n], &len);
      if (strcmp(str, "?") == 0) continue;
      if (s->encoding == SNACK_FLOAT || s->encoding == SNACK_DOUBLE) {
	if (Tcl_GetDoubleFromObj(interp, objv[n], &fval) != TCL_OK) return TCL_ERROR;
	/*
	if (fval < -32768.0 || fval > 32767.0) {
	  Tcl_AppendResult(interp, "Sample value not in range -32768, 32767",
			   NULL);
	  return TCL_ERROR;
	}
	*/
      } else {
	if (Tcl_GetIntFromObj(interp, objv[n], &val) != TCL_OK) return TCL_ERROR;
      }
      switch (s->encoding) {
      case LIN16:
      case ALAW:
      case MULAW:
	if (val < -32768 || val > 32767) {
	  Tcl_AppendResult(interp, "Sample value not in range -32768, 32767",
			   NULL);
	  return TCL_ERROR;
	}
      case LIN32:
      case LIN24:
	if (val < -8388608 || val > 8388607) {
	  Tcl_AppendResult(interp, "Sample value not in range -8388608, 8388607",
			   NULL);
	  return TCL_ERROR;
	}
	if (s->precision == SNACK_SINGLE_PREC) {
	  FSAMPLE(s, i) = (float) val;
	} else {
	  DSAMPLE(s, i) = (double) val;
	}
	break;
      case SNACK_FLOAT:
      case SNACK_DOUBLE:
	if (s->precision == SNACK_SINGLE_PREC) {
	  FSAMPLE(s, i) = (float) fval;
	} else {
	  DSAMPLE(s, i) = fval;
	}
	break;
      case LIN8OFFSET:
	if (val < 0 || val > 255) {
	  Tcl_AppendResult(interp, "Sample value not in range 0, 255", NULL);
	  return TCL_ERROR;
	}
	if (s->precision == SNACK_SINGLE_PREC) {
	  FSAMPLE(s, i) = (float) val;
	} else {
	  DSAMPLE(s, i) = (double) val;
	}
	break;
      case LIN8:
	if (val < -128 || val > 127) {
	  Tcl_AppendResult(interp, "Sample value not in range -128, 127",
			   NULL);
	  return TCL_ERROR;
	}
	if (s->precision == SNACK_SINGLE_PREC) {
	  FSAMPLE(s, i) = (float) val;
	} else {
	  DSAMPLE(s, i) = (double) val;
	}
	break;
      }
    }
  }

  return TCL_OK;
}

int
swapCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  Sound *t;
  char *string;
  int tmpInt;
  float tmpFloat;

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "reverse only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "swap sound");
    return TCL_ERROR;
  }
      
  string = Tcl_GetStringFromObj(objv[2], NULL);

  if ((t = Snack_GetSound(interp, string)) == NULL) {
    return TCL_ERROR;
  }

  if (s->encoding != t->encoding || s->nchannels != t->nchannels
      || s->samprate != t->samprate) {
    Tcl_AppendResult(interp, "Sound format differs: ", string, NULL);
    return TCL_ERROR;
  }

  SnackSwapSoundBuffers(s, t);

  tmpFloat   = s->maxsamp;
  s->maxsamp = t->maxsamp;
  t->maxsamp = tmpFloat;

  tmpFloat   = s->minsamp;
  s->minsamp = t->minsamp;
  t->minsamp = tmpFloat;

  tmpFloat = s->abmax;
  s->abmax = t->abmax;
  t->abmax = tmpFloat;

  tmpInt    = s->length;
  s->length = t->length;
  t->length = tmpInt;

  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);
  Snack_ExecCallbacks(t, SNACK_NEW_SOUND);

  return TCL_OK;
}

int
byteswapCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  /*
  float *block = (float *) ckalloc(1000000);
  float *block2 = (float *) ckalloc(1000000);
  int i, j, jmax = 100;
  double time;

  time = SnackCurrentTime();
  for (j = 0; j < jmax ; j++) {  
    memcpy(block, block2, 1000000);
  }
  Snack_WriteLogInt("memcpy", (int)(1000000*(SnackCurrentTime()-time)));

  time = SnackCurrentTime();
  for (j = 0; j < jmax ; j++) {  
    for (i = 0; i < 250000; i++) {
      block[i] = block2[i];
    }
  }
  Snack_WriteLogInt(" =[] ", (int)(1000000*(SnackCurrentTime()-time)));

  time = SnackCurrentTime();
  for (j = 0; j < jmax ; j++) {
    float *p = block, *q = block2;
    for (i = 0; i < 250000; i++) {
      *p++ = *q++;
    }
  }
  Snack_WriteLogInt(" =++ ", (int)(1000000*(SnackCurrentTime()-time)));
*/
  return TCL_OK;
}

/* byte reverse command for Snack qzhou@lucent.com 2-3-2000 */

/* byte reverse (bit swap) macro */
#define FLIP_BITS( byte ) \
   (unsigned char)((  ((byte >> 7) & 0x01) | ((byte >> 5) & 0x02) | \
               ((byte >> 3) & 0x04) | ((byte >> 1) & 0x08) | \
               ((byte << 1) & 0x10) | ((byte << 3) & 0x20) | \
               ((byte << 5) & 0x40) | ((byte << 7) & 0x80)) & 0xFF )


/* Function to add flipBits command, not updated for 2.0 !*/
int
flipBitsCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  /*
  int i;
  unsigned char *sampCh; */
  
  if ( s->storeType != SOUND_IN_MEMORY ) {
    Tcl_AppendResult(interp, "flipBits only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "flipBits");
    return TCL_ERROR;
  }

  if (s->encoding == MULAW) {
    Tcl_AppendResult(interp, "flipBits only works with Mulaw sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  /* stop writing */
  if (s->writeStatus == WRITE) {
    Snack_StopSound(s, interp);
  }
  
  /* flip bits for every byte */
  /*
  for ( i = 0; i < s->length; i++ ) {
    sampCh = &(UCSAMPLE(s, i));
    *sampCh = FLIP_BITS( *sampCh );
  }
  */
  Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);
  
  return TCL_OK;
}

/*

  The following functions add interoperability between Snack and the CSLU
  Speech Toolkit. Two functions are provided to convert Snack sound objects
  into CSLUsh wave objects and vice versa.

 */

#ifdef SNACK_CSLU_TOOLKIT

#include <dballoc.h>
#include <result.h>
#include <wave.h>

#include <vec.h>
#include <utils.h>
#include <cmds.h>
#include <obj.h>

int
fromCSLUshWaveCmd(Sound *s, Tcl_Interp *interp, int objc,Tcl_Obj *CONST objv[])
{
  Wave *w;
  char *handle;

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "fromCSLUshWave only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "fromCSLUshWave waveObj");
    return TCL_ERROR;
  }

  handle = Tcl_GetStringFromObj(objv[2], NULL);
  if (!(w = Obj_GetData(interp, WAVE, handle))) {
    Tcl_AppendResult(interp, "Failed getting data from waveObj: ",
		     handle, NULL);
    return TCL_ERROR;
  }
  if (w->attr[WAVE_TYPE] != WAVE_TYPE_LINEAR) {
    Tcl_AppendResult(interp, "waveObj must be WAVE_TYPE_LINEAR", NULL);
    return TCL_ERROR;
  }
  if (s->writeStatus == WRITE) {
    Snack_StopSound(s, interp);
  }
  s->samprate = (int) w->attr[WAVE_RATE];
  s->encoding = LIN16;
  s->sampsize = sizeof(short);
  s->nchannels = 1;
  s->length = w->len;
  if (Snack_ResizeSoundStorage(s, s->length) != TCL_OK) {
    return TCL_ERROR;
  }
  Snack_PutSoundData(s, 0, w->samples, w->len * s->sampsize);
  Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);

  return TCL_OK;
}

int
toCSLUshWaveCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  Wave *w;
  float attr[WAVE_ATTRIBUTES];

  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "toCSLUshWave only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "toCSLUshWave");
    return TCL_ERROR;
  }
  if (s->encoding != LIN16 || s->nchannels != 1) {
    Tcl_AppendResult(interp, "Sorry, only implemented for lin16, mono sounds",
		     NULL);
    return TCL_ERROR;
  }

  attr[WAVE_RATE] = (float) s->samprate;
  attr[WAVE_TYPE] = 0;
  /* Doesn't handle large sounds yet > 512kB */
  if(createWave(interp, s->length, WAVE_ATTRIBUTES, attr, s->blocks[0], &w) != TCL_OK)
    return TCL_ERROR;

  return TCL_OK;
}
#endif
