/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/chips/ct_dga.c,v 1.5 2002/11/25 14:04:58 eich Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xaalocal.h"
#include "ct_driver.h"
#include "dgaproc.h"


static Bool CHIPS_OpenFramebuffer(ScrnInfoPtr, char **, unsigned char **, 
					int *, int *, int *);
static Bool CHIPS_SetMode(ScrnInfoPtr, DGAModePtr);
static int  CHIPS_GetViewport(ScrnInfoPtr);
static void CHIPS_SetViewport(ScrnInfoPtr, int, int, int);
static void CHIPS_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void CHIPS_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);
#if 0
static void CHIPS_BlitTransRect(ScrnInfoPtr, int, int, int, int, int, int, 
					unsigned long);
#endif

static
DGAFunctionRec CHIPS_DGAFuncs = {
   CHIPS_OpenFramebuffer,
   NULL,
   CHIPS_SetMode,
   CHIPS_SetViewport,
   CHIPS_GetViewport,
   CHIPSSync,
   CHIPS_FillRect,
   CHIPS_BlitRect,
#if 0
   CHIPS_BlitTransRect
#else
   NULL
#endif
};

static
DGAFunctionRec CHIPS_MMIODGAFuncs = {
   CHIPS_OpenFramebuffer,
   NULL,
   CHIPS_SetMode,
   CHIPS_SetViewport,
   CHIPS_GetViewport,
   CHIPSMMIOSync,
   CHIPS_FillRect,
   CHIPS_BlitRect,
#if 0
   CHIPS_BlitTransRect
#else
   NULL
#endif
};

static
DGAFunctionRec CHIPS_HiQVDGAFuncs = {
   CHIPS_OpenFramebuffer,
   NULL,
   CHIPS_SetMode,
   CHIPS_SetViewport,
   CHIPS_GetViewport,
   CHIPSHiQVSync,
   CHIPS_FillRect,
   CHIPS_BlitRect,
#if 0
   CHIPS_BlitTransRect
#else
   NULL
#endif
};


Bool
CHIPSDGAInit(ScreenPtr pScreen)
{   
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   CHIPSPtr cPtr = CHIPSPTR(pScrn);
   DGAModePtr modes = NULL, newmodes = NULL, currentMode;
   DisplayModePtr pMode, firstMode;
   int Bpp = pScrn->bitsPerPixel >> 3;
   int num = 0;
   Bool oneMore;
   int imlines =  (pScrn->videoRam * 1024) /
      (pScrn->displayWidth * (pScrn->bitsPerPixel >> 3));

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
	if(cPtr->Flags & ChipsAccelSupport)
	   currentMode->flags |= (cPtr->Flags & ChipsAccelSupport) 
	     ? (DGA_FILL_RECT | DGA_BLIT_RECT) : 0;
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
 	currentMode->viewportFlags = DGA_FLIP_RETRACE | DGA_FLIP_IMMEDIATE;
	currentMode->offset = 0;
	currentMode->address = cPtr->FbBase;

	if(oneMore) { /* first one is narrow width */
	    currentMode->bytesPerScanline = ((pMode->HDisplay * Bpp) + 3) & ~3L;
	    currentMode->imageWidth = pMode->HDisplay;
	    currentMode->imageHeight =  imlines;
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
	    currentMode->imageHeight =  imlines;
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

   cPtr->numDGAModes = num;
   cPtr->DGAModes = modes;

   if (IS_HiQV(cPtr)) {
	return DGAInit(pScreen, &CHIPS_HiQVDGAFuncs, modes, num);  
   } else {
	if(!cPtr->UseMMIO) {
	    return DGAInit(pScreen, &CHIPS_DGAFuncs, modes, num);  
	} else {
	    return DGAInit(pScreen, &CHIPS_MMIODGAFuncs, modes, num);  
	}
   }
}


static Bool
CHIPS_SetMode(
   ScrnInfoPtr pScrn,
   DGAModePtr pMode
){
   static int OldDisplayWidth[MAXSCREENS];
   int index = pScrn->pScreen->myNum;

   CHIPSPtr cPtr = CHIPSPTR(pScrn);

   if (!pMode) { /* restore the original mode */
	/* put the ScreenParameters back */
       if (cPtr->DGAactive) {
           pScrn->displayWidth = OldDisplayWidth[index];
	   pScrn->EnterVT(pScrn->scrnIndex,0);

	   cPtr->DGAactive = FALSE;
       }
   } else {
	if(!cPtr->DGAactive) {  /* save the old parameters */
	    OldDisplayWidth[index] = pScrn->displayWidth;
	    pScrn->LeaveVT(pScrn->scrnIndex,0);
	    cPtr->DGAactive = TRUE;
	}

	pScrn->displayWidth = pMode->bytesPerScanline / 
			      (pMode->bitsPerPixel >> 3);

        CHIPSSwitchMode(index, pMode->mode, 0);
   }
   
   return TRUE;
}



static int  
CHIPS_GetViewport(
  ScrnInfoPtr pScrn
){
    CHIPSPtr cPtr = CHIPSPTR(pScrn);

    return cPtr->DGAViewportStatus;
}

static void 
CHIPS_SetViewport(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int flags
   ){
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CHIPSPtr cPtr = CHIPSPTR(pScrn);
  
    if (flags & DGA_FLIP_RETRACE) {
 	while ((hwp->readST01(hwp)) & 0x08){};
 	while (!(hwp->readST01(hwp)) & 0x08){};
    }

    CHIPSAdjustFrame(pScrn->pScreen->myNum, x, y, flags);
    cPtr->DGAViewportStatus = 0;  /* CHIPSAdjustFrame loops until finished */
}

static void 
CHIPS_FillRect (
   ScrnInfoPtr pScrn, 
   int x, int y, int w, int h, 
   unsigned long color
){
    CHIPSPtr cPtr = CHIPSPTR(pScrn);

    if(cPtr->AccelInfoRec) {
	(*cPtr->AccelInfoRec->SetupForSolidFill)(pScrn, color, GXcopy, ~0);
	(*cPtr->AccelInfoRec->SubsequentSolidFillRect)(pScrn, x, y, w, h);
	SET_SYNC_FLAG(cPtr->AccelInfoRec);
    }
}

static void 
CHIPS_BlitRect(
   ScrnInfoPtr pScrn, 
   int srcx, int srcy, 
   int w, int h, 
   int dstx, int dsty
){
    CHIPSPtr cPtr = CHIPSPTR(pScrn);

    if(cPtr->AccelInfoRec) {
	int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
	int ydir = (srcy < dsty) ? -1 : 1;

	(*cPtr->AccelInfoRec->SetupForScreenToScreenCopy)(
		pScrn, xdir, ydir, GXcopy, ~0, -1);
	(*cPtr->AccelInfoRec->SubsequentScreenToScreenCopy)(
		pScrn, srcx, srcy, dstx, dsty, w, h);
	SET_SYNC_FLAG(cPtr->AccelInfoRec);
    }
}

#if 0
static void 
CHIPS_BlitTransRect(
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
CHIPS_OpenFramebuffer(
   ScrnInfoPtr pScrn, 
   char **name,
   unsigned char **mem,
   int *size,
   int *offset,
   int *flags
){
    CHIPSPtr cPtr = CHIPSPTR(pScrn);

    *name = NULL; 		/* no special device */
    *mem = (unsigned char*)cPtr->FbAddress;
    *size = cPtr->FbMapSize;
    *offset = 0;
    *flags = DGA_NEED_ROOT;

    return TRUE;
}
