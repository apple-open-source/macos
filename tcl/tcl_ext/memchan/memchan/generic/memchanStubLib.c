/* 
 * memchanStubLib.c -- 
 *
 *	Stub object that will be statically linked into extensions that wish
 *	to access the Memchan API.
 *
 * Copyright (c) 1998 Paul Duffin.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * $Id: memchanStubLib.c,v 1.2 2005/06/08 17:47:59 andreas_kupries Exp $
 */

#ifndef USE_TCL_STUBS
#define USE_TCL_STUBS
#endif

#include "memchan.h"
#include "buf.h"

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

extern BufStubs *bufStubsPtr;
MemchanStubs *memchanStubsPtr;

/*
 *----------------------------------------------------------------------
 *
 * Memchan_InitStubs --
 *
 *	Loads the Memchan extension and initializes the stubs table.
 *
 * Results:
 *	The actual version of Memchan in use. NULL if an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */

CONST char *
Memchan_InitStubs(interp, version, exact)
    Tcl_Interp *interp;
    CONST char *version;
    int exact;
{
    CONST char *result;

    /* HACK: de-CONST 'version' if compiled against 8.3.
     * The API has no CONST despite not modifying the argument
     * And a debug build with high warning-level on windows
     * will abort the compilation.
     */

#if ((TCL_MAJOR_VERSION < 8) || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION < 4)))
#define UNCONST (char*)
#else
#define UNCONST 
#endif

    /* NOTE: Memchan actuall provide the Buf stubs. The Memchan stubs
     *       table is hooked into this.
     */

    result = Tcl_PkgRequireEx(interp, "Memchan", UNCONST version, exact,
		(ClientData *) &bufStubsPtr);
    if (!result || !bufStubsPtr) {
        return (char *) NULL;
    }

    memchanStubsPtr = bufStubsPtr->hooks->memchanStubs;
    return result;
}
#undef UNCONST
