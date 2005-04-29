/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atichip.h,v 1.27 2004/02/24 16:51:22 tsi Exp $ */
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

#ifndef ___ATICHIP_H___
#define ___ATICHIP_H___ 1

#include "atipriv.h"
#include "atiregs.h"

#include "Xmd.h"

/*
 * Chip-related definitions.
 */
typedef enum
{
    ATI_CHIP_NONE = 0,

#ifndef AVOID_CPIO

    ATI_CHIP_VGA,               /* Generic VGA */
    ATI_CHIP_18800,
    ATI_CHIP_18800_1,
    ATI_CHIP_28800_2,
    ATI_CHIP_28800_4,
    ATI_CHIP_28800_5,
    ATI_CHIP_28800_6,
    ATI_CHIP_8514A,             /* 8514/A */
    ATI_CHIP_CT480,             /* 8514/A clone */
    ATI_CHIP_38800_1,           /* Mach8 */
    ATI_CHIP_68800,             /* Mach32 */
    ATI_CHIP_68800_3,           /* Mach32 */
    ATI_CHIP_68800_6,           /* Mach32 */
    ATI_CHIP_68800LX,           /* Mach32 */
    ATI_CHIP_68800AX,           /* Mach32 */

#endif /* AVOID_CPIO */

    ATI_CHIP_88800GXC,          /* Mach64 */
    ATI_CHIP_88800GXD,          /* Mach64 */
    ATI_CHIP_88800GXE,          /* Mach64 */
    ATI_CHIP_88800GXF,          /* Mach64 */
    ATI_CHIP_88800GX,           /* Mach64 */
    ATI_CHIP_88800CX,           /* Mach64 */
    ATI_CHIP_264CT,             /* Mach64 */
    ATI_CHIP_264ET,             /* Mach64 */
    ATI_CHIP_264VT,             /* Mach64 */
    ATI_CHIP_264GT,             /* Mach64 */
    ATI_CHIP_264VTB,            /* Mach64 */
    ATI_CHIP_264GTB,            /* Mach64 */
    ATI_CHIP_264VT3,            /* Mach64 */
    ATI_CHIP_264GTDVD,          /* Mach64 */
    ATI_CHIP_264LT,             /* Mach64 */
    ATI_CHIP_264VT4,            /* Mach64 */
    ATI_CHIP_264GT2C,           /* Mach64 */
    ATI_CHIP_264GTPRO,          /* Mach64 */
    ATI_CHIP_264LTPRO,          /* Mach64 */
    ATI_CHIP_264XL,             /* Mach64 */
    ATI_CHIP_MOBILITY,          /* Mach64 */
    ATI_CHIP_Mach64,            /* Last among Mach64's */
    ATI_CHIP_RAGE128GL,         /* Rage128 */
    ATI_CHIP_RAGE128VR,         /* Rage128 */
    ATI_CHIP_RAGE128PROGL,      /* Rage128 */
    ATI_CHIP_RAGE128PROVR,      /* Rage128 */
    ATI_CHIP_RAGE128PROULTRA,   /* Rage128 */
    ATI_CHIP_RAGE128MOBILITY3,  /* Rage128 */
    ATI_CHIP_RAGE128MOBILITY4,  /* Rage128 */
    ATI_CHIP_Rage128,           /* Last among Rage128's */
    ATI_CHIP_RADEON,            /* Radeon */
    ATI_CHIP_RADEONVE,          /* Radeon VE */
    ATI_CHIP_RADEONMOBILITY6,   /* Radeon M6 */
    ATI_CHIP_RS100,             /* IGP320 */
    ATI_CHIP_RS200,             /* IGP340 */
    ATI_CHIP_RS250,             /* Radoen 7000 IGP */
    ATI_CHIP_RV200,             /* RV200 */
    ATI_CHIP_RADEONMOBILITY7,   /* Radeon M7 */
    ATI_CHIP_R200,              /* R200 */
    ATI_CHIP_RV250,             /* RV250 */
    ATI_CHIP_RADEONMOBILITY9,   /* Radeon M9 */
    ATI_CHIP_RS300,             /* Radoen 9000 IGP */
    ATI_CHIP_RV280,             /* RV250 */
    ATI_CHIP_RADEONMOBILITY9PLUS,   /* Radeon M9+ */
    ATI_CHIP_R300,              /* R300 */
    ATI_CHIP_RV350,             /* RV350 */
    ATI_CHIP_R350,              /* R350 */
    ATI_CHIP_R360,              /* R360 */
    ATI_CHIP_Radeon,            /* Last among Radeon's */
    ATI_CHIP_HDTV               /* HDTV */
} ATIChipType;

extern const char *ATIChipNames[];

/*
 * Foundry codes for 264xT's.
 */
typedef enum
{
    ATI_FOUNDRY_SGS,    /* SGS-Thompson */
    ATI_FOUNDRY_NEC,    /* NEC */
    ATI_FOUNDRY_KSC,    /* KSC (?) */
    ATI_FOUNDRY_UMC,    /* United Microelectronics Corporation */
    ATI_FOUNDRY_TSMC,   /* Taiwan Semiconductor Manufacturing Company */
    ATI_FOUNDRY_5,
    ATI_FOUNDRY_6,
    ATI_FOUNDRY_UMCA    /* UMC alternate */
} ATIFoundryType;

extern const char *ATIFoundryNames[];

#ifndef AVOID_CPIO

extern void        ATIMach32ChipID FunctionPrototype((ATIPtr));

#endif /* AVOID_CPIO */

extern void        ATIMach64ChipID FunctionPrototype((ATIPtr, const CARD16));
extern ATIChipType ATIChipID       FunctionPrototype((const CARD16,
                                                      const CARD8));

#define OldChipID(_1, _0) \
    (SetBits(_0 - 'A', CHIP_CODE_0) | SetBits(_1 - 'A', CHIP_CODE_1))

#define NewChipID(_1, _0) \
    (SetBits(_0, CFG_CHIP_TYPE0) | SetBits(_1, CFG_CHIP_TYPE1))

#define OldToNewChipID(_ChipID) \
    (SetBits(GetBits(_ChipID, CHIP_CODE_0) + 'A', CFG_CHIP_TYPE0) | \
     SetBits(GetBits(_ChipID, CHIP_CODE_1) + 'A', CFG_CHIP_TYPE1))

#define NewToOldChipID(_ChipID) \
    (SetBits(GetBits(_ChipID, CFG_CHIP_TYPE0) - 'A', CHIP_CODE_0) | \
    (SetBits(GetBits(_ChipID, CFG_CHIP_TYPE1) - 'A', CHIP_CODE_1))

#endif /* ___ATICHIP_H___ */
