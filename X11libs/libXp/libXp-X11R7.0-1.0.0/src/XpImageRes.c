/* $Xorg: XpImageRes.c,v 1.4 2000/08/17 19:46:07 cpqbld Exp $ */
/******************************************************************************
 ******************************************************************************
 **
 ** (c) Copyright 1996 Hewlett-Packard Company
 ** (c) Copyright 1996 International Business Machines Corp.
 ** (c) Copyright 1996 Sun Microsystems, Inc.
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
/* $XFree86: xc/lib/Xp/XpImageRes.c,v 1.4 2001/01/17 19:43:02 dawes Exp $ */


#define NEED_REPLIES

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"


Bool
XpSetImageResolution (
    Display        *dpy,
    XPContext      print_context,
    int            image_res,
    int            *prev_res
)
{
    xPrintSetImageResolutionReq    *req;
    xPrintSetImageResolutionReply  rep;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    if (image_res < 0 || image_res > 65535)
	return (False);

    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return (False);

    LockDisplay (dpy);

    GetReq(PrintSetImageResolution,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintSetImageResolution;
    req->printContext = print_context;
    req->imageRes = image_res;

    if (! _XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return(False);
    }

    UnlockDisplay(dpy);
    SyncHandle();
    if (rep.status && prev_res)
	*prev_res = rep.prevRes;
    return ( rep.status );
}

int
XpGetImageResolution (
    Display        *dpy,
    XPContext      print_context
)
{
    xPrintGetImageResolutionReq    *req;
    xPrintGetImageResolutionReply  rep;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return (-1);

    LockDisplay (dpy);

    GetReq(PrintGetImageResolution,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintGetImageResolution;
    req->printContext = print_context;

    if (! _XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return(-1);
    }

    UnlockDisplay(dpy);
    SyncHandle();
    return ( rep.imageRes );
}
