/*----------------------------------------------------------------------------
|   Copyright (c) 1999 Jochen Loewer (loewerj@hotmail.com)
+-----------------------------------------------------------------------------
|
|   $Id: tcldom.c,v 1.113 2007/12/25 23:19:02 rolf Exp $
|
|
|   A DOM implementation for Tcl using James Clark's expat XML parser
|
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
|   The Initial Developer of the Original Code is Jochen Loewer
|   Portions created by Jochen Loewer are Copyright (C) 1998, 1999
|   Jochen Loewer. All Rights Reserved.
|
|   Contributor(s):
|       Sept99  Carsten Zerbst    Added comment and processing instructions
|                                 nodes.
|       June00  Zoran Vasiljevic  Made thread-safe.
|       July00  Zoran Vasiljevic  Added "domNode appendFromScript"
|
|
|   written by Jochen Loewer
|   April, 1999
|
\---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
|   Includes
|
\---------------------------------------------------------------------------*/
#include <tcl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dom.h>
#include <domxpath.h>
#include <domxslt.h>
#include <xmlsimple.h>
#include <domhtml.h>
#include <nodecmd.h>
#include <tcldom.h>

/* #define DEBUG */
/*----------------------------------------------------------------------------
|   Debug Macros
|
\---------------------------------------------------------------------------*/
#ifdef DEBUG
# define DBG(x) x
#else
# define DBG(x) 
#endif


/*----------------------------------------------------------------------------
|   Macros
|
\---------------------------------------------------------------------------*/
#define XP_CHILD         0
#define XP_DESCENDANT    1
#define XP_ANCESTOR      2
#define XP_FSIBLING      3
#define XP_PSIBLING      4

#define MAX_REWRITE_ARGS 50

#define SetResult(str) Tcl_ResetResult(interp); \
                     Tcl_SetStringObj(Tcl_GetObjResult(interp), (str), -1)

#define SetIntResult(i) Tcl_ResetResult(interp); \
                     Tcl_SetIntObj(Tcl_GetObjResult(interp), (i))
                     
#define SetDoubleResult(d) Tcl_ResetResult(interp); \
                     Tcl_SetDoubleObj(Tcl_GetObjResult(interp), (d))

#define SetBooleanResult(i) Tcl_ResetResult(interp); \
                     Tcl_SetBooleanObj(Tcl_GetObjResult(interp), (i))
 
#define AppendResult(str) {Tcl_Obj *o = Tcl_GetObjResult(interp); \
                     if (Tcl_IsShared(o)) { \
                          o = Tcl_DuplicateObj(o); \
                          Tcl_SetObjResult(interp, o); \
                     } \
                     Tcl_AppendToObj(o, (str), -1);}

#define CheckArgs(min,max,n,msg) \
                     if ((objc < min) || (objc >max)) { \
                         Tcl_WrongNumArgs(interp, n, objv, msg); \
                         return TCL_ERROR; \
                     }
#define CheckName(interp, name, errText, isFQ) \
                     if (!TSD(dontCheckName)) { \
                         if (!tcldom_nameCheck(interp, name, errText, isFQ)) {\
                             return TCL_ERROR; \
                         } \
                     }

#define CheckPIName(interp, name) \
                     if (!TSD(dontCheckName)) { \
                         if (!tcldom_PINameCheck(interp, name)) {\
                             return TCL_ERROR; \
                         } \
                     }

#define CheckText(interp, text, errText) \
                     if (!TSD(dontCheckCharData)) { \
                         if (!tcldom_textCheck(interp, text, errText)) {\
                             return TCL_ERROR; \
                         } \
                     }

#define CheckComment(interp, text) \
                     if (!TSD(dontCheckCharData)) { \
                         if (!tcldom_commentCheck(interp, text)) {\
                             return TCL_ERROR; \
                         } \
                     }

#define CheckCDATA(interp, text) \
                     if (!TSD(dontCheckCharData)) { \
                         if (!tcldom_CDATACheck(interp, text)) {\
                             return TCL_ERROR; \
                         } \
                     }

#define CheckPIValue(interp, text) \
                     if (!TSD(dontCheckCharData)) { \
                         if (!tcldom_PIValueCheck(interp, text)) {\
                             return TCL_ERROR; \
                         } \
                     }

#if TclOnly8Bits
#define writeChars(var,chan,buf,len)  (chan) ? \
                     ((void)Tcl_Write ((chan), (buf), (len) )) : \
                     (Tcl_AppendToObj ((var), (buf), (len) ));
#else
#define writeChars(var,chan,buf,len)  (chan) ? \
                     ((void)Tcl_WriteChars ((chan), (buf), (len) )) : \
                     (Tcl_AppendToObj ((var), (buf), (len) ));
#endif

#define DOM_CREATECMDMODE_AUTO 0
#define DOM_CREATECMDMODE_CMDS 1
#define DOM_CREATECMDMODE_TOKENS 2

/*----------------------------------------------------------------------------
|   Module Globals
|
\---------------------------------------------------------------------------*/
#ifndef TCL_THREADS
    static TEncoding *Encoding_to_8bit      = NULL;
    static int        storeLineColumn       = 0;
    static int        dontCreateObjCommands = 0;
    static int        dontCheckCharData     = 0;
    static int        dontCheckName         = 0;
    static int        domCreateCmdMode      = 0;
#   define TSD(x)     x
#   define GetTcldomTSD()
#else
    typedef struct ThreadSpecificData {
        TEncoding *Encoding_to_8bit;
        int        storeLineColumn;
        int        dontCreateObjCommands;
        int        dontCheckCharData;
        int        dontCheckName;
        int        domCreateCmdMode;
    } ThreadSpecificData;
    static Tcl_ThreadDataKey dataKey;
    static Tcl_HashTable     sharedDocs;
    static Tcl_Mutex         tableMutex;
    static int               tcldomInitialized;
#   define TSD(x)            tsdPtr->x
#   define GetTcldomTSD()  ThreadSpecificData *tsdPtr = \
                                (ThreadSpecificData*)   \
                                Tcl_GetThreadData(      \
                                    &dataKey,           \
                                    sizeof(ThreadSpecificData));
#endif /* TCL_THREADS */

static char dom_usage[] =
    "Usage dom <subCommand> <args>, where subCommand can be:    \n"
    "    parse ?-keepEmpties? ?-channel <channel> ?-baseurl <baseurl>?  \n"
    "        ?-feedbackAfter <#Bytes>?                    \n"
    "        ?-externalentitycommand <cmd>?               \n"
    "        ?-useForeignDTD <boolean>?                   \n"
    "        ?-paramentityparsing <none|always|standalone>\n"
    "        ?-simple? ?-html? ?<xml>? ?<objVar>?         \n"
    "    createDocument docElemName ?objVar?              \n"
    "    createDocumentNS uri docElemName ?objVar?        \n"
    "    createDocumentNode ?objVar?                      \n"
    TDomThreaded(
    "    attachDocument domDoc ?objVar?                   \n"
    "    detachDocument domDoc                            \n"
    )
    "    createNodeCmd ?-returnNodeCmd? (element|comment|text|cdata|pi)Node cmdName \n"
    "    setResultEncoding ?encodingName?                 \n"
    "    setStoreLineColumn ?boolean?                     \n"
    "    setNameCheck ?boolean?                           \n"
    "    setTextCheck ?boolean?                           \n"
    "    setObjectCommands ?(automatic|token|command)?    \n"
    "    isCharData string                                \n"
    "    isComment string                                 \n"
    "    isCDATA string                                   \n"
    "    isPIValue string                                 \n"
    "    isName string                                    \n"
    "    isQName string                                   \n"
    "    isNCName string                                  \n"
    "    isPIName string                                  \n"
;

static char doc_usage[] =
    "Usage domDoc <method> <args>, where method can be:\n"
    "    documentElement ?objVar?                \n"
    "    getElementsByTagName name               \n"
    "    getElementsByTagNameNS uri localname    \n"
    "    createElement tagName ?objVar?          \n"
    "    createElementNS uri tagName ?objVar?    \n"
    "    createCDATASection data ?objVar?        \n"
    "    createTextNode text ?objVar?            \n"
    "    createComment text ?objVar?             \n"
    "    createProcessingInstruction target data ?objVar? \n"
    "    asXML ?-indent <none,0..8>? ?-channel <channel>? ?-escapeNonASCII? ?-escapeAllQuot? ?-doctypeDeclaration <boolean>?\n"
    "    asHTML ?-channel <channelId>? ?-escapeNonASCII? ?-htmlEntities?\n"
    "    asText                                  \n"
    "    getDefaultOutputMethod                  \n"
    "    publicId ?publicId?                     \n"
    "    systemId ?systemId?                     \n"
    "    internalSubset ?internalSubset?         \n"
    "    indent ?boolean?                        \n"
    "    omit-xml-declaration ?boolean?          \n"
    "    encoding ?value?                        \n"
    "    standalone ?boolean?                    \n"
    "    mediaType ?value?                       \n"
    "    delete                                  \n"
    "    xslt ?-parameters parameterList? ?-ignoreUndeclaredParameters? ?-xsltmessagecmd cmd? <xsltDocNode> ?objVar?\n"
    "    toXSLTcmd                               \n"
    "    cdataSectionElements (?URI:?localname|*) ?boolean?\n"
    "    normalize ?-forXPath?                   \n"
    "    nodeType                                \n"
    "    hasChildNodes                           \n"
    "    childNodes                              \n"
    "    firstChild ?nodeObjVar?                 \n"
    "    lastChild ?nodeObjVar?                  \n"
    "    appendChild new                         \n"
    "    insertBefore new ref                    \n"
    "    replaceChild new old                    \n"
    "    removeChild child                       \n"
    "    ownerDocument                           \n"
    "    getElementById id                       \n"
    "    baseURI ?URI?                           \n"
    "    appendFromList nestedList               \n"
    "    appendFromScript script                 \n"        
    "    insertBeforeFromScript script ref       \n"
    "    appendXML xmlString                     \n"
    "    selectNodesNamespaces ?prefixUriList?   \n" 
    "    selectNodes ?-namespaces prefixUriList? ?-cache <boolean>? xpathQuery ?typeVar? \n"
    "    renameNode <nodelist> <newName>         \n"
    "    deleteXPathCache ?xpathQuery?           \n"
    TDomThreaded(
    "    readlock                                \n"
    "    writelock                               \n"
    "    renumber                                \n"
    )
;

static char node_usage[] =
    "Usage nodeObj <method> <args>, where method can be:\n"
    "    nodeType                     \n"
    "    nodeName                     \n"
    "    nodeValue ?newValue?         \n"
    "    hasChildNodes                \n"
    "    childNodes                   \n"
    "    childNodesLive               \n"
    "    parentNode                   \n"
    "    firstChild ?nodeObjVar?      \n"
    "    lastChild ?nodeObjVar?       \n"
    "    nextSibling ?nodeObjVar?     \n"
    "    previousSibling ?nodeObjVar? \n"
    "    hasAttribute attrName        \n"
    "    getAttribute attrName ?defaultValue? \n"
    "    setAttribute attrName value ?attrName value ...? \n"
    "    removeAttribute attrName     \n"
    "    hasAttributeNS uri localName \n"
    "    getAttributeNS uri localName ?defaultValue? \n"
    "    setAttributeNS uri attrName value ?attrName value ...? \n"
    "    removeAttributeNS uri attrName \n"
    "    attributes ?attrNamePattern?   \n"
    "    appendChild new              \n"
    "    insertBefore new ref         \n"
    "    replaceChild new old         \n"
    "    removeChild child            \n"
    "    cloneNode ?-deep?            \n"
    "    ownerDocument                \n"
    "    getElementsByTagName name    \n"
    "    getElementsByTagNameNS uri localname \n"
    "    getElementById id            \n"
    "    find attrName attrValue ?nodeObjVar?   \n"
    "    child      number|all ?type? ?attrName attrValue? \n"
    "    descendant number|all ?type? ?attrName attrValue? \n"
    "    ancestor   number|all ?type? ?attrName attrValue? \n"
    "    fsibling   number|all ?type? ?attrName attrValue? \n"
    "    psibling   number|all ?type? ?attrName attrValue? \n"
    "    root ?nodeObjVar?            \n"
    "    target                       \n"
    "    data                         \n"
    "    text                         \n"
    "    prefix                       \n"
    "    namespaceURI                 \n"
    "    getBaseURI                   \n"
    "    baseURI ?URI?                \n"
    "    localName                    \n"
    "    delete                       \n"
    "    getLine                      \n"
    "    getColumn                    \n"
    "    @<attrName> ?defaultValue?   \n"
    "    asList                       \n"
    "    asXML ?-indent <none,0..8>? ?-channel <channel>? ?-escapeNonASCII? ?-escapeAllQuot? ?-doctypeDeclaration <boolean>?\n"
    "    asHTML ?-channel <channelId>? ?-escapeNonASCII? ?-htmlEntities?\n"
    "    asText                       \n"
    "    appendFromList nestedList    \n"
    "    appendFromScript script      \n"
    "    insertBeforeFromScript script ref \n"
    "    appendXML xmlString          \n"
    "    selectNodes ?-namespaces prefixUriList? ?-cache <boolean>? xpathQuery ?typeVar? \n"
    "    toXPath                      \n"
    "    disableOutputEscaping ?boolean? \n"
    "    precedes node                \n"
    "    normalize ?-forXPath?        \n"
    "    xslt ?-parameters parameterList? <xsltDocNode>\n"
    TDomThreaded(
    "    readlock                     \n"
    "    writelock                    \n"
    )
;

/*----------------------------------------------------------------------------
|   Types
|
\---------------------------------------------------------------------------*/

typedef struct XsltMsgCBInfo {
    Tcl_Interp * interp;
    Tcl_Obj    * msgcmd;
} XsltMsgCBInfo;

/*----------------------------------------------------------------------------
|   Prototypes for procedures defined later in this file:
|
\---------------------------------------------------------------------------*/

static Tcl_VarTraceProc  tcldom_docTrace;
static Tcl_VarTraceProc  tcldom_nodeTrace;

static Tcl_CmdDeleteProc tcldom_docCmdDeleteProc;
static Tcl_CmdDeleteProc tcldom_nodeCmdDeleteProc;

#ifdef TCL_THREADS

static int tcldom_EvalLocked(Tcl_Interp* interp, Tcl_Obj** objv,
                             domDocument* doc, int flag);

static int tcldom_RegisterDocShared(domDocument* doc);
static int tcldom_CheckDocShared(domDocument* doc);
static int tcldom_UnregisterDocShared(Tcl_Interp* interp, domDocument* doc);

/*----------------------------------------------------------------------------
|   tcldom_Finalize
|
|   Activated in application exit handler to delete shared document table
|   Table entries are deleted by the object command deletion callbacks,
|   so at this time, table should be empty. If not, we will leave some
|   memory leaks. This is not fatal, though: we're exiting the app anyway.
|   This is a private function to this file. 
\---------------------------------------------------------------------------*/

static void 
tcldom_Finalize(
    ClientData unused
)
{
    Tcl_MutexLock(&tableMutex);
    Tcl_DeleteHashTable(&sharedDocs);
    Tcl_MutexUnlock(&tableMutex);
}

/*----------------------------------------------------------------------------
|   tcldom_initialize
|   Activated at module load to initialize shared document table.
|   This is exported since we need it in tdominit.c.
\---------------------------------------------------------------------------*/

void tcldom_initialize()
{
    if (!tcldomInitialized) {
        Tcl_MutexLock(&tableMutex);
        Tcl_InitHashTable(&sharedDocs, TCL_ONE_WORD_KEYS);
        Tcl_CreateExitHandler(tcldom_Finalize, NULL);
        tcldomInitialized = 1;
        Tcl_MutexUnlock(&tableMutex);
    }
}

#endif /* TCL_THREADS */

/*----------------------------------------------------------------------------
|   tcldom_deleteNode
|
\---------------------------------------------------------------------------*/
static void
tcldom_deleteNode (
    domNode  * node,
    void     * clientData
)
{
    Tcl_Interp *interp = clientData;
    char        objCmdName[80];

    /* Try to delete the node object commands, ignore errors */
    if (node->nodeFlags & VISIBLE_IN_TCL) {
        NODE_CMD(objCmdName, node);
        Tcl_DeleteCommand(interp, objCmdName);
        node->nodeFlags &= ~VISIBLE_IN_TCL;
    }
}

/*----------------------------------------------------------------------------
|   tcldom_deleteDoc
|
\---------------------------------------------------------------------------*/
static
void tcldom_deleteDoc (
    Tcl_Interp  * interp,
    domDocument * doc
)
{
    int deleted = 1;

    TDomThreaded(deleted = tcldom_UnregisterDocShared(interp, doc));
    if (deleted) {
        domFreeDocument(doc, tcldom_deleteNode, interp);
    }
}

/*----------------------------------------------------------------------------
|   tcldom_docCmdDeleteProc
|
\---------------------------------------------------------------------------*/
static
void tcldom_docCmdDeleteProc(
    ClientData clientData
)
{
    domDeleteInfo *dinfo = (domDeleteInfo *)clientData;
    domDocument   *doc   = dinfo->document;
    char          *var   = dinfo->traceVarName;
    
    DBG(fprintf(stderr, "--> tcldom_docCmdDeleteProc doc %p\n", doc));
    if (var) {
        DBG(fprintf(stderr, "--> tcldom_docCmdDeleteProc calls "
                    "Tcl_UntraceVar for \"%s\"\n", var));
        Tcl_UntraceVar(dinfo->interp, var, TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                       tcldom_docTrace, clientData);
        FREE(var);
        dinfo->traceVarName = NULL;
    }

    tcldom_deleteDoc(dinfo->interp, doc);

    FREE((void*)dinfo);
}


/*----------------------------------------------------------------------------
|   tcldom_nodeCmdDeleteProc
|
\---------------------------------------------------------------------------*/
static
void tcldom_nodeCmdDeleteProc (
    ClientData  clientData
)
{
    domDeleteInfo *dinfo = (domDeleteInfo *)clientData;
    char          *var   = dinfo->traceVarName;

    DBG(fprintf (stderr, "--> tcldom_nodeCmdDeleteProc node %p\n", 
                 dinfo->node));

    if (var) {
         DBG(fprintf(stderr, "--> tcldom_nodeCmdDeleteProc calls "
                    "Tcl_UntraceVar for \"%s\"\n", var));
        Tcl_UntraceVar(dinfo->interp, var, 
                       TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                       tcldom_nodeTrace, clientData);
        FREE(var);
        dinfo->traceVarName = NULL;
    }
    FREE((void*)dinfo);
}


/*----------------------------------------------------------------------------
|   tcldom_docTrace
|
\---------------------------------------------------------------------------*/
static
char * tcldom_docTrace (
    ClientData    clientData,
    Tcl_Interp   *interp,
    CONST84 char *name1,
    CONST84 char *name2,
    int           flags
)
{
    domDeleteInfo *dinfo = (domDeleteInfo*) clientData;
    domDocument   *doc   = dinfo->document;
    char           objCmdName[80];

    DBG(fprintf(stderr, "--> tcldom_docTrace %x %p\n", flags, doc));

    if (flags & TCL_INTERP_DESTROYED) {
        return NULL;
    }
    if (flags & TCL_TRACE_WRITES) {
        return "var is read-only";
    }
    if (flags & TCL_TRACE_UNSETS) {
        DOC_CMD(objCmdName, doc);
        DBG(fprintf(stderr, "--> tcldom_docTrace delete doc %p\n", doc));
        Tcl_DeleteCommand(interp, objCmdName);
    }

    return NULL;
}


/*----------------------------------------------------------------------------
|   tcldom_nodeTrace
|
\---------------------------------------------------------------------------*/
static
char * tcldom_nodeTrace (
    ClientData    clientData,
    Tcl_Interp   *interp,
    CONST84 char *name1,
    CONST84 char *name2,
    int           flags
)
{
    domDeleteInfo *dinfo = (domDeleteInfo*)clientData;
    domNode       *node = dinfo->node;
    char           objCmdName[80];

    DBG(fprintf(stderr, "--> tcldom_nodeTrace %x %p\n", flags, node));

    if (flags & TCL_INTERP_DESTROYED) {
        return NULL;
    }
    if (flags & TCL_TRACE_WRITES) {
        return "var is read-only";
    }
    if (flags & TCL_TRACE_UNSETS) {
        NODE_CMD(objCmdName, node);
        DBG(fprintf(stderr, "--> tcldom_nodeTrace delete node %p\n", node));
        Tcl_UntraceVar(interp, name1, TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
                       tcldom_nodeTrace, clientData);
        Tcl_DeleteCommand(interp, objCmdName);
        node->nodeFlags &= ~VISIBLE_IN_TCL;
    }

    return NULL;
}


/*----------------------------------------------------------------------------
|   tcldom_createNodeObj
|
\---------------------------------------------------------------------------*/
void tcldom_createNodeObj (
    Tcl_Interp * interp,
    domNode    * node,
    char       * objCmdName
)
{
    GetTcldomTSD()

    NODE_CMD(objCmdName, node);

    if (TSD(dontCreateObjCommands) == 0) {
        DBG(fprintf(stderr,"--> creating node %s\n", objCmdName));
        
        Tcl_CreateObjCommand(interp, objCmdName,
                             (Tcl_ObjCmdProc *)  tcldom_NodeObjCmd,
                             (ClientData)        node,
                             (Tcl_CmdDeleteProc*)NULL);
        node->nodeFlags |= VISIBLE_IN_TCL;
    }
}

/*----------------------------------------------------------------------------
|   tcldom_returnNodeObj
|
\---------------------------------------------------------------------------*/
static
int tcldom_returnNodeObj (
    Tcl_Interp *interp,
    domNode    *node,
    int         setVariable,
    Tcl_Obj    *var_name
)
{
    char            objCmdName[80], *objVar;
    domDeleteInfo * dinfo;
    Tcl_CmdInfo     cmdInfo;

    GetTcldomTSD()

    if (node == NULL) {
        if (setVariable) {
            objVar = Tcl_GetString(var_name);
            Tcl_UnsetVar(interp, objVar, 0);
            Tcl_SetVar(interp, objVar, "", 0);
        }
        SetResult("");
        return TCL_OK;
    }
    tcldom_createNodeObj(interp, node, objCmdName);
    if (TSD(dontCreateObjCommands)) {
        if (setVariable) {
            objVar = Tcl_GetString(var_name);
            Tcl_SetVar(interp, objVar, objCmdName, 0);
        }
    } else {
        if (setVariable) {
            objVar = Tcl_GetString(var_name);
            Tcl_UnsetVar(interp, objVar, 0);
            Tcl_SetVar  (interp, objVar, objCmdName, 0);
            Tcl_GetCommandInfo(interp, objCmdName, &cmdInfo);
            if (0) {
                dinfo = (domDeleteInfo*)MALLOC(sizeof(domDeleteInfo));
                dinfo->interp       = interp;
                dinfo->node         = node;
                dinfo->traceVarName = NULL;
                Tcl_TraceVar(interp, objVar, 
                             TCL_TRACE_WRITES | TCL_TRACE_UNSETS,
                             (Tcl_VarTraceProc*)tcldom_nodeTrace,
                             (ClientData)dinfo);
                dinfo->traceVarName = tdomstrdup(objVar);

                /* Patch node object command to remove above trace 
                   on teardown */
                cmdInfo.deleteData = (ClientData)dinfo;
                cmdInfo.deleteProc = tcldom_nodeCmdDeleteProc;
                Tcl_SetCommandInfo(interp, objCmdName, &cmdInfo);
            }
        }
    }

    SetResult(objCmdName);
    return TCL_OK;
}

/*----------------------------------------------------------------------------
|   tcldom_returnDocumentObj
|
\---------------------------------------------------------------------------*/
int tcldom_returnDocumentObj (
    Tcl_Interp  *interp,
    domDocument *document,
    int          setVariable,
    Tcl_Obj     *var_name,
    int          trace,
    int          forOwnerDocument
)
{
    char           objCmdName[80], *objVar;
    domDeleteInfo *dinfo;
    Tcl_CmdInfo    cmd_info;

    GetTcldomTSD()

    if (document == NULL) {
        if (setVariable) {
            objVar = Tcl_GetString(var_name);
            Tcl_UnsetVar(interp, objVar, 0);
            Tcl_SetVar  (interp, objVar, "", 0);
        }
        SetResult("");
        return TCL_OK;
    }

    DOC_CMD(objCmdName, document);

    if (TSD(dontCreateObjCommands)) {
        if (setVariable) {
            objVar = Tcl_GetString(var_name);
            Tcl_SetVar(interp, objVar, objCmdName, 0);
        }
    } else {
        if (!Tcl_GetCommandInfo(interp, objCmdName, &cmd_info)) {
            dinfo = (domDeleteInfo*)MALLOC(sizeof(domDeleteInfo));
            dinfo->interp       = interp;
            dinfo->document     = document;
            dinfo->traceVarName = NULL;
            Tcl_CreateObjCommand(interp, objCmdName,
                                 (Tcl_ObjCmdProc *)  tcldom_DocObjCmd,
                                 (ClientData)        dinfo,
                                 (Tcl_CmdDeleteProc*)tcldom_docCmdDeleteProc);
        } else {
            dinfo = (domDeleteInfo*)cmd_info.objClientData;
        }
        if (setVariable) {
            objVar = Tcl_GetString(var_name);
            Tcl_UnsetVar(interp, objVar, 0);
            Tcl_SetVar  (interp, objVar, objCmdName, 0);
            if (trace) {
                dinfo->traceVarName = tdomstrdup(objVar);
                Tcl_TraceVar(interp,objVar,TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
                             (Tcl_VarTraceProc*)tcldom_docTrace,
                             (ClientData)dinfo);
            }
        }
    }
    
    if (!forOwnerDocument) {
        TDomThreaded(tcldom_RegisterDocShared(document));
    }
    SetResult(objCmdName);

    return TCL_OK;
}


/*----------------------------------------------------------------------------
|   tcldom_getElementsByTagName
|
\---------------------------------------------------------------------------*/
static int
tcldom_getElementsByTagName (
    Tcl_Interp *interp,
    char       *namePattern,
    domNode    *node,
    int         nsIndex,
    const char *uri
)
{
    int         result;
    domNode    *child;
    char        prefix[MAX_PREFIX_LEN], objCmdName[80];
    const char *localName;
    Tcl_Obj    *namePtr, *resultPtr;

    /* nsIndex == -1 ==> DOM 1 no NS i.e getElementsByTagName
       nsIndex != -1 are the NS aware cases
       nsIndex == -2 ==> more than one namespace in the document with the 
                         requested namespace, we have to strcmp the URI
                         with the namespace uri of every node
       nsIndex == -3 ==> NS wildcard '*'
       nsIndex == -4 ==> special handled case uri == "", i.e. all
                         nodes not in a namespace */

    while (node) {
        if (node->nodeType != ELEMENT_NODE) {
            node = node->nextSibling;
            continue;
        }
        if ( (nsIndex == -1)
             || (nsIndex == (int)node->namespace)
             || (nsIndex == -3)
             || (nsIndex == -2 
                 && node->namespace 
                 && strcmp(uri, domNamespaceURI (node)) == 0)
             || (nsIndex == -4
                 && (!node->namespace 
                     || strcmp ("", domNamespaceURI (node))==0)) )
        {
            if (nsIndex == -1) {
                localName = node->nodeName;
            } else {
                domSplitQName(node->nodeName, prefix, &localName);
            }
            if (Tcl_StringMatch(localName, namePattern)) {
                resultPtr = Tcl_GetObjResult(interp);
                tcldom_createNodeObj(interp, node, objCmdName);
                namePtr = Tcl_NewStringObj(objCmdName, -1);
                result = Tcl_ListObjAppendElement(interp, resultPtr, namePtr);
                if (result != TCL_OK) {
                    Tcl_DecrRefCount(namePtr);
                    return result;
                }
            }
        }

        /* recurs to the child nodes */
        child = node->firstChild;
        result = tcldom_getElementsByTagName(interp, namePattern, child,
                                             nsIndex, uri);
        if (result != TCL_OK) {
            return result;
        }
        node = node->nextSibling;
    }
    
    return TCL_OK;
}


/*----------------------------------------------------------------------------
|   tcldom_find
|
\---------------------------------------------------------------------------*/
static
domNode * tcldom_find (
    domNode    *node,
    char       *attrName,
    char       *attrVal,
    int         length
)
{
    domNode     *child, *result;
    domAttrNode *attrs;

    if (node->nodeType != ELEMENT_NODE) return NULL;

    attrs = node->firstAttr;
    while (attrs) {
        if ((strcmp(attrs->nodeName, attrName)==0) &&
            (length == attrs->valueLength)         &&
            (strncmp(attrs->nodeValue, attrVal, length)==0)) {

            return node;
        }
        attrs = attrs->nextSibling;
    }
    child = node->firstChild;
    while (child != NULL) {

        result = tcldom_find(child, attrName, attrVal, length);
        if (result != NULL) {
            return result;
        }
        child = child->nextSibling;
    }
    return NULL;
}


/*----------------------------------------------------------------------------
|   tcldom_xpointerAddCallback
|
\---------------------------------------------------------------------------*/
static
int tcldom_xpointerAddCallback (
    domNode    * node,
    void       * clientData
)
{
    Tcl_Interp * interp = (Tcl_Interp*)clientData;
    Tcl_Obj    * resultPtr = Tcl_GetObjResult(interp);
    Tcl_Obj    * namePtr;
    char         objCmdName[80];
    int          result;


    tcldom_createNodeObj(interp, node, objCmdName);
    namePtr = Tcl_NewStringObj(objCmdName, -1);
    result  = Tcl_ListObjAppendElement(interp, resultPtr, namePtr);
    if (result != TCL_OK) {
        Tcl_DecrRefCount(namePtr);
    }
    return result;
}


/*----------------------------------------------------------------------------
|   tcldom_xpointerSearch
|
\---------------------------------------------------------------------------*/
static
int tcldom_xpointerSearch (
    Tcl_Interp * interp,
    int          mode,
    domNode    * node,
    int          objc,
    Tcl_Obj    * CONST  objv[]
)
{
    char *str;
    int   i = 0;
    int   result = 0;
    int   all = 0;
    int   instance = 0;
    int   type = ELEMENT_NODE;
    char *element   = NULL;
    char *attrName  = NULL;
    char *attrValue = NULL;
    int   attrLen;


    str = Tcl_GetString(objv[2]);
    if (strcmp(str, "all")==0) {
        all = 1;
    } else {
        if (Tcl_GetIntFromObj(interp, objv[2], &instance) != TCL_OK) {
            SetResult( "instance must be integer or 'all'");
            return TCL_ERROR;
        }
    }
    if (objc > 3) {
        str = Tcl_GetString(objv[3]);
        if (*str == '#') {
            if (strcmp(str,"#text")==0) {
                type = TEXT_NODE;
            } else if (strcmp(str,"#cdata")==0) {
                type = CDATA_SECTION_NODE;
            } else if (strcmp(str,"#all")==0) {
                type = ALL_NODES;
            } else if (strcmp(str,"#element")==0) {
                type = ELEMENT_NODE;
            } else {
                SetResult( "wrong node type");
                return TCL_ERROR;
            }
        } else {
            element = str;
        }
    }
    if (objc >= 5) {
        if ((type != ELEMENT_NODE) && (type != ALL_NODES)) {
            SetResult( "Attribute search only for element nodes");
            return TCL_ERROR;
        }
        attrName  = Tcl_GetString(objv[4]);
        if (objc == 6) {
            attrValue = Tcl_GetStringFromObj(objv[5], &attrLen);
        } else {
            attrValue = "*";
            attrLen = 1;
        }
    }
    Tcl_ResetResult(interp);
    switch (mode) {
        case XP_CHILD:
            result = domXPointerChild
                (node, all, instance, type, element, attrName, 
                 attrValue, attrLen, tcldom_xpointerAddCallback, interp);
            break;

        case XP_DESCENDANT:
            result = domXPointerDescendant
                (node, all, instance, &i, type, element, attrName, 
                 attrValue, attrLen, tcldom_xpointerAddCallback, interp);
            break;

        case XP_ANCESTOR:
            result = domXPointerAncestor
                (node, all, instance, &i, type, element, attrName, 
                 attrValue, attrLen, tcldom_xpointerAddCallback, interp);
            break;

        case XP_FSIBLING:
            result = domXPointerXSibling
                (node, 1, all, instance, type, element, attrName, 
                 attrValue, attrLen, tcldom_xpointerAddCallback, interp);
            break;

        case XP_PSIBLING:
            result = domXPointerXSibling
                (node, 0, all, instance, type, element, attrName, 
                 attrValue,  attrLen, tcldom_xpointerAddCallback, interp);
            break;
    }
    if (result != 0) {
        return TCL_ERROR;
    }
    return TCL_OK;
}


/*----------------------------------------------------------------------------
|   tcldom_getNodeFromName
|
\---------------------------------------------------------------------------*/
domNode * tcldom_getNodeFromName (
    Tcl_Interp  *interp,
    char        *nodeName,
    char       **errMsg
)
{
    Tcl_CmdInfo  cmdInfo;
    domNode     *node = NULL;

    if (strncmp(nodeName, "domNode", 7)) {
        *errMsg = "parameter not a domNode!";
        return NULL;
    }
    if (sscanf(&nodeName[7], "%p", &node) != 1) {
        if (!Tcl_GetCommandInfo(interp, nodeName, &cmdInfo)) {
           *errMsg = "parameter not a domNode!";
           return NULL;
        }
        if (   (cmdInfo.isNativeObjectProc == 0)
            || (cmdInfo.objProc != (Tcl_ObjCmdProc*)tcldom_NodeObjCmd)) {
            *errMsg = "parameter not a domNode object command!";
            return NULL;
        }
        node = (domNode*)cmdInfo.objClientData;
    }

    return node;
}

/*----------------------------------------------------------------------------
|   tcldom_getDocumentFromName
|
\---------------------------------------------------------------------------*/
domDocument * tcldom_getDocumentFromName (
    Tcl_Interp  *interp,
    char        *docName,
    char       **errMsg
)
{
    Tcl_CmdInfo  cmdInfo;
    domDocument *doc = NULL;
    int          shared = 1;
    
    if (strncmp(docName, "domDoc", 6)) {
        *errMsg = "parameter not a domDoc!";
        return NULL;
    }
    if (sscanf(&docName[6], "%p", &doc) != 1) {
        if (!Tcl_GetCommandInfo(interp, docName, &cmdInfo)) {
            *errMsg = "parameter not a domDoc!";
            return NULL;
        }
        if (   (cmdInfo.isNativeObjectProc == 0)
            || (cmdInfo.objProc != (Tcl_ObjCmdProc*)tcldom_DocObjCmd)) {
            *errMsg = "parameter not a domDoc object command!";
            return NULL;
        }
        doc = ((domDeleteInfo*)cmdInfo.objClientData)->document;
    }

    TDomThreaded(shared = tcldom_CheckDocShared(doc));

    if (!shared) {
        *errMsg = "parameter not a shared domDoc!";
        return NULL;
    }

    return doc;
}


/*----------------------------------------------------------------------------
|   tcldom_appendXML
|
\---------------------------------------------------------------------------*/
int tcldom_appendXML (
    Tcl_Interp *interp,
    domNode    *node,
    Tcl_Obj    *obj
)
{
    char        *xml_string, *extResolver = NULL;
    int          xml_string_len;
    domDocument *doc;
    domNode     *nodeToAppend;
    XML_Parser   parser;

    GetTcldomTSD()

    xml_string = Tcl_GetStringFromObj(obj, &xml_string_len);

#ifdef TDOM_NO_EXPAT
    SetResult("tDOM was compiled without Expat!");
    return TCL_ERROR;
#else
    parser = XML_ParserCreate_MM(NULL, MEM_SUITE, NULL);

    if (node->ownerDocument->extResolver) {
        extResolver = tdomstrdup (node->ownerDocument->extResolver);
    }

    doc = domReadDocument(parser,
                          xml_string,
                          xml_string_len,
                          1,
                          TSD(Encoding_to_8bit),
                          TSD(storeLineColumn),
                          0,
                          NULL,
                          NULL,
                          extResolver,
                          0,
                          (int) XML_PARAM_ENTITY_PARSING_ALWAYS,
                          interp);
    if (doc == NULL) {
        char s[50];
        long byteIndex, i;

        Tcl_ResetResult(interp);
        sprintf(s, "%ld", XML_GetCurrentLineNumber(parser));
        Tcl_AppendResult(interp, "error \"",
                         XML_ErrorString(XML_GetErrorCode(parser)),
                         "\" at line ", s, " character ", NULL);
        sprintf(s, "%ld", XML_GetCurrentColumnNumber(parser));
        Tcl_AppendResult(interp, s, NULL);
        byteIndex = XML_GetCurrentByteIndex(parser);
        if (byteIndex != -1) {
             Tcl_AppendResult(interp, "\n\"", NULL);
             s[1] = '\0';
             for (i=-20; i < 40; i++) {
                 if ((byteIndex+i)>=0) {
                     if (xml_string[byteIndex+i]) {
                         s[0] = xml_string[byteIndex+i];
                         Tcl_AppendResult(interp, s, NULL);
                         if (i==0) {
                             Tcl_AppendResult(interp, " <--Error-- ", NULL);
                         }
                     } else {
                         break;
                     }
                 }
             }
             Tcl_AppendResult(interp, "\"",NULL);
        }
        XML_ParserFree(parser);
        return TCL_ERROR;
    }
    XML_ParserFree(parser);

    
    nodeToAppend = doc->rootNode->firstChild;
    while (nodeToAppend) {
        domAppendChild(node, nodeToAppend);
        nodeToAppend = nodeToAppend->nextSibling;
    }
    domFreeDocument(doc, NULL, NULL);

    return tcldom_returnNodeObj(interp, node, 0, NULL);
#endif
}


/*----------------------------------------------------------------------------
|   tcldom_xpathResultSet
|
\---------------------------------------------------------------------------*/
static
int tcldom_xpathResultSet (
    Tcl_Interp      *interp,
    xpathResultSet  *rs,
    Tcl_Obj         *type,
    Tcl_Obj         *value
)
{
    int          rc, i;
    Tcl_Obj     *namePtr, *objv[2];
    char         objCmdName[80];
    domAttrNode *attr;
    domNodeType  startType;
    int          mixedNodeSet;

    switch (rs->type) {
        case EmptyResult:
             Tcl_SetStringObj(type, "empty", -1);
             Tcl_SetStringObj(value, "", -1);
             break;

        case BoolResult:
             Tcl_SetStringObj(type, "bool", -1);
             Tcl_SetIntObj(value, rs->intvalue);
             break;

        case IntResult:
             Tcl_SetStringObj(type, "number", -1);
             Tcl_SetIntObj(value, rs->intvalue);
             break;

        case RealResult:
             Tcl_SetStringObj(type, "number", -1);
             Tcl_SetDoubleObj(value, rs->realvalue);
             break;
             
        case NaNResult:
             Tcl_SetStringObj(type, "number", -1);
             Tcl_SetStringObj(value, "NaN", -1);
             break;

        case InfResult:
             Tcl_SetStringObj(type, "number", -1);
             Tcl_SetStringObj(value, "Infinity", -1);
             break;

        case NInfResult:
             Tcl_SetStringObj(type, "number", -1);
             Tcl_SetStringObj(value, "-Infinity", -1);
             break;
             
        case StringResult:
             Tcl_SetStringObj(type, "string", -1);
             Tcl_SetStringObj(value, rs->string, rs->string_len);
             break;

        case xNodeSetResult:
             startType = rs->nodes[0]->nodeType;
             mixedNodeSet = 0;
             for (i=0; i<rs->nr_nodes; i++) {
                 if (rs->nodes[i]->nodeType != startType) mixedNodeSet = 1;

                 if (rs->nodes[i]->nodeType == ATTRIBUTE_NODE) {
                     attr = (domAttrNode*)rs->nodes[i];
                     objv[0] = Tcl_NewStringObj(attr->nodeName, -1);
                     objv[1] = Tcl_NewStringObj(attr->nodeValue,
                                                attr->valueLength);
                     namePtr = Tcl_NewListObj(2, objv);
                 } else {
                     tcldom_createNodeObj(interp, rs->nodes[i], objCmdName);
                     namePtr = Tcl_NewStringObj(objCmdName, -1);
                 }
                 rc = Tcl_ListObjAppendElement(interp, value, namePtr);
                 if (rc != TCL_OK) {
                     Tcl_DecrRefCount(namePtr);
                     return rc;
                 }
             }
             if (mixedNodeSet) {
                 Tcl_SetStringObj(type, "mixed", 5);
             } else {
                 if (startType == ATTRIBUTE_NODE)
                     Tcl_SetStringObj(type, "attrnodes",-1);
                 else
                     Tcl_SetStringObj(type, "nodes", 5);
             }
             break;

     }
     return TCL_OK;
}


/*----------------------------------------------------------------------------
|   tcldom_xpathFuncCallBack
|
\---------------------------------------------------------------------------*/
static
int tcldom_xpathFuncCallBack (
    void            *clientData,
    char            *functionName,
    domNode         *ctxNode,
    int              position,
    xpathResultSet  *nodeList,
    domNode         *exprContext,
    int              argc,
    xpathResultSets *args,
    xpathResultSet  *result,
    char           **errMsg
)
{
    Tcl_Interp  *interp = (Tcl_Interp*) clientData;
    char         tclxpathFuncName[200], objCmdName[80];
    char         *errStr, *typeStr, *nodeName;
    Tcl_Obj     *resultPtr, *objv[MAX_REWRITE_ARGS], *type, *value, *nodeObj;
    Tcl_CmdInfo  cmdInfo;
    int          objc, rc, i, errStrLen, listLen, intValue, res;
    double       doubleValue;
    domNode     *node;

    DBG(fprintf(stderr, "tcldom_xpathFuncCallBack functionName=%s "
                "position=%d argc=%d\n", functionName, position, argc);)

    sprintf (tclxpathFuncName, "::dom::xpathFunc::%s", functionName);
    DBG(fprintf(stderr, "testing %s\n", tclxpathFuncName);)
    rc = Tcl_GetCommandInfo (interp, tclxpathFuncName, &cmdInfo);
    if (!rc) {
        *errMsg = (char*)MALLOC (80 + strlen (functionName));
        strcpy (*errMsg, "Unknown XPath function: \"");
        strcat (*errMsg, functionName);
        strcat (*errMsg, "\"!");
        return XPATH_EVAL_ERR;
    }
    if (!cmdInfo.isNativeObjectProc) {
        *errMsg = (char*)tdomstrdup("can't access Tcl level method!");
        return XPATH_EVAL_ERR;
    }
    if ( (5+(2*argc)) >= MAX_REWRITE_ARGS) {
        *errMsg = (char*)tdomstrdup("too many args for Tcl level method!");
        return XPATH_EVAL_ERR;
    }
    objc = 0;
    objv[objc] = Tcl_NewStringObj(tclxpathFuncName, -1);
    Tcl_IncrRefCount(objv[objc++]);
    tcldom_createNodeObj(interp, ctxNode, objCmdName);
    objv[objc] = Tcl_NewStringObj(objCmdName, -1);
    Tcl_IncrRefCount(objv[objc++]);

    objv[objc] = Tcl_NewIntObj(position);
    Tcl_IncrRefCount(objv[objc++]);

    type  = Tcl_NewObj();
    value = Tcl_NewObj();
    tcldom_xpathResultSet(interp, nodeList, type, value);
    objv[objc] = type;
    Tcl_IncrRefCount(objv[objc++]);
    objv[objc] = value;
    Tcl_IncrRefCount(objv[objc++]);

    for (i=0; i<argc; i++) {
        type  = Tcl_NewObj();
        value = Tcl_NewObj();
        tcldom_xpathResultSet(interp, args[i], type, value);
        objv[objc] = type;
        Tcl_IncrRefCount(objv[objc++]);
        objv[objc] = value;
        Tcl_IncrRefCount(objv[objc++]);
    }
    rc = (cmdInfo.objProc(cmdInfo.objClientData, interp, objc, objv));
    if (rc == TCL_OK) {
        xpathRSInit(result);
        resultPtr = Tcl_GetObjResult(interp);
        rc = Tcl_ListObjLength(interp, resultPtr, &listLen);
        if (rc == TCL_OK) {
            if (listLen == 1) {
                rsSetString(result, Tcl_GetString(resultPtr));
                res = XPATH_OK;
                goto funcCallCleanup;
            }
            if (listLen != 2) {
                *errMsg = (char*)tdomstrdup("wrong return tuple; "
                                            "must be {type value}!");
                res = XPATH_EVAL_ERR;
                goto funcCallCleanup;
            }
            rc = Tcl_ListObjIndex(interp, resultPtr, 0, &type);
            rc = Tcl_ListObjIndex(interp, resultPtr, 1, &value);
            typeStr = Tcl_GetString(type);
            if (strcmp(typeStr, "bool")==0) {
                rc = Tcl_GetBooleanFromObj(interp, value, &intValue);
                rsSetBool(result, intValue );
            } else
            if (strcmp(typeStr, "number")==0) {
                rc = Tcl_GetIntFromObj(interp, value, &intValue);
                if (rc == TCL_OK) {
                    rsSetInt(result, intValue);
                } else {
                    rc = Tcl_GetDoubleFromObj(interp, value, &doubleValue);
                    rsSetReal(result, doubleValue);
                }
            } else
            if (strcmp(typeStr, "string")==0) {
                rsSetString(result, Tcl_GetString(value));
            } else
            if (strcmp(typeStr, "nodes")==0) {
                rc = Tcl_ListObjLength(interp, value, &listLen);
                if (rc != TCL_OK) {
                    *errMsg = tdomstrdup("value not a node list!");
                    res = XPATH_EVAL_ERR;
                    goto funcCallCleanup;
                }
                for (i=0; i < listLen; i++) {
                    rc = Tcl_ListObjIndex(interp, value, i, &nodeObj);
                    nodeName = Tcl_GetString(nodeObj);
                    node = tcldom_getNodeFromName(interp, nodeName, &errStr);
                    if (node == NULL) {
                        *errMsg = tdomstrdup(errStr);
                        res = XPATH_EVAL_ERR;
                        goto funcCallCleanup;
                    }
                    rsAddNode(result, node);
                }
                sortByDocOrder(result);
            } else
            if (strcmp(typeStr, "attrnodes")==0) {
                *errMsg = tdomstrdup("attrnodes not implemented yet!");
                res = XPATH_EVAL_ERR;
                goto funcCallCleanup;
            } else
            if (strcmp(typeStr, "attrvalues")==0) {
                rsSetString(result, Tcl_GetString(value));
            } else {
                *errMsg = (char*)MALLOC (80 + strlen (typeStr)
                                         + strlen (functionName));
                strcpy(*errMsg, "Unknown type of return value \"");
                strcat(*errMsg, typeStr);
                strcat(*errMsg, "\" from tcl coded XPath function \"");
                strcat(*errMsg, functionName);
                strcat(*errMsg, "\"!");
                res = XPATH_EVAL_ERR;
                goto funcCallCleanup;
            }
        } else {
            DBG(fprintf(stderr, "ListObjLength != TCL_OK "
                        "--> returning XPATH_EVAL_ERR \n");)
            res = XPATH_EVAL_ERR;
            goto funcCallCleanup;
        }
        Tcl_ResetResult(interp);
        res = XPATH_OK;
    } else {
        errStr = Tcl_GetStringFromObj( Tcl_GetObjResult(interp), &errStrLen);
        *errMsg = (char*)MALLOC(120+strlen(functionName) + errStrLen);
        strcpy(*errMsg, "Tcl error while executing XPATH extension function '");
        strcat(*errMsg, functionName );
        strcat(*errMsg, "':\n" );
        strcat(*errMsg, errStr);
        Tcl_ResetResult(interp);
        DBG(fprintf(stderr, "returning XPATH_EVAL_ERR \n");)
        res = XPATH_EVAL_ERR;
    }
 funcCallCleanup:
    for (i = 0; i < objc; i++) {
        Tcl_DecrRefCount(objv[i]);
    }
    return res;
}

/*----------------------------------------------------------------------------
|   tcldom_xsltMsgCB
|
\---------------------------------------------------------------------------*/
static
void tcldom_xsltMsgCB (
    void *clientData,
    char *str,
    int   length,
    int   terminate
    )
{
    XsltMsgCBInfo *msgCBInfo = (XsltMsgCBInfo *)clientData;
    Tcl_Obj       *cmdPtr;

    if (msgCBInfo->msgcmd == NULL) {
        return;
    }
    
    cmdPtr = Tcl_DuplicateObj(msgCBInfo->msgcmd);
    Tcl_IncrRefCount(cmdPtr);
    if (Tcl_ListObjAppendElement(msgCBInfo->interp, cmdPtr, 
                                 Tcl_NewStringObj(str, length)) != TCL_OK) {
        Tcl_DecrRefCount(cmdPtr);
        return;
    }
    if (terminate) {
        Tcl_ListObjAppendElement(msgCBInfo->interp, cmdPtr,
                                 Tcl_NewBooleanObj(1));
    } else {
        Tcl_ListObjAppendElement(msgCBInfo->interp, cmdPtr,
                                 Tcl_NewBooleanObj(0));
    }
    Tcl_GlobalEvalObj(msgCBInfo->interp, cmdPtr);
    Tcl_DecrRefCount(cmdPtr);
    return;
}

/*----------------------------------------------------------------------------
|   tcldom_xpathResolveVar
|
\---------------------------------------------------------------------------*/
static
char * tcldom_xpathResolveVar (
    void  *clientData,
    char  *strToParse,
    int   *offset,
    char **errMsg
    )
{
    CONST char *varValue;
    CONST char *termPtr;
    Tcl_Interp *interp = (Tcl_Interp *) clientData;
    
    *offset = 0;
    varValue = Tcl_ParseVar(interp, strToParse, &termPtr);
    if (varValue) {
        *offset = termPtr - strToParse;
        /* If strToParse start with a single '$' without a following
         * var name (according to tcl var name rules), Tcl_ParseVar()
         * doesn't report a parsing error but returns just a pointer
         * to a static string "$". */ 
        if (*offset == 1) {
            *errMsg = tdomstrdup ("Missing var name after '$'.");
            varValue = NULL;
        }
    } else {
        *errMsg = tdomstrdup (Tcl_GetStringResult(interp));
    }
    Tcl_ResetResult (interp);
    return (char*)varValue;
}

/*----------------------------------------------------------------------------
|   tcldom_selectNodes
|
\---------------------------------------------------------------------------*/
static
int tcldom_selectNodes (
    Tcl_Interp *interp,
    domNode    *node,
    int         objc,
    Tcl_Obj    *CONST objv[]
)
{
    char          *xpathQuery, *typeVar, *option;
    char          *errMsg = NULL, **mappings = NULL;
    int            rc, i, len, optionIndex, localmapping, cache = 0;
    xpathResultSet rs;
    Tcl_Obj       *type, *objPtr;
    xpathCBs       cbs;
    xpathParseVarCB parseVarCB;

    static CONST84 char *selectNodesOptions[] = {
        "-namespaces", "-cache", NULL
    };
    enum selectNodesOption {
        o_namespaces, o_cache
    };

    if (objc < 2) {
        SetResult("Wrong # of arguments.");
        return TCL_ERROR;
    }
    while (objc > 2) {
        option = Tcl_GetString (objv[1]);
        if (option[0] != '-') {
            break;
        }
        if (Tcl_GetIndexFromObj (NULL, objv[1], selectNodesOptions, "option",
                                 0, &optionIndex) != TCL_OK) {
            break;
        }
        switch ((enum selectNodesOption) optionIndex) {
        case o_namespaces:
            rc = Tcl_ListObjLength (interp, objv[2], &len);
            if (rc != TCL_OK || (len % 2) != 0) {
                SetResult ("The \"-namespaces\" option requires a 'prefix"
                           " namespace' pairs list as argument");
                return TCL_ERROR;
            }
            if (mappings) {
                FREE (mappings);
            }
            mappings = MALLOC (sizeof (char *) * (len + 1));
            for (i = 0; i < len; i++) {
                Tcl_ListObjIndex (interp, objv[2], i, &objPtr);
                /* The prefixMappings array is only used at xpath expr
                   compile time. (And every needed info is strdup'ed.)
                   So, we don't need to fiddle around with the refcounts
                   of the prefixMappings list members.
                   If we, at some day, allow to evaluate tcl scripts during
                   xpath lexing/parsing, this must be revisited. */
                mappings[i] = Tcl_GetString (objPtr);
            }
            mappings[len] = NULL;
            objc -= 2;
            objv += 2;
            break;

        case o_cache:
            if (Tcl_GetBooleanFromObj (interp, objv[2], &cache) != TCL_OK) {
                return TCL_ERROR;
            }
            objc -= 2;
            objv += 2;
            break;
            
        default:
            Tcl_ResetResult (interp);
            Tcl_AppendResult (interp, "bad option \"", 
                              Tcl_GetString (objv[1]), "\"; must be "
                              "-namespaces", NULL);
            return TCL_ERROR;
        }
    }
    if (objc != 2 && objc != 3) {
        if (mappings) {
            FREE (mappings);
        }
        SetResult("Wrong # of arguments.");
        return TCL_ERROR;
    }

    xpathQuery = Tcl_GetString(objv[1]);

    xpathRSInit(&rs);

    cbs.funcCB         = tcldom_xpathFuncCallBack;
    cbs.funcClientData = interp;
    cbs.varCB          = NULL;
    cbs.varClientData  = NULL;

    parseVarCB.parseVarCB         = tcldom_xpathResolveVar;
    parseVarCB.parseVarClientData = interp;
    
    if (mappings == NULL) {
        mappings = node->ownerDocument->prefixNSMappings;
        localmapping = 0;
    } else {
        localmapping = 1;
    }
    if (cache) {
        if (!node->ownerDocument->xpathCache) {
            node->ownerDocument->xpathCache = MALLOC (sizeof (Tcl_HashTable));
            Tcl_InitHashTable (node->ownerDocument->xpathCache,
                               TCL_STRING_KEYS);
        }
        rc = xpathEval (node, node, xpathQuery, mappings, &cbs, &parseVarCB,
                        node->ownerDocument->xpathCache, &errMsg, &rs);
    } else {
        rc = xpathEval (node, node, xpathQuery, mappings, &cbs, &parseVarCB,
                        NULL, &errMsg, &rs);
    }

    if (rc != XPATH_OK) {
        xpathRSFree(&rs);
        SetResult(errMsg);
        DBG(fprintf(stderr, "errMsg = %s \n", errMsg);)
        if (errMsg) {
            FREE(errMsg);
        }
        if (localmapping && mappings) {
            FREE(mappings);
        }
        return TCL_ERROR;
    }
    if (errMsg) {
        FREE(errMsg);
    }
    typeVar = NULL;
    if (objc > 2) {
        typeVar = Tcl_GetString(objv[2]);
    }
    type = Tcl_NewObj();
    Tcl_IncrRefCount(type);
    DBG(fprintf(stderr, "before tcldom_xpathResultSet \n");)
    tcldom_xpathResultSet(interp, &rs, type, Tcl_GetObjResult(interp));
    DBG(fprintf(stderr, "after tcldom_xpathResultSet \n");)
    if (typeVar) {
        Tcl_SetVar(interp,typeVar, Tcl_GetString(type), 0);
    }
    Tcl_DecrRefCount(type);

    xpathRSFree( &rs );
    if (localmapping && mappings) {
        FREE (mappings);
    }
    return TCL_OK;
}

/*----------------------------------------------------------------------------
|   tcldom_nameCheck
|
\---------------------------------------------------------------------------*/
int tcldom_nameCheck (
    Tcl_Interp *interp,
    char       *name,
    char       *nameType,
    int         isFQName
)
{
    int         result;

    if (isFQName) {
        result = domIsQNAME (name);
    } else {
        result = domIsNAME (name);
    }
    if (!result) {
        Tcl_ResetResult (interp);
        Tcl_AppendResult (interp, "Invalid ", nameType, " name '", name, "'",
                          (char *) NULL);
        return 0;
    }
    return 1;
}

/*----------------------------------------------------------------------------
|   tcldom_PINameCheck
|
\---------------------------------------------------------------------------*/
int tcldom_PINameCheck (
    Tcl_Interp *interp,
    char       *name
)
{
    /* XML rec, production 17 */
    if (!domIsPINAME (name)) {
        Tcl_ResetResult (interp);
        Tcl_AppendResult (interp, "Invalid processing instruction name '", 
                          name, "'", NULL);
        return 0;
    }
    return 1;
}

/*----------------------------------------------------------------------------
|   tcldom_textCheck
|
\---------------------------------------------------------------------------*/
int tcldom_textCheck (
    Tcl_Interp *interp,
    char       *text,
    char       *errText
)
{
    if (!domIsChar (text)) {
        Tcl_ResetResult (interp);
        Tcl_AppendResult (interp, "Invalid ", errText, " value '", text, "'",
                          (char *) NULL);
        return 0;
    }
    return 1;
}


/*----------------------------------------------------------------------------
|   tcldom_commentCheck
|
\---------------------------------------------------------------------------*/
int tcldom_commentCheck (
    Tcl_Interp *interp,
    char       *text
)
{
    if (!domIsComment (text)) {
        Tcl_ResetResult (interp);
        Tcl_AppendResult (interp, "Invalid comment value '", text, "'",
                          (char *) NULL);
        return 0;
    }
    return 1;
}

/*----------------------------------------------------------------------------
|   tcldom_CDATACheck
|
\---------------------------------------------------------------------------*/
int tcldom_CDATACheck (
    Tcl_Interp *interp,
    char       *text
)
{
    if (!domIsCDATA (text)) {
        Tcl_ResetResult (interp);
        Tcl_AppendResult (interp, "Invalid CDATA section value '", text, "'",
                          (char *) NULL);
        return 0;
    }
    return 1;
}

/*----------------------------------------------------------------------------
|   tcldom_PIValueCheck
|
\---------------------------------------------------------------------------*/
int tcldom_PIValueCheck (
    Tcl_Interp *interp,
    char       *text
)
{
    if (!domIsPIValue (text)) {
        Tcl_ResetResult (interp);
        Tcl_AppendResult (interp, "Invalid processing instruction value '", 
                          text, "'", (char *) NULL);
        return 0;
    }
    return 1;
}

/*----------------------------------------------------------------------------
|   tcldom_appendFromTclList
|
\---------------------------------------------------------------------------*/
static
int tcldom_appendFromTclList (
    Tcl_Interp *interp,
    domNode    *node,
    Tcl_Obj    *obj
)
{
    int      i, rc, length, valueLength, attrLength, attrValueLength;
    int      childListLength;
    Tcl_Obj *lnode, *tagNameObj, *piNameObj, *valueObj,
            *attrListObj, *attrObj, *childListObj, *childObj;
    char    *tag_name, *pi_name, *value, *attrName, *attrValue;
    domNode *newnode;

    GetTcldomTSD();

    /*------------------------------------------------------------------------
    |   check format of Tcl list node
    \-----------------------------------------------------------------------*/
    lnode = obj;
    if ((rc = Tcl_ListObjLength(interp, lnode, &length)) != TCL_OK) {
        return rc;
    }
    if ((length != 3) && (length != 2)) {
        SetResult( "invalid node list format!");
        return TCL_ERROR;
    }

    /*------------------------------------------------------------------------
    |   create node
    \-----------------------------------------------------------------------*/
    if ((rc = Tcl_ListObjIndex(interp, lnode, 0, &tagNameObj)) != TCL_OK) {
        return rc;
    }
    tag_name = Tcl_GetString(tagNameObj);

    if (   (strcmp(tag_name,"#cdata")==0) 
        || (strcmp(tag_name,"#text")==0)
        || (strcmp(tag_name,"#comment")==0) ) {
        if (length != 2) {
            SetResult( "invalid text or comment node list format!");
            return TCL_ERROR;
        }
        /*--------------------------------------------------------------------
        |   create text node
        \-------------------------------------------------------------------*/
        if ((rc = Tcl_ListObjIndex(interp, lnode, 1, &valueObj)) != TCL_OK) {
            return rc;
        }
        value = Tcl_GetStringFromObj(valueObj, &valueLength);
        if (strcmp(tag_name, "#text")==0) {
            CheckText (interp, value, "text");
            newnode = (domNode*)domNewTextNode(node->ownerDocument, value,
                                               valueLength, TEXT_NODE);
        } else if (strcmp(tag_name, "#comment")==0) {
            CheckComment (interp, value);
            newnode = (domNode*)domNewTextNode(node->ownerDocument, value,
                                               valueLength, COMMENT_NODE);
        } else {
            CheckCDATA (interp, value);
            newnode = (domNode*)domNewTextNode(node->ownerDocument, value,
                                              valueLength, CDATA_SECTION_NODE);
        }
        domAppendChild(node, newnode);
        return TCL_OK;
    }

    if (strcmp(tag_name,"#pi")==0) {
        if (length != 3) {
            SetResult( "invalid PI node list format!");
            return TCL_ERROR;
        }
        /*--------------------------------------------------------------------
        |   create processing instruction node
        \-------------------------------------------------------------------*/
        if ((rc = Tcl_ListObjIndex(interp, lnode, 1, &piNameObj)) != TCL_OK) {
            return rc;
        }
        if ((rc = Tcl_ListObjIndex(interp, lnode, 2, &valueObj)) != TCL_OK) {
            return rc;
        }
        pi_name = Tcl_GetStringFromObj(piNameObj, &length);
        CheckPIName (interp, pi_name);
        value   = Tcl_GetStringFromObj(valueObj, &valueLength);
        CheckPIValue (interp, value);
        newnode = (domNode*)domNewProcessingInstructionNode
            (node->ownerDocument, pi_name, length, value, valueLength);
        
        domAppendChild(node, newnode);
        return TCL_OK;
    }
    
    /*------------------------------------------------------------------------
    |   create element node
    \-----------------------------------------------------------------------*/
    if (length != 3) {
        SetResult("invalid element node list format!");
        return TCL_ERROR;
    }
    CheckName (interp, tag_name, "tag", 0);
    newnode = domNewElementNode(node->ownerDocument, tag_name, ELEMENT_NODE);
    domAppendChild(node, newnode);

    /*------------------------------------------------------------------------
    |   create atributes
    \-----------------------------------------------------------------------*/
    if ((rc = Tcl_ListObjIndex(interp, lnode, 1, &attrListObj)) != TCL_OK) {
        return rc;
    }
    if ((rc = Tcl_ListObjLength(interp, attrListObj, &attrLength))
        != TCL_OK) {
        return rc;
    }
    if (attrLength % 2) {
        SetResult("invalid attributes list format!");
        return TCL_ERROR;
    }
    for (i=0; i<attrLength; i++) {
        
        if ((rc = Tcl_ListObjIndex(interp, attrListObj, i, &attrObj))
            != TCL_OK) {
            return rc;
        }
        attrName = Tcl_GetString(attrObj);
        CheckName (interp, attrName, "attribute", 0);
        i++;
        
        if ((rc = Tcl_ListObjIndex(interp, attrListObj, i, &attrObj))
            != TCL_OK) {
            return rc;
        }
        attrValue = Tcl_GetStringFromObj(attrObj, &attrValueLength);
        CheckText (interp, attrValue, "attribute");
        domSetAttribute(newnode, attrName, attrValue);
    }

    /*------------------------------------------------------------------------
    |   add child nodes
    \-----------------------------------------------------------------------*/
    if ((rc = Tcl_ListObjIndex(interp, lnode, 2, &childListObj)) 
        != TCL_OK) {
        return rc;
    }
    if ((rc = Tcl_ListObjLength(interp, childListObj, &childListLength)) 
        != TCL_OK) {
        return rc;
    }
    for (i=0; i<childListLength; i++) {
        if ((rc = Tcl_ListObjIndex(interp, childListObj, i, &childObj))
            != TCL_OK) {
            return rc;
        }
        if ((rc = tcldom_appendFromTclList(interp, newnode, childObj))
            != TCL_OK) {
            return rc;
        }
    }
    return tcldom_returnNodeObj(interp, node, 0, NULL);
}


/*----------------------------------------------------------------------------
|   tcldom_treeAsTclList
|
\---------------------------------------------------------------------------*/
static
Tcl_Obj * tcldom_treeAsTclList (
    Tcl_Interp *interp,
    domNode    *node
)
{
    Tcl_Obj *name, *value;
    Tcl_Obj *attrsList, *attrName, *attrValue;
    Tcl_Obj *childList;
    Tcl_Obj *objv[4];
    int     result;
    domNode     *child;
    domAttrNode *attrs;



    if (   (node->nodeType == TEXT_NODE) 
        || (node->nodeType == CDATA_SECTION_NODE)) {

        value   = Tcl_NewStringObj(((domTextNode*)node)->nodeValue,
                                     ((domTextNode*)node)->valueLength);
        objv[0] = Tcl_NewStringObj("#text", -1);
        objv[1] = value;
        return Tcl_NewListObj(2, objv);
    }

    if (node->nodeType == COMMENT_NODE) {
        value   = Tcl_NewStringObj(((domTextNode*)node)->nodeValue,
                                     ((domTextNode*)node)->valueLength);
        objv[0] = Tcl_NewStringObj("#comment", -1);
        objv[1] = value;
        return Tcl_NewListObj(2, objv);
    }

    if (node->nodeType == PROCESSING_INSTRUCTION_NODE) {
        domProcessingInstructionNode *dpn;
        dpn = (domProcessingInstructionNode *)node;
        name    = Tcl_NewStringObj(dpn->targetValue, dpn->targetLength);
        value   = Tcl_NewStringObj(dpn->dataValue, dpn->dataLength);
        objv[0] = Tcl_NewStringObj("#pi", -1);
        objv[1] = name;
        objv[2] = value;
        return Tcl_NewListObj(3, objv);
    }

    name = Tcl_NewStringObj(node->nodeName, -1);

    attrsList = Tcl_NewListObj(0, NULL);
    attrs = node->firstAttr;
    while (attrs) {
        attrName = Tcl_NewStringObj(attrs->nodeName, -1);
        attrValue = Tcl_NewStringObj(attrs->nodeValue, attrs->valueLength);
        Tcl_ListObjAppendElement(interp, attrsList, attrName);
        Tcl_ListObjAppendElement(interp, attrsList, attrValue);
        attrs = attrs->nextSibling;
    }

    childList = Tcl_NewListObj(0, NULL);
    if (node->nodeType == ELEMENT_NODE) {
        child = node->firstChild;
        while (child != NULL) {
            result = Tcl_ListObjAppendElement
                (interp, childList, tcldom_treeAsTclList(interp, child));
            if (result != TCL_OK) {
                return NULL;
            }
            child = child->nextSibling;
        }
    }

    objv[0] = name;
    objv[1] = attrsList;
    objv[2] = childList;

    return Tcl_NewListObj(3, objv);
}


/*----------------------------------------------------------------------------
|   tcldom_AppendEscaped
|
\---------------------------------------------------------------------------*/
static
void tcldom_AppendEscaped (
    Tcl_Obj    *xmlString,
    Tcl_Channel chan,
    char       *value,
    int         value_length,
    int         forAttr,
    int         escapeNonASCII,
    int         htmlEntities,
    int         escapeAllQuot
)
{
#define APESC_BUF_SIZE 512
#define AP(c)  *b++ = c;
#define AE(s)  pc1 = s; while(*pc1) *b++ = *pc1++;
    char  buf[APESC_BUF_SIZE+80], *b, *bLimit,  *pc, *pc1, *pEnd, charRef[10];
    int   charDone, i;
#if !TclOnly8Bits
    int   clen = 0;
    Tcl_UniChar uniChar;
#endif

    b = buf;
    bLimit = b + APESC_BUF_SIZE;
    pc = pEnd = value;
    if (value_length != -1) {
        pEnd = pc + value_length;
    }
    while (   (value_length == -1 && *pc)
           || (value_length != -1 && pc != pEnd)
    ) {
        if ((forAttr || escapeAllQuot) && (*pc == '"')) { 
            AP('&') AP('q') AP('u') AP('o') AP('t') AP(';')
        } else
        if (*pc == '&') { AP('&') AP('a') AP('m') AP('p') AP(';')
        } else
        if (*pc == '<') { AP('&') AP('l') AP('t') AP(';')
        } else
        if (*pc == '>') { AP('&') AP('g') AP('t') AP(';')
        } else
        if (forAttr && (*pc == '\n')) { AP('&') AP('#') AP('x') AP('A') AP(';')
        } else
        {
            charDone = 0;
            if (htmlEntities) {
                charDone = 1;
#if TclOnly8Bits
                switch ((unsigned int)*pc)
#else           
                Tcl_UtfToUniChar(pc, &uniChar);
                switch (uniChar) 
#endif
                {
                case 0240: AE("&nbsp;");    break;     
                case 0241: AE("&iexcl;");   break;    
                case 0242: AE("&cent;");    break;     
                case 0243: AE("&pound;");   break;    
                case 0244: AE("&curren;");  break;   
                case 0245: AE("&yen;");     break;      
                case 0246: AE("&brvbar;");  break;   
                case 0247: AE("&sect;");    break;     
                case 0250: AE("&uml;");     break;      
                case 0251: AE("&copy;");    break;     
                case 0252: AE("&ordf;");    break;     
                case 0253: AE("&laquo;");   break;    
                case 0254: AE("&not;");     break;      
                case 0255: AE("&shy;");     break;      
                case 0256: AE("&reg;");     break;      
                case 0257: AE("&macr;");    break;     
                case 0260: AE("&deg;");     break;      
                case 0261: AE("&plusmn;");  break;   
                case 0262: AE("&sup2;");    break;     
                case 0263: AE("&sup3;");    break;     
                case 0264: AE("&acute;");   break;    
                case 0265: AE("&micro;");   break;    
                case 0266: AE("&para;");    break;     
                case 0267: AE("&middot;");  break;   
                case 0270: AE("&cedil;");   break;    
                case 0271: AE("&sup1;");    break;     
                case 0272: AE("&ordm;");    break;     
                case 0273: AE("&raquo;");   break;    
                case 0274: AE("&frac14;");  break;   
                case 0275: AE("&frac12;");  break;   
                case 0276: AE("&frac34;");  break;   
                case 0277: AE("&iquest;");  break;   
                case 0300: AE("&Agrave;");  break;   
                case 0301: AE("&Aacute;");  break;   
                case 0302: AE("&Acirc;");   break;    
                case 0303: AE("&Atilde;");  break;   
                case 0304: AE("&Auml;");    break;     
                case 0305: AE("&Aring;");   break;    
                case 0306: AE("&AElig;");   break;    
                case 0307: AE("&Ccedil;");  break;   
                case 0310: AE("&Egrave;");  break;   
                case 0311: AE("&Eacute;");  break;   
                case 0312: AE("&Ecirc;");   break;    
                case 0313: AE("&Euml;");    break;     
                case 0314: AE("&Igrave;");  break;   
                case 0315: AE("&Iacute;");  break;   
                case 0316: AE("&Icirc;");   break;    
                case 0317: AE("&Iuml;");    break;     
                case 0320: AE("&ETH;");     break;      
                case 0321: AE("&Ntilde;");  break;   
                case 0322: AE("&Ograve;");  break;   
                case 0323: AE("&Oacute;");  break;   
                case 0324: AE("&Ocirc;");   break;    
                case 0325: AE("&Otilde;");  break;   
                case 0326: AE("&Ouml;");    break;     
                case 0327: AE("&times;");   break;    
                case 0330: AE("&Oslash;");  break;   
                case 0331: AE("&Ugrave;");  break;   
                case 0332: AE("&Uacute;");  break;   
                case 0333: AE("&Ucirc;");   break;    
                case 0334: AE("&Uuml;");    break;     
                case 0335: AE("&Yacute;");  break;   
                case 0336: AE("&THORN;");   break;    
                case 0337: AE("&szlig;");   break;    
                case 0340: AE("&agrave;");  break;   
                case 0341: AE("&aacute;");  break;   
                case 0342: AE("&acirc;");   break;    
                case 0343: AE("&atilde;");  break;   
                case 0344: AE("&auml;");    break;     
                case 0345: AE("&aring;");   break;    
                case 0346: AE("&aelig;");   break;    
                case 0347: AE("&ccedil;");  break;   
                case 0350: AE("&egrave;");  break;   
                case 0351: AE("&eacute;");  break;   
                case 0352: AE("&ecirc;");   break;    
                case 0353: AE("&euml;");    break;     
                case 0354: AE("&igrave;");  break;   
                case 0355: AE("&iacute;");  break;   
                case 0356: AE("&icirc;");   break;    
                case 0357: AE("&iuml;");    break;     
                case 0360: AE("&eth;");     break;      
                case 0361: AE("&ntilde;");  break;   
                case 0362: AE("&ograve;");  break;   
                case 0363: AE("&oacute;");  break;   
                case 0364: AE("&ocirc;");   break;    
                case 0365: AE("&otilde;");  break;   
                case 0366: AE("&ouml;");    break;     
                case 0367: AE("&divide;");  break;   
                case 0370: AE("&oslash;");  break;   
                case 0371: AE("&ugrave;");  break;   
                case 0372: AE("&uacute;");  break;   
                case 0373: AE("&ucirc;");   break;    
                case 0374: AE("&uuml;");    break;     
                case 0375: AE("&yacute;");  break;   
                case 0376: AE("&thorn;");   break;    
                case 0377: AE("&yuml;");    break;     
#if !TclOnly8Bits
                /* "Special" chars, according to XHTML xhtml-special.ent */
                case 338:  AE("&OElig;");   break;
                case 339:  AE("&oelig;");   break;
                case 352:  AE("&Scaron;");  break;
                case 353:  AE("&scaron;");  break;
                case 376:  AE("&Yuml;");    break;
                case 710:  AE("&circ;");    break;
                case 732:  AE("&tilde;");   break;
                case 8194: AE("&ensp;");    break;
                case 8195: AE("&emsp;");    break;
                case 8201: AE("&thinsp;");  break;
                case 8204: AE("&zwnj;");    break;
                case 8205: AE("&zwj;");     break;
                case 8206: AE("&lrm;");     break;
                case 8207: AE("&rlm;");     break;
                case 8211: AE("&ndash;");   break;
                case 8212: AE("&mdash;");   break;
                case 8216: AE("&lsquo;");   break;
                case 8217: AE("&rsquo;");   break;
                case 8218: AE("&sbquo;");   break;
                case 8220: AE("&ldquo;");   break;
                case 8221: AE("&rdquo;");   break;
                case 8222: AE("&bdquo;");   break;
                case 8224: AE("&dagger;");  break;
                case 8225: AE("&Dagger;");  break;
                case 8240: AE("&permil;");  break;
                case 8249: AE("&lsaquo;");  break;
                case 8250: AE("&rsaquo;");  break;
                case 8364: AE("&euro;");    break;
                /* "Symbol" chars, according to XHTML xhtml-symbol.ent */
                case 402:  AE("&fnof;");    break;     
                case 913:  AE("&Alpha;");   break;    
                case 914:  AE("&Beta;");    break;     
                case 915:  AE("&Gamma;");   break;    
                case 916:  AE("&Delta;");   break;    
                case 917:  AE("&Epsilon;"); break;  
                case 918:  AE("&Zeta;");    break;     
                case 919:  AE("&Eta;");     break;      
                case 920:  AE("&Theta;");   break;    
                case 921:  AE("&Iota;");    break;     
                case 922:  AE("&Kappa;");   break;    
                case 923:  AE("&Lambda;");  break;   
                case 924:  AE("&Mu;");      break;       
                case 925:  AE("&Nu;");      break;       
                case 926:  AE("&Xi;");      break;       
                case 927:  AE("&Omicron;"); break;  
                case 928:  AE("&Pi;");      break;       
                case 929:  AE("&Rho;");     break;      
                case 931:  AE("&Sigma;");   break;    
                case 932:  AE("&Tau;");     break;      
                case 933:  AE("&Upsilon;"); break;  
                case 934:  AE("&Phi;");     break;      
                case 935:  AE("&Chi;");     break;      
                case 936:  AE("&Psi;");     break;      
                case 937:  AE("&Omega;");   break;    
                case 945:  AE("&alpha;");   break;    
                case 946:  AE("&beta;");    break;     
                case 947:  AE("&gamma;");   break;    
                case 948:  AE("&delta;");   break;    
                case 949:  AE("&epsilon;"); break;  
                case 950:  AE("&zeta;");    break;     
                case 951:  AE("&eta;");     break;      
                case 952:  AE("&theta;");   break;    
                case 953:  AE("&iota;");    break;     
                case 954:  AE("&kappa;");   break;    
                case 955:  AE("&lambda;");  break;   
                case 956:  AE("&mu;");      break;       
                case 957:  AE("&nu;");      break;       
                case 958:  AE("&xi;");      break;       
                case 959:  AE("&omicron;"); break;  
                case 960:  AE("&pi;");      break;       
                case 961:  AE("&rho;");     break;      
                case 962:  AE("&sigmaf;");  break;   
                case 963:  AE("&sigma;");   break;    
                case 964:  AE("&tau;");     break;      
                case 965:  AE("&upsilon;"); break;  
                case 966:  AE("&phi;");     break;      
                case 967:  AE("&chi;");     break;      
                case 968:  AE("&psi;");     break;      
                case 969:  AE("&omega;");   break;    
                case 977:  AE("&thetasym;");break; 
                case 978:  AE("&upsih;");   break;    
                case 982:  AE("&piv;");     break;      
                case 8226: AE("&bull;");    break;     
                case 8230: AE("&hellip;");  break;   
                case 8242: AE("&prime;");   break;    
                case 8243: AE("&Prime;");   break;    
                case 8254: AE("&oline;");   break;    
                case 8260: AE("&frasl;");   break;    
                case 8472: AE("&weierp;");  break;   
                case 8465: AE("&image;");   break;    
                case 8476: AE("&real;");    break;     
                case 8482: AE("&trade;");   break;    
                case 8501: AE("&alefsym;"); break;  
                case 8592: AE("&larr;");    break;     
                case 8593: AE("&uarr;");    break;     
                case 8594: AE("&rarr;");    break;     
                case 8595: AE("&darr;");    break;     
                case 8596: AE("&harr;");    break;     
                case 8629: AE("&crarr;");   break;    
                case 8656: AE("&lArr;");    break;     
                case 8657: AE("&uArr;");    break;     
                case 8658: AE("&rArr;");    break;     
                case 8659: AE("&dArr;");    break;     
                case 8660: AE("&hArr;");    break;     
                case 8704: AE("&forall;");  break;   
                case 8706: AE("&part;");    break;     
                case 8707: AE("&exist;");   break;    
                case 8709: AE("&empty;");   break;    
                case 8711: AE("&nabla;");   break;    
                case 8712: AE("&isin;");    break;     
                case 8713: AE("&notin;");   break;    
                case 8715: AE("&ni;");      break;       
                case 8719: AE("&prod;");    break;     
                case 8721: AE("&sum;");     break;      
                case 8722: AE("&minus;");   break;    
                case 8727: AE("&lowast;");  break;   
                case 8730: AE("&radic;");   break;    
                case 8733: AE("&prop;");    break;     
                case 8734: AE("&infin;");   break;    
                case 8736: AE("&ang;");     break;      
                case 8743: AE("&and;");     break;      
                case 8744: AE("&or;");      break;       
                case 8745: AE("&cap;");     break;      
                case 8746: AE("&cup;");     break;      
                case 8747: AE("&int;");     break;      
                case 8756: AE("&there4;");  break;   
                case 8764: AE("&sim;");     break;      
                case 8773: AE("&cong;");    break;     
                case 8776: AE("&asymp;");   break;    
                case 8800: AE("&ne;");      break;       
                case 8801: AE("&equiv;");   break;    
                case 8804: AE("&le;");      break;       
                case 8805: AE("&ge;");      break;       
                case 8834: AE("&sub;");     break;      
                case 8835: AE("&sup;");     break;      
                case 8836: AE("&nsub;");    break;     
                case 8838: AE("&sube;");    break;     
                case 8839: AE("&supe;");    break;     
                case 8853: AE("&oplus;");   break;    
                case 8855: AE("&otimes;");  break;   
                case 8869: AE("&perp;");    break;     
                case 8901: AE("&sdot;");    break;     
                case 8968: AE("&lceil;");   break;    
                case 8969: AE("&rceil;");   break;    
                case 8970: AE("&lfloor;");  break;   
                case 8971: AE("&rfloor;");  break;   
                case 9001: AE("&lang;");    break;     
                case 9002: AE("&rang;");    break;     
                case 9674: AE("&loz;");     break;      
                case 9824: AE("&spades;");  break;   
                case 9827: AE("&clubs;");   break;    
                case 9829: AE("&hearts;");  break;   
                case 9830: AE("&diams;");   break;    
#endif                    
                default: charDone = 0; 
                }
#if !TclOnly8Bits
                if (charDone) {
                    clen = UTF8_CHAR_LEN(*pc);
                    pc += (clen - 1);
                }
#endif
            }
#if TclOnly8Bits
            if (!charDone) {
                if (escapeNonASCII && ((unsigned char)*pc > 127)) {
                    AP('&') AP('#')
                    sprintf(charRef, "%d", (unsigned char)*pc);
                    for (i = 0; i < 3; i++) {
                        AP(charRef[i]);
                    }
                    AP(';')
                } else {
                    AP(*pc);
                }
            }
#else
            if (!charDone) {
                if ((unsigned char)*pc > 127) {
                    clen = UTF8_CHAR_LEN(*pc);
                    if (!clen) {
                        domPanic("tcldom_AppendEscaped: can only handle "
                                 "UTF-8 chars up to 3 bytes length");
                    }
                    if (escapeNonASCII) {
                        Tcl_UtfToUniChar(pc, &uniChar);
                        AP('&') AP('#')
                            sprintf(charRef, "%d", uniChar);
                        for (i = 0; i < (int)strlen(charRef); i++) {
                            AP(charRef[i]);
                        }
                        AP(';')
                        pc += (clen - 1);
                    } else {
                        for (i = 0; i < clen; i++) {
                            AP(*pc);
                            pc++;
                        }
                        pc--;
                    }
                } else {
                    AP(*pc);
                }
            }
#endif
        }
        if (b >= bLimit) {
            writeChars(xmlString, chan, buf, b - buf);
            b = buf;
        }
        pc++;
    }
    if (b > buf) {
        writeChars(xmlString, chan, buf, b - buf);
    }
}

/*----------------------------------------------------------------------------
|   tcldom_tolower
|
\---------------------------------------------------------------------------*/
void tcldom_tolower (
    const char *str,
    char *str_out,
    int  len
)
{
    char *p;
    int  i;

    len--; i = 0; p = str_out;
    while (*str && (i < len)) {
        *p++ = tolower((unsigned char)*str++);
        i++;
    }
    *p++ = '\0';
}


/*----------------------------------------------------------------------------
|   tcldom_treeAsHTML
|
\---------------------------------------------------------------------------*/
static
void tcldom_treeAsHTML (
    Tcl_Obj     *htmlString,
    domNode     *node,
    Tcl_Channel  chan,
    int          escapeNonASCII,
    int          htmlEntities,
    int          doctypeDeclaration,
    int          noEscaping
)
{
    int          empty, scriptTag;
    domNode     *child;
    domAttrNode *attrs;
    domDocument *doc;
    char         tag[80], attrName[80];

    if (node->nodeType == DOCUMENT_NODE) {
        doc = (domDocument*) node;
        if (doctypeDeclaration && doc->documentElement) {
            writeChars(htmlString, chan, "<!DOCTYPE ", 10);
            writeChars(htmlString, chan, doc->documentElement->nodeName, -1);
            if (   doc->doctype 
                && doc->doctype->systemId 
                && doc->doctype->systemId[0] != '\0') {
                if (   doc->doctype->publicId 
                    && doc->doctype->publicId[0] != '\0') {
                    writeChars(htmlString, chan, " PUBLIC \"", 9);
                    writeChars(htmlString, chan, doc->doctype->publicId, -1);
                    writeChars(htmlString, chan, "\" \"", 3);
                    writeChars(htmlString, chan, doc->doctype->systemId, -1);
                    writeChars(htmlString, chan, "\"", 1);
                } else {
                    writeChars(htmlString, chan, " SYSTEM \"", 9);
                    writeChars(htmlString, chan, doc->doctype->systemId, -1);
                    writeChars(htmlString, chan, "\"", 1);
                }
            }
            if (doc->doctype && doc->doctype->internalSubset) {
                writeChars(htmlString, chan, " [", 2);
                writeChars(htmlString, chan, doc->doctype->internalSubset, -1);
                writeChars(htmlString, chan, "]", 1);
            }
            writeChars(htmlString, chan, ">\n", 2);
        }
        child = doc->rootNode->firstChild;
        while (child) {
            tcldom_treeAsHTML(htmlString, child, chan, escapeNonASCII,
                              htmlEntities, doctypeDeclaration, 0);
            child = child->nextSibling;
        }
        return;
    }
    
    if (node->nodeType == PROCESSING_INSTRUCTION_NODE) {
        domProcessingInstructionNode *dpn;
        dpn = (domProcessingInstructionNode *)node;
        writeChars(htmlString, chan, "<?", 2);
        writeChars(htmlString, chan, dpn->targetValue, dpn->targetLength);
        writeChars(htmlString, chan, " ", 1);
        writeChars(htmlString, chan, dpn->dataValue, dpn->dataLength);
        writeChars(htmlString, chan, ">", 1);
        return;
    }

    if (node->nodeType == TEXT_NODE) {
        if ((node->nodeFlags & DISABLE_OUTPUT_ESCAPING) 
            || noEscaping) {
            writeChars(htmlString, chan, ((domTextNode*)node)->nodeValue,
                       ((domTextNode*)node)->valueLength);
        } else {
            tcldom_AppendEscaped(htmlString, chan,
                                 ((domTextNode*)node)->nodeValue,
                                 ((domTextNode*)node)->valueLength, 0,
                                 escapeNonASCII, htmlEntities, 0);
        }
        return;
    }

    if (node->nodeType == CDATA_SECTION_NODE) {
        if (noEscaping) {
            writeChars(htmlString, chan, ((domTextNode*)node)->nodeValue,
                       ((domTextNode*)node)->valueLength);
        } else {
            tcldom_AppendEscaped(htmlString, chan,
                                 ((domTextNode*)node)->nodeValue,
                                 ((domTextNode*)node)->valueLength, 0,
                                 escapeNonASCII, htmlEntities, 0);
        }
    }

    if (node->nodeType == COMMENT_NODE) {
        writeChars(htmlString, chan, "<!--", 4);
        writeChars(htmlString, chan, ((domTextNode*)node)->nodeValue,
                   ((domTextNode*)node)->valueLength);
        writeChars(htmlString, chan,  "-->", 3);
        return;

    }

    tcldom_tolower(node->nodeName, tag, 80);
    writeChars(htmlString, chan, "<", 1);
    writeChars(htmlString, chan, tag, -1);


    /*-----------------------------------------------------------
    |   check for HTML tags, that must be handled special:
    |   empty tags and script tags (todo: HTML tags with
    |   URI attributes, to do escaping of Non-ASCII chars
    |   in the URI).
    \----------------------------------------------------------*/
    empty = 0;
    scriptTag = 0;
    switch (tag[0]) {
        case 'a':  if (!strcmp(tag,"area"))       empty = 1; break;
        case 'b':  if (!strcmp(tag,"br")     ||
                       !strcmp(tag,"base")   ||
                       !strcmp(tag,"basefont"))   empty = 1;
        case 'c':  if (!strcmp(tag,"col"))        empty = 1; break;
        case 'f':  if (!strcmp(tag,"frame"))      empty = 1; break;
        case 'h':  if (!strcmp(tag,"hr"))         empty = 1; break;
        case 'i':  if (!strcmp(tag,"img")    ||
                       !strcmp(tag,"input")  ||
                       !strcmp(tag,"isindex"))    empty = 1; break;
        case 'l':  if (!strcmp(tag,"link"))       empty = 1; break;
        case 'm':  if (!strcmp(tag,"meta"))       empty = 1; break;
        case 'p':  if (!strcmp(tag,"param"))      empty = 1; break;
        case 's':  if (!strcmp(tag,"script") ||     
                       !strcmp(tag,"style"))  scriptTag = 1; break;
    }


    attrs = node->firstAttr;
    while (attrs) {
        tcldom_tolower(attrs->nodeName, attrName, 80);
        writeChars(htmlString, chan, " ", 1);
        writeChars (htmlString, chan, attrName, -1);
        writeChars(htmlString, chan, "=\"", 2);
        tcldom_AppendEscaped(htmlString, chan, attrs->nodeValue, -1, 1,
                             escapeNonASCII, htmlEntities, 0);
        writeChars(htmlString, chan, "\"", 1);
        attrs = attrs->nextSibling;
    }
    writeChars(htmlString, chan, ">", 1);


    if (empty) {
        /* strange ! should not happen ! */
        child = node->firstChild;
        while (child != NULL) {
            tcldom_treeAsHTML(htmlString, child, chan, escapeNonASCII,
                              htmlEntities, doctypeDeclaration, scriptTag);
            child = child->nextSibling;
        }
        return;
    }

    if (node->nodeType == ELEMENT_NODE) {
        child = node->firstChild;
        if ((child != NULL) && (child != node->lastChild)
            && (child->nodeType != TEXT_NODE)) {
            writeChars(htmlString, chan, "\n", 1);
        }
        while (child != NULL) {
            tcldom_treeAsHTML(htmlString, child, chan, escapeNonASCII,
                               htmlEntities, doctypeDeclaration, scriptTag);
            child = child->nextSibling;
        }
        if ((node->firstChild != NULL) && (node->firstChild != node->lastChild)
            && (node->lastChild->nodeType != TEXT_NODE)) {
            writeChars(htmlString, chan, "\n", 1);
        }
    }
    writeChars(htmlString, chan, "</", 2);
    writeChars(htmlString, chan, tag, -1);
    writeChars(htmlString, chan, ">",  1);
}


/*----------------------------------------------------------------------------
|   tcldom_treeAsXML
|
\---------------------------------------------------------------------------*/
static
void tcldom_treeAsXML (
    Tcl_Obj    *xmlString,
    domNode    *node,
    int         indent,
    int         level,
    int         doIndent,
    Tcl_Channel chan,
    int         escapeNonASCII,
    int         doctypeDeclaration,
    int         cdataChild,
    int         escapeAllQuot
)
{
    domAttrNode   *attrs;
    domNode       *child;
    domDocument   *doc;
    int            first, hasElements, i;
    char           prefix[MAX_PREFIX_LEN], *start, *p;
    const char    *localName;
    Tcl_HashEntry *h;
    Tcl_DString    dStr;

    if (node->nodeType == DOCUMENT_NODE) {
        doc = (domDocument*) node;
        if (doctypeDeclaration && doc->documentElement) {
            writeChars(xmlString, chan, "<!DOCTYPE ", 10);
            writeChars(xmlString, chan, doc->documentElement->nodeName, -1);
            if (   doc->doctype 
                && doc->doctype->systemId
                && (doc->doctype->systemId[0] != '\0')) {
                if (   doc->doctype->publicId 
                    && doc->doctype->publicId[0] != '\0') {
                    writeChars(xmlString, chan, " PUBLIC \"", 9);
                    writeChars(xmlString, chan, doc->doctype->publicId, -1);
                    writeChars(xmlString, chan, "\" \"", 3);
                    writeChars(xmlString, chan, doc->doctype->systemId, -1);
                    writeChars(xmlString, chan, "\"", 1);
                } else {
                    writeChars(xmlString, chan, " SYSTEM \"", 9);
                    writeChars(xmlString, chan, doc->doctype->systemId, -1);
                    writeChars(xmlString, chan, "\"", 1);
                }
                if (doc->doctype->internalSubset) {
                    writeChars(xmlString, chan, " [", 2);
                    writeChars(xmlString, chan, doc->doctype->internalSubset,
                               -1);
                    writeChars(xmlString, chan, "]", 1);
                }
            }
            writeChars(xmlString, chan, ">\n", 2);
        }
        child = doc->rootNode->firstChild;
        while (child) {
            tcldom_treeAsXML(xmlString, child, indent, level, doIndent, chan,
                             escapeNonASCII, doctypeDeclaration, 0,
                             escapeAllQuot);
            child = child->nextSibling;
        }
        return;
    }

    if (node->nodeType == TEXT_NODE) {
        if (cdataChild) {
            writeChars(xmlString, chan, "<![CDATA[", 9);
            i = 0;
            start = p = ((domTextNode*)node)->nodeValue;
            while (i < ((domTextNode*)node)->valueLength) {
                if (*p == ']') {
                    p++; i++;;
                    if (i >= ((domTextNode*)node)->valueLength) break;
                    if (*p == ']') {
                        p++; i++;;
                        if (i >= ((domTextNode*)node)->valueLength) break;
                        if (*p == '>') {
                            writeChars(xmlString, chan, start, p-start);
                            writeChars(xmlString, chan, "]]><![CDATA[>", 13);
                            start = p+1;
                        }
                    }
                }
                p++; i++;;
            }
            writeChars(xmlString, chan, start, p-start);
            writeChars(xmlString, chan, "]]>", 3);
        } else {
            if (node->nodeFlags & DISABLE_OUTPUT_ESCAPING) {
                writeChars(xmlString, chan, ((domTextNode*)node)->nodeValue,
                           ((domTextNode*)node)->valueLength);
            } else {
                tcldom_AppendEscaped(xmlString, chan,
                                     ((domTextNode*)node)->nodeValue,
                                     ((domTextNode*)node)->valueLength, 0,
                                     escapeNonASCII, 0, escapeAllQuot);
            }
        }
        return;
    }

    if (node->nodeType == CDATA_SECTION_NODE) {
        writeChars(xmlString, chan, "<![CDATA[", 9);
        writeChars(xmlString, chan, ((domTextNode*)node)->nodeValue,
                                    ((domTextNode*)node)->valueLength);
        writeChars(xmlString, chan, "]]>", 3);
        return;
    }

    if ((indent != -1) && doIndent) {
        for(i=0; i<level; i++) {
            writeChars(xmlString, chan, "        ", indent);
        }
    }

    if (node->nodeType == COMMENT_NODE) {
        writeChars(xmlString, chan, "<!--", 4);
        writeChars(xmlString, chan, ((domTextNode*)node)->nodeValue,
                                    ((domTextNode*)node)->valueLength);
        writeChars(xmlString, chan, "-->", 3);
        if (indent != -1) writeChars (xmlString, chan, "\n", 1);
        return;
    }

    if (node->nodeType == PROCESSING_INSTRUCTION_NODE) {
        writeChars(xmlString, chan, "<?", 2);
        writeChars(xmlString, chan, 
                    ((domProcessingInstructionNode*)node)->targetValue,
                    ((domProcessingInstructionNode*)node)->targetLength);
        writeChars(xmlString, chan, " ", 1);
        writeChars(xmlString, chan, 
                   ((domProcessingInstructionNode*)node)->dataValue,
                   ((domProcessingInstructionNode*)node)->dataLength);
        writeChars(xmlString, chan, "?>", 2);
        if (indent != -1) writeChars (xmlString, chan, "\n", 1);
        return;
    }

    writeChars(xmlString, chan, "<", 1);
    writeChars(xmlString, chan, node->nodeName, -1);

    attrs = node->firstAttr;
    while (attrs) {
        writeChars(xmlString, chan, " ", 1);
        writeChars(xmlString, chan, attrs->nodeName, -1);
        writeChars(xmlString, chan, "=\"", 2);
        tcldom_AppendEscaped(xmlString, chan, attrs->nodeValue, 
                             attrs->valueLength, 1, escapeNonASCII, 0,
                             escapeAllQuot);
        writeChars(xmlString, chan, "\"", 1);
        attrs = attrs->nextSibling;
    }

    hasElements = 0;
    first       = 1;
    doIndent    = 1;

    if (node->nodeType == ELEMENT_NODE) {
        cdataChild = 0;
        if (node->ownerDocument->doctype
            && node->ownerDocument->doctype->cdataSectionElements) {
            if (node->namespace) {
                Tcl_DStringInit (&dStr);
                Tcl_DStringAppend (&dStr, domNamespaceURI(node), -1);
                Tcl_DStringAppend (&dStr, ":", 1);
                domSplitQName (node->nodeName, prefix, &localName);
                Tcl_DStringAppend (&dStr, localName, -1);
                h = Tcl_FindHashEntry (
                    node->ownerDocument->doctype->cdataSectionElements,
                    Tcl_DStringValue (&dStr));
                Tcl_DStringFree (&dStr);
            } else {
                h = Tcl_FindHashEntry (
                    node->ownerDocument->doctype->cdataSectionElements,
                    node->nodeName);
            }
            if (h) {
                cdataChild = 1;
            }
        }
        child = node->firstChild;
        while (child != NULL) {

            if (  (child->nodeType == ELEMENT_NODE)
                ||(child->nodeType == PROCESSING_INSTRUCTION_NODE)
                ||(child->nodeType == COMMENT_NODE) )
            {
                hasElements = 1;
            }
            if (first) {
                writeChars(xmlString, chan, ">", 1);
                if ((indent != -1) && hasElements) {
                    writeChars(xmlString, chan, "\n", 1);
                }
            }
            first = 0;
            tcldom_treeAsXML(xmlString, child, indent, level+1, doIndent,
                             chan, escapeNonASCII, doctypeDeclaration,
                             cdataChild, escapeAllQuot);
            doIndent = 0;
            if (  (child->nodeType == ELEMENT_NODE)
                ||(child->nodeType == PROCESSING_INSTRUCTION_NODE)
                ||(child->nodeType == COMMENT_NODE) )
            {
               doIndent = 1;
            }
            child = child->nextSibling;
        }
    }

    if (first) {
        if (indent != -1) {
            writeChars(xmlString, chan, "/>\n", 3);
        } else {
            writeChars(xmlString, chan, "/>",   2);
        }
    } else {
        if ((indent != -1) && hasElements) {
            for(i=0; i<level; i++) {
                writeChars(xmlString, chan, "        ", indent);
            }
        }
        writeChars (xmlString, chan, "</", 2);
        writeChars(xmlString, chan, node->nodeName, -1);
        if (indent != -1) {
            writeChars(xmlString, chan, ">\n", 2);
        } else {
            writeChars(xmlString, chan, ">",   1);
        }
    }
}

/*----------------------------------------------------------------------------
|   findBaseURI
|
\---------------------------------------------------------------------------*/
const char *findBaseURI (
    domNode *node
)
{
    const char *baseURI = NULL;
    Tcl_HashEntry *entryPtr;
    domNode       *orgNode;
    
    orgNode = node;
    do {
        if (node->nodeFlags & HAS_BASEURI) {
            entryPtr = Tcl_FindHashEntry(node->ownerDocument->baseURIs,
                                         (char*)node);
            baseURI = (const char *)Tcl_GetHashValue(entryPtr);
            break;
        } else {
            node = node->parentNode;
        }
    } while (node);
    if (!baseURI) {
        node = orgNode->ownerDocument->rootNode;
        if (node->nodeFlags & HAS_BASEURI) {
            entryPtr = Tcl_FindHashEntry(node->ownerDocument->baseURIs,
                                          (char*)node);
            baseURI = (const char *)Tcl_GetHashValue(entryPtr);
        }
    }
    return baseURI;
}

/*----------------------------------------------------------------------------
|   serializeAsXML
|
\---------------------------------------------------------------------------*/
static int serializeAsXML (
    domNode    *node,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    *CONST objv[]
)
{
    char          *channelId, prefix[MAX_PREFIX_LEN];
    const char    *localName;
    int            indent, mode, escapeNonASCII = 0, doctypeDeclaration = 0;
    int            optionIndex, cdataChild, escapeAllQuot = 0;
    Tcl_Obj       *resultPtr;
    Tcl_Channel    chan = (Tcl_Channel) NULL;
    Tcl_HashEntry *h;
    Tcl_DString    dStr;

    static CONST84 char *asXMLOptions[] = {
        "-indent", "-channel", "-escapeNonASCII", "-doctypeDeclaration",
        "-escapeAllQuot", 
        NULL
    };
    enum asXMLOption {
        m_indent, m_channel, m_escapeNonASCII, m_doctypeDeclaration,
        m_escapeAllQuot
    };
    
    if (objc > 10) {
        Tcl_WrongNumArgs(interp, 2, objv,
                         "?-indent <0..8>? ?-channel <channelID>? "
                         "?-escapeNonASCII? ?-escapeAllQuot? "
                         "?-doctypeDeclaration <boolean>?");
        return TCL_ERROR;
    }
    indent = 4;
    while (objc > 2) {
        if (Tcl_GetIndexFromObj(interp, objv[2], asXMLOptions, "option", 0,
                               &optionIndex) != TCL_OK) {
            return TCL_ERROR;
        }
        switch ((enum asXMLOption) optionIndex) {

        case m_indent:
            if (objc < 4) {
                SetResult("-indent must have an argument "
                          "(0..8 or 'no'/'none')");
                return TCL_ERROR;
            }
            if (strcmp("none", Tcl_GetString(objv[3]))==0) {
                indent = -1;
            }
            else if (strcmp("no", Tcl_GetString(objv[3]))==0) {
                indent = -1;
            }
            else if (Tcl_GetIntFromObj(interp, objv[3], &indent) != TCL_OK) {
                SetResult( "indent must be an integer (0..8) or 'no'/'none'");
                return TCL_ERROR;
            }
            objc -= 2;
            objv += 2;
            break;

        case m_channel:
            if (objc < 4) {
                SetResult("-channel must have a channeldID as argument");
                return TCL_ERROR;
            }
            channelId = Tcl_GetString(objv[3]);
            chan = Tcl_GetChannel(interp, channelId, &mode);
            if (chan == (Tcl_Channel) NULL) {
                SetResult("-channel must have a channeldID as argument");
                return TCL_ERROR;
            }
            if ((mode & TCL_WRITABLE) == 0) {
                Tcl_AppendResult(interp, "channel \"", channelId,
                                "\" wasn't opened for writing", (char*)NULL);
                return TCL_ERROR;
            }
            objc -= 2;
            objv += 2;
            break;

        case m_escapeNonASCII:
            escapeNonASCII = 1;
            objc--;
            objv++;
            break;
            
        case m_doctypeDeclaration:
            if (node->nodeType != DOCUMENT_NODE) {
                SetResult("-doctypeDeclaration as flag to the method "
                          "'asXML' is only allowed for domDocCmds");
                return TCL_ERROR;
            }
            if (objc < 4) {
                SetResult("-doctypeDeclaration must have a boolean value "
                          "as argument");
                return TCL_ERROR;
            }
            if (Tcl_GetBooleanFromObj(interp, objv[3], &doctypeDeclaration)
                != TCL_OK) {
                return TCL_ERROR;
            }
            objc -= 2;
            objv += 2;
            break;

        case m_escapeAllQuot:
            escapeAllQuot = 1;
            objc -= 1;
            objv += 1;
            break;
        }
    }
    if (indent > 8)  indent = 8;
    if (indent < -1) indent = -1;

    resultPtr = Tcl_NewStringObj("", 0);
    cdataChild = 0;
    if (node->nodeType == ELEMENT_NODE
        && node->ownerDocument->doctype 
        && node->ownerDocument->doctype->cdataSectionElements) {
        if (node->namespace) {
            Tcl_DStringInit (&dStr);
            Tcl_DStringAppend (&dStr, domNamespaceURI(node), -1);
            Tcl_DStringAppend (&dStr, ":", 1);
            domSplitQName (node->nodeName, prefix, &localName);
            Tcl_DStringAppend (&dStr, localName, -1);
            h = Tcl_FindHashEntry (
                node->ownerDocument->doctype->cdataSectionElements,
                Tcl_DStringValue (&dStr));
            Tcl_DStringFree (&dStr);
        } else {
            h = Tcl_FindHashEntry (
                node->ownerDocument->doctype->cdataSectionElements,
                node->nodeName);
        }
        if (h) {
            cdataChild = 1;
        }
    }
    tcldom_treeAsXML(resultPtr, node, indent, 0, 1, chan, escapeNonASCII,
                     doctypeDeclaration, cdataChild, escapeAllQuot);
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*----------------------------------------------------------------------------
|   serializeAsHTML
|
\---------------------------------------------------------------------------*/
static int serializeAsHTML (
    domNode    *node,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    *CONST objv[]
)
{
    char       *channelId;
    int         optionIndex, mode, escapeNonASCII = 0, htmlEntities = 0;
    int         doctypeDeclaration = 0;
    Tcl_Obj    *resultPtr;
    Tcl_Channel chan = (Tcl_Channel) NULL;

    static CONST84 char *asHTMLOptions[] = {
        "-channel", "-escapeNonASCII", "-htmlEntities", "-doctypeDeclaration",
        NULL
    };
    enum asHTMLOption {
        m_channel, m_escapeNonASCII, m_htmlEntities, m_doctypeDeclaration
    };
    
    if (objc > 8) {
        Tcl_WrongNumArgs(interp, 2, objv,
                         "?-channel <channelId>? ?-escapeNonASCII? "
                         "?-htmlEntities? ?-doctypeDeclaration <boolean>?");
        return TCL_ERROR;
    }
    while (objc > 2) {
        if (Tcl_GetIndexFromObj(interp, objv[2], asHTMLOptions, "option", 
                                0, &optionIndex) != TCL_OK) {
            return TCL_ERROR;
        }
        switch ((enum asHTMLOption) optionIndex) {
            
        case m_channel:
            if (objc < 4) {
                SetResult("-channel must have a channeldID as argument");
                return TCL_ERROR;
            }
            channelId = Tcl_GetString(objv[3]);
            chan = Tcl_GetChannel(interp, channelId, &mode);
            if (chan == (Tcl_Channel) NULL) {
                SetResult("-channel must have a channeldID as argument");
                return TCL_ERROR;
            }
            if ((mode & TCL_WRITABLE) == 0) {
                Tcl_AppendResult(interp, "channel \"", channelId,
                                "\" wasn't opened for writing", (char*)NULL);
                return TCL_ERROR;
            }
            objc -= 2;
            objv += 2;
            break;

        case m_escapeNonASCII:
            escapeNonASCII = 1;
            objc--;
            objv++;
            break;

        case m_htmlEntities:
            htmlEntities = 1;
            objc--;
            objv++;
            break;

        case m_doctypeDeclaration:
            if (node->nodeType != DOCUMENT_NODE) {
                SetResult("-doctypeDeclaration as flag to the method "
                          "'asHTML' is only allowed for domDocCmds");
                return TCL_ERROR;
            }
            if (objc < 4) {
                SetResult("-doctypeDeclaration must have a boolean value "
                          "as argument");
                return TCL_ERROR;
            }
            if (Tcl_GetBooleanFromObj(interp, objv[3], &doctypeDeclaration)
                != TCL_OK) {
                return TCL_ERROR;
            }
            objc -= 2;
            objv += 2;
            break;
        }
    }
    resultPtr = Tcl_NewStringObj("", 0);
    tcldom_treeAsHTML(resultPtr, node, chan, escapeNonASCII, htmlEntities,
                      doctypeDeclaration, 0);
    Tcl_AppendResult(interp, Tcl_GetString(resultPtr), NULL);
    Tcl_DecrRefCount(resultPtr);
    return TCL_OK;
}

/*----------------------------------------------------------------------------
|   cdataSectionElements
|
\---------------------------------------------------------------------------*/
static int cdataSectionElements (
    domDocument *doc,
    Tcl_Interp  *interp,
    int          objc,
    Tcl_Obj     *CONST objv[] 
    )
{
    int result, hnew;
    Tcl_Obj *resultPtr,*namePtr;
    Tcl_HashEntry *h;
    Tcl_HashSearch search;
    
    CheckArgs (3,4,0, "<domDoc> cdataSectionElements ?URI:?localname "
               "?boolean?");
    if (objc == 3) {
        if (Tcl_GetString(objv[2])[0] == '*' 
            && Tcl_GetString(objv[2])[1] == '\0') {
            Tcl_ResetResult (interp);
            if (doc->doctype && doc->doctype->cdataSectionElements) {
                resultPtr = Tcl_GetObjResult (interp);
                for (h = Tcl_FirstHashEntry (
                         doc->doctype->cdataSectionElements, &search);
                     h != NULL;
                     h = Tcl_NextHashEntry(&search)) {
                    namePtr = Tcl_NewStringObj (
                        Tcl_GetHashKey (doc->doctype->cdataSectionElements,
                                        h), -1);
                    result = Tcl_ListObjAppendElement (interp, resultPtr, 
                                                       namePtr);
                    if (result != TCL_OK) {
                        Tcl_DecrRefCount(namePtr);
                        return result;
                    }
                }
            }
            return TCL_OK;
        }
        if (!doc->doctype || !doc->doctype->cdataSectionElements) {
            SetBooleanResult (0);
        } else {
            if (Tcl_FindHashEntry (doc->doctype->cdataSectionElements,
                                   Tcl_GetString (objv[2]))) {
                SetBooleanResult (1);
            } else {
                SetBooleanResult (0);
            }
        }
    } else {
        if (Tcl_GetBooleanFromObj (interp, objv[3], &result) 
            != TCL_OK) {
            return TCL_ERROR;
        }
        if (result) {
            if (!doc->doctype) {
                doc->doctype = (domDocInfo *)MALLOC(sizeof(domDocInfo));
                memset(doc->doctype, 0,(sizeof(domDocInfo)));
            }
            if (!doc->doctype->cdataSectionElements) {
                doc->doctype->cdataSectionElements =
                    (Tcl_HashTable *)MALLOC(sizeof(Tcl_HashTable));
                Tcl_InitHashTable (doc->doctype->cdataSectionElements,
                                   TCL_STRING_KEYS);
            }
            Tcl_CreateHashEntry (doc->doctype->cdataSectionElements,
                                 Tcl_GetString (objv[2]), &hnew);
        } else {
            if (doc->doctype && doc->doctype->cdataSectionElements) {
                h = Tcl_FindHashEntry (doc->doctype->cdataSectionElements,
                                       Tcl_GetString (objv[2]));
                if (h) {
                    Tcl_DeleteHashEntry (h);
                    if (!doc->doctype->cdataSectionElements->numEntries) {
                        Tcl_DeleteHashTable (
                            doc->doctype->cdataSectionElements
                            );
                        FREE (doc->doctype->cdataSectionElements);
                        doc->doctype->cdataSectionElements = NULL;
                    }
                }
            }
        }
        SetBooleanResult(result);
    }
    return TCL_OK;
}

/*----------------------------------------------------------------------------
|   selectNodesNamespaces
|
\---------------------------------------------------------------------------*/
static int selectNodesNamespaces (
    domDocument *doc,
    Tcl_Interp  *interp,
    int          objc,
    Tcl_Obj     *CONST objv[] 
    )
{
    int      len, i, result;
    Tcl_Obj *objPtr, *listPtr;

    CheckArgs (2,3,2, "?prefixUriList?");
    if (objc == 3) {
        result = Tcl_ListObjLength (interp, objv[2], &len);
        if (result != TCL_OK || (len % 2) != 0) {
            SetResult ("The optional argument to the selectNodesNamespaces"
                       " method must be a 'prefix namespace' pairs list");
            return TCL_ERROR;
        }
        i = 0;
        if (doc->prefixNSMappings) {
            while (doc->prefixNSMappings[i]) {
                FREE (doc->prefixNSMappings[i]);
                i++;
            }
        }
        if (i < len + 1) {
            if (doc->prefixNSMappings) FREE (doc->prefixNSMappings);
            doc->prefixNSMappings = MALLOC (sizeof (char*)*(len+1));
        }
        for (i = 0; i < len; i++) {
            Tcl_ListObjIndex (interp, objv[2], i, &objPtr);
            doc->prefixNSMappings[i] = tdomstrdup (Tcl_GetString (objPtr));
        }
        doc->prefixNSMappings[len] = NULL;
        Tcl_SetObjResult (interp, objv[2]);
    } else {
        listPtr = Tcl_NewListObj (0, NULL);
        i = 0;
        if (doc->prefixNSMappings) {
            while (doc->prefixNSMappings[i]) {
                objPtr = Tcl_NewStringObj (doc->prefixNSMappings[i], -1);
                Tcl_ListObjAppendElement (interp, listPtr, objPtr);
                i++;
            }
        }
        Tcl_SetObjResult (interp, listPtr);
    }
    return TCL_OK;
}

/*----------------------------------------------------------------------------
|   renameNodes
|
\---------------------------------------------------------------------------*/
static int renameNodes (
    domDocument *doc,
    Tcl_Interp  *interp,
    int          objc,
    Tcl_Obj     *CONST objv[] 
    )
{
    int      len, i, hnew;
    char    *errMsg;
    Tcl_HashEntry *h;
    Tcl_Obj *objPtr;
    domNode     *node;
    
    CheckArgs (4,4,0, "<domDoc> renameNode nodeList name");
    if (Tcl_ListObjLength (interp, objv[2], &len) != TCL_OK) {
        SetResult ("The first argument to the renameNode method"
                   " must be a list of element nodes.");
        return TCL_ERROR;
    }
    h = Tcl_CreateHashEntry(&HASHTAB(doc,tdom_tagNames), 
                            Tcl_GetString(objv[3]), &hnew);
    for (i = 0; i < len; i++) {
        Tcl_ListObjIndex (interp, objv[2], i, &objPtr);
        node = tcldom_getNodeFromName (interp, Tcl_GetString (objPtr),
                                       &errMsg);
        if (node == NULL) {
            SetResult (errMsg);
            if (errMsg) FREE (errMsg);
            return TCL_ERROR;
        }
        node->nodeName = (char *)&(h->key);
    }
    return TCL_OK;
}

/*----------------------------------------------------------------------------
|   deleteXPathCache
|
\---------------------------------------------------------------------------*/
static int deleteXPathCache (
    domDocument *doc,
    Tcl_Interp  *interp,
    int          objc,
    Tcl_Obj     *CONST objv[] 
    )
{
    Tcl_HashEntry *h;
    Tcl_HashSearch search;
    
    CheckArgs (2,3,0, "<domDoc> deleteXPathCache ?xpathQuery?");
    if (objc == 3) {
        if (!doc->xpathCache) {
            return TCL_OK;
        }
        h = Tcl_FindHashEntry (doc->xpathCache, Tcl_GetString(objv[2]));
        if (h) {
            xpathFreeAst((ast)Tcl_GetHashValue (h));
            Tcl_DeleteHashEntry (h);
        }
        return TCL_OK;
    }
    if (!doc->xpathCache) {
        return TCL_OK;
    }
    h = Tcl_FirstHashEntry (doc->xpathCache, &search);
    while (h) {
        xpathFreeAst((ast)Tcl_GetHashValue (h));
        h = Tcl_NextHashEntry (&search);
    }
    Tcl_DeleteHashTable (doc->xpathCache);
    FREE (doc->xpathCache);
    doc->xpathCache = NULL;
    return TCL_OK;
}


/*----------------------------------------------------------------------------
|   applyXSLT
|
\---------------------------------------------------------------------------*/
static int applyXSLT (
    domNode     *node,
    Tcl_Interp  *interp,
    void        *clientData,
    int          objc,
    Tcl_Obj     *CONST objv[]
    )
{
    char          *usage, **parameters = NULL, *errMsg, *option;
    Tcl_Obj       *objPtr, *localListPtr = (Tcl_Obj *)NULL;
    int            i, result, length, optionIndex;
    int            ignoreUndeclaredParameters = 0;
    domDocument   *xsltDoc, *xmlDoc, *resultDoc;
    XsltMsgCBInfo  xsltMsgInfo;

    static char *method_usage = 
        "wrong # args: should be \"nodeObj xslt ?-parameters parameterList? "
        "?-ignoreUndeclaredParameters? ?-xsltmessagecmd cmd? xsltDocNode "
        "?varname?\"";

    static char *cmd_usage = 
        "wrong # args: should be \"?-parameters parameterList? "
        "?-ignoreUndeclaredParameters? ?-xsltmessagecmd cmd? <xmlDocObj> "
        "?objVar?\"";

    static CONST84 char *xsltOptions[] = {
        "-parameters", "-ignoreUndeclaredParameters", "-xsltmessagecmd", NULL
    };

    enum xsltOption {
        m_parmeters, m_ignoreUndeclaredParameters, m_xsltmessagecmd
    };

    xsltMsgInfo.interp = interp;
    xsltMsgInfo.msgcmd = NULL;

    if (node)  usage = method_usage;
    else       usage = cmd_usage;
    
    while (objc > 1) {
        option = Tcl_GetString(objv[0]);
        if (option[0] != '-') {
            break;
        }
        if (Tcl_GetIndexFromObj(interp, objv[0], xsltOptions, "option", 0,
                                 &optionIndex) != TCL_OK) {
            goto applyXSLTCleanUP;
        }
    
        switch ((enum xsltOption) optionIndex) {

        case m_parmeters:
            if (objc < 3) {SetResult(usage); goto applyXSLTCleanUP;}
            if (Tcl_ListObjLength(interp, objv[1], &length) != TCL_OK) {
                SetResult("ill-formed parameters list: the -parameters "
                          "option needs a list of parameter name and "
                          "parameter value pairs");
                goto applyXSLTCleanUP;
            }
            if (length % 2) {
                SetResult("parameter value missing: the -parameters "
                          "option needs a list of parameter name and "
                          "parameter value pairs");
                goto applyXSLTCleanUP;
            }
            if (parameters) {
                SetResult("only one -parameters option allowed");
                goto applyXSLTCleanUP;
            }
            localListPtr = Tcl_DuplicateObj(objv[1]);
            Tcl_IncrRefCount(localListPtr);
            parameters =  (char **)MALLOC(sizeof(char *)*(length+1));
            for (i = 0; i < length; i ++) {
                Tcl_ListObjIndex(interp, localListPtr, i, &objPtr);
                parameters[i] = Tcl_GetString(objPtr);
            }
            parameters[length] = NULL;
            objc -= 2;
            objv += 2;
            break;
        
        case m_ignoreUndeclaredParameters:
            if (objc < 2) {SetResult(usage); goto applyXSLTCleanUP;}
            ignoreUndeclaredParameters = 1;
            objc--; objv++;
            break;
            
        case m_xsltmessagecmd:
            if (objc < 3) {SetResult(usage); goto applyXSLTCleanUP;}
            if (xsltMsgInfo.msgcmd) {
                Tcl_DecrRefCount(xsltMsgInfo.msgcmd);
            }
            xsltMsgInfo.msgcmd = objv[1];
            Tcl_IncrRefCount(xsltMsgInfo.msgcmd);
            objc -= 2;
            objv += 2;
            break;
        }
    }
    if (objc > 2 || objc < 1) {SetResult(usage); goto applyXSLTCleanUP;}
    if (node) {
        xsltDoc = tcldom_getDocumentFromName(interp, Tcl_GetString(objv[0]),
                                             &errMsg);
        if (xsltDoc == NULL) {
            SetResult( errMsg );
            goto applyXSLTCleanUP;
        }
    } else {
        xmlDoc = tcldom_getDocumentFromName(interp,Tcl_GetString(objv[0]),
                                            &errMsg);
        if (xmlDoc == NULL) {
            SetResult( errMsg );
            goto applyXSLTCleanUP;
        }
        node = (domNode *) xmlDoc;
        xsltDoc = NULL;
    }
    result = xsltProcess(xsltDoc, node, clientData, parameters, 
                          ignoreUndeclaredParameters,
                          tcldom_xpathFuncCallBack,  interp,
                          tcldom_xsltMsgCB, &xsltMsgInfo,
                          &errMsg, &resultDoc);

    if (result < 0) {
        SetResult( errMsg );
        FREE(errMsg);
        goto applyXSLTCleanUP;
    }
    if (parameters) {
        Tcl_DecrRefCount(localListPtr);
        FREE((char *) parameters);
    }
    if (xsltMsgInfo.msgcmd) {
        Tcl_DecrRefCount(xsltMsgInfo.msgcmd);
    }
    return tcldom_returnDocumentObj(interp, resultDoc, (objc == 2),
                                     (objc == 2) ? objv[1] : NULL, 1, 0);
            
 applyXSLTCleanUP:
    if (localListPtr) {
        Tcl_DecrRefCount(localListPtr);
        FREE((char *) parameters);
    }
    if (xsltMsgInfo.msgcmd) {
        Tcl_DecrRefCount(xsltMsgInfo.msgcmd);
    }
    return TCL_ERROR;
}

/*----------------------------------------------------------------------------
|   tcldom_XSLTObjCmd
|
\---------------------------------------------------------------------------*/
static int tcldom_XSLTObjCmd (
    ClientData  clientData,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    *CONST objv[]
)
{
    int          index;
    char        *errMsg = NULL;
    
    static CONST84 char *options[] = {
        "transform", "delete", NULL
    };
    enum option {
        m_transform, m_delete
    };
    

    /* Longest possible call currently is:
       xsltCmd transform -parameters parameterList \
                         -ignoreUndeclaredParameters
                         -xsltmessagecmd cmd <xmlDocObj> objVar */
    CheckArgs(2,9,1,"option ?arg ...?");

    /* This is not optimal, because we do the
       tcldom_getDocumentFromName call here and again in
       applyXSLT. This is only transitional, until <domNode xslt ..>
       will be deprecated */
    if ((tcldom_getDocumentFromName (interp, Tcl_GetString(objv[1]), &errMsg) 
         != NULL)
        || (Tcl_GetString (objv[1])[0] == '-')) {
        /* Method obmitted, may default to "transform", try this */
        objv++;
        objc--;
        return applyXSLT(NULL, interp, (void *) clientData, objc, objv);
    }

    if (Tcl_GetIndexFromObj (interp, objv[1], options, "option", 0, &index)
        != TCL_OK) {
        return TCL_ERROR;
    }
    switch ((enum option) index) {
    case m_transform:
        objv++;objv++;
        objc--;objc--;
        return applyXSLT(NULL, interp, (void *) clientData, objc, objv);
    case m_delete:
        if (objc != 2) {
            Tcl_WrongNumArgs(interp, 2, objv, "");
            return TCL_ERROR;
        }
        Tcl_DeleteCommand(interp, Tcl_GetString(objv[0]));
    }
    return TCL_OK;
}

/*----------------------------------------------------------------------------
|   convertToXSLTCmd
|
\---------------------------------------------------------------------------*/
static int convertToXSLTCmd (
    domDocument *doc,
    Tcl_Interp  *interp,
    int          setVariable,
    Tcl_Obj     *var_name
    )
{
    char *errMsg, *objVar, objCmdName[80];
    ClientData *clientData;

    doc->nodeFlags |= DONT_FREE;
    clientData = (ClientData *) xsltCompileStylesheet(doc,
                                                      tcldom_xpathFuncCallBack,
                                                      interp, 0, &errMsg);
    if (!clientData) {
        SetResult(errMsg);
        if (setVariable) {
            objVar = Tcl_GetString(var_name);
            Tcl_UnsetVar(interp, objVar, 0);
            Tcl_SetVar   (interp, objVar, "", 0);
        }
        FREE(errMsg);
        return TCL_ERROR;
    }
    DOC_CMD(objCmdName, doc);
    Tcl_DeleteCommand( interp, objCmdName );
    XSLT_CMD(objCmdName, doc);
    Tcl_CreateObjCommand(interp, objCmdName, tcldom_XSLTObjCmd, clientData,
                          xsltFreeStateWrapper);
    if (setVariable) {
        objVar = Tcl_GetString(var_name);
        Tcl_UnsetVar (interp, objVar, 0);
        Tcl_SetVar   (interp, objVar, objCmdName, 0);
    }
    SetResult(objCmdName);
    return TCL_OK;
}

/*----------------------------------------------------------------------------
|   tcldom_NodeObjCmd
|
\---------------------------------------------------------------------------*/
int tcldom_NodeObjCmd (
    ClientData  clientData,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    *CONST objv[]
)
{
    GetTcldomTSD()

    domNode     *node, *child, *refChild, *oldChild, *refNode;
    domNS       *ns;
    domAttrNode *attrs;
    domException exception;
    char         tmp[200], objCmdName[80], prefix[MAX_PREFIX_LEN],
                *method, *nodeName, *str, *attr_name, *attr_val, *filter,
                *errMsg;
    const char  *localName, *uri, *nsStr;
    int          result, length, methodIndex, i, line, column;
    int          nsIndex, bool, hnew;
    Tcl_Obj     *namePtr, *resultPtr;
    Tcl_Obj     *mobjv[MAX_REWRITE_ARGS];
    Tcl_CmdInfo  cmdInfo;
    Tcl_HashEntry *h;

    static CONST84 char *nodeMethods[] = {
        "firstChild",      "nextSibling",    "getAttribute",    "nodeName",
        "nodeValue",       "nodeType",       "attributes",      "asList",
        "find",            "setAttribute",   "removeAttribute", "parentNode",
        "previousSibling", "lastChild",      "appendChild",     "removeChild",
        "hasChildNodes",   "localName",      "childNodes",      "ownerDocument",
        "insertBefore",    "replaceChild",   "getLine",         "getColumn",
        "asXML",           "appendFromList", "child",           "fsibling",
        "psibling",        "descendant",     "ancestor",        "text",
        "root",            "hasAttribute",   "cloneNode",       "appendXML",
        "target",          "data",           "selectNodes",     "namespaceURI",
        "getAttributeNS",  "setAttributeNS", "hasAttributeNS",  "removeAttributeNS",
        "asHTML",          "prefix",         "getBaseURI",      "appendFromScript",
        "xslt",            "toXPath",        "delete",          "getElementById",
        "getElementsByTagName",              "getElementsByTagNameNS",
        "disableOutputEscaping",             "precedes",         "asText",
        "insertBeforeFromScript",            "normalize",        "baseURI",
#ifdef TCL_THREADS
        "readlock",        "writelock",
#endif
        NULL
    };
    enum nodeMethod {
        m_firstChild,      m_nextSibling,    m_getAttribute,    m_nodeName,
        m_nodeValue,       m_nodeType,       m_attributes,      m_asList,
        m_find,            m_setAttribute,   m_removeAttribute, m_parentNode,
        m_previousSibling, m_lastChild,      m_appendChild,     m_removeChild,
        m_hasChildNodes,   m_localName,      m_childNodes,      m_ownerDocument,
        m_insertBefore,    m_replaceChild,   m_getLine,         m_getColumn,
        m_asXML,           m_appendFromList, m_child,           m_fsibling,
        m_psibling,        m_descendant,     m_ancestor,        m_text,
        m_root,            m_hasAttribute,   m_cloneNode,       m_appendXML,
        m_target,          m_data,           m_selectNodes,     m_namespaceURI,
        m_getAttributeNS,  m_setAttributeNS, m_hasAttributeNS,  m_removeAttributeNS,
        m_asHTML,          m_prefix,         m_getBaseURI,      m_appendFromScript,
        m_xslt,            m_toXPath,        m_delete,          m_getElementById,
        m_getElementsByTagName,              m_getElementsByTagNameNS,
        m_disableOutputEscaping,             m_precedes,        m_asText,
        m_insertBeforeFromScript,            m_normalize,       m_baseURI
#ifdef TCL_THREADS
        ,m_readlock,        m_writelock
#endif
    };


    node = (domNode*) clientData;
    if (TSD(domCreateCmdMode) == DOM_CREATECMDMODE_AUTO) {
        TSD(dontCreateObjCommands) = 0;
    }
    if (node == NULL) {
        if (objc < 3) {
            SetResult(node_usage);
            return TCL_ERROR;
        }
        if (TSD(domCreateCmdMode) == DOM_CREATECMDMODE_AUTO) {
            TSD(dontCreateObjCommands) = 1;
        }
        nodeName = Tcl_GetString(objv[1]);
        node = tcldom_getNodeFromName(interp, nodeName, &errMsg);
        if (node == NULL) {
            SetResult(errMsg);
            return TCL_ERROR;
        }
        objc--;
        objv++;
    }
    if (objc < 2) {
        SetResult(node_usage);
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(NULL, objv[1], nodeMethods, "method", 0,
                            &methodIndex) != TCL_OK) {

        method = Tcl_GetString(objv[1]);
        if (*method != '@') {
            /*--------------------------------------------------------
            |   not a getAttribute short cut:
            |   try to find method implemented as normal Tcl proc
            \-------------------------------------------------------*/
            result = 0;
            if (node->nodeType == ELEMENT_NODE) {
                /*----------------------------------------------------
                |   try to find Tcl level node specific method proc
                |
                |       ::dom::domNode::<nodeName>::<method>
                |
                \---------------------------------------------------*/
                sprintf(tmp, "::dom::domNode::%s::%s", (char*)node->nodeName,
                        method);
                DBG(fprintf(stderr, "testing %s\n", tmp));
                result = Tcl_GetCommandInfo(interp, tmp, &cmdInfo);
            }
            if (!result) {
                /*----------------------------------------------------
                |   try to find Tcl level general method proc
                |
                |       ::dom::domNode::<method>
                |
                \---------------------------------------------------*/
                sprintf(tmp, "::dom::domNode::%s", method);
                DBG(fprintf(stderr, "testing %s\n", tmp));
                result = Tcl_GetCommandInfo(interp, tmp, &cmdInfo);
            }
            if (!result) {
                SetResult(node_usage);
                return TCL_ERROR;
            }
            if (!cmdInfo.isNativeObjectProc) {
                SetResult("can't access Tcl level method!");
                return TCL_ERROR;
            }
            if (objc >= MAX_REWRITE_ARGS) {
                SetResult("too many args to call Tcl level method!");
                return TCL_ERROR;
            }
            mobjv[0] = objv[1];
            mobjv[1] = objv[0];
            for (i=2; i<objc; i++) mobjv[i] = objv[i];
            return cmdInfo.objProc(cmdInfo.objClientData, interp, objc, mobjv);
        }

        /*--------------------------------------------------------
        |   @<attributeName>: try to look up attribute
        \-------------------------------------------------------*/
        Tcl_ResetResult(interp);
        CheckArgs(2,3,1,"@<attributeName> ?defaultvalue?");
        if (node->nodeType != ELEMENT_NODE) {
            SetResult("NOT_AN_ELEMENT : there are no attributes");
            return TCL_ERROR;
        }
        attrs = node->firstAttr;
        while (attrs && strcmp(attrs->nodeName, &(method[1]))) {
            attrs = attrs->nextSibling;
        }
        if (attrs) {
            SetResult(attrs->nodeValue);
        } else {
            if (objc == 3) {
                SetResult(Tcl_GetString(objv[2]));
            } else {
                Tcl_ResetResult(interp);
                Tcl_AppendResult(interp, "Attribute \"", &(method[1]),
                                 "\" not found!", NULL);
                return TCL_ERROR;
            }
        }
        return TCL_OK;
    }

    /*----------------------------------------------------------------------
    |   node may have been deleted in the meantime by some other 
    |   thread operating on the tree, so check this fact before.
    |
    \---------------------------------------------------------------------*/

    if (node->nodeFlags & IS_DELETED) {
        SetResult("node has been deleted");
        return TCL_ERROR;
    }

    /*----------------------------------------------------------------------
    |   dispatch the node object method
    |
    \---------------------------------------------------------------------*/
    switch ((enum nodeMethod)methodIndex) {

        case m_toXPath:
            CheckArgs(2,2,2,"");
            str = xpathNodeToXPath(node);
            SetResult (str);
            FREE (str);
            return TCL_OK;

        case m_xslt:
            CheckArgs(3,9,2, "?-parameters parameterList? "
                      "?-ignoreUndeclaredParameters? ?-xsltmessagecmd cmd? "
                      "<xsltDocNode> ?objVar?");
            objv += 2; objc -= 2;
            return applyXSLT(node, interp, NULL, objc, objv);

        case m_selectNodes:
            return tcldom_selectNodes (interp, node, --objc, ++objv);

        case m_find:
            CheckArgs(4,5,2,"attrName attrVal ?nodeObjVar?");
            attr_name = Tcl_GetStringFromObj(objv[2], NULL);
            attr_val  = Tcl_GetStringFromObj(objv[3], &length);
            return tcldom_returnNodeObj
                (interp, tcldom_find(node, attr_name, attr_val, length),
                 (objc == 5), (objc == 5) ? objv[4] : NULL);

        case m_child:
            CheckArgs(3,6,2,"instance|all ?type? ?attr value?");
            return tcldom_xpointerSearch(interp, XP_CHILD, node, objc, objv);

        case m_descendant:
            CheckArgs(3,6,2,"instance|all ?type? ?attr value?");
            return tcldom_xpointerSearch(interp, XP_DESCENDANT,node,objc,objv);

        case m_ancestor:
            CheckArgs(3,6,2,"instance|all ?type? ?attr value?");
            return tcldom_xpointerSearch(interp, XP_ANCESTOR, node,objc,objv);

        case m_fsibling:
            CheckArgs(3,6,2,"instance|all ?type? ?attr value?");
            return tcldom_xpointerSearch(interp, XP_FSIBLING, node,objc,objv);

        case m_psibling:
            CheckArgs(3,6,2,"instance|all ?type? ?attr value?");
            return tcldom_xpointerSearch(interp, XP_PSIBLING, node,objc,objv);

        case m_root:
            CheckArgs(2,3,2,"?nodeObjVar?");
            while (node->parentNode) {
                node = node->parentNode;
            }
            return tcldom_returnNodeObj(interp, node, (objc == 3),
                                        (objc == 3) ? objv[2] : NULL);

        case m_text:
            CheckArgs(2,2,2,"");
            if (node->nodeType != ELEMENT_NODE) {
                SetResult("NOT_AN_ELEMENT");
                return TCL_ERROR;
            }
            Tcl_ResetResult(interp);
            child = node->firstChild;
            while (child) {
                if ((child->nodeType == TEXT_NODE) ||
                    (child->nodeType == CDATA_SECTION_NODE)) {
                    Tcl_AppendToObj(Tcl_GetObjResult(interp),
                                     ((domTextNode*)child)->nodeValue,
                                     ((domTextNode*)child)->valueLength);
                }
                child = child->nextSibling;
            }
            return TCL_OK;

        case m_attributes:
            CheckArgs(2,3,2,"?nameFilter?");
            if (node->nodeType != ELEMENT_NODE) {
                SetResult("");
                return TCL_OK;
            }
            if (objc == 3) {
                filter = Tcl_GetString(objv[2]);
            } else {
                filter = "*";
            }
            Tcl_ResetResult(interp);
            resultPtr = Tcl_GetObjResult(interp);

            attrs = node->firstAttr;
            while (attrs != NULL) {
                if (Tcl_StringMatch((char*)attrs->nodeName, filter)) {

                    if (attrs->namespace == 0) {
                        namePtr = Tcl_NewStringObj((char*)attrs->nodeName, -1);
                    } else {
                        domSplitQName((char*)attrs->nodeName, prefix, 
                                      &localName);
                        mobjv[0] = Tcl_NewStringObj((char*)localName, -1);
                        mobjv[1] = Tcl_NewStringObj(
                            domNamespacePrefix((domNode*)attrs), -1
                            );
                        mobjv[2] = Tcl_NewStringObj(
                            domNamespaceURI((domNode*)attrs), -1
                            );
                        namePtr  = Tcl_NewListObj(3, mobjv);
                    }
                    result = Tcl_ListObjAppendElement(interp, resultPtr, 
                                                      namePtr);
                    if (result != TCL_OK) {
                        Tcl_DecrRefCount(namePtr);
                        return result;
                    }
                }
                attrs = attrs->nextSibling;
            }
            break;

        case m_asList:
            CheckArgs(2,2,2,"");
            Tcl_SetObjResult(interp, tcldom_treeAsTclList(interp, node));
            break;

        case m_asXML:
            Tcl_ResetResult(interp);
            if (serializeAsXML(node, interp, objc, objv) != TCL_OK) {
                return TCL_ERROR;
            }
            break;

        case m_asHTML:
            Tcl_ResetResult(interp);
            if (serializeAsHTML(node, interp, objc, objv) != TCL_OK) {
                return TCL_ERROR;
            }
            break;

        case m_getAttribute:
            CheckArgs(3,4,2,"attrName ?defaultValue?");
            if (node->nodeType != ELEMENT_NODE) {
                SetResult("NOT_AN_ELEMENT : there are no attributes");
                return TCL_ERROR;
            }
            attr_name = Tcl_GetString(objv[2]);
            attrs = node->firstAttr;
            while(attrs && strcmp(attrs->nodeName, attr_name)) {
                attrs = attrs->nextSibling;
            }
            if (attrs) {
                SetResult(attrs->nodeValue);
                return TCL_OK;
            }
            if (objc == 4) {
                SetResult(Tcl_GetString(objv[3]));
                return TCL_OK;
            } else {
                Tcl_ResetResult(interp);
                Tcl_AppendResult(interp, "Attribute \"", attr_name,
                                 "\" not found!", NULL);
                return TCL_ERROR;
            }
            break;

        case m_getAttributeNS:
            CheckArgs(4,4,2,"uri localName");
            if (node->nodeType != ELEMENT_NODE) {
                SetResult("NOT_AN_ELEMENT : there are no attributes");
                return TCL_ERROR;
            }
            uri = Tcl_GetString(objv[2]);
            localName = Tcl_GetString(objv[3]);
            attrs = domGetAttributeNodeNS(node, uri, localName);
            if (attrs) {
                SetResult(attrs->nodeValue);
                return TCL_OK;
            } 
            sprintf(tmp,"attribute with localName %80.80s not found!",localName);
            SetResult(tmp);
            return TCL_ERROR;

        case m_setAttribute:
            if (node->nodeType != ELEMENT_NODE) {
                SetResult("NOT_AN_ELEMENT : there are no attributes");
                return TCL_ERROR;
            }
            if ((objc < 4) || ((objc % 2)!=0)) {
                SetResult("attrName value pairs expected");
                return TCL_ERROR;
            }
            for ( i = 2;  i < objc; ) {
                attr_name = Tcl_GetString(objv[i++]);
                CheckName (interp, attr_name, "attribute", 0);
                attr_val  = Tcl_GetString(objv[i++]);
                CheckText (interp, attr_val, "attribute");
                domSetAttribute(node, attr_name, attr_val);
            }
            return tcldom_returnNodeObj(interp, node, 0, NULL);

        case m_setAttributeNS:
            if (node->nodeType != ELEMENT_NODE) {
                SetResult("NOT_AN_ELEMENT : there are no attributes");
                return TCL_ERROR;
            }
            if ((objc < 5) || (((objc - 2) % 3) != 0)) {
                SetResult("uri attrName value triples expected");
                return TCL_ERROR;
            }
            for (i = 2; i < objc;) {
                uri       = Tcl_GetString(objv[i++]);
                attr_name = Tcl_GetString(objv[i++]);
                CheckName (interp, attr_name, "full qualified attribute", 1);
                attr_val  = Tcl_GetString(objv[i++]);
                CheckText (interp, attr_val, "attribute");
                attrs = domSetAttributeNS(node, attr_name, attr_val, uri, 0);
                if (!attrs) {
                    if (uri[0]) {
                        SetResult("An attribute in a namespace "
                                  "must have a prefix");
                    } else {
                        SetResult("For all prefixed attributes with prefixes "
                                  "other than 'xml' or 'xmlns' "
                                  "you have to provide a namespace URI");
                    }
                    return TCL_ERROR;
                }
            }
            return tcldom_returnNodeObj(interp, node, 0, NULL);

        case m_hasAttribute:
            CheckArgs(3,3,2,"attrName");
            if (node->nodeType != ELEMENT_NODE) {		
                SetResult("NOT_AN_ELEMENT : there are no attributes");
                return TCL_ERROR;
            }
            attr_name = Tcl_GetString(objv[2]);
            attrs = node->firstAttr;
            while (attrs && strcmp(attrs->nodeName, attr_name)) {
                attrs = attrs->nextSibling;
            }
            if (attrs) {
                SetResult("1");
                return TCL_OK;
            }
            SetResult("0");
            return TCL_OK;

        case m_hasAttributeNS:
            CheckArgs(4,4,2,"uri localName");
            if (node->nodeType != ELEMENT_NODE) {		
                SetResult("NOT_AN_ELEMENT : there are no attributes");
                return TCL_ERROR;
            }
            uri = Tcl_GetString(objv[2]);
            localName = Tcl_GetString(objv[3]);
            attrs = node->firstAttr;
            while (attrs) {
                domSplitQName(attrs->nodeName, prefix, &nsStr);
                if (!strcmp(localName,nsStr)) {
                    ns = domGetNamespaceByIndex(node->ownerDocument, 
                                                attrs->namespace);
                    if (ns && !strcmp(ns->uri, uri)) {
                        SetResult("1");
                        return TCL_OK;
                    }
                }
                attrs = attrs->nextSibling;
            }
            SetResult("0");
            return TCL_OK;

        case m_removeAttribute:
            CheckArgs(3,3,2,"attrName");
            if (node->nodeType != ELEMENT_NODE) {		
                SetResult("NOT_AN_ELEMENT : there are no attributes");
                return TCL_ERROR;
            }
            attr_name = Tcl_GetString(objv[2]);
            result = domRemoveAttribute(node, attr_name);
            if (result) {
                SetResult("can't remove attribute '");
                AppendResult(attr_name);
                AppendResult("'");
                return TCL_ERROR;
            }
            return tcldom_returnNodeObj(interp, node, 0, NULL);

        case m_removeAttributeNS:
            CheckArgs(4,4,2,"uri attrName");
            if (node->nodeType != ELEMENT_NODE) {		
                SetResult("NOT_AN_ELEMENT : there are no attributes");
                return TCL_ERROR;
            }
            uri = Tcl_GetString(objv[2]);
            localName = Tcl_GetString(objv[3]);
            result = domRemoveAttributeNS(node, uri, localName);
            if (result < 0) {
                SetResult("can't remove attribute with localName '");
                AppendResult(localName);
                AppendResult("'");
                return TCL_ERROR;
            }
            return tcldom_returnNodeObj(interp, node, 0, NULL);

        case m_nextSibling:
            CheckArgs(2,3,2,"?nodeObjVar?");
            return tcldom_returnNodeObj(interp, node->nextSibling,
                                        (objc == 3), 
                                        (objc == 3) ? objv[2] : NULL);
        case m_previousSibling:
            CheckArgs(2,3,2,"?nodeObjVar?");
            return tcldom_returnNodeObj(interp, node->previousSibling,
                                        (objc == 3),
                                        (objc == 3) ? objv[2] : NULL);
        case m_firstChild:
            CheckArgs(2,3,2,"?nodeObjVar?");
            if (node->nodeType == ELEMENT_NODE) {
                return tcldom_returnNodeObj(interp, node->firstChild,
                                            (objc == 3),
                                            (objc == 3) ? objv[2] : NULL);
            }
            return tcldom_returnNodeObj(interp, NULL, (objc == 3),
                                        (objc == 3) ? objv[2] : NULL);
        case m_lastChild:
            CheckArgs(2,3,2,"?nodeObjVar?");
            if (node->nodeType == ELEMENT_NODE) {
                return tcldom_returnNodeObj(interp, node->lastChild,
                                            (objc == 3),
                                            (objc == 3) ? objv[2] : NULL);
            }
            return tcldom_returnNodeObj(interp, NULL, (objc == 3),
                                        (objc == 3) ? objv[2] : NULL);
        case m_parentNode:
            CheckArgs(2,3,2,"?nodeObjVar?");
            return tcldom_returnNodeObj(interp, node->parentNode, (objc == 3),
                                        (objc == 3) ? objv[2] : NULL);
        case m_appendFromList:
            CheckArgs(3,3,2,"list");
            return tcldom_appendFromTclList(interp, node, objv[2]);

        case m_appendFromScript:
            CheckArgs(3,3,2,"script");
            if (nodecmd_appendFromScript(interp, node, objv[2]) != TCL_OK) {
                return TCL_ERROR;
            }
            return tcldom_returnNodeObj(interp, node, 0, NULL);

        case m_insertBeforeFromScript:
            CheckArgs(4,4,2, "script refChild");
            nodeName = Tcl_GetString (objv[3]);
            if (nodeName[0] == '\0') {
                refChild = NULL;
            } else {
                refChild = tcldom_getNodeFromName (interp, nodeName, &errMsg);
                if (refChild == NULL) {
                    SetResult ( errMsg );
                    return TCL_ERROR;
                }
            }
            if (nodecmd_insertBeforeFromScript(interp, node, objv[2], refChild)
                != TCL_OK) {
                return TCL_ERROR;
            }
            return tcldom_returnNodeObj (interp, node, 0, NULL);
            
        case m_appendXML:
            CheckArgs(3,3,2,"xmlString");
            return tcldom_appendXML(interp, node, objv[2]);

        case m_appendChild:
            CheckArgs(3,3,2,"nodeToAppend");
            nodeName = Tcl_GetString(objv[2]);
            child = tcldom_getNodeFromName(interp, nodeName, &errMsg);
            if (child == NULL) {
                SetResult(errMsg);
                return TCL_ERROR;
            }
            exception = domAppendChild (node, child);
            if (exception != OK) {
                SetResult(domException2String(exception));
                return TCL_ERROR;
            }
            return tcldom_returnNodeObj(interp, child, 0, NULL);

        case m_cloneNode:
            CheckArgs(2,3,2,"?-deep?");
            if (objc == 3) {
                if (!strcmp(Tcl_GetString(objv[2]), "-deep")) {
                    return tcldom_returnNodeObj(interp, domCloneNode(node, 1),
                                                0, NULL);
                }
                SetResult("unknown option! Options: ?-deep? ");
                return TCL_ERROR;
            }
            return tcldom_returnNodeObj(interp, domCloneNode(node, 0), 0, NULL);

        case m_removeChild:
            CheckArgs(3,3,2,"childToRemove");
            nodeName = Tcl_GetString(objv[2]);
            child = tcldom_getNodeFromName(interp, nodeName, &errMsg);
            if (child == NULL) {
                SetResult(errMsg);
                return TCL_ERROR;
            }
            exception = domRemoveChild (node, child);
            if (exception != OK) {
                SetResult (domException2String (exception));
                return TCL_ERROR;
            }
            return tcldom_returnNodeObj(interp, child, 0, NULL);

        case m_insertBefore:
            CheckArgs(4,4,2,"childToInsert refChild");
            nodeName = Tcl_GetString(objv[2]);
            child = tcldom_getNodeFromName(interp, nodeName, &errMsg);
            if (child == NULL) {
                SetResult(errMsg);
                return TCL_ERROR;
            }

            nodeName = Tcl_GetString (objv[3]);
            if (nodeName[0] == '\0') {
                refChild = NULL;
            } else {
                refChild = tcldom_getNodeFromName (interp, nodeName, &errMsg);
                if (refChild == NULL) {
                    SetResult ( errMsg );
                    return TCL_ERROR;
                }
            }
            exception = domInsertBefore(node, child, refChild);
            if (exception != OK) {
                SetResult(domException2String(exception));
                return TCL_ERROR;
            }
            return tcldom_returnNodeObj(interp, child, 0, NULL);

        case m_replaceChild:
            CheckArgs(4,4,2,"new old");
            nodeName = Tcl_GetString(objv[2]);
            child = tcldom_getNodeFromName(interp, nodeName, &errMsg);
            if (child == NULL) {
                SetResult(errMsg);
                return TCL_ERROR;
            }

            nodeName = Tcl_GetString(objv[3]);
            oldChild = tcldom_getNodeFromName(interp, nodeName, &errMsg);
            if (oldChild == NULL) {
                SetResult(errMsg);
                return TCL_ERROR;
            }
            exception = domReplaceChild(node, child, oldChild);
            if (exception != OK) {
                SetResult(domException2String(exception));
                return TCL_ERROR;
            }
            return tcldom_returnNodeObj(interp, oldChild, 0, NULL);

        case m_hasChildNodes:
            CheckArgs(2,2,2,"");
            if (node->nodeType == ELEMENT_NODE) {
                SetIntResult(node->firstChild ? 1 : 0);
            } else {
                SetIntResult(0);
            }
            break;

        case m_childNodes:
            CheckArgs(2,2,2,"");
            resultPtr = Tcl_GetObjResult(interp);
            if (node->nodeType == ELEMENT_NODE) {
                child = node->firstChild;
                while (child != NULL) {
                    tcldom_createNodeObj(interp, child, objCmdName);
                    namePtr = Tcl_NewStringObj(objCmdName, -1);
                    result  = Tcl_ListObjAppendElement(interp, resultPtr,
                                                       namePtr);
                    if (result != TCL_OK) {
                        Tcl_DecrRefCount(namePtr);
                        return result;
                    }
                    child = child->nextSibling;
                }
            }
            break;

        case m_getElementsByTagName:
            CheckArgs(3,3,2,"elementName");
            if (node->nodeType != ELEMENT_NODE) {
                SetResult("Node must be an element node.");
                return TCL_ERROR;
            }
            Tcl_ResetResult(interp);
            return tcldom_getElementsByTagName(interp, Tcl_GetString(objv[2]),
                                               node->firstChild, -1, NULL);

        case m_getElementsByTagNameNS:
            CheckArgs(4,4,2,"uri localname");
            if (node->nodeType != ELEMENT_NODE) {
                SetResult("Node must be an element node.");
                return TCL_ERROR;
            }
            uri = Tcl_GetString(objv[2]);
            str = Tcl_GetString(objv[3]);
            nsIndex = -1;
            if (uri[0] == '*' && uri[1] == '\0') {
                nsIndex = -3;
            } else if (uri[0] == '\0') {
                /* all elements not in a namespace */
                nsIndex = -4;
            } else {
                for (i = 0; i <= node->ownerDocument->nsptr; i++) {
                    if (strcmp (node->ownerDocument->namespaces[i]->uri,
                                uri)==0) {
                        if (nsIndex != -1) {
                            /* OK, this is one of the 'degenerated' (though
                               legal) documents, which bind the same URI
                               to different prefixes. */
                            nsIndex = -2;
                            break;
                        }
                        nsIndex = node->ownerDocument->namespaces[i]->index;
                    }
                }
            }
            if (nsIndex == -1) {
                /* There isn't such a namespace declared in this document.
                   Since getElementsByTagNameNS doesn't raise an execption
                   short cut: return empty result */
                Tcl_ResetResult(interp);
                return TCL_OK;
            }
            return tcldom_getElementsByTagName(interp, str, node->firstChild,
                                                nsIndex, uri);
            
        case m_getElementById:
            CheckArgs(3,3,2,"id");
            if (node->ownerDocument->ids) {
                str = Tcl_GetString(objv[2]);
                h = Tcl_FindHashEntry(node->ownerDocument->ids, str);
                if (h) {
                    domNode *node = Tcl_GetHashValue(h);
                    return tcldom_returnNodeObj(interp, node, 0, NULL);
                }
            }
            SetResult("");
            return TCL_OK;

        case m_nodeName:
            CheckArgs(2,2,2,"");
            if (node->nodeType == ELEMENT_NODE) {
                SetResult((char*)node->nodeName);
            } else
            if (node->nodeType == TEXT_NODE) {
                SetResult("#text");
            } else
            if (node->nodeType == PROCESSING_INSTRUCTION_NODE) {
                domProcessingInstructionNode *dpn;
                dpn = (domProcessingInstructionNode *)node;
                Tcl_SetStringObj(Tcl_GetObjResult(interp),
                                 dpn->targetValue, dpn->targetLength);
            } else 
            if (node->nodeType == COMMENT_NODE) {
                SetResult("#comment");
            } else 
            if (node->nodeType == CDATA_SECTION_NODE) {
                SetResult("#cdata-section");
            } else {
                SetResult("");
            }
            break;

        case m_nodeValue:
            CheckArgs(2,3,2,"?newValue?");
            if (node->nodeType == ELEMENT_NODE) {
                Tcl_SetStringObj(Tcl_GetObjResult(interp), "", 0);
            } else if (node->nodeType == PROCESSING_INSTRUCTION_NODE) {
                domProcessingInstructionNode *dpn;
                dpn = (domProcessingInstructionNode *)node;
                Tcl_SetStringObj(Tcl_GetObjResult(interp),
                                 dpn->dataValue, dpn->dataLength);
            } else {
                domTextNode *dtn;
                dtn = (domTextNode*)node;
                Tcl_SetStringObj(Tcl_GetObjResult(interp),
                                 dtn->nodeValue, dtn->valueLength);
            }
            if (objc == 3) {
                str = Tcl_GetStringFromObj(objv[2], &length);
                switch (node->nodeType) {
                case TEXT_NODE: CheckText (interp, str, "text"); break;
                case COMMENT_NODE: CheckComment (interp, str); break;
                case CDATA_SECTION_NODE: CheckCDATA (interp, str); break;
                default: break; /* Do nothing */
                }
                exception = domSetNodeValue(node, str, length);
                if (exception != OK) {
                    SetResult(domException2String(exception));
                    return TCL_ERROR;
                }
            }
            break;

        case m_nodeType:
            CheckArgs(2,2,2,"");
            switch (node->nodeType) {
               case ELEMENT_NODE:
                    SetResult("ELEMENT_NODE");
                    break;
               case ATTRIBUTE_NODE:
                    SetResult("ATTRIBUTE_NODE");
                    break;
               case TEXT_NODE:
                    SetResult("TEXT_NODE");
                    break;
               case CDATA_SECTION_NODE:
                    SetResult("CDATA_SECTION_NODE");
                    break;
               case COMMENT_NODE:
                    SetResult("COMMENT_NODE");
                    break;
	       case PROCESSING_INSTRUCTION_NODE:
                    SetResult("PROCESSING_INSTRUCTION_NODE");
                    break;
               default:
                    SetResult("unknown nodeType!");
                    return TCL_ERROR;
            }
            break;

        case m_prefix:
            CheckArgs(2,2,2,"");
            nsStr = domNamespacePrefix(node);
            if (nsStr) {
                SetResult(nsStr);
            } else {
                SetResult("");
            }
            return TCL_OK;

        case m_namespaceURI:
            CheckArgs(2,2,2,"");
            nsStr = domNamespaceURI(node);
            if (nsStr) {
                SetResult(nsStr);
            } else {
                SetResult("");
            }
            return TCL_OK;

        case m_localName:
            CheckArgs(2,2,2,"");
            if (node->nodeType == ELEMENT_NODE) {
                if (node->namespace != 0) {
                    SetResult(domGetLocalName((char*)node->nodeName));
                    break;
                }
            }
            SetResult("");
            break;

        case m_ownerDocument:
            CheckArgs(2,3,2,"?docObjVar?");
            return tcldom_returnDocumentObj(interp, node->ownerDocument,
                                            (objc == 3),
                                            (objc == 3) ? objv[2] : NULL, 0, 
                                            1);
        case m_target:
            CheckArgs(2,2,2,"");
            if (node->nodeType != PROCESSING_INSTRUCTION_NODE) {
                SetResult("not a PROCESSING_INSTRUCTION_NODE!");
                return TCL_ERROR;
            } else {
                domProcessingInstructionNode *dpn;
                dpn = (domProcessingInstructionNode *)node;
                Tcl_SetStringObj(Tcl_GetObjResult(interp), 
                                 dpn->targetValue, dpn->targetLength);
            }
            break;

        case m_delete:
            CheckArgs(2,2,2,"");
            domDeleteNode(node, tcldom_deleteNode, interp);
            break;

        case m_data:
            CheckArgs(2,2,2,"");
            if (node->nodeType == PROCESSING_INSTRUCTION_NODE) {
                domProcessingInstructionNode *dpn;
                dpn = (domProcessingInstructionNode*)node;
                Tcl_SetStringObj(Tcl_GetObjResult(interp),
                                 dpn->dataValue, dpn->dataLength);
            } else
            if (   node->nodeType == TEXT_NODE 
                || node->nodeType == CDATA_SECTION_NODE
                || node->nodeType == COMMENT_NODE) {
                domTextNode *dtn;
                dtn = (domTextNode*)node;
                Tcl_SetStringObj(Tcl_GetObjResult(interp),
                                 dtn->nodeValue, dtn->valueLength);
            } else {
                SetResult("not a "
                          "TEXT_NODE / "
                          "CDATA_SECTION_NODE / "
                          "COMMENT_NODE / "
                          "PROCESSING_INSTRUCTION_NODE !");
                return TCL_ERROR;
            }
            break;

        case m_getLine:
            CheckArgs(2,2,2,"");
            if (domGetLineColumn(node, &line, &column) < 0) {
                SetResult("no line/column information available!");
                return TCL_ERROR;
            }
            SetIntResult(line);
            break;

        case m_getColumn:
            CheckArgs(2,2,2,"");
            if (domGetLineColumn (node, &line, &column) < 0) {
                SetResult("no line/column information available!");
                return TCL_ERROR;
            }
            SetIntResult(column);
            break;

        case m_getBaseURI:
            CheckArgs(2,2,2,"");
            /* fall thru */

        case m_baseURI:    
            CheckArgs(2,3,2,"?URI?");
            if (objc == 3) {
                h = Tcl_CreateHashEntry (node->ownerDocument->baseURIs, 
                                         (char *) node, &hnew);
                if (!hnew) {
                    FREE (Tcl_GetHashValue (h));
                }
                Tcl_SetHashValue (h, tdomstrdup (Tcl_GetString (objv[2])));
                node->nodeFlags |= HAS_BASEURI;
                SetResult (Tcl_GetString (objv[2]));
            } else {
                nsStr = findBaseURI(node);
                if (!nsStr) {
                    SetResult("");
                } else {
                    SetResult(nsStr);
                }
            }
            break;

        case m_disableOutputEscaping:
            CheckArgs(2,3,2,"?boolean?");
            if (node->nodeType != TEXT_NODE) {
                SetResult("not a TEXT_NODE!");
                return TCL_ERROR;
            }
            SetIntResult(
                (((node->nodeFlags & DISABLE_OUTPUT_ESCAPING) == 0) ? 0 : 1));
            if (objc == 3) {
                if (Tcl_GetBooleanFromObj(interp, objv[2], &bool) != TCL_OK) {
                    return TCL_ERROR;
                }
                if (bool) {
                    node->nodeFlags |= DISABLE_OUTPUT_ESCAPING;
                } else {
                    node->nodeFlags &= (~DISABLE_OUTPUT_ESCAPING);
                }
            }
            break;

        case m_precedes:
            CheckArgs(3,3,2, "node");
            nodeName = Tcl_GetString(objv[2]);
            refNode = tcldom_getNodeFromName(interp, nodeName, &errMsg);
            if (refNode == NULL) {
                SetResult(errMsg);
                return TCL_ERROR;
            }
            if (node->ownerDocument != refNode->ownerDocument) {
                SetResult("Cannot compare the relative order of nodes "
                          "out of different documents.");
                return TCL_ERROR;
            }
            if (((node->parentNode == NULL) 
                 && (node != node->ownerDocument->documentElement)
                 && (node != node->ownerDocument->rootNode))
                || 
                ((refNode->parentNode == NULL)
                 && (refNode != refNode->ownerDocument->documentElement)
                 && (refNode != refNode->ownerDocument->rootNode))) {
                SetResult("Cannot compare the relative order of a node "
                          "with a node out of the fragment list.");
                return TCL_ERROR;
            }
            SetBooleanResult (domPrecedes (node, refNode));
            break;

        case m_asText:
            CheckArgs (2,2,2, "");
            str = xpathGetStringValue(node, &length);
            Tcl_SetStringObj(Tcl_GetObjResult(interp), str, length);
            FREE (str);
            return TCL_OK;

        case m_normalize:
            CheckArgs (2,3,2, "?-forXPath?");
            bool = 0;
            if (objc == 3) {
                if (strcmp (Tcl_GetString(objv[2]), "-forXPath") == 0) {
                    bool = 1;
                } else {
                    SetResult("unknown option! Options: ?-forXPath?");
                    return TCL_ERROR;
                }
            }
            domNormalize (node, bool, tcldom_deleteNode, interp);
            return TCL_OK;

        TDomThreaded(
        case m_writelock:
            CheckArgs(3,3,2,"script");
            return tcldom_EvalLocked(interp, (Tcl_Obj**)objv, 
                                     node->ownerDocument, LOCK_WRITE);
        case m_readlock:
            CheckArgs(3,3,2,"script");
            return tcldom_EvalLocked(interp, (Tcl_Obj**)objv, 
                                     node->ownerDocument, LOCK_READ);
        )
    }
    return TCL_OK;
}


/*----------------------------------------------------------------------------
|   tcldom_DocObjCmd
|
\---------------------------------------------------------------------------*/
int tcldom_DocObjCmd (
    ClientData  clientData,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    *CONST objv[]
)
{
    GetTcldomTSD()

    domDeleteInfo       * dinfo;
    domDocument         * doc;
    char                * method, *tag, *data, *target, *uri, tmp[100];
    char                * str, *docName, *errMsg;
    int                   methodIndex, result, data_length, target_length, i;
    int                   nsIndex, forXPath, bool, setDocumentElement = 0;
    int                   restoreDomCreateCmdMode = 0;
    domNode             * n;
    Tcl_CmdInfo           cmdInfo;
    Tcl_Obj             * mobjv[MAX_REWRITE_ARGS];

    static CONST84 char *docMethods[] = {
        "documentElement", "getElementsByTagName",       "delete",
        "createElement",   "createCDATASection",         "createTextNode",
        "createComment",   "createProcessingInstruction",
        "createElementNS", "getDefaultOutputMethod",     "asXML",
        "asHTML",          "getElementsByTagNameNS",     "xslt", 
        "publicId",        "systemId",                   "internalSubset",
        "toXSLTcmd",       "asText",                     "normalize",
        "indent",          "omit-xml-declaration",       "encoding",
        "standalone",      "mediaType",                  "nodeType",
        "cdataSectionElements",
        "selectNodesNamespaces",
        "renameNode",      "deleteXPathCache", 
        /* The following methods will be dispatched to tcldom_NodeObjCmd */
        "getElementById",  "firstChild",                 "lastChild",
        "appendChild",     "removeChild",                "hasChildNodes",
        "childNodes",      "ownerDocument",              "insertBefore",
        "replaceChild",    "appendFromList",             "appendXML",
        "selectNodes",     "baseURI",                    "appendFromScript",
        "insertBeforeFromScript",
#ifdef TCL_THREADS
        "readlock",        "writelock",                  "renumber",
#endif
        NULL
    };
    enum docMethod {
        m_documentElement,  m_getElementsByTagName,       m_delete,
        m_createElement,    m_createCDATASection,         m_createTextNode,
        m_createComment,    m_createProcessingInstruction,
        m_createElementNS,  m_getdefaultoutputmethod,     m_asXML,
        m_asHTML,           m_getElementsByTagNameNS,     m_xslt,
        m_publicId,         m_systemId,                   m_internalSubset,
        m_toXSLTcmd,        m_asText,                     m_normalize,
        m_indent,           m_omitXMLDeclaration,         m_encoding,
        m_standalone,       m_mediaType,                  m_nodeType,
        m_cdataSectionElements,
        m_selectNodesNamespaces,
        m_renameNode,       m_deleteXPathCache,
        /* The following methods will be dispatched to tcldom_NodeObjCmd */
        m_getElementById,   m_firstChild,                 m_lastChild,
        m_appendChild,      m_removeChild,                m_hasChildNodes,
        m_childNodes,       m_ownerDocument,              m_insertBefore,
        m_replaceChild,     m_appendFromList,             m_appendXML,
        m_selectNodes,      m_baseURI,                    m_appendFromScript,
        m_insertBeforeFromScript
#ifdef TCL_THREADS
       ,m_readlock,         m_writelock,                  m_renumber
#endif
    };

    dinfo = (domDeleteInfo*)clientData;
    if (TSD(domCreateCmdMode) == DOM_CREATECMDMODE_AUTO) {
        TSD(dontCreateObjCommands) = 0;
    }
    if (dinfo == NULL) {
        if (objc < 3) {
            SetResult(doc_usage);
            return TCL_ERROR;
        }
        if (TSD(domCreateCmdMode) == DOM_CREATECMDMODE_AUTO) {    
            TSD(dontCreateObjCommands) = 1;
        }
        docName = Tcl_GetString(objv[1]);
        doc = tcldom_getDocumentFromName(interp, docName, &errMsg);
        if (doc == NULL) {
            SetResult(errMsg);
            return TCL_ERROR;
        }
        objc--;
        objv++;
    } else {
        doc = dinfo->document;
    }

    if (objc < 2) {
        SetResult(doc_usage);
        return TCL_ERROR;
    }
    method = Tcl_GetString(objv[1]);
    if (Tcl_GetIndexFromObj(NULL, objv[1], docMethods, "method", 0,
                            &methodIndex) != TCL_OK)
    {
        /*--------------------------------------------------------
        |   try to find method implemented as normal Tcl proc
        \-------------------------------------------------------*/
        sprintf(tmp, "::dom::domDoc::%s", method);
        DBG(fprintf(stderr, "testing %s\n", tmp));
        result = Tcl_GetCommandInfo(interp, tmp, &cmdInfo);
        if (!result) {
            SetResult(doc_usage);
            return TCL_ERROR;
        }
        if (!cmdInfo.isNativeObjectProc) {
            SetResult( "can't access Tcl level method!");
            return TCL_ERROR;
        }
        if (objc >= MAX_REWRITE_ARGS) {
            SetResult("too many args to call Tcl level method!");
            return TCL_ERROR;
        }
        mobjv[0] = objv[1];
        mobjv[1] = objv[0];
        for (i = 2; i < objc; i++) {
            mobjv[i] = objv[i];
        }
        return cmdInfo.objProc(cmdInfo.objClientData, interp, objc, mobjv);
    }

    CheckArgs (2,10,1,dom_usage);
    Tcl_ResetResult (interp);

    /*----------------------------------------------------------------------
    |   dispatch the doc object method
    |
    \---------------------------------------------------------------------*/

    switch ((enum docMethod) methodIndex ) {

        case m_documentElement:
            CheckArgs(2,3,2,"");
            return tcldom_returnNodeObj(interp, doc->documentElement,
                                        (objc == 3), 
                                        (objc == 3) ? objv[2] : NULL);
        case m_getElementsByTagName:
            CheckArgs(3,3,2,"elementName");
            return tcldom_getElementsByTagName(interp, Tcl_GetString(objv[2]),
                                               doc->documentElement, -1, NULL);
        case m_getElementsByTagNameNS:
            CheckArgs(4,4,2,"uri localname");
            uri = Tcl_GetString(objv[2]);
            str = Tcl_GetString(objv[3]);
            nsIndex = -1;
            if (uri[0] == '*' && uri[1] == '\0') {
                nsIndex = -3;
            } else if (uri[0] == '\0') {
                /* all elements not in a namespace i.e. */
                nsIndex = -4;
            } else {
                for (i = 0; i <= doc->nsptr; i++) {
                    if (strcmp(doc->namespaces[i]->uri, uri)==0) {
                        if (nsIndex != -1) {
                            /* OK, this is one of the 'degenerated' (though
                               legal) documents, which bind the same URI
                               to different prefixes. */
                            nsIndex = -2;
                            break;
                        }
                        nsIndex = doc->namespaces[i]->index;
                    }
                }
            }
            if (nsIndex == -1) {
                /* There isn't such a namespace declared in this document.
                   Since getElementsByTagNameNS doesn't raise an execption
                   short cut: return empty result */
                return TCL_OK;
            }
            return tcldom_getElementsByTagName(interp, str,
                                               doc->documentElement, nsIndex,
                                               uri);
        case m_createElement:
            CheckArgs(3,4,2,"elementName ?newObjVar?");
            tag = Tcl_GetString(objv[2]);
            CheckName (interp, tag, "tag", 0);
            n = domNewElementNode(doc, tag, ELEMENT_NODE);
            return tcldom_returnNodeObj(interp, n, (objc == 4),
                                        (objc == 4) ? objv[3] : NULL);

        case m_createElementNS:
            CheckArgs(4,5,2,"elementName uri ?newObjVar?");
            uri = Tcl_GetString(objv[2]);
            tag = Tcl_GetString(objv[3]);
            CheckName (interp, tag, "full qualified tag", 1);
            n = domNewElementNodeNS(doc, tag, uri, ELEMENT_NODE);
            return tcldom_returnNodeObj(interp, n, (objc == 5),
                                        (objc == 5) ? objv[4] : NULL);

        case m_createTextNode:
            CheckArgs(3,4,2,"data ?newObjVar?");
            data = Tcl_GetStringFromObj(objv[2], &data_length);
            CheckText (interp, data, "text");
            n = (domNode*)domNewTextNode(doc, data, data_length, TEXT_NODE);
            return tcldom_returnNodeObj(interp, n, (objc == 4),
                                        (objc == 4) ? objv[3] : NULL);

        case m_createCDATASection:
            CheckArgs(3,4,2,"data ?newObjVar?");
            data = Tcl_GetStringFromObj(objv[2], &data_length);
            CheckCDATA (interp, data);
            n = (domNode*)domNewTextNode(doc, data, data_length, 
                                         CDATA_SECTION_NODE);
            return tcldom_returnNodeObj(interp, n, (objc == 4),
                                        (objc == 4) ? objv[3] : NULL);

        case m_createComment:
            CheckArgs(3,4,2,"data ?newObjVar?");
            data = Tcl_GetStringFromObj(objv[2], &data_length);
            CheckComment(interp, data);
            n = (domNode*)domNewTextNode(doc, data, data_length, COMMENT_NODE);
            return tcldom_returnNodeObj(interp, n, (objc == 4),
                                        (objc == 4) ? objv[3] : NULL);

        case m_createProcessingInstruction:
            CheckArgs(4,5,2,"target data ?newObjVar?");
            target = Tcl_GetStringFromObj(objv[2], &target_length);
            CheckPIName (interp, target);
            data   = Tcl_GetStringFromObj(objv[3], &data_length);
            CheckPIValue (interp, data);
            n = (domNode*)domNewProcessingInstructionNode(doc, target, 
                                                          target_length, data, 
                                                          data_length);
            return tcldom_returnNodeObj(interp, n, (objc == 5),
                                        (objc == 5) ? objv[4] : NULL);

        case m_delete:
            CheckArgs(2,2,2,"");
            if (clientData != NULL) {
                Tcl_DeleteCommand(interp, Tcl_GetString (objv[0]));
            } else {
                tcldom_deleteDoc(interp, doc);
            }
            SetResult("");
            return TCL_OK;

        case m_getdefaultoutputmethod:
            CheckArgs(2,2,2,"");
            if (doc->doctype && doc->doctype->method) {
                SetResult (doc->doctype->method);
            } else {
                SetResult("xml");
            }
            return TCL_OK;

        case m_asXML:
            if (serializeAsXML((domNode*)doc, interp, objc, objv) != TCL_OK) {
                return TCL_ERROR;
            }
            return TCL_OK;

        case m_asHTML:
            if (serializeAsHTML((domNode*)doc, interp, objc, objv)
                != TCL_OK) {
                return TCL_ERROR;
            }
            return TCL_OK;

        case m_xslt:
            CheckArgs(3,9,2, "?-parameters parameterList? "
                      "?-ignoreUndeclaredParameters? "
                      "?-xsltmessagecmd cmd? <xsltDocNode> ?objVar?");
            objv += 2; objc -= 2;
            return applyXSLT((domNode *) doc, interp, NULL, objc, objv);


        case m_toXSLTcmd:
            CheckArgs(2,3,2, "?objVar?");
            return convertToXSLTCmd(doc, interp, (objc == 3),
                                    (objc == 3) ? objv[2] : NULL);
            
        case m_publicId:
            CheckArgs(2,3,2, "?publicID?");
            if (doc->doctype && doc->doctype->publicId) {
                SetResult(doc->doctype->publicId);
            } else {
                SetResult("");
            }
            if (objc == 3) {
                if (!doc->doctype) {
                    doc->doctype = (domDocInfo *)MALLOC(sizeof(domDocInfo));
                    memset(doc->doctype, 0,(sizeof(domDocInfo)));
                } else if (doc->doctype->publicId) {
                    FREE(doc->doctype->publicId);
                }
                doc->doctype->publicId = tdomstrdup(Tcl_GetString(objv[2]));
            }
            return TCL_OK;
            
        case m_systemId:
            CheckArgs(2,3,2, "?systemID?");
            if (doc->doctype && doc->doctype->systemId) {
                SetResult(doc->doctype->systemId);
            } else {
                SetResult("");
            }
            if (objc == 3) {
                if (!doc->doctype) {
                    doc->doctype = (domDocInfo *)MALLOC(sizeof(domDocInfo));
                    memset(doc->doctype, 0,(sizeof(domDocInfo)));
                } else if (doc->doctype->systemId) {
                    FREE(doc->doctype->systemId);
                }
                doc->doctype->systemId = 
                    tdomstrdup(Tcl_GetString(objv[2]));
            }
            return TCL_OK;

        case m_internalSubset:
            CheckArgs(2,3,2, "?internalSubset?");
            if (doc->doctype && doc->doctype->internalSubset) {
                SetResult(doc->doctype->internalSubset);
            } else {
                SetResult("");
            }
            if (objc == 3) {
                if (!doc->doctype) {
                    doc->doctype = (domDocInfo *)MALLOC(sizeof(domDocInfo));
                    memset(doc->doctype, 0,(sizeof(domDocInfo)));
                } else if (doc->doctype->systemId) {
                    FREE(doc->doctype->systemId);
                }
                doc->doctype->internalSubset = 
                    tdomstrdup(Tcl_GetString(objv[2]));
            }
            return TCL_OK;
            
        case m_indent:
            CheckArgs(2,3,2, "?boolean?");
            if (doc->nodeFlags & OUTPUT_DEFAULT_INDENT) {
                SetBooleanResult (1);
            } else {
                SetBooleanResult(0);
            }
            if (objc == 3) {
                if (Tcl_GetBooleanFromObj (interp, objv[2], &bool) != TCL_OK) {
                    return TCL_ERROR;
                }
                if (bool) {
                    doc->nodeFlags |= OUTPUT_DEFAULT_INDENT;
                } else {
                    doc->nodeFlags &= ~OUTPUT_DEFAULT_INDENT;
                }
            }
            return TCL_OK;
            
        case m_omitXMLDeclaration:
            CheckArgs(2,3,2, "?boolean?");
            if (doc->doctype) {
                SetBooleanResult (doc->doctype->omitXMLDeclaration);
            } else {
                SetBooleanResult (1);
            }
            if (objc == 3) {
                if (!doc->doctype) {
                    doc->doctype = (domDocInfo *)MALLOC(sizeof(domDocInfo));
                    memset(doc->doctype, 0,(sizeof(domDocInfo)));
                }
                if (Tcl_GetBooleanFromObj (
                        interp, objv[2], &(doc->doctype->omitXMLDeclaration)
                        ) != TCL_OK) {
                    return TCL_ERROR;
                }
            }
            return TCL_OK;

        case m_encoding:
            CheckArgs(2,3,2, "?value?");
            if (doc->doctype && doc->doctype->encoding) {
                SetResult (doc->doctype->encoding);
            } else {
                SetResult ("");
            }
            if (objc == 3) {
                if (!doc->doctype) {
                    doc->doctype = (domDocInfo *)MALLOC(sizeof(domDocInfo));
                    memset(doc->doctype, 0,(sizeof(domDocInfo)));
                } else {
                    if (doc->doctype->encoding) FREE (doc->doctype->encoding);
                }
                doc->doctype->encoding = tdomstrdup (Tcl_GetString (objv[2]));
            }
            return TCL_OK;
                    
        case m_standalone:
            CheckArgs(2,3,2, "?boolean?");
            if (doc->doctype) {
                SetBooleanResult (doc->doctype->standalone);
            } else {
                SetBooleanResult (0);
            }
            if (objc == 3) {
                if (!doc->doctype) {
                    doc->doctype = (domDocInfo *)MALLOC(sizeof(domDocInfo));
                    memset(doc->doctype, 0,(sizeof(domDocInfo)));
                }
                if (Tcl_GetBooleanFromObj (
                        interp, objv[2], &(doc->doctype->standalone)
                        ) != TCL_OK) {
                    return TCL_ERROR;
                }
            }
            return TCL_OK;

        case m_mediaType:
            CheckArgs(2,3,2, "?value?");
            if (doc->doctype && doc->doctype->mediaType) {
                SetResult (doc->doctype->mediaType);
            } else {
                SetResult ("");
            }
            if (objc == 3) {
                if (!doc->doctype) {
                    doc->doctype = (domDocInfo *)MALLOC(sizeof(domDocInfo));
                    memset(doc->doctype, 0,(sizeof(domDocInfo)));
                } else {
                    if (doc->doctype->mediaType) FREE(doc->doctype->mediaType);
                }
                doc->doctype->mediaType = tdomstrdup (Tcl_GetString (objv[2]));
            }
            return TCL_OK;
                    
        case m_asText:
            CheckArgs (2,2,2,"");
            data = xpathGetStringValue (doc->rootNode, &data_length);
            Tcl_SetStringObj (Tcl_GetObjResult (interp), data, data_length);
            FREE (data);
            return TCL_OK;

        case m_normalize:
            CheckArgs (2,3,2, "?-forXPath?");
            forXPath = 0;
            if (objc == 3) {
                if (strcmp (Tcl_GetString (objv[2]), "-forXPath") == 0) {
                    forXPath = 1;
                } else {
                    SetResult("unknown option! Options: ?-forXPath?");
                    return TCL_ERROR;
                }
            }
            domNormalize(doc->rootNode, forXPath, tcldom_deleteNode, interp);
            return TCL_OK;

        case m_nodeType:
            CheckArgs (2,2,2, "");
            SetResult("DOCUMENT_NODE");
            return TCL_OK;

        case m_cdataSectionElements:
            return cdataSectionElements (doc, interp, objc, objv);

        case m_selectNodesNamespaces:
            return selectNodesNamespaces (doc, interp, objc, objv);

        case m_renameNode:
            return renameNodes (doc, interp, objc, objv);
            
        case m_deleteXPathCache:
            return deleteXPathCache (doc, interp, objc, objv);

        case m_appendChild:
        case m_removeChild:
        case m_insertBefore:
        case m_replaceChild:
        case m_appendFromList:
        case m_appendXML:
        case m_appendFromScript:
        case m_insertBeforeFromScript:
            setDocumentElement = 1;
            /* Fall throuh */
        case m_firstChild:
        case m_lastChild:
        case m_hasChildNodes:
        case m_childNodes:
        case m_ownerDocument:
        case m_selectNodes:
        case m_baseURI:
        case m_getElementById:
            /* We dispatch the method call to tcldom_NodeObjCmd */
            if (TSD(domCreateCmdMode) == DOM_CREATECMDMODE_AUTO) {
                if (dinfo == NULL) {
                    /* tcldom_DocObjCmd was called with a doc token.
                       Since the domCreateCmdMode is 'automatic'
                       and we call tcldom_DocObjCmd with the root node
                       as 'clientData', we temporarily set domCreateCmdMode
                       to 'token', to get token results from that call
                       and later to set it back. */
                    TSD(domCreateCmdMode) = DOM_CREATECMDMODE_TOKENS;
                    restoreDomCreateCmdMode = 1;
                }
            }
            if (tcldom_NodeObjCmd (doc->rootNode, interp, objc, objv) !=
                TCL_OK) {
                if (restoreDomCreateCmdMode) {
                    TSD(domCreateCmdMode) = DOM_CREATECMDMODE_AUTO;
                    TSD(dontCreateObjCommands) = 0;
                }
                return TCL_ERROR;
            }
            if (setDocumentElement) {
                /* The method call may have altered the documentElement. */
                /* There may be even no node anymore */
                domSetDocumentElement (doc);
            }
            if (restoreDomCreateCmdMode) {
                TSD(domCreateCmdMode) = DOM_CREATECMDMODE_AUTO;
                TSD(dontCreateObjCommands) = 0;
            }
            return TCL_OK;
            
        TDomThreaded(
        case m_writelock:
            CheckArgs(3,3,2,"script");
            return tcldom_EvalLocked(interp, (Tcl_Obj**)objv, doc, LOCK_WRITE);

        case m_readlock:
            CheckArgs(3,3,2,"script");
            return tcldom_EvalLocked(interp, (Tcl_Obj**)objv, doc, LOCK_READ);

        case m_renumber:
            CheckArgs(2,2,2,"");
            if (doc->nodeFlags & NEEDS_RENUMBERING) {
                domRenumberTree (doc->rootNode);
                doc->nodeFlags &= ~NEEDS_RENUMBERING;
            }
            return TCL_OK;
        )
    }

    SetResult(doc_usage);
    return TCL_ERROR;
}


/*----------------------------------------------------------------------------
|   tcldom_createDocument
|
\---------------------------------------------------------------------------*/
static
int tcldom_createDocument (
    ClientData  clientData,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    * const objv[]
)
{
    int          setVariable = 0;
    domDocument *doc;
    Tcl_Obj     *newObjName = NULL;


    CheckArgs(2,3,1,"docElemName ?newObjVar?");

    if (objc == 3) {
        newObjName = objv[2];
        setVariable = 1;
    }

    doc = domCreateDocument(interp, NULL, Tcl_GetString(objv[1]));
    if (doc == NULL) {
        return TCL_ERROR;
    }

    return tcldom_returnDocumentObj(interp, doc, setVariable, newObjName, 1,
                                    0);
}

/*----------------------------------------------------------------------------
|   tcldom_createDocumentNode
|
\---------------------------------------------------------------------------*/
static
int tcldom_createDocumentNode (
    ClientData  clientData,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    * const objv[]
)
{
    int          setVariable = 0;
    domDocument *doc;
    Tcl_Obj     *newObjName = NULL;


    CheckArgs(1,2,1,"?newObjVar?");

    if (objc == 2) {
        newObjName = objv[1];
        setVariable = 1;
    }

    doc = domCreateDoc(NULL, 0);

    return tcldom_returnDocumentObj(interp, doc, setVariable, newObjName, 1,
                                    0);
}

/*----------------------------------------------------------------------------
|   tcldom_createDocumentNS
|
\---------------------------------------------------------------------------*/
static
int tcldom_createDocumentNS (
    ClientData  clientData,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    * const objv[]
)
{
    int          setVariable = 0;
    domDocument *doc;
    Tcl_Obj     *newObjName = NULL;


    CheckArgs(3,4,1,"uri docElemName ?newObjVar?");

    if (objc == 4) {
        newObjName = objv[3];
        setVariable = 1;
    }

    doc = domCreateDocument(interp, Tcl_GetString(objv[1]),
                            Tcl_GetString(objv[2]));
    if (doc == NULL) {
        return TCL_ERROR;
    }

    return tcldom_returnDocumentObj(interp, doc, setVariable, newObjName, 1,
                                    0);
}


/*----------------------------------------------------------------------------
|   tcldom_setResultEncoding
|
\---------------------------------------------------------------------------*/
static
int tcldom_setResultEncoding (
    ClientData  clientData,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    * const objv[]
)
{
    GetTcldomTSD()

    TEncoding *encoding;
    char      *encodingName;

    CheckArgs(1,2,1,"?encodingName?");
    if (objc == 1) {
        if (TSD(Encoding_to_8bit) == NULL) {
            Tcl_AppendResult(interp, "UTF-8", NULL);
        } else {
            Tcl_AppendResult(interp, TSD(Encoding_to_8bit->name), NULL);
        }
        return TCL_OK;
    }
    encodingName = Tcl_GetString(objv[1]);
    if ( (strcmp(encodingName, "UTF-8")==0)
       ||(strcmp(encodingName, "UTF8")==0)
       ||(strcmp(encodingName, "utf-8")==0)
       ||(strcmp(encodingName, "utf8")==0)) {

        TSD(Encoding_to_8bit) = NULL;
    } else {
        encoding = tdom_GetEncoding ( encodingName );
        if (encoding == NULL) {
             Tcl_AppendResult(interp, "encoding not found", NULL);
             return TCL_ERROR;
        }
        TSD(Encoding_to_8bit) = encoding;
    }
    return TCL_OK;
}


/*----------------------------------------------------------------------------
|   tcldom_parse
|
\---------------------------------------------------------------------------*/
static
int tcldom_parse (
    ClientData  clientData,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    * const objv[]
)
{
    GetTcldomTSD()

    char        *xml_string, *option, *errStr, *channelId, *baseURI = NULL;
    char        *extResolver = NULL;
    CONST84 char *interpResult;
    int          optionIndex, value, xml_string_len, mode;
    int          ignoreWhiteSpaces   = 1;
    int          takeSimpleParser    = 0;
    int          takeHTMLParser      = 0;
    int          setVariable         = 0;
    int          feedbackAfter       = 0;
    int          useForeignDTD       = 0;
    int          paramEntityParsing  = (int)XML_PARAM_ENTITY_PARSING_ALWAYS;
    domDocument *doc;
    Tcl_Obj     *newObjName = NULL;
    XML_Parser   parser;
    Tcl_Channel  chan = (Tcl_Channel) NULL;

    static CONST84 char *parseOptions[] = {
        "-keepEmpties",           "-simple",        "-html",
        "-feedbackAfter",         "-channel",       "-baseurl",
        "-externalentitycommand", "-useForeignDTD", "-paramentityparsing",
        NULL
    };
    enum parseOption {
        o_keepEmpties,            o_simple,         o_html,
        o_feedbackAfter,          o_channel,        o_baseurl,
        o_externalentitycommand,  o_useForeignDTD,  o_paramentityparsing
    };

    static CONST84 char *paramEntityParsingValues[] = {
        "always",
        "never",
        "notstandalone",
        (char *) NULL
    };
    enum paramEntityParsingValue {
        EXPAT_PARAMENTITYPARSINGALWAYS,
        EXPAT_PARAMENTITYPARSINGNEVER,
        EXPAT_PARAMENTITYPARSINGNOTSTANDALONE
    };
    
    while (objc > 1) {
        option = Tcl_GetString(objv[1]);
        if (option[0] != '-') {
            break;
        }
        if (Tcl_GetIndexFromObj(interp, objv[1], parseOptions, "option", 0,
                                 &optionIndex) != TCL_OK) {
            return TCL_ERROR;
        }

        switch ((enum parseOption) optionIndex) {

        case o_keepEmpties:
            ignoreWhiteSpaces = 0;
            objv++;  objc--; continue;

        case o_simple:
            takeSimpleParser = 1;
            objv++;  objc--; continue;

        case o_html:
            takeSimpleParser = 1;
            takeHTMLParser = 1;
            objv++;  objc--; continue;
            
        case o_feedbackAfter:
            objv++; objc--;
            if (objc > 1) {
                if (Tcl_GetIntFromObj(interp, objv[1], &feedbackAfter)
                    != TCL_OK) {
                    SetResult("-feedbackAfter must have an integer argument");
                    return TCL_ERROR;
                }
            } else {
                SetResult("The \"dom parse\" option \"-feedbackAfter\" requires"
                          " an integer as argument.");
                return TCL_ERROR;
            }
            objv++; objc--;
            continue;

        case o_channel:
            objv++; objc--;
            if (objc > 1) {
                channelId = Tcl_GetString(objv[1]);
            } else {
                SetResult("The \"dom parse\" option \"-channel\" "
                          "requires a tcl channel as argument.");
                return TCL_ERROR;
            }
            chan = Tcl_GetChannel(interp, channelId, &mode);
            if (chan == (Tcl_Channel) NULL) {
                return TCL_ERROR;
            }
            if ((mode & TCL_READABLE) == 0) {
                Tcl_AppendResult(interp, "channel \"", channelId,
                                "\" wasn't opened for reading", (char *) NULL);
                return TCL_ERROR;
            }
            objv++; objc--;
            continue;

        case o_baseurl:
            objv++; objc--;
            if (objc > 1) {
                baseURI = Tcl_GetString(objv[1]);
            } else {
                SetResult("The \"dom parse\" option \"-baseurl\" "
                          "requires the base URL of the document "
                          "to parse as argument.");
                return TCL_ERROR;
            }
            objv++; objc--;
            continue;

        case o_externalentitycommand:
            objv++; objc--;
            if (objc > 1) {
                extResolver = tdomstrdup (Tcl_GetString (objv[1]));
            } else {
                SetResult("The \"dom parse\" option \"-externalentitycommand\" "
                          "requires a script as argument.");
                return TCL_ERROR;
            }
            objv++; objc--;
            continue;

        case o_useForeignDTD:
            objv++; objc--;
            if (objc > 1) {
                if (Tcl_GetBooleanFromObj(interp, objv[1], &useForeignDTD)
                    != TCL_OK) {
                    return TCL_ERROR;
                }
            } else {
                SetResult(dom_usage);
                return TCL_ERROR;
            }
            objv++; objc--;
            continue;

        case o_paramentityparsing:
            if (objc > 2) {
                if (Tcl_GetIndexFromObj(interp, objv[2], 
                                        paramEntityParsingValues, "value", 0, 
                                        &value) != TCL_OK) {
                    return TCL_ERROR;
                }
                switch ((enum paramEntityParsingValue) value) {
                case EXPAT_PARAMENTITYPARSINGALWAYS:
                    paramEntityParsing = (int) XML_PARAM_ENTITY_PARSING_ALWAYS;
                    break;
                case EXPAT_PARAMENTITYPARSINGNEVER:
                    paramEntityParsing = (int) XML_PARAM_ENTITY_PARSING_NEVER;
                    break;
                case EXPAT_PARAMENTITYPARSINGNOTSTANDALONE:
                    paramEntityParsing = 
                        (int) XML_PARAM_ENTITY_PARSING_UNLESS_STANDALONE;
                    break;
                }
            } else {
                SetResult("-paramEntityParsing requires 'always', 'never' "
                          "or 'notstandalone' as argument");
                return TCL_ERROR;
            }
            objv++; objc--;
            objv++; objc--;
            continue;
        }
    }
    
    if (chan == NULL) {
        if (objc < 2) {
            SetResult(dom_usage);
            return TCL_ERROR;
        }
        xml_string = Tcl_GetStringFromObj( objv[1], &xml_string_len);
        if (objc == 3) {
            newObjName = objv[2];
            setVariable = 1;
        }
    } else {
        if (objc > 2) {
            SetResult(dom_usage);
            return TCL_ERROR;
        }
        xml_string = NULL;
        xml_string_len = 0;
        if (takeSimpleParser || takeHTMLParser) {
            Tcl_AppendResult(interp, "simple and/or HTML parser(s) "
                             " don't support channel reading", NULL);
            return TCL_ERROR;
        }
        if (objc == 2) {
            newObjName = objv[1];
            setVariable = 1;
        }
    }

    if (takeSimpleParser) {
        char s[50];
        int  byteIndex, i;

        errStr = NULL;

        if (takeHTMLParser) {
            doc = HTML_SimpleParseDocument(xml_string, ignoreWhiteSpaces,
                                           &byteIndex, &errStr);
        } else {
            doc = XML_SimpleParseDocument(xml_string, ignoreWhiteSpaces,
                                          baseURI, extResolver,
                                          &byteIndex, &errStr);
        }
        if (errStr != NULL) {
            domFreeDocument (doc, NULL, interp);

            Tcl_ResetResult(interp);
            sprintf(s, "%d", byteIndex);
            Tcl_AppendResult(interp, "error \"", errStr, "\" at position ", 
                             s, NULL);
            if (byteIndex != -1) {
                Tcl_AppendResult(interp, "\n\"", NULL);
                s[1] = '\0';
                for (i=-80; i < 80; i++) {
                    if ((byteIndex+i)>=0) {
                        if (xml_string[byteIndex+i]) {
                            s[0] = xml_string[byteIndex+i];
                            Tcl_AppendResult(interp, s, NULL);
                            if (i==0) {
                                Tcl_AppendResult(interp, " <--Error-- ", NULL);
                            }
                        } else {
                            break;
                        }
                    }
                }
                Tcl_AppendResult(interp, "\"",NULL);
            }
            if (takeHTMLParser) {
                FREE(errStr);
            }
            return TCL_ERROR;
        }
        return tcldom_returnDocumentObj (interp, doc, setVariable, newObjName,
                                         1, 0);
    }

#ifdef TDOM_NO_EXPAT
    Tcl_AppendResult(interp, "tDOM was compiled without Expat!", NULL);
    return TCL_ERROR;
#else
    parser = XML_ParserCreate_MM(NULL, MEM_SUITE, NULL);
    Tcl_ResetResult(interp);

    doc = domReadDocument(parser, xml_string,
                          xml_string_len,
                          ignoreWhiteSpaces,
                          TSD(Encoding_to_8bit),
                          TSD(storeLineColumn),
                          feedbackAfter,
                          chan,
                          baseURI,
                          extResolver,
                          useForeignDTD,
                          paramEntityParsing,
                          interp);
    if (doc == NULL) {
        char s[50];
        long byteIndex, i;

        interpResult = Tcl_GetStringResult(interp);
        if (interpResult[0] == '\0') {
            /* If the interp result isn't empty, then there was an error
               in an enternal entity and the interp result has already the
               error msg. If we don't got a document, but interp result is
               empty, the error occured in the main document and we
               build the error msg as follows. */
            sprintf(s, "%ld", XML_GetCurrentLineNumber(parser));
            Tcl_AppendResult(interp, "error \"", 
                             XML_ErrorString(XML_GetErrorCode(parser)),
                             "\" at line ", s, " character ", NULL);
            sprintf(s, "%ld", XML_GetCurrentColumnNumber(parser));
            Tcl_AppendResult(interp, s, NULL);
            byteIndex = XML_GetCurrentByteIndex(parser);
            if ((byteIndex != -1) && (chan == NULL)) {
                Tcl_AppendResult(interp, "\n\"", NULL);
                s[1] = '\0';
                for (i=-20; i < 40; i++) {
                    if ((byteIndex+i)>=0) {
                        if (xml_string[byteIndex+i]) {
                            s[0] = xml_string[byteIndex+i];
                            Tcl_AppendResult(interp, s, NULL);
                            if (i==0) {
                                Tcl_AppendResult(interp, " <--Error-- ", NULL);
                            }
                        } else {
                            break;
                        }
                    }
                }
                Tcl_AppendResult(interp, "\"",NULL);
            }
        }
        XML_ParserFree(parser);
        return TCL_ERROR;
    }
    XML_ParserFree(parser);

    return tcldom_returnDocumentObj (interp, doc, setVariable, newObjName, 1,
                                     0);
#endif

}


/*----------------------------------------------------------------------------
|   tcldom_DomObjCmd
|
\---------------------------------------------------------------------------*/
int tcldom_DomObjCmd (
    ClientData   clientData,
    Tcl_Interp * interp,
    int          objc,
    Tcl_Obj    * CONST objv[]
)
{
    GetTcldomTSD()

    char        * method, tmp[300];
    int           methodIndex, result, i, bool;
    Tcl_CmdInfo   cmdInfo;
    Tcl_Obj     * mobjv[MAX_REWRITE_ARGS];

    static CONST84 char *domMethods[] = {
        "createDocument",  "createDocumentNS",   "createNodeCmd",
        "parse",           "setResultEncoding",  "setStoreLineColumn",
        "isCharData",      "isName",             "isPIName",
        "isQName",         "isComment",          "isCDATA",
        "isPIValue",       "isNCName",           "createDocumentNode",
        "setNameCheck",    "setTextCheck",       "setObjectCommands",
#ifdef TCL_THREADS
        "attachDocument",  "detachDocument",
#endif
        NULL
    };
    enum domMethod {
        m_createDocument,    m_createDocumentNS,   m_createNodeCmd,
        m_parse,             m_setResultEncoding,  m_setStoreLineColumn,
        m_isCharData,        m_isName,             m_isPIName,
        m_isQName,           m_isComment,          m_isCDATA,
        m_isPIValue,         m_isNCName,           m_createDocumentNode,
        m_setNameCheck,      m_setTextCheck,       m_setObjectCommands
#ifdef TCL_THREADS
        ,m_attachDocument,   m_detachDocument
#endif
    };

    static CONST84 char *nodeModeValues[] = {
        "automatic", "command", "token", NULL
    };
    enum nodeModeValue {
        v_automatic, v_command, v_token
    };

    if (objc < 2) {
        SetResult(dom_usage);
        return TCL_ERROR;
    }
    if (TSD(domCreateCmdMode) == DOM_CREATECMDMODE_AUTO) {
        TSD(dontCreateObjCommands) = 0;
    }
    method = Tcl_GetString(objv[1]);
    if (Tcl_GetIndexFromObj(NULL, objv[1], domMethods, "method", 0,
                            &methodIndex) != TCL_OK) {
        /*--------------------------------------------------------
        |   try to find method implemented as normal Tcl proc
        \-------------------------------------------------------*/
        if ((strlen(method)-1) >= 270) {
            SetResult("method name to long!");
            return TCL_ERROR;
        }
        sprintf(tmp, "::dom::DOMImplementation::%s", method);
        DBG(fprintf(stderr, "testing %s\n", tmp));
        result = Tcl_GetCommandInfo(interp, tmp, &cmdInfo);
        if (!result) {
            SetResult(dom_usage);
            return TCL_ERROR;
        }
        if (!cmdInfo.isNativeObjectProc) {
            SetResult("can't access Tcl level method!");
            return TCL_ERROR;
        }
        if (objc >= MAX_REWRITE_ARGS) {
            SetResult("too many args to call Tcl level method!");
            return TCL_ERROR;
        }
        mobjv[0] = objv[1];
        mobjv[1] = objv[0];
        for (i=2; i<objc; i++) mobjv[i] = objv[i];
        return cmdInfo.objProc(cmdInfo.objClientData, interp, objc, mobjv);
    }
    CheckArgs(2,12,1,dom_usage);
    switch ((enum domMethod) methodIndex) {

        case m_createDocument:
            return tcldom_createDocument(clientData, interp, --objc, objv+1);

        case m_createDocumentNS:
            return tcldom_createDocumentNS(clientData, interp, --objc, objv+1);

        case m_createDocumentNode:
            return tcldom_createDocumentNode (clientData, interp, --objc,
                                              objv+1);
        case m_createNodeCmd:
            return nodecmd_createNodeCmd(interp, --objc, objv+1,
                                         !TSD(dontCheckName),
                                         !TSD(dontCheckCharData));
        case m_parse:
            return tcldom_parse(clientData, interp, --objc, objv+1);

#ifdef TCL_THREADS
        case m_attachDocument:
            {
                char *cmdName, *errMsg;
                domDocument *doc;
                if (objc < 3) {
                    SetResult(dom_usage);
                    return TCL_ERROR;
                }
                cmdName = Tcl_GetString(objv[2]);
                doc = tcldom_getDocumentFromName(interp, cmdName, &errMsg);
                if (doc == NULL) {
                    SetResult(errMsg);
                    return TCL_ERROR;
                }
                return tcldom_returnDocumentObj(interp, doc, (objc == 4),
                                                (objc==4) ? objv[3] : NULL,
                                                1, 0);
            }
            break;
        case m_detachDocument:
            {
                char objCmdName[80], *cmdName, *errMsg;
                Tcl_CmdInfo cmdInfo;
                domDocument *doc;
                if (objc < 3) {
                    SetResult(dom_usage);
                    return TCL_ERROR;
                }
                cmdName = Tcl_GetString(objv[2]);
                doc = tcldom_getDocumentFromName(interp, cmdName, &errMsg);
                if (doc == NULL) {
                    SetResult(errMsg);
                    return TCL_ERROR;
                }
                DOC_CMD(objCmdName, doc);
                if (Tcl_GetCommandInfo(interp, objCmdName, &cmdInfo)) {
                    Tcl_DeleteCommand(interp, objCmdName);
                } else {
                    tcldom_deleteDoc(interp, doc);
                }
                SetResult("");
                return TCL_OK;
            }
            break;
#endif

        case m_setResultEncoding:
            return tcldom_setResultEncoding(clientData, interp, --objc, objv+1);

        case m_setStoreLineColumn:
            if (objc == 3) {
                if (Tcl_GetBooleanFromObj(interp, objv[2], &bool) != TCL_OK) {
                    return TCL_ERROR;
                }
                TSD(storeLineColumn) = bool;
            }
            SetBooleanResult(TSD(storeLineColumn));
            return TCL_OK;

        case m_setNameCheck:
            if (objc == 3) {
                if (Tcl_GetBooleanFromObj(interp, objv[2], &bool) != TCL_OK) {
                    return TCL_ERROR;
                }
                TSD(dontCheckName) = !bool;
            }
            SetBooleanResult(!TSD(dontCheckName));
            return TCL_OK;
            
        case m_setTextCheck:
            if (objc == 3) {
                if (Tcl_GetBooleanFromObj(interp, objv[2], &bool) != TCL_OK) {
                    return TCL_ERROR;
                }
                TSD(dontCheckCharData) = !bool;
            }
            SetBooleanResult(!TSD(dontCheckCharData));
            return TCL_OK;
            
        case m_setObjectCommands:
            if (objc == 3) {
                if (Tcl_GetIndexFromObj (interp, objv[2], nodeModeValues,
                                         "mode value", 0, &i) != TCL_OK) {
                    return TCL_ERROR;
                }
                switch ((enum nodeModeValue) i) {
                case v_automatic:
                    TSD(domCreateCmdMode) = DOM_CREATECMDMODE_AUTO;
                    TSD(dontCreateObjCommands) = 0;
                    break;
                case v_command:
                    TSD(domCreateCmdMode) = DOM_CREATECMDMODE_CMDS;
                    TSD(dontCreateObjCommands) = 0;
                    break;
                case v_token:
                    TSD(domCreateCmdMode) = DOM_CREATECMDMODE_TOKENS;
                    TSD(dontCreateObjCommands) = 1;
                    break;
                }
            }
            switch (TSD(domCreateCmdMode)) {
            case DOM_CREATECMDMODE_AUTO:
                SetResult("automatic");
                break;
            case DOM_CREATECMDMODE_CMDS:
                SetResult("command");
                break;
            case DOM_CREATECMDMODE_TOKENS:
                SetResult("token");
                break;
            default:
                domPanic("Impossible node creation mode.");
            }
            return TCL_OK;

        case m_isCharData:
            CheckArgs(3,3,2,"string");
            SetBooleanResult(domIsChar(Tcl_GetString(objv[2])));
            return TCL_OK;
            
        case m_isName:
            CheckArgs(3,3,2,"string");
            SetBooleanResult(domIsNAME(Tcl_GetString(objv[2])));
            return TCL_OK;
            
        case m_isPIName:
            CheckArgs(3,3,2,"string");
            SetBooleanResult(domIsPINAME(Tcl_GetString(objv[2])));
            return TCL_OK;
            
        case m_isQName:
            CheckArgs(3,3,2,"string");
            SetBooleanResult(domIsQNAME(Tcl_GetString(objv[2])));
            return TCL_OK;
            
        case m_isComment:
            CheckArgs(3,3,2,"string");
            SetBooleanResult(domIsComment(Tcl_GetString(objv[2])));
            return TCL_OK;
            
        case m_isCDATA:
            CheckArgs(3,3,2,"string");
            SetBooleanResult(domIsCDATA(Tcl_GetString(objv[2])));
            return TCL_OK;

        case m_isPIValue:
            CheckArgs(3,3,2,"string");
            SetBooleanResult(domIsPIValue(Tcl_GetString(objv[2])));
            return TCL_OK;
            
        case m_isNCName:
            CheckArgs(3,3,2,"string");
            SetBooleanResult(domIsNCNAME(Tcl_GetString(objv[2])));
            return TCL_OK;

    }
    SetResult( dom_usage);
    return TCL_ERROR;
}

#ifdef TCL_THREADS

/*----------------------------------------------------------------------------
|   tcldom_EvalLocked
|
\---------------------------------------------------------------------------*/

static
int tcldom_EvalLocked (
    Tcl_Interp  * interp, 
    Tcl_Obj    ** objv, 
    domDocument * doc,
    int          flag
)
{
    int ret = TCL_OK;
    domlock *dl = doc->lock;

    domLocksLock(dl, flag);

    Tcl_AllowExceptions(interp);
    ret = Tcl_EvalObj(interp, objv[2]);
    if (ret == TCL_ERROR) {
        char msg[64 + TCL_INTEGER_SPACE];
        sprintf(msg, "\n    (\"%s %s\" body line %d)", Tcl_GetString(objv[0]),
                Tcl_GetString(objv[1]), interp->errorLine);
        Tcl_AddErrorInfo(interp, msg);
    }

    domLocksUnlock(dl);

    return (ret == TCL_BREAK) ? TCL_OK : ret;
}

/*----------------------------------------------------------------------------
|   tcldom_RegisterDocShared
|
\---------------------------------------------------------------------------*/

static
int tcldom_RegisterDocShared (
    domDocument * doc
)
{
    Tcl_HashEntry *entryPtr;
    int refCount, newEntry;

    Tcl_MutexLock(&tableMutex);
    refCount = ++doc->refCount;
    entryPtr = Tcl_CreateHashEntry(&sharedDocs, (char*)doc, &newEntry);
    if (newEntry) {
        Tcl_SetHashValue(entryPtr, (ClientData)doc);
    }
    Tcl_MutexUnlock(&tableMutex);

    DBG(fprintf(stderr, "--> tcldom_RegisterDocShared: doc %p %s "
                "shared table now with refcount of %d\n", doc,
                newEntry ? "entered into" : "already in", refCount));
    return 0;
}

/*----------------------------------------------------------------------------
|   tcldom_UnregisterDocShared
|
\---------------------------------------------------------------------------*/

static
int tcldom_UnregisterDocShared (
    Tcl_Interp  * interp,
    domDocument * doc
)
{
    int deleted;

    Tcl_MutexLock(&tableMutex);
    if (doc->refCount > 1) {
        tcldom_deleteNode(doc->rootNode, interp);
        domFreeNode(doc->rootNode, tcldom_deleteNode, interp, 1);
        doc->refCount--;
        deleted = 0;
    } else {
        Tcl_HashEntry *entryPtr = Tcl_FindHashEntry(&sharedDocs, (char*)doc);
        if (entryPtr) {
            Tcl_DeleteHashEntry(entryPtr);
            deleted = 1;
        } else {
            deleted = 0;
        }
    }
    Tcl_MutexUnlock(&tableMutex);

    DBG(fprintf(stderr, "--> tcldom_UnregisterDocShared: doc %p %s "
                "shared table\n", doc, deleted ? "deleted from" : "left in"));

    return deleted;
}

/*----------------------------------------------------------------------------
|   tcldom_CheckDocShared
|
\---------------------------------------------------------------------------*/

static
int tcldom_CheckDocShared (
    domDocument * doc
)
{
    Tcl_HashEntry *entryPtr;
    domDocument *tabDoc = NULL;
    int found = 0;

    Tcl_MutexLock(&tableMutex);
    entryPtr = Tcl_FindHashEntry(&sharedDocs, (char*)doc);
    if (entryPtr == NULL) {
        found = 0;
    } else {
        tabDoc = (domDocument*)Tcl_GetHashValue(entryPtr);
        found  = tabDoc ? 1 : 0;
    }
    Tcl_MutexUnlock(&tableMutex);

    if (found && doc != tabDoc) {
        Tcl_Panic("document mismatch; doc=%p, in table=%p\n", doc, tabDoc);
    }

    return found;
}

#endif /* TCL_THREADS */

#ifndef TDOM_NO_UNKNOWN_CMD

/*----------------------------------------------------------------------------
|   tcldom_unknownCmd
|
\---------------------------------------------------------------------------*/
int tcldom_unknownCmd (
    ClientData   clientData,
    Tcl_Interp * interp,
    int          objc,
    Tcl_Obj    * CONST objv[]
)
{
    int          len, i, rc, openedParen, count, args;
    char        *cmd, *dot, *paren, *arg[MAX_REWRITE_ARGS], *object, *method;
    Tcl_DString  callString;
    Tcl_CmdInfo  cmdInfo;
    Tcl_Obj     *vector[2+MAX_REWRITE_ARGS];
    Tcl_Obj     **objvCall;


    cmd = Tcl_GetStringFromObj(objv[1], &len);

    DBG(fprintf(stderr, "tcldom_unknownCmd: cmd=-%s- \n", cmd));

    dot = strchr(cmd,'.');
    if ((dot != NULL) && (dot != cmd)) {

        object = cmd;
        cmd    = dot+1;
        *dot   = '\0';
        dot    = strchr(cmd,'.');

        while (dot != NULL) {

            method = cmd;
            paren = strchr(cmd,'(');
            args = 0;
            if (paren && (paren < dot)) {
                *paren = '\0';
                paren++;
                arg[args] = paren;
                openedParen = 1;
                while (*paren) {
                    if (*paren == '\\') {
                        (void) Tcl_Backslash(paren, &count);
                        paren += count;
                    } else if (*paren == ')') {
                        openedParen--;
                        if (openedParen==0) {
                            *paren = '\0';
                            args++;
                            break;
                        }
                    } else if (*paren == '(') {
                        openedParen++;
                        paren++;
                    } else if (*paren == ',') {
                        *paren = '\0';
                        arg[++args] = paren+1;
                        if (args >= MAX_REWRITE_ARGS) {
                            SetResult( "too many args");
                            return TCL_ERROR;
                        }
                        paren++;
                    } else {
                        paren++;
                    }
                }
                if (openedParen!=0) {
                    SetResult( "mismatched (");
                    return TCL_ERROR;
                }
            }
            cmd    = dot+1;
            *dot   = '\0';

            DBG(fprintf(stderr, "method=-%s- \n", method);
                fprintf(stderr, "rest=-%s- \n", cmd);
                for(i=0; i<args; i++) {
                    fprintf(stderr, "args %d =-%s- \n", i, arg[i]);
                }
            )

            /*---------------------------------------------------------
            |   intermediate call
            \--------------------------------------------------------*/
            rc = Tcl_GetCommandInfo(interp, object, &cmdInfo);
            if (rc && cmdInfo.isNativeObjectProc) {
                vector[0] = Tcl_NewStringObj(object, -1);
                vector[1] = Tcl_NewStringObj(method, -1);
                for(i=0; i<args; i++) {
                    vector[2+i] = Tcl_NewStringObj(arg[i], -1);
                }
                rc = cmdInfo.objProc(cmdInfo.objClientData, interp, 2+args,
                                     vector);
                if (rc != TCL_OK) {
                   return rc;
                }
                for(i=args+1; i >= 0; i--) {
                    Tcl_DecrRefCount(vector[i]);
                }
            } else {
                Tcl_DStringInit(&callString);
                Tcl_DStringAppendElement(&callString, object);
                Tcl_DStringAppendElement(&callString, method);
                for(i=0; i<args; i++) {
                    Tcl_DStringAppendElement(&callString, arg[i] );
                }
                rc = Tcl_Eval(interp, Tcl_DStringValue(&callString));
                Tcl_DStringFree(&callString);
                if (rc != TCL_OK) {
                   return rc;
                }
            }
            /* get the new object returned from above call */
            object = Tcl_GetStringResult(interp);
            dot = strchr(cmd,'.');
        }

        method = cmd;
            paren = strchr(cmd,'(');
            args = 0;
            if (paren) {
                *paren = '\0';
                paren++;
                arg[args] = paren;
                openedParen = 1;
                while (*paren) {
                    if (*paren == '\\') {
                        (void) Tcl_Backslash(paren, &count);
                        paren += count;
                    } else if (*paren == ')') {
                        openedParen--;
                        if (openedParen==0) {
                            *paren = '\0';
                            args++;
                            break;
                        }
                    } else if (*paren == '(') {
                        openedParen++;
                        paren++;
                    } else if (*paren == ',') {
                        *paren = '\0';
                        arg[++args] = paren+1;
                        if (args >= MAX_REWRITE_ARGS) {
                            SetResult( "too many args");
                            return TCL_ERROR;
                        }
                        paren++;
                    } else {
                        paren++;
                    }
                }
                if (openedParen!=0) {
                    SetResult( "mismatched (");
                    return TCL_ERROR;
                }
            }
            DBG(fprintf(stderr, "method=-%s- \n", method);
                fprintf(stderr, "rest=-%s- \n", cmd);
                for(i=0; i<args; i++) {
                    fprintf(stderr, "args %d =-%s- \n", i, arg[i]);
                }
            )

        /*----------------------------------------------------------------
        |   final call
        \---------------------------------------------------------------*/
        rc = Tcl_GetCommandInfo(interp, object, &cmdInfo);
        if (rc && cmdInfo.isNativeObjectProc) {

            objvCall = (Tcl_Obj**)MALLOC(sizeof(Tcl_Obj*) * (objc+args));

            objvCall[0] = Tcl_NewStringObj(object, -1);
            objvCall[1] = Tcl_NewStringObj(method, -1);
            for(i=0; i<args; i++) {
                objvCall[2+i] = Tcl_NewStringObj(arg[i], -1);
            }
            for (i=2; i<objc; i++) {
                objvCall[i+args] = objv[i];
            }
            rc = cmdInfo.objProc(cmdInfo.objClientData, interp, objc + args,
                                 objvCall);
            for(i=objc+args-1; i >= 0; i--) {
                Tcl_DecrRefCount(objvCall[i]);
            }
            FREE((void*)objvCall);

        } else {
            Tcl_DStringInit(&callString);
            Tcl_DStringAppendElement(&callString, object);
            Tcl_DStringAppendElement(&callString, method);
            for(i=2; i<objc; i++) {
                Tcl_DStringAppendElement(&callString, Tcl_GetString(objv[i]));
            }
            rc = Tcl_Eval(interp, Tcl_DStringValue(&callString));
            Tcl_DStringFree(&callString);
        }
        return rc;

    } else {

        /*----------------------------------------------------------------
        |   call the original unknown function
        |
        \---------------------------------------------------------------*/
        Tcl_DStringInit(&callString);
        Tcl_DStringAppendElement(&callString, "unknown_tdom");
        for(i=1; i<objc; i++) {
            Tcl_DStringAppendElement(&callString, Tcl_GetString(objv[i]));
        }
        rc = Tcl_Eval(interp, Tcl_DStringValue(&callString));
        Tcl_DStringFree(&callString);
        return rc;
    }
}

#endif
