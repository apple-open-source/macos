/*
 * dpsXpriv.c
 *
 * (c) Copyright 1988-1994 Adobe Systems Incorporated.
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
/* $XFree86: xc/lib/dps/dpsXpriv.c,v 1.8 2002/10/21 13:32:53 alanh Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef GC
#undef GC
#endif /* GC */

#ifdef VMS
/* Xlib does not like UNIX defined to any value under VMS. */
#undef UNIX
#include <decw$include/X.h>
#include <decw$include/Xproto.h>
#include <decw$include/Xlib.h>
#include <decw$include/Xutil.h>
#else /* VMS */
#include <X11/X.h>
#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif /* VMS */
#include "DPS/XDPSlib.h"
#include "DPS/XDPS.h"
#include "dpsprivate.h"
#include "DPS/dpsconfig.h"
#include "publictypes.h"
#include "dpsXpriv.h"
#include "DPS/dpsXclient.h"
#include "DPS/dpsexcept.h"
#include "dpsassert.h"

#ifdef ISC
#include <sys/bsdtypes.h>
#endif

#ifdef __QNX__
#include <sys/select.h>
#endif

#if defined(hpux) || defined(AIXV3)
#define SELECT_TYPE int *
#else
#define SELECT_TYPE fd_set *
#endif

typedef struct _RegstDPY
  {
  Display		*dpy;
  int			firstEvent;
  struct _RegstDPY	*next;
  unsigned char	ctxtTokenType;  /* establish for each context. */
  unsigned char	prefTokenType;  /* ...of the server. */
  } RegstDPYRec, *PRegstDPY;


/* XDPSContextTimedOut is set to true by the BlockForEvent routine
   when expected return values are not received within a certain
   amount of time.  In this case, BlockForEvent will return and the
   higher callers may take appropriate action to avoid deadlocks. */
int	XDPSContextTimedOut = false;  /* Unimplemented */
DPSProcs XDPSconvProcs = NIL;
DPSProcs XDPSrawProcs = NIL;

/* XDPSQuitBlocking is set to false by the BlockForEvent routine and
   becomes true if either a zombie status event or an output event
   is received for the context waiting for return values.  */
int	XDPSQuitBlocking = false;


static DPSClientPrintProc clientPrintProc = NIL;
static PRegstDPY	   firstDPY = NIL;
/* operands for "setobjectformat" if we must agree with server. */
static char		   *format_operands[] = {"1", "2", "3", "4"};



static PRegstDPY IsRegistered (
  register Display	*dpy)
{
  register PRegstDPY	rdpy;
  
  for (rdpy = firstDPY;  rdpy != NIL;  rdpy = rdpy->next)
    if (rdpy->dpy == dpy)
      return (rdpy);
  return (NIL);
}


void XDPSPrivZapDpy(
  register Display *dpy)
{
  register PRegstDPY rdpy, prev = NIL;
  
  for (rdpy = firstDPY; rdpy != NIL; prev = rdpy,rdpy = rdpy->next)
    if (rdpy->dpy == dpy)
      {
      if (prev == NIL)
          firstDPY = rdpy->next;
      else
          prev->next = rdpy->next;
      break;
      }
  free(rdpy);
}


/* ARGSUSED */
static int UsuallyFalse (
  Display *dpy,
  XEvent  *event,
  char    *arg)
{
  return((event->type & 0x7F) == X_Error);
}

void XDPSForceEvents (
  Display *dpy)
{
  XEvent event;
  
  while (XCheckIfEvent (dpy, &event, UsuallyFalse, (char *) NULL)) {
      int (*proc)(Display *, XErrorEvent *) = XSetErrorHandler(NULL);
      (void) XSetErrorHandler(proc);
      if (proc != 0 && event.type < 256)
	      (void)(*proc)(dpy, &event.xerror);
  }
}


static void OutputEventHandler (
  register XDPSLOutputEvent *event)
{
  PRegstDPY rdpy;
  register DPSContext ctxt;
  
  if ((rdpy = IsRegistered (event->display)) == NIL ||
      rdpy->firstEvent != event->type)
    return;

  ctxt = XDPSContextFromXID (event->display, event->cxid);
  if (ctxt != NIL)
    {
    if (ctxt->resultTable != NIL)
      XDPSQuitBlocking = true;
    (*clientPrintProc) (ctxt, event->data, event->length);
    }
}


static int BlockForEvent (
    Display *dpy)
{
    fd_set readfds;
  
    XDPSQuitBlocking = false;
    /* XDPSQuitBlocking becomes true if a zombie status event or
       any output event is received by the status event handler for
       the currently-awaiting-results context. */
    while (1) {
	FD_SET(ConnectionNumber(dpy), &readfds);
	if (select (ConnectionNumber(dpy)+1, (SELECT_TYPE) &readfds,
		    (SELECT_TYPE) NULL, (SELECT_TYPE) NULL,
		    (struct timeval *) NULL) < 0) {
	    if (errno == EINTR) {
		/* Ignore interrupt signals */
		errno = 0;
		continue;
	    }
	    return (-1);  /* Broken connection (errno == EBADF) */
	} else {
	    XDPSForceEvents (dpy);
	    if (XDPSQuitBlocking) break;
	    XNoOp(dpy);
	    /* The noop is necessary to force proper behavior when the
	       connection goes away - listen carefully! When the dpy
	       connection is closed, the above select returns indicating
	       activity on the connection. We call XDPSForceEvents, which
	       calls XCheckIfEvent, which ultimately may call XFlush
	       (if there are no events queued). The XNoOp call puts
	       a message in the outgoing queue, so that XFlush is forced
	       to write on the connection. When it tries to write, the
	       error condition will be noted and XIOError will be called,
	       usually causing the application to terminate. Note that
	       the error won't happen until the second time thru this
	       loop, but that's ok. */
	}
    }
    return (0);
}


void XDPSSetContextEncoding (
  DPSContext	     ctxt,
  DPSProgramEncoding progEncoding,
  DPSNameEncoding    nameEncoding) 
{
  /* This routine should not be called if ctxt is a text context */
  
  if ((nameEncoding != dps_indexed && nameEncoding != dps_strings) ||
      (progEncoding != dps_ascii && progEncoding != dps_encodedTokens &&
       progEncoding != dps_binObjSeq))
    {
    if (ctxt->errorProc != NIL) 
      (*ctxt->errorProc) (ctxt, dps_err_encodingCheck,
			  nameEncoding, progEncoding);
    return;
    }
  else if (progEncoding == dps_ascii || progEncoding == dps_encodedTokens ||
	   nameEncoding == dps_strings)
    ctxt->procs = XDPSconvProcs;
  else
    ctxt->procs = XDPSrawProcs;

  ctxt->nameEncoding = nameEncoding;
  ctxt->programEncoding = progEncoding;
}


/* ARGSUSED */
void DPSDefaultTextBackstop (ctxt, buf, count)
  DPSContext	    ctxt;
  char		    *buf;
  long unsigned int count;
{
  if (buf == NULL || count == 0)
      {
      (void) fflush(stdout);
      return;
      }
  (void) fwrite (buf, sizeof (char), count, stdout);
  (void) fflush (stdout);
}

/* ARGSUSED */
void DPSInitClient(
  DPSTextProc	textProc,
  void		(*releaseProc) (char *, char *))
{
  DPSAssert (releaseProc != NIL);
  XDPSSetProcs ();
  DPSSetTextBackstop (DPSDefaultTextBackstop);
  DPSSetErrorBackstop (DPSDefaultErrorProc);
}


DPSNumFormat XDPSNumFormat (
  Display *dpy)
{
  PRegstDPY	rdpy;
  
  if ((rdpy = IsRegistered (dpy)) == NIL)
    return ((DPSNumFormat) -1);
  else
    return ((rdpy->ctxtTokenType < DPS_HI_NATIVE) ? dps_ieee : dps_native);
}

XDPSPrivContext XDPSCreatePrivContextRec (
  Display		*dpy,
  Drawable		drawable,
  GC			gc,
  int			x,
  int			y,
  unsigned int		eventmask,
  XStandardColormap	*grayramp,
  XStandardColormap	*ccube,
  int			actual,
  int			secure)
{
  int			event_base;
  int			token_type;		/* From server. */
  char			*num_format_name;	/* From server. */
  PRegstDPY		rdpy;
  XDPSPrivContext	wh;

  if (DPSInitialize() != 0) return(NULL);
  if ((rdpy = IsRegistered (dpy)) == NIL)
    {
    /* DPS extension on this dpy? */
    event_base = XDPSLInit (dpy, &token_type, &num_format_name);
    if (event_base >= 0 &&
	(rdpy = (PRegstDPY) calloc (sizeof (RegstDPYRec), 1)))
      {
      XDPSLSetTextEventHandler (dpy, (XDPSLEventHandler) OutputEventHandler);
      XDPSLSetStatusEventHandler (dpy, (XDPSLEventHandler) XDPSStatusEventHandler);
      XDPSLSetReadyEventHandler (dpy, (XDPSLEventHandler) XDPSReadyEventHandler);
      XDPSLInitDisplayFlags(dpy);
      rdpy->dpy = dpy;
      rdpy->firstEvent = event_base;
      rdpy->next = firstDPY;

      rdpy->prefTokenType = (unsigned char) token_type;
      
      if (strcmp (num_format_name, DPS_FORMATNAME) == 0)
	rdpy->ctxtTokenType = DPS_DEF_TOKENTYPE;
      else
	/* Everybody must talk ieee! */
#if SWAPBITS
	rdpy->ctxtTokenType = DPS_LO_IEEE;
#else /* SWAPBITS */
	rdpy->ctxtTokenType = DPS_HI_IEEE;
#endif /* SWAPBITS */
      
      firstDPY = rdpy;
      }
    else
      return (NULL);
    }
  
  if ((wh = (XDPSPrivContext) calloc (sizeof (XDPSPrivContextRec), 1)) != 0)
    {
    wh->dpy = dpy;
    wh->drawable = drawable;
    wh->gc = gc;
    wh->x = x;
    wh->y = y;
    wh->eventmask = eventmask;
    wh->grayramp = grayramp;
    wh->ccube = ccube;
    wh->actual = actual;
    wh->newObjFormat = format_operands[rdpy->ctxtTokenType - DPS_HI_IEEE];
    wh->secure = secure;
    return (wh);
    }
  else
    return (NULL);
}


DPSNumFormat DPSCreatePrivContext (
  XDPSPrivContext	wh,
  DPSContext		ctxt,
  ContextPSID		*cidP,
  SpaceXID		*sxidP,
  boolean		newSpace,
  DPSClientPrintProc	printProc)
{
  PRegstDPY		rdpy;
  
  if (clientPrintProc == NIL)
    clientPrintProc = printProc;
  
  if ((rdpy = IsRegistered (wh->dpy)) == NIL)
    return ((DPSNumFormat) -1);
    
  if (newSpace || sxidP == NIL)
    wh->cxid = XDPSLCreateContextAndSpace (wh->dpy, wh->drawable, wh->gc,
					   wh->x, wh->y, wh->eventmask,
					   wh->grayramp, wh->ccube,
					   wh->actual, cidP, sxidP,
                                           wh->secure); /* L2-DPS/PROTO 9 */
  else
    wh->cxid = XDPSLCreateContext (wh->dpy, *sxidP, wh->drawable, wh->gc,
				   wh->x, wh->y, wh->eventmask,
				   wh->grayramp, wh->ccube, wh->actual, cidP,
				   wh->secure); /* L2-DPS/PROTO 9 */
  if (wh->cxid == None) return((DPSNumFormat) -1);
  wh->ctxt = ctxt;
  if (wh->newObjFormat != NIL)
    {
    XDPSLGiveInput (wh->dpy, wh->cxid, wh->newObjFormat, 1);
    XDPSLGiveInput (wh->dpy, wh->cxid, " setobjectformat\n", 17);
    }

  if (rdpy->ctxtTokenType != DPS_DEF_TOKENTYPE)
    ctxt->procs = XDPSconvProcs;
  return ((rdpy->ctxtTokenType < DPS_HI_NATIVE) ? dps_ieee : dps_native);
}


void DPSIncludePrivContext (
  XDPSPrivContext	wh,
  DPSContext		ctxt,
  ContextPSID		cid,
  SpaceXID		sxid,
  DPSClientPrintProc	printProc)
{
  XDPSPrivContext	newWh;
  SpaceXID		space;
  
  if (clientPrintProc == NIL)
    clientPrintProc = printProc;
  
  newWh = (XDPSPrivContext) calloc (sizeof (XDPSPrivContextRec), 1);
  if (!newWh) DPSOutOfMemory();
  *newWh = *wh;
  if (IsRegistered (wh->dpy) != NIL)
    {
    newWh->cxid = XDPSLCreateContextFromID (wh->dpy, cid, &space);
    DPSAssertWarn (space == sxid, ctxt, "attempting context from context ID from different space");
    newWh->ctxt = ctxt;
    /* Did we have to change object format for parent context? */
    /* Note: the child context must inherit the object format of
	     its parent.  When this happens in the server there
	     will be no need for the following code segment. */
    if (wh->newObjFormat != NIL)   /* Yes, do it for the child too. */
      {
      XDPSLGiveInput (wh->dpy, newWh->cxid, wh->newObjFormat, 1);
      XDPSLGiveInput (wh->dpy, newWh->cxid, " setobjectformat\n", 17);
      }
    }
  else
    {
    newWh->cxid = 0;  /* Must not refer to a good context. */
    newWh->ctxt = NIL;
    }

  (void) DPSSetWh (ctxt, (char *) newWh);
}

/* ARGSUSED */
void DPSSendPostScript (
  register XDPSPrivContext	wh,
  DPSClientPrintProc		printProc,
  ContextPSID			cid,
  char				*buffer,
  long int			count,
  boolean			(*returnControl)(void))
{
    boolean blocking = buffer == NIL;

    if (IsRegistered (wh->dpy) == NIL)
        (*printProc) (wh->ctxt, NIL, 0);
    else {
	if (count > 0)
	    XDPSLGiveInput (wh->dpy, wh->cxid, buffer, count);
  
	if (blocking) {
	    XDPSLFlush (wh->dpy);
	    if (BlockForEvent (wh->dpy) < 0 && wh->ctxt->errorProc != NIL) {
		(*(wh->ctxt->errorProc)) (wh->ctxt, dps_err_closedDisplay,
					  ConnectionNumber(wh->dpy),
					  0);
	    }
	}
	DPSCheckRaiseError(wh->ctxt);
    }
}


/* ARGSUSED */
void DPSSendInterrupt (
  XDPSPrivContext	wh,
  ContextPSID		cid,
  DPSClientPrintProc	printProc)
{
  XDPSLNotifyContext (wh->dpy, wh->cxid, PSINTERRUPT);
}


/* ARGSUSED */
void DPSSendEOF (
  XDPSPrivContext	wh,
  ContextPSID		cid,
  DPSClientPrintProc	printProc)
{
  XDPSLReset (wh->dpy, wh->cxid);
}


/* ARGSUSED */
void DPSSendTerminate (
  XDPSPrivContext	wh,
  ContextPSID		cid,
  DPSClientPrintProc	printProc)
{
  XDPSLNotifyContext (wh->dpy, wh->cxid, PSKILL);
}


void XDPSSendUnfreeze (
  Display    *dpy,
  ContextXID cxid)
{
  XDPSLNotifyContext (dpy, cxid, PSUNFREEZE);
}


/* ARGSUSED */
void DPSSendDestroySpace(
  XDPSPrivContext	wh,
  SpaceXID		sxid,
  DPSClientPrintProc	printProc)
{
  XDPSLDestroySpace (wh->dpy, sxid);
}


void DPSOutOfMemory (void)
{
  DPSFatalProc(NULL, "DPS Client Library Error:  Out of memory.\n");
  exit (1);
}
