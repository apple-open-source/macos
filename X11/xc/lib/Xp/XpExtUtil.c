/* $Xorg: XpExtUtil.c,v 1.3 2000/08/17 19:46:06 cpqbld Exp $ */
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
/* $XFree86: xc/lib/Xp/XpExtUtil.c,v 1.7 2002/10/16 00:37:31 dawes Exp $ */

#define NEED_EVENTS
#define NEED_REPLIES
#include <stdio.h>
#include <X11/extensions/Printstr.h>
#include <X11/Xlibint.h>
#include "XpExtUtil.h"
#include <X11/Xos.h>

#define ENQUEUE_EVENT   True
#define DONT_ENQUEUE    False

static XExtensionInfo     xp_info_data;
static XExtensionInfo     *xp_info = &xp_info_data;
static /* const */ char   *xp_extension_name = XP_PRINTNAME;

static int    XpClose();
static char   *XpError();
static Bool   XpWireToEvent();
static Status XpEventToWire();

#define XpCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, xp_extension_name, val)

static /* const */ XExtensionHooks xpprint_extension_hooks = {
    NULL,			/* create_gc */
    NULL,			/* copy_gc */
    NULL,			/* flush_gc */
    NULL,			/* free_gc */
    NULL,			/* create_font */
    NULL,			/* free_font */
    XpClose,			/* close_display */
    XpWireToEvent,		/* wire_to_event */
    XpEventToWire,		/* event_to_wire */
    NULL,			/* error */
    XpError,			/* error_string */
};

typedef struct {
    int     present;
    short   major_version;
    short   minor_version;
} XPrintLocalExtensionVersion;

typedef struct _xpPrintData {
    XEvent              data;
    XPrintLocalExtensionVersion   *vers;
} xpPrintData;

static char *XpErrorList[ /* XP_ERRORS */ ] = {
	"XPBadContext",
	"XPBadSequence",
	"XPBadResourceID"
};

XEXT_GENERATE_FIND_DISPLAY (xp_find_display, xp_info, 
	xp_extension_name, &xpprint_extension_hooks, XP_EVENTS, NULL)

static XEXT_GENERATE_ERROR_STRING (XpError, xp_extension_name,
				   XP_ERRORS, XpErrorList)

/*******************************************************************
 *
 * XP Print extension versions.
 */

static XPrintLocalExtensionVersion xpprintversions[] = {{XP_ABSENT,0,0},
	{XP_PRESENT, XP_PROTO_MAJOR, XP_PROTO_MINOR}};


/***********************************************************************
 *
 * Check to see if the Xp Print extension is installed in the server.
 * Also check to see if the version is >= the requested version.
 *
 * xpprintversions[] shows all revisions of this library, past to present.
 * xpprintversions[version_index] shows which version *this* library is.
 */

int XpCheckExtInit(dpy, version_index)
    register	Display *dpy;
    register	int	version_index;
{
    XExtDisplayInfo 	*info = xp_find_display (dpy);

    XpCheckExtension (dpy, info, -1);


    /*
     * Check for broken condition - a partial info world.
     */
    if (info->data) {
	if (!(((xpPrintData *) info->data)->vers)) {
	    return (-1);
	}
    }

    _XLockMutex(_Xglobal_lock);
    if (info->data == NULL) {
	/*
	 * Hang a Xp private data struct.   Use it for version
	 * information.
	 */
	info->data = (caddr_t) Xmalloc (sizeof (xpPrintData));
	if (!info->data) {
	    _XUnlockMutex(_Xglobal_lock);
	    return (-1);
	}

	((xpPrintData *) info->data)->vers =
	    (XPrintLocalExtensionVersion *) Xmalloc(sizeof(XPrintLocalExtensionVersion));
	if (!(((xpPrintData *) info->data)->vers)) {
	    _XUnlockMutex(_Xglobal_lock);
	    return (-1);
	}

	/*
	 * Set present to false so that XpQueryVersion will fill
	 * it in vs using its own cache which now contains garbage.
	 */
	((xpPrintData *) info->data)->vers->present = False;

	((xpPrintData *) info->data)->vers->present = XpQueryVersion(dpy,
	    &(((xpPrintData *) info->data)->vers->major_version),
	    &(((xpPrintData *) info->data)->vers->minor_version) );
    }
    if (xpprintversions[version_index].major_version > XP_DONT_CHECK) {
	if ( ( ((xpPrintData *) info->data)->vers->major_version
		< xpprintversions[version_index].major_version) ||
	     ( (((xpPrintData *) info->data)->vers->major_version
		== xpprintversions[version_index].major_version) &&
	     (((xpPrintData *) info->data)->vers->minor_version
		< xpprintversions[version_index].minor_version))) {
	    _XUnlockMutex(_Xglobal_lock);
	    return (-1);
	}
    }
    _XUnlockMutex(_Xglobal_lock);
    return (0);
}

/***********************************************************************
 *
 * Close display routine.
 *
 */

static int
XpClose (dpy, codes)
    Display *dpy;
    XExtCodes *codes;
    {
    XExtDisplayInfo 	*info = xp_find_display (dpy);

    _XLockMutex(_Xglobal_lock);
    if ( ((xpPrintData *) info->data)) {
	if (((xpPrintData *) info->data)->vers) {
	    XFree((char *)((xpPrintData *) info->data)->vers);
	}
	XFree((char *)info->data);
	info->data = (char *) NULL;	/* NULL since tests are run on */
    }
    _XUnlockMutex(_Xglobal_lock);

    return XextRemoveDisplay (xp_info, dpy);
    }


/******************************************************************************
 *
 * Handle extension events.
 */

/********************************************************************
 *
 * Reformat a wire event into an XEvent structure of the right type.
 */
static Bool
XpWireToEvent (dpy, re, event)
    Display	*dpy;
    XEvent	*re;
    xEvent	*event;
{
    XExtDisplayInfo *info = xp_find_display (dpy);


    /*
     * type, serial, send_event, display are common to all events.
     */
    re->type = event->u.u.type & 0x7f;
    ((XAnyEvent *)re)->serial = _XSetLastRequestRead(dpy,
                                     (xGenericReply *)event);
    ((XAnyEvent *)re)->send_event = ((event->u.u.type & 0x80) != 0);
    ((XAnyEvent *)re)->display = dpy;


    switch (re->type - info->codes->first_event)
	{

	case XPPrintNotify:
	    {
	    register XPPrintEvent  *ev = (XPPrintEvent *)     re;
	    xPrintPrintEvent      *ev2 = (xPrintPrintEvent *) event;

	    /* type, serial, send_event, *display */

	    ev->context		= (XPContext) ev2->printContext;
	    ev->cancel		= (Bool) ev2->cancel;
	    ev->detail		= (int) ev2->detail;

	    return (ENQUEUE_EVENT);
	    break;
	    }

	case XPAttributeNotify:
	    {
	    register XPAttributeEvent  *ev = (XPAttributeEvent *)     re;
	    xPrintAttributeEvent       *ev2 = (xPrintAttributeEvent *) event;

	    /* type, serial, send_event, *display */

	    ev->context		= (XPContext) ev2->printContext;
	    ev->detail		= (int) ev2->detail;

	    return (ENQUEUE_EVENT);
	    break;
	    }

	default:
	    printf ("XpWireToEvent: UNKNOWN WIRE EVENT! type=%d\n",re->type);
	    break;
	}

    return (DONT_ENQUEUE);
}

/********************************************************************
 *
 * Reformat an XEvent into a wire event.
 */
static Status
XpEventToWire(dpy, re, event, count)
    register Display *dpy;      /* pointer to display structure */
    register XEvent *re;        /* pointer to client event */
    register xEvent **event;    /* wire protocol event */
    register int *count;
{
    XExtDisplayInfo *info = (XExtDisplayInfo *) xp_find_display (dpy);

    switch ((re->type & 0x7f) - info->codes->first_event)
        {
#ifdef PRINT_SomeEventExample2
        case PRINT_SomeEventExample:
            {
            register XDeviceKeyEvent *ev = (XDeviceKeyEvent*) re;
            register deviceKeyButtonPointer *kev;
            register deviceValuator *vev;
	    int i;

            *count = 2;
            kev = (deviceKeyButtonPointer *) Xmalloc (*count * sizeof (xEvent));
            if (!kev)
                return(_XUnknownNativeEvent(dpy, re, event));
            *event = (xEvent *) kev;

            kev->type           = ev->type;
            kev->root           = ev->root;
            kev->event          = ev->window;
            kev->child          = ev->subwindow;
            kev->time           = ev->time;
            kev->event_x        = ev->x ;
            kev->event_y        = ev->y ;
            kev->root_x         = ev->x_root;
            kev->root_y         = ev->y_root;
            kev->state          = ev->state;
            kev->same_screen    = ev->same_screen;
            kev->detail         = ev->keycode;
            kev->deviceid       = ev->deviceid | MORE_EVENTS;

            vev = (deviceValuator *) ++kev;
            vev->type = info->codes->first_event + XI_DeviceValuator;
            vev->deviceid = ev->deviceid;
            vev->device_state = ev->device_state;
            vev->first_valuator = ev->first_axis;
            vev->num_valuators = ev->axes_count;
            i = vev->num_valuators;
            if (i > 6) i = 6;
            switch (i)
                {
                case 6: vev->valuator5 = ev->axis_data[5];
                case 5: vev->valuator4 = ev->axis_data[4];
                case 4: vev->valuator3 = ev->axis_data[3];
                case 3: vev->valuator2 = ev->axis_data[2];
                case 2: vev->valuator1 = ev->axis_data[1];
                case 1: vev->valuator0 = ev->axis_data[0];
                }
            break;
            }
#endif /* PRINT_SomeEventExample2 */

        default:
            return(_XUnknownNativeEvent(dpy, re, *event));
        }
}
