/* $Xorg: sunCursor.c,v 1.4 2001/02/09 02:04:43 xorgcvs Exp $ */
/*

Copyright 1988  Sun Microsystems, Inc.
Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/Xserver/hw/sun/sunCursor.c,v 3.6 2003/11/17 22:20:36 dawes Exp $ */

/*-
 * sunCursor.c --
 *	Functions for maintaining the Sun software cursor...
 *
 */

#define NEED_EVENTS
#include    "sun.h"
#include    "cfb.h"
#include    "mfb.h"

#ifdef FBIOGCURMAX  /* has hardware cursor kernel support */

#define GetCursorPrivate(s) (&(GetScreenPrivate(s)->hardwareCursor))
#define SetupCursor(s)	    sunCursorPtr pCurPriv = GetCursorPrivate(s)

static void sunLoadCursor();

static Bool
sunRealizeCursor (pScreen, pCursor)
    ScreenPtr	pScreen;
    CursorPtr	pCursor;
{
    SetupCursor(pScreen);
    int	    x, y;

    /* miRecolorCursor does this */
    if (pCurPriv->pCursor == pCursor)
    {
	miPointerPosition (&x, &y);
	sunLoadCursor (pScreen, pCursor, x, y);
    }
    return TRUE;
}

static Bool
sunUnrealizeCursor (pScreen, pCursor)
    ScreenPtr	pScreen;
    CursorPtr	pCursor;
{
    return TRUE;
}

static void
sunCursorRepad (pScreen, bits, src_bits, dst_bits, ptSrc, w, h)
    ScreenPtr	    pScreen;
    CursorBitsPtr   bits;
    unsigned char   *src_bits, *dst_bits;
    DDXPointPtr	    ptSrc;
    int		    w, h;
{
    PixmapPtr	src, dst;
    BoxRec	box;
    RegionRec	rgnDst;

    if (!(src = GetScratchPixmapHeader(pScreen, bits->width, bits->height,
				       /*bpp*/ 1, /*depth*/ 1,
				      PixmapBytePad(bits->width,1), src_bits)))
	return;
    if (!(dst = GetScratchPixmapHeader(pScreen, w, h, /*bpp*/ 1, /*depth*/ 1,
				       PixmapBytePad(w,1), dst_bits)))
    {
	FreeScratchPixmapHeader(src);
	return;
    }
    box.x1 = 0;
    box.y1 = 0;
    box.x2 = w;
    box.y2 = h;
    REGION_INIT(pScreen, &rgnDst, &box, 1);
#ifndef LOWMEMFTPT
    mfbDoBitblt(&src->drawable, &dst->drawable, GXcopy, &rgnDst, ptSrc);
#else
    cfbDoBitblt(&src->drawable, &dst->drawable, GXcopy, &rgnDst, ptSrc,
	0xFFFFFFFF);
#endif /* ifndef LOWMEMFTPT */
    REGION_UNINIT(pScreen, &rgnDst);
    FreeScratchPixmapHeader(src);
    FreeScratchPixmapHeader(dst);
}

static void
sunLoadCursor (pScreen, pCursor, x, y)
    ScreenPtr	pScreen;
    CursorPtr	pCursor;
    int		x, y;
{
    SetupCursor(pScreen);
    struct fbcursor fbcursor;
    int	w, h;
    unsigned char   r[2], g[2], b[2];
    DDXPointRec	ptSrc;
    unsigned char   source_temp[1024], mask_temp[1024];

    fbcursor.set = FB_CUR_SETALL;
    fbcursor.enable = 1;
    fbcursor.pos.x = x;
    fbcursor.pos.y = y;
    fbcursor.hot.x = pCursor->bits->xhot;
    fbcursor.hot.y = pCursor->bits->yhot;
    r[0] = pCursor->backRed >> 8;
    g[0] = pCursor->backGreen >> 8;
    b[0] = pCursor->backBlue >> 8;
    r[1] = pCursor->foreRed >> 8;
    g[1] = pCursor->foreGreen >> 8;
    b[1] = pCursor->foreBlue >> 8;
    fbcursor.cmap.index = 0;
    fbcursor.cmap.count = 2;
    fbcursor.cmap.red = r;
    fbcursor.cmap.green = g;
    fbcursor.cmap.blue = b;
    fbcursor.image = (char *) pCursor->bits->source;
    fbcursor.mask = (char *) pCursor->bits->mask;
    w = pCursor->bits->width;
    h = pCursor->bits->height;
    if (w > pCurPriv->width || h > pCurPriv->height) {
	ptSrc.x = 0;
	ptSrc.y = 0;
	if (w > pCurPriv->width)
	    w = pCurPriv->width;
	if (h > pCurPriv->height)
	    h = pCurPriv->height;
	sunCursorRepad (pScreen, pCursor->bits, pCursor->bits->source,
			source_temp, &ptSrc, w, h);
	sunCursorRepad (pScreen, pCursor->bits, pCursor->bits->mask,
			mask_temp, &ptSrc, w, h);
	fbcursor.image = (char *) source_temp;
	fbcursor.mask = (char *) mask_temp;
    }
    fbcursor.size.x = w;
    fbcursor.size.y = h;
#ifndef Lynx
    (void) ioctl (sunFbs[pScreen->myNum].fd, FBIOSCURSOR, &fbcursor);
#else
    (void) sunIoctl (&sunFbs[pScreen->myNum], FBIOSCURSOR, &fbcursor);
#endif
}

static void
sunSetCursor (pScreen, pCursor, x, y)
    ScreenPtr	pScreen;
    CursorPtr	pCursor;
    int		x, y;
{
    SetupCursor(pScreen);

    if (pCursor)
    	sunLoadCursor (pScreen, pCursor, x, y);
    else
	sunDisableCursor (pScreen);
    pCurPriv->pCursor = pCursor;
}

static void
sunMoveCursor (pScreen, x, y)
    ScreenPtr	pScreen;
    int		x, y;
{
    struct fbcurpos pos;

    pos.x = x;
    pos.y = y;
#ifndef Lynx
    ioctl (sunFbs[pScreen->myNum].fd, FBIOSCURPOS, &pos);
#else
    sunIoctl (&sunFbs[pScreen->myNum], FBIOSCURPOS, &pos);
#endif
}

miPointerSpriteFuncRec sunPointerSpriteFuncs = {
    sunRealizeCursor,
    sunUnrealizeCursor,
    sunSetCursor,
    sunMoveCursor,
};

static void
sunQueryBestSize (class, pwidth, pheight, pScreen)
    int	class;
    unsigned short   *pwidth, *pheight;
    ScreenPtr	pScreen;
{
    SetupCursor (pScreen);

    switch (class)
    {
    case CursorShape:
	if (*pwidth > pCurPriv->width)
	    *pwidth = pCurPriv->width;
	if (*pheight > pCurPriv->height)
	    *pheight = pCurPriv->height;
	if (*pwidth > pScreen->width)
	    *pwidth = pScreen->width;
	if (*pheight > pScreen->height)
	    *pheight = pScreen->height;
	break;
    default:
	mfbQueryBestSize (class, pwidth, pheight, pScreen);
	break;
    }
}

extern miPointerScreenFuncRec	sunPointerScreenFuncs;

#endif

Bool sunCursorInitialize (
    ScreenPtr	pScreen)
{
#ifdef FBIOGCURMAX
    SetupCursor (pScreen);
    struct fbcurpos maxsize;

    pCurPriv->has_cursor = FALSE;
#ifndef Lynx
    if (ioctl (sunFbs[pScreen->myNum].fd, FBIOGCURMAX, &maxsize) == -1)
#else
    if (sunIoctl (&sunFbs[pScreen->myNum], FBIOGCURMAX, &maxsize) == -1)
#endif
	return FALSE;
    pCurPriv->width = maxsize.x;
    pCurPriv->height= maxsize.y;
    pScreen->QueryBestSize = sunQueryBestSize;
    miPointerInitialize (pScreen,
			 &sunPointerSpriteFuncs,
			 &sunPointerScreenFuncs,
			 FALSE);
    pCurPriv->has_cursor = TRUE;
    pCurPriv->pCursor = NULL;
    return TRUE;
#else
    return FALSE;
#endif
}

void sunDisableCursor (
    ScreenPtr	pScreen)
{
#ifdef FBIOGCURMAX
    SetupCursor (pScreen);
    struct fbcursor fbcursor;

    if (pCurPriv->has_cursor)
    {
    	fbcursor.set = FB_CUR_SETCUR;
    	fbcursor.enable = 0;
#ifndef Lynx
    	(void) ioctl (sunFbs[pScreen->myNum].fd, FBIOSCURSOR, &fbcursor);
#else
    	(void) sunIoctl (&sunFbs[pScreen->myNum], FBIOSCURSOR, &fbcursor);
#endif
	pCurPriv->pCursor = NULL;
    }
#endif
}
