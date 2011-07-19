#ifndef _XVMCLIBINT_H
#define _XVMCLIBINT_H

#include <X11/Xlibint.h>
#include <X11/extensions/Xvproto.h>
#include <X11/extensions/XvMCproto.h>
#include <X11/extensions/XvMClib.h>

#define XvMCCheckExtension(dpy, i, val) \
  XextCheckExtension(dpy, i, xvmc_extension_name, val)


#if !defined(UNIXCPP)
#define XvMCGetReq(name, req) \
        WORD64ALIGN\
	if ((dpy->bufptr + sizeof(xvmc##name##Req)) > dpy->bufmax)\
		_XFlush(dpy);\
	req = (xvmc##name##Req *)(dpy->last_req = dpy->bufptr);\
	req->reqType = info->codes->major_opcode;\
        req->xvmcReqType = xvmc_##name; \
        req->length = sizeof(xvmc##name##Req)>>2;\
	dpy->bufptr += sizeof(xvmc##name##Req);\
	dpy->request++
#else
#define XvMCGetReq(name, req) \
        WORD64ALIGN\
	if ((dpy->bufptr + sizeof(xvmc/**/name/**/Req)) > dpy->bufmax)\
		_XFlush(dpy);\
	req = (xvmc/**/name/**/Req *)(dpy->last_req = dpy->bufptr);\
	req->reqType = info->codes->major_opcode;\
	req->xvmcReqType = xvmc_/**/name;\
	req->length = sizeof(xvmc/**/name/**/Req)>>2;\
	dpy->bufptr += sizeof(xvmc/**/name/**/Req);\
	dpy->request++
#endif

_XFUNCPROTOBEGIN

Status _xvmc_create_context(
    Display *		/* dpy */,
    XvMCContext *	/* context */,
    int *		/* priv_count */,
    CARD32 **		/* priv_data */
);

Status _xvmc_destroy_context(
    Display *		/* dpy */,
    XvMCContext *	/* context */
);

Status _xvmc_create_surface (
    Display *		/* dpy */,
    XvMCContext *	/* context */,
    XvMCSurface *	/* surface */,
    int *		/* priv_count */,
    CARD32 **		/* priv_data */
);

Status _xvmc_destroy_surface (
    Display *		/* dpy */,
    XvMCSurface *	/* surface */
);

Status _xvmc_create_subpicture (
    Display *		/* dpy */,
    XvMCContext *	/* context */,
    XvMCSubpicture *	/* subpicture */,
    int *		/* priv_count */,
    CARD32 **		/* priv_data */
);

Status _xvmc_destroy_subpicture(
    Display *		/* dpy */,
    XvMCSubpicture *	/* subpicture */
);

_XFUNCPROTOEND

#endif /* XVMCLIBINT_H */
