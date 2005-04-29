/*
 * tclDomProInt.h --
 *
 *       This header file contains information for the internal
 *       implementation of the TclDomPro extension.
 *
 * Copyright (c) 1998-2000 Ajuba Solutions.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * $Id: tclDomProInt.h,v 1.12 2003/04/02 22:48:14 jenglish Exp $
 */

#ifndef _TCLDOMPROINT
#define _TCLDOMPROINT

#include <tcl.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <stddef.h>
#include "tdp.h"

#include <expat.h>


/*
 * Define position of the reference node with respect
 * to an iterator
 */

typedef enum {
    REFERENCE_IS_BEFORE_ITERATOR = 1,
    REFERENCE_IS_AFTER_ITERATOR = 0
} TclDomIteratorPos;

typedef enum {
    ELEMENT_NODE = 1,
    ATTRIBUTE_NODE = 2,
    TEXT_NODE = 3,
    CDATA_SECTION_NODE = 4,
    ENTITY_REFERENCE_NODE = 5,
    ENTITY_NODE = 6,
    PROCESSING_INSTRUCTION_NODE = 7,
    COMMENT_NODE = 8,
    DOCUMENT_NODE = 9,
    DOCUMENT_TYPE_NODE = 10,
    DOCUMENT_FRAGMENT_NODE = 11,
    NOTATION_NODE = 12
} TclDomNodeType;

/*
 * The following is used to determine how
 * to display error info.
 */

typedef enum {
    UTF8 = 0, 
    UTF16 = 1,
    UTF16SWAPPED = 2,
    OTHER = 3
} TclDomDocumentEncoding;

#define SHOW_ALL 0xffff
#define SHOW_ELEMENT                (1 << (ELEMENT_NODE-1))
#define SHOW_ATTRIBUTE              (1 << (ATTRIBUTE_NODE-1))
#define SHOW_TEXT                   (1 << (TEXT_NODE-1))
#define SHOW_CDATA_SECTION          (1 << (CDATA_SECTION_NODE-1))
#define SHOW_ENTITY_REFERENCE       (1 << (ENTITY_REFERENCE_NODE-1))
#define SHOW_ENTITY                 (1 << (ENTITY_NODE-1))
#define SHOW_PROCESSING_INSTRUCTION (1 << (PROCESSING_INSTRUCTION_NODE-1))
#define SHOW_COMMENT                (1 << (COMMENT_NODE-1))
#define SHOW_DOCUMENT               (1 << (DOCUMENT_NODE-1))
#define SHOW_DOCUMENT_TYPE          (1 << (DOCUMENT_TYPE_NODE-1))
#define SHOW_DOCUMENT_FRAGMENT      (1 << (DOCUMENT_FRAGMENT_NODE-1))
#define SHOW_NOTATION               (1 << (NOTATION_NODE-1))

#define DOM_ACCEPT 0
#define DOM_SKIP   1
#define DOM_REJECT 2


/* 
 * Define text for standard DOM error codes 
 */

#define INDEX_SIZE_ERR_TEXT "index size error: an index or size is negative, or greater than the allowed value"

#define DOMSTRING_SIZE_ERR_TEXT "DOMString size error: the specified range of text does not fit into a DOMString"

#define INVALID_CHARACTER_ERR_TEXT "invalid character error: a name contains an invalid character"

#define NO_DATA_ALLOWED_ERR_TEXT "no data allowed error: dat is specified for a node that does not support data"

#define NOT_FOUND_ERR_TEXT "not found error: attempt to reference a node in a context in which it doesn't exist"

#define HIERARCHY_REQUEST_ERR_TEXT "hierarchy request error: attempt to insert a node where is is not allowed"

#define WRONG_DOCUMENT_ERR_TEXT "wrong document error: referenced nodes were created in different documents"

#define NO_MODIFICATION_ALLOWED_ERR_TEXT "no modification allowed error: an attempt was made to modify an object where modifications are not allowed"

#define NOT_SUPPORTED_ERR_TEXT "not supported error: the implementation does not support the type of object requested"

#define INUSE_ATTRIBUTE_ERR_TEXT "in use attribute error: an attempt was made to add an attribute that is already in use elsewhere"

#define NAMESPACE "dom::"
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "dom"
#endif
#define NAMESPACE_PREFIX "::dom::"

typedef char *TclDomString;

/*
 * Definition of an Element node
 * This type is also used to pass generic node
 * pointer, which are then typecast to the appropriate 
 * type.
 */

typedef struct _tcldomNode {
    unsigned int            nodeId;                 /* The node's unique id */
    Tcl_HashEntry           *entryPtr;              /* If not null, entry in 
	                                             * hash table */
    TclDomNodeType          nodeType;               /* Type of node */
    struct _tclDomDocument  *containingDocumentPtr; /* Document that contains 
	                                             * this node */
    struct _tcldomNode      *parentNodePtr;         /* Parent node, or null for
	                                             * fragment */
    struct _tcldomNode      *previousSiblingPtr;    /* Previous sibling */
    struct _tcldomNode      *nextSiblingPtr;        /* Next sibling */
    TclDomString            nodeName;               /* Node's name if not 
	                                             * NULL */
    TclDomString            nodeValue;              /* Node's value if not 
	                                             * NULL */
    int                     valueLength;            /* Length of value string 
	                                             * in bytes */
    int                     startLine;              /* Line in source (one-up) 
	                                             * where node is defined */
    int                     startColumn;            /* Column (0-up) where node
	                                             * is defined */
    int                     startWidth;             /* Length of node 
	                                             * production in UTF-8 
						     * characters */
    int			    startLineClose;	    /* Line where start-tag ends */
    int			    startColumnClose;	    /* Column where start-tag ends */
    int                     endLine;                /* For elements, line where
	                                             * node ends */
    int                     endColumn;              /* For elements, column 
	                                             * where node ends */
    int                     endWidth;               /* For elements, width of 
	                                             * end production in UTF-8 
						     * characters */
    int			    endLineClose;	    /* Line where end-tag ends */
    int			    endColumnClose;	    /* Column where end-tag ends */
    int                     nodeComplete;           /* For incremental parsing,
	                                             * 1 -> this node is 
						     * completely defined */

    /*
     * All node data types except Attributes must be identical above this point
     */


    struct _tcldomNode      *firstChildPtr;         /* First child of this 
	                                                 * node */
    struct _tcldomNode      *lastChildPtr;          /* Last child of
	                                                 * this node */
    struct _tcldomAttributeNode     *firstAttributePtr;     /* First attribute 
	                                                 * of this node */
    struct _tcldomAttributeNode     *lastAttributePtr;      /* Last attribute 
	                                                 * of this node */

    Tcl_Obj                 *childNodeListVarName;  /* If not null, name of 
						     * variable representing 
						     * live child list */
    Tcl_Obj                 *attributeArrayVarName; /* If not null, name of 
						     * variable representing 
						     * live attribute array */
} TclDomNode;

typedef struct _tcldomTextNode {
    unsigned int            nodeId; 
    Tcl_HashEntry           *entryPtr;
    TclDomNodeType          nodeType;
    struct _tclDomDocument  *containingDocumentPtr;
    struct _tcldomNode      *parentNodePtr;
    struct _tcldomNode      *previousSiblingPtr;
    struct _tcldomNode      *nextSiblingPtr;
    TclDomString            nodeName;
    TclDomString            nodeValue; 
    int                     valueLength;

    int                     startLine;	    /* the line at which the construct starts */
    int                     startColumn;    /* the column at which the construct starts */
    int                     startWidth;	    /* the width of the construct DEPRECATED  */
    int			    startCloseLine;  /* the line at which the construct ends */
    int			    startCloseCol;   /* the column at which the  ends */
    int                     endLine;
    int                     endColumn; 
    int                     endWidth;
    int			    endCloseLine;
    int			    endCloseCol;   
    int                     nodeComplete;   /* For incremental parsing, 
	                                         * 1 -> this node is completely 
											 * defined */

    /*
     * All node data types except Attributes must be identical above this point
     */
} TclDomTextNode;

typedef struct _tcldomDocTypeNode {
    unsigned int            nodeId; 
    Tcl_HashEntry           *entryPtr;
    TclDomNodeType          nodeType;
    struct _tclDomDocument  *containingDocumentPtr;
    struct _tcldomNode      *parentNodePtr;
    struct _tcldomNode      *previousSiblingPtr;
    struct _tcldomNode      *nextSiblingPtr;
    TclDomString            nodeName;
    TclDomString            nodeValue; 
    int                     valueLength;

    int                     startLine;
    int                     startColumn;
    int                     startWidth;
    int			    startCloseLine;
    int			    startCloseCol;
    int                     endLine;
    int                     endColumn;  
    int                     endWidth;
    int			    endCloseLine;
    int			    endCloseCol;
    int                     nodeComplete;       /* For incremental 
	                                         * parsing, 1 -> this node is
						 * completely defined */


    /*
     * All node data types except Attributes must be identical above this point
     */

    TclDomString	    publicId;       /* DOM level 2. */    
    TclDomString	    systemId;	    /* DOM level 2. */
    TclDomString	    internalSubset; /* DOM level 2. */
} TclDomDocTypeNode;

typedef struct _tcldomAttributeNode {
    unsigned int            nodeId; 
    Tcl_HashEntry           *entryPtr;
    TclDomNodeType          nodeType;
    struct _tclDomDocument  *containingDocumentPtr;
    struct _tcldomNode      *parentNodePtr;
    struct _tcldomNode      *previousSiblingPtr;
    struct _tcldomAttributeNode     *nextSiblingPtr;
    TclDomString            nodeName;
    TclDomString            nodeValue;
    int                     valueLength;
   
} TclDomAttributeNode;


typedef struct _tclDomDocument {
    Tcl_Interp         *interp;
    struct _TclDomInterpData    *interpDataPtr;
    TclDomNode         *selfPtr;                /* the document as a node */
    struct _tcldomNode *fragmentsPtr;
    TclDomDocumentEncoding encoding;
} TclDomDocument;

/*
 * This structure encapsulates information necessary to invoke
 * a NodeFilter.
 */

typedef struct _tclDomNodeFilter {
    Tcl_Interp          *interp;
    struct _TclDomInterpData    *interpDataPtr;
    Tcl_Obj *           filterCmdPtr;
} TclDomNodeFilter;


typedef struct _tclDomNodeIterator {
    Tcl_Interp          *interp;
    struct _TclDomInterpData    *interpDataPtr;
    Tcl_HashEntry       *entryPtr;  /* entry in hash table */
    TclDomNode          *rootPtr;      /* the subtree we're iterating through */
    TclDomNode          *referencePtr; /* where we're at in the tree */
    TclDomIteratorPos   position;           
    unsigned int        whatToShow;
    int                 expandEntityReferences;
    TclDomNodeFilter    *filterPtr;
} TclDomNodeIterator;

typedef struct _tclDomTreeWalker {
    Tcl_Interp          *interp;
    struct _TclDomInterpData    *interpDataPtr;
    Tcl_HashEntry       *entryPtr;  /* entry in hash table */
    TclDomNode          *rootPtr;      /* the subtree we're iterating through */
    TclDomNode          *currentNodePtr; /* where we're at in the tree */
    TclDomIteratorPos   position;           
    unsigned int        whatToShow;
    int                 expandEntityReferences;
    TclDomNodeFilter    *filterPtr;
} TclDomTreeWalker;


/*
 * The structure below is used to refer to an expat parser object.
 */

typedef struct TclDomExpatInfo {
    XML_Parser parser;		       /* The expat parser structure */
    Tcl_Interp *interp;		       /* Interpreter for this instance */
    struct _TclDomInterpData *interpDataPtr;
				       /* State data for the interpreter */
    TclDomDocument *documentPtr;       /* The current document */
    TclDomNode *currentNodePtr;	       /* The current DOM node */
    int depth;			       /* Nesting depth; 0 == tree root */
    int currentWidth;		       /* Node text width information passed
					* between DefaultHandler and other
					* expat handlers */
    int trim;			       /* Indicates whether empty text nodes
					* should be trimmed. */
} TclDomExpatInfo;

/*
 * This structure contains state data per interpreter
 */

typedef struct _TclDomInterpData {
    unsigned long nodeSeed;	       /* Counter for generating node ids */
    Tcl_HashTable documentHashTable;   /* Documents that exist in this
					* interpreter; key is token */
    Tcl_HashTable nodeHashTable;       /* Nodes that have been tokenized in
					* this interpreter */
    Tcl_HashTable iteratorHashTable;   /* Iterators that are currently defined
					* in this interpreter */
    Tcl_HashTable treeWalkerHashTable; /* Iterators that are currently
					* defined in this interpreter */
    Tcl_Obj *nullNodeListVarName;      /* Variable name for invariant NULL node
					* list in this interpreter */

    XML_Parser parser;
    TclDomExpatInfo parserInfo;
} TclDomInterpData;

#ifdef BUILD_tcldom
#	define TCLDOMAPI DLLEXPORT
#else
#	define TCLDOMAPI
#endif

/*
 * Declarations for externally visible functions.
 */

TCLDOMAPI int Tcldom_Init(Tcl_Interp *interp);
TCLDOMAPI int Tcldom_SafeInit(Tcl_Interp *interp);

void		TclDomDeleteNode(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, TclDomNode *nodePtr);
void		TclDomDeleteDocument(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomDocument *documentPtr);
int		TclDomCreateEmptyDocumentNode(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr);
TclDomDocument *TclDomEmptyDocument(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr);
int		TclDomSetNodeResult(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, TclDomNode *nodePtr);
TclDomDocument *TclDomGetDocumentFromToken(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, Tcl_Obj *nodeTokenPtr);
TclDomNode *	TclDomGetNodeFromToken(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, Tcl_Obj *nodeTokenPtr);
TclDomNode *	TclDomGetDocumentElement(TclDomDocument *documentPtr);
TclDomNode *	TclDomGetDoctypeNode(TclDomDocument *documentPtr);
TclDomNode *	TclDomCreateElement(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomDocument *documentPtr, char *tagName);
TclDomTextNode *TclDomCreateTextNode(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomDocument *documentPtr, char *tagName);
TclDomNode *	TclDomCreateCommentNode(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomDocument *documentPtr, char *tagName);
TclDomNode *	TclDomCreateCDATANode(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomDocument *documentPtr, char *tagName);
TclDomNode *	TclDomCreateDocumentFragment(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomDocument *documentPtr);
TclDomNode *	TclDomCreateProcessingInstructionNode(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomDocument *documentPtr, char *target, char *data);
TclDomNode *	TclDomCreateDocType(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomDocument *documentPtr, char *name, char *publicId, 
			char *systemId);
int		TclDomGetElementsByTagname(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, char *tagname,
		    TclDomNode *nodePtr);
int		TclDomAppendChild(Tcl_Interp *interp,
 		    TclDomInterpData *interpDataPtr,
 		    TclDomNode *nodePtr, TclDomNode *childPtr);
int		TclDomGetChildNodeList(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, 
		    TclDomNode *nodePtr);
Tcl_Obj	*	TclDomGetChildren(Tcl_Interp*, TclDomInterpData*, TclDomNode*);
int		TclDomValidateChildType(Tcl_Interp *interp,
		    TclDomNode *nodePtr, TclDomNode *childPtr);
int		TclDomInsertBefore(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomNode *nodePtr, TclDomNode *childPtr,
		    TclDomNode *refChildPtr);
int		TclDomRemoveChild(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomNode *nodePtr, TclDomNode *childPtr);
int		TclDomReplaceChild(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomNode *nodePtr, TclDomNode *newChildPtr,
		    TclDomNode *oldChildPtr);
int		TclDomSerialize(Tcl_Interp *interp, TclDomNode *nodePtr);
int		TclDomIsName(char *s);
int		TclDomSetAttribute(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, TclDomNode *nodePtr, 
		    char *name, char *value);
int		TclDomRemoveAttribute(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, 
		    TclDomNode *nodePtr, char *name);
int		TclDomAttributeArray(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, TclDomNode *nodePtr);
int		TclDomCloneNode(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, 
		    TclDomNode *nodePtr, int deepFlag);
TclDomNode * TclDomImportNode(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
            TclDomDocument *documentPtr,
		    TclDomNode *nodePtr, int deepFlag);
int		TclDomNodeTypeName(Tcl_Interp *interp, TclDomNode *nodePtr);
TdpDomError		TclDomSetNodeValue(TclDomNode *nodePtr,
		    char *value);
int		TclDomGetNodeName(Tcl_Interp *interp, TclDomNode *nodePtr);  
int		TclDomCreateNodeIterator(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, 
		    TclDomNode *nodePtr, unsigned int whatToShow,  
		    Tcl_Obj *filterObjPtr, 
		    int expandEntityReferences);
int		TclDomCreateTreeWalker(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, 
		    TclDomNode *nodePtr, unsigned int whatToShow,  
		    Tcl_Obj *filterObjPtr, 
		    int expandEntityReferences);
void		TclDomDeleteNodeIterator(TclDomNodeIterator *nodeIteratorPtr);
int		TclDomGetPreviousNodeFromIterator(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomNodeIterator *nodeIteratorPtr);
int		TclDomGetNextNodeFromIterator(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomNodeIterator *nodeIteratorPtr);
TclDomNodeIterator *TclDomGetNodeIteratorFromToken(Tcl_Interp *interp, 
		    TclDomInterpData *interpDataPtr, Tcl_Obj *nodeTokenPtr); 
int		TclDomGetTypeFromName(Tcl_Interp *interp, char *nodeName,
		    unsigned int *nodeMaskPtr);
int		TclDomGetTypeMaskFromName(Tcl_Interp *interp, char *nodeName,
		    unsigned int *nodeMaskPtr);
void		TclDomDeleteTreeWalker(TclDomTreeWalker *treeWalkerPtr);
TclDomTreeWalker *TclDomGetTreeWalkerFromToken(Tcl_Interp *interp, 
		    TclDomInterpData *interpDataPtr, Tcl_Obj *nodeTokenPtr); 
int		TclDomGetPreviousNodeFromTreeWalker(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomTreeWalker *treeWalkerPtr);
int		TclDomGetNextNodeFromTreeWalker(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomTreeWalker *treeWalkerPtr);
int		TclDomGetFirstChildFromTreeWalker(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomTreeWalker *treeWalkerPtr);
int		TclDomGetLastChildFromTreeWalker(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomTreeWalker *treeWalkerPtr);
int		TclDomGetPreviousSiblingFromTreeWalker(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomTreeWalker *treeWalkerPtr);
int		TclDomGetNextSiblingFromTreeWalker(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomTreeWalker *treeWalkerPtr);
int		TclDomGetParentNodeFromTreeWalker(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr,
		    TclDomTreeWalker *treeWalkerPtr);
int		TclDomGetNameFromEnum(int nodeType, char **nodeNamePtr);
int		TclDomReadDocument(Tcl_Interp *interp,
		    TclDomInterpData *interpDataPtr, char *xmlSource,
		    int length, int final, int trim);
Tcl_Obj	*	TclDomGetNodeObj(TclDomInterpData *interpDataPtr,
		    TclDomNode *nodePtr);
int     TclDomHasChildren(TclDomNode *nodePtr);
int     TclDomNodeBefore(TclDomNode *nodePtr, TclDomNode *rootNodePtr,
            unsigned int showMask, TclDomNodeFilter *filterPtr, 
			TclDomNode **nodePtrPtr);
int	    TclDomNodeAfter(TclDomNode *nodePtr, TclDomNode *rootPtr,
		    unsigned int showMask, TclDomNodeFilter *filterPtr,
		    TclDomNode **nextNodePtrPtr);
int	TclDomTreeWalkerPreviousNode(TclDomNode *nodePtr, 
		TclDomNode *rootNodePtr, unsigned int showMask, 
		TclDomNodeFilter *filterPtr, 
	        TclDomNode **previousNodePtrPtr);
void	TclDomSetDomError(Tcl_Interp *interp,
	    TdpDomError domError);

/* Live list utilities:
 */

/* %% HERE:
extern void TclDomRegisterLiveList( ... )
*/

/* Tcl version compatibility macros:
 */
#ifndef CONST84			/* added in TIP 27 */ 
#define CONST84
#endif

#endif /* _TCLDOMPROINT */
