/* 
 * trfStubLib.c --
 *
 *	Stub object that will be statically linked into extensions that wish
 *	to access Trf.
 */


/*
 * We need to ensure that we use the stub macros so that this file contains
 * no references to any of the stub functions.  This will make it possible
 * to build an extension that references Trf_InitStubs but doesn't end up
 * including the rest of the stub functions.
 */

#ifndef USE_TCL_STUBS
#define USE_TCL_STUBS
#endif
#undef  USE_TCL_STUB_PROCS

#ifndef USE_TRF_STUBS
#define USE_TRF_STUBS
#endif
#undef  USE_TRF_STUB_PROCS

#include "transform.h"
#include "trfIntDecls.h"

/*
 * Ensure that Trf_InitStubs is built as an exported symbol.  The other stub
 * functions should be built as non-exported symbols.
 */

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

TrfStubs *trfStubsPtr;
TrfIntStubs *trfIntStubsPtr;


/*
 *----------------------------------------------------------------------
 *
 * Trf_InitStubs --
 *
 *	Checks that the correct version of Trf is loaded and that it
 *	supports stubs. It then initialises the stub table pointers.
 *
 * Results:
 *	The actual version of Trf that satisfies the request, or
 *	NULL to indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */

#ifdef Trf_InitStubs
#undef Trf_InitStubs
#endif

char *
Trf_InitStubs(interp, version, exact)
    Tcl_Interp *interp;
    CONST char *version;
    int exact;
{
    CONST char *actualVersion;

    actualVersion = Tcl_PkgRequireEx(interp, "Trf", (char *) version, exact,
		(ClientData *) &trfStubsPtr);

    if (!actualVersion) {
	return NULL;
    }

    if (!trfStubsPtr) {
	Tcl_SetResult(interp,
		"This implementation of Trf does not support stubs",
		TCL_STATIC);
	return NULL;
    }
    
    trfIntStubsPtr = trfStubsPtr->hooks->trfIntStubs;
    
    return (char*) actualVersion;
}
