/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/savage/savage_dga.c,v 1.7 2004/02/13 23:58:44 dawes Exp $ */

/*
 * Copyright (C) 1994-2000 The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * file: savage_dga.c
 * ported from s3v, which was ported from mga
 *
 */


#include "xaalocal.h"
#include "savage_driver.h"
#include "dgaproc.h"


Bool SavageDGAInit(ScreenPtr pScreen);
static Bool Savage_OpenFramebuffer(ScrnInfoPtr, char **, unsigned char **, 
		int *, int *, int *);
static Bool Savage_SetMode(ScrnInfoPtr, DGAModePtr);
static int  Savage_GetViewport(ScrnInfoPtr);
static void Savage_SetViewport(ScrnInfoPtr, int, int, int);
static void Savage_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void Savage_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);


static
DGAFunctionRec Savage_DGAFuncs = {
    Savage_OpenFramebuffer,
    NULL,	/* CloseFrameBuffer */
    Savage_SetMode,
    Savage_SetViewport,
    Savage_GetViewport,
    SavageAccelSync,
    Savage_FillRect,
    Savage_BlitRect,
    NULL			 /* BlitTransRect */
};

#define DGATRACE	4

/*
 * I don't understand the thinking here.  As near as I can tell, we are
 * never asked to change into a depth other than the frame buffer depth.
 * So why create modes to do so?
 */

static DGAModePtr
SavageSetupDGAMode(
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
)
{
    SavagePtr psav = SAVPTR(pScrn);
    DGAModePtr mode, newmodes = NULL;
    DisplayModePtr pMode, firstMode;
    int otherPitch, Bpp = bitsPerPixel >> 3;
    Bool oneMore;

    xf86ErrorFVerb(DGATRACE, "		SavageSetupDGAMode\n");

    pMode = firstMode = pScrn->modes;

    /*
     * DGA 1.0 would only provide modes where the depth and stride
     * matched the current desktop.  Some DGA apps might still expect
     * this, so we provide them, too.
     */

    while(pMode) {

	otherPitch = secondPitch ? secondPitch : pMode->HDisplay;

	if(pMode->HDisplay != otherPitch) {
	    newmodes = xrealloc(modes, (*num + 2) * sizeof(DGAModeRec));
	    oneMore = TRUE;
	} else {
	    newmodes = xrealloc(modes, (*num + 1) * sizeof(DGAModeRec));
	    oneMore = FALSE;
	}

	if(!newmodes) {
	   xfree(modes);
	   return NULL;
	}
	modes = newmodes;

SECOND_PASS:

	mode = modes + *num;
	(*num)++;

	mode->mode = pMode;
	mode->flags = DGA_CONCURRENT_ACCESS | DGA_PIXMAP_AVAILABLE;
	if(!psav->NoAccel)
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
	mode->xViewportStep = 2;
	mode->yViewportStep = 1;
	mode->viewportFlags = DGA_FLIP_RETRACE;
	mode->offset = 0;
	mode->address = psav->FBBase;

	xf86ErrorFVerb(DGATRACE,
	    "SavageDGAInit vpWid=%d, vpHgt=%d, Bpp=%d, mdbitsPP=%d\n",
	    mode->viewportWidth,
	    mode->viewportHeight,
	    Bpp,
	    mode->bitsPerPixel
	);

	if(oneMore) { /* first one is narrow width */
	    /* Force stride to multiple of 16 pixels. */
	    mode->bytesPerScanline = ((pMode->HDisplay + 15) & ~15) * Bpp;
	    mode->imageWidth = pMode->HDisplay;
	    mode->imageHeight =  pMode->VDisplay;
	    mode->pixmapWidth = mode->imageWidth;
	    mode->pixmapHeight = mode->imageHeight;
	    mode->maxViewportX = mode->imageWidth - mode->viewportWidth;
	    /* this might need to get clamped to some maximum */
	    mode->maxViewportY = mode->imageHeight - mode->viewportHeight;
	    oneMore = FALSE;

	    xf86ErrorFVerb(DGATRACE,
		"SavageDGAInit 1 imgHgt=%d, stride=%d\n",
		mode->imageHeight,
		mode->bytesPerScanline );
 
	    goto SECOND_PASS;
	} else {
	    mode->bytesPerScanline = ((pScrn->displayWidth + 15) & ~15) * Bpp;
	    mode->imageWidth = pScrn->displayWidth;
	    mode->imageHeight = psav->videoRambytes / mode->bytesPerScanline;
	    mode->pixmapWidth = mode->imageWidth;
	    mode->pixmapHeight = mode->imageHeight;
	    mode->maxViewportX = mode->imageWidth - mode->viewportWidth;
	    /* this might need to get clamped to some maximum */
	    mode->maxViewportY = mode->imageHeight - mode->viewportHeight;

	    xf86ErrorFVerb(DGATRACE,
		"SavageDGAInit 2 imgHgt=%d, stride=%d\n",
		mode->imageHeight,
		mode->bytesPerScanline );
	}		

	pMode = pMode->next;
	if(pMode == firstMode)
	    break;
    }

    return modes;
}


Bool
SavageDGAInit(ScreenPtr pScreen)
{   
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    SavagePtr psav = SAVPTR(pScrn);
    DGAModePtr modes = NULL;
    int num = 0;

    xf86ErrorFVerb(DGATRACE, "		SavageDGAInit\n");

    /* 8 */
    modes = SavageSetupDGAMode (pScrn, modes, &num, 8, 8, 
		(pScrn->bitsPerPixel == 8),
		(pScrn->bitsPerPixel != 8) ? 0 : pScrn->displayWidth,
		0, 0, 0, PseudoColor);

    /* 15 */
    modes = SavageSetupDGAMode (pScrn, modes, &num, 16, 15, 
		(pScrn->bitsPerPixel == 16),
		(pScrn->depth != 15) ? 0 : pScrn->displayWidth,
		0x7c00, 0x03e0, 0x001f, TrueColor);

    modes = SavageSetupDGAMode (pScrn, modes, &num, 16, 15, 
		(pScrn->bitsPerPixel == 16),
		(pScrn->depth != 15) ? 0 : pScrn->displayWidth,
		0x7c00, 0x03e0, 0x001f, DirectColor);

    /* 16 */
    modes = SavageSetupDGAMode (pScrn, modes, &num, 16, 16, 
		(pScrn->bitsPerPixel == 16),
		(pScrn->depth != 16) ? 0 : pScrn->displayWidth,
		0xf800, 0x07e0, 0x001f, TrueColor);

    modes = SavageSetupDGAMode (pScrn, modes, &num, 16, 16, 
		(pScrn->bitsPerPixel == 16),
		(pScrn->depth != 16) ? 0 : pScrn->displayWidth,
		0xf800, 0x07e0, 0x001f, DirectColor);

    /* 24-in-32 */
    modes = SavageSetupDGAMode (pScrn, modes, &num, 32, 24, 
		(pScrn->bitsPerPixel == 32),
		(pScrn->bitsPerPixel != 32) ? 0 : pScrn->displayWidth,
		0xff0000, 0x00ff00, 0x0000ff, TrueColor);

    modes = SavageSetupDGAMode (pScrn, modes, &num, 32, 24, 
		(pScrn->bitsPerPixel == 32),
		(pScrn->bitsPerPixel != 32) ? 0 : pScrn->displayWidth,
		0xff0000, 0x00ff00, 0x0000ff, DirectColor);

    psav->numDGAModes = num;
    psav->DGAModes = modes;

    return DGAInit(pScreen, &Savage_DGAFuncs, modes, num);  
}


static Bool
Savage_SetMode(
    ScrnInfoPtr pScrn,
    DGAModePtr pMode
){
    static int OldDisplayWidth[MAXSCREENS];
    static int OldBitsPerPixel[MAXSCREENS];
    static int OldDepth[MAXSCREENS];
    static DisplayModePtr OldMode[MAXSCREENS];
    int index = pScrn->pScreen->myNum;
    SavagePtr psav = SAVPTR(pScrn);

    if(!pMode) { /* restore the original mode */
	/* put the ScreenParameters back */

	pScrn->displayWidth = OldDisplayWidth[index];
	pScrn->bitsPerPixel = OldBitsPerPixel[index];
	pScrn->depth = OldDepth[index];
	pScrn->currentMode = OldMode[index];

	psav->DGAactive = FALSE;
	SavageSwitchMode(index, pScrn->currentMode, 0);
	if( psav->hwcursor && psav->hwc_on )
	    SavageShowCursor(pScrn);
    } else {
	Bool holdBIOS = psav->UseBIOS;

#if 0
	ErrorF( 
	    "pScrn->bitsPerPixel %d, pScrn->depth %d\n",
	    pScrn->bitsPerPixel, pScrn->depth);
	ErrorF(
	    " want  bitsPerPixel %d,  want  depth %d\n",
	    pMode->bitsPerPixel, pMode->depth);
#endif

	if( psav->hwcursor && psav->hwc_on) {
	    SavageHideCursor(pScrn);
	    psav->hwc_on = TRUE;    /* save for later restauration */
	}
	

	if(!psav->DGAactive) {  /* save the old parameters */
	    OldDisplayWidth[index] = pScrn->displayWidth;
	    OldBitsPerPixel[index] = pScrn->bitsPerPixel;
	    OldDepth[index] = pScrn->depth;
	    OldMode[index] = pScrn->currentMode;

	    psav->DGAactive = TRUE;
	}

	pScrn->bitsPerPixel = pMode->bitsPerPixel;
	pScrn->depth = pMode->depth;
	pScrn->displayWidth = pMode->bytesPerScanline / 
	    (pMode->bitsPerPixel >> 3);

/*	psav->UseBIOS = FALSE; */
	SavageSwitchMode(index, pMode->mode, 0);
	psav->UseBIOS = holdBIOS;
    }

    return TRUE;
}


static int	
Savage_GetViewport(
    ScrnInfoPtr pScrn
){
    SavagePtr psav = SAVPTR(pScrn);
    return psav->DGAViewportStatus;
}


static void 
Savage_SetViewport(
   ScrnInfoPtr pScrn, 
   int x, int y, 
   int flags
){
    SavagePtr psav = SAVPTR(pScrn);

    SavageAdjustFrame(pScrn->pScreen->myNum, x, y, flags);
    psav->DGAViewportStatus = 0;  /* MGAAdjustFrame loops until finished */
}

static void 
Savage_FillRect (
    ScrnInfoPtr pScrn, 
    int x, int y, int w, int h, 
    unsigned long color
){
    SavagePtr psav = SAVPTR(pScrn);

    if(psav->AccelInfoRec) {
	(*psav->AccelInfoRec->SetupForSolidFill)(pScrn, color, GXcopy, ~0);
	(*psav->AccelInfoRec->SubsequentSolidFillRect)(pScrn, x, y, w, h);
	SET_SYNC_FLAG(psav->AccelInfoRec);
    }
}

static void 
Savage_BlitRect(
    ScrnInfoPtr pScrn, 
    int srcx, int srcy, 
    int w, int h, 
    int dstx, int dsty
){
    SavagePtr psav = SAVPTR(pScrn);

    if(psav->AccelInfoRec) {
    int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
    int ydir = (srcy < dsty) ? -1 : 1;

    (*psav->AccelInfoRec->SetupForScreenToScreenCopy)(
	pScrn, xdir, ydir, GXcopy, ~0, -1);
    (*psav->AccelInfoRec->SubsequentScreenToScreenCopy)(
	pScrn, srcx, srcy, dstx, dsty, w, h);
    SET_SYNC_FLAG(psav->AccelInfoRec);
    }
}


static Bool 
Savage_OpenFramebuffer(
    ScrnInfoPtr pScrn, 
    char **name,
    unsigned char **mem,
    int *size,
    int *offset,
    int *flags
){
    SavagePtr psav = SAVPTR(pScrn);

    *name = NULL;	 /* no special device */
    *mem = (unsigned char*)psav->FrameBufferBase;
    *size = psav->videoRambytes;
    *offset = 0;
    *flags = DGA_NEED_ROOT;

    return TRUE;
}
