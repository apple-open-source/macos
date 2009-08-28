/*
 * tcldompro.c --
 *
 *	This file implements the TclDomPro extension.
 *
 * Copyright (c) 1998-2000 Ajuba Solutions.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * $Id: tcldompro.c,v 1.15 2003/04/02 22:48:14 jenglish Exp $
 */

#include "tclDomProInt.h"

/*
 * Option definitions shared by cget and configure commands
 */

static CONST84 char *nodeOptions[] = {
    "-nodeName", "-nodeType", "-parentNode", 
    "-childNodes", "-firstChild", "-lastChild", "-previousSibling",
    "-nextSibling", "-attributes", "-nodeValue", 
    "-startLine", "-startColumn", "-endLine", "-endColumn", 
    "-startWidth", "-endWidth", "-parsingComplete", 
    "-startCloseLine", "-startCloseColumn", 
    "-endCloseLine", "-endCloseColumn", 
    "-startSpan", "-endSpan",
    NULL
};


enum nodeOptions {
    NODE_NAME, NODE_TYPE, PARENT_NODE, CHILD_NODES,
    FIRST_CHILD, LAST_CHILD, PREVIOUS_SIBLING,
    NEXT_SIBLING, ATTRIBUTES, NODE_VALUE,
    START_LINE, START_COLUMN, END_LINE, END_COLUMN,
    START_WIDTH, END_WIDTH, PARSE_COMPLETE,
    START_CLOSE_LINE, START_CLOSE_COLUMN, END_CLOSE_LINE, END_CLOSE_COLUMN,
    START_SPAN, END_SPAN
};

static CONST84 char *treeWalkerOptions[] = {
    "-show", "-filter", "-expandEntities", "-currentNode", NULL
};

enum treeWalkerOptions {
    WHAT_TO_SHOW, NODE_FILTER, EXPAND_ENTITY_REFERENCES, CURRENT_NODE
};

/*
 * W3C DOM / TclDOM incompatibilities:
 *
 * According to the W3C DOM spec, the [dom::document createXXX]
 * methods create nodes as unattached, parentless nodes.
 *
 * In the TclDOM implementation, newly created nodes are
 * added as children of the specified parent node.
 *
 * By default, dom::c now follows the dom::tcl implementation. 
 * Compile with -DW3C_CONSTRUCTOR_BEHAVIOR=1 to get the W3C behaviour,
 */

#if W3C_CONSTRUCTOR_BEHAVIOR
#  define AddCreatedNode(interp,interpDataPtr,subjectNode,child) /*no-op*/
#else
#  define  AddCreatedNode(interp,interpDataPtr,subjectNode,child) \
	if ( \
	TclDomAppendChild(interp,interpDataPtr,subjectNode,child) \
	!= TCL_OK) return TCL_ERROR;
#endif

/*
 * Forward declarations
 */

static int  DOMImplementationCmd (ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST objv[]);
static int  TclDomDoctypeCmd (ClientData clientData, Tcl_Interp *interp,
 	int objc, Tcl_Obj *CONST objv[]);
static int  TclDomNodeCmd (ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST objv[]);
static void TclDomInterpDataDeleteProc (ClientData clientData,
	Tcl_Interp *interp);
static int  TclDomElementCmd (ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST objv[]);
static int  TclDomDocumentCmd (ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST objv[]);
static int  TclDomDocumentTraversalCmd (ClientData clientData,
	Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
static int  TclDomNodeIteratorCmd (ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST objv[]);
static int  TclDomTreeWalkerCmd (ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST objv[]);
static int TclDomIteratorCGetCmd(ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST objv[]);
static int TclDomTreeWalkerCGetCmd(ClientData clientData, Tcl_Interp *interp,
	int objc, Tcl_Obj *CONST objv[]);
static int TclDomTreeWalkerConfigureCmd(ClientData clientData,
	Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);


/*
 *--------------------------------------------------------------
 *
 * Tcldom_Init --
 *
 *      Initializes the Tcl-DOM package.
 *
 * Results:
 *      Returns a standard Tcl completion code, and leaves an error
 *	    message in interp->result if an error occurs.
 *
 * Side effects:
 *      Initializes DOM.
 *
 *--------------------------------------------------------------
 */
int 
TCLDOMAPI
Tcldom_Init(
    Tcl_Interp *interp)     /* The interpreter for the extension */
{
    TclDomInterpData *interpDataPtr;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }
#endif

    /*
     * Do once-per-extension initialization
     */   

    interpDataPtr = (TclDomInterpData *) ckalloc(sizeof(TclDomInterpData));
    memset(interpDataPtr, 0, sizeof(TclDomInterpData));
	Tcl_SetAssocData(interp, PACKAGE_NAME, 
		TclDomInterpDataDeleteProc, (ClientData)interpDataPtr);

    Tcl_InitHashTable(&interpDataPtr->documentHashTable, TCL_STRING_KEYS);
    Tcl_InitHashTable(&interpDataPtr->nodeHashTable, TCL_STRING_KEYS);
    Tcl_InitHashTable(&interpDataPtr->iteratorHashTable, TCL_STRING_KEYS);
    Tcl_InitHashTable(&interpDataPtr->treeWalkerHashTable, TCL_STRING_KEYS);

    /*
     * Create additional commands.
     */

    Tcl_CreateObjCommand(interp, NAMESPACE "DOMImplementation",
	    DOMImplementationCmd, (ClientData) interpDataPtr,
	    (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, NAMESPACE "node", TclDomNodeCmd,
            (ClientData) interpDataPtr, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, NAMESPACE "element", TclDomElementCmd,
            (ClientData) interpDataPtr, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, NAMESPACE "document", TclDomDocumentCmd,
            (ClientData) interpDataPtr, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, NAMESPACE "doctype", TclDomDoctypeCmd,
            (ClientData) interpDataPtr, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, NAMESPACE "DocumentTraversal",
	    TclDomDocumentTraversalCmd, (ClientData) interpDataPtr,
	    (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, NAMESPACE "nodeIterator",
	    TclDomNodeIteratorCmd, (ClientData) interpDataPtr,
	    (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateObjCommand(interp, NAMESPACE "treeWalker", TclDomTreeWalkerCmd,
            (ClientData) interpDataPtr, (Tcl_CmdDeleteProc *) NULL);

    /*
     * Create constants for node filter return values
     */

    Tcl_ObjSetVar2(interp, Tcl_NewStringObj("::dom::accept", -1), NULL, 
	    Tcl_NewIntObj(DOM_ACCEPT), 0);

    Tcl_ObjSetVar2(interp, Tcl_NewStringObj("::dom::skip", -1), NULL, 
	    Tcl_NewIntObj(DOM_SKIP), 0);

    Tcl_ObjSetVar2(interp, Tcl_NewStringObj("::dom::reject", -1), NULL, 
	    Tcl_NewIntObj(DOM_REJECT), 0);

    Tcl_PkgProvide(interp, "tcldom", VERSION);
    Tcl_PkgProvide(interp, "dom::c", VERSION);
    Tcl_PkgProvide(interp, "dom",    VERSION);
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * Tcldom_SafeInit --
 *
 *      Initializes the tcldompro package for a safe interpreter.
 *
 * Results:
 *      Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */

int
TCLDOMAPI
Tcldom_SafeInit(
    Tcl_Interp *interp)     /* Safe Interpreter for the extension */
{
    /*
     * This package does not provide any unsafe capabilities, so we
     * just call the regular initializer.
     */

    return Tcldom_Init(interp);
}


/*
 *--------------------------------------------------------------
 *
 * TclDomInterpDataDeleteProc --
 *
 *      This procedure is called when the extension's 
 *      interpreter is deleted.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Frees resources.
 *
 *--------------------------------------------------------------
 */

static void
TclDomInterpDataDeleteProc(
    ClientData clientData,  /* Per interpreter data */
    Tcl_Interp *interp)     /* Interpreter being deleted */
{
    TclDomInterpData *interpDataPtr = (TclDomInterpData *) clientData;
    if (interpDataPtr) {
        Tcl_HashEntry *entry;
       	Tcl_HashSearch search;
        TclDomDocument *documentPtr;
        TclDomNodeIterator *nodeIteratorPtr;
        TclDomTreeWalker *treeWalkerPtr;

        /*
         * Delete any dangling objects
         */ 
        
        if (interpDataPtr->parserInfo.documentPtr) {
            TclDomDeleteDocument(interp, interpDataPtr,
		    interpDataPtr->parserInfo.documentPtr);
        }
        
        for (entry = Tcl_FirstHashEntry(&interpDataPtr->documentHashTable,
		        &search); entry; entry = Tcl_NextHashEntry(&search)) {
            documentPtr = (TclDomDocument *) Tcl_GetHashValue(entry);
            TclDomDeleteDocument(interp, interpDataPtr, documentPtr);
	}

        for (entry = Tcl_FirstHashEntry(&interpDataPtr->iteratorHashTable,
		        &search); entry; entry = Tcl_NextHashEntry(&search)) {
            nodeIteratorPtr = (TclDomNodeIterator *) Tcl_GetHashValue(entry);
            TclDomDeleteNodeIterator(nodeIteratorPtr);
	}

        for (entry = Tcl_FirstHashEntry(&interpDataPtr->treeWalkerHashTable,
		    &search); entry; entry = Tcl_NextHashEntry(&search)) {
            treeWalkerPtr = (TclDomTreeWalker *) Tcl_GetHashValue(entry);
            TclDomDeleteTreeWalker(treeWalkerPtr);
	}

        Tcl_DeleteHashTable(&interpDataPtr->documentHashTable);
        Tcl_DeleteHashTable(&interpDataPtr->nodeHashTable);
        Tcl_DeleteHashTable(&interpDataPtr->iteratorHashTable);
        Tcl_DeleteHashTable(&interpDataPtr->treeWalkerHashTable);


        if (interpDataPtr->parser) {
            XML_ParserFree(interpDataPtr->parser);
        }

        ckfree((char *) interpDataPtr);
    }  
}


/*
 *--------------------------------------------------------------
 *
 * TclDomCGetNodeCmd --
 *
 *      This procedure handles the cget method for a 
 *      TclDom node command.
 *
 * Results:
 *      Return TCL_OK if a valid attribute is requested;
 *      otherwise return TCL_ERROR.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */

static int
TclDomCGetNodeCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int optionIndex;
    TclDomInterpData *interpDataPtr;
    TclDomNode *nodePtr;

    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "node option");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], nodeOptions, "option", 0, 
	        &optionIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[2]);
    if (nodePtr == NULL) {
        return TCL_ERROR;
    }

    switch ((enum nodeOptions) optionIndex) {
        case NODE_NAME:
            return TclDomGetNodeName(interp, nodePtr);

        case NODE_TYPE:
            return TclDomNodeTypeName(interp, nodePtr);

        case PARENT_NODE:
            return TclDomSetNodeResult(interp, interpDataPtr,
		            nodePtr->parentNodePtr);

        case CHILD_NODES:
            return TclDomGetChildNodeList(interp, interpDataPtr, nodePtr);

        case FIRST_CHILD:
            if (TclDomHasChildren(nodePtr)) {
                return TclDomSetNodeResult(interp, interpDataPtr,
			    nodePtr->firstChildPtr);
            } else {
                return TCL_OK;
            }

        case LAST_CHILD:
            if (TclDomHasChildren(nodePtr)) {
                return TclDomSetNodeResult(interp, interpDataPtr,
			            nodePtr->lastChildPtr);
            } else {
                return TCL_OK;
            }

        case PREVIOUS_SIBLING:
	    return TclDomSetNodeResult(interp, interpDataPtr,
			    nodePtr->previousSiblingPtr);

        case NEXT_SIBLING:
	    return TclDomSetNodeResult(interp, interpDataPtr,
			    nodePtr->nextSiblingPtr);

        case ATTRIBUTES:
            return TclDomAttributeArray(interp, interpDataPtr, nodePtr);

        case NODE_VALUE:
            if (nodePtr->nodeValue) {
                Tcl_SetObjResult(interp,
			            Tcl_NewStringObj(nodePtr->nodeValue, -1));
            }
            return TCL_OK;

        case START_LINE:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(nodePtr->startLine));
            return TCL_OK;
        
        case START_COLUMN:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(nodePtr->startColumn));
            return TCL_OK;
        
        case END_LINE:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(nodePtr->endLine));
            return TCL_OK;
        
        case END_COLUMN:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(nodePtr->endColumn));
            return TCL_OK;
           
        case START_WIDTH:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(nodePtr->startWidth));
            return TCL_OK;
               
        case END_WIDTH:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(nodePtr->endWidth));
            return TCL_OK;

	case START_CLOSE_LINE:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(nodePtr->startLineClose));
            return TCL_OK;
        
        case START_CLOSE_COLUMN:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(nodePtr->startColumnClose));
            return TCL_OK;

	case END_CLOSE_LINE:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(nodePtr->endLineClose));
            return TCL_OK;
        
        case END_CLOSE_COLUMN:
            Tcl_SetObjResult(interp, Tcl_NewIntObj(nodePtr->endColumnClose));
            return TCL_OK;

	case START_SPAN: {
	    Tcl_Obj *spanPtr;
	    spanPtr = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(interp, spanPtr, Tcl_NewIntObj(nodePtr->startLine));
	    Tcl_ListObjAppendElement(interp, spanPtr, Tcl_NewIntObj(nodePtr->startColumn));
	    Tcl_ListObjAppendElement(interp, spanPtr, Tcl_NewIntObj(nodePtr->startLineClose));
	    Tcl_ListObjAppendElement(interp, spanPtr, Tcl_NewIntObj(nodePtr->startColumnClose));
	    Tcl_SetObjResult(interp, spanPtr);
	    return TCL_OK;
	}

	case END_SPAN: {
	    Tcl_Obj *spanPtr;
	    spanPtr = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(interp, spanPtr, Tcl_NewIntObj(nodePtr->endLine));
	    Tcl_ListObjAppendElement(interp, spanPtr, Tcl_NewIntObj(nodePtr->endColumn));
	    Tcl_ListObjAppendElement(interp, spanPtr, Tcl_NewIntObj(nodePtr->endLineClose));
	    Tcl_ListObjAppendElement(interp, spanPtr, Tcl_NewIntObj(nodePtr->endColumnClose));
	    Tcl_SetObjResult(interp, spanPtr);
	    return TCL_OK;
	}

        case PARSE_COMPLETE:
            Tcl_SetObjResult(interp, Tcl_NewBooleanObj(nodePtr->nodeComplete));
            return TCL_OK;

        default:
            Tcl_SetResult(interp, "unknown option", TCL_STATIC);
            return TCL_ERROR;
    }
    /*NOTREACHED*/
}


/*
 *--------------------------------------------------------------
 *
 * TclDomConfigureNodeCmd --
 *
 *      This procedure handles the configure method for a 
 *      TclDom node command.
 *
 * Results:
 *      Return TCL_OK if a valid attribute is requested;
 *      otherwise return TCL_ERROR.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */

static int
TclDomConfigureNodeCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int optionIndex;
    char *value;
    TclDomInterpData *interpDataPtr;
    TclDomNode *nodePtr;

    static CONST84 char *writableNodeOptions[] = {
        "-nodeValue",
        NULL
    };

    enum writableNodeOptions {
        NODE_VALUE,
    };

    if (objc != 5) {
        return TclDomCGetNodeCmd(clientData, interp, objc, objv);
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], nodeOptions, "option", 0, 
	        &optionIndex) != TCL_OK) {
        return TCL_ERROR;
    }
    
    if (Tcl_GetIndexFromObj(interp, objv[3], writableNodeOptions,
	        "writable option", 0, &optionIndex) != TCL_OK) {
        Tcl_SetResult(interp, NO_MODIFICATION_ALLOWED_ERR_TEXT, TCL_STATIC);
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[2]);
    if (nodePtr == NULL) {
        return TCL_ERROR;
    }

    value = Tcl_GetStringFromObj(objv[4], NULL);

    switch ((enum nodeOptions) optionIndex) {        
        case NODE_VALUE: {
			TdpDomError status;
            status = TclDomSetNodeValue(nodePtr, value);
			if (status != TDP_OK) {
				TclDomSetDomError(interp, status);
				return TCL_ERROR;
			}
            break;
	    }

        default:
            Tcl_SetResult(interp, "unknown option", TCL_STATIC);
            return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomDestroy --
 *
 *      This procedure handles the destroy method for a 
 *      TclDom implementation.
 *
 * Results:
 *      Return TCL_OK if the request is for a valid document
 *      node; otherwise return TCL_ERROR:
 *
 * Side effects:
 *      Deletes a DOM node.
 *
 *--------------------------------------------------------------
 */

static int
TclDomDestroy(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    TclDomInterpData *interpDataPtr;
    TclDomNode *nodePtr;
    char *temp;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "token");
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[2]);

    if (nodePtr == NULL) {
        return TCL_ERROR;
    } /* else */

    /* If this is a DOCUMENT node, delete the whole document:
     */

    if (nodePtr->nodeType == DOCUMENT_NODE) {
	TclDomDeleteDocument(interp, interpDataPtr, 
		nodePtr->containingDocumentPtr);
    } else {
    	/* @@ Just unlink the child from the tree, don't actually destroy it;
	 * It will be destroyed when the document itself is.
	 */
	if (nodePtr->parentNodePtr) {
	    TclDomRemoveChild(interp, interpDataPtr,
		    nodePtr->parentNodePtr, nodePtr);
    	}
    }
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomDestroyTraversalObject --
 *
 *      This procedure handles the destroy method for a 
 *      TclDom Traversal implementation.
 *
 * Results:
 *      Return TCL_OK if the request is for a valid document
 *      node; otherwise return TCL_ERROR:
 *
 * Side effects:
 *      Deletes a DOM TreeWalker or NodeIterator.
 *
 *--------------------------------------------------------------
 */

static int
TclDomDestroyTraversalObject(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    TclDomInterpData *interpDataPtr;
    TclDomTreeWalker *treeWalkerPtr;
    TclDomNodeIterator *nodeIteratorPtr;
    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "token");
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    treeWalkerPtr = TclDomGetTreeWalkerFromToken(interp, interpDataPtr,
	        objv[2]);
    if (treeWalkerPtr == NULL) {
        Tcl_ResetResult(interp);
        nodeIteratorPtr = TclDomGetNodeIteratorFromToken(interp, 
				interpDataPtr, objv[2]);
        if (nodeIteratorPtr == NULL) {
            return TCL_ERROR;
        }
        TclDomDeleteNodeIterator(nodeIteratorPtr);
    } else {
        TclDomDeleteTreeWalker(treeWalkerPtr);
    }
    return TCL_OK;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomParse --
 *
 *      This procedure handles the parse method for a 
 *      TclDom implementation.
 *
 * Results:
 *      Return TCL_OK if the input XML source is successfully
 *      parsed; otherwise returns TCL_ERROR.
 *
 * Side effects:
 *      Invokes the expat parser to create a parse tree for the
 *      input XML. Creates a handle for the document.
 *
 *--------------------------------------------------------------
 */

static int TclDomParse(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    char *xmlSource;
    char *progressCmd;
    long chunkSize;
    TclDomInterpData *interpDataPtr;
    int length, index, i;
    int final = 1;
    int trim = 0;
    int validate = 0;

    static CONST84 char *options[] = {
	    "-chunksize", "-final", "-parser", "-progressCmd", "-trim",
        "-validate", NULL
    };
    enum options {
	    OPT_CHUNKSIZE, OPT_FINAL, OPT_PARSER, OPT_PROGRESSCMD, OPT_TRIM,
            OPT_VALIDATE
    };

    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "data ?options?");
        return TCL_ERROR;
    }

    /*
     * Check for matched option / value pairs
     */

    for (i = 3; i < objc; i++) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", 0, 
		        &index) != TCL_OK) {
	        return TCL_ERROR;
	    }
	    if ((index != OPT_TRIM) && (i == objc-1)) {
	        Tcl_AppendResult(interp, "missing option value", (char *) NULL);
	        return TCL_ERROR;
	    }

	    switch (index) {
	        case OPT_PARSER:
		        i++;
		        if (strcmp("expat", Tcl_GetStringFromObj(objv[i], NULL))
			            != 0) {
		            Tcl_AppendResult(interp, "parser must be expat",
			                (char *) NULL);
		            return TCL_ERROR;
		        }
		        break;

	        case OPT_PROGRESSCMD:
		        i++;
		        progressCmd = Tcl_GetStringFromObj(objv[i], NULL);
		        break;

	        case OPT_CHUNKSIZE:
		        i++;
		        if (Tcl_GetLongFromObj(interp, objv[i], &chunkSize)
			            != TCL_OK) {
		            return TCL_ERROR;
		        }
		        break;

	        case OPT_FINAL:
		        i++;
		        if (Tcl_GetBooleanFromObj(interp, objv[i], &final) != TCL_OK) {
		            return TCL_ERROR;
		        }
		        break;

	        case OPT_TRIM:
		        trim = 1;
		        break;

            case OPT_VALIDATE:
                i++;
		        if (Tcl_GetBooleanFromObj(interp, objv[i], &validate) != TCL_OK) {
		            return TCL_ERROR;
		        }
		        break;
	    }
    } 

    interpDataPtr = (TclDomInterpData *) clientData;

    xmlSource = Tcl_GetStringFromObj(objv[2], &length);

    return TclDomReadDocument(interp, interpDataPtr, xmlSource, length, final,
	        trim);
}


/*
 *--------------------------------------------------------------
 *
 * TclDomImplemenationCmd --
 *
 *      This procedure handles the DOMImplementation command.
 *      Refer to the user documenation for details.
 *
 * Results:
 *      Return TCL_OK if a method succeeded; TCL_ERROR
 *      otherwise.
 *
 * Side effects:
 *      Depends on the command methods.
 *
 *--------------------------------------------------------------
 */

int
DOMImplementationCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int methodIndex;
    char *requestedVersion;
    TclDomInterpData *interpDataPtr;
    TclDomNode *nodePtr;

    static CONST84 char *methods[] = {
	    "create", "hasFeature", "destroy", 
	    "parse", "serialize", "trim", NULL
    };

    enum methods {
        CREATE, HASFEATURE, DESTROY, PARSE, SERIALIZE, TRIM
    };

    if (objc < 2) {
	    Tcl_WrongNumArgs(interp, 1, objv, "method ?arg arg ...?");
	    return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], methods, "method", 0, 
	        &methodIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    switch ((enum methods) methodIndex) {
        case CREATE:
            if (objc == 3) {
                /*
                 * Make a special check for an optional "arrayName"
                 * which some versions of tcldom may support, but which
                 * we do not support. See the tcldom documentation for further
                 * explanation.
                 */

                Tcl_AppendResult(interp,
			            "the 'arrayName' option is not supported", 
						(char *) NULL);
                return TCL_ERROR;
            }
            if (objc != 2) {
                Tcl_WrongNumArgs(interp, 2, objv, NULL);
                return TCL_ERROR;
            }
            return TclDomCreateEmptyDocumentNode(interp, interpDataPtr);

        case HASFEATURE:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "feature");
                return TCL_ERROR;
            }
            /*
             * Check if the feature is valid
             */

            if (Tcl_GetIndexFromObj(interp, objv[2], methods, "", 0, 
		            &methodIndex) != TCL_OK) {
                Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
                return TCL_OK;
            }
            requestedVersion = Tcl_GetStringFromObj(objv[3], NULL);
            if (strcmp(requestedVersion, "1.0") == 0) {
                Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
            } else {
                Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
            }
            return TCL_OK;

        case DESTROY:
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "token");
                return TCL_ERROR;
            }
            return TclDomDestroy(clientData, interp, objc, objv);

        case PARSE:
            return TclDomParse(clientData, interp, objc, objv);

        case SERIALIZE:
            if (objc < 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "token ?options?");
                return TCL_ERROR;
            }
            nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[2]);
            if (nodePtr == NULL) {
                return TCL_ERROR;
            }
            if (objc >= 4) {
                char *option = Tcl_GetStringFromObj(objv[3], NULL);
                if (strcmp(option, "-newline") == 0) {
                    /*
                     * We don't support this as yet; the intent is to provide 
		     * a list of element tags for which newlines will be 
		     * appended.
                     */
                } else {
                    Tcl_AppendResult(interp, "invalid option ", option,
			                (char *) NULL);
                    return TCL_ERROR;
                }
            }

            return TclDomSerialize(interp, nodePtr);

        case TRIM:
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "token");
                return TCL_ERROR;
            }
            Tcl_AppendResult(interp, "trim method not implemented",
		            (char *) NULL);
            return TCL_ERROR;

        default:
            Tcl_SetResult(interp, "unknown method", TCL_STATIC);
            return TCL_ERROR;
    }
    
    /*NOTREACHED*/
}


/*
 *--------------------------------------------------------------
 *
 * TclDomNodeCmd --
 *
 *      This procedure processes commands for DOM Node objects.
 *      Refer to the user documentation to see what it does.
 *
 * Results:
 *      Return TCL_OK if a method succeeded; TCL_ERROR
 *      otherwise.
 *
 * Side effects:
 *      Depends on the command methods.
 *
 *--------------------------------------------------------------
 */

static int
TclDomNodeCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int methodIndex;
    TclDomInterpData *interpDataPtr;
    TclDomNode *nodePtr;
    TclDomNode *childPtr, *refChildPtr, *newChildPtr, *oldChildPtr;
    int hasChildren, deepFlag;

    static CONST84 char *methods[] = {
	    "cget", "configure", "insertBefore", "replaceChild", 
	    "removeChild", "appendChild", "hasChildNodes", "cloneNode",
	    "children", "parent", NULL
    };

    enum methods {
        CGET, CONFIGURE, INSERT_BEFORE, REPLACE_CHILD, REMOVE_CHILD,
        APPEND_CHILD, HAS_CHILD_NODES, CLONE_NODE, CHILDREN, PARENT
    };

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "method token ?arg ...?");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], methods, "method", 0, 
	        &methodIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[2]);
    if (nodePtr == NULL) {
        return TCL_ERROR;
    }

    switch ((enum methods) methodIndex) {
        case CGET:
            return TclDomCGetNodeCmd(clientData, interp, objc, objv);

        case CONFIGURE:
            return TclDomConfigureNodeCmd(clientData, interp, objc, objv);

        case INSERT_BEFORE:
            if (objc < 4 || objc > 5) {
                Tcl_WrongNumArgs(interp, 2, objv,
			            "token newchild ?refchild?");
                return TCL_ERROR;
            }
            childPtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[3]);
            if (childPtr == NULL) {
                return TCL_ERROR;
            }
            if (TclDomValidateChildType(interp, nodePtr, childPtr) != TCL_OK) {
                return TCL_ERROR;
            }
            if (objc == 5) {
                refChildPtr = TclDomGetNodeFromToken(interp, interpDataPtr,
			            objv[4]);
                if (refChildPtr == NULL) {
                    return TCL_ERROR;
                }
                if (TclDomValidateChildType(interp, nodePtr, refChildPtr)
			            != TCL_OK) {
                    return TCL_ERROR;
                }
            } else {
                refChildPtr = NULL;
            }

            if (refChildPtr) {           
                return TclDomInsertBefore(interp, interpDataPtr, nodePtr,
			            childPtr, refChildPtr);
            } else {
                return TclDomAppendChild(interp, interpDataPtr, nodePtr,
			        childPtr);
            }

        case REPLACE_CHILD:
            if (objc != 5) {
                Tcl_WrongNumArgs(interp, 2, objv, "token newchild oldchild");
                return TCL_ERROR;
            }
            newChildPtr = TclDomGetNodeFromToken(interp, interpDataPtr,
		    objv[3]);
            if (newChildPtr == NULL) {
                return TCL_ERROR;
            }
            if (TclDomValidateChildType(interp, nodePtr, newChildPtr)
		            != TCL_OK) {
                return TCL_ERROR;
            }
            oldChildPtr = TclDomGetNodeFromToken(interp, interpDataPtr,
		            objv[4]);
            if (oldChildPtr == NULL) {
                return TCL_ERROR;
            }
            return TclDomReplaceChild(interp, interpDataPtr, nodePtr,
		            newChildPtr, oldChildPtr);

        case REMOVE_CHILD:
	        if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token oldchild");
                return TCL_ERROR;
            }
            childPtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[3]);
            if (childPtr == NULL) {
                return TCL_ERROR;
            }
            return TclDomRemoveChild(interp, interpDataPtr, nodePtr, childPtr);

        case APPEND_CHILD:
	    if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token newchild");
                return TCL_ERROR;
            }

            childPtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[3]);
            if (childPtr == NULL) {
                return TCL_ERROR;
            }
            if (TclDomValidateChildType(interp, nodePtr, childPtr) != TCL_OK) {
                return TCL_ERROR;
            }
            return TclDomAppendChild(interp, interpDataPtr, nodePtr, childPtr);

        case HAS_CHILD_NODES:
            nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[2]);
            if (nodePtr == NULL) {
                return TCL_ERROR;
            }
            hasChildren = ((nodePtr->nodeType == ELEMENT_NODE
		            || nodePtr->nodeType == DOCUMENT_NODE) 
                    && (nodePtr->firstChildPtr != NULL));
            Tcl_SetObjResult(interp, Tcl_NewBooleanObj(hasChildren));
            return TCL_OK;

        case CLONE_NODE:
            if (objc != 3 && objc != 5) {
                Tcl_WrongNumArgs(interp, 2, objv,
			            "token ?-deep deepFlag?");
                return TCL_ERROR;
            }
            if (objc == 5) {
                char *option;
                int result;
                option = Tcl_GetStringFromObj(objv[3], NULL);
                if (strcmp(option, "-deep")) {
                    Tcl_AppendResult(interp,
			                "invalid option, should be \"-deep\"");
                    return TCL_ERROR;
                }
                result = Tcl_GetBooleanFromObj(interp, objv[4], &deepFlag);
                if (result != TCL_OK) {
                    return result;
                }
            }
            return TclDomCloneNode(interp, interpDataPtr, nodePtr, 
                deepFlag);

        case CHILDREN:
	{
	    Tcl_Obj *result;
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "token");
                return TCL_ERROR;
            }
	    result = TclDomGetChildren(interp, interpDataPtr, nodePtr);
	    if (result == NULL)
	    	return TCL_ERROR;
	    Tcl_SetObjResult(interp, result);
	    return TCL_OK;
	}
        case PARENT:
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "token");
                return TCL_ERROR;
            }
            return TclDomSetNodeResult(interp, interpDataPtr,
		            nodePtr->parentNodePtr);

        default:
            Tcl_SetResult(interp, "unknown method", TCL_STATIC);
            return TCL_ERROR;
    }
    
    /*NOTREACHED*/
}


/*
 *--------------------------------------------------------------
 *
 * TclDomElementCmd --
 *
 *      This procedure processes commands for DOM Element objects.
 *      Refer to the user documentation to see what it does.
 *
 * Results:
 *      Return TCL_OK if a method succeeded; TCL_ERROR
 *      otherwise.
 *
 * Side effects:
 *      Depends on the command methods.
 *
 *--------------------------------------------------------------
 */

static int
TclDomElementCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int methodIndex;
    TclDomInterpData *interpDataPtr;
    TclDomNode *nodePtr;
    TclDomAttributeNode *attributeNodePtr;
    char *option, *attributeName, *tagname, *name, *value;

    static CONST84 char *methods[] = {
	    "cget", "configure", "getAttribute", "setAttribute", 
	    "removeAttribute", "getAttributeNode", "setAttributeNode",
	    "removeAttributeNode", "getElementsByTagName", "normalize", NULL
    };

    enum methods {
        CGET, CONFIGURE, 
        GET_ATTRIBUTE, SET_ATTRIBUTE, REMOVE_ATTRIBUTE,
        GET_ATTRIBUTE_NODE, SET_ATTRIBUTE_NODE, REMOVE_ATTRIBUTE_NODE, 
        GET_ELEMENTS_BY_TAGNAME, NORMALIZE
    };

    if (Tcl_GetIndexFromObj(interp, objv[1], methods, "method", 0, 
	        &methodIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[2]);
    if (nodePtr == NULL) {
        return TCL_ERROR;
    }

    if (nodePtr->nodeType != ELEMENT_NODE) {
        Tcl_AppendResult(interp, "not an element type node", (char *) NULL);
        return TCL_ERROR;
    }

    switch ((enum methods) methodIndex) {
        case CGET:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token option");
                return TCL_ERROR;
            }
            option = Tcl_GetStringFromObj(objv[3], NULL);
            if (strcmp(option, "-tagName") == 0) {
                if (nodePtr->nodeName) {
                    Tcl_SetObjResult(interp,
			                Tcl_NewStringObj(nodePtr->nodeName, -1));
                }
                return TCL_OK;
            } else if (strcmp(option, "-empty") == 0) {
                Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
                return TCL_OK;
            } else {
                Tcl_AppendResult(interp, "unknown option '", option,
			            "', should be -empty or -tagName", (char *) NULL);
                return TCL_ERROR;
            }

        case CONFIGURE:
            Tcl_AppendResult(interp,
		            "element configure method not implemented", (char *) NULL);
            return TCL_ERROR;

        case GET_ATTRIBUTE:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token name");
                return TCL_ERROR;
            }
            attributeName = Tcl_GetStringFromObj(objv[3], NULL);
            attributeNodePtr = nodePtr->firstAttributePtr;
            while (attributeNodePtr && strcmp(attributeName,
		            attributeNodePtr->nodeName)) {
                attributeNodePtr = attributeNodePtr->nextSiblingPtr;
            }

            if (attributeNodePtr) {
                Tcl_SetObjResult(interp,
			            Tcl_NewStringObj(attributeNodePtr->nodeValue, -1));
                return TCL_OK;
            } else {
                return TCL_OK;
            }

        case SET_ATTRIBUTE:
            if (objc != 5) {
                Tcl_WrongNumArgs(interp, 2, objv, "token name value");
                return TCL_ERROR;
            }
            name = Tcl_GetStringFromObj(objv[3], NULL);
            if (!TclDomIsName(name)) {
                Tcl_AppendResult(interp, INVALID_CHARACTER_ERR_TEXT,
			            (char *) NULL);
                return TCL_ERROR;
            }
            value = Tcl_GetStringFromObj(objv[4], NULL);
            return TclDomSetAttribute(interp, interpDataPtr, nodePtr, name,
		            value);

        case REMOVE_ATTRIBUTE:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token name");
                return TCL_ERROR;
            }
            name = Tcl_GetStringFromObj(objv[3], NULL);
            return TclDomRemoveAttribute(interp, interpDataPtr, nodePtr, name);

        case GET_ATTRIBUTE_NODE:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token name");
                return TCL_ERROR;
            }
            Tcl_AppendResult(interp, "getAttribute method not implemented",
		            (char *) NULL);
            return TCL_ERROR;

        case SET_ATTRIBUTE_NODE:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token name");
                return TCL_ERROR;
            }
            Tcl_AppendResult(interp, "setAttribute method not implemented",
		            (char *) NULL);
            return TCL_ERROR;

        case REMOVE_ATTRIBUTE_NODE:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token name");
                return TCL_ERROR;
            }
            return TCL_ERROR;

        case GET_ELEMENTS_BY_TAGNAME:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token name");
                return TCL_ERROR;
            }
            tagname = Tcl_GetStringFromObj(objv[3], NULL);
            return TclDomGetElementsByTagname(interp, interpDataPtr, tagname,
		    nodePtr);

        case NORMALIZE:
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "token");
                return TCL_ERROR;
            }
            Tcl_AppendResult(interp, "normalize method not implemented",
		            (char *) NULL);
            return TCL_ERROR;

        default:
            Tcl_SetResult(interp, "unknown method", TCL_STATIC);
            return TCL_ERROR;
    }
    
    /*NOTREACHED*/
}


/*
 *--------------------------------------------------------------
 *
 * TclDomDoctypeCmd --
 *
 *      This procedure processes commands for DOM Doctype objects.
 *      Refer to the user documentation to see what it does.
 *
 * Results:
 *      Return TCL_OK if a method succeeded; TCL_ERROR
 *      otherwise.
 *
 * Side effects:
 *      Depends on the command methods.
 *
 *--------------------------------------------------------------
 */

static int
TclDomDoctypeCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int methodIndex;
    TclDomInterpData *interpDataPtr;
    TclDomNode *nodePtr;
    TclDomDocTypeNode *docTypePtr;
    char *option;

    static CONST84 char *methods[] = {
	"cget", "configure", NULL
    };

    enum methods {
        CGET, CONFIGURE
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv,
		"method ?args...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], methods, "method", 0, 
	        &methodIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[2]);
    if (nodePtr == NULL) {
        return TCL_ERROR;
    }

    if (nodePtr->nodeType != DOCUMENT_TYPE_NODE) {
        Tcl_AppendResult(interp, "not a doctype type node", (char *) NULL);
        return TCL_ERROR;
    }
    docTypePtr = (TclDomDocTypeNode*) nodePtr;

    switch ((enum methods) methodIndex) {
        case CGET:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token option");
                return TCL_ERROR;
            }
            option = Tcl_GetStringFromObj(objv[3], NULL);
            if (strcmp(option, "-systemId") == 0) {
                if (docTypePtr->systemId) {
                    Tcl_SetObjResult(interp,
			    Tcl_NewStringObj(docTypePtr->systemId, -1));
                }
                return TCL_OK;
            } else if (strcmp(option, "-nodeName") == 0) {
                if (docTypePtr->nodeName) {
                    Tcl_SetObjResult(interp,
			    Tcl_NewStringObj(docTypePtr->nodeName, -1));
                }
                return TCL_OK;
            } else if (strcmp(option, "-publicId") == 0) {
                if (docTypePtr->publicId) {
                    Tcl_SetObjResult(interp,
			    Tcl_NewStringObj(docTypePtr->publicId, -1));
                }
                return TCL_OK;
            } else if (strcmp(option, "-internalSubset") == 0) {
                if (docTypePtr->internalSubset) {
                    Tcl_SetObjResult(interp,
			    Tcl_NewStringObj(docTypePtr->internalSubset, -1));
                }
                return TCL_OK;
            } else {
                Tcl_AppendResult(interp, "unknown option '", option,
			            "', should be -internalSubset, -nodeName, -publicId, or -systemId", (char *) NULL);
                return TCL_ERROR;
            }

        case CONFIGURE:
            Tcl_AppendResult(interp,
		            "doctype configure method not implemented", (char *) NULL);
            return TCL_ERROR;
        default:
            Tcl_SetResult(interp, "unknown method", TCL_STATIC);
            return TCL_ERROR;
    }
    
    /*NOTREACHED*/
}


/*
 *--------------------------------------------------------------
 *
 * TclDomDocumentCmd --
 *
 *      This procedure processes commands for DOM Document objects.
 *      Refer to the user documentation to see what it does.
 *
 * Results:
 *      Return TCL_OK if a method succeeded; TCL_ERROR
 *      otherwise.
 *
 * Side effects:
 *      Depends on the command methods.
 *
 *--------------------------------------------------------------
 */

static int
TclDomDocumentCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int methodIndex, attributeIndex;
    TclDomInterpData *interpDataPtr;
    TclDomDocument *documentPtr;
    TclDomNode *subjectNodePtr;
    TclDomNode *importNodePtr;
    int deepFlag = 0;   /* for import node */
    TclDomNode *nodePtr;
    TclDomTextNode *textNodePtr;
    char *tagName, *text, *target;

    static CONST84 char *methods[] = {
	    "cget", "configure", 
	    "createElement", "createDocumentFragment", "createTextNode", 
	    "createComment", "createCDATASection", "createProcessingInstruction",
	    "createAttribute", "createEntity", "createEntityReference",
	    "createDocTypeDecl", "getElementsByTagName",
        "importNode", NULL
    };

    enum methods {
        CGET, CONFIGURE, 
        CREATE_ELEMENT, CREATE_DOCUMENT_FRAGMENT, CREATE_TEXT_NODE,
        CREATE_COMMENT, CREATE_CDATA_SECTION, CREATE_PROCESSING_INSTRUCTION, 
        CREATE_ATTRIBUTE, CREATE_ENTITY, CREATE_ENTITY_REFERENCE,
	    CREATE_DOCTYPE_DECL, GET_ELEMENTS_BY_TAGNAME,
        IMPORT_NODE
    };

    static CONST84 char *attributes[] = {
        "-doctype", "-implementation", "-documentElement", NULL
    };

    enum attributes {
        DOCTYPE, IMPLEMENTATION, DOCUMENT_ELEMENT
    };

    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "method token ...");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], methods, "method", 0, 
	        &methodIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    subjectNodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[2]);
    if (subjectNodePtr == NULL) {
        return TCL_ERROR;
    }
    documentPtr = subjectNodePtr->containingDocumentPtr;

    switch ((enum methods) methodIndex) {
        case CONFIGURE:
        case CGET:
            if (Tcl_GetIndexFromObj(interp, objv[3], attributes, "attribute",
		            0, &attributeIndex) != TCL_OK) {
                return TCL_ERROR;
            }
            if (methodIndex == CONFIGURE) {
                if (objc < 4 || objc > 5) {
                    Tcl_WrongNumArgs(interp, 2, objv,
			                "token option ?value?");
                    return TCL_ERROR;
                }
                if (objc == 5) {
                    char *optionName = Tcl_GetStringFromObj(objv[3], NULL);
                    Tcl_AppendResult(interp, "attribute \"", optionName,
			                "\" is read-only", (char *) NULL);
                    return TCL_ERROR;
                }
            } else {
                if (objc != 4) {
                    Tcl_WrongNumArgs(interp, 2, objv, "token option");
                    return TCL_ERROR;
                }
            }
            if (attributeIndex == DOCTYPE) {
		return TclDomSetNodeResult(interp, interpDataPtr,
			    TclDomGetDoctypeNode(documentPtr));
            } else if (attributeIndex == IMPLEMENTATION) {
                Tcl_SetObjResult(interp,
			            Tcl_NewStringObj("::dom::DOMImplementation", -1));
                return TCL_OK;
            } else if (attributeIndex == DOCUMENT_ELEMENT) {
                return TclDomSetNodeResult(interp, interpDataPtr,
				    TclDomGetDocumentElement(documentPtr));
            } else {
                Tcl_AppendResult(interp, "unknown option", (char *) NULL);
                return TCL_ERROR;
            }

        case CREATE_ELEMENT:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token type");
                return TCL_ERROR;
            }
            tagName = Tcl_GetStringFromObj(objv[3], NULL);
            if (!TclDomIsName(tagName)) {
                Tcl_AppendResult(interp, INVALID_CHARACTER_ERR_TEXT,
			            (char *) NULL);
                return TCL_ERROR;
            }
            nodePtr = TclDomCreateElement(interp, interpDataPtr, documentPtr,
		                tagName);
	    AddCreatedNode(interp, interpDataPtr, subjectNodePtr, nodePtr);
            return TclDomSetNodeResult(interp, interpDataPtr, nodePtr);

        case CREATE_DOCUMENT_FRAGMENT:
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "token");
                return TCL_ERROR;
            }
            nodePtr = TclDomCreateDocumentFragment(interp, interpDataPtr,
		            documentPtr);
            return TclDomSetNodeResult(interp, interpDataPtr, nodePtr);
            
        case CREATE_TEXT_NODE:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token text");
                return TCL_ERROR;
            }
            text = Tcl_GetStringFromObj(objv[3], NULL);
            textNodePtr = TclDomCreateTextNode(interp, interpDataPtr,
		            documentPtr, text);
	    AddCreatedNode(interp, interpDataPtr, subjectNodePtr, 
	    	(TclDomNode*)textNodePtr);
            return TclDomSetNodeResult(interp, interpDataPtr,
		            (TclDomNode *) textNodePtr);

        case CREATE_COMMENT:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token data");
                return TCL_ERROR;
            }
            text = Tcl_GetStringFromObj(objv[3], NULL);
            nodePtr = TclDomCreateCommentNode(interp, interpDataPtr,
		            documentPtr, text);
	    AddCreatedNode(interp, interpDataPtr, subjectNodePtr, nodePtr);
            return TclDomSetNodeResult(interp, interpDataPtr, nodePtr);

        case CREATE_CDATA_SECTION:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token text");
                return TCL_ERROR;
            }
            text = Tcl_GetStringFromObj(objv[3], NULL);
            nodePtr = TclDomCreateCDATANode(interp, interpDataPtr,
		            documentPtr, text);
	    AddCreatedNode(interp, interpDataPtr, subjectNodePtr, nodePtr);
            return TclDomSetNodeResult(interp, interpDataPtr, nodePtr);

        case CREATE_PROCESSING_INSTRUCTION:
            if (objc != 5) {
                Tcl_WrongNumArgs(interp, 2, objv, "token target data");
                return TCL_ERROR;
            }
            target = Tcl_GetStringFromObj(objv[3], NULL);
            text = Tcl_GetStringFromObj(objv[4], NULL);
            nodePtr = TclDomCreateProcessingInstructionNode(interp,
		            interpDataPtr, documentPtr, 
                    target, text);
	    AddCreatedNode(interp, interpDataPtr, subjectNodePtr, nodePtr);
            return TclDomSetNodeResult(interp, interpDataPtr, nodePtr);

        case CREATE_ATTRIBUTE:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token name");
                return TCL_ERROR;
            }
            return TCL_ERROR;

        case CREATE_ENTITY:
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "token");
                return TCL_ERROR;
            }
            Tcl_AppendResult(interp,
		            "document createEntity method not implemented",
		            (char *) NULL);
            return TCL_ERROR;

        case CREATE_ENTITY_REFERENCE:
            if (objc != 3) {
                Tcl_WrongNumArgs(interp, 2, objv, "token");
                return TCL_ERROR;
            }
            Tcl_AppendResult(interp,
		            "document createEntityReference method not implemented",
		            (char *) NULL);
            return TCL_ERROR;

	    case CREATE_DOCTYPE_DECL:
            if (objc != 8) {
                Tcl_WrongNumArgs(interp, 2, objv, 
						"token name external id dtd entities notations");
                return TCL_ERROR;
            }
            Tcl_AppendResult(interp,
		            "document createDocType method not implemented",
		            (char *) NULL);
            return TCL_ERROR;

	    case GET_ELEMENTS_BY_TAGNAME:
            if (objc != 4) {
                Tcl_WrongNumArgs(interp, 2, objv, "token name");
                return TCL_ERROR;
            }
            tagName = Tcl_GetStringFromObj(objv[3], NULL);
            return TclDomGetElementsByTagname(interp, interpDataPtr, tagName,
		            documentPtr->selfPtr);

        case IMPORT_NODE:
            if (objc != 4 && objc != 6) {
                Tcl_WrongNumArgs(interp, 2, objv,
			            "token token ?-deep deepFlag?");
                return TCL_ERROR;
            }
            if (objc == 6) {
                char *option;
                int result;
                option = Tcl_GetStringFromObj(objv[4], NULL);
                if (strcmp(option, "-deep")) {
                    Tcl_AppendResult(interp, 
							"invalid option, should be \"-deep\"");
                    return TCL_ERROR;
                }
                result = Tcl_GetBooleanFromObj(interp, objv[5], &deepFlag);
                if (result != TCL_OK) {
                    return result;
                }
            }
            importNodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, 
					objv[3]);
            if (importNodePtr == NULL) {
                return TCL_ERROR;
            }
            nodePtr = TclDomImportNode(interp, interpDataPtr, 
					documentPtr, importNodePtr, deepFlag);
            if (nodePtr) {
                return TclDomSetNodeResult(interp, interpDataPtr, nodePtr);
            } else {
                return TCL_ERROR;
            }

        default:
            Tcl_SetResult(interp, "unknown method", TCL_STATIC);
            return TCL_ERROR;
    }
    
    /*NOTREACHED*/
}


/*
 *--------------------------------------------------------------
 *
 * TclDomDocumentTraversalCmd --
 *
 *      This procedure implements the DOM  DocumentTraversal
 *      interface.
 *      Refer to the user documentation to see what it does.
 *
 * Results:
 *      Return TCL_OK if a method succeeded; TCL_ERROR
 *      otherwise.
 *
 * Side effects:
 *      Depends on the command methods.
 *
 *--------------------------------------------------------------
 */

static int
TclDomDocumentTraversalCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int methodIndex;
    TclDomInterpData *interpDataPtr;
    int i, j, numberNodeTypes;
    char *option, *nodeName;
    TclDomNode *nodePtr;
    int expandEntityReferences;
    Tcl_Obj *listObjPtr, *nodeNameObjPtr;
    unsigned int whatToShow = SHOW_ALL;
    unsigned int nodeType; 
    Tcl_Obj *filterObjPtr = NULL;

    static CONST84 char *methods[] = {
	    "createNodeIterator", "createTreeWalker", "destroy", NULL
    };

    enum methods {
        CREATE_NODE_ITERATOR, CREATE_TREE_WALKER, DESTROY
    };


    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "method token ...");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], methods, "method", 0, 
	        &methodIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    if (methodIndex == DESTROY) {
        return TclDomDestroyTraversalObject(clientData, interp, objc, objv);
    }

    if (objc > 3 && ((objc & 0x1) == 0)) {
        Tcl_AppendResult(interp, "missing option value", (char *) NULL);
	    return TCL_ERROR;
    }

    for (i = 3; i < objc; i++) {
        option = Tcl_GetStringFromObj(objv[i], NULL);
        if (strcmp(option, "-show") == 0) {
            i++;
            listObjPtr = objv[i];
            if (Tcl_ListObjLength(interp, listObjPtr, &numberNodeTypes)
		            != TCL_OK) {
                Tcl_AppendResult(interp, "invalid list of node types to show",
			            (char *) NULL);
                return TCL_ERROR;
            }
            whatToShow = 0;
            for (j = 0; j < numberNodeTypes; j++) {
                if (Tcl_ListObjIndex(interp, listObjPtr, j, &nodeNameObjPtr)
			            != TCL_OK) {
                    Tcl_AppendResult(interp,
			                "invalid list of node types to show",
			                (char *) NULL);
                    return TCL_ERROR;
                }
                nodeName = Tcl_GetStringFromObj(nodeNameObjPtr, NULL);
                if (*nodeName == '-') {
                    if (TclDomGetTypeMaskFromName(interp, nodeName+1,
			                &nodeType) != TCL_OK) {
                        return TCL_ERROR;
                    }
                    whatToShow &= (~nodeType);
                } else if (*nodeName == '+') {
                    if (TclDomGetTypeMaskFromName(interp, nodeName+1,
			                &nodeType) != TCL_OK) {
                        return TCL_ERROR;
                    }
                    whatToShow |= nodeType;
                } else {
                    if (TclDomGetTypeMaskFromName(interp, nodeName,
			                &nodeType) != TCL_OK) {
                        return TCL_ERROR;
                    }
                    whatToShow |= nodeType;
                }
            }
        } else if (strcmp(option, "-filter") == 0) {
            i++;
            filterObjPtr = objv[i];
        } else if (strcmp(option, "-expandEntities") == 0) {
            i++;
            if (Tcl_GetBooleanFromObj(interp, objv[i],
		            &expandEntityReferences) != TCL_OK) {
                return TCL_ERROR;
            }
        } else {
            Tcl_AppendResult(interp, "invalid option", (char *) NULL);
            return TCL_ERROR;
        }
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[2]);
    if (nodePtr == NULL) {
        return TCL_ERROR;
    }
 
    switch ((enum methods) methodIndex) {
        case CREATE_NODE_ITERATOR:
            return TclDomCreateNodeIterator(interp, interpDataPtr, nodePtr,
		            whatToShow, filterObjPtr, expandEntityReferences);

        case CREATE_TREE_WALKER:
	        return TclDomCreateTreeWalker(interp, interpDataPtr, nodePtr,
		            whatToShow, filterObjPtr, expandEntityReferences);

        default:
            Tcl_SetResult(interp, "unknown method", TCL_STATIC);
            return TCL_ERROR;
    }
    
    /*NOTREACHED*/
}


/*
 *--------------------------------------------------------------
 *
 * TclDomNodeIteratorCmd --
 *
 *      This procedure processes commands for DOM NodeIterator
 *      objects.
 *      Refer to the user documentation to see what it does.
 *
 * Results:
 *      Return TCL_OK if a method succeeded; TCL_ERROR
 *      otherwise.
 *
 * Side effects:
 *      Depends on the command methods.
 *
 *--------------------------------------------------------------
 */

static int
TclDomNodeIteratorCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int methodIndex;
    TclDomInterpData *interpDataPtr;
    TclDomNodeIterator *nodeIteratorPtr;
    int result;

    static CONST84 char *methods[] = {
	    "cget", "configure", "previousNode", "nextNode", NULL
    };

    enum methods {
        CGET, CONFIGURE, PREVIOUS_NODE, NEXT_NODE,
    };

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "method handle ?arg ...?");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], methods, "method", 0, 
	        &methodIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    nodeIteratorPtr = TclDomGetNodeIteratorFromToken(interp, interpDataPtr,
	        objv[2]);
    if (nodeIteratorPtr == NULL) {
        return TCL_ERROR;
    }

    if (methodIndex > CONFIGURE) {
        if (objc != 3) {
            Tcl_WrongNumArgs(interp, 1, objv, "method handle");
            return TCL_ERROR;
        }
    }

    Tcl_Preserve((ClientData) nodeIteratorPtr);

    switch ((enum methods) methodIndex) {
        case CGET:
            result = TclDomIteratorCGetCmd(clientData, interp, objc, objv);
            break;

        case CONFIGURE:
            if (objc == 5) {
                Tcl_SetResult(interp, NO_MODIFICATION_ALLOWED_ERR_TEXT,
			            TCL_STATIC);
                return TCL_ERROR;
            }
            result = TclDomIteratorCGetCmd(clientData, interp, objc, objv);
            break;

        case PREVIOUS_NODE:
            result = TclDomGetPreviousNodeFromIterator(interp, interpDataPtr,
		            nodeIteratorPtr);
            break;

        case NEXT_NODE:
            result = TclDomGetNextNodeFromIterator(interp, interpDataPtr,
		            nodeIteratorPtr);
            break;

        default:
            Tcl_SetResult(interp, "unknown method", TCL_STATIC);
            result = TCL_ERROR;
    }

    Tcl_Release((ClientData) nodeIteratorPtr);
    
    return result;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomTreeWalkerCmd --
 *
 *      This procedure processes commands for DOM TreeWalker
 *      objects.
 *      Refer to the user documentation to see what it does.
 *
 * Results:
 *      Return TCL_OK if a method succeeded; TCL_ERROR
 *      otherwise.
 *
 * Side effects:
 *      Depends on the command methods.
 *
 *--------------------------------------------------------------
 */

static int
TclDomTreeWalkerCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int methodIndex;
    TclDomInterpData *interpDataPtr;
    TclDomTreeWalker *treeWalkerPtr;
    int result;

    static CONST84 char *methods[] = {
	    "cget", "configure", "parentNode", "firstChild", "lastChild",
	    "previousSibling", "nextSibling", "previousNode", "nextNode",
	    NULL
    };

    enum methods {
        CGET, CONFIGURE, PARENT_NODE, FIRST_CHILD, LAST_CHILD,
	    PREVIOUS_SIBLING, NEXT_SIBLING, PREVIOUS_NODE, NEXT_NODE
    };

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "method handle ?arg ...?");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], methods, "method", 0, 
	        &methodIndex) != TCL_OK) {
        return TCL_ERROR;
    }

    if (methodIndex > CONFIGURE) {
        if (objc != 3) {
            Tcl_WrongNumArgs(interp, 1, objv, "method handle");
            return TCL_ERROR;
        }
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    treeWalkerPtr = TclDomGetTreeWalkerFromToken(interp, interpDataPtr,
	        objv[2]);
    if (treeWalkerPtr == NULL) {
        return TCL_ERROR;
    }

    Tcl_Preserve((ClientData) treeWalkerPtr);

    switch ((enum methods) methodIndex) {
        case CGET:
            result = TclDomTreeWalkerCGetCmd(clientData, interp, objc, objv);
            break;

        case CONFIGURE:
            result = TclDomTreeWalkerConfigureCmd(clientData, interp, objc,
		            objv);
            break;

        case PARENT_NODE:
            result = TclDomGetParentNodeFromTreeWalker(interp, interpDataPtr,
		            treeWalkerPtr);
            break;

        case FIRST_CHILD:
            return TclDomGetFirstChildFromTreeWalker(interp, interpDataPtr,
		            treeWalkerPtr);

        case LAST_CHILD:
            result = TclDomGetLastChildFromTreeWalker(interp, interpDataPtr,
		            treeWalkerPtr);
            break;

        case PREVIOUS_SIBLING:
            result =  TclDomGetPreviousSiblingFromTreeWalker(interp,
		            interpDataPtr, treeWalkerPtr);
            break;

        case NEXT_SIBLING:
            result = TclDomGetNextSiblingFromTreeWalker(interp, interpDataPtr,
		            treeWalkerPtr);
            break;

        case PREVIOUS_NODE:
            result =  TclDomGetPreviousNodeFromTreeWalker(interp, interpDataPtr,
		            treeWalkerPtr);
            break;

        case NEXT_NODE:
            result =  TclDomGetNextNodeFromTreeWalker(interp, interpDataPtr,
		            treeWalkerPtr);
            break;

        default:
            Tcl_SetResult(interp, "unknown method", TCL_STATIC);
            result = TCL_ERROR;
            break;
    }

    Tcl_Release((ClientData) treeWalkerPtr);
    
    return result;
}


/*
 *--------------------------------------------------------------
 *
 * TclDomIteratorCGetCmd --
 *
 *      This procedure handles the cget method for a 
 *      TclDom node command.
 *
 * Results:
 *      Return TCL_OK if a valid attribute is requested;
 *      otherwise return TCL_ERROR.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */

static int
TclDomIteratorCGetCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int optionIndex;
    TclDomInterpData *interpDataPtr;
    TclDomNodeIterator *nodeIteratorPtr;
    char *nodeName;
    int i;

    static CONST84 char *iteratorOptions[] = {"-show", "-filter", 
        "-expandEntities", NULL
    };

    enum iteratorOptions {
        WHAT_TO_SHOW, NODE_FILTER, EXPAND_ENTITY_REFERENCES
    };

    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "iterator option");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], iteratorOptions, "option", 0, 
	        &optionIndex) != TCL_OK) {
        return TCL_ERROR;
    } 

    interpDataPtr = (TclDomInterpData *) clientData;

    nodeIteratorPtr = TclDomGetNodeIteratorFromToken(interp, interpDataPtr,
	    objv[2]);
    if (nodeIteratorPtr == NULL) {
        return TCL_ERROR;
    }

    switch ((enum iteratorOptions) optionIndex) {
        case NODE_FILTER:
            if (nodeIteratorPtr->filterPtr->filterCmdPtr) {
                Tcl_SetObjResult(interp,
			            nodeIteratorPtr->filterPtr->filterCmdPtr);
            }
            return TCL_OK;

        case EXPAND_ENTITY_REFERENCES:
            Tcl_SetObjResult(interp, 
					Tcl_NewBooleanObj(nodeIteratorPtr->expandEntityReferences));
            return TCL_OK;

        case WHAT_TO_SHOW:
            /* 
             * convert bitmap to list of element types
             */
            for (i = 1; i < 13; i++) {
                if (nodeIteratorPtr->whatToShow & (1 << (i-1))) {
                    TclDomGetNameFromEnum(i, &nodeName);
                    Tcl_AppendElement(interp, nodeName);
                }
            }
            return TCL_OK;

        default:
            Tcl_SetResult(interp, "unknown option", TCL_STATIC);
            return TCL_ERROR;
    }
    /*NOTREACHED*/
}


/*
 *--------------------------------------------------------------
 *
 * TclDomTreeWalkerCGetCmd --
 *
 *      This procedure handles the cget method for a 
 *      TclDom node command.
 *
 * Results:
 *      Return TCL_OK if a valid attribute is requested;
 *      otherwise return TCL_ERROR.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */

static int
TclDomTreeWalkerCGetCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int optionIndex;
    TclDomInterpData *interpDataPtr;
    TclDomTreeWalker *treeWalkerPtr;
    char *nodeName;
    int i;
   
    if (objc != 4) {
        Tcl_WrongNumArgs(interp, 2, objv, "treewalker option");
        return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], treeWalkerOptions, "option", 0, 
	        &optionIndex) != TCL_OK) {
        return TCL_ERROR;
    } 

    interpDataPtr = (TclDomInterpData *) clientData;

    treeWalkerPtr = TclDomGetTreeWalkerFromToken(interp, interpDataPtr,
	        objv[2]);
    if (treeWalkerPtr == NULL) {
        return TCL_ERROR;
    }

    switch ((enum treeWalkerOptions) optionIndex) {
        case NODE_FILTER:
            if (treeWalkerPtr->filterPtr->filterCmdPtr) {
                Tcl_SetObjResult(interp,
			            treeWalkerPtr->filterPtr->filterCmdPtr);
            }
            return TCL_OK;

        case EXPAND_ENTITY_REFERENCES:
            Tcl_SetObjResult(interp, 
					Tcl_NewBooleanObj(treeWalkerPtr->expandEntityReferences));
            return TCL_OK;

        case WHAT_TO_SHOW:
            /* 
             * convert bitmap to list of element types
             */
            for (i = 1; i < 13; i++) {		/* %%% <<== 13 */
                if (treeWalkerPtr->whatToShow & (1 << (i-1))) {
                    TclDomGetNameFromEnum(i, &nodeName);
                    Tcl_AppendElement(interp, nodeName);
                }
            }
            return TCL_OK;

        case CURRENT_NODE:
            if (treeWalkerPtr->currentNodePtr != NULL) {
                return TclDomSetNodeResult(interp, interpDataPtr,
			            treeWalkerPtr->currentNodePtr);
            } else {
                return TCL_OK;
            }

        default:
            Tcl_SetResult(interp, "unknown option", TCL_STATIC);
            return TCL_ERROR;
    }
    /*NOTREACHED*/
}


/*
 *--------------------------------------------------------------
 *
 * TclDomTreeWalkerConfigureCmd --
 *
 *      This procedure handles the configure method for a 
 *      TclDom node command.
 *
 * Results:
 *      Return TCL_OK if a valid attribute is requested;
 *      otherwise return TCL_ERROR.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */

static int
TclDomTreeWalkerConfigureCmd(
    ClientData clientData,	    /* State data for this interp */
    Tcl_Interp *interp,		    /* Current interpreter. */
    int objc,			        /* Number of arguments. */
    Tcl_Obj *CONST objv[])	    /* The argument objects. */
{
    int optionIndex;
    TclDomInterpData *interpDataPtr;
    TclDomTreeWalker *treeWalkerPtr;
    TclDomNode *nodePtr;

    static CONST84 char *writableNodeOptions[] = {
        "-currentNode",
        NULL
    };

    enum writableNodeOptions {
        CURRENT_NODE,
    };

    if (objc != 5) {
        return TclDomTreeWalkerCGetCmd(clientData, interp, objc, objv);
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], treeWalkerOptions, "option", 0, 
	        &optionIndex) != TCL_OK) {
        return TCL_ERROR;
    }
    
    if (Tcl_GetIndexFromObj(interp, objv[3], writableNodeOptions,
	        "writable option", 0, &optionIndex) != TCL_OK) {
        Tcl_SetResult(interp, NO_MODIFICATION_ALLOWED_ERR_TEXT, TCL_STATIC);
        return TCL_ERROR;
    }

    interpDataPtr = (TclDomInterpData *) clientData;

    treeWalkerPtr = TclDomGetTreeWalkerFromToken(interp, interpDataPtr,
	        objv[2]);
    if (treeWalkerPtr == NULL) {
        return TCL_ERROR;
    }

    switch ((enum nodeOptions) optionIndex) {        
        case CURRENT_NODE:
	        nodePtr = TclDomGetNodeFromToken(interp, interpDataPtr, objv[4]);
            if (nodePtr == NULL) {
                return TCL_ERROR;
            }
            treeWalkerPtr->currentNodePtr = nodePtr;
            return TCL_OK;

        default:
            Tcl_SetResult(interp, "unknown option", TCL_STATIC);
            return TCL_ERROR;
    }
    /*NOTREACHED*/
}
