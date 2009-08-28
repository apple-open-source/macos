/* $Id: aolstub.c,v 1.1 2004/05/23 22:50:39 neumann Exp $

   This file provides the stubs needed for the AOL_SERVER,
   Please note, that you have to have to apply a small patch
   to the AOL server as well (available from www.xotcl.org)
   in order to get it working.

   Authore:
      Zoran Vasiljevic
      Archiware Inc.

*/
#ifdef AOL_SERVER


#include "xotcl.h"
#include <ns.h>

int Ns_ModuleVersion = 1;

#if NS_MAJOR_VERSION>=4
# define AOL4
#endif

/*
 *----------------------------------------------------------------------------
 *
 * NsXotcl_Init --
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
NsXotcl_Init (Tcl_Interp *interp, void *context)
{
 static int firsttime = 1;
 int ret;
 
 ret = Xotcl_Init(interp);
 
 if (firsttime) {
   if (ret != TCL_OK) {
     Ns_Log(Warning, "can't load module %s: %s", (char *)context,
	    Tcl_GetStringResult(interp));
   } else {
     Ns_Log(Notice, "%s module version %s%s", (char*)context, 
	    XOTCLVERSION,XOTCLPATCHLEVEL);
     /*
      * Import the XOTcl namespace only for the shell after 
      * predefined is through
      */
     Tcl_Import(interp, Tcl_GetGlobalNamespace(interp), 
		"xotcl::*", 0);
   }
   firsttime = 0;
 }

 return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 * NsXotcl_Init1 --
 *
 *    Loads the package in each thread-interpreter.
 *    This is needed since XOTcl Class/Object commands are not copied 
 *    from the startup thread to the connection (or any other) thread.
 *    during AOLserver initialization and/or thread creation times.
 *
 *    Why ?
 *    
 *    Simply because these two commands declare a delete callback which is
 *    unsafe to call in any other thread but in the one which created them.
 *
 *    To understand this, you may need to get yourself acquainted with the
 *    mechanics of the AOLserver, more precisely, with the way Tcl interps
 *    are initialized (dive into nsd/tclinit.c in AOLserver distro).
 *   
 *    So, we made sure (by patching the AOLserver code) that no commands with 
 *    delete callbacks declared, are ever copied from the startup thread.
 *    Additionaly, we also made sure that AOLserver properly invokes any
 *    AtCreate callbacks. So, instead of activating those callbacks *after*
 *    running the Tcl-initialization script (which is the standard behaviour)
 *    we activate them *before*. So we may get a chance to configure the
 *    interpreter correctly for any commands within the init script. 
 *    
 *    Proper XOTcl usage would be to declare all resources (classes, objects)
 *    at server initialization time and let AOLserver machinery to copy them
 *    (or re-create them, better yet) in each new thread.
 *    Resources created within a thread are automatically garbage-collected
 *    on thread-exit time, so don't create any XOTcl resources there.
 *    Create them in the startup thread and they will automatically be copied 
 *    for you. 
 *    Look in <serverroot>/modules/tcl/xotcl for a simple example. 
 *
 * Results:
 *    Standard Tcl result.
 *
 * Side effects:
 *    Tcl commands created.
 *
 *----------------------------------------------------------------------------
 */

static int
NsXotcl_Init1 (Tcl_Interp *interp, void *notUsed)
{
  int result;

#ifndef AOL4
  result = Xotcl_Init(interp);
#else
  result = TCL_OK;
#endif
  
  /*
   * Import the XOTcl namespace only for the shell after 
   * predefined is through
   */
  Tcl_Import(interp, Tcl_GetGlobalNamespace(interp), "xotcl::*", 1);

  return result;
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
Ns_ModuleInit(char *hServer, char *hModule)
{
  int ret;
  
  /*Ns_Log(Notice, "+++ ModuleInit","INIT");*/
  ret = Ns_TclInitInterps(hServer, NsXotcl_Init, (void*)hModule);

  if (ret == TCL_OK) {
    /*
     * See discussion for NsXotcl_Init1 procedure.
     * Note that you need to patch AOLserver for this to work!
     * The patch basically forbids copying of C-level commands with
     * declared delete callbacks. It also runs all AtCreate callbacks
     * BEFORE AOLserver runs the Tcl script for initializing new interps.
     * These callbacks are then responsible for setting up the stage
     * for correct (XOTcl) extension startup (including copying any
     * XOTcl resources (classes, objects) created in the startup thread.
     */
    Ns_TclRegisterAtCreate((Ns_TclInterpInitProc *)NsXotcl_Init1, NULL);
  }

  return ret == TCL_OK ? NS_OK : NS_ERROR;
}
#endif
