/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiscreen.c,v 1.29 2003/01/01 19:16:34 tsi Exp $ */
/*
 * Copyright 1999 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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
#include "atiaccel.h"
#include "aticonsole.h"
#include "aticursor.h"
#include "atidac.h"
#include "atidga.h"
#include "atiscreen.h"
#include "atistruct.h"
#include "atixv.h"

#include "shadowfb.h"
#include "xf86cmap.h"

#include "xf1bpp.h"
#include "xf4bpp.h"

#include "fb.h"

#include "mibank.h"
#include "micmap.h"
#include "mipointer.h"

/*
 * ATIRefreshArea --
 *
 * This function is called by the shadow frame buffer code to refresh the
 * hardware frame buffer.
 */
static void
ATIRefreshArea
(
    ScrnInfoPtr pScreenInfo,
    int         nBox,
    BoxPtr      pBox
)
{
    ATIPtr  pATI = ATIPTR(pScreenInfo);
    pointer pSrc, pDst;
    int     offset, w, h;

    while (nBox-- > 0)
    {
        w = (pBox->x2 - pBox->x1) * pATI->FBBytesPerPixel;
        h = pBox->y2 - pBox->y1;
        offset =
            (pBox->y1 * pATI->FBPitch) + (pBox->x1 * pATI->FBBytesPerPixel);
        pSrc = (char *)pATI->pShadow + offset;
        pDst = (char *)pATI->pMemory + offset;

        while (h-- > 0)
        {
            (void)memcpy(pDst, pSrc, w);
            pSrc = (char *)pSrc + pATI->FBPitch;
            pDst = (char *)pDst + pATI->FBPitch;
        }

        pBox++;
    }
}

/*
 * ATIScreenInit --
 *
 * This function is called by DIX to initialise the screen.
 */
Bool
ATIScreenInit
(
    int       iScreen,
    ScreenPtr pScreen,
    int       argc,
    char      **argv
)
{
    ScrnInfoPtr  pScreenInfo = xf86Screens[iScreen];
    ATIPtr       pATI        = ATIPTR(pScreenInfo);
    pointer      pFB;
    int          VisualMask;

    /* Set video hardware state */
    if (!ATIEnterGraphics(pScreen, pScreenInfo, pATI))
        return FALSE;

    /* Re-initialise mi's visual list */
    miClearVisualTypes();

    if ((pATI->depth > 8) && (pATI->DAC == ATI_DAC_INTERNAL))
        VisualMask = TrueColorMask;
    else
        VisualMask = miGetDefaultVisualMask(pATI->depth);

    if (!miSetVisualTypes(pATI->depth, VisualMask, pATI->rgbBits,
                          pScreenInfo->defaultVisual))
        return FALSE;

    if (!miSetPixmapDepths())
        return FALSE;

    pFB = pATI->pMemory;
    if (pATI->OptionShadowFB)
    {
        pATI->FBBytesPerPixel = pATI->bitsPerPixel >> 3;
        pATI->FBPitch = PixmapBytePad(pATI->displayWidth, pATI->depth);
        if ((pATI->pShadow = xalloc(pATI->FBPitch * pScreenInfo->virtualY)))
            pFB = pATI->pShadow;
        else
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Insufficient virtual memory for shadow frame buffer.\n");
            pATI->OptionShadowFB = FALSE;
        }
    }

    /* Initialise framebuffer layer */
    switch (pATI->bitsPerPixel)
    {

#ifndef AVOID_CPIO

        case 1:
            pATI->Closeable = xf1bppScreenInit(pScreen, pFB,
                pScreenInfo->virtualX, pScreenInfo->virtualY,
                pScreenInfo->xDpi, pScreenInfo->yDpi, pATI->displayWidth);
            break;

        case 4:
            pATI->Closeable = xf4bppScreenInit(pScreen, pFB,
                pScreenInfo->virtualX, pScreenInfo->virtualY,
                pScreenInfo->xDpi, pScreenInfo->yDpi, pATI->displayWidth);
            break;

#endif /* AVOID_CPIO */

        case 8:
        case 16:
        case 24:
        case 32:
            pATI->Closeable = fbScreenInit(pScreen, pFB,
                pScreenInfo->virtualX, pScreenInfo->virtualY,
                pScreenInfo->xDpi, pScreenInfo->yDpi, pATI->displayWidth,
                pATI->bitsPerPixel);
            break;

        default:
            return FALSE;
    }

    if (!pATI->Closeable)
        return FALSE;

    /* Fixup RGB ordering */
    if (pATI->depth > 8)
    {
        VisualPtr pVisual = pScreen->visuals + pScreen->numVisuals;

        while (--pVisual >= pScreen->visuals)
        {
            if ((pVisual->class | DynamicClass) != DirectColor)
                continue;

            pVisual->offsetRed = pScreenInfo->offset.red;
            pVisual->offsetGreen = pScreenInfo->offset.green;
            pVisual->offsetBlue = pScreenInfo->offset.blue;

            pVisual->redMask = pScreenInfo->mask.red;
            pVisual->greenMask = pScreenInfo->mask.green;
            pVisual->blueMask = pScreenInfo->mask.blue;
        }
    }

    /* If applicable, initialise RENDER extension */
    if (pATI->bitsPerPixel > 4)
    {
        if (pATI->OptionShadowFB)
        {
            if (serverGeneration == 1)
                xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                    "RENDER extension not supported with a shadowed"
                    " framebuffer.\n");
        }

#ifndef AVOID_CPIO

        else if (pATI->BankInfo.BankSize)
        {
            if (serverGeneration == 1)
                xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                    "RENDER extension not supported with a banked"
                    " framebuffer.\n");
        }

#endif /* AVOID_CPIO */

        else if (!fbPictureInit(pScreen, NULL, 0) &&
                 (serverGeneration == 1))
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "RENDER extension initialisation failed.\n");
    }

    xf86SetBlackWhitePixels(pScreen);

#ifndef AVOID_CPIO

    /* Initialise banking if needed */
    if (!miInitializeBanking(pScreen,
                             pScreenInfo->virtualX, pScreenInfo->virtualY,
                             pATI->displayWidth, &pATI->BankInfo))
        return FALSE;

#endif /* AVOID_CPIO */

    /* Setup acceleration */
    if (!ATIInitializeAcceleration(pScreen, pScreenInfo, pATI))
        return FALSE;

#ifndef AVOID_DGA

    /* Initialise DGA support */
    (void)ATIDGAInit(pScreenInfo, pScreen, pATI);

#endif /* AVOID_DGA */

    /* Initialise backing store */
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);

    /* Initialise cursor */
    if (!ATIInitializeCursor(pScreen, pATI))
        return FALSE;

    /* Create default colourmap */
    if (!miCreateDefColormap(pScreen))
        return FALSE;

#ifdef AVOID_CPIO

    if (!xf86HandleColormaps(pScreen, 256, pATI->rgbBits, ATILoadPalette, NULL,
                             CMAP_PALETTED_TRUECOLOR |
                             CMAP_LOAD_EVEN_IF_OFFSCREEN))
            return FALSE;

#else /* AVOID_CPIO */

    if (pATI->depth > 1)
        if (!xf86HandleColormaps(pScreen, (pATI->depth == 4) ? 16 : 256,
                                 pATI->rgbBits, ATILoadPalette, NULL,
                                 CMAP_PALETTED_TRUECOLOR |
                                 CMAP_LOAD_EVEN_IF_OFFSCREEN))
            return FALSE;

#endif /* AVOID_CPIO */

    /* Initialise shadow framebuffer */
    if (pATI->OptionShadowFB &&
        !ShadowFBInit(pScreen, ATIRefreshArea))
        return FALSE;

    /* Initialise DPMS support */
    (void)xf86DPMSInit(pScreen, ATISetDPMSMode, 0);

    /* Initialise XVideo support */
    (void)ATIInitializeXVideo(pScreen, pScreenInfo, pATI);

    /* Set pScreen->SaveScreen and wrap CloseScreen vector */
    pScreen->SaveScreen = ATISaveScreen;
    pATI->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = ATICloseScreen;

    if (serverGeneration == 1)
        xf86ShowUnusedOptions(pScreenInfo->scrnIndex, pScreenInfo->options);

    return TRUE;
}

/*
 * ATICloseScreen --
 *
 * This function is called by DIX to close the screen.
 */
Bool
ATICloseScreen
(
    int       iScreen,
    ScreenPtr pScreen
)
{
    ScrnInfoPtr pScreenInfo = xf86Screens[iScreen];
    ATIPtr      pATI        = ATIPTR(pScreenInfo);
    Bool        Closed      = TRUE;

    if (pATI->pXAAInfo)
    {
        XAADestroyInfoRec(pATI->pXAAInfo);
        pATI->pXAAInfo = NULL;
    }

    if ((pScreen->CloseScreen = pATI->CloseScreen))
    {
        pATI->CloseScreen = NULL;
        Closed = (*pScreen->CloseScreen)(iScreen, pScreen);
    }

    pATI->Closeable = FALSE;

    if (pATI->pCursorInfo)
    {
        xf86DestroyCursorInfoRec(pATI->pCursorInfo);
        pATI->pCursorInfo = NULL;
    }

    ATILeaveGraphics(pScreenInfo, pATI);

    xfree(pATI->ExpansionBitmapScanlinePtr[1]);
    pATI->ExpansionBitmapScanlinePtr[0] =
        pATI->ExpansionBitmapScanlinePtr[1] = NULL;

    xfree(pATI->pShadow);
    pATI->pShadow = NULL;
    pScreenInfo->pScreen = NULL;

    return Closed;
}
