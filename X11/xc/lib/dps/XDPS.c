/*
 * XDPS.c -- implementation of low-level Xlib routines for XDPS extension
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
/* $XFree86: xc/lib/dps/XDPS.c,v 1.3 2001/10/28 03:32:42 tsi Exp $ */

#define NEED_EVENTS
#define NEED_REPLIES

#include <stdio.h>
/* Include this first so that Xasync.h, included from Xlibint.h, can find
   the definition of NOFILE */
#include <stdlib.h>
#include <sys/param.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "DPS/XDPS.h"
#include "DPS/XDPSproto.h"
#include "DPS/XDPSlib.h"
#include "DPS/dpsNXargs.h"

#include "cslibint.h"
#include "dpsassert.h"
#include "DPSCAPClient.h"

#include "publictypes.h"
#include "dpsXpriv.h"

/* === DEFINITIONS === */

#ifndef _NFILE
#define DPSMAXDISPLAYS 128
#else
#define DPSMAXDISPLAYS _NFILE
#endif /* _NFILE */

#define MajorOpCode(dpy)	(Codes[DPY_NUMBER(dpy)] ?		\
				 Codes[DPY_NUMBER(dpy)]->major_opcode : \
				 Punt())

#define DPSCAP_INITCTXTS	4  /* per display */

/* === TYPES === */

typedef Status (*PSCMProc)(Display *, XEvent *, xEvent *);

typedef struct {
    char passEvents;
    char wrapWaiting;
    char syncMask;      /* CSDPS only */
    char debugMask;     /* CSDPS only */
} DPSDisplayFlags;

/* For now DPSDisplayFlags is no larger than a pointer.  Revisit this if the
   structure grows. */

typedef int (*GenericProcPtrReturnsInt)(Display *);

typedef struct _t_DPSCAPPausedContextData {
    struct _t_DPSCAPPausedContextData *next;
    Bool paused;
    ContextXID cxid;
    unsigned int seqnum;
} DPSCAPPausedContextData;

typedef struct {
    char showSmallSizes;
    char pixMem;
} DPSCAPAgentArgs;

typedef struct {
    void (*Flush)(Display *);
    int (*Read)(Display*, char*, long);
    void (*ReadPad)(Display*, char*, long);
    Status (*Reply)(Display*, xReply*, int, Bool);
    void (*Send)(Display*, _Xconst char*, long);
} XDPSLIOProcs;

/* === GLOBALS === */

/* For debugging, allows client to force the library to use an agent */
int gForceCSDPS = 0;

/* Force all DPS NX protocol requests to flush if CSDPS */
int gAutoFlush = 1;

/* Force all NX XDPSLGiveInputs to flush independent of gAutoFlush */
int gForceFlush = 1;

/* Quick check for any paused contexts */
int gTotalPaused = 0;

/* === LOCALS === */

/* Common stuff */
static XExtCodes *Codes[DPSMAXDISPLAYS];
static int version[DPSMAXDISPLAYS];
static XDPSLEventHandler StatusProc[DPSMAXDISPLAYS];
static DPSDisplayFlags displayFlags[DPSMAXDISPLAYS];
static XDPSLEventHandler TextProc = NULL;
static XDPSLEventHandler ReadyProc[DPSMAXDISPLAYS]; /* L2-DPS/PROTO 9 */
static int NumberType[DPSMAXDISPLAYS];  /* Garbage okay after dpy closed */
static char *FloatingName[DPSMAXDISPLAYS];  /* Garbage okay after dpy closed */

/* CSDPS stuff */
static Display *ShuntMap[DPSMAXDISPLAYS];
static PSCMProc ClientMsgProc[DPSMAXDISPLAYS];
static GenericProcPtrReturnsInt AfterProcs[DPSMAXDISPLAYS];
static DPSCAPPausedContextData *PausedPerDisplay[DPSMAXDISPLAYS];
static DPSCAPAgentArgs AgentArgs[DPSMAXDISPLAYS];
static unsigned int LastXRequest[DPSMAXDISPLAYS];
static int GCFlushMode[DPSMAXDISPLAYS];

#ifdef VMS
static Display *dpys[DPSMAXDISPLAYS];
static nextDpy = 0;
#endif /* VMS */

static void DPSCAPInitGC(Display *dpy, Display *agent, GC gc);
static Status DPSCAPClientMessageProc(Display *dpy, XEvent *re, xEvent *event);
static int DPSCAPAfterProc(Display *xdpy);
static unsigned int DPSCAPSetPause(Display *xdpy, ContextXID cxid);
static Bool DPSCAPResumeContext(Display *xdpy, ContextXID cxid);
static Bool WaitForSyncProc(Display *xdpy, XEvent *event, char *arg);

static XDPSLIOProcs xlProcs = {  /* Use these for DPS/X extension */
    _XFlush,
    _XRead,
    _XReadPad,
    _XReply,
    _XSend
    };

static XDPSLIOProcs nxlProcs = { /* Use these for NX */
    N_XFlush,
    N_XRead,
    N_XReadPad,
    N_XReply,
    N_XSend
    };

/* === MACROS === */

#define IFNXSETCALL(a, x) call = ((a) != (x)) ? &nxlProcs : &xlProcs

/* === PRIVATE PROCS === */

static int Punt(void)
{
    DPSFatalProc(NULL, "Extension has not been initialized");
    exit(1);
}

#ifdef VMS
/* This is a terribly inefficient way to find a per-display index, but we
   need it till we get dpy->fd fixed in VMS%%%%%*/
static int FindDpyNum(Display *dpy)
{
int i;
for (i=0; dpys[i] != dpy ; i++)
    {
    if (i == nextDpy)
        {
        dpys[nextDpy++]=dpy;
        break;
        }
    }
return i;
}
#define DPY_NUMBER(dpy)         FindDpyNum(dpy)
#else /* VMS */
#define DPY_NUMBER(dpy)		((dpy)->fd)
#endif /* VMS */

/* === PROCEDURES === */

/* ARGSUSED */
void
XDPSLSetTextEventHandler(Display *dpy, XDPSLEventHandler proc)
{
    TextProc = proc;
}

/* ARGSUSED */
void
XDPSLCallOutputEventHandler(Display *dpy, XEvent *event)
{
    (*TextProc)(event);
}

void
XDPSLSetStatusEventHandler(Display *dpy, XDPSLEventHandler proc)
{
    StatusProc[DPY_NUMBER(dpy)] = proc;
}

void
XDPSLCallStatusEventHandler(Display *dpy, XEvent *event)
{
    (*(StatusProc[DPY_NUMBER(dpy)]))(event);
}

/* Added for L2-DPS/PROTO 9 */
void
XDPSLSetReadyEventHandler(Display *dpy, XDPSLEventHandler proc)
{
    ReadyProc[DPY_NUMBER(dpy)] = proc;
}

/* Added for L2-DPS/PROTO 9 */
void
XDPSLCallReadyEventHandler(Display *dpy, XEvent *event)
{
    (*(ReadyProc[DPY_NUMBER(dpy)]))(event);
}

/* Added for L2-DPS/PROTO 9 */
int
XDPSLGetVersion(Display *dpy)
{
    return(version[DPY_NUMBER(dpy)]);
}
/* See CSDPS additions for XDPSLSetVersion */

void
XDPSLInitDisplayFlags(Display *dpy)
{
    int d = DPY_NUMBER(dpy);
    displayFlags[d].wrapWaiting = False;

    /* Instead of explicitly setting the pass-event flag, rely upon the fact
       that it gets initialized to 0 by the compiler.  This means that you
       can set the event delivery mode on a display before creating any
       contexts, which is a Good Thing */
}

XExtCodes *XDPSLGetCodes(Display *dpy)
{
    return Codes[DPY_NUMBER(dpy)];
}

/* ARGSUSED */
static int
CloseDisplayProc(Display *dpy, XExtCodes *codes)
{
    /* This proc is for native DPS/X only, not CSDPS */
    Codes[DPY_NUMBER(dpy)] = NULL;
    /* Clear list */
    XDPSPrivZapDpy(dpy);
#ifdef VMS
    dpys[DPY_NUMBER(dpy)] = NULL;
 /*%%%%Temp till we fix dpy->fd*/
#endif /* VMS */
    return 0;	/* return-value is ignored */
}

Bool
XDPSLGetPassEventsFlag(Display *dpy)
{
    return displayFlags[DPY_NUMBER(dpy)].passEvents;
}

void
XDPSLSetPassEventsFlag(Display *dpy, Bool flag)
{
    displayFlags[DPY_NUMBER(dpy)].passEvents = flag;
}

Bool
XDPSLGetWrapWaitingFlag(Display *dpy)
{
    return displayFlags[DPY_NUMBER(dpy)].wrapWaiting;
}

void
XDPSLSetWrapWaitingFlag(Display *dpy, Bool flag)
{
    displayFlags[DPY_NUMBER(dpy)].wrapWaiting = flag;
}

static Status 
ConvertOutputEvent(Display *dpy, XEvent *ce, xEvent *we)
{
    register PSOutputEvent *wireevent = (PSOutputEvent *) we;
    register XDPSLOutputEvent *clientevent = (XDPSLOutputEvent *) ce;
    
    clientevent->type = wireevent->type & 0x7f;
    clientevent->serial = _XSetLastRequestRead(dpy,
					       (xGenericReply *)wireevent);
    clientevent->send_event = (wireevent->type & 0x80) != 0;
    clientevent->display = dpy;
    clientevent->cxid = wireevent->cxid;
    clientevent->length = wireevent->length;
    bcopy((char *) wireevent->data, clientevent->data, wireevent->length);
    if (TextProc && !XDPSLGetPassEventsFlag(dpy)) {
	(*TextProc)((XEvent *) clientevent);
	return False;
    }
    return True;
}

static Status 
ConvertStatusEvent(Display *dpy, XEvent *ce, xEvent *we)
{
    register PSStatusEvent *wireevent = (PSStatusEvent *) we;
    register XDPSLStatusEvent *clientevent = (XDPSLStatusEvent *) ce;
    
    clientevent->type = wireevent->type & 0x7f;
    clientevent->serial = _XSetLastRequestRead(dpy,
					       (xGenericReply *)wireevent);
    clientevent->send_event = (wireevent->type & 0x80) != 0;
    clientevent->display = dpy;
    clientevent->cxid = wireevent->cxid;
    clientevent->status = wireevent->status;
    if (StatusProc[DPY_NUMBER(dpy)] && !XDPSLGetPassEventsFlag(dpy)) {
	(*(StatusProc[DPY_NUMBER(dpy)]))((XEvent *) clientevent);
	return False;
    }
    return True;
}

/* Added for L2-DPS/PROTO 9 */
static Status
ConvertReadyEvent(Display *dpy, XEvent *ce, xEvent *we)
{
    register PSReadyEvent *wireevent = (PSReadyEvent *) we;
    register XDPSLReadyEvent *clientevent = (XDPSLReadyEvent *) ce;
    
    clientevent->type = wireevent->type & 0x7f;
    clientevent->serial = _XSetLastRequestRead(dpy,
					       (xGenericReply *)wireevent);
    clientevent->send_event = (wireevent->type & 0x80) != 0;
    clientevent->display = dpy;
    clientevent->cxid = wireevent->cxid;
    clientevent->val[0] = wireevent->val1;
    clientevent->val[1] = wireevent->val2;
    clientevent->val[2] = wireevent->val3;
    clientevent->val[3] = wireevent->val4;
    if (ReadyProc[DPY_NUMBER(dpy)] && !XDPSLGetPassEventsFlag(dpy)) {
	(*(ReadyProc[DPY_NUMBER(dpy)]))((XEvent *) clientevent);
	return False;
    }
    return True;
}

/* Added for L2-DPS/PROTO 9 */
/* ARGSUSED */

static int
CatchBadMatch(Display *dpy, xError *err, XExtCodes *codes, int *ret_code)
{
    if (err->errorCode == BadMatch)
        {
	*ret_code = 0;
	return 1;  /* suppress error */
	}
    else
        {
	*ret_code = 1;
	return 0;  /* pass error along */
	}
}


int
XDPSLInit(
    Display *dpy,
    int *numberType,		/* RETURN */
    char **floatingName)	/* RETURN: CALLER MUST NOT MODIFY OR FREE! */
{
    XExtCodes *codes = (XExtCodes *)NULL;
    register xPSInitReq *req;
    xPSInitReply rep;
    char *ptr;
    int first_event;
    
    {
        char *ddt;

        if ((ddt = getenv("DPSNXOVER")) != NULL) {
            gForceCSDPS = (*ddt == 't' || *ddt == 'T');
            if (gForceCSDPS)
                DPSWarnProc(NULL, "*** USING DPS NX ***");
        }
    }
    
    if ((codes = Codes[DPY_NUMBER(dpy)]) != NULL) {
	if (numberType)
	    *numberType = NumberType[DPY_NUMBER(dpy)];
	if (floatingName)
	    *floatingName = FloatingName[DPY_NUMBER(dpy)];
	return codes->first_event;
    } else {
        if (gForceCSDPS)
            goto try_dps_nx;
	codes = XInitExtension(dpy, DPSNAME);
	if (codes == NULL) {
	    /* try DEC UWS 2.2 server */
	    codes = XInitExtension(dpy, DECDPSNAME);
try_dps_nx:
            if (codes == NULL) {
                int myNumberType;
		char *myFloatingName;
		
	        first_event = CSDPSInit(dpy, &myNumberType, &myFloatingName);
		NumberType[DPY_NUMBER(dpy)] = myNumberType;
		FloatingName[DPY_NUMBER(dpy)] = myFloatingName;
		if (numberType)
		    *numberType = myNumberType;
		if (floatingName)
		    *floatingName = myFloatingName;
		return first_event;
	    }
	}
	Codes[DPY_NUMBER(dpy)] = codes;
	ShuntMap[DPY_NUMBER(dpy)] = dpy;
	/* set procs for native DPS/X */
	XESetCloseDisplay(dpy, codes->extension, CloseDisplayProc);
	XESetWireToEvent(dpy, codes->first_event + PSEVENTOUTPUT,
			 ConvertOutputEvent);
	XESetWireToEvent(dpy, codes->first_event + PSEVENTSTATUS,
			 ConvertStatusEvent);
	XESetWireToEvent(dpy, codes->first_event + PSEVENTREADY,
			 ConvertReadyEvent);
	first_event = codes->first_event;
    }

    /* We have to handle a BadMatch error, in the case where
       the client has a later (higher) version of
       the protocol than the server */
    {
    int (*oldErrorProc)(Display*, xError*, XExtCodes*, int*);
    int libVersion;
    Bool doneIt;
    
    XSync(dpy, False);
    LockDisplay(dpy);
    oldErrorProc = XESetError(dpy, codes->extension, CatchBadMatch);
    libVersion = DPSPROTOCOLVERSION;
    doneIt = False;
    while (libVersion >= DPSPROTO_OLDEST)
        {
	GetReq(PSInit, req);
	req->reqType = MajorOpCode(dpy);
	req->dpsReqType = X_PSInit;
	req->libraryversion = libVersion;
        if (_XReply(dpy, (xReply *) &rep, 0, xFalse))
	    {
	    doneIt = True;
	    break;
	    }
	/* otherwise, try previous version */
	--libVersion;
        }
    oldErrorProc = XESetError(dpy, codes->extension, oldErrorProc);
    if (!doneIt)
        {
	DPSFatalProc(NULL, "Incompatible protocol versions");
	exit(1);	
	}
	
    /* NOTE *************************************************
       We made a boo-boo in the 1007.2 and earlier versions of
       our X server glue code.  Instead of sending a
       BadMatch error if the client's version is newer (higher)
       than the server's, it just replies with success.  We
       could test for that situation here by looking at
       rep.serverversion, but it turns out that we don't need
       to do anything special.  Since rep.serverversion gets
       assigned to our version[] array, it is as if we handled
       the BadMatch correctly.  Just for safety's sake, we'll
       do some bulletproofing here instead. 
       Fixes 2ps_xdps BUG #6 */
       
    else if (rep.serverversion < DPSPROTO_OLDEST 
      || rep.serverversion > DPSPROTOCOLVERSION)
        {
	DPSFatalProc(NULL, "Server replied with bogus version");
	exit(1);	
	}
    }

    version[DPY_NUMBER(dpy)] = rep.serverversion;
    NumberType[DPY_NUMBER(dpy)] = rep.preferredNumberFormat;
    if (numberType)
	*numberType = rep.preferredNumberFormat;

    ptr = (char *) Xmalloc(rep.floatingNameLength + 1);
    _XReadPad(dpy, ptr, rep.floatingNameLength);
    ptr[rep.floatingNameLength] = 0;
    FloatingName[DPY_NUMBER(dpy)] = ptr;
    if (floatingName)
	*floatingName = ptr;

    UnlockDisplay(dpy);
    SyncHandle();
    return first_event;
}




static void CopyColorMapsIntoCreateContextReq(
    xPSCreateContextReq *req,
    XStandardColormap *colorcube,
    XStandardColormap *grayramp)
{
    req->cmap = 0;
    if (colorcube != NULL) {
        req->cmap = colorcube->colormap;
	req->redmax = colorcube->red_max;
	req->redmult = colorcube->red_mult;
	req->greenmax = colorcube->green_max;
	req->greenmult = colorcube->green_mult;
	req->bluemax = colorcube->blue_max;
	req->bluemult = colorcube->blue_mult;
	req->colorbase = colorcube->base_pixel;
    }
    else {
	req->redmult = 0;
	/* The rest of this shouldn't be necessary, but there are some
	   servers out there that erroneously check the other fields
	   even when redmult is 0 */
        req->redmax = 0;
        req->greenmult = 0;
        req->greenmax = 0;
        req->bluemult = 0;
        req->bluemax = 0;
        req->colorbase = 0;
    }

    if (grayramp != NULL) {
	req->cmap = grayramp->colormap;
	req->graymax = grayramp->red_max;
	req->graymult = grayramp->red_mult;
	req->graybase = grayramp->base_pixel;
    }
    else req->graymult = 0;
}






/* ARGSUSED */
ContextXID
XDPSLCreateContextAndSpace(
    register Display *xdpy,
    Drawable draw,
    GC gc,
    int x, int y,
    unsigned int eventMask,
    XStandardColormap *grayRamp,
    XStandardColormap *colorCube,
    unsigned int actual,
    ContextPSID *cpsid,		/* RETURN */
    SpaceXID *sxid,		/* RETURN */
    Bool secure)                /* Added for L2-DPS/PROTO 9 */
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    ContextXID cxid;
    register xPSCreateContextReq *req;  /* Same struct for CreateSecureContext */
    xPSCreateContextReply rep;
    XStandardColormap defColorcube, defGrayramp;
    XStandardColormap *requestCube, *requestRamp;
    int index;
    XDPSLIOProcs *call;
    
    if (grayRamp == NULL && colorCube == NULL) return(None);

    if (secure && version[dpyix] < DPSPROTO_V09)
        return(None);                 /* No secure contexts before PROTO 9 */

    /* Index gets encoded as follows:
     *
     *  0  grayRamp = Default,	   colorCube = Default
     *  1  grayRamp = non-Default, colorcube = Default
     *  2  grayRamp = Default,	   colorcube = non-Default
     *  3  grayRamp = non-Default, colorcube = non-Default
     *
     */
    index = ((grayRamp == DefaultStdCMap || grayRamp == NULL) ? 0 : 1) +
	(colorCube == DefaultStdCMap ? 0 : 2);

    switch (index)
	{
	default:
	case 0: /* Both are default */
	    XDPSGetDefaultColorMaps(xdpy, (Screen *) NULL, draw,
				    &defColorcube, &defGrayramp);
	    requestCube = &defColorcube;
	    requestRamp = &defGrayramp;
	    break;

	case 1: /* gray specified, Color default */
	    XDPSGetDefaultColorMaps(xdpy, (Screen *) NULL, draw,
				    &defColorcube, (XStandardColormap *) NULL);
	    requestCube = &defColorcube;
	    requestRamp = grayRamp;
	    break;

	case 2: /* gray default, Color specified */
	    XDPSGetDefaultColorMaps(xdpy, (Screen *) NULL, draw,
				    (XStandardColormap *) NULL, &defGrayramp);
	    requestCube = colorCube;
	    requestRamp = &defGrayramp;
	    break;

	case 3: /* Both specified */
	    requestCube = colorCube;
	    requestRamp = grayRamp;
	    break;
	}

    if (gc != NULL)
        XDPSLFlushGC(xdpy, gc);
    if (dpy != xdpy)
        {
        int syncMask = displayFlags[dpyix].syncMask;
        
	/* Don't worry about pauses here, since we are just
	   now creating the context! */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSCreateContext, req);
    CopyColorMapsIntoCreateContextReq(req, requestCube, requestRamp);

    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = (secure) ? X_PSCreateSecureContext : X_PSCreateContext;
    req->x = x;
    req->y = y;
    req->drawable = draw;
    req->gc = (gc != NULL) ? XGContextFromGC(gc) : None;
    cxid = req->cxid = XAllocID(xdpy);
    req->sxid = XAllocID(xdpy);	  
    if (sxid)
	*sxid = req->sxid;
    req->eventmask = 0;		/* %%% */
    req->actual = actual;
    IFNXSETCALL(dpy, xdpy);
    (void) (*call->Reply) (dpy, (xReply *)&rep, 0, xTrue);

    if (cpsid)
	*cpsid = (int)rep.cpsid;

    UnlockDisplay(dpy);
    /* If the context creation succeeded and we are CSDPS, send GC values */    
    if (rep.cpsid && xdpy != dpy && gc != NULL)
        {
        DPSCAPInitGC(xdpy, dpy, gc);
        }
    SyncHandle();
    
    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
    return (cxid);
}


/* ARGSUSED */
ContextXID
XDPSLCreateContext(
    register Display *xdpy,
    SpaceXID sxid,
    Drawable draw,
    GC gc,
    int x, int y,
    unsigned int eventMask,
    XStandardColormap *grayRamp,
    XStandardColormap *colorCube,
    unsigned int actual,
    ContextPSID *cpsid,		/* RETURN */
    Bool secure)                /* L2-DPS/PROTO 9 addition */
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSCreateContextReq *req;
    xPSCreateContextReply rep;
    ContextXID cxid;			/* RETURN */
    XStandardColormap defColorcube, defGrayramp;
    XStandardColormap *requestCube, *requestRamp;
    int index;
    XDPSLIOProcs *call;

    if (secure && version[dpyix] < DPSPROTO_V09)
        return(None);                /* No secure contexts before PROTO 9 */

    /* Index gets encoded as follows:
     *
     *  0  grayRamp = Default,	   colorCube = Default
     *  1  grayRamp = non-Default, colorcube = Default
     *  2  grayRamp = Default,	   colorcube = non-Default
     *  3  grayRamp = non-Default, colorcube = non-Default
     *
     *  Note that only the first or last case should ever happen.
     */
    index = ((grayRamp == DefaultStdCMap) ? 0 : 1) +
	((colorCube == DefaultStdCMap) ? 0 : 2);

    switch (index)
	{
	default:
	case 0: /* Both are default */
	    XDPSGetDefaultColorMaps(xdpy, (Screen *) NULL, draw,
				    &defColorcube, &defGrayramp);
	    requestCube = &defColorcube;
	    requestRamp = &defGrayramp;
	    break;

	case 1: /* gray specified, Color default */
	    XDPSGetDefaultColorMaps(xdpy, (Screen *) NULL, draw,
				    &defColorcube, (XStandardColormap *) NULL);
	    requestCube = &defColorcube;
	    requestRamp = grayRamp;
	    break;

	case 2: /* gray default, Color specified */
	    XDPSGetDefaultColorMaps(xdpy, (Screen *) NULL, draw,
				    (XStandardColormap *) NULL, &defGrayramp);
	    requestCube = colorCube;
	    requestRamp = &defGrayramp;
	    break;

	case 3: /* Both specified */
	    requestCube = colorCube;
	    requestRamp = grayRamp;
	    break;
	}


    if (gc != NULL)
        XDPSLFlushGC(xdpy, gc);
    if (dpy != xdpy)
        {
        int syncMask = displayFlags[dpyix].syncMask;
        
	/* Don't worry about pauses here, since we are
	   just now creating this context! */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSCreateContext, req);
    CopyColorMapsIntoCreateContextReq(req, requestCube, requestRamp);

    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = (secure) ? X_PSCreateSecureContext : X_PSCreateContext;
    req->x = x;
    req->y = y;
    req->drawable = draw;
    req->gc = (gc != NULL) ? XGContextFromGC(gc) : None;
    cxid = req->cxid = XAllocID(xdpy);
    req->sxid = sxid;
    req->actual = actual;

    IFNXSETCALL(dpy, xdpy);
    (void) (*call->Reply) (dpy, (xReply *)&rep, 0, xTrue);
    if (cpsid)
	*cpsid = (int)rep.cpsid;

    UnlockDisplay(dpy);
    /* If the context creation succeeded and we are CSDPS, send GC values */
    if (rep.cpsid && xdpy != dpy && gc != NULL)
        {
        DPSCAPInitGC(xdpy, dpy, gc);
        }

    SyncHandle();
    
    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
    return cxid;
}

SpaceXID
XDPSLCreateSpace(Display *xdpy)
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSCreateSpaceReq *req;
    SpaceXID sxid;

    LockDisplay(dpy);

    NXMacroGetReq(PSCreateSpace, req);
    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = X_PSCreateSpace;
    sxid = req->sxid = XAllocID(xdpy);

    UnlockDisplay(dpy);
    SyncHandle();
    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
    return sxid;
}



/*
 * I'm not sure how portable my coalescing code is, so I've provided the
 * below define.  If it turns out this code just breaks somewhere, you
 * can simply undefine COALESCEGIVEINPUT, and then everything will work
 * (but slower).  6/16/89 (tw)
 */

#define COALESCEGIVEINPUT

void
XDPSLGiveInput(Display *xdpy, ContextXID cxid, char *data, int length)
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSGiveInputReq *req;
    Bool didFlush = False;

    if (dpy != xdpy)
        {
        register int syncMask = displayFlags[dpyix].syncMask;
	
	if (syncMask & DPSCAP_SYNCMASK_RECONCILE)
	    {
	    XDPSLReconcileRequests(xdpy, cxid);
	    didFlush = True;
	    }
	
	/* If this context got paused, no matter how, ignore
	   mode and resume it */
	if (gTotalPaused && DPSCAPResumeContext(xdpy, cxid))
	    {
	    /* xdpy was flushed by DPSCAPResumeContext */
	    if (!didFlush)
	        {
	        N_XFlush(dpy);
		didFlush = True;
		}
	    }
	else if (syncMask & DPSCAP_SYNCMASK_SYNC)
	    {
	    didFlush = True;
            XSync(xdpy, False);
	    }
        }
    LockDisplay(dpy);
    
#ifdef COALESCEGIVEINPUT
    req = (xPSGiveInputReq *) dpy->last_req;
    if (req->reqType == MajorOpCode(xdpy)
	  && req->dpsReqType == X_PSGiveInput
	  && req->cxid == cxid
	  && dpy->bufptr + length + 3 < dpy->bufmax) {
	bcopy(data, ((char *) req) + sizeof(xPSGiveInputReq) + req->nunits,
	      length);
	req->nunits += length;
	req->length = (sizeof(xPSGiveInputReq) + req->nunits + 3) >> 2;
	dpy->bufptr = dpy->last_req + sizeof(xPSGiveInputReq) +
	    ((req->nunits + 3) & ~3);
    } else
#endif /* COALESCEGIVEINPUT */
    {
        int flushOnce = 1;
	int maxedOutLen = xdpy->max_request_size - sizeof(xPSGiveInputReq) - 4;
	int nunits;
	
	/* We have the rare opportunity to chop up a buffer that is larger
	   than the max request size into separate requests, unlike
	   most other X requests (such as DrawText).  The -4 is to
	   force these maxed out requests to be less than the maximum
	   padding that would ever be needed, and to minimize padding
	   in the case where the input buffer is several times
	   larger than max request length. */
	   
        nunits = maxedOutLen;
	do {
	    if (length < maxedOutLen)
	        nunits = length;  /* Normal size block */
	    NXMacroGetReq(PSGiveInput, req);
	    req->reqType = MajorOpCode(xdpy);
	    req->dpsReqType = X_PSGiveInput;
	    req->cxid = cxid;
	    req->nunits = nunits;
	    req->length += ((nunits + 3) >> 2);
	    if (dpy != xdpy) {
	        if (flushOnce && !didFlush) {
		    LockDisplay(xdpy);
		    _XFlush(xdpy);
		    UnlockDisplay(xdpy);
		    flushOnce = 0;
		}
	        NXProcData(dpy, (char *) data, nunits);
	    } else
	        {Data(dpy, (char *) data, nunits);}
	    data += nunits;
	    length -= nunits;
	} while (length);
    }

    /* In the NX case (didFlush is always False for the non-NX case),
       the xdpy may have been flushed, but there is stuff left
       buffered in dpy (NX connection).  We can't leave the stuff
       there, since we may never call a DPS routine again.  Until
       we can be notified about xdpy being flushed, we have to
       clear out the dpy buffer after we cleared out the xdpy
       buffer (didFlush == True). */

    if (dpy != xdpy
      && dpy->bufptr != dpy->buffer
      && (gForceFlush || didFlush))
	N_XFlush(dpy);

    UnlockDisplay(dpy);
    SyncHandle();
    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
}


int
XDPSLGetStatus(Display *xdpy, ContextXID cxid)
{
    int dpyix;
    Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSGetStatusReq *req;
    xPSGetStatusReply rep;
    XDPSLIOProcs *call;
    
    if (dpy != xdpy)
        {
        register int syncMask = displayFlags[dpyix].syncMask;
        
	/* ASSERT: There is no reason to pause the context for this
	   request, so just sync. */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSGetStatus, req);
    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = X_PSGetStatus;
    req->cxid = cxid;
    req->notifyIfChange = 0;

    IFNXSETCALL(dpy, xdpy);
    if (! (*call->Reply)(dpy, (xReply *)&rep, 0, xTrue))
	rep.status = PSSTATUSERROR;
    UnlockDisplay(dpy);
    SyncHandle();
    /* For CSDPS, guarantee that status events arrive just like DPS/X */
    if (dpy != xdpy)
        {
	XDPSLSync(xdpy);
	LastXRequest[dpyix] = XNextRequest(xdpy)-1;
	}
    return (int) rep.status;
}

void
XDPSLDestroySpace(Display *xdpy, SpaceXID sxid)
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSDestroySpaceReq *req;

    if (dpy != xdpy)
        {
        int syncMask = displayFlags[dpyix].syncMask;
        
	/* ASSERT: There is no reason to pause the context for this
	   request, so just sync. */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSDestroySpace, req);
    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = X_PSDestroySpace;
    req->sxid = sxid;

    if (gAutoFlush && dpy != xdpy)
        {
        N_XFlush(dpy);
        }
    UnlockDisplay(dpy);
    SyncHandle();
    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
}


void
XDPSLReset(Display *xdpy, ContextXID cxid) 
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSResetReq *req;

    if (dpy != xdpy)
        {
        register int syncMask = displayFlags[dpyix].syncMask;
        
	/* ASSERT: There is no reason to pause the context for this
	   request, so just sync. */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSReset, req);
    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = X_PSReset;
    req->cxid = cxid;

    if (gAutoFlush && dpy != xdpy)
        {
        N_XFlush(dpy);
        }
    UnlockDisplay(dpy);
    SyncHandle();
    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
}

void
XDPSLNotifyContext(
    Display *xdpy, 
    ContextXID cxid, 
    int ntype)		  /* should this be an enum?? %%% */
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSNotifyContextReq *req;

    if (dpy != xdpy)
        {
        register int syncMask = displayFlags[dpyix].syncMask;
        
	/* ASSERT: There is no reason to pause the context for this
	   request, so just sync. */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSNotifyContext, req);
    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = X_PSNotifyContext;
    req->cxid = cxid;
    req->notifyType = ntype;
    
    if (dpy != xdpy)
        {
        N_XFlush(dpy);  /* THIS IS CRITICAL TO AVOID HANGING! */
        }

    UnlockDisplay(dpy);
    SyncHandle();

    if (dpy != xdpy)
        {
        if (ntype == PSKILL)
            XDPSLCleanContext(xdpy, cxid);
	LastXRequest[dpyix] = XNextRequest(xdpy)-1;
	}
}


ContextXID
XDPSLCreateContextFromID(
    Display *xdpy, 
    ContextPSID cpsid, 
    SpaceXID *sxid)		/* RETURN */
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSCreateContextFromIDReq *req;
    xPSCreateContextFromIDReply rep;
    ContextXID cxid;
    XDPSLIOProcs *call;

    if (dpy != xdpy)
        {
        int syncMask = displayFlags[dpyix].syncMask;
        
	/* ASSERT: There is no reason to pause the context for this
	   request, so just sync. */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSCreateContextFromID, req);
    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = X_PSCreateContextFromID;
    req->cpsid = cpsid;
    cxid = req->cxid = XAllocID(xdpy);
    
    IFNXSETCALL(dpy, xdpy);
    (void) (*call->Reply) (dpy, (xReply *)&rep, 0, xTrue);
    if (sxid)
	*sxid = (int)rep.sxid;

    UnlockDisplay(dpy);
    SyncHandle();
    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
    return(cxid);
}


/* Returns 1 on success, 0 on failure (cpsid not a valid context). */

Status
XDPSLIDFromContext(
    Display *xdpy,
    ContextPSID cpsid, 
    ContextXID *cxid,			/* RETURN */
    SpaceXID *sxid)			/* RETURN */
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSXIDFromContextReq *req;
    xPSXIDFromContextReply rep;
    XDPSLIOProcs *call;

    if (dpy != xdpy)
        {
        int syncMask = displayFlags[dpyix].syncMask;
        
	/* ASSERT: There is no reason to pause the context for this
	   request, so just sync. */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSXIDFromContext, req);
    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = X_PSXIDFromContext;
    req->cpsid = cpsid;
    
    IFNXSETCALL(dpy, xdpy);
    (void) (*call->Reply) (dpy, (xReply *)&rep, 0, xTrue);
    *sxid = (int)rep.sxid;
    *cxid = (int)rep.cxid;

    UnlockDisplay(dpy);
    SyncHandle();

    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
    return((Status)(*sxid != None && *cxid != None));
}


ContextPSID
XDPSLContextFromXID(Display *xdpy, ContextXID cxid)
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSContextFromXIDReq *req;
    xPSContextFromXIDReply rep;
    XDPSLIOProcs *call;

    if (dpy != xdpy)
        {
        int syncMask = displayFlags[dpyix].syncMask;
        
	/* ASSERT: There is no reason to pause the context for this
	   request, so just sync. */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSContextFromXID, req);
    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = X_PSContextFromXID;
    req->cxid = cxid;
    
    IFNXSETCALL(dpy, xdpy);
    (void) (*call->Reply) (dpy, (xReply *)&rep, 0, xTrue);

    UnlockDisplay(dpy);
    SyncHandle();

    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
    return (int)rep.cpsid;
}


void
XDPSLSetStatusMask(
    Display *xdpy,
    ContextXID cxid,
    unsigned int enableMask,
    unsigned int disableMask,
    unsigned int nextMask)
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSSetStatusMaskReq *req;

    if (dpy != xdpy)
        {
        register int syncMask = displayFlags[dpyix].syncMask;
        
	/* ASSERT: There is no reason to pause the context for this
	   request, so just sync. */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSSetStatusMask, req);
    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = X_PSSetStatusMask;
    req->cxid = cxid;
    req->enableMask = enableMask;
    req->disableMask = disableMask;
    req->nextMask = nextMask;

    if (gAutoFlush && dpy != xdpy)
        {
        N_XFlush(dpy);
        }
    UnlockDisplay(dpy);
    SyncHandle();
    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
}


#ifdef VMS
/*
 * _XReadPad - Read bytes from the socket taking into account incomplete
 * reads.  If the number of bytes is not 0 mod 32, read additional pad
 * bytes. This routine may have to be reworked if int < long.
 */
 
/* This is really in xlib, but is not in the sharable image transfer vector
 * so I am copying it here for now.  BF

The following notice applies only to the functions
_XReadPad and XFlush

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
void
_XReadPad (Display *dpy, char *data, long size)
{
	static int padlength[4] = {0,3,2,1};
	register long bytes_read;
	char pad[3];
 
	CheckLock(dpy);
	if (size == 0) return;
	_XRead( dpy, data, size );
	if ( padlength[size & 3] ) {
	    _XRead( dpy, pad, padlength[size & 3] );
	}

}
#endif /* VMS */

/* _____________ LEVEL 2 DPS/PROTOCOL 9 ADDITIONS _____________ */

void
XDPSLNotifyWhenReady(
    Display *xdpy,
    ContextXID cxid,
    int val[4])
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xPSNotifyWhenReadyReq *req;

    if (version[dpyix] < DPSPROTO_V09)
        {
	DPSWarnProc(NULL, "Attempted use of XDPSLNotifyWhenReady with incompatible server ignored");
        return;                                   /* PROTO 9 or later only */
	}
	
    if (dpy != xdpy)
        {
        register int syncMask = displayFlags[dpyix].syncMask;
        
	if (syncMask & DPSCAP_SYNCMASK_RECONCILE)
	    XDPSLReconcileRequests(xdpy, cxid);
	
	/* If this context got paused, no matter how, ignore
	   mode and resume it */
	if (gTotalPaused && DPSCAPResumeContext(xdpy, cxid))
	    {
	    /* xdpy was flushed by DPSCAPResumeContext */
	    if (gAutoFlush)
	        N_XFlush(dpy);
	    }
	else if (syncMask & DPSCAP_SYNCMASK_SYNC)
            XSync(xdpy, False);
        }
    LockDisplay(dpy);

    NXMacroGetReq(PSNotifyWhenReady, req);
    req->reqType = MajorOpCode(xdpy);
    req->dpsReqType = X_PSNotifyWhenReady;
    req->cxid = cxid;
    req->val1 = val[0];
    req->val2 = val[1];
    req->val3 = val[2];
    req->val4 = val[3];

    if (gAutoFlush && dpy != xdpy)
        {
        N_XFlush(dpy);
        }
    UnlockDisplay(dpy);
    SyncHandle();
    if (dpy != xdpy)
        LastXRequest[dpyix] = XNextRequest(xdpy)-1;
}

XDPSLPSErrors
XDPSLTestErrorCode(Display *dpy, int ecode)
{
    XExtCodes *c = XDPSLGetCodes(dpy);

    if (c == NULL)
        return False;	/* Not inited on that display; must be False */

    switch (ecode - c->first_error)
        {
	case PSERRORBADCONTEXT: return(pserror_badcontext);
	case PSERRORBADSPACE: return(pserror_badspace);
	case PSERRORABORT: 
	    if (version[DPY_NUMBER(dpy)] < DPSPROTO_V09)
	        return(not_pserror);
	    else
	        return(pserror_abort);
	default: return(not_pserror);
        }
}

/* _____________ CLIENT SIDE DPS ADDITIONS _____________ */

/* === NEW HOOKS INTO XDPS === */

void
XDPSLSetVersion(Display *dpy, unsigned ver)
{
    version[DPY_NUMBER(dpy)] = ver;
}

void
XDPSLSetCodes(Display *dpy, XExtCodes *codes)
{
    Codes[DPY_NUMBER(dpy)] = codes;
}

Display *
XDPSLGetShunt(Display *dpy_in)
{
    return(ShuntMap[DPY_NUMBER(dpy_in)]);
}

void
XDPSLSetShunt(Display *dpy_in, Display *dpy_out)
{
    ShuntMap[DPY_NUMBER(dpy_in)] = dpy_out;
}

int
XDPSLGetSyncMask(Display *dpy)
{
    return (int)displayFlags[DPY_NUMBER(dpy)].syncMask;
}

void
XDPSLSetSyncMask(Display *dpy, int mask)
{
    displayFlags[DPY_NUMBER(dpy)].syncMask = (char)mask;
    gForceFlush = (mask & DPSCAP_SYNCMASK_RECONCILE);
}

void
XDPSLFlush(Display *xdpy)
{
    register Display *dpy = ShuntMap[DPY_NUMBER(xdpy)];

    _XFlush(xdpy);
    if (dpy != xdpy)
        N_XFlush(dpy);
}

void
XDPSLSyncGCClip(Display *xdpy, GC gc)
{
    register unsigned long oldDirty;
    register int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];

    /* We DON'T want to notice all gc changes, just the clip */
    oldDirty = gc->dirty;
    gc->dirty = (GCClipXOrigin|GCClipYOrigin);
    XDPSLFlushGC(xdpy, gc);
    gc->dirty = oldDirty;
    if (dpy == xdpy || gNXSyncGCMode != 1) /* 1 means sync always */
        {
	/* For DPS NX and SLOW mode, flushing the gc cache has
	the side-effect of synching agent and server connections.
	So, to have consistent behavior, we sync for the DPS/X
	or FAST cases too. */
	
	if (GCFlushMode[dpyix] == XDPSNX_GC_UPDATES_FAST
	  || dpy == xdpy)
	    XDPSLSync(xdpy);
	}
}


#ifdef VMS
void
XDPSLSetDisplay(Display *dpy)
{
    dpys[DPY_NUMBER(dpy)] = dpy;
}
#endif /* VMS */

char *
XDPSLSetAgentName(Display *dpy, char *name, int deflt)
{
    char *old;
    
    if (gCSDPS == NULL)
        DPSCAPStartUp();
    if (deflt)
        {
        old = gCSDPS->defaultAgentName;
        gCSDPS->defaultAgentName = name;
        }
    else
        {
        old = gCSDPS->map[DPY_NUMBER(dpy)];
        gCSDPS->map[DPY_NUMBER(dpy)] = name;
        }
    return(old);
} 


void
XDPSLSetClientMessageHandler(Display *dpy)
{
    if (dpy == NULL) return;
    ClientMsgProc[DPY_NUMBER(dpy)] = XESetWireToEvent(
      dpy,
      ClientMessage,
      DPSCAPClientMessageProc);
}

void
XDPSLSetAfterProc(Display *xdpy)
{
    if (xdpy == NULL) return;
    AfterProcs[DPY_NUMBER(xdpy)] = (GenericProcPtrReturnsInt)
      XSetAfterFunction(xdpy, DPSCAPAfterProc);
    /* +++ Consider using agent->synchandler to store old proc */
}


CSDPSFakeEventTypes
XDPSLGetCSDPSFakeEventType(Display *dpy, XEvent *event)
{
    XExtCodes *codes = Codes[DPY_NUMBER(dpy)];
    XExtData *extData;
    DPSCAPData my;
    
    if (event->type != ClientMessage || codes == NULL)
        return(csdps_not);
    extData = XFindOnExtensionList(
      CSDPSHeadOfDpyExt(dpy),
      codes->extension);
    if (!extData)
        return(csdps_not);
    my = (DPSCAPData) extData->private_data;
    
    if (event->xclient.message_type == my->typePSOutput)
        return(csdps_output);
    if (event->xclient.message_type == my->typePSOutputWithLen)
        return(csdps_output_with_len);
    if (event->xclient.message_type == my->typePSStatus)
        return(csdps_status);
    if (event->xclient.message_type == my->typeNoop)
        return(csdps_noop);
    if (event->xclient.message_type == my->typePSReady)
        return(csdps_ready);
    return(csdps_not);
}

Bool
XDPSLDispatchCSDPSFakeEvent(
    Display *dpy,
    XEvent *event,
    CSDPSFakeEventTypes t)
{
    register XDPSLOutputEvent *oce;
    register DPSCAPOutputEvent *oev;
    XDPSLOutputEvent fakeOutput;
    XExtCodes *codes = Codes[DPY_NUMBER(dpy)];

    if (codes == NULL)
        return(False);

    /* Fake up an event in the client's format.  Bypasses
       extension wire-to-event conversion */
    switch (t)
        {
        case csdps_output:
            oce = &fakeOutput;
	    oev = (DPSCAPOutputEvent *)event->xclient.data.b;
	    oce->length = DPSCAP_BYTESPEROUTPUTEVENT;
	    goto samo_samo;
	case csdps_output_with_len:
	    oce = &fakeOutput;
	    oev = (DPSCAPOutputEvent *)event->xclient.data.b;
	    oce->length = oev->data[DPSCAP_DATA_LEN];
samo_samo:
	    oce->type = codes->first_event + PSEVENTOUTPUT;
	    oce->serial = event->xclient.serial;
	    oce->send_event = True;  /* ??? */
	    oce->display = dpy;
	    oce->cxid = oev->cxid;
	    bcopy((char *) oev->data, oce->data, oce->length);
	    XDPSLCallOutputEventHandler(dpy, (XEvent *) oce);
	    break;
	case csdps_status:
	    {
	    register XDPSLStatusEvent *sce;
	    register DPSCAPStatusEvent *sev;
	    XDPSLStatusEvent fakeStatus;

	    sev = (DPSCAPStatusEvent *)event->xclient.data.b;
	    sce = &fakeStatus;
	    sce->type = codes->first_event + PSEVENTSTATUS;
	    sce->serial = event->xclient.serial;
	    sce->send_event = True;  /* ??? */
	    sce->display = dpy;
	    sce->status = sev->status;
	    sce->cxid = sev->cxid;
	    XDPSLCallStatusEventHandler(dpy, (XEvent *) sce);
	    break;
	    }
	case csdps_ready:                      /* L2-DPS/PROTO 9 addition */
	    {
	    register XDPSLReadyEvent *rce;
	    XDPSLReadyEvent fakeReady;
	    
	    rce = &fakeReady;
	    rce->type = codes->first_event + PSEVENTREADY;
	    rce->serial = event->xclient.serial;
	    rce->send_event = True;
	    rce->display = dpy;
	    rce->cxid = event->xclient.data.l[0];
	    rce->val[0] = event->xclient.data.l[1];
	    rce->val[1] = event->xclient.data.l[2];
	    rce->val[2] = event->xclient.data.l[3];
	    rce->val[3] = event->xclient.data.l[4];
	    XDPSLCallReadyEventHandler(dpy, (XEvent *) rce);
	    break;
	    }
	default:
	    return(False);
        }
    return(True);
}

extern struct _t_DPSContextRec *XDPSContextFromXID(Display *, ContextXID);

void
XDPSLGetCSDPSStatus(
    Display *xdpy,
    XEvent *event,
    void **ret_ctxt,
    int *ret_status)
{
    register DPSCAPStatusEvent *sev;

    /* Assert: event is ClientMessage with typePSStatus */
    sev = (DPSCAPStatusEvent *)event->xclient.data.b;
   
    if (ret_ctxt != NULL) 
        *ret_ctxt = XDPSContextFromXID(xdpy, sev->cxid);
    if (ret_status != NULL)
        *ret_status = sev->status;
}

void
XDPSLGetCSDPSReady(
    Display *xdpy,
    XEvent *event,
    void **ret_ctxt,
    int *ret_val)
{
    /* Assert: event is ClientMessage with typePSReady */
   
    if (ret_ctxt != NULL) 
        *ret_ctxt = 
	  XDPSContextFromXID(xdpy, (ContextXID)event->xclient.data.l[0]);
    if (ret_val != NULL)
        {
	ret_val[0] = event->xclient.data.l[1];
	ret_val[1] = event->xclient.data.l[2];
	ret_val[2] = event->xclient.data.l[3];
	ret_val[3] = event->xclient.data.l[4];
	}
}

void
XDPSLCAPNotify(
    Display *xdpy, 
    ContextXID cxid, 
    unsigned int ntype,		  /* should this be an enum?? %%% */
    unsigned int data,
    unsigned int extra)
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    register xCAPNotifyReq *req;
    
    if (dpy == xdpy) return;
    
    /* We _have_ to sync client and server here in order to guarantee
       correct execution sequencing.  We call this procedure alot
       to keep track of GC's going away, so this is a major 
       performance hit. */
    if (ntype == DPSCAPNOTE_FREEGC)   
        XSync(xdpy, False);

    LockDisplay(dpy);

    NXMacroGetReq(CAPNotify, req);
    req->reqType = DPSCAPOPCODEBASE;
    req->type = X_CAPNotify;
    req->cxid = cxid;
    req->notification = ntype;
    req->data = data;
    req->extra = extra;
    
    if (gAutoFlush)
        N_XFlush(dpy);

    UnlockDisplay(dpy);
    SyncHandle();
    LastXRequest[dpyix] = XNextRequest(xdpy)-1;
}

void
XDPSLSync(Display *xdpy)
{
    register Display *dpy = ShuntMap[DPY_NUMBER(xdpy)];
    
    if (dpy == xdpy)
        {
        /* DPS/X */
        XSync(dpy, False);
        }
    else
        {
        /* CSDPS */
        XEvent event;
        DPSCAPData my;
        XExtData *extData;
        XExtCodes *codes = Codes[DPY_NUMBER(xdpy)];

	if (codes == NULL)
	    return;
	/* Get private data */
	extData = XFindOnExtensionList(
	  CSDPSHeadOfDpyExt(xdpy),
	  codes->extension);
	if (!extData)
	    return;
        my = (DPSCAPData) extData->private_data;
        my->saveseq = XNextRequest(dpy)-1;
	/* first send notification to agent */
	XDPSLCAPNotify(xdpy, 0, DPSCAPNOTE_SYNC, my->saveseq, 0);
#ifdef XXX
fprintf(stderr, "\nXDPSLSync(DPSCAPNOTE_SYNC) sending ... ");
#endif
        _XFlush(xdpy);
	N_XFlush(dpy);
#ifdef XXX
fprintf(stderr, "sent.\nWaiting for reply ... ");
#endif
	/* agent should send a ClientMessage, so wait for it */
	XIfEvent(xdpy, &event, WaitForSyncProc, (char *) my);

#ifdef XXX
fprintf(stderr, "received.\n");
#endif
	/* now client, agent, and server are synchronized */
        }
}

void
XDPSLReconcileRequests(Display *xdpy, ContextXID cxid)
{
    int dpyix;
    unsigned int seqnum;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];

    if (dpy == xdpy)
        return;  /* No-op for DPS/X */

    /* Get the sequence number and set the pause flag
       IFF we are sure that some X protocol has occurred
       since the last time we did a DPS request.  This
       minimizes pause/resume requests */
    
    if (LastXRequest[dpyix] == XNextRequest(xdpy)-1)
        {
	if (gAutoFlush)
	    N_XFlush(dpy);  /* This is what XDPSLCAPNotify would do */
        return;
	}
    else
        seqnum = DPSCAPSetPause(xdpy, cxid);
    
    /* Pause the context specified. */
    XDPSLCAPNotify(xdpy, cxid, DPSCAPNOTE_PAUSE, seqnum, 0);
    
    /* We don't even need to flush.  All we have to do is make
       sure that the notify request is queued before any
       DPS requests that follow. */
}

Status
XDPSLSetAgentArg(Display *xdpy, int arg, int val)
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    CARD32 capArg;
    register xCAPSetArgReq *req;
    
    if (dpy == xdpy)
        return(Success);  /* No-op for DPS/X */

    /* dpy will be NIL if we haven't opened a connection yet,
       but that's okay since we need to save the value anyway. */
    
    if (dpy)
        {
        int syncMask = displayFlags[dpyix].syncMask;
        
	/* ASSERT: There is no reason to pause the context for this
	   request, so just sync. */
        if (syncMask & (DPSCAP_SYNCMASK_SYNC|DPSCAP_SYNCMASK_RECONCILE))
            XSync(xdpy, False);
        }

    switch (arg)
        {
	case AGENT_ARG_SMALLFONTS:
	    AgentArgs[dpyix].showSmallSizes = val;
	    capArg = DPSCAP_ARG_SMALLFONTS;
	    break;
	case AGENT_ARG_PIXMEM:
	    AgentArgs[dpyix].pixMem = val;
	    capArg = DPSCAP_ARG_PIXMEM;
	    break;
	default:
	    return(!Success);
	}
    if (!dpy)
        return(Success);

    LockDisplay(dpy);

    NXMacroGetReq(CAPSetArg, req);
    req->reqType = DPSCAPOPCODEBASE;
    req->type = X_CAPSetArg;
    req->arg = capArg;
    req->val = val;
    
    if (gAutoFlush)
        N_XFlush(dpy);

    UnlockDisplay(dpy);
    SyncHandle();
    LastXRequest[dpyix] = XNextRequest(xdpy)-1;
    return(Success);
}


void
XDPSLCleanAll(xdpy)
    register Display *xdpy;
{
    /* Clean up all state associated with dpy */
    register DPSCAPPausedContextData *slot;
    int dpyix = DPY_NUMBER(xdpy);

    /* Clean up paused context list */
    for (slot = PausedPerDisplay[dpyix]; slot; slot = PausedPerDisplay[dpyix])
        {
	PausedPerDisplay[dpyix] = slot->next;
	Xfree(slot);
	}

    /* Clear agent args */
    AgentArgs[dpyix].showSmallSizes = 0;
    AgentArgs[dpyix].pixMem = 0;
}

void
XDPSLUpdateAgentArgs(xdpy)
    register Display *xdpy;
{
    int dpyix = DPY_NUMBER(xdpy);
    
    if (AgentArgs[dpyix].showSmallSizes)
        XDPSLSetAgentArg(xdpy, AGENT_ARG_SMALLFONTS, AgentArgs[dpyix].showSmallSizes);
    if (AgentArgs[dpyix].pixMem)
        XDPSLSetAgentArg(xdpy, AGENT_ARG_PIXMEM, AgentArgs[dpyix].pixMem);
}

void 
XDPSLCleanContext(xdpy, cxid)
    Display *xdpy;
    ContextXID cxid;
{
    /* Clean up all state associated with cxid on this dpy */
    register DPSCAPPausedContextData *slot, *prev;
    int dpyix = DPY_NUMBER(xdpy);
    
    /* If this is DPS/X, then slot will never have been initialized.
       See XDPSLNotifyContext */

    /* Clean up paused context list */
    prev = (DPSCAPPausedContextData *)NULL;
    for (slot = PausedPerDisplay[dpyix]; slot; prev = slot, slot = slot->next)
        {
	if (slot->cxid != cxid)
	    continue;
	if (!prev)
	    PausedPerDisplay[dpyix] = slot->next;
	else
	    prev->next = slot->next;
	Xfree(slot);
	break;
	}
}

/* DPS NX 2.0 */
void
XDPSLSetGCFlushMode(dpy, value)
    Display *dpy;
    int value;
{
    int dpyix;
    register Display *agent = ShuntMap[dpyix = DPY_NUMBER(dpy)];

    if (value != XDPSNX_GC_UPDATES_SLOW && value != XDPSNX_GC_UPDATES_FAST)
        {
	DPSWarnProc(NULL, "DPS NX: Bogus GC flush mode.\n");
        return;
	}
    /* 0 means no NX */
    GCFlushMode[dpyix] = (agent == dpy) ? 0 : value;
}

int
XDPSLGetGCFlushMode(dpy)
    Display *dpy;
{
    return(GCFlushMode[DPY_NUMBER(dpy)]);
}

void
XDPSLFlushGC(xdpy, gc)
    Display *xdpy;
    GC gc;
{
    int dpyix;
    register Display *dpy = ShuntMap[dpyix = DPY_NUMBER(xdpy)];
    
    if (!gc->dirty) return;

    if (GCFlushMode[dpyix] == XDPSNX_GC_UPDATES_FAST)
        {
	XGCValues vals;
	static unsigned long valuemask = DPSGCBITS & ~(GCClipMask);
	
	/* Okay to call Xlib, since dpy isn't locked */
	DPSAssertWarn(XGetGCValues(xdpy, gc, valuemask, &vals),
	      NULL, "DPS NX: XGetGCValues returned False\n");
	vals.clip_mask = gc->values.clip_mask;
	LockDisplay(dpy);
	DPSCAPChangeGC(dpy, gc, DPSGCBITS, &vals);
	UnlockDisplay(dpy);
	SyncHandle();
	}
     /* Fall thru.  Either the GCFlushMode is SLOW, which means
        we will DPSCAPChangeGC as a side-effect of FlushGC when
	the GC hook is called, or we just did it in the FAST case. */
     FlushGC(xdpy, gc);
     XDPSLFlush(xdpy);
}

/* === PRIVATE CSDPS PROCS === */

static Status
DPSCAPClientMessageProc(
    Display *dpy,
    XEvent *re,
    xEvent *event)
{
    register XDPSLOutputEvent *oce;
    register DPSCAPOutputEvent *oev;
    XDPSLOutputEvent fakeOutput;
    XExtCodes *codes = Codes[DPY_NUMBER(dpy)];
    PSCMProc oldProc = ClientMsgProc[DPY_NUMBER(dpy)];

    if (codes != NULL)
        {
        /* Get private data */
        XExtData *extData = XFindOnExtensionList(
	  CSDPSHeadOfDpyExt(dpy),
          codes->extension);
        DPSCAPData my;
        
        /* There's no extension, or there is an extension but we are
           passing events uninterpreted, so just pass it along 
           unless it is a DPSCAP error. */
           
        if (!extData)
            goto pass_the_buck;
        my = (DPSCAPData) extData->private_data;
        if (XDPSLGetPassEventsFlag(dpy) && 
            (event->u.clientMessage.u.l.type != my->typeXError))
            goto pass_the_buck;

	/* Fake up a DPS extension event and handle it transparently,
	   without going through the Xlib event queue */
	
	if (event->u.clientMessage.u.b.type == my->typePSOutput)
	    {
	    oce = &fakeOutput;
	    oce->length = DPSCAP_BYTESPEROUTPUTEVENT;
	    oev = (DPSCAPOutputEvent *)event->u.clientMessage.u.b.bytes;
	    goto common_stuff;
	    }
	else if (event->u.clientMessage.u.b.type == my->typePSOutputWithLen)
	    {
	    oce = &fakeOutput;
	    oev = (DPSCAPOutputEvent *)event->u.clientMessage.u.b.bytes;
	    oce->length = oev->data[DPSCAP_DATA_LEN];
common_stuff:
	    oce->type = codes->first_event + PSEVENTOUTPUT;
	    oce->serial = _XSetLastRequestRead(dpy, (xGenericReply *)event);
	    oce->send_event = True;  /* ??? */
	    oce->display = dpy;
	    oce->cxid = oev->cxid;
	    bcopy((char *) oev->data, oce->data, oce->length);
	    /* We've converted the event, give it to DPS */
	    if (TextProc)
		(*TextProc)((XEvent *) oce);
	    return(False);
	    }
	else if (event->u.clientMessage.u.b.type == my->typePSStatus)
	    {
	    register XDPSLStatusEvent *sce;
	    register DPSCAPStatusEvent *sev;
	    XDPSLStatusEvent fakeStatus;

	    sev = (DPSCAPStatusEvent *)event->u.clientMessage.u.b.bytes;
	    sce = &fakeStatus;
	    sce->type = codes->first_event + PSEVENTSTATUS;
	    sce->serial = _XSetLastRequestRead(dpy, (xGenericReply *)event);
	    sce->send_event = True; /* ??? */
	    sce->display = dpy;
	    sce->cxid = sev->cxid;
	    sce->status = sev->status;
	    /* We've converted the event, give it to DPS */
	    if (StatusProc[DPY_NUMBER(dpy)]) 
		(*(StatusProc[DPY_NUMBER(dpy)]))((XEvent *) sce);
	    return(False);
	    }
	else if (event->u.clientMessage.u.l.type == my->typeXError)
	    {
	    xError err;
	    register xError *e = &err;
	    
	    e->type = X_Error;
	    e->errorCode = event->u.clientMessage.u.l.longs0;
	    e->sequenceNumber =  event->u.u.sequenceNumber;
	    e->resourceID = event->u.clientMessage.u.l.longs3;
	    e->minorCode = event->u.clientMessage.u.l.longs2;
	    e->majorCode = event->u.clientMessage.u.l.longs1;
	    /* Smash the wire event here, before going off deep end */
	    event->u.clientMessage.u.l.type = my->typeNoop;
	    /* Jump! */
	    return(_XError(dpy, e));
	    }
	else if (event->u.clientMessage.u.l.type == my->typePSReady)
	    /* L2-DPS/PROTO 9 addition */
	    {
	    register XDPSLReadyEvent *rce;
	    XDPSLReadyEvent fakeReady;
	    
	    rce = &fakeReady;
	    rce->type = codes->first_event + PSEVENTREADY;
	    rce->serial = _XSetLastRequestRead(dpy, (xGenericReply *)event);
	    rce->send_event = True;
	    rce->display = dpy;
	    rce->cxid = event->u.clientMessage.u.l.longs0;
	    rce->val[0] = event->u.clientMessage.u.l.longs1;
	    rce->val[1] = event->u.clientMessage.u.l.longs2;
	    rce->val[2] = event->u.clientMessage.u.l.longs3;
	    rce->val[3] = event->u.clientMessage.u.l.longs4;
	    XDPSLCallReadyEventHandler(dpy, (XEvent *) rce);
	    return(False);
	    }
	}
    
    /* Put the event on the queue, so that Xlib is happy */
pass_the_buck:    
    return(oldProc(dpy, re, event));
}


static void
DPSCAPInitGC(Display *dpy, Display *agent, GC gc)
{
    XGCValues values;
    unsigned long valuemask = DPSGCBITS & ~(GCClipMask);
    
    /* Okay to call Xlib, since dpy isn't locked */
    DPSAssertWarn(XGetGCValues(dpy, gc, valuemask, &values),
         NULL, "DPS NX: XGetGCValues returned False\n");
    values.clip_mask = gc->values.clip_mask;
    DPSCAPChangeGC(agent, gc, DPSGCBITS, &values);
    SyncHandle();
    XDPSLSync(dpy);
}


/* ARGSUSED */

static Bool
WaitForSyncProc(Display *xdpy, XEvent *event, char *arg)
{
    DPSCAPData my = (DPSCAPData)arg;
    
    if ((event->type & 0x7F) == ClientMessage 
	&& event->xclient.message_type == my->typeSync
	&& event->xclient.data.l[0] == (long) my->saveseq) {
      return(True);
    } else {
      return(False);
    }
}


static int
DPSCAPAfterProc(Display *xdpy)
{
    register Display *dpy = ShuntMap[DPY_NUMBER(xdpy)];
    GenericProcPtrReturnsInt proc;
    
    if (dpy != (Display *)NULL && dpy != xdpy)
        {
        LockDisplay(dpy);
        N_XFlush(dpy);
        UnlockDisplay(dpy);
        LockDisplay(xdpy);
        _XFlush(xdpy);
        UnlockDisplay(xdpy);
        }
    if ((proc = AfterProcs[DPY_NUMBER(xdpy)]) != NULL)
        return((*proc)(xdpy));
    else
        return(0);
}


static unsigned int
DPSCAPSetPause(Display *xdpy, ContextXID cxid)
{
    register DPSCAPPausedContextData *slot;
    int dpyix;
    unsigned int ret;

    /* Find or create slot */
    
    slot = PausedPerDisplay[dpyix = DPY_NUMBER(xdpy)];
    if (!slot)
        {
	slot = (DPSCAPPausedContextData *)
	  Xcalloc(1, sizeof(DPSCAPPausedContextData));
	PausedPerDisplay[dpyix] = slot;
	goto common_code;
	/* IMPLICATION: it is okay to fall through common_code
	   and do test_ret. */
	}
    while (1)
        if (slot->cxid == cxid)
	    {
	    if (!slot->paused)
	        {
	        slot->paused = True;
		++gTotalPaused;
		}
	    /* Back-to-back pauses get different sequence numbers */
	    ret = ++slot->seqnum;
	    goto test_ret;
	    }
	else if (slot->next) slot = slot->next;
	else break;
    /* cxid wasn't found, so add it */
    /* ASSERT: slot points to last record of the list */
    slot->next = (DPSCAPPausedContextData *)
      Xcalloc(1, sizeof(DPSCAPPausedContextData));
    slot = slot->next;
common_code:
    slot->paused = True;
    ++gTotalPaused;
    slot->cxid = cxid;
    ret = ++slot->seqnum;
test_ret:
    if (!ret)
        {
	DPSWarnProc(NULL, "Pause sequence wrapped around!");
	}
    return(ret);
}

static Bool
DPSCAPResumeContext(Display *xdpy, ContextXID cxid)
{
    register DPSCAPPausedContextData *slot;
    int dpyix = DPY_NUMBER(xdpy);

    /* Try to match cxid to list of paused contexts */
    for (slot = PausedPerDisplay[dpyix]; slot; slot = slot->next)
        if (slot->cxid == cxid && slot->paused)
	    {
	    /* Send resume event */
	    register XClientMessageEvent *ee;
	    XEvent e;
	    XExtData *extData;
	    DPSCAPData my;
	    XExtCodes *codes = Codes[dpyix];
    
	    extData = XFindOnExtensionList(
		CSDPSHeadOfDpyExt(xdpy),
		codes->extension);
	    if (!extData)
		return(False);
	    my = (DPSCAPData) extData->private_data;

	    ee = &e.xclient;
	    ee->type = ClientMessage;
	    ee->display = xdpy;
	    ee->window = my->agentWindow;
	    ee->format = 32;
	    ee->message_type = my->typeResume;
	    ee->data.l[0] = cxid;
	    ee->data.l[1] = slot->seqnum;
	    (void) XSendEvent(
	      xdpy,
	      my->agentWindow,
	      False,
	      NoEventMask,
	      &e);
	    XFlush(xdpy);
	    /* Turn off flag */
	    slot->paused = False;
	    --gTotalPaused;
	    return(True);
	    }
    /* Fall thru */
    return(False);
}
