/* $Xorg: XpPrinter.c,v 1.4 2000/08/17 19:46:07 cpqbld Exp $ */
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
/* $XFree86: xc/lib/Xp/XpPrinter.c,v 1.9 2002/10/16 00:37:32 dawes Exp $ */

#define NEED_REPLIES

#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"

#define _XpPadOut(len) (((len) + 3) & ~3)

XPPrinterList
XpGetPrinterList (
    Display     *dpy,
    char        *printer_name,
    int         *list_count             /* return value */
)
{
    xPrintGetPrinterListReq     *req;
    xPrintGetPrinterListReply   rep;

    int		printer_name_len, locale_len;
    char        *locale;

    /* For decoding the variable portion of Reply */
    long	dataLenVR;
    CARD8	*dataVR;	/* aka STRING8 */

    XPPrinterList ptr_list;

    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    /* For converting root winID to corresponding ScreenPtr */
    int    i;


    if (XpCheckExtInit(dpy, XP_DONT_CHECK) == -1)
        return ( (XPPrinterList) NULL ); /* No such extension */

    /*
     * Fetch locale information. Note: XpGetLocaleNetString has
     * a thread-safe mutex on _Xglobal_lock.
     */
    locale = XpGetLocaleNetString();

    LockDisplay (dpy);

    GetReq(PrintGetPrinterList,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintGetPrinterList;

    /*
     * Compute lengths of variable portions.
     */
    if ( printer_name == (char *) NULL )
	req->printerNameLen = 0;
    else if ( *printer_name == (char) NULL )
	req->printerNameLen = 0;
    else {
	printer_name_len    = strlen( printer_name );
	req->length        += _XpPadOut(printer_name_len) >> 2;
	req->printerNameLen = (unsigned long) printer_name_len;
    }

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
     * Attach variable data.
     */
    if (req->printerNameLen)
	Data( dpy, (char *) printer_name, (long) req->printerNameLen );

    if (req->localeLen)
	Data( dpy, (char *) locale, (long) req->localeLen );

    if (! _XReply (dpy, (xReply *) &rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return ( (XPPrinterList) NULL ); /* error */
    }

    XFree(locale);

    *list_count = rep.listCount;

    if (*list_count) {
	ptr_list = (XPPrinterList)
		Xmalloc( (unsigned) (sizeof(XPPrinterRec) * (*list_count + 1)));

	if (!ptr_list) {
            UnlockDisplay(dpy);
            SyncHandle();
            return ( (XPPrinterList) NULL ); /* malloc error */
	}

	/*
	 * NULL last entry so XpFreePrinterList can work without a list_count
	 */
	ptr_list[*list_count].name = (char *) NULL;
	ptr_list[*list_count].desc = (char *) NULL;

	for ( i = 0; i < *list_count; i++ ) {
	    /*
	     * Pull printer length and then name.
	     */
	    _XRead32 (dpy, &dataLenVR, (long) sizeof(CARD32) );

	    if (dataLenVR) {
		dataVR = (CARD8 *) Xmalloc( (unsigned) dataLenVR + 1 );

		if (!dataVR) {
		    UnlockDisplay(dpy);
		    SyncHandle();
		    return ( (XPPrinterList) NULL ); /* malloc error */
		}

		_XReadPad (dpy, (char *) dataVR, (long) dataLenVR);
		dataVR[dataLenVR] = 0;
		ptr_list[i].name = (char *) dataVR;
	    }
	    else {
		ptr_list[i].name = (char *) NULL;
	    }

	    /*
	     * Pull localized description length and then description.
	     */
	    _XRead32 (dpy, &dataLenVR, (long) sizeof(CARD32) );

	    if (dataLenVR) {
		dataVR = (CARD8 *) Xmalloc( (unsigned) dataLenVR + 1 );

		if (!dataVR) {
		    UnlockDisplay(dpy);
		    SyncHandle();
		    return ( (XPPrinterList) NULL ); /* malloc error */
		}

		_XReadPad (dpy, (char *) dataVR, (long) dataLenVR);
		dataVR[dataLenVR] = 0;
		ptr_list[i].desc = (char *) dataVR;
	    }
	    else {
		ptr_list[i].desc = (char *) NULL;
	    }
	}
    }
    else {
	ptr_list = (XPPrinterList) NULL;
    }

    UnlockDisplay(dpy);
    SyncHandle();

    return ( ptr_list );
}


void
XpFreePrinterList (
    XPPrinterList  printer_list
)
{
    int i;


    if ( printer_list == NULL )
	return;

    i = 0;
    while (printer_list[i].name != NULL) {
	Xfree( (char *) printer_list[i].name );
	Xfree( (char *) printer_list[i].desc );
	i++;
    }

    Xfree( (char *) printer_list );
}


void
XpRehashPrinterList (
    Display    *dpy
)
{
    xPrintRehashPrinterListReq *req;
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);


    if (XpCheckExtInit(dpy, XP_INITIAL_RELEASE) == -1)
        return; /* NoSuchExtension */

    LockDisplay (dpy);

    GetReq(PrintRehashPrinterList,req);
    req->reqType = info->codes->major_opcode;
    req->printReqType = X_PrintRehashPrinterList;

    UnlockDisplay (dpy);
    SyncHandle ();
}

