/* $Xorg: XpDoc.c,v 1.3 2000/08/17 19:46:06 cpqbld Exp $ */
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
/* $XFree86: xc/lib/Xp/XpDoc.c,v 1.3 2001/01/17 19:43:01 dawes Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"

typedef struct {
    int event_base_return;
    XPContext context;
} _XpDiscardDocRec;

static Bool _XpDiscardDoc(Display *dpy, XEvent *event, XPointer arg)
{
    _XpDiscardDocRec *disrec = (_XpDiscardDocRec *) arg;

    if (event->type != disrec->event_base_return + XPPrintNotify)
        return False;

    if (disrec->context != ((XPPrintEvent *) event)->context)
        return False;

    if ((((XPPrintEvent *) event)->detail == XPEndDocNotify) ||
        (((XPPrintEvent *) event)->detail == XPEndPageNotify))  {
        return False;
    }

    return True;
}


void
XpStartDoc (
    Display        *dpy,
    XPDocumentType type
)
{
    xPrintStartDocReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
	return; /* NoSuchExtension NULL */

    LockDisplay (dpy);

    GetReq(PrintStartDoc,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintStartDoc;
    req->type = (unsigned char ) type;

    UnlockDisplay (dpy);
    SyncHandle ();
}

void
XpEndDoc (
    Display  *dpy
)
{
    xPrintEndDocReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);


    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
	return; /* NoSuchExtension NULL */

    LockDisplay (dpy);

    GetReq(PrintEndDoc,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintEndDoc;
    req->cancel = False;

    UnlockDisplay (dpy);
    SyncHandle ();
}


void
XpCancelDoc (
    Display  *dpy,
    Bool     discard
)
{
    xPrintEndDocReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    _XpDiscardDocRec disrec;
    XEvent event;


    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
	return; /* NoSuchExtension NULL */

    LockDisplay (dpy);

    GetReq(PrintEndDoc,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintEndDoc;
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
         * discard doc, and also page XPPrintNotify events.
         */
        while (XCheckIfEvent(dpy, &event, _XpDiscardDoc, (XPointer) &disrec))
        {
            /*EMPTY*/
        }
    }
}

