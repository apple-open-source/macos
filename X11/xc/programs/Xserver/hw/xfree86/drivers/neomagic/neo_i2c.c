/**********************************************************************
Copyright 1998, 1999 by Precision Insight, Inc., Cedar Park, Texas.

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and
its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Precision Insight not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  Precision Insight
and its suppliers make no representations about the suitability of this
software for any purpose.  It is provided "as is" without express or 
implied warranty.

PRECISION INSIGHT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**********************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/neomagic/neo_i2c.c,v 1.5 2002/10/30 12:52:22 alanh Exp $ */

/*
 * The original Precision Insight driver for
 * XFree86 v.3.3 has been sponsored by Red Hat.
 *
 * Authors:
 *   Jens Owen (jens@tungstengraphics.com)
 *   Kevin E. Martin (kevin@precisioninsight.com)
 *
 * Port to Xfree86 v.4.0
 *   1998, 1999 by Egbert Eich (Egbert.Eich@Physik.TU-Darmstadt.DE)
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"

#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "vgaHW.h"

#include "neo.h"

static void
neo_I2CPutBits(I2CBusPtr b, int clock,  int data) {
    vgaHWPtr hwp = VGAHWPTR(xf86Screens[b->scrnIndex]);
    unsigned int reg = 0xF0;

    VGAwCR(0x21,0x00);
    VGAwCR(0x1D,0x01);
    
    if(clock) reg |= 1;
    if(data)  reg |= 0x4;
    VGAwGR(0xA1,reg);
    /*ErrorF("neo_I2CPutBits: %d %d\n", clock, data); */
}

static void
neo_I2CGetBits(I2CBusPtr b, int *clock, int *data) {
    unsigned int reg;
    vgaHWPtr hwp = VGAHWPTR(xf86Screens[b->scrnIndex]);
    
    reg = VGArGR(0xA1);
    *clock = 1 /* (reg & 0x?? ) */;
    *data  = (reg & 0x8) != 0;
    /*ErrorF("neo_I2CGetBits: %d %d\n", *clock, *data);*/
}

Bool 
neo_I2CInit(ScrnInfoPtr pScrn)
{
    NEOPtr pNeo = NEOPTR(pScrn);
    I2CBusPtr I2CPtr;

    I2CPtr = xf86CreateI2CBusRec();

    if(!I2CPtr) return FALSE;

    pNeo->I2C = I2CPtr;

    I2CPtr->BusName    = "I2C bus";
    I2CPtr->scrnIndex  = pScrn->scrnIndex;
    I2CPtr->I2CPutBits = neo_I2CPutBits;
    I2CPtr->I2CGetBits = neo_I2CGetBits;
    /* increase these as the defaults are too low */
    I2CPtr->RiseFallTime = 2;
    I2CPtr->HoldTime = 40;

    if (!xf86I2CBusInit(I2CPtr))
      return FALSE;

    return TRUE;
}



