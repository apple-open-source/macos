/*
 * $XFree86: xc/lib/Xrandr/Xrandr.c,v 1.13 2003/02/07 11:21:07 eich Exp $
 *
 * Copyright © 2000 Compaq Computer Corporation, Inc.
 * Copyright © 2002 Hewlett Packard Company, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Compaq or HP not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.  HP makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * HP DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL COMPAQ
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Jim Gettys, HP Labs, HP.
 */

#include <stdio.h>
#include <X11/Xlib.h>
/* we need to be able to manipulate the Display structure on events */
#include <X11/Xlibint.h>
#include <X11/extensions/render.h>
#include <X11/extensions/Xrender.h>
#include "Xrandrint.h"

XExtensionInfo XRRExtensionInfo;
char XRRExtensionName[] = RANDR_NAME;

static Bool     XRRWireToEvent(Display *dpy, XEvent *event, xEvent *wire);
static Status   XRREventToWire(Display *dpy, XEvent *event, xEvent *wire);

static XRRScreenConfiguration *_XRRGetScreenInfo (Display *dpy, Window window);

static int
XRRCloseDisplay (Display *dpy, XExtCodes *codes);

static /* const */ XExtensionHooks rr_extension_hooks = {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    XRRCloseDisplay,			/* close_display */
    XRRWireToEvent,			/* wire_to_event */
    XRREventToWire,			/* event_to_wire */
    NULL,				/* error */
    NULL,				/* error_string */
};

static Bool XRRWireToEvent(Display *dpy, XEvent *event, xEvent *wire)
{
    XExtDisplayInfo *info = XRRFindDisplay(dpy);
    XRRScreenChangeNotifyEvent *aevent;
    xRRScreenChangeNotifyEvent *awire;

    RRCheckExtension(dpy, info, False);

    switch ((wire->u.u.type & 0x7F) - info->codes->first_event)
    {
      case RRScreenChangeNotify:
	awire = (xRRScreenChangeNotifyEvent *) wire;
	aevent = (XRRScreenChangeNotifyEvent *) event;
	aevent->type = awire->type & 0x7F;
	aevent->serial = _XSetLastRequestRead(dpy,
					      (xGenericReply *) wire);
	aevent->send_event = (awire->type & 0x80) != 0;
	aevent->display = dpy;
	aevent->window = awire->window;
	aevent->root = awire->root;
	aevent->timestamp = awire->timestamp;
	aevent->config_timestamp = awire->configTimestamp;
	aevent->size_index = awire->sizeID;
	aevent->subpixel_order = awire->subpixelOrder;
	aevent->rotation = awire->rotation;
	aevent->width = awire->widthInPixels;
	aevent->height = awire->heightInPixels;
	aevent->mwidth = awire->widthInMillimeters;
	aevent->mheight = awire->heightInMillimeters;
	return True;
    }

    return False;
}

static Status XRREventToWire(Display *dpy, XEvent *event, xEvent *wire)
{
    XExtDisplayInfo *info = XRRFindDisplay(dpy);
    XRRScreenChangeNotifyEvent *aevent;
    xRRScreenChangeNotifyEvent *awire;

    RRCheckExtension(dpy, info, False);

    switch ((event->type & 0x7F) - info->codes->first_event)
    {
      case RRScreenChangeNotify:
	awire = (xRRScreenChangeNotifyEvent *) wire;
	aevent = (XRRScreenChangeNotifyEvent *) event;
	awire->type = aevent->type | (aevent->send_event ? 0x80 : 0);
	awire->rotation = (CARD8) aevent->rotation;
	awire->sequenceNumber = aevent->serial & 0xFFFF;
	awire->timestamp = aevent->timestamp;
	awire->configTimestamp = aevent->config_timestamp;
	awire->root = aevent->root;
	awire->window = aevent->window;
	awire->sizeID = aevent->size_index;
	awire->subpixelOrder = aevent->subpixel_order;
	awire->widthInPixels = aevent->width;
	awire->heightInPixels = aevent->height;
	awire->widthInMillimeters = aevent->mwidth;
	awire->heightInMillimeters = aevent->mheight;
	return True;
    }
    return False;
}

XExtDisplayInfo *
XRRFindDisplay (Display *dpy)
{
    XExtDisplayInfo *dpyinfo;
    XRandRInfo *xrri;
    int i, numscreens;

    dpyinfo = XextFindDisplay (&XRRExtensionInfo, dpy);
    if (!dpyinfo) {
	dpyinfo = XextAddDisplay (&XRRExtensionInfo, dpy, 
				  XRRExtensionName,
				  &rr_extension_hooks,
				  RRNumberEvents, 0);
	numscreens = ScreenCount(dpy);
	xrri = Xmalloc (sizeof(XRandRInfo) + 
				 sizeof(char *) * numscreens);
	xrri->config = (XRRScreenConfiguration **)(xrri + 1);
	for(i = 0; i < numscreens; i++) 
	  xrri->config[i] = NULL;
	xrri->major_version = -1;
	dpyinfo->data = (char *) xrri;
    }
    return dpyinfo;
}

static int
XRRCloseDisplay (Display *dpy, XExtCodes *codes)
{
    int i;
    XRRScreenConfiguration **configs;
    XExtDisplayInfo *info = XRRFindDisplay (dpy);
    XRandRInfo *xrri;

    LockDisplay(dpy);
    /*
     * free cached data
     */
    if (XextHasExtension(info)) {
	xrri = (XRandRInfo *) info->data;
	if (xrri) {
	    configs = xrri->config;

	    for (i = 0; i < ScreenCount(dpy); i++) {
		if (configs[i] != NULL) XFree (configs[i]);
	    }
	    XFree (xrri);
	}
    }
    UnlockDisplay(dpy);
    return XextRemoveDisplay (&XRRExtensionInfo, dpy);
}
    

Rotation XRRConfigRotations(XRRScreenConfiguration *config, Rotation *current_rotation)
{
  *current_rotation = config->current_rotation;
  return config->rotations;
}

XRRScreenSize *XRRConfigSizes(XRRScreenConfiguration *config, int *nsizes)
{
   *nsizes = config->nsizes;
  return config->sizes;
}

short *XRRConfigRates (XRRScreenConfiguration *config, int sizeID, int *nrates)
{
    short   *r = config->rates;
    int	    nents = config->nrates;

    /* Skip over the intervening rate lists */
    while (sizeID > 0 && nents > 0)
    {
	int i = (*r + 1);
	r += i;
	nents -= i;
	sizeID--;
    }
    if (!nents)
    {
	*nrates = 0;
	return 0;
    }
    *nrates = (int) *r;
    return r + 1;
}

Time XRRConfigTimes (XRRScreenConfiguration *config, Time *config_timestamp)
{
    *config_timestamp = config->config_timestamp;
    return config->timestamp;
}


SizeID XRRConfigCurrentConfiguration (XRRScreenConfiguration *config, 
			      Rotation *rotation)
{
    *rotation = (Rotation) config->current_rotation;
    return (SizeID) config->current_size;
}

short XRRConfigCurrentRate (XRRScreenConfiguration *config)
{
    return config->current_rate;
}

/* 
 * Go get the screen configuration data and salt it away for future use; 
 * returns NULL if extension not supported
 */
static XRRScreenConfiguration *_XRRValidateCache (Display *dpy, int screen)
{
    XExtDisplayInfo *info = XRRFindDisplay (dpy);
    XRRScreenConfiguration **configs;
    XRandRInfo *xrri;

    if (XextHasExtension(info)) {
	xrri = (XRandRInfo *) info->data;
	configs = xrri->config;

	if (configs[screen] == NULL)
	    configs[screen] = _XRRGetScreenInfo (dpy, RootWindow(dpy, screen));
	return configs[screen];
    } else {
	return NULL;
    }
}

/* given a screen, return the information from the (possibly) cached data */
Rotation XRRRotations(Display *dpy, int screen, Rotation *current_rotation)
{
  XRRScreenConfiguration *config;
  Rotation cr;
  LockDisplay(dpy);
  if ((config = _XRRValidateCache(dpy, screen))) {
    *current_rotation = config->current_rotation;
    cr = config->rotations;
    UnlockDisplay(dpy);
    return cr;
  }
  else {
    UnlockDisplay(dpy);
    *current_rotation = RR_Rotate_0;
    return 0;	/* no rotations supported */
  }
}

/* given a screen, return the information from the (possibly) cached data */
XRRScreenSize *XRRSizes(Display *dpy, int screen, int *nsizes)
{
  XRRScreenConfiguration *config; 
  XRRScreenSize *sizes;

  LockDisplay(dpy);
  if ((config = _XRRValidateCache(dpy, screen))) {
    *nsizes = config->nsizes;
    sizes = config->sizes;
    UnlockDisplay(dpy);
    return sizes;
    }
  else {
    UnlockDisplay(dpy);
    *nsizes = 0;
    return NULL;
  }  
}

short *XRRRates (Display *dpy, int screen, int sizeID, int *nrates)
{
  XRRScreenConfiguration *config; 
  short *rates;

  LockDisplay(dpy);
  if ((config = _XRRValidateCache(dpy, screen))) {
    rates = XRRConfigRates (config, sizeID, nrates);
    UnlockDisplay(dpy);
    return rates;
    }
  else {
    UnlockDisplay(dpy);
    *nrates = 0;
    return NULL;
  }  
}

/* given a screen, return the information from the (possibly) cached data */
Time XRRTimes (Display *dpy, int screen, Time *config_timestamp)
{
  XRRScreenConfiguration *config; 
  Time ts;

  LockDisplay(dpy);
  if ((config = _XRRValidateCache(dpy, screen))) {
      *config_timestamp = config->config_timestamp;
      ts = config->timestamp;
      UnlockDisplay(dpy);
      return ts;
    } else {
      UnlockDisplay(dpy);
	return CurrentTime;
    }
}

int XRRRootToScreen(Display *dpy, Window root)
{
  int snum;
  for (snum = 0; snum < ScreenCount(dpy); snum++) {
    if (RootWindow(dpy, snum) == root) return snum;
  }
  return -1;
}


Bool XRRQueryExtension (Display *dpy, int *event_basep, int *error_basep)
{
  XExtDisplayInfo *info = XRRFindDisplay (dpy);

    if (XextHasExtension(info)) {
	*event_basep = info->codes->first_event;
	*error_basep = info->codes->first_error;
	return True;
    } else {
	return False;
    }
}

static Bool
_XRRHasRates (int major, int minor)
{
    return major > 1 || (major == 1 && minor >= 1);
}

Status XRRQueryVersion (Display *dpy,
			    int	    *major_versionp,
			    int	    *minor_versionp)
{
    XExtDisplayInfo *info = XRRFindDisplay (dpy);
    xRRQueryVersionReply rep;
    xRRQueryVersionReq  *req;
    XRandRInfo *xrri;

    RRCheckExtension (dpy, info, 0);

    xrri = (XRandRInfo *) info->data;

    /* 
     * only get the version information from the server if we don't have it already
     */
    if (xrri->major_version == -1) {
      LockDisplay (dpy);
      GetReq (RRQueryVersion, req);
      req->reqType = info->codes->major_opcode;
      req->randrReqType = X_RRQueryVersion;
      req->majorVersion = RANDR_MAJOR;
      req->minorVersion = RANDR_MINOR;
      if (!_XReply (dpy, (xReply *) &rep, 0, xTrue)) {
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
      }
      xrri->major_version = rep.majorVersion;
      xrri->minor_version = rep.minorVersion;
      xrri->has_rates = _XRRHasRates (xrri->major_version, xrri->minor_version);
    }
    *major_versionp = xrri->major_version;
    *minor_versionp = xrri->minor_version;
    UnlockDisplay (dpy);
    SyncHandle ();
    return 1;
}

typedef struct _randrVersionState {
    unsigned long   version_seq;
    Bool	    error;
    int		    major_version;
    int		    minor_version;
} _XRRVersionState;

static Bool
_XRRVersionHandler (Display	    *dpy,
			xReply	    *rep,
			char	    *buf,
			int	    len,
			XPointer    data)
{
    xRRQueryVersionReply	replbuf;
    xRRQueryVersionReply	*repl;
    _XRRVersionState	*state = (_XRRVersionState *) data;

    if (dpy->last_request_read != state->version_seq)
	return False;
    if (rep->generic.type == X_Error)
    {
	state->error = True;
	return False;
    }
    repl = (xRRQueryVersionReply *)
	_XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
		     (SIZEOF(xRRQueryVersionReply) - SIZEOF(xReply)) >> 2,
			True);
    state->major_version = repl->majorVersion;
    state->minor_version = repl->minorVersion;
    return True;
}
/* need a version that does not hold the display lock */
static XRRScreenConfiguration *_XRRGetScreenInfo (Display *dpy, Window window)
{
    XExtDisplayInfo *info = XRRFindDisplay(dpy);
    xRRGetScreenInfoReply   rep;
    xRRGetScreenInfoReq	    *req;
    _XAsyncHandler 	    async;
    _XRRVersionState	    async_state;
    int			    nbytes, nbytesRead, rbytes;
    int			    i;
    int			    snum;
    xScreenSizes	    size;
    struct _XRRScreenConfiguration  *scp;
    XRRScreenSize	    *ssp;
    short    		    *rates;
    xRRQueryVersionReq      *vreq;
    XRandRInfo		    *xrri;
    Bool		    getting_version = False;

    RRCheckExtension (dpy, info, 0);

    xrri = (XRandRInfo *) info->data;

    if (xrri->major_version == -1)
    {
	/* hide a version query in the request */
	GetReq (RRQueryVersion, vreq);
	vreq->reqType = info->codes->major_opcode;
	vreq->randrReqType = X_RRQueryVersion;
	vreq->majorVersion = RANDR_MAJOR;
	vreq->minorVersion = RANDR_MINOR;
    
	async_state.version_seq = dpy->request;
	async_state.error = False;
	async.next = dpy->async_handlers;
	async.handler = _XRRVersionHandler;
	async.data = (XPointer) &async_state;
	dpy->async_handlers = &async;

	getting_version = True;
    }

    GetReq (RRGetScreenInfo, req);
    req->reqType = info->codes->major_opcode;
    req->randrReqType = X_RRGetScreenInfo;
    req->window = window;

    if (!_XReply (dpy, (xReply *) &rep, 0, xFalse))
    {
	if (getting_version)
	    DeqAsyncHandler (dpy, &async);
	SyncHandle ();
	return NULL;
    }
    if (getting_version)
    {
	DeqAsyncHandler (dpy, &async);
	if (async_state.error)
	{
	  SyncHandle();
	}
	xrri->major_version = async_state.major_version;
	xrri->minor_version = async_state.minor_version;
	xrri->has_rates = _XRRHasRates (xrri->minor_version, xrri->major_version);
    }

    /*
     * Make the reply compatible with v1.1
     */
    if (!xrri->has_rates)
    {
	rep.rate = 0;
	rep.nrateEnts = 0;
    }
    
    nbytes = (long) rep.length << 2;

    nbytesRead = (long) (rep.nSizes * SIZEOF (xScreenSizes) +
			 ((rep.nrateEnts + 1)& ~1) * 2 /* SIZEOF (CARD16) */);
    
    /* 
     * first we must compute how much space to allocate for 
     * randr library's use; we'll allocate the structures in a single
     * allocation, on cleanlyness grounds.
     */

    rbytes = sizeof (XRRScreenConfiguration) +
      (rep.nSizes * sizeof (XRRScreenSize) +
       rep.nrateEnts * sizeof (int));

    scp = (struct _XRRScreenConfiguration *) Xmalloc(rbytes);
    if (scp == NULL) {
	_XEatData (dpy, (unsigned long) nbytes);
	SyncHandle ();
	return NULL;
    }


    ssp = (XRRScreenSize *)(scp + 1);
    rates = (short *) (ssp + rep.nSizes);

    /* set up the screen configuration structure */
    scp->screen = 
      ScreenOfDisplay (dpy, (snum = XRRRootToScreen(dpy, rep.root)));

    scp->sizes = ssp;
    scp->rates = rates;
    scp->rotations = rep.setOfRotations;
    scp->current_size = rep.sizeID;
    scp->current_rate = rep.rate;
    scp->current_rotation = rep.rotation;
    scp->timestamp = rep.timestamp;
    scp->config_timestamp = rep.configTimestamp;
    scp->nsizes = rep.nSizes;
    scp->nrates = rep.nrateEnts;

    /*
     * Time to unpack the data from the server.
     */

    /*
     * First the size information
     */
    for (i = 0; i < rep.nSizes; i++)  {
	_XReadPad (dpy, (char *) &size, SIZEOF (xScreenSizes));
	
        ssp[i].width = size.widthInPixels;
	ssp[i].height = size.heightInPixels;
	ssp[i].mwidth = size.widthInMillimeters;
	ssp[i].mheight = size.heightInMillimeters;
    }
    /*
     * And the rates
     */
    _XRead16Pad (dpy, rates, 2 /* SIZEOF (CARD16) */ * rep.nrateEnts);
    
    /*
     * Skip any extra data
     */
    if (nbytes > nbytesRead)
	_XEatData (dpy, (unsigned long) (nbytes - nbytesRead));
    
    return (XRRScreenConfiguration *)(scp);
}

XRRScreenConfiguration *XRRGetScreenInfo (Display *dpy, Window window)
{
  XRRScreenConfiguration *config;
  LockDisplay (dpy);
  config = _XRRGetScreenInfo(dpy, window);
  UnlockDisplay (dpy);
  SyncHandle ();
  return config;
}

    
void XRRFreeScreenConfigInfo (XRRScreenConfiguration *config)
{
    Xfree (config);
}


/* 
 * in protocol version 0.1, routine added to allow selecting for new events.
 */

void XRRSelectInput (Display *dpy, Window window, int mask)
{
    XExtDisplayInfo *info = XRRFindDisplay (dpy);
    xRRSelectInputReq  *req;

    RRSimpleCheckExtension (dpy, info);

    LockDisplay (dpy);
    GetReq (RRSelectInput, req);
    req->reqType = info->codes->major_opcode;
    req->randrReqType = X_RRSelectInput;
    req->window = window;
    req->enable = 0;
    if (mask) req->enable = mask;
    UnlockDisplay (dpy);
    SyncHandle ();
    return;
}

Status XRRSetScreenConfigAndRate (Display *dpy,
				  XRRScreenConfiguration *config,
				  Drawable draw,
				  int size_index,
				  Rotation rotation, 
				  short rate,
				  Time timestamp)
{
    XExtDisplayInfo *info = XRRFindDisplay (dpy);
    xRRSetScreenConfigReply rep;
    XRandRInfo *xrri;
    int major, minor;

    RRCheckExtension (dpy, info, 0);

    /* Make sure has_rates is set */
    if (!XRRQueryVersion (dpy, &major, &minor))
	return 0;
    
    LockDisplay (dpy);
    xrri = (XRandRInfo *) info->data;
    if (xrri->has_rates)
    {
	xRRSetScreenConfigReq  *req;
	GetReq (RRSetScreenConfig, req);
	req->reqType = info->codes->major_opcode;
	req->randrReqType = X_RRSetScreenConfig;
	req->drawable = draw;
	req->sizeID = size_index;
	req->rotation = rotation;
	req->timestamp = timestamp;
	req->configTimestamp = config->config_timestamp;
	req->rate = rate;
    }
    else
    {
	xRR1_0SetScreenConfigReq  *req;
	GetReq (RR1_0SetScreenConfig, req);
	req->reqType = info->codes->major_opcode;
	req->randrReqType = X_RRSetScreenConfig;
	req->drawable = draw;
	req->sizeID = size_index;
	req->rotation = rotation;
	req->timestamp = timestamp;
	req->configTimestamp = config->config_timestamp;
    }
    
    (void) _XReply (dpy, (xReply *) &rep, 0, xTrue);

    if (rep.status == RRSetConfigSuccess) {
      /* if we succeed, set our view of reality to what we set it to */
      config->config_timestamp = rep.newConfigTimestamp;
      config->timestamp = rep.newTimestamp;
      config->screen = ScreenOfDisplay (dpy, XRRRootToScreen(dpy, rep.root));
      config->current_size = size_index;
      config->current_rotation = rotation;
    }
    UnlockDisplay (dpy);
    SyncHandle ();
    return(rep.status);
}

Status XRRSetScreenConfig (Display *dpy,
			   XRRScreenConfiguration *config,
			   Drawable draw,
			   int size_index,
			   Rotation rotation, Time timestamp)
{
    return XRRSetScreenConfigAndRate (dpy, config, draw, size_index,
				      rotation, 0, timestamp);
}
    
int XRRUpdateConfiguration(XEvent *event)
{
    XRRScreenChangeNotifyEvent *scevent;
    XConfigureEvent *rcevent;
    Display *dpy = event->xany.display;
    XExtDisplayInfo *info;
    XRandRInfo *xrri;
    int snum;

    /* first, see if it is a vanilla configure notify event */
    if (event->type == ConfigureNotify) {
	rcevent = (XConfigureEvent *) event;
	snum = XRRRootToScreen(dpy, rcevent->window);
	dpy->screens[snum].width   = rcevent->width;
	dpy->screens[snum].height  = rcevent->height;
	return 1;
    }

    info = XRRFindDisplay(dpy);
    RRCheckExtension (dpy, info, 0);

    switch (event->type - info->codes->first_event) {
    case RRScreenChangeNotify:
	scevent = (XRRScreenChangeNotifyEvent *) event;
	snum = XRRRootToScreen(dpy, 
			       ((XRRScreenChangeNotifyEvent *) event)->root);
	dpy->screens[snum].width   = scevent->width;
	dpy->screens[snum].height  = scevent->height;
	dpy->screens[snum].mwidth  = scevent->mwidth;
	dpy->screens[snum].mheight = scevent->mheight;
	XRenderSetSubpixelOrder (dpy, snum, scevent->subpixel_order);
	break;
    default:
	return 0;
    }
    xrri = (XRandRInfo *) info->data;
    /* 
     * so the next time someone wants some data, it will be fetched; 
     * it might be better to force the round trip immediately, but 
     * I dislike pounding the server simultaneously when not necessary
     */
    if (xrri->config[snum] != NULL) {
	XFree (xrri->config[snum]);
	xrri->config[snum] = NULL;
    }
    return 1;
}
