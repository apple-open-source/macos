/*
 * dpsXpriv.h -- client lib internal impl interface for the X version
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

#ifndef DPSXPRIVATE_H
#define DPSXPRIVATE_H

#ifdef VMS
#include <decw$include/X.h>
#include <decw$include/Xlib.h>
#include <decw$include/Xutil.h>
#else /* VMS */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif /* VMS */

#include <DPS/XDPSlib.h>
#include "DPS/dpsclient.h"
#include "dpsprivate.h"
#include "DPS/dpsXclient.h"
#include "publictypes.h"

/* typedefs */

typedef struct _t_XDPSPrivContextRec
  {
  Display		*dpy;
  Drawable		drawable;
  GC			gc;
  int			x;
  int			y;
  unsigned int		eventmask;
  XStandardColormap	*grayramp;
  XStandardColormap	*ccube;
  int			actual;
  DPSContext		ctxt;	/* Points back to its context */
  ContextXID		cxid;
  char			*newObjFormat;	/* This is the object format that a */
					/* new context must use for sending */
					/* BOS's to the client.  If the     */
					/* server and client have the same  */
					/* number formats then this will be */
					/* null.			    */
  int			secure;
  } XDPSPrivContextRec, *XDPSPrivContext;


extern DPSProcs XDPSconvProcs;
extern DPSProcs XDPSrawProcs;
extern int	XDPSQuitBlocking;


extern XDPSPrivContext XDPSCreatePrivContextRec (
  Display	    * /* dpy */,
  Drawable	      /* drawable */,
  GC		      /* gc */,
  int		      /* x */,
  int		      /* y */,
  unsigned int	      /* eventmask */,
  XStandardColormap * /* grayramp */,
  XStandardColormap * /* ccube */,
  int		      /* actual */,
  int		      /* secure */);
  
  /* See if dpy supports the DPS extension.  If not, return NULL.  If so,
     it sets up a private context object that is used for creating
     contexts and spaces. */

extern DPSNumFormat DPSCreatePrivContext(
  XDPSPrivContext	/* wh */,
  DPSContext		/* ctxt */,
  ContextPSID *		/* cidP */,
  SpaceXID *		/* sxidP */,
  boolean		/* newSpace */,
  DPSClientPrintProc	/* printProc */);
  /* returns -1 if server can't create the context */

extern void DPSIncludePrivContext(
  XDPSPrivContext	/* wh */,
  DPSContext		/* ctxt */,
  ContextPSID		/* cid */,
  SpaceXID		/* sxid */,
  DPSClientPrintProc	/* printProc */);

extern void DPSSendPostScript(
  XDPSPrivContext	/* wh */,
  DPSClientPrintProc	/* printProc */,
  ContextPSID		/* cid */,
  char			* /* buffer */,
  long int		/* count */,
  boolean		(* /* returnControl */)(void));

extern void DPSSendInterrupt(
  XDPSPrivContext	/* wh */,
  ContextPSID		/* cid */,
  DPSClientPrintProc	/* printProc */);

extern void DPSSendEOF(
  XDPSPrivContext	/* wh */,
  ContextPSID		/* cid */,
  DPSClientPrintProc	/* printProc */);

extern void DPSSendTerminate(
  XDPSPrivContext	/* wh */,
  ContextPSID		/* cid */,
  DPSClientPrintProc	/* printProc */);

extern void XDPSPrivZapDpy(
  Display *		/* dpy */);

extern DPSNumFormat XDPSNumFormat (Display * /* dpy */);

  /* Determine the number format for server over the "dpy" connection. */

extern void XDPSSetProcs (void);

  /* Set pointers to raw and conversion context procs. */

extern void XDPSSetContextEncoding (
  DPSContext /* ctxt */,
  DPSProgramEncoding /* progEncoding */,
  DPSNameEncoding /* nameEncoding */);
  
  /* Sets context's program and name encodings to new values. */

extern void XDPSStatusEventHandler (XDPSLStatusEvent * /* event */);

  /* Is registered with Xlib and is called when a dps status event is
     received.  It determines what context the event belongs to and,
     if that context has a status event handler, calls its handler
     passing it the status type. */

extern void XDPSReadyEventHandler (XDPSLReadyEvent * /* event */);

  /* Is registered with Xlib and is called when a dps ready event is
     received.  It determines what context the event belongs to and,
     if that context has a status event handler, calls its handler
     passing it the ready data. */

extern void XDPSForceEvents (Display * /* dpy */);

  /* Forces processing of events that are pending over the 'dpy'
     connection.  This causes DPS events to be handled by their handlers. */

extern void XDPSSendUnfreeze (Display * /* dpy */, ContextXID /* cxid */);

extern void DPSSendDestroySpace(
		XDPSPrivContext /* wh */,
		SpaceXID /* sxid */,
		DPSClientPrintProc /* printProc */);

#endif /* DPSXPRIVATE_H */
