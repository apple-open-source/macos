/*
   Copyright (c) 1999,  The XFree86 Project Inc. 
   Written by Mark Vojkovich <markv@valinux.com>
*/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_shadow.c,v 1.6 2001/01/22 21:32:36 dawes Exp $ */

#include "nv_local.h"
#include "nv_include.h"
#include "nv_type.h"
#include "shadowfb.h"
#include "servermd.h"


void
NVRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    NVPtr pNv = NVPTR(pScrn);
    int width, height, Bpp, FBPitch;
    unsigned char *src, *dst;
   
    Bpp = pScrn->bitsPerPixel >> 3;
    FBPitch = BitmapBytePad(pScrn->displayWidth * pScrn->bitsPerPixel);

    while(num--) {
	width = (pbox->x2 - pbox->x1) * Bpp;
	height = pbox->y2 - pbox->y1;
	src = pNv->ShadowPtr + (pbox->y1 * pNv->ShadowPitch) + 
						(pbox->x1 * Bpp);
	dst = pNv->FbStart + (pbox->y1 * FBPitch) + (pbox->x1 * Bpp);

	while(height--) {
	    memcpy(dst, src, width);
	    dst += FBPitch;
	    src += pNv->ShadowPitch;
	}
	
	pbox++;
    }
} 

void
NVPointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    NVPtr pNv = NVPTR(pScrn);
    int newX, newY;

    if(pNv->Rotate == 1) {
	newX = pScrn->pScreen->height - y - 1;
	newY = x;
    } else {
	newX = y;
	newY = pScrn->pScreen->width - x - 1;
    }

    (*pNv->PointerMoved)(index, newX, newY);
}

void
NVRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    NVPtr pNv = NVPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD8 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pNv->Rotate * pNv->ShadowPitch;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~3;
	y2 = (pbox->y2 + 3) & ~3;
	height = (y2 - y1) >> 2;  /* in dwords */

	if(pNv->Rotate == 1) {
	    dstPtr = pNv->FbStart + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	    srcPtr = pNv->ShadowPtr + ((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = pNv->FbStart + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = pNv->ShadowPtr + (y1 * srcPitch) + pbox->x2 - 1;
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
	    srcPtr += pNv->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
} 


void
NVRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    NVPtr pNv = NVPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD16 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pNv->Rotate * pNv->ShadowPitch >> 1;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~1;
	y2 = (pbox->y2 + 1) & ~1;
	height = (y2 - y1) >> 1;  /* in dwords */

	if(pNv->Rotate == 1) {
	    dstPtr = (CARD16*)pNv->FbStart + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	    srcPtr = (CARD16*)pNv->ShadowPtr + 
			((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD16*)pNv->FbStart + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = (CARD16*)pNv->ShadowPtr + 
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
	    srcPtr += pNv->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}


void
NVRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    NVPtr pNv = NVPTR(pScrn);
    int count, width, height, dstPitch, srcPitch;
    CARD32 *dstPtr, *srcPtr, *src, *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pNv->Rotate * pNv->ShadowPitch >> 2;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	height = pbox->y2 - pbox->y1;

	if(pNv->Rotate == 1) {
	    dstPtr = (CARD32*)pNv->FbStart + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - pbox->y2;
	    srcPtr = (CARD32*)pNv->ShadowPtr + 
			((1 - pbox->y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD32*)pNv->FbStart + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + pbox->y1;
	    srcPtr = (CARD32*)pNv->ShadowPtr + 
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
	    srcPtr += pNv->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}



