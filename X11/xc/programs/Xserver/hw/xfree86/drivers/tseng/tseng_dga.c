/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng_dga.c,v 1.2 2001/10/01 13:44:11 eich Exp $ */
/*
 * Copyright 2000 by Rainer Keller, <Rainer.Keller@studmail.uni-stuttgart.de>.
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
 * Authors:  Adapted from: Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *           by: Rainer Keller, <Rainer.Keller@studmail.uni-stuttgart.de>
 */

#include "tseng.h"
#include "dgaproc.h"

static Bool Tseng_OpenFramebuffer(ScrnInfoPtr, char **, unsigned char **, 
				  int *, int *, int *);
static Bool Tseng_SetMode(ScrnInfoPtr, DGAModePtr);
static void Tseng_Sync(ScrnInfoPtr);
static int  Tseng_GetViewport(ScrnInfoPtr);
static void Tseng_SetViewport(ScrnInfoPtr, int, int, int);
static void Tseng_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void Tseng_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);
/*
static void Tseng_BlitTransRect(ScrnInfoPtr, int, int, int, int, int, int, 
                                unsigned long);
*/

static
DGAFunctionRec TsengDGAFuncs = {
   Tseng_OpenFramebuffer,
   NULL, /* Tseng_CloseFramebuffer */
   Tseng_SetMode,
   Tseng_SetViewport,
   Tseng_GetViewport,
   Tseng_Sync,
   Tseng_FillRect,
   Tseng_BlitRect,
   NULL  /* Tseng_BlitTransRect */
};




Bool
TsengDGAInit(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  TsengPtr pTseng = TsengPTR(pScrn);
  DGAModePtr modes = NULL, newmodes = NULL, currentMode;
  DisplayModePtr pMode, firstMode;
  int Bpp = pScrn->bitsPerPixel >> 3;
  int num = 0;
  int imlines =  (pScrn->videoRam * 1024) /
       (pScrn->displayWidth * (pScrn->bitsPerPixel >> 3));

  
  if (!pTseng->UseLinMem)
      return FALSE;
  
  if (!pTseng->DGAnumModes) {
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
	  | ((pTseng->UseAccel) ? (DGA_FILL_RECT | DGA_BLIT_RECT) : 0);
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
      currentMode->address          = pTseng->FbBase;
      currentMode->bytesPerScanline = ((pScrn->displayWidth * Bpp) + 3) & ~3L;
      currentMode->pixmapWidth      = currentMode->imageWidth = pScrn->displayWidth;
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
    pTseng->DGAnumModes = num;
    pTseng->DGAModes = modes;
  }
  return DGAInit(pScreen, &TsengDGAFuncs, pTseng->DGAModes, pTseng->DGAnumModes);
}

static Bool 
Tseng_OpenFramebuffer(
   ScrnInfoPtr pScrn, 
   char **name,
   unsigned char **mem,
   int *size,
   int *offset,
   int *flags
){
    TsengPtr pTseng = TsengPTR(pScrn);

    *name = NULL; 		/* no special device */
    *mem = (unsigned char*)pTseng->LinFbAddress;
    *size = pTseng->FbMapSize;
    *offset = 0;                /* Always */
    *flags = 0;                 /* Root permissions OS-dependent */

    return TRUE;
}


static Bool
Tseng_SetMode(
   ScrnInfoPtr pScrn,
   DGAModePtr pMode
){
   TsengPtr pTseng = TsengPTR(pScrn);
   static int OldDisplayWidth[MAXSCREENS];
   int index = pScrn->pScreen->myNum;
   Bool ret;

   if(!pMode) { /* restore the original mode */
	/* put the ScreenParameters back */
	pScrn->displayWidth = OldDisplayWidth[index];
	ret = TsengModeInit(xf86Screens[index], pScrn->currentMode);
	pTseng->DGAactive = FALSE;
   } else {
	if(!pTseng->DGAactive) {  /* save the old parameters */
	    OldDisplayWidth[index] = pScrn->displayWidth;

	    pTseng->DGAactive = TRUE;
	}
	pScrn->displayWidth = pMode->bytesPerScanline / 
			      (pMode->bitsPerPixel >> 3);

	ret = TsengModeInit(xf86Screens[index], pMode->mode);
   }
   return ret;
}

static void 
Tseng_SetViewport(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int flags
){
   TsengPtr pTseng = TsengPTR(pScrn);
   vgaHWPtr hwp = VGAHWPTR(pScrn);

   TsengAdjustFrame(pScrn->pScreen->myNum, x, y, flags);
   while((hwp->readST01(hwp) & 0x08));
   while(!(hwp->readST01(hwp) & 0x08));

   pTseng->DGAViewportStatus = 0;  /* TsengAdjustFrame loops until finished */
}

static int  
Tseng_GetViewport(
  ScrnInfoPtr pScrn
){
    TsengPtr pTseng = TsengPTR(pScrn);

    return pTseng->DGAViewportStatus;
}



static void 
Tseng_Sync(
   ScrnInfoPtr pScrn
){
    TsengPtr pTseng = TsengPTR(pScrn);

    if(pTseng->AccelInfoRec) {
	(*pTseng->AccelInfoRec->Sync)(pScrn);
    }
}

static void 
Tseng_FillRect (
   ScrnInfoPtr pScrn, 
   int x, int y, int w, int h, 
   unsigned long color
){
    TsengPtr pTseng = TsengPTR(pScrn);

    if(pTseng->AccelInfoRec) {
	(*pTseng->AccelInfoRec->SetupForSolidFill)(pScrn, color, GXcopy, ~0);
	(*pTseng->AccelInfoRec->SubsequentSolidFillRect)(pScrn, x, y, w, h);
	SET_SYNC_FLAG(pTseng->AccelInfoRec);
    }
}

static void 
Tseng_BlitRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty
){
    TsengPtr pTseng = TsengPTR(pScrn);

    if(pTseng->AccelInfoRec) {
	int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
	int ydir = (srcy < dsty) ? -1 : 1;

	(*pTseng->AccelInfoRec->SetupForScreenToScreenCopy)(
		pScrn, xdir, ydir, GXcopy, ~0, -1);
	(*pTseng->AccelInfoRec->SubsequentScreenToScreenCopy)(
		pScrn, srcx, srcy, dstx, dsty, w, h);
	SET_SYNC_FLAG(pTseng->AccelInfoRec);
    }
}
