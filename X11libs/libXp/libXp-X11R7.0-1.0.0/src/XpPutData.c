/* $Xorg: XpPutData.c,v 1.3 2000/08/17 19:46:07 cpqbld Exp $ */
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
/* $XFree86: xc/lib/Xp/XpPutData.c,v 1.3 2001/01/17 19:43:02 dawes Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"

#define _XpPadOut(len) (((len) + 3) & ~3)

void
XpPutDocumentData (
    Display       *dpy,
    Drawable      drawable,
    unsigned char *data,
    int           data_len,
    char          *doc_fmt,
    char          *options
)
{
    xPrintPutDocumentDataReq *req;
    long maxcando, varlen;
    int doc_fmt_len, options_len;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);


    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
	return; /* NoSuchExtension */

    doc_fmt_len = strlen( doc_fmt );
    options_len = strlen( options );

    maxcando = XExtendedMaxRequestSize(dpy);
    if (!maxcando)
	maxcando = XMaxRequestSize(dpy);
    varlen = (_XpPadOut(data_len) +
	      _XpPadOut(doc_fmt_len) + _XpPadOut(options_len)) >> 2;
    if (maxcando < ((sz_xPrintPutDocumentDataReq >> 2) + varlen))
	return;

    LockDisplay (dpy);

    GetReq(PrintPutDocumentData,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintPutDocumentData;
    req->drawable = drawable;
    req->len_data = data_len;
    req->len_fmt = doc_fmt_len;
    req->len_options = options_len;
    SetReqLen(req, varlen, varlen);

    /*
     * Send strings, each with padding provided by Data()
     */
    Data( dpy, (char *) data, data_len );
    Data( dpy, doc_fmt, doc_fmt_len );
    Data( dpy, options, options_len );

    UnlockDisplay (dpy);
    SyncHandle ();
    return; /* Success */
}

