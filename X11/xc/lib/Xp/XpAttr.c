/* $Xorg: XpAttr.c,v 1.4 2000/08/17 19:46:05 cpqbld Exp $ */
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
/* $XFree86: xc/lib/Xp/XpAttr.c,v 1.7 2002/10/16 00:37:31 dawes Exp $ */

#define NEED_REPLIES

#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"

#define _XpPadOut(len) (((len) + 3) & ~3)


#include <stdio.h>
#include <sys/stat.h>

char *
XpGetAttributes (
    Display       *dpy,
    XPContext     print_context,
    XPAttributes  type
)
{
    char *buf;

    xPrintGetAttributesReq     *req;
    xPrintGetAttributesReply   rep;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);


    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return( (char *) NULL ); /* No such extension */

    LockDisplay (dpy);

    GetReq(PrintGetAttributes,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintGetAttributes;
    req->printContext = print_context;
    req->type = type;

    if (! _XReply (dpy, (xReply *) &rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return( (char *) NULL ); /* No such extension */
    }

    /*
     * Read pool and return to caller.
     */
    buf = Xmalloc( (unsigned) rep.stringLen + 1 );

    if (!buf) {
        UnlockDisplay(dpy);
        SyncHandle();
        return( (char *) NULL ); /* malloc error */
    }

    _XReadPad (dpy, (char *) buf, (long) rep.stringLen );

    buf[rep.stringLen] = 0;

    UnlockDisplay(dpy);
    SyncHandle();

    return( buf );
}


char *
XpGetOneAttribute (
    Display       *dpy,
    XPContext     print_context,
    XPAttributes  type,
    char          *attribute_name
)
{
    char    *buf;

    xPrintGetOneAttributeReq     *req;
    xPrintGetOneAttributeReply   rep;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);


    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return( (char *) NULL ); /* No such extension */

    LockDisplay (dpy);

    GetReq(PrintGetOneAttribute,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintGetOneAttribute;
    req->type = type;
    req->printContext = print_context;
    req->nameLen = strlen( attribute_name );

    /*
     * Attach variable data and adjust request length.
     */
    req->length += _XpPadOut(req->nameLen) >> 2 ;
    Data( dpy, (char *) attribute_name, req->nameLen );    /* n bytes + pad */

    if (! _XReply (dpy, (xReply *) &rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return( (char *) NULL ); /* No such extension */
    }

    /*
     * Read variable answer.
     */
    buf = Xmalloc( (unsigned) rep.valueLen + 1 );

    if (!buf) {
        UnlockDisplay(dpy);
        SyncHandle();
        return( (char *) NULL ); /* malloc error */
    }

    buf[rep.valueLen] = 0;

    _XReadPad (dpy, (char *) buf, (long) rep.valueLen );
    buf[rep.valueLen] = 0;

    UnlockDisplay(dpy);
    SyncHandle();

    return( buf );
}


void
XpSetAttributes (
    Display           *dpy,
    XPContext         print_context,
    XPAttributes      type,
    char              *pool,
    XPAttrReplacement replacement_rule
)
{
    xPrintSetAttributesReq     *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);


    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return; /* No such extension */

    LockDisplay (dpy);

    GetReq(PrintSetAttributes,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintSetAttributes;

    req->printContext = print_context;
    req->type = type;
    req->rule = replacement_rule;

    /*
     * Attach variable data and adjust request length.
     */
    req->stringLen = (CARD32) strlen( (char *) pool );
    req->length += _XpPadOut(req->stringLen) >> 2 ;

    Data( dpy, (char *) pool, req->stringLen );      /* n bytes + pad */

    UnlockDisplay(dpy);
    SyncHandle();

    return;
}

