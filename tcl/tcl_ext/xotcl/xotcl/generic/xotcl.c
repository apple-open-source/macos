/*
 *
 *  XOTcl - Extended OTcl
 *
 *  Copyright (C) 1999-2004 Gustaf Neumann (a), Uwe Zdun (b)
 *
 * (a) Vienna University of Economics and Business Administration
 *     Dept. of Information Systems / New Media
 *     A-1090, Augasse 2-6
 *     Vienna, Austria
 *
 * (b) University of Essen
 *     Specification of Software Systems
 *     Altendorferstraﬂe 97-101
 *     D-45143 Essen, Germany
 *
 *  Permission to use, copy, modify, distribute, and sell this
 *  software and its documentation for any purpose is hereby granted
 *  without fee, provided that the above copyright notice appear in
 *  all copies and that both that copyright notice and this permission
 *  notice appear in supporting documentation. We make no
 *  representations about the suitability of this software for any
 *  purpose.  It is provided "as is" without express or implied
 *  warranty.
 *
 *
 *  This software is based upon MIT Object Tcl by David Wetherall and
 *  Christopher J. Lindblad, that contains the following copyright
 *  message:
 *
 *   "Copyright 1993 Massachusetts Institute of Technology
 *
 *    Permission to use, copy, modify, distribute, and sell this
 *    software and its documentation for any purpose is hereby granted
 *    without fee, provided that the above copyright notice appear in
 *    all copies and that both that copyright notice and this
 *    permission notice appear in supporting documentation, and that
 *    the name of M.I.T. not be used in advertising or publicity
 *    pertaining to distribution of the software without specific,
 *    written prior permission.  M.I.T. makes no representations about
 *    the suitability of this software for any purpose.  It is
 *    provided "as is" without express or implied warranty."
 * */

#define XOTCL_C 1
#include "xotclInt.h"
#include "xotclAccessInt.h"

#ifdef KEEP_TCL_CMD_TYPE
# include "tclCompile.h"
#endif

#ifdef COMPILE_XOTCL_STUBS
extern XotclStubs xotclStubs;
#endif

#ifdef XOTCL_MEM_COUNT
int xotclMemCountInterpCounter = 0;
#endif

/*
 * Tcl_Obj Types for XOTcl Objects
 */

#ifdef USE_TCL_STUBS
# define XOTcl_ExprObjCmd(cd,in,objc,objv) \
	XOTclCallCommand(in, EXPR, objc, objv)
# define XOTcl_IncrObjCmd(cd,in,objc,objv) \
	XOTclCallCommand(in, INCR, objc, objv)
# define XOTcl_SubstObjCmd(cd,in,objc,objv) \
	XOTclCallCommand(in, SUBST, objc, objv)
#else
# define XOTcl_ExprObjCmd(cd,in,objc,objv) \
	Tcl_ExprObjCmd(cd, in, objc, objv)
# define XOTcl_IncrObjCmd(cd,in,objc,objv) \
	Tcl_IncrObjCmd(cd, in, objc, objv)
# define XOTcl_SubstObjCmd(cd,in,objc,objv) \
	Tcl_SubstObjCmd(cd, in, objc, objv)
#endif


static int SetXOTclObjectFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void UpdateStringOfXOTclObject(Tcl_Obj *objPtr);
static void FreeXOTclObjectInternalRep(Tcl_Obj *objPtr);
static void DupXOTclObjectInternalRep(Tcl_Obj *src, Tcl_Obj *cpy);

XOTCLINLINE static void GuardAdd(Tcl_Interp *in, XOTclCmdList* filterCL, Tcl_Obj *guard);
static int GuardCheck(Tcl_Interp *in, ClientData guards);
static int GuardCall(XOTclObject *obj, XOTclClass* cl, Tcl_Command cmd, Tcl_Interp *in, ClientData clientData, int push);
static void GuardDel(XOTclCmdList* filterCL);

static Tcl_ObjType XOTclObjectType = {
  "XOTclObject",			
  FreeXOTclObjectInternalRep,
  DupXOTclObjectInternalRep,
  UpdateStringOfXOTclObject,	
  SetXOTclObjectFromAny		
};


typedef enum { CALLING_LEVEL, ACTIVE_LEVEL } CallStackLevel;

typedef struct callFrameContext {
  int framesSaved;
  Tcl_CallFrame *framePtr;
  Tcl_CallFrame *varFramePtr;
} callFrameContext;
typedef struct tclCmdClientData {
  XOTcl_Object *obj;
  Tcl_Obj *cmdName;
} tclCmdClientData;

static int ObjDispatch(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[],
		       int flags);
static int XOTclNextMethod(XOTclObject *obj, Tcl_Interp *in, XOTclClass *givenCl,
			   char *givenMethod, int objc, Tcl_Obj *CONST objv[],
			   int useCSObjs);
static int XOTclOEvalMethod(ClientData cd, Tcl_Interp *in,
			    int objc, Tcl_Obj * CONST objv[]);

static XOTclObject *GetObject(Tcl_Interp *in, char *name);
static XOTclClass *GetClass(Tcl_Interp *in, char *name);

#if !defined(NDEBUG)
static void checkAllInstances(Tcl_Interp *in, XOTclClass *startCl, int lvl);
#endif

static XOTclCallStackContent* CallStackGetFrame(Tcl_Interp *in);



#ifdef PRE81
/* for backward compatibility only
*/
static int
Tcl_EvalObjv(Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[], int flags) {
  int i, result;
  Tcl_DString ds, *dsp = &ds;

  assert(flags == 0);
  DSTRING_INIT(dsp);
  for (i = 0; i < objc; i++) {
    Tcl_DStringAppendElement(dsp, ObjStr(objv[i]));
  }
  result = Tcl_Eval(in, Tcl_DStringValue(dsp));
  DSTRING_FREE(dsp);
  return result;
}
static int
Tcl_IncrObjCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  int result;
  char *argv[3];
  argv[0] = XOTclGlobalStrings[INCR];
  argv[1] = ObjStr(objv[1]);
  if (objc == 3)
    argv[2] = ObjStr(objv[2]);
  result = Tcl_IncrCmd(cd, in, objc, argv);
  return result;
}
static int
Tcl_SubstObjCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  char *ov[20];
  int i;
  assert(objc<19);
  for (i=0; i<objc; i++)
    ov[i] = ObjStr(objv[i]);

  return Tcl_SubstCmd(cd, in, objc, ov);
}
#endif


/*
 * call an XOTcl method
 */
static int
callMethod(ClientData cd, Tcl_Interp *in, Tcl_Obj *method,
	   int objc, Tcl_Obj *CONST objv[], int flags) {
  XOTclObject *obj = (XOTclObject*) cd;
  int result;
  DEFINE_NEW_TCL_OBJS_ON_STACK(objc, tov);

  tov[0] = obj->cmdName;
  tov[1] = method;
  if (objc>2)
    memcpy(tov+2, objv, sizeof(Tcl_Obj *)*(objc-2));

  result = ObjDispatch(cd, in, objc, tov, flags);
  /*fprintf(stderr, "     callMethod returns %d\n", result);*/
  FREE_TCL_OBJS_ON_STACK(tov);
  return result;
}

int
XOTclCallMethodWithArg(ClientData cd, Tcl_Interp *in, Tcl_Obj *method, Tcl_Obj *arg,
		  int objc, Tcl_Obj *CONST objv[], int flags) {
  XOTclObject *obj = (XOTclObject*) cd;
  int result;
  DEFINE_NEW_TCL_OBJS_ON_STACK(objc, tov);

  tov[0] = obj->cmdName;
  tov[1] = method;
  tov[2] = arg;
  if (objc>3)
    memcpy(tov+3, objv, sizeof(Tcl_Obj *)*(objc-3));

  result = ObjDispatch(cd, in, objc, tov, flags);

  FREE_TCL_OBJS_ON_STACK(tov);
  return result;
}

/*
 *  realize self, class, proc through the [self] command
 */

XOTCLINLINE static CONST84 char*
GetSelfProc(Tcl_Interp *in) {
  /*return Tcl_GetCommandName(in, RUNTIME_STATE(in)->cs.top->cmdPtr);*/
  return Tcl_GetCommandName(in, CallStackGetFrame(in)->cmdPtr);

}

XOTCLINLINE static XOTclClass*
GetSelfClass(Tcl_Interp *in) {
  /*return RUNTIME_STATE(in)->cs.top->cl;*/
  return CallStackGetFrame(in)->cl;
}

XOTCLINLINE static XOTclObject*
GetSelfObj(Tcl_Interp *in) {
  return CallStackGetFrame(in)->self;
}

XOTCLINLINE static Tcl_Command
GetSelfProcCmdPtr(Tcl_Interp *in) {
  /*return RUNTIME_STATE(in)->cs.top->cmdPtr;*/
  return CallStackGetFrame(in)->cmdPtr;
}

/*
 * prints a msg to the screen that oldCmd is deprecated
 * optinal: give a new cmd
 */
extern void
XOTclDeprecatedMsg(char *oldCmd, char *newCmd) {
  fprintf(stderr, "**\n**\n** The command/method <%s> is deprecated.\n", oldCmd);
  if (newCmd)
    fprintf(stderr, "** Use <%s> instead.\n", newCmd);
  fprintf(stderr, "**\n");
}

static int
XOTcl_DeprecatedCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  char *new;
  if (objc == 2)
    new = 0;
  else if (objc == 3)
    new = ObjStr(objv[2]);
  else
    return XOTclObjErrArgCnt(in, NULL, "deprecated oldcmd ?newcmd?");
  XOTclDeprecatedMsg(ObjStr(objv[1]), new);
  return TCL_OK;
}
#ifdef DISPATCH_TRACE
static void printObjv(int objc, Tcl_Obj *CONST objv[]) {
  int i, j;
  if (objc <= 3) j = objc; else j = 3;
  for (i=0;i<j;i++) fprintf(stderr, " %s", ObjStr(objv[i]));
  if (objc > 3) fprintf(stderr," ...");
  fprintf(stderr," (objc=%d)",objc);
}

static void printCall(Tcl_Interp *in, char *string, int objc, Tcl_Obj *CONST objv[]) {
  fprintf(stderr, "     (%d) >%s: ", Tcl_Interp_numLevels(in), string);
  printObjv(objc, objv);
  fprintf(stderr, "\n");
}
static void printExit(Tcl_Interp *in, char *string,
		      int objc, Tcl_Obj *CONST objv[], int result) {
  fprintf(stderr, "     (%d) <%s: ", Tcl_Interp_numLevels(in), string);
  /*printObjv(objc, objv);*/
  fprintf(stderr, " result=%d\n", result);
}
#endif


/*
 *  XOTclObject Reference Accounting
 */
#if defined(XOTCLOBJ_TRACE)
# define XOTclObjectRefCountIncr(obj) \
  obj->refCount++; \
  fprintf(stderr, "RefCountIncr %p count=%d %s\n", obj, obj->refCount,obj->cmdName?ObjStr(obj->cmdName):"no name"); \
  MEM_COUNT_ALLOC("XOTclObject RefCount",obj)
# define XOTclObjectRefCountDecr(obj) \
  obj->refCount--; \
  fprintf(stderr, "RefCountDecr %p count=%d\n", obj, obj->refCount); \
  MEM_COUNT_FREE("XOTclObject RefCount", obj)
#else
# define XOTclObjectRefCountIncr(obj) \
  obj->refCount++; \
  MEM_COUNT_ALLOC("XOTclObject RefCount",obj)
# define XOTclObjectRefCountDecr(obj) \
  obj->refCount--; \
  MEM_COUNT_FREE("XOTclObject RefCount",obj)
#endif

#if defined(XOTCLOBJ_TRACE)
void objTrace(char *string, XOTclObject *obj) {
  if(obj)
    fprintf(stderr,"--- %s tcl %p (%d) xotcl %p (%d) %s \n", string,
	    obj->cmdName, obj->cmdName->refCount, obj, obj->refCount, ObjStr(obj->cmdName));
  else
    fprintf(stderr,"--- No object: %s\n",string);
}
#else
# define objTrace(a,b)
#endif


/* search for tail of name */
static char*
NSTail(char *string) {
  register char *p;
  for (p=string+strlen(string); p>=string && *p != ':'; p--);
  return (p+1);
}

XOTCLINLINE static int
isClassName(char *string) {
  return (strncmp((string), "::xotcl::classes", 16) == 0);
}

/* removes preceding ::xotcl::classes from a string */
XOTCLINLINE static char*
NSCutXOTclClasses(char *string) {
  assert(strncmp((string), "::xotcl::classes", 16) == 0);
  return string+16;
}

XOTCLINLINE static char*
NSCmdFullName(Tcl_Command cmd) {
  Tcl_Namespace *nsPtr = Tcl_Command_nsPtr(cmd);
  return nsPtr ? nsPtr->fullName : "";
}

static void
XOTclCleanupObject(XOTclObject *obj) {
  XOTclObjectRefCountDecr(obj);
  if (obj->refCount <= 0) {
    assert(obj->refCount == 0);
    assert(obj->flags & XOTCL_DESTROYED);
#if REFCOUNTED
    fprintf(stderr,"###CLNO %p flags %d rc %d destr %d dc %d\n",
	    obj, obj->flags,
	    (obj->flags & XOTCL_REFCOUNTED) != 0,
	    (obj->flags & XOTCL_DESTROYED) != 0,
	    (obj->flags & XOTCL_DESTROY_CALLED) != 0
	    );
#endif

    MEM_COUNT_FREE("XOTclObject/XOTclClass",obj);
#if defined(XOTCLOBJ_TRACE)
    fprintf(stderr, "CKFREE Object %p refcount=%d\n", obj, obj->refCount);
#endif
#if !defined(NDEBUG)
    memset(obj, 0, sizeof(XOTclObject));
#endif
    ckfree((char *) obj);
  }
}



/*
 *  Tcl_Obj functions for objects
 */
static void
RegisterObjTypes() {
  Tcl_RegisterObjType(&XOTclObjectType);
}

static void
FreeXOTclObjectInternalRep(register Tcl_Obj *objPtr) {
  XOTclObject *obj = (XOTclObject*) objPtr->internalRep.otherValuePtr;
#if defined(XOTCLOBJ_TRACE)
  if (obj)
    fprintf(stderr,"FIP --- tcl %p (%d)\n",objPtr,objPtr->refCount);
#endif
  if (obj != NULL) {
#if REFCOUNTED
    fprintf(stderr,"FIP %p flags %d rc %d destr %d dc %d\n",
	    obj, obj->flags,
	    (obj->flags & XOTCL_REFCOUNTED) != 0,
	    (obj->flags & XOTCL_DESTROYED) != 0,
	    (obj->flags & XOTCL_DESTROY_CALLED) != 0
	    );
    if (obj->flags & XOTCL_REFCOUNTED &&
	!(obj->flags & XOTCL_DESTROYED)) {
      Tcl_Interp *in = obj->teardown;
      obj->teardown = NULL;
      fprintf(stderr, " ... teardown = %p\n", obj->teardown);
      /*obj->flags &= ~XOTCL_REFCOUNTED;*/
      callDestroyMethod((ClientData)obj, in, obj, 0);
      fprintf(stderr, " ... destroy method done \n");
    } else {
      fprintf(stderr, "BEFORE CLEANUPOBJ %d\n",
	      (obj->flags & XOTCL_REFCOUNTED));
#endif
      XOTclCleanupObject(obj);
#if REFCOUNTED
      fprintf(stderr, "AFTER CLEANUPOBJ\n");
    }
#endif
    objPtr->internalRep.otherValuePtr = NULL;
    objPtr->typePtr = NULL;
  }
}

static void
DupXOTclObjectInternalRep(Tcl_Obj *src, Tcl_Obj *cpy) {
  XOTclObject *obj = (XOTclObject*)src->internalRep.otherValuePtr;
#if defined(XOTCLOBJ_TRACE)
  if (obj) fprintf(stderr,"DIP --- tcl %p (%d)\n", src, src->refCount);
#endif
  XOTclObjectRefCountIncr(obj);
  cpy->internalRep.otherValuePtr = src->internalRep.otherValuePtr;
  cpy->typePtr = src->typePtr;
}

static int
SetXOTclObjectFromAny(Tcl_Interp *interp, register Tcl_Obj *objPtr) {
  Tcl_ObjType *oldTypePtr = objPtr->typePtr;
  char *string = ObjStr(objPtr);
  XOTclObject *obj;
  int result = TCL_OK;

#ifdef XOTCLOBJ_TRACE
  fprintf(stderr,"SetXOTclObjectFromAny %p '%s' %p\n",
	  objPtr,string,objPtr->typePtr);
  if (oldTypePtr)
    fprintf(stderr,"   convert %s to XOTclObject\n", oldTypePtr->name);
#endif

  obj = GetObject(interp, string);
  if (obj) {
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
#ifdef XOTCLOBJ_TRACE
      fprintf(stderr,"   freeing type=%p, xottyp=%p\n",
	      objPtr->typePtr, &XOTclObjectType);
#endif
      oldTypePtr->freeIntRepProc(objPtr);
    }
    XOTclObjectRefCountIncr(obj);
#if defined(XOTCLOBJ_TRACE)
    fprintf(stderr, "SetXOTclObjectFromAny tcl %p (%d) xotcl %p (%d)\n",
	    objPtr, objPtr->refCount, obj, obj->refCount);
#endif
    objPtr->internalRep.otherValuePtr = (XOTclObject*) obj;
    objPtr->typePtr = &XOTclObjectType;
  } else
    result = TCL_ERROR;

  return result;
}

static void
UpdateStringOfXOTclObject(register Tcl_Obj *objPtr) {
  XOTclObject *obj = (XOTclObject *)objPtr->internalRep.otherValuePtr;
  char *nsFullName = NULL;
  register Tcl_Command cmd;

#ifdef XOTCLOBJ_TRACE
  fprintf(stderr,"UpdateStringOfXOTclObject %p refCount %d\n",
	  objPtr,objPtr->refCount);
  fprintf(stderr,"    teardown %p id %p destroyCalled %d\n",
	  obj->teardown, obj->id, (obj->flags & XOTCL_DESTROY_CALLED));
#endif

  /* Here we use GetCommandName, because it doesnt need
     Interp*, but Tcl_GetCommandFullName(in,obj->id,ObjName); does*/
  if (obj && !(obj->flags & XOTCL_DESTROY_CALLED)) {
    Tcl_DString ds, *dsp = &ds;
    unsigned l;
    DSTRING_INIT(dsp);
    cmd = obj->id;
    nsFullName = NSCmdFullName(cmd);
    if (!(*nsFullName==':' && *(nsFullName+1)==':' &&
	  *(nsFullName+2)=='\0')) {
      Tcl_DStringAppend(dsp, nsFullName, -1);
    }
    Tcl_DStringAppend(dsp, "::", 2);
    Tcl_DStringAppend(dsp, Tcl_GetCommandName(NULL, obj->id), -1);

    l = (unsigned) Tcl_DStringLength(dsp)+1;
    objPtr->bytes = (char*) ckalloc(l);
    memcpy(objPtr->bytes, Tcl_DStringValue(dsp), l);
    objPtr->length = Tcl_DStringLength(dsp);
    DSTRING_FREE(dsp);
  } else if (obj) {
    /*fprintf(stderr,"try to read string of deleted command\n");*/
    FreeXOTclObjectInternalRep(objPtr);
    objPtr->bytes = NULL;
    objPtr->length = 0;
  } else {
    objPtr->bytes = NULL;
    objPtr->length = 0;
  }
  /*
    fprintf(stderr, "+++UpdateStringOfXOTclObject bytes='%s',length=%d\n",
	    objPtr->bytes,objPtr->length);
  */
}

#ifdef NOTUSED
static Tcl_Obj *
NewXOTclObjectObj(register XOTclObject *obj) {
    register Tcl_Obj *objPtr = 0;
    XOTclNewObj(objPtr);
    objPtr->bytes = NULL;
    objPtr->internalRep.otherValuePtr = obj;
    objPtr->typePtr = &XOTclObjectType;
#ifdef XOTCLOBJ_TRACE
  fprintf(stderr,"NewXOTclObjectObj %p\n",objPtr);
#endif
  return objPtr;
}
#endif

static Tcl_Obj *
NewXOTclObjectObjName(register XOTclObject *obj, char *name, int l)
{
  register Tcl_Obj *objPtr = 0;

  XOTclNewObj(objPtr);

  objPtr->length = l;
  objPtr->bytes = ckalloc(l+1);
  memcpy(objPtr->bytes, name, l);
  *(objPtr->bytes+l) = 0;
  objPtr->internalRep.otherValuePtr = obj;
  objPtr->typePtr = &XOTclObjectType;

#ifdef XOTCLOBJ_TRACE
    fprintf(stderr,"NewXOTclObjectObjName tcl %p (%d) xotcl %p (%d) %s\n",
	    objPtr, objPtr->refCount, obj, obj->refCount, objPtr->bytes);
#endif
  XOTclObjectRefCountIncr(obj);

  return objPtr;
}

static int
GetXOTclObjectFromObj(Tcl_Interp *interp, register Tcl_Obj *objPtr, XOTclObject **obj)
{
  int result;
  register Tcl_ObjType *cmdType = objPtr->typePtr;
#ifdef KEEP_TCL_CMD_TYPE
  static Tcl_ObjType *tclCmdNameType = NULL;

  if (tclCmdNameType == NULL) {
# if defined(PRE82)
    if (cmdType && cmdType != &XOTclObjectType && !strcmp(cmdType->name,"cmdName")) {
      tclCmdNameType = cmdType;
    }
# else
    static XOTclMutex initMutex = 0;
    XOTclMutexLock(&initMutex);
    if (tclCmdNameType == NULL)
      tclCmdNameType = Tcl_GetObjType("cmdName");
    XOTclMutexUnlock(&initMutex);
# endif
  }
#endif

  /*
   * Only really share the "::x" Tcl_Objs but not "x" because we so not have
   * references upon object kills and then will get dangling
   * internalRep references to killed XOTclObjects
   */
  if (cmdType == &XOTclObjectType) {
    if (obj) {
      XOTclObject *o = (XOTclObject*) objPtr->internalRep.otherValuePtr;
      int refetch = 0;
      if (o->flags & XOTCL_DESTROYED) {
	/*fprintf(stderr,"????? calling free by hand\n");*/
	FreeXOTclObjectInternalRep(objPtr);
	refetch = 1;
	result = SetXOTclObjectFromAny(interp, objPtr);
	if (result == TCL_OK) {
	  o = (XOTclObject*) objPtr->internalRep.otherValuePtr;
	  assert(o && !(o->flags & XOTCL_DESTROYED));
	}
      } else {
	result = TCL_OK;
      }

      *obj = o;

#ifdef XOTCLOBJ_TRACE
      if (result == TCL_OK)
	fprintf(stderr,"GetXOTclObjectFromObj tcl %p (%d) xotcl %p (%d) r=%d %s\n",
		objPtr, objPtr->refCount, o, o->refCount, refetch, objPtr->bytes);
      else
	fprintf(stderr,"GetXOTclObjectFromObj tcl %p (%d) **** rc=%d r=%d %s\n",
		objPtr, objPtr->refCount, result,refetch, objPtr->bytes);
#endif
    } else {
      result = TCL_OK;
    }
#ifdef KEEP_TCL_CMD_TYPE
  } else if (cmdType == tclCmdNameType) {
    Tcl_Command cmd = Tcl_GetCommandFromObj(interp, objPtr);
    if (cmd) {
      XOTclObject *o = XOTclGetObjectFromCmdPtr(cmd);

      /*fprintf(stderr,"Got Object from '%s' %p\n",objPtr->bytes,o);
      fprintf(stderr,"cmd->objProc %p == %p, proc=%p\n",
	      Tcl_Command_objProc(cmd), XOTclObjDispatch,
	      Tcl_Command_proc(cmd) );*/
      
      if (o) {
	if (obj) *obj = o;
	result = TCL_OK;
      } else {
	goto convert_to_xotcl_object;
      }
    } else goto convert_to_xotcl_object;
#endif
  } else {
#ifdef KEEP_TCL_CMD_TYPE
  convert_to_xotcl_object:
#endif
    result = SetXOTclObjectFromAny(interp, objPtr);
    if (result == TCL_OK) {
      if (obj) *obj = (XOTclObject*) objPtr->internalRep.otherValuePtr;
    }
  }
  return result;
}

static int
GetXOTclClassFromObj(Tcl_Interp *in, register Tcl_Obj *objPtr,
		     XOTclClass **cl, int retry) {
  XOTclObject *obj;
  XOTclClass *cls;
  int result = GetXOTclObjectFromObj(in, objPtr, &obj);
  if (result == TCL_OK) {
    cls = XOTclObjectToClass(obj);
    if (cls) {
      if (cl) *cl = cls;
    } else
      result = TCL_ERROR;
  } else if (retry) {
    Tcl_Obj *ov[3];
    ov[0] = RUNTIME_STATE(in)->theClass->object.cmdName;
    ov[1] = XOTclGlobalObjects[__UNKNOWN];
    ov[2] = objPtr;
    INCR_REF_COUNT(objPtr);
    /* fprintf(stderr,"+++ calling __unknown for %s\n", ObjStr(objPtr));*/
    result = Tcl_EvalObjv(in, 3, ov, 0);
    if (result == TCL_OK) {
      result = GetXOTclClassFromObj(in, objPtr, cl, 0);
    }
    DECR_REF_COUNT(objPtr);
  }
  return result;
}

/*
 * precedence ordering functions
 */

enum colors { WHITE, GRAY, BLACK };

static int
TopoSort(XOTclClass *cl, XOTclClass *base, XOTclClasses* (*next)(XOTclClass*)) {
  XOTclClasses* sl = (*next)(cl);
  XOTclClasses* pl;

  /*
   * careful to reset the color of unreported classes to
   * white in case we unwind with error, and on final exit
   * reset color of reported classes to white
   */

  cl->color = GRAY;
  for (; sl != 0; sl = sl->next) {
    XOTclClass *sc = sl->cl;
    if (sc->color == GRAY) { cl->color = WHITE; return 0; }
    if (sc->color == WHITE && !TopoSort(sc, base, next)) {
      cl->color = WHITE;
      if (cl == base) {
        XOTclClasses* pc = cl->order;
        while (pc != 0) { pc->cl->color = WHITE; pc = pc->next; }
      }
      return 0;
    }
  }
  cl->color = BLACK;
  pl = NEW(XOTclClasses);
  pl->cl = cl;
  pl->next = base->order;
  base->order = pl;
  if (cl == base) {
    XOTclClasses* pc = cl->order;
    while (pc != 0) { pc->cl->color = WHITE; pc = pc->next; }
  }
  return 1;
}

extern void
XOTclFreeClasses(XOTclClasses* sl) {
  XOTclClasses *n;
  for (; sl; sl = n) {
    n = sl->next;
    FREE(XOTclClasses,sl);
  }
}

extern XOTclClasses**
XOTclAddClass(XOTclClasses **cList, XOTclClass *cl, ClientData cd) {
  XOTclClasses *l = *cList, *element = NEW(XOTclClasses);
  element->cl = cl;
  element->clientData = cd;
  element->next = NULL;
  if (l) {
    while (l->next) l = l->next;
    l->next = element;
  } else
    *cList = element;
  return &(element->next);
}

static XOTclClasses* Super(XOTclClass *cl) { return cl->super; }
static XOTclClasses* Sub(XOTclClass *cl) { return cl->sub; }

static XOTclClasses*
TopoOrder(XOTclClass *cl, XOTclClasses* (*next)(XOTclClass*)) {
  if (TopoSort(cl, cl, next))
    return cl->order;
  XOTclFreeClasses(cl->order);
  cl->order = 0;
  return 0;
}

XOTCLINLINE static XOTclClasses*
ComputeOrder(register XOTclClass *cl, XOTclClasses* (*direction)(XOTclClass*)) {
  if (!cl->order)
    cl->order = TopoOrder(cl, direction);
  return cl->order;
}

extern XOTclClasses*
XOTclComputePrecedence(XOTclClass *cl) {
  return ComputeOrder(cl, Super);
}

extern XOTclClasses*
XOTclComputeDependents(XOTclClass *cl) {
  return ComputeOrder(cl, Sub);
}


static void
FlushPrecedences(XOTclClass *cl) {
  XOTclClasses* pc;
  XOTclFreeClasses(cl->order);
  cl->order = 0;
  pc = ComputeOrder(cl, Sub);

  /*
   * ordering doesn't matter here - we're just using toposort
   * to find all lower classes so we can flush their caches
   */

  if (pc) pc = pc->next;
  for (; pc; pc = pc->next) {
    XOTclFreeClasses(pc->cl->order);
    pc->cl->order = 0;
  }
  XOTclFreeClasses(cl->order);
  cl->order = 0;
}

static void
AddInstance(XOTclObject *obj, XOTclClass *cl) {
  obj->cl = cl;
  if (cl != 0) {
    int nw;
    (void) Tcl_CreateHashEntry(&cl->instances, (char*)obj, &nw);
  }
}

static int
RemoveInstance(XOTclObject *obj, XOTclClass *cl) {
  if (cl) {
    Tcl_HashEntry *hPtr = Tcl_FindHashEntry(&cl->instances, (char*)obj);
    if (hPtr) {
	Tcl_DeleteHashEntry(hPtr);
	return 1;
    }
  }
  return 0;
}

/*
 * superclass/subclass list maintenance
 */

static void
AS(XOTclClass *cl, XOTclClass *s, XOTclClasses **sl) {
  register XOTclClasses* l = *sl;
  while (l && l->cl != s) l = l->next;
  if (!l) {
    XOTclClasses* sc = NEW(XOTclClasses);
    sc->cl = s;
    sc->next = *sl;
    *sl = sc;
  }
}

static void
AddSuper(XOTclClass *cl, XOTclClass *super) {
  if (cl && super) {
    /*
     * keep corresponding sub in step with super
     */
    AS(cl, super, &cl->super);
    AS(super, cl, &super->sub);
  }
}

static int
RemoveSuper1(XOTclClass *cl, XOTclClass *s, XOTclClasses **sl) {
  XOTclClasses* l = *sl;
  if (!l) return 0;
  if (l->cl == s) {
    *sl = l->next;
    FREE(XOTclClasses,l);
    return 1;
  }
  while (l->next && l->next->cl != s) l = l->next;
  if (l->next) {
    XOTclClasses* n = l->next->next;
    FREE(XOTclClasses,l->next);
    l->next = n;
    return 1;
  }
  return 0;
}

static int
RemoveSuper(XOTclClass *cl, XOTclClass *super) {
  /*
   * keep corresponding sub in step with super
   */
  int sp = RemoveSuper1(cl, super, &cl->super);
  int sb = RemoveSuper1(super, cl, &super->sub);

  return sp && sb;
}

/*
 * internal type checking
 */

extern  XOTcl_Class*
XOTclIsClass(Tcl_Interp *in, ClientData cd) {
  XOTclObject *obj = (XOTclObject*)cd;
  if (obj && XOTclObjectIsClass(obj))
    return (XOTcl_Class*) cd;
  return 0;
}

/*
 * methods lookup
 */

XOTCLINLINE static Tcl_Command
FindMethod (char *methodName, Tcl_HashTable *cmdTable) {
  Tcl_HashEntry* entryPtr;
  Tcl_Command cmd;

  assert(cmdTable);
  if ((entryPtr = Tcl_FindHashEntry(cmdTable, methodName))) {
    cmd = (Tcl_Command) Tcl_GetHashValue(entryPtr);
  } else {
    cmd = NULL;
  }
  return cmd;
}

XOTCLINLINE static XOTclClass*
SearchPLMethod(register XOTclClasses* pl, char *nm, Tcl_Command *cmd) {
  /* Search the class hierarchy */
  for (; pl;  pl = pl->next) {
    Tcl_Command pi = FindMethod(nm, Tcl_Namespace_cmdTable(pl->cl->nsPtr));
    if (pi) {
      *cmd = pi;
      return pl->cl;
    }
  }
  return NULL;
}


static XOTclClass*
SearchCMethod(XOTclClass *cl, char *nm, Tcl_Command *cmd) {
  assert(cl);
  return SearchPLMethod(ComputeOrder(cl, Super), nm, cmd);
}

static int
callDestroyMethod(ClientData cd, Tcl_Interp *in, XOTclObject *obj, int flags) {
  int result;

  /* don't call destroy after exit handler started physical
     destruction */
  if (RUNTIME_STATE(in)->exitHandlerDestroyRound ==
      XOTCL_EXITHANDLER_ON_PHYSICAL_DESTROY)
    return TCL_OK;

  /* we don't call destroy, if we're in the exit handler
     during destruction of Object and Class */
  if (!RUNTIME_STATE(in)->callDestroy) {
    obj->flags |= XOTCL_DESTROY_CALLED;

    /* return TCL_ERROR so that clients know we haven't deleted the
       associated command yet */
    return TCL_ERROR;
  }
  /*fprintf(stderr, "+++ calldestroy flags=%d\n",flags);*/
  if (obj->flags & XOTCL_DESTROY_CALLED)
    return TCL_OK;

#if !defined(NDEBUG)
  {char *cmdName = ObjStr(obj->cmdName);
  assert(cmdName != NULL);
  assert(Tcl_FindCommand(in, cmdName, NULL, 0) != NULL);
  }
#endif


#ifdef OBJDELETION_TRACE
  fprintf(stderr, "   command found\n");
  PRINTOBJ("callDestroy", obj);
#endif
  result = callMethod(cd, in, XOTclGlobalObjects[DESTROY], 2, 0, flags);
  if (result != TCL_OK) {
    static char cmd[] =
	"puts stderr \"[self]: Error in instproc destroy\n\
	 $::errorCode $::errorInfo\"";
    Tcl_Eval(in, cmd);
    if (++RUNTIME_STATE(in)->errorCount > 20)
      panic("too many destroy errors occured. Endless loop?", NULL);
  } else {
    if (RUNTIME_STATE(in)->errorCount > 0)
      RUNTIME_STATE(in)->errorCount--;
  }

#ifdef OBJDELETION_TRACE
  fprintf(stderr, "callDestroyMethod for %p exit\n", obj);
#endif
  return result;
}

/*
 * conditional memory allocations of optional storage
 */

extern XOTclObjectOpt *
XOTclRequireObjectOpt(XOTclObject *obj) {
   if (!obj->opt) {
     obj->opt = NEW(XOTclObjectOpt);
     memset(obj->opt, 0, sizeof(XOTclObjectOpt));
   }
   return obj->opt;
}

extern XOTclClassOpt*
XOTclRequireClassOpt(XOTclClass *cl) {
  assert(cl);
  if (!cl->opt) {
    cl->opt = NEW(XOTclClassOpt);
    memset(cl->opt, 0, sizeof(XOTclClassOpt));
  }
  return cl->opt;
}

#define freeObjectOpt(obj) \
  if (obj->opt) {\
    FREE(XOTclObjectOpt,obj->opt); \
    obj->opt = 0; }

#define freeClassOpt(cl) \
  if (cl->opt) { \
    FREE(XOTclClassOpt, cl->opt); \
    cl->opt = 0; }


static Tcl_Namespace*
NSGetFreshNamespace(Tcl_Interp *in, ClientData cd, char *name);

static void
makeObjNamespace(Tcl_Interp *in, XOTclObject *obj) {
#ifdef NAMESPACE_TRACE
  fprintf(stderr, "+++ Make Namespace for %s\n", ObjStr(obj->cmdName));
#endif
  if (!obj->nsPtr) {
    Tcl_Namespace* nsPtr;
    char *cmdName =  ObjStr(obj->cmdName);
    obj->nsPtr = NSGetFreshNamespace(in, (ClientData)obj, cmdName);
    if (!obj->nsPtr)
      panic("makeObjNamespace: Unable to make namespace",0);
    nsPtr = obj->nsPtr;

    /*
     * Copy all obj variables to the newly created namespace
     */
    if (obj->varTable) {
      Tcl_HashSearch search;
      Tcl_HashEntry *hPtr, *newHPtr;
      register Var *varPtr;

      for (hPtr = Tcl_FirstHashEntry(obj->varTable, &search);  hPtr != NULL;
	   hPtr = Tcl_NextHashEntry(&search)) {
	int new = 0;
	char *name = Tcl_GetHashKey(obj->varTable, hPtr);
	Tcl_HashTable *varTable = Tcl_Namespace_varTable(nsPtr);

	varPtr = (Var *) Tcl_GetHashValue(hPtr);
	
	if (!name) {
	  panic("Can't copy: Hash Entry with no name", NULL);
	  continue;
	}

	newHPtr = Tcl_CreateHashEntry(varTable, name, &new);
	if (new) {
	  /*
	   * put var into new hashtable entry
	   */
	  varPtr->flags |= VAR_IN_HASHTABLE;
	  Tcl_SetHashValue(newHPtr, varPtr);
	  varPtr->hPtr = newHPtr;
	  /*
	   * Mark the variable as a namespace variable
	
	  if (!(varPtr->flags & VAR_NAMESPACE_VAR)) {
	    varPtr->flags |= VAR_NAMESPACE_VAR;
	    varPtr->refCount++;
	  }
	  */
	  /*
	   * and correct the namespace information
	   */
	  varPtr->nsPtr = (Namespace *)nsPtr;
	} else {
	  panic("Can't copy varTable variable to new namespace", NULL);
	}
      }
      /*
      MEM_COUNT_FREE("obj->varTable",obj->varTable);
      */
      Tcl_DeleteHashTable(obj->varTable);
      ckfree((char *) obj->varTable);
      obj->varTable = 0;
    }
  }
}

#define requireObjNamespace(in,obj) \
  if (!obj->nsPtr) makeObjNamespace(in,obj)

extern void
XOTclRequireObjNamespace(Tcl_Interp *in, XOTcl_Object *obji) {
  XOTclObject *obj = (XOTclObject*) obji;
  requireObjNamespace(in, obj);
}


/*
 * Namespace related commands
 */

static int
NSDeleteCmd(Tcl_Interp *in, Tcl_Namespace* ns, char *name) {
  /* a simple deletion would delete a global command with
     the same name, if the command is not existing, so
     we use the CmdToken */
  Tcl_Command token;
  assert(ns);
  if ((token = FindMethod(name, Tcl_Namespace_cmdTable(ns)))) {
    return Tcl_DeleteCommandFromToken(in, token);
  }
  return 0;
}

static void
CallStackDestroyObject(Tcl_Interp *in, XOTclObject *obj);
static void
PrimitiveCDestroy(ClientData cd);
static void
PrimitiveODestroy(ClientData cd);

static void
NSDeleteChildren(Tcl_Interp *in, Tcl_Namespace* ns) {
  /*Tcl_HashTable *childTable = Tcl_Namespace_childTable(ns);*/
  Tcl_HashTable *cmdTable = Tcl_Namespace_cmdTable(ns);
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr = Tcl_FirstHashEntry(cmdTable, &hSrch);

#ifdef OBJDELETION_TRACE
  fprintf(stderr, "NSDeleteChildren %s\n", ns->fullName);
#endif
  for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
    Tcl_Command cmd = (Tcl_Command)Tcl_GetHashValue(hPtr);
    if (!Tcl_Command_cmdEpoch(cmd)) {
      char *oname = Tcl_GetHashKey(cmdTable, hPtr);
      Tcl_DString name;
      XOTclObject *obj;
      /*  fprintf(stderr, " ... child %s\n", oname);
       */
      ALLOC_NAME_NS(&name, ns->fullName, oname);
      obj = GetObject(in, Tcl_DStringValue(&name));

      if (obj) {
	/*
	  fprintf(stderr, " ... obj= %s\n", ObjStr(obj->cmdName));
	*/
	/* in the exit handler physical destroy --> directly call destroy */
	if (RUNTIME_STATE(in)->exitHandlerDestroyRound
	    == XOTCL_EXITHANDLER_ON_PHYSICAL_DESTROY) {
	  if (XOTclObjectIsClass(obj))
	    PrimitiveCDestroy((ClientData) obj);
	  else
	    PrimitiveODestroy((ClientData) obj);
	} else {
	  if (obj->teardown != 0 && obj->id &&
	      !(obj->flags & XOTCL_DESTROY_CALLED)) {
	    /*         Tcl_Command oid = obj->id;*/
	    if (callDestroyMethod((ClientData)obj, in, obj, 0) != TCL_OK) {
	      /* destroy method failed, but we have to remove the command
		 anyway. */
	      obj->flags |= XOTCL_DESTROY_CALLED;
	
	      if (obj->teardown) {
		CallStackDestroyObject(in, obj);
	      }
	      /*(void*) Tcl_DeleteCommandFromToken(in, oid);*/
	    }
	  }
	}
      }
      DSTRING_FREE(&name);
    }
  }
}

/*
 * ensure that a variable exists on object varTable or nsPtr->varTable,
 * if necessary create it. Return Var* if successful, otherwise 0
 */
static Var*
NSRequireVariableOnObj(Tcl_Interp *in, XOTclObject *obj, char *name, int flgs) {
  XOTcl_FrameDecls;
  Var *varPtr, *arrayPtr;

  XOTcl_PushFrame(in, obj);
  varPtr = TclLookupVar(in, name, 0, flgs, "obj vwait",
			/*createPart1*/ 1, /*createPart2*/ 0, &arrayPtr);
  XOTcl_PopFrame(in, obj);
  return varPtr;
}

/* delete a namespace recursively, but check whether the
   namespace is an object or not */
static void
NSDeleteNamespace(Tcl_Interp *in, Tcl_Namespace* ns) {
  Tcl_HashTable *childTable = Tcl_Namespace_childTable(ns);
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr;

  NSDeleteChildren(in, ns);
  for (hPtr = Tcl_FirstHashEntry(childTable, &hSrch); hPtr != 0;
       hPtr = Tcl_NextHashEntry(&hSrch)) {
    Tcl_Namespace *child = ((Tcl_Namespace*)Tcl_GetHashValue(hPtr));
    NSDeleteNamespace(in, child);
  }
  /*	
  fprintf(stderr, "NSDeleteNamespace deleting %s\n", ns->fullName);
  */
  MEM_COUNT_FREE("TclNamespace",ns);
  Tcl_DeleteNamespace(ns);
}

static int
XOTcl_DeleteCommandFromToken(Tcl_Interp *in, Tcl_Command cmd) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  XOTclCallStackContent *csc = cs->top;

  for (; csc > cs->content; csc--) {
    if (csc->cmdPtr == cmd)
      csc->cmdPtr = NULL;
    break;
  }
  return Tcl_DeleteCommandFromToken(in, cmd);
}

/*
 * delete all vars & procs in a namespace
 */
static void
NSCleanupNamespace(Tcl_Interp *in, Tcl_Namespace* ns) {
  Tcl_HashTable *varTable = Tcl_Namespace_varTable(ns);
  Tcl_HashTable *cmdTable = Tcl_Namespace_cmdTable(ns);
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr;
  Tcl_Command cmd;
  /*
   * Delete all variables and initialize var table again
   * (deletevars frees the vartable)
   */
  TclDeleteVars((Interp *)in, varTable);
  Tcl_InitHashTable(varTable, TCL_STRING_KEYS);

  /*
   * Delete all user-defined procs in the namespace
   */
  for (hPtr = Tcl_FirstHashEntry(cmdTable, &hSrch); hPtr != 0;
       hPtr = Tcl_NextHashEntry(&hSrch)) {
    cmd = (Tcl_Command) Tcl_GetHashValue(hPtr);
    /* objects should not be deleted here to preseve children deletion order*/
    if (!XOTclGetObjectFromCmdPtr(cmd)) {
      XOTcl_DeleteCommandFromToken(in, cmd);
    }
  }
}


static void
NSNamespaceDeleteProc(ClientData clientData) {
  /* dummy for ns identification by pointer comparison */
}

void
XOTcl_DeleteNamespace(Tcl_Interp *in, Tcl_Namespace *nsPtr) {
  int activationCount = 0;
  Tcl_CallFrame *f = (Tcl_CallFrame *)Tcl_Interp_framePtr(in);

  /*
  fprintf(stderr, "  ... correcting ActivationCount for %s was %d ",
	  nsPtr->fullName, nsp->activationCount);
  */
  while (f) {
    if (f->nsPtr == nsPtr)
      activationCount++;
    f = Tcl_CallFrame_callerPtr(f);
  }

  Tcl_Namespace_activationCount(nsPtr) = activationCount;
  /*
  fprintf(stderr, "to %d. \n", nsp->activationCount);
  */
  MEM_COUNT_FREE("TclNamespace",nsPtr);
  Tcl_DeleteNamespace(nsPtr);
}

static Tcl_Namespace*
NSGetFreshNamespace(Tcl_Interp *in, ClientData cd, char *name) {
  Tcl_Namespace *ns = Tcl_FindNamespace(in, name, NULL, 0);

  if (ns) {
    if (ns->deleteProc != NULL || ns->clientData != NULL) {
      panic("Can only convert a plain Tcl namespace into an XOTcl namespace",0);
    }
    ns->clientData = cd;
    ns->deleteProc = (Tcl_NamespaceDeleteProc*) NSNamespaceDeleteProc;
  } else {
    ns = Tcl_CreateNamespace(in, name, cd,
			     (Tcl_NamespaceDeleteProc*) NSNamespaceDeleteProc);
  }
  MEM_COUNT_ALLOC("TclNamespace",ns);
  return ns;
}


/*
 * check colons for illegal object/class names
 */
XOTCLINLINE static int
NSCheckColons(char *name, int l) {
  register char *n = name;
  if (*n == '\0') return 0; /* empty name */
  if (*(n+l-1) == ':')  return 0; /* name ends with : */
  if (*n == ':' && *(n+1) != ':') return 0; /* name begins with single : */
  for (; *n != '\0'; n++) {
    if (*n == ':' && *(n+1) == ':' && *(n+2) == ':')
      return 0;   /* more than 2 colons in series in a name */
  }
  return 1;
}

/*
 * check for parent namespace existance (used before commands are created)
 */

XOTCLINLINE static int
NSCheckForParent(Tcl_Interp *in, char *name, int l) {
  register char *n = name+l;
  int result = 1;

  /*search for last '::'*/
  while ((*n != ':' || *(n-1) != ':') && n-1 > name) {n--; }
  if (*n == ':' && n > name && *(n-1) == ':') {n--;}

  if ((n-name)>0) {
    Tcl_DString parentNSName, *dsp = &parentNSName;
    char *parentName;
    DSTRING_INIT(dsp);

    Tcl_DStringAppend(dsp, name, (n-name));
    parentName = Tcl_DStringValue(dsp);

    if (Tcl_FindNamespace(in, parentName, (Tcl_Namespace *) NULL, TCL_GLOBAL_ONLY) == 0) {
      XOTclObject *parentObj = (XOTclObject*) GetObject(in, parentName);
      if (parentObj) {
	requireObjNamespace(in, parentObj);
      } else {
	result = 0;
      }
    }
    DSTRING_FREE(dsp);
  }
  return result;
}

/*
 * Find the "real" command belonging eg. to an XOTcl class or object.
 * Do not return cmds produced by Tcl_Import, but the "real" cmd
 * to which they point.
 */
XOTCLINLINE static Tcl_Command
NSFindCommand(Tcl_Interp *in, char *name) {
  Tcl_Command cmd;
  if ((cmd = Tcl_FindCommand(in, name, NULL, 0))) {
    Tcl_Command importedCmd;
    if ((importedCmd = TclGetOriginalCommand(cmd)))
      cmd = importedCmd;
  }
  return cmd;
}



/*
 * C interface routines for manipulating objects and classes
 */



extern XOTcl_Object*
XOTclGetObject(Tcl_Interp *in, char *name) {
  return (XOTcl_Object*) GetObject(in, name);
}

/*
 * Find an object using a char *name
 */
static XOTclObject*
GetObject(Tcl_Interp *in, char *name) {
  register Tcl_Command cmd;
  assert(name);
  cmd = NSFindCommand(in, name);
  if (cmd && Tcl_Command_objProc(cmd) == XOTclObjDispatch) {
    return (XOTclObject*)Tcl_Command_objClientData(cmd);
  }
  return 0;
}

/*
 * Find a class using a char *name
 */

extern XOTcl_Class*
XOTclGetClass(Tcl_Interp *in, char *name) {
  return (XOTcl_Class*)GetClass(in, name);
}

static XOTclClass*
GetClass(Tcl_Interp *in, char *name) {
  XOTclObject *obj = GetObject(in, name);
  return (obj && XOTclObjectIsClass(obj)) ? (XOTclClass*)obj : NULL;
}

static XOTclClass*
GetClassFromFullName(Tcl_Interp *in, char *fullName) {
  XOTclClass *cl;
  if (isClassName(fullName)) {
    cl = GetClass(in, NSCutXOTclClasses(fullName));
  } else {
    cl = NULL;
  }
  return cl;
}


void
XOTclAddPMethod(Tcl_Interp *in, XOTcl_Object *obji, char *nm, Tcl_ObjCmdProc* proc,
		ClientData cd, Tcl_CmdDeleteProc* dp) {
  XOTclObject *obj = (XOTclObject*) obji;
  Tcl_DString newCmd, *cptr = &newCmd;
  requireObjNamespace(in, obj);
  ALLOC_NAME_NS(cptr, obj->nsPtr->fullName, nm);
  Tcl_CreateObjCommand(in, Tcl_DStringValue(cptr), proc, cd, dp);
  DSTRING_FREE(cptr);
}

void
XOTclAddIMethod(Tcl_Interp *in, XOTcl_Class *cli, char *nm,
		Tcl_ObjCmdProc* proc, ClientData cd, Tcl_CmdDeleteProc* dp) {
  XOTclClass *cl = (XOTclClass*) cli;
  Tcl_DString newCmd, *cptr = &newCmd;
  ALLOC_NAME_NS(cptr, cl->nsPtr->fullName, nm);
  Tcl_CreateObjCommand(in, Tcl_DStringValue(cptr), proc, cd, dp);
  DSTRING_FREE(cptr);
}


/*
 * Generic Tcl_Obj List
 */

static void
TclObjListFreeList(XOTclTclObjList* list) {
  XOTclTclObjList* del;
  while (list) {
    del = list;
    list = list->next;
    DECR_REF_COUNT(del->content);
    FREE(XOTclTclObjList, del);
  }
}

static Tcl_Obj*
TclObjListNewElement(XOTclTclObjList **list, Tcl_Obj *ov) {
  XOTclTclObjList* elt = NEW(XOTclTclObjList);
  INCR_REF_COUNT(ov);
  elt->content = ov;
  elt->next = *list;
  *list = elt;
  return ov;
}

/*
 * Autonaming
 */

static Tcl_Obj*
AutonameIncr(Tcl_Interp *in, Tcl_Obj *name, XOTclObject *obj,
	     int instanceOpt, int resetOpt) {
  int valueLength, mustCopy = 1, format = 0;
  char *valueString, *c;
  Tcl_Obj *valueObject, *result = NULL, *savedResult = NULL;
  XOTcl_FrameDecls;
  int flgs = TCL_LEAVE_ERR_MSG;

  XOTcl_PushFrame(in, obj);
  if (obj->nsPtr)
    flgs |= TCL_NAMESPACE_ONLY;

#ifndef PRE83
  valueObject = TclIncrVar2(in, XOTclGlobalObjects[AUTONAMES], name, 1, flgs);
#else
  valueObject = TclIncrVar2(in, XOTclGlobalObjects[AUTONAMES], name, 1, 0);
#endif

  if (resetOpt) {
    if (valueObject != NULL) { /* we have an entry */
      Tcl_UnsetVar2(in, XOTclGlobalStrings[AUTONAMES], ObjStr(name), flgs);
    }
    result = XOTclGlobalObjects[EMPTY];
    INCR_REF_COUNT(result);
  } else {
    if (valueObject == NULL) {
      valueObject = Tcl_ObjSetVar2(in, XOTclGlobalObjects[AUTONAMES],
				   name, XOTclGlobalObjects[ZERO], flgs);
    }
    if (instanceOpt) {
      char buffer[1], firstChar, *nextChars;
      nextChars = ObjStr(name);
      firstChar = *(nextChars ++);
      if (isupper((int)firstChar)) {
	buffer[0] = tolower((int)firstChar);
	result = Tcl_NewStringObj(buffer,1);
	INCR_REF_COUNT(result);
	Tcl_AppendToObj(result, nextChars, -1);
	mustCopy = 0;
      }
    }
    if (mustCopy) {
      result = Tcl_DuplicateObj(name);
      INCR_REF_COUNT(result);
      /*
      fprintf(stderr,"*** copy %p %s = %p\n", name,ObjStr(name),result);
      */
    }
    valueString = Tcl_GetStringFromObj(valueObject,&valueLength);

    /* if we find a % in the autoname -> We use Tcl_FormatObjCmd
       to let the autoname string be formated, like Tcl "format"
       command, with the value. E.g.:
	    autoname a%06d --> a000000, a000001, a000002, ...
    */
    for (c = ObjStr(result); *c != '\0'; c++) {
      if (*c == '%') {
	if (*(c+1) != '%') {
	  format = 1;
	  break;
	} else {
	  /* when we find a %% we format and then append autoname, e.g.
	     autoname a%% --> a%1, a%2, ... */
	  c++;
	}
      }
    }
    if (format) {
      DEFINE_NEW_TCL_OBJS_ON_STACK(3, ov);
      savedResult = Tcl_GetObjResult(in);
      INCR_REF_COUNT(savedResult);
      ov[0] = XOTclGlobalObjects[FORMAT];
      ov[1] = result;
      ov[2] = valueObject;
      if (Tcl_EvalObjv(in, 3, ov, 0) != TCL_OK) {
	XOTcl_PopFrame(in, obj);
	DECR_REF_COUNT(savedResult);
	FREE_TCL_OBJS_ON_STACK(ov);
	return 0;
      }
      DECR_REF_COUNT(result);
      result = Tcl_DuplicateObj(Tcl_GetObjResult(in));
      INCR_REF_COUNT(result);
      Tcl_SetObjResult(in, savedResult);
      DECR_REF_COUNT(savedResult);
      FREE_TCL_OBJS_ON_STACK(ov);
    } else {
      /* append the value string, if not
	 formated or if only %% occurs */
      /*fprintf(stderr,"+++ append to obj %p %s : %s\n",
	result,ObjStr(result), valueString);*/
      Tcl_AppendToObj(result, valueString, valueLength);
      /*fprintf(stderr,"+++ append to obj done\n");*/
    }
  }

  XOTcl_PopFrame(in, obj);
  assert((resetOpt && result->refCount>=1) || (result->refCount == 1));
  return result;
}

/*
 * XOTcl CallStack
 */

XOTclCallStackContent *
XOTclCallStackFindLastInvocation(Tcl_Interp *in, int offset) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  register XOTclCallStackContent *csc = CallStackGetFrame(in);

  /* skip through toplevel inactive filters, do this offset times */
  for (csc=cs->top; csc > cs->content; csc--) {
    if (csc->callsNext || (csc->frameType & XOTCL_CSC_INACTIVE_FLAG))
      continue;
    if (offset)
      offset--;
    else
      return csc;
  }
  /* for some reasons, we could not find invocation (topLevel, destroy) */
  return NULL;
}

static XOTclCallStackContent *
CallStackFindActiveFilter(Tcl_Interp *in) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  register XOTclCallStackContent *csc;

  /* search for first active frame and set tcl frame pointers */
  for (csc=cs->top; csc > cs->content; csc --) {
    if (csc->frameType & XOTCL_CSC_INACTIVE_FLAG) continue;
    if (csc->frameType & XOTCL_CSC_TYPE_FILTER) return csc;
  }
  /* for some reasons, we could not find invocation (topLevel, destroy) */
  return NULL;
}

XOTclCallStackContent *
XOTclCallStackFindActiveFrame(Tcl_Interp *in, int offset) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  register XOTclCallStackContent *csc;

  /* search for first active frame and set tcl frame pointers */
  for (csc=cs->top-offset; csc > cs->content; csc --) {
    if (!(csc->frameType & XOTCL_CSC_INACTIVE_FLAG)) {
      /* we found the highest active frame */
      return csc;
    }
  }
  /* we could not find an active frame; called from toplevel? */
  return NULL;
}

static void
CallStackUseActiveFrames(Tcl_Interp *in, callFrameContext *ctx) {
  XOTclCallStackContent *active, *top = RUNTIME_STATE(in)->cs.top;
  Tcl_CallFrame *inFramePtr = Tcl_Interp_varFramePtr(in);

  active = XOTclCallStackFindActiveFrame(in, 0);
  /*fprintf(stderr,"active %p, varFrame(in) %p, topVarFrame %p, active->curr %p\n",
	  active, inFramePtr, top->currentFramePtr,
	  active? active->currentFramePtr : NULL);*/

  if (active == top || inFramePtr == NULL) {
    /* top frame is a active frame, or we could not find a calling
       frame, call frame pointers are fine */
    ctx->framesSaved = 0;
  } else if (active == NULL) {
    Tcl_CallFrame *cf = inFramePtr;
    /* fprintf(stderr,"active == NULL\n"); */
    /* find a proc frame, which is not equal the top level cmd */
    for (; cf; cf = Tcl_CallFrame_callerPtr(cf)) {
      if (Tcl_CallFrame_isProcCallFrame(cf)) {
	Proc *procPtr = Tcl_CallFrame_procPtr(cf);
	/* fprintf(stderr, " procPtr=%p cmdPtr=%p '%s' top->cmdPtr %p\n",
		procPtr,procPtr->cmdPtr,
		(char*) Tcl_GetCommandName(in, top->cmdPtr), top->cmdPtr);*/
	if (procPtr && (Tcl_Command)procPtr->cmdPtr != top->cmdPtr) {
	  break;
	}
      }
    }
    ctx->varFramePtr = inFramePtr;
    Tcl_Interp_varFramePtr(in) = cf;
    ctx->framesSaved = 1;
  } else {
    Tcl_CallFrame *framePtr;
    /*fprintf(stderr,"active == deeper active=%p frame %p, active+1 %p frame %p\n",
	    active, active->currentFramePtr,
	    active+1, (active+1)->currentFramePtr);*/
    /* search returned a deeper pointer, use stored tcl frame pointers;
       If Tcl is mixed with XOTcl it is needed to use instead of
       active->currentFrame the callerPtr of the last inactive frame
       unless the last inactive is NULL */
    if ((framePtr = (active+1)->currentFramePtr))
      framePtr = Tcl_CallFrame_callerPtr(framePtr);
    else
      framePtr = active->currentFramePtr;
    ctx->varFramePtr = inFramePtr;
    Tcl_Interp_varFramePtr(in) = framePtr;
    ctx->framesSaved = 1;
  }
}

static void
CallStackRestoreSavedFrames(Tcl_Interp *in, callFrameContext *ctx) {
  if (ctx->framesSaved) {
    Tcl_Interp_varFramePtr(in) = ctx->varFramePtr;
    /*RUNTIME_STATE(in)->varFramePtr = ctx->varFramePtr;*/

  }
}


XOTCLINLINE static int
CallStackPush(Tcl_Interp *in, XOTclObject *obj, XOTclClass *cl,
	      Tcl_Command cmd, int objc, Tcl_Obj *CONST objv[], int frameType) {
  XOTclCallStack *cs;
  register XOTclCallStackContent *csc;

  cs = &RUNTIME_STATE(in)->cs;
  if (cs->top >= &cs->content[MAX_NESTING_DEPTH-1]) {
    Tcl_SetResult(in, "too many nested calls to Tcl_EvalObj (infinite loop?)",
		  TCL_STATIC);
    return TCL_ERROR;
  }
  /*
  fprintf(stderr, "PUSH obj %s proc %s, self=%p cmd=%p (%s) id=%p (%s)\n",
	  ObjStr(obj->cmdName), procName, obj,
	  cmd, (char*) Tcl_GetCommandName(in, cmd),
	  obj->id, Tcl_GetCommandName(in, obj->id));
  */
  csc = ++cs->top;
  csc->self          = obj;
  csc->cl            = cl;
  csc->cmdPtr        = cmd;
  csc->destroyedCmd  = 0;
  csc->frameType     = frameType;
  csc->callsNext     = 0;
  csc->currentFramePtr = NULL; /* this will be set by InitProcNSCmd */

  if (frameType == XOTCL_CSC_TYPE_ACTIVE_FILTER)
    csc->filterStackEntry = obj->filterStack;
  else
    csc->filterStackEntry = 0;

  MEM_COUNT_ALLOC("CallStack",NULL);
  /*
  fprintf(stderr, "pop %p %p %d\n", cs->top, cs->content,
         cs->top == cs->content);

  if (cs->top == cs->content)
    XOTclShadowTclCommands(in, SHADOW_REFETCH);
  */
  return TCL_OK;
}

XOTCLINLINE static void
CallStackDoDestroy(Tcl_Interp *in, XOTclObject *obj) {
  Tcl_Command oid;

  oid = obj->id;
  PRINTOBJ("CallStackDoDestroy", obj);

  obj->id = 0;
  if (obj->teardown && oid) {
    Tcl_DeleteCommandFromToken(in, oid);
  }
}


static void
CallStackDestroyObject(Tcl_Interp *in, XOTclObject *obj) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  XOTclCallStackContent *csc;
  int countSelfs = 0;
  Tcl_Command oid = obj->id;

  for (csc = &cs->content[1]; csc <= cs->top; csc++) {
    if (csc->self == obj) {
      csc->destroyedCmd = oid;
      if (csc->destroyedCmd) {
	Tcl_Command_refCount(csc->destroyedCmd)++;
	MEM_COUNT_ALLOC("command refCount",csc->destroyedCmd);
      }
      countSelfs++;
    }
  }
  /* if the object is not referenced at the callstack anymore
     we have to directly destroy it, because CallStackPop won't
     find the object destroy */
  if (countSelfs == 0) {
    CallStackDoDestroy(in, obj);
  } else
    /* to prevail the deletion order call delete children now
       -> children destructors are called before parent's
       destructor */
    if (obj->teardown && obj->nsPtr) {
      NSDeleteChildren(in, obj->nsPtr);
    }
}

XOTCLINLINE static int
CallStackIsDestroyed(Tcl_Interp *in) {
  return (RUNTIME_STATE(in)->cs.top->destroyedCmd == NULL) ? 0 : 1;
}

XOTCLINLINE static void
CallStackPop(Tcl_Interp *in) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  XOTclCallStackContent *csc;
  XOTclCallStackContent *h = cs->top;

  assert(cs->top > cs->content);
  csc = cs->top;
  if (csc->destroyedCmd != 0) {
    int destroy = 1;
    TclCleanupCommand((Command *)csc->destroyedCmd);
    MEM_COUNT_FREE("command refCount", csc->destroyedCmd);
    /* do not physically destroy, when callstack still contains "self"
       entries of the object */
    while (--h > cs->content) {
      if (h->self == csc->self) {
	destroy = 0;
	break;
      }
    }
    if (destroy) {
      CallStackDoDestroy(in, csc->self);
    }
  }

  cs->top--;
  MEM_COUNT_FREE("CallStack",NULL);
}



XOTCLINLINE static XOTclCallStackContent*
CallStackGetTopFrame(Tcl_Interp *in) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  return cs->top;
}

static XOTclCallStackContent*
CallStackGetFrame(Tcl_Interp *in) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  register XOTclCallStackContent *top = cs->top;
  Tcl_CallFrame *varFramePtr = Tcl_Interp_varFramePtr(in);

  if (Tcl_Interp_framePtr(in) != varFramePtr && top->currentFramePtr) {
    XOTclCallStackContent *bot = cs->content + 1;
    /* we are in a uplevel */
    while (varFramePtr > top->currentFramePtr && top>bot) {
      top--;
    }
  }
  return top;
}

/*
 * cmd list handling
 */

/*
 * Cmd List Add/Remove ... returns the new element
 */
static XOTclCmdList*
CmdListAdd(XOTclCmdList **cList, Tcl_Command c, int noDuplicates) {
  XOTclCmdList *l = *cList, *new;

  /*
   * check for duplicates, if necessary
   */
  if (noDuplicates) {
    XOTclCmdList *h = l, **end = NULL;
    while (h) {
      if (h->cmdPtr == c)
	return h;
      end = &(h->next);
      h = h->next;
    }
    if (end) {
      /* no duplicates, no need to search below, we are at the end of the list */
      cList = end;
      l = NULL;
    }
  }

  /*
   * ok, we have no duplicates -> append "new"
   * to the end of the list
   */
  new = NEW(XOTclCmdList);
  new->cmdPtr = c;
  Tcl_Command_refCount(new->cmdPtr)++;
  MEM_COUNT_ALLOC("command refCount",new->cmdPtr);
  new->clientData = NULL;
  new->next = NULL;

  if (l) {
    while (l->next)
      l = l->next;
    l->next = new;
  } else
    *cList = new;
  return new;
}

static void
CmdListReplaceCmd(XOTclCmdList* replace, Tcl_Command cmd) {
  Tcl_Command del = replace->cmdPtr;
  replace->cmdPtr = cmd;
  Tcl_Command_refCount(cmd)++;
  MEM_COUNT_ALLOC("command refCount", cmd);
  TclCleanupCommand((Command *)del);
  MEM_COUNT_FREE("command refCount",cmd);
}

/*
static void
CmdListPrint(char *title, XOTclCmdList* cmdList) {
  if (cmdList)
    fprintf(stderr,title);
  while (cmdList) {
    fprintf(stderr, "   CL=%p, cmdPtr=%p, clientData=%p\n",
	    cmdList,
	    cmdList->cmdPtr,
	    cmdList->clientData);
      cmdList = cmdList->next;
  }
}
*/

/*
 * physically delete an entry 'del'
 */
static void
CmdListDeleteCmdListEntry(XOTclCmdList* del, XOTclFreeCmdListClientData* freeFct) {
  if (freeFct)
    (*freeFct)(del);
  MEM_COUNT_FREE("command refCount",del->cmdPtr);
  TclCleanupCommand((Command *)del->cmdPtr);
  FREE(XOTclCmdList, del);
}

/*
 * remove a command 'delCL' from a command list, but do not
 * free it ... returns the removed XOTclCmdList*
 */
static XOTclCmdList*
CmdListRemoveFromList(XOTclCmdList **cmdList, XOTclCmdList* delCL) {
  register XOTclCmdList* c = *cmdList, *del = 0;
  if (c == 0)
    return NULL;
  if (c == delCL) {
    *cmdList = c->next;
    del = c;
  } else {
    while (c->next && c->next != delCL) {
      c = c->next;
    }
    if (c->next == delCL) {
      del = delCL;
      c->next = delCL->next;
    }
  }
  return del;
}

/*
 * remove all command pointers from a list that have a bumped epoch
 */
static void
CmdListRemoveEpoched(XOTclCmdList **cmdList, XOTclFreeCmdListClientData *freeFct) {
   XOTclCmdList *f = *cmdList, *del;
   while (f) {
     if (Tcl_Command_cmdEpoch(f->cmdPtr)) {
       del = f;
       f = f->next;
       del = CmdListRemoveFromList(cmdList, del);
       CmdListDeleteCmdListEntry(del, freeFct);
     } else
       f = f->next;
   }
}

/*
 * delete all entries from a given namespace
 */
static void
CmdListRemoveNSFromList(XOTclCmdList **cmdList, Tcl_Namespace* nsPtr,
			XOTclFreeCmdListClientData* freeFct) {
  XOTclCmdList* c, *del = 0;
  /*
  CmdListRemoveEpoched(cmdList, freeFct);
  */
  c = *cmdList;
  while (c && Tcl_Command_nsPtr(c->cmdPtr) == nsPtr) {
    del = c;
    *cmdList = c->next;
    CmdListDeleteCmdListEntry(del, freeFct);
    c = *cmdList;
  }
  while (c) {
    if (Tcl_Command_nsPtr(c->cmdPtr) == nsPtr) {
      del = c;
      c = *cmdList;
      while (c->next && c->next != del)
	c = c->next;
      if (c->next == del)
	c->next = del->next;
      CmdListDeleteCmdListEntry(del, freeFct);
    }
    c = c->next;
  }
}

/*
 * free the memory of a whole 'cmdList'
 */
static void
CmdListRemoveList(XOTclCmdList **cmdList, XOTclFreeCmdListClientData* freeFct) {
  XOTclCmdList *del;
  while (*cmdList) {
    del = *cmdList;
    *cmdList = (*cmdList)->next;
    CmdListDeleteCmdListEntry(del, freeFct);
  }
}

/*
 * simple list search proc to search a list of cmds
 * for a command ptr
 */
static XOTclCmdList*
CmdListFindCmdInList(Tcl_Command cmd, XOTclCmdList* l) {
  register XOTclCmdList* h = l;
  while (h != 0) {
    if (h->cmdPtr == cmd)
      return h;
    h = h->next;
  }
  return 0;
}

/*
 * simple list search proc to search a list of cmds
 * for a simple Name
 */
static XOTclCmdList*
CmdListFindNameInList(Tcl_Interp *in, char *name, XOTclCmdList* l) {
  register XOTclCmdList* h = l;
  while (h != 0) {
    CONST84 char *cmdName = Tcl_GetCommandName(in, h->cmdPtr);
    if (cmdName[0] == name[0] && !strcmp(cmdName, name))
      return h;
    h = h->next;
  }
  return 0;
}

/*
 * Assertions
 */
static XOTclTclObjList*
AssertionNewList(Tcl_Interp *in, Tcl_Obj *aObj) {
  Tcl_Obj **ov; int oc;
  XOTclTclObjList *last = NULL;

  if (Tcl_ListObjGetElements(in, aObj, &oc, &ov) == TCL_OK) {
    if (oc > 0) {
      int i;
      for (i=oc-1; i>=0; i--) {
	TclObjListNewElement(&last, ov[i]);
      }
    }
  }
  return last;
}

static Tcl_Obj*
AssertionList(Tcl_Interp *in, XOTclTclObjList* alist) {
  Tcl_Obj *newAssStr = Tcl_NewStringObj("",0);
  for (; alist!=NULL; alist = alist->next) {
    Tcl_AppendStringsToObj(newAssStr, "{", ObjStr(alist->content),
			   "}", (char *) NULL);
    if (alist->next != NULL)
      Tcl_AppendStringsToObj(newAssStr, " ", (char*) NULL);
  }
  return newAssStr;
}

/* append a string of pre and post assertions to a proc
   or instproc body */
static void
AssertionAppendPrePost(Tcl_Interp *in, Tcl_DString *dsPtr, XOTclProcAssertion *procs) {
  if (procs) {
    Tcl_Obj *preAss = AssertionList(in, procs->pre);
    Tcl_Obj *postAss = AssertionList(in, procs->post);
    INCR_REF_COUNT(preAss); INCR_REF_COUNT(postAss);
    Tcl_DStringAppendElement(dsPtr, ObjStr(preAss));
    Tcl_DStringAppendElement(dsPtr, ObjStr(postAss));
    DECR_REF_COUNT(preAss); DECR_REF_COUNT(postAss);
  }
}

static int
AssertionListCheckOption(Tcl_Interp *in, XOTclObject *obj) {
  XOTclObjectOpt *opt = obj->opt;
  if (!opt)
    return TCL_OK;
  if (opt->checkoptions & CHECK_OBJINVAR)
    Tcl_AppendElement(in, "invar");
  if (opt->checkoptions & CHECK_CLINVAR)
    Tcl_AppendElement(in, "instinvar");
  if (opt->checkoptions & CHECK_PRE)
    Tcl_AppendElement(in, "pre");
  if (opt->checkoptions & CHECK_POST)
    Tcl_AppendElement(in, "post");
  return TCL_OK;
}

static XOTclProcAssertion*
AssertionFindProcs(XOTclAssertionStore* aStore, char *name) {
  Tcl_HashEntry *hPtr;
  if (aStore == NULL) return NULL;
  hPtr = Tcl_FindHashEntry(&aStore->procs, name);
  if (hPtr == NULL) return NULL;
  return (XOTclProcAssertion*) Tcl_GetHashValue(hPtr);
}

static void
AssertionRemoveProc(XOTclAssertionStore* aStore, char *name) {
  Tcl_HashEntry *hPtr;
  if (aStore) {
    hPtr = Tcl_FindHashEntry(&aStore->procs, name);
    if (hPtr) {
      XOTclProcAssertion* procAss =
	(XOTclProcAssertion*) Tcl_GetHashValue(hPtr);
      TclObjListFreeList(procAss->pre);
      TclObjListFreeList(procAss->post);
      FREE(XOTclProcAssertion, procAss);
      Tcl_DeleteHashEntry(hPtr);
    }
  }
}

static void
AssertionAddProc(Tcl_Interp *in, char *name, XOTclAssertionStore* aStore,
		 Tcl_Obj *pre, Tcl_Obj *post) {
  int nw = 0;
  Tcl_HashEntry* hPtr = NULL;
  XOTclProcAssertion *procs = NEW(XOTclProcAssertion);

  AssertionRemoveProc(aStore, name);
  procs->pre = AssertionNewList(in, pre);
  procs->post = AssertionNewList(in, post);
  hPtr = Tcl_CreateHashEntry(&aStore->procs, name, &nw);
  if (nw) Tcl_SetHashValue(hPtr, (ClientData)procs);
}

static XOTclAssertionStore*
AssertionCreateStore() {
  XOTclAssertionStore* aStore = NEW(XOTclAssertionStore);
  aStore->invariants = NULL;
  Tcl_InitHashTable(&aStore->procs, TCL_STRING_KEYS);
  MEM_COUNT_ALLOC("Tcl_InitHashTable",&aStore->procs);
  return aStore;
}

static void
AssertionRemoveStore(XOTclAssertionStore* aStore) {
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr;

  if (aStore) {
    hPtr = Tcl_FirstHashEntry(&aStore->procs, &hSrch);
    while (hPtr) {
      /*
       * AssertionRemoveProc calls Tcl_DeleteHashEntry(hPtr), thus
       * we get the FirstHashEntry afterwards again to proceed
       */
      AssertionRemoveProc(aStore, Tcl_GetHashKey(&aStore->procs, hPtr));
      hPtr = Tcl_FirstHashEntry(&aStore->procs, &hSrch);
    }
    Tcl_DeleteHashTable(&aStore->procs);
    MEM_COUNT_FREE("Tcl_InitHashTable",&aStore->procs);
    TclObjListFreeList(aStore->invariants);
    FREE(XOTclAssertionStore, aStore);
  }
}

/*
 * check a given condition in the current callframe's scope
 * it's the responsiblity of the caller to push the intended callframe
 */
static int
checkConditionInScope(Tcl_Interp *in, Tcl_Obj *condition) {
  int result, success;
  Tcl_Obj *ov[2];
  ov [1] = condition;
  INCR_REF_COUNT(condition);
  /*
  fprintf(stderr, "----- evaluating condition '%s'\n", ObjStr(condition));
  */
  result = XOTcl_ExprObjCmd(NULL, in, 2, ov);
  DECR_REF_COUNT(condition);
  /*
  fprintf(stderr, "----- running condition '%s', result=%d '%s'\n",
	  ObjStr(condition), result,
	  ObjStr(Tcl_GetObjResult(in)));
  */
  if (result == TCL_OK) {
    result = Tcl_GetIntFromObj(in,Tcl_GetObjResult(in),&success);
    /*
    fprintf(stderr, "   success=%d\n", success);
    */
    if (result == TCL_OK && success == 0)
      result = XOTCL_CHECK_FAILED;
  }
  return result;
}

static int
AssertionCheckList(Tcl_Interp *in, XOTclObject *obj,
		   XOTclTclObjList* alist, char *methodName) {
  XOTclTclObjList* checkFailed = NULL;
  Tcl_Obj *savedObjResult = Tcl_GetObjResult(in);
  int savedCheckoptions, acResult = TCL_OK;

  /*
   * no obj->opt -> checkoption == CHECK_NONE
   */
  if (!obj->opt)
    return TCL_OK;

  /* we do not check assertion modifying methods, otherwise
     we can not react in catch on a runtime assertion check failure */
  if (isCheckString(methodName) || isInfoString(methodName) ||
      isInvarString(methodName) || isInstinvarString(methodName) ||
      isProcString(methodName) || isInstprocString(methodName))
    return TCL_OK;

  INCR_REF_COUNT(savedObjResult);

  Tcl_ResetResult(in);

  while (alist != NULL) {
    /* Eval instead of IfObjCmd => the substitutions in the
       conditions will be done by Tcl */
    char *assStr = ObjStr(alist->content), *c = assStr;
    int comment = 0;

    while (c != 0 && *c != '\0') {
      if (*c == '#') {
	comment = 1; break;
      }
      c++;
    }
    if (!comment) {
      XOTcl_FrameDecls;
      XOTcl_PushFrame(in, obj);
      CallStackPush(in, obj, 0, 0, 0, 0, XOTCL_CSC_TYPE_PLAIN);

      /* don't check assertion during assertion check */
      savedCheckoptions = obj->opt->checkoptions;
      obj->opt->checkoptions = CHECK_NONE;

      /*fprintf(stderr, "Checking Assertion %s ", assStr);*/

      /*
       * now check the assertion in the pushed callframe's scope
       */
      acResult = checkConditionInScope(in, alist->content);
      if (acResult != TCL_OK)
	checkFailed = alist;

      obj->opt->checkoptions = savedCheckoptions;

      /*fprintf(stderr, "...%s\n", checkFailed ? "failed" : "ok");*/

      CallStackPop(in);
      XOTcl_PopFrame(in,obj);
    }
    if (checkFailed)
      break;
    alist = alist->next;
  }

  if (checkFailed != NULL) {
    DECR_REF_COUNT(savedObjResult);
    if (acResult == TCL_ERROR) {
      Tcl_Obj *sr = Tcl_GetObjResult(in);
      INCR_REF_COUNT(sr);	
      XOTclVarErrMsg(in, "Error in Assertion: {",
			  ObjStr(checkFailed->content), "} in proc '",
			  GetSelfProc(in), "'\n\n", ObjStr(sr), (char *)NULL);
      DECR_REF_COUNT(sr);
      return TCL_ERROR;
    }
    return XOTclVarErrMsg(in, "Assertion failed check: {",
			  ObjStr(checkFailed->content), "} in proc '",
			  GetSelfProc(in), "'", (char *)NULL);
  }

  Tcl_SetObjResult(in, savedObjResult);
  DECR_REF_COUNT(savedObjResult);
  return TCL_OK;
}

static int
AssertionCheckInvars(Tcl_Interp *in, XOTclObject *obj, char *method,
		     int checkoptions) {
  int result = TCL_OK;

  if (checkoptions & CHECK_OBJINVAR && obj->opt->assertions) {
    result = AssertionCheckList(in, obj, obj->opt->assertions->invariants,
				method);
  }

  if (result != TCL_ERROR && checkoptions & CHECK_CLINVAR) {
    XOTclClasses* clPtr;
    clPtr = ComputeOrder(obj->cl, Super);
    while (clPtr != 0 && result != TCL_ERROR) {
      XOTclAssertionStore* aStore = (clPtr->cl->opt) ? clPtr->cl->opt->assertions : 0;
      if (aStore) {
	result = AssertionCheckList(in, obj, aStore->invariants, method);
      }
      clPtr = clPtr->next;
    }
  }
  return result;
}

static int
AssertionCheck(Tcl_Interp *in, XOTclObject *obj, XOTclClass *cl,
	       char *method, int checkOption) {
  XOTclProcAssertion* procs;
  int result = TCL_OK;
  XOTclAssertionStore* aStore;

  if (cl)
    aStore = cl->opt ? cl->opt->assertions : 0;
  else
    aStore = obj->opt ? obj->opt->assertions : 0;

  assert(obj->opt);

  if (checkOption & obj->opt->checkoptions) {
    procs = AssertionFindProcs(aStore, method);
    if (procs) {
      switch (checkOption) {
      case CHECK_PRE:
	result = AssertionCheckList(in, obj, procs->pre, method);
	break;
      case CHECK_POST:
	result = AssertionCheckList(in, obj, procs->post, method);
	break;
      }
    }
    if (result != TCL_ERROR)
      result = AssertionCheckInvars(in, obj, method, obj->opt->checkoptions);
  }
  return result;
}

/*
 * Per-Object-Mixins
 */

/*
 * push a mixin stack information on this object
 */
static int
MixinStackPush(XOTclObject *obj) {
  register XOTclMixinStack* h = NEW(XOTclMixinStack);
  h->currentCmdPtr = 0;
  h->next = obj->mixinStack;
  obj->mixinStack = h;
  return 1;
}

/*
 * pop a mixin stack information on this object
 */
static void
MixinStackPop(XOTclObject *obj) {
  register XOTclMixinStack *h = obj->mixinStack;
  obj->mixinStack = h->next;
  FREE(XOTclMixinStack,h);
}

/*
 * Appends XOTclClasses* containing the mixin classes and their
 * superclasses to 'mixinClasses' list from a given mixinList
 */
static void
MixinComputeOrderFullList(Tcl_Interp *in, XOTclCmdList **mixinList,
			  XOTclClasses **mixinClasses) {
  XOTclCmdList *m;
  XOTclClasses *pl, **clPtr = mixinClasses;

  CmdListRemoveEpoched(mixinList, GuardDel);
  m = *mixinList;

  while (m) {
    XOTclClass *mCl = XOTclGetClassFromCmdPtr(m->cmdPtr);
    if (mCl) {
      for (pl = ComputeOrder(mCl, Super); pl; pl = pl->next) {
	/*fprintf(stderr, " %s, ", ObjStr(pl->cl->object.cmdName));*/
	if (!(pl->cl == RUNTIME_STATE(in)->theObject)) {
	  XOTclClassOpt* opt = pl->cl->opt;
	  if (opt && opt->instmixins != 0) {
	    /* compute transitively the instmixin classes of this added
	       class */
	    MixinComputeOrderFullList(in, &opt->instmixins, mixinClasses);
	  }
	  clPtr = XOTclAddClass(clPtr, pl->cl, m->clientData);
	}
      }
    }
    m = m->next;
  }
}

static void
MixinResetOrder(XOTclObject *obj) {
  CmdListRemoveList(&obj->mixinOrder, NULL /*GuardDel*/);
  obj->mixinOrder = 0;
}

/*
 * Computes a linearized order of per-object and per-class mixins. Then
 * duplicates in the full list and with the class inheritance list of
 * 'obj' are eliminated.
 * The precendence rule is that the last occurence makes it into the
 * final list.
 */
static void
MixinComputeOrder(Tcl_Interp *in, XOTclObject *obj) {
  XOTclClasses *fullList, *mixinClasses = 0, *nextCl, *pl,
    *checker, *guardChecker;

  if (obj->mixinOrder)  MixinResetOrder(obj);
  /*fprintf(stderr, "Mixin Order:\n First List: ");*/

  /* append per-obj mixins */
  if (obj->opt)
    MixinComputeOrderFullList(in, &obj->opt->mixins, &mixinClasses);

  /* append per-class mixins */
  for (pl = ComputeOrder(obj->cl, Super); pl; pl = pl->next) {
    XOTclClassOpt* opt = pl->cl->opt;
    if (opt && opt->instmixins)
      MixinComputeOrderFullList(in, &opt->instmixins, &mixinClasses);
  }

  fullList = mixinClasses;

  /* use no duplicates & no classes of the precedence order
     on the resulting list */
  while (mixinClasses) {
    checker = nextCl = mixinClasses->next;
    while (checker) {
      if (checker->cl == mixinClasses->cl) break;
      checker = checker->next;
    }
    /* if mixinClasses has no duplicate on mixinList ->
       check obj->cl hierachy */
    if (checker == 0) {
      for (checker = ComputeOrder(obj->cl, Super); checker; checker = checker->next) {
	if (checker->cl == mixinClasses->cl)
	  break;
      }
    }
    /* if class is also not found on precedence order ->
       add it to mixinOrder list (otherwise free the memory) */
    if (checker == 0) {
      XOTclCmdList* new;
      new = CmdListAdd(&obj->mixinOrder, mixinClasses->cl->object.id,
		       /*noDuplicates*/ 0);

      /* in the client data of the order list, we require the first
	 matiching guard ... scan the full list for it. */
      for (guardChecker = fullList; guardChecker; guardChecker = guardChecker->next) {
	if (guardChecker->cl == mixinClasses->cl) {
	  new->clientData = guardChecker->clientData;
	  break;
	}
      }

      /*
      fprintf(stderr, "  Adding %s,\n", ObjStr(mixinClasses->cl->object.cmdName));
      */
    }
    mixinClasses = nextCl;
  }

  /* ... and free the memory of the full list */
  while (fullList) {
    nextCl = fullList->next;
    FREE(XOTclClasses, fullList);
    fullList = nextCl;
  }
  /*CmdListPrint("mixin order\n", obj->mixinOrder);*/

}

/*
 * add a mixin class to 'mixinList' by appending it
 */
static int
MixinAdd(Tcl_Interp *in, XOTclCmdList **mixinList, Tcl_Obj *name) {
  XOTclClass *mixin;
  Tcl_Obj *guard = NULL;
  int ocName; Tcl_Obj **ovName;
  XOTclCmdList* new;

  if (Tcl_ListObjGetElements(in, name, &ocName, &ovName) == TCL_OK && ocName > 1) {
    if (ocName == 3 && !strcmp(ObjStr(ovName[1]), XOTclGlobalStrings[GUARD_OPTION])) {
      name = ovName[0];
      guard = ovName[2];
      /*fprintf(stderr,"mixinadd name = '%s', guard = '%s'\n", ObjStr(name), ObjStr(guard));*/
    } /*else return XOTclVarErrMsg(in, "mixin registration '", ObjStr(name),
	"' has too many elements.", (char *)NULL);*/
  }

  if (GetXOTclClassFromObj(in, name, &mixin, 1) != TCL_OK)
    return XOTclErrBadVal(in, "a class as mixin", ObjStr(name));


  new = CmdListAdd(mixinList, mixin->object.id, /*noDuplicates*/ 1);

  if (guard) {
    GuardAdd(in, new, guard);
  } else {
    if (new->clientData)
      GuardDel(new);
  }

  return TCL_OK;
}

/*
 * if the class hierarchy or class mixins have changed ->
 * invalidate mixin entries in all dependent instances
 */
static void
MixinInvalidateObjOrders(XOTclClass *cl) {
  XOTclClasses *saved = cl->order, *clPtr;
  cl->order = 0;

  for (clPtr = ComputeOrder(cl, Sub); clPtr; clPtr = clPtr->next) {
    Tcl_HashSearch hSrch;
    Tcl_HashEntry* hPtr = &clPtr->cl->instances ?
      Tcl_FirstHashEntry(&clPtr->cl->instances, &hSrch) : 0;

    for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
      XOTclObject *obj = (XOTclObject*)
	Tcl_GetHashKey(&clPtr->cl->instances, hPtr);
      MixinResetOrder(obj);
      obj->flags &= ~XOTCL_MIXIN_ORDER_VALID;
    }
  }

  XOTclFreeClasses(cl->order);
  cl->order = saved;
}

/*
 * the mixin order is either
 *   DEFINED (there are mixins on the instance),
 *   NONE    (there are no mixins for the instance),
 *   or INVALID (a class re-strucuturing has occured, thus it is not clear
 *               whether mixins are defined or not).
 * If it is INVALID MixinComputeDefined can be used to compute the order
 * and set the instance to DEFINE or NONE
 */
static void
MixinComputeDefined(Tcl_Interp *in, XOTclObject *obj) {
  MixinComputeOrder(in, obj);
  obj->flags |= XOTCL_MIXIN_ORDER_VALID;
  if (obj->mixinOrder)
    obj->flags |= XOTCL_MIXIN_ORDER_DEFINED;
  else
    obj->flags &= ~XOTCL_MIXIN_ORDER_DEFINED;
}

/*
 * walk through the mixin order until the current mixin is reached.
 * then use the next mixin as current mixin.
 *
 * precondition: obj->mixinStack is not NULL
 */
static void
MixinSeekCurrent(Tcl_Interp *in, XOTclObject *obj, XOTclCmdList **cmdList) {
  Tcl_Command currentCmdPtr;

  /* ensure that the mixin order is not invalid, otherwise compute order */
  assert(obj->flags & XOTCL_MIXIN_ORDER_VALID);
  /* otherwise: MixinComputeDefined(in, obj);*/

  currentCmdPtr = obj->mixinStack->currentCmdPtr;
  *cmdList = obj->mixinOrder;

  /* go forward to current class */
  while (*cmdList && currentCmdPtr) {
    if ((*cmdList)->cmdPtr == currentCmdPtr)
      currentCmdPtr = 0;
    *cmdList = (*cmdList)->next;
  }
}

/*
 * before we can perform a mixin dispatch, MixinSearchProc seeks the
 * current mixin and the relevant calling information
 */
static Tcl_Command
MixinSearchProc(Tcl_Interp *in, XOTclObject *obj, char *methodName,
		XOTclClass **cl, Tcl_ObjCmdProc **proc, ClientData* cp,
		Tcl_Command* currentCmdPtr) {
  Tcl_Command cmd = NULL;
  XOTclCmdList *cmdList;
  XOTclClass *cls;

  assert(obj);
  assert(obj->mixinStack);

  MixinSeekCurrent(in, obj, &cmdList);

  /*
  fprintf(stderr, "MixinSearch searching for '%s' %p\n", methodName, cmdList);
  CmdListPrint("MixinSearch CL = \n", cmdList);
  */

  while (cmdList) {
    if(Tcl_Command_cmdEpoch(cmdList->cmdPtr)) {
      cmdList = cmdList->next;
    } else {
      cls = XOTclGetClassFromCmdPtr(cmdList->cmdPtr);
      /*
      fprintf(stderr,"+++ MixinSearch %s->%s in %p cmdPtr %p clientData %p\n",
	      ObjStr(obj->cmdName),methodName, cmdList,
	      cmdList->cmdPtr, cmdList->clientData);
      */
      if (cls) {
	int guardOk = TCL_OK;
	cmd = FindMethod(methodName, Tcl_Namespace_cmdTable(cls->nsPtr));
	if (cmd && cmdList->clientData) {
	  if (!RUNTIME_STATE(in)->cs.guardCount) {
	    guardOk = GuardCall(obj, cls, (Tcl_Command) cmd, in, cmdList->clientData, 1);
	  }
	}
	if (cmd && guardOk == TCL_OK) {
	/*
	 * on success: compute mixin call data
	 */
	  *cl = cls;
	  *proc = Tcl_Command_objProc(cmd);
	  *cp   = Tcl_Command_objClientData(cmd);
	  *currentCmdPtr = cmdList->cmdPtr;
	  break;
        } else {
	  cmd = NULL;
	  cmdList = cmdList->next;
	}
      }
    }
  }

  return cmd;
}

/*
 * info option for mixins and instmixins
 */
static int
MixinInfo(Tcl_Interp *in, XOTclCmdList* m, char *pattern, int withGuards) {
  Tcl_Obj *list = Tcl_NewListObj(0, NULL);
  XOTclClass *mixinClass;
  while (m) {
    mixinClass = XOTclGetClassFromCmdPtr(m->cmdPtr);
    if (mixinClass &&
	(!pattern ||
	 Tcl_StringMatch(ObjStr(mixinClass->object.cmdName), pattern))) {
      if (withGuards && m->clientData) {
	Tcl_Obj *l = Tcl_NewListObj(0, NULL);
	Tcl_Obj *g = (Tcl_Obj*) m->clientData;
	Tcl_ListObjAppendElement(in, l, mixinClass->object.cmdName);
	Tcl_ListObjAppendElement(in, l, XOTclGlobalObjects[GUARD_OPTION]);
	Tcl_ListObjAppendElement(in, l, g);
	Tcl_ListObjAppendElement(in, list, l);
      } else
	Tcl_ListObjAppendElement(in, list, mixinClass->object.cmdName);
    }
    m = m->next;
  }
  Tcl_SetObjResult(in, list);
  return TCL_OK;
}

static Tcl_Command
MixinSearchMethodByName(Tcl_Interp *in, XOTclCmdList* mixinList, char *name) {
  Tcl_Command cmd;
  while (mixinList) {
    XOTclClass *mcl =
      GetClass(in, (char*) Tcl_GetCommandName(in, mixinList->cmdPtr));
    if (mcl && SearchCMethod(mcl, name, &cmd))
      return cmd;

    mixinList = mixinList->next;
  }
  return 0;
}


/*
 *  Filter-Commands
 */

/*
 * The search method implements filter search order for filter
 * and instfilter: first a given name is interpreted as fully
 * qualified instproc name. If no instproc is found, a proc is
 * search with fully name. Otherwise the simple name is searched
 * on the heritage order: object (only for
 * per-object filters), class, meta-class
 */

static Tcl_Command
FilterSearch(Tcl_Interp *in, char *name, XOTclObject *startingObj, 
	     XOTclClass *startingCl) {
  Tcl_Command cmd = NULL;

  if (startingObj) {
    XOTclObjectOpt *opt = startingObj->opt;
    /*
     * the object-specific filter can also be defined on the object's
     * class, its hierarchy, or the respective instmixins; thus use the
     * object's class as start point for the class-specific search then ...
     */
    startingCl = startingObj->cl;

    /*
     * search for filters on object mixins
     */
    if (opt && opt->mixins) {
      if ((cmd = MixinSearchMethodByName(in, opt->mixins, name)))
	return cmd;
    }
  }

  /*
   * search for instfilters on instmixins
   */
  if (startingCl) {
    XOTclClassOpt* opt = startingCl->opt;
    if (opt && opt->instmixins) {
      if ((cmd = MixinSearchMethodByName(in, opt->instmixins, name)))
	return cmd;
    }
  }

  /*
   * seach for object procs that are used as filters
   */
  if (startingObj && startingObj->nsPtr) {
    if ((cmd = FindMethod(name, Tcl_Namespace_cmdTable(startingObj->nsPtr))))
      return cmd;
  }

  /*
   * ok, no filter on obj or mixins -> search class
   */
  if (startingCl) {
    if (!SearchCMethod(startingCl, name, &cmd)) {
      /*
       * If no filter is found yet -> search the meta-class
       */
      SearchCMethod(startingCl->object.cl, name, &cmd);
    }
  }
  return cmd;
}

/*
 * Filter Guards
 */

/* check a filter guard, return 1 if ok */
static int
GuardCheck(Tcl_Interp *in, ClientData clientData) {
  Tcl_Obj *guard = (Tcl_Obj*) clientData;
  int rc;
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;

  if (guard) {
    /*
     * if there are more than one filter guard for this filter
     * (i.e. they are inherited), then they are OR combined
     * -> if one check succeeds => return 1
     */

    /*fprintf(stderr, "checking guard **%s**\n", ObjStr(guard));*/

    cs->guardCount++;
    rc = checkConditionInScope(in, guard);
    cs->guardCount--;

    /*fprintf(stderr, "checking guard **%s** returned rc=%d\n",
      ObjStr(fr->content), rc);*/

    if (rc == TCL_OK) {
      /* fprintf(stderr, " +++ OK\n"); */
      return TCL_OK;
    } else if (rc == TCL_ERROR) {
      Tcl_Obj *sr = Tcl_GetObjResult(in);
      INCR_REF_COUNT(sr);

      /* fprintf(stderr, " +++ ERROR\n");*/

      XOTclVarErrMsg(in, "Guard Error: '", ObjStr(guard), "'\n\n",
		     ObjStr(sr), (char *)NULL);
      DECR_REF_COUNT(sr);
      return TCL_ERROR;
    }
  }
  /*
  fprintf(stderr, " +++ FAILED\n");
  */
  return XOTCL_CHECK_FAILED;
}

/*
static void
GuardPrint(Tcl_Interp *in, ClientData clientData) {
  Tcl_Obj *guard = (TclObj*) clientData;
  fprintf(stderr, " +++ <GUARDS> \n");
  if (guard) {
    fprintf(stderr, "   *     %s \n", ObjStr(guard));
  }
  fprintf(stderr, " +++ </GUARDS>\n");
}
*/

static void
GuardDel(XOTclCmdList* CL) {
  /*fprintf(stderr, "GuardDel %p cd = %p\n",
    CL, CL? CL->clientData : NULL);*/
  if (CL && CL->clientData) {
    DECR_REF_COUNT((Tcl_Obj*)CL->clientData);
    CL->clientData = NULL;
  }
}

XOTCLINLINE static void
GuardAdd(Tcl_Interp *in, XOTclCmdList* CL, Tcl_Obj *guard) {
  if (guard) {
    GuardDel(CL);
    if (strlen(ObjStr(guard)) != 0) {
      INCR_REF_COUNT(guard);
      CL->clientData = (ClientData) guard;
      /*fprintf(stderr,"guard added to %p cmdPtr=%p, clientData= %p\n",
	CL, CL->cmdPtr, CL->clientData);
      */
    }
  }
}
/*
static void
GuardAddList(Tcl_Interp *in, XOTclCmdList* dest, ClientData source) {
  XOTclTclObjList* s = (XOTclTclObjList*) source;
  while (s) {
    GuardAdd(in, dest, (Tcl_Obj*) s->content);
    s = s->next;
  }
} */

static int
GuardCall(XOTclObject *obj, XOTclClass *cl, Tcl_Command cmd, 
	  Tcl_Interp *in, ClientData clientData, int push) {
  int rc = TCL_OK;

  if (clientData) {
    Tcl_Obj *res = Tcl_GetObjResult(in); /* save the result */
    INCR_REF_COUNT(res);

    /* GuardPrint(in, cmdList->clientData); */
    /*
     * ok, there is a guard ... we have to push a
     * fake callframe on the tcl stack so that uplevel
     * is in sync with the XOTcl callstack, and we can uplevel
     * into the above pushed CallStack entry
     */
    if (push) {
      CallStackPush(in, obj, cl, cmd, 0, 0, XOTCL_CSC_TYPE_GUARD);
      rc = GuardCheck(in, clientData);
      CallStackPop(in);
    } else {
      rc = GuardCheck(in, clientData);
    }
    Tcl_SetObjResult(in, res);  /* restore the result */
    DECR_REF_COUNT(res);
  }

  return rc;
}

static int
GuardAddFromDefinitionList(Tcl_Interp *in, XOTclCmdList* dest,
			   XOTclObject *obj, Tcl_Command interceptorCmd,
			   XOTclCmdList* interceptorDefList) {
  XOTclCmdList* h;
  if (interceptorDefList != 0) {
    h = CmdListFindCmdInList(interceptorCmd, interceptorDefList);
    if (h) {
      GuardAdd(in, dest, (Tcl_Obj*) h->clientData);
      /*
       * 1 means we have added a guard successfully "interceptorCmd"
       */
      return 1;
    }
  }
  /*
   * 0 means we have not added a guard successfully "interceptorCmd"
   */
  return 0;
}

static void
GuardAddInheritedGuards(Tcl_Interp *in, XOTclCmdList* dest,
			     XOTclObject *obj, Tcl_Command filterCmd) {
  XOTclClasses* pl;
  int guardAdded = 0;
  XOTclObjectOpt *opt;

  /* search guards for instfilters registered on mixins */
  if (!(obj->flags & XOTCL_MIXIN_ORDER_VALID))
    MixinComputeDefined(in, obj);
  if (obj->flags & XOTCL_MIXIN_ORDER_DEFINED_AND_VALID) {
    XOTclCmdList *ml = obj->mixinOrder;
    XOTclClass *mixin;
    while (ml && ! guardAdded) {
      mixin = XOTclGetClassFromCmdPtr(ml->cmdPtr);
      if (mixin && mixin->opt) {
	guardAdded = GuardAddFromDefinitionList(in, dest, obj, filterCmd,
						mixin->opt->instfilters);
      }
      ml = ml->next;
    }
  }

  /* search per-object filters */
  opt = obj->opt;
  if (!guardAdded && opt && opt->filters) {
    guardAdded = GuardAddFromDefinitionList(in, dest, obj, filterCmd, opt->filters);
  }

  if (!guardAdded) {
    /* search per-class filters */
    for (pl = ComputeOrder(obj->cl, Super); !guardAdded && pl; pl = pl->next) {
      XOTclClassOpt* opt = pl->cl->opt;
      if (opt) {
	guardAdded = GuardAddFromDefinitionList(in, dest, obj, filterCmd,
						opt->instfilters);
      }
    }


    /*
     * if this is not a registered filter, it is an inherited filter, like:
     *   Class A
     *   A instproc f ...
     *   Class B -superclass A
     *   B instproc {{f {<guard>}}}
     *   B instfilter f
     * -> get the guard from the filter that inherits it (here B->f)
     */
    if (!guardAdded) {
      XOTclCmdList* registeredFilter =
	CmdListFindNameInList(in,(char*) Tcl_GetCommandName(in, filterCmd),
			      obj->filterOrder);
      if (registeredFilter) {
	GuardAdd(in, dest, (Tcl_Obj*) registeredFilter->clientData);
      }
    }
  }
}

static int
GuardList(Tcl_Interp *in, XOTclCmdList* frl, char *interceptorName) {
  XOTclCmdList* h;
  if (frl) {
    /* try to find simple name first */
    h = CmdListFindNameInList(in, interceptorName, frl);
    if (!h) {
      /* maybe it is a qualified name */
      Tcl_Command cmd = NSFindCommand(in, interceptorName);
      if (cmd) {
	h = CmdListFindCmdInList(cmd, frl);
      }
    }
    if (h) {
      Tcl_ResetResult(in);
      if (h->clientData) {
	Tcl_Obj *g = (Tcl_Obj*) h->clientData;
	Tcl_SetObjResult(in, g);
      }
      return TCL_OK;
    }
  }
  return XOTclVarErrMsg(in, "info (*)guard: can't find filter/mixin ",
			interceptorName, (char *)NULL);
}

/*
 * append a filter command to the 'filterList' of an obj/class
 */
static int
FilterAdd(Tcl_Interp *in, XOTclCmdList **filterList, Tcl_Obj *name,
	  XOTclObject *startingObj, XOTclClass *startingCl) {
  Tcl_Command cmd;
  int ocName; Tcl_Obj **ovName;
  Tcl_Obj *guard = NULL;
  XOTclCmdList* new;

  if (Tcl_ListObjGetElements(in, name, &ocName, &ovName) == TCL_OK && ocName > 1) {
    if (ocName == 3 && !strcmp(ObjStr(ovName[1]), XOTclGlobalStrings[GUARD_OPTION])) {
      name = ovName[0];
      guard = ovName[2];
    }
  }

  if (!(cmd = FilterSearch(in, ObjStr(name), startingObj, startingCl))) {
    if (startingObj)
      return XOTclVarErrMsg(in, "filter: can't find filterproc on: ",
			    ObjStr(startingObj->cmdName), " - proc: ",
			    ObjStr(name), (char *)NULL);
    else
      return XOTclVarErrMsg(in, "instfilter: can't find filterproc on: ",
			    ObjStr(startingCl->object.cmdName), " - proc: ",
			    ObjStr(name), (char *)NULL);
  }
  /*
  fprintf(stderr, " +++ adding filter %s \n", ObjStr(name));
  */
  new = CmdListAdd(filterList, cmd, /*noDuplicates*/ 1);

  if (guard) {
    GuardAdd(in, new, guard);
  } else {
    if (new->clientData)
      GuardDel(new);
  }

  return TCL_OK;
}

/*
 * reset the filter order cached in obj->filterOrder
 */
static void
FilterResetOrder(XOTclObject *obj) {
  CmdListRemoveList(&obj->filterOrder, GuardDel);
  obj->filterOrder = 0;
}

/*
 * search the filter in the hierarchy again with FilterSearch, e.g.
 * upon changes in the class hierarchy or mixins that carry the filter
 * command, so that we can be sure it is still reachable.
 */
static void
FilterSearchAgain(Tcl_Interp *in, XOTclCmdList **filters,
		  XOTclObject *startingObj, XOTclClass *startingCl) {
  char *simpleName;
  Tcl_Command cmd;
  XOTclCmdList* cmdList, *del;

  CmdListRemoveEpoched(filters, GuardDel);
  cmdList = *filters;
  while (cmdList) {
    simpleName = (char*) Tcl_GetCommandName(in, cmdList->cmdPtr);
    cmd = FilterSearch(in, simpleName, startingObj, startingCl);
    if (cmd == NULL) {
      del = cmdList;
      cmdList = cmdList->next;
      del = CmdListRemoveFromList(filters, del);
      CmdListDeleteCmdListEntry(del, GuardDel);
    } else {
      if (cmd != cmdList->cmdPtr)
	CmdListReplaceCmd(cmdList, cmd);
      cmdList = cmdList->next;
    }
  }
  /* some entries might be NULL now, if they are not found anymore
     -> delete those
  CmdListRemoveNulledEntries(filters, GuardDel);
  */

}

/*
 * if the class hierarchy or class filters have changed ->
 * invalidate filter entries in all dependent instances
 *
 */
static void
FilterInvalidateObjOrders(Tcl_Interp *in, XOTclClass *cl) {
  XOTclClasses *saved = cl->order, *clPtr, *savePtr;

  cl->order = 0;
  savePtr = clPtr = ComputeOrder(cl, Sub);
  cl->order = saved;

  while (clPtr != 0) {
    Tcl_HashSearch hSrch;
    Tcl_HashEntry* hPtr = &clPtr->cl->instances ?
      Tcl_FirstHashEntry(&clPtr->cl->instances, &hSrch) : 0;

    /* recalculate the commands of all instfilter registrations */
    if (clPtr->cl->opt)
      FilterSearchAgain(in, &clPtr->cl->opt->instfilters, 0, clPtr->cl);
    for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
      XOTclObject *obj = (XOTclObject*)
	Tcl_GetHashKey(&clPtr->cl->instances, hPtr);
      FilterResetOrder(obj);
      obj->flags &= ~XOTCL_FILTER_ORDER_VALID;

      /* recalculate the commands of all object filter registrations */
      if (obj->opt)
	FilterSearchAgain(in, &obj->opt->filters, obj, 0);
    }
    clPtr = clPtr->next;
  }
  XOTclFreeClasses(savePtr);
}

/*
 * from cl on down the hierarchy we remove all filters
 * the refer to "removeClass" namespace. E.g. used to
 * remove filters defined in superclass list from dependent
 * class cl
 */
static void
FilterRemoveDependentFilterCmds(XOTclClass *cl, XOTclClass *removeClass) {
  XOTclClasses *saved = cl->order, *clPtr;
  cl->order = 0;

  for (clPtr = ComputeOrder(cl, Sub); clPtr; clPtr = clPtr->next) {
    Tcl_HashSearch hSrch;
    Tcl_HashEntry* hPtr = &clPtr->cl->instances ?
      Tcl_FirstHashEntry(&clPtr->cl->instances, &hSrch) : 0;
    XOTclClassOpt* opt = clPtr->cl->opt;
    if (opt)
      CmdListRemoveNSFromList(&opt->instfilters, removeClass->nsPtr, GuardDel);

    for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
      XOTclObject *obj = (XOTclObject*)	Tcl_GetHashKey(&clPtr->cl->instances, hPtr);
      if (obj->opt) {
	CmdListRemoveNSFromList(&obj->opt->filters, removeClass->nsPtr, GuardDel);
      }
    }
  }

  XOTclFreeClasses(cl->order);
  cl->order = saved;
}

/*
 * build up a qualifier of the form <obj/cl> proc/instproc <procName>
 * if cl is not NULL, we build an instproc identifier for cl, else a proc
 * with obj
 */
static Tcl_Obj*
getFullProcQualifier(Tcl_Interp *in, CONST84 char *cmdName,
		     XOTclObject *obj, XOTclClass *cl) {
  Tcl_Obj *list = Tcl_NewListObj(0, NULL);
  Tcl_Obj *procObj = Tcl_NewStringObj(cmdName, -1);
  if (cl) {
    Tcl_ListObjAppendElement(in, list, cl->object.cmdName);
    Tcl_ListObjAppendElement(in, list, XOTclGlobalObjects[INSTPROC]);
  } else {
    Tcl_ListObjAppendElement(in, list, obj->cmdName);
    Tcl_ListObjAppendElement(in, list, XOTclGlobalObjects[PROC]);
  }
  Tcl_ListObjAppendElement(in, list, procObj);
  return list;
}

/*
 * info option for filters and instfilters
 * withGuards -> if not 0 => append guards
 * fullProcQualifiers -> if not 0 => full names with obj/class proc/instproc
 */
static int
FilterInfo(Tcl_Interp *in, XOTclCmdList* f, char *pattern,
	   int withGuards, int fullProcQualifiers) {
  CONST84 char *simpleName;
  Tcl_Obj *list = Tcl_NewListObj(0, NULL);

  /* guard lists should only have unqualified filter lists
     when withGuards is activated, fullProcQualifiers has not
     effect */
  if (withGuards) {
    fullProcQualifiers = 0;
  }

  while (f) {
    simpleName = Tcl_GetCommandName(in, f->cmdPtr);
    if (!pattern || Tcl_StringMatch(simpleName, pattern)) {
      if (withGuards && f->clientData) {
	Tcl_Obj *innerList = Tcl_NewListObj(0, NULL);
	Tcl_Obj *g = (Tcl_Obj*) f->clientData;
	Tcl_ListObjAppendElement(in, innerList,
				 Tcl_NewStringObj(simpleName, -1));
	Tcl_ListObjAppendElement(in, innerList, XOTclGlobalObjects[GUARD_OPTION]);
	Tcl_ListObjAppendElement(in, innerList, g);
	Tcl_ListObjAppendElement(in, list, innerList);
      } else {
	if (fullProcQualifiers) {
	  char *fullName = NSCmdFullName(f->cmdPtr);
          XOTclClass *fcl = GetClassFromFullName(in,fullName);
	  XOTclObject *fobj = fcl ? 0 : GetObject(in, fullName);
	  Tcl_ListObjAppendElement(in, list,
				   getFullProcQualifier(in, simpleName, fobj, fcl));
	} else {
	  Tcl_ListObjAppendElement(in, list, Tcl_NewStringObj(simpleName, -1));
	}
      }
    }
    f = f->next;
  }
  Tcl_SetObjResult(in, list);
  return TCL_OK;
}

/*
 * Appends XOTclCmdPtr* containing the filter cmds and their
 * superclass specializations to 'filterList'
 */
static void
FilterComputeOrderFullList(Tcl_Interp *in, XOTclCmdList **filters,
			   XOTclCmdList **filterList) {
  XOTclCmdList *f ;
  char *simpleName;
  XOTclClass *fCl;
  XOTclClasses *pl;

  /*
   * ensure that no epoched command is in the filters list
   */
  CmdListRemoveEpoched(filters, GuardDel);

  for (f = *filters; f; f = f->next) {
    char *fullName = NSCmdFullName(f->cmdPtr);
    simpleName = (char*) Tcl_GetCommandName(in, f->cmdPtr);
    fCl = GetClassFromFullName(in, fullName);
    CmdListAdd(filterList, f->cmdPtr, /*noDuplicates*/ 0);

    if (!fCl) {
      /* try to find the object for per-object filter */
      XOTclObject *fObj = GetObject(in, fullName);
      /* and then seek class/inherited filters */
      if (fObj)
	fCl = fObj->cl;
    }

    /* if we have a filter class -> search up the inheritance hierarchy*/
    if (fCl) {
      pl = ComputeOrder(fCl, Super);
      if (pl && pl->next) {
	/* don't search on the start class again */
	pl = pl->next;
	/* now go up the hierarchy */
	for(; pl; pl = pl->next) {
	  Tcl_Command pi = FindMethod(simpleName,
				      Tcl_Namespace_cmdTable(pl->cl->nsPtr));
	  if (pi) {
	    CmdListAdd(filterList, pi, /*noDuplicates*/ 0);
	    /*
	    fprintf(stderr, " %s::%s, ", ObjStr(pl->cl->object.cmdName), simpleName);
	    */
	  }
	}
      }
    }
  }
}

/*
 * Computes a linearized order of filter and instfilter. Then
 * duplicates in the full list and with the class inheritance list of
 * 'obj' are eliminated.
 * The precendence rule is that the last occurence makes it into the
 * final list.
 */
static void
FilterComputeOrder(Tcl_Interp *in, XOTclObject *obj) {
  XOTclCmdList *filterList = 0, *next, *checker, *newlist;
  XOTclClasses *pl;

  if (obj->filterOrder) FilterResetOrder(obj);
  /*
  fprintf(stderr, "<Filter Order obj=%s> List: ", ObjStr(obj->cmdName));
  */

  /* append instfilters registered for mixins */
  if (!(obj->flags & XOTCL_MIXIN_ORDER_VALID))
    MixinComputeDefined(in, obj);

  if (obj->flags & XOTCL_MIXIN_ORDER_DEFINED_AND_VALID) {
    XOTclCmdList *ml = obj->mixinOrder;
    XOTclClass *mixin;
    while (ml) {
      mixin = XOTclGetClassFromCmdPtr(ml->cmdPtr);
      if (mixin && mixin->opt && mixin->opt->instfilters)
	FilterComputeOrderFullList(in, &mixin->opt->instfilters, &filterList);
      ml = ml->next;
    }
  }

  /* append per-obj filters */
  if (obj->opt)
    FilterComputeOrderFullList(in, &obj->opt->filters, &filterList);

  /* append per-class filters */
  for (pl = ComputeOrder(obj->cl, Super); pl; pl=pl->next) {
    XOTclClassOpt* opt = pl->cl->opt;
    if (opt && opt->instfilters) {
      FilterComputeOrderFullList(in, &opt->instfilters, &filterList);
    }
  }

  /*
  fprintf(stderr, "\n");
  */
  /* use no duplicates & no classes of the precedence order
     on the resulting list */
  while (filterList) {
    checker = next = filterList->next;
    while (checker) {
      if (checker->cmdPtr == filterList->cmdPtr) break;
      checker = checker->next;
    }
    if (checker == 0) {
      newlist = CmdListAdd(&obj->filterOrder, filterList->cmdPtr,
			   /*noDuplicates*/ 0);
      GuardAddInheritedGuards(in, newlist, obj, filterList->cmdPtr);
      /*
      fprintf(stderr, "  Adding %s::%s,\n", filterList->cmdPtr->nsPtr->fullName, Tcl_GetCommandName(in, filterList->cmdPtr));
      */
      /*
      GuardPrint(in, newlist->clientData);
      */

    }

    CmdListDeleteCmdListEntry(filterList, GuardDel);

    filterList = next;
  }
  /*
  fprintf(stderr, "</Filter Order>\n");
  */
}

/*
 * the filter order is either
 *   DEFINED (there are filter on the instance),
 *   NONE    (there are no filter for the instance),
 *   or INVALID (a class re-strucuturing has occured, thus it is not clear
 *               whether filters are defined or not).
 * If it is INVALID FilterComputeDefined can be used to compute the order
 * and set the instance to DEFINE or NONE
 */
static void
FilterComputeDefined(Tcl_Interp *in, XOTclObject *obj) {
  FilterComputeOrder(in, obj);
  obj->flags |= XOTCL_FILTER_ORDER_VALID;
  if (obj->filterOrder)
    obj->flags |= XOTCL_FILTER_ORDER_DEFINED;
  else
    obj->flags &= ~XOTCL_FILTER_ORDER_DEFINED;
}

/*
 * push a filter stack information on this object
 */
static int
FilterStackPush(Tcl_Interp *in, XOTclObject *obj, Tcl_Obj *calledProc) {
  register XOTclFilterStack* h = NEW(XOTclFilterStack);

  h->currentCmdPtr = 0;
  h->calledProc = calledProc;
  INCR_REF_COUNT(h->calledProc);
  h->next = obj->filterStack;
  obj->filterStack = h;
  return 1;
}

/*
 * pop a filter stack information on this object
 */
static void
FilterStackPop(XOTclObject *obj) {
  register XOTclFilterStack *h = obj->filterStack;
  obj->filterStack = h->next;

  /* free stack entry */
  DECR_REF_COUNT(h->calledProc);
  FREE(XOTclFilterStack, h);
}

/*
 * walk through the filter order until the current filter is reached.
 * then use the next filter as current filter.
 *
 * precondition: obj->filterStack is not NULL
 */
static void
FilterSeekCurrent(Tcl_Interp *in, XOTclObject *obj, XOTclCmdList **cmdList) {
  Tcl_Command currentCmd = obj->filterStack->currentCmdPtr;

  assert(obj->flags & XOTCL_FILTER_ORDER_VALID);
  /* ensure that the filter order is not invalid, otherwise compute order
    FilterComputeDefined(in, obj);
  */

  *cmdList = obj->filterOrder;

  /* go forward to current class */
  while (*cmdList && currentCmd) {
    if ((*cmdList)->cmdPtr == currentCmd)
      currentCmd = 0;
    *cmdList = (*cmdList)->next;
  }
}

/*
 * seek through the filters active for "obj" and check whether cmdPtr
 * is among them
 */
XOTCLINLINE static int
FilterActiveOnObj(Tcl_Interp *in, XOTclObject *obj, Tcl_Command cmd) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  XOTclCallStackContent *bot = cs->content;
  register XOTclCallStackContent *csc = cs->top;
  while (csc > bot) {
    /* only check the callstack entries for this object &&
       only check the callstack entries for the given cmd */
    if (obj == csc->self && cmd == csc->cmdPtr &&
	csc->frameType == XOTCL_CSC_TYPE_ACTIVE_FILTER) {
      return 1;
    }
    csc--;
  }
  return 0;
}

/*
 * search through the filter list on obj and class hierarchy
 * for registration of a command ptr as filter
 *
 * returns a tcl obj list with the filter registration, like:
 * "<obj> filter <filterName>,
 * "<class> instfilter <filterName>,
 * or an empty list, if not registered
 */
static Tcl_Obj*
FilterFindReg(Tcl_Interp *in, XOTclObject *obj, Tcl_Command cmd) {
  Tcl_Obj *list = Tcl_NewListObj(0, NULL);
  XOTclClasses* pl;

  /* search per-object filters */
  if (obj->opt && CmdListFindCmdInList(cmd, obj->opt->filters)) {
    Tcl_ListObjAppendElement(in, list, obj->cmdName);
    Tcl_ListObjAppendElement(in, list, XOTclGlobalObjects[FILTER]);
    Tcl_ListObjAppendElement(in, list,
      Tcl_NewStringObj(Tcl_GetCommandName(in, cmd), -1));
    return list;
  }

  /* search per-class filters */
  for (pl = ComputeOrder(obj->cl, Super); pl; pl = pl->next) {
    XOTclClassOpt* opt = pl->cl->opt;
    if (opt && opt->instfilters) {
      if (CmdListFindCmdInList(cmd, opt->instfilters)) {
	Tcl_ListObjAppendElement(in, list, pl->cl->object.cmdName);
	Tcl_ListObjAppendElement(in, list, XOTclGlobalObjects[INSTFILTER]);
	Tcl_ListObjAppendElement(in, list,
				 Tcl_NewStringObj(Tcl_GetCommandName(in, cmd), -1));
	return list;
      }
    }
  }
  return list;
}

/*
 * before we can perform a filter dispatch, FilterSearchProc seeks the
 * current filter and the relevant calling information
 */
static Tcl_Command
FilterSearchProc(Tcl_Interp *in, XOTclObject *obj, Tcl_ObjCmdProc **proc, ClientData* cp,
		 Tcl_Command* currentCmd) {
  XOTclCmdList *cmdList;

  assert(obj);
  assert(obj->filterStack);

  *currentCmd = 0;
  *proc = 0;
  *cp = 0;

  FilterSeekCurrent(in, obj, &cmdList);
  while (cmdList) {
    if(Tcl_Command_cmdEpoch(cmdList->cmdPtr)) {
      cmdList = cmdList->next;
    } else if (FilterActiveOnObj(in, obj, cmdList->cmdPtr)) {
      /*
      fprintf(stderr, "Filter <%s> -- Active on: %s\n",
	     Tcl_GetCommandName(in, (Tcl_Command)cmdList->cmdPtr), ObjStr(obj->cmdName));
      */
      obj->filterStack->currentCmdPtr = cmdList->cmdPtr;
      FilterSeekCurrent(in, obj, &cmdList);
    } else {
      /* ok. we' ve found it */
      *proc = Tcl_Command_objProc(cmdList->cmdPtr);
      *cp   = Tcl_Command_objClientData(cmdList->cmdPtr);
      *currentCmd = cmdList->cmdPtr;
      /*
      fprintf(stderr, "FilterSearchProc - found: %s, %p\n",
	     Tcl_GetCommandName(in, (Tcl_Command)cmdList->cmdPtr), cmdList->cmdPtr);
      */
      return cmdList->cmdPtr;
    }
  }
  return 0;
}


/*
 * Search default values specified through 'parameter' on one class
 */
static int
SearchDefaultValuesOnClass(Tcl_Interp *in, XOTclObject *obj,
                           XOTclClass *cmdCl, XOTclClass *targetClass) {
  int result = TCL_OK;
  register Tcl_HashEntry* entryPtr = 0;
  Var* defaults;
  Tcl_Namespace *ns = targetClass->object.nsPtr;

  if (ns) {
    Tcl_HashTable *varTable = Tcl_Namespace_varTable(ns);
    entryPtr = Tcl_FindHashEntry(varTable, "__defaults");
  } else if (targetClass->object.varTable) {
    entryPtr = Tcl_FindHashEntry(targetClass->object.varTable, "__defaults");
  }
  if (entryPtr) {
    defaults = (Var*) Tcl_GetHashValue(entryPtr);
    /*
    fprintf(stderr, "+++ we have defaults for <%s>\n",
	    className(targetClass));
    */
    if (TclIsVarArray(defaults)) {
      Tcl_HashTable *table = defaults->value.tablePtr;
      Tcl_HashSearch hSrch;
      Tcl_HashEntry* hPtr = table ? Tcl_FirstHashEntry(table, &hSrch) : 0;

      for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
	char *varName        = Tcl_GetHashKey(table, hPtr);
	Tcl_Obj *varNameObj  = Tcl_NewStringObj(varName,-1);
	Var  *val            = (Var*)Tcl_GetHashValue(hPtr);
	INCR_REF_COUNT(varNameObj);
	if (TclIsVarScalar(val)) {
	  Tcl_Obj *oldValue;
	  oldValue = XOTclOGetInstVar2((XOTcl_Object*) obj, in, varNameObj, NULL,
				       TCL_LEAVE_ERR_MSG|TCL_PARSE_PART1);
	  /** we check whether the variable is already set.
	      if so, we do not set it again */
	  if (oldValue == NULL) {
	    char *value = ObjStr(val->value.objPtr), *v;
	    Tcl_Obj *valueObj = val->value.objPtr;
	    int doSubst = 0;
	    for (v=value; *v; v++) {
	      if (*v == '[' && doSubst == 0)
		doSubst = 1;
	      else if ((doSubst == 1 && *v == ']') || *v == '$') {
		doSubst = 2;
		break;
	      }
	    }
	    if (doSubst == 2) { /* we have to subst */
	      Tcl_Obj *ov[2];
	      int rc = CallStackPush(in, obj, cmdCl, 0, 1,
				     &varNameObj, XOTCL_CSC_TYPE_PLAIN);
	      if (rc != TCL_OK) {
		DECR_REF_COUNT(varNameObj);
		return rc;
	      }
	      ov[1] = valueObj;
	      Tcl_ResetResult(in);
	      rc = XOTcl_SubstObjCmd(NULL, in, 2, ov);
	      CallStackPop(in);
	      if (rc == TCL_OK) {
		valueObj = Tcl_GetObjResult(in);
	      } else {
		DECR_REF_COUNT(varNameObj);
		return rc;
	      }
	    }
	    INCR_REF_COUNT(valueObj);
	    result = XOTclCallMethodWithArg((ClientData)obj, in,
				       varNameObj, valueObj, 3, 0, 0);
	    DECR_REF_COUNT(valueObj);
	  }
	}
	DECR_REF_COUNT(varNameObj);
      }
    }
  }
  return result;
}

/*
 * Search default values specified through 'parameter' on
 * mixin and class hierarchy
 */
static int
SearchDefaultValues(Tcl_Interp *in, XOTclObject *obj, XOTclClass *cmdCl) {
  XOTcl_FrameDecls;
  XOTclClass *cl = obj->cl, *mixin;
  XOTclClasses *pl = 0;
  XOTclCmdList *ml = 0;
  int result = TCL_OK;

  if (!(obj->flags & XOTCL_MIXIN_ORDER_VALID))
    MixinComputeDefined(in, obj);
  if (obj->flags & XOTCL_MIXIN_ORDER_DEFINED_AND_VALID)
    ml = obj->mixinOrder;

  assert(cl);

  XOTcl_PushFrame(in, obj);

  while (ml) {
    mixin = XOTclGetClassFromCmdPtr(ml->cmdPtr);
    result = SearchDefaultValuesOnClass(in, obj, cmdCl, mixin);
    if (result != TCL_OK)
      break;
    ml = ml->next;
  }
  for (pl = ComputeOrder(cl, Super); pl; pl = pl->next) {
    result = SearchDefaultValuesOnClass(in, obj, cmdCl, pl->cl);
    if (result != TCL_OK)
      break;
  }

  XOTcl_PopFrame(in,obj);
  return result;
}

static int
ParameterSearchDefaultsMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  XOTclObject *defaultObj;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc != 2)
    return XOTclObjErrArgCnt(in, cl->object.cmdName,
                             "searchDefaults obj");
  if (GetXOTclObjectFromObj(in, objv[1], &defaultObj) != TCL_OK)
    return XOTclVarErrMsg(in, "Can't find default object ",
                          ObjStr(objv[1]), (char *)NULL);

  /*
   *  Search for default values for vars on superclasses
   */
  return SearchDefaultValues(in, defaultObj, defaultObj->cl);
}

static int
callParameterMethodWithArg(XOTclObject *obj, Tcl_Interp *in, Tcl_Obj *method, 
			   Tcl_Obj *arg, int objc, Tcl_Obj *CONST objv[], int flags) {
  XOTclClassOpt* opt = obj->cl->opt;
  Tcl_Obj *pcl = (opt && opt->parameterClass) ? opt->parameterClass :
    XOTclGlobalObjects[PARAM_CL];
  XOTclClass *paramCl;
  int result;

  if (GetXOTclClassFromObj(in,pcl,&paramCl, 1) == TCL_OK) {
    result = XOTclCallMethodWithArg((ClientData)paramCl, in,
			       method, arg, objc, objv, flags);
  }
  else
    result = XOTclVarErrMsg(in, "create: can't find parameter class", (char *)NULL);
  return result;
}


/*
 * method dispatch
 */

/* actually call a method (with assertion checking) */
static int
callProcCheck(ClientData cp, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[],
	      Tcl_Command cmd, XOTclObject *obj, XOTclClass *cl, char *methodName,
	      int frameType, int isTclProc) {
  int result = TCL_OK, callStackPushed = 0;
  CheckOptions co;

#if defined(PROFILE)
  long int startUsec, startSec;
  struct timeval trt;

  gettimeofday(&trt, NULL);
  startSec  = trt.tv_sec;
  startUsec = trt.tv_usec;
#endif
  assert(obj);

  RUNTIME_STATE(in)->callIsDestroy = 0;

  /*
  fprintf(stderr,"*** callProcCheck: cmd = %p\n",cmd);
  fprintf(stderr,
	  "cp=%p, isTclProc=%d %p %s, dispatch=%d %p, eval=%d %p, ov[0]=%p\n",
	  cp,
	  isTclProc, cmd,
	  Tcl_GetCommandName(in, cmd),
	  Tcl_Command_objProc(cmd) == XOTclObjDispatch, XOTclObjDispatch,
	  Tcl_Command_objProc(cmd) == XOTclOEvalMethod, XOTclOEvalMethod,
	  objv[0]
	  );
  */
  if (isTclProc || (Tcl_Command_objProc(cmd) == XOTclObjDispatch)) {
    /* push the xotcl info */
    if ((CallStackPush(in, obj, cl, cmd, objc,objv, frameType)) == TCL_OK)
      callStackPushed = 1;
    else {
      result = TCL_ERROR; goto finish;
    }
  }

#ifdef CALLSTACK_TRACE
  XOTclCallStackTrace(in);
#endif

  if (!isTclProc) {
    co = obj->opt ? obj->opt->checkoptions : 0;
    if (obj->teardown && (co & CHECK_INVAR) &&
	((result = AssertionCheckInvars(in, obj, methodName, co)) == TCL_ERROR)) {
	goto finish;
    }

#ifdef DISPATCH_TRACE
    printCall(in,"callProcCheck cmd", objc,objv);
    /*fprintf(stderr,"\tproc=%s\n",Tcl_GetCommandName(in,cmd));*/
#endif
    result = (*Tcl_Command_objProc(cmd))(cp, in, objc, objv);

#ifdef DISPATCH_TRACE
    printExit(in,"callProcCheck cmd", objc,objv, result);
    /*fprintf(stderr, " returnCode %d xotcl rc %d\n",
      Tcl_Interp_returnCode(in), RUNTIME_STATE(in)->returnCode);*/
#endif

    /*
      if (obj && obj->teardown && cl && !(obj->flags & XOTCL_DESTROY_CALLED)) {
      fprintf(stderr, "Obj= %s ", ObjStr(obj->cmdName));
      fprintf(stderr, "CL= %s ", ObjStr(cl->object.cmdName));
      fprintf(stderr, "method=%s\n", methodName);
      }
    */
    co = (!RUNTIME_STATE(in)->callIsDestroy) && obj->opt ? obj->opt->checkoptions : 0;
    if ((co & CHECK_INVAR) &&
	((result = AssertionCheckInvars(in, obj, methodName, co)) == TCL_ERROR)) {
      goto finish;
    }
  } else {
    /* isTclProc == 1
     * if this is a filter, check whether its guard applies,
     * if not: just step forward to the next filter
     */
    if (frameType & XOTCL_CSC_TYPE_FILTER) {
      XOTclCmdList *cmdList;
      /*
       * seek cmd in obj's filterOrder
       */
      assert(obj->flags & XOTCL_FILTER_ORDER_VALID);
      /* otherwise: FilterComputeDefined(in, obj);*/

      cmdList = obj->filterOrder;
      while (cmdList && cmdList->cmdPtr != cmd)
	cmdList = cmdList->next;

      /*
       * when it is found, check whether it has a filter guard
       */
      if (cmdList) {
	int rc = GuardCall(obj, cl, (Tcl_Command) cmdList->cmdPtr, in, 
			   cmdList->clientData, 0);
	if (rc != TCL_OK) {
	  if (rc != TCL_ERROR) {
	    /*
	     * call next, use the given objv's, not the callstack objv
	     * we may not be in a method, thus there may be wrong or
	     * no callstackobjs
	     */
	    rc = XOTclNextMethod(obj, in, cl, methodName,
				 objc, objv, /*useCallStackObjs*/ 0);
	  }
	
	  if (callStackPushed) {
	    CallStackPop(in);
	  }
	  return rc;
	}
      }
    }

    if (obj->teardown && !(obj->flags & XOTCL_DESTROY_CALLED)) {
      co = obj->opt ? obj->opt->checkoptions : 0;
      if ((co & CHECK_PRE) &&
	  (result = AssertionCheck(in, obj, cl, methodName, CHECK_PRE)) == TCL_ERROR) {
	goto finish;
      }
    }

    if (Tcl_Interp_numLevels(in) <= 2)
      RUNTIME_STATE(in)->returnCode = TCL_OK;
#ifdef DISPATCH_TRACE
    printCall(in,"callProcCheck tclCmd", objc,objv);
    fprintf(stderr,"\tproc=%s\n",Tcl_GetCommandName(in,cmd));
#endif
    result = (*Tcl_Command_objProc(cmd))(cp, in, objc, objv);

#ifdef DISPATCH_TRACE
    printExit(in,"callProcCheck tclCmd", objc,objv, result);
    /* fprintf(stderr, " returnCode %d xotcl rc %d\n",
       Tcl_Interp_returnCode(in), RUNTIME_STATE(in)->returnCode);*/
#endif
    if (Tcl_Interp_numLevels(in) <= 2 && RUNTIME_STATE(in)->returnCode == TCL_BREAK)
      result = TCL_BREAK;
    else if (result == TCL_BREAK && RUNTIME_STATE(in)->returnCode == TCL_OK)
      RUNTIME_STATE(in)->returnCode = result;

    /* we give the information whether the call has destroyed the
       object back to the caller, because after CallStackPop it
       cannot be retrieved via the call stack */
    /* if the object is destroyed -> the assertion structs's are already
       destroyed */
    co = obj->opt ? obj->opt->checkoptions : 0;
    if (!RUNTIME_STATE(in)->callIsDestroy &&  obj->teardown && (co & CHECK_POST) &&
	(result = AssertionCheck(in, obj, cl, methodName, CHECK_POST) == TCL_ERROR)) {
      goto finish;
    }
  }

 finish:
  if (callStackPushed)
    CallStackPop(in);

#if defined(PROFILE)
  if (RUNTIME_STATE(in)->callIsDestroy == 0) {
    XOTclProfileEvaluateData(in, startSec, startUsec, obj, cl, methodName);
  }
#endif

  return result;
}

static int
DoCallProcCheck(ClientData cp, ClientData cd, Tcl_Interp *in,
		int objc, Tcl_Obj *CONST objv[],
		Tcl_Command cmd, XOTclObject *obj, XOTclClass *cl, char *methodName,
		int frameType, int fromNext) {
  int isTclProc = (TclIsProc((Command *)cmd) != 0);
  int xotclCall = 0;

  if (cp) {
    if (Tcl_Command_objProc(cmd) == XOTclOEvalMethod) {
      /* fprintf(stderr,"calling oeval obj=%p %s\n", obj, ObjStr(obj->cmdName));
      */
      tclCmdClientData *tcd = (tclCmdClientData *)cp;
      tcd->obj = (XOTcl_Object*)obj;
      xotclCall = 1;
    } else if (Tcl_Command_objProc(cmd) == XOTclObjDispatch)
      xotclCall = 1;
  } else {
    xotclCall = 1;
    cp = cd;
  }
  /*
  fprintf(stderr,"*** DoCallProcCheck: cmd = %p\n",cmd);
  fprintf(stderr,
	  "DoCallProcCheck cp=%p, tclProc=%d %p %s, dispatch=%d %p, eval=%d %p, ov[0]=%p %d %d\n",
	  cp,
	  TclIsProc((Command*)cmd)!=0, cmd,
	  Tcl_GetCommandName(in, cmd),
	  Tcl_Command_objProc(cmd) == XOTclObjDispatch, XOTclObjDispatch,
	  Tcl_Command_objProc(cmd) == XOTclOEvalMethod, XOTclOEvalMethod,
	  objv[0], xotclCall, fromNext
	  );
  */
  if ((xotclCall || isTclProc) && !fromNext) {
    objc--;
    objv++;
  }
  return callProcCheck(cp, in, objc, objv, cmd, obj, cl,
		       methodName, frameType, isTclProc);
}


XOTCLINLINE static int
DoDispatch(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[], int flags) {
  register XOTclObject *obj = (XOTclObject*)cd;
  int result, mixinStackPushed = 0,
    filterStackPushed = 0,
    frameType = XOTCL_CSC_TYPE_PLAIN;
#ifdef OBJDELETION_TRACE
  Tcl_Obj *method = objv[1];
#endif
  char *methodName = ObjStr(objv[1]),
    *callMethod = methodName;
  XOTclClass *cl = 0;
  ClientData cp = 0;
  Tcl_ObjCmdProc *proc = 0;
  Tcl_Command cmd = 0;
  Tcl_Obj *cmdName = obj->cmdName;
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;


#ifdef AUTOVARS
  int isNext = isNextString(methodName);
#endif

#ifdef DISPATCH_TRACE
  printCall(in,"DISPATCH", objc,objv);
#endif

#ifdef OBJDELETION_TRACE
  if (method == XOTclGlobalObjects[CLEANUP] ||
      method == XOTclGlobalObjects[DESTROY]) {
    fprintf(stderr, "%s->%s id=%p destroyCalled=%d\n",
	    ObjStr(cmdName), methodName, obj,
	    (obj->flags & XOTCL_DESTROY_CALLED));
  }
#endif

  INCR_REF_COUNT(cmdName);

  if (!(obj->flags & XOTCL_FILTER_ORDER_VALID))
    FilterComputeDefined(in, obj);

  if (!(obj->flags & XOTCL_MIXIN_ORDER_VALID))
    MixinComputeDefined(in, obj);

#ifdef AUTOVARS
    if(!isNext) {
#endif
      /* Only start new filter chain, if
	 (a) filters are defined and
	 (b) the toplevel csc entry is not an filter on self
      */
      if (!(flags & XOTCL_CM_NO_FILTERS) && !cs->guardCount &&
	  ((obj->flags & XOTCL_FILTER_ORDER_DEFINED_AND_VALID) ==
	   XOTCL_FILTER_ORDER_DEFINED_AND_VALID)) {
	XOTclObject *self = GetSelfObj(in);
	if (obj != self ||
	    cs->top->frameType != XOTCL_CSC_TYPE_ACTIVE_FILTER) {
	
	  filterStackPushed = FilterStackPush(in, obj, objv[1]);
	  cmd = FilterSearchProc(in, obj, &proc, &cp,
				      &obj->filterStack->currentCmdPtr);
	  if (cmd) { /* 'proc' and the other output vars are set as well */
	    frameType = XOTCL_CSC_TYPE_ACTIVE_FILTER;
	    cl = GetClassFromFullName(in, NSCmdFullName(cmd));
	    callMethod = (char*) Tcl_GetCommandName(in, cmd);
	    /* RUNTIME_STATE(in)->filterCalls++; */
	  } else {
	    FilterStackPop(obj);
	    filterStackPushed = 0;
	  }
	}
      }

      /* check if a mixin is to be called.
	 don't use mixins on next method calls, since normally it is not
	 intercepted (it is used as a primitive command).
	 don't use mixins on init calls, since init is invoked on mixins
	 during mixin registration (in XOTclOMixinMethod)
      */

      if ((obj->flags & XOTCL_MIXIN_ORDER_DEFINED_AND_VALID) ==
	  XOTCL_MIXIN_ORDER_DEFINED_AND_VALID) {

	mixinStackPushed = MixinStackPush(obj);

	if (frameType != XOTCL_CSC_TYPE_ACTIVE_FILTER) {
	  cmd = MixinSearchProc(in, obj, methodName, &cl, &proc, &cp,
				     &obj->mixinStack->currentCmdPtr);
	  if (cmd) { /* 'proc' and the other output vars are set as well */
	    frameType = XOTCL_CSC_TYPE_MIXIN;
	  } else { /* the else branch could be deleted */
	     MixinStackPop(obj);
	     mixinStackPushed = 0;
	  }
	}
      }
#ifdef AUTOVARS
    }
#endif
    
    /* if no filter/mixin is found => do ordinary method lookup */
    if (proc == 0) {
      if (obj->nsPtr)
	cmd = FindMethod(methodName, Tcl_Namespace_cmdTable(obj->nsPtr));
      
      if (cmd == NULL)
	cl = SearchCMethod(obj->cl, methodName, &cmd);
      
      if (cmd) {
	proc = Tcl_Command_objProc(cmd);
	cp   = Tcl_Command_objClientData(cmd);
      } else {
	assert(cp == 0);
      }
    }
    
    if (proc) {
      result = DoCallProcCheck(cp, cd, in, objc, objv, cmd, obj, cl, 
			       callMethod, frameType, 0 /* fromNext */);
      if (result == TCL_ERROR)
	XOTclErrInProc(in, cmdName, cl?cl->object.cmdName:NULL, callMethod);

    } else if (XOTclObjectIsClass(obj) &&  (flags & XOTCL_CM_NO_UNKNOWN)) {
      Tcl_AppendResult(in, ObjStr(objv[0]), ": unable to dispatch method '",
		       callMethod, "'", 0);
      result = TCL_ERROR;
    } else {
      result = XOTCL_UNKNOWN;
    }

#ifdef DISPATCH_TRACE
    printExit(in,"DISPATCH", objc,objv, result);
    fprintf(stderr,"obj %p mixinStackPushed %d mixinStack %p\n",
	    obj, mixinStackPushed, obj->mixinStack);
#endif
    
    if (obj && !(obj->flags & XOTCL_DESTROY_CALLED)/* !isDestroyed*/) {
      if (mixinStackPushed && obj->mixinStack)
	MixinStackPop(obj);

      if (filterStackPushed && obj->filterStack)
	FilterStackPop(obj);
    }

    DECR_REF_COUNT(cmdName);
    return result;
}

static int
ObjDispatch(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[],
	    int flags) {
  int result;
  char *method;

#ifdef STACK_TRACE
  XOTclStackTrace(in);
#endif

#ifdef CALLSTACK_TRACE
  XOTclCallStackTrace(in);
#endif

  if (objc == 1) {
    Tcl_Obj *tov[2];
    tov[0] = objv[0];
    tov[1] = XOTclGlobalObjects[DEFAULTMETHOD];
    result = DoDispatch(cd, in, 2, tov, flags);
  } else {
    /* try normal dispatch */
    result = DoDispatch(cd, in, objc, objv, flags);
  }

  if (result != XOTCL_UNKNOWN) {
    return result;
  } else {
    /*
     * back off and try unknown;
     */
    XOTclObject *obj = (XOTclObject*)cd;
    DEFINE_NEW_TCL_OBJS_ON_STACK(objc+1, tov);

    /*
    fprintf(stderr,"calling unknwon for %s %s ...\n",
	    ObjStr(obj->cmdName), ObjStr(objv[1]));
    */

    tov[0] = obj->cmdName;
    tov[1] = XOTclGlobalObjects[UNKNOWN];
    if (objc>1)
      memcpy(tov+2, objv+1, sizeof(Tcl_Obj *)*(objc-1));
    /*
    fprintf(stderr,"?? %s unknown %s\n",ObjStr(obj->cmdName), ObjStr(tov[2]));
    */
    result = DoDispatch(cd, in, objc+1, tov, flags);
    FREE_TCL_OBJS_ON_STACK(tov);

    if (result != XOTCL_UNKNOWN)
      return result;
  }
  /*
   * and if that fails too, error out
   */
  method = (objc > 1) ? ObjStr(objv[1]) : XOTclGlobalStrings[DEFAULTMETHOD];
  Tcl_ResetResult(in);
  Tcl_AppendResult(in, ObjStr(objv[0]),
                   ": unable to dispatch method '", method, "'", 0);
  return TCL_ERROR;
}

#ifdef XOTCL_BYTECODE
int
XOTclDirectSelfDispatch(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  int result;
#ifdef XOTCLOBJ_TRACE
  XOTclObject *obj;
#endif
  objTrace("BEFORE SELF DISPATCH", obj);
  result =  ObjDispatch((ClientData)GetSelfObj(in), in, objc, objv, 0);
  objTrace("AFTER SELF DISPATCH", obj);
  return result;
}
#endif

int
XOTclObjDispatch(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  return ObjDispatch(cd, in, objc, objv, 0);
}

/*
 *  Proc-Creation
 */

static int
MakeProc(Tcl_Namespace* ns, XOTclAssertionStore* aStore,
	 Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
    int result, oc = objc;
    Tcl_CallFrame frame;
    Tcl_Obj *oldBody;
    char *body;
#ifdef AUTOVARS
    char *p;
#endif
    oldBody = objv[3];
    body = ObjStr(oldBody);

    objv[3] = Tcl_NewStringObj("", 0);
    INCR_REF_COUNT(objv[3]);

    Tcl_AppendStringsToObj(objv[3], "::xotcl::initProcNS\n", NULL);

#ifdef AUTOVARS
    if ((p = strstr(body, "self")) && p != body && *(p-1) != '[')
      Tcl_AppendStringsToObj(objv[3], "::set self [self]\n", NULL);
    if (strstr(body, "proc"))
      Tcl_AppendStringsToObj(objv[3], "::set proc [self proc]\n", NULL);
    if (strstr(body, "class"))
      Tcl_AppendStringsToObj(objv[3], "::set class [self class]\n", NULL);
#endif

    Tcl_AppendStringsToObj(objv[3], body, NULL);
    Tcl_PushCallFrame(in,&frame,ns,0);

    if (objc > 4) oc = 4;
    result = Tcl_ProcObjCmd(0, in, oc, objv) != TCL_OK;
    Tcl_PopCallFrame(in);

    if (objc == 6)
      AssertionAddProc(in, ObjStr(objv[1]), aStore, objv[4], objv[5]);

    DECR_REF_COUNT(objv[3]);
    objv[3] = oldBody;

    return result;
}

/*
 * List-Functions for Info
 */
static int
ListInfo(Tcl_Interp *in, int isclass) {
  Tcl_ResetResult(in);
  Tcl_AppendElement(in, "vars"); Tcl_AppendElement(in, "body");
  Tcl_AppendElement(in, "default"); Tcl_AppendElement(in, "args");
  Tcl_AppendElement(in, "procs"); Tcl_AppendElement(in, "commands");
  Tcl_AppendElement(in, "class"); Tcl_AppendElement(in, "children");
  Tcl_AppendElement(in, "filter"); Tcl_AppendElement(in, "filterguard");
  Tcl_AppendElement(in, "info");
  Tcl_AppendElement(in, "invar"); Tcl_AppendElement(in, "mixin");
  Tcl_AppendElement(in, "methods");
  Tcl_AppendElement(in, "parent");
  Tcl_AppendElement(in, "pre"); Tcl_AppendElement(in, "post");
  if (isclass) {
    Tcl_AppendElement(in, "superclass"); Tcl_AppendElement(in, "subclass");
    Tcl_AppendElement(in, "heritage"); Tcl_AppendElement(in, "instances");
    Tcl_AppendElement(in, "instcommands"); Tcl_AppendElement(in, "instprocs");
    Tcl_AppendElement(in, "instdefault"); Tcl_AppendElement(in, "instbody");
    Tcl_AppendElement(in, "instmixin");
    Tcl_AppendElement(in, "classchildren"); Tcl_AppendElement(in, "classparent");
    Tcl_AppendElement(in, "instfilter"); Tcl_AppendElement(in, "instfilterguard");
    Tcl_AppendElement(in, "instinvar");
    Tcl_AppendElement(in, "instpre"); Tcl_AppendElement(in, "instpost");
    Tcl_AppendElement(in, "parameter");
  }
  return TCL_OK;
}

XOTCLINLINE static int
noMetaChars(char *pattern) {
  register char c, *p = pattern;
  assert(pattern);
  for (c=*p; c; c = *++p) {
    if (c == '*' || c == '[') {
      return 0;
    }
  }
  return 1;
}

static int
ListKeys(Tcl_Interp *in, Tcl_HashTable *table, char *pattern) {
  Tcl_HashEntry* hPtr;
  char *key;

  if (pattern && noMetaChars(pattern)) {
    hPtr = table ? Tcl_FindHashEntry(table, pattern) : 0;
    if (hPtr) {
      key = Tcl_GetHashKey(table, hPtr);
      Tcl_SetResult(in, key, TCL_VOLATILE);
    } else {
      Tcl_SetObjResult(in, XOTclGlobalObjects[EMPTY]);
    }
  } else {
    Tcl_Obj *list = Tcl_NewListObj(0, NULL);
    Tcl_HashSearch hSrch;
    hPtr = table ? Tcl_FirstHashEntry(table, &hSrch) : 0;
    for (; hPtr; hPtr = Tcl_NextHashEntry(&hSrch)) {
      key = Tcl_GetHashKey(table, hPtr);
      if (!pattern || Tcl_StringMatch(key, pattern)) {
	Tcl_ListObjAppendElement(in, list, Tcl_NewStringObj(key,-1));
      }
    }
    Tcl_SetObjResult(in, list);
  }
  return TCL_OK;
}

static int
varExists(Tcl_Interp *in, XOTclObject *obj, char *varName) {
  XOTcl_FrameDecls;
  Var *varPtr;
  int result;
#if defined(PRE83)
  Var *arrayPtr;
#endif

  XOTcl_PushFrame(in, obj);

#if defined(PRE83)
  varPtr = TclLookupVar(in, varName, (char *) NULL, TCL_PARSE_PART1, "access",
			/*createPart1*/ 0, /*createPart2*/ 0, &arrayPtr);
#else
  varPtr = TclVarTraceExists(in, varName);
#endif
  result = ((varPtr != NULL) && !TclIsVarUndefined(varPtr));

  XOTcl_PopFrame(in, obj);

  return result;
}

static int
ListVars(Tcl_Interp *in, XOTclObject *obj, char *pattern) {
  Tcl_Obj *varlist, *okList, *element;
  int i, length;

  if (obj->nsPtr) {
    Tcl_HashTable *varTable = Tcl_Namespace_varTable(obj->nsPtr);
    ListKeys(in, varTable, pattern);
  } else {
    ListKeys(in, obj->varTable, pattern);
  }
  varlist = Tcl_GetObjResult(in);
  Tcl_ListObjLength(in, varlist, &length);
  okList = Tcl_NewListObj(0, NULL);
  for (i=0; i<length; i++) {
    Tcl_ListObjIndex(in, varlist, i, &element);
    if (varExists(in, obj, ObjStr(element))) {
      Tcl_ListObjAppendElement(in, okList, element);
    } else {
      /*fprintf(stderr,"must ignore '%s' %d\n", ObjStr(element),i);*/
      /*Tcl_ListObjReplace(in, varlist, i, 1, 0, NULL);*/
    }
  }
  Tcl_SetObjResult(in, okList);
  return TCL_OK;
}

static int
ListObjPtrHashTable(Tcl_Interp *in, Tcl_HashTable *table, char *pattern) {
  Tcl_HashEntry* hPtr;
  if (pattern && noMetaChars(pattern)) {
    XOTclObject *childobj = GetObject(in, pattern);
    hPtr = Tcl_FindHashEntry(table, (char*)childobj);
    if (hPtr) {
      Tcl_SetObjResult(in, childobj->cmdName);
    } else {
      Tcl_SetObjResult(in, XOTclGlobalObjects[EMPTY]);
    }
  } else {
    Tcl_Obj *list = Tcl_NewListObj(0, NULL);
    Tcl_HashSearch hSrch;
    hPtr = table ? Tcl_FirstHashEntry(table, &hSrch) : 0;
    for (;  hPtr; hPtr = Tcl_NextHashEntry(&hSrch)) {
      XOTclObject *obj = (XOTclObject*)Tcl_GetHashKey(table, hPtr);
      if (!pattern || Tcl_StringMatch(ObjStr(obj->cmdName), pattern)) {
	Tcl_ListObjAppendElement(in, list, obj->cmdName);
      }
    }
    Tcl_SetObjResult(in, list);
  }
  return TCL_OK;
}

static int
ListMethodKeys(Tcl_Interp *in, Tcl_HashTable *table, char *pattern,
	       int noProcs, int noCmds) {
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr = table ? Tcl_FirstHashEntry(table, &hSrch) : 0;
  for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
    char *key = Tcl_GetHashKey(table, hPtr);
    Tcl_Command cmd = (Tcl_Command)Tcl_GetHashValue(hPtr);
    Tcl_ObjCmdProc* proc = Tcl_Command_objProc(cmd);

    if (pattern && !Tcl_StringMatch(key, pattern)) continue;
    if (noCmds  && proc != RUNTIME_STATE(in)->objInterpProc) continue;
    if (noProcs && proc == RUNTIME_STATE(in)->objInterpProc) continue;
    Tcl_AppendElement(in, key);
  }
  return TCL_OK;
}

static int
ListMethods(Tcl_Interp *in, XOTclObject *obj, char *pattern,
	    int noProcs, int noCmds, int noMixins, int inContext) {
  XOTclClasses* pl;
  if (obj->nsPtr) {
    Tcl_HashTable *cmdTable = Tcl_Namespace_cmdTable(obj->nsPtr);
    ListMethodKeys(in, cmdTable, pattern, noProcs, noCmds);
  }

  if (!noMixins) {
    if (!(obj->flags & XOTCL_MIXIN_ORDER_VALID))
      MixinComputeDefined(in, obj);
    if (obj->flags & XOTCL_MIXIN_ORDER_DEFINED_AND_VALID) {
      XOTclCmdList *ml = obj->mixinOrder;
      XOTclClass *mixin;
      while (ml) {
	int guardOk = TCL_OK;
	mixin = XOTclGetClassFromCmdPtr(ml->cmdPtr);
	if (inContext) {
	  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
	  if (!cs->guardCount) {
	    guardOk = GuardCall(obj, 0, 0, in, ml->clientData, 1);
	  }
	}
	if (mixin && guardOk == TCL_OK) {
	  Tcl_HashTable *cmdTable = Tcl_Namespace_cmdTable(mixin->nsPtr);
	  ListMethodKeys(in, cmdTable, pattern, noProcs, noCmds);
	}
	ml = ml->next;
      }
    }
  }

  /* append per-class filters */
  for (pl = ComputeOrder(obj->cl, Super); pl; pl = pl->next) {
    Tcl_HashTable *cmdTable = Tcl_Namespace_cmdTable(pl->cl->nsPtr);
    ListMethodKeys(in, cmdTable, pattern, noProcs, noCmds);
  }
  return TCL_OK;
}

static int XOTclCInfoMethod(ClientData d, Tcl_Interp *h, int i, Tcl_Obj *CONST v[]);

static int
ListClass(Tcl_Interp *in, XOTclObject *obj, char *pattern,
	  int objc, Tcl_Obj *CONST objv[]) {
  if (pattern == 0) {
    Tcl_SetObjResult(in, obj->cl->object.cmdName);
    return TCL_OK;
  } else {
    int result;
    DEFINE_NEW_TCL_OBJS_ON_STACK(objc, ov);

    memcpy(ov, objv, sizeof(Tcl_Obj *)*objc);
    ov[1] = Tcl_NewStringObj("superclass", 10);
    INCR_REF_COUNT(ov[1]);
    result = XOTclCInfoMethod((ClientData)obj->cl, in, objc, ov);
    DECR_REF_COUNT(ov[1]);
    FREE_TCL_OBJS_ON_STACK(ov);
    return result;
  }
}

static int
ListSuperclasses(Tcl_Interp *in, XOTclClass *cl, char *pattern) {
  if (pattern == 0) {
    XOTclClasses* sl = cl->super;
    XOTclClasses* sc = 0;

    /*
     * reverse the list to obtain presentation order
     */

    Tcl_ResetResult(in);
    while (sc != sl) {
      XOTclClasses* nl = sl;
      while (nl->next != sc) nl = nl->next;
      Tcl_AppendElement(in, className(nl->cl));
      sc = nl;
    }
  } else {
    XOTclClass *isc = GetClass(in, pattern);
    XOTclClasses* pl;
    if (isc == 0) return XOTclErrBadVal(in, "a class", pattern);

    /*
     * search precedence to see if we're related or not
     */
    for (pl = ComputeOrder(cl, Super); pl; pl = pl->next) {
      if (pl->cl == isc) {
	Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
	break;
      }
    }
    if (pl == 0)
      Tcl_SetIntObj(Tcl_GetObjResult(in), 0);
  }
  return TCL_OK;
}

static int
ListSubclasses(Tcl_Interp *in, XOTclClass *cl, char *pattern) {
  if (pattern == 0) {
    XOTclClasses* sl = cl->sub;
    XOTclClasses* sc = 0;

    /*
     * order unimportant
     */
    Tcl_ResetResult(in);
    for (sc = sl; sc != 0; sc = sc->next)
      Tcl_AppendElement(in, className(sc->cl));
  } else {
    XOTclClass *isc = GetClass(in, pattern);
    XOTclClasses* pl;
    XOTclClasses* saved;

    if (isc == 0) return XOTclErrBadVal(in, "a class", pattern);
    saved = cl->order;
    cl->order = 0;

    /*
     * search precedence to see if we're related or not
     */
    for (pl = ComputeOrder(cl, Sub); pl; pl = pl->next) {
      if (pl->cl == isc) {
	Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
	break;
      }
    }
    if (pl == 0)
      Tcl_SetIntObj(Tcl_GetObjResult(in), 0);

    XOTclFreeClasses(cl->order);
    cl->order = saved;
  }
  return TCL_OK;
}



static int
ListHeritage(Tcl_Interp *in, XOTclClass *cl, char *pattern) {
  XOTclClasses* pl = ComputeOrder(cl, Super);
  if (pl) pl = pl->next;
  Tcl_ResetResult(in);
  for (; pl != 0; pl = pl->next) {
    char *name = className(pl->cl);
    if (pattern && !Tcl_StringMatch(name, pattern)) continue;
    Tcl_AppendElement(in, name);
  }
  return TCL_OK;
}

static Proc*
FindProc(Tcl_Interp *in, Tcl_HashTable *table, char *name) {
  Tcl_HashEntry* hPtr = table ? Tcl_FindHashEntry(table, name) : 0;
  if (hPtr) {
    Tcl_Command cmd = (Tcl_Command)Tcl_GetHashValue(hPtr);
    Tcl_ObjCmdProc *proc = Tcl_Command_objProc(cmd);
    if (proc == RUNTIME_STATE(in)->objInterpProc)
      return (Proc*) Tcl_Command_objClientData(cmd);
    else if ((Tcl_CmdProc*)proc == RUNTIME_STATE(in)->interpProc)
      return (Proc*) Tcl_Command_clientData(cmd);
  }
  return 0;
}

static int
ListProcArgs(Tcl_Interp *in, Tcl_HashTable *table, char *name) {
  Proc* proc = FindProc(in, table, name);
  if (proc) {
      CompiledLocal *args = proc->firstLocalPtr;
      Tcl_ResetResult(in);
      for (;args != NULL; args = args->nextPtr) {
          if TclIsVarArgument(args)
	      Tcl_AppendElement(in, args->name);

      }
      return TCL_OK;
  }
  return XOTclErrBadVal(in, "a tcl method name", name);
}

static int
GetProcDefault(Tcl_Interp *in, Tcl_HashTable *table,
	       char *name, char *arg, Tcl_Obj **resultObj) {
  Proc* proc = FindProc(in, table, name);
  *resultObj = 0;
  if (proc) {
    CompiledLocal *ap;
    for (ap = proc->firstLocalPtr; ap != 0; ap = ap->nextPtr) {
      if (!TclIsVarArgument(ap)) continue;
      if (strcmp(arg, ap->name) != 0) continue;
	
      if (ap->defValuePtr != NULL) {
	*resultObj = ap->defValuePtr;
	return TCL_OK;
      }
      return TCL_OK;
    }
  }
  return TCL_ERROR;
}


static int
ListProcDefault(Tcl_Interp *in, Tcl_HashTable *table,
		 char *name, char *arg, Tcl_Obj *var) {
  Tcl_Obj *defVal;
  int result;
  if (GetProcDefault(in, table, name, arg, &defVal) == TCL_OK) {
    callFrameContext ctx = {0};
    CallStackUseActiveFrames(in,&ctx);

    if (defVal != 0) {
      if (Tcl_ObjSetVar2(in, var, 0, defVal, 0) != NULL) {
	Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
	result = TCL_OK;
      } else
	result = TCL_ERROR;
    } else {
      if (Tcl_ObjSetVar2(in, var, 0, XOTclGlobalObjects[EMPTY], 0) != NULL) {
	Tcl_SetIntObj(Tcl_GetObjResult(in), 0);
	return TCL_OK;
      } else
	result = TCL_ERROR;
    }
    CallStackRestoreSavedFrames(in, &ctx);

    if (result == TCL_ERROR) {
      Tcl_ResetResult(in);
      Tcl_AppendResult(in, "couldn't store default value in variable '",
	 var, "'", (char *) 0);
    }
  } else {
    Tcl_ResetResult(in);
    Tcl_AppendResult(in, "procedure '", name,
		     "' doesn't exist or doesn't have an argument '",
		     arg, "'", (char *) 0);
    result = TCL_ERROR;
  }
  return result;
}

static int
ListProcBody(Tcl_Interp *in, Tcl_HashTable *table, char *name) {
  Proc* proc = FindProc(in, table, name);

  if (proc) {
    char *body = ObjStr(proc->bodyPtr);
    if (strncmp(body, "::xotcl::initProcNS\n",20) == 0)
      body+=20;
    Tcl_SetObjResult(in, Tcl_NewStringObj(body, -1));
    return TCL_OK;
  }
  return XOTclErrBadVal(in, "a tcl method name", name);
}

static int
ListChildren(Tcl_Interp *in, XOTclObject *obj, char *pattern, int classesOnly) {
  XOTclObject *childobj;
  Tcl_HashTable *cmdTable;
  XOTcl_FrameDecls;

  if (!obj->nsPtr) return TCL_OK;
  cmdTable = Tcl_Namespace_cmdTable(obj->nsPtr);

  if (pattern && noMetaChars(pattern)) {
    XOTcl_PushFrame(in, obj);
    if ((childobj = GetObject(in, pattern)) &&
	(!classesOnly || XOTclObjectIsClass(childobj))) {
      Tcl_SetObjResult(in, childobj->cmdName);
    } else {
      Tcl_SetObjResult(in, XOTclGlobalObjects[EMPTY]);
    }
    XOTcl_PopFrame(in,obj);
  } else {
    Tcl_Obj *list = Tcl_NewListObj(0, NULL);
    Tcl_HashSearch hSrch;
    Tcl_HashEntry* hPtr = Tcl_FirstHashEntry(cmdTable, &hSrch);
    XOTcl_PushFrame(in, obj);
    for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
      char *key = Tcl_GetHashKey(cmdTable, hPtr);
      if (!pattern || Tcl_StringMatch(key, pattern)) {
	if ((childobj = GetObject(in, key)) &&
	    (!classesOnly || XOTclObjectIsClass(childobj))) {
	  Tcl_ListObjAppendElement(in, list, childobj->cmdName);
	}
      }
    }
    XOTcl_PopFrame(in,obj);
    Tcl_SetObjResult(in, list);
  }
  return TCL_OK;
}

static int
ListParent(Tcl_Interp *in, XOTclObject *obj) {
  if (obj->id) {
    Tcl_SetResult(in, NSCmdFullName(obj->id), TCL_VOLATILE);
  }
  return TCL_OK;
}

static XOTclClass*
FindCalledClass(Tcl_Interp *in, XOTclObject *obj) {
  char *methodName = 0;
  XOTclClass *cl;
  Tcl_Command cmd = NULL;
  XOTclCallStackContent *csc = CallStackGetTopFrame(in);

  if (csc->frameType == XOTCL_CSC_TYPE_PLAIN)
    return GetSelfClass(in);

  if (csc->frameType & XOTCL_CSC_TYPE_FILTER)
    methodName = ObjStr(csc->filterStackEntry->calledProc);
  else if (csc->frameType == XOTCL_CSC_TYPE_MIXIN && obj->mixinStack)
    methodName = (char*) GetSelfProc(in);

  if (!methodName) methodName = "";

  if (obj->nsPtr)
    cmd = FindMethod(methodName, Tcl_Namespace_cmdTable(obj->nsPtr));

  if (cmd) {
    cl = 0;
  } else {
    cl = SearchCMethod(obj->cl, methodName, &cmd);
  }
  return cl;
}

/*
 * Next Primitive Handling
 */
XOTCLINLINE static void
NextSearchMethod(XOTclObject *obj, Tcl_Interp *in, XOTclCallStackContent *csc,
		 XOTclClass **cl, char **method, Tcl_ObjCmdProc **proc, Tcl_Command *cmd,
		 ClientData *cp, int* isMixinEntry, int* isFilterEntry,
		 int* endOfFilterChain, Tcl_Command* currentCmd) {
  XOTclClasses *pl = 0;
  int endOfChain = 0;
  *endOfFilterChain = 0;

  /*
   *  Next in filters
   */
  assert(obj->flags & XOTCL_FILTER_ORDER_VALID);
  /* otherwise: FilterComputeDefined(in, obj); */

  if ((obj->flags & XOTCL_FILTER_ORDER_VALID) &&
      obj->filterStack &&
      obj->filterStack->currentCmdPtr) {
    *cmd = FilterSearchProc(in, obj, proc, cp, currentCmd);
    if (*proc == 0) {
      if (csc->frameType == XOTCL_CSC_TYPE_ACTIVE_FILTER) {
	/* reset the information to the values of method, cl
	   to the values they had before calling the filters */
	*method = ObjStr(obj->filterStack->calledProc);
	endOfChain = 1;
	*endOfFilterChain = 1;
	*cl = 0;
      }
    } else {
      *method = (char*) Tcl_GetCommandName(in, *cmd);
      *cl  = GetClassFromFullName(in, NSCmdFullName(*cmd));
      *isFilterEntry = 1;
      return;
    }
  }

  /*
   *  Next in Mixins
   */
  assert(obj->flags & XOTCL_MIXIN_ORDER_VALID);
  /* otherwise: MixinComputeDefined(in, obj); */

  if ((obj->flags & XOTCL_MIXIN_ORDER_VALID) && obj->mixinStack) {
    *cmd = MixinSearchProc(in, obj, *method, cl, proc, cp, currentCmd);
    if (*proc == 0) {
      if (csc->frameType == XOTCL_CSC_TYPE_ACTIVE_MIXIN) {
	endOfChain = 1;
	*cl = 0;
      }
    } else {
      *isMixinEntry = 1;
      return;
    }
  }

  /*
   * otherwise: normal method dispatch
   *
   * if we are already in the precedence ordering, then advance
   * past our last point; otherwise (if cl==0) begin from the start
   */

  /* if a mixin or filter chain has ended -> we have to search
     the obj-specific methods as well */

  if (obj->nsPtr && endOfChain) {
    *cmd = FindMethod(*method, Tcl_Namespace_cmdTable(obj->nsPtr));
  } else {
    *cmd = 0;
  }


  if (!*cmd) {
    for (pl = ComputeOrder(obj->cl, Super); pl && *cl; pl = pl->next) {
      if (pl->cl == *cl)
	*cl = 0;
    }

    /*
     * search for a further class method
     */
    *cl = SearchPLMethod(pl, *method, cmd);
  } else {
    *cl = 0;
  }

  if (*cmd) {
    *proc = Tcl_Command_objProc(*cmd);
    *cp   = Tcl_Command_objClientData(*cmd);
  }

  return;
}

static int
XOTclNextMethod(XOTclObject *obj, Tcl_Interp *in, XOTclClass *givenCl,
		char *givenMethod, int objc, Tcl_Obj *CONST objv[],
		int useCallstackObjs) {
  XOTclCallStackContent *csc = CallStackGetTopFrame(in);
  Tcl_ObjCmdProc *proc = 0;
  Tcl_Command cmd, currentCmd = NULL;
  ClientData cp = 0;
  int result = TCL_OK,
    frameType = XOTCL_CSC_TYPE_PLAIN,
    isMixinEntry = 0, isFilterEntry = 0,
    endOfFilterChain = 0;
  int nobjc; Tcl_Obj **nobjv;
  XOTclClass **cl = &givenCl;
  char **method = &givenMethod;

  /* if no args are given => use args from stack */
  if (objc < 2 && useCallstackObjs) {
    nobjc = Tcl_CallFrame_objc(csc->currentFramePtr);
    nobjv = (Tcl_Obj **)Tcl_CallFrame_objv(csc->currentFramePtr);
  } else {
    nobjc = objc;
    nobjv = (Tcl_Obj **)objv;
  }

  /*
   * Search the next method & compute its method data
   */
  NextSearchMethod(obj, in, csc, cl, method, &proc, &cmd, &cp,
		   &isMixinEntry, &isFilterEntry, &endOfFilterChain, &currentCmd);

  /*
  fprintf(stderr, "NextSearchMethod -- RETURN: method=%s", *method);
  if (obj)
    fprintf(stderr, " obj=%s,", ObjStr(obj->cmdName));
  if ((*cl))
    fprintf(stderr, " cl=%s,", (*cl)->nsPtr->fullName);
  fprintf(stderr, " mixin=%d, filter=%d, proc=%p\n",
	  isMixinEntry, isFilterEntry, proc);
  */

  /*
   * change mixin state
   */
  if (obj->mixinStack) {
    if (csc->frameType == XOTCL_CSC_TYPE_ACTIVE_MIXIN)
      csc->frameType = XOTCL_CSC_TYPE_INACTIVE_MIXIN;

    /* otherwise move the command pointer forward */
    if (isMixinEntry) {
      frameType = XOTCL_CSC_TYPE_MIXIN;
      obj->mixinStack->currentCmdPtr = currentCmd;
    }
  }
  /*
   * change filter state
   */
  if (obj->filterStack) {
    if (csc->frameType == XOTCL_CSC_TYPE_ACTIVE_FILTER)
      csc->frameType = XOTCL_CSC_TYPE_INACTIVE_FILTER;

    /* otherwise move the command pointer forward */
    if (isFilterEntry) {
      frameType = XOTCL_CSC_TYPE_ACTIVE_FILTER;
      obj->filterStack->currentCmdPtr = currentCmd;
    }
  }

  Tcl_ResetResult(in); /* needed for bytecode support */

  /*
   * now actually call the "next" method
   */
  if (proc != 0) {
    /* cut the flag, that no stdargs should be used, if it is there */
    if (nobjc > 1) {
      char *nobjv1 = ObjStr(nobjv[1]);
      if (nobjv1[0] == '-' && !strcmp(nobjv1, "--noArgs"))
	nobjc = 1;
    }
    csc->callsNext = 1;
    result = DoCallProcCheck(cp, (ClientData)obj, in, nobjc, nobjv, cmd,
			     obj, *cl, *method, frameType, 1/*fromNext*/);
    csc->callsNext = 0;
    if (csc->frameType == XOTCL_CSC_TYPE_INACTIVE_FILTER)
      csc->frameType = XOTCL_CSC_TYPE_ACTIVE_FILTER;
    else if (csc->frameType == XOTCL_CSC_TYPE_INACTIVE_MIXIN)
      csc->frameType = XOTCL_CSC_TYPE_ACTIVE_MIXIN;
  } else if (result == TCL_OK && endOfFilterChain) {
    result = XOTCL_UNKNOWN;
  }

  return result;
}

int
XOTclNextObjCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclCallStackContent *csc = CallStackGetTopFrame(in);

  if (!csc->self)
    return XOTclVarErrMsg(in, "next: can't find self", (char *)NULL);

  if (!csc->cmdPtr)
    return XOTclErrMsg(in, "next: no executing proc", TCL_STATIC);

  return XOTclNextMethod(csc->self, in, csc->cl,
			 (char*)Tcl_GetCommandName(in, csc->cmdPtr), objc, objv, 1);
}

static int
XOTclONextMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  XOTclCallStackContent *csc = CallStackGetTopFrame(in);
  char *methodName;

  for (; csc >= cs->content; csc--) {
    if (csc->self == obj) break;
  }
  if (csc<cs->content)
    return XOTclVarErrMsg(in, "next: can't find object", ObjStr(obj->cmdName), NULL);
  methodName = (char*)Tcl_GetCommandName(in, csc->cmdPtr);
  /*fprintf(stderr,"******* next for proc %s\n", methodName);*/
  return XOTclNextMethod(obj, in, csc->cl, methodName, objc-1, &objv[1], 0);
}


/*
 * "self" object command
 */

static int
FindSelfNext(Tcl_Interp *in, XOTclObject *obj) {
  XOTclCallStackContent *csc = CallStackGetTopFrame(in);
  Tcl_ObjCmdProc *proc = 0;
  Tcl_Command cmd, currentCmd = 0;
  ClientData cp = 0;
  int isMixinEntry = 0,
    isFilterEntry = 0,
    endOfFilterChain = 0;
  XOTclClass *cl = csc->cl;
  XOTclObject *o = csc->self;
  char *methodName;

  Tcl_ResetResult(in);

  methodName = (char*) GetSelfProc(in);
  if (!methodName)
    return TCL_OK;

  NextSearchMethod(o, in, csc, &cl, &methodName, &proc, &cmd, &cp,
		   &isMixinEntry, &isFilterEntry, &endOfFilterChain, &currentCmd);

  if (cmd) {
    Tcl_SetObjResult(in, getFullProcQualifier(in, Tcl_GetCommandName(in, cmd), o, cl));
  }
  return TCL_OK;
}

/*
#define checkIsXOTclProcFrame(frame) \
  (((CallFrame*)frame)->isProcCallFrame && ((CallFrame*)frame)->procPtr && \
  ((CallFrame*)frame)->procPtr->cmdPtr && \
  !((CallFrame*)frame)->procPtr->cmdPtr->cmdEpoch && \
  ((CallFrame*)frame)->procPtr->cmdPtr->nsPtr && \
  ((CallFrame*)frame)->procPtr->cmdPtr->nsPtr->deleteProc == NSNamespaceDeleteProc)
*/


static Tcl_Obj *
computeLevelObj(Tcl_Interp *in, CallStackLevel level) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  XOTclCallStackContent *csc;
  Tcl_Obj *resultObj;

  switch (level) {
  case CALLING_LEVEL: csc = XOTclCallStackFindLastInvocation(in, 1); break;
  case ACTIVE_LEVEL:  csc = XOTclCallStackFindActiveFrame(in, 1); break;
  default: csc = NULL;
  }

  /*XOTclCallStackTrace(in);*/
  if (cs->top->currentFramePtr == Tcl_Interp_varFramePtr(in)
      && csc && csc < cs->top && csc->currentFramePtr) {
    /* this was from an xotcl frame, return absolute frame number */
    char buffer[LONG_AS_STRING];
    int l;
    buffer[0] = '#';
    XOTcl_ltoa(buffer+1,(long)Tcl_CallFrame_level(csc->currentFramePtr),&l);
    resultObj = Tcl_NewStringObj(buffer,l+1);
  } else {
    /* If not called from an xotcl frame, return 1 as default */
    resultObj = Tcl_NewIntObj(1);
  }
  return resultObj;
}

static int
XOTclSelfSubCommand(Tcl_Interp *in, XOTclObject *obj, char *option) {
  assert(option);

  if (isProcString(option)) { /* proc subcommand */
    char *procName = (char*) GetSelfProc(in);
    if (procName) {
      Tcl_SetResult(in, procName, TCL_VOLATILE);
      return TCL_OK;
    } else
      return XOTclVarErrMsg(in, "Can't find proc", (char *)NULL);
  } else if (isClassString(option)) { /* class subcommand */
    XOTclClass *cl = GetSelfClass(in);
    Tcl_SetObjResult(in, cl ? cl->object.cmdName : XOTclGlobalObjects[EMPTY]);
    return TCL_OK;
  } else {
    XOTclCallStackContent *csc = NULL;
    switch (*option) { /* other callstack information */
    case 'a':
      if (!strcmp(option, "activelevel")) {
	Tcl_SetObjResult(in, computeLevelObj(in, ACTIVE_LEVEL));
	return TCL_OK;
      }
    case 'c':
      if (!strcmp(option, "calledproc")) {
	if (!(csc = CallStackFindActiveFilter(in)))
	  return XOTclVarErrMsg(in, "self calledproc called from outside of a filter",
				NULL);
	Tcl_SetObjResult(in, csc->filterStackEntry->calledProc);
	return TCL_OK;
      } else if (!strcmp(option, "calledclass")) {
	Tcl_ResetResult(in);
	Tcl_AppendResult(in, className(FindCalledClass(in, obj)), (char*) NULL);
	return TCL_OK;
      } else if (!strcmp(option, "callingproc")) {
	csc = XOTclCallStackFindLastInvocation(in, 1);
	Tcl_SetResult(in, csc ? (char*)Tcl_GetCommandName(in, csc->cmdPtr) : "",
		      TCL_VOLATILE);
	return TCL_OK;
      } else if (!strcmp(option, "callingclass")) {
	csc = XOTclCallStackFindLastInvocation(in, 1);
	Tcl_SetObjResult(in, csc && csc->cl ? csc->cl->object.cmdName :
			 XOTclGlobalObjects[EMPTY]);
	return TCL_OK;
      } else if (!strcmp(option, "callinglevel")) {
	Tcl_SetObjResult(in, computeLevelObj(in, CALLING_LEVEL));
	return TCL_OK;
      } else if (!strcmp(option, "callingobject")) {
	csc = XOTclCallStackFindLastInvocation(in, 1);
	Tcl_SetObjResult(in, csc ? csc->self->cmdName : XOTclGlobalObjects[EMPTY]);
	return TCL_OK;
      }
    case 'f':
      if (!strcmp(option, "filterreg")) {
	if (!(csc = CallStackFindActiveFilter(in)))
	  return XOTclVarErrMsg(in, "self filterreg called from outside of a filter",
		   NULL);
	Tcl_SetObjResult(in, FilterFindReg(in, obj, GetSelfProcCmdPtr(in)));
	return TCL_OK;
      case 'n':
	if (!strcmp(option, "next"))
	  return FindSelfNext(in, obj);
      default:
	return XOTclVarErrMsg(in, "unknown option for <self>", (char *)NULL);
      }
    }
  }
  return TCL_ERROR;
}

int
XOTclGetSelfObjCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj;

  if (objc > 2)
    return XOTclVarErrMsg(in, "wrong # of args for self", (char *)NULL);

  obj = GetSelfObj(in);
  if (!obj) {
    if (objc == 2 && !strcmp(ObjStr(objv[1]),"callinglevel")) {
      Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
      return TCL_OK;
    } else {
      return XOTclVarErrMsg(in, "self: no current object", (char *)NULL);
    }
  }

  if (objc == 1) {
    Tcl_SetObjResult(in, obj->cmdName);
    return TCL_OK;
  } else {
    return XOTclSelfSubCommand(in, obj, ObjStr(objv[1]));
  }
}


/*
 * object creation & destruction
 */

/*
 * mark an obj on the existing callstack, as not destroyed
 */
static void
UndestroyObj(Tcl_Interp *in, XOTclObject *obj) {
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  XOTclCallStackContent *csc;

  /*
   * mark the object on the whole callstack as not destroyed
   */
  for (csc = &cs->content[1]; csc <= cs->top; csc++) {
    if (obj == csc->self && csc->destroyedCmd != 0) {
      /*
       * The ref count was incremented, when csc->destroyedCmd
       * was set. We revert this first before forgetting the
       * destroyedCmd.
       */
      if (Tcl_Command_refCount(csc->destroyedCmd) > 1) {
	Tcl_Command_refCount(csc->destroyedCmd)--;
	MEM_COUNT_FREE("command refCount",csc->destroyedCmd);
      }
      csc->destroyedCmd  = 0;
    }
  }
  /*
   * mark obj->flags XOTCL_DESTROY_CALLED as NOT CALLED (0)
   */
  obj->flags &= ~XOTCL_DESTROY_CALLED;
}

/*
 * bring an object into a state, as after initialization
 */
static void
CleanupDestroyObject(Tcl_Interp *in, XOTclObject *obj) {
  XOTclClass *thecls, *theobj;

  thecls = RUNTIME_STATE(in)->theClass;
  theobj = RUNTIME_STATE(in)->theObject;
  /* remove the instance, but not for ::Class/::Object */
  if (obj != &(thecls->object) && obj != &(theobj->object))
    (void)RemoveInstance(obj, obj->cl);

  if (obj->nsPtr) {
    NSCleanupNamespace(in, obj->nsPtr);
    NSDeleteChildren(in, obj->nsPtr);
  }

  if (obj->varTable) {
    TclDeleteVars(((Interp *)in), obj->varTable);
    ckfree((char*)obj->varTable);
	   /*
	     FREE(obj->varTable, obj->varTable);*/
    obj->varTable = 0;
  }

  if (obj->opt) {
    XOTclObjectOpt *opt = obj->opt;
    AssertionRemoveStore(opt->assertions);
    opt->assertions = NULL;

#ifdef XOTCL_METADATA
    XOTclMetaDataDestroy(obj);
#endif

    CmdListRemoveList(&opt->mixins, GuardDel);
    CmdListRemoveList(&opt->filters, GuardDel);
    freeObjectOpt(obj);
  }

  obj->flags &= ~XOTCL_MIXIN_ORDER_VALID;
  if (obj->mixinOrder)  MixinResetOrder(obj);
  obj->flags &= ~XOTCL_FILTER_ORDER_VALID;
  if (obj->filterOrder) FilterResetOrder(obj);
}

/*
 * do obj initialization & namespace creation
 */
static void
CleanupInitObject(Tcl_Interp *in, XOTclObject *obj,
		  XOTclClass *cl, Tcl_Namespace *namespacePtr) {
#ifdef OBJDELETION_TRACE
  fprintf(stderr,"+++ CleanupInitObject\n");
#endif
  obj->teardown = in;
  obj->nsPtr = namespacePtr;
  AddInstance(obj, cl);
  if (obj->flags & XOTCL_RECREATE) {
    obj->opt = 0;
    obj->varTable = 0;
    obj->mixinOrder = 0;
    obj->filterOrder = 0;
    obj->flags = 0;
  }
}

/*
 * physical object destroy
 */
static void
PrimitiveODestroy(ClientData cd) {
  XOTclObject *obj = (XOTclObject*)cd;
  Tcl_Interp *in;
  Tcl_Command cmd;

  assert(obj && !(obj->flags & XOTCL_DESTROYED));

  /*
   * check and latch against recurrent calls with obj->teardown
   */
  PRINTOBJ("PrimitiveODestroy", obj);

  if (!obj || !obj->teardown) return;
  in = obj->teardown;
  obj->teardown = 0;

  /*
   * Don't destroy, if the interpreter is destroyed already
   * e.g. TK calls Tcl_DeleteInterp directly, if the window is killed
   */
  if (Tcl_InterpDeleted(in)) return;
  /*
   * call and latch user destroy with obj->id if we haven't
   */
  if (!(obj->flags & XOTCL_DESTROY_CALLED)) {
    callDestroyMethod(cd, in, obj, 0);
    obj->id = 0;
  }

#ifdef OBJDELETION_TRACE
  fprintf(stderr,"  physical delete of %p id=%p destroyCalled=%d '%s'\n",
	  obj, obj->id, (obj->flags & XOTCL_DESTROY_CALLED), ObjStr(obj->cmdName));
#endif

  CleanupDestroyObject(in, obj);

  while (obj->mixinStack != NULL)
    MixinStackPop(obj);
  while (obj->filterStack != NULL)
    FilterStackPop(obj);

  cmd = Tcl_FindCommand(in, ObjStr(obj->cmdName), 0, 0);

  if (cmd != NULL)
    Tcl_Command_deleteProc(cmd) = 0;

  if (obj->nsPtr) {
    XOTcl_DeleteNamespace(in, obj->nsPtr);
    obj->nsPtr = 0;
  }

  /*fprintf(stderr, " +++ OBJ/CLS free: %s\n", ObjStr(obj->cmdName));*/

  obj->flags |= XOTCL_DESTROYED;
  objTrace("ODestroy", obj);
#if REFCOUNTED
  fprintf(stderr,"ODestroy %p flags %d rc %d destr %d dc %d\n",
	    obj, obj->flags,
	    (obj->flags & XOTCL_REFCOUNTED) != 0,
	    (obj->flags & XOTCL_DESTROYED) != 0,
	    (obj->flags & XOTCL_DESTROY_CALLED) != 0
	    );
  if (!(obj->flags & XOTCL_REFCOUNTED)) {
#endif

    DECR_REF_COUNT(obj->cmdName);

#if REFCOUNTED
  }
  fprintf(stderr,"ODestroy CALL CLEANUPO\n");
#endif
  XOTclCleanupObject(obj);
#if REFCOUNTED
  fprintf(stderr,"ODestroy DONE\n");
#endif

#if !defined(NDEBUG)
  if (obj != (XOTclObject*)RUNTIME_STATE(in)->theClass)
    checkAllInstances(in, RUNTIME_STATE(in)->theClass,0);
#endif
}

static void
PrimitiveOInit(void* mem, Tcl_Interp *in, char *name, XOTclClass *cl) {
  XOTclObject *obj = (XOTclObject*)mem;

#ifdef OBJDELETION_TRACE
  fprintf(stderr,"+++ PrimitiveOInit\n");
#endif

#ifdef XOTCLOBJ_TRACE
  fprintf(stderr, "OINIT %s = %p\n", name, obj);
#endif
  XOTclObjectRefCountIncr(obj);

  /* if the command of the obj was used before, we have to clean
   * up the callstack from set "destroyedCmd" flags
   */
  UndestroyObj(in, obj);

  if (Tcl_FindNamespace(in, name, NULL, 0)) {
    CleanupInitObject(in, obj, cl,
		      NSGetFreshNamespace(in, (ClientData)obj, name));
  } else {
    CleanupInitObject(in, obj, cl, NULL);
  }
  /*obj->flags = XOTCL_MIXIN_ORDER_VALID | XOTCL_FILTER_ORDER_VALID;*/
  obj->mixinStack = 0;
  obj->filterStack = 0;
}

/*
 * Object creation: create object name (full name) and Tcl command
 */
static XOTclObject*
PrimitiveOCreate(Tcl_Interp *in, char *name, XOTclClass *cl) {
  XOTclObject *obj = (XOTclObject*)ckalloc(sizeof(XOTclObject));
  Tcl_DString fullName, *fullNamePtr = &fullName;
  int length;
  char *fn;

#if defined(XOTCLOBJ_TRACE)
  fprintf(stderr, "CKALLOC Object %p %s\n", obj, name);
#endif
#ifdef OBJDELETION_TRACE
  fprintf(stderr,"+++ PrimitiveOCreate\n");
#endif

  memset(obj, 0, sizeof(XOTclObject));
  MEM_COUNT_ALLOC("XOTclObject/XOTclClass",obj);
  assert(obj); /* ckalloc panics, if malloc fails */

  if (*name == ':' && *(name+1) == ':') {
    fn = name;
    length = strlen(name);
  } else {
    Tcl_Namespace* currNsPtr = Tcl_GetCurrentNamespace(in);
    if (currNsPtr != Tcl_GetGlobalNamespace(in) &&
	!(currNsPtr->deleteProc == NSNamespaceDeleteProc)) {
      ALLOC_NAME_NS(fullNamePtr, currNsPtr->fullName, name);
    } else {
      ALLOC_TOP_NS(fullNamePtr, name);
    }
    fn = Tcl_DStringValue(fullNamePtr);
    length = Tcl_DStringLength(fullNamePtr);
  }

  if (!NSCheckForParent(in, fn, length)) {
    if (name != fn) {
      DSTRING_FREE(fullNamePtr);
    }
    ckfree((char*) obj);
    return 0;
  }
  obj->id = Tcl_CreateObjCommand(in, fn, XOTclObjDispatch,
				 (ClientData)obj, PrimitiveODestroy);

  PrimitiveOInit(obj, in, fn, cl);
  obj->cmdName = NewXOTclObjectObjName(obj, fn, length);
  INCR_REF_COUNT(obj->cmdName);

  objTrace("PrimitiveOCreate", obj);

  if (name != fn) {
    DSTRING_FREE(fullNamePtr);
  }
  return obj;
}

/*
 * Cleanup class: remove filters, mixins, assertions, instances ...
 * and remove class from class hierarchy
 */
static void
CleanupDestroyClass(Tcl_Interp *in, XOTclClass *cl) {
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr;
  XOTclClass *theobj = RUNTIME_STATE(in)->theObject;
  XOTclObject *obj = (XOTclObject*)cl;
  XOTclClassOpt* opt = cl->opt;

  if (opt) {
    CmdListRemoveList(&opt->instmixins, GuardDel);
    MixinInvalidateObjOrders(cl);

    CmdListRemoveList(&opt->instfilters, GuardDel);
    FilterInvalidateObjOrders(in, cl);
    /* remove dependent filters of this class from all subclasses*/
    FilterRemoveDependentFilterCmds(cl, cl);
    AssertionRemoveStore(opt->assertions);
#ifdef XOTCL_OBJECTDATA
    XOTclFreeObjectData(cl);
#endif
  }

  NSCleanupNamespace(in, cl->nsPtr);
  NSDeleteChildren(in, cl->nsPtr);

  /* reset all instances to the class ::Object, that makes no sense
     for ::Object itself */
  if (cl != theobj) {
    hPtr = &cl->instances ? Tcl_FirstHashEntry(&cl->instances, &hSrch) : 0;
    for (; hPtr != 0; hPtr = Tcl_NextHashEntry(&hSrch)) {
      XOTclObject *inst = (XOTclObject*)Tcl_GetHashKey(&cl->instances, hPtr);
      if (inst && (inst != (XOTclObject*)cl) && inst->id) {
	if (inst != &(theobj->object)) {
	  (void)RemoveInstance(inst, obj->cl);
	  AddInstance(inst, theobj);
	}
      }
    }
  }

  Tcl_DeleteHashTable(&cl->instances);
  MEM_COUNT_FREE("Tcl_InitHashTable",&cl->instances);

  if (cl->parameters) {
    DECR_REF_COUNT(cl->parameters);
  }

  if (opt) {
    if (opt->parameterClass) {
      DECR_REF_COUNT(opt->parameterClass);
    }
    freeClassOpt(cl);
  }

  /*
   * flush all caches, unlink superclasses
   */

  FlushPrecedences(cl);
  while (cl->sub) {
    XOTclClass *subClass = cl->sub->cl;
    (void)RemoveSuper(subClass, cl);
    /* if there are no more super classes add the Object
     * class as superclasses
     * -> don't do that for Object itself!
     */
    if (subClass->super == 0 && cl != theobj)
      AddSuper(subClass, theobj);
  }
  while (cl->super) (void)RemoveSuper(cl, cl->super->cl);
}

/*
 * do class initialization & namespace creation
 */
static void
CleanupInitClass(Tcl_Interp *in, XOTclClass *cl, Tcl_Namespace *namespacePtr) {
  XOTclObject *obj = (XOTclObject*)cl;

#ifdef OBJDELETION_TRACE
  fprintf(stderr,"+++ CleanupInitClass\n");
#endif

  /*
   * during init of Object and Class the theClass value is not set
   */
  /*
  if (RUNTIME_STATE(in)->theClass != 0)
    obj->type = RUNTIME_STATE(in)->theClass;
  */
  XOTclObjectSetClass(obj);

  cl->nsPtr = namespacePtr;

  cl->super = 0;
  cl->sub = 0;
  AddSuper(cl, RUNTIME_STATE(in)->theObject);
  cl->parent = RUNTIME_STATE(in)->theObject;
  cl->color = WHITE;
  cl->order = 0;
  cl->parameters = 0;

  Tcl_InitHashTable(&cl->instances, TCL_ONE_WORD_KEYS);
  MEM_COUNT_ALLOC("Tcl_InitHashTable",&cl->instances);

  cl->opt = 0;
}

/*
 * class physical destruction
 */
static void
PrimitiveCDestroy(ClientData cd) {
  XOTclClass *cl = (XOTclClass*)cd;
  XOTclObject *obj = (XOTclObject*)cd;
  Tcl_Interp *in;
  Tcl_Namespace* saved;

  /*
   * check and latch against recurrent calls with obj->teardown
   */
  if (!obj || !obj->teardown) return;
  in = obj->teardown;
  obj->teardown = 0;

  /*
   * Don't destroy, if the interpreted is destroyed already
   * e.g. TK calls Tcl_DeleteInterp directly, if Window is killed
   */
  if (Tcl_InterpDeleted(in)) return;

  /*
   * call and latch user destroy with obj->id if we haven't
   */
  if (!(obj->flags & XOTCL_DESTROY_CALLED))
    callDestroyMethod(cd, in, obj, 0);

  CleanupDestroyClass(in, cl);

  /*
   * handoff the primitive teardown
   */

  saved = cl->nsPtr;
  obj->teardown = in;

  /*
   * class object destroy + physical destroy
   */
  PrimitiveODestroy(cd);

  XOTcl_DeleteNamespace(in, saved);

  return;
}

/*
 * class init
 */
static void
PrimitiveCInit(void* mem, Tcl_Interp *in, char *name, XOTclClass *class) {
  XOTclClass *cl = (XOTclClass*)mem;

  Tcl_CallFrame frame;
  Tcl_Namespace* ns;
  char *n = name;

  /*
   * ensure that namespace is newly created during CleanupInitClass
   * ie. kill it, if it exists already
   */
  if (Tcl_PushCallFrame(in, &frame, RUNTIME_STATE(in)->XOTclClassesNS, 0)!= TCL_OK)
    return;
  ns = NSGetFreshNamespace(in, (ClientData)cl, n);
  Tcl_PopCallFrame(in);

  CleanupInitClass(in, cl, ns);
  return;
}

/*
 * class create: creation of namespace + class full name
 * calls class object creation
 */
static XOTclClass*
PrimitiveCCreate(Tcl_Interp *in, char *name, XOTclClass *class) {
  XOTclClass *cl = (XOTclClass*)ckalloc(sizeof(XOTclClass));
  Tcl_DString fullName, *fullNamePtr = &fullName;
  char *fn;
  int length;
  XOTclObject *obj = (XOTclObject*)cl;

  memset(cl, 0, sizeof(XOTclClass));

  MEM_COUNT_ALLOC("XOTclObject/XOTclClass",cl);
  /*
  fprintf(stderr, " +++ CLS alloc: %s\n", name);
  */
  if (*name == ':' && *(name+1) == ':') {
    fn = name;
    length = strlen(name);
  } else {
    Tcl_Namespace *currNs = Tcl_GetCurrentNamespace(in);
    if (currNs != Tcl_GetGlobalNamespace(in)
	&& !(currNs->deleteProc == NSNamespaceDeleteProc)) {
      ALLOC_NAME_NS(fullNamePtr, currNs->fullName, name);
    } else {
      ALLOC_TOP_NS(fullNamePtr, name);
    }
    fn = Tcl_DStringValue(fullNamePtr);
    length = Tcl_DStringLength(fullNamePtr);
  }
  /*
  fprintf(stderr,"Class alloc %p '%s'\n", cl, fn);
  */
  /* check whether Object parent NS already exists,
     otherwise: error */
  if (!NSCheckForParent(in, fn, length)) {
    if (fn != name) {
      DSTRING_FREE(fullNamePtr);
    }
    ckfree((char*) cl);
    return 0;
  }
  obj->id = Tcl_CreateObjCommand(in, fn, XOTclObjDispatch,
				 (ClientData)cl, PrimitiveCDestroy);

  PrimitiveOInit(obj, in, fn, class);
  obj->cmdName = NewXOTclObjectObjName(obj,fn,length);
  INCR_REF_COUNT(obj->cmdName);
  PrimitiveCInit(cl, in, fn+2, class);

  objTrace("PrimitiveCCreate", obj);

  if (fn != name) {
    DSTRING_FREE(fullNamePtr);
  }
  return cl;
}

/* change XOTcl class conditionally; obj must not be NULL */

XOTCLINLINE static void
changeClass(Tcl_Interp *in, XOTclObject *obj, XOTclClass *cl) {
  assert(obj);

  if (cl != obj->cl) {
    (void)RemoveInstance(obj, obj->cl);
    AddInstance(obj, cl);

    MixinComputeDefined(in, obj);
    FilterComputeDefined(in, obj);
  }
}


/*
 * Undestroy the object, reclass it, and call "cleanup" afterwards
 */
static int
doCleanup(Tcl_Interp *in, XOTclObject *newobj, XOTclObject *classobj,
	  int objc, Tcl_Obj *objv[]) {
  int destroyed = 0, result;
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;
  XOTclCallStackContent *csc;
  /*
   * we check whether the object to be re-created is destroyed or not
   */
  for (csc = &cs->content[1]; csc <= cs->top; csc++) {
    if (newobj == csc->self && csc->destroyedCmd != 0) {
      destroyed = 1; break;
    }
  }

  if (destroyed)
    UndestroyObj(in, newobj);

  /*
   *  re-create, first ensure correct class for newobj
   */

  changeClass(in, newobj, (XOTclClass*) classobj);

  /*
   * dispatch "cleanup"
   */
  result = callMethod((ClientData) newobj, in, XOTclGlobalObjects[CLEANUP], 2, 0, 0);
  return result;
}

/*
 * Std initialization:
 *   call parameter default values
 *   apply "-" methods (call "configure" with given arguments)
 *   call constructor "init", if it was not called before
 */
static int
doObjInitialization(Tcl_Interp *in, XOTclObject *obj, int objc, Tcl_Obj *objv[]) {
  int result, initArgsC = objc;

  /*
   * Search for default values of parameter on superclasses
   */
  result = callParameterMethodWithArg(obj, in, XOTclGlobalObjects[SEARCH_DEFAULTS],
				      obj->cmdName, 3, 0, 0);
  if (result != TCL_OK)
    return result;

  /* clear INIT_CALLED_FLAG */
  obj->flags &= ~XOTCL_INIT_CALLED;

  /*
   * call init methods (starting with '-')
   */
  result = callMethod((ClientData) obj, in,
		      XOTclGlobalObjects[CONFIGURE], objc, objv+2, 0);
  if (result != TCL_OK)
    return result;

  /* check, whether init was called already, and determine where the
   * configure (with '-') start (we don't send them as args to
   * "init").  */

  if (!(obj->flags & XOTCL_INIT_CALLED)) {
    int newargs;
    /*
     * Call the user-defined constructor 'init'
     */
    result = Tcl_GetIntFromObj(in,Tcl_GetObjResult(in),&newargs);
    if (result == TCL_OK && newargs+2 < objc)
      initArgsC = newargs+2;
    result = callMethod((ClientData) obj, in, XOTclGlobalObjects[INIT],
			initArgsC, objv+2, 0);
    obj->flags |= XOTCL_INIT_CALLED;
  }

  return result;
}


/*
 * experimental resolver implementation -> not used at the moment
 */
#ifdef NOT_USED
static int
XOTclResolveCmd(Tcl_Interp *in, char *name, Tcl_Namespace *contextNsPtr,
		int flags, Tcl_Command *rPtr) {

  Tcl_Namespace *nsPtr[2], *cxtNsPtr;
  char *simpleName;
  register Tcl_HashEntry *entryPtr;
  register Tcl_Command cmd;
  register int search;

  /*fprintf(stderr, "  ***%s->%s\n", contextNsPtr->fullName, name);*/

  /*
   * Find the namespace(s) that contain the command.
   */
  if ((flags & TCL_GLOBAL_ONLY) != 0) {
    cxtNsPtr = Tcl_GetGlobalNamespace(in);
  }
  else if (contextNsPtr != NULL) {
    cxtNsPtr = contextNsPtr;
  }
  else {
    cxtNsPtr = Tcl_GetCurrentNamespace(in);
  }

  TclGetNamespaceForQualName(in, name, (Namespace *) contextNsPtr,
			     flags, &nsPtr[0], &nsPtr[1], &cxtNsPtr, &simpleName);

  /*fprintf(stderr, "  ***Found %s, %s\n", nsPtr[0]->fullName, nsPtr[0]->fullName);*/

  /*
   * Look for the command in the command table of its namespace.
   * Be sure to check both possible search paths: from the specified
   * namespace context and from the global namespace.
   */

  cmd = NULL;
  for (search = 0;  (search < 2) && (cmd == NULL);  search++) {
    if ((nsPtr[search] != NULL) && (simpleName != NULL)) {
      cmdTable = Tcl_Namespace_cmdTable(nsPtr[search]);
      entryPtr = Tcl_FindHashEntry(cmdTable, simpleName);
      if (entryPtr != NULL) {
	cmd = (Tcl_Command) Tcl_GetHashValue(entryPtr);
      }
    }
  }
  if (cmd != NULL) {
    Tcl_ObjCmdProc* objProc = Tcl_Command_objProc(cmd);
    if (cxtNsPtr->deleteProc == NSNamespaceDeleteProc &&
	objProc != XOTclObjDispatch &&
	objProc != XOTclNextObjCmd &&
	objProc != XOTclGetSelfObjCmd) {

      /*
       * the cmd is defined in an XOTcl object or class namespace, but
       * not an object & not self/next -> redispatch in
       * global namespace
       */
      cmd = 0;
      nsPtr[0] = Tcl_GetGlobalNamespace(in);
      if ((nsPtr[0] != NULL) && (simpleName != NULL)) {
	cmdTable = Tcl_Namespace_cmdTable(nsPtr[0]);
	if ((entryPtr = Tcl_FindHashEntry(cmdTable, simpleName))) {
	  cmd = (Tcl_Command) Tcl_GetHashValue(entryPtr);
	}
      }

      /*
      XOTclStackTrace(in);
      XOTclCallStackTrace(in);
      */
    }
    *rPtr = cmd;
    return TCL_OK;
  }

  return TCL_CONTINUE;
}
static int
XOTclResolveVar(Tcl_Interp *interp, char *name, Tcl_Namespace *context,
 	        Tcl_ResolvedVarInfo *rPtr) {
  /*fprintf(stderr, "Resolving %s in %s\n", name, context->fullName);*/

  return TCL_CONTINUE;
}
#endif

/*
 * object method implementations
 */

static int
XOTclODestroyMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 1) return XOTclObjErrArgCnt(in, obj->cmdName, "destroy");
  PRINTOBJ("XOTclODestroyMethod", obj);

  /*
   * call instdestroy for [self]
   */
  return XOTclCallMethodWithArg((ClientData)obj->cl, in,
			   XOTclGlobalObjects[INSTDESTROY], obj->cmdName,
			   objc+2, objv+1, 0);
}

static int
XOTclOCleanupMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTclClass  *cl  = XOTclObjectToClass(obj);
  char *fn;
  Tcl_Obj *savedNameObj;

#if defined(OBJDELETION_TRACE)
  fprintf(stderr,"+++ XOTclOCleanupMethod\n");
#endif

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 1) return XOTclObjErrArgCnt(in, obj->cmdName, "cleanup");
  PRINTOBJ("XOTclOCleanupMethod", obj);

  fn = ObjStr(obj->cmdName);
  savedNameObj = obj->cmdName;
  INCR_REF_COUNT(savedNameObj);

  CleanupDestroyObject(in, obj);
  CleanupInitObject(in, obj, obj->cl, obj->nsPtr);

  if (cl) {
    CleanupDestroyClass(in, cl);
    CleanupInitClass(in, cl, cl->nsPtr);
  }

  DECR_REF_COUNT(savedNameObj);

  return TCL_OK;
}

static int
XOTclOIsClassMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd, *o;
  Tcl_Obj *className;
  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 1 || objc > 2) return XOTclObjErrArgCnt(in, obj->cmdName,
					  "isclass ?className?");

  className = (objc == 2) ? objv[1] : obj->cmdName;

  if (GetXOTclObjectFromObj(in, className, &o) == TCL_OK
      && XOTclObjectIsClass(o)) {
    Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
  } else {
    Tcl_SetIntObj(Tcl_GetObjResult(in), 0);
  }
  return TCL_OK;
}

static int
XOTclOIsObjectMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd, *o;;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 2) return XOTclObjErrArgCnt(in, obj->cmdName, "isobject <objName>");

  if (GetXOTclObjectFromObj(in, objv[1], &o) == TCL_OK) {
    Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
  } else {
    Tcl_SetIntObj(Tcl_GetObjResult(in), 0);
  }
  return TCL_OK;
}

static int
IsMetaClass(Tcl_Interp *in, XOTclClass *cl) {
  /* check if cl is a meta-class by checking is Class is a superclass of cl*/
  XOTclClasses* pl;
  if (cl == RUNTIME_STATE(in)->theClass)
    return 1;

  for (pl = ComputeOrder(cl, Super); pl; pl = pl->next) {
    if (pl->cl == RUNTIME_STATE(in)->theClass)
      return 1;
  }
  return 0;
}

static int
XOTclOIsMetaClassMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd, *o;
  Tcl_Obj *className;
  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 1 || objc > 2) return XOTclObjErrArgCnt(in, obj->cmdName,
					  "ismetaclass ?metaClassName?");

  className = (objc == 2) ? objv[1] : obj->cmdName;

  if (GetXOTclObjectFromObj(in, className, &o) == TCL_OK
      && XOTclObjectIsClass(o)
      && IsMetaClass(in, (XOTclClass*)o)) {
    Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
  } else {
    Tcl_SetIntObj(Tcl_GetObjResult(in), 0);
  }
  return TCL_OK;
}

static int
XOTclOIsTypeMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTclClass *cl;
  XOTclClasses *t;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 2) return XOTclObjErrArgCnt(in, obj->cmdName, "istype <className>");
  Tcl_ResetResult(in);

  if (obj->cl && GetXOTclClassFromObj(in,objv[1],&cl, 0) == TCL_OK) {
    if (cl == obj->cl) {
      Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
      return TCL_OK;
    }
    t = ComputeOrder(obj->cl, Super);
    while (t && t->cl && t->cl != cl) {
      t = t->next;
    }
    if (t && t->cl == cl) {
      Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
      return TCL_OK;
    }
  }
  Tcl_SetIntObj(Tcl_GetObjResult(in), 0);
  return TCL_OK;
}

static int
XOTclOIsMixinMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTclClass *cl;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 2) return XOTclObjErrArgCnt(in, obj->cmdName, "ismixin <className>");

  if (!(obj->flags & XOTCL_MIXIN_ORDER_VALID))
    MixinComputeDefined(in, obj);

  if ((obj->flags & XOTCL_MIXIN_ORDER_DEFINED_AND_VALID) &&
      GetXOTclClassFromObj(in,objv[1],&cl, 0) == TCL_OK) {
    XOTclCmdList *ml = obj->mixinOrder;
    XOTclClass *mixin;
    while (ml) {
      mixin = XOTclGetClassFromCmdPtr(ml->cmdPtr);
      if (mixin && mixin == cl) {
	Tcl_SetIntObj(Tcl_GetObjResult(in), 1);
	return TCL_OK;
      }
      ml = ml->next;
    }
  }
  Tcl_SetIntObj(Tcl_GetObjResult(in), 0);
  return TCL_OK;
}

static int
XOTclOClassMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTclClass *cl;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 2) return XOTclObjErrArgCnt(in, obj->cmdName, "class <class>");

  /*
   * allow a change to any class; type system enforces safety later
   */
  if (GetXOTclClassFromObj(in, objv[1], &cl, 1) != TCL_OK)
    return XOTclErrBadVal(in, "a class", ObjStr(objv[1]));

  changeClass(in, obj, cl);

  return TCL_OK;
}

static int
XOTclOExistsMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 2) return XOTclObjErrArgCnt(in, obj->cmdName, "exists var");

  Tcl_SetIntObj(Tcl_GetObjResult(in), varExists(in, obj, ObjStr(objv[1])));
  return TCL_OK;
}

static int
countModifiers(int objc, Tcl_Obj * CONST objv[]) {
  int i, count = 0;
  char *to;
  for (i = 2; i < objc; i++) {
    to = ObjStr(objv[i]);
    if (to[0] == '-') {
      count++;
      /* '--' stops modifiers */
      if (to[1] == '-') break;
    }
  }
  return count;
}

static int
checkForModifier(Tcl_Obj * CONST objv[], int numberModifiers, char *modifier) {
  int i;
  if (numberModifiers == 0) return 0;
  for (i = 2; i-2 < numberModifiers; i++) {
    char *ov = ObjStr(objv[i]);
    /* all start with a "-", so there must be a ov[1] */
    if (ov[1] == modifier[1] && !strcmp(ov, modifier))
      return 1;
  }
  return 0;
}

static int
XOTclOInfoMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  Tcl_Namespace *nsp = obj->nsPtr;
  char *cmd, *pattern;
  int modifiers = 0;
  XOTclObjectOpt *opt;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2)
    return XOTclObjErrArgCnt(in, obj->cmdName, "info <opt> ?args?");

  opt = obj->opt;
  cmd = ObjStr(objv[1]);
  pattern = (objc > 2) ? ObjStr(objv[2]) : 0;

  /*
   * check for "-" modifiers
   */
  if (pattern && *pattern == '-') {
    modifiers = countModifiers(objc, objv);
    pattern = (objc > 2+modifiers) ? ObjStr(objv[2+modifiers]) : 0;
  }

  switch (*cmd) {
  case 'a':
    if (isArgsString(cmd)) {
      if (objc != 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info args <proc>");
      if (nsp)
	return ListProcArgs(in, Tcl_Namespace_cmdTable(nsp), pattern);
      else
	return TCL_OK;
    }
    break;

  case 'b':
    if (!strcmp(cmd, "body")) {
      if (objc != 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info body <proc>");
      if (nsp)
	return ListProcBody(in, Tcl_Namespace_cmdTable(nsp), pattern);
      else
	return TCL_OK;
    }
    break;

  case 'c':
    if (isClassString(cmd)) {
      if (objc > 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info class ?class?");
      return ListClass(in, obj, pattern, objc, objv);
    } else if (!strcmp(cmd, "commands")) {
      if (objc > 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info commands ?pat?");
      if (nsp)
	return ListKeys(in, Tcl_Namespace_cmdTable(nsp), pattern);
      else
	return TCL_OK;
    } else if (!strcmp(cmd, "children")) {
      if (objc > 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info children ?pat?");
      return ListChildren(in, obj, pattern, 0);
    } else if (!strcmp(cmd, "check")) {
      if (objc != 2 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info check");
      return AssertionListCheckOption(in, obj);
    }
    break;

  case 'd':
    if (!strcmp(cmd, "default")) {
      if (objc != 5 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info default <proc> <arg> <var>");
      if (nsp)
	return ListProcDefault(in, Tcl_Namespace_cmdTable(nsp), pattern,
			       ObjStr(objv[3]), objv[4]);
      else
	return TCL_OK;
    }
    break;

  case 'f':
    if (!strcmp(cmd, "filter")) {
      int withGuards = 0, withOrder = 0;
      if (objc-modifiers > 3)
	return XOTclObjErrArgCnt(in, obj->cmdName,
				 "info filter ?-guards? ?-order? ?pat?");
      if (modifiers > 0) {
	withGuards = checkForModifier(objv, modifiers, "-guards");
	withOrder = checkForModifier(objv, modifiers, "-order");

	if (withGuards == 0 && withOrder == 0)
	  return XOTclVarErrMsg(in, "info filter: unknown modifier ",
				ObjStr(objv[2]), (char *)NULL);
	/*
	if (withGuards && withOrder)
	  return XOTclVarErrMsg(in, "info filter: cannot use -guards and -order together",
				ObjStr(objv[2]), (char *)NULL);
	*/
      }

      if (withOrder) {
	if (!(obj->flags & XOTCL_FILTER_ORDER_VALID))
	  FilterComputeDefined(in, obj);
	return FilterInfo(in, obj->filterOrder, pattern, withGuards, 1);
      }

      return opt ? FilterInfo(in, opt->filters, pattern, withGuards, 0) : TCL_OK;

    } else if (!strcmp(cmd, "filterguard")) {
      if (objc != 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info filterguard filter");
      return opt ? GuardList(in, opt->filters, pattern) : TCL_OK;
    }
    break;

  case 'h':
    if (!strcmp(cmd, "hasNamespace")) {
      Tcl_SetBooleanObj(Tcl_GetObjResult(in), nsp != NULL);
      return TCL_OK;
    }
    break;

  case 'i':
    if (!strcmp(cmd, "invar")) {
      if (objc != 2 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info invar");
      if (opt && opt->assertions)
	Tcl_SetObjResult(in, AssertionList(in, opt->assertions->invariants));
      return TCL_OK;
    } else if (!strcmp(cmd, "info")) {
      if (objc > 2 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info info");
      return ListInfo(in, GetXOTclClassFromObj(in,obj->cmdName,NULL,0) == TCL_OK);
    }
    break;

  case 'm':
    if (!strcmp(cmd, "mixin")) {
      int withOrder = 0, withGuards = 0;
      if (objc-modifiers > 3)
	return XOTclObjErrArgCnt(in, obj->cmdName,
				 "info mixin ?-guards? ?-order? ?class?");
      if (modifiers > 0) {
	withOrder = checkForModifier(objv, modifiers, "-order");
	withGuards = checkForModifier(objv, modifiers, "-guards");

	if (withOrder == 0 && withGuards == 0)
	  return XOTclVarErrMsg(in, "info mixin: unknown modifier . ",
				ObjStr(objv[2]), (char *)NULL);
      }

      if (withOrder) {
	if (!(obj->flags & XOTCL_MIXIN_ORDER_VALID))
	  MixinComputeDefined(in, obj);
	return MixinInfo(in, obj->mixinOrder, pattern, withGuards);
      }

      return opt ? MixinInfo(in, opt->mixins, pattern, withGuards) : TCL_OK;

    } else if (!strcmp(cmd, "mixinguard")) {
      if (objc != 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info mixinguard mixin");

      return opt ? GuardList(in, opt->mixins, pattern) : TCL_OK;
    } else if (!strcmp(cmd, "methods")) {
      int noprocs = 0, nocmds = 0, nomixins = 0, inContext = 0;
      if (objc-modifiers > 3)
	return XOTclObjErrArgCnt(in, obj->cmdName,
	 "info methods ?-noprocs? ?-nocmds? ?-nomixins? -incontext ?pat?");
      if (modifiers > 0) {
	noprocs = checkForModifier(objv, modifiers, "-noprocs");
	nocmds = checkForModifier(objv, modifiers, "-nocmds");
	nomixins = checkForModifier(objv, modifiers, "-nomixins");
	inContext = checkForModifier(objv, modifiers, "-incontext");
      }
      return ListMethods(in, obj, pattern, noprocs, nocmds, nomixins, inContext);
    }
#ifdef XOTCL_METADATA
     else  if (!strcmp(cmd, "metadata")) {
      if (objc > 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info metadata ?pat?");
      return ListKeys(in, &obj->metaData, pattern);
    }
#endif
    break;
  case 'p':
    if (!strcmp(cmd, "procs")) {
      if (objc > 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info procs ?pat?");
      if (nsp)
	return ListMethodKeys(in, Tcl_Namespace_cmdTable(nsp), pattern,
			      /*noProcs*/ 0, /*noCmds*/ 1);
      else
	return TCL_OK;
    } else if (!strcmp(cmd, "parent")) {
      if (objc > 2 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info parent");
      return ListParent(in, obj);
    } else if (!strcmp(cmd, "pre")) {
      XOTclProcAssertion* procs;
      if (objc != 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info pre <proc>");
      if (opt) {
	procs = AssertionFindProcs(opt->assertions, ObjStr(objv[2]));
	if (procs) Tcl_SetObjResult(in, AssertionList(in, procs->pre));
      }
      return TCL_OK;
    } else if (!strcmp(cmd, "post")) {
      XOTclProcAssertion* procs;
      if (objc != 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info post <proc>");
      if (opt) {
	procs = AssertionFindProcs(opt->assertions, ObjStr(objv[2]));
	if (procs) Tcl_SetObjResult(in, AssertionList(in, procs->post));
      }
      return TCL_OK;
    }
    break;
  case 'v':
    if (!strcmp(cmd, "vars")) {
      if (objc > 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, obj->cmdName, "info vars ?pat?");
      return ListVars(in, obj, pattern);
    }
    break;
  }
  return XOTclErrBadVal
    (in, "an info option (use 'info info' to list all info options)", cmd);
  return TCL_OK;
}


static int
XOTclOProcMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj * CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  char *argStr, *bdyStr, *name;
  XOTclObjectOpt *opt;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 4 && objc != 6)
    return XOTclObjErrArgCnt(in, obj->cmdName,
			     "proc name args body ?preAssertion postAssertion?");
  argStr = ObjStr(objv[2]);
  bdyStr = ObjStr(objv[3]);
  name = ObjStr(objv[1]);

  if (*argStr == 0 && *bdyStr == 0) {
    opt = obj->opt;
    if (opt)
      AssertionRemoveProc(opt->assertions, name);
    if (obj->nsPtr)
      NSDeleteCmd(in, obj->nsPtr, name);
  } else {
    opt = XOTclRequireObjectOpt(obj);
    if (!opt->assertions)
      opt->assertions = AssertionCreateStore();
    requireObjNamespace(in, obj);
    MakeProc(obj->nsPtr, opt->assertions, in, objc, (Tcl_Obj **) objv);
  }

  /* could be a filter => recompute filter order */
  FilterComputeDefined(in, obj);

  return TCL_OK;
}

static int
XOTclONoinitMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj * CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 1) return XOTclObjErrArgCnt(in, obj->cmdName, "noninit");

  obj->flags |= XOTCL_INIT_CALLED;

  return TCL_OK;
}

static int
XOTclOIncrMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj * CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTcl_FrameDecls;
  int result;

  if (!obj)
    return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2)
    return XOTclObjErrArgCnt(in, obj->cmdName, "incr <varName> ?increment?");

  XOTcl_PushFrame(in, obj);
  result = XOTcl_IncrObjCmd(cd, in, objc, objv);
  XOTcl_PopFrame(in, obj);

  return result;
}

Tcl_Obj*
XOTclOSetInstVar(XOTcl_Object *obj, Tcl_Interp *in,
		 Tcl_Obj *name, Tcl_Obj *value, int flgs) {
  return XOTclOSetInstVar2(obj, in, name, (Tcl_Obj *)NULL, value, (flgs|TCL_PARSE_PART1));
}

Tcl_Obj*
XOTclOGetInstVar(XOTcl_Object *obj, Tcl_Interp *in, Tcl_Obj *name, int flgs) {
    return XOTclOGetInstVar2(obj, in, name, (Tcl_Obj *)NULL, (flgs|TCL_PARSE_PART1));
}

int
XOTclUnsetInstVar(XOTcl_Object *obj, Tcl_Interp *in, char *name, int flgs) {
    return XOTclUnsetInstVar2 (obj, in, name,(char*)NULL, flgs);
}

extern int
XOTclCreateObject(Tcl_Interp *in, Tcl_Obj *name, XOTcl_Class *cli) {
  XOTclClass *cl = (XOTclClass*) cli;
  int result;
  INCR_REF_COUNT(name);
  result = XOTclCallMethodWithArg((ClientData)cl, in,
			     XOTclGlobalObjects[CREATE], name, 3, 0, 0);
  DECR_REF_COUNT(name);
  return result;
}

extern int
XOTclCreateClass(Tcl_Interp *in, Tcl_Obj *name, XOTcl_Class *cli) {
  XOTclClass *cl = (XOTclClass*) cli;
  int result;
  INCR_REF_COUNT(name);
  result = XOTclCallMethodWithArg((ClientData)cl, in,
			     XOTclGlobalObjects[CREATE], name, 3, 0, 0);
  DECR_REF_COUNT(name);
  return result;
}

int
XOTclDeleteObject(Tcl_Interp *in, XOTcl_Object *obji) {
  XOTclObject *obj = (XOTclObject*) obji;
  return callMethod((ClientData)obj, in, XOTclGlobalObjects[DESTROY],2,0,0);
}

int
XOTclDeleteClass(Tcl_Interp *in, XOTcl_Class *cli) {
  XOTclClass *cl = (XOTclClass*) cli;
  return callMethod((ClientData)cl, in, XOTclGlobalObjects[DESTROY],2,0,0);
}

extern Tcl_Obj*
XOTclOSetInstVar2(XOTcl_Object *obji, Tcl_Interp *in, Tcl_Obj *name1, Tcl_Obj *name2,
		  Tcl_Obj *value, int flgs) {
  XOTclObject *obj = (XOTclObject*) obji;
  Tcl_Obj *result;
  XOTcl_FrameDecls;

  XOTcl_PushFrame(in, obj);
  if (obj->nsPtr)
    flgs |= TCL_NAMESPACE_ONLY;

  result = Tcl_ObjSetVar2(in, name1, name2, value, flgs);
  XOTcl_PopFrame(in, obj);
  return result;
}

extern int
XOTclUnsetInstVar2(XOTcl_Object *obji, Tcl_Interp *in, char *name1, char *name2,
        int flgs) {
  XOTclObject *obj = (XOTclObject*) obji;
  int result;
  XOTcl_FrameDecls;

  XOTcl_PushFrame(in, obj);
  if (obj->nsPtr)
    flgs |= TCL_NAMESPACE_ONLY;

  result = Tcl_UnsetVar2(in, name1, name2, flgs);
  XOTcl_PopFrame(in, obj);
  return result;
}

/*
 * We need NewVar from tclVar.c ... but its not exported
 */
static Var *NewVar() {
  register Var *varPtr;

  varPtr = (Var *) ckalloc(sizeof(Var));
  varPtr->value.objPtr = NULL;
  varPtr->name = NULL;
  varPtr->nsPtr = NULL;
  varPtr->hPtr = NULL;
  varPtr->refCount = 0;
  varPtr->tracePtr = NULL;
  varPtr->searchPtr = NULL;
  varPtr->flags = (VAR_SCALAR | VAR_UNDEFINED | VAR_IN_HASHTABLE);
  return varPtr;
}

static void
CleanupVar(Var * varPtr, Var *arrayPtr) {
    if (TclIsVarUndefined(varPtr) && (varPtr->refCount == 0)
	    && (varPtr->tracePtr == NULL)
	    && (varPtr->flags & VAR_IN_HASHTABLE)) {
	if (varPtr->hPtr != NULL) {
	    Tcl_DeleteHashEntry(varPtr->hPtr);
	}
	ckfree((char *) varPtr);
    }
    if (arrayPtr != NULL) {
	if (TclIsVarUndefined(arrayPtr) && (arrayPtr->refCount == 0)
		&& (arrayPtr->tracePtr == NULL)
	        && (arrayPtr->flags & VAR_IN_HASHTABLE)) {
	    if (arrayPtr->hPtr != NULL) {
		Tcl_DeleteHashEntry(arrayPtr->hPtr);
	    }
	    ckfree((char *) arrayPtr);
	}
    }
}

static int
GetInstVarIntoCurrentScope(Tcl_Interp *in, XOTclObject *obj,
			   char *varName, char *newName) {
  Var *varPtr, *otherPtr = 0, *arrayPtr;
  int new;
  Tcl_CallFrame *varFramePtr;
  Tcl_HashEntry *hPtr;
  Tcl_HashTable *tablePtr;
  XOTcl_FrameDecls;

  int flgs = TCL_LEAVE_ERR_MSG |
    /* PARSE_PART1 needed for 8.0.5 */ TCL_PARSE_PART1;

  XOTcl_PushFrame(in, obj);
  if (obj->nsPtr) {
    flgs = flgs|TCL_NAMESPACE_ONLY;
  }

  otherPtr = TclLookupVar(in, varName, (char *) NULL, flgs, "define",
			  /*createPart1*/ 1, /*createPart2*/ 1, &arrayPtr);
  XOTcl_PopFrame(in, obj);

  if (otherPtr == NULL) {
    return XOTclVarErrMsg(in, "can't make instvar ", varName,
			  ": can't find variable on ", ObjStr(obj->cmdName), NULL);
  }

  /*
   * if newName == NULL -> there is no alias, use varName
   * as target link name
   */
  if (newName == NULL) {
    /*
     * Variable link into namespace cannot be an element in an array.
     * see Tcl_VariableObjCmd ...
     */
    if (arrayPtr) {
      return XOTclVarErrMsg(in, "can't make instvar ", varName,
			    " on ", ObjStr(obj->cmdName),
			    ": variable cannot be an element in an array;",
			    " use an alias or objeval.", (char *)NULL);
    }

    newName = varName;
  }

  varFramePtr = Tcl_Interp_varFramePtr(in);

  /*
   * If we are executing inside a Tcl procedure, create a local
   * variable linked to the new namespace variable "varName".
   */
  if (varFramePtr != NULL && Tcl_CallFrame_isProcCallFrame(varFramePtr)) {
    Proc *procPtr = Tcl_CallFrame_procPtr(varFramePtr);
    int localCt = procPtr->numCompiledLocals;
    CompiledLocal *localPtr = procPtr->firstLocalPtr;
    Var *localVarPtr = Tcl_CallFrame_compiledLocals(varFramePtr);
    int nameLen = strlen(newName);
    int i;

    varPtr = NULL;
    for (i = 0;  i < localCt;  i++) {    /* look in compiled locals */
      if (!TclIsVarTemporary(localPtr)) {
	char *localName = localVarPtr->name;
	if ((newName[0] == localName[0])
	    && (nameLen == localPtr->nameLength)
	    && (strcmp(newName, localName) == 0)) {
	  varPtr = localVarPtr;
	  new = 0;
	  break;
	}
      }
      localVarPtr++;
      localPtr = localPtr->nextPtr;
    }

    if (varPtr == NULL) {	/* look in frame's local var hashtable */
      tablePtr = Tcl_CallFrame_varTablePtr(varFramePtr);
      if (tablePtr == NULL) {
	tablePtr = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(tablePtr, TCL_STRING_KEYS);
	Tcl_CallFrame_varTablePtr(varFramePtr) = tablePtr;
      }
      hPtr = Tcl_CreateHashEntry(tablePtr, newName, &new);
      if (new) {
	varPtr = NewVar();
	Tcl_SetHashValue(hPtr, varPtr);
	varPtr->hPtr = hPtr;
	varPtr->nsPtr = (Namespace *)varFramePtr->nsPtr;
      } else {
	varPtr = (Var *) Tcl_GetHashValue(hPtr);
      }
    }
    /*
     * if we define an alias (newName != varName), be sure that
     * the target does not exist already
     */
    if (!new) {
      if (varPtr == otherPtr)
	return XOTclVarErrMsg(in, "can't instvar to variable itself", (char *)NULL);
      if (TclIsVarLink(varPtr)) {
	/* we try to make the same instvar again ... this is ok */
	Var *linkPtr = varPtr->value.linkPtr;
	if (linkPtr == otherPtr) {
	  return TCL_OK;
	}

	linkPtr->refCount--;
	if (TclIsVarUndefined(linkPtr)) {
	  CleanupVar(linkPtr, (Var *) NULL);
	}

	    /*
	return XOTclVarErrMsg(in, "can't instvar to link", (char *)NULL);
	    */
      } else if (!TclIsVarUndefined(varPtr)) {
	Tcl_AppendResult(in, "variable \"", newName,
			 "\" already exists", (char *)NULL);
	return TCL_ERROR;
      } else if (varPtr->tracePtr != NULL) {
	Tcl_AppendResult(in, "variable \"", newName,
			 "\" has traces: can't use for instvar", (char *) NULL);
	return TCL_ERROR;
      }
    }
    TclSetVarLink(varPtr);
    TclClearVarUndefined(varPtr);
    varPtr->value.linkPtr = otherPtr;
    otherPtr->refCount++;
  }
  return TCL_OK;
}

static int
XOTclOInstVarMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]);

extern int
XOTclInstVar(XOTcl_Object *obji, Tcl_Interp *in, char *name, char *destName) {
  XOTclObject *obj = (XOTclObject*) obji;
  int result;
  Tcl_Obj *alias = 0;
  DEFINE_NEW_TCL_OBJS_ON_STACK(2, objv);

  objv[0] = XOTclGlobalObjects[INSTVAR];
  objv[1] = Tcl_NewStringObj(name, -1);
  INCR_REF_COUNT(objv[1]);

  if (destName != 0) {
    alias = Tcl_NewStringObj(destName, -1);
    INCR_REF_COUNT(alias);
    Tcl_ListObjAppendElement(in, objv[1], alias);
  }

  result = XOTclOInstVarMethod((ClientData) obj, in, 2, objv);

  if (destName != 0) {
    DECR_REF_COUNT(alias);
  }
  DECR_REF_COUNT(objv[1]);
  FREE_TCL_OBJS_ON_STACK(objv);
  return result;
}

extern void
XOTclRemovePMethod(Tcl_Interp *in, XOTcl_Object *obji, char *nm) {
  XOTclObject *obj = (XOTclObject*) obji;
  if (obj->nsPtr)
    NSDeleteCmd(in, obj->nsPtr, nm);
}

extern void
XOTclRemoveIMethod(Tcl_Interp *in, XOTcl_Class *cli, char *nm) {
  XOTclClass *cl = (XOTclClass*) cli;
  NSDeleteCmd(in, cl->nsPtr, nm);
}

/*
 * obj/cl ClientData setter/getter
 */
extern void
XOTclSetObjClientData(XOTcl_Object *obji, ClientData data) {
  XOTclObject *obj = (XOTclObject*) obji;
  XOTclObjectOpt *opt = XOTclRequireObjectOpt(obj);
  opt->clientData = data;
}
extern ClientData
XOTclGetObjClientData(XOTcl_Object *obji) {
  XOTclObject *obj = (XOTclObject*) obji;
  return (obj && obj->opt) ? obj->opt->clientData : 0;
}
extern void
XOTclSetClassClientData(XOTcl_Class *cli, ClientData data) {
  XOTclClass *cl = (XOTclClass*) cli;
  XOTclRequireClassOpt(cl);
  cl->opt->clientData = data;
}
extern ClientData
XOTclGetClassClientData(XOTcl_Class *cli) {
  XOTclClass *cl = (XOTclClass*) cli;
  return (cl && cl->opt) ? cl->opt->clientData : 0;
}

static int
XOTclOSetMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  Tcl_Obj *result;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");

  if (objc == 2) {
    /*fprintf(stderr,"+++ read var '%s'\n", ObjStr(objv[1]));*/
    result = XOTclOGetInstVar2((XOTcl_Object*)obj, in, objv[1], NULL,
			       (TCL_LEAVE_ERR_MSG|TCL_PARSE_PART1));
  } else if (objc == 3) {
    /*fprintf(stderr,"+++ write var '%s' = '%s'\n",
      ObjStr(objv[1]),ObjStr(objv[2]));*/
    result = XOTclOSetInstVar2((XOTcl_Object*)obj, in, objv[1], NULL, objv[2],
			       (TCL_LEAVE_ERR_MSG|TCL_PARSE_PART1));
  } else
    return XOTclObjErrArgCnt(in, obj->cmdName, "set var ?value?");

  if (result) {
    Tcl_SetObjResult(in, result);
    return TCL_OK;
  } else {
    return TCL_ERROR;
  }
}

static int
XOTclSetterMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  Tcl_Obj *result;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");

  if (objc == 1)
    result = XOTclOGetInstVar2((XOTcl_Object*)obj, in, objv[0], NULL,
			       (TCL_LEAVE_ERR_MSG|TCL_PARSE_PART1));
  else if (objc == 2)
    result = XOTclOSetInstVar2((XOTcl_Object*)obj, in, objv[0], NULL, objv[1],
			       (TCL_LEAVE_ERR_MSG|TCL_PARSE_PART1));
  else
    return XOTclObjErrArgCnt(in, obj->cmdName, "<parameter> ?value?");

  if (result) {
    Tcl_SetObjResult(in, result);
    return TCL_OK;
  } else {
    return XOTclVarErrMsg(in, "Can't find result of parameter ",
			  ObjStr(objv[0]), (char *)NULL);
  }
}

static int
XOTclOUnsetMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj * CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  int i, result = TCL_ERROR;
  XOTcl_FrameDecls;
  int flgs = TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2) return XOTclObjErrArgCnt(in, obj->cmdName, "unset ?vars?");

  XOTcl_PushFrame(in, obj);

  if (obj->nsPtr)
    flgs = flgs|TCL_NAMESPACE_ONLY;

  for (i=1; i<objc; i++) {
    result =  Tcl_UnsetVar2(in, ObjStr(objv[i]), (char *) NULL, flgs);
    if (result != TCL_OK) break;
  }

  XOTcl_PopFrame(in, obj);
  return result;
}

static int
XOTclOUpvarMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj * CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  Tcl_Obj *frameInfoObj = NULL;
  int i, result = TCL_ERROR;
  char *frameInfo;
  callFrameContext ctx = {0};

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2) return XOTclObjErrArgCnt(in, obj->cmdName,
	"?level? otherVar localVar ?otherVar localVar ...?");

  if (objc % 2 == 0) {
    frameInfo = ObjStr(objv[1]);
    i = 2;
  } else {
    frameInfoObj = computeLevelObj(in, CALLING_LEVEL);
    INCR_REF_COUNT(frameInfoObj);
    frameInfo = ObjStr(frameInfoObj);
    i = 1;
  }

  if (obj && (obj->filterStack || obj->mixinStack)) {
    CallStackUseActiveFrames(in, &ctx);
  }

  for ( ;  i < objc;  i += 2) {
    result = Tcl_UpVar2(in, frameInfo, ObjStr(objv[i]), NULL,
			ObjStr(objv[i+1]), 0 /*flags*/);
    if (result != TCL_OK)
      break;
  }

  if (frameInfoObj) {
    DECR_REF_COUNT(frameInfoObj);
  }
  CallStackRestoreSavedFrames(in, &ctx);
  return result;
}

static int
XOTclOUplevelMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj * CONST objv[]) {
  XOTclObject *obj = (XOTclObject *)cd;
  int i, result = TCL_ERROR;
  char *frameInfo = NULL;
  Tcl_CallFrame *framePtr = NULL, *savedVarFramePtr;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2) {
  uplevelSyntax:
    return XOTclObjErrArgCnt(in, obj->cmdName, "?level? command ?arg ...?");
  }
  /*
   * Find the level to use for executing the command.
   */
  if (objc>2) {
    CallFrame *cf;
    frameInfo = ObjStr(objv[1]);
    result = TclGetFrame(in, frameInfo, &cf);
    if (result == -1) {
      return TCL_ERROR;
    }
    framePtr = (Tcl_CallFrame *)cf;
    i = result+1;
  } else {
    i = 1;
  }

  objc -= i;
  objv += i;
  if (objc == 0) {
    goto uplevelSyntax;
  }

  if (!framePtr) {
    XOTclCallStackContent *csc = XOTclCallStackFindLastInvocation(in, 1);
    framePtr = csc->currentFramePtr;
  }

  savedVarFramePtr = Tcl_Interp_varFramePtr(in);
  Tcl_Interp_varFramePtr(in) = framePtr;

  /*
   * Execute the residual arguments as a command.
   */

  if (objc == 1) {
    result = Tcl_EvalObjEx(in, objv[0], TCL_EVAL_DIRECT);
  } else {
    /*
     * More than one argument: concatenate them together with spaces
     * between, then evaluate the result.  Tcl_EvalObjEx will delete
     * the object when it decrements its refcount after eval'ing it.
     */
    Tcl_Obj *objPtr = Tcl_ConcatObj(objc, objv);
    result = Tcl_EvalObjEx(in, objPtr, TCL_EVAL_DIRECT);
  }
  if (result == TCL_ERROR) {
    char msg[32 + TCL_INTEGER_SPACE];
    sprintf(msg, "\n    (\"uplevel\" body line %d)", in->errorLine);
    Tcl_AddObjErrorInfo(in, msg, -1);
  }

  /*
   * Restore the variable frame, and return.
   */

  Tcl_Interp_varFramePtr(in) = savedVarFramePtr;
  return result;
}


static int
XOTclOEvalMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj * CONST objv[]) {
  tclCmdClientData *tcd = (tclCmdClientData *)cd;
  XOTcl_FrameDecls;
  int result;
  DEFINE_NEW_TCL_OBJS_ON_STACK(objc, ov);

  if (!tcd || !tcd->obj) return XOTclObjErrType(in, objv[0], "Object");

  XOTcl_PushFrame(in, tcd->obj);
  /*
  fprintf(stderr,"ovalmethod oc=%d tcd=%p cmdname=%s obj=%s\n",
	  objc,tcd,ObjStr(tcd->cmdName), ObjStr(tcd->obj->cmdName));
  */
  ov[0] = tcd->cmdName;
  memcpy(ov+1, objv+1, sizeof(Tcl_Obj *)*(objc-1));
  result = Tcl_EvalObjv(in, objc, ov, 0);

  XOTcl_PopFrame(in, tcd->obj);
  FREE_TCL_OBJS_ON_STACK(ov);
  return result;
}

static int
XOTclOInstVarMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject  *obj   = (XOTclObject*)cd;
  Tcl_Obj      **ov;
  int          i, oc, result = TCL_OK;
  callFrameContext ctx = {0};

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2) return XOTclObjErrArgCnt(in, obj->cmdName, "instvar ?vars?");

  if (obj && (obj->filterStack || obj->mixinStack) ) {
    CallStackUseActiveFrames(in, &ctx);
  }
  if (!Tcl_Interp_varFramePtr(in)) {
    CallStackRestoreSavedFrames(in, &ctx);
    return XOTclVarErrMsg(in, "instvar used on ", ObjStr(obj->cmdName),
			  ", but callstack is not in procedure scope", NULL);
  }

  for (i=1; i<objc; i++) {
    if ((result = Tcl_ListObjGetElements(in, objv[i], &oc, &ov)) == TCL_OK) {
      char *alias = 0;
      if (oc == 2)
	alias = ObjStr(ov[1]);
      result = GetInstVarIntoCurrentScope(in, obj, ObjStr(ov[0]), alias);
      if (result != TCL_OK)
	break;
    } else
      break;
  }
  CallStackRestoreSavedFrames(in, &ctx);
  return result;
}

/*
 * copied from Tcl, since not exported
 */
static char *
VwaitVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    int *donePtr = (int *) clientData;

    *donePtr = 1;
    return (char *) NULL;
}
static int
XOTclOVwaitMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  int done, foundEvent;
  char *nameString;
  int flgs = TCL_TRACE_WRITES|TCL_TRACE_UNSETS;
  XOTcl_FrameDecls;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 2)
      return XOTclObjErrArgCnt(in, obj->cmdName, "vwait varname");

  nameString = ObjStr(objv[1]);

  /*
   * Make sure the var table exists and the varname is in there
   */
  if (NSRequireVariableOnObj(in, obj, nameString, flgs) == 0)
    return XOTclVarErrMsg(in, "Can't lookup (and create) variable ",
			  nameString, " on ", ObjStr(obj->cmdName), NULL);

  XOTcl_PushFrame(in, obj);
  /*
   * much of this is copied from Tcl, since we must avoid
   * access with flag TCL_GLOBAL_ONLY ... doesn't work on
   * obj->varTable vars
   */
  if (Tcl_TraceVar(in, nameString, flgs, (Tcl_VarTraceProc *)VwaitVarProc,
		   (ClientData) &done) != TCL_OK) {
    return TCL_ERROR;
  }
  done = 0;
  foundEvent = 1;
  while (!done && foundEvent) {
    foundEvent = Tcl_DoOneEvent(TCL_ALL_EVENTS);
  }
  Tcl_UntraceVar(in, nameString, flgs, (Tcl_VarTraceProc *)VwaitVarProc,
		 (ClientData) &done);
  XOTcl_PopFrame(in, obj);
  /*
   * Clear out the interpreter's result, since it may have been set
   * by event handlers.
   */
  Tcl_ResetResult(in);

  if (!foundEvent) {
    Tcl_AppendResult(in, "can't wait for variable \"", nameString,
		     "\":  would wait forever", (char *) NULL);
    return TCL_ERROR;
  }
  return TCL_OK;
}

static int
XOTclOInvariantsMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTclObjectOpt *opt;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 2)
      return XOTclObjErrArgCnt(in, obj->cmdName, "invar <invariantList>");

  opt = XOTclRequireObjectOpt(obj);

  if (opt->assertions)
    TclObjListFreeList(opt->assertions->invariants);
  else
    opt->assertions = AssertionCreateStore();

  opt->assertions->invariants = AssertionNewList(in, objv[1]);
  return TCL_OK;
}

static int
XOTclOAutonameMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  int instanceOpt = 0, resetOpt = 0;
  Tcl_Obj *autoname;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc == 3) {
    instanceOpt = (strcmp(ObjStr(objv[1]), "-instance") == 0);
    resetOpt = (strcmp(ObjStr(objv[1]), "-reset") == 0);
  }
  if ((objc < 2 || objc > 3) || (objc == 3 && !instanceOpt && !resetOpt))
    return XOTclObjErrArgCnt(in, obj->cmdName, "autoname [-instance | -reset] name");

  autoname = AutonameIncr(in, objv[objc-1], obj, instanceOpt, resetOpt);
  if (autoname) {
    Tcl_SetObjResult(in, autoname);
    DECR_REF_COUNT(autoname);
  }
  else
    return XOTclVarErrMsg(in,
			  "Autoname failed. Probably format string (with %) was not well-formed",
			  NULL);

  return TCL_OK;
}

static int
XOTclOCheckMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  int ocArgs; Tcl_Obj **ovArgs;
  int i;
  XOTclObjectOpt *opt;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 2)
    return XOTclObjErrArgCnt(in, obj->cmdName,
			     "check (?all? ?pre? ?post? ?invar? ?instinvar?)");

  opt = XOTclRequireObjectOpt(obj);
  opt->checkoptions = CHECK_NONE;

  if (Tcl_ListObjGetElements(in, objv[1], &ocArgs, &ovArgs) == TCL_OK
      && ocArgs > 0) {
    for (i = 0; i < ocArgs; i++) {
      char *option = ObjStr(ovArgs[i]);
      if (option != 0) {
	switch (*option) {
	case 'i':
	  if (strcmp(option, "instinvar") == 0) {
	    opt->checkoptions |= CHECK_CLINVAR;
	  } else if (strcmp(option, "invar") == 0) {
	    opt->checkoptions |= CHECK_OBJINVAR;
	  }
	  break;
	case 'p':
	  if (strcmp(option, "pre") == 0) {
	    opt->checkoptions |= CHECK_PRE;
	  } else if (strcmp(option, "post") == 0) {
	    opt->checkoptions  |= CHECK_POST;
	  }
	  break;
	case 'a':
	  if (strcmp(option, "all") == 0) {
	    opt->checkoptions |= CHECK_ALL;
	  }
	  break;	
	}
      }
    }
  }
  if (opt->checkoptions == CHECK_NONE && ocArgs>0) {
    return XOTclVarErrMsg(in, "Unknown check option in command '",
			  ObjStr(obj->cmdName), " ", ObjStr(objv[0]),
			  " ", ObjStr(objv[1]),
			  "', valid: all pre post invar instinvar", (char*) NULL);
  }

  Tcl_ResetResult(in);
  return TCL_OK;
}

static int
XOTclOMixinMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  int oc; Tcl_Obj **ov;
  XOTclObject *obj = (XOTclObject*)cd;
  int i, result = TCL_OK;
  XOTclObjectOpt *opt;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2)
    return XOTclObjErrArgCnt(in, obj->cmdName, "mixin <classes> ?args?");

  if (Tcl_ListObjGetElements(in, objv[1], &oc, &ov)!= TCL_OK)
    return TCL_ERROR;

  if (obj->opt) {
    CmdListRemoveList(&obj->opt->mixins, GuardDel);
  }

  obj->flags &= ~XOTCL_MIXIN_ORDER_VALID;
  opt = XOTclRequireObjectOpt(obj);

  /*
   * since mixin procs may be used as filters -> we have to invalidate
   */
  obj->flags &= ~XOTCL_FILTER_ORDER_VALID;

  for (i = 0; i < oc; i++) {
    result = MixinAdd(in, &opt->mixins, ov[i]);
    /*CmdListPrint("object mixins\n", opt->mixins);*/
    if (result != TCL_OK)
      return result;
  }

  MixinComputeDefined(in, obj);
  FilterComputeDefined(in, obj);

  return result;
}


static int
XOTclOMixinGuardMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTclCmdList* h;
  XOTclObjectOpt *opt;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 3)
    return XOTclObjErrArgCnt(in, obj->cmdName, "mixinguard mixin guards");

  opt = obj->opt;
  if (opt && opt->mixins) {
    XOTclClass *mixinCl = GetClass(in, ObjStr(objv[1]));
    Tcl_Command mixinCmd = NULL;
    if (mixinCl) {
      mixinCmd = Tcl_GetCommandFromObj(in, mixinCl->object.cmdName);
    }
    if (mixinCmd) {
      h = CmdListFindCmdInList(mixinCmd, opt->mixins);
      if (h) {
	if (h->clientData)
	  GuardDel((XOTclCmdList*) h);
	GuardAdd(in, h, objv[2]);
	obj->flags &= ~XOTCL_MIXIN_ORDER_VALID;
	return TCL_OK;
      }
    }
  }

  return XOTclVarErrMsg(in, "Mixinguard: can't find mixin ",
			ObjStr(objv[1]), " on ", ObjStr(obj->cmdName), NULL);
}

static int
XOTclOFilterMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  int oc; Tcl_Obj **ov;
  XOTclObject *obj = (XOTclObject*)cd;
  int i, result = TCL_OK;
  XOTclObjectOpt *opt;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2)
    return XOTclObjErrArgCnt(in, obj->cmdName, "filter filterNameList");

  if (Tcl_ListObjGetElements(in, objv[1], &oc, &ov) != TCL_OK)
    return TCL_ERROR;

  if (obj->opt) {
    CmdListRemoveList(&obj->opt->filters, GuardDel);
  }

  obj->flags &= ~XOTCL_FILTER_ORDER_VALID;
  opt = XOTclRequireObjectOpt(obj);

  for (i = 0; i < oc; i ++) {
    result = FilterAdd(in, &opt->filters, ov[i], obj, 0);
    if (result != TCL_OK)
      return result;
  }
  /*FilterComputeDefined(in, obj);*/
  return result;
}

static int
XOTclOFilterGuardMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTclCmdList* h;
  XOTclObjectOpt *opt;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 3)
    return XOTclObjErrArgCnt(in, obj->cmdName, "filterguard filtername filterGuards");

  opt = obj->opt;
  if (opt && opt->filters) {
    h = CmdListFindNameInList(in, ObjStr(objv[1]), opt->filters);
    if (h) {
      if (h->clientData)
	GuardDel((XOTclCmdList*) h);
      GuardAdd(in, h, objv[2]);
      obj->flags &= ~XOTCL_FILTER_ORDER_VALID;
      return TCL_OK;
    }
  }

  return XOTclVarErrMsg(in, "Filterguard: can't find filter ",
			ObjStr(objv[1]), " on ", ObjStr(obj->cmdName), NULL);
}

/*
 *  Searches for filter on [self] and returns fully qualified name
 *  if it is not found it returns an empty string
 */
static int
XOTclOFilterSearchMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  char *methodName, *fullName;
  XOTclCmdList *cmdList;
  XOTclClass *fcl;
  XOTclObject *fobj;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2) return XOTclObjErrArgCnt(in, obj->cmdName, "filtersearch name");
  Tcl_ResetResult(in);

  if (!(obj->flags & XOTCL_FILTER_ORDER_VALID))
    FilterComputeDefined(in, obj);
  if (!(obj->flags & XOTCL_FILTER_ORDER_DEFINED))
    return TCL_OK;

  methodName = ObjStr(objv[1]);
  cmdList = obj->filterOrder;

  while (cmdList) {
    CONST84 char *filterName = Tcl_GetCommandName(in, cmdList->cmdPtr);
    if (filterName[0] == methodName[0] && !strcmp(filterName, methodName))
      break;
    cmdList = cmdList->next;
  }

  if (!cmdList)
    return TCL_OK;

  fullName = NSCmdFullName(cmdList->cmdPtr);
  if ((fcl = GetClassFromFullName(in, fullName))) {
    fobj = 0;
  } else {
    fobj = GetObject(in, fullName);
  }

  Tcl_SetObjResult(in, getFullProcQualifier(in, methodName, fobj, fcl));
  return TCL_OK;
}

static int
XOTclOProcSearchMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  XOTclClass *cl = 0;
  Tcl_Command cmd = 0;
  char *simpleName, *methodName;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2) return XOTclObjErrArgCnt(in, obj->cmdName, "procsearch name");

  Tcl_ResetResult(in);

  methodName = ObjStr(objv[1]);

  if (obj->nsPtr)
    cmd = FindMethod(methodName, Tcl_Namespace_cmdTable(obj->nsPtr));

  if (!cmd) {
    if (!(obj->flags & XOTCL_MIXIN_ORDER_VALID))
      MixinComputeDefined(in, obj);

    if (obj->flags & XOTCL_MIXIN_ORDER_DEFINED_AND_VALID) {
      XOTclCmdList* mixinList = obj->mixinOrder;
      while (mixinList) {
	XOTclClass *mcl = GetClass(in, (char *)Tcl_GetCommandName(in, mixinList->cmdPtr));
	if (SearchCMethod(mcl, methodName, &cmd))
	  break;
	mixinList = mixinList->next;
      }
    }
  }

  if (!cmd)
    cl = SearchCMethod(obj->cl, methodName, &cmd);

  if (cmd) {
    char *fullName = NSCmdFullName(cmd);
    XOTclClass *pcl = GetClassFromFullName(in, fullName);
    XOTclObject *pobj = pcl ? 0 : GetObject(in, fullName);
    simpleName = (char*) Tcl_GetCommandName(in, cmd);
    Tcl_SetObjResult(in, getFullProcQualifier(in, simpleName, pobj, pcl));
  }
  return TCL_OK;
}

static int
XOTclORequireNamespaceMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc != 1) return XOTclObjErrArgCnt(in, obj->cmdName, "requireNamespace");

  requireObjNamespace(in, obj);
  return TCL_OK;
}

typedef enum {NO_DASH, SKALAR_DASH, LIST_DASH} dashArgType;

static dashArgType
isDashArg(Tcl_Interp *in, Tcl_Obj *obj, char **methodName, int *objc, Tcl_Obj **objv[]) {
  char *flag;
  static Tcl_ObjType *listType = NULL;

  assert(obj);

  /* fetch list type, if not set already; if used on more places, this should
      be moved into the interpreter state
  */
  if (listType == NULL) {
#if defined(PRE82)
    Tcl_Obj *tmp = Tcl_NewListObj(1, &obj);
    listType = tmp->typePtr;
    DECR_REF_COUNT(tmp);
#else
    static XOTclMutex initMutex = 0;
    XOTclMutexLock(&initMutex);
    if (listType == NULL) {
      listType = Tcl_GetObjType("list");
    }
    XOTclMutexUnlock(&initMutex);
#endif
  }

  if (obj->typePtr == listType) {
    if (Tcl_ListObjGetElements(in, obj, objc, objv) == TCL_OK && *objc>0) {
      flag = ObjStr(*objv[0]);
      if (*flag == '-') {
	*methodName = flag+1;
	return LIST_DASH;
      }
    }
  }
  flag = ObjStr(obj);
  if (*flag == '-' && isalpha((int)*((flag)+1))) {
    *methodName = flag+1;
    *objc = 1;
    return SKALAR_DASH;
  }
  return NO_DASH;
}

static int
callConfigureMethod(Tcl_Interp *in, XOTclObject *obj,
		    char *methodName, int argc, Tcl_Obj *argv[]) {
  int result;
  Tcl_Obj *method = Tcl_NewStringObj(methodName,-1);

  /*fprintf(stderr,"callConfigureMethod method %s->'%s' argc %d\n",
    ObjStr(obj->cmdName), methodName, argc);*/

  if (isInitString(methodName))
    obj->flags |= XOTCL_INIT_CALLED;

  INCR_REF_COUNT(method);
  result = callMethod((ClientData)obj, in, method, argc, argv, XOTCL_CM_NO_UNKNOWN);
  DECR_REF_COUNT(method);

  /* fprintf(stderr, "method  '%s' called args: %d o=%p, result=%d\n",
     methodName, argc+1, obj, result); */

  if (result != TCL_OK) {
    Tcl_AppendResult(in, " during '", ObjStr(obj->cmdName), " ",
		     methodName, "'", 0);
  }
  return result;
}


static int
XOTclOConfigureMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclObject *obj = (XOTclObject*)cd;
  Tcl_Obj **argv, **nextArgv;
  int i, argc, nextArgc, normalArgs, result = TCL_OK, isdasharg = NO_DASH;
  char *methodName, *nextMethodName;

  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 1) return XOTclObjErrArgCnt(in, obj->cmdName,
					 "configure ?args?");
  /* find arguments without leading dash */
  for (i=1; i < objc; i++) {
    if ((isdasharg = isDashArg(in, objv[i], &methodName, &argc, &argv)))
      break;
  }
  normalArgs = i-1;

  for( ; i < objc;  argc=nextArgc, argv=nextArgv, methodName=nextMethodName) {
    Tcl_ResetResult(in);
    switch (isdasharg) {
    case SKALAR_DASH:    /* argument is a skalar with a leading dash */
      { int j;
	for (j = i+1; j < objc; j++, argc++) {
	  if ((isdasharg = isDashArg(in, objv[j], &nextMethodName, &nextArgc, &nextArgv)))
	    break;
	}
	result = callConfigureMethod(in, obj, methodName, argc+1, objv+i+1);
	if (result != TCL_OK)
	  return result;
	i += argc;
	break;
      }
    case LIST_DASH:  /* argument is a list with a leading dash, grouping determined by list */
      {	i++;
	if (i<objc)
	  isdasharg = isDashArg(in, objv[i], &nextMethodName, &nextArgc, &nextArgv);
	result = callConfigureMethod(in, obj, methodName, argc+1, argv+1);
	if (result != TCL_OK)
	  return result;
	break;
      }
    default:
      {
	Tcl_AppendResult(in, ObjStr(obj->cmdName),
			 " configure: unexpected argument '", ObjStr(objv[i]),
			 "' between parameters", 0);
	return TCL_ERROR;
      }
    }
  }
  Tcl_ResetResult(in);
  Tcl_SetIntObj(Tcl_GetObjResult(in), normalArgs);
  return result;
}


/*
 * class method implementations
 */

static int
XOTclCInstDestroyMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  XOTclObject *delobj;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc < 2)
      return XOTclObjErrArgCnt(in, cl->object.cmdName, "instdestroy <obj/cl>");

  if (GetXOTclObjectFromObj(in, objv[1], &delobj) != TCL_OK)
    return XOTclVarErrMsg(in, "Can't destroy object ",
			  ObjStr(objv[1]), " that does not exist.", NULL);
  /*
   * latch, and call delete command if not already in progress
   */
  delobj->flags |= XOTCL_DESTROY_CALLED;
  RUNTIME_STATE(in)->callIsDestroy = 1;

  if (RUNTIME_STATE(in)->exitHandlerDestroyRound !=
      XOTCL_EXITHANDLER_ON_SOFT_DESTROY) {
    CallStackDestroyObject(in, delobj);
  }

  return TCL_OK;
}

static int
XOTclCAllocMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  XOTclClass *newcl;
  XOTclObject *newobj;
  int result;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc < 2)
      return XOTclObjErrArgCnt(in, cl->object.cmdName, "alloc <obj/cl> ?args?");

#if 0  
  fprintf(stderr, "type(%s)=%p %s %d\n", 
	  ObjStr(objv[1]), objv[1]->typePtr, objv[1]->typePtr?
	  objv[1]->typePtr->name:"NULL",
	  GetXOTclObjectFromObj(in, objv[1], &newobj)
	  );
  /*
   * if the lookup via GetObject for the object succeeds,
   * the object exists already,
   * and we do not overwrite it, but re-create it
   */
  if (GetXOTclObjectFromObj(in, objv[1], &newobj) == TCL_OK) {
    fprintf(stderr, "lookup successful\n");
    result = doCleanup(in, newobj, &cl->object, objc, objv);
  } else
#endif
    {
    /*
     * create a new object from scratch
     */
    if (IsMetaClass(in, cl)) {
      /*
       * if the base class is a meta-class, we create a class
       */
      newcl = PrimitiveCCreate(in, ObjStr(objv[1]), cl);
      if (newcl == 0)
	result = XOTclVarErrMsg(in, "Class alloc failed for '",ObjStr(objv[1]),
				"' (possibly parent namespace does not exist)", NULL);
      else
	result = TCL_OK;

    } else {
      /*
       * if the base class is an ordinary class, we create an object
       */
      newobj = PrimitiveOCreate(in, ObjStr(objv[1]), cl);
      if (newobj == 0)
	result = XOTclVarErrMsg(in, "Object alloc failed for '",ObjStr(objv[1]),
				"' (possibly parent namespace does not exist)", NULL);
      else
	result = TCL_OK;
    }
  }

  if (result == TCL_OK)
    Tcl_ResetResult(in);

  return result;
}


static int
XOTclCCreateMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  XOTclObject *obj = &cl->object;
  XOTclObject *newobj;

  int result, nameLength;
  char *objName;
  Tcl_Obj *nameObj;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2)
    return XOTclObjErrArgCnt(in, cl->object.cmdName, "create <obj> ?args?");

  /*
   * Check whether we have to call recreate (i.e. when the
   * object exists already)
   */
  /* fprintf(stderr,"+++ create objv[1] = %p (%d) %s\n",
     objv[1],objv[1]->refCount,ObjStr(objv[1])); */

  if (GetXOTclObjectFromObj(in, objv[1], &newobj) == TCL_OK) {
    /*fprintf(stderr, "+++ recreate, call recreate method ... %s\n", ObjStr(objv[1]));*/
    /* call recreate --> initialization */
    result = callMethod((ClientData) obj, in,
                         XOTclGlobalObjects[RECREATE], objc+1, objv+1, 0);
    if (result != TCL_OK)
      return result;

    nameObj = newobj->cmdName;
    INCR_REF_COUNT(nameObj);

    objTrace("RECREATE", newobj);

  } else {
    objName = Tcl_GetStringFromObj(objv[1],&nameLength);
    if (!NSCheckColons(objName, nameLength))
      return XOTclVarErrMsg(in, "Cannot create object -- illegal name '",
			    objName, "'", (char *)NULL);
    /*
     * call "alloc"
     */
    /* fprintf(stderr, "alloc ... %s\n", ObjStr(objv[1]));*/
    result = callMethod((ClientData) obj, in,
                         XOTclGlobalObjects[ALLOC], objc+1, objv+1, 0);
    if (result != TCL_OK)
      return result;

    newobj = GetObject(in, objName);
    if (newobj == 0)
      return XOTclErrMsg(in, "couldn't find result of alloc", TCL_STATIC);
    nameObj = newobj->cmdName;

    (void)RemoveInstance(newobj, newobj->cl);
    AddInstance(newobj, cl);
    objTrace("CREATE", newobj);

    /* in case, the object is destroyed during initialization, we incr refcount */
    INCR_REF_COUNT(nameObj);
    result = doObjInitialization(in, newobj, objc, objv);
  }
  /* fprintf(stderr, "alloc -- end ... %s\n", ObjStr(objv[1]));*/

  if (result == TCL_OK) {
    Tcl_SetObjResult(in, nameObj);
  }
  DECR_REF_COUNT(nameObj);

  return result;
}

static char *
XOTclUnsetTrace(ClientData cd, Tcl_Interp *in, CONST84 char *name, CONST84 char *name2, int flags)
{
  Tcl_Obj *obj = (Tcl_Obj *)cd;
  XOTclObject *o;
  char *result = NULL;

  if (GetXOTclObjectFromObj(in, obj, &o) == TCL_OK) {
    Tcl_Obj *res = Tcl_GetObjResult(in); /* save the result */
    INCR_REF_COUNT(res);

    if (callMethod((ClientData)o, in, XOTclGlobalObjects[DESTROY],2,0,0) != TCL_OK) {
      result = "Destroy for volatile object failed";
    } else
      result = "No XOTcl Object passed";

    Tcl_SetObjResult(in, res);  /* restore the result */
    DECR_REF_COUNT(res);
  }
  DECR_REF_COUNT(obj);
  return result;
}



static int
XOTclCNewMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass  *cl = XOTclObjectToClass(cd);
  XOTclObject *obj = &cl->object, *child = NULL;
  Tcl_Obj *fullname;
  int result, offset = 1, 
#if REFCOUNTED
    isrefcount = 0, 
#endif
    i;
  Tcl_DString dFullname, *dsPtr = &dFullname;
  XOTclStringIncrStruct *iss = &RUNTIME_STATE(in)->iss;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 1)
    return XOTclObjErrArgCnt(in, cl->object.cmdName, "new [-childof obj] ?args?");

  for (i=1; i<objc; i++) {
    char *option = ObjStr(objv[i]);
    if (*option == '-' && strcmp(option,"-childof")==0 && i<objc-1) {
      offset += 2;
      if (GetXOTclObjectFromObj(in, objv[i+1], &child) != TCL_OK) {
	return XOTclErrMsg(in, "not a valid object specified as child\n", TCL_STATIC);
      }
#if REFCOUNTED
    } else if (strcmp(option,"-refcount")==0) {
      isrefcount = 1;
      offset += 1;
#endif
    } else
      break;
  }

  Tcl_DStringInit(dsPtr);
  if (child) {
    Tcl_DStringAppend(dsPtr, ObjStr(child->cmdName), -1);
    Tcl_DStringAppend(dsPtr, "::__#", 5);
  } else {
    Tcl_DStringAppend(dsPtr, "::xotcl::__#", 12);
  }
  (void)XOTclStringIncr(iss);
  Tcl_DStringAppend(dsPtr, iss->start, iss->length);

  fullname = Tcl_NewStringObj(Tcl_DStringValue(dsPtr), Tcl_DStringLength(dsPtr));

  INCR_REF_COUNT(fullname);

  objc -= offset;
  {
    DEFINE_NEW_TCL_OBJS_ON_STACK(objc+3, ov);

    ov[0] = objv[0];
    ov[1] = XOTclGlobalObjects[CREATE];
    ov[2] = fullname;
    if (objc >= 1)
      memcpy(ov+3, objv+offset, sizeof(Tcl_Obj *)*objc);

    result = DoDispatch(cd, in, objc+3, ov, 0);
    FREE_TCL_OBJS_ON_STACK(ov);
  }

  if (result == TCL_OK) {
#if REFCOUNTED
    if (isrefcount) {
      Tcl_Obj *obj = Tcl_GetObjResult(in);
      XOTclObject *o = (XOTclObject*) obj->internalRep.otherValuePtr;
      o->flags |= XOTCL_REFCOUNTED;
      o->teardown = in;
      DECR_REF_COUNT(obj);
    }
#endif
  }

  DECR_REF_COUNT(fullname);
  Tcl_DStringFree(dsPtr);

  return result;
}


static int
XOTclCRecreateMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  XOTclObject *obj = &cl->object, *newobj;
  int result;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (!obj) return XOTclObjErrType(in, objv[0], "Object");
  if (objc < 2)
    return XOTclObjErrArgCnt(in, cl->object.cmdName, "recreate <obj> ?args?");

  if (GetXOTclObjectFromObj(in, objv[1], &newobj) != TCL_OK)
    return XOTclVarErrMsg(in, "can't recreate not existing obj ",
			  ObjStr(objv[1]), (char *)NULL);

  INCR_REF_COUNT(objv[1]);
  newobj->flags |= XOTCL_RECREATE;

  result = doCleanup(in, newobj, obj, objc, objv);
  if (result == TCL_OK) {
      result = doObjInitialization(in, newobj, objc, objv);
      if (result == TCL_OK)
	Tcl_SetObjResult(in, objv[1]);
  }
  DECR_REF_COUNT(objv[1]);
  return result;
}

static int
XOTclCSuperClassMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  XOTclClasses* osl = 0;
  int oc; Tcl_Obj **ov;
  XOTclClass **scl = 0;
  int reversed = 0;
  int i, j;
  XOTclClasses* filterCheck = ComputeOrder(cl, Super);

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc != 2)
    return XOTclObjErrArgCnt(in, cl->object.cmdName, "superclass <classes>");

  if (Tcl_ListObjGetElements(in, objv[1], &oc, &ov)!= TCL_OK || oc==0)
    return XOTclObjErrArgCnt(in, cl->object.cmdName, "superclass <classes>");


  /*
   * we have to remove all dependent superclass filter referenced
   * by class or one of its subclasses
   *
   * do not check the class "cl" itself (first entry in
   * filterCheck class list)
   */
  if (filterCheck)
    filterCheck = filterCheck->next;
  while (filterCheck) {
    FilterRemoveDependentFilterCmds(cl, filterCheck->cl);
    filterCheck = filterCheck->next;
  }

  /* invalidate all interceptors orders of instances of this
     and of all depended classes */
  MixinInvalidateObjOrders(cl);
  FilterInvalidateObjOrders(in, cl);

  scl = NEW_ARRAY(XOTclClass*,oc);
  for (i = 0; i < oc; i++) {
    if (GetXOTclClassFromObj(in, ov[i], &scl[i], 1) != TCL_OK) {
      FREE(XOTclClass**, scl);
      return XOTclErrBadVal(in, "a list of classes", ObjStr(objv[1]));
    }
  }

  /*
   * check that superclasses don't precede their classes
   */

  for (i = 0; i < oc; i++) {
    if (reversed != 0) break;
    for (j = i+1; j < oc; j++) {
      XOTclClasses* dl = ComputeOrder(scl[j], Super);
      if (reversed != 0) break;
      while (dl != 0) {
        if (dl->cl == scl[i]) break;
        dl = dl->next;
      }
      if (dl != 0) reversed = 1;
    }
  }

  if (reversed != 0) {
    return XOTclErrBadVal(in, "classes in dependence order", ObjStr(objv[1]));
  }

  while (cl->super != 0) {

    /*
     * build up an old superclass list in case we need to revert
     */

    XOTclClass *sc = cl->super->cl;
    XOTclClasses* l = osl;
    osl = NEW(XOTclClasses);
    osl->cl = sc;
    osl->next = l;
    (void)RemoveSuper(cl, cl->super->cl);
  }
  for (i = 0; i < oc; i++)
    AddSuper(cl, scl[i]);
  FREE(XOTclClass**,scl);
  FlushPrecedences(cl);

  if (!ComputeOrder(cl, Super)) {

    /*
     * cycle in the superclass graph, backtrack
     */

    XOTclClasses* l;
    while (cl->super != 0) (void)RemoveSuper(cl, cl->super->cl);
    for (l = osl; l != 0; l = l->next) AddSuper(cl, l->cl);
    XOTclFreeClasses(osl);
    return XOTclErrBadVal(in, "a cycle-free graph", ObjStr(objv[1]));
  }
  XOTclFreeClasses(osl);

  /* if there are no more super classes add the Object
     class as superclasses */
  if (cl->super == 0)
    AddSuper(cl, RUNTIME_STATE(in)->theObject);

  Tcl_ResetResult(in);
  return TCL_OK;
}

static int
XOTclCInfoMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj * CONST objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  Tcl_Namespace *nsp = cl->nsPtr;
  XOTclClassOpt* opt = cl->opt;

  char *pattern, *cmd;
  int modifiers = 0;

  if (!cl)
    return XOTclObjErrType(in, objv[0], "Class");
  if (objc < 2)
    return XOTclObjErrArgCnt(in, cl->object.cmdName, "info <opt> ?args?");

  cmd = ObjStr(objv[1]);
  pattern = (objc > 2) ? ObjStr(objv[2]) : 0;

  /*
   * check for "-" modifiers
   */
  if (pattern && *pattern == '-') {
    modifiers = countModifiers(objc, objv);
    pattern = (objc > 2+modifiers) ? ObjStr(objv[2+modifiers]) : 0;
  }

  switch (*cmd) {
  case 'c':
    if (!strcmp(cmd, "classchildren")) {
      if (objc > 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, cl->object.cmdName, "info classchildren ?pat?");
      return ListChildren(in, (XOTclObject*) cl, pattern, 1);
    } else if (!strcmp(cmd, "classparent")) {
      if (objc > 2 || modifiers > 0)
	return XOTclObjErrArgCnt(in, cl->object.cmdName, "info classparent");
      return ListParent(in, &cl->object);
    }
    break;

  case 'h':
    if (!strcmp(cmd, "heritage")) {
      if (objc > 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, cl->object.cmdName, "info heritage ?pat?");
      return ListHeritage(in, cl, pattern);
    }
    break;

  case 'i':
    if (cmd[1] == 'n' && cmd[2] == 's' && cmd[3] == 't') {
      char *cmdTail = cmd + 4;
      switch (*cmdTail) {
      case 'a':
	if (!strcmp(cmdTail, "ances")) {
	  if (objc > 3 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName, "info instances ?pat?");
	  return ListObjPtrHashTable(in, &cl->instances, pattern);
	} else if (!strcmp(cmdTail, "args")) {
	  if (objc != 3 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName,
				     "info instargs <instproc>");
	  return ListProcArgs(in, Tcl_Namespace_cmdTable(nsp), pattern);
	}
	break;

      case 'b':
	if (!strcmp(cmdTail, "body")) {
	  if (objc != 3 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName,
				     "info instbody <instproc>");
	  return ListProcBody(in, Tcl_Namespace_cmdTable(nsp), pattern);
	}
	break;

      case 'c':
	if (!strcmp(cmdTail, "commands")) {
	  if (objc > 3 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName,
				     "info instcommands ?pat?");
	  return ListKeys(in, Tcl_Namespace_cmdTable(nsp), pattern);
	}
	break;

      case 'd':
	if (!strcmp(cmdTail, "default")) {
	  if (objc != 5 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName,
				     "info instdefault <instproc> <arg> <var>");
	  return ListProcDefault(in, Tcl_Namespace_cmdTable(nsp), pattern,
				 ObjStr(objv[3]), objv[4]);
	}
	break;

      case 'f':
	if (!strcmp(cmdTail, "filter")) {
	  int withGuards = 0;
	  if (objc-modifiers > 3)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName,
				     "info instfilter ?-guards? ?pat?");
	  if (modifiers > 0) {
	    withGuards = checkForModifier(objv, modifiers, "-guards");
	    if (withGuards == 0)
	      return XOTclVarErrMsg(in, "info instfilter: unknown modifier ",
				    ObjStr(objv[2]), (char *)NULL);
	  }
	  return opt ? FilterInfo(in, opt->instfilters, pattern, withGuards, 0) : TCL_OK;

	} else if (!strcmp(cmdTail, "filterguard")) {
	  if (objc != 3 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName,
				     "info instfilterguard filter");
	  return opt ? GuardList(in, opt->instfilters, pattern) : TCL_OK;
	}
	break;

      case 'i':
	if (!strcmp(cmdTail, "invar")) {
	  XOTclAssertionStore *assertions = opt ? opt->assertions : 0;
	  if (objc != 2 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName, "info instinvar");

	  if (assertions && assertions->invariants)
	    Tcl_SetObjResult(in, AssertionList(in, assertions->invariants));
	  return TCL_OK;
	}
	break;

      case 'm':
	if (!strcmp(cmdTail, "mixin")) {
	  int withGuards = 0;

	  if (objc-modifiers > 3 || modifiers > 1)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName,
				     "info instmixin ?-guards? ?class?");
	  if (modifiers > 0) {
	    withGuards = checkForModifier(objv, modifiers, "-guards");
	    if (withGuards == 0)
	      return XOTclVarErrMsg(in, "info instfilter: unknown modifier ",
				    ObjStr(objv[2]), (char *)NULL);
	  }
	  return opt ? MixinInfo(in, opt->instmixins, pattern, withGuards) : TCL_OK;

	} else if (!strcmp(cmdTail, "mixinguard")) {
	  if (objc != 3 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName,
				     "info instmixinguard mixin");
	  return opt ? GuardList(in, opt->instmixins, pattern) : TCL_OK;
	}
	break;

      case 'p':
	if (!strcmp(cmdTail, "procs")) {
	  if (objc > 3 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName, "info instprocs ?pat?");
	  return ListMethodKeys(in, Tcl_Namespace_cmdTable(nsp), pattern,
				/*noProcs*/ 0, /*noCmds*/ 1);
	} else if (!strcmp(cmdTail, "pre")) {
	  XOTclProcAssertion* procs;
	  if (objc != 3 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName,
				     "info instpre <proc>");
	  if (opt && opt->assertions) {
	    procs = AssertionFindProcs(opt->assertions, ObjStr(objv[2]));
	    if (procs) Tcl_SetObjResult(in, AssertionList(in, procs->pre));
	  }
	  return TCL_OK;
	} else if (!strcmp(cmdTail, "post")) {
	  XOTclProcAssertion* procs;
	  if (objc != 3 || modifiers > 0)
	    return XOTclObjErrArgCnt(in, cl->object.cmdName,
				     "info instpost <proc>");
	  if (opt && opt->assertions) {
	    procs = AssertionFindProcs(opt->assertions, ObjStr(objv[2]));
	    if (procs) Tcl_SetObjResult(in, AssertionList(in, procs->post));
	  }
	  return TCL_OK;
	}
	break;
      }
    }
    break;

  case 'p':
    if (!strcmp(cmd, "parameterclass")) {
      if (opt && opt->parameterClass) {
	Tcl_SetObjResult(in, opt->parameterClass);
      } else {
	Tcl_SetObjResult(in, XOTclGlobalObjects[PARAM_CL]);
      }
      return TCL_OK;
    } else if (!strcmp(cmd, "parameter")) {
      if (cl->parameters) {
	Tcl_SetObjResult(in, cl->parameters);
      } else {
	Tcl_SetObjResult(in, XOTclGlobalObjects[EMPTY]);
      }
      return TCL_OK;
    }
    break;

  case 's':
    if (!strcmp(cmd, "superclass")) {
      if (objc > 3 || modifiers > 0)
        return XOTclObjErrArgCnt(in, cl->object.cmdName, "info superclass ?class?");
      return ListSuperclasses(in, cl, pattern);
    } else if (!strcmp(cmd, "subclass")) {
      if (objc > 3 || modifiers > 0)
	return XOTclObjErrArgCnt(in, cl->object.cmdName, "info subclass ?class?");
      return ListSubclasses(in, cl, pattern);
    }
    break;
  }
  return XOTclOInfoMethod(cd, in, objc, (Tcl_Obj **)objv);
}

static int
XOTclCParameterMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  XOTclObject *obj = &cl->object;
  Tcl_Obj     **pv = 0;
  Tcl_Obj     **ov = 0;
  int         elts, pc, oc, result;
  char *      params;
  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc != 2)
    return XOTclObjErrArgCnt(in, cl->object.cmdName, "parameter ?params?");
  if (cl->parameters) {
    DECR_REF_COUNT(cl->parameters);
  }
  /* did we delete the parameters ? */
  params = ObjStr(objv[1]);
  if ((params == NULL) || (*params == '\0')) {
    cl->parameters = 0;
    return TCL_OK;
  }

  /* ok, remember the params */
  cl->parameters = objv[1];
  INCR_REF_COUNT(cl->parameters);

  /* call getter/setter methods in params */
  result = Tcl_ListObjGetElements(in, objv[1], &pc, &pv);
  if (result == TCL_OK) {
    for (elts = 0; elts < pc; elts++) {
      result = Tcl_ListObjGetElements(in, pv[elts], &oc, &ov);
      if (result == TCL_OK && oc > 0 ) {
	result = callParameterMethodWithArg(obj, in, XOTclGlobalObjects[MKGETTERSETTER],
			    cl->object.cmdName, 3+oc, ov,0);
      }
      if (result != TCL_OK)
	break;
    }
  }
  return result;
}
static int
XOTclCParameterClassMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  char *paramClStr;
  XOTclClassOpt* opt = cl->opt;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc != 2)
    return XOTclObjErrArgCnt(in, cl->object.cmdName, "parameterclass cl");

  paramClStr = ObjStr(objv[1]);
  if (opt && opt->parameterClass) {
    DECR_REF_COUNT(opt->parameterClass);
  }
  if ((paramClStr == NULL) || (*paramClStr == '\0')) {
    if (opt)
      opt->parameterClass = 0;
  } else {
    opt = XOTclRequireClassOpt(cl);
    opt->parameterClass = objv[1];
    INCR_REF_COUNT(opt->parameterClass);
  }
  return TCL_OK;
}

static int
XOTclCInstParameterCmdMethod(ClientData cd, Tcl_Interp *in,
			     int objc, Tcl_Obj * CONST objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc < 2) return XOTclObjErrArgCnt(in, cl->object.cmdName, "instparametercmd name");
  XOTclAddIMethod(in, (XOTcl_Class*) cl, ObjStr(objv[1]),
		  (Tcl_ObjCmdProc*)XOTclSetterMethod, 0, 0);
  return TCL_OK;
}

static int
XOTclCParameterCmdMethod(ClientData cd, Tcl_Interp *in, 
			 int objc, Tcl_Obj * CONST objv[]) {
  XOTclObject *obj = (XOTclObject*) cd;

  if (objc < 2) return XOTclObjErrArgCnt(in, obj->cmdName, "parametercmd name");
  XOTclAddPMethod(in, (XOTcl_Object*) obj, ObjStr(objv[1]),
		  (Tcl_ObjCmdProc*)XOTclSetterMethod, 0, 0);
  return TCL_OK;
}

static void tclCmdDeleteProc(ClientData cd) {
  tclCmdClientData *tcd = (tclCmdClientData *)cd;
  DECR_REF_COUNT(tcd->cmdName);
  FREE(tclCmdClientData, tcd);
}

static int
XOTclCInstTclCmdMethod(ClientData cd, Tcl_Interp *in,
		      int objc, Tcl_Obj * CONST objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  tclCmdClientData *tcd;
  char *cmdName;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc < 2) return XOTclObjErrArgCnt(in, cl->object.cmdName, "insttclcmd name");
  cmdName = ObjStr(objv[1]);
  tcd = NEW(tclCmdClientData);
  tcd->obj = (XOTcl_Object*)cl;
  tcd->cmdName = objv[1];
  INCR_REF_COUNT(tcd->cmdName);

  XOTclAddIMethod(in, (XOTcl_Class*) cl, NSTail(cmdName), 
		  (Tcl_ObjCmdProc*)XOTclOEvalMethod,
		  (ClientData)tcd, tclCmdDeleteProc);
  return TCL_OK;
}

static int
XOTclCTclCmdMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj * CONST objv[]) {
  XOTcl_Object *obj = (XOTcl_Object*) cd;
  char *cmdName;
  Tcl_Obj *cmdObj;
  tclCmdClientData *tcd;

  if (objc < 2)  return XOTclObjErrArgCnt(in, obj->cmdName, "tclcmd name");

  cmdName = ObjStr(objv[1]);
  if (*cmdName != ':') {
    cmdObj = Tcl_NewStringObj("::", 2);
    Tcl_AppendObjToObj(cmdObj, objv[1]);
  } else {
    cmdObj = objv[1];
  }
  tcd = NEW(tclCmdClientData);
  tcd->obj = obj;
  tcd->cmdName = cmdObj;
  INCR_REF_COUNT(tcd->cmdName);
  /*
  fprintf(stderr,"tcd?%p, tcd->cmdName=%p, %s\n",
	  tcd, tcd->cmdName, ObjStr(tcd->cmdName));
  */

  XOTclAddPMethod(in, obj, NSTail(cmdName), (Tcl_ObjCmdProc*)XOTclOEvalMethod,
		  (ClientData)tcd, tclCmdDeleteProc);
  return TCL_OK;
}

static int
XOTclCVolatileMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj * CONST objv[]) {
  XOTclObject *obj = (XOTclObject*) cd;
  Tcl_Obj *o = obj->cmdName;
  int result = TCL_ERROR;
  char *fullName = ObjStr(o);
  char *vn;
  callFrameContext ctx = {0};

  if (objc != 1) return XOTclObjErrArgCnt(in, obj->cmdName, "volatile");

  CallStackUseActiveFrames(in, &ctx);
  vn = NSTail(fullName);
  if (Tcl_SetVar2(in, vn, 0, fullName, 0) != NULL) {
    result = Tcl_TraceVar(in, vn, TCL_TRACE_UNSETS, (Tcl_VarTraceProc*)XOTclUnsetTrace,
			  (ClientData)o);
  }
  CallStackRestoreSavedFrames(in, &ctx);

  if (result == TCL_OK) {
    INCR_REF_COUNT(o);
  }
  return result;
}

static int
XOTclCInstProcMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  char *argStr, *bdyStr, *name;
  XOTclClassOpt* opt;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc != 4 && objc != 6)
    return XOTclObjErrArgCnt(in, cl->object.cmdName,
			     "instproc name args body ?preAssertion postAssertion?");

  argStr = ObjStr(objv[2]);
  bdyStr = ObjStr(objv[3]);
  name = ObjStr(objv[1]);

  if ((cl == RUNTIME_STATE(in)->theObject && isDestroyString(name)) ||
      (cl == RUNTIME_STATE(in)->theClass && isInstDestroyString(name)) ||
      (cl == RUNTIME_STATE(in)->theClass && isAllocString(name)) ||
      (cl == RUNTIME_STATE(in)->theClass && isCreateString(name)))
    return XOTclVarErrMsg(in, className(cl), " instproc: '", name, "' of ",
			  className(cl), " can not be overwritten. Derive a ",
			  "sub-class", (char*) NULL);

  if (*argStr == 0 && *bdyStr == 0) {
    opt = cl->opt;
    if (opt && opt->assertions)
      AssertionRemoveProc(opt->assertions, name);
    NSDeleteCmd(in, cl->nsPtr, name);
  } else {
    opt = XOTclRequireClassOpt(cl);
    if (!opt->assertions)
      opt->assertions = AssertionCreateStore();
    MakeProc(cl->nsPtr, opt->assertions, in, objc, (Tcl_Obj **) objv);
  }

  /* could be a filter or filter inheritance ... update filter orders */
  FilterInvalidateObjOrders(in, cl);

  return TCL_OK;
}

static int
XOTclCInstFilterMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  int i, result = TCL_OK;
  Tcl_Obj **ov; int oc;
  XOTclClassOpt* opt;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc < 2) return XOTclObjErrArgCnt(in, cl->object.cmdName,
    "instfilter filterNameList");

  if (Tcl_ListObjGetElements(in, objv[1], &oc, &ov) != TCL_OK)
    return TCL_ERROR;

  opt = cl->opt;
  if (opt)
    CmdListRemoveList(&opt->instfilters, GuardDel);

  FilterInvalidateObjOrders(in, cl);
  opt = XOTclRequireClassOpt(cl);

  for (i = 0; i < oc; i ++) {
    result = FilterAdd(in, &opt->instfilters, ov[i], 0, cl);
    if (result != TCL_OK)
      return result;
  }
  return result;
}

static int
XOTclCInstFilterGuardMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  XOTclCmdList* h;
  XOTclClassOpt* opt;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc != 3) return XOTclObjErrArgCnt(in, cl->object.cmdName,
					  "instfilterguard filtername filterGuard");

  opt = cl->opt;
  if (opt && opt->instfilters) {
    h = CmdListFindNameInList(in, ObjStr(objv[1]), opt->instfilters);
    if (h) {
      if (h->clientData)
	GuardDel(h);
      GuardAdd(in, h, objv[2]);
      FilterInvalidateObjOrders(in, cl);
      return TCL_OK;
    }
  }

  return XOTclVarErrMsg(in, "Instfilterguard: can't find filter ",
			ObjStr(objv[1]), " on ", ObjStr(cl->object.cmdName), NULL);
}


static int
XOTclCInstMixinMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  int oc; Tcl_Obj **ov;
  XOTclClass *cl = XOTclObjectToClass(cd);
  int i, result = TCL_OK;
  XOTclClassOpt* opt;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc < 2) return XOTclObjErrArgCnt(in, cl->object.cmdName,
					 "instmixin classList");

  if (Tcl_ListObjGetElements(in, objv[1], &oc, &ov)!= TCL_OK)
    return TCL_ERROR;

  if ((opt = cl->opt))
    CmdListRemoveList(&opt->instmixins, GuardDel);

  MixinInvalidateObjOrders(cl);
  /*
   * since mixin procs may be used as filters -> we have to invalidate
   */
  FilterInvalidateObjOrders(in, cl);

  opt = XOTclRequireClassOpt(cl);
  for (i = 0; i < oc; i++) {
    result = MixinAdd(in, &opt->instmixins, ov[i]);
    if (result != TCL_OK)
      return result;
  }
  return result;
}

static int
XOTclCInstMixinGuardMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  XOTclCmdList* h;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc != 3) return XOTclObjErrArgCnt(in, cl->object.cmdName,
					  "instmixinguard mixin guard");

  if (cl->opt && cl->opt->instmixins) {
    XOTclClass *mixinCl = GetClass(in, ObjStr(objv[1]));
    Tcl_Command mixinCmd = NULL;
    if (mixinCl) {
      mixinCmd = Tcl_GetCommandFromObj(in, mixinCl->object.cmdName);
    }
    if (mixinCmd) {
      h = CmdListFindCmdInList(mixinCmd, cl->opt->instmixins);
       if (h) {
	 if (h->clientData)
	   GuardDel((XOTclCmdList*) h);
	 GuardAdd(in, h, objv[2]);
	 MixinInvalidateObjOrders(cl);
	 return TCL_OK;
       }
    }
  }

  return XOTclVarErrMsg(in, "Instmixinguard: can't find mixin ",
			ObjStr(objv[1]), " on ", ObjStr(cl->object.cmdName), NULL);
}

static int
XOTclCInvariantsMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  XOTclClass *cl = XOTclObjectToClass(cd);
  XOTclClassOpt* opt;

  if (!cl) return XOTclObjErrType(in, objv[0], "Class");
  if (objc != 2)
      return XOTclObjErrArgCnt(in, cl->object.cmdName,
			       "instinvar <invariantList>");
  opt = XOTclRequireClassOpt(cl);

  if (opt->assertions)
    TclObjListFreeList(opt->assertions->invariants);
  else
    opt->assertions = AssertionCreateStore();

  opt->assertions->invariants = AssertionNewList(in, objv[1]);
  return TCL_OK;
}

static int
XOTclCUnknownMethod(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *obj = (XOTclObject*) cd;
  char *self = ObjStr(obj->cmdName);
  int rc;

  if (objc < 2) return XOTclObjErrArgCnt(in, objv[0], "message ?args .. args?");
  if (isCreateString(self))
    return XOTclVarErrMsg(in, "error ", self, ": unable to dispatch '",
			  ObjStr(objv[1]), "'", (char*)NULL);

  rc = callMethod(cd, in, XOTclGlobalObjects[CREATE], objc+1, objv+1, 0);
  return rc;
}

/*
 * New Tcl Commands
 */
static int
XOTcl_NSCopyCmds(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Command cmd;
  Tcl_Obj *newFullCmdName, *oldFullCmdName;
  char *newName, *oldName, *name;
  Tcl_Namespace *ns, *newNs;
  Tcl_HashTable *cmdTable;
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr;

  if (objc != 3)
    return XOTclObjErrArgCnt(in, NULL, "namespace_copycmds fromNs toNs");

  ns = Tcl_FindNamespace(in, ObjStr(objv[1]), (Tcl_Namespace *)NULL, 0);
  if (!ns)
    return TCL_OK;

  newNs = Tcl_FindNamespace(in, ObjStr(objv[2]), (Tcl_Namespace *)NULL, 0);
  if (!newNs)
    return XOTclVarErrMsg(in, "CopyCmds: Destination namespace ",
			  ObjStr(objv[2]), " does not exist", (char *)NULL);
  /*
   * copy all procs & commands in the ns
   */
  cmdTable = Tcl_Namespace_cmdTable(ns);
  hPtr = Tcl_FirstHashEntry(cmdTable, &hSrch);
  while (hPtr != NULL) {
    name = Tcl_GetHashKey(cmdTable, hPtr);

    /*
     * construct full cmd names
     */
    newFullCmdName = Tcl_NewStringObj(newNs->fullName,-1);
    oldFullCmdName = Tcl_NewStringObj(ns->fullName,-1);

    INCR_REF_COUNT(newFullCmdName); INCR_REF_COUNT(oldFullCmdName);
    Tcl_AppendStringsToObj(newFullCmdName, "::", name, (char*)NULL);
    Tcl_AppendStringsToObj(oldFullCmdName, "::", name, (char*)NULL);
    newName = ObjStr(newFullCmdName);
    oldName = ObjStr(oldFullCmdName);

    /*
     * Make sure that the destination command does not already exist.
     * Otherwise: do not copy
     */
    cmd = Tcl_FindCommand(in, newName, 0, 0);

    if (cmd != NULL) {
      /*fprintf(stderr, "%s already exists\n", newName);*/
      if (!GetObject(in, newName)) {
	/* command or instproc will be deleted & than copied */
	Tcl_DeleteCommandFromToken(in, cmd);
      } else {
	/* don't overwrite objects -> will be recreated */
	hPtr = Tcl_NextHashEntry(&hSrch);
	DECR_REF_COUNT(newFullCmdName); DECR_REF_COUNT(oldFullCmdName);
	continue;
      }
    }

    /*
     * Find the existing command. An error is returned if simpleName can't
     * be found
     */
    cmd = Tcl_FindCommand(in, oldName, 0, 0);

    if (cmd == NULL) {
      Tcl_AppendStringsToObj(Tcl_GetObjResult(in), "can't copy ", " \"",
			     oldName, "\": command doesn't exist", (char *) NULL);
      DECR_REF_COUNT(newFullCmdName); DECR_REF_COUNT(oldFullCmdName);
      return TCL_ERROR;
    }
    /*
     * Do not copy Objects or Classes
     */
    if (!GetObject(in, oldName)) {
      if (TclIsProc((Command*)cmd)) {
	Proc *procPtr = TclFindProc((Interp *)in, oldName);
	Tcl_Obj *arglistObj;
	CompiledLocal *localPtr;

	/*
	 * Build a list containing the arguments of the proc
	 */

	arglistObj = Tcl_NewListObj(0, NULL);
	INCR_REF_COUNT(arglistObj);
	for (localPtr = procPtr->firstLocalPtr;  localPtr != NULL;
	     localPtr = localPtr->nextPtr) {
	  if (TclIsVarArgument(localPtr)) {
	    Tcl_Obj *defVal, *defStringObj = Tcl_NewStringObj(localPtr->name, -1);
	    INCR_REF_COUNT(defStringObj);
	    /* check for default values */
	    if ((GetProcDefault(in, cmdTable, name,
				localPtr->name, &defVal) == TCL_OK) &&
		(defVal != 0)) {
	      Tcl_AppendStringsToObj(defStringObj, " ", ObjStr(defVal), 0);
	    }
	    Tcl_ListObjAppendElement(in, arglistObj, defStringObj);
	    DECR_REF_COUNT(defStringObj);
	  }
	}
	
	if (Tcl_Command_objProc(cmd) == RUNTIME_STATE(in)->objInterpProc) {
	  Tcl_DString ds, *dsPtr = &ds;

	  if (isClassName(ns->fullName)) {
	    /* it started with ::xotcl::classes */
	    XOTclClass *cl = GetClass(in, NSCutXOTclClasses(ns->fullName));
	    XOTclProcAssertion* procs;

	    if (cl) {
	      procs = cl->opt ?
		AssertionFindProcs(cl->opt->assertions, name) : 0;
	    } else {
	      DECR_REF_COUNT(newFullCmdName);
	      DECR_REF_COUNT(oldFullCmdName);
	      DECR_REF_COUNT(arglistObj);
	      return XOTclVarErrMsg(in, "No class for inst - assertions", (char *)NULL);
	    }

	    /* XOTcl InstProc */
	    DSTRING_INIT(dsPtr);
	    Tcl_DStringAppendElement(dsPtr, NSCutXOTclClasses(newNs->fullName));
	    Tcl_DStringAppendElement(dsPtr, "instproc");
	    Tcl_DStringAppendElement(dsPtr, name);
	    Tcl_DStringAppendElement(dsPtr, ObjStr(arglistObj));
	    Tcl_DStringAppendElement(dsPtr, ObjStr(procPtr->bodyPtr));
	    if (procs) {
	      XOTclRequireClassOpt(cl);
	      AssertionAppendPrePost(in, dsPtr, procs);
	    }
	    Tcl_Eval(in, Tcl_DStringValue(dsPtr));
	    DSTRING_FREE(dsPtr);
	  } else {
	    XOTclObject *obj = GetObject(in, ns->fullName);
	    XOTclProcAssertion* procs;
	    if (obj) {
	      procs = obj->opt ?
		AssertionFindProcs(obj->opt->assertions, name) : 0;
	    } else {
	      DECR_REF_COUNT(newFullCmdName);
	      DECR_REF_COUNT(oldFullCmdName);
	      DECR_REF_COUNT(arglistObj);
	      return XOTclVarErrMsg(in, "No object for assertions", (char *)NULL);
	    }

	    /* XOTcl Proc */
	    DSTRING_INIT(dsPtr);
	    Tcl_DStringAppendElement(dsPtr, newNs->fullName);
	    Tcl_DStringAppendElement(dsPtr, "proc");
	    Tcl_DStringAppendElement(dsPtr, name);
	    Tcl_DStringAppendElement(dsPtr, ObjStr(arglistObj));
	    Tcl_DStringAppendElement(dsPtr, ObjStr(procPtr->bodyPtr));
	    if (procs) {
	      XOTclRequireObjectOpt(obj);
	      AssertionAppendPrePost(in, dsPtr, procs);
	    }
	    Tcl_Eval(in, Tcl_DStringValue(dsPtr));
	    DSTRING_FREE(dsPtr);
	  }
	  DECR_REF_COUNT(arglistObj);
	} else {
	  /* Tcl Proc */
	  Tcl_VarEval(in, "proc ", newName, " {", ObjStr(arglistObj),"} {\n",
		      ObjStr(procPtr->bodyPtr), "}", 0);
	}
      } else {
	/*
	 * Otherwise copy command
	 */
	Tcl_ObjCmdProc* objProc = Tcl_Command_objProc(cmd);
	Tcl_CmdDeleteProc *deleteProc = Tcl_Command_deleteProc(cmd);
	if (objProc) {
	  Tcl_CreateObjCommand(in, newName, objProc,
			       Tcl_Command_objClientData(cmd), deleteProc);
	} else {
	  Tcl_CreateCommand(in, newName, Tcl_Command_proc(cmd),
			    Tcl_Command_clientData(cmd), deleteProc);
	}
      }
    }
    hPtr = Tcl_NextHashEntry(&hSrch);
    DECR_REF_COUNT(newFullCmdName); DECR_REF_COUNT(oldFullCmdName);
  }
  return TCL_OK;
}

static int
XOTcl_NSCopyVars(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  Tcl_Namespace *ns, *newNs;
  Var *varPtr = 0;
  Tcl_DString ds, *dsPtr = &ds;
  Tcl_HashSearch hSrch;
  Tcl_HashEntry* hPtr;
  Tcl_HashTable *varTable;
  int rc = TCL_OK;
  char *varName;
  XOTclObject *obj;
  char *destFullName;

  if (objc != 3)
    return XOTclObjErrArgCnt(in, NULL, "namespace_copyvars fromNs toNs");

  ns = Tcl_FindNamespace(in, ObjStr(objv[1]), (Tcl_Namespace *)NULL, 0);
  if (ns) {
    newNs = Tcl_FindNamespace(in, ObjStr(objv[2]), (Tcl_Namespace *)NULL, 0);
    if (!newNs)
      return XOTclVarErrMsg(in, "CopyVars: Destination namespace ",
			    ObjStr(objv[2]), " does not exist", (char *)NULL);
    obj = GetObject(in, ns->fullName);
    varTable = Tcl_Namespace_varTable(ns);
    destFullName = newNs->fullName;
  } else {
    XOTclObject *newObj;
    obj = GetObject(in, ObjStr(objv[1]));
    if (!obj)
      return XOTclVarErrMsg(in, "CopyVars: Origin object/namespace ",
			    ObjStr(objv[1]), " does not exist", (char *)NULL);
    newObj = GetObject(in, ObjStr(objv[2]));
    if (!newObj)
      return XOTclVarErrMsg(in, "CopyVars: Destination object/namespace ",
			    ObjStr(objv[2]), " does not exist", (char *)NULL);
    varTable = obj->varTable;
    destFullName = ObjStr(newObj->cmdName);
  }

  /* copy all vars in the ns */
  hPtr = varTable ? Tcl_FirstHashEntry(varTable, &hSrch) : 0;
  while (hPtr != NULL) {
    varPtr = (Var *) Tcl_GetHashValue(hPtr);
    if (!TclIsVarUndefined(varPtr) && !TclIsVarLink(varPtr)) {
      varName = Tcl_GetHashKey(varTable, hPtr);

      if (TclIsVarScalar(varPtr)) {
	/* it may seem odd that we do not copy obj vars with the
	 * same SetVar2 as normal vars, but we want to dispatch it in order to
	 * be able to intercept the copying */
	if (obj) {
	  DSTRING_INIT(dsPtr);
	  Tcl_DStringAppendElement(dsPtr, destFullName);
	  Tcl_DStringAppendElement(dsPtr, "set");
	  Tcl_DStringAppendElement(dsPtr, varName);
	  Tcl_DStringAppendElement(dsPtr, ObjStr(varPtr->value.objPtr));
	  rc = Tcl_Eval(in, Tcl_DStringValue(dsPtr));
	  DSTRING_FREE(dsPtr);
	} else {
	  ALLOC_NAME_NS(&ds, destFullName, varName);
	  Tcl_SetVar2(in, Tcl_DStringValue(&ds), 0,
		      ObjStr(varPtr->value.objPtr), TCL_GLOBAL_ONLY);
	  DSTRING_FREE(&ds);
	}
      } else {
	if (TclIsVarArray(varPtr)) {
	  Tcl_HashTable *aTable = varPtr->value.tablePtr;
	  Tcl_HashSearch ahSrch;
	  Tcl_HashEntry* ahPtr = aTable ? Tcl_FirstHashEntry(aTable, &ahSrch) : 0;
	
	  for (; ahPtr != 0; ahPtr = Tcl_NextHashEntry(&ahSrch)) {
	    char *eltName = Tcl_GetHashKey(aTable, ahPtr);
	    Var  *eltVar  = (Var*) Tcl_GetHashValue(ahPtr);
	
	    if (TclIsVarScalar(eltVar)) {
	      if (obj) {
		Tcl_DString ds2, *ds2Ptr = &ds2;
		DSTRING_INIT(dsPtr);
		Tcl_DStringAppendElement(dsPtr, destFullName);
		Tcl_DStringAppendElement(dsPtr, "set");
		DSTRING_INIT(ds2Ptr);
		Tcl_DStringAppend(ds2Ptr, varName, -1);
		Tcl_DStringAppend(ds2Ptr, "(", 1);
		Tcl_DStringAppend(ds2Ptr, eltName, -1);
		Tcl_DStringAppend(ds2Ptr, ")", 1);
		Tcl_DStringAppendElement(dsPtr, Tcl_DStringValue(ds2Ptr));
		Tcl_DStringAppendElement(dsPtr, ObjStr(eltVar->value.objPtr));
		rc = Tcl_Eval(in, Tcl_DStringValue(dsPtr));
		DSTRING_FREE(dsPtr);
		DSTRING_FREE(ds2Ptr);
	      } else {
		ALLOC_NAME_NS(&ds, destFullName, varName);
		Tcl_SetVar2(in, Tcl_DStringValue(&ds), eltName,
			    ObjStr(eltVar->value.objPtr), TCL_GLOBAL_ONLY);
		DSTRING_FREE(&ds);
	      }
	    }
	  }
	}
      }
    }
    hPtr = Tcl_NextHashEntry(&hSrch);
  }
  return rc;
}

int
XOTclSelfDispatchCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  XOTclObject *self;
  int result;
  if ((self = GetSelfObj(in))) {
    result = callMethod((ClientData)self, in, objv[1], objc, objv+2, 0);
  } else {
    result = XOTclVarErrMsg(in, "Cannot resolve 'self', probably called outside the context of an XOTcl Object",
			    (char*)NULL);
  }
  return result;
}

int
XOTclInitProcNSCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *CONST objv[]) {
  Tcl_CallFrame *varFramePtr = Tcl_Interp_varFramePtr(in);

  /*RUNTIME_STATE(in)->varFramePtr = varFramePtr;*/
  RUNTIME_STATE(in)->cs.top->currentFramePtr = varFramePtr;
  if (varFramePtr) {
    Tcl_CallFrame *callerVarPtr = Tcl_CallFrame_callerVarPtr(varFramePtr);
    if (callerVarPtr) {
      varFramePtr->nsPtr = (Tcl_Namespace *)callerVarPtr->nsPtr;
    } else {
      varFramePtr->nsPtr = Tcl_Interp_globalNsPtr(in);
    }
  }
  return TCL_OK;
}



/* create a slave interp that calls XOTcl Init */
static int
XOTcl_InterpObjCmd(ClientData cd, Tcl_Interp *in, int objc, Tcl_Obj *objv[]) {
  Tcl_Interp *slave;
  Tcl_Obj *saved;
  char *subCmd;

  if (objc < 1)
      return XOTclObjErrArgCnt(in, NULL, "::xotcl::interp name ?args?");

  saved = objv[0];
  objv[0] = XOTclGlobalObjects[INTERP];
  if (Tcl_EvalObjv(in, objc, objv, TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) != TCL_OK) {
    objv[0] = saved;
    return TCL_ERROR;
  }
  objv[0] = saved;

  subCmd = ObjStr(objv[1]);
  if (isCreateString(subCmd)) {
    slave = Tcl_GetSlave(in, ObjStr(objv[2]));
    if (!slave)
      return XOTclVarErrMsg(in, "Creation of slave interpreter failed", (char*)NULL);
    if (Xotcl_Init(slave) == TCL_ERROR) {
      return TCL_ERROR;
    }
#ifdef XOTCL_MEM_COUNT
    xotclMemCountInterpCounter++;
#endif
  }

  return TCL_OK;
}


extern Tcl_Obj*
XOTclOGetInstVar2(XOTcl_Object *obj, Tcl_Interp *in, Tcl_Obj *name1, Tcl_Obj *name2,
		  int flgs) {
  Tcl_Obj *result;
  XOTcl_FrameDecls;

  XOTcl_PushFrame(in, obj);
  if (obj->nsPtr)
    flgs |= TCL_NAMESPACE_ONLY;

  result = Tcl_ObjGetVar2(in, name1, name2, flgs);
  XOTcl_PopFrame(in, obj);

  return result;
}

/*
 * get all instances of a class recursively to an initialized
 * String key hashtable
 */
static void
getAllInstances(Tcl_HashTable *destTable, XOTclClass *startCl) {
  Tcl_HashTable *table = &startCl->instances;
  Tcl_HashSearch search;
  Tcl_HashEntry *hPtr;

  for (hPtr = Tcl_FirstHashEntry(table, &search);  hPtr != NULL;
       hPtr = Tcl_NextHashEntry(&search)) {
    XOTclObject *inst = (XOTclObject*) Tcl_GetHashKey(table, hPtr);
    Tcl_HashEntry *hPtrDest;
    int new;
    hPtrDest = Tcl_CreateHashEntry(destTable, ObjStr(inst->cmdName), &new);
    /*
    fprintf (stderr, " -- %s (%s)\n", ObjStr(inst->cmdName), ObjStr(startCl->object.cmdName));
    */
    if (new && XOTclObjectIsClass(inst)) {
      getAllInstances(destTable, (XOTclClass*) inst);
    }
  }
}
#if !defined(NDEBUG)
static void
checkAllInstances(Tcl_Interp *in, XOTclClass *cl, int lvl) {
  Tcl_HashSearch search;
  Tcl_HashEntry *hPtr;
  if (cl && cl->object.refCount>0) {
    /*fprintf(stderr,"checkallinstances %d cl=%p '%s'\n", lvl, cl, ObjStr(cl->object.cmdName));*/
    for (hPtr = Tcl_FirstHashEntry(&cl->instances, &search);  hPtr != NULL;
	 hPtr = Tcl_NextHashEntry(&search)) {
      XOTclObject *inst = (XOTclObject*) Tcl_GetHashKey(&cl->instances, hPtr);
      assert(inst);
      assert(inst->refCount>0);
      assert(inst->cmdName->refCount>0);
      if (XOTclObjectIsClass(inst) && (XOTclClass*)inst != RUNTIME_STATE(in)->theClass) {
	checkAllInstances(in, (XOTclClass*) inst, lvl+1);
      }
    }
  }
}
#endif

#ifdef DO_FULL_CLEANUP
/* delete global variables and procs */
static void deleteProcsAndVars(Tcl_Interp *in) {
  Tcl_Namespace *ns = Tcl_GetGlobalNamespace(in);
  Tcl_HashTable *varTable = ns ? Tcl_Namespace_varTable(ns) : NULL;
  Tcl_HashTable *cmdTable = ns ? Tcl_Namespace_cmdTable(ns) : NULL;
  Tcl_HashSearch search;
  Var *varPtr;
  Tcl_Command cmd;
  register Tcl_HashEntry *entryPtr;
  char *varName;

  entryPtr = Tcl_FirstHashEntry(varTable, &search);
  while (entryPtr != NULL) {
    varPtr = (Var *) Tcl_GetHashValue(entryPtr);
    if (!TclIsVarUndefined(varPtr) || (varPtr->flags & VAR_NAMESPACE_VAR)) {
      varName = Tcl_GetHashKey(varTable, entryPtr);
      /* fprintf(stderr, "unsetting var %s\n", varName);*/
      Tcl_UnsetVar2(in, varName, (char *) NULL, TCL_GLOBAL_ONLY);
    }
    entryPtr = Tcl_NextHashEntry(&search);
  }

  entryPtr = Tcl_FirstHashEntry(cmdTable, &search);
  while (entryPtr) {
    cmd = (Tcl_Command)Tcl_GetHashValue(entryPtr);
    if (Tcl_Command_proc(cmd) == RUNTIME_STATE(in)->interpProc) {
      /*
	char *key = Tcl_GetHashKey(cmdTable, entryPtr);
	fprintf(stderr, "cmdname = %s cmd %p proc %p objProc %p %d\n",
		key,cmd,cmd->proc,cmd->objProc,
		cmd->proc==RUNTIME_STATE(in)->interpProc);

	*/
      Tcl_DeleteCommandFromToken(in, cmd);
    }
    entryPtr = Tcl_NextHashEntry(&search);
  }
}
#endif


#ifdef DO_CLEANUP
static int
ClassHasSubclasses(XOTclClass *cl) {
  return (cl->sub != NULL);
}

static int
ClassHasInstances(XOTclClass *cl) {
  Tcl_HashSearch hSrch;
  return (Tcl_FirstHashEntry(&cl->instances, &hSrch) != NULL);
}

static int
ObjectHasChildren(Tcl_Interp *in, XOTclObject *obj) {
  Tcl_Namespace *ns = obj->nsPtr;
  int result = 0;

  if (ns) {
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch hSrch;
    Tcl_HashTable *cmdTable = Tcl_Namespace_cmdTable(ns);
    XOTcl_FrameDecls;

    XOTcl_PushFrame(in, obj);
    for (hPtr = Tcl_FirstHashEntry(cmdTable, &hSrch); hPtr;
	 hPtr = Tcl_NextHashEntry(&hSrch)) {
      char *key = Tcl_GetHashKey(cmdTable, hPtr);
      if (GetObject(in, key)) {
	/*fprintf(stderr,"child = %s\n",key);*/
	result = 1;
	break;
      }
    }
    XOTcl_PopFrame(in,obj);
  }
  return result;
}

static void freeAllXOTclObjectsAndClasses(Tcl_Interp *in, Tcl_HashTable *commandTable) {
  Tcl_HashEntry *hPtr, *hDel;
  Tcl_HashSearch hSrch;
  XOTclObject *obj;
  XOTclClass  *thecls, *theobj, *cl;

  /*fprintf(stderr,"??? freeAllXOTclObjectsAndClasses in %p\n", in);*/

  thecls = RUNTIME_STATE(in)->theClass;
  theobj = RUNTIME_STATE(in)->theObject;
  /***** PHYSICAL DESTROY *****/
  RUNTIME_STATE(in)->exitHandlerDestroyRound = XOTCL_EXITHANDLER_ON_PHYSICAL_DESTROY;
  while (1) {
    int deleted = 0;
    hPtr = Tcl_FirstHashEntry(commandTable, &hSrch);
    while (hPtr) {
      char *key = Tcl_GetHashKey(commandTable, hPtr);
      obj = GetObject(in, key);
      if (obj && !XOTclObjectIsClass(obj) && !ObjectHasChildren(in,obj)) {
	/*fprintf(stderr,"  ... delete object %s %p\n",key,obj);*/
	Tcl_DeleteCommandFromToken(in, obj->id);
	hDel = hPtr;
	deleted++;
      } else {
	hDel = NULL;
      }
      hPtr = Tcl_NextHashEntry(&hSrch);
      if (hDel)
	Tcl_DeleteHashEntry(hDel);
    }
    /*fprintf(stderr, "deleted %d Objects\n",deleted);*/
    if (deleted>0)
      continue;


    hPtr = Tcl_FirstHashEntry(commandTable, &hSrch);
    while (hPtr) {
      char *key = Tcl_GetHashKey(commandTable, hPtr);
      cl = GetClass(in, key);
      /*fprintf(stderr,"cl key = %s %p\n", key, cl);*/
      if (cl
	  && !ObjectHasChildren(in, (XOTclObject*)cl)
	  && !ClassHasInstances(cl)
	  && !ClassHasSubclasses(cl)
	  && cl != RUNTIME_STATE(in)->theClass
	  && cl != RUNTIME_STATE(in)->theObject
	  ) {
	/*fprintf(stderr,"  ... delete class %s %p\n",key,cl);*/
	Tcl_DeleteCommandFromToken(in, cl->object.id);
	hDel = hPtr;
	deleted++;
      } else {
	hDel = NULL;
      }
      hPtr = Tcl_NextHashEntry(&hSrch);
      if (hDel)
	Tcl_DeleteHashEntry(hDel);
    }
    /*fprintf(stderr, "deleted %d Classes\n",deleted);*/
    if (deleted>0)
      continue;

    break;

  }

#ifdef DO_FULL_CLEANUP
  deleteProcsAndVars(in);
#endif

  RUNTIME_STATE(in)->callDestroy = 0;
  RemoveSuper(thecls, theobj);
  RemoveInstance((XOTclObject*)thecls, thecls);
  RemoveInstance((XOTclObject*)theobj, thecls);

  Tcl_DeleteCommandFromToken(in, theobj->object.id);
  RUNTIME_STATE(in)->theObject = NULL;

  Tcl_DeleteCommandFromToken(in, thecls->object.id);
  RUNTIME_STATE(in)->theClass = NULL;

  XOTcl_DeleteNamespace(in, RUNTIME_STATE(in)->fakeNS);
  XOTcl_DeleteNamespace(in, RUNTIME_STATE(in)->XOTclClassesNS);
  XOTcl_DeleteNamespace(in, RUNTIME_STATE(in)->XOTclNS);

}
#endif /* DO_CLEANUP */


/*
 *  Exit Handler
 */
static void
ExitHandler(ClientData cd) {
  Tcl_Interp *in = (Tcl_Interp *) cd;
  XOTclObject *obj;
  XOTclClass *cl;
  int result, flags, i;
  Tcl_HashSearch hSrch;
  Tcl_HashEntry *hPtr;
  Tcl_HashTable objTable, *commandTable = &objTable;
  XOTclCallStack *cs = &RUNTIME_STATE(in)->cs;

  /*
   * Don't use exit handler, if the interpreted is destroyed already
   * Call to exit handler comes after freeing namespaces, commands, etc.
   * e.g. TK calls Tcl_DeleteInterp directly, if Window is killed
   */

  /*
   * Ahem ...
   *
   * Since we *must* be sure that our destroy methods will run
   * we must *cheat* (I mean CHEAT) here: we flip the interp
   * flag, saying, "hey boy, you're not deleted any more".
   * After our handlers are done, we restore the old state...
   * All this is needed so we can do an eval in the interp which
   * is potentially marked for delete when we start working here.
   *
   * I know, I know, this is not really elegant. But...  I'd need a
   * standard way of invoking some code at interpreter delete time
   * but JUST BEFORE the actual deletion process starts. Sadly,
   * there is no such hook in Tcl as of Tcl8.3.2, that I know of.
   *
   * So, for the rest of procedure, assume the interp is alive !
   */

  /*fprintf(stderr,"+++ EXIT handler\n"); */
  flags = Tcl_Interp_flags(in);
  Tcl_Interp_flags(in) &= ~DELETED;
#if defined(PROFILE)
  XOTclProfilePrintData(in);
#endif
  /*
   * evaluate user-defined exit handler
   */
  result = callMethod((ClientData)RUNTIME_STATE(in)->theObject, in,
		      XOTclGlobalObjects[EXIT_HANDLER], 2, 0, 0);
  if (result != TCL_OK) {
    fprintf(stderr,"User defined exit handler contains errors!\n"
            "Error in line %d: %s\nExecution interrupted.\n",
            in->errorLine, ObjStr(Tcl_GetObjResult(in)));
  }
  /*
   * Pop any callstack entry that is still alive (e.g.
   * if "exit" is called and we were jumping out of the
   * callframe
   */
  while (cs->top > cs->content)
    CallStackPop(in);

  while (Tcl_Interp_framePtr(in))
    Tcl_PopCallFrame(in);
  /*
   * deleting in two rounds:
   *  (a) SOFT DESTROY: call all user-defined destroys
   *  (b) PHYSICAL DESTROY: delete the commands, user-defined
   *      destroys are not executed anymore
   *
   * this is to prevent user-defined destroys from overriding physical
   * destroy during exit handler, but still ensure that all
   * user-defined destroys are called.
   */

  Tcl_InitHashTable(commandTable, TCL_STRING_KEYS);
  MEM_COUNT_ALLOC("Tcl_InitHashTable",commandTable);
  getAllInstances(commandTable, RUNTIME_STATE(in)->theClass);
  /***** SOFT DESTROY *****/
  RUNTIME_STATE(in)->exitHandlerDestroyRound = XOTCL_EXITHANDLER_ON_SOFT_DESTROY;

  hPtr = Tcl_FirstHashEntry(commandTable, &hSrch);
  while (hPtr) {
    char *key = Tcl_GetHashKey(commandTable, hPtr);
    obj = GetObject(in, key);
    /*fprintf(stderr,"key = %s %p %d\n",
      key, obj, obj && !XOTclObjectIsClass(obj));*/
    if (obj && !XOTclObjectIsClass(obj)
	&& !(obj->flags & XOTCL_DESTROY_CALLED))
      callDestroyMethod((ClientData)obj, in, obj, 0);
    hPtr = Tcl_NextHashEntry(&hSrch);
  }
  hPtr = Tcl_FirstHashEntry(commandTable, &hSrch);
  while (hPtr) {
    char *key = Tcl_GetHashKey(commandTable, hPtr);
    cl = GetClass(in, key);
    if (cl
	&& !(cl->object.flags & XOTCL_DESTROY_CALLED))
      callDestroyMethod((ClientData)cl, in, (XOTclObject*)cl, 0);
    hPtr = Tcl_NextHashEntry(&hSrch);
  }
#ifdef DO_CLEANUP
  freeAllXOTclObjectsAndClasses(in, commandTable);
#endif

  /* must be before freeing of XOTclGlobalObjects */
  XOTclShadowTclCommands(in, SHADOW_UNLOAD);
  /* free global objects */
  for (i = 0; i < nr_elements(XOTclGlobalStrings); i++) {
    DECR_REF_COUNT(XOTclGlobalObjects[i]);
  }
  XOTclStringIncrFree(&RUNTIME_STATE(in)->iss);
  FREE(Tcl_Obj**, XOTclGlobalObjects);
  FREE(XOTclRuntimeState, RUNTIME_STATE(in));

  MEM_COUNT_FREE("Tcl_InitHashTable",commandTable);
  Tcl_DeleteHashTable(commandTable);

#if defined(TCL_MEM_DEBUG)
  TclDumpMemoryInfo (stderr);
  Tcl_DumpActiveMemory ("./xotclActiveMem");
  /* Tcl_GlobalEval(in, "puts {checkmem to checkmemFile};
     checkmem checkmemFile"); */
#endif
  MEM_COUNT_DUMP();

  Tcl_Interp_flags(in) = flags;
  Tcl_Release((ClientData) in);
}


#if defined(TCL_THREADS)
/*
 * Gets activated at thread-exit
 */
static void
XOTcl_ThreadExitProc(ClientData cd) {
  /*fprintf(stderr,"+++ XOTcl_ThreadExitProc\n");*/
#if !defined(PRE83)
  static void XOTcl_ExitProc(ClientData cd);
  Tcl_DeleteExitHandler(XOTcl_ExitProc, cd);
#endif
  ExitHandler(cd);
}
#endif

/*
 * Gets activated at application-exit
 */
static void
XOTcl_ExitProc(ClientData cd) {
  /*fprintf(stderr,"+++ XOTcl_ExitProc\n");*/
#if !defined(PRE83) && defined(TCL_THREADS)
  Tcl_DeleteThreadExitHandler(XOTcl_ThreadExitProc, cd);
#endif
  ExitHandler(cd);
}


/*
 * Registers thread/appl exit handlers.
 */
static void
RegisterExitHandlers(ClientData cd) {
  Tcl_Preserve(cd);
#if !defined(PRE83) && defined(TCL_THREADS)
  Tcl_CreateThreadExitHandler(XOTcl_ThreadExitProc, cd);
#endif
  Tcl_CreateExitHandler(XOTcl_ExitProc, cd);
}



/*
 * Tcl extension initialization routine
 */

extern int
Xotcl_Init(Tcl_Interp *in) {
  XOTclClass *theobj = 0;
  XOTclClass *thecls = 0;
  XOTclClass *paramCl = 0;
  ClientData runtimeState;
  int result, i;
#ifdef XOTCL_BYTECODE
  XOTclCompEnv *instructions = XOTclGetCompEnv();
#endif

#ifndef PRE81
# ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(in, "8.1", 0) == NULL) {
    return TCL_ERROR;
  }
# endif
#endif

#if defined(TCL_MEM_DEBUG)
  TclDumpMemoryInfo(stderr);
#endif

  MEM_COUNT_INIT();

  /*
  fprintf(stderr, "SIZES: obj=%d, tcl_obj=%d, DString=%d, class=%d, namespace=%d, command=%d, HashTable=%d\n", sizeof(XOTclObject), sizeof(Tcl_Obj), sizeof(Tcl_DString), sizeof(XOTclClass), sizeof(Namespace), sizeof(Command), sizeof(Tcl_HashTable));
  */

  /*
   * Runtime State stored in the client data of the Interp's global
   * Namespace in order to avoid global state information
   */
  runtimeState = (ClientData) NEW(XOTclRuntimeState);
#if USE_ASSOC_DATA
  Tcl_SetAssocData(in, "XOTclRuntimeState", NULL, runtimeState);
#else
  Tcl_Interp_globalNsPtr(in)->clientData = runtimeState;
#endif

  /* CallStack initialization */
  memset(RUNTIME_STATE(in), 0, sizeof(XOTclRuntimeState));
  memset(RUNTIME_STATE(in)->cs.content, 0, sizeof(XOTclCallStackContent));

  RUNTIME_STATE(in)->cs.top = RUNTIME_STATE(in)->cs.content;
  RUNTIME_STATE(in)->callDestroy = 1;

  /* create xotcl namespace */
  RUNTIME_STATE(in)->XOTclNS =
    Tcl_CreateNamespace(in, "::xotcl", (ClientData)NULL, (Tcl_NamespaceDeleteProc*)NULL);

  MEM_COUNT_ALLOC("TclNamespace",RUNTIME_STATE(in)->XOTclNS);

  /*
   * init an empty, faked proc structure in the RUNTIME state
   */
  RUNTIME_STATE(in)->fakeProc.iPtr = (Interp *)in;
  RUNTIME_STATE(in)->fakeProc.refCount = 1;
  RUNTIME_STATE(in)->fakeProc.cmdPtr = NULL;
  RUNTIME_STATE(in)->fakeProc.bodyPtr = NULL;
  RUNTIME_STATE(in)->fakeProc.numArgs  = 0;
  RUNTIME_STATE(in)->fakeProc.numCompiledLocals = 0;
  RUNTIME_STATE(in)->fakeProc.firstLocalPtr = NULL;
  RUNTIME_STATE(in)->fakeProc.lastLocalPtr = NULL;
  RUNTIME_STATE(in)->fakeNS =
    Tcl_CreateNamespace(in, "::xotcl::fakeNS", (ClientData)NULL,
			(Tcl_NamespaceDeleteProc*)NULL);
  MEM_COUNT_ALLOC("TclNamespace",RUNTIME_STATE(in)->fakeNS);

  /* XOTclClasses in separate Namespace / Objects */
  RUNTIME_STATE(in)->XOTclClassesNS =
    Tcl_CreateNamespace(in, "::xotcl::classes",	(ClientData)NULL,
			(Tcl_NamespaceDeleteProc*)NULL);
  MEM_COUNT_ALLOC("TclNamespace",RUNTIME_STATE(in)->XOTclClassesNS);


  /* cache interpreters proc interpretation functions */
  RUNTIME_STATE(in)->objInterpProc = TclGetObjInterpProc();
  RUNTIME_STATE(in)->interpProc = TclGetInterpProc();
  RUNTIME_STATE(in)->exitHandlerDestroyRound = XOTCL_EXITHANDLER_OFF;

  RegisterObjTypes();
  RegisterExitHandlers((ClientData)in);

  XOTclStringIncrInit(&RUNTIME_STATE(in)->iss);

  /* initialize global Tcl_Obj*/
  XOTclGlobalObjects = NEW_ARRAY(Tcl_Obj*,nr_elements(XOTclGlobalStrings));

  for (i = 0; i < nr_elements(XOTclGlobalStrings); i++) {
    XOTclGlobalObjects[i] = Tcl_NewStringObj(XOTclGlobalStrings[i],-1);
    INCR_REF_COUNT(XOTclGlobalObjects[i]);
  }

  /* create Object and Class, and store them in the RUNTIME STATE */
  theobj = PrimitiveCCreate(in, "::xotcl::Object", 0);
  RUNTIME_STATE(in)->theObject = theobj;
  if (!theobj) panic("Cannot create ::xotcl::Object",0);

  thecls = PrimitiveCCreate(in, "::xotcl::Class", 0);
  RUNTIME_STATE(in)->theClass = thecls;
  if (!thecls) panic("Cannot create ::xotcl::Class",0);

  theobj->parent = 0;
  thecls->parent = theobj;

  Tcl_Export(in, RUNTIME_STATE(in)->XOTclNS, "Object", 0);
  Tcl_Export(in, RUNTIME_STATE(in)->XOTclNS, "Class", 0);
  /*Tcl_AddInterpResolvers(in, "XOTcl", XOTclResolveCmd, 0, 0);*/

#ifdef XOTCL_BYTECODE
  instructions[INST_SELF].cmdPtr = (Command *)
#endif
  Tcl_CreateObjCommand(in, "::xotcl::self", XOTclGetSelfObjCmd, 0, 0);
#ifdef XOTCL_BYTECODE
  instructions[INST_NEXT].cmdPtr = (Command *)
#endif
  Tcl_CreateObjCommand(in, "::xotcl::next", (Tcl_ObjCmdProc*) XOTclNextObjCmd, 0, 0);
  Tcl_Export(in, RUNTIME_STATE(in)->XOTclNS, "self", 0);
  Tcl_Export(in, RUNTIME_STATE(in)->XOTclNS, "next", 0);
  Tcl_Export(in, RUNTIME_STATE(in)->XOTclNS, "my", 0);

#if defined(PROFILE)
  XOTclProfileInit(in);
#endif

  /* test Object and Class creation */
  if (!theobj || !thecls) {
    RUNTIME_STATE(in)->callDestroy = 0;

    if (thecls) PrimitiveCDestroy((ClientData) thecls);
    if (theobj) PrimitiveCDestroy((ClientData) theobj);

    for (i = 0; i < nr_elements(XOTclGlobalStrings); i++) {
      DECR_REF_COUNT(XOTclGlobalObjects[i]);
    }
    FREE(Tcl_Obj**, XOTclGlobalObjects);
    FREE(XOTclRuntimeState, RUNTIME_STATE(in));

    return XOTclErrMsg(in, "Object/Class failed", TCL_STATIC);
  }

  AddInstance((XOTclObject*)theobj, thecls);
  AddInstance((XOTclObject*)thecls, thecls);
  AddSuper(thecls, theobj);

  /*
   * and fill them with functionality
   */
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "autoname", XOTclOAutonameMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "check", (Tcl_ObjCmdProc*)XOTclOCheckMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "class", XOTclOClassMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "cleanup", XOTclOCleanupMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "configure", (Tcl_ObjCmdProc*) XOTclOConfigureMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "destroy", XOTclODestroyMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "exists", (Tcl_ObjCmdProc*)XOTclOExistsMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "filter", (Tcl_ObjCmdProc*)XOTclOFilterMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "filterguard", (Tcl_ObjCmdProc*)XOTclOFilterGuardMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "filtersearch", (Tcl_ObjCmdProc*)XOTclOFilterSearchMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "incr", (Tcl_ObjCmdProc*)XOTclOIncrMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "info", XOTclOInfoMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "instvar", (Tcl_ObjCmdProc*)XOTclOInstVarMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "invar", (Tcl_ObjCmdProc*)XOTclOInvariantsMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "isclass", XOTclOIsClassMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "ismetaclass", XOTclOIsMetaClassMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "isobject", XOTclOIsObjectMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "istype", XOTclOIsTypeMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "ismixin", XOTclOIsMixinMethod, 0, 0);
#ifdef XOTCL_METADATA
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "metadata", (Tcl_ObjCmdProc*)XOTclOMetaDataMethod, 0, 0);
#endif
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "mixin", (Tcl_ObjCmdProc*)XOTclOMixinMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "mixinguard", (Tcl_ObjCmdProc*)XOTclOMixinGuardMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "__next", (Tcl_ObjCmdProc*)XOTclONextMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "noinit", (Tcl_ObjCmdProc*)XOTclONoinitMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "parametercmd", (Tcl_ObjCmdProc*)XOTclCParameterCmdMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "proc", XOTclOProcMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "procsearch", (Tcl_ObjCmdProc*)XOTclOProcSearchMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "requireNamespace", (Tcl_ObjCmdProc*)XOTclORequireNamespaceMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "set", (Tcl_ObjCmdProc*)XOTclOSetMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "tclcmd", (Tcl_ObjCmdProc*)XOTclCTclCmdMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "unset", XOTclOUnsetMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "uplevel", XOTclOUplevelMethod, 0,0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "upvar", XOTclOUpvarMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "volatile", (Tcl_ObjCmdProc*)XOTclCVolatileMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) theobj, "vwait", (Tcl_ObjCmdProc*)XOTclOVwaitMethod, 0, 0);



  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "alloc", (Tcl_ObjCmdProc*)XOTclCAllocMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "create", (Tcl_ObjCmdProc*)XOTclCCreateMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "new", (Tcl_ObjCmdProc*)XOTclCNewMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "info", XOTclCInfoMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "instdestroy", XOTclCInstDestroyMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "instfilter", (Tcl_ObjCmdProc*)XOTclCInstFilterMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "instfilterguard", (Tcl_ObjCmdProc*)XOTclCInstFilterGuardMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "instinvar", (Tcl_ObjCmdProc*)XOTclCInvariantsMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "instmixin", (Tcl_ObjCmdProc*)XOTclCInstMixinMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "instmixinguard", (Tcl_ObjCmdProc*)XOTclCInstMixinGuardMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "instparametercmd", (Tcl_ObjCmdProc*)XOTclCInstParameterCmdMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "instproc", (Tcl_ObjCmdProc*)XOTclCInstProcMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "insttclcmd", (Tcl_ObjCmdProc*)XOTclCInstTclCmdMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "parameter", (Tcl_ObjCmdProc*)XOTclCParameterMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "parameterclass", (Tcl_ObjCmdProc*)XOTclCParameterClassMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "recreate", (Tcl_ObjCmdProc*) XOTclCRecreateMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "superclass", (Tcl_ObjCmdProc*)XOTclCSuperClassMethod, 0, 0);
  XOTclAddIMethod(in, (XOTcl_Class*) thecls, "unknown", (Tcl_ObjCmdProc*) XOTclCUnknownMethod, 0, 0);

  /*
   * overwritten tcl objs
   */
  result = XOTclShadowTclCommands(in, SHADOW_LOAD);
  if (result != TCL_OK)
    return result;

  /*
   * new tcl cmds
   */
  Tcl_CreateObjCommand(in, "::xotcl::interp", (Tcl_ObjCmdProc*)XOTcl_InterpObjCmd, 0, 0);
  Tcl_CreateObjCommand(in, "::xotcl::trace", XOTcl_TraceObjCmd, 0, 0);
  Tcl_CreateObjCommand(in, "::xotcl::namespace_copyvars", XOTcl_NSCopyVars, 0, 0);
  Tcl_CreateObjCommand(in, "::xotcl::namespace_copycmds", XOTcl_NSCopyCmds, 0, 0);
  Tcl_CreateObjCommand(in, "::xotcl::deprecated", XOTcl_DeprecatedCmd, 0, 0);
#ifdef XOTCL_BYTECODE
  instructions[INST_INITPROC].cmdPtr = (Command *)
#endif
  Tcl_CreateObjCommand(in, "::xotcl::initProcNS", XOTclInitProcNSCmd, 0, 0);
#ifdef XOTCL_BYTECODE
  instructions[INST_SELF_DISPATCH].cmdPtr = (Command *)
#endif
  Tcl_CreateObjCommand(in, "::xotcl::my", XOTclSelfDispatchCmd, 0, 0);

#ifdef XOTCL_BYTECODE
  XOTclBytecodeInit();
#endif

  /*
   *  Parameter Class
   */
  paramCl = PrimitiveCCreate(in, XOTclGlobalStrings[PARAM_CL], thecls);
  XOTclAddPMethod(in, (XOTcl_Object*) &paramCl->object,
		  XOTclGlobalStrings[SEARCH_DEFAULTS],
		  (Tcl_ObjCmdProc*) ParameterSearchDefaultsMethod, 0, 0);
  /*
   * set runtime version information in Tcl variable
   */
  Tcl_SetVar(in, "::xotcl::version", XOTCLVERSION, TCL_GLOBAL_ONLY);
  Tcl_SetVar(in, "::xotcl::patchlevel", XOTCLPATCHLEVEL, TCL_GLOBAL_ONLY);

  /*
   * with some methods and library procs in tcl - they could go in a
   * xotcl.tcl file, but they're embedded here with Tcl_GlobalEval
   * to avoid the need to carry around a separate file at runtime.
   */
  {

#include "predefined.h"

    /* fprintf(stderr, "predefined=<<%s>>\n",cmd);*/
    if (Tcl_GlobalEval(in, cmd) != TCL_OK) 
      return TCL_ERROR;
  }

#ifndef AOL_SERVER
  /* the AOL server uses a different package loading mechanism */
# ifdef COMPILE_XOTCL_STUBS
  Tcl_PkgProvideEx(in, "XOTcl", XOTCLVERSION, (ClientData) &xotclStubs);
# else
  Tcl_PkgProvide(in, "XOTcl", XOTCLVERSION);
# endif
#endif

#if !defined(TCL_THREADS) && !defined(PRE81)
  if ((Tcl_GetVar2(in, "tcl_platform", "threaded", TCL_GLOBAL_ONLY) != NULL)) {
    /* a non threaded XOTcl version is loaded into a threaded environment */
    Tcl_Panic("\n A non threaded XOTCL version is loaded into threaded environment\n Please reconfigure XOTcl with --enable-threads!\n");
  }
#endif

  Tcl_ResetResult(in);
  Tcl_SetIntObj(Tcl_GetObjResult(in), 1);

  return TCL_OK;
}

#ifdef NEVER
extern int
Xotcl_SafeInit(interp)
    Tcl_Interp *interp;
{
  /*** dummy for now **/
  return Xotcl_Init(interp);
}
#endif
