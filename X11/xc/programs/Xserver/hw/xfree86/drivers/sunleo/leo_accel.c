/*
 * Acceleration for the LEO (ZX) framebuffer.
 *
 * Copyright (C) 1999, 2000 Jakub Jelinek (jakub@redhat.com)
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
 * JAKUB JELINEK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_accel.c,v 1.3 2001/03/03 22:41:34 tsi Exp $ */

#define	PSZ	32

#include	"scrnintstr.h"
#include	"pixmapstr.h"
#include	"regionstr.h"
#include	"mistruct.h"
#include	"fontstruct.h"
#include	"dixfontstr.h"
#include	"cfb.h"
#include	"cfbmskbits.h"
#include	"cfb8bit.h"
#include	"mibstore.h"
#include	"mifillarc.h"
#include	"miwideline.h"
#include	"fastblt.h"
#include	"mergerop.h"
#include	"migc.h"
#include	"mi.h"

#include	"leo.h"
#include	"leo_gc.h"

int	LeoScreenPrivateIndex;
int	LeoGCPrivateIndex;
int	LeoWindowPrivateIndex;
int	LeoGeneration;

int	leoRopTable[16] = {
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_ZERO,		/* GXclear */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW_AND_OLD,	/* GXand */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW_AND_NOLD,	/* GXandReverse */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW,		/* GXcopy */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NNEW_AND_OLD,	/* GXandInverted */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_OLD,		/* GXnoop */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW_XOR_OLD,	/* GXxor */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW_OR_OLD,	/* GXor */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NNEW_AND_NOLD,	/* GXnor */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NNEW_XOR_NOLD,	/* GXequiv */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NOLD,		/* GXinvert */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW_OR_NOLD,	/* GXorReverse */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NNEW,		/* GXcopyInverted */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NNEW_OR_OLD,	/* GXorInverted */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_NNEW_OR_NOLD,	/* GXnand */
	LEO_ATTR_RGBE_ENABLE|LEO_ROP_ONES		/* GXset */
};

static void
LeoCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr prgnSrc)
{
	ScreenPtr pScreen = pWin->drawable.pScreen;
	LeoPtr pLeo = LeoGetScreenPrivate (pScreen);
	DDXPointPtr pptSrc;
	DDXPointPtr ppt;
	RegionPtr prgnDst;
	BoxPtr pbox;
	int dx, dy;
	int i, nbox;
	WindowPtr pwinRoot;

	if (pLeo->vtSema)
		return;

	dx = ptOldOrg.x - pWin->drawable.x;
	dy = ptOldOrg.y - pWin->drawable.y;

	pwinRoot = WindowTable[pWin->drawable.pScreen->myNum];

	prgnDst = REGION_CREATE(pWin->drawable.pScreen, NULL, 1);

	REGION_TRANSLATE(pWin->drawable.pScreen, prgnSrc, -dx, -dy);
	REGION_INTERSECT(pWin->drawable.pScreen, prgnDst, &pWin->borderClip, prgnSrc);

	pbox = REGION_RECTS(prgnDst);
	nbox = REGION_NUM_RECTS(prgnDst);
	if(!(pptSrc = (DDXPointPtr )ALLOCATE_LOCAL(nbox * sizeof(DDXPointRec))))
		return;
	ppt = pptSrc;

	for (i = nbox; --i >= 0; ppt++, pbox++) {
		ppt->x = pbox->x1 + dx;
		ppt->y = pbox->y1 + dy;
	}

	LeoDoBitblt ((DrawablePtr)pwinRoot, (DrawablePtr)pwinRoot,
		     GXcopy, prgnDst, pptSrc, ~0L);
	DEALLOCATE_LOCAL(pptSrc);
	REGION_DESTROY(pWin->drawable.pScreen, prgnDst);
}

void LeoVtChange (ScreenPtr pScreen, int enter)
{
	LeoPtr pLeo = LeoGetScreenPrivate (pScreen); 
	LeoCommand0 *lc0 = pLeo->lc0;
	LeoDraw *ld0 = pLeo->ld0;

	ld0->wid = 1;
	ld0->widclip = 0;
	ld0->wmask = 0xffff;
	ld0->planemask = 0xffffff;
	ld0->rop = LEO_ATTR_WE_ENABLE|LEO_ATTR_RGBE_ENABLE|LEO_ATTR_FORCE_WID;
	ld0->fg = 0;
	ld0->vclipmin = 0;
	ld0->vclipmax = (pLeo->psdp->width - 1) | ((pLeo->psdp->height - 1) << 16);
	
	while (lc0->csr & LEO_CSR_BLT_BUSY);
	
	lc0->extent = (pLeo->psdp->width - 1) | ((pLeo->psdp->height - 1) << 11);
	lc0->fill = 0;
	
	while (lc0->csr & LEO_CSR_BLT_BUSY);
	
	lc0->addrspace = LEO_ADDRSPC_OBGR;
	ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;
}

extern Bool LeoCreateGC (GCPtr pGC);

Bool LeoAccelInit (ScreenPtr pScreen, LeoPtr pLeo)
{
	LeoCommand0 *lc0;
	LeoDraw *ld0;

	if (serverGeneration != LeoGeneration) {
		LeoScreenPrivateIndex = AllocateScreenPrivateIndex ();
		if (LeoScreenPrivateIndex == -1) return FALSE;
		LeoGCPrivateIndex = AllocateGCPrivateIndex ();
		LeoWindowPrivateIndex = AllocateWindowPrivateIndex ();
		LeoGeneration = serverGeneration;
	}
	
	/* Allocate private structures holding pointer to both videoRAM and control registers.
	   We do not have to map these by ourselves, because the XServer did it for us; we
	   only copy the pointers to out structures. */
	if (!AllocateGCPrivate(pScreen, LeoGCPrivateIndex, sizeof(LeoPrivGCRec))) return FALSE;
	if (!AllocateWindowPrivate(pScreen, LeoWindowPrivateIndex, 0)) return FALSE;
	pScreen->devPrivates[LeoScreenPrivateIndex].ptr = pLeo;
	pLeo->lc0 = lc0 = (LeoCommand0 *) ((char *)pLeo->fb + LEO_LC0_VOFF);
	pLeo->ld0 = ld0 = (LeoDraw *) ((char *)pLeo->fb + LEO_LD0_VOFF);

	if (!pLeo->NoAccel) {
		/* Replace various screen functions. */
		pScreen->CreateGC = LeoCreateGC;
		pScreen->CopyWindow = LeoCopyWindow;
	}

	/* We will now clear the screen: we'll draw a rectangle covering all the
	 * viewscreen, using a 'blackness' ROP.
	 */
	ld0->wid = 1;
	ld0->widclip = 0;
	ld0->wmask = 0xffff;
	ld0->planemask = 0xffffff;
	ld0->rop = LEO_ATTR_WE_ENABLE|LEO_ATTR_RGBE_ENABLE|LEO_ATTR_FORCE_WID;
	ld0->fg = 0;
	ld0->vclipmin = 0;
	ld0->vclipmax = (pLeo->psdp->width - 1) | ((pLeo->psdp->height - 1) << 16);
	pLeo->vclipmax = (pLeo->psdp->width - 1) | ((pLeo->psdp->height - 1) << 16);
	pLeo->width = pLeo->psdp->width;
	pLeo->height = pLeo->psdp->height;
	
	while (lc0->csr & LEO_CSR_BLT_BUSY);
	
	lc0->extent = (pLeo->psdp->width - 1) | ((pLeo->psdp->height - 1) << 11);
	lc0->fill = 0;
	
	while (lc0->csr & LEO_CSR_BLT_BUSY);
	
	lc0->addrspace = LEO_ADDRSPC_OBGR;
	ld0->rop = LEO_ATTR_RGBE_ENABLE|LEO_ROP_NEW;

	/* Success */
	return TRUE;
}
