
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i740/i740_cursor.c,v 1.5 2002/10/21 13:32:58 alanh Exp $ */

/*
 * Authors:
 *   Daryll Strauss <daryll@precisioninsight.com>
 *
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"

#include "xf86fbman.h"

#include "vgaHW.h"
#include "xf86xv.h"
#include "i740.h"

static void I740LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
static void I740ShowCursor(ScrnInfoPtr pScrn);
static void I740HideCursor(ScrnInfoPtr pScrn);
static void I740SetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void I740SetCursorColors(ScrnInfoPtr pScrn, int bg, int fb);
static Bool I740UseHWCursor(ScreenPtr pScrn, CursorPtr pCurs);

Bool
I740CursorInit(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn;
  I740Ptr pI740;
  xf86CursorInfoPtr infoPtr;
  FBAreaPtr fbarea;

  pScrn = xf86Screens[pScreen->myNum];
  pI740 = I740PTR(pScrn);
  pI740->CursorInfoRec = infoPtr = xf86CreateCursorInfoRec();
  if (!infoPtr) return FALSE;

  infoPtr->MaxWidth = 64;
  infoPtr->MaxHeight = 64;
  infoPtr->Flags =  HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
    HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
    HARDWARE_CURSOR_INVERT_MASK |
    HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
    HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
    HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64;

  infoPtr->SetCursorColors = I740SetCursorColors;
  infoPtr->SetCursorPosition = I740SetCursorPosition;
  infoPtr->LoadCursorImage = I740LoadCursorImage;
  infoPtr->HideCursor = I740HideCursor;
  infoPtr->ShowCursor = I740ShowCursor;
  infoPtr->UseHWCursor = I740UseHWCursor;

/*
 * Allocate a region the full width and tall enough
 * that at least 6K of video memory is consumed.
 * Then use a 1 kilobyte piece that is 4K byte aligned
 * within that region. KAO.
 */
  fbarea = xf86AllocateOffscreenArea(pScreen, 
				pScrn->displayWidth,
				((6*1024)/(pScrn->displayWidth*pI740->cpp))+1,
				0,0,0,0);
  if (fbarea == NULL) {
    pI740->CursorStart=0;
    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
	       "Hardware cursor disabled due to failure allocating offscreen memory.\n");
  }
  else {
    pI740->CursorStart = ((((fbarea->box.x1 + pScrn->displayWidth * fbarea->box.y1) * pI740->cpp)+4096)&0xfff000);
  }
  /*
   * Perhaps move the cursor to the beginning of the frame buffer
   * so that it never fails?
   */
  if (pI740->CursorStart>4*1024*1024) {
    pI740->CursorStart=0;
    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
	       "Disabling hardware cursor due to large framebuffer\n");
  }
  return xf86InitCursor(pScreen, infoPtr);
}

static Bool
I740UseHWCursor(ScreenPtr pScreen, CursorPtr pCurs) {
  ScrnInfoPtr pScrn;
  I740Ptr pI740;

  pScrn = xf86Screens[pScreen->myNum];
  pI740 = I740PTR(pScrn);
  if (pScrn->currentMode->Flags&V_DBLSCAN)
    return FALSE;
  if (!pI740->CursorStart) return FALSE;
  return TRUE;
}

static void
I740LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src) {
  I740Ptr pI740;
  int x, y;
  CARD8 *pcurs;

  pI740 = I740PTR(pScrn);
  pcurs = (CARD8 *)(pI740->FbBase + pI740->CursorStart);
  for (y = 0; y < 64; y++) {
    for (x = 0; x < 64 / 4; x++) {
      *pcurs++ = *src++;
    }
  }
}

static void
I740SetCursorPosition(ScrnInfoPtr pScrn, int x, int y) {
  I740Ptr pI740;
  int flag;

  pI740 = I740PTR(pScrn);
  if (x >= 0) flag = CURSOR_X_POS;
  else {
    flag = CURSOR_X_NEG;
    x=-x;
  }
  pI740->writeControl(pI740, XRX, CURSOR_X_LO, x&0xFF);
  pI740->writeControl(pI740, XRX, CURSOR_X_HI, (((x >> 8) & 0x07) | flag));

  if (y >= 0) flag = CURSOR_Y_POS;
  else {
    flag = CURSOR_Y_NEG;
    y=-y;
  }
  pI740->writeControl(pI740, XRX, CURSOR_Y_LO, y&0xFF);
  pI740->writeControl(pI740, XRX, CURSOR_Y_HI, (((y >> 8) & 0x07) | flag));
}

static void
I740ShowCursor(ScrnInfoPtr pScrn) {
  I740Ptr pI740;
  unsigned char tmp;

  pI740 = I740PTR(pScrn);
  pI740->writeControl(pI740, XRX, CURSOR_BASEADDR_LO, 
		      (pI740->CursorStart & 0x0000F000) >> 8);
  pI740->writeControl(pI740, XRX, CURSOR_BASEADDR_HI,
		      (pI740->CursorStart & 0x003F0000) >> 16);
  pI740->writeControl(pI740, XRX, CURSOR_CONTROL,
		      CURSOR_ORIGIN_DISPLAY | CURSOR_MODE_64_3C);

  tmp=pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_0);
  tmp |= HW_CURSOR_ENABLE;
  pI740->writeControl(pI740, XRX, PIXPIPE_CONFIG_0, tmp);
}

static void
I740HideCursor(ScrnInfoPtr pScrn) {
  unsigned char tmp;
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);
  tmp=pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_0);
  tmp &= ~HW_CURSOR_ENABLE;
  pI740->writeControl(pI740, XRX, PIXPIPE_CONFIG_0, tmp);
}

static void
I740SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg) {
  int tmp;
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);
  tmp=pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_0);
  tmp |= EXTENDED_PALETTE;
  pI740->writeControl(pI740, XRX, PIXPIPE_CONFIG_0, tmp);

  pI740->writeStandard(pI740, DACMASK, 0xFF);
  pI740->writeStandard(pI740, DACWX, 0x04);

  pI740->writeStandard(pI740, DACDATA, (bg & 0x00FF0000) >> 16);
  pI740->writeStandard(pI740, DACDATA, (bg & 0x0000FF00) >> 8);
  pI740->writeStandard(pI740, DACDATA, (bg & 0x000000FF));

  pI740->writeStandard(pI740, DACDATA, (fg & 0x00FF0000) >> 16);
  pI740->writeStandard(pI740, DACDATA, (fg & 0x0000FF00) >> 8);
  pI740->writeStandard(pI740, DACDATA, (fg & 0x000000FF));

  tmp=pI740->readControl(pI740, XRX, PIXPIPE_CONFIG_0);
  tmp &= ~EXTENDED_PALETTE;
  pI740->writeControl(pI740, XRX, PIXPIPE_CONFIG_0, tmp);
}



