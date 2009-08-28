/*
 * livelist.c --
 *
 * Support routines for DOM "live list" interfaces.
 *
 * Copyright (c) 2000 Joe English.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * $Id: livelist.c,v 1.3 2003/03/28 20:05:49 jenglish Exp $
 *
 */


#include <tcl.h>
#include "tclDomProInt.h"

/* @@ TODO: move this routine into this file, make it static. */
extern Tcl_Obj *GetUniqueListVariableName(Tcl_Interp *, char *, int);

/*
 * LiveListTraceProc clientData structure:
 *
 * NOTES: We store a Tcl_Obj* containing the _handle_ of the starting node
 * and look it up when the trace is called instead of storing a pointer
 * to the node itself, in order to avoid dangling references.
 * Similarly, the update procedure's callback data is stored as
 * a Tcl_Obj instead of a generic clientData so we don't need to 
 * worry about tracking separate 'deleteProc's
 */

typedef void (*TclDomLiveListUpdateProc)(
    Tcl_Interp *,		/* Tcl interpreter */
    TclDomInterpData *,		/* TclDom extension data */
    TclDomNode *,		/* starting node */
    Tcl_Obj *params,		/* client data */ 
    Tcl_Obj *result);		/* Tcl_ListObj in which to store result */

typedef struct 
{
    TclDomInterpData *interpDataPtr;	/* TclDOM extension state data */
    Tcl_Obj *nodeHandle;		/* Handle for starting node */
    TclDomLiveListUpdateProc updateProc; /* Update procedure to call */
    Tcl_Obj *params;			/* Extra parameters for updateProc */
} LiveListInfo;

/*
 * Trace procedure for live list variables:
 */
static char *LiveListTraceProc(
    ClientData clientData, Tcl_Interp *interp,
    CONST84 char *name1, CONST84 char *name2, int flags)
{
    LiveListInfo *llInfo = (LiveListInfo *)clientData;

    if (flags & TCL_TRACE_READS) {
	Tcl_Obj *resultList = Tcl_NewListObj(0,NULL);
	TclDomNode *nodePtr;
	Tcl_SavedResult state;

	Tcl_SaveResult(interp, &state);

	/* Call update procedure:
	*/
	nodePtr = TclDomGetNodeFromToken(interp, 	
		llInfo->interpDataPtr, llInfo->nodeHandle);

	if (nodePtr) {
	    llInfo->updateProc(interp, llInfo->interpDataPtr, 
	    	nodePtr,llInfo->params, resultList);
	}
	/* else -- node has been destroyed; result is the empty list */

	Tcl_ObjSetVar2(interp, Tcl_NewStringObj(name1, -1), NULL,
		resultList, TCL_GLOBAL_ONLY);
	Tcl_RestoreResult(interp, &state);
    }

    if (flags & TCL_TRACE_WRITES) {
    	return "Read-only variable";
    }
    if (flags & TCL_TRACE_UNSETS) {
	Tcl_DecrRefCount(llInfo->nodeHandle);
	if (llInfo->params) {
	    Tcl_DecrRefCount(llInfo->params);
	}
	Tcl_Free((ClientData)llInfo);
    }

    return NULL;
}


/* TclDomRegisterLiveList --
 * 	Creates a DOM "live list" object.
 *
 * Side effects:
 *	Creates a new global variable which, when read,
 *	will return the current value of a DOM NodeList.
 *
 * Returns:
 *	Tcl_Obj pointer holding the new variable name. 
 */
Tcl_Obj *TclDomRegisterLiveList(
    Tcl_Interp		*interp, 
    TclDomInterpData	*interpDataPtr,
    TclDomNode		*nodePtr,
    TclDomLiveListUpdateProc updateProc,
    Tcl_Obj		*params)
{
    Tcl_Obj *varNameObj = GetUniqueListVariableName(interp,"listVar",1);
    char *varName = Tcl_GetString(varNameObj);
    LiveListInfo *llInfo = (LiveListInfo *)Tcl_Alloc(sizeof(*llInfo));

    llInfo->interpDataPtr = interpDataPtr;
    llInfo->nodeHandle = TclDomGetNodeObj(interpDataPtr,nodePtr);
    llInfo->updateProc = updateProc;
    llInfo->params = params;
    Tcl_IncrRefCount(llInfo->params);

    Tcl_TraceVar(interp, varName,
    	TCL_TRACE_READS | TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
    	LiveListTraceProc, (ClientData)llInfo);

    return varNameObj;
}

/*
 *--------------------------------------------------------------
 *
 * UpdateElementsByTagnameNodeList
 *
 *	Preorder tree traversal routine which implements the 
 *	"getElementsByTagName" method.
 *	See the DOM specification for further information.
 *
 * Side effects:
 *	Appends matching children to listObj.
 *
 *--------------------------------------------------------------
 */

static void 
UpdateElementsByTagnameNodeList(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    TclDomNode *nodePtr,		/* Current search point */
    char *tagname,			/* Tag pattern to match against */
    Tcl_Obj *listObjPtr)		/* Where to store results */
{
    int result;
    if (nodePtr->nodeName && Tcl_StringMatch(nodePtr->nodeName, tagname)) {
	    Tcl_Obj *tokenPtr = TclDomGetNodeObj(interpDataPtr, nodePtr);
	    result = Tcl_ListObjAppendElement(interp, listObjPtr, tokenPtr);
	    if (result != TCL_OK) {
	        Tcl_DecrRefCount(tokenPtr);
	        return;
	    }
    }

    /*
     * Now add child nodes 
     */
    if (TclDomHasChildren(nodePtr)) {
	    TclDomNode *childPtr = nodePtr->firstChildPtr;
	    while (childPtr) {
	        UpdateElementsByTagnameNodeList(interp, interpDataPtr,
				childPtr, tagname, listObjPtr);
	        childPtr = childPtr->nextSiblingPtr;
	    }
    }
}

/*
 *--------------------------------------------------------------
 * getElementsByTagNameUpdateProc --
 *
 *	NodeList update procedure implenting "getElementsByTagName"
 *
 *	Closure parameter is the tag name to match.
 */
static void getElementsByTagNameUpdateProc(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr,
    TclDomNode	*nodePtr,
    Tcl_Obj *closure,
    Tcl_Obj *resultPtr)
{
    char *tagname = Tcl_GetString(closure);
    if (TclDomHasChildren(nodePtr)) {
    	TclDomNode *childPtr = nodePtr->firstChildPtr;
	while (childPtr) {
	    UpdateElementsByTagnameNodeList(interp, interpDataPtr,
	    	childPtr, tagname, resultPtr);
	    childPtr = childPtr->nextSiblingPtr;
	}
    }
    return;
}

/*
 *--------------------------------------------------------------
 * TclDomGetElementsByTagname
 *	This procedure implements the "getElementsByTagname" method.
 *	See the DOM specification for further information.
 *
 * Results:
 *	Sets the interpreter's result to the name of 
 *	a Tcl global variable representing the NodeList.
 *
 * Side effects:
 *	Creates a Tcl global variable.
 *
 *--------------------------------------------------------------
 */

int
TclDomGetElementsByTagname(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    char *tagname,			/* Search pattern */
    TclDomNode *nodePtr)		/* Node to search */
{
    Tcl_Obj *closure = Tcl_NewStringObj(tagname, -1);
    Tcl_Obj *varNameObj =
    	TclDomRegisterLiveList(interp, interpDataPtr, nodePtr, 
	    getElementsByTagNameUpdateProc, closure);
    Tcl_SetObjResult(interp, varNameObj);
    return TCL_OK;
}

