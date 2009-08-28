#include "module.h"
#include "except.h"
#include "constant.h"

int xpybConstant_modinit(PyObject *m)
{
    /* Basic constants */
    PyModule_AddIntConstant(m, "X_PROTOCOL", X_PROTOCOL);
    PyModule_AddIntConstant(m, "X_PROTOCOL_REVISION", X_PROTOCOL_REVISION);
    PyModule_AddIntConstant(m, "X_TCP_PORT", X_TCP_PORT);
    PyModule_AddIntConstant(m, "NONE", XCB_NONE);
    PyModule_AddIntConstant(m, "CopyFromParent", XCB_COPY_FROM_PARENT);
    PyModule_AddIntConstant(m, "CurrentTime", XCB_CURRENT_TIME);
    PyModule_AddIntConstant(m, "NoSymbol", XCB_NO_SYMBOL);

    /* Pre-defined atoms */
    PyModule_AddIntConstant(m, "XA_PRIMARY", XA_PRIMARY);
    PyModule_AddIntConstant(m, "XA_SECONDARY", XA_SECONDARY);
    PyModule_AddIntConstant(m, "XA_ARC", XA_ARC);
    PyModule_AddIntConstant(m, "XA_ATOM", XA_ATOM);
    PyModule_AddIntConstant(m, "XA_BITMAP", XA_BITMAP);
    PyModule_AddIntConstant(m, "XA_CARDINAL", XA_CARDINAL);
    PyModule_AddIntConstant(m, "XA_COLORMAP", XA_COLORMAP);
    PyModule_AddIntConstant(m, "XA_CURSOR", XA_CURSOR);
    PyModule_AddIntConstant(m, "XA_CUT_BUFFER0", XA_CUT_BUFFER0);
    PyModule_AddIntConstant(m, "XA_CUT_BUFFER1", XA_CUT_BUFFER1);
    PyModule_AddIntConstant(m, "XA_CUT_BUFFER2", XA_CUT_BUFFER2);
    PyModule_AddIntConstant(m, "XA_CUT_BUFFER3", XA_CUT_BUFFER3);
    PyModule_AddIntConstant(m, "XA_CUT_BUFFER4", XA_CUT_BUFFER4);
    PyModule_AddIntConstant(m, "XA_CUT_BUFFER5", XA_CUT_BUFFER5);
    PyModule_AddIntConstant(m, "XA_CUT_BUFFER6", XA_CUT_BUFFER6);
    PyModule_AddIntConstant(m, "XA_CUT_BUFFER7", XA_CUT_BUFFER7);
    PyModule_AddIntConstant(m, "XA_DRAWABLE", XA_DRAWABLE);
    PyModule_AddIntConstant(m, "XA_FONT", XA_FONT);
    PyModule_AddIntConstant(m, "XA_INTEGER", XA_INTEGER);
    PyModule_AddIntConstant(m, "XA_PIXMAP", XA_PIXMAP);
    PyModule_AddIntConstant(m, "XA_POINT", XA_POINT);
    PyModule_AddIntConstant(m, "XA_RECTANGLE", XA_RECTANGLE);
    PyModule_AddIntConstant(m, "XA_RESOURCE_MANAGER", XA_RESOURCE_MANAGER);
    PyModule_AddIntConstant(m, "XA_RGB_COLOR_MAP", XA_RGB_COLOR_MAP);
    PyModule_AddIntConstant(m, "XA_RGB_BEST_MAP", XA_RGB_BEST_MAP);
    PyModule_AddIntConstant(m, "XA_RGB_BLUE_MAP", XA_RGB_BLUE_MAP);
    PyModule_AddIntConstant(m, "XA_RGB_DEFAULT_MAP", XA_RGB_DEFAULT_MAP);
    PyModule_AddIntConstant(m, "XA_RGB_GRAY_MAP", XA_RGB_GRAY_MAP);
    PyModule_AddIntConstant(m, "XA_RGB_GREEN_MAP", XA_RGB_GREEN_MAP);
    PyModule_AddIntConstant(m, "XA_RGB_RED_MAP", XA_RGB_RED_MAP);
    PyModule_AddIntConstant(m, "XA_STRING", XA_STRING);
    PyModule_AddIntConstant(m, "XA_VISUALID", XA_VISUALID);
    PyModule_AddIntConstant(m, "XA_WINDOW", XA_WINDOW);
    PyModule_AddIntConstant(m, "XA_WM_COMMAND", XA_WM_COMMAND);
    PyModule_AddIntConstant(m, "XA_WM_HINTS", XA_WM_HINTS);
    PyModule_AddIntConstant(m, "XA_WM_CLIENT_MACHINE", XA_WM_CLIENT_MACHINE);
    PyModule_AddIntConstant(m, "XA_WM_ICON_NAME", XA_WM_ICON_NAME);
    PyModule_AddIntConstant(m, "XA_WM_ICON_SIZE", XA_WM_ICON_SIZE);
    PyModule_AddIntConstant(m, "XA_WM_NAME", XA_WM_NAME);
    PyModule_AddIntConstant(m, "XA_WM_NORMAL_HINTS", XA_WM_NORMAL_HINTS);
    PyModule_AddIntConstant(m, "XA_WM_SIZE_HINTS", XA_WM_SIZE_HINTS);
    PyModule_AddIntConstant(m, "XA_WM_ZOOM_HINTS", XA_WM_ZOOM_HINTS);
    PyModule_AddIntConstant(m, "XA_MIN_SPACE", XA_MIN_SPACE);
    PyModule_AddIntConstant(m, "XA_NORM_SPACE", XA_NORM_SPACE);
    PyModule_AddIntConstant(m, "XA_MAX_SPACE", XA_MAX_SPACE);
    PyModule_AddIntConstant(m, "XA_END_SPACE", XA_END_SPACE);
    PyModule_AddIntConstant(m, "XA_SUPERSCRIPT_X", XA_SUPERSCRIPT_X);
    PyModule_AddIntConstant(m, "XA_SUPERSCRIPT_Y", XA_SUPERSCRIPT_Y);
    PyModule_AddIntConstant(m, "XA_SUBSCRIPT_X", XA_SUBSCRIPT_X);
    PyModule_AddIntConstant(m, "XA_SUBSCRIPT_Y", XA_SUBSCRIPT_Y);
    PyModule_AddIntConstant(m, "XA_UNDERLINE_POSITION", XA_UNDERLINE_POSITION);
    PyModule_AddIntConstant(m, "XA_UNDERLINE_THICKNESS", XA_UNDERLINE_THICKNESS);
    PyModule_AddIntConstant(m, "XA_STRIKEOUT_ASCENT", XA_STRIKEOUT_ASCENT);
    PyModule_AddIntConstant(m, "XA_STRIKEOUT_DESCENT", XA_STRIKEOUT_DESCENT);
    PyModule_AddIntConstant(m, "XA_ITALIC_ANGLE", XA_ITALIC_ANGLE);
    PyModule_AddIntConstant(m, "XA_X_HEIGHT", XA_X_HEIGHT);
    PyModule_AddIntConstant(m, "XA_QUAD_WIDTH", XA_QUAD_WIDTH);
    PyModule_AddIntConstant(m, "XA_WEIGHT", XA_WEIGHT);
    PyModule_AddIntConstant(m, "XA_POINT_SIZE", XA_POINT_SIZE);
    PyModule_AddIntConstant(m, "XA_RESOLUTION", XA_RESOLUTION);
    PyModule_AddIntConstant(m, "XA_COPYRIGHT", XA_COPYRIGHT);
    PyModule_AddIntConstant(m, "XA_NOTICE", XA_NOTICE);
    PyModule_AddIntConstant(m, "XA_FONT_NAME", XA_FONT_NAME);
    PyModule_AddIntConstant(m, "XA_FAMILY_NAME", XA_FAMILY_NAME);
    PyModule_AddIntConstant(m, "XA_FULL_NAME", XA_FULL_NAME);
    PyModule_AddIntConstant(m, "XA_CAP_HEIGHT", XA_CAP_HEIGHT);
    PyModule_AddIntConstant(m, "XA_WM_CLASS", XA_WM_CLASS);
    PyModule_AddIntConstant(m, "XA_WM_TRANSIENT_FOR", XA_WM_TRANSIENT_FOR);
    PyModule_AddIntConstant(m, "XA_LAST_PREDEFINED", XA_LAST_PREDEFINED);

    return 0;
}
