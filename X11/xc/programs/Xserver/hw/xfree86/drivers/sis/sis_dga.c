/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_dga.c,v 1.5 2003/01/29 15:42:17 eich Exp $ */
/*
 * Copyright 2000 by Alan Hourihane, Sychdyn, North Wales, UK.
 * Copyright 2002 by Thomas Winischhofer, Vienna, Austria
 *
 * Portions from radeon_dga.c which is
 *          Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                         VA Linux Systems Inc., Fremont, California.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the providers not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The providers make no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE PROVIDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE PROVIDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *           Thomas Winischhofer <thomas@winischhofer.net>
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xaalocal.h"
#include "sis.h"
#include "sis_regs.h"
#include "dgaproc.h"

static Bool SIS_OpenFramebuffer(ScrnInfoPtr, char **, unsigned char **, 
                    int *, int *, int *);
static Bool SIS_SetMode(ScrnInfoPtr, DGAModePtr);
static void SIS_Sync(ScrnInfoPtr);
static int  SIS_GetViewport(ScrnInfoPtr);
static void SIS_SetViewport(ScrnInfoPtr, int, int, int);
static void SIS_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void SIS_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);
static void SIS_BlitTransRect(ScrnInfoPtr, int, int, int, int, int, int,
                    unsigned long);

static
DGAFunctionRec SISDGAFuncs = {
   SIS_OpenFramebuffer,
   NULL,
   SIS_SetMode,
   SIS_SetViewport,
   SIS_GetViewport,
   SIS_Sync,
   SIS_FillRect,
   SIS_BlitRect,
   NULL
};

static
DGAFunctionRec SISDGAFuncs3xx = {
   SIS_OpenFramebuffer,
   NULL,
   SIS_SetMode,
   SIS_SetViewport,
   SIS_GetViewport,
   SIS_Sync,
   SIS_FillRect,
   SIS_BlitRect,
   SIS_BlitTransRect
};

static DGAModePtr
SISSetupDGAMode(
   ScrnInfoPtr pScrn,
   DGAModePtr modes,
   int *num,
   int bitsPerPixel,
   int depth,
   Bool pixmap,
   int secondPitch,
   unsigned long red,
   unsigned long green,
   unsigned long blue,
   short visualClass
){
   SISPtr pSiS = SISPTR(pScrn);
   DGAModePtr newmodes = NULL, currentMode;
   DisplayModePtr pMode, firstMode;
   int otherPitch, Bpp = bitsPerPixel >> 3;
   Bool oneMore;

   pMode = firstMode = pScrn->modes;

   while (pMode) {

	otherPitch = secondPitch ? secondPitch : pMode->HDisplay;

	if(pMode->HDisplay != otherPitch) {

	    newmodes = xrealloc(modes, (*num + 2) * sizeof(DGAModeRec));
	    oneMore  = TRUE;

	} else {

	    newmodes = xrealloc(modes, (*num + 1) * sizeof(DGAModeRec));
	    oneMore  = FALSE;

	}

	if(!newmodes) {
	    xfree(modes);
	    return NULL;
	}
	modes = newmodes;

SECOND_PASS:

	currentMode = modes + *num;
	(*num)++;

	currentMode->mode           = pMode;
	currentMode->flags          = DGA_CONCURRENT_ACCESS;
	if(pixmap)
	    currentMode->flags     |= DGA_PIXMAP_AVAILABLE;
	if(!pSiS->NoAccel) {
	    currentMode->flags     |= DGA_FILL_RECT | DGA_BLIT_RECT;
	    if((pSiS->VGAEngine == SIS_300_VGA) ||
	       (pSiS->VGAEngine == SIS_315_VGA) ||
	       (pSiS->VGAEngine == SIS_530_VGA)) {
               currentMode->flags  |= DGA_BLIT_RECT_TRANS;
            }
	}
	if (pMode->Flags & V_DBLSCAN)
	    currentMode->flags     |= DGA_DOUBLESCAN;
	if (pMode->Flags & V_INTERLACE)
	    currentMode->flags     |= DGA_INTERLACED;
	currentMode->byteOrder      = pScrn->imageByteOrder;
	currentMode->depth          = depth;
	currentMode->bitsPerPixel   = bitsPerPixel;
	currentMode->red_mask       = red;
	currentMode->green_mask     = green;
	currentMode->blue_mask      = blue;
	currentMode->visualClass    = visualClass;
	currentMode->viewportWidth  = pMode->HDisplay;
	currentMode->viewportHeight = pMode->VDisplay;
	currentMode->xViewportStep  = 1;
	currentMode->yViewportStep  = 1;
	currentMode->viewportFlags  = DGA_FLIP_RETRACE;
	currentMode->offset         = 0;
	currentMode->address        = pSiS->FbBase;

	if(oneMore) {

	    /* first one is narrow width */
	    currentMode->bytesPerScanline = (((pMode->HDisplay * Bpp) + 3) & ~3L);
	    currentMode->imageWidth   = pMode->HDisplay;
	    currentMode->imageHeight  = pMode->VDisplay;
	    currentMode->pixmapWidth  = currentMode->imageWidth;
	    currentMode->pixmapHeight = currentMode->imageHeight;
	    currentMode->maxViewportX = currentMode->imageWidth -
					currentMode->viewportWidth;
	    /* this might need to get clamped to some maximum */
	    currentMode->maxViewportY = (currentMode->imageHeight -
					 currentMode->viewportHeight);
	    oneMore = FALSE;
	    goto SECOND_PASS;

	} else {

	    currentMode->bytesPerScanline = ((otherPitch * Bpp) + 3) & ~3L;
	    currentMode->imageWidth       = otherPitch;
	    currentMode->imageHeight      = pMode->VDisplay;
	    currentMode->pixmapWidth      = currentMode->imageWidth;
	    currentMode->pixmapHeight     = currentMode->imageHeight;
	    currentMode->maxViewportX     = (currentMode->imageWidth -
					     currentMode->viewportWidth);
            /* this might need to get clamped to some maximum */
	    currentMode->maxViewportY     = (currentMode->imageHeight -
					     currentMode->viewportHeight);
	}

	pMode = pMode->next;
	if (pMode == firstMode)
	    break;
    }

    return modes;
}


Bool
SISDGAInit(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   SISPtr pSiS = SISPTR(pScrn);
   DGAModePtr modes = NULL;
   int num = 0;

   /* 8 */
#ifdef SISDUALHEAD
   /* TW: We don't ever use 8bpp modes in dual head mode,
          so don't offer them to DGA either
    */
   if(!pSiS->DualHeadMode) {
#endif
      modes = SISSetupDGAMode(pScrn, modes, &num, 8, 8,
	 	              (pScrn->bitsPerPixel == 8),
			      ((pScrn->bitsPerPixel != 8)
			          ? 0 : pScrn->displayWidth),
			      0, 0, 0, PseudoColor);
#ifdef SISDUALHEAD
   }
#endif

   /* 16 */
   modes = SISSetupDGAMode(pScrn, modes, &num, 16, 16,
			   (pScrn->bitsPerPixel == 16),
			   ((pScrn->depth != 16)
				? 0 : pScrn->displayWidth),
			   0xf800, 0x07e0, 0x001f, TrueColor);

   if((pSiS->VGAEngine == SIS_530_VGA) || (pSiS->VGAEngine == SIS_OLD_VGA)) {
     /* 24 */
     modes = SISSetupDGAMode(pScrn, modes, &num, 24, 24,
	 		     (pScrn->bitsPerPixel == 24),
			     ((pScrn->bitsPerPixel != 24)
				? 0 : pScrn->displayWidth),
			      0xff0000, 0x00ff00, 0x0000ff, TrueColor);
   }

   if(pSiS->VGAEngine != SIS_OLD_VGA) {
     /* 32 */
     modes = SISSetupDGAMode(pScrn, modes, &num, 32, 24,
			   (pScrn->bitsPerPixel == 32),
			   ((pScrn->bitsPerPixel != 32)
				? 0 : pScrn->displayWidth),
			   0xff0000, 0x00ff00, 0x0000ff, TrueColor);
   }

   pSiS->numDGAModes = num;
   pSiS->DGAModes = modes;

   if((pSiS->VGAEngine == SIS_300_VGA) ||
      (pSiS->VGAEngine == SIS_315_VGA) ||
      (pSiS->VGAEngine == SIS_530_VGA)) {
     return DGAInit(pScreen, &SISDGAFuncs3xx, modes, num);
   } else {
     return DGAInit(pScreen, &SISDGAFuncs, modes, num);
   }
}


static Bool
SIS_SetMode(
   ScrnInfoPtr pScrn,
   DGAModePtr pMode
){
   static SISFBLayout BackupLayouts[MAXSCREENS];
   int index = pScrn->pScreen->myNum;
   SISPtr pSiS = SISPTR(pScrn);

    if(!pMode) { /* restore the original mode */

        if(pSiS->DGAactive) {
           /* put the ScreenParameters back */
	   memcpy(&pSiS->CurrentLayout, &BackupLayouts[index], sizeof(SISFBLayout));
        }

	pScrn->currentMode = pSiS->CurrentLayout.mode;

        (*pScrn->SwitchMode)(index, pScrn->currentMode, 0);
	(*pScrn->AdjustFrame)(index, pScrn->frameX0, pScrn->frameY0, 0);
        pSiS->DGAactive = FALSE;

    } else {	/* set new mode */

        if(!pSiS->DGAactive) {
	    /* save the old parameters */
            memcpy(&BackupLayouts[index], &pSiS->CurrentLayout, sizeof(SISFBLayout));
            pSiS->DGAactive = TRUE;
    	}

	pSiS->CurrentLayout.bitsPerPixel = pMode->bitsPerPixel;
	pSiS->CurrentLayout.depth        = pMode->depth;
	pSiS->CurrentLayout.displayWidth = pMode->bytesPerScanline / (pMode->bitsPerPixel >> 3);

    	(*pScrn->SwitchMode)(index, pMode->mode, 0);
	/* TW: Adjust viewport to 0/0 after mode switch */
	/* This should fix the vmware-in-dualhead problems */
	(*pScrn->AdjustFrame)(index, 0, 0, 0);
    }

    return TRUE;
}

static int
SIS_GetViewport(
  ScrnInfoPtr pScrn
){
    SISPtr pSiS = SISPTR(pScrn);

    return pSiS->DGAViewportStatus;
}

static void 
SIS_SetViewport(
   ScrnInfoPtr pScrn,
   int x, int y, 
   int flags
){
   SISPtr pSiS = SISPTR(pScrn);

   (*pScrn->AdjustFrame)(pScrn->pScreen->myNum, x, y, flags);
   pSiS->DGAViewportStatus = 0;  /* There are never pending Adjusts */
}

static void 
SIS_FillRect (
   ScrnInfoPtr pScrn, 
   int x, int y, int w, int h, 
   unsigned long color
){
    SISPtr pSiS = SISPTR(pScrn);

    if(pSiS->AccelInfoPtr) {
      (*pSiS->AccelInfoPtr->SetupForSolidFill)(pScrn, color, GXcopy, ~0);
      (*pSiS->AccelInfoPtr->SubsequentSolidFillRect)(pScrn, x, y, w, h);
      SET_SYNC_FLAG(pSiS->AccelInfoPtr);
    }
}

static void 
SIS_Sync(
   ScrnInfoPtr pScrn
){
    SISPtr pSiS = SISPTR(pScrn);

    if(pSiS->AccelInfoPtr) {
      (*pSiS->AccelInfoPtr->Sync)(pScrn);
    }
}

static void 
SIS_BlitRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty
){
    SISPtr pSiS = SISPTR(pScrn);

    if(pSiS->AccelInfoPtr) {
      int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
      int ydir = (srcy < dsty) ? -1 : 1;

      (*pSiS->AccelInfoPtr->SetupForScreenToScreenCopy)(
          pScrn, xdir, ydir, GXcopy, (CARD32)~0, -1);
      (*pSiS->AccelInfoPtr->SubsequentScreenToScreenCopy)(
          pScrn, srcx, srcy, dstx, dsty, w, h);
      SET_SYNC_FLAG(pSiS->AccelInfoPtr);
    }
}

static void
SIS_BlitTransRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty,
   unsigned long color
){
    SISPtr pSiS = SISPTR(pScrn);

    if(pSiS->AccelInfoPtr) {
       int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
       int ydir = (srcy < dsty) ? -1 : 1;

       (*pSiS->AccelInfoPtr->SetupForScreenToScreenCopy)(
          pScrn, xdir, ydir, GXcopy, ~0, color);
       (*pSiS->AccelInfoPtr->SubsequentScreenToScreenCopy)(
          pScrn, srcx, srcy, dstx, dsty, w, h);
       SET_SYNC_FLAG(pSiS->AccelInfoPtr);
    }
}

static Bool 
SIS_OpenFramebuffer(
   ScrnInfoPtr pScrn,
   char **name,
   unsigned char **mem,
   int *size,
   int *offset,
   int *flags
){
    SISPtr pSiS = SISPTR(pScrn);

    *name = NULL;       /* no special device */
    *mem = (unsigned char*)pSiS->FbAddress;
    *size = pSiS->maxxfbmem;
    *offset = 0;
    *flags = DGA_NEED_ROOT;

    return TRUE;
}
