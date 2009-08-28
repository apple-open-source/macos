/* 
 * Copyright (C) 1997-2004 Kare Sjolander <kare@speech.kth.se>
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

#if defined(MAC)
#  define FIXED_READ_CHUNK 1
#endif /* MAC */

int rop = IDLE;
int numRec = 0;
int wop = IDLE;
static ADesc adi;
static ADesc ado;
static int globalRate = 16000;
static int globalOutWidth = 0;
static int globalStreamWidth = 0;
static long globalNWritten = 0;
static int globalNFlowThrough = 0;

short shortBuffer[PBSIZE];
float floatBuffer[PBSIZE];
float fff[PBSIZE];
static Tcl_TimerToken ptoken;
static Tcl_TimerToken rtoken;

#define FPS 32
#define RECGRAIN 10
#define BUFSCROLLSIZE 25000
struct jkQueuedSound *rsoundQueue = NULL;

extern int debugLevel;
extern char *snackDumpFile;
static Tcl_Channel snackDumpCh = NULL;

extern struct Snack_FileFormat *snackFileFormats;

static void
RecCallback(ClientData clientData)
{
  jkQueuedSound *p;
  int nRead = 0, i, sampsleft = SnackAudioReadable(&adi);
  int size = globalRate / FPS;
  Snack_FileFormat *ff;

  if (debugLevel > 1) Snack_WriteLogInt("  Enter RecCallback", sampsleft);

  if (sampsleft > size * 2) size *= 2;
  if (sampsleft > size * 2) size = sampsleft;
  if (sampsleft < size) size = sampsleft;
  if (size > PBSIZE / globalStreamWidth) {
    size = PBSIZE / globalStreamWidth;
  }

#ifdef FIXED_READ_CHUNK
  size = globalRate / 16;
#endif

  if (adi.bytesPerSample == 4) {
    nRead = SnackAudioRead(&adi, floatBuffer, size);
  } else {
    nRead = SnackAudioRead(&adi, shortBuffer, size);
  }

  for (p = rsoundQueue; p != NULL; p = p->next) {
    Sound *s = p->sound;

    if (s->debug > 2) Snack_WriteLogInt("    readstatus? ", s->readStatus);
    if (s->readStatus == IDLE) continue;
    if (p->status) continue;
    if (s->rwchan) { /* sound from file or channel */

      if ((s->length + nRead - s->validStart) * s->nchannels > FBLKSIZE) {
	s->validStart += (BUFSCROLLSIZE / s->nchannels);
	memmove(&s->blocks[0][0], &s->blocks[0][BUFSCROLLSIZE],
		(FBLKSIZE-BUFSCROLLSIZE) * sizeof(float));
      }

      if (adi.bytesPerSample == 4) {
	for (i = 0; i < nRead * s->nchannels; i++) {
	  FSAMPLE(s, (s->length - s->validStart) * s->nchannels + i) =
	    (float) (((int*)floatBuffer)[i]/256);
	}
      } else {
	for (i = 0; i < nRead * s->nchannels; i++) {
	  FSAMPLE(s, (s->length - s->validStart) * s->nchannels + i) =
	    (float) shortBuffer[i];
	}
      }

      for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
	if (strcmp(s->fileType, ff->name) == 0) {
	  WriteSound(ff->writeProc, s, s->interp, s->rwchan, NULL,
		     s->length - s->validStart, nRead);
	}
      }
      
      Tcl_Flush(s->rwchan);
      
    } else { /* sound in memory */
      if (s->length > s->maxlength - max(sampsleft, adi.bytesPerSample * nRead)) {
	if (Snack_ResizeSoundStorage(s, s->length + max(sampsleft, adi.bytesPerSample * nRead)) != TCL_OK) {
	  return;
	}
      }

      if (s->debug > 2) Snack_WriteLogInt("    adding frames", nRead);
      if (adi.bytesPerSample == 4) {
	for (i = 0; i < nRead * s->nchannels; i++) {
	  FSAMPLE(s, s->length * s->nchannels + i) = (float) (((int*)floatBuffer)[i]/256);
	}
      } else {
	for (i = 0; i < nRead * s->nchannels; i++) {
	  FSAMPLE(s, s->length * s->nchannels + i) = (float) shortBuffer[i];
	}
      }
    }
    if (nRead > 0) {
      if (s->storeType == SOUND_IN_MEMORY) {
	Snack_UpdateExtremes(s, s->length, s->length + nRead, SNACK_MORE_SOUND);
      }
      s->length += nRead;
      Snack_ExecCallbacks(s, SNACK_MORE_SOUND);
    }
  }
  rtoken = Tcl_CreateTimerHandler(RECGRAIN, (Tcl_TimerProc *) RecCallback,
				  (int *) NULL);

  if (debugLevel > 1) Snack_WriteLogInt("  Exit RecCallback", nRead);
}

static void
ExecSoundCmd(Sound *s, Tcl_Obj *cmdPtr)
{
  Tcl_Interp *interp = s->interp;
  
  if (cmdPtr != NULL) {
    Tcl_Preserve((ClientData) interp);
    if (Tcl_GlobalEvalObj(interp, cmdPtr) != TCL_OK) {
      Tcl_AddErrorInfo(interp, "\n    (\"command\" script)");
      Tcl_BackgroundError(interp);
    }
    Tcl_Release((ClientData) interp);
  }
}

struct jkQueuedSound *soundQueue = NULL;
static int corr = 0;
static Sound *sCurr = NULL;

static void
CleanPlayQueue()
{
  jkQueuedSound *p, *q;

  if (soundQueue == NULL) return;

  p = soundQueue;
  do {
    q = p->next;
    p->sound->writeStatus = IDLE;
    if (p->cmdPtr != NULL) {
      Tcl_DecrRefCount(p->cmdPtr);
      p->cmdPtr = NULL;
    }
    if (p->sound->destroy) {
      Snack_DeleteSound(p->sound);
    }
    if (p->filterName != NULL) {
      ckfree((char *)p->filterName);
    }
    ckfree((char *)p);
    p = q;
  } while (p != NULL);

  soundQueue = NULL;
}

static void
CleanRecordQueue()
{
  jkQueuedSound *p, *q;

  if (rsoundQueue == NULL) return;

  p = rsoundQueue;
  do {
    q = p->next;
    ckfree((char *)p);
    p = q;
  } while (p != NULL);

  rsoundQueue = NULL;
}
/*
static void
DumpQueue(char *msg, struct jkQueuedSound *q)
{
  jkQueuedSound *p;

  printf("%s\t", msg);

  for (p = q; p != NULL; p = p->next) {
    printf("%s\t", p->name);
  }
  printf("\n\t");

  for (p = q; p != NULL; p = p->next) {
    if (p->status == SNACK_QS_QUEUED) 
      printf("Q\t");
    else if (p->status == SNACK_QS_PAUSED)
      printf("P\t");
    else
      printf("D\t");
  }
  printf("\n");
}
*/

extern Tcl_HashTable *filterHashTable;
extern float globalScaling;

static int
AssembleSoundChunk(int inSize)
{
  int chunkWritten = 1, writeSize = 0, size = inSize, i, j;
  int longestChunk = 0, startPos, endPos, totLen;
  long nWritten;
  int emptyQueue = 1;
  jkQueuedSound *p;
  Sound *s;
  Tcl_HashEntry *hPtr;
  Snack_Filter f;

  if (debugLevel > 2) Snack_WriteLogInt("    Enter AssembleSoundChunk", size);

  for (i = 0; i < inSize * globalOutWidth; i++) {
    floatBuffer[i] = 0.0f;
    fff[i] = 0.0f;
  }

  for (p = soundQueue; p != NULL; p = p->next) {
    int first = 0, inFrames, outFrames, nPrepared = 0, inputExhausted = 0;
    float frac = 1.0f;

    if (p->status == SNACK_QS_PAUSED || p->status == SNACK_QS_DONE) continue;
    emptyQueue = 0;
    if (p->startTime > globalNWritten + size) continue;

    s = p->sound;
    startPos = p->startPos;
    endPos = p->endPos;
    totLen = endPos - startPos + 1;
    nWritten = p->nWritten;
    frac = (float) s->samprate / (float) globalRate;
    if (s->debug > 1) {
      Snack_WriteLogInt("    asc len", s->length);
      Snack_WriteLogInt("        end", p->endPos);
      Snack_WriteLogInt("        wrt", nWritten);
    }

    if (s->storeType == SOUND_IN_MEMORY) { /* sound in memory */

      if (nWritten < totLen && (startPos + nWritten < s->length)) {
	writeSize = size;
	if (writeSize > (totLen - nWritten) / frac) {
	  writeSize = (int) ((totLen - nWritten) / frac);
	}
	if (p->nWritten == 0 && p->startTime > 0) {
	  first = max(p->startTime - globalNWritten, 0);
	  if (writeSize > first) {
	    writeSize -= first;
	  }
	}
	if (s->debug > 1) Snack_WriteLogInt("      first ", first);
	longestChunk = max(longestChunk, writeSize);
	for (i = 0; i < first * s->nchannels; i++) fff[i] = 0.0f;
	if (s->samprate == globalRate) {
	  for (i = first * s->nchannels, j = (startPos + nWritten) * 
		 s->nchannels; i < writeSize * s->nchannels;
	       i++, j++) {
	    fff[i] = FSAMPLE(s, j);
	  }
	  nPrepared = writeSize;	
	} else {
	  int c, ij, pos;
	  float smp1 = 0.0, smp2, f, dj;

	  for (c = 0; c < s->nchannels; c++) {
	    for (i = first * s->nchannels, j = 0;
		 i < writeSize * s->nchannels; i++, j++) {
	      dj = frac * i; 
	      ij = (int) dj;
	      f = dj - ij;
	      pos = (startPos + nWritten + ij) * s->nchannels + c;
	      if (pos >= (s->length - 1) * s->nchannels) break;
	      smp1 = FSAMPLE(s, pos);
	      smp2 = FSAMPLE(s, pos + s->nchannels);
	      fff[i * s->nchannels + c] = smp1 * (1.0f - f) + smp2 * f;
	    }
	  }
	  nPrepared = (int) (frac * writeSize + 0.5);
	} /* s->samprate != globalRate */
	if (totLen <= nWritten + nPrepared + 1) inputExhausted = 1;
      } else { /* nWritten < totLen ... */
	if (s->readStatus != READ) {
	  inputExhausted = 1;
	}
	writeSize = 0;
      } /* nWritten < totLen ... */
    } else { /* sound in file or channel */
      if ((nWritten < totLen || endPos == -1 || totLen == 0) &&
	  s->linkInfo.eof == 0) {
	writeSize = size;
	if (s->length > 0) {
	  if (writeSize > (s->length - startPos - nWritten) / frac) {
	    writeSize = (int) ((s->length - startPos - nWritten) / frac);
	  }
	}
	if (endPos != -1) {
	  if (totLen != 0 && writeSize > (totLen - nWritten) / frac) {
	    writeSize = (int) ((totLen - nWritten) / frac);
	  }
	}
	if (nWritten == 0 && p->startTime > 0) {
	  first = max(p->startTime - globalNWritten, 0);
	  writeSize -= first;
	}
	for (i = 0; i < first * s->nchannels; i++) fff[i] = 0.0f;
	if (s->samprate == globalRate) {
	  for (i = first * s->nchannels, j = (startPos + nWritten) *
		 s->nchannels; i < writeSize * s->nchannels; i++, j++) {
	    fff[i] = GetSample(&s->linkInfo, j);
	    if (s->linkInfo.eof) {
	      inputExhausted = 1;
	      writeSize = i / s->nchannels;
	      break;
	    }
	  }
	  nPrepared = writeSize;
	} else {
	  int c, ij, pos;
	  float smp1 = 0.0, smp2, f, dj;
	  
	  for (c = 0; c < s->nchannels; c++) {
	    for (i = first * s->nchannels, j = 0;
		 i < writeSize * s->nchannels; i++, j++) {
	      dj = frac * i; 
	      ij = (int) dj;
	      f = dj - ij;
	      pos = (startPos + nWritten + ij) * s->nchannels + c;
	      if (pos >= (s->length - 1) * s->nchannels) break;
	      smp1 = GetSample(&s->linkInfo, pos);
	      smp2 = GetSample(&s->linkInfo, pos + s->nchannels);
	      fff[i * s->nchannels + c] = smp1 * (1.0f - f) + smp2 * f;
	      if (s->linkInfo.eof) {
		inputExhausted = 1;
		writeSize = i / s->nchannels;
		break;
	      }
	    }
	  }
	  nPrepared = (int) (frac * writeSize + 0.5);
	} /* s->samprate != globalRate */
	longestChunk = max(longestChunk, writeSize);
      } else { /* p->nWritten == totLen or EOF */
	if (s->readStatus != READ) {
	  if (s->storeType == SOUND_IN_FILE) {
	    if (s->linkInfo.linkCh != NULL) {
	      CloseLinkedFile(&s->linkInfo);
              if (s->debug > 1)
		Snack_WriteLogInt("    Closing File, len= ", s->length);
	      s->linkInfo.linkCh = NULL;
	    }
	  } else {
	    s->linkInfo.linkCh = NULL;
	    if (s->linkInfo.buffer != NULL) {
	      ckfree((char *) s->linkInfo.buffer);
	      s->linkInfo.buffer = NULL;
	    }
	  }
	  inputExhausted = 1;
	}
      } /* nWritten < totLen ... */
      /*if (totLen == nWritten + nPrepared) inputExhausted = 1;*/
      if (totLen > 0)
	if (totLen <= nWritten + nPrepared + 1) inputExhausted = 1;
    } /* s->storeType */

    if (s->nchannels != globalStreamWidth) {
      if (s->nchannels < globalStreamWidth) {
	for (i = writeSize - 1; i >= first; i--) {
	  int c;
	  
	  for (c = 0; c < s->nchannels; c++) {
	    fff[i * globalStreamWidth + c] = fff[i * s->nchannels + c];
	  }
	  for (;c < globalStreamWidth; c++) {
	    fff[i * globalStreamWidth + c] = fff[i * s->nchannels];
	  }
	}
      } else {
	for (i = 0; i < writeSize; i++) {
	  int c;
	  
	  for (c = 0; c < s->nchannels; c++) {
	    fff[i * globalStreamWidth + c] = fff[i * s->nchannels + c];
	  }
	}
      }
    }

    inFrames = writeSize;
    if (s->readStatus != READ) {
      outFrames = size;
    } else {
      outFrames = writeSize;
    }
    if (p->filterName != NULL) { /* Apply filter */
      hPtr = Tcl_FindHashEntry(filterHashTable, p->filterName);
      if (hPtr != NULL) {
	f = (Snack_Filter) Tcl_GetHashValue(hPtr);
	f->si->streamWidth = globalStreamWidth;
	(f->flowProc)(f, f->si, fff, fff, &inFrames, &outFrames);
      }
      p->nWritten += nPrepared;
      if (s->readStatus != READ) {
	if (inFrames < outFrames || outFrames == 0 || inputExhausted) {
	  p->status = SNACK_QS_DRAIN;
	}
	if (outFrames < size && p->status == SNACK_QS_DRAIN) {
	  p->status = SNACK_QS_DONE;
	}
      }
      longestChunk = max(longestChunk, outFrames);
    } else { /* No filter to apply */
      if (inputExhausted) {
	p->status = SNACK_QS_DONE;
      }
      p->nWritten += nPrepared;
      outFrames = writeSize;
    }
    
    for (i = first * globalOutWidth, j = first * globalStreamWidth;
	 i < outFrames * globalOutWidth;) {
      int c;

      for (c = 0; c < globalOutWidth; c++, i++, j++) {

	switch (s->encoding) {
	case LIN16:
	case ALAW:
	case MULAW:
	  floatBuffer[i] += fff[j];
	  break;
	case LIN32:
	  floatBuffer[i] += fff[j] / 65536.0f;
	  break;
	case LIN8:
	  floatBuffer[i] += fff[j] * 256.0f;
	  break;
	case LIN8OFFSET:
	  floatBuffer[i] += (fff[j] - 128.0f) * 256.0f;
	  break;
	case LIN24:
	case LIN24PACKED:
	  floatBuffer[i] += fff[j] / 256.0f;
	  break;
	case SNACK_FLOAT:
	case SNACK_DOUBLE:
	  if (s->maxsamp > 1.0) {
	    floatBuffer[i] += fff[j];
	  } else {
	    floatBuffer[i] += fff[j] * 65536.0f;
	  }
	  break;
	}
      }
      if (globalStreamWidth > globalOutWidth) {
	j += (globalStreamWidth - globalOutWidth);
      }
    }
  } /* p = soundQueue */

  if (emptyQueue == 0 && longestChunk == 0) longestChunk = inSize;
  
  for (i = 0; i < longestChunk * globalOutWidth; i++) {
    float tmp = floatBuffer[i] * globalScaling;

    if (tmp > 32767.0f) tmp = 32767.0f;
    if (tmp < -32768.0f) tmp = -32768.0f;
    shortBuffer[i] = (short) tmp;
  }

  if (snackDumpCh) {
    Tcl_Write(snackDumpCh, (char *)shortBuffer,2*longestChunk*globalOutWidth);
  }
  chunkWritten = SnackAudioWrite(&ado, shortBuffer, longestChunk);
  globalNWritten += chunkWritten;

  if (debugLevel > 2) {
    Snack_WriteLogInt("    Exit AssembleSoundChunk", chunkWritten);
  }

  return chunkWritten;
}

#define IPLAYGRAIN 0
#define PLAYGRAIN 100

extern double globalLatency;
double startDevTime;
static int playid = 0;
static int inPlayCB = 0;

static void
PlayCallback(ClientData clientData)
{
  long currPlayed, writeable, totPlayed = 0;
  int closedDown = 0, size;
  int playgrain, blockingPlay = sCurr->blockingPlay, lastid;
  jkQueuedSound *p, *last, *q;
  Tcl_Interp *interp = sCurr->interp;

  if (debugLevel > 1) Snack_WriteLog("  Enter PlayCallback\n");

  do {
    totPlayed = SnackAudioPlayed(&ado);
    currPlayed = totPlayed - corr;
    writeable = SnackAudioWriteable(&ado);

    if (debugLevel > 2) Snack_WriteLogInt("    totPlayed", totPlayed);

    if (totPlayed == -1) { /* error in SnackAudioPlayed */
      closedDown = 1;
      break;
    }

    if (globalNWritten - currPlayed < globalLatency * globalRate ||
	blockingPlay) {
      size = (int)(globalLatency * globalRate) - (globalNWritten - currPlayed);

      if (writeable >= 0 && writeable < size) {
	size = writeable;
      }

      if (size > PBSIZE / globalStreamWidth/* || blockingPlay*/) {
	size = PBSIZE / globalStreamWidth;
      }
      
      if (AssembleSoundChunk(size) < size && globalNFlowThrough == 0) {
	static int oplayed = -1;
	double stCheck =(SnackCurrentTime() - startDevTime )*(double)globalRate;
	jkQueuedSound *p;
	int hw = 0, canCloseDown = 1;

	for (p = soundQueue; p != NULL; p = p->next) {
	  if (p->status == SNACK_QS_PAUSED) {
	    hw = 1;
	  }
	}
	if (hw) {
	  SnackAudioPause(&ado);
	  startDevTime = SnackCurrentTime() - startDevTime;
	  wop = PAUSED;
	  Tcl_DeleteTimerHandler(ptoken);
	  return;
	}

	lastid = playid;
	for (p = soundQueue; p!=NULL; p=p->next) {
	  if (p->status == SNACK_QS_DONE) {
	    if ((p->sound->linkInfo.eof == 0 && p->startPos + p->nWritten >=
		 p->endPos) ||
		(p->sound->linkInfo.eof && p->nWritten < (int)stCheck) ||
		(p->nWritten - currPlayed <= 0 || currPlayed == oplayed)) {
	      /*
		(SnackCurrentTime() - startDevTime)*globalRate)
		often never makes it to p->nWritten before object is ready to
		be closed down, so we have the last check above to make sure
	      */
	      if (p->cmdPtr != NULL) {
		ExecSoundCmd(p->sound, p->cmdPtr);
		if (debugLevel > 0)
		  Snack_WriteLogInt("   a ExecSoundCmd", (int)stCheck);
                /*
                 * The soundQueue can be removed by the -command, so check it
                 * otherwise p is garbage
                 */
                if (soundQueue == NULL) {
		  oplayed = currPlayed; /* close it down */
		  break;
                }
		if (p->cmdPtr != NULL) {
		  Tcl_DecrRefCount(p->cmdPtr);
		  p->cmdPtr = NULL;
		}
	      }
	    }
	  } else {
	    canCloseDown = 0;
	  }
	}
	if (canCloseDown) {
	  SnackAudioPost(&ado);
	  if (globalNWritten - currPlayed <= 0 || currPlayed == oplayed) {
	    if (debugLevel > 0)
	      Snack_WriteLogInt("    Closing Down",(int)SnackCurrentTime());
	    if (SnackAudioClose(&ado) != -1) {
	      if (snackDumpCh) {
		Tcl_Close(interp, snackDumpCh);
	      }
	      closedDown = 1;
	      oplayed = -1;
	      break;
	    }
	  } else {
	    oplayed = currPlayed;
	  }
	}
      }
    } /* if (globalNWritten - currPlayed < globalLatency * globalRate) */
  } while (blockingPlay);

  last = soundQueue;
  for (p = soundQueue; p != NULL; p = p->next) {
    /*    printf("%d %d %d %d %d %f\n", p->id, p->status,
	   p->startPos + p->nWritten,
		 p->endPos,
		 p->sound->linkInfo.eof,
		 (SnackCurrentTime() - startDevTime)*globalRate);*/
    if (p->status == SNACK_QS_DONE && p->sound->destroy == 0 &&
	p->cmdPtr == NULL) {
      int count = 0;
      
      for (q = soundQueue; q != NULL; q = q->next) {
	if (p->sound == q->sound) count++;
      }
      
      /*      printf("deleted %d\n", p->id);*/
      last->next = p->next;
      if (p == soundQueue) soundQueue = p->next;
      
      if (count == 1) p->sound->writeStatus = IDLE;
      if (p->filterName != NULL) {
	ckfree((char *)p->filterName);
      }
      ckfree((char *)p);
      break;
    }
    last = p;
  }


  if (closedDown) {
    CleanPlayQueue();
    wop = IDLE;
    return;
  }

  if (!blockingPlay) {
    playgrain = 30;/*max(min(PLAYGRAIN, (int) (globalLatency * 500.0)), 1);*/
    
    ptoken = Tcl_CreateTimerHandler(playgrain, (Tcl_TimerProc *) PlayCallback,
				    (int *) NULL);
  }

  if (debugLevel > 1) Snack_WriteLogInt("  Exit PlayCallback", globalNWritten);
}

void
Snack_StopSound(Sound *s, Tcl_Interp *interp)
{
  jkQueuedSound *p;
  int i;

  if (s->debug > 1) Snack_WriteLog("  Enter Snack_StopSound\n");

  if (s->writeStatus == WRITE && s->readStatus == READ) {
    globalNFlowThrough--;
  }

  if (s->storeType == SOUND_IN_MEMORY) {

    /* In-memory sound record */

    if ((rop == READ || rop == PAUSED) && (s->readStatus == READ)) {
      for (p = rsoundQueue; p->sound != s; p = p->next);
      if (p->sound == s) {
	if (p->next != NULL) {
	  p->next->prev = p->prev;
	}
	if (p->prev != NULL) {
	  p->prev->next = p->next;
	} else {
	  rsoundQueue = p->next;
	}
	ckfree((char *)p);
      }

      if (rsoundQueue == NULL && rop == READ) {
	int remaining;

	SnackAudioPause(&adi);
	remaining = SnackAudioReadable(&adi);

	while (remaining > 0) {
	  if (s->length < s->maxlength - s->samprate / 16) {
	    int nRead = 0;
	    int size = s->samprate / 16;

	    nRead = SnackAudioRead(&adi, shortBuffer, size);
	    for (i = 0; i < nRead * s->nchannels; i++) {
	      FSAMPLE(s, s->length * s->nchannels + i) =
		(float) shortBuffer[i];
	    }
	    
	    if (nRead > 0) {
	      if (s->debug > 1) Snack_WriteLogInt("  Recording", nRead);
	      Snack_UpdateExtremes(s, s->length, s->length + nRead,
				   SNACK_MORE_SOUND);
	      s->length += nRead;
	    }
	    remaining -= nRead;
	  } else {
	    break;
	  }
	}
	SnackAudioFlush(&adi);
	SnackAudioClose(&adi);
	Tcl_DeleteTimerHandler(rtoken);
	rop = IDLE;
      }
      s->readStatus = IDLE;
      Snack_ExecCallbacks(s, SNACK_MORE_SOUND);
    }
    
    /* In-memory sound play */

    if ((wop == WRITE || wop == PAUSED) && (s->writeStatus == WRITE)) {
      int hw = 1;

      if (s->debug > 1) Snack_WriteLogInt("  Stopping",SnackAudioPlayed(&ado));

      for (p = soundQueue; p != NULL; p = p->next) {
	if (p->sound == s) {
	  p->status = SNACK_QS_DONE;
	}
      }
      
      for (p = soundQueue; p != NULL; p = p->next) {
	if (p->status != SNACK_QS_DONE) {
	  hw = 0;
	}
      }

      if (hw == 1) {
	if (wop == PAUSED) {
	  SnackAudioResume(&ado);
	}
	SnackAudioFlush(&ado);
	SnackAudioClose(&ado);
	wop = IDLE;
	Tcl_DeleteTimerHandler(ptoken);
	CleanPlayQueue();
      }

    }
  } else { /* sound in file or channel */

    /* file or channel sound record */

    if ((rop == READ || rop == PAUSED) && (s->readStatus == READ)) {
      Snack_FileFormat *ff;
      for (p = rsoundQueue; p->sound != s; p = p->next);
      if (p->sound == s) {
	if (p->next != NULL) {
	  p->next->prev = p->prev;
	}
	if (p->prev != NULL) {
	  p->prev->next = p->next;
	} else {
	  rsoundQueue = p->next;
	}
	ckfree((char *)p);
      }
      
      if (rsoundQueue == NULL && rop == READ) {
	int remaining;

	SnackAudioPause(&adi);
	remaining = SnackAudioReadable(&adi);

	while (remaining > 0) {
	  int nRead = 0, i;
	  int size = s->samprate / 16;
	  nRead = SnackAudioRead(&adi, shortBuffer, size);
	  	  
       	  if ((s->length + nRead - s->validStart) * s->nchannels > FBLKSIZE) {
	    s->validStart += (BUFSCROLLSIZE / s->nchannels);
	    memmove(&s->blocks[0][0], &s->blocks[0][BUFSCROLLSIZE],
		    (FBLKSIZE-BUFSCROLLSIZE) * sizeof(float));
	  }
	  
	  for (i = 0; i < nRead * s->nchannels; i++) {
	    FSAMPLE(s, (s->length - s->validStart) * s->nchannels + i) =
	      (float) shortBuffer[i];
	  }

	  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
	    if (strcmp(s->fileType, ff->name) == 0) {
	      WriteSound(ff->writeProc, s, s->interp, s->rwchan, NULL,
			 s->length - s->validStart, nRead);
	    }
	  }
	  /*
	  WriteSound(NULL, s, s->interp, s->rwchan, NULL,
		     (s->length - s->validStart) * s->nchannels,
		     nRead * s->nchannels);
	  */
	  Tcl_Flush(s->rwchan);

	  if (s->debug > 2) Snack_WriteLogInt("    Tcl_Read", nRead);
	  
	  s->length += nRead;
	  remaining -= nRead;
	}
	SnackAudioFlush(&adi);
	SnackAudioClose(&adi);
	Tcl_DeleteTimerHandler(rtoken);
	rop = IDLE;
	CleanRecordQueue();
      }
      if (TCL_SEEK(s->rwchan, 0, SEEK_SET) != -1) {
	PutHeader(s, interp, 0, NULL, s->length);
	TCL_SEEK(s->rwchan, 0, SEEK_END);
      }
      if (s->storeType == SOUND_IN_FILE) {
	for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
	  if (strcmp(s->fileType, ff->name) == 0) {
	    SnackCloseFile(ff->closeProc, s, interp, &s->rwchan);
	  }
	}
	/*Tcl_Close(interp, s->rwchan);*/
      }
      /*ckfree((char *)s->tmpbuf);
	s->tmpbuf = NULL;*/
      s->rwchan = NULL;
      s->validStart = 0;
      s->readStatus = IDLE;
      Snack_ExecCallbacks(s, SNACK_MORE_SOUND); 
    }

    /* file or channel sound play */

    if ((wop == WRITE || wop == PAUSED) && (s->writeStatus == WRITE)) {
      int hw = 1;

      if (s->debug > 1) Snack_WriteLogInt("  Stopping",SnackAudioPlayed(&ado));

      for (p = soundQueue; p != NULL; p = p->next) {
	if (p->sound == s) {
	  p->status = SNACK_QS_DONE;
	}
      }
      
      for (p = soundQueue; p != NULL; p = p->next) {
	if (p->status != SNACK_QS_DONE) {
	  hw = 0;
	}
      }
      
      if (hw == 1) {
	if (wop == PAUSED) {
	  SnackAudioResume(&ado);
	}
	SnackAudioFlush(&ado);
	SnackAudioClose(&ado);
	wop = IDLE;
	Tcl_DeleteTimerHandler(ptoken);
	CleanPlayQueue();
      }	
      /*      ckfree((char *)s->tmpbuf);
	      s->tmpbuf = NULL;*/
      if (s->rwchan != NULL) {
	if (s->storeType == SOUND_IN_FILE) {
	  Snack_FileFormat *ff;
	  for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
	    if (strcmp(s->fileType, ff->name) == 0) {
	      SnackCloseFile(ff->closeProc, s, s->interp, &s->rwchan);
	      s->rwchan = NULL;
	      break;
	    }
	  }
	}
      }
    }
  }
  
  if (s->debug > 1) Snack_WriteLog("  Exit Snack_StopSound\n");
}

extern char defaultOutDevice[];

int
playCmd(Sound *s, Tcl_Interp *interp, int objc,	Tcl_Obj *CONST objv[])
{
  int startPos = 0, endPos = -1, block = 0, arg, startTime = 0, duration = 0;
  int devChannels = -1, rate = -1, noPeeping = 0;
  double dStart = 0.0, dDuration = 0.0;
  static CONST84 char *subOptionStrings[] = {
    "-output", "-start", "-end", "-command", "-blocking", "-device", "-filter",
    "-starttime", "-duration", "-devicechannels", "-devicerate", "-nopeeping",
    NULL
  };
  enum subOptions {
    OUTPUT, STARTPOS, END, COMMAND, BLOCKING, DEVICE, FILTER, STARTTIME,
    DURATION, DEVCHANNELS, DEVRATE, NOPEEPING
  };
  jkQueuedSound *qs, *p;
  Snack_FileFormat *ff;
  Snack_Filter f = NULL;
  char *filterName = NULL;
  Tcl_Obj *cmdPtr = NULL;

  if (s->writeStatus == WRITE && wop == PAUSED) {
    for (p = soundQueue; p != NULL; p = p->next) {
      if (p->sound == s) {
	if (p->status == SNACK_QS_PAUSED) {
	  p->status = SNACK_QS_QUEUED;
	}
      }
    }
    startDevTime = SnackCurrentTime() - startDevTime;
    wop = WRITE;
    SnackAudioResume(&ado);
    ptoken = Tcl_CreateTimerHandler(IPLAYGRAIN,
			     (Tcl_TimerProc *) PlayCallback, (int *) NULL);
    return TCL_OK;
  }

  s->firstNRead = 0;
  s->devStr = defaultOutDevice;

  for (arg = 2; arg < objc; arg+=2) {
    int index, length;
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
    case OUTPUT:
      {
	str = Tcl_GetStringFromObj(objv[arg+1], &length);
	SnackMixerSetOutputJack(str, "1");
	break;
      }
    case STARTPOS:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &startPos) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case END:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &endPos) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case COMMAND:
      {
	Tcl_IncrRefCount(objv[arg+1]);
	cmdPtr = objv[arg+1];
	break;
      }
    case BLOCKING:
      {
	if (Tcl_GetBooleanFromObj(interp, objv[arg+1], &block) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case DEVICE:
      {
	int i, n, found = 0;
	char *arr[MAX_NUM_DEVICES];

	s->devStr = Tcl_GetStringFromObj(objv[arg+1], NULL);
	
	if (strlen(s->devStr) > 0) {
	  n = SnackGetOutputDevices(arr, MAX_NUM_DEVICES);
	  
	  for (i = 0; i < n; i++) {
	    if (strncmp(s->devStr, arr[i], strlen(s->devStr)) == 0) {
	      found = 1;
	    }
	    ckfree(arr[i]);
	  }
	  if (found == 0) {
	    Tcl_AppendResult(interp, "No such device: ", s->devStr,
			     (char *) NULL);
	    return TCL_ERROR;
	  }
	}
	break;
      }
    case FILTER:
      {
	char *str = Tcl_GetStringFromObj(objv[arg+1], NULL);

	if (strlen(str) > 0) {
	  Tcl_HashEntry *hPtr;
	  
	  hPtr = Tcl_FindHashEntry(filterHashTable, str);
	  if (hPtr == NULL) {
	    Tcl_AppendResult(interp, "No such filter: ", str,
			     (char *) NULL);
	    return TCL_ERROR;
	  }
	  filterName = ckalloc(strlen(str)+1);
	  if (filterName) {
	    strncpy(filterName, str, strlen(str)+1);
	  }
	  f = (Snack_Filter) Tcl_GetHashValue(hPtr);
	  if (f->si != NULL) ckfree((char *) f->si);
	  f->si = (Snack_StreamInfo) ckalloc(sizeof(SnackStreamInfo));
	}
	break;
      }
    case STARTTIME:
      {
	if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &dStart) != TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    case DURATION:
      {
	if (Tcl_GetDoubleFromObj(interp, objv[arg+1], &dDuration) != TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    case DEVCHANNELS:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &devChannels) != TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    case DEVRATE:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &rate) != TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    case NOPEEPING:
      {
 	if (Tcl_GetBooleanFromObj(interp, objv[arg+1], &noPeeping) != TCL_OK)
 	  return TCL_ERROR;
 	break;
      }
    }
  }
  if (s->storeType == SOUND_IN_CHANNEL && !noPeeping) {
    int tlen = 0, rlen = 0;

    s->buffersize = CHANNEL_HEADER_BUFFER;
    if ((s->tmpbuf = (short *) ckalloc(CHANNEL_HEADER_BUFFER)) == NULL) {
      Tcl_AppendResult(interp, "Could not allocate buffer!", NULL);
      return TCL_ERROR;
    }
    while (tlen < s->buffersize) {
      rlen = Tcl_Read(s->rwchan, &((char *)s->tmpbuf)[tlen], 1);
      if (rlen <= 0) break;
      s->firstNRead += rlen;
      tlen += rlen;
      if (s->forceFormat == 0) {
	s->fileType = GuessFileType((char *)s->tmpbuf, tlen, 0);
	if (strcmp(s->fileType, QUE_STRING) != 0) break;
      }
    }
    for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
      if (strcmp(s->fileType, ff->name) == 0) {
	if ((ff->getHeaderProc)(s, interp, s->rwchan, NULL,
				(char *)s->tmpbuf)
	    != TCL_OK) return TCL_ERROR;
	break;
      }
    }
    if (strcmp(s->fileType, RAW_STRING) == 0 && s->guessEncoding) {
      GuessEncoding(s, (unsigned char *)s->tmpbuf, s->firstNRead / 2);
    }
    ckfree((char *)s->tmpbuf);
    s->tmpbuf = NULL;
    s->firstNRead -= s->headSize;
  }
  if (s->storeType != SOUND_IN_MEMORY) {
    /*if (s->buffersize < s->samprate / 2) {
      s->buffersize = s->samprate / 2;
    }
    if (s->tmpbuf) {
      ckfree((char *)s->tmpbuf);
    }
    if ((s->tmpbuf = (short *) ckalloc(s->buffersize * s->sampsize *
				       s->nchannels)) == NULL) {
      Tcl_AppendResult(interp, "Could not allocate buffer!", NULL);
      return TCL_ERROR;
    }
     */
    if (s->linkInfo.linkCh == NULL && s->storeType == SOUND_IN_FILE) {
      if (OpenLinkedFile(s, &s->linkInfo) != TCL_OK) {
	return TCL_ERROR;
      }
    }
  }
  if (s->storeType == SOUND_IN_MEMORY) {
    if (endPos < 0 || endPos > s->length - 1) endPos = s->length - 1;
  } else if (s->length != -1 && s->storeType == SOUND_IN_FILE) {
    if (endPos < 0 || endPos > s->length - 1) endPos = s->length - 1;
  } else {
    s->length = 0;
  }
  if (startPos >= endPos && endPos != -1) {
    ExecSoundCmd(s, cmdPtr);
    if (cmdPtr != NULL) Tcl_DecrRefCount(cmdPtr);
    return TCL_OK;
  }
  if (startPos < 0) startPos = 0;
  if (s->storeType == SOUND_IN_CHANNEL) {
    s->linkInfo.sound = s;
    s->linkInfo.buffer = (float *) ckalloc(ITEMBUFFERSIZE);
    s->linkInfo.filePos = -1;
    s->linkInfo.linkCh = s->rwchan;
    s->linkInfo.validSamples = 0;
    s->linkInfo.eof = 0;
  }
  if (rate == -1) {
    rate = s->samprate;
  }

#ifdef MAC_OSX_TCL
  rate = 44100;
#endif

  if (dStart > 0) {
    if (wop == IDLE) {
      startTime = (int) (dStart / 1000.0 * rate + .5);
    } else {
      startTime = (int) (dStart / 1000.0 * globalRate + .5);
    }
  }
  if (inPlayCB) {
    startTime += inPlayCB;
  }
  if (dDuration > 0) {
    if (wop == IDLE) {
      duration = (int) (dDuration / 1000.0 * rate + .5);
    } else {
      duration = (int) (dDuration / 1000.0 * globalRate + .5);
    }
  }
  qs = (jkQueuedSound *) ckalloc(sizeof(jkQueuedSound));
  
  if (qs == NULL) {
    Tcl_AppendResult(interp, "Unable to alloc queue struct", NULL);
    return TCL_ERROR;
  }
  qs->sound = s;
  qs->name = "junk";
  qs->startPos = startPos;
  qs->endPos = endPos;
  qs->nWritten = 0;
  qs->startTime = startTime;
  qs->duration = duration;
  qs->cmdPtr = cmdPtr;
  qs->status = SNACK_QS_QUEUED;
  qs->filterName = filterName;
  qs->next = NULL;
  qs->id = playid++;
  if (soundQueue == NULL) {
    soundQueue = qs;
  } else {
    for (p = soundQueue; p->next != NULL; p = p->next);
    p->next = qs;
  }

  if (wop == IDLE) {
    if (devChannels == -1) {
      globalStreamWidth = s->nchannels;
      if (s->nchannels > SnackAudioMaxNumberChannels(s->devStr)) {
	devChannels = SnackAudioMaxNumberChannels(s->devStr);
      } else {
	devChannels = s->nchannels;
      }
      if (devChannels < SnackAudioMinNumberChannels(s->devStr)) {
	devChannels = SnackAudioMinNumberChannels(s->devStr);
	globalStreamWidth = devChannels;
      }
    } else {
      globalStreamWidth = devChannels; /* option -devicechannels used */
    }
  } else {
    if (s->nchannels > globalStreamWidth) {
      globalStreamWidth = s->nchannels;
    }
    devChannels = globalStreamWidth;
  }

  if (filterName != NULL) {
    f->si->streamWidth = globalStreamWidth;
    f->si->outWidth    = devChannels;
    f->si->rate        = rate;
    (f->startProc)(f, f->si);
  }

  if (!((wop == IDLE) && (s->writeStatus == IDLE))) {
    s->writeStatus = WRITE;

    if (wop == PAUSED) {
      startDevTime = SnackCurrentTime() - startDevTime;
      wop = WRITE;
      SnackAudioResume(&ado);
      ptoken = Tcl_CreateTimerHandler(IPLAYGRAIN,
				      (Tcl_TimerProc *) PlayCallback,
				      (int *) NULL);
    }
    return TCL_OK;
  } else {
    qs->status = SNACK_QS_QUEUED;
  }
  ado.debug = s->debug;
  if (s->storeType == SOUND_IN_FILE) {
    s->rwchan = NULL;
  }    
  wop = WRITE;
  s->writeStatus = WRITE;

  if (SnackAudioOpen(&ado, interp, s->devStr, PLAY, rate, devChannels,
		     LIN16) != TCL_OK) {
    wop = IDLE;
    s->writeStatus = IDLE;
    return TCL_ERROR;
  }
  if (snackDumpFile) {
    snackDumpCh = Tcl_OpenFileChannel(interp, snackDumpFile, "w", 438);
    Tcl_SetChannelOption(interp, snackDumpCh, "-translation", "binary");
#ifdef TCL_81_API
    Tcl_SetChannelOption(interp, snackDumpCh, "-encoding", "binary");
#endif
  }
  globalRate = rate;
  globalOutWidth = devChannels;
  globalNWritten = 0;
  if (s->writeStatus == WRITE && s->readStatus == READ) {
    globalNFlowThrough++;
  }
  sCurr = s;
  s->blockingPlay = block;
  corr = 0;
  if (s->blockingPlay) {
    PlayCallback((ClientData) s);
  } else {
    ptoken = Tcl_CreateTimerHandler(IPLAYGRAIN, (Tcl_TimerProc *) PlayCallback,
				    (int *) NULL);
  }
  if (rop == IDLE) {
   startDevTime = SnackCurrentTime();
  }

  return TCL_OK;
}

extern char defaultInDevice[];

int
recordCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  jkQueuedSound *qs, *p;
  int arg, append = 0, mode, encoding = LIN16;
  static CONST84 char *subOptionStrings[] = {
    "-input", "-append", "-device", "-fileformat", NULL
  };
  enum subOptions {
    INPUT, APPEND, DEVICE, FILEFORMAT
  };
  
  if (s->debug > 0) { Snack_WriteLog("Enter recordCmd\n"); }

  if (s->encoding == LIN24 || s->encoding == LIN24PACKED || s->encoding == SNACK_FLOAT
      || s->encoding == LIN32) encoding = LIN24;

  if (s->readStatus == READ && rop == PAUSED) {
    startDevTime = SnackCurrentTime() - startDevTime;
    rop = READ;
    if (SnackAudioOpen(&adi, interp, s->devStr, RECORD, s->samprate,
		       s->nchannels, encoding) != TCL_OK) {
      rop = IDLE;
      s->readStatus = IDLE;
      return TCL_ERROR;
    }
    SnackAudioFlush(&adi);
    SnackAudioResume(&adi);
    Snack_ExecCallbacks(s, SNACK_MORE_SOUND); 
    rtoken = Tcl_CreateTimerHandler(RECGRAIN, (Tcl_TimerProc *) RecCallback,
				    (int *) NULL);

    return TCL_OK;
  }

  if (s->readStatus == IDLE) {
    s->readStatus = READ;
  } else {
    return TCL_OK;
  }

  s->devStr = defaultInDevice;
  s->tmpbuf = NULL;
      
  for (arg = 2; arg < objc; arg+=2) {
    int index, length;
    char *str;
    
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
    case INPUT:
      {
	str = Tcl_GetStringFromObj(objv[arg+1], &length);
	SnackMixerSetInputJack(interp, str, "1");
	break;
      }
    case APPEND:
      {
	if (Tcl_GetBooleanFromObj(interp, objv[arg+1], &append) != TCL_OK) {
	  return TCL_ERROR;
	}
	break;
      }
    case DEVICE:
      {
	int i, n, found = 0;
	char *arr[MAX_NUM_DEVICES];

	s->devStr = Tcl_GetStringFromObj(objv[arg+1], NULL);

	if (strlen(s->devStr) > 0) {
	  n = SnackGetInputDevices(arr, MAX_NUM_DEVICES);
	  
	  for (i = 0; i < n; i++) {
	    if (strncmp(s->devStr, arr[i], strlen(s->devStr)) == 0) {
	      found = 1;
	    }
	    ckfree(arr[i]);
	  }
	  if (found == 0) {
	    Tcl_AppendResult(interp, "No such device: ", s->devStr,
			     (char *) NULL);
	    return TCL_ERROR;
	  }
	}
	break;
      }
    case FILEFORMAT:
      {
	if (GetFileFormat(interp, objv[arg+1], &s->fileType) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    }
  }
  
  qs = (jkQueuedSound *) ckalloc(sizeof(jkQueuedSound));
  
  if (qs == NULL) {
    Tcl_AppendResult(interp, "Unable to alloc queue struct", NULL);
    return TCL_ERROR;
  }
  qs->sound = s;
  qs->name = Tcl_GetStringFromObj(objv[0], NULL);
  qs->status = SNACK_QS_QUEUED;
  qs->next = NULL;
  qs->prev = NULL;
  if (rsoundQueue == NULL) {
    rsoundQueue = qs;
  } else {
    for (p = rsoundQueue; p->next != NULL; p = p->next);
    p->next = qs;
    qs->prev = p;
  }
  
  if (!append) {
    s->length = 0;
    s->maxsamp = 0.0f;
    s->minsamp = 0.0f;
  }

  if (s->storeType == SOUND_IN_MEMORY) {
  } else { /* SOUND_IN_FILE or SOUND_IN_CHANNEL */
    if (s->buffersize < s->samprate / 2) {
      s->buffersize = s->samprate / 2;
    }
    
    if ((s->tmpbuf = (short *) ckalloc(s->buffersize * s->sampsize * 
				       s->nchannels)) == NULL) {
      Tcl_AppendResult(interp, "Could not allocate buffer!", NULL);
      return TCL_ERROR;
    }
    
    if (s->storeType == SOUND_IN_FILE) {
      Snack_FileFormat *ff;

      for (ff = snackFileFormats; ff != NULL; ff = ff->nextPtr) {
	if (strcmp(s->fileType, ff->name) == 0) {
	  if (SnackOpenFile(ff->openProc, s, interp, &s->rwchan, "w") !=
	      TCL_OK) {
	    return TCL_ERROR;
	  }
	}
      }

      /*
	s->rwchan = Tcl_OpenFileChannel(interp, s->fcname, "w", 420);
      */
      if (s->rwchan != NULL) {
	mode = TCL_WRITABLE;
      }
    } else {
      s->rwchan = Tcl_GetChannel(interp, s->fcname, &mode);
    }
    
    if (s->rwchan == NULL) {
      return TCL_ERROR;
    }
    Tcl_SetChannelOption(interp, s->rwchan, "-translation", "binary");
#ifdef TCL_81_API
    Tcl_SetChannelOption(interp, s->rwchan, "-encoding", "binary");
#endif
    if (!(mode & TCL_WRITABLE)) {
      Tcl_AppendResult(interp, "channel \"", s->fcname, 
		       "\" wasn't opened for writing", NULL);
      s->rwchan = NULL;
      return TCL_ERROR;
    }
    
    if (PutHeader(s, interp, 0, NULL, -1) < 0) {
      return TCL_ERROR;
    }
    s->validStart = 0;
  }
  Snack_ResizeSoundStorage(s, FBLKSIZE);

  if (rop == IDLE || rop == PAUSED) {
    adi.debug = s->debug;
    if (SnackAudioOpen(&adi, interp, s->devStr, RECORD, s->samprate,
		       s->nchannels, encoding) != TCL_OK) {
      rop = IDLE;
      s->readStatus = IDLE;
      return TCL_ERROR;
    }
    SnackAudioFlush(&adi);
    SnackAudioResume(&adi);
    rtoken = Tcl_CreateTimerHandler(RECGRAIN,(Tcl_TimerProc *) RecCallback,
				    (int *) NULL);
  }
  globalRate = s->samprate;
  if (s->writeStatus == WRITE && s->readStatus == READ) {
    globalNFlowThrough++;
  }
  globalStreamWidth = s->nchannels;
  numRec++;
  rop = READ;
  if (wop == IDLE) {
    startDevTime = SnackCurrentTime();
  }
  Snack_ExecCallbacks(s, SNACK_NEW_SOUND);

  if (s->debug > 0) { Snack_WriteLog("Exit recordCmd\n"); }

  return TCL_OK;
}

int
stopCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  Snack_StopSound(s, interp);

  return TCL_OK;
}

int
pauseCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  jkQueuedSound *p;

  if (s->debug > 1) Snack_WriteLog("  Enter pauseCmd\n");

  if (s->writeStatus == WRITE) {
    int hw = 1;

    for (p = soundQueue; p != NULL; p = p->next) {
      if (p->sound == s) {
	if (p->status == SNACK_QS_QUEUED) {
	  p->status = SNACK_QS_PAUSED;
	} else if (p->status == SNACK_QS_PAUSED) {
	  p->status = SNACK_QS_QUEUED;
	}
      }
    }

    for (p = soundQueue; p != NULL; p = p->next) {
      if (p->status == SNACK_QS_QUEUED) {
	hw = 0;
      }
    }

    if (hw == 1 || wop == PAUSED) {
      if (wop == WRITE) {
	long tmp = SnackAudioPause(&ado);

	startDevTime = SnackCurrentTime() - startDevTime;
	wop = PAUSED;

        Tcl_DeleteTimerHandler(ptoken);
	if (tmp != -1) {
	  jkQueuedSound *p;
	  long count = 0;

	  for (p = soundQueue; p != NULL && p->status == SNACK_QS_PAUSED;
	       p = p->next) {
	    long totLen;

            if (p->endPos == -1) {
	      totLen = (p->sound->length - p->startPos);
	    } else {
	      totLen = (p->endPos - p->startPos + 1);
	    }

	    count += totLen;

	    if (count > tmp) {
	      sCurr = p->sound;
	      globalNWritten = tmp - (count - totLen);
	      corr = count - totLen;
	      break;
	    }
	  }
	  /*
	  for (p = p->next; p != NULL && p->status == SNACK_QS_PAUSED;
	       p = p->next) {
	    p->status = SNACK_QS_QUEUED;
	    }*/
	}
      } else if (wop == PAUSED) {
	startDevTime = SnackCurrentTime() - startDevTime;
	wop = WRITE;
	SnackAudioResume(&ado);
	ptoken = Tcl_CreateTimerHandler(IPLAYGRAIN, (Tcl_TimerProc *) PlayCallback,
					(int *) NULL);
      }
    }
  }
  if (s->readStatus == READ) {
    int hw = 1;

    for (p = rsoundQueue; p != NULL && p->sound != s; p = p->next);
    if (p->sound == s) {
      if (p->status == SNACK_QS_QUEUED) {
	p->status = SNACK_QS_PAUSED;
      } else if (p->status == SNACK_QS_PAUSED) {
	p->status = SNACK_QS_QUEUED;
      }
    }
    
    for (p = rsoundQueue; p != NULL; p = p->next) {
      if (p->status == SNACK_QS_QUEUED) {
	hw = 0;
      } 
    }

    if (hw == 1 || rop == PAUSED) {
      if (rop == READ) {
	int remaining;

	SnackAudioPause(&adi);
	startDevTime = SnackCurrentTime() - startDevTime;

	remaining = SnackAudioReadable(&adi);

	while (remaining > 0) {
	  if (s->length < s->maxlength - s->samprate / 16) {
	    int nRead = 0;
	    int size = s->samprate / 16, i;

	    nRead = SnackAudioRead(&adi, shortBuffer, size);
	    for (i = 0; i < nRead * s->nchannels; i++) {
	      FSAMPLE(s, s->length * s->nchannels + i) =
		(float) shortBuffer[i];
	    }
	    
	    if (nRead > 0) {
	      if (s->debug > 1) Snack_WriteLogInt("  Recording", nRead);
	      Snack_UpdateExtremes(s, s->length, s->length + nRead,
				   SNACK_MORE_SOUND);
	      s->length += nRead;
	    }
	    remaining -= nRead;
	  } else {
	    break;
	  }
	}
	SnackAudioFlush(&adi);
	SnackAudioClose(&adi);
	rop = PAUSED;
	s->readStatus = READ;
	Tcl_DeleteTimerHandler(rtoken);
      } else if (rop == PAUSED) {
	for (p = rsoundQueue; p->sound != s; p = p->next);
	if (p->sound == s) {
	  p->status = SNACK_QS_QUEUED;
	}
	
	rop = READ;
	if (SnackAudioOpen(&adi, interp, s->devStr, RECORD, s->samprate,
			   s->nchannels, LIN16) != TCL_OK) {
	  rop = IDLE;
	  s->readStatus = IDLE;
	  return TCL_ERROR;
	}
	SnackAudioFlush(&adi);
	SnackAudioResume(&adi);
	startDevTime = SnackCurrentTime() - startDevTime;
	Snack_ExecCallbacks(s, SNACK_MORE_SOUND); 
	rtoken = Tcl_CreateTimerHandler(RECGRAIN,
					(Tcl_TimerProc *) RecCallback,
					(int *) NULL);
      }
    }
  }

  if (s->debug > 1) Snack_WriteLog("  Exit pauseCmd\n");

  return TCL_OK;
}

int
current_positionCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int n = -1;
  int arg, len, type = 0;
  jkQueuedSound *p;

  if (soundQueue != NULL) {
    for (p = soundQueue; p != NULL && p->sound != s; p = p->next);
    if (p->sound == s) {
      n = p->startPos + p->nWritten;
    }
  }
  if (wop == IDLE) {
    Tcl_SetObjResult(interp, Tcl_NewIntObj(-1));
    return TCL_OK;
  }
  for (arg = 2; arg < objc; arg++) {
    char *string = Tcl_GetStringFromObj(objv[arg], &len);
	
    if (strncmp(string, "-units", len) == 0) {
      string = Tcl_GetStringFromObj(objv[++arg], &len);
      if (strncasecmp(string, "seconds", len) == 0) type = 1;
      if (strncasecmp(string, "samples", len) == 0) type = 0;
      arg++;
    }
  }
  
  if (type == 0) {
    Tcl_SetObjResult(interp, Tcl_NewIntObj(max(n, 0)));
  } else {
    Tcl_SetObjResult(interp, Tcl_NewDoubleObj((float) max(n,0) / s->samprate));
  }

  return TCL_OK;
}

void
Snack_ExitProc(ClientData clientData)
{
  if (debugLevel > 1) Snack_WriteLog("  Enter Snack_ExitProc\n");

  if (rop != IDLE) {
    SnackAudioFlush(&adi);
    SnackAudioClose(&adi);
  }
  if (wop != IDLE) {
    SnackAudioFlush(&ado);
    SnackAudioClose(&ado);
  }
  SnackAudioFree();
  rop = IDLE;
  wop = IDLE;
  if (debugLevel > 1) Snack_WriteLog("  Exit Snack\n");
}

/*
 *----------------------------------------------------------------------
 *
 * SnackCurrentTime --
 *
 *	Returns the current system time in seconds (with decimals)
 *	since the beginning of the epoch: 00:00 UCT, January 1, 1970.
 *
 * Results:
 *	Returns the current time.
 *
 *----------------------------------------------------------------------
 */

#ifdef MAC
#  include <time.h>
#elif  defined(WIN)
#  include <sys/types.h>
#  include <sys/timeb.h>
#else
#  include <sys/time.h>
#endif

double
SnackCurrentTime()
{
#if defined(MAC)
	double nTime;
	clock_t tclock;
	double t;

	tclock = clock();
	t = (double) CLOCKS_PER_SEC;
	nTime = (double) tclock;
	nTime = nTime / t;
	return(nTime);
#elif defined(WIN)
  struct timeb t;
  
  ftime(&t);

  return(t.time + t.millitm * 0.001);
#else
  struct timeval tv;
  struct timezone tz;
  
  (void) gettimeofday(&tv, &tz);

  return(tv.tv_sec + tv.tv_usec * 0.000001);

#endif
}

void SnackPauseAudio()
{
  if (wop == WRITE) {
    SnackAudioPause(&ado);    
    startDevTime = SnackCurrentTime() - startDevTime;
    wop = PAUSED;
    Tcl_DeleteTimerHandler(ptoken);
  } else if (wop == PAUSED) {
    startDevTime = SnackCurrentTime() - startDevTime;
    wop = WRITE;
    SnackAudioResume(&ado);
    ptoken = Tcl_CreateTimerHandler(IPLAYGRAIN, (Tcl_TimerProc *) PlayCallback,
				    (int *) NULL);
  }
}
