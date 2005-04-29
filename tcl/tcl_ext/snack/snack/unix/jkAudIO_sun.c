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
#include "jkSound.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

static char *audioDev;
#define DEF_AUDIO "/dev/audio"
#define ENV_AUDIO "AUDIODEV"

extern void Snack_WriteLog(char *s);
extern void Snack_WriteLogInt(char *s, int n);

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif

int ctlfd = 0;

#define SNACK_NUMBER_MIXERS 2
#define SNACK_NUMBER_JACKS 5
#define SNACK_NUMBER_OUTJACKS 3

struct MixerLink mixerLinks[SNACK_NUMBER_JACKS][2];

int
SnackAudioOpen(ADesc *A, Tcl_Interp *interp, char *device, int mode, int freq,
	       int nchannels, int encoding)
{
  ioctl(ctlfd, AUDIO_GETINFO, &A->ainfo);

  A->mode = mode;
  switch (mode) {
  case RECORD:
    if ((A->afd = open(audioDev, O_RDONLY, 0)) < 0) {
      Tcl_AppendResult(interp, "Couldn't open ", audioDev, " for read.",
		       NULL);
      return TCL_ERROR;
    }
    break;
    
  case PLAY:
    if ((A->afd = open(audioDev, O_WRONLY, 0)) < 0) {
      Tcl_AppendResult(interp, "Couldn't open ", audioDev, " for write.",
		       NULL);
      return TCL_ERROR;
    }
    fcntl(A->afd, F_SETFL, O_NONBLOCK);
    break;
  }

  fcntl(A->afd, F_SETFD, FD_CLOEXEC);

  A->ainfo.play.sample_rate = freq;
  A->ainfo.play.channels = nchannels;
  A->ainfo.record.sample_rate = freq;
  A->ainfo.record.channels = nchannels;
  A->nChannels = nchannels;
  A->convert = 0;
  A->convBuf = NULL;
  A->convSize = 0;

  switch (encoding) {
  case LIN16:
    A->ainfo.play.encoding    = AUDIO_ENCODING_LINEAR;
    A->ainfo.record.encoding  = AUDIO_ENCODING_LINEAR;
    A->ainfo.play.precision   = 16;
    A->ainfo.record.precision = 16;
    A->bytesPerSample         = sizeof(short);
    break;
  case ALAW:
    if (nchannels == 1) {
      A->ainfo.play.encoding    = AUDIO_ENCODING_ALAW;
      A->ainfo.record.encoding  = AUDIO_ENCODING_ALAW;
      A->ainfo.play.precision   = 8;
      A->ainfo.record.precision = 8;
      A->bytesPerSample         = sizeof(char);
    } else {
      A->ainfo.play.encoding    = AUDIO_ENCODING_LINEAR;
      A->ainfo.record.encoding  = AUDIO_ENCODING_LINEAR;
      A->ainfo.play.precision   = 16;
      A->ainfo.record.precision = 16;
      A->bytesPerSample         = sizeof(short);
      A->convert                = ALAW;
    }
    break;
  case MULAW:
    if (nchannels == 1) {
      A->ainfo.play.encoding    = AUDIO_ENCODING_ULAW;
      A->ainfo.record.encoding  = AUDIO_ENCODING_ULAW;
      A->ainfo.play.precision   = 8;
      A->ainfo.record.precision = 8;
      A->bytesPerSample         = sizeof(char);
    } else {
      A->ainfo.play.encoding    = AUDIO_ENCODING_LINEAR;
      A->ainfo.record.encoding  = AUDIO_ENCODING_LINEAR;
      A->ainfo.play.precision   = 16;
      A->ainfo.record.precision = 16;
      A->bytesPerSample         = sizeof(short);
      A->convert                = MULAW;
    }
    break;
  case LIN8OFFSET:
    A->ainfo.play.encoding    = AUDIO_ENCODING_LINEAR8;
    A->ainfo.record.encoding  = AUDIO_ENCODING_LINEAR8;
    A->ainfo.play.precision   = 8;
    A->ainfo.record.precision = 8;
    A->bytesPerSample         = sizeof(char);
    break;
  }

  if (ioctl(A->afd, AUDIO_SETINFO, &A->ainfo) < 0) {
    Tcl_AppendResult(interp, "Unssupported audio format.", NULL);
    return TCL_ERROR;
  }
  A->time = SnackCurrentTime();
  A->timep = 0.0;
  A->freq = freq;

  return TCL_OK;
}

int
SnackAudioClose(ADesc *A)
{
  close(A->afd);
  ckfree(A->convBuf);

  return(0);
}

long
SnackAudioPause(ADesc *A)
{
  /*  int count;*/
  long res = SnackAudioPlayed(A);

  AUDIO_INITINFO(&A->ainfo);

  switch (A->mode) {
  case RECORD:
    A->ainfo.record.pause = 1;
    ioctl(A->afd, AUDIO_SETINFO, &A->ainfo);
    return(-1);
    break;
    
  case PLAY:
    /*    count = SnackAudioPlayed(A);*/
    A->ainfo.play.pause = 1;
    ioctl(A->afd, AUDIO_SETINFO, &A->ainfo);
    SnackAudioFlush(A);
    /*    return(count);*/
    A->timep = SnackCurrentTime();

    return(res);
    break;
  }
}

void
SnackAudioResume(ADesc *A)
{
  AUDIO_INITINFO(&A->ainfo);

  switch (A->mode) {
  case RECORD:
    A->ainfo.record.pause = 0;
    break;
    
  case PLAY:
    A->ainfo.play.pause = 0;
    A->time = A->time + SnackCurrentTime() - A->timep;
    break;
  }
  ioctl(A->afd, AUDIO_SETINFO, &A->ainfo);
}

void
SnackAudioFlush(ADesc *A)
{
  if (A->mode == RECORD) {
    ioctl(A->afd, I_FLUSH, FLUSHR);
  } else {
    ioctl(A->afd, I_FLUSH, FLUSHW);
  }
}

void
SnackAudioPost(ADesc *A)
{
}

int
SnackAudioRead(ADesc *A, void *buf, int nFrames)
{
  if (nFrames == 0) return(0);

  if (A->convert) {
    int n = 0, i, res;
    short s[2];
    
    for (i = 0; i < nFrames * A->nChannels; i += 2) {
      res = read(A->afd, &s, 2*sizeof(short));
      if (res <= 0) return(n / (A->bytesPerSample * A->nChannels));
      if (A->convert == ALAW) {
	((unsigned char *)buf)[i] = Snack_Lin2Alaw(s[0]);
	((unsigned char *)buf)[i+1] = Snack_Lin2Alaw(s[1]);
      } else {
	((unsigned char *)buf)[i] = Snack_Lin2Mulaw(s[0]);
	((unsigned char *)buf)[i+1] = Snack_Lin2Mulaw(s[1]);
      }
      n += res;
    }
    
    return(n / (A->bytesPerSample * A->nChannels));
  } else {
    int n = read(A->afd, buf, nFrames * A->bytesPerSample * A->nChannels);
    if (n > 0) n /= (A->bytesPerSample * A->nChannels);
    return(n);
  }
}

int
SnackAudioWrite(ADesc *A, void *buf, int nFrames)
{
  if (A->convert) {
    int n = 0, i, res;

    if (nFrames * A->bytesPerSample * A->nChannels > A->convSize) {
      A->convSize = nFrames * A->bytesPerSample * A->nChannels;
      if ((A->convBuf = (short *) ckrealloc(A->convBuf, A->convSize)) == NULL) {
	return(-1);
      }
    }
    A->convSize = nFrames * A->bytesPerSample * A->nChannels;
    for (i = 0; i < nFrames * A->nChannels; i += 2) {
      if (A->convert == ALAW) {
	A->convBuf[i] = Snack_Alaw2Lin(((unsigned char *)buf)[i]);
	A->convBuf[i+1] = Snack_Alaw2Lin(((unsigned char *)buf)[i+1]);
      } else {
	A->convBuf[i] = Snack_Mulaw2Lin(((unsigned char *)buf)[i]);
	A->convBuf[i+1] = Snack_Mulaw2Lin(((unsigned char *)buf)[i+1]);
      }
    }
    n = write(A->afd, A->convBuf, nFrames * A->bytesPerSample * A->nChannels);
    if (n > 0) n /= (A->bytesPerSample * A->nChannels);
    return(n);
  } else {
    int n = write(A->afd, buf, nFrames * A->bytesPerSample * A->nChannels); 
    if (n > 0) n /= (A->bytesPerSample * A->nChannels);
    return(n);
  }
}

int
SnackAudioReadable(ADesc *A)
{
  int NBytes = 0;

  ioctl(A->afd, FIONREAD, &NBytes);
  return(NBytes / (A->bytesPerSample * A->nChannels));
}

int
SnackAudioWriteable(ADesc *A)
{
  return -1;
}

long
SnackAudioPlayed(ADesc *A)
{
  /*  ioctl(A->afd, AUDIO_GETINFO, &A->ainfo);

      return(A->ainfo.play.samples);*/
  long res;
  
  res = (A->freq * (SnackCurrentTime() - A->time) +.5);
  return(res);
}

void
SnackAudioInit()
{
  char *audioCtl;
  
  audioDev = getenv(ENV_AUDIO);  /* try environment variable first */
  if (!audioDev)
    audioDev = DEF_AUDIO;        /* take default */
  
  audioCtl = ckalloc(strlen(audioDev) + 4);
  if (audioCtl) {
    strcpy(audioCtl, audioDev);
    strcat(audioCtl, "ctl");
    if ((ctlfd = open(audioCtl, O_RDWR)) < 0) {
      fprintf(stderr, "Unable to open %s\n", audioCtl);
    }
    ckfree(audioCtl);
  }
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

  if (ctlfd > 0) close(ctlfd);
}

void
ASetRecGain(int gain)
{
  int g = min(max(gain, 0), 100);

  audio_info_t info;

  AUDIO_INITINFO(&info);
  info.record.gain = (int) (g * AUDIO_MAX_GAIN / 100.0 + 0.5);
  ioctl(ctlfd, AUDIO_SETINFO, &info);
}

void
ASetPlayGain(int gain)
{
  int g = min(max(gain, 0), 100);

  audio_info_t info;

  AUDIO_INITINFO(&info);
  info.play.gain = (int) (g * AUDIO_MAX_GAIN / 100.0 + 0.5);
  ioctl(ctlfd, AUDIO_SETINFO, &info);
}

int
AGetRecGain()
{
  int g = 0;
  audio_info_t info;

  ioctl(ctlfd, AUDIO_GETINFO, &info);
  g = (int) (info.record.gain * 100.0 / AUDIO_MAX_GAIN + 0.5);

  return(g);
}

int
AGetPlayGain()
{
  int g = 0;
  audio_info_t info;

  ioctl(ctlfd, AUDIO_GETINFO, &info);
  g = (int) (info.play.gain * 100.0 / AUDIO_MAX_GAIN + 0.5);

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
  strncpy(buf, "Microphone \"Line In\"", n);
  buf[n-1] = '\0';
}

void
SnackMixerGetOutputJackLabels(char *buf, int n)
{
  strncpy(buf, "Speaker \"Line Out\" Headphones", n);
  buf[n-1] = '\0';
}

void
SnackMixerGetInputJack(char *buf, int n)
{
  audio_info_t info;
  int pos = 0;

  ioctl(ctlfd, AUDIO_GETINFO, &info);
  
  if (info.record.port & AUDIO_LINE_IN) {
    pos += (int) sprintf(&buf[pos],  "\"Line In\" ");
  }
  if (info.record.port & AUDIO_MICROPHONE) {
    pos += (int) sprintf(&buf[pos],  "Microphone");
  }
}

int
SnackMixerSetInputJack(Tcl_Interp *interp, char *jack, CONST84 char *status)
{
  audio_info_t info;
  int jackLen = strlen(jack), start = 0, end = 0, mask;

  ioctl(ctlfd, AUDIO_GETINFO, &info);

  while(end < jackLen) {
    while ((end < jackLen) && (!isspace(jack[end]))) end++;
    if (strncasecmp(&jack[start], "Microphone", end - start) == 0) {
      mask = AUDIO_MICROPHONE;
    } else if (strncasecmp(&jack[start], "Line In", end - start) == 0) {
      mask = AUDIO_LINE_IN;
    }
    if (strcmp(status, "0") == 0) {
      info.record.port = (~mask) & 0x03;
    } else {
      info.record.port = mask;
    }
    while ((end < jackLen) && (isspace(jack[end]))) end++;
    start = end;
  }

  ioctl(ctlfd, AUDIO_SETINFO, &info);

  return 0;
}

void
SnackMixerGetOutputJack(char *buf, int n)
{
  audio_info_t info;
  int pos = 0;

  ioctl(ctlfd, AUDIO_GETINFO, &info);
  
  if (info.play.port & AUDIO_SPEAKER) {
    pos += (int) sprintf(&buf[pos],  "Speaker ");
  }
  if (info.play.port & AUDIO_HEADPHONE) {
    pos += (int) sprintf(&buf[pos],  "Headphones ");
  }
  if (info.play.port & AUDIO_LINE_OUT) {
    pos += (int) sprintf(&buf[pos],  "\"Line Out\"");
  }
}

void
SnackMixerSetOutputJack(char *jack, char *status)
{
  audio_info_t info;
  int jackLen = strlen(jack), start = 0, end = 0, mask;

  ioctl(ctlfd, AUDIO_GETINFO, &info);

  while(end < jackLen) {
    while ((end < jackLen) && (!isspace(jack[end]))) end++;
    if (strncasecmp(&jack[start], "Speaker", end - start) == 0) {
      mask = AUDIO_SPEAKER;
    } else if (strncasecmp(&jack[start], "Line Out", end - start) == 0) {
      mask = AUDIO_LINE_OUT;
    } else if (strncasecmp(&jack[start], "Headphones", end - start) == 0) {
      mask = AUDIO_HEADPHONE;
    }
    if (strcmp(status, "0") == 0) {
      info.play.port &= ~mask;
    } else {
      info.play.port |= mask;
    }
    while ((end < jackLen) && (isspace(jack[end]))) end++;
    start = end;
  }

  ioctl(ctlfd, AUDIO_SETINFO, &info);
}

static char *
JackVarProc(ClientData clientData, Tcl_Interp *interp, CONST84 char *name1,
	    CONST84 char *name2, int flags)
{
  MixerLink *mixLink = (MixerLink *) clientData;
  int i, j, status = 0;
  CONST84 char *stringValue;
  char *jackLabels[] = { "Speaker", "Line Out", "Headphones",
			 "Microphone", "Line In" };
  Tcl_Obj *obj, *var;
  audio_info_t info;

  ioctl(ctlfd, AUDIO_GETINFO, &info);

  for (i = 0; i < SNACK_NUMBER_JACKS; i++) {
    if (strncasecmp(mixLink->jack, jackLabels[i], strlen(mixLink->jack))
	== 0) {
      break;
    }
  }
  if ((i == 0 && info.play.port & AUDIO_SPEAKER) ||
      (i == 1 && info.play.port & AUDIO_LINE_OUT) ||
      (i == 2 && info.play.port & AUDIO_HEADPHONE) ||
      (i == 3 && info.record.port & AUDIO_MICROPHONE) ||
      (i == 4 && info.record.port & AUDIO_LINE_IN)) {
    status = 1;
  }

  if (flags & TCL_TRACE_UNSETS) {
    if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
      obj = Tcl_NewIntObj(status);
      var = Tcl_NewStringObj(mixLink->jackVar, -1);
      Tcl_ObjSetVar2(interp, var, NULL, obj, TCL_GLOBAL_ONLY | TCL_PARSE_PART1);
      Tcl_TraceVar(interp, mixLink->jackVar,
		   TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		   JackVarProc, mixLink);
    }
    return (char *) NULL;
  }

  stringValue = Tcl_GetVar(interp, mixLink->jackVar, TCL_GLOBAL_ONLY);
  if (stringValue != NULL) {
    if (i < SNACK_NUMBER_OUTJACKS) {
      SnackMixerSetOutputJack(mixLink->jack, stringValue);
    } else {
      SnackMixerSetInputJack(interp, mixLink->jack, stringValue);
    }
  }

  ioctl(ctlfd, AUDIO_GETINFO, &info);

  for (j = SNACK_NUMBER_OUTJACKS; j < SNACK_NUMBER_JACKS; j++) {
    if ((j == 3 && info.record.port & AUDIO_MICROPHONE) ||
	(j == 4 && info.record.port & AUDIO_LINE_IN)) {
      status = 1;
    } else {
      status = 0;
    }
    if (i == j) continue;
    obj = Tcl_NewIntObj(status);
    var = Tcl_NewStringObj(mixerLinks[j][0].jackVar, -1);
    Tcl_ObjSetVar2(interp, var, NULL, obj, TCL_GLOBAL_ONLY |TCL_PARSE_PART1);
  }

  return (char *) NULL;
}

void
SnackMixerLinkJacks(Tcl_Interp *interp, char *jack, Tcl_Obj *var)
{
  char *jackLabels[] = { "Speaker", "Line Out", "Headphones",
			 "Microphone", "Line In" };
  int i, status = 0;
  CONST84 char *value;
  audio_info_t info;

  ioctl(ctlfd, AUDIO_GETINFO, &info);

  for (i = 0; i < SNACK_NUMBER_JACKS; i++) {
    if (strncasecmp(jack, jackLabels[i], strlen(jack)) == 0) {
      if ((i == 0 && info.play.port & AUDIO_SPEAKER) ||
	  (i == 1 && info.play.port & AUDIO_LINE_OUT) ||
	  (i == 2 && info.play.port & AUDIO_HEADPHONE) ||
	  (i == 3 && info.record.port & AUDIO_MICROPHONE) ||
	  (i == 4 && info.record.port & AUDIO_LINE_IN)) {
	status = 1;
      }
      mixerLinks[i][0].jack = (char *)SnackStrDup(jack);
      mixerLinks[i][0].jackVar = (char *)SnackStrDup(Tcl_GetStringFromObj(var, NULL));
      value = Tcl_GetVar(interp, mixerLinks[i][0].jackVar, TCL_GLOBAL_ONLY);
      if (value != NULL) {
	if (i < SNACK_NUMBER_OUTJACKS) {
	  SnackMixerSetOutputJack(mixerLinks[i][0].jack, value);
	} else {
	  SnackMixerSetInputJack(interp, mixerLinks[i][0].jack, value);
	}
      } else {
	Tcl_Obj *obj = Tcl_NewIntObj(status);
	Tcl_ObjSetVar2(interp, var, NULL, obj, 
		       TCL_GLOBAL_ONLY | TCL_PARSE_PART1);
      }
      Tcl_TraceVar(interp, mixerLinks[i][0].jackVar,
		   TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		   JackVarProc, (ClientData) &mixerLinks[i][0]);
      break;
    }
  }
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
		   VolumeVarProc, mixLink);
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
  int i, j, status = 0;
  char tmp[VOLBUFSIZE];
  Tcl_Obj *obj, *var;
  audio_info_t info;

  ioctl(ctlfd, AUDIO_GETINFO, &info);

  for (i = 0; i < SNACK_NUMBER_JACKS; i++) {
    for (j = 0; j < 2; j++) {
      if (mixerLinks[i][j].mixerVar != NULL) {
	SnackMixerGetVolume(mixerLinks[i][j].mixer, mixerLinks[i][j].channel,
			    tmp, VOLBUFSIZE);
	obj = Tcl_NewIntObj(atoi(tmp));
	var = Tcl_NewStringObj(mixerLinks[i][j].mixerVar, -1);
	Tcl_ObjSetVar2(interp, var, NULL, obj, TCL_GLOBAL_ONLY|TCL_PARSE_PART1);
      }
    }
    if ((i == 0 && info.play.port & AUDIO_SPEAKER) ||
	(i == 1 && info.play.port & AUDIO_LINE_OUT) ||
	(i == 2 && info.play.port & AUDIO_HEADPHONE) ||
	(i == 3 && info.record.port & AUDIO_MICROPHONE) ||
	(i == 4 && info.record.port & AUDIO_LINE_IN)) {
      status = 1;
    } else {
      status = 0;
    }
    
    if (mixerLinks[i][j].jackVar != NULL) {
      obj = Tcl_NewIntObj(status);
      var = Tcl_NewStringObj(mixerLinks[i][0].jackVar, -1);
      Tcl_ObjSetVar2(interp, var, NULL, obj, TCL_GLOBAL_ONLY |TCL_PARSE_PART1);
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
