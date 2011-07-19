#ifndef _GLXSCOPE_H_
#define _GLXSCOPE_H_

#define GLXREQUESTHEADER  "GLXREQUEST"
#define GLXREPLYHEADER	  "GLXREPLY"

/*
  To aid in making the choice between level 1 and level 2, we
  define the following define, which does not print relatively
  unimportant fields.
*/

#define printfield(a,b,c,d,e) if (Verbose > 1) PrintField(a,b,c,d,e)

extern void GLXRender			(FD fd, const unsigned char *buf);
extern void GLXRenderLarge		(FD fd, const unsigned char *buf);
extern void GLXCreateContext		(FD fd, const unsigned char *buf);
extern void GLXDestroyContext		(FD fd, const unsigned char *buf);
extern void GLXMakeCurrent		(FD fd, const unsigned char *buf);
extern void GLXMakeCurrentReply		(FD fd, const unsigned char *buf);
extern void GLXIsDirect			(FD fd, const unsigned char *buf);
extern void GLXIsDirectReply		(FD fd, const unsigned char *buf);
extern void GLXQueryVersion		(FD fd, const unsigned char *buf);
extern void GLXQueryVersionReply	(FD fd, const unsigned char *buf);
extern void GLXWaitGL			(FD fd, const unsigned char *buf);
extern void GLXWaitX			(FD fd, const unsigned char *buf);
extern void GLXCopyContext		(FD fd, const unsigned char *buf);
extern void GLXSwapBuffers		(FD fd, const unsigned char *buf);
extern void GLXUseXFont			(FD fd, const unsigned char *buf);
extern void GLXCreateGLXPixmap		(FD fd, const unsigned char *buf);
extern void GLXGetVisualConfigs		(FD fd, const unsigned char *buf);
extern void GLXGetVisualConfigsReply	(FD fd, const unsigned char *buf);
extern void GLXDestroyGLXPixmap		(FD fd, const unsigned char *buf);
extern void GLXVendorPrivate		(FD fd, const unsigned char *buf);
extern void GLXVendorPrivateWithReply	(FD fd, const unsigned char *buf);
extern void GLXVendorPrivateWithReplyReply(FD fd, const unsigned char *buf);
extern void GLXQueryExtensionsString	(FD fd, const unsigned char *buf);
extern void GLXQueryExtensionsStringReply(FD fd, const unsigned char *buf);
extern void GLXQueryServerString	(FD fd, const unsigned char *buf);
extern void GLXQueryServerStringReply	(FD fd, const unsigned char *buf);
extern void GLXClientInfo		(FD fd, const unsigned char *buf);
extern void GLXGetFBConfigs		(FD fd, const unsigned char *buf);
extern void GLXGetFBConfigsReply	(FD fd, const unsigned char *buf);
extern void GLXCreatePixmap		(FD fd, const unsigned char *buf);
extern void GLXDestroyPixmap		(FD fd, const unsigned char *buf);
extern void GLXCreateNewContext		(FD fd, const unsigned char *buf);
extern void GLXQueryContext		(FD fd, const unsigned char *buf);
extern void GLXQueryContextReply	(FD fd, const unsigned char *buf);
extern void GLXMakeContextCurrent	(FD fd, const unsigned char *buf);
extern void GLXMakeContextCurrentReply	(FD fd, const unsigned char *buf);
extern void GLXCreatePbuffer		(FD fd, const unsigned char *buf);
extern void GLXDestroyPbuffer		(FD fd, const unsigned char *buf);
extern void GLXGetDrawableAttributes	(FD fd, const unsigned char *buf);
extern void GLXGetDrawableAttributesReply(FD fd, const unsigned char *buf);
extern void GLXChangeDrawableAttributes	(FD fd, const unsigned char *buf);
extern void GLXCreateWindow		(FD fd, const unsigned char *buf);
extern void GLXDestroyWindow		(FD fd, const unsigned char *buf);

extern void GLXBadContextError		(FD fd, const unsigned char *buf);
extern void GLXBadContextStateError	(FD fd, const unsigned char *buf);
extern void GLXBadDrawableError		(FD fd, const unsigned char *buf);
extern void GLXBadPixmapError		(FD fd, const unsigned char *buf);
extern void GLXBadContextTagError	(FD fd, const unsigned char *buf);
extern void GLXBadCurrentWindowError	(FD fd, const unsigned char *buf);
extern void GLXBadRenderRequestError	(FD fd, const unsigned char *buf);
extern void GLXBadLargeRequestError	(FD fd, const unsigned char *buf);
extern void GLXUnsupportedPrivateRequestError(FD fd, const unsigned char *buf);
extern void GLXBadFBConfigError		(FD fd, const unsigned char *buf);
extern void GLXBadPbufferError		(FD fd, const unsigned char *buf);
extern void GLXBadCurrentDrawableError	(FD fd, const unsigned char *buf);
extern void GLXBadWindowError		(FD fd, const unsigned char *buf);

#endif

