/* -*- Mode: c++ -*-
 * $Id: s.xotclTrace.c 1.5 01/10/11 18:37:46+02:00 neumann@mohegan.wu-wien.ac.at $
 *  
 *  Extended Object Tcl (XOTcl)
 *
 *  Copyright (C) 1999-2000 Gustaf Neumann, Uwe Zdun
 *
 *
 *  xotclTrace.c --
 *  
 *  Tracing facilities for XOTcl
 *  
 */

#include "xotclInt.h"
#include "xotclAccessInt.h"

void
XOTclStackTrace(Tcl_Interp *in) {
  Interp *iPtr = (Interp *) in;
  CallFrame *f = iPtr->framePtr, *v = iPtr->varFramePtr;
  Tcl_Obj *varCmdObj;

  XOTclNewObj(varCmdObj);
  fprintf (stderr, "     TCL STACK: ");
  if (f == 0) fprintf(stderr, "- ");
  while (f) {
    Tcl_Obj *cmdObj;
    XOTclNewObj(cmdObj);
    if (f && f->isProcCallFrame && f->procPtr && f->procPtr->cmdPtr) {
      Tcl_GetCommandFullName(in, (Tcl_Command)  f->procPtr->cmdPtr, cmdObj);
      if (cmdObj) {
	fprintf(stderr, " %s (%d)", ObjStr(cmdObj), f->level);
      }
      DECR_REF_COUNT(cmdObj);
    } else fprintf(stderr, "- ");

    f = f->callerPtr;
    if (f) fprintf(stderr, ",");
  }

  fprintf (stderr, " VARFRAME: ");
  if (v && v->isProcCallFrame && v->procPtr && v->procPtr->cmdPtr) {
    Tcl_GetCommandFullName(in, (Tcl_Command)  v->procPtr->cmdPtr, varCmdObj);
    if (varCmdObj) {
      fprintf(stderr, " %s (%d)\n", ObjStr(varCmdObj), v->level);
    }
  } else fprintf(stderr, "- \n");
  DECR_REF_COUNT(varCmdObj);
}

void
XOTclCallStackTrace(Tcl_Interp *in) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  XOTclCallStackContent *csc;
  int i=1, entries = cs->top - cs->content;

  fprintf (stderr, "     XOTCL CALLSTACK: (%d entries, top: %p) \n", entries, cs->top);
  for (csc = &cs->content[1]; csc <= cs->top; csc++) {
    fprintf(stderr, "       %d: %p ",i++,csc);
    if (csc->self)
      fprintf(stderr, "OBJ %s, ", ObjStr(csc->self->cmdName));
    if (csc->cl)
      fprintf(stderr, "INSTPROC %s->", className(csc->cl));
    else
      fprintf(stderr, "PROC ");

    /*fprintf(stderr, " cmd %p, obj %p, ",csc->cmdPtr, csc->self);*/

    if (csc->cmdPtr && !csc->destroyedCmd)
      fprintf(stderr, "%s, ", Tcl_GetCommandName(in, (Tcl_Command)csc->cmdPtr));
    else 
      fprintf(stderr, "NULL, ");

    fprintf(stderr, "frameType: %d, ", csc->frameType);
    fprintf(stderr, "next: %d ", csc->callsNext);
    fprintf(stderr, "cframe %p ", csc->currentFramePtr);

    if (csc->currentFramePtr) 
      fprintf(stderr,"l=%d ",Tcl_CallFrame_level(csc->currentFramePtr));

    if (csc->destroyedCmd)
      fprintf(stderr, "--destroyed cmd set (%p) ", csc->destroyedCmd);

    fprintf(stderr, "\n");
  }
  /*
  if (entries > 0) {
    XOTclCallStackContent *c;
    c = XOTclCallStackFindLastInvocation(in);
    fprintf(stderr,"     --- findLastInvocation %p ",c);
    if (c) {
      if (c <= cs->top && c->currentFramePtr) 
	fprintf(stderr," l=%d", Tcl_CallFrame_level(c->currentFramePtr));
    }
    c = XOTclCallStackFindActiveFrame(in, 1);
    fprintf(stderr,"     findActiveFrame    %p ",c);
    if (c) {
      if (c <= cs->top && c->currentFramePtr) 
	fprintf(stderr," l=%d", Tcl_CallFrame_level(c->currentFramePtr));
    }
    fprintf(stderr," --- \n");
  }
  */
}

/* helper function to print the vars dynamically created on a
  callframe
static void printLocalTable (CallFrame* c) {
  Tcl_HashEntry *entryPtr;
  Tcl_HashTable *localVarTablePtr = c->varTablePtr;
  Tcl_HashSearch search;

  fprintf(stderr, "LocalVars:");

  if (localVarTablePtr != NULL) {
    for (entryPtr = Tcl_FirstHashEntry(localVarTablePtr, &search);
	 entryPtr != NULL;
	 entryPtr = Tcl_NextHashEntry(&search)) {
      char *varName = Tcl_GetHashKey(localVarTablePtr, entryPtr);
      fprintf(stderr, " %s,", varName);
    }
  }
  fprintf(stderr,"\n");
}
*/

int
XOTcl_TraceObjCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  char *option;
  if (objc != 2)
    return XOTclObjErrArgCnt(in, NULL, "::xotcl::trace");

  option = ObjStr(objv[1]);
  if (strcmp(option,"stack") == 0) {
    XOTclStackTrace(in);
    return TCL_OK;
  }
  if (strcmp(option,"callstack") == 0) {
    XOTclCallStackTrace(in);
    return TCL_OK;
  }
  return XOTclVarErrMsg(in, "xotcltrace: unknown option", (char*) NULL);
}

#ifdef XOTCL_MEM_COUNT
void 
XOTclMemCountAlloc(char *id, void *p) {
  int new;
  XOTclMemCounter *entry;
  Tcl_HashTable *table = &xotclMemCount;
  Tcl_HashEntry *hPtr;
  hPtr = Tcl_CreateHashEntry(table, id, &new);
#ifdef XOTCL_MEM_TRACE
  fprintf(stderr, "+++ alloc %s %p\n",id,p);
#endif
  /*fprintf(stderr,"+++alloc '%s'\n",id);*/
  if (new) {
    entry = (XOTclMemCounter*)ckalloc(sizeof(XOTclMemCounter));
    entry->count = 1;
    entry->peak = 1;
    Tcl_SetHashValue(hPtr, entry);
  } else {
    entry = (XOTclMemCounter*) Tcl_GetHashValue(hPtr);
    entry->count++;
    if (entry->count > entry->peak)
      entry->peak = entry->count;
  }
}

void
XOTclMemCountFree(char *id, void *p) {
  XOTclMemCounter *entry;
  Tcl_HashTable *table = &xotclMemCount;
  Tcl_HashEntry *hPtr;
#ifdef XOTCL_MEM_TRACE
  fprintf(stderr, "+++ free %s %p\n",id,p);
#endif

  hPtr = Tcl_FindHashEntry(table, id);
  if (!hPtr) {
    fprintf(stderr, "******** MEM COUNT ALERT: Trying to free <%s>, but was not allocated\n", id);
    return;
  }
  entry = (XOTclMemCounter*)  Tcl_GetHashValue(hPtr);
  entry->count--;
}

void
XOTclMemCountDump() {
  Tcl_HashTable *table = &xotclMemCount;
  Tcl_HashSearch search;
  Tcl_HashEntry *hPtr;
  int count = 0;

  xotclMemCountInterpCounter--;
  if (xotclMemCountInterpCounter != 0) {
    return;
  }

  fprintf(stderr, "******** XOTcl MEM Count *********\n*  count peak\n");

  for (hPtr = Tcl_FirstHashEntry(table, &search);  hPtr != NULL;
       hPtr = Tcl_NextHashEntry(&search)) {
    char *id = Tcl_GetHashKey(table, hPtr);
    XOTclMemCounter *entry = (XOTclMemCounter*)  Tcl_GetHashValue(hPtr);
    count += entry->count;
    fprintf(stderr, "* %4d %6d %s\n", entry->count, entry->peak, id);
    ckfree ((char*) entry);
  }
  
  Tcl_DeleteHashTable(table);
  
  fprintf(stderr, "******** Count Overall = %d\n", count);
}

#endif
