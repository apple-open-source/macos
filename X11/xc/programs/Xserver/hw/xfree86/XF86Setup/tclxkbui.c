/* $XConsortium: tclxkbui.c /main/2 1996/10/19 19:06:46 kaleb $ */





/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tclxkbui.c,v 3.2 1996/12/27 06:54:25 dawes Exp $ */
/* 
 * tkXkbUIWin.c --
 *
 *	This module implements "xkbview" widgets.
 *	A "xkbview" is a widget that uses the xkbui library to
 *	display a representation of the keyboard as described by
 *	the XKB geometry.
 *
 * Copyright 1996 Joseph Moss
 *
 * The file is derived from the sample widget code tkSquare.c, which is
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <Xos.h>
#include <Xfuncs.h>
#include <tk.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBgeom.h>
#include <X11/extensions/XKBui.h>

#define DEF_XKBUI_HEIGHT "100"
#define DEF_XKBUI_WIDTH	 "300"

extern char *	TkInitXkbUIWin _ANSI_ARGS_((Tcl_Interp *interp,
			Tk_Window tkwin, int toplevel, int argc,
			char *argv[]));

extern XkbDescPtr	GetXkbDescPtr _ANSI_ARGS_((Tcl_Interp *interp,
				char *handle));

/*
 * A data structure of the following type is kept for each xkbview
 * widget managed by this file:
 */

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the xkbview.  NULL
				 * means window has been deleted but
				 * widget record hasn't been cleaned up yet. */
    Display *display;		/* X's token for the window's display. */
    Tcl_Interp *interp;		/* Interpreter associated with widget. */
    Tcl_Command widgetCmd;	/* Token for xkbview's widget command. */
    int width, height;		/* Window height & width */

    /*
     * Information used when displaying widget:
     */

    int borderWidth;		/* Width of 3-D border around whole widget. */
    Tk_3DBorder bgBorder;	/* Used for drawing background. */
    int relief;			/* Indicates whether window as a whole is
				 * raised, sunken, or flat. */
    GC gc;			/* Graphics context for copying from
				 * off-screen pixmap onto screen. */
    int doubleBuffer;		/* Non-zero means double-buffer redisplay
				 * with pixmap;  zero means draw straight
				 * onto the display. */
    int updatePending;		/* Non-zero means a call to XkbUIWinDisplay
				 * has already been scheduled. */
    char *xkbHandle;		/* Handle of XkbDescRec */
    XkbUI_ViewPtr view;		/* XkbUI view structure */

} XkbUIWin;

/*
 * Information used for argv parsing.
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	"#cdb79e", Tk_Offset(XkbUIWin, bgBorder), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	"white", Tk_Offset(XkbUIWin, bgBorder), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	"2", Tk_Offset(XkbUIWin, borderWidth), 0},
    {TK_CONFIG_INT, "-dbl", "doubleBuffer", "DoubleBuffer",
	"1", Tk_Offset(XkbUIWin, doubleBuffer), 0},
    {TK_CONFIG_PIXELS, "-height", "height", "Height",
	DEF_XKBUI_HEIGHT, Tk_Offset(XkbUIWin, height), 0},
    {TK_CONFIG_STRING, "-keyboard", "keyboard", (char *) NULL,
	"xkb1", Tk_Offset(XkbUIWin, xkbHandle), 0},
    {TK_CONFIG_SYNONYM, "-kbd", "keyboard", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	"raised", Tk_Offset(XkbUIWin, relief), 0},
    {TK_CONFIG_PIXELS, "-width", "width", "Width",
	DEF_XKBUI_WIDTH, Tk_Offset(XkbUIWin, width), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		XkbUIWinCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static int		XkbUIWinConfigure _ANSI_ARGS_((Tcl_Interp *interp,
			    XkbUIWin *xkbui, int argc, char **argv,
			    int flags));
static void		XkbUIWinDestroy _ANSI_ARGS_((ClientData clientData));
static void		XkbUIWinDisplay _ANSI_ARGS_((ClientData clientData));
static void		XkbUIWinEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static int		XkbUIWinWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *, int argc, char **argv));

/*
 *--------------------------------------------------------------
 *
 * XkbUIWinCmd --
 *
 *	This procedure is invoked to process the "xkbview" Tcl
 *	command.  It creates a new "xkbview" widget.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A new widget is created and configured.
 *
 *--------------------------------------------------------------
 */

int
XkbUIWinCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window main = (Tk_Window) clientData;
    XkbUIWin *xkbuiPtr;
    Tk_Window tkwin;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    tkwin = Tk_CreateWindowFromPath(interp, main, argv[1], (char *) NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    Tk_SetClass(tkwin, "Xkbui");

    /*
     * Allocate and initialize the widget record.
     */

    xkbuiPtr = (XkbUIWin *) ckalloc(sizeof(XkbUIWin));
    xkbuiPtr->tkwin = tkwin;
    xkbuiPtr->display = Tk_Display(tkwin);
    xkbuiPtr->interp = interp;
    xkbuiPtr->widgetCmd = Tcl_CreateCommand(interp,
	    Tk_PathName(xkbuiPtr->tkwin), XkbUIWinWidgetCmd,
	    (ClientData) xkbuiPtr, XkbUIWinCmdDeletedProc);
    xkbuiPtr->height = -1;
    xkbuiPtr->width = -1;
    xkbuiPtr->borderWidth = 0;
    xkbuiPtr->bgBorder = NULL;
    xkbuiPtr->relief = TK_RELIEF_FLAT;
    xkbuiPtr->gc = None;
    xkbuiPtr->doubleBuffer = 1;
    xkbuiPtr->updatePending = 0;
    xkbuiPtr->xkbHandle = NULL;
    xkbuiPtr->view = NULL;

    Tk_CreateEventHandler(xkbuiPtr->tkwin, ExposureMask|StructureNotifyMask,
	    XkbUIWinEventProc, (ClientData) xkbuiPtr);
    if (XkbUIWinConfigure(interp, xkbuiPtr, argc-2, argv+2, 0) != TCL_OK) {
	Tk_DestroyWindow(xkbuiPtr->tkwin);
	return TCL_ERROR;
    }

    interp->result = Tk_PathName(xkbuiPtr->tkwin);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * XkbUIWinWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
XkbUIWinWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Information about xkbview widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    XkbUIWin *xkbuiPtr = (XkbUIWin *) clientData;
    int result = TCL_OK;
    size_t length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tk_Preserve((ClientData) xkbuiPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Tk_ConfigureValue(interp, xkbuiPtr->tkwin, configSpecs,
		(char *) xkbuiPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, xkbuiPtr->tkwin, configSpecs,
		    (char *) xkbuiPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, xkbuiPtr->tkwin, configSpecs,
		    (char *) xkbuiPtr, argv[2], 0);
	} else {
	    result = XkbUIWinConfigure(interp, xkbuiPtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'r') && (strncmp(argv[1], "refresh", length) == 0)
	    && (length >= 2)) {
	/* Nothing - The call to Tk_DoWhenIdle below takes care of this */
	;
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\":  must be cget, configure, or refresh",
		(char *) NULL);
	goto error;
    }
    if (!xkbuiPtr->updatePending) {
	Tk_DoWhenIdle(XkbUIWinDisplay, (ClientData) xkbuiPtr);
	xkbuiPtr->updatePending = 1;
    }
    Tk_Release((ClientData) xkbuiPtr);
    return result;

    error:
    Tk_Release((ClientData) xkbuiPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * XkbUIWinConfigure --
 *
 *	This procedure is called to process an argv/argc list in
 *	conjunction with the Tk option database to configure (or
 *	reconfigure) a xkbview widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for xkbuiPtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
XkbUIWinConfigure(interp, xkbuiPtr, argc, argv, flags)
    Tcl_Interp *interp;			/* Used for error reporting. */
    XkbUIWin *xkbuiPtr;			/* Information about widget. */
    int argc;				/* Number of valid entries in argv. */
    char **argv;			/* Arguments. */
    int flags;				/* Flags to pass to
					 * Tk_ConfigureWidget. */
{
    if (Tk_ConfigureWidget(interp, xkbuiPtr->tkwin, configSpecs,
	    argc, argv, (char *) xkbuiPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Set the background for the window and create a graphics context
     * for use during redisplay.
     */

    Tk_SetWindowBackground(xkbuiPtr->tkwin,
	    Tk_3DBorderColor(xkbuiPtr->bgBorder)->pixel);
    if ((xkbuiPtr->gc == None) && (xkbuiPtr->doubleBuffer)) {
	XGCValues gcValues;
	gcValues.function = GXcopy;
	gcValues.graphics_exposures = False;
	xkbuiPtr->gc = Tk_GetGC(xkbuiPtr->tkwin,
		GCFunction|GCGraphicsExposures, &gcValues);
    }

    /*
     * Register the desired geometry for the window.  Then arrange for
     * the window to be redisplayed.
     */

    Tk_GeometryRequest(xkbuiPtr->tkwin, 200, 150);
    Tk_SetInternalBorder(xkbuiPtr->tkwin, xkbuiPtr->borderWidth);
    if (!xkbuiPtr->updatePending) {
	Tk_DoWhenIdle(XkbUIWinDisplay, (ClientData) xkbuiPtr);
	xkbuiPtr->updatePending = 1;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * XkbUIWinEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on xkbviews.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
XkbUIWinEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    XkbUIWin *xkbuiPtr = (XkbUIWin *) clientData;

    if (eventPtr->type == Expose) {
	if (!xkbuiPtr->updatePending) {
	    Tk_DoWhenIdle(XkbUIWinDisplay, (ClientData) xkbuiPtr);
	    xkbuiPtr->updatePending = 1;
	}
    } else if (eventPtr->type == ConfigureNotify) {
	if (!xkbuiPtr->updatePending) {
	    Tk_DoWhenIdle(XkbUIWinDisplay, (ClientData) xkbuiPtr);
	    xkbuiPtr->updatePending = 1;
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (xkbuiPtr->tkwin != NULL) {
	    xkbuiPtr->tkwin = NULL;
	    Tcl_DeleteCommand(xkbuiPtr->interp,
		    Tcl_GetCommandName(xkbuiPtr->interp,
		    xkbuiPtr->widgetCmd));
	}
	if (xkbuiPtr->updatePending) {
	    Tk_CancelIdleCall(XkbUIWinDisplay, (ClientData) xkbuiPtr);
	}
	Tk_EventuallyFree((ClientData) xkbuiPtr,
			  (Tcl_FreeProc *)XkbUIWinDestroy);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XkbUIWinCmdDeletedProc --
 *
 *	This procedure is invoked when a widget command is deleted.  If
 *	the widget isn't already in the process of being destroyed,
 *	this command destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
XkbUIWinCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    XkbUIWin *xkbuiPtr = (XkbUIWin *) clientData;
    Tk_Window tkwin = xkbuiPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	xkbuiPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *--------------------------------------------------------------
 *
 * XkbUIWinDisplay --
 *
 *	This procedure redraws the contents of a xkbview window.
 *	It is invoked as a do-when-idle handler, so it only runs
 *	when there's nothing else for the application to do.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

static void
XkbUIWinDisplay(clientData)
    ClientData clientData;	/* Information about window. */
{
    XkbUIWin *xkbuiPtr = (XkbUIWin *) clientData;
    Tk_Window tkwin = xkbuiPtr->tkwin;
    Pixmap pm = None;
    Drawable d;
    XkbDescPtr xkb;
    XkbUI_ViewOptsRec opts;
    int width, height, bd;

    xkbuiPtr->updatePending = 0;
    if (!Tk_IsMapped(tkwin)) {
	return;
    }

    /*
     * Create a pixmap for double-buffering, if necessary.
     */

    if (xkbuiPtr->doubleBuffer) {
	pm = XCreatePixmap(Tk_Display(tkwin), Tk_WindowId(tkwin),
		(unsigned) Tk_Width(tkwin), (unsigned) Tk_Height(tkwin),
		(unsigned) DefaultDepthOfScreen(Tk_Screen(tkwin)));
	d = pm;
    } else {
	d = Tk_WindowId(tkwin);
    }

    /*
     * Redraw the widget's background and border.
     */

    Tk_Fill3DRectangle(tkwin, d, xkbuiPtr->bgBorder, 0, 0, Tk_Width(tkwin),
	    Tk_Height(tkwin), xkbuiPtr->borderWidth, xkbuiPtr->relief);

    /*
     * Display the keyboard.
     */

    bzero((char *)&opts, sizeof(opts));
    bd = 0;
    if (xkbuiPtr->relief != TK_RELIEF_FLAT)
	bd = xkbuiPtr->borderWidth;
    opts.present = XkbUI_SizeMask|XkbUI_ColormapMask
			|XkbUI_MarginMask|XkbUI_OffsetMask;
    opts.margin_width = opts.margin_height = 0;
    opts.viewport.x   = opts.viewport.y    = bd;
    width  = opts.viewport.width  = Tk_Width(tkwin) - 2*bd;
    height = opts.viewport.height = Tk_Height(tkwin) - 2*bd;
    opts.cmap = Tk_Colormap(tkwin);
    xkb = GetXkbDescPtr(xkbuiPtr->interp, xkbuiPtr->xkbHandle);
    if (xkb != NULL) {
	if (xkbuiPtr->view)
	    ckfree(xkbuiPtr->view);
	xkbuiPtr->view= XkbUI_Init(xkbuiPtr->display, d,
					width, height, xkb, &opts);
        if (xkbuiPtr->view)
	    XkbUI_DrawRegion(xkbuiPtr->view,NULL);
    }

    /*
     * If double-buffered, copy to the screen and release the pixmap.
     */

    if (xkbuiPtr->doubleBuffer) {
	XCopyArea(Tk_Display(tkwin), pm, Tk_WindowId(tkwin), xkbuiPtr->gc,
		0, 0, (unsigned) Tk_Width(tkwin), (unsigned) Tk_Height(tkwin),
		0, 0);
	XFreePixmap(Tk_Display(tkwin), pm);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XkbUIWinDestroy --
 *
 *	This procedure is invoked by Tk_EventuallyFree or Tk_Release
 *	to clean up the internal structure of a xkbview at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the xkbview is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
XkbUIWinDestroy(clientData)
    ClientData clientData;	/* Info about xkbview widget. */
{
    XkbUIWin *xkbuiPtr = (XkbUIWin *) clientData;

    Tk_FreeOptions(configSpecs, (char *) xkbuiPtr, xkbuiPtr->display, 0);
    if (xkbuiPtr->gc != None) {
	Tk_FreeGC(xkbuiPtr->display, xkbuiPtr->gc);
    }
    if (xkbuiPtr->view != NULL) {
	ckfree(xkbuiPtr->view);
    }
    ckfree((char *) xkbuiPtr);
}

