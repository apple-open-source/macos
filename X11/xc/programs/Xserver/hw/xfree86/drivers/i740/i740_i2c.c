/*
 * Copyright 2001 by Patrick LERDA
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Patrick LERDA not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Patrick LERDA makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * PATRICK LERDA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL PATRICK LERDA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Patrick LERDA
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i740/i740_i2c.c,v 1.1 2002/10/21 13:32:58 alanh Exp $ */

/* not working at this time */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"

#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "vgaHW.h"

#include "xf86xv.h"
#include "i740.h"


static void i740_I2CPutBits(I2CBusPtr b, int clk,  int dat)
{
  I740Ptr pI740=I740PTR(xf86Screens[b->scrnIndex]);
  unsigned char val;

  val=pI740->readControl(pI740, XRX, 0x1C);

  if(clk) val&=~0x40; else val|=0x40;  if(dat) val&=~0x08; else val|=0x08;

  /*if     ( clk &&  dat) val&=0xBF; else if( clk && !dat) val&=0xF7; else if(!clk &&  dat) val|=0x40; else val|=0x08;*/

  val|=0x90;

  pI740->writeControl(pI740, XRX, 0x1C, val);

  ErrorF("i740_I2CPutBits: clk=%d dat=%d [<1c>=0x%02x] [<63>=0x%02x] clk=%d dat=%d\n", clk, dat,val,pI740->readControl(pI740, XRX, 0x63),
	 !!(pI740->readControl(pI740, XRX, 0x63) & 0x02), !!(pI740->readControl(pI740, XRX, 0x63) & 0x01) );
}

static void i740_I2CGetBits(I2CBusPtr b, int *clk, int *dat)
{
  I740Ptr pI740=I740PTR(xf86Screens[b->scrnIndex]);
  unsigned char val;

  {
    val=pI740->readControl(pI740, XRX, 0x1C);
    val|=0x90;
    pI740->writeControl(pI740, XRX, 0x1C, val);
  }

  {
    val=pI740->readControl(pI740, XRX, 0x63);
    *clk=!!(val & 0x02);
    *dat=!!(val & 0x01);
  }

  ErrorF("i740_I2CGetBits: clk=%d dat=%d [<1c>=0x%02x] [<63>=0x%02x]\n", *clk, *dat,pI740->readControl(pI740, XRX, 0x1c) & 0xff,pI740->readControl(pI740, XRX, 0x63));
}

Bool I740_I2CInit(ScrnInfoPtr pScrn)
{
  I740Ptr pI740=I740PTR(pScrn);
  I2CBusPtr I2CPtr;

  { unsigned char val; val=pI740->readControl(pI740, XRX, 0x63); val&=0xFC; pI740->writeControl(pI740, XRX, 0x63, val); }
  { unsigned char val; val=pI740->readControl(pI740, XRX, 0x1C); val|=0x90; pI740->writeControl(pI740, XRX, 0x1C, val); }
  { unsigned char val; val=pI740->readControl(pI740, XRX, 0x63); val&=0xFC; pI740->writeControl(pI740, XRX, 0x63, val); }


  I2CPtr = xf86CreateI2CBusRec();
  if(!I2CPtr) return FALSE;

  pI740->rc_i2c = I2CPtr;

  I2CPtr->BusName    = "I2C bus";
  I2CPtr->scrnIndex  = pScrn->scrnIndex;
  I2CPtr->I2CPutBits = i740_I2CPutBits;
  I2CPtr->I2CGetBits = i740_I2CGetBits;

  if (!xf86I2CBusInit(I2CPtr))
    return FALSE;

  return TRUE;
}
