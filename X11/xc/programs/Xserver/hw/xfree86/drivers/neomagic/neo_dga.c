/*
 * Copyright 2000 by Alan Hourihane, Sychdyn, North Wales, UK.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/neomagic/neo_dga.c,v 1.5 2002/04/04 14:05:44 eich Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xaalocal.h"
#include "neo.h"
#include "neo_reg.h"
#include "dgaproc.h"
#include "vgaHW.h"

static Bool NEO_OpenFramebuffer(ScrnInfoPtr, char **, unsigned char **, 
					int *, int *, int *);
static Bool NEO_SetMode(ScrnInfoPtr, DGAModePtr);
static void NEO_Sync(ScrnInfoPtr);
static int  NEO_GetViewport(ScrnInfoPtr);
static void NEO_SetViewport(ScrnInfoPtr, int, int, int);
static void NEO_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void NEO_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);
#if 0
static void NEO_BlitTransRect(ScrnInfoPtr, int, int, int, int, int, int, 
					unsigned long);
#endif

static
DGAFunctionRec NEODGAFuncs = {
   NEO_OpenFramebuffer,
   NULL,
   NEO_SetMode,
   NEO_SetViewport,
   NEO_GetViewport,
   NEO_Sync,
   NEO_FillRect,
   NEO_BlitRect,
#if 0
   NEO_BlitTransRect
#else
   NULL
#endif
};

Bool
NEODGAInit(ScreenPtr pScreen)
{   
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   NEOPtr pNEO = NEOPTR(pScrn);
   DGAModePtr modes = NULL, newmodes = NULL, currentMode;
   DisplayModePtr pMode, firstMode;
   int Bpp = pScrn->bitsPerPixel >> 3;
   int num = 0, imlines, pixlines;

   imlines =  (pScrn->videoRam * 1024) /
      (pScrn->displayWidth * (pScrn->bitsPerPixel >> 3));

   pixlines =  (imlines > 1024 && !pNEO->noAccel)  ? 1024 : imlines;

   pMode = firstMode = pScrn->modes;

   while(pMode) {

	newmodes = xrealloc(modes, (num + 1) * sizeof(DGAModeRec));

	if(!newmodes) {
	   xfree(modes);
	   return FALSE;
	}
	modes = newmodes;

	currentMode = modes + num;
	num++;

	currentMode->mode = pMode;
	currentMode->flags = DGA_CONCURRENT_ACCESS | DGA_PIXMAP_AVAILABLE;
	if (!pNEO->noAccel)
	    currentMode->flags |= (DGA_FILL_RECT | DGA_BLIT_RECT);
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
	currentMode->address = pNEO->NeoFbBase;

	currentMode->bytesPerScanline = 
			((pScrn->displayWidth * Bpp) + 3) & ~3L;
	currentMode->imageWidth = pScrn->displayWidth;
	currentMode->imageHeight =  imlines;
	currentMode->pixmapWidth = currentMode->imageWidth;
	currentMode->pixmapHeight = pixlines;
	currentMode->maxViewportX = currentMode->imageWidth - 
					currentMode->viewportWidth;
	currentMode->maxViewportY = currentMode->imageHeight -
					currentMode->viewportHeight;

	pMode = pMode->next;
	if(pMode == firstMode)
	   break;
   }

   pNEO->numDGAModes = num;
   pNEO->DGAModes = modes;

   return DGAInit(pScreen, &NEODGAFuncs, modes, num);  
}

static DisplayModePtr NEOSavedDGAModes[MAXSCREENS];

static Bool
NEO_SetMode(
   ScrnInfoPtr pScrn,
   DGAModePtr pMode
){
   int index = pScrn->pScreen->myNum;
   NEOPtr pNEO = NEOPTR(pScrn);

   if(!pMode) { /* restore the original mode */
 	if(pNEO->DGAactive) {	
	    pScrn->currentMode = NEOSavedDGAModes[index];
            NEOSwitchMode(index, pScrn->currentMode, 0);
	    NEOAdjustFrame(index, 0, 0, 0);
 	    pNEO->DGAactive = FALSE;
	}
   } else {
	if(!pNEO->DGAactive) {  /* save the old parameters */
	    NEOSavedDGAModes[index] = pScrn->currentMode;
	    pNEO->DGAactive = TRUE;
	}

        NEOSwitchMode(index, pMode->mode, 0);
   }
   
   return TRUE;
}

static int  
NEO_GetViewport(
  ScrnInfoPtr pScrn
){
    NEOPtr pNEO = NEOPTR(pScrn);

    return pNEO->DGAViewportStatus;
}

static void 
NEO_SetViewport(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int flags
){
   NEOPtr pNEO = NEOPTR(pScrn);
   vgaHWPtr hwp = VGAHWPTR(pScrn);
   
   NEOAdjustFrame(pScrn->pScreen->myNum, x, y, flags);
   /* wait for retrace */
   while((hwp->readST01(hwp) & 0x08));
   while(!(hwp->readST01(hwp) & 0x08));

   pNEO->DGAViewportStatus = 0;  
}

static void 
NEO_FillRect (
   ScrnInfoPtr pScrn, 
   int x, int y, int w, int h, 
   unsigned long color
){
    NEOPtr pNEO = NEOPTR(pScrn);

    if(pNEO->AccelInfoRec) {
	(*pNEO->AccelInfoRec->SetupForSolidFill)(pScrn, color, GXcopy, ~0);
	(*pNEO->AccelInfoRec->SubsequentSolidFillRect)(pScrn, x, y, w, h);
	SET_SYNC_FLAG(pNEO->AccelInfoRec);
    }
}

static void 
NEO_Sync(
   ScrnInfoPtr pScrn
){
    NEOPtr pNEO = NEOPTR(pScrn);

    if(pNEO->AccelInfoRec) {
	(*pNEO->AccelInfoRec->Sync)(pScrn);
    }
}

static void 
NEO_BlitRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty
){
    NEOPtr pNEO = NEOPTR(pScrn);

    if(pNEO->AccelInfoRec) {
	int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
	int ydir = (srcy < dsty) ? -1 : 1;

	(*pNEO->AccelInfoRec->SetupForScreenToScreenCopy)(
		pScrn, xdir, ydir, GXcopy, ~0, -1);
	(*pNEO->AccelInfoRec->SubsequentScreenToScreenCopy)(
		pScrn, srcx, srcy, dstx, dsty, w, h);
	SET_SYNC_FLAG(pNEO->AccelInfoRec);
    }
}

#if 0
static void 
NEO_BlitTransRect(
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
NEO_OpenFramebuffer(
   ScrnInfoPtr pScrn, 
   char **name,
   unsigned char **mem,
   int *size,
   int *offset,
   int *flags
){
    NEOPtr pNEO = NEOPTR(pScrn);

    *name = NULL; 		/* no special device */
    *mem = (unsigned char*)pNEO->NeoLinearAddr;
    *size = pNEO->NeoFbMapSize;
    *offset = 0;
    *flags = DGA_NEED_ROOT;

    return TRUE;
}
