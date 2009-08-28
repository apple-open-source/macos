/*
 * Copyright © 2008 Bart Massey	<bart@cs.pdx.edu>
 * Copyright © 2008 Jamey Sharp	<jamey@minilop.net>
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
#include <stdio.h>
#include "reply_formats.h"

#define WINFMT "0x%08x"

int format_get_window_attributes_reply(xcb_window_t wid, xcb_get_window_attributes_reply_t *reply)
{
    if(!reply)
    {
        fprintf(stderr, "Failed to get attributes for window " WINFMT ".\n",
            (unsigned int) wid);
        return 0;
    }

    printf("Window " WINFMT " has attributes:\n"
           "    backingStore       = %d\n"
           "    visualID           = %#x\n"
           "    class              = %d\n"
           "    bitGravity         = %d\n"
           "    winGravity         = %d\n"
           "    backingBitPlanes   = 0x%08x\n"
           "    backingPixel       = %d\n"
           "    saveUnder          = %d\n"
           "    mapInstalled       = %d\n"
           "    mapState           = %d\n"
           "    override           = %d\n"
           "    colormap           = 0x%08x\n"
           "    allEventMasks      = 0x%08x\n"
           "    yourEventMask      = 0x%08x\n"
           "    doNotPropagateMask = 0x%08x\n",
        (unsigned int) wid,
        reply->backing_store,
        (unsigned int) reply->visual,
        reply->_class,
        reply->bit_gravity,
        reply->win_gravity,
        reply->backing_planes,
        reply->backing_pixel,
        reply->save_under,
        reply->map_is_installed,
        reply->map_state,
        reply->override_redirect,
        (unsigned int) reply->colormap,
        (unsigned int) reply->all_event_masks,
        (unsigned int) reply->your_event_mask,
        reply->do_not_propagate_mask);

    fflush(stdout);
    return 1;
}

int format_get_geometry_reply(xcb_window_t wid, xcb_get_geometry_reply_t *reply)
{
    if(!reply)
    {
        fprintf(stderr, "Failed to get geometry for window " WINFMT ".\n",
            (unsigned int) wid);
        return 0;
    }

    printf("Geometry for window " WINFMT ": %dx%d%+d%+d\n",
        (unsigned int) wid,
        reply->width,
        reply->height,
        reply->x,
        reply->y);

    fflush(stdout);
    return 1;
}

int format_query_tree_reply(xcb_window_t wid, xcb_query_tree_reply_t *reply)
{
    int i;

    if(!reply)
    {
        fprintf(stderr, "Failed to query tree for window " WINFMT ".\n",
            (unsigned int) wid);
        return 0;
    }

    printf("Window " WINFMT " has parent " WINFMT ", root " WINFMT ", and %d children%c\n",
        (unsigned int) wid,
        (unsigned int) reply->parent,
        (unsigned int) reply->root,
        (unsigned int) reply->children_len,
        reply->children_len ? ':' : '.');

    for(i = 0; i < reply->children_len; ++i)
        printf("    window " WINFMT "\n",
            (unsigned int) xcb_query_tree_children(reply)[i]);

    fflush(stdout);
    return 1;
}

static const char *labelError[] = {
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

static const char *labelRequest[] = {
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

static const char *labelEvent[] = {
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

static const char *labelSendEvent[] = {
    "",
    " (from SendEvent)",
};

int format_event(xcb_generic_event_t *e)
{
    uint8_t sendEvent;
    uint16_t seqnum;

    sendEvent = (e->response_type & 0x80) ? 1 : 0;
    e->response_type &= ~0x80;
    seqnum = *((uint16_t *) e + 1);

    switch(e->response_type)
    {
    case 0:
        printf("Error %s on seqnum %d (%s).\n",
            labelError[*((uint8_t *) e + 1)],
            seqnum,
            labelRequest[*((uint8_t *) e + 10)]);
        break;
    default:
        printf("Event %s following seqnum %d%s.\n",
            labelEvent[e->response_type],
            seqnum,
            labelSendEvent[sendEvent]);
        break;
    case XCB_KEYMAP_NOTIFY:
        printf("Event %s%s.\n",
            labelEvent[e->response_type],
            labelSendEvent[sendEvent]);
        break;
    }

    fflush(stdout);
    return 1;
}
