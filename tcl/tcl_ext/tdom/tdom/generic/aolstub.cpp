/*
 * aolstub.cpp --
 *
 * Adds interface for loading the extension into the AOLserver.
 *
 * See the file "LICENSE" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Rcsid: @(#)$Id: aolstub.cpp,v 1.7 2007/08/10 22:46:45 rolf Exp $
 * ---------------------------------------------------------------------------
 */

#if defined (NS_AOLSERVER)
#include <ns.h>

int Ns_ModuleVersion = 1;

/*
 * Structure to pass to NsThread_Init. This holds the module
 * and virtual server name for proper interp initializations. 
 * This is valid only for AOLservers 4.x or later.
 */

struct mydata {
    char *modname;
    char *server;
};

/*
 *----------------------------------------------------------------------------
 *
 * NsTdom_Init --
 *
 *    Loads the package in the Tcl interpreter. 
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
NsTdom_Init (Tcl_Interp *interp, void *cd)
{
    struct mydata *md = (struct mydata*)cd;
    int ret = Tdom_Init(interp);

    if (ret != TCL_OK) {
        Ns_Log(Warning, "can't load module %s: %s", md->modname,
               Tcl_GetStringResult(interp));
    }
    Tcl_SetAssocData(interp, "tdom:nsd", NULL, (ClientData)md);

    return TCL_OK;
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
    struct mydata *md = NULL;

    md = (struct mydata*)ns_malloc(sizeof(struct mydata));
    md->modname = strcpy(ns_malloc(strlen(mod)+1), mod);
    md->server  = strcpy(ns_malloc(strlen(srv)+1), srv);

    return (Ns_TclInitInterps(srv, NsTdom_Init, (void*)md) == TCL_OK)
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
