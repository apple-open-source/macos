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

extern void Snack_WriteLog(char *s);
extern void Snack_WriteLogInt(char *s, int n);

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

#define HP_DEFAULT   1
#define HP_SPEAKER   2
#define HP_LINEOUT   4
#define HP_HEADPHONE 8
#define HP_MIC       16
#define HP_LINEIN    32

static int hpJacks = HP_HEADPHONE | HP_MIC;

#define SNACK_MIXER_LABELS { "Play", "Record" }
#define SNACK_JACK_LABELS  { "Default", "Speaker", "Line Out", "Headphones",\
                             "Microphone", "Line In" }
#define SNACK_NUMBER_MIXERS 2
#define SNACK_NUMBER_JACKS 6
#define SNACK_NUMBER_OUTJACKS 4

struct MixerLink mixerLinks[SNACK_NUMBER_JACKS][2];

Audio *a; /* for mixer */

long newHandler(Audio * audio, AErrorEvent * errorEvent)
{
  char buf[132];

  AGetErrorText (audio, errorEvent->error_code, buf, 131);
  fprintf (stderr, "%s: %s\n", "Audio Error", buf);
  exit (1);
}

int
SnackAudioOpen(ADesc *A, Tcl_Interp *interp, char *device, int mode, int freq,
	       int nchannels, int encoding)
{
  AudioAttrMask   attribsMask;
  AudioAttributes attribs;
  AGainEntry      gainEntry[SNACK_NUMBER_OUTJACKS];
  SSRecordParams  recordParams;
  SSPlayParams    playParams;
  SStream	  audioStream;
  long            status;
  int             n = 0;

  if (A->debug > 1) Snack_WriteLog("  Enter SnackAudioOpen\n");

  ASetErrorHandler(newHandler);

  if ((A->audio = AOpenAudio(NULL, NULL)) == NULL) {
    Tcl_AppendResult(interp, "Audio server not active (AOpenAudio returns NULL).", NULL);
    return TCL_ERROR;
  }

  attribs.type = ATSampled;
  attribs.attr.sampled_attr.sampling_rate = freq;
  attribs.attr.sampled_attr.channels = nchannels;
  A->nChannels = nchannels;
  A->convert = 0;

  switch (encoding) {
  case LIN16:
    attribs.attr.sampled_attr.data_format = ADFLin16;
    attribs.attr.sampled_attr.bits_per_sample = 16;
    A->bytesPerSample = sizeof(short);
    break;
  case ALAW:
    attribs.attr.sampled_attr.data_format = ADFALaw;
    attribs.attr.sampled_attr.bits_per_sample = 8;
    A->bytesPerSample = sizeof(char);
    break;
  case MULAW:
    attribs.attr.sampled_attr.data_format = ADFMuLaw;
    attribs.attr.sampled_attr.bits_per_sample = 8;
    A->bytesPerSample = sizeof(char);
    break;
  case LIN8OFFSET:
    /*
      attribs.attr.sampled_attr.data_format = ADFLin8Offset;
    */
    A->convert = 1;
    attribs.attr.sampled_attr.data_format = ADFMuLaw;
    attribs.attr.sampled_attr.bits_per_sample = 8;
    A->bytesPerSample = sizeof(char);
    break;
  case LIN8:
    /*
      attribs.attr.sampled_attr.data_format = ADFLin8;
    */
    A->convert = 1;
    attribs.attr.sampled_attr.data_format = ADFMuLaw;
    attribs.attr.sampled_attr.bits_per_sample = 8;
    A->bytesPerSample = sizeof(char);
    Tcl_AppendResult(interp, "Invalid audio format.", NULL);
    return TCL_ERROR;
    break;
  }
  attribs.attr.sampled_attr.interleave = 1;
  attribsMask = ASSamplingRateMask | ASChannelsMask | ASDataFormatMask | ASBitsPerSampleMask | ASInterleaveMask;

  A->mode = mode;
  switch (mode) {
  case RECORD:
    if (hpJacks & HP_MIC) {
      gainEntry[n].u.i.in_ch = AICTMono;
      gainEntry[n].gain = AUnityGain;
      gainEntry[n].u.i.in_src = AISTMonoMicrophone;
      n++;
    }
    if (hpJacks & HP_LINEIN) {
      gainEntry[n].u.i.in_ch = AICTMono;
      gainEntry[n].gain = AUnityGain;
      gainEntry[n].u.i.in_src = AISTMonoAuxiliary;
      n++;
    }
    if (hpJacks & HP_DEFAULT) {
      gainEntry[n].u.i.in_ch = AICTMono;
      gainEntry[n].gain = AUnityGain;
      gainEntry[n].u.i.in_src = AISTDefaultInput;
      n++;
    }
    recordParams.gain_matrix.type = AGMTInput;
    recordParams.gain_matrix.num_entries = n;
    recordParams.gain_matrix.gain_entries = gainEntry;
    recordParams.record_gain = AUnityGain;
    recordParams.pause_first = 1;
    recordParams.event_mask = 0;
    
    A->transid = ARecordSStream(A->audio, attribsMask, &attribs, &recordParams, &audioStream, NULL);

    break;
  case PLAY:
    if (hpJacks & HP_SPEAKER) {
      gainEntry[n].u.o.out_ch = AOCTMono;
      gainEntry[n].gain = AUnityGain;
      gainEntry[n].u.o.out_dst = AODTMonoIntSpeaker;
      n++;
    }
    if (hpJacks & HP_LINEOUT) {
      gainEntry[n].u.o.out_ch = AOCTMono;
      gainEntry[n].gain = AUnityGain;
      gainEntry[n].u.o.out_dst = AODTMonoLineOut;
      n++;
    }
    if (hpJacks & HP_HEADPHONE) {
      gainEntry[n].u.o.out_ch = AOCTMono;
      gainEntry[n].gain = AUnityGain;
      gainEntry[n].u.o.out_dst = AODTMonoHeadphone;
      n++;
    }
    if (hpJacks & HP_DEFAULT) {
      gainEntry[n].u.o.out_ch = AOCTMono;
      gainEntry[n].gain = AUnityGain;
      gainEntry[n].u.o.out_dst = AODTDefaultOutput;
      n++;
    }
    playParams.gain_matrix.type = AGMTOutput;
    playParams.gain_matrix.num_entries = n;
    playParams.gain_matrix.gain_entries = gainEntry;
    playParams.play_volume = AUnityGain;
    playParams.priority = APriorityNormal;
    playParams.event_mask = 0;

    A->transid = APlaySStream(A->audio, attribsMask, &attribs, &playParams, &audioStream, NULL);

    break;
  default:
    return TCL_ERROR;
    break;
  }
  if ((A->Socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    Tcl_AppendResult(interp, "Couldn't open audio device!", NULL);
    return TCL_ERROR;
  }
  status = connect(A->Socket, (struct sockaddr *)&audioStream.tcp_sockaddr, sizeof(struct sockaddr_in));

  A->last = 0;
  /*
  A->time = SnackCurrentTime();
  A->freq = freq;
  */
  if (A->debug > 1) Snack_WriteLogInt("  Exit SnackAudioOpen",
				       A->bytesPerSample);

  return TCL_OK;
}

int
SnackAudioClose(ADesc *A)
{
  if (A->debug > 1) Snack_WriteLog("  Enter SnackAudioClose\n");

  close(A->Socket);
  ASetCloseDownMode(A->audio, AKeepTransactions, NULL);
  ACloseAudio(A->audio, NULL);
  A->audio = NULL;

  if (A->debug > 1) Snack_WriteLog("  Exit SnackAudioClose\n");

  return(0);
}

long
SnackAudioPause(ADesc *A)
{
  APauseAudio(A->audio, A->transid, NULL, NULL);
  return(-1);
}

void
SnackAudioResume(ADesc *A)
{
  AResumeAudio(A->audio, A->transid, NULL, NULL);
}

#define FLUSH_BUFSIZE 1024

void
SnackAudioFlush(ADesc *A)
{
  if (A->debug > 1) Snack_WriteLog("  Enter SnackAudioFlush\n");

  if (A->mode == RECORD) {
    short buffer[FLUSH_BUFSIZE];
    int i;
    int n = SnackAudioReadable(A);
    
    while (n > 0) {
      if (n > FLUSH_BUFSIZE) {
	n -= SnackAudioRead(A, buffer, FLUSH_BUFSIZE);
      } else {
	n -= SnackAudioRead(A, buffer, n);
      }
    }
  } else {
    AFlushAudio(A->audio, A->transid, NULL, NULL);
  }

  if (A->debug > 1) Snack_WriteLog("  Exit SnackAudioFlush\n");
}

void
SnackAudioPost(ADesc *A)
{
}

int
SnackAudioRead(ADesc *A, void *buf, int nFrames)
{
  int n = read(A->Socket, buf, nFrames * A->bytesPerSample * A->nChannels);
  if (n > 0) n /= (A->bytesPerSample * A->nChannels);

  return(n);
}

int
SnackAudioWrite(ADesc *A, void *buf, int nFrames)
{
  if (A->convert) {
    int n = 0, i, res;
    unsigned char c;

    for (i = 0; i < nFrames * A->nChannels; i++) {
      c = Snack_Lin2Mulaw((short)((((unsigned char *)buf)[i] ^ 128) << 8));
      res = write(A->Socket, &c, sizeof(unsigned char));
      if (res <= 0) return(n / A->nChannels);
      n += res;
    }

    return(n / A->nChannels);
  } else {
    int n = write(A->Socket, buf, nFrames * A->bytesPerSample * A->nChannels);
    if (n > 0) n /= (A->bytesPerSample * A->nChannels);
    
    return(n);
  }
}

int
SnackAudioReadable(ADesc *A)
{
  int NBytes = 0;

  ioctl(A->Socket, FIONREAD, &NBytes);
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
  ATransStatus trans_stat;
  long status;

  trans_stat.time.type = ATTSamples;
  AGetTransStatus(A->audio, A->transid, &trans_stat, &status);
  if (trans_stat.state == ATSStopped || status != AENoError || trans_stat.time.u.samples < 0) return(0);
  /*
    if (A->last >= 0 && A->last >= trans_stat.time.u.samples) {
    A->last++;
    } else {*/
  A->last = trans_stat.time.u.samples;
  /* }*//*
     int res;
     
     res = (A->freq * (SnackCurrentTime() - A->time) +.5);
     return(res);
     */
  return(A->last);
}

void
SnackAudioInit()
{
  char *s;
  
  s = getenv("MIC");  
  if (s != NULL && strcasecmp("LINE", s) == 0) {
    hpJacks = HP_LINEIN;
  }
  
  s = getenv("SPEAKER");
  if (s != NULL && strcasecmp("INTERNAL", s) == 0) {
    hpJacks |= HP_SPEAKER;
  }
  
  a = AOpenAudio(NULL, NULL);
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

  if (a == NULL) return;

  g = (int) (g * (a->max_input_gain - a->min_input_gain) / 100.0 + a->min_input_gain + 0.5);

  ASetSystemRecordGain(a, g, (long *)NULL);
  ASetSystemChannelGain(a, ASGTRecord, ACTMono, g, (long *)NULL);
}

void
ASetPlayGain(int gain)
{
  int g = min(max(gain, 0), 100);

  if (a == NULL) return;

  g = (int) (g * (a->max_output_gain - a->min_output_gain) / 100.0 + a->min_output_gain);

  ASetSystemChannelGain(a, ASGTPlay, ACTMono, g, (long *)NULL);
}

int
AGetRecGain()
{
  long g = 0;
  
  if (a == NULL) return(0);
  
  AGetSystemChannelGain(a, ASGTRecord, ACTMono, &g, (long *)NULL);

  g = (int) ((g - a->min_input_gain) * 100.0 / (a->max_input_gain - a->min_input_gain) + 0.5);

  return(g);
}

int
AGetPlayGain()
{
  long g = 0;
  
  if (a == NULL) return(0);
  
  AGetSystemChannelGain(a, ASGTPlay, ACTMono, &g, (long *)NULL);

  g = (int) ((g - a->min_output_gain) * 100.0 / (a->max_output_gain - a->min_output_gain));

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
  if (hpJacks & HP_MIC) {
    strcpy(buf, "Microphone");
  } else if (hpJacks & HP_LINEIN) {
    strcpy(buf, "\"Line In\"");
  }
}

int
SnackMixerSetInputJack(Tcl_Interp *interp, char *jack, CONST84 char *status)
{
  int jackLen = strlen(jack), start = 0, end = 0, mask;

  while(end < jackLen) {
    while ((end < jackLen) && (!isspace(jack[end]))) end++;
    if (strncasecmp(&jack[start], "Microphone", end - start) == 0) {
      mask = HP_MIC;
    } else if (strncasecmp(&jack[start], "Line In", end - start) == 0) {
      mask = HP_LINEIN;
    }
    if (strcmp(status, "0") == 0) {
      hpJacks = (hpJacks & 0xffffffcf) | (~mask & 0x30);
    } else {
      hpJacks = (hpJacks & 0xffffffcf) | (mask & 0x30);
    }
    while ((end < jackLen) && (isspace(jack[end]))) end++;
    start = end;
  }

  return 0;
}

void
SnackMixerGetOutputJack(char *buf, int n)
{
  int pos = 0;

  if (hpJacks & HP_SPEAKER) {
    pos += sprintf(&buf[pos],  "Speaker ");
  }
  if (hpJacks & HP_LINEOUT) {
    pos += sprintf(&buf[pos],  "\"Line Out\" ");
  }
  if (hpJacks & HP_HEADPHONE) {
    pos += sprintf(&buf[pos],  "Headphones");
  }
}

void
SnackMixerSetOutputJack(char *jack, char *status)
{
  int jackLen = strlen(jack), start = 0, end = 0, mask;

  while(end < jackLen) {
    while ((end < jackLen) && (!isspace(jack[end]))) end++;
    if (strncasecmp(&jack[start], "Speaker", end - start) == 0) {
      mask = HP_SPEAKER;
    } else if (strncasecmp(&jack[start], "Line Out", end - start) == 0) {
      mask = HP_LINEOUT;
    } else if (strncasecmp(&jack[start], "Headphones", end - start) == 0) {
      mask = HP_HEADPHONE;
    }
    if (strcmp(status, "0") == 0) {
      hpJacks &= ~mask;
    } else {
      hpJacks |= mask;
    }
    while ((end < jackLen) && (isspace(jack[end]))) end++;
    start = end;
  }
}

static char *
JackVarProc(ClientData clientData, Tcl_Interp *interp, CONST84 char *name1,
	    CONST84 char *name2, int flags)
{
  MixerLink *mixLink = (MixerLink *) clientData;
  int i, recSrc = 0, status = 0;
  CONST84 char *stringValue;
  char *jackLabels[] = SNACK_JACK_LABELS;
  Tcl_Obj *obj, *var;

  for (i = 0; i < SNACK_NUMBER_JACKS; i++) {
    if (strncasecmp(mixLink->jack, jackLabels[i], strlen(mixLink->jack))
	== 0) {
      break;
    }
  }
  if ((i == 0 && hpJacks & HP_DEFAULT) ||
      (i == 1 && hpJacks & HP_SPEAKER) ||
      (i == 2 && hpJacks & HP_LINEOUT) ||
      (i == 3 && hpJacks & HP_HEADPHONE) ||
      (i == 4 && hpJacks & HP_MIC) ||
      (i == 5 && hpJacks & HP_LINEIN)) {
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
  for (i = SNACK_NUMBER_OUTJACKS; i < SNACK_NUMBER_JACKS; i++) {
    obj = Tcl_NewIntObj((hpJacks & (1 << i))?1:0);
    var = Tcl_NewStringObj(mixerLinks[i][0].jackVar, -1);
    Tcl_ObjSetVar2(interp, var, NULL, obj, TCL_GLOBAL_ONLY |TCL_PARSE_PART1);
  }

  return (char *) NULL;
}

void
SnackMixerLinkJacks(Tcl_Interp *interp, char *jack, Tcl_Obj *var)
{
  char *jackLabels[] = SNACK_JACK_LABELS;
  int i, status;
  CONST84 char *value;

  for (i = 0; i < SNACK_NUMBER_JACKS; i++) {
    if (strncasecmp(jack, jackLabels[i], strlen(jack)) == 0) {
      if ((1 << i) & hpJacks) {
	status = 1;
      } else {
	status = 0;
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
  if (strncasecmp(line, "Record", strlen(line)) == 0) {
    strncpy(buf, "Mono", n);
  } else if (strncasecmp(line, "Play", strlen(line)) == 0) {
    strncpy(buf, "Mono", n);
  } else {
    buf[0] = '\0';
  }
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
  char *mixLabels[] = SNACK_MIXER_LABELS;
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
