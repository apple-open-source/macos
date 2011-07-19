#include "xotclInt.h"
#define TclDecrRefCount Tcl_DecrRefCount
/*
 * the following functions are copied from Tcl 8.4.19.
 * We need just TclDeleteVars, but TclDeleteVars calls 
 * CallVarTraces() and DeleteArray();
 */
static void
DisposeTraceResult(flags, result)
    int flags;			/* Indicates type of result to determine
				 * proper disposal method */
    char *result;		/* The result returned from a trace
				 * procedure to be disposed */
{
    if (flags & TCL_TRACE_RESULT_DYNAMIC) {
	ckfree(result);
    } else if (flags & TCL_TRACE_RESULT_OBJECT) {
	Tcl_DecrRefCount((Tcl_Obj *) result);
    }
}

static void
VarErrMsg(interp, part1, part2, operation, reason)
    Tcl_Interp *interp;         /* Interpreter in which to record message. */
    CONST char *part1;
    CONST char *part2;		/* Variable's two-part name. */
    CONST char *operation;      /* String describing operation that failed,
                                 * e.g. "read", "set", or "unset". */
    CONST char *reason;         /* String describing why operation failed. */
{
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "can't ", operation, " \"", part1,
	    (char *) NULL);
    if (part2 != NULL) {
        Tcl_AppendResult(interp, "(", part2, ")", (char *) NULL);
    }
    Tcl_AppendResult(interp, "\": ", reason, (char *) NULL);
}

static int
CallVarTraces(iPtr, arrayPtr, varPtr, part1, part2, flags, leaveErrMsg)
    Interp *iPtr;		/* Interpreter containing variable. */
    register Var *arrayPtr;	/* Pointer to array variable that contains
				 * the variable, or NULL if the variable
				 * isn't an element of an array. */
    Var *varPtr;		/* Variable whose traces are to be
				 * invoked. */
    CONST char *part1;
    CONST char *part2;		/* Variable's two-part name. */
    int flags;			/* Flags passed to trace procedures:
				 * indicates what's happening to variable,
				 * plus other stuff like TCL_GLOBAL_ONLY,
				 * or TCL_NAMESPACE_ONLY. */
    CONST int leaveErrMsg;	/* If true, and one of the traces indicates an
				 * error, then leave an error message and stack
				 * trace information in *iPTr. */
{
    register VarTrace *tracePtr;
    ActiveVarTrace active;
    char *result;
    CONST char *openParen, *p;
    Tcl_DString nameCopy;
    int copiedName;
    int code = TCL_OK;
    int disposeFlags = 0;
    int saveErrFlags = iPtr->flags 
	    & (ERR_IN_PROGRESS | ERR_ALREADY_LOGGED | ERROR_CODE_SET);

    /*
     * If there are already similar trace procedures active for the
     * variable, don't call them again.
     */

    if (varPtr->flags & VAR_TRACE_ACTIVE) {
	return code;
    }
    varPtr->flags |= VAR_TRACE_ACTIVE;
    varPtr->refCount++;
    if (arrayPtr != NULL) {
	arrayPtr->refCount++;
    }

    /*
     * If the variable name hasn't been parsed into array name and
     * element, do it here.  If there really is an array element,
     * make a copy of the original name so that NULLs can be
     * inserted into it to separate the names (can't modify the name
     * string in place, because the string might get used by the
     * callbacks we invoke).
     */

    copiedName = 0;
    if (part2 == NULL) {
	for (p = part1; *p ; p++) {
	    if (*p == '(') {
		openParen = p;
		do {
		    p++;
		} while (*p != '\0');
		p--;
		if (*p == ')') {
		    int offset = (openParen - part1);
		    char *newPart1;
		    Tcl_DStringInit(&nameCopy);
		    Tcl_DStringAppend(&nameCopy, part1, (p-part1));
		    newPart1 = Tcl_DStringValue(&nameCopy);
		    newPart1[offset] = 0;
		    part1 = newPart1;
		    part2 = newPart1 + offset + 1;
		    copiedName = 1;
		}
		break;
	    }
	}
    }

    /*
     * Invoke traces on the array containing the variable, if relevant.
     */

    result = NULL;
    active.nextPtr = iPtr->activeVarTracePtr;
    iPtr->activeVarTracePtr = &active;
    Tcl_Preserve((ClientData) iPtr);
    if (arrayPtr != NULL && !(arrayPtr->flags & VAR_TRACE_ACTIVE)) {
	active.varPtr = arrayPtr;
	for (tracePtr = arrayPtr->tracePtr;  tracePtr != NULL;
	     tracePtr = active.nextTracePtr) {
	    active.nextTracePtr = tracePtr->nextPtr;
	    if (!(tracePtr->flags & flags)) {
		continue;
	    }
	    Tcl_Preserve((ClientData) tracePtr);
	    if (Tcl_InterpDeleted((Tcl_Interp *)iPtr)) {
		flags |= TCL_INTERP_DESTROYED;
	    }
	    result = (*tracePtr->traceProc)(tracePtr->clientData,
		    (Tcl_Interp *) iPtr, part1, part2, flags);
	    if (result != NULL) {
		if (flags & TCL_TRACE_UNSETS) {
		    /* Ignore errors in unset traces */
		    DisposeTraceResult(tracePtr->flags, result);
		} else {
	            disposeFlags = tracePtr->flags;
		    code = TCL_ERROR;
		}
	    }
	    Tcl_Release((ClientData) tracePtr);
	    if (code == TCL_ERROR) {
		goto done;
	    }
	}
    }

    /*
     * Invoke traces on the variable itself.
     */

    if (flags & TCL_TRACE_UNSETS) {
	flags |= TCL_TRACE_DESTROYED;
    }
    active.varPtr = varPtr;
    for (tracePtr = varPtr->tracePtr; tracePtr != NULL;
	 tracePtr = active.nextTracePtr) {
	active.nextTracePtr = tracePtr->nextPtr;
	if (!(tracePtr->flags & flags)) {
	    continue;
	}
	Tcl_Preserve((ClientData) tracePtr);
	if (Tcl_InterpDeleted((Tcl_Interp *)iPtr)) {
	    flags |= TCL_INTERP_DESTROYED;
	}
	result = (*tracePtr->traceProc)(tracePtr->clientData,
		(Tcl_Interp *) iPtr, part1, part2, flags);
	if (result != NULL) {
	    if (flags & TCL_TRACE_UNSETS) {
		/* Ignore errors in unset traces */
		DisposeTraceResult(tracePtr->flags, result);
	    } else {
		disposeFlags = tracePtr->flags;
		code = TCL_ERROR;
	    }
	}
	Tcl_Release((ClientData) tracePtr);
	if (code == TCL_ERROR) {
	    goto done;
	}
    }

    /*
     * Restore the variable's flags, remove the record of our active
     * traces, and then return.
     */

    done:
    if (code == TCL_OK) {
	iPtr->flags |= saveErrFlags;
    }
    if (code == TCL_ERROR) {
	if (leaveErrMsg) {
	    CONST char *type = "";
	    switch (flags&(TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_ARRAY)) {
		case TCL_TRACE_READS: {
		    type = "read";
		    break;
		}
		case TCL_TRACE_WRITES: {
		    type = "set";
		    break;
		}
		case TCL_TRACE_ARRAY: {
		    type = "trace array";
		    break;
		}
	    }
	    if (disposeFlags & TCL_TRACE_RESULT_OBJECT) {
		VarErrMsg((Tcl_Interp *) iPtr, part1, part2, type,
			Tcl_GetString((Tcl_Obj *) result));
	    } else {
		VarErrMsg((Tcl_Interp *) iPtr, part1, part2, type, result);
	    }
	}
	DisposeTraceResult(disposeFlags,result);
    }

    if (arrayPtr != NULL) {
	arrayPtr->refCount--;
    }
    if (copiedName) {
	Tcl_DStringFree(&nameCopy);
    }
    varPtr->flags &= ~VAR_TRACE_ACTIVE;
    varPtr->refCount--;
    iPtr->activeVarTracePtr = active.nextPtr;
    Tcl_Release((ClientData) iPtr);

    return code;
}
static void
DeleteSearches(arrayVarPtr)
    register Var *arrayVarPtr;		/* Variable whose searches are
					 * to be deleted. */
{
    ArraySearch *searchPtr;

    while (arrayVarPtr->searchPtr != NULL) {
	searchPtr = arrayVarPtr->searchPtr;
	arrayVarPtr->searchPtr = searchPtr->nextPtr;
	ckfree((char *) searchPtr);
    }
}

static void
DeleteArray(iPtr, arrayName, varPtr, flags)
    Interp *iPtr;			/* Interpreter containing array. */
    CONST char *arrayName;	        /* Name of array (used for trace
					 * callbacks). */
    Var *varPtr;			/* Pointer to variable structure. */
    int flags;				/* Flags to pass to CallVarTraces:
					 * TCL_TRACE_UNSETS and sometimes
					 * TCL_NAMESPACE_ONLY, or
					 * TCL_GLOBAL_ONLY. */
{
    Tcl_HashSearch search;
    register Tcl_HashEntry *hPtr;
    register Var *elPtr;
    ActiveVarTrace *activePtr;
    Tcl_Obj *objPtr;

    DeleteSearches(varPtr);
    for (hPtr = Tcl_FirstHashEntry(varPtr->value.tablePtr, &search);
	 hPtr != NULL;  hPtr = Tcl_NextHashEntry(&search)) {
	elPtr = (Var *) Tcl_GetHashValue(hPtr);
	if (TclIsVarScalar(elPtr) && (elPtr->value.objPtr != NULL)) {
	    objPtr = elPtr->value.objPtr;
	    TclDecrRefCount(objPtr);
	    elPtr->value.objPtr = NULL;
	}
	elPtr->hPtr = NULL;
	if (elPtr->tracePtr != NULL) {
	    elPtr->flags &= ~VAR_TRACE_ACTIVE;
	    CallVarTraces(iPtr, (Var *) NULL, elPtr, arrayName,
		    Tcl_GetHashKey(varPtr->value.tablePtr, hPtr), flags,
		    /* leaveErrMsg */ 0);
	    while (elPtr->tracePtr != NULL) {
		VarTrace *tracePtr = elPtr->tracePtr;
		elPtr->tracePtr = tracePtr->nextPtr;
		Tcl_EventuallyFree((ClientData) tracePtr,TCL_DYNAMIC);
	    }
	    for (activePtr = iPtr->activeVarTracePtr; activePtr != NULL;
		 activePtr = activePtr->nextPtr) {
		if (activePtr->varPtr == elPtr) {
		    activePtr->nextTracePtr = NULL;
		}
	    }
	}
	TclSetVarUndefined(elPtr);
	TclSetVarScalar(elPtr);

	/*
	 * Even though array elements are not supposed to be namespace
	 * variables, some combinations of [upvar] and [variable] may
	 * create such beasts - see [Bug 604239]. This is necessary to
	 * avoid leaking the corresponding Var struct, and is otherwise
	 * harmless. 
	 */

	if (elPtr->flags & VAR_NAMESPACE_VAR) {
	    elPtr->flags &= ~VAR_NAMESPACE_VAR;
	    elPtr->refCount--;
	}
	if (elPtr->refCount == 0) {
	    ckfree((char *) elPtr); /* element Vars are VAR_IN_HASHTABLE */
	}
    }
    Tcl_DeleteHashTable(varPtr->value.tablePtr);
    ckfree((char *) varPtr->value.tablePtr);
}

void
TclDeleteVars84(iPtr, tablePtr)
    Interp *iPtr;		/* Interpreter to which variables belong. */
    Tcl_HashTable *tablePtr;	/* Hash table containing variables to
				 * delete. */
{
    Tcl_Interp *interp = (Tcl_Interp *) iPtr;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    register Var *varPtr;
    Var *linkPtr;
    int flags;
    ActiveVarTrace *activePtr;
    Tcl_Obj *objPtr;
    Namespace *currNsPtr = (Namespace *) Tcl_GetCurrentNamespace(interp);

    /*
     * Determine what flags to pass to the trace callback procedures.
     */

    flags = TCL_TRACE_UNSETS;
    if (tablePtr == &iPtr->globalNsPtr->varTable) {
	flags |= TCL_GLOBAL_ONLY;
    } else if (tablePtr == &currNsPtr->varTable) {
	flags |= TCL_NAMESPACE_ONLY;
    }

    for (hPtr = Tcl_FirstHashEntry(tablePtr, &search);  hPtr != NULL;
	 hPtr = Tcl_NextHashEntry(&search)) {
	varPtr = (Var *) Tcl_GetHashValue(hPtr);

	/*
	 * For global/upvar variables referenced in procedures, decrement
	 * the reference count on the variable referred to, and free
	 * the referenced variable if it's no longer needed. Don't delete
	 * the hash entry for the other variable if it's in the same table
	 * as us: this will happen automatically later on.
	 */
	if (TclIsVarLink(varPtr)) {
	    linkPtr = varPtr->value.linkPtr;
	    linkPtr->refCount--;
	    if ((linkPtr->refCount == 0) && TclIsVarUndefined(linkPtr)
		    && (linkPtr->tracePtr == NULL)
		    && (linkPtr->flags & VAR_IN_HASHTABLE)) {
		if (linkPtr->hPtr == NULL) {
		    ckfree((char *) linkPtr);
		} else if (linkPtr->hPtr->tablePtr != tablePtr) {
		    Tcl_DeleteHashEntry(linkPtr->hPtr);
		    ckfree((char *) linkPtr);
		}
	    }
	}

	/*
	 * Invoke traces on the variable that is being deleted, then
	 * free up the variable's space (no need to free the hash entry
	 * here, unless we're dealing with a global variable: the
	 * hash entries will be deleted automatically when the whole
	 * table is deleted). Note that we give CallVarTraces the variable's
	 * fully-qualified name so that any called trace procedures can
	 * refer to these variables being deleted.
	 */

	if (varPtr->tracePtr != NULL) {
	    objPtr = Tcl_NewObj();
	    Tcl_IncrRefCount(objPtr); /* until done with traces */
	    Tcl_GetVariableFullName(interp, (Tcl_Var) varPtr, objPtr);
	    CallVarTraces(iPtr, (Var *) NULL, varPtr, Tcl_GetString(objPtr),
		    NULL, flags, /* leaveErrMsg */ 0);
	    Tcl_DecrRefCount(objPtr); /* free no longer needed obj */

	    while (varPtr->tracePtr != NULL) {
		VarTrace *tracePtr = varPtr->tracePtr;
		varPtr->tracePtr = tracePtr->nextPtr;
		Tcl_EventuallyFree((ClientData) tracePtr, TCL_DYNAMIC);
	    }
	    for (activePtr = iPtr->activeVarTracePtr; activePtr != NULL;
		 activePtr = activePtr->nextPtr) {
		if (activePtr->varPtr == varPtr) {
		    activePtr->nextTracePtr = NULL;
		}
	    }
	}

	if (TclIsVarArray(varPtr)) {
	    DeleteArray(iPtr, Tcl_GetHashKey(tablePtr, hPtr), varPtr,
	            flags);
	    varPtr->value.tablePtr = NULL;
	}

	if (TclIsVarScalar(varPtr) && (varPtr->value.objPtr != NULL)) {
	    objPtr = varPtr->value.objPtr;
	    TclDecrRefCount(objPtr);
	    varPtr->value.objPtr = NULL;
	}

	varPtr->hPtr = NULL;
	varPtr->tracePtr = NULL;
	TclSetVarUndefined(varPtr);
	TclSetVarScalar(varPtr);

	/*
	 * If the variable was a namespace variable, decrement its 
	 * reference count. We are in the process of destroying its
	 * namespace so that namespace will no longer "refer" to the
	 * variable.
	 */

	if (varPtr->flags & VAR_NAMESPACE_VAR) {
	    varPtr->flags &= ~VAR_NAMESPACE_VAR;
	    varPtr->refCount--;
	}

	/*
	 * Recycle the variable's memory space if there aren't any upvar's
	 * pointing to it. If there are upvars to this variable, then the
	 * variable will get freed when the last upvar goes away.
	 */

	if (varPtr->refCount == 0) {
	    ckfree((char *) varPtr); /* this Var must be VAR_IN_HASHTABLE */
	}
    }
    Tcl_DeleteHashTable(tablePtr);

}
