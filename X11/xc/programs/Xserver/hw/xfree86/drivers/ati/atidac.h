/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atidac.h,v 1.15 2003/01/01 19:16:31 tsi Exp $ */
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

#ifndef ___ATIDAC_H___

#if !defined(___ATI_H___) && defined(XFree86Module)
# error missing #include "ati.h" before #include "atidac.h"
# undef XFree86Module
#endif

#define ___ATIDAC_H___ 1

#include "aticrtc.h"
#include "atipriv.h"
#include "atiproto.h"

#include "xf86str.h"

#include "colormapst.h"

/*
 * RAMDAC-related definitions.
 */
#define ATI_DAC_MAX_TYPE         MaxBits(DACTYPE)
#define ATI_DAC_MAX_SUBTYPE      MaxBits(BIOS_INIT_DAC_SUBTYPE)

#define ATI_DAC(_Type, _Subtype) (((_Type) << 4) | (_Subtype))

#define ATI_DAC_ATI68830         ATI_DAC(0x0U, 0x0U)
#define ATI_DAC_SC11483          ATI_DAC(0x1U, 0x0U)
#define ATI_DAC_ATI68875         ATI_DAC(0x2U, 0x0U)
#define ATI_DAC_TVP3026_A        ATI_DAC(0x2U, 0x7U)
#define ATI_DAC_GENERIC          ATI_DAC(0x3U, 0x0U)
#define ATI_DAC_BT481            ATI_DAC(0x4U, 0x0U)
#define ATI_DAC_ATT20C491        ATI_DAC(0x4U, 0x1U)
#define ATI_DAC_SC15026          ATI_DAC(0x4U, 0x2U)
#define ATI_DAC_MU9C1880         ATI_DAC(0x4U, 0x3U)
#define ATI_DAC_IMSG174          ATI_DAC(0x4U, 0x4U)
#define ATI_DAC_ATI68860_B       ATI_DAC(0x5U, 0x0U)
#define ATI_DAC_ATI68860_C       ATI_DAC(0x5U, 0x1U)
#define ATI_DAC_TVP3026_B        ATI_DAC(0x5U, 0x7U)
#define ATI_DAC_STG1700          ATI_DAC(0x6U, 0x0U)
#define ATI_DAC_ATT20C498        ATI_DAC(0x6U, 0x1U)
#define ATI_DAC_STG1702          ATI_DAC(0x7U, 0x0U)
#define ATI_DAC_SC15021          ATI_DAC(0x7U, 0x1U)
#define ATI_DAC_ATT21C498        ATI_DAC(0x7U, 0x2U)
#define ATI_DAC_STG1703          ATI_DAC(0x7U, 0x3U)
#define ATI_DAC_CH8398           ATI_DAC(0x7U, 0x4U)
#define ATI_DAC_ATT20C408        ATI_DAC(0x7U, 0x5U)
#define ATI_DAC_INTERNAL         ATI_DAC(0x8U, 0x0U)
#define ATI_DAC_IBMRGB514        ATI_DAC(0x9U, 0x0U)
#define ATI_DAC_UNKNOWN          ATI_DAC((ATI_DAC_MAX_TYPE << 2) + 3, \
                                         ATI_DAC_MAX_SUBTYPE)
extern const SymTabRec ATIDACDescriptors[];

#ifdef AVOID_CPIO

#   define DACDelay     /* Nothing */

#else /* AVOID_CPIO */

#   define DACDelay                         \
        do                                  \
        {                                   \
            (void)inb(pATI->CPIO_DAC_WAIT); \
            (void)inb(pATI->CPIO_DAC_WAIT); \
        } while (0)

    extern void ATISetDACIOPorts FunctionPrototype((ATIPtr, ATICRTCType));

#endif /* AVOID_CPIO */

extern CARD8 ATIGetDACCmdReg     FunctionPrototype((ATIPtr));

extern void ATIDACPreInit        FunctionPrototype((ScrnInfoPtr, ATIPtr,
                                                    ATIHWPtr));
extern void ATIDACSave           FunctionPrototype((ATIPtr, ATIHWPtr));
extern void ATIDACSet            FunctionPrototype((ATIPtr, ATIHWPtr));

extern void ATILoadPalette       FunctionPrototype((ScrnInfoPtr, int, int *,
                                                    LOCO *, VisualPtr));

#endif /* ___ATIDAC_H___ */
