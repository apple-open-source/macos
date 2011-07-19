/******************************************************************************
 ******************************************************************************
 **
 ** (c) Copyright 1996 Hewlett-Packard Company
 ** (c) Copyright 1996 International Business Machines Corp.
 ** (c) Copyright 1996, Oracle and/or its affiliates. All rights reserved.
 ** (c) Copyright 1996 Novell, Inc.
 ** (c) Copyright 1996 Digital Equipment Corp.
 ** (c) Copyright 1996 Fujitsu Limited
 ** (c) Copyright 1996 Hitachi, Ltd.
 ** 
 ** Permission is hereby granted, free of charge, to any person obtaining a copy
 ** of this software and associated documentation files (the "Software"), to deal
 ** in the Software without restriction, including without limitation the rights
 ** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 ** copies of the Software, and to permit persons to whom the Software is
 ** furnished to do so, subject to the following conditions:
 **
 ** The above copyright notice and this permission notice shall be included in
 ** all copies or substantial portions of the Software.
 **
 ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 ** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 ** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 ** COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 ** IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 ** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 **
 ** Except as contained in this notice, the names of the copyright holders shall
 ** not be used in advertising or otherwise to promote the sale, use or other
 ** dealings in this Software without prior written authorization from said
 ** copyright holders.
 **
 ******************************************************************************
 *****************************************************************************/

#if defined(sun) && defined(i386) && defined(SVR4) && !defined(__EXTENSIONS__)
#define __EXTENSIONS__
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include <X11/Xos.h>
#include "XpExtUtil.h"
#include <limits.h>
#ifdef XTHREADS
#include <X11/Xthreads.h>
#endif
#ifndef WIN32
#define X_INCLUDE_PWD_H
#define XOS_USE_XLIB_LOCKING
#include <X11/Xos_r.h>
#endif
#include <limits.h>


typedef struct {
    int event_base_return;
    XPContext context;
} _XpDiscardJobRec;

static Bool _XpDiscardJob(Display *dpy, XEvent *event, XPointer arg)
{
    _XpDiscardJobRec *disrec = (_XpDiscardJobRec *) arg;

    if (event->type != disrec->event_base_return + XPPrintNotify)
	return False;

    if (disrec->context != ((XPPrintEvent *) event)->context)
	return False;
    
    if ((((XPPrintEvent *) event)->detail == XPEndJobNotify) ||
	(((XPPrintEvent *) event)->detail == XPEndDocNotify) ||
	(((XPPrintEvent *) event)->detail == XPEndPageNotify))  {
	return False;
    }

    return True;
}


void
XpStartJob (
    Display    *dpy,
    XPSaveData save_data
)
{
    xPrintStartJobReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    XPContext    context;


    /****************************************************************
     *
     * PRIOR TO XPSTARTJOB, set the job attribute "job-owner"
     * which will be used by the X-Server when it spools the
     * output.  When XpStartJob completes, the job attribute
     * pool is frozen, disallowing "job-owner" to be modified.
     */
    {
	char            *joa;		/* job owner attribute */
 	char *PwName;
#ifndef WIN32
#ifdef X_NEEDS_PWPARAMS
	_Xgetpwparams pwparams;
#endif
	struct passwd *pw;
	pw = _XGetpwuid(getuid(),pwparams);

	if (pw && (PwName = pw->pw_name)) {
#else
	if ((PwName = getenv("USERNAME"))) {
#endif
	    joa = (char *) Xmalloc( strlen( PwName ) + 20 );
	    sprintf( joa, "*job-owner: %s", PwName );
            context = XpGetContext( dpy );
	    XpSetAttributes( dpy, context, XPJobAttr, joa, XPAttrMerge );

	    Xfree( joa );
	}
    }

    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
	return; /* NoSuchExtension */

    LockDisplay (dpy);

    GetReq(PrintStartJob,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintStartJob;
    req->saveData = (CARD8) save_data;

    UnlockDisplay (dpy);
    SyncHandle ();
}


void
XpEndJob (
    Display  *dpy
)
{
    xPrintEndJobReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
	return; /* NoSuchExtension */

    LockDisplay (dpy);

    GetReq(PrintEndJob,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintEndJob;
    req->cancel = False;

    UnlockDisplay (dpy);
    SyncHandle ();
}


void
XpCancelJob (
    Display  *dpy,
    Bool     discard
)
{
    xPrintEndJobReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    _XpDiscardJobRec disrec;
    XEvent event;


    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
	return; /* NoSuchExtension */

    LockDisplay (dpy);

    GetReq(PrintEndJob,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintEndJob;
    req->cancel = True;

    UnlockDisplay (dpy);
    SyncHandle ();

    if (discard) {
	/*
	 * get context and effectively do a sync of events.
	 */
	disrec.event_base_return = info->codes->first_event;
	disrec.context = XpGetContext(dpy);

	/*
	 * discard job, and also doc and page XPPrintNotify events.
	 */
	while (XCheckIfEvent(dpy, &event, _XpDiscardJob, (XPointer) &disrec))
	{
	    /*EMPTY*/
	}
    }
}

