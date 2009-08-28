/* 
 * Copyright (C) 2003-2004 Kare Sjolander <kare@speech.kth.se>
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
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <CoreServices/CoreServices.h>
#include <CoreAudio/AudioHardware.h>

extern void Snack_WriteLog(char *s);
extern void Snack_WriteLogInt(char *s, int n);

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define SNACK_NUMBER_MIXERS 1

struct MixerLink mixerLinks[SNACK_NUMBER_MIXERS][2];

#define BUFLEN (44100*2)
static short otmp[BUFLEN];
static float itmp[BUFLEN];
static int usageCount = 0;
static float rate;
static ADesc *AO = NULL;
static ADesc *AI = NULL;

OSStatus
appIOProc(AudioDeviceID inDevice, const AudioTimeStamp* inNow,
	 const AudioBufferList* inInputData, const AudioTimeStamp* inInputTime,
	  AudioBufferList* outOutputData, const AudioTimeStamp* inOutputTime, 
	  void* adesc)
{    
  ADesc *A = adesc;
  int i;
  int numFrames = A->deviceBufferSize / A->deviceFormat.mBytesPerFrame;
  float *out = outOutputData->mBuffers[0].mData;
  float *in  = inInputData->mBuffers[0].mData;

  /*  printf("w %d r %d  %d %d\n", A->wpos, A->rpos, numFrames*2, A->nChannels);*/
  if (AO != NULL && AO->mode == PLAY) {
    for (i = 0; i < numFrames*A->nChannels; ++i) {
      *out++ = (float) (otmp[(A->rpos*A->nChannels + i) % BUFLEN] / 32768.0);
    if (A->nChannels == 1) {
      *out++ = (float) (otmp[(A->rpos*A->nChannels + i) % BUFLEN] / 32768.0);
    }
    }
    A->rpos = (A->rpos + numFrames) % (BUFLEN/A->nChannels);
  }
  /*  printf("appIOProc wpos %d   %d %d\n",
      A->wpos,numFrames,inInputData->mNumberBuffers);*/
  if (AI != NULL && AI->mode == RECORD) {
    if (A->wpos + numFrames < BUFLEN/2) {
      memcpy(&itmp[A->wpos*2], in, numFrames*2*sizeof(float));
      A->wpos += numFrames;
    } else {
      memcpy(&itmp[A->wpos*2], in, (BUFLEN/2 - A->wpos)*2*sizeof(float));
      memcpy(itmp, &in[(BUFLEN/2 - A->wpos)*2],
	     (numFrames-(BUFLEN/2-A->wpos))*2*sizeof(float));
      A->wpos = (A->wpos + numFrames) % (BUFLEN/2);
    }
    A->tot += numFrames;
  }

  return (kAudioHardwareNoError);     
}

int
SnackAudioOpen(ADesc *A, Tcl_Interp *interp, char *device, int mode, int freq,
	       int nchannels, int encoding)
{
  OSStatus err = kAudioHardwareNoError;
  UInt32 count = sizeof(AudioDeviceID);
  UInt32 bufferSize;
  AudioStreamBasicDescription format;

  if (mode == PLAY) {
    AO = A;
    A->time = SnackCurrentTime();
    A->wpos = 0;
    A->rpos = 0;
    A->tot = 0;
  } else {
    AI = A;
    A->rpos = 0;
    A->wpos = 0;
    A->tot = 0;
  }
  if (usageCount == 1) {
    usageCount = 2;
    return TCL_OK;
  }

  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
				 &count, (void *) &A->device);

  count = sizeof(bufferSize);
  err = AudioDeviceGetProperty(A->device, 0, false,
			       kAudioDevicePropertyBufferSize,
			       &count, &bufferSize);

  count = sizeof(format);
  err = AudioDeviceGetProperty(A->device, 0, false,
			       kAudioDevicePropertyStreamFormat,
			       &count, &format);

  if ((err = AudioDeviceAddIOProc(A->device, appIOProc, (void *)A))
      != -kAudioHardwareNoError) {
    Tcl_AppendResult(interp, "AudioDeviceAddIOProc failed", NULL);
    return TCL_ERROR;
  }
  
  if ((err = AudioDeviceStart(A->device, appIOProc))
      != -kAudioHardwareNoError) {
    Tcl_AppendResult(interp, "AudioDeviceStart failed", NULL);
    return TCL_ERROR;
  }

  A->deviceBufferSize = bufferSize;
  A->deviceFormat.mBytesPerFrame = format.mBytesPerFrame;
  A->nChannels = nchannels;
  A->mode = mode;
  A->encoding = encoding;
  rate = (float) freq;
  usageCount = 1;

  switch (encoding) {
  case LIN24:
    A->bytesPerSample = sizeof(int);
    break;
  case LIN16:
    A->bytesPerSample = sizeof(short);
    break;
  }

  return TCL_OK;
}

int
SnackAudioClose(ADesc *A)
{
  OSStatus err = kAudioHardwareNoError;

  if (A->mode == PLAY) {
    AO = NULL;
  } else {
    AI = NULL;
  }
  if (usageCount == 2) {
    usageCount = 1;
    return(0);
  }
  usageCount = 0;

  /*  printf("SnackAudioClose\n");*/
    
  if ((err = AudioDeviceStop(A->device, appIOProc))
      != -kAudioHardwareNoError) {
    /*    printf("AudioDeviceStop failed\n");*/
    return TCL_ERROR;
  }
  
  if ((err = AudioDeviceRemoveIOProc(A->device, appIOProc))
      != -kAudioHardwareNoError) {
    /*    printf("AudioDeviceRemoveIOProc failed\n");*/
    return TCL_ERROR;
  }
  
  return(0);
}

long
SnackAudioPause(ADesc *A)
{
  return(-1);
}

void
SnackAudioResume(ADesc *A)
{
}

void
SnackAudioFlush(ADesc *A)
{
}

void
SnackAudioPost(ADesc *A)
{
  if (A->mode == PLAY) {
    int i;
    
    for (i = A->wpos*A->nChannels; i < BUFLEN; i++) otmp[i] = 0;
    for (i = 0; i < A->rpos*A->nChannels; i++) otmp[i] = 0;
  }
}

int
SnackAudioRead(ADesc *A, void *buf, int nFrames)
{
  int i, c;
  float frac = rate / 44100.0f;
  int tot = (int) (frac * A->tot);

  /*  printf("SnackAudioRead frames %d rpos %d tot %d\n", nFrames, A->rpos, A->tot);*/

  if (nFrames > tot) {
    nFrames = tot;
  }
  for (c = 0; c < A->nChannels; c++) {
    for (i = 0; i < nFrames; i++) {
      int ij, pos;
      float smp1 = 0.0, smp2, f, dj;
      
      dj = i / frac; 
      ij = (int) dj;
      f = dj - ij;
      pos = ij * 2 + c;
      switch (A->encoding) {
      case LIN24:
      case LIN24PACKED:
	smp1 = (8388607.0*itmp[(A->rpos*2 + pos) % (BUFLEN)]);
	smp2 = (8388607.0*itmp[(A->rpos*2 + pos + A->nChannels)%(BUFLEN)]);
	((int *)buf)[i * A->nChannels + c] = smp1 * (1.0f - f) + smp2 * f;
	break;
      case LIN32:
      case SNACK_FLOAT:
	smp1 = (2147483647.0*itmp[(A->rpos*2 + pos) % (BUFLEN)]);
	smp2 = (2147483647.0*itmp[(A->rpos*2 + pos + A->nChannels)%(BUFLEN)]);
	((int *)buf)[i * A->nChannels + c] = smp1 * (1.0f - f) + smp2 * f;
	break;
      case LIN16:
      case MULAW:
      case ALAW:
	smp1 = (short) (32767.0*itmp[(A->rpos*2 + pos) % (BUFLEN)]);
	smp2 = (short) (32767.0*itmp[(A->rpos*2 + pos + A->nChannels)%(BUFLEN)]);
	((short *)buf)[i * A->nChannels + c] = smp1 * (1.0f - f) + smp2 * f;
	break;
      }
    }
  }
  A->rpos = (A->rpos + (int)(nFrames/frac)) % (BUFLEN/2);
  A->tot -= (int) (nFrames/frac);
  
  return(nFrames);
}

int
SnackAudioWrite(ADesc *A, void *buf, int nFrames)
{
  /*  printf("SnackAudioWrite %d frames %d (%d %d)\n", A->wpos, nFrames,nFrames*4,&otmp[A->wpos*2]);*/

  if (A->wpos + nFrames < BUFLEN/A->nChannels) {
    memcpy(&otmp[A->wpos*A->nChannels], buf, nFrames*A->nChannels*2);
    A->wpos += nFrames;
  } else {
    memcpy(&otmp[A->wpos*A->nChannels], buf, (BUFLEN/A->nChannels - A->wpos)*A->nChannels*2);
    memcpy(otmp, &((short *)buf)[(BUFLEN/A->nChannels - A->wpos)*A->nChannels],(nFrames-(BUFLEN/A->nChannels-A->wpos))*A->nChannels*2);
    A->wpos = (A->wpos + nFrames) % (BUFLEN/A->nChannels);
  }
   
  return(nFrames);
}

int
SnackAudioReadable(ADesc *A)
{
  return((int) (A->tot * rate / 44100.0f));
}

int
SnackAudioWriteable(ADesc *A)
{
  return -1;
}

long
SnackAudioPlayed(ADesc *A)
{
  long res;

  res = (int) (44100 * (SnackCurrentTime() - A->time) +.5);

  /*  printf("SnackAudioPlayed %d\n", res);*/
  
  return(res);
}

void
SnackAudioInit()
{
  /*
  OSStatus  err = noErr;
  UInt32 count, bufferSize;
  AudioDeviceID	device = kAudioDeviceUnknown;
  AudioStreamBasicDescription format;
  
  count = sizeof(device);
  err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
				 &count, (void *) &device);
  fprintf(stderr, "kAudioHardwarePropertyDefaultOutputDevice %d\n", err);
  if (err != noErr) goto Bail;
  
  count = sizeof(bufferSize);
  err = AudioDeviceGetProperty(device, 0, false,
			       kAudioDevicePropertyBufferSize,
			       &count, &bufferSize);
  fprintf(stderr, "kAudioDevicePropertyBufferSize %d %d\n", err, bufferSize);
  if (err != noErr) goto Bail;
  
  count = sizeof(format);
  err = AudioDeviceGetProperty(device, 0, false,
			       kAudioDevicePropertyStreamFormat,
			       &count, &format);
  fprintf(stderr, "kAudioDevicePropertyStreamFormat %d\n", err);
  fprintf(stderr, "sampleRate %g\n", format.mSampleRate);
  fprintf(stderr, "mFormatFlags %08X\n", format.mFormatFlags);
  fprintf(stderr, "mBytesPerPacket %d\n", format.mBytesPerPacket);
  fprintf(stderr, "mFramesPerPacket %d\n", format.mFramesPerPacket);
  fprintf(stderr, "mChannelsPerFrame %d\n", format.mChannelsPerFrame);
  fprintf(stderr, "mBytesPerFrame %d\n", format.mBytesPerFrame);
  fprintf(stderr, "mBitsPerChannel %d\n", format.mBitsPerChannel);
  fprintf(stderr, "mFormatID !=  %d %d %d\n", format.mFormatID != kAudioFormatLinearPCM, format.mFormatID, kAudioFormatLinearPCM);
  if (err != kAudioHardwareNoError) goto Bail;*/
  /*  FailWithAction(format.mFormatID != kAudioFormatLinearPCM, err = paramErr, Bail);*/
  /*
  memset(&format, 0, sizeof(AudioStreamBasicDescription));
  format.mSampleRate = 44100.0;
  err = AudioDeviceSetProperty(device, 0, 0, 0,
			       kAudioDevicePropertyStreamFormat,
			       sizeof(format), &format);
  fprintf(stderr, "kAudioDevicePropertyStreamFormat %d\n", err);

  memset(&format, 0, sizeof(AudioStreamBasicDescription));
  format.mChannelsPerFrame = 2;
  err = AudioDeviceSetProperty(device, 0, 0, 0,
			       kAudioDevicePropertyStreamFormat,
			       sizeof(format), &format);
  fprintf(stderr, "kAudioDevicePropertyStreamFormat %d\n", err);
  */
  /*  
 Bail:
 fprintf(stderr, "done\n");*/
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
  return(LIN16);
}

void
SnackAudioGetRates(char *device, char *buf, int n)
{
  strncpy(buf, "8000 11025 16000 22050 32000 44100 48000 96000", n);
  buf[n-1] = '\0';
}

int
SnackAudioMaxNumberChannels(char *device)
{
  return(64);
}

int
SnackAudioMinNumberChannels(char *device)
{
  return(1);
}

void
SnackMixerGetInputJackLabels(char *buf, int n)
{
  buf[0] = '\0';
}

void
SnackMixerGetOutputJackLabels(char *buf, int n)
{
  buf[0] = '\0';
}

void
SnackMixerGetInputJack(char *buf, int n)
{
  buf[0] = '\0';
}

int
SnackMixerSetInputJack(Tcl_Interp *interp, char *jack, CONST84 char *status)
{
  return 1;
}

void
SnackMixerGetOutputJack(char *buf, int n)
{
  buf[0] = '\0';
}

void
SnackMixerSetOutputJack(char *jack, char *status)
{
}

void
SnackMixerGetChannelLabels(char *line, char *buf, int n)
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
