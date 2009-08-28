/*----------------------------------------------------------------------------
|   Copyright (c) 2007 Rolf Ade (rolf@pointsman.de)
+-----------------------------------------------------------------------------
|
|   $Id: tclAppInit.c,v 1.5 2007/08/11 22:20:11 rolf Exp $
|
|
|   Main file for a standalone tclsh with tDOM build in ('big tclsh').
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
|   written by Rolf Ade
|   August, 2007
|
\---------------------------------------------------------------------------*/

#include "tcl.h"
 
extern int Tdom_Init _ANSI_ARGS_((Tcl_Interp *interp));
extern int Tdom_SafeInit _ANSI_ARGS_((Tcl_Interp *interp));

/*----------------------------------------------------------------------------
|   main
|
\---------------------------------------------------------------------------*/
int
main(
    int    argc,
    char **argv
    )
{
    Tcl_Main (argc, argv, Tcl_AppInit);
    return 0;
}

/*----------------------------------------------------------------------------
|   Tcl_AppInit
|
\---------------------------------------------------------------------------*/
int
Tcl_AppInit(interp)
    Tcl_Interp *interp;
{
    if (Tcl_Init(interp) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (Tdom_Init(interp) == TCL_ERROR) {
        return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "tdom", Tdom_Init, Tdom_SafeInit);
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.tcldomshrc", TCL_GLOBAL_ONLY);
    return TCL_OK;
}
