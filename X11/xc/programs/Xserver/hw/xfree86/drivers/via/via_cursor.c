/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_cursor.c,v 1.3 2003/08/27 15:16:08 tsi Exp $ */
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

/*************************************************************************
 *
 *  File:       via_cursor.c
 *  Content:    Hardware cursor support for VIA/S3G UniChrom
 *
 ************************************************************************/

#include "via_driver.h"

static void VIALoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
static void VIASetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void VIASetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);


/*
 * Read/write to the DAC via MMIO
 */

#define inCRReg(reg)        (VGAHWPTR(pScrn))->readCrtc(VGAHWPTR(pScrn), reg)
#define outCRReg(reg, val)  (VGAHWPTR(pScrn))->writeCrtc(VGAHWPTR(pScrn), reg, val)
#define inStatus1()         (VGAHWPTR(pScrn))->readST01(VGAHWPTR(pScrn))


#define MAX_CURS 32

Bool
VIAHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VIAPtr pVia = VIAPTR(pScrn);
    xf86CursorInfoPtr infoPtr;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VIAHWCursorInit\n"));
    infoPtr = xf86CreateCursorInfoRec();
    if (!infoPtr)
        return FALSE;

    pVia->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = MAX_CURS;
    infoPtr->MaxHeight = MAX_CURS;
    infoPtr->Flags = HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32 |
                     HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
                     /*HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |*/
                     HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                     HARDWARE_CURSOR_INVERT_MASK |
                     HARDWARE_CURSOR_BIT_ORDER_MSBFIRST|
                     0;

    infoPtr->SetCursorColors = VIASetCursorColors;
    infoPtr->SetCursorPosition = VIASetCursorPosition;
    infoPtr->LoadCursorImage = VIALoadCursorImage;
    infoPtr->HideCursor = VIAHideCursor;
    infoPtr->ShowCursor = VIAShowCursor;
    infoPtr->UseHWCursor = NULL;

    if (!pVia->CursorStart) {
        pVia->CursorStart = pVia->FBFreeEnd - VIA_CURSOR_SIZE;
        pVia->FBFreeEnd -= VIA_CURSOR_SIZE;

        /* Set cursor location in frame buffer.  */
        VIASETREG(VIA_REG_CURSOR_MODE, pVia->CursorStart);
    }

    return xf86InitCursor(pScreen, infoPtr);
}



void
VIAShowCursor(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 dwCursorMode;

    dwCursorMode = VIAGETREG(VIA_REG_CURSOR_MODE);

    /* Turn on Hardware Cursor */
    VIASETREG(VIA_REG_CURSOR_MODE, dwCursorMode | 0x3);
}


void
VIAHideCursor(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 dwCursorMode;

    dwCursorMode = VIAGETREG(VIA_REG_CURSOR_MODE);

    /* Turn cursor off. */
    VIASETREG(VIA_REG_CURSOR_MODE, dwCursorMode & 0xFFFFFFFE);
}


static void
VIALoadCursorImage(ScrnInfoPtr pScrn, unsigned char* src)
{
    VIAPtr pVia = VIAPTR(pScrn);
    CARD32 dwCursorMode;

    WaitIdle();

    dwCursorMode = VIAGETREG(VIA_REG_CURSOR_MODE);

    /* Turn cursor off. */
    VIASETREG(VIA_REG_CURSOR_MODE, dwCursorMode & 0xFFFFFFFE);

    /* Upload the cursor image to the frame buffer. */
    memcpy(pVia->FBBase + pVia->CursorStart, src, MAX_CURS * MAX_CURS / 8 * 2);

    /* Restore cursor status */
    VIASETREG(VIA_REG_CURSOR_MODE, dwCursorMode);
}

static void
VIASetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    VIAPtr          pVia = VIAPTR(pScrn);
    VIABIOSInfoPtr  pBIOSInfo = pVia->pBIOSInfo;
    unsigned char   xoff, yoff;
    CARD32          dwCursorMode;

    if (x < 0) {
        xoff = ((-x) & 0xFE);
        x = 0;
    } else {
        xoff = 0;
    }

    if (y < 0) {
        yoff = ((-y) & 0xFE);
        y = 0;
    } else {
        yoff = 0;
        /* LCD Expand Mode Cursor Y Position Re-Calculated */
        if (pBIOSInfo->scaleY) {
            y = (int)(((pBIOSInfo->panelY * y) + (pBIOSInfo->resY >> 1)) / pBIOSInfo->resY);
        }
    }

    /* Hide cursor before set cursor position in order to avoid ghost cursor
     * image when directly set cursor position. It should be a HW bug but
     * we can use patch by SW. */
    dwCursorMode = VIAGETREG(VIA_REG_CURSOR_MODE);

    /* Turn cursor off. */
    VIASETREG(VIA_REG_CURSOR_MODE, dwCursorMode & 0xFFFFFFFE);

    VIASETREG(VIA_REG_CURSOR_ORG, ((xoff << 16) | (yoff & 0x003f)));
    VIASETREG(VIA_REG_CURSOR_POS, ((x << 16) | (y & 0x07ff)));

    /* Restore cursor status */
    VIASETREG(VIA_REG_CURSOR_MODE, dwCursorMode);
}


static void
VIASetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    VIAPtr pVia = VIAPTR(pScrn);

    VIASETREG(VIA_REG_CURSOR_FG, fg);
    VIASETREG(VIA_REG_CURSOR_BG, bg);

}
