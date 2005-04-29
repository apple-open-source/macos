/* 
 * tclLoadDyld.c --
 *
 *     This procedure provides a version of the TclLoadFile that
 *     works with Apple's dyld dynamic loading.  This file
 *     provided by Wilfredo Sanchez (wsanchez@apple.com).
 *     This works on Mac OS X.
 *
 * Copyright (c) 1995 Apple Computer, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: trf.diff,v 1.2 2004/08/20 18:39:39 tpeterso Exp $
 */

#include <mach-o/dyld.h>
#include <errno.h>

#include "tcl.h"

typedef struct Tcl_DyldModuleHandle {
    struct Tcl_DyldModuleHandle *nextModuleHandle;
    NSModule module;
} Tcl_DyldModuleHandle;

typedef struct Tcl_DyldLoadHandle {
    const struct mach_header *dyld_lib;
    Tcl_DyldModuleHandle *firstModuleHandle;
} Tcl_DyldLoadHandle;

/*
 *----------------------------------------------------------------------
 *
 * TclpDlopen --
 *
 *	Dynamically loads a binary code file into memory and returns
 *	a handle to the new code.
 *
 * Results:
 *     A standard Tcl completion code.  If an error occurs, an error
 *     message is left in the interpreter's result. 
 *
 * Side effects:
 *     New code suddenly appears in memory.
 *
 *----------------------------------------------------------------------
 */

VOID *
dlopen(path, flags)
    CONST char *path;
    int flags;
{
    Tcl_DyldLoadHandle *dyldLoadHandle;
    const struct mach_header *dyld_lib;

    dyld_lib = NSAddImage(path, 
        NSADDIMAGE_OPTION_WITH_SEARCHING | 
        NSADDIMAGE_OPTION_RETURN_ON_ERROR);
    
    if (!dyld_lib) {
        return NULL;
    }
    dyldLoadHandle = (Tcl_DyldLoadHandle *) ckalloc(sizeof(Tcl_DyldLoadHandle));
    if (!dyldLoadHandle) return NULL;
    dyldLoadHandle->dyld_lib = dyld_lib;
    dyldLoadHandle->firstModuleHandle = NULL;
    return (Tcl_LoadHandle) dyldLoadHandle;     
}

char *
dlerror()
{
    NSLinkEditErrors editError;
    char *name, *msg;
    NSLinkEditError(&editError, &errno, &name, &msg);
    return msg;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpFindSymbol --
 *
 *	Looks up a symbol, by name, through a handle associated with
 *	a previously loaded piece of code (shared library).
 *
 * Results:
 *	Returns a pointer to the function associated with 'symbol' if
 *	it is found.  Otherwise returns NULL and may leave an error
 *	message in the interp's result.
 *
 *----------------------------------------------------------------------
 */
VOID *dlsym(handle, symbol)
    VOID *handle;
    CONST char *symbol;
{
    NSSymbol nsSymbol;
    CONST char *native;
    Tcl_DString newName, ds;
    Tcl_PackageInitProc* proc = NULL;
    Tcl_DyldLoadHandle *dyldLoadHandle = (Tcl_DyldLoadHandle *) handle;
    /* 
     * dyld adds an underscore to the beginning of symbol names.
     */

    native = Tcl_UtfToExternalDString(NULL, symbol, -1, &ds);
    Tcl_DStringInit(&newName);
    Tcl_DStringAppend(&newName, "_", 1);
    native = Tcl_DStringAppend(&newName, native, -1);
    nsSymbol = NSLookupSymbolInImage(dyldLoadHandle->dyld_lib, native, 
	NSLOOKUPSYMBOLINIMAGE_OPTION_BIND_NOW | 
	NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
    if(nsSymbol) {
	Tcl_DyldModuleHandle *dyldModuleHandle;
	proc = NSAddressOfSymbol(nsSymbol);
	dyldModuleHandle = (Tcl_DyldModuleHandle *) ckalloc(sizeof(Tcl_DyldModuleHandle));
	if (dyldModuleHandle) {
	    dyldModuleHandle->module = NSModuleForSymbol(nsSymbol);
	    dyldModuleHandle->nextModuleHandle = dyldLoadHandle->firstModuleHandle;
	    dyldLoadHandle->firstModuleHandle = dyldModuleHandle;
	}
    }
    Tcl_DStringFree(&newName);
    Tcl_DStringFree(&ds);
    
    return proc;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpUnloadFile --
 *
 *     Unloads a dynamically loaded binary code file from memory.
 *     Code pointers in the formerly loaded file are no longer valid
 *     after calling this function.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Code dissapears from memory.
 *     Note that this is a no-op on older (OpenStep) versions of dyld.
 *
 *----------------------------------------------------------------------
 */

int
dlclose(handle)
    VOID *handle;
{
    Tcl_DyldLoadHandle *dyldLoadHandle = (Tcl_DyldLoadHandle *) handle;
    Tcl_DyldModuleHandle *dyldModuleHandle = dyldLoadHandle->firstModuleHandle;
    void *ptr;

    while (dyldModuleHandle) {
	NSUnLinkModule(dyldModuleHandle->module, NSUNLINKMODULE_OPTION_NONE);
	ptr = dyldModuleHandle;
	dyldModuleHandle = dyldModuleHandle->nextModuleHandle;
	ckfree(ptr);
    }
    ckfree(dyldLoadHandle);
}
