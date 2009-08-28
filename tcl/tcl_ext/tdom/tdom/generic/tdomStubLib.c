/*----------------------------------------------------------------------------
|   Copyright (c) 2007 Rolf Ade (rolf@pointsman.de)
+-----------------------------------------------------------------------------
|
|   $Id: tdomStubLib.c,v 1.8 2007/08/12 11:29:25 rolf Exp $
|
|   Implements entry point, which has to be called by C coded extensions
|   to tDOM. Following http://wiki.tcl.tk/3358.
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

#ifndef USE_TCL_STUBS
#  define USE_TCL_STUBS
#endif
#undef USE_TCL_STUB_PROCS

#include <tdom.h>

/*
 * Ensure that Tdom_InitStubs is built as an exported symbol.  The other stub
 * functions should be built as non-exported symbols.
 */

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

TdomStubs *tdomStubsPtr;

/*----------------------------------------------------------------------------
|   Tdom_InitStubs
|
\---------------------------------------------------------------------------*/

CONST char *
Tdom_InitStubs (
    Tcl_Interp *interp, 
    char *version, 
    int exact
    )
{
    CONST char *actualVersion;
    ClientData clientData = NULL;

#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 0)
    Tcl_SetResult(interp, "Too old Tcl version. Binary extensions "
                  "to tDOM are not possible, with a that outdated "
                  "Tcl version.", TCL_STATIC);
    return NULL;
#else
    actualVersion = Tcl_PkgRequireEx(interp, "tdom", version, exact,
                                     (ClientData*) &clientData);
    tdomStubsPtr = (TdomStubs*)clientData;

    if (!actualVersion) {
        return NULL;
    }
    if (!tdomStubsPtr) {
        Tcl_SetResult(interp, "This implementation of Tdom does not "
                      "support stubs", TCL_STATIC);
        return NULL;
    }
    
    return actualVersion;
#endif
}
