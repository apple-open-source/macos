/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiident.c,v 1.11 2003/01/01 19:16:32 tsi Exp $ */
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

#include "ati.h"
#include "atiident.h"
#include "atiutil.h"
#include "ativersion.h"

#include "r128_probe.h"
#include "radeon_probe.h"

const char *ATIChipsetNames[] =
{
    "ati",

#ifndef AVOID_CPIO

    "ativga",
    "ibmvga",
    "ibm8514",
    "vgawonder",
    "mach8",
    "mach32",

#endif /* AVOID_CPIO */

    "mach64",
    "rage128",
    "radeon"
};

static SymTabRec ATIPublicChipsetNames[] =
{
    {ATI_CHIPSET_ATI, "ati"},

#ifndef AVOID_CPIO

    {ATI_CHIPSET_ATIVGA, "ativga"},
#ifdef __MAYBE_NOT__
    {ATI_CHIPSET_IBMVGA, "ibmvga"},
#endif
#ifdef __NOT_YET__
    {ATI_CHIPSET_IBM8514, "ibm8514"},
#endif

#endif /* AVOID_CPIO */

    {-1, NULL}
};

/*
 * ATIIdentify --
 *
 * Print the driver's list of chipset names.
 */
void
ATIIdentify
(
    int flags
)
{
    xf86PrintChipsets(ATI_NAME,
        (NumberOf(ATIPublicChipsetNames) <= 2) ?
            "ATI driver (version " ATI_VERSION_NAME ") for chipset" :
            "ATI driver (version " ATI_VERSION_NAME ") for chipsets",
        ATIPublicChipsetNames);
    R128Identify(flags);
    RADEONIdentify(flags);
}

/*
 * ATIIdentProbe --
 *
 * This function determines if the user specified a chipset name acceptable to
 * the driver.  It returns an ATIChipsetType or -1.
 */
int
ATIIdentProbe
(
    const char *ChipsetName
)
{
    int              Chipset;

    static SymTabRec SpecificNames[] =
    {

#ifndef AVOID_CPIO

        {ATI_CHIPSET_VGAWONDER, "vgawonder"},
#ifdef __NOT_YET__
        {ATI_CHIPSET_MACH8, "mach8"},
#endif
        {ATI_CHIPSET_MACH32, "mach32"},

#endif /* AVOID_CPIO */

        {ATI_CHIPSET_MACH64, "mach64"},
        {ATI_CHIPSET_RAGE128, "rage128"},
        {ATI_CHIPSET_RADEON, "radeon"},
        {-1, NULL}
    };

    /* If no Chipset specification, default to "ati" */
    if (!ChipsetName || !*ChipsetName)
        return ATI_CHIPSET_ATI;

    Chipset = xf86StringToToken(ATIPublicChipsetNames, ChipsetName);
    if (Chipset != -1)
        return Chipset;

    /* Check for some other chipset names */
    return xf86StringToToken(SpecificNames, ChipsetName);
}
