/* $XConsortium: nv_driver.c /main/3 1996/10/28 05:13:37 kaleb $ */
/*
 * Copyright 1996-1997  David J. McKay
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * DAVID J. MCKAY BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Rewritten with reference from mga driver and 3.3.4 NVIDIA driver by
   Jarno Paananen <jpaana@s2.org> */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_cursor.c,v 1.11 2002/11/26 23:41:58 mvojkovi Exp $ */

#include "nv_include.h"

#include "cursorstr.h"

/****************************************************************************\
*                                                                            *
*                          HW Cursor Entrypoints                             *
*                                                                            *
\****************************************************************************/

#define TRANSPARENT_PIXEL   0

#define ConvertToRGB555(c) \
(((c & 0xf80000) >> 9 ) | ((c & 0xf800) >> 6 ) | ((c & 0xf8) >> 3 ) | 0x8000)

#define ConvertToRGB888(c) (c | 0xff000000)

#define BYTE_SWAP_32(c)  ((c & 0xff000000) >> 24) |  \
                         ((c & 0xff0000) >> 8) |     \
                         ((c & 0xff00) << 8) |       \
                         ((c & 0xff) << 24)


static void 
ConvertCursor1555(NVPtr pNv, CARD32 *src, CARD16 *dst)
{
    CARD32 b, m;
    int i, j;
    
    for ( i = 0; i < 32; i++ ) {
        b = *src++;
        m = *src++;
        for ( j = 0; j < 32; j++ ) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
            if ( m & 0x80000000)
                *dst = ( b & 0x80000000) ? pNv->curFg : pNv->curBg;
            else
                *dst = TRANSPARENT_PIXEL;
            b <<= 1;
            m <<= 1;
#else
            if ( m & 1 )
                *dst = ( b & 1) ? pNv->curFg : pNv->curBg;
            else
                *dst = TRANSPARENT_PIXEL;
            b >>= 1;
            m >>= 1;
#endif
            dst++;
        }
    }
}


static void
ConvertCursor8888(NVPtr pNv, CARD32 *src, CARD32 *dst)
{
    CARD32 b, m;
    int i, j;
   
    for ( i = 0; i < 128; i++ ) {
        b = *src++;
        m = *src++;
        for ( j = 0; j < 32; j++ ) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
            if ( m & 0x80000000)
                *dst = ( b & 0x80000000) ? pNv->curFg : pNv->curBg;
            else
                *dst = TRANSPARENT_PIXEL;
            b <<= 1;
            m <<= 1;
#else
            if ( m & 1 )
                *dst = ( b & 1) ? pNv->curFg : pNv->curBg;
            else
                *dst = TRANSPARENT_PIXEL;
            b >>= 1;
            m >>= 1;
#endif
            dst++;
        }
    }
}


static void
TransformCursor (NVPtr pNv)
{
    CARD32 *tmp;
    int i, dwords;

    /* convert to color cursor */
    if(pNv->alphaCursor) {
       dwords = 64 * 64;
       if(!(tmp = ALLOCATE_LOCAL(dwords * 4))) return;
       ConvertCursor8888(pNv, pNv->curImage, tmp);
    } else {
       dwords = (32 * 32) >> 1;
       if(!(tmp = ALLOCATE_LOCAL(dwords * 4))) return;
       ConvertCursor1555(pNv, pNv->curImage, (CARD16*)tmp);
    }

    for(i = 0; i < dwords; i++)
        pNv->riva.CURSOR[i] = tmp[i];

    DEALLOCATE_LOCAL(tmp);
}

static void
NVLoadCursorImage( ScrnInfoPtr pScrn, unsigned char *src )
{
    NVPtr pNv = NVPTR(pScrn);

    /* save copy of image for color changes */
    memcpy(pNv->curImage, src, (pNv->alphaCursor) ? 1024 : 256);

    TransformCursor(pNv);
}

static void
NVSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    NVPtr pNv = NVPTR(pScrn);

    pNv->riva.PRAMDAC[0x0000300/4] = (x & 0xFFFF) | (y << 16);
}

static void
NVSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    NVPtr pNv = NVPTR(pScrn);
    CARD32 fore, back;

    if(pNv->alphaCursor) {
        fore = ConvertToRGB888(fg);
        back = ConvertToRGB888(bg);
#if X_BYTE_ORDER == X_BIG_ENDIAN
        if((pNv->Chipset & 0x0ff0) == 0x0110) {
           fore = BYTE_SWAP_32(fore);
           back = BYTE_SWAP_32(back);
        }
#endif
    } else {
        fore = ConvertToRGB555(fg);
        back = ConvertToRGB555(bg);
#if X_BYTE_ORDER == X_BIG_ENDIAN
        if((pNv->Chipset & 0x0ff0) == 0x0110) {
           fore = ((fore & 0xff) << 8) | (fore >> 8);
           back = ((back & 0xff) << 8) | (back >> 8);
        }
#endif
   }

    if ((pNv->curFg != fore) || (pNv->curBg != back)) {
        pNv->curFg = fore;
        pNv->curBg = back;
            
        TransformCursor(pNv);
    }
}


static void 
NVShowCursor(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    /* Enable cursor - X-Windows mode */
    pNv->riva.ShowHideCursor(&pNv->riva, 1);
}

static void
NVHideCursor(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    /* Disable cursor */
    pNv->riva.ShowHideCursor(&pNv->riva, 0);
}

static Bool 
NVUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    return TRUE;
}

#ifdef ARGB_CURSOR
static Bool 
NVUseHWCursorARGB(ScreenPtr pScreen, CursorPtr pCurs)
{
    if((pCurs->bits->width <= 64) && (pCurs->bits->height <= 64))
        return TRUE;

    return FALSE;
}

static void
NVLoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    NVPtr pNv = NVPTR(pScrn);
    CARD32 *image = pCurs->bits->argb;
    CARD32 *dst = (CARD32*)pNv->riva.CURSOR;
    int x, y, w, h;

    w = pCurs->bits->width;
    h = pCurs->bits->height;

#if X_BYTE_ORDER == X_BIG_ENDIAN
    if((pNv->Chipset & 0x0ff0) == 0x0110) {
       CARD32 tmp;

       for(y = 0; y < h; y++) {
          for(x = 0; x < w; x++) {
              tmp = *image++;
              *dst++ = BYTE_SWAP_32(tmp);
          }
          for(; x < 64; x++)
              *dst++ = 0;
       }
    } else 
#endif
    {
       for(y = 0; y < h; y++) {
          for(x = 0; x < w; x++) 
              *dst++ = *image++;
          for(; x < 64; x++)
              *dst++ = 0;
       }
    }

    if(y < 64)
      memset(dst, 0, 64 * (64 - y) * 4);
}
#endif

Bool 
NVCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);
    xf86CursorInfoPtr infoPtr;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NVCursorInit\n"));

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pNv->CursorInfoRec = infoPtr;

    if(pNv->alphaCursor)
       infoPtr->MaxWidth = infoPtr->MaxHeight = 64;
    else
       infoPtr->MaxWidth = infoPtr->MaxHeight = 32;

    infoPtr->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                     HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32; 
    infoPtr->SetCursorColors = NVSetCursorColors;
    infoPtr->SetCursorPosition = NVSetCursorPosition;
    infoPtr->LoadCursorImage = NVLoadCursorImage;
    infoPtr->HideCursor = NVHideCursor;
    infoPtr->ShowCursor = NVShowCursor;
    infoPtr->UseHWCursor = NVUseHWCursor;

#ifdef ARGB_CURSOR
    if(pNv->alphaCursor &&
       (((pNv->Chipset & 0x0ff0) != 0x0110) ||
        !(pNv->riva.flatPanel & FP_DITHER)))
    {
       infoPtr->UseHWCursorARGB = NVUseHWCursorARGB;
       infoPtr->LoadCursorARGB = NVLoadCursorARGB;
    }
#endif

    return(xf86InitCursor(pScreen, infoPtr));
}
