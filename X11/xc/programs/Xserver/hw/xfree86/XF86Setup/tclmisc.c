/* $Xconsortium: $ */





/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tclmisc.c,v 3.14 2001/07/25 15:05:05 dawes Exp $ */
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

  This file contains Tcl bindings to the XFree86-Misc extension

 */

#include <X11/Intrinsic.h>
#include <X11/Xmd.h>
#include <X11/extensions/xf86misc.h>
#include <X11/Xos.h>
#include <tcl.h>
#include <tk.h>
#include <ctype.h>
#include "tclmisc.h"

static int (*savErrorFunc)();
static int errorOccurred;
static char errMsgBuf[512];

int XF86Misc_Init(Tcl_Interp *interp);

static int miscError(Display *dis, XErrorEvent *err)
{
	XGetErrorText(dis, err->error_code, errMsgBuf, 512);
	errorOccurred = TRUE;
	return 0;
}

/*
   Adds all the xf86misc specific commands to the Tcl interpreter
*/

int
XF86Misc_Init(interp)
Tcl_Interp *interp;
{
	Tcl_CreateCommand(interp, "xf86misc_getversion",
		TCL_XF86MiscQueryVersion, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xf86misc_getbasevals",
		TCL_XF86MiscQueryExtension, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xf86misc_getsaver",
		TCL_XF86MiscGetSaver, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xf86misc_setsaver",
		TCL_XF86MiscSetSaver, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xf86misc_getkeyboard",
		TCL_XF86MiscGetKbdSettings, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xf86misc_setkeyboard",
		TCL_XF86MiscSetKbdSettings, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xf86misc_getmouse",
		TCL_XF86MiscGetMouseSettings, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "xf86misc_setmouse",
		TCL_XF86MiscSetMouseSettings, (ClientData) NULL,
		(void (*)()) NULL);

	return TCL_OK;
}

/* Note, the characters '_', ' ', and '\t' are ignored in the comparison */
int
StrCaseCmp(s1, s2)
char *s1, *s2;
{
	char c1, c2;

        if (*s1 == 0)
                if (*s2 == 0)
                        return(0);
                else
                        return(1);
	while (*s1 == '_' || *s1 == ' ' || *s1 == '\t')
		s1++;
	while (*s2 == '_' || *s2 == ' ' || *s2 == '\t')
		s2++;
        c1 = (isupper(*s1) ? tolower(*s1) : *s1);
        c2 = (isupper(*s2) ? tolower(*s2) : *s2);
        while (c1 == c2)
        {
                if (c1 == '\0')
                        return(0);
                s1++; s2++;
		while (*s1 == '_' || *s1 == ' ' || *s1 == '\t')
			s1++;
		while (*s2 == '_' || *s2 == ' ' || *s2 == '\t')
			s2++;
                c1 = (isupper(*s1) ? tolower(*s1) : *s1);
                c2 = (isupper(*s2) ? tolower(*s2) : *s2);
        }
        return(c1 - c2);
}

/*
   Implements the xf86misc_getversion command which
   returns (in interp->result) the version of the
   XFree86-MiscExtension that is built into the X server
   The version is returned simple floating point number (e.g. 0.4)
*/

int
TCL_XF86MiscQueryVersion(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	int MajorVersion, MinorVersion;
	Tk_Window tkwin;
	char tmpbuf[16];

        if (argc != 1) {
                Tcl_SetResult(interp, "Usage: xf86misc_getversion", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	if (!XF86MiscQueryVersion(Tk_Display(tkwin), &MajorVersion, &MinorVersion))
	{
		Tcl_AppendResult(interp,
			"Could not query XF86Misc extension version",
			(char *) NULL);
		return TCL_ERROR;
	} else {
		sprintf(tmpbuf, "%d.%d", MajorVersion, MinorVersion);
		Tcl_AppendResult(interp, tmpbuf, (char *) NULL);
		return TCL_OK;
	}
}


/*
   Implements the xf86misc_getbasevals command which
   returns (in interp->result) a list containing two elements.
   The first element is the EventBase and the second is the ErrorBase
*/

int
TCL_XF86MiscQueryExtension(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window tkwin;
	int eventBase, errorBase;

        if (argc != 1) {
                Tcl_SetResult(interp, "Usage: xf86misc_getbasevals", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	if (!XF86MiscQueryExtension(Tk_Display(tkwin),
					&eventBase, &errorBase)) {
		Tcl_AppendResult(interp,
			"Unable to query XF86Misc extension information",
			(char *) NULL);
		return TCL_ERROR;
	}

	sprintf(interp->result, "%d %d", eventBase, errorBase);
	return TCL_OK;
}

/*
   Implements the xf86misc_getsaver command which
   returns (in interp->result) a list containing the
   powersaver timeouts
*/

int
TCL_XF86MiscGetSaver(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	Tk_Window tkwin;

        if (argc != 1) {
                Tcl_SetResult(interp, "Usage: xf86misc_getsaver", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
#if 0
	if (!XF86MiscGetSaver(Tk_Display(tkwin), Tk_ScreenNumber(tkwin),
			&suspendtime, &offtime)) {
		Tcl_AppendResult(interp,
			"Unable to get screen saver timeouts",
			(char *) NULL);
		return TCL_ERROR;
	} else {
		sprintf(interp->result, "%d %d", suspendtime, offtime);
		return TCL_OK;
	}
#else
	return TCL_OK;
#endif
}

/*
   Implements the xf86misc_setsaver command which
   sets the powersaver timeouts
*/

int
TCL_XF86MiscSetSaver(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	int suspendtime, offtime;
	Tk_Window tkwin;

        if (argc != 3) {
                Tcl_SetResult(interp,
			"Usage: xf86misc_setsaver <suspendtime> <offtime>",
			TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	suspendtime = atoi(argv[1]);
	offtime = atoi(argv[2]);

#if 0
	XSync(Tk_Display(tkwin), False);
	savErrorFunc = XSetErrorHandler(miscError);
	errorOccurred = 0;
	XF86MiscSetSaver(Tk_Display(tkwin), Tk_ScreenNumber(tkwin),
			suspendtime, offtime);
	XSync(Tk_Display(tkwin), False);
	XSetErrorHandler(savErrorFunc);
	if (errorOccurred) {
		Tcl_AppendResult(interp,
			"Unable to set screen saver timeouts: ",
			errMsgBuf, (char *) NULL);
		return TCL_ERROR;
	}
#endif
	return TCL_OK;
}

static char *kbdtable[] = { "None", "84Key", "101Key", "Other", "Xqueue" };
/*
   Implements the xf86misc_getkeyboard command which
   returns (in interp->result) a list containing the
   keyboard settings
*/

int
TCL_XF86MiscGetKbdSettings(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	XF86MiscKbdSettings kbdinfo;
	Tk_Window tkwin;

        if (argc != 1) {
                Tcl_SetResult(interp, "Usage: xf86misc_getkeyboard", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	if (!XF86MiscGetKbdSettings(Tk_Display(tkwin), &kbdinfo)) {
		Tcl_AppendResult(interp,
			"Unable to get keyboard settings",
			(char *) NULL);
		return TCL_ERROR;
	} else {
		sprintf(interp->result, "%s %d %d %s",
			kbdtable[kbdinfo.type], kbdinfo.delay, kbdinfo.rate,
			kbdinfo.servnumlock? "on": "off");
		return TCL_OK;
	}
}

/*
   Implements the xf86misc_setkeyboard command which
   sets the keyboard settings
*/

int
TCL_XF86MiscSetKbdSettings(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	XF86MiscKbdSettings kbdinfo;
	Tk_Window tkwin;
	int i;
	char *usage = "Usage: xf86misc_setkeyboard 84Key|101Key|Other|Xqueue"
			    " <delay> <rate> on|off";

        if (argc != 5) {
                Tcl_SetResult(interp, usage, TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	kbdinfo.type = -1;
	for (i = 1; i < sizeof(kbdtable)/sizeof(char *); i++) {
		if (!StrCaseCmp(kbdtable[i], argv[1])) {
			kbdinfo.type = i;
		}
	}
	if (kbdinfo.type == -1) {
                Tcl_AppendResult(interp, "Invalid keyboard type\n",
				 usage, (char *) NULL);
                return TCL_ERROR;
        }
	kbdinfo.delay = atoi(argv[2]);
	kbdinfo.rate = atoi(argv[3]);
	if (!StrCaseCmp(argv[4], "on"))
		kbdinfo.servnumlock = 1;
	else if (!StrCaseCmp(argv[4], "off"))
		kbdinfo.servnumlock = 0;
	else {
                Tcl_AppendResult(interp, "Option must be either on or off\n",
				usage, (char *) NULL);
                return TCL_ERROR;
	}
	XSync(Tk_Display(tkwin), False);
	savErrorFunc = XSetErrorHandler(miscError);
	errorOccurred = 0;
	XF86MiscSetKbdSettings(Tk_Display(tkwin), &kbdinfo);
	XSync(Tk_Display(tkwin), False);
	XSetErrorHandler(savErrorFunc);
	if (errorOccurred) {
		Tcl_AppendResult(interp,
			"Unable to set keyboard settings: ",
			errMsgBuf, (char *) NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
}

static char *msetable[] = { "None", "Microsoft", "MouseSystems", "MMSeries",
			    "Logitech", "BusMouse", "Mouseman", "PS/2",
			    "MMHitTab", "GlidePoint", "IntelliMouse",
			    "ThinkingMouse", "IMPS/2", "ThinkingMousePS/2",
			    "MouseManPlusPS/2", "GlidePointPS/2", 
			    "NetMousePS/2", "NetScrollPS/2", "SysMouse",
			    "Auto", "Xqueue", "OSMouse" };
#define MSETABLESIZE	(sizeof(msetable)/sizeof(char *))

/*
   Implements the xf86misc_getmouse command which
   returns (in interp->result) a list containing the
   mouse settings
*/

int
TCL_XF86MiscGetMouseSettings(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	XF86MiscMouseSettings mseinfo;
	Tk_Window tkwin;
	char tmpbuf[200];

        if (argc != 1) {
                Tcl_SetResult(interp, "Usage: xf86misc_getmouse", TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	if (!XF86MiscGetMouseSettings(Tk_Display(tkwin), &mseinfo)) {
		Tcl_AppendResult(interp,
			"Unable to get mouse settings",
			(char *) NULL);
		return TCL_ERROR;
	} else {
		char *name;
		if (mseinfo.type == MTYPE_XQUEUE)
			name = "Xqueue";
		else if (mseinfo.type == MTYPE_OSMOUSE)
			name = "OSMouse";
		else if (mseinfo.type < 0 || (mseinfo.type >= MSETABLESIZE))
			name = "Unknown";
		else
			name = msetable[mseinfo.type+1];
		sprintf(tmpbuf, "%s %s %d %d %d %d %s %d %s",
			mseinfo.device==NULL? "{}": mseinfo.device,
			name,
			mseinfo.baudrate, mseinfo.samplerate,
			mseinfo.resolution, mseinfo.buttons,
			mseinfo.emulate3buttons? "on": "off",
			mseinfo.emulate3timeout,
			mseinfo.chordmiddle? "on": "off");
		if (mseinfo.flags & MF_CLEAR_DTR)
			strcat(tmpbuf, " ClearDTR");
		if (mseinfo.flags & MF_CLEAR_RTS)
			strcat(tmpbuf, " ClearRTS");
		Tcl_SetResult(interp, tmpbuf, TCL_VOLATILE);
		if (mseinfo.device) {
		    XtFree(mseinfo.device);
		}
		return TCL_OK;
	}
}

/*
   Implements the xf86misc_setmouse command which
   sets the mouse settings
*/

static char *setmouseusage =
		"Usage: xf86misc_setmouse <device> <mousetype>"
		" <baudrate> <samplerate> <resolution> <buttons>"
		" on|off <timeout> on|off [ClearDTR] [ClearRTS] [ReOpen]";

int
TCL_XF86MiscSetMouseSettings(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	XF86MiscMouseSettings mseinfo;
	int i;
	Tk_Window tkwin;

        if (argc < 9 || argc > 12) {
                Tcl_SetResult(interp, setmouseusage, TCL_STATIC);
                return TCL_ERROR;
        }

	if ((tkwin = Tk_MainWindow(interp)) == (Tk_Window) NULL)
		return TCL_ERROR;
	mseinfo.device = argv[1];
	mseinfo.type = -1;
	for (i = 1; i < sizeof(msetable)/sizeof(char *); i++) {
		if (!StrCaseCmp(msetable[i], argv[2])) {
			mseinfo.type = i - 1;
		}
	}
	if (!StrCaseCmp("Xqueue", argv[2]))
		mseinfo.type = MTYPE_XQUEUE;
	else if (!StrCaseCmp("OSMouse", argv[2]))
		mseinfo.type = MTYPE_OSMOUSE;
	if (mseinfo.type == -1) {
                Tcl_AppendResult(interp, "Invalid mouse type\n",
				 setmouseusage, (char *) NULL);
                return TCL_ERROR;
        }
	mseinfo.baudrate = atoi(argv[3]);
	mseinfo.samplerate = atoi(argv[4]);
	mseinfo.resolution = atoi(argv[5]);
	mseinfo.buttons = atoi(argv[6]);
	if (!StrCaseCmp(argv[7], "on"))
		mseinfo.emulate3buttons = 1;
	else if (!StrCaseCmp(argv[7], "off"))
		mseinfo.emulate3buttons = 0;
	else {
                Tcl_AppendResult(interp, "Option must be either on or off\n",
				setmouseusage, (char *) NULL);
                return TCL_ERROR;
	}
	mseinfo.emulate3timeout = atoi(argv[8]);
	if (!StrCaseCmp(argv[9], "on"))
		mseinfo.chordmiddle = 1;
	else if (!StrCaseCmp(argv[9], "off"))
		mseinfo.chordmiddle = 0;
	else {
                Tcl_AppendResult(interp, "Option must be either on or off\n",
				setmouseusage, (char *) NULL);
                return TCL_ERROR;
	}
	mseinfo.flags = 0;
	for (i = 10; i < argc; i++) {
		if (!StrCaseCmp(argv[i], "cleardtr"))
			mseinfo.flags |= MF_CLEAR_DTR;
		else if (!StrCaseCmp(argv[i], "clearrts"))
			mseinfo.flags |= MF_CLEAR_RTS;
		else if (!StrCaseCmp(argv[i], "reopen"))
			mseinfo.flags |= MF_REOPEN;
		else {
			Tcl_AppendResult(interp,
					"Flag must be one of ClearDTR,"
					    "ClearRTS, or ReOpen\n",
					setmouseusage, (char *) NULL);
			return TCL_ERROR;
		}
	}
	XSync(Tk_Display(tkwin), False);
	savErrorFunc = XSetErrorHandler(miscError);
	errorOccurred = 0;
	XF86MiscSetMouseSettings(Tk_Display(tkwin), &mseinfo);
	XSync(Tk_Display(tkwin), False);
	XSetErrorHandler(savErrorFunc);
	if (errorOccurred) {
		Tcl_AppendResult(interp,
			"Unable to set mouse settings: ",
			errMsgBuf, (char *) NULL);
		return TCL_ERROR;
	}
	return TCL_OK;
}

