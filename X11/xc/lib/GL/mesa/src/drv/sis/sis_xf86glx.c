/* $XFree86: xc/lib/GL/mesa/src/drv/sis/sis_xf86glx.c,v 1.1 2000/06/30 19:30:29 dawes Exp $ */
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
 *
 * $PI: xc/programs/Xserver/GL/mesa/src/X/xf86glx.c,v 1.15 1999/08/04 18:14:14 faith Exp $
 */

#include <miscstruct.h>
#include <resource.h>
#include <GL/gl.h>
#include <GL/glxint.h>
#include <GL/glxtokens.h>
#include <scrnintstr.h>
#include <config.h>
#include <glxserver.h>
#include <glxscreens.h>
#include <glxdrawable.h>
#include <glxcontext.h>
#include <glxext.h>
#include <glxutil.h>
#include "xf86glxint.h"
#include "xmesaP.h"
#include <GL/xf86glx.h>

/*
 * This define is for the glcore.h header file.
 * If you add it here, then make sure you also add it in
 * ../../../glx/Imakefile.
 */
#if 0
#define DEBUG
#include <GL/internal/glcore.h>
#undef DEBUG
#else
#include <GL/internal/glcore.h>
#endif


/*
 * This structure is statically allocated in the __glXScreens[]
 * structure.  This struct is not used anywhere other than in
 * __glXScreenInit to initialize each of the active screens
 * (__glXActiveScreens[]).  Several of the fields must be initialized by
 * the screenProbe routine before they are copied to the active screens
 * struct.  In particular, the contextCreate, pGlxVisual, numVisuals,
 * and numUsableVisuals fields must be initialized.
 */
__GLXscreenInfo __glDDXScreenInfo = {
    __MESA_screenProbe,   /* Must be generic and handle all screens */
    __MESA_createContext, /* Substitute screen's createContext routine */
    __MESA_createBuffer,  /* Substitute screen's createBuffer routine */
    NULL,                 /* Set up pGlxVisual in probe */
    NULL,                 /* Set up pVisualPriv in probe */
    0,                    /* Set up numVisuals in probe */
    0,                    /* Set up numUsableVisuals in probe */
    "Vendor String",      /* GLXvendor is overwritten by __glXScreenInit */
    "Version String",     /* GLXversion is overwritten by __glXScreenInit */
    "Extensions String",  /* GLXextensions is overwritten by __glXScreenInit */
    NULL                  /* WrappedPositionWindow is overwritten */
};

__GLXextensionInfo __glDDXExtensionInfo = {
    GL_CORE_MESA,
    __MESA_resetExtension,
    __MESA_initVisuals,
    __MESA_setVisualConfigs
};

__MESA_screen  MESAScreens[MAXSCREENS];
__GLcontext   *MESA_CC        = NULL;

int                 numConfigs     = 0;
__GLXvisualConfig  *visualConfigs  = NULL;
void              **visualPrivates = NULL;

static int count_bits(unsigned int n)
{
   int bits = 0;

   while (n > 0) {
      if (n & 1) bits++;
      n >>= 1;
   }
   return bits;
}

static XMesaVisual find_mesa_visual(int screen, VisualID vid)
{
    XMesaVisual xm_vis = NULL;
    __MESA_screen *pMScr = &MESAScreens[screen];
    int i;

    for (i = 0; i < pMScr->num_vis; i++) {
	if (pMScr->glx_vis[i].vid == vid) {
	    break;
	}
    }

    if (i < pMScr->num_vis) {
	xm_vis = pMScr->xm_vis[i];
    }
    return xm_vis;
}

#define VISUAL_CONFIG(rgba,accum,back,depth,stencil,rating)                  \
{                                                                            \
    -1,                 /* vid */                                            \
    -1,                 /* class */                                          \
    rgba,               /* rgba */                                           \
    -1, -1, -1, 0,      /* rgba sizes, alpha not supported, yet */           \
    -1, -1, -1, 0,      /* rgba masks, alpha not supported, yet */           \
    accum, accum, accum, accum, /* rgba accum sizes */                       \
    back,               /* doubleBuffer */                                   \
    GL_FALSE,           /* stereo */                                         \
    -1,                 /* bufferSize */                                     \
    depth,              /* depthSize */                                      \
    stencil,            /* stencilSize */                                    \
    0,                  /* auxBuffers */                                     \
    0,                  /* level */                                          \
    rating,             /* visualRating */                                   \
    0,                  /* transparentPixel */                               \
    0, 0, 0, 0,         /* transparent rgba color (floats scaled to ints) */ \
    0                   /* transparentIndex */                               \
}

#define IS_RGBA     GL_TRUE
#define IS_CI       GL_FALSE
#define HAS_ACCUM   ACCUM_BITS
#define NO_ACCUM    0
#define HAS_BACK    GL_TRUE
#define NO_BACK     GL_FALSE
#define HAS_DEPTH   DEPTH_BITS
#define NO_DEPTH    0
#define HAS_STENCIL STENCIL_BITS
#define NO_STENCIL  0

static __GLXvisualConfig __MESAvisualConfigs[] = {
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM,  NO_BACK,  0,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM,  NO_BACK,  16,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM,  NO_BACK,  32,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM,  NO_BACK,  24,  8, 0),

    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM, HAS_BACK,  0,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM, HAS_BACK,  16,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM, HAS_BACK,  32,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM, HAS_BACK,  24,  8, 0),

    VISUAL_CONFIG(IS_RGBA,  HAS_ACCUM,  NO_BACK,  0,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  HAS_ACCUM,  NO_BACK,  16,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  HAS_ACCUM,  NO_BACK,  32,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  HAS_ACCUM,  NO_BACK,  24,  8, 0),

    VISUAL_CONFIG(IS_RGBA,  HAS_ACCUM, HAS_BACK,  0,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  HAS_ACCUM, HAS_BACK,  16,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  HAS_ACCUM, HAS_BACK,  32,  0, 0),
    VISUAL_CONFIG(IS_RGBA,  HAS_ACCUM, HAS_BACK,  24,  8, 0),
/*
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM,  NO_BACK,  NO_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM,  NO_BACK, HAS_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM, HAS_BACK,  NO_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM, HAS_BACK, HAS_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM,  NO_BACK,  NO_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM, HAS_BACK, HAS_DEPTH, HAS_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM, HAS_BACK, HAS_DEPTH, HAS_STENCIL, 0),
*/
};

/*
static __GLXvisualConfig __MESAvisualConfigs[] = {
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM,  NO_BACK,  NO_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM,  NO_BACK, HAS_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM, HAS_BACK,  NO_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM, HAS_BACK, HAS_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM,  NO_BACK,  NO_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(IS_RGBA,  NO_ACCUM, HAS_BACK, HAS_DEPTH, HAS_STENCIL, 0),
    VISUAL_CONFIG(IS_RGBA, HAS_ACCUM, HAS_BACK, HAS_DEPTH, HAS_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM,  NO_BACK,  NO_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM,  NO_BACK, HAS_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM, HAS_BACK,  NO_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM, HAS_BACK, HAS_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM,  NO_BACK,  NO_DEPTH,  NO_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM, HAS_BACK, HAS_DEPTH, HAS_STENCIL, 0),
    VISUAL_CONFIG(  IS_CI,  NO_ACCUM, HAS_BACK, HAS_DEPTH, HAS_STENCIL, 0),
};
*/

static int __numMESAvisualConfigs =
                    sizeof(__MESAvisualConfigs)/sizeof(__GLXvisualConfig);
static int __numRGBconfigs        = -1;
static int __numCIconfigs         = -1;

static Bool init_visuals(int *nvisualp, VisualPtr *visualp,
			 VisualID *defaultVisp,
			 int ndepth, DepthPtr pdepth,
			 int rootDepth)
{
    int numVisuals = *nvisualp;
    int numMesaVisuals = 0;
    int numMergedVisualConfigs = 0;
    VisualPtr pVisual = *visualp;
    VisualPtr pVisualNew = NULL;
    VisualID *orig_vid = NULL;
    __GLXvisualConfig *glXVisualPtr = NULL;
    __GLXvisualConfig *pMergedVisualConfigs = NULL;
    void **glXVisualPriv;
    void **pMergedVisualPriv;
    int *mesaConfigUsed;
    int *driverConfigUsed;
    int found_default = FALSE;
    int i, j, k;
    int is_rgb;
    Bool match;

    /* Alloc space for the list of merged GLX visuals */
    pMergedVisualConfigs =
	(__GLXvisualConfig *)__glXMalloc((numConfigs +
					  __numMESAvisualConfigs) * 
					 sizeof(__GLXvisualConfig));
    if (!pMergedVisualConfigs) {
	return FALSE;
    }

    /* Alloc space for the list of merged GLX visual privates */
    pMergedVisualPriv =
	(void **)__glXMalloc((numConfigs + __numMESAvisualConfigs) *
			     sizeof(void *));
    if (!pMergedVisualPriv) {
	__glXFree(pMergedVisualConfigs);
	return FALSE;
    }

    /* Compute the intersection of the driver's visual configs */
    mesaConfigUsed = __glXCalloc(__numMESAvisualConfigs, sizeof(int));
    driverConfigUsed = __glXCalloc(numConfigs, sizeof(int));

    for (i = j = 0; i < numConfigs; i++) {
	k = 0;
	while ((k < __numMESAvisualConfigs) && (!driverConfigUsed[i])) {
	    if (!mesaConfigUsed[k]) {

#ifdef DEBUG_VISUAL_CONFIG
#define TEST_AND_COPY(fld) do {						    \
	if (match) {							    \
	    if ((__MESAvisualConfigs[k].fld == visualConfigs[i].fld) ||	    \
		(__MESAvisualConfigs[k].fld == -1)) {		    	    \
		pMergedVisualConfigs[j].fld = visualConfigs[i].fld; 	    \
	    } 								    \
	    else if (visualConfigs[i].fld == -1) {			    \
		pMergedVisualConfigs[j].fld = __MESAvisualConfigs[k].fld;   \
	    }								    \
	    else {							    \
		match = FALSE;						    \
		xf86DrvMsg (0, 0, "[GLXVisualInit] mismatch: "		    \
		    "(%s) DriverVisualConfig[%d] MesaVisualConfig[%d]\n",   \
		    #fld, i, k);					    \
	    } 								    \
	} 								    \
    } while (0)
#else
#define TEST_AND_COPY(fld) do {						    \
	if (match) {							    \
	    if ((__MESAvisualConfigs[k].fld == visualConfigs[i].fld) ||	    \
		(__MESAvisualConfigs[k].fld == -1)) {		    	    \
		pMergedVisualConfigs[j].fld = visualConfigs[i].fld; 	    \
	    } 								    \
	    else if (visualConfigs[i].fld == -1) {			    \
		pMergedVisualConfigs[j].fld = __MESAvisualConfigs[k].fld;   \
	    }								    \
	    else {							    \
		match = FALSE;						    \
	    } 								    \
	} 								    \
    } while (0)
#endif

		match = TRUE;
		TEST_AND_COPY(class);
		TEST_AND_COPY(rgba);
		TEST_AND_COPY(redSize);
		TEST_AND_COPY(greenSize);
		TEST_AND_COPY(blueSize);
		TEST_AND_COPY(alphaSize);
		TEST_AND_COPY(redMask);
		TEST_AND_COPY(greenMask);
		TEST_AND_COPY(blueMask);
		TEST_AND_COPY(alphaMask);
		TEST_AND_COPY(accumRedSize);
		TEST_AND_COPY(accumGreenSize);
		TEST_AND_COPY(accumBlueSize);
		TEST_AND_COPY(accumAlphaSize);
		TEST_AND_COPY(doubleBuffer);
		TEST_AND_COPY(stereo);
		TEST_AND_COPY(bufferSize);
		TEST_AND_COPY(depthSize);
		TEST_AND_COPY(stencilSize);
		TEST_AND_COPY(auxBuffers);
		TEST_AND_COPY(level);
		TEST_AND_COPY(visualRating);
		TEST_AND_COPY(transparentPixel);
		TEST_AND_COPY(transparentRed);
		TEST_AND_COPY(transparentGreen);
		TEST_AND_COPY(transparentBlue);
		TEST_AND_COPY(transparentAlpha);
		TEST_AND_COPY(transparentIndex);
		if (match) {
		    driverConfigUsed[i] = TRUE;
		    mesaConfigUsed[k] = TRUE;
		    pMergedVisualPriv[j] = visualPrivates[i];
		    j++;
#ifdef DEBUG_VISUAL_CONFIG
		    xf86DrvMsg (0, 0, "[GLXVisualInit] MATCH: "
			"DriverVisualConfig[%d] MesaVisualConfig[%d]\n", i, k);
#endif
		}
	    }
	    k++;
	}
    }

    /*
    ** If SetVisualConfigs was not called, then just make all of Mesa's
    ** visual configs available.
    */
    if (!numConfigs) {
	memcpy(pMergedVisualConfigs, __MESAvisualConfigs,
	       sizeof(__GLXvisualConfig) * __numMESAvisualConfigs);
	memset(pMergedVisualPriv, 0, sizeof(void *) * __numMESAvisualConfigs);
	memset(mesaConfigUsed, TRUE, sizeof(int) * __numMESAvisualConfigs);
	j = __numMESAvisualConfigs;
    }

    /*
    ** This code is not currently used.  When the visual caveat
    ** extension is supported by the DRI and Mesa, we can take advantage
    ** of this code.
    */
#if 0
    /* Add any unclaimed MESA configs w/ slow caveat */
    for (i = 0; i < __numMESAvisualConfigs; i++) {
	if (!mesaConfigUsed[i]) {
	    memcpy(&pMergedVisualConfigs[j], &__MESAvisualConfigs[i],
		   sizeof(__GLXvisualConfig));
	    pMergedVisualConfigs[j].visualRating = GLX_SLOW_VISUAL_EXT;
	    j++;
#ifdef DEBUG_VISUAL_CONFIG
	    xf86DrvMsg (0, 0, "[GLXVisualInit] slow config: "
				    "MesaVisualConfig[%d]\n", i);
#endif
	}
    }

    /* Add any unclaimed driver configs w/ nonconformant caveat */
    for (i = 0; i < numConfigs; i++) {
	if (!driverConfigUsed[i]) {
	    memcpy(&pMergedVisualConfigs[j], &visualConfigs[i],
		   sizeof(__GLXvisualConfig));
	    pMergedVisualConfigs[j].visualRating = GLX_NON_CONFORMANT_VISUAL_EXT;
	    j++;
#ifdef DEBUG_VISUAL_CONFIG
	    xf86DrvMsg (0, 0, "[GLXVisualInit] non-conformant config: "
				    "DriverVisualConfig[%d]\n", i);
#endif
	}
    }
#endif

    numMergedVisualConfigs = j;

    /* Count the number of RGB and CI visual configs */
    __numRGBconfigs = __numCIconfigs = 0;
    for (i = 0; i < numMergedVisualConfigs; i++) {
	if (pMergedVisualConfigs[i].rgba)
	    __numRGBconfigs++;
	else
	    __numCIconfigs++;
    }

    /* Count the total number of visuals to compute */
    for (i = 0; i < numVisuals; i++) {
        numMesaVisuals +=
	    (pVisual[i].class == TrueColor || pVisual[i].class == DirectColor)
	    ? __numRGBconfigs : __numCIconfigs;
    }

    /* Reset variables for use with the next screen/driver's visual configs */
    visualConfigs = NULL;
    numConfigs = 0;

    /* Alloc temp space for the list of orig VisualIDs for each new visual */
    orig_vid = (VisualID *)__glXMalloc(numMesaVisuals * sizeof(VisualID));
    if (!orig_vid) {
	__glXFree(pMergedVisualPriv);
	__glXFree(pMergedVisualConfigs);
	return FALSE;
    }

    /* Alloc space for the list of glXVisuals */
    glXVisualPtr = (__GLXvisualConfig *)__glXMalloc(numMesaVisuals *
						    sizeof(__GLXvisualConfig));
    if (!glXVisualPtr) {
	__glXFree(orig_vid);
	__glXFree(pMergedVisualPriv);
	__glXFree(pMergedVisualConfigs);
	return FALSE;
    }

    /* Alloc space for the list of glXVisualPrivates */
    glXVisualPriv = (void **)__glXMalloc(numMesaVisuals * sizeof(void *));
    if (!glXVisualPriv) {
	__glXFree(glXVisualPtr);
	__glXFree(orig_vid);
	__glXFree(pMergedVisualPriv);
	__glXFree(pMergedVisualConfigs);
	return FALSE;
    }

    /* Alloc space for the new list of the X server's visuals */
    pVisualNew = (VisualPtr)__glXMalloc(numMesaVisuals * sizeof(VisualRec));
    if (!pVisualNew) {
	__glXFree(glXVisualPriv);
	__glXFree(glXVisualPtr);
	__glXFree(orig_vid);
	__glXFree(pMergedVisualPriv);
	__glXFree(pMergedVisualConfigs);
	return FALSE;
    }

    /* Initialize the new visuals */
    for (i = j = 0; i < numVisuals; i++) {
	is_rgb = (pVisual[i].class == TrueColor ||
		  pVisual[i].class == DirectColor);

	for (k = 0; k < numMergedVisualConfigs; k++) {
	    if (pMergedVisualConfigs[k].rgba != is_rgb)
		continue;

	    /* Initialize the new visual */
	    pVisualNew[j] = pVisual[i];
	    pVisualNew[j].vid = FakeClientID(0);

	    /* Check for the default visual */
	    if (!found_default && pVisual[i].vid == *defaultVisp) {
		*defaultVisp = pVisualNew[j].vid;
		found_default = TRUE;
	    }

	    /* Save the old VisualID */
	    orig_vid[j] = pVisual[i].vid;

	    /* Initialize the glXVisual */
	    glXVisualPtr[j] = pMergedVisualConfigs[k];
	    glXVisualPtr[j].vid = pVisualNew[j].vid;

	    /*
	     * If the class is -1, then assume the X visual information
	     * is identical to what GLX needs, and take them from the X
	     * visual.  NOTE: if class != -1, then all other fields MUST
	     * be initialized.
	     */
	    if (glXVisualPtr[j].class == -1) {
		glXVisualPtr[j].class      = pVisual[i].class;
		glXVisualPtr[j].redSize    = count_bits(pVisual[i].redMask);
		glXVisualPtr[j].greenSize  = count_bits(pVisual[i].greenMask);
		glXVisualPtr[j].blueSize   = count_bits(pVisual[i].blueMask);
		glXVisualPtr[j].alphaSize  = 0; /* Not supported in Mesa */
		glXVisualPtr[j].redMask    = pVisual[i].redMask;
		glXVisualPtr[j].greenMask  = pVisual[i].greenMask;
		glXVisualPtr[j].blueMask   = pVisual[i].blueMask;
		glXVisualPtr[j].alphaMask  = 0; /* Not supported in Mesa */
		glXVisualPtr[j].bufferSize = rootDepth;
	    }

	    /* Save the device-dependent private for this visual */
	    glXVisualPriv[j] = pMergedVisualPriv[k];

	    j++;
	}
    }

    /* Save the GLX visuals in the screen structure */
    MESAScreens[screenInfo.numScreens-1].num_vis = numMesaVisuals;
    MESAScreens[screenInfo.numScreens-1].glx_vis = glXVisualPtr;
    MESAScreens[screenInfo.numScreens-1].private = glXVisualPriv;

    /* Set up depth's VisualIDs */
    for (i = 0; i < ndepth; i++) {
	int numVids = 0;
	VisualID *pVids = NULL;
	int k, n = 0;

	/* Count the new number of VisualIDs at this depth */
	for (j = 0; j < pdepth[i].numVids; j++)
	    for (k = 0; k < numMesaVisuals; k++)
		if (pdepth[i].vids[j] == orig_vid[k])
		    numVids++;

	/* Allocate a new list of VisualIDs for this depth */
	pVids = (VisualID *)__glXMalloc(numVids * sizeof(VisualID));

	/* Initialize the new list of VisualIDs for this depth */
	for (j = 0; j < pdepth[i].numVids; j++)
	    for (k = 0; k < numMesaVisuals; k++)
		if (pdepth[i].vids[j] == orig_vid[k])
		    pVids[n++] = pVisualNew[k].vid;

	/* Update this depth's list of VisualIDs */
	__glXFree(pdepth[i].vids);
	pdepth[i].vids = pVids;
	pdepth[i].numVids = numVids;
    }

    /* Update the X server's visuals */
    *nvisualp = numMesaVisuals;
    *visualp = pVisualNew;

    /* Free the old list of the X server's visuals */
    __glXFree(pVisual);

    /* Clean up temporary allocations */
    __glXFree(orig_vid);
    __glXFree(pMergedVisualPriv);
    __glXFree(pMergedVisualConfigs);

    /* Free the private list created by DDX HW driver */
    if (visualPrivates) xfree(visualPrivates);
    visualPrivates = NULL;

    return TRUE;
}

void __MESA_setVisualConfigs(int nconfigs, __GLXvisualConfig *configs,
			     void **privates)
{
    numConfigs = nconfigs;
    visualConfigs = configs;
    visualPrivates = privates;
}

Bool __MESA_initVisuals(VisualPtr *visualp, DepthPtr *depthp,
			int *nvisualp, int *ndepthp, int *rootDepthp,
			VisualID *defaultVisp, unsigned long sizes,
			int bitsPerRGB)
{
    /*
     * Setup the visuals supported by this particular screen.
     */
    return init_visuals(nvisualp, visualp, defaultVisp,
			*ndepthp, *depthp, *rootDepthp);
}

static void fixup_visuals(int screen)
{
    ScreenPtr pScreen = screenInfo.screens[screen];
    __MESA_screen *pMScr = &MESAScreens[screen];
    __GLXvisualConfig *pGLXVis  = pMScr->glx_vis;
    VisualPtr pVis;
    int i, j;

    for (i = 0; i < pMScr->num_vis; i++, pGLXVis++) {
	pVis = pScreen->visuals;

	/* Find a visual that matches the GLX visual's class and size */
	for (j = 0; j < pScreen->numVisuals; j++, pVis++) {
	    if (pVis->class == pGLXVis->class &&
		pVis->nplanes == pGLXVis->bufferSize) {

		/* Fixup the masks */
		pGLXVis->redMask   = pVis->redMask;
		pGLXVis->greenMask = pVis->greenMask;
		pGLXVis->blueMask  = pVis->blueMask;

		/* Recalc the sizes */
		pGLXVis->redSize   = count_bits(pGLXVis->redMask);
		pGLXVis->greenSize = count_bits(pGLXVis->greenMask);
		pGLXVis->blueSize  = count_bits(pGLXVis->blueMask);
	    }
	}
    }
}

static void init_screen_visuals(int screen)
{
    ScreenPtr pScreen = screenInfo.screens[screen];
    __GLXvisualConfig *pGLXVis = MESAScreens[screen].glx_vis;
    XMesaVisual *pXMesaVisual;
    VisualPtr pVis;
    int *used;
    int i, j;

    /* Alloc space for the list of XMesa visuals */
    pXMesaVisual = (XMesaVisual *)__glXMalloc(MESAScreens[screen].num_vis *
					      sizeof(XMesaVisual));

    used = (int *)__glXMalloc(pScreen->numVisuals * sizeof(int));
    __glXMemset(used, 0, pScreen->numVisuals * sizeof(int));

    for (i = 0; i < MESAScreens[screen].num_vis; i++, pGLXVis++) {

	pVis = pScreen->visuals;
	for (j = 0; j < pScreen->numVisuals; j++, pVis++) {

	    if (pVis->class == pGLXVis->class &&
		pVis->nplanes == pGLXVis->bufferSize &&
		!used[j]) {

		if (pVis->redMask   == pGLXVis->redMask &&
		    pVis->greenMask == pGLXVis->greenMask &&
		    pVis->blueMask  == pGLXVis->blueMask) {

		    /* Create the XMesa visual */
		    pXMesaVisual[i] =
			XMesaCreateVisual(pScreen,
					  pVis,
					  pGLXVis->rgba,
					  (pGLXVis->alphaSize > 0),
					  pGLXVis->doubleBuffer,
					  pGLXVis->stereo,
					  GL_TRUE, /* ximage_flag */
					  pGLXVis->depthSize,
					  pGLXVis->stencilSize,
					  pGLXVis->accumRedSize,
					  pGLXVis->level);

		    /* Set the VisualID */
		    pGLXVis->vid = pVis->vid;

		    /* Mark this visual used */
		    used[j] = 1;
		    break;
		}
	    }
	}
    }

    __glXFree(used);

    MESAScreens[screen].xm_vis = pXMesaVisual;
}

Bool __MESA_screenProbe(int screen)
{
    /*
     * Set up the current screen's visuals.
     */
    __glDDXScreenInfo.pGlxVisual = MESAScreens[screen].glx_vis;
    __glDDXScreenInfo.pVisualPriv = MESAScreens[screen].private;
    __glDDXScreenInfo.numVisuals =
	__glDDXScreenInfo.numUsableVisuals = MESAScreens[screen].num_vis;

    /*
     * Set the current screen's createContext routine.  This could be
     * wrapped by a DDX GLX context creation routine.
     */
    __glDDXScreenInfo.createContext = __MESA_createContext;

    /*
     * The ordering of the rgb compenents might have been changed by the
     * driver after mi initialized them.
     */
    fixup_visuals(screen);

    /*
     * Find the GLX visuals that are supported by this screen and create
     * XMesa's visuals.
     */
    init_screen_visuals(screen);

    return TRUE;
}

extern void __MESA_resetExtension(void)
{
    int i, j;

    XMesaReset();

    for (i = 0; i < screenInfo.numScreens; i++) {
	for (j = 0; j < MESAScreens[i].num_vis; j++) {
	    XMesaDestroyVisual(MESAScreens[i].xm_vis[j]);
	}
	__glXFree(MESAScreens[i].glx_vis);
	MESAScreens[i].glx_vis = NULL;
	MESAScreens[i].num_vis = 0;
    }
    MESA_CC = NULL;
}

void __MESA_createBuffer(__GLXdrawablePrivate *glxPriv)
{
    DrawablePtr pDraw = glxPriv->pDraw;
    XMesaVisual xm_vis = find_mesa_visual(pDraw->pScreen->myNum,
					  glxPriv->pGlxVisual->vid);
    __GLdrawablePrivate *glPriv = &glxPriv->glPriv;
    __MESA_buffer buf;

    buf = (__MESA_buffer)__glXMalloc(sizeof(struct __MESA_bufferRec));

    /* Create Mesa's buffers */
    if (glxPriv->type == DRAWABLE_WINDOW) {
	buf->xm_buf = (void *)XMesaCreateWindowBuffer(xm_vis,
						      (WindowPtr)pDraw);
    } else {
	buf->xm_buf = (void *)XMesaCreatePixmapBuffer(xm_vis,
						      (PixmapPtr)pDraw, 0);
    }

    /* Wrap the front buffer's resize routine */
    buf->fbresize = glPriv->frontBuffer.resize;
    glPriv->frontBuffer.resize = __MESA_resizeBuffers;

    /* Wrap the swap buffers routine */
    buf->fbswap = glxPriv->swapBuffers;
    glxPriv->swapBuffers = __MESA_swapBuffers;

    /* Save Mesa's private buffer structure */
    glPriv->private = (void *)buf;
    glPriv->freePrivate = __MESA_destroyBuffer;
}

GLboolean __MESA_resizeBuffers(__GLdrawableBuffer *buffer,
			       GLint x, GLint y,
			       GLuint width, GLuint height, 
			       __GLdrawablePrivate *glPriv,
			       GLuint bufferMask)
{
    __MESA_buffer buf = (__MESA_buffer)glPriv->private;

    if (buf->xm_buf && buf->xm_buf->xm_context) {
	GLcontext *ctx = buf->xm_buf->xm_context->gl_ctx;
	XMesaForceCurrent(buf->xm_buf->xm_context);
	(*ctx->CurrentDispatch->ResizeBuffersMESA)();
        if (MESA_CC)
           XMesaForceCurrent(MESA_CC->xm_ctx);
    }

    return (*buf->fbresize)(buffer, x, y, width, height, glPriv, bufferMask);
}

GLboolean __MESA_swapBuffers(__GLXdrawablePrivate *glxPriv)
{
    __MESA_buffer buf = (__MESA_buffer)glxPriv->glPriv.private;

    /*
    ** Do not call the wrapped swap buffers routine since Mesa has
    ** already done the swap.
    */
    XMesaSwapBuffers(buf->xm_buf);

    return GL_TRUE;
}

void __MESA_destroyBuffer(__GLdrawablePrivate *glPriv)
{
    __MESA_buffer buf = (__MESA_buffer)glPriv->private;
    __GLXdrawablePrivate *glxPriv = (__GLXdrawablePrivate *)glPriv->other;

    /* Destroy Mesa's buffers */
    if (buf->xm_buf)
	XMesaDestroyBuffer(buf->xm_buf);

    /* Unwrap these routines */
    glxPriv->swapBuffers = buf->fbswap;
    glPriv->frontBuffer.resize = buf->fbresize;

    __glXFree(glPriv->private);
    glPriv->private = NULL;
}

__GLinterface *__MESA_createContext(__GLimports *imports,
				    __GLcontextModes *modes,
				    __GLinterface *shareGC)
{
    __GLcontext *gl_ctx;
    XMesaContext m_share = NULL;
    XMesaVisual xm_vis;
    __GLXcontext *glxc = (__GLXcontext *)imports->other;

    gl_ctx = (__GLcontext *)__glXMalloc(sizeof(__GLcontext));
    if (!gl_ctx)
	return NULL;

    gl_ctx->iface.imports = *imports;

    gl_ctx->iface.exports.destroyContext = __MESA_destroyContext;
    gl_ctx->iface.exports.loseCurrent = __MESA_loseCurrent;
    gl_ctx->iface.exports.makeCurrent = __MESA_makeCurrent;
    gl_ctx->iface.exports.shareContext = __MESA_shareContext;
    gl_ctx->iface.exports.copyContext = __MESA_copyContext;
    gl_ctx->iface.exports.forceCurrent = __MESA_forceCurrent;
    gl_ctx->iface.exports.notifyResize = __MESA_notifyResize;
    gl_ctx->iface.exports.notifyDestroy = __MESA_notifyDestroy;
    gl_ctx->iface.exports.notifySwapBuffers = __MESA_notifySwapBuffers;
    gl_ctx->iface.exports.dispatchExec = __MESA_dispatchExec;
    gl_ctx->iface.exports.beginDispatchOverride = __MESA_beginDispatchOverride;
    gl_ctx->iface.exports.endDispatchOverride = __MESA_endDispatchOverride;

    if (shareGC) m_share = ((__GLcontext *)shareGC)->xm_ctx;
    xm_vis = find_mesa_visual(glxc->pScreen->myNum, glxc->pGlxVisual->vid);
    if (xm_vis) {
	gl_ctx->xm_ctx = XMesaCreateContext(xm_vis, m_share);
    } else {
	__glXFree(gl_ctx);
	gl_ctx = NULL;
    }

    return (__GLinterface *)gl_ctx;
}

GLboolean __MESA_destroyContext(__GLcontext *gc)
{
    XMesaDestroyContext(gc->xm_ctx);
    __glXFree(gc);
    return GL_TRUE;
}

GLboolean __MESA_loseCurrent(__GLcontext *gc)
{
    MESA_CC = NULL;
    __glXLastContext = NULL;
    return XMesaLoseCurrent(gc->xm_ctx);
}

GLboolean __MESA_makeCurrent(__GLcontext *gc, __GLdrawablePrivate *glPriv)
{
    __MESA_buffer buf = (__MESA_buffer)glPriv->private;

    MESA_CC = gc;
    return XMesaMakeCurrent(gc->xm_ctx, buf->xm_buf);
}

GLboolean __MESA_shareContext(__GLcontext *gc, __GLcontext *gcShare)
{
    /* NOT_DONE */
    ErrorF("__MESA_shareContext\n");
    return GL_FALSE;
}

GLboolean __MESA_copyContext(__GLcontext *dst, const __GLcontext *src,
			     GLuint mask)
{
    /* NOT_DONE */
    ErrorF("__MESA_copyContext\n");
    return GL_FALSE;
}

GLboolean __MESA_forceCurrent(__GLcontext *gc)
{
    MESA_CC = gc;
    return XMesaForceCurrent(gc->xm_ctx);
}

GLboolean __MESA_notifyResize(__GLcontext *gc)
{
    /* NOT_DONE */
    ErrorF("__MESA_notifyResize\n");
    return GL_FALSE;
}

void __MESA_notifyDestroy(__GLcontext *gc)
{
    /* NOT_DONE */
    ErrorF("__MESA_notifyDestroy\n");
    return;
}

void __MESA_notifySwapBuffers(__GLcontext *gc)
{
    /* NOT_DONE */
    ErrorF("__MESA_notifySwapBuffers\n");
    return;
}

struct __GLdispatchStateRec *__MESA_dispatchExec(__GLcontext *gc)
{
    /* NOT_DONE */
    ErrorF("__MESA_dispatchExec\n");
    return NULL;
}

void __MESA_beginDispatchOverride(__GLcontext *gc)
{
    /* NOT_DONE */
    ErrorF("__MESA_beginDispatchOverride\n");
    return;
}

void __MESA_endDispatchOverride(__GLcontext *gc)
{
    /* NOT_DONE */
    ErrorF("__MESA_endDispatchOverride\n");
    return;
}

GLint __glEvalComputeK(GLenum target)
{
    switch (target) {
    case GL_MAP1_VERTEX_4:
    case GL_MAP1_COLOR_4:
    case GL_MAP1_TEXTURE_COORD_4:
    case GL_MAP2_VERTEX_4:
    case GL_MAP2_COLOR_4:
    case GL_MAP2_TEXTURE_COORD_4:
	return 4;
    case GL_MAP1_VERTEX_3:
    case GL_MAP1_TEXTURE_COORD_3:
    case GL_MAP1_NORMAL:
    case GL_MAP2_VERTEX_3:
    case GL_MAP2_TEXTURE_COORD_3:
    case GL_MAP2_NORMAL:
	return 3;
    case GL_MAP1_TEXTURE_COORD_2:
    case GL_MAP2_TEXTURE_COORD_2:
	return 2;
    case GL_MAP1_TEXTURE_COORD_1:
    case GL_MAP2_TEXTURE_COORD_1:
    case GL_MAP1_INDEX:
    case GL_MAP2_INDEX:
	return 1;
    default:
	return 0;
    }
}

GLuint __glFloorLog2(GLuint val)
{
    int c = 0;

    while (val > 1) {
	c++;
	val >>= 1;
    }
    return c;
}

