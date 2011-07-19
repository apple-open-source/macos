/*
 * tclLoadDld.c --
 *
 *	This procedure provides a version of dlopen() that
 *	works with the "dld_link" and "dld_get_func" library procedures
 *	for dynamic loading.  It has been tested on Linux 1.1.95 and
 *	dld-3.2.7.  This file probably isn't needed anymore, since it
 *	makes more sense to use "dl_open" etc.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclLoadDld.c 1.4 96/02/15 11:58:46
 */

#include "tcl.h"
#include "compat/dlfcn.h"
#include "dld.h"

/*
 *----------------------------------------------------------------------
 *
 * dlopen --
 *
 *	This function is an implementation of dlopen() using
 *	the dld library.
 *
 * Results:
 *	Returns the handle of the newly loaded library, or NULL on
 *	failure.
 *
 * Side effects:
 *	Loads the specified library into the process.
 *
 *----------------------------------------------------------------------
 */

static int returnCode = 0;

extern char *tclExecutableName;

void *dlopen(path, mode)
    const char *path;
    int mode;
{
    static int firstTime = 1;

    /*
     *  The dld package needs to know the pathname to the tcl binary.
     *  If that's not know, return an error.
     */

    returnCode = 0;
    if (firstTime) {
	if (tclExecutableName == NULL) {
	    return (void *) NULL;
	}
	returnCode = dld_init(tclExecutableName);
	if (returnCode != 0) {
	    return (void *) NULL;
	}
	firstTime = 0;
    }

    if ((path != NULL) && ((returnCode = dld_link(path)) != 0)) {
	return (void *) NULL;
    }

    return (void *) 1;
}

void *
dlsym(handle, symbol)
    void *handle;
    const char *symbol;
{
    return (void *) dld_get_func(symbol);
}

char *
dlerror()
{
    if (tclExecutableName == NULL) {
	return (char *) "don't know name of application binary file, so can't initialize dynamic loader";
    }
    return dld_strerror(returnCode);
}

int
dlclose(handle)
    void *handle;
{
    return 0;
}
