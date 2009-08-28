/* 
 * cmpRPkg.c --
 *
 *  This file contains the C interfaces to the Tcl load command for the
 *  Reader package: the Reader_Init function.
 *
 * Copyright (c) 1998-2000 Ajuba Solutions
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: cmpRPkg.c,v 1.2 2000/05/30 22:19:06 wart Exp $
 */

#include "cmpInt.h"
#include "proTbcLoad.h"

/*
 * name and version of this package
 */

static char packageName[]    = PACKAGE_NAME;
static char packageVersion[] = PACKAGE_VERSION;

/*
 * Name of the commands exported by this package
 */

static char evalCommand[] = CMP_EVAL_COMMAND;
static char procCommand[] = CMP_PROC_COMMAND;

/*
 * this struct describes an entry in the table of command names and command
 * procs
 */

typedef struct CmdTable
{
    char *cmdName;		/* command name */
    Tcl_ObjCmdProc *proc;	/* command proc */
    int exportIt;		/* if 1, export the command */
} CmdTable;

/*
 * Declarations for functions defined in this file.
 */

static int	TbcloadInitInternal _ANSI_ARGS_((Tcl_Interp *interp,
			int isSafe));
static int	RegisterCommand _ANSI_ARGS_((Tcl_Interp* interp,
			char *namespace, CONST CmdTable *cmdTablePtr));

/*
 * List of commands to create when the package is loaded; must go after the
 * declarations of the enable command procedure.
 */

static CONST CmdTable commands[] =
{
    { evalCommand,	Tbcload_EvalObjCmd,	1 },
    { procCommand,	Tbcload_ProcObjCmd,	1 },

    { 0, 0, 0 }
};

static CONST CmdTable safeCommands[] =
{
    { evalCommand,	Tbcload_EvalObjCmd,	1 },
    { procCommand,	Tbcload_ProcObjCmd,	1 },

    { 0, 0, 0 }
};

/*
 *----------------------------------------------------------------------
 *
 * Tbcload_Init --
 *
 *  This procedure initializes the Loader package.
 *  The initialization consists of add ing the compiled script loader to the
 *  set of registered script loaders.
 *
 * Results:
 *  A standard Tcl result.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

int
Tbcload_Init(interp)
    Tcl_Interp *interp;		/* the Tcl interpreter for which the package
                                 * is initialized */
{
    return TbcloadInitInternal(interp, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * Tbcload_SafeInit --
 *
 *  This procedure initializes the Loader package.
 *  The initialization consists of add ing the compiled script loader to the
 *  set of registered script loaders.
 *
 * Results:
 *  A standard Tcl result.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

int
Tbcload_SafeInit(interp)
    Tcl_Interp *interp;		/* the Tcl interpreter for which the package
                                 * is initialized */
{
    return TbcloadInitInternal(interp, 1);
}

/*
 *----------------------------------------------------------------------
 *
 * RegisterCommand --
 *
 *  This procedure registers a command in the context of the given namespace.
 *
 * Results:
 *  A standard Tcl result.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static int RegisterCommand(interp, namespace, cmdTablePtr)
    Tcl_Interp* interp;			/* the Tcl interpreter for which the
                                         * operation is performed */
    char *namespace;			/* the namespace in which the command
                                         * is registered */
    CONST CmdTable *cmdTablePtr;	/* the command to register */
{
    char buf[128];

    if (cmdTablePtr->exportIt) {
        sprintf(buf, "namespace eval %s { namespace export %s }",
                namespace, cmdTablePtr->cmdName);
        if (Tcl_Eval(interp, buf) != TCL_OK)
            return TCL_ERROR;
    }
    
    sprintf(buf, "%s::%s", namespace, cmdTablePtr->cmdName);
    Tcl_CreateObjCommand(interp, buf, cmdTablePtr->proc, 0, 0);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TbcloadInitInternal --
 *
 *  This procedure initializes the Loader package.
 *  The isSafe flag is 1 if the interpreter is safe, 0 otherwise.
 *
 * Results:
 *  A standard Tcl result.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

static int
TbcloadInitInternal(interp, isSafe)
    Tcl_Interp *interp;		/* the Tcl interpreter for which the package
                                 * is initialized */
    int isSafe;			/* 1 if this is a safe interpreter */
{
    CONST CmdTable *cmdTablePtr;

    if (TbcloadInit(interp) != TCL_OK) {
        return TCL_ERROR;
    }
    
    cmdTablePtr = (isSafe) ? &safeCommands[0] : &commands[0];
    for ( ; cmdTablePtr->cmdName ; cmdTablePtr++) {
        if (RegisterCommand(interp, packageName, cmdTablePtr) != TCL_OK) {
            return TCL_ERROR;
        }
    }
    
    return Tcl_PkgProvide(interp, packageName, packageVersion);
}

/*
 *----------------------------------------------------------------------
 *
 * TbcloadGetPackageName --
 *
 *  Returns the package name for the loader package.
 *
 * Results:
 *  See above.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------
 */

CONST char *
TbcloadGetPackageName()
{
    return packageName;
}
