/* $Xorg: sunCfb24.c,v 1.4 2001/02/09 02:04:43 xorgcvs Exp $ */

/*

Copyright 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE X
CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from The Open Group.

*/
/* $XFree86: xc/programs/Xserver/hw/sun/sunCfb24.c,v 1.3 2001/12/14 19:59:42 dawes Exp $ */

/*
 * The CG8 is similar to the CG4 in that it has a mono plane, an enable 
 * plane, and a color plane. While the CG4 only has an 8-bit color
 * plane the CG8 has a 24-bit color plane. 
 *
 * If you have a CG4 you know that you can switch between the mono and
 * the color screens merely by dragging the pointer off the edge of the
 * screen, causing the other screen to be switched in. However this is
 * the cause of some consternation on the part of those people who have
 * both a CG4 and another frame buffer.
 *
 * Because of this problem, and some other considerations, I have chosen
 * to ignore the mono plane of the CG8 in this code.
 */

#define PSZ 32
#include "sun.h"
#include "cfb/cfb.h"

#define PIXPG_24BIT_COLOR 5
#define PIXPG_24BIT_COLOR_INDEX (PIXPG_24BIT_COLOR << 25)
#define PR_FORCE_UPDATE (1 << 24)

static void CG24UpdateColormap(pScreen, index, count, rmap, gmap, bmap)
    ScreenPtr	pScreen;
    int		index, count;
    u_char	*rmap, *gmap, *bmap;
{
    struct fbcmap sunCmap;

    sunCmap.index = index | PIXPG_24BIT_COLOR_INDEX | PR_FORCE_UPDATE;
    sunCmap.count = count;
    sunCmap.red = &rmap[index];
    sunCmap.green = &gmap[index];
    sunCmap.blue = &bmap[index];

    if (ioctl(sunFbs[pScreen->myNum].fd, FBIOPUTCMAP, &sunCmap) == -1)
	FatalError( "CG24UpdateColormap: FBIOPUTCMAP failed\n");
}

static void CG24StoreColors (pmap, ndef, pdefs)
    ColormapPtr pmap;
    int ndef;
    xColorItem* pdefs;
{
  u_char rmap[256], gmap[256], bmap[256];
  SetupScreen (pmap->pScreen);
  VisualPtr pVisual = pmap->pVisual;
  int i;

  if (pPrivate->installedMap != NULL && pPrivate->installedMap != pmap)
    return;
  for (i = 0; i < 256; i++) {
    rmap[i] = pmap->red[i].co.local.red >> 8;
    gmap[i] = pmap->green[i].co.local.green >> 8;
    bmap[i] = pmap->blue[i].co.local.blue >> 8;
  }
  while (ndef--) {
    i = pdefs->pixel;
    if (pdefs->flags & DoRed)
      rmap[(i & pVisual->redMask) >> pVisual->offsetRed] = (pdefs->red >> 8);
    if (pdefs->flags & DoGreen)
      gmap[(i & pVisual->greenMask) >> pVisual->offsetGreen] = (pdefs->green >> 8);
    if (pdefs->flags & DoBlue)
      bmap[(i & pVisual->blueMask) >> pVisual->offsetBlue] = (pdefs->blue >> 8);
    pdefs++;
  }
  CG24UpdateColormap (pmap->pScreen, 0, 256, rmap, gmap, bmap);
}

#define CG8_COLOR_OFFSET 0x40000

static void CG24ScreenInit (pScreen)
    ScreenPtr pScreen;
{
#ifndef STATIC_COLOR
    SetupScreen (pScreen);
#endif
    int i;

    /* Make sure the overlay plane is disabled */
    for (i = 0; i < CG8_COLOR_OFFSET; i++)
	sunFbs[pScreen->myNum].fb[i] = 0;

#ifndef STATIC_COLOR
    pScreen->InstallColormap = sunInstallColormap;
    pScreen->UninstallColormap = sunUninstallColormap;
    pScreen->ListInstalledColormaps = sunListInstalledColormaps;
    pScreen->StoreColors = CG24StoreColors;
    pPrivate->UpdateColormap = CG24UpdateColormap;
    if (sunFlipPixels) {
	Pixel pixel = pScreen->whitePixel;
	pScreen->whitePixel = pScreen->blackPixel;
	pScreen->blackPixel = pixel;
    }
#endif
}

Bool sunCG8Init (screen, pScreen, argc, argv)
    int		    screen;    	/* what screen am I going to be */
    ScreenPtr	    pScreen;  	/* The Screen to initialize */
    int		    argc;    	/* The number of the Server's arguments. */
    char	    **argv;   	/* The arguments themselves. Don't change! */
{
    sunFbs[screen].EnterLeave = (void (*)())NoopDDA;
    return sunInitCommon (screen, pScreen, (off_t) 0,
	cfb32ScreenInit, CG24ScreenInit,
	cfbCreateDefColormap, sunSaveScreen, CG8_COLOR_OFFSET);
}

