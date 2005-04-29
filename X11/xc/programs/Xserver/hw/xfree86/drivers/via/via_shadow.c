/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_shadow.c,v 1.3 2003/08/27 15:16:13 tsi Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "via_driver.h"
#include "shadowfb.h"
#include "servermd.h"


void
VIARefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int width, height, Bpp, FBPitch;
    unsigned char *src, *dst;
   
    Bpp = pScrn->bitsPerPixel >> 3;
    FBPitch = BitmapBytePad(pScrn->displayWidth * pScrn->bitsPerPixel);

    while (num--) {
        width = (pbox->x2 - pbox->x1) * Bpp;
        height = pbox->y2 - pbox->y1;
        src = pVia->ShadowPtr + (pbox->y1 * pVia->ShadowPitch) + 
              (pbox->x1 * Bpp);
        dst = pVia->FBStart + (pbox->y1 * FBPitch) + (pbox->x1 * Bpp);

        while (height--) {
            memcpy(dst, src, width);
            dst += FBPitch;
            src += pVia->ShadowPitch;
        }
    
        pbox++;
    }
} 


void
VIAPointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    VIAPtr pVia = VIAPTR(pScrn);
    int newX, newY;

    if (pVia->rotate == 1) {
        newX = pScrn->pScreen->height - y - 1;
        newY = x;
    } 
    else {
        newX = y;
        newY = pScrn->pScreen->width - x - 1;
        
    }

    (*pVia->PointerMoved)(index, newX, newY);
}


void
VIARefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD8 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pVia->rotate * pVia->ShadowPitch;

    while (num--) {
        width = pbox->x2 - pbox->x1;
        y1 = pbox->y1 & ~3;
        y2 = (pbox->y2 + 3) & ~3;
        height = (y2 - y1) >> 2;  /* in dwords */

        if (pVia->rotate == 1) {
            dstPtr = pVia->FBStart + (pbox->x1 * dstPitch) + 
                     pScrn->virtualX - y2;
            srcPtr = pVia->ShadowPtr + ((1 - y2) * srcPitch) + pbox->x1;
        } 
        else {
            dstPtr = pVia->FBStart + 
                     ((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
            srcPtr = pVia->ShadowPtr + (y1 * srcPitch) + pbox->x2 - 1;
        }

        while (width--) {
            src = srcPtr;
            dst = (CARD32*) dstPtr;
            count = height;
            
            while (count--) {
                *(dst++) = src[0] | (src[srcPitch] << 8) | 
                           (src[srcPitch * 2] << 16) | 
                           (src[srcPitch * 3] << 24);
                src += srcPitch * 4;
            }
            
            srcPtr += pVia->rotate;
            dstPtr += dstPitch;
        }

        pbox++;
    }
} 


void
VIARefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD16 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pVia->rotate * pVia->ShadowPitch >> 1;

    while(num--) {
        width = pbox->x2 - pbox->x1;
        y1 = pbox->y1 & ~1;
        y2 = (pbox->y2 + 1) & ~1;
        height = (y2 - y1) >> 1;  /* in dwords */

        if (pVia->rotate == 1) {
            dstPtr = (CARD16*)pVia->FBStart + 
                     (pbox->x1 * dstPitch) + pScrn->virtualX - y2;
            srcPtr = (CARD16*)pVia->ShadowPtr + 
                     ((1 - y2) * srcPitch) + pbox->x1;
        } 
        else {
            dstPtr = (CARD16*)pVia->FBStart + 
                     ((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
            srcPtr = (CARD16*)pVia->ShadowPtr + 
                     (y1 * srcPitch) + pbox->x2 - 1;
        }

        while (width--) {
            src = srcPtr;
            dst = (CARD32*)dstPtr;
            count = height;
            
            while (count--) {
                *(dst++) = src[0] | (src[srcPitch] << 16);
                src += srcPitch * 2;
            }
            
            srcPtr += pVia->rotate;
            dstPtr += dstPitch;
        }

        pbox++;
    }
}


/* this one could be faster */
void
VIARefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD8 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    dstPitch = BitmapBytePad(pScrn->displayWidth * 24);
    srcPitch = -pVia->rotate * pVia->ShadowPitch;

    while (num--) {
        width = pbox->x2 - pbox->x1;
        y1 = pbox->y1 & ~3;
        y2 = (pbox->y2 + 3) & ~3;
        height = (y2 - y1) >> 2;  /* blocks of 3 dwords */

        if (pVia->rotate == 1) {
            dstPtr = pVia->FBStart + 
                     (pbox->x1 * dstPitch) + ((pScrn->virtualX - y2) * 3);
            srcPtr = pVia->ShadowPtr + ((1 - y2) * srcPitch) + (pbox->x1 * 3);
        } 
        else {
            dstPtr = pVia->FBStart + 
                     ((pScrn->virtualY - pbox->x2) * dstPitch) + (y1 * 3);
            srcPtr = pVia->ShadowPtr + (y1 * srcPitch) + (pbox->x2 * 3) - 3;
        }

        while (width--) {
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
            
            srcPtr += pVia->rotate * 3;
            dstPtr += dstPitch; 
        }

        pbox++;
    }
}


void
VIARefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int count, width, height, dstPitch, srcPitch;
    CARD32 *dstPtr, *srcPtr, *src, *dst;

    dstPitch = pScrn->displayWidth;
    srcPitch = -pVia->rotate * pVia->ShadowPitch >> 2;

    while (num--) {
        width = pbox->x2 - pbox->x1;
        height = pbox->y2 - pbox->y1;

        if (pVia->rotate == 1) {
            dstPtr = (CARD32*)pVia->FBStart + 
                     (pbox->x1 * dstPitch) + pScrn->virtualX - pbox->y2;
            srcPtr = (CARD32*)pVia->ShadowPtr + 
                     ((1 - pbox->y2) * srcPitch) + pbox->x1;
        } 
        else {
            dstPtr = (CARD32*)pVia->FBStart + 
                     ((pScrn->virtualY - pbox->x2) * dstPitch) + pbox->y1;
            srcPtr = (CARD32*)pVia->ShadowPtr + 
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
            
            srcPtr += pVia->rotate;
            dstPtr += dstPitch;
        }

        pbox++;
    }
}
