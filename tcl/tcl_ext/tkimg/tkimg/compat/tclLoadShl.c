/*
 * tclLoadShl.c --
 *
 *	This procedure provides a version of the TclLoadFile that works
 *	with the "shl_load" and "shl_findsym" library procedures for
 *	dynamic loading (e.g. for HP machines).
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclLoadShl.c 1.5 96/03/15 15:01:44
 */

#include <dl.h>

/*
 * On some HP machines, dl.h defines EXTERN; remove that definition.
 */

#ifdef EXTERN
#   undef EXTERN
#endif

#include "tcl.h"
#include "compat/dlfcn.h"
#include <errno.h>

#ifndef DYNAMIC_PATH
#    define DYNAMIC_PATH 0
#endif

VOID *
dlopen(path, mode)
    const char *path;
    int mode;
{
    int flags, length;

    if (path == (char *) NULL) {
	return (VOID *) PROG_HANDLE;
    }
    flags = ((mode & RTLD_NOW) ? BIND_IMMEDIATE : BIND_DEFERRED) |
	    DYNAMIC_PATH;
#ifdef BIND_VERBOSE
    length = strlen(path);
    if ((length > 2) && !(strcmp(path+length-3,".sl"))) {
	flags |= BIND_VERBOSE;
    }
#endif
    return (VOID *) shl_load(path, flags, 0L);
}

VOID *
dlsym(handle, symbol)
    VOID *handle;
    const char *symbol;
{   VOID *address;

    if (shl_findsym((shl_t *)&handle, symbol,
	    (short) TYPE_UNDEFINED, &address) != 0) {
	address = NULL;
    }
    return address;
}

char *
dlerror()
{
    return Tcl_ErrnoMsg(errno);
}

int
dlclose(handle)
    VOID *handle;
{
    return shl_unload((shl_t) handle);
}
