/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_dga.c,v 1.16 2002/09/16 18:05:55 eich Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xaalocal.h"
#include "mga_bios.h"
#include "mga.h"
#include "mga_reg.h"
#include "dgaproc.h"


static Bool MGA_OpenFramebuffer(ScrnInfoPtr, char **, unsigned char **, 
					int *, int *, int *);
static Bool MGA_SetMode(ScrnInfoPtr, DGAModePtr);
static int  MGA_GetViewport(ScrnInfoPtr);
static void MGA_SetViewport(ScrnInfoPtr, int, int, int);
static void MGA_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void MGA_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);
static void MGA_BlitTransRect(ScrnInfoPtr, int, int, int, int, int, int, 
					unsigned long);

static
DGAFunctionRec MGA_DGAFuncs = {
   MGA_OpenFramebuffer,
   NULL,
   MGA_SetMode,
   MGA_SetViewport,
   MGA_GetViewport,
   MGAStormSync,
   MGA_FillRect,
   MGA_BlitRect,
   MGA_BlitTransRect
};


static int
FindSmallestPitch(
   MGAPtr pMga,
   int Bpp,
   int width
){
   int Pitches1[] = 
	  {640, 768, 800, 960, 1024, 1152, 1280, 1600, 1920, 2048, 0};
   int Pitches2[] = 
	  {512, 640, 768, 800, 832, 960, 1024, 1152, 1280, 1600, 1664, 
		1920, 2048, 0};
   int *linePitches = NULL;
   int pitch;
        

   if(!pMga->NoAccel) {
	switch(pMga->Chipset) {
	case PCI_CHIP_MGA2064:
	    linePitches = Pitches1;
	    break;
	case PCI_CHIP_MGA2164:
	case PCI_CHIP_MGA2164_AGP:
	case PCI_CHIP_MGA1064:
	    linePitches = Pitches2;
	    break;
	}
   }

   pitch = pMga->Roundings[Bpp - 1] - 1;

   if(linePitches) {
	while((*linePitches < width) || (*linePitches & pitch))
	   linePitches++;
	return *linePitches;
   } 

   return ((width + pitch) & ~pitch); 
}

static DGAModePtr
MGASetupDGAMode(
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
   DisplayModePtr firstMode, pMode;
   MGAPtr pMga = MGAPTR(pScrn);
   DGAModePtr mode, newmodes;
   int size, pitch, Bpp = bitsPerPixel >> 3;

SECOND_PASS:

   pMode = firstMode = pScrn->modes;

   while(1) {

        
	pitch = FindSmallestPitch(pMga, Bpp, pMode->HDisplay);
	size = pitch * Bpp * pMode->VDisplay;

	if((!secondPitch || (pitch != secondPitch)) &&
		(size <= pMga->FbUsableSize)) {

	    if(secondPitch)
		pitch = secondPitch; 

	    if(!(newmodes = xrealloc(modes, (*num + 1) * sizeof(DGAModeRec))))
		break;

	    modes = newmodes;
	    mode = modes + *num;

	    mode->mode = pMode;
	    mode->flags = DGA_CONCURRENT_ACCESS;
            if(pixmap)
		mode->flags |= DGA_PIXMAP_AVAILABLE;
	    if(!pMga->NoAccel) {
		mode->flags |= DGA_FILL_RECT | DGA_BLIT_RECT;
		if((Bpp != 3) && (pMga->Chipset != PCI_CHIP_MGA2064))
		    mode->flags |= DGA_BLIT_RECT_TRANS;
	    }
	    if(pMode->Flags & V_DBLSCAN)
		mode->flags |= DGA_DOUBLESCAN;
	    if(pMode->Flags & V_INTERLACE)
		mode->flags |= DGA_INTERLACED;
	    mode->byteOrder = pScrn->imageByteOrder;
	    mode->depth = depth;
	    mode->bitsPerPixel = bitsPerPixel;
	    mode->red_mask = red;
	    mode->green_mask = green;
	    mode->blue_mask = blue;
	    mode->visualClass = visualClass;
	    mode->viewportWidth = pMode->HDisplay;
	    mode->viewportHeight = pMode->VDisplay;
	    mode->xViewportStep = (3 - pMga->BppShifts[Bpp - 1]);
	    if((Bpp == 3) && 
		    (pMga->Chipset == PCI_CHIP_MGAG400 || pMga->Chipset == PCI_CHIP_MGAG550))
		mode->xViewportStep <<= 1;
	    mode->yViewportStep = 1;
	    mode->viewportFlags = DGA_FLIP_RETRACE;
	    mode->offset = pMga->YDstOrg * Bpp;  /* gonna need to fix that */
	    mode->address = pMga->FbStart;
	    mode->bytesPerScanline = pitch * Bpp;
	    mode->imageWidth = pitch;
	    mode->imageHeight =  pMga->FbUsableSize / mode->bytesPerScanline; 
	    mode->pixmapWidth = pitch;
	    mode->pixmapHeight = (min(pMga->FbUsableSize, 16*1024*1024)) / 
					mode->bytesPerScanline;
	    mode->maxViewportX = mode->imageWidth - mode->viewportWidth;
	    mode->maxViewportY = (pMga->FbUsableSize / mode->bytesPerScanline) -
				 	mode->viewportHeight;

	    if( (pMga->Chipset == PCI_CHIP_MGA2064) ||
		(pMga->Chipset == PCI_CHIP_MGA2164) ||
		(pMga->Chipset == PCI_CHIP_MGA2164_AGP)) 
	    {
		int tmp;

		tmp = (8*1024*1024 / mode->bytesPerScanline) -
					mode->viewportHeight;
		if(tmp < 0) tmp = 0;
		if(tmp < mode->maxViewportY)
		    mode->maxViewportY = tmp;
	    }

	    (*num)++;
	}

	pMode = pMode->next;
	if(pMode == firstMode)
	   break;
    }

    if(secondPitch) {
	secondPitch = 0;
	goto SECOND_PASS;
    }

    return modes;
}


Bool
MGADGAInit(ScreenPtr pScreen)
{   
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   MGAPtr pMga = MGAPTR(pScrn);
   DGAModePtr modes = NULL;
   int num = 0;

   /* 8 */
   modes = MGASetupDGAMode (pScrn, modes, &num, 8, 8, 
		(pScrn->bitsPerPixel == 8),
		(pScrn->bitsPerPixel != 8) ? 0 : pScrn->displayWidth,
		0, 0, 0, PseudoColor);

   /* 15 */
   modes = MGASetupDGAMode (pScrn, modes, &num, 16, 15, 
		(pScrn->bitsPerPixel == 16),
		(pScrn->depth != 15) ? 0 : pScrn->displayWidth,
		0x7c00, 0x03e0, 0x001f, TrueColor);

   modes = MGASetupDGAMode (pScrn, modes, &num, 16, 15, 
		(pScrn->bitsPerPixel == 16),
		(pScrn->depth != 15) ? 0 : pScrn->displayWidth,
		0x7c00, 0x03e0, 0x001f, DirectColor);

   /* 16 */
   modes = MGASetupDGAMode (pScrn, modes, &num, 16, 16, 
		(pScrn->bitsPerPixel == 16),
		(pScrn->depth != 16) ? 0 : pScrn->displayWidth,
		0xf800, 0x07e0, 0x001f, TrueColor);

   modes = MGASetupDGAMode (pScrn, modes, &num, 16, 16, 
		(pScrn->bitsPerPixel == 16),
		(pScrn->depth != 16) ? 0 : pScrn->displayWidth,
		0xf800, 0x07e0, 0x001f, DirectColor);

   /* 24 */
   modes = MGASetupDGAMode (pScrn, modes, &num, 24, 24, 
		(pScrn->bitsPerPixel == 24),
		(pScrn->bitsPerPixel != 24) ? 0 : pScrn->displayWidth,
		0xff0000, 0x00ff00, 0x0000ff, TrueColor);

   modes = MGASetupDGAMode (pScrn, modes, &num, 24, 24, 
		(pScrn->bitsPerPixel == 24),
		(pScrn->bitsPerPixel != 24) ? 0 : pScrn->displayWidth,
		0xff0000, 0x00ff00, 0x0000ff, DirectColor);

   /* 32 */
   modes = MGASetupDGAMode (pScrn, modes, &num, 32, 24, 
		(pScrn->bitsPerPixel == 32),
		(pScrn->bitsPerPixel != 32) ? 0 : pScrn->displayWidth,
		0xff0000, 0x00ff00, 0x0000ff, TrueColor);

   modes = MGASetupDGAMode (pScrn, modes, &num, 32, 24, 
		(pScrn->bitsPerPixel == 32),
		(pScrn->bitsPerPixel != 32) ? 0 : pScrn->displayWidth,
		0xff0000, 0x00ff00, 0x0000ff, DirectColor);

   pMga->numDGAModes = num;
   pMga->DGAModes = modes;

   return DGAInit(pScreen, &MGA_DGAFuncs, modes, num);  
}


static int 
BitsSet(unsigned long data)
{
   unsigned long mask;
   int set = 0;

   for(mask = 1; mask; mask <<= 1)
        if(mask & data) set++;   

   return set;
}

static Bool
MGA_SetMode(
   ScrnInfoPtr pScrn,
   DGAModePtr pMode
){
   static MGAFBLayout SavedLayouts[MAXSCREENS];
   int index = pScrn->pScreen->myNum;

   MGAPtr pMga = MGAPTR(pScrn);

   if(!pMode) { /* restore the original mode */
      if(pMga->DGAactive)
        memcpy(&pMga->CurrentLayout, &SavedLayouts[index], sizeof(MGAFBLayout));
                
      pScrn->currentMode = pMga->CurrentLayout.mode;
      pScrn->SwitchMode(index, pScrn->currentMode, 0);
      MGAAdjustFrame(index, pScrn->frameX0, pScrn->frameY0, 0);
      pMga->DGAactive = FALSE;
   } else {
      if(!pMga->DGAactive) {  /* save the old parameters */
	memcpy(&SavedLayouts[index], &pMga->CurrentLayout, sizeof(MGAFBLayout));
	pMga->DGAactive = TRUE;
      }
      /* update CurrentLayout */
      pMga->CurrentLayout.bitsPerPixel = pMode->bitsPerPixel;
      pMga->CurrentLayout.depth = pMode->depth;
      pMga->CurrentLayout.displayWidth = pMode->bytesPerScanline / 
                              (pMode->bitsPerPixel >> 3);
      pMga->CurrentLayout.weight.red = BitsSet(pMode->red_mask);
      pMga->CurrentLayout.weight.green = BitsSet(pMode->green_mask);
      pMga->CurrentLayout.weight.blue = BitsSet(pMode->blue_mask);
      pMga->CurrentLayout.Overlay8Plus24 = FALSE;
      /* MGAModeInit() will set the mode field */

      pScrn->SwitchMode(index, pMode->mode, 0);
   }
   
   return TRUE;
}



static int  
MGA_GetViewport(
  ScrnInfoPtr pScrn
){
    MGAPtr pMga = MGAPTR(pScrn);

    return pMga->DGAViewportStatus;
}

static void 
MGA_SetViewport(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int flags
){
   MGAPtr pMga = MGAPTR(pScrn);

   MGAAdjustFrame(pScrn->pScreen->myNum, x, y, flags);
   pMga->DGAViewportStatus = 0;  /* MGAAdjustFrame loops until finished */
}

static void 
MGA_FillRect (
   ScrnInfoPtr pScrn, 
   int x, int y, int w, int h, 
   unsigned long color
){
    MGAPtr pMga = MGAPTR(pScrn);

    if(!pMga->AccelInfoRec) return;

    switch(pMga->CurrentLayout.bitsPerPixel) {
    case 8:
	Mga8SetupForSolidFill(pScrn, color, GXcopy, ~0);
	break;
    case 16:
	Mga16SetupForSolidFill(pScrn, color, GXcopy, ~0);
	break;
    case 24:
	Mga24SetupForSolidFill(pScrn, color, GXcopy, ~0);
	break;
    case 32:
	Mga32SetupForSolidFill(pScrn, color, GXcopy, ~0);
	break;
    }

    (*pMga->AccelInfoRec->SubsequentSolidFillRect)(pScrn, x, y, w, h);

    SET_SYNC_FLAG(pMga->AccelInfoRec);
}

static void 
MGA_BlitRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty
){
    MGAPtr pMga = MGAPTR(pScrn);
    int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
    int ydir = (srcy < dsty) ? -1 : 1;

    if(!pMga->AccelInfoRec) return;

    switch(pMga->CurrentLayout.bitsPerPixel) {
    case 8:
	Mga8SetupForScreenToScreenCopy(
		pScrn, xdir, ydir, GXcopy, ~0, -1);
	break;
    case 16:
	Mga16SetupForScreenToScreenCopy(
		pScrn, xdir, ydir, GXcopy, ~0, -1);
	break;
    case 24:
	Mga24SetupForScreenToScreenCopy(
		pScrn, xdir, ydir, GXcopy, ~0, -1);
	break;
    case 32:
	Mga32SetupForScreenToScreenCopy(
		pScrn, xdir, ydir, GXcopy, ~0, -1);
	break;
    }

    (*pMga->AccelInfoRec->SubsequentScreenToScreenCopy)(
		pScrn, srcx, srcy, dstx, dsty, w, h);

    SET_SYNC_FLAG(pMga->AccelInfoRec);
}


static void 
MGA_BlitTransRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty,
   unsigned long color
){
    MGAPtr pMga = MGAPTR(pScrn);
    int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
    int ydir = (srcy < dsty) ? -1 : 1;

    if(!pMga->AccelInfoRec) return;
    if(pMga->CurrentLayout.bitsPerPixel == 24) return;
    if(pMga->Chipset == PCI_CHIP_MGA2064) return;

    pMga->DrawTransparent = TRUE;

    switch(pMga->CurrentLayout.bitsPerPixel) {
    case 8:
	Mga8SetupForScreenToScreenCopy(
		pScrn, xdir, ydir, GXcopy, ~0, color);
	break;
    case 16:
	Mga16SetupForScreenToScreenCopy(
		pScrn, xdir, ydir, GXcopy, ~0, color);
	break;
    case 32:
	Mga32SetupForScreenToScreenCopy(
		pScrn, xdir, ydir, GXcopy, ~0, color);
	break;
    }

    pMga->DrawTransparent = FALSE;

    (*pMga->AccelInfoRec->SubsequentScreenToScreenCopy)(
		pScrn, srcx, srcy, dstx, dsty, w, h);

    SET_SYNC_FLAG(pMga->AccelInfoRec);
}


static Bool 
MGA_OpenFramebuffer(
   ScrnInfoPtr pScrn, 
   char **name,
   unsigned char **mem,
   int *size,
   int *offset,
   int *flags
){
    MGAPtr pMga = MGAPTR(pScrn);

    *name = NULL; 		/* no special device */
    *mem = (unsigned char*)pMga->FbAddress;
    *size = pMga->FbMapSize;
    *offset = 0;
    *flags = DGA_NEED_ROOT;

    return TRUE;
}
