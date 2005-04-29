/*
   Copyright (c) 1999,  The XFree86 Project Inc. 
   Written by Mark Vojkovich <markv@valinux.com>
*/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/riva_shadow.c,v 1.1 2003/07/31 20:24:31 mvojkovi Exp $ */

#include "riva_local.h"
#include "riva_include.h"
#include "riva_type.h"
#include "shadowfb.h"
#include "servermd.h"


void
RivaRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    int width, height, Bpp, FBPitch;
    unsigned char *src, *dst;
   
    Bpp = pScrn->bitsPerPixel >> 3;
    FBPitch = BitmapBytePad(pScrn->displayWidth * pScrn->bitsPerPixel);

    while(num--) {
	width = (pbox->x2 - pbox->x1) * Bpp;
	height = pbox->y2 - pbox->y1;
	src = pRiva->ShadowPtr + (pbox->y1 * pRiva->ShadowPitch) + 
						(pbox->x1 * Bpp);
	dst = pRiva->FbStart + (pbox->y1 * FBPitch) + (pbox->x1 * Bpp);

	while(height--) {
	    memcpy(dst, src, width);
	    dst += FBPitch;
	    src += pRiva->ShadowPitch;
	}
	
	pbox++;
    }
} 

void
RivaPointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    RivaPtr pRiva = RivaPTR(pScrn);
    int newX, newY;

    if(pRiva->Rotate == 1) {
	newX = pScrn->pScreen->height - y - 1;
	newY = x;
    } else {
	newX = y;
	newY = pScrn->pScreen->width - x - 1;
    }

    (*pRiva->PointerMoved)(index, newX, newY);
}

void
RivaRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD8 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pRiva->Rotate * pRiva->ShadowPitch;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~3;
	y2 = (pbox->y2 + 3) & ~3;
	height = (y2 - y1) >> 2;  /* in dwords */

	if(pRiva->Rotate == 1) {
	    dstPtr = pRiva->FbStart + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	    srcPtr = pRiva->ShadowPtr + ((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = pRiva->FbStart + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = pRiva->ShadowPtr + (y1 * srcPitch) + pbox->x2 - 1;
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
	    srcPtr += pRiva->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
} 


void
RivaRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD16 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pRiva->Rotate * pRiva->ShadowPitch >> 1;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~1;
	y2 = (pbox->y2 + 1) & ~1;
	height = (y2 - y1) >> 1;  /* in dwords */

	if(pRiva->Rotate == 1) {
	    dstPtr = (CARD16*)pRiva->FbStart + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	    srcPtr = (CARD16*)pRiva->ShadowPtr + 
			((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD16*)pRiva->FbStart + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = (CARD16*)pRiva->ShadowPtr + 
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
	    srcPtr += pRiva->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}


void
RivaRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    int count, width, height, dstPitch, srcPitch;
    CARD32 *dstPtr, *srcPtr, *src, *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pRiva->Rotate * pRiva->ShadowPitch >> 2;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	height = pbox->y2 - pbox->y1;

	if(pRiva->Rotate == 1) {
	    dstPtr = (CARD32*)pRiva->FbStart + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - pbox->y2;
	    srcPtr = (CARD32*)pRiva->ShadowPtr + 
			((1 - pbox->y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD32*)pRiva->FbStart + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + pbox->y1;
	    srcPtr = (CARD32*)pRiva->ShadowPtr + 
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
	    srcPtr += pRiva->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}



