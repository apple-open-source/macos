/*
 * DPSCAPClient.h -- DPSCAP client definitions
 *		 
 * (c) Copyright 1990-1994 Adobe Systems Incorporated.
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

#ifndef DPSCAPCLIENT_H
#define DPSCAPCLIENT_H 1

#include "DPSCAP.h"
#include "DPSCAPproto.h"
#include <X11/Xlib.h>
#ifndef Xmalloc
#endif
#include <DPS/XDPSlib.h>

/* === DEFINES === */

#ifndef _NFILE
#define _NFILE 128
#endif

#define DPSCAPFAILED		-1
#define DPSCAPSUCCESS		0

#define DPSGCBITS \
(GCPlaneMask|GCSubwindowMode|GCClipXOrigin|GCClipYOrigin|GCClipMask)

/* === TYPEDEFS === */

typedef enum {
  dpscap_nopad,
  dpscap_pad,
  dpscap_insert,
  dpscap_append
} DPSCAPIOFlags;

typedef struct _t_DPSCAPData {
  struct _t_DPSCAPData *next;
  Display *dpy;
  /* shadow connection to agent */
  Display *agent;		/* Dummy display structure is agent */
  char *otherConnID;		/* VMS AST? */
  XExtCodes *codes;
  XExtData *extData;		/* Back pointer for clearing private */
  Atom typePSOutput;
  Atom typePSOutputWithLen;
  Atom typePSStatus;
  Atom typeNoop;
  Atom typeSync;
  Atom typeXError;
  Atom typePSReady;             /* L2-DPS/PROTO 9 addition */
  Atom typeResume;
  unsigned long saveseq;
  int dpscapVersion;
  Window agentWindow;
} DPSCAPDataRec, *DPSCAPData;

typedef struct {
  struct _t_DPSCAPData *head;	/* list of active agent connections */
  char *defaultAgentName;	/* settable agent name */
  char *map[_NFILE];		/* map DPY_NUMBER to agent name */
} DPSCAPGlobalsRec, *DPSCAPGlobals;

/* === GLOBALS === */

extern DPSCAPGlobals gCSDPS;

/* === PUBLIC PROCS === */

extern int CSDPSInit(Display * /* dpy */, int * /* numberType */, char ** /* floatingName */);

extern XExtData **CSDPSHeadOfDpyExt(Display * /* dpy */);

/* === SUPPORT PROCS === */

extern void DPSCAPChangeGC(Display * /* agent */, GC /* gc */, unsigned long /* valuemask */, XGCValues * /* values */);

extern DPSCAPData DPSCAPCreate(Display * /* dpy */, Display * /* agent */);

extern int DPSCAPDestroy(XExtData * /* extData */);

extern XExtData * DPSCAPOpenAgent(Display * /* dpy */, char * /* trueDisplayName */);

extern void DPSCAPRead(DPSCAPData /* my */, char * /* buf */, unsigned /* len */, DPSCAPIOFlags /* includePad */);

extern void DPSCAPStartUp(void);

extern void DPSCAPWrite(Display * /* my */, char * /* buf */, unsigned /* len */, DPSCAPIOFlags /* writePad */, DPSCAPIOFlags /* bufMode */);

/* ext callback hooks */

extern int DPSCAPCloseDisplayProc(Display * /* dpy */, XExtCodes * /* codes */);

extern int DPSCAPGrabServerProc(Display * /* dpy */, XExtCodes * /* codes */);

extern int DPSCAPUngrabServerProc(Display * /* dpy */, XExtCodes * /* codes */);

extern void DPSCAPCloseAgent(Display * /* agent */);

extern int DPSCAPCopyGCProc(Display * /* dpy */, GC /* gc */, XExtCodes * /* codes */);

extern int DPSCAPFreeGCProc(Display * /* pdpy */, GC /* gc */, XExtCodes * /* codes */);

extern int DPSCAPFlushGCProc(Display * /* dpy */, GC /* gc */, XExtCodes * /* codes */);

#endif /* DPSCAPCLIENT_H */
