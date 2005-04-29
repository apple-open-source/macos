/* 
 * procbody.c --
 *
 *  Contains the implementation of the Tcl object TclProProcBody, an object
 *  that encapsulates information about a compiled procedure body.
 *  This object is used by the compiler to store information about a compiled
 *  procedure body so that the loader can extract it back and regenerate it
 *  in its specialized proc command implementation.
 *  This object is essentially a container that knows how to copy and free its
 *  internal rep; the UpdateString and SetFromAny procedures don't expect to
 *  be called, and therefore panic when they are. To create this type of
 *  object, clients use the ProcBodyNewObj function. Access to the
 *  components of internal representation is provided via a number of
 *  procedures.
 *
 * Copyright (c) 1998-2000 Ajuba Solutions
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: cmpProcBody.c,v 1.2 2000/05/30 22:19:06 wart Exp $
 */

#include "cmpInt.h"

/*
 * Prototypes for static functions in this file
 */

static void	ProcBodyDup _ANSI_ARGS_((Tcl_Obj *srcPtr, Tcl_Obj *dupPtr));
static void	ProcBodyFree _ANSI_ARGS_((Tcl_Obj *objPtr));
static int	ProcBodySetFromAny _ANSI_ARGS_((Tcl_Interp *interp,
		Tcl_Obj *objPtr));
static void	ProcBodyUpdateString _ANSI_ARGS_((Tcl_Obj *objPtr));

/*
 * The CompilerProcBodyObjType type
 */

static Tcl_ObjType cmpProcBodyObjType = {
    CMP_PROCBODY_OBJ_TYPE_OLD,	/* name for this type */
    ProcBodyFree,		/* FreeInternalRep procedure */
    ProcBodyDup,		/* DupInternalRep procedure */
    ProcBodyUpdateString,	/* UpdateString procedure */
    ProcBodySetFromAny		/* SetFromAny procedure */
};

/*
 * controls registration of object types
 */

static int cmpTypesRegistered = 0;


/*
 *----------------------------------------------------------------------
 *
 * ProcBodyRegisterTypes --
 *
 *  Register the object type(s) used for the procedure body support.
 *  Can be called multiple times, it will register only once.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Registers the "TclProProcBody" Tcl_Obj with TCL.
 *
 *----------------------------------------------------------------------
 */

void
ProcBodyRegisterTypes()
{
    if (cmpTypesRegistered < 1) {
        Tcl_RegisterObjType(&cmpProcBodyObjType);
        cmpTypesRegistered = 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ProcBodyNewObj --
 *
 *  Creates a new object, of type "TclProProcBody", whose internal
 *  representation is the given Proc struct.
 *  The newly created object's reference count is 0.
 *
 * Results:
 *  Returns a pointer to a newly allocated Tcl_Obj, 0 on error.
 *
 * Side effects:
 *  The reference count in the ByteCode attached to the Proc is bumped up
 *  by one, since the internal rep stores a pointer to it.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
ProcBodyNewObj(procPtr)
    Proc *procPtr;	/* the Proc struct to store as the internal
                         * representation. */
{
    Tcl_Obj *objPtr;

    if (!procPtr) {
        panic("ProcBodyNewObj: NULL Proc pointer");
    }
    
    objPtr = Tcl_NewObj();

    if (objPtr) {
        objPtr->typePtr = &cmpProcBodyObjType;
        objPtr->internalRep.otherValuePtr = (VOID *) procPtr;

        procPtr->refCount++;
    }

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ProcBodyDup --
 *
 *  Tcl_ObjType's Dup function for the proc body object.
 *  Bumps the reference count on the Proc stored in the internal
 *  representation.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Sets up the object in dupPtr to be a duplicate of the one in srcPtr.
 *
 *----------------------------------------------------------------------
 */

static void ProcBodyDup(srcPtr, dupPtr)
    Tcl_Obj *srcPtr;		/* object to copy */
    Tcl_Obj *dupPtr;		/* target object for the duplication */
{
    Proc *procPtr = (Proc *) srcPtr->internalRep.otherValuePtr;
    
    dupPtr->typePtr = &cmpProcBodyObjType;
    dupPtr->internalRep.otherValuePtr = (VOID *) procPtr;
    procPtr->refCount++;
}

/*
 *----------------------------------------------------------------------
 *
 * ProcBodyFree --
 *
 *  Tcl_ObjType's Free function for the proc body object.
 *  The reference count on its Proc struct is decreased by 1; if the count
 *  reaches 0, the proc is freed.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  If the reference count on the Proc struct reaches 0, the struct is freed.
 *
 *----------------------------------------------------------------------
 */

static void
ProcBodyFree(objPtr)
    Tcl_Obj *objPtr;		/* the object to clean up */
{
    Proc *procPtr = (Proc *) objPtr->internalRep.otherValuePtr;
    procPtr->refCount--;
    if (procPtr->refCount <= 0) {
        ProcBodyCleanupProc(procPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ProcBodyCleanupProc --
 *
 *  This procedure does all the real work of freeing up a Proc
 *  structure.  It's called only when the structure's reference
 *  count becomes zero.
 *
 *  Lifted from tclproc.c::CleanupProc.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Memory gets freed.
 *
 *----------------------------------------------------------------------
 */

void
ProcBodyCleanupProc(procPtr)
    Proc *procPtr;		/* Procedure to be deleted. */
{
#if 1
    TclProcCleanupProc(procPtr);
#else
    register CompiledLocal *localPtr;
    Tcl_Obj *bodyPtr = procPtr->bodyPtr;
    Tcl_Obj *defPtr;

    if (bodyPtr != NULL) {
        Tcl_DecrRefCount(bodyPtr);
    }
    for (localPtr = procPtr->firstLocalPtr;  localPtr != NULL;  ) {
        CompiledLocal *nextPtr = localPtr->nextPtr;

        if (localPtr->defValuePtr != NULL) {
            defPtr = localPtr->defValuePtr;
            Tcl_DecrRefCount(defPtr);
        }
        ckfree((char *) localPtr);
        localPtr = nextPtr;
    }
    ckfree((char *) procPtr);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * ProcBodySetFromAny --
 *
 *  Tcl_ObjType's SetFromAny function for the proc body object.
 *  Calls panic.
 *
 * Results:
 *  Theoretically returns a TCL result code.
 *
 * Side effects:
 *  Calls panic, since we can't set the value of the object from a string
 *  representation (or any other internal ones).
 *
 *----------------------------------------------------------------------
 */

static int
ProcBodySetFromAny(interp, objPtr)
    Tcl_Interp *interp;			/* current interpreter */
    Tcl_Obj *objPtr;			/* object pointer */
{
    panic("called ProcBodySetFromAny");

    /*
     * this to keep compilers happy.
     */
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ProcBodyUpdateString --
 *
 *  Tcl_ObjType's UpdateString function for the proc body object.
 *  Calls panic.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Calls panic, since we this type has no string representation.
 *
 *----------------------------------------------------------------------
 */

static void
ProcBodyUpdateString(objPtr)
    Tcl_Obj *objPtr;		/* the object to update */
{
    panic("called ProcBodyUpdateString");
}
