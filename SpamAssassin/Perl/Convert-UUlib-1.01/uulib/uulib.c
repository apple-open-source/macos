/*
 * This file is part of uudeview, the simple and friendly multi-part multi-
 * file uudecoder  program  (c) 1994-2001 by Frank Pilhofer. The author may
 * be contacted at fp@fpx.de
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
 */

/*
 * This file implements the externally visible functions, as declared
 * in uudeview.h, and some internal interfacing functions
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef SYSTEM_WINDLL
#include <windows.h>
#endif
#ifdef SYSTEM_OS2
#include <os2.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#else
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#else
#ifdef HAVE_VARARGS_H
#include <varargs.h>
#endif
#endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

/* to get open() in Windows */
#ifdef HAVE_IO_H
#include <io.h>
#endif

#include <uudeview.h>
#include <uuint.h>
#include <fptools.h>
#include <uustring.h>

char * uulib_id = "$Id: uulib.c,v 1.1 2004/04/19 17:50:27 dasenbro Exp $";

#ifdef SYSTEM_WINDLL
BOOL _export WINAPI 
DllEntryPoint (HINSTANCE hInstance, DWORD seginfo,
	       LPVOID lpCmdLine)
{
  /* Don't do anything, so just return true */
  return TRUE;
}
#endif

/*
 * In DOS, we must open the file binary, O_BINARY is defined there
 */

#ifndef O_BINARY
#define O_BINARY      0
#endif

/* for braindead systems */
#ifndef SEEK_SET
#ifdef L_BEGIN
#define SEEK_SET L_BEGIN
#else
#define SEEK_SET 0
#endif
#endif

/*
 * Callback functions and their opaque arguments
 */

void   (*uu_MsgCallback)     (void *, char *, int)         = NULL;
int    (*uu_BusyCallback)    (void *, uuprogress *)        = NULL;
int    (*uu_FileCallback)    (void *, char *, char *, int) = NULL;
char * (*uu_FNameFilter)     (void *, char *)              = NULL;
char * (*uu_FileNameCallback)(void *, char *, char *);
;
void *uu_MsgCBArg  = NULL;
void *uu_BusyCBArg = NULL;
void *uu_FileCBArg = NULL;
void *uu_FFCBArg   = NULL;
void *uu_FNCBArg;

/*
 * Global variables
 */

int uu_fast_scanning = 0;	/* assumes at most 1 part per file          */
int uu_bracket_policy = 0;	/* gives part numbers in [] higher priority */
int uu_verbose = 1;		/* enables/disables messages&notes          */
int uu_desperate = 0;		/* desperate mode                           */
int uu_ignreply = 0;		/* ignore replies                           */
int uu_debug = 0;		/* debugging mode (print __FILE__/__LINE__) */
int uu_errno = 0;		/* the errno that caused this UURET_IOERR   */
int uu_dumbness = 0;		/* switch off the program's intelligence    */
int uu_overwrite = 1;		/* whether it's ok to overwrite ex. files   */
int uu_ignmode = 0;		/* ignore the original file mode            */
int uu_handletext = 0;		/* do we want text/plain messages           */
int uu_usepreamble = 0;		/* do we want Mime preambles/epilogues      */
int uu_tinyb64 = 0;		/* detect short B64 outside of MIME         */
int uu_remove_input = 0;        /* remove input files after decoding        */
int uu_more_mime = 0;           /* strictly adhere to MIME headers          */
int uu_dotdot = 0;		/* dot-unescaping has not yet been done     */

headercount hlcount = {
  3,			        /* restarting after a MIME body             */
  2,                            /* after useful data in freestyle mode      */
  1                             /* after useful data and an empty line      */
};

/*
 * version string
 */

char uulibversion[256] = VERSION "pl" PATCH;

/*
 * prefix to the files on disk, usually a path name to save files to
 */

char *uusavepath;

/*
 * extension to use when encoding single-part files
 */

char *uuencodeext;

/*
 * areas to malloc
 */

char *uulib_msgstring;
char *uugen_inbuffer;
char *uugen_fnbuffer;

/*
 * The Global List of Files
 */

uulist *UUGlobalFileList = NULL;

/*
 * time values for BusyCallback. msecs is MILLIsecs here
 */

static long uu_busy_msecs = 0;	/* call callback function each msecs */
static long uu_last_secs  = 0;	/* secs  of last call to callback */
static long uu_last_usecs = 0;	/* usecs of last call to callback */

/*
 * progress information
 */

uuprogress progress;

/*
 * Linked list of files we want to delete after decoding
 */

typedef struct _itbd {
  char *fname;
  struct _itbd *NEXT;
} itbd;
static itbd * ftodel = NULL;

/*
 * for the busy poll
 */

unsigned long uuyctr;

/*
 * Areas to allocate. Instead of using static memory areas, we malloc()
 * the memory in UUInitialize() and release them in UUCleanUp to prevent
 * blowing up of the binary size
 * This is a table with the pointers to allocate and required sizes.
 * They are guaranteed to be never NULL.
 */

typedef struct {
  char **ptr;
  size_t size;
} allomap;

static allomap toallocate[] = {
  { &uugen_fnbuffer,    4096 },  /* generic filename buffer */
  { &uugen_inbuffer,    1024 },  /* generic input data buffer */
  { &uucheck_lastname,   256 },	 /* from uucheck.c */
  { &uucheck_tempname,   256 },
  { &uuestr_itemp,       256 },  /* from uuencode.c:UUEncodeStream() */
  { &uuestr_otemp,      1024 },
  { &uulib_msgstring,   1024 },  /* from uulib.c:UUMessage() */
  { &uuncdl_fulline,    1200 },  /* from uunconc.c:UUDecodeLine() */
  { &uuncdp_oline,      3600 },  /* from uunconc.c:UUDecodePart() */
  { &uunconc_UUxlat,     256 * sizeof (int) },  /* from uunconc.c:toplevel */
  { &uunconc_UUxlen,      64 * sizeof (int) },
  { &uunconc_B64xlat,    256 * sizeof (int) },
  { &uunconc_XXxlat,     256 * sizeof (int) },
  { &uunconc_BHxlat,     256 * sizeof (int) },
  { &uunconc_save,    3*1200 },  /* from uunconc.c:decoding buffer */
  { &uuscan_shlline,    1024 },  /* from uuscan.c:ScanHeaderLine() */
  { &uuscan_shlline2,   1024 },  /* from uuscan.c:ScanHeaderLine() */
  { &uuscan_pvvalue,     300 },  /* from uuscan.c:ParseValue() */
  { &uuscan_phtext,      300 },  /* from uuscan.c:ParseHeader() */
  { &uuscan_sdline,      300 },  /* from uuscan.c:ScanData() */
  { &uuscan_sdbhds1,     300 },
  { &uuscan_sdbhds2,     300 },
  { &uuscan_spline,      300 },  /* from uuscan.c:ScanPart() */
  { &uuutil_bhwtmp,      300 },  /* from uuutil.c:UUbhwrite() */
  { NULL, 0 }
};

/*
 * Handle the printing of messages. Works like printf.
 */

#if defined(STDC_HEADERS) || defined(HAVE_STDARG_H)
int
UUMessage (char *file, int line, int level, char *format, ...)
#else
int
UUMessage (va_alist)
  va_dcl
#endif
{
  char *msgptr;
#if defined(STDC_HEADERS) || defined(HAVE_STDARG_H)
  va_list ap;

  va_start (ap, format);
#else
  char *file, *format;
  int   line, level;
  va_list ap;

  va_start (ap);
  file   = va_arg (ap, char *);
  line   = va_arg (ap, int);
  level  = va_arg (ap, int);
  format = va_arg (ap, char *);
#endif

  if (uu_debug) {
    sprintf (uulib_msgstring, "%s(%d): %s", file, line, msgnames[level]);
    msgptr = uulib_msgstring + strlen (uulib_msgstring);
  }
  else {
    sprintf (uulib_msgstring, "%s", msgnames[level]);
    msgptr = uulib_msgstring + strlen (uulib_msgstring);
  }

  if (uu_MsgCallback && (level>UUMSG_NOTE || uu_verbose)) {
    vsprintf (msgptr, format, ap);

    (*uu_MsgCallback) (uu_MsgCBArg, uulib_msgstring, level);
  }

  va_end (ap);

  return UURET_OK;
}

/*
 * Call the Busy Callback from time to time. This function must be
 * polled from the Busy loops.
 */

int
UUBusyPoll (void)
{
#ifdef HAVE_GETTIMEOFDAY
  struct timeval tv;
  long msecs;

  if (uu_BusyCallback) {
    (void) gettimeofday (&tv, NULL);

    msecs = 1000*(tv.tv_sec-uu_last_secs)+(tv.tv_usec-uu_last_usecs)/1000;

    if (uu_last_secs==0 || msecs > uu_busy_msecs) {
      uu_last_secs  = tv.tv_sec;
      uu_last_usecs = tv.tv_usec;

      return (*uu_BusyCallback) (uu_BusyCBArg, &progress);
    }
  }
#else
  time_t now;
  long msecs;

  if (uu_BusyCallback) {
    if (uu_busy_msecs <= 0) {
      msecs = 1;
    }
    else {
      now   = time(NULL);
      msecs = 1000 * (now - uu_last_secs);
    }

    if (uu_last_secs==0 || msecs > uu_busy_msecs) {
      uu_last_secs  = now;
      uu_last_usecs = 0;

      return (*uu_BusyCallback) (uu_BusyCBArg, &progress);
    }
  }
#endif

  return 0;
}

/*
 * Initialization function
 */

int UUEXPORT
UUInitialize (void)
{
  allomap *aiter;

  progress.action     = 0;
  progress.curfile[0] = '\0';

  ftodel = NULL;

  uusavepath  = NULL;
  uuencodeext = NULL;

  mssdepth = 0;
  memset (&localenv, 0, sizeof (headers));
  memset (&sstate,   0, sizeof (scanstate));

  nofnum    = 0;
  mimseqno  = 0;
  lastvalid = 0;
  lastenc   = 0;
  uuyctr    = 0;

  /*
   * Allocate areas
   */

  for (aiter=toallocate; aiter->ptr; aiter++)
    *(aiter->ptr) = NULL;

  for (aiter=toallocate; aiter->ptr; aiter++) {
    if ((*(aiter->ptr) = (char *) malloc (aiter->size)) == NULL) {
      /*
       * oops. we may not print a message here, because we need these
       * areas (uulib_msgstring) in UUMessage()
       */
      for (aiter=toallocate; aiter->ptr; aiter++) {
	_FP_free (*(aiter->ptr));
      }
      return UURET_NOMEM;
    }
  }

  /*
   * Must be called after areas have been malloced
   */

  UUInitConc ();

  return UURET_OK;
}

/*
 * Set and get Options
 */

int UUEXPORT
UUGetOption (int option, int *ivalue, char *cvalue, int clength)
{
  int result;

  switch (option) {
  case UUOPT_VERSION:
    _FP_strncpy (cvalue, uulibversion, clength);
    result = 0;
    break;
  case UUOPT_FAST:
    if (ivalue) *ivalue = uu_fast_scanning;
    result = uu_fast_scanning;
    break;
  case UUOPT_DUMBNESS:
    if (ivalue) *ivalue = uu_dumbness;
    result = uu_dumbness;
    break;
  case UUOPT_BRACKPOL:
    if (ivalue) *ivalue = uu_bracket_policy;
    result = uu_bracket_policy;
    break;
  case UUOPT_VERBOSE:
    if (ivalue) *ivalue = uu_verbose;
    result = uu_verbose;
    break;
  case UUOPT_DESPERATE:
    if (ivalue) *ivalue = uu_desperate;
    result = uu_desperate;
    break;
  case UUOPT_IGNREPLY:
    if (ivalue) *ivalue = uu_ignreply;
    result = uu_ignreply;
    break;
  case UUOPT_DEBUG:
    if (ivalue) *ivalue = uu_debug;
    result = uu_debug;
    break;
  case UUOPT_ERRNO:
    if (ivalue) *ivalue = uu_errno;
    result = uu_errno;
    break;
  case UUOPT_OVERWRITE:
    if (ivalue) *ivalue = uu_overwrite;
    result = uu_overwrite;
    break;
  case UUOPT_SAVEPATH:
    _FP_strncpy (cvalue, uusavepath, clength);
    result = 0;
    break;
  case UUOPT_PROGRESS:
    if (clength==sizeof(uuprogress)) {
      memcpy (cvalue, &progress, sizeof (uuprogress));
      result = 0;
    }
    else
      result = -1;
    break;
  case UUOPT_IGNMODE:
    if (ivalue) *ivalue = uu_ignmode;
    result = uu_ignmode;
    break;
  case UUOPT_USETEXT:
    if (ivalue) *ivalue = uu_handletext;
    result = uu_handletext;
    break;
  case UUOPT_PREAMB:
    if (ivalue) *ivalue = uu_usepreamble;
    result = uu_usepreamble;
    break;
  case UUOPT_TINYB64:
    if (ivalue) *ivalue = uu_tinyb64;
    result = uu_tinyb64;
    break;
  case UUOPT_ENCEXT:
    _FP_strncpy (cvalue, uuencodeext, clength);
    result = 0;
    break;
  case UUOPT_REMOVE:
    if (ivalue) *ivalue = uu_remove_input;
    result = uu_remove_input;
    break;
  case UUOPT_MOREMIME:
    if (ivalue) *ivalue = uu_more_mime;
    result = uu_more_mime;
    break;
  case UUOPT_DOTDOT:
    if (ivalue) *ivalue = uu_dotdot;
    result = uu_dotdot;
    break;
  default:
    return -1;
  }
  return result;
}

int UUEXPORT
UUSetOption (int option, int ivalue, char *cvalue)
{
  switch (option) {
  case UUOPT_FAST:
    uu_fast_scanning  = ivalue;
    break;
  case UUOPT_DUMBNESS:
    uu_dumbness       = ivalue;
    break;
  case UUOPT_BRACKPOL:
    uu_bracket_policy = ivalue;
    break;
  case UUOPT_VERBOSE:
    uu_verbose        = ivalue;
    break;
  case UUOPT_DESPERATE:
    uu_desperate      = ivalue;
    break;
  case UUOPT_IGNREPLY:
    uu_ignreply       = ivalue;
    break;
  case UUOPT_DEBUG:
    uu_debug          = ivalue;
    break;
  case UUOPT_OVERWRITE:
    uu_overwrite      = ivalue;
    break;
  case UUOPT_SAVEPATH:
    _FP_free (uusavepath);
    uusavepath = _FP_strdup (cvalue);
    break;
  case UUOPT_IGNMODE:
    uu_ignmode = ivalue;
    break;
  case UUOPT_USETEXT:
    uu_handletext = ivalue;
    break;
  case UUOPT_PREAMB:
    uu_usepreamble = ivalue;
    break;
  case UUOPT_TINYB64:
    uu_tinyb64 = ivalue;
    break;
  case UUOPT_ENCEXT:
    _FP_free (uuencodeext);
    uuencodeext = _FP_strdup (cvalue);
    break;
  case UUOPT_REMOVE:
    uu_remove_input = ivalue;
    break;
  case UUOPT_MOREMIME:
    uu_more_mime = ivalue;
    break;
  case UUOPT_DOTDOT:
    uu_dotdot = ivalue;
    break;
  default:
    return UURET_ILLVAL;
  }
  return UURET_OK;
}

char * UUEXPORT
UUstrerror (int code)
{
  return uuretcodes[code];
}

/*
 * Set the various Callback functions
 */

int UUEXPORT
UUSetMsgCallback (void *opaque, 
		  void (*func) (void *, char *, int))
{
  uu_MsgCallback = func;
  uu_MsgCBArg    = opaque;

  return UURET_OK;
}

int UUEXPORT
UUSetBusyCallback (void *opaque,
		   int (*func) (void *, uuprogress *),
		   long msecs)
{
  uu_BusyCallback = func;
  uu_BusyCBArg    = opaque;
  uu_busy_msecs   = msecs;

  return UURET_OK;
}

int UUEXPORT
UUSetFileCallback (void *opaque,
		   int (*func) (void *, char *, char *, int))
{
  uu_FileCallback = func;
  uu_FileCBArg    = opaque;

  return UURET_OK;
}

int UUEXPORT
UUSetFNameFilter (void *opaque,
		  char * (*func) (void *, char *))
{
  uu_FNameFilter = func;
  uu_FFCBArg     = opaque;

  return UURET_OK;
}

int UUEXPORT
UUSetFileNameCallback (void *opaque,
		       char * (*func) (void *, char *, char *))
{
  uu_FileNameCallback = func;
  uu_FNCBArg          = opaque;

  return UURET_OK;
}

/*
 * Return a pointer to the nth element of the GlobalFileList
 * zero-based, returns NULL if item is too large.
 */

uulist * UUEXPORT
UUGetFileListItem (int item)
{
  uulist *iter=UUGlobalFileList;

  if (item < 0)
    return NULL;
  while (item && iter) {
    iter = iter->NEXT;
    item--;
  }
  return iter;
}

/*
 * call the current filter
 */

char * UUEXPORT
UUFNameFilter (char *fname)
{
  if (uu_FNameFilter)
    return (*uu_FNameFilter) (uu_FFCBArg, fname);

  return fname;
}

/*
 * Load a File. We call ScanPart repeatedly until at EOF and
 * add the parts to UUGlobalFileList
 */

int UUEXPORT
UULoadFile (char *filename, char *fileid, int delflag, int *partcount)
{
  return UULoadFileWithPartNo(filename, fileid, delflag, -1, partcount);
}

int UUEXPORT
UULoadFileWithPartNo (char *filename, char *fileid, int delflag, int partno, int *partcount)
{
  int res, sr, count=0;
  struct stat finfo;
  fileread *loaded;
  uufile *fload;
  itbd *killem;
  FILE *datei;

  int _count;
  if (!partcount)
    partcount = &_count;
  
  *partcount = 0;

  if ((datei = fopen (filename, "rb")) == NULL) {
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_NOT_OPEN_SOURCE),
	       filename, strerror (uu_errno = errno));
    return UURET_IOERR;
  }

  if (fstat (fileno(datei), &finfo) == -1) {
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_NOT_STAT_FILE),
	       filename, strerror (uu_errno = errno));
    fclose (datei);
    return UURET_IOERR;
  }

  /*
   * schedule for destruction
   */

  if (delflag && fileid==NULL) {
    if ((killem = (itbd *) malloc (sizeof (itbd))) == NULL) {
      UUMessage (uulib_id, __LINE__, UUMSG_WARNING,
		 uustring (S_OUT_OF_MEMORY), sizeof (itbd));
    }
    else if ((killem->fname = _FP_strdup (filename)) == NULL) {
      UUMessage (uulib_id, __LINE__, UUMSG_WARNING,
		 uustring (S_OUT_OF_MEMORY), strlen(filename)+1);
      _FP_free (killem);
    }
    else {
      killem->NEXT = ftodel;
      ftodel = killem;
    }
  }

  progress.action   = 0;
  progress.partno   = 0;
  progress.numparts = 1;
  progress.fsize    = (long) ((finfo.st_size>0)?finfo.st_size:-1);
  progress.percent  = 0;
  progress.foffset  = 0;
  _FP_strncpy (progress.curfile,
	       (strlen(filename)>255)?
	       (filename+strlen(filename)-255):filename,
	       256);
  progress.action   = UUACT_SCANNING;

  if (fileid == NULL)
    fileid = filename;

  while (!feof (datei) && !ferror (datei)) {
    /* 
     * Peek file, or some systems won't detect EOF
     */
    res = fgetc (datei);
    if (feof (datei) || ferror (datei))
      break;
    else
      ungetc (res, datei);
    
    if ((loaded = ScanPart (datei, fileid, &sr)) == NULL) {
      if (sr != UURET_NODATA && sr != UURET_OK && sr != UURET_CONT) {
	UUkillfread (loaded);
	if (sr != UURET_CANCEL)
	  UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
		     uustring (S_READ_ERROR), filename,
		     strerror (uu_errno));

	UUCheckGlobalList ();
	progress.action = 0;
	fclose (datei);
	return sr;
      }
      continue;
    }

    if (ferror (datei)) {
      UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
		 uustring (S_READ_ERROR), filename,
		 strerror (uu_errno = errno));
      UUCheckGlobalList ();
      progress.action = 0;
      fclose (datei);
      return UURET_IOERR;
    }

    if (partno != -1)
      loaded->partno = partno;

    if ((loaded->uudet == QP_ENCODED || loaded->uudet == PT_ENCODED) &&
	(loaded->filename == NULL || *(loaded->filename) == '\0') &&
	!uu_handletext && (loaded->flags&FL_PARTIAL)==0) {
      /*
       * Don't want text
       */
      UUkillfread (loaded);
      continue;
    }

    if ((loaded->subject == NULL || *(loaded->subject) == '\0') &&
	(loaded->mimeid  == NULL || *(loaded->mimeid)  == '\0') &&
	(loaded->filename== NULL || *(loaded->filename)== '\0') &&
	(loaded->uudet   == 0)) {
      /*
       * no useful data here
       */
      UUkillfread (loaded);
      if (uu_fast_scanning && sr != UURET_CONT) break;
      continue;
    }
    
    if ((fload = UUPreProcessPart (loaded, &res)) == NULL) {
      /*
       * no useful data found
       */
      if (res != UURET_NODATA) {
	UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
		   uustring (S_READ_ERROR), filename,
		   (res==UURET_IOERR)?strerror(uu_errno):UUstrerror(res));
      }
      UUkillfread (loaded);
      if (uu_fast_scanning && sr != UURET_CONT) break;
      continue;
    }

    if ((loaded->subject && *(loaded->subject)) ||
	(loaded->mimeid  && *(loaded->mimeid))  ||
	(loaded->filename&& *(loaded->filename))||
	(loaded->uudet)) {
      UUMessage (uulib_id, __LINE__, UUMSG_MESSAGE,
		 uustring (S_LOADED_PART),
		 filename,
		 (loaded->subject)  ? loaded->subject  : "",
		 (fload->subfname)  ? fload->subfname  : "",
		 (loaded->filename) ? loaded->filename : "",
		 fload->partno,
		 (loaded->begin)    ? "begin" : "",
		 (loaded->end)      ? "end"   : "",
		 codenames[loaded->uudet]);
    }
    
    if ((res = UUInsertPartToList (fload))) {
      /*
       * couldn't use the data
       */
      UUkillfile (fload);

      if (res != UURET_NODATA) {
	UUCheckGlobalList ();
	progress.action = 0;
	fclose (datei);
	return res;
      }
      if (uu_fast_scanning && sr != UURET_CONT)
	break;

      continue;
    }

    /*
     * if in fast mode, we don't look any further, because we're told
     * that each source file holds at most one encoded part
     */

    if (loaded->uudet)
      (*partcount)++;

    if (uu_fast_scanning && sr != UURET_CONT)
      break;
  }
  if (ferror (datei)) {
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_READ_ERROR), filename,
	       strerror (uu_errno = errno));
    UUCheckGlobalList ();
    progress.action = 0;
    fclose (datei);
    return UURET_IOERR;
  }
  fclose (datei);

  if (!uu_fast_scanning && *partcount == 0)
    UUMessage (uulib_id, __LINE__, UUMSG_NOTE,
	       uustring (S_NO_DATA_FOUND), filename);

  progress.action = 0;
  UUCheckGlobalList ();

  return UURET_OK;
}

/*
 * decode to a temporary file. this is well handled by uudecode()
 */

int UUEXPORT
UUDecodeToTemp (uulist *thefile)
{
  return UUDecode (thefile);
}

/*
 * decode file first to temp file, then copy it to a final location
 */

int UUEXPORT
UUDecodeFile (uulist *thefile, char *destname)
{
  FILE *target, *source;
  struct stat finfo;
  int fildes, res;
  size_t bytes;

  if (thefile == NULL)
    return UURET_ILLVAL;

  if ((res = UUDecode (thefile)) != UURET_OK)
    if (res != UURET_NOEND || !uu_desperate)
      return res;

  if (thefile->binfile == NULL) {
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_NO_BIN_FILE));
    return UURET_IOERR;
  }

  if ((source = fopen (thefile->binfile, "rb")) == NULL) {
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_NOT_OPEN_FILE),
	       thefile->binfile, strerror (uu_errno = errno));
    return UURET_IOERR;
  }

  /*
   * for system security, strip setuid/setgid bits from mode
   */

  if ((thefile->mode & 0777) != thefile->mode) {
    UUMessage (uulib_id, __LINE__, UUMSG_NOTE,
	       uustring (S_STRIPPED_SETUID),
	       destname, (int)thefile->mode);
    thefile->mode &= 0777;
  }

  /*
   * Determine the name of the target file according to the rules:
   * 
   * IF (destname!=NULL) THEN filename=destname;
   * ELSE
   *   filename = thefile->filename
   *   IF (FilenameFilter!=NULL) THEN filename=FilenameFilter(filename);
   *   filename = SaveFilePath + filename
   * END
   */

  if (destname)
    strcpy (uugen_fnbuffer, destname);
  else {
    sprintf (uugen_fnbuffer, "%.1024s%.3071s",
	     (uusavepath)?uusavepath:"",
	     UUFNameFilter ((thefile->filename)?
			    thefile->filename:"unknown.xxx"));
  }

  /*
   * if we don't want to overwrite existing files, check if it's there
   */

  if (!uu_overwrite) {
    if (stat (uugen_fnbuffer, &finfo) == 0) {
      UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
		 uustring (S_TARGET_EXISTS), uugen_fnbuffer);
      fclose (source);
      return UURET_EXISTS;
    }
  }

  if (fstat (fileno(source), &finfo) == -1) {
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_NOT_STAT_FILE),
	       thefile->binfile, strerror (uu_errno = errno));
    fclose (source);
    return UURET_IOERR;
  }

  /* try rename() shortcut first */
  if (!rename (thefile->binfile, uugen_fnbuffer))
    {
      mode_t mask = 0000; /* there is a slight window here anyway */
#if HAVE_UMASK
      mask = umask (0022); umask (mask);
#endif
      fclose (source);
#if HAVE_CHMOD
      chmod (uugen_fnbuffer, thefile->mode & ~mask);
#endif
      goto skip_copy;
    }

  progress.action   = 0;
  _FP_strncpy (progress.curfile,
	       (strlen(uugen_fnbuffer)>255)?
	       (uugen_fnbuffer+strlen(uugen_fnbuffer)-255):uugen_fnbuffer,
	       256);
  progress.partno   = 0;
  progress.numparts = 1;
  progress.fsize    = (long) ((finfo.st_size)?finfo.st_size:-1);
  progress.foffset  = 0;
  progress.percent  = 0;
  progress.action   = UUACT_COPYING;

  if ((fildes = open (uugen_fnbuffer,
                      O_WRONLY | O_CREAT | O_BINARY | O_TRUNC,
                      (uu_ignmode)?0666:thefile->mode)) == -1) {
    progress.action = 0;
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_NOT_OPEN_TARGET),
	       uugen_fnbuffer, strerror (uu_errno = errno));
    fclose (source);
    return UURET_IOERR;
  }

  if ((target = fdopen (fildes, "wb")) == NULL) {
    progress.action = 0;
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_IO_ERR_TARGET),
	       uugen_fnbuffer, strerror (uu_errno = errno));
    fclose (source);
    close  (fildes);
    return UURET_IOERR;
  }

  while (!feof (source)) {

    if (UUBUSYPOLL(ftell(source),progress.fsize)) {
      UUMessage (uulib_id, __LINE__, UUMSG_NOTE,
		 uustring (S_DECODE_CANCEL));
      fclose (source);
      fclose (target);
      unlink (uugen_fnbuffer);
      return UURET_CANCEL;
    }

    bytes = fread (uugen_inbuffer, 1, 1024, source);

    if (ferror (source) || (bytes == 0 && !feof (source))) {
      progress.action = 0;
      UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
		 uustring (S_READ_ERROR),
		 thefile->binfile, strerror (uu_errno = errno));
      fclose (source);
      fclose (target);
      unlink (uugen_fnbuffer);
      return UURET_IOERR;
    }
    if (fwrite (uugen_inbuffer, 1, bytes, target) != bytes) {
      progress.action = 0;
      UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
		 uustring (S_WR_ERR_TARGET),
		 uugen_fnbuffer, strerror (uu_errno = errno));
      fclose (source);
      fclose (target);
      unlink (uugen_fnbuffer);
      return UURET_IOERR;
    }
  }

  fclose (source);
  if (fclose (target)) {
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_WR_ERR_TARGET),
	       uugen_fnbuffer, strerror (uu_errno = errno));
    unlink (uugen_fnbuffer);
    return UURET_IOERR;
  }

  /*
   * after a successful decoding run, we delete the temporary file
   */

  if (unlink (thefile->binfile)) {
    UUMessage (uulib_id, __LINE__, UUMSG_WARNING,
	       uustring (S_TMP_NOT_REMOVED),
	       thefile->binfile,
	       strerror (uu_errno = errno));
  }

skip_copy:
  _FP_free (thefile->binfile);
  thefile->binfile = NULL;
  thefile->state  &= ~UUFILE_TMPFILE;
  thefile->state  |=  UUFILE_DECODED;
  progress.action  = 0;

  return UURET_OK;
}

/*
 * Calls a function repeatedly with all the info we have for a file
 * If the function returns non-zero, we break and don't send any more
 */

int UUEXPORT
UUInfoFile (uulist *thefile, void *opaque,
	    int (*func) (void *, char *))
{
  int errflag=0, res, bhflag=0, dd;
  long maxpos;
  FILE *inpfile;

  /*
   * We might need to ask our callback function to download the file
   */

  if (uu_FileCallback) {
    if ((res = (*uu_FileCallback) (uu_FileCBArg, 
				   thefile->thisfile->data->sfname,
				   uugen_fnbuffer,
				   1)) != UURET_OK)
      return res;
    if ((inpfile = fopen (uugen_fnbuffer, "rb")) == NULL) {
      (*uu_FileCallback) (uu_FileCBArg, thefile->thisfile->data->sfname,
			  uugen_fnbuffer, 0);
      UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_OPEN_FILE), uugen_fnbuffer,
		 strerror (uu_errno = errno));
      return UURET_IOERR;
    }
  }
  else {
    if ((inpfile = fopen (thefile->thisfile->data->sfname, "rb")) == NULL) {
      UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
		 uustring (S_NOT_OPEN_FILE), 
		 thefile->thisfile->data->sfname,
		 strerror (uu_errno=errno));
      return UURET_IOERR;
    }
    _FP_strncpy (uugen_fnbuffer, thefile->thisfile->data->sfname, 1024);
  }

  /*
   * seek to beginning of info
   */

  fseek (inpfile, thefile->thisfile->data->startpos, SEEK_SET);
  maxpos = thefile->thisfile->data->startpos + thefile->thisfile->data->length;

  while (!feof (inpfile) && 
	 (uu_fast_scanning || ftell(inpfile) < maxpos)) {
    if (_FP_fgets (uugen_inbuffer, 511, inpfile) == NULL)
      break;
    uugen_inbuffer[511] = '\0';

    if (ferror (inpfile))
      break;

    dd = UUValidData (uugen_inbuffer, 0, &bhflag);

    if (thefile->uudet == B64ENCODED && dd == B64ENCODED)
      break;
    else if (thefile->uudet == BH_ENCODED && bhflag)
      break;
    else if ((thefile->uudet == UU_ENCODED || thefile->uudet == XX_ENCODED) &&
	     strncmp (uugen_inbuffer, "begin ", 6) == 0)
      break;
    else if (thefile->uudet == YENC_ENCODED &&
	     strncmp (uugen_inbuffer, "=ybegin ", 8) == 0)
      break;

    if ((*func) (opaque, uugen_inbuffer))
      break;
  }

  if (ferror (inpfile)) {
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_READ_ERROR),
	       uugen_fnbuffer, strerror (uu_errno = errno));
    errflag = 1;
  }

  fclose (inpfile);

  if (uu_FileCallback)
    (*uu_FileCallback) (uu_FileCBArg, 
			thefile->thisfile->data->sfname,
			uugen_fnbuffer, 0);

  if (errflag)
    return UURET_IOERR;

  return UURET_OK;
}
	    
int UUEXPORT
UURenameFile (uulist *thefile, char *newname)
{
  char *oldname;

  if (thefile == NULL)
    return UURET_ILLVAL;

  oldname = thefile->filename;

  if ((thefile->filename = _FP_strdup (newname)) == NULL) {
    UUMessage (uulib_id, __LINE__, UUMSG_ERROR,
	       uustring (S_NOT_RENAME),
	       oldname, newname);
    thefile->filename = oldname;
    return UURET_NOMEM;
  }
  _FP_free (oldname);
  return UURET_OK;
}

int UUEXPORT
UURemoveTemp (uulist *thefile)
{
  if (thefile == NULL)
    return UURET_ILLVAL;

  if (thefile->binfile) {
    if (unlink (thefile->binfile)) {
      UUMessage (uulib_id, __LINE__, UUMSG_WARNING,
		 uustring (S_TMP_NOT_REMOVED),
		 thefile->binfile,
		 strerror (uu_errno = errno));
    }
    _FP_free (thefile->binfile);
    thefile->binfile = NULL;
    thefile->state  &= ~UUFILE_TMPFILE;
  }
  return UURET_OK;
}

int UUEXPORT
UUCleanUp (void)
{
  itbd *iter=ftodel, *ptr;
  uulist *liter;
  uufile *fiter;
  allomap *aiter;

  /*
   * delete temporary input files (such as the copy of stdin)
   */

  while (iter) {
    if (unlink (iter->fname)) {
      UUMessage (uulib_id, __LINE__, UUMSG_WARNING,
		 uustring (S_TMP_NOT_REMOVED),
		 iter->fname, strerror (uu_errno = errno));
    }
    _FP_free (iter->fname);
    ptr  = iter;
    iter = iter->NEXT;
    _FP_free (ptr);
  }

  ftodel = NULL;

  /*
   * Delete input files after successful decoding
   */

  if (uu_remove_input) {
    liter = UUGlobalFileList;
    while (liter) {
      if (liter->state & UUFILE_DECODED) {
	fiter = liter->thisfile;
	while (fiter) {
	  if (fiter->data && fiter->data->sfname) {
	    /*
	     * Error code ignored. We might want to delete a file multiple
	     * times
	     */
	    unlink (fiter->data->sfname);
	  }
	  fiter = fiter->NEXT;
	}
      }
      liter = liter->NEXT;
    }
  }

  UUkilllist (UUGlobalFileList);
  UUGlobalFileList = NULL;

  _FP_free (uusavepath);
  _FP_free (uuencodeext);
  _FP_free (sstate.source);

  uusavepath  = NULL;
  uuencodeext = NULL;

  UUkillheaders (&localenv);
  UUkillheaders (&sstate.envelope);
  memset (&localenv, 0, sizeof (headers));
  memset (&sstate,   0, sizeof (scanstate));

  while (mssdepth) {
    mssdepth--;
    UUkillheaders (&(multistack[mssdepth].envelope));
    _FP_free (multistack[mssdepth].source);
  }

  /*
   * clean up the malloc'ed stuff
   */

  for (aiter=toallocate; aiter->ptr; aiter++) {
    _FP_free (*(aiter->ptr));
    *(aiter->ptr) = NULL;
  }

  return UURET_OK;
}

