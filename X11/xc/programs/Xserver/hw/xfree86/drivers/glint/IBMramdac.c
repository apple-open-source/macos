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
 * glintOutIBMRGBIndReg() and glintInIBMRGBIndReg() are used to access 
 * the indirect IBM RAMDAC registers only.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/IBMramdac.c,v 1.6 1999/02/12 22:52:02 hohndel Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "IBM.h"
#include "glint_regs.h"
#include "glint.h"

#define IBMRGB_WRITE_ADDR           0x4000  
#define IBMRGB_RAMDAC_DATA          0x4008 
#define IBMRGB_PIXEL_MASK           0x4010 
#define IBMRGB_READ_ADDR            0x4018  
#define IBMRGB_INDEX_LOW            0x4020 
#define IBMRGB_INDEX_HIGH           0x4028 
#define IBMRGB_INDEX_DATA           0x4030  
#define IBMRGB_INDEX_CONTROL        0x4038 

void
glintOutIBMRGBIndReg(ScrnInfoPtr pScrn,
		     CARD32 reg, unsigned char mask, unsigned char data)
{
  GLINTPtr pGlint = GLINTPTR(pScrn);
  unsigned char tmp = 0x00;

  GLINT_SLOW_WRITE_REG((reg>>8) & 0xff, IBMRGB_INDEX_HIGH);
  GLINT_SLOW_WRITE_REG (reg & 0xFF, IBMRGB_INDEX_LOW);

  if (mask != 0x00)
    tmp = GLINT_READ_REG (IBMRGB_INDEX_DATA) & mask;

  GLINT_SLOW_WRITE_REG (tmp | data, IBMRGB_INDEX_DATA);
}

unsigned char
glintInIBMRGBIndReg (ScrnInfoPtr pScrn, CARD32 reg)
{
  GLINTPtr pGlint = GLINTPTR(pScrn);
  unsigned char ret;

  GLINT_SLOW_WRITE_REG(reg & 0xFF, IBMRGB_INDEX_LOW);
  GLINT_SLOW_WRITE_REG((reg>>8) & 0xff, IBMRGB_INDEX_HIGH);
  ret = GLINT_READ_REG(IBMRGB_INDEX_DATA);
  return (ret);
}

void
glintIBMWriteAddress (ScrnInfoPtr pScrn, CARD32 index)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
    GLINT_SLOW_WRITE_REG(index, IBMRGB_WRITE_ADDR);
}

void
glintIBMWriteData (ScrnInfoPtr pScrn, unsigned char data)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
    GLINT_SLOW_WRITE_REG(data, IBMRGB_RAMDAC_DATA);
}

void
glintIBMReadAddress (ScrnInfoPtr pScrn, CARD32 index)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
    GLINT_SLOW_WRITE_REG(0xFF, IBMRGB_PIXEL_MASK);
    GLINT_SLOW_WRITE_REG(index, IBMRGB_READ_ADDR);
}

unsigned char
glintIBMReadData (ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
    return(GLINT_READ_REG(IBMRGB_RAMDAC_DATA));
}

Bool 
glintIBMHWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    xf86CursorInfoPtr infoPtr;

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pGlint->CursorInfoRec = infoPtr;

    (*pGlint->RamDac->HWCursorInit)(infoPtr);

    return(xf86InitCursor(pScreen, infoPtr));
}
