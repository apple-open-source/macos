/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atibank.h,v 1.8 2003/01/01 19:16:30 tsi Exp $ */
/*
 * Copyright 1997 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ___ATIBANK_H___
#define ___ATIBANK_H___ 1

#include "atipriv.h"
#include "atiproto.h"

#include "mibank.h"

#ifndef AVOID_CPIO

/*
 * Banking definitions.
 */

/*
 * Bank selection function for VGA Wonder V3 adapters (which are
 * single-banked).
 */
#define ATIV3SetRead     ATIV3SetReadWrite
#define ATIV3SetWrite    ATIV3SetReadWrite
extern miBankProc        ATIV3SetReadWrite;

/*
 * Bank selection functions for VGA Wonder V4 and V5 adapters.
 */
extern miBankProc        ATIV4V5SetRead,
                         ATIV4V5SetWrite,
                         ATIV4V5SetReadWrite;

/*
 * Bank selection functions for 28800-x, 68800-x and 88800 based adapters.
 */
extern miBankProc        ATIx8800SetRead,
                         ATIx8800SetWrite,
                         ATIx8800SetReadWrite;

/*
 * Bank selection functions used to simulate a banked VGA aperture with a
 * Mach64's small dual paged apertures.  There are two sets of these:  one for
 * packed modes, and one for planar modes.
 */
extern miBankProc        ATIMach64SetReadPacked,
                         ATIMach64SetWritePacked,
                         ATIMach64SetReadWritePacked;
extern miBankProc        ATIMach64SetReadPlanar,
                         ATIMach64SetWritePlanar,
                         ATIMach64SetReadWritePlanar;

/*
 * The CRT save/restore code also needs a separate banking interface that can
 * used before ATIScreenInit() is called.
 */

typedef void ATIBankProc FunctionPrototype((ATIPtr, unsigned int));
typedef ATIBankProc     *ATIBankProcPtr;

extern ATIBankProc       ATIV3SetBank,
                         ATIV4V5SetBank,
                         ATIx8800SetBank,
                         ATIMach64SetBankPacked,
                         ATIMach64SetBankPlanar;

#endif /* AVOID_CPIO */

#endif /* ___ATIBANK_H___ */
