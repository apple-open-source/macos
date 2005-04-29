/* 
 * Copyright (C) 1999-2001 Claude Barras and Kare Sjolander
 */

#include <tcl.h>
#include "snack.h"
#include <sp/sphere.h>

#if defined(__WIN32__)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  undef WIN32_LEAN_AND_MEAN
#  define EXPORT(a,b) __declspec(dllexport) a b
BOOL APIENTRY
DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
  return TRUE;
}
#else
#  define EXPORT(a,b) a b
#endif

#define fail(a) { \
   Tcl_AppendResult(interp, a, NULL); \
   if (ch) sp_close((SP_FILE *)ch); \
   return TCL_ERROR; \
}

#define NIST_STRING "NIST"
#define SPHERE_STRING "SPHERE"
#define SPHERE_HEADERSIZE 1024
#define SNACK_SPHERE_INT 17

static int
GetSphereHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	      char *buf);

static char * 
GuessSphereFile(char *buf, int len)
{
  if (len < (int) strlen(NIST_STRING)) return(QUE_STRING);
  if (strncasecmp(NIST_STRING, buf, strlen(NIST_STRING)) == 0) {
    return(SPHERE_STRING);
  }
  return(NULL);
}

char *
ExtSphereFile(char *s)
{
  int l1 = strlen(".sph");
  int l2 = strlen(s);

  if (strncasecmp(".sph", &s[l2 - l1], l1) == 0) {
    return(SPHERE_STRING);
  }
  return(NULL);
}

#define SPHERE_BUFFER_SIZE 100000

static int
OpenSphereFile(Sound *s, Tcl_Interp *interp, Tcl_Channel *ch, char *mode)
{
   /* open sphere file  */
  *ch = (Tcl_Channel) sp_open(Snack_GetSoundFilename(s), mode);
  if (*ch == NULL) {
    Tcl_AppendResult(interp, "SPHERE: unable to open file: ",
		     Snack_GetSoundFilename(s), NULL);
    return TCL_ERROR;
  }
  /*
    Hack for the pculaw format. Read header again because
    sp_set_data_mode() has to be called.
    */
  GetSphereHeader(s, interp, *ch, NULL, NULL);

  if (s->extHead != NULL && s->extHeadType != SNACK_SPHERE_INT) {
    Snack_FileFormat *ff;
    
    for (ff = Snack_GetFileFormats(); ff != NULL; ff = ff->nextPtr) {
      if (strcmp(s->fileType, ff->name) == 0) {
	if (ff->freeHeaderProc != NULL) {
	  (ff->freeHeaderProc)(s);
	}
      }
    }
  }
  
  if (s->extHead == NULL) {
    s->extHead = ckalloc(sizeof(short) * SPHERE_BUFFER_SIZE);
    s->extHeadType = SNACK_SPHERE_INT;
  }

  return TCL_OK;
}

static int
CloseSphereFile(Sound *s, Tcl_Interp *interp, Tcl_Channel *ch)
{
  /* close */
  if (sp_close((SP_FILE *)*ch))
    fail("SPHERE: error closing file");
  *ch = NULL;

  return TCL_OK;
}

static int
ReadSphereSamples(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, char *ibuf,
		  float *obuf, int len)
{
  int tot = len / Snack_GetNumChannels(s);
  int i = 0, le = Snack_PlatformIsLittleEndian();
  unsigned char *q = (unsigned char *) s->extHead;
  char *sc  = (char *)  s->extHead;
  short *r  = (short *) s->extHead;
  int   *is = (int *)   s->extHead;
  float *fs = (float *) s->extHead;
  float *f  = obuf;
  int size = min(tot, SPHERE_BUFFER_SIZE / Snack_GetNumChannels(s));
  int read = sp_read_data(s->extHead, size, (SP_FILE *)ch);

  if (!(sp_error((SP_FILE *)ch) == 0 || sp_error((SP_FILE *)ch) == 101)) {
    return -1;
  }
  
  for (i = 0; i < read * Snack_GetNumChannels(s); i++) {
    switch (s->encoding) {
    case LIN16:
      if (s->swap) *r = Snack_SwapShort(*r);
      *f++ = (float) *r++;
      break;
    case LIN32:
      if (s->swap) *is = Snack_SwapLong(*is);
      *f++ = (float) *is++;
      break;
    case SNACK_FLOAT:
      if (s->swap) *fs = (float) Snack_SwapLong((int)*fs);
      *f++  = (float) *fs++;
      break;
    case ALAW:
      *f++ = (float) Snack_Alaw2Lin(*q++);
      break;
    case MULAW:
      *f++ = (float) Snack_Mulaw2Lin(*q++);
      break;
    case LIN8:
      *f++ = (float) *sc++;
      break;
    case LIN8OFFSET:
      *f++ = (float) *q++;
      break;
    case LIN24:
      {
	int ee;
	if (s->swap) {
	  if (le) {
	    ee = 0;
	  } else {
	    ee = 1;
	  }
	} else {
	  if (le) {
	    ee = 1;
	  } else {
	    ee = 0;
	  }		
	}
	if (ee) {
	  int t = *q++;
	  t |= *q++ << 8;
	  t |= *q++ << 16;
	  if (t & 0x00800000) {
	    t |= (unsigned int) 0xff000000;
	  }
	  *f++ = (float) t;
	} else {
	  int t = *q++ << 16;
	  t |= *q++ << 8;
	  t |= *q++;
	  if (t & 0x00800000) {
	    t |= (unsigned int) 0xff000000;
	  }
	  *f++ = (float) t;
	}
	break;
      }
    }
  }

  return(i);
}

static int
SeekSphereFile(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, int pos)
{
  /* seek to pos */

  if (sp_seek((SP_FILE *)ch, pos, 0)) {
    return(-1);
  } else {
    return(pos);
  }
}

static int
GetSphereHeader(Sound *s, Tcl_Interp *interp, Tcl_Channel ch, Tcl_Obj *obj,
	      char *buf)
{
   /* SPHERE file and header fields */
   long sample_rate = 16000;
   long channel_count = 1;
   long sample_n_bytes = 2;
   long sample_cnt = 0;
   char *sample_coding = "";

   if (obj != NULL)
      fail("'data' subcommand forbidden for NIST/SPHERE format");

   if (Snack_GetDebugFlag(s) > 2) {
     Snack_WriteLog("    Reading NIST/SPHERE header\n");
   }

   /* sample_rate */
   if (sp_h_get_field((SP_FILE *)ch, "sample_rate", T_INTEGER,
		       (void *)&sample_rate) > 0)
      fail("SPHERE: unable to read sample_rate");

   Snack_SetSampleRate(s, sample_rate);
   if (Snack_GetDebugFlag(s) > 3) {
     Snack_WriteLogInt("      Setting rate", Snack_GetSampleRate(s));
   }

   /* sample_n_bytes */
   if (sp_h_get_field((SP_FILE *)ch, "sample_n_bytes", T_INTEGER,
		       (void *)&sample_n_bytes) > 0)
      fail("SPHERE: unable to read sample_n_bytes");
   Snack_SetBytesPerSample(s, sample_n_bytes);
   if (Snack_GetDebugFlag(s) > 3) {
     Snack_WriteLogInt("      Setting sampsize", Snack_GetBytesPerSample(s));
   }
   
   /* channel_count */
   if (sp_h_get_field((SP_FILE *)ch, "channel_count", T_INTEGER,
		       (void *)&channel_count) > 0)
      fail("SPHERE: unable to read channel_count");
   Snack_SetNumChannels(s, channel_count);
   if (Snack_GetDebugFlag(s) > 3) {
     Snack_WriteLogInt("      Setting channels", Snack_GetNumChannels(s));
   }

   /* sample_count */
   if (sp_h_get_field((SP_FILE *)ch, "sample_count", T_INTEGER,
		       (void *)&sample_cnt) > 0) {
      sample_cnt = 0;
   }
   if (Snack_GetDebugFlag(s) > 3) {
     Snack_WriteLogInt("      Setting length", sample_cnt);
   }

   /* sample_coding */
   if (sp_h_get_field((SP_FILE *)ch, "sample_coding", T_STRING,
                       (void *)&sample_coding) > 0) {
      sample_coding = "";
   }
   if (strncmp(sample_coding, "pculaw", 6) == 0) {
      sp_set_data_mode((SP_FILE *)ch, "SE-PCM-2");
      Snack_SetSampleEncoding(s, LIN16);
      Snack_SetBytesPerSample(s, 2);
   } else if (strncmp(sample_coding, "alaw", 4) == 0) {
      Snack_SetSampleEncoding(s, ALAW);
   } else if (strncmp(sample_coding, "ulaw", 4) == 0) {
      Snack_SetSampleEncoding(s, MULAW);
   } else if (strncmp(sample_coding, "pcm", 3) == 0 || sample_coding == "") {
      if (Snack_GetBytesPerSample(s) == 2) {
         Snack_SetSampleEncoding(s, LIN16);
      } else {
         Snack_SetSampleEncoding(s, LIN8);
      }
   }
   if (sample_coding != "") {
     free(sample_coding);
   }

   /* header size shouldn't be needed by user,
      so it is not given directly by SPHERE user interface */

   Snack_SetHeaderSize(s, SPHERE_HEADERSIZE);
   Snack_SetLength(s, sample_cnt);

   return TCL_OK;
}

void
FreeSphereHeader(Sound *s)
{
  if (s->extHead != NULL) {
    ckfree((char *) s->extHead);
    s->extHead = NULL;
    s->extHeadType = 0;
  }
}

#define SPHEREFILE_VERSION "1.2"

Snack_FileFormat snackSphFormat = {
  SPHERE_STRING,
  GuessSphereFile,
  GetSphereHeader,
  ExtSphereFile,
  NULL,
  OpenSphereFile,
  CloseSphereFile,
  ReadSphereSamples,
  NULL,
  SeekSphereFile,
  FreeSphereHeader,
  NULL,
  (Snack_FileFormat *) NULL
};

/* Called by "load libsnacksphere" */
EXPORT(int, Snacksphere_Init) _ANSI_ARGS_((Tcl_Interp *interp))
{
  int res;
  
#ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(interp, "8", 0) == NULL) {
    return TCL_ERROR;
  }
#endif
  
#ifdef USE_SNACK_STUBS
  if (Snack_InitStubs(interp, "2", 0) == NULL) {
    return TCL_ERROR;
  }
#endif
  
  res = Tcl_PkgProvide(interp, "snacksphere", SPHEREFILE_VERSION);
  
  if (res != TCL_OK) return res;

  Tcl_SetVar(interp, "snack::snacksphere", SPHEREFILE_VERSION,TCL_GLOBAL_ONLY);

  Snack_CreateFileFormat(&snackSphFormat);

  return TCL_OK;
}

EXPORT(int, Snacksphere_SafeInit)(Tcl_Interp *interp)
{
  return Snacksphere_Init(interp);
}
