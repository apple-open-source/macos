/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cirrus/cir_dga.c,v 1.7 2001/10/01 13:44:05 eich Exp $ */
/*
 * Copyright 2000 by Egbert Eich
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
 * EGBERT EICH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Adapted from: Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *           by: Egbert Eich
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xaalocal.h"
#include "vgaHW.h"
#include "cir.h"
#include "dgaproc.h"

static Bool Cir_OpenFramebuffer(ScrnInfoPtr, char **, unsigned char **, 
				  int *, int *, int *);
static Bool Cir_SetMode(ScrnInfoPtr, DGAModePtr);
static void Cir_Sync(ScrnInfoPtr);
static int  Cir_GetViewport(ScrnInfoPtr);
static void Cir_SetViewport(ScrnInfoPtr, int, int, int);
static void Cir_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void Cir_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);
/*
static void Cir_BlitTransRect(ScrnInfoPtr, int, int, int, int, int, int, 
                                unsigned long);
*/

static
DGAFunctionRec CirDGAFuncs = {
   Cir_OpenFramebuffer,
   NULL, /* Cir_CloseFramebuffer */
   Cir_SetMode,
   Cir_SetViewport,
   Cir_GetViewport,
   Cir_Sync,
   Cir_FillRect,
   Cir_BlitRect,
   NULL  /* Cir_BlitTransRect */
};




Bool
CirDGAInit(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  CirPtr pCir = CIRPTR(pScrn);
  DGAModePtr modes = NULL, newmodes = NULL, currentMode;
  DisplayModePtr pMode, firstMode;
  int Bpp = pScrn->bitsPerPixel >> 3;
  int num = 0;
  int imlines =  (pScrn->videoRam * 1024) /
       (pScrn->displayWidth * (pScrn->bitsPerPixel >> 3));

  
  if (!pCir->DGAnumModes) {
    pMode = firstMode = pScrn->modes;
    while (pMode) {
      newmodes = xrealloc(modes, (num + 1) * sizeof (DGAModeRec));
      if (!newmodes) {
	xfree(modes);
	return FALSE;
      }
      modes = newmodes;
      currentMode = modes + num;
      num++;
      (void)memset(currentMode, 1, sizeof(DGAModeRec));
      currentMode->mode = pMode;
      currentMode->flags = DGA_PIXMAP_AVAILABLE
	  | ((!pCir->NoAccel) ? (DGA_FILL_RECT | DGA_BLIT_RECT) : 0);
      if (pMode->Flags & V_DBLSCAN)
	currentMode->flags |= DGA_DOUBLESCAN;
      if(pMode->Flags & V_INTERLACE)
	currentMode->flags |= DGA_INTERLACED;
      currentMode->byteOrder        = pScrn->imageByteOrder;
      currentMode->depth            = pScrn->depth;
      currentMode->bitsPerPixel     = pScrn->bitsPerPixel;
      currentMode->red_mask         = pScrn->mask.red;
      currentMode->green_mask       = pScrn->mask.green;
      currentMode->blue_mask        = pScrn->mask.blue;
      currentMode->visualClass      = (Bpp == 1) ? PseudoColor : TrueColor;
      currentMode->viewportWidth    = pMode->HDisplay;
      currentMode->viewportHeight   = pMode->VDisplay;
      currentMode->xViewportStep    = 1; /*  The granularity of x and y pos. */
      currentMode->yViewportStep    = 1;
      currentMode->viewportFlags    = 0 /*DGA_FLIP_RETRACE*/;
      currentMode->offset           = 0;
      currentMode->address          = pCir->FbBase;
      currentMode->bytesPerScanline = ((pScrn->displayWidth * Bpp) + 3) & ~3L;
      currentMode->pixmapWidth      = currentMode->imageWidth 
	                               = pScrn->displayWidth;
      currentMode->pixmapHeight     = currentMode->imageHeight = imlines;
      currentMode->maxViewportX     = currentMode->imageWidth - 
                                      currentMode->viewportWidth;
      /* this might need to get clamped to some maximum */
      currentMode->maxViewportY     = currentMode->imageHeight -
                                      currentMode->viewportHeight;
      
      pMode = pMode->next;
      if(pMode == firstMode)
	break;
    }
    pCir->DGAnumModes = num;
    pCir->DGAModes = modes;
  }
  return DGAInit(pScreen, &CirDGAFuncs, pCir->DGAModes, pCir->DGAnumModes);
}

static Bool 
Cir_OpenFramebuffer(
   ScrnInfoPtr pScrn, 
   char **name,
   unsigned char **mem,
   int *size,
   int *offset,
   int *flags
){
    CirPtr pCir = CIRPTR(pScrn);

    *name = NULL; 		/* no special device */
    *mem = (unsigned char*)(long)pCir->FbAddress;
    *size = pCir->FbMapSize;
    *offset = 0;                /* Always */
    *flags = 0;                 /* Root permissions OS-dependent */

    return TRUE;
}


static Bool
Cir_SetMode(
   ScrnInfoPtr pScrn,
   DGAModePtr pMode
){
   CirPtr pCir = CIRPTR(pScrn);
   static int OldDisplayWidth[MAXSCREENS];
   int index = pScrn->pScreen->myNum;
   Bool ret = FALSE;

   if(!pMode) { /* restore the original mode */
	/* put the ScreenParameters back */
	pScrn->displayWidth = OldDisplayWidth[index];
	ret = pCir->DGAModeInit(xf86Screens[index], pScrn->currentMode);
	pCir->DGAactive = FALSE;
   } else {
	if(!pCir->DGAactive) {  /* save the old parameters */
	    OldDisplayWidth[index] = pScrn->displayWidth;

	    pCir->DGAactive = TRUE;
	}
	pScrn->displayWidth = pMode->bytesPerScanline / 
			      (pMode->bitsPerPixel >> 3);

	ret = pCir->DGAModeInit(xf86Screens[index], pMode->mode);
   }
   return ret;
}

static void 
Cir_SetViewport(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int flags
){
   CirPtr pCir = CIRPTR(pScrn);
   vgaHWPtr hwp = VGAHWPTR(pScrn);

   pScrn->AdjustFrame(pScrn->pScreen->myNum, x, y, flags);

   while((hwp->readST01(hwp) & 0x08));
   while(!(hwp->readST01(hwp) & 0x08));

   pCir->DGAViewportStatus = 0;  /* AdjustFrame loops until finished */
}

static int  
Cir_GetViewport(
  ScrnInfoPtr pScrn
){
    CirPtr pCir = CIRPTR(pScrn);

    return pCir->DGAViewportStatus;
}



static void 
Cir_Sync(
   ScrnInfoPtr pScrn
){
    CirPtr pCir = CIRPTR(pScrn);

    if(pCir->AccelInfoRec) {
	(*pCir->AccelInfoRec->Sync)(pScrn);
    }
}

static void 
Cir_FillRect (
   ScrnInfoPtr pScrn, 
   int x, int y, int w, int h, 
   unsigned long color
){
    CirPtr pCir = CIRPTR(pScrn);

    if(pCir->AccelInfoRec) {
	(*pCir->AccelInfoRec->SetupForSolidFill)(pScrn, color, GXcopy, ~0);
	(*pCir->AccelInfoRec->SubsequentSolidFillRect)(pScrn, x, y, w, h);
	SET_SYNC_FLAG(pCir->AccelInfoRec);
    }
}

static void 
Cir_BlitRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty
){
    CirPtr pCir = CIRPTR(pScrn);

    if(pCir->AccelInfoRec) {
	int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
	int ydir = (srcy < dsty) ? -1 : 1;

	(*pCir->AccelInfoRec->SetupForScreenToScreenCopy)(
		pScrn, xdir, ydir, GXcopy, ~0, -1);
	(*pCir->AccelInfoRec->SubsequentScreenToScreenCopy)(
		pScrn, srcx, srcy, dstx, dsty, w, h);
	SET_SYNC_FLAG(pCir->AccelInfoRec);
    }
}
