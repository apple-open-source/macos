/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/radeon_probe.h,v 1.14 2003/11/10 18:41:23 tsi Exp $ */
/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *
 * Modified by Marc Aurele La France <tsi@xfree86.org> for ATI driver merge.
 */

#ifndef _RADEON_PROBE_H_
#define _RADEON_PROBE_H_ 1

#include "atiproto.h"

#include "xf86str.h"
#include "xf86DDC.h"

#define _XF86MISC_SERVER_
#include "xf86misc.h"

typedef struct
{
    Bool HasSecondary;

    /*
     * The next two are used to make sure CRTC2 is restored before CRTC_EXT,
     * otherwise it could lead to blank screens.
     */
    Bool IsSecondaryRestored;
    Bool RestorePrimary;

    ScrnInfoPtr pSecondaryScrn;
    ScrnInfoPtr pPrimaryScrn;

    int MonType1;
    int MonType2;
    xf86MonPtr MonInfo1;
    xf86MonPtr MonInfo2;
    Bool ReversedDAC;	  /* TVDAC used as primary dac */
    Bool ReversedTMDS;    /* DDC_DVI is used for external TMDS */
} RADEONEntRec, *RADEONEntPtr;

/* radeon_probe.c */
extern const OptionInfoRec *RADEONAvailableOptions
			    FunctionPrototype((int, int));
extern void                 RADEONIdentify
			    FunctionPrototype((int));
extern Bool                 RADEONProbe
			    FunctionPrototype((DriverPtr, int));

extern SymTabRec            RADEONChipsets[];
extern PciChipsets          RADEONPciChipsets[];

/* radeon_driver.c */
extern void                 RADEONLoaderRefSymLists
			    FunctionPrototype((void));
extern Bool                 RADEONPreInit
			    FunctionPrototype((ScrnInfoPtr, int));
extern Bool                 RADEONScreenInit
			    FunctionPrototype((int, ScreenPtr, int, char **));
extern Bool                 RADEONSwitchMode
			    FunctionPrototype((int, DisplayModePtr, int));
#ifdef X_XF86MiscPassMessage
extern Bool                 RADEONHandleMessage
			    FunctionPrototype((int, const char*, const char*,
					       char**));
#endif
extern void                 RADEONAdjustFrame
			    FunctionPrototype((int, int, int, int));
extern Bool                 RADEONEnterVT
			    FunctionPrototype((int, int));
extern void                 RADEONLeaveVT
			    FunctionPrototype((int, int));
extern void                 RADEONFreeScreen
			    FunctionPrototype((int, int));
extern ModeStatus           RADEONValidMode
			    FunctionPrototype((int, DisplayModePtr, Bool,
					       int));

extern const OptionInfoRec  RADEONOptions[];

#endif /* _RADEON_PROBE_H_ */
