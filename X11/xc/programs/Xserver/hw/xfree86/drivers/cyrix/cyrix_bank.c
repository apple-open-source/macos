/*
 * Copyright 2000 by Richard A. Hecker, California, United States
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Richard Hecker not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Richard Hecker makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * RICHARD HECKER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL RICHARD HECKER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Richard Hecker, hecker@cat.dfrc.nasa.gov
 *          Re-written for XFree86 v4.0
 * Previous driver (pre-XFree86 v4.0) by
 *          Annius V. Groenink (A.V.Groenink@zfc.nl, avg@cwi.nl),
 *          Dirk H. Hohndel (hohndel@suse.de),
 *          Portions: the GGI project & confidential CYRIX databooks.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cyrix/cyrix_bank.c,v 1.3 2002/11/06 11:38:59 alanh Exp $ */

#define PSZ 8

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

/* Everything using inb/outb, etc needs "compiler.h" */
#include "compiler.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* Driver specific headers */
#include "cyrix.h"

int
CYRIXSetRead(ScreenPtr pScreen, int bank)
{ 
    outw(0x3D6, ((((bank << 3) & 0xFF) << 8) | 0x10));
    return 0;
}


int
CYRIXSetWrite(ScreenPtr pScreen, int bank)
{
    outw(0x3D6, ((((bank << 3) & 0xFF) << 8) | 0x11));
    return 0;
}


int
CYRIXSetReadWrite(ScreenPtr pScreen, int bank)
{
    outw(0x3D6, ((((bank << 3) & 0xFF) << 8) | 0x10));
    outw(0x3D6, ((((bank << 3) & 0xFF) << 8) | 0x11));
    return 0;
}
