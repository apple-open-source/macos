/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atimodule.c,v 1.15 2003/01/01 19:16:32 tsi Exp $ */
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

#ifdef XFree86LOADER

#include "ati.h"
#include "atimodule.h"
#include "ativersion.h"

/* Module loader interface */

const char *ATISymbols[] =
{
    "ATIPreInit",
    "ATIScreenInit",
    "ATISwitchMode",
    "ATIAdjustFrame",
    "ATIEnterVT",
    "ATILeaveVT",
    "ATIFreeScreen",
    "ATIValidMode",
    NULL
};

const char *R128Symbols[] =
{
    "R128PreInit",
    "R128ScreenInit",
    "R128SwitchMode",
    "R128AdjustFrame",
    "R128EnterVT",
    "R128LeaveVT",
    "R128FreeScreen",
    "R128ValidMode",
    "R128Options",
    NULL
};

const char *RADEONSymbols[] =
{
    "RADEONPreInit",
    "RADEONScreenInit",
    "RADEONSwitchMode",
    "RADEONAdjustFrame",
    "RADEONEnterVT",
    "RADEONLeaveVT",
    "RADEONFreeScreen",
    "RADEONValidMode",
    "RADEONOptions",
    NULL
};

static XF86ModuleVersionInfo ATIVersionRec =
{
    ATI_DRIVER_NAME,
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    ATI_VERSION_MAJOR, ATI_VERSION_MINOR, ATI_VERSION_PATCH,
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0}
};

/*
 * ATISetup --
 *
 * This function is called every time the module is loaded.
 */
static pointer
ATISetup
(
    pointer Module,
    pointer Options,
    int     *ErrorMajor,
    int     *ErrorMinor
)
{
    static Bool Inited = FALSE;

    if (!Inited)
    {
        Inited = TRUE;
        xf86AddDriver(&ATI, Module, 0);

        xf86LoaderRefSymLists(
            ATISymbols,
            R128Symbols,
            RADEONSymbols,
            NULL);
    }

    return (pointer)1;
}

/* The following record must be called atiModuleData */
XF86ModuleData atiModuleData =
{
    &ATIVersionRec,
    ATISetup,
    NULL
};

#endif /* XFree86LOADER */
