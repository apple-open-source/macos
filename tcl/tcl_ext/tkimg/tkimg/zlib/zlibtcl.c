/*
 * zlibtcl.c --
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
 * $Id: zlibtcl.c 274 2010-06-28 13:23:34Z nijtmans $
 *
 */

#include "zlibtcl.h"

MODULE_SCOPE const ZlibtclStubs zlibtclStubs;

/*
 * Prototypes for procedures defined later in this file:
 */

/*
 *----------------------------------------------------------------------------
 *
 * Zlibtcl_Init --
 *
 *  Initialisation routine for loadable module
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter,
 *  loads zlibtcl package.
 *
 *----------------------------------------------------------------------------
 */

int
Zlibtcl_Init (
	Tcl_Interp *interp /* Interpreter to initialise. */
) {
	if (!Tcl_InitStubs(interp, "8.3", 0)) {
		return TCL_ERROR;
	}

	if (Tcl_PkgProvideEx(interp, PACKAGE_NAME, PACKAGE_VERSION,
			(void *) &zlibtclStubs) != TCL_OK) {
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * Zlibtcl_SafeInit --
 *
 *  Initialisation routine for loadable module in a safe interpreter.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter,
 *  loads zlibtcl package.
 *
 *----------------------------------------------------------------------------
 */

int
Zlibtcl_SafeInit (
	Tcl_Interp *interp /* Interpreter to initialise. */
) {
	return Zlibtcl_Init(interp);
}
