/* Header:   //Mercury/Projects/archives/XFree86/4.0/smi_dga.c-arc   1.5   27 Nov 2000 15:47:18   Frido  $ */

/*
Copyright (C) 1994-2000 The XFree86 Project, Inc.  All Rights Reserved.
Copyright (C) 2000 Silicon Motion, Inc.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the names of the XFree86 Project and
Silicon Motion shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization from the XFree86 Project and Silicon Motion.
*/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/siliconmotion/smi_dga.c,v 1.2 2001/02/15 18:20:33 dawes Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"
#include "xaa.h"
#include "xaalocal.h"

#include "smi.h"
#include "dgaproc.h"

static Bool SMI_OpenFramebuffer(ScrnInfoPtr, char **, unsigned char **, int *,
								int *, int *);
static Bool SMI_SetMode(ScrnInfoPtr, DGAModePtr);
static int  SMI_GetViewport(ScrnInfoPtr);
static void SMI_SetViewport(ScrnInfoPtr, int, int, int);
static void SMI_FillRect(ScrnInfoPtr, int, int, int, int, unsigned long);
static void SMI_BlitRect(ScrnInfoPtr, int, int, int, int, int, int);
static void SMI_BlitTransRect(ScrnInfoPtr, int, int, int, int, int, int,
							  unsigned long);

static
DGAFunctionRec SMI_DGAFuncs =
{
   SMI_OpenFramebuffer,
   NULL,
   SMI_SetMode,
   SMI_SetViewport,
   SMI_GetViewport,
   SMI_AccelSync,
   SMI_FillRect,
   SMI_BlitRect,
   SMI_BlitTransRect
};

Bool
SMI_DGAInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	SMIPtr pSmi = SMIPTR(pScrn);
	DGAModePtr modes = NULL, newmodes = NULL, currentMode;
	DisplayModePtr pMode, firstMode;
	int Bpp = pScrn->bitsPerPixel >> 3;
	int num = 0;
	Bool ret;

	ENTER_PROC("SMI_DGAInit");

	pMode = firstMode = pScrn->modes;

	while (pMode)
	{
		newmodes = xrealloc(modes, (num + 1) * sizeof(DGAModeRec));
		if (newmodes == NULL)
		{
			xfree(modes);
			LEAVE_PROC("SMI_DGAInit");
			return(FALSE);
		}

		modes = newmodes;

		currentMode = modes + num;
		num++;

		currentMode->mode  = pMode;
		currentMode->flags = DGA_PIXMAP_AVAILABLE;
		if (!pSmi->NoAccel)
		{
			currentMode->flags |= DGA_FILL_RECT
							   |  DGA_BLIT_RECT
							   |  DGA_BLIT_RECT_TRANS;
		}
		if (pMode->Flags & V_DBLSCAN)
		{
			currentMode->flags |= DGA_DOUBLESCAN;
		}
		if (pMode->Flags & V_INTERLACE)
		{
			currentMode->flags |= DGA_INTERLACED;
		}

		currentMode->byteOrder      = pScrn->imageByteOrder;
		currentMode->depth          = pScrn->depth;
		currentMode->bitsPerPixel   = pScrn->bitsPerPixel;
		currentMode->red_mask       = pScrn->mask.red;
		currentMode->green_mask     = pScrn->mask.green;
		currentMode->blue_mask      = pScrn->mask.blue;
		currentMode->visualClass    = (Bpp == 1) ? PseudoColor : TrueColor;
		currentMode->viewportWidth  = pMode->HDisplay;
		currentMode->viewportHeight = pMode->VDisplay;
		currentMode->xViewportStep  = (Bpp == 3) ? 8 : (8 / Bpp);
		currentMode->yViewportStep  = 1;
		currentMode->viewportFlags  = DGA_FLIP_RETRACE;
		currentMode->offset         = 0;
		currentMode->address        = pSmi->FBBase;

		xf86ErrorFVerb(VERBLEV, "\tSMI_DGAInit %dx%d @ %d bpp\n",
				currentMode->viewportWidth, currentMode->viewportHeight,
				currentMode->bitsPerPixel);

		currentMode->bytesPerScanline = ((pScrn->displayWidth * Bpp) + 15)
									  & ~15L;
		currentMode->imageWidth       = pScrn->displayWidth;
		currentMode->imageHeight      = pSmi->FBReserved
									  / currentMode->bytesPerScanline;
	    currentMode->pixmapWidth      = currentMode->imageWidth;
	    currentMode->pixmapHeight     = currentMode->imageHeight;
	    currentMode->maxViewportX     = currentMode->imageWidth
	    							  - currentMode->viewportWidth;
	    currentMode->maxViewportY     = currentMode->imageHeight
	    							  - currentMode->viewportHeight;

		pMode = pMode->next;
		if (pMode == firstMode)
		{
			break;
		}
	}

	pSmi->numDGAModes = num;
	pSmi->DGAModes    = modes;

	ret = DGAInit(pScreen, &SMI_DGAFuncs, modes, num);
	LEAVE_PROC("SMI_DGAInit");
	return(ret);
}

static Bool
SMI_SetMode(ScrnInfoPtr pScrn, DGAModePtr pMode)
{
	static int OldDisplayWidth[MAXSCREENS];
	int index = pScrn->pScreen->myNum;
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_SetMode");

	if (pMode == NULL)
	{	/* restore the original mode */

		/* put the ScreenParameters back */
		pScrn->displayWidth = OldDisplayWidth[index];

		SMI_SwitchMode(index, pScrn->currentMode, 0);
		pSmi->DGAactive = FALSE;
	}
	else
	{
		if (!pSmi->DGAactive)
		{	/* save the old parameters */
			OldDisplayWidth[index] = pScrn->displayWidth;

			pSmi->DGAactive = TRUE;
		}

		pScrn->displayWidth = pMode->bytesPerScanline
							/ (pMode->bitsPerPixel >> 3);

		SMI_SwitchMode(index, pMode->mode, 0);
	}

	LEAVE_PROC("SMI_SetMode");
	return(TRUE);
}


static int
SMI_GetViewport(ScrnInfoPtr pScrn)
{
    SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_GetViewport");

	LEAVE_PROC("SMI_GetViewport");

    return(pSmi->DGAViewportStatus);
}

static void
SMI_SetViewport(ScrnInfoPtr pScrn, int x, int y, int flags)
{
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_SetViewport");

	SMI_AdjustFrame(pScrn->pScreen->myNum, x, y, flags);
	pSmi->DGAViewportStatus = 0;

	LEAVE_PROC("SMI_SetViewport");
}

static void
SMI_FillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h, unsigned long color)
{
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_FillRect");

	if (pSmi->AccelInfoRec)
	{
		(*pSmi->AccelInfoRec->SetupForSolidFill)(pScrn, color, GXcopy, ~0);
		(*pSmi->AccelInfoRec->SubsequentSolidFillRect)(pScrn, x, y, w, h);
		SET_SYNC_FLAG(pSmi->AccelInfoRec);
	}

	LEAVE_PROC("SMI_FillRect");
}

static void
SMI_BlitRect(ScrnInfoPtr pScrn, int srcx, int srcy, int w, int h, int dstx,
			 int dsty)
{
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_BlitRect");

    if (pSmi->AccelInfoRec)
    {
		int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
		int ydir = (srcy < dsty) ? -1 : 1;

		(*pSmi->AccelInfoRec->SetupForScreenToScreenCopy)(pScrn, xdir, ydir,
														  GXcopy, ~0, -1);
		(*pSmi->AccelInfoRec->SubsequentScreenToScreenCopy)(pScrn, srcx, srcy,
															dstx, dsty, w, h);
		SET_SYNC_FLAG(pSmi->AccelInfoRec);
	}

	LEAVE_PROC("SMI_BlitRect");
}

static void
SMI_BlitTransRect(ScrnInfoPtr pScrn, int srcx, int srcy, int w, int h, int dstx,
				  int dsty, unsigned long color)
{
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_BlitTraneRect");

	if (pSmi->AccelInfoRec)
	{
		int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
		int ydir = (srcy < dsty) ? -1 : 1;

		(*pSmi->AccelInfoRec->SetupForScreenToScreenCopy)(pScrn, xdir, ydir,
														  GXcopy, ~0, color);
		(*pSmi->AccelInfoRec->SubsequentScreenToScreenCopy)(pScrn, srcx, srcy,
															dstx, dsty, w, h);
		SET_SYNC_FLAG(pSmi->AccelInfoRec);
	}

	LEAVE_PROC("SMI_BlitTraneRect");
}

static Bool
SMI_OpenFramebuffer(ScrnInfoPtr pScrn, char **name, unsigned char **mem,
					int *size, int *offset, int *flags)
{
	SMIPtr pSmi = SMIPTR(pScrn);

	ENTER_PROC("SMI_OpenFrameBuffer");

	*name   = NULL;	/* no special device */
	*mem    = (unsigned char*)pSmi->FBBase;
	*size   = pSmi->videoRAMBytes;
	*offset = 0;
	*flags  = DGA_NEED_ROOT;

	LEAVE_PROC("SMI_OpenFrameBuffer");
	return(TRUE);
}
