/* $XFree86: xc/programs/Xserver/hw/xfree86/xf24_32bpp/cfbgcmisc.c,v 1.1 1999/01/23 09:56:14 dawes Exp $ */

#include "X.h"
#include "Xmd.h"
#include "Xproto.h"
#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb24.h"
#include "cfb32.h"
#include "cfb24_32.h"
#include "fontstruct.h"
#include "dixfontstr.h"
#include "gcstruct.h"
#include "windowstr.h"
#include "pixmapstr.h"
#include "scrnintstr.h"
#include "region.h"

#include "mistruct.h"
#include "mibstore.h"
#include "migc.h"


static void cfb24_32ValidateGC(GCPtr, unsigned long, DrawablePtr);
static void cfb24_32DestroyGC(GCPtr pGC);
static void cfb24_32ChangeGC(GCPtr pGC, unsigned long mask);
static void cfb24_32CopyGC(GCPtr pGCSrc, unsigned long changes, GCPtr pGCDst);

static
GCFuncs cfb24_32GCFuncs = {
    cfb24_32ValidateGC,
    cfb24_32ChangeGC,
    cfb24_32CopyGC,
    cfb24_32DestroyGC,
    miChangeClip,
    miDestroyClip,
    miCopyClip,
};

static void
cfb24_32DestroyGC(GCPtr pGC)
{
    cfb24_32GCPtr pGCPriv = CFB24_32_GET_GC_PRIVATE(pGC);

    if (pGC->freeCompClip)
        REGION_DESTROY(pGC->pScreen, pGC->pCompositeClip);
    if(pGCPriv->Ops24bpp)
	miDestroyGCOps(pGCPriv->Ops24bpp);
    if(pGCPriv->Ops32bpp)
	miDestroyGCOps(pGCPriv->Ops32bpp);
}

static void
cfb24_32ChangeGC(
    GCPtr           pGC,
    unsigned long   mask
){
   
   if((mask & GCTile) && pGC->tile.pixmap && !pGC->tileIsPixel) {
	PixmapPtr pPix = pGC->tile.pixmap;
	cfb24_32PixmapPtr pixPriv = CFB24_32_GET_PIXMAP_PRIVATE(pPix);

	if(pixPriv->pix && (pPix->refcnt != pixPriv->pix->refcnt))
	    pixPriv->pix->refcnt = pPix->refcnt;
   }

   return;
}

static void
cfb24_32CopyGC(
    GCPtr           pGCSrc,
    unsigned long   changes,
    GCPtr           pGCDst
){
   if((changes & GCTile) && pGCDst->tile.pixmap && !pGCDst->tileIsPixel) {
	PixmapPtr pPix = pGCDst->tile.pixmap;
	cfb24_32PixmapPtr pixPriv = CFB24_32_GET_PIXMAP_PRIVATE(pPix);

	if(pixPriv->pix && (pPix->refcnt != pixPriv->pix->refcnt))
	    pixPriv->pix->refcnt = pPix->refcnt;
   }

   return;
}



Bool
cfb24_32CreateGC(GCPtr pGC)
{
    cfb24_32GCPtr pGCPriv;
    cfbPrivGC *pPriv;

    if (PixmapWidthPaddingInfo[pGC->depth].padPixelsLog2 == LOG2_BITMAP_PAD)
        return (mfbCreateGC(pGC));

    pGC->clientClip = NULL;
    pGC->clientClipType = CT_NONE;
    pGC->miTranslate = 1;
    pGC->fExpose = TRUE;
    pGC->freeCompClip = FALSE;
    pGC->pRotatedPixmap = (PixmapPtr) NULL;

    pPriv = cfbGetGCPrivate(pGC);
    pPriv->rop = pGC->alu;
    pPriv->oneRect = FALSE;

    pGC->ops = NULL;
    pGC->funcs = &cfb24_32GCFuncs;

    pGCPriv = CFB24_32_GET_GC_PRIVATE(pGC);
    pGCPriv->Ops24bpp = NULL;
    pGCPriv->Ops32bpp = NULL;
    pGCPriv->OpsAre24bpp = FALSE;
    pGCPriv->changes = 0;
	
    return TRUE;
}


static void
cfb24_32ValidateGC(
    GCPtr pGC,
    unsigned long changes,
    DrawablePtr pDraw
){
    cfb24_32GCPtr pGCPriv = CFB24_32_GET_GC_PRIVATE(pGC);

    if(pDraw->bitsPerPixel == 32) {
	if(pGCPriv->OpsAre24bpp) {
	    int origChanges = changes;
	    pGC->ops = pGCPriv->Ops32bpp;
	    changes |= pGCPriv->changes;
	    pGCPriv->changes = origChanges;
	    pGCPriv->OpsAre24bpp = FALSE;
	} else 
	    pGCPriv->changes |= changes;

	if((pGC->fillStyle == FillTiled) && 
           (pGC->tile.pixmap->drawable.bitsPerPixel == 24)){
	   pGC->tile.pixmap = cfb24_32RefreshPixmap(pGC->tile.pixmap);
	   changes |= GCTile;
	}

	cfb24_32ValidateGC32(pGC, changes, pDraw);
	pGCPriv->Ops32bpp = pGC->ops;
    } else {  /* bitsPerPixel == 24 */
	if(!pGCPriv->OpsAre24bpp) {
	    int origChanges = changes;
	    pGC->ops = pGCPriv->Ops24bpp;
	    changes |= pGCPriv->changes;
	    pGCPriv->changes = origChanges;
	    pGCPriv->OpsAre24bpp = TRUE;
	} else 
	    pGCPriv->changes |= changes;

	if((pGC->fillStyle == FillTiled) && 
           (pGC->tile.pixmap->drawable.bitsPerPixel == 32)){
	   pGC->tile.pixmap = cfb24_32RefreshPixmap(pGC->tile.pixmap);
	   changes |= GCTile;
	}

	cfb24_32ValidateGC24(pGC, changes, pDraw);
	pGCPriv->Ops24bpp = pGC->ops;
    }

}

