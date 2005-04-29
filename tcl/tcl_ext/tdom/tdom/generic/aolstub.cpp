/*
 * aolstub.cpp --
 *
 * Adds interface for loading the extension into the AOLserver.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Rcsid: @(#)$Id: aolstub.cpp,v 1.5 2003/05/16 14:39:27 zoran Exp $
 * ---------------------------------------------------------------------------
 */

#if 1 && defined (NS_AOLSERVER)
#include <ns.h>

int Ns_ModuleVersion = 1;

/*
 *----------------------------------------------------------------------------
 *
 * NsTdom_Init --
 *
 *    Loads the package for the first time, i.e. in the startup thread.
 *
 * Results:
 *    Standard Tcl result
 *
 * Side effects:
 *    Package initialized. Tcl commands created.
 *
 *----------------------------------------------------------------------------
 */

static int
NsTdom_Init (Tcl_Interp *interp, void *context)
{
    int ret = Tdom_Init(interp);

    if (ret != TCL_OK) {
        Ns_Log(Warning, "can't load module %s: %s", 
               (char *)context, Tcl_GetStringResult(interp));
    }

    return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 * Ns_ModuleInit --
 *
 *    Called by the AOLserver when loading shared object file.
 *
 * Results:
 *    Standard AOLserver result
 *
 * Side effects:
 *    Many. Depends on the package.
 *
 *----------------------------------------------------------------------------
 */

int
Ns_ModuleInit(char *srv, char *mod)
{
    return (Ns_TclInitInterps(srv, NsTdom_Init, (void*)mod) == TCL_OK)
        ? NS_OK : NS_ERROR; 
}

#endif /* NS_AOLSERVER */

/* EOF $RCSfile: aolstub.cpp,v $ */

/* Emacs Setup Variables */
/* Local Variables:      */
/* mode: C               */
/* indent-tabs-mode: nil */
/* c-basic-offset: 4     */
/* End:                  */
