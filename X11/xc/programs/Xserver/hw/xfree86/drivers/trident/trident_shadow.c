/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident_shadow.c,v 1.3 2001/06/14 02:23:50 keithp Exp $ */

/*
   Copyright (c) 1999, 2000 The XFree86 Project Inc. 
   based on code written by Mark Vojkovich <markv@valinux.com>
*/

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "shadowfb.h"
#include "servermd.h"
#include "trident.h"

void
TRIDENTRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int width, height, Bpp, FBPitch;
    unsigned char *src, *dst;
   
    Bpp = pScrn->bitsPerPixel >> 3;
    FBPitch = BitmapBytePad(pScrn->displayWidth * pScrn->bitsPerPixel);

    while(num--) {
	width = (pbox->x2 - pbox->x1) * Bpp;
	height = pbox->y2 - pbox->y1;
	src = pTrident->ShadowPtr + (pbox->y1 * pTrident->ShadowPitch) + 
						(pbox->x1 * Bpp);
	dst = pTrident->FbBase + (pbox->y1 * FBPitch) + (pbox->x1 * Bpp);

	while(height--) {
	    memcpy(dst, src, width);
	    dst += FBPitch;
	    src += pTrident->ShadowPitch;
	}
	
	pbox++;
    }
} 

void
TRIDENTShadowUpdate (ScreenPtr pScreen, shadowBufPtr pBuf)
{
    RegionPtr damage = &pBuf->damage;
    ScrnInfoPtr pScrn;
    pScrn = xf86Screens[pScreen->myNum];
    
    (TRIDENTPTR(pScrn))->RefreshArea (pScrn, REGION_NUM_RECTS(damage), 
				      REGION_RECTS(damage));
}

void
TRIDENTPointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int newX, newY;

    if(pTrident->Rotate == 1) {
	newX = pScrn->pScreen->height - y - 1;
	newY = x;
    } else {
	newX = y;
	newY = pScrn->pScreen->width - x - 1;
    }

    (*pTrident->PointerMoved)(index, newX, newY);
}

void
TRIDENTRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD8 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pTrident->Rotate * pTrident->ShadowPitch;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~3;
	y2 = (pbox->y2 + 3) & ~3;
	height = (y2 - y1) >> 2;  /* in dwords */

	if(pTrident->Rotate == 1) {
	    dstPtr = pTrident->FbBase + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	    srcPtr = pTrident->ShadowPtr + ((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = pTrident->FbBase + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = pTrident->ShadowPtr + (y1 * srcPitch) + pbox->x2 - 1;
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
	    srcPtr += pTrident->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
} 


void
TRIDENTRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD16 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pTrident->Rotate * pTrident->ShadowPitch >> 1;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~1;
	y2 = (pbox->y2 + 1) & ~1;
	height = (y2 - y1) >> 1;  /* in dwords */

	if(pTrident->Rotate == 1) {
	    dstPtr = (CARD16*)pTrident->FbBase + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	    srcPtr = (CARD16*)pTrident->ShadowPtr + 
			((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD16*)pTrident->FbBase + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = (CARD16*)pTrident->ShadowPtr + 
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
	    srcPtr += pTrident->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}


/* this one could be faster */
void
TRIDENTRefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD8 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = BitmapBytePad(pScrn->displayWidth * 24);
    srcPitch = -pTrident->Rotate * pTrident->ShadowPitch;

    while(num--) {
        width = pbox->x2 - pbox->x1;
        y1 = pbox->y1 & ~3;
        y2 = (pbox->y2 + 3) & ~3;
        height = (y2 - y1) >> 2;  /* blocks of 3 dwords */

	if(pTrident->Rotate == 1) {
	    dstPtr = pTrident->FbBase + 
			(pbox->x1 * dstPitch) + ((pScrn->virtualX - y2) * 3);
	    srcPtr = pTrident->ShadowPtr + ((1 - y2) * srcPitch) + (pbox->x1 * 3);
	} else {
	    dstPtr = pTrident->FbBase + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + (y1 * 3);
	    srcPtr = pTrident->ShadowPtr + (y1 * srcPitch) + (pbox->x2 * 3) - 3;
	}

	while(width--) {
	    src = srcPtr;
	    dst = (CARD32*)dstPtr;
	    count = height;
	    while(count--) {
		dst[0] = src[0] | (src[1] << 8) | (src[2] << 16) |
				(src[srcPitch] << 24);		
		dst[1] = src[srcPitch + 1] | (src[srcPitch + 2] << 8) |
				(src[srcPitch * 2] << 16) |
				(src[(srcPitch * 2) + 1] << 24);		
		dst[2] = src[(srcPitch * 2) + 2] | (src[srcPitch * 3] << 8) |
				(src[(srcPitch * 3) + 1] << 16) |
				(src[(srcPitch * 3) + 2] << 24);	
		dst += 3;
		src += srcPitch * 4;
	    }
	    srcPtr += pTrident->Rotate * 3;
	    dstPtr += dstPitch; 
	}

	pbox++;
    }
}

void
TRIDENTRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int count, width, height, dstPitch, srcPitch;
    CARD32 *dstPtr, *srcPtr, *src, *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pTrident->Rotate * pTrident->ShadowPitch >> 2;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	height = pbox->y2 - pbox->y1;

	if(pTrident->Rotate == 1) {
	    dstPtr = (CARD32*)pTrident->FbBase + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - pbox->y2;
	    srcPtr = (CARD32*)pTrident->ShadowPtr + 
			((1 - pbox->y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD32*)pTrident->FbBase + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + pbox->y1;
	    srcPtr = (CARD32*)pTrident->ShadowPtr + 
			(pbox->y1 * srcPitch) + pbox->x2 - 1;
	}

	while(width--) {
	    src = srcPtr;
	    dst = dstPtr;
	    count = height;
	    while(count--) {
		*(dst++) = *src;
		src += srcPitch;
	    }
	    srcPtr += pTrident->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}


