/*
 * dpsXclient.c -- Implementation of the Display PostScript Client Library.
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
/* $XFree86: xc/lib/dps/dpsXclient.c,v 1.3 2000/09/26 15:56:59 tsi Exp $ */

#include <stdlib.h>
#include <unistd.h>	/* sleep() */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef VMS
/* Xlib does not like UNIX defined to any value under VMS. */
#undef UNIX
#include <decw$include/X.h>
#include <decw$include/Xlib.h>

#else /* VMS */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif /* VMS */

#include "DPS/XDPSlib.h"
#include "DPS/XDPS.h"

#include "publictypes.h"
#include "DPS/dpsclient.h"
#include "dpsprivate.h"

#include "dpsXpriv.h"
#include "DPS/dpsXclient.h"

#include "dpsdict.h"
#include "DPS/dpsexcept.h"

#include "dpsXint.h"

static DPSPrivContext FindPrivContext (
  Display  *	dpy,
  long int	cid)
{
  DPSPrivSpace ss;
  DPSPrivContext cc;

  for (ss = spaces;  ss != NIL;  ss = ss->next)
    for (cc = ss->firstContext;  cc != NIL;  cc = cc->next)
      if (cc->cid == cid && ((XDPSPrivContext) cc->wh)->dpy == dpy)
	return (cc);
  return (NIL);
}

DPSContext XDPSFindContext (
  Display  *	dpy,
  int		cid)
{
  return ((DPSContext) FindPrivContext (dpy, cid));
}

DPSContext DPSContextFromContextID(
  DPSContext ctxt,
  int contextID,
  DPSTextProc textProc,
  DPSErrorProc errorProc)
{
  DPSPrivSpace ss;
  Display *dpy = ((XDPSPrivContext) ((DPSPrivContext) ctxt)->wh)->dpy;
  DPSPrivContext c, cc = FindPrivContext (dpy, contextID);
  DPSPrivContext oldc = (DPSPrivContext)ctxt;
  
  if (cc != NIL) return (DPSContext)cc;

  c = (DPSPrivContext)DPScalloc(sizeof(DPSPrivContextRec), 1);
  if (!c) return NIL;
  *c = *oldc;
  ss = (DPSPrivSpace)c->space;
  
  if (textProc) c->textProc = textProc;
  if (errorProc) c->errorProc = errorProc;

  c->eofReceived = false;
  c->cid = contextID;
  c->buf = c->outBuf = c->objBuf = NIL;
  c->chainParent = c->chainChild = NIL;
  
  c->nBufChars = c->nOutBufChars = c->nObjBufChars = 0;

  c->next = ss->firstContext;
  DPSAssert(c->next != c);
  ss->firstContext = c;

  /* Note: there's no way to determine whether the new context id was obtained
  ** as a result of a fork operation or from another application.  so, it must
  ** be assumed that the application is the creator of the new context.
  ** Otherwise, it would have called the XDPSContextFromSharedID.
  */
  c->creator = true;
  c->zombie = false;
  c->numstringOffsets = NULL;

  DPSIncludePrivContext(
    (XDPSPrivContext) c->wh, (DPSContext)c, c->cid, ss->sid, DPSclientPrintProc);

  return (DPSContext)c;
}

boolean DPSPrivateCheckWait(
    DPSContext ctxt)
{
    DPSPrivContext cc = (DPSPrivContext) ctxt;

    if (!cc->creator || cc->zombie) {
	DPSSafeSetLastNameIndex(ctxt);
	if (cc->errorProc != NIL) {
	    (*cc->errorProc) (ctxt, cc->zombie ? dps_err_deadContext :
			                         dps_err_invalidAccess,
			      (unsigned long) ctxt, 0);
	}
	return true;
    }
    return false;
}

static void procFlushContext(
  DPSContext ctxt)
{
  DPSPrivContext c = (DPSPrivContext) ctxt;
  XDPSLFlush (((XDPSPrivContext) c->wh)->dpy);
  if (ctxt->chainChild != NIL) DPSFlushContext(ctxt->chainChild);
}

/* ARGSUSED */
static Bool FindDPSEvent(
  Display *dpy,
  XEvent *event,
  char *arg)
{
  return XDPSIsDPSEvent(event);
}

static void procAwaitReturnValues(DPSContext ctxt)
{
  DPSPrivContext c = (DPSPrivContext)ctxt;
 
  XDPSPrivContext xwh = (XDPSPrivContext) c->wh;
  XEvent ev;

  /* Output currently goes only to creator! */
  if (!c->creator)
    {
    DPSSafeSetLastNameIndex(ctxt);
    c->resultTable = NIL;
    c->resultTableLength = 0;
    if (c->errorProc != NIL)
      (*c->errorProc) (ctxt, dps_err_invalidAccess, 0, 0);
    return;
    }
  if (c->resultTable != NIL)
    {
    DPSCheckInitClientGlobals();

    if (XDPSLGetWrapWaitingFlag(xwh->dpy)) {
	DPSSafeSetLastNameIndex(ctxt);
	c->resultTable = NIL;
	c->resultTableLength = 0;
	if (c->errorProc != NIL)
		(*c->errorProc) (ctxt, dps_err_recursiveWait,
				 (unsigned long) xwh->dpy, 0);
	return;
    }
    XDPSLSetWrapWaitingFlag(xwh->dpy, True);

  DURING
    DPSFlushContext(ctxt);
    while (c->resultTable != NIL)
      {
      /* We may block indefinitely if the context is frozen or it
	 somehow needs more input. */
      if (c->zombie)
	{
	DPSSafeSetLastNameIndex(ctxt);
	c->resultTable = NIL;
	c->resultTableLength = 0;
	if (c->errorProc != NIL)
	  (*c->errorProc) (ctxt, dps_err_deadContext, (unsigned long) c, 0);
	XDPSLSetWrapWaitingFlag(xwh->dpy, False);
	E_RTRN_VOID;
	}
      
      /* Someone could conceivably change the event delivery mode in the
	 middle of this...best to check every time */

      if (XDPSLGetPassEventsFlag(xwh->dpy)) {
	  XIfEvent(xwh->dpy, &ev, FindDPSEvent, (char *) NULL);
	  if (!XDPSDispatchEvent(&ev)) DPSCantHappen();
      } else DPSSendPostScript((XDPSPrivContext) c->wh, DPSclientPrintProc,
			       c->cid, NIL, 0, NIL);
    }
  HANDLER
    XDPSLSetWrapWaitingFlag(xwh->dpy, False);
    RERAISE;
  END_HANDLER

  XDPSLSetWrapWaitingFlag(xwh->dpy, False);

  }

 /* update space's name map.
       space->lastNameIndex is the highest index known to be known to the
	 server for this space.
       c->lastNameIndex is the highest index sent so far to the context
 */

  if (((DPSPrivSpace)(c->space))->lastNameIndex < c->lastNameIndex)
    ((DPSPrivSpace)(c->space))->lastNameIndex = c->lastNameIndex;
}

void DPSinnerProcWriteData(
  DPSContext ctxt,
  char *buf,
  unsigned int count)
{
  DPSPrivContext c = (DPSPrivContext) ctxt;
  
  /* ASSERT: safe to call with chain */

  /* No local buffering */
  DPSSendPostScript ((XDPSPrivContext) c->wh, DPSclientPrintProc,
		     c->cid, buf, count, NIL);
} /* DPSinnerProcWriteData */

static void procResetContext(DPSContext ctxt)
{
  DPSPrivContext c = (DPSPrivContext) ctxt;
  int currStatus;
  register XDPSPrivContext xwh = (XDPSPrivContext) c->wh;
  int retries = 0;
  int backoff = 2;
  
  /* First make sure context isn't frozen, try to unfreeze. */

#define DPS_SLEEP_SECS 2

  while((currStatus = XDPSLGetStatus(xwh->dpy, xwh->cxid)) == PSFROZEN)
    {
    XDPSLNotifyContext(xwh->dpy, xwh->cxid, PSUNFREEZE);
    sleep(DPS_SLEEP_SECS);
      /* Okay if context is PSRUNNING, since the EOF will
         be handled at the next PSNEEDSINPUT */
    }

  /* Remove events from Xlib Qs before sending the reset request. */
  XDPSForceEvents (xwh->dpy);
  
  if (currStatus == PSSTATUSERROR)
    /* +++ report error? */;
  else  /* Didn't become zombie. */
    {
    currStatus = 0;
    XDPSLReset(xwh->dpy, xwh->cxid);
    XDPSLFlush(xwh->dpy);
    /* Be optmistic for the first try. Assume the app set up a status mask
       correctly, we should get a status event without asking the
       server for status. */
    
    XDPSForceEvents(xwh->dpy);
    currStatus = c->statusFromEvent;

    while (currStatus != PSNEEDSINPUT && currStatus != PSZOMBIE)
      {
      if (currStatus == PSFROZEN)
          XDPSLNotifyContext(xwh->dpy, xwh->cxid, PSUNFREEZE);
      if (retries > backoff)
        {
        /* Optimism failed.  App probably didn't set up a status mask.
           Ask the server for status. */
        currStatus = XDPSLGetStatus(xwh->dpy, xwh->cxid);
        retries = 0;
        backoff = (backoff > 30) ? 2 : backoff + 1;
        continue;
        }
      else
        ++retries;
      sleep(DPS_SLEEP_SECS);
      XDPSForceEvents(xwh->dpy);
      currStatus = c->statusFromEvent;
      }
    }
  
  c->eofReceived = false;
}

void DPSPrivateDestroyContext(DPSContext ctxt)
{
  DPSPrivContext c = (DPSPrivContext)ctxt;
  DPSPrivSpace s = (DPSPrivSpace) c->space;

  if (c->creator)
    DPSSendTerminate((XDPSPrivContext) c->wh, c->cid, DPSclientPrintProc);
  else 
    XDPSSetStatusMask(ctxt, 0, XDPSL_ALL_EVENTS, 0);  /* Stop status events */
  /* Don't free the space's wh out from under it */
  if (c->wh != s->wh) free(c->wh);
}

void DPSPrivateDestroySpace(DPSSpace space)
{
    DPSPrivSpace ss = (DPSPrivSpace) space;

    if (ss->creator) DPSSendDestroySpace((XDPSPrivContext) ss->wh, ss->sid,
					 DPSclientPrintProc);

    free (ss->wh);
}

boolean DPSCheckShared(DPSPrivContext ctxt)
{
  return ctxt->creator == false && ctxt->resultTable != NIL;
  /* let procAwaitReturnValues generate error */
}

/* ARGSUSED */
void DPSServicePostScript(boolean (*returnControl)(void))
{
} /* DPSServicePostScript */

void DPSHandleBogusError(DPSContext ctxt, char *prefix, char *suffix)
{
    char *buf = "bogus error output from context";
    DPSDefaultPrivateHandler(ctxt, dps_err_warning,
			     (long unsigned int)buf, 0, prefix, suffix);
}

void DPSDefaultPrivateHandler(
  DPSContext ctxt,
  DPSErrorCode errorCode,
  long unsigned int arg1,
  long unsigned int arg2,
  char *prefix,
  char *suffix)
{

  DPSTextProc textProc = DPSGetCurrentTextBackstop();

  switch (errorCode) {
    case dps_err_invalidAccess:
      if (textProc != NIL)
	{
	char m[100];
	(void) sprintf (m, "%sInvalid context access.%s", prefix, suffix);
	(*textProc) (ctxt, m, strlen (m));
	}
	break;
    case dps_err_encodingCheck:
      if (textProc != NIL)
	{
	char m[100];
	(void) sprintf (m, "%sInvalid name/program encoding: %d/%d.%s",
			prefix, (int) arg1, (int) arg2, suffix);
	(*textProc) (ctxt, m, strlen (m));
	}
	break;
    case dps_err_closedDisplay:
      if (textProc != NIL)
	{
	char m[100];
	(void) sprintf (m, "%sBroken display connection %d.%s",
			prefix, (int) arg1, suffix);
	(*textProc) (ctxt, m, strlen (m));
	}
	break;
    case dps_err_deadContext:
      if (textProc != NIL)
	{
	char m[100];
	(void) sprintf (m, "%sDead context 0x0%x.%s", prefix,
			(int) arg1, suffix);
	(*textProc) (ctxt, m, strlen (m));
	}
	break;
    case dps_err_warning:
      if (textProc != NIL)
        {
        char *warn = (char *)arg1;
        char *msg = "%% DPS Client Library Warning:\n   ";
        (*textProc)(ctxt, msg, strlen(msg));
        (*textProc)(ctxt, warn, strlen(warn));
        msg = "\n";
        (*textProc)(ctxt, msg, strlen(msg));
        /* flush convention */
        (*textProc)(ctxt, msg, 0);
        }
        break;
    case dps_err_fatal:
      if (textProc != NIL)
        {
        char *fatal = (char *)arg1;
        char *msg = "%% DPS Client Library Fatal Internal Error:\n   ";
        (*textProc)(ctxt, msg, strlen(msg));
        (*textProc)(ctxt, fatal, strlen(fatal));
        msg = ".\nAborting ...\n";
        (*textProc)(ctxt, msg, strlen(msg));
        /* flush convention */
        (*textProc)(ctxt, msg, 0);
        abort();
        }
        break;
    case dps_err_recursiveWait:
      if (textProc != NIL)
	{
	char m[100];
	(void) sprintf (m,
			"%sRecursive wait for return values, display 0x%x.%s",
			prefix, (int) arg1, suffix);
	(*textProc) (ctxt, m, strlen (m));
	}
	break;
  }
}

void DPSInitPrivateSpaceFields(DPSPrivSpace s)
{
    s->creator = true;
}

void DPSInitPrivateContextFields(DPSPrivContext c, DPSPrivSpace s)
{
    c->creator = true;
    c->zombie = false;
    if (!s->creator) {
	c->procs = XDPSconvProcs;
	c->nameEncoding = dps_strings;
    }
}

void  DPSInitPrivateTextContextFields(DPSPrivContext c, DPSPrivSpace s)
{
    c->creator = true;
    c->zombie = false;
    c->space = (DPSSpace) s;
    c->next = s->firstContext;
    s->firstContext = c;
}
  
long int DPSLastUserObjectIndex = 0;

long int DPSNewUserObjectIndex (void)
{
  return (DPSLastUserObjectIndex++);
}

void XDPSSetProcs (void)
{
  DPSCheckInitClientGlobals ();
  if (!textCtxProcs)
    {
    textCtxProcs = (DPSProcs) DPScalloc (sizeof (DPSProcsRec), 1);
    DPSInitCommonTextContextProcs(textCtxProcs);
    DPSInitSysNames();
    }
  if (!ctxProcs)
    {
    ctxProcs = (DPSProcs) DPScalloc (sizeof (DPSProcsRec), 1);
    DPSInitCommonContextProcs(ctxProcs);
    DPSInitPrivateContextProcs(ctxProcs);
    }
  if (!XDPSconvProcs)
    XDPSconvProcs = (DPSProcs) DPScalloc (sizeof (DPSProcsRec), 1);
  if (!XDPSrawProcs)
    XDPSrawProcs = ctxProcs;
  *XDPSconvProcs = *ctxProcs;
  XDPSconvProcs->BinObjSeqWrite = textCtxProcs->BinObjSeqWrite;
  XDPSconvProcs->WriteStringChars = textCtxProcs->WriteStringChars;
  XDPSconvProcs->WritePostScript = textCtxProcs->WritePostScript;
  XDPSconvProcs->WriteNumString = textCtxProcs->WriteNumString;
}

void DPSInitPrivateContextProcs(DPSProcs p)
{
    p->FlushContext = procFlushContext;
    p->ResetContext = procResetContext;
    p->AwaitReturnValues = procAwaitReturnValues;
}

DPSContext XDPSCreateSimpleContext (
  Display	*dpy,
  Drawable	draw,
  GC		gc,
  int		x,
  int		y,
  DPSTextProc	textProc,
  DPSErrorProc	errorProc,
  DPSSpace	space)
{
  XDPSPrivContext xwh = XDPSCreatePrivContextRec (dpy, draw, gc, x, y,
  						  0, DefaultStdCMap,
						  DefaultStdCMap, 0, false);
  DPSContext newCtxt;
  
  if (xwh == NIL)
    return (NIL);
  else
    {
    newCtxt = DPSCreateContext ((char *) xwh, textProc, errorProc, space);
    if (newCtxt == NIL)
      free ((char *) xwh);
    return (newCtxt);
    }
}


DPSContext XDPSCreateContext (
  Display		*dpy,
  Drawable		draw,
  GC			gc,
  int			x,
  int			y,
  unsigned int		eventmask,
  XStandardColormap	*grayramp,
  XStandardColormap	*ccube,
  int			actual,
  DPSTextProc		textProc,
  DPSErrorProc		errorProc,
  DPSSpace		space)
{
  XDPSPrivContext xwh = XDPSCreatePrivContextRec (dpy, draw, gc, x, y,
  						  eventmask, grayramp,
						  ccube, actual, false);
  DPSContext newCtxt;
  
  if (xwh == NIL)
    return (NIL);
  else
    {
    newCtxt = DPSCreateContext ((char *) xwh, textProc, errorProc, space);
    if (newCtxt == NIL)
      free ((char *) xwh);
    return (newCtxt);
    }
}

DPSContext XDPSCreateSecureContext (
  Display		*dpy,
  Drawable		draw,
  GC			gc,
  int			x,
  int			y,
  unsigned int		eventmask,
  XStandardColormap	*grayramp,
  XStandardColormap	*ccube,
  int			actual,
  DPSTextProc		textProc,
  DPSErrorProc		errorProc,
  DPSSpace		space)
{
  XDPSPrivContext xwh = XDPSCreatePrivContextRec (dpy, draw, gc, x, y,
  						  eventmask, grayramp,
						  ccube, actual, true);
  DPSContext newCtxt;
  
  if (xwh == NIL)
    return (NIL);
  else
    {
    newCtxt = DPSCreateContext ((char *) xwh, textProc, errorProc, space);
    if (newCtxt == NIL)
      free ((char *) xwh);
    return (newCtxt);
    }
}


DPSContext XDPSContextFromSharedID (dpy, cid, textProc, errorProc)
  Display	  *dpy;
  ContextPSID	  cid;
  DPSTextProc	  textProc;
  DPSErrorProc	  errorProc;
{
  DPSPrivContext	c;
  DPSPrivSpace		s; 
  ContextXID		cxid;
  SpaceXID		sxid;
  XDPSPrivContext	xwh;

  if (DPSInitialize () != 0)
    return (NIL);

  c = FindPrivContext (dpy, cid);
  if (c != NIL)
    return ((DPSContext) c);

  xwh = XDPSCreatePrivContextRec (dpy, 0, 0, 0, 0, 0, NIL, NIL, 0, false);
  if (xwh == NIL)
    return (NIL);
  else if (XDPSLIDFromContext (dpy, cid, &cxid, &sxid) != 1)
    {
    free ((char *) xwh);
    return (NIL);
    }
  xwh->cxid = cxid;

  if (spaceProcs == NIL)
    {
    spaceProcs = (DPSSpaceProcs) DPScalloc (sizeof (DPSSpaceProcsRec), 1);
    DPSInitCommonSpaceProcs(spaceProcs);
    }

  s = spaces;
  while (s != NIL)
    if ((SpaceXID)s->sid == sxid && ((XDPSPrivContext) s->wh)->dpy == dpy)
      break;
    else
      s = s->next;

  if (s == NIL)	  /* Create new space record. */
    {
    s = (DPSPrivSpace) DPScalloc (sizeof (DPSPrivSpaceRec), 1);
    s->procs = spaceProcs;
    s->lastNameIndex = -1;
    s->sid = sxid;
    s->wh = (char *) xwh;
    s->creator = false;
    s->next = spaces;
    spaces = s;
    }

  c = (DPSPrivContext) DPScalloc (sizeof (DPSPrivContextRec), 1);
  c->space = (DPSSpace) s;
  c->procs = XDPSconvProcs;
  c->textProc = textProc;
  c->errorProc = errorProc;
  c->programEncoding = DPSDefaultProgramEncoding;
  c->nameEncoding = dps_strings;
  c->next = s->firstContext;
  s->firstContext = c;
  c->lastNameIndex = s->lastNameIndex;
  c->cid = cid;
  c->numstringOffsets = NULL;
  c->creator = false;
  c->zombie = false;
  c->numFormat = XDPSNumFormat (dpy);
  c->wh = (char *) xwh;
  
  xwh->ctxt = (DPSContext) c;

  return ((DPSContext) c);
}


void DPSChangeEncoding (ctxt, newProgEncoding, newNameEncoding)
  DPSContext	     ctxt;
  DPSProgramEncoding newProgEncoding;
  DPSNameEncoding    newNameEncoding;
{
  if (ctxt->programEncoding != newProgEncoding ||
      ctxt->nameEncoding != newNameEncoding)
    {
      DPSPrivContext cc = (DPSPrivContext) ctxt;
      DPSPrivSpace   ss = (DPSPrivSpace) (cc->space);
      
      if ((!cc->creator || !ss->creator) && newNameEncoding != dps_strings)
	{
	DPSSafeSetLastNameIndex(ctxt);
	if (cc->errorProc != NIL)
	  (*cc->errorProc) (ctxt, dps_err_encodingCheck,
			    (unsigned long) newNameEncoding,
			    (unsigned long) newProgEncoding);
	return;
	}
      if (ctxt->procs == textCtxProcs)
        {
        ctxt->programEncoding = newProgEncoding;
        ctxt->nameEncoding = newNameEncoding;
        }
      else
        XDPSSetContextEncoding (ctxt, newProgEncoding, newNameEncoding);
    }
}


DPSSpace XDPSSpaceFromSharedID (dpy, sid)
  Display	*dpy;
  SpaceXID	sid;
{
  DPSPrivSpace		s;
  XDPSPrivContext	xwh;

  if (DPSInitialize () != 0)
    return (NIL);

  if (spaceProcs == NIL)
    {
    spaceProcs = (DPSSpaceProcs) DPScalloc (sizeof (DPSSpaceProcsRec), 1);
    DPSInitCommonSpaceProcs(spaceProcs);
    }

  s = spaces;
  while (s != NIL)
    if ((SpaceXID)s->sid == sid && ((XDPSPrivContext) s->wh)->dpy == dpy)
      break;
    else
      s = s->next;

  if (s == NIL)	  /* Create new space record. */
    {
    xwh = XDPSCreatePrivContextRec (dpy, 0, 0, 0, 0, 0, NIL, NIL, 0, false);
    if (xwh == NIL)
      return (NIL);

    s = (DPSPrivSpace) DPScalloc (sizeof (DPSPrivSpaceRec), 1);
    s->procs = spaceProcs;
    s->lastNameIndex = -1;
    s->sid = sid;
    s->wh = (char *) xwh;
    s->creator = false;
    s->next = spaces;
    spaces = s;
    }

  return ((DPSSpace) s);
}


void XDPSUnfreezeContext (ctxt)
  DPSContext ctxt;
{
  XDPSPrivContext wh = (XDPSPrivContext) (((DPSPrivContext) ctxt)->wh);
  
  if (wh != NIL && wh->cxid != 0)
    XDPSSendUnfreeze (wh->dpy, wh->cxid);
}


ContextXID XDPSXIDFromContext (Pdpy, ctxt)
  Display    **Pdpy;
  DPSContext ctxt;
{
  XDPSPrivContext  xwh = (XDPSPrivContext) (((DPSPrivContext) ctxt)->wh);
  
  if (xwh == NIL || xwh->cxid == 0)
    {
    *Pdpy = NULL;
    return (0);
    }
  else
    {
    *Pdpy = xwh->dpy;
    return (xwh->cxid);
    }
}


SpaceXID XDPSXIDFromSpace (Pdpy, space)
  Display    **Pdpy;
  DPSSpace   space;
{
  DPSPrivSpace	   ss = (DPSPrivSpace) space;
  XDPSPrivContext  xwh = (XDPSPrivContext) ss->wh;
  
  if (xwh != NIL && xwh->dpy != NULL)
    {
    *Pdpy = xwh->dpy;
    return (ss->sid);
    }
  else
    {
    *Pdpy = NULL;
    return (0);
    }
}


DPSContext XDPSContextFromXID (dpy, cxid)
  Display    *dpy;
  ContextXID cxid;
{
  DPSPrivContext c;
  DPSPrivSpace	 ss;
  
  for (ss = spaces;  ss != NIL;  ss = ss->next)
    if (((XDPSPrivContext) ss->wh)->dpy == dpy)
      for (c = ss->firstContext;  c != NIL;  c = c->next)
	if (((XDPSPrivContext) c->wh)->cxid == cxid)
	  return ((DPSContext) c);

  return (NIL);
}


DPSSpace XDPSSpaceFromXID (dpy, sxid)
  Display    *dpy;
  SpaceXID   sxid;
{
  DPSPrivSpace  ss;
  
  for (ss = spaces;  ss != NIL;  ss = ss->next)
    if ((SpaceXID)ss->sid == sxid && ((XDPSPrivContext) ss->wh)->dpy == dpy)
      return ((DPSSpace) ss);

  return (NIL);
}


XDPSStatusProc XDPSRegisterStatusProc (ctxt, statusProc)
  DPSContext	 ctxt;
  XDPSStatusProc statusProc;
{
  DPSPrivContext c = (DPSPrivContext) ctxt;
  XDPSStatusProc old = c->statusProc;
  
  if (c->wh != NIL) c->statusProc = statusProc;
  return old;
}


XDPSReadyProc XDPSRegisterReadyProc (ctxt, readyProc)
  DPSContext	 ctxt;
  XDPSReadyProc readyProc;
{
  DPSPrivContext c = (DPSPrivContext) ctxt;
  XDPSReadyProc old = c->readyProc;
  
  if (c->wh != NIL) c->readyProc = readyProc;
  return old;
}


void XDPSSetStatusMask(ctxt, enableMask, disableMask, nextMask)
  DPSContext ctxt;
  unsigned long enableMask, disableMask, nextMask;
{
  XDPSPrivContext xwh = (XDPSPrivContext) (((DPSPrivContext) ctxt)->wh);
  
  if (xwh != NIL && xwh->cxid != 0)
    XDPSLSetStatusMask(xwh->dpy, xwh->cxid, enableMask, disableMask, nextMask);
}


int XDPSGetContextStatus(ctxt)
  DPSContext ctxt;
{
  DPSPrivContext  c = (DPSPrivContext) ctxt;
  XDPSPrivContext xwh = (XDPSPrivContext) c->wh;
  
  if (xwh != NIL && xwh->cxid != 0)
    return (XDPSLGetStatus(xwh->dpy, xwh->cxid));
  else
    return (0);
}

void XDPSNotifyWhenReady(ctxt, i0, i1, i2, i3)
    DPSContext ctxt;
    int i0, i1, i2, i3;
{
    DPSPrivContext  c = (DPSPrivContext) ctxt;
    XDPSPrivContext xwh = (XDPSPrivContext) c->wh;
    int i[4];

    i[0] = i0;
    i[1] = i1;
    i[2] = i2;
    i[3] = i3;
    
    XDPSLNotifyWhenReady(xwh->dpy, xwh->cxid, i);
}

void XDPSStatusEventHandler (e)
  XDPSLStatusEvent *e;
{
  DPSPrivContext  c = (DPSPrivContext) XDPSContextFromXID(e->display, e->cxid);
  
  if (c == NIL)
    return;
  
  c->statusFromEvent = e->status;
  if (e->status == PSZOMBIE)
    {
    c->zombie = true;
    if (c->resultTable != NIL)  /* Currently waiting for results */
      XDPSQuitBlocking = true;
    }
  
  if (c->statusProc != NIL)
    (*(c->statusProc)) ((DPSContext) c, e->status);
}

void XDPSReadyEventHandler (e)
  XDPSLReadyEvent *e;
{
  DPSPrivContext  c = (DPSPrivContext) XDPSContextFromXID(e->display, e->cxid);
  
  if (c == NIL)
    return;
  
  if (c->readyProc != NIL)
    (*(c->readyProc)) ((DPSContext) c, e->val);
}



void DPSWarnProc(
    DPSContext ctxt,
    char *msg)
{
    DPSErrorProc ep;

    if (DPSInitialize() != 0) return;
    ep = DPSGetCurrentErrorBackstop();
    if (ep == NULL) ep = DPSDefaultErrorProc;
    (*ep)(ctxt, dps_err_warning, (long unsigned int)msg, 0);
}

void DPSFatalProc(
    DPSContext ctxt,
    char *msg)
{
    DPSErrorProc ep;

    if (DPSInitialize() != 0) return;
    ep = DPSGetCurrentErrorBackstop();
    if (ep == NULL) ep = DPSDefaultErrorProc;
    (*ep)(ctxt, dps_err_fatal, (long unsigned int)msg, 0);
}

void DPSCantHappen(void)
{
  static int locked = 0;
  char *msg = "assertion failure or DPSCantHappen";
  if (locked > 0) abort();
  ++locked;
  DPSFatalProc((DPSContext)NULL, msg);
  /* Fatal proc shouldn't return, but client can override and do anything. */
  --locked;
}


/* Procedures for delayed event dispatching */

DPSEventDelivery XDPSSetEventDelivery(
    Display *dpy,
    DPSEventDelivery newMode)
{
    Bool old = XDPSLGetPassEventsFlag(dpy);

    switch (newMode) {
	case dps_event_pass_through:
	    XDPSLSetPassEventsFlag(dpy, True);
	    break;
	case dps_event_internal_dispatch:
	    XDPSLSetPassEventsFlag(dpy, False);
	    break;
	default:
	    break;
    }

    if (old) return dps_event_pass_through;
    else return dps_event_internal_dispatch;
}

Bool XDPSIsStatusEvent(
    XEvent *event,
    DPSContext *ctxt,
    int *status)
{
    Display *d = event->xany.display;
    XExtCodes *c = XDPSLGetCodes(d);
    XDPSLStatusEvent *se = (XDPSLStatusEvent *) event;

    if (c == NULL) return False;	/* Not inited on that display;
					   must be False */

    if (!c->first_event)		/* Check CSDPS first */
        {
	if (XDPSLGetCSDPSFakeEventType(d, event) == csdps_status)
	    {				/* Check CSDPS first */
	    XDPSLGetCSDPSStatus(d, event, (void **)ctxt, status);
	    return True;
	    }
	else
	    return False;
	}

    if (event->type != c->first_event + PSEVENTSTATUS) return False;

    if (ctxt != NULL) *ctxt = XDPSContextFromXID(d, se->cxid);
    if (status != NULL) *status = se->status;
    return True;
}

Bool XDPSIsOutputEvent(
    XEvent *event)
{
    Display *d = event->xany.display;
    XExtCodes *c = XDPSLGetCodes(d);
    CSDPSFakeEventTypes t;

    if (c == NULL) return False;	/* Not inited on that display;
					   must be False */

    if (!c->first_event)		/* Check CSDPS first */
        {
	if ((t = XDPSLGetCSDPSFakeEventType(d, event)) == csdps_output
	  || t == csdps_output_with_len)
	    return True;
	else
	    return False;
        }

    return event->type == c->first_event + PSEVENTOUTPUT;
}

Bool XDPSIsDPSEvent(
    XEvent *event)
{
    Display *d = event->xany.display;
    XExtCodes *c = XDPSLGetCodes(d);

    if (c == NULL) return False;	/* Not inited on that display;
					   must be False */

    if (!c->first_event)		/* Check CSDPS first */
        {
	if (XDPSLGetCSDPSFakeEventType(d, event) != csdps_not)
	    return True;
	else
	    return False;
	}

    return event->type >= c->first_event &&
	   event->type <  c->first_event + NPSEVENTS;
}

Bool XDPSDispatchEvent(
    XEvent *event)
{
    Display *d = event->xany.display;
    XExtCodes *c = XDPSLGetCodes(d);
    CSDPSFakeEventTypes t;

    if (c == NULL) return False;	/* Not inited on that display;
					   must be False */

    if (!c->first_event)		/* Check CSDPS first */
        {
	if ((t = XDPSLGetCSDPSFakeEventType(d, event)) != csdps_not) 
	    return(XDPSLDispatchCSDPSFakeEvent(d, event, t));
	else
	    return False;
        }

    if (event->type == c->first_event + PSEVENTSTATUS) {
	XDPSLCallStatusEventHandler(d, event);
    } else if (event->type == c->first_event + PSEVENTOUTPUT) {
	XDPSLCallOutputEventHandler(d, event);
    } else if (event->type == c->first_event + PSEVENTREADY) {
	XDPSLCallReadyEventHandler(d, event);
    } else return False;
    return True;
}

/* L2-DPS/PROTO 9 addition */
Bool XDPSIsReadyEvent(
    XEvent *event,
    DPSContext *ctxt,
    int *val)
{
    Display *d = event->xany.display;
    XExtCodes *c = XDPSLGetCodes(d);
    XDPSLReadyEvent *re = (XDPSLReadyEvent *) event;

    if (c == NULL) return False;        /* Not inited on that display;
                                           must be False */

    if (!c->first_event)		/* Check CSDPS first */
        {
	if (XDPSLGetCSDPSFakeEventType(d, event) == csdps_ready)
	    {
	    XDPSLGetCSDPSReady(d, event, (void **)ctxt, val);
	    return True;
	    }
	else
	    return False;
	}

    if (event->type != c->first_event + PSEVENTREADY) return False;

    if (ctxt != NULL) *ctxt = XDPSContextFromXID(d, re->cxid);
    if (val != NULL) {
        val[0] = re->val[0];
        val[1] = re->val[1];
        val[2] = re->val[2];
        val[4] = re->val[3];
    }
    return True;
}

int XDPSGetProtocolVersion(
    Display *dpy)
{
    return XDPSLGetVersion(dpy);
}
