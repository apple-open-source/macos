/************************************************************

Copyright (c) 2003, Oracle and/or its affiliates. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

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

static int close_display(
    Display *		/* dpy */,
    XExtCodes *		/* codes */
);

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
XevieQueryVersion(
    Display	*dpy,
    int		*major_version_return,
    int		*minor_version_return)
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

Status 
XevieStart(
    Display* dpy)
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
XevieEnd(Display *dpy)
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
XevieSendEvent(
    Display	*dpy,
    XEvent	*event,
    int		 dataType)
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
XevieSelectInput(
    Display	*dpy,
    long	 event_mask)
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

