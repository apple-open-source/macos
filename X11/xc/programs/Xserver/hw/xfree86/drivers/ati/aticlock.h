/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/aticlock.h,v 1.9 2004/01/05 16:42:01 tsi Exp $ */
/*
 * Copyright 1997 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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

#ifndef ___ATICLOCK_H___
#define ___ATICLOCK_H___ 1

#include "atipriv.h"
#include "atiproto.h"

#include "xf86str.h"

/*
 * Definitions related to non-programmable clock generators.
 */
typedef enum
{
    ATI_CLOCK_NONE = 0,
    ATI_CLOCK_VGA = 1,
    ATI_CLOCK_CRYSTALS = 2,
    ATI_CLOCK_18810,
    ATI_CLOCK_18811_0,
    ATI_CLOCK_18811_1,
    ATI_CLOCK_2494AM,
    ATI_CLOCK_MACH64A,
    ATI_CLOCK_MACH64B,
    ATI_CLOCK_MACH64C
} ATIClockType;
extern const char *ATIClockNames[];

/*
 * Definitions related to programmable clock generators.
 */
typedef enum
{
    ATI_CLOCK_UNKNOWN = -1,
    ATI_CLOCK_FIXED = 0,        /* Further described by ATIClockType */
    ATI_CLOCK_ICS2595,
    ATI_CLOCK_STG1703,
    ATI_CLOCK_CH8398,
    ATI_CLOCK_INTERNAL,
    ATI_CLOCK_ATT20C408,
    ATI_CLOCK_IBMRGB514,
    ATI_CLOCK_MAX               /* Must be last */
} ATIProgrammableClockType;

typedef struct
{
    CARD16 MinN, MaxN;          /* Feedback divider and ... */
    CARD16 NAdjust;             /* ... its adjustment and ... */
    CARD16 N1, N2;              /* ... its restrictions */
    CARD16 MinM, MaxM;          /* Reference divider and ... */
    CARD16 MAdjust;             /* ... its adjustment */
    CARD16 NumD, *PostDividers; /* Post-dividers */
    const char *ClockName;
} ClockRec, *ClockPtr;
extern ClockRec ATIClockDescriptors[];

extern void ATIClockPreInit   FunctionPrototype((ScrnInfoPtr, ATIPtr, GDevPtr,
                                                 ClockRangePtr));
extern void ATIClockSave      FunctionPrototype((ScrnInfoPtr, ATIPtr,
                                                 ATIHWPtr));
extern Bool ATIClockCalculate FunctionPrototype((int, ATIPtr, ATIHWPtr,
                                                 DisplayModePtr));
extern void ATIClockSet       FunctionPrototype((ATIPtr, ATIHWPtr));

#endif /* ___ATICLOCK_H___ */
