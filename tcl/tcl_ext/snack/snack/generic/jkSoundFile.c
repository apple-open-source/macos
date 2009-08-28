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
#include <string.h>
#include <math.h>
#include "snack.h"

extern int littleEndian;

struct Snack_FileFormat *snackFileFormats = NULL;

extern int useOldObjAPI;

static char *
GuessWavFile(char *buf, int len)
{
  if (len < 21) return(QUE_STRING);
  if (strncasecmp("RIFF", buf, strlen("RIFF")) == 0) {
    if (buf[20] == 85) {
      return(MP3_STRING);
    }
    if (strncasecmp("WAVE", &buf[8], strlen("WAVE")) == 0) {
      return(WAV_STRING);
    }
  }
  return(NULL);
}

static char *
GuessAuFile(char *buf, int len)
{
  if (len < 4) return(QUE_STRING);
  if (strncmp(".snd", buf, strlen(".snd")) == 0) {
    return(AU_STRING);
  }
  return(NULL);
}

static char *
GuessAiffFile(char *buf, int len)
{
  if (len < 12) return(QUE_STRING);
  if (strncasecmp("FORM", buf, strlen("FORM")) == 0) {
    if (strncasecmp("AIFF", &buf[8], strlen("AIFF")) == 0) {
      return(AIFF_STRING);
    }
  }
  return(NULL);
}

static char *
GuessSmpFile(char *buf, int len)
{
  int i, end = len - strlen("file=samp");

  for (i = 0; i < end; i++) {
    if (strncasecmp("file=samp", &buf[i], strlen("file=samp")) == 0) {
      return(SMP_STRING);
    }
  }
  if (len < 512) return(QUE_STRING);
  return(NULL);
}

static char *
GuessSdFile(char *buf, int len)
{
  if (len < 20) return(QUE_STRING);
  if (buf[16] == 0 && buf[17] == 0 && buf[18] == 106 && buf[19] == 26) {
    return(SD_STRING);
  }
  return(NULL);
}

static char *
GuessCslFile(char *buf, int len)
{
  if (len < 8) return(QUE_STRING);
  if (strncmp("FORMDS16", buf, strlen("FORMDS16")) == 0) {
    return(CSL_STRING);
  }
  return(NULL);
}

static char *
GuessRawFile(char *buf, int len)
{
  return(RAW_STRING);  
}

char *
GuessFileType(char *buf, int len, int eof)
{
  Snack_FileFormat *ff;
  int flag = 0;

  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
    char *type = (ff->guessProc)(buf, len);

    if (type == NULL) {
      /* guessProc can't recognize this header */
    } else if (strcmp(type, QUE_STRING) == 0) {
      flag = 1; /* guessProc needs more bytes in order to decide */
    } else if (strcmp(type, RAW_STRING) != 0) {
      return(type);
    }
  }

  /* Don't decide yet if there's more header bytes to be had */

  if (flag && !eof) {
    return(QUE_STRING);
  }

  /* No guessProc recognized this header => guess RAW format */

  return(RAW_STRING);
}

static int
ExtCmp(char *s1, char *s2)
{
  int l1 = strlen(s1);
  int l2 = strlen(s2);

  return(strncasecmp(s1, &s2[l2 - l1], l1));
}

static char *
ExtSmpFile(char *s)
{
  if (ExtCmp(".smp", s) == 0) {
    return(SMP_STRING);
  }
  return(NULL);
}

static char *
ExtWavFile(char *s)
{
  if (ExtCmp(".wav", s) == 0) {
    return(WAV_STRING);
  }
  return(NULL);
}

static char *
ExtAuFile(char *s)
{
  if (ExtCmp(".au", s) == 0 || ExtCmp(".snd", s) == 0) {
    return(AU_STRING);
  }
  return(NULL);
}

static char *
ExtAiffFile(char *s)
{
  if (ExtCmp(".aif", s) == 0 || ExtCmp(".aiff", s) == 0) {
    return(AIFF_STRING);
  }
  return(NULL);
}

static char *
ExtSdFile(char *s)
{
  if (ExtCmp(".sd", s) == 0) {
    return(SD_STRING);
  }
  return(NULL);
}

static char *
ExtCslFile(char *s)
{
  if (ExtCmp(".nsp", s) == 0) {
    return(CSL_STRING);
  }
  return(NULL);
}

char *
NameGuessFileType(char *s)
{
  Snack_FileFormat *ff;
  
  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
    if (ff->extProc != NULL) {
      char *type = (ff->extProc)(s);
      if (type != NULL) {
	return(type);
      }
    }
  }
  return(RAW_STRING); 
}
/*
static short
ReadBEShort(Tcl_Channel ch)
{
  short ts;

  Tcl_Read(ch, (char *) &ts, sizeof(short));

  if (littleEndian) {
  ts = Snack_SwapShort(ts);
  }
  
  return(ts);
}

static short
ReadLEShort(Tcl_Channel ch)
{
  short ts;

  Tcl_Read(ch, (char *) &ts, sizeof(short));
  
  if (!littleEndian) {
    ts = Snack_SwapShort(ts);
  }

  return(ts);
}

static int32_t
ReadBELong(Tcl_Channel ch)
{
  int32_t tl;

  Tcl_Read(ch, (char *) &tl, sizeof(int32_t));

  if (littleEndian) {
    tl = Snack_SwapLong(tl);
  }

  return(tl);
}

static int32_t
ReadLELong(Tcl_Channel ch)
{
  int32_t tl;

  Tcl_Read(ch, (char *) &tl, sizeof(int32_t));

  if (!littleEndian) {
    tl = Snack_SwapLong(tl);
  }

  return(tl);
}
*/
static int
WriteLEShort(Tcl_Channel ch, short s)
{
  short ts = s;

  if (!littleEndian) {
    ts = Snack_SwapShort(ts);
  }

  return(Tcl_Write(ch, (char *) &ts, sizeof(short)));
}

int
WriteLELong(Tcl_Channel ch, int32_t l)
{
  int32_t tl = l;

  if (!littleEndian) {
    tl = Snack_SwapLong(tl);
  }

  return(Tcl_Write(ch, (char *) &tl, sizeof(int32_t)));
}

static int
WriteBEShort(Tcl_Channel ch, short s)
{
  short ts = s;

  if (littleEndian) {
    ts = Snack_SwapShort(ts);
  }

  return(Tcl_Write(ch, (char *) &ts, sizeof(short)));
}

int
WriteBELong(Tcl_Channel ch, int32_t l)
{
  int32_t tl = l;

  if (littleEndian) {
    tl = Snack_SwapLong(tl);
  }

  return(Tcl_Write(ch, (char *) &tl, sizeof(int32_t)));
}
  
static int32_t
GetLELong(char *buf, int pos)
{
  int32_t tl;

  memcpy(&tl, &buf[pos], sizeof(int32_t));

  if (!littleEndian) {
    tl = Snack_SwapLong(tl);
  }

  return(tl);
}

static short
GetLEShort(char *buf, int pos)
{
  short ts;
  char *p;
  short *q;

  p = &buf[pos];
  q = (short *) p;
  ts = *q;

  if (!littleEndian) {
    ts = Snack_SwapShort(ts);
  }

  return(ts);
}

static int32_t
GetBELong(char *buf, int pos)
{
  int32_t tl;

  memcpy(&tl, &buf[pos], sizeof(int32_t));

  if (littleEndian) {
    tl = Snack_SwapLong(tl);
  }

  return(tl);
}

static short
GetBEShort(char *buf, int pos)
{
  short ts;
  char *p;
  short *q;

  p = &buf[pos];
  q = (short *) p;
  ts = *q;

  if (littleEndian) {
    ts = Snack_SwapShort(ts);
  }

  return(ts);
}

static void
PutBELong(char *buf, int pos, int32_t l)
{
  int32_t tl = l;

  if (littleEndian) {
    tl = Snack_SwapLong(tl);
  }

  memcpy(&buf[pos], &tl, sizeof(int32_t));
}

static void
PutBEShort(char *buf, int pos, short s)
{
  short ts = s;
  char *p;
  short *q;

  p = &buf[pos];
  q = (short *) p;

  if (littleEndian) {
    ts = Snack_SwapShort(ts);
  }

  *q = ts;
}

/* Note: pos must be a multiple of 4 */

static void
PutLELong(char *buf, int pos, int32_t l)
{
  int32_t tl = l;
  char *p;
  int32_t *q;

  p = &buf[pos];
  q = (int32_t *) p;

  if (!littleEndian) {
    tl = Snack_SwapLong(tl);
  }

  *q = tl;
}

static void
PutLEShort(char *buf, int pos, short s)
{
  short ts = s;
  char *p;
  short *q;

  p = &buf[pos];
  q = (short *) p;

  if (!littleEndian) {
    ts = Snack_SwapShort(ts);
  }

  *q = ts;
}

extern short shortBuffer[];
extern float floatBuffer[];

static int
ReadSound(readSamplesProc *readProc, Sound *s, Tcl_Interp *interp,
	  Tcl_Channel ch, Tcl_Obj *obj, int startpos, int endpos)
{
  int tot, totrlen = 0, res, i, j = s->loadOffset, size;
  char *b = (char *) shortBuffer;

  if (s->debug > 1) Snack_WriteLogInt("  Enter ReadSound", s->length);

  if (s->length > 0) {
    if (endpos < 0 || endpos > (s->length - 1)) {
      endpos = s->length - 1;
    }
    s->length = endpos - startpos + 1;
    if (s->length < 0) s->length = 0;
    if (Snack_ResizeSoundStorage(s, s->length) != TCL_OK) {
      s->length = 0;
      Tcl_AppendResult(interp, "Memory allocation failed", NULL);
      return TCL_ERROR;
    }
  }
  if (s->encoding == SNACK_DOUBLE) {
    s->sampsize = 8;
  }
  if (s->length == -1) {
    tot = 1 << 30;
  } else {
    tot = (s->length - s->loadOffset) * s->sampsize * s->nchannels;
  }
  Snack_ProgressCallback(s->cmdPtr, interp, "Reading sound", 0.0);

  while (tot > 0) {
    int rlen;

    if (s->encoding != LIN24) {
      size = min(tot, sizeof(short) * PBSIZE);
    } else {
      size = min(tot, sizeof(short) * (PBSIZE - 1));
    }
    /* Samples on disk are 8 bytes -> make sure they fit in buffer */
    if (s->encoding == SNACK_DOUBLE && size > (PBSIZE / 2)) {
      size /= 2;
    }
    if (s->length == -1) {
      if (Snack_ResizeSoundStorage(s, s->maxlength+1) != TCL_OK) {
	s->length = 0;
	Tcl_AppendResult(interp, "Memory allocation failed", NULL);
	return TCL_ERROR;
      }
    }
    if (ch != NULL) {
      if (readProc == NULL) {
	rlen = Tcl_Read(ch, b, size);
	tot -= size;
      } else {
	size = min(s->length * s->nchannels, PBSIZE);
	rlen = (readProc)(s, interp, ch, NULL, (float*)&floatBuffer, size);
	Snack_PutSoundData(s, totrlen / s->sampsize, &floatBuffer, rlen);
	if (rlen > 0) {
	  rlen *= s->sampsize;
	  tot -= rlen;
	}
      }
      if (rlen < 0) {
	Tcl_AppendResult(interp, "Error reading data", NULL);
	return TCL_ERROR;
      }
      totrlen += rlen;
      if (rlen < size) {
	tot = 0;
      }
    } else {
      int length = 0;
      unsigned char *ptr = NULL;
      if (useOldObjAPI) {
	ptr = (unsigned char *) obj->bytes;
      } else {
#ifdef TCL_81_API
	Tcl_GetByteArrayFromObj(obj, &length);
	ptr = Tcl_GetByteArrayFromObj(obj, NULL);
#endif
      }
      if (readProc == NULL) {
	memcpy(b, &ptr[totrlen + s->headSize + startpos * s->sampsize
		      * s->nchannels], size);
	totrlen += size;
	tot -= size;
      } else {
	size = min(tot / (s->sampsize * s->nchannels), PBSIZE);
	/*printf("%d cnk %d obj %d slen %d\n", tot, size, length, s->length);*/
	rlen = (readProc)(s, interp, NULL, (char *) ptr, (float*)&floatBuffer,
			  size);
	Snack_PutSoundData(s, totrlen / s->sampsize, &floatBuffer, rlen);
	rlen *= s->sampsize;
	totrlen += rlen;
	tot -= rlen;
	if (rlen < size) {
	  tot = 0;
	}
      }
    }

    if (readProc == NULL) { /* unpack block */
      unsigned char *q = (unsigned char *) b;
      char   *sc = (char *)   b;
      short  *r  = (short *)  b;
      int    *is = (int *)    b;
      float  *fs = (float *)  b;
      double *fd = (double *) b;

      if (s->precision == SNACK_SINGLE_PREC) {
	for (i = 0; i < size / s->sampsize; i++, j++) {
          int writeblock = (j >> FEXP);
          if (writeblock >= s->nblks) {
	    /* Reached end of allocated blocks for s */
	    break;
          }
	  switch (s->encoding) {
	  case LIN16:
	    if (s->swap) *r = Snack_SwapShort(*r);
	    FSAMPLE(s, j) = (float) *r++;
	    break;
	  case LIN32:
	    if (s->swap) *is = Snack_SwapLong(*is);
	    FSAMPLE(s, j) = (float) *is++;
	    break;
	  case SNACK_FLOAT:
	    if (s->swap) *fs = (float) Snack_SwapFloat(*fs);
	    FSAMPLE(s, j) = (float) *fs++;
	    break;
	  case SNACK_DOUBLE:
	    if (s->swap) *fd = (float) Snack_SwapDouble(*fd);
	    FSAMPLE(s, j) = (float) *fd++;
	    break;
	  case ALAW:
	    FSAMPLE(s, j) = (float) Snack_Alaw2Lin(*q++);
	    break;
	  case MULAW:
	    FSAMPLE(s, j) = (float) Snack_Mulaw2Lin(*q++);
	    break;
	  case LIN8:
	    FSAMPLE(s, j) = (float) *sc++;
	    break;
	  case LIN8OFFSET:
	    FSAMPLE(s, j) = (float) *q++;
	    break;
	  case LIN24:
	  case LIN24PACKED:
	    {
	      int ee;
	      if (s->swap) {
		if (littleEndian) {
		  ee = 0;
		} else {
		  ee = 1;
		}
	      } else {
		if (littleEndian) {
		  ee = 1;
		} else {
		  ee = 0;
		}		
	      }
	      if (ee) {
		int t = *q++;
		t |= *q++ << 8;
		t |= *q++ << 16;
		if (t & 0x00800000) {
		  t |= (unsigned int) 0xff000000;
		}
	        FSAMPLE(s, j) = (float) t;
	      } else {
		int t = *q++ << 16;
		t |= *q++ << 8;
		t |= *q++;
		if (t & 0x00800000) {
		  t |= (unsigned int) 0xff000000;
		}
		FSAMPLE(s, j) = (float) t;
	      }
	      break;
	    }
	  }
	}
      } else {   /*s->precision == SNACK_DOUBLE_PREC */
	for (i = 0; i < size / s->sampsize; i++, j++) {
          int writeblock = (j >> DEXP);
          if (writeblock >= s->nblks) {
	    /* Reached end of allocated blocks for s */
	    break;
          }
	  switch (s->encoding) {
	  case LIN16:
	    DSAMPLE(s, j) = (float) *r++;
	    break;
	  case LIN32:
	    DSAMPLE(s, j) = (float) *is++;
	    break;
	  case SNACK_FLOAT:
	    DSAMPLE(s, j) = (float) *fs++;
	    break;
	  case ALAW:
	    DSAMPLE(s, j) = (float) Snack_Alaw2Lin(*q++);
	    break;
	  case MULAW:
	    DSAMPLE(s, j) = (float) Snack_Mulaw2Lin(*q++);
	    break;
	  case LIN8:
	    DSAMPLE(s, j) = (float) *sc++;
	    break;
	  case LIN8OFFSET:
	    DSAMPLE(s, j) = (float) *q++;
	    break;
	  case LIN24:
	  case LIN24PACKED:
	    {
	      if (littleEndian) {
		int t = *q++;
		t |= *q++ << 8;
		t |= *q++ << 16;
		if (t & 0x00800000) {
		  t |= (unsigned int) 0xff000000;
		}
		DSAMPLE(s, j) = (float) t;
	      } else {
		int t = *q++ << 16;
		t |= *q++ << 8;
		t |= *q++;
		if (t & 0x00800000) {
		  t |= (unsigned int) 0xff000000;
		}
		DSAMPLE(s, j) = (float) t;
	      }
	      break;
	    }
	  }
	}
      }  /*s->precision == SNACK_DOUBLE_PREC */
    } /* unpack block */

    res = Snack_ProgressCallback(s->cmdPtr, interp, "Reading sound",
				 (double) totrlen /
				 (s->length * s->sampsize * s->nchannels));
    if (res != TCL_OK) {
      Snack_ResizeSoundStorage(s, 0);
      s->length = 0;
      return TCL_ERROR;
    }
  }

  if ((double) totrlen / (s->length * s->sampsize * s->nchannels) != 1.0) {
    Snack_ProgressCallback(s->cmdPtr, interp, "Reading sound", 1.0);
  }
  if (s->length * s->sampsize * s->nchannels != totrlen) {
    s->length = totrlen / (s->sampsize * s->nchannels);
  }
  if (s->length == -1) {
    s->length = totrlen / (s->sampsize * s->nchannels);
  }

  if (s->loadOffset > 0) {
    if (s->precision == SNACK_SINGLE_PREC) {
      for (i = 0; i < s->loadOffset; i++) {
	FSAMPLE(s, i) = 0.0f;
      }
    } else {
      for (i = 0; i < s->loadOffset; i++) {
	DSAMPLE(s, i) = 0.0;
      }
    }	
    s->length += s->loadOffset;
    s->loadOffset = 0;
  }
  if (s->encoding == SNACK_DOUBLE) {
    s->sampsize = 4;
  }

  if (s->debug > 1) Snack_WriteLogInt("  Exit ReadSound", s->length);

  return TCL_OK;
}

int
WriteSound(writeSamplesProc *writeProc, Sound *s, Tcl_Interp *interp,
	   Tcl_Channel ch, Tcl_Obj *obj, int startpos, int len)
{
  int i = 0, j;
  short sh;
  int   is;
  float fs;
  unsigned char uc;
  char c;

  if (s->debug > 1) Snack_WriteLog("  Enter WriteSound\n");

  if (s->inByteOrder == SNACK_NATIVE && s->swap) {
    if (littleEndian) {
      s->inByteOrder = SNACK_BIGENDIAN;
    } else {
      s->inByteOrder = SNACK_LITTLEENDIAN;
    }
  }

  startpos *= s->nchannels;
  len      *= s->nchannels;

  if (ch != NULL) {
    Snack_ProgressCallback(s->cmdPtr, interp, "Writing sound", 0.0);
    if (writeProc == NULL) {
      for (i = startpos; i < startpos + len; i++) {

	if (s->storeType == SOUND_IN_MEMORY || s->readStatus == READ) {
	  fs = FSAMPLE(s, i);
	} else {
	  fs = GetSample(&s->linkInfo, i);
	}

	/* pack sample */

	switch (s->encoding) {
	case LIN16:
	  if (fs > 32767.0f)  fs = 32767.0f;
	  if (fs < -32768.0f) fs = -32768.0f;
	  sh = (short) fs;
	  switch (s->inByteOrder) {
	  case SNACK_NATIVE:
	    if (Tcl_Write(ch, (char *) &sh, 2) == -1) return TCL_ERROR;
	    break;
	  case SNACK_BIGENDIAN:
	    if (WriteBEShort(ch, sh) == -1) return TCL_ERROR;
	    break;
	  case SNACK_LITTLEENDIAN:
	    if (WriteLEShort(ch, sh) == -1) return TCL_ERROR;
	    break;
	  }
	  break;
	case LIN32:
	  if (fs > 2147483647.0f)  fs = 2147483647.0f;
	  if (fs < -2147483648.0f) fs = -2147483648.0f;
	  is = (int) fs;
	  switch (s->inByteOrder) {
	  case SNACK_NATIVE:
	    break;
	  case SNACK_BIGENDIAN:
	    if (littleEndian) {
	      is = Snack_SwapLong(is);
	    }
	    break;
	  case SNACK_LITTLEENDIAN:
	    if (!littleEndian) {
	      is = Snack_SwapLong(is);
	    }
	    break;
	  }
	  if (Tcl_Write(ch, (char *) &is, 4) == -1) return TCL_ERROR;
	  break;
	case SNACK_FLOAT:
	  if (fs > 32767.0f)  fs = 32767.0f;
	  if (fs < -32768.0f) fs = -32768.0f;
	  switch (s->inByteOrder) {
	  case SNACK_NATIVE:
	    break;
	  case SNACK_BIGENDIAN:
	    if (littleEndian) {
	      fs = Snack_SwapFloat(fs);
	    }
	    break;
	  case SNACK_LITTLEENDIAN:
	    if (!littleEndian) {
	      fs = Snack_SwapFloat(fs);
	    }
	    break;
	  }
	  if (Tcl_Write(ch, (char *) &fs, 4) == -1) return TCL_ERROR;
	  break;
	case ALAW:
	  {
	    if (fs > 32767.0f)  fs = 32767.0f;
	    if (fs < -32768.0f) fs = -32768.0f;
	    uc = Snack_Lin2Alaw((short) fs);
	    if (Tcl_Write(ch, (char *)&uc, 1) == -1) return TCL_ERROR;
	    break;
	  }
	case MULAW:
	  {
	    if (fs > 32767.0f)  fs = 32767.0f;
	    if (fs < -32768.0f) fs = -32768.0f;
	    uc = Snack_Lin2Mulaw((short) fs);
	    if (Tcl_Write(ch, (char *)&uc, 1) == -1) return TCL_ERROR;
	    break;
	  }
	case LIN8:
	  {
	    if (fs > 127.0f)  fs = 127.0f;
	    if (fs < -128.0f) fs = -128.0f;
	    c = (char) fs;
	    if (Tcl_Write(ch, (char *)&c, 1) == -1) return TCL_ERROR;
	    break;
	  }
	case LIN8OFFSET:
	  {
	    if (fs > 255.0f) fs = 255.0f;
	    if (fs < 0.0f)  fs = 0.0f;
	    uc = (unsigned char) fs;
	    if (Tcl_Write(ch, (char *)&uc, 1) == -1) return TCL_ERROR;
	    break;
	  }
	case LIN24:
	case LIN24PACKED:
	  {
	    int offset = 0;
	    union {
	      char c[sizeof(int)];
	      int i;
	    } pack;

	    if (fs > 8388607.0f)  fs = 8388607.0f;
	    if (fs < -8388608.0f) fs = -8388608.0f;
	    is = (int) fs;
	    switch (s->inByteOrder) {
	    case SNACK_NATIVE:
	      break;
	    case SNACK_BIGENDIAN:
	    if (littleEndian) {
	      is = Snack_SwapLong(is);
	    }
	    break;
	    case SNACK_LITTLEENDIAN:
	      if (!littleEndian) {
		is = Snack_SwapLong(is);
	      }
	      break;
	    }
	    
	    if (littleEndian) {
	      offset = 1;
	    } else {
	      offset = 1;
	    }
	    pack.i = (int) is;
	    if (Tcl_Write(ch, (char *) &pack.c[offset], 3) == -1) {
	      return TCL_ERROR;
	    }
	  }
	}
	if ((i % 100000) == 99999) {
	  int res = Snack_ProgressCallback(s->cmdPtr, interp, "Writing sound",
					   (double)(i-startpos)/len);
	  if (res != TCL_OK) {
	    return TCL_ERROR;
	  }
	}
      }
    } else { /* writeProc != NULL */
      int tot = len;

      while (tot > 0) {
	int size = min(tot, FBLKSIZE/2), res;

	(writeProc)(s, ch, obj, startpos, size);

	tot -= size;
	startpos += size;
	res = Snack_ProgressCallback(s->cmdPtr, interp, "Writing sound",
				     1.0-(double)tot/len);
	if (res != TCL_OK) {
	  return TCL_ERROR;
	}
      }
    }
    Snack_ProgressCallback(s->cmdPtr, interp, "Writing sound", 1.0);
  } else { /* ch == NULL */
    unsigned char *p = NULL;
    
    if (useOldObjAPI) {
      Tcl_SetObjLength(obj, s->headSize + len * s->sampsize);
      p = (unsigned char *) &obj->bytes[s->headSize];
    } else {
#ifdef TCL_81_API
      p = Tcl_SetByteArrayLength(obj, s->headSize +len * s->sampsize);
      p = &p[s->headSize];
#endif
    }
    for (i = startpos, j = 0; i < startpos + len; i++, j++) {
      short *sp = (short *) p;
      int   *ip = (int *) p;
      float *fp = (float *) p;
      char  *cp = (char *) p;

      if (s->storeType == SOUND_IN_MEMORY) {
	fs = FSAMPLE(s, i);
      } else {
	fs = GetSample(&s->linkInfo, i);
      }
      
      /* pack sample */
      
      switch (s->encoding) {
      case LIN16:
	if (fs > 32767.0f)  fs = 32767.0f;
	if (fs < -32768.0f) fs = -32768.0f;
	sh = (short) fs;
	switch (s->inByteOrder) {
	case SNACK_NATIVE:
	  break;
	case SNACK_BIGENDIAN:
	  if (littleEndian) {
	    sh = Snack_SwapShort(sh);
	  }
	  break;
	case SNACK_LITTLEENDIAN:
	  if (!littleEndian) {
	    sh = Snack_SwapShort(sh);
	  }
	  break;
	}
	sp[j] = sh;
	break;
      case LIN32:
	if (fs > 2147483647.0f)  fs = 2147483647.0f;
	if (fs < -2147483648.0f) fs = -2147483648.0f;
	is = (int) fs;
	switch (s->inByteOrder) {
	case SNACK_NATIVE:
	  break;
	case SNACK_BIGENDIAN:
	  if (littleEndian) {
	    is = Snack_SwapLong(is);
	  }
	  break;
	case SNACK_LITTLEENDIAN:
	  if (!littleEndian) {
	    is = Snack_SwapLong(is);
	  }
	  break;
	}
	ip[j] = is;
	break;
      case SNACK_FLOAT:
	if (fs > 32767.0f)  fs = 32767.0f;
	if (fs < -32768.0f) fs = -32768.0f;
	switch (s->inByteOrder) {
	case SNACK_NATIVE:
	  break;
	case SNACK_BIGENDIAN:
	  if (littleEndian) {
	    fs = Snack_SwapFloat(fs);
	  }
	  break;
	case SNACK_LITTLEENDIAN:
	  if (!littleEndian) {
	    fs = Snack_SwapFloat(fs);
	  }
	  break;
	}
	fp[j] = fs;
	break;
      case ALAW:
	{
	  if (fs > 32767.0f)  fs = 32767.0f;
	  if (fs < -32768.0f) fs = -32768.0f;
	  p[j] = Snack_Lin2Alaw((short) fs);
	  break;
	}
      case MULAW:
	{
	  if (fs > 32767.0f)  fs = 32767.0f;
	  if (fs < -32768.0f) fs = -32768.0f;
	  p[j] = Snack_Lin2Mulaw((short) fs);
	  break;
	}
      case LIN8:
	{
	  if (fs > 127.0f)  fs = 127.0f;
	  if (fs < -128.0f) fs = -128.0f;
	  cp[j] = (char) fs;
	  break;
	}
      case LIN8OFFSET:
	{
	  if (fs > 255.0f) fs = 255.0f;
	  if (fs < 0.0f)  fs = 0.0f;
	  p[j] = (unsigned char) fs;
	  break;
	}
      case LIN24:
      case LIN24PACKED:
	{
	  int offset = 0;
	  union {
	    char c[sizeof(int)];
	    int i;
	  } pack;

	  if (fs > 8388607.0f) fs = 8388607.0f;
	  if (fs < -8388608.0f) fs = -8388608.0f;
	  is = (int) fs;
	  
	  switch (s->inByteOrder) {
	  case SNACK_NATIVE:
	    break;
	  case SNACK_BIGENDIAN:
	    if (littleEndian) {
	      is = Snack_SwapLong(is);
	    }
	    break;
	  case SNACK_LITTLEENDIAN:
	    if (!littleEndian) {
	      is = Snack_SwapLong(is);
	    }
	    break;
	  }
	  
	  if (littleEndian) {
	    offset = 0;
	  } else {
	    offset = 1;
	  }
	  pack.i = (int) is;
	  memcpy(&p, &pack.c[offset],3);
	  p += 3;
	}
      }
    }
  }
  if (s->debug > 1) Snack_WriteLog("  Exit WriteSound\n");

  return TCL_OK;
}
#define NFIRSTSAMPLES 40000
#define DEFAULT_MULAW_RATE 8000
#define DEFAULT_ALAW_RATE 8000
#define DEFAULT_LIN8OFFSET_RATE 11025
#define DEFAULT_LIN8_RATE 11025

typedef enum {
  GUESS_LIN16,
  GUESS_LIN16S,
  GUESS_ALAW,
  GUESS_MULAW,
  GUESS_LIN8OFFSET,
  GUESS_LIN8,
  GUESS_LIN24,
  GUESS_LIN24S
} sampleEncoding;

#define GUESS_FFT_LENGTH 512
#define SNACK_DEFAULT_GFWINTYPE SNACK_WIN_HAMMING

int
GuessEncoding(Sound *s, unsigned char *buf, int len) {
  int i, j, format;
  float energyLIN16 = 0.0, energyLIN16S = 0.0;
  float energyMULAW = 0.0, energyALAW = 0.0;
  float energyLIN8  = 0.0, energyLIN8O = 0.0, minEnergy;
  float energyLIN24 = 0.0, energyLIN24S = 0.0;
  float fft[GUESS_FFT_LENGTH];
  float totfft[GUESS_FFT_LENGTH];
  float hamwin[GUESS_FFT_LENGTH];
  double toterg = 0.0, cmperg = 0.0, minBin = 0.0;

  if (s->debug > 2) Snack_WriteLogInt("    Enter GuessEncoding", len);

  /*
    Byte order and sample encoding detection suggested by David van Leeuwen
    */
  
  for (i = 0; i < len / 2; i++) {
    short sampleLIN16  = ((short *)buf)[i];
    short sampleLIN16S = Snack_SwapShort(sampleLIN16);
    short sampleMULAW  = Snack_Mulaw2Lin(buf[i]);
    short sampleALAW   = Snack_Alaw2Lin(buf[i]);
    short sampleLIN8O  = (char)(buf[i] ^ 128) << 8;
    short sampleLIN8   = (char)buf[i] << 8;

    energyLIN16  += (float) sampleLIN16  * (float) sampleLIN16;
    energyLIN16S += (float) sampleLIN16S * (float) sampleLIN16S;
    energyMULAW  += (float) sampleMULAW  * (float) sampleMULAW;
    energyALAW   += (float) sampleALAW   * (float) sampleALAW;
    energyLIN8O  += (float) sampleLIN8O  * (float) sampleLIN8O;
    energyLIN8   += (float) sampleLIN8   * (float) sampleLIN8;
  }

  for (i = 0; i < len / 2; i+=3) {
    union {
      char c[sizeof(int)];
      int s;
    } sampleLIN24, sampleLIN24S;

    sampleLIN24.c[0] = (char)buf[i];
    sampleLIN24.c[1] = (char)buf[i+1];
    sampleLIN24.c[2] = (char)buf[i+2];
    sampleLIN24S.c[2] = (char)buf[i];
    sampleLIN24S.c[1] = (char)buf[i+1];
    sampleLIN24S.c[0] = (char)buf[i+2];

    sampleLIN24.s /= 65536;
    sampleLIN24S.s /= 65536;
    energyLIN24  += (float) sampleLIN24.s * (float) sampleLIN24.s;
    energyLIN24S += (float) sampleLIN24S.s * (float) sampleLIN24S.s;
  }
  
  format = GUESS_LIN16;
  minEnergy = energyLIN16;

  if (energyLIN16S < minEnergy) {
    format = GUESS_LIN16S;
    minEnergy = energyLIN16S;
  }
  if (energyALAW < minEnergy) {
    format = GUESS_ALAW;
    minEnergy = energyALAW;
  }
  if (energyMULAW < minEnergy) {
    format = GUESS_MULAW;
    minEnergy = energyMULAW;
  }
  if (energyLIN8O < minEnergy) {
    format = GUESS_LIN8OFFSET;
    minEnergy = energyLIN8O;
  }
  if (energyLIN8 < minEnergy) {
    format = GUESS_LIN8;
    minEnergy = energyLIN8;
  }
  /*if (energyLIN24 < minEnergy) {
    format = GUESS_LIN24;
    minEnergy = energyLIN24;
  }
  if (energyLIN24S < minEnergy) {
    format = GUESS_LIN24S;
    minEnergy = energyLIN24S;
  }
  printf("AA %f %f %f %f\n", energyLIN16, energyLIN16S, energyLIN24, energyLIN24S);*/
  switch (format) {
  case GUESS_LIN16:
    s->swap = 0;
    if (s->sampsize == 1) {
      s->length /= 2;
    }
    s->encoding = LIN16;
    s->sampsize = 2;
    break;
  case GUESS_LIN16S:
    s->swap = 1;
    if (s->sampsize == 1) {
      s->length /= 2;
    }
    s->encoding = LIN16;
    s->sampsize = 2;
    break;
  case GUESS_ALAW:
    if (s->sampsize == 2) {
      s->length *= 2;
    }
    s->encoding = ALAW;
    s->sampsize = 1;
    if (s->guessRate) {
      s->samprate = DEFAULT_ALAW_RATE;
    }
    break;
  case GUESS_MULAW:
    if (s->sampsize == 2) {
      s->length *= 2;
    }
    s->encoding = MULAW;
    s->sampsize = 1;
    if (s->guessRate) {
      s->samprate = DEFAULT_MULAW_RATE;
    }
    break;
  case GUESS_LIN8OFFSET:
    if (s->sampsize == 2) {
      s->length *= 2;
    }
    s->encoding = LIN8OFFSET;
    s->sampsize = 1;
    if (s->guessRate) {
      s->samprate = DEFAULT_LIN8OFFSET_RATE;
    }
    break;
  case GUESS_LIN8:
    if (s->sampsize == 2) {
      s->length *= 2;
    }
    s->encoding = LIN8;
    s->sampsize = 1;
    if (s->guessRate) {
      s->samprate = DEFAULT_LIN8_RATE;
    }
    break;
  case GUESS_LIN24:
    s->swap = 0;
    s->encoding = LIN24;
    s->sampsize = 4;
    break;
  case GUESS_LIN24S:
    s->swap = 1;
    s->encoding = LIN24;
    s->sampsize = 4;
    break;
  }

  if (s->guessRate && s->encoding == LIN16) {
    for (i = 0; i < GUESS_FFT_LENGTH; i++) {
      totfft[i] = 0.0;
    }
    Snack_InitFFT(GUESS_FFT_LENGTH);
    Snack_InitWindow(hamwin, GUESS_FFT_LENGTH, GUESS_FFT_LENGTH / 2,
		     SNACK_DEFAULT_GFWINTYPE);
    for (i = 0; i < (len / s->sampsize) / (GUESS_FFT_LENGTH + 1); i++) {
      for (j = 0; j < GUESS_FFT_LENGTH; j++) {
	short sample  = ((short *)buf)[j + i * (GUESS_FFT_LENGTH / 2)];
	if (s->swap) {
	  sample = Snack_SwapShort(sample);
	}
	fft[j] = (float) sample * hamwin[j];
      }
      Snack_DBPowerSpectrum(fft);
      for (j = 0; j < GUESS_FFT_LENGTH / 2; j++) {
	totfft[j] += fft[j];
      }
    }
    for (i = 0; i < GUESS_FFT_LENGTH / 2; i++) {
      if (totfft[i] < minBin) minBin = totfft[i];
    }
    for (i = 0; i < GUESS_FFT_LENGTH / 2; i++) {
      toterg += (totfft[i] - minBin);
    }
    for (i = 0; i < GUESS_FFT_LENGTH / 2; i++) {
      cmperg += (totfft[i] - minBin);
      if (cmperg > toterg / 2.0) break;
    }

    if (i > 100) {
      /* Silence, don't guess */
    } else if (i > 64) {
      s->samprate = 8000;
    } else if (i > 46) {
      s->samprate = 11025;
    } else if (i > 32) {
      s->samprate = 16000;
    } else if (i > 23) {
      s->samprate = 22050;
    } else if (i > 16) {
      s->samprate = 32000;
    } else if (i > 11) {
      s->samprate = 44100;
    }
  }

  if (s->debug > 2) Snack_WriteLogInt("    Exit GuessEncoding", s->encoding);
  
  return TCL_OK;
}

static int
GetRawHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	     char *buf)
{
  if (s->debug > 2) Snack_WriteLog("    Reading RAW header\n");

  if (ch != NULL) {
    TCL_SEEK(ch, 0, SEEK_END);
    s->length = (TCL_TELL(ch) - s->skipBytes) / (s->sampsize * s->nchannels);
  }
  if (obj != NULL) {
    if (useOldObjAPI) {
      s->length = (obj->length  - s->skipBytes) / (s->sampsize * s->nchannels);
    } else {
#ifdef TCL_81_API
      int length = 0;
      
      Tcl_GetByteArrayFromObj(obj, &length);
      s->length = (length - s->skipBytes) / (s->sampsize * s->nchannels);
#endif
    }
  }
  s->headSize = s->skipBytes;

  return TCL_OK;
}

static int
PutRawHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	     int objc, Tcl_Obj *CONST objv[], int len)
{
  s->headSize = 0;
  
  return TCL_OK;
}

#define NIST_HEADERSIZE 1024

static int
GetSmpHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	     char *buf)
{
  char s1[100], s2[100];
  int i = 0, cont = 1;

  if (s->debug > 2) Snack_WriteLog("    Reading SMP header\n");

  if (s->firstNRead < NIST_HEADERSIZE) {
    if (Tcl_Read(ch, (char *)&buf[s->firstNRead],
		 NIST_HEADERSIZE-s->firstNRead) < 0) {
      return TCL_ERROR; 
    }
  }

  do {
    sscanf(&buf[i], "%s", s1);
    if (strncmp(s1, "sftot", 5) == 0) {
      sscanf(&buf[i+6], "%d", &s->samprate);
      if (s->debug > 3) {
	Snack_WriteLogInt("      Setting rate", s->samprate);
      }
    } else if (strncmp(s1, "msb", 3) == 0) {
      sscanf(&buf[i+4], "%s", s2);
      if (s->debug > 3) {
	Snack_WriteLog("      ");
	Snack_WriteLog(s2);
	Snack_WriteLog(" byte order\n");
      }
    } else if (strncmp(s1, "nchans", 6) == 0) {
      sscanf(&buf[i+7], "%d", &s->nchannels);
      if (s->debug > 3) {
	Snack_WriteLogInt("      Setting number of channels", s->nchannels);
      }
    } else if (buf[i] == 0) {
      cont = 0;
    }
    while (buf[i] != 10 && buf[i] != 0) i++;
    i++;
  } while (cont);

  s->encoding = LIN16;
  s->sampsize = 2;
  s->swap = 0;

  if (ch != NULL) {
    TCL_SEEK(ch, 0, SEEK_END);
    s->length = (TCL_TELL(ch) - NIST_HEADERSIZE) / (s->sampsize * s->nchannels);
  }
  if (obj != NULL) {
    if (useOldObjAPI) {
      s->length = (obj->length - NIST_HEADERSIZE) / (s->sampsize * s->nchannels);
    } else {
#ifdef TCL_81_API
      int length = 0;
      
      Tcl_GetByteArrayFromObj(obj, &length);
      s->length = (length - NIST_HEADERSIZE) / (s->sampsize * s->nchannels);
#endif
    }
  }
  s->headSize = NIST_HEADERSIZE;
  if (strcmp(s2, "first") == 0) {
    if (littleEndian) {
      SwapIfLE(s);
    }
  } else {
    if (!littleEndian) {
      SwapIfBE(s);
    }
  }

  return TCL_OK;
}

static int
PutSmpHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	     int objc, Tcl_Obj *CONST objv[], int len)
{
  int i = 0;
  char buf[HEADBUF];

  if (s->encoding != LIN16) {
    Tcl_AppendResult(interp, "Unsupported encoding format", NULL);
    return -1;
  }

  i += (int) sprintf(&buf[i], "file=samp\r\n");
  i += (int) sprintf(&buf[i], "sftot=%d\r\n", s->samprate);
  if (littleEndian) {
    i += (int) sprintf(&buf[i], "msb=last\r\n");
  } else {
    i += (int) sprintf(&buf[i], "msb=first\r\n");
  }
  i += (int) sprintf(&buf[i], "nchans=%d\r\n", s->nchannels);
  i += (int) sprintf(&buf[i],"preemph=none\r\nborn=snack\r\n=\r\n%c%c%c", 0,4,26);

  for (;i < NIST_HEADERSIZE; i++) buf[i] = 0;

  if (ch != NULL) {
    if (Tcl_Write(ch, buf, NIST_HEADERSIZE) == -1) {
      Tcl_AppendResult(interp, "Error while writing header", NULL);
      return -1;
    }
  } else {
    if (useOldObjAPI) {
      Tcl_SetObjLength(obj, NIST_HEADERSIZE);
      memcpy(obj->bytes, buf, NIST_HEADERSIZE);
    } else {
#ifdef TCL_81_API
      unsigned char *p = Tcl_SetByteArrayLength(obj, NIST_HEADERSIZE);
      memcpy(p, buf, NIST_HEADERSIZE);
#endif
    }
  }
  s->inByteOrder = SNACK_NATIVE;
  s->swap = 0;
  s->headSize = NIST_HEADERSIZE;
  
  return TCL_OK;
}

#define SNACK_SD_INT 20

static int
GetSdHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	    char *buf)
{
  int datastart, len, i, j;
  double freq = 16000.0;
  double start = 0.0;
  int first = 1;

  if (s->debug > 2) Snack_WriteLog("    Reading SD header\n");

  datastart = GetBELong(buf, 8);
  s->nchannels = GetBELong(buf, 144);

  for (i = 0; i < s->firstNRead; i++) { 
    if (strncasecmp("record_freq", &buf[i], strlen("record_freq")) == 0) {
      i = i + 18;
      if (littleEndian) {
	for (j = 0; j < 4; j++) {
	  char c = buf[i+j];
	  
	  buf[i+j] = buf[i+7-j];
	  buf[i+7-j] = c;
	}
      }
      memcpy(&freq, &buf[i], 8);
    }
    if (strncasecmp("start_time", &buf[i], strlen("start_time")) == 0 && first) {
      first = 0;
      i = i + 18;
      if (littleEndian) {
	for (j = 0; j < 4; j++) {
	  char c = buf[i+j];
	  
	  buf[i+j] = buf[i+7-j];
	  buf[i+7-j] = c;
	}
      }
      memcpy(&start, &buf[i], 8);

      if (s->extHead != NULL && s->extHeadType != SNACK_SD_INT) {
	Snack_FileFormat *ff;
	
	for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
	  if (strcmp(s->fileType, ff->name) == 0) {
	    if (ff->freeHeaderProc != NULL) {
	      (ff->freeHeaderProc)(s);
	    }
	  }
	}
      }
      if (s->extHead == NULL) {
	s->extHead = (char *) ckalloc(sizeof(double));
	memcpy(s->extHead, &buf[i], sizeof(double));
	s->extHeadType = SNACK_SD_INT;
      }
    }
  }
  
  s->encoding = LIN16;
  s->sampsize = 2;
  s->samprate = (int) freq;
  s->loadOffset = 0; /*(int) (start * s->samprate + 0.5);*/

  if (ch != NULL) {
    TCL_SEEK(ch, 0, SEEK_END);
    len = TCL_TELL(ch);
    if (len == 0 || len < datastart) {
      Tcl_AppendResult(interp, "Failed reading SD header", NULL);
      return TCL_ERROR;
    }
    s->length = (len - datastart) / s->sampsize + s->loadOffset;
  }
  if (obj != NULL) {
    if (useOldObjAPI) {
      s->length = obj->length / s->sampsize + s->loadOffset;
    } else {
#ifdef TCL_81_API
      int length = 0;
      
      Tcl_GetByteArrayFromObj(obj, &length);
      s->length = length / s->sampsize + s->loadOffset;
#endif
    }
  }
  s->length /= s->nchannels;
  s->headSize = datastart;
  SwapIfLE(s);

  return TCL_OK;
}

static int
ConfigSdHeader(Sound *s, Tcl_Interp *interp, int objc,
                Tcl_Obj *CONST objv[])
{
  int index;
  static CONST84 char *optionStrings[] = {
    "-start_time", NULL
  };
  enum options {
    STARTTIME
  };

  if (s->extHeadType != SNACK_SD_INT || objc < 3) return 0;

  if (objc == 3) { /* get option */
    if (Tcl_GetIndexFromObj(interp, objv[2], optionStrings, "option", 0,
                            &index) != TCL_OK) {
      Tcl_AppendResult(interp, ", or\n", NULL);
      return 0;
    }

    switch ((enum options) index) {
    case STARTTIME:
      {
	double *start = (double *) s->extHead;
        Tcl_SetObjResult(interp, Tcl_NewDoubleObj(*start));
        break;
      }
    }
  }

  return 1;
}

static void
FreeSdHeader(Sound *s)
{
  if (s->debug > 2) Snack_WriteLog("    Enter FreeSdHeader\n");

  if (s->extHead != NULL) {
    ckfree((char *)s->extHead);
    s->extHead = NULL;
    s->extHeadType = 0;
  }
  
  if (s->debug > 2) Snack_WriteLog("    Exit FreeSdHeader\n");
}

#define SND_FORMAT_MULAW_8   1
#define SND_FORMAT_LINEAR_8  2
#define SND_FORMAT_LINEAR_16 3
#define SND_FORMAT_LINEAR_24 4
#define SND_FORMAT_LINEAR_32 5
#define SND_FORMAT_FLOAT     6
#define SND_FORMAT_DOUBLE    7
#define SND_FORMAT_ALAW_8    27

#define AU_HEADERSIZE 28

static int
GetAuHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	    char *buf)
{
  int fmt, hlen, nsamp, nsampfile;

  if (s->debug > 2) Snack_WriteLog("    Reading AU/SND header\n");

  if (s->firstNRead < AU_HEADERSIZE) {
    if (Tcl_Read(ch, (char *)&buf[s->firstNRead],
		 AU_HEADERSIZE-s->firstNRead) < 0) {
      return TCL_ERROR; 
    }
  }
  hlen = GetBELong(buf, 4);
  fmt  = GetBELong(buf, 12);
  
  switch (fmt) {
  case SND_FORMAT_MULAW_8:
    s->encoding = MULAW;
    s->sampsize = 1;
    break;
  case SND_FORMAT_LINEAR_8:
    s->encoding = LIN8;
    s->sampsize = 1;
    break;
  case SND_FORMAT_LINEAR_16:
    s->encoding = LIN16;
    s->sampsize = 2;
    break;
  case SND_FORMAT_LINEAR_24:
    s->encoding = LIN24;
    s->sampsize = 3;
    break;
  case SND_FORMAT_LINEAR_32:
    s->encoding = LIN32;
    s->sampsize = 4;
    break;
  case SND_FORMAT_FLOAT:
    s->encoding = SNACK_FLOAT;
    s->sampsize = 4;
    break;
  case SND_FORMAT_DOUBLE:
    s->encoding = SNACK_DOUBLE;
    s->sampsize = 4;
    break;
  case SND_FORMAT_ALAW_8:
    s->encoding = ALAW;
    s->sampsize = 1;
    break;
  default:
    Tcl_AppendResult(interp, "Unsupported AU format", NULL);
    return TCL_ERROR;
  }
  s->samprate = GetBELong(buf, 16);
  s->nchannels = GetBELong(buf, 20);
  if (hlen < 24) {
    hlen = 24;
  }
  s->headSize = hlen;
  nsamp = GetBELong(buf, 8) / (s->sampsize * s->nchannels);

  if (ch != NULL) {
    TCL_SEEK(ch, 0, SEEK_END);
    nsampfile = (TCL_TELL(ch) - hlen) / (s->sampsize * s->nchannels);
    if (nsampfile < nsamp || nsamp <= 0) {
      nsamp = nsampfile;
    }
  }
  if (obj != NULL) {
    if (useOldObjAPI) {
      nsamp = (obj->length - hlen) / (s->sampsize * s->nchannels);
    } else {
#ifdef TCL_81_API
      int length = 0;
      
      Tcl_GetByteArrayFromObj(obj, &length);
      nsamp = (length - hlen) / (s->sampsize * s->nchannels);
#endif
    }
  }
  if (s->encoding != SNACK_DOUBLE) {
    s->length = nsamp;
  } else {
    s->length = nsamp/2;
  }
  SwapIfLE(s);

  return TCL_OK;
}

static int
PutAuHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	    int objc, Tcl_Obj *CONST objv[], int len)
{
  int fmt = 0;
  char buf[HEADBUF];

  if (s->debug > 2) Snack_WriteLog("    Saving AU/SND\n");

  PutBELong(buf, 0, 0x2E736E64);
  PutBELong(buf, 4, AU_HEADERSIZE);
  PutBELong(buf, 8, len * s->sampsize * s->nchannels);

  switch (s->encoding) {
  case MULAW:
    fmt = SND_FORMAT_MULAW_8;
    break;
  case LIN8:
    fmt = SND_FORMAT_LINEAR_8;
    break;
  case LIN16:
    fmt = SND_FORMAT_LINEAR_16;
    break;
  case LIN24:
    fmt = SND_FORMAT_LINEAR_24;
    break;
  case LIN32:
    fmt = SND_FORMAT_LINEAR_32;
    break;
  case SNACK_FLOAT:
  case SNACK_DOUBLE:
    fmt = SND_FORMAT_FLOAT;
    break;
  case ALAW:
    fmt = SND_FORMAT_ALAW_8;
    break;
  default:
    Tcl_AppendResult(interp, "Unsupported AU format", NULL);
    return -1;
  }
  PutBELong(buf, 12, fmt);

  PutBELong(buf, 16, s->samprate);
  PutBELong(buf, 20, s->nchannels);
  PutBELong(buf, 24, 0);

  if (ch != NULL) {
    if (Tcl_Write(ch, buf, AU_HEADERSIZE) == -1) {
      Tcl_AppendResult(interp, "Error while writing header", NULL);
      return -1;
    }
  } else {
    if (useOldObjAPI) {
      Tcl_SetObjLength(obj, AU_HEADERSIZE);
      memcpy(obj->bytes, buf, AU_HEADERSIZE);
    } else {
#ifdef TCL_81_API
      unsigned char *p = Tcl_SetByteArrayLength(obj, AU_HEADERSIZE);
      memcpy(p, buf, AU_HEADERSIZE);
#endif
    }
  }

  if (len == -1) {
    SwapIfLE(s);
  }
  s->inByteOrder = SNACK_BIGENDIAN;
  s->headSize = AU_HEADERSIZE;
  
  return TCL_OK;
}

#define WAVE_FORMAT_PCM	1
#ifndef WIN
#  define WAVE_FORMAT_IEEE_FLOAT 3
#  define WAVE_FORMAT_ALAW  6
#  define WAVE_FORMAT_MULAW 7
#endif
#define WAVE_EX		(-2)	/* (OxFFFE) in a 2-byte word */

static int
GetHeaderBytes(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, char *buf, 
	       int len)
{
  int rlen = Tcl_Read(ch, &buf[s->firstNRead], len - s->firstNRead);

  if (rlen < len - s->firstNRead){
    Tcl_AppendResult(interp, "Failed reading header bytes", NULL);
    return TCL_ERROR;
  }
  s->firstNRead += rlen;

  return TCL_OK;
}

static int
GetWavHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	     char *buf)
{
  int fmt, nsamp = 0, nsampfile, i = 12, chunkLen;

  if (s->debug > 2) Snack_WriteLog("    Reading WAV header\n");

  /* buf[] = "RIFFxxxxWAVE" */

  while (1) {
    if (strncasecmp("fmt ", &buf[i], strlen("fmt ")) == 0) {
      chunkLen = GetLELong(buf, i + 4) + 8;
      if (s->firstNRead < i + chunkLen) {
	if (GetHeaderBytes(s, interp, ch, buf, i + chunkLen) != TCL_OK) {
	  return TCL_ERROR;
	}
      }
      fmt = GetLEShort(buf, i+8);
      s->nchannels = GetLEShort(buf, i+10);
      s->samprate  = GetLELong(buf, i+12);
      s->sampsize  = GetLEShort(buf, i+22) / 8;

      /* For WAVE-EX, the format is the first two bytes of the GUID */
      if (fmt == WAVE_EX)
	fmt = GetLEShort(buf, i+32);

      switch (fmt) {
      case WAVE_FORMAT_PCM:
	if (s->sampsize == 1) {
	  s->encoding = LIN8OFFSET;
	} else if (s->sampsize == 2) {
	  s->encoding = LIN16;
	} else if (s->sampsize == 3) {
	  s->encoding = LIN24;
	} else if (s->sampsize == 4) {
	  s->encoding = LIN32;
	}
	break;
      case WAVE_FORMAT_IEEE_FLOAT:
	if (s->sampsize == 4) {
	  s->encoding = SNACK_FLOAT;
	} else {
	  s->encoding = SNACK_DOUBLE;
	}
	s->sampsize = 4;
	break;
      case WAVE_FORMAT_ALAW:
	s->encoding = ALAW;
	break;
      case WAVE_FORMAT_MULAW:
	s->encoding = MULAW;
	break;
      default:
	Tcl_AppendResult(interp, "Unsupported WAV format", NULL);
	return TCL_ERROR;
      }
      
      if (s->debug > 3) {
	Snack_WriteLogInt("      fmt chunk parsed", chunkLen);
      }
    } else if (strncasecmp("data", &buf[i], strlen("data")) == 0) {
      nsamp = GetLELong(buf, i + 4) / (s->sampsize * s->nchannels);
      if (s->debug > 3) {
	Snack_WriteLogInt("      data chunk parsed", nsamp);
      }
      break;
    } else { /* unknown chunk */
      chunkLen = GetLELong(buf, i + 4) + 8;

      if (chunkLen < 0) {
	Tcl_AppendResult(interp, "Failed parsing WAV header", NULL);
	return TCL_ERROR;
      }
      while (s->firstNRead < i + chunkLen) {
	if (GetHeaderBytes(s, interp, ch, buf, i + chunkLen) != TCL_OK) {
	  return TCL_ERROR;
	}
      }
      if (s->debug > 3) {
	Snack_WriteLogInt("      Skipping unknown chunk", chunkLen);
      }
    }

    i += chunkLen;
    if (s->firstNRead < i + 8) {
      if (GetHeaderBytes(s, interp, ch, buf, i + 8) != TCL_OK) {
	return TCL_ERROR;
      }
    }
    if (i >= HEADBUF) {
      Tcl_AppendResult(interp, "Failed parsing WAV header", NULL);
      return TCL_ERROR;
    }
  }
  
  s->headSize = i + 8;
  if (ch != NULL) {
    TCL_SEEK(ch, 0, SEEK_END);
    nsampfile = (TCL_TELL(ch) - s->headSize) / (s->sampsize * s->nchannels);
    if (nsampfile < nsamp || nsamp == 0) {
      nsamp = nsampfile;
    }
  }
  if (obj != NULL) {
    if (useOldObjAPI) {
      nsampfile = (obj->length - s->headSize) / (s->sampsize * s->nchannels);
    } else {
#ifdef TCL_81_API
      int length = 0;
      
      Tcl_GetByteArrayFromObj(obj, &length);
      nsampfile = (length - s->headSize) / (s->sampsize * s->nchannels);
#endif
    }
    if (nsampfile < nsamp || nsamp == 0) {
      nsamp = nsampfile;
    }
  }

  if (s->encoding != SNACK_DOUBLE) {
    s->length = nsamp;
  } else {
    s->length = nsamp/2;
  }

  if (s->sampsize == 4 && s->encoding == LIN32) {
    double energyLIN32 = 0.0, energyFLOAT = 0.0;
    
    for (i = s->headSize; i < s->firstNRead / 4; i++) {
      int   sampleLIN32 = ((int   *)buf)[i];
      float sampleFLOAT = ((float *)buf)[i];
      if (!littleEndian) {
	sampleLIN32 = Snack_SwapLong(sampleLIN32);
	sampleFLOAT = Snack_SwapFloat(sampleFLOAT);
      }
      energyLIN32 += (double) (sampleLIN32 * sampleLIN32);
      energyFLOAT += (double) (sampleFLOAT * sampleFLOAT);
    }
    if (fabs(energyLIN32) > fabs(energyFLOAT)) {
      s->encoding = SNACK_FLOAT;
    }
  }

  SwapIfBE(s);

  return TCL_OK;
}

#define SNACK_WAV_HEADERSIZE 44

static int
PutWavHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	     int objc, Tcl_Obj *CONST objv[], int len)
{
  char buf[HEADBUF];

  sprintf(&buf[0], "RIFF");
  if (len != -1) {
    PutLELong(buf, 4, len * s->sampsize * s->nchannels + 36);
  } else {
    SwapIfBE(s);
    PutLELong(buf, 4, 0x7FFFFFFF);
  }
  sprintf(&buf[8], "WAVEfmt ");
  PutLELong(buf, 16, 16);

  switch (s->encoding) {
  case ALAW:
    PutLEShort(buf, 20, WAVE_FORMAT_ALAW);
    break;
  case MULAW:
    PutLEShort(buf, 20, WAVE_FORMAT_MULAW);
    break;
  case SNACK_FLOAT:
  case SNACK_DOUBLE:
    PutLEShort(buf, 20, WAVE_FORMAT_IEEE_FLOAT);
    break;
  default:
    PutLEShort(buf, 20, WAVE_FORMAT_PCM);
  }
  PutLEShort(buf, 22, (short)s->nchannels);
  PutLELong(buf, 24, s->samprate);
  PutLELong(buf, 28, (s->samprate * s->nchannels * s->sampsize * 8 + 7) / 8);
  PutLEShort(buf, 32, (short)((s->nchannels * s->sampsize * 8 + 7) / 8));
  PutLEShort(buf, 34, (short) (s->sampsize * 8));
  sprintf(&buf[36], "data");
  if (len != -1) {
    PutLELong(buf, 40, len * s->sampsize * s->nchannels);
  } else {
    PutLELong(buf, 40, 0x7FFFFFDB);
  }
  if (ch != NULL) {
    if (Tcl_Write(ch, buf, SNACK_WAV_HEADERSIZE) == -1) {
      Tcl_AppendResult(interp, "Error while writing header", NULL);
      return -1;
    }
  } else {
    if (useOldObjAPI) {
      Tcl_SetObjLength(obj, SNACK_WAV_HEADERSIZE);
      memcpy(obj->bytes, buf, SNACK_WAV_HEADERSIZE);
    } else {
#ifdef TCL_81_API
      unsigned char *p = Tcl_SetByteArrayLength(obj, SNACK_WAV_HEADERSIZE);
      memcpy(p, buf, SNACK_WAV_HEADERSIZE);
#endif
    }
  }
  s->inByteOrder = SNACK_LITTLEENDIAN;
  s->headSize = SNACK_WAV_HEADERSIZE;
  
  return TCL_OK;
}

/* See http://www.borg.com/~jglatt/tech/aiff.htm */

static uint32_t
ConvertFloat(unsigned char *buffer)
{
  uint32_t mantissa;
  uint32_t last = 0;
  unsigned char exp;
  
  memcpy(&mantissa, buffer + 2, sizeof(int32_t));
  if (littleEndian) {
    mantissa = Snack_SwapLong(mantissa);
  }
  exp = 30 - *(buffer+1);
  while (exp--) {
    last = mantissa;
    mantissa >>= 1;
  }
  if (last & 0x00000001) mantissa++;
  return(mantissa);
}

static void
StoreFloat(unsigned char * buffer, uint32_t value)
{
  uint32_t exp;
  unsigned char i;
  
  memset(buffer, 0, 10);
  
  exp = value;
  exp >>= 1;
  for (i=0; i<32; i++) {
    exp >>= 1;
    if (!exp) break;
  }
  *(buffer+1) = i;
  
  for (i=32; i; i--) {
    if (value & 0x80000000) break;
    value <<= 1;
  }

  if (littleEndian) {
    value = Snack_SwapLong(value);
  }
  buffer[0] = 0x40;
  memcpy(buffer + 2, &value, sizeof(int32_t));
}

#define ICEILV(n,m)	(((n) + ((m) - 1)) / (m))	/* int n,m >= 0 */
#define RNDUPV(n,m)	((m) * ICEILV (n, m))		/* Round up */

static int
GetAiffHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	      char *buf)
{
  int bits = 0, offset = 0, i = 12, chunkLen = 4;

  if (s->debug > 2) Snack_WriteLog("    Reading AIFF header\n");
  
  /* buf[] = "FORMxxxxAIFF" */

  while (1) {
    if (strncasecmp("COMM", &buf[i], strlen("COMM")) == 0) {
      chunkLen = GetBELong(buf, i + 4) + 8;
      if (s->firstNRead < i + chunkLen) {
	if (GetHeaderBytes(s, interp, ch, buf, i + chunkLen) != TCL_OK) {
	  return TCL_ERROR;
	}
      }
      s->nchannels = GetBEShort(buf, i + 8);
      bits = GetBEShort(buf, i + 14);
      bits = RNDUPV (bits, 8);
      switch (bits) {
      case 8:
	s->encoding = LIN8;
	s->sampsize = 1;
	break;
      case 16:
	s->encoding = LIN16;
	s->sampsize = 2;
	break;
      case 24:
	s->encoding = LIN24;
	s->sampsize = 3;
	break;
      case 32:
	s->encoding = LIN32;
	s->sampsize = 4;
	break;
      default:
	Tcl_AppendResult(interp, "Unsupported AIFF format", NULL);
	return TCL_ERROR;
      }
      s->samprate = ConvertFloat((unsigned char *)&buf[i+16]);
      if (s->debug > 3) {
	Snack_WriteLogInt("      COMM chunk parsed", chunkLen);
      }
    } else if (strncasecmp("SSND", &buf[i], strlen("SSND")) == 0) {
      chunkLen = 16;
      if (s->firstNRead < i + chunkLen) {
	if (GetHeaderBytes(s, interp, ch, buf, i + 8) != TCL_OK) {
	  return TCL_ERROR;
	}
      }
      s->length = (GetBELong(buf, i + 4) - 8) / (s->sampsize * s->nchannels);
      offset = GetBELong(buf, i + 8);
      i += chunkLen;
      if (s->debug > 3) {
	Snack_WriteLogInt("      SSND chunk parsed", chunkLen);
      }
      break;
    } else {
      if (i > HEADBUF - 4) {
	Tcl_AppendResult(interp, "Missing chunk in AIFF header", NULL);
	return TCL_ERROR;
      } else {
	if (s->debug > 3) {
	  char chunkStr[5];
	
	  strncpy(chunkStr, &buf[i], 4);
	  chunkStr[4] = '\0';
	  Snack_WriteLog(chunkStr);
	  Snack_WriteLog(" chunk skipped\n");
	}
	chunkLen = GetBELong(buf, i + 4) + 8;
      }
    }
    i += chunkLen;
    if (s->firstNRead < i + 8) {
      if (GetHeaderBytes(s, interp, ch, buf, i + 8) != TCL_OK) {
	return TCL_ERROR;
      }
    }
  }
  s->headSize = i + offset;
  SwapIfLE(s);

  return TCL_OK;
}

#define SNACK_AIFF_HEADERSIZE 54

int
PutAiffHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	      int objc, Tcl_Obj *CONST objv[], int len)
{
  char buf[HEADBUF];

  if (s->encoding == LIN8OFFSET || s->encoding == ALAW ||
      s->encoding == MULAW || s->encoding == SNACK_FLOAT) {
    Tcl_AppendResult(interp, "Unsupported encoding format", NULL);
    return -1;
  }

  sprintf(&buf[0], "FORM");
  if (len != -1) {
    PutBELong(buf, 4, len * s->sampsize * s->nchannels + 46);
  } else {
    SwapIfLE(s);
    PutBELong(buf, 4, 0x7FFFFFFF);
  }
  sprintf(&buf[8], "AIFFCOMM");
  PutBELong(buf, 16, 18);
  PutBEShort(buf, 20, (short) s->nchannels);
  PutBELong(buf, 22, s->length);
  PutBEShort(buf, 26, (short) (s->sampsize * 8));
  StoreFloat((unsigned char *) &buf[28], (int32_t) s->samprate);
  sprintf(&buf[38], "SSND");
  if (len != -1) {
    PutBELong(buf, 42, 8 + s->length * s->sampsize * s->nchannels);
  } else {
    PutBELong(buf, 42, 8 + 0x7FFFFFD1);
  }
  PutBELong(buf, 46, 0);
  PutBELong(buf, 50, 0);
  if (ch != NULL) {
    if (Tcl_Write(ch, buf, SNACK_AIFF_HEADERSIZE) == -1) {
      Tcl_AppendResult(interp, "Error while writing header", NULL);
      return -1;
    }
  } else {
    if (useOldObjAPI) {
      Tcl_SetObjLength(obj, SNACK_AIFF_HEADERSIZE);
      memcpy(obj->bytes, buf, SNACK_AIFF_HEADERSIZE);
    } else {
#ifdef TCL_81_API
      unsigned char *p = Tcl_SetByteArrayLength(obj, SNACK_AIFF_HEADERSIZE);
      memcpy(p, buf, SNACK_AIFF_HEADERSIZE);
#endif
    }
  }
  s->inByteOrder = SNACK_BIGENDIAN;
  s->headSize = SNACK_AIFF_HEADERSIZE;
  
  return TCL_OK;
}

static int
GetCslHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	     char *buf)
{
  int tmp1, tmp2, nsamp = 0, nsampfile, i = 12, chunkLen;

  if (s->debug > 2) Snack_WriteLog("    Reading CSL header\n");

  /* buf[] = "FORMDS16xxxxHEDR" */

  while (1) {
    if (strncasecmp("HEDR", &buf[i], strlen("HEDR")) == 0) {
      chunkLen = GetLELong(buf, i + 4) + 8;
      if (s->firstNRead < i + chunkLen) {
	if (GetHeaderBytes(s, interp, ch, buf, i + chunkLen) != TCL_OK) {
	  return TCL_ERROR;
	}
      }
      s->encoding = LIN16;
      s->sampsize   = 2;
      s->nchannels  = 1;
      s->samprate   = GetLELong(buf, i+28);
      tmp1 = GetLEShort(buf, i+36);
      tmp2 = GetLEShort(buf, i+38);
      if (tmp1 != -1 && tmp2 != -1) {
	s->nchannels = 2;
      }
      if (s->debug > 3) {
	Snack_WriteLogInt("      HEDR block parsed", chunkLen);
      }
    } else if (strncasecmp("HDR8", &buf[i], strlen("HDR8")) == 0) {
      chunkLen = GetLELong(buf, i + 4) + 8;
      if (s->firstNRead < i + chunkLen) {
	if (GetHeaderBytes(s, interp, ch, buf, i + chunkLen) != TCL_OK) {
	  return TCL_ERROR;
	}
      }
      s->encoding = LIN16;
      s->sampsize   = 2;
      s->nchannels  = 1;
      s->samprate   = GetLELong(buf, i+28);
      tmp1 = GetLEShort(buf, i+36);
      tmp2 = GetLEShort(buf, i+38);
      if (tmp1 != -1 && tmp2 != -1) {
	s->nchannels = 2;
      }
      if (s->debug > 3) {
	Snack_WriteLogInt("      HDR8 block parsed", chunkLen);
      }
    } else if (strncasecmp("SDA_", &buf[i], strlen("SDA_")) == 0) {
      s->nchannels  = 1;
      nsamp = GetLELong(buf, i + 4) / (s->sampsize * s->nchannels);
      if (s->debug > 3) {
	Snack_WriteLogInt("      SDA_ block parsed", nsamp);
      }
      break;
    } else if (strncasecmp("SD_B", &buf[i], strlen("SD_B")) == 0) {
      s->nchannels  = 1;
      nsamp = GetLELong(buf, i + 4) / (s->sampsize * s->nchannels);
      if (s->debug > 3) {
	Snack_WriteLogInt("      SD_B block parsed", nsamp);
      }
      break;
    } else if (strncasecmp("SDAB", &buf[i], strlen("SDAB")) == 0) {
      nsamp = GetLELong(buf, i + 4) / (s->sampsize * s->nchannels);
      if (s->debug > 3) {
	Snack_WriteLogInt("      SDAB block parsed", nsamp);
      }
      break;
    } else { /* unknown block */
      chunkLen = GetLELong(buf, i + 4) + 8;
      if (chunkLen & 1) chunkLen++;
      if (chunkLen < 0 || chunkLen > HEADBUF) {
	Tcl_AppendResult(interp, "Failed parsing CSL header", NULL);
	return TCL_ERROR;
      }
      if (s->firstNRead < i + chunkLen) {
	if (GetHeaderBytes(s, interp, ch, buf, i + chunkLen) != TCL_OK) {
	  return TCL_ERROR;
	}
      }
      if (s->debug > 3) {
	Snack_WriteLogInt("      Skipping unknown block", chunkLen);
      }
    }

    i += chunkLen;
    if (s->firstNRead < i + 8) {
      if (GetHeaderBytes(s, interp, ch, buf, i + 8) != TCL_OK) {
	return TCL_ERROR;
      }
    }
    if (i >= HEADBUF) {
      Tcl_AppendResult(interp, "Failed parsing CSL header", NULL);
      return TCL_ERROR;
    }
  }
  
  s->headSize = i + 8;
  if (ch != NULL) {
    TCL_SEEK(ch, 0, SEEK_END);
    nsampfile = (TCL_TELL(ch) - s->headSize) / (s->sampsize * s->nchannels);
    if (nsampfile < nsamp || nsamp == 0) {
      nsamp = nsampfile;
    }
  }
  if (obj != NULL) {
    if (useOldObjAPI) {
      nsampfile = (obj->length - s->headSize) / (s->sampsize * s->nchannels);
    } else {
#ifdef TCL_81_API
      int length = 0;
      
      Tcl_GetByteArrayFromObj(obj, &length);
      nsampfile = (length - s->headSize) / (s->sampsize * s->nchannels);
#endif
    }
    if (nsampfile < nsamp || nsamp == 0) {
      nsamp = nsampfile;
    }
  }
  s->length = nsamp;
  SwapIfBE(s);

  return TCL_OK;
}

#define SNACK_CSL_HEADERSIZE 88
#define CSL_DATECOMMAND "clock format [clock seconds] -format {%b %d %T %Y}"

static int
PutCslHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	     int objc, Tcl_Obj *CONST objv[], int len)
{
  char buf[HEADBUF];

  if (s->encoding != LIN16) {
    Tcl_AppendResult(interp, "Unsupported encoding format", NULL);
    return -1;
  }
  
  sprintf(&buf[0], "FORMDS16");
  if (len != -1) {
    PutLELong(buf, 8, len * s->sampsize * s->nchannels + 76);
  } else {
    SwapIfBE(s);
    PutLELong(buf, 8, 0);
  }
  sprintf(&buf[12], "HEDR");
  PutLELong(buf, 16, 32);
  Tcl_GlobalEvalObj(s->interp, Tcl_NewStringObj(CSL_DATECOMMAND, -1));
  sprintf(&buf[20], Tcl_GetStringResult(s->interp));
  
  PutLELong(buf, 40, s->samprate);
  PutLELong(buf, 44, s->length);
  PutLEShort(buf, 48, (short) s->abmax);
  if (s->nchannels == 1) {
    PutLEShort(buf, 50, (short) -1);
  } else {
    PutLEShort(buf, 50, (short) s->abmax);
  }
  
  sprintf(&buf[52], "NOTE");
  PutLELong(buf, 56, 19);
  sprintf(&buf[60], "Created by Snack   ");

  if (s->nchannels == 1) {
    sprintf(&buf[80], "SDA_");
  } else {
    sprintf(&buf[80], "SDAB");
  }
  if (len != -1) {
    PutLELong(buf, 84, len * s->sampsize * s->nchannels);
  } else {
    PutLELong(buf, 84, 0);
  }
  if (ch != NULL) {
    if (Tcl_Write(ch, buf, SNACK_CSL_HEADERSIZE) == -1) {
      Tcl_AppendResult(interp, "Error while writing header", NULL);
      return -1;
    }
  } else {
    if (useOldObjAPI) {
      Tcl_SetObjLength(obj, SNACK_CSL_HEADERSIZE);
      memcpy(obj->bytes, buf, SNACK_CSL_HEADERSIZE);
    } else {
#ifdef TCL_81_API
      unsigned char *p = Tcl_SetByteArrayLength(obj, SNACK_CSL_HEADERSIZE);
      memcpy(p, buf, SNACK_CSL_HEADERSIZE);
#endif
    }
  }
  s->inByteOrder = SNACK_LITTLEENDIAN;
  s->headSize = SNACK_CSL_HEADERSIZE;
  
  return TCL_OK;
}

int
SnackOpenFile(openProc *openProc, Sound *s, Tcl_Interp *interp,
	      Tcl_Channel *ch, char *mode)
{
  int permissions;

  if (strcmp(mode, "r") == 0) {
    permissions = 0;
  } else {
    permissions = 420;
  }
  if (openProc == NULL) {
    if ((*ch = Tcl_OpenFileChannel(interp, s->fcname, mode, permissions))==0) {
      return TCL_ERROR;
    }
    Tcl_SetChannelOption(interp, *ch, "-translation", "binary");
#ifdef TCL_81_API
    Tcl_SetChannelOption(interp, *ch, "-encoding", "binary");
#endif
  } else {
    return((openProc)(s, interp, ch, mode));
  }

  return TCL_OK;
}

int
SnackCloseFile(closeProc *closeProc, Sound *s, Tcl_Interp *interp,
	       Tcl_Channel *ch)
{
  if (closeProc == NULL) {
    Tcl_Close(interp, *ch);
    *ch = NULL;
  } else {
    return((closeProc)(s, interp, ch));
  }

  return TCL_OK;
}

int
SnackSeekFile(seekProc *seekProc, Sound *s, Tcl_Interp *interp,
	      Tcl_Channel ch, int pos)
{
  if (seekProc == NULL) {
    return(TCL_SEEK(ch, s->headSize + pos * s->sampsize * s->nchannels,
		    SEEK_SET));
  } else {
    return((seekProc)(s, interp, ch, pos));
  }
}

char *
LoadSound(Sound *s, Tcl_Interp *interp, Tcl_Obj *obj, int startpos,
	  int endpos)
{
  Tcl_Channel ch = NULL;
  int status = TCL_OK;
  Snack_FileFormat *ff;
  int oldsampfmt = s->encoding;
  
  if (s->debug > 1) Snack_WriteLog("  Enter LoadSound\n");

  if (GetHeader(s, interp, obj) != TCL_OK) {
    return NULL;
  }
  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
    if (strcmp(s->fileType, ff->name) == 0) {
      int pos = 0;
      if (obj == NULL) {
	status = SnackOpenFile(ff->openProc, s, interp, &ch, "r");
      }
      if (status == TCL_OK) {
	if (obj == NULL) {
	  pos = SnackSeekFile(ff->seekProc, s, interp, ch, startpos);
	  if (pos < 0) {
	    SnackCloseFile(ff->closeProc, s, interp, &ch);
	    return NULL;
	  }
	}
      }
      if (status == TCL_OK && pos >= 0) {
	if (s->writeStatus == WRITE && s->encoding != oldsampfmt) {
	  Snack_StopSound(s, NULL);
	}
	status = ReadSound(ff->readProc, s, interp, ch, obj, startpos, endpos);
      }
      if (status == TCL_OK && obj == NULL) {
	status = SnackCloseFile(ff->closeProc, s, interp, &ch);
      }
      if (status == TCL_ERROR) {
	return NULL;
      }
      break;
    }
  }

  if (s->debug > 1) Snack_WriteLog("  Exit LoadSound\n");

  return(s->fileType);
}

int
SaveSound(Sound *s, Tcl_Interp *interp, char *filename, Tcl_Obj *obj,
	  int objc, Tcl_Obj *CONST objv[], int startpos, int len, char *type)
{
  Tcl_Channel ch = NULL;
  Snack_FileFormat *ff;
  char *tmp = s->fcname;

  if (s->debug > 1) Snack_WriteLog("  Enter SaveSound\n");

  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
    if (strcmp(type, ff->name) == 0) {
      if (ff->putHeaderProc != NULL) {
	s->fcname = filename;
	if (filename != NULL) {
	  if (SnackOpenFile(ff->openProc, s, interp, &ch, "w") != TCL_OK) {
	    return TCL_ERROR;
	  }
	}
	if ((ff->putHeaderProc)(s, interp, ch, obj, objc, objv, len)
	    != TCL_OK) {
	  return TCL_ERROR;
	}
	if (WriteSound(ff->writeProc, s, interp, ch, obj, startpos,
		       len) != TCL_OK) {
	  Tcl_AppendResult(interp, "Error while writing", NULL);
	  s->fcname = tmp;
	  return TCL_ERROR;
	}
	s->fcname = tmp;
      } else {
	Tcl_AppendResult(interp, "Unsupported save format", NULL);
	return TCL_ERROR;
      }
      break;
    }
  }

  if (ch != NULL) {
    SnackCloseFile(ff->closeProc, s, interp, &ch);
  }

  if (s->debug > 1) Snack_WriteLog("  Exit SaveSound\n");

  return TCL_OK;
}

int
readCmd(Sound *s, Tcl_Interp *interp, int objc,	Tcl_Obj *CONST objv[])
{
  char *filetype;
  int arg, startpos = 0, endpos = -1;
  static CONST84 char *subOptionStrings[] = {
    "-rate", "-frequency", "-skiphead", "-byteorder", "-channels",
    "-encoding", "-format", "-start", "-end", "-fileformat",
    "-guessproperties", "-progress", NULL
  };
  enum subOptions {
    RATE, FREQUENCY, SKIPHEAD, BYTEORDER, CHANNELS, ENCODING, FORMAT,
    START, END, FILEFORMAT, GUESSPROPS, PROGRESS
  };

  if (s->debug > 0) Snack_WriteLog("Enter readCmd\n");

  if (objc < 3) {
    Tcl_AppendResult(interp, "No file name given", NULL);
    return TCL_ERROR;
  }
  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "read only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }
  if (Tcl_IsSafe(interp)) {
    Tcl_AppendResult(interp, "can not read sound from a file in a safe",
		     " interpreter", (char *) NULL);
    return TCL_ERROR;
  }

  s->guessEncoding = -1;
  s->guessRate = -1;
  s->swap = 0;
  s->forceFormat = 0;
  if (s->cmdPtr != NULL) {
    Tcl_DecrRefCount(s->cmdPtr);
    s->cmdPtr = NULL;
  }

  for (arg = 3; arg < objc; arg+=2) {
    int index;
	
    if (Tcl_GetIndexFromObj(interp, objv[arg], subOptionStrings, "option",
			    0, &index) != TCL_OK) {
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
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &s->samprate) != TCL_OK)
	  return TCL_ERROR;
	s->guessRate = 0;
	break;
      }
    case SKIPHEAD: 
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &s->skipBytes) != TCL_OK)
	  return TCL_ERROR;
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
    case CHANNELS:
      {
	if (GetChannels(interp, objv[arg+1], &s->nchannels) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case ENCODING:
    case FORMAT:
      {
	if (GetEncoding(interp, objv[arg+1], &s->encoding, &s->sampsize) !=
	    TCL_OK)
	  return TCL_ERROR;
	s->guessEncoding = 0;
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
	  if (GetFileFormat(interp, objv[arg+1], &s->fileType) != TCL_OK) {
	    return TCL_ERROR;
	  }
	  s->forceFormat = 1;
	}
	break;
      }
    case GUESSPROPS:
      {
	int guessProps;
	if (Tcl_GetBooleanFromObj(interp, objv[arg+1], &guessProps) != TCL_OK)
	  return TCL_ERROR;
	if (guessProps) {
	  if (s->guessEncoding == -1) s->guessEncoding = 1;
	  if (s->guessRate == -1) s->guessRate = 1;
	}
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
  if (s->guessEncoding == -1) s->guessEncoding = 0;
  if (s->guessRate == -1) s->guessRate = 0;
  if (startpos < 0) startpos = 0;
  if (startpos > endpos && endpos != -1) return TCL_OK;
  if (SetFcname(s, interp, objv[2]) != TCL_OK) {
    return TCL_ERROR;
  }
  if (strlen(s->fcname) == 0) {
    return TCL_OK;
  }
  filetype = LoadSound(s, interp, NULL, startpos, endpos);

  if (filetype == NULL) {
    return TCL_ERROR;
  } else {
    Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
    Snack_ExecCallbacks(s, SNACK_NEW_SOUND);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(filetype, -1));
  }

  if (s->debug > 0) Snack_WriteLog("Exit readCmd\n");

  return TCL_OK;
}

void
Snack_RemoveOptions(int objc, Tcl_Obj *CONST objv[],
		    CONST84 char **subOptionStrings,
		    int *newobjc, Tcl_Obj **newobjv)
{
  int arg, n = 0;
  Tcl_Obj **new = NULL;

  if ((new = (Tcl_Obj **) ckalloc(sizeof(Tcl_Obj *) * objc)) == NULL) {
    return;
  }
  for (arg = 0; arg < objc; arg+=2) {
    int index;
    
    if (Tcl_GetIndexFromObj(NULL, objv[arg], subOptionStrings,
			    NULL, 0, &index) != TCL_OK) {
      new[n++] = Tcl_DuplicateObj(objv[arg]);
      if (n < objc) new[n++] = Tcl_DuplicateObj(objv[arg+1]);
    }
  }
  *newobjc = n;
  *newobjv = (Tcl_Obj *) new;
}

int
writeCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int startpos = 0, endpos = s->length, arg, len, newobjc;
  char *string, *filetype = NULL;
  Tcl_Obj **newobjv = NULL;
  static CONST84 char *subOptionStrings[] = {
    "-start", "-end", "-fileformat", "-progress", "-byteorder", NULL
  };
  enum subOptions {
    START, END, FILEFORMAT, PROGRESS, BYTEORDER
  };

  if (s->debug > 0) { Snack_WriteLog("Enter writeCmd\n"); }

  if (Tcl_IsSafe(interp)) {
    Tcl_AppendResult(interp, "can not write sound to a file in a safe",
		     " interpreter", (char *) NULL);
    return TCL_ERROR;
  }

  s->inByteOrder = SNACK_NATIVE;
  if (s->cmdPtr != NULL) {
    Tcl_DecrRefCount(s->cmdPtr);
    s->cmdPtr = NULL;
  }

  for (arg = 3; arg < objc; arg+=2) {
    int index;
	
    if (Tcl_GetIndexFromObj(NULL, objv[arg], subOptionStrings,
			    "option", 0, &index) != TCL_OK) {
      continue;
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
    case FILEFORMAT:
      {
	if (GetFileFormat(interp, objv[arg+1], &filetype) != TCL_OK)
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
    case BYTEORDER:
      {
	int length;
	char *str = Tcl_GetStringFromObj(objv[arg+1], &length);

	if (strncasecmp(str, "littleEndian", length) == 0) {
  	  s->inByteOrder = SNACK_LITTLEENDIAN;
	} else if (strncasecmp(str, "bigEndian", length) == 0) {
	  s->inByteOrder = SNACK_BIGENDIAN;
	} else {
	  Tcl_AppendResult(interp, "-byteorder option should be bigEndian",
			   " or littleEndian", NULL);
	  return TCL_ERROR;
	}
	break;
      }
    }
  }
  len = s->length;
  if (endpos >= len) endpos = len;
  if (endpos < 0)    endpos = len;
  if (endpos > startpos) len -= (len - endpos);
  if (startpos > endpos) return TCL_OK;
  if (startpos > 0) len -= startpos; else startpos = 0;

  Snack_RemoveOptions(objc-3, objv+3, subOptionStrings, &newobjc,
		      (Tcl_Obj **) &newobjv);

  if (objc < 3) {
    Tcl_AppendResult(interp, "No file name given", NULL);
    return TCL_ERROR;
  }
  string = Tcl_GetStringFromObj(objv[2], NULL);
  if (filetype == NULL) {
    filetype = NameGuessFileType(string);
  }
  if (strlen(string) == 0) {
    return TCL_OK;
  }
  if (s->storeType != SOUND_IN_MEMORY) {
    if (s->linkInfo.linkCh == NULL) {
      OpenLinkedFile(s, &s->linkInfo);
    }
  }
  if (SaveSound(s, interp, string, NULL, newobjc, (Tcl_Obj **CONST) newobjv,
		startpos, len, filetype) == TCL_ERROR) {
    return TCL_ERROR;
  }


  for (arg = 0; arg <newobjc; arg++) {
    Tcl_DecrRefCount(newobjv[arg]);
  }
  ckfree((char *)newobjv);

  if (s->debug > 0) { Snack_WriteLog("Exit writeCmd\n"); }

  return TCL_OK;
} /* writeCmd */

int
dataCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  if (s->storeType != SOUND_IN_MEMORY) {
    Tcl_AppendResult(interp, "data only works with in-memory sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }
  
  if ((objc % 2) == 0) { /* sound -> variable */
    Tcl_Obj *new = Tcl_NewObj();
    char *filetype = s->fileType;
    int arg, len, startpos = 0, endpos = s->length;
    static CONST84 char *subOptionStrings[] = {
      "-fileformat", "-start", "-end", "-byteorder",
      NULL
    };
    enum subOptions {
      FILEFORMAT, START, END, BYTEORDER
    };

    s->swap = 0;

    for (arg = 2; arg < objc; arg += 2) {
      int index;
      char *str;
      
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
      case FILEFORMAT:
	{
	  if (GetFileFormat(interp, objv[arg+1], &filetype) != TCL_OK)
	    return TCL_ERROR;
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
      case BYTEORDER:
	{
	  str = Tcl_GetStringFromObj(objv[arg+1], &len);
	  if (strncasecmp(str, "littleEndian", len) == 0) {
	    SwapIfBE(s);
	  } else if (strncasecmp(str, "bigEndian", len) == 0) {
	    SwapIfLE(s);
	  } else {
	    Tcl_AppendResult(interp,
	       "-byteorder option should be bigEndian or littleEndian", NULL);
	    return TCL_ERROR;
	  }
	  break;
	}
      }
    }
    
    len = s->length;
    if (endpos >= len) endpos = len;
    if (endpos < 0)    endpos = len;
    if (endpos > startpos) len -= (len - endpos);
    if (startpos > endpos) return TCL_OK;
    if (startpos > 0) len -= startpos; else startpos = 0;

    if (SaveSound(s, interp, NULL, new, objc-2, objv+2, startpos, len,filetype)
	== TCL_ERROR) {
      return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, new);
  } else { /* variable -> sound */
    int arg, startpos = 0, endpos = -1;
    char *filetype;
    static CONST84 char *subOptionStrings[] = {
      "-rate", "-frequency", "-skiphead", "-byteorder",
      "-channels", "-encoding", "-format", "-start", "-end", "-fileformat",
      "-guessproperties", NULL
    };
    enum subOptions {
      RATE, FREQUENCY, SKIPHEAD, BYTEORDER, CHANNELS, ENCODING, FORMAT,
      START, END, FILEFORMAT, GUESSPROPS
    };

    s->guessEncoding = -1;
    s->guessRate = -1;
    s->swap = 0;
    s->forceFormat = 0;

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
	  if (Tcl_GetIntFromObj(interp, objv[arg+1], &s->samprate) != TCL_OK)
	    return TCL_ERROR;
	  s->guessRate = 0;
	  break;
	}
      case SKIPHEAD: 
	{
	  if (Tcl_GetIntFromObj(interp, objv[arg+1], &s->skipBytes) != TCL_OK) {
	    return TCL_ERROR;
	  }
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
      case CHANNELS:
	{
	  if (GetChannels(interp, objv[arg+1], &s->nchannels) != TCL_OK)
	    return TCL_ERROR;
	  break;
	}
      case ENCODING:
      case FORMAT:
	{
	  if (GetEncoding(interp, objv[arg+1], &s->encoding, &s->sampsize)
	      != TCL_OK)
	    return TCL_ERROR;
	  s->guessEncoding = 0;
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
	    if (GetFileFormat(interp, objv[arg+1], &s->fileType) != TCL_OK)
	      return TCL_ERROR;
	    s->forceFormat = 1;
	    break;
	  }
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
      }
    }
    if (s->guessEncoding == -1) s->guessEncoding = 0;
    if (s->guessRate == -1) s->guessRate = 0;
    if (startpos < 0) startpos = 0;
    if (startpos > endpos && endpos != -1) return TCL_OK;
    filetype = LoadSound(s, interp, objv[2], startpos, endpos);
    if (filetype == NULL) {
      return TCL_ERROR;
    } else {
      Snack_UpdateExtremes(s, 0, s->length, SNACK_NEW_SOUND);
      Snack_ExecCallbacks(s, SNACK_NEW_SOUND);
      Tcl_SetObjResult(interp, Tcl_NewStringObj(filetype, -1));
    }
  }

  return TCL_OK;
} /* dataCmd */

int
GetHeader(Sound *s, Tcl_Interp *interp, Tcl_Obj *obj)
{
  Snack_FileFormat *ff;
  Tcl_Channel ch = NULL;
  int status = TCL_OK, openedOk = 0;
  int buflen = max(HEADBUF, CHANNEL_HEADER_BUFFER), len = 0;

  if (s->guessEncoding) {
    s->swap = 0;
  }
  if (s->tmpbuf != NULL) {
    ckfree((char *)s->tmpbuf);
  }
  if ((s->tmpbuf = (short *) ckalloc(buflen)) == NULL) {
    Tcl_AppendResult(interp, "Could not allocate buffer!", NULL);
    return TCL_ERROR;
  }
  if (obj == NULL) {
    ch = Tcl_OpenFileChannel(interp, s->fcname, "r", 0);
    if (ch != NULL) {
      Tcl_SetChannelOption(interp, ch, "-translation", "binary");
#ifdef TCL_81_API
      Tcl_SetChannelOption(interp, ch, "-encoding", "binary");
#endif
      if ((len = Tcl_Read(ch, (char *)s->tmpbuf, buflen)) > 0) {
	Tcl_Close(interp, ch);
	ch = NULL;
      }
    } else {
      ckfree((char *)s->tmpbuf);
      s->tmpbuf = NULL;
      return TCL_ERROR;
    }
  } else {
    unsigned char *ptr = NULL;

    if (useOldObjAPI) {
      len = min(obj->length, buflen);
      memcpy((char *)s->tmpbuf, obj->bytes, len);
    } else {
#ifdef TCL_81_API
      int length = 0;
      
      ptr = Tcl_GetByteArrayFromObj(obj, &length);
      len = min(length, buflen);
      memcpy((char *)s->tmpbuf, ptr, len);
#endif
    }
  }
  if (s->forceFormat == 0) {
    s->fileType = GuessFileType((char *)s->tmpbuf, len, 1);
  }
  s->firstNRead = len;

  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
    if (strcmp(s->fileType, ff->name) == 0) {
      if (obj == NULL) {
	status = SnackOpenFile(ff->openProc, s, interp, &ch, "r");
	if (status == TCL_OK) openedOk = 1;
      }
      if (status == TCL_OK) {
	status = (ff->getHeaderProc)(s, interp, ch, obj, (char *)s->tmpbuf);
      }
      if (strcmp(s->fileType, RAW_STRING) == 0 && s->guessEncoding) {
	GuessEncoding(s, (unsigned char *)s->tmpbuf, len);
      }
      if (obj == NULL && openedOk == 1) {
	status = SnackCloseFile(ff->closeProc, s, interp, &ch);
      }
      ckfree((char *)s->tmpbuf);
      s->tmpbuf = NULL;

      return(status);
    }
  }
  ckfree((char *)s->tmpbuf);
  s->tmpbuf = NULL;

  return TCL_OK;
}

int
PutHeader(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[],
	  int length)
{
  Snack_FileFormat *ff;

  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
    if (strcmp(s->fileType, ff->name) == 0) {
      if (ff->putHeaderProc != NULL) {
	return (ff->putHeaderProc)(s, interp, s->rwchan, NULL, objc, objv,
				   length);
      }
      break;
    }
  }
  return 0;
}

int
GetFileFormat(Tcl_Interp *interp, Tcl_Obj *obj, char **filetype)
{
  int length;
  char *str = Tcl_GetStringFromObj(obj, &length);
  Snack_FileFormat *ff;

  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
    if (strcasecmp(str, ff->name) == 0) {
      *filetype = ff->name;
      return TCL_OK;
    }
  }

  if (strcasecmp(str, RAW_STRING) == 0) {
    *filetype = RAW_STRING;
    return TCL_OK;
  }

  Tcl_AppendResult(interp, "Unknown file format", NULL);

  return TCL_ERROR;
}

void
Snack_CreateFileFormat(Snack_FileFormat *typePtr)
{
  Snack_FileFormat *typePtr2, *prevPtr;

  /*
   * If there's already a filter type with the given name, remove it.
   */
  
  for (typePtr2 = snackFileFormats, prevPtr = NULL; typePtr2 != NULL;
       prevPtr = typePtr2, typePtr2 = typePtr2->nextPtr) {
    if (strcmp(typePtr2->name, typePtr->name) == 0) {
      if (prevPtr == NULL) {
	snackFileFormats = typePtr2->nextPtr;
      } else {
	prevPtr->nextPtr = typePtr2->nextPtr;
      }
      break;
    }
  }
  typePtr->nextPtr = snackFileFormats;
  snackFileFormats = typePtr;
}

/* Deprecated, use Snack_CreateFileFormat() instead */

int
Snack_AddFileFormat(char *name, guessFileTypeProc *guessProc,
		    getHeaderProc *getHeadProc, extensionFileTypeProc *extProc,
		    putHeaderProc *putHeadProc, openProc *openProc,
		    closeProc *closeProc, readSamplesProc *readProc,
		    writeSamplesProc *writeProc, seekProc *seekProc)
{
  Snack_FileFormat *ff = (Snack_FileFormat *)ckalloc(sizeof(Snack_FileFormat));

  if (ff == NULL) {
    return TCL_ERROR;
  }
  ff->name          = name;
  ff->guessProc     = guessProc;
  ff->getHeaderProc = getHeadProc;
  ff->extProc       = extProc;
  ff->putHeaderProc = putHeadProc;
  ff->openProc      = openProc;
  ff->closeProc     = closeProc;
  ff->readProc      = readProc;
  ff->writeProc     = writeProc;
  ff->seekProc      = seekProc;
  ff->nextPtr       = snackFileFormats;
  snackFileFormats  = ff;

  return TCL_OK;
}

Snack_FileFormat snackRawFormat = {
  RAW_STRING,
  GuessRawFile,
  GetRawHeader,
  NULL,
  PutRawHeader,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  (Snack_FileFormat *) NULL
};

Snack_FileFormat snackMp3Format = {
  MP3_STRING,
  GuessMP3File,
  GetMP3Header,
  ExtMP3File,
  NULL,
  OpenMP3File,
  CloseMP3File,
  ReadMP3Samples,
  NULL,
  SeekMP3File,
  FreeMP3Header,
  ConfigMP3Header,
  (Snack_FileFormat *) NULL
};

Snack_FileFormat snackSmpFormat = {
  SMP_STRING,
  GuessSmpFile,
  GetSmpHeader,
  ExtSmpFile,
  PutSmpHeader,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  (Snack_FileFormat *) NULL
};

Snack_FileFormat snackCslFormat = {
  CSL_STRING,
  GuessCslFile,
  GetCslHeader,
  ExtCslFile,
  PutCslHeader,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  (Snack_FileFormat *) NULL
};

Snack_FileFormat snackSdFormat = {
  SD_STRING,
  GuessSdFile,
  GetSdHeader,
  ExtSdFile,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  FreeSdHeader,
  ConfigSdHeader,
  (Snack_FileFormat *) NULL
};

Snack_FileFormat snackAiffFormat = {
  AIFF_STRING,
  GuessAiffFile,
  GetAiffHeader,
  ExtAiffFile,
  PutAiffHeader,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  (Snack_FileFormat *) NULL
};

Snack_FileFormat snackAuFormat = {
  AU_STRING,
  GuessAuFile,
  GetAuHeader,
  ExtAuFile,
  PutAuHeader,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  (Snack_FileFormat *) NULL
};

Snack_FileFormat snackWavFormat = {
  WAV_STRING,
  GuessWavFile,
  GetWavHeader,
  ExtWavFile,
  PutWavHeader,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  (Snack_FileFormat *) NULL
};

void
SnackDefineFileFormats(Tcl_Interp *interp)
/*
{
  snackFileFormats        = &snackWavFormat;
  snackWavFormat.nextPtr  = &snackAiffFormat;
  snackAiffFormat.nextPtr = &snackAuFormat;
  snackAuFormat.nextPtr   = &snackSmpFormat;
  snackSmpFormat.nextPtr  = &snackCslFormat;
  snackCslFormat.nextPtr  = &snackSdFormat;
  snackSdFormat.nextPtr   = &snackMp3Format;
  snackMp3Format.nextPtr  = &snackRawFormat;
  snackRawFormat.nextPtr  = NULL;
}
*/
{
  snackFileFormats        = &snackWavFormat;
  snackWavFormat.nextPtr  = &snackMp3Format;
  snackMp3Format.nextPtr  = &snackAiffFormat;
  snackAiffFormat.nextPtr = &snackAuFormat;
  snackAuFormat.nextPtr   = &snackSmpFormat;
  snackSmpFormat.nextPtr  = &snackCslFormat;
  snackCslFormat.nextPtr  = &snackSdFormat;
  snackSdFormat.nextPtr   = &snackRawFormat;
  snackRawFormat.nextPtr  = NULL;
}

#define BACKLOGSAMPS 1

int
OpenLinkedFile(Sound *s, SnackLinkedFileInfo *infoPtr)
{
  Snack_FileFormat *ff;

  infoPtr->sound = s;

  if (strlen(s->fcname) == 0) {
    return TCL_OK;
  }
  if (s->itemRefCnt && s->readStatus == READ) {
    return TCL_OK;
  }

  infoPtr->buffer = (float *) ckalloc(ITEMBUFFERSIZE);
  infoPtr->filePos = -1;
  infoPtr->validSamples = 0;
  infoPtr->eof = 0;

  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
    if (strcmp(s->fileType, ff->name) == 0) {
      if (SnackOpenFile(ff->openProc, s, s->interp, &infoPtr->linkCh, "r")
	  != TCL_OK) {
	return TCL_ERROR;
      }
      return TCL_OK;
    }
  }
  return TCL_ERROR;
}

void
CloseLinkedFile(SnackLinkedFileInfo *infoPtr)
{
  Sound *s = infoPtr->sound;
  Snack_FileFormat *ff;

  if (strlen(s->fcname) == 0) {
    return;
  }
  if (s->itemRefCnt && s->readStatus == READ) {
    return;
  }

  ckfree((char *) infoPtr->buffer);

  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
    if (strcmp(s->fileType, ff->name) == 0) {
      SnackCloseFile(ff->closeProc, s, s->interp, &infoPtr->linkCh);
      return;
    }
  }
}

float junkBuffer[PBSIZE];

float
GetSample(SnackLinkedFileInfo *infoPtr, int index)
{
  Sound *s = infoPtr->sound;
  Snack_FileFormat *ff;
  int nRead = 0, size = ITEMBUFFERSIZE / sizeof(float), i;

  if (s->itemRefCnt && s->readStatus == READ) {
    return FSAMPLE(s, index);
  }

  if (index < infoPtr->filePos + ITEMBUFFERSIZE / (int) sizeof(float)
      && index >= infoPtr->filePos && infoPtr->filePos != -1) {
    if (index < infoPtr->filePos + infoPtr->validSamples) {
      return(infoPtr->buffer[index-infoPtr->filePos]);
    } else {
      infoPtr->eof = 1;
      return(0.0f);
    }
  } else {
    int pos = 0, doSeek = 1;

    if (index == infoPtr->filePos + ITEMBUFFERSIZE / (int) sizeof(float)) {
      doSeek = 0;
    }

    /* Keep BACKLOGSAMPS old samples in the buffer */

    if (index > BACKLOGSAMPS * s->nchannels) {
      index -= BACKLOGSAMPS * s->nchannels;
    }

    for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
      if (strcmp(s->fileType, ff->name) == 0) {
	char *b = &((char *)infoPtr->buffer)[ITEMBUFFERSIZE  - size * s->sampsize];

	if (doSeek || ff->readProc == NULL) {
	  SnackSeekFile(ff->seekProc, s, s->interp, infoPtr->linkCh, index /
			s->nchannels);
	}
	if (s->nchannels > 1 && index % s->nchannels > 0) {
	  pos   = index % s->nchannels + s->nchannels;
	  index = s->nchannels * (int)(index / s->nchannels);
	} else {
	  if (index > 0) {
	    pos = s->nchannels;
	  }
	}

	if (ff->readProc == NULL) {
	  nRead = Tcl_Read(infoPtr->linkCh, b, size * s->sampsize);
	  infoPtr->validSamples = nRead / s->sampsize;
	} else {
	  int tries=10,maxt=tries;
	  /* TFW: Workaround for streaming issues:
	   * Make sure we get something from the channel if possible
	   * on some (e.g. ogg) streams, we sometime get a -1 back for length
	   * typically on the second retry we get it right.
           */
	  for (;tries>0;tries--) {
	    nRead = (ff->readProc)(s, s->interp, infoPtr->linkCh, NULL,
				   junkBuffer, size);
	    if (nRead > 0) break;
	  }
	  if (s->debug > 1 && tries < maxt) {
	    Snack_WriteLogInt("  Read Tries", maxt-tries);
	    Snack_WriteLogInt("  Read Samples", nRead);
	  }
	  infoPtr->validSamples = nRead;
	  memcpy(infoPtr->buffer, junkBuffer, nRead * sizeof(float));
	}

	if (ff->readProc == NULL) { /* unpack block */
	  unsigned char *q = (unsigned char *) b;
	  char *sc = (char *) b;
	  short *r = (short *) b;
	  int   *is = (int *) b;
	  float *fs = (float *) b;
	  float *f = infoPtr->buffer;
	  
	  for (i = 0; i < size; i++) {
	    switch (s->encoding) {
	    case LIN16:
	      if (s->swap) *r = Snack_SwapShort(*r);
	      *f++ = (float) *r++;
	      break;
	    case LIN32:
	      if (s->swap) *is = Snack_SwapLong(*is);
	      *f++ = (float) *is++;
	      break;
	    case SNACK_FLOAT:
	      if (s->swap) *fs = (float) Snack_SwapLong((int)*fs);
	      *f++  = (float) *fs++;
	      break;
	    case ALAW:
	      *f++ = (float) Snack_Alaw2Lin(*q++);
	      break;
	    case MULAW:
	      *f++ = (float) Snack_Mulaw2Lin(*q++);
	      break;
	    case LIN8:
	      *f++ = (float) *sc++;
	      break;
	    case LIN8OFFSET:
	      *f++ = (float) *q++;
	      break;
	    case LIN24:
	    case LIN24PACKED:
	      {
		int ee;
		if (s->swap) {
		  if (littleEndian) {
		    ee = 0;
		  } else {
		    ee = 1;
		  }
		} else {
		  if (littleEndian) {
		    ee = 1;
		  } else {
		    ee = 0;
		  }		
		}
		if (ee) {
		  int t = *q++;
		  t |= *q++ << 8;
		  t |= *q++ << 16;
		  if (t & 0x00800000) {
		    t |= (unsigned int) 0xff000000;
		  }
		  *f++ = (float) t;
		} else {
		  int t = *q++ << 16;
		  t |= *q++ << 8;
		  t |= *q++;
		  if (t & 0x00800000) {
		    t |= (unsigned int) 0xff000000;
		  }
		  *f++ = (float) t;
		}
		break;
	      }
	    }
	  }
	}
	break;
      }
    }
    infoPtr->filePos = index;

    return(infoPtr->buffer[pos]);
  }
}

Snack_FileFormat *
Snack_GetFileFormats()
{
  return snackFileFormats;
}
