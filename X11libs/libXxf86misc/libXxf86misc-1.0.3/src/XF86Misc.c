/* $XFree86: xc/lib/Xxf86misc/XF86Misc.c,v 3.12 2002/11/20 04:04:57 dawes Exp $ */

/*
 * Copyright (c) 1995, 1996  The XFree86 Project, Inc
 */

/* THIS IS NOT AN X CONSORTIUM STANDARD */

#define NEED_EVENTS
#define NEED_REPLIES
#include <X11/Xlibint.h>
#include <X11/extensions/xf86mscstr.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>

static XExtensionInfo _xf86misc_info_data;
static XExtensionInfo *xf86misc_info = &_xf86misc_info_data;
static char *xf86misc_extension_name = XF86MISCNAME;

#define XF86MiscCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, xf86misc_extension_name, val)

/*****************************************************************************
 *                                                                           *
 *			   private utility routines                          *
 *                                                                           *
 *****************************************************************************/

static int close_display(Display *dpy, XExtCodes *codes);

static /* const */ XExtensionHooks xf86misc_extension_hooks = {
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

static XEXT_GENERATE_FIND_DISPLAY (find_display, xf86misc_info, 
				   xf86misc_extension_name, 
				   &xf86misc_extension_hooks, 
				   0, NULL)

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, xf86misc_info)


/*****************************************************************************
 *                                                                           *
 *		    public XFree86-Misc Extension routines                *
 *                                                                           *
 *****************************************************************************/

Bool XF86MiscQueryExtension (Display *dpy, int *event_basep, int *error_basep)
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

Bool XF86MiscQueryVersion(Display* dpy, int* majorVersion, int* minorVersion)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXF86MiscQueryVersionReply rep;
    xXF86MiscQueryVersionReq *req;

    XF86MiscCheckExtension (dpy, info, False);
    LockDisplay(dpy);
    GetReq(XF86MiscQueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->xf86miscReqType = X_XF86MiscQueryVersion;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }
    *majorVersion = rep.majorVersion;
    *minorVersion = rep.minorVersion;
    UnlockDisplay(dpy);
    SyncHandle();
    if (*majorVersion > 0 || *minorVersion > 5)
	XF86MiscSetClientVersion(dpy);
    
    return True;
}

Bool
XF86MiscSetClientVersion(Display *dpy)
{
    XExtDisplayInfo *info = find_display(dpy);
    xXF86MiscSetClientVersionReq *req;

    XF86MiscCheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(XF86MiscSetClientVersion, req);
    req->reqType = info->codes->major_opcode;
    req->xf86miscReqType = X_XF86MiscSetClientVersion;
    req->major = XF86MISC_MAJOR_VERSION;
    req->minor = XF86MISC_MINOR_VERSION;
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Bool XF86MiscGetMouseSettings(Display* dpy, XF86MiscMouseSettings *mouseinfo)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXF86MiscGetMouseSettingsReply rep;
    xXF86MiscGetMouseSettingsReq *req;

    XF86MiscCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(XF86MiscGetMouseSettings, req);
    req->reqType = info->codes->major_opcode;
    req->xf86miscReqType = X_XF86MiscGetMouseSettings;
    if (!_XReply(dpy, (xReply *)&rep,
		(SIZEOF(xXF86MiscGetMouseSettingsReply) - SIZEOF(xReply))>>2,
		xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }

    mouseinfo->type = rep.mousetype;
    mouseinfo->baudrate = rep.baudrate;
    mouseinfo->samplerate = rep.samplerate;
    mouseinfo->resolution = rep.resolution;
    mouseinfo->buttons = rep.buttons;
    mouseinfo->emulate3buttons = rep.emulate3buttons;
    mouseinfo->emulate3timeout = rep.emulate3timeout;
    mouseinfo->chordmiddle = rep.chordmiddle;
    mouseinfo->flags = rep.flags;
    if (rep.devnamelen > 0) {
        if (!(mouseinfo->device = Xcalloc(rep.devnamelen + 1, 1))) {
            _XEatData(dpy, (rep.devnamelen+3) & ~3);
            Xfree(mouseinfo->device);
            return False;
        }
        _XReadPad(dpy, mouseinfo->device, rep.devnamelen);
    } else
	mouseinfo->device = NULL;

    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Bool XF86MiscGetKbdSettings(Display* dpy, XF86MiscKbdSettings *kbdinfo)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXF86MiscGetKbdSettingsReply rep;
    xXF86MiscGetKbdSettingsReq *req;

    XF86MiscCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(XF86MiscGetKbdSettings, req);
    req->reqType = info->codes->major_opcode;
    req->xf86miscReqType = X_XF86MiscGetKbdSettings;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }

    kbdinfo->type = rep.kbdtype;
    kbdinfo->rate = rep.rate;
    kbdinfo->delay = rep.delay;
    kbdinfo->servnumlock = rep.servnumlock;

    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Bool XF86MiscSetMouseSettings(Display* dpy, XF86MiscMouseSettings *mouseinfo)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXF86MiscSetMouseSettingsReq *req;
    int majorVersion, minorVersion;
    
    XF86MiscCheckExtension (dpy, info, False);
    XF86MiscQueryVersion(dpy, &majorVersion, &minorVersion);
    
    LockDisplay(dpy);
    GetReq(XF86MiscSetMouseSettings, req);
    
    req->reqType = info->codes->major_opcode;
    req->xf86miscReqType = X_XF86MiscSetMouseSettings;
    req->mousetype = mouseinfo->type;
    req->baudrate = mouseinfo->baudrate;
    req->samplerate = mouseinfo->samplerate;
    req->resolution = mouseinfo->resolution;
    req->buttons = mouseinfo->buttons;
    req->emulate3buttons = mouseinfo->emulate3buttons;
    req->emulate3timeout = mouseinfo->emulate3timeout;
    req->chordmiddle = mouseinfo->chordmiddle;
    req->flags = mouseinfo->flags;
    if (majorVersion > 0 || minorVersion > 5) {
	int len;
	if ((len = strlen(mouseinfo->device))) {
	req->devnamelen =  len + 1;
	len = (req->devnamelen + 3) >> 2;
	SetReqLen(req,len,len);
	Data(dpy, mouseinfo->device, req->devnamelen);
	}
    }
	
    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Bool XF86MiscSetKbdSettings(Display* dpy, XF86MiscKbdSettings *kbdinfo)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXF86MiscSetKbdSettingsReq *req;

    XF86MiscCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(XF86MiscSetKbdSettings, req);
    req->reqType = info->codes->major_opcode;
    req->xf86miscReqType = X_XF86MiscSetKbdSettings;
    req->kbdtype = kbdinfo->type;
    req->rate = kbdinfo->rate;
    req->delay = kbdinfo->delay;
    req->servnumlock = kbdinfo->servnumlock;

    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

int XF86MiscSetGrabKeysState(Display* dpy, Bool enable)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXF86MiscSetGrabKeysStateReply rep;
    xXF86MiscSetGrabKeysStateReq *req;

    XF86MiscCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(XF86MiscSetGrabKeysState, req);
    req->reqType = info->codes->major_opcode;
    req->xf86miscReqType = X_XF86MiscSetGrabKeysState;
    req->enable = enable;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return 0;
    }

    UnlockDisplay(dpy);
    SyncHandle();
    return rep.status;
}

Bool XF86MiscGetFilePaths(Display* dpy, XF86MiscFilePaths *filpaths)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXF86MiscGetFilePathsReply rep;
    xXF86MiscGetFilePathsReq *req;

    XF86MiscCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(XF86MiscGetFilePaths, req);
    req->reqType = info->codes->major_opcode;
    req->xf86miscReqType = X_XF86MiscGetFilePaths;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return False;
    }

    if (rep.configlen) {
        if (!(filpaths->configfile = Xcalloc(rep.configlen + 1, 1))) {
            _XEatData(dpy, ((rep.configlen+3) & ~3) + ((rep.modulelen+3) & ~3)
			    + ((rep.loglen+3) & ~3));
            return False;
        }
    }

    if (rep.modulelen) {
        if (!(filpaths->modulepath = Xcalloc(rep.modulelen + 1, 1))) {
            _XEatData(dpy, ((rep.configlen+3) & ~3) + ((rep.modulelen+3) & ~3)
			    + ((rep.loglen+3) & ~3));
            if (filpaths->configfile)
		    Xfree(filpaths->configfile);
            return False;
        }
    }

    if (rep.loglen) {
        if (!(filpaths->logfile = Xcalloc(rep.loglen + 1, 1))) {
            _XEatData(dpy, ((rep.configlen+3) & ~3) + ((rep.modulelen+3) & ~3)
			    + ((rep.loglen+3) & ~3));
            if (filpaths->configfile)
		    Xfree(filpaths->configfile);
            if (filpaths->modulepath)
		    Xfree(filpaths->modulepath);
            return False;
        }
    }

    if (rep.configlen)
        _XReadPad(dpy, filpaths->configfile, rep.configlen);
    else
	filpaths->configfile = "";

    if (rep.modulelen)
        _XReadPad(dpy, filpaths->modulepath, rep.modulelen);
    else
	filpaths->modulepath = "";

    if (rep.loglen)
        _XReadPad(dpy, filpaths->logfile, rep.loglen);
    else
	filpaths->logfile = "";

    UnlockDisplay(dpy);
    SyncHandle();
    return True;
}

Status XF86MiscPassMessage(Display* dpy, int screen,
			   const char* msgtype, const char* msgval,
			   char** retmsg)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXF86MiscPassMessageReply rep;
    xXF86MiscPassMessageReq *req;
    int len;

    XF86MiscCheckExtension (dpy, info, False);

    LockDisplay(dpy);
    GetReq(XF86MiscPassMessage, req);
    req->reqType = info->codes->major_opcode;
    req->xf86miscReqType = X_XF86MiscPassMessage;
    req->screen = screen;
    if ((len = strlen(msgtype))) {
	req->typelen =  len + 1;
	len = (req->typelen + 3) >> 2;
	SetReqLen(req,len,len);
	Data(dpy, msgtype, req->typelen);
    }
    if ((len = strlen(msgval))) {
	req->vallen =  len + 1;
	len = (req->vallen + 3) >> 2;
	SetReqLen(req,len,len);
	Data(dpy, msgval, req->vallen);
    }
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
	UnlockDisplay(dpy);
	SyncHandle();
	return BadImplementation;
    }

    if (rep.mesglen) {
        if (!(*retmsg = Xcalloc(rep.mesglen + 1, 1))) {
            _XEatData(dpy, ((rep.mesglen+3) & ~3));
            return BadAlloc;
        }
        _XReadPad(dpy, *retmsg, rep.mesglen);
    }

    UnlockDisplay(dpy);
    SyncHandle();
    return rep.status;
}

