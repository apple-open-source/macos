	/* $XFree86: xc/lib/GL/mesa/dri/dri_mesa.c,v 1.17 2001/08/27 17:40:57 dawes Exp $ */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright (c) 2002 Apple Computer, Inc.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Kevin E. Martin <kevin@precisioninsight.com>
 *   Brian E. Paul <brian@precisioninsight.com>
 */

/*
 * This file gets compiled into each of the DRI 3D drivers.  The
 * functions defined here are called from the GL library via
 * function pointers in the __DRIdisplayRec, __DRIscreenRec,
 * __DRIcontextRec, __DRIdrawableRec structures defined in glxclient.h
 *
 * Those function pointers are initialized by code in this file.
 * The process starts when libGL calls the __driCreateScreen() function
 * at the end of this file.
 *
 * The above-mentioned DRI structures have no dependencies on Mesa.
 * Each structure instead has a generic (void *) private pointer that
 * points to a private structure.  For Mesa drivers, these private
 * structures are the __DRIdrawablePrivateRec, __DRIcontextPrivateRec,
 * __DRIscreenPrivateRec, and __DRIvisualPrivateRec structures defined
 * in dri_mesaint.h.  We allocate and attach those structs here in
 * this file.
 */



#ifdef GLX_DIRECT_RENDERING

/* Need to include these first to ensure we pick up the correct gl.h
   and glext.h (from the framework) */
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLContext.h>

#include <unistd.h>
#include <X11/Xlibint.h>
#include <X11/extensions/Xext.h>
#include "extutil.h"
#include "glxclient.h"
#include "appledri.h"
#include "dri_driver.h"
#include "x-list.h"
#include "x-hash.h"

/* Context binding */
static Bool driMesaBindContext(Display *dpy, int scrn,
			       GLXDrawable draw, GLXContext gc);
static Bool driMesaUnbindContext(Display *dpy, int scrn,
				 GLXDrawable draw, GLXContext gc,
				 int will_rebind);

/* Drawable methods */
static void *driMesaCreateDrawable(Display *dpy, int scrn, GLXDrawable draw,
				   VisualID vid, __DRIdrawable *pdraw);
static __DRIdrawable *driMesaGetDrawable(Display *dpy, GLXDrawable draw,
					 void *screenPrivate);
static void driMesaSwapBuffers(Display *dpy, void *drawPrivate);
static void driMesaDestroyDrawable(Display *dpy, void *drawPrivate);

/* Context methods */
static void *driMesaCreateContext(Display *dpy, XVisualInfo *vis, void *shared,
				  __DRIcontext *pctx);
static void driMesaDestroyContext(Display *dpy, int scrn, void *screenPrivate);

/* Screen methods */
static void *driMesaCreateScreen(Display *dpy, int scrn, __DRIscreen *psc,
				 int numConfigs, __GLXvisualConfig *config);
static void driMesaDestroyScreen(Display *dpy, int scrn, void *screenPrivate);

static void driMesaCreateSurface(Display *dpy, int scrn,
				 __DRIdrawablePrivate *pdp);

static void unwrap_context (__DRIcontextPrivate *pcp);
static void wrap_context (__DRIcontextPrivate *pcp);

extern const CGLContextObj XAppleDRIGetIndirectContext (void);

/*****************************************************************/

/* Maintain a list of drawables */

static inline Bool
__driMesaAddDrawable(x_hash_table *drawHash, __DRIdrawable *pdraw)
{
    __DRIdrawablePrivate *pdp = (__DRIdrawablePrivate *)pdraw->private;

    assert (drawHash != NULL);

    x_hash_table_insert (drawHash, (void *) pdp->draw, pdraw);

    return GL_TRUE;
}

static inline __DRIdrawable *
__driMesaFindDrawable(x_hash_table *drawHash, GLXDrawable draw)
{
    if (drawHash == NULL)
	return NULL;

    return x_hash_table_lookup (drawHash, (void *) draw, NULL);
}

struct find_by_uid_closure {
    unsigned int uid;
    __DRIdrawable *ret;
};

static void
find_by_uid_cb (void *k, void *v, void *data)
{
    __DRIdrawable *pdraw = v;
    __DRIdrawablePrivate *pdp = (__DRIdrawablePrivate *)pdraw->private;
    struct find_by_uid_closure *c = data;

    if (pdp->uid == c->uid)
	c->ret = pdraw;
}

static __DRIdrawable *
__driMesaFindDrawableByUID (x_hash_table *drawHash, unsigned int uid)
{
    struct find_by_uid_closure c;

    c.uid = uid;
    c.ret = NULL;
    x_hash_table_foreach (drawHash, find_by_uid_cb, &c);

    return c.ret;
}

static inline void
__driMesaRemoveDrawable(x_hash_table *drawHash, __DRIdrawable *pdraw)
{
    __DRIdrawablePrivate *pdp = (__DRIdrawablePrivate *)pdraw->private;

    if (drawHash == NULL)
	return;

    x_hash_table_remove (drawHash, (void *) pdp->draw);
}

static Bool __driMesaWindowExistsFlag;

static int __driMesaWindowExistsErrorHandler(Display *dpy, XErrorEvent *xerr)
{
    if (xerr->error_code == BadWindow) {
	__driMesaWindowExistsFlag = GL_FALSE;
    }
    return 0;
}

static Bool __driMesaWindowExists(Display *dpy, GLXDrawable draw)
{
    XWindowAttributes xwa;
    int (*oldXErrorHandler)(Display *, XErrorEvent *);

    __driMesaWindowExistsFlag = GL_TRUE;
    oldXErrorHandler = XSetErrorHandler(__driMesaWindowExistsErrorHandler);
    XGetWindowAttributes(dpy, draw, &xwa); /* dummy request */
    XSetErrorHandler(oldXErrorHandler);
    return __driMesaWindowExistsFlag;
}

static void __driMesaCollectCallback (void *k, void *v, void *data)
{
    GLXDrawable draw = (GLXDrawable) k;
    __DRIdrawable *pdraw = v;
    x_list **todelete = data;

    __DRIdrawablePrivate *pdp = (__DRIdrawablePrivate *)pdraw->private;
    Display *dpy;

    dpy = pdp->driScreenPriv->display;
    XSync(dpy, GL_FALSE);
    if (!pdp->destroyed && !__driMesaWindowExists(dpy, draw)) {
	/* Destroy the local drawable data in the hash table, if the
	   drawable no longer exists in the Xserver */
	pdp->destroyed = TRUE;
	*todelete = x_list_prepend (*todelete, pdraw);
    }
}

static void __driMesaGarbageCollectDrawables(void *drawHash)
{
    __DRIdrawable *pdraw;
    __DRIdrawablePrivate *pdp;
    Display *dpy;
    x_list *todelete = NULL, *node;

    x_hash_table_foreach (drawHash, __driMesaCollectCallback, &todelete);

    for (node = todelete; node != NULL; node = node->next)
    {
	pdraw = node->data;
	pdp = (__DRIdrawablePrivate *)pdraw->private;
	dpy = pdp->driScreenPriv->display;

	/* Destroy the local drawable data in the hash table, if the
	   drawable no longer exists in the Xserver */

	__driMesaRemoveDrawable(drawHash, pdraw);
	(*pdraw->destroyDrawable)(dpy, pdraw->private);
	Xfree(pdraw);
    }

    x_list_free (todelete);
}

/*****************************************************************/

/* returns with psp->mutex locked if successful. */
static Bool
driMesaFindDrawableByUID (Display *dpy,unsigned int uid,
			  __DRIscreenPrivate **psp_ret,
			  __DRIdrawablePrivate **pdp_ret)
{
    __DRIscreen *pDRIScreen;
    __DRIscreenPrivate *psp;
    __DRIdrawable *pdraw;
    int scrn;

    for (scrn = 0; scrn < ScreenCount (dpy); scrn++)
    {
	if (!(pDRIScreen = __glXFindDRIScreen(dpy, scrn))) {
	    /* ERROR!!! */
	    return FALSE;
	} else if (!(psp = (__DRIscreenPrivate *)pDRIScreen->private)) {
	    /* ERROR!!! */
	    return FALSE;
	}

	xmutex_lock (psp->mutex);

	pdraw = __driMesaFindDrawableByUID (psp->drawHash, uid);
	if (pdraw != NULL) {
	    *psp_ret = psp;
	    *pdp_ret = pdraw->private;
	    return TRUE;
	};

	xmutex_unlock (psp->mutex);
    }

    return FALSE;
}

static void
unbind_context (__DRIcontextPrivate *pcp)
{
    /* Unbind the context from its old drawable. */

    if (pcp->driDrawablePriv != NULL)
    {
	if (pcp->next != NULL)
	    pcp->next->prev = pcp->prev;
	if (pcp->prev != NULL)
	    pcp->prev->next = pcp->next;

	if (pcp->driDrawablePriv->driContextPriv == pcp)
	    pcp->driDrawablePriv->driContextPriv = pcp->next;

	pcp->driDrawablePriv = NULL;
	pcp->prev = pcp->next = NULL;
    }

    if (pcp->surface_id != 0)
    {
	pcp->surface_id = 0;
	pcp->pending_clear = TRUE;
    }
}

static void
unbind_drawable (__DRIdrawablePrivate *pdp)
{
    __DRIcontextPrivate *pcp, *next;

    for (pcp = pdp->driContextPriv; pcp != NULL; pcp = next)
    {
	next = pcp->next;
	unbind_context (pcp);
    }
}

static void
update_context (__DRIcontextPrivate *pcp)
{
    if (pcp->pending_clear)
    {
	CGLClearDrawable (pcp->ctx);
	pcp->pending_clear = FALSE;
    }

    if (pcp->pending_update && pcp->surface_id != 0)
    {
	xp_update_gl_context (pcp->ctx);
	pcp->pending_update = FALSE;
    }
}

static Bool driMesaUnbindContext(Display *dpy, int scrn,
				 GLXDrawable draw, GLXContext gc,
				 int will_rebind)
{
    __DRIscreen *pDRIScreen;
//  __DRIdrawable *pdraw;
    __DRIcontextPrivate *pcp;
    __DRIscreenPrivate *psp;
    __DRIdrawablePrivate *pdp;

    /*
    ** Assume error checking is done properly in glXMakeCurrent before
    ** calling driMesaUnbindContext.
    */

    if (gc == NULL || draw == None) {
	/* ERROR!!! */
	return GL_FALSE;
    }

    if (!(pDRIScreen = __glXFindDRIScreen(dpy, scrn))) {
	/* ERROR!!! */
	return GL_FALSE;
    } else if (!(psp = (__DRIscreenPrivate *)pDRIScreen->private)) {
	/* ERROR!!! */
	return GL_FALSE;
    }

    xmutex_lock (psp->mutex);

    pcp = (__DRIcontextPrivate *)gc->driContext.private;

    pdp = pcp->driDrawablePriv;
    if (pdp == NULL) {
	/* ERROR!!! */
	xmutex_unlock (psp->mutex);
	return GL_FALSE;
    }

    /* Put this thread back into normal (indirect) dispatch mode. */
    CGLSetCurrentContext (XAppleDRIGetIndirectContext ());
    pcp->thread_id = 0;

    /* Lazily unbind the drawable from the context */
    unbind_context (pcp);

    if (pdp->refcount == 0) {
	/* ERROR!!! */
	xmutex_unlock (psp->mutex);
	return GL_FALSE;
    } else if (--pdp->refcount == 0) {
#if 0
	/*
	** NOT_DONE: When a drawable is unbound from one direct
	** rendering context and then bound to another, we do not want
	** to destroy the drawable data structure each time only to
	** recreate it immediatly afterwards when binding to the next
	** context.  This also causes conflicts with caching of the
	** drawable stamp.
	**
	** In addition, we don't destroy the drawable here since Mesa
	** keeps private data internally (e.g., software accumulation
	** buffers) that should not be destroyed unless the client
	** explicitly requests that the window be destroyed.
	**
	** When GLX 1.3 is integrated, the create and destroy drawable
	** functions will have user level counterparts and the memory
	** will be able to be recovered.
	** 
	** Below is an example of what needs to go into the destroy
	** drawable routine to support GLX 1.3.
	*/
	__driMesaRemoveDrawable(psp->drawHash, pdraw);
	(*pdraw->destroyDrawable)(dpy, pdraw->private);
	Xfree(pdraw);
#endif
    }

    xmutex_unlock (psp->mutex);
    return GL_TRUE;
}

static Bool driMesaBindContext(Display *dpy, int scrn,
			       GLXDrawable draw, GLXContext gc)
{
    __DRIscreen *pDRIScreen;
    __DRIdrawable *pdraw;
    __DRIdrawablePrivate *pdp;
    __DRIscreenPrivate *psp;
    __DRIcontextPrivate *pcp;

    /*
    ** Assume error checking is done properly in glXMakeCurrent before
    ** calling driMesaBindContext.
    */

    if (gc == NULL || draw == None) {
	/* ERROR!!! */
	return GL_FALSE;
    }

    if (!(pDRIScreen = __glXFindDRIScreen(dpy, scrn))) {
	/* ERROR!!! */
	return GL_FALSE;
    } else if (!(psp = (__DRIscreenPrivate *)pDRIScreen->private)) {
	/* ERROR!!! */
	return GL_FALSE;
    }

    xmutex_lock (psp->mutex);

    pdraw = __driMesaFindDrawable(psp->drawHash, draw);
    if (!pdraw) {
	/* Allocate a new drawable */
	pdraw = (__DRIdrawable *)Xmalloc(sizeof(__DRIdrawable));
	if (!pdraw) {
	    /* ERROR!!! */
	    xmutex_unlock (psp->mutex);
	    return GL_FALSE;
	}

	/* Create a new drawable */
	pdraw->private = driMesaCreateDrawable(dpy, scrn, draw, gc->vid,
					       pdraw);
	if (!pdraw->private) {
	    /* ERROR!!! */
	    Xfree(pdraw);
	    xmutex_unlock (psp->mutex);
	    return GL_FALSE;
	}

	/* Add pdraw to drawable list */
	if (!__driMesaAddDrawable(psp->drawHash, pdraw)) {
	    /* ERROR!!! */
	    (*pdraw->destroyDrawable)(dpy, pdraw->private);
	    Xfree(pdraw);
	    xmutex_unlock (psp->mutex);
	    return GL_FALSE;
	}
    }

    pdp = (__DRIdrawablePrivate *)pdraw->private;
    pcp = (__DRIcontextPrivate *)gc->driContext.private;

    if (pdp->surface_id == 0)
    {
	/* Surface got destroyed. Try to create a new one. */

	driMesaCreateSurface (dpy, scrn, pdp);
    }

    unbind_context (pcp);

    /* Bind the drawable to the context */
    pcp->driDrawablePriv = pdp;
    pcp->prev = NULL;
    pcp->next = pdp->driContextPriv;
    pdp->driContextPriv = pcp;
    pdp->refcount++;

    /* And the physical surface to the physical context */
    if (pcp->surface_id != pdp->surface_id)
    {
	pcp->surface_id = 0;

	/* Attaching the drawable sets the default viewport. But we don't
	   want to catch that call to glViewport in our wrappers. */
	unwrap_context (pcp);

	if (pdp->surface_id == 0)
	    CGLClearDrawable (pcp->ctx);
	else if (xp_attach_gl_context (pcp->ctx, pdp->surface_id) == Success)
	    pcp->surface_id = pdp->surface_id;
	else
	    fprintf (stderr, "failed to bind to surface\n");

	wrap_context (pcp);

	pcp->pending_clear = FALSE;
	pcp->pending_update = FALSE;
    }
    else if (pcp->pending_clear)
    {
	CGLClearDrawable (pcp->ctx);
	pcp->pending_clear = FALSE;
    }

    /* Activate the CGL context and remember which thread it's current for. */
    CGLSetCurrentContext (pcp->ctx);
    pcp->thread_id = xthread_self ();

    xmutex_unlock (psp->mutex);
    return GL_TRUE;
}

/*****************************************************************/

static xp_client_id
get_client_id (void)
{
    static xp_client_id id;

    if (id == 0)
    {
	if (xp_init (XP_IN_BACKGROUND) != Success
	    || xp_get_client_id (&id) != Success)
	{
	    return 0;
	}
    }

    return id;
}

static void driMesaCreateSurface(Display *dpy, int scrn,
				 __DRIdrawablePrivate *pdp)
{
    xp_client_id client_id;
    unsigned int key[2];

    pdp->surface_id = 0;
    pdp->uid = 0;

    client_id = get_client_id ();
    if (client_id == 0)
	return;

    if (XAppleDRICreateSurface (dpy, scrn, pdp->draw,
				client_id, key, &pdp->uid))
    {
	xp_import_surface (key, &pdp->surface_id);
    }
}

static void *driMesaCreateDrawable(Display *dpy, int scrn, GLXDrawable draw,
				   VisualID vid, __DRIdrawable *pdraw)
{
    __DRIscreen *pDRIScreen;
    __DRIscreenPrivate *psp;
    __DRIdrawablePrivate *pdp;

    pdp = (__DRIdrawablePrivate *)Xmalloc(sizeof(__DRIdrawablePrivate));
    if (!pdp) {
	return NULL;
    }

    pdp->draw = draw;
    pdp->refcount = 0;
    pdp->surface_id = 0;
    pdp->uid = 0;
    pdp->destroyed = FALSE;

    if (!(pDRIScreen = __glXFindDRIScreen(dpy, scrn))) {
	Xfree(pdp);
	return NULL;
    } else if (!(psp = (__DRIscreenPrivate *)pDRIScreen->private)) {
	Xfree(pdp);
	return NULL;
    }
    pdp->driScreenPriv = psp;
    pdp->driContextPriv = NULL;

    driMesaCreateSurface (dpy, scrn, pdp);
    if (pdp->surface_id == 0) {
	Xfree (pdp);
	return NULL;
    }

    pdraw->destroyDrawable = driMesaDestroyDrawable;
    pdraw->swapBuffers = driMesaSwapBuffers;

    return (void *)pdp;
}

static __DRIdrawable *driMesaGetDrawable(Display *dpy, GLXDrawable draw,
					 void *screenPrivate)
{
    __DRIscreenPrivate *psp = (__DRIscreenPrivate *) screenPrivate;
    __DRIdrawable *dri_draw;

    xmutex_lock (psp->mutex);

    /*
    ** Make sure this routine returns NULL if the drawable is not bound
    ** to a direct rendering context!
    */
    dri_draw = __driMesaFindDrawable(psp->drawHash, draw);

    xmutex_unlock (psp->mutex);
    return dri_draw;
}

static void driMesaSwapBuffers(Display *dpy, void *drawPrivate)
{
    __DRIdrawablePrivate *pdp = (__DRIdrawablePrivate *) drawPrivate;
    __DRIcontextPrivate *pcp;
    xthread_t self = xthread_self ();
    static Bool warned;

    xmutex_lock (pdp->driScreenPriv->mutex);

    /* FIXME: this is sub-optimal, since we may not always find a context
       bound to the given drawable on this thread. */

    for (pcp = pdp->driContextPriv; pcp != NULL; pcp = pcp->next)
    {
	if (pcp->thread_id == self || pcp->thread_id == 0)
	    break;
    }

    if (pcp != NULL)
    {
	CGLFlushDrawable (pcp->ctx);
    }
    else
    {
	if (!warned) {
	    fprintf (stderr, "glXSwapBuffers: no context for this drawable\n");
	    warned = TRUE;
	}
    }


    xmutex_unlock (pdp->driScreenPriv->mutex);
}

static void driMesaDestroyDrawable(Display *dpy, void *drawPrivate)
{
    __DRIdrawablePrivate *pdp = (__DRIdrawablePrivate *)drawPrivate;

    if (pdp) {
	xmutex_lock (pdp->driScreenPriv->mutex);
	unbind_drawable (pdp);
	xmutex_unlock (pdp->driScreenPriv->mutex);
	if (pdp->surface_id != 0) {
	    xp_destroy_surface (pdp->surface_id);
	    pdp->surface_id = 0;
	}
	if (!pdp->destroyed) {
	    /* don't try to destroy an already destroyed surface. */
	    XAppleDRIDestroySurface(dpy, pdp->driScreenPriv->myNum, pdp->draw);
	}
	Xfree(pdp);
    }
}

/*****************************************************************/

static CGLPixelFormatObj
driCreatePixelFormat (Display *dpy, __DRIscreenPrivate *psp,
		      XVisualInfo *visinfo, __GLXvisualConfig *config)
{
    int i;
    CGLPixelFormatAttribute attr[64]; // currently uses max of 30
    CGLPixelFormatObj result;
    long n_formats;

    i = 0;

    if (!config->rgba)
	return NULL;

    if (config->stereo)
	attr[i++] = kCGLPFAStereo;

    if (config->doubleBuffer)
	attr[i++] = kCGLPFADoubleBuffer;

    attr[i++] = kCGLPFAColorSize;
    attr[i++] = config->redSize + config->greenSize + config->blueSize;
    attr[i++] = kCGLPFAAlphaSize;
    attr[i++] = 1; /* FIXME: ignoring config->alphaSize which is always 0 */

    if (config->accumRedSize + config->accumGreenSize
	+ config->accumBlueSize + config->accumAlphaSize > 0)
    {
	attr[i++] = kCGLPFAAccumSize;
	attr[i++] = (config->accumRedSize + config->accumGreenSize
		     + config->accumBlueSize + config->accumAlphaSize);
    }

    if (config->depthSize > 0) {
	attr[i++] = kCGLPFADepthSize;
	attr[i++] = config->depthSize;
    }

    if (config->stencilSize > 0) {
	attr[i++] = kCGLPFAStencilSize;
	attr[i++] = config->stencilSize;
    }

    if (config->auxBuffers > 0) {
	attr[i++] = kCGLPFAAuxBuffers;
	attr[i++] = config->auxBuffers;
    }

    /* FIXME: things we don't handle: color/alpha masks, level,
       visualrating, transparentFoo */

    attr[i++] = 0;

    result = NULL;
    CGLChoosePixelFormat(attr, &result, &n_formats);

    return result;
}

static void *driMesaCreateContext(Display *dpy, XVisualInfo *vis, void *shared,
				  __DRIcontext *pctx)
{
    __DRIscreen *pDRIScreen;
    __DRIcontextPrivate *pcp;
    __DRIcontextPrivate *pshare = (__DRIcontextPrivate *)shared;
    __DRIscreenPrivate *psp;
    int i;

    if (!(pDRIScreen = __glXFindDRIScreen(dpy, vis->screen))) {
	/* ERROR!!! */
	return NULL;
    } else if (!(psp = (__DRIscreenPrivate *)pDRIScreen->private)) {
	/* ERROR!!! */
	return NULL;
    }

    /* Create the hash table */
    if (!psp->drawHash) {
	xmutex_lock (psp->mutex);
	if (!psp->drawHash)
	    psp->drawHash = x_hash_table_new (NULL, NULL, NULL, NULL);
	xmutex_unlock (psp->mutex);
    }

    pcp = (__DRIcontextPrivate *)Xmalloc(sizeof(__DRIcontextPrivate));
    if (!pcp) {
	return NULL;
    }

    pcp->display = dpy;
    pcp->driScreenPriv = psp;
    pcp->driDrawablePriv = NULL;

    pcp->ctx = NULL;
    pcp->surface_id = 0;

    pcp->pending_clear = FALSE;
    pcp->pending_update = FALSE;

    pcp->ctx = NULL;
    for (i = 0; pcp->ctx == NULL && i < psp->numVisuals; i++) {
        if (psp->visuals[i].vid == vis->visualid) {
	    CGLCreateContext (psp->visuals[i].pixel_format,
			      pshare ? pshare->ctx : NULL, &pcp->ctx);
        }
    }

    if (!pcp->ctx) {
	Xfree(pcp);
	return NULL;
    }

    pctx->destroyContext = driMesaDestroyContext;
    pctx->bindContext    = driMesaBindContext;
    pctx->unbindContext  = driMesaUnbindContext;

    wrap_context (pcp);

    xmutex_lock (psp->mutex);
    __driMesaGarbageCollectDrawables(pcp->driScreenPriv->drawHash);
    xmutex_unlock (psp->mutex);

    return pcp;
}

static void driMesaDestroyContext(Display *dpy, int scrn, void *contextPrivate)
{
    __DRIcontextPrivate  *pcp   = (__DRIcontextPrivate *) contextPrivate;

    if (pcp) {
	xmutex_lock (pcp->driScreenPriv->mutex);
	unbind_context (pcp);
	__driMesaGarbageCollectDrawables(pcp->driScreenPriv->drawHash);
	xmutex_unlock (pcp->driScreenPriv->mutex);
	CGLDestroyContext (pcp->ctx);
	Xfree(pcp);
    }
}

/*****************************************************************/

static void *driMesaCreateScreen(Display *dpy, int scrn, __DRIscreen *psc,
				 int numConfigs, __GLXvisualConfig *config)
{
    int directCapable, i, n;
    __DRIscreenPrivate *psp;
    XVisualInfo visTmpl, *visinfo;

    if (!XAppleDRIQueryDirectRenderingCapable(dpy, scrn, &directCapable)) {
	return NULL;
    }

    if (!directCapable) {
	return NULL;
    }

    psp = (__DRIscreenPrivate *)Xmalloc(sizeof(__DRIscreenPrivate));
    if (!psp) {
	return NULL;
    }

    psp->mutex = xmutex_malloc ();
    psp->display = dpy;
    psp->myNum = scrn;

#if 0
    if (!XAppleDRIAuthConnection(dpy, scrn, magic)) {
	Xfree(psp);
	(void)XAppleDRICloseConnection(dpy, scrn);
	return NULL;
    }
#endif

    /*
     * Allocate space for an array of visual records and initialize them.
     */
    psp->visuals = (__DRIvisualPrivate *)Xmalloc(numConfigs *
						 sizeof(__DRIvisualPrivate));
    if (!psp->visuals) {
	Xfree(psp);
	return NULL;
    }

    visTmpl.screen = scrn;
    visinfo = XGetVisualInfo(dpy, VisualScreenMask, &visTmpl, &n);
    if (n != numConfigs) {
	Xfree(psp);
	return NULL;
    }

    psp->numVisuals = 0;
    for (i = 0; i < numConfigs; i++, config++) {
	psp->visuals[psp->numVisuals].vid = visinfo[i].visualid;
        psp->visuals[psp->numVisuals].pixel_format =
                 driCreatePixelFormat (dpy, psp, &visinfo[i], config);
	if (psp->visuals[psp->numVisuals].pixel_format != NULL) {
	    psp->numVisuals++;
	}
    }

    XFree(visinfo);

    if (psp->numVisuals == 0) {
	/* Couldn't create any pixel formats. */
	Xfree(psp->visuals);
	Xfree(psp);
	return NULL;
    }

    /* Initialize the drawHash when the first context is created */
    psp->drawHash = NULL;

    psc->destroyScreen  = driMesaDestroyScreen;
    psc->createContext  = driMesaCreateContext;
    psc->createDrawable = driMesaCreateDrawable;
    psc->getDrawable    = driMesaGetDrawable;

    return (void *)psp;
}

static void driMesaDestroyScreen(Display *dpy, int scrn, void *screenPrivate)
{
    __DRIscreenPrivate *psp = (__DRIscreenPrivate *) screenPrivate;

    if (psp) {
	//FIXME resetDriver ?
	Xfree(psp->visuals);
	Xfree(psp);
    }
}

/* Note: definitely can't make any X protocol requests here. */
static void driAppleSurfaceNotify (Display *dpy, unsigned int uid, int kind)
{
    __DRIscreenPrivate *psp;
    __DRIdrawablePrivate *pdp;
    __DRIcontextPrivate *pcp;

    /* locks psp->mutex if successful. */
    if (driMesaFindDrawableByUID (dpy, uid, &psp, &pdp))
    {
	xthread_t self = xthread_self ();

	switch (kind)
	{
	    Bool all_safe;

	case AppleDRISurfaceNotifyDestroyed:
	    xp_destroy_surface (pdp->surface_id);
	    pdp->surface_id = 0;

	    for (pcp = pdp->driContextPriv; pcp != NULL; pcp = pcp->next)
	    {
		pcp->surface_id = 0;

		if (pcp->thread_id == self || pcp->thread_id == 0) {
		    CGLClearDrawable (pcp->ctx);
		    pcp->pending_clear = FALSE;
		} else
		    pcp->pending_clear = TRUE;
	    }
	    break;

	case AppleDRISurfaceNotifyChanged:
	    all_safe = TRUE;
	    for (pcp = pdp->driContextPriv; pcp != NULL; pcp = pcp->next)
	    {
		if (pcp->thread_id != 0 && pcp->thread_id != self) {
		    all_safe = FALSE;
		    break;
		}
	    }
	    for (pcp = pdp->driContextPriv; pcp != NULL; pcp = pcp->next)
	    {
		if (all_safe) {
		    xp_update_gl_context (pcp->ctx);
		    pcp->pending_update = FALSE;
		} else
		    pcp->pending_update = TRUE;
	    }
	    break;
	}

	xmutex_unlock (psp->mutex);
    }
}

/*
 * This is the entrypoint into the DRI 3D driver.
 * The driCreateScreen name is the symbol that libGL.so fetches via
 * dlsym() in order to bootstrap the driver.
 */
void *__driCreateScreen(Display *dpy, int scrn, __DRIscreen *psc,
                        int numConfigs, __GLXvisualConfig *config)
{
    static int here_before;

    if (!here_before)
    {
	XAppleDRISetSurfaceNotifyHandler (driAppleSurfaceNotify);
	here_before = True;
    }

    return driMesaCreateScreen(dpy, scrn, psc, numConfigs, config);
}

void __driRegisterExtensions (void)
{
}

__private_extern__ void XAppleDRIUseIndirectDispatch (void)
{
    CGLSetCurrentContext (XAppleDRIGetIndirectContext ());
}

/*****************************************************************/

/*
 * Currently (Mac OS X 10.3) the only way we have of regaining control
 * from threads calling GL and nothing else is by patching the dispatch
 * table of the CGLContext, so that glViewport, glFlush and glFinish
 * call us back.
 *
 * Since glNewList and glEndList overwrite the entire dispatch table we
 * also need to patch those so we can restore the others.
 *
 * WARNING: this is not expected to work on future OS releases.
 */

#define WRAP_CGL(context, vec, fun)			\
    do {						\
	(context)->disp.vec = (context)->ctx->disp.vec;	\
	(context)->ctx->disp.vec = (fun);		\
    } while (0)

#define UNWRAP_CGL(context, vec)			\
    do {						\
	(context)->ctx->disp.vec = (context)->disp.vec;	\
    } while (0)

#define WRAP_BOILERPLATE					\
    GLXContext gc;						\
    __DRIcontextPrivate *pcp;					\
    gc = __glXGetCurrentContext ();				\
    if (gc == NULL || !gc->isDirect) return;			\
    pcp = (__DRIcontextPrivate *) gc->driContext.private;	\
    if (pcp == NULL) return;

static void viewport_callback (GLIContext ctx, GLint x, GLint y,
			       GLsizei width, GLsizei height)
{
    WRAP_BOILERPLATE

    xmutex_lock (pcp->driScreenPriv->mutex);
    update_context (pcp);
    xmutex_unlock (pcp->driScreenPriv->mutex);

    (*pcp->disp.viewport) (ctx, x, y, width, height);
}

static void new_list_callback (GLIContext ctx, GLuint list, GLenum mode)
{
    WRAP_BOILERPLATE

    unwrap_context (pcp);
    (*pcp->ctx->disp.new_list) (ctx, list, mode);
    wrap_context (pcp);
}

static void end_list_callback (GLIContext ctx)
{
    WRAP_BOILERPLATE

    unwrap_context (pcp);
    (*pcp->ctx->disp.end_list) (ctx);
    wrap_context (pcp);
}

static void unwrap_context (__DRIcontextPrivate *pcp)
{
    UNWRAP_CGL (pcp, viewport);
    UNWRAP_CGL (pcp, new_list);
    UNWRAP_CGL (pcp, end_list);
}

static void wrap_context (__DRIcontextPrivate *pcp)
{
    WRAP_CGL (pcp, new_list, new_list_callback);
    WRAP_CGL (pcp, end_list, end_list_callback);
    WRAP_CGL (pcp, viewport, viewport_callback);
}

#endif /* GLX_DIRECT_RENDERING */
