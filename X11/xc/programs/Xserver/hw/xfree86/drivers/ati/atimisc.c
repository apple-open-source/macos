/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atimisc.c,v 1.6 2003/01/01 19:16:32 tsi Exp $ */
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

#ifdef XFree86LOADER

#include "ati.h"
#include "atiload.h"
#include "ativersion.h"

/* Module loader interface for subsidiary driver module */

static XF86ModuleVersionInfo ATIVersionRec =
{
    "atimisc",
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
        /* Ensure main driver module is loaded, but not as a submodule */
        if (!xf86ServerIsOnlyDetecting() && !LoaderSymbol(ATI_NAME))
            xf86LoadOneModule(ATI_DRIVER_NAME, Options);

        /*
         * Tell loader about symbols from other modules that this module might
         * refer to.
         */
        xf86LoaderRefSymLists(
            ATIint10Symbols,
            ATIddcSymbols,
            ATIvbeSymbols,

#ifndef AVOID_CPIO

            ATIxf1bppSymbols,
            ATIxf4bppSymbols,

#endif /* AVOID_CPIO */

            ATIfbSymbols,
            ATIshadowfbSymbols,
            ATIxaaSymbols,
            ATIramdacSymbols,
            NULL);

        Inited = TRUE;
    }

    return (pointer)TRUE;
}

/* The following record must be called atimiscModuleData */
XF86ModuleData atimiscModuleData =
{
    &ATIVersionRec,
    ATISetup,
    NULL
};

#endif /* XFree86LOADER */
