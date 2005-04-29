/* -*- Mode: c++ -*-
 *  $Id: s.xotclInt.h 1.17 01/10/11 18:37:46+02:00 neumann@mohegan.wu-wien.ac.at $
 *  Extended Object Tcl (XOTcl)
 *
 *  Copyright (C) 1999-2002 Gustaf Neumann, Uwe Zdun
 *
 *  xotclInt.h --
 *
 *  Mostly internally used API Functions
 */

#ifndef _xotcl_int_h_
#define _xotcl_int_h_

#include <tclInt.h>
#include "xotcl.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(PROFILE)
#  include <sys/time.h>
#endif

#ifdef DMALLOC
#  include "dmalloc.h"
#endif

#ifdef BUILD_xotcl
# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLEXPORT
#endif

/*
#define XOTCL_METADATA
*/

/*
 * Makros
 */
#ifdef XOTCL_MEM_COUNT
Tcl_HashTable xotclMemCount; 
extern int xotclMemCountInterpCounter;
typedef struct XOTclMemCounter {
  int peak;
  int count;
} XOTclMemCounter;
#  define MEM_COUNT_ALLOC(id,p) XOTclMemCountAlloc(id,p)
#  define MEM_COUNT_FREE(id,p) XOTclMemCountFree(id,p)
#  define MEM_COUNT_INIT() \
      if (xotclMemCountInterpCounter == 0) { \
        Tcl_InitHashTable(&xotclMemCount, TCL_STRING_KEYS); \
        xotclMemCountInterpCounter = 1; \
      }
#  define MEM_COUNT_DUMP() XOTclMemCountDump(in)
#  define MEM_COUNT_OPEN_FRAME() 
/*if (obj->varTable) noTableBefore = 0*/
#  define MEM_COUNT_CLOSE_FRAME() 
/*      if (obj->varTable && noTableBefore) \
	XOTclMemCountAlloc("obj->varTable",NULL)*/
#else
#  define MEM_COUNT_ALLOC(id,p)
#  define MEM_COUNT_FREE(id,p)
#  define MEM_COUNT_INIT()
#  define MEM_COUNT_DUMP()
#  define MEM_COUNT_OPEN_FRAME()
#  define MEM_COUNT_CLOSE_FRAME()
#endif

#define DSTRING_INIT(D) Tcl_DStringInit(D); MEM_COUNT_ALLOC("DString",D)
#define DSTRING_FREE(D) Tcl_DStringFree(D); MEM_COUNT_FREE("DString",D)

#if USE_ASSOC_DATA
# define RUNTIME_STATE(in) ((XOTclRuntimeState*) Tcl_GetAssocData(in, "XOTclRuntimeState", NULL))
#else
# define RUNTIME_STATE(in) \
    ((XOTclRuntimeState*)((Interp*) in)->globalNsPtr->clientData)
#endif


#define ALLOC_NAME_NS(DSP, NS, NAME) \
     DSTRING_INIT(DSP);\
     Tcl_DStringAppend(DSP, NS, -1),\
     Tcl_DStringAppend(DSP, "::", 2),\
     Tcl_DStringAppend(DSP, NAME, -1)
#define ALLOC_TOP_NS(DSP, NAME) \
     DSTRING_INIT(DSP);\
     Tcl_DStringAppend(DSP, "::", 2),\
     Tcl_DStringAppend(DSP, NAME, -1)
#define ALLOC_DSTRING(DSP,ENTRY) \
     DSTRING_INIT(DSP);\
     Tcl_DStringAppend(DSP, ENTRY, -1)

#define nr_elements(arr)  ((int) (sizeof(arr) / sizeof(arr[0])))

# define NEW(type) \
	(type *)ckalloc(sizeof(type)); MEM_COUNT_ALLOC(#type, NULL)
# define NEW_ARRAY(type,n) \
	(type *)ckalloc(sizeof(type)*(n)); MEM_COUNT_ALLOC(#type "*", NULL)
# define FREE(type, var) \
	ckfree((char*) var); MEM_COUNT_FREE(#type,var)

#define isArgsString(m) (\
	*m   == 'a' && m[1] == 'r' && m[2] == 'g' && m[3] == 's' && \
	m[4] == '\0')
#define isBodyString(m) (\
	*m   == 'b' && m[1] == 'o' && m[2] == 'd' && m[3] == 'y' && \
	m[4] == '\0')
#define isClassString(m) (\
	*m   == 'c' && m[1] == 'l' && m[2] == 'a' && m[3] == 's' && \
	m[4] == 's' && m[5] == '\0')
#define isCheckString(m) (\
	*m   == 'c' && m[1] == 'h' && m[2] == 'e' && m[3] == 'c' && \
	m[4] == 'k' && m[5] == '\0')
#define isCreateString(m) (\
	*m   == 'c' && m[1] == 'r' && m[2] == 'e' && m[3] == 'a' && \
	m[4] == 't' && m[5] == 'e' && m[6] == '\0')
#define isAllocString(m) (\
	*m   == 'a' && m[1] == 'l' && m[2] == 'l' && m[3] == 'o' && \
	m[4] == 'c' && m[5] == '\0')
#define isDestroyString(m) (\
	*m   == 'd' && m[1] == 'e' && m[2] == 's' && m[3] == 't' && \
	m[4] == 'r' && m[5] == 'o' && m[6] == 'y' && m[7] == '\0')
#define isInstDestroyString(m) (\
        *m   == 'i' && m[1] == 'n' && m[2] == 's' && m[3] == 't' && \
	m[4] == 'd' && m[5] == 'e' && m[6] == 's' && m[7] == 't' && \
	m[8] == 'r' && m[9] == 'o' && m[10] == 'y' && m[11] == '\0')
#define isInitString(m) (\
	*m   == 'i' && m[1] == 'n' && m[2] == 'i' && m[3] == 't' && \
	m[4] == '\0')
#define isInfoString(m) (\
	*m   == 'i' && m[1] == 'n' && m[2] == 'f' && m[3] == 'o' && \
	m[4] == '\0')
#define isNextString(m) (\
	*m   == 'n' && m[1] == 'e' && m[2] == 'x' && m[3] == 't' && \
	m[4] == '\0')
#define isInstinvarString(m) (\
        *m   == 'i' && m[1] == 'n' && m[2] == 's' && m[3] == 't' && \
	m[4] == 'i' && m[5] == 'n' && m[6] == 'v' && m[7] == 'a' && \
	m[8] == 'r' && m[9] == '\0')
#define isInvarString(m) (\
	*m   == 'i' && m[1] == 'n' && m[2] == 'v' && m[3] == 'a' && \
	m[4] == 'r' && m[5] == '\0')
#define isInstprocString(m) (\
	*m   == 'i' && m[1] == 'n' && m[2] == 's' && m[3] == 't' && \
	m[4] == 'p' && m[5] == 'r' && m[6] == 'o' && m[7] == 'c' && \
        m[8] == '\0')
#define isProcString(m) (\
	*m   == 'p' && m[1] == 'r' && m[2] == 'o' && m[3] == 'c' && \
	m[4] == '\0')

#if defined(sun) /*|| defined(__linux__)*/
#  define USE_ALLOCA
#endif

#if _IBMC__ >= 0x0306
#  define USE_ALLOCA
#endif

#if defined(VISUAL_CC)
#  define USE_MALLOC
#endif

#if defined(USE_MALLOC)
#  define DEFINE_NEW_TCL_OBJS_ON_STACK(oc,ov) \
    Tcl_Obj** ov = (Tcl_Obj**) ckalloc((oc)*sizeof(Tcl_Obj*))
#  define FREE_TCL_OBJS_ON_STACK(ov) ckfree((char*) ov)
#elif defined(USE_ALLOCA)
#  define DEFINE_NEW_TCL_OBJS_ON_STACK(oc,ov) \
    Tcl_Obj** ov = (Tcl_Obj**) alloca((oc)*sizeof(Tcl_Obj*))
#  define FREE_TCL_OBJS_ON_STACK(ov)
#else
# if !defined(NDEBUG)
#  define DEFINE_NEW_TCL_OBJS_ON_STACK(oc,ov) \
    int __OC = (oc)+2; Tcl_Obj *__OV[__OC]; Tcl_Obj** ov = __OV+1; \
    __OV[0] = __OV[__OC-1] = (Tcl_Obj*)0xdeadbeaf
#  define FREE_TCL_OBJS_ON_STACK(ov) \
    assert(__OV[0] == __OV[__OC-1] && __OV[0] == (Tcl_Obj*)0xdeadbeaf)
# else 
#  define DEFINE_NEW_TCL_OBJS_ON_STACK(oc,ov) Tcl_Obj* ov[(oc)]
#  define FREE_TCL_OBJS_ON_STACK(ov)
#  endif
#endif

#ifdef USE_ALLOCA
# include <alloca.h>
#endif

#ifdef  __WIN32__
# define XOTCLINLINE
# define XOTclNewObj(A) A=Tcl_NewObj()
# define DECR_REF_COUNT(A) \
	MEM_COUNT_FREE("INCR_REF_COUNT",A); Tcl_DecrRefCount(A)
#else
# if defined(sun) || __IBMC__ >= 0x0306
#  define XOTCLINLINE
# else
#  define XOTCLINLINE inline
# endif
# ifdef USE_TCL_STUBS
#  define XOTclNewObj(A) A=Tcl_NewObj()
#  define DECR_REF_COUNT(A) \
	MEM_COUNT_FREE("INCR_REF_COUNT",A); assert((A)->refCount > -1); \
        Tcl_DecrRefCount(A) \
	
# else
#  define XOTclNewObj(A) TclNewObj(A)
#  define DECR_REF_COUNT(A) \
	MEM_COUNT_FREE("INCR_REF_COUNT",A); TclDecrRefCount(A)
# endif
#endif

#if !defined(PRE83) && defined(TCL_THREADS)
# define XOTclMutex Tcl_Mutex 
# define XOTclMutexLock(a) Tcl_MutexLock(a)
# define XOTclMutexUnlock(a) Tcl_MutexUnlock(a)
#else
# define XOTclMutex int
# define XOTclMutexLock(a)   (*(a))++
# define XOTclMutexUnlock(a) (*(a))--
#endif

#if defined(PRE84)
# define CONST84 
#else
# define CONST84 CONST
#endif

#if defined(PRE81)
# define ObjStr(obj) Tcl_GetStringFromObj(obj, ((int*)NULL))
#else
# define ObjStr(obj) (obj)->bytes ? (obj)->bytes : Tcl_GetString(obj)
/*# define ObjStr(obj) Tcl_GetString(obj) */
#endif

#ifdef V81
# define EvalObj(in,cmd) Tcl_EvalObj(in,cmd,0)
# define TclIsVarArgument(args) (args->isArg)
# define Tcl_ObjSetVar2(in,p1,p2,newval,flags) \
	Tcl_SetObjVar2(in,ObjStr(p1),p2,newval,flags)
#define Tcl_ObjGetVar2(in,name1,name2,flgs) \
  Tcl_GetObjVar2(in, ObjStr(name1), \
		((name2==NULL) ? (char*)NULL : ObjStr(name2)), flgs)
#else
# if defined(PRE83)
#  define EvalObj(in,cmd) Tcl_EvalObj(in,cmd)
# else
#  define EvalObj(in,cmd) Tcl_EvalObjEx(in,cmd,0)
# endif
# if defined(PRE81) && TCL_RELEASE_SERIAL<3
#  define TclIsVarArgument(args) (args->isArg)
# endif
#endif

#if 1
#define XOTcl_FrameDecls CallFrame *oldFramePtr = 0, frame, *newFramePtr = &frame
#define XOTcl_PushFrame(in,obj) \
     memset(newFramePtr, 0, sizeof(CallFrame)); \
     oldFramePtr = ((Interp *)in)->varFramePtr; \
     if (obj->nsPtr) { \
       newFramePtr->nsPtr = (Namespace*) obj->nsPtr; \
     } else { \
       newFramePtr->nsPtr = (Namespace*) RUNTIME_STATE(in)->fakeNS; \
       newFramePtr->isProcCallFrame = 1; \
       newFramePtr->procPtr = &RUNTIME_STATE(in)->fakeProc; \
       newFramePtr->varTablePtr = obj->varTable; \
     } \
     ((Interp *)in)->varFramePtr = newFramePtr; \
     MEM_COUNT_OPEN_FRAME()
#define XOTcl_PopFrame(in,obj) \
     if (!obj->nsPtr && obj->varTable == 0) \
       obj->varTable = newFramePtr->varTablePtr; \
     ((Interp *)in)->varFramePtr = oldFramePtr; \
     MEM_COUNT_CLOSE_FRAME()

#else
/* slightly slower version based on Tcl_PushCallFrame */
#define XOTcl_FrameDecls Tcl_CallFrame frame
#define XOTcl_PushFrame(in,obj) \
     if (obj->nsPtr) { \
       Tcl_PushCallFrame(in, &frame, obj->nsPtr, 0); \
     } else { \
       Tcl_PushCallFrame(in, &frame, RUNTIME_STATE(in)->fakeNS, 1); \
       Tcl_CallFrame_procPtr(&frame) = &RUNTIME_STATE(in)->fakeProc; \
       Tcl_CallFrame_varTablePtr(&frame) = obj->varTable; \
     }
#define XOTcl_PopFrame(in,obj) \
     if (!obj->nsPtr) { \
       if (obj->varTable == 0) \
         obj->varTable = Tcl_CallFrame_varTablePtr(&frame); \
       Tcl_CallFrame_varTablePtr(&frame) = 0; \
       Tcl_CallFrame_procPtr(&frame) = 0; \
     } \
     Tcl_PopCallFrame(in)
#endif


#define INCR_REF_COUNT(A) MEM_COUNT_ALLOC("INCR_REF_COUNT",A); Tcl_IncrRefCount(A)

#ifdef OBJDELETION_TRACE
# define PRINTOBJ(ctx,obj) \
  fprintf(stderr, "  %s %p %s oid=%p teardown=%p destroyCalled=%d\n", \
	  ctx,obj,ObjStr(obj->cmdName), obj->id, obj->teardown, \
	  (obj->flags & XOTCL_DESTROY_CALLED))
#else
# define PRINTOBJ(ctx,obj) 
#endif

#define className(cl) (cl ? ObjStr(cl->object.cmdName) : "")


#define LONG_AS_STRING 32

/* TCL_CONTINUE is defined as 4, from 5 on we can
   use app-specific return codes */
#define XOTCL_UNKNOWN 5
#define XOTCL_CHECK_FAILED 6

/* flags for call method */
#define XOTCL_CM_NO_FILTERS 1
#define XOTCL_CM_NO_UNKNOWN 2

/*
 *
 * XOTcl Structures
 *
 */

/*
 * Filter structures
 */
typedef struct XOTclFilterStack {
  Tcl_Command currentCmdPtr;
  Tcl_Obj* calledProc;
  struct XOTclFilterStack* next;
} XOTclFilterStack;

typedef struct XOTclTclObjList {
  Tcl_Obj* content;
  struct XOTclTclObjList* next;
} XOTclTclObjList;

/*
 * Assertion structures
 */

typedef struct XOTclProcAssertion {
  XOTclTclObjList* pre;
  XOTclTclObjList* post;
} XOTclProcAssertion;

typedef struct XOTclAssertionStore {
  XOTclTclObjList* invariants;
  Tcl_HashTable procs;
} XOTclAssertionStore;

typedef enum { /* powers of 2; add to ALL, if default; */
  CHECK_NONE  = 0, CHECK_CLINVAR = 1, CHECK_OBJINVAR = 2,
  CHECK_PRE   = 4, CHECK_POST = 8,
  CHECK_INVAR = CHECK_CLINVAR + CHECK_OBJINVAR,
  CHECK_ALL   = CHECK_INVAR   + CHECK_PRE + CHECK_POST
} CheckOptions;

void XOTclAssertionRename(Tcl_Interp* in, Tcl_Command cmd, 
			  XOTclAssertionStore *as, 
			  char *oldSimpleCmdName, char *newName);
/*
 * mixins
 */
typedef struct XOTclMixinStack {
  Tcl_Command currentCmdPtr;
  struct XOTclMixinStack* next;
} XOTclMixinStack;

/*
 * Generic command pointer list
 */
typedef struct XOTclCmdList {
  Tcl_Command cmdPtr;
  ClientData clientData;
  struct XOTclCmdList* next;
} XOTclCmdList;

typedef void (XOTclFreeCmdListClientData) _ANSI_ARGS_((XOTclCmdList*));

/* for incr string */
typedef struct XOTclStringIncrStruct {
  char *buffer;
  char *start;
  int bufSize;
  int length;
} XOTclStringIncrStruct;

/* 
 * object flags ...
 */

/* DESTROY_CALLED indicates that destroy was called on obj */ 
#define XOTCL_DESTROY_CALLED                 0x0001
/* INIT_CALLED indicates that init was called on obj */ 
#define XOTCL_INIT_CALLED                    0x0002
/* MIXIN_ORDER_VALID set when mixin order is valid */
#define XOTCL_MIXIN_ORDER_VALID              0x0004
/* MIXIN_ORDER_DEFINED set, when mixins are defined for obj */
#define XOTCL_MIXIN_ORDER_DEFINED            0x0008
#define XOTCL_MIXIN_ORDER_DEFINED_AND_VALID  0x000c
/* FILTER_ORDER_VALID set, when filter order is valid */
#define XOTCL_FILTER_ORDER_VALID             0x0010
/* FILTER_ORDER_DEFINED set, when filters are defined for obj */
#define XOTCL_FILTER_ORDER_DEFINED           0x0020
#define XOTCL_FILTER_ORDER_DEFINED_AND_VALID 0x0030
/* IS_CLASS set, when object is a class */
#define XOTCL_IS_CLASS                       0x0040
/* DESTROYED set, when object is physically destroyed with PrimitiveODestroy  */
#define XOTCL_DESTROYED                      0x0080
#define XOTCL_REFCOUNTED                     0x0100
#define XOTCL_RECREATE                       0x0200

#define XOTclObjectSetClass(obj) \
	(obj)->flags |= XOTCL_IS_CLASS
#define XOTclObjectIsClass(obj) \
	((obj)->flags & XOTCL_IS_CLASS)
#define XOTclObjectToClass(obj) \
	(XOTclClass*)((((XOTclObject*)obj)->flags & XOTCL_IS_CLASS)?obj:0)



/*
 * object and class internals
 */
typedef struct XOTclObjectOpt {
  XOTclAssertionStore *assertions;
  XOTclCmdList* filters;
  XOTclCmdList* mixins;
#ifdef XOTCL_METADATA
  Tcl_HashTable metaData;
#endif
  ClientData clientData;
  short checkoptions;
} XOTclObjectOpt;

typedef struct XOTclObject {
  Tcl_Obj *cmdName;
  Tcl_Command id;
  Tcl_Interp *teardown;
  struct XOTclClass *cl;
  Tcl_HashTable *varTable;
  Tcl_Namespace *nsPtr;
  XOTclObjectOpt *opt;
  struct XOTclCmdList *filterOrder;
  struct XOTclCmdList *mixinOrder;
  XOTclFilterStack *filterStack;
  XOTclMixinStack *mixinStack;
  int refCount;
  short flags;
} XOTclObject;

typedef struct XOTclClassOpt {
  XOTclCmdList* instfilters;
  XOTclCmdList* instmixins;
  XOTclAssertionStore *assertions;
  Tcl_Obj* parameterClass;
#ifdef XOTCL_OBJECTDATA
  Tcl_HashTable* objectdata;
#endif
  ClientData clientData;
} XOTclClassOpt;

typedef struct XOTclClass {
  struct XOTclObject object;
  struct XOTclClasses* super;
  struct XOTclClasses* sub;
  short color;
  struct XOTclClasses* order;
  struct XOTclClass* parent;
  Tcl_HashTable instances;
  Tcl_Namespace *nsPtr;
  Tcl_Obj* parameters;
  XOTclClassOpt* opt;
} XOTclClass;

typedef struct XOTclClasses {
  struct XOTclClass* cl;
  ClientData clientData;
  struct XOTclClasses* next;
} XOTclClasses;

/* XOTcl global names and strings */
/* these are names and contents for global (corresponding) Tcl_Objs
   and Strings - otherwise these "constants" would have to be built
   every time they are used; now they are built once in XOTcl_Init */
typedef enum {
    EMPTY, UNKNOWN, CREATE, DESTROY, INSTDESTROY, ALLOC,
    INIT, INSTVAR, INTERP, AUTONAMES,
    ZERO, MOVE, SELF, CLASS, RECREATE,
    SELF_CLASS, SELF_PROC, PARAM_CL,
    SEARCH_DEFAULTS, EXIT_HANDLER,
    CLEANUP, CONFIGURE, FILTER, INSTFILTER,
    INSTPROC, PROC, MKGETTERSETTER, FORMAT, 
    NEWOBJ, GUARD_OPTION, DEFAULTMETHOD,
    __UNKNOWN,
    /** these are the redefined tcl commands; leave them
	together at the end */
    EXPR, INCR, INFO, RENAME, SUBST, 
} XOTclGlobalNames;
#if !defined(XOTCL_C)
extern char *XOTclGlobalStrings[];
#else
char *XOTclGlobalStrings[] = {
  "", "unknown", "create", "destroy", "instdestroy", "alloc",
  "init", "instvar", "interp", "__autonames",
  "0", "move", "self", "class", "recreate",
  "self class", "self proc", "::xotcl::Class::Parameter",
  "searchDefaults", "__exitHandler",
  "cleanup", "configure", "filter", "instfilter",
  "instproc", "proc", "mkGetterSetter", "format", 
  "__#", "-guard", "defaultmethod",
  "__unknown",
  "expr", "incr", "info", "rename", "subst", 
};
#endif

#define XOTclGlobalObjects RUNTIME_STATE(in)->methodObjNames

/* XOTcl ShadowTclCommands */
typedef struct XOTclShadowTclCommandInfo {
  TclObjCmdProcType proc;
  ClientData cd;
} XOTclShadowTclCommandInfo;
typedef enum {SHADOW_LOAD=1, SHADOW_UNLOAD=0, SHADOW_REFETCH=2} XOTclShadowOperations;

int XOTclCallCommand(Tcl_Interp* in, XOTclGlobalNames name,
		     int objc, Tcl_Obj *CONST objv[]);
int XOTclShadowTclCommands(Tcl_Interp* in, XOTclShadowOperations load);


/*
 * XOTcl CallStack
 */
typedef struct XOTclCallStackContent {
  XOTclObject *self;
  XOTclClass *cl;
  Tcl_Command cmdPtr;
  Tcl_Command destroyedCmd;
  Tcl_CallFrame *currentFramePtr;
  unsigned short frameType;
  unsigned short callsNext;
  XOTclFilterStack *filterStackEntry;
} XOTclCallStackContent;

#define XOTCL_CSC_TYPE_PLAIN 0
#define XOTCL_CSC_TYPE_MIXIN 1
#define XOTCL_CSC_TYPE_FILTER 2
#define XOTCL_CSC_INACTIVE_FLAG 4
#define XOTCL_CSC_TYPE_ACTIVE_MIXIN 1
#define XOTCL_CSC_TYPE_ACTIVE_FILTER 2
#define XOTCL_CSC_TYPE_INACTIVE_MIXIN 5
#define XOTCL_CSC_TYPE_INACTIVE_FILTER 6
#define XOTCL_CSC_TYPE_GUARD 8

typedef struct XOTclCallStack {
  XOTclCallStackContent content[MAX_NESTING_DEPTH];
  XOTclCallStackContent *top;
  short guardCount;
} XOTclCallStack;

#if defined(PROFILE)
typedef struct XOTclProfile {
  long int overallTime;
  Tcl_HashTable objectData;
  Tcl_HashTable methodData;
} XOTclProfile;
#endif

typedef struct XOTclRuntimeState {
  XOTclCallStack cs;
  Tcl_Namespace *XOTclClassesNS;
  Tcl_Namespace *XOTclNS;
  /*
   * definitions of the main xotcl objects
   */
  XOTclClass *theObject;
  XOTclClass *theClass;
  Tcl_CmdProc *interpProc;
  Tcl_ObjCmdProc *objInterpProc;
  Tcl_Obj **methodObjNames;
  struct XOTclShadowTclCommandInfo *tclCommands;
  int errorCount;
  int callDestroy;
  int callIsDestroy;
  int exitHandlerDestroyRound;
  int returnCode;
  long newCounter;
  XOTclStringIncrStruct iss;
  Proc fakeProc;
  Tcl_Namespace *fakeNS;
  XotclStubs *xotclStubs;
  Tcl_CallFrame *varFramePtr;
#if defined(PROFILE)
  XOTclProfile profile;
#endif
  ClientData clientData;
} XOTclRuntimeState;

#define XOTCL_EXITHANDLER_OFF 0
#define XOTCL_EXITHANDLER_ON_SOFT_DESTROY 1
#define XOTCL_EXITHANDLER_ON_PHYSICAL_DESTROY 2

/*
 *
 *  Mostly internally used API functions
 *
 */

#include "xotclIntDecls.h"

/*
 * Profiling functions
 */

#if defined(PROFILE)
extern void
XOTclProfileFillTable(Tcl_HashTable* table, Tcl_DString* key,
		 double totalMicroSec);
extern void
XOTclProfileEvaluateData(Tcl_Interp* in, long int startSec, long int startUsec,
		    XOTclObject* obj, XOTclClass *cl, char *methodName);
extern void
XOTclProfilePrintTable(Tcl_HashTable* table);

extern void
XOTclProfilePrintData(Tcl_Interp* in);

extern void 
XOTclProfileInit(Tcl_Interp* in);
#endif

/*
 * MEM Counting
 */
#ifdef XOTCL_MEM_COUNT
void XOTclMemCountAlloc(char* id, void *);
void XOTclMemCountFree(char* id, void *);
void XOTclMemCountDump();
#endif /* XOTCL_MEM_COUNT */
/*
 * old, deprecated meta-data command
 */
#if defined(XOTCL_METADATA)
extern void
XOTclMetaDataDestroy(XOTclObject* obj);
extern void
XOTclMetaDataInit(XOTclObject* obj);
extern int
XOTclOMetaDataMethod (ClientData cd, Tcl_Interp* in, 
		      int objc, Tcl_Obj *objv[]);
#endif /* XOTCL_METADATA */


/* 
 * bytecode support
 */
#ifdef XOTCL_BYTECODE
typedef struct XOTclCompEnv {
  int bytecode;
  Command *cmdPtr;
  CompileProc *compileProc;
  Tcl_ObjCmdProc *callProc;
} XOTclCompEnv;

typedef enum {INST_INITPROC, INST_NEXT, INST_SELF, INST_SELF_DISPATCH, 
	      LAST_INSTRUCTION} XOTclByteCodeInstructions;


extern XOTclCompEnv *XOTclGetCompEnv();

Tcl_ObjCmdProc XOTclInitProcNSCmd, XOTclSelfDispatchCmd, 
  XOTclNextObjCmd, XOTclGetSelfObjCmd;
#endif

int XOTclCorrectAutoPath(Tcl_Interp* in);
int XOTclDirectSelfDispatch(ClientData cd, Tcl_Interp* in,
		     int objc, Tcl_Obj *CONST objv[]);
int XOTclObjDispatch(ClientData cd, Tcl_Interp* in,
		     int objc, Tcl_Obj *CONST objv[]);


XOTclCallStackContent *
XOTclCallStackFindActiveFrame(Tcl_Interp* in, int offset);
XOTclCallStackContent *
XOTclCallStackFindLastInvocation(Tcl_Interp* in, int offset);

int XOTclCallMethodWithArg(ClientData cd, Tcl_Interp* in, 
			   Tcl_Obj* method, Tcl_Obj* arg,
			   int objc, Tcl_Obj *CONST objv[], int flags);

/* functions from xotclUtil.c */
char *XOTcl_ltoa(char *buf, long i, int *len);
char *XOTclStringIncr(XOTclStringIncrStruct *iss);
void XOTclStringIncrInit(XOTclStringIncrStruct *iss);
void XOTclStringIncrFree(XOTclStringIncrStruct *iss);


#if !defined(NDEBUG)
/*# define XOTCLINLINE*/
#endif


/*** common win sermon ***/
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _xotcl_int_h_ */
