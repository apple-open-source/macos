/* 
 * tdomStubLib.c --
 *
 *	Stub object that will be statically linked into extensions that wish
 *	to access Tdom.
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
 * to build an extension that references Tdom_InitStubs but doesn't end up
 * including the rest of the stub functions.
 */

#ifndef USE_TCL_STUBS
#define USE_TCL_STUBS
#endif
#undef USE_TCL_STUB_PROCS

#include <dom.h>
#include <tdom.h>

/*
 * Ensure that Tdom_InitStubs is built as an exported symbol.  The other stub
 * functions should be built as non-exported symbols.
 */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

TdomStubs *tdomStubsPtr;

/*
 *----------------------------------------------------------------------
 *
 * Tdom_InitStubs --
 *
 *	Checks that the correct version of Tdom is loaded and that it
 *	supports stubs. It then initialises the stub table pointers.
 *
 * Results:
 *	The actual version of Tdom that satisfies the request, or
 *	NULL to indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */

CONST char *
Tdom_InitStubs (Tcl_Interp *interp, char *version, int exact)
{
    CONST char *actualVersion;

#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 0)
    actualVersion = Tcl_PkgRequire(interp, "tdom", version, exact);
#else
    actualVersion = Tcl_PkgRequireEx(interp, "tdom", version, exact,
                                     (ClientData *) &tdomStubsPtr);
#endif

    if (!actualVersion) {
        return NULL;
    }
    if (!tdomStubsPtr) {
        Tcl_SetResult(interp,
                      "This implementation of Tdom does not support stubs",
                      TCL_STATIC);
        return NULL;
    }
  
    return actualVersion;
}
