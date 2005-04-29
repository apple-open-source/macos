/* $XFree86: xc/lib/GL/dri/dri_util.c,v 1.11 2003/11/13 17:22:49 dawes Exp $ */
/**
 * \file dri_util.c
 * DRI utility functions.
 *
 * This module acts as glue between GLX and the actual hardware driver.  A DRI
 * driver doesn't really \e have to use any of this - it's optional.  But, some
 * useful stuff is done here that otherwise would have to be duplicated in most
 * drivers.
 * 
 * Basically, these utility functions take care of some of the dirty details of
 * screen initialization, context creation, context binding, DRM setup, etc.
 *
 * These functions are compiled into each DRI driver so libGL.so knows nothing
 * about them.
 */


#ifdef GLX_DIRECT_RENDERING

#include <assert.h>
#include <stdarg.h>
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
static int driQueryFrameTracking( Display * dpy, void * priv,
    int64_t * sbc, int64_t * missedFrames, float * lastMissedUsage,
    float * usage );

static void *driCreateDrawable(Display *dpy, int scrn, GLXDrawable draw,
                               GLboolean isPixmap,
                               VisualID vid, __DRIdrawable *pdraw);

static void driDestroyDrawable(Display *dpy, void *drawablePrivate);


/**
 * Print message to \c stderr if the \c LIBGL_DEBUG environment variable
 * is set. 
 * 
 * Is called from the drivers.
 * 
 * \param f printf() like format string.
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
/** \name Visual utility functions                               */
/*****************************************************************/
/*@{*/


/**
 * Initialize a __GLcontextModes structure with the GLX parameters
 * specified by the given visual ID.  Too bad that the __GLXvisualConfig
 * and __GLcontextModes structures are so similar, but separate.
 */
static GLboolean
findConfigMode(Display *dpy, int scrn, VisualID vid, __GLcontextModes *modes)
{
    const __GLXvisualConfig *config;
    const __DRIscreen *pDRIScreen;
    const __DRIscreenPrivate *screenPriv;
    int i;

    pDRIScreen = __glXFindDRIScreen(dpy, scrn);
    if (!pDRIScreen)
        return GL_FALSE;
    screenPriv = (const __DRIscreenPrivate *) pDRIScreen->private;

    /* Search list of configs for matching vid */
    config = NULL;
    for (i = 0; i < screenPriv->numConfigs; i++) {
        if (screenPriv->configs[i].vid == vid) {
            config = screenPriv->configs + i;
            break;
        }
    }
    if (!config)
        return GL_FALSE;

    /* return mode parameters */
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

    return GL_TRUE;
}

/*@}*/


/*****************************************************************/
/** \name Drawable list management */
/*****************************************************************/
/*@{*/

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

    XSync(dpy, GL_FALSE);
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
            /* XXX someday, use libGL's __glXWindowExists() function */
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

/*@}*/


/*****************************************************************/
/** \name Context (un)binding functions                          */
/*****************************************************************/
/*@{*/


/**
 * Unbind context.
 * 
 * \param dpy the display handle.
 * \param scrn the screen number.
 * \param draw drawable.
 * \param read Current reading drawable.
 * \param gc context.
 *
 * \return GL_TRUE on success, or GL_FALSE on failure.
 * 
 * \internal
 * This function calls __DriverAPIRec::UnbindContext, and then decrements
 * __DRIdrawablePrivateRec::refcount which must be non-zero for a successful
 * return.
 * 
 * While casting the opaque private pointers associated with the parameters
 * into their respective real types it also assures they are not null. 
 */
static Bool driUnbindContext2(Display *dpy, int scrn,
			      GLXDrawable draw, GLXDrawable read,
			      GLXContext gc)
{
    __DRIscreen *pDRIScreen;
    __DRIdrawable *pdraw;
    __DRIdrawable *pread;
    __DRIcontextPrivate *pcp;
    __DRIscreenPrivate *psp;
    __DRIdrawablePrivate *pdp;
    __DRIdrawablePrivate *prp;

    /*
    ** Assume error checking is done properly in glXMakeCurrent before
    ** calling driUnbindContext2.
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

    pcp = (__DRIcontextPrivate *)gc->driContext.private;

    pdraw = __driFindDrawable(psp->drawHash, draw);
    if (!pdraw) {
	/* ERROR!!! */
	return GL_FALSE;
    }
    pdp = (__DRIdrawablePrivate *)pdraw->private;

    pread = __driFindDrawable(psp->drawHash, read);
    if (!pread) {
	/* ERROR!!! */
	return GL_FALSE;
    }
    prp = (__DRIdrawablePrivate *)pread->private;


    /* Let driver unbind drawable from context */
    (*psp->DriverAPI.UnbindContext)(pcp);


    if (pdp->refcount == 0) {
	/* ERROR!!! */
	return GL_FALSE;
    }

    pdp->refcount--;

    if (prp != pdp) {
        if (prp->refcount == 0) {
	    /* ERROR!!! */
	    return GL_FALSE;
	}

	prp->refcount--;
    }

#if 0
    if (pdp->refcount == 0) {
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
    }
#endif

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


/**
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
    if ( pdp != prp ) {
	prp->refcount++;
    }

    /*
    ** Now that we have a context associated with this drawable, we can
    ** initialize the drawable information if has not been done before.
    */
    if (!pdp->pStamp || *pdp->pStamp != pdp->lastStamp) {
	DRM_SPINLOCK(&psp->pSAREA->drawable_lock, psp->drawLockID);
	__driUtilUpdateDrawableInfo(pdp);
	DRM_SPINUNLOCK(&psp->pSAREA->drawable_lock, psp->drawLockID);
    }

    /* Call device-specific MakeCurrent */
    (*psp->DriverAPI.MakeCurrent)(pcp, pdp, prp);

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


/*
 * Simply call bind with the same GLXDrawable for the read and draw buffers.
 */
static Bool driUnbindContext(Display *dpy, int scrn,
                             GLXDrawable draw, GLXContext gc,
                             int will_rebind)
{
   (void) will_rebind;
   return driUnbindContext2( dpy, scrn, draw, draw, gc );
}

/*@}*/


/*****************************************************************/
/** \name Drawable handling functions                            */
/*****************************************************************/
/*@{*/


/**
 * Update private drawable information.
 *
 * \param pdp pointer to the private drawable information to update.
 * 
 * This function basically updates the __DRIdrawablePrivate struct's
 * cliprect information by calling XF86DRIGetDrawableInfo().  This is
 * usually called by the DRI_VALIDATE_DRAWABLE_INFO macro which
 * compares the __DRIdrwablePrivate pStamp and lastStamp values.  If
 * the values are different that means we have to update the clipping
 * info.
 */
void
__driUtilUpdateDrawableInfo(__DRIdrawablePrivate *pdp)
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
	!XF86DRIGetDrawableInfo(pdp->display, pdp->screen, pdp->draw,
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

/*@}*/

/*****************************************************************/
/** \name GLX callbacks                                          */
/*****************************************************************/
/*@{*/

/**
 * Swap buffers.
 *
 * \param dpy the display handle.
 * \param drawablePrivate opaque pointer to the per-drawable private info.
 * 
 * \internal
 * This function calls __DRIdrawablePrivate::swapBuffers.
 * 
 * Is called directly from glXSwapBuffers().
 */
static void driSwapBuffers( Display *dpy, void *drawablePrivate )
{
    __DRIdrawablePrivate *dPriv = (__DRIdrawablePrivate *) drawablePrivate;
    dPriv->swapBuffers(dPriv);
    (void) dpy;
}

/**
 * Called directly from a number of higher-level GLX functions.
 */
static int driGetMSC( void *screenPrivate, int64_t *msc )
{
    __DRIscreenPrivate *sPriv = (__DRIscreenPrivate *) screenPrivate;

    return sPriv->DriverAPI.GetMSC( sPriv, msc );
}

/**
 * Called directly from a number of higher-level GLX functions.
 */
static int driGetSBC( Display *dpy, void *drawablePrivate, int64_t *sbc )
{
   __DRIdrawablePrivate *dPriv = (__DRIdrawablePrivate *) drawablePrivate;
   __DRIswapInfo  sInfo;
   int  status;


   status = dPriv->driScreenPriv->DriverAPI.GetSwapInfo( dPriv, & sInfo );
   *sbc = sInfo.swap_count;

   return status;
}

static int driWaitForSBC( Display * dpy, void *drawablePriv,
			  int64_t target_sbc,
			  int64_t * msc, int64_t * sbc )
{
    __DRIdrawablePrivate *dPriv = (__DRIdrawablePrivate *) drawablePriv;

    return dPriv->driScreenPriv->DriverAPI.WaitForSBC( dPriv, target_sbc,
                                                       msc, sbc );
}

static int driWaitForMSC( Display * dpy, void *drawablePriv,
			  int64_t target_msc,
			  int64_t divisor, int64_t remainder,
			  int64_t * msc, int64_t * sbc )
{
    __DRIdrawablePrivate *dPriv = (__DRIdrawablePrivate *) drawablePriv;
    __DRIswapInfo  sInfo;
    int  status;


    status = dPriv->driScreenPriv->DriverAPI.WaitForMSC( dPriv, target_msc,
                                                         divisor, remainder,
                                                         msc );

    /* GetSwapInfo() may not be provided by the driver if GLX_SGI_video_sync
     * is supported but GLX_OML_sync_control is not.  Therefore, don't return
     * an error value if GetSwapInfo() is not implemented.
    */
    if ( status == 0
         && dPriv->driScreenPriv->DriverAPI.GetSwapInfo ) {
        status = dPriv->driScreenPriv->DriverAPI.GetSwapInfo( dPriv, & sInfo );
        *sbc = sInfo.swap_count;
    }

    return status;
}

static int64_t driSwapBuffersMSC( Display * dpy, void *drawablePriv,
				  int64_t target_msc,
				  int64_t divisor, int64_t remainder )
{
    __DRIdrawablePrivate *dPriv = (__DRIdrawablePrivate *) drawablePriv;

    return dPriv->driScreenPriv->DriverAPI.SwapBuffersMSC( dPriv, target_msc,
                                                           divisor, 
                                                           remainder );
}

/**
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
    pdp->pdraw = pdraw;
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
    pdp->display = dpy;
    pdp->screen = scrn;

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

    if (!findConfigMode(dpy, scrn, vid, &modes))
        return NULL;

    if (!(*psp->DriverAPI.CreateBuffer)(psp, pdp, &modes, isPixmap)) {
       (void)XF86DRIDestroyDrawable(dpy, scrn, pdp->draw);
       Xfree(pdp);
       return NULL;
    }

    pdraw->destroyDrawable = driDestroyDrawable;
    pdraw->swapBuffers = driSwapBuffers;  /* called by glXSwapBuffers() */

    if ( driCompareGLXAPIVersion( 20030317 ) >= 0 ) {
        pdraw->getSBC = driGetSBC;
        pdraw->waitForSBC = driWaitForSBC;
        pdraw->waitForMSC = driWaitForMSC;
        pdraw->swapBuffersMSC = driSwapBuffersMSC;
        pdraw->frameTracking = NULL;
        pdraw->queryFrameTracking = driQueryFrameTracking;

        pdraw->swap_interval = (getenv( "LIBGL_THROTTLE_REFRESH" ) == NULL)
	    ? 0 : 1;
    }

    pdp->swapBuffers = psp->DriverAPI.SwapBuffers;

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

/*@}*/


/*****************************************************************/
/** \name Context handling functions                             */
/*****************************************************************/
/*@{*/


/**
 * Destroy the per-context private information.
 * 
 * \param dpy the display handle.
 * \param scrn the screen number.
 * \param contextPrivate opaque pointer to the per-drawable private info.
 *
 * \internal
 * This function calls __DriverAPIRec::DestroyContext on \p contextPrivate, calls
 * drmDestroyContext(), and finally frees \p contextPrivate.
 */
static void driDestroyContext(Display *dpy, int scrn, void *contextPrivate)
{
    __DRIcontextPrivate  *pcp   = (__DRIcontextPrivate *) contextPrivate;

    if (pcp) {
	(*pcp->driScreenPriv->DriverAPI.DestroyContext)(pcp);
	__driGarbageCollectDrawables(pcp->driScreenPriv->drawHash);
	(void)XF86DRIDestroyContext(dpy, scrn, pcp->contextID);
	Xfree(pcp);
    }
}

/**
 * Create the per-drawable private driver information.
 * 
 * \param dpy the display handle.
 * \param vis the visual information.
 * \param sharedPrivate the shared context dependent methods or NULL if non-existent.
 * \param pctx will receive the context dependent methods.
 *
 * \returns a opaque pointer to the per-context private information on success, or NULL
 * on failure.
 * 
 * \internal
 * This function allocates and fills a __DRIcontextPrivateRec structure.  It
 * gets the visual, converts it into a __GLcontextModesRec and passes it
 * to __DriverAPIRec::CreateContext to create the context.
 */
static void *driCreateContext(Display *dpy, XVisualInfo *vis,
                              void *sharedPrivate, __DRIcontext *pctx)
{
    __DRIscreen *pDRIScreen;
    __DRIcontextPrivate *pcp;
    __DRIcontextPrivate *pshare = (__DRIcontextPrivate *) sharedPrivate;
    __DRIscreenPrivate *psp;
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
    if (!findConfigMode(dpy, vis->screen, vis->visualid, &modes))
        return NULL;

    shareCtx = pshare ? pshare->driverPrivate : NULL;
    if (!(*psp->DriverAPI.CreateContext)(&modes, pcp, shareCtx)) {
        (void)XF86DRIDestroyContext(dpy, vis->screen, pcp->contextID);
        Xfree(pcp);
        return NULL;
    }

    pctx->destroyContext = driDestroyContext;
    pctx->bindContext    = driBindContext;
    pctx->unbindContext  = driUnbindContext;
    if ( driCompareGLXAPIVersion( 20030606 ) >= 0 ) {
        pctx->bindContext2   = driBindContext2;
        pctx->unbindContext2 = driUnbindContext2;
    }

    __driGarbageCollectDrawables(pcp->driScreenPriv->drawHash);

    return pcp;
}

/*@}*/


/*****************************************************************/
/** \name Screen handling functions                              */
/*****************************************************************/
/*@{*/


/**
 * Destroy the per-screen private information.
 * 
 * \param dpy the display handle.
 * \param scrn the screen number.
 * \param screenPrivate opaque pointer to the per-screen private information.
 *
 * \internal
 * This function calls __DriverAPIRec::DestroyScreen on \p screenPrivate, calls
 * drmClose(), and finally frees \p screenPrivate.
 */
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
                      int numConfigs, __GLXvisualConfig *configs,
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
    psp->display = dpy;
    psp->myNum = scrn;
    psp->psc = psc;

    psp->numConfigs = numConfigs;
    psp->configs = configs;

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

    /*
     * Get the DRI X extension version.
     */
    if (!XF86DRIQueryVersion(dpy,
			     &psp->driMajor,
			     &psp->driMinor,
			     &psp->driPatch)) {
        fprintf(stderr, "libGL error: XF86DRIQueryVersion failed\n");
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
    if ( driCompareGLXAPIVersion( 20030317 ) >= 0 )
        psc->getMSC     = driGetMSC;

    return psp;
}


/**
 * Compare the current GLX API version with a driver supplied required version.
 * 
 * The minimum required version is compared with the API version exported by
 * the \c __glXGetInternalVersion function (in libGL.so).
 * 
 * \param   required_version Minimum required internal GLX API version.
 * \return  A tri-value return, as from strcmp is returned.  A value less
 *          than, equal to, or greater than zero will be returned if the
 *          internal GLX API version is less than, equal to, or greater
 *          than \c required_version.
 *
 * \sa __glXGetInternalVersion().
 */
int driCompareGLXAPIVersion( GLuint required_version )
{
   static GLuint  api_ver = 0;


   if ( api_ver == 0 ) {
      PFNGLXGETINTERNALVERSIONPROC get_ver;

      get_ver = (PFNGLXGETINTERNALVERSIONPROC)
	  glXGetProcAddress( (const GLubyte *) "__glXGetInternalVersion" );
      api_ver = ( get_ver != NULL ) ? get_ver() : 1;
   }


   if ( api_ver > required_version ) {
      return 1;
   }
   else if ( api_ver == required_version ) {
      return 0;
   }

   return -1;
}


static int
driQueryFrameTracking( Display * dpy, void * priv,
		       int64_t * sbc, int64_t * missedFrames,
		       float * lastMissedUsage, float * usage )
{
   static PFNGLXGETUSTPROC   get_ust;
   __DRIswapInfo   sInfo;
   int             status;
   int64_t         ust;
   __DRIdrawablePrivate * dpriv = (__DRIdrawablePrivate *) priv;

   if ( get_ust == NULL ) {
      get_ust = (PFNGLXGETUSTPROC) glXGetProcAddress( (const GLubyte *) "__glXGetUST" );
   }

   status = dpriv->driScreenPriv->DriverAPI.GetSwapInfo( dpriv, & sInfo );
   if ( status == 0 ) {
      *sbc = sInfo.swap_count;
      *missedFrames = sInfo.swap_missed_count;
      *lastMissedUsage = sInfo.swap_missed_usage;

      (*get_ust)( & ust );
      *usage = driCalculateSwapUsage( dpriv, sInfo.swap_ust, ust );
   }

   return status;
}


/**
 * Calculate amount of swap interval used between GLX buffer swaps.
 * 
 * The usage value, on the range [0,max], is the fraction of total swap
 * interval time used between GLX buffer swaps is calculated.
 *
 *            \f$p = t_d / (i * t_r)\f$
 * 
 * Where \f$t_d\$f is the time since the last GLX buffer swap, \f$i\f$ is the
 * swap interval (as set by \c glXSwapIntervalSGI), and \f$t_r\f$ time
 * required for a single vertical refresh period (as returned by \c
 * glXGetMscRateOML).
 * 
 * See the documentation for the GLX_MESA_swap_frame_usage extension for more
 * details.
 *
 * \param   dPriv  Pointer to the private drawable structure.
 * \return  If less than a single swap interval time period was required
 *          between GLX buffer swaps, a number greater than 0 and less than
 *          1.0 is returned.  If exactly one swap interval time period is
 *          required, 1.0 is returned, and if more than one is required then
 *          a number greater than 1.0 will be returned.
 *
 * \sa glXSwapIntervalSGI(), glXGetMscRateOML().
 */
float
driCalculateSwapUsage( __DRIdrawablePrivate *dPriv, int64_t last_swap_ust,
		       int64_t current_ust )
{
   static PFNGLXGETMSCRATEOMLPROC get_msc_rate = NULL;
   int32_t   n;
   int32_t   d;
   int       interval;
   float     usage = 1.0;


   /* FIXME: Instead of caching the function, would it be possible to
    * FIXME: cache the sync rate?
    */
   if ( get_msc_rate == NULL ) {
      get_msc_rate = (PFNGLXGETMSCRATEOMLPROC)
	  glXGetProcAddress( (const GLubyte *) "glXGetMscRateOML" );
   }
   
   if ( (get_msc_rate != NULL)
	&& get_msc_rate( dPriv->display, dPriv->draw, &n, &d ) ) {
      interval = (dPriv->pdraw->swap_interval != 0)
	  ? dPriv->pdraw->swap_interval : 1;


      /* We want to calculate
       * (current_UST - last_swap_UST) / (interval * us_per_refresh).  We get
       * current_UST by calling __glXGetUST.  last_swap_UST is stored in
       * dPriv->swap_ust.  interval has already been calculated.
       *
       * The only tricky part is us_per_refresh.  us_per_refresh is
       * 1000000 / MSC_rate.  We know the MSC_rate is n / d.  We can flip it
       * around and say us_per_refresh = 1000000 * d / n.  Since this goes in
       * the denominator of the final calculation, we calculate
       * (interval * 1000000 * d) and move n into the numerator.
       */

      usage = (current_ust - last_swap_ust);
      usage *= n;
      usage /= (interval * d);
      usage /= 1000000.0;
   }
   
   return usage;
}

/*@}*/

#endif
