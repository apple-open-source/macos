/* $Xorg: reqtype.c,v 1.3 2000/08/17 19:53:56 cpqbld Exp $ */
/*
 * Copyright 1994 Network Computing Devices, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name Network Computing Devices, Inc. not be
 * used in advertising or publicity pertaining to distribution of this
 * software without specific, written prior permission.
 *
 * THIS SOFTWARE IS PROVIDED `AS-IS'.  NETWORK COMPUTING DEVICES, INC.,
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT
 * LIMITATION ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NONINFRINGEMENT.  IN NO EVENT SHALL NETWORK
 * COMPUTING DEVICES, INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA,
 * OR PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS OF
 * WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* decides what tupe of request it is */
#include	"misc.h"
#include        "os.h"		/* in server/include */
#include	"reqtype.h"
#include	"lbxext.h"

static char cause_replies[] = {
    FALSE,			/* BadRequest */
    FALSE,			/* CreateWindow */
    FALSE,			/* ChangeWindowAttributes */
    TRUE,			/* GetWindowAttributes */
    FALSE,			/* DestroyWindow */
    FALSE,			/* DestroySubwindows */
    FALSE,			/* ChangeSaveSet */
    FALSE,			/* ReparentWindow */
    FALSE,			/* MapWindow */
    FALSE,			/* MapSubwindows */
    FALSE,			/* UnmapWindow */
    FALSE,			/* UnmapSubwindows */
    FALSE,			/* ConfigureWindow */
    FALSE,			/* CirculateWindow */
    TRUE,			/* GetGeometry */
    TRUE,			/* QueryTree */
    TRUE,			/* InternAtom */
    TRUE,			/* GetAtomName */
    FALSE,			/* ChangeProperty */
    FALSE,			/* DeleteProperty */
    TRUE,			/* GetProperty */
    TRUE,			/* ListProperties */
    FALSE,			/* SetSelectionOwner */
    TRUE,			/* GetSelectionOwner */
    FALSE,			/* ConvertSelection */
    FALSE,			/* SendEvent */
    TRUE,			/* GrabPointer */
    FALSE,			/* UngrabPointer */
    FALSE,			/* GrabButton */
    FALSE,			/* UngrabButton */
    FALSE,			/* ChangeActivePointerGrab */
    TRUE,			/* GrabKeyboard */
    FALSE,			/* UngrabKeyboard */
    FALSE,			/* GrabKey */
    FALSE,			/* UngrabKey */
    FALSE,			/* AllowEvents */
    FALSE,			/* GrabServer */
    FALSE,			/* UngrabServer */
    TRUE,			/* QueryPointer */
    TRUE,			/* GetMotionEvents */
    TRUE,			/* TranslateCoords */
    FALSE,			/* WarpPointer */
    FALSE,			/* SetInputFocus */
    TRUE,			/* GetInputFocus */
    TRUE,			/* QueryKeymap */
    FALSE,			/* OpenFont */
    FALSE,			/* CloseFont */
    TRUE,			/* QueryFont */
    TRUE,			/* QueryTextExtents */
    TRUE,			/* ListFonts */
    TRUE,			/* ListFontsWithInfo */
    FALSE,			/* SetFontPath */
    TRUE,			/* GetFontPath */
    FALSE,			/* CreatePixmap */
    FALSE,			/* FreePixmap */
    FALSE,			/* CreateGC */
    FALSE,			/* ChangeGC */
    FALSE,			/* CopyGC */
    FALSE,			/* SetDashes */
    FALSE,			/* SetClipRectangles */
    FALSE,			/* FreeGC */
    FALSE,			/* ClearToBackground */
    FALSE,			/* CopyArea */
    FALSE,			/* CopyPlane */
    FALSE,			/* PolyPoint */
    FALSE,			/* PolyLine */
    FALSE,			/* PolySegment */
    FALSE,			/* PolyRectangle */
    FALSE,			/* PolyArc */
    FALSE,			/* FillPoly */
    FALSE,			/* PolyFillRectangle */
    FALSE,			/* PolyFillArc */
    FALSE,			/* PutImage */
    TRUE,			/* GetImage */
    FALSE,			/* PolyText */
    FALSE,			/* PolyText */
    FALSE,			/* ImageText8 */
    FALSE,			/* ImageText16 */
    FALSE,			/* CreateColormap */
    FALSE,			/* FreeColormap */
    FALSE,			/* CopyColormapAndFree */
    FALSE,			/* InstallColormap */
    FALSE,			/* UninstallColormap */
    TRUE,			/* ListInstalledColormaps */
    TRUE,			/* AllocColor */
    TRUE,			/* AllocNamedColor */
    TRUE,			/* AllocColorCells */
    TRUE,			/* AllocColorPlanes */
    FALSE,			/* FreeColors */
    FALSE,			/* StoreColors */
    FALSE,			/* StoreNamedColor */
    TRUE,			/* QueryColors */
    TRUE,			/* LookupColor */
    FALSE,			/* CreateCursor */
    FALSE,			/* CreateGlyphCursor */
    FALSE,			/* FreeCursor */
    FALSE,			/* RecolorCursor */
    TRUE,			/* QueryBestSize */
    TRUE,			/* QueryExtension */
    TRUE,			/* ListExtensions */
    FALSE,			/* ChangeKeyboardMapping */
    TRUE,			/* GetKeyboardMapping */
    FALSE,			/* ChangeKeyboardControl */
    TRUE,			/* GetKeyboardControl */
    FALSE,			/* Bell */
    FALSE,			/* ChangePointerControl */
    TRUE,			/* GetPointerControl */
    FALSE,			/* SetScreenSaver */
    TRUE,			/* GetScreenSaver */
    FALSE,			/* ChangeHosts */
    TRUE,			/* ListHosts */
    FALSE,			/* ChangeAccessControl */
    FALSE,			/* ChangeCloseDownMode */
    FALSE,			/* KillClient */
    FALSE,			/* RotateProperties */
    FALSE,			/* ForceScreenSaver */
    TRUE,			/* SetPointerMapping */
    TRUE,			/* GetPointerMapping */
    TRUE,			/* SetModifierMapping */
    TRUE,			/* GetModifierMapping */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE			/* NoOp */
};

static char cause_events[] = {
    FALSE,			/* BadRequest */
    TRUE,			/* CreateWindow */
    TRUE,			/* ChangeWindowAttributes */
    FALSE,			/* GetWindowAttributes */
    TRUE,			/* DestroyWindow */
    TRUE,			/* DestroySubwindows */
    FALSE,			/* ChangeSaveSet */
    TRUE,			/* ReparentWindow */
    TRUE,			/* MapWindow */
    TRUE,			/* MapSubwindows */
    TRUE,			/* UnmapWindow */
    TRUE,			/* UnmapSubwindows */
    TRUE,			/* ConfigureWindow */
    TRUE,			/* CirculateWindow */
    FALSE,			/* GetGeometry */
    FALSE,			/* QueryTree */
    FALSE,			/* InternAtom */
    FALSE,			/* GetAtomName */
    TRUE,			/* ChangeProperty */
    TRUE,			/* DeleteProperty */
    TRUE,			/* GetProperty */
    FALSE,			/* ListProperties */
    TRUE,			/* SetSelectionOwner */
    FALSE,			/* GetSelectionOwner */
    TRUE,			/* ConvertSelection */
    TRUE,			/* SendEvent */
    TRUE,			/* GrabPointer */
    TRUE,			/* UngrabPointer */
    FALSE,			/* GrabButton */
    FALSE,			/* UngrabButton */
    FALSE,			/* ChangeActivePointerGrab */
    TRUE,			/* GrabKeyboard */
    TRUE,			/* UngrabKeyboard */
    FALSE,			/* GrabKey */
    FALSE,			/* UngrabKey */
    TRUE,			/* AllowEvents */
    FALSE,			/* GrabServer */
    FALSE,			/* UngrabServer */
    FALSE,			/* QueryPointer */
    FALSE,			/* GetMotionEvents */
    FALSE,			/* TranslateCoords */
    TRUE,			/* WarpPointer */
    TRUE,			/* SetInputFocus */
    FALSE,			/* GetInputFocus */
    FALSE,			/* QueryKeymap */
    FALSE,			/* OpenFont */
    FALSE,			/* CloseFont */
    FALSE,			/* QueryFont */
    FALSE,			/* QueryTextExtents */
    FALSE,			/* ListFonts */
    FALSE,			/* ListFontsWithInfo */
    FALSE,			/* SetFontPath */
    FALSE,			/* GetFontPath */
    FALSE,			/* CreatePixmap */
    FALSE,			/* FreePixmap */
    FALSE,			/* CreateGC */
    FALSE,			/* ChangeGC */
    FALSE,			/* CopyGC */
    FALSE,			/* SetDashes */
    FALSE,			/* SetClipRectangles */
    FALSE,			/* FreeGC */
    TRUE,			/* ClearToBackground */
    TRUE,			/* CopyArea */
    TRUE,			/* CopyPlane */
    FALSE,			/* PolyPoint */
    FALSE,			/* PolyLine */
    FALSE,			/* PolySegment */
    FALSE,			/* PolyRectangle */
    FALSE,			/* PolyArc */
    FALSE,			/* FillPoly */
    FALSE,			/* PolyFillRectangle */
    FALSE,			/* PolyFillArc */
    FALSE,			/* PutImage */
    FALSE,			/* GetImage */
    FALSE,			/* PolyText */
    FALSE,			/* PolyText */
    FALSE,			/* ImageText8 */
    FALSE,			/* ImageText16 */
    FALSE,			/* CreateColormap */
    TRUE,			/* FreeColormap */
    FALSE,			/* CopyColormapAndFree */
    TRUE,			/* InstallColormap */
    TRUE,			/* UninstallColormap */
    FALSE,			/* ListInstalledColormaps */
    FALSE,			/* AllocColor */
    FALSE,			/* AllocNamedColor */
    FALSE,			/* AllocColorCells */
    FALSE,			/* AllocColorPlanes */
    FALSE,			/* FreeColors */
    FALSE,			/* StoreColors */
    FALSE,			/* StoreNamedColor */
    FALSE,			/* QueryColors */
    FALSE,			/* LookupColor */
    FALSE,			/* CreateCursor */
    FALSE,			/* CreateGlyphCursor */
    FALSE,			/* FreeCursor */
    FALSE,			/* RecolorCursor */
    FALSE,			/* QueryBestSize */
    FALSE,			/* QueryExtension */
    FALSE,			/* ListExtensions */
    TRUE,			/* ChangeKeyboardMapping */
    FALSE,			/* GetKeyboardMapping */
    FALSE,			/* ChangeKeyboardControl */
    FALSE,			/* GetKeyboardControl */
    FALSE,			/* Bell */
    FALSE,			/* ChangePointerControl */
    FALSE,			/* GetPointerControl */
    FALSE,			/* SetScreenSaver */
    FALSE,			/* GetScreenSaver */
    FALSE,			/* ChangeHosts */
    FALSE,			/* ListHosts */
    FALSE,			/* ChangeAccessControl */
    FALSE,			/* ChangeCloseDownMode */
    FALSE,			/* KillClient */
    TRUE,			/* RotateProperties */
    FALSE,			/* ForceScreenSaver */
    TRUE,			/* SetPointerMapping */
    FALSE,			/* GetPointerMapping */
    TRUE,			/* SetModifierMapping */
    FALSE,			/* GetModifierMapping */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE,			/* BadRequest */
    FALSE			/* NoOp */
};


Bool
GeneratesEvents(client, req)
    ClientPtr	client;
    xReq       *req;
{
    if (req->reqType > X_NoOperation)
	return CheckExtensionForEvents(client, req);
    return cause_events[req->reqType];
}

int
GeneratesReplies(client, req)
    ClientPtr	client;
    xReq       *req;
{
    if (req->reqType > X_NoOperation)
	return CheckExtensionForReplies(client, req);
    return (int) cause_replies[req->reqType];
}
