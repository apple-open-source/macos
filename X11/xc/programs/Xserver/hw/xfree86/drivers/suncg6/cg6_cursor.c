/*
 * Hardware cursor support for GX or Turbo GX
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/suncg6/cg6_cursor.c,v 1.1 2000/06/30 19:30:46 dawes Exp $ */

#include "cg6.h"

static void CG6LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
static void CG6ShowCursor(ScrnInfoPtr pScrn);
static void CG6HideCursor(ScrnInfoPtr pScrn);
static void CG6SetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void CG6SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);

static void
CG6LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    Cg6Ptr pCg6 = GET_CG6_FROM_SCRN(pScrn);
    int i;
    unsigned int *data = (unsigned int *)src;

    for (i = 0; i < 32; i++)
	pCg6->thc->thc_cursmask[i] = *data++;
    for (i = 0; i < 32; i++)
	pCg6->thc->thc_cursbits[i] = *data++;
}

static void 
CG6ShowCursor(ScrnInfoPtr pScrn)
{
    Cg6Ptr pCg6 = GET_CG6_FROM_SCRN(pScrn);

    pCg6->thc->thc_cursxy = pCg6->CursorXY;
    pCg6->CursorEnabled = TRUE;
}

static void
CG6HideCursor(ScrnInfoPtr pScrn)
{
    Cg6Ptr pCg6 = GET_CG6_FROM_SCRN(pScrn);

    pCg6->thc->thc_cursxy = ((65536 - 32) << 16) | (65536 - 32);
    pCg6->CursorEnabled = FALSE;
}

static void
CG6SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    Cg6Ptr pCg6 = GET_CG6_FROM_SCRN(pScrn);

    pCg6->CursorXY = ((x & 0xffff) << 16) | (y & 0xffff);
    if (pCg6->CursorEnabled)
	pCg6->thc->thc_cursxy = pCg6->CursorXY;
}

static void
CG6SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    Cg6Ptr pCg6 = GET_CG6_FROM_SCRN(pScrn);

    if (bg != pCg6->CursorBg || fg != pCg6->CursorFg) {
	xf86SbusSetOsHwCursorCmap(pCg6->psdp, bg, fg);
	pCg6->CursorBg = bg;
	pCg6->CursorFg = fg;
    }
}

Bool 
CG6HWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    Cg6Ptr pCg6;
    xf86CursorInfoPtr infoPtr;

    pCg6 = GET_CG6_FROM_SCRN(pScrn);
    pCg6->CursorXY = 0;
    pCg6->CursorBg = pCg6->CursorFg = 0;
    pCg6->CursorEnabled = FALSE;

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pCg6->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = 32;
    infoPtr->MaxHeight = 32;
    infoPtr->Flags = HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
	HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
	HARDWARE_CURSOR_SOURCE_MASK_NOT_INTERLEAVED |
	HARDWARE_CURSOR_TRUECOLOR_AT_8BPP;

    infoPtr->SetCursorColors = CG6SetCursorColors;
    infoPtr->SetCursorPosition = CG6SetCursorPosition;
    infoPtr->LoadCursorImage = CG6LoadCursorImage;
    infoPtr->HideCursor = CG6HideCursor;
    infoPtr->ShowCursor = CG6ShowCursor;
    infoPtr->UseHWCursor = NULL;

    return xf86InitCursor(pScreen, infoPtr);
}
