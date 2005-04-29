/* -*- Mode: c++ -*-
 * $Id: s.xotclUtil.c 1.5 01/10/11 18:37:46+02:00 neumann@mohegan.wu-wien.ac.at $
 *  
 *  Extended Object Tcl (XOTcl)
 *
 *  Copyright (C) 1999-2004 Gustaf Neumann, Uwe Zdun
 *
 *
 *  xotclShadow.c --
 *  
 *  Shadowing (overloading) and accessing global tcl obj commands
 *  
 */

#include "xotclInt.h"
#include "xotclAccessInt.h"

static int
XOTclReplaceCommandCleanup(Tcl_Interp *in, XOTclGlobalNames name) {
  Tcl_Command cmd;
  int result = TCL_OK;
  XOTclShadowTclCommandInfo *ti = &RUNTIME_STATE(in)->tclCommands[name-EXPR];

  /*fprintf(stderr," cleanup for %s  ti=%p in %p\n", XOTclGlobalStrings[name], ti, in);*/
  cmd = Tcl_GetCommandFromObj(in, XOTclGlobalObjects[name]);
  if (cmd != NULL) {
    Tcl_Command_objProc(cmd) = ti->proc;
    ti->proc = NULL;
  } else {
    result = TCL_ERROR;
  }
  
  return result;
}

static void
XOTclReplaceCommandCheck(Tcl_Interp *in, XOTclGlobalNames name, Tcl_ObjCmdProc *proc) {
  Tcl_Command cmd;
  XOTclShadowTclCommandInfo *ti = &RUNTIME_STATE(in)->tclCommands[name-EXPR];
  cmd = Tcl_GetCommandFromObj(in, XOTclGlobalObjects[name]);
  
  if (cmd != NULL && ti->proc && Tcl_Command_objProc(cmd) != proc) {
    /*
    fprintf(stderr, "we have to do something about %s %p %p\n",
	    XOTclGlobalStrings[name], Tcl_Command_objProc(cmd), proc);
    */
    ti->proc = Tcl_Command_objProc(cmd);
    ti->cd   = Tcl_Command_objClientData(cmd);
    Tcl_Command_objProc(cmd) = proc;
  }
}

static int
XOTclReplaceCommand(Tcl_Interp *in, XOTclGlobalNames name,
		    Tcl_ObjCmdProc *xotclReplacementProc, int pass) {
  Tcl_Command cmd;
  XOTclShadowTclCommandInfo *ti = &RUNTIME_STATE(in)->tclCommands[name-EXPR];
  int result = TCL_OK;

  /*fprintf(stderr,"XOTclReplaceCommand %d\n",name);*/
  cmd = Tcl_GetCommandFromObj(in, XOTclGlobalObjects[name]);
  
  if (cmd == NULL) {
    result = TCL_ERROR;
  } else {
    Tcl_ObjCmdProc *objProc = Tcl_Command_objProc(cmd);
    if (xotclReplacementProc != objProc) {
      if (pass == 0) { /* setting values on first pass (must be locked here) */
	ti->proc = objProc;
	ti->cd = Tcl_Command_objClientData(cmd);
      } else if (ti->proc != objProc) {
	/*fprintf(stderr, "we have to refetch command for %s\n",XOTclGlobalStrings[name]);*/
	ti->proc = objProc;
	ti->cd = Tcl_Command_objClientData(cmd);
      }
      if (xotclReplacementProc) {
	Tcl_Command_objProc(cmd) = xotclReplacementProc;
	/*Tcl_CreateObjCommand(in, XOTclGlobalStrings[name], xotclReplacementProc, 0, 0);*/
      }
    }
  }
  return result;
}

static int
XOTcl_RenameObjCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  /* this call the Tcl_RenameObjCmd, but it ensures before that
     the renamed obj, functions, etc. are not part of XOTcl */
  XOTclObject *obj = NULL;
  XOTclClass *cl = NULL;
  Tcl_Command cmd;

  /* wrong # args => normal Tcl ErrMsg*/
  if (objc != 3)
    return XOTclCallCommand(in, RENAME, objc, objv);

  /* if an obj/cl should be renamed => call the XOTcl move method */
  cmd = Tcl_FindCommand(in, ObjStr(objv[1]), (Tcl_Namespace *)NULL,0);

  if (cmd != NULL) {
    obj = XOTclGetObjectFromCmdPtr(cmd);
    if (obj) {
      return XOTclCallMethodWithArg((ClientData)obj, in,
			       XOTclGlobalObjects[MOVE], objv[2], 3, 0, 0);
    }
  }

  /* Actually rename the cmd using Tcl's rename*/
  return XOTclCallCommand(in, RENAME, objc, objv);
}

static int
XOTcl_InfoObjCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  int result, isBody = 0;

  if (objc > 1) {
    char *opt = ObjStr(objv[1]);
    if (isBodyString(opt) && objc > 2)
      isBody = 1;
  }
  result = XOTclCallCommand(in, INFO, objc, objv);

  if (isBody && result == TCL_OK) {
    char *body = ObjStr(Tcl_GetObjResult(in));
    if (strncmp(body, "::xotcl::initProcNS\n",20) == 0)
      body+=20;
    Tcl_SetObjResult(in, Tcl_NewStringObj(body, -1));
  }
  return result;
}


/*
 * Obtain the names of the tcl commands
 * not available through the stub interface and overload some global commands
 */
int
XOTclShadowTclCommands(Tcl_Interp *in, XOTclShadowOperations load) {
  int rc = TCL_OK;
  if (load == SHADOW_LOAD) {
    int initialized = (int) RUNTIME_STATE(in)->tclCommands;
    assert(initialized == 0);
    RUNTIME_STATE(in)->tclCommands = 
      NEW_ARRAY(XOTclShadowTclCommandInfo, SUBST-EXPR+1);

    /*fprintf(stderr, "+++ load tcl commands %d %d\n", load, initialized);*/

#ifdef USE_TCL_STUBS
    /* no commands are overloeaded, these are only used for calling 
       e.g. Tcl_ExprObjCmd(), Tcl_IncrObjCmd() and Tcl_SubstObjCmd(), 
       which are not avalailable in though the stub table */
    rc|= XOTclReplaceCommand(in, EXPR,     0, initialized);
    rc|= XOTclReplaceCommand(in, INCR,     0, initialized);
    rc|= XOTclReplaceCommand(in, SUBST,    0, initialized);
#endif
    /* for the following commands, we have to add our own semantics */
    rc|= XOTclReplaceCommand(in, INFO,     XOTcl_InfoObjCmd, initialized);
    rc|= XOTclReplaceCommand(in, RENAME,   XOTcl_RenameObjCmd, initialized);
    
  } else if (load == SHADOW_REFETCH) {
    XOTclReplaceCommandCheck(in, INFO,     XOTcl_InfoObjCmd);
    XOTclReplaceCommandCheck(in, RENAME,   XOTcl_RenameObjCmd);
  } else {
    XOTclReplaceCommandCleanup(in, INFO);
    XOTclReplaceCommandCleanup(in, RENAME);
    FREE(XOTclShadowTclCommandInfo*, RUNTIME_STATE(in)->tclCommands);
    RUNTIME_STATE(in)->tclCommands = NULL;
  }
  return rc;
}

/*
 * call a Tcl command with given objv's ... replace objv[0]
 * with the given command name
 */
int XOTclCallCommand(Tcl_Interp *in, XOTclGlobalNames name,
	    int objc, Tcl_Obj *CONST objv[]) {
  int result;
  XOTclShadowTclCommandInfo *ti = &RUNTIME_STATE(in)->tclCommands[name-EXPR];
  DEFINE_NEW_TCL_OBJS_ON_STACK(objc, ov);

  /* {int i;
    fprintf(stderr,"calling %s (%p %p) in %p, objc=%d ",
	    XOTclGlobalStrings[name],ti,ti->proc, in, objc);
    for(i=0;i<objc;i++){fprintf(stderr, "'%s' ", ObjStr(objv[i]));}
    fprintf(stderr,"\n");
  } */
  
  ov[0] = XOTclGlobalObjects[name];
  if (objc > 1)
    memcpy(ov+1, objv+1, sizeof(Tcl_Obj *)*(objc-1));
  result = (*ti->proc)(ti->cd, in, objc, ov);
  FREE_TCL_OBJS_ON_STACK(ov);
  return result;
}
