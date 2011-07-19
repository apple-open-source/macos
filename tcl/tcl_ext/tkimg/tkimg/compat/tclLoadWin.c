/*
 * tclLoadWin.c --
 *
 *	This procedure provides a version of dlopen() that
 *	works with the Windows "LoadLibrary" and "GetProcAddress"
 *	API for dynamic loading.
 *
 */

#include "imgInt.h"
#ifdef USE_TCL_STUBS
#   include "tclWinInt.h"
#else
#   include <windows.h>
#endif
#include "compat/dlfcn.h"

typedef struct LibraryList {
    HINSTANCE handle;
    struct LibraryList *nextPtr;
} LibraryList;

static LibraryList *libraryList = NULL;	/* List of currently loaded DLL's.  */

/*
 * Declarations for functions that are only used in this file.
 */

static void UnloadLibraries(ClientData clientData);


/*
 *----------------------------------------------------------------------
 *
 * dlopen --
 *
 *	This function is an alternative for the functions
 *	TclWinLoadLibrary and TclWinGetTclInstance.  It is
 *	responsible for adding library handles to the library list so
 *	the libraries can be freed when tcl.dll is unloaded.
 *
 * Results:
 *	Returns the handle of the newly loaded library, or NULL on
 *	failure. If path is NULL, the global library instance handle
 *	is returned.
 *
 * Side effects:
 *	Loads the specified library into the process.
 *
 *----------------------------------------------------------------------
 */

void *dlopen(
    const char *path,
    int mode
) {
    void *handle;
    LibraryList *ptr;
    static int initialized = 0;

    if (!initialized) {
	initialized = 1;
	Tcl_CreateExitHandler((Tcl_ExitProc *) UnloadLibraries,
	    (ClientData) &libraryList);
    }
    handle = (void *) LoadLibrary(path);
    if (handle != NULL) {
	    ptr = (LibraryList*) ckalloc(sizeof(LibraryList));
	    ptr->handle = (HINSTANCE) handle;
	    ptr->nextPtr = libraryList;
	    libraryList = ptr;
    }
    return handle;
}


/*
 *----------------------------------------------------------------------
 *
 * dlclose --
 *
 *	This function is an alternative for the function
 *	FreeLibrary.  It is responsible for removing library
 *	handles from the library list and remove the dll
 *	from memory.
 *
 * Results:
 *	-1 on error, 0 on success.
 *
 * Side effects:
 *	Removes the specified library from the process.
 *
 *----------------------------------------------------------------------
 */

int
dlclose(
    void *handle
) {
    LibraryList *ptr, *prevPtr;

    ptr = libraryList; prevPtr = NULL;
    while (ptr != NULL) {
	if (ptr->handle == (HINSTANCE) handle) {
	    FreeLibrary((HINSTANCE) handle);
	    if (prevPtr) {
		prevPtr->nextPtr = ptr->nextPtr;
	    } else {
		libraryList = ptr->nextPtr;
	    }
	    ckfree((char *) ptr);
	    return 0;
	}
	prevPtr = ptr;
	ptr = ptr->nextPtr;
    }
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * dlsym --
 *
 *	This function is an alternative for the system function
 *	GetProcAddress. It returns the address of a
 *	symbol, give the handle returned by dlopen().
 *
 * Results:
 *	Returns the address of the symbol in the dll.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void *dlsym(
    void *handle,
    const char *symbol
) {
    return (void *) GetProcAddress((HINSTANCE) handle, symbol);
}


/*
 *----------------------------------------------------------------------
 *
 * dlerror --
 *
 *	This function returns a string describing the error which
 *	occurred in dlopen().
 *
 * Results:
 *	Returns an error message.
 *
 * Side effects:
 *	errno is set.
 *
 *----------------------------------------------------------------------
 */

char *
dlerror(
) {
    TclWinConvertError(GetLastError());
    return (char *) Tcl_ErrnoMsg(Tcl_GetErrno());
}

/*
 *----------------------------------------------------------------------
 *
 * UnloadLibraries --
 *
 *	Frees any dynamically allocated libraries loaded by Tcl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the libraries on the library list as well as the list.
 *
 *----------------------------------------------------------------------
 */

static void
UnloadLibraries(
    ClientData clientData
) {
    LibraryList *ptr;
    LibraryList *list = *((LibraryList **) clientData);

    while (list != NULL) {
	FreeLibrary(list->handle);
	ptr = list->nextPtr;
	ckfree((char *) list);
	list = ptr;
    }
}
