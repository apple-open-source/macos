/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atixv.c,v 1.3 2003/01/01 19:16:35 tsi Exp $ */
/*
 * Copyright 2001 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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

#include "atistruct.h"
#include "atixv.h"

#include "xf86xv.h"

/*
 * ATIInitializeXVideo --
 *
 * This function is called to initialise XVideo extension support on a screen.
 */
Bool
ATIInitializeXVideo
(
    ScreenPtr   pScreen,
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI
)
{
    XF86VideoAdaptorPtr *ppAdaptor;
    int                 nAdaptor;

    if (!(pScreenInfo->memPhysBase = pATI->LinearBase))
        return FALSE;

    pScreenInfo->fbOffset = 0;

    nAdaptor = xf86XVListGenericAdaptors(pScreenInfo, &ppAdaptor);
    return xf86XVScreenInit(pScreen, ppAdaptor, nAdaptor);
}
