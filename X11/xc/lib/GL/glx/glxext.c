/* $XFree86: xc/lib/GL/glx/glxext.c,v 1.16 2003/01/20 21:37:19 tsi Exp $ */

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

/*                                                            <
 * Direct rendering support added by Precision Insight, Inc.  <
 *                                                            <
 * Authors:                                                   <
 *   Kevin E. Martin <kevin@precisioninsight.com>             <
 *                                                            <
 */     

#include "packrender.h"
#include <stdio.h>
#include <Xext.h>
#include <extutil.h>
#include <assert.h>
#include "indirect_init.h"
#include "glapi.h"
#ifdef XTHREADS
#include "Xthreads.h"
#endif

#ifdef DEBUG
void __glXDumpDrawBuffer(__GLXcontext *ctx);
#endif

/*
** We setup some dummy structures here so that the API can be used
** even if no context is current.
*/

static GLubyte dummyBuffer[__GLX_BUFFER_LIMIT_SIZE];

/*
** Dummy context used by small commands when there is no current context.
** All the
** gl and glx entry points are designed to operate as nop's when using
** the dummy context structure.
*/
static __GLXcontext dummyContext = {
    &dummyBuffer[0],
    &dummyBuffer[0],
    &dummyBuffer[0],
    &dummyBuffer[__GLX_BUFFER_LIMIT_SIZE],
    sizeof(dummyBuffer),
};


/*
** All indirect rendering contexts will share the same indirect dispatch table.
*/
static __GLapi *IndirectAPI = NULL;


/*
 * Current context management and locking
 */

#if defined(GLX_DIRECT_RENDERING) && defined(XTHREADS)

/* thread safe */
static GLboolean TSDinitialized = GL_FALSE;
static xthread_key_t ContextTSD;

__GLXcontext *__glXGetCurrentContext(void)
{
   if (!TSDinitialized) {
      xthread_key_create(&ContextTSD, NULL);
      TSDinitialized = GL_TRUE;
      return &dummyContext;
   }
   else {
      void *p;
      xthread_get_specific(ContextTSD, &p);
      if (!p)
         return &dummyContext;
      else
         return (__GLXcontext *) p;
   }
}

void __glXSetCurrentContext(__GLXcontext *c)
{
   if (!TSDinitialized) {
      xthread_key_create(&ContextTSD, NULL);
      TSDinitialized = GL_TRUE;
   }
   xthread_set_specific(ContextTSD, c);
}


/* Used by the __glXLock() and __glXUnlock() macros */
xmutex_rec __glXmutex;

#else

/* not thread safe */
__GLXcontext *__glXcurrentContext = &dummyContext;

#endif


/*
** You can set this cell to 1 to force the gl drawing stuff to be
** one command per packet
*/
int __glXDebug = 0;

/*
** forward prototype declarations
*/
int __glXCloseDisplay(Display *dpy, XExtCodes *codes);

/************************************************************************/

/* Extension required boiler plate */

static char *__glXExtensionName = GLX_EXTENSION_NAME;
XExtensionInfo *__glXExtensionInfo = NULL;

static /* const */ char *error_list[] = {
    "GLXBadContext",
    "GLXBadContextState",
    "GLXBadDrawable",
    "GLXBadPixmap",
    "GLXBadContextTag",
    "GLXBadCurrentWindow",
    "GLXBadRenderRequest",
    "GLXBadLargeRequest",
    "GLXUnsupportedPrivateRequest",
};

int __glXCloseDisplay(Display *dpy, XExtCodes *codes)
{
  GLXContext gc;

  gc = __glXGetCurrentContext();
  if (dpy == gc->currentDpy) {
    __glXSetCurrentContext(&dummyContext);
#ifdef GLX_DIRECT_RENDERING
    _glapi_set_dispatch(NULL);  /* no-op functions */
#endif
    __glXFreeContext(gc);
  }

  return XextRemoveDisplay(__glXExtensionInfo, dpy);
}


static XEXT_GENERATE_ERROR_STRING(__glXErrorString, __glXExtensionName,
				  __GLX_NUMBER_ERRORS, error_list)

static /* const */ XExtensionHooks __glXExtensionHooks = {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    __glXCloseDisplay,			/* close_display */
    NULL,				/* wire_to_event */
    NULL,				/* event_to_wire */
    NULL,				/* error */
    __glXErrorString,			/* error_string */
};

static
XEXT_GENERATE_FIND_DISPLAY(__glXFindDisplay, __glXExtensionInfo,
			   __glXExtensionName, &__glXExtensionHooks,
			   __GLX_NUMBER_EVENTS, NULL)

/************************************************************************/

/*
** Free the per screen configs data as well as the array of
** __glXScreenConfigs.
*/
static void FreeScreenConfigs(__GLXdisplayPrivate *priv)
{
    __GLXscreenConfigs *psc;
    GLint i, screens;

    /* Free screen configuration information */
    psc = priv->screenConfigs;
    screens = ScreenCount(priv->dpy);
    for (i = 0; i < screens; i++, psc++) {
	if (psc->configs) {
	    Xfree((char*) psc->configs);
	    if(psc->effectiveGLXexts)
		Xfree(psc->effectiveGLXexts);
	    psc->configs = 0;	/* NOTE: just for paranoia */
	}

#ifdef GLX_DIRECT_RENDERING
	/* Free the direct rendering per screen data */
	if (psc->driScreen.private)
	    (*psc->driScreen.destroyScreen)(priv->dpy, i,
					    psc->driScreen.private);
	psc->driScreen.private = NULL;
#endif
    }
    XFree((char*) priv->screenConfigs);
}

/*
** Release the private memory referred to in a display private
** structure.  The caller will free the extension structure.
*/
static int __glXFreeDisplayPrivate(XExtData *extension)
{
    __GLXdisplayPrivate *priv;

    priv = (__GLXdisplayPrivate*) extension->private_data;
    FreeScreenConfigs(priv);
    if(priv->serverGLXvendor) {
	Xfree((char*)priv->serverGLXvendor);
	priv->serverGLXvendor = 0x0; /* to protect against double free's */
    }
    if(priv->serverGLXversion) {
	Xfree((char*)priv->serverGLXversion);
	priv->serverGLXversion = 0x0; /* to protect against double free's */
    }

#if 0 /* GLX_DIRECT_RENDERING */
    /* Free the direct rendering per display data */
    if (priv->driDisplay.private)
	(*priv->driDisplay.destroyDisplay)(priv->dpy,
					   priv->driDisplay.private);
    priv->driDisplay.private = NULL;
#endif

#ifdef GLX_DIRECT_RENDERING
    XFree(priv->driDisplay.createScreen);
#endif

    Xfree((char*) priv);
    return 0;
}

/************************************************************************/

/*
** Query the version of the GLX extension.  This procedure works even if
** the client extension is not completely set up.
*/
static Bool QueryVersion(Display *dpy, int opcode, int *major, int *minor)
{
    xGLXQueryVersionReq *req;
    xGLXQueryVersionReply reply;

    /* Send the glXQueryVersion request */
    LockDisplay(dpy);
    GetReq(GLXQueryVersion,req);
    req->reqType = opcode;
    req->glxCode = X_GLXQueryVersion;
    req->majorVersion = GLX_MAJOR_VERSION;
    req->minorVersion = GLX_MINOR_VERSION;
    _XReply(dpy, (xReply*) &reply, 0, False);
    UnlockDisplay(dpy);
    SyncHandle();

    if (reply.majorVersion != GLX_MAJOR_VERSION) {
	/*
	** The server does not support the same major release as this
	** client.
	*/
	return GL_FALSE;
    }
    *major = reply.majorVersion;
    *minor = min(reply.minorVersion, GLX_MINOR_VERSION);
    return GL_TRUE;
}

/*
** Allocate the memory for the per screen configs for each screen.
** If that works then fetch the per screen configs data.
*/
static Bool AllocAndFetchScreenConfigs(Display *dpy, __GLXdisplayPrivate *priv)
{
    xGLXGetVisualConfigsReq *req;
    xGLXGetVisualConfigsReply reply;
    __GLXscreenConfigs *psc;
    __GLXvisualConfig *config;
    GLint i, j, k, nprops, screens;
    INT32 buf[__GLX_TOTAL_CONFIG], *props;

    /*
    ** First allocate memory for the array of per screen configs.
    */
    screens = ScreenCount(dpy);
    psc = (__GLXscreenConfigs*) Xmalloc(screens * sizeof(__GLXscreenConfigs));
    if (!psc) {
	return GL_FALSE;
    }
    memset(psc, 0, screens * sizeof(__GLXscreenConfigs));
    priv->screenConfigs = psc;
    
    /*
    ** Now fetch each screens configs structures.  If a screen supports
    ** GL (by returning a numVisuals > 0) then allocate memory for our
    ** config structure and then fill it in.
    */
    for (i = 0; i < screens; i++, psc++) {
	/* Send the glXGetVisualConfigs request */
	LockDisplay(dpy);
	GetReq(GLXGetVisualConfigs,req);
	req->reqType = priv->majorOpcode;
	req->glxCode = X_GLXGetVisualConfigs;
	req->screen = i;
	if (!_XReply(dpy, (xReply*) &reply, 0, False)) {
	    /* Something is busted. Punt. */
	    UnlockDisplay(dpy);
	    SyncHandle();
	    FreeScreenConfigs(priv);
	    return GL_FALSE;
	}
	if (!reply.numVisuals) {
	    /* This screen does not support GL rendering */
	    UnlockDisplay(dpy);
	    continue;
	}

	/* Check number of properties */
	nprops = reply.numProps;
	if ((nprops < __GLX_MIN_CONFIG_PROPS) ||
	    (nprops > __GLX_MAX_CONFIG_PROPS)) {
	    /* Huh?  Not in protocol defined limits.  Punt */
	    UnlockDisplay(dpy);
	    SyncHandle();
	    FreeScreenConfigs(priv);
	    return GL_FALSE;
	}

	/* Allocate memory for our config structure */
	psc->configs = (__GLXvisualConfig*)
	    Xmalloc(reply.numVisuals * sizeof(__GLXvisualConfig));
	psc->numConfigs = reply.numVisuals;
	if (!psc->configs) {
	    UnlockDisplay(dpy);
	    SyncHandle();
	    FreeScreenConfigs(priv);
	    return GL_FALSE;
	}
	/* Allocate memory for the properties, if needed */
	if (nprops <= __GLX_MIN_CONFIG_PROPS) {
	    props = buf;
	} else {
	    props = (INT32 *) Xmalloc(nprops * __GLX_SIZE_INT32);
	} 

	/* Read each config structure and convert it into our format */
	config = psc->configs;
	for (j = 0; j < reply.numVisuals; j++, config++) {
	    INT32 *bp = props;

	    _XRead(dpy, (char *)bp, nprops * __GLX_SIZE_INT32);

	    /* Copy in the first set of properties */
	    config->vid = *bp++;
	    config->class = *bp++;
	    config->rgba = *bp++;

	    config->redSize = *bp++;
	    config->greenSize = *bp++;
	    config->blueSize = *bp++;
	    config->alphaSize = *bp++;
	    config->accumRedSize = *bp++;
	    config->accumGreenSize = *bp++;
	    config->accumBlueSize = *bp++;
	    config->accumAlphaSize = *bp++;

	    config->doubleBuffer = *bp++;
	    config->stereo = *bp++;

	    config->bufferSize = *bp++;
	    config->depthSize = *bp++;
	    config->stencilSize = *bp++;
	    config->auxBuffers = *bp++;
	    config->level = *bp++;

	    /*
	    ** Additional properties may be in a list at the end
	    ** of the reply.  They are in pairs of property type
	    ** and property value.
	    */
	    config->visualRating = GLX_NONE_EXT;
	    config->transparentPixel = GL_FALSE;

	    for (k = __GLX_MIN_CONFIG_PROPS; k < nprops; k+=2) {
		switch(*bp++) {
		  case GLX_VISUAL_CAVEAT_EXT:
		    config->visualRating = *bp++;    
		    break;
		  case GLX_TRANSPARENT_TYPE_EXT:
		    config->transparentPixel = *bp++;    
		    break;
		  case GLX_TRANSPARENT_INDEX_VALUE_EXT:
		    config->transparentIndex = *bp++;    
		    break;
		  case GLX_TRANSPARENT_RED_VALUE_EXT:
		    config->transparentRed = *bp++;    
		    break;
		  case GLX_TRANSPARENT_GREEN_VALUE_EXT:
		    config->transparentGreen = *bp++;    
		    break;
		  case GLX_TRANSPARENT_BLUE_VALUE_EXT:
		    config->transparentBlue = *bp++;    
		    break;
		  case GLX_TRANSPARENT_ALPHA_VALUE_EXT:
		    config->transparentAlpha = *bp++;    
		    break;
		}
	    }
	}
	if (props != buf) {
	    Xfree((char *)props);
	}
	UnlockDisplay(dpy);

#ifdef GLX_DIRECT_RENDERING
	/* Initialize the direct rendering per screen data and functions */
	if (priv->driDisplay.private &&
		priv->driDisplay.createScreen &&
		priv->driDisplay.createScreen[i]) {
	    psc->driScreen.private =
		(*(priv->driDisplay.createScreen[i]))(dpy, i, &psc->driScreen,
						 psc->numConfigs,
						 psc->configs);
	}
#endif
    }
    SyncHandle();
    return GL_TRUE;
}

/*
** Initialize the client side extension code.
*/
__GLXdisplayPrivate *__glXInitialize(Display* dpy)
{
    XExtDisplayInfo *info = __glXFindDisplay(dpy);
    XExtData **privList, *private, *found;
    __GLXdisplayPrivate *dpyPriv;
    XEDataObject dataObj;
    int major, minor;

#if defined(GLX_DIRECT_RENDERING) && defined(XTHREADS)
    {
        static int firstCall = 1;
        if (firstCall) {
            /* initialize the GLX mutexes */
            xmutex_init(&__glXmutex);
            firstCall = 0;
        }
    }
#endif

    /* The one and only long long lock */
    __glXLock();

    if (!XextHasExtension(info)) {
	/* No GLX extension supported by this server. Oh well. */
	__glXUnlock();
	XMissingExtension(dpy, __glXExtensionName);
	return 0;
    }

    /* See if a display private already exists.  If so, return it */
    dataObj.display = dpy;
    privList = XEHeadOfExtensionList(dataObj);
    found = XFindOnExtensionList(privList, info->codes->extension);
    if (found) {
	__glXUnlock();
	return (__GLXdisplayPrivate *) found->private_data;
    }

    /* See if the versions are compatible */
    if (!QueryVersion(dpy, info->codes->major_opcode, &major, &minor)) {
	/* The client and server do not agree on versions.  Punt. */
	__glXUnlock();
	return 0;
    }

    /*
    ** Allocate memory for all the pieces needed for this buffer.
    */
    private = (XExtData *) Xmalloc(sizeof(XExtData));
    if (!private) {
	__glXUnlock();
	return 0;
    }
    dpyPriv = (__GLXdisplayPrivate *) Xmalloc(sizeof(__GLXdisplayPrivate));
    if (!dpyPriv) {
	__glXUnlock();
	Xfree((char*) private);
	return 0;
    }

    /*
    ** Init the display private and then read in the screen config
    ** structures from the server.
    */
    dpyPriv->majorOpcode = info->codes->major_opcode;
    dpyPriv->majorVersion = major;
    dpyPriv->minorVersion = minor;
    dpyPriv->dpy = dpy;

    dpyPriv->serverGLXvendor = 0x0; 
    dpyPriv->serverGLXversion = 0x0;

#ifdef GLX_DIRECT_RENDERING
    /*
    ** Initialize the direct rendering per display data and functions.
    ** Note: This _must_ be done before calling any other DRI routines
    ** (e.g., those called in AllocAndFetchScreenConfigs).
    */
    if (getenv("LIBGL_ALWAYS_INDIRECT")) {
        /* Assinging zero here assures we'll never go direct */
        dpyPriv->driDisplay.private = 0;
        dpyPriv->driDisplay.destroyDisplay = 0;
        dpyPriv->driDisplay.createScreen = 0;
    }
    else {
        dpyPriv->driDisplay.private =
            driCreateDisplay(dpy, &dpyPriv->driDisplay);
    }
#endif

    if (!AllocAndFetchScreenConfigs(dpy, dpyPriv)) {
	__glXUnlock();
	Xfree((char*) dpyPriv);
	Xfree((char*) private);
	return 0;
    }

    /*
    ** Fill in the private structure.  This is the actual structure that
    ** hangs off of the Display structure.  Our private structure is
    ** referred to by this structure.  Got that?
    */
    private->number = info->codes->extension;
    private->next = 0;
    private->free_private = __glXFreeDisplayPrivate;
    private->private_data = (char *) dpyPriv;
    XAddToExtensionList(privList, private);

    if (dpyPriv->majorVersion == 1 && dpyPriv->minorVersion >= 1) {
        __glXClientInfo(dpy, dpyPriv->majorOpcode);
    }
    __glXUnlock();

    return dpyPriv;
}

/*
** Setup for sending a GLX command on dpy.  Make sure the extension is
** initialized.  Try to avoid calling __glXInitialize as its kinda slow.
*/
CARD8 __glXSetupForCommand(Display *dpy)
{
    GLXContext gc;
    __GLXdisplayPrivate *priv;

    /* If this thread has a current context, flush its rendering commands */
    gc = __glXGetCurrentContext();
    if (gc->currentDpy) {
	/* Flush rendering buffer of the current context, if any */
	(void) __glXFlushRenderBuffer(gc, gc->pc);

	if (gc->currentDpy == dpy) {
	    /* Use opcode from gc because its right */
	    return gc->majorOpcode;
	} else {
	    /*
	    ** Have to get info about argument dpy because it might be to
	    ** a different server
	    */
	}
    }

    /* Forced to lookup extension via the slow initialize route */
    priv = __glXInitialize(dpy);
    if (!priv) {
	return 0;
    }
    return priv->majorOpcode;
}

/*
** Flush the drawing command transport buffer.
*/
GLubyte *__glXFlushRenderBuffer(__GLXcontext *ctx, GLubyte *pc)
{
    Display *dpy;
    xGLXRenderReq *req;
    GLint size;

    if (!(dpy = ctx->currentDpy)) {
	/* Using the dummy context */
	ctx->pc = ctx->buf;
	return ctx->pc;
    }

    size = pc - ctx->buf;
    if (size) {
	/* Send the entire buffer as an X request */
	LockDisplay(dpy);
	GetReq(GLXRender,req); 
	req->reqType = ctx->majorOpcode;
	req->glxCode = X_GLXRender; 
	req->contextTag = ctx->currentContextTag;
	req->length += (size + 3) >> 2;
	_XSend(dpy, (char *)ctx->buf, size);
	UnlockDisplay(dpy);
	SyncHandle();
    }

    /* Reset pointer and return it */
    ctx->pc = ctx->buf;
    return ctx->pc;
}

/*
** Send a large command, one that is too large for some reason to
** send using the GLXRender protocol request.  One reason to send
** a large command is to avoid copying the data.
*/
void __glXSendLargeCommand(__GLXcontext *ctx,
			   const GLvoid *header, GLint headerLen,
			   const GLvoid *data, GLint dataLen)
{
    Display *dpy = ctx->currentDpy;
    xGLXRenderLargeReq *req;
    GLint maxSize, amount;
    GLint totalRequests, requestNumber;

    maxSize = ctx->bufSize - sizeof(xGLXRenderLargeReq);
    totalRequests = 1 + (dataLen / maxSize);
    if (dataLen % maxSize) totalRequests++;

    /*
    ** Send all of the command, except the large array, as one request.
    */
    LockDisplay(dpy);
    GetReq(GLXRenderLarge,req); 
    req->reqType = ctx->majorOpcode;
    req->glxCode = X_GLXRenderLarge; 
    req->contextTag = ctx->currentContextTag;
    req->length += (headerLen + 3) >> 2;
    req->requestNumber = 1;
    req->requestTotal = totalRequests;
    req->dataBytes = headerLen;
    Data(dpy, (const void *)header, headerLen);

    /*
    ** Send enough requests until the whole array is sent.
    */
    requestNumber = 2;
    while (dataLen > 0) {
	amount = dataLen;
	if (amount > maxSize) {
	    amount = maxSize;
	}
	GetReq(GLXRenderLarge,req); 
	req->reqType = ctx->majorOpcode;
	req->glxCode = X_GLXRenderLarge; 
	req->contextTag = ctx->currentContextTag;
	req->length += (amount + 3) >> 2;
	req->requestNumber = requestNumber++;
	req->requestTotal = totalRequests;
	req->dataBytes = amount;
	Data(dpy, (const void *)data, amount);
	dataLen -= amount;
	data = ((const char*) data) + amount;
    }
    UnlockDisplay(dpy);
    SyncHandle();
}

/************************************************************************/

GLXContext glXGetCurrentContext(void)
{
    GLXContext cx = __glXGetCurrentContext();
    
    if (cx == &dummyContext) {
	return NULL;
    } else {
	return cx;
    }
}

GLXDrawable glXGetCurrentDrawable(void)
{
    GLXContext gc = __glXGetCurrentContext();
    return gc->currentDrawable;
}


/************************************************************************/

#ifdef GLX_DIRECT_RENDERING
/* Return the DRI per screen structure */
__DRIscreen *__glXFindDRIScreen(Display *dpy, int scrn)
{
    __DRIscreen *pDRIScreen = NULL;
    XExtDisplayInfo *info = __glXFindDisplay(dpy);
    XExtData **privList, *found;
    __GLXdisplayPrivate *dpyPriv;
    XEDataObject dataObj;

    __glXLock();
    dataObj.display = dpy;
    privList = XEHeadOfExtensionList(dataObj);
    found = XFindOnExtensionList(privList, info->codes->extension);
    __glXUnlock();

    if (found) {
	dpyPriv = (__GLXdisplayPrivate *)found->private_data;
	pDRIScreen = &dpyPriv->screenConfigs[scrn].driScreen;
    }

    return pDRIScreen;
}
#endif

/************************************************************************/

/*
** Make a particular context current.
** NOTE: this is in this file so that it can access dummyContext.
*/
Bool GLX_PREFIX(glXMakeCurrent)(Display *dpy, GLXDrawable draw, GLXContext gc)
{
    xGLXMakeCurrentReq *req;
    xGLXMakeCurrentReply reply;
    GLXContext oldGC;
    CARD8 opcode, oldOpcode;
    Display *dpyTmp;
    Bool sentRequestToOldDpy = False;
    Bool bindReturnValue = True;

    opcode = __glXSetupForCommand(dpy);
    if (!opcode) {
	return GL_FALSE;
    }
    /*
    ** Make sure that the new context has a nonzero ID.  In the request,
    ** a zero context ID is used only to mean that we bind to no current
    ** context.
    */
    if ((gc != NULL) && (gc->xid == None)) {
	return GL_FALSE;
    }

    oldGC = __glXGetCurrentContext();
    if ((dpy != oldGC->currentDpy || (gc && gc->isDirect)) &&
	!oldGC->isDirect && oldGC != &dummyContext) {
	/*
	** We are either switching from one dpy to another and have to
	** send a request to the previous dpy to unbind the previous
	** context, or we are switching away from a indirect context to
	** a direct context and have to send a request to the dpy to
	** unbind the previous context.
	*/
	sentRequestToOldDpy = True;
	dpyTmp = dpy;

	if (dpy != oldGC->currentDpy) {
	    /*
	    ** The GetReq macro uses "dpy", so we have to save and
	    ** restore later.
	    */
	    dpy = oldGC->currentDpy;
	    oldOpcode = __glXSetupForCommand(dpy);
	    if (!oldOpcode) {
		return GL_FALSE;
	    }
	} else {
	    oldOpcode = opcode;
	}
	LockDisplay(dpy);
	GetReq(GLXMakeCurrent,req);
	req->reqType = oldOpcode;
	req->glxCode = X_GLXMakeCurrent;
	req->drawable = None;
	req->context = None;
	req->oldContextTag = oldGC->currentContextTag;
	if (!_XReply(dpy, (xReply*) &reply, 0, False)) {
	    /* The make current failed.  Just return GL_FALSE. */
	    UnlockDisplay(dpy);
	    SyncHandle();
	    return GL_FALSE;
	}
	UnlockDisplay(dpy);
	dpy = dpyTmp;
	oldGC->currentContextTag = 0;
    }
    
#ifdef GLX_DIRECT_RENDERING
    /* Unbind the old direct rendering context */
    if (oldGC->isDirect) {
	if (oldGC->driContext.private) {
	    int will_rebind = (gc && gc->isDirect
			       && draw == oldGC->currentDrawable);
	    if (!(*oldGC->driContext.unbindContext)(oldGC->currentDpy,
						    oldGC->screen,
						    oldGC->currentDrawable,
						    oldGC,
						    will_rebind)) {
		/* The make current failed.  Just return GL_FALSE. */
		return GL_FALSE;
	    }
	}
	oldGC->currentContextTag = 0;
    }

    /* Bind the direct rendering context to the drawable */
    if (gc && gc->isDirect) {
	if (gc->driContext.private) {
	    bindReturnValue =
		(*gc->driContext.bindContext)(dpy, gc->screen, draw, gc);
	}
    } else {
#endif
        _glapi_check_multithread();
	/* Send a glXMakeCurrent request to bind the new context. */
	LockDisplay(dpy);
	GetReq(GLXMakeCurrent,req);
	req->reqType = opcode;
	req->glxCode = X_GLXMakeCurrent;
	req->drawable = draw;
	req->context = gc ? gc->xid : None;
	req->oldContextTag = oldGC->currentContextTag;
	bindReturnValue = _XReply(dpy, (xReply*) &reply, 0, False);
        UnlockDisplay(dpy);
#ifdef GLX_DIRECT_RENDERING
    }
#endif


    if (!bindReturnValue) {
	/* The make current failed. */
	if (gc && !gc->isDirect) {
	    SyncHandle();
	}

#ifdef GLX_DIRECT_RENDERING
	/* If the old context was direct rendering, then re-bind to it. */
	if (oldGC->isDirect) {
	    if (oldGC->driContext.private) {
		if (!(*oldGC->driContext.bindContext)(oldGC->currentDpy,
						      oldGC->screen,
						      oldGC->currentDrawable,
						      oldGC)) {
		    /*
		    ** The request failed; this cannot happen with the
		    ** current API.  If in the future the API is
		    ** extended to allow context sharing between
		    ** clients, then this may fail (because another
		    ** client may have grabbed the context); in that
		    ** case, we cannot undo the previous request, and
		    ** cannot adhere to the "no-op" behavior.
		    */
		}
	    }
	} else
#endif
	/*
	** If we had just sent a request to a previous dpy, we have to
	** undo that request (because if a command fails, it should act
	** like a no-op) by making current to the previous context and
	** drawable.
	*/
	if (sentRequestToOldDpy) {
	    if (dpy != oldGC->currentDpy) {
		dpy = oldGC->currentDpy;
		oldOpcode = __glXSetupForCommand(dpy);
	    } else {
		oldOpcode = opcode;
	    }
	    LockDisplay(dpy);
	    GetReq(GLXMakeCurrent,req);
	    req->reqType = oldOpcode;
	    req->glxCode = X_GLXMakeCurrent;
	    req->drawable = oldGC->currentDrawable;
	    req->context = oldGC->xid;
	    req->oldContextTag = 0;
	    if (!_XReply(dpy, (xReply*) &reply, 0, False)) {
		UnlockDisplay(dpy);
		SyncHandle();
		/*
		** The request failed; this cannot happen with the
		** current API.  If in the future the API is extended to
		** allow context sharing between clients, then this may
		** fail (because another client may have grabbed the
		** context); in that case, we cannot undo the previous
		** request, and cannot adhere to the "no-op" behavior.
		*/
	    }
            else {
		UnlockDisplay(dpy);
            }
	    oldGC->currentContextTag = reply.contextTag;
	}
	return GL_FALSE;
    }

    /* Update our notion of what is current */
    __glXLock();
    if (gc == oldGC) {
	/*
	** Even though the contexts are the same the drawable might have
	** changed.  Note that gc cannot be the dummy, and that oldGC
	** cannot be NULL, therefore if they are the same, gc is not
	** NULL and not the dummy.
	*/
	gc->currentDrawable = draw;
    } else {
	if (oldGC != &dummyContext) {
	    /* Old current context is no longer current to anybody */
	    oldGC->currentDpy = 0;
	    oldGC->currentDrawable = None;
	    oldGC->currentContextTag = 0;

	    if (oldGC->xid == None) {
		/* 
		** We are switching away from a context that was
		** previously destroyed, so we need to free the memory
		** for the old handle.
		*/
#ifdef GLX_DIRECT_RENDERING
		/* Destroy the old direct rendering context */
		if (oldGC->isDirect) {
		    if (oldGC->driContext.private) {
			(*oldGC->driContext.destroyContext)
			    (dpy, oldGC->screen, oldGC->driContext.private);
			oldGC->driContext.private = NULL;
		    }
		}
#endif
		__glXFreeContext(oldGC);
	    }
	}
	if (gc) {
	    __glXSetCurrentContext(gc);
#ifdef GLX_DIRECT_RENDERING
            if (!gc->isDirect) {
               if (!IndirectAPI)
                  IndirectAPI = __glXNewIndirectAPI();
               _glapi_set_dispatch(IndirectAPI);
# ifdef GLX_USE_APPLEGL
	       do {
		   extern void XAppleDRIUseIndirectDispatch (void);
		   XAppleDRIUseIndirectDispatch ();
	       } while (0);
# endif
            }
#else
            /* if not direct rendering, always need indirect dispatch */
            if (!IndirectAPI)
               IndirectAPI = __glXNewIndirectAPI();
            _glapi_set_dispatch(IndirectAPI);
#endif
	    gc->currentDpy = dpy;
	    gc->currentDrawable = draw;
#ifdef GLX_DIRECT_RENDERING
	    if (gc->isDirect) reply.contextTag = -1;
#endif
	    gc->currentContextTag = reply.contextTag;
	} else {
	    __glXSetCurrentContext(&dummyContext);
#ifdef GLX_DIRECT_RENDERING
            _glapi_set_dispatch(NULL);  /* no-op functions */
#endif
	}
    }
    __glXUnlock();
    return GL_TRUE;
}

#ifdef DEBUG
void __glXDumpDrawBuffer(__GLXcontext *ctx)
{
    GLubyte *p = ctx->buf;
    GLubyte *end = ctx->pc;
    GLushort opcode, length;

    while (p < end) {
	/* Fetch opcode */
	opcode = *((GLushort*) p);
	length = *((GLushort*) (p + 2));
	printf("%2x: %5d: ", opcode, length);
	length -= 4;
	p += 4;
	while (length > 0) {
	    printf("%08x ", *((unsigned *) p));
	    p += 4;
	    length -= 4;
	}
	printf("\n");
    }	    
}
#endif
