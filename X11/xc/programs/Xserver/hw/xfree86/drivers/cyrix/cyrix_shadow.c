/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cyrix/cyrix_shadow.c,v 1.1 2002/11/06 11:38:59 alanh Exp $ */

/*
   Copyright (c) 1999,  The XFree86 Project Inc. 
   Written by Mark Vojkovich <markv@valinux.com>
*/

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "cyrix.h"
#include "shadowfb.h"
#include "servermd.h"



void
CYRIXRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
    int width, height, Bpp, FBPitch;
    unsigned char *src, *dst;
   
    Bpp = pScrn->bitsPerPixel >> 3;
    FBPitch = BitmapBytePad(pScrn->displayWidth * pScrn->bitsPerPixel);

    while(num--) {
	width = (pbox->x2 - pbox->x1) * Bpp;
	height = pbox->y2 - pbox->y1;
	src = pCyrix->ShadowPtr + (pbox->y1 * pCyrix->ShadowPitch) + 
						(pbox->x1 * Bpp);
	dst = pCyrix->FbBase + (pbox->y1 * FBPitch) + (pbox->x1 * Bpp);

	while(height--) {
	    memcpy(dst, src, width);
	    dst += FBPitch;
	    src += pCyrix->ShadowPitch;
	}
	
	pbox++;
    }
} 

void
CYRIXPointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
    int newX, newY;

    if(pCyrix->Rotate == 1) {
	newX = pScrn->pScreen->height - y - 1;
	newY = x;
    } else {
	newX = y;
	newY = pScrn->pScreen->width - x - 1;
    }

    (*pCyrix->PointerMoved)(index, newX, newY);
}

void
CYRIXRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD8 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pCyrix->Rotate * pCyrix->ShadowPitch;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~3;
	y2 = (pbox->y2 + 3) & ~3;
	height = (y2 - y1) >> 2;  /* in dwords */

	if(pCyrix->Rotate == 1) {
	    dstPtr = pCyrix->FbBase + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	    srcPtr = pCyrix->ShadowPtr + ((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = pCyrix->FbBase + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = pCyrix->ShadowPtr + (y1 * srcPitch) + pbox->x2 - 1;
	}

	while(width--) {
	    src = srcPtr;
	    dst = (CARD32*)dstPtr;
	    count = height;
	    while(count--) {
		*(dst++) = src[0] | (src[srcPitch] << 8) | 
					(src[srcPitch * 2] << 16) | 
					(src[srcPitch * 3] << 24);
		src += srcPitch * 4;
	    }
	    srcPtr += pCyrix->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
} 


void
CYRIXRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD16 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pCyrix->Rotate * pCyrix->ShadowPitch >> 1;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~1;
	y2 = (pbox->y2 + 1) & ~1;
	height = (y2 - y1) >> 1;  /* in dwords */

	if(pCyrix->Rotate == 1) {
	    dstPtr = (CARD16*)pCyrix->FbBase + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	    srcPtr = (CARD16*)pCyrix->ShadowPtr + 
			((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD16*)pCyrix->FbBase + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = (CARD16*)pCyrix->ShadowPtr + 
			(y1 * srcPitch) + pbox->x2 - 1;
	}

	while(width--) {
	    src = srcPtr;
	    dst = (CARD32*)dstPtr;
	    count = height;
	    while(count--) {
		*(dst++) = src[0] | (src[srcPitch] << 16);
		src += srcPitch * 2;
	    }
	    srcPtr += pCyrix->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}

