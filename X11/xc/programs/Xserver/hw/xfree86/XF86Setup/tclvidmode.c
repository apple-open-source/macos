/* $XConsortium: tclvidmode.c /main/2 1996/10/19 19:06:29 kaleb $ */





/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tclvidmode.c,v 3.9 1997/07/10 08:17:20 hohndel Exp $ */
/*
 * Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Joseph Moss not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Joseph Moss makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * JOSEPH MOSS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL JOSEPH MOSS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


/*

  This file contains Tcl bindings to the XFree86-VidModeExtension

 */

#define NOT_YET_IMPLEMENTED 0

#include <X11/Intrinsic.h>
#include <X11/Xmd.h>
#include <X11/extensions/xf86vmode.h>
#include <tcl.h>
#include <tk.h>
#include "tclvidmode.h"

/* Mode flags -- ignore flags not in V_FLAG_MASK */
#define V_FLAG_MASK	0x1FF;
#define V_PHSYNC	0x001 
#define V_NHSYNC	0x002
#define V_PVSYNC	0x004
#define V_NVSYNC	0x008
#define V_INTERLACE	0x010 
#define V_DBLSCAN	0x020
#define V_CSYNC		0x040
#define V_PCSYNC	0x080
#define V_NCSYNC	0x100
#define V_HSKEW		0x200

static int (*savErrorFunc)();
static int errorOccurred;
static char errMsgBuf[512];

/*
  Simple error handler
*/
static int vidError(dis, err)
Display *dis;
XErrorEvent *err;
{
	XGetErrorText(dis, err->error_code, errMsgBuf, 512);
	errorOccurred = TRUE;
	return 0;
}

static int modeline2list(interp, mode_line)
    Tcl_Interp	*interp;
    XF86VidModeModeInfo	*mode_line;
{
	sprintf(interp->result, "%6.2f %d %d %d %d %d %d %d %d",
	    mode_line->dotclock/1000.0,
	    mode_line->hdisplay, mode_line->hsyncstart,
	    mode_line->hsyncend, mode_line->htotal,
	    mode_line->vdisplay, mode_line->vsyncstart,
	    mode_line->vsyncend, mode_line->vtotal);
#define chkflag(flg,string)	if (mode_line->flags & flg) \
			    Tcl_AppendResult(interp, string, (char *) NULL)
	chkflag(V_PHSYNC," +hsync");
	chkflag(V_NHSYNC," -hsync");
	chkflag(V_PVSYNC," +vsync");
	chkflag(V_NVSYNC," -vsync");
	chkflag(V_INTERLACE," interlace");
	chkflag(V_CSYNC," composite");
	chkflag(V_PCSYNC," +csync");
	chkflag(V_NCSYNC," -csync");
	chkflag(V_DBLSCAN," doublescan");
	if (mode_line->flags & V_HSKEW) {
	    char tmpbuf[16];
	    sprintf(tmpbuf, "%d", mode_line->hskew);
	    Tcl_AppendResult(interp, " doublescan ", tmpbuf, (char *) NULL);
	}
#undef chkflag
	return TCL_OK;
}

#define TclOkay(expr)	if ((expr) != TCL_OK) return TCL_ERROR

static int list2modeline(interp, buf, mode_line)
    Tcl_Interp	*interp;
    char	*buf;
    XF86VidModeModeInfo	*mode_line;
{
	char	**av;
	int	ac, i, tmpint;
	double	tmpdbl;

	TclOkay(Tcl_SplitList(interp, buf, &ac, &av));
	if (ac < 9) return TCL_ERROR;

	mode_line->hskew = 0;

	TclOkay(Tcl_GetDouble(interp, av[0], &tmpdbl));
	mode_line->dotclock = (int) (tmpdbl * 1000.0);

	TclOkay(Tcl_GetInt(interp, av[1], &tmpint));
	mode_line->hdisplay   = (unsigned short) tmpint;
	TclOkay(Tcl_GetInt(interp, av[2], &tmpint));
	mode_line->hsyncstart = (unsigned short) tmpint;
	TclOkay(Tcl_GetInt(interp, av[3], &tmpint));
	mode_line->hsyncend   = (unsigned short) tmpint;
	TclOkay(Tcl_GetInt(interp, av[4], &tmpint));
	mode_line->htotal     = (unsigned short) tmpint;
	TclOkay(Tcl_GetInt(interp, av[5], &tmpint));
	mode_line->vdisplay   = (unsigned short) tmpint;
	TclOkay(Tcl_GetInt(interp, av[6], &tmpint));
	mode_line->vsyncstart = (unsigned short) tmpint;
	TclOkay(Tcl_GetInt(interp, av[7], &tmpint));
	mode_line->vsyncend   = (unsigned short) tmpint;
	TclOkay(Tcl_GetInt(interp, av[8], &tmpint));
	mode_line->vtotal     = (unsigned short) tmpint;

	mode_line->flags = 0;
	for (i = 9; i < ac; i++) {
	    if (!strcmp(av[i], "+hsync"))          mode_line->flags |= V_PHSYNC;
	    else if (!strcmp(av[i], "-hsync"))     mode_line->flags |= V_NHSYNC;
	    else if (!strcmp(av[i], "+vsync"))     mode_line->flags |= V_PVSYNC;
	    else if (!strcmp(av[i], "-vsync"))     mode_line->flags |= V_NVSYNC;
	    else if (!strcmp(av[i], "interlace"))  mode_line->flags |= V_INTERLACE;
	    else if (!strcmp(av[i], "composite"))  mode_line->flags |= V_CSYNC;
	    else if (!strcmp(av[i], "+csync"))     mode_line->flags |= V_PCSYNC;
	    else if (!strcmp(av[i], "-csync"))     mode_line->flags |= V_NCSYNC;
	    else if (!strcmp(av[i], "doublescan")) mode_line->flags |= V_DBLSCAN;
	    else if (!strcmp(av[i], "hskew") && i < ac-1) {
	    	mode_line->flags |= V_HSKEW;
		TclOkay(Tcl_GetInt(interp, av[++i], &tmpint));
		mode_line->hskew = (unsigned short) tmpint;
	    } else {
	    	Tcl_AppendResult(interp, "Invalid mode flag: ", av[i], (char *)0);
		return TCL_ERROR;
	    }
	}
	mode_line->privsize = 0;
	mode_line->private  = NULL;
	return TCL_OK;
}

/*
   Adds all the vidmode specific commands to the Tcl interpreter
*/

int
XF86vid_Init(interp)
    Tcl_Interp	*interp;
{
     Tcl_CreateCommand(interp, "xf86vid_getversion",
	     TCL_XF86VidModeQueryVersion, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_getbasevals",
	     TCL_XF86VidModeQueryExtension, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_addmodeline",
	     TCL_XF86VidModeAddModeLine, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_modifymodeline",
	     TCL_XF86VidModeModModeLine, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_checkmodeline",
	     TCL_XF86VidModeValidateModeLine, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_deletemodeline",
	     TCL_XF86VidModeDeleteModeLine, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_getmodeline",
	     TCL_XF86VidModeGetModeLine, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_getallmodelines",
	     TCL_XF86VidModeGetAllModeLines, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_lockmodeswitch",
	     TCL_XF86VidModeLockModeSwitch, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_switchmode",
	     TCL_XF86VidModeSwitchMode, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_switchtomode",
	     TCL_XF86VidModeSwitchToMode, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_getmonitor",
	     TCL_XF86VidModeGetMonitor, (ClientData) NULL,
	     (void (*)()) NULL);

     Tcl_CreateCommand(interp, "xf86vid_getclocks",
	     TCL_XF86VidModeGetDotClocks, (ClientData) NULL,
	     (void (*)()) NULL);

     return TCL_OK;
}

/*
   Implements the xf86vid_getversion command which
   returns (in interp->result) the version of the
   XFree86-VidModeExtension that is built into the X server
   The version is returned simple floating point number (e.g. 0.4)
*/

int
TCL_XF86VidModeQueryVersion(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	int MajorVersion, MinorVersion;
	Tk_Window tkwin;
	char tmpbuf[16];

        if (argc != 1) {
                Tcl_SetResult(interp, "Usage: xf86vid_getversion", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	if (!XF86VidModeQueryVersion(Tk_Display(tkwin), &MajorVersion, &MinorVersion))
	{
		Tcl_AppendResult(interp,
			"Could not query vidmode extension version",
			(char *) NULL);
		return TCL_ERROR;
	} else {
		sprintf(tmpbuf, "%d.%d", MajorVersion, MinorVersion);
		Tcl_AppendResult(interp, tmpbuf, (char *) NULL);
		return TCL_OK;
	}
}


/*
   Implements the xf86vid_getbasevals command which
   returns (in interp->result) a list containing two elements.
   The first element is the EventBase and the second is the ErrorBase
*/

int
TCL_XF86VidModeQueryExtension(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	int EventBase, ErrorBase;
	Tk_Window tkwin;
	char tmpbuf[16];

        if (argc != 1) {
                Tcl_SetResult(interp, "Usage: xf86vid_getbasevals", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	if (!XF86VidModeQueryExtension(Tk_Display(tkwin), &EventBase, &ErrorBase)) {
		Tcl_AppendResult(interp,
			"Unable to query video extension information",
			(char *) NULL);
		return TCL_ERROR;
	} else {
		sprintf(tmpbuf, "%d %d", EventBase, ErrorBase);
		Tcl_AppendResult(interp, tmpbuf, (char *) NULL);
		return TCL_OK;
	}
}


/*
   Implements the xf86vid_addmodeline command which
   adds a new mode to the list of video modes.
*/

int
TCL_XF86VidModeAddModeLine(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window tkwin;
	XF86VidModeModeInfo newmode, aftermode;

        if (argc < 2 || argc > 3) {
                Tcl_SetResult(interp,
		    "Usage: xf86vid_addmodeline <new_mode> [<after_mode>]",
			TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	TclOkay(list2modeline(interp, argv[1], &newmode));
	XSync(Tk_Display(tkwin), False);
	savErrorFunc = XSetErrorHandler(vidError);
	errorOccurred = 0;
	XF86VidModeAddModeLine(Tk_Display(tkwin),
				Tk_ScreenNumber(tkwin), &newmode,
				(argc==2)? NULL: &aftermode);
	XSync(Tk_Display(tkwin), False);
	XSetErrorHandler(savErrorFunc);
	if (errorOccurred) {
                Tcl_AppendResult(interp, "Unable to add modeline: ",
                        errMsgBuf, (char *) NULL);
                return TCL_ERROR;
	}

	return TCL_OK;
}

/*
   Implements the xf86vid_modifymodeline command which
   changes the current mode is to match the specified parameters.
*/

int
TCL_XF86VidModeModModeLine(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window tkwin;
	XF86VidModeModeInfo mode_info;
	XF86VidModeModeLine mode_line;

        if (argc != 2) {
                Tcl_SetResult(interp,
		    "Usage: xf86vid_modifymodeline <modeline>", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	TclOkay(list2modeline(interp, argv[1], &mode_info));
	mode_line.hdisplay =	mode_info.hdisplay;
	mode_line.hsyncstart =	mode_info.hsyncstart;
	mode_line.hsyncend =	mode_info.hsyncend;
	mode_line.htotal =	mode_info.htotal;
	mode_line.hskew =	mode_info.hskew;
	mode_line.vdisplay =	mode_info.vdisplay;
	mode_line.vsyncstart =	mode_info.vsyncstart;
	mode_line.vsyncend =	mode_info.vsyncend;
	mode_line.vtotal =	mode_info.vtotal;
	mode_line.flags =	mode_info.flags;
	mode_line.privsize =	mode_info.privsize;
	mode_line.private =	mode_info.private;
	XSync(Tk_Display(tkwin), False);
	savErrorFunc = XSetErrorHandler(vidError);
	errorOccurred = 0;
	XF86VidModeModModeLine(Tk_Display(tkwin),
		    Tk_ScreenNumber(tkwin), &mode_line);
	XSync(Tk_Display(tkwin), False);
	XSetErrorHandler(savErrorFunc);
	if (errorOccurred) {
                Tcl_AppendResult(interp, "Unable to modify modeline: ",
                        errMsgBuf, (char *) NULL);
                return TCL_ERROR;
	}

	return TCL_OK;
}

/*
   Implements the xf86vid_checkmodeline command which
   checks that the specified mode is usable with the
   video driver and monitor.
*/

int
TCL_XF86VidModeValidateModeLine(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window tkwin;
	XF86VidModeModeInfo mode_line;

        if (argc != 2) {
                Tcl_SetResult(interp,
		    "Usage: xf86vid_checkmodeline <modeline>", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	TclOkay(list2modeline(interp, argv[1], &mode_line));
	sprintf(interp->result, "%d",
		XF86VidModeValidateModeLine(Tk_Display(tkwin),
		    Tk_ScreenNumber(tkwin), &mode_line));
	return TCL_OK;
}

/*
   Implements the xf86vid_deletemodeline command which
   removes the specified mode from the list of valid modes
*/

int
TCL_XF86VidModeDeleteModeLine(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window tkwin;
	XF86VidModeModeInfo mode_line;

        if (argc != 2) {
                Tcl_SetResult(interp,
		    "Usage: xf86vid_deletemodeline <modeline>", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	TclOkay(list2modeline(interp, argv[1], &mode_line));

	XSync(Tk_Display(tkwin), False);
	savErrorFunc = XSetErrorHandler(vidError);
	errorOccurred = 0;
	XF86VidModeDeleteModeLine(Tk_Display(tkwin),
				Tk_ScreenNumber(tkwin), &mode_line);
	XSync(Tk_Display(tkwin), False);
	XSetErrorHandler(savErrorFunc);
	if (errorOccurred) {
                Tcl_AppendResult(interp, "Unable to delete modeline: ",
                        errMsgBuf, (char *) NULL);
                return TCL_ERROR;
	}

	return TCL_OK;
}

/*
   Implements the xf86vid_getmodeline command which
   returns (in interp->result) a list containing the
   various video mode parameters (including any flags)
   of the current mode
*/

int
TCL_XF86VidModeGetModeLine(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	int dot_clock;
	Tk_Window tkwin;
	XF86VidModeModeLine mode_line;
	XF86VidModeModeInfo mode_info;

        if (argc != 1) {
                Tcl_SetResult(interp, "Usage: xf86vid_getmodeline", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	if (!XF86VidModeGetModeLine(Tk_Display(tkwin),
			Tk_ScreenNumber(tkwin),
			&dot_clock, &mode_line)) {
		Tcl_AppendResult(interp,
			"Unable to get mode line information",
			(char *) NULL);
		return TCL_ERROR;
	}
	XtFree((char *) mode_line.private);
	mode_info.dotclock =   dot_clock;
	mode_info.hdisplay =   mode_line.hdisplay;
	mode_info.hsyncstart = mode_line.hsyncstart;
	mode_info.hsyncend =   mode_line.hsyncend;
	mode_info.htotal =     mode_line.htotal;
	mode_info.hskew =      mode_line.hskew;
	mode_info.vdisplay =   mode_line.vdisplay;
	mode_info.vsyncstart = mode_line.vsyncstart;
	mode_info.vsyncend =   mode_line.vsyncend;
	mode_info.vtotal =     mode_line.vtotal;
	mode_info.flags =      mode_line.flags;
	return modeline2list(interp, &mode_info);
}

/*
   Implements the xf86vid_getallmodelines command which
   returns (in interp->result) a list containing lists of the
   various video mode parameters (including any flags)
*/

int
TCL_XF86VidModeGetAllModeLines(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	int i, modecount, mode_flags;
	Tk_Window topwin, tkwin;
	XF86VidModeModeInfo **modelines;
	char tmpbuf[200], tmpbuf2[16];

        if (argc != 1 && !(argc==3 && !strcmp(argv[1],"-displayof"))) {
                Tcl_SetResult(interp,
			"Usage: xf86vid_getallmodelines [-displayof <window>]",
			TCL_STATIC);
                return TCL_ERROR;
        }

	if ((topwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	if (argc == 3) {
		tkwin = Tk_NameToWindow(interp, argv[2], topwin);
	} else
		tkwin = topwin;
	if (!XF86VidModeGetAllModeLines(Tk_Display(tkwin),
					Tk_ScreenNumber(tkwin),
					&modecount, &modelines)) {
		Tcl_AppendResult(interp,
			"Unable to get mode line information",
			(char *) NULL);
		return TCL_ERROR;
	} else {
	    for (i = 0; i < modecount; i++) {
		sprintf(tmpbuf, "%6.2f %d %d %d %d %d %d %d %d",
		    (float) modelines[i]->dotclock/1000.0,
		    modelines[i]->hdisplay, modelines[i]->hsyncstart,
		    modelines[i]->hsyncend, modelines[i]->htotal,
		    modelines[i]->vdisplay, modelines[i]->vsyncstart,
		    modelines[i]->vsyncend, modelines[i]->vtotal);
		mode_flags = modelines[i]->flags;
		if (mode_flags & V_PHSYNC)    strcat(tmpbuf, " +hsync");
		if (mode_flags & V_NHSYNC)    strcat(tmpbuf, " -hsync");
		if (mode_flags & V_PVSYNC)    strcat(tmpbuf, " +vsync");
		if (mode_flags & V_NVSYNC)    strcat(tmpbuf, " -vsync");
		if (mode_flags & V_INTERLACE) strcat(tmpbuf, " interlace");
		if (mode_flags & V_CSYNC)     strcat(tmpbuf, " composite");
		if (mode_flags & V_PCSYNC)    strcat(tmpbuf, " +csync");
		if (mode_flags & V_NCSYNC)    strcat(tmpbuf, " -csync");
		if (mode_flags & V_DBLSCAN)   strcat(tmpbuf, " doublescan");
		if (mode_flags & V_HSKEW) {
		    strcat(tmpbuf, " hskew ");
		    sprintf(tmpbuf2, "%d", modelines[i]->hskew);
		    strcat(tmpbuf, tmpbuf2);
		}
		Tcl_AppendElement(interp, tmpbuf);
	    }
	    XtFree((char *) modelines);
	    return TCL_OK;
	}
}

/*
   Returns the monitor's manufacturer and model names and its
   horiz and vert sync rates,
*/

int
TCL_XF86VidModeGetMonitor(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
#define MNHSync ((int) monitor.nhsync)
#define MNVSync ((int) monitor.nvsync)

	XF86VidModeMonitor monitor;
	Tk_Window tkwin;
	char *Hsyncbuf, *Vsyncbuf, *tmpptr, *av[5];
	int i;

        if (argc != 1) {
                Tcl_SetResult(interp, "Usage: xf86vid_getmonitor", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	if (!XF86VidModeGetMonitor(Tk_Display(tkwin),
			Tk_ScreenNumber(tkwin), &monitor))
	{
		Tcl_AppendResult(interp,
			"Could not get monitor information",
			(char *) NULL);
		return TCL_ERROR;
	} else {
		av[0] = monitor.vendor;
		av[1] = monitor.model;

		tmpptr = Hsyncbuf = XtMalloc(MNHSync*14);
		for (i = 0; i < MNHSync; i++) {
			sprintf(tmpptr, "%s%.5g-%.5g", (i? ",": ""),
				monitor.hsync[i].lo, monitor.hsync[i].hi);
			tmpptr += strlen(tmpptr);
		}
		av[2] = Hsyncbuf;

		tmpptr = Vsyncbuf = XtMalloc(MNVSync*14);
		for (i = 0; i < MNVSync; i++) {
			sprintf(tmpptr, "%s%.5g-%.5g", (i? ",": ""),
				monitor.vsync[i].lo, monitor.vsync[i].hi);
			tmpptr += strlen(tmpptr);
		}
		av[3] = Vsyncbuf;
		av[4] = NULL;

		Tcl_SetResult(interp, Tcl_Merge(4, av), TCL_DYNAMIC);
		XtFree(Hsyncbuf);
		XtFree(Vsyncbuf);
		XtFree(monitor.vendor);
		XtFree(monitor.model);
		XtFree((char *) monitor.hsync);
		XtFree((char *) monitor.vsync);
		return TCL_OK;
	}
#undef MNHSync
#undef MNVSync
}

/*
   Turn on/off video mode switching
*/

int
TCL_XF86VidModeLockModeSwitch(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	int lock;
	Tk_Window tkwin;
	static char usagemsg[] = "Usage: xf86vid_lockmodeswitch lock|unlock";

        if (argc != 2) {
                Tcl_SetResult(interp, usagemsg, TCL_STATIC);
                return TCL_ERROR;
        }

	if (!strcmp(argv[1], "lock")) {
		lock = TRUE;
	} else if (!strcmp(argv[1], "unlock")) {
		lock = FALSE;
	} else {
                Tcl_SetResult(interp, usagemsg, TCL_STATIC);
                return TCL_ERROR;
        }

        if ((tkwin = Tk_MainWindow(interp)) == NULL)
                return TCL_ERROR;
 
	XSync(Tk_Display(tkwin), False);
	savErrorFunc = XSetErrorHandler(vidError);
	errorOccurred = 0;
	XF86VidModeLockModeSwitch(Tk_Display(tkwin),
				Tk_ScreenNumber(tkwin), lock);
	XSync(Tk_Display(tkwin), False);
	XSetErrorHandler(savErrorFunc);
	if (errorOccurred) {
		Tcl_AppendResult(interp, "Unable to ",
			(lock? "":"un"), "lock mode switching: ",
			errMsgBuf, (char *) NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
}

/*
   Change to the previous/next video mode
*/

int
TCL_XF86VidModeSwitchMode(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
#define PREV -1
#define NEXT 1
	int direction;
	Tk_Window tkwin;
	static char usagemsg[] = "Usage: xf86vid_switchmode previous|next";

        if (argc != 2) {
                Tcl_SetResult(interp, usagemsg, TCL_STATIC);
                return TCL_ERROR;
        }

	if (!strncmp(argv[1], "prev", 4)) {
		direction = PREV;
	} else if (!strcmp(argv[1], "next")) {
		direction = NEXT;
	} else {
                Tcl_SetResult(interp, usagemsg, TCL_STATIC);
                return TCL_ERROR;
        }

        if ((tkwin = Tk_MainWindow(interp)) == NULL)
                return TCL_ERROR;
 
	XSync(Tk_Display(tkwin), False);
	savErrorFunc = XSetErrorHandler(vidError);
	errorOccurred = 0;
	XF86VidModeSwitchMode(Tk_Display(tkwin),
				Tk_ScreenNumber(tkwin), direction);
	XSync(Tk_Display(tkwin), False);
	XSetErrorHandler(savErrorFunc);
	if (errorOccurred) {
		Tcl_AppendResult(interp,
			"Unable to switch modes: ",
			errMsgBuf, (char *) NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
#undef PREV
#undef NEXT
}

/*
   Implements the xf86vid_switchtomode command which
   attempts to make the specified video mode, the current display mode
*/

int
TCL_XF86VidModeSwitchToMode(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window tkwin;
	XF86VidModeModeInfo mode_line;

        if (argc != 2) {
                Tcl_SetResult(interp,
		    "Usage: xf86vid_switchtomode <modeline>", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;
	TclOkay(list2modeline(interp, argv[1], &mode_line));

	sprintf(interp->result, "%d",
		XF86VidModeSwitchToMode(Tk_Display(tkwin),
		    Tk_ScreenNumber(tkwin), &mode_line));
	return TCL_OK;
}

/*
   Implements the xf86vid_getclocks command which
   returns a list of available dot clocks
*/

int
TCL_XF86VidModeGetDotClocks(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window tkwin;
	int flags, numclocks, maxclocks, *clocks, i;
	char tmpbuf[200];

        if (argc != 1) {
                Tcl_SetResult(interp,
		    "Usage: xf86vid_getclocks", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == NULL)
		return TCL_ERROR;

	if (!XF86VidModeGetDotClocks(Tk_Display(tkwin),
		    Tk_ScreenNumber(tkwin),
		    &flags, &numclocks, &maxclocks, &clocks))
	{
		Tcl_AppendResult(interp,
			"Unable to get dot clock information",
			(char *) NULL);
		return TCL_ERROR;
	} else {
		sprintf(tmpbuf, "%d",
		    maxclocks * ((flags&CLKFLAG_PROGRAMABLE)? -1: 1));
		Tcl_SetResult(interp, tmpbuf, TCL_VOLATILE);

		for (i = 0; i < numclocks; i++) {
		    sprintf(tmpbuf, "%d", clocks[i]);
		    Tcl_AppendElement(interp, tmpbuf);
		}
	}

	return TCL_OK;
}

