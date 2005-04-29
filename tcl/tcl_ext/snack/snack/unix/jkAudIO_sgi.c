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

#include "tcl.h"
#include "jkAudIO.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

extern void Snack_WriteLog(char *s);
extern void Snack_WriteLogInt(char *s, int n);

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define SNACK_NUMBER_MIXERS 2

struct MixerLink mixerLinks[SNACK_NUMBER_MIXERS][2];

#define OLD_AL

int
SnackAudioOpen(ADesc *A, Tcl_Interp *interp, char *device, int mode, int freq,
	       int nchannels, int encoding)
{
#ifdef OLD_AL
#  define alSetWidth(A,B)       ALsetwidth(A,B) 
#  define alSetChannels(A,B)    ALsetchannels(A,B) 
#  define alGetFrameNumber(A,B) ALgetframenumber(A, (unsigned long long *)B) 
#  define alGetFilled(A)        ALgetfilled(A)
#  define alClosePort(A)        ALcloseport(A)
#  define alFreeConfig(A)       ALfreeconfig(A)
#  define alReadFrames(A,B,C)   ALreadsamps(A,B,C) 
#  define alWriteFrames(A,B,C)  ALwritesamps(A,B,C)
#  define alSetFillPoint(A,B)   ALsetfillpoint(A,B)
#  define alGetFillable(A)      ALgetfillable(A)

  typedef long long stamp_t;

  long buf[2];
  A->config = ALnewconfig();
#else 
  ALpv pv[2];
  A->config = alNewConfig();
#endif /* OLD_AL */

  switch (encoding) {
  case LIN16:
    if (alSetWidth(A->config, AL_SAMPLE_16) == -1) {
      Tcl_AppendResult(interp, "Failed alSetWidth: AL_SAMPLE_16.", NULL);
      return TCL_ERROR;
    }
    A->bytesPerSample = sizeof(short);
    break;
  case ALAW:
    A->bytesPerSample = sizeof(char);
    Tcl_AppendResult(interp, "Unsupported format ALAW.", NULL);
    return TCL_ERROR;
  case MULAW:
    A->bytesPerSample = sizeof(char);
    Tcl_AppendResult(interp, "Unsupported format MULAW.", NULL);
    return TCL_ERROR;
  case LIN8OFFSET:
    A->bytesPerSample = sizeof(char);
    Tcl_AppendResult(interp, "Unsupported format LIN8OFFSET.", NULL);
    return TCL_ERROR;
  case LIN8:
    if (alSetWidth(A->config, AL_SAMPLE_8) == -1) {
      Tcl_AppendResult(interp, "Failed alSetWidth: AL_SAMPLE_8.", NULL);
      return TCL_ERROR;
    }
    A->bytesPerSample = sizeof(char);
    break;
  }
  
  if (alSetChannels(A->config, nchannels) == -1) {
    Tcl_AppendResult(interp, "Failed alSetChannels.", NULL);
    return TCL_ERROR;
  }
  A->nChannels = nchannels;
  
  A->mode = mode;
  switch (mode) {
  case RECORD:

#ifdef OLD_AL
    buf[0] = AL_INPUT_RATE;
    buf[1] = freq;
    if (ALsetparams(AL_DEFAULT_DEVICE, buf, 2) == -1) {
      Tcl_AppendResult(interp, "Failed ALsetparams.", NULL);
      return TCL_ERROR;
    }
    if ((A->port = ALopenport("snack-in", "r", A->config)) == NULL) {
      Tcl_AppendResult(interp, "Failed ALopenport.", NULL);
      return TCL_ERROR;
    }
#else
    pv[0].param = AL_MASTER_CLOCK;
    pv[0].value.i = AL_CRYSTAL_MCLK_TYPE;
    pv[1].param = AL_RATE;
    pv[1].value.ll = alDoubleToFixed((double)(freq * 1.0));
    if (alSetParams(AL_DEFAULT_INPUT, pv, 2) == -1) {
      printf("Error: %s\n", alGetErrorString(oserror()));
      if (pv[0].sizeOut < 0) printf("AL_MASTER_CLOCK\n");
      if (pv[1].sizeOut < 0) printf("Rate invalid\n");
      Tcl_AppendResult(interp, "Failed alSetParams.", NULL);
      return TCL_ERROR;
    }
    if ((A->port = alOpenPort("snack-in", "r", A->config)) == NULL) {
      Tcl_AppendResult(interp, "Failed ALopenport.", NULL);
      return TCL_ERROR;
    }
#endif /* OLD_AL */
    break;

  case PLAY:
#ifdef OLD_AL
    buf[0] = AL_OUTPUT_RATE;
    buf[1] = freq;
    if (ALsetparams(AL_DEFAULT_DEVICE, buf, 2) == -1) {
      Tcl_AppendResult(interp, "Failed ALsetparams.", NULL);
      return TCL_ERROR;
    }
    if ((A->port = ALopenport("snack-out", "w", A->config)) ==NULL) {
      Tcl_AppendResult(interp, "Failed ALopenport.", NULL);
      return TCL_ERROR;
    }
#else
    pv[0].param = AL_MASTER_CLOCK;
    pv[0].value.i = AL_CRYSTAL_MCLK_TYPE;
    pv[1].param = AL_RATE;
    pv[1].value.ll = alDoubleToFixed((double)(freq * 1.0));
    if (alSetParams(AL_DEFAULT_OUTPUT, pv, 1) == -1) {
      Tcl_AppendResult(interp, "Failed ALsetparams.", NULL);
      return TCL_ERROR;
    }
    if ((A->port = alOpenPort("snack-out", "w", A->config)) == NULL) {
      Tcl_AppendResult(interp, "Failed ALopenport.", NULL);
      return TCL_ERROR;
    }
#endif /* OLD_AL */
    break;
  }

  alGetFrameNumber(A->port, (stamp_t*) &A->startfn);
  A->count = 0;

  return TCL_OK;
}

int
SnackAudioClose(ADesc *A)
{
  if (A->port != NULL) {
    if (alGetFilled(A->port) > 0) {
      return(-1);
    }
    alClosePort(A->port);
  }
  alFreeConfig(A->config);
  A->count = 0;

  return(0);
}

long
SnackAudioPause(ADesc *A)
{
  switch (A->mode) {
  case RECORD:
#ifdef OLD_AL
    alClosePort(A->port);
    A->port = NULL;
#else
#endif
    break;
    
  case PLAY:
#ifdef OLD_AL
    A->count = SnackAudioPlayed(A);
    alClosePort(A->port);
    A->port = NULL;
    return(A->count);
#else
#endif
    /*    break;*/
  }
  return(-1);
}

void
SnackAudioResume(ADesc *A)
{
  switch (A->mode) {
  case RECORD:
    break;
    
  case PLAY:
#ifdef OLD_AL
    A->port = ALopenport("snack-out", "w", A->config);
#else
#endif
    break;
  }
}

void
SnackAudioFlush(ADesc *A)
{
  switch (A->mode) {
  case RECORD:
    break;
    
  case PLAY:
#ifdef OLD_AL
    alClosePort(A->port);
    A->port = NULL;
#else
    alDiscardFrames(A->port, 10000000);
#endif
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
  alReadFrames(A->port, buf, nFrames * A->nChannels); /* always returns 0 */
  return(nFrames);
}

int
SnackAudioWrite(ADesc *A, void *buf, int nFrames)
{
  alWriteFrames(A->port, buf, nFrames * A->nChannels);
  return(nFrames);
}

int
SnackAudioReadable(ADesc *A)
{
  if (A->port == NULL) {
    return(0);
  }
  return alGetFilled(A->port);
}

int
SnackAudioWriteable(ADesc *A)
{
  return alGetFillable(A->port);
}

long
SnackAudioPlayed(ADesc *A)
{
  unsigned long long fnum;

  /* Needed on IRIX 6.2 */
  typedef long long stamp_t;

  alGetFrameNumber(A->port, (stamp_t*) &fnum);

  return(A->count + (int)(fnum - A->startfn));
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

#ifdef OLD_AL
  long buf[4];

  g = 255 - g * 255 / 100;
  buf[0] = AL_LEFT_INPUT_ATTEN;
  buf[1] = g;
  buf[2] = AL_RIGHT_INPUT_ATTEN;
  buf[3] = g;
  ALsetparams(AL_DEFAULT_DEVICE, buf, 4);

#else
  ALpv x[2];
  ALfixed gains[2];
  ALparamInfo pi;
  double gmin, gmax, fg;

  alGetParamInfo(AL_DEFAULT_INPUT, AL_GAIN, &pi);
  gmax = alFixedToDouble(pi.max.ll);
  gmin = alFixedToDouble(pi.min.ll);

  x[0].param = AL_GAIN;
  x[0].value.ptr = gains;
  x[0].sizeIn = 2;
  x[1].param = AL_CHANNELS;

  fg = alDoubleToFixed(((double) g / 100.0) * (gmax - gmin) + gmin);
  gains[0] = gains[1] = fg;
#endif /* OLD_AL */
}

void
ASetPlayGain(int gain)
{
  int g = min(max(gain, 0), 100);

#ifdef OLD_AL
  long buf[4];

  g = 255 - g * 255 / 100;
  buf[0] = AL_LEFT_INPUT_ATTEN;
  buf[1] = g;
  buf[2] = AL_RIGHT_INPUT_ATTEN;
  buf[3] = g;
  ALsetparams(AL_DEFAULT_DEVICE, buf, 4);
#else
  ALpv x[2];
  ALfixed gains[2];
  ALparamInfo pi;
  double gmin, gmax, fg;

  alGetParamInfo(AL_DEFAULT_OUTPUT, AL_GAIN, &pi);
  gmax = alFixedToDouble(pi.max.ll);
  gmin = alFixedToDouble(pi.min.ll);

  x[0].param = AL_GAIN;
  x[0].value.ptr = gains;
  x[0].sizeIn = 2;
  x[1].param = AL_CHANNELS;

  fg = alDoubleToFixed(((double) g / 100.0) * (gmax - gmin) + gmin);
  gains[0] = gains[1] = fg;
#endif /* OLD_AL */
}

int
AGetRecGain()
{
#ifdef OLD_AL
  int g = 0;
  long buf[2];

  buf[0] = AL_LEFT_INPUT_ATTEN;
  ALgetparams(AL_DEFAULT_DEVICE, buf, 2);
  g = 100 - buf[1] * 100 / 255;

#else
  int g = 0;
  ALpv x[2];
  ALfixed gain[2];
  ALparamInfo pi;
  double gmin, gmax;

  alGetParamInfo(AL_DEFAULT_INPUT, AL_GAIN, &pi);
  gmax = alFixedToDouble(pi.max.ll);
  gmin = alFixedToDouble(pi.min.ll);

  x[0].param = AL_GAIN;
  x[0].value.ptr = gain;
  x[0].sizeIn = 2;
  x[1].param = AL_CHANNELS;

  alGetParams(AL_DEFAULT_INPUT, x, 2);

  g = (int) (100.0 * (alFixedToDouble(gain[0]) - gmin) / (gmax - gmin));
#endif /* OLD_AL */

  return(g);
}

int
AGetPlayGain()
{
#ifdef OLD_AL
  int g = 0;
  long buf[2];

  buf[0] = AL_LEFT_SPEAKER_GAIN;
  ALgetparams(AL_DEFAULT_DEVICE, buf, 2);
  g = buf[1] * 100 / 255;

#else
  int g = 0;
  ALpv x[2];
  ALfixed gain[2];
  ALparamInfo pi;
  double gmin, gmax;

  alGetParamInfo(AL_DEFAULT_OUTPUT, AL_GAIN, &pi);
  gmax = alFixedToDouble(pi.max.ll);
  gmin = alFixedToDouble(pi.min.ll);

  x[0].param = AL_GAIN;
  x[0].value.ptr = gain;
  x[0].sizeIn = 2;
  x[1].param = AL_CHANNELS;

  alGetParams(AL_DEFAULT_INPUT, x, 2);

  g = (int) (100.0 * (alFixedToDouble(gain[0]) - gmin) / (gmax - gmin));
#endif /* OLD_AL */

  return(g);
}

int
SnackAudioGetEncodings(char *device)
{
  return(LIN24 | LIN16);
}

void
SnackAudioGetRates(char *device, char *buf, int n)
{
  strncpy(buf, "8000 11025 16000 22050 32000 44100 48000", n);
  buf[n-1] = '\0';
}

int
SnackAudioMaxNumberChannels(char *device)
{
  return(16);
}

int
SnackAudioMinNumberChannels(char *device)
{
  return(1);
}

void
SnackMixerGetInputJackLabels(char *buf, int n)
{
  strncpy(buf, "Microphone \"Line In\"", n);
  buf[n-1] = '\0';
}

void
SnackMixerGetOutputJackLabels(char *buf, int n)
{
  strncpy(buf, "Line Headphones", n);
  buf[n-1] = '\0';
}

void
SnackMixerGetInputJack(char *buf, int n)
{
  strncpy(buf, "Microphone", n);
  buf[n-1] = '\0';
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
  if (strncasecmp(line, "Record", strlen(line)) == 0) {
    sprintf(buf, "%d", AGetRecGain());
  }
  if (strncasecmp(line, "Play", strlen(line)) == 0) {
    sprintf(buf, "%d", AGetPlayGain());
  }  
}

void
SnackMixerSetVolume(char *line, int channel, int volume)
{
  if (strncasecmp(line, "Record", strlen(line)) == 0) {
    ASetRecGain(volume);
  }
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
  char *mixLabels[] = { "Play", "Record" };
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
  strncpy(buf, "Play Record", n);
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
