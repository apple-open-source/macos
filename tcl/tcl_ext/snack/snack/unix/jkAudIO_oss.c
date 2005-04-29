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

#include "tcl.h"
#include "jkAudIO.h"
#include "jkSound.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#ifdef __NetBSD__
 #include <soundcard.h>
#else /* OSS default */
 #include <sys/soundcard.h>
#endif
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <glob.h>
#define DEVICE_NAME "/dev/dsp"
#define MIXER_NAME  "/dev/mixer"
static char *defaultDeviceName = DEVICE_NAME;
extern void Snack_WriteLog(char *s);
extern void Snack_WriteLogInt(char *s, int n);

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif

static int mfd = 0;

static struct MixerLink mixerLinks[SOUND_MIXER_NRDEVICES][2];

static int littleEndian = 0;

static int minNumChan = 1;

int
SnackAudioOpen(ADesc *A, Tcl_Interp *interp, char *device, int mode, int freq,
	       int nchannels, int encoding)
{
  int format;
  int nformat;
  int channels;
  int speed;
  int mask;
  /*  int frag = 0x7fff0010;*/

  if (A->debug > 1) Snack_WriteLog("  Enter SnackAudioOpen\n");

  if (device == NULL) {
    device = defaultDeviceName;
  }
  if (strlen(device) == 0) {
    device = defaultDeviceName;
  }

  /* Test if the device is not locked by another process. This is
   * just a crude workaround to avoid complete lockup of snack. It is
   * not perfect since it is theoretically possible that another program
   * locks the device between the close() and open() calls below. */
  A->afd = open(device, O_WRONLY|O_NONBLOCK);
  if(A->afd == -1) {
    Tcl_AppendResult(interp, "Could not gain access to ", device, " for writing.",NULL);
    return TCL_ERROR;
  }
  close(A->afd);

  A->mode = mode;
  switch (mode) {
  case RECORD:
    if ((A->afd = open(device, O_RDONLY, 0)) == -1) {
      Tcl_AppendResult(interp, "Could not open ", device, " for read.",
		       NULL);
      return TCL_ERROR;
    }
    break;

  case PLAY:
    if ((A->afd = open(device, O_WRONLY, 0)) == -1) {
      Tcl_AppendResult(interp, "Could not open ", device, " for write.",
		       NULL);
      return TCL_ERROR;
    }
    break;
  }

  fcntl(A->afd, F_SETFD, FD_CLOEXEC);

  /*  if (ioctl(A->afd, SNDCTL_DSP_SETFRAGMENT, &frag)) return(-1);*/

  if (ioctl(A->afd, SNDCTL_DSP_GETFMTS, &mask) == -1) {
    close(A->afd);
    Tcl_AppendResult(interp, "Failed getting formats.", NULL);
    return TCL_ERROR;
  }
  
  A->convert = 0;

  switch (encoding) {
  case LIN16:
    if (littleEndian) {
      format = AFMT_S16_LE;
    } else {
      format = AFMT_S16_BE;
    }
    A->bytesPerSample = sizeof(short);
    break;
#ifdef AFMT_S32_LE
  case LIN24:
    if (littleEndian) {
      format = AFMT_S32_LE;
    } else {
      format = AFMT_S32_BE;
    }
    A->bytesPerSample = sizeof(int);
    break;
#endif
  case ALAW:
    if (mask & AFMT_A_LAW) {
      format = AFMT_A_LAW;
      A->bytesPerSample = sizeof(char);
    } else {
      if (littleEndian) {
	format = AFMT_S16_LE;
      } else {
	format = AFMT_S16_BE;
      }
      A->bytesPerSample = sizeof(short);
      A->convert = ALAW;
    }
    break;
  case MULAW:
    if (mask & AFMT_MU_LAW) {
      format = AFMT_MU_LAW;
      A->bytesPerSample = sizeof(char);
    } else {
      if (littleEndian) {
	format = AFMT_S16_LE;
      } else {
	format = AFMT_S16_BE;
      }
      A->bytesPerSample = sizeof(short);
      A->convert = MULAW;
    }
    break;
  case LIN8OFFSET:
    format = AFMT_U8;
    A->bytesPerSample = sizeof(char);
    break;
  case LIN8:
    format = AFMT_S8;
    A->bytesPerSample = sizeof(char);
    break;
  }

  nformat = format;
  if (ioctl(A->afd, SNDCTL_DSP_SETFMT, &format) == -1
      || format != nformat) {
    close(A->afd);
    Tcl_AppendResult(interp, "Failed setting format.", NULL);
    return TCL_ERROR;
  }

  A->nChannels = nchannels;
  channels = nchannels;
  if (ioctl(A->afd, SNDCTL_DSP_CHANNELS, &channels) == -1
      || channels != nchannels) {
    close(A->afd);
    Tcl_AppendResult(interp, "Failed setting number of channels.", NULL);
    return TCL_ERROR;
  }

  speed = freq;
  if (ioctl(A->afd, SNDCTL_DSP_SPEED, &speed) == -1
      || abs(speed - freq) > freq / 100) {
    close(A->afd);
    Tcl_AppendResult(interp, "Failed setting sample frequency.", NULL);
    return TCL_ERROR;
  }
  
  /*  A->count = 0;*/
  A->frag_size = 0;
  if (ioctl(A->afd, SNDCTL_DSP_GETBLKSIZE, &A->frag_size) == -1) {
    close(A->afd);
    Tcl_AppendResult(interp, "Failed getting fragment size.", NULL);
    return TCL_ERROR;
  }
  /*    printf("Frag size: %d\n",  A->frag_size);*/
  A->time = SnackCurrentTime();
  A->timep = 0.0;
  A->freq = freq;
  A->warm = 0;

  if (A->debug > 1) Snack_WriteLogInt("  Exit SnackAudioOpen", A->frag_size);

  return TCL_OK;
}

int
SnackAudioClose(ADesc *A)
{
  if (A->debug > 1) Snack_WriteLog("  Enter SnackAudioClose\n");

  /*  A->count = 0;*/

  close(A->afd);

  if (A->debug > 1) Snack_WriteLog("  Exit SnackAudioClose\n");

  return(0);
}

long
SnackAudioPause(ADesc *A)
{
  long res = SnackAudioPlayed(A);

  A->timep = SnackCurrentTime();
  ioctl(A->afd, SNDCTL_DSP_RESET, 0);

  return(res);
}

void
SnackAudioResume(ADesc *A)
{
  A->time = A->time + SnackCurrentTime() - A->timep;
}

void
SnackAudioFlush(ADesc *A)
{
  if (A->mode == RECORD) {
  } else {
    ioctl(A->afd, SNDCTL_DSP_RESET, 0);
  }
}

static char zeroBlock[16];

void
SnackAudioPost(ADesc *A)
{
  if (A->debug > 1) Snack_WriteLog("  Enter SnackAudioPost\n");

  if (A->warm == 1) {
    int i;
    for (i = 0; i < A->frag_size / (A->bytesPerSample * A->nChannels); i++) {
      write(A->afd, zeroBlock, A->bytesPerSample * A->nChannels);
    }
    A->warm = 2;
    ioctl(A->afd, SNDCTL_DSP_POST, 0);
  }

  if (A->debug > 1) Snack_WriteLog("  Exit SnackAudioPost\n");
}

int
SnackAudioRead(ADesc *A, void *buf, int nFrames)
{
  int n = 2;

  if (A->debug > 1) Snack_WriteLogInt("  Enter SnackAudioRead", nFrames);
  

  while (nFrames > n * 2) n *= 2;
  nFrames = n;

  if (A->convert) {
    int n = 0, i, res;
    short s[2];

    for (i = 0; i < nFrames * A->nChannels; i += A->nChannels) {
      res = read(A->afd, &s, A->nChannels * sizeof(short));
      if (res <= 0) return(n / (A->bytesPerSample * A->nChannels));
      if (A->convert == ALAW) {
	((unsigned char *)buf)[i] = Snack_Lin2Alaw(s[0]);
	if (A->nChannels == 2) {
	  ((unsigned char *)buf)[i+1] = Snack_Lin2Alaw(s[1]);
	}
      } else {
	((unsigned char *)buf)[i] = Snack_Lin2Mulaw(s[0]);
	if (A->nChannels == 2) {
	  ((unsigned char *)buf)[i+1] = Snack_Lin2Mulaw(s[1]);
	}
      }
      n += res;
    }

    return(n / (A->bytesPerSample * A->nChannels));
  } else {
    int n = read(A->afd, (unsigned char *)buf, nFrames * A->bytesPerSample * A->nChannels);

    if (n > 0) n /= (A->bytesPerSample * A->nChannels);

    if (A->debug > 1) Snack_WriteLogInt("  Exit SnackAudioRead", n);

    return(n);
  }
}

int
SnackAudioWrite(ADesc *A, void *buf, int nFrames)
{
  if (A->warm == 0) A->warm = 1;

  if (A->convert) {
    int n = 0, i, res;
    short s;

    for (i = 0; i < nFrames * A->nChannels; i++) {
      if (A->convert == ALAW) {
	s = Snack_Alaw2Lin(((unsigned char *)buf)[i]);
      } else {
	s = Snack_Mulaw2Lin(((unsigned char *)buf)[i]);
      }
      res = write(A->afd, &s, sizeof(short));
      if (res <= 0) return(n / (A->bytesPerSample * A->nChannels));
      n += res;
    }

    return(n / (A->bytesPerSample * A->nChannels));
  } else {
    int n = write(A->afd, buf, nFrames * A->bytesPerSample * A->nChannels); 
    if (n > 0) n /= (A->bytesPerSample * A->nChannels);

    return(n);
  }
}

int
SnackAudioReadable(ADesc *A)
{
  audio_buf_info info;

  if (A->debug > 1) Snack_WriteLog("  Enter SnackAudioReadable\n");

  ioctl(A->afd, SNDCTL_DSP_GETISPACE, &info);
  if (info.bytes > 60*44100*4) info.bytes = 0;
  if (A->debug > 1) Snack_WriteLogInt("  Exit SnackAudioReadable", info.bytes);

  return (info.bytes / (A->bytesPerSample * A->nChannels));
}

int
SnackAudioWriteable(ADesc *A)
{
  audio_buf_info info;

  ioctl(A->afd, SNDCTL_DSP_GETOSPACE, &info);

  return (info.bytes / (A->bytesPerSample * A->nChannels));
}

long
SnackAudioPlayed(ADesc *A)
{
  /*
  count_info info;
  int res = 0;

  if (A->warm) {
    ioctl(A->afd, SNDCTL_DSP_GETOPTR, &info);
    if (info.bytes > 0 || A->warm == 2) {  
      res = (A->freq * (SnackCurrentTime() - A->time) +.5);
      A->warm = 2;
    }
  }
  */
  long res;
  
  res = (A->freq * (SnackCurrentTime() - A->time) +.5);
  
  return(res);
}

void
SnackAudioInit()
{
  union {
    char c[sizeof(short)];
    short s;
  } order;
  int afd, format, channels, nchannels;
  /*
  int i, n;
  char *arr[MAX_NUM_DEVICES];
  */

  /* Compute the byte order of this machine. */

  order.s = 1;
  if (order.c[0] == 1) {
    littleEndian = 1;
  }

  if ((mfd = open(MIXER_NAME, O_RDWR, 0)) == -1) {
    fprintf(stderr, "Unable to open mixer %s\n", MIXER_NAME);
  }
  /*
  n = SnackGetOutputDevices(arr, MAX_NUM_DEVICES);
  for (i = 0; i < n; i++) {
    printf("Trying %s %d\n",arr[i], open(arr[i], O_WRONLY, 0));
    if ((afd = open(arr[i], O_WRONLY, 0)) != -1) {
      defaultDeviceName = arr[i];
      printf("accepting %s %d\n",defaultDeviceName,afd);
      break;
    }
  }
  */

  if ((afd = open(defaultDeviceName, O_WRONLY, 0)) == -1) {
    defaultDeviceName = "/dev/sound/dsp";
    if ((afd = open(defaultDeviceName, O_WRONLY, 0)) == -1) {
      return;
    }
  }
  close(afd);
  
  /* Determine minimum number of channels supported. */
  
  if ((afd = open(defaultDeviceName, O_WRONLY, 0)) == -1) {
    return;
  }
  
  if (littleEndian) {
    format = AFMT_S16_LE;
  } else {
    format = AFMT_S16_BE;
  }
  if (ioctl(afd, SNDCTL_DSP_SETFMT, &format) == -1) {
    close(afd);
    return;
  }
  channels = nchannels = 1;
  if (ioctl(afd, SNDCTL_DSP_CHANNELS, &channels) == -1
      || channels != nchannels) {
    minNumChan = channels;
  }
  close(afd);
}

void
SnackAudioFree()
{
  int i, j;

  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
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

  close(mfd);
}

void
ASetRecGain(int gain)
{
  int g = min(max(gain, 0), 100);
  int recsrc = 0;

  g = g * 256 + g;
  ioctl(mfd, SOUND_MIXER_READ_RECSRC, &recsrc);
  if (recsrc & SOUND_MASK_LINE) {
    ioctl(mfd, SOUND_MIXER_WRITE_LINE, &g);
  } else {
    ioctl(mfd, SOUND_MIXER_WRITE_MIC, &g);
  }
}

void
ASetPlayGain(int gain)
{
  int g = min(max(gain, 0), 100);
  int pcm_gain = 25700;

  g = g * 256 + g;
  ioctl(mfd, SOUND_MIXER_WRITE_VOLUME, &g);
  ioctl(mfd, SOUND_MIXER_WRITE_PCM, &pcm_gain);
}

int
AGetRecGain()
{
  int g = 0, left, right, recsrc = 0;

  ioctl(mfd, SOUND_MIXER_READ_RECSRC, &recsrc);
  if (recsrc & SOUND_MASK_LINE) {
    ioctl(mfd, SOUND_MIXER_READ_LINE, &g);
  } else {
    ioctl(mfd, SOUND_MIXER_READ_MIC, &g);
  }
  left  =  g & 0xff;
  right = (g & 0xff00) / 256;
  g = (left + right) / 2;

  return(g);
}

int
AGetPlayGain()
{
  int g = 0, left, right;
  
  ioctl(mfd, SOUND_MIXER_READ_VOLUME, &g);
  left  =  g & 0xff;
  right = (g & 0xff00) / 256;
  g = (left + right) / 2;

  return(g);
}

int
SnackAudioGetEncodings(char *device)
{
  int afd, mask;
  
  if ((afd = open(DEVICE_NAME, O_WRONLY, 0)) == -1) {
    return(0);
  }
  if (ioctl(afd, SNDCTL_DSP_GETFMTS, &mask) == -1) {
    return(0);
  }
  close(afd);
  
  if (mask & AFMT_S16_LE || mask & AFMT_S16_BE) {
    return(LIN16);
  } else {
    return(0);
  }
}

void
SnackAudioGetRates(char *device, char *buf, int n)
{
  int afd, freq, pos= 0, i;
  int f[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 96000 };

  if ((afd = open(DEVICE_NAME, O_WRONLY, 0)) == -1) {
    buf[0] = '\0';
    return;
  }
  for (i = 0; i < 8; i++) {
    freq = f[i];
    if (ioctl(afd, SNDCTL_DSP_SPEED, &freq) == -1) break;
    if (abs(f[i] - freq) > freq / 100) continue;
    pos += sprintf(&buf[pos], "%d ", freq);
  }
  close(afd);
}

int
SnackAudioMaxNumberChannels(char *device)
{
  return(2);
}

int
SnackAudioMinNumberChannels(char *device)
{
  return(minNumChan);
}

void
SnackMixerGetInputJackLabels(char *buf, int n)
{
  char *jackLabels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
  int i, recMask, pos = 0;

  if (mfd != -1) {
    ioctl(mfd, SOUND_MIXER_READ_RECMASK, &recMask);
    for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
      if ((1 << i) & recMask) {
	pos += sprintf(&buf[pos], "%s", jackLabels[i]);
	pos += sprintf(&buf[pos], " ");
      }
    }
  } else {
    buf[0] = '\0';
  }
  buf[n-1] = '\0';
}

void
SnackMixerGetOutputJackLabels(char *buf, int n)
{
  buf[0] = '\0';
}

void
SnackMixerGetInputJack(char *buf, int n)
{
  char *jackLabels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
  int i, recSrc = 0, pos = 0;

  ioctl(mfd, SOUND_MIXER_READ_RECSRC, &recSrc);
  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
    if ((1 << i) & recSrc) {
      pos += sprintf(&buf[pos], "%s", jackLabels[i]);
      while (isspace(buf[pos-1])) pos--;
      pos += sprintf(&buf[pos], " ");
    }
  }
  if(isspace(buf[pos-1])) pos--;
  buf[pos] = '\0';
  /*printf("SnackMixerGetInputJack %x, %s\n", recSrc, buf);*/
}

int
SnackMixerSetInputJack(Tcl_Interp *interp, char *jack, CONST84 char *status)
{
  char *jackLabels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
  int i, recSrc = 0, currSrc;

  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
    if (strncasecmp(jack, jackLabels[i], strlen(jack)) == 0) {
      recSrc = 1 << i;
      break;
    }
  }
  
  ioctl(mfd, SOUND_MIXER_READ_RECSRC, &currSrc);

/*  printf("SnackMixerSetInputJack1 %x %s %s\n", currSrc, jack, status);*/

  if (strcmp(status, "1") == 0) {
    recSrc |= currSrc;
  } else {
    recSrc = (currSrc & ~recSrc);
  }
/*  printf("SnackMixerSetInputJack2 %x\n", recSrc);*/
  
  if (ioctl(mfd, SOUND_MIXER_WRITE_RECSRC, &recSrc) == -1) {
    return 1;
  } else {
    ioctl(mfd, SOUND_MIXER_READ_RECSRC, &recSrc);
/*    printf("SnackMixerSetInputJack3 %x\n", recSrc);*/
    return 0;
  }
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

static int dontTrace = 0;

static char *
JackVarProc(ClientData clientData, Tcl_Interp *interp, CONST84 char *name1,
	    CONST84 char *name2, int flags)
{
  MixerLink *mixLink = (MixerLink *) clientData;
  char *jackLabels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
  int i, recSrc = 0, status = 0;
  CONST84 char *stringValue;
  Tcl_Obj *obj, *var;

  if (dontTrace) return (char *) NULL;

  ioctl(mfd, SOUND_MIXER_READ_RECSRC, &recSrc);
/*printf("JackVarProc %x %s %s\n", recSrc, name1, name2);*/
  if (flags & TCL_TRACE_UNSETS) {
    if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
      for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
	if (strncasecmp(mixLink->jack, jackLabels[i], strlen(mixLink->jack))
	    == 0) {
	  if ((1 << i) & recSrc) {
	    status = 1;
	  } else {
	    status = 0;
	  }
	  break;
	}
      }
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
    SnackMixerSetInputJack(interp, mixLink->jack, stringValue);
  }

  ioctl(mfd, SOUND_MIXER_READ_RECSRC, &recSrc);
/*printf("JackVarProc2 %x\n", recSrc);*/
  dontTrace = 1;
  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
    if (mixerLinks[i][0].jackVar != NULL) {
      if ((1 << i) & recSrc) {
	status = 1;
      } else {
	status = 0;
      }
      obj = Tcl_NewIntObj(status);
      var = Tcl_NewStringObj(mixerLinks[i][0].jackVar, -1);
      Tcl_ObjSetVar2(interp, var, NULL, obj, TCL_GLOBAL_ONLY |TCL_PARSE_PART1);
    }
  }
  dontTrace = 0;

  return (char *) NULL;
}

void
SnackMixerLinkJacks(Tcl_Interp *interp, char *jack, Tcl_Obj *var)
{
  char *jackLabels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
  int i, recSrc = 0, status;
  CONST84 char *value;

  ioctl(mfd, SOUND_MIXER_READ_RECSRC, &recSrc);

  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
    if (strncasecmp(jack, jackLabels[i], strlen(jack)) == 0) {
      if ((1 << i) & recSrc) {
	status = 1;
      } else {
	status = 0;
      }
      mixerLinks[i][0].jack = SnackStrDup(jack);
      mixerLinks[i][0].jackVar = SnackStrDup(Tcl_GetStringFromObj(var, NULL));
      value = Tcl_GetVar(interp, mixerLinks[i][0].jackVar, TCL_GLOBAL_ONLY);
      if (value != NULL) {
	SnackMixerSetInputJack(interp, mixerLinks[i][0].jack, value);
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
  char *mixLabels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
  int i, devMask;

  ioctl(mfd, SOUND_MIXER_READ_STEREODEVS, &devMask);
  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
    if (strncasecmp(line, mixLabels[i], strlen(line)) == 0) {
      if (devMask & (1 << i)) {
	sprintf(buf, "Left Right");
      } else {
	sprintf(buf, "Mono");
      }
      break;
    }
  }
}

void
SnackMixerGetVolume(char *line, int channel, char *buf, int n)
{
  char *mixLabels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
  int i, vol = 0, devMask, isStereo = 0, left, right;

  buf[0] = '\0';

  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
    if (strncasecmp(line, mixLabels[i], strlen(line)) == 0) {
      ioctl(mfd, MIXER_READ(i), &vol);
      ioctl(mfd, SOUND_MIXER_READ_STEREODEVS, &devMask);
      if (devMask & (1 << i)) {
	isStereo = 1;
      }
      break;
    }
  }
  left  =  vol & 0xff;
  right = (vol & 0xff00) >> 8;
  if (isStereo) {
    if (channel == 0) {
      sprintf(buf, "%d", left);
    } else if (channel == 1) {
      sprintf(buf, "%d", right);
    } else if (channel == -1) {
      sprintf(buf, "%d", (left + right)/2);
    }
  } else {
    sprintf(buf, "%d", left);
  }
}

void
SnackMixerSetVolume(char *line, int channel, int volume)
{
  char *mixLabels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
  int tmp = min(max(volume, 0), 100), i, oldVol = 0;
  int vol = (tmp << 8) + tmp;

  if (channel == 0) {
    vol = tmp;
  }
  if (channel == 1) {
    vol = tmp << 8;
  }

  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
    if (strncasecmp(line, mixLabels[i], strlen(line)) == 0) {
      ioctl(mfd, MIXER_READ(i), &oldVol);
      if (channel == 0) {
	vol = (oldVol & 0xff00) | (vol & 0x00ff);
      }
      if (channel == 1) {
	vol = (vol & 0xff00) | (oldVol & 0x00ff);
      }
      ioctl(mfd, MIXER_WRITE(i), &vol);
      break;
    }
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
  char *mixLabels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
  int i, j, channel;
  CONST84 char *value;
  char tmp[VOLBUFSIZE];

  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
    if (strncasecmp(line, mixLabels[i], strlen(line)) == 0) {
      for (j = 0; j < n; j++) {
	if (n == 1) {
	  channel = -1;
	} else {
	  channel = j;
	}
	mixerLinks[i][j].mixer = SnackStrDup(line);
	mixerLinks[i][j].mixerVar = SnackStrDup(Tcl_GetStringFromObj(objv[j+3],NULL));
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
  int i, j, recSrc, status;
  char tmp[VOLBUFSIZE];
  Tcl_Obj *obj, *var;

  ioctl(mfd, SOUND_MIXER_READ_RECSRC, &recSrc);
  for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
    for (j = 0; j < 2; j++) {
      if (mixerLinks[i][j].mixerVar != NULL) {
	SnackMixerGetVolume(mixerLinks[i][j].mixer, mixerLinks[i][j].channel,
			    tmp, VOLBUFSIZE);
	obj = Tcl_NewIntObj(atoi(tmp));
	var = Tcl_NewStringObj(mixerLinks[i][j].mixerVar, -1);
	Tcl_ObjSetVar2(interp, var, NULL, obj, TCL_GLOBAL_ONLY|TCL_PARSE_PART1);
      }
    }
    if (mixerLinks[i][0].jackVar != NULL) {
      if ((1 << i) & recSrc) {
	status = 1;
      } else {
	status = 0;
      }
      obj = Tcl_NewIntObj(status);
      var = Tcl_NewStringObj(mixerLinks[i][0].jackVar, -1);
      Tcl_ObjSetVar2(interp, var, NULL, obj, TCL_GLOBAL_ONLY | TCL_PARSE_PART1);
    }
  }
}

void
SnackMixerGetLineLabels(char *buf, int n)
{
  char *mixLabels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;
  int i, devMask, pos = 0;

  if (mfd != -1) {
    ioctl(mfd, SOUND_MIXER_READ_DEVMASK, &devMask);
    for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
      if ((1 << i) & devMask && pos < n-8) {
	pos += sprintf(&buf[pos], "%s", mixLabels[i]);
	pos += sprintf(&buf[pos], " ");
      }
    }
  } else {
    buf[0] = '\0';
  }
  buf[n-1] = '\0';
}

int
SnackGetOutputDevices(char **arr, int n)
{
  return SnackGetInputDevices(arr, n);
}

int
SnackGetInputDevices(char **arr, int n)
{
  int i, j = 0;
  glob_t globt;
  
  glob("/dev/dsp*", 0, NULL, &globt);
  glob("/dev/audio*", GLOB_APPEND, NULL, &globt);
  glob("/dev/sound/dsp*", GLOB_APPEND, NULL, &globt);
  glob("/dev/sound/audio*", GLOB_APPEND, NULL, &globt);

  for (i = 0; i < globt.gl_pathc; i++) {
    if (j < n) {
      arr[j++] = (char *) SnackStrDup(globt.gl_pathv[i]);
    }
  }
  globfree(&globt);

  return(j);
}

int
SnackGetMixerDevices(char **arr, int n)
{
  int i, j = 0;
  glob_t globt;
  
  glob("/dev/mixer*", 0, NULL, &globt);
  glob("/dev/sound/mixer*", GLOB_APPEND, NULL, &globt);
  
  for (i = 0; i < globt.gl_pathc; i++) {
    if (j < n) {
      arr[j++] = (char *) SnackStrDup(globt.gl_pathv[i]);
    }
  }
  globfree(&globt);

  return(j);
}
