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

#ifndef __UUDEVIEW_H__
#define __UUDEVIEW_H__

/*
 * This include file features all the definitions that should
 * be externally visible. This isn't much.
 *
 * $Id: uudeview.h,v 1.1 2004/04/19 17:50:27 dasenbro Exp $
 */

#ifndef _ANSI_ARGS_
#ifdef PROTOTYPES
#define _ANSI_ARGS_(c)	c
#else
#define _ANSI_ARGS_(c)	()
#endif
#endif

/*
 * Message Types
 */

#define UUMSG_MESSAGE	(0)	/* just a message, nothing important */
#define UUMSG_NOTE	(1)	/* something that should be noticed */
#define UUMSG_WARNING	(2)	/* important msg, processing continues */
#define UUMSG_ERROR	(3)	/* processing has been terminated */
#define UUMSG_FATAL	(4)	/* decoder cannot process further requests */
#define UUMSG_PANIC	(5)	/* recovery impossible, app must terminate */

/*
 * Return Values
 */

#define UURET_OK	(0)	/* everything went fine */
#define UURET_IOERR	(1)	/* I/O Error - examine errno */
#define UURET_NOMEM	(2)	/* not enough memory */
#define UURET_ILLVAL	(3)	/* illegal value for operation */
#define UURET_NODATA	(4)	/* decoder didn't find any data */
#define UURET_NOEND	(5)	/* encoded data wasn't ended properly */
#define UURET_UNSUP	(6)	/* unsupported function (encoding) */
#define UURET_EXISTS	(7)	/* file exists (decoding) */
#define UURET_CONT	(8)	/* continue -- special from ScanPart */
#define UURET_CANCEL	(9)	/* operation canceled */

/*
 * File states, may be OR'ed
 */

#define UUFILE_READ	(0)	/* Read in, but not further processed */
#define UUFILE_MISPART	(1)	/* Missing Part(s) detected */
#define UUFILE_NOBEGIN	(2)	/* No 'begin' found */
#define UUFILE_NOEND	(4)	/* No 'end' found */
#define UUFILE_NODATA	(8)	/* File does not contain valid uudata */
#define UUFILE_OK	(16)	/* All Parts found, ready to decode */
#define UUFILE_ERROR	(32)	/* Error while decoding */
#define UUFILE_DECODED	(64)	/* Successfully decoded */
#define UUFILE_TMPFILE	(128)	/* Temporary decoded file exists */

/*
 * Encoding Types
 */

#define UU_ENCODED	(1)	/* UUencoded data   */
#define B64ENCODED	(2)	/* Mime-Base64 data */
#define XX_ENCODED	(3)	/* XXencoded data   */
#define BH_ENCODED	(4)	/* Binhex encoded   */
#define PT_ENCODED	(5)	/* Plain-Text encoded (MIME) */
#define QP_ENCODED	(6)	/* Quoted-Printable (MIME)   */
#define YENC_ENCODED	(7)	/* yEnc encoded */

/*
 * Option indices for GetOption / SetOption
 */

#define UUOPT_VERSION	(0)	/* version number MAJOR.MINORplPATCH (ro) */
#define UUOPT_FAST	(1)	/* assumes only one part per file */
#define UUOPT_DUMBNESS	(2)	/* switch off the program's intelligence */
#define UUOPT_BRACKPOL	(3)	/* give numbers in [] higher precendence */
#define UUOPT_VERBOSE	(4)	/* generate informative messages */
#define UUOPT_DESPERATE	(5)	/* try to decode incomplete files */
#define UUOPT_IGNREPLY	(6)	/* ignore RE:plies (off by default) */
#define UUOPT_OVERWRITE	(7)	/* whether it's OK to overwrite ex. files */
#define UUOPT_SAVEPATH	(8)	/* prefix to save-files on disk */
#define UUOPT_IGNMODE	(9)	/* ignore the original file mode */
#define UUOPT_DEBUG	(10)	/* print messages with FILE/LINE info */
#define UUOPT_ERRNO	(14)	/* get last error code for UURET_IOERR (ro) */
#define UUOPT_PROGRESS	(15)	/* retrieve progress information */
#define UUOPT_USETEXT	(16)	/* handle text messages */
#define UUOPT_PREAMB	(17)	/* handle Mime preambles/epilogues */
#define UUOPT_TINYB64	(18)	/* detect short B64 outside of Mime */
#define UUOPT_ENCEXT	(19)	/* extension for single-part encoded files */
#define UUOPT_REMOVE    (20)    /* remove input files after decoding */
#define UUOPT_MOREMIME  (21)    /* strict MIME adherence */
#define UUOPT_DOTDOT	(22)	/* .. unescaping has not yet been done on input files */

/*
 * Code for the "action" in the progress structure
 */

#define UUACT_IDLE	(0)	/* we don't do anything */
#define UUACT_SCANNING	(1)	/* scanning an input file */
#define UUACT_DECODING	(2)	/* decoding into a temp file */
#define UUACT_COPYING	(3)	/* copying temp to target */
#define UUACT_ENCODING	(4)	/* encoding a file */

/*
 * forward definition
 */

struct _uufile;

/*
 * Structure for holding the list of files that have been found
 * uufile items are inserted into this list with UUInsertPartToList
 * After inserting a bunch of files, UUCheckGlobalList must be called
 * to update the states.
 */

typedef struct _uulist {
  short    state;		/* Status as described by the macros above */
  short    mode;		/* file mode as found on begin line        */

  int      begin;		/* part number where begin was detected    */
  int      end;			/* part number where end was detected      */

  short    uudet;		/* Encoding type (see macros above)        */
  int      flags;		/* flags, especially for single-part files */

  long     size;		/* approximate size of resulting file      */
  char    *filename;		/* malloc'ed file name                     */
  char    *subfname;		/* malloc'ed ID from subject line          */
  char    *mimeid;		/* malloc'ed MIME-ID, if available         */
  char    *mimetype;		/* malloc'ed Content-Type, if available    */

  char    *binfile;		/* name of temp file, if already decoded   */

  struct _uufile *thisfile;	/* linked list of this file's parts        */

  int     *haveparts;		/* the parts we have (max. 256 are listed) */
  int     *misparts;		/* list of missing parts (max. 256)        */

  struct _uulist *NEXT;		/* next item of the list                   */
  struct _uulist *PREV;		/* previous item of the list               */
} uulist;

/*
 * The "progress" structure which is passed to the Busy Callback
 */

typedef struct {
  int  action;			/* see UUACT_* definitions above           */
  char curfile[256];		/* the file we are working on, incl. path  */
  int  partno;			/* part we're currently decoding           */
  int  numparts;		/* total number of parts of this file      */
  long fsize;			/* size of the current file                */
  int  percent;			/* % of _current part_                     */
  long foffset;			/* file offset -- internal use only        */
  long totsize;			/* file total size -- internal use only    */
} uuprogress;
  

/*
 * Externally visible Functions
 */

#ifndef UUEXPORT
#define UUEXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

int	UUEXPORT UUInitialize		_ANSI_ARGS_((void));
int	UUEXPORT UUGetOption		_ANSI_ARGS_((int, int *, char *, int));
int	UUEXPORT UUSetOption		_ANSI_ARGS_((int, int, char *));
char *	UUEXPORT UUstrerror		_ANSI_ARGS_((int));
int	UUEXPORT UUSetMsgCallback	_ANSI_ARGS_((void *,
						     void (*) (void *, 
							       char *,
							       int)));
int	UUEXPORT UUSetBusyCallback	_ANSI_ARGS_((void *,
						     int (*) (void *,
							      uuprogress *),
						     long));
int	UUEXPORT UUSetFileCallback	_ANSI_ARGS_((void *,
						     int (*) (void *, char *,
							      char *, int)));
int	UUEXPORT UUSetFNameFilter	_ANSI_ARGS_((void *,
						     char * (*) (void *,
								 char *)));
int	UUEXPORT UUSetFileNameCallback	_ANSI_ARGS_((void *,
						     char * (*) (void *,
								 char *, char *)));
char *	UUEXPORT UUFNameFilter		_ANSI_ARGS_((char *));
int	UUEXPORT UULoadFile		_ANSI_ARGS_((char *, char *, int, int *));
int	UUEXPORT UULoadFileWithPartNo	_ANSI_ARGS_((char *, char *, int, int, int *));
uulist *UUEXPORT UUGetFileListItem	_ANSI_ARGS_((int));
int	UUEXPORT UURenameFile		_ANSI_ARGS_((uulist *, char *));
int	UUEXPORT UUDecodeToTemp		_ANSI_ARGS_((uulist *));
int	UUEXPORT UURemoveTemp		_ANSI_ARGS_((uulist *));
int	UUEXPORT UUDecodeFile		_ANSI_ARGS_((uulist *, char *));
int	UUEXPORT UUInfoFile		_ANSI_ARGS_((uulist *, void *,
						     int (*) (void *, 
							      char *)));
int	UUEXPORT UUSmerge		_ANSI_ARGS_((int));
int	UUEXPORT UUCleanUp		_ANSI_ARGS_((void));

int	UUEXPORT UUQuickDecode		_ANSI_ARGS_((FILE *, FILE *,
						     char *, long));

int	UUEXPORT UUEncodeMulti		_ANSI_ARGS_((FILE *, FILE *,
						     char *, int,
						     char *, char *, int));
int	UUEXPORT UUEncodePartial	_ANSI_ARGS_((FILE *, FILE *,
						     char *, int,
						     char *, char *,
						     int, int, long,
						     unsigned long*));
int	UUEXPORT UUEncodeToStream	_ANSI_ARGS_((FILE *, FILE *,
						     char *, int,
						     char *, int));
int	UUEXPORT UUEncodeToFile		_ANSI_ARGS_((FILE *, char *, int,
						     char *, char *, long));
int	UUEXPORT UUE_PrepSingle		_ANSI_ARGS_((FILE *, FILE *,
						     char *, int,
						     char *, int,
						     char *, char *,
						     char *, int));
int	UUEXPORT UUE_PrepPartial	_ANSI_ARGS_((FILE *, FILE *,
						     char *, int,
						     char *, int,
						     int, long, long, char *,
						     char *, char *, int));

int	UUEXPORT UUE_PrepSingleExt	_ANSI_ARGS_((FILE *, FILE *,
						     char *, int,
						     char *, int,
						     char *, char *,
						     char *, char *,
						     int));
int	UUEXPORT UUE_PrepPartialExt	_ANSI_ARGS_((FILE *, FILE *,
						     char *, int,
						     char *, int,
						     int, long, long, char *,
						     char *, char *, char *,
						     int));
#ifdef __cplusplus
}
#endif
#endif
