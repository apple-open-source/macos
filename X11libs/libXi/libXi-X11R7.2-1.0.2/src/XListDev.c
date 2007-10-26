/* $Xorg: XListDev.c,v 1.5 2001/02/09 02:03:51 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/Xi/XListDev.c,v 3.4 2001/12/14 19:55:19 dawes Exp $ */

/***********************************************************************
 *
 * XListInputDevices - Request the server to return a list of 
 *			 available input devices.
 *
 */

#define NEED_REPLIES
#define NEED_EVENTS
#include <X11/extensions/XI.h>
#include <X11/extensions/XIproto.h>
#include <X11/Xlibint.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"

XDeviceInfo *
XListInputDevices(dpy, ndevices)
    register Display *dpy;
    int *ndevices;
{
    int size;
    xListInputDevicesReq *req;
    xListInputDevicesReply rep;
    xDeviceInfo *list, *slist = NULL;
    XDeviceInfo *sclist = NULL;
    XDeviceInfo *clist = NULL;
    xAnyClassPtr any, sav_any;
    XAnyClassPtr Any;
    char *nptr, *Nptr;
    register int i, j, k;
    register long rlen;
    XExtDisplayInfo *info = XInput_find_display(dpy);

    LockDisplay(dpy);
    if (_XiCheckExtInit(dpy, XInput_Initial_Release, info) == -1)
	return ((XDeviceInfo *) NULL);

    GetReq(ListInputDevices, req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_ListInputDevices;

    if (!_XReply(dpy, (xReply *) & rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return (XDeviceInfo *) NULL;
    }

    if ((*ndevices = rep.ndevices)) {	/* at least 1 input device */
	size = *ndevices * sizeof(XDeviceInfo);
	rlen = rep.length << 2;	/* multiply length by 4    */
	list = (xDeviceInfo *) Xmalloc(rlen);
	slist = list;
	if (!slist) {
	    _XEatData(dpy, (unsigned long)rlen);
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return (XDeviceInfo *) NULL;
	}
	_XRead(dpy, (char *)list, rlen);

	any = (xAnyClassPtr) ((char *)list + (*ndevices * sizeof(xDeviceInfo)));
	sav_any = any;
	for (i = 0; i < *ndevices; i++, list++) {
	    for (j = 0; j < (int)list->num_classes; j++) {
		switch (any->class) {
		case KeyClass:
		    size += sizeof(XKeyInfo);
		    break;
		case ButtonClass:
		    size += sizeof(XButtonInfo);
		    break;
		case ValuatorClass:
		{
		    xValuatorInfoPtr v;

		    v = (xValuatorInfoPtr) any;
		    size += sizeof(XValuatorInfo) +
			(v->num_axes * sizeof(XAxisInfo));
		    break;
		}
		default:
		    break;
		}
		any = (xAnyClassPtr) ((char *)any + any->length);
	    }
	}

	for (i = 0, nptr = (char *)any; i < *ndevices; i++) {
	    size += *nptr + 1;
	    nptr += (*nptr + 1);
	}

	clist = (XDeviceInfoPtr) Xmalloc(size);
	if (!clist) {
	    XFree((char *)slist);
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return (XDeviceInfo *) NULL;
	}
	sclist = clist;
	Any = (XAnyClassPtr) ((char *)clist +
			      (*ndevices * sizeof(XDeviceInfo)));
	list = slist;
	any = sav_any;
	for (i = 0; i < *ndevices; i++, list++, clist++) {
	    clist->type = list->type;
	    clist->id = list->id;
	    clist->use = list->use;
	    clist->num_classes = list->num_classes;
	    clist->inputclassinfo = Any;
	    for (j = 0; j < (int)list->num_classes; j++) {
		switch (any->class) {
		case KeyClass:
		{
		    XKeyInfoPtr K = (XKeyInfoPtr) Any;
		    xKeyInfoPtr k = (xKeyInfoPtr) any;

		    K->class = KeyClass;
		    K->length = sizeof(XKeyInfo);
		    K->min_keycode = k->min_keycode;
		    K->max_keycode = k->max_keycode;
		    K->num_keys = k->num_keys;
		    break;
		}
		case ButtonClass:
		{
		    XButtonInfoPtr B = (XButtonInfoPtr) Any;
		    xButtonInfoPtr b = (xButtonInfoPtr) any;

		    B->class = ButtonClass;
		    B->length = sizeof(XButtonInfo);
		    B->num_buttons = b->num_buttons;
		    break;
		}
		case ValuatorClass:
		{
		    XValuatorInfoPtr V = (XValuatorInfoPtr) Any;
		    xValuatorInfoPtr v = (xValuatorInfoPtr) any;
		    XAxisInfoPtr A;
		    xAxisInfoPtr a;

		    V->class = ValuatorClass;
		    V->length = sizeof(XValuatorInfo) +
			(v->num_axes * sizeof(XAxisInfo));
		    V->num_axes = v->num_axes;
		    V->motion_buffer = v->motion_buffer_size;
		    V->mode = v->mode;
		    A = (XAxisInfoPtr) ((char *)V + sizeof(XValuatorInfo));
		    V->axes = A;
		    a = (xAxisInfoPtr) ((char *)any + sizeof(xValuatorInfo));
		    for (k = 0; k < (int)v->num_axes; k++, a++, A++) {
			A->min_value = a->min_value;
			A->max_value = a->max_value;
			A->resolution = a->resolution;
		    }
		    break;
		}
		default:
		    break;
		}
		any = (xAnyClassPtr) ((char *)any + any->length);
		Any = (XAnyClassPtr) ((char *)Any + Any->length);
	    }
	}

	clist = sclist;
	nptr = (char *)any;
	Nptr = (char *)Any;
	for (i = 0; i < *ndevices; i++, clist++) {
	    clist->name = (char *)Nptr;
	    memcpy(Nptr, nptr + 1, *nptr);
	    Nptr += (*nptr);
	    *Nptr++ = '\0';
	    nptr += (*nptr + 1);
	}
    }

    XFree((char *)slist);
    UnlockDisplay(dpy);
    SyncHandle();
    return (sclist);
}

/***********************************************************************
 *
 * Free the list of input devices.
 *
 */

void
XFreeDeviceList(list)
    XDeviceInfo *list;
{
    if (list != NULL) {
	XFree((char *)list);
    }
}
