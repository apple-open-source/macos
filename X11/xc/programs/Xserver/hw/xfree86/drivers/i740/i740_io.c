/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i740/i740_io.c,v 1.5 2002/10/21 13:32:58 alanh Exp $ */

/*
 * Authors:
 *   Daryll Strauss <daryll@precisioninsight.com>
 *
 */

#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "vgaHW.h"

#include "xf86xv.h"
#include "i740.h"

static void I740WriteControlPIO(I740Ptr pI740, int addr, unsigned char index, char val) {
  outb(addr, index);
  outb(addr+1, val);
}

static char I740ReadControlPIO(I740Ptr pI740, int addr, unsigned char index) {
  outb(addr, index);
  return inb(addr+1);
}

static void I740WriteStandardPIO(I740Ptr pI740, int addr, unsigned char val) {
  outb(addr, val);
}

static char I740ReadStandardPIO(I740Ptr pI740, int addr) {
  return inb(addr);
}

void I740SetPIOAccess(I740Ptr pI740) {
  pI740->writeControl=I740WriteControlPIO;
  pI740->readControl=I740ReadControlPIO;
  pI740->writeStandard=I740WriteStandardPIO;
  pI740->readStandard=I740ReadStandardPIO;
}

static void I740WriteControlMMIO(I740Ptr pI740, int addr, unsigned char index, char val) {
  moutb(addr, index);
  moutb(addr+1, val);
}

static char I740ReadControlMMIO(I740Ptr pI740, int addr, unsigned char index) {
  moutb(addr, index);
  return minb(addr+1);
}

static void I740WriteStandardMMIO(I740Ptr pI740, int addr, unsigned char val) {
  moutb(addr, val);
}

static char I740ReadStandardMMIO(I740Ptr pI740, int addr) {
  return minb(addr);
}

void I740SetMMIOAccess(I740Ptr pI740) {
  pI740->writeControl=I740WriteControlMMIO;
  pI740->readControl=I740ReadControlMMIO;
  pI740->writeStandard=I740WriteStandardMMIO;
  pI740->readStandard=I740ReadStandardMMIO;
}

