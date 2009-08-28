/* -*- Mode: c++ -*-
 *  xotclObjectData.c 
 *  
 *  Extended Object Tcl (XOTcl)
 *
 *  Copyright (C) 1999-2008 Gustaf Neumann, Uwe Zdun
 *
 *
 *  xotclObjectData.c --
 *  
 *  XOTcl Object Data, needs XOTCL_OBJECTDATA to be compiled in
 *  
 */

#include "xotclInt.h"

#ifdef XOTCL_OBJECTDATA
extern void
XOTclFreeObjectData(XOTclClass* cl) {
  if (cl->opt && cl->opt->objectdata) {
    Tcl_DeleteHashTable(cl->opt->objectdata);
    ckfree((char*)cl->opt->objectdata);
    cl->opt->objectdata = 0; 
  }
}
extern void
XOTclSetObjectData(XOTclObject* obj, XOTclClass* cl, ClientData data) {
  Tcl_HashEntry *hPtr;
  int nw;

  XOTclRequireClassOpt(cl);

  if (!cl->opt->objectdata) {
    cl->opt->objectdata = (Tcl_HashTable*)ckalloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(cl->opt->objectdata, TCL_ONE_WORD_KEYS);
  }
  hPtr = Tcl_CreateHashEntry(cl->opt->objectdata, (char*)obj, &nw);
  Tcl_SetHashValue(hPtr, data);
}

extern int
XOTclGetObjectData(XOTclObject* obj, XOTclClass* cl, ClientData* data) {
  Tcl_HashEntry *hPtr;
  if (!cl->opt || !cl->opt->objectdata) 
    return 0;
  hPtr = Tcl_FindHashEntry(cl->opt->objectdata, (char*)obj);
  if (data) *data = hPtr ? Tcl_GetHashValue(hPtr) : 0;
  return hPtr != 0;
}

extern int
XOTclUnsetObjectData(XOTclObject* obj, XOTclClass* cl) {
  Tcl_HashEntry *hPtr;

  if (!cl->opt || !cl->opt->objectdata) 
    return 0;
  hPtr = Tcl_FindHashEntry(cl->opt->objectdata, (char*)obj);
  if (hPtr) Tcl_DeleteHashEntry(hPtr);
  return hPtr != 0;
}
#endif
