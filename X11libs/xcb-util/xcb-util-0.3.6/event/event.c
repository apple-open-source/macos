/*
 * Copyright © 2008-2009 Julien Danjou <julien@danjou.info>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the names of the authors or
 * their institutions shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without
 * prior written authorization from the authors.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "xcb_event.h"
#include "../xcb-util-common.h"

void
xcb_event_handlers_init(xcb_connection_t *c, xcb_event_handlers_t *evenths)
{
    memset(evenths, 0, sizeof(xcb_event_handlers_t));
    evenths->c = c;
}

xcb_connection_t *
xcb_event_get_xcb_connection(xcb_event_handlers_t *evenths)
{
    return evenths->c;
}

static xcb_event_handler_t *
get_event_handler(xcb_event_handlers_t *evenths, int event)
{
    assert(event < 256);
    event &= XCB_EVENT_RESPONSE_TYPE_MASK;
    assert(event >= 2);
    return &evenths->event[event - 2];
}

static xcb_event_handler_t *
get_error_handler(xcb_event_handlers_t *evenths, int error)
{
    assert(error >= 0 && error < 256);
    return &evenths->error[error];
}

int
xcb_event_handle(xcb_event_handlers_t *evenths, xcb_generic_event_t *event)
{
    xcb_event_handler_t *eventh = 0;
    assert(event->response_type != 1);

    if(event->response_type == 0)
        eventh = get_error_handler(evenths, ((xcb_generic_error_t *) event)->error_code);
    else
        eventh = get_event_handler(evenths, event->response_type);

    if(eventh->handler)
        return eventh->handler(eventh->data, evenths->c, event);
    return 0;
}

void
xcb_event_wait_for_event_loop(xcb_event_handlers_t *evenths)
{
    xcb_generic_event_t *event;
    while((event = xcb_wait_for_event(evenths->c)))
    {
        xcb_event_handle(evenths, event);
        free(event);
    }
}

void
xcb_event_poll_for_event_loop(xcb_event_handlers_t *evenths)
{
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(evenths->c)))
    {
        xcb_event_handle(evenths, event);
        free(event);
    }
}

static void
set_handler(xcb_generic_event_handler_t handler, void *data, xcb_event_handler_t *place)
{
    xcb_event_handler_t eventh = { handler, data };
    *place = eventh;
}

void
xcb_event_set_handler(xcb_event_handlers_t *evenths, int event, xcb_generic_event_handler_t handler, void *data)
{
    set_handler(handler, data, get_event_handler(evenths, event));
}

void
xcb_event_set_error_handler(xcb_event_handlers_t *evenths, int error, xcb_generic_error_handler_t handler, void *data)
{
    set_handler((xcb_generic_event_handler_t) handler, data, get_error_handler(evenths, error));
}

static const char *labelError[] =
{
    "Success",
    "BadRequest",
    "BadValue",
    "BadWindow",
    "BadPixmap",
    "BadAtom",
    "BadCursor",
    "BadFont",
    "BadMatch",
    "BadDrawable",
    "BadAccess",
    "BadAlloc",
    "BadColor",
    "BadGC",
    "BadIDChoice",
    "BadName",
    "BadLength",
    "BadImplementation",
};

static const char *labelRequest[] =
{
    "no request",
    "CreateWindow",
    "ChangeWindowAttributes",
    "GetWindowAttributes",
    "DestroyWindow",
    "DestroySubwindows",
    "ChangeSaveSet",
    "ReparentWindow",
    "MapWindow",
    "MapSubwindows",
    "UnmapWindow",
    "UnmapSubwindows",
    "ConfigureWindow",
    "CirculateWindow",
    "GetGeometry",
    "QueryTree",
    "InternAtom",
    "GetAtomName",
    "ChangeProperty",
    "DeleteProperty",
    "GetProperty",
    "ListProperties",
    "SetSelectionOwner",
    "GetSelectionOwner",
    "ConvertSelection",
    "SendEvent",
    "GrabPointer",
    "UngrabPointer",
    "GrabButton",
    "UngrabButton",
    "ChangeActivePointerGrab",
    "GrabKeyboard",
    "UngrabKeyboard",
    "GrabKey",
    "UngrabKey",
    "AllowEvents",
    "GrabServer",
    "UngrabServer",
    "QueryPointer",
    "GetMotionEvents",
    "TranslateCoords",
    "WarpPointer",
    "SetInputFocus",
    "GetInputFocus",
    "QueryKeymap",
    "OpenFont",
    "CloseFont",
    "QueryFont",
    "QueryTextExtents",
    "ListFonts",
    "ListFontsWithInfo",
    "SetFontPath",
    "GetFontPath",
    "CreatePixmap",
    "FreePixmap",
    "CreateGC",
    "ChangeGC",
    "CopyGC",
    "SetDashes",
    "SetClipRectangles",
    "FreeGC",
    "ClearArea",
    "CopyArea",
    "CopyPlane",
    "PolyPoint",
    "PolyLine",
    "PolySegment",
    "PolyRectangle",
    "PolyArc",
    "FillPoly",
    "PolyFillRectangle",
    "PolyFillArc",
    "PutImage",
    "GetImage",
    "PolyText",
    "PolyText",
    "ImageText",
    "ImageText",
    "CreateColormap",
    "FreeColormap",
    "CopyColormapAndFree",
    "InstallColormap",
    "UninstallColormap",
    "ListInstalledColormaps",
    "AllocColor",
    "AllocNamedColor",
    "AllocColorCells",
    "AllocColorPlanes",
    "FreeColors",
    "StoreColors",
    "StoreNamedColor",
    "QueryColors",
    "LookupColor",
    "CreateCursor",
    "CreateGlyphCursor",
    "FreeCursor",
    "RecolorCursor",
    "QueryBestSize",
    "QueryExtension",
    "ListExtensions",
    "ChangeKeyboardMapping",
    "GetKeyboardMapping",
    "ChangeKeyboardControl",
    "GetKeyboardControl",
    "Bell",
    "ChangePointerControl",
    "GetPointerControl",
    "SetScreenSaver",
    "GetScreenSaver",
    "ChangeHosts",
    "ListHosts",
    "SetAccessControl",
    "SetCloseDownMode",
    "KillClient",
    "RotateProperties",
    "ForceScreenSaver",
    "SetPointerMapping",
    "GetPointerMapping",
    "SetModifierMapping",
    "GetModifierMapping",
    "major 120",
    "major 121",
    "major 122",
    "major 123",
    "major 124",
    "major 125",
    "major 126",
    "NoOperation",
};

static const char *labelEvent[] =
{
    "error",
    "reply",
    "KeyPress",
    "KeyRelease",
    "ButtonPress",
    "ButtonRelease",
    "MotionNotify",
    "EnterNotify",
    "LeaveNotify",
    "FocusIn",
    "FocusOut",
    "KeymapNotify",
    "Expose",
    "GraphicsExpose",
    "NoExpose",
    "VisibilityNotify",
    "CreateNotify",
    "DestroyNotify",
    "UnmapNotify",
    "MapNotify",
    "MapRequest",
    "ReparentNotify",
    "ConfigureNotify",
    "ConfigureRequest",
    "GravityNotify",
    "ResizeRequest",
    "CirculateNotify",
    "CirculateRequest",
    "PropertyNotify",
    "SelectionClear",
    "SelectionRequest",
    "SelectionNotify",
    "ColormapNotify",
    "ClientMessage",
    "MappingNotify",
};

const char *
xcb_event_get_label(uint8_t type)
{
    if(type < countof(labelEvent))
        return labelEvent[type];
    return NULL;
}

const char *
xcb_event_get_error_label(uint8_t type)
{
    if(type < countof(labelError))
        return labelError[type];
    return NULL;
}

const char *
xcb_event_get_request_label(uint8_t type)
{
    if(type < countof(labelRequest))
        return labelRequest[type];
    return NULL;
}
