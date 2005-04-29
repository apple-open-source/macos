/*
 * Copyright (C) 1998-2002
 * Dan Ellis
 * Leonid Spektor
 * Frederic Bonnet
 * Kåre Sjölander
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
#include "jkAudIO.h"
#include "jkSound.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <Memory.h>

extern void Snack_WriteLog(char *s);
extern void Snack_WriteLogInt(char *s, int n);

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define SNACK_NUMBER_MIXERS 1

struct MixerLink mixerLinks[SNACK_NUMBER_MIXERS][2];

#define ABS(a) (((a)<0)?-(a):(a))

#ifndef TRUE
#define TRUE (1)
#define FALSE (0)
#endif /* !TRUE */

#define DBGSTR(a)
#define DBGINT(a)
#define ASSERT(a) if(!(a)) { DBGSTR(#a); DBGSTR(" (assert failed)\n"); SysBeep(10); }

/* init support */
static int
GetSndInputNames(long inRefNum, int which, char *buf) 
{
  /* Scan the input names associated with the open sndin <inrefnum>
     and copy them into pre-allocated string <buf>.  If which <= 0, 
     return them all separated by spaces; otherwise, return just 
     the one indexed by <which>.  Return value is the total number 
     of names available, -1 on error. */
  int rc = -1;
  
  /* default return is empty string */
  OSErr oe;
  unsigned short sbuf[32];
  Handle tmpHan;
  short *sp;
  int i, count;
  char *cp;
  
  /* default to empty list */
  buf[0] = '\0';
  
  /* Get available input sources, by name */
  oe = SPBGetDeviceInfo(inRefNum, siInputSourceNames, (void *)sbuf);
  if (oe == 0) {
    tmpHan = (Handle)((sbuf[0]<<16) + sbuf[1]);
    HLock(tmpHan);
    sp = *(short **)tmpHan;
    count = sp[0];
    /* fprintf(stderr, "siInputSourceNames: rc=%d count=%d\n", oe, count); */
    cp = (char *)(sp+1);
    for (i=0; i<count && i < 16; ++i) {
      int namlen = (unsigned char)cp[0];
      if (which < 1 || which == i+1) {
	int j, bracket = 0;
	/* copy this name, perhaps with a space sep */
	if (strlen(buf)) {
	  strcat(buf, " ");
	}
	/* We need to put brackets around elements containing spaces */
	for (j=0; j < namlen; ++j) { if (cp[j+1] == ' ')  bracket = 1; }
	if (bracket)  strcat(buf, "{");
	strncat(buf, cp+1, namlen);
	if (bracket)  strcat(buf, "}");
      }
      /* step over str len */
      cp += 1 + namlen;
    }
    HUnlock(tmpHan);
    DisposeHandle(tmpHan);
    rc = count;
  }
  return rc;
}

/* playback support */

static void
dbpFill(ADesc *A)
{   /* using the data pointers in the au, fill in as much more into the 
       current db. Carry on and fill the other if it is not already full. */
  char  *src, *dst;
  int   chans = A->nChannels;
  int   fmtsz = A->bytesPerSample;
  long  todo;
  int   otherBufFull, thisbuf = A->currentBuf;
  SndDoubleBufferPtr db = A->bufs[thisbuf];
  
  todo = min(A->bufFrames - db->dbNumFrames, A->totalFrames - A->doneFrames);
  src = ((char *)A->data) + A->doneFrames * chans * fmtsz;
  dst = ((char *)db->dbSoundData) + db->dbNumFrames * chans * fmtsz;
  BlockMove(src, dst, chans*fmtsz*todo);
  A->doneFrames  += todo;
  db->dbNumFrames += todo;
  /* first check if this buffer is full.  If it is, we need to block until
     the next turnaround, so don't clear the 'ready for more data' semaphore */
  if (db->dbNumFrames == A->bufFrames) {  /* this buffer all set to go */
    A->bufFull[thisbuf] = 1;
    /* once we set this flag, if the interrupt occurs it will automatically
       call the follow-on routine.	We may try to too, but it will already
       be full - no harm done. */
    db->dbFlags |= dbBufferReady;	/* triggers the DBP to be called for the other */
    otherBufFull = A->bufFull[1-thisbuf];
    if (!otherBufFull) { /* we have to fill both buffers - like an underrun */
      ++(A->underruns);
      A->currentBuf = 1-thisbuf;
      dbpFill(A);
    }
  } else {	/* we did not fill the buffer - must have run out of data */
    /* mark that we would like more data */
    A->doneFrames	= 0;
    A->totalFrames = 0;
    A->data	= NULL;
  }
}

static pascal void
DoubleBackProc(SndChannelPtr schn, SndDoubleBufferPtr db)
{   /* callback when one buffer is empty */
  ADesc *A = (ADesc *)db->dbUserInfo[0];
  int returnedbuf = (db==A->bufs[0])?0:1;
  
  ++(A->completedblocks);
  /* flag that returned buf is ready for filling */
  db->dbNumFrames = 0;
  A->bufFull[returnedbuf] = 0;
  /* since this routine isn't called until both buffers are marked ready,
     we know that the other buffer has completed filling, so we can go right
     ahead and try to fill this buffer. */
  if (A->bufFull[1-returnedbuf]) {
    A->currentBuf = returnedbuf;
    dbpFill(A);
  } else {
    /* underrun, somehow */
    ++(A->underruns);
  }
}

static int 
MySndIdle(SndChannelPtr schn)
{   /* an activity to put in a polling loop */
  SCStatus	status;
  SndChannelStatus(schn, sizeof(status), &status);
  return status.scChannelBusy;
}

static void 
AUWaitNoData(ADesc *A)
{   /* Wait until the double-back procedure indicates that the current data
       has all been copied into buffers. */
  if (A->debug > 2) Snack_WriteLog("    AUWaitNoData\n");
  while(A->data != NULL) {
    MySndIdle(A->schn);
  }
}

static void 
AUWaitNotBusy(ADesc *A)
{   /* block until the current playing is complete */
  /* can look at channel to see if it's busy, also look at double buffers
     to see when they have reached the end.  We'll look at the channel */
  if (A->debug > 2) Snack_WriteLog("    AUWaitNotBusy\n");
  while(MySndIdle(A->schn))
    ;
  A->running = 0;
}

static void 
AUNewData(ADesc *A, void *buf, long frames)
{   /* queue up some new data */
  if (A->debug > 2) Snack_WriteLog("    AUNewData\n");
  AUWaitNoData(A);
  A->data = buf;
  A->totalFrames = frames;
  dbpFill(A);
}

static long 
AUStart(ADesc *A, void *buf, long frames)
{   /* Have not actually started the double buffering yet - wait for
       both bufs to fill */
  if (A->debug > 2) Snack_WriteLog("    AUStart\n");
  /*ASSERT(A->data == NULL);*/  /* if we really haven't started, but there is
			       waiting data, AUNewData will block forever */
  AUNewData(A, buf, frames);
  if (A->bufFull[1]) {	    /* second buffer is full - OK to start */
    /* start the command */
#if defined(MAC_TCL)
    SndPlayDoubleBuffer(A->schn, (SndDoubleBufferHeaderPtr)&(A->dbh));
#else // MAC_OSX_TCL
    CarbonSndPlayDoubleBuffer(A->schn, (SndDoubleBufferHeaderPtr)&(A->dbh));
#endif
    A->running = 1;
  }
  return frames;  /* does not block */
}

/* Record double-buffer routines */

static void 
AUInBufStartNext(ADesc *A) {
  /* Start up the next input buffer */
  OSErr oe;
  int numbufs = NBUFS;
  int nextbuf = A->bufsIssued % numbufs;
  int async = 1;	/* recording has to be async */
  SPBPtr spb = A->spb[nextbuf];
  SndDoubleBufferPtr db = A->bufs[nextbuf];
  
  if (A->debug > 2) Snack_WriteLog("    AUInBufStartNext\n");
  
  /* If the next buf hasn't been emptied, it's an overrun */
  if (A->bufFull[nextbuf] != -1) {
    ++(A->underruns);
  }
  /* Mark the next buffer empty & pass it to the record proc */
  A->bufFull[nextbuf] = 0;
  spb->bufferPtr = (char *)db->dbSoundData;
  spb->count = spb->bufferLength;
  spb->milliseconds = 0;
  /* Maybe executing this at interrupt time is a mistake? Copies the last 8kB over again? */  
  if ( (oe = SPBRecord(spb, async)) != 0) {
    /*fprintf(stderr, "AUInBufStartNext: error %d from SPBRecord\n", oe);*/
  }
  ++ A->bufsIssued;
}

static pascal void
AUInBufComplete(SPBPtr spb) {
  /* Called to swap buffers by interrupt routine */
  ADesc *A = (ADesc *)spb->userLong;
  int numbufs = NBUFS;
  int thisbuf = (A->bufsCompleted)%numbufs;
  int bytesPerFrame = A->bytesPerSample * A->nChannels;
  SndDoubleBufferPtr db = A->bufs[thisbuf];
  
  /* Check we're where we think we are */
  /*ASSERT(spb == A->spb[thisbuf]);*/
  /* Mark this buffer full */
  A->bufFull[thisbuf] = 1;
  db->dbNumFrames = spb->count/bytesPerFrame;
  A->totalFrames += db->dbNumFrames;
  ++A->bufsCompleted;
}

static void
AUInBufStart(ADesc *A) {
  /* Set up and start the double-buffering input chain */
  int async = 1;
  SPBPtr spb;
  int i;
  
  if (A->debug > 2) Snack_WriteLog("    AUInBufStart\n");

  for (i=0; i<NBUFS; ++i) {
    spb = (SPB*)NewPtr(sizeof(SPB));
    /* Put it where AURead can find it to launch next cycle */
    A->spb[i] = spb;
    spb->inRefNum = A->inRefNum;
    spb->bufferLength = A->bytesPerSample * A->nChannels * A->bufFrames;
    spb->milliseconds = 0;
    spb->count = spb->bufferLength;
    /*spb->completionRoutine = NewSICompletionProc(AUInBufComplete);*/
    spb->completionRoutine = NewSICompletionUPP(AUInBufComplete);
    spb->interruptRoutine = NULL;
    spb->userLong = (long)(A);
    spb->error = 0;
    /* point to corresponding data region */
    spb->bufferPtr = (char *)(A->bufs[i])->dbSoundData;
  }
  /* Assume first buffer is ready to go */
  A->currentBuf = 0;
  A->bufsIssued = 0;    
  A->bufsCompleted = 0;    
  /* Start it up */
  A->running = 1;
  /* queue first buffer right away */
  AUInBufStartNext(A);
}

static void 
AURecIdle(ADesc * A) {
  /* Do something to pass control to OS while waiting for next snd interrupt */
  /*
    OSErr oe;
    short val;
    if ( (oe = SPBGetDeviceInfo(A->inRefNum, siHardwareBusy, &val)) != 0) {
        fprintf(stderr, "AURecIdle: error (%d) checking busy\n", oe);
    }
  */
#if defined(MAC_TCL)
  SystemTask();
#endif
}

int
SnackAudioOpen(ADesc *A, Tcl_Interp *interp, char *device, int mode, int freq,
	       int nchannels, int encoding)
{
  unsigned long fxSr = (unsigned long)(freq * (1L<<16));
  long CompressionFormat;
  int oerr = 0, i, CompressionID;

  /* 'pull in' to hardware supported sample rates to avoid poor
     interpolation (no 'frac' continuity across blocks) */
  if (42000 < freq && freq < 46000)	 fxSr = rate44khz;
  else if (21000 < freq && freq < 23000)  fxSr = rate22khz;
  else if (10500 < freq && freq < 11500)  fxSr = rate11khz;	
  
  if (A->debug > 1) Snack_WriteLogInt("  SnackAudioOpen, mode: ", (int)mode);
  if (A->debug > 2) Snack_WriteLogInt("    rate: ", (int)freq);
 
  A->nChannels = nchannels;

  CompressionFormat = 0L;
  CompressionID = 0;
  switch (encoding) {
  case LIN16:
    A->bytesPerSample = sizeof(short);
    break;
  case ALAW:
    A->bytesPerSample = sizeof(char);
    CompressionID = fixedCompression;
    CompressionFormat = kALawCompression;
    break;
  case MULAW:
    A->bytesPerSample = sizeof(char);
    CompressionID = fixedCompression;
    CompressionFormat = kULawCompression;
    break;
  case LIN8:
    A->bytesPerSample = sizeof(char);
    break;
  case LIN8OFFSET:
    A->bytesPerSample = sizeof(char);
    Tcl_AppendResult(interp, "Can not play sound format Lin8Offset", NULL);
    return TCL_ERROR;
    break;
  }

  A->mode = mode;
  
  if (mode == PLAY) {
    /* allocate double buffer handles */
    int desbuf, desbufx, dbbytes;
    
    desbufx = (int)(DFLT_BUFTIME * freq);	/* aim to buffer 1/8 of a sec each time */
    /* .. but round it up to a power of two */
    desbuf = 128;   /* smallest length we'll consider */ 
    while (desbuf < desbufx) {
      desbuf = 2*desbuf;
    } 
    dbbytes = sizeof(SndDoubleBuffer) + desbuf*A->bytesPerSample*nchannels;
    
    if (A->debug > 1) Snack_WriteLog("  SnackAudioOpen:play\n");
    if (A->debug > 2) Snack_WriteLogInt("    desbuf: ", desbuf);
    
    for(i = 0; i<NBUFS; ++i) {
      A->bufs[i] = (SndDoubleBuffer *)NewPtr(dbbytes);
      if (A->bufs[i] == NULL) {
	return -1;
      }
      A->bufs[i]->dbNumFrames = 0;
      A->bufs[i]->dbFlags = 0;
      A->bufs[i]->dbUserInfo[0] = (long)(A);
      A->dbh.dbhBufferPtr[i] = A->bufs[i];
      A->bufFull[i] = 0;    /* our underrun semaphores */
    }
    
    /* initialize sound double buffer header */
    A->dbh.dbhNumChannels = nchannels;
    A->dbh.dbhSampleSize  = 8 * A->bytesPerSample;
    A->dbh.dbhCompressionID = CompressionID;
    A->dbh.dbhPacketSize    = 0;
    A->dbh.dbhSampleRate    = fxSr;
#if defined(MAC_TCL)
    /*    A->dbh.dbhDoubleBack    = NewSndDoubleBackProc(DoubleBackProc);*/
    A->dbh.dbhDoubleBack    = NewSndDoubleBackUPP(DoubleBackProc);
#else // MAC_OSX_TCL
    A->dbh.dbhDoubleBack    = DoubleBackProc;
#endif
    A->dbh.dbhFormat		= CompressionFormat;

    /* setup the DB struct */
    A->data = NULL;
    A->totalFrames = 0;    /* how many frames there are */
    A->doneFrames  = 0;    /* how many we have already copied */
    A->bufFrames = desbuf;   /* number of frames allocated per buffer */

    A->currentBuf = 0;
    A->running = 0;
    A->pause = 0;

    /* debug stats */
    A->completedblocks = 0;
    A->underruns	= -1;	/* first call adds a spurious underrun pre-filling buffers */

    /* setup sound output hardware */
    A->schn = NULL;
    if (nchannels == 1)
      oerr = SndNewChannel(&(A->schn), sampledSynth, initMono, NULL);
    else /* opchans == 2 */
      oerr = SndNewChannel(&(A->schn), sampledSynth, initStereo, NULL);
    if (oerr) {
      return -1;
    }
  } else if (mode == RECORD) {
    char *devname = NULL;	/* how to ask for default input device */
    long inRefNum;
    unsigned short usbuf[32];
    short permission = siWritePermission;  /* i.e. permission to change states and get samples */
    long desbuf = 0, dbbytes;
    Handle tmpHan;
    unsigned long *ulp;
    unsigned long ul;
    int count, oe;
    int bits_per_samp;

    if (A->debug > 1) Snack_WriteLog("  SnackAudioOpen:rec\n");

    /* Open the default input device */
    oe = SPBOpenDevice((unsigned char *)devname, permission, &inRefNum);
    if (oe != 0) {
      return -1;
    }
    
    /* Set up requested sampling rate */
    /* First, read the available rates and choose the closest */
    if ( (oe = SPBGetDeviceInfo(inRefNum, siSampleRateAvailable, (void *)usbuf)) != 0) {
      SPBCloseDevice(inRefNum);
      return -1;
    }
    count = usbuf[0];
    ulp = (unsigned long *)(usbuf + 1);
    if (count == 0) {
      /* Continuously-variable sample rate; range in next two */
      double minsr = (ulp[0] / 65536.0);
      double maxsr = (ulp[1] / 65536.0);
      if (freq < minsr)   freq = minsr;
      else if (freq > maxsr) freq = maxsr;
    } else { 
      /* Count >= 1, so read them back */
      tmpHan = (Handle)((usbuf[1]<<16) + usbuf[2]);
      HLock(tmpHan);
      {
        double bestsr = (*(unsigned long **)tmpHan)[0]/65536.0;
        double bestdist = ABS(freq - bestsr);
        double newsr, newdist;
        for (i=1; i<count; ++i) {
	  newsr = (*(unsigned long **)tmpHan)[i] / 65536.0;
	  newdist = ABS(freq - newsr);
	  if (newdist < bestdist) {
	    bestdist = newdist;
	    bestsr = newsr;
	  }
        }
	freq = bestsr;
      }
      HUnlock(tmpHan);
      DisposeHandle(tmpHan);    
    }
    /* Now freq is set to a legal sample rate: set it */
    ul = (unsigned long)(65536.0*freq);
    if ( (oe = SPBSetDeviceInfo(inRefNum, siSampleRate, &ul)) != 0) {
      /* fprintf(stderr, "AUOpen_In: error (%d) setting SR (to 0x%lx / %f)\n", oe, ul, freq); */
      SPBCloseDevice(inRefNum);
      return -1;
    }
    
    /* Set up the number of channels */
    /* Scan available channel counts */
    if ( (oe = SPBGetDeviceInfo(inRefNum, siChannelAvailable, (void *)usbuf)) != 0) {
      /* fprintf(stderr, "AUOpen_In: error (%d) querying channel count\n", oe); */
      SPBCloseDevice(inRefNum);
      return -1;
    }
    /* How many do we actually want? */
    nchannels = min(nchannels, usbuf[0]);
    /* Set it up */
    usbuf[0] = nchannels;
    if ( (oe = SPBSetDeviceInfo(inRefNum, siNumberChannels, (void *)usbuf)) != 0) {
      /* fprintf(stderr, "AUOpen_In: error (%d) setting channels (%d)\n", oe, usbuf[0]); */
      SPBCloseDevice(inRefNum);
      return -1;
    }

    /* Set up the data format */
    /* Scan available data sizes */
    if ( (oe = SPBGetDeviceInfo(inRefNum, siSampleSizeAvailable, (void *)usbuf)) != 0) {
      /* fprintf(stderr, "AUOpen_In: error (%d) querying sample sizes\n", oe); */
      SPBCloseDevice(inRefNum);
      return -1;
    }
    /* what sample size do we want? */
    bits_per_samp = 8 * A->bytesPerSample;
    /* Search for best match */
    count = usbuf[0];
    tmpHan = (Handle)((usbuf[1]<<16) + usbuf[2]);
    HLock(tmpHan);
    {
      short bestsz = (*(short **)tmpHan)[0];
      short bestdist = ABS(bestsz - bits_per_samp);
      for (i=1; i<count; ++i) {
	int dist = ABS((*(short **)tmpHan)[i] - bits_per_samp);
	if (dist < bestdist) {
	  bestsz = (*(short **)tmpHan)[i];
	  bestdist = dist;
	}
      }
      if (bestsz != bits_per_samp) {
	/* fprintf(stderr, "AUOpen_In: Requested sample size %d served by %d\n", 
	   bits_per_samp, bestsz); */
	SPBCloseDevice(inRefNum);
	return -1;
      }
    }
    HUnlock(tmpHan);
    DisposeHandle(tmpHan);    
    /* Set it up */
    usbuf[0] = bits_per_samp;
    if ( (oe = SPBSetDeviceInfo(inRefNum, siSampleSize, (void *)usbuf)) != 0) {
      /* fprintf(stderr, "AUOpen_In: error (%d) setting sample size (%d)\n", oe, usbuf[0]); */
      SPBCloseDevice(inRefNum);
      return -1;
    }
    /* Don't forget to signal continuous recording */
    usbuf[0] = 1;
    if ( (oe = SPBSetDeviceInfo(inRefNum, siContinuous, (void *)usbuf)) != 0) {
      /* fprintf(stderr, "AUOpen_In: error (%d) setting continuos flag (%d)\n", oe, usbuf[0]);
	 SPBCloseDevice(inRefNum); */
      SPBCloseDevice(inRefNum);
      return -1;
    }
    /* select the requested source 
       usbuf[0] = jack+1;  // 1=MIC, 2=LINE; works on my 2400, but not portable
       if ( (oe = SPBSetDeviceInfo(inRefNum, siInputSource, (void *)usbuf)) != 0) {
       SPBCloseDevice(inRefNum);
       return -1;
       }
       */
    
    /* Set up the descriptor structure */
    A->nChannels = nchannels;
    A->inRefNum = inRefNum;
    A->completedblocks = 0;
    A->underruns = 0;
    A->running = 0;
    A->pause = 0;
    
    /* Set up double-buffers for asynchronous background */
    if (desbuf == 0) {
      desbuf = freq*DFLT_BUFTIME;
    }
    /* force desbuf to be nearest integral number of hardware buffers */
    if ( (oe = SPBGetDeviceInfo(inRefNum, siDeviceBufferInfo, (void *)usbuf)) != 0) {
      /* fprintf(stderr, "AUOpen_In: error (%d) getting buffer size (%d)\n", oe, usbuf[0]); */
      SPBCloseDevice(inRefNum);
      return -1;
    } else {
      long hbuflen = (usbuf[0] << 16L) + usbuf[1];
      /* divide down if hbuflen much larger */
      /*    while(hbuflen > desbuf) {
            hbuflen = hbuflen/2;
	    } */
      desbuf = hbuflen * max(1, (desbuf+hbuflen/2)/hbuflen);
    }
    if (A->debug > 2) Snack_WriteLogInt("    desbuf: ", desbuf);
    /* allocate double buffer handles */
    dbbytes = sizeof(SndDoubleBuffer) + desbuf*A->bytesPerSample*A->nChannels;
    for(i = 0; i<NBUFS; ++i) {
      A->bufs[i] = (SndDoubleBuffer *)NewPtr(dbbytes);
      if (A->bufs[i] == NULL) {
	SPBCloseDevice(inRefNum);
	return -1;
      }
      A->bufs[i]->dbNumFrames = 0;
      A->bufs[i]->dbFlags = 0;
      A->bufs[i]->dbUserInfo[0] = (long)(A);
      A->bufFull[i] = 0;
    }
    A->totalFrames = 0;    /* how many frames there are */
    A->doneFrames  = 0;    /* how many we have already copied */
    A->bufFrames = desbuf;   /* number of frames allocated per buffer */

  } else {
    /* fprintf(stderr, "Invalid mode\n"); */
    return TCL_ERROR;
  }

  return TCL_OK;
}

int
SnackAudioClose(ADesc *A)
{
  int i;
  
  switch (A->mode) {
  case RECORD:
    if (A->debug > 1) Snack_WriteLog("  SnackAudioClose:rec\n");
    
    A->running = 0;
    while (A->bufsCompleted < A->bufsIssued) {
      AURecIdle(A);
    }
    SPBCloseDevice(A->inRefNum);
    A->pause = 0;
    
    for (i = 0; i < NBUFS; ++i) {
      DisposeRoutineDescriptor(A->spb[i]->completionRoutine);
      DisposePtr((char *)A->spb[i]);
      A->spb[i] = 0;
      DisposePtr((char *)A->bufs[i]);
      A->bufs[i] = 0;
    }
    break;
    
  case PLAY:
    if (A->debug > 1) Snack_WriteLog("  SnackAudioClose:play\n");
    
    /* Return -1 if buffers haven't been used up */
    if (A->data != NULL) {
      MySndIdle(A->schn);
      return -1;
    }
    /* mark the last, partially-full buffer as the last */
    A->bufs[A->currentBuf]->dbFlags |= (dbBufferReady|dbLastBuffer);
    if (!A->running) {
      A->bufFull[0] = A->bufFull[1] = 1;   /* trick AUStart into starting */
      AUStart(A, NULL, 0);
    }
    /* Return -1 if data remains to be played */
    if(MySndIdle(A->schn)) return -1;
    A->running = 0;
    SndDisposeChannel(A->schn, FALSE); /* doesn't wait for commands to end */
    /* free all the data */
    for (i = 0; i < NBUFS; ++i) {
      DisposePtr((char *)A->bufs[i]);
    }
    break;
  }
  
  return(0);
}

int
SnackAudioPause(ADesc *A)
{
  if (A->debug > 1) Snack_WriteLog("  SnackAudioPause\n");
  
  if (!A->pause) {
    switch (A->mode) {
    case RECORD:
      break;
      
    case PLAY:
      A->scmd.cmd = pauseCmd_MacOS; /* See jkAudIO.h for explanations */
      A->scmd.param1 = 0;
      A->scmd.param2 = 0;
      SndDoImmediate(A->schn, &(A->scmd));
      break;
    }
    A->pause = 1;
  }
  
  return(-1);
}

void
SnackAudioResume(ADesc *A)
{
  if (A->debug > 1) Snack_WriteLog("  SnackAudioResume\n");
  
  if (A->pause) {
    switch (A->mode) {
    case RECORD:
      /* If we've been paused long enough to run out of buffers, 
	 start the next buffer recording */
      if (A->bufsIssued <= A->bufsCompleted + INBUF_OVERLAP) {
	AUInBufStartNext(A);
      }
      break;
      
    case PLAY:
      A->scmd.cmd = resumeCmd;
      A->scmd.param1 = 0;
      A->scmd.param2 = 0;
      SndDoImmediate(A->schn, &(A->scmd));
      break;
    }
    
    A->pause = 0;
  }
}

void
SnackAudioFlush(ADesc *A)
{
  if (A->debug > 1) Snack_WriteLog("  SnackAudioFlush\n");
  
  switch (A->mode) {
  case RECORD:      
    break;

  case PLAY:
    A->scmd.cmd = flushCmd_MacOS;
    A->scmd.param1 = 0;
    A->scmd.param2 = 0;
    SndDoImmediate(A->schn, &(A->scmd));

    A->scmd.cmd = quietCmd;
    A->scmd.param1 = 0;
    A->scmd.param2 = 0;
    SndDoImmediate(A->schn, &(A->scmd));
    break;
  }
}

void
SnackAudioPost(ADesc *A)
{
}

int
SnackAudioRead(ADesc *A, void *buf, int nFrames)
{
  int gotframes = 0;
  
  if (A->debug > 1) Snack_WriteLogInt("  SnackAudioRead, samps:", nFrames); 
  if (A->debug > 2) Snack_WriteLogInt("    curbuf: ", A->currentBuf); 
  if (A->debug > 2) Snack_WriteLogInt("    cmpbuf: ", A->bufsCompleted); 
  if (A->debug > 2) Snack_WriteLogInt("    issbuf: ", A->bufsIssued);
  
  /* Start recording if necessary */
  if (A->running == 0 && A->pause == 0) {
    AUInBufStart(A);
  }
  /* Grab buffers until satisfied */
  {
    int numbufs = NBUFS;
    int curbuf = A->currentBuf % numbufs;
    int remain = nFrames;
    int thistime;
    int async = 1;
    SndDoubleBufferPtr db = A->bufs[curbuf];
    int bytesPerFrame = A->bytesPerSample*A->nChannels;
    
    /* We may be draining after a pause.  In that case, eventually the current 
       buffer will be advanced to an unissued one.  If so, A->bufFull[curbuf] 
       will == -1; note that this implies pause = 1. */
    while (remain > 0 && A->bufFull[curbuf] != -1) {
      
      if (A->debug > 2) Snack_WriteLogInt("    dbnumf: ", db->dbNumFrames);
      
      /* if current buffer is filling, block until it is full */
      while (A->bufFull[curbuf] == 0) {
	/* fprintf(stderr, "blocking/ status=%d,%d\n", au->bufFull[0], au->bufFull[1]); */
	AURecIdle(A);
      }
      if (!A->pause) {
	/* check that we're not waiting on an unqueued buffer */
	/*ASSERT(A->bufFull[curbuf] == 1);*/
	/* maybe issue the next buffer (overlap of INBUF_OVERLAP (=0) calls) */
	if (A->bufsIssued <= A->bufsCompleted + INBUF_OVERLAP) {
	  AUInBufStartNext(A);
	}
      }
      
      /* transfer some bytes */
      thistime = min(remain, db->dbNumFrames);
      BlockMove((const char*)db->dbSoundData 
		+ (A->bufFrames - db->dbNumFrames)*bytesPerFrame, 
		((char *)buf) + gotframes*bytesPerFrame, 
		thistime * bytesPerFrame);
      remain -= thistime;
      db->dbNumFrames -= thistime;
      gotframes += thistime;
      if (db->dbNumFrames == 0) {
	/* Mark as ready for reuse */
	A->bufFull[curbuf] = -1;
	/* start looking at the other buf */
	++A->currentBuf;
	curbuf = A->currentBuf % numbufs;
	db = A->bufs[curbuf];
      }
    }
  }
  
  if (gotframes != nFrames) {
    /* fprintf(stdout, "AURead: req %d frames, got %d\n", frames, gotframes); */
  }
  
  A->doneFrames += gotframes;
  
  if (A->debug > 1) {
    Snack_WriteLogInt("  ARead got: ", gotframes * A->nChannels);
  }
  
  return gotframes * A->nChannels;
}

static int block = 0;

int
SnackAudioWrite(ADesc *A, void *buf, int nFrames)
{
  if (A->debug > 1) Snack_WriteLog("  Enter SnackAudioWrite\n");

  if (block) return 0;
  if (!A->running) {
    AUStart(A, buf, nFrames);
  } else {
    AUNewData(A, buf, nFrames);
  }

  block = 1;

  if (A->debug > 1) Snack_WriteLogInt("  Exit SnackAudioWrite", nFrames);
  return nFrames;
}

int
SnackAudioReadable(ADesc *A)
{
  int numbufs = NBUFS;
  int curbuf = A->currentBuf % numbufs;
  int nextbuf = A->bufsIssued % numbufs;
  SndDoubleBufferPtr db = A->bufs[curbuf];
  SPBPtr spb = A->spb[nextbuf];
  int bytesPerBuffer = spb->bufferLength;
  int bytesPerFrame = A->bytesPerSample*A->nChannels;
  int nsamps = (bytesPerFrame * db->dbNumFrames \
		+ bytesPerBuffer * (A->bufsIssued - A->bufsCompleted))/
    A->bytesPerSample;
  
  static ncalls = 0;
  
  if (A->debug > 1) {
    Snack_WriteLogInt("  SnackAudioReadable, curbuf: ", A->currentBuf);
  }
  if (A->debug > 2) Snack_WriteLogInt("    dbNumF: ", db->dbNumFrames);
  if (A->debug > 2) Snack_WriteLogInt("    smpslft: ", nsamps);
  
  ++ncalls;
  if (ncalls > 1000) {
    nsamps = 0;
  }
  
  return nsamps;
}

int
SnackAudioWriteable(ADesc *A)
{
  if (block) {
    if (A->data && A->doneFrames < A->totalFrames) {
      MySndIdle(A->schn);
      return 0;
    }
  }
  block = 0;
  
  return -1;
}

int
SnackAudioPlayed(ADesc *A)
{
  return A->completedblocks * A->bufFrames * A->nChannels;
}

void
SnackAudioInit()
{
}

void
SnackAudioFree()
{
  int i, j;

  for (i = 0; i < SNACK_NUMBER_MIXERS; i++) {
    for (j = 0; j < 2; j++) {
      if (mixerLinks[i][j].mixer != NULL) {
	ckfree(mixerLinks[i][j].mixer);
      }
      if (mixerLinks[i][j].mixerVar != NULL) {
	ckfree(mixerLinks[i][j].mixerVar);
      }
    }
    if (mixerLinks[i][0].jack != NULL) {
      ckfree(mixerLinks[i][0].jack);
    }
    if (mixerLinks[i][0].jackVar != NULL) {
      ckfree((char *)mixerLinks[i][0].jackVar);
    }
  }
}

void
ASetRecGain(int gain)
{
  int g = min(max(gain, 0), 100);
}

void
ASetPlayGain(int gain)
{
  int g = min(max(gain, 0), 100);
}

int
AGetRecGain()
{
  int g = 0;

  return(g);
}

int
AGetPlayGain()
{
  int g = 0;

  return(g);
}

int
SnackAudioGetEncodings(char *device)
{
  OSErr oe;
  char *devname = NULL;	/* how to ask for default input device */
  long inRefNum;
  short permission = siReadPermission;  /* only want to read status, not change state */
  int lin24 = 0;
  
  /* Open the default input device */
  oe = SPBOpenDevice((unsigned char *)devname, permission, &inRefNum);
  if (oe == 0) {
    /* managed to open it */
    unsigned short sbuf[32];
    
    /* Scan available sample sizes */
    oe = SPBGetDeviceInfo(inRefNum, siSampleSizeAvailable, (void *)sbuf);
    if (oe == 0) {
      int count = sbuf[0];
      short *sp;
      int i;
      Handle tmpHan = (Handle)((sbuf[1]<<16) + sbuf[2]);
      
      HLock(tmpHan);
      sp = *(short **)tmpHan;
      
      for (i=0; i<count; ++i) {
	if ((int)sp[i] == 24) {
	  lin24 = 1;
	}
      }
      HUnlock(tmpHan);
      DisposeHandle(tmpHan);
    }
    SPBCloseDevice(inRefNum);
  }
  if (lin24) {
    return(LIN24 | LIN16);
  } else {
    return(LIN16);
  }
}

void
SnackAudioGetRates(char *device, char *buf, int n)
{/*
  OSErr oe;
  char *devname = NULL;	*//* how to ask for default input device *//*
  long inRefNum;
  short permission = siReadPermission;  *//* only want to read status, not change state */
  
  /* default to empty list *//*
  buf[0] = '\0';
  
   *//* Open the default input device *//*
  oe = SPBOpenDevice((unsigned char *)devname, permission, &inRefNum);
  if (oe == 0) {
   *//* managed to open it *//*
    unsigned short usbuf[32];
    
     *//* Scan available sample rates *//*
    oe = SPBGetDeviceInfo(inRefNum, siSampleRateAvailable, (void *)usbuf);
    if (oe == 0) {
      int count = usbuf[0];
      Handle tmpHan = (Handle)((usbuf[1]<<16) + usbuf[2]);
      short *sp;
      unsigned long *ulp;
      int i;
      
      HLock(tmpHan);
      sp = *(short **)tmpHan;
      ulp = *(unsigned long **)tmpHan;
      
      for (i=0; i<count; ++i) {
	if (strlen(buf)) strcat(buf, " ");
	sprintf(buf+strlen(buf), "%d", (int)(ulp[i]/65536.0));
      }
      HUnlock(tmpHan);            
      DisposeHandle(tmpHan);
    }
    SPBCloseDevice(inRefNum);
  } 
	*/
  strncpy(buf, "8000 11025 16000 22050 32000 44100 48000", n);
  buf[n-1] = '\0';
}

int
SnackAudioMaxNumberChannels(char *device)
{
  return(2);
}

int
SnackAudioMinNumberChannels(char *device)
{
  return(1);
}

void
SnackMixerGetInputJackLabels(char *buf, int n)
{
  OSErr oe;
  char *devname = NULL;	/* how to ask for default input device */
  long inRefNum;
  short permission = siReadPermission;  /* only want to read status, not change state */
  
  /* default to empty list */
  buf[0] = '\0';
  
  /* Open the default input device */
  oe = SPBOpenDevice((unsigned char *)devname, permission, &inRefNum);
  if (oe == 0) {
    /* managed to open it */
    GetSndInputNames(inRefNum, 0, buf);
    SPBCloseDevice(inRefNum);
  }
}

void
SnackMixerGetOutputJackLabels(char *buf, int n)
{
  strcpy(buf, "Speaker");
}

void
SnackMixerGetInputJack(char *buf, int n)
{
  OSErr oe;
  char *devname = NULL;	/* how to ask for default input device */
  long inRefNum;
  short permission = siReadPermission;  /* only want to read status, not change state */
  int devnum;
  unsigned short sbuf[32];
  
  /* default to empty list */
  buf[0] = '\0';
  
  /* Open the default input device */
  oe = SPBOpenDevice((unsigned char *)devname, permission, &inRefNum);
  if (oe == 0) {
    /* managed to open it */
    
    /* Which source are we using? */
    oe = SPBGetDeviceInfo(inRefNum, siInputSource, (void *)sbuf);
    /* fprintf(stderr, "siInputSource: rc=%d val=%d\n", oe, buf[0]); */
    if (oe == 0) {
      devnum = sbuf[0];
      GetSndInputNames(inRefNum, devnum, buf);
    }
    SPBCloseDevice(inRefNum);
  }
}

int
SnackMixerSetInputJack(Tcl_Interp *interp, char *jack, CONST84 char *status)
{
  OSErr oe;
  char *devname = NULL;	/* how to ask for default input device */
  long inRefNum;
  short permission = siWritePermission;  /* want to change state */
  unsigned short sbuf[32];
  
  /* Open the default input device */
  oe = SPBOpenDevice((unsigned char *)devname, permission, &inRefNum);
  if (oe == 0) {
    /* select the default source */
    sbuf[0] = (short)*jack+1;  /* 1=MIC, 2=LINE; works on my 2400, but not portable */
    oe = SPBSetDeviceInfo(inRefNum, siInputSource, (void *)sbuf);
    
    SPBCloseDevice(inRefNum);
  }
  return 0;
}

void
SnackMixerGetOutputJack(char *buf, int n)
{
  strcpy(buf, "Speaker");
}

void
SnackMixerSetOutputJack(char *jack, char *status)
{
}

void
SnackMixerGetChannelLabels(char *mixer, char *buf, int n)
{
  strncpy(buf, "Mono", n);
  buf[n-1] = '\0';
}

void
SnackMixerGetVolume(char *line, int channel, char *buf, int n)
{
  if (strncasecmp(line, "Play", strlen(line)) == 0) {
    sprintf(buf, "%d", AGetPlayGain());
  } 
}

void
SnackMixerSetVolume(char *line, int channel, int volume)
{
  if (strncasecmp(line, "Play", strlen(line)) == 0) {
    ASetPlayGain(volume);
  } 
}

void
SnackMixerLinkJacks(Tcl_Interp *interp, char *jack, Tcl_Obj *var)
{
}

static char *
VolumeVarProc(ClientData clientData, Tcl_Interp *interp, CONST84 char *name1,
	      CONST84 char *name2, int flags)
{
  MixerLink *mixLink = (MixerLink *) clientData;
  CONST84 char *stringValue;
  
  if (flags & TCL_TRACE_UNSETS) {
    if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
      Tcl_Obj *obj, *var;
      char tmp[VOLBUFSIZE];

      SnackMixerGetVolume(mixLink->mixer, mixLink->channel, tmp, VOLBUFSIZE);
      obj = Tcl_NewIntObj(atoi(tmp));
      var = Tcl_NewStringObj(mixLink->mixerVar, -1);
      Tcl_ObjSetVar2(interp, var, NULL, obj, TCL_GLOBAL_ONLY | TCL_PARSE_PART1);
      Tcl_TraceVar(interp, mixLink->mixerVar,
		   TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		   VolumeVarProc, (int *)mixLink);
    }
    return (char *) NULL;
  }
  stringValue = Tcl_GetVar(interp, mixLink->mixerVar, TCL_GLOBAL_ONLY);
  if (stringValue != NULL) {
    SnackMixerSetVolume(mixLink->mixer, mixLink->channel, atoi(stringValue));
  }

  return (char *) NULL;
}

void
SnackMixerLinkVolume(Tcl_Interp *interp, char *line, int n,
		     Tcl_Obj *CONST objv[])
{
  char *mixLabels[] = { "Play" };
  int i, j, channel;
  CONST84 char *value;
  char tmp[VOLBUFSIZE];

  for (i = 0; i < SNACK_NUMBER_MIXERS; i++) {
    if (strncasecmp(line, mixLabels[i], strlen(line)) == 0) {
      for (j = 0; j < n; j++) {
	if (n == 1) {
	  channel = -1;
	} else {
	  channel = j;
	}
	mixerLinks[i][j].mixer = (char *)SnackStrDup(line);
	mixerLinks[i][j].mixerVar = (char *)SnackStrDup(Tcl_GetStringFromObj(objv[j+3], NULL));
	mixerLinks[i][j].channel = j;
	value = Tcl_GetVar(interp, mixerLinks[i][j].mixerVar, TCL_GLOBAL_ONLY);
	if (value != NULL) {
	  SnackMixerSetVolume(line, channel, atoi(value));
	} else {
	  Tcl_Obj *obj;
	  SnackMixerGetVolume(line, channel, tmp, VOLBUFSIZE);
	  obj = Tcl_NewIntObj(atoi(tmp));
	  Tcl_ObjSetVar2(interp, objv[j+3], NULL, obj, 
			 TCL_GLOBAL_ONLY | TCL_PARSE_PART1);
	}
	Tcl_TraceVar(interp, mixerLinks[i][j].mixerVar,
		     TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		     VolumeVarProc, (ClientData) &mixerLinks[i][j]);
      }
    }
  }
}

void
SnackMixerUpdateVars(Tcl_Interp *interp)
{
  int i, j;
  char tmp[VOLBUFSIZE];
  Tcl_Obj *obj, *var;

  for (i = 0; i < SNACK_NUMBER_MIXERS; i++) {
    for (j = 0; j < 2; j++) {
      if (mixerLinks[i][j].mixerVar != NULL) {
	SnackMixerGetVolume(mixerLinks[i][j].mixer, mixerLinks[i][j].channel,
			    tmp, VOLBUFSIZE);
	obj = Tcl_NewIntObj(atoi(tmp));
	var = Tcl_NewStringObj(mixerLinks[i][j].mixerVar, -1);
	Tcl_ObjSetVar2(interp, var, NULL, obj, TCL_GLOBAL_ONLY|TCL_PARSE_PART1);
      }
    }
  }
}

void
SnackMixerGetLineLabels(char *buf, int n)
{
  strncpy(buf, "Play", n);
  buf[n-1] = '\0';
}

int
SnackGetOutputDevices(char **arr, int n)
{
  arr[0] = (char *) SnackStrDup("default");

  return 1;
}

int
SnackGetInputDevices(char **arr, int n)
{
  arr[0] = (char *) SnackStrDup("default");

  return 1;
}

int
SnackGetMixerDevices(char **arr, int n)
{
  arr[0] = (char *) SnackStrDup("default");

  return 1;
}
