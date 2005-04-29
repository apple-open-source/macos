/* Header:   //Mercury/Projects/archives/XFree86/4.0/smi_hwcurs.c-arc   1.12   27 Nov 2000 15:47:48   Frido  $ */

/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/siliconmotion/smi_hwcurs.c,v 1.3 2003/10/08 11:13:01 eich Exp $ */

#include "cursorstr.h"
#include "smi.h"

#define MAX_CURSOR	32

static unsigned char *
SMI_RealizeCursor(xf86CursorInfoPtr infoPtr, CursorPtr pCurs)
{
	SMIPtr pSmi = SMIPTR(infoPtr->pScrn);
	CursorBitsPtr bits = pCurs->bits;
	unsigned char * ram;
	unsigned char * psource = bits->source;
	unsigned char * pmask = bits->mask;
	int x, y, srcwidth, i;

	ENTER_PROC("SMI_RealizeCursor");

	/* Allocate memory */
	ram = (unsigned char *) xcalloc(1, 1024);
	if (ram == NULL)
	{
		LEAVE_PROC("SMI_RealizeCursor");
		return(NULL);
	}

	/* Calculate cursor information */
	srcwidth = ((bits->width + 31) / 8) & ~3;
	i = 0;

	switch (pSmi->rotate)
	{
		default:
			/* Copy cursor image */
			for (y = 0; y < min(MAX_CURSOR, bits->height); y++)
			{
				for (x = 0; x < min(MAX_CURSOR / 8, srcwidth); x++)
				{
					unsigned char mask   = byte_reversed[*pmask++];
					unsigned char source = byte_reversed[*psource++] & mask;

					ram[i++] = ~mask;
					ram[i++] = source;
					if (i & 4) i += 4;
				}

				pmask   += srcwidth - x;
				psource += srcwidth - x;

				/* Fill remaining part of line with no shape */
				for (; x < MAX_CURSOR / 8; x++)
				{
					ram[i++] = 0xFF;
					ram[i++] = 0x00;
					if (i & 4) i += 4;
				}
			}

			/* Fill remaining part of memory with no shape */
			for (; y < MAX_CURSOR; y++)
			{
				for (x = 0; x < MAX_CURSOR / 8; x++)
				{
					ram[i++] = 0xFF;
					ram[i++] = 0x00;
					if (i & 4) i += 4;
				}
			}
			break;

		case SMI_ROTATE_CW:
			/* Initialize cursor memory */
			for (i = 0; i < 1024;)
			{
				ram[i++] = 0xFF;
				ram[i++] = 0x00;
				if (i & 4) i += 4;
			}

			/* Rotate cursor image */
			for (y = 0; y < min(MAX_CURSOR, bits->height); y++)
			{
				unsigned char bitmask = 0x01 << (y & 7);
				int           index   = ((MAX_CURSOR - y - 1) / 8) * 2;
				if (index & 4) index += 4;

				for (x = 0; x < min(MAX_CURSOR / 8, srcwidth); x++)
				{
					unsigned char mask   = *pmask++;
					unsigned char source = *psource++ & mask;

					i = index + (x * 8) * 16;
					if (mask || (source & mask))
					{
						unsigned char bit;
						for (bit = 0x01; bit; bit <<= 1)
						{
							if (mask & bit)
							{
								ram[i + 0] &= ~bitmask;
							}

							if (source & bit)
							{
								ram[i + 1] |= bitmask;
							}

							i += 16;
						}
					}
				}

				pmask   += srcwidth - x;
				psource += srcwidth - x;
			}
			break;

		case SMI_ROTATE_CCW:
			/* Initialize cursor memory */
			for (i = 0; i < 1024;)
			{
				ram[i++] = 0xFF;
				ram[i++] = 0x00;
				if (i & 4) i += 4;
			}

			/* Rotate cursor image */
			for (y = 0; y < min(MAX_CURSOR, bits->height); y++)
			{
				unsigned char bitmask = 0x80 >> (y & 7);
				int			  index	  = (y >> 3) * 2;
				if (index & 4) index += 4;

				for (x = 0; x < min(MAX_CURSOR / 8, srcwidth); x++)
				{
					unsigned char mask   = *pmask++;
					unsigned char source = *psource++ & mask;

					i = index + (MAX_CURSOR - x * 8 - 1) * 16;
					if (mask || (source & mask))
					{
						unsigned char bit;
						for (bit = 0x01; bit; bit <<= 1)
						{
							if (mask & bit)
							{
								ram[i + 0] &= ~bitmask;
							}

							if (source & bit)
							{
								ram[i + 1] |= bitmask;
							}

							i -= 16;
						}
					}
				}

				pmask   += srcwidth - x;
				psource += srcwidth - x;
			}
			break;
	}

	LEAVE_PROC("SMI_RealizeCursor");
	return(ram);
}

static void
SMI_LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	CARD8 tmp;

	ENTER_PROC("SMI_LoadCursorImage");

    /* Load storage location. */
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x80,
			pSmi->FBCursorOffset / 2048);
	tmp = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81) & 0x80;
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81,
			tmp | ((pSmi->FBCursorOffset / 2048) >> 8));

    /* Program FPR copy when on the 730 */
    if (pSmi->Chipset == SMI_COUGAR3DR)
    {
        CARD32 fpr15c;

        /* put address in upper word, and disable the cursor */
        fpr15c  = READ_FPR(pSmi, FPR15C) & FPR15C_MASK_HWCCOLORS;
        fpr15c |= (pSmi->FBCursorOffset / 2048) << 16;
        WRITE_FPR(pSmi, FPR15C, fpr15c);
    }

	/* Copy cursor image to framebuffer storage */
	memcpy(pSmi->FBBase + pSmi->FBCursorOffset, src, 1024);

	LEAVE_PROC("SMI_LoadCursorImage");
}

static void
SMI_ShowCursor(ScrnInfoPtr pScrn)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	char tmp;

	ENTER_PROC("SMI_ShowCursor");

	/* Show cursor */
	tmp = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81, tmp | 0x80);

    /* Program FPR copy when on the 730 */
    if (pSmi->Chipset == SMI_COUGAR3DR)
    {
        CARD32 fpr15c;

        /* turn on the top bit */
        fpr15c  = READ_FPR(pSmi, FPR15C);
        fpr15c |= FPR15C_MASK_HWCENABLE;
        WRITE_FPR(pSmi, FPR15C, fpr15c);
    }

	LEAVE_PROC("SMI_ShowCursor");
}

static void
SMI_HideCursor(ScrnInfoPtr pScrn)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	char tmp;

	ENTER_PROC("SMI_HideCursor");

	/* Hide cursor */
	tmp = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81, tmp & ~0x80);

    /* Program FPR copy when on the 730 */
    if (pSmi->Chipset == SMI_COUGAR3DR)
    {
        CARD32 fpr15c;

        /* turn off the top bit */
        fpr15c  = READ_FPR(pSmi, FPR15C);
        fpr15c &= ~FPR15C_MASK_HWCENABLE;
        WRITE_FPR(pSmi, FPR15C, fpr15c);
    }

	LEAVE_PROC("SMI_HideCursor");
}

static void
SMI_SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	int xoff, yoff;

	ENTER_PROC("SMI_SetCursorPosition");

	/* Calculate coordinates for rotation */
	switch (pSmi->rotate)
	{
		default:
			xoff = x;
			yoff = y;
			break;

		case SMI_ROTATE_CW:
			xoff = pSmi->ShadowHeight - y - MAX_CURSOR;
			yoff = x;
			break;

		case SMI_ROTATE_CCW:
			xoff = y;
			yoff = pSmi->ShadowWidth - x - MAX_CURSOR;
			break;
	}

	/* Program coordinates */
	if (xoff >= 0)
	{
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x88, xoff & 0xFF);
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x89,
				(xoff >> 8) & 0x07);
	}
	else
	{
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x88,
				(-xoff) & (MAX_CURSOR - 1));
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x89, 0x08);
	}

	if (yoff >= 0)
	{
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x8A, yoff & 0xFF);
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x8B,
				(yoff >> 8) & 0x07);
	}
	else
	{
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x8A,
				(-yoff) & (MAX_CURSOR - 1));
		VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x8B, 0x08);
	}

    /* Program FPR copy when on the 730 */
    if (pSmi->Chipset == SMI_COUGAR3DR)
    {
        CARD32 fpr158;

        if (xoff >= 0)
        {
            fpr158 = (xoff & FPR158_MASK_MAXBITS)<<16;
        }
        else
        {
            fpr158 = (((-xoff) & FPR158_MASK_MAXBITS) | FPR158_MASK_BOUNDARY)<<16;
        }

        if (yoff >= 0)
        {
            fpr158 |= (yoff & FPR158_MASK_MAXBITS);
        }
        else
        {
            fpr158 |= (((-yoff) & FPR158_MASK_MAXBITS) | FPR158_MASK_BOUNDARY);
        }

        /* Program combined coordinates */
        WRITE_FPR(pSmi, FPR158, fpr158);

    }

	LEAVE_PROC("SMI_SetCursorPosition");
}

static void
SMI_SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
	SMIPtr pSmi = SMIPTR(pScrn);
	unsigned char packedFG, packedBG;

	ENTER_PROC("SMI_SetCursorColors");

	/* Pack the true color into 8 bit */
	packedFG = (fg & 0xE00000) >> 16
			 | (fg & 0x00E000) >> 11
			 | (fg & 0x0000C0) >> 6
			 ;
	packedBG = (bg & 0xE00000) >> 16
			 | (bg & 0x00E000) >> 11
			 | (bg & 0x0000C0) >> 6
			 ;

	/* Program the colors */
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x8C, packedFG);
	VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x8D, packedBG);

    /* Program FPR copy when on the 730 */
    if (pSmi->Chipset == SMI_COUGAR3DR)
    {
        CARD32 fpr15c;

        fpr15c  = READ_FPR(pSmi, FPR15C) & FPR15C_MASK_HWCADDREN;
        fpr15c |= packedFG;
        fpr15c |= packedBG<<8;
        WRITE_FPR(pSmi, FPR15C, fpr15c);
    }

	LEAVE_PROC("SMI_SetCursorColors");
}

Bool
SMI_HWCursorInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	SMIPtr pSmi = SMIPTR(pScrn);
	xf86CursorInfoPtr infoPtr;
	Bool ret;

	ENTER_PROC("SMI_HWCursorInit");

	/* Create cursor infor record */
	infoPtr = xf86CreateCursorInfoRec();
	if (infoPtr == NULL)
	{
		LEAVE_PROC("SMI_HWCursorInit");
		return(FALSE);
	}

    pSmi->CursorInfoRec = infoPtr;

	/* Fill in the information */
    infoPtr->MaxWidth  = MAX_CURSOR;
    infoPtr->MaxHeight = MAX_CURSOR;
    infoPtr->Flags	   = HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_8
					   | HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK
					   | HARDWARE_CURSOR_AND_SOURCE_WITH_MASK
					   | HARDWARE_CURSOR_BIT_ORDER_MSBFIRST
					   | HARDWARE_CURSOR_TRUECOLOR_AT_8BPP
					   | HARDWARE_CURSOR_INVERT_MASK;

    infoPtr->SetCursorColors   = SMI_SetCursorColors;
    infoPtr->SetCursorPosition = SMI_SetCursorPosition;
    infoPtr->LoadCursorImage   = SMI_LoadCursorImage;
    infoPtr->HideCursor        = SMI_HideCursor;
    infoPtr->ShowCursor        = SMI_ShowCursor;
	infoPtr->RealizeCursor	   = SMI_RealizeCursor;
    infoPtr->UseHWCursor       = NULL;

	/* Proceed with cursor initialization */
    ret = xf86InitCursor(pScreen, infoPtr);

	LEAVE_PROC("SMI_HWCursorInit");
	return(ret);
}
