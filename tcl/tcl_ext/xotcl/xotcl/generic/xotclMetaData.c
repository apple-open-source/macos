/* -*- Mode: c++ -*-
 *  $Id: xotclMetaData.c,v 1.5 2006/09/27 08:12:40 neumann Exp $
 *  
 *  Extended Object Tcl (XOTcl)
 *
 *  Copyright (C) 1999-2008 Gustaf Neumann, Uwe Zdun
 *
 *
 *  xotclReference.c --
 *  
 *  XOTcl Object References
 *  
 */

#include "xotclInt.h"

#ifdef XOTCL_METADATA
/*
 * Meta Data
 */

void
XOTclMetaDataRemoveDepending(XOTclClass *cl, char *name) {
  XOTclClasses *saved = cl->order, *clPtr;
  cl->order = 0;

  clPtr = XOTclComputeDependents(cl);

  while (clPtr != 0) {
      Tcl_HashSearch hSrch;
      Tcl_HashEntry *hPtr = &clPtr->cl->instances ?
	Tcl_FirstHashEntry(&clPtr->cl->instances, &hSrch) : 0;
      for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
	  XOTclObject *obj = (XOTclObject*)
	    Tcl_GetHashKey(&clPtr->cl->instances, hPtr);
	  Tcl_HashEntry *h1Ptr = 0;
	  if (obj->opt)
	    h1Ptr = Tcl_FindHashEntry(&obj->opt->metaData, name);
	  if (h1Ptr) {
	      Tcl_DeleteHashEntry(h1Ptr);
	  }
      }
      clPtr = clPtr->next;
  }

  XOTclRemoveClasses(cl->order); 
  cl->order = saved;
}

int
XOTclMetaDataInheritance (XOTclObject *obj, char *name) {
  XOTclClasses *clPtr;
  assert(obj);
  
  if (!obj->cl->order)
    obj->cl->order = XOTclComputePrecedence(obj->cl);
  clPtr = obj->cl->order;
  if (clPtr) {
    while (clPtr != 0) {
      if (clPtr->cl->object.opt) {
	if (Tcl_FindHashEntry(&clPtr->cl->object.opt->metaData, name)) {
	  return 1;
	}
      }
      clPtr = clPtr->next;
    }
  }
  return 0;
}

void
XOTclMetaDataDestroy(XOTclObject *obj) {
  if (obj->opt)
    Tcl_DeleteHashTable(&obj->opt->metaData);
}

void
XOTclMetaDataInit(XOTclObject *obj) {
  XOTclRequireObjectOpt(obj);
  Tcl_InitHashTable(&obj->opt->metaData, TCL_STRING_KEYS);
}

int
XOTclOMetaDataMethod (ClientData cd, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTclClass  *cl  = XOTclObjectToClass(cd);
  char *option; int add = -1;
  int result = TCL_OK;
  int oc; Tcl_Obj* *ov; int i;

  if (!obj) return XOTclObjErrType(interp, obj->cmdName, "Object");
  if (objc < 2)
    return XOTclObjErrArgCnt(interp,obj->cmdName,
				  "metadata ?(add|remove)? metaDataList");

  option = ObjStr(objv[1]);
  switch (*option) {
  case 'a':
    if (strcmp(option,"add") == 0) add = 1; break;
  case 'r':
    if (strcmp(option,"remove") == 0) add = 0; break;
  }
  if (add == -1) {
    if (objc == 2) {
      if (obj->opt) {
	Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&obj->opt->metaData, option);
	if (hPtr) {
	  Tcl_Obj *entry = (Tcl_Obj *) Tcl_GetHashValue(hPtr);
	  if (entry) {
	    Tcl_SetObjResult(interp, (Tcl_Obj *) Tcl_GetHashValue(hPtr));
	  } else {
	    Tcl_ResetResult(interp);
	  }
	}
	return TCL_OK;	
      }
    }
    if (objc == 3) {
      if (obj->opt) {
	Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&obj->opt->metaData, option);
	if (!hPtr) {
	  int nw;
	  if (XOTclMetaDataInheritance(obj, option)) {
	    hPtr = Tcl_CreateHashEntry(&obj->opt->metaData, option, &nw);
	    if (!nw)
	      return XOTclVarErrMsg(interp,
				    "MetaData: Can't create MetaData Entry: ",
				    option, (char*) NULL);
	  }
	}	
	if (hPtr) {
	  Tcl_Obj *entry = (Tcl_Obj *) Tcl_GetHashValue(hPtr);
	  if (entry)
	    DECR_REF_COUNT(entry);
	  INCR_REF_COUNT(objv[2]);
	  Tcl_SetHashValue(hPtr, (ClientData) objv[2]);	
	  return TCL_OK;
	}
      }
    }
    return XOTclVarErrMsg(interp,"MetaData: Unknown option;  given Option: ",
			  option, (char*) NULL);
  }

  if (Tcl_ListObjGetElements(interp, objv[2], &oc, &ov) == TCL_OK) {
    for (i = 0; i < oc; i ++) {
      char *value = ObjStr (ov[i]);
      if (obj->opt) {
	if (add) {	
	  int nw;
	  Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&obj->opt->metaData, value);
	  if (hPtr)
	    return XOTclVarErrMsg(interp,
				  "Can't add MetaData, MetaData exists: ",
				  value,
				  (char*) NULL);	
	  hPtr = Tcl_CreateHashEntry(&obj->opt->metaData, value, &nw);
	  if (!nw)
	    return XOTclVarErrMsg(interp,
				  "MetaData: Can't create MetaData Entry: ",
				  value,(char*) NULL);
	} else {
	  Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&obj->opt->metaData, value);
	  if (hPtr) {		
	    Tcl_DeleteHashEntry(hPtr);
	    if (cl) {
	      XOTclMetaDataRemoveDepending(cl, value);
	    }
	  }
	}
      }
    }
  }
  return result;
}
#endif
