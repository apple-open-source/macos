/* $XFree86: xc/lib/GL/glx/glxcmds.c,v 1.19 2003/01/20 21:37:18 tsi Exp $ */
/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.1 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
** 
** http://oss.sgi.com/projects/FreeB
** 
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
** 
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
** 
** Additional Notice Provisions: The application programming interfaces
** established by SGI in conjunction with the Original Code are The
** OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
** April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
** 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
** Window System(R) (Version 1.3), released October 19, 1998. This software
** was created using the OpenGL(R) version 1.2.1 Sample Implementation
** published by SGI, but has not been independently verified as being
** compliant with the OpenGL(R) version 1.2.1 Specification.
**
*/

#include "packsingle.h"
#include "glxclient.h"
#include <extutil.h>
#include <Xext.h>
#include <string.h>
#include "glapi.h"
#ifdef GLX_DIRECT_RENDERING
#include "indirect_init.h"
#endif

static const char __glXGLClientExtensions[] = 
			"GL_ARB_imaging "
			"GL_ARB_multitexture "
			"GL_ARB_texture_border_clamp "
			"GL_ARB_texture_cube_map "
			"GL_ARB_texture_env_add "
			"GL_ARB_texture_env_combine "
			"GL_ARB_texture_env_dot3 "
			"GL_ARB_transpose_matrix "
			"GL_EXT_abgr "
			"GL_EXT_blend_color "
			"GL_EXT_blend_minmax "
			"GL_EXT_blend_subtract "
			"GL_EXT_texture_env_add "
			"GL_EXT_texture_env_combine "
			"GL_EXT_texture_env_dot3 "
			"GL_EXT_texture_lod_bias "
			;

static const char __glXGLXClientVendorName[] = "SGI";
static const char __glXGLXClientVersion[] = "1.2";
static const char __glXGLXDefaultClientExtensions[] = 
			"GLX_EXT_visual_info "
			"GLX_EXT_visual_rating "
			"GLX_EXT_import_context "
			;

static const char *__glXGLXClientExtensions = __glXGLXDefaultClientExtensions;


/*
** Create a new context.
*/
static
GLXContext CreateContext(Display *dpy, XVisualInfo *vis,
				GLXContext shareList, 
				Bool allowDirect, GLXContextID contextID)
{
    xGLXCreateContextReq *req;
    GLXContext gc;
    int bufSize = XMaxRequestSize(dpy) * 4;
    CARD8 opcode;
#ifdef GLX_DIRECT_RENDERING
    __GLXdisplayPrivate *priv;
#endif

    if (!dpy || !vis)
       return NULL;

    opcode = __glXSetupForCommand(dpy);
    if (!opcode) {
	return NULL;
    }

    /* Allocate our context record */
    gc = (GLXContext) Xmalloc(sizeof(struct __GLXcontextRec));
    if (!gc) {
	/* Out of memory */
	return NULL;
    }
    memset(gc, 0, sizeof(struct __GLXcontextRec));

    /* Allocate transport buffer */
    gc->buf = (GLubyte *) Xmalloc(bufSize);
    if (!gc->buf) {
	Xfree(gc);
	return NULL;
    }
    gc->bufSize = bufSize;

    /* Fill in the new context */
    gc->renderMode = GL_RENDER;

    gc->state.storePack.alignment = 4;
    gc->state.storeUnpack.alignment = 4;

    __glXInitVertexArrayState(gc);

    gc->attributes.stackPointer = &gc->attributes.stack[0];

    /*
    ** PERFORMANCE NOTE: A mode dependent fill image can speed things up.
    ** Other code uses the fastImageUnpack bit, but it is never set
    ** to GL_TRUE.
    */
    gc->fastImageUnpack = GL_FALSE;
    gc->fillImage = __glFillImage;
    gc->isDirect = GL_FALSE;
    gc->pc = gc->buf;
    gc->bufEnd = gc->buf + bufSize;
    if (__glXDebug) {
	/*
	** Set limit register so that there will be one command per packet
	*/
	gc->limit = gc->buf;
    } else {
	gc->limit = gc->buf + bufSize - __GLX_BUFFER_LIMIT_SIZE;
    }
    gc->createDpy = dpy;
    gc->majorOpcode = opcode;

    /*
    ** Constrain the maximum drawing command size allowed to be
    ** transfered using the X_GLXRender protocol request.  First
    ** constrain by a software limit, then constrain by the protocl
    ** limit.
    */
    if (bufSize > __GLX_RENDER_CMD_SIZE_LIMIT) {
        bufSize = __GLX_RENDER_CMD_SIZE_LIMIT;
    }
    if (bufSize > __GLX_MAX_RENDER_CMD_SIZE) {
        bufSize = __GLX_MAX_RENDER_CMD_SIZE;
    }
    gc->maxSmallRenderCommandSize = bufSize;


    if (None == contextID) {
#ifdef GLX_DIRECT_RENDERING
	/*
	** Create the direct rendering context, if requested and
	** available.
	*/
	priv = __glXInitialize(dpy);
	if (allowDirect && priv->driDisplay.private) {
	    __GLXscreenConfigs *psc = &priv->screenConfigs[vis->screen];
	    if (psc && psc->driScreen.private) {
		void *shared = (shareList ?
				shareList->driContext.private : NULL);
		gc->driContext.private =
		    (*psc->driScreen.createContext)(dpy, vis, shared,
						    &gc->driContext);
		if (gc->driContext.private) {
		    gc->isDirect = GL_TRUE;
		    gc->screen = vis->screen;
		    gc->vid = vis->visualid;
		}
	    }
	}
#endif

	/* Send the glXCreateContext request */
	LockDisplay(dpy);
	GetReq(GLXCreateContext,req);
	req->reqType = gc->majorOpcode;
	req->glxCode = X_GLXCreateContext;
	req->context = gc->xid = XAllocID(dpy);
	req->visual = vis->visualid;
	req->screen = vis->screen;
	req->shareList = shareList ? shareList->xid : None;
	req->isDirect = gc->isDirect;
	UnlockDisplay(dpy);
	SyncHandle();
	gc->imported = GL_FALSE;
    }
    else {
	gc->xid = contextID;
	gc->imported = GL_TRUE;
    }

    return gc;
}

GLXContext GLX_PREFIX(glXCreateContext)(Display *dpy, XVisualInfo *vis,
			    GLXContext shareList, Bool allowDirect)
{
   return CreateContext(dpy, vis, shareList, allowDirect, None);
}

void __glXFreeContext(__GLXcontext *gc)
{
    if (gc->vendor) XFree((char *) gc->vendor);
    if (gc->renderer) XFree((char *) gc->renderer);
    if (gc->version) XFree((char *) gc->version);
    if (gc->extensions) XFree((char *) gc->extensions);
    __glFreeAttributeState(gc);
    XFree((char *) gc->buf);
    XFree((char *) gc);
    
}

/*
** Destroy the named context
*/
static void 
DestroyContext(Display *dpy, GLXContext gc)
{
    xGLXDestroyContextReq *req;
    GLXContextID xid;
    CARD8 opcode;
    GLboolean imported;

    opcode = __glXSetupForCommand(dpy);
    if (!opcode || !gc) {
	return;
    }

    __glXLock();
    xid = gc->xid;
    imported = gc->imported;
    gc->xid = None;

#ifdef GLX_DIRECT_RENDERING
    /* Destroy the direct rendering context */
    if (gc->isDirect) {
	if (gc->driContext.private) {
	    (*gc->driContext.destroyContext)(dpy, gc->screen,
					     gc->driContext.private);
	    gc->driContext.private = NULL;
	}
    }
#endif

    if (gc->currentDpy) {
	/* Have to free later cuz it's in use now */
	__glXUnlock();
    } else {
	/* Destroy the handle if not current to anybody */
	__glXUnlock();
	__glXFreeContext(gc);
    }

    if (!imported) {
	/* 
	** This dpy also created the server side part of the context.
	** Send the glXDestroyContext request.
	*/
	LockDisplay(dpy);
	GetReq(GLXDestroyContext,req);
	req->reqType = opcode;
	req->glxCode = X_GLXDestroyContext;
	req->context = xid;
	UnlockDisplay(dpy);
	SyncHandle();
    }
}
void GLX_PREFIX(glXDestroyContext)(Display *dpy, GLXContext gc)
{
    DestroyContext(dpy, gc);
}

/*
** Return the major and minor version #s for the GLX extension
*/
Bool GLX_PREFIX(glXQueryVersion)(Display *dpy, int *major, int *minor)
{
    __GLXdisplayPrivate *priv;

    /* Init the extension.  This fetches the major and minor version. */
    priv = __glXInitialize(dpy);
    if (!priv) return GL_FALSE;

    if (major) *major = priv->majorVersion;
    if (minor) *minor = priv->minorVersion;
    return GL_TRUE;
}

/*
** Query the existance of the GLX extension
*/
Bool GLX_PREFIX(glXQueryExtension)(Display *dpy, int *errorBase, int *eventBase)
{
    int major_op, erb, evb;
    Bool rv;

    rv = XQueryExtension(dpy, GLX_EXTENSION_NAME, &major_op, &evb, &erb);
    if (rv) {
	if (errorBase) *errorBase = erb;
	if (eventBase) *eventBase = evb;
    }
    return rv;
}

/*
** Put a barrier in the token stream that forces the GL to finish its
** work before X can proceed.
*/
void GLX_PREFIX(glXWaitGL)(void)
{
    xGLXWaitGLReq *req;
    GLXContext gc = __glXGetCurrentContext();
    Display *dpy = gc->currentDpy;

    if (!dpy) return;

    /* Flush any pending commands out */
    __glXFlushRenderBuffer(gc, gc->pc);

#ifdef GLX_DIRECT_RENDERING
    if (gc->isDirect) {
/* This bit of ugliness unwraps the glFinish function */
#ifdef glFinish
#undef glFinish
#endif
	glFinish();
	return;
    }
#endif

    /* Send the glXWaitGL request */
    LockDisplay(dpy);
    GetReq(GLXWaitGL,req);
    req->reqType = gc->majorOpcode;
    req->glxCode = X_GLXWaitGL;
    req->contextTag = gc->currentContextTag;
    UnlockDisplay(dpy);
    SyncHandle();
}

/*
** Put a barrier in the token stream that forces X to finish its
** work before GL can proceed.
*/
void GLX_PREFIX(glXWaitX)(void)
{
    xGLXWaitXReq *req;
    GLXContext gc = __glXGetCurrentContext();
    Display *dpy = gc->currentDpy;

    if (!dpy) return;

    /* Flush any pending commands out */
    __glXFlushRenderBuffer(gc, gc->pc);

#ifdef GLX_DIRECT_RENDERING
    if (gc->isDirect) {
	XSync(dpy, False);
	return;
    }
#endif

    /*
    ** Send the glXWaitX request.
    */
    LockDisplay(dpy);
    GetReq(GLXWaitX,req);
    req->reqType = gc->majorOpcode;
    req->glxCode = X_GLXWaitX;
    req->contextTag = gc->currentContextTag;
    UnlockDisplay(dpy);
    SyncHandle();
}

void GLX_PREFIX(glXUseXFont)(Font font, int first, int count, int listBase)
{
    xGLXUseXFontReq *req;
    GLXContext gc = __glXGetCurrentContext();
    Display *dpy = gc->currentDpy;

    if (!dpy) return;

    /* Flush any pending commands out */
    (void) __glXFlushRenderBuffer(gc, gc->pc);

#ifdef GLX_DIRECT_RENDERING
    if (gc->isDirect) {
      DRI_glXUseXFont(font, first, count, listBase);
      return;
    }
#endif

    /* Send the glXUseFont request */
    LockDisplay(dpy);
    GetReq(GLXUseXFont,req);
    req->reqType = gc->majorOpcode;
    req->glxCode = X_GLXUseXFont;
    req->contextTag = gc->currentContextTag;
    req->font = font;
    req->first = first;
    req->count = count;
    req->listBase = listBase;
    UnlockDisplay(dpy);
    SyncHandle();
}

/************************************************************************/

/*
** Copy the source context to the destination context using the
** attribute "mask".
*/
void GLX_PREFIX(glXCopyContext)(Display *dpy, GLXContext source, GLXContext dest,
		    unsigned long mask)
{
    xGLXCopyContextReq *req;
    GLXContext gc = __glXGetCurrentContext();
    GLXContextTag tag;
    CARD8 opcode;

    opcode = __glXSetupForCommand(dpy);
    if (!opcode) {
	return;
    }

#ifdef GLX_DIRECT_RENDERING
    if (gc->isDirect) {
	/* NOT_DONE: This does not work yet */
    }
#endif

    /*
    ** If the source is the current context, send its tag so that the context
    ** can be flushed before the copy.
    */
    if (source == gc && dpy == gc->currentDpy) {
	tag = gc->currentContextTag;
    } else {
	tag = 0;
    }

    /* Send the glXCopyContext request */
    LockDisplay(dpy);
    GetReq(GLXCopyContext,req);
    req->reqType = opcode;
    req->glxCode = X_GLXCopyContext;
    req->source = source ? source->xid : None;
    req->dest = dest ? dest->xid : None;
    req->mask = mask;
    req->contextTag = tag;
    UnlockDisplay(dpy);
    SyncHandle();
}


/*
** Return GL_TRUE if the context is direct rendering or not.
*/
static Bool __glXIsDirect(Display *dpy, GLXContextID contextID)
{
    xGLXIsDirectReq *req;
    xGLXIsDirectReply reply;
    CARD8 opcode;

    opcode = __glXSetupForCommand(dpy);
    if (!opcode) {
	return GL_FALSE;
    }

    /* Send the glXIsDirect request */
    LockDisplay(dpy);
    GetReq(GLXIsDirect,req);
    req->reqType = opcode;
    req->glxCode = X_GLXIsDirect;
    req->context = contextID;
    _XReply(dpy, (xReply*) &reply, 0, False);
    UnlockDisplay(dpy);
    SyncHandle();

    return reply.isDirect;
}

Bool GLX_PREFIX(glXIsDirect)(Display *dpy, GLXContext gc)
{
    if (!gc) {
	return GL_FALSE;
#ifdef GLX_DIRECT_RENDERING
    } else if (gc->isDirect) {
	return GL_TRUE;
#endif
    }
    return __glXIsDirect(dpy, gc->xid);
}

GLXPixmap GLX_PREFIX(glXCreateGLXPixmap)(Display *dpy, XVisualInfo *vis, Pixmap pixmap)
{
    xGLXCreateGLXPixmapReq *req;
    GLXPixmap xid;
    CARD8 opcode;

    opcode = __glXSetupForCommand(dpy);
    if (!opcode) {
	return None;
    }

    /* Send the glXCreateGLXPixmap request */
    LockDisplay(dpy);
    GetReq(GLXCreateGLXPixmap,req);
    req->reqType = opcode;
    req->glxCode = X_GLXCreateGLXPixmap;
    req->screen = vis->screen;
    req->visual = vis->visualid;
    req->pixmap = pixmap;
    req->glxpixmap = xid = XAllocID(dpy);
    UnlockDisplay(dpy);
    SyncHandle();
    return xid;
}

/*
** Destroy the named pixmap
*/
void GLX_PREFIX(glXDestroyGLXPixmap)(Display *dpy, GLXPixmap glxpixmap)
{
    xGLXDestroyGLXPixmapReq *req;
    CARD8 opcode;

    opcode = __glXSetupForCommand(dpy);
    if (!opcode) {
	return;
    }
    
    /* Send the glXDestroyGLXPixmap request */
    LockDisplay(dpy);
    GetReq(GLXDestroyGLXPixmap,req);
    req->reqType = opcode;
    req->glxCode = X_GLXDestroyGLXPixmap;
    req->glxpixmap = glxpixmap;
    UnlockDisplay(dpy);
    SyncHandle();
}

void GLX_PREFIX(glXSwapBuffers)(Display *dpy, GLXDrawable drawable)
{
    xGLXSwapBuffersReq *req;
    GLXContext gc = __glXGetCurrentContext();
    GLXContextTag tag;
    CARD8 opcode;
#ifdef GLX_DIRECT_RENDERING
    __GLXdisplayPrivate *priv;
    __DRIdrawable *pdraw;

    priv = __glXInitialize(dpy);
    if (priv->driDisplay.private) {
	__GLXscreenConfigs *psc = &priv->screenConfigs[gc->screen];
	if (psc && psc->driScreen.private) {
	    /*
	    ** getDrawable returning NULL implies that the drawable is
	    ** not bound to a direct rendering context.
	    */
	    pdraw = (*psc->driScreen.getDrawable)(dpy, drawable,
						  psc->driScreen.private);
	    if (pdraw) {
		(*pdraw->swapBuffers)(dpy, pdraw->private);
		return;
	    }
	}
    }
#endif

    opcode = __glXSetupForCommand(dpy);
    if (!opcode) {
	return;
    }

    /*
    ** The calling thread may or may not have a current context.  If it
    ** does, send the context tag so the server can do a flush.
    */
    if ((dpy == gc->currentDpy) && (drawable == gc->currentDrawable)) {
	tag = gc->currentContextTag;
    } else {
	tag = 0;
    }

    /* Send the glXSwapBuffers request */
    LockDisplay(dpy);
    GetReq(GLXSwapBuffers,req);
    req->reqType = opcode;
    req->glxCode = X_GLXSwapBuffers;
    req->drawable = drawable;
    req->contextTag = tag;
    UnlockDisplay(dpy);
    SyncHandle();
    XFlush(dpy);
}

/*
** Return configuration information for the given display, screen and
** visual combination.
*/
int GLX_PREFIX(glXGetConfig)(Display *dpy, XVisualInfo *vis, int attribute,
		 int *value_return)
{
    __GLXvisualConfig *pConfig;
    __GLXscreenConfigs *psc;
    __GLXdisplayPrivate *priv;
    GLint i;

    /* Initialize the extension, if needed */
    priv = __glXInitialize(dpy);
    if (!priv) {
	/* No extension */
	return GLX_NO_EXTENSION;
    }

    /* Check screen number to see if its valid */
    if ((vis->screen < 0) || (vis->screen >= ScreenCount(dpy))) {
	return GLX_BAD_SCREEN;
    }

    /* Check to see if the GL is supported on this screen */
    psc = &priv->screenConfigs[vis->screen];
    pConfig = psc->configs;
    if (!pConfig) {
	/* No support for GL on this screen regardless of visual */
	if (attribute == GLX_USE_GL) {
	    *value_return = GL_FALSE;
	    return Success;
	}
	return GLX_BAD_VISUAL;
    }

    /* Lookup attribute after first finding a match on the visual */
    for (i = psc->numConfigs; --i >= 0; pConfig++) {
	if (pConfig->vid == vis->visualid) {
	    switch (attribute) {
	      case GLX_USE_GL:
		*value_return = GL_TRUE;
		return Success;
	      case GLX_BUFFER_SIZE:
		*value_return =  pConfig->bufferSize;
		return Success;
	      case GLX_RGBA:
		*value_return = pConfig->rgba;
		return Success;
	      case GLX_RED_SIZE:
		*value_return =  pConfig->redSize;
		return Success;
	      case GLX_GREEN_SIZE:
		*value_return =  pConfig->greenSize;
		return Success;
	      case GLX_BLUE_SIZE:
		*value_return =  pConfig->blueSize;
		return Success;
	      case GLX_ALPHA_SIZE:
		*value_return =  pConfig->alphaSize;
		return Success;
	      case GLX_DOUBLEBUFFER:
		*value_return =  pConfig->doubleBuffer;
		return Success;
	      case GLX_STEREO:
		*value_return =  pConfig->stereo;
		return Success;
	      case GLX_AUX_BUFFERS:
		*value_return =  pConfig->auxBuffers;
		return Success;
	      case GLX_DEPTH_SIZE:
		*value_return =  pConfig->depthSize;
		return Success;
	      case GLX_STENCIL_SIZE:
		*value_return =  pConfig->stencilSize;
		return Success;
	      case GLX_ACCUM_RED_SIZE:
		*value_return =  pConfig->accumRedSize;
		return Success;
	      case GLX_ACCUM_GREEN_SIZE:
		*value_return =  pConfig->accumGreenSize;
		return Success;
	      case GLX_ACCUM_BLUE_SIZE:
		*value_return =  pConfig->accumBlueSize;
		return Success;
	      case GLX_ACCUM_ALPHA_SIZE:
		*value_return =  pConfig->accumAlphaSize;
		return Success;
	      case GLX_LEVEL:
		*value_return =  pConfig->level;
		return Success;
	      case GLX_TRANSPARENT_TYPE_EXT:
		*value_return = pConfig->transparentPixel;
		return Success;
	      case GLX_TRANSPARENT_RED_VALUE_EXT:
		*value_return = pConfig->transparentRed;
		return Success;
	      case GLX_TRANSPARENT_GREEN_VALUE_EXT:
		*value_return = pConfig->transparentGreen;
		return Success;
	      case GLX_TRANSPARENT_BLUE_VALUE_EXT:
		*value_return = pConfig->transparentBlue;
		return Success;
	      case GLX_TRANSPARENT_ALPHA_VALUE_EXT:
		*value_return = pConfig->transparentAlpha;
		return Success;
	      case GLX_TRANSPARENT_INDEX_VALUE_EXT:
		*value_return = pConfig->transparentIndex;
		return Success;
	      case GLX_X_VISUAL_TYPE_EXT:
		switch(pConfig->class) {
		    case TrueColor:    
		      *value_return = GLX_TRUE_COLOR_EXT;   break;
		    case DirectColor:  
		      *value_return = GLX_DIRECT_COLOR_EXT; break;
		    case PseudoColor:  
		      *value_return = GLX_PSEUDO_COLOR_EXT; break;
		    case StaticColor:  
		      *value_return = GLX_STATIC_COLOR_EXT; break;
		    case GrayScale:    
		      *value_return = GLX_GRAY_SCALE_EXT;   break;
		    case StaticGray:   
		      *value_return = GLX_STATIC_GRAY_EXT;  break;
		}
		return Success;
	      case GLX_VISUAL_CAVEAT_EXT:
		*value_return = pConfig->visualRating;
		return Success;
	      default:
		return GLX_BAD_ATTRIBUTE;
	    }
	}
    }

    /*
    ** If we can't find the config for this visual, this visual is not
    ** supported by the OpenGL implementation on the server.
    */
    if (attribute == GLX_USE_GL) {
	*value_return = GL_FALSE;
	return Success;
    }
    return GLX_BAD_VISUAL;
}

/************************************************************************/

/*
** Penalize for more auxiliary buffers than requested
*/
static int AuxScore(int minAux, int aux)
{
    return minAux - aux;
}

/*
** If color is desired, give increasing score for amount available.
** Scale this score by a multiplier to make color differences more
** important than other differences.  Otherwise give decreasing score for
** amount available.
*/
static int ColorScore(int minColor, int color)
{
    if (minColor)
	return 4 * (color - minColor);
    else
	return -color;
}

/*
** If accum buffer is desired, give increasing score for amount
** available.  Otherwise give decreasing score for amount available.
*/
static int AccumScore(int minAccum, int accum)
{
    if (minAccum)
	return accum - minAccum;
    else
	return -accum;
}

/*
** Penalize for indexes larger than requested
*/
static int IndexScore(int minIndex, int ix)
{
    return minIndex - ix;
}

/*
** If depth buffer is desired, give increasing score for amount
** available.  Scale this score by a multiplier to make depth differences
** more important than other non-color differences.  Otherwise give
** decreasing score for amount available.
*/
static int DepthScore(int minDepth, int depth)
{
    if (minDepth)
	return 2 * (depth - minDepth);
    else
	return -depth;
}

/*
** Penalize for stencil buffer larger than requested
*/
static int StencilScore(int minStencil, int stencil)
{
    return minStencil - stencil;
}

/* "Logical" xor - like && or ||; would be ^^ */
#define __GLX_XOR(a,b) (((a) && !(b)) || (!(a) && (b)))

/* Fetch a configuration value */
#define __GLX_GCONF(attrib)				    \
    if (GLX_PREFIX(glXGetConfig)(dpy, thisVis, attrib, &val)) { 	    \
	XFree((char *)visualList);			    \
	return NULL;					    \
    }


/*
** Return the visual that best matches the template.  Return None if no
** visual matches the template.
*/
XVisualInfo *GLX_PREFIX(glXChooseVisual)(Display *dpy, int screen, int *attribList)
{
    XVisualInfo visualTemplate;
    XVisualInfo *visualList;
    XVisualInfo *thisVis;
    int count, i, maxscore = 0, maxi, score, val, thisVisRating, maxRating = 0;

    /*
    ** Declare and initialize template variables
    */
    int bufferSize = 0;
    int level = 0;
    int rgba = 0;
    int doublebuffer = 0;
    int stereo = 0;
    int auxBuffers = 0;
    int redSize = 0;
    int greenSize = 0;
    int blueSize = 0;
    int alphaSize = 0;
    int depthSize = 0;
    int stencilSize = 0;
    int accumRedSize = 0;
    int accumGreenSize = 0;
    int accumBlueSize = 0;
    int accumAlphaSize = 0;
    /* for visual_info extension */
    int visualType = 0;		
    int visualTypeValue = 0;
    int transparentPixel = 0;
    int transparentPixelValue = GLX_NONE_EXT; 
    int transparentIndex = 0;
    int transparentIndexValue = 0; 
    int transparentRed = 0;	
    int transparentRedValue = 0;
    int transparentGreen = 0;
    int transparentGreenValue = 0; 
    int transparentBlue = 0;
    int transparentBlueValue = 0; 
    int transparentAlpha = 0;	
    int transparentAlphaValue = 0; 
    /* for visual_rating extension */
    int visualRating = 0; 
    int visualRatingValue = GLX_NONE_EXT; 

    /*
    ** Get a list of all visuals, return if list is empty
    */
    visualTemplate.screen = screen;
    visualList = XGetVisualInfo(dpy,VisualScreenMask,&visualTemplate,&count);
    if (visualList == NULL)
	return None;

    /*
    ** Build a template from the defaults and the attribute list
    ** Free visual list and return if an unexpected token is encountered
    */
    while (*attribList != None) {
	switch (*attribList++) {
	  case GLX_USE_GL:
	    break;
	  case GLX_BUFFER_SIZE:
	    bufferSize = *attribList++;
	    break;
	  case GLX_LEVEL:
	    level = *attribList++;
	    break;
	  case GLX_RGBA:
	    rgba = 1;
	    break;
	  case GLX_DOUBLEBUFFER:
	    doublebuffer = 1;
	    break;
	  case GLX_STEREO:
	    stereo = 1;
	    break;
	  case GLX_AUX_BUFFERS:
	    auxBuffers = *attribList++;
	    break;
	  case GLX_RED_SIZE:
	    redSize = *attribList++;
	    break;
	  case GLX_GREEN_SIZE:
	    greenSize = *attribList++;
	    break;
	  case GLX_BLUE_SIZE:
	    blueSize = *attribList++;
	    break;
	  case GLX_ALPHA_SIZE:
	    alphaSize = *attribList++;
	    break;
	  case GLX_DEPTH_SIZE:
	    depthSize = *attribList++;
	    break;
	  case GLX_STENCIL_SIZE:
	    stencilSize = *attribList++;
	    break;
	  case GLX_ACCUM_RED_SIZE:
	    accumRedSize = *attribList++;
	    break;
	  case GLX_ACCUM_GREEN_SIZE:
	    accumGreenSize = *attribList++;
	    break;
	  case GLX_ACCUM_BLUE_SIZE:
	    accumBlueSize = *attribList++;
	    break;
	  case GLX_ACCUM_ALPHA_SIZE:
	    accumAlphaSize = *attribList++;
	    break;
	  case GLX_X_VISUAL_TYPE_EXT:
	    visualType = 1;
	    visualTypeValue = *attribList++;
	    break;
	  case GLX_TRANSPARENT_TYPE_EXT:
	    transparentPixel = 1;
	    transparentPixelValue = *attribList++;
	    break;
	  case GLX_TRANSPARENT_INDEX_VALUE_EXT:
	    transparentIndex= 1;
	    transparentIndexValue = *attribList++;
	    break;
	  case GLX_TRANSPARENT_RED_VALUE_EXT:
	    transparentRed = 1;
	    transparentRedValue = *attribList++;
	    break;
	  case GLX_TRANSPARENT_GREEN_VALUE_EXT:
	    transparentGreen = 1;
	    transparentGreenValue = *attribList++;
	    break;
	  case GLX_TRANSPARENT_BLUE_VALUE_EXT:
	    transparentBlue = 1;
	    transparentBlueValue = *attribList++;
	    break;
	  case GLX_TRANSPARENT_ALPHA_VALUE_EXT:
	    transparentAlpha = 1;
	    transparentAlphaValue = *attribList++;
	    break;
	  case GLX_VISUAL_CAVEAT_EXT:
	    visualRating = 1;
	    visualRatingValue = *attribList++;
	    break;
	  default:
	    XFree((char *)visualList);
	    return None;
	}
    }

    /*
    ** Eliminate visuals that don't meet minimum requirements
    ** Compute a score for those that do
    ** Remember which visual, if any, got the highest score
    */
    maxi = -1;
    for (i = 0; i < count; i++) {
	score = 0;
	thisVis = &visualList[i];	/* NOTE: used by __GLX_GCONF */

	if (thisVis->class == TrueColor || thisVis->class == PseudoColor) {
	    /* Bump score by one for TrueColor and PseudoColor visuals. */
	    score++;
	}
	
	__GLX_GCONF(GLX_USE_GL);
	if (! val)
	    continue;
	__GLX_GCONF(GLX_LEVEL);
	if (level != val)
	    continue;
	__GLX_GCONF(GLX_RGBA);
	if (__GLX_XOR(rgba, val))
	    continue;
	__GLX_GCONF(GLX_DOUBLEBUFFER);
	if (__GLX_XOR(doublebuffer, val))
	    continue;
	__GLX_GCONF(GLX_STEREO);
	if (__GLX_XOR(stereo, val))
	    continue;
	__GLX_GCONF(GLX_AUX_BUFFERS);
	if (auxBuffers > val)
	    continue;
	else
	    score += AuxScore(auxBuffers, val);
	if (transparentPixel) {
	    __GLX_GCONF(GLX_TRANSPARENT_TYPE_EXT);
	    if (transparentPixelValue != val)
		continue;
	    if (transparentPixelValue == GLX_TRANSPARENT_TYPE_EXT) {
		if (rgba) {
		    __GLX_GCONF(GLX_TRANSPARENT_RGB_EXT);
		    if (transparentRed) {
			__GLX_GCONF(GLX_TRANSPARENT_RED_VALUE_EXT);
			if (transparentRedValue != val)
			    continue;
		    }
		    if (transparentGreen) {
			__GLX_GCONF(GLX_TRANSPARENT_GREEN_VALUE_EXT);
			if (transparentGreenValue != val)
			    continue;
		    }
		    if (transparentBlue) {
			__GLX_GCONF(GLX_TRANSPARENT_BLUE_VALUE_EXT);
			if (transparentBlueValue != val)
		    continue;
		    }
		    /* Transparent Alpha ignored for now */
		} else {
		    __GLX_GCONF(GLX_TRANSPARENT_INDEX_EXT);
		    if (transparentIndex) {
			__GLX_GCONF(GLX_TRANSPARENT_INDEX_VALUE_EXT);
			if (transparentIndexValue != val)
			    continue;
		    }
		}
	    }
	}
	if (visualType) {
	    __GLX_GCONF(GLX_X_VISUAL_TYPE_EXT);
	    if (visualTypeValue != val)
		continue;
	} else if (rgba) {
	    /* If the extension isn't specified then insure that rgba
	    ** and ci return the usual visual types.
	    */
	    if (!(thisVis->class == TrueColor || thisVis->class == DirectColor))
		continue;
	} else {
	    if (!(thisVis->class == PseudoColor 
			|| thisVis->class == StaticColor))
		continue;
	}
	    
	__GLX_GCONF(GLX_VISUAL_CAVEAT_EXT);
	/** 
	** Unrated visuals are given rating GLX_NONE.
	*/
	thisVisRating = val ? val : GLX_NONE_EXT;
	if (visualRating && (visualRatingValue != val))
	    continue;
	if (rgba) {
	    __GLX_GCONF(GLX_RED_SIZE);
	    if (redSize > val)
		continue;
	    else 
		score += ColorScore(redSize,val);
	    __GLX_GCONF(GLX_GREEN_SIZE);
	    if (greenSize > val)
		continue;
	    else 
		score += ColorScore(greenSize, val);
	    __GLX_GCONF(GLX_BLUE_SIZE);
	    if (blueSize > val)
		continue;
	    else 
		score += ColorScore(blueSize, val);
	    __GLX_GCONF(GLX_ALPHA_SIZE);
	    if (alphaSize > val)
		continue;
	    else 
		score += ColorScore(alphaSize, val);
	    __GLX_GCONF(GLX_ACCUM_RED_SIZE);
	    if (accumRedSize > val)
		continue;
	    else 
		score += AccumScore(accumRedSize, val);
	    __GLX_GCONF(GLX_ACCUM_GREEN_SIZE);
	    if (accumGreenSize > val)
		continue;
	    else 
		score += AccumScore(accumGreenSize, val);
	    __GLX_GCONF(GLX_ACCUM_BLUE_SIZE);
	    if (accumBlueSize > val)
		continue;
	    else 
		score += AccumScore(accumBlueSize, val);
	    __GLX_GCONF(GLX_ACCUM_ALPHA_SIZE);
	    if (accumAlphaSize > val)
		continue;
	    else 
		score += AccumScore(accumAlphaSize, val);
	} else {
	    __GLX_GCONF(GLX_BUFFER_SIZE);
	    if (bufferSize > val)
		continue;
	    else
		score += IndexScore(bufferSize, val);
	}
	__GLX_GCONF(GLX_DEPTH_SIZE);
	if (depthSize > val)
	    continue;
	else
	    score += DepthScore(depthSize, val);
	__GLX_GCONF(GLX_STENCIL_SIZE);
	if (stencilSize > val)
	    continue;
	else
	    score += StencilScore(stencilSize, val);

	/*
	** The visual_rating extension indicates that a NONE visual
	** is always returned in preference to a SLOW one.
	** Note that enum values are in increasing order (NONE < SLOW).
	*/
	if (maxi < 0 || maxRating > thisVisRating) {
	    maxi = i;
	    maxscore = score;
	    maxRating = thisVisRating;
	} else {
	    if (score > maxscore) {
		maxi = i;
		maxscore = score;
	    }
	}
    }

    /*
    ** If no visual is acceptable, return None
    ** Otherwise, create an XVisualInfo list with just the selected X visual
    **   and return this after freeing the original list
    */
    if (maxi < 0) {
	XFree((char *)visualList);
	return None;
    } else {
	visualTemplate.visualid = visualList[maxi].visualid;
	XFree((char *)visualList);
	visualList = XGetVisualInfo(dpy,VisualScreenMask|VisualIDMask,&visualTemplate,&count);
	return visualList;
    }
}

/*
** Query the Server GLX string and cache it in the display private.
** This routine will allocate the necessay space for the string.
*/
static char *QueryServerString( Display *dpy, int opcode,
                                        int screen, int name )
{
    xGLXQueryServerStringReq *req;
    xGLXQueryServerStringReply reply;
    int length, numbytes, slop;
    char *buf;

    /* Send the glXQueryServerString request */
    LockDisplay(dpy);
    GetReq(GLXQueryServerString,req);
    req->reqType = opcode;
    req->glxCode = X_GLXQueryServerString;
    req->screen = screen;
    req->name = name;
    _XReply(dpy, (xReply*) &reply, 0, False);

    length = reply.length;
    numbytes = reply.n;
    slop = numbytes * __GLX_SIZE_INT8 & 3;
    buf = (char *)Xmalloc(numbytes);
    if (!buf) {
        /* Throw data on the floor */
        _XEatData(dpy, length);
    } else {
        _XRead(dpy, (char *)buf, numbytes);
        if (slop) _XEatData(dpy,4-slop);
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return buf;
}

#define SEPARATOR " "

static char *combine_strings( const char *cext_string, const char *sext_string )
{
   int clen, slen;
   char *combo_string, *token, *s1;
   const char *s2, *end;

   /*
   ** String can't be longer than min(cstring, sstring)
   ** pull tokens out of shortest string
   ** include space in combo_string for final separator and null terminator
   */
   if ( (clen = strlen( cext_string)) > (slen = strlen( sext_string)) ) {
        combo_string = (char *) Xmalloc( slen + 2 );
	s1 = (char *) Xmalloc( slen + 2 ); strcpy( s1, sext_string );
        s2 = cext_string;
   } else {
        combo_string = (char *) Xmalloc( clen + 2 );
	s1 = (char *) Xmalloc( clen + 2 ); strcpy( s1, cext_string);
        s2 = sext_string;
   }
   if (!combo_string || !s1) {
	if (combo_string) Xfree(combo_string);
	if (s1) Xfree(s1);
        return NULL;
   }
   combo_string[0] = '\0';

   /* Get first extension token */
   token = strtok( s1, SEPARATOR);
   while ( token != NULL ) {

	/*
	** if token in second string then save it
	** beware of extension names which are prefixes of other extension names
	*/
	const char *p = s2;
	end = p + strlen(p);
	while (p < end) {
	    int n = strcspn(p, SEPARATOR);
	    if ((strlen(token) == n) && (strncmp(token, p, n) == 0)) {
		combo_string = strcat( combo_string, token);
		combo_string = strcat( combo_string, SEPARATOR);
	    }
	    p += (n + 1);
	}

        /* Get next extension token */
        token = strtok( NULL, SEPARATOR);
   }
   Xfree(s1);
   return combo_string;
}

const char *GLX_PREFIX(glXQueryExtensionsString)( Display *dpy, int screen )
{
    __GLXvisualConfig *pConfig;
    __GLXscreenConfigs *psc;
    __GLXdisplayPrivate *priv;

    /* Initialize the extension, if needed .  This has the added value
       of initializing/allocating the display private */
    priv = __glXInitialize(dpy);
    if (!priv) {
	return NULL;
    }

    /* Check screen number to see if its valid */
    if ((screen < 0) || (screen >= ScreenCount(dpy))) {
	return NULL;
    }

    /* Check to see if the GL is supported on this screen */
    psc = &priv->screenConfigs[screen];
    pConfig = psc->configs;
    if (!pConfig) {
	/* No support for GL on this screen regardless of visual */
	return NULL;
    }
   
    if (!psc->effectiveGLXexts) {
        if (!psc->serverGLXexts) {
	    psc->serverGLXexts = QueryServerString(dpy, priv->majorOpcode,
					  	   screen, GLX_EXTENSIONS);
	}
	psc->effectiveGLXexts = combine_strings(__glXGLXClientExtensions,
						psc->serverGLXexts);
    }

    return psc->effectiveGLXexts;
}

const char *GLX_PREFIX(glXGetClientString)( Display *dpy, int name )
{
    switch(name) {
	case GLX_VENDOR:
	    return (__glXGLXClientVendorName);
	case GLX_VERSION:
	    return (__glXGLXClientVersion);
	case GLX_EXTENSIONS:
	    return (__glXGLXClientExtensions);
	default:
	    return NULL;
    }
}

const char *GLX_PREFIX(glXQueryServerString)( Display *dpy, int screen, int name )
{
    __GLXvisualConfig *pConfig;
    __GLXscreenConfigs *psc;
    __GLXdisplayPrivate *priv;

    /* Initialize the extension, if needed .  This has the added value
       of initializing/allocating the display private */
    priv = __glXInitialize(dpy);
    if (!priv) {
	/* No extension */
	return NULL;
    }

    /* Check screen number to see if its valid */
    if ((screen < 0) || (screen >= ScreenCount(dpy))) {
	return NULL;
    }

    /* Check to see if the GL is supported on this screen */
    psc = &priv->screenConfigs[screen];
    pConfig = psc->configs;
    if (!pConfig) {
	/* No support for GL on this screen regardless of visual */
	return NULL;
    }
 
    switch(name) {
	case GLX_VENDOR:
	    if (!priv->serverGLXvendor) {
	 	priv->serverGLXvendor = 
			QueryServerString(dpy, priv->majorOpcode,
					  screen, GLX_VENDOR);
	    }
	    return(priv->serverGLXvendor);
	case GLX_VERSION:
	    if (!priv->serverGLXversion) {
	 	priv->serverGLXversion = 
			QueryServerString(dpy, priv->majorOpcode,
					  screen, GLX_VERSION);
	    }
	    return(priv->serverGLXversion);
	case GLX_EXTENSIONS:
	    if (!psc->serverGLXexts) {
	 	psc->serverGLXexts = 
			QueryServerString(dpy, priv->majorOpcode,
					  screen, GLX_EXTENSIONS);
	    }
	    return(psc->serverGLXexts);
	default:
	    return NULL;
    }
}

void __glXClientInfo (  Display *dpy, int opcode  )
{
    xGLXClientInfoReq *req;
    int size;

    /* Send the glXClientInfo request */
    LockDisplay(dpy);
    GetReq(GLXClientInfo,req);
    req->reqType = opcode;
    req->glxCode = X_GLXClientInfo;
    req->major = GLX_MAJOR_VERSION;
    req->minor = GLX_MINOR_VERSION;

    size = strlen(__glXGLClientExtensions) + 1;
    req->length += (size + 3) >> 2;
    req->numbytes = size;
    Data(dpy, __glXGLClientExtensions, size);

    UnlockDisplay(dpy);
    SyncHandle();
}

/************************************************************************/
/*
** EXT_import_context entry points
*/
/************************************************************************/

Display *glXGetCurrentDisplay(void)
{
    GLXContext gc = __glXGetCurrentContext();
    if (NULL == gc) return NULL;
    return gc->currentDpy;
}


Display *glXGetCurrentDisplayEXT(void)
{
    GLXContext gc = __glXGetCurrentContext();
    if (NULL == gc) return NULL;
    return gc->currentDpy;
}


static int __glXQueryContextInfo(Display *dpy, GLXContext ctx)
{
    xGLXVendorPrivateReq *vpreq;
    xGLXQueryContextInfoEXTReq *req;
    xGLXQueryContextInfoEXTReply reply;
    CARD8 opcode;
    GLuint numValues;
    int retval;

    if (ctx == NULL) {
	return GLX_BAD_CONTEXT;
    }
    opcode = __glXSetupForCommand(dpy);
    if (!opcode) {
	return 0;
    }

    /* Send the glXQueryContextInfoEXT request */
    LockDisplay(dpy);
    GetReqExtra(GLXVendorPrivate,
	sz_xGLXQueryContextInfoEXTReq-sz_xGLXVendorPrivateReq,vpreq);
    req = (xGLXQueryContextInfoEXTReq *)vpreq;
    req->reqType = opcode;
    req->glxCode = X_GLXVendorPrivateWithReply;
    req->vendorCode = X_GLXvop_QueryContextInfoEXT;
    req->context = (unsigned int)(ctx->xid);
    _XReply(dpy, (xReply*) &reply, 0, False);

    numValues = reply.n;
    if (numValues == 0)
	retval = Success;
    else if (numValues > __GLX_MAX_CONTEXT_PROPS)
	retval = 0;
    else
    {
	int *propList, *pProp;
	int nPropListBytes;
	int i;

	nPropListBytes = numValues << 3;
	propList = (int *) Xmalloc(nPropListBytes);
	if (NULL == propList) {
	    retval = 0;
	} else {
	    _XRead(dpy, (char *)propList, nPropListBytes);
	    pProp = propList;
	    for (i=0; i < numValues; i++) {
		switch (*pProp++) {
		case GLX_SHARE_CONTEXT_EXT:
		    ctx->share_xid = *pProp++;
		    break;
		case GLX_VISUAL_ID_EXT:
		    ctx->vid = *pProp++;
		    break;
		case GLX_SCREEN_EXT:
		    ctx->screen = *pProp++;
		    break;
		default:
		    pProp++;
		    continue;
		}
	    }
	    Xfree((char *)propList);
	    retval = Success;
	}
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return retval;
}

int GLX_PREFIX(glXQueryContextInfoEXT)(Display *dpy, GLXContext ctx, 
				int attribute, int *value)
{
    int retVal;

    /* get the information from the server if we don't have it already */
    if (!ctx->isDirect && (ctx->vid == None)) {
	retVal = __glXQueryContextInfo(dpy, ctx);
	if (Success != retVal) return retVal;
    }
    switch (attribute) {
    case GLX_SHARE_CONTEXT_EXT:
	*value = (int)(ctx->share_xid);
	break;
    case GLX_VISUAL_ID_EXT:
	*value = (int)(ctx->vid);
	break;
    case GLX_SCREEN_EXT:
	*value = (int)(ctx->screen);
	break;
    default:
	return GLX_BAD_ATTRIBUTE;
    }
    return Success;
}

GLXContextID glXGetContextIDEXT(const GLXContext ctx)
{
    return ctx->xid;
}

GLXContext GLX_PREFIX(glXImportContextEXT)(Display *dpy, GLXContextID contextID)
{
    GLXContext ctx;

    if (contextID == None) {
	return NULL;
    }
    if (__glXIsDirect(dpy, contextID)) {
	return NULL;
    }

    ctx = CreateContext(dpy, NULL, NULL, GL_FALSE, contextID);
    if (NULL != ctx) {
	if (Success != __glXQueryContextInfo(dpy, ctx)) {
	   return NULL;
	}
    }
    return ctx;
}

void GLX_PREFIX(glXFreeContextEXT)(Display *dpy, GLXContext ctx)
{
    DestroyContext(dpy, ctx);
}



/*
 * GLX 1.3 functions - these are just stubs for now!
 */

GLXFBConfig *GLX_PREFIX(glXChooseFBConfig)(Display *dpy, int screen, const int *attribList, int *nitems)
{
    (void) dpy;
    (void) screen;
    (void) attribList;
    (void) nitems;
    return 0;
}


GLXContext GLX_PREFIX(glXCreateNewContext)(Display *dpy, GLXFBConfig config, int renderType, GLXContext shareList, Bool direct)
{
    (void) dpy;
    (void) config;
    (void) renderType;
    (void) shareList;
    (void) direct;
    return 0;
}


GLXPbuffer GLX_PREFIX(glXCreatePbuffer)(Display *dpy, GLXFBConfig config, const int *attribList)
{
    (void) dpy;
    (void) config;
    (void) attribList;
    return 0;
}


GLXPixmap GLX_PREFIX(glXCreatePixmap)(Display *dpy, GLXFBConfig config, Pixmap pixmap, const int *attribList)
{
    (void) dpy;
    (void) config;
    (void) pixmap;
    (void) attribList;
    return 0;
}


GLXWindow GLX_PREFIX(glXCreateWindow)(Display *dpy, GLXFBConfig config, Window win, const int *attribList)
{
    (void) dpy;
    (void) config;
    (void) win;
    (void) attribList;
    return 0;
}


void GLX_PREFIX(glXDestroyPbuffer)(Display *dpy, GLXPbuffer pbuf)
{
    (void) dpy;
    (void) pbuf;
}


void GLX_PREFIX(glXDestroyPixmap)(Display *dpy, GLXPixmap pixmap)
{
    (void) dpy;
    (void) pixmap;
}


void GLX_PREFIX(glXDestroyWindow)(Display *dpy, GLXWindow window)
{
    (void) dpy;
    (void) window;
}


GLXDrawable glXGetCurrentReadDrawable(void)
{
    GLXContext gc = __glXGetCurrentContext();
    return gc->currentDrawable;
}


GLXFBConfig *GLX_PREFIX(glXGetFBConfigs)(Display *dpy, int screen, int *nelements)
{
   (void) dpy;
   (void) screen;
   (void) nelements;
   return 0;
}


int GLX_PREFIX(glXGetFBConfigAttrib)(Display *dpy, GLXFBConfig config, int attribute, int *value)
{
    (void) dpy;
    (void) config;
    (void) attribute;
    (void) value;
    return 0;
}


void GLX_PREFIX(glXGetSelectedEvent)(Display *dpy, GLXDrawable drawable, unsigned long *mask)
{
    (void) dpy;
    (void) drawable;
    (void) mask;
}


XVisualInfo *GLX_PREFIX(glXGetVisualFromFBConfig)(Display *dpy, GLXFBConfig config)
{
    (void) dpy;
    (void) config;
    return 0;
}


Bool GLX_PREFIX(glXMakeContextCurrent)(Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx)
{
    (void) dpy;
    (void) draw;
    (void) read;
    (void) ctx;
    return 0;
}


int GLX_PREFIX(glXQueryContext)(Display *dpy, GLXContext ctx, int attribute, int *value)
{
    (void) dpy;
    (void) ctx;
    (void) attribute;
    (void) value;
    return 0;
}


void GLX_PREFIX(glXQueryDrawable)(Display *dpy, GLXDrawable draw, int attribute, unsigned int *value)
{
    (void) dpy;
    (void) draw;
    (void) attribute;
    (void) value;
}


void GLX_PREFIX(glXSelectEvent)(Display *dpy, GLXDrawable drawable, unsigned long mask)
{
    (void) dpy;
    (void) drawable;
    (void) mask;
}


/*
** GLX_SGIS_make_current_read
*/
Bool GLX_PREFIX(glXMakeCurrentReadSGI)(Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx)
{
   (void) dpy;
   (void) draw;
   (void) read;
   (void) ctx;
   return False;
}

GLXDrawable glXGetCurrentReadDrawableSGI(void)
{
   return 0;
}


/*
** GLX_SGI_swap_control
*/
int GLX_PREFIX(glXSwapIntervalSGI)(int interval)
{
   (void) interval;
   return 0;
}


/*
** GLX_SGI_video_sync
*/
int GLX_PREFIX(glXGetVideoSyncSGI)(unsigned int *count)
{
   (void) count;
   return 0;
}

int GLX_PREFIX(glXWaitVideoSyncSGI)(int divisor, int remainder, unsigned int *count)
{
   (void) divisor;
   (void) remainder;
   (void) count;
   return 0;
}


/*
** GLX_SGIS_video_source
*/
#if defined(_VL_H)

GLXVideoSourceSGIX GLX_PREFIX(glXCreateGLXVideoSourceSGIX)(Display *dpy, int screen, VLServer server, VLPath path, int nodeClass, VLNode drainNode)
{
   (void) dpy;
   (void) screen;
   (void) server;
   (void) path;
   (void) nodeClass;
   (void) drainNode;
   return 0;
}

void GLX_PREFIX(glXDestroyGLXVideoSourceSGIX)(Display *dpy, GLXVideoSourceSGIX src)
{
   (void) dpy;
   (void) src;
}

#endif


/*
** GLX_SGIX_fbconfig
*/
int GLX_PREFIX(glXGetFBConfigAttribSGIX)(Display *dpy, GLXFBConfigSGIX config, int attribute, int *value)
{
   (void) dpy;
   (void) config;
   (void) attribute;
   (void) value;
   return 0;
}

GLXFBConfigSGIX * GLX_PREFIX(glXChooseFBConfigSGIX)(Display *dpy, int screen, int *attrib_list, int *nelements)
{
   (void) dpy;
   (void) screen;
   (void) attrib_list;
   (void) nelements;
   return 0;
}

GLXPixmap GLX_PREFIX(glXCreateGLXPixmapWithConfigSGIX)(Display *dpy, GLXFBConfigSGIX config, Pixmap pixmap)
{
   (void) dpy;
   (void) config;
   (void) pixmap;
   return 0;
}

GLXContext GLX_PREFIX(glXCreateContextWithConfigSGIX)(Display *dpy, GLXFBConfigSGIX config, int render_type, GLXContext share_list, Bool direct)
{
   (void) dpy;
   (void) config;
   (void) render_type;
   (void) share_list;
   (void) direct;
   return 0;
}

XVisualInfo * GLX_PREFIX(glXGetVisualFromFBConfigSGIX)(Display *dpy, GLXFBConfigSGIX config)
{
   (void) dpy;
   (void) config;
   return NULL;
}

GLXFBConfigSGIX GLX_PREFIX(glXGetFBConfigFromVisualSGIX)(Display *dpy, XVisualInfo *vis)
{
   (void) dpy;
   (void) vis;
   return 0;
}


/*
** GLX_SGIX_pbuffer
*/
GLXPbufferSGIX GLX_PREFIX(glXCreateGLXPbufferSGIX)(Display *dpy, GLXFBConfigSGIX config, unsigned int width, unsigned int height, int *attrib_list)
{
   (void) dpy;
   (void) config;
   (void) width;
   (void) height;
   (void) attrib_list;
   return 0;
}

void GLX_PREFIX(glXDestroyGLXPbufferSGIX)(Display *dpy, GLXPbufferSGIX pbuf)
{
   (void) dpy;
   (void) pbuf;
}

int GLX_PREFIX(glXQueryGLXPbufferSGIX)(Display *dpy, GLXPbufferSGIX pbuf, int attribute, unsigned int *value)
{
   (void) dpy;
   (void) pbuf;
   (void) attribute;
   (void) value;
   return 0;
}

void GLX_PREFIX(glXSelectEventSGIX)(Display *dpy, GLXDrawable drawable, unsigned long mask)
{
   (void) dpy;
   (void) drawable;
   (void) mask;
}

void GLX_PREFIX(glXGetSelectedEventSGIX)(Display *dpy, GLXDrawable drawable, unsigned long *mask)
{
   (void) dpy;
   (void) drawable;
   (void) mask;
}


/*
** GLX_SGI_cushion
*/
void GLX_PREFIX(glXCushionSGI)(Display *dpy, Window win, float cushion)
{
   (void) dpy;
   (void) win;
   (void) cushion;
}


/*
** GLX_SGIX_video_resize
*/
int GLX_PREFIX(glXBindChannelToWindowSGIX)(Display *dpy, int screen, int channel , Window window)
{
   (void) dpy;
   (void) screen;
   (void) channel;
   (void) window;
   return 0;
}

int GLX_PREFIX(glXChannelRectSGIX)(Display *dpy, int screen, int channel, int x, int y, int w, int h)
{
   (void) dpy;
   (void) screen;
   (void) channel;
   (void) x;
   (void) y;
   (void) w;
   (void) h;
   return 0;
}

int GLX_PREFIX(glXQueryChannelRectSGIX)(Display *dpy, int screen, int channel, int *x, int *y, int *w, int *h)
{
   (void) dpy;
   (void) screen;
   (void) channel;
   (void) x;
   (void) y;
   (void) w;
   (void) h;
   return 0;
}

int GLX_PREFIX(glXQueryChannelDeltasSGIX)(Display *dpy, int screen, int channel, int *dx, int *dy, int *dw, int *dh)
{
   (void) dpy;
   (void) screen;
   (void) channel;
   (void) dx;
   (void) dy;
   (void) dw;
   (void) dh;
   return 0;
}

int GLX_PREFIX(glXChannelRectSyncSGIX)(Display *dpy, int screen, int channel, GLenum synctype)
{
   (void) dpy;
   (void) screen;
   (void) channel;
   (void) synctype;
   return 0;
}


#if defined(_DM_BUFFER_H_)

Bool GLX_PREFIX(glXAssociateDMPbufferSGIX)(Display *dpy, GLXPbufferSGIX pbuffer, DMparams *params, DMbuffer dmbuffer)
{
   (void) dpy;
   (void) pbuffer;
   (void) params;
   (void) dmbuffer;
   return False;
}

#endif


/*
** GLX_SGIX_swap_group
*/
void GLX_PREFIX(glXJoinSwapGroupSGIX)(Display *dpy, GLXDrawable drawable, GLXDrawable member)
{
   (void) dpy;
   (void) drawable;
   (void) member;
}


/*
** GLX_SGIX_swap_barrier
*/
void GLX_PREFIX(glXBindSwapBarrierSGIX)(Display *dpy, GLXDrawable drawable, int barrier)
{
   (void) dpy;
   (void) drawable;
   (void) barrier;
}

Bool GLX_PREFIX(glXQueryMaxSwapBarriersSGIX)(Display *dpy, int screen, int *max)
{
   (void) dpy;
   (void) screen;
   (void) max;
   return False;
}


/*
** GLX_SUN_get_transparent_index
*/
Status GLX_PREFIX(glXGetTransparentIndexSUN)(Display *dpy, Window overlay, Window underlay, long *pTransparent)
{
   (void) dpy;
   (void) overlay;
   (void) underlay;
   (void) pTransparent;
   return 0;
}



/*
** Mesa extension stubs.  These will help reduce portability problems.
*/

Bool GLX_PREFIX(glXReleaseBuffersMESA)( Display *dpy, GLXDrawable d )
{
   (void) dpy;
   (void) d;
   return False;
}


GLXPixmap GLX_PREFIX(glXCreateGLXPixmapMESA)( Display *dpy,
                                              XVisualInfo *visual,
                                              Pixmap pixmap, Colormap cmap )
{
   (void) dpy;
   (void) visual;
   (void) pixmap;
   (void) cmap;
   return 0;
}


void GLX_PREFIX(glXCopySubBufferMESA)( Display *dpy, GLXDrawable drawable,
                                       int x, int y, int width, int height )
{
   (void) dpy;
   (void) drawable;
   (void) x;
   (void) y;
   (void) width;
   (void) height;
}


Bool GLX_PREFIX(glXSet3DfxModeMESA)( int mode )
{
   (void) mode;
   return GL_FALSE;
}



/* strdup() is actually not a standard ANSI C or POSIX routine.
 * Irix will not define it if ANSI mode is in effect.
 */
char *
__glXstrdup(const char *str)
{
   char *copy;
   copy = (char *) Xmalloc(strlen(str) + 1);
   if (!copy)
      return NULL;
   strcpy(copy, str);
   return copy;
}

/*
** glXGetProcAddress support
*/

struct name_address_pair {
   const char *Name;
   GLvoid *Address;
   struct name_address_pair *Next;
};

static struct name_address_pair GLX_functions[] = {
   /*** GLX_VERSION_1_0 ***/
   { "glXChooseVisual", (GLvoid *) glXChooseVisual, NULL },
   { "glXCopyContext", (GLvoid *) glXCopyContext, NULL },
   { "glXCreateContext", (GLvoid *) glXCreateContext, NULL },
   { "glXCreateGLXPixmap", (GLvoid *) glXCreateGLXPixmap, NULL },
   { "glXDestroyContext", (GLvoid *) glXDestroyContext, NULL },
   { "glXDestroyGLXPixmap", (GLvoid *) glXDestroyGLXPixmap, NULL },
   { "glXGetConfig", (GLvoid *) glXGetConfig, NULL },
   { "glXGetCurrentContext", (GLvoid *) glXGetCurrentContext, NULL },
   { "glXGetCurrentDrawable", (GLvoid *) glXGetCurrentDrawable, NULL },
   { "glXIsDirect", (GLvoid *) glXIsDirect, NULL },
   { "glXMakeCurrent", (GLvoid *) glXMakeCurrent, NULL },
   { "glXQueryExtension", (GLvoid *) glXQueryExtension, NULL },
   { "glXQueryVersion", (GLvoid *) glXQueryVersion, NULL },
   { "glXSwapBuffers", (GLvoid *) glXSwapBuffers, NULL },
   { "glXUseXFont", (GLvoid *) glXUseXFont, NULL },
   { "glXWaitGL", (GLvoid *) glXWaitGL, NULL },
   { "glXWaitX", (GLvoid *) glXWaitX, NULL },

   /*** GLX_VERSION_1_1 ***/
   { "glXGetClientString", (GLvoid *) glXGetClientString, NULL },
   { "glXQueryExtensionsString", (GLvoid *) glXQueryExtensionsString, NULL },
   { "glXQueryServerString", (GLvoid *) glXQueryServerString, NULL },

   /*** GLX_VERSION_1_2 ***/
   { "glXGetCurrentDisplay", (GLvoid *) glXGetCurrentDisplay, NULL },

   /*** GLX_VERSION_1_3 ***/
   { "glXChooseFBConfig", (GLvoid *) glXChooseFBConfig, NULL },
   { "glXCreateNewContext", (GLvoid *) glXCreateNewContext, NULL },
   { "glXCreatePbuffer", (GLvoid *) glXCreatePbuffer, NULL },
   { "glXCreatePixmap", (GLvoid *) glXCreatePixmap, NULL },
   { "glXCreateWindow", (GLvoid *) glXCreateWindow, NULL },
   { "glXDestroyPbuffer", (GLvoid *) glXDestroyPbuffer, NULL },
   { "glXDestroyPixmap", (GLvoid *) glXDestroyPixmap, NULL },
   { "glXDestroyWindow", (GLvoid *) glXDestroyWindow, NULL },
   { "glXGetCurrentReadDrawable", (GLvoid *) glXGetCurrentReadDrawable, NULL },
   { "glXGetFBConfigAttrib", (GLvoid *) glXGetFBConfigAttrib, NULL },
   { "glXGetFBConfigs", (GLvoid *) glXGetFBConfigs, NULL },
   { "glXGetSelectedEvent", (GLvoid *) glXGetSelectedEvent, NULL },
   { "glXGetVisualFromFBConfig", (GLvoid *) glXGetVisualFromFBConfig, NULL },
   { "glXMakeContextCurrent", (GLvoid *) glXMakeContextCurrent, NULL },
   { "glXQueryContext", (GLvoid *) glXQueryContext, NULL },
   { "glXQueryDrawable", (GLvoid *) glXQueryDrawable, NULL },
   { "glXSelectEvent", (GLvoid *) glXSelectEvent, NULL },

   /*** GLX_SGI_swap_control ***/
   { "glXSwapIntervalSGI", (GLvoid *) glXSwapIntervalSGI, NULL },

   /*** GLX_SGI_video_sync ***/
   { "glXGetVideoSyncSGI", (GLvoid *) glXGetVideoSyncSGI, NULL },
   { "glXWaitVideoSyncSGI", (GLvoid *) glXWaitVideoSyncSGI, NULL },

   /*** GLX_SGI_make_current_read ***/
   { "glXMakeCurrentReadSGI", (GLvoid *) glXMakeCurrentReadSGI, NULL },
   { "glXGetCurrentReadDrawableSGI", (GLvoid *) glXGetCurrentReadDrawableSGI, NULL },

   /*** GLX_SGIX_video_source ***/
#if defined(_VL_H)
   { "glXCreateGLXVideoSourceSGIX", (GLvoid *) glXCreateGLXVideoSourceSGIX, NULL },
   { "glXDestroyGLXVideoSourceSGIX", (GLvoid *) glXDestroyGLXVideoSourceSGIX, NULL },
#endif

   /*** GLX_EXT_import_context ***/
   { "glXFreeContextEXT", (GLvoid *) glXFreeContextEXT, NULL },
   { "glXGetContextIDEXT", (GLvoid *) glXGetContextIDEXT, NULL },
   { "glXGetCurrentDisplayEXT", (GLvoid *) glXGetCurrentDisplayEXT, NULL },
   { "glXImportContextEXT", (GLvoid *) glXImportContextEXT, NULL },
   { "glXQueryContextInfoEXT", (GLvoid *) glXQueryContextInfoEXT, NULL },

   /*** GLX_SGIX_fbconfig ***/
   { "glXGetFBConfigAttribSGIX", (GLvoid *) glXGetFBConfigAttribSGIX, NULL },
   { "glXChooseFBConfigSGIX", (GLvoid *) glXChooseFBConfigSGIX, NULL },
   { "glXCreateGLXPixmapWithConfigSGIX", (GLvoid *) glXCreateGLXPixmapWithConfigSGIX, NULL },
   { "glXCreateContextWithConfigSGIX", (GLvoid *) glXCreateContextWithConfigSGIX, NULL },
   { "glXGetVisualFromFBConfigSGIX", (GLvoid *) glXGetVisualFromFBConfigSGIX, NULL },
   { "glXGetFBConfigFromVisualSGIX", (GLvoid *) glXGetFBConfigFromVisualSGIX, NULL },

   /*** GLX_SGIX_pbuffer ***/
   { "glXCreateGLXPbufferSGIX", (GLvoid *) glXCreateGLXPbufferSGIX, NULL },
   { "glXDestroyGLXPbufferSGIX", (GLvoid *) glXDestroyGLXPbufferSGIX, NULL },
   { "glXQueryGLXPbufferSGIX", (GLvoid *) glXQueryGLXPbufferSGIX, NULL },
   { "glXSelectEventSGIX", (GLvoid *) glXSelectEventSGIX, NULL },
   { "glXGetSelectedEventSGIX", (GLvoid *) glXGetSelectedEventSGIX, NULL },

   /*** GLX_SGI_cushion ***/
   { "glXCushionSGI", (GLvoid *) glXCushionSGI, NULL },

   /*** GLX_SGIX_video_resize ***/
   { "glXBindChannelToWindowSGIX", (GLvoid *) glXBindChannelToWindowSGIX, NULL },
   { "glXChannelRectSGIX", (GLvoid *) glXChannelRectSGIX, NULL },
   { "glXQueryChannelRectSGIX", (GLvoid *) glXQueryChannelRectSGIX, NULL },
   { "glXQueryChannelDeltasSGIX", (GLvoid *) glXQueryChannelDeltasSGIX, NULL },
   { "glXChannelRectSyncSGIX", (GLvoid *) glXChannelRectSyncSGIX, NULL },

   /*** GLX_SGIX_dmbuffer **/
#if defined(_DM_BUFFER_H_)
   { "glXAssociateDMPbufferSGIX", (GLvoid *) glXAssociateDMPbufferSGIX, NULL },
#endif

   /*** GLX_SGIX_swap_group ***/
   { "glXJoinSwapGroupSGIX", (GLvoid *) glXJoinSwapGroupSGIX, NULL },

   /*** GLX_SGIX_swap_barrier ***/
   { "glXBindSwapBarrierSGIX", (GLvoid *) glXBindSwapBarrierSGIX, NULL },
   { "glXQueryMaxSwapBarriersSGIX", (GLvoid *) glXQueryMaxSwapBarriersSGIX, NULL },

   /*** GLX_SUN_get_transparent_index ***/
   { "glXGetTransparentIndexSUN", (GLvoid *) glXGetTransparentIndexSUN, NULL },

   /*** GLX_MESA_copy_sub_buffer ***/
   { "glXCopySubBufferMESA", (GLvoid *) glXCopySubBufferMESA, NULL },

   /*** GLX_MESA_pixmap_colormap ***/
   { "glXCreateGLXPixmapMESA", (GLvoid *) glXCreateGLXPixmapMESA, NULL },

   /*** GLX_MESA_release_buffers ***/
   { "glXReleaseBuffersMESA", (GLvoid *) glXReleaseBuffersMESA, NULL },

   /*** GLX_MESA_set_3dfx_mode ***/
   { "glXSet3DfxModeMESA", (GLvoid *) glXSet3DfxModeMESA, NULL },

   /*** GLX_ARB_get_proc_address ***/
   { "glXGetProcAddressARB", (GLvoid *) glXGetProcAddressARB, NULL },

   /*** GLX 1.4 ***/
   { "glXGetProcAddress", (GLvoid *) glXGetProcAddress, NULL },

   /*** GLX_???_allocate_memory ***/
   { "glXAllocateMemoryNV", (GLvoid *) glXAllocateMemoryNV, NULL },
   { "glXFreeMemoryNV", (GLvoid *) glXFreeMemoryNV, NULL },

   /*** GLX_MESA_agp_pointer ***/
   { "glXGetAGPOffsetMESA", (GLvoid *) glXGetAGPOffsetMESA, NULL },

   { NULL, NULL, NULL }   /* end of list */
};


static struct name_address_pair *Dynamic_GLX_functions = NULL;


/*
 * Drivers can call this function to append the name of a new GLX
 * extension string to __glXGLXClientExtensions.  Then, when the user
 * calls glXGetClientString() they'll see it listed.
 * This is a companion to __glXRegisterGLXFunction().
 */
void
__glXRegisterGLXExtensionString(const char *extName)
{
   char *newList;
   if (!extName)
      return;
   newList = Xmalloc(strlen(__glXGLXClientExtensions) +
                     strlen(extName) + 2); /* 2 for ' ' and '\0' */
   if (!newList)
      return;
   strcpy(newList, __glXGLXClientExtensions);
   strcat(newList, " ");
   strcat(newList, extName);
   if (__glXGLXClientExtensions != __glXGLXDefaultClientExtensions)
      Xfree((void *) __glXGLXClientExtensions);
   __glXGLXClientExtensions = newList;
}


/*
 * DRI drivers should call this function if they want to extend
 * the GLX API.  After registering a new GLX function, the user
 * can query and use it by calling glXGetProcAddress().
 * Input: funcName - name of new GLX function
 *        funcAddr - pointer to the function.
 * Return: address of previously registered function with this
 *         name, or NULL.
 */
void *
__glXRegisterGLXFunction(const char *funcName, void *funcAddr)
{
   struct name_address_pair *ext;

   /* look if the function is already registered */
   for (ext = Dynamic_GLX_functions; ext; ext = ext->Next) {
      if (strcmp(ext->Name, funcName) == 0) {
	 /* It's up the caller to use this return value if he wants
	  * to chain-call or wrap the previously registered function.
	  */
         void *prevAddr = ext->Address;
	 ext->Address = funcAddr;
	 return prevAddr;
      }
   }

   /* add new function */
   ext = Xmalloc(sizeof(struct name_address_pair));
   if (!ext)
      return NULL;
   ext->Name = __glXstrdup(funcName);
   if (!ext->Name) {
      Xfree(ext);
      return NULL;
   }
   ext->Address = funcAddr;
   ext->Next = Dynamic_GLX_functions;
   Dynamic_GLX_functions = ext;
   return NULL;
}


static const GLvoid *
get_glx_proc_address(const char *funcName)
{
   const struct name_address_pair *ext;
   GLuint i;

   /* try dynamic functions */
   for (ext = Dynamic_GLX_functions; ext; ext = ext->Next) {
      if (strcmp(ext->Name, funcName) == 0) {
	 return ext->Address;
      }
   }

   /* try static functions */
   for (i = 0; GLX_functions[i].Name; i++) {
      if (strcmp(GLX_functions[i].Name, funcName) == 0)
         return GLX_functions[i].Address;
   }
   return NULL;
}


#ifndef GLX_BUILT_IN_XMESA
void (*glXGetProcAddressARB(const GLubyte *procName))( void )
{
   typedef void (*gl_function)( void );
   gl_function f;

#if defined(GLX_DIRECT_RENDERING)
   __glXRegisterExtensions();
#endif

   f = (gl_function) get_glx_proc_address((const char *) procName);
   if (f) {
      return f;
   }

   f = (gl_function) _glapi_get_proc_address((const char *) procName);
   return f;
}

/* GLX 1.4 */
void (*glXGetProcAddress(const GLubyte *procName))( void )
{
   return glXGetProcAddressARB(procName);
}
#endif


/*
 * AGP memory allocation
 */
void *GLX_PREFIX(glXAllocateMemoryNV)(GLsizei size,
				      GLfloat readFrequency,
				      GLfloat writeFrequency,
				      GLfloat priority)
{
   /* This is special - search the list of dynamically-added functions
    * and call the allocator if present.
    * More typically, the user will have gotten a pointer to
    * glXAllocateMemoryNV() via glXGetProcAddress() so we won't be
    * doing this.
    */
   typedef void * (*allocFunc)(GLsizei size, GLfloat readFrequency, GLfloat writeFrequency, GLfloat priority);
   const struct name_address_pair *ext;
   static allocFunc f = (allocFunc) NULL;

   if (!f) {
      for (ext = Dynamic_GLX_functions; ext; ext = ext->Next) {
	 if (strcmp(ext->Name, "glXAllocateMemoryNV") == 0) {
	    f = (allocFunc) ext->Address;
	    break;
	 }
      }
   }
   if (f)
      return (*f)(size, readFrequency, writeFrequency, priority);
   return NULL;
}


void GLX_PREFIX(glXFreeMemoryNV)(GLvoid *pointer)
{
   /* This is special - search the list of dynamically-added functions
    * and call the free func if present.
    * More typically, the user will have gotten a pointer to
    * glXFreeMemoryNV() via glXGetProcAddress() so we won't be
    * doing this.
    */
   typedef void * (*freeFunc)(GLvoid *pointer);
   const struct name_address_pair *ext;
   static freeFunc f = (freeFunc) NULL;

   if (!f) {
      for (ext = Dynamic_GLX_functions; ext; ext = ext->Next) {
	 if (strcmp(ext->Name, "glXFreeMemoryNV") == 0) {
	    f = (freeFunc) ext->Address;
	    break;
	 }
      }
   }
   if (f)
      (*f)(pointer);
}


GLuint GLX_PREFIX(glXGetAGPOffsetMESA)( const GLvoid *pointer )
{
   /* This is special - search the list of dynamically-added functions
    * and call the free func if present.
    * More typically, the user will have gotten a pointer to
    * glXGetAGPOffsetMESA() via glXGetProcAddress() so we won't be
    * doing this.
    */
   typedef GLuint (*getAGPOffsetFunc)(const GLvoid *pointer);
   const struct name_address_pair *ext;
   static getAGPOffsetFunc f = (getAGPOffsetFunc) NULL;

   if (!f) {
      for (ext = Dynamic_GLX_functions; ext; ext = ext->Next) {
	 if (strcmp(ext->Name, "glXGetAGPOffsetMESA") == 0) {
	    f = (getAGPOffsetFunc) ext->Address;
	    break;
	 }
      }
   }
   if (f)
      return (*f)(pointer);
   else
      return ~0;
}
