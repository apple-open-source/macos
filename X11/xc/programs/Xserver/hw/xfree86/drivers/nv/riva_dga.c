/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/riva_dga.c,v 1.1 2003/07/31 20:24:29 mvojkovi Exp $ */

#include "riva_local.h"
#include "riva_include.h"
#include "riva_type.h"
#include "riva_proto.h"
#include "xaalocal.h"
#include "dgaproc.h"


static Bool Riva_OpenFramebuffer(ScrnInfoPtr, char **, unsigned char **, 
					int *, int *, int *);
static Bool Riva_SetMode(ScrnInfoPtr, DGAModePtr);
static int  Riva_GetViewport(ScrnInfoPtr);
static void Riva_SetViewport(ScrnInfoPtr, int, int, int);
static void Riva_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void Riva_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);
static void Riva_BlitTransRect(ScrnInfoPtr, int, int, int, int, int, int, 
					unsigned long);

static
DGAFunctionRec Riva_DGAFuncs = {
   Riva_OpenFramebuffer,
   NULL,
   Riva_SetMode,
   Riva_SetViewport,
   Riva_GetViewport,
   RivaSync,
   Riva_FillRect,
   Riva_BlitRect,
   Riva_BlitTransRect
};



static DGAModePtr
RivaSetupDGAMode(
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
   RivaPtr pRiva = RivaPTR(pScrn);
   DGAModePtr mode, newmodes;
   int size, pitch, Bpp = bitsPerPixel >> 3;

SECOND_PASS:

   pMode = firstMode = pScrn->modes;

   while(1) {

	pitch = (pMode->HDisplay + 31) & ~31;
	size = pitch * Bpp * pMode->VDisplay;

	if((!secondPitch || (pitch != secondPitch)) &&
		(size <= pRiva->FbUsableSize)) {

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
	    if(!pRiva->NoAccel)
		mode->flags |= DGA_FILL_RECT | DGA_BLIT_RECT;
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
	    mode->xViewportStep = 4 / Bpp;
	    mode->yViewportStep = 1;
	    mode->viewportFlags = DGA_FLIP_RETRACE;
	    mode->offset = 0;
	    mode->address = pRiva->FbStart;
	    mode->bytesPerScanline = pitch * Bpp;
	    mode->imageWidth = pitch;
	    mode->imageHeight =  pRiva->FbUsableSize / mode->bytesPerScanline; 
	    mode->pixmapWidth = mode->imageWidth;
	    mode->pixmapHeight = mode->imageHeight;
	    mode->maxViewportX = mode->imageWidth - mode->viewportWidth;
	    mode->maxViewportY = mode->imageHeight - mode->viewportHeight;
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
RivaDGAInit(ScreenPtr pScreen)
{   
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   RivaPtr pRiva = RivaPTR(pScrn);
   DGAModePtr modes = NULL;
   int num = 0;

   /* 8 */
   modes = RivaSetupDGAMode (pScrn, modes, &num, 8, 8, 
		(pScrn->bitsPerPixel == 8),
		(pScrn->bitsPerPixel != 8) ? 0 : pScrn->displayWidth,
		0, 0, 0, PseudoColor);

   /* 15 */
   modes = RivaSetupDGAMode (pScrn, modes, &num, 16, 15, 
		(pScrn->bitsPerPixel == 16),
		(pScrn->depth != 15) ? 0 : pScrn->displayWidth,
		0x7c00, 0x03e0, 0x001f, TrueColor);

   /* 32 */
   modes = RivaSetupDGAMode (pScrn, modes, &num, 32, 24, 
		(pScrn->bitsPerPixel == 32),
		(pScrn->bitsPerPixel != 32) ? 0 : pScrn->displayWidth,
		0xff0000, 0x00ff00, 0x0000ff, TrueColor);

   pRiva->numDGAModes = num;
   pRiva->DGAModes = modes;

   return DGAInit(pScreen, &Riva_DGAFuncs, modes, num);  
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
Riva_SetMode(
   ScrnInfoPtr pScrn,
   DGAModePtr pMode
){
   static RivaFBLayout SavedLayouts[MAXSCREENS];
   int index = pScrn->pScreen->myNum;

   RivaPtr pRiva = RivaPTR(pScrn);

   if(!pMode) { /* restore the original mode */
      if(pRiva->DGAactive)
        memcpy(&pRiva->CurrentLayout, &SavedLayouts[index], sizeof(RivaFBLayout));
                
      pScrn->currentMode = pRiva->CurrentLayout.mode;
      RivaSwitchMode(index, pScrn->currentMode, 0);
      RivaAdjustFrame(index, pScrn->frameX0, pScrn->frameY0, 0);
      pRiva->DGAactive = FALSE;
   } else {
      if(!pRiva->DGAactive) {  /* save the old parameters */
	memcpy(&SavedLayouts[index], &pRiva->CurrentLayout, sizeof(RivaFBLayout));
	pRiva->DGAactive = TRUE;
      }

      /* update CurrentLayout */
      pRiva->CurrentLayout.bitsPerPixel = pMode->bitsPerPixel;
      pRiva->CurrentLayout.depth = pMode->depth;
      pRiva->CurrentLayout.displayWidth = pMode->bytesPerScanline / 
                              (pMode->bitsPerPixel >> 3);
      pRiva->CurrentLayout.weight.red = BitsSet(pMode->red_mask);
      pRiva->CurrentLayout.weight.green = BitsSet(pMode->green_mask);
      pRiva->CurrentLayout.weight.blue = BitsSet(pMode->blue_mask);
      /* RivaModeInit() will set the mode field */
      RivaSwitchMode(index, pMode->mode, 0);
   }
   
   return TRUE;
}



static int  
Riva_GetViewport(
  ScrnInfoPtr pScrn
){
    RivaPtr pRiva = RivaPTR(pScrn);

    return pRiva->DGAViewportStatus;
}

static void 
Riva_SetViewport(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int flags
){
   RivaPtr pRiva = RivaPTR(pScrn);

   RivaAdjustFrame(pScrn->pScreen->myNum, x, y, flags);

   while(VGA_RD08(pRiva->riva.PCIO, 0x3da) & 0x08);
   while(!(VGA_RD08(pRiva->riva.PCIO, 0x3da) & 0x08));

   pRiva->DGAViewportStatus = 0;  
}

static void 
Riva_FillRect (
   ScrnInfoPtr pScrn, 
   int x, int y, int w, int h, 
   unsigned long color
){
    RivaPtr pRiva = RivaPTR(pScrn);

    if(!pRiva->AccelInfoRec) return;

    (*pRiva->AccelInfoRec->SetupForSolidFill)(pScrn, color, GXcopy, ~0);
    (*pRiva->AccelInfoRec->SubsequentSolidFillRect)(pScrn, x, y, w, h);

    SET_SYNC_FLAG(pRiva->AccelInfoRec);
}

static void 
Riva_BlitRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty
){
    RivaPtr pRiva = RivaPTR(pScrn);
    int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
    int ydir = (srcy < dsty) ? -1 : 1;

    if(!pRiva->AccelInfoRec) return;

    (*pRiva->AccelInfoRec->SetupForScreenToScreenCopy)(
		pScrn, xdir, ydir, GXcopy, ~0, -1);

    (*pRiva->AccelInfoRec->SubsequentScreenToScreenCopy)(
		pScrn, srcx, srcy, dstx, dsty, w, h);

    SET_SYNC_FLAG(pRiva->AccelInfoRec);
}


static void 
Riva_BlitTransRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty,
   unsigned long color
){
   /* not implemented... yet */
}


static Bool 
Riva_OpenFramebuffer(
   ScrnInfoPtr pScrn, 
   char **name,
   unsigned char **mem,
   int *size,
   int *offset,
   int *flags
){
    RivaPtr pRiva = RivaPTR(pScrn);

    *name = NULL; 		/* no special device */
    *mem = (unsigned char*)pRiva->FbAddress;
    *size = pRiva->FbMapSize;
    *offset = 0;
    *flags = DGA_NEED_ROOT;

    return TRUE;
}
