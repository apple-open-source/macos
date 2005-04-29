/*
 * Author: Al Biggerstaff
 */
/*
 * traversal.c --
 *
 * This file contains routines supporting
 * the DOM Level 2 traversal interface.
 *
 * The NodeIterator interface supports a list-like
 * view of the document, where nodes are accessed 
 * in document order.
 *
 * The TreeWalker interface supports a tree view
 * of the document.
 * 
 * Copyright (c) 1999-2000 Ajuba Solutions
 *
 * $Id: traversal.c,v 1.4 2003/03/28 20:41:12 jenglish Exp $
 *
 */

#include "tclDomProInt.h"
#include <expat.h>

static int	NodeAtOrBefore(TclDomNode *nodePtr, TclDomNode *rootPtr,
		    unsigned int showMask, TclDomNodeFilter *filterPtr,
		    TclDomNode **previousNodePtrPtr);
static int	IteratorNodeAtOrBefore(TclDomNode *nodePtr, TclDomNode *rootPtr,
		    unsigned int showMask, TclDomNodeFilter *filterPtr,
		    TclDomNode **previousNodePtrPtr);
static int	IteratorNodeBefore(TclDomNode *nodePtr, TclDomNode *rootPtr,
		    unsigned int showMask, TclDomNodeFilter *filterPtr,
		    TclDomNode **previousNodePtrPtr);
static int	NodeAtOrAfter(TclDomNode *nodePtr, TclDomNode *rootPtr, 
		    unsigned int showMask, TclDomNodeFilter *filterPtr,
		    TclDomNode **nextNodePtrPtr);
static int	FirstChild(TclDomNode *nodePtr, TclDomNode *rootPtr, 
		    unsigned int showMask, TclDomNodeFilter *filterPtr,
		    TclDomNode **firstChildPtrPtr);
static int	LastChild(TclDomNode *nodePtr, TclDomNode *rootPtr, 
		    unsigned int showMask, TclDomNodeFilter *filterPtr,
		    TclDomNode **lastChildPtrPtr);
static int	ApplyFilter(TclDomNode *nodePtr, TclDomNodeFilter *filterPtr, 
		    int *filterValuePtr);
static int	NextSibling(TclDomNode *nodePtr, TclDomNode *rootNodePtr,
		    unsigned int showMask, TclDomNodeFilter *filterPtr,
		    TclDomNode **siblingPtrPtr);
static int	PreviousSibling(TclDomNode *nodePtr, TclDomNode *rootNodePtr, 
		    unsigned int showMask, TclDomNodeFilter *filterPtr, 
		    TclDomNode **siblingPtrPtr);


/*
 *--------------------------------------------------------------
 *
 * CheckNode
 *
 *	Determine whether a node belongs in a logical view of a tree,
 *	depending on the show mask and node filter.
 *	Note that the DOM spec requires that
 *	errors be propagated back to the user's code, e.g.,
 *	to "previousNode", "nextNode", etc., so we need
 *	to return the Tcl result.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if an internal error occurs.
 *	If the invoked procedure does not return an error, then
 *	the integer return value is written to the value
 *	pointed to by "filterValuePtr".
 *
 * Side effects:
 *	Tcl procedure is invoked.
 *
 *--------------------------------------------------------------
 */

static int CheckNode(
    TclDomNode *nodePtr,	/* Node to check */ 
    unsigned int showMask,	/* Bit flags of node types to match */
    TclDomNodeFilter *filterPtr,/* Node filter proc definition */ 
    int *filterValuePtr)	/* Return value of filter proc */
{
    if ((1 << (nodePtr->nodeType-1)) & showMask) {
        if (filterPtr == NULL || filterPtr->filterCmdPtr == NULL) {
            *filterValuePtr = DOM_ACCEPT;
            return TCL_OK;
        } else {
            return ApplyFilter(nodePtr, filterPtr, filterValuePtr);
        }
    } else {
	*filterValuePtr = DOM_SKIP;
	return TCL_OK;
    }
}


/*
 *--------------------------------------------------------------
 *
 * ApplyFilter
 *
 *	Invoke a filter procedure for NodeIterators or
 *	TreeWalkers. Note that the DOM spec requires that
 *	errors be propagated back to the user's code, e.g.,
 *	to "previousNode", "nextNode", etc., so we need
 *	to return the Tcl result.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if an internal error occurs.
 *	If the invoked procedure does not return an error, then
 *	the integer return value is written to the value
 *	pointed to by "filterValuePtr".
 *
 * Side effects:
 *	Tcl procedure is invoked.
 *
 *--------------------------------------------------------------
 */

static int 
ApplyFilter(
    TclDomNode *nodePtr,	/* Node to apply filter to */ 
    TclDomNodeFilter *filterPtr,/* Tcl proc for filter */
    int *filterValuePtr)	/* Return value from filter proc */
{
    if (filterPtr == NULL || filterPtr->filterCmdPtr == NULL) {
	*filterValuePtr = DOM_ACCEPT;
	return TCL_OK;
    } else {
	Tcl_Obj **objv;
	Tcl_Obj *cmdObjPtr;
	Tcl_Obj *resultObjPtr;
	int result;

	*filterValuePtr = DOM_SKIP;

	/*
	 * Append the node token to the filter command
	 */

	objv = (Tcl_Obj **) ckalloc(2 * sizeof(Tcl_Obj *));
	objv[0] = filterPtr->filterCmdPtr;
	objv[1] = TclDomGetNodeObj(filterPtr->interpDataPtr, nodePtr);

	cmdObjPtr = Tcl_ConcatObj(2, objv);
	ckfree((char *) objv);
	result = Tcl_EvalObjEx(filterPtr->interp, cmdObjPtr, TCL_EVAL_GLOBAL);
	if (result == TCL_OK) {
            int filterValue;
	    resultObjPtr = Tcl_GetObjResult(filterPtr->interp);

            result = Tcl_GetIntFromObj(filterPtr->interp, resultObjPtr, 
		    &filterValue);
            if (result != TCL_OK 
		    || (filterValue != DOM_ACCEPT && filterValue != DOM_SKIP 
		    && filterValue != DOM_REJECT)) {
		Tcl_SetResult(filterPtr->interp, 
			"invalid filter return value: should be \"dom::skip\", \"dom::accept\", or \"dom::reject\"", TCL_STATIC);
                return TCL_ERROR;
            } else {
                Tcl_ResetResult(filterPtr->interp);
                *filterValuePtr = filterValue;
                return TCL_OK;
            } 
	} else {
	    return result;
	}
    }
}


/*
 *--------------------------------------------------------------
 *
 * GetParent
 *
 *	Returns a node's parent node, in parentPtrPtr;
 *
 * Results:
 *      Normally returns TCL_OK; may propagate a TCL_ERROR from 
 *      the node filter proc.
 *
 * Side effects:
 *	May invoke a Tcl command.
 *
 *--------------------------------------------------------------
 */

static int 
GetParent(
    TclDomNode *nodePtr,	/* Node object */ 
    TclDomNode *rootNodePtr,	/* Root node for search */
    unsigned int showMask,	/* Node types in view */
    TclDomNodeFilter *filterPtr,/* Tcl proc to filter nodes */
    TclDomNode **parentPtrPtr)	/* Returned parent node value */
{
    int result;
    TclDomNode *newNodePtr;
    int acceptNode;

    *parentPtrPtr = NULL;

    if (nodePtr == NULL || nodePtr == rootNodePtr) {
        return TCL_OK;
    }

    newNodePtr = nodePtr->parentNodePtr;

    if (newNodePtr == NULL) {
        return TCL_OK;
    }

    result = CheckNode(newNodePtr, showMask, filterPtr, &acceptNode);
    if (result != TCL_OK) {
        return result;
    }

    if (acceptNode == DOM_ACCEPT) {
        *parentPtrPtr = newNodePtr;
        return TCL_OK;
    } else {
        return GetParent(newNodePtr, rootNodePtr, showMask, filterPtr, 
		parentPtrPtr);
    }
}


/*
 *--------------------------------------------------------------
 *
 * TclDomNodeBefore
 *
 *	Return a visible node that precedes a node in an 
 *	iteration.
 *
 * Results:
 *	A pointer to a TclDomNode.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int 
TclDomNodeBefore(
    TclDomNode *nodePtr,	/* Node */ 
    TclDomNode *rootNodePtr,	/* Root of tree being searched */
    unsigned int showMask,	/* Node types to include in view */
    TclDomNodeFilter *filterPtr,/* Tcl proc to filter nodes */
    TclDomNode **nodePtrPtr)	/* Return value for preceding node */
{
    TclDomNode *testNodePtr, *tempNodePtr;
    int result;

    result = PreviousSibling(nodePtr, rootNodePtr, showMask, filterPtr, 
	    &tempNodePtr);
    if (result != TCL_OK) {
        return result;
    }
    if (tempNodePtr) {
        while (TclDomHasChildren(tempNodePtr)) {
            result = LastChild(tempNodePtr, rootNodePtr, showMask, filterPtr, 
		    &testNodePtr);
            if (result != TCL_OK) {
                return result;
            }
            if (!testNodePtr) {
		break;
	    }
            tempNodePtr = testNodePtr;
        }
        *nodePtrPtr = tempNodePtr;
        return TCL_OK;
    }

    if (nodePtr != rootNodePtr) {
        int acceptNode;
        result = CheckNode(nodePtr->parentNodePtr, showMask, filterPtr, &acceptNode);
	if (result != TCL_OK) {
	    return result;
	}
	if (acceptNode == DOM_ACCEPT) {
	    *nodePtrPtr = nodePtr->parentNodePtr;
	    return TCL_OK;
	}
    }

    if (nodePtr != rootNodePtr) {
	if (nodePtr->parentNodePtr) {
	    result = NodeAtOrBefore(nodePtr->parentNodePtr, rootNodePtr, 
		    showMask, filterPtr, &testNodePtr);
	    if (result != TCL_OK) {
		return result;
	    }
	    *nodePtrPtr = testNodePtr;
	    return TCL_OK;
	}
    }

    *nodePtrPtr = NULL;
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * IteratorPreviousNode
 *
 *	Helper routine to return the node that precedes a node
 *      in a Node Iterator iteration.
 *
 * Results:
 *	A pointer to a TclDomNode.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static TclDomNode *
IteratorPreviousNode(
    TclDomNode *nodePtr,    /* Starting node */
    TclDomNode *rootNodePtr)/* Root node for iteration */
{
    TclDomNode *newNodePtr;

    if (nodePtr == NULL || nodePtr == rootNodePtr) {
        return NULL;
    }

    newNodePtr = nodePtr->previousSiblingPtr;
    if (newNodePtr == NULL) {
        return nodePtr->parentNodePtr;
    }

    if (TclDomHasChildren(newNodePtr)) {
        while (TclDomHasChildren(newNodePtr)) {
            newNodePtr = newNodePtr->lastChildPtr;
        }
    }
    return newNodePtr;
}


/*
 *--------------------------------------------------------------
 *
 * IteratorNextNode
 *
 *	Helper routine to return the node that follows a node
 *      in a Node Iterator iteration.
 *
 * Results:
 *	A pointer to a TclDomNode.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static TclDomNode *
IteratorNextNode(
    TclDomNode *nodePtr,    /* Node to start from */
    TclDomNode *rootNodePtr)/* Root node for view */
{
    TclDomNode *newNodePtr;

    if (nodePtr == NULL) {
        return NULL;
    } 

    if (TclDomHasChildren(nodePtr)) {
        return nodePtr->firstChildPtr;
    }

    if (nodePtr->nextSiblingPtr) {
        return nodePtr->nextSiblingPtr;
    }

    newNodePtr = nodePtr->parentNodePtr;
    while (newNodePtr && newNodePtr != rootNodePtr) {
        if (newNodePtr->nextSiblingPtr) {
            return newNodePtr->nextSiblingPtr;
        } else {
            newNodePtr = newNodePtr->parentNodePtr;
        }
    }
    return NULL;
}


/*
 *--------------------------------------------------------------
 *
 * IteratorNodeBefore
 *
 *	Return a node that precedes a node in an 
 *	Node Iterator iteration.
 *
 * Results:
 *	Normally TCL_OK; TCL_ERROR is returns if an error occurred
 *      in a filter procedure during the search. The new
 *      node is returned in "nodePtrPtr".
 *
 * Side effects:
 *	May invoke a Tcl procedure.
 *
 *--------------------------------------------------------------
 */

static int 
IteratorNodeBefore(
    TclDomNode *nodePtr,    /* Node */
    TclDomNode *rootNodePtr,/* Root node for tree */
    unsigned int showMask,  /* Flags for nodes types in view */
    TclDomNodeFilter *filterPtr, /* Tcl proc to filter nodes */
    TclDomNode **nodePtrPtr)/* Return preceding node */
{
    TclDomNode *tempNodePtr;
    int result, acceptNode;

    *nodePtrPtr = NULL;

    tempNodePtr = nodePtr;

    while (1) {
        tempNodePtr = IteratorPreviousNode(tempNodePtr, rootNodePtr);
        if (tempNodePtr == NULL) {
            return TCL_OK;
        }

        result = CheckNode(tempNodePtr, showMask, filterPtr, &acceptNode);
        if (result == TCL_ERROR) {
            return result;
        }
        if (acceptNode == DOM_ACCEPT) {
            *nodePtrPtr = tempNodePtr;
            return TCL_OK;
        }
    }
}


/*
 *--------------------------------------------------------------
 *
 * IteratorNodeAfter
 *
 *	Return a node that follows a node in a
 *	Node Iterator iteration.
 *
 * Results:
 *	Normally TCL_OK; TCL_ERROR is returned if an error occurred
 *      in a filter procedure during the search. The new
 *      node is returned in "nodePtrPtr".
 *
 * Side effects:
 *	May invoke a Tcl procedure.
 *
 *--------------------------------------------------------------
 */

static int 
IteratorNodeAfter(
    TclDomNode *nodePtr,    /* Node */ 
    TclDomNode *rootNodePtr,/* Root node for tree */ 
    unsigned int showMask,  /* Flags for node types in view */
    TclDomNodeFilter *filterPtr, /* Tcl proc for node filter */
    TclDomNode **nodePtrPtr)/* Location in which to return next node */
{
    TclDomNode *tempNodePtr;
    int result, acceptNode;

    *nodePtrPtr = NULL;

    tempNodePtr = nodePtr;

    while (1) {
        tempNodePtr = IteratorNextNode(tempNodePtr, rootNodePtr);
        if (tempNodePtr == NULL) {
            return TCL_OK;
        }

        result = CheckNode(tempNodePtr, showMask, filterPtr, &acceptNode);
        if (result == TCL_ERROR) {
            return result;
        }
        if (acceptNode == DOM_ACCEPT) {
            *nodePtrPtr = tempNodePtr;
            return TCL_OK;
        }
    }
}


/*
 *--------------------------------------------------------------
 *
 * TclDomTreeWalkerPreviousNode
 *
 *	Return a node that precedes a node in a
 *	TreeWalker traversal.
 *
 * Results:
 *	Normally TCL_OK; TCL_ERROR if an error occurred
 *      in a filter procedure during the search. The new
 *      node is returned in "nodePtrPtr".
 *
 * Side effects:
 *	May invoke a Tcl procedure.
 *
 *--------------------------------------------------------------
 */

int 
TclDomTreeWalkerPreviousNode(
    TclDomNode *nodePtr,	/* Current node */
    TclDomNode *rootNodePtr,	/* Root node for tree */
    unsigned int showMask,	/* Flags for node types in view */
    TclDomNodeFilter *filterPtr,/* Tcl procedure for node filter */
    TclDomNode **previousNodePtrPtr) /* Return location for new node */
{
    int result;
    TclDomNode *newNodePtr, *lastChildPtr;

    *previousNodePtrPtr = NULL;

    if (nodePtr == NULL) {
        return TCL_OK;
    }

    result = PreviousSibling(nodePtr, rootNodePtr, showMask, filterPtr, 
			&newNodePtr);
    if (result != TCL_OK) {
        return result;
    }

    if (newNodePtr == NULL) {
        result = GetParent(nodePtr, rootNodePtr, showMask, filterPtr, 
				&newNodePtr);
        if (result != TCL_OK) {
            return result;
        }
        *previousNodePtrPtr = newNodePtr;
        return TCL_OK;
    }

    result = LastChild(newNodePtr, rootNodePtr, showMask, filterPtr, 
			&lastChildPtr);
    if (result != TCL_OK) {
        return result;
    }
    
    if (lastChildPtr) {
        *previousNodePtrPtr = lastChildPtr;
        return TCL_OK;
    }

    *previousNodePtrPtr = newNodePtr;
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TreeWalkerNextNode
 *
 *	Return a node that follows a node in a
 *	TreeWalker traversal.
 *
 * Results:
 *	Normally TCL_OK; TCL_ERROR if an error occurred
 *      in a filter procedure during the search. The new
 *      node is returned in "nodePtrPtr".
 *
 * Side effects:
 *	May invoke a Tcl procedure.
 *
 *--------------------------------------------------------------
 */

static int 
TreeWalkerNextNode(
    TclDomNode *nodePtr,    /* Current node */
    TclDomNode *rootNodePtr,/* Root node for tree being walked */
    unsigned int showMask,  /* Flags for node types in view */
    TclDomNodeFilter *filterPtr, /* Tcl proc for node filter */
    TclDomNode **nextNodePtrPtr) /* Location to return next node in tree walk */
{
    int result;
    TclDomNode *newNodePtr, *parentNodePtr;

    *nextNodePtrPtr = NULL;

    if (nodePtr == NULL) {
        return TCL_OK;
    }

    result = FirstChild(nodePtr, rootNodePtr, showMask, filterPtr, &newNodePtr);
    if (result != TCL_OK) {
        return result;
    }
    if (newNodePtr != NULL) {
        *nextNodePtrPtr = newNodePtr;
        return TCL_OK;
    }

    result = NextSibling(nodePtr, rootNodePtr, showMask, filterPtr, 
			&newNodePtr);
    if (result != TCL_OK) {
        return result;
    }
    if (newNodePtr != NULL) {
        *nextNodePtrPtr = newNodePtr;
        return TCL_OK;
    }

    result = GetParent(nodePtr, rootNodePtr, showMask, filterPtr, 
			&parentNodePtr);
    if (result != TCL_OK) {
        return result;
    }
    while (parentNodePtr) {
        result = NextSibling(parentNodePtr, rootNodePtr, showMask, filterPtr, 
				&newNodePtr);
        if (result != TCL_OK) {
            return result;
        }
        if (newNodePtr) {
            *nextNodePtrPtr = newNodePtr;
            return TCL_OK;
        } else {
            result = GetParent(parentNodePtr, rootNodePtr, showMask, 
					filterPtr, &parentNodePtr);
            if (result != TCL_OK) {
                return result;
            }
        }
    }

    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * NodeAtOrBefore
 *
 *	Validate that a node is one of the node types that
 *	is visible to an iterator. It the node is not visible,
 *	then return the first node preceding it that is visible.
 *
 * Results:
 *	A pointer to a TclDomNode.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int 
NodeAtOrBefore(
    TclDomNode *nodePtr,    /* Node */
    TclDomNode *rootNodePtr,/* Root node for iteration */
    unsigned int showMask,  /* Bit flags for node types in view */
    TclDomNodeFilter *filterPtr, /* Tcl proc for node filter */
    TclDomNode **previousNodePtrPtr) /* Location to return previous node */
{
    int result, acceptNode;
    result = CheckNode(nodePtr, showMask, filterPtr, &acceptNode);
    if (result != TCL_OK) {
	return result;
    }
    if (acceptNode == DOM_ACCEPT) {
	*previousNodePtrPtr = nodePtr;
	return TCL_OK;
    }
    return TclDomNodeBefore(nodePtr, rootNodePtr, showMask, filterPtr, 
	    previousNodePtrPtr);
}


/*
 *--------------------------------------------------------------
 *
 * IteratorNodeAtOrBefore
 *
 *	Validate that a node is one of the node types that
 *	is visible to an iterator. It the node is not visible,
 *	then return the first node preceding it that is visible.
 *
 * Results:
 *	A pointer to a TclDomNode.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int 
IteratorNodeAtOrBefore(
    TclDomNode *nodePtr, 
    TclDomNode *rootNodePtr,
    unsigned int showMask, 
    TclDomNodeFilter *filterPtr, 
    TclDomNode **previousNodePtrPtr)
{
    int result, acceptNode;
    result = CheckNode(nodePtr, showMask, filterPtr, &acceptNode);
    if (result != TCL_OK) {
	return result;
    }
    if (acceptNode == DOM_ACCEPT) {
	*previousNodePtrPtr = nodePtr;
	return TCL_OK;
    }
    return IteratorNodeBefore(nodePtr, rootNodePtr, showMask, filterPtr, 
	    previousNodePtrPtr);
}


/*
 *--------------------------------------------------------------
 *
 * NodeAtOrAfter
 *
 *	Validate that a node is one of the node types that
 *	is visible to an iterator. It the node is not visible,
 *	then return the first node following it that is visible.
 *
 * Results:
 *	A pointer to a TclDomNode.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int 
NodeAtOrAfter(
    TclDomNode *nodePtr, 
    TclDomNode *rootNodePtr, 
    unsigned int showMask, 
    TclDomNodeFilter *filterPtr, 
    TclDomNode **nextNodePtrPtr)
{
    int result, acceptNode;
    result = CheckNode(nodePtr, showMask, filterPtr, &acceptNode);
    if (result != TCL_OK) {
	return result;
    }
    if (acceptNode == DOM_ACCEPT) {
	*nextNodePtrPtr = nodePtr;
	return TCL_OK;
    }
    return TclDomNodeAfter(nodePtr, rootNodePtr, showMask, filterPtr, 
	nextNodePtrPtr);  
}


/*
 *--------------------------------------------------------------
 *
 * IteratorNodeAtOrAfter
 *
 *	Validate that a node is one of the node types that
 *	is visible to an iterator. It the node is not visible,
 *	then return the first node following it that is visible.
 *
 * Results:
 *	A pointer to a TclDomNode.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int 
IteratorNodeAtOrAfter(
    TclDomNode *nodePtr, 
    TclDomNode *rootNodePtr, 
    unsigned int showMask, 
    TclDomNodeFilter *filterPtr, 
    TclDomNode **nextNodePtrPtr)
{
    int result, acceptNode;
    result = CheckNode(nodePtr, showMask, filterPtr, &acceptNode);
    if (result != TCL_OK) {
	return result;
    }
    if (acceptNode == DOM_ACCEPT) {
	*nextNodePtrPtr = nodePtr;
	return TCL_OK;
    }
    return IteratorNodeAfter(nodePtr, rootNodePtr, showMask, filterPtr, 
	    nextNodePtrPtr); 
}


/*
 *--------------------------------------------------------------
 *
 * NodeNonChildAfter
 *
 *	Returns a node's successor that is not a child, 
 *      in nextNodePtrPtr;
 *
 * Results:
 *      Normally returns TCL_OK; may propagate a TCL_ERROR from 
 *      the node filter proc.
 *
 * Side effects:
 *	May invoke a Tcl command.
 *
 *--------------------------------------------------------------
 */

static int NodeNonChildAfter(
    TclDomNode *nodePtr, 
    TclDomNode *rootNodePtr, 
    unsigned int showMask, 
    TclDomNodeFilter *filterPtr, 
    TclDomNode **nextNodePtrPtr)
{
    TclDomNode *testNodePtr;
    int result;

    result = NextSibling(nodePtr, rootNodePtr, showMask, filterPtr, 
	    &testNodePtr);
    if (result != TCL_OK) {
        return result;
    }
    if (testNodePtr) {
        *nextNodePtrPtr = testNodePtr;
        return TCL_OK;
    }

    if (nodePtr != rootNodePtr) {
	if (nodePtr->parentNodePtr) {
	    result = NodeNonChildAfter(nodePtr->parentNodePtr, rootNodePtr, 
		    showMask, filterPtr, &testNodePtr);
	    if (result != TCL_OK) {
	        return result;
	    }
	    *nextNodePtrPtr = testNodePtr;
	    return TCL_OK;
	}
    }
    *nextNodePtrPtr = NULL;

    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomNodeAfter
 *
 *	Return a visible node that follows a node in an 
 *	iteration.
 *
 * Results:
 *	A pointer to a TclDomNode.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int 
TclDomNodeAfter(TclDomNode *nodePtr, TclDomNode *rootNodePtr, 
	unsigned int showMask, TclDomNodeFilter *filterPtr, 
	TclDomNode **nextNodePtrPtr)
{
    TclDomNode *testNodePtr;
    int result;

    /*
     * First check the node's children
     */

    result = FirstChild(nodePtr, rootNodePtr, showMask, filterPtr, 
	    &testNodePtr);
    if (result != TCL_OK) {
        return result;
    }

    if (testNodePtr) {
        *nextNodePtrPtr = testNodePtr;
        return TCL_OK;
    }

    /*
     * Then check the node's siblings
     */

    result = NextSibling(nodePtr, rootNodePtr, showMask, filterPtr, 
	    &testNodePtr);
    if (result != TCL_OK) {
        return result;
    }

    if (testNodePtr) {
        *nextNodePtrPtr = testNodePtr;
        return TCL_OK;
    }

    /*
     * Finally, recurse up the tree
     */

    if (nodePtr != rootNodePtr) {
	if (nodePtr->parentNodePtr) {
	    result = NodeNonChildAfter(nodePtr->parentNodePtr, rootNodePtr, 
		    showMask, filterPtr, &testNodePtr);
	    if (result != TCL_OK) {
		return result;
	    }
	    *nextNodePtrPtr = testNodePtr;
	    return TCL_OK;
	}
    }

    *nextNodePtrPtr = NULL;
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * FirstChild
 *
 *	Return the node that is the first child of a node in
 *	a logical view of the tree.
 *
 * Results:
 *	A pointer to a TclDomNode.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int 
FirstChild(TclDomNode *nodePtr, TclDomNode *rootNodePtr, unsigned int showMask,
    TclDomNodeFilter *filterPtr, TclDomNode **childPtrPtr)
{
    TclDomNode *newNodePtr;
    int result;

    *childPtrPtr = NULL;
    if (nodePtr == NULL) {
        return TCL_OK;
    }

    if (nodePtr->nodeType == ELEMENT_NODE 
	    || nodePtr->nodeType == DOCUMENT_FRAGMENT_NODE
	    || nodePtr->nodeType == DOCUMENT_NODE) {
        int acceptNode;

        newNodePtr = nodePtr->firstChildPtr;
        if (newNodePtr == NULL) {
            return TCL_OK;
        }

        result = CheckNode(newNodePtr, showMask, filterPtr, &acceptNode);
        if (result != TCL_OK) {
            return result;
        }

        if (acceptNode == DOM_ACCEPT) {
            *childPtrPtr = newNodePtr;
            return TCL_OK;
        } else if (acceptNode == DOM_SKIP  && TclDomHasChildren(newNodePtr)) {
            return FirstChild(newNodePtr, rootNodePtr, showMask, filterPtr, 
		    childPtrPtr);
        } else {
            return NextSibling(newNodePtr, rootNodePtr, showMask, filterPtr, 
		    childPtrPtr);
        }
    } else {
        return TCL_OK;
    }
}


/*
 *--------------------------------------------------------------
 *
 * LastChild
 *
 *	Return the node that is the last child of a node in
 *	a logical view of the tree.
 *
 * Results:
 *	A pointer to a TclDomNode.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int 
LastChild(TclDomNode *nodePtr, TclDomNode *rootNodePtr, unsigned int showMask,
    TclDomNodeFilter *filterPtr, TclDomNode **childPtrPtr)
{
    TclDomNode *newNodePtr;
    int acceptNode;
    int result;

    *childPtrPtr = NULL;

    if (nodePtr == NULL) {
        return TCL_OK;
    }

    if (TclDomHasChildren(nodePtr) == 0) {
        return TCL_OK;
    }

    newNodePtr = nodePtr->lastChildPtr;
    if (newNodePtr == NULL) {
        return TCL_OK;
    }

    result = CheckNode(newNodePtr, showMask, filterPtr, &acceptNode);
    if (result != TCL_OK) {
        return result;
    }

    if (acceptNode == DOM_ACCEPT) {
        *childPtrPtr = newNodePtr;
        return TCL_OK;
    } else if (acceptNode == DOM_SKIP && TclDomHasChildren(newNodePtr)) {
        return LastChild(newNodePtr, rootNodePtr, showMask, filterPtr, 
		childPtrPtr);
    } else {
        return PreviousSibling(newNodePtr, rootNodePtr, showMask, filterPtr, 
		childPtrPtr);
    }
}


/*
 *--------------------------------------------------------------
 *
 * PreviousSibling
 *
 *	Returns a node's previous sibling in siblingPtrPtr;
 *
 * Results:
 *      Normally returns TCL_OK; may propagate a TCL_ERROR from 
 *      the node filter proc.
 *
 * Side effects:
 *	May invoke a Tcl command.
 *
 *--------------------------------------------------------------
 */

static int PreviousSibling(TclDomNode *nodePtr, TclDomNode *rootNodePtr, 
	unsigned int showMask, TclDomNodeFilter *filterPtr, 
	TclDomNode **siblingPtrPtr)
{
    TclDomNode *newNodePtr;
    int result;

    *siblingPtrPtr = NULL;

    if (nodePtr == NULL || nodePtr == rootNodePtr) {
        return TCL_OK;
    }

    newNodePtr = nodePtr->previousSiblingPtr;
    if (newNodePtr == NULL) {
        int acceptParent;
        newNodePtr = nodePtr->parentNodePtr;
        if (newNodePtr == NULL || newNodePtr == rootNodePtr) {
            return TCL_OK;
        }
        result = CheckNode(newNodePtr, showMask, filterPtr, &acceptParent);
        if (result != TCL_OK) {
            return result;
        }
        if (acceptParent == DOM_SKIP || acceptParent == DOM_REJECT) {
            return PreviousSibling(newNodePtr, rootNodePtr, showMask, 
					filterPtr, siblingPtrPtr);
        } 
        return TCL_OK;
    } else {
        int acceptNode;
        result = CheckNode(newNodePtr, showMask, filterPtr, &acceptNode);
        if (result != TCL_OK) {
            return result;
        }

        if (acceptNode == DOM_ACCEPT) {
            *siblingPtrPtr = newNodePtr;
            return TCL_OK;
        } else if (acceptNode == DOM_SKIP) {
            TclDomNode *childPtr;
            result = LastChild(newNodePtr, rootNodePtr, showMask, filterPtr, 
		    &childPtr);
            if (result != TCL_OK) {
                return result;
            }
            if (childPtr == NULL) {
                return PreviousSibling(newNodePtr, rootNodePtr, showMask, 
			filterPtr, siblingPtrPtr);
            } else {
                *siblingPtrPtr = childPtr;
                return TCL_OK;
            }
        } else {
            return PreviousSibling(newNodePtr, rootNodePtr, showMask, 
		    filterPtr, siblingPtrPtr);
        }
    }
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * NextSibling
 *
 *	Returns a node's next sibling in siblingPtrPtr;
 *
 * Results:
 *      Normally returns TCL_OK; may propagate a TCL_ERROR from 
 *      the node filter proc.
 *
 * Side effects:
 *	May invoke a Tcl command.
 *
 *--------------------------------------------------------------
 */

static int NextSibling(
    TclDomNode *nodePtr,
    TclDomNode *rootNodePtr,
    unsigned int showMask,
    TclDomNodeFilter *filterPtr,
    TclDomNode **siblingPtrPtr)
{
    int result;
    int acceptNode;

    TclDomNode *newNodePtr;

    *siblingPtrPtr = NULL;

    if (nodePtr == NULL || nodePtr == rootNodePtr) {
        return TCL_OK;
    }

    newNodePtr = nodePtr->nextSiblingPtr;

    if (newNodePtr == NULL) {
        int acceptParent;
        newNodePtr = nodePtr->parentNodePtr;
        if (newNodePtr == NULL || newNodePtr == rootNodePtr) {
            return TCL_OK;
        }

        result = CheckNode(newNodePtr, showMask, filterPtr, &acceptParent);
        if (result != TCL_OK) {
	    return result;
	}
        if (acceptParent == DOM_SKIP) {
            return NextSibling(newNodePtr, rootNodePtr, showMask, filterPtr, 
		    siblingPtrPtr);
        } else {
            return TCL_OK;
        }
    } else {
        result = CheckNode(newNodePtr, showMask, filterPtr, &acceptNode);
        if (result != TCL_OK) {
            return result;
        }
        if (acceptNode == DOM_ACCEPT) {
            *siblingPtrPtr = newNodePtr;
            return TCL_OK;
        } else if (acceptNode == DOM_SKIP) {
            TclDomNode *childNodePtr;
            result = FirstChild(newNodePtr, rootNodePtr, showMask, filterPtr,
		    &childNodePtr);
            if (result != TCL_OK) {
                return TCL_ERROR;
            }
            if (childNodePtr == NULL) {
                return NextSibling(newNodePtr, rootNodePtr, showMask, 
			filterPtr, siblingPtrPtr);
            } else {
                *siblingPtrPtr = childNodePtr;
                return TCL_OK;
            }
        } else {
            return NextSibling(newNodePtr, rootNodePtr, showMask, filterPtr, 
		    siblingPtrPtr);
        }
    }
}


/*
 *--------------------------------------------------------------
 *
 * DestroyTreeWalkter
 *
 *	This procedure does the actual work of deleting a 
 *      TreeWalker and its allocated memory.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void 
DestroyTreeWalker(
    char *clientData)
{
    TclDomTreeWalker *treeWalkerPtr = (TclDomTreeWalker *) clientData;
    if (treeWalkerPtr->filterPtr) {
        Tcl_DecrRefCount(treeWalkerPtr->filterPtr->filterCmdPtr);
	ckfree((char *) treeWalkerPtr->filterPtr);
    }
    Tcl_DeleteHashEntry(treeWalkerPtr->entryPtr);
    ckfree((char *) treeWalkerPtr);
} 


/*
 *--------------------------------------------------------------
 *
 * DestroyNodeIterator
 *
 *	This procedure does the actual work of deleting a 
 *      NodeIterator and its allocated memory.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void 
DestroyNodeIterator(
    char *dataPtr)
{
    TclDomNodeIterator *nodeIteratorPtr = (TclDomNodeIterator *) dataPtr;

    if (nodeIteratorPtr->filterPtr) {
	Tcl_DecrRefCount(nodeIteratorPtr->filterPtr->filterCmdPtr);
	ckfree((char *) nodeIteratorPtr->filterPtr);
    }
    Tcl_DeleteHashEntry(nodeIteratorPtr->entryPtr);
    ckfree((char *) nodeIteratorPtr);
} 


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateNodeIterator
 *
 *	This procedure creates a node iterator for the root
 *	subtree. A token for the iterator is
 *	written to the interpreter's result.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if an internal error occurs.
 *
 * Side effects:
 *	A token is allocated for the iterator.
 *
 *--------------------------------------------------------------
 */

int 
TclDomCreateNodeIterator(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* State information */
    TclDomNode *rootNodePtr,		/* Root node for iteration */
    unsigned int whatToShow,		/* Bit map of node types to visit */
    Tcl_Obj *filterObjPtr,		/* Optional Tcl filter proc */
    int expandEntityReferences)		/* Not currently used */
{
    char workString[32];
    TclDomNodeIterator *nodeIteratorPtr;
    Tcl_HashEntry *entryPtr;
    int newFlag, id;
    Tcl_Obj *objPtr;

    nodeIteratorPtr = (TclDomNodeIterator *) 
	    ckalloc(sizeof(TclDomNodeIterator));
    memset(nodeIteratorPtr, 0, sizeof(TclDomNodeIterator));

    nodeIteratorPtr->interp = interp;
    nodeIteratorPtr->interpDataPtr = interpDataPtr;
    nodeIteratorPtr->rootPtr = rootNodePtr;
    nodeIteratorPtr->referencePtr = rootNodePtr;
    nodeIteratorPtr->whatToShow = whatToShow;
    if (filterObjPtr) {
	Tcl_IncrRefCount(filterObjPtr);
	nodeIteratorPtr->filterPtr = (TclDomNodeFilter *) 
		ckalloc(sizeof(TclDomNodeFilter));
	nodeIteratorPtr->filterPtr->interp = interp;
	nodeIteratorPtr->filterPtr->interpDataPtr = interpDataPtr;
	nodeIteratorPtr->filterPtr->filterCmdPtr = filterObjPtr;
    }

    id = ++interpDataPtr->nodeSeed;

    /*
     * Catalog the iterator object
     */

    sprintf(workString, "iterator%u", id);
    entryPtr = Tcl_CreateHashEntry(&interpDataPtr->iteratorHashTable, 
	    workString, &newFlag);
    if (entryPtr == NULL) {
	Tcl_AppendResult(interp, "couldn't create nodeIterator", 
		(char *) NULL);
	return TCL_ERROR;
    }
    Tcl_SetHashValue(entryPtr, nodeIteratorPtr);
    nodeIteratorPtr->entryPtr = entryPtr;

    objPtr = Tcl_NewStringObj(workString, -1);
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}



/*
 *--------------------------------------------------------------
 *
 * TclDomGetPreviousNodeFromIterator
 *
 *	Implements the NodeIterator previousNode method.
 *
 * Results:
 *	TCL_OK. If a previous node was found, then its token
 *	is written to the interpreter's result.
 *
 * Side effects:
 *	A node token may be created.
 *
 *--------------------------------------------------------------
 */

int 
TclDomGetPreviousNodeFromIterator(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr,
    TclDomNodeIterator *nodeIteratorPtr)
{
    int result;
    TclDomNode *previousNodePtr = NULL;

    if (nodeIteratorPtr->referencePtr == NULL) {
	    return TCL_OK;
    }

    Tcl_Preserve((ClientData) nodeIteratorPtr->rootPtr->containingDocumentPtr);

    if (nodeIteratorPtr->position == REFERENCE_IS_BEFORE_ITERATOR) {
	result = IteratorNodeAtOrBefore(nodeIteratorPtr->referencePtr,
		nodeIteratorPtr->rootPtr, nodeIteratorPtr->whatToShow, 
		nodeIteratorPtr->filterPtr, &previousNodePtr);
	nodeIteratorPtr->position = REFERENCE_IS_AFTER_ITERATOR;
    } else {
	result = IteratorNodeBefore(nodeIteratorPtr->referencePtr,
		nodeIteratorPtr->rootPtr, nodeIteratorPtr->whatToShow,
		nodeIteratorPtr->filterPtr, &previousNodePtr);
    }

    if (result == TCL_OK && previousNodePtr) {
	nodeIteratorPtr->referencePtr = previousNodePtr;
	result = TclDomSetNodeResult(interp, interpDataPtr, previousNodePtr);
    }
    
    Tcl_Release((ClientData) nodeIteratorPtr->rootPtr->containingDocumentPtr);

    return result;
} 


/*
 *--------------------------------------------------------------
 *
 * TclDomGetNextNodeFromIterator
 *
 *	Implements the NodeIterator nextNode method.
 *
 * Results:
 *	TCL_OK. If a next node was found, then its token
 *	is written to the interpreter's result.
 *
 * Side effects:
 *	A node token may be created.
 *
 *--------------------------------------------------------------
 */

int 
TclDomGetNextNodeFromIterator(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr,
    TclDomNodeIterator *nodeIteratorPtr)
{
    TclDomNode *nextNodePtr;
    int result;

    if (nodeIteratorPtr->referencePtr == NULL) {
	return TCL_OK;
    }

    Tcl_Preserve((ClientData) nodeIteratorPtr->rootPtr->containingDocumentPtr);

    if (nodeIteratorPtr->position == REFERENCE_IS_AFTER_ITERATOR) {
	result = IteratorNodeAtOrAfter(nodeIteratorPtr->referencePtr,
		nodeIteratorPtr->rootPtr, nodeIteratorPtr->whatToShow,
		nodeIteratorPtr->filterPtr, &nextNodePtr);
	nodeIteratorPtr->position = REFERENCE_IS_BEFORE_ITERATOR;
    } else {
	result = IteratorNodeAfter(nodeIteratorPtr->referencePtr,
		nodeIteratorPtr->rootPtr, nodeIteratorPtr->whatToShow,
		nodeIteratorPtr->filterPtr, &nextNodePtr);
    }

    if (result == TCL_OK && nextNodePtr) {
	nodeIteratorPtr->referencePtr = nextNodePtr;
	result = TclDomSetNodeResult(interp, interpDataPtr, nextNodePtr);
    } 

    Tcl_Release((ClientData) nodeIteratorPtr->rootPtr->containingDocumentPtr);

    return result;
}



/*
 *--------------------------------------------------------------
 *
 * TclDomDeleteNodeIterator
 *
 *	This procedure deletes a NodeIterator.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void 
TclDomDeleteNodeIterator(
    TclDomNodeIterator *nodeIteratorPtr)
{
    Tcl_EventuallyFree((ClientData) nodeIteratorPtr, DestroyNodeIterator);
} 


/*
 *--------------------------------------------------------------
 *
 * TclDomGetNodeIteratorFromToken
 *
 *	This procedure maps a TclDomPro node iterator token
 *	into a TclDomNodeIterator pointer.
 *
 * Results:
 *	A pointer to the TclDomNode, or null if the
 *	token can't be found.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

TclDomNodeIterator *
TclDomGetNodeIteratorFromToken(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    Tcl_Obj *nodeTokenPtr)		/* Token string value */
{
    char *token;
    Tcl_HashEntry *entryPtr;
    TclDomNodeIterator *nodeIteratorPtr;

    token = Tcl_GetStringFromObj(nodeTokenPtr, NULL);

    entryPtr = Tcl_FindHashEntry(&interpDataPtr->iteratorHashTable, token);
    if (entryPtr == NULL) {
	Tcl_AppendResult(interp, "token not found", NULL);
	return NULL;
    }
    nodeIteratorPtr = (TclDomNodeIterator *) Tcl_GetHashValue(entryPtr);
    return nodeIteratorPtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCreateTreeWalker
 *
 *	This procedure creates a tree walker for the root
 *	subtree. A token for the TreeWalker is
 *	written to the interpreter's result.
 *
 * Results:
 *	Returns TCL_OK, or TCL_ERROR if an internal error occurs.
 *
 * Side effects:
 *	A token is allocated for the TreeWalker.
 *
 *--------------------------------------------------------------
 */

int 
TclDomCreateTreeWalker(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr, 
    TclDomNode *rootNodePtr,
    unsigned int whatToShow,
    Tcl_Obj *filterObjPtr, 
    int expandEntityReferences)
{
    char workString[32];
    TclDomTreeWalker *treeWalkerPtr;
    Tcl_HashEntry *entryPtr;
    int newFlag, id;
    Tcl_Obj *objPtr;

    treeWalkerPtr = (TclDomTreeWalker *) ckalloc(sizeof(TclDomTreeWalker));
    memset(treeWalkerPtr, 0, sizeof(TclDomTreeWalker));

    treeWalkerPtr->interp = interp;
    treeWalkerPtr->interpDataPtr = interpDataPtr;
    treeWalkerPtr->rootPtr = rootNodePtr;
    treeWalkerPtr->currentNodePtr = rootNodePtr;
    treeWalkerPtr->whatToShow = whatToShow;
    if (filterObjPtr) {
	Tcl_IncrRefCount(filterObjPtr);
	treeWalkerPtr->filterPtr =
		(TclDomNodeFilter *) ckalloc(sizeof(TclDomNodeFilter));
	treeWalkerPtr->filterPtr->interp = interp;
	treeWalkerPtr->filterPtr->interpDataPtr = interpDataPtr;
	treeWalkerPtr->filterPtr->filterCmdPtr = filterObjPtr;
    }

    id = ++interpDataPtr->nodeSeed;

    /*
     * Catalog the iterator object
     */

    sprintf(workString, "treewalker%u", id);
    entryPtr = Tcl_CreateHashEntry(&interpDataPtr->treeWalkerHashTable,
	    workString, &newFlag);
    if (entryPtr == NULL) {
	    Tcl_AppendResult(interp, "couldn't create treeWalker", (char *) NULL);
	    return TCL_ERROR;
    }
    Tcl_SetHashValue(entryPtr, treeWalkerPtr);
    treeWalkerPtr->entryPtr = entryPtr;

    objPtr = Tcl_NewStringObj(workString, -1);
    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomGetTreeWalkerFromToken
 *
 *	This procedure maps a TclDomPro TreeWalker token
 *	into a TclDomTreeWalker pointer.
 *
 * Results:
 *	A pointer to the TclDomTreeWalker, or null if the
 *	token can't be found.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */


TclDomTreeWalker *
TclDomGetTreeWalkerFromToken(
    Tcl_Interp *interp,			/* Tcl interpreter */
    TclDomInterpData *interpDataPtr,	/* Extension state data */
    Tcl_Obj *nodeTokenPtr)		/* Token string value */
{
    char *token;
    Tcl_HashEntry *entryPtr;
    TclDomTreeWalker *treeWalkerPtr;

    token = Tcl_GetStringFromObj(nodeTokenPtr, NULL);

    entryPtr = Tcl_FindHashEntry(&interpDataPtr->treeWalkerHashTable, token);
    if (entryPtr == NULL) {
	Tcl_AppendResult(interp, "token not found", NULL);
	return NULL;
    }
    treeWalkerPtr = (TclDomTreeWalker *) Tcl_GetHashValue(entryPtr);
    return treeWalkerPtr;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomDeleteTreeWalker
 *
 *	This procedure deletes a TreeWalker.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void 
TclDomDeleteTreeWalker(
    TclDomTreeWalker *treeWalkerPtr)
{
    Tcl_EventuallyFree((ClientData) treeWalkerPtr, DestroyTreeWalker);
} 


/*
 *--------------------------------------------------------------
 *
 * TclDomGetNextNodeFromTreeWalker
 *
 *	Implements the TreeWalker nextNode method.
 *
 * Results:
 *	TCL_OK. If a next node was found, then its token
 *	is written to the interpreter's result.
 *
 * Side effects:
 *	A node token may be created.
 *
 *--------------------------------------------------------------
 */

int 
TclDomGetNextNodeFromTreeWalker(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr,
    TclDomTreeWalker *treeWalkerPtr)
{
    TclDomNode *nextNodePtr;
    int result;

    if (treeWalkerPtr->currentNodePtr == NULL) {
	return TCL_OK;
    }

    Tcl_Preserve((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    result = TreeWalkerNextNode(treeWalkerPtr->currentNodePtr,
	    treeWalkerPtr->rootPtr, treeWalkerPtr->whatToShow,
	    treeWalkerPtr->filterPtr, &nextNodePtr);

    if (result == TCL_OK && nextNodePtr) {
	    treeWalkerPtr->currentNodePtr = nextNodePtr;
	    result = TclDomSetNodeResult(interp, interpDataPtr, nextNodePtr);
    } 

    Tcl_Release((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    return result;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomGetPreviousNodeFromTreeWalker
 *
 *	Implements the TreeWalker previousNode method.
 *
 * Results:
 *	TCL_OK. If a previous node was found, then its token
 *	is written to the interpreter's result.
 *
 * Side effects:
 *	A node token may be created.
 *
 *--------------------------------------------------------------
 */

int 
TclDomGetPreviousNodeFromTreeWalker(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr,
    TclDomTreeWalker *treeWalkerPtr)
{
    int result;
    TclDomNode *previousNodePtr = NULL;

    if (treeWalkerPtr->currentNodePtr == NULL) {
	    return TCL_OK;
    }

    Tcl_Preserve((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    result = TclDomTreeWalkerPreviousNode(treeWalkerPtr->currentNodePtr,
	    treeWalkerPtr->rootPtr, treeWalkerPtr->whatToShow,
	    treeWalkerPtr->filterPtr, &previousNodePtr);

    if (result == TCL_OK && previousNodePtr) {
	treeWalkerPtr->currentNodePtr = previousNodePtr;
	result = TclDomSetNodeResult(interp, interpDataPtr, previousNodePtr);
    }
    
    Tcl_Release((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    return result;
} 


/*
 *--------------------------------------------------------------
 *
 * TclDomGetFirstChildFromTreeWalker
 *
 *	Implements the TreeWalker firstChild method.
 *
 * Results:
 *	TCL_OK. If a child node was found, then its token
 *	is written to the interpreter's result.
 *
 * Side effects:
 *	A node token may be created.
 *
 *--------------------------------------------------------------
 */

int 
TclDomGetFirstChildFromTreeWalker(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr,
    TclDomTreeWalker *treeWalkerPtr)
{
    int result;
    TclDomNode *firstChildNodePtr = NULL;

    if (treeWalkerPtr->currentNodePtr == NULL) {
	return TCL_OK;
    }

    Tcl_Preserve((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    result = FirstChild(treeWalkerPtr->currentNodePtr,
	    treeWalkerPtr->rootPtr, treeWalkerPtr->whatToShow,
	    treeWalkerPtr->filterPtr, &firstChildNodePtr);

    if (result == TCL_OK && firstChildNodePtr) {
	treeWalkerPtr->currentNodePtr = firstChildNodePtr;
	result = TclDomSetNodeResult(interp, interpDataPtr, firstChildNodePtr);
    }
    
    Tcl_Release((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    return result;
} 


/*
 *--------------------------------------------------------------
 *
 * TclDomGetLastChildFromTreeWalker
 *
 *	Implements the TreeWalker firstChild method.
 *
 * Results:
 *	TCL_OK. If a child node was found, then its token
 *	is written to the interpreter's result.
 *
 * Side effects:
 *	A node token may be created.
 *
 *--------------------------------------------------------------
 */

int 
TclDomGetLastChildFromTreeWalker(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr,
    TclDomTreeWalker *treeWalkerPtr)
{
    int result;
    TclDomNode *lastChildNodePtr = NULL;

    if (treeWalkerPtr->currentNodePtr == NULL) {
	    return TCL_OK;
    }

    Tcl_Preserve((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    result = LastChild(treeWalkerPtr->currentNodePtr,
	    treeWalkerPtr->rootPtr, treeWalkerPtr->whatToShow,
	    treeWalkerPtr->filterPtr, &lastChildNodePtr);

    if (result == TCL_OK && lastChildNodePtr) {
	treeWalkerPtr->currentNodePtr = lastChildNodePtr;
	result = TclDomSetNodeResult(interp, interpDataPtr, lastChildNodePtr);
    }
    
    Tcl_Release((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    return result;
} 


/*
 *--------------------------------------------------------------
 *
 * TclDomGetPreviousSiblingFromTreeWalker
 *
 *	Implements the TreeWalker nextSibling method.
 *
 * Results:
 *	TCL_OK. If a child node was found, then its token
 *	is written to the interpreter's result.
 *
 * Side effects:
 *	A node token may be created.
 *
 *--------------------------------------------------------------
 */

int 
TclDomGetPreviousSiblingFromTreeWalker(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr,
    TclDomTreeWalker *treeWalkerPtr)
{
    int result;
    TclDomNode *previousSiblingNodePtr = NULL;

    if (treeWalkerPtr->currentNodePtr == NULL) {
	    return TCL_OK;
    }

    Tcl_Preserve((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    result = PreviousSibling(treeWalkerPtr->currentNodePtr,
	    treeWalkerPtr->rootPtr, treeWalkerPtr->whatToShow,
	    treeWalkerPtr->filterPtr, &previousSiblingNodePtr);

    if (result == TCL_OK && previousSiblingNodePtr) {
	treeWalkerPtr->currentNodePtr = previousSiblingNodePtr;
	result = TclDomSetNodeResult(interp, interpDataPtr,
		previousSiblingNodePtr);
    }	
    
    Tcl_Release((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    return result;
} 


/*
 *--------------------------------------------------------------
 *
 * TclDomGetNextSiblingFromTreeWalker
 *
 *	Implements the TreeWalker nextSibling method.
 *
 * Results:
 *	TCL_OK. If a child node was found, then its token
 *	is written to the interpreter's result.
 *
 * Side effects:
 *	A node token may be created.
 *
 *--------------------------------------------------------------
 */

int 
TclDomGetNextSiblingFromTreeWalker(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr,
    TclDomTreeWalker *treeWalkerPtr)
{
    int result;
    TclDomNode *nextSiblingNodePtr = NULL;

    if (treeWalkerPtr->currentNodePtr == NULL) {
	return TCL_OK;
    }

    Tcl_Preserve((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    result = NextSibling(treeWalkerPtr->currentNodePtr,
	    treeWalkerPtr->rootPtr, treeWalkerPtr->whatToShow,
	    treeWalkerPtr->filterPtr, &nextSiblingNodePtr);

    if (result == TCL_OK && nextSiblingNodePtr) {
	    treeWalkerPtr->currentNodePtr = nextSiblingNodePtr;
	    result = TclDomSetNodeResult(interp, interpDataPtr, nextSiblingNodePtr);
    }
    
    Tcl_Release((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    return result;
} 


/*
 *--------------------------------------------------------------
 *
 * TclDomGetParentNodeFromTreeWalker
 *
 *	Implements the TreeWalker parentNode method.
 *
 * Results:
 *	TCL_OK. If a child node was found, then its token
 *	is written to the interpreter's result.
 *
 * Side effects:
 *	A node token may be created.
 *
 *--------------------------------------------------------------
 */

int 
TclDomGetParentNodeFromTreeWalker(
    Tcl_Interp *interp,
    TclDomInterpData *interpDataPtr,
    TclDomTreeWalker *treeWalkerPtr)
{
    int result;
    TclDomNode *parentNodePtr = NULL;

    if (treeWalkerPtr->currentNodePtr == NULL) {
	return TCL_OK;
    }

    Tcl_Preserve((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    result = GetParent(treeWalkerPtr->currentNodePtr,
	    treeWalkerPtr->rootPtr, treeWalkerPtr->whatToShow,
	    treeWalkerPtr->filterPtr, &parentNodePtr);

    if (result == TCL_OK && parentNodePtr) {
	treeWalkerPtr->currentNodePtr = parentNodePtr;
	result = TclDomSetNodeResult(interp, interpDataPtr, parentNodePtr);
    }
    
    Tcl_Release((ClientData) treeWalkerPtr->rootPtr->containingDocumentPtr);

    return result;
} 
