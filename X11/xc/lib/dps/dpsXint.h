/*
 * dpsXint.h	-- internal definitions to dpsXclient.c
 *
 * (c) Copyright 1989-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */

#ifndef DPSXINT_H
#define DPSXINT_H

#include <X11/X.h>
#include <DPS/dpsXclient.h>

/* The first part of this structure is generic; the last part is
   implementation-specific. */

typedef struct _t_DPSPrivContextRec {
  char *priv;
  DPSSpace space;
  DPSProgramEncoding programEncoding;
  DPSNameEncoding nameEncoding;
  DPSProcs procs;
  DPSTextProc textProc;
  DPSErrorProc errorProc;
  DPSResults resultTable;
  unsigned int resultTableLength;
  struct _t_DPSContextRec *chainParent, *chainChild;
  unsigned int contextFlags;
  DPSContextExtensionRec *extension;
  
  struct _t_DPSPrivContextRec *next;
  integer lastNameIndex, cid;
  boolean eofReceived;
  char *wh;
  char *buf, *outBuf, *objBuf;
  integer nBufChars, nOutBufChars, nObjBufChars;
  DPSNumFormat numFormat;
  boolean resyncing;	/* Error has occurred and waiting ResetContext */
  int *numstringOffsets;	/* see comment below */

/* Everthing after this is XDPS-specific */

  boolean creator;	/* Did this app. create the context? */
  int statusFromEvent;  /* Latest status reported by an event during reset. */
  XDPSStatusProc statusProc;
  boolean zombie;    /* To avoid DPSAwaitReturnValues */
  XDPSReadyProc readyProc;
} DPSPrivContextRec, *DPSPrivContext;

/* The numstringOffsets field lists offsets of encoded number strings in
   the current buffer.  If non NULL, the first entry contains the size of
   the allocated offset buffer and the second entry contains the next
   available offset entry for use. */

/* The first part of this structure is generic; the last part is
   implementation-specific. */

typedef struct _t_DPSPrivSpaceRec {
  DPSSpaceProcs procs;
  
  struct _t_DPSPrivSpaceRec *next;
  long int lastNameIndex, sid;
  char *wh; /* KLUDGE to support DPSSendDestroySpace */
  DPSPrivContext firstContext;

/* Everthing after this is XDPS-specific */

  boolean  creator;	/* Did this app. create the space? */
} DPSPrivSpaceRec, *DPSPrivSpace;

#include "dpsint.h"

#endif /* DPSXINT_H */
