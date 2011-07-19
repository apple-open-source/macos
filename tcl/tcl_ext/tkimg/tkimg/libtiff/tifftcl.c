/*
 * tifftcl.c --
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
 * $Id: tifftcl.c 290 2010-07-08 08:53:05Z nijtmans $
 *
 */

#include "tifftcl.h"

/*
 * Declarations for externally visible functions.
 */

extern DLLEXPORT int Tifftcl_Init(Tcl_Interp *interp);
extern DLLEXPORT int Tifftcl_SafeInit(Tcl_Interp *interp);

/*
 * Prototypes for procedures defined later in this file:
 */

/*
 *----------------------------------------------------------------------------
 *
 * Tifftcl_Init --
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
Tifftcl_Init (interp)
      Tcl_Interp *interp; /* Interpreter to initialise. */
{
  extern const TifftclStubs tifftclStubs;

  if (Tcl_InitStubs(interp, "8.3", 0) == NULL) {
    return TCL_ERROR;
  }
  if (Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION,
		       (ClientData) &tifftclStubs) != TCL_OK) {
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * Tifftcl_SafeInit --
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
Tifftcl_SafeInit (interp)
      Tcl_Interp *interp; /* Interpreter to initialise. */
{
    return Tifftcl_Init(interp);
}

/*
 *----------------------------------------------------------------------------
 *
 * Tifftcl_XXX --
 *
 *  Wrappers around the zlib functionality.
 *
 * Results:
 *  Depends on function.
 *
 * Side effects:
 *  Depends on function.
 *
 *----------------------------------------------------------------------------
 */

/*
 * No wrappers are required. Due to intelligent definition of the stub
 * table using the function names of the libz sources the stub table
 * contains jumps to the actual functionality.
 */
