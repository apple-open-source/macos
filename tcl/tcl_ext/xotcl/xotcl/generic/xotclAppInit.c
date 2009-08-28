/* 
 * tclAppInit.c --
 *
 *	Provides a default version of the main program and Tcl_AppInit
 *	procedure for Tcl applications (without Tk).
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "tcl-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclAppInit.c 1.20 97/03/24 14:29:43
 */

/* include tclInt.h for access to namespace API */
#include "tclInt.h"

#include "xotcl.h"

#if defined(VISUAL_CC)
#  include <windows.h>
#  include <locale.h>
#endif
#include <stdio.h>

#if TCL_MAJOR_VERSION < 7
  #error Tcl distribution TOO OLD
#endif

/*
 * The following variable is a special hack that is needed in order for
 * Sun shared libraries to be used for Tcl.
 */

#ifdef NEED_MATHERR
extern int matherr();
int *tclDummyMathPtr = (int *) matherr;
#endif

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	This is the main program for the application.
 *
 * Results:
 *	None: Tcl_Main never returns here, so this procedure never
 *	returns either.
 *
 * Side effects:
 *	Whatever the application does.
 *
 *----------------------------------------------------------------------
 */

#if TCL_MAJOR_VERSION == 7 && TCL_MINOR_VERSION < 4

extern int main();
int *tclDummyMainPtr = (int *) main;

#else

int
main(argc, argv)
    int argc;			/* Number of command-line arguments. */
    char **argv;		/* Values of command-line arguments. */
{
#if defined(VISUAL_CC)
    setlocale(LC_ALL, "C");
#endif
    Tcl_Main(argc, argv, Tcl_AppInit);
    return 0;			/* Needed only to prevent compiler warning. */
}

#endif


/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit(interp)
    Tcl_Interp *interp;		/* Interpreter for application. */
{
    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /*
     * Call the init procedures for included packages.  Each call should
     * look like this:
     *
     * if (Mod_Init(interp) == TCL_ERROR) {
     *     return TCL_ERROR;
     * }
     *
     * where "Mod" is the name of the module.

    if (Xotcl_Init(interp) == TCL_ERROR) {
        return TCL_ERROR;
    }

     Tcl_StaticPackage(interp, "XOTcl", Xotcl_Init, 0);
    */

    if (Tcl_PkgRequire(interp, "XOTcl", XOTCLVERSION, 1) == NULL) {
      return TCL_ERROR;
    }

    /*
     *  This is xotclsh, so import all xotcl commands by
     *  default into the global namespace.  
     */
  
    if (Tcl_Import(interp, Tcl_GetGlobalNamespace(interp),
            "::xotcl::*", /* allowOverwrite */ 1) != TCL_OK) {
        return TCL_ERROR;
    }
    /*
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */

    /*
     * Specify a user-specific startup file to invoke if the application
     * is run interactively.  Typically the startup file is "~/.apprc"
     * where "app" is the name of the application.  If this line is deleted
     * then no user-specific startup file will be run under any conditions.
     */

    Tcl_SetVar(interp, "tcl_rcFileName", "~/.tclshrc", TCL_GLOBAL_ONLY);
    return TCL_OK;
}
