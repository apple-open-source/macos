/*
 * tkimg.c --
 *
 *  Generic interface to XML parsers.
 *
 * Copyright (c) 2002 Andreas Kupries <andreas_kupries@users.sourceforge.net>
 *
 * Zveno Pty Ltd makes this software and associated documentation
 * available free of charge for any purpose.  You may make copies
 * of the software but you must include all of this notice on any copy.
 *
 * Zveno Pty Ltd does not warrant that this software is error free
 * or fit for any purpose.  Zveno Pty Ltd disclaims any liability for
 * all claims, expenses, losses, damages and costs any user may incur
 * as a result of using, copying or modifying the software.
 *
 * $Id: tkimg.c 170 2008-11-14 13:31:59Z nijtmans $
 *
 */

#include "tk.h"
#include "tkimg.h"


/*
 * Declarations for externally visible functions.
 */

#undef TCL_STORAGE_CLASS
#ifdef BUILD_tkimg
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_TKIMG_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif

EXTERN int Tkimg_Init (Tcl_Interp *interp);
EXTERN int Tkimg_SafeInit (Tcl_Interp *interp);

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#ifdef ALLOW_B64
static int tob64(ClientData clientData, Tcl_Interp *interp,
	int argc, Tcl_Obj *const objv[]);
static int fromb64(ClientData clientData, Tcl_Interp *interp,
	int argc, Tcl_Obj *const objv[]);
#endif

/*
 *----------------------------------------------------------------------------
 *
 * Tkimg_Init --
 *
 *  Initialisation routine for loadable module
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter,
 *  loads xml package.
 *
 *----------------------------------------------------------------------------
 */

int
Tkimg_Init (interp)
Tcl_Interp *interp; /* Interpreter to initialise. */
{
	extern int TkimgInitUtilities (Tcl_Interp* interp);

	extern TkimgStubs tkimgStubs;

#ifdef USE_TCL_STUBS
	if (Tcl_InitStubs(interp, "8.3", 0) == NULL) {
		return TCL_ERROR;
	}
#endif
#ifdef USE_TK_STUBS
	if (Tk_InitStubs(interp, "8.3", 0) == NULL) {
		return TCL_ERROR;
	}
#endif
	if (!TkimgInitUtilities(interp)) {
		return TCL_ERROR;
	}
#ifdef ALLOW_B64 /* Undocumented feature */
	Tcl_CreateObjCommand(interp, "img_to_base64", tob64, (ClientData) NULL, NULL);
	Tcl_CreateObjCommand(interp, "img_from_base64", fromb64, (ClientData) NULL, NULL);
#endif

	if (Tcl_PkgProvideEx
		(interp, PACKAGE_TCLNAME, PACKAGE_VERSION,
			(ClientData) &tkimgStubs) != TCL_OK
	) {
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * Tkimg_SafeInit --
 *
 *  Initialisation routine for loadable module in a safe interpreter.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter,
 *  loads xml package.
 *
 *----------------------------------------------------------------------------
 */

int
Tkimg_SafeInit (interp)
Tcl_Interp *interp; /* Interpreter to initialise. */
{
	return Tkimg_Init(interp);
}

/*
 *-------------------------------------------------------------------------
 * tob64 --
 *  This function converts the contents of a file into a base-64
 *  encoded string.
 *
 * Results:
 *  none
 *
 * Side effects:
 *  none
 *
 *-------------------------------------------------------------------------
 */

#ifdef ALLOW_B64
int tob64(clientData, interp, argc, objv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
Tcl_Obj *const objv[];
{
	Tcl_DString dstring;
	tkimg_MFile handle;
	Tcl_Channel chan;
	char buffer[1024];
	int len;

	if (argc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "filename");
		return TCL_ERROR;
	}

	chan = tkimg_OpenFileChannel(interp, Tcl_GetStringFromObj(objv[1], &len), 0);
	if (!chan) {
		return TCL_ERROR;
	}

	Tcl_DStringInit(&dstring);
	tkimg_WriteInit(&dstring, &handle);

	while ((len = Tcl_Read(chan, buffer, 1024)) == 1024) {
		tkimg_Write(&handle, buffer, 1024);
	}
	if (len > 0) {
		tkimg_Write(&handle, buffer, len);
	}
	if ((Tcl_Close(interp, chan) == TCL_ERROR) || (len < 0)) {
		Tcl_DStringFree(&dstring);
		Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[1], &len),
			": ", Tcl_PosixError(interp), (char *)NULL);
		return TCL_ERROR;
	}
	tkimg_Putc(IMG_DONE, &handle);

	Tcl_DStringResult(interp, &dstring);
	return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 * fromb64 --
 *  This function converts a base-64 encoded string into binary form,
 *  which is written to a file.
 *
 * Results:
 *  none
 *
 * Side effects:
 *  none
 *
 *-------------------------------------------------------------------------
 */

int fromb64(clientData, interp, argc, objv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
Tcl_Obj *const objv[];
{
	tkimg_MFile handle;
	Tcl_Channel chan;
	char buffer[1024];
	int len;

	if (argc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "filename data");
		return TCL_ERROR;
	}

	chan = tkimg_OpenFileChannel(interp, Tcl_GetStringFromObj(objv[1], &len), 0644);
	if (!chan) {
		return TCL_ERROR;
	}

	handle.data = Tcl_GetStringFromObj(objv[2], &handle.length);
	handle.state = 0;

	while ((len = tkimg_Read(&handle, buffer, 1024)) == 1024) {
		if (Tcl_Write(chan, buffer, 1024) != 1024) {
			goto writeerror;
		}
	}
	if (len > 0) {
		if (Tcl_Write(chan, buffer, len) != len) {
			goto writeerror;
		}
	}
	if (Tcl_Close(interp, chan) == TCL_ERROR) {
		return TCL_ERROR;
	}
	return TCL_OK;

writeerror:
	Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[1], &len), ": ",
		Tcl_PosixError(interp), (char *)NULL);
	return TCL_ERROR;
}
#endif
