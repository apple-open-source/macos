/*
 * file rendition_shadow.h
 *
 * The functions used by ShadowFB
 * Based on code written by Mark Vojkovich <markv@valinux.com>
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/rendition/rendition_shadow.c,v 1.1 2000/03/01 00:25:25 dawes Exp $ */

#include "rendition.h"
#include "vtypes.h"
#include "rendition_shadow.h"
#include "shadowfb.h"
#include "servermd.h"



void
renditionRefreshArea(ScrnInfoPtr pScreenInfo, int num, BoxPtr pbox)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    int width, height, Bpp, FBPitch;
    unsigned char *src, *dst;
   
    Bpp = pScreenInfo->bitsPerPixel >> 3;
    FBPitch = BitmapBytePad(pScreenInfo->displayWidth * 
			    pScreenInfo->bitsPerPixel);

    while(num--) {
	width = (pbox->x2 - pbox->x1) * Bpp;
	height = pbox->y2 - pbox->y1;
	src = pRendition->board.shadowPtr + 
	      (pbox->y1 * pRendition->board.shadowPitch) + (pbox->x1 * Bpp);

	dst = pRendition->board.vmem_base+pRendition->board.fbOffset +
	      (pbox->y1 * FBPitch) + (pbox->x1 * Bpp);

	while(height--) {
	    memcpy(dst, src, width);
	    dst += FBPitch;
	    src += pRendition->board.shadowPitch;
	}
	
	pbox++;
    }
} 

void
renditionPointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScreenInfo = xf86Screens[index];
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    int newX, newY;

    if(pRendition->board.rotate == 1) {
	newX = pScreenInfo->pScreen->height - y - 1;
	newY = x;
    } else {
	newX = y;
	newY = pScreenInfo->pScreen->width - x - 1;
    }

    (*pRendition->board.PointerMoved)(index, newX, newY);
}

void
renditionRefreshArea8(ScrnInfoPtr pScreenInfo, int num, BoxPtr pbox)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD8 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScreenInfo->displayWidth;
    srcPitch = -pRendition->board.rotate * pRendition->board.shadowPitch;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~3;
	y2 = (pbox->y2 + 3) & ~3;
	height = (y2 - y1) >> 2;  /* in dwords */

	if(pRendition->board.rotate == 1) {
	    dstPtr = pRendition->board.vmem_base+pRendition->board.fbOffset +
  	             (pbox->x1 * dstPitch) + pScreenInfo->virtualX - y2;
	    srcPtr = pRendition->board.shadowPtr + ((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = pRendition->board.vmem_base+pRendition->board.fbOffset +
	             ((pScreenInfo->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = pRendition->board.shadowPtr + (y1 * srcPitch) + pbox->x2 - 1;
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
	    srcPtr += pRendition->board.rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
} 


void
renditionRefreshArea16(ScrnInfoPtr pScreenInfo, int num, BoxPtr pbox)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD16 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScreenInfo->displayWidth;
    srcPitch = -pRendition->board.rotate * pRendition->board.shadowPitch >> 1;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~1;
	y2 = (pbox->y2 + 1) & ~1;
	height = (y2 - y1) >> 1;  /* in dwords */

	if(pRendition->board.rotate == 1) {
	    dstPtr = (CARD16*)(pRendition->board.vmem_base+
	                      pRendition->board.fbOffset) +
			(pbox->x1 * dstPitch) + pScreenInfo->virtualX - y2;
	    srcPtr = (CARD16*)pRendition->board.shadowPtr + 
			((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD16*)(pRendition->board.vmem_base+
			       pRendition->board.fbOffset) + 
			((pScreenInfo->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = (CARD16*)pRendition->board.shadowPtr + 
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
	    srcPtr += pRendition->board.rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}


/* this one could be faster */
void
renditionRefreshArea24(ScrnInfoPtr pScreenInfo, int num, BoxPtr pbox)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD8 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = BitmapBytePad(pScreenInfo->displayWidth * 24);
    srcPitch = -pRendition->board.rotate * pRendition->board.shadowPitch;

    while(num--) {
        width = pbox->x2 - pbox->x1;
        y1 = pbox->y1 & ~3;
        y2 = (pbox->y2 + 3) & ~3;
        height = (y2 - y1) >> 2;  /* blocks of 3 dwords */

	if(pRendition->board.rotate == 1) {
	    dstPtr = pRendition->board.vmem_base+pRendition->board.fbOffset+
		     (pbox->x1 * dstPitch) + ((pScreenInfo->virtualX - y2) * 3);
	    srcPtr = pRendition->board.shadowPtr + ((1 - y2) * srcPitch) +
	             (pbox->x1 * 3);
	} else {
	    dstPtr = pRendition->board.vmem_base+pRendition->board.fbOffset +
	             ((pScreenInfo->virtualY - pbox->x2) * dstPitch) + (y1 * 3);
	    srcPtr = pRendition->board.shadowPtr + (y1 * srcPitch) +
	             (pbox->x2 * 3) - 3;
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
	    srcPtr += pRendition->board.rotate * 3;
	    dstPtr += dstPitch; 
	}

	pbox++;
    }
}

void
renditionRefreshArea32(ScrnInfoPtr pScreenInfo, int num, BoxPtr pbox)
{
    renditionPtr pRendition = RENDITIONPTR(pScreenInfo);
    int count, width, height, dstPitch, srcPitch;
    CARD32 *dstPtr, *srcPtr, *src, *dst;

    dstPitch = pScreenInfo->displayWidth;
    srcPitch = -pRendition->board.rotate * pRendition->board.shadowPitch >> 2;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	height = pbox->y2 - pbox->y1;

	if(pRendition->board.rotate == 1) {
	    dstPtr = (CARD32*)(pRendition->board.vmem_base+
			       pRendition->board.fbOffset) +
		     (pbox->x1 * dstPitch) + pScreenInfo->virtualX - pbox->y2;
	    srcPtr = (CARD32*)pRendition->board.shadowPtr + 
		     ((1 - pbox->y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD32*)(pRendition->board.vmem_base+
			       pRendition->board.fbOffset) + 
		     ((pScreenInfo->virtualY - pbox->x2) * dstPitch) + pbox->y1;
	    srcPtr = (CARD32*)pRendition->board.shadowPtr + 
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
	    srcPtr += pRendition->board.rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}

