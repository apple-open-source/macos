/* $Xorg: sunLyCfb.c,v 1.4 2001/02/09 02:04:44 xorgcvs Exp $ */
/*
 * This is sunCfb.c modified for LynxOS
 * Copyright 1996 by Thomas Mueller
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Mueller not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Mueller makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS MUELLER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS MUELLER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/sunLynx/sunLyCfb.c,v 3.5 2001/12/14 19:59:44 dawes Exp $ */

/*
Copyright 1990, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 */

/************************************************************
Copyright 1987 by Sun Microsystems, Inc. Mountain View, CA.

                    All Rights Reserved

Permission  to  use,  copy,  modify,  and  distribute   this
software  and  its documentation for any purpose and without
fee is hereby granted, provided that the above copyright no-
tice  appear  in all copies and that both that copyright no-
tice and this permission notice appear in  supporting  docu-
mentation,  and  that the names of Sun or The Open Group
not be used in advertising or publicity pertaining to 
distribution  of  the software  without specific prior 
written permission. Sun and The Open Group make no 
representations about the suitability of this software for 
any purpose. It is provided "as is" without any express or 
implied warranty.

SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO  THIS  SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-
NESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SUN BE  LI-
ABLE  FOR  ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,  DATA  OR
PROFITS,  WHETHER  IN  AN  ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

/*
 * Copyright 1987 by the Regents of the University of California
 * Copyright 1987 by Adam de Boor, UC Berkeley
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

/****************************************************************/
/* Modified from  sunCG4C.c for X11R3 by Tom Jarmolowski	*/
/****************************************************************/

/* 
 * Copyright 1991, 1992, 1993 Kaleb S. Keithley
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  Kaleb S. Keithley makes no 
 * representations about the suitability of this software for 
 * any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#include "sun.h"
#include "cfb/cfb.h"
#include "mi/miline.h"

#define GXZEROLINEBIAS	(OCTANT1 | OCTANT3 | OCTANT4 | OCTANT6)

static void CGUpdateColormap(pScreen, dex, count, rmap, gmap, bmap)
    ScreenPtr	pScreen;
    int		dex, count;
    u_char	*rmap, *gmap, *bmap;
{
    struct fbcmap sunCmap;

    sunCmap.index = dex;
    sunCmap.count = count;
    sunCmap.red = &rmap[dex];
    sunCmap.green = &gmap[dex];
    sunCmap.blue = &bmap[dex];

    if (sunIoctl(&sunFbs[pScreen->myNum], FBIOPUTCMAP, &sunCmap) < 0) {
	Error("CGUpdateColormap");
	FatalError( "CGUpdateColormap: FBIOPUTCMAP failed\n" );
    }
}

void sunInstallColormap(cmap)
    ColormapPtr	cmap;
{
    SetupScreen(cmap->pScreen);
    register int i;
    register Entry *pent;
    register VisualPtr pVisual = cmap->pVisual;
    u_char	  rmap[256], gmap[256], bmap[256];
    unsigned long rMask, gMask, bMask;
    int	oRed, oGreen, oBlue;

    if (cmap == pPrivate->installedMap)
	return;
    if (pPrivate->installedMap)
	WalkTree(pPrivate->installedMap->pScreen, TellLostMap,
		 (pointer) &(pPrivate->installedMap->mid));
    if ((pVisual->class | DynamicClass) == DirectColor) {
	if (pVisual->ColormapEntries < 256) {
	    rMask = pVisual->redMask;
	    gMask = pVisual->greenMask;
	    bMask = pVisual->blueMask;
	    oRed = pVisual->offsetRed;
	    oGreen = pVisual->offsetGreen;
	    oBlue = pVisual->offsetBlue;
	} else {
	    rMask = gMask = bMask = 255;
	    oRed = oGreen = oBlue = 0;
	}
	for (i = 0; i < 256; i++) {
	    rmap[i] = cmap->red[(i & rMask) >> oRed].co.local.red >> 8;
	    gmap[i] = cmap->green[(i & gMask) >> oGreen].co.local.green >> 8;
	    bmap[i] = cmap->blue[(i & bMask) >> oBlue].co.local.blue >> 8;
	}
    } else {
	for (i = 0, pent = cmap->red;
	     i < pVisual->ColormapEntries;
	     i++, pent++) {
	    if (pent->fShared) {
		rmap[i] = pent->co.shco.red->color >> 8;
		gmap[i] = pent->co.shco.green->color >> 8;
		bmap[i] = pent->co.shco.blue->color >> 8;
	    }
	    else {
		rmap[i] = pent->co.local.red >> 8;
		gmap[i] = pent->co.local.green >> 8;
		bmap[i] = pent->co.local.blue >> 8;
	    }
	}
    }
    pPrivate->installedMap = cmap;
    (*pPrivate->UpdateColormap) (cmap->pScreen, 0, 256, rmap, gmap, bmap);
    WalkTree(cmap->pScreen, TellGainedMap, (pointer) &(cmap->mid));
}

void sunUninstallColormap(cmap)
    ColormapPtr	cmap;
{
    SetupScreen(cmap->pScreen);
    if (cmap == pPrivate->installedMap) {
	Colormap defMapID = cmap->pScreen->defColormap;

	if (cmap->mid != defMapID) {
	    ColormapPtr defMap = (ColormapPtr) LookupIDByType(defMapID,
							      RT_COLORMAP);

	    if (defMap)
		(*cmap->pScreen->InstallColormap)(defMap);
	    else
	        ErrorF("sunFbs: Can't find default colormap\n");
	}
    }
}

int sunListInstalledColormaps(pScreen, pCmapList)
    ScreenPtr	pScreen;
    Colormap	*pCmapList;
{
    SetupScreen(pScreen);
    *pCmapList = pPrivate->installedMap->mid;
    return (1);
}

static void CGStoreColors(pmap, ndef, pdefs)
    ColormapPtr	pmap;
    int		ndef;
    xColorItem	*pdefs;
{
    SetupScreen(pmap->pScreen);
    u_char	rmap[256], gmap[256], bmap[256];
    xColorItem	expanddefs[256];
    register int i;

    if (pPrivate->installedMap != NULL && pPrivate->installedMap != pmap)
	return;
    if ((pmap->pVisual->class | DynamicClass) == DirectColor) {
	ndef = cfbExpandDirectColors(pmap, ndef, pdefs, expanddefs);
	pdefs = expanddefs;
    }
    while (ndef--) {
	i = pdefs->pixel;
	rmap[i] = pdefs->red >> 8;
	gmap[i] = pdefs->green >> 8;
	bmap[i] = pdefs->blue >> 8;
	(*pPrivate->UpdateColormap) (pmap->pScreen, i, 1, rmap, gmap, bmap);
	pdefs++;
    }
}

static void CGScreenInit (pScreen)
    ScreenPtr	pScreen;
{
#ifndef STATIC_COLOR /* { */
    SetupScreen (pScreen);
    pScreen->InstallColormap = sunInstallColormap;
    pScreen->UninstallColormap = sunUninstallColormap;
    pScreen->ListInstalledColormaps = sunListInstalledColormaps;
    pScreen->StoreColors = CGStoreColors;
    pPrivate->UpdateColormap = CGUpdateColormap;
    if (sunFlipPixels) {
	Pixel pixel = pScreen->whitePixel;
	pScreen->whitePixel = pScreen->blackPixel;
	pScreen->blackPixel = pixel;
    }
#endif /* } */
}

static void checkMono (argc, argv)
    int argc;
    char** argv;
{
    int i;

    for (i = 1; i < argc; i++)
	if (strcmp (argv[i], "-mono") == 0)
	    ErrorF ("-mono not appropriate for CG3/CG6\n");
}

Bool sunCG3Init (screen, pScreen, argc, argv)
    int	    	  screen;    	/* what screen am I going to be */
    ScreenPtr	  pScreen;  	/* The Screen to initialize */
    int	    	  argc;	    	/* The number of the Server's arguments. */
    char    	  **argv;   	/* The arguments themselves. Don't change! */
{
    unsigned long addr;

    checkMono (argc, argv);
    sunFbs[screen].EnterLeave = (void (*)())NoopDDA;
    if (ioctl(sunFbs[screen].fd, TIO_QUERYLOC, &addr) < 0) {
    	perror("TIO_QUERYLOC");
    	FatalError("%s:%d can't query FB addr sunFbs[%d].fd=%d\n",
		   __FILE__, __LINE__, screen, sunFbs[screen]);
    }
    return sunInitCommon (screen, pScreen, (off_t) addr,
	sunCfbScreenInit, CGScreenInit,
	cfbCreateDefColormap, sunSaveScreen, 0);
}

Bool sunCG6Init (screen, pScreen, argc, argv)
    int		screen;    	/* The index of pScreen in the ScreenInfo */
    ScreenPtr	pScreen;  	/* The Screen to initialize */
    int		argc;	    	/* The number of the Server's arguments. */
    char**	argv;   	/* The arguments themselves. Don't change! */
{
    unsigned long dacoffset;
    unsigned long addr;
    pointer	fbc;
    pointer	fhc;
    pointer	fb;

    checkMono (argc, argv);
    if (!sunScreenAllocate (pScreen))
	return FALSE;
    if (!sunFbs[screen].fbuf) {
	if (ioctl(sunFbs[screen].fd, TIO_QUERYLOC, &addr) < 0) {
	    perror("TIO_QUERYLOC");
	    FatalError("%s:%d can't query FB addr sunFbs[%d].fd=%d\n",
		       __FILE__, __LINE__, screen, sunFbs[screen]);
	}
	if ((fb = sunMemoryMap(0x00100000, (off_t) addr + 0x00800000UL,
			       sunFbs[screen].fd, "FB")) == NULL)
	    return FALSE;

	if ((fbc = sunMemoryMap(0x2000, (off_t) addr + 0x00700000UL,
				sunFbs[screen].fd, "FBC_TEC")) == NULL)
	    return FALSE;

	if ((fhc = sunMemoryMap(0x2000, (off_t) addr + 0x00300000UL,
				sunFbs[screen].fd, "FHC_THC")) == NULL)
	    return FALSE;

	sunFbs[screen].fhc = fhc;
	sunFbs[screen].thc = (char *)fhc + 0x1000;
	sunFbs[screen].fb = fbc;
	sunFbs[screen].tec = (char *)fbc + 0x1000;
	sunFbs[screen].fbuf = fb;

	if (ioctl(sunFbs[screen].fd, TIO_QUERYRAMDAC, &dacoffset) < 0)
	    FatalError("can't query DAC addr\n");
	if ((sunFbs[screen].ramdac =
	     sunMemoryMap((size_t) sunFbs[screen].info.fb_cmsize * 3,
			  (unsigned long)addr + dacoffset,
			  sunFbs[screen].fd, "DAC")) == NULL)
	    return FALSE;
    }

    sunFbs[screen].EnterLeave = (void (*)())NoopDDA;
    if (!sunCfbSetupScreen (pScreen, 
	    sunFbs[screen].fbuf,
	    sunFbs[screen].info.fb_width, 
	    sunFbs[screen].info.fb_height,
	    monitorResolution, monitorResolution, 
	    sunFbs[screen].info.fb_width,
	    sunFbs[screen].info.fb_depth))
	return FALSE;
    if (sunNoGX == FALSE) {
	/* cleanup CG& hardware state */
    	if (sunIoctl(&sunFbs[screen], FBIORESET, 0) < 0)
    		perror("FBIORESET");
	if (!sunGXInit (pScreen, &sunFbs[screen]))
	    return FALSE;
    }
    if (!sunCfbFinishScreenInit(pScreen,
	    sunFbs[screen].fbuf,
	    sunFbs[screen].info.fb_width, 
	    sunFbs[screen].info.fb_height,
	    monitorResolution, monitorResolution, 
	    sunFbs[screen].info.fb_width,
	    sunFbs[screen].info.fb_depth))
	return FALSE;
    miInitializeBackingStore(pScreen);
    if (sunNoGX == FALSE) {
	miSetZeroLineBias(pScreen, GXZEROLINEBIAS);
    }
    CGScreenInit (pScreen);
    if (!sunScreenInit (pScreen))
	return FALSE;
    sunSaveScreen (pScreen, SCREEN_SAVER_OFF);
    return cfbCreateDefColormap(pScreen);
}
