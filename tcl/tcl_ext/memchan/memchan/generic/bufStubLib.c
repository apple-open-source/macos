/* 
 * bufStubLib.c --
 *
 *	Stub object that will be statically linked into extensions that wish
 *	to access Buf.
 */


/*
 * We need to ensure that we use the stub macros so that this file contains
 * no references to any of the stub functions.  This will make it possible
 * to build an extension that references Buf_InitStubs but doesn't end up
 * including the rest of the stub functions.
 */

#ifndef USE_TCL_STUBS
#define USE_TCL_STUBS
#endif
#undef  USE_TCL_STUB_PROCS

#ifndef USE_BUF_STUBS
#define USE_BUF_STUBS
#endif
#undef  USE_BUF_STUB_PROCS

#include "buf.h"
#include "bufIntDecls.h"

/*
 * Ensure that Buf_InitStubs is built as an exported symbol.  The other stub
 * functions should be built as non-exported symbols.
 */

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

BufStubs *bufStubsPtr;
BufIntStubs *bufIntStubsPtr;


/*
 *----------------------------------------------------------------------
 *
 * Buf_InitStubs --
 *
 *	Checks that the correct version of Buf is loaded and that it
 *	supports stubs. It then initialises the stub table pointers.
 *
 * Results:
 *	The actual version of Buf that satisfies the request, or
 *	NULL to indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */

#ifdef Buf_InitStubs
#undef Buf_InitStubs
#endif

char *
Buf_InitStubs(interp, version, exact)
    Tcl_Interp *interp;
    CONST char *version;
    int exact;
{
    CONST char *actualVersion;

    /* ** NOTE ** the speciality:
     * The interface 'Buf' is provided by the package 'Memchan'.
     */

    actualVersion = Tcl_PkgRequireEx(interp, "Memchan", MC_UNCONSTB84 version,
		exact, (ClientData *) &bufStubsPtr);

    if (!actualVersion) {
	return NULL;
    }

    if (!bufStubsPtr) {
	Tcl_SetResult(interp,
		"This implementation of Buf does not support stubs",
		TCL_STATIC);
	return NULL;
    }
    
    bufIntStubsPtr = bufStubsPtr->hooks->bufIntStubs;
    
    return (char*) actualVersion;
}
