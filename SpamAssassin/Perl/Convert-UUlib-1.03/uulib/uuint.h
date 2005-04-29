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

#ifndef __UUINT_H__
#define __UUINT_H__

/*
 * This file describes the internal structures, variables and definitions
 * of UUDeview. It should not be included from other packages. Subject to
 * change without notice. Do not depend on anything here.
 *
 * $Id: uuint.h,v 1.1 2004/05/14 15:23:05 dasenbro Exp $
 */

/*
 * Busy Polls will be made after processing ... lines
 */

#define BUSY_LINE_TICKS		50

/*
 * States of MIME scanner
 */

#define MS_HEADERS	1	/* still inside of headers      */
#define MS_BODY		2	/* body of `simple' messages    */
#define MS_PREAMBLE	3	/* preamble of Multipart/Mixed  */
#define MS_SUBPART	4	/* within one of the Multiparts */
#define MS_EPILOGUE	5	/* epilogue of Multipart/Mixed  */

/*
 * Number of subsequent encoded lines we require to believe this
 * is valid data.
 */

#define ELC_COUNT	4

/*
 * Flags a part may have. FL_PROPER means that we are sure about the file's
 * encoding, beginning and end, and don't have to use special care when de-
 * coding.
 */

#define FL_NONE		0	/* no flag, just plain normal   */
#define FL_SINGLE	1	/* standalone MSG, do not mix   */
#define FL_PARTIAL	2	/* from Message/Partial         */
#define FL_PROPER	4	/* proper MIME part             */
#define FL_TOEND	8	/* part continues to EOF        */

/*
 * Auxiliary macro: compute the percentage of a against b.
 * The obvious answer is (100*a)/b, but this overflows for large a.
 * a/(b/100) is better; we use a/((b/100)+1) so that we don't divide
 * by zero for b<100 and the result doesn't become larger than 100%
 */

#define UUPERCENT(a,b)	((int) ((unsigned long)(a) / \
				(((unsigned long)(b)/100)+1)))
     
/*
 * Make the Busy Callback easier. The macro returns true if the BusyCallback
 * wants us to terminate.
 */

extern unsigned long uuyctr;
#define UUBUSYPOLL(a,b) (((++uuyctr%BUSY_LINE_TICKS)==0) ? (progress.percent=UUPERCENT((a),(b)),UUBusyPoll()):0)

/*
 * How many lines of headers do we need to believe another mail
 * header is approaching? Use more restrictive values for MIME
 * mails, less restrictive for Freestyle
 */

typedef struct {
  int restart;		/* restarting after a MIME body (not subpart) */
  int afterdata;	/* after we had useful data in freestyle mode */
  int afternl;		/* after an empty line in freestyle mode      */
} headercount;

extern headercount hlcount;

/*
 * Information from the headers of a message. Each instance must
 * have its very own copy of the strings. If `mimevers' is NULL,
 * then this message does not comply to the MIME standard.
 */

typedef struct _headers {
  char *from;		/* From:                                          */
  char *subject;	/* Subject:                                       */
  char *rcpt;		/* To:                                            */
  char *date;		/* Date:                                          */
  char *mimevers;	/* MIME-Version:                                  */
  char *ctype;		/* Content-Type:                                  */
  char *ctenc;		/* Content-Transfer-Encoding:                     */
  char *fname;		/* Potential Filename from Content-Type Parameter */
  char *boundary;	/* MIME-Boundary from Content-Type Parameter      */
  char *mimeid;		/* MIME-Id for Message/Partial                    */
  int partno;		/* part number for Message/Partial                */
  int numparts;		/* number of parts for Message/Partial            */
} headers;

/*
 * Scanner state
 */

typedef struct _scanstate {
  int isfolder;		/* if we think this is a valid email folder       */
  int ismime;		/* if we are within a valid MIME message          */
  int mimestate;	/* state of MIME scanner                          */
  int mimeenc;		/* encoding of this MIME file                     */
  char *source;		/* source filename                                */
  headers envelope;	/* mail envelope headers                          */
} scanstate;

/*
 * Structure that holds the information for a single file / part of
 * a file. If a subject line is encountered, it is copied to subject;
 * if a begin is found, the mode and name of the file is extracted.
 * flags are set if 'begin' or 'end' is detected and 'uudet' if valid
 * uuencoded data is found. If the file contains a 'From:' line with
 * a '@' in it (indicating an origin email address), it is preserved
 * in 'origin'.
 **/

typedef struct _fileread {
  char *subject;	/* Whole subject line */
  char *filename;	/* Only filled in if begin detected */
  char *origin;		/* Whole 'From:' line */
  char *mimeid;		/* the ID for Mime-encoded files */
  char *mimetype;	/* Content-Type */
  short mode;		/* Mode of File (from 'begin') */
  int   begin;		/* begin detected */
  int   end;		/* end detected */
  int   flags;		/* associated flags */

  short uudet;		/* valid encoded data. value indicates encoding */
  short partno;		/* Mime-files have a part number within */
  short maxpno;		/* ... plus the total number of parts   */

  char *sfname;		/* Associated source file */
  long startpos;	/* ftell() position where data starts */
  long length;		/* length of data */
} fileread;

/*
 * Structure for holding one part of a file, with some more information
 * about it. The UUPreProcessPart() function takes one a fileread structure
 * and produces this uufile structure.
 * Linked List, ordered by partno.
 **/

typedef struct _uufile {
  char     *filename;
  char     *subfname;
  char     *mimeid;
  char     *mimetype;
  short     partno;
  fileread *data;
  struct _uufile *NEXT;
} uufile;

extern void *uu_MsgCBArg;
extern void *uu_BusyCBArg;
extern void *uu_FileCBArg;
extern void *uu_FFCBArg;
extern void *uu_FNCBArg;

/*
 * variables
 */

extern int uu_fast_scanning;
extern int uu_bracket_policy;
extern int uu_verbose;
extern int uu_desperate;
extern int uu_ignreply;
extern int uu_debug;
extern int uu_errno;
extern int uu_dumbness;
extern int uu_overwrite;
extern int uu_ignmode;
extern int uu_headercount;
extern int uu_usepreamble;
extern int uu_handletext;
extern int uu_tinyb64;
extern int uu_remove_input;
extern int uu_more_mime;
extern int uu_dotdot;

extern char *uusavepath;
extern char *uuencodeext;

/*
 * Encoding/Decoding tables
 */

extern unsigned char UUEncodeTable[];
extern unsigned char XXEncodeTable[];
extern unsigned char B64EncodeTable[];
extern unsigned char BHEncodeTable[];

/*
 * String tables from uustring.c
 */

extern char *msgnames[];
extern char *codenames[];
extern char *uuretcodes[];

extern uulist *UUGlobalFileList;

/*
 * State of MIME variables and current progress
 */

extern int nofnum, mssdepth;
extern int mimseqno, lastvalid;
extern int lastenc;
extern scanstate  multistack[];
extern headers    localenv;
extern scanstate  sstate;
extern uuprogress progress;

/*
 * mallocable areas
 */

extern char *uugen_fnbuffer, *uugen_inbuffer;
extern char *uucheck_lastname, *uucheck_tempname;
extern char *uuestr_itemp, *uuestr_otemp;
extern char *uulib_msgstring, *uuncdl_fulline;
extern char *uuncdp_oline, *uuscan_shlline, *uuscan_shlline2;
extern char *uuscan_pvvalue, *uuscan_phtext;
extern char *uuscan_sdline, *uuscan_sdbhds1;
extern char *uuscan_sdbhds2, *uuscan_spline;
extern char *uuutil_bhwtmp;
extern char *uunconc_UUxlat, *uunconc_UUxlen;
extern char *uunconc_B64xlat, *uunconc_XXxlat;
extern char *uunconc_BHxlat, *uunconc_save;

#ifdef __cplusplus
extern "C" {
#endif

extern void   (*uu_MsgCallback)     (void *, char *, int);
extern int    (*uu_BusyCallback)    (void *, uuprogress *);
extern int    (*uu_FileCallback)    (void *, char *, char *, int);
extern char * (*uu_FNameFilter)     (void *, char *);
extern char * (*uu_FileNameCallback)(void *, char *, char *);

/*
 * Functions from uulib.c that aren't defined in <uudeview.h>
 * Be careful about the definition with variable arguments.
 */

#if defined(STDC_HEADERS) || defined(HAVE_STDARG_H)
int		UUMessage		(char *, int, int, char *, ...);
#else
int		UUMessage		();
#endif
int		UUBusyPoll		(void);

/*
 * Functions from uucheck.c
 */

uufile *	UUPreProcessPart	(fileread *, int *);
int 		UUInsertPartToList	(uufile *);
uulist *	UUCheckGlobalList	(void);

/*
 * Functions from uuutil.c
 */

void 		UUkillfread 		(fileread *);
void	 	UUkillfile 		(uufile *);
void 		UUkilllist 		(uulist *);
void 		UUkillheaders 		(headers *);

fileread *	ScanPart 		(FILE *, char *, int *);

int		UUbhdecomp		(char *, char *,
				         char *, int *,
				         size_t, size_t, 
				         size_t *);
size_t		UUbhwrite		(char *, size_t, size_t, FILE *);

/*
 * Functions from uunconc.c
 */

int		UURepairData		(FILE *, char *, int, int *);

void 		UUInitConc		_ANSI_ARGS_((void));
int 		UUValidData		_ANSI_ARGS_((char *, int, int *));
size_t 		UUDecodeLine		_ANSI_ARGS_((char *, char *, int));
int		UUDecodeField		_ANSI_ARGS_((char *, char *, int));
int		UUDecodePart		_ANSI_ARGS_((FILE *, FILE *, int *,
						     long, int, int, char *));
int 		UUDecode 		_ANSI_ARGS_((uulist *));

/*
 * Message retrieval from uustring.c
 */

char *		uustring		(int);

/*
 * From uuscan.c
 */

int		UUScanHeader		(FILE *, headers *);

#ifdef __cplusplus
}
#endif
#endif
