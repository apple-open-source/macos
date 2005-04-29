/*
 * Copyright 1998 by Alan Hourihane, Wigan, England.
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
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *
 * Permedia2OutIndReg() and Permedia2InIndReg() are used to access 
 * the indirect Permedia2 RAMDAC registers only.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/pm2ramdac.c,v 1.12 2003/10/08 15:48:40 eich Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "glint_regs.h"
#include "glint.h"

void
Permedia2OutIndReg(ScrnInfoPtr pScrn,
		     CARD32 reg, unsigned char mask, unsigned char data)
{
  GLINTPtr pGlint = GLINTPTR(pScrn);
  unsigned char tmp = 0x00;

  GLINT_SLOW_WRITE_REG (reg, PM2DACIndexReg);

  if (mask != 0x00)
    tmp = GLINT_READ_REG (PM2DACIndexData) & mask;

  GLINT_SLOW_WRITE_REG (tmp | data, PM2DACIndexData);
}

unsigned char
Permedia2InIndReg (ScrnInfoPtr pScrn, CARD32 reg)
{
  GLINTPtr pGlint = GLINTPTR(pScrn);
  unsigned char ret;

  GLINT_SLOW_WRITE_REG (reg, PM2DACIndexReg);
  GLINTDACDelay(5);
  ret = GLINT_READ_REG (PM2DACIndexData);

  return (ret);
}

void
Permedia2WriteAddress (ScrnInfoPtr pScrn, CARD32 index)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
    GLINT_SLOW_WRITE_REG(index, PM2DACWriteAddress);
}

void
Permedia2WriteData (ScrnInfoPtr pScrn, unsigned char data)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    GLINT_SLOW_WRITE_REG(data, PM2DACData);
}

void
Permedia2ReadAddress (ScrnInfoPtr pScrn, CARD32 index)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
    GLINT_SLOW_WRITE_REG(0xFF, PM2DACReadMask);
    GLINT_SLOW_WRITE_REG(index, PM2DACReadAddress);
}

unsigned char
Permedia2ReadData (ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINTDACDelay(5);
    return(GLINT_READ_REG(PM2DACData));
}

void Permedia2LoadPalette(
    ScrnInfoPtr pScrn, 
    int numColors, 
    int *indices,
    LOCO *colors,
    VisualPtr pVisual
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int i, index, shift = 0, j, repeat = 1;

    if (pScrn->depth == 15) {
	repeat = 8;
	shift = 3;
    }

    for(i = 0; i < numColors; i++) {
	index = indices[i];
	for (j = 0; j < repeat; j++) {
	    Permedia2WriteAddress(pScrn, (index << shift)+j);
	    Permedia2WriteData(pScrn, colors[index].red);
	    Permedia2WriteData(pScrn, colors[index].green);
	    Permedia2WriteData(pScrn, colors[index].blue);
	}
	/* for video i/o */
        GLINT_SLOW_WRITE_REG(index, TexelLUTIndex);
	GLINT_SLOW_WRITE_REG((colors[index].red & 0xFF) |
			     ((colors[index].green & 0xFF) << 8) |
			     ((colors[index].blue & 0xFF) << 16),
			     TexelLUTData);
    }
}

/* special one for 565 mode */
void Permedia2LoadPalette16(
    ScrnInfoPtr pScrn, 
    int numColors, 
    int *indices,
    LOCO *colors,
    VisualPtr pVisual
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int i, index, j;

    for(i = 0; i < numColors; i++) {
	index = indices[i];
	for (j = 0; j < 4; j++) {
	    Permedia2WriteAddress(pScrn, (index << 2)+j);
	    Permedia2WriteData(pScrn, colors[index >> 1].red);
	    Permedia2WriteData(pScrn, colors[index].green);
	    Permedia2WriteData(pScrn, colors[index >> 1].blue);
	}
        GLINT_SLOW_WRITE_REG(index, TexelLUTIndex);
	GLINT_SLOW_WRITE_REG((colors[index].red & 0xFF) |
			     ((colors[index].green & 0xFF) << 8) |
			     ((colors[index].blue & 0xFF) << 16),
			     TexelLUTData);

	if(index <= 31) {
	    for (j = 0; j < 4; j++) {
	    	Permedia2WriteAddress(pScrn, (index << 3)+j);
	    	Permedia2WriteData(pScrn, colors[index].red);
	    	Permedia2WriteData(pScrn, colors[(index << 1) + 1].green);
	    	Permedia2WriteData(pScrn, colors[index].blue);
	    }
	}
    }
}
