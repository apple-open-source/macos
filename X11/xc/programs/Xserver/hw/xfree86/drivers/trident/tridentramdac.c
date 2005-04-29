/*
 * Copyright 1992-2003 by Alan Hourihane, North Wales, UK.
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
 * TridentOutIndReg() and TridentInIndReg() are used to access 
 * the indirect Trident RAMDAC registers only.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/tridentramdac.c,v 1.5 2003/09/05 22:07:29 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "trident_regs.h"
#include "trident.h"

void
TridentWriteAddress(ScrnInfoPtr pScrn, CARD32 index)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    MMIO_OUTB(0x3C6, 0xFF);
    MMIO_OUTB(0x3C8, index);
}

void
TridentWriteData(ScrnInfoPtr pScrn, unsigned char data)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    MMIO_OUTB(0x3C9, data);
}

void
TridentReadAddress(ScrnInfoPtr pScrn, CARD32 index)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    MMIO_OUTB(0x3C6, 0xFF);
    MMIO_OUTB(0x3C7, index);
}

unsigned char
TridentReadData(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    return(MMIO_INB(0x3C9));
}
