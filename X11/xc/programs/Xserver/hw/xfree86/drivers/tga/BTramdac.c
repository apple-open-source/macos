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
 * tgaBTOutIndReg() and tgaBTInIndReg() are used to access 
 * the indirect TGA BT RAMDAC registers only.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/BTramdac.c,v 1.5 2000/10/20 12:57:26 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "tga_regs.h"
#include "BT.h"
#include "tga.h"

void
tgaBTOutIndReg(ScrnInfoPtr pScrn,
		     CARD32 reg, unsigned char mask, unsigned char data)
{
  TGAPtr pTga;
  unsigned char tmp = 0x00;

  pTga = TGAPTR(pScrn);
  
  TGA_WRITE_REG(reg << 1 | BT485_READ_BIT, TGA_RAMDAC_SETUP_REG);

  if (mask != 0x00)
    tmp = (TGA_READ_REG(TGA_RAMDAC_REG)>>16) & mask;

  TGA_WRITE_REG(reg << 1 | BT485_WRITE_BIT, TGA_RAMDAC_SETUP_REG);

  TGA_WRITE_REG ((tmp | data) | (reg<<9), TGA_RAMDAC_REG);
}

unsigned char
tgaBTInIndReg (ScrnInfoPtr pScrn, CARD32 reg)
{
  TGAPtr pTga;
  unsigned char ret;

  pTga  = TGAPTR(pScrn);

  TGA_WRITE_REG(reg << 1 | BT485_READ_BIT, TGA_RAMDAC_SETUP_REG);
  ret = TGA_READ_REG (TGA_RAMDAC_REG)>>16;

  return (ret);
}

void
tgaBTWriteAddress (ScrnInfoPtr pScrn, CARD32 index)
{
    TGAPtr pTga;

    pTga = TGAPTR(pScrn);
    
    TGA_WRITE_REG(BT_WRITE_ADDR << 1 | BT485_WRITE_BIT, TGA_RAMDAC_SETUP_REG);
    TGA_WRITE_REG(index | (BT_WRITE_ADDR<<9), TGA_RAMDAC_REG);
    TGA_WRITE_REG(BT_RAMDAC_DATA << 1 | BT485_WRITE_BIT, TGA_RAMDAC_SETUP_REG);
}

void
tgaBTWriteData (ScrnInfoPtr pScrn, unsigned char data)
{
    TGAPtr pTga;

    pTga = TGAPTR(pScrn);
    
    TGA_WRITE_REG(data | (BT_RAMDAC_DATA << 9), TGA_RAMDAC_REG);
}

void
tgaBTReadAddress (ScrnInfoPtr pScrn, CARD32 index)
{
    TGAPtr pTga;

    pTga = TGAPTR(pScrn);
    
    TGA_WRITE_REG(BT_PIXEL_MASK << 1 | BT485_WRITE_BIT, TGA_RAMDAC_SETUP_REG);
    TGA_WRITE_REG(0xFF | (BT_PIXEL_MASK<<9), TGA_RAMDAC_REG);
    TGA_WRITE_REG(BT_READ_ADDR << 1 | BT485_WRITE_BIT, TGA_RAMDAC_SETUP_REG);
    TGA_WRITE_REG(index | (BT_READ_ADDR<<9), TGA_RAMDAC_REG);
    TGA_WRITE_REG(BT_RAMDAC_DATA << 1 | BT485_READ_BIT, TGA_RAMDAC_SETUP_REG);
}

unsigned char
tgaBTReadData (ScrnInfoPtr pScrn)
{
    TGAPtr pTga;

    pTga = TGAPTR(pScrn);
    
    return(TGA_READ_REG(TGA_RAMDAC_REG)>>16);
}

/********************* TGA2 stuff below here ********************/

void
tga2BTOutIndReg(ScrnInfoPtr pScrn,
		     CARD32 reg, unsigned char mask, unsigned char data)
{
  TGAPtr pTga;
  unsigned char tmp = 0x00;
  unsigned int addr = 0xe000U | (reg << 8);

  pTga = TGAPTR(pScrn);
  
  if (mask != 0x00)
    tmp = TGA2_READ_RAMDAC_REG(addr) & mask;

#if 0
  xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "tga2OutIndReg: reg 0x%x data 0x%x\n",
	     reg, tmp|data);
#endif
  TGA2_WRITE_RAMDAC_REG ((tmp | data), addr);
}

unsigned char
tga2BTInIndReg (ScrnInfoPtr pScrn, CARD32 reg)
{
  TGAPtr pTga;
  unsigned char ret;
  unsigned int addr = 0xe000U | (reg << 8);

  pTga  = TGAPTR(pScrn);

  ret = TGA2_READ_RAMDAC_REG(addr);

  return (ret);
}

void
tga2BTWriteAddress (ScrnInfoPtr pScrn, CARD32 index)
{
    tga2BTOutIndReg(pScrn, BT_WRITE_ADDR, 0, index);
}

void
tga2BTWriteData (ScrnInfoPtr pScrn, unsigned char data)
{
    tga2BTOutIndReg(pScrn, BT_RAMDAC_DATA, 0, data);
}

void
tga2BTReadAddress (ScrnInfoPtr pScrn, CARD32 index)
{
    tga2BTOutIndReg(pScrn, BT_PIXEL_MASK, 0, 0xff);
    tga2BTOutIndReg(pScrn, BT_READ_ADDR, 0, index);
}

unsigned char
tga2BTReadData (ScrnInfoPtr pScrn)
{
    return tga2BTInIndReg(pScrn, BT_RAMDAC_DATA);
}
