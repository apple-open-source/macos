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

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/riva_cursor.c,v 1.1 2003/07/31 20:24:29 mvojkovi Exp $ */

#include "riva_include.h"

#include "cursorstr.h"

/****************************************************************************\
*                                                                            *
*                          HW Cursor Entrypoints                             *
*                                                                            *
\****************************************************************************/

#define TRANSPARENT_PIXEL   0

#define ConvertToRGB555(c) \
(((c & 0xf80000) >> 9 ) | ((c & 0xf800) >> 6 ) | ((c & 0xf8) >> 3 ) | 0x8000)


static void 
RivaConvertCursor1555(RivaPtr pRiva, CARD32 *src, CARD16 *dst)
{
    CARD32 b, m;
    int i, j;
    
    for ( i = 0; i < 32; i++ ) {
        b = *src++;
        m = *src++;
        for ( j = 0; j < 32; j++ ) {
            if ( m & 1 )
                *dst = ( b & 1) ? pRiva->curFg : pRiva->curBg;
            else
                *dst = TRANSPARENT_PIXEL;
            b >>= 1;
            m >>= 1;
            dst++;
        }
    }
}


static void
RivaTransformCursor (RivaPtr pRiva)
{
    CARD32 *tmp;
    int i, dwords;

    dwords = (32 * 32) >> 1;
    if(!(tmp = ALLOCATE_LOCAL(dwords * 4))) return;
    RivaConvertCursor1555(pRiva, pRiva->curImage, (CARD16*)tmp);

    for(i = 0; i < dwords; i++)
        pRiva->riva.CURSOR[i] = tmp[i];

    DEALLOCATE_LOCAL(tmp);
}

static void
RivaLoadCursorImage( ScrnInfoPtr pScrn, unsigned char *src )
{
    RivaPtr pRiva = RivaPTR(pScrn);

    /* save copy of image for color changes */
    memcpy(pRiva->curImage, src, 256);

    RivaTransformCursor(pRiva);
}

static void
RivaSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    RivaPtr pRiva = RivaPTR(pScrn);

    pRiva->riva.PRAMDAC[0x0000300/4] = (x & 0xFFFF) | (y << 16);
}

static void
RivaSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    CARD32 fore, back;

    fore = ConvertToRGB555(fg);
    back = ConvertToRGB555(bg);

    if ((pRiva->curFg != fore) || (pRiva->curBg != back)) {
        pRiva->curFg = fore;
        pRiva->curBg = back;
            
        RivaTransformCursor(pRiva);
    }
}


static void 
RivaShowCursor(ScrnInfoPtr pScrn)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    /* Enable cursor - X-Windows mode */
    pRiva->riva.ShowHideCursor(&pRiva->riva, 1);
}

static void
RivaHideCursor(ScrnInfoPtr pScrn)
{
    RivaPtr pRiva = RivaPTR(pScrn);
    /* Disable cursor */
    pRiva->riva.ShowHideCursor(&pRiva->riva, 0);
}

static Bool 
RivaUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    return TRUE;
}


Bool 
RivaCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    RivaPtr pRiva = RivaPTR(pScrn);
    xf86CursorInfoPtr infoPtr;

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pRiva->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = infoPtr->MaxHeight = 32;
    infoPtr->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                     HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32; 
    infoPtr->SetCursorColors = RivaSetCursorColors;
    infoPtr->SetCursorPosition = RivaSetCursorPosition;
    infoPtr->LoadCursorImage = RivaLoadCursorImage;
    infoPtr->HideCursor = RivaHideCursor;
    infoPtr->ShowCursor = RivaShowCursor;
    infoPtr->UseHWCursor = RivaUseHWCursor;

    return(xf86InitCursor(pScreen, infoPtr));
}
