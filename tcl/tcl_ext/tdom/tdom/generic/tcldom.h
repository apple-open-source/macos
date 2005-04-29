/*----------------------------------------------------------------------------
|   Copyright (c) 1999 Jochen Loewer (loewerj@hotmail.com)
+-----------------------------------------------------------------------------
|
|   $Header: /usr/local/pubcvs/tdom/generic/tcldom.h,v 1.7 2003/10/17 08:45:38 zoran Exp $
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


#ifndef __TCLDOM_H__
#define __TCLDOM_H__  

#include <tcl.h>

void tcldom_initialize(void);

Tcl_ObjCmdProc tcldom_DomObjCmd;
Tcl_ObjCmdProc tcldom_DocObjCmd;
Tcl_ObjCmdProc tcldom_NodeObjCmd;
Tcl_ObjCmdProc TclExpatObjCmd;
Tcl_ObjCmdProc tcldom_unknownCmd;
Tcl_ObjCmdProc TclTdomObjCmd;

#if defined(_MSC_VER)
#  undef TCL_STORAGE_CLASS
#  define TCL_STORAGE_CLASS DLLEXPORT
#endif

#define STR_TDOM_VERSION(v) (VERSION)

EXTERN int Tdom_Init     _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Tdom_SafeInit _ANSI_ARGS_((Tcl_Interp *interp));

#endif


