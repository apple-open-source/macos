/* $XFree86: xc/programs/Xserver/hw/xfree86/xf24_32bpp/cfbwindow.c,v 1.2 1999/02/28 11:19:50 dawes Exp $ */

#include "X.h"
#include "windowstr.h"
#include "regionstr.h"
#include "pixmapstr.h"
#include "scrnintstr.h"
#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb24.h"
#include "cfb32.h"
#include "cfb24_32.h"
#include "mi.h"


Bool
cfb24_32CreateWindow(WindowPtr pWin)
{
    cfbPrivWin *pPrivWin = cfbGetWindowPrivate(pWin);

    pPrivWin->fastBackground = FALSE;
    pPrivWin->fastBorder = FALSE;

    pWin->drawable.bitsPerPixel = 24;
    return TRUE;
}


Bool
cfb24_32DestroyWindow(WindowPtr pWin)
{
    return TRUE;
}

Bool
cfb24_32PositionWindow(
    WindowPtr pWin,
    int x, int y
){
    return TRUE;
}


Bool
cfb24_32ChangeWindowAttributes(
    WindowPtr pWin,
    unsigned long mask
){ 
    cfb24_32PixmapPtr pixPriv;
    PixmapPtr pPix;

    /* The dix layer may have incremented a refcnt.  We sync them here */

    if((mask & CWBackPixmap) && (pWin->backgroundState == BackgroundPixmap)) {
	pPix = pWin->background.pixmap;
	pixPriv = CFB24_32_GET_PIXMAP_PRIVATE(pPix);

	if(pixPriv->pix && (pPix->refcnt != pixPriv->pix->refcnt))
	    pixPriv->pix->refcnt = pPix->refcnt;

	if(pPix->drawable.bitsPerPixel != 24)
	    pWin->background.pixmap = cfb24_32RefreshPixmap(pPix);
    }	

    if((mask & CWBorderPixmap) && !pWin->borderIsPixel) {
	pPix = pWin->border.pixmap;
	pixPriv = CFB24_32_GET_PIXMAP_PRIVATE(pPix);

	if(pixPriv->pix && (pPix->refcnt != pixPriv->pix->refcnt))
	    pixPriv->pix->refcnt = pPix->refcnt;

	if(pPix->drawable.bitsPerPixel != 24)
	    pWin->border.pixmap = cfb24_32RefreshPixmap(pPix);
    }

    return TRUE;
}

extern WindowPtr *WindowTable;

void 
cfb24_32CopyWindow(
    WindowPtr pWin,
    DDXPointRec ptOldOrg,
    RegionPtr prgnSrc
){
    DDXPointPtr pptSrc, ppt;
    RegionRec rgnDst;
    BoxPtr pbox;
    int i, nbox, dx, dy;
    WindowPtr pwinRoot;

    pwinRoot = WindowTable[pWin->drawable.pScreen->myNum];

    REGION_INIT(pWin->drawable.pScreen, &rgnDst, NullBox, 0);

    dx = ptOldOrg.x - pWin->drawable.x;
    dy = ptOldOrg.y - pWin->drawable.y;
    REGION_TRANSLATE(pWin->drawable.pScreen, prgnSrc, -dx, -dy);
    REGION_INTERSECT(pWin->drawable.pScreen, &rgnDst, 
			&pWin->borderClip, prgnSrc);

    pbox = REGION_RECTS(&rgnDst);
    nbox = REGION_NUM_RECTS(&rgnDst);
    if(!nbox || 
	!(pptSrc = (DDXPointPtr )ALLOCATE_LOCAL(nbox * sizeof(DDXPointRec))))
    {
	REGION_UNINIT(pWin->drawable.pScreen, &rgnDst);
	return;
    }
    ppt = pptSrc;

    for (i = nbox; --i >= 0; ppt++, pbox++)
    {
	ppt->x = pbox->x1 + dx;
	ppt->y = pbox->y1 + dy;
    }

    cfb24_32DoBitblt24To24GXcopy((DrawablePtr)pwinRoot, (DrawablePtr)pwinRoot,
		GXcopy, &rgnDst, pptSrc, ~0L, 0);
    DEALLOCATE_LOCAL(pptSrc);
    REGION_UNINIT(pWin->drawable.pScreen, &rgnDst);
}
