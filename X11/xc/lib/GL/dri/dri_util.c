/* $XFree86: xc/lib/GL/dri/dri_util.c,v 1.6 2003/02/15 22:12:29 dawes Exp $ */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
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
 * This file gets compiled into each of the DRI 3D drivers.  This is
 * basically just a collection of utility functions that are useful
 * for most drivers.  A DRI driver doesn't have to use any of this,
 * but it's useful boilerplate.
 *
 *
 * Many of the functions defined here are called from the GL library
 * via function pointers in the __DRIdisplayRec, __DRIscreenRec,
 * __DRIcontextRec, __DRIdrawableRec structures defined in glxclient.h
 *
 * Those function pointers are initialized by code in this file.
 * The process starts when libGL calls the __driCreateScreen() function
 * at the end of this file.
 *
 * The above-mentioned DRI structures have no dependencies on Mesa.
 * Each structure instead has a generic (void *) private pointer that
 * points to a private structure.  For the current drivers, these private
 * structures are the __DRIdrawablePrivateRec, __DRIcontextPrivateRec,
 * __DRIscreenPrivateRec, and __DRIvisualPrivateRec structures defined
 * in dri_util.h.  We allocate and attach those structs here in
 * this file.
 */


#ifdef GLX_DIRECT_RENDERING

#include <assert.h>
#include <unistd.h>
#include <X11/Xlibint.h>
#include <Xext.h>
#include <extutil.h>
#include <stdio.h>
#include "glxclient.h"
#include "xf86dri.h"
#include "sarea.h"
#include "dri_util.h"


/* forward declarations */
static void *driCreateDrawable(Display *dpy, int scrn, GLXDrawable draw,
                               GLboolean isPixmap,
                               VisualID vid, __DRIdrawable *pdraw);

static void driDestroyDrawable(Display *dpy, void *drawablePrivate);




static Bool driFeatureOn(const char *name)
{
    char *env = getenv(name);

    if (!env) return GL_FALSE;
    if (!strcasecmp(env, "enable")) return GL_TRUE;
    if (!strcasecmp(env, "1"))      return GL_TRUE;
    if (!strcasecmp(env, "on"))     return GL_TRUE;
    if (!strcasecmp(env, "true"))   return GL_TRUE;
    if (!strcasecmp(env, "t"))      return GL_TRUE;
    if (!strcasecmp(env, "yes"))    return GL_TRUE;
    if (!strcasecmp(env, "y"))      return GL_TRUE;

    return GL_FALSE;
}


/*
** Print message to stderr if LIBGL_DEBUG env var is set.
*/
void
__driUtilMessage(const char *f, ...)
{
    va_list args;

    if (getenv("LIBGL_DEBUG")) {
        fprintf(stderr, "libGL error: \n");
        va_start(args, f);
        vfprintf(stderr, f, args);
        va_end(args);
        fprintf(stderr, "\n");
    }
}


/*****************************************************************/

/*
 * Return pointer to the __GLXvisualConfig specified by dpy, scrn and vid.
 * Return NULL if not found.
 */
static __GLXvisualConfig *
__driFindGlxConfig(Display *dpy, int scrn, VisualID vid)
{
    __GLXdisplayPrivate *priv;
    __GLXscreenConfigs *glxScrnConfigs;
    __GLXvisualConfig *glxConfigs;
    int numConfigs, i;

    priv = __glXInitialize(dpy);
    assert(priv);

    glxScrnConfigs = priv->screenConfigs;
    assert(glxScrnConfigs);

    numConfigs = glxScrnConfigs[scrn].numConfigs;
    glxConfigs = glxScrnConfigs[scrn].configs;

    for (i = 0; i < numConfigs; i++) {
        if (glxConfigs[i].vid == vid) {
            return glxConfigs +i;
        }
    }
    return NULL;
}


/* This function comes from programs/Xserver/GL/glx/glxcmds.c
 */
static void
__glXFormatGLModes(__GLcontextModes *modes, const __GLXvisualConfig *config)
{
    /*__glXMemset(modes, 0, sizeof(__GLcontextModes));*/
    memset(modes, 0, sizeof(__GLcontextModes));

    modes->rgbMode = (config->rgba != 0);
    modes->colorIndexMode = !(modes->rgbMode);
    modes->doubleBufferMode = (config->doubleBuffer != 0);
    modes->stereoMode = (config->stereo != 0);

    modes->haveAccumBuffer = ((config->accumRedSize +
			       config->accumGreenSize +
			       config->accumBlueSize +
			       config->accumAlphaSize) > 0);
    modes->haveDepthBuffer = (config->depthSize > 0);
    modes->haveStencilBuffer = (config->stencilSize > 0);

    modes->redBits = config->redSize;
    modes->greenBits = config->greenSize;
    modes->blueBits = config->blueSize;
    modes->alphaBits = config->alphaSize;
    modes->redMask = config->redMask;
    modes->greenMask = config->greenMask;
    modes->blueMask = config->blueMask;
    modes->alphaMask = config->alphaMask;
    modes->rgbBits = config->bufferSize;
    modes->indexBits = config->bufferSize;

    modes->accumRedBits = config->accumRedSize;
    modes->accumGreenBits = config->accumGreenSize;
    modes->accumBlueBits = config->accumBlueSize;
    modes->accumAlphaBits = config->accumAlphaSize;
    modes->depthBits = config->depthSize;
    modes->stencilBits = config->stencilSize;

    modes->numAuxBuffers = 0;	/* XXX: should be picked up from the visual */

    modes->level = config->level;
}


/*****************************************************************/

/* Maintain a list of drawables */

static Bool __driAddDrawable(void *drawHash, __DRIdrawable *pdraw)
{
    __DRIdrawablePrivate *pdp = (__DRIdrawablePrivate *)pdraw->private;

    if (drmHashInsert(drawHash, pdp->draw, pdraw))
	return GL_FALSE;

    return GL_TRUE;
}

static __DRIdrawable *__driFindDrawable(void *drawHash, GLXDrawable draw)
{
    int retcode;
    __DRIdrawable *pdraw;

    retcode = drmHashLookup(drawHash, draw, (void **)&pdraw);
    if (retcode)
	return NULL;

    return pdraw;
}

static void __driRemoveDrawable(void *drawHash, __DRIdrawable *pdraw)
{
    int retcode;
    __DRIdrawablePrivate *pdp = (__DRIdrawablePrivate *)pdraw->private;

    retcode = drmHashLookup(drawHash, pdp->draw, (void **)&pdraw);
    if (!retcode) { /* Found */
	drmHashDelete(drawHash, pdp->draw);
    }
}

static Bool __driWindowExistsFlag;

static int __driWindowExistsErrorHandler(Display *dpy, XErrorEvent *xerr)
{
    if (xerr->error_code == BadWindow) {
	__driWindowExistsFlag = GL_FALSE;
    }
    return 0;
}

static Bool __driWindowExists(Display *dpy, GLXDrawable draw)
{
    XWindowAttributes xwa;
    int (*oldXErrorHandler)(Display *, XErrorEvent *);

    __driWindowExistsFlag = GL_TRUE;
    oldXErrorHandler = XSetErrorHandler(__driWindowExistsErrorHandler);
    XGetWindowAttributes(dpy, draw, &xwa); /* dummy request */
    XSetErrorHandler(oldXErrorHandler);
    return __driWindowExistsFlag;
}

static void __driGarbageCollectDrawables(void *drawHash)
{
    GLXDrawable draw;
    __DRIdrawable *pdraw;
    Display *dpy;

    if (drmHashFirst(drawHash, &draw, (void **)&pdraw)) {
	do {
	    __DRIdrawablePrivate *pdp = (__DRIdrawablePrivate *)pdraw->private;
	    dpy = pdp->driScreenPriv->display;
	    XSync(dpy, GL_FALSE);
	    if (!__driWindowExists(dpy, draw)) {
		/* Destroy the local drawable data in the hash table, if the
		   drawable no longer exists in the Xserver */
		__driRemoveDrawable(drawHash, pdraw);
		(*pdraw->destroyDrawable)(dpy, pdraw->private);
		Xfree(pdraw);
	    }
	} while (drmHashNext(drawHash, &draw, (void **)&pdraw));
    }
}

/*****************************************************************/

static Bool driUnbindContext(Display *dpy, int scrn,
                             GLXDrawable draw, GLXContext gc,
                             int will_rebind)
{
    __DRIscreen *pDRIScreen;
    __DRIdrawable *pdraw;
    __DRIcontextPrivate *pcp;
    __DRIscreenPrivate *psp;
    __DRIdrawablePrivate *pdp;

    /*
    ** Assume error checking is done properly in glXMakeCurrent before
    ** calling driUnbindContext.
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

    pcp = (__DRIcontextPrivate *)gc->driContext.private;

    pdraw = __driFindDrawable(psp->drawHash, draw);
    if (!pdraw) {
	/* ERROR!!! */
	return GL_FALSE;
    }
    pdp = (__DRIdrawablePrivate *)pdraw->private;

				/* Don't leave fullscreen mode if the
                                   drawable will be rebound in the next
                                   step -- this avoids a protocol
                                   request. */
    if (!will_rebind && psp->fullscreen) {
	psp->DriverAPI.CloseFullScreen(pcp);
	XF86DRICloseFullScreen(dpy, scrn, draw);
	psp->fullscreen = NULL;
    }

    /* Let driver unbind drawable from context */
    (*psp->DriverAPI.UnbindContext)(pcp);

    if (pdp->refcount == 0) {
	/* ERROR!!! */
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
	__driRemoveDrawable(psp->drawHash, pdraw);
	(*pdraw->destroyDrawable)(dpy, pdraw->private);
	Xfree(pdraw);
#endif
    }

    /* XXX this is disabled so that if we call SwapBuffers on an unbound
     * window we can determine the last context bound to the window and
     * use that context's lock. (BrianP, 2-Dec-2000)
     */
#if 0
    /* Unbind the drawable */
    pcp->driDrawablePriv = NULL;
    pdp->driContextPriv = &psp->dummyContextPriv;
#endif

    return GL_TRUE;
}


/*
 * This function takes both a read buffer and a draw buffer.
 * This is needed for glXMakeCurrentReadSGI() or GLX 1.3's
 * glxMakeContextCurrent() function.
 */
static Bool driBindContext2(Display *dpy, int scrn,
                            GLXDrawable draw, GLXDrawable read,
                            GLXContext gc)
{
    __DRIscreen *pDRIScreen;
    __DRIdrawable *pdraw;
    __DRIdrawablePrivate *pdp;
    __DRIdrawable *pread;
    __DRIdrawablePrivate *prp;
    __DRIscreenPrivate *psp;
    __DRIcontextPrivate *pcp;
    static Bool envchecked      = False;
    static Bool checkfullscreen = False;

    /*
    ** Assume error checking is done properly in glXMakeCurrent before
    ** calling driBindContext.
    */

    if (gc == NULL || draw == None || read == None) {
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

    /* Find the _DRIdrawable which corresponds to the writing GLXDrawable */
    pdraw = __driFindDrawable(psp->drawHash, draw);
    if (!pdraw) {
	/* Allocate a new drawable */
	pdraw = (__DRIdrawable *)Xmalloc(sizeof(__DRIdrawable));
	if (!pdraw) {
	    /* ERROR!!! */
	    return GL_FALSE;
	}

	/* Create a new drawable */
	pdraw->private = driCreateDrawable(dpy, scrn, draw, GL_FALSE,
                                           gc->vid, pdraw);
	if (!pdraw->private) {
	    /* ERROR!!! */
	    Xfree(pdraw);
	    return GL_FALSE;
	}

	/* Add pdraw to drawable list */
	if (!__driAddDrawable(psp->drawHash, pdraw)) {
	    /* ERROR!!! */
	    (*pdraw->destroyDrawable)(dpy, pdraw->private);
	    Xfree(pdraw);
	    return GL_FALSE;
	}
    }
    pdp = (__DRIdrawablePrivate *) pdraw->private;

    /* Find the _DRIdrawable which corresponds to the reading GLXDrawable */
    if (read == draw) {
        /* read buffer == draw buffer */
        prp = pdp;
    }
    else {
        pread = __driFindDrawable(psp->drawHash, read);
        if (!pread) {
            /* Allocate a new drawable */
            pread = (__DRIdrawable *)Xmalloc(sizeof(__DRIdrawable));
            if (!pread) {
                /* ERROR!!! */
                return GL_FALSE;
            }

            /* Create a new drawable */
            pread->private = driCreateDrawable(dpy, scrn, read, GL_FALSE,
                                               gc->vid, pread);
            if (!pread->private) {
                /* ERROR!!! */
                Xfree(pread);
                return GL_FALSE;
            }

            /* Add pread to drawable list */
            if (!__driAddDrawable(psp->drawHash, pread)) {
                /* ERROR!!! */
                (*pread->destroyDrawable)(dpy, pread->private);
                Xfree(pread);
                return GL_FALSE;
            }
        }
        prp = (__DRIdrawablePrivate *) pread->private;
    }

    /* Bind the drawable to the context */
    pcp = (__DRIcontextPrivate *)gc->driContext.private;
    pcp->driDrawablePriv = pdp;
    pdp->driContextPriv = pcp;
    pdp->refcount++;

    /*
    ** Now that we have a context associated with this drawable, we can
    ** initialize the drawable information if has not been done before.
    ** Also, since we need it when LIBGL_DRI_FULLSCREEN, pick up any changes
    ** that are outstanding.
    */
    if (!pdp->pStamp || *pdp->pStamp != pdp->lastStamp) {
	DRM_SPINLOCK(&psp->pSAREA->drawable_lock, psp->drawLockID);
	__driUtilUpdateDrawableInfo(dpy, scrn, pdp);
	DRM_SPINUNLOCK(&psp->pSAREA->drawable_lock, psp->drawLockID);
    }

    /* Call device-specific MakeCurrent */
    (*psp->DriverAPI.MakeCurrent)(pcp, pdp, prp);

    /* Check for the potential to enter an automatic full-screen mode.
       This may need to be moved up. */
    if (!envchecked) {
	checkfullscreen = driFeatureOn("LIBGL_DRI_AUTOFULLSCREEN");
	envchecked = GL_TRUE;
    }
    if (checkfullscreen && pdp->numClipRects == 1) {
	/* If there is valid information in the SAREA, we can use it to
	avoid a protocol request.  The only time when the SAREA
	information won't be valid will be initially, so in the worst
	case, we'll make one protocol request that we could have
	avoided. */
	int try = 1;
	int clw = pdp->pClipRects[0].x2 - pdp->pClipRects[0].x1;
	int clh = pdp->pClipRects[0].y2 - pdp->pClipRects[0].y1;

#if 0
				/* Useful client-side debugging message */
	fprintf(stderr,
		"********************************************\n"
		"********************************************\n"
		"********************************************\n"
		"%d @ %d,%d,%d,%d\n"
		"frame %d,%d,%d,%d\n"
		"win   %d,%d,%d,%d\n"
		"fs = %p pdp = %p sarea = %d\n"
		"********************************************\n"
		"********************************************\n"
		"********************************************\n",
		pdp->numClipRects,
		pdp->pClipRects[0].x1,
		pdp->pClipRects[0].y1,
		pdp->pClipRects[0].x2,
		pdp->pClipRects[0].y2,
		psp->pSAREA->frame.x,
		psp->pSAREA->frame.y,
		psp->pSAREA->frame.width,
		psp->pSAREA->frame.height,
		pdp->x, pdp->y, pdp->w, pdp->h,
		psp->fullscreen, pdp, psp->pSAREA->frame.fullscreen);
#endif


	if (pdp->x != pdp->pClipRects[0].x1
	    || pdp->y != pdp->pClipRects[0].y1
	    || pdp->w != clw
	    || pdp->h != clh) try = 0;

	if (try && psp->pSAREA->frame.width && psp->pSAREA->frame.height) {
	    if (pdp->x != psp->pSAREA->frame.x
		|| pdp->y != psp->pSAREA->frame.y
		|| pdp->w != psp->pSAREA->frame.width
		|| pdp->h != psp->pSAREA->frame.height) try = 0;
	}

	if (try) {
	    if (psp->fullscreen && !psp->pSAREA->frame.fullscreen) {
				/* Server has closed fullscreen mode */
		__driUtilMessage("server closed fullscreen mode\n");
		psp->fullscreen = NULL;
	    }
	    if (XF86DRIOpenFullScreen(dpy, scrn, draw)) {
		psp->fullscreen = pdp;
		psp->DriverAPI.OpenFullScreen(pcp);
	    }
	}
    }

    return GL_TRUE;
}


/*
 * Simply call bind with the same GLXDrawable for the read and draw buffers.
 */
static Bool driBindContext(Display *dpy, int scrn,
                           GLXDrawable draw, GLXContext gc)
{
    return driBindContext2(dpy, scrn, draw, draw, gc);
}


/*****************************************************************/

/*
 * This function basically updates the __DRIdrawablePrivate struct's
 * cliprect information by calling XF86DRIGetDrawableInfo().  This is
 * usually called by the DRI_VALIDATE_DRAWABLE_INFO macro which
 * compares the __DRIdrwablePrivate pStamp and lastStamp values.  If
 * the values are different that means we have to update the clipping
 * info.
 */
void
__driUtilUpdateDrawableInfo(Display *dpy, int scrn,
                            __DRIdrawablePrivate *pdp)
{
    __DRIscreenPrivate *psp;
    __DRIcontextPrivate *pcp = pdp->driContextPriv;
    
    if (!pcp || (pdp != pcp->driDrawablePriv)) {
	/* ERROR!!! */
	return;
    }

    psp = pdp->driScreenPriv;
    if (!psp) {
	/* ERROR!!! */
	return;
    }

    if (pdp->pClipRects) {
	Xfree(pdp->pClipRects); 
    }

    if (pdp->pBackClipRects) {
	Xfree(pdp->pBackClipRects); 
    }

    DRM_SPINUNLOCK(&psp->pSAREA->drawable_lock, psp->drawLockID);

    if (!__driFindDrawable(psp->drawHash, pdp->draw) ||
	!XF86DRIGetDrawableInfo(dpy, scrn, pdp->draw,
				&pdp->index, &pdp->lastStamp,
				&pdp->x, &pdp->y, &pdp->w, &pdp->h,
				&pdp->numClipRects, &pdp->pClipRects,
				&pdp->backX,
				&pdp->backY,
				&pdp->numBackClipRects,
				&pdp->pBackClipRects
                                )) {
	/* Error -- eg the window may have been destroyed.  Keep going
	 * with no cliprects.
	 */
	pdp->pStamp = &pdp->lastStamp; /* prevent endless loop */
	pdp->numClipRects = 0;
	pdp->pClipRects = NULL;
	pdp->numBackClipRects = 0;
	pdp->pBackClipRects = NULL;
    }
    else
       pdp->pStamp = &(psp->pSAREA->drawableTable[pdp->index].stamp);

    DRM_SPINLOCK(&psp->pSAREA->drawable_lock, psp->drawLockID);

}

/*****************************************************************/

/*
 * This is called via __DRIscreenRec's createDrawable pointer.
 * libGL doesn't use it at this time.  See comments in glxclient.h.
 */
static void *driCreateDrawable_dummy(Display *dpy, int scrn,
                                     GLXDrawable draw,
                                     VisualID vid, __DRIdrawable *pdraw)
{
   return driCreateDrawable(dpy, scrn, draw, GL_FALSE, vid, pdraw);
}



static void *driCreateDrawable(Display *dpy, int scrn, GLXDrawable draw,
                               GLboolean isPixmap,
                               VisualID vid, __DRIdrawable *pdraw)
{
    __DRIscreen *pDRIScreen;
    __DRIscreenPrivate *psp;
    __DRIdrawablePrivate *pdp;
    __GLXvisualConfig *config;
    __GLcontextModes modes;

    pdp = (__DRIdrawablePrivate *)Xmalloc(sizeof(__DRIdrawablePrivate));
    if (!pdp) {
	return NULL;
    }

    if (!XF86DRICreateDrawable(dpy, scrn, draw, &pdp->hHWDrawable)) {
	Xfree(pdp);
	return NULL;
    }

    pdp->draw = draw;
    pdp->refcount = 0;
    pdp->pStamp = NULL;
    pdp->lastStamp = 0;
    pdp->index = 0;
    pdp->x = 0;
    pdp->y = 0;
    pdp->w = 0;
    pdp->h = 0;
    pdp->numClipRects = 0;
    pdp->numBackClipRects = 0;
    pdp->pClipRects = NULL;
    pdp->pBackClipRects = NULL;

    if (!(pDRIScreen = __glXFindDRIScreen(dpy, scrn))) {
	(void)XF86DRIDestroyDrawable(dpy, scrn, pdp->draw);
	Xfree(pdp);
	return NULL;
    } else if (!(psp = (__DRIscreenPrivate *)pDRIScreen->private)) {
	(void)XF86DRIDestroyDrawable(dpy, scrn, pdp->draw);
	Xfree(pdp);
	return NULL;
    }
    pdp->driScreenPriv = psp;
    pdp->driContextPriv = &psp->dummyContextPriv;

    config = __driFindGlxConfig(dpy, scrn, vid);
    if (!config)
        return NULL;

    /* convert GLXvisualConfig struct to GLcontextModes struct */
    __glXFormatGLModes(&modes, config);

    if (!(*psp->DriverAPI.CreateBuffer)(dpy, psp, pdp, &modes, isPixmap)) {
       (void)XF86DRIDestroyDrawable(dpy, scrn, pdp->draw);
       Xfree(pdp);
       return NULL;
    }

    pdraw->destroyDrawable = driDestroyDrawable;
    pdraw->swapBuffers = psp->DriverAPI.SwapBuffers;

    return (void *) pdp;
}

static __DRIdrawable *driGetDrawable(Display *dpy, GLXDrawable draw,
					 void *screenPrivate)
{
    __DRIscreenPrivate *psp = (__DRIscreenPrivate *) screenPrivate;

    /*
    ** Make sure this routine returns NULL if the drawable is not bound
    ** to a direct rendering context!
    */
    return __driFindDrawable(psp->drawHash, draw);
}

static void driDestroyDrawable(Display *dpy, void *drawablePrivate)
{
    __DRIdrawablePrivate *pdp = (__DRIdrawablePrivate *) drawablePrivate;
    __DRIscreenPrivate *psp = pdp->driScreenPriv;
    int scrn = psp->myNum;

    if (pdp) {
        (*psp->DriverAPI.DestroyBuffer)(pdp);
	if (__driWindowExists(dpy, pdp->draw))
	    (void)XF86DRIDestroyDrawable(dpy, scrn, pdp->draw);
	if (pdp->pClipRects) {
	    Xfree(pdp->pClipRects);
	    pdp->pClipRects = NULL;
	}
	if (pdp->pBackClipRects) {
	    Xfree(pdp->pBackClipRects);
	    pdp->pBackClipRects = NULL;
	}
	Xfree(pdp);
    }
}

/*****************************************************************/

static void driDestroyContext(Display *dpy, int scrn, void *contextPrivate)
{
    __DRIcontextPrivate  *pcp   = (__DRIcontextPrivate *) contextPrivate;
    __DRIscreenPrivate   *psp;
    __DRIdrawablePrivate *pdp;

    if (pcp) {
  	if ((pdp = pcp->driDrawablePriv)) {
 	    /* Shut down fullscreen mode */
 	    if ((psp = pdp->driScreenPriv) && psp->fullscreen) {
 		psp->DriverAPI.CloseFullScreen(pcp);
 		XF86DRICloseFullScreen(dpy, scrn, pdp->draw);
 		psp->fullscreen = NULL;
 	    }
	}
	(*pcp->driScreenPriv->DriverAPI.DestroyContext)(pcp);
	__driGarbageCollectDrawables(pcp->driScreenPriv->drawHash);
	(void)XF86DRIDestroyContext(dpy, scrn, pcp->contextID);
	Xfree(pcp);
    }
}

static void *driCreateContext(Display *dpy, XVisualInfo *vis,
                              void *sharedPrivate,
                              __DRIcontext *pctx)
{
    __DRIscreen *pDRIScreen;
    __DRIcontextPrivate *pcp;
    __DRIcontextPrivate *pshare = (__DRIcontextPrivate *) sharedPrivate;
    __DRIscreenPrivate *psp;
    __GLXvisualConfig *config;
    __GLcontextModes modes;
    void *shareCtx;

    if (!(pDRIScreen = __glXFindDRIScreen(dpy, vis->screen))) {
	/* ERROR!!! */
	return NULL;
    } else if (!(psp = (__DRIscreenPrivate *)pDRIScreen->private)) {
	/* ERROR!!! */
	return NULL;
    }

    if (!psp->dummyContextPriv.driScreenPriv) {
	if (!XF86DRICreateContext(dpy, vis->screen, vis->visual,
				  &psp->dummyContextPriv.contextID,
				  &psp->dummyContextPriv.hHWContext)) {
	    return NULL;
	}
	psp->dummyContextPriv.driScreenPriv = psp;
	psp->dummyContextPriv.driDrawablePriv = NULL;
        psp->dummyContextPriv.driverPrivate = NULL;
	/* No other fields should be used! */
    }

    /* Create the hash table */
    if (!psp->drawHash) psp->drawHash = drmHashCreate();

    pcp = (__DRIcontextPrivate *)Xmalloc(sizeof(__DRIcontextPrivate));
    if (!pcp) {
	return NULL;
    }

    pcp->display = dpy;
    pcp->driScreenPriv = psp;
    pcp->driDrawablePriv = NULL;

    if (!XF86DRICreateContext(dpy, vis->screen, vis->visual,
			      &pcp->contextID, &pcp->hHWContext)) {
	Xfree(pcp);
	return NULL;
    }

    /* This is moved because the Xserver creates a global dummy context
     * the first time XF86DRICreateContext is called.
     */

    if (!psp->dummyContextPriv.driScreenPriv) {
#if 0
	/* We no longer use this cause we have the shared dummyContext
	 * in the SAREA.
	 */
	if (!XF86DRICreateContext(dpy, vis->screen, vis->visual,
				  &psp->dummyContextPriv.contextID,
				  &psp->dummyContextPriv.hHWContext)) {
	    return NULL;
	}
#endif
        psp->dummyContextPriv.hHWContext = psp->pSAREA->dummy_context;
	psp->dummyContextPriv.driScreenPriv = psp;
	psp->dummyContextPriv.driDrawablePriv = NULL;
        psp->dummyContextPriv.driverPrivate = NULL;
	/* No other fields should be used! */
    }

    /* Setup a __GLcontextModes struct corresponding to vis->visualid
     * and create the rendering context.
     */
    config = __driFindGlxConfig(dpy, vis->screen, vis->visualid);
    if (!config)
        return NULL;

    __glXFormatGLModes(&modes, config);
    shareCtx = pshare ? pshare->driverPrivate : NULL;
    if (!(*psp->DriverAPI.CreateContext)(dpy, &modes, pcp, shareCtx)) {
        (void)XF86DRIDestroyContext(dpy, vis->screen, pcp->contextID);
        Xfree(pcp);
        return NULL;
    }

    pctx->destroyContext = driDestroyContext;
    pctx->bindContext    = driBindContext;
    pctx->unbindContext  = driUnbindContext;

    __driGarbageCollectDrawables(pcp->driScreenPriv->drawHash);

    return pcp;
}

/*****************************************************************/

static void driDestroyScreen(Display *dpy, int scrn, void *screenPrivate)
{
    __DRIscreenPrivate *psp = (__DRIscreenPrivate *) screenPrivate;

    if (psp) {
#if 0
	/*
	** NOT_DONE: For the same reason as that listed below, we cannot
	** call the X server here to destroy the dummy context.
	*/
	if (psp->dummyContextPriv.driScreenPriv) {
	    (void)XF86DRIDestroyContext(dpy, scrn,
					psp->dummyContextPriv.contextID);
	}
#endif
	if (psp->DriverAPI.DestroyScreen)
	    (*psp->DriverAPI.DestroyScreen)(psp);

	(void)drmUnmap((drmAddress)psp->pSAREA, SAREA_MAX);
	(void)drmUnmap((drmAddress)psp->pFB, psp->fbSize);
	Xfree(psp->pDevPriv);
	(void)drmClose(psp->fd);
	Xfree(psp);

#if 0
	/*
	** NOT_DONE: Normally, we would call XF86DRICloseConnection()
	** here, but since this routine is called after the
	** XCloseDisplay() function has already shut down the connection
	** to the Display, there is no protocol stream open to the X
	** server anymore.  Luckily, XF86DRICloseConnection() does not
	** really do anything (for now).
	*/
	(void)XF86DRICloseConnection(dpy, scrn);
#endif
    }
}


__DRIscreenPrivate *
__driUtilCreateScreen(Display *dpy, int scrn, __DRIscreen *psc,
                      int numConfigs, __GLXvisualConfig *config,
                      const struct __DriverAPIRec *driverAPI)
{
    int directCapable;
    __DRIscreenPrivate *psp;
    drmHandle hFB, hSAREA;
    char *BusID, *driverName;
    drmMagic magic;

    if (!XF86DRIQueryDirectRenderingCapable(dpy, scrn, &directCapable)) {
	return NULL;
    }

    if (!directCapable) {
	return NULL;
    }

    psp = (__DRIscreenPrivate *)Xmalloc(sizeof(__DRIscreenPrivate));
    if (!psp) {
	return NULL;
    }
    psp->fullscreen = NULL;
    psp->display = dpy;
    psp->myNum = scrn;

    if (!XF86DRIOpenConnection(dpy, scrn, &hSAREA, &BusID)) {
	Xfree(psp);
	return NULL;
    }

    /*
    ** NOT_DONE: This is used by the X server to detect when the client
    ** has died while holding the drawable lock.  The client sets the
    ** drawable lock to this value.
    */
    psp->drawLockID = 1;

    psp->fd = drmOpen(NULL,BusID);
    if (psp->fd < 0) {
        fprintf(stderr, "libGL error: failed to open DRM: %s\n", strerror(-psp->fd));
        fprintf(stderr, "libGL error: reverting to (slow) indirect rendering\n");
	Xfree(BusID);
	Xfree(psp);
	(void)XF86DRICloseConnection(dpy, scrn);
	return NULL;
    }
    Xfree(BusID); /* No longer needed */

    if (drmGetMagic(psp->fd, &magic)) {
        fprintf(stderr, "libGL error: drmGetMagic failed\n");
	(void)drmClose(psp->fd);
	Xfree(psp);
	(void)XF86DRICloseConnection(dpy, scrn);
	return NULL;
    }

    {
        drmVersionPtr version = drmGetVersion(psp->fd);
        if (version) {
            psp->drmMajor = version->version_major;
            psp->drmMinor = version->version_minor;
            psp->drmPatch = version->version_patchlevel;
            drmFreeVersion(version);
        }
        else {
            psp->drmMajor = -1;
            psp->drmMinor = -1;
            psp->drmPatch = -1;
        }
    }

    if (!XF86DRIAuthConnection(dpy, scrn, magic)) {
        fprintf(stderr, "libGL error: XF86DRIAuthConnection failed\n");
	(void)drmClose(psp->fd);
	Xfree(psp);
	(void)XF86DRICloseConnection(dpy, scrn);
	return NULL;
    }

    /*
     * Get device name (like "tdfx") and the ddx version numbers.
     * We'll check the version in each DRI driver's "createScreen"
     * function.
     */
    if (!XF86DRIGetClientDriverName(dpy, scrn,
				    &psp->ddxMajor,
				    &psp->ddxMinor,
				    &psp->ddxPatch,
				    &driverName)) {
        fprintf(stderr, "libGL error: XF86DRIGetClientDriverName failed\n");
	(void)drmClose(psp->fd);
	Xfree(psp);
	(void)XF86DRICloseConnection(dpy, scrn);
	return NULL;
    }

    /* install driver's callback functions */
    memcpy(&psp->DriverAPI, driverAPI, sizeof(struct __DriverAPIRec));

    /*
     * Get device-specific info.  pDevPriv will point to a struct
     * (such as DRIRADEONRec in xfree86/driver/ati/radeon_dri.h)
     * that has information about the screen size, depth, pitch,
     * ancilliary buffers, DRM mmap handles, etc.
     */
    if (!XF86DRIGetDeviceInfo(dpy, scrn,
			      &hFB,
			      &psp->fbOrigin,
			      &psp->fbSize,
			      &psp->fbStride,
			      &psp->devPrivSize,
			      &psp->pDevPriv)) {
        fprintf(stderr, "libGL error: XF86DRIGetDeviceInfo failed\n");
	(void)drmClose(psp->fd);
	Xfree(psp);
	(void)XF86DRICloseConnection(dpy, scrn);
	return NULL;
    }
    psp->fbWidth = DisplayWidth(dpy, scrn);
    psp->fbHeight = DisplayHeight(dpy, scrn);
    psp->fbBPP = 32; /* NOT_DONE: Get this from X server */

    /*
     * Map the framebuffer region.
     */
    if (drmMap(psp->fd, hFB, psp->fbSize, (drmAddressPtr)&psp->pFB)) {
        fprintf(stderr, "libGL error: drmMap of framebuffer failed\n");
	Xfree(psp->pDevPriv);
	(void)drmClose(psp->fd);
	Xfree(psp);
	(void)XF86DRICloseConnection(dpy, scrn);
	return NULL;
    }

    /*
     * Map the SAREA region.  Further mmap regions may be setup in
     * each DRI driver's "createScreen" function.
     */
    if (drmMap(psp->fd, hSAREA, SAREA_MAX, (drmAddressPtr)&psp->pSAREA)) {
        fprintf(stderr, "libGL error: drmMap of sarea failed\n");
	(void)drmUnmap((drmAddress)psp->pFB, psp->fbSize);
	Xfree(psp->pDevPriv);
	(void)drmClose(psp->fd);
	Xfree(psp);
	(void)XF86DRICloseConnection(dpy, scrn);
	return NULL;
    }

    /* Initialize the screen specific GLX driver */
    if (psp->DriverAPI.InitDriver) {
	if (!(*psp->DriverAPI.InitDriver)(psp)) {
	    fprintf(stderr, "libGL error: InitDriver failed\n");
	    (void)drmUnmap((drmAddress)psp->pSAREA, SAREA_MAX);
	    (void)drmUnmap((drmAddress)psp->pFB, psp->fbSize);
	    Xfree(psp->pDevPriv);
	    (void)drmClose(psp->fd);
	    Xfree(psp);
	    (void)XF86DRICloseConnection(dpy, scrn);
	    return NULL;
	}
    }

    /*
    ** Do not init dummy context here; actual initialization will be
    ** done when the first DRI context is created.  Init screen priv ptr
    ** to NULL to let CreateContext routine that it needs to be inited.
    */
    psp->dummyContextPriv.driScreenPriv = NULL;

    /* Initialize the drawHash when the first context is created */
    psp->drawHash = NULL;

    psc->destroyScreen  = driDestroyScreen;
    psc->createContext  = driCreateContext;
    psc->createDrawable = driCreateDrawable_dummy;
    psc->getDrawable    = driGetDrawable;

    return psp;
}


#endif
