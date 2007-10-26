/* $Xorg: XpContext.c,v 1.4 2000/08/17 19:46:05 cpqbld Exp $ */
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
/* $XFree86: xc/lib/Xp/XpContext.c,v 1.6 2001/10/28 03:32:39 tsi Exp $ */

#define NEED_REPLIES

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"

#define _XpPadOut(len) (((len) + 3) & ~3)

XPContext
XpCreateContext (
    Display  *dpy,
    char     *printer_name
)
{
    xPrintCreateContextReq     *req;
    char                        *locale;
    int                         locale_len;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);


    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return ( (XPContext) None ); /* No such extension */

    /*
     * Fetch locale information.  Note: XpGetLocaleNetString has
     * a thread-safe mutex on _Xglobal_lock.
     */
    locale = XpGetLocaleNetString();

    LockDisplay (dpy);

    GetReq(PrintCreateContext,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintCreateContext;

    req->contextID      = XAllocID(dpy);
    req->printerNameLen = strlen(printer_name);

    if ( locale == (char *) NULL )
        req->localeLen = 0;
    else if ( *locale == (char) NULL )
        req->localeLen = 0;
    else {
        locale_len     = strlen( locale );
        req->length   += _XpPadOut(locale_len) >> 2;
        req->localeLen = (unsigned long) locale_len;
    }

    /*
     * Attach variable data
     */
    req->length += _XpPadOut(req->printerNameLen) >> 2;
    Data( dpy, (char *) printer_name, (long) req->printerNameLen );

    if (req->localeLen)
	Data( dpy, (char *) locale, (long) req->localeLen );

    UnlockDisplay(dpy);
    SyncHandle();

    XFree(locale);

    return ( (XPContext) req->contextID );
}


void
XpSetContext (
    Display         *dpy,
    XPContext  print_context
)
{
    xPrintSetContextReq     *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);


    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return; /* No such extension */

    LockDisplay (dpy);

    GetReq(PrintSetContext,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintSetContext;

    req->printContext = print_context;

    UnlockDisplay(dpy);
    SyncHandle();

    return;
}


XPContext
XpGetContext (
    Display  *dpy
)
{
    xPrintGetContextReq     *req;
    xPrintGetContextReply   rep;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);


    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return ( (XPContext) None ); /* No such extension */

    LockDisplay (dpy);

    GetReq(PrintGetContext,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintGetContext;

    if (! _XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return ( (XPContext) None ); /* No such extension */
    }

    UnlockDisplay(dpy);
    SyncHandle();

    return ( (XPContext) rep.printContext );
}


void
XpDestroyContext (
    Display    *dpy,
    XPContext  print_context
)
{
    xPrintDestroyContextReq   *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);


    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return; /* No such extension */

    LockDisplay (dpy);

    GetReq(PrintDestroyContext,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintDestroyContext;
    req->printContext = print_context;

    UnlockDisplay(dpy);
    SyncHandle();

    return;
}



Screen *
XpGetScreenOfContext (
    Display    *dpy,
    XPContext  print_context
)
{
    xPrintGetContextScreenReq     *req;
    xPrintGetContextScreenReply   rep;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    int    i;
    Screen *checkScr = NULL;
    int    ok;


    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return ( (Screen *) NULL ); /* No such extension */

    LockDisplay (dpy);

    GetReq(PrintGetContextScreen,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintGetContextScreen;

    req->printContext = print_context;

    if (! _XReply (dpy, (xReply *) &rep, 0, xTrue)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return ( (Screen *) NULL ); /* No such extension */
    }

    /*
     * Pull rootWindow ID and convert to the corresponding
     * Screen rec.
     */
    ok = False;

    for ( i = 0; i < XScreenCount(dpy); i++ ) {
	checkScr = XScreenOfDisplay(dpy, i);
	if ( XRootWindowOfScreen( checkScr ) == (Window) rep.rootWindow  ) {
	    ok = True;
	    break;
	}
    }

    if (!ok)
	checkScr = (Screen *) NULL;

    UnlockDisplay(dpy);
    SyncHandle();

    return ( (Screen *) checkScr );
}

