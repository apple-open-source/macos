/* $XFree86: xc/lib/GL/dri/XF86dri.c,v 1.12 2001/08/27 17:40:57 dawes Exp $ */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright 2000 VA Linux Systems, Inc.
Copyright (c) 2002 Apple Computer, Inc.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Kevin E. Martin <martin@valinux.com>
 *   Jens Owen <jens@valinux.com>
 *   Rickard E. (Rik) Faith <faith@valinux.com>
 *
 */

/* THIS IS NOT AN X CONSORTIUM STANDARD */

#define NEED_EVENTS
#define NEED_REPLIES
#include <Xlibint.h>
#include "appledristr.h"
#include <extensions/Xext.h>
#include <extensions/extutil.h>
#include <stdio.h>

static XExtensionInfo _appledri_info_data;
static XExtensionInfo *appledri_info = &_appledri_info_data;
static char *appledri_extension_name = APPLEDRINAME;

#define AppleDRICheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, appledri_extension_name, val)

/*****************************************************************************
 *                                                                           *
 *			   private utility routines                          *
 *                                                                           *
 *****************************************************************************/

static int close_display(Display *dpy, XExtCodes *extCodes);
static Bool wire_to_event();

static /* const */ XExtensionHooks appledri_extension_hooks = {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    close_display,			/* close_display */
    wire_to_event,			/* wire_to_event */
    NULL,				/* event_to_wire */
    NULL,				/* error */
    NULL,				/* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, appledri_info, 
				   appledri_extension_name, 
				   &appledri_extension_hooks, 
				   AppleDRINumberEvents, NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, appledri_info)

static void (*surface_notify_handler) ();

void *
XAppleDRISetSurfaceNotifyHandler (void (*fun) ())
{
    void *old = surface_notify_handler;
    surface_notify_handler = fun;
    return old;
}

static Bool wire_to_event (dpy, re, event)
    Display *dpy;
    XEvent  *re;
    xEvent  *event;
{
    XExtDisplayInfo *info = find_display (dpy);
    xAppleDRINotifyEvent *sevent;

    AppleDRICheckExtension (dpy, info, False);

    switch ((event->u.u.type & 0x7f) - info->codes->first_event) {
    case AppleDRISurfaceNotify:
	sevent = (xAppleDRINotifyEvent *) event;
	if (surface_notify_handler != NULL)
	{
	    (*surface_notify_handler) (dpy, (unsigned int) sevent->arg,
				       (int) sevent->kind);
	}
	return False;
    }
    return False;
}

/*****************************************************************************
 *                                                                           *
 *		    public Apple-DRI Extension routines                      *
 *                                                                           *
 *****************************************************************************/

#if 0
#include <stdio.h>
#define TRACE(msg)  fprintf(stderr, "AppleDRI%s\n", msg);
#else
#define TRACE(msg)
#endif


Bool XAppleDRIQueryExtension (dpy, event_basep, error_basep)
    Display *dpy;
    int *event_basep, *error_basep;
{
    XExtDisplayInfo *info = find_display (dpy);

    TRACE("QueryExtension...");
    if (XextHasExtension(info)) {
	*event_basep = info->codes->first_event;
	*error_basep = info->codes->first_error;
        TRACE("QueryExtension... return True");
	return True;
    } else {
        TRACE("QueryExtension... return False");
	return False;
    }
}

Bool XAppleDRIQueryVersion(dpy, majorVersion, minorVersion, patchVersion)
    Display* dpy;
    int* majorVersion; 
    int* minorVersion;
    int* patchVersion;
{
    XExtDisplayInfo *info = find_display (dpy);
    xAppleDRIQueryVersionReply rep;
    xAppleDRIQueryVersionReq *req;

    TRACE("QueryVersion...");
    AppleDRICheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(AppleDRIQueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->driReqType = X_AppleDRIQueryVersion;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
        TRACE("QueryVersion... return False");
	return False;
    }
    *majorVersion = rep.majorVersion;
    *minorVersion = rep.minorVersion;
    *patchVersion = rep.patchVersion;
    UnlockDisplay(dpy);
    SyncHandle();
    TRACE("QueryVersion... return True");
    return True;
}

Bool XAppleDRIQueryDirectRenderingCapable(dpy, screen, isCapable)
    Display* dpy;
    int screen;
    Bool* isCapable;
{
    XExtDisplayInfo *info = find_display (dpy);
    xAppleDRIQueryDirectRenderingCapableReply rep;
    xAppleDRIQueryDirectRenderingCapableReq *req;

    TRACE("QueryDirectRenderingCapable...");
    AppleDRICheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(AppleDRIQueryDirectRenderingCapable, req);
    req->reqType = info->codes->major_opcode;
    req->driReqType = X_AppleDRIQueryDirectRenderingCapable;
    req->screen = screen;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
        TRACE("QueryDirectRenderingCapable... return False");
	return False;
    }
    *isCapable = rep.isCapable;
    UnlockDisplay(dpy);
    SyncHandle();
    TRACE("QueryDirectRenderingCapable... return True");
    return True;
}

Bool XAppleDRIAuthConnection(dpy, screen, magic)
    Display* dpy;
    int screen;
    unsigned int magic;
{
    XExtDisplayInfo *info = find_display (dpy);
    xAppleDRIAuthConnectionReq *req;
    xAppleDRIAuthConnectionReply rep;

    TRACE("AuthConnection...");
    AppleDRICheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(AppleDRIAuthConnection, req);
    req->reqType = info->codes->major_opcode;
    req->driReqType = X_AppleDRIAuthConnection;
    req->screen = screen;
    req->magic = magic;
    rep.authenticated = 0;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse) || !rep.authenticated) {
	UnlockDisplay(dpy);
	SyncHandle();
        TRACE("AuthConnection... return False");
	return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    TRACE("AuthConnection... return True");
    return True;
}

Bool XAppleDRICreateSurface(dpy, screen, drawable, client_id, key, uid)
    Display* dpy;
    int screen;
    Drawable drawable;
    unsigned int client_id;
    unsigned int *key;
    unsigned int *uid;
{
    XExtDisplayInfo *info = find_display (dpy);
    xAppleDRICreateSurfaceReply rep;
    xAppleDRICreateSurfaceReq *req;

    TRACE("CreateSurface...");
    AppleDRICheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(AppleDRICreateSurface, req);
    req->reqType = info->codes->major_opcode;
    req->driReqType = X_AppleDRICreateSurface;
    req->screen = screen;
    req->drawable = drawable;
    req->client_id = client_id;
    rep.key_0 = rep.key_1 = rep.uid = 0;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse) || !rep.key_0) {
	UnlockDisplay(dpy);
	SyncHandle();
        TRACE("CreateSurface... return False");
	return False;
    }
    key[0] = rep.key_0;
    key[1] = rep.key_1;
    *uid = rep.uid;
    UnlockDisplay(dpy);
    SyncHandle();
    TRACE("CreateSurface... return True");
    return True;
}

Bool XAppleDRIDestroySurface(dpy, screen, drawable)
    Display* dpy;
    int screen;
    Drawable drawable;
{
    XExtDisplayInfo *info = find_display (dpy);
    xAppleDRIDestroySurfaceReq *req;

    TRACE("DestroySurface...");
    AppleDRICheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(AppleDRIDestroySurface, req);
    req->reqType = info->codes->major_opcode;
    req->driReqType = X_AppleDRIDestroySurface;
    req->screen = screen;
    req->drawable = drawable;
    UnlockDisplay(dpy);
    SyncHandle();
    TRACE("DestroySurface... return True");
    return True;
}
