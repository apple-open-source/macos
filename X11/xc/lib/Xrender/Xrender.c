/*
 * $XFree86: xc/lib/Xrender/Xrender.c,v 1.14 2002/11/22 02:10:41 keithp Exp $
 *
 * Copyright © 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

#include "Xrenderint.h"

XExtensionInfo XRenderExtensionInfo;
char XRenderExtensionName[] = RENDER_NAME;

static int XRenderCloseDisplay(Display *dpy, XExtCodes *codes);

static /* const */ XExtensionHooks render_extension_hooks = {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    XRenderCloseDisplay,		/* close_display */
    NULL,				/* wire_to_event */
    NULL,				/* event_to_wire */
    NULL,				/* error */
    NULL,				/* error_string */
};

XExtDisplayInfo *
XRenderFindDisplay (Display *dpy)
{
    XExtDisplayInfo *dpyinfo;

    dpyinfo = XextFindDisplay (&XRenderExtensionInfo, dpy);
    if (!dpyinfo)
	dpyinfo = XextAddDisplay (&XRenderExtensionInfo, dpy, 
				  XRenderExtensionName,
				  &render_extension_hooks,
				  0, 0);
    return dpyinfo;
}

static int
XRenderCloseDisplay (Display *dpy, XExtCodes *codes)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    if (info->data) XFree (info->data);
    
    return XextRemoveDisplay (&XRenderExtensionInfo, dpy);
}
    
/****************************************************************************
 *                                                                          *
 *			    Render public interfaces                         *
 *                                                                          *
 ****************************************************************************/

Bool XRenderQueryExtension (Display *dpy, int *event_basep, int *error_basep)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);

    if (XextHasExtension(info)) {
	*event_basep = info->codes->first_event;
	*error_basep = info->codes->first_error;
	return True;
    } else {
	return False;
    }
}


Status XRenderQueryVersion (Display *dpy,
			    int	    *major_versionp,
			    int	    *minor_versionp)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    XRenderInfo	    *xri;

    if (!XextHasExtension (info))
	return 0;

    if (!XRenderQueryFormats (dpy))
	return 0;
    
    xri = (XRenderInfo *) info->data; 
    *major_versionp = xri->major_version;
    *minor_versionp = xri->minor_version;
    return 1;
}

static XRenderPictFormat *
_XRenderFindFormat (XRenderInfo *xri, PictFormat format)
{
    int	nf;
    
    for (nf = 0; nf < xri->nformat; nf++)
	if (xri->format[nf].id == format)
	    return &xri->format[nf];
    return 0;
}

static Visual *
_XRenderFindVisual (Display *dpy, VisualID vid)
{
    return _XVIDtoVisual (dpy, vid);
}

typedef struct _renderVersionState {
    unsigned long   version_seq;
    Bool	    error;
    int		    major_version;
    int		    minor_version;
    
} _XrenderVersionState;

static Bool
_XRenderVersionHandler (Display	    *dpy,
			xReply	    *rep,
			char	    *buf,
			int	    len,
			XPointer    data)
{
    xRenderQueryVersionReply	replbuf;
    xRenderQueryVersionReply	*repl;
    _XrenderVersionState	*state = (_XrenderVersionState *) data;

    if (dpy->last_request_read != state->version_seq)
	return False;
    if (rep->generic.type == X_Error)
    {
	state->error = True;
	return False;
    }
    repl = (xRenderQueryVersionReply *)
	_XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
		     (SIZEOF(xRenderQueryVersionReply) - SIZEOF(xReply)) >> 2,
			True);
    state->major_version = repl->majorVersion;
    state->minor_version = repl->minorVersion;
    return True;
}

Status
XRenderQueryFormats (Display *dpy)
{
    XExtDisplayInfo		*info = XRenderFindDisplay (dpy);
    _XAsyncHandler		async;
    _XrenderVersionState	async_state;
    xRenderQueryVersionReq	*vreq;
    xRenderQueryPictFormatsReply rep;
    xRenderQueryPictFormatsReq  *req;
    XRenderInfo			*xri;
    XRenderPictFormat		*format;
    XRenderScreen		*screen;
    XRenderDepth		*depth;
    XRenderVisual		*visual;
    xPictFormInfo		*xFormat;
    xPictScreen			*xScreen;
    xPictDepth			*xDepth;
    xPictVisual			*xVisual;
    CARD32			*xSubpixel;
    void			*xData;
    int				nf, ns, nd, nv;
    int				rlength;
    int				nbytes;
    
    RenderCheckExtension (dpy, info, 0);
    LockDisplay (dpy);
    if (info->data)
    {
	UnlockDisplay (dpy);
	return 1;
    }
    GetReq (RenderQueryVersion, vreq);
    vreq->reqType = info->codes->major_opcode;
    vreq->renderReqType = X_RenderQueryVersion;
    vreq->majorVersion = RENDER_MAJOR;
    vreq->minorVersion = RENDER_MINOR;
    
    async_state.version_seq = dpy->request;
    async_state.error = False;
    async.next = dpy->async_handlers;
    async.handler = _XRenderVersionHandler;
    async.data = (XPointer) &async_state;
    dpy->async_handlers = &async;
    
    GetReq (RenderQueryPictFormats, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderQueryPictFormats;
    
    if (!_XReply (dpy, (xReply *) &rep, 0, xFalse)) 
    {
	DeqAsyncHandler (dpy, &async);
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
    }
    DeqAsyncHandler (dpy, &async);
    if (async_state.error)
    {
	UnlockDisplay(dpy);
	SyncHandle();
	return 0;
    }
    /*
     * Check for the lack of sub-pixel data
     */
    if (async_state.major_version == 0 && async_state.minor_version < 6)
	rep.numSubpixel = 0;
	
    xri = (XRenderInfo *) Xmalloc (sizeof (XRenderInfo) +
				   rep.numFormats * sizeof (XRenderPictFormat) +
				   rep.numScreens * sizeof (XRenderScreen) +
				   rep.numDepths * sizeof (XRenderDepth) +
				   rep.numVisuals * sizeof (XRenderVisual));
    xri->major_version = async_state.major_version;
    xri->minor_version = async_state.minor_version;
    xri->format = (XRenderPictFormat *) (xri + 1);
    xri->nformat = rep.numFormats;
    xri->screen = (XRenderScreen *) (xri->format + rep.numFormats);
    xri->nscreen = rep.numScreens;
    xri->depth = (XRenderDepth *) (xri->screen + rep.numScreens);
    xri->ndepth = rep.numDepths;
    xri->visual = (XRenderVisual *) (xri->depth + rep.numDepths);
    xri->nvisual = rep.numVisuals;
    rlength = (rep.numFormats * sizeof (xPictFormInfo) +
	       rep.numScreens * sizeof (xPictScreen) +
	       rep.numDepths * sizeof (xPictDepth) +
	       rep.numVisuals * sizeof (xPictVisual) +
	       rep.numSubpixel * 4);
    xData = (void *) Xmalloc (rlength);
    nbytes = (int) rep.length << 2;
    
    if (!xri || !xData || nbytes < rlength)
    {
	if (xri) Xfree (xri);
	if (xData) Xfree (xData);
	_XEatData (dpy, nbytes);
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
    }
    _XRead (dpy, (char *) xData, rlength);
    format = xri->format;
    xFormat = (xPictFormInfo *) xData;
    for (nf = 0; nf < rep.numFormats; nf++)
    {
	format->id = xFormat->id;
	format->type = xFormat->type;
	format->depth = xFormat->depth;
	format->direct.red = xFormat->direct.red;
	format->direct.redMask = xFormat->direct.redMask;
	format->direct.green = xFormat->direct.green;
	format->direct.greenMask = xFormat->direct.greenMask;
	format->direct.blue = xFormat->direct.blue;
	format->direct.blueMask = xFormat->direct.blueMask;
	format->direct.alpha = xFormat->direct.alpha;
	format->direct.alphaMask = xFormat->direct.alphaMask;
	format->colormap = xFormat->colormap;
	format++;
	xFormat++;
    }
    xScreen = (xPictScreen *) xFormat;
    screen = xri->screen;
    depth = xri->depth;
    visual = xri->visual;
    for (ns = 0; ns < xri->nscreen; ns++)
    {
	screen->depths = depth;
	screen->ndepths = xScreen->nDepth;
	screen->fallback = _XRenderFindFormat (xri, xScreen->fallback);
	screen->subpixel = SubPixelUnknown;
	xDepth = (xPictDepth *) (xScreen + 1);
	for (nd = 0; nd < screen->ndepths; nd++)
	{
	    depth->depth = xDepth->depth;
	    depth->nvisuals = xDepth->nPictVisuals;
	    depth->visuals = visual;
	    xVisual = (xPictVisual *) (xDepth + 1);
	    for (nv = 0; nv < depth->nvisuals; nv++)
	    {
		visual->visual = _XRenderFindVisual (dpy, xVisual->visual);
		visual->format = _XRenderFindFormat (xri, xVisual->format);
		visual++;
		xVisual++;
	    }
	    depth++;
	    xDepth = (xPictDepth *) xVisual;
	}
	screen++;
	xScreen = (xPictScreen *) xDepth;	    
    }
    xSubpixel = (CARD32 *) xScreen;
    screen = xri->screen;
    for (ns = 0; ns < rep.numSubpixel; ns++)
    {
	screen->subpixel = *xSubpixel;
	xSubpixel++;
	screen++;
    }
    info->data = (XPointer) xri;
    /*
     * Skip any extra data
     */
    if (nbytes > rlength)
	_XEatData (dpy, (unsigned long) (nbytes - rlength));
    
    UnlockDisplay (dpy);
    SyncHandle ();
    Xfree (xData);
    return 1;
}

int
XRenderQuerySubpixelOrder (Display *dpy, int screen)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    XRenderInfo	    *xri;

    if (!XextHasExtension (info))
	return SubPixelUnknown;

    if (!XRenderQueryFormats (dpy))
	return SubPixelUnknown;

    xri = (XRenderInfo *) info->data;
    return xri->screen[screen].subpixel;
}

Bool
XRenderSetSubpixelOrder (Display *dpy, int screen, int subpixel)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    XRenderInfo	    *xri;

    if (!XextHasExtension (info))
	return False;

    if (!XRenderQueryFormats (dpy))
	return False;

    xri = (XRenderInfo *) info->data;
    xri->screen[screen].subpixel = subpixel;
    return True;
}

XRenderPictFormat *
XRenderFindVisualFormat (Display *dpy, _Xconst Visual *visual)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    int		    nv;
    XRenderInfo	    *xri;
    XRenderVisual   *xrv;
    
    RenderCheckExtension (dpy, info, 0);
    if (!XRenderQueryFormats (dpy))
        return 0;
    xri = (XRenderInfo *) info->data;
    for (nv = 0, xrv = xri->visual; nv < xri->nvisual; nv++, xrv++)
	if (xrv->visual == visual)
	    return xrv->format;
    return 0;
}

XRenderPictFormat *
XRenderFindFormat (Display		*dpy,
		   unsigned long	mask,
		   _Xconst XRenderPictFormat	*template,
		   int			count)
{
    XExtDisplayInfo *info = XRenderFindDisplay (dpy);
    int		    nf;
    XRenderInfo     *xri;
    
    RenderCheckExtension (dpy, info, 0);
    if (!XRenderQueryFormats (dpy))
	return 0;
    xri = (XRenderInfo *) info->data;
    for (nf = 0; nf < xri->nformat; nf++)
    {
	if (mask & PictFormatID)
	    if (template->id != xri->format[nf].id)
		continue;
	if (mask & PictFormatType)
	if (template->type != xri->format[nf].type)
		continue;
	if (mask & PictFormatDepth)
	    if (template->depth != xri->format[nf].depth)
		continue;
	if (mask & PictFormatRed)
	    if (template->direct.red != xri->format[nf].direct.red)
		continue;
	if (mask & PictFormatRedMask)
	    if (template->direct.redMask != xri->format[nf].direct.redMask)
		continue;
	if (mask & PictFormatGreen)
	    if (template->direct.green != xri->format[nf].direct.green)
		continue;
	if (mask & PictFormatGreenMask)
	    if (template->direct.greenMask != xri->format[nf].direct.greenMask)
		continue;
	if (mask & PictFormatBlue)
	    if (template->direct.blue != xri->format[nf].direct.blue)
		continue;
	if (mask & PictFormatBlueMask)
	    if (template->direct.blueMask != xri->format[nf].direct.blueMask)
		continue;
	if (mask & PictFormatAlpha)
	    if (template->direct.alpha != xri->format[nf].direct.alpha)
		continue;
	if (mask & PictFormatAlphaMask)
	    if (template->direct.alphaMask != xri->format[nf].direct.alphaMask)
		continue;
	if (mask & PictFormatColormap)
	    if (template->colormap != xri->format[nf].colormap)
		continue;
	if (count-- == 0)
	    return &xri->format[nf];
    }
    return 0;
}

XRenderPictFormat *
XRenderFindStandardFormat (Display  *dpy,
			   int	    format)
{
    static struct {
	XRenderPictFormat   templ;
	unsigned long	    mask;
    } standardFormats[PictStandardNUM] = {
	/* PictStandardARGB32 */
	{
	    {
		0,			    /* id */
		PictTypeDirect,		    /* type */
		32,			    /* depth */
		{			    /* direct */
		    16,			    /* direct.red */
		    0xff,		    /* direct.redMask */
		    8,			    /* direct.green */
		    0xff,		    /* direct.greenMask */
		    0,			    /* direct.blue */
		    0xff,		    /* direct.blueMask */
		    24,			    /* direct.alpha */
		    0xff,		    /* direct.alphaMask */
		},
		0,			    /* colormap */
	    },
	    PictFormatType | 
	    PictFormatDepth |
	    PictFormatRed |
	    PictFormatRedMask |
	    PictFormatGreen |
	    PictFormatGreenMask |
	    PictFormatBlue |
	    PictFormatBlueMask |
	    PictFormatAlpha |
	    PictFormatAlphaMask,
	},
	/* PictStandardRGB24 */
	{
	    {
		0,			    /* id */
		PictTypeDirect,		    /* type */
		24,			    /* depth */
		{			    /* direct */
		    16,			    /* direct.red */
		    0xff,		    /* direct.redMask */
		    8,			    /* direct.green */
		    0xff,		    /* direct.greenMask */
		    0,			    /* direct.blue */
		    0xff,		    /* direct.blueMask */
		    0,			    /* direct.alpha */
		    0x00,		    /* direct.alphaMask */
		},
		0,			    /* colormap */
	    },
	    PictFormatType | 
	    PictFormatDepth |
	    PictFormatRed |
	    PictFormatRedMask |
	    PictFormatGreen |
	    PictFormatGreenMask |
	    PictFormatBlue |
	    PictFormatBlueMask |
	    PictFormatAlphaMask,
	},
	/* PictStandardA8 */
	{
	    {
		0,			    /* id */
		PictTypeDirect,		    /* type */
		8,			    /* depth */
		{			    /* direct */
		    0,			    /* direct.red */
		    0x00,		    /* direct.redMask */
		    0,			    /* direct.green */
		    0x00,		    /* direct.greenMask */
		    0,			    /* direct.blue */
		    0x00,		    /* direct.blueMask */
		    0,			    /* direct.alpha */
		    0xff,		    /* direct.alphaMask */
		},
		0,			    /* colormap */
	    },
	    PictFormatType | 
	    PictFormatDepth |
	    PictFormatRedMask |
	    PictFormatGreenMask |
	    PictFormatBlueMask |
	    PictFormatAlpha |
	    PictFormatAlphaMask,
	},
	/* PictStandardA4 */
	{
	    {
		0,			    /* id */
		PictTypeDirect,		    /* type */
		4,			    /* depth */
		{			    /* direct */
		    0,			    /* direct.red */
		    0x00,		    /* direct.redMask */
		    0,			    /* direct.green */
		    0x00,		    /* direct.greenMask */
		    0,			    /* direct.blue */
		    0x00,		    /* direct.blueMask */
		    0,			    /* direct.alpha */
		    0x0f,		    /* direct.alphaMask */
		},
		0,			    /* colormap */
	    },
	    PictFormatType | 
	    PictFormatDepth |
	    PictFormatRedMask |
	    PictFormatGreenMask |
	    PictFormatBlueMask |
	    PictFormatAlpha |
	    PictFormatAlphaMask,
	},
	/* PictStandardA1 */
	{
	    {
		0,			    /* id */
		PictTypeDirect,		    /* type */
		1,			    /* depth */
		{			    /* direct */
		    0,			    /* direct.red */
		    0x00,		    /* direct.redMask */
		    0,			    /* direct.green */
		    0x00,		    /* direct.greenMask */
		    0,			    /* direct.blue */
		    0x00,		    /* direct.blueMask */
		    0,			    /* direct.alpha */
		    0x01,		    /* direct.alphaMask */
		},
		0,			    /* colormap */
	    },
	    PictFormatType | 
	    PictFormatDepth |
	    PictFormatRedMask |
	    PictFormatGreenMask |
	    PictFormatBlueMask |
	    PictFormatAlpha |
	    PictFormatAlphaMask,
	},
    };

    if (0 <= format && format < PictStandardNUM)
	return XRenderFindFormat (dpy, 
				  standardFormats[format].mask,
				  &standardFormats[format].templ,
				  0);
    return 0;
}

XIndexValue *
XRenderQueryPictIndexValues(Display			*dpy,
			    _Xconst XRenderPictFormat	*format,
			    int				*num)
{
    XExtDisplayInfo			*info = XRenderFindDisplay (dpy);
    xRenderQueryPictIndexValuesReq	*req;
    xRenderQueryPictIndexValuesReply	rep;
    XIndexValue				*values;
    int					nbytes, nread, rlength, i;

    RenderCheckExtension (dpy, info, 0);

    LockDisplay (dpy);
    GetReq (RenderQueryPictIndexValues, req);
    req->reqType = info->codes->major_opcode;
    req->renderReqType = X_RenderQueryPictIndexValues;
    req->format = format->id;
    if (!_XReply (dpy, (xReply *) &rep, 0, xFalse))
    {
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
    }

    /* request data length */
    nbytes = (long)rep.length << 2;
    /* bytes of actual data in the request */
    nread = rep.numIndexValues * SIZEOF (xIndexValue);
    /* size of array returned to application */
    rlength = rep.numIndexValues * sizeof (XIndexValue);

    /* allocate returned data */
    values = (XIndexValue *)Xmalloc (rlength);
    if (!values)
    {
	_XEatData (dpy, nbytes);
	UnlockDisplay (dpy);
	SyncHandle ();
	return 0;
    }

    /* read the values one at a time and convert */
    *num = rep.numIndexValues;
    for(i = 0; i < rep.numIndexValues; i++)
    {
	xIndexValue value;
	
	_XRead (dpy, (char *) &value, SIZEOF (xIndexValue));
	values[i].pixel = value.pixel;
	values[i].red = value.red;
	values[i].green = value.green;
	values[i].blue = value.blue;
	values[i].alpha = value.alpha;
    }
    /* skip any padding */
    if(nbytes > nread)
    {
	_XEatData (dpy, (unsigned long) (nbytes - nread));
    }
    UnlockDisplay (dpy);
    SyncHandle ();
    return values;
}
