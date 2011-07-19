# xotcl.decls --
#
#	This file contains the declarations for all supported public
#	functions that are exported by the XOTcl library via the stubs table.
#	This file is used to generate the xotclDecls.h, xotclPlatDecls.h,
#	xotclStub.c, and xotclPlatStub.c files.
#	
#
# Copyright (c) 1998-1999 by Scriptics Corporation.
# See the file "tcl-license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: $Id: xotcl.decls,v 1.4 2006/09/27 08:12:40 neumann Exp $

library xotcl

interface xotcl
hooks {xotclInt}

# Declare each of the functions in the public Tcl interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

declare 0 generic {
    int Xotcl_Init(Tcl_Interp *interp)
}
# 1 is reserved for safe init
#declare 1 generic {
#    int Xotcl_SafeInit(Tcl_Interp *interp)
#}
declare 2 generic {
    struct XOTcl_Class* XOTclIsClass(Tcl_Interp* in, ClientData cd)
}
#declare 3 generic {
#
#}
declare 4 generic {
    struct XOTcl_Object* XOTclGetObject(Tcl_Interp* in, char* name)
}
declare 5 generic {
    struct XOTcl_Class* XOTclGetClass(Tcl_Interp* in, char* name)
}
declare 6 generic {
    int XOTclCreateObject(Tcl_Interp* in, Tcl_Obj* name, struct XOTcl_Class* cl)
}
#declare 7 generic {
#
#}
declare 8 generic {
    int XOTclCreateClass(Tcl_Interp* in, Tcl_Obj* name, struct XOTcl_Class* cl)
}
declare 9 generic {
    int XOTclDeleteObject(Tcl_Interp* in, struct XOTcl_Object* obj)
}
declare 10 generic {
    int XOTclDeleteClass(Tcl_Interp* in, struct XOTcl_Class* cl)
}
declare 11 generic {
    void XOTclAddPMethod(Tcl_Interp* in, struct XOTcl_Object* obj, 
                         CONST char* nm, Tcl_ObjCmdProc* proc,
	                 ClientData cd, Tcl_CmdDeleteProc* dp)
}
declare 12 generic {
    void XOTclAddIMethod(Tcl_Interp* in, struct XOTcl_Class* cl, 
                         CONST char* nm, Tcl_ObjCmdProc* proc,
	                 ClientData cd, Tcl_CmdDeleteProc* dp)
}
declare 13 generic {
    void XOTclRemovePMethod(Tcl_Interp* in,struct XOTcl_Object* obj, char* nm)
}
declare 14 generic {
    void XOTclRemoveIMethod(Tcl_Interp* in, struct XOTcl_Class* cl, char* nm)
}
declare 15 generic {
    Tcl_Obj* XOTclOSetInstVar(struct XOTcl_Object* obj, Tcl_Interp* in,
	       Tcl_Obj* name, Tcl_Obj* value, int flgs)
}
declare 16 generic {
    Tcl_Obj* XOTclOGetInstVar(struct XOTcl_Object* obj, Tcl_Interp* in,
	       Tcl_Obj* name, int flgs)
}
declare 17 generic {
    int	XOTclInstVar(struct XOTcl_Object* obj, Tcl_Interp* in,
	     char* name, char* destName)
}
#declare 18 generic {
#
#}
declare 19 generic {
    Tcl_Obj* XOTclOSetInstVar2(struct XOTcl_Object* obj,
                 Tcl_Interp* in,Tcl_Obj* name1,Tcl_Obj* name2,
		 Tcl_Obj* value,int flgs)
}
declare 20 generic {
    Tcl_Obj* XOTclOGetInstVar2(struct XOTcl_Object* obj, 
                 Tcl_Interp* in,Tcl_Obj* name1,Tcl_Obj* name2,
		 int flgs)
}
declare 21 generic {
    int XOTclUnsetInstVar2(struct XOTcl_Object* obj, Tcl_Interp* in, 
                           char* name1, char* name2, 
		           int flgs)
}
declare 22 generic {
    int XOTcl_TraceObjCmd(ClientData cd, Tcl_Interp *in,
                          int objc, Tcl_Obj *CONST objv[])
}
declare 23 generic {
    int XOTclErrMsg(Tcl_Interp* in, char* msg, Tcl_FreeProc* type)
}
declare 24 generic {
    int XOTclVarErrMsg(Tcl_Interp * in, ...)
}
declare 25 generic {
    int XOTclErrInProc (Tcl_Interp *in, Tcl_Obj* objName,
		Tcl_Obj* clName, char* procName)
}
declare 26 generic {
    int XOTclObjErrArgCnt(Tcl_Interp *in, Tcl_Obj *cmdname, char *arglist)
}
declare 27 generic {
   int XOTclErrBadVal_(Tcl_Interp *in, char *expected, char *value)
}
declare 28 generic {
    int XOTclObjErrType(Tcl_Interp *in, Tcl_Obj *nm, char* wt)
}
declare 29 generic {
    void XOTclStackDump (Tcl_Interp* in)
}
declare 30 generic {
    void XOTclCallStackDump (Tcl_Interp* in)
}
declare 31 generic {
    void XOTclDeprecatedMsg(char* oldCmd, char* newCmd)
}
declare 32 generic {
    void XOTclSetObjClientData(XOTcl_Object* obj, ClientData data)
}
declare 33 generic {
    ClientData XOTclGetObjClientData(XOTcl_Object* obj)
}
declare 34 generic {
    void XOTclSetClassClientData(XOTcl_Class* cl, ClientData data)
}
declare 35 generic {
    ClientData XOTclGetClassClientData(XOTcl_Class* cl)
}
declare 36 generic {
    void XOTclRequireObjNamespace(Tcl_Interp* in, XOTcl_Object* obj)
}
declare 37 generic {
    int XOTclErrBadVal(Tcl_Interp *in, char *context, char *expected, char *value)
}
declare 38 generic {
        int XOTclNextObjCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[])
}
declare 39 generic {
        int  XOTclCallMethodWithArgs(ClientData cd, Tcl_Interp *in, 
                Tcl_Obj *method, Tcl_Obj *arg,
       	         int objc, Tcl_Obj *CONST objv[], int flags)
}

