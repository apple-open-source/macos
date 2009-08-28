# xotclInt.decls --
#
#	This file contains the declarations for all unsupported
#	functions that are exported by the Tcl library.  This file
#	is used to generate the itclIntDecls.h and itclIntStub.c
#	files
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# See the file "tcl-license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: xotclInt.decls,v 1.1 2004/05/23 22:50:39 neumann Exp $

library xotcl

# Define the unsupported generic interfaces.

interface xotclInt


#
# Functions used within the package, but not considered "public"
#

#declare 0 generic {

#}
#declare 1 generic {

#}
# declare 2 generic {
#     int XOTclErrInProc (Tcl_Interp *in, Tcl_Obj* objName,
# 		Tcl_Obj* clName, char* procName)
# }
# declare 3 generic {
#     int XOTclObjErrArgCnt(Tcl_Interp *in, Tcl_Obj *cmdname, char *arglist)
# }
# declare 4 generic {
#     int XOTclErrBadVal(Tcl_Interp *in, char *expected, char *value)
# }
# declare 5 generic {
#     int XOTclObjErrType(Tcl_Interp *in, Tcl_Obj *nm, char* wt)
# }
# declare 6 generic {
#     void XOTclStackTrace (Tcl_Interp* in)
# }
# declare 7 generic {
#     void XOTclCallStackTrace (Tcl_Interp* in)
# }
#declare 8 generic {
#    void XOTclFilterTrace (Tcl_Interp* in)
#}
#declare 9 generic {
#    int XOTclIsType(XOTclObject* obj, XOTclClass* type)
#}
#declare 10 generic {
#    void XOTclRemoveClasses(XOTclClasses* sl)
#}
# declare 11 generic {
#     XOTclClasses** XOTclAddClass(XOTclClasses** cList, XOTclClass* cl, ClientData cd)
# }
# declare 12 generic {
#     XOTclClasses* XOTclComputePrecedence(register XOTclClass* cl)
# }
# declare 13 generic {
#     XOTclClasses* XOTclComputeDependents(register XOTclClass* cl)
# }
# declare 14 generic {
#     void XOTclDeprecatedMsg(char* oldCmd, char* newCmd)
# }
# declare 15 generic {
#     void XOTclSetObjClientData(XOTclObject* obj, ClientData data)
# }
# declare 16 generic {
#     ClientData XOTclGetObjClientData(XOTclObject* obj)
# }
# declare 17 generic {
#     void XOTclSetClassClientData(XOTclClass* cl, ClientData data)
# }
# declare 18 generic {
#     ClientData XOTclGetClassClientData(XOTclClass* cl)
# }
# declare 19 generic {
#     void XOTclRequireObjectOpt(XOTclObject* obj)
# }
# declare 20 generic {
#     void XOTclRequireClassOpt(XOTclClass* cl)
# }
# declare 21 generic {
#     void XOTclRequireObjNamespace(Tcl_Interp* in, XOTclObject* obj)
# }
# declare 22 generic {
    
# }
# declare 23 generic {
    
# }
# declare 24 generic {
    
# }
# declare 25 generic {
    
# }
# declare 26 generic {
    
# }
# declare 27 generic {
    
# }
# declare 28 generic {
    
# }
# declare 29 generic {
    
# }
# declare 30 generic {
    
# }
