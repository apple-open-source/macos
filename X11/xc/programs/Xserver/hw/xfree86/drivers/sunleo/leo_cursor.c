/*
 * Hardware cursor support for Leo (ZX)
 *
 * Copyright 2000 by Jakub Jelinek <jakub@redhat.com>.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Jakub
 * Jelinek not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Jakub Jelinek makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * JAKUB JELINEK DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL JAKUB JELINEK BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_cursor.c,v 1.1 2000/05/18 23:21:39 dawes Exp $ */

#include "leo.h"

static void LeoLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
static void LeoShowCursor(ScrnInfoPtr pScrn);
static void LeoHideCursor(ScrnInfoPtr pScrn);
static void LeoSetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void LeoSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);

static void
LeoLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    LeoPtr pLeo = GET_LEO_FROM_SCRN(pScrn);
    int i, j, x, y;
    unsigned int *data = (unsigned int *)src, value;

    pLeo->CursorData = src;
    x = pLeo->CursorShiftX;
    y = pLeo->CursorShiftY;
    if (x >= 32 || y >= 32)
	y = 32;
    pLeo->dac->cur_type = 0;
    for (j = 0; j < 2; j++) {
	data += y;
	for (i = y; i < 32; i++, data++) {
	    value = (*data >> 16) | (*data << 16);
	    value = ((value & 0xff00ff00) >> 8) |
		    ((value & 0x00ff00ff) << 8);
	    pLeo->dac->cur_data = value >> x;
	}
	for (i = 0; i < y; i++)
	    pLeo->dac->cur_data = 0;
    }
}

static void 
LeoShowCursor(ScrnInfoPtr pScrn)
{
    LeoPtr pLeo = GET_LEO_FROM_SCRN(pScrn);

    pLeo->dac->cur_misc |= 0x80;
}

static void
LeoHideCursor(ScrnInfoPtr pScrn)
{
    LeoPtr pLeo = GET_LEO_FROM_SCRN(pScrn);

    pLeo->dac->cur_misc &= ~0x80;
    pLeo->CursorData = NULL;
}

static void
LeoSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    LeoPtr pLeo = GET_LEO_FROM_SCRN(pScrn);
    int CursorShiftX = 0, CursorShiftY = 0;

    if (x < 0) {
	CursorShiftX = -x;
	x = 0;
	if (CursorShiftX > 32)
	    CursorShiftX = 32;
    }
    if (y < 0) {
	CursorShiftY = -y;
	y = 0;
	if (CursorShiftY > 32)
	    CursorShiftY = 32;
    }
    pLeo->dac->cur_misc &= ~0x80;
    if ((CursorShiftX != pLeo->CursorShiftX ||
	 CursorShiftY != pLeo->CursorShiftY) &&
	pLeo->CursorData != NULL) {
	pLeo->CursorShiftX = CursorShiftX;
	pLeo->CursorShiftY = CursorShiftY;
	LeoLoadCursorImage(pScrn, pLeo->CursorData);
    }
	
    pLeo->dac->cur_cursxy = ((y & 0x7ff) << 11) | (x & 0x7ff);
    pLeo->dac->cur_misc |= 0x30;
    pLeo->dac->cur_misc |= 0x80;
}

static void
LeoSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    LeoPtr pLeo = GET_LEO_FROM_SCRN(pScrn);

    pLeo->dac->cur_type = 0x50;
    pLeo->dac->cur_data = bg;
    pLeo->dac->cur_data = fg;
    pLeo->dac->cur_misc |= 0x03;
}

Bool 
LeoHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    LeoPtr pLeo;
    xf86CursorInfoPtr infoPtr;

    pLeo = GET_LEO_FROM_SCRN(pScrn);
    pLeo->CursorShiftX = 0;
    pLeo->CursorShiftY = 0;
    pLeo->CursorData = NULL;

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pLeo->CursorInfoRec = infoPtr;
    pLeo->dac = (LeoCursor *)((char *)pLeo->fb + LEO_LX0_CURSOR_VOFF);

    infoPtr->MaxWidth = 32;
    infoPtr->MaxHeight = 32;
    infoPtr->Flags =  HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
	HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
	HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
	HARDWARE_CURSOR_SOURCE_MASK_NOT_INTERLEAVED;

    infoPtr->SetCursorColors = LeoSetCursorColors;
    infoPtr->SetCursorPosition = LeoSetCursorPosition;
    infoPtr->LoadCursorImage = LeoLoadCursorImage;
    infoPtr->HideCursor = LeoHideCursor;
    infoPtr->ShowCursor = LeoShowCursor;
    infoPtr->UseHWCursor = NULL;

    return xf86InitCursor(pScreen, infoPtr);
}
