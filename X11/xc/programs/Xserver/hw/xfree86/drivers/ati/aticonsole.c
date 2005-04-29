/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/aticonsole.c,v 1.23 2004/01/05 16:42:01 tsi Exp $ */
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

#include "atiadapter.h"
#include "aticonsole.h"
#include "aticrtc.h"
#include "atii2c.h"
#include "atilock.h"
#include "atimach64.h"
#include "atimode.h"
#include "atistruct.h"
#include "ativga.h"
#include "atividmem.h"

#include "xf86.h"

/*
 * ATISaveScreen --
 *
 * This function is a screen saver hook for DIX.
 */
Bool
ATISaveScreen
(
    ScreenPtr pScreen,
    int       Mode
)
{
    ScrnInfoPtr pScreenInfo;
    ATIPtr      pATI;

    if ((Mode != SCREEN_SAVER_ON) && (Mode != SCREEN_SAVER_CYCLE))
        SetTimeSinceLastInputEvent();

    if (!pScreen)
        return TRUE;

    pScreenInfo = xf86Screens[pScreen->myNum];
    if (!pScreenInfo->vtSema)
        return TRUE;

    pATI = ATIPTR(pScreenInfo);
    switch (pATI->NewHW.crtc)
    {

#ifndef AVOID_CPIO

        case ATI_CRTC_VGA:
            ATIVGASaveScreen(pATI, Mode);
            break;

#endif /* AVOID_CPIO */

        case ATI_CRTC_MACH64:
            ATIMach64SaveScreen(pATI, Mode);
            break;

        default:
            break;
    }

    return TRUE;
}

/*
 * ATISetDPMSMode --
 *
 * This function sets the adapter's VESA Display Power Management Signaling
 * mode.
 */
void
ATISetDPMSMode
(
    ScrnInfoPtr pScreenInfo,
    int         DPMSMode,
    int         flags
)
{
    ATIPtr pATI;

    if (!pScreenInfo || !pScreenInfo->vtSema)
        return;

    pATI = ATIPTR(pScreenInfo);

    switch (pATI->Adapter)
    {
        case ATI_ADAPTER_MACH64:
            ATIMach64SetDPMSMode(pScreenInfo, pATI, DPMSMode);
            break;

        default:

#ifndef AVOID_CPIO

            /* Assume EGA/VGA */
            ATIVGASetDPMSMode(pATI, DPMSMode);
            break;

        case ATI_ADAPTER_NONE:
        case ATI_ADAPTER_8514A:
        case ATI_ADAPTER_MACH8:

#endif /* AVOID_CPIO */

            break;
    }
}

/*
 * ATIEnterGraphics --
 *
 * This function sets the hardware to a graphics video state.
 */
Bool
ATIEnterGraphics
(
    ScreenPtr   pScreen,
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI
)
{
    /* Map apertures */
    if (!ATIMapApertures(pScreenInfo->scrnIndex, pATI))
        return FALSE;

    /* Unlock device */
    ATIUnlock(pATI);

    /* Calculate hardware data */
    if (pScreen &&
        !ATIModeCalculate(pScreenInfo->scrnIndex, pATI, &pATI->NewHW,
            pScreenInfo->currentMode))
        return FALSE;

    pScreenInfo->vtSema = TRUE;

    /* Save current state */
    ATIModeSave(pScreenInfo, pATI, &pATI->OldHW);

    /* Set graphics state */
    ATIModeSet(pScreenInfo, pATI, &pATI->NewHW);

    /* Possibly blank the screen */
    if (pScreen)
       (void)ATISaveScreen(pScreen, SCREEN_SAVER_ON);

    /* Position the screen */
    (*pScreenInfo->AdjustFrame)(pScreenInfo->scrnIndex,
        pScreenInfo->frameX0, pScreenInfo->frameY0, 0);

    SetTimeSinceLastInputEvent();

    return TRUE;
}

/*
 * ATILeaveGraphics --
 *
 * This function restores the hardware to its previous state.
 */
void
ATILeaveGraphics
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI
)
{
    if (pScreenInfo->vtSema)
    {
        /* If not exiting, save graphics video state */
        if (!xf86ServerIsExiting())
            ATIModeSave(pScreenInfo, pATI, &pATI->NewHW);

        /* Restore mode in effect on server entry */
        ATIModeSet(pScreenInfo, pATI, &pATI->OldHW);

        pScreenInfo->vtSema = FALSE;
    }

    /* Lock device */
    ATILock(pATI);

    /* Unmap apertures */

#ifdef AVOID_DGA

    if (!pATI->Closeable)

#else /* AVOID_DGA */

    if (!pATI->Closeable || !pATI->nDGAMode)

#endif /* AVOID_DGA */

        ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);

    SetTimeSinceLastInputEvent();
}

/*
 * ATISwitchMode --
 *
 * This function switches to another graphics video state.
 */
Bool
ATISwitchMode
(
    int            iScreen,
    DisplayModePtr pMode,
    int            flags
)
{
    ScrnInfoPtr pScreenInfo = xf86Screens[iScreen];
    ATIPtr      pATI        = ATIPTR(pScreenInfo);

    /* Calculate new hardware data */
    if (!ATIModeCalculate(iScreen, pATI, &pATI->NewHW, pMode))
        return FALSE;

    /* Set new hardware state */
    if (pScreenInfo->vtSema)
    {
        pScreenInfo->currentMode = pMode;
        ATIModeSet(pScreenInfo, pATI, &pATI->NewHW);
    }

    SetTimeSinceLastInputEvent();

    return TRUE;
}

/*
 * ATIEnterVT --
 *
 * This function sets the server's virtual console to a graphics video state.
 */
Bool
ATIEnterVT
(
    int iScreen,
    int flags
)
{
    ScrnInfoPtr pScreenInfo = xf86Screens[iScreen];
    ScreenPtr   pScreen     = pScreenInfo->pScreen;
    ATIPtr      pATI        = ATIPTR(pScreenInfo);
    PixmapPtr   pScreenPixmap;
    DevUnion    PixmapPrivate;
    Bool        Entered;

    if (!ATIEnterGraphics(NULL, pScreenInfo, pATI))
        return FALSE;

    /* The rest of this isn't needed for shadowfb */
    if (pATI->OptionShadowFB)
        return TRUE;

#ifndef AVOID_CPIO

    /* If used, modify banking interface */
    if (!miModifyBanking(pScreen, &pATI->BankInfo))
        return FALSE;

#endif /* AVOID_CPIO */

    pScreenPixmap = (*pScreen->GetScreenPixmap)(pScreen);
    PixmapPrivate = pScreenPixmap->devPrivate;
    if (!PixmapPrivate.ptr)
        pScreenPixmap->devPrivate = pScreenInfo->pixmapPrivate;

    /* Tell framebuffer about remapped aperture */
    Entered = (*pScreen->ModifyPixmapHeader)(pScreenPixmap,
        -1, -1, -1, -1, -1, pATI->pMemory);

    if (!PixmapPrivate.ptr)
    {
        pScreenInfo->pixmapPrivate = pScreenPixmap->devPrivate;
        pScreenPixmap->devPrivate.ptr = NULL;
    }

    return Entered;
}

/*
 * ATILeaveVT --
 *
 * This function restores the server's virtual console to its state on server
 * entry.
 */
void
ATILeaveVT
(
    int iScreen,
    int flags
)
{
    ScrnInfoPtr pScreenInfo = xf86Screens[iScreen];

    ATILeaveGraphics(pScreenInfo, ATIPTR(pScreenInfo));
}

/*
 * ATIFreeScreen --
 *
 * This function frees all driver data related to a screen.
 */
void
ATIFreeScreen
(
    int iScreen,
    int flags
)
{
    ScreenPtr   pScreen     = screenInfo.screens[iScreen];
    ScrnInfoPtr pScreenInfo = xf86Screens[iScreen];
    ATIPtr      pATI        = ATIPTR(pScreenInfo);

    if (pATI->Closeable || (serverGeneration > 1))
        ATII2CFreeScreen(iScreen);

    if (pATI->Closeable)
        (void)(*pScreen->CloseScreen)(iScreen, pScreen);

    ATILeaveGraphics(pScreenInfo, pATI);

#ifndef AVOID_CPIO

    xfree(pATI->OldHW.frame_buffer);
    xfree(pATI->NewHW.frame_buffer);

#endif /* AVOID_CPIO */

    xfree(pATI->pShadow);

#ifndef AVOID_DGA

    xfree(pATI->pDGAMode);

#endif /* AVOID_DGA */

    xfree(pATI);
    pScreenInfo->driverPrivate = NULL;
}
