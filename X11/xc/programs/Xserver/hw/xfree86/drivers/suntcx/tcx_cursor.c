/*
 * Hardware cursor support for TCX
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/suntcx/tcx_cursor.c,v 1.1 2000/06/30 17:15:16 dawes Exp $ */

#include "tcx.h"

static void TCXLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
static void TCXShowCursor(ScrnInfoPtr pScrn);
static void TCXHideCursor(ScrnInfoPtr pScrn);
static void TCXSetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void TCXSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);

static void
TCXLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    TcxPtr pTcx = GET_TCX_FROM_SCRN(pScrn);
    int i, x, y;
    unsigned int *data = (unsigned int *)src;
        
    pTcx->CursorData = src;
    x = pTcx->CursorShiftX;
    y = pTcx->CursorShiftY;
    if (x >= 32 || y >= 32)
	y = 32;
    data += y;
    for (i = 0; i < 32 - y; i++)
	pTcx->thc->thc_cursmask[i] = *data++ << x;
    for (; i < 32; i++)
	pTcx->thc->thc_cursmask[i] = 0;
    data += y;
    for (i = 0; i < 32 - y; i++)
	pTcx->thc->thc_cursbits[i] = *data++ << x;
    for (; i < 32; i++)
	pTcx->thc->thc_cursbits[i] = 0;
}

static void 
TCXShowCursor(ScrnInfoPtr pScrn)
{
    TcxPtr pTcx = GET_TCX_FROM_SCRN(pScrn);

    pTcx->thc->thc_cursxy = pTcx->CursorXY;
    pTcx->CursorEnabled = TRUE;
}

static void
TCXHideCursor(ScrnInfoPtr pScrn)
{
    TcxPtr pTcx = GET_TCX_FROM_SCRN(pScrn);

    pTcx->thc->thc_cursxy = ((65536 - 32) << 16) | (65536 - 32);
    pTcx->CursorEnabled = FALSE;
}

static void
TCXSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    TcxPtr pTcx = GET_TCX_FROM_SCRN(pScrn);
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
    if ((CursorShiftX != pTcx->CursorShiftX ||
	 CursorShiftY != pTcx->CursorShiftY) &&
	 pTcx->CursorData != NULL) {
	pTcx->CursorShiftX = CursorShiftX;
	pTcx->CursorShiftY = CursorShiftY;
	TCXLoadCursorImage(pScrn, pTcx->CursorData);
    }

    pTcx->CursorXY = ((x & 0xffff) << 16) | (y & 0xffff);
    if (pTcx->CursorEnabled)
	pTcx->thc->thc_cursxy = pTcx->CursorXY;
}

static void
TCXSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    TcxPtr pTcx = GET_TCX_FROM_SCRN(pScrn);

    if (bg != pTcx->CursorBg || fg != pTcx->CursorFg) {
	xf86SbusSetOsHwCursorCmap(pTcx->psdp, bg, fg);
	pTcx->CursorBg = bg;
	pTcx->CursorFg = fg;
    }
}

Bool 
TCXHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TcxPtr pTcx;
    xf86CursorInfoPtr infoPtr;

    pTcx = GET_TCX_FROM_SCRN(pScrn);
    pTcx->CursorXY = 0;
    pTcx->CursorBg = pTcx->CursorFg = 0;
    pTcx->CursorEnabled = FALSE;
    pTcx->CursorShiftX = 0;
    pTcx->CursorShiftY = 0;
    pTcx->CursorData = NULL;

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pTcx->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = 32;
    infoPtr->MaxHeight = 32;
    infoPtr->Flags = HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
	HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
	HARDWARE_CURSOR_SOURCE_MASK_NOT_INTERLEAVED |
	HARDWARE_CURSOR_TRUECOLOR_AT_8BPP;

    infoPtr->SetCursorColors = TCXSetCursorColors;
    infoPtr->SetCursorPosition = TCXSetCursorPosition;
    infoPtr->LoadCursorImage = TCXLoadCursorImage;
    infoPtr->HideCursor = TCXHideCursor;
    infoPtr->ShowCursor = TCXShowCursor;
    infoPtr->UseHWCursor = NULL;

    return xf86InitCursor(pScreen, infoPtr);
}
