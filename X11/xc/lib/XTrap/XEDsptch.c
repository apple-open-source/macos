/* $XFree86: xc/lib/XTrap/XEDsptch.c,v 1.1 2001/11/02 23:29:27 dawes Exp $ */
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

#include <stdio.h>
#include <errno.h>
#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>

static void XETrapDispatchCB(XETC *tc, XETrapDatum *pdatum)
{
    void_function pfunc = NULL;
    BYTE *userp = NULL;

    /* Need to deal with Delta Timestamps here before calling client CB */
    if (XETrapGetTCFlagDeltaTimes(tc))
    {
        CARD32 last_time = XETrapGetTCTime(tc);
        if (XETrapHeaderIsEvent(&pdatum->hdr))
        {   /* then we can play with the timestamps */
            pdatum->hdr.timestamp = 
                pdatum->u.event.u.keyButtonPointer.time;
        }
        else
        {   /* 
             * the current one from GetTimeInMillis is worthless
             * as it's only updated during event instances (e.g. not
             * wall clock).
             */
            pdatum->hdr.timestamp = last_time;
        }
        if (!pdatum->hdr.timestamp)
        {   /* for dual monitor bug */
            pdatum->hdr.timestamp = last_time;
        }
        if (!last_time)
        {   /* first one!  Prime it! */
            last_time = pdatum->hdr.timestamp;
        }
        tc->values.last_time = pdatum->hdr.timestamp;   /* no macro! */
        if (pdatum->hdr.timestamp < last_time)
        {   /* for clock rollover */
            pdatum->hdr.timestamp = 0;
        }
        else
        {   /* the real delta */
            pdatum->hdr.timestamp = pdatum->hdr.timestamp - last_time;
        }
    }
    /*  Get the user supplied callback function */
    if (XETrapHeaderIsEvent(&pdatum->hdr))
    {
        pfunc = tc->values.evt_cb[pdatum->u.event.u.u.type].func;
        userp = tc->values.evt_cb[pdatum->u.event.u.u.type].data;
    }
    else if (XETrapHeaderIsRequest(&pdatum->hdr) ||
        XETrapHeaderIsReply(&pdatum->hdr))
    {
        pfunc = tc->values.req_cb[pdatum->u.req.reqType].func;
        userp = tc->values.req_cb[pdatum->u.req.reqType].data;
    }

    /* If there is a callback then call it with the data */
    if (pfunc != NULL)
    { 
        (*pfunc)(tc,pdatum,userp); 
    }
}

Boolean XETrapDispatchXLib(XETrapDataEvent *event, XETC *tc)
{   
    memcpy(&tc->xbuff[event->idx*sz_EventData], event->data, sz_EventData);

    if (event->detail == XETrapDataLast)
    {
        XETrapDispatchCB(tc, (XETrapDatum *)tc->xbuff);
    }
    return True;
}
