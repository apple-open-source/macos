/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/aticonfig.c,v 1.12 2003/01/01 19:16:31 tsi Exp $ */
/*
 * Copyright 2000 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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

#include "ati.h"
#include "atiadapter.h"
#include "atichip.h"
#include "aticonfig.h"
#include "aticursor.h"
#include "atioption.h"
#include "atistruct.h"

/*
 * Non-publicised XF86Config options.
 */
typedef enum
{
    ATI_OPTION_CRT_SCREEN,      /* Legacy negation of "PanelDisplay" */
    ATI_OPTION_DEVEL,           /* Intentionally undocumented */
    ATI_OPTION_BLEND,           /* Force horizontal blending of small modes */
    ATI_OPTION_SYNC             /* Use XF86Config panel mode porches */
} ATIPrivateOptionType;

/*
 * ATIProcessOptions --
 *
 * This function extracts options from what was parsed out of the XF86Config
 * file.
 */
void
ATIProcessOptions
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI
)
{
    OptionInfoPtr PublicOption = xnfalloc(ATIPublicOptionSize);
    OptionInfoRec PrivateOption[] =
    {
        {                       /* Negation of "PanelDisplay" public option */
            ATI_OPTION_CRT_SCREEN,
            "crtscreen",
            OPTV_BOOLEAN,
            {0, },
            FALSE
        },
        {                       /* ON:   Horizontally blend most modes */
            ATI_OPTION_BLEND,   /* OFF:  Use pixel replication more often */
            "lcdblend",
            OPTV_BOOLEAN,
            {0, },
            FALSE
        },
        {                       /* ON:   Use XF86Config porch timings */
            ATI_OPTION_SYNC,    /* OFF:  Use porches from mode on entry */
            "lcdsync",
            OPTV_BOOLEAN,
            {0, },
            FALSE
        },
        {                       /* ON:   Ease exploration of loose ends */
            ATI_OPTION_DEVEL,   /* OFF:  Fit for public consumption */
            "tsi",
            OPTV_BOOLEAN,
            {0, },
            FALSE
        },
        {
            -1,
            NULL,
            OPTV_NONE,
            {0, },
            FALSE
        }
    };

    (void)memcpy(PublicOption, ATIPublicOptions, ATIPublicOptionSize);

#   define Accel        PublicOption[ATI_OPTION_ACCEL].value.bool
#   define Blend        PrivateOption[ATI_OPTION_BLEND].value.bool
#   define CRTDisplay   PublicOption[ATI_OPTION_CRT_DISPLAY].value.bool
#   define CRTScreen    PrivateOption[ATI_OPTION_CRT_SCREEN].value.bool
#   define CSync        PublicOption[ATI_OPTION_CSYNC].value.bool
#   define Devel        PrivateOption[ATI_OPTION_DEVEL].value.bool
#   define HWCursor     PublicOption[ATI_OPTION_HWCURSOR].value.bool

#ifndef AVOID_CPIO

#   define Linear       PublicOption[ATI_OPTION_LINEAR].value.bool

#endif /* AVOID_CPIO */

#   define CacheMMIO    PublicOption[ATI_OPTION_MMIO_CACHE].value.bool
#   define PanelDisplay PublicOption[ATI_OPTION_PANEL_DISPLAY].value.bool
#   define ProbeClocks  PublicOption[ATI_OPTION_PROBE_CLOCKS].value.bool
#   define ShadowFB     PublicOption[ATI_OPTION_SHADOW_FB].value.bool
#   define SWCursor     PublicOption[ATI_OPTION_SWCURSOR].value.bool
#   define Sync         PrivateOption[ATI_OPTION_SYNC].value.bool

#   define ReferenceClock \
        PublicOption[ATI_OPTION_REFERENCE_CLOCK].value.freq.freq

    /* Pick up XF86Config options */
    xf86CollectOptions(pScreenInfo, NULL);

    /* Set non-zero defaults */

#ifndef AVOID_CPIO

    if (pATI->Adapter >= ATI_ADAPTER_MACH64)

#endif /* AVOID_CPIO */

    {
        Accel = CacheMMIO = HWCursor = TRUE;

#ifndef AVOID_CPIO

        Linear = TRUE;

#endif /* AVOID_CPIO */

    }

    ReferenceClock = ((double)157500000.0) / ((double)11.0);

#ifndef AVOID_CPIO

    if (pATI->PCIInfo)

#endif /* AVOID_CPIO */

    {
        ShadowFB = TRUE;
    }

    Blend = PanelDisplay = TRUE;

    xf86ProcessOptions(pScreenInfo->scrnIndex, pScreenInfo->options,
        PublicOption);
    xf86ProcessOptions(pScreenInfo->scrnIndex, pScreenInfo->options,
        PrivateOption);

#ifndef AVOID_CPIO

    /* Disable linear apertures if the OS doesn't support them */
    if (!xf86LinearVidMem() && Linear)
    {
        if (PublicOption[ATI_OPTION_LINEAR].found)
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "OS does not support linear apertures.\n");
        Linear = FALSE;
    }

#endif /* AVOID_CPIO */

    /* Move option values into driver private structure */
    pATI->OptionAccel = Accel;
    pATI->OptionBlend = Blend;
    pATI->OptionCRTDisplay = CRTDisplay;
    pATI->OptionCSync = CSync;
    pATI->OptionDevel = Devel;

#ifndef AVOID_CPIO

    pATI->OptionLinear = Linear;

#endif /* AVOID_CPIO */

    pATI->OptionMMIOCache = CacheMMIO;
    pATI->OptionProbeClocks = ProbeClocks;
    pATI->OptionShadowFB = ShadowFB;
    pATI->OptionSync = Sync;

    /* "CRTScreen" is now "NoPanelDisplay" */
    if ((PanelDisplay != CRTScreen) ||
        PublicOption[ATI_OPTION_PANEL_DISPLAY].found)
        pATI->OptionPanelDisplay = PanelDisplay;
    else
        pATI->OptionPanelDisplay = !CRTScreen;

    /* Validate and set cursor options */
    pATI->Cursor = ATI_CURSOR_SOFTWARE;
    if (SWCursor || !HWCursor)
    {
        if (HWCursor && PublicOption[ATI_OPTION_HWCURSOR].found)
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Option \"sw_cursor\" overrides Option \"hw_cursor\".\n");
    }
    else if (pATI->Chip < ATI_CHIP_264CT)
    {
        if (HWCursor && PublicOption[ATI_OPTION_HWCURSOR].found)
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Option \"hw_cursor\" not supported in this configuration.\n");
    }
    else
    {
        pATI->Cursor = ATI_CURSOR_HARDWARE;
    }

    /* Only set the reference clock if it hasn't already been determined */
    if (!pATI->ReferenceNumerator || !pATI->ReferenceDenominator)
    {
        switch ((int)(ReferenceClock / ((double)100000.0)))
        {
            case 143:
                pATI->ReferenceNumerator = 157500;
                pATI->ReferenceDenominator = 11;
                break;

            case 286:
                pATI->ReferenceNumerator = 315000;
                pATI->ReferenceDenominator = 11;
                break;

            default:
                pATI->ReferenceNumerator =
                    (int)(ReferenceClock / ((double)1000.0));
                pATI->ReferenceDenominator = 1;
                break;
        }
    }

    xfree(PublicOption);
}
