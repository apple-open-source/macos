/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3virge/s3v_i2c.c,v 1.4 2000/03/31 20:13:32 dawes Exp $ */

/*
Copyright (C) 1994-2000 The XFree86 Project, Inc.  All Rights Reserved.

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

Except as contained in this notice, the name of the XFree86 Project shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the XFree86 Project.
*/

/*
Copyright (C) 1994-2000 The XFree86 Project, Inc.  All Rights Reserved.

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

Except as contained in this notice, the name of the XFree86 Project shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the XFree86 Project.
*/

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"

#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "vgaHW.h"

#include "s3v.h"

static void
s3v_I2CPutBits(I2CBusPtr b, int clock,  int data)
{
    S3VPtr ps3v = S3VPTR(xf86Screens[b->scrnIndex]);
    unsigned int reg = 0x10;

    if(clock) reg |= 0x1;
    if(data)  reg |= 0x2;

    OUTREG(DDC_REG,reg);
    /*ErrorF("s3v_I2CPutBits: %d %d\n", clock, data); */
}

static void
s3v_I2CGetBits(I2CBusPtr b, int *clock, int *data)
{
    S3VPtr ps3v = S3VPTR(xf86Screens[b->scrnIndex]);
    unsigned int reg;

    reg = (INREG(DDC_REG));

    *clock = reg & 0x4;
    *data = reg & 0x8;
    
    /*ErrorF("s3v_I2CGetBits: %d %d\n", *clock, *data);*/
}

Bool 
S3V_I2CInit(ScrnInfoPtr pScrn)
{
    S3VPtr ps3v = S3VPTR(pScrn);
    I2CBusPtr I2CPtr;


    I2CPtr = xf86CreateI2CBusRec();
    if(!I2CPtr) return FALSE;

    ps3v->I2C = I2CPtr;

    I2CPtr->BusName    = "I2C bus";
    I2CPtr->scrnIndex  = pScrn->scrnIndex;
    I2CPtr->I2CPutBits = s3v_I2CPutBits;
    I2CPtr->I2CGetBits = s3v_I2CGetBits;

    if (!xf86I2CBusInit(I2CPtr))
      return FALSE;

    return TRUE;
}



