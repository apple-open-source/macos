/*----------------------------------------------------------------------------
|   Copyright (C) 1999  Jochen C. Loewer (loewerj@hotmail.com)
+-----------------------------------------------------------------------------
|
|   Rcsid: @(#)$Id: nodecmd.c,v 1.20 2005/03/26 14:08:06 rolf Exp $
|
|   The contents of this file are subject to the Mozilla Public License
|   Version 1.1 (the "License"); you may not use this file except in
|   compliance with the License. You may obtain a copy of the License at
|   http://www.mozilla.org/MPL/
|
|   Software distributed under the License is distributed on an "AS IS"
|   basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
|   License for the specific language governing rights and limitations
|   under the License.
|
|   The Original Code is tDOM.
|
|   The Initial Developer of the Original Code is Jochen Loewer.
|
|   Portions created by Jochen Loewer are Copyright (C) 1998, 1999
|   Jochen Loewer. All Rights Reserved.
|
|   Portions created by Zoran Vasiljevic are Copyright (C) 2000-2002
|   Zoran Vasiljevic. All Rights Reserved.
|
|   Portions created by Rolf Ade are Copyright (C) 1999-2002
|   Rolf Ade. All Rights Reserved.
|
|   Written by Zoran Vasiljevic
|   July 12, 2000
|
\---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
|   Includes
|
\---------------------------------------------------------------------------*/
#include <dom.h>
#include <tcldom.h>
#include <tcl.h>
#include <nodecmd.h>

#define PARSER_NODE 9999 /* Hack so that we can invoke XML parser */
/* More hacked domNodeTypes - used to signal, that we want to check
   name/data of the node to create. */
#define ELEMENT_NODE_ANAME_CHK 10000
#define ELEMENT_NODE_AVALUE_CHK 10001
#define ELEMENT_NODE_CHK 10002
#define TEXT_NODE_CHK 10003
#define COMMENT_NODE_CHK 10004
#define CDATA_SECTION_NODE_CHK 10005
#define PROCESSING_INSTRUCTION_NODE_NAME_CHK 10006
#define PROCESSING_INSTRUCTION_NODE_VALUE_CHK 10007
#define PROCESSING_INSTRUCTION_NODE_CHK 10008

/*----------------------------------------------------------------------------
|   Types
|
|   This structure represents one stack slot. The stack itself
|   is implemented as double-linked-list of following structures.
|
\---------------------------------------------------------------------------*/
typedef struct StackSlot {
    void             *element;   /* The stacked element */
    struct StackSlot *nextPtr;   /* Next link */
    struct StackSlot *prevPtr;   /* Previous link */
} StackSlot;

/*----------------------------------------------------------------------------
|   Beginning of the stack and current element pointer are local
|   to current thread and also local to this file.
|   For non-threaded environments, it's a regular static.
|
\---------------------------------------------------------------------------*/
typedef struct CurrentStack {
    StackSlot *elementStack;
    StackSlot *currentSlot;
} CurrentStack;

/*----------------------------------------------------------------------------
|   Structure used as clientData of the created commands.
|   The structure stores, which type of node the command has
|   to create and, in case of elementNodes and if given, the
|   namespace of the node.
\---------------------------------------------------------------------------*/
typedef struct NodeInfo {
    int   type;
    char *namespace;
} NodeInfo;

#ifndef TCL_THREADS
  static CurrentStack dataKey;
# define TSDPTR(a) a
#else
  static Tcl_ThreadDataKey dataKey;
# define TSDPTR(a) (CurrentStack*)Tcl_GetThreadData((a),sizeof(CurrentStack))
#endif

/*----------------------------------------------------------------------------
|   Forward declarations
|
\---------------------------------------------------------------------------*/
static void * StackPush  _ANSI_ARGS_((void *));
static void * StackPop   _ANSI_ARGS_((void));
static void * StackTop   _ANSI_ARGS_((void));
static int    NodeObjCmd _ANSI_ARGS_((ClientData,Tcl_Interp*,int,Tcl_Obj *CONST o[]));
static void   StackFinalize _ANSI_ARGS_((ClientData));

extern int tcldom_appendXML (Tcl_Interp*, domNode*, Tcl_Obj*);


/*----------------------------------------------------------------------------
|   StackPush
|
\---------------------------------------------------------------------------*/
static void *
StackPush (element)
    void *element;
{
    StackSlot *newElement;
    CurrentStack *tsdPtr = TSDPTR(&dataKey);

    /*-------------------------------------------------------------------
    |   Reuse already allocated stack slots, if any
    |
    \------------------------------------------------------------------*/
    if (tsdPtr->currentSlot && tsdPtr->currentSlot->nextPtr) {
        tsdPtr->currentSlot = tsdPtr->currentSlot->nextPtr;
        tsdPtr->currentSlot->element = element;
        return element;
    }

    /*-------------------------------------------------------------------
    |   Allocate new stack slot
    |
    \------------------------------------------------------------------*/
    newElement = (StackSlot *)MALLOC(sizeof(StackSlot));
    memset(newElement, 0, sizeof(StackSlot));

    if (tsdPtr->elementStack == NULL) {
        tsdPtr->elementStack = newElement;
#ifdef TCL_THREADS
        Tcl_CreateThreadExitHandler(StackFinalize, tsdPtr->elementStack);
#else
        Tcl_CreateExitHandler (StackFinalize, tsdPtr->elementStack);
#endif
    } else {
        tsdPtr->currentSlot->nextPtr = newElement;
        newElement->prevPtr = tsdPtr->currentSlot;
    }

    tsdPtr->currentSlot = newElement;
    tsdPtr->currentSlot->element = element;

    return element;
}

/*----------------------------------------------------------------------------
|   StackPop  -  pops the element from stack
|
\---------------------------------------------------------------------------*/
static void *
StackPop ()
{
    void *element;
    CurrentStack *tsdPtr = TSDPTR(&dataKey);

    element = tsdPtr->currentSlot->element;
    if (tsdPtr->currentSlot->prevPtr) {
        tsdPtr->currentSlot = tsdPtr->currentSlot->prevPtr;
    } else {
        tsdPtr->currentSlot->element = NULL;
    }

    return element;
}

/*----------------------------------------------------------------------------
|   StackTop  -  returns top-level element from stack
|
\---------------------------------------------------------------------------*/
static void *
StackTop ()
{
    CurrentStack *tsdPtr = TSDPTR(&dataKey);

    if (tsdPtr->currentSlot == NULL) {
        return NULL;
    }

    return tsdPtr->currentSlot->element;
}


/*----------------------------------------------------------------------------
|   StackFinalize - reclaims stack memory (slots only, not elements)
|
\---------------------------------------------------------------------------*/
static void
StackFinalize (clientData)
    ClientData clientData;
{
    StackSlot *tmp, *stack = (StackSlot *)clientData;

    while (stack) {
        tmp = stack->nextPtr;
        FREE((char*)stack);
        stack = tmp;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * namespaceTail --
 *
 *      Returns the trailing name at the end of a string with "::"
 *      namespace qualifiers. These qualifiers are namespace names
 *      separated by "::"s. For example, for "::foo::p" this function
 *      returns a pointer to the "p" in that obj string rep, and for
 *      "::" it returns a pointer to "".
 *
 * Results:
 *	Returns a pointer to the start of the tail name.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static char*
namespaceTail (nameObj) 
    Tcl_Obj *nameObj;
{
    char *name,*p;
    int   len;
    
    name = Tcl_GetStringFromObj(nameObj, &len);
    p = name + len;
    /* Isolate just the tail name, i.e. skip it's parent namespace */
    while (--p > name) {
        if ((*p == ':') && (*(p-1) == ':')) {
            p++; /* just after the last "::" */
            name = p;
            break;
        }
    }
    return name;
}

/*----------------------------------------------------------------------------
|   NodeObjCmdDeleteProc
|
\---------------------------------------------------------------------------*/
static void
NodeObjCmdDeleteProc (
    ClientData clientData
    )
{
    NodeInfo *nodeInfo = (NodeInfo *) clientData;
    
    if (nodeInfo->namespace) {
        FREE (nodeInfo->namespace);
    }
    FREE (nodeInfo);
}

/*----------------------------------------------------------------------------
|   NodeObjCmd
|
\---------------------------------------------------------------------------*/
static int
NodeObjCmd (arg, interp, objc, objv)
    ClientData      arg;                /* Type of node to create. */
    Tcl_Interp    * interp;             /* Current interpreter. */
    int             objc;               /* Number of arguments. */
    Tcl_Obj *CONST  objv[];             /* Argument objects. */
{
    int type, createType, len, dlen, i, ret, disableOutputEscaping = 0, 
        index = 1;
    char *tag, *p, *tval, *aval;
    domNode *parent, *newNode = NULL;
    domDocument *doc;
    Tcl_Obj *cmdObj, **opts;
    NodeInfo *nodeInfo = (NodeInfo*) arg;

    /*------------------------------------------------------------------------
    |   Need parent node to get the owner document and to append new 
    |   child tag to it. The current parent node is stored on the stack.
    |
    \-----------------------------------------------------------------------*/

    parent = (domNode *)StackTop();    
    if (parent == NULL) {
        Tcl_AppendResult(interp, "called outside domNode context", NULL);
        return TCL_ERROR;
    }
    doc = parent->ownerDocument;

    /*------------------------------------------------------------------------
    |   Create new node according to type. Special case is the ELEMENT_NODE
    |   since here we may enter into recursion. The ELEMENT_NODE is the only
    |   node type which may have script body as last argument.
    |
    \-----------------------------------------------------------------------*/

    ret  = TCL_OK;
    type = nodeInfo->type;

    switch (abs(type)) {
    case CDATA_SECTION_NODE:     
    case CDATA_SECTION_NODE_CHK: 
    case COMMENT_NODE:           
    case COMMENT_NODE_CHK:       
    case TEXT_NODE:              
    case TEXT_NODE_CHK:
        if (objc != 2) {
            if (abs(type) == TEXT_NODE || abs(type) == TEXT_NODE_CHK) {
                if (objc != 3 ||
                    strcmp ("-disableOutputEscaping",
                            Tcl_GetStringFromObj (objv[1], &len))!=0) {
                    Tcl_WrongNumArgs(interp, 1, objv,
                                     "?-disableOutputEscaping? text");
                    return TCL_ERROR;
                } else {
                    disableOutputEscaping = 1;
                    index = 2;
                }
            } else {
                Tcl_WrongNumArgs(interp, 1, objv, "text");
                return TCL_ERROR;
            }
        }
        tval = Tcl_GetStringFromObj(objv[index], &len);
        switch (abs(type)) {
        case TEXT_NODE_CHK:
            if (!tcldom_textCheck (interp, tval, "text")) return TCL_ERROR;
            createType = TEXT_NODE;
            break;
        case COMMENT_NODE_CHK:
            if (!tcldom_commentCheck (interp, tval)) return TCL_ERROR;
            createType = COMMENT_NODE;
            break;
        case CDATA_SECTION_NODE_CHK:
            if (!tcldom_CDATACheck (interp, tval)) return TCL_ERROR;
            createType = CDATA_SECTION_NODE;
            break;
        default:
            createType = nodeInfo->type;
            break;
        }
        newNode = (domNode*)domNewTextNode(doc, tval, len, createType);
        if (disableOutputEscaping) {
            newNode->nodeFlags |= DISABLE_OUTPUT_ESCAPING;
        }
        domAppendChild(parent, newNode);
        break;

    case PROCESSING_INSTRUCTION_NODE_NAME_CHK:
    case PROCESSING_INSTRUCTION_NODE_VALUE_CHK:
    case PROCESSING_INSTRUCTION_NODE_CHK:
    case PROCESSING_INSTRUCTION_NODE:
        if (objc != 3) {
            Tcl_WrongNumArgs(interp, 1, objv, "target data");
            return TCL_ERROR;
        } 
        tval = Tcl_GetStringFromObj(objv[1], &len);
        if (abs(type) == PROCESSING_INSTRUCTION_NODE_NAME_CHK
            || abs(type) == PROCESSING_INSTRUCTION_NODE_CHK) {
            if (!tcldom_PINameCheck (interp, tval)) return TCL_ERROR;
        }
        aval = Tcl_GetStringFromObj(objv[2], &dlen);
        if (abs(type) == PROCESSING_INSTRUCTION_NODE_VALUE_CHK
            || abs(type) == PROCESSING_INSTRUCTION_NODE_CHK) {
            if (!tcldom_PIValueCheck (interp, aval)) return TCL_ERROR;
        }
        newNode = (domNode *)
            domNewProcessingInstructionNode(doc, tval, len, aval, dlen);
        domAppendChild(parent, newNode);
        break;

    case PARSER_NODE: /* non-standard node-type : a hack! */
        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 1, objv, "markup");
            return TCL_ERROR;
        }
        ret = tcldom_appendXML(interp, parent, objv[1]);
        break;

    case ELEMENT_NODE_ANAME_CHK:
    case ELEMENT_NODE_AVALUE_CHK:
    case ELEMENT_NODE_CHK:
    case ELEMENT_NODE:
        tag = Tcl_GetStringFromObj(objv[0], &len);
        p = tag + len;
        /* Isolate just the tag name, i.e. skip it's parent namespace */
        while (--p > tag) {
            if ((*p == ':') && (*(p-1) == ':')) {
                p++; /* just after the last "::" */
                tag = p;
                break;
            }
        }

        newNode = domAppendNewElementNode (parent, tag, NULL);
        
        /*
         * Allow for following syntax:
         *   cmd ?-option value ...? ?script?
         *   cmd ?opton value ...? ?script?
         *   cmd key_value_list script
         *       where list contains "-key value ..." or "key value ..."
         */

        if ((objc % 2) == 0) {
            cmdObj = objv[objc-1];
            len  = objc - 2; /* skip both command and script */
            opts = (Tcl_Obj**)objv + 1;
        } else if((objc == 3)
                  && Tcl_ListObjGetElements(interp,objv[1],&len,&opts)==TCL_OK
                  && (len == 0 || len > 1)) {
            if ((len % 2)) {
                Tcl_AppendResult(interp, "list must have "
                                 "an even number of elements", NULL);
                return TCL_ERROR;
            }
            cmdObj = objv[2];
        } else {
            cmdObj = NULL;
            len  = objc - 1; /* skip command */
            opts = (Tcl_Obj**)objv + 1;
        }
        for (i = 0; i < len; i += 2) {
            tval = Tcl_GetString(opts[i]);
            if (*tval == '-') {
                tval++;
            }
            if (abs(type) == ELEMENT_NODE_ANAME_CHK
                || abs(type) == ELEMENT_NODE_CHK) {
                if (!tcldom_nameCheck (interp, tval, "attribute", 0)) {
                    return TCL_ERROR;
                }
            }
            aval = Tcl_GetString(opts[i+1]);
            if (abs(type) == ELEMENT_NODE_AVALUE_CHK
                || abs(type) == ELEMENT_NODE_CHK) {
                if (!tcldom_textCheck (interp, aval, "attribute")) {
                    return TCL_ERROR;
                }
            }
            domSetAttribute(newNode, tval, aval);
        }
        if (cmdObj) {
            ret = nodecmd_appendFromScript(interp, newNode, cmdObj);
        }
        break;
    }

    if (type < 0 && newNode != NULL) {
        char buf[64];
        tcldom_createNodeObj(interp, newNode, buf);
        Tcl_SetObjResult(interp, Tcl_NewStringObj(buf, strlen(buf)));
    }
    
    if (ret == TCL_OK) doc->nodeFlags |= NEEDS_RENUMBERING;
    return ret;
}

/*----------------------------------------------------------------------------
|   nodecmd_createNodeCmd  -  implements the "createNodeCmd" method of
|                             "dom" Tcl command
|
|   This command is used to generate other Tcl commands which in turn
|   generate tDOM nodes. These new commands can only be called within
|   the context of the domNode command, however.
|
|   Syntax: dom createNodeCmd ?-returnNodeCmd? <nodeType> cmdName
|
|           where <nodeType> can be one of:
|              elementNode, commentNode, textNode, cdataNode or piNode
|
|   The optional "-returnNodeCmd" parameter, if given, instructs the
|   command to return the object-based node command of the newly generated
|   node. Without the parameter, the command returns current interp result.
|
|   Example:
|
|      % dom createNodeCmd                elementNode html::body
|      % dom createNodeCmd -returnNodeCmd elementNode html::title
|      % dom createNodeCmd                textNode    html::t
|
|   And usage:
|
|      % set d [dom createDocument html]
|      % set n [$d documentElement]
|      % $n appendFromScript {
|           html::body {
|           set title [html::title {html::t "This is an example"}]
|           $title setAttribute dummy 1
|      }
|      % puts [$n asHTML]
|
\---------------------------------------------------------------------------*/
int
nodecmd_createNodeCmd (interp, objc, objv, checkName, checkCharData)
    Tcl_Interp    * interp;             /* Current interpreter. */
    int             objc;               /* Number of arguments. */
    Tcl_Obj *CONST  objv[];             /* Argument objects. */
    int             checkName;          /* Flag: Name checks? */
    int             checkCharData;      /* Flag: Data checks? */
{
    int ix, index, ret, type, nodecmd = 0;
    char *nsName, buf[64];
    Tcl_DString cmdName;
    NodeInfo *nodeInfo;

    /*
     * Syntax:  
     *
     *     dom createNodeCmd ?-returnNodeCmd? nodeType commandName
     */

    enum subCmd {
        ELM_NODE, TXT_NODE, CDS_NODE, CMT_NODE, PIC_NODE, PRS_NODE
    };

    static CONST84 char *subcmds[] = {
        "elementNode", "textNode", "cdataNode", "commentNode", "piNode",
        "parserNode", NULL
    };

    if (objc != 3 && objc != 4) {
        goto usage;
    }
    if (objc == 4) {
        if (strcmp(Tcl_GetString(objv[1]), "-returnNodeCmd")) {
            goto usage;
        }
        nodecmd = 1;
        ix = 2;
    } else {
        nodecmd = 0;
        ix = 1;
    }
    ret = Tcl_GetIndexFromObj(interp, objv[ix], subcmds, "option", 0, &index);
    if (ret != TCL_OK) {
        return ret;
    }

    /*--------------------------------------------------------------------
    |   Construct fully qualified command name using current namespace
    |
    \-------------------------------------------------------------------*/
    Tcl_DStringInit(&cmdName);
    strcpy(buf, "namespace current");
    ret = Tcl_Eval(interp, buf);
    if (ret != TCL_OK) {
        return ret;
    }
    nsName = (char *)Tcl_GetStringResult(interp);
    Tcl_DStringAppend(&cmdName, nsName, -1);
    if (strcmp(nsName, "::")) {
        Tcl_DStringAppend(&cmdName, "::", 2);
    }
    Tcl_DStringAppend(&cmdName, Tcl_GetString(objv[ix+1]), -1);

    nodeInfo = (NodeInfo *) MALLOC (sizeof (NodeInfo));
    nodeInfo->namespace = NULL;
    Tcl_ResetResult (interp);
    switch ((enum subCmd)index) {
    case ELM_NODE: 
        if (!tcldom_nameCheck(interp, namespaceTail(objv[ix+1]), "tag", 0)) {
            FREE (nodeInfo);
            return TCL_ERROR;
        }
        if (checkName && checkCharData) {
            type = ELEMENT_NODE_CHK;
        } else if (checkName) {
            type = ELEMENT_NODE_ANAME_CHK;
        } else if (checkCharData) {
            type = ELEMENT_NODE_AVALUE_CHK;
        } else {
            type = ELEMENT_NODE;
        }
        break;
    case PRS_NODE: 
        type = PARSER_NODE;
        break;
    case TXT_NODE: 
        if (checkCharData) {
            type = TEXT_NODE_CHK;
        } else {
            type = TEXT_NODE;
        }
        break;
    case CDS_NODE: 
        if (checkCharData) {
            type = CDATA_SECTION_NODE_CHK;
        } else {
            type = CDATA_SECTION_NODE;
        }
        break;
    case CMT_NODE:
        if (checkCharData) {
            type = COMMENT_NODE_CHK;
        } else {
            type = COMMENT_NODE;
        }
        break;
    case PIC_NODE: 
        if (checkName && checkCharData) {
            type = PROCESSING_INSTRUCTION_NODE_CHK;
        } else if (checkName) {
            type = PROCESSING_INSTRUCTION_NODE_NAME_CHK;
        } else if (checkCharData) {
            type = PROCESSING_INSTRUCTION_NODE_VALUE_CHK;
        } else {
            type = PROCESSING_INSTRUCTION_NODE;
        }
        break;
    }
    
    nodeInfo->type = type;
    if (nodecmd) {
        nodeInfo->type *= -1; /* Signal this fact */
    }
    Tcl_CreateObjCommand(interp, Tcl_DStringValue(&cmdName), NodeObjCmd,
                         (ClientData)nodeInfo, NodeObjCmdDeleteProc);
    Tcl_DStringResult(interp, &cmdName);
    Tcl_DStringFree(&cmdName);

    return TCL_OK;

 usage:
    Tcl_AppendResult(interp, "dom createNodeCmd ?-returnNodeCmd?"
                     " nodeType cmdName", NULL);
    return TCL_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * nodecmd_appendFromScript --
 *
 *	This procedure implements the dom method appendFromScript.
 *      See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Appends new child nodes to node.
 *
 *----------------------------------------------------------------------
 */

int
nodecmd_appendFromScript (interp, node, cmdObj)
    Tcl_Interp *interp;                 /* Current interpreter. */
    domNode    *node;                   /* Parent dom node */
    Tcl_Obj    *cmdObj;                 /* Argument objects. */
{
    int ret;
    domNode *oldLastChild, *child, *nextChild;

    if (node->nodeType != ELEMENT_NODE) {
        Tcl_SetResult (interp, "NOT_AN_ELEMENT : can't append nodes", NULL);
        return TCL_ERROR;
    }
    
    oldLastChild = node->lastChild;

    StackPush((void *)node);
    Tcl_AllowExceptions(interp);
    ret = Tcl_EvalObj(interp, cmdObj);
    if (ret != TCL_ERROR) {
        Tcl_ResetResult(interp);
    }
    StackPop();

    if (ret == TCL_ERROR) {
        if (oldLastChild) {
            child = oldLastChild->nextSibling;
        } else {
            child = node->firstChild;
        }
        while (child) {
            nextChild = child->nextSibling;
            domFreeNode (child, NULL, NULL, 0);
            child = nextChild;
        }
        if (oldLastChild) {
            oldLastChild->nextSibling = NULL;
            node->lastChild = oldLastChild;
        } else {
            node->firstChild = NULL;
            node->lastChild = NULL;
        }
    }
            
    return (ret == TCL_BREAK) ? TCL_OK : ret;
}


/*
 *----------------------------------------------------------------------
 *
 * nodecmd_insertBeforeFromScript --
 *
 *	This procedure implements the dom method
 *	insertBeforeFromScript. See the user documentation for details
 *	on what it does.
 *
 *      This procedure is actually mostly a wrapper around
 *      nodecmd_appendFromScript.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Insert new child nodes before referenceChild to node.
 *
 *----------------------------------------------------------------------
 */

int
nodecmd_insertBeforeFromScript (interp, node, cmdObj, refChild)
    Tcl_Interp *interp;                 /* Current interpreter. */
    domNode    *node;                   /* Parent dom node */
    Tcl_Obj    *cmdObj;                 /* Argument objects. */
    domNode    *refChild;               /* Insert new childs before this
                                         * node; may be NULL */
{
    int      ret;
    domNode *storedLastChild, *n;

    if (!refChild) {
        return nodecmd_appendFromScript (interp, node, cmdObj);
    }
    
    if (node->nodeType != ELEMENT_NODE) {
        Tcl_SetResult (interp, "NOT_AN_ELEMENT : can't append nodes", NULL);
        return TCL_ERROR;
    }

    /* check, if node is in deed the parent of refChild */
    if (refChild->parentNode != node) {
        /* If node is the root node of a document and refChild
           is in deed a child of this node, then 
           refChild->parentNode will be NULL. In this case, we
           loop throu the childs of node, to see, if the refChild
           is valid. */
        Tcl_ResetResult (interp);
        if (node->ownerDocument->rootNode == node) {
            n = node->firstChild;
            while (n) {
                if (n == refChild) {
                    /* refChild is in deed a child of node */
                    break;
                }
                n = n->nextSibling;
            }
            if (!n) {
                Tcl_SetStringObj(Tcl_GetObjResult(interp), "NOT_FOUND_ERR",
                                 -1);
                return TCL_ERROR;
            }
        } else {
            Tcl_SetStringObj(Tcl_GetObjResult(interp), "NOT_FOUND_ERR", -1);
            return TCL_ERROR;
        }
    }

    storedLastChild = node->lastChild;
    if (refChild->previousSibling) {
        refChild->previousSibling->nextSibling = NULL;
        node->lastChild = refChild->previousSibling;
    } else {
        node->firstChild = NULL;
        node->lastChild = NULL;
    }
    ret = nodecmd_appendFromScript (interp, node, cmdObj);
    if (node->lastChild) {
        node->lastChild->nextSibling = refChild;
        refChild->previousSibling = node->lastChild;
    } else {
        node->firstChild = refChild;
    }
    node->lastChild = storedLastChild;
    
    return ret;
}


/*----------------------------------------------------------------------------
|   nodecmd_curentNode
|
\---------------------------------------------------------------------------*/

void *
nodecmd_currentNode(void)
{
    return StackTop();
}

/* EOF $RCSfile $ */

/* Emacs Setup Variables */
/* Local Variables:      */
/* mode: C               */
/* indent-tabs-mode: nil */
/* c-basic-offset: 4     */
/* End:                  */

