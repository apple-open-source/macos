/*
 * jpegtcl.c --
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
 * $Id: jpegtcl.c 286 2010-07-07 11:08:08Z nijtmans $
 *
 */

#include "jpegtcl.h"

/*
 *----------------------------------------------------------------------------
 *
 * Jpegtcl_Init --
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
Jpegtcl_Init (interp)
      Tcl_Interp *interp; /* Interpreter to initialise. */
{
  extern const JpegtclStubs jpegtclStubs;

  if (Tcl_InitStubs(interp, "8.3", 0) == NULL) {
    return TCL_ERROR;
  }
  if (Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION,
		       (ClientData) &jpegtclStubs) != TCL_OK) {
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * Jpegtcl_SafeInit --
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
Jpegtcl_SafeInit (interp)
      Tcl_Interp *interp; /* Interpreter to initialise. */
{
    return Jpegtcl_Init(interp);
}
