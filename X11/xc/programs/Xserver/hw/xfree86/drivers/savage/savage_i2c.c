/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/savage/savage_i2c.c,v 1.4 2004/02/13 23:58:44 dawes Exp $ */

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


#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"

#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "vgaHW.h"

#include "savage_driver.h"

static void
SavageI2CPutBits(I2CBusPtr b, int clock,  int data)
{
    ScrnInfoPtr pScrn = (ScrnInfoPtr)(xf86Screens[b->scrnIndex]);
    SavagePtr psav = SAVPTR(pScrn);
    unsigned char reg = 0x10;

    if(clock) reg |= 0x1;
    if(data)  reg |= 0x2;

    OutI2CREG(psav,reg);
    /*ErrorF("SavageI2CPutBits: %d %d\n", clock, data); */
}

static void
SavageI2CGetBits(I2CBusPtr b, int *clock, int *data)
{
    ScrnInfoPtr pScrn = (ScrnInfoPtr)(xf86Screens[b->scrnIndex]);
    SavagePtr psav = SAVPTR(pScrn);
    unsigned char reg = 0x10;

    InI2CREG(psav,reg);

    *clock = reg & 0x4;
    *data = reg & 0x8;
    
    /*ErrorF("SavageI2CGetBits: %d %d\n", *clock, *data); */
}

Bool 
SavageI2CInit(ScrnInfoPtr pScrn)
{
    SavagePtr psav = SAVPTR(pScrn);
    I2CBusPtr I2CPtr;

    I2CPtr = xf86CreateI2CBusRec();
    if(!I2CPtr) return FALSE;

    psav->I2C = I2CPtr;

    I2CPtr->BusName    = "I2C bus";
    I2CPtr->scrnIndex  = pScrn->scrnIndex;
    I2CPtr->I2CPutBits = SavageI2CPutBits;
    I2CPtr->I2CGetBits = SavageI2CGetBits;

    if (!xf86I2CBusInit(I2CPtr))
	return FALSE;

    return TRUE;
}



