/*
 * Acceleration for the Creator and Creator3D framebuffer.
 *
 * Copyright (C) 1998,1999,2000 Jakub Jelinek (jakub@redhat.com)
 * Copyright (C) 1998 Michal Rehacek (majkl@iname.com)
 * Copyright (C) 1999,2000 David S. Miller (davem@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JAKUB JELINEK, MICHAL REHACEK, OR DAVID MILLER BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_accel.c,v 1.5 2001/03/03 22:41:34 tsi Exp $ */

#include	"scrnintstr.h"
#include	"pixmapstr.h"
#include	"regionstr.h"
#include	"mistruct.h"
#include	"fontstruct.h"
#include	"dixfontstr.h"
#define PSZ 8
#include	"cfb.h"
#undef PSZ
#include	"cfb32.h"
#include	"mibstore.h"
#include	"mifillarc.h"
#include	"miwideline.h"
#include	"miline.h"
#include	"fastblt.h"
#include	"mergerop.h"
#include	"migc.h"
#include	"mi.h"

#include	"cfb8_32wid.h"

#include	"ffb.h"
#include	"ffb_fifo.h"
#include	"ffb_rcache.h"
#include	"ffb_loops.h"
#include	"ffb_regs.h"
#include	"ffb_stip.h"
#include 	"ffb_gc.h"

int	CreatorScreenPrivateIndex;
int	CreatorGCPrivateIndex;
int	CreatorWindowPrivateIndex;
int	CreatorGeneration;

/* Indexed by ffb resolution enum. */
struct fastfill_parms ffb_fastfill_parms[] = {
	/* fsmall, psmall,  ffh,  ffw,  pfh,  pfw */
	{  0x00c0, 0x1400, 0x04, 0x08, 0x10, 0x50 },	/* Standard: 1280 x 1024 */
	{  0x0140, 0x2800, 0x04, 0x10, 0x10, 0xa0 },	/* High:     1920 x 1360 */
	{  0x0080, 0x0a00, 0x02, 0x08, 0x08, 0x50 },	/* Stereo:   960  x 580  */
/*XXX*/	{  0x00c0, 0x0a00, 0x04, 0x08, 0x08, 0x50 },	/* Portrait: 1280 x 2048 XXX */
};

static Bool
CreatorCreateWindow (WindowPtr pWin)
{
	ScreenPtr pScreen = pWin->drawable.pScreen;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pScreen);
	CreatorPrivWinPtr pFfbPrivWin;
	unsigned int fbc;
	int depth = (pWin->drawable.depth == 8) ? 8 : 24;
	int i, visual, visclass;

	if (depth == 8) {
		if (!cfbCreateWindow (pWin))
			return FALSE;
	} else {
		if (!cfb32CreateWindow (pWin))
			return FALSE;
	}

	pFfbPrivWin = xalloc(sizeof(CreatorPrivWinRec));
	if (!pFfbPrivWin)
		return FALSE;

	fbc  = FFB_FBC_WB_A | FFB_FBC_WM_COMBINED | FFB_FBC_RB_A;
	fbc |= FFB_FBC_WE_FORCEON;
	fbc |= FFB_FBC_SB_BOTH;
	fbc |= FFB_FBC_ZE_OFF | FFB_FBC_YE_OFF;
	if (depth == 8)
		fbc |= (FFB_FBC_RE_MASK | FFB_FBC_GE_OFF | FFB_FBC_BE_OFF);
	else
		fbc |= FFB_FBC_RGBE_MASK;
	fbc |= FFB_FBC_XE_ON;
	pFfbPrivWin->fbc_base = fbc;

	visual = wVisual(pWin);
	visclass = 0;
	for (i = 0; i < pScreen->numVisuals; i++) {
		if (pScreen->visuals[i].vid == visual) {
			visclass = pScreen->visuals[i].class;
			break;
		}
	}

	pFfbPrivWin->wid = FFBWidAlloc(pFfb, visclass, wColormap(pWin), TRUE);
	if (pFfbPrivWin->wid == (unsigned int) -1) {
		xfree(pFfbPrivWin);
		return FALSE;
	}
	FFBLOG(("CreatorCreateWindow: pWin %p depth %d wid %x fbc_base %x\n",
		pWin, depth, pFfbPrivWin->wid, pFfbPrivWin->fbc_base));

	pFfbPrivWin->Stipple = NULL;
	CreatorSetWindowPrivate(pWin, pFfbPrivWin);

	return TRUE;
}

static Bool
CreatorDestroyWindow (WindowPtr pWin)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pWin->drawable.pScreen);
	CreatorPrivWinPtr pFfbPrivWin;
	int depth = (pWin->drawable.depth == 8) ? 8 : 24;

	FFBLOG(("CreatorDestroyWindow: pWin %p depth %d\n", pWin, depth));
	pFfbPrivWin = CreatorGetWindowPrivate(pWin);
	if (pFfbPrivWin->Stipple)
		xfree(pFfbPrivWin->Stipple);
	FFBWidFree(pFfb, pFfbPrivWin->wid);
	xfree(pFfbPrivWin);

	if (depth == 8)
		return cfbDestroyWindow (pWin);
	else
		return cfb32DestroyWindow (pWin);
}

extern CreatorStipplePtr FFB_tmpStipple;

static int
CreatorChangeWindowAttributes (WindowPtr pWin, Mask mask)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pWin->drawable.pScreen);
	CreatorPrivWinPtr pFfbPrivWin;
	CreatorStipplePtr stipple;
	Mask index;
	WindowPtr pBgWin;
	register cfbPrivWin *pPrivWin;
	int width, depth;

	FFBLOG(("CreatorChangeWindowAttributes: WIN(%p) mask(%08x)\n", pWin, mask));
	pPrivWin = (cfbPrivWin *)(pWin->devPrivates[cfbWindowPrivateIndex].ptr);
	pFfbPrivWin = CreatorGetWindowPrivate(pWin);
	depth = pWin->drawable.depth;

	/*
	 * When background state changes from ParentRelative and
	 * we had previously rotated the fast border pixmap to match
	 * the parent relative origin, rerotate to match window
	 */
	if (mask & (CWBackPixmap | CWBackPixel) &&
	    pWin->backgroundState != ParentRelative &&
	    pPrivWin->fastBorder &&
	    (pPrivWin->oldRotate.x != pWin->drawable.x ||
	     pPrivWin->oldRotate.y != pWin->drawable.y)) {
		if (depth == 8) {
			cfbXRotatePixmap(pPrivWin->pRotatedBorder,
					 pWin->drawable.x - pPrivWin->oldRotate.x);
			cfbYRotatePixmap(pPrivWin->pRotatedBorder,
					 pWin->drawable.y - pPrivWin->oldRotate.y);
		} else {
			cfb32XRotatePixmap(pPrivWin->pRotatedBorder,
					   pWin->drawable.x - pPrivWin->oldRotate.x);
			cfb32YRotatePixmap(pPrivWin->pRotatedBorder,
					   pWin->drawable.y - pPrivWin->oldRotate.y);
		}
		pPrivWin->oldRotate.x = pWin->drawable.x;
		pPrivWin->oldRotate.y = pWin->drawable.y;
	}
	while (mask) {
		index = lowbit(mask);
		mask &= ~index;
		switch (index) {
		case CWBackPixmap:
			stipple = pFfbPrivWin->Stipple;
			if (pWin->backgroundState == None ||
			    pWin->backgroundState == ParentRelative) {
				pPrivWin->fastBackground = FALSE;
				if (stipple) {
					xfree (stipple);
					pFfbPrivWin->Stipple = NULL;
				}
				/* Rotate border to match parent origin */
				if (pWin->backgroundState == ParentRelative &&
				    pPrivWin->pRotatedBorder)  {
					for (pBgWin = pWin->parent;
					     pBgWin->backgroundState == ParentRelative;
					     pBgWin = pBgWin->parent);
					if (depth == 8) {
						cfbXRotatePixmap(pPrivWin->pRotatedBorder,
								 pBgWin->drawable.x - pPrivWin->oldRotate.x);
						cfbYRotatePixmap(pPrivWin->pRotatedBorder,
								 pBgWin->drawable.y - pPrivWin->oldRotate.y);
					} else {
						cfb32XRotatePixmap(pPrivWin->pRotatedBorder,
								   pBgWin->drawable.x - pPrivWin->oldRotate.x);
						cfb32YRotatePixmap(pPrivWin->pRotatedBorder,
								   pBgWin->drawable.y - pPrivWin->oldRotate.y);
					}
					pPrivWin->oldRotate.x = pBgWin->drawable.x;
					pPrivWin->oldRotate.y = pBgWin->drawable.y;
				}
				break;
			}
			if (!stipple) {
				if (!FFB_tmpStipple)
					FFB_tmpStipple = (CreatorStipplePtr)
						xalloc (sizeof *FFB_tmpStipple);
				stipple = FFB_tmpStipple;
			}
			if (stipple) {
				int ph = FFB_FFPARMS(pFfb).pagefill_height;

				if (CreatorCheckTile (pWin->background.pixmap, stipple,
						      ((DrawablePtr)pWin)->x & 31,
						      ((DrawablePtr)pWin)->y & 31, ph)) {
					stipple->alu = GXcopy;
					pPrivWin->fastBackground = FALSE;
					if (stipple == FFB_tmpStipple) {
						pFfbPrivWin->Stipple = stipple;
						FFB_tmpStipple = 0;
					}
					break;
				}
			}
			if ((stipple = pFfbPrivWin->Stipple) != NULL) {
				xfree (stipple);
				pFfbPrivWin->Stipple = NULL;
			}
			if (((width = (pWin->background.pixmap->drawable.width *
				       pWin->background.pixmap->drawable.bitsPerPixel)) <= 32) &&
			    !(width & (width - 1))) {
				if (depth == 8) {
					cfbCopyRotatePixmap(pWin->background.pixmap,
							    &pPrivWin->pRotatedBackground,
							    pWin->drawable.x,
							    pWin->drawable.y);
				} else {
					cfb32CopyRotatePixmap(pWin->background.pixmap,
							      &pPrivWin->pRotatedBackground,
							      pWin->drawable.x,
							      pWin->drawable.y);
				}
				if (pPrivWin->pRotatedBackground) {
					pPrivWin->fastBackground = TRUE;
					pPrivWin->oldRotate.x = pWin->drawable.x;
					pPrivWin->oldRotate.y = pWin->drawable.y;
				} else
					pPrivWin->fastBackground = FALSE;
				break;
			}
			pPrivWin->fastBackground = FALSE;
			break;

		case CWBackPixel:
			pPrivWin->fastBackground = FALSE;
			break;

		case CWBorderPixmap:
			/* don't bother with accelerator for border tiles (just lazy) */
			if (((width = (pWin->border.pixmap->drawable.width *
				       pWin->border.pixmap->drawable.bitsPerPixel)) <= 32) &&
			    !(width & (width - 1))) {
				for (pBgWin = pWin;
				     pBgWin->backgroundState == ParentRelative;
				     pBgWin = pBgWin->parent)
					;
				if (depth == 8) {
					cfbCopyRotatePixmap(pWin->border.pixmap,
							    &pPrivWin->pRotatedBorder,
							    pBgWin->drawable.x,
							    pBgWin->drawable.y);
				} else {
					cfb32CopyRotatePixmap(pWin->border.pixmap,
							      &pPrivWin->pRotatedBorder,
							      pBgWin->drawable.x,
							      pBgWin->drawable.y);
				}
				if (pPrivWin->pRotatedBorder) {
					pPrivWin->fastBorder = TRUE;
					pPrivWin->oldRotate.x = pBgWin->drawable.x;
					pPrivWin->oldRotate.y = pBgWin->drawable.y;
				} else
					pPrivWin->fastBorder = FALSE;
			} else
				pPrivWin->fastBorder = FALSE;
			break;
			
		case CWBorderPixel:
			pPrivWin->fastBorder = FALSE;
			break;
		}
	}
	return (TRUE);
}

static void
CreatorPaintWindow(WindowPtr pWin, RegionPtr pRegion, int what)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pWin->drawable.pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	register cfbPrivWin *pPrivWin;
	CreatorPrivWinPtr pFfbPrivWin;
	CreatorStipplePtr stipple;
	WindowPtr pBgWin;
	int depth = pWin->drawable.depth;

	if (pFfb->vtSema)
		return;

	FFBLOG(("CreatorPaintWindow: WIN(%p) what(%d)\n", pWin, what));
	pPrivWin = cfbGetWindowPrivate(pWin);
	pFfbPrivWin = CreatorGetWindowPrivate(pWin);
	switch (what) {
	case PW_BACKGROUND:
		stipple = pFfbPrivWin->Stipple;
		switch (pWin->backgroundState) {
		case None:
			return;
		case ParentRelative:
			do {
				pWin = pWin->parent;
			} while (pWin->backgroundState == ParentRelative);
			(*pWin->drawable.pScreen->PaintWindowBackground)(pWin, pRegion, what);
			return;
		case BackgroundPixmap:
			if (stipple) {
				CreatorFillBoxStipple ((DrawablePtr)pWin, 
						       (int)REGION_NUM_RECTS(pRegion),
						       REGION_RECTS(pRegion),
						       stipple);
				return;
			}
			FFB_ATTR_SFB_VAR_WIN(pFfb, 0x00ffffff, GXcopy, pWin);
			FFBWait(pFfb, ffb);
			if (pPrivWin->fastBackground) {
				if (depth == 8) {
					cfbFillBoxTile32 ((DrawablePtr)pWin,
							  (int)REGION_NUM_RECTS(pRegion),
							  REGION_RECTS(pRegion),
							  pPrivWin->pRotatedBackground);
				} else {
					cfb32FillBoxTile32 ((DrawablePtr)pWin,
							    (int)REGION_NUM_RECTS(pRegion),
							    REGION_RECTS(pRegion),
							    pPrivWin->pRotatedBackground);
				}
			} else {
				if (depth == 8) {
					cfbFillBoxTileOdd ((DrawablePtr)pWin,
							   (int)REGION_NUM_RECTS(pRegion),
							   REGION_RECTS(pRegion),
							   pWin->background.pixmap,
							   (int) pWin->drawable.x,
							   (int) pWin->drawable.y);
				} else {
					cfb32FillBoxTileOdd ((DrawablePtr)pWin,
							     (int)REGION_NUM_RECTS(pRegion),
							     REGION_RECTS(pRegion),
							     pWin->background.pixmap,
							     (int) pWin->drawable.x,
							     (int) pWin->drawable.y);
				}
			}
			return;
		case BackgroundPixel:
			CreatorFillBoxSolid ((DrawablePtr)pWin,
					     (int)REGION_NUM_RECTS(pRegion),
					     REGION_RECTS(pRegion),
					     pWin->background.pixel);
			return;
		}
		break;
	case PW_BORDER:
		if (pWin->borderIsPixel) {
			CreatorFillBoxSolid ((DrawablePtr)pWin,
					     (int)REGION_NUM_RECTS(pRegion),
					     REGION_RECTS(pRegion),
					     pWin->border.pixel);
			return;
		}
		FFB_ATTR_SFB_VAR_WIN(pFfb, 0x00ffffff, GXcopy, pWin);
		FFBWait(pFfb, ffb);
		if (pPrivWin->fastBorder) {
			if (depth == 8) {
				cfbFillBoxTile32 ((DrawablePtr)pWin,
						  (int)REGION_NUM_RECTS(pRegion),
						  REGION_RECTS(pRegion),
						  pPrivWin->pRotatedBorder);
			} else {
				cfb32FillBoxTile32 ((DrawablePtr)pWin,
						    (int)REGION_NUM_RECTS(pRegion),
						    REGION_RECTS(pRegion),
						    pPrivWin->pRotatedBorder);
			}
		} else {
			for (pBgWin = pWin;
			     pBgWin->backgroundState == ParentRelative;
			     pBgWin = pBgWin->parent)
				;

			if (depth == 8) {
				cfbFillBoxTileOdd ((DrawablePtr)pWin,
						   (int)REGION_NUM_RECTS(pRegion),
						   REGION_RECTS(pRegion),
						   pWin->border.pixmap,
						   (int) pBgWin->drawable.x,
						   (int) pBgWin->drawable.y);
			} else {
				cfb32FillBoxTileOdd ((DrawablePtr)pWin,
						     (int)REGION_NUM_RECTS(pRegion),
						     REGION_RECTS(pRegion),
						     pWin->border.pixmap,
						     (int) pBgWin->drawable.x,
						     (int) pBgWin->drawable.y);
			}
		}
		return;
	}
}

static void 
CreatorCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr prgnSrc)
{
	ScreenPtr pScreen = pWin->drawable.pScreen;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pScreen);
	DDXPointPtr pptSrc;
	DDXPointPtr ppt;
	RegionRec rgnDst;
	BoxPtr pbox;
	int dx, dy;
	int i, nbox;
	WindowPtr pwinRoot;

	if (pFfb->vtSema)
		return;

	FFBLOG(("CreatorCopyWindow: WIN(%p)\n", pWin));

	REGION_INIT(pScreen, &rgnDst, NullBox, 0);

	dx = ptOldOrg.x - pWin->drawable.x;
	dy = ptOldOrg.y - pWin->drawable.y;
	REGION_TRANSLATE(pScreen, prgnSrc, -dx, -dy);
	REGION_INTERSECT(pScreen, &rgnDst, &pWin->borderClip, prgnSrc);

	pbox = REGION_RECTS(&rgnDst);
	nbox = REGION_NUM_RECTS(&rgnDst);
	if(!(pptSrc = (DDXPointPtr )ALLOCATE_LOCAL(nbox * sizeof(DDXPointRec))))
		return;

	ppt = pptSrc;
	for (i = nbox; --i >= 0; ppt++, pbox++) {
		ppt->x = pbox->x1 + dx;
		ppt->y = pbox->y1 + dy;
	}

	/* XXX Optimize this later to only gcopy/vcopy the 8bpp+WID plane
	 * XXX when possible.  -DaveM
	 */

	pwinRoot = WindowTable[pScreen->myNum];

	if (!pFfb->disable_vscroll && (!dx && dy)) {
		FFBPtr pFfb = GET_FFB_FROM_SCREEN(pScreen);

		FFB_ATTR_VSCROLL_WINCOPY(pFfb);
		CreatorDoVertBitblt ((DrawablePtr)pwinRoot, (DrawablePtr)pwinRoot,
				     GXcopy, &rgnDst, pptSrc, ~0L);
	} else {
		FFBPtr pFfb = GET_FFB_FROM_SCREEN(pScreen);
		ffb_fbcPtr ffb = pFfb->regs;

		FFB_ATTR_SFB_VAR_WINCOPY(pFfb);
		FFBWait(pFfb, ffb);
		CreatorDoBitblt ((DrawablePtr)pwinRoot, (DrawablePtr)pwinRoot,
				 GXcopy, &rgnDst, pptSrc, ~0L);
	}
	DEALLOCATE_LOCAL(pptSrc);
	REGION_UNINIT(pScreen, &rgnDst);
}

static void
CreatorSaveAreas(PixmapPtr pPixmap, RegionPtr prgnSave, int xorg, int yorg, WindowPtr pWin)
{
	ScreenPtr pScreen = pPixmap->drawable.pScreen;
	FFBPtr pFfb = GET_FFB_FROM_SCREEN(pScreen);
	cfb8_32WidScreenPtr pScreenPriv =
		CFB8_32WID_GET_SCREEN_PRIVATE(pScreen);
	ffb_fbcPtr ffb = pFfb->regs;
	register DDXPointPtr pPt;
	DDXPointPtr pPtsInit;
	register BoxPtr pBox;
	register int i;
	PixmapPtr pScrPix;

	if (pFfb->vtSema)
		return;

	FFBLOG(("CreatorSaveAreas: WIN(%p)\n", pWin));
	i = REGION_NUM_RECTS(prgnSave);
	pPtsInit = (DDXPointPtr)ALLOCATE_LOCAL(i * sizeof(DDXPointRec));
	if (!pPtsInit)
		return;
    
	pBox = REGION_RECTS(prgnSave);
	pPt = pPtsInit;
	while (--i >= 0) {
		pPt->x = pBox->x1 + xorg;
		pPt->y = pBox->y1 + yorg;
		pPt++;
		pBox++;
	}

	if (pWin->drawable.bitsPerPixel == 8)
		pScrPix = (PixmapPtr) pScreenPriv->pix8;
	else
		pScrPix = (PixmapPtr) pScreenPriv->pix32;

	/* SRC is the framebuffer, DST is a pixmap.  The SFB_VAR attributes may
	 * seem silly, but they are needed even in this case to handle
	 * double-buffered windows properly.
	 */
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0x00ffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	CreatorDoBitblt((DrawablePtr) pScrPix, (DrawablePtr)pPixmap,
			GXcopy, prgnSave, pPtsInit, ~0L);

	DEALLOCATE_LOCAL (pPtsInit);
}

static void
CreatorRestoreAreas(PixmapPtr pPixmap, RegionPtr prgnRestore, int xorg, int yorg, WindowPtr pWin)
{
	FFBPtr pFfb;
	ffb_fbcPtr ffb;
	register DDXPointPtr pPt;
	DDXPointPtr pPtsInit;
	register BoxPtr pBox;
	register int i;
	ScreenPtr pScreen = pPixmap->drawable.pScreen;
	cfb8_32WidScreenPtr pScreenPriv =
		CFB8_32WID_GET_SCREEN_PRIVATE(pScreen);
	PixmapPtr pScrPix;

	pFfb = GET_FFB_FROM_SCREEN(pScreen);
	if (pFfb->vtSema)
		return;

	FFBLOG(("CreatorRestoreAreas: WIN(%p)\n", pWin));
	i = REGION_NUM_RECTS(prgnRestore);
	pPtsInit = (DDXPointPtr)ALLOCATE_LOCAL(i*sizeof(DDXPointRec));
	if (!pPtsInit)
		return;
    
	pBox = REGION_RECTS(prgnRestore);
	pPt = pPtsInit;
	while (--i >= 0) {
		pPt->x = pBox->x1 - xorg;
		pPt->y = pBox->y1 - yorg;
		pPt++;
		pBox++;
	}

	if (pWin->drawable.bitsPerPixel == 8)
		pScrPix = (PixmapPtr) pScreenPriv->pix8;
	else
		pScrPix = (PixmapPtr) pScreenPriv->pix32;

	pFfb = GET_FFB_FROM_SCREEN(pScreen);
	ffb = pFfb->regs;

	/* SRC is a pixmap, DST is the framebuffer */
	FFB_ATTR_SFB_VAR_WIN(pFfb, 0x00ffffff, GXcopy, pWin);
	FFBWait(pFfb, ffb);
	CreatorDoBitblt((DrawablePtr)pPixmap, (DrawablePtr) pScrPix,
			GXcopy, prgnRestore, pPtsInit, ~0L);

	DEALLOCATE_LOCAL (pPtsInit);
}

static void
CreatorGetImage(DrawablePtr pDrawable, int sx, int sy, int w, int h, unsigned int format, unsigned long planeMask, char* pdstLine)
{
	BoxRec box;
	DDXPointRec ptSrc;
	RegionRec rgnDst;
	ScreenPtr pScreen;
	PixmapPtr pPixmap;

	FFBLOG(("CreatorGetImage: s[%08x:%08x] wh[%08x:%08x]\n", sx, sy, w, h));
	if ((w == 0) || (h == 0))
		return;
	if (pDrawable->bitsPerPixel == 1) {
		mfbGetImage(pDrawable, sx, sy, w, h, format, planeMask, pdstLine);
		return;
	}
	pScreen = pDrawable->pScreen;
	/*
	 * XFree86 DDX empties the root borderClip when the VT is
	 * switched away; this checks for that case
	 */
	if (!cfbDrawableEnabled (pDrawable))
		return;
	if(format == ZPixmap) {
		FFBPtr pFfb = GET_FFB_FROM_SCREEN (pScreen);
		ffb_fbcPtr ffb = pFfb->regs;

		/* We have to have the full planemask. */
		if (pDrawable->type == DRAWABLE_WINDOW) {
			WindowPtr pWin = (WindowPtr) pDrawable;

			FFB_ATTR_SFB_VAR_WIN(pFfb, 0x00ffffff, GXcopy, pWin);
			FFBWait(pFfb, ffb);
		}

		if (pDrawable->bitsPerPixel == 8) {
			if((planeMask & 0x000000ff) != 0x000000ff) {
				cfbGetImage(pDrawable, sx, sy, w, h,
					    format, planeMask, pdstLine);
				return;
			}
		} else {
			if((planeMask & 0x00ffffff) != 0x00ffffff) {
				cfb32GetImage(pDrawable, sx, sy, w, h,
					      format, planeMask, pdstLine);
				return;
			}
		}

		/* SRC is the framebuffer, DST is a pixmap */
		if (pDrawable->type == DRAWABLE_WINDOW && w == 1 && h == 1) {
			/* Benchmarks do this make sure the acceleration hardware
			 * has completed all of it's operations, therefore I feel
			 * it is not cheating to special case this because if
			 * anything it gives the benchmarks more accurate results.
			 */
			if (pDrawable->bitsPerPixel == 32) {
				unsigned char *sfb = (unsigned char *)pFfb->sfb32;
				unsigned int *dstPixel = (unsigned int *)pdstLine;
				unsigned int tmp;

				tmp = *((unsigned int *)(sfb +
							 ((sy + pDrawable->y) << 13) +
							 ((sx + pDrawable->x) << 2)));
				*dstPixel = (tmp & 0x00ffffff);
			} else {
				unsigned char *sfb = (unsigned char *)pFfb->sfb8r;
				unsigned char *dstPixel = (unsigned char *)pdstLine;

				*dstPixel = *((unsigned char *)(sfb +
								((sy + pDrawable->y) << 11) +
								((sx + pDrawable->x) << 0)));
			}
			return;
		}
		pPixmap = GetScratchPixmapHeader(pScreen, w, h, 
						 pDrawable->depth, pDrawable->bitsPerPixel,
						 PixmapBytePad(w,pDrawable->depth), (pointer)pdstLine);
		if (!pPixmap)
			return;
		ptSrc.x = sx + pDrawable->x;
		ptSrc.y = sy + pDrawable->y;
		box.x1 = 0;
		box.y1 = 0;
		box.x2 = w;
		box.y2 = h;
		REGION_INIT(pScreen, &rgnDst, &box, 1);
		CreatorDoBitblt(pDrawable, (DrawablePtr)pPixmap, GXcopy, &rgnDst,
				&ptSrc, planeMask);
		REGION_UNINIT(pScreen, &rgnDst);
		FreeScratchPixmapHeader(pPixmap);
	} else
		miGetImage(pDrawable, sx, sy, w, h, format, planeMask, pdstLine);
}

extern void
CreatorGetSpans(DrawablePtr pDrawable, int wMax, DDXPointPtr ppt,
		int *pwidth, int nspans, char *pchardstStart);

void
CreatorVtChange (ScreenPtr pScreen, int enter)
{
	FFBPtr pFfb = GET_FFB_FROM_SCREEN (pScreen);
	ffb_fbcPtr ffb = pFfb->regs;

	pFfb->rp_active = 1;
	FFBWait(pFfb, ffb);	
	pFfb->fifo_cache = -1;
	pFfb->fbc_cache = (FFB_FBC_WB_A | FFB_FBC_WM_COMBINED |
			   FFB_FBC_RB_A | FFB_FBC_SB_BOTH| FFB_FBC_XE_OFF |
			   FFB_FBC_ZE_OFF | FFB_FBC_YE_OFF | FFB_FBC_RGBE_MASK);
	pFfb->ppc_cache = (FFB_PPC_FW_DISABLE |
			   FFB_PPC_VCE_DISABLE | FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST |
			   FFB_PPC_XS_CONST | FFB_PPC_YS_CONST | FFB_PPC_ZS_CONST|
			   FFB_PPC_DCE_DISABLE | FFB_PPC_ABE_DISABLE | FFB_PPC_TBE_OPAQUE);

	pFfb->pmask_cache = ~0;
	pFfb->rop_cache = FFB_ROP_EDIT_BIT;
	pFfb->drawop_cache = FFB_DRAWOP_RECTANGLE;
	pFfb->fg_cache = pFfb->bg_cache = 0;
	pFfb->fontw_cache = 32;
	pFfb->fontinc_cache = (1 << 16) | 0;
	pFfb->laststipple = NULL;
	FFBFifo(pFfb, 9);
	ffb->fbc = pFfb->fbc_cache;
	ffb->ppc = pFfb->ppc_cache;
	ffb->pmask = pFfb->pmask_cache;
	ffb->rop = pFfb->rop_cache;
	ffb->drawop = pFfb->drawop_cache;
	ffb->fg = pFfb->fg_cache;
	ffb->bg = pFfb->bg_cache;
	ffb->fontw = pFfb->fontw_cache;
	ffb->fontinc = pFfb->fontinc_cache;
	pFfb->rp_active = 1;
	FFBWait(pFfb, ffb);

	/* Fixup the FBC/PPC caches to deal with actually using
	 * a WID for every ROP.
	 */
	pFfb->fbc_cache = (FFB_FBC_WB_A | FFB_FBC_WM_COMBINED |
			   FFB_FBC_RB_A | FFB_FBC_SB_BOTH | FFB_FBC_XE_ON |
			   FFB_FBC_ZE_OFF | FFB_FBC_YE_OFF | FFB_FBC_RGBE_ON);
	pFfb->ppc_cache &= ~FFB_PPC_XS_MASK;
	pFfb->ppc_cache |= FFB_PPC_XS_WID;
	pFfb->wid_cache = 0xff;
	FFBFifo(pFfb, 8);
	ffb->fbc = pFfb->fbc_cache;
	ffb->ppc = FFB_PPC_XS_WID;
	ffb->wid = pFfb->wid_cache;
	ffb->xpmask = 0xff;
	ffb->xclip = FFB_XCLIP_TEST_ALWAYS;
	ffb->cmp = 0x80808080;
	ffb->matchab = 0x80808080;
	ffb->magnab = 0x80808080;
	FFBWait(pFfb, ffb);
}

/* Multiplies and divides suck... */
static void CreatorAlignTabInit(FFBPtr pFfb)
{
	struct fastfill_parms *ffp = &FFB_FFPARMS(pFfb);
	short *tab = pFfb->Pf_AlignTab;
	int i;

	for(i = 0; i < 0x800; i++) {
		int alignval;

		alignval = (i / ffp->pagefill_width) * ffp->pagefill_width;
		*tab++ = alignval;
	}
}

static Bool
CreatorPositionWindow(WindowPtr pWin, int x, int y)
{
	if (pWin->drawable.bitsPerPixel == 8)
		return cfbPositionWindow(pWin, x, y);
	else
		return cfb32PositionWindow(pWin, x, y);
}

extern Bool CreatorCreateGC (GCPtr pGC);

#ifdef DEBUG_FFB
FILE *FDEBUG_FD = NULL;
#endif

BSFuncRec CreatorBSFuncRec = {
    CreatorSaveAreas,
    CreatorRestoreAreas,
    (BackingStoreSetClipmaskRgnProcPtr) 0,
    (BackingStoreGetImagePixmapProcPtr) 0,
    (BackingStoreGetSpansPixmapProcPtr) 0,
};

Bool FFBAccelInit (ScreenPtr pScreen, FFBPtr pFfb)
{
	ffb_fbcPtr ffb;

	if (serverGeneration != CreatorGeneration) {
		CreatorScreenPrivateIndex = AllocateScreenPrivateIndex ();
		if (CreatorScreenPrivateIndex == -1) return FALSE;
		CreatorGCPrivateIndex = AllocateGCPrivateIndex ();
		CreatorWindowPrivateIndex = AllocateWindowPrivateIndex ();
		CreatorGeneration = serverGeneration;
	}
	
	if (!AllocateGCPrivate(pScreen, CreatorGCPrivateIndex, sizeof(CreatorPrivGCRec)))
		return FALSE;
	if (!AllocateWindowPrivate(pScreen, CreatorWindowPrivateIndex, 0))
		return FALSE;
	pScreen->devPrivates[CreatorScreenPrivateIndex].ptr = pFfb;

	pFfb->fifo_cache = 0;
	ffb = pFfb->regs;

	/* Replace various screen functions. */
	pScreen->CreateGC = CreatorCreateGC;
	pScreen->CreateWindow = CreatorCreateWindow;
	pScreen->DestroyWindow = CreatorDestroyWindow;
	pScreen->PositionWindow = CreatorPositionWindow;
	pScreen->ChangeWindowAttributes = CreatorChangeWindowAttributes;
	pScreen->PaintWindowBackground = CreatorPaintWindow;
	pScreen->PaintWindowBorder = CreatorPaintWindow;
	pScreen->GetSpans = CreatorGetSpans;
	pScreen->CopyWindow = CreatorCopyWindow;
	pScreen->GetImage = CreatorGetImage;
	pScreen->BackingStoreFuncs = CreatorBSFuncRec;

	/* cfb8_32wid took over this to init the WID plane,
	 * and with how our code works that is not necessary.
	 */
	pScreen->WindowExposures = miWindowExposures;

	/* Set FFB line-bias for clipping. */
	miSetZeroLineBias(pScreen, OCTANT3 | OCTANT4 | OCTANT6 | OCTANT1);

	FFB_DEBUG_init();
	FDEBUG((FDEBUG_FD,
		"FFB: cfg0(%08x) cfg1(%08x) cfg2(%08x) cfg3(%08x) ppcfg(%08x)\n",
		ffb->fbcfg0, ffb->fbcfg1, ffb->fbcfg2, ffb->fbcfg3, ffb->ppcfg));

	/* Determine the current screen resolution type.  This is
	 * needed to figure out the fastfill/pagefill parameters.
	 */
	switch(ffb->fbcfg0 & FFB_FBCFG0_RES_MASK) {
	default:
	case FFB_FBCFG0_RES_STD:
		pFfb->ffb_res = ffb_res_standard;
		break;
	case FFB_FBCFG0_RES_HIGH:
		pFfb->ffb_res = ffb_res_high;
		break;
	case FFB_FBCFG0_RES_STEREO:
		pFfb->ffb_res = ffb_res_stereo;
		break;
	case FFB_FBCFG0_RES_PRTRAIT:
		pFfb->ffb_res = ffb_res_portrait;
		break;
	};
	CreatorAlignTabInit(pFfb);

	/* Next, determine the hwbug workarounds and feature enables
	 * we should be using on this board.
	 */
	pFfb->disable_pagefill = 0;
	pFfb->disable_vscroll = 0;
	pFfb->has_brline_bug = 0;
	pFfb->use_blkread_prefetch = 0;
	if (pFfb->ffb_type == ffb1_prototype ||
	    pFfb->ffb_type == ffb1_standard ||
	    pFfb->ffb_type == ffb1_speedsort) {
		pFfb->has_brline_bug = 1;
		if (pFfb->ffb_res == ffb_res_high)
			pFfb->disable_vscroll = 1;
		if (pFfb->ffb_res == ffb_res_high ||
		    pFfb->ffb_res == ffb_res_stereo)
			pFfb->disable_pagefill = 1;

	} else {
		/* FFB2 has blkread prefetch.  AFB supposedly does too
		 * but the chip locks up on me when I try to use it. -DaveM
		 */
#define AFB_PREFETCH_IS_BUGGY	1
		if (!AFB_PREFETCH_IS_BUGGY ||
		    (pFfb->ffb_type != afb_m3 &&
		     pFfb->ffb_type != afb_m6)) {
			pFfb->use_blkread_prefetch = 1;
		}
		/* XXX I still cannot get page/block fast fills
		 * XXX to work reliably on any of my AFB boards. -DaveM
		 */
#define AFB_FASTFILL_IS_BUGGY	1
		if (AFB_FASTFILL_IS_BUGGY &&
		    (pFfb->ffb_type == afb_m3 ||
		     pFfb->ffb_type == afb_m6))
			pFfb->disable_pagefill = 1;
	}
	pFfb->disable_fastfill_ap = 0;
	if (pFfb->ffb_res == ffb_res_stereo ||
	    pFfb->ffb_res == ffb_res_high)
		pFfb->disable_fastfill_ap = 1;

	pFfb->ppc_cache = (FFB_PPC_FW_DISABLE |
			   FFB_PPC_VCE_DISABLE | FFB_PPC_APE_DISABLE | FFB_PPC_CS_CONST |
			   FFB_PPC_XS_CONST | FFB_PPC_YS_CONST | FFB_PPC_ZS_CONST |
			   FFB_PPC_DCE_DISABLE | FFB_PPC_ABE_DISABLE | FFB_PPC_TBE_OPAQUE);

	pFfb->pmask_cache = ~0;
	pFfb->rop_cache = (FFB_ROP_ZERO | (FFB_ROP_NEW << 8));
	pFfb->drawop_cache = FFB_DRAWOP_RECTANGLE;
	pFfb->fg_cache = pFfb->bg_cache = 0;
	pFfb->fontw_cache = 32;
	pFfb->fontinc_cache = (1 << 16) | 0;
	pFfb->fbc_cache = (FFB_FBC_WB_A | FFB_FBC_WM_COMBINED |
			   FFB_FBC_RB_A | FFB_FBC_SB_BOTH | FFB_FBC_XE_OFF |
			   FFB_FBC_ZE_OFF | FFB_FBC_YE_OFF | FFB_FBC_RGBE_MASK);
	pFfb->laststipple = NULL;

	/* We will now clear the screen: we'll draw a rectangle covering all the
	 * viewscreen, using a 'blackness' ROP.
	 */
	FFBFifo(pFfb, 13);
	ffb->fbc = pFfb->fbc_cache;
	ffb->ppc = pFfb->ppc_cache;
	ffb->pmask = pFfb->pmask_cache;
	ffb->rop = pFfb->rop_cache;
	ffb->drawop = pFfb->drawop_cache;
	ffb->fg = pFfb->fg_cache;
	ffb->bg = pFfb->bg_cache;
	ffb->fontw = pFfb->fontw_cache;
	ffb->fontinc = pFfb->fontinc_cache;
	FFB_WRITE64(&ffb->by, 0, 0);
	FFB_WRITE64_2(&ffb->bh, pFfb->psdp->height, pFfb->psdp->width);
	pFfb->rp_active = 1;
	FFBWait(pFfb, ffb);
	
	/* Fixup the FBC/PPC caches to deal with actually using
	 * a WID for every ROP.
	 */
	pFfb->fbc_cache = (FFB_FBC_WB_A | FFB_FBC_WM_COMBINED |
			   FFB_FBC_RB_A | FFB_FBC_SB_BOTH | FFB_FBC_XE_ON |
			   FFB_FBC_ZE_OFF | FFB_FBC_YE_OFF | FFB_FBC_RGBE_ON);
	pFfb->ppc_cache &= ~FFB_PPC_XS_MASK;
	pFfb->ppc_cache |= FFB_PPC_XS_WID;
	pFfb->wid_cache = 0xff;
	FFBFifo(pFfb, 8);
	ffb->fbc = pFfb->fbc_cache;
	ffb->ppc = FFB_PPC_XS_WID;
	ffb->wid = pFfb->wid_cache;
	ffb->xpmask = 0xff;
	ffb->xclip = FFB_XCLIP_TEST_ALWAYS;
	ffb->cmp = 0x80808080;
	ffb->matchab = 0x80808080;
	ffb->magnab = 0x80808080;
	FFBWait(pFfb, ffb);

	/* Success */
	return TRUE;
}
