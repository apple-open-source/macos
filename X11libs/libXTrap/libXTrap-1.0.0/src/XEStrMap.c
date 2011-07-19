/* $XFree86: xc/lib/XTrap/XEStrMap.c,v 1.1 2001/11/02 23:29:27 dawes Exp $ */
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991, 1992, 1994 by Digital Equipment Corp., 
Maynard, MA

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*****************************************************************************/
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>

static char *eventName[] = {
    "", "",
    "KeyPress",         "KeyRelease",       "ButtonPress",
    "ButtonRelease",    "MotionNotify",     "EnterNotify",
    "LeaveNotify",      "FocusIn",          "FocusOut",
    "KeymapNotify",     "Expose",           "GraphicsExpose",
    "NoExpose",         "VisibilityNotify", "CreateNotify",
    "DestroyNotify",    "UnmapNotify",      "MapNotify",
    "MapRequest",       "ReparentNotify",   "ConfigureNotify",
    "ConfigureRequest", "GravityNotify",    "ResizeRequest",
    "CirculateNotify",  "CirculateRequest", "PropertyNotify",
    "SelectionClear",   "SelectionRequest", "SelectionNotify",
    "ColormapNotify",   "ClientMessage",    "MappingNotify",
    "LASTEvent",
};

static char *requestName[] = {
    "Invalid",
    "X_CreateWindow",           "X_ChangeWindowAttributes",
    "X_GetWindowAttributes",    "X_DestroyWindow",
    "X_DestroySubwindows",      "X_ChangeSaveSet",
    "X_ReparentWindow",         "X_MapWindow",
    "X_MapSubwindows",          "X_UnmapWindow",
    "X_UnmapSubwindows",        "X_ConfigureWindow",
    "X_CirculateWindow",        "X_GetGeometry",
    "X_QueryTree",              "X_InternAtom",
    "X_GetAtomName",            "X_ChangeProperty",
    "X_DeleteProperty",         "X_GetProperty",
    "X_ListProperties",         "X_SetSelectionOwner",
    "X_GetSelectionOwner",      "X_ConvertSelection",
    "X_SendEvent",              "X_GrabPointer",
    "X_UngrabPointer",          "X_GrabButton",
    "X_UngrabButton",           "X_ChangeActivePointerGrab",
    "X_GrabKeyboard",           "X_UngrabKeyboard",
    "X_GrabKey",                "X_UngrabKey",
    "X_AllowEvents",            "X_GrabServer",
    "X_UngrabServer",           "X_QueryPointer",
    "X_GetMotionEvents",        "X_TranslateCoords",
    "X_WarpPointer",            "X_SetInputFocus",
    "X_GetInputFocus",          "X_QueryKeymap",
    "X_OpenFont",               "X_CloseFont",
    "X_QueryFont",              "X_QueryTextExtents",
    "X_ListFonts",              "X_ListFontsWithInfo",
    "X_SetFontPath",            "X_GetFontPath",
    "X_CreatePixmap",           "X_FreePixmap",
    "X_CreateGC",               "X_ChangeGC",
    "X_CopyGC",                 "X_SetDashes",
    "X_SetClipRectangles",      "X_FreeGC",
    "X_ClearArea",              "X_CopyArea",
    "X_CopyPlane",              "X_PolyPoint",
    "X_PolyLine",               "X_PolySegment",
    "X_PolyRectangle",          "X_PolyArc", 
    "X_FillPoly",               "X_PolyFillRectangle",
    "X_PolyFillArc",            "X_PutImage",
    "X_GetImage",               "X_PolyText8",
    "X_PolyText16",             "X_ImageText8",
    "X_ImageText16",            "X_CreateColormap",
    "X_FreeColormap",           "X_CopyColormapAndFree",
    "X_InstallColormap",        "X_UninstallColormap",
    "X_ListInstalledColormaps", "X_AllocColor",
    "X_AllocNamedColor",        "X_AllocColorCells",
    "X_AllocColorPlanes",       "X_FreeColors",
    "X_StoreColors",            "X_StoreNamedColor",
    "X_QueryColors",            "X_LookupColor",
    "X_CreateCursor",           "X_CreateGlyphCursor",
    "X_FreeCursor",             "X_RecolorCursorSize",
    "X_QueryBestSize",          "X_QueryExtension",
    "X_ListExtensions",         "X_ChangeKeyboardMapping",
    "X_GetKeyboardMapping",     "X_ChangeKeyboardControl",
    "X_GetKeyboardControl",     "X_Bell",
    "X_ChangePointerControl",   "X_GetPointerControl",
    "X_SetScreenSaver",         "X_GetScreenSaver",
    "X_ChangeHosts",            "X_ListHosts",
    "X_SetAccessControl",       "X_SetCloseDownMode",
    "X_KillClient",             "X_RotateProperties",
    "X_ForceScreenSaver",       "X_SetPointerMapping",
    "X_GetPointerMapping",      "X_SetModifierMapping",
    "X_GetModifierMapping",     "Invalid", 
    "Invalid",                  "Invalid", 
    "Invalid",                  "Invalid",
    "Invalid",                  "Invalid",
    "X_NoOperation",
};

typedef struct
{
    char *extName;
    int  extEvent;
}  _extensionData;
_extensionData *extensionData;
int numExtension = -1;

static struct _pf_tbl {CARD32 id; char *str;} pf_tbl[] =
{
    {PF_Apollo,         "Apollo"},
    {PF_ATT,            "ATT"},
    {PF_Cray1,          "Cray1"},
    {PF_Cray2,          "Cray2"},
    {PF_DECUltrix,      "DECUltrix"},
    {PF_DECVMS,         "DECVMS"},
    {PF_DECELN,         "DECELN"},
    {PF_DECOSF1,        "DECOSF1"},
    {PF_DECVT1000,      "DECVT1000"},
    {PF_DECXTerm,       "DECXTerm"},
    {PF_HP9000s800,     "HP9000s800"},
    {PF_HP9000s300,     "HP9000s300"},
    {PF_IBMAT,          "IBMAT"},
    {PF_IBMRT,          "IBMRT"},
    {PF_IBMPS2,         "IBMPS2"},
    {PF_IBMRS,          "IBMRS"},
    {PF_MacII,          "MacII"},
    {PF_Pegasus,        "Pegasus"},
    {PF_SGI,            "SGI"},
    {PF_Sony,           "Sony"},
    {PF_Sun3,           "Sun3"},
    {PF_Sun386i,        "Sun386i"},
    {PF_SunSparc,       "SunSparc"},
    {PF_Other,          "Other"}        /* always the last one! */
};

static char unknown[] = "unknown";

#define ASize(array) (sizeof(array)/sizeof((array)[0]))

static INT16 _StringToID(register char *match, register char **strings,
    INT16 nstrings)
{
    register INT16 id = nstrings;

    if (match && *match)
    { while ((--id >= 0L) && (strcmp(match,strings[id]) != 0L)); }
    else
    { id = -1L; }

    return(id);
}
static void loadExtStrings(XETC *tc)
{
    char **extensionName=XListExtensions(tc->dpy,&numExtension);
    if (numExtension)
    {
        int i;
        extensionData = (_extensionData *)XtCalloc(numExtension,
                            sizeof(_extensionData));
        for (i = 0; i < numExtension; i++)
        {   /* Arrange extensions in opcode order */
            int opcode,event,error;
            if (XQueryExtension(tc->dpy,extensionName[i],&opcode,&event,
                &error))
            {
                extensionData[opcode-128].extName = extensionName[i];
                extensionData[opcode-128].extEvent = event;
            }
            else
            {   /* This extension didn't load!  Error! */
                extensionData[opcode-128].extName = "Invalid_Extension";
            }
        }
        XFreeExtensionList(extensionName);
    }
}

INT16 XEEventStringToID(register char *string)
{
    return(_StringToID(string,eventName,ASize(eventName)));
}

INT16 XERequestStringToID(register char *string)
{
    return(_StringToID(string,requestName,ASize(requestName)));
}

CARD32 XEPlatformStringToID(register char *string)
{
    struct _pf_tbl *ptr = &(pf_tbl[0]);
    while(ptr->id != PF_Other)
    {
        if (!strncmp(ptr->str, string, strlen(ptr->str)))
        {
            return(ptr->id);
        }
        ptr++;
    }
    return((!strncmp(ptr->str,string,strlen(ptr->str))) ? ptr->id : -1L);
}

char *XEEventIDToString(register CARD8 id, XETC *tc)
{
    int i;
    if (id < ASize(eventName))
        return(eventName[id]);
    /* either erroneous or an extension event */
    if (numExtension < 0)
    {   /* 
         * This is unfortunate, but necessary.  The client
         * program has requested the string identifier for
         * an extension request/event.  Since there's no Xlib
         * equivalent for this, we have to query *all* the
         * extensions looking for a match.  Chances are
         * if a client wants one, it'll want them all,
         * so just go through and initialize the extension
         * list once.
         */
        loadExtStrings(tc);
    }
    /* Find id within extensionData */
    for (i=0; i<numExtension; i++)
    {
        if (extensionData[i].extEvent == id)
            return(extensionData[i].extName);
    }
    return(unknown);
}

char *XERequestIDToExtString(register CARD8 id, XETC *tc)
{
    int extid;

    extid = id - ASize(requestName);

    if (numExtension < 0)
    {   /* 
         * This is unfortunate, but necessary.  The client
         * program has requested the string identifier for
         * an extension request/event.  Since there's no Xlib
         * equivalent for this, we have to query *all* the
         * extensions looking for a match.  Chances are
         * if a client wants one, it'll want them all,
         * so just go through and initialize the extension
         * list once.
         */
        loadExtStrings(tc);
    }
    return((extid >=0 && extid < numExtension) ? 
        extensionData[extid].extName : unknown);
}


char *XERequestIDToString(register CARD8 id, XETC *tc)
{
    return((id < ASize(requestName)) ? requestName[id] :
        XERequestIDToExtString(id,tc));
}

char *XEPlatformIDToString(register CARD32 id)
{
    struct _pf_tbl *ptr = &(pf_tbl[0]);
    while((ptr->id != PF_Other) || (id == ptr->id))
    {
        if (id == ptr->id)
        {
            return(ptr->str);
        }
        ptr++;
    }
    return(unknown);
}
