/* $XConsortium: tclother.c /main/3 1996/10/28 04:46:43 kaleb $ */





/* $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tclother.c,v 3.10 2001/07/25 15:05:05 dawes Exp $ */
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

  This file contains routines to add a few misc commands to Tcl

 */

#include <stdlib.h>
#include <stdio.h>
#include <X11/Xos.h>
#include <X11/Xproto.h>
#include <X11/Xfuncs.h>
#include <tcl.h>
#include <tk.h>
#include <sys/types.h>
#include <sys/stat.h>

static int	TCL_XF86GetUID(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86ServerRunning(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86ProcessRunning(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86HasSymlinks(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86Link(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86UnLink(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86Umask(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86MkDir(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86RmDir(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

static int	TCL_XF86Sleep(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);

#if TCL_MAJOR_VERSION == 7 && TCL_MINOR_VERSION == 4
static int	TCL_XF86Clock(
#if NeedNestedProtoTypes
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    char *argv[]
#endif
);
#endif

/*
   Adds all the new commands to the Tcl interpreter
*/

int
XF86Other_Init(interp)
    Tcl_Interp	*interp;
{
	Tcl_CreateCommand(interp, "getuid",
		TCL_XF86GetUID, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "server_running",
		TCL_XF86ServerRunning, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "process_running",
		TCL_XF86ProcessRunning, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "has_symlinks",
		TCL_XF86HasSymlinks, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "link",
		TCL_XF86Link, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "unlink",
		TCL_XF86UnLink, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "umask",
		TCL_XF86Umask, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "mkdir",
		TCL_XF86MkDir, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "rmdir",
		TCL_XF86RmDir, (ClientData) NULL,
		(void (*)()) NULL);

	Tcl_CreateCommand(interp, "sleep",
		TCL_XF86Sleep, (ClientData) NULL,
		(void (*)()) NULL);

#if TCL_MAJOR_VERSION == 7 && TCL_MINOR_VERSION == 4
	Tcl_CreateCommand(interp, "clock",
		TCL_XF86Clock, (ClientData) NULL,
		(void (*)()) NULL);
#endif

	return TCL_OK;
}

/*
  Returns the users numeric id
*/

int
TCL_XF86GetUID(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	if (argc != 1) {
		Tcl_SetResult(interp, "Usage: getuid", TCL_STATIC);
		return TCL_ERROR;
	}

	/* This is short, so we can write directly into the
	   pre-allocated buffer */
	sprintf(interp->result, "%d", (int) getuid());
	return TCL_OK;
}

/*
  Trivial error handler used to ignore failed attempts to connect
  to the server
*/

static int ignoreErrors(disp, error)
    Display	*disp;
    XErrorEvent	*error;
{
	return 0;
}

/*
  Attempt to connect to an Xserver.
  Also used to close the connection opened in a previous call.
*/

int
TCL_XF86ServerRunning(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	static Tcl_HashTable	connectTable;
	static Bool		initted = False;
	Tcl_HashEntry		*entry;
	int			new, (*old)();
	Display			*display;

	if (!initted) {
		initted = True;
		Tcl_InitHashTable(&connectTable, TCL_STRING_KEYS);
	}
	if (argc < 2 || argc > 3
			|| (argc == 3 && strcmp(argv[1],"-close")) ) {
		Tcl_SetResult(interp,
			"Usage: server_running [-close] <display>",
			TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 3) {
		entry = Tcl_FindHashEntry(&connectTable, argv[2]);
		if (entry == NULL) {
			Tcl_SetResult(interp, "No connection to display",
				TCL_STATIC);
			return TCL_ERROR;
		}
		display = (Display *) Tcl_GetHashValue(entry);
		XCloseDisplay(display);
		Tcl_DeleteHashEntry(entry);
	} else {
		entry = Tcl_FindHashEntry(&connectTable, argv[1]);
		if (entry != NULL) {
			Tcl_SetResult(interp,
				"Connection to display already open",
				TCL_STATIC);
			return TCL_ERROR;
		}
		old = XSetErrorHandler(ignoreErrors);
		display = XOpenDisplay(argv[1]);
		if (display == (Display *) NULL) {
			Tcl_SetResult(interp, "0", TCL_STATIC);
		} else {
			entry = Tcl_CreateHashEntry(&connectTable, argv[1], &new);
			Tcl_SetHashValue(entry, display);
			Tcl_SetResult(interp, "1", TCL_STATIC);
			XSync(display, False);
		}
		(void) XSetErrorHandler(old);
	}
	return TCL_OK;
}

/*
  Check if the specified process is running
*/

int
TCL_XF86ProcessRunning(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: process_running <pid>",
				TCL_STATIC);
		return TCL_ERROR;
	}

	if (kill(atoi(argv[1]), 0) == 0) {
		Tcl_SetResult(interp, "1", TCL_STATIC);
	} else {
		Tcl_SetResult(interp, "0", TCL_STATIC);
	}
	return TCL_OK;
}

/*
  Return 1 if the system supports symbolic links, zero otherwise
*/

int
TCL_XF86HasSymlinks(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	if (argc != 1) {
		Tcl_SetResult(interp, "Usage: has_symlinks", TCL_STATIC);
		return TCL_ERROR;
	}

#ifdef S_IFLNK
	Tcl_SetResult(interp, "1", TCL_STATIC);
#else
	Tcl_SetResult(interp, "0", TCL_STATIC);
#endif
	return TCL_OK;
}

/*
  Make a link from one file to another (use symlinks, if available)
*/

int
TCL_XF86Link(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	if (argc != 3) {
		Tcl_SetResult(interp, "Usage: link <oldfilename> <newfilename>", TCL_STATIC);
		return TCL_ERROR;
	}

#ifdef S_IFLNK
	if (symlink(argv[1], argv[2]) == -1)
#else
	if (link(argv[1], argv[2]) == -1)
#endif
		Tcl_SetResult(interp, "0", TCL_STATIC);
	else
		Tcl_SetResult(interp, "1", TCL_STATIC);
	return TCL_OK;
}

/*
  Delete the specified file
*/

int
TCL_XF86UnLink(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: unlink <filename>", TCL_STATIC);
		return TCL_ERROR;
	}

	if (unlink(argv[1]) == -1)
		Tcl_SetResult(interp, "0", TCL_STATIC);
	else
		Tcl_SetResult(interp, "1", TCL_STATIC);
	return TCL_OK;
}

/*
  Set the umask
*/

int
TCL_XF86Umask(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	int mode;

	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: umask <value>", TCL_STATIC);
		return TCL_ERROR;
	}

	if (Tcl_GetInt(interp, argv[1], &mode) != TCL_OK)
		return TCL_ERROR;
	if (umask((mode_t) mode) == (mode_t)-1)
		Tcl_SetResult(interp, "0", TCL_STATIC);
	else
		Tcl_SetResult(interp, "1", TCL_STATIC);
	return TCL_OK;
}

/*
  Create the named directory
*/

int
TCL_XF86MkDir(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	int mode = 0777;

	if (argc < 2 || argc > 3) {
		Tcl_SetResult(interp, "Usage: mkdir <dirname> [<mode>]", TCL_STATIC);
		return TCL_ERROR;
	}

	if (argc == 3) {
		if (Tcl_GetInt(interp, argv[2], &mode) != TCL_OK)
			return TCL_ERROR;
	}

	if (mkdir(argv[1], mode) == -1)
		Tcl_SetResult(interp, "0", TCL_STATIC);
	else
		Tcl_SetResult(interp, "1", TCL_STATIC);
	return TCL_OK;
}

/*
  Remove the specified directory
*/

int
TCL_XF86RmDir(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: rmdir <dirname>", TCL_STATIC);
		return TCL_ERROR;
	}

	if (rmdir(argv[1]) == -1)
		Tcl_SetResult(interp, "0", TCL_STATIC);
	else
		Tcl_SetResult(interp, "1", TCL_STATIC);
	return TCL_OK;
}

/*
  Pause for the specified number of seconds
*/

int
TCL_XF86Sleep(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	if (argc != 2) {
		Tcl_SetResult(interp, "Usage: sleep <seconds>", TCL_STATIC);
		return TCL_ERROR;
	}

	sleep(atoi(argv[1]));
	return TCL_OK;
}

#if TCL_MAJOR_VERSION == 7 && TCL_MINOR_VERSION == 4
/*
  Emulate a subset of the Tcl 7.5 clock command
*/

#include <time.h>

int
TCL_XF86Clock(clientData, interp, argc, argv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		argc;
    char	*argv[];
{
	if (argc < 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " option ?arg ...?\"", (char *) NULL);
		return TCL_ERROR;
	}

	if (!strcmp(argv[1], "clicks")) {
		if (argc != 2) {
			Tcl_AppendResult(interp, "wrong # arguments: must be \"",
				argv[0], " clicks\"", (char *) NULL);
			return TCL_ERROR;
		}
#ifndef AMOEBA
		{
		    struct timeval  tp;

		    X_GETTIMEOFDAY(&tp);
		    sprintf(interp->result, "%lu",
			    tp.tv_sec*1000000 + tp.tv_usec);
		}
#else
		sprintf(interp->result, "%lu", sys_milli());
#endif
		return TCL_OK;
	} else if (!strcmp(argv[1], "seconds")) {
		if (argc != 2) {
			Tcl_AppendResult(interp, "wrong # arguments: must be \"",
				argv[0], " seconds\"", (char *) NULL);
			return TCL_ERROR;
		}
		sprintf(interp->result, "%ld", (long) time(0));
	} else {
		Tcl_AppendResult(interp, "unknown option \"", argv[1],
			"\": must be clicks, format, scan, or seconds",
			(char *) NULL);
		return TCL_ERROR;
	}
}
#endif

