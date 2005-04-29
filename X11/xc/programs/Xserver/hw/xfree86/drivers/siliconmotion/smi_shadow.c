/* Header:   //Mercury/Projects/archives/XFree86/4.0/smi_shadow.c-arc   1.10   30 Nov 2000 11:40:38   Frido  $ */

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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/siliconmotion/smi_shadow.c,v 1.3 2003/10/08 11:13:01 eich Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "shadowfb.h"
#include "servermd.h"

#include "smi.h"

/******************************************************************************\
|* SMI_RefreshArea
|*=============================================================================
|*
|*  PARAMETERS:		pScrn	Pointer to ScrnInfo structure.
|*					num		Number of boxes to refresh.
|*					pbox	Pointer to an array of boxes to refresh.
|*
|*  DESCRIPTION:	Refresh a portion of the shadow buffer to the visual screen
|*					buffer.  This is mainly used for rotation purposes.
|*												y
|*  RETURNS:		Nothing.
|*
\******************************************************************************/
void SMI_RefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	int width, height, srcX, srcY, destX, destY;

	ENTER_PROC("SMI_RefreshArea");

	/* #671 */
	if (pSmi->polyLines)
	{
		pSmi->polyLines = FALSE;
		return;
	}

	if (pSmi->rotate)
	{
		/* IF we need to do rotation, setup the hardware here. */
		WaitIdleEmpty();
		WRITE_DPR(pSmi, 0x10, pSmi->ShadowPitch);
		WRITE_DPR(pSmi, 0x3C, pSmi->ShadowPitch);
		WRITE_DPR(pSmi, 0x44, pSmi->FBOffset >> 3);
	}

	/* #672 */
	if (pSmi->ClipTurnedOn)
	{
		WaitQueue(1);
		WRITE_DPR(pSmi, 0x2C, pSmi->ScissorsLeft);
		pSmi->ClipTurnedOn = FALSE;
	}

	while (num--)
	{
		/* Get coordinates of the box to refresh. */
		srcX   = pbox->x1;
		srcY   = pbox->y1;
		width  = pbox->x2 - srcX;
		height = pbox->y2 - srcY;

		DEBUG((VERBLEV, "x=%d y=%d w=%d h=%d\n", srcX, srcY, width, height));

		if ((width > 0) && (height > 0))
		{
			switch (pSmi->rotate)
			{
				case SMI_ROTATE_CW:
					/* 90 degrees CW rotation.  Calculate destination
					   coordinates:

						*---+
						|   |       +-----*
						|   |       |     |  destX = shadowHeight - srcY - 1
						|   |  -->  |     |  destY = srcX
						|   |       |     |
						|   |       +-----+
						+---+
					*/
					destX = pSmi->ShadowHeight - srcY - 1;
					destY = srcX;

					WaitQueue(4);
					WRITE_DPR(pSmi, 0x00, (srcX << 16)   + srcY);
					WRITE_DPR(pSmi, 0x04, (destX << 16)  + destY);
					WRITE_DPR(pSmi, 0x08, (width << 16)  + height);
					WRITE_DPR(pSmi, 0x0C, 0xCC | SMI_ROTATE_BLT |
							SMI_ROTATE_CW | SMI_START_ENGINE);
					break;

				case SMI_ROTATE_CCW:
					/* 90 degrees CCW rotatation.  Calculate destination
					   coordinates:

						*---+
						|   |       +-----+
						|   |       |     |  destX = srcY
						|   |  -->  |     |  destY = shadowWidth - srcX - 1
						|   |       |     |
						|   |       *-----+
						+---+
					*/
					destX = srcY;
					destY = pSmi->ShadowWidth - srcX - 1;

					WaitQueue(4);
					WRITE_DPR(pSmi, 0x00, (srcX << 16)   + srcY);
					WRITE_DPR(pSmi, 0x04, (destX << 16)  + destY);
					WRITE_DPR(pSmi, 0x08, (width << 16)  + height);
					WRITE_DPR(pSmi, 0x0C, 0xCC | SMI_ROTATE_BLT |
							SMI_ROTATE_CCW | SMI_START_ENGINE);
					break;

				default:
					/* No rotation, perform a normal copy. */
					if (pScrn->bitsPerPixel == 24)
					{
						srcX  *= 3;
						width *= 3;

						if (pSmi->Chipset == SMI_LYNX)
						{
							srcY *= 3;
						}
					}

					WaitQueue(4);
					WRITE_DPR(pSmi, 0x00, (srcX << 16)  + srcY);
					WRITE_DPR(pSmi, 0x04, (srcX << 16)  + srcY);
					WRITE_DPR(pSmi, 0x08, (width << 16) + height);
					WRITE_DPR(pSmi, 0x0C, SMI_BITBLT + SMI_START_ENGINE + 0xCC);
					break;
			}
		}

		pbox++;
	}

	if (pSmi->rotate)
	{
		/* If we did a rotation, we need to restore the hardware state here. */
		WaitIdleEmpty();
		WRITE_DPR(pSmi, 0x10, (pSmi->Stride << 16) | pSmi->Stride);
		WRITE_DPR(pSmi, 0x3C, (pSmi->Stride << 16) | pSmi->Stride);
		WRITE_DPR(pSmi, 0x44, 0);
	}

	LEAVE_PROC("SMI_RefreshArea");
}

/* Custom version for the 730 series (Cougar3DR).
   This chipset has problems with large rotate-blts. */

void SMI_RefreshArea730(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	int width, height, srcX, srcY, destX, destY;
	int maxPixels, tempWidth;

	ENTER_PROC("SMI_RefreshArea730");

	/* #671 */
	if (pSmi->polyLines)
	{
		pSmi->polyLines = FALSE;
		return;
	}

	if (pSmi->rotate)
	{
		/* IF we need to do rotation, setup the hardware here. */
		WaitIdleEmpty();
		WRITE_DPR(pSmi, 0x10, pSmi->ShadowPitch);
		WRITE_DPR(pSmi, 0x3C, pSmi->ShadowPitch);
		WRITE_DPR(pSmi, 0x44, pSmi->FBOffset >> 3);
	}

	/* #672 */
	if (pSmi->ClipTurnedOn)
	{
		WaitQueue(1);
		WRITE_DPR(pSmi, 0x2C, pSmi->ScissorsLeft);
		pSmi->ClipTurnedOn = FALSE;
	}

	/* SM731 cannot rotate-blt more than a certain number of pixels
	   (based on a calculation from the Windows driver source */
	maxPixels = 1280 / pScrn->bitsPerPixel;

	while (num--)
	{
		/* Get coordinates of the box to refresh. */
		srcX   = pbox->x1;
		srcY   = pbox->y1;
		width  = pbox->x2 - srcX;
		height = pbox->y2 - srcY;

		DEBUG((VERBLEV, "x=%d y=%d w=%d h=%d\n", srcX, srcY, width, height));

		if ((width > 0) && (height > 0))
		{
			switch (pSmi->rotate)
			{
				case SMI_ROTATE_CW:
					/* 90 degrees CW rotation.  Calculate destination
					   coordinates:

						*---+
						|   |       +-----*
						|   |       |     |  destX = shadowHeight - srcY - 1
						|   |  -->  |     |  destY = srcX
						|   |       |     |
						|   |       +-----+
						+---+
					*/
					destX = pSmi->ShadowHeight - srcY - 1;
					destY = srcX;

					for (tempWidth=width; tempWidth>0;)
					{
						if (width>maxPixels)
							width = maxPixels;
						WaitQueue(4);
						WRITE_DPR(pSmi, 0x00, (srcX << 16)   + srcY);
						WRITE_DPR(pSmi, 0x04, (destX << 16)  + destY);
						WRITE_DPR(pSmi, 0x08, (width << 16)  + height);
						WRITE_DPR(pSmi, 0x0C, 0xCC | SMI_ROTATE_BLT |
								SMI_ROTATE_CW | SMI_START_ENGINE);
						destY 	  += maxPixels;
						srcX  	  += maxPixels;
						tempWidth -= maxPixels;
						width      = tempWidth;
					}

					break;

				case SMI_ROTATE_CCW:
					/* 90 degrees CCW rotatation.  Calculate destination
					   coordinates:

						*---+
						|   |       +-----+
						|   |       |     |  destX = srcY
						|   |  -->  |     |  destY = shadowWidth - srcX - 1
						|   |       |     |
						|   |       *-----+
						+---+
					*/
					destX = srcY;
					destY = pSmi->ShadowWidth - srcX - 1;

					for (tempWidth=width; tempWidth>0;)
					{
						if (width>maxPixels)
							width = maxPixels;
						WaitQueue(4);
						WRITE_DPR(pSmi, 0x00, (srcX << 16)   + srcY);
						WRITE_DPR(pSmi, 0x04, (destX << 16)  + destY);
						WRITE_DPR(pSmi, 0x08, (width << 16)  + height);
						WRITE_DPR(pSmi, 0x0C, 0xCC | SMI_ROTATE_BLT |
								SMI_ROTATE_CCW | SMI_START_ENGINE);
						destY 	  -= maxPixels;
						srcX  	  += maxPixels;
						tempWidth -= maxPixels;
						width      = tempWidth;
					}

					break;

				default:
					/* No rotation, perform a normal copy. */
					if (pScrn->bitsPerPixel == 24)
					{
						srcX  *= 3;
						width *= 3;

						if (pSmi->Chipset == SMI_LYNX)
						{
							srcY *= 3;
						}
					}

					WaitQueue(4);
					WRITE_DPR(pSmi, 0x00, (srcX << 16)  + srcY);
					WRITE_DPR(pSmi, 0x04, (srcX << 16)  + srcY);
					WRITE_DPR(pSmi, 0x08, (width << 16) + height);
					WRITE_DPR(pSmi, 0x0C, SMI_BITBLT + SMI_START_ENGINE + 0xCC);
					break;
			}
		}

		pbox++;
	}

	if (pSmi->rotate)
	{
		/* If we did a rotation, we need to restore the hardware state here. */
		WaitIdleEmpty();
		WRITE_DPR(pSmi, 0x10, (pSmi->Stride << 16) | pSmi->Stride);
		WRITE_DPR(pSmi, 0x3C, (pSmi->Stride << 16) | pSmi->Stride);
		WRITE_DPR(pSmi, 0x44, 0);
	}

	LEAVE_PROC("SMI_RefreshArea730");
}

/******************************************************************************\
|* SMI_PointerMoved
|*=============================================================================
|*
|*  PARAMETERS:		index	Index of current screen.
|*					x		X location of pointer.
|*					y		Y location of pointer.
|*
|*  DESCRIPTION:	Adjust the pointer location if we are in rotation mode.
|*
|*  RETURNS:		Nothing.
|*
\******************************************************************************/
void SMI_PointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    SMIPtr pSmi = SMIPTR(pScrn);
    int newX, newY;

	switch (pSmi->rotate)
	{
		case SMI_ROTATE_CW:
			/* 90 degrees CW rotation. */
			newX = pScrn->pScreen->height - y - 1;
			newY = x;
			break;

		case SMI_ROTATE_CCW:
			/* 90 degrees CCW rotation. */
			newX = y;
			newY = pScrn->pScreen->width - x - 1;
			break;

		default:
			/* No rotation. */
			newX = x;
			newY = y;
			break;
    }

	/* Pass adjusted pointer coordinates original PointerMoved function. */
    (*pSmi->PointerMoved)(index, newX, newY);
}
