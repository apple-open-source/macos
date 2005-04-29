/* 
 * snackStubLib.c --
 *
 *	Stub object that will be statically linked into extensions that wish
 *	to access Snack.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * Copyright (c) 1998 Paul Duffin.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

/*
 * We need to ensure that we use the stub macros so that this file contains
 * no references to any of the stub functions.  This will make it possible
 * to build an extension that references Snack_InitStubs but doesn't end up
 * including the rest of the stub functions.
 */

#ifndef USE_TCL_STUBS
#define USE_TCL_STUBS
#endif
#undef USE_TCL_STUB_PROCS

#ifndef USE_TK_STUBS
#define USE_TK_STUBS
#endif
#undef USE_TK_STUB_PROCS

#include "snack.h"
#include "snackDecls.h"

/*
 * Ensure that Snack_InitStubs is built as an exported symbol.  The other stub
 * functions should be built as non-exported symbols.
 */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

SnackStubs *snackStubsPtr;

/*
 *----------------------------------------------------------------------
 *
 * Snack_InitStubs --
 *
 *	Checks that the correct version of Snack is loaded and that it
 *	supports stubs. It then initialises the stub table pointers.
 *
 * Results:
 *	The actual version of Snack that satisfies the request, or
 *	NULL to indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */

CONST84 char *
Snack_InitStubs (Tcl_Interp *interp, char *version, int exact)
{
  CONST84 char *actualVersion;
  
  actualVersion = Tcl_PkgRequireEx(interp, "snack", version, exact,
				   (ClientData *) &snackStubsPtr);
  if (!actualVersion) {
    return NULL;
  }
  
  if (!snackStubsPtr) {
    Tcl_SetResult(interp,
		  "This implementation of Snack does not support stubs",
		  TCL_STATIC);
    return NULL;
  }
  
  return actualVersion;
}
