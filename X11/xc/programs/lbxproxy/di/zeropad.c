/* $Xorg: zeropad.c,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */

/*

Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

/*
 * This module handles zeroing out unused pad bytes in X requests.
 * This will hopefully improve both stream and delta compression,
 * since we are removing the random values in pad bytes.
 */

#include <X11/Xproto.h>

static void
ZeroEmptyReq (req)
    xReq *req;
{
    req->data = 0;
}

#define ZeroGetInputFocusReq ZeroEmptyReq
#define ZeroGetFontPathReq ZeroEmptyReq
#define ZeroGetKeyboardControlReq ZeroEmptyReq
#define ZeroGetPointerControlReq ZeroEmptyReq
#define ZeroGetPointerMappingReq ZeroEmptyReq
#define ZeroGetScreenSaverReq ZeroEmptyReq
#define ZeroGrabServerReq ZeroEmptyReq
#define ZeroListExtensionsReq ZeroEmptyReq
#define ZeroNoOperationReq ZeroEmptyReq
#define ZeroGetModifierMappingReq ZeroEmptyReq
#define ZeroQueryKeymapReq ZeroEmptyReq
#define ZeroUngrabServerReq ZeroEmptyReq


static void
ZeroResourceReq (req)
    xResourceReq *req;
{
    req->pad = 0;
}

#define ZeroFreePixmapReq ZeroResourceReq
#define ZeroGetAtomNameReq ZeroResourceReq
#define ZeroDestroySubwindowsReq ZeroResourceReq
#define ZeroDestroyWindowReq ZeroResourceReq
#define ZeroCloseFontReq ZeroResourceReq
#define ZeroQueryFontReq ZeroResourceReq
#define ZeroFreeCursorReq ZeroResourceReq
#define ZeroFreeGCReq ZeroResourceReq
#define ZeroGetGeometryReq ZeroResourceReq
#define ZeroGetSelectionOwnerReq ZeroResourceReq
#define ZeroGetWindowAttributesReq ZeroResourceReq
#define ZeroInstallColormapReq ZeroResourceReq
#define ZeroListInstalledColormapsReq ZeroResourceReq
#define ZeroListPropertiesReq ZeroResourceReq
#define ZeroMapSubwindowsReq ZeroResourceReq
#define ZeroMapWindowReq ZeroResourceReq
#define ZeroQueryPointerReq ZeroResourceReq
#define ZeroQueryTreeReq ZeroResourceReq
#define ZeroUngrabKeyboardReq ZeroResourceReq
#define ZeroUngrabPointerReq ZeroResourceReq
#define ZeroUninstallColormapReq ZeroResourceReq
#define ZeroUnmapSubwindowsReq ZeroResourceReq
#define ZeroUnmapWindowReq ZeroResourceReq
#define ZeroKillClientReq ZeroResourceReq
#define ZeroFreeColormapReq ZeroResourceReq


static void
ZeroChangeWindowAttributesReq (req)
    xChangeWindowAttributesReq *req;
{
    req->pad = 0;
}

static void
ZeroReparentWindowReq (req)
    xReparentWindowReq *req;
{
    req->pad = 0;
}

static void
ZeroConfigureWindowReq (req)
    xConfigureWindowReq *req;
{
    req->pad = 0;
    req->pad2 = 0;
}

static void
ZeroInternAtomReq (req)
    xInternAtomReq *req;
{
    req->pad = 0;
}

static void
ZeroChangePropertyReq (req)
    xChangePropertyReq *req;
{
    req->pad[0] = 0;
    req->pad[1] = 0;
    req->pad[2] = 0;
}

static void
ZeroDeletePropertyReq (req)
    xDeletePropertyReq *req;
{
    req->pad = 0;
}

static void
ZeroSetSelectionOwnerReq (req)
    xSetSelectionOwnerReq *req;
{
    req->pad = 0;
}

static void
ZeroConvertSelectionReq (req)
    xConvertSelectionReq *req;
{
    req->pad = 0;
}

static void
ZeroGrabButtonReq (req)
    xGrabButtonReq *req;
{
    req->pad = 0;
}

static void
ZeroUngrabButtonReq (req)
    xUngrabButtonReq *req;
{
    req->pad = 0;
}

static void
ZeroChangeActivePointerGrabReq (req)
    xChangeActivePointerGrabReq *req;
{
    req->pad = 0;
    req->pad2 = 0;
}

static void
ZeroGrabKeyboardReq (req)
    xGrabKeyboardReq *req;
{
    req->pad = 0;
}

static void
ZeroGrabKeyReq (req)
    xGrabKeyReq *req;
{
    req->pad1 = 0;
    req->pad2 = 0;
    req->pad3  = 0;
}

static void
ZeroUngrabKeyReq (req)
    xUngrabKeyReq *req;
{
    req->pad = 0;
}

static void
ZeroGetMotionEventsReq (req)
    xGetMotionEventsReq *req;
{
    req->pad = 0;
}

static void
ZeroTranslateCoordsReq (req)
    xTranslateCoordsReq *req;
{
    req->pad = 0;
}

static void
ZeroWarpPointerReq (req)
    xWarpPointerReq *req;
{
    req->pad = 0;
}

static void
ZeroOpenFontReq (req)
    xOpenFontReq *req;
{
    req->pad = 0;
    req->pad1 = 0;
    req->pad2 = 0;
}

static void
ZeroListFontsReq (req)
    xListFontsReq *req;
{
    req->pad = 0;
}

#define ZeroListFontsWithInfoReq ZeroListFontsReq

static void
ZeroSetFontPathReq (req)
    xSetFontPathReq *req;
{
    req->pad = 0;
    req->pad1 = 0;
    req->pad2 = 0;
}

static void
ZeroCreateGCReq (req)
    xCreateGCReq *req;
{
    req->pad = 0;
}

static void
ZeroChangeGCReq (req)
    xChangeGCReq *req;
{
    req->pad = 0;
}    

static void
ZeroCopyGCReq (req)
    xCopyGCReq *req;
{
    req->pad = 0;
}    

static void
ZeroSetDashesReq (req)
    xSetDashesReq *req;
{
    req->pad = 0;
}    

static void
ZeroCopyAreaReq (req)
    xCopyAreaReq *req;
{
    req->pad = 0;
}    

static void
ZeroCopyPlaneReq (req)
    xCopyPlaneReq *req;
{
    req->pad = 0;
}    

static void
ZeroPolySegmentReq (req)
    xPolySegmentReq *req;
{
    req->pad = 0;
}    

#define ZeroPolyArcReq ZeroPolySegmentReq
#define ZeroPolyRectangleReq ZeroPolySegmentReq
#define ZeroPolyFillRectangleReq ZeroPolySegmentReq
#define ZeroPolyFillArcReq ZeroPolySegmentReq

static void
ZeroFillPolyReq (req)
    xFillPolyReq *req;
{
    req->pad = 0;
    req->pad1 = 0;
}    

static void
ZeroPutImageReq (req)
    xPutImageReq *req;
{
    req->pad = 0;
}    

static void
ZeroPolyTextReq (req)
    xPolyTextReq *req;
{
    req->pad = 0;
}    

#define ZeroPolyText8Req ZeroPolyTextReq
#define ZeroPolyText16Req ZeroPolyTextReq

static void
ZeroCopyColormapAndFreeReq (req)
    xCopyColormapAndFreeReq *req;
{
    req->pad = 0;
}    

static void
ZeroAllocColorReq (req)
    xAllocColorReq *req;
{
    req->pad = 0;
    req->pad2 = 0;
}    

static void
ZeroAllocNamedColorReq (req)
    xAllocNamedColorReq *req;
{
    req->pad = 0;
    req->pad1 = 0;
    req->pad2 = 0;
}    

static void
ZeroFreeColorsReq (req)
    xFreeColorsReq *req;
{
    req->pad = 0;
}    

static void
ZeroStoreColorsReq (req)
    xStoreColorsReq *req;
{
    req->pad = 0;
}    

static void
ZeroStoreNamedColorReq (req)
    xStoreNamedColorReq *req;
{
    req->pad1 = 0;
    req->pad2 = 0;
}

static void
ZeroQueryColorsReq (req)
    xQueryColorsReq *req;
{
    req->pad = 0;
}    

static void
ZeroLookupColorReq (req)
    xLookupColorReq *req;
{
    req->pad = 0;
    req->pad1 = 0;
    req->pad2 = 0;
}    

static void
ZeroCreateCursorReq (req)
    xCreateCursorReq *req;
{
    req->pad = 0;
}    

static void
ZeroCreateGlyphCursorReq (req)
    xCreateGlyphCursorReq *req;
{
    req->pad = 0;
}    

static void
ZeroRecolorCursorReq (req)
    xRecolorCursorReq *req;
{
    req->pad = 0;
}    

static void
ZeroQueryExtensionReq (req)
    xQueryExtensionReq *req;
{
    req->pad = 0;
    req->pad1 = 0;
    req->pad2 = 0;
}

static void
ZeroGetKeyboardMappingReq (req)
    xGetKeyboardMappingReq *req;
{
    req->pad = 0;
    req->pad1 = 0;
}    

static void
ZeroChangeKeyboardMappingReq (req)
    xChangeKeyboardMappingReq *req;
{
    req->pad1 = 0;
}

static void
ZeroChangeKeyboardControlReq (req)
    xChangeKeyboardControlReq *req;
{
    req->pad = 0;
}    

static void
ZeroChangePointerControlReq (req)
    xChangePointerControlReq *req;
{
    req->pad = 0;
}    

static void
ZeroSetScreenSaverReq (req)
    xSetScreenSaverReq *req;
{
    req->pad = 0;
    req->pad2 = 0;
}    

static void
ZeroChangeHostsReq (req)
    xChangeHostsReq *req;
{
    req->pad = 0;
}    

static void
ZeroListHostsReq (req)
    xListHostsReq *req;
{
    req->pad = 0;
}

static void
ZeroRotatePropertiesReq (req)
    xRotatePropertiesReq *req;
{
    req->pad = 0;
}

void (*ZeroPadReqVector[128]) () =
{
    0,
    0, /* CreateWindowReq */
    ZeroChangeWindowAttributesReq,
    ZeroGetWindowAttributesReq,
    ZeroDestroyWindowReq,
    ZeroDestroySubwindowsReq,		/* 5 */
    0, /* ChangeSaveSetReq */
    ZeroReparentWindowReq,
    ZeroMapWindowReq,
    ZeroMapSubwindowsReq,
    ZeroUnmapWindowReq,			/* 10 */
    ZeroUnmapSubwindowsReq,
    ZeroConfigureWindowReq,
    0, /* CirculateWindowReq */
    ZeroGetGeometryReq,
    ZeroQueryTreeReq,			/* 15 */
    ZeroInternAtomReq,
    ZeroGetAtomNameReq,
    ZeroChangePropertyReq,
    ZeroDeletePropertyReq,
    0, /* GetPropertyReq */		/* 20 */
    ZeroListPropertiesReq,
    ZeroSetSelectionOwnerReq,
    ZeroGetSelectionOwnerReq,
    ZeroConvertSelectionReq,
    0, /* SendEventReq */		/* 25 */
    0, /* GrabPointerReq */
    ZeroUngrabPointerReq,
    ZeroGrabButtonReq,
    ZeroUngrabButtonReq,
    ZeroChangeActivePointerGrabReq,	/* 30 */
    ZeroGrabKeyboardReq,
    ZeroUngrabKeyboardReq,
    ZeroGrabKeyReq,
    ZeroUngrabKeyReq,
    0, /* AllowEventsReq */		/* 35 */
    ZeroGrabServerReq,
    ZeroUngrabServerReq,
    ZeroQueryPointerReq,
    ZeroGetMotionEventsReq,
    ZeroTranslateCoordsReq,		/* 40 */
    ZeroWarpPointerReq,
    0, /* SetInputFocusReq */
    ZeroGetInputFocusReq,
    ZeroQueryKeymapReq,
    ZeroOpenFontReq,			/* 45 */
    ZeroCloseFontReq,
    ZeroQueryFontReq,
    0, /* QueryTextExtentsReq */
    ZeroListFontsReq,
    ZeroListFontsWithInfoReq,		/* 50 */
    ZeroSetFontPathReq,
    ZeroGetFontPathReq,
    0, /* CreatePixmapReq */
    ZeroFreePixmapReq,
    ZeroCreateGCReq,			/* 55 */
    ZeroChangeGCReq,
    ZeroCopyGCReq,
    ZeroSetDashesReq,
    0, /* SetClipRectanglesReq */
    ZeroFreeGCReq,			/* 60 */
    0, /* ClearToBackgroundReq */
    ZeroCopyAreaReq,
    ZeroCopyPlaneReq,
    0, /* PolyPointReq */
    0, /* PolyLineReq */		/* 65 */
    ZeroPolySegmentReq,
    ZeroPolyRectangleReq,
    ZeroPolyArcReq,
    ZeroFillPolyReq,
    ZeroPolyFillRectangleReq,		/* 70 */
    ZeroPolyFillArcReq,
    ZeroPutImageReq,
    0, /* GetImageReq */
    ZeroPolyText8Req,
    ZeroPolyText16Req,			/* 75 */
    0, /* ImageText8Req */
    0, /* ImageText16Req */
    0, /* CreateColormapReq */
    ZeroFreeColormapReq,
    ZeroCopyColormapAndFreeReq,		/* 80 */
    ZeroInstallColormapReq,
    ZeroUninstallColormapReq,
    ZeroListInstalledColormapsReq,
    ZeroAllocColorReq,
    ZeroAllocNamedColorReq,		/* 85 */
    0, /* AllocColorCellsReq */
    0, /* AllocColorPlanesReq */
    ZeroFreeColorsReq,
    ZeroStoreColorsReq,
    ZeroStoreNamedColorReq,		/* 90 */
    ZeroQueryColorsReq,
    ZeroLookupColorReq,
    ZeroCreateCursorReq,
    ZeroCreateGlyphCursorReq,
    ZeroFreeCursorReq,			/* 95 */
    ZeroRecolorCursorReq,
    0, /* QueryBestSizeReq */
    ZeroQueryExtensionReq,
    ZeroListExtensionsReq,
    ZeroChangeKeyboardMappingReq,	/* 100 */
    ZeroGetKeyboardMappingReq,
    ZeroChangeKeyboardControlReq,
    ZeroGetKeyboardControlReq,
    0, /* BellReq */
    ZeroChangePointerControlReq,	/* 105 */
    ZeroGetPointerControlReq,
    ZeroSetScreenSaverReq,
    ZeroGetScreenSaverReq,
    ZeroChangeHostsReq,
    ZeroListHostsReq,			/* 110 */
    0, /* ChangeAccessControlReq */
    0, /* ChangeCloseDownModeReq */
    ZeroKillClientReq,
    ZeroRotatePropertiesReq,
    0, /* ForceScreenSaverReq */	/* 115 */
    0, /* SetPointerMappingReq */
    ZeroGetPointerMappingReq,
    0, /* SetModifierMappingReq */
    ZeroGetModifierMappingReq,
    0,					/* 120 */
    0,
    0,
    0,
    0,
    0,					/* 125 */
    0,
    ZeroNoOperationReq
};
