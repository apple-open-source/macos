/* $XFree86: xc/lib/Xxf86rush/XF86Rush.c,v 1.6 2002/10/16 00:37:34 dawes Exp $ */
/*

Copyright (c) 1998 Daryll Strauss

*/

/* THIS IS NOT AN X CONSORTIUM STANDARD */

#define NEED_EVENTS
#define NEED_REPLIES
#include <X11/Xlibint.h>
#include <X11/extensions/xf86rushstr.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>

static XExtensionInfo _xf86rush_info_data;
static XExtensionInfo *xf86rush_info = &_xf86rush_info_data;
static char *xf86rush_extension_name = XF86RUSHNAME;

#define XF86RushCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, xf86rush_extension_name, val)

/*****************************************************************************
 *                                                                           *
 *			   private utility routines                          *
 *                                                                           *
 *****************************************************************************/

static int close_display();
static /* const */ XExtensionHooks xf86rush_extension_hooks = {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    close_display,			/* close_display */
    NULL,				/* wire_to_event */
    NULL,				/* event_to_wire */
    NULL,				/* error */
    NULL,				/* error_string */
};

static XEXT_GENERATE_FIND_DISPLAY (find_display, xf86rush_info, 
				   xf86rush_extension_name, 
				   &xf86rush_extension_hooks, 
				   0, NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, xf86rush_info)


/*****************************************************************************
 *                                                                           *
 *		    public XFree86-DGA Extension routines                *
 *                                                                           *
 *****************************************************************************/

Bool XF86RushQueryExtension (Display *dpy, int *event_basep, int *error_basep)
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

Bool XF86RushQueryVersion(Display *dpy, int *majorVersion, int *minorVersion)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXF86RushQueryVersionReply rep;
    xXF86RushQueryVersionReq *req;

    XF86RushCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(XF86RushQueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->rushReqType = X_XF86RushQueryVersion;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    *majorVersion = rep.majorVersion;
    *minorVersion = rep.minorVersion;
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Bool XF86RushLockPixmap(Display *dpy, int screen, Pixmap pixmap, void **addr)
{
  XExtDisplayInfo *info = find_display (dpy);
  xXF86RushLockPixmapReply rep;
  xXF86RushLockPixmapReq *req;

  XF86RushCheckExtension (dpy, info, False);
  LockDisplay(dpy);
  GetReq(XF86RushLockPixmap, req);
  req->reqType = info->codes->major_opcode;
  req->rushReqType = X_XF86RushLockPixmap;
  req->screen = screen;
  req->pixmap = pixmap;
  if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
    UnlockDisplay(dpy);
    SyncHandle();
    return False;
  }
  if (addr)
      *addr = (void *)(long)rep.addr;
  UnlockDisplay(dpy);
  SyncHandle();
  return True;
}

Bool XF86RushUnlockPixmap(Display *dpy, int screen, Pixmap pixmap)
{
  XExtDisplayInfo *info = find_display(dpy);
  xXF86RushUnlockPixmapReq *req;

  XF86RushCheckExtension (dpy, info, False);
  LockDisplay(dpy);
  GetReq(XF86RushUnlockPixmap, req);
  req->reqType = info->codes->major_opcode;
  req->rushReqType = X_XF86RushUnlockPixmap;
  req->screen=screen;
  req->pixmap=pixmap;
  UnlockDisplay(dpy);
  SyncHandle();
  return True;
}

Bool XF86RushUnlockAllPixmaps(Display *dpy)
{
  XExtDisplayInfo *info = find_display(dpy);
  xXF86RushUnlockAllPixmapsReq *req;

  XF86RushCheckExtension (dpy, info, False);
  LockDisplay(dpy);
  GetReq(XF86RushUnlockAllPixmaps, req);
  req->reqType = info->codes->major_opcode;
  req->rushReqType = X_XF86RushUnlockAllPixmaps;
  UnlockDisplay(dpy);
  SyncHandle();
  return True;
}

Bool XF86RushSetCopyMode(Display *dpy, int screen, int mode)
{
  XExtDisplayInfo *info = find_display(dpy);
  xXF86RushSetCopyModeReq *req;

  XF86RushCheckExtension (dpy, info, False);
  LockDisplay(dpy);
  GetReq(XF86RushSetCopyMode, req);
  req->reqType = info->codes->major_opcode;
  req->rushReqType = X_XF86RushSetCopyMode;
  req->screen = screen;
  req->CopyMode = mode;
  UnlockDisplay(dpy);
  SyncHandle();
  return True;
}

Bool XF86RushSetPixelStride(Display *dpy, int screen, int stride)
{
  XExtDisplayInfo *info = find_display(dpy);
  xXF86RushSetPixelStrideReq *req;

  XF86RushCheckExtension (dpy, info, False);
  LockDisplay(dpy);
  GetReq(XF86RushSetPixelStride, req);
  req->reqType = info->codes->major_opcode;
  req->rushReqType = X_XF86RushSetPixelStride;
  req->screen = screen;
  req->PixelStride = stride;
  UnlockDisplay(dpy);
  SyncHandle();
  return True;
}

int XF86RushOverlayPixmap (Display *dpy, XvPortID port, Drawable d,
			   GC gc, Pixmap pixmap, int src_x, int src_y,
			   unsigned int src_w, unsigned int src_h,
			   int dest_x, int dest_y,
			   unsigned int dest_w, unsigned int dest_h,
			   unsigned int id)
{
  XExtDisplayInfo *info = find_display(dpy);
  xXF86RushOverlayPixmapReq *req;

  XF86RushCheckExtension (dpy, info, False);

  FlushGC(dpy, gc);

  LockDisplay(dpy);
  GetReq(XF86RushOverlayPixmap, req);

  req->reqType = info->codes->major_opcode;
  req->rushReqType = X_XF86RushOverlayPixmap;
  req->port = port;
  req->drawable = d;
  req->gc = gc->gid;
  req->id = id;
  req->pixmap = pixmap;
  req->src_x = src_x;
  req->src_y = src_y;
  req->src_w = src_w;
  req->src_h = src_h;
  req->drw_x = dest_x;
  req->drw_y = dest_y;
  req->drw_w = dest_w;
  req->drw_h = dest_h;

  UnlockDisplay(dpy);
  SyncHandle();
  return Success;
}

int XF86RushStatusRegOffset (Display *dpy, int screen)
{
  XExtDisplayInfo *info = find_display(dpy);
  xXF86RushStatusRegOffsetReq *req;
  xXF86RushStatusRegOffsetReply rep;

  XF86RushCheckExtension (dpy, info, False);

  LockDisplay(dpy);
  GetReq(XF86RushStatusRegOffset, req);

  req->reqType = info->codes->major_opcode;
  req->rushReqType = X_XF86RushStatusRegOffset;
  req->screen = screen;

  if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
    UnlockDisplay(dpy);
    SyncHandle();
    return False;
  }

  UnlockDisplay(dpy);
  SyncHandle();
  return rep.offset;
}

Bool XF86RushAT3DEnableRegs (Display *dpy, int screen)
{
  XExtDisplayInfo *info = find_display(dpy);
  xXF86RushAT3DEnableRegsReq *req;

  XF86RushCheckExtension (dpy, info, False);

  LockDisplay(dpy);
  GetReq(XF86RushAT3DEnableRegs, req);

  req->reqType = info->codes->major_opcode;
  req->rushReqType = X_XF86RushAT3DEnableRegs;
  req->screen = screen;

  UnlockDisplay(dpy);
  SyncHandle();
  /*
   * The request has to be processed to stay in sync...
   */
  XSync(dpy, False);
  return Success;
}

Bool XF86RushAT3DDisableRegs (Display *dpy, int screen)
{
  XExtDisplayInfo *info = find_display(dpy);
  xXF86RushAT3DDisableRegsReq *req;

  XF86RushCheckExtension (dpy, info, False);

  LockDisplay(dpy);
  GetReq(XF86RushAT3DDisableRegs, req);

  req->reqType = info->codes->major_opcode;
  req->rushReqType = X_XF86RushAT3DDisableRegs;
  req->screen = screen;

  UnlockDisplay(dpy);
  SyncHandle();
  return Success;
}
