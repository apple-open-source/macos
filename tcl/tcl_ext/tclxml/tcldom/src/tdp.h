/*
 * tdp.h
 *
 *  External TclDomPro API
 *
 * Copyright (c) 1999-2000 Ajuba Solutions
 *
 * RCS: $Id: tdp.h,v 1.2 2000/12/15 00:15:58 jenglish Exp $
 */

#ifndef _TDPH
#define _TDPH

#include "tcl.h"

#ifdef BUILD_tcldompro
#  undef TCL_STORAGE_CLASS
#  define TCL_STORAGE_CLASS DLLEXPORT
#endif

typedef struct Tdp_Document_ *Tdp_Document;
typedef struct Tdp_Node_ *Tdp_Node;
typedef struct Tdp_Element_ *Tdp_Element;
typedef struct Tdp_TextNode_ *Tdp_TextNode;
typedef struct Tdp_CommentNode_ *Tdp_CommentNode;
typedef struct Tdp_PINode_ *Tdp_PINode;
typedef struct Tdp_DocumentType_ *Tdp_DocumentType;

typedef enum {
    TDP_OK = 0,
    TDP_INDEX_SIZE_ERR = 1,
    TDP_DOMSTRING_SIZE_ERR = 2,
    TDP_HIERARCHY_REQUEST_ERR = 3,
    TDP_WRONG_DOCUMENT_ERR = 4,
    TDP_INVALID_CHARACTER_ERR = 5,
    TDP_NO_DATA_ALLOWED_ERR = 6,
    TDP_NO_MODIFICATION_ALLOWED_ERR = 7,
    TDP_NOT_FOUND_ERR = 8,
    TDP_NOT_SUPPORTED_ERR = 9,
    INUSE_ATTRIBUTE_ERR = 10
} TdpDomError;

typedef enum {
    TDP_ELEMENT_NODE = 1,
    TDP_ATTRIBUTE_NODE = 2,
    TDP_TEXT_NODE = 3,
    TDP_CDATA_SECTION_NODE = 4,
    TDP_ENTITY_REFERENCE_NODE = 5, 
    TDP_ENTITY_NODE = 6, 
    TDP_PROCESSING_INSTRUCTION_NODE = 7,
    TDP_COMMENT_NODE = 8,
    TDP_DOCUMENT_NODE = 9,
    TDP_DOCUMENT_TYPE_NODE = 10,
    TDP_DOCUMENT_FRAGMENT_NODE = 11,
    TDP_NOTATION_NODE = 12
} TdpNodeType;

/*
 * DOMImplementation Methods
 */
/*
 * NB: Tdp_CreateDocument not currently compliant with the DOM level 2 spec.
 */
EXTERN Tdp_Document Tdp_CreateDocument(Tcl_Interp *interp);
EXTERN Tdp_DocumentType Tdp_CreateDocumentType(Tcl_Interp *interp,
	Tdp_Document document, char *name, char *publicId, char *systemId);

/*
 * Document Methods
 */
EXTERN TdpDomError Tdp_CreateElement(Tcl_Interp *interp, Tdp_Document document, 
        char *tag, Tdp_Element *element);
EXTERN Tdp_TextNode Tdp_CreateTextNode(Tcl_Interp *interp, 
        Tdp_Document document, char *data);
EXTERN Tdp_CommentNode Tdp_CreateCommentNode(Tcl_Interp *interp, 
        Tdp_Document document, char *data);
EXTERN Tdp_Element Tdp_GetDocumentElement(Tdp_Document document);
EXTERN Tdp_PINode Tdp_CreateProcessingInstructionNode(Tcl_Interp *interp, 
        Tdp_Document document, char *target, char *data);

/*
 * Node Methods
 */
EXTERN TdpDomError Tdp_AppendChild(Tcl_Interp *interp,
        Tdp_Node parent, Tdp_Node newChild);
EXTERN Tdp_Node Tdp_GetParentNode(Tdp_Node node);
EXTERN Tdp_Node Tdp_GetLastChild(Tdp_Node node);
EXTERN TdpNodeType Tdp_GetNodeType(Tdp_Node node);
EXTERN char * Tdp_GetNodeValue(Tdp_Node node);
EXTERN TdpDomError Tdp_SetNodeValue(Tdp_Node node, char *data);

/*
 * Element Methods
 */
EXTERN TdpDomError Tdp_SetAttribute(Tcl_Interp *interp, Tdp_Element element,
    char *name, char *value);

/*
 * TclDomPro Extensions
 */
EXTERN Tcl_Obj* Tdp_GetDocumentObj(Tcl_Interp *interp, 
        Tdp_Document document);
EXTERN void Tdp_SetStartLocation(Tdp_Node node, unsigned int line,
    unsigned int column, unsigned int width, unsigned int closeLine, 
    unsigned int closeColumn);
EXTERN void Tdp_SetEndLocation(Tdp_Node node, unsigned int line,
    unsigned int column, unsigned int width, unsigned int closeLine, 
    unsigned int closeColumn);
EXTERN void Tdp_GetStartLocation(Tdp_Node node, unsigned int* linePtr,
    unsigned int* columnPtr, unsigned int* widthPtr);
EXTERN void Tdp_GetEndLocation(Tdp_Node node, unsigned int* linePtr,
    unsigned int* columnPtr, unsigned int* widthPtr);
EXTERN void Tdp_SetDocumentType(Tcl_Interp *interp, Tdp_Document document, Tdp_DocumentType
	documentType);

#  undef TCL_STORAGE_CLASS
#  define TCL_STORAGE_CLASS DLLIMPORT


#endif /* _TDPH */
