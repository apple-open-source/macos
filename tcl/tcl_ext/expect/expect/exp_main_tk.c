/* exp_main_tk.c - main for expectk

   This file consists of three pieces:
   1) AppInit for Expectk.  This has been suitably modified to invoke
      a modified version of Tk_Init.
   2) Tk_Init for Expectk.  What's wrong with the normal Tk_Init is that
      removes the -- in the cmd-line arg list, so Expect cannot know
      whether args are flags to Expectk or data for the script.  Sigh.
   3) Additions and supporting utilities to Tk's Argv parse table to
      support Expectk's flags.

   Author: Don Libes, NIST, 2/20/96

*/

/* Expectk's AppInit */

/* 
 * tkAppInit.c --
 *
 *	Provides a default version of the Tcl_AppInit procedure for
 *	use in wish and similar Tk-based applications.
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef lint
static char sccsid[] = "@(#) tkAppInit.c 1.19 95/12/23 17:09:24";
#endif /* not lint */

/* Don't use stubs since we are in the main application. */
#undef USE_TCL_STUBS

#include <ctype.h>

#include "tk.h"

#include "expect_tcl.h"
#include "tcldbg.h"

#if (TCL_MAJOR_VERSION < 8) || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION < 4))
/*
 * The following variable is a special hack that is needed in order for
 * Sun shared libraries to be used for Tcl.
 */

extern int matherr();
int *tclDummyMathPtr = (int *) matherr;
#endif

#ifdef TK_TEST
EXTERN int		Tktest_Init _ANSI_ARGS_((Tcl_Interp *interp));
#endif /* TK_TEST */

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	This is the main program for the application.
 *
 * Results:
 *	None: Tk_Main never returns here, so this procedure never
 *	returns either.
 *
 * Side effects:
 *	Whatever the application does.
 *
 *----------------------------------------------------------------------
 */

int
main(argc, argv)
    int argc;			/* Number of command-line arguments. */
    char **argv;		/* Values of command-line arguments. */
{
    Tk_Main(argc, argv, Tcl_AppInit);
    return 0;			/* Needed only to prevent compiler warning. */
}

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
 *	message in the interp's result if an error occurs.
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

    /* do Expect first so we can get access to Expect commands when */
    /* Tk_Init does the argument parsing of -c */
    if (Expect_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "Expect", Expect_Init, (Tcl_PackageInitProc *)NULL);

    if (Tk_Init2(interp) == TCL_ERROR) {	/* DEL */
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "Tk", Tk_Init, (Tcl_PackageInitProc *) NULL);

    /*
     * Call the init procedures for included packages.  Each call should
     * look like this:
     *
     * if (Mod_Init(interp) == TCL_ERROR) {
     *     return TCL_ERROR;
     * }
     *
     * where "Mod" is the name of the module.
     */

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

    Tcl_SetVar(interp, "tcl_rcFileName", "~/.wishrc", TCL_GLOBAL_ONLY);
    return TCL_OK;
}




/*
 * Count of number of main windows currently open in this process.
 */

static int numMainWindows;

/*
 * The variables and table below are used to parse arguments from
 * the "argv" variable in Tk_Init.
 */

static int synchronize;
static CONST char *name;
static char *display;
static char *geometry;
static char *colormap;
static char *visual;
static int rest = 0;

/* for Expect */
int my_rc = 1;
int sys_rc = 1;
static int optcmd_eval();
static int optcmd_diagToStderr();
#ifdef TCL_DEBUGGER
static int optcmd_debug();
#endif
int print_version = 0;

static Tk_ArgvInfo argTable[] = {
    {"-colormap", TK_ARGV_STRING, (char *) NULL, (char *) &colormap,
	"Colormap for main window"},
    {"-display", TK_ARGV_STRING, (char *) NULL, (char *) &display,
	"Display to use"},
    {"-geometry", TK_ARGV_STRING, (char *) NULL, (char *) &geometry,
	"Initial geometry for window"},
    {"-name", TK_ARGV_STRING, (char *) NULL, (char *) &name,
	"Name to use for application"},
    {"-sync", TK_ARGV_CONSTANT, (char *) 1, (char *) &synchronize,
	"Use synchronous mode for display server"},
    {"-visual", TK_ARGV_STRING, (char *) NULL, (char *) &visual,
	"Visual for main window"},
    {"--", TK_ARGV_REST, (char *) 1, (char *) &rest,
	"Pass all remaining arguments through to script"},
/* for Expect */
    {"-command", TK_ARGV_GENFUNC, (char *) optcmd_eval, (char *)0,
	"Command(s) to execute immediately"},
    {"-diag", TK_ARGV_CONSTANT, (char *) optcmd_diagToStderr, (char *)0,
	"Enable diagnostics"},
    {"-norc", TK_ARGV_CONSTANT, (char *) 0, (char *) &my_rc,
	"Don't read ~/.expect.rc"},
    {"-NORC", TK_ARGV_CONSTANT, (char *) 0, (char *) &sys_rc,
	"Don't read system-wide expect.rc"},
    {"-version", TK_ARGV_CONSTANT, (char *) 1, (char *) &print_version,
	"Print version and exit"},
#if TCL_DEBUGGER
    {"-Debug", TK_ARGV_GENFUNC, (char *) optcmd_debug, (char *)0, 
	"Enable debugger"},
#endif
    {(char *) NULL, TK_ARGV_END, (char *) NULL, (char *) NULL,
	(char *) NULL}
};

/*
 *----------------------------------------------------------------------
 *
 * Tk_Init --
 *
 *	This procedure is invoked to add Tk to an interpreter.  It
 *	incorporates all of Tk's commands into the interpreter and
 *	creates the main window for a new Tk application.  If the
 *	interpreter contains a variable "argv", this procedure
 *	extracts several arguments from that variable, uses them
 *	to configure the main window, and modifies argv to exclude
 *	the arguments (see the "wish" documentation for a list of
 *	the arguments that are extracted).
 *
 * Results:
 *	Returns a standard Tcl completion code and sets the interp's
 *	result if there is an error.
 *
 * Side effects:
 *	Depends on various initialization scripts that get invoked.
 *
 *----------------------------------------------------------------------
 */

int
Tk_Init2(interp)
    Tcl_Interp *interp;		/* Interpreter to initialize. */
{
    CONST char *p;
    char* alist, *cstr;
    int argc, code;
    char **argv, *args[20];
    Tcl_DString class;
    char buffer[30];

    /*
     * If there is an "argv" variable, get its value, extract out
     * relevant arguments from it, and rewrite the variable without
     * the arguments that we used.
     */

    synchronize = 0;
    name = display = geometry = colormap = visual = NULL; 
    p = Tcl_GetVar2(interp, "argv", (char *) NULL, TCL_GLOBAL_ONLY);
    argv = NULL;
    if (p != NULL) {
	if (Tcl_SplitList(interp, p, &argc, &argv) != TCL_OK) {
	    argError:
	    Tcl_AddErrorInfo(interp,
		    "\n    (processing arguments in argv variable)");
	    return TCL_ERROR;
	}
	if (Tk_ParseArgv(interp, (Tk_Window) NULL, &argc, argv,
		argTable, TK_ARGV_DONT_SKIP_FIRST_ARG|TK_ARGV_NO_DEFAULTS)
		!= TCL_OK) {
	    ckfree((char *) argv);
	    goto argError;
	}

	if (print_version) {
	    extern char exp_version[];
	    printf ("expectk version %s\n", exp_version);

	    /* SF #439042 -- Allow overide of "exit" by user / script
	     */
	    {
	      char buffer [] = "exit 0";
	      Tcl_Eval(interp, buffer); 
	    }
	}

	alist = Tcl_Merge(argc, argv);
	Tcl_SetVar2(interp, "argv", (char *) NULL, alist, TCL_GLOBAL_ONLY);
	sprintf(buffer, "%d", argc);
	Tcl_SetVar2(interp, "argc", (char *) NULL, buffer, TCL_GLOBAL_ONLY);
	ckfree(alist);
    }

    /*
     * Figure out the application's name and class.
     */

    if (name == NULL) {
	name = Tcl_GetVar(interp, "argv0", TCL_GLOBAL_ONLY);
	if ((name == NULL) || (*name == 0)) {
	    name = "tk";
	} else {
	    p = (char *)strrchr(name, '/');     /* added cast - DEL */
	    if (p != NULL) {
		name = p+1;
	    }
	}
    }
    Tcl_DStringInit(&class);
    Tcl_DStringAppend(&class, name, -1);
    cstr = Tcl_DStringValue(&class);
    if (islower(*cstr)) {
	*cstr = toupper((unsigned char) *cstr);
    }

    /*
     * Create an argument list for creating the top-level window,
     * using the information parsed from argv, if any.
     */

    args[0] = "toplevel";
    args[1] = ".";
    args[2] = "-class";
    args[3] = Tcl_DStringValue(&class);
    argc = 4;
    if (display != NULL) {
	args[argc] = "-screen";
	args[argc+1] = display;
	argc += 2;

	/*
	 * If this is the first application for this process, save
	 * the display name in the DISPLAY environment variable so
	 * that it will be available to subprocesses created by us.
	 */

	if (numMainWindows == 0) {
	    Tcl_SetVar2(interp, "env", "DISPLAY", display, TCL_GLOBAL_ONLY);
	}
    }
    if (colormap != NULL) {
	args[argc] = "-colormap";
	args[argc+1] = colormap;
	argc += 2;
    }
    if (visual != NULL) {
	args[argc] = "-visual";
	args[argc+1] = visual;
	argc += 2;
    }
    args[argc] = NULL;
    code = TkCreateFrame((ClientData) NULL, interp, argc, args, 1, name);
    Tcl_DStringFree(&class);
    if (code != TCL_OK) {
	goto done;
    }
    Tcl_ResetResult(interp);
#ifndef MAC_OSX_TK
    if (synchronize) {
	XSynchronize(Tk_Display(Tk_MainWindow(interp)), True);
    }
#endif

    /*
     * Set the geometry of the main window, if requested.  Put the
     * requested geometry into the "geometry" variable.
     */

    if (geometry != NULL) {
	Tcl_SetVar(interp, "geometry", geometry, TCL_GLOBAL_ONLY);
	code = Tcl_VarEval(interp, "wm geometry . ", geometry, (char *) NULL);
	if (code != TCL_OK) {
	    goto done;
	}
    }
    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 1) == NULL) {
	code = TCL_ERROR;
	goto done;
    }
    code = Tcl_PkgProvide(interp, "Tk", TK_VERSION);
    if (code != TCL_OK) {
	goto done;
    }

    /*
     * Invoke platform-specific initialization.
     */

    code = TkpInit(interp, 0);

    done:
    if (argv != NULL) {
	ckfree((char *) argv);
    }
    return code;
}

/*ARGSUSED*/
static int
optcmd_eval(dst,interp,key,argc,argv)
char *dst;
Tcl_Interp *interp;
char *key;
int argc;
char **argv;
{
	int i;
	int rc;

	exp_cmdlinecmds = 1;

	rc = Tcl_Eval(interp,argv[0]);
	if (rc == TCL_ERROR) return -1;

	argc--;
	for (i=0;i<argc;i++) {
		argv[i] = argv[i+1];
	}

	return argc;
}

static int
optcmd_diagToStderr(dst,interp,key,argc,argv)
    char *dst;
    Tcl_Interp *interp;
    char *key;
    int argc;
    char **argv;
{
    expDiagToStderrSet(1);
    return --argc;  /* what the heck is the convention here!! */
}

#ifdef TCL_DEBUGGER
/*ARGSUSED*/
static int
optcmd_debug(dst,interp,key,argc,argv)
char *dst;
Tcl_Interp *interp;
char *key;
int argc;
char **argv;
{
	int i;

	if (argc == 0) {
	Tcl_SetResult (interp,"-Debug flag needs 1 or 0 argument", TCL_STATIC);
		return -1;
	}

	if (Tcl_GetInt(interp,argv[0],&i) != TCL_OK) {
		return -1;
	}

	if (i) {
		Dbg_On(interp,0);
	}

	argc--;
	for (i=0;i<argc;i++) {
		argv[i] = argv[i+1];
	}

	return argc;
}
#endif /*TCL_DEBUGGER*/

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
