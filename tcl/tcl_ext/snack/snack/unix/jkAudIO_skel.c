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

#define SNACK_NUMBER_MIXERS 1

struct MixerLink mixerLinks[SNACK_NUMBER_MIXERS][2];

int
SnackAudioOpen(ADesc *A, Tcl_Interp *interp, char *device, int mode, int freq,
	       int nchannels, int encoding)
{
  return TCL_OK;
}

int
SnackAudioClose(ADesc *A)
{
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
}

int
SnackAudioRead(ADesc *A, void *buf, int nFrames)
{
}

int
SnackAudioWrite(ADesc *A, void *buf, int nFrames)
{
}

int
SnackAudioReadable(ADesc *A)
{
}

int
SnackAudioWriteable(ADesc *A)
{
  return -1;
}

long
SnackAudioPlayed(ADesc *A)
{
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
