/* $Xorg: XExtInt.c,v 1.4 2001/02/09 02:03:50 xorgcvs Exp $ */

/************************************************************

Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1989 by Hewlett-Packard Company, Palo Alto, California.

			All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Hewlett-Packard not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

HEWLETT-PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
HEWLETT-PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/
/* $XFree86: xc/lib/Xi/XExtInt.c,v 3.6 2001/12/14 19:55:09 dawes Exp $ */

/***********************************************************************
 *
 * Input Extension library internal functions.
 *
 */

#define NEED_EVENTS
#define NEED_REPLIES
#include <stdio.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XIproto.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"

#define ENQUEUE_EVENT	True
#define DONT_ENQUEUE	False

static XExtensionInfo *xinput_info;
static /* const */ char *xinput_extension_name = INAME;
static int XInputClose();
static char *XInputError();
static Bool XInputWireToEvent();
Status _XiEventToWire();
static /* const */ XEvent emptyevent;

typedef struct _XInputData
{
    XEvent data;
    XExtensionVersion *vers;
} XInputData;

static /* const */ XExtensionHooks xinput_extension_hooks = {
    NULL,	/* create_gc */
    NULL,	/* copy_gc */
    NULL,	/* flush_gc */
    NULL,	/* free_gc */
    NULL,	/* create_font */
    NULL,	/* free_font */
    XInputClose,	/* close_display */
    XInputWireToEvent,	/* wire_to_event */
    _XiEventToWire,	/* event_to_wire */
    NULL,	/* error */
    XInputError,	/* error_string */
};

static char *XInputErrorList[] = {
    "BadDevice, invalid or uninitialized input device",	/* BadDevice */
    "BadEvent, invalid event type",	/* BadEvent */
    "BadMode, invalid mode parameter",	/* BadMode  */
    "DeviceBusy, device is busy",	/* DeviceBusy */
    "BadClass, invalid event class",	/* BadClass */
};

XEXT_GENERATE_FIND_DISPLAY(XInput_find_display, xinput_info,
			   xinput_extension_name, &xinput_extension_hooks,
			   IEVENTS, NULL)

    static XEXT_GENERATE_ERROR_STRING(XInputError, xinput_extension_name,
				      IERRORS, XInputErrorList)
/*******************************************************************
 *
 * Input extension versions.
 *
 */
    static XExtensionVersion versions[] = { {XI_Absent, 0, 0},
    {XI_Present, XI_Initial_Release_Major, XI_Initial_Release_Minor},
    {XI_Present, XI_Add_XDeviceBell_Major, XI_Add_XDeviceBell_Minor},
    {XI_Present, XI_Add_XSetDeviceValuators_Major,
     XI_Add_XSetDeviceValuators_Minor},
    {XI_Present, XI_Add_XChangeDeviceControl_Major,
     XI_Add_XChangeDeviceControl_Minor},
    {XI_Present, XI_Add_DevicePresenceNotify_Major,
     XI_Add_DevicePresenceNotify_Minor},
    {XI_Present, XI_Add_DeviceProperties_Major,
        XI_Add_DeviceProperties_Minor},
    };

/***********************************************************************
 *
 * Return errors reported by this extension.
 *
 */

void
_xibaddevice(dpy, error)
    Display *dpy;
    int *error;
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *error = info->codes->first_error + XI_BadDevice;
}

void
_xibadclass(dpy, error)
    Display *dpy;
    int *error;
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *error = info->codes->first_error + XI_BadClass;
}

void
_xibadevent(dpy, error)
    Display *dpy;
    int *error;
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *error = info->codes->first_error + XI_BadEvent;
}

void
_xibadmode(dpy, error)
    Display *dpy;
    int *error;
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *error = info->codes->first_error + XI_BadMode;
}

void
_xidevicebusy(dpy, error)
    Display *dpy;
    int *error;
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    *error = info->codes->first_error + XI_DeviceBusy;
}

static int XInputCheckExtension(Display *dpy, XExtDisplayInfo *info)
{
    XextCheckExtension (dpy, info, xinput_extension_name, 0);
    return 1;
}

/***********************************************************************
 *
 * Check to see if the input extension is installed in the server.
 * Also check to see if the version is >= the requested version.
 *
 */

int
_XiCheckExtInit(dpy, version_index, info)
    register Display *dpy;
    register int version_index;
    XExtDisplayInfo *info;
{
    XExtensionVersion *ext;

    if (!XInputCheckExtension(dpy, info)) {
	UnlockDisplay(dpy);
	return (-1);
    }

    if (info->data == NULL) {
	info->data = (XPointer) Xmalloc(sizeof(XInputData));
	if (!info->data) {
	    UnlockDisplay(dpy);
	    return (-1);
	}
	((XInputData *) info->data)->vers =
	    _XiGetExtensionVersion(dpy, "XInputExtension", info);
    }

    if (versions[version_index].major_version > Dont_Check) {
	ext = ((XInputData *) info->data)->vers;
	if ((ext->major_version < versions[version_index].major_version) ||
	    ((ext->major_version == versions[version_index].major_version) &&
	     (ext->minor_version < versions[version_index].minor_version))) {
	    UnlockDisplay(dpy);
	    return (-1);
	}
    }
    return (0);
}

/***********************************************************************
 *
 * Close display routine.
 *
 */

static int
XInputClose(dpy, codes)
    Display *dpy;
    XExtCodes *codes;
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    if (info->data != NULL) {
	XFree((char *)((XInputData *) info->data)->vers);
	XFree((char *)info->data);
    }
    return XextRemoveDisplay(xinput_info, dpy);
}

static int
Ones(mask)
    Mask mask;
{
    register Mask y;

    y = (mask >> 1) & 033333333333;
    y = mask - y - ((y >> 1) & 033333333333);
    return (((y + (y >> 3)) & 030707070707) % 077);
}

int
_XiGetDevicePresenceNotifyEvent(Display * dpy)
{
    XExtDisplayInfo *info = XInput_find_display(dpy);

    return info->codes->first_event + XI_DevicePresenceNotify;
}

/***********************************************************************
 *
 * Handle Input extension events.
 * Reformat a wire event into an XEvent structure of the right type.
 *
 */

static Bool
XInputWireToEvent(dpy, re, event)
    Display *dpy;
    XEvent *re;
    xEvent *event;
{
    unsigned int type, reltype;
    unsigned int i, j;
    XExtDisplayInfo *info = XInput_find_display(dpy);
    XEvent *save = (XEvent *) info->data;

    type = event->u.u.type & 0x7f;
    reltype = (type - info->codes->first_event);

    if (reltype != XI_DeviceValuator &&
	reltype != XI_DeviceKeystateNotify &&
	reltype != XI_DeviceButtonstateNotify) {
	*save = emptyevent;
	save->type = type;
	((XAnyEvent *) save)->serial = _XSetLastRequestRead(dpy,
							    (xGenericReply *)
							    event);
	((XAnyEvent *) save)->send_event = ((event->u.u.type & 0x80) != 0);
	((XAnyEvent *) save)->display = dpy;
    }

    switch (reltype) {
    case XI_DeviceMotionNotify:
    {
	register XDeviceMotionEvent *ev = (XDeviceMotionEvent *) save;
	deviceKeyButtonPointer *ev2 = (deviceKeyButtonPointer *) event;

	ev->root = ev2->root;
	ev->window = ev2->event;
	ev->subwindow = ev2->child;
	ev->time = ev2->time;
	ev->x_root = ev2->root_x;
	ev->y_root = ev2->root_y;
	ev->x = ev2->event_x;
	ev->y = ev2->event_y;
	ev->state = ev2->state;
	ev->same_screen = ev2->same_screen;
	ev->is_hint = ev2->detail;
	ev->deviceid = ev2->deviceid & DEVICE_BITS;
	return (DONT_ENQUEUE);
    }
	break;
    case XI_DeviceKeyPress:
    case XI_DeviceKeyRelease:
    {
	register XDeviceKeyEvent *ev = (XDeviceKeyEvent *) save;
	deviceKeyButtonPointer *ev2 = (deviceKeyButtonPointer *) event;

	ev->root = ev2->root;
	ev->window = ev2->event;
	ev->subwindow = ev2->child;
	ev->time = ev2->time;
	ev->x_root = ev2->root_x;
	ev->y_root = ev2->root_y;
	ev->x = ev2->event_x;
	ev->y = ev2->event_y;
	ev->state = ev2->state;
	ev->same_screen = ev2->same_screen;
	ev->keycode = ev2->detail;
	ev->deviceid = ev2->deviceid & DEVICE_BITS;
	if (ev2->deviceid & MORE_EVENTS)
	    return (DONT_ENQUEUE);
	else {
	    *re = *save;
	    return (ENQUEUE_EVENT);
	}
    }
	break;
    case XI_DeviceButtonPress:
    case XI_DeviceButtonRelease:
    {
	register XDeviceButtonEvent *ev = (XDeviceButtonEvent *) save;
	deviceKeyButtonPointer *ev2 = (deviceKeyButtonPointer *) event;

	ev->root = ev2->root;
	ev->window = ev2->event;
	ev->subwindow = ev2->child;
	ev->time = ev2->time;
	ev->x_root = ev2->root_x;
	ev->y_root = ev2->root_y;
	ev->x = ev2->event_x;
	ev->y = ev2->event_y;
	ev->state = ev2->state;
	ev->same_screen = ev2->same_screen;
	ev->button = ev2->detail;
	ev->deviceid = ev2->deviceid & DEVICE_BITS;
	if (ev2->deviceid & MORE_EVENTS)
	    return (DONT_ENQUEUE);
	else {
	    *re = *save;
	    return (ENQUEUE_EVENT);
	}
    }
	break;
    case XI_ProximityIn:
    case XI_ProximityOut:
    {
	register XProximityNotifyEvent *ev = (XProximityNotifyEvent *) save;
	deviceKeyButtonPointer *ev2 = (deviceKeyButtonPointer *) event;

	ev->root = ev2->root;
	ev->window = ev2->event;
	ev->subwindow = ev2->child;
	ev->time = ev2->time;
	ev->x_root = ev2->root_x;
	ev->y_root = ev2->root_y;
	ev->x = ev2->event_x;
	ev->y = ev2->event_y;
	ev->state = ev2->state;
	ev->same_screen = ev2->same_screen;
	ev->deviceid = ev2->deviceid & DEVICE_BITS;
	if (ev2->deviceid & MORE_EVENTS)
	    return (DONT_ENQUEUE);
	else {
	    *re = *save;
	    return (ENQUEUE_EVENT);
	}
    }
	break;
    case XI_DeviceValuator:
    {
	deviceValuator *xev = (deviceValuator *) event;
	int save_type = save->type - info->codes->first_event;

	if (save_type == XI_DeviceKeyPress || save_type == XI_DeviceKeyRelease) {
	    XDeviceKeyEvent *kev = (XDeviceKeyEvent *) save;

	    kev->device_state = xev->device_state;
	    kev->axes_count = xev->num_valuators;
	    kev->first_axis = xev->first_valuator;
	    i = xev->num_valuators;
	    if (i > 6)
		i = 6;
	    switch (i) {
	    case 6:
		kev->axis_data[5] = xev->valuator5;
	    case 5:
		kev->axis_data[4] = xev->valuator4;
	    case 4:
		kev->axis_data[3] = xev->valuator3;
	    case 3:
		kev->axis_data[2] = xev->valuator2;
	    case 2:
		kev->axis_data[1] = xev->valuator1;
	    case 1:
		kev->axis_data[0] = xev->valuator0;
	    }
	} else if (save_type == XI_DeviceButtonPress ||
		   save_type == XI_DeviceButtonRelease) {
	    XDeviceButtonEvent *bev = (XDeviceButtonEvent *) save;

	    bev->device_state = xev->device_state;
	    bev->axes_count = xev->num_valuators;
	    bev->first_axis = xev->first_valuator;
	    i = xev->num_valuators;
	    if (i > 6)
		i = 6;
	    switch (i) {
	    case 6:
		bev->axis_data[5] = xev->valuator5;
	    case 5:
		bev->axis_data[4] = xev->valuator4;
	    case 4:
		bev->axis_data[3] = xev->valuator3;
	    case 3:
		bev->axis_data[2] = xev->valuator2;
	    case 2:
		bev->axis_data[1] = xev->valuator1;
	    case 1:
		bev->axis_data[0] = xev->valuator0;
	    }
	} else if (save_type == XI_DeviceMotionNotify) {
	    XDeviceMotionEvent *mev = (XDeviceMotionEvent *) save;

	    mev->device_state = xev->device_state;
	    mev->axes_count = xev->num_valuators;
	    mev->first_axis = xev->first_valuator;
	    i = xev->num_valuators;
	    if (i > 6)
		i = 6;
	    switch (i) {
	    case 6:
		mev->axis_data[5] = xev->valuator5;
	    case 5:
		mev->axis_data[4] = xev->valuator4;
	    case 4:
		mev->axis_data[3] = xev->valuator3;
	    case 3:
		mev->axis_data[2] = xev->valuator2;
	    case 2:
		mev->axis_data[1] = xev->valuator1;
	    case 1:
		mev->axis_data[0] = xev->valuator0;
	    }
	} else if (save_type == XI_ProximityIn || save_type == XI_ProximityOut) {
	    XProximityNotifyEvent *pev = (XProximityNotifyEvent *) save;

	    pev->device_state = xev->device_state;
	    pev->axes_count = xev->num_valuators;
	    pev->first_axis = xev->first_valuator;
	    i = xev->num_valuators;
	    if (i > 6)
		i = 6;
	    switch (i) {
	    case 6:
		pev->axis_data[5] = xev->valuator5;
	    case 5:
		pev->axis_data[4] = xev->valuator4;
	    case 4:
		pev->axis_data[3] = xev->valuator3;
	    case 3:
		pev->axis_data[2] = xev->valuator2;
	    case 2:
		pev->axis_data[1] = xev->valuator1;
	    case 1:
		pev->axis_data[0] = xev->valuator0;
	    }
	} else if (save_type == XI_DeviceStateNotify) {
	    XDeviceStateNotifyEvent *sev = (XDeviceStateNotifyEvent *) save;
	    XInputClass *any = (XInputClass *) & sev->data[0];
	    XValuatorStatus *v;

	    for (i = 0; i < sev->num_classes; i++)
		if (any->class != ValuatorClass)
		    any = (XInputClass *) ((char *)any + any->length);
	    v = (XValuatorStatus *) any;
	    i = v->num_valuators;
	    j = xev->num_valuators;
	    if (j > 3)
		j = 3;
	    switch (j) {
	    case 3:
		v->valuators[i + 2] = xev->valuator2;
	    case 2:
		v->valuators[i + 1] = xev->valuator1;
	    case 1:
		v->valuators[i + 0] = xev->valuator0;
	    }
	    v->num_valuators += j;

	}
	*re = *save;
	return (ENQUEUE_EVENT);
    }
	break;
    case XI_DeviceFocusIn:
    case XI_DeviceFocusOut:
    {
	register XDeviceFocusChangeEvent *ev = (XDeviceFocusChangeEvent *) re;
	deviceFocus *fev = (deviceFocus *) event;

	*ev = *((XDeviceFocusChangeEvent *) save);
	ev->window = fev->window;
	ev->time = fev->time;
	ev->mode = fev->mode;
	ev->detail = fev->detail;
	ev->deviceid = fev->deviceid & DEVICE_BITS;
	return (ENQUEUE_EVENT);
    }
	break;
    case XI_DeviceStateNotify:
    {
	XDeviceStateNotifyEvent *stev = (XDeviceStateNotifyEvent *) save;
	deviceStateNotify *sev = (deviceStateNotify *) event;
	char *data;

	stev->window = None;
	stev->deviceid = sev->deviceid & DEVICE_BITS;
	stev->time = sev->time;
	stev->num_classes = Ones((Mask) sev->classes_reported & InputClassBits);
	data = (char *)&stev->data[0];
	if (sev->classes_reported & (1 << KeyClass)) {
	    register XKeyStatus *kstev = (XKeyStatus *) data;

	    kstev->class = KeyClass;
	    kstev->length = sizeof(XKeyStatus);
	    kstev->num_keys = sev->num_keys;
	    memcpy((char *)&kstev->keys[0], (char *)&sev->keys[0], 4);
	    data += sizeof(XKeyStatus);
	}
	if (sev->classes_reported & (1 << ButtonClass)) {
	    register XButtonStatus *bev = (XButtonStatus *) data;

	    bev->class = ButtonClass;
	    bev->length = sizeof(XButtonStatus);
	    bev->num_buttons = sev->num_buttons;
	    memcpy((char *)bev->buttons, (char *)sev->buttons, 4);
	    data += sizeof(XButtonStatus);
	}
	if (sev->classes_reported & (1 << ValuatorClass)) {
	    register XValuatorStatus *vev = (XValuatorStatus *) data;

	    vev->class = ValuatorClass;
	    vev->length = sizeof(XValuatorStatus);
	    vev->num_valuators = sev->num_valuators;
	    vev->mode = sev->classes_reported >> ModeBitsShift;
	    j = sev->num_valuators;
	    if (j > 3)
		j = 3;
	    switch (j) {
	    case 3:
		vev->valuators[2] = sev->valuator2;
	    case 2:
		vev->valuators[1] = sev->valuator1;
	    case 1:
		vev->valuators[0] = sev->valuator0;
	    }
	    data += sizeof(XValuatorStatus);
	}
	if (sev->deviceid & MORE_EVENTS)
	    return (DONT_ENQUEUE);
	else {
	    *re = *save;
	    stev = (XDeviceStateNotifyEvent *) re;
	    return (ENQUEUE_EVENT);
	}
    }
	break;
    case XI_DeviceKeystateNotify:
    {
	int i;
	XInputClass *anyclass;
	register XKeyStatus *kv;
	deviceKeyStateNotify *ksev = (deviceKeyStateNotify *) event;
	XDeviceStateNotifyEvent *kstev = (XDeviceStateNotifyEvent *) save;

	anyclass = (XInputClass *) & kstev->data[0];
	for (i = 0; i < kstev->num_classes; i++)
	    if (anyclass->class == KeyClass)
		break;
	    else
		anyclass = (XInputClass *) ((char *)anyclass +
					    anyclass->length);

	kv = (XKeyStatus *) anyclass;
	kv->num_keys = 256;
	memcpy((char *)&kv->keys[4], (char *)ksev->keys, 28);
	if (ksev->deviceid & MORE_EVENTS)
	    return (DONT_ENQUEUE);
	else {
	    *re = *save;
	    kstev = (XDeviceStateNotifyEvent *) re;
	    return (ENQUEUE_EVENT);
	}
    }
	break;
    case XI_DeviceButtonstateNotify:
    {
	int i;
	XInputClass *anyclass;
	register XButtonStatus *bv;
	deviceButtonStateNotify *bsev = (deviceButtonStateNotify *) event;
	XDeviceStateNotifyEvent *bstev = (XDeviceStateNotifyEvent *) save;

	anyclass = (XInputClass *) & bstev->data[0];
	for (i = 0; i < bstev->num_classes; i++)
	    if (anyclass->class == ButtonClass)
		break;
	    else
		anyclass = (XInputClass *) ((char *)anyclass +
					    anyclass->length);

	bv = (XButtonStatus *) anyclass;
	bv->num_buttons = 256;
	memcpy((char *)&bv->buttons[4], (char *)bsev->buttons, 28);
	if (bsev->deviceid & MORE_EVENTS)
	    return (DONT_ENQUEUE);
	else {
	    *re = *save;
	    bstev = (XDeviceStateNotifyEvent *) re;
	    return (ENQUEUE_EVENT);
	}
    }
	break;
    case XI_DeviceMappingNotify:
    {
	register XDeviceMappingEvent *ev = (XDeviceMappingEvent *) re;
	deviceMappingNotify *ev2 = (deviceMappingNotify *) event;

	*ev = *((XDeviceMappingEvent *) save);
	ev->window = 0;
	ev->first_keycode = ev2->firstKeyCode;
	ev->request = ev2->request;
	ev->count = ev2->count;
	ev->time = ev2->time;
	ev->deviceid = ev2->deviceid & DEVICE_BITS;
	return (ENQUEUE_EVENT);
    }
	break;
    case XI_ChangeDeviceNotify:
    {
	register XChangeDeviceNotifyEvent *ev = (XChangeDeviceNotifyEvent *) re;
	changeDeviceNotify *ev2 = (changeDeviceNotify *) event;

	*ev = *((XChangeDeviceNotifyEvent *) save);
	ev->window = 0;
	ev->request = ev2->request;
	ev->time = ev2->time;
	ev->deviceid = ev2->deviceid & DEVICE_BITS;
	return (ENQUEUE_EVENT);
    }
	break;

    case XI_DevicePresenceNotify:
    {
	XDevicePresenceNotifyEvent *ev = (XDevicePresenceNotifyEvent *) re;
	devicePresenceNotify *ev2 = (devicePresenceNotify *) event;

	*ev = *(XDevicePresenceNotifyEvent *) save;
	ev->window = 0;
	ev->time = ev2->time;
        ev->devchange = ev2->devchange;
        ev->deviceid = ev2->deviceid;
        ev->control = ev2->control;
	return (ENQUEUE_EVENT);
    }
	break;

    case XI_DevicePropertyNotify:

        {
            XDevicePropertyNotifyEvent* ev = (XDevicePropertyNotifyEvent*)re;
            devicePropertyNotify *ev2 = (devicePropertyNotify*)event;

            *ev = *(XDevicePropertyNotifyEvent*)save;
            ev->time = ev2->time;
            ev->deviceid = ev2->deviceid;
            ev->atom = ev2->atom;
            ev->state = ev2->state;
            return ENQUEUE_EVENT;
        }
        break;

    default:
	printf("XInputWireToEvent: UNKNOWN WIRE EVENT! type=%d\n", type);
	break;
    }

    return (DONT_ENQUEUE);
}
