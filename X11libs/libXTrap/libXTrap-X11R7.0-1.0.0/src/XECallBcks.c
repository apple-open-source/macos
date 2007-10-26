/* $XFree86$ */
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991 by Digital Equipment Corp., Maynard, MA

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*****************************************************************************/
/*
 *
 *  CONTRIBUTORS:
 *
 *      Dick Annicchiarico
 *      Robert Chesler
 *      Dan Coutu
 *      Gene Durso
 *      Marc Evans
 *      Alan Jamison
 *      Mark Henry
 *      Ken Miller
 *
 */
/*
 * This file contains XETrap Callback initilization routines.
 * The callback data hang off of the TC and are freed as part of the
 * XEFreeTC routine.
 */

#include <X11/Xos.h>
#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>

int XEAddRequestCB(XETC *tc, CARD8 req, void_function func, BYTE *data)
{
    if (!tc->values.req_cb)
    {   /* This is the first time for this particular TC, need to malloc */
        if ((tc->values.req_cb =
            (XETrapCB *)XtCalloc(256L,sizeof(XETrapCB))) == NULL)
        {
            /* XtCalloc already reported the error */
            return(False);
        }
    }
    tc->values.req_cb[req].func = func;
    tc->values.req_cb[req].data = data;

    return(True);
}

int XEAddRequestCBs(XETC *tc, ReqFlags req_flags, void_function func,
    BYTE *data)
{
    int i;
    int status = True;

    for (i=0; i<=255L; i++)
    {
        if (BitIsTrue(req_flags, i))
        {
            status = XEAddRequestCB(tc, (CARD8)i, func, data);
        }
    }
    return(status);
}

int XEAddEventCB(XETC *tc, CARD8 evt, void_function func, BYTE *data)
{
    if (!tc->values.evt_cb)
    {   /* This is the first time for this particular TC, need to malloc */
        if ((tc->values.evt_cb = 
            (XETrapCB *)XtCalloc(XETrapCoreEvents,sizeof(XETrapCB))) == NULL)
        {
            /* XtCalloc already reported the error */
            return(False);
        }
    }
    tc->values.evt_cb[evt].func = func;
    tc->values.evt_cb[evt].data = data;

    return(True);
}

int XEAddEventCBs(XETC *tc, EventFlags evt_flags, void_function func,
    BYTE *data)
{
    int i;
    int status = True;

    for (i=KeyPress; i<=MotionNotify; i++)
    {
        if (BitIsTrue(evt_flags, i))
        {
            status = XEAddEventCB(tc, (CARD8)i, func, data);
        }
    }
    return(status);
}

void XERemoveRequestCB(XETC *tc, CARD8 req)
{
    if (!tc->values.req_cb)
    {   /* We gotta problem!  CB struct not allocated! */
        return;
    }
    tc->values.req_cb[req].func = (void_function)NULL;
    tc->values.req_cb[req].data = (BYTE *)NULL;
    return;
}
void XERemoveRequestCBs(XETC *tc, ReqFlags req_flags)
{
    int i;

    for (i=0; i<=255L; i++)
    {
        if (BitIsTrue(req_flags, i))
        {
            XERemoveRequestCB(tc, (CARD8)i);
        }
    }
}

void XERemoveAllRequestCBs(XETC *tc)
{
    if (!tc->values.req_cb)
    {   /* We gotta problem!  CB struct not allocated! */
        return;
    }
    XtFree((XtPointer)tc->values.req_cb);
}

void XERemoveEventCB(XETC *tc, CARD8 evt)
{
    if (!tc->values.evt_cb)
    {   /* We gotta problem!  CB struct not allocated! */
        return;
    }
    tc->values.evt_cb[evt].func = (void_function)NULL;
    tc->values.evt_cb[evt].data = (BYTE *)NULL;
    return;
}

void XERemoveEventCBs(XETC *tc, EventFlags evt_flags)
{
    int i;

    for (i=KeyPress; i<=MotionNotify; i++)
    {
        if (BitIsTrue(evt_flags, i))
        {
            XERemoveEventCB(tc, (CARD8)i);
        }
    }
}

void XERemoveAllEventCBs(XETC *tc)
{
    if (!tc->values.evt_cb)
    {   /* We gotta problem!  CB struct not allocated! */
        return;
    }
    XtFree((XtPointer)tc->values.evt_cb);
}
