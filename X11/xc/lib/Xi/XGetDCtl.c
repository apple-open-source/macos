/* $Xorg: XGetDCtl.c,v 1.4 2001/02/09 02:03:50 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/Xi/XGetDCtl.c,v 3.4 2002/10/16 00:37:28 dawes Exp $ */

/***********************************************************************
 *
 * XGetDeviceControl - get the Device control state of an extension device.
 *
 */

#include <X11/extensions/XI.h>
#include <X11/extensions/XIproto.h>
#include <X11/Xlibint.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/extutil.h>
#include "XIint.h"

XDeviceControl
*XGetDeviceControl (dpy, dev, control)
    register	Display 	*dpy;
    XDevice			*dev;
    int				control;
    {
    int	size = 0;
    int	nbytes, i;
    XDeviceControl *Device = NULL;
    XDeviceControl *Sav = NULL;
    xDeviceState *d = NULL;
    xDeviceState *sav = NULL;
    xGetDeviceControlReq *req;
    xGetDeviceControlReply rep;
    XExtDisplayInfo *info = XInput_find_display (dpy);

    LockDisplay (dpy);
    if (_XiCheckExtInit(dpy, XInput_Add_XChangeDeviceControl) == -1)
	return ((XDeviceControl *) NoSuchExtension);

    GetReq(GetDeviceControl,req);
    req->reqType = info->codes->major_opcode;
    req->ReqType = X_GetDeviceControl;
    req->deviceid = dev->device_id;
    req->control = control;

    if (! _XReply (dpy, (xReply *) &rep, 0, xFalse)) 
	{
	UnlockDisplay(dpy);
	SyncHandle();
	return (XDeviceControl *) NULL;
	}
    if (rep.length > 0) 
	{
	nbytes = (long)rep.length << 2;
	d = (xDeviceState *) Xmalloc((unsigned) nbytes);
        if (!d)
	    {
	    _XEatData (dpy, (unsigned long) nbytes);
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return (XDeviceControl *) NULL;
	    }
	sav = d;
	_XRead (dpy, (char *) d, nbytes);

	switch (d->control)
	    {
	    case DEVICE_RESOLUTION:
		{
		xDeviceResolutionState *r;

		r = (xDeviceResolutionState *) d;
		size += sizeof (XDeviceResolutionState) + 
			(3 * sizeof(int) * r->num_valuators);
		break;
		}
	    default:
		size += d->length;
		break;
	    }

	Device = (XDeviceControl *) Xmalloc((unsigned) size);
        if (!Device)
	    {
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return (XDeviceControl *) NULL;
	    }
	Sav = Device;

	d = sav;
	switch (control)
	    {
	    case DEVICE_RESOLUTION:
		{
		int *iptr, *iptr2;
		xDeviceResolutionState *r;
		XDeviceResolutionState *R;
		r = (xDeviceResolutionState *) d;
		R = (XDeviceResolutionState *) Device;

		R->control = DEVICE_RESOLUTION;
		R->length = sizeof (XDeviceResolutionState);
		R->num_valuators = r->num_valuators;
		iptr = (int *) (R+1);
		iptr2 = (int *) (r+1);
		R->resolutions = iptr;
		R->min_resolutions = iptr + R->num_valuators;
		R->max_resolutions = iptr + (2 * R->num_valuators);
		for (i=0; i < (3 * R->num_valuators); i++)
		    *iptr++ = *iptr2++;
		break;
		}
	    default:
		break;
	    }
	XFree (sav);
	}

    UnlockDisplay(dpy);
    SyncHandle();
    return (Sav);
    }

void XFreeDeviceControl (control)
    XDeviceControl *control;
    {
    XFree (control);
    }
