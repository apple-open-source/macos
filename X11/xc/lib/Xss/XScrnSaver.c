/*
 * $XConsortium: XScrnSaver.c,v 1.5 94/04/17 20:59:35 rws Exp $
 *
Copyright (c) 1992  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
 *
 * Author:  Keith Packard, MIT X Consortium
 */
/* $XFree86: xc/lib/Xss/XScrnSaver.c,v 3.2 2002/10/16 00:37:32 dawes Exp $ */

#define NEED_EVENTS
#define NEED_REPLIES
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/saverproto.h>
#include <X11/extensions/scrnsaver.h>


static XExtensionInfo _screen_saver_info_data;
static XExtensionInfo *screen_saver_info = &_screen_saver_info_data;
static /* const */ char *screen_saver_extension_name = ScreenSaverName;

#define ScreenSaverCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, screen_saver_extension_name, val)
#define ScreenSaverSimpleCheckExtension(dpy,i) \
  XextSimpleCheckExtension (dpy, i, screen_saver_extension_name)

static int close_display();
static Bool wire_to_event();
static Status event_to_wire();
static /* const */ XExtensionHooks screen_saver_extension_hooks = {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    close_display,			/* close_display */
    wire_to_event,			/* wire_to_event */
    event_to_wire,			/* event_to_wire */
    NULL,				/* error */
    NULL,				/* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, screen_saver_info,
				   screen_saver_extension_name, 
				   &screen_saver_extension_hooks,
				   ScreenSaverNumberEvents, NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, screen_saver_info)


static Bool wire_to_event (dpy, re, event)
    Display *dpy;
    XEvent  *re;
    xEvent  *event;
{
    XExtDisplayInfo *info = find_display (dpy);
    XScreenSaverNotifyEvent	*se;
    xScreenSaverNotifyEvent	*sevent;

    ScreenSaverCheckExtension (dpy, info, False);

    switch ((event->u.u.type & 0x7f) - info->codes->first_event) {
    case ScreenSaverNotify:
    	se = (XScreenSaverNotifyEvent *) re;
	sevent = (xScreenSaverNotifyEvent *) event;
    	se->type = sevent->type & 0x7f;
    	se->serial = _XSetLastRequestRead(dpy,(xGenericReply *) event);
    	se->send_event = (sevent->type & 0x80) != 0;
    	se->display = dpy;
    	se->window = sevent->window;
    	se->window = sevent->root;
    	se->state = sevent->state;
	se->kind = sevent->kind;
	se->forced = True;
	if (sevent->forced == xFalse)
	    se->forced = False;
	se->time = sevent->timestamp;
    	return True;
    }
    return False;
}

static Status event_to_wire (dpy, re, event)
    Display *dpy;
    XEvent  *re;
    xEvent  *event;
{
    XExtDisplayInfo *info = find_display (dpy);
    XScreenSaverNotifyEvent	*se;
    xScreenSaverNotifyEvent	*sevent;

    ScreenSaverCheckExtension (dpy, info, 0);

    switch ((re->type & 0x7f) - info->codes->first_event) {
    case ScreenSaverNotify:
    	se = (XScreenSaverNotifyEvent *) re;
	sevent = (xScreenSaverNotifyEvent *) event;
    	sevent->type = se->type | (se->send_event ? 0x80 : 0);
    	sevent->sequenceNumber = se->serial & 0xffff;
    	sevent->root = se->root;
    	sevent->window = se->window;
    	sevent->state = se->state;
	sevent->kind = se->kind;
	sevent->forced = xFalse;
	if (se->forced == True)
	    sevent->forced = xTrue;
	sevent->timestamp = se->time;
    	return 1;
    }
    return 0;
}

/****************************************************************************
 *                                                                          *
 *			    ScreenSaver public interfaces                         *
 *                                                                          *
 ****************************************************************************/

Bool XScreenSaverQueryExtension (dpy, event_basep, error_basep)
    Display *dpy;
    int *event_basep, *error_basep;
{
    XExtDisplayInfo *info = find_display (dpy);

    if (XextHasExtension(info)) {
	*event_basep = info->codes->first_event;
	*error_basep = info->codes->first_error;
	return True;
    } else {
	return False;
    }
}


Status XScreenSaverQueryVersion(dpy, major_versionp, minor_versionp)
    Display *dpy;
    int	    *major_versionp, *minor_versionp;
{
    XExtDisplayInfo *info = find_display (dpy);
    xScreenSaverQueryVersionReply	    rep;
    register xScreenSaverQueryVersionReq  *req;

    ScreenSaverCheckExtension (dpy, info, 0);

    LockDisplay (dpy);
    GetReq (ScreenSaverQueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->saverReqType = X_ScreenSaverQueryVersion;
    req->clientMajor = ScreenSaverMajorVersion;
    req->clientMinor = ScreenSaverMinorVersion;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
    }
    *major_versionp = rep.majorVersion;
    *minor_versionp = rep.minorVersion;
    UnlockDisplay (dpy);
    SyncHandle ();
    return 1;
}

XScreenSaverInfo *XScreenSaverAllocInfo ()
{
    return (XScreenSaverInfo *) Xmalloc (sizeof (XScreenSaverInfo));
}

Status XScreenSaverQueryInfo (dpy, drawable, saver_info)
    Display		*dpy;
    Drawable		drawable;
    XScreenSaverInfo	*saver_info;
{
    XExtDisplayInfo			*info = find_display (dpy);
    xScreenSaverQueryInfoReply		rep;
    register xScreenSaverQueryInfoReq	*req;

    ScreenSaverCheckExtension (dpy, info, 0);

    LockDisplay (dpy);
    GetReq (ScreenSaverQueryInfo, req);
    req->reqType = info->codes->major_opcode;
    req->saverReqType = X_ScreenSaverQueryInfo;
    req->drawable = drawable;
    if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
    }
    UnlockDisplay (dpy);
    SyncHandle ();
    saver_info->window = rep.window;
    saver_info->state = rep.state;
    saver_info->kind = rep.kind;
    saver_info->til_or_since = rep.tilOrSince;
    saver_info->idle = rep.idle;
    saver_info->eventMask = rep.eventMask;
    return 1;
}

void XScreenSaverSelectInput (dpy, drawable, mask)
    register Display	*dpy;
    Drawable		drawable;
    unsigned long	mask;
{
    XExtDisplayInfo *info = find_display (dpy);
    register xScreenSaverSelectInputReq   *req;

    ScreenSaverSimpleCheckExtension (dpy, info);

    LockDisplay (dpy);
    GetReq (ScreenSaverSelectInput, req);
    req->reqType = info->codes->major_opcode;
    req->saverReqType = X_ScreenSaverSelectInput;
    req->drawable = drawable;
    req->eventMask = mask;
    UnlockDisplay (dpy);
    SyncHandle ();
}

static void
XScreenSaverProcessWindowAttributes (dpy, req, valuemask, attributes)
    register Display *dpy;
    xChangeWindowAttributesReq *req;
    register unsigned long valuemask;
    register XSetWindowAttributes *attributes;
    {
    unsigned long values[32];
    register unsigned long *value = values;
    unsigned int nvalues;

    if (valuemask & CWBackPixmap)
	*value++ = attributes->background_pixmap;
	
    if (valuemask & CWBackPixel)
    	*value++ = attributes->background_pixel;

    if (valuemask & CWBorderPixmap)
    	*value++ = attributes->border_pixmap;

    if (valuemask & CWBorderPixel)
    	*value++ = attributes->border_pixel;

    if (valuemask & CWBitGravity)
    	*value++ = attributes->bit_gravity;

    if (valuemask & CWWinGravity)
	*value++ = attributes->win_gravity;

    if (valuemask & CWBackingStore)
        *value++ = attributes->backing_store;
    
    if (valuemask & CWBackingPlanes)
	*value++ = attributes->backing_planes;

    if (valuemask & CWBackingPixel)
    	*value++ = attributes->backing_pixel;

    if (valuemask & CWOverrideRedirect)
    	*value++ = attributes->override_redirect;

    if (valuemask & CWSaveUnder)
    	*value++ = attributes->save_under;

    if (valuemask & CWEventMask)
	*value++ = attributes->event_mask;

    if (valuemask & CWDontPropagate)
	*value++ = attributes->do_not_propagate_mask;

    if (valuemask & CWColormap)
	*value++ = attributes->colormap;

    if (valuemask & CWCursor)
	*value++ = attributes->cursor;

    req->length += (nvalues = value - values);

    nvalues <<= 2;			    /* watch out for macros... */
    Data32 (dpy, (long *) values, (long)nvalues);

    }

void XScreenSaverSetAttributes (dpy, drawable, x, y, width, height,
				border_width, depth, class, visual,
				valuemask, attributes)
    Display*		    dpy;
    Drawable		    drawable;
    int			    x;
    int			    y;
    unsigned int	    width;
    unsigned int	    height;
    unsigned int	    border_width;
    int			    depth;
    unsigned int	    class;
    Visual *		    visual;
    unsigned long	    valuemask;
    XSetWindowAttributes    *attributes;
{
    XExtDisplayInfo *info = find_display (dpy);
    register xScreenSaverSetAttributesReq   *req;

    ScreenSaverSimpleCheckExtension (dpy, info);

    LockDisplay (dpy);
    GetReq (ScreenSaverSetAttributes, req);
    req->reqType = info->codes->major_opcode;
    req->saverReqType = X_ScreenSaverSetAttributes;
    req->drawable = drawable;
    req->x = x;
    req->y = y;
    req->width = width;
    req->height = height;
    req->borderWidth = border_width;
    req->c_class = class;
    req->depth = depth;
    if (visual == CopyFromParent)
	req->visualID = CopyFromParent;
    else
	req->visualID = visual->visualid;
    /* abuse an Xlib internal interface - is this legal for us? */
    if ((req->mask = valuemask)) 
        XScreenSaverProcessWindowAttributes (dpy,
			(xChangeWindowAttributesReq *)req, 
			valuemask, attributes);
    UnlockDisplay (dpy);
    SyncHandle ();
}


void XScreenSaverUnsetAttributes (dpy, drawable)
    register Display	*dpy;
    Drawable		drawable;
{
    XExtDisplayInfo *info = find_display (dpy);
    register xScreenSaverUnsetAttributesReq   *req;

    ScreenSaverSimpleCheckExtension (dpy, info);

    LockDisplay (dpy);
    GetReq (ScreenSaverUnsetAttributes, req);
    req->reqType = info->codes->major_opcode;
    req->saverReqType = X_ScreenSaverUnsetAttributes;
    req->drawable = drawable;
    UnlockDisplay (dpy);
    SyncHandle ();
}


Status XScreenSaverRegister (dpy, screen, xid, type)
    Display *dpy;
    int screen;
    XID xid;
    Atom type;
{
    Atom prop;
    unsigned long ul;

    prop = XInternAtom (dpy, ScreenSaverPropertyName, False);
    if (!prop)
	return 0;

    ul = (unsigned long) xid;
    XChangeProperty (dpy, RootWindow(dpy,screen), prop, type, 32, 
		     PropModeReplace, (unsigned char *) &ul, 1);
    return 1;
}



Status XScreenSaverUnregister (dpy, screen)
    Display *dpy;
    int screen;
{
    Atom prop;

    prop = XInternAtom (dpy, ScreenSaverPropertyName, False);
    if (!prop)
	return 0;

    XDeleteProperty (dpy, RootWindow(dpy,screen), prop);
    return 1;
}



Status XScreenSaverGetRegistered (dpy, screen, xid, type)
    Display *dpy;
    int screen;
    XID *xid;
    Atom *type;
{
    Atom actual_type = None;
    int actual_format;
    unsigned long nitems, bytesafter;
    unsigned long *ulp = (unsigned long *) 0;
    Atom prop;
    int retval = 0;

    prop = XInternAtom (dpy, ScreenSaverPropertyName, False);
    if (!prop)
	return retval;

    if (XGetWindowProperty (dpy, RootWindow(dpy,screen), prop, 0L, 1L, False,
			    AnyPropertyType, &actual_type,  &actual_format,
			    &nitems, &bytesafter, (unsigned char **) &ulp)
	!= Success)
	return retval;

    if (ulp) {
	if (actual_format == 32) {
	    *xid = (XID) ulp[0];
	    *type = actual_type;
	    retval = 1;
	}
	XFree ((char *) ulp);
    }
    return retval;
}	
