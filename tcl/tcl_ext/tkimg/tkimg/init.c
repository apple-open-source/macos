/*
 * init.c --
 *
 *  Generic photo image type initialization, Tcl/Tk package
 *
 * Copyright (c) 2002 Andreas Kupries <andreas_kupries@users.sourceforge.net>
 *
 * $Id: init.c 170 2008-11-14 13:31:59Z nijtmans $
 *
 */

#include "tkimg.h"

#ifndef MORE_INITIALIZATION
#define MORE_INITIALIZATION /* Nothing */
#endif

#undef TCL_STORAGE_CLASS
#ifdef BUILD_%PACKAGE%
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_%PACKAGE_UP%_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif

/*
 * Functions exported for package management.
 */


EXTERN int @CPACKAGE@_Init(Tcl_Interp *interp);
EXTERN int @CPACKAGE@_SafeInit(Tcl_Interp *interp);

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

/*
 * Declarations of internal functions.
 */

static int ChnMatch(Tcl_Interp *interp, Tcl_Channel chan,
	const char *fileName, Tcl_Obj *format, int *widthPtr,
	int *heightPtr);

static int ObjMatch(Tcl_Interp *interp, Tcl_Obj *dataObj,
	Tcl_Obj *format, int *widthPtr, int *heightPtr);

static int ChnRead(Tcl_Interp *interp, Tcl_Channel chan,
	const char *fileName, Tcl_Obj *format, Tk_PhotoHandle imageHandle,
	int destX, int destY, int width, int height, int srcX, int srcY);

static int ObjRead(Tcl_Interp *interp, Tcl_Obj *dataObj,
	Tcl_Obj *format, Tk_PhotoHandle imageHandle,
	int destX, int destY, int width, int height, int srcX, int srcY);

static int ChnWrite(Tcl_Interp *interp, const char *filename,
	Tcl_Obj *format, Tk_PhotoImageBlock *blockPtr);

static int StringWrite(Tcl_Interp *interp,
	Tcl_DString *data, Tcl_Obj *format,
	Tk_PhotoImageBlock *blockPtr);

static Tk_PhotoImageFormat format = {
	(char *) "%PHIMGTYPE%", /* name */
	(Tk_ImageFileMatchProc *) ChnMatch, /* fileMatchProc */
	(Tk_ImageStringMatchProc *) ObjMatch, /* stringMatchProc */
	(Tk_ImageFileReadProc *) ChnRead, /* fileReadProc */
	(Tk_ImageStringReadProc *) ObjRead, /* stringReadProc */
	(Tk_ImageFileWriteProc *) ChnWrite, /* fileWriteProc */
	(Tk_ImageStringWriteProc *) StringWrite /* stringWriteProc */
};

#ifdef SECOND_FORMAT
/*
 * Declare procedures of the second format as needed. The macro we
 * check for allow us to share code between first and second
 * format. Current user of this feature: The PS/PDF combo handler
 */

#ifndef SECOND_CHNMATCH
#define SECOND_CHNMATCH ChnMatchBeta
static int ChnMatchBeta(Tcl_Interp *interp, Tcl_Channel chan,
	const char *fileName, Tcl_Obj *format, int *widthPtr,
	int *heightPtr);
#endif
#ifndef SECOND_OBJMATCH
#define SECOND_OBJMATCH ObjMatchBeta
static int ObjMatchBeta(Tcl_Interp *interp, Tcl_Obj *dataObj,
	Tcl_Obj *format, int *widthPtr, int *heightPtr);
#endif
#ifndef SECOND_CHNREAD
#define SECOND_CHNREAD ChnReadBeta
static int ChnReadBeta(Tcl_Interp *interp, Tcl_Channel chan,
	const char *fileName, Tcl_Obj *format, Tk_PhotoHandle imageHandle,
	int destX, int destY, int width, int height, int srcX, int srcY);
#endif
#ifndef SECOND_OBJREAD
#define SECOND_OBJREAD ChnObjReadBeta
static int ObjReadBeta(Tcl_Interp *interp, Tcl_Obj *dataObj,
	Tcl_Obj *format, Tk_PhotoHandle imageHandle,
	int destX, int destY, int width, int height, int srcX, int srcY);
#endif
#ifndef SECOND_CHNWRITE
#define SECOND_CHNWRITE ChnWriteBeta
static int ChnWriteBeta(Tcl_Interp *interp, const char *filename,
	Tcl_Obj *format, Tk_PhotoImageBlock *blockPtr);
#endif
#ifndef SECOND_STRWRITE
#define SECOND_STRWRITE StringWriteBeta
static int StringWriteBeta(Tcl_Interp *interp,
	Tcl_DString *data, Tcl_Obj *format,
	Tk_PhotoImageBlock *blockPtr);
#endif

static Tk_PhotoImageFormat format_beta = {
	(char *) "%PHIMGTYPE_BETA%", /* name */
	(Tk_ImageFileMatchProc *) SECOND_CHNMATCH, /* fileMatchProc */
	(Tk_ImageStringMatchProc *) SECOND_OBJMATCH, /* stringMatchProc */
	(Tk_ImageFileReadProc *) SECOND_CHNREAD, /* fileReadProc */
	(Tk_ImageStringReadProc *) SECOND_OBJREAD, /* stringReadProc */
	(Tk_ImageFileWriteProc *) SECOND_CHNWRITE, /* fileWriteProc */
	(Tk_ImageStringWriteProc *) SECOND_STRWRITE /* stringWriteProc */
};

#endif /* SECOND_FORMAT */


/*
 *----------------------------------------------------------------------------
 *
 * @CPACKAGE@_Init --
 *
 *  Initialisation routine for loadable module
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter, loads package.
 *
 *----------------------------------------------------------------------------
 */

int
@CPACKAGE@_Init(
	Tcl_Interp *interp /* Interpreter to initialise. */
) {
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
#ifdef USE_TKIMG_STUBS
	if (Tkimg_InitStubs(interp, TKIMG_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}
#endif

	MORE_INITIALIZATION;

	/*
	 * Register the new photo image type.
	 */

	Tk_CreatePhotoImageFormat (&format);
#ifdef SECOND_FORMAT
	Tk_CreatePhotoImageFormat (&format_beta);
#endif /* SECOND_FORMAT */

	/*
	 * At last provide the package ...
	 */

	if (Tcl_PkgProvide(interp, PACKAGE_TCLNAME, PACKAGE_VERSION) != TCL_OK) {
		return TCL_ERROR;
	}
	return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * @CPACKAGE@_SafeInit --
 *
 *  Initialisation routine for loadable module in a safe interpreter.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter, loads package.
 *
 *----------------------------------------------------------------------------
 */

int
@CPACKAGE@_SafeInit(
	Tcl_Interp *interp /* Interpreter to initialise. */
) {
	return @CPACKAGE@_Init (interp);
}

