/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atioption.c,v 1.23 2004/01/05 16:42:03 tsi Exp $ */
/*
 * Copyright 1999 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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

#include "atioption.h"
#include "atiutil.h"

#include "radeon_probe.h"
#include "r128_probe.h"

/*
 * Recognised XF86Config options.
 */
const OptionInfoRec ATIPublicOptions[] =
{
    {
        ATI_OPTION_ACCEL,
        "accel",
        OPTV_BOOLEAN,
        {0, },
        FALSE
    },
    {
        ATI_OPTION_CRT_DISPLAY,
        "crt_display",
        OPTV_BOOLEAN,
        {0, },
        FALSE
    },
    {
        ATI_OPTION_CSYNC,
        "composite_sync",
        OPTV_BOOLEAN,
        {0, },
        FALSE
    },
    {
        ATI_OPTION_HWCURSOR,
        "hw_cursor",
        OPTV_BOOLEAN,
        {0, },
        FALSE,
    },

#ifndef AVOID_CPIO

    {
        ATI_OPTION_LINEAR,
        "linear",
        OPTV_BOOLEAN,
        {0, },
        FALSE
    },

#endif /* AVOID_CPIO */

    {
        ATI_OPTION_MMIO_CACHE,
        "mmio_cache",
        OPTV_BOOLEAN,
        {0, },
        FALSE
    },
    {
        ATI_OPTION_TEST_MMIO_CACHE,
        "test_mmio_cache",
        OPTV_BOOLEAN,
        {0, },
        FALSE
    },
    {
        ATI_OPTION_PANEL_DISPLAY,
        "panel_display",
        OPTV_BOOLEAN,
        {0, },
        FALSE
    },
    {
        ATI_OPTION_PROBE_CLOCKS,
        "probe_clocks",
        OPTV_BOOLEAN,
        {0, },
        FALSE
    },
    {
        ATI_OPTION_REFERENCE_CLOCK,
        "reference_clock",
        OPTV_FREQ,
        {0, },
        FALSE
    },
    {
        ATI_OPTION_SHADOW_FB,
        "shadow_fb",
        OPTV_BOOLEAN,
        {0, },
        FALSE
    },
    {
        ATI_OPTION_SWCURSOR,
        "sw_cursor",
        OPTV_BOOLEAN,
        {0, },
        FALSE,
    },
    {
        -1,
        NULL,
        OPTV_NONE,
        {0, },
        FALSE
    }
};

const unsigned long ATIPublicOptionSize = SizeOf(ATIPublicOptions);

/*
 * ATIAvailableOptions --
 *
 * Return recognised options that are intended for public consumption.
 */
const OptionInfoRec *
ATIAvailableOptions
(
    int ChipId,
    int BusId
)
{
    const OptionInfoRec *pOptions;

    if ((pOptions = R128AvailableOptions(ChipId, BusId)))
        return pOptions;

    if ((pOptions = RADEONAvailableOptions(ChipId, BusId)))
        return pOptions;

    return ATIPublicOptions;
}
