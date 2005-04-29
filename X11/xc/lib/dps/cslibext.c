/*
 *  cslibext.c -- DPSCAP client Xlib extension hookup
 *
 * (c) Copyright 1991-1994 Adobe Systems Incorporated.
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
/* $XFree86: xc/lib/dps/cslibext.c,v 1.5 2003/05/27 22:26:44 tsi Exp $ */

#include <stdio.h>
#include <stdlib.h>

#include <sys/param.h>		/* for MAXHOSTNAMELEN */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include "cslibint.h"
#include <DPS/XDPS.h>
#include <DPS/XDPSproto.h>
#include <DPS/dpsXclient.h>
#include <DPS/dpsNXargs.h>
#include "DPSCAPClient.h"
#include "dpsassert.h"
#include <DPS/XDPSlib.h>

#include "publictypes.h"
#include "dpsXpriv.h"

/* === DEFINES === */

#define DPSNXSYNCGCMODE_FLUSH	0
#define DPSNXSYNCGCMODE_SYNC	1
#define DPSNXSYNCGCMODE_DELAYED	2
#define DPSNXSYNCGCMODE_DEFAULT	DPSNXSYNCGCMODE_DELAYED

/* === GLOBALS === */

DPSCAPGlobals gCSDPS = NULL;

#ifdef DPSLNKL
#define ANXCYAN
#define ANXMAGENTA
#define ANXYELLOW
#define ANXBLACK
#define ANXSPOT
#include "dpslnkl.inc"
#endif /* DPSLNKL */

int gNXSyncGCMode = DPSNXSYNCGCMODE_DEFAULT;

/* === PUBLIC PROCS === */

#ifdef MAHALO
static int
DPSCAPFlushAfterProc(Display *agent)
{
    LockDisplay(agent);
    N_XFlush(agent);
    UnlockDisplay(agent);
}
#endif

int
CSDPSInit(
    Display *dpy,
    int *numberType,		/* RETURN */
    char **floatingName)	/* RETURN */
{
    register Display *agent;
    register xCAPConnReplyPrefix *p;
    register char *c;
    DPSCAPData my;
    xCAPConnSetupReq setup;
    union {
        xCAPConnSuccess good;
        xCAPConnFailed bad;
    } reply;
    XExtData *extData;
    XExtCodes *codes;
    int indian;
    int rest;
    Window clientWindow;
    char fullDisplayName[MAXHOSTNAMELEN+10];

    if (gCSDPS == NULL)
        DPSCAPStartUp();

    /* To fix dps-nx #68, Motif too slow on HP */
    if ((c = getenv("DPSNXGCMODE")) != 0)
        {
	gNXSyncGCMode = atoi(c);
	if (gNXSyncGCMode < DPSNXSYNCGCMODE_FLUSH
	  || gNXSyncGCMode > DPSNXSYNCGCMODE_DELAYED)
	    gNXSyncGCMode = DPSNXSYNCGCMODE_DEFAULT;
	}

    /* We may have already called this routine via XDPSExtensionPresent,
       so don't do it again! */

    if ((codes = XDPSLGetCodes(dpy))
        && (agent = XDPSLGetShunt(dpy))
        && agent != dpy
	&& codes->major_opcode == DPSXOPCODEBASE)
        return(DPSCAPSUCCESS);

    /* Try to create a window for ClientMessage communication */

    clientWindow = XCreateSimpleWindow(
      dpy,
      DefaultRootWindow(dpy),
      0, 0,
      1, 1,
      0,
      BlackPixel(dpy, DefaultScreen(dpy)),
      WhitePixel(dpy, DefaultScreen(dpy)));
    if (clientWindow == None)
        return(DPSCAPFAILED);

    /* Try to open a connection to an agent */

    if ((extData = DPSCAPOpenAgent(dpy, fullDisplayName)) == NULL)
        {
        XDestroyWindow(dpy, clientWindow);
        return(DPSCAPFAILED);
        }

    /* DPSCAPOpenAgent only partially fills in extData, so finish it */

    codes = XAddExtension(dpy);
    codes->major_opcode = DPSXOPCODEBASE;
    codes->first_event = 0;  /* REQUIRED */
    codes->first_error = FirstExtensionError;
    extData->number = codes->extension;
    extData->free_private = DPSCAPDestroy;
    my = (DPSCAPData) extData->private_data;
    my->codes = codes;
    agent = my->agent;
    /* +++ Is this all we have to do here? */

    /* Send opening handshake */

    indian = 1;
    if (*(char *) &indian)
        setup.byteorder = 'l';
    else
    	setup.byteorder = 'B';
    setup.dpscapVersion = DPSCAPPROTOVERSION;
    setup.flags = DPSCAPNONEFLAG;
    setup.libraryversion = DPSPROTOCOLVERSION;
    setup.authProtoNameLength = 0;
    setup.authProtoDataLength = 0;
    setup.displayStringLength = strlen(fullDisplayName);
    setup.nodeStringLength = 0;
    setup.transportStringLength = 0;
    setup.display = 0;
    setup.screen = 0;
    setup.reserved = 0;
    setup.clientWindow = clientWindow;

#ifndef DPSLNKL
    DPSCAPWrite(agent, (char *)&setup, sizeof(xCAPConnSetupReq), dpscap_nopad,dpscap_insert);
    DPSCAPWrite(agent, fullDisplayName, setup.displayStringLength, dpscap_pad, dpscap_append);
    N_XFlush(agent);
#else /* DPSLNKL */
    if (CSDPSConfirmDisplay(agent, dpy, &setup, &reply, fullDisplayName) == 1)
        {
	p = (xCAPConnReplyPrefix *)&reply.good;
        goto skip_read;
	}
    /* Read normal reply */
#endif /* DPSLNKL */

    /* Read common reply prefix */

    p = (xCAPConnReplyPrefix *)&reply.good;
    N_XRead(agent, (char *)p, (long)sizeof(xCAPConnReplyPrefix));
#ifdef DPSLNKL
skip_read:
#endif
    if (!p->success)
        {
	char mbuf[512];
        /* read the rest */
        c = (char *)&reply.bad.serverVersion;
        N_XRead(agent, c, sz_xCAPConnFailed - sz_xCAPConnReplyPrefix);
	sprintf(mbuf, "DPS NX: connection to \"%s\" refused by agent.", DisplayString(agent));
	DPSWarnProc(NULL, mbuf);
        c = (char *)Xmalloc(reply.bad.reasonLength);
        if (!c) return(DPSCAPFAILED);
        N_XReadPad(agent, c, (long)reply.bad.reasonLength);
	if (!reply.bad.reasonLength)
	    sprintf(mbuf, "DPS NX: (no reason given)\n");
	else
            {
	    strcpy(mbuf, "DPS NX: ");
	    strncat(mbuf, c, reply.bad.reasonLength);
	    mbuf[reply.bad.reasonLength+7] = '\0';
	    }
	DPSWarnProc(NULL, mbuf);
        Xfree(c);
        DPSCAPDestroy(extData);
        Xfree(extData);
        XDestroyWindow(dpy, clientWindow);
        return(DPSCAPFAILED);
        }

    /* read the rest of the fixed length reply */
    c = (char *)&reply.good.serverVersion;
    rest = sizeof(xCAPConnSuccess) - sizeof(xCAPConnReplyPrefix);
    N_XRead(agent, c, rest);

    /* verify */

    if (reply.good.serverVersion < DPSPROTOCOLVERSION)
        {
	/* Fine, we downgrade the client */
	char qbuf[256];
	sprintf(qbuf, "NX: server version %ld older than expected %d, client will downgrade", (long)reply.good.serverVersion, DPSPROTOCOLVERSION);
	DPSWarnProc(NULL, qbuf);
	}
    my->dpscapVersion = reply.good.dpscapVersion;
    if (my->dpscapVersion < DPSCAPPROTOVERSION)
        {
	/* Fine, we downgrade the client */
	char kbuf[256];
	sprintf(kbuf, "NX: agent version %d older than expected %d, client will downgrade", my->dpscapVersion, DPSCAPPROTOVERSION);
	DPSWarnProc(NULL, kbuf);
#ifdef XXX
        /* Saving this code as a reminder about what needs to be
	   cleaned up if we exit here */
        DPSCAPDestroy(extData);
        Xfree(extData);
        XDestroyWindow(clientWindow);
        return(DPSCAPFAILED);
#endif
	}

    if (numberType)
        *numberType = reply.good.preferredNumberFormat;

    /* read additional data */

    c = (char *)Xmalloc(reply.good.floatingNameLength + 1);
    N_XReadPad(agent, c, reply.good.floatingNameLength);
    c[reply.good.floatingNameLength] = 0;
    if (floatingName)
	*floatingName = c;
    else
	Xfree(c);

    /* set library extension data */

    XDPSLSetVersion(agent, reply.good.serverVersion);
    XDPSLSetVersion(dpy, reply.good.serverVersion);
    XDPSLSetShunt(dpy, agent);
    XDPSLSetCodes(dpy, codes);
    if (XDPSLGetSyncMask(dpy) == DPSCAP_SYNCMASK_NONE)
        XDPSLSetSyncMask(dpy, DPSCAP_SYNCMASK_DFLT);
    my->agentWindow = reply.good.agentWindow;
    XDPSLSetGCFlushMode(dpy, XDPSNX_GC_UPDATES_SLOW); /* default */

    /* Hook my extension data on the dpy */

    my->extData = extData;
    XAddToExtensionList(CSDPSHeadOfDpyExt(dpy), extData);
    (void) XESetCloseDisplay(dpy, codes->extension, DPSCAPCloseDisplayProc);
    (void) XESetCopyGC(dpy, codes->extension, DPSCAPCopyGCProc);
    (void) XESetFreeGC(dpy, codes->extension, DPSCAPFreeGCProc);
    (void) XESetFlushGC(dpy, codes->extension, DPSCAPFlushGCProc);
    XDPSLSetClientMessageHandler(dpy);

    /* Chain my data on global list */

    my->next = gCSDPS->head;
    gCSDPS->head = my;

#ifdef MAHALO
    /* Set function that is called after every Xlib protocol proc */
    XDPSLSetAfterProc(dpy);

    /* All CSDPS protocol is auto-flushed */
    (void) XSetAfterFunction(agent, DPSCAPFlushAfterProc);
#endif /* MAHALO */

    /* set agent arguments, if needed */
    /* must follow setting of ShuntMap at least, so safest to
       do here when everything has been setup */
    XDPSLUpdateAgentArgs(dpy);

    return(DPSCAPSUCCESS);
}


XExtData **
CSDPSHeadOfDpyExt(
   Display *dpy)
{
   XEDataObject object;

   object.display = dpy;
   return(XEHeadOfExtensionList(object));
}

void
XDPSSyncGCClip(
    register Display *dpy,
    register GC gc)
{
    /* The primary utility of this function is for DPS NX correctness,
       but it also helps DPS/X do less work in tracking GC changes. */
    XDPSLSyncGCClip(dpy, gc);
}

void
XDPSReconcileRequests(
    register DPSContext ctxt)
{
    Display *dpy;
    register ContextXID cxid;
    register DPSContext curCtxt;

    for (curCtxt = ctxt; curCtxt; curCtxt = curCtxt->chainChild)
        {
	cxid = XDPSXIDFromContext(&dpy, curCtxt);
	if (dpy == (Display *)NULL || cxid == None)
	    break;  /* Skip text contexts */
	XDPSLReconcileRequests(dpy, cxid);
        }
}

Status
XDPSNXSetAgentArg(
    Display *dpy,
    int arg, int val)
{
    if (!dpy || arg >= 0 || arg < AGENTLASTARG)
        return(!Success);
    else
        return(XDPSLSetAgentArg(dpy, arg, val));
}

/* New for DPS NX 2.0 */
void
XDPSFlushGC(
    Display *dpy,
    GC gc)
{
    if (dpy && gc)
        XDPSLFlushGC(dpy, gc);
}

/* === SUPPORT PROCS === */

void
DPSCAPChangeGC(
    register Display *agent,
    GC gc,
    unsigned long valuemask,
    XGCValues *values)
{
    register xChangeGCReq *req;
    unsigned long oldDirty = gc->dirty;

    /* ASSERT: called from within LockDisplay section */

    /* Always include the clip_mask */
    valuemask |= GCClipMask;
    /* +++ HACK!  Hide the gc->rects flag in arc_mode */
    valuemask |= GCArcMode;
    valuemask &= (1L << (GCLastBit + 1)) - 1;

    /* Stupid macro insists on Display being called 'dpy' */
    {
    Display *dpy = agent;
    Display *xdpy = (Display *)NULL;
    NXMacroGetReq(ChangeGC, req);
    }
    req->gc = XGContextFromGC(gc);
    gc->dirty = req->mask = valuemask;
    {
/* Derived from MIT XCrGC.c, _XGenerateGCList:
Copyright 1985, 1986, 1987, 1988, 1989 by the
Massachusetts Institute of Technology

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation, and that the name of M.I.T. not be used in advertising or
publicity pertaining to distribution of the software without specific,
written prior permission.  M.I.T. makes no representations about the
suitability of this software for any purpose.  It is provided "as is"
without express or implied warranty.
*/
    /* Warning!  This code assumes that "unsigned int" is 32-bits wide */

    unsigned int vals[32];
    register unsigned int *value = vals;
    long nvalues;
    register XGCValues *gv = values;
    register unsigned long dirty = gc->dirty;

    /*
     * Note: The order of these tests are critical; the order must be the
     * same as the GC mask bits in the word.
     */
    if (dirty & GCFunction)          *value++ = gv->function;
    if (dirty & GCPlaneMask)         *value++ = gv->plane_mask;
    if (dirty & GCForeground)        *value++ = gv->foreground;
    if (dirty & GCBackground)        *value++ = gv->background;
    if (dirty & GCLineWidth)         *value++ = gv->line_width;
    if (dirty & GCLineStyle)         *value++ = gv->line_style;
    if (dirty & GCCapStyle)          *value++ = gv->cap_style;
    if (dirty & GCJoinStyle)         *value++ = gv->join_style;
    if (dirty & GCFillStyle)         *value++ = gv->fill_style;
    if (dirty & GCFillRule)          *value++ = gv->fill_rule;
    if (dirty & GCTile)              *value++ = gv->tile;
    if (dirty & GCStipple)           *value++ = gv->stipple;
    if (dirty & GCTileStipXOrigin)   *value++ = gv->ts_x_origin;
    if (dirty & GCTileStipYOrigin)   *value++ = gv->ts_y_origin;
    if (dirty & GCFont)              *value++ = gv->font;
    if (dirty & GCSubwindowMode)     *value++ = gv->subwindow_mode;
    if (dirty & GCGraphicsExposures) *value++ = gv->graphics_exposures;
    if (dirty & GCClipXOrigin)       *value++ = gv->clip_x_origin;
    if (dirty & GCClipYOrigin)       *value++ = gv->clip_y_origin;
    if (dirty & GCClipMask)          *value++ = gv->clip_mask;
    if (dirty & GCDashOffset)        *value++ = gv->dash_offset;
    if (dirty & GCDashList)          *value++ = gv->dashes;
    /* +++ HACK!  Hide gc->rects flag in GCArcMode */
    if (dirty & GCArcMode)           *value++ = gc->rects;

    req->length += (nvalues = value - vals);

    /*
     * note: Data is a macro that uses its arguments multiple
     * times, so "nvalues" is changed in a separate assignment
     * statement
     */

    nvalues <<= 2;
    Data32 (agent, (long *) vals, nvalues);
    }

    gc->dirty = oldDirty;

    /* ASSERT: SyncHandle called by caller */
}


DPSCAPData
DPSCAPCreate(
    Display *dpy, Display *agent)
{
    register DPSCAPData my = (DPSCAPData)Xcalloc(1, sizeof(DPSCAPDataRec));

    if (my == (DPSCAPData)NULL) return(NULL);
    my->dpy = dpy;
    my->agent = agent;
    my->typePSOutput = XInternAtom(
      dpy,
      DPSCAP_TYPE_PSOUTPUT,
      False);
    my->typePSOutputWithLen = XInternAtom(
      dpy,
      DPSCAP_TYPE_PSOUTPUT_LEN,
      False);
    my->typePSStatus = XInternAtom(
      dpy,
      DPSCAP_TYPE_PSSTATUS,
      False);
    my->typeNoop = XInternAtom(
      dpy,
      DPSCAP_TYPE_NOOP,
      False);
    my->typeSync = XInternAtom(
      dpy,
      DPSCAP_TYPE_SYNC,
      False);
    my->typeXError = XInternAtom(
      dpy,
      DPSCAP_TYPE_XERROR,
      False);
    my->typePSReady = XInternAtom(
      dpy,
      DPSCAP_TYPE_PSREADY,
      False);
    my->typeResume = XInternAtom(
      dpy,
      DPSCAP_TYPE_RESUME,
      False);
    return(my);
}

int
DPSCAPDestroy(
    XExtData *extData)
{
    register DPSCAPData my = (DPSCAPData) extData->private_data;
    register DPSCAPData n;

    if (my == (DPSCAPData)NULL) return(0);
    DPSCAPCloseAgent(my->agent);
    my->agent = NULL;
    /* my->extData->private_data = NIL; ???? +++ */
    if (my == gCSDPS->head)
        gCSDPS->head = my->next;
    else for (n = gCSDPS->head; n != NULL; n = n->next)
        if (n->next == my)
            {
            n->next = my->next;
            break;
            }
    Xfree(my);
    /* extData freed by caller (e.g., _XFreeExtData) */
    return(0);
}

void
DPSCAPStartUp(void)
{
    gCSDPS = (DPSCAPGlobals)Xcalloc(1, sizeof(DPSCAPGlobalsRec));
}


static unsigned char padAdd[] = {0, 3, 2, 1};

void
DPSCAPWrite(
    Display *agent,
    char *buf,
    unsigned len,
    DPSCAPIOFlags writePad,
    DPSCAPIOFlags bufMode)
{
    int pad = padAdd[len & 3];
    unsigned fullLen = (writePad == dpscap_pad) ? len + pad  : len;

    if (agent->bufptr + fullLen > agent->bufmax)
        N_XFlush(agent);
    if (agent->max_request_size && fullLen > agent->max_request_size)
        {
	DPSWarnProc(NULL, "DPS Client Library: request length exceeds max request size. Truncated.\n");
	len = agent->max_request_size;
	pad = 0;
	}
    if (bufMode == dpscap_insert)
        {
        agent->last_req = agent->bufptr;
        agent->request++;
        }
    bcopy(buf, agent->bufptr, len);
    agent->bufptr += len;
    if (writePad == dpscap_pad && pad)
        {
        bcopy((char *) padAdd, agent->bufptr, pad);
        agent->bufptr += pad;
        }
}


/* === EXT CALLBACK HOOKS === */

int
DPSCAPCloseDisplayProc(
    Display *dpy,
    XExtCodes *codes)
{
#ifdef CSDPS
    fprintf(stderr, "NX: Closing agent \"%s\"\n", dpy->display_name);
#endif

    /* Although it seems that we should free codes here, we can't
       because Xlib owns the storage */

    XDPSLSetShunt(dpy, (Display *) NULL);
    XDPSLSetCodes(dpy, (XExtCodes *) NULL);
    XDPSLSetSyncMask(dpy, DPSCAP_SYNCMASK_NONE);
    XDPSLCleanAll(dpy);
    XDPSPrivZapDpy(dpy);
    return(0);
}


int
DPSCAPCopyGCProc(
    Display *dpy,
    GC gc,
    XExtCodes *codes)
{
    XGCValues values;
    DPSCAPData my;
    XExtData *extData = XFindOnExtensionList(
      CSDPSHeadOfDpyExt(dpy),
      codes->extension);

    if (extData)
        my = (DPSCAPData) extData->private_data;
    else
        return(0);

    /* We change the GC unconditionally, since friggin' XCopyGC
       clears the dirty bits of the values that are copied! */

    DPSAssertWarn(XGetGCValues(dpy, gc, DPSGCBITS & ~(GCClipMask), &values),
	NULL, "DPS NX: XGetGCValues returned False\n");
    values.clip_mask = gc->values.clip_mask;
    DPSCAPChangeGC(my->agent, gc, DPSGCBITS, &values);
    /* We have to make sure that the agent completely processes
	the change to the GC.  If we allow the agent to update the
	GC in its own sweet time, the stupid client may delete the
	GC after the agent has already queued a request to, e.g.,
	copy the GC, but before the request is flushed. */
    XDPSLSync(dpy);
    return(1);
}

int
DPSCAPFreeGCProc(
    Display *pdpy,
    GC gc,
    XExtCodes *codes)
{
    register xCAPNotifyReq *req;
    DPSCAPData my;
    Display *dpy = pdpy;  /* Stupid macros insists on Display being 'dpy' */
    XExtData *extData = XFindOnExtensionList(
      CSDPSHeadOfDpyExt(dpy),
      codes->extension);

    if (extData)
        my = (DPSCAPData) extData->private_data;
    else
        return(0);

    /* Notify the agent that the client deleted a GC.  Let the
       agent figure out if it cares. */

    /* ASSERT: called from within LockDisplay section */

    dpy = my->agent;
    if (dpy == (Display *)NULL || dpy == pdpy)
        return(0);

    /* May need to sync changes to GC */
    if (gNXSyncGCMode == DPSNXSYNCGCMODE_DELAYED)
        XDPSLSync(pdpy);

    {
    Display *xdpy = pdpy;			    /* pdpy is X server */
    NXMacroGetReq(CAPNotify, req);
    }
    req->reqType = DPSCAPOPCODEBASE;
    req->type = X_CAPNotify;
    req->cxid = 0;
    req->notification = DPSCAPNOTE_FREEGC;
    req->data = XGContextFromGC(gc);
    req->extra = 0;
    /* Guarantee that everyone sees GC go away */
    XSync(pdpy, False);                             /* pdpy is X server */
    if (gNXSyncGCMode == DPSNXSYNCGCMODE_FLUSH)
        {
        LockDisplay(dpy);                           /* dpy means agent here */
        N_XFlush(dpy);
        UnlockDisplay(dpy);
	}
    else
        XDPSLSync(pdpy);

    /* ASSERT: SynchHandle called by caller */
    return(1);
}

#ifdef CSDPSDEBUG
static unsigned int gcCountFlushedClean = 0;
static unsigned int gcCountFlushedDirty = 0;
#endif /* CSDPSDEBUG */

int
DPSCAPFlushGCProc(
    Display *dpy,
    GC gc,
    XExtCodes *codes)
{
    XGCValues values;
    DPSCAPData my;
    XExtData *extData;
#ifdef CSDPSDEBUG
    unsigned long int dirty;
#endif /* CSDPSDEBUG */

    /* When GC is created, it is flushed with no dirty bits set,
       so we have to notice that situation. */

    if (gc->dirty)
        {
	if (XDPSLGetGCFlushMode(dpy) == XDPSNX_GC_UPDATES_FAST
	   || !(gc->dirty & DPSGCBITS))
	    return(0);
	}
    extData = XFindOnExtensionList(CSDPSHeadOfDpyExt(dpy), codes->extension);
    if (extData)
        my = (DPSCAPData) extData->private_data;
    else
        return(0);
    /* HERE IF (gc->dirty & DPSGCBITS || !gc->dirty) */
#ifdef CSDPSDEBUG
    dirty = gc->dirty;
#endif /* CSDPSDEBUG */
    DPSAssertWarn(XGetGCValues(dpy, gc, DPSGCBITS & ~(GCClipMask), &values),
	NULL, "NX: XGetGCValues returned False\n");
    values.clip_mask = gc->values.clip_mask;
    /* Must guarantee that gc change is registered by X server
       before notification is sent to agent. */
    XSync(dpy, False);
    DPSCAPChangeGC(my->agent, gc, DPSGCBITS, &values);
    /* We have to make sure that the agent completely processes
       the change to the GC.  If we allow the agent to update the
       GC in its own sweet time, the stupid client may delete the
       GC after the agent has already queued a request to, e.g.,
       copy the GC, but before the request is flushed. */
    if (gNXSyncGCMode == DPSNXSYNCGCMODE_SYNC)
        XDPSLSync(dpy);
    else
        XDPSLFlush(dpy);
#ifdef CSDPSDEBUG
    if (dirty)
        ++gcCountFlushedDirty;
    else
        ++gcCountFlushedClean;
#endif /* CSDPSDEBUG */
    return(1);
}
