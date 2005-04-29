/* $Xorg: XpExtVer.c,v 1.4 2000/08/17 19:46:07 cpqbld Exp $ */
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
/* $XFree86: xc/lib/Xp/XpExtVer.c,v 1.6 2003/11/21 05:13:21 dawes Exp $ */

#define NEED_REPLIES

#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"


Bool
XpQueryExtension (
    Display *dpy,
    int     *event_base_return,     /* return value */
    int     *error_base_return      /* return value */
)
{
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    if (XextHasExtension (info)) {
        *event_base_return = info->codes->first_event;
        *error_base_return = info->codes->first_error;
        return True;
    } else {
        return False;
    }
}

/* Prototype for |XpCheckExtInitUnlocked()|
 * See XpExtUtil.c - same as |XpCheckExtInit()| but does not obtain the Xlib global lock */
extern int XpCheckExtInitUnlocked(
    register	Display *dpy,
    register	int	version_index
);
    
Status
XpQueryVersion (
    Display  *dpy,
    short    *major_version,
    short    *minor_version
)
{
    xPrintQueryVersionReq     *req;
    xPrintQueryVersionReply   rep;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    /*
     * Note: many extensions treat major and minor as "ints", but
     * protocol wise and internal wise, they're shorts.
     */
    *major_version = 0;
    *minor_version = 0;

#ifdef Would_Need_xpPrintData_Struct_Def
    /*
     * We may already have the answer cached from a previous query.
     */
    if (( info->data ) && ( ((xpPrintData *) info->data)->vers->present )) {
	*major_version = ((xpPrintData *) info->data)->vers->major_version ;
	*minor_version = ((xpPrintData *) info->data)->vers->minor_version ;
	return (/* non-zero indicates extension present */ True);
    }
#endif /* Would_Need_xpPrintData_Struct_Def */

    if (XpCheckExtInitUnlocked(dpy, XP_DONT_CHECK) == -1)
        return (/* No such extension */ False);

    LockDisplay (dpy);

    GetReq(PrintQueryVersion,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintQueryVersion;

    if (! _XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return(/* No such extension */ False);
    }

    *major_version = rep.majorVersion;
    *minor_version = rep.minorVersion;

    UnlockDisplay(dpy);
    SyncHandle();
    return (/* non-zero indicates extension present */ True);
}

