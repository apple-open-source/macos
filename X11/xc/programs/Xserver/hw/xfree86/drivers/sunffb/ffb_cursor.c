/*
 * Hardware cursor support for Creator, Creator 3D and Elite 3D
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_cursor.c,v 1.2 2000/05/23 04:47:44 dawes Exp $ */

#include "ffb.h"

/* This file just performs cursor software state management.  The
 * actual programming is done by calls into the DAC layer.
 */

static void FFBLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
static void FFBShowCursor(ScrnInfoPtr pScrn);
static void FFBHideCursor(ScrnInfoPtr pScrn);
static void FFBSetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void FFBSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);

static void
FFBLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    FFBPtr pFfb = GET_FFB_FROM_SCRN(pScrn);
    unsigned int *data = (unsigned int *)src;
    int x, y;

    pFfb->CursorData = src;
    x = pFfb->CursorShiftX;
    y = pFfb->CursorShiftY;
    if (x >= 64 || y >= 64)
	y = 64;
    FFBDacCursorLoadBitmap(pFfb, x, y, data);
}

static void 
FFBShowCursor(ScrnInfoPtr pScrn)
{
    FFBPtr pFfb = GET_FFB_FROM_SCRN(pScrn);

    FFBDacCursorEnableDisable(pFfb, 1);
}

static void
FFBHideCursor(ScrnInfoPtr pScrn)
{
    FFBPtr pFfb = GET_FFB_FROM_SCRN(pScrn);

    FFBDacCursorEnableDisable(pFfb, 0);
    pFfb->CursorData = NULL;
}

static void
FFBSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    FFBPtr pFfb = GET_FFB_FROM_SCRN(pScrn);
    int CursorShiftX = 0, CursorShiftY = 0;

    if (x < 0) {
	CursorShiftX = -x;
	x = 0;
	if (CursorShiftX > 64)
	    CursorShiftX = 64;
    }
    if (y < 0) {
	CursorShiftY = -y;
	y = 0;
	if (CursorShiftY > 64)
	    CursorShiftY = 64;
    }
    if ((CursorShiftX != pFfb->CursorShiftX ||
	 CursorShiftY != pFfb->CursorShiftY) &&
	pFfb->CursorData != NULL) {
	pFfb->CursorShiftX = CursorShiftX;
	pFfb->CursorShiftY = CursorShiftY;
	FFBLoadCursorImage(pScrn, pFfb->CursorData);
    }
	
    FFBDacLoadCursorPos(pFfb, x, y);
}

static void
FFBSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    FFBPtr pFfb = GET_FFB_FROM_SCRN(pScrn);

    FFBDacLoadCursorColor(pFfb, fg, bg);
}

Bool 
FFBHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    FFBPtr pFfb;
    xf86CursorInfoPtr infoPtr;

    pFfb = GET_FFB_FROM_SCRN(pScrn);
    pFfb->CursorShiftX = 0;
    pFfb->CursorShiftY = 0;
    pFfb->CursorData = NULL;

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pFfb->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = 64;
    infoPtr->MaxHeight = 64;
    infoPtr->Flags =  HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
	HARDWARE_CURSOR_SOURCE_MASK_NOT_INTERLEAVED;

    infoPtr->SetCursorColors = FFBSetCursorColors;
    infoPtr->SetCursorPosition = FFBSetCursorPosition;
    infoPtr->LoadCursorImage = FFBLoadCursorImage;
    infoPtr->HideCursor = FFBHideCursor;
    infoPtr->ShowCursor = FFBShowCursor;
    infoPtr->UseHWCursor = NULL;

    return xf86InitCursor(pScreen, infoPtr);
}
