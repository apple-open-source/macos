/*
 *      Copyright 2001  Ani Joshi <ajoshi@unixbox.com>
 * 
 *      XFree86 4.x driver for S3 chipsets
 * 
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation and
 * that the name of Ani Joshi not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Ani Joshi makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *                 
 * ANI JOSHI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ANI JOSHI BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3/s3_dga.c,v 1.1 2001/07/02 10:46:04 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "dgaproc.h"

#include "s3.h"
#include "s3_reg.h"


static Bool S3_SetMode(ScrnInfoPtr pScrn, DGAModePtr pMode);
static int S3_GetViewport(ScrnInfoPtr pScrn);
static void S3_SetViewport(ScrnInfoPtr pScrn, int x, int y, int flags);
static void S3_FillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h,
		        unsigned long color);
static void S3_BltRect(ScrnInfoPtr pScrn, int srcx, int srcy, int w, int h,
		       int dstx, int dsty);
static Bool S3_OpenFramebuffer(ScrnInfoPtr pScrn, char **name,
			       unsigned char **mem, int *size, int *offset,
			       int *flags);
static void S3_Sync(ScrnInfoPtr pScrn);


static DGAFunctionRec S3_DGAFuncs = {
	S3_OpenFramebuffer,
	NULL,
	S3_SetMode,
	S3_SetViewport,
	S3_GetViewport,
	S3_Sync,
	S3_FillRect,
	S3_BltRect,
	NULL
};


static DGAModePtr S3SetupDGAMode(ScrnInfoPtr pScrn, DGAModePtr modes,
				 int *num, int bitsPerPixel, int depth,
				 Bool pixmap, int secondPitch,
				 unsigned long red, unsigned long green,
				 unsigned long blue, short visualClass)
{
	S3Ptr pS3 = S3PTR(pScrn);
	DGAModePtr newmodes = NULL, currentMode;
	DisplayModePtr pMode, firstMode;
	int otherPitch, Bpp = bitsPerPixel >> 3;
	Bool oneMore;

	pMode = firstMode = pScrn->modes;

	while (pMode) {
		otherPitch = secondPitch ? secondPitch : pMode->HDisplay;

		if (pMode->HDisplay != otherPitch) {
			newmodes = xrealloc(modes, (*num + 2) * sizeof(DGAModeRec));
			oneMore = TRUE;
		} else {
			newmodes = xrealloc(modes, (*num + 1) * sizeof(DGAModeRec));
			oneMore = FALSE;
		}

		if (!newmodes) {
			xfree(modes);
			return NULL;
		}

		modes = newmodes;

SECOND_PASS:

		currentMode = modes + *num;
		(*num)++;

		currentMode->mode = pMode;
		currentMode->flags = DGA_CONCURRENT_ACCESS;
		if (pixmap)
			currentMode->flags |= DGA_PIXMAP_AVAILABLE;
		if (pS3->pXAA)
			currentMode->flags |= DGA_FILL_RECT | DGA_BLIT_RECT;
		if (pMode->Flags & V_DBLSCAN)
			currentMode->flags |= DGA_DOUBLESCAN;
		if (pMode->Flags & V_INTERLACE)
			currentMode->flags |= DGA_INTERLACED;
		currentMode->byteOrder = pScrn->imageByteOrder;
		currentMode->depth = depth;
		currentMode->bitsPerPixel = bitsPerPixel;
		currentMode->red_mask = red;
		currentMode->green_mask = green;
		currentMode->blue_mask = blue;
		currentMode->visualClass = visualClass;
		currentMode->viewportWidth = pMode->HDisplay;
		currentMode->viewportHeight = pMode->VDisplay;
		currentMode->xViewportStep = 8;
		currentMode->yViewportStep = 1;
		currentMode->viewportFlags = DGA_FLIP_RETRACE;
		currentMode->offset = 0;
		currentMode->address = (unsigned char*)pS3->FBAddress;

		if (oneMore) {
			currentMode->bytesPerScanline = (((pMode->HDisplay * Bpp) + 3) & ~3L);

			currentMode->imageWidth = pMode->HDisplay;
			currentMode->imageHeight = pMode->VDisplay;
			currentMode->pixmapWidth = currentMode->imageWidth;
			currentMode->pixmapHeight = currentMode->imageHeight;
			currentMode->maxViewportX = currentMode->imageWidth -
						    currentMode->viewportWidth;
			currentMode->maxViewportY = currentMode->imageHeight -
						    currentMode->viewportHeight;

			oneMore = FALSE;
			goto SECOND_PASS;
		} else {
			currentMode->bytesPerScanline = (((otherPitch * Bpp) + 3) & ~3L);

			currentMode->imageWidth = otherPitch;
			currentMode->imageHeight = pMode->VDisplay;
			currentMode->pixmapWidth = currentMode->imageWidth;
			currentMode->pixmapHeight = currentMode->imageHeight;
			currentMode->maxViewportX = currentMode->imageWidth -
						    currentMode->viewportWidth;
			currentMode->maxViewportY = currentMode->imageHeight -
						    currentMode->viewportHeight;
			
		}

		pMode = pMode->next;

		if (pMode == firstMode)
			break;

	}

	return modes;
}



Bool S3DGAInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	S3Ptr pS3 = S3PTR(pScrn);
	DGAModePtr modes = NULL;
	int num = 0;

	modes = S3SetupDGAMode(pScrn, modes, &num, 8, 8,
			       (pScrn->bitsPerPixel == 8),
			       ((pScrn->bitsPerPixel != 8) ? 0 : pScrn->displayWidth),
			       0, 0, 0, PseudoColor);

	modes = S3SetupDGAMode(pScrn, modes, &num, 16, 15,
			       (pScrn->bitsPerPixel == 16),
			       ((pScrn->depth != 15)
			       ? 0 : pScrn->displayWidth),
			       0x7c00, 0x03e0, 0x001f, TrueColor);

	modes = S3SetupDGAMode(pScrn, modes, &num, 16, 15,
			       (pScrn->bitsPerPixel == 16),
			       ((pScrn->depth != 15)
			       ? 0 : pScrn->displayWidth),
			       0x7c00, 0x03e0, 0x001f, DirectColor);

	modes = S3SetupDGAMode(pScrn, modes, &num, 16, 16,
			       (pScrn->bitsPerPixel == 16),
			       ((pScrn->depth != 16)
			       ? 0 : pScrn->displayWidth),
			       0xf800, 0x07e0, 0x001f, TrueColor);

	modes = S3SetupDGAMode(pScrn, modes, &num, 16, 16,
			       (pScrn->bitsPerPixel == 16),
			       ((pScrn->depth != 16)
			       ? 0 : pScrn->displayWidth),
			       0xf800, 0x07e0, 0x001f, DirectColor);

	modes = S3SetupDGAMode(pScrn, modes, &num, 32, 24,
			       (pScrn->bitsPerPixel == 32),
			       ((pScrn->bitsPerPixel != 32)
			       ? 0 : pScrn->displayWidth),
			       0xff0000, 0x00ff00, 0x0000ff, TrueColor);

	modes = S3SetupDGAMode(pScrn, modes, &num, 32, 24,
			       (pScrn->bitsPerPixel == 32),
			       ((pScrn->bitsPerPixel != 32)
			       ? 0 : pScrn->displayWidth),
			       0xff0000, 0x00ff00, 0x0000ff, DirectColor);

	pS3->numDGAModes = num;
	pS3->DGAModes = modes;

	return DGAInit(pScreen, &S3_DGAFuncs, modes, num);
}


static Bool S3_SetMode(ScrnInfoPtr pScrn, DGAModePtr pMode)
{
	S3Ptr pS3 = S3PTR(pScrn);
	static S3FBLayout SavedLayouts[MAXSCREENS];
	int indx = pScrn->pScreen->myNum;

	if (!pMode) {
		if (pS3->DGAactive) {
			memcpy(&pS3->CurrentLayout, &SavedLayouts[indx],
			       sizeof(S3FBLayout));
			pS3->DGAactive = TRUE;
		}

		pS3->CurrentLayout.bitsPerPixel = pMode->bitsPerPixel;
		pS3->CurrentLayout.depth = pMode->depth;
		pS3->CurrentLayout.displayWidth = (pMode->bytesPerScanline /
						   (pMode->bitsPerPixel >> 3));
		pS3->CurrentLayout.pixel_bytes = pMode->bitsPerPixel / 8;
		pS3->CurrentLayout.pixel_code = (pMode->bitsPerPixel != 16 ?
						 pMode->bitsPerPixel :
						 pMode->depth);

		S3SwitchMode(indx, pMode->mode, 0);
	}

	return TRUE;
}


static int S3_GetViewport(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);

	return pS3->DGAViewportStatus;
}


static void S3_SetViewport(ScrnInfoPtr pScrn, int x, int y, int flags)
{
	S3Ptr pS3 = S3PTR(pScrn);

	pScrn->AdjustFrame(pScrn->pScreen->myNum, x, y, flags);
	pS3->DGAViewportStatus = 0;
}


static void S3_FillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h,
		        unsigned long color)
{
	S3Ptr pS3 = S3PTR(pScrn);

	if (pS3->pXAA) {
		(*pS3->pXAA->SetupForSolidFill)(pScrn, color, GXcopy, (CARD32)(~0));
		(*pS3->pXAA->SubsequentSolidFillRect)(pScrn, x, y, w, h);
		SET_SYNC_FLAG(pS3->pXAA);
	}
}


static void S3_BltRect(ScrnInfoPtr pScrn, int srcx, int srcy, int w, int h,
		       int dstx, int dsty)
{
	S3Ptr pS3 = S3PTR(pScrn);

	if (pS3->pXAA) {
		int xdir = ((srcx < dstx) && (srcy == dsty)) ? -1 : 1;
		int ydir = (srcy < dsty) ? -1 : 1;

		(*pS3->pXAA->SetupForScreenToScreenCopy)(pScrn, xdir, ydir,
							 GXcopy, (CARD32)(~0), -1);
		(*pS3->pXAA->SubsequentScreenToScreenCopy)(pScrn, srcx, srcy,
							   dstx, dsty, w, h);
		SET_SYNC_FLAG(pS3->pXAA);
	}
}


static Bool S3_OpenFramebuffer(ScrnInfoPtr pScrn, char **name,
			       unsigned char **mem, int *size, int *offset,
			       int *flags)
{
	S3Ptr pS3 = S3PTR(pScrn);

	*name = NULL;
	*mem = (unsigned char*)pS3->FBAddress;
	*size = (pScrn->videoRam * 1024);
	*offset = 0;
	*flags = 0;

	return TRUE;
}


static void S3_Sync(ScrnInfoPtr pScrn)
{
	WaitIdle();
}
