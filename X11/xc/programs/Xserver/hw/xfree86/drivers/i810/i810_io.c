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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i810_io.c,v 1.5 2002/09/11 00:29:32 dawes Exp $ */

/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

/*
 * Authors:
 *   Daryll Strauss <daryll@precisioninsight.com>
 *
 */

#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "compiler.h"

#ifdef BUILD_FOR_I830
#include "i830.h"
#define pI810 pI830
#define I810Ptr I830Ptr
#define I810WriteControlPIO I830WriteControlPIO
#define I810ReadControlPIO I830ReadControlPIO
#define I810WriteStandardPIO I830WriteStandardPIO
#define I810ReadStandardPIO I830ReadStandardPIO
#define I810SetPIOAccess I830SetPIOAccess
#define I810WriteControlMMIO I830WriteControlMMIO
#define I810ReadControlMMIO I830ReadControlMMIO
#define I810WriteStandardMMIO I830WriteStandardMMIO
#define I810ReadStandardMMIO I830ReadStandardMMIO
#define I810SetMMIOAccess I830SetMMIOAccess
#else
#include "i810.h"
#endif

#define minb(p) *(volatile CARD8 *)(pI810->MMIOBase + (p))
#define moutb(p,v) *(volatile CARD8 *)(pI810->MMIOBase + (p)) = (v)

static void
I810WriteControlPIO(I810Ptr pI810, IOADDRESS addr, CARD8 index, CARD8 val)
{
   addr += pI810->ioBase;
   outb(addr, index);
   outb(addr + 1, val);
}

static CARD8
I810ReadControlPIO(I810Ptr pI810, IOADDRESS addr, CARD8 index)
{
   addr += pI810->ioBase;
   outb(addr, index);
   return inb(addr + 1);
}

static void
I810WriteStandardPIO(I810Ptr pI810, IOADDRESS addr, CARD8 val)
{
   outb(pI810->ioBase + addr, val);
}

static CARD8
I810ReadStandardPIO(I810Ptr pI810, IOADDRESS addr)
{
   return inb(pI810->ioBase + addr);
}

void
I810SetPIOAccess(I810Ptr pI810)
{
   pI810->writeControl = I810WriteControlPIO;
   pI810->readControl = I810ReadControlPIO;
   pI810->writeStandard = I810WriteStandardPIO;
   pI810->readStandard = I810ReadStandardPIO;
}

static void
I810WriteControlMMIO(I810Ptr pI810, IOADDRESS addr, CARD8 index, CARD8 val)
{
   moutb(addr, index);
   moutb(addr + 1, val);
}

static CARD8
I810ReadControlMMIO(I810Ptr pI810, IOADDRESS addr, CARD8 index)
{
   moutb(addr, index);
   return minb(addr + 1);
}

static void
I810WriteStandardMMIO(I810Ptr pI810, IOADDRESS addr, CARD8 val)
{
   moutb(addr, val);
}

static CARD8
I810ReadStandardMMIO(I810Ptr pI810, IOADDRESS addr)
{
   return minb(addr);
}

void
I810SetMMIOAccess(I810Ptr pI810)
{
   pI810->writeControl = I810WriteControlMMIO;
   pI810->readControl = I810ReadControlMMIO;
   pI810->writeStandard = I810WriteStandardMMIO;
   pI810->readStandard = I810ReadStandardMMIO;
}
