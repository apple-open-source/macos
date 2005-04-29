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

#ifndef _SNACK_AUDIO
#define _SNACK_AUDIO

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONST84
#   define CONST84
#endif

#ifdef HPUX
#  include <Alib.h>
#endif

#ifdef Solaris
#  include <sys/types.h>
#  include <sys/file.h>
#  include <sys/ioctl.h>
#  include <sys/fcntl.h>
#  include <stropts.h>
#  include <sys/errno.h>
#  include <sys/audioio.h>
#  include <errno.h>
#  include <sys/filio.h>
#endif

#ifdef WIN
#  include <windows.h>
#  include <mmsystem.h>
#  include <mmreg.h>
#  include <dsound.h>
# ifdef WAVEFORMATEXTENSIBLE
#  include <ks.h>
# endif
#endif

#ifdef IRIX
#  include <audio.h>
#endif

#if defined(MAC) || defined(OS_X_CORE_AUDIO)

/* We need to temporarily redefine several symbols used by an obsolete
 *  MacOS interface as they are also used by Snack */

#  define convertCmd convertCmd_MacOS
#  define soundCmd   soundCmd_MacOS
#  define flushCmd   flushCmd_MacOS
#  define volumeCmd  volumeCmd_MacOS
#  define pauseCmd   pauseCmd_MacOS

#if defined(OS_X_CORE_AUDIO)
#undef min
#undef max
#  include <CoreServices/CoreServices.h>
#  include <CoreAudio/AudioHardware.h>
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#else
#  include <Sound.h>
#endif

#  undef convertCmd
#  undef soundCmd
#  undef flushCmd
#  undef volumeCmd
#  undef pauseCmd

#  ifndef rate44khz
#    define rate44khz ((unsigned long)(44100.0*65536))
#  endif /* !rate44khz */
/* How many buffers to maintain (2 is enough) */
#define NBUFS 2
/* The duration in seconds desired for each buffer */
/*#define DFLT_BUFTIME (0.0625) *//* i.e. frq/16, the favorite transfer size of the system */
#define DFLT_BUFTIME (0.25)	/* seems to work much better on the mac */
/* The number of SPBRecord calls to overlap.  I *think* this *has* to be zero */
#define INBUF_OVERLAP (0)

#endif /* MAC */

#ifdef ALSA
#include <alsa/asoundlib.h>
#endif

typedef struct ADesc {

#ifdef HPUX
  Audio    *audio;
  ATransID  transid;
  int       Socket;
  int       last;
  int       convert;
  double    time;
  int       freq;
#endif

#ifdef OSS
  int    afd;
  /*int    count;*/
  int    frag_size;
  double time;
  double timep;
  int    freq;
  int    convert;
  int    warm;
#endif

#ifdef ALSA
  snd_pcm_t *handle;
  int       freq;
  long      nWritten;
  long      nPlayed;
#endif

#ifdef Solaris
  int afd;
  audio_info_t ainfo;
  double time;
  double timep;
  int freq;
  int convert;
  short *convBuf;
  int convSize;
#endif

#ifdef WIN
  int curr;
  int freq;
  int shortRead;
  int convert;

  PCMWAVEFORMAT pcmwf;
  DSBUFFERDESC dsbdesc;
  DSCBUFFERDESC dscbdesc;
  LPDIRECTSOUNDBUFFER lplpDsb;
  LPDIRECTSOUNDCAPTUREBUFFER lplpDscb;
  PCMWAVEFORMAT pcmwfPB;
  DSBUFFERDESC dsbdescPB;
  LPDIRECTSOUNDBUFFER lplpDsPB;
  unsigned int BufPos;
  int BufLen;
  long written;
  long lastWritten;
#endif

#ifdef IRIX
  ALport   port;
  ALconfig config;
  unsigned long long startfn;
  int count;
#endif

#if defined(MAC)/* || defined(OS_X_CORE_AUDIO)*/
  /* Fields for handling output */
  SndChannelPtr schn;
  SndCommand	  scmd;
  SndDoubleBufferHeader2  dbh;
  SndDoubleBufferPtr	   bufs[NBUFS]; /* the two double buffers */
  int currentBuf;	/* our own track of which buf is current */
  int bufsIssued;	/* For record: how many bufs have been set going */
  int bufsCompleted;	/* For record: how many bufs have completed */
  int bufFull[NBUFS];
  long     bufFrames;	    /* number of frames allocated per buffer */
  int running;	/* flag as to whether we have started yet */
  int pause;    /* flag that we are paused (used on input only?) */
  /* data for the callbacks */
  void     *data;	    /* pointer to the base of the sampled data */
  long     totalFrames;   /* how many frames there are */
  long     doneFrames;    /* how many we have already copied */
  /* Fields for input */
  long inRefNum;	    /* MacOS reference to input channel */
  SPBPtr spb[NBUFS];	    /* ptr to the parameter blocks for recording */
  /* debug stats */
  int completedblocks;
  int underruns;
#endif /* MAC */

#ifdef OS_X_CORE_AUDIO
  AudioDeviceID	device;
  UInt32 deviceBufferSize;
  AudioStreamBasicDescription deviceFormat;
  int rpos, wpos;
  double time;
  int tot;
  int encoding;
#endif /* OS_X_CORE_AUDIO */

  int bytesPerSample;
  int nChannels;
  int mode;
  int debug;

} ADesc;

extern int SnackGetInputDevices(char **arr, int n);
extern int SnackGetOutputDevices(char **arr, int n);
extern int SnackGetMixerDevices(char **arr, int n);

extern void SnackAudioInit();
extern void SnackAudioFree();
extern int  SnackAudioOpen(ADesc *A, Tcl_Interp *interp, char *device,
			   int mode, int freq, int channels,
			   int encoding);
extern int  SnackAudioClose(ADesc *A);
extern long SnackAudioPause(ADesc *A);
extern void SnackAudioResume(ADesc *A);
extern void SnackAudioFlush(ADesc *A);
extern void SnackAudioPost(ADesc *A);
extern int  SnackAudioRead(ADesc *A, void *buf, int nSamples);
extern int  SnackAudioWrite(ADesc *A, void *buf, int nSamples);
extern int  SnackAudioReadable(ADesc *A);
extern long SnackAudioPlayed(ADesc *A);
extern int  SnackAudioWriteable(ADesc *A);

extern int SnackAudioGetEncodings(char *device);
extern void SnackAudioGetRates(char *device, char *buf, int n);
extern int SnackAudioMaxNumberChannels(char *device);
extern int SnackAudioMinNumberChannels(char *device);

extern void ASetRecGain(int gain);
extern void ASetPlayGain(int gain);
extern int  AGetRecGain();
extern int  AGetPlayGain();

extern void SnackMixerGetInputJackLabels(char *buf, int n);
extern void SnackMixerGetOutputJackLabels(char *buf, int n);
extern void SnackMixerGetInputJack(char *buf, int n);
extern int  SnackMixerSetInputJack(Tcl_Interp *interp, char *jack,
				   CONST84 char *status);
extern void SnackMixerGetOutputJack(char *buf, int n);
extern void SnackMixerSetOutputJack(char *jack, char *status);
extern void SnackMixerGetChannelLabels(char *mixer, char *buf, int n);
extern void SnackMixerGetVolume(char *mixer, int channel, char *buf, int n);
extern void SnackMixerSetVolume(char *mixer, int channel, int volume);
extern void SnackMixerGetLineLabels(char *buf, int n);
extern void SnackMixerLinkJacks(Tcl_Interp *interp, char *jack, Tcl_Obj *var);
extern void SnackMixerLinkVolume(Tcl_Interp *interp, char *mixer, int n,
			Tcl_Obj *CONST objv[]);
extern void SnackMixerUpdateVars(Tcl_Interp *interp);
extern int  SnackGetInDevices(char **arr, int n);
extern int  SnackGetOutDevices(char **arr, int n);

#define RECORD 1
#define PLAY   2

#define SNACK_MONO   1
#define SNACK_STEREO 2
#define SNACK_QUAD   4

#define LIN16        1
#define ALAW         2
#define MULAW        3
#define LIN8OFFSET   4
#define LIN8         5
#define LIN24        6
#define LIN32        7
#define SNACK_FLOAT  8
#define SNACK_DOUBLE 9
#define LIN24PACKED 10

#define CAPABLEN 100

/*#ifdef OSS
extern short Snack_Alaw2Lin(unsigned char a_val);
extern short Snack_Mulaw2Lin(unsigned char u_val);
extern unsigned char Snack_Lin2Alaw(short pcm_val);
extern unsigned char Snack_Lin2Mulaw(short pcm_val);
#endif*/
extern double SnackCurrentTime();

typedef struct MixerLink {
  char *mixer;
  char *mixerVar;
  char *jack;
  CONST84 char *jackVar;
  int channel;
} MixerLink;

#define VOLBUFSIZE 20
#define JACKBUFSIZE 40

extern char *SnackStrDup(const char *str);

#define QUERYBUFSIZE 1000
#define MAX_DEVICE_NAME_LENGTH 100
#define MAX_NUM_DEVICES 20

extern int strncasecmp(const char *s1, const char *s2, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* _SNACK_AUDIO */
