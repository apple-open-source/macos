/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/aticursor.c,v 1.5 2004/01/05 16:42:01 tsi Exp $ */
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
#include "aticursor.h"
#include "atimach64cursor.h"
#include "atistruct.h"

#include "xf86.h"

/*
 * ATIInitializeCursor --
 *
 * This function initialises the screen cursor.
 */
Bool
ATIInitializeCursor
(
    ScreenPtr pScreen,
    ATIPtr    pATI
)
{
    /* Initialise software cursor */
    if (!miDCInitialize(pScreen, xf86GetPointerScreenFuncs()))
        return FALSE;

    if (pATI->Cursor == ATI_CURSOR_SOFTWARE)
        return TRUE;

    if (!(pATI->pCursorInfo = xf86CreateCursorInfoRec()))
        return FALSE;

    switch (pATI->Adapter)
    {
        case ATI_ADAPTER_MACH64:
            if (ATIMach64CursorInit(pATI->pCursorInfo))
                break;
            /* Fall through */

        default:
            xf86DestroyCursorInfoRec(pATI->pCursorInfo);
            pATI->pCursorInfo = NULL;
            return FALSE;
    }

    if (xf86InitCursor(pScreen, pATI->pCursorInfo))
    {
        xf86SetSilkenMouse(pScreen);
        return TRUE;
    }

    xf86DestroyCursorInfoRec(pATI->pCursorInfo);
    pATI->pCursorInfo = NULL;
    return FALSE;
}
