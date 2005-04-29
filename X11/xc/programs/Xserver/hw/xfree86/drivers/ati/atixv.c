/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atixv.c,v 1.6 2004/01/05 16:42:05 tsi Exp $ */
/*
 * Copyright 2001 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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

#include "atiadapter.h"
#include "atimach64xv.h"
#include "atistruct.h"
#include "atixv.h"

/*
 * ATIXVFreeAdaptorInfo --
 *
 * Free XVideo adaptor information.
 */
static void
ATIXVFreeAdaptorInfo
(
    XF86VideoAdaptorPtr *ppAdaptor,
    int                 nAdaptor
)
{
    if (!ppAdaptor)
        return;

    while (nAdaptor > 0)
        xfree(ppAdaptor[--nAdaptor]);

    xfree(ppAdaptor);
}

/*
 * ATIXVInitializeAdaptor --
 *
 * This is called by the server's XVideo support layer to initialise an XVideo
 * adapter.
 */
static int
ATIXVInitializeAdaptor
(
    ScrnInfoPtr         pScreenInfo,
    XF86VideoAdaptorPtr **pppAdaptor
)
{
    ScreenPtr           pScreen    = screenInfo.screens[pScreenInfo->scrnIndex];
    ATIPtr              pATI       = ATIPTR(pScreenInfo);
    XF86VideoAdaptorPtr *ppAdaptor = NULL;
    int                 nAdaptor;

    switch (pATI->Adapter)
    {
        case ATI_ADAPTER_MACH64:
            nAdaptor = ATIMach64XVInitialiseAdaptor(pScreen, pScreenInfo, pATI,
                &ppAdaptor);
            break;

        default:
            nAdaptor = 0;
            break;
    }

    if (pppAdaptor)
        *pppAdaptor = ppAdaptor;
    else
        ATIXVFreeAdaptorInfo(ppAdaptor, nAdaptor);

    return nAdaptor;
}

/*
 * ATIXVPreInit --
 *
 * This function is called by ATIPreInit() to set up the environment required
 * to support the XVideo extension.
 */
void
ATIXVPreInit
(
    ATIPtr      pATI
)
{

#ifndef AVOID_CPIO

    /* Currently a linear aperture is needed ... */
    if (!pATI->LinearBase)
        return;

#endif /* AVOID_CPIO */

    (void)xf86XVRegisterGenericAdaptorDriver(ATIXVInitializeAdaptor);
}

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
    Bool                result;

    if (!(pScreenInfo->memPhysBase = pATI->LinearBase))
        return FALSE;

    pScreenInfo->fbOffset = 0;

    nAdaptor = xf86XVListGenericAdaptors(pScreenInfo, &ppAdaptor);
    result = xf86XVScreenInit(pScreen, ppAdaptor, nAdaptor);

    ATIXVFreeAdaptorInfo(ppAdaptor, nAdaptor);

    return result;
}

/*
 * ATICloseXVideo --
 *
 * This function is called during screen termination to clean up after XVideo
 * initialisation.
 */
void
ATICloseXVideo
(
    ScreenPtr   pScreen,
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI
)
{
    switch (pATI->Adapter)
    {
        case ATI_ADAPTER_MACH64:
            ATIMach64CloseXVideo(pScreen, pScreenInfo, pATI);
            break;

        default:
            break;
    }
}
