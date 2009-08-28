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

#ifndef _SNACK_SOUND
#define _SNACK_SOUND

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
  /* Defines for VC++, not used for unix or mingw */
  typedef __int32 int32_t;
  typedef unsigned __int32 uint32_t;
#else
#  ifdef HAVE_STDINT_H
#    include <stdint.h>
#  else
#    include <sys/types.h>
#  endif
#endif

#if defined(MAC) || defined(MAC_OSX_TCL)
#  include <string.h>
#endif
#if defined(MAC)
extern int strcasecmp (const char *str1, const char *str2);
extern int strncasecmp(const char *str1, const char *str2, size_t nchars);
#  define EXPORT(a,b) a b
#elif  defined(_MSC_VER)
#  include "windows.h"
#  include "mmsystem.h"
#  define strncasecmp strnicmp
#  define strcasecmp  stricmp
#  define EXPORT(a,b) __declspec(dllexport) a b
#else
#  define EXPORT(a,b) a b
#endif

typedef void (updateProc)(ClientData clientData, int flag);

typedef struct jkCallback {
  updateProc *proc;
  ClientData clientData;
  struct jkCallback *next;
  int id;
} jkCallback;

typedef enum {
  SNACK_NATIVE,
  SNACK_BIGENDIAN,
  SNACK_LITTLEENDIAN
} SnackEndianness;

typedef struct SnackLinkedFileInfo {
  Tcl_Channel linkCh;
  float *buffer;
  int filePos;
  int validSamples;
  int eof;
  struct Sound *sound;
} SnackLinkedFileInfo;

typedef struct Sound {
  int    samprate;
  int    encoding;
  int    sampsize;
  int    nchannels;
  int    length;
  int    maxlength;
  float  maxsamp;
  float  minsamp;
  float  abmax;
  float  **blocks;
  int    maxblks;
  int    nblks;
  int    exact;
  int    precision;
  int    writeStatus;
  int    readStatus;
  short  *tmpbuf;
  int    swap;
  int    storeType;
  int    headSize;
  int    skipBytes;
  int    buffersize;
  Tcl_Interp     *interp;
  Tcl_Obj        *cmdPtr;
  char           *fcname;
  struct jkCallback *firstCB;
  char *fileType;
  int blockingPlay;
  int debug;
  int destroy;
  int guessEncoding;
  Tcl_Channel rwchan;
  int inByteOrder;
  int firstNRead;
  int guessRate;
  int forceFormat;
  int itemRefCnt;
  int validStart;
  SnackLinkedFileInfo linkInfo;
  char *devStr;
  Tcl_HashTable *soundTable;
  char *filterName;
  char *extHead;
  char *extHead2;
  int extHeadType;
  int extHead2Type;
  int loadOffset;
  Tcl_Obj *changeCmdPtr;
  unsigned int userFlag; /* User flags, for new file formats, etc */
  char *userData;        /* User data pointer */

} Sound;

#define IDLE    0
#define READ    1
#define WRITE   2
#define PAUSED  3

#ifndef _MSC_VER
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define SNACK_NEW_SOUND	 1
#define SNACK_MORE_SOUND 2
#define SNACK_DESTROY_SOUND 3

#define SNACK_SINGLE_PREC 1
#define SNACK_DOUBLE_PREC 2

extern int Snack_SoundCmd(ClientData cdata, Tcl_Interp *interp,
			  int objc, Tcl_Obj *CONST objv[]);

extern void Snack_SoundDeleteCmd(ClientData cdata);

extern int Snack_AudioCmd(ClientData cdata, Tcl_Interp *interp,
			  int objc, Tcl_Obj *CONST objv[]);

extern void Snack_AudioDeleteCmd(ClientData cdata);

extern int Snack_MixerCmd(ClientData cdata, Tcl_Interp *interp, int objc,
			  Tcl_Obj *CONST objv[]);

extern void Snack_MixerDeleteCmd(ClientData clientData);

#define MAXNBLKS 200
#define FEXP 17
#define CEXP (FEXP+2)
#define DEXP (FEXP-1)
#define FBLKSIZE (1<<FEXP)
#define DBLKSIZE (1<<DEXP)
#define CBLKSIZE (1<<CEXP)

#define FSAMPLE(s, i) (s)->blocks[(i)>>FEXP][((i)&(FBLKSIZE-1))]
#define DSAMPLE(s, i) ((double **)(s)->blocks)[(i)>>DEXP][((i)&(DBLKSIZE-1))]

#define Snack_SetSample(s, c, i, val) \
                 if ((s)->precision == SNACK_DOUBLE_PREC) { \
                   DSAMPLE((s),(i)*(s)->nchannels+(c)) = (double) (val); \
       	         } else { \
		   FSAMPLE((s),(i)*(s)->nchannels+(c)) = (float) (val); \
	         }

#define Snack_GetSample(s, c, i) ( \
 ((s)->precision == SNACK_DOUBLE_PREC) ? \
     DSAMPLE((s), (i)*(s)->nchannels+(c)): \
     FSAMPLE(s, (i)*(s)->nchannels+(c)))

#define Snack_SetLength(s, len)        (s)->length = (len)
#define Snack_GetLength(s)             (s)->length
#define Snack_SetSampleRate(s, rate)   (s)->samprate = (rate)
#define Snack_GetSampleRate(s)         (s)->samprate
#define Snack_SetFrequency(s, rate)    (s)->samprate = (rate)
#define Snack_GetFrequency(s)          (s)->samprate
#define Snack_SetSampleEncoding(s, en) (s)->encoding = (en)
#define Snack_GetSampleEncoding(s)     (s)->encoding
#define Snack_SetSampleFormat(s, en)   (s)->encoding = (en)
#define Snack_GetSampleFormat(s)       (s)->encoding
#define Snack_SetNumChannels(s, nchan) (s)->nchannels = (nchan)
#define Snack_GetNumChannels(s)        (s)->nchannels
#define Snack_GetBytesPerSample(s)     (s)->sampsize
#define Snack_SetBytesPerSample(s, n)  (s)->sampsize = (n)
#define Snack_GetDebugFlag(s)          (s)->debug
#define Snack_SetHeaderSize(s, size)   (s)->headSize = (size)
#define Snack_GetSoundFilename(s)      (s)->fcname
#define Snack_GetSoundWriteStatus(s)   (s)->writeStatus
#define Snack_GetSoundReadStatus(s)    (s)->readStatus
#define Snack_GetSoundPrecision(s)     (s)->precision

#define SOUND_IN_MEMORY 0
#define SOUND_IN_CHANNEL  1
#define SOUND_IN_FILE     2
/* NFIRSTSAMPLES*/
#define CHANNEL_HEADER_BUFFER 20000

typedef int (soundCmd)(Sound *s, Tcl_Interp *interp, int objc,
		     Tcl_Obj *CONST objv[]);
typedef int (audioCmd)(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
typedef int (mixerCmd)(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
typedef void *Snack_CmdProc;
typedef void (soundDelCmd)(Sound *s);
typedef void (audioDelCmd)();
typedef void (mixerDelCmd)();
typedef void *Snack_DelCmdProc;

#define SNACK_SOUND_CMD	1
#define SNACK_AUDIO_CMD	2
#define SNACK_MIXER_CMD	3

#define HEADBUF 4096

extern char *LoadSound(Sound *s, Tcl_Interp *interp, Tcl_Obj *obj,
		       int startpos, int endpos);

extern int GetChannels(Tcl_Interp *interp, Tcl_Obj *obj, int *nchannels);

extern int GetEncoding(Tcl_Interp *interp, Tcl_Obj *obj, 
		       int *encoding, int *sampsize);

extern float Snack_SwapFloat(float f);

extern double Snack_SwapDouble(double d);

extern void ByteSwapSound(Sound *s);

extern void SwapIfBE(Sound *s);

extern void SwapIfLE(Sound *s);

extern int GetHeader(Sound *s, Tcl_Interp *interp, Tcl_Obj *obj);

extern int PutHeader(Sound *s, Tcl_Interp *interp, int objc,
		     Tcl_Obj *CONST objv[], int length);

extern int WriteLELong(Tcl_Channel ch, int32_t l);

extern int WriteBELong(Tcl_Channel ch, int32_t l);

extern int SetFcname(Sound *s, Tcl_Interp *interp, Tcl_Obj *obj);

extern char *NameGuessFileType(char *s);

extern void SnackDefineFileFormats(Tcl_Interp *interp);

extern int GetFileFormat(Tcl_Interp *interp, Tcl_Obj *obj, char **filetype);

extern void SnackCopySamples(Sound *dest, int to, Sound *src, int from,
			     int len);

typedef char *(guessFileTypeProc)(char *buf, int len);

typedef int  (getHeaderProc)(Sound *s, Tcl_Interp *interp, Tcl_Channel ch,
			     Tcl_Obj *obj, char *buf);

typedef char *(extensionFileTypeProc)(char *buf);

typedef int  (putHeaderProc)(Sound *s, Tcl_Interp *interp, Tcl_Channel ch,
			     Tcl_Obj *obj, int objc,
			     Tcl_Obj *CONST objv[], int length);

typedef int  (openProc)(Sound *s, Tcl_Interp *interp,Tcl_Channel *ch,
			char *mode);

typedef int  (closeProc)(Sound *s, Tcl_Interp *interp, Tcl_Channel *ch);

typedef int  (readSamplesProc)(Sound *s, Tcl_Interp *interp, Tcl_Channel ch,
			       char *inBuffer, float *outBuffer, int length);

typedef int  (writeSamplesProc)(Sound *s, Tcl_Channel ch, Tcl_Obj *obj,
				int start, int length);

typedef int  (seekProc)(Sound *s, Tcl_Interp *interp, Tcl_Channel ch,
			int position);

typedef void (freeHeaderProc)(Sound *s);

typedef int  (configureProc)(Sound *s, Tcl_Interp *interp, int objc,
			     Tcl_Obj *CONST objv[]);

/* Deprecated: SnackFileFormat */

typedef struct SnackFileFormat {
  char *formatName;
  guessFileTypeProc      *guessProc;
  getHeaderProc          *getHeaderProc;
  extensionFileTypeProc  *extProc;
  putHeaderProc          *putHeaderProc;
  openProc               *openProc;
  closeProc              *closeProc;
  readSamplesProc        *readProc;
  writeSamplesProc       *writeProc;
  seekProc               *seekProc;
  freeHeaderProc         *freeHeaderProc;
  struct SnackFileFormat *nextPtr;
} SnackFileFormat;

typedef struct Snack_FileFormat {
  char                    *name;
  guessFileTypeProc       *guessProc;
  getHeaderProc           *getHeaderProc;
  extensionFileTypeProc   *extProc;
  putHeaderProc           *putHeaderProc;
  openProc                *openProc;
  closeProc               *closeProc;
  readSamplesProc         *readProc;
  writeSamplesProc        *writeProc;
  seekProc                *seekProc;
  freeHeaderProc          *freeHeaderProc;
  configureProc           *configureProc;
  struct Snack_FileFormat *nextPtr;
} Snack_FileFormat;

extern int GuessEncoding(Sound *s, unsigned char *buf, int len);

extern char *GuessFileType(char *buf, int len, int eof);

extern int lengthCmd(Sound *s, Tcl_Interp *interp, int objc,
		     Tcl_Obj *CONST objv[]);
extern int insertCmd(Sound *s, Tcl_Interp *interp, int objc,
		     Tcl_Obj *CONST objv[]);
extern int cropCmd(Sound *s, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[]);
extern int copyCmd(Sound *s, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[]);
extern int appendCmd(Sound *s, Tcl_Interp *interp, int objc,
		     Tcl_Obj *CONST objv[]);
extern int concatenateCmd(Sound *s, Tcl_Interp *interp, int objc,
			  Tcl_Obj *CONST objv[]);
extern int cutCmd(Sound *s, Tcl_Interp *interp, int objc,
		  Tcl_Obj *CONST objv[]);
extern int convertCmd(Sound *s, Tcl_Interp *interp, int objc,
		      Tcl_Obj *CONST objv[]);
extern int dBPowerSpectrumCmd(Sound *s, Tcl_Interp *interp, int objc,
			      Tcl_Obj *CONST objv[]);
extern int powerSpectrumCmd(Sound *s, Tcl_Interp *interp, int objc,
			    Tcl_Obj *CONST objv[]);
extern int speaturesCmd(Sound *s, Tcl_Interp *interp, int objc,
			Tcl_Obj *CONST objv[]);
extern int alCmd(Sound *s, Tcl_Interp *interp, int objc,
		 Tcl_Obj *CONST objv[]);
extern int vpCmd(Sound *s, Tcl_Interp *interp, int objc,
		 Tcl_Obj *CONST objv[]);
extern int joinCmd(Sound *s, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[]);
extern int fitCmd(Sound *s, Tcl_Interp *interp, int objc,
		  Tcl_Obj *CONST objv[]);
extern int inaCmd(Sound *s, Tcl_Interp *interp, int objc,
		  Tcl_Obj *CONST objv[]);
extern int lastIndexCmd(Sound *s, Tcl_Interp *interp, int objc,
			Tcl_Obj *CONST objv[]);
extern int stretchCmd(Sound *s, Tcl_Interp *interp, int objc,
		      Tcl_Obj *CONST objv[]);
extern int ocCmd(Sound *s, Tcl_Interp *interp, int objc,
		 Tcl_Obj *CONST objv[]);
extern int arCmd(Sound *s, Tcl_Interp *interp, int objc,
		 Tcl_Obj *CONST objv[]);
extern int mixCmd(Sound *s, Tcl_Interp *interp, int objc,
		  Tcl_Obj *CONST objv[]);
extern int sampleCmd(Sound *s, Tcl_Interp *interp, int objc,
		     Tcl_Obj *CONST objv[]);
extern int flipBitsCmd(Sound *s, Tcl_Interp *interp, int objc,
		       Tcl_Obj *CONST objv[]);
extern int byteswapCmd(Sound *s, Tcl_Interp *interp, int objc,
		       Tcl_Obj *CONST objv[]);
extern int readCmd(Sound *s, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[]);
extern int writeCmd(Sound *s, Tcl_Interp *interp, int objc,
		    Tcl_Obj *CONST objv[]);
extern int dataCmd(Sound *s, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[]);
extern int playCmd(Sound *s, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[]);
extern int recordCmd(Sound *s, Tcl_Interp *interp, int objc,
		     Tcl_Obj *CONST objv[]);
extern int pauseCmd(Sound *s, Tcl_Interp *interp, int objc,
		    Tcl_Obj *CONST objv[]);
extern int stopCmd(Sound *s, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[]);
extern int current_positionCmd(Sound *s, Tcl_Interp *interp, int objc,
			       Tcl_Obj *CONST objv[]);
extern int swapCmd(Sound *s, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[]);

#define QUE_STRING "QUE"
#define RAW_STRING "RAW"
#define WAV_STRING "WAV"
#define AIFF_STRING "AIFF"
#define SMP_STRING "SMP"
#define AU_STRING "AU"
#define SD_STRING "SD"
#define MP3_STRING "MP3"
#define CSL_STRING "CSL"

extern char *GuessMP3File(char *buf, int len);

extern int GetMP3Header(Sound *s, Tcl_Interp *interp, Tcl_Channel ch,
			Tcl_Obj *obj, char *buf);

extern int ReadMP3Samples(Sound *s, Tcl_Interp *interp, Tcl_Channel ch,
			  char *ibuf, float *obuf, int len);

extern int SeekMP3File(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, int pos);

extern char *ExtMP3File(char *s);

extern int OpenMP3File(Sound *s, Tcl_Interp *interp, Tcl_Channel *ch,
		       char *mode);

extern int CloseMP3File(Sound *s, Tcl_Interp *interp, Tcl_Channel *ch);

extern void FreeMP3Header(Sound *s);

extern int ConfigMP3Header(Sound *s, Tcl_Interp *interp, int objc,
			   Tcl_Obj *CONST objv[]);

typedef enum {
  SNACK_WIN_HAMMING,
  SNACK_WIN_HANNING,
  SNACK_WIN_BARTLETT,
  SNACK_WIN_BLACKMAN,
  SNACK_WIN_RECT
} SnackWindowType;

extern int GetWindowType(Tcl_Interp *interp, char *str, SnackWindowType *type);

extern int GetChannel(Tcl_Interp *interp, char *str, int nchannels,
		      int *channel);

extern float LpcAnalysis(float *data, int N, float *f, int order);

extern void PreEmphase(float *sig, float presample, int len, float preemph);

extern double SnackCurrentTime();

extern CONST84 char *Snack_InitStubs (Tcl_Interp *interp, char *version,
				      int exact);

extern int pitchCmd(Sound *s, Tcl_Interp *interp, int objc,
		    Tcl_Obj *CONST objv[]);

extern int powerCmd(Sound *s, Tcl_Interp *interp, int objc,
		    Tcl_Obj *CONST objv[]);

extern int reverseCmd(Sound *s, Tcl_Interp *interp, int objc,
		      Tcl_Obj *CONST objv[]);

extern int formantCmd(Sound *s, Tcl_Interp *interp, int objc,
		      Tcl_Obj *CONST objv[]);

#define ITEMBUFFERSIZE 100000

extern float GetSample(SnackLinkedFileInfo *infoPtr, int index);

extern int OpenLinkedFile(Sound *s, SnackLinkedFileInfo *infoPtr);

extern void CloseLinkedFile(SnackLinkedFileInfo *infoPtr);

extern void Snack_ExitProc(ClientData clientData);

typedef enum {
  SNACK_QS_QUEUED = 0,
  SNACK_QS_PAUSED,
  SNACK_QS_DRAIN,
  SNACK_QS_DONE
} queuedSoundStatus;

typedef struct jkQueuedSound {
  Sound *sound;
  int startPos;
  int endPos;
  long nWritten;
  long startTime;
  Tcl_Obj *cmdPtr;
  queuedSoundStatus status;
  int duration;
  char *name;
  char *filterName;
  int id;
  struct jkQueuedSound *next;
  struct jkQueuedSound *prev;
} jkQueuedSound;

extern int shapeCmd(Sound *s, Tcl_Interp *interp, int objc,
		    Tcl_Obj *CONST objv[]);

extern int dataSamplesCmd(Sound *s, Tcl_Interp *interp, int objc,
			  Tcl_Obj *CONST objv[]);

extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);

extern int Snack_FilterCmd(ClientData cdata, Tcl_Interp *interp, int objc,
			   Tcl_Obj *CONST objv[]);

extern void Snack_FilterDeleteCmd(ClientData clientData);

typedef struct SnackFilter     *Snack_Filter;
typedef struct SnackStreamInfo *Snack_StreamInfo;

typedef Snack_Filter (createProc)(Tcl_Interp *interp, int objc,
				    Tcl_Obj *CONST objv[]);

typedef int (configProc)(Snack_Filter f, Tcl_Interp *interp, int objc,
			 Tcl_Obj *CONST objv[]);

typedef int (startProc)(Snack_Filter f, Snack_StreamInfo si);

typedef int (flowProc)(Snack_Filter f, Snack_StreamInfo si,
		       float *inBuffer, float *outBuffer,
		       int *inFrames, int *outFrames);

typedef void (freeProc)(Snack_Filter f);

typedef struct SnackStreamInfo {
  Sound *is1;
  Sound *is2;
  Sound *os1;
  Sound *os2;
  int streamWidth;
  int outWidth;
  int rate;
} SnackStreamInfo;

typedef struct SnackFilter {
  configProc *configProc;
  startProc  *startProc;
  flowProc   *flowProc;
  freeProc   *freeProc;
  Tcl_Interp *interp;
  Snack_Filter prev;
  Snack_Filter next;
  Snack_StreamInfo si;
  double dataRatio;
  int reserved[4];
} SnackFilter;

typedef struct Snack_FilterType {
  char               *name;
  createProc         *createProc;
  configProc         *configProc;
  startProc          *startProc;
  flowProc           *flowProc;
  freeProc           *freeProc;
  struct Snack_FilterType *nextPtr;
} Snack_FilterType;

void SnackCreateFilterTypes(Tcl_Interp *interp);

extern int Snack_HSetCmd(ClientData cdata, Tcl_Interp *interp, int objc,
			   Tcl_Obj *CONST objv[]);

extern void Snack_HSetDeleteCmd(ClientData clientData);

extern int Snack_arCmd(ClientData cdata, Tcl_Interp *interp, int objc,
			   Tcl_Obj *CONST objv[]);

extern void Snack_arDeleteCmd(ClientData clientData);

extern int isynCmd(ClientData cdata, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[]);

extern int osynCmd(ClientData cdata, Tcl_Interp *interp, int objc,
		   Tcl_Obj *CONST objv[]);

extern int WriteSound(writeSamplesProc *writeProc, Sound *s,
		      Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
		      int startpos, int len);

extern void Snack_RemoveOptions(int objc, Tcl_Obj *CONST objv[],
				CONST84 char **subOptionStrings, int *newobjc,
				Tcl_Obj **newobjv);

#ifndef MAC
#  define PBSIZE 100000
#  define NMAX 65536
#else
#  define PBSIZE 64000
#  define NMAX 8192
#endif
#define NMIN 8

extern void SnackPauseAudio();

#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 4
#define TCL_SEEK Tcl_Seek
#define TCL_TELL Tcl_Tell
#else
#define TCL_SEEK Tcl_SeekOld
#define TCL_TELL Tcl_TellOld
#endif

#define SNACK_DB 4.34294481903251830000000 /*  = 10 / ln(10)  */
#define SNACK_INTLOGARGMIN 1.0
#define SNACK_CORRN (float) 138.308998699
#define SNACK_CORR0 (float) 132.288396199

/*
 * Include the public function declarations that are accessible via
 * the stubs table.
 */

#include "snackDecls.h"
  /*
extern void Snack_StopSound(Sound *s, Tcl_Interp *interp);
extern Sound *Snack_GetSound(Tcl_Interp *interp, char *name);
extern int Snack_AddCallback(Sound *s, updateProc *proc, ClientData cd);
extern void Snack_WriteLog(char *str);
extern void Snack_WriteLogInt(char *str, int num);
extern void Snack_RemoveCallback(Sound *s, int id);
extern int Snack_ResizeSoundStorage(Sound *s, int len);
extern void Snack_UpdateExtremes(Sound *s, int start, int end, int flag);
extern int SnackOpenFile(openProc *openProc, Sound *s, Tcl_Interp *interp,
			 Tcl_Channel *ch, char *mode);
extern int SnackCloseFile(closeProc *closeProc, Sound *s, Tcl_Interp *interp,
			  Tcl_Channel *ch);
extern void Snack_ExecCallbacks(Sound *s, int flag);
extern short Snack_SwapShort(short s);
extern int32_t Snack_SwapLong(int32_t l);
extern int Snack_ProgressCallback(Tcl_Obj *cmdPtr, Tcl_Interp *interp,
				  char *type, double fraction);
extern void Snack_DeleteSound(Sound *s);
extern Sound *Snack_NewSound(int rate, int encoding, int nchannels);
extern short Snack_Mulaw2Lin(unsigned char u_val);
extern unsigned char Snack_Lin2Mulaw(short pcm_val);
extern short Snack_Alaw2Lin(unsigned char a_val);
extern unsigned char Snack_Lin2Alaw(short pcm_val);
extern void Snack_PutSoundData(Sound *s, int pos, void *buf, int nSamples);
extern void Snack_GetSoundData(Sound *s, int pos, void *buf, int nSamples);
extern void Snack_InitWindow(float *win, int winlen, int fftlen, int type);
extern int  Snack_InitFFT(int n);
extern void Snack_DBPowerSpectrum(float *x);
extern void Snack_PowerSpectrum(float *x);
extern void Snack_CreateFilterType(Snack_FilterType *typePtr);
extern int SaveSound(Sound *s, Tcl_Interp *interp, char *filename,
		     Tcl_Obj *obj, int objc, Tcl_Obj *CONST objv[],
		     int startpos, int len, char *type);
  */
#ifdef __cplusplus
}
#endif

#endif /* _SNACK_SOUND */
