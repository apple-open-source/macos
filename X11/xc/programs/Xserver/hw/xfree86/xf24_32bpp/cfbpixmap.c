/* $XFree86: xc/programs/Xserver/hw/xfree86/xf24_32bpp/cfbpixmap.c,v 1.3 2000/04/01 00:17:19 mvojkovi Exp $ */

#include "Xmd.h"
#include "servermd.h"
#include "scrnintstr.h"
#include "pixmapstr.h"
#include "mi.h"
#define PSZ 8
#include "cfb.h"
#undef PSZ
#include "cfb24_32.h"


PixmapPtr
cfb24_32CreatePixmap (
    ScreenPtr	pScreen,
    int		width,
    int		height,
    int		depth
){
    cfb24_32PixmapPtr pPixPriv;
    PixmapPtr pPix;
    int size, bpp, pitch;

    /* All depth 24 pixmaps are 24bpp unless the caller is allocating
	its own data (width == 0) */

    if(depth == 1) {
	pitch = ((width + 31) >> 5) << 2;
	bpp = 1;
    } else {  /* depth == 24 */
	pitch = ((width * 3) + 3) & ~3L;
	bpp = (width && height) ? 24 : 32;
    }

    size = height * pitch;
    pPix = AllocatePixmap(pScreen, size);
    if (!pPix)
	return NullPixmap;
    pPix->drawable.type = DRAWABLE_PIXMAP;
    pPix->drawable.class = 0;
    pPix->drawable.pScreen = pScreen;
    pPix->drawable.depth = depth;
    pPix->drawable.bitsPerPixel = bpp;
    pPix->drawable.id = 0;
    pPix->drawable.serialNumber = NEXT_SERIAL_NUMBER;
    pPix->drawable.x = 0;
    pPix->drawable.y = 0;
    pPix->drawable.width = width;
    pPix->drawable.height = height;
    pPix->devKind = pitch;
    pPix->refcnt = 1;
    pPix->devPrivate.ptr = size ? 
	(pointer)((char*)pPix + pScreen->totalPixmapSize) : NULL;

    pPixPriv = CFB24_32_GET_PIXMAP_PRIVATE(pPix);
    pPixPriv->pix = NULL;           /* no clone yet */
    pPixPriv->freePrivate = FALSE;
    pPixPriv->isRefPix = FALSE;

    return pPix;
}

Bool
cfb24_32DestroyPixmap(PixmapPtr pPix)
{
    cfb24_32PixmapPtr pPriv = CFB24_32_GET_PIXMAP_PRIVATE(pPix);
    PixmapPtr pClone = pPriv->pix;

    if(pClone) {
	cfb24_32PixmapPtr cPriv = CFB24_32_GET_PIXMAP_PRIVATE(pClone);
	int refcnt = pPix->refcnt;
	cPriv->pix = NULL; /* avoid looping back */
	
	if(refcnt != pClone->refcnt)
	   ErrorF("Pixmap refcnt mismatch in DestroyPixmap()\n");

	(*pPix->drawable.pScreen->DestroyPixmap)(pClone);

	if(refcnt > 1)
	   cPriv->pix = pPix;
    }

    if(--pPix->refcnt)
	return TRUE;

    if(pPriv->freePrivate)
	xfree(pPix->devPrivate.ptr);
    xfree(pPix);

    return TRUE;
}

PixmapPtr
cfb24_32RefreshPixmap(PixmapPtr pPix) 
{
    cfb24_32PixmapPtr newPixPriv, pixPriv = CFB24_32_GET_PIXMAP_PRIVATE(pPix);
    ScreenPtr pScreen = pPix->drawable.pScreen;
    int width = pPix->drawable.width;
    int height = pPix->drawable.height;
    GCPtr pGC;

    if(pPix->drawable.bitsPerPixel == 24) {
        if(!pixPriv->pix) {	/* need to make a 32bpp clone */
	    int pitch = width << 2;
	    unsigned char* data;
	    PixmapPtr newPix;

	    if(!(data = (unsigned char*)xalloc(pitch * height)))
		FatalError("Out of memory\n");

	    /* cfb24_32CreatePixmap will make a 32bpp header for us */
	    newPix = (*pScreen->CreatePixmap)(pScreen, 0, 0, 24);
	    newPix->devKind = pitch;
	    newPix->devPrivate.ptr = (pointer)data;
	    newPix->drawable.width = width;
	    newPix->drawable.height = height;
	    newPix->refcnt = pPix->refcnt;
	    pixPriv->pix = newPix;
	    newPixPriv = CFB24_32_GET_PIXMAP_PRIVATE(newPix);
	    newPixPriv->pix = pPix;
	    newPixPriv->freePrivate = TRUE;
	    pixPriv->isRefPix = TRUE;
	}
    } else { /* bitsPerPixel == 32 */
        if(!pixPriv->pix) {	/* need to make a 32bpp clone */

	    /* cfb24_32CreatePixmap will make a 24bpp pixmap for us */
	    pixPriv->pix = (*pScreen->CreatePixmap)(pScreen, width, height, 24);
	    pixPriv->pix->refcnt = pPix->refcnt;
	    newPixPriv = CFB24_32_GET_PIXMAP_PRIVATE(pixPriv->pix);
	    newPixPriv->pix = pPix;
	    pixPriv->isRefPix = TRUE;
	}
    }

    if(pPix->refcnt != pixPriv->pix->refcnt)
	ErrorF("Pixmap refcnt mismatch in RefreshPixmap()\n");

    /* make sure copies only go from the real to the clone */
    if(pixPriv->isRefPix) {
	pGC = GetScratchGC(24, pScreen);
	ValidateGC((DrawablePtr)pixPriv->pix, pGC);
	(*pGC->ops->CopyArea)((DrawablePtr)pPix, (DrawablePtr)pixPriv->pix,
					pGC, 0, 0, width, height, 0, 0);
	FreeScratchGC(pGC);
    }

    return pixPriv->pix;
}

