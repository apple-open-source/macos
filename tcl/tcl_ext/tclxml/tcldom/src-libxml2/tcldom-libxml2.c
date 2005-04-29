/* tcldom-libxml.c --
 *
 *	A Tcl wrapper for libxml's node tree API,
 *	conformant to the TclDOM API.
 *
 * Copyright (c) 2001-2003 Zveno Pty Ltd
 * http://www.zveno.com/
 *
 * Zveno Pty Ltd makes this software and associated documentation
 * available free of charge for any purpose.  You may make copies
 * of the software but you must include all of this notice on any copy.
 *
 * Zveno Pty Ltd does not warrant that this software is error free
 * or fit for any purpose.  Zveno Pty Ltd disclaims any liability for
 * all claims, expenses, losses, damages and costs any user may incur
 * as a result of using, copying or modifying the software.
 *
 * $Id: tcldom-libxml2.c,v 1.57 2003/03/09 11:12:49 balls Exp $
 */

#include "tcldom-libxml2.h"
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlIO.h>
#include <libxml/HTMLtree.h>
#include <libxml/globals.h>

#define TCL_DOES_STUBS \
    (TCL_MAJOR_VERSION > 8 || TCL_MAJOR_VERSION == 8 && (TCL_MINOR_VERSION > 1 || \
    (TCL_MINOR_VERSION == 1 && TCL_RELEASE_LEVEL == TCL_FINAL_RELEASE)))

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT

#define NUM_EVENT_TYPES 17

/*
 * Data structures for internal functions.
 */

typedef struct ParserClientData {
  Tcl_Interp *interp;
  Tcl_Obj *externalentityloader;	/* Callback for loading an external entity */
} ParserClientData;

typedef struct TclDOMDocument {
  char *token;

  xmlDocPtr docPtr;

  Tcl_HashTable *nodes;
  int nodeCntr;
  Tcl_HashTable *events;
  int eventCntr;
  int listening[NUM_EVENT_TYPES];

} TclDOMDocument;

/*
 * Data structures to support Events
 */

typedef struct TclDOMEvent {
  Tcl_Interp *interp;
  Tcl_Obj *objPtr;	/* Tcl object that wraps this structure */
  Tcl_Command cmd;	/* Tcl command that accesses this structure */
  char *cmdname;	/* Name of that command */

  TclDOMDocument *ownerDocument;	/* Toplevel Document for this event */

  int stopPropagation;
  int preventDefault;
  int dispatched;

  Tcl_Obj *altKey;
  Tcl_Obj *attrName;
  Tcl_Obj *attrChange;
  Tcl_Obj *bubbles;
  Tcl_Obj *button;
  Tcl_Obj *cancelable;
  Tcl_Obj *clientX;
  Tcl_Obj *clientY;
  Tcl_Obj *ctrlKey;
  Tcl_Obj *currentNode;
  Tcl_Obj *detail;
  Tcl_Obj *eventPhase;
  Tcl_Obj *metaKey;
  Tcl_Obj *newValue;
  Tcl_Obj *prevValue;
  Tcl_Obj *relatedNode;
  Tcl_Obj *screenX;
  Tcl_Obj *screenY;
  Tcl_Obj *shiftKey;
  Tcl_Obj *target;
  Tcl_Obj *timeStamp;
  Tcl_Obj *type;
  Tcl_Obj *view;
} TclDOMEvent;

/*
 * Prototypes for procedures defined later in this file:
 */

Tcl_FreeInternalRepProc	TclDOM_DocFree;
Tcl_DupInternalRepProc	TclDOM_DocDup;
Tcl_UpdateStringProc	TclDOM_DocUpdate;
Tcl_SetFromAnyProc	TclDOM_DocSetFromAny;

Tcl_FreeInternalRepProc	TclDOM_NodeFree;
Tcl_DupInternalRepProc	TclDOM_NodeDup;
Tcl_UpdateStringProc	TclDOM_NodeUpdate;
Tcl_SetFromAnyProc	TclDOM_NodeSetFromAny;

Tcl_FreeInternalRepProc	TclDOM_EventFree;
Tcl_DupInternalRepProc	TclDOM_EventDup;
Tcl_UpdateStringProc	TclDOM_EventUpdate;
Tcl_SetFromAnyProc	TclDOM_EventSetFromAny;

/*
 * Forward declarations for private functions.
 */

static void TclDOMGenericError _ANSI_ARGS_((void *ctx, const char *msg, ...));

static int TclDOM_GetDoc2FromObj _ANSI_ARGS_((Tcl_Interp *interp,
                                              Tcl_Obj *objPtr,
                                              TclDOMDocument **doc));

Tcl_Obj * TclDOM_CreateObjFromNode _ANSI_ARGS_((xmlNodePtr nodePtr));
Tcl_Obj * TclDOM_CreateObjFromDoc  _ANSI_ARGS_((xmlDocPtr  docPtr));

static int TclDOMDestroyDocument _ANSI_ARGS_((TclDOMDocument *docPtr));
static int TclDOMDestroyNode _ANSI_ARGS_((xmlNodePtr nodePtr, Tcl_Obj *objPtr));
/* see below for destroy event */

static char * TclDOMLiveNodeListNode _ANSI_ARGS_((ClientData clientData,
						  Tcl_Interp *interp,
						  char *name1,
						  char *name2,
						  int flags));
static char * TclDOMLiveNodeListDoc _ANSI_ARGS_((ClientData clientData,
						  Tcl_Interp *interp,
						  char *name1,
						  char *name2,
						  int flags));
static char * TclDOMLiveNamedNodeMap _ANSI_ARGS_((ClientData clientData,
						  Tcl_Interp *interp,
						  char *name1,
						  char *name2,
						  int flags));
static int TclDOMSetLiveNodeListNode _ANSI_ARGS_((Tcl_Interp *interp,
						  char *varname,
						  xmlNodePtr nodePtr));
static int TclDOMSetLiveNodeListDoc _ANSI_ARGS_((Tcl_Interp *interp,
						  char *varname,
						  xmlDocPtr docPtr));
static int TclDOMSetLiveNamedNodeMap _ANSI_ARGS_((Tcl_Interp *interp,
						  char *varname,
						  xmlNodePtr nodePtr));

/*
 * Forward declarations of commands
 */

static int TclDOMDOMImplementationCommand _ANSI_ARGS_((ClientData dummy,
						       Tcl_Interp *interp,
						       int objc,
						       Tcl_Obj *CONST objv[]));
static int TclDOMDocumentCommand _ANSI_ARGS_((ClientData dummy,
					      Tcl_Interp *interp,
					      int objc,
					      Tcl_Obj *CONST objv[]));
static int TclDOMNodeCommand _ANSI_ARGS_((ClientData dummy,
					  Tcl_Interp *interp,
					  int objc,
					  Tcl_Obj *CONST objv[]));
static int TclDOMElementCommand _ANSI_ARGS_((ClientData dummy,
					     Tcl_Interp *interp,
					     int objc,
					     Tcl_Obj *CONST objv[]));
static int TclDOMEventCommand _ANSI_ARGS_((ClientData dummy,
					   Tcl_Interp *interp,
					   int objc,
					   Tcl_Obj *CONST objv[]));

/*
 * Functions that implement the TclDOM_Implementation interface
 */

static int TclDOM_HasFeatureCommand _ANSI_ARGS_((ClientData dummy,
					    Tcl_Interp *interp,
					    int objc,
					    Tcl_Obj *CONST objv[]));
static int TclDOMCreateCommand _ANSI_ARGS_((ClientData dummy,
					    Tcl_Interp *interp,
					    int objc,
					    Tcl_Obj *CONST objv[]));
static int TclDOMDestroyCommand _ANSI_ARGS_((ClientData dummy,
					    Tcl_Interp *interp,
					    int objc,
					    Tcl_Obj *CONST objv[]));
static int TclDOMParseCommand _ANSI_ARGS_((ClientData dummy,
					    Tcl_Interp *interp,
					    int objc,
					    Tcl_Obj *CONST objv[]));
static int TclDOMSerializeCommand _ANSI_ARGS_((ClientData dummy,
					    Tcl_Interp *interp,
					    int objc,
					    Tcl_Obj *CONST objv[]));
static int TclDOMSelectNodeCommand _ANSI_ARGS_((ClientData dummy,
					    Tcl_Interp *interp,
					    int objc,
					    Tcl_Obj *CONST objv[]));

/*
 * Additional features
 */

static int TclDOMValidateCommand _ANSI_ARGS_((ClientData dummy,
					    Tcl_Interp *interp,
					    int objc,
					    Tcl_Obj *CONST objv[]));

static int TclDOMXIncludeCommand _ANSI_ARGS_((ClientData dummy,
					      Tcl_Interp *interp,
					      int objc,
					      Tcl_Obj *CONST objv[]));

static int TclDOMPrefix2NSCommand _ANSI_ARGS_((ClientData dummy,
					       Tcl_Interp *interp,
					       int objc,
					       Tcl_Obj *CONST objv[]));

static void TclDOMDeleteEvent _ANSI_ARGS_((ClientData clientData));

/*
 * Functions for event support
 */

static Tcl_Obj * TclDOMNewEvent _ANSI_ARGS_((Tcl_Interp *interp,
					     xmlDocPtr doc,
                                             Tcl_Obj *type));
/*
 * Other utilities
 */

static Tcl_Obj * TclDOMGetPath _ANSI_ARGS_((Tcl_Interp *interp,
					    xmlNodePtr nodePtr));

/*
 * Externally visible functions
 */

EXTERN int Tcldomxml_Init _ANSI_ARGS_((Tcl_Interp *interp));

/*
 * libxml2 callbacks
 */

static xmlParserInputPtr TclDOMExternalEntityLoader _ANSI_ARGS_((const char *URL,
								 const char *ID,
								 xmlParserCtxtPtr ctxt));

/*
 * Private libxml2 functions
 */

EXTERN xmlParserCtxtPtr xmlCreateMemoryParserCtxt _ANSI_ARGS_((const char *buffer,
							      int size));

Tcl_ObjType TclDOM_DocObjType = {
  "libxml2-doc",
  TclDOM_DocFree,
  TclDOM_DocDup,
  TclDOM_DocUpdate,
  TclDOM_DocSetFromAny
};

Tcl_ObjType TclDOM_NodeObjType = {
  "libxml2-node",
  TclDOM_NodeFree,
  TclDOM_NodeDup,
  TclDOM_NodeUpdate,
  TclDOM_NodeSetFromAny
};

Tcl_ObjType TclDOM_EventObjType = {
  "tcldom-event",
  TclDOM_EventFree,
  TclDOM_EventDup,
  TclDOM_EventUpdate,
  TclDOM_EventSetFromAny
};

/*
 * Hash tables for mapping string reps to doc structure:
 * one by string rep, the other by xmlDocPtr.
 * NB. nodes and events are now stored in a per-document hash table.
 */

static Tcl_HashTable documents;
static Tcl_HashTable docByPtr;
static int docCntr = 0;

/*
 * Event support.
 *
 * Each table is indexed by xmlNodePtr.
 * The value of an entry is a pointer to a hash table
 * for that node containing listeners, indexed by
 * event type.
 */

static Tcl_HashTable captureListeners;
static Tcl_HashTable bubbleListeners;

/*
 * default loader is overridden while parsing
 */

static xmlExternalEntityLoader defaultLoader;

/*
 * Accessor function for event objects
 */

static int TclDOM_GetEventFromObj _ANSI_ARGS_((Tcl_Interp *interp,
					       Tcl_Obj *objPtr,
					       TclDOMEvent **eventPtr));
static int TclDOM_GetDoc2FromObj _ANSI_ARGS_((Tcl_Interp *interp,
					       Tcl_Obj *objPtr,
					       TclDOMDocument **docPtr));

/*
 * Destructor function for event objects
 */

static int TclDOMDestroyEvent _ANSI_ARGS_((TclDOMEvent *eventPtr, Tcl_Obj *objPtr));

/*
 * Event management
 */

static void TclDOMInitEvent _ANSI_ARGS_((TclDOMEvent *eventPtr,
                                         Tcl_Obj *typePtr,
                                         Tcl_Obj *bubblesPtr,
                                         Tcl_Obj *cancelablePtr));
static void TclDOMInitUIEvent _ANSI_ARGS_((TclDOMEvent *eventPtr,
                                           Tcl_Obj *typePtr,
                                           Tcl_Obj *bubblesPtr,
                                           Tcl_Obj *cancelablePtr,
                                           Tcl_Obj *viewPtr,
                                           Tcl_Obj *detailPtr));
static void TclDOMInitMouseEvent _ANSI_ARGS_((TclDOMEvent *eventPtr,
                                              Tcl_Obj *typePtr,
                                              Tcl_Obj *bubblesPtr,
                                              Tcl_Obj *cancelablePtr,
                                              Tcl_Obj *viewPtr,
                                              Tcl_Obj *detailPtr,
                                              Tcl_Obj *screenXPtr,
                                              Tcl_Obj *screenYPtr,
                                              Tcl_Obj *clientXPtr,
                                              Tcl_Obj *clientYPtr,
                                              Tcl_Obj *ctrlKeyPtr,
                                              Tcl_Obj *altKeyPtr,
                                              Tcl_Obj *shiftKeyPtr,
                                              Tcl_Obj *metaKeyPtr,
                                              Tcl_Obj *relatedNodePtr));
static void TclDOMInitMutationEvent _ANSI_ARGS_((TclDOMEvent *eventPtr,
                                              Tcl_Obj *typePtr,
                                              Tcl_Obj *bubblesPtr,
                                              Tcl_Obj *cancelablePtr,
                                              Tcl_Obj *relatedNodePtr,
                                              Tcl_Obj *prevValuePtr,
                                              Tcl_Obj *newValuePtr,
                                              Tcl_Obj *attrNamePtr,
                                              Tcl_Obj *attrChangePtr));
static int TclDOM_PostUIEvent _ANSI_ARGS_((Tcl_Interp *interp,
                                           xmlDocPtr docPtr,
                                           Tcl_Obj *nodeObjPtr,
                                           Tcl_Obj *typePtr,
                                           Tcl_Obj *bubblesPtr,
                                           Tcl_Obj *cancelablePtr,
                                           Tcl_Obj *viewPtr,
                                           Tcl_Obj *detailPtr));
static int TclDOM_PostMouseEvent _ANSI_ARGS_((Tcl_Interp *interp,
                                              xmlDocPtr docPtr,
                                              Tcl_Obj *nodeObjPtr,
                                              Tcl_Obj *typePtr,
                                              Tcl_Obj *bubblesPtr,
                                              Tcl_Obj *cancelablePtr,
                                              Tcl_Obj *viewPtr,
                                              Tcl_Obj *detailPtr,
                                              Tcl_Obj *screenXPtr,
                                              Tcl_Obj *screenYPtr,
                                              Tcl_Obj *clientXPtr,
                                              Tcl_Obj *clientYPtr,
                                              Tcl_Obj *ctrlKeyPtr,
                                              Tcl_Obj *altKeyPtr,
                                              Tcl_Obj *shiftKeyPtr,
                                              Tcl_Obj *metaKeyPtr,
                                              Tcl_Obj *relatedNodePtr));
static int TclDOM_PostMutationEvent _ANSI_ARGS_((Tcl_Interp *interp,
                                              xmlDocPtr docPtr,
                                              Tcl_Obj *nodeObjPtr,
                                              Tcl_Obj *typePtr,
                                              Tcl_Obj *bubblesPtr,
                                              Tcl_Obj *cancelablePtr,
                                              Tcl_Obj *relatedNodePtr,
                                              Tcl_Obj *prevValuePtr,
                                              Tcl_Obj *newValuePtr,
                                              Tcl_Obj *attrNamePtr,
                                              Tcl_Obj *attrChangePtr));
static int TclDOM_AddEventListener _ANSI_ARGS_((Tcl_Interp *interp,
                                                TclDOMDocument *tcldomdocPtr,
                                                void *tokenPtr,
                                                Tcl_Obj *typePtr,
                                                Tcl_Obj *listenerPtr,
                                                int capturing));
static int TclDOM_RemoveEventListener _ANSI_ARGS_((Tcl_Interp *interp,
                                                TclDOMDocument *tcldomdocPtr,
                                                void *tokenPtr,
                                                Tcl_Obj *typePtr,
                                                Tcl_Obj *listenerPtr,
                                                int capturing));
static int TclDOM_DispatchEvent _ANSI_ARGS_((Tcl_Interp *interp,
                                             Tcl_Obj *nodePtr,
                                             Tcl_Obj *eventObjPtr,
                                             TclDOMEvent *eventPtr));

/*
 * For additional checks when creating nodes
 */

static Tcl_Obj *checkName;
static Tcl_Obj *checkQName;

/*
 * For debugging
 */

static Tcl_Channel stderrChan;
static char dbgbuf[200];

/*
 * Switch tables
 */

#ifndef CONST84
#define CONST84 /* Before 8.4 no 'const' required */
#endif

static CONST84 char *DOMImplementationCommandMethods[] = {
  "hasFeature",
  "createDocument",
  "create",
  "createDocumentType",
  "createNode",
  "destroy",
  "isNode",
  "parse",
  "selectNode",
  "serialize",
  "trim",
  (char *) NULL
};
enum DOMImplementationCommandMethods {
  TCLDOM_IMPL_HASFEATURE,
  TCLDOM_IMPL_CREATEDOCUMENT,
  TCLDOM_IMPL_CREATE,
  TCLDOM_IMPL_CREATEDOCUMENTYPE,
  TCLDOM_IMPL_CREATENODE,
  TCLDOM_IMPL_DESTROY,
  TCLDOM_IMPL_ISNODE,
  TCLDOM_IMPL_PARSE,
  TCLDOM_IMPL_SELECTNODE,
  TCLDOM_IMPL_SERIALIZE,
  TCLDOM_IMPL_TRIM
};
static CONST84 char *DocumentCommandMethods[] = {
  "cget",
  "configure",
  "createElement",
  "createDocumentFragment",
  "createTextNode",
  "createComment",
  "createCDATASection",
  "createProcessingInstruction",
  "createAttribute",
  "createEntity",
  "createEntityReference",
  "createDocTypeDecl",
  "importNode",
  "createElementNS",
  "createAttributeNS",
  "getElementsByTagNameNS",
  "getElementsById",
  "createEvent",
  "getElementsByTagName",
  (char *) NULL
};
enum DocumentCommandMethods {
  TCLDOM_DOCUMENT_CGET,
  TCLDOM_DOCUMENT_CONFIGURE,
  TCLDOM_DOCUMENT_CREATEELEMENT,
  TCLDOM_DOCUMENT_CREATEDOCUMENTFRAGMENT,
  TCLDOM_DOCUMENT_CREATETEXTNODE,
  TCLDOM_DOCUMENT_CREATECOMMENT,
  TCLDOM_DOCUMENT_CREATECDATASECTION,
  TCLDOM_DOCUMENT_CREATEPI,
  TCLDOM_DOCUMENT_CREATEATTRIBUTE,
  TCLDOM_DOCUMENT_CREATEENTITY,
  TCLDOM_DOCUMENT_CREATEENTITYREFERENCE,
  TCLDOM_DOCUMENT_CREATEDOCTYPEDECL,
  TCLDOM_DOCUMENT_IMPORTNODE,
  TCLDOM_DOCUMENT_CREATEELEMENTNS,
  TCLDOM_DOCUMENT_CREATEATTRIBUTENS,
  TCLDOM_DOCUMENT_GETELEMENTSBYTAGNAMENS,
  TCLDOM_DOCUMENT_GETELEMENTSBYID,
  TCLDOM_DOCUMENT_CREATEEVENT,
  TCLDOM_DOCUMENT_GETELEMENTSBYTAGNAME
};
static CONST84 char *DocumentCommandOptions[] = {
  "-doctype",
  "-implementation",
  "-documentElement",
  (char *) NULL
};
enum DocumentCommandOptions {
  TCLDOM_DOCUMENT_DOCTYPE,
  TCLDOM_DOCUMENT_IMPLEMENTATION,
  TCLDOM_DOCUMENT_DOCELEMENT
};
static CONST84 char *NodeCommandMethods[] = {
  "cget",
  "configure",
  "insertBefore",
  "replaceChild",
  "removeChild",
  "appendChild",
  "hasChildNodes",
  "cloneNode",
  "children",
  "parent",
  "path",
  "createNode",
  "selectNode",
  "stringValue",
  "addEventListener",
  "removeEventListener",
  "dispatchEvent",
  "isSameNode",
  (char *) NULL
};
enum NodeCommandMethods {
  TCLDOM_NODE_CGET,
  TCLDOM_NODE_CONFIGURE,
  TCLDOM_NODE_INSERTBEFORE,
  TCLDOM_NODE_REPLACECHILD,
  TCLDOM_NODE_REMOVECHILD,
  TCLDOM_NODE_APPENDCHILD,
  TCLDOM_NODE_HASCHILDNODES,
  TCLDOM_NODE_CLONENODE,
  TCLDOM_NODE_CHILDREN,
  TCLDOM_NODE_PARENT,
  TCLDOM_NODE_PATH,
  TCLDOM_NODE_CREATENODE,
  TCLDOM_NODE_SELECTNODE,
  TCLDOM_NODE_STRINGVALUE,
  TCLDOM_NODE_ADDEVENTLISTENER,
  TCLDOM_NODE_REMOVEEVENTLISTENER,
  TCLDOM_NODE_DISPATCHEVENT,
  TCLDOM_NODE_ISSAMENODE
};
static CONST84 char *NodeCommandOptions[] = {
  "-nodeType",
  "-parentNode",
  "-childNodes",
  "-firstChild",
  "-lastChild",
  "-previousSibling",
  "-nextSibling",
  "-attributes",
  "-namespaceURI",
  "-prefix",
  "-localName",
  "-nodeValue",
  "-cdatasection",
  "-nodeName",
  "-ownerDocument",
  (char *) NULL
};
enum NodeCommandOptions {
  TCLDOM_NODE_NODETYPE,
  TCLDOM_NODE_PARENTNODE,
  TCLDOM_NODE_CHILDNODES,
  TCLDOM_NODE_FIRSTCHILD,
  TCLDOM_NODE_LASTCHILD,
  TCLDOM_NODE_PREVIOUSSIBLING,
  TCLDOM_NODE_NEXTSIBLING,
  TCLDOM_NODE_ATTRIBUTES,
  TCLDOM_NODE_NAMESPACEURI,
  TCLDOM_NODE_PREFIX,
  TCLDOM_NODE_LOCALNAME,
  TCLDOM_NODE_NODEVALUE,
  TCLDOM_NODE_CDATASECTION,
  TCLDOM_NODE_NODENAME,
  TCLDOM_NODE_OWNERDOCUMENT
};
static CONST84 char *NodeCommandAddEventListenerOptions[] = {
  "-usecapture",
  (char *) NULL
};
enum NodeCommandAddEventListenerOptions {
  TCLDOM_NODE_ADDEVENTLISTENER_USECAPTURE
};
static CONST84 char *ElementCommandMethods[] = {
  "cget",
  "configure",
  "getAttribute",
  "setAttribute",
  "removeAttribute",
  "getAttributeNS",
  "setAttributeNS",
  "removeAttributeNS",
  "getAttributeNode",
  "setAttributeNode",
  "removeAttributeNode",
  "getAttributeNodeNS",
  "setAttributeNodeNS",
  "removeAttributeNodeNS",
  "getElementsByTagName",
  "normalize",
  (char *) NULL
};
enum ElementCommandMethods {
  TCLDOM_ELEMENT_CGET,
  TCLDOM_ELEMENT_CONFIGURE,
  TCLDOM_ELEMENT_GETATTRIBUTE,
  TCLDOM_ELEMENT_SETATTRIBUTE,
  TCLDOM_ELEMENT_REMOVEATTRIBUTE,
  TCLDOM_ELEMENT_GETATTRIBUTENS,
  TCLDOM_ELEMENT_SETATTRIBUTENS,
  TCLDOM_ELEMENT_REMOVEATTRIBUTENS,
  TCLDOM_ELEMENT_GETATTRIBUTENODE,
  TCLDOM_ELEMENT_SETATTRIBUTENODE,
  TCLDOM_ELEMENT_REMOVEATTRIBUTENODE,
  TCLDOM_ELEMENT_GETATTRIBUTENODENS,
  TCLDOM_ELEMENT_SETATTRIBUTENODENS,
  TCLDOM_ELEMENT_REMOVEATTRIBUTENODENS,
  TCLDOM_ELEMENT_GETELEMENTSBYTAGNAME,
  TCLDOM_ELEMENT_NORMALIZE
};
static CONST84 char *ElementCommandOptions[] = {
  "-tagName",
  "-empty",
  (char *) NULL
};
enum ElementCommandOptions {
  TCLDOM_ELEMENT_TAGNAME,
  TCLDOM_ELEMENT_EMPTY
};
static CONST84 char *EventCommandMethods[] = {
  "cget",
  "configure",
  "stopPropagation",
  "preventDefault",
  "initEvent",
  "initUIEvent",
  "initMouseEvent",
  "initMutationEvent",
  "postUIEvent",
  "postMouseEvent",
  "postMutationEvent",
  (char *) NULL
};
enum EventCommandMethods {
  TCLDOM_EVENT_CGET,
  TCLDOM_EVENT_CONFIGURE,
  TCLDOM_EVENT_STOPPROPAGATION,
  TCLDOM_EVENT_PREVENTDEFAULT,
  TCLDOM_EVENT_INITEVENT,
  TCLDOM_EVENT_INITUIEVENT,
  TCLDOM_EVENT_INITMOUSEEVENT,
  TCLDOM_EVENT_INITMUTATIONEVENT,
  TCLDOM_EVENT_POSTUIEVENT,
  TCLDOM_EVENT_POSTMOUSEEVENT,
  TCLDOM_EVENT_POSTMUTATIONEVENT
};
static CONST84 char *EventCommandOptions[] = {
  "-altKey",
  "-attrName",
  "-attrChange",
  "-bubbles",
  "-button",
  "-cancelable",
  "-clientX",
  "-clientY",
  "-ctrlKey",
  "-currentNode",
  "-detail",
  "-eventPhase",
  "-metaKey",
  "-newValue",
  "-prevValue",
  "-relatedNode",
  "-screenX",
  "-screenY",
  "-shiftKey",
  "-target",
  "-timeStamp",
  "-type",
  "-view",
  (char *) NULL
};
enum EventCommandOptions {
  TCLDOM_EVENT_ALTKEY,
  TCLDOM_EVENT_ATTRNAME,
  TCLDOM_EVENT_ATTRCHANGE,
  TCLDOM_EVENT_BUBBLES,
  TCLDOM_EVENT_BUTTON,
  TCLDOM_EVENT_CANCELABLE,
  TCLDOM_EVENT_CLIENTX,
  TCLDOM_EVENT_CLIENTY,
  TCLDOM_EVENT_CTRLKEY,
  TCLDOM_EVENT_CURRENTNODE,
  TCLDOM_EVENT_DETAIL,
  TCLDOM_EVENT_EVENTPHASE,
  TCLDOM_EVENT_METAKEY,
  TCLDOM_EVENT_NEWVALUE,
  TCLDOM_EVENT_PREVVALUE,
  TCLDOM_EVENT_RELATEDNODE,
  TCLDOM_EVENT_SCREENX,
  TCLDOM_EVENT_SCREENY,
  TCLDOM_EVENT_SHIFTKEY,
  TCLDOM_EVENT_TARGET,
  TCLDOM_EVENT_TIMESTAMP,
  TCLDOM_EVENT_TYPE,
  TCLDOM_EVENT_VIEW
};
static CONST84 char *EventTypes[] = {
  "DOMFocusIn",
  "DOMFocusOut",
  "DOMActivate",
  "click",
  "mousedown",
  "mouseup",
  "mouseover",
  "mousemove",
  "mouseout",
  "DOMSubtreeModified",
  "DOMNodeInserted",
  "DOMNodeRemoved",
  "DOMNodeInsertedIntoDocument",
  "DOMNodeRemovedFromDocument",
  "DOMAttrModified",
  "DOMCharacterDataModified"
};
enum EventTypes {
  TCLDOM_EVENT_DOMFOCUSIN,
  TCLDOM_EVENT_DOMFOCUSOUT,
  TCLDOM_EVENT_DOMACTIVATE,
  TCLDOM_EVENT_CLICK,
  TCLDOM_EVENT_MOUSEDOWN,
  TCLDOM_EVENT_MOUSEUP,
  TCLDOM_EVENT_MOUSEOVER,
  TCLDOM_EVENT_MOUSEMOVE,
  TCLDOM_EVENT_MOUSEOUT,
  TCLDOM_EVENT_DOMSUBTREEMODIFIED,
  TCLDOM_EVENT_DOMNODEINSERTED,
  TCLDOM_EVENT_DOMNODEREMOVED,
  TCLDOM_EVENT_DOMNODEINSERTEDINTODOCUMENT,
  TCLDOM_EVENT_DOMNODEREMOVEDFROMDOCUMENT,
  TCLDOM_EVENT_DOMATTRMODIFIED,
  TCLDOM_EVENT_DOMCHARACTERDATAMODIFIED
};

static CONST84 char *ParseCommandOptions[] = {
  "-baseuri",
  "-externalentitycommand",
  (char *) NULL
};
enum ParseCommandOptions {
  TCLDOM_PARSE_BASEURI,
  TCLDOM_PARSE_EXTERNALENTITYCOMMAND
};
static CONST84 char *SerializeCommandOptions[] = {
  "-indent",
  "-method",
  (char *) NULL
};
enum SerializeCommandOptions {
  TCLDOM_SERIALIZE_INDENT,
  TCLDOM_SERIALIZE_METHOD,
};
static CONST84 char *SerializeMethods[] = {
  "xml",
  "html",
  "text",
  (char *) NULL
};
enum SerializeMethods {
  TCLDOM_SERIALIZE_METHOD_XML,
  TCLDOM_SERIALIZE_METHOD_HTML,
  TCLDOM_SERIALIZE_METHOD_TEXT
};
static CONST84 char *SelectNodeOptions[] = {
  "-namespaces",
  (char *) NULL
};
enum SelectNodeOptions {
  TCLDOM_SELECTNODE_OPTION_NAMESPACES
};

/*
 * Error context for passing error result back to caller.
 */

typedef struct GenericError_Info {
  Tcl_Interp *interp;
  int code;
  Tcl_Obj *msg;
} GenericError_Info;

/*
 * Default values
 */

EXTERN int xmlLoadExtDtdDefaultValue;


/*
 *----------------------------------------------------------------------------
 *
 * Tcldomxml_Init --
 *
 *  Initialisation routine for loadable module
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter,
 *
 *----------------------------------------------------------------------------
 */

int
Tcldomxml_Init (interp)
     Tcl_Interp *interp;	/* Interpreter to initialise */
{
  TclDOM_Implementation *implPtr;
  int dbgMode;

#ifdef USE_TCL_STUBS
  if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
    return TCL_ERROR;
  }
#endif
#ifdef USE_TCLDOM_STUBS
  if (Tcldom_InitStubs(interp, TCLDOM_VERSION, 1) == NULL) {
    return TCL_ERROR;
  }
#endif

  if (Tcl_PkgRequire(interp, "xml", TCLDOMXML_VERSION, 0) == NULL) {
    Tcl_SetResult(interp, "unable to load XML package", NULL);
    return TCL_ERROR;
  }

  stderrChan = Tcl_GetChannel(interp, "stderr", &dbgMode);

  /* Configure the libxml2 parser */
  xmlInitMemory();
  xmlSubstituteEntitiesDefault(1);

  /*
   * 6 will load external entities.
   * 0 will not.
   * TODO: provide configuration option for setting this value.
   */
  xmlLoadExtDtdDefaultValue = 6;

  xmlXPathInit();

  Tcl_RegisterObjType(&TclDOM_DocObjType);
  Tcl_RegisterObjType(&TclDOM_NodeObjType);
  Tcl_RegisterObjType(&TclDOM_EventObjType);

  /*
   * Register this DOM implementation with the TclDOM
   * generic layer.  We must do this for each separate
   * Tcl object type.
   */

  implPtr = (TclDOM_Implementation *) Tcl_Alloc(sizeof(TclDOM_Implementation));
  implPtr->name = Tcl_NewStringObj("libxml2-doc", -1);
  Tcl_IncrRefCount(implPtr->name);
  implPtr->type = &TclDOM_DocObjType;
  implPtr->create = TclDOMCreateCommand;
  implPtr->parse = TclDOMParseCommand;
  implPtr->serialize = TclDOMSerializeCommand;
  implPtr->document = TclDOMDocumentCommand;
  implPtr->documentfragment = NULL;
  implPtr->node = TclDOMNodeCommand;
  implPtr->element = NULL;
  implPtr->select = TclDOMSelectNodeCommand;
  TclDOM_Register(interp, implPtr);

  implPtr = (TclDOM_Implementation *) Tcl_Alloc(sizeof(TclDOM_Implementation));
  implPtr->name = Tcl_NewStringObj("libxml2-node", -1);
  Tcl_IncrRefCount(implPtr->name);
  implPtr->type = &TclDOM_NodeObjType;
  implPtr->create = TclDOMCreateCommand;
  implPtr->parse = TclDOMParseCommand;
  implPtr->serialize = TclDOMSerializeCommand;
  implPtr->document = TclDOMDocumentCommand;
  implPtr->documentfragment = NULL;
  implPtr->node = TclDOMNodeCommand;
  implPtr->element = TclDOMElementCommand;
  implPtr->select = TclDOMSelectNodeCommand;
  TclDOM_Register(interp, implPtr);

  Tcl_CreateObjCommand(interp, "dom::libxml2::DOMImplementation", TclDOMDOMImplementationCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::DOMImplementation", TclDOMDOMImplementationCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::hasfeature", TclDOM_HasFeatureCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::hasfeature", TclDOM_HasFeatureCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::document", TclDOMDocumentCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::document", TclDOMDocumentCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::node", TclDOMNodeCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::node", TclDOMNodeCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::create", TclDOMCreateCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::create", TclDOMCreateCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::parse", TclDOMParseCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::parse", TclDOMParseCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::serialize", TclDOMSerializeCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::serialize", TclDOMSerializeCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::selectnode", TclDOMSelectNodeCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::selectNode", TclDOMSelectNodeCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::element", TclDOMElementCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::element", TclDOMElementCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::event", TclDOMEventCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::event", TclDOMEventCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::validate", TclDOMValidateCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::validate", TclDOMValidateCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::xinclude", TclDOMXIncludeCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::xinclude", TclDOMXIncludeCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::prefix2namespaceURI", TclDOMPrefix2NSCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::prefix2namespaceURI", TclDOMPrefix2NSCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::libxml2::destroy", TclDOMDestroyCommand, NULL, NULL);
  Tcl_CreateObjCommand(interp, "dom::destroy", TclDOMDestroyCommand, NULL, NULL);

  Tcl_InitHashTable(&documents, TCL_STRING_KEYS);
  Tcl_InitHashTable(&docByPtr, TCL_ONE_WORD_KEYS);
  Tcl_InitHashTable(&captureListeners, TCL_ONE_WORD_KEYS);
  Tcl_InitHashTable(&bubbleListeners, TCL_ONE_WORD_KEYS);

  /* Setup name checking REs */
  checkName = Tcl_NewStringObj("^", -1);
  Tcl_AppendObjToObj(checkName, Tcl_GetVar2Ex(interp, "::xml::Name", NULL, 0));
  Tcl_AppendToObj(checkName, "$", -1);
  Tcl_IncrRefCount(checkName);
  checkQName = Tcl_NewStringObj("^", -1);
  Tcl_AppendObjToObj(checkQName, Tcl_GetVar2Ex(interp, "::xml::QName", NULL, 0));
  Tcl_AppendToObj(checkQName, "$", -1);
  Tcl_IncrRefCount(checkQName);

  #if TCL_DOES_STUBS
    {
      extern TcldomxmlStubs tcldomxmlStubs;
      if (Tcl_PkgProvideEx(interp, "dom::libxml2", TCLDOMXML_VERSION,
	(ClientData) &tcldomxmlStubs) != TCL_OK) {
        return TCL_ERROR;
      }
    }
  #else
    if (Tcl_PkgProvide(interp, "dom::libxml2", TCLDOMXML_VERSION) != TCL_OK) {
      return TCL_ERROR;
    }
  #endif

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_HasFeatureCommand --
 *
 *  Implements dom::libxml2::hasfeature command
 *
 * Results:
 *  Returns boolean.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_HasFeatureCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 0, objv, "hasfeature feature version");
    return TCL_ERROR;
  }

  if (Tcl_RegExpMatchObj(interp, objv[1], Tcl_NewStringObj("create|destroy|parse|query|serialize|trim|Events|UIEvents|isNode", -1)) == 1) {
    if (Tcl_StringMatch(Tcl_GetStringFromObj(objv[2], NULL), "1.0") == 1) {
      Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
    } else {
      Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
    }
  } else {
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
  }

  return TCL_OK;
}


/*
 *----------------------------------------------------------------------------
 *
 * TclDOMCreateCommand --
 *
 *  Implements dom::libxml2::create command
 *
 * Results:
 *  Creates a new document.
 *
 * Side effects:
 *  Allocates memory.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMCreateCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  if (objc != 1) {
    Tcl_WrongNumArgs(interp, 1, objv, "");
    return TCL_ERROR;
  }

  Tcl_SetObjResult(interp, TclDOM_NewDoc(interp));
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMDestroyCommand --
 *
 *  Implements dom::libxml2::destroy command
 *
 * Results:
 *  Frees document or node.
 *
 * Side effects:
 *  Deallocates memory.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMDestroyCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  TclDOMDocument *doc;
  xmlNodePtr node;
  TclDOMEvent *event;
  
  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "token");
    return TCL_ERROR;
  }

  if (TclDOM_GetDoc2FromObj(interp, objv[1], &doc) == TCL_OK) {
    if (TclDOMDestroyDocument(doc) != TCL_OK) {
      return TCL_ERROR;
    }
  } else if (TclDOM_GetNodeFromObj(interp, objv[1], &node) == TCL_OK) {
    if (TclDOMDestroyNode(node, objv[1]) != TCL_OK) {
      return TCL_ERROR;
    }
  } else if (TclDOM_GetEventFromObj(interp, objv[1], &event) == TCL_OK) {
    if (TclDOMDestroyEvent(event, objv[1]) != TCL_OK) {
      return TCL_ERROR;
    }
  } else {
    Tcl_SetResult(interp, "not a DOM node", NULL);
    return TCL_ERROR;
  }

  /* Invalidate the internal rep */
  objv[1]->typePtr = NULL;
  objv[1]->internalRep.otherValuePtr = NULL;

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMDestroyDocument --
 *
 *  Destroys an entire document
 *
 * Results:
 *  Frees document.
 *
 * Side effects:
 *  Deallocates memory.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMDestroyDocument (doc)
    TclDOMDocument *doc;
{
  Tcl_HashEntry *entry;
  Tcl_HashSearch search;
  xmlNodePtr node;
  TclDOMEvent *event;
  Tcl_Obj *objPtr;

  entry = Tcl_FindHashEntry(&documents, doc->token);
  if (!entry) {
    return TCL_ERROR;
  }
  Tcl_DeleteHashEntry(entry);
  /* Patch: rnurmi bug #593190 */
  entry = Tcl_FindHashEntry(&docByPtr, (ClientData) doc->docPtr);
  if (!entry) {
    return TCL_ERROR;
  }
  Tcl_DeleteHashEntry(entry);

  Tcl_Free(doc->token);

  entry = Tcl_FirstHashEntry(doc->nodes, &search);
  while (entry) {
    node = (xmlNodePtr) Tcl_GetHashValue(entry);
    objPtr = (Tcl_Obj *) node->_private;
    if (objPtr) {
      objPtr->typePtr = NULL;
      objPtr->internalRep.otherValuePtr = NULL;
      Tcl_DecrRefCount(objPtr);
    }
    entry = Tcl_NextHashEntry(&search);
  }
  Tcl_DeleteHashTable(doc->nodes);
  Tcl_Free((char*) doc->nodes);

  entry = Tcl_FirstHashEntry(doc->events, &search);
  while (entry) {
    event = (TclDOMEvent *) Tcl_GetHashValue(entry);
    objPtr = (Tcl_Obj *) event->objPtr;
    if (objPtr) {
      objPtr->typePtr = NULL;
      objPtr->internalRep.otherValuePtr = NULL;
    }
    Tcl_DeleteCommandFromToken(event->interp, event->cmd);

    entry = Tcl_NextHashEntry(&search);
  }
  Tcl_DeleteHashTable(doc->events);
  Tcl_Free((char*) doc->events);

  xmlFreeDoc(doc->docPtr);
  Tcl_Free((char *) doc);

  return TCL_OK;
}
/*
 *----------------------------------------------------------------------------
 *
 * TclDOMDestroyNode --
 *
 *  Destroys a node
 *
 * Results:
 *  Frees node.
 *
 * Side effects:
 *  Deallocates memory.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMForgetNode (node, objPtr)
    xmlNodePtr node;
    Tcl_Obj *objPtr;
{
  xmlDocPtr doc = node->doc;
  TclDOMDocument *tcldomDoc;
  Tcl_HashEntry *entry;

  entry = Tcl_FindHashEntry(&docByPtr, (ClientData) doc);
  if (!entry) {
    return TCL_ERROR;
  }
  tcldomDoc = (TclDOMDocument *) Tcl_GetHashValue(entry);

  entry = Tcl_FindHashEntry(tcldomDoc->nodes, Tcl_GetStringFromObj(objPtr, NULL));
  if (entry) {
    Tcl_DeleteHashEntry(entry);
  }

  node->_private = NULL;

  return TCL_OK;
}

int
TclDOMDestroyNode (node, objPtr)
    xmlNodePtr node;
    Tcl_Obj *objPtr;
{
  TclDOMForgetNode(node, objPtr);
  xmlFreeNode(node);

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMDestroyEvent --
 *
 *  Destroys an event node
 *
 * Results:
 *  Frees node.
 *
 * Side effects:
 *  Deallocates memory.
 *
 *----------------------------------------------------------------------------
 */

static void
TclDOMDeleteEvent(clientData)
    ClientData clientData;
{
  TclDOMEvent *event = (TclDOMEvent *) clientData;
  TclDOMDocument *doc = event->ownerDocument;
  Tcl_HashEntry *entry;

  entry = Tcl_FindHashEntry(doc->events, event->cmdname);
  if (entry) {
    Tcl_DeleteHashEntry(entry);
  }

  Tcl_Free((char *) event);

}

int
TclDOMDestroyEvent (event, objPtr)
    TclDOMEvent *event;
    Tcl_Obj *objPtr;
{
  Tcl_DeleteCommandFromToken(event->interp, event->cmd);

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMParseCommand --
 *
 *  Implements dom::libxml2::parse command
 *
 * Results:
 *  Depends on method.
 *
 * Side effects:
 *  Depends on method.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMParseCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  char *buf;
  int len, new, option;
  xmlParserCtxtPtr ctxt;
  xmlDocPtr docPtr;
  Tcl_Obj *objPtr;
  Tcl_HashEntry *entryPtr;
  Tcl_Obj *baseuriPtr = NULL;
  Tcl_Obj *extentity = NULL;
  ParserClientData *clientData;
  GenericError_Info *errorInfoPtr;
  void *oldErrorCtx;
  xmlGenericErrorFunc old_xmlGenericError;
  TclDOMDocument *tcldomDoc;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "xml ?args ...?");
    return TCL_ERROR;
  }

  buf = Tcl_GetStringFromObj(objv[1], &len);
  if (buf == NULL) {
    Tcl_SetResult(interp, "unable to get document to parse", NULL);
    return TCL_ERROR;
  }

  /*
   * Process options
   */

  objc -= 2;
  objv += 2;
  while (objc) {
    if (objc == 1) {
      Tcl_Obj *msgPtr;

      msgPtr = Tcl_NewStringObj("missing value for configuration option \"", -1);
      Tcl_AppendObjToObj(msgPtr, objv[0]);
      Tcl_AppendStringsToObj(msgPtr, "\"", (char *) NULL);
      Tcl_SetObjResult(interp, msgPtr);
      return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[0], ParseCommandOptions,
			    "option", 0, &option) != TCL_OK) {
      return TCL_ERROR;
    }

    switch ((enum ParseCommandOptions) option) {
    case TCLDOM_PARSE_BASEURI:
      baseuriPtr = objv[1];
      break;

    case TCLDOM_PARSE_EXTERNALENTITYCOMMAND:
      extentity = objv[1];
      break;
    }

    objc -= 2;
    objv += 2;
  }

  /*
   * This does the real work
   */

  xmlInitParser();
  ctxt = xmlCreateMemoryParserCtxt(buf, len);
  if (ctxt == NULL) {
    /* Out of memory - we're in big trouble... */
    Tcl_SetResult(interp, "unable to allocate parser context", NULL);
    return TCL_ERROR;
  }

  /*
   * Use the _private field to store TclDOM data
   */

  clientData = (ParserClientData *) Tcl_Alloc(sizeof(ParserClientData));
  ctxt->_private = (char *) clientData;
  clientData->interp = interp;

  if (baseuriPtr) {
    ctxt->input->filename = Tcl_GetStringFromObj(baseuriPtr, NULL);
  }

  defaultLoader = xmlGetExternalEntityLoader();
  if (extentity) {
    clientData->externalentityloader = extentity;
    xmlSetExternalEntityLoader(TclDOMExternalEntityLoader);
  }

  /*
   * Create a generic error handler... just in case
   */

  errorInfoPtr = (GenericError_Info *) Tcl_Alloc(sizeof(GenericError_Info));
  errorInfoPtr->msg = NULL;
  errorInfoPtr->interp = interp;
  errorInfoPtr->code = TCL_OK;

  /*
   * Save the previous error context so that it can
   * be restored upon completion of parsing.
   */
  old_xmlGenericError = xmlGenericError;
  oldErrorCtx = xmlGenericErrorContext;

  xmlSetGenericErrorFunc((void *) errorInfoPtr, TclDOMGenericError);

  /* docPtr = xmlParseMemory(buf, len); */
  if (xmlParseDocument(ctxt) != 0) {
    docPtr = NULL;
  } else {
    docPtr = ctxt->myDoc;
  }

  xmlSetExternalEntityLoader(defaultLoader);
  /* SF bug #590473: murmi: moved to here, before xmlFreeParserCtxt */
  Tcl_Free(ctxt->_private);
  xmlFreeParserCtxt(ctxt);
  xmlCleanupParser();

  xmlSetGenericErrorFunc((void *) oldErrorCtx, old_xmlGenericError);

  if (docPtr == NULL) {
    if (errorInfoPtr->msg) {
      int code = errorInfoPtr->code;
      Tcl_SetObjResult(interp, errorInfoPtr->msg);
      Tcl_DecrRefCount(errorInfoPtr->msg);
      Tcl_Free((char *) errorInfoPtr);
      return code;
    } else {
      Tcl_SetResult(interp, "unable to parse document", NULL);
    }
    Tcl_Free((char *) errorInfoPtr);
    return TCL_ERROR;
  }

  Tcl_Free((char *) errorInfoPtr);

  /*
   * Make sure base URI is stored in the document.
   * Setting the input filename is insufficient.
   */

  if (baseuriPtr && docPtr->URL == NULL) {
    buf = Tcl_GetStringFromObj(baseuriPtr, &len);
    docPtr->URL = Tcl_Alloc(len + 1);
    strcpy((char *) docPtr->URL, buf);
  }

  /*
   * Wrap the document pointer in a Tcl object
   */

  tcldomDoc = (TclDOMDocument *) Tcl_Alloc(sizeof(TclDOMDocument));

  tcldomDoc->docPtr = docPtr;

  tcldomDoc->token = Tcl_Alloc(20);
  sprintf(tcldomDoc->token, "doc%d", docCntr++);

  entryPtr = Tcl_CreateHashEntry(&documents, tcldomDoc->token, &new);
  if (!new) {
    Tcl_Free(tcldomDoc->token);
    Tcl_SetResult(interp, "internal error: previously allocated token", NULL);
    return TCL_ERROR;
  }
  Tcl_SetHashValue(entryPtr, (void *) tcldomDoc);
  entryPtr = Tcl_CreateHashEntry(&docByPtr, (char *) docPtr, &new);
  if (!new) {
    Tcl_Free(buf);
    /* Delete above hash entry... */
    Tcl_SetResult(interp, "internal error: previously allocated token", NULL);
    return TCL_ERROR;
  }
  Tcl_SetHashValue(entryPtr, (void *) tcldomDoc);

  objPtr = Tcl_NewObj();
  objPtr->internalRep.otherValuePtr = (VOID *) tcldomDoc;
  objPtr->typePtr = &TclDOM_DocObjType;
  objPtr->bytes = Tcl_Alloc(20);
  strcpy(objPtr->bytes, tcldomDoc->token);
  objPtr->length = strlen(tcldomDoc->token);

  tcldomDoc->nodes = (Tcl_HashTable *) Tcl_Alloc(sizeof(Tcl_HashTable));
  tcldomDoc->nodeCntr = 0;
  Tcl_InitHashTable(tcldomDoc->nodes, TCL_STRING_KEYS);
  tcldomDoc->events = (Tcl_HashTable *) Tcl_Alloc(sizeof(Tcl_HashTable));
  tcldomDoc->eventCntr = 0;
  Tcl_InitHashTable(tcldomDoc->events, TCL_STRING_KEYS);
  for (new = 0; new < NUM_EVENT_TYPES; new++) {
    tcldomDoc->listening[new] = 0;
  }

  /*
   * Use the _private field to point back to the wrapping
   * object.  This allows convenient reuse of the
   * object.
   */

  docPtr->_private = (void *) objPtr;

  Tcl_SetObjResult(interp, objPtr);

  return TCL_OK;
}

/*
 * Interpose on resolving external entity references.
 *
 * The return code of the script evaluation determines
 * the behaviour:
 * TCL_OK - return value is to be used as the entity data
 * TCL_CONTINUE - use the default libxml2 entity loader
 * TCL_BREAK - forget about this entity
 * TCL_ERROR - background error
 */

static xmlParserInputPtr
TclDOMExternalEntityLoader(url, id, ctxt)
     const char *url;
     const char *id;
     xmlParserCtxtPtr ctxt;
{
  ParserClientData *clientData = (ParserClientData *) ctxt->_private;
  Tcl_Obj *cmdPtr;

  if (clientData) {
    cmdPtr = Tcl_DuplicateObj(clientData->externalentityloader);
  } else {
    return NULL;
  }

  if (url) {
    Tcl_ListObjAppendElement(clientData->interp, cmdPtr, Tcl_NewStringObj(url, -1));
  } else {
    Tcl_ListObjAppendElement(clientData->interp, cmdPtr, Tcl_NewListObj(0, NULL));
  }
  if (id) {
    Tcl_ListObjAppendElement(clientData->interp, cmdPtr, Tcl_NewStringObj(id, -1));
  } else {
    Tcl_ListObjAppendElement(clientData->interp, cmdPtr, Tcl_NewListObj(0, NULL));
  }

  switch (Tcl_GlobalEvalObj(clientData->interp, cmdPtr)) {

  case TCL_OK:
    /* TODO: Use the return value as the input */
    return (defaultLoader)(url, id, ctxt);

  case TCL_CONTINUE:
    /* Use the default libxml2 loader */
    return (defaultLoader)(url, id, ctxt);

  case TCL_BREAK:
    /* Do not load external entity, but no error */
    return NULL;

  default:
    Tcl_BackgroundError(clientData->interp);
    return NULL;
  }
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMSerializeCommand --
 *
 *  Implements dom::libxml2::serialize command
 *
 * Results:
 *  Depends on method.
 *
 * Side effects:
 *  Depends on method.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMSerializeCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  xmlDocPtr docPtr;
  xmlNodePtr nodePtr;
  xmlChar *result;
  int option, method = TCLDOM_SERIALIZE_METHOD_XML, indent = 0, len;
  char *buf;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "node ?option value ...?");
    return TCL_ERROR;
  }

  if (TclDOM_GetDocFromObj(interp, objv[1], &docPtr) != TCL_OK) {
    if (TclDOM_GetNodeFromObj(interp, objv[1], &nodePtr) == TCL_OK) {
      /* Serialize just the node */
      Tcl_SetResult(interp, "not yet implemented - serialize whole document", NULL);
      return TCL_ERROR;
    } else {
      Tcl_SetResult(interp, "not a libxml2 node", NULL);
      return TCL_ERROR;
    }
  }

  if (objc > 2) {
    objc -= 2;
    objv += 2;

    while (objc) {

      if (objc == 1) {
	Tcl_Obj *msgPtr;

	msgPtr = Tcl_NewStringObj("missing value for configuration option \"", -1);
	Tcl_AppendObjToObj(msgPtr, objv[0]);
	Tcl_AppendStringsToObj(msgPtr, "\"", (char *) NULL);
	Tcl_SetObjResult(interp, msgPtr);
	return TCL_ERROR;
      }

      if (Tcl_GetIndexFromObj(interp, objv[0], SerializeCommandOptions,
			    "option", 0, &option) != TCL_OK) {
	return TCL_ERROR;
      }

      switch ((enum SerializeCommandOptions) option) {
      case TCLDOM_SERIALIZE_METHOD:

	buf = Tcl_GetStringFromObj(objv[1], &len);
	if (len == 0) {
	  method = TCLDOM_SERIALIZE_METHOD_XML;
	} else if (Tcl_GetIndexFromObj(interp, objv[1], SerializeMethods,
				       "method", 0, &method) != TCL_OK) {
	  return TCL_ERROR;
	}

	break;

      case TCLDOM_SERIALIZE_INDENT:

	if (Tcl_GetBooleanFromObj(interp, objv[1], &indent) != TCL_OK) {
	  return TCL_ERROR;
	}

	break;

      default:
	Tcl_SetResult(interp, "unknown option", NULL);
	return TCL_ERROR;
      }

      objc -= 2;
      objv += 2;
    }
  }

  /* This code substantially borrowed from libxslt xsltutil.c */

  switch ((enum SerializeMethods) method) {
  case TCLDOM_SERIALIZE_METHOD_XML:
    xmlDocDumpFormatMemory(docPtr, &result, &len, indent);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(result, len));
    free(result);

    break;

  case TCLDOM_SERIALIZE_METHOD_HTML:

    htmlSetMetaEncoding(docPtr, (const xmlChar *) "UTF-8");
    htmlDocDumpMemory(docPtr, &result, &len);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(result, len));
    free(result);

    break;

  case TCLDOM_SERIALIZE_METHOD_TEXT:

    nodePtr = docPtr->children;

    while (nodePtr != NULL) {
      if (nodePtr->type = XML_TEXT_NODE)
	Tcl_AppendResult(interp, (char *) nodePtr->content, NULL);

      if (nodePtr->children != NULL) {
	if ((nodePtr->children->type != XML_ENTITY_DECL) &&
	    (nodePtr->children->type != XML_ENTITY_REF_NODE) &&
	    (nodePtr->children->type != XML_ENTITY_NODE)) {
	  nodePtr = nodePtr->children;
	  continue;
	}
      }

      if (nodePtr->next != NULL) {
	nodePtr = nodePtr->next;
	continue;
      }

      do {
	nodePtr = nodePtr->parent;
	if (nodePtr == NULL)
	  break;
	if (nodePtr == (xmlNodePtr) docPtr) {
	  nodePtr = NULL;
	  break;
	}
	if (nodePtr->next != NULL) {
	  nodePtr = nodePtr->next;
	  break;
	}
      } while (nodePtr != NULL);
    }

    break;

  default:
    Tcl_SetResult(interp, "internal error", NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMDOMImplementationCommand --
 *
 *  Implements dom::libxml2::DOMImplementation command
 *
 * Results:
 *  Depends on method.
 *
 * Side effects:
 *  Depends on method.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMDOMImplementationCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int method;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "method ?args...?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], DOMImplementationCommandMethods,
			  "method", 0, &method) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum DOMImplementationCommandMethods) method) {
  case TCLDOM_IMPL_HASFEATURE:
    return TclDOM_HasFeatureCommand(dummy, interp, objc - 1, objv + 1);
  case TCLDOM_IMPL_CREATE:
    if (objc == 2) {
      return TclDOMCreateCommand(dummy, interp, 1, objv);
    } else if (objc == 3) {
      Tcl_Obj *objPtr;
      xmlDocPtr docPtr;
      xmlNodePtr nodePtr;

      if (TclDOMCreateCommand(dummy, interp, 0, NULL) != TCL_OK) {
	return TCL_ERROR;
      }
      objPtr = Tcl_GetObjResult(interp);
      TclDOM_GetDocFromObj(interp, objPtr, &docPtr);
      nodePtr = xmlNewDocNode(docPtr, NULL, Tcl_GetStringFromObj(objv[2], NULL), NULL);
      if (nodePtr == NULL) {
	Tcl_SetResult(interp, "unable to create document element", NULL);
        TclDOMDestroyDocument((TclDOMDocument *) objPtr->internalRep.otherValuePtr);
	Tcl_DecrRefCount(objPtr);
	return TCL_ERROR;
      }

      Tcl_SetObjResult(interp, objPtr);
    } else {
      Tcl_WrongNumArgs(interp, 1, objv, "create ?doc?");
      return TCL_ERROR;
    }

    break;

  case TCLDOM_IMPL_PARSE:
    return TclDOMParseCommand(dummy, interp, objc - 1, objv + 1);

  case TCLDOM_IMPL_SERIALIZE:
    return TclDOMSerializeCommand(dummy, interp, objc - 1, objv + 1);

  case TCLDOM_IMPL_SELECTNODE:
    return TclDOMSelectNodeCommand(dummy, interp, objc - 1, objv + 1);

  case TCLDOM_IMPL_DESTROY:
    return TclDOMDestroyCommand(dummy, interp, objc - 1, objv + 1);

  default:
    Tcl_SetResult(interp, "method \"", NULL);
    Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[1], NULL));
    Tcl_AppendResult(interp, "\" not yet implemented", NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMValidateCommand --
 *
 *  Implements dom::libxml2::validate command.
 *
 * Results:
 *  Returns result of validation on XML document.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

#ifdef WIN32
#if !defined (__CYGWIN__)
#define vsnprintf _vsnprintf
#endif /* __CYGWIN__ */
#endif /* WIN32 */

static void
TclDOMValidityError(void *userData, const char *format, ...)
{
  va_list args;
  Tcl_Interp *interp = (Tcl_Interp *) userData;
  char buf[1025];

  va_start(args,format);
  vsnprintf(buf, 1024, format, args);
  Tcl_AppendResult(interp, buf, NULL);
  va_end(args);

}

static void
TclDOMValidityWarning(void *userData, const char *format, ...) 
{
  va_list args;
  Tcl_Interp *interp = (Tcl_Interp *) userData;
  char buf[1025];

  va_start(args,format);
  vsnprintf(buf, 1024, format, args);
  Tcl_AppendResult(interp, buf, NULL);
  va_end(args);

}

int
TclDOMValidateCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  xmlDocPtr docPtr;
  xmlValidCtxt ctxt;
  GenericError_Info *errorInfoPtr;
  void *oldErrorCtx;
  xmlGenericErrorFunc old_xmlGenericError;

  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "doc");
    return TCL_ERROR;
  }

  if (TclDOM_GetDocFromObj(interp, objv[1], &docPtr) != TCL_OK) {
    return TCL_ERROR;
  }

  errorInfoPtr = (GenericError_Info *) Tcl_Alloc(sizeof(GenericError_Info));
  errorInfoPtr->msg = NULL;
  errorInfoPtr->interp = interp;
  errorInfoPtr->code = TCL_OK;

  /*
   * Save the previous error context so that it can
   * be restored upon completion of parsing.
   */
  old_xmlGenericError = xmlGenericError;
  oldErrorCtx = xmlGenericErrorContext;

  xmlSetGenericErrorFunc((void *) errorInfoPtr, TclDOMGenericError);

  ctxt.userData = (void *) interp;
  ctxt.error = (xmlValidityErrorFunc) TclDOMValidityError;
  ctxt.warning = (xmlValidityWarningFunc) TclDOMValidityWarning;

  Tcl_SetResult(interp, "document is not valid\n", NULL);

  if (!xmlValidateDocument(&ctxt, docPtr)) {
    if (errorInfoPtr->msg) {
      Tcl_AppendObjToObj(Tcl_GetObjResult(interp), errorInfoPtr->msg);
    }
    Tcl_Free((char *) errorInfoPtr);
    xmlSetGenericErrorFunc((void *) oldErrorCtx, old_xmlGenericError);
    return TCL_ERROR;
  }

  xmlSetGenericErrorFunc((void *) oldErrorCtx, old_xmlGenericError);

  Tcl_ResetResult(interp);
  Tcl_Free((char *) errorInfoPtr);

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMXIncludeCommand --
 *
 *  Implements dom::libxml2::xinclude command.
 *
 * Results:
 *  Performs XInclude processing on a document.
 *
 * Side effects:
 *  The supplied DOM tree may be modified.
 *
 *----------------------------------------------------------------------------
 */
int
TclDOMXIncludeCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  xmlDocPtr docPtr;
  int subs;

  if (objc != 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "doc");
    return TCL_ERROR;
  }

  if (TclDOM_GetDocFromObj(interp, objv[1], &docPtr) != TCL_OK) {
    return TCL_ERROR;
  }

  subs = xmlXIncludeProcess(docPtr);
  if (subs < 0) {
    Tcl_SetResult(interp, "unable to complete XInclude processing", NULL);
    return TCL_ERROR;
  }

  Tcl_SetObjResult(interp, Tcl_NewIntObj(subs));
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMPrefix2NSCommand --
 *
 *  Implements dom::libxml2::prefix2namespaceURI command.
 *
 * Results:
 *  Returns namespace URI for a given prefix.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */
int
TclDOMPrefix2NSCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  xmlNodePtr nodePtr;
  xmlNsPtr nsPtr;

  if (objc != 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "node prefix");
    return TCL_ERROR;
  }

  if (TclDOM_GetNodeFromObj(interp, objv[1], &nodePtr) != TCL_OK) {
    return TCL_ERROR;
  }

  nsPtr = xmlSearchNs(nodePtr->doc, nodePtr, Tcl_GetStringFromObj(objv[2], NULL));

  if (!nsPtr) {
    Tcl_SetResult(interp, "no XML Namespace declaration", NULL);
    return TCL_ERROR;
  }

  Tcl_SetObjResult(interp, Tcl_NewStringObj(nsPtr->href, -1));
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMSelectNodeCommand --
 *
 *  Implements dom::libxml2::selectnode command.
 *
 * Results:
 *  Returns result of XPath expression evaluation.
 *
 * Side effects:
 *  Memory is allocated for Tcl object to return result.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMSelectNodeCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int i, len, option;
  char *path;
  Tcl_Obj *objPtr, *nsOptPtr = NULL;
  xmlDocPtr docPtr;
  xmlNodePtr nodePtr = NULL;
  xmlXPathContextPtr ctxt;
  xmlXPathObjectPtr xpathObj;
  GenericError_Info *errorInfoPtr;
  void *oldErrorCtx;
  xmlGenericErrorFunc old_xmlGenericError;

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 1, objv, "doc location-path ?option value...?");
    return TCL_ERROR;
  }

  path = Tcl_GetStringFromObj(objv[2], &len);
  if (len == 0) {
    return TCL_OK;
  }

  if (TclDOM_GetDocFromObj(interp, objv[1], &docPtr) != TCL_OK) {
    if (TclDOM_GetNodeFromObj(interp, objv[1], &nodePtr) == TCL_OK) {
      docPtr = nodePtr->doc;
    } else {
      return TCL_ERROR;
    }
  }

  for (i = 3; i < objc; i += 2) {
    if (i == objc - 1) {
      Tcl_AppendResult(interp, "missing value for option \"", Tcl_GetStringFromObj(objv[i], NULL), "\"", NULL);
      return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[i], SelectNodeOptions,
			  "option", 0, &option) != TCL_OK) {
      goto opt_error;
    }
    switch ((enum SelectNodeOptions) option) {

    case TCLDOM_SELECTNODE_OPTION_NAMESPACES:
      if (nsOptPtr) {
        if (Tcl_ListObjAppendList(interp, nsOptPtr, objv[i + 1]) != TCL_OK) {
          Tcl_SetResult(interp, "-namespaces option value must be a list", NULL);
          goto opt_error;
        }
      } else {
        nsOptPtr = Tcl_DuplicateObj(objv[i + 1]);
      }
      if (Tcl_ListObjLength(interp, nsOptPtr, &len) != TCL_OK) {
        Tcl_SetResult(interp, "-namespaces option value must be a list", NULL);
        goto opt_error;
      } else if (len % 2 != 0) {
        Tcl_SetResult(interp, "value missing from namespaces list", NULL);
        goto opt_error;
      }

      break;

    default:
      Tcl_AppendResult(interp, "unknown option \"", Tcl_GetStringFromObj(objv[i], NULL), "\"", NULL);
      goto opt_error;
    }
  }

  ctxt = xmlXPathNewContext(docPtr);
  if (ctxt == NULL) {
    Tcl_SetResult(interp, "unable to create XPath context", NULL);
    return TCL_ERROR;
  }

  if (nodePtr) {
    ctxt->node = nodePtr;
  }

  /*
   * Create a generic error handler... just in case
   */

  errorInfoPtr = (GenericError_Info *) Tcl_Alloc(sizeof(GenericError_Info));
  errorInfoPtr->msg = NULL;
  errorInfoPtr->interp = interp;
  errorInfoPtr->code = TCL_OK;

  /*
   * Save the previous error context so that it can
   * be restored upon completion of parsing.
   */
  old_xmlGenericError = xmlGenericError;
  oldErrorCtx = xmlGenericErrorContext;

  xmlSetGenericErrorFunc((void *) errorInfoPtr, TclDOMGenericError);

  /*
   * Setup any XML Namespace prefixes given as arguments
   */
  if (nsOptPtr) {
    Tcl_ListObjLength(interp, nsOptPtr, &len);
    for (i = 0; i < len; i += 2) {
      Tcl_Obj *prefixPtr, *nsURIPtr;

      Tcl_ListObjIndex(interp, nsOptPtr, i, &prefixPtr);
      Tcl_ListObjIndex(interp, nsOptPtr, i + 1, &nsURIPtr);
      if (xmlXPathRegisterNs(ctxt, Tcl_GetStringFromObj(prefixPtr, NULL), Tcl_GetStringFromObj(nsURIPtr, NULL))) {
        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, "unable to register XML Namespace \"", Tcl_GetStringFromObj(nsURIPtr, NULL), "\"", NULL);
        goto error;
      }
    }
  }

  xpathObj = xmlXPathEval(path, ctxt);

  xmlSetGenericErrorFunc((void *) oldErrorCtx, old_xmlGenericError);

  if (xpathObj == NULL) {
    if (errorInfoPtr->msg) {
      Tcl_SetObjResult(interp, errorInfoPtr->msg);
      Tcl_DecrRefCount(errorInfoPtr->msg);
      goto error;
    } else {
      Tcl_SetResult(interp, "error evaluating XPath location path", NULL);
      goto error;
    }
  }
  Tcl_Free((char *) errorInfoPtr);

  objPtr = Tcl_NewObj();
  switch (xpathObj->type) {

  case XPATH_NODESET:
    len = xmlXPathNodeSetGetLength(xpathObj->nodesetval);
    for (i = 0; i < len; i++) {
      nodePtr = xmlXPathNodeSetItem(xpathObj->nodesetval, i);
      Tcl_ListObjAppendElement(interp, objPtr, TclDOM_CreateObjFromNode(nodePtr));
    }
    break;

  case XPATH_BOOLEAN:
    Tcl_SetBooleanObj(objPtr, xpathObj->boolval);
    break;

  case XPATH_NUMBER:
    Tcl_SetDoubleObj(objPtr, xpathObj->floatval);
    break;

  case XPATH_STRING:
    Tcl_SetStringObj(objPtr, xpathObj->stringval, strlen(xpathObj->stringval));
    break;

  default:
    Tcl_SetResult(interp, "bad XPath object type", NULL);
    goto error2;
  }

  if (nsOptPtr) {
    Tcl_DecrRefCount(nsOptPtr);
  }
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(ctxt);

  Tcl_SetObjResult(interp, objPtr);
  return TCL_OK;

 opt_error:
  if (nsOptPtr) {
    Tcl_DecrRefCount(nsOptPtr);
    return TCL_ERROR;
  }

 error2:
  if (nsOptPtr) {
    Tcl_DecrRefCount(nsOptPtr);
  }
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(ctxt);
  return TCL_ERROR;

 error:
  if (nsOptPtr) {
    Tcl_DecrRefCount(nsOptPtr);
  }
  Tcl_Free((char *) errorInfoPtr);
  xmlXPathFreeContext(ctxt);
  return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMDocumentCommand --
 *
 *  Implements dom::libxml2::document command.
 *
 * Results:
 *  Depends on method.
 *
 * Side effects:
 *  Depends on method.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMDocumentCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int method, option, postMutationEvent = 0;
  xmlDocPtr docPtr = NULL;
  xmlNodePtr nodePtr = NULL, newNodePtr = NULL;
  xmlNsPtr nsPtr = NULL;
  Tcl_Obj *nodeObjPtr, *newNodeObjPtr = NULL;
  char *buf, *bufptr;

  if (objc < 3) {
    Tcl_WrongNumArgs(interp, 2, objv, "method token ?args...?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], DocumentCommandMethods,
			  "method", 0, &method) != TCL_OK) {
    return TCL_ERROR;
  }

  if (TclDOM_GetDocFromObj(interp, objv[2], &docPtr) != TCL_OK) {
    docPtr = NULL;
    if (TclDOM_GetNodeFromObj(interp, objv[2], &nodePtr) != TCL_OK) {
      return TCL_ERROR;
    } else {
      nodeObjPtr = objv[2];
    }
  }

  switch ((enum DocumentCommandMethods) method) {

  case TCLDOM_DOCUMENT_CGET:

    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 3, objv, "cget option");
      return TCL_ERROR;
    }

    if (!docPtr) {
      Tcl_SetResult(interp, "not a document", NULL);
      return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], DocumentCommandOptions,
			    "option", 0, &option) != TCL_OK) {
      return TCL_ERROR;
    }

    switch ((enum DocumentCommandOptions) option) {

    case TCLDOM_DOCUMENT_DOCTYPE:
      Tcl_SetResult(interp, "cget option \"", NULL);
      Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[3], NULL), NULL);
      Tcl_AppendResult(interp, "\" not yet implemented", NULL);
      return TCL_ERROR;

    case TCLDOM_DOCUMENT_IMPLEMENTATION:
      Tcl_SetResult(interp, "::dom::libxml2::DOMImplementation", NULL);
      return TCL_ERROR;

    case TCLDOM_DOCUMENT_DOCELEMENT:

      nodePtr = xmlDocGetRootElement(docPtr);

      if (nodePtr) {
	Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(nodePtr));
      } else {
	Tcl_ResetResult(interp);
	return TCL_OK;
      }

      break;

    default:
      Tcl_SetResult(interp, "unknown option", NULL);
      return TCL_ERROR;
    }

    break;

  case TCLDOM_DOCUMENT_CONFIGURE:

    if (objc == 4) {
      Tcl_Obj *newobjv[4];

      newobjv[0] = objv[0];
      newobjv[1] = Tcl_NewStringObj("cget", -1);
      newobjv[2] = objv[2];
      newobjv[3] = objv[3];

      if (TclDOMDocumentCommand(NULL, interp, 4, newobjv) != TCL_OK) {
	return TCL_ERROR;
      }

    } else {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "attribute \"", Tcl_GetStringFromObj(objv[3], NULL), "\" is read-only", NULL);
      return TCL_ERROR;
    }

    break;

  case TCLDOM_DOCUMENT_CREATEELEMENTNS:
    if (objc != 5) {
      Tcl_WrongNumArgs(interp, 2, objv, "token nsuri qualname");
      return TCL_ERROR;
    }

    /*
     * libxml2 doesn't check for invalid element name,
     * so must do that here.
     */
    if (Tcl_RegExpMatchObj(interp, objv[4], checkQName) == 0) {
      Tcl_SetResult(interp, "invalid element name", NULL);
      return TCL_ERROR;
    }

    /* Find localName of element */
    buf = Tcl_GetStringFromObj(objv[4], NULL);
    for (bufptr = buf; *bufptr != ':'; bufptr++) ;
    bufptr += 1;

    if (docPtr) {
      /* We're creating the document element, so must create the namespace too */
      xmlNodePtr old;
      char *prefix;

      newNodePtr = xmlNewDocNode(docPtr, NULL, bufptr, NULL);
      if (newNodePtr == NULL) {
        Tcl_SetResult(interp, "unable to create element node", NULL);
        return TCL_ERROR;
      }
      old = xmlDocSetRootElement(docPtr, newNodePtr);
      if (old) {
	xmlDocSetRootElement(docPtr, old);
	xmlFreeNode(newNodePtr);
	Tcl_SetResult(interp, "document element already exists", NULL);
	return TCL_ERROR;
      }

      prefix = Tcl_Alloc(bufptr - buf);
      strncpy(prefix, buf, bufptr - buf - 1);
      prefix[bufptr - buf - 1] = '\0';

      nsPtr = xmlNewNs(newNodePtr, Tcl_GetStringFromObj(objv[3], NULL), prefix);
      if (nsPtr == NULL) {
	Tcl_SetResult(interp, "unable to create XML Namespace", NULL);
	Tcl_Free(prefix);
	xmlUnlinkNode(newNodePtr);
	xmlFreeNode(newNodePtr);
	return TCL_ERROR;
      }

      xmlSetNs(newNodePtr, nsPtr);
      Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(newNodePtr));

    } else {
      /* Find XML Namespace */
      nsPtr = xmlSearchNsByHref(nodePtr->doc, nodePtr, Tcl_GetStringFromObj(objv[3], NULL));
      if (nsPtr == NULL) {
	char *prefix;

	prefix = Tcl_Alloc(bufptr - buf);
	strncpy(prefix, buf, bufptr - buf - 1);
	prefix[bufptr - buf - 1] = '\0';

	newNodePtr = xmlNewChild(nodePtr, NULL, bufptr, NULL);
	nsPtr = xmlNewNs(newNodePtr, Tcl_GetStringFromObj(objv[3], NULL), prefix);
	if (nsPtr == NULL) {
	  Tcl_SetResult(interp, "unable to create XML Namespace", NULL);
	  return TCL_ERROR;
	}
	xmlSetNs(newNodePtr, nsPtr);

      } else {
	newNodePtr = xmlNewChild(nodePtr, nsPtr, bufptr, NULL);
	if (newNodePtr == NULL) {
	  Tcl_SetResult(interp, "unable to create element node", NULL);
	  return TCL_ERROR;
	}
      }

      newNodeObjPtr = TclDOM_CreateObjFromNode(newNodePtr);

      postMutationEvent = 1;
    }

    break;

  case TCLDOM_DOCUMENT_CREATEELEMENT:

    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 2, objv, "token name");
      return TCL_ERROR;
    }

    /*
     * libxml2 doesn't check for invalid element name,
     * so must do that here.
     */
    if (Tcl_RegExpMatchObj(interp, objv[3], checkName) == 0) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "invalid element name \"", Tcl_GetStringFromObj(objv[3], NULL), "\"", NULL);
      return TCL_ERROR;
    }

    if (docPtr) {
      xmlNodePtr old;
      newNodePtr = xmlNewDocNode(docPtr, NULL, Tcl_GetStringFromObj(objv[3], NULL), NULL);
      if (newNodePtr == NULL) {
        Tcl_SetResult(interp, "unable to create element node", NULL);
        return TCL_ERROR;
      }
      old = xmlDocSetRootElement(docPtr, newNodePtr);
      if (old) {
	xmlDocSetRootElement(docPtr, old);
	xmlFreeNode(newNodePtr);
	Tcl_SetResult(interp, "document element already exists", NULL);
	return TCL_ERROR;
      }
      newNodeObjPtr = TclDOM_CreateObjFromNode(newNodePtr);
    } else {
      newNodePtr = xmlNewChild(nodePtr, NULL, Tcl_GetStringFromObj(objv[3], NULL), NULL);
      if (newNodePtr == NULL) {
        Tcl_SetResult(interp, "unable to create element node", NULL);
        return TCL_ERROR;
      }
      newNodeObjPtr = TclDOM_CreateObjFromNode(newNodePtr);
    }

    postMutationEvent = 1;

    break;

  case TCLDOM_DOCUMENT_CREATEDOCUMENTFRAGMENT:

    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 2, objv, "token");
      return TCL_ERROR;
    }

    if (docPtr) {
      newNodePtr = xmlNewDocFragment(docPtr);
    } else {
      newNodePtr = xmlNewDocFragment(nodePtr->doc);
    }
    if (newNodePtr == NULL) {
      Tcl_SetResult(interp, "unable to create document fragment", NULL);
      return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(newNodePtr));

    /* The node hasn't been inserted into the tree yet */
    postMutationEvent = 0;

    break;

  case TCLDOM_DOCUMENT_CREATETEXTNODE:

    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 2, objv, "token text");
      return TCL_ERROR;
    }

    if (docPtr) {
      char *content;
      int len;

      content = Tcl_GetStringFromObj(objv[3], &len);
      newNodePtr = xmlNewDocTextLen(docPtr, content, len);
      if (newNodePtr == NULL) {
        Tcl_SetResult(interp, "unable to create text node", NULL);
        return TCL_ERROR;
      }
      Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(newNodePtr));

      postMutationEvent = 0;

    } else {
      xmlNodePtr returnNode;
      char *content;
      int len;

      content = Tcl_GetStringFromObj(objv[3], &len);
      newNodePtr = xmlNewTextLen(content, len);
      if (newNodePtr == NULL) {
	Tcl_SetResult(interp, "creating text node failed", NULL);
	return TCL_ERROR;
      }
      returnNode = xmlAddChild(nodePtr, newNodePtr);
      if (returnNode == NULL) {
	xmlFreeNode(newNodePtr);
	Tcl_SetResult(interp, "add child failed", NULL);
	return TCL_ERROR;
      }

      newNodeObjPtr = TclDOM_CreateObjFromNode(newNodePtr);

      postMutationEvent = 1;
    }

    break;

  case TCLDOM_DOCUMENT_CREATECOMMENT:

    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 2, objv, "token data");
      return TCL_ERROR;
    }

    if (docPtr) {
      newNodePtr = xmlNewComment(Tcl_GetStringFromObj(objv[3], NULL));
      if (newNodePtr == NULL) {
        Tcl_SetResult(interp, "unable to create comment node", NULL);
        return TCL_ERROR;
      }
      Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(newNodePtr));

      postMutationEvent = 0;

    } else {
      newNodePtr = xmlNewDocComment(nodePtr->doc, Tcl_GetStringFromObj(objv[3], NULL));
      if (newNodePtr == NULL) {
        Tcl_SetResult(interp, "unable to create comment node", NULL);
        return TCL_ERROR;
      }
      xmlAddChild(nodePtr, newNodePtr);

      newNodeObjPtr = TclDOM_CreateObjFromNode(newNodePtr);

      postMutationEvent = 1;
    }

    break;

  case TCLDOM_DOCUMENT_CREATECDATASECTION:

    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 2, objv, "token text");
      return TCL_ERROR;
    }

    if (docPtr) {
      char *content;
      int len;

      content = Tcl_GetStringFromObj(objv[3], &len);
      newNodePtr = xmlNewDocTextLen(docPtr, content, len);
      if (newNodePtr == NULL) {
        Tcl_SetResult(interp, "unable to create text node", NULL);
        return TCL_ERROR;
      }
      Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(newNodePtr));

      postMutationEvent = 0;

    } else {
      char *content;
      int len;

      content = Tcl_GetStringFromObj(objv[3], &len);
      newNodePtr = xmlNewTextLen(content, len);
      if (newNodePtr == NULL) {
        Tcl_SetResult(interp, "unable to create text node", NULL);
        return TCL_ERROR;
      }
      xmlAddChild(nodePtr, newNodePtr);

      newNodeObjPtr = TclDOM_CreateObjFromNode(newNodePtr);

      postMutationEvent = 1;
    }

    break;

  case TCLDOM_DOCUMENT_CREATEPI:
    if (objc != 5) {
      Tcl_WrongNumArgs(interp, 2, objv, "token target data");
      return TCL_ERROR;
    }

    newNodePtr = xmlNewPI(Tcl_GetStringFromObj(objv[3], NULL), Tcl_GetStringFromObj(objv[4], NULL));
    if (newNodePtr == NULL) {
      Tcl_SetResult(interp, "unable to create processing instruction node", NULL);
      return TCL_ERROR;
    }

    if (docPtr) {
      Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(newNodePtr));

      postMutationEvent = 0;

    } else {
      xmlAddChild(nodePtr, newNodePtr);

      newNodeObjPtr = TclDOM_CreateObjFromNode(newNodePtr);

      postMutationEvent = 1;
    }

    break;

  case TCLDOM_DOCUMENT_CREATEEVENT:

    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 2, objv, "token type");
    }

    if (!docPtr) {
      docPtr = nodePtr->doc;
    }

    Tcl_SetObjResult(interp, TclDOMNewEvent(interp, docPtr, objv[3]));

    break;

  case TCLDOM_DOCUMENT_CREATEATTRIBUTE:
  case TCLDOM_DOCUMENT_CREATEENTITY:
  case TCLDOM_DOCUMENT_CREATEENTITYREFERENCE:
  case TCLDOM_DOCUMENT_CREATEDOCTYPEDECL:
  default:
    Tcl_SetResult(interp, "method \"", NULL);
    Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[1], NULL), "\" not yet implemented", NULL);
    return TCL_ERROR;
  }

  if (postMutationEvent) {


    TclDOM_PostMutationEvent(interp, docPtr == NULL ? nodePtr->doc : docPtr, newNodeObjPtr, Tcl_NewStringObj("DOMNodeInserted", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), objv[2], NULL, NULL, NULL, NULL);
    TclDOM_PostMutationEvent(interp, docPtr == NULL ? nodePtr->doc : docPtr, newNodeObjPtr, Tcl_NewStringObj("DOMNodeInsertedIntoDocument", -1), Tcl_NewIntObj(0), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);

    if (nodePtr) {
      TclDOM_PostMutationEvent(interp, nodePtr->doc, nodeObjPtr, Tcl_NewStringObj("DOMSubtreeModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);
    } else {
      /*
       * We just added the document element.
       */
    }

    Tcl_SetObjResult(interp, newNodeObjPtr);
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TriggerEventListeners --
 *
 *  Iterates through the list of event listeners for
 *  a node or document and fires events.
 *
 * Results:
 *  Depends on listeners.
 *
 * Side effects:
 *  Depends on listeners.
 *
 *----------------------------------------------------------------------------
 */

static int
TriggerEventListeners(interp, type, tokenPtr, eventObjPtr, eventPtr)
     Tcl_Interp *interp;
     Tcl_HashTable *type;
     void *tokenPtr;
     Tcl_Obj *eventObjPtr;
     TclDOMEvent *eventPtr;
{
  Tcl_HashEntry *entryPtr;
  Tcl_HashTable *tablePtr;
  Tcl_Obj *listenerListPtr;
  int listenerLen, listenerIdx;

  entryPtr = Tcl_FindHashEntry(type, tokenPtr);
  if (!entryPtr) {
    return TCL_OK;
  }
  tablePtr = (Tcl_HashTable *) Tcl_GetHashValue(entryPtr);

  entryPtr = Tcl_FindHashEntry(tablePtr, Tcl_GetStringFromObj(eventPtr->type, NULL));
  if (!entryPtr) {
    return TCL_OK;
  }
  listenerListPtr = (Tcl_Obj *) Tcl_GetHashValue(entryPtr);

  /*
   * DOM L2 specifies that the ancestors are determined
   * at the moment of event dispatch, so using a static
   * list is the correct thing to do.
   */

  Tcl_ListObjLength(interp, listenerListPtr, &listenerLen);
  for (listenerIdx = 0; listenerIdx < listenerLen; listenerIdx++) {
    Tcl_Obj *listenerObj;
    Tcl_Obj **objv;
    int objc;

    Tcl_ListObjIndex(interp, listenerListPtr, listenerIdx, &listenerObj);

    /*
     * BUG workaround: eval'ing the command loses the event
     * object's internal rep.  By the time it gets to EventSetFromAny
     * the Tcl object seems to be corrupt.  As a workaround,
     * assume that the listener script is a single command
     * and invoke directly.
     *

    cmdPtr = Tcl_DuplicateObj(listenerObj);
    if (Tcl_ListObjAppendElement(interp, cmdPtr, eventPtr) != TCL_OK) {
      return TCL_ERROR;
    }
    if (Tcl_GlobalEvalObj(interp, cmdPtr) != TCL_OK) {
      Tcl_BackgroundError(interp);
    }
    */

    if (Tcl_ListObjGetElements(interp, listenerObj, &objc, &objv) != TCL_OK) {
      Tcl_BackgroundError(interp);
    } else {
      Tcl_Obj **newobjv = (Tcl_Obj **) Tcl_Alloc((objc + 2) * sizeof(Tcl_Obj *));
      int count;

      for (count = 0; count < objc; count++) {
	newobjv[count] = objv[count];
      }
      newobjv[count] = eventObjPtr;
      newobjv[count + 1] = NULL;

      if (Tcl_EvalObjv(interp, count + 1, newobjv, TCL_EVAL_GLOBAL) != TCL_OK) {
	Tcl_BackgroundError(interp);
      }
    }

  }

  return TCL_OK;
}

static int
TclDOMSetLiveNodeListNode(interp, varName, nodePtr)
    Tcl_Interp *interp;
    char *varName;
    xmlNodePtr nodePtr;
{
  Tcl_Obj *valuePtr = Tcl_NewListObj(0, NULL);
  xmlNodePtr childPtr;

  for (childPtr = nodePtr->children; childPtr; childPtr = childPtr->next) {
    Tcl_ListObjAppendElement(interp, valuePtr, TclDOM_CreateObjFromNode(childPtr));
  }

  Tcl_SetVar2Ex(interp, varName, NULL, valuePtr, TCL_GLOBAL_ONLY);

  return TCL_OK;
}

static int
TclDOMSetLiveNodeListDoc(interp, varName, docPtr)
    Tcl_Interp *interp;
    char *varName;
    xmlDocPtr docPtr;
{
  Tcl_Obj *valuePtr = Tcl_NewListObj(0, NULL);
  xmlNodePtr childPtr;

  for (childPtr = docPtr->children; childPtr; childPtr = childPtr->next) {
    Tcl_ListObjAppendElement(interp, valuePtr, TclDOM_CreateObjFromNode(childPtr));
  }

  Tcl_SetVar2Ex(interp, varName, NULL, valuePtr, TCL_GLOBAL_ONLY);

  return TCL_OK;
}

static char *
TclDOMLiveNodeListNode(clientData, interp, name1, name2, flags)
    ClientData clientData;
    Tcl_Interp *interp;
    char *name1;
    char *name2;
    int flags;
{
  xmlNodePtr nodePtr = (xmlNodePtr) clientData;

  if (flags & (TCL_INTERP_DESTROYED | TCL_TRACE_DESTROYED)) {
    return NULL;
  } else if (flags & TCL_TRACE_READS) {
    TclDOMSetLiveNodeListNode(interp, name1, nodePtr);
  } else if (flags & TCL_TRACE_WRITES) {
    TclDOMSetLiveNodeListNode(interp, name1, nodePtr);
    return "variable is read-only";
  } else if (flags & TCL_TRACE_UNSETS) {
  }

  return NULL;
}
static char *
TclDOMLiveNodeListDoc(clientData, interp, name1, name2, flags)
    ClientData clientData;
    Tcl_Interp *interp;
    char *name1;
    char *name2;
    int flags;
{
  xmlDocPtr docPtr = (xmlDocPtr) clientData;

  if (flags & (TCL_INTERP_DESTROYED | TCL_TRACE_DESTROYED)) {
    return NULL;
  } else if (flags & TCL_TRACE_READS) {
    TclDOMSetLiveNodeListDoc(interp, name1, docPtr);
  } else if (flags & TCL_TRACE_WRITES) {
    TclDOMSetLiveNodeListDoc(interp, name1, docPtr);
    return "variable is read-only";
  } else if (flags & TCL_TRACE_UNSETS) {
  }

  return NULL;
}

static int
TclDOMSetLiveNamedNodeMap(interp, varName, nodePtr)
    Tcl_Interp *interp;
    char *varName;
    xmlNodePtr nodePtr;
{
  xmlAttrPtr attrPtr;

  Tcl_UnsetVar(interp, varName, TCL_GLOBAL_ONLY);

  for (attrPtr = nodePtr->properties; attrPtr; attrPtr = attrPtr->next) {

    if (Tcl_SetVar2Ex(interp, varName, (char *) attrPtr->name, Tcl_NewStringObj(xmlGetProp(nodePtr, attrPtr->name), -1), TCL_GLOBAL_ONLY) == NULL) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp, "unable to set attribute \"", attrPtr->name, "\"", NULL);
      return TCL_ERROR;
    }

    if (Tcl_TraceVar2(interp, varName, (char *) attrPtr->name, TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS|TCL_GLOBAL_ONLY, TclDOMLiveNamedNodeMap, (ClientData) nodePtr) != TCL_OK) {
      return TCL_ERROR;
    }
  }

  return TCL_OK;
}

static char *
TclDOMLiveNamedNodeMap(clientData, interp, name1, name2, flags)
    ClientData clientData;
    Tcl_Interp *interp;
    char *name1;
    char *name2;
    int flags;
{
  xmlNodePtr nodePtr = (xmlNodePtr) clientData;

  if (flags & (TCL_INTERP_DESTROYED | TCL_TRACE_DESTROYED)) {
    return NULL;
  } else if (flags & TCL_TRACE_READS && name2 == NULL) {
    TclDOMSetLiveNamedNodeMap(interp, name1, nodePtr);
  } else if (flags & TCL_TRACE_READS && name2 != NULL) {
    if (Tcl_SetVar2Ex(interp, name1, name2, Tcl_NewStringObj(xmlGetProp(nodePtr, name2), -1), TCL_GLOBAL_ONLY) == NULL) {
      return "unable to set attribute";
    }
  } else if (flags & TCL_TRACE_WRITES) {
    TclDOMSetLiveNamedNodeMap(interp, name1, nodePtr);
    return "variable is read-only";
  } else if (flags & TCL_TRACE_UNSETS) {
  }

  return NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMNodeCommand --
 *
 *  Implements dom::libxml2::node command.
 *
 * Results:
 *  Depends on method.
 *
 * Side effects:
 *  Depends on method.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMNodeCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int method, option, len, usecapture = 0;
  char *buf, varname[100];
  xmlNodePtr nodePtr = NULL, childNodePtr, refPtr, newPtr, oldParent;
  xmlDocPtr docPtr = NULL;
  Tcl_Obj *nodeObjPtr = NULL;
  Tcl_Obj *docObjPtr = NULL;
  Tcl_Obj *objPtr, *resultPtr, *livePtr;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "method token ?arg ...?");
    return TCL_ERROR;
  }

/*
  Tcl_WriteChars(stderrChan, "NodeCommand", -1);
  for (method = 0; method < objc; method++) {
    char *dbgtype;
    char dbgnodebuf[200];
    sprintf(dbgnodebuf, "");
    if (objv[method]->typePtr) {
      dbgtype = objv[method]->typePtr->name;
      if (objv[method]->typePtr == &TclDOM_NodeObjType) {
        xmlNodePtr dbgnodeptr = (xmlNodePtr) objv[method]->internalRep.otherValuePtr;
        sprintf(dbgnodebuf, " name \"%s\" value \"%s\"", dbgnodeptr->name, XML_GET_CONTENT(dbgnodeptr));
      }
    } else {
      dbgtype = "(null)";
    }
    sprintf(dbgbuf, " \"%s\" (obj x%x type %s int x%x%s)", Tcl_GetStringFromObj(objv[method], NULL), objv[method], dbgtype, objv[method]->internalRep.otherValuePtr, dbgnodebuf);
    Tcl_WriteChars(stderrChan, dbgbuf, -1);
  }
  Tcl_WriteChars(stderrChan, "\n", -1);
*/

  if (Tcl_GetIndexFromObj(interp, objv[1], NodeCommandMethods,
			  "method", 0, &method) != TCL_OK) {
    return TCL_ERROR;
  }

  if (TclDOM_GetNodeFromObj(interp, objv[2], &nodePtr) != TCL_OK) {
    if (TclDOM_GetDocFromObj(interp, objv[2], &docPtr) != TCL_OK) {
      return TCL_ERROR;
    } else {
      docObjPtr = objv[2];
      nodePtr = NULL;
    }
  } else {
    nodeObjPtr = objv[2];
    docPtr = NULL;
  }

  switch ((enum NodeCommandMethods) method) {

  case TCLDOM_NODE_CGET:

    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 3, objv, "cget option");
      return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], NodeCommandOptions,
			    "option", 0, &option) != TCL_OK) {
      return TCL_ERROR;
    }

    switch ((enum NodeCommandOptions) option) {

    case TCLDOM_NODE_NODETYPE:

      if (docPtr) {
	Tcl_SetResult(interp, "document", NULL);
	break;
      }

      switch (nodePtr->type) {
      case XML_ELEMENT_NODE:
	Tcl_SetResult(interp, "element", NULL);
	break;
      case XML_ATTRIBUTE_NODE:
	Tcl_SetResult(interp, "attribute", NULL);
	break;
      case XML_TEXT_NODE:
      case XML_CDATA_SECTION_NODE:
	Tcl_SetResult(interp, "textNode", NULL);
	break;
      case XML_ENTITY_REF_NODE:
	Tcl_SetResult(interp, "entityReference", NULL);
	break;
      case XML_ENTITY_NODE:
	Tcl_SetResult(interp, "entity", NULL);
	break;
      case XML_PI_NODE:
	Tcl_SetResult(interp, "processingInstruction", NULL);
	break;
      case XML_COMMENT_NODE:
	Tcl_SetResult(interp, "comment", NULL);
	break;
      case XML_DOCUMENT_NODE:
	Tcl_SetResult(interp, "document", NULL);
	break;
      case XML_DOCUMENT_TYPE_NODE:
	Tcl_SetResult(interp, "docType", NULL);
	break;
      case XML_DOCUMENT_FRAG_NODE:
	Tcl_SetResult(interp, "documentFragment", NULL);
	break;
      case XML_NOTATION_NODE:
	Tcl_SetResult(interp, "notation", NULL);
	break;
      case XML_HTML_DOCUMENT_NODE:
	Tcl_SetResult(interp, "HTMLdocument", NULL);
	break;
      case XML_DTD_NODE:
	Tcl_SetResult(interp, "dtd", NULL);
	break;
      case XML_ELEMENT_DECL:
	Tcl_SetResult(interp, "elementDecl", NULL);
	break;
      case XML_ATTRIBUTE_DECL:
	Tcl_SetResult(interp, "attributeDecl", NULL);
	break;
      case XML_ENTITY_DECL:
	Tcl_SetResult(interp, "entityDecl", NULL);
	break;
      case XML_NAMESPACE_DECL:
	Tcl_SetResult(interp, "namespaceDecl", NULL);
	break;
      case XML_XINCLUDE_START:
	Tcl_SetResult(interp, "xincludeStart", NULL);
	break;
      case XML_XINCLUDE_END:
	Tcl_SetResult(interp, "xincludeEnd", NULL);
	break;
      default:
	Tcl_SetResult(interp, "unknown", NULL);
      }

      break;

    case TCLDOM_NODE_LOCALNAME:
    case TCLDOM_NODE_NODENAME:

      /* This isn't quite right: nodeName should return the expanded name */

      if (docPtr) {
	Tcl_SetResult(interp, "#document", NULL);
	break;
      }
      /* libxml2 doesn't maintain the correct DOM node name */
      switch (nodePtr->type) {
      case XML_ELEMENT_NODE:
      case XML_ATTRIBUTE_NODE:
      case XML_ENTITY_REF_NODE:
      case XML_ENTITY_NODE:
      case XML_PI_NODE:
      case XML_DOCUMENT_TYPE_NODE:
      case XML_NOTATION_NODE:
	Tcl_SetObjResult(interp, Tcl_NewStringObj(nodePtr->name, -1));
	break;
      case XML_TEXT_NODE:
	Tcl_SetResult(interp, "#text", NULL);
	break;
      case XML_CDATA_SECTION_NODE:
	Tcl_SetResult(interp, "#cdata-section", NULL);
	break;
      case XML_COMMENT_NODE:
	Tcl_SetResult(interp, "#comment", NULL);
	break;
      case XML_DOCUMENT_NODE:
	/* Already handled above */
	Tcl_SetResult(interp, "#document", NULL);
	break;
      case XML_DOCUMENT_FRAG_NODE:
	Tcl_SetResult(interp, "#document-fragment", NULL);
	break;
      case XML_HTML_DOCUMENT_NODE:
	/* Not standard DOM */
	Tcl_SetResult(interp, "#HTML-document", NULL);
	break;
      case XML_DTD_NODE:
	/* Not standard DOM */
	Tcl_SetResult(interp, "#dtd", NULL);
	break;
      case XML_ELEMENT_DECL:
	/* Not standard DOM */
	Tcl_SetResult(interp, "#element-declaration", NULL);
	break;
      case XML_ATTRIBUTE_DECL:
	/* Not standard DOM */
	Tcl_SetResult(interp, "#attribute-declaration", NULL);
	break;
      case XML_ENTITY_DECL:
	/* Not standard DOM */
	Tcl_SetResult(interp, "#entity-declaration", NULL);
	break;
      case XML_NAMESPACE_DECL:
	/* Not standard DOM */
	Tcl_SetResult(interp, "#namespace-declaration", NULL);
	break;
      case XML_XINCLUDE_START:
	/* Not standard DOM */
	Tcl_SetResult(interp, "#xinclude-start", NULL);
	break;
      case XML_XINCLUDE_END:
	/* Not standard DOM */
	Tcl_SetResult(interp, "#xinclude-end", NULL);
	break;
      default:
	Tcl_SetResult(interp, "#unknown", NULL);
      }

      break;

    case TCLDOM_NODE_NODEVALUE:

      if (docPtr) {
	break;
      }

      if (XML_GET_CONTENT(nodePtr) != NULL) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(XML_GET_CONTENT(nodePtr), -1));
      }

      break;

    case TCLDOM_NODE_OWNERDOCUMENT:

      if (docPtr) {
	Tcl_SetObjResult(interp, objv[2]);
	break;
      }

      Tcl_SetObjResult(interp, TclDOM_CreateObjFromDoc(nodePtr->doc));

      break;

    case TCLDOM_NODE_PARENTNODE:

      if (docPtr) {
	Tcl_ResetResult(interp);
	break;
      }

      if (nodePtr->parent) {
	Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(nodePtr->parent));
      } else {
	Tcl_SetObjResult(interp, TclDOM_CreateObjFromDoc(nodePtr->doc));
      }

      break;

    case TCLDOM_NODE_CHILDNODES:

      /* Set up live NodeList variable */

      if (docPtr) {
	sprintf(varname, "::dom::libxml2::nodelist.%s", Tcl_GetStringFromObj(docObjPtr, NULL));
      } else {
	sprintf(varname, "::dom::libxml2::nodelist.%s", Tcl_GetStringFromObj(nodeObjPtr, NULL));
      }
      livePtr = Tcl_GetVar2Ex(interp, varname, NULL, TCL_GLOBAL_ONLY);
      if (!livePtr) {
	Tcl_Obj *nodelistPtr = Tcl_NewListObj(0, NULL);

	Tcl_SetVar2Ex(interp, varname, NULL, nodelistPtr, TCL_GLOBAL_ONLY);
	Tcl_IncrRefCount(nodelistPtr);

	if (docPtr) {
	  if (Tcl_TraceVar(interp, varname, TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS|TCL_GLOBAL_ONLY, TclDOMLiveNodeListDoc, (ClientData) docPtr) != TCL_OK) {
	    Tcl_DecrRefCount(nodelistPtr);
	    return TCL_ERROR;
	  } else {
	    TclDOMLiveNodeListDoc((ClientData) docPtr, interp, varname, NULL, TCL_TRACE_READS);
	  }
	} else {
	  if (Tcl_TraceVar(interp, varname, TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS|TCL_GLOBAL_ONLY, TclDOMLiveNodeListNode, (ClientData) nodePtr) != TCL_OK) {
	    Tcl_DecrRefCount(nodelistPtr);
	    return TCL_ERROR;
	  } else {
	    TclDOMLiveNodeListNode((ClientData) nodePtr, interp, varname, NULL, TCL_TRACE_READS);
	  }
	}
      }

      Tcl_SetObjResult(interp, Tcl_NewStringObj(varname, -1));

      break;

    case TCLDOM_NODE_FIRSTCHILD:

      if (docPtr) {
	childNodePtr = docPtr->children;
      } else {
	childNodePtr = nodePtr->children;
      }

      Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(childNodePtr));

      break;

    case TCLDOM_NODE_LASTCHILD:

      if (docPtr) {
	childNodePtr = docPtr->last;
      } else {
	childNodePtr = xmlGetLastChild(nodePtr);
      }
      if (childNodePtr != NULL) {
	Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(childNodePtr));
      }

      break;

    case TCLDOM_NODE_NEXTSIBLING:
      if (!docPtr && nodePtr->next) {
        Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(nodePtr->next));
      }
      
      break;
      
    case TCLDOM_NODE_PREVIOUSSIBLING:
      if (!docPtr && nodePtr->prev) {
        Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(nodePtr->prev));
      }

      break;

    case TCLDOM_NODE_ATTRIBUTES:

      if (docPtr) {
        Tcl_ResetResult(interp);
        return TCL_OK;
      } else if (nodePtr->type != XML_ELEMENT_NODE) {
        Tcl_SetResult(interp, "wrong object type", NULL);
        return TCL_ERROR;
      } else {
        /* Set up live NamedNodeMap variable */

        /* If there's already a variable, return it */
        sprintf(varname, "::dom::libxml2::att.%s", Tcl_GetStringFromObj(nodeObjPtr, NULL));
        livePtr = Tcl_GetVar2Ex(interp, varname, NULL, TCL_GLOBAL_ONLY);
        if (!livePtr) {
	  if (TclDOMSetLiveNamedNodeMap(interp, varname, (ClientData) nodePtr) != TCL_OK) {
	    Tcl_UnsetVar(interp, varname, TCL_GLOBAL_ONLY);
	    return TCL_ERROR;
	  }

          if (Tcl_TraceVar(interp, varname, TCL_TRACE_ARRAY|TCL_TRACE_READS|TCL_TRACE_WRITES|TCL_TRACE_UNSETS|TCL_GLOBAL_ONLY, TclDOMLiveNamedNodeMap, (ClientData) nodePtr) != TCL_OK) {
            Tcl_UnsetVar(interp, varname, TCL_GLOBAL_ONLY);
            return TCL_ERROR;
          }
        }

        Tcl_SetObjResult(interp, Tcl_NewStringObj(varname, -1));

      }

      break;

    case TCLDOM_NODE_NAMESPACEURI:

      if (!docPtr && nodePtr->ns) {
        if (nodePtr->ns->href) {
          Tcl_SetObjResult(interp, Tcl_NewStringObj(nodePtr->ns->href, -1));
        }
      }

      break;

    case TCLDOM_NODE_PREFIX:

      if (!docPtr && nodePtr->ns) {
        if (nodePtr->ns->prefix) {
          Tcl_SetObjResult(interp, Tcl_NewStringObj(nodePtr->ns->prefix, -1));
        }
      }

      break;

    default:
      Tcl_SetResult(interp, "unknown option or not yet implemented", NULL);
      return TCL_ERROR;
    }

    break;

  case TCLDOM_NODE_PATH:

    if (docPtr) {
      Tcl_Obj *newobjv[2];

      newobjv[0] = TclDOM_CreateObjFromDoc(docPtr);
      newobjv[1] = NULL;
      Tcl_SetObjResult(interp, Tcl_NewListObj(1, newobjv));
    } else {
      Tcl_SetObjResult(interp, TclDOMGetPath(interp, nodePtr));
    }

    break;

  case TCLDOM_NODE_CONFIGURE:

    if (objc < 4) {
      Tcl_WrongNumArgs(interp, 3, objv, "configure token option ?value? ?option value ...?");
      return TCL_ERROR;
    }

    if (objc == 4) {
      /* equivalent to cget */
      Tcl_Obj *newobjv[5];
      newobjv[0] = objv[0];
      newobjv[1] = Tcl_NewStringObj("cget", -1);
      newobjv[2] = objv[2];
      newobjv[3] = objv[3];
      newobjv[4] = NULL;
      return TclDOMNodeCommand(dummy, interp, 4, newobjv);
    }

    objc -= 3;
    objv += 3;
    while (objc) {
      if (objc == 1) {
	Tcl_SetResult(interp, "missing value", NULL);
	return TCL_ERROR;
      }

      if (Tcl_GetIndexFromObj(interp, objv[0], NodeCommandOptions,
			    "option", 0, &option) != TCL_OK) {
	return TCL_ERROR;
      }

      switch ((enum NodeCommandOptions) option) {
      case TCLDOM_NODE_NODETYPE:
      case TCLDOM_NODE_NODENAME:
      case TCLDOM_NODE_PARENTNODE:
      case TCLDOM_NODE_CHILDNODES:
      case TCLDOM_NODE_FIRSTCHILD:
      case TCLDOM_NODE_LASTCHILD:
      case TCLDOM_NODE_PREVIOUSSIBLING:
      case TCLDOM_NODE_NEXTSIBLING:
      case TCLDOM_NODE_ATTRIBUTES:
      case TCLDOM_NODE_NAMESPACEURI:
      case TCLDOM_NODE_PREFIX:
      case TCLDOM_NODE_LOCALNAME:
      case TCLDOM_NODE_OWNERDOCUMENT:

	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "attribute \"", Tcl_GetStringFromObj(objv[0], NULL), "\" is read-only", NULL);
	return TCL_ERROR;

      case TCLDOM_NODE_NODEVALUE:

        if (docPtr) {
	  Tcl_ResetResult(interp);
	  return TCL_OK;
	} else {
          docPtr = nodePtr->doc;
        }

	switch (nodePtr->type) {
	case XML_ELEMENT_NODE:
	case XML_DOCUMENT_NODE:
	case XML_DOCUMENT_FRAG_NODE:
	case XML_DOCUMENT_TYPE_NODE:
	case XML_ENTITY_NODE:
	case XML_ENTITY_REF_NODE:
	case XML_NOTATION_NODE:
	case XML_HTML_DOCUMENT_NODE:
	case XML_DTD_NODE:
	case XML_ELEMENT_DECL:
	case XML_ATTRIBUTE_DECL:
	case XML_ENTITY_DECL:
	case XML_NAMESPACE_DECL:
	case XML_XINCLUDE_START:
	case XML_XINCLUDE_END:
	  /*
	   * DOM defines these nodes as not having a node value.
	   * libxml2 clobbers existing content if the value is set,
	   * so don't do it!
	   */
	  Tcl_ResetResult(interp);
	  return TCL_OK;

	default:
	  /* fall-through */
	  break;
	}

        objPtr = Tcl_NewStringObj(xmlNodeGetContent(nodePtr), -1);

	buf = Tcl_GetStringFromObj(objv[1], &len);
	xmlNodeSetContentLen(nodePtr, buf, len);

        TclDOM_PostMutationEvent(interp, docPtr, nodeObjPtr, Tcl_NewStringObj("DOMCharacterDataModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, objPtr, objv[1], NULL, NULL);

        Tcl_DecrRefCount(objPtr);

	break;

      case TCLDOM_NODE_CDATASECTION:

	break;
      }

      objc -= 2;
      objv += 2;

    }

    break;

  case TCLDOM_NODE_INSERTBEFORE:
    if (objc < 4 || objc > 5) {
      Tcl_WrongNumArgs(interp, 2, objv, "token ref ?new?");
      return TCL_ERROR;
    } else if (docPtr) {
      /* TODO: allow appending comments & PIs */
      Tcl_SetResult(interp, "document already has document element", NULL);
      return TCL_ERROR;
    } else if (objc == 4) {
      xmlNodePtr oldSibling;

      /* No reference child specified - new appended to child list */
      if (TclDOM_GetNodeFromObj(interp, objv[3], &newPtr) != TCL_OK) {
        return TCL_ERROR;
      }
      refPtr = newPtr;
      oldParent = newPtr->parent;
      oldSibling = newPtr->next;
      if (oldParent != nodePtr) {
        TclDOM_PostMutationEvent(interp, nodePtr->doc, objv[3], Tcl_NewStringObj("DOMNodeRemoved", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), TclDOM_CreateObjFromNode(newPtr->parent), NULL, NULL, NULL, NULL);
      }
      /* Although xmlAddChild claims to release the child from its previous context,
       * that doesn't appear to actually happen.
       */
      xmlUnlinkNode(newPtr);
      if (xmlAddChild(nodePtr, newPtr) == NULL) {
	if (oldSibling) {
	  xmlAddPrevSibling(oldSibling, newPtr);
	} else {
	  xmlAddChild(oldParent, newPtr);
	}
        Tcl_SetResult(interp, "unable to insert node", NULL);
        return TCL_ERROR;
      }
    } else if (objc == 5) {
      if (TclDOM_GetNodeFromObj(interp, objv[3], &newPtr) != TCL_OK) {
        return TCL_ERROR;
      }
      if (TclDOM_GetNodeFromObj(interp, objv[4], &refPtr) != TCL_OK) {
        return TCL_ERROR;
      }
      oldParent = newPtr->parent;
      if (oldParent != refPtr->parent) {
        TclDOM_PostMutationEvent(interp, nodePtr->doc, objv[4], Tcl_NewStringObj("DOMNodeRemoved", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), TclDOM_CreateObjFromNode(newPtr->parent), NULL, NULL, NULL, NULL);
      }
      if (xmlAddPrevSibling(refPtr, newPtr) == NULL) {
        Tcl_SetResult(interp, "unable to insert node", NULL);
        return TCL_ERROR;
      }
    }

    /* If parent has changed, notify old parent */
    if (oldParent != NULL && oldParent != refPtr->parent) {
      TclDOM_PostMutationEvent(interp, nodePtr->doc, TclDOM_CreateObjFromNode(oldParent), Tcl_NewStringObj("DOMSubtreeModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);
    }
    /* Notify new parent */
    TclDOM_PostMutationEvent(interp, nodePtr->doc, TclDOM_CreateObjFromNode(newPtr->parent), Tcl_NewStringObj("DOMSubtreeModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);
    /* Inserted event */
    TclDOM_PostMutationEvent(interp, nodePtr->doc, TclDOM_CreateObjFromNode(newPtr), Tcl_NewStringObj("DOMNodeInserted", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);

    break;

  case TCLDOM_NODE_REPLACECHILD:
    if (objc !=  5) {
      Tcl_WrongNumArgs(interp, 2, objv, "token ref new");
      return TCL_ERROR;
    } else if (docPtr) {
      /* TODO: allow replacing comments & PIs */
      Tcl_SetResult(interp, "document already has document element", NULL);
      return TCL_ERROR;
    } else {
      if (TclDOM_GetNodeFromObj(interp, objv[3], &refPtr) != TCL_OK) {
        return TCL_ERROR;
      }
      if (TclDOM_GetNodeFromObj(interp, objv[4], &newPtr) != TCL_OK) {
        return TCL_ERROR;
      }
      oldParent = newPtr->parent;
      if (oldParent != refPtr->parent) {
        TclDOM_PostMutationEvent(interp, nodePtr->doc, TclDOM_CreateObjFromNode(newPtr), Tcl_NewStringObj("DOMNodeRemoved", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), TclDOM_CreateObjFromNode(newPtr->parent), NULL, NULL, NULL, NULL);
      }
      if (xmlReplaceNode(refPtr, newPtr) == NULL) {
        Tcl_SetResult(interp, "unable to replace node", NULL);
        return TCL_ERROR;
      }
    }

    /* If parent has changed, notify old parent */
    if (oldParent != NULL && oldParent != newPtr->parent) {
      TclDOM_PostMutationEvent(interp, nodePtr->doc, TclDOM_CreateObjFromNode(oldParent), Tcl_NewStringObj("DOMSubtreeModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);
    }
    /* Notify new parent */
    TclDOM_PostMutationEvent(interp, nodePtr->doc, TclDOM_CreateObjFromNode(newPtr->parent), Tcl_NewStringObj("DOMSubtreeModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);
    /* Inserted event */
    TclDOM_PostMutationEvent(interp, nodePtr->doc, TclDOM_CreateObjFromNode(newPtr), Tcl_NewStringObj("DOMNodeInserted", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);

    break;

  case TCLDOM_NODE_REMOVECHILD:
    if (objc !=  4) {
      Tcl_WrongNumArgs(interp, 2, objv, "node child");
      return TCL_ERROR;
    } else if (docPtr) {
      /* TODO: allow removing comments & PIs */
      Tcl_SetResult(interp, "document must have document element", NULL);
      return TCL_ERROR;
    } else {
      xmlNodePtr childPtr;
      if (TclDOM_GetNodeFromObj(interp, objv[3], &childPtr) != TCL_OK) {
        return TCL_ERROR;
      }
      oldParent = childPtr->parent;
      TclDOM_PostMutationEvent(interp, nodePtr->doc, objv[3], Tcl_NewStringObj("DOMNodeRemoved", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), TclDOM_CreateObjFromNode(oldParent), NULL, NULL, NULL, NULL);
      TclDOM_PostMutationEvent(interp, nodePtr->doc, objv[3], Tcl_NewStringObj("DOMNodeRemovedFromDocument", -1), Tcl_NewIntObj(0), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);
      xmlUnlinkNode(childPtr);
      Tcl_SetObjResult(interp, objv[3]);
      TclDOM_PostMutationEvent(interp, nodePtr->doc, TclDOM_CreateObjFromNode(oldParent), Tcl_NewStringObj("DOMSubtreeModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);
    }

    break;

  case TCLDOM_NODE_APPENDCHILD:
    if (objc !=  4) {
      Tcl_WrongNumArgs(interp, 2, objv, "node child");
      return TCL_ERROR;
    } else if (docPtr) {
      xmlNodePtr oldPtr;

      if (TclDOM_GetNodeFromObj(interp, objv[3], &childNodePtr) != TCL_OK) {
        return TCL_ERROR;
      }
      /* TODO: allow appending comments & PIs */
      oldPtr = xmlDocSetRootElement(docPtr, childNodePtr);
      if (oldPtr) {
        xmlDocSetRootElement(docPtr, oldPtr);
        Tcl_SetResult(interp, "document element already exists", NULL);
        return TCL_ERROR;
      }
    } else {
      if (TclDOM_GetNodeFromObj(interp, objv[3], &childNodePtr) != TCL_OK) {
        return TCL_ERROR;
      }
      oldParent = childNodePtr->parent;
      if (oldParent != NULL && oldParent != nodePtr) {
        TclDOM_PostMutationEvent(interp, nodePtr->doc, objv[3], Tcl_NewStringObj("DOMNodeRemoved", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), TclDOM_CreateObjFromNode(oldParent), NULL, NULL, NULL, NULL);
      }
      if (xmlAddChild(nodePtr, childNodePtr) == NULL) {
        Tcl_SetResult(interp, "unable to append node", NULL);
        return TCL_ERROR;
      }

      /* If parent has changed, notify old parent */
      if (oldParent != NULL && oldParent != nodePtr) {
        TclDOM_PostMutationEvent(interp, nodePtr->doc, TclDOM_CreateObjFromNode(oldParent), Tcl_NewStringObj("DOMSubtreeModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);
      }
      /* Notify new parent */
      TclDOM_PostMutationEvent(interp, nodePtr->doc, nodeObjPtr, Tcl_NewStringObj("DOMSubtreeModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, NULL, NULL, NULL, NULL);
      /* Inserted event */
      TclDOM_PostMutationEvent(interp, nodePtr->doc, objv[3], Tcl_NewStringObj("DOMNodeInserted", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), objv[2], NULL, NULL, NULL, NULL);

    }

    break;

  case TCLDOM_NODE_HASCHILDNODES:
    if (docPtr) {
      if (docPtr->children) {
       Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
      } else {
       Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
      }
    } else {
      if (nodePtr->children) {
       Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
      } else {
       Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
      }
    }

    break;

  case TCLDOM_NODE_ISSAMENODE:
    /* DOM Level 3 method */

    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 1, objv, "isSameNode node ref");
      return TCL_ERROR;
    }

    if (docPtr) {
      xmlDocPtr docRefPtr;

      if (TclDOM_GetDocFromObj(interp, objv[3], &docRefPtr) != TCL_OK) {
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
	return TCL_OK;
      }

      Tcl_SetObjResult(interp, Tcl_NewBooleanObj(docPtr == docRefPtr));

    } else {
      if (TclDOM_GetNodeFromObj(interp, objv[3], &refPtr) != TCL_OK) {
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
	return TCL_OK;
      }

      Tcl_SetObjResult(interp, Tcl_NewBooleanObj(nodePtr == refPtr));
    }

    break;

  case TCLDOM_NODE_CLONENODE:
    if (objc != 3 && objc != 5) {
      Tcl_WrongNumArgs(interp, 1, objv, "cloneNode token ?-deep boolean?");
      return TCL_ERROR;
    } else if (docPtr) {
      Tcl_SetResult(interp, "documents cannot be cloned", NULL);
      return TCL_ERROR;
    } else {
      int deep = 0;
      xmlNodePtr copyPtr;

      if (objc == 5) {
	if (Tcl_RegExpMatchObj(interp, objv[3], Tcl_NewStringObj("-de?e?p?", -1)) == 0) {
	  Tcl_ResetResult(interp);
	  Tcl_AppendResult(interp, "invalid option \"", Tcl_GetStringFromObj(objv[3], NULL), "\", must be \"-deep\"", NULL);
	  return TCL_ERROR;
	}
        if (Tcl_GetBooleanFromObj(interp, objv[4], &deep) != TCL_OK) {
          return TCL_ERROR;
        }
      }
      copyPtr = xmlDocCopyNode(nodePtr, nodePtr->doc, deep);
      if (copyPtr == NULL) {
        Tcl_SetResult(interp, "unable to copy node", NULL);
        return TCL_ERROR;
      }
      Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(copyPtr));
    }
    break;

  case TCLDOM_NODE_PARENT:

    if (docPtr) {
      break;
    }

    if (nodePtr->parent) {
      Tcl_SetObjResult(interp, TclDOM_CreateObjFromNode(nodePtr->parent));
    } else {
      Tcl_SetObjResult(interp, TclDOM_CreateObjFromDoc(nodePtr->doc));
    }

    break;

  case TCLDOM_NODE_CHILDREN:

    resultPtr = Tcl_NewListObj(0, NULL);

    if (docPtr) {
      childNodePtr = docPtr->children;
    } else {
      childNodePtr = nodePtr->children;
    }

    while (childNodePtr) {
      Tcl_ListObjAppendElement(interp, resultPtr, TclDOM_CreateObjFromNode(childNodePtr));
      childNodePtr = childNodePtr->next;
    }

    Tcl_SetObjResult(interp, resultPtr);

    break;

  case TCLDOM_NODE_ADDEVENTLISTENER:

    if (objc < 5) {
      Tcl_WrongNumArgs(interp, 2, objv, "addEventListener token type listener ?-usecapture boolean?");
      return TCL_ERROR;
    } else {
      Tcl_Obj *typePtr, *listenerPtr;
      void *tokenPtr = NULL;
      TclDOMDocument *tcldomdocPtr;

      if (nodePtr) {
	tokenPtr = (void *) nodePtr;
      } else {
	tokenPtr = (void *) docPtr;
      }

      typePtr = objv[3];
      listenerPtr = objv[4];

      objc -= 5;
      objv += 5;
      while (objc) {
	if (objc == 1) {
	  Tcl_SetResult(interp, "missing value", NULL);
	  return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(interp, objv[0], NodeCommandAddEventListenerOptions,
				"option", 0, &option) != TCL_OK) {
	  return TCL_ERROR;
	}
	switch ((enum NodeCommandAddEventListenerOptions) option) {
	case TCLDOM_NODE_ADDEVENTLISTENER_USECAPTURE:

	  if (Tcl_GetBooleanFromObj(interp, objv[1], &usecapture) != TCL_OK) {
	    return TCL_ERROR;
	  }

	  break;

	default:
	  Tcl_SetResult(interp, "unknown option", NULL);
	  return TCL_ERROR;
	}

	objc -= 2;
	objv += 2;
      }

      if (nodePtr) {
	docObjPtr = TclDOM_CreateObjFromDoc(nodePtr->doc);
      } else {
	docObjPtr = TclDOM_CreateObjFromDoc(docPtr);
      }
      TclDOM_GetDoc2FromObj(interp, docObjPtr, &tcldomdocPtr);

      return TclDOM_AddEventListener(interp, tcldomdocPtr, tokenPtr, typePtr, listenerPtr, usecapture);
    }

  case TCLDOM_NODE_REMOVEEVENTLISTENER:

    if (objc < 5) {
      Tcl_WrongNumArgs(interp, 2, objv, "removeEventListener token type listener ?-usecapture boolean?");
      return TCL_ERROR;
    } else {
      Tcl_Obj *typePtr, *listenerPtr;
      void *tokenPtr = NULL;
      TclDOMDocument *tcldomdocPtr;

      if (nodePtr) {
        tokenPtr = (void *) nodePtr;
      } else {
        tokenPtr = (void *) docPtr;
      }

      typePtr = objv[3];
      listenerPtr = objv[4];

      objc -= 5;
      objv += 5;
      while (objc) {
	if (Tcl_GetIndexFromObj(interp, objv[0], NodeCommandAddEventListenerOptions,
				"option", 0, &option) != TCL_OK) {
	  return TCL_ERROR;
	}
	switch ((enum NodeCommandAddEventListenerOptions) option) {
	case TCLDOM_NODE_ADDEVENTLISTENER_USECAPTURE:

	  if (Tcl_GetBooleanFromObj(interp, objv[1], &usecapture) != TCL_OK) {
	    return TCL_ERROR;
	  }

	  break;

	default:
	  Tcl_SetResult(interp, "unknown option", NULL);
	  return TCL_ERROR;
	}

	objc -= 2;
	objv += 2;
      }

      if (nodePtr) {
	docObjPtr = TclDOM_CreateObjFromDoc(nodePtr->doc);
      } else {
	docObjPtr = TclDOM_CreateObjFromDoc(docPtr);
      }
      TclDOM_GetDoc2FromObj(interp, docObjPtr, &tcldomdocPtr);

      return TclDOM_RemoveEventListener(interp, tcldomdocPtr, tokenPtr, typePtr, listenerPtr, usecapture);
    }

    break;

  case TCLDOM_NODE_DISPATCHEVENT:

    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 2, objv, "dispatchEvent token event");
      return TCL_ERROR;
    } else {
      TclDOMEvent *eventPtr;

      if (TclDOM_GetEventFromObj(interp, objv[3], &eventPtr) != TCL_OK) {
	return TCL_ERROR;
      }

      return TclDOM_DispatchEvent(interp, objv[2], objv[3], eventPtr);
    }

    break;

  case TCLDOM_NODE_STRINGVALUE:

    if (nodePtr) {
      buf = xmlNodeGetContent(nodePtr);
      Tcl_SetObjResult(interp, Tcl_NewStringObj(buf, -1));
      xmlFree (buf);
    } else if (docPtr) {
      nodePtr = xmlDocGetRootElement(docPtr);
      if (nodePtr) {
        buf = xmlNodeGetContent(nodePtr);
        Tcl_SetObjResult(interp, Tcl_NewStringObj(buf, -1));
        xmlFree (buf);
      }
    } else {
      Tcl_SetResult(interp, "cannot determine string value: internal error", NULL);
      return TCL_ERROR;
    }

    break;

  case TCLDOM_NODE_SELECTNODE:

    Tcl_ResetResult(interp);

    return TclDOMSelectNodeCommand(dummy, interp, objc - 1, objv + 1);

    break;

  default:
    Tcl_SetResult(interp, "method \"", NULL);
    Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[1], NULL), "\" not yet implemented", NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_AddEventListener --
 *
 *  Register an event listener.
 *
 * Results:
 *  Event listener stored.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_AddEventListener(interp, tcldomdocPtr, tokenPtr, typePtr, listenerPtr, capturer)
    Tcl_Interp *interp;
    TclDOMDocument *tcldomdocPtr;
    void *tokenPtr;
    Tcl_Obj *typePtr;
    Tcl_Obj *listenerPtr;
    int capturer;
{
  Tcl_HashTable *tablePtr;
  Tcl_HashEntry *entryPtr;
  int new, eventType;

  if (capturer) {
    tablePtr = &captureListeners;
  } else {
    tablePtr = &bubbleListeners;
  }

  entryPtr = Tcl_CreateHashEntry(tablePtr, tokenPtr, &new);
  if (new) {
    tablePtr = (Tcl_HashTable *) Tcl_Alloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tablePtr, TCL_STRING_KEYS);
    Tcl_SetHashValue(entryPtr, (char *) tablePtr);
  } else {
    tablePtr = (Tcl_HashTable *) Tcl_GetHashValue(entryPtr);
  }

  entryPtr = Tcl_CreateHashEntry(tablePtr, Tcl_GetStringFromObj(typePtr, NULL), &new);
  if (new) {
    Tcl_Obj *listPtr = Tcl_NewListObj(0, NULL);

    Tcl_IncrRefCount(listenerPtr);
    Tcl_IncrRefCount(listPtr);
    Tcl_ListObjAppendElement(interp, listPtr, listenerPtr);
    Tcl_SetHashValue(entryPtr, (char *) listPtr);

  } else {
    Tcl_Obj *listPtr = (Tcl_Obj *) Tcl_GetHashValue(entryPtr);
    Tcl_Obj *curPtr;
    int idx, len, listenerLen, len2, listlen;
    char *listenerBuf, *buf2;

    if (Tcl_ListObjLength(interp, listPtr, &len) != TCL_OK) {
      Tcl_SetResult(interp, "internal error - bad list", NULL);
      return TCL_ERROR;
    }
    listenerBuf = Tcl_GetStringFromObj(listenerPtr, &listenerLen);

    /*
      sprintf(dbgbuf, "looking for listener \"%s\" in existing list of listeners\n", listenerBuf);
      Tcl_WriteChars(stderrChan, dbgbuf, -1);
     */

    new = 0;
    for (idx = 0; idx < len; idx++) {
      Tcl_ListObjIndex(interp, listPtr, idx, &curPtr);
      buf2 = Tcl_GetStringFromObj(curPtr, &len2);

      /*
	sprintf(dbgbuf, "comparing against list entry \"%s\"\n", buf2);
	Tcl_WriteChars(stderrChan, dbgbuf, -1);
       */

      if (listenerLen == len2 && 
          !strncmp(listenerBuf, buf2, listenerLen)) {
        /* Tcl_WriteChars(stderrChan, "found it\n", -1); */
        new = 1;
        break;
      } /* else {
	  Tcl_WriteChars(stderrChan, "keep looking\n", -1);
	  } */
    }

    if (Tcl_ListObjLength(interp, listPtr, &listlen) != TCL_OK) {
      return TCL_ERROR;
    }

    /*
      sprintf(dbgbuf, "replacing %d entry %d in list of length %d\n", new, idx, listlen);
      Tcl_WriteChars(stderrChan, dbgbuf, -1);
     */

    Tcl_ListObjReplace(interp, listPtr, idx, new, 1, &listenerPtr);

  }

  /*
   * Performance optimization:
   * Keep track of which event types have listeners registered.
   * If there are no listeners for an event type, then there's
   * no point in dispatching that type of event.
   * NB. This does not keep track of user-defined events types.
   */

  if (Tcl_GetIndexFromObj(interp, typePtr, EventTypes,
                          "type", TCL_EXACT, &eventType) == TCL_OK) {
    tcldomdocPtr->listening[eventType]++;
  } /* else this is a user-defined event type - it won't be tracked */

  return TCL_OK;
}
/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_RemoveEventListener --
 *
 *  Deregister an event listener.
 *
 * Results:
 *  Event listener data deleted.
 *
 * Side effects:
 *  May free Tcl objects.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_RemoveEventListener(interp, tcldomdocPtr, tokenPtr, typePtr, listenerPtr, capturer)
    Tcl_Interp *interp;
    TclDOMDocument *tcldomdocPtr;
    void *tokenPtr;
    Tcl_Obj *typePtr;
    Tcl_Obj *listenerPtr;
    int capturer;
{
  Tcl_HashTable *tablePtr;
  Tcl_HashEntry *entryPtr;
  int eventType;

  if (capturer) {
    tablePtr = &captureListeners;
  } else {
    tablePtr = &bubbleListeners;
  }

  entryPtr = Tcl_FindHashEntry(tablePtr, tokenPtr);
  if (entryPtr) {
    tablePtr = (Tcl_HashTable *) Tcl_GetHashValue(entryPtr);

    entryPtr = Tcl_FindHashEntry(tablePtr, Tcl_GetStringFromObj(typePtr, NULL));
    if (entryPtr) {
      Tcl_Obj *listPtr = (Tcl_Obj *) Tcl_GetHashValue(entryPtr);
      Tcl_Obj *curPtr;
      int idx, listenerLen, len, len2;
      char *listenerBuf, *buf2;

      if (Tcl_ListObjLength(interp, listPtr, &len) != TCL_OK) {
        Tcl_SetResult(interp, "internal error - bad list", NULL);
        return TCL_ERROR;
      }
      listenerBuf = Tcl_GetStringFromObj(listenerPtr, &listenerLen);
      for (idx = 0; idx < len; idx++) {
        Tcl_ListObjIndex(interp, listPtr, idx, &curPtr);
        buf2 = Tcl_GetStringFromObj(curPtr, &len2);
        if (listenerLen != len2 || 
            strncmp(listenerBuf, buf2, listenerLen)) {
          continue;
        }
      }

      if (idx == len) {
        Tcl_SetResult(interp, "no listener registered", NULL);
        return TCL_ERROR;
      } else {
        Tcl_ListObjReplace(interp, listPtr, idx, 1, 0, NULL);

        /*
         * Keep track of which event types have listeners registered.
         */

        if (Tcl_GetIndexFromObj(interp, typePtr, EventTypes,
			         "type", TCL_EXACT, &eventType) == TCL_OK) {
	    tcldomdocPtr->listening[eventType]--;
	  } /* else user-defined event type - not being tracked */
	}
      } else {
	Tcl_SetResult(interp, "no listeners registered", NULL);
	return TCL_ERROR;
      }
    } else {
      Tcl_SetResult(interp, "no listeners registered", NULL);
      return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_DispatchEvent --
 *
 *  Dispatch an event object.
 *
 * Results:
 *  Event propagtes through the DOM tree.
 *
 * Side effects:
 *  Depends on event listeners.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_DispatchEvent(interp, nodeObjPtr, eventObjPtr, eventPtr)
    Tcl_Interp *interp;
    Tcl_Obj *nodeObjPtr;
    Tcl_Obj *eventObjPtr;
    TclDOMEvent *eventPtr;
{
  xmlNodePtr nodePtr;
  xmlDocPtr docPtr;
  TclDOMDocument *tcldomdocPtr;
  char *phase;
  Tcl_Obj *docObjPtr, *pathPtr = NULL;
  int eventType, idx, len, cancelable;
  void *tokenPtr;

  /*
    sprintf(dbgbuf, "dispatchEvent node %s event type %s\n", Tcl_GetStringFromObj(nodeObjPtr, NULL), Tcl_GetStringFromObj(eventPtr->type, NULL));
    Tcl_WriteChars(stderrChan, dbgbuf, -1);
  */

  /*
   * Performance optimization:
   * If there are no listeners registered for this event type,
   * then there is no point in propagating the event.
   */

  if (TclDOM_GetNodeFromObj(interp, nodeObjPtr, &nodePtr) != TCL_OK) {
    if (TclDOM_GetDocFromObj(interp, nodeObjPtr, &docPtr) != TCL_OK) {
      Tcl_SetResult(interp, "unrecognised token", NULL);
      return TCL_ERROR;
    } else {
      docObjPtr = nodeObjPtr;
      nodeObjPtr = NULL;
      nodePtr = NULL;
    }
  } else {
    docPtr = nodePtr->doc;
    docObjPtr = TclDOM_CreateObjFromDoc(docPtr);
  }

  if (Tcl_GetIndexFromObj(interp, eventPtr->type, EventTypes,
                          "type", TCL_EXACT, &eventType) == TCL_OK) {
    TclDOM_GetDoc2FromObj(interp, docObjPtr, &tcldomdocPtr);
    if (tcldomdocPtr->listening[eventType] <= 0) {
      eventPtr->dispatched = 1;
      return TCL_OK;
    }
  } /* else this is a user-defined event so continue as normal */

  phase = Tcl_GetStringFromObj(eventPtr->eventPhase, &len);

  if (!len) {
    /*
     * This is the initial dispatch of the event.
     * First trigger any capturing event listeners
     * Starting from the root, proceed downward
     */

    Tcl_SetStringObj(eventPtr->eventPhase, "capturing_phase", -1);
    eventPtr->target = nodeObjPtr;

    if (nodePtr) {
      pathPtr = TclDOMGetPath(interp, nodePtr);
    } else {
      pathPtr = Tcl_NewObj();
    }
    if (TriggerEventListeners(interp, &captureListeners, (void *) docObjPtr, eventObjPtr, eventPtr) != TCL_OK) {
      Tcl_DecrRefCount(pathPtr);
      return TCL_ERROR;
    }

    if (Tcl_GetBooleanFromObj(interp, eventPtr->cancelable, &cancelable) != TCL_OK) {
      Tcl_DecrRefCount(pathPtr);
      return TCL_ERROR;
    }
    if (cancelable && eventPtr->stopPropagation) {
      goto stop_propagation;
    }

    Tcl_ListObjLength(interp, pathPtr, &len);
    Tcl_ListObjReplace(interp, pathPtr, len - 1, 1, 0, NULL);
    Tcl_ListObjReplace(interp, pathPtr, 0, 1, 0, NULL);
    Tcl_ListObjLength(interp, pathPtr, &len);
    for (idx = 0; idx < len; idx++) {
      Tcl_Obj *ancestorObjPtr;
      xmlNodePtr ancestorPtr;

      Tcl_ListObjIndex(interp, pathPtr, idx, &ancestorObjPtr);
      eventPtr->currentNode = ancestorObjPtr;
      Tcl_IncrRefCount(ancestorObjPtr);
      if (TclDOM_GetNodeFromObj(interp, ancestorObjPtr, &ancestorPtr) != TCL_OK) {
        Tcl_SetResult(interp, "cannot find ancestor node \"", NULL);
        Tcl_AppendResult(interp, Tcl_GetStringFromObj(ancestorObjPtr, NULL), "\"", NULL);
        return TCL_ERROR;
      }

      if (TriggerEventListeners(interp, &captureListeners, (void *) ancestorPtr, eventObjPtr, eventPtr) != TCL_OK) {
        return TCL_ERROR;
      }

      /*
       * A listener may stop propagation,
       * but we check here to let all of the
       * listeners at that level complete.
       */

      if (Tcl_GetBooleanFromObj(interp, eventPtr->cancelable, &cancelable) != TCL_OK) {
        return TCL_ERROR;
      }
      if (cancelable && eventPtr->stopPropagation) {
        Tcl_DecrRefCount(ancestorObjPtr);
        goto stop_propagation;
      }

      Tcl_DecrRefCount(ancestorObjPtr);

    }

    /* Prepare for the next phase */

    if (Tcl_IsShared(eventPtr->eventPhase)) {
      Tcl_DecrRefCount(eventPtr->eventPhase);
      eventPtr->eventPhase = Tcl_NewStringObj("at_target", -1);
      Tcl_IncrRefCount(eventPtr->eventPhase);
    } else {
      Tcl_SetStringObj(eventPtr->eventPhase, "at_target", -1);
    }
  }

  if (nodePtr) {
    eventPtr->currentNode = nodeObjPtr;
    tokenPtr = (void *) nodePtr;
  } else {
    eventPtr->currentNode = docObjPtr;
    tokenPtr = (void *) docPtr;
  }

  if (TriggerEventListeners(interp, &bubbleListeners, tokenPtr, eventObjPtr, eventPtr) != TCL_OK) {
    return TCL_ERROR;
  }

  if (Tcl_IsShared(eventPtr->eventPhase)) {
    Tcl_DecrRefCount(eventPtr->eventPhase);
    eventPtr->eventPhase = Tcl_NewStringObj("bubbling_phase", -1);
    Tcl_IncrRefCount(eventPtr->eventPhase);
  } else {
    Tcl_SetStringObj(eventPtr->eventPhase, "bubbling_phase", -1);
  }

  if (Tcl_GetBooleanFromObj(interp, eventPtr->cancelable, &cancelable) != TCL_OK) {
    return TCL_ERROR;
  }
  if (cancelable && eventPtr->stopPropagation) {
    /* Do no more */
  } else if (nodePtr && nodePtr->parent && nodePtr->parent != (xmlNodePtr) nodePtr->doc) {
    return TclDOM_DispatchEvent(interp, TclDOM_CreateObjFromNode(nodePtr->parent), eventObjPtr, eventPtr);
  } else if (nodePtr && nodePtr->parent) {
    return TclDOM_DispatchEvent(interp, TclDOM_CreateObjFromDoc(nodePtr->doc), eventObjPtr, eventPtr);
  }

stop_propagation:
  eventPtr->dispatched = 1;

  if (pathPtr) {
    Tcl_DecrRefCount(pathPtr);
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMElementCommand --
 *
 *  Implements dom::libxml2::element command.
 *
 * Results:
 *  Depends on method.
 *
 * Side effects:
 *  Depends on method.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMElementCommand (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int method, option;
  xmlNodePtr nodePtr;
  char *value;
  xmlAttrPtr attrPtr;
  xmlNsPtr nsPtr;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "method ?args...?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], ElementCommandMethods,
			  "method", 0, &method) != TCL_OK) {
    return TCL_ERROR;
  }

  if (TclDOM_GetNodeFromObj(interp, objv[2], &nodePtr) != TCL_OK) {
    return TCL_ERROR;
  }

  /* Should check that the node is of element type */

  switch ((enum ElementCommandMethods) method) {

  case TCLDOM_ELEMENT_CGET:
    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 1, objv, "cget element option");
      return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], ElementCommandOptions,
			    "option", 0, &option) != TCL_OK) {
      return TCL_ERROR;
    }

    switch ((enum ElementCommandOptions) option) {
    case TCLDOM_ELEMENT_TAGNAME:
      Tcl_SetObjResult(interp, Tcl_NewStringObj(nodePtr->name, -1));
      break;

    case TCLDOM_ELEMENT_EMPTY:
      Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
      break;

    default:
      Tcl_SetResult(interp, "unknown option", NULL);
      return TCL_ERROR;
    }

    break;

  case TCLDOM_ELEMENT_CONFIGURE:
    Tcl_SetResult(interp, "option cannot be changed", NULL);
    return TCL_ERROR;

  case TCLDOM_ELEMENT_GETATTRIBUTE:
    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 1, objv, "getAttribute element attr");
      return TCL_ERROR;
    }

    value = xmlGetProp(nodePtr, Tcl_GetStringFromObj(objv[3], NULL));
    if (value) {
      Tcl_SetObjResult(interp, Tcl_NewStringObj(value, -1));
    }

    break;

  case TCLDOM_ELEMENT_GETATTRIBUTENS:
    if (objc != 5) {
      Tcl_WrongNumArgs(interp, 1, objv, "getAttribute element ns attr");
      return TCL_ERROR;
    }

    value = xmlGetNsProp(nodePtr, Tcl_GetStringFromObj(objv[4], NULL), Tcl_GetStringFromObj(objv[3], NULL));
    if (value) {
      Tcl_SetObjResult(interp, Tcl_NewStringObj(value, -1));
    }

    break;

  case TCLDOM_ELEMENT_SETATTRIBUTE:
    if (objc != 5) {
      Tcl_WrongNumArgs(interp, 1, objv, "getAttribute element attr value");
      return TCL_ERROR;
    }

    value = xmlGetProp(nodePtr, Tcl_GetStringFromObj(objv[3], NULL));
    attrPtr = xmlSetProp(nodePtr, Tcl_GetStringFromObj(objv[3], NULL), Tcl_GetStringFromObj(objv[4], NULL));

    if (!attrPtr) {
      Tcl_SetResult(interp, "unable to set attribute", NULL);
      return TCL_ERROR;
    }

    TclDOM_PostMutationEvent(interp, nodePtr->doc, objv[2], Tcl_NewStringObj("DOMAttrModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, Tcl_NewStringObj(value, -1), objv[4], objv[3], value == NULL? Tcl_NewStringObj("modification", -1) : Tcl_NewStringObj("addition", -1));

    Tcl_SetObjResult(interp, objv[4]);

    break;

  case TCLDOM_ELEMENT_SETATTRIBUTENS:
    if (objc != 6) {
      Tcl_WrongNumArgs(interp, 1, objv, "getAttribute element ns attr value");
      return TCL_ERROR;
    }

    nsPtr = xmlSearchNsByHref(nodePtr->doc, nodePtr, Tcl_GetStringFromObj(objv[3], NULL));
    if (!nsPtr) {
      Tcl_SetResult(interp, "no XML Namespace declaration for namespace", NULL);
      return TCL_ERROR;
    }

    value = xmlGetNsProp(nodePtr, Tcl_GetStringFromObj(objv[4], NULL), Tcl_GetStringFromObj(objv[5], NULL));
    attrPtr = xmlSetNsProp(nodePtr, nsPtr, Tcl_GetStringFromObj(objv[4], NULL), Tcl_GetStringFromObj(objv[5], NULL));

    if (!attrPtr) {
      Tcl_SetResult(interp, "unable to set attribute", NULL);
      return TCL_ERROR;
    }

    TclDOM_PostMutationEvent(interp, nodePtr->doc, objv[2], Tcl_NewStringObj("DOMAttrModified", -1), Tcl_NewIntObj(1), Tcl_NewIntObj(0), NULL, Tcl_NewStringObj(value, -1), objv[5], objv[4], value == NULL? Tcl_NewStringObj("modification", -1) : Tcl_NewStringObj("addition", -1));

    break;

  case TCLDOM_ELEMENT_REMOVEATTRIBUTE:

    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 1, objv, "removeAttribute element attr");
      return TCL_ERROR;
    }

    Tcl_ResetResult(interp);

    /* It doesn't matter if this fails due to a non-existant attribute */
    xmlUnsetProp(nodePtr, Tcl_GetStringFromObj(objv[3], NULL));

    break;

  default:
    Tcl_SetResult(interp, "method \"", NULL);
    Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[1], NULL), "\" not yet implemented", NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMInitEvent --
 *
 *  Initializes an event object.
 *
 * Results:
 *  Tcl_Obj references stored.
 *
 * Side effects:
 *  Tcl_Obj's reference count changed.
 *
 *----------------------------------------------------------------------------
 */

void
TclDOMInitEvent(eventPtr, typePtr, bubblesPtr, cancelablePtr)
    TclDOMEvent *eventPtr;
    Tcl_Obj *typePtr;
    Tcl_Obj *bubblesPtr;
    Tcl_Obj *cancelablePtr;
{
    if (eventPtr->type != typePtr) {
      Tcl_DecrRefCount(eventPtr->type);
      eventPtr->type = typePtr;
      Tcl_IncrRefCount(eventPtr->type);
    }

    if (bubblesPtr && eventPtr->bubbles != bubblesPtr) {
      Tcl_DecrRefCount(eventPtr->bubbles);
      eventPtr->bubbles = bubblesPtr;
      Tcl_IncrRefCount(eventPtr->bubbles);
    }
    if (cancelablePtr && eventPtr->cancelable != cancelablePtr) {
      Tcl_DecrRefCount(eventPtr->cancelable);
      eventPtr->cancelable = cancelablePtr;
      Tcl_IncrRefCount(eventPtr->cancelable);
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * TclDOMInitUIEvent --
 *
 *  Initializes an event object.
 *
 * Results:
 *  Tcl_Obj references stored.
 *
 * Side effects:
 *  Tcl_Obj's reference count changed.
 *
 *----------------------------------------------------------------------------
 */

void
TclDOMInitUIEvent(eventPtr, typePtr, bubblesPtr, cancelablePtr, viewPtr, detailPtr)
    TclDOMEvent *eventPtr;
    Tcl_Obj *typePtr;
    Tcl_Obj *bubblesPtr;
    Tcl_Obj *cancelablePtr;
    Tcl_Obj *viewPtr;
    Tcl_Obj *detailPtr;
{
    TclDOMInitEvent(eventPtr, typePtr, bubblesPtr, cancelablePtr);

    if (viewPtr && eventPtr->view != viewPtr) {
      Tcl_DecrRefCount(eventPtr->view);
      eventPtr->view = viewPtr;
      Tcl_IncrRefCount(eventPtr->view);
    }
    if (detailPtr && eventPtr->detail != detailPtr) {
      Tcl_DecrRefCount(eventPtr->detail);
      eventPtr->detail = detailPtr;
      Tcl_IncrRefCount(eventPtr->detail);
    } else if (detailPtr == NULL) {
      Tcl_DecrRefCount(eventPtr->detail);
      eventPtr->detail = Tcl_NewObj();
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * TclDOMInitMouseEvent --
 *
 *  Initializes an event object.
 *
 * Results:
 *  Tcl_Obj references stored.
 *
 * Side effects:
 *  Tcl_Obj's reference count changed.
 *
 *----------------------------------------------------------------------------
 */

void
TclDOMInitMouseEvent(eventPtr, typePtr, bubblesPtr, cancelablePtr, viewPtr, detailPtr, screenXPtr, screenYPtr, clientXPtr, clientYPtr, ctrlKeyPtr, altKeyPtr, shiftKeyPtr, metaKeyPtr, relatedNodePtr)
    TclDOMEvent *eventPtr;
    Tcl_Obj *typePtr;
    Tcl_Obj *bubblesPtr;
    Tcl_Obj *cancelablePtr;
    Tcl_Obj *viewPtr;
    Tcl_Obj *detailPtr;
    Tcl_Obj *screenXPtr;
    Tcl_Obj *screenYPtr;
    Tcl_Obj *clientXPtr;
    Tcl_Obj *clientYPtr;
    Tcl_Obj *ctrlKeyPtr;
    Tcl_Obj *altKeyPtr;
    Tcl_Obj *shiftKeyPtr;
    Tcl_Obj *metaKeyPtr;
    Tcl_Obj *relatedNodePtr;
{
    TclDOMInitUIEvent(eventPtr, typePtr, bubblesPtr, cancelablePtr, viewPtr, detailPtr);

    if (screenXPtr && eventPtr->screenX != screenXPtr) {
      Tcl_DecrRefCount(eventPtr->screenX);
      eventPtr->screenX = screenXPtr;
      Tcl_IncrRefCount(eventPtr->screenX);
    }
    if (screenYPtr && eventPtr->screenY != screenYPtr) {
      Tcl_DecrRefCount(eventPtr->screenY);
      eventPtr->screenY = screenYPtr;
      Tcl_IncrRefCount(eventPtr->screenY);
    }

    if (clientXPtr && eventPtr->clientX != clientXPtr) {
      Tcl_DecrRefCount(eventPtr->clientX);
      eventPtr->clientX = clientXPtr;
      Tcl_IncrRefCount(eventPtr->clientX);
    }
    if (clientYPtr && eventPtr->clientY != clientYPtr) {
      Tcl_DecrRefCount(eventPtr->clientY);
      eventPtr->clientY = clientYPtr;
      Tcl_IncrRefCount(eventPtr->clientY);
    }

    if (ctrlKeyPtr && eventPtr->ctrlKey != ctrlKeyPtr) {
      Tcl_DecrRefCount(eventPtr->ctrlKey);
      eventPtr->ctrlKey = ctrlKeyPtr;
      Tcl_IncrRefCount(eventPtr->ctrlKey);
    }
    if (ctrlKeyPtr && eventPtr->altKey != altKeyPtr) {
      Tcl_DecrRefCount(eventPtr->altKey);
      eventPtr->altKey = altKeyPtr;
      Tcl_IncrRefCount(eventPtr->altKey);
    }
    if (ctrlKeyPtr && eventPtr->shiftKey != shiftKeyPtr) {
      Tcl_DecrRefCount(eventPtr->shiftKey);
      eventPtr->shiftKey = shiftKeyPtr;
      Tcl_IncrRefCount(eventPtr->shiftKey);
    }
    if (ctrlKeyPtr && eventPtr->metaKey != metaKeyPtr) {
      Tcl_DecrRefCount(eventPtr->metaKey);
      eventPtr->metaKey = metaKeyPtr;
      Tcl_IncrRefCount(eventPtr->metaKey);
    }

    if (relatedNodePtr && eventPtr->relatedNode != relatedNodePtr) {
      Tcl_DecrRefCount(eventPtr->relatedNode);
      eventPtr->relatedNode = relatedNodePtr;
      Tcl_IncrRefCount(eventPtr->relatedNode);
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * TclDOMInitMutationEvent --
 *
 *  Initializes an event object.
 *
 * Results:
 *  Tcl_Obj references stored.
 *
 * Side effects:
 *  Tcl_Obj's reference count changed.
 *
 *----------------------------------------------------------------------------
 */

void
TclDOMInitMutationEvent(eventPtr, typePtr, bubblesPtr, cancelablePtr, relatedNodePtr, prevValuePtr, newValuePtr, attrNamePtr, attrChangePtr)
    TclDOMEvent *eventPtr;
    Tcl_Obj *typePtr;
    Tcl_Obj *bubblesPtr;
    Tcl_Obj *cancelablePtr;
    Tcl_Obj *relatedNodePtr;
    Tcl_Obj *prevValuePtr;
    Tcl_Obj *newValuePtr;
    Tcl_Obj *attrNamePtr;
    Tcl_Obj *attrChangePtr;
{
    TclDOMInitEvent(eventPtr, typePtr, bubblesPtr, cancelablePtr);

    if (relatedNodePtr && eventPtr->relatedNode != relatedNodePtr) {
      Tcl_DecrRefCount(eventPtr->relatedNode);
      eventPtr->relatedNode = relatedNodePtr;
      Tcl_IncrRefCount(eventPtr->relatedNode);
    }

    if (prevValuePtr && eventPtr->prevValue != prevValuePtr) {
      Tcl_DecrRefCount(eventPtr->prevValue);
      eventPtr->prevValue = prevValuePtr;
      Tcl_IncrRefCount(eventPtr->prevValue);
    }
    if (newValuePtr && eventPtr->newValue != newValuePtr) {
      Tcl_DecrRefCount(eventPtr->newValue);
      eventPtr->newValue = newValuePtr;
      Tcl_IncrRefCount(eventPtr->newValue);
    }
    if (attrNamePtr && eventPtr->attrName != attrNamePtr) {
      Tcl_DecrRefCount(eventPtr->attrName);
      eventPtr->attrName = attrNamePtr;
      Tcl_IncrRefCount(eventPtr->attrName);
    }
    if (attrChangePtr && eventPtr->attrChange != attrChangePtr) {
      Tcl_DecrRefCount(eventPtr->attrChange);
      eventPtr->attrChange = attrChangePtr;
      Tcl_IncrRefCount(eventPtr->attrChange);
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_PostUIEvent --
 *
 *  Post an event and cleanup afterward.
 *
 * Results:
 *  Event created and propagated.
 *
 * Side effects:
 *  Depends on event listeners.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_PostUIEvent(interp, docPtr, nodeObjPtr, typePtr, bubblesPtr, cancelablePtr, viewPtr, detailPtr)
     Tcl_Interp *interp;
     xmlDocPtr docPtr;
     Tcl_Obj *nodeObjPtr;
     Tcl_Obj *typePtr;
     Tcl_Obj *bubblesPtr;
     Tcl_Obj *cancelablePtr;
     Tcl_Obj *viewPtr;
     Tcl_Obj *detailPtr;
{
    Tcl_Obj *eventObj = TclDOMNewEvent(interp, docPtr, typePtr);
    TclDOMEvent *eventPtr = (TclDOMEvent *) eventObj->internalRep.otherValuePtr;
    int result;

    TclDOMInitUIEvent(eventPtr, typePtr, bubblesPtr, cancelablePtr, viewPtr, detailPtr);

    Tcl_ResetResult(interp);
    result = TclDOM_DispatchEvent(interp, nodeObjPtr, eventObj, eventPtr);

    TclDOMDestroyEvent(eventPtr, eventObj);

    return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_PostMouseEvent --
 *
 *  Post an event and cleanup afterward.
 *
 * Results:
 *  Event created and propagated.
 *
 * Side effects:
 *  Depends on event listeners.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_PostMouseEvent(interp, docPtr, nodeObjPtr, typePtr, bubblesPtr, cancelablePtr, relatedNodePtr, viewPtr, detailPtr, screenXPtr, screenYPtr, clientXPtr, clientYPtr, ctrlKeyPtr, shiftKeyPtr, metaKeyPtr, buttonPtr)
     Tcl_Interp *interp;
     xmlDocPtr docPtr;
     Tcl_Obj *nodeObjPtr;
     Tcl_Obj *typePtr;
     Tcl_Obj *bubblesPtr;
     Tcl_Obj *cancelablePtr;
     Tcl_Obj *relatedNodePtr;
     Tcl_Obj *viewPtr;
     Tcl_Obj *detailPtr;
     Tcl_Obj *screenXPtr;
     Tcl_Obj *screenYPtr;
     Tcl_Obj *clientXPtr;
     Tcl_Obj *clientYPtr;
     Tcl_Obj *ctrlKeyPtr;
     Tcl_Obj *shiftKeyPtr;
     Tcl_Obj *metaKeyPtr;
     Tcl_Obj *buttonPtr;
{
    Tcl_Obj *eventObj = TclDOMNewEvent(interp, docPtr, typePtr);
    TclDOMEvent *eventPtr = (TclDOMEvent *) eventObj->internalRep.otherValuePtr;
    int result;

    TclDOMInitMouseEvent(eventPtr, typePtr, bubblesPtr, cancelablePtr, relatedNodePtr, viewPtr, detailPtr, screenXPtr, screenYPtr, clientXPtr, clientYPtr, ctrlKeyPtr, shiftKeyPtr, metaKeyPtr, buttonPtr);

    Tcl_ResetResult(interp);
    result = TclDOM_DispatchEvent(interp, nodeObjPtr, eventObj, eventPtr);

    TclDOMDestroyEvent(eventPtr, eventObj);

    return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_PostMutationEvent --
 *
 *  Post an event and cleanup afterward.
 *
 * Results:
 *  Event created and propagated.
 *
 * Side effects:
 *  Depends on event listeners.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_PostMutationEvent(interp, docPtr, nodeObjPtr, typePtr, bubblesPtr, cancelablePtr, relatedNodePtr, prevValuePtr, newValuePtr, attrNamePtr, attrChangePtr)
     Tcl_Interp *interp;
     xmlDocPtr docPtr;
     Tcl_Obj *nodeObjPtr;
     Tcl_Obj *typePtr;
     Tcl_Obj *bubblesPtr;
     Tcl_Obj *cancelablePtr;
     Tcl_Obj *relatedNodePtr;
     Tcl_Obj *prevValuePtr;
     Tcl_Obj *newValuePtr;
     Tcl_Obj *attrNamePtr;
     Tcl_Obj *attrChangePtr;
{
    Tcl_Obj *eventObj = TclDOMNewEvent(interp, docPtr, typePtr);
    TclDOMEvent *eventPtr;
    int result;

    if (eventObj == NULL) {
      Tcl_SetResult(interp, "unable to create event", NULL);
      return TCL_ERROR;
    }

    eventPtr = (TclDOMEvent *) eventObj->internalRep.otherValuePtr;
    TclDOMInitMutationEvent(eventPtr, typePtr, bubblesPtr, cancelablePtr, relatedNodePtr, prevValuePtr, newValuePtr, attrNamePtr, attrChangePtr);

    Tcl_ResetResult(interp);
    result = TclDOM_DispatchEvent(interp, nodeObjPtr, eventObj, eventPtr);

    TclDOMDestroyEvent(eventPtr, eventObj);

    return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * TclDOMEventCommand --
 *
 *  Implements dom::libxml2::event command.
 *
 * Results:
 *  Depends on method.
 *
 * Side effects:
 *  Depends on method.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOMEventCommand (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int method, option;
  TclDOMEvent *eventPtr;
  xmlDocPtr docPtr;
  xmlNodePtr nodePtr;
  Tcl_Obj *eventObj, *nodeObj;
  Tcl_Obj *bubblesPtr, *cancelablePtr, *viewPtr, *detailPtr;
  Tcl_Obj *relatedNodePtr, *screenXPtr, *screenYPtr, *clientXPtr, *clientYPtr;
  Tcl_Obj *ctrlKeyPtr, *shiftKeyPtr, *metaKeyPtr, *buttonPtr;
  Tcl_Obj *prevValuePtr, *newValuePtr, *attrNamePtr, *attrChangePtr;
  Tcl_Obj **argPtr;

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "method ?args...?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], EventCommandMethods,
			  "method", 0, &method) != TCL_OK) {
    return TCL_ERROR;
  }

  switch ((enum EventCommandMethods) method) {

  case TCLDOM_EVENT_CGET:
    if (objc != 4) {
      Tcl_WrongNumArgs(interp, 3, objv, "cget event option");
      return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[3], EventCommandOptions,
			    "option", 0, &option) != TCL_OK) {
      return TCL_ERROR;
    }

    if (clientData) {
      eventObj = (Tcl_Obj *) clientData;
      eventPtr = (TclDOMEvent *) eventObj->internalRep.otherValuePtr;
    } else {
      eventObj = objv[2];
      if (TclDOM_GetEventFromObj(interp, objv[2], &eventPtr) != TCL_OK) {
	return TCL_ERROR;
      }
    }

    switch ((enum EventCommandOptions) option) {
    case TCLDOM_EVENT_ALTKEY:
      Tcl_SetObjResult(interp, eventPtr->altKey);
      break;
    case TCLDOM_EVENT_ATTRNAME:
      Tcl_SetObjResult(interp, eventPtr->attrName);
      break;
    case TCLDOM_EVENT_ATTRCHANGE:
      Tcl_SetObjResult(interp, eventPtr->attrChange);
      break;
    case TCLDOM_EVENT_BUBBLES:
      Tcl_SetObjResult(interp, eventPtr->bubbles);
      break;
    case TCLDOM_EVENT_BUTTON:
      Tcl_SetObjResult(interp, eventPtr->button);
      break;
    case TCLDOM_EVENT_CANCELABLE:
      Tcl_SetObjResult(interp, eventPtr->cancelable);
      break;
    case TCLDOM_EVENT_CLIENTX:
      Tcl_SetObjResult(interp, eventPtr->clientX);
      break;
    case TCLDOM_EVENT_CLIENTY:
      Tcl_SetObjResult(interp, eventPtr->clientY);
      break;
    case TCLDOM_EVENT_CTRLKEY:
      Tcl_SetObjResult(interp, eventPtr->ctrlKey);
      break;
    case TCLDOM_EVENT_CURRENTNODE:
      Tcl_SetObjResult(interp, eventPtr->currentNode);
      break;
    case TCLDOM_EVENT_DETAIL:
      Tcl_SetObjResult(interp, eventPtr->detail);
      break;
    case TCLDOM_EVENT_EVENTPHASE:
      Tcl_SetObjResult(interp, eventPtr->eventPhase);
      break;
    case TCLDOM_EVENT_METAKEY:
      Tcl_SetObjResult(interp, eventPtr->metaKey);
      break;
    case TCLDOM_EVENT_NEWVALUE:
      Tcl_SetObjResult(interp, eventPtr->newValue);
      break;
    case TCLDOM_EVENT_PREVVALUE:
      Tcl_SetObjResult(interp, eventPtr->prevValue);
      break;
    case TCLDOM_EVENT_RELATEDNODE:
      Tcl_SetObjResult(interp, eventPtr->relatedNode);
      break;
    case TCLDOM_EVENT_SCREENX:
      Tcl_SetObjResult(interp, eventPtr->screenX);
      break;
    case TCLDOM_EVENT_SCREENY:
      Tcl_SetObjResult(interp, eventPtr->screenY);
      break;
    case TCLDOM_EVENT_SHIFTKEY:
      Tcl_SetObjResult(interp, eventPtr->shiftKey);
      break;
    case TCLDOM_EVENT_TARGET:
      Tcl_SetObjResult(interp, eventPtr->target);
      break;
    case TCLDOM_EVENT_TIMESTAMP:
      Tcl_SetObjResult(interp, eventPtr->timeStamp);
      break;
    case TCLDOM_EVENT_TYPE:
      Tcl_SetObjResult(interp, eventPtr->type);
      break;
    case TCLDOM_EVENT_VIEW:
      Tcl_SetObjResult(interp, eventPtr->view);
      break;
    default:
      Tcl_SetResult(interp, "unknown option", NULL);
      return TCL_ERROR;
    }

    break;

  case TCLDOM_EVENT_CONFIGURE:
    if (objc < 2) {
      Tcl_WrongNumArgs(interp, 3, objv, "configure option ?value?");
      return TCL_ERROR;
    }

    /* No event options are writable */
    Tcl_SetResult(interp, "option cannot be modified", NULL);
    return TCL_ERROR;

    break;

  case TCLDOM_EVENT_STOPPROPAGATION:

    if (clientData) {
      eventObj = (Tcl_Obj *) clientData;
      eventPtr = (TclDOMEvent *) eventObj->internalRep.otherValuePtr;
    } else {
      eventObj = objv[2];
      if (TclDOM_GetEventFromObj(interp, objv[2], &eventPtr) != TCL_OK) {
	return TCL_ERROR;
      }
    }

    if (objc != 0) {
      Tcl_WrongNumArgs(interp, 1, objv, "");
      return TCL_ERROR;
    }

    eventPtr->stopPropagation = 1;

    break;

  case TCLDOM_EVENT_PREVENTDEFAULT:

    if (clientData) {
      eventObj = (Tcl_Obj *) clientData;
      eventPtr = (TclDOMEvent *) eventObj->internalRep.otherValuePtr;
    } else {
      eventObj = objv[2];
      if (TclDOM_GetEventFromObj(interp, objv[2], &eventPtr) != TCL_OK) {
	return TCL_ERROR;
      }
    }

    if (objc != 0) {
      Tcl_WrongNumArgs(interp, 2, objv, "");
      return TCL_ERROR;
    }

    eventPtr->preventDefault = 1;

    break;

  case TCLDOM_EVENT_INITEVENT:

    if (objc != 3) {
      Tcl_WrongNumArgs(interp, 2, objv, "type bubbles cancelable");
      return TCL_ERROR;
    }

    if (clientData) {
      eventObj = (Tcl_Obj *) clientData;
      eventPtr = (TclDOMEvent *) eventObj->internalRep.otherValuePtr;
      argPtr = (Tcl_Obj **) &objv[2];
      objc -= 2;
    } else {
      eventObj = objv[2];
      if (TclDOM_GetEventFromObj(interp, objv[2], &eventPtr) != TCL_OK) {
	return TCL_ERROR;
      }
      argPtr = (Tcl_Obj **) &objv[3];
      objc -= 3;
    }

    if (eventPtr->dispatched) {
      Tcl_SetResult(interp, "event has been dispatched", NULL);
      return TCL_ERROR;
    }

    TclDOMInitEvent(eventPtr, argPtr[0], argPtr[1], argPtr[2]);

    break;

  case TCLDOM_EVENT_INITUIEVENT:

    if (objc < 4 || objc > 5) {
      Tcl_WrongNumArgs(interp, 2, objv, "type bubbles cancelable view ?detail?");
      return TCL_ERROR;
    }

    if (clientData) {
      eventObj = (Tcl_Obj *) clientData;
      eventPtr = (TclDOMEvent *) eventObj->internalRep.otherValuePtr;
      argPtr = (Tcl_Obj **) &objv[2];
      objc -= 2;
    } else {
      eventObj = objv[2];
      if (TclDOM_GetEventFromObj(interp, objv[2], &eventPtr) != TCL_OK) {
	return TCL_ERROR;
      }
      argPtr = (Tcl_Obj **) &objv[3];
      objc -= 3;
    }

    if (eventPtr->dispatched) {
      Tcl_SetResult(interp, "event has been dispatched", NULL);
      return TCL_ERROR;
    }

    TclDOMInitUIEvent(eventPtr, argPtr[0], argPtr[1], argPtr[2], argPtr[3], objc == 5 ? argPtr[4] : NULL);

    break;

  case TCLDOM_EVENT_INITMOUSEEVENT:

    if (objc != 18) {
      Tcl_WrongNumArgs(interp, 2, objv, "type bubbles cancelable view detail screenX screenY clientX clientY ctrlKey altKey shiftKey metaKey button relatedNode");
      return TCL_ERROR;
    }

    if (clientData) {
      eventObj = (Tcl_Obj *) clientData;
      eventPtr = (TclDOMEvent *) eventObj->internalRep.otherValuePtr;
      argPtr = (Tcl_Obj **) &objv[2];
      objc -= 2;
    } else {
      eventObj = objv[2];
      if (TclDOM_GetEventFromObj(interp, objv[2], &eventPtr) != TCL_OK) {
	return TCL_ERROR;
      }
      argPtr = (Tcl_Obj **) &objv[3];
      objc -= 3;
    }

    if (eventPtr->dispatched) {
      Tcl_SetResult(interp, "event has been dispatched", NULL);
      return TCL_ERROR;
    }

    TclDOMInitMouseEvent(eventPtr, argPtr[0], argPtr[1], argPtr[2], argPtr[3], argPtr[4], argPtr[5], argPtr[6], argPtr[7], argPtr[8], argPtr[9], argPtr[10], argPtr[11], argPtr[12], argPtr[13]);

    break;

  case TCLDOM_EVENT_INITMUTATIONEVENT:

    if (objc != 10) {
      Tcl_WrongNumArgs(interp, 2, objv, "type bubbles cancelable relatedNode prevValue newValue attrName attrChange");
      return TCL_ERROR;
    }

    if (clientData) {
      eventObj = (Tcl_Obj *) clientData;
      eventPtr = (TclDOMEvent *) eventObj->internalRep.otherValuePtr;
      argPtr = (Tcl_Obj **) &objv[2];
      objc -= 2;
    } else {
      eventObj = objv[2];
      if (TclDOM_GetEventFromObj(interp, objv[2], &eventPtr) != TCL_OK) {
	return TCL_ERROR;
      }
      argPtr = (Tcl_Obj **) &objv[3];
      objc -= 3;
    }

    if (eventPtr->dispatched) {
      Tcl_SetResult(interp, "event has been dispatched", NULL);
      return TCL_ERROR;
    }

    TclDOMInitMutationEvent(eventPtr, argPtr[0], argPtr[1], argPtr[2], argPtr[3], argPtr[4], argPtr[5], argPtr[6], argPtr[7]);

    break;

  case TCLDOM_EVENT_POSTUIEVENT:

    if (objc < 4) {
      Tcl_WrongNumArgs(interp, 1, objv, "postUIEvent node type ?args ...?");
      return TCL_ERROR;
    }

    if (TclDOM_GetNodeFromObj(interp, objv[2], &nodePtr) != TCL_OK) {
      return TCL_ERROR;
    }

    bubblesPtr = Tcl_GetVar2Ex(interp, "::dom::bubbles", Tcl_GetStringFromObj(objv[3], NULL), TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG);
    if (!bubblesPtr) {
      return TCL_ERROR;
    }
    Tcl_IncrRefCount(bubblesPtr);
    cancelablePtr = Tcl_GetVar2Ex(interp, "::dom::cancelable", Tcl_GetStringFromObj(objv[3], NULL), TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG);
    if (!cancelablePtr) {
      Tcl_DecrRefCount(bubblesPtr);
      return TCL_ERROR;
    }
    Tcl_IncrRefCount(cancelablePtr);

    viewPtr = Tcl_NewObj();
    detailPtr = Tcl_NewObj();

    objc -= 4;
    objv += 4;
    while (objc) {

      if (objc == 1) {
	Tcl_SetResult(interp, "value missing", NULL);
        Tcl_DecrRefCount(bubblesPtr);
        Tcl_DecrRefCount(cancelablePtr);
        Tcl_DecrRefCount(viewPtr);
        Tcl_DecrRefCount(detailPtr);
	return TCL_ERROR;
      }

      if (Tcl_GetIndexFromObj(interp, objv[0], EventCommandOptions,
			      "option", 0, &option) != TCL_OK) {
        Tcl_DecrRefCount(bubblesPtr);
        Tcl_DecrRefCount(cancelablePtr);
        Tcl_DecrRefCount(viewPtr);
        Tcl_DecrRefCount(detailPtr);
	return TCL_ERROR;
      }
      switch ((enum EventCommandOptions) option) {
      case TCLDOM_EVENT_BUBBLES:
	Tcl_DecrRefCount(bubblesPtr);
	bubblesPtr = objv[1];
	Tcl_IncrRefCount(bubblesPtr);
	break;
      case TCLDOM_EVENT_CANCELABLE:
	Tcl_DecrRefCount(cancelablePtr);
	cancelablePtr = objv[1];
	Tcl_IncrRefCount(cancelablePtr);
	break;
      case TCLDOM_EVENT_VIEW:
	Tcl_DecrRefCount(viewPtr);
	viewPtr = objv[1];
	Tcl_IncrRefCount(viewPtr);
	break;
      case TCLDOM_EVENT_DETAIL:
	Tcl_DecrRefCount(detailPtr);
	detailPtr = objv[1];
	Tcl_IncrRefCount(detailPtr);
	break;
      default:
	Tcl_SetResult(interp, "bad option", NULL);
        Tcl_DecrRefCount(bubblesPtr);
        Tcl_DecrRefCount(cancelablePtr);
        Tcl_DecrRefCount(viewPtr);
        Tcl_DecrRefCount(detailPtr);
	return TCL_ERROR;
      }

      objc -= 2;
      objv += 2;
    }

    if (TclDOM_PostUIEvent(interp, nodePtr->doc, objv[2], objv[3], bubblesPtr, cancelablePtr, viewPtr, detailPtr) != TCL_OK) {
      Tcl_DecrRefCount(bubblesPtr);
      Tcl_DecrRefCount(cancelablePtr);
      Tcl_DecrRefCount(viewPtr);
      Tcl_DecrRefCount(detailPtr);
      return TCL_ERROR;
    }

    break;

  case TCLDOM_EVENT_POSTMOUSEEVENT:

    if (objc < 4) {
      Tcl_WrongNumArgs(interp, 1, objv, "postMouseEvent node type ?args ...?");
      return TCL_ERROR;
    }

    if (TclDOM_GetNodeFromObj(interp, objv[2], &nodePtr) != TCL_OK) {
      return TCL_ERROR;
    }
    nodeObj = objv[2];
    docPtr = nodePtr->doc;

    bubblesPtr = Tcl_GetVar2Ex(interp, "::dom::bubbles", Tcl_GetStringFromObj(objv[3], NULL), TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG);
    if (!bubblesPtr) {
      return TCL_ERROR;
    }
    Tcl_IncrRefCount(bubblesPtr);
    cancelablePtr = Tcl_GetVar2Ex(interp, "::dom::cancelable", Tcl_GetStringFromObj(objv[3], NULL), TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG);
    if (!cancelablePtr) {
      Tcl_DecrRefCount(bubblesPtr);
      return TCL_ERROR;
    }
    Tcl_IncrRefCount(cancelablePtr);

    viewPtr = Tcl_NewObj();
    detailPtr = Tcl_NewObj();
    relatedNodePtr = Tcl_NewObj();
    screenXPtr = Tcl_NewObj();
    screenYPtr = Tcl_NewObj();
    clientXPtr = Tcl_NewObj();
    clientYPtr = Tcl_NewObj();
    ctrlKeyPtr = Tcl_NewObj();
    shiftKeyPtr = Tcl_NewObj();
    metaKeyPtr = Tcl_NewObj();
    buttonPtr = Tcl_NewObj();

    objc -= 4;
    objv += 4;
    while (objc) {

      if (objc == 1) {
	Tcl_SetResult(interp, "value missing", NULL);
        goto mouse_error;
      }

      if (Tcl_GetIndexFromObj(interp, objv[0], EventCommandOptions,
			      "option", 0, &option) != TCL_OK) {
        goto mouse_error;
      }
      switch ((enum EventCommandOptions) option) {
      case TCLDOM_EVENT_BUBBLES:
	Tcl_DecrRefCount(bubblesPtr);
	bubblesPtr = objv[1];
	Tcl_IncrRefCount(bubblesPtr);
	break;
      case TCLDOM_EVENT_CANCELABLE:
	Tcl_DecrRefCount(cancelablePtr);
	cancelablePtr = objv[1];
	Tcl_IncrRefCount(cancelablePtr);
	break;
      case TCLDOM_EVENT_RELATEDNODE:
	Tcl_DecrRefCount(relatedNodePtr);
	relatedNodePtr = objv[1];
	Tcl_IncrRefCount(relatedNodePtr);
	break;
      case TCLDOM_EVENT_VIEW:
	Tcl_DecrRefCount(viewPtr);
	viewPtr = objv[1];
	Tcl_IncrRefCount(viewPtr);
	break;
      case TCLDOM_EVENT_DETAIL:
	Tcl_DecrRefCount(detailPtr);
	detailPtr = objv[1];
	Tcl_IncrRefCount(detailPtr);
	break;
      case TCLDOM_EVENT_SCREENX:
	Tcl_DecrRefCount(screenXPtr);
	screenXPtr = objv[1];
	Tcl_IncrRefCount(screenXPtr);
	break;
      case TCLDOM_EVENT_SCREENY:
	Tcl_DecrRefCount(screenYPtr);
	screenYPtr = objv[1];
	Tcl_IncrRefCount(screenYPtr);
	break;
      case TCLDOM_EVENT_CLIENTX:
	Tcl_DecrRefCount(clientXPtr);
	clientXPtr = objv[1];
	Tcl_IncrRefCount(clientXPtr);
	break;
      case TCLDOM_EVENT_CLIENTY:
	Tcl_DecrRefCount(clientYPtr);
	clientYPtr = objv[1];
	Tcl_IncrRefCount(clientYPtr);
	break;
      case TCLDOM_EVENT_CTRLKEY:
	Tcl_DecrRefCount(ctrlKeyPtr);
	ctrlKeyPtr = objv[1];
	Tcl_IncrRefCount(ctrlKeyPtr);
	break;
      case TCLDOM_EVENT_SHIFTKEY:
	Tcl_DecrRefCount(shiftKeyPtr);
	shiftKeyPtr = objv[1];
	Tcl_IncrRefCount(shiftKeyPtr);
	break;
      case TCLDOM_EVENT_METAKEY:
	Tcl_DecrRefCount(metaKeyPtr);
	metaKeyPtr = objv[1];
	Tcl_IncrRefCount(metaKeyPtr);
	break;
      case TCLDOM_EVENT_BUTTON:
	Tcl_DecrRefCount(buttonPtr);
	buttonPtr = objv[1];
	Tcl_IncrRefCount(buttonPtr);
	break;
      default:
	Tcl_SetResult(interp, "bad option", NULL);
	goto mouse_error;
      }

      objc -= 2;
      objv += 2;
    }

    if (TclDOM_PostMouseEvent(interp, nodePtr->doc, objv[2], objv[3], bubblesPtr, cancelablePtr, relatedNodePtr, viewPtr, detailPtr, screenXPtr, screenYPtr, clientXPtr, clientYPtr, ctrlKeyPtr, shiftKeyPtr, metaKeyPtr, buttonPtr) != TCL_OK) {
      goto mouse_error;
    }

    break;

mouse_error:
    Tcl_DecrRefCount(bubblesPtr);
    Tcl_DecrRefCount(cancelablePtr);
    Tcl_DecrRefCount(viewPtr);
    Tcl_DecrRefCount(detailPtr);
    Tcl_DecrRefCount(relatedNodePtr);
    Tcl_DecrRefCount(screenXPtr);
    Tcl_DecrRefCount(screenYPtr);
    Tcl_DecrRefCount(clientXPtr);
    Tcl_DecrRefCount(clientYPtr);
    Tcl_DecrRefCount(ctrlKeyPtr);
    Tcl_DecrRefCount(shiftKeyPtr);
    Tcl_DecrRefCount(metaKeyPtr);
    Tcl_DecrRefCount(buttonPtr);

    return TCL_ERROR;

  case TCLDOM_EVENT_POSTMUTATIONEVENT:

    if (objc < 4) {
      Tcl_WrongNumArgs(interp, 1, objv, "postMutationEvent node type ?args ...?");
      return TCL_ERROR;
    }

    if (TclDOM_GetNodeFromObj(interp, objv[2], &nodePtr) != TCL_OK) {
      return TCL_ERROR;
    }
    nodeObj = objv[2];
    docPtr = nodePtr->doc;

    bubblesPtr = Tcl_GetVar2Ex(interp, "::dom::bubbles", Tcl_GetStringFromObj(objv[3], NULL), TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG);
    if (!bubblesPtr) {
      return TCL_ERROR;
    }
    Tcl_IncrRefCount(bubblesPtr);
    cancelablePtr = Tcl_GetVar2Ex(interp, "::dom::cancelable", Tcl_GetStringFromObj(objv[3], NULL), TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG);
    if (!cancelablePtr) {
      Tcl_DecrRefCount(bubblesPtr);
      return TCL_ERROR;
    }
    Tcl_IncrRefCount(cancelablePtr);

    relatedNodePtr = Tcl_NewObj();
    prevValuePtr = Tcl_NewObj();
    newValuePtr = Tcl_NewObj();
    attrNamePtr = Tcl_NewObj();
    attrChangePtr = Tcl_NewObj();

    objc -= 4;
    objv += 4;
    while (objc) {

      if (objc == 1) {
	Tcl_SetResult(interp, "value missing", NULL);
        goto mutation_error;
      }

      if (Tcl_GetIndexFromObj(interp, objv[0], EventCommandOptions,
			      "option", 0, &option) != TCL_OK) {
        goto mutation_error;
      }
      switch ((enum EventCommandOptions) option) {
      case TCLDOM_EVENT_BUBBLES:
	Tcl_DecrRefCount(bubblesPtr);
	bubblesPtr = objv[1];
	Tcl_IncrRefCount(bubblesPtr);
	break;
      case TCLDOM_EVENT_CANCELABLE:
	Tcl_DecrRefCount(cancelablePtr);
	cancelablePtr = objv[1];
	Tcl_IncrRefCount(cancelablePtr);
	break;
      case TCLDOM_EVENT_RELATEDNODE:
	Tcl_DecrRefCount(relatedNodePtr);
	relatedNodePtr = objv[1];
	Tcl_IncrRefCount(relatedNodePtr);
	break;
      case TCLDOM_EVENT_PREVVALUE:
	Tcl_DecrRefCount(prevValuePtr);
	prevValuePtr = objv[1];
	Tcl_IncrRefCount(prevValuePtr);
	break;
      case TCLDOM_EVENT_NEWVALUE:
	Tcl_DecrRefCount(newValuePtr);
	newValuePtr = objv[1];
	Tcl_IncrRefCount(newValuePtr);
	break;
      case TCLDOM_EVENT_ATTRNAME:
	Tcl_DecrRefCount(attrNamePtr);
	attrNamePtr = objv[1];
	Tcl_IncrRefCount(attrNamePtr);
	break;
      case TCLDOM_EVENT_ATTRCHANGE:
	Tcl_DecrRefCount(attrChangePtr);
	attrChangePtr = objv[1];
	Tcl_IncrRefCount(attrChangePtr);
	break;
      default:
	Tcl_SetResult(interp, "bad option", NULL);
        goto mutation_error;
      }

      objc -= 2;
      objv += 2;
    }

    if (TclDOM_PostMutationEvent(interp, nodePtr->doc, objv[2], objv[3], bubblesPtr, cancelablePtr, relatedNodePtr, prevValuePtr, newValuePtr, attrNamePtr, attrChangePtr) != TCL_OK) {
      goto mutation_error;
    }

    break;

mutation_error:
    Tcl_DecrRefCount(bubblesPtr);
    Tcl_DecrRefCount(cancelablePtr);
    Tcl_DecrRefCount(relatedNodePtr);
    Tcl_DecrRefCount(prevValuePtr);
    Tcl_DecrRefCount(newValuePtr);
    Tcl_DecrRefCount(attrNamePtr);
    Tcl_DecrRefCount(attrChangePtr);

    return TCL_ERROR;

  default:

    Tcl_SetResult(interp, "unknown method", NULL);
    return TCL_ERROR;

  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMNewEvent --
 *
 *  Create a Tcl_Obj for an event.
 *
 * Results:
 *  Returns Tcl_Obj*.
 *
 * Side effects:
 *  Allocates object.
 *
 *----------------------------------------------------------------------------
 */

Tcl_Obj *
TclDOMNewEvent (interp, doc, type)
     Tcl_Interp *interp;
     xmlDocPtr doc;
     Tcl_Obj *type;
{
  Tcl_Obj *newPtr;
  TclDOMEvent *eventPtr;
  TclDOMDocument *tcldomDoc;
  Tcl_Time time;
  Tcl_HashEntry *entry;
  int inew;

  entry = Tcl_FindHashEntry(&docByPtr, (char *) doc);
  if (!entry) {
    return NULL;
  }
  tcldomDoc = (TclDOMDocument *) Tcl_GetHashValue(entry);

  eventPtr = (TclDOMEvent *) Tcl_Alloc(sizeof(TclDOMEvent));
  eventPtr->interp = interp;
  eventPtr->cmdname = Tcl_Alloc(30);
  sprintf(eventPtr->cmdname, "%s.event%d", tcldomDoc->token, tcldomDoc->eventCntr++);
  eventPtr->ownerDocument = tcldomDoc;

  newPtr = Tcl_NewObj();
  newPtr->bytes = Tcl_Alloc(30);
  strcpy(newPtr->bytes, eventPtr->cmdname);
  newPtr->length = strlen(newPtr->bytes);

  eventPtr->objPtr = newPtr;

  entry = Tcl_CreateHashEntry(tcldomDoc->events, newPtr->bytes, &inew);
  Tcl_SetHashValue(entry, (void *) eventPtr);
  Tcl_IncrRefCount(newPtr);

  eventPtr->stopPropagation = 0;
  eventPtr->preventDefault = 0;
  eventPtr->dispatched = 0;

  eventPtr->altKey = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->altKey);
  eventPtr->attrName = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->attrName);
  eventPtr->attrChange = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->attrChange);
  eventPtr->bubbles = Tcl_NewIntObj(1);
  Tcl_IncrRefCount(eventPtr->bubbles);
  eventPtr->button = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->button);
  eventPtr->cancelable = Tcl_NewIntObj(1);
  Tcl_IncrRefCount(eventPtr->cancelable);
  eventPtr->clientX = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->clientX);
  eventPtr->clientY = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->clientY);
  eventPtr->ctrlKey = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->ctrlKey);
  eventPtr->currentNode = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->currentNode);
  eventPtr->detail = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->detail);
  eventPtr->eventPhase = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->eventPhase);
  eventPtr->metaKey = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->metaKey);
  eventPtr->newValue = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->newValue);
  eventPtr->prevValue = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->prevValue);
  eventPtr->relatedNode = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->relatedNode);
  eventPtr->screenX = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->screenX);
  eventPtr->screenY = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->screenY);
  eventPtr->shiftKey = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->shiftKey);
  eventPtr->target = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->target);

  /* Timestamping of DOM events is not available in Tcl 8.3.x.
   * The required API (Tcl_GetTime) is public only since 8.4.0.
   */

#if (TCL_MAJOR_VERSION > 8) || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION > 3))
  Tcl_GetTime(&time);
  eventPtr->timeStamp = Tcl_NewLongObj(time.sec*1000 + time.usec/1000);
#else
  eventPtr->timeStamp = Tcl_NewLongObj (0);
#endif
  Tcl_IncrRefCount(eventPtr->timeStamp);

  eventPtr->type = type;
  Tcl_IncrRefCount(eventPtr->type);
  eventPtr->view = Tcl_NewObj();
  Tcl_IncrRefCount(eventPtr->view);

  newPtr->internalRep.otherValuePtr = (VOID *) eventPtr;
  newPtr->typePtr = &TclDOM_EventObjType;

  eventPtr->cmd = Tcl_CreateObjCommand(interp, newPtr->bytes, TclDOMEventCommand, (ClientData) eventPtr, TclDOMDeleteEvent);

  return newPtr;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMInitDoc --
 *
 *  Initialise a TclDOMDocument structure.
 *
 * Results:
 *  Structure is initialised for use.
 *
 * Side effects:
 *  Allocates memory.
 *
 *----------------------------------------------------------------------------
 */

void
TclDOMInitDoc (tcldomDoc)
  TclDOMDocument *tcldomDoc;
{
    Tcl_HashEntry *entry;
    int new;

    tcldomDoc->token = Tcl_Alloc(20);
    sprintf(tcldomDoc->token, "doc%d", docCntr++);
    tcldomDoc->docPtr = NULL;

    tcldomDoc->nodes = (Tcl_HashTable *) Tcl_Alloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tcldomDoc->nodes, TCL_STRING_KEYS);
    tcldomDoc->nodeCntr = 0;
    tcldomDoc->events = (Tcl_HashTable *) Tcl_Alloc(sizeof(Tcl_HashTable));
    Tcl_InitHashTable(tcldomDoc->events, TCL_STRING_KEYS);
    tcldomDoc->eventCntr = 0;
    for (new = 0; new < NUM_EVENT_TYPES; new++) {
      tcldomDoc->listening[new] = 0;
    }

    entry = Tcl_CreateHashEntry(&documents, tcldomDoc->token, &new);
    Tcl_SetHashValue(entry, tcldomDoc);

}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_CreateObjFromDoc --
 *
 *  Create a Tcl_Obj to wrap a document.
 *
 * Results:
 *  Returns Tcl_Obj*.
 *
 * Side effects:
 *  Allocates object.
 *
 *----------------------------------------------------------------------------
 */

Tcl_Obj *
TclDOM_CreateObjFromDoc (docPtr)
  xmlDocPtr docPtr;
{
  Tcl_HashEntry *entry;
  int new;

  /*
   * This document may have already been wrapped by a Tcl_Object.
   */

  if (docPtr->_private) {
    Tcl_IncrRefCount((Tcl_Obj *) docPtr->_private);
    return (Tcl_Obj *) docPtr->_private;
  } else {
    TclDOMDocument *tcldomDoc;
    Tcl_Obj *objPtr;

    tcldomDoc = (TclDOMDocument *) Tcl_Alloc(sizeof(TclDOMDocument));
    TclDOMInitDoc(tcldomDoc);
    tcldomDoc->docPtr = docPtr;
    entry = Tcl_CreateHashEntry(&docByPtr, (char *) docPtr, &new);
    Tcl_SetHashValue(entry, tcldomDoc);

    objPtr = Tcl_NewObj();
    Tcl_IncrRefCount(objPtr);
    objPtr->internalRep.otherValuePtr = (VOID *) tcldomDoc;
    objPtr->typePtr = &TclDOM_DocObjType;
    objPtr->bytes = Tcl_Alloc(20);
    strcpy(objPtr->bytes, tcldomDoc->token);
    objPtr->length = strlen(tcldomDoc->token);

    docPtr->_private = (void *) objPtr;

    return objPtr;
  }
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_CreateObjFromNode --
 *
 *  Create a Tcl_Obj to wrap a tree node.
 *
 *  NB. We could get alot fancier in generating a
 *  string rep, eg. to allow mapping from a string
 *  rep back to an xmlNodePtr.  However, this will
 *  use alot more memory since there may be many
 *  nodes.  So, the application will have to
 *  keep hold of the internal rep.
 *
 * Results:
 *  Returns Tcl_Obj*.
 *
 * Side effects:
 *  Allocates object.
 *
 *----------------------------------------------------------------------------
 */

Tcl_Obj *
TclDOM_CreateObjFromNode (nodePtr)
  xmlNodePtr nodePtr;
{
  if (nodePtr->_private) {
    Tcl_IncrRefCount((Tcl_Obj *) nodePtr->_private);
    return (Tcl_Obj *) nodePtr->_private;
  } else {
    TclDOMDocument *tcldomDoc;
    Tcl_Obj *objPtr;
    Tcl_HashEntry *entry;
    int new;

    entry = Tcl_FindHashEntry(&docByPtr, (char *) nodePtr->doc);
    if (!entry) {
      return NULL;
    }
    tcldomDoc = (TclDOMDocument *) Tcl_GetHashValue(entry);

    objPtr = Tcl_NewObj();
    Tcl_IncrRefCount(objPtr);
    objPtr->internalRep.otherValuePtr = (VOID *) nodePtr;
    objPtr->typePtr = &TclDOM_NodeObjType;

    objPtr->bytes = Tcl_Alloc(30);
    sprintf(objPtr->bytes, "%s.node%d", tcldomDoc->token, tcldomDoc->nodeCntr++);
    objPtr->length = strlen(objPtr->bytes);

    nodePtr->_private = (void *) objPtr;

    entry = Tcl_CreateHashEntry(tcldomDoc->nodes, objPtr->bytes, &new);
    if (!new) {
      /* internal error: this should never occur */
      return NULL;
    }
    Tcl_SetHashValue(entry, (void *) nodePtr);

    return objPtr;
  }
}

/*
 *	Manage TclDOM objects
 */

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_GetDocFromObj --
 *
 *  Gets an xmlDocPtr from a Tcl_Obj.
 *
 * Results:
 *  Returns success code.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_GetDocFromObj (interp, objPtr, docPtr)
     Tcl_Interp *interp;
     Tcl_Obj *objPtr;
     xmlDocPtr *docPtr;
{
  TclDOMDocument *tcldomDoc;

  if (objPtr->typePtr == &TclDOM_DocObjType) {
    tcldomDoc = (TclDOMDocument *) objPtr->internalRep.otherValuePtr;
    *docPtr = tcldomDoc->docPtr;
  } else if (TclDOM_DocSetFromAny(interp, objPtr) == TCL_OK) {
    tcldomDoc = (TclDOMDocument *) objPtr->internalRep.otherValuePtr;
    *docPtr = tcldomDoc->docPtr;
  } else {
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_GetDoc2FromObj --
 *
 *  Gets a TclDOMDocument from a Tcl_Obj.
 *
 * Results:
 *  Returns success code.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_GetDoc2FromObj (interp, objPtr, docPtr)
     Tcl_Interp *interp;
     Tcl_Obj *objPtr;
     TclDOMDocument **docPtr;
{

  if (objPtr->typePtr == &TclDOM_DocObjType) {
    *docPtr = (TclDOMDocument *) objPtr->internalRep.otherValuePtr;
  } else if (TclDOM_DocSetFromAny(interp, objPtr) == TCL_OK) {
    *docPtr = (TclDOMDocument *) objPtr->internalRep.otherValuePtr;
  } else {
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_GetNodeFromObj --
 *
 *  Gets an xmlNodePtr from a Tcl_Obj.
 *
 * Results:
 *  Returns success code.
 *
 * Side effects:
 *  None.
 *
 *----------------------------------------------------------------------------
 */

int
TclDOM_GetNodeFromObj (interp, objPtr, nodePtr)
     Tcl_Interp *interp;
     Tcl_Obj *objPtr;
     xmlNodePtr *nodePtr;
{

  if (objPtr->typePtr == &TclDOM_NodeObjType) {
     *nodePtr = objPtr->internalRep.otherValuePtr;
  } else if (TclDOM_NodeSetFromAny(interp, objPtr) == TCL_OK) {
    *nodePtr = objPtr->internalRep.otherValuePtr;
  } else {
    return TCL_ERROR;
  }

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_NewDoc --
 *
 *  Creates a new xmlDocPtr and wraps it in a Tcl_Obj.
 *
 * Results:
 *  Returns a *TclObj
 *
 * Side effects:
 *  Objects allocated.
 *
 *----------------------------------------------------------------------------
 */

Tcl_Obj *
TclDOM_NewDoc(interp)
     Tcl_Interp *interp;
{
  xmlDocPtr new;

  new = xmlNewDoc("1.0");
  if (!new) {
    Tcl_SetResult(interp, "unable to create document", NULL);
    return NULL;
  }

  return TclDOM_CreateObjFromDoc(new);
}

int
TclDOM_DocSetFromAny(interp, objPtr)
     Tcl_Interp *interp;
     Tcl_Obj *objPtr;
{
  Tcl_HashEntry *entryPtr;

  entryPtr = Tcl_FindHashEntry(&documents, Tcl_GetStringFromObj(objPtr, NULL));

  if (entryPtr) {

    if (objPtr->typePtr != NULL && objPtr->typePtr->freeIntRepProc != NULL) {
      objPtr->typePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.otherValuePtr = Tcl_GetHashValue(entryPtr);
    objPtr->typePtr = &TclDOM_DocObjType;

  } else {
    char fmt[] = "token \"%s\" is not a DOM Document";
    char *result, *string;
    int len;

    string = Tcl_GetStringFromObj(objPtr, &len);
    result = Tcl_Alloc(len + strlen(fmt) + 1);
    sprintf(result, fmt, string);
    Tcl_ResetResult(interp);
    Tcl_AppendToObj(Tcl_GetObjResult(interp), result, -1);

    Tcl_Free (result);

    return TCL_ERROR;
  }

  return TCL_OK;
}

void
TclDOM_DocUpdate(objPtr)
     Tcl_Obj *objPtr;
{
  Tcl_HashEntry *entryPtr;
  Tcl_HashSearch search;

  entryPtr = Tcl_FirstHashEntry(&documents, &search);
  while (entryPtr && objPtr->internalRep.otherValuePtr != Tcl_GetHashValue(entryPtr)) {
    entryPtr = Tcl_NextHashEntry(&search);
  }
  Tcl_InvalidateStringRep(objPtr);
  if (entryPtr == NULL) {
    objPtr->bytes = NULL;
    objPtr->length = 0;
  } else {
    objPtr->length = strlen(Tcl_GetHashKey(&documents, entryPtr));
    objPtr->bytes = Tcl_Alloc(objPtr->length + 1);
    strcpy(objPtr->bytes, Tcl_GetHashKey(&documents, entryPtr));
  }
}

void
TclDOM_DocDup(srcPtr, dstPtr)
     Tcl_Obj *srcPtr;
     Tcl_Obj *dstPtr;
{
  TclDOMDocument *srcTcldomDoc, *dstTcldomDoc;
  xmlDocPtr dstDoc;
  Tcl_HashEntry *entry;
  int new;

  srcTcldomDoc = (TclDOMDocument *) srcPtr->internalRep.otherValuePtr;

  dstDoc = xmlCopyDoc(srcTcldomDoc->docPtr, 1);
  if (dstDoc == NULL) {
    return;
  }

  /*
   * xmlCopyDoc doesn't copy some of the fields.
   */

  if (srcTcldomDoc->docPtr->URL) {
    dstDoc->URL = Tcl_Alloc(strlen(srcTcldomDoc->docPtr->URL) + 1);
    strcpy((char *) dstDoc->URL, srcTcldomDoc->docPtr->URL);
  }

  Tcl_InvalidateStringRep(dstPtr);

  dstTcldomDoc = (TclDOMDocument *) Tcl_Alloc(sizeof(TclDOMDocument));
  TclDOMInitDoc(dstTcldomDoc);
  dstTcldomDoc->docPtr = dstDoc;
  entry = Tcl_CreateHashEntry(&docByPtr, (char *) dstDoc, &new);
  Tcl_SetHashValue(entry, dstTcldomDoc);
  dstPtr->typePtr = &TclDOM_DocObjType;
  dstPtr->internalRep.otherValuePtr = (ClientData) dstTcldomDoc;
  dstPtr->bytes = Tcl_Alloc(20);
  strcpy(dstPtr->bytes, dstTcldomDoc->token);
  dstPtr->length = strlen(dstTcldomDoc->token);
  dstDoc->_private = (void *) dstPtr;
}

void
TclDOM_DocFree(objPtr)
     Tcl_Obj *objPtr;
{
  /* Nothing to do */
}

int
TclDOM_NodeSetFromAny(interp, objPtr)
     Tcl_Interp *interp;
     Tcl_Obj *objPtr;
{
  TclDOMDocument *tcldomDoc;
  Tcl_HashEntry *entry;
  char *id, doc[21], node[21];
  int i, idlen, len;

  /* Parse string rep for doc and node ids */
  id = Tcl_GetStringFromObj(objPtr, &idlen);
  for (i = 0; i < idlen && id[i] != '.' && i < 20; i++) {
    if (!((id[i] >= 'a' && id[i] <= 'z') || (id[i] >= '0' && id[i] <= '9'))) {
        /* only lowercase chars and digits are found in a token */
        Tcl_Obj *tmpPtr = Tcl_NewStringObj("malformed node token \"", -1);
        Tcl_AppendObjToObj(tmpPtr, objPtr);
        Tcl_AppendStringsToObj(tmpPtr, "\"", NULL);
        Tcl_SetObjResult(interp, tmpPtr);
        return TCL_ERROR;
    }
    doc[i] = id[i];
  }
  if (i == idlen || id[i] != '.') {
    Tcl_Obj *tmpPtr = Tcl_NewStringObj("malformed node token \"", -1);
    Tcl_AppendObjToObj(tmpPtr, objPtr);
    Tcl_AppendStringsToObj(tmpPtr, "\"", NULL);
    Tcl_SetObjResult(interp, tmpPtr);
    return TCL_ERROR;
  }
  doc[i] = '\0';
  for (len = i + 1, i = 0; i + len < idlen && i < 20; i++) {
    node[i] = id[len + i];
  }
  node[i] = '\0';

  entry = Tcl_FindHashEntry(&documents, doc);
  if (!entry) {
    Tcl_SetResult(interp, "invalid node token", NULL);
    return TCL_ERROR;
  }
  tcldomDoc = (TclDOMDocument *) Tcl_GetHashValue(entry);

/*
  sprintf(dbgbuf, "Looking for node \"%s\"\nDocument node table contains:\n", id);
  Tcl_WriteChars(stderrChan, dbgbuf, -1);
  {
    Tcl_HashSearch search;

    entry = Tcl_FirstHashEntry(tcldomDoc->nodes, &search);
    while (entry) {
      xmlNodePtr tmp;

      tmp = (xmlNodePtr) Tcl_GetHashValue(entry);
      sprintf(dbgbuf, "node \"%s\", name \"%s\"\n", Tcl_GetHashKey(tcldomDoc->nodes, entry), tmp->name);
      Tcl_WriteChars(stderrChan, dbgbuf, -1);

      entry = Tcl_NextHashEntry(&search);
    }
  }
*/

  entry = Tcl_FindHashEntry(tcldomDoc->nodes, id);
  if (entry) {

    if (objPtr->typePtr != NULL && objPtr->typePtr->freeIntRepProc != NULL) {
      objPtr->typePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.otherValuePtr = Tcl_GetHashValue(entry);
    objPtr->typePtr = &TclDOM_NodeObjType;

    return TCL_OK;

  } else {
    Tcl_SetResult(interp, "not a DOM node", NULL);
    return TCL_ERROR;
  }
}

void
TclDOM_NodeUpdate(objPtr)
     Tcl_Obj *objPtr;
{
  xmlNodePtr nodePtr = (xmlNodePtr) objPtr->internalRep.otherValuePtr;
  TclDOMDocument *tcldomDoc;
  Tcl_HashEntry *entry;
  Tcl_HashSearch search;

  entry = Tcl_FindHashEntry(&docByPtr, (char *) nodePtr->doc);
  if (entry) {
    tcldomDoc = (TclDOMDocument *) Tcl_GetHashValue(entry);
    entry = Tcl_FirstHashEntry(tcldomDoc->nodes, &search);
    while (entry && objPtr->internalRep.otherValuePtr != Tcl_GetHashValue(entry)) {
      entry = Tcl_NextHashEntry(&search);
    }
    Tcl_InvalidateStringRep(objPtr);
    if (entry == NULL) {
      objPtr->bytes = NULL;
      objPtr->length = 0;
    } else {
      objPtr->length = strlen(Tcl_GetHashKey(tcldomDoc->nodes, entry));
      objPtr->bytes = Tcl_Alloc(objPtr->length + 1);
      strcpy(objPtr->bytes, Tcl_GetHashKey(tcldomDoc->nodes, entry));
    }
  }
}

/* This operation is not supported */
void
TclDOM_NodeDup(srcPtr, dstPtr)
     Tcl_Obj *srcPtr;
     Tcl_Obj *dstPtr;
{
  Tcl_InvalidateStringRep(dstPtr);
}

void
TclDOM_NodeFree(objPtr)
     Tcl_Obj *objPtr;
{
  /* TclDOMForgetNode((xmlNodePtr) objPtr->internalRep.otherValuePtr, objPtr); */
}

int
TclDOM_EventSetFromAny(interp, objPtr)
     Tcl_Interp *interp;
     Tcl_Obj *objPtr;
{
  TclDOMDocument *tcldomDoc;
  Tcl_HashEntry *entry;
  char *id, doc[20], event[20];
  int i, idlen, len;

  /* Parse string rep for doc and event ids */
  id = Tcl_GetStringFromObj(objPtr, &idlen);
  for (i = 0; i < idlen && id[i] != '.'; i++) {
    doc[i] = id[i];
  }
  if (i == idlen || id[i] != '.') {
    Tcl_SetResult(interp, "malformed event token", NULL);
    return TCL_ERROR;
  }
  doc[i] = '\0';
  for (len = i + 1, i = 0; i + len < idlen; i++) {
    event[i] = id[len + i];
  }
  event[i] = '\0';

  entry = Tcl_FindHashEntry(&documents, doc);
  if (!entry) {
    Tcl_SetResult(interp, "invalid event token", NULL);
    return TCL_ERROR;
  }
  tcldomDoc = (TclDOMDocument *) Tcl_GetHashValue(entry);

  entry = Tcl_FindHashEntry(tcldomDoc->events, id);
  if (entry) {

    if (objPtr->typePtr != NULL && objPtr->typePtr->freeIntRepProc != NULL) {
      objPtr->typePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.otherValuePtr = Tcl_GetHashValue(entry);
    objPtr->typePtr = &TclDOM_EventObjType;
  } else {
    Tcl_SetResult(interp, "unable to find event", NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}

void
TclDOM_EventUpdate(objPtr)
     Tcl_Obj *objPtr;
{
  TclDOMEvent *event = (TclDOMEvent *) objPtr->internalRep.otherValuePtr;
  TclDOMDocument *tcldomDoc = event->ownerDocument;
  Tcl_HashEntry *entry;
  Tcl_HashSearch search;

  entry = Tcl_FirstHashEntry(tcldomDoc->events, &search);
  while (entry && objPtr->internalRep.otherValuePtr != Tcl_GetHashValue(entry)) {
    entry = Tcl_NextHashEntry(&search);
  }
  Tcl_InvalidateStringRep(objPtr);
  if (entry == NULL) {
    objPtr->bytes = NULL;
    objPtr->length = 0;
  } else {
    objPtr->length = strlen(Tcl_GetHashKey(tcldomDoc->events, entry));
    objPtr->bytes = Tcl_Alloc(objPtr->length + 1);
    strcpy(objPtr->bytes, Tcl_GetHashKey(tcldomDoc->events, entry));
  }
}

/* This operation is not supported */
void
TclDOM_EventDup(srcPtr, dstPtr)
     Tcl_Obj *srcPtr;
     Tcl_Obj *dstPtr;
{
  Tcl_InvalidateStringRep(dstPtr);
}

/* This operation does nothing - app should use dom::event interface */
void
TclDOM_EventFree(objPtr)
     Tcl_Obj *objPtr;
{
    TclDOMEvent *event = (TclDOMEvent *) objPtr->internalRep.otherValuePtr;
    event->objPtr = NULL;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMGetPath --
 *
 *  Constructs a list of ancestor nodes.
 *
 * Results:
 *  Returns list as a Tcl_Obj.
 *
 * Side effects:
 *  Allocates Tcl_Obj structures.
 *
 *----------------------------------------------------------------------------
 */

static Tcl_Obj *
TclDOMGetPath (interp, nodePtr)
     Tcl_Interp *interp;
     xmlNodePtr nodePtr;
{
  Tcl_Obj *listPtr, *resultPtr;
  Tcl_Obj *objv[2];

  if (nodePtr) {
    objv[0] = TclDOM_CreateObjFromNode(nodePtr);
    objv[1] = NULL;

    listPtr = Tcl_NewListObj(1, objv);
    if (nodePtr->parent) {
      resultPtr = TclDOMGetPath(interp, nodePtr->parent);
      Tcl_ListObjAppendList(interp, resultPtr, listPtr);
    } else {
      resultPtr = listPtr;
    }
    return resultPtr;
  } else {
    return Tcl_NewObj();
  }
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOM_GetEventFromObj --
 *
 *  Returns TclDOMEvent pointer from Tcl object.
 *
 * Results:
 *  Returns success code and assigns event pointer.
 *
 * Side effects:
 *  May invalidate object's internal rep.
 *
 *----------------------------------------------------------------------------
 */

static int
TclDOM_GetEventFromObj (interp, objPtr, eventPtr)
     Tcl_Interp *interp;
     Tcl_Obj *objPtr;
     TclDOMEvent **eventPtr;
{
  if (!objPtr) {
    Tcl_SetResult(interp, "null object", NULL);
    return TCL_ERROR;
  }
  if (objPtr->typePtr == &TclDOM_EventObjType) {
    *eventPtr = (TclDOMEvent *) objPtr->internalRep.otherValuePtr;
  } else if (TclDOM_EventSetFromAny(interp, objPtr) == TCL_OK) {
    *eventPtr = (TclDOMEvent *) objPtr->internalRep.otherValuePtr;
  } else {
    return TCL_ERROR;
  }
  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * TclDOMGenericError --
 *
 *  Handler for parse errors.
 *
 * Results:
 *  Stores error message.
 *
 * Side effects:
 *  Transform will return error condition.
 *
 *----------------------------------------------------------------------------
 */

static void
TclDOMGenericError (void *ctx, const char *msg, ...)
{
  va_list args;
  GenericError_Info *errorInfoPtr = (GenericError_Info *) ctx;
  char buf[2048];
  int len;

  errorInfoPtr->code = TCL_ERROR;

  if (!errorInfoPtr->msg) {
    errorInfoPtr->msg = Tcl_NewObj();
    Tcl_IncrRefCount(errorInfoPtr->msg);
  }

  va_start(args,msg);
  len = vsnprintf(buf, 2047, msg, args);
  va_end(args);

  Tcl_AppendToObj(errorInfoPtr->msg, buf, len);

}

