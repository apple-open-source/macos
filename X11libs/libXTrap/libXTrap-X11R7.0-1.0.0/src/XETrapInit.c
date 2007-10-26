/* $XFree86: xc/lib/XTrap/XETrapInit.c,v 1.2 2001/11/08 04:00:12 tsi Exp $ */
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991, 1992 by Digital Equipment Corp., 
Maynard, MA

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
#define NEED_EVENTS
#define NEED_REPLIES

#include <stdio.h>
#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>

#ifndef XETrapNumberErrors
#define XETrapNumberErrors 0
#endif

#ifdef UWS40
#define _XSetLastRequestRead _SetLastRequestRead
#endif
#ifndef vms
extern unsigned long _XSetLastRequestRead(Display *dpy, xGenericReply *rep);
#else
static unsigned long _XSetLastRequestRead(Display *dpy, xGenericReply *rep);
#endif

static XExtensionInfo *xtrap_info = NULL;
static /* const */ char *xtrap_extension_name = XTrapExtName;

#define XTrapCheckExtension(dpy,i,val) \
    XextCheckExtension(dpy, i, xtrap_extension_name, val)
#define XTrapSimpleCheckExtension(dpy,i) \
    XextSimpleCheckExtension(dpy, i, xtrap_extension_name)

static XEXT_CLOSE_DISPLAY_PROTO(close_display);
static Bool wire_to_event(Display *dpy, XEvent *event, xEvent *wire_ev);
static Status event_to_wire(Display *dpy, XEvent *event, xEvent *wire_ev);

#ifdef X11R3
static int error_string();
#else
static XEXT_ERROR_STRING_PROTO(error_string);
#endif
static /* const */ XExtensionHooks xtrap_extension_hooks = {
    NULL,                               /* create_gc */
    NULL,                               /* copy_gc */
    NULL,                               /* flush_gc */
    NULL,                               /* free_gc */
    NULL,                               /* create_font */
    NULL,                               /* free_font */
    close_display,                      /* close_display */
    wire_to_event,                      /* wire_to_event */
    event_to_wire,                      /* event_to_wire */
    NULL,                               /* error */
    error_string                        /* error_string */
};

static /* const */ char *xtrap_error_list[] = {
    "BadTransport (I/O transport not available)",
    "BadMailbox (Cannot connect/disconnect to mailbox)",
    "BadIO (I/O error while reading/writing in extension)",
    "BadHostname (Cannot locate requested host)",
    "BadStatistics (Statistics not configured/available)",
    "BadDevices (Devices not properly vectored)",
    "BadSocket (Cannot connect to INTERNET socket)",
    "BadScreen (Cannot send event to given screen)",
    "BadSwapReq (Cannot trap extension requests for swapped client)",
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, xtrap_info,
                                   xtrap_extension_name,
                                   &xtrap_extension_hooks,
                                   XETrapNumberEvents, NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, xtrap_info)

static XEXT_GENERATE_ERROR_STRING (error_string, xtrap_extension_name,
                                   XETrapNumErrors, xtrap_error_list)

static Bool event_to_wire(Display *dpy, XEvent *libevent, xEvent *netevent)
{
    Bool status = False;
    XExtDisplayInfo *info = find_display(dpy);

    XTrapCheckExtension(dpy,info,False);

    /* If we had more then one event */
#if XETrapNumberErrors > 1
    switch((netevent->u.u.type & 0x7f) - info->codes->first_event)
    {   case XETrapData:
#endif
        {
            XETrapDataEvent *ev    = (XETrapDataEvent *) libevent;
            xETrapDataEvent *event = (xETrapDataEvent *) netevent;

            event->type           = ev->type;
            event->detail         = ev->detail;
            event->sequenceNumber = (ev->serial & 0xFFFF);
            event->idx            = ev->idx;
            (void)memcpy(event->data,ev->data,sizeof(event->data));
            status = True;
        }
#if XETrapNumberErrors > 1
    }
#endif
    return(status);
}

static Bool wire_to_event(Display *dpy, XEvent *libevent, xEvent *netevent)
{
    Bool status = False;
    XExtDisplayInfo *info = find_display(dpy);

    XTrapCheckExtension(dpy,info,False);

    /* If we had more then one event */
#if XETrapNumberErrors > 1
    switch((netevent->u.u.type & 0x7f) - info->codes->first_event)
    {   case XETrapData:
#endif
        {
            XETrapDataEvent *ev    = (XETrapDataEvent *) libevent;
            xETrapDataEvent *event = (xETrapDataEvent *) netevent;

            ev->type      = event->type & 0x7F;
            ev->detail    = event->detail;
            ev->serial    = _XSetLastRequestRead(dpy,(xGenericReply *)netevent);
            ev->synthetic = ((event->type & 0x80) != 0);
            ev->display   = dpy;
            ev->idx       = event->idx;
            (void)memcpy(ev->data,event->data,sizeof(ev->data));
            status = True;
        }
#if XETrapNumberErrors > 1
    }
#endif
    return(status);
}

/*
 * XETrapQueryExtension -
 *      Returns True if the DEC-XTRAP extension is available
 *      on the given display.  If the extension exists, the value of the
 *      first event code is stored into event_base and the value of the first
 *      error code is stored into error_base.
 */
Bool XETrapQueryExtension(Display *dpy,INT32 *event_base_return,
    INT32 *error_base_return, INT32 *opcode_return)
{
    Bool status = True;
    XExtDisplayInfo *info = find_display (dpy);

    if (XextHasExtension (info))
    {
        *event_base_return = (INT32)(info->codes->first_event);
        *error_base_return = (INT32)(info->codes->first_error);
	*opcode_return     = (INT32)(info->codes->major_opcode);
    }
    else
    {
        status = False;
    }
    return(status);
}

#ifdef vms
/* Hard-coded since this didn't make it into XLibShr's xfer vector */
/* From [.XLIBEL.SRC]XLibInt.c in VMS Source Pool */
unsigned long _XSetLastRequestRead(Display *dpy, xGenericReply *rep)
{
    register unsigned long      newseq, lastseq;

    /*
     * KeymapNotify has no sequence number, but is always guaranteed
     * to immediately follow another event, except when generated via
     * SendEvent (hmmm).
     */
    if ((rep->type & 0x7f) == KeymapNotify)
        return(dpy->last_request_read);

    newseq = (dpy->last_request_read & ~((unsigned long)0xffff)) |
             rep->sequenceNumber;
    lastseq = dpy->last_request_read;
    while (newseq < lastseq) {
        newseq += 0x10000;
        if (newseq > dpy->request) {
            (void) fprintf (stderr,
            "Xlib:  sequence lost (0x%lx > 0x%lx) in reply type 0x%x!\n",
                                    newseq, dpy->request,
                                   (unsigned int) rep->type);
            newseq -= 0x10000;
           break;
        }
    }

    dpy->last_request_read = newseq;
    return(newseq);
}
#endif

