/*----------------------------------------------------------------------------
|   Copyright (c) 1999 Jochen Loewer (loewerj@hotmail.com)
+-----------------------------------------------------------------------------
|
|   $Header: /usr/local/pubcvs/tdom/generic/tdominit.c,v 1.13 2004/07/09 01:15:56 rolf Exp $
|
|
|   A DOM implementation for Tcl using James Clark's expat XML parser
| 
|
|   The contents of this file are subject to the Mozilla Public License
|   Version 1.1 (the "License"); you may not use this file except in
|   compliance with the License. You may obtain a copy of the License at
|   http://www.mozilla.org/MPL/
|
|   Software distributed under the License is distributed on an "AS IS"
|   basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
|   License for the specific language governing rights and limitations
|   under the License.
|
|   The Original Code is tDOM.
|
|   The Initial Developer of the Original Code is Jochen Loewer
|   Portions created by Jochen Loewer are Copyright (C) 1998, 1999
|   Jochen Loewer. All Rights Reserved.
|
|   Contributor(s):
|
|
|   written by Jochen Loewer
|   April, 1999
|
\---------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------
|   Includes
|
\---------------------------------------------------------------------------*/
#include <tcl.h>
#include <dom.h>
#include <tdom.h>
#include <tcldom.h>

extern TdomStubs tdomStubs;

/*
 *----------------------------------------------------------------------------
 *
 * Tdom_Init --
 *
 *	Initialization routine for loadable module
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Defines "expat"/"dom" commands in the interpreter.
 *
 *----------------------------------------------------------------------------
 */

int
Tdom_Init (interp)
     Tcl_Interp *interp; /* Interpreter to initialize. */
{
    char *bool = NULL;
    int   threaded = 0;

#ifdef USE_TCL_STUBS
    Tcl_InitStubs(interp, "8", 0);
#endif

    bool = (char*)Tcl_GetVar2(interp, "::tcl_platform", "threaded", 0);
    if (bool) {
        threaded = atoi(bool);
    } else {
        threaded = 0;
    }
    
#ifdef TCL_THREADS
    if (!threaded) {
        char *err = "Tcl core wasn't compiled for multithreading.";
        Tcl_SetObjResult(interp, Tcl_NewStringObj(err, -1));
        return TCL_ERROR;
    }
    domModuleInitialize();
    tcldom_initialize();
#else
    if (threaded) {
        char *err = "tDOM wasn't compiled for multithreading.";
        Tcl_SetObjResult(interp, Tcl_NewStringObj(err, -1));
        return TCL_ERROR;
    }
    domModuleInitialize();
#endif /* TCL_THREADS */

#ifndef TDOM_NO_UNKNOWN_CMD
    Tcl_Eval(interp, "rename unknown unknown_tdom");   
    Tcl_CreateObjCommand(interp, "unknown", tcldom_unknownCmd,  NULL, NULL );
#endif

    Tcl_CreateObjCommand(interp, "dom",     tcldom_DomObjCmd,   NULL, NULL );
    Tcl_CreateObjCommand(interp, "domDoc",  tcldom_DocObjCmd,   NULL, NULL );
    Tcl_CreateObjCommand(interp, "domNode", tcldom_NodeObjCmd,  NULL, NULL );
    Tcl_CreateObjCommand(interp, "tdom",    TclTdomObjCmd,      NULL, NULL );

#ifndef TDOM_NO_EXPAT    
    Tcl_CreateObjCommand(interp, "expat",       TclExpatObjCmd, NULL, NULL );
    Tcl_CreateObjCommand(interp, "xml::parser", TclExpatObjCmd, NULL, NULL );
#endif
    
#ifdef USE_TCL_STUBS
    Tcl_PkgProvideEx(interp, "tdom", STR_TDOM_VERSION(TDOM_VERSION), 
                     (ClientData) &tdomStubs);
#else
    Tcl_PkgProvide(interp, "tdom", STR_TDOM_VERSION(TDOM_VERSION));
#endif

    return TCL_OK;
}

int
Tdom_SafeInit (interp)
     Tcl_Interp *interp;
{
    return Tdom_Init (interp);
}

/*
 * Load the AOLserver stub. This allows the library
 * to be loaded as AOLserver module.
 */

#if defined (NS_AOLSERVER)
# include "aolstub.cpp"
#endif

