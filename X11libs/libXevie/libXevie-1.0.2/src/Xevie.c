/* $XdotOrg: xc/lib/Xevie/Xevie.c,v 1.2 2005/05/22 01:20:14 alanc Exp $ */
/************************************************************

Copyright 2003 Sun Microsystems, Inc.

All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, and/or sell copies of the Software, and to permit persons
to whom the Software is furnished to do so, provided that the above
copyright notice(s) and this permission notice appear in all copies of
the Software and that both the above copyright notice(s) and this
permission notice appear in supporting documentation.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

Except as contained in this notice, the name of a copyright holder
shall not be used in advertising or otherwise to promote the sale, use
or other dealings in this Software without prior written authorization
of the copyright holder.

************************************************************/

#define NEED_EVENTS
#define NEED_REPLIES
#include <X11/Xlibint.h>
#include <X11/extensions/Xevie.h>
#include <X11/extensions/Xeviestr.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>

static XExtensionInfo _xevie_info_data;
static XExtensionInfo *xevie_info = &_xevie_info_data;
static char *xevie_extension_name = XEVIENAME;
static int major_opcode = 0;
static long xevie_mask = 0;


/*****************************************************************************
 *                                                                           *
 *			   private utility routines                          *
 *                                                                           *
 *****************************************************************************/

static int close_display();
static /* const */ XExtensionHooks xevie_extension_hooks = {
    NULL,                               /* create_gc */
    NULL,                               /* copy_gc */
    NULL,                               /* flush_gc */
    NULL,                               /* free_gc */
    NULL,                               /* create_font */
    NULL,                               /* free_font */
    close_display,                      /* close_display */
    NULL,                               /* wire_to_event */
    NULL,                               /* event_to_wire */
    NULL,                               /* error */
    NULL,                               /* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, xevie_info,
                                   xevie_extension_name,
                                   &xevie_extension_hooks,
                                   0, NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, xevie_info)

/*****************************************************************************
 *                                                                           *
 *		    public Xevie Extension routines                           *
 *                                                                           *
 *****************************************************************************/

Status
XevieQueryVersion(dpy, major_version_return, minor_version_return)
    Display* dpy;
    int* major_version_return; 
    int* minor_version_return;
{
    XExtDisplayInfo *info = find_display (dpy);
    xXevieQueryVersionReply rep;
    xXevieQueryVersionReq *req;

    XextCheckExtension(dpy, info, xevie_extension_name, False);

    major_opcode = info->codes->major_opcode;
    LockDisplay(dpy);
    GetReq(XevieQueryVersion, req);
    req->reqType = major_opcode;
    req->xevieReqType = X_XevieQueryVersion;
    req->client_major_version = XEVIE_MAJOR_VERSION;
    req->client_minor_version = XEVIE_MINOR_VERSION;
    if (!_XReply(dpy, (xReply *)&rep, 0, xTrue)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    *major_version_return = rep.server_major_version;
    *minor_version_return = rep.server_minor_version;
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

/* Win32 reserves 20 colormap entries for its desktop */
#ifndef TYP_RESERVED_ENTRIES
#define TYP_RESERVED_ENTRIES 20
#endif

Status 
#if NeedFunctionPrototypes
XevieStart(
    Display* dpy)
#else
XevieStart(dpy)
    Display* dpy;
#endif
{
    XExtDisplayInfo *info = find_display (dpy);
    xXevieStartReply rep;
    xXevieStartReq *req;

    XextCheckExtension(dpy, info, xevie_extension_name, False);

    major_opcode = info->codes->major_opcode; 
    LockDisplay(dpy);
    GetReq(XevieStart, req);
    req->reqType = major_opcode;
    req->xevieReqType = X_XevieStart;
    if (_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return(rep.pad1);
}

Status
XevieEnd(dpy)
    Display* dpy;
{
    XExtDisplayInfo *info = find_display (dpy);
    xXevieEndReply rep;
    xXevieEndReq *req;

    XextCheckExtension (dpy, info, xevie_extension_name, False); 

    LockDisplay(dpy);
    GetReq(XevieEnd, req);
    req->reqType = info->codes->major_opcode;
    req->xevieReqType = X_XevieEnd;

    if (_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Status
XevieSendEvent(dpy, event, dataType)
    Display* dpy;
    XEvent*  event;
    char     dataType;
{
    xXevieSendReply rep;
    xXevieSendReq *req;

    LockDisplay(dpy);
    GetReq(XevieSend, req);
    req->reqType = major_opcode;
    req->xevieReqType = X_XevieSend;
    req->dataType = dataType;
    _XEventToWire(dpy, event, &req->event);
    if (_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Status
XevieSelectInput(dpy, event_mask)
    Display* dpy;
    long     event_mask;
{
    xXevieSelectInputReply rep;
    xXevieSelectInputReq *req;

    LockDisplay(dpy);
    GetReq(XevieSelectInput, req);
    req->reqType = major_opcode;
    req->xevieReqType = X_XevieSelectInput;
    req->event_mask = event_mask;
    xevie_mask = event_mask;    
    if (_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

