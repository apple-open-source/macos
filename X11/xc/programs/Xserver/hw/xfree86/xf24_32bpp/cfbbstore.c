/* $XFree86: xc/programs/Xserver/hw/xfree86/xf24_32bpp/cfbbstore.c,v 1.1 1999/01/23 09:56:13 dawes Exp $ */

#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb24.h"
#include "cfb32.h"
#include "cfb24_32.h"
#include "X.h"
#include "mibstore.h"
#include "regionstr.h"
#include "scrnintstr.h"
#include "pixmapstr.h"
#include "windowstr.h"

void
cfb24_32SaveAreas(
    PixmapPtr	  	pPixmap,
    RegionPtr	  	prgnSave, 
    int	    	  	xorg,
    int	    	  	yorg,
    WindowPtr		pWin
){
    DDXPointPtr pPt;
    DDXPointPtr	pPtsInit;
    BoxPtr	pBox;
    ScreenPtr	pScreen = pPixmap->drawable.pScreen;
    PixmapPtr	pScrPix;
    int		i = REGION_NUM_RECTS(prgnSave);

    
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

    pScrPix = (PixmapPtr) pScreen->devPrivate;

    cfbDoBitblt24To32((DrawablePtr) pScrPix, (DrawablePtr)pPixmap,
		    GXcopy, prgnSave, pPtsInit, ~0L, 0);

    DEALLOCATE_LOCAL (pPtsInit);
}


void
cfb24_32RestoreAreas(
    PixmapPtr	  	pPixmap, 
    RegionPtr	  	prgnRestore,
    int	    	  	xorg,
    int	    	  	yorg,
    WindowPtr		pWin
){
    DDXPointPtr pPt;
    DDXPointPtr pPtsInit;
    BoxPtr	pBox;
    int		i;
    ScreenPtr	pScreen = pPixmap->drawable.pScreen;
    PixmapPtr	pScrPix;

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

    pScrPix = (PixmapPtr) pScreen->devPrivate;

    cfbDoBitblt32To24((DrawablePtr)pPixmap, (DrawablePtr) pScrPix,
		    GXcopy, prgnRestore, pPtsInit, ~0L, 0);

    DEALLOCATE_LOCAL (pPtsInit);
}
