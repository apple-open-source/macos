/* $Xorg: XpPage.c,v 1.4 2001/03/06 13:59:02 pookie Exp $ */
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
/* $XFree86: xc/lib/Xp/XpPage.c,v 1.4 2001/12/19 21:28:44 dawes Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"


typedef struct {
    int event_base_return;
    XPContext context;
} _XpDiscardPageRec;

static Bool _XpDiscardPage(Display *dpy, XEvent *event, XPointer arg)
{
    _XpDiscardPageRec *disrec = (_XpDiscardPageRec *) arg;

    if (event->type != disrec->event_base_return + XPPrintNotify)
        return False;

    if (disrec->context != ((XPPrintEvent *) event)->context)
        return False;

    if (((XPPrintEvent *) event)->detail == XPEndPageNotify)  {
        return False;
    }

    return True;
}


void
XpStartPage (
    Display  *dpy,
    Window   window
)
{
    xPrintStartPageReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    XMapWindow(dpy, window);
    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
	return; /* NoSuchExtension */

    LockDisplay (dpy);

    GetReq(PrintStartPage,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintStartPage;
    req->window = window;

    UnlockDisplay (dpy);
    SyncHandle ();
}


void
XpEndPage (
    Display  *dpy
)
{
    xPrintEndPageReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
	return; /* NoSuchExtension */

    LockDisplay (dpy);

    GetReq(PrintEndPage,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintEndPage;
    req->cancel = False;

    UnlockDisplay (dpy);
    SyncHandle ();
}


void
XpCancelPage (
    Display  *dpy,
    Bool     discard
)
{
    xPrintEndPageReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    _XpDiscardPageRec disrec;
    XEvent event;


    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
	return; /* NoSuchExtension */

    LockDisplay (dpy);

    GetReq(PrintEndPage,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintEndPage;
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
         * discard page XPPrintNotify events.
         */
        while (XCheckIfEvent(dpy, &event, _XpDiscardPage, (XPointer) &disrec))
        {
            /*EMPTY*/
        }
    }
}

