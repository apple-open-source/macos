/*
 * Copyright 1997-2003 by Alan Hourihane, North Wales, UK.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident_dga.c,v 1.5 2003/09/05 22:07:28 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xaalocal.h"
#include "trident.h"
#include "trident_regs.h"
#include "dgaproc.h"


static Bool TRIDENT_OpenFramebuffer(ScrnInfoPtr, char **, unsigned char **, 
					int *, int *, int *);
static Bool TRIDENT_SetMode(ScrnInfoPtr, DGAModePtr);
static void TRIDENT_Sync(ScrnInfoPtr);
static int  TRIDENT_GetViewport(ScrnInfoPtr);
static void TRIDENT_SetViewport(ScrnInfoPtr, int, int, int);
static void TRIDENT_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void TRIDENT_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);
#if 0
static void TRIDENT_BlitTransRect(ScrnInfoPtr, int, int, int, int, int, int, 
					unsigned long);
#endif

static
DGAFunctionRec TRIDENTDGAFuncs = {
   TRIDENT_OpenFramebuffer,
   NULL,
   TRIDENT_SetMode,
   TRIDENT_SetViewport,
   TRIDENT_GetViewport,
   TRIDENT_Sync,
   TRIDENT_FillRect,
   TRIDENT_BlitRect,
#if 0
   TRIDENT_BlitTransRect
#else
   NULL
#endif
};

Bool
TRIDENTDGAInit(ScreenPtr pScreen)
{   
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
   DGAModePtr modes = NULL, newmodes = NULL, currentMode;
   DisplayModePtr pMode, firstMode;
   int Bpp = pScrn->bitsPerPixel >> 3;
   int num = 0;
   Bool oneMore;

   pMode = firstMode = pScrn->modes;

   while(pMode) {

	if(0 /*pScrn->displayWidth != pMode->HDisplay*/) {
	    newmodes = xrealloc(modes, (num + 2) * sizeof(DGAModeRec));
	    oneMore = TRUE;
	} else {
	    newmodes = xrealloc(modes, (num + 1) * sizeof(DGAModeRec));
	    oneMore = FALSE;
	}

	if(!newmodes) {
	   xfree(modes);
	   return FALSE;
	}
	modes = newmodes;

SECOND_PASS:

	currentMode = modes + num;
	num++;

	currentMode->mode = pMode;
	currentMode->flags = DGA_CONCURRENT_ACCESS | DGA_PIXMAP_AVAILABLE;
	if(!pTrident->NoAccel)
	   currentMode->flags |= DGA_FILL_RECT | DGA_BLIT_RECT;
	if(pMode->Flags & V_DBLSCAN)
	   currentMode->flags |= DGA_DOUBLESCAN;
	if(pMode->Flags & V_INTERLACE)
	   currentMode->flags |= DGA_INTERLACED;
	currentMode->byteOrder = pScrn->imageByteOrder;
	currentMode->depth = pScrn->depth;
	currentMode->bitsPerPixel = pScrn->bitsPerPixel;
	currentMode->red_mask = pScrn->mask.red;
	currentMode->green_mask = pScrn->mask.green;
	currentMode->blue_mask = pScrn->mask.blue;
	currentMode->visualClass = (Bpp == 1) ? PseudoColor : TrueColor;
	currentMode->viewportWidth = pMode->HDisplay;
	currentMode->viewportHeight = pMode->VDisplay;
	currentMode->xViewportStep = 1;
	currentMode->yViewportStep = 1;
	currentMode->viewportFlags = DGA_FLIP_RETRACE;
	currentMode->offset = 0;
	currentMode->address = pTrident->FbBase;

	if(oneMore) { /* first one is narrow width */
	    currentMode->bytesPerScanline = ((pMode->HDisplay * Bpp) + 3) & ~3L;
	    currentMode->imageWidth = pMode->HDisplay;
	    currentMode->imageHeight =  pMode->VDisplay;
	    currentMode->pixmapWidth = currentMode->imageWidth;
	    currentMode->pixmapHeight = currentMode->imageHeight;
	    currentMode->maxViewportX = currentMode->imageWidth - 
					currentMode->viewportWidth;
	    /* this might need to get clamped to some maximum */
	    currentMode->maxViewportY = currentMode->imageHeight -
					currentMode->viewportHeight;
	    oneMore = FALSE;
	    goto SECOND_PASS;
	} else {
	    currentMode->bytesPerScanline = 
			((pScrn->displayWidth * Bpp) + 3) & ~3L;
	    currentMode->imageWidth = pScrn->displayWidth;
	    currentMode->imageHeight =  pMode->VDisplay;
	    currentMode->pixmapWidth = currentMode->imageWidth;
	    currentMode->pixmapHeight = currentMode->imageHeight;
	    currentMode->maxViewportX = currentMode->imageWidth - 
					currentMode->viewportWidth;
	    /* this might need to get clamped to some maximum */
	    currentMode->maxViewportY = currentMode->imageHeight -
					currentMode->viewportHeight;
	}	    

	pMode = pMode->next;
	if(pMode == firstMode)
	   break;
   }

   pTrident->numDGAModes = num;
   pTrident->DGAModes = modes;

   return DGAInit(pScreen, &TRIDENTDGAFuncs, modes, num);  
}


static Bool
TRIDENT_SetMode(
   ScrnInfoPtr pScrn,
   DGAModePtr pMode
){
   static int OldDisplayWidth[MAXSCREENS];
   int index = pScrn->pScreen->myNum;
   TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

   if(!pMode) { /* restore the original mode */
	/* put the ScreenParameters back */
	
	pScrn->displayWidth = OldDisplayWidth[index];
	
        TRIDENTSwitchMode(index, pScrn->currentMode, 0);
	pTrident->DGAactive = FALSE;
   } else {
	if(!pTrident->DGAactive) {  /* save the old parameters */
	    OldDisplayWidth[index] = pScrn->displayWidth;

	    pTrident->DGAactive = TRUE;
	}

	pScrn->displayWidth = pMode->bytesPerScanline / 
			      (pMode->bitsPerPixel >> 3);

        TRIDENTSwitchMode(index, pMode->mode, 0);
   }
   
   return TRUE;
}



static int  
TRIDENT_GetViewport(
  ScrnInfoPtr pScrn
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    return pTrident->DGAViewportStatus;
}

static void 
TRIDENT_SetViewport(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int flags
){
   TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

   TRIDENTAdjustFrame(pScrn->pScreen->myNum, x, y, flags);
   pTrident->DGAViewportStatus = 0;  /* TRIDENTAdjustFrame loops until finished */
}

static void 
TRIDENT_FillRect (
   ScrnInfoPtr pScrn, 
   int x, int y, int w, int h, 
   unsigned long color
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if(pTrident->AccelInfoRec) {
	(*pTrident->AccelInfoRec->SetupForSolidFill)(pScrn, color, GXcopy, ~0);
	(*pTrident->AccelInfoRec->SubsequentSolidFillRect)(pScrn, x, y, w, h);
	SET_SYNC_FLAG(pTrident->AccelInfoRec);
    }
}

static void 
TRIDENT_Sync(
   ScrnInfoPtr pScrn
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if(pTrident->AccelInfoRec) {
	(*pTrident->AccelInfoRec->Sync)(pScrn);
    }
}

static void 
TRIDENT_BlitRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if(pTrident->AccelInfoRec) {
	int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
	int ydir = (srcy < dsty) ? -1 : 1;

	(*pTrident->AccelInfoRec->SetupForScreenToScreenCopy)(
		pScrn, xdir, ydir, GXcopy, ~0, -1);
	(*pTrident->AccelInfoRec->SubsequentScreenToScreenCopy)(
		pScrn, srcx, srcy, dstx, dsty, w, h);
	SET_SYNC_FLAG(pTrident->AccelInfoRec);
    }
}

#if 0
static void 
TRIDENT_BlitTransRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty,
   unsigned long color
){
  /* this one should be separate since the XAA function would
     prohibit usage of ~0 as the key */
}
#endif

static Bool 
TRIDENT_OpenFramebuffer(
   ScrnInfoPtr pScrn, 
   char **name,
   unsigned char **mem,
   int *size,
   int *offset,
   int *flags
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    *name = NULL; 		/* no special device */
    *mem = (unsigned char*)pTrident->FbAddress;
    *size = pTrident->FbMapSize;
    *offset = 0;
    *flags = DGA_NEED_ROOT;

    return TRUE;
}
