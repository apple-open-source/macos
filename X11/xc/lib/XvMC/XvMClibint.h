/* $XFree86: xc/lib/XvMC/XvMClibint.h,v 1.5 2002/10/16 00:37:33 dawes Exp $ */

#ifndef _XVMCLIBINT_H
#define _XVMCLIBINT_H
#define NEED_REPLIES

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

#endif /* XVMCLIBINT_H */
