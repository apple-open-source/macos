/*
 * dom.c --
 *
 * Dom-like interface to support building DOM documents.
 * 
 * Copyright (c) 1999, 2000 Ajuba Solutions
 *
 * RCS: $Id: dom.c,v 1.3 2001/09/20 18:28:48 jenglish Exp $
 *
 */

#include "tdp.h"
#include "tclDomProInt.h"

/*
 * DOMImplemenation Methods
 */
Tdp_DocumentType Tdp_CreateDocumentType(Tcl_Interp *interp, 
    Tdp_Document parentDocument, char *name, char *publicId, char *systemId)
{
    TclDomInterpData *interpDataPtr;
    interpDataPtr = (TclDomInterpData *) Tcl_GetAssocData(interp, 
	    PACKAGE_NAME, NULL);
    return ((Tdp_DocumentType) TclDomCreateDocType(interp, interpDataPtr,
	    (TclDomDocument *) parentDocument, name, publicId, systemId));
}

/*
 * Document Methods
 */

Tdp_Document Tdp_CreateDocument(Tcl_Interp *interp)
{
    TclDomInterpData *interpDataPtr;
    interpDataPtr = (TclDomInterpData *) Tcl_GetAssocData(interp, 
	    PACKAGE_NAME, NULL);
    return ((Tdp_Document) TclDomEmptyDocument(interp, interpDataPtr));
}

TdpDomError Tdp_CreateElement(Tcl_Interp *interp, Tdp_Document parentDocument,
	char *tagname, Tdp_Element *elementPtr)
{
    TclDomInterpData *interpDataPtr;
    TclDomDocument *documentPtr;
    interpDataPtr = (TclDomInterpData *) Tcl_GetAssocData(interp, 
	    PACKAGE_NAME, NULL);
    documentPtr = (TclDomDocument *) parentDocument;
    /* XXX should check for valid tag in TclDomCreateElement */
    *elementPtr = (Tdp_Element) TclDomCreateElement(interp, interpDataPtr, 
	    documentPtr, tagname);
    return TDP_OK;
}

Tdp_TextNode Tdp_CreateTextNode(Tcl_Interp *interp,
    Tdp_Document parentDocument, char *data)
{
    TclDomInterpData *interpDataPtr;
    interpDataPtr = (TclDomInterpData *) Tcl_GetAssocData(interp, 
	    PACKAGE_NAME, NULL);
    return ((Tdp_TextNode) TclDomCreateTextNode(interp, interpDataPtr,
	    (TclDomDocument *) parentDocument, data));
}

Tdp_CommentNode Tdp_CreateCommentNode(Tcl_Interp *interp,
    Tdp_Document parentDocument, char *data)
{
    TclDomInterpData *interpDataPtr;
    interpDataPtr = (TclDomInterpData *) Tcl_GetAssocData(interp, 
	    PACKAGE_NAME, NULL);
    return ((Tdp_CommentNode) TclDomCreateCommentNode(interp, interpDataPtr,
	    (TclDomDocument *) parentDocument, data));
}

Tdp_PINode Tdp_CreateProcessingInstructionNode(Tcl_Interp *interp,
    Tdp_Document parentDocument, char *target, char *data)
{
    TclDomInterpData *interpDataPtr;
    interpDataPtr = (TclDomInterpData *) Tcl_GetAssocData(interp, 
	    PACKAGE_NAME, NULL);
    return ((Tdp_PINode) TclDomCreateProcessingInstructionNode(interp, 
	    interpDataPtr, (TclDomDocument *) parentDocument, target, data));
}

Tdp_Element Tdp_GetDocumentElement(Tdp_Document document) 
{
    TclDomDocument *documentPtr = (TclDomDocument *) document;
	return (Tdp_Element) documentPtr->selfPtr;
}

/*
 * Node Methods
 */

Tdp_Node Tdp_GetParentNode(Tdp_Node node) 
{
    TclDomNode *nodePtr = (TclDomNode *) node;
    if (nodePtr->nodeType != ELEMENT_NODE) {
	    return NULL;
    }
    return (Tdp_Node) nodePtr->parentNodePtr;
}

Tdp_Node Tdp_GetLastChild(Tdp_Node node) 
{
    TclDomNode *nodePtr = (TclDomNode *) node;
    if (nodePtr->nodeType != ELEMENT_NODE) {
	    return NULL;
    }
    return (Tdp_Node) nodePtr->lastChildPtr;
}

TdpNodeType Tdp_GetNodeType(Tdp_Node node)
{
    return (((TclDomNode *) node)->nodeType);
}

char * Tdp_GetNodeValue(Tdp_Node node)
{
    if (((TclDomNode *) node)->nodeValue) {
	return ((TclDomNode *) node)->nodeValue;
    } else {
	return "";
    }
}

TdpDomError Tdp_SetNodeValue(Tdp_Node node, char *data)
{
    return TclDomSetNodeValue((TclDomNode *) node, data);
}

TdpDomError Tdp_AppendChild(Tcl_Interp *interp, Tdp_Node parent,
        Tdp_Node newChild)
{
    int result;
    TclDomInterpData *interpDataPtr;
    TclDomNode *parentPtr;
    TclDomNode *newChildPtr;
    interpDataPtr = (TclDomInterpData *) Tcl_GetAssocData(interp, 
	    PACKAGE_NAME, NULL);
    parentPtr = (TclDomNode *) parent;
    newChildPtr = (TclDomNode *) newChild;
    result = TclDomAppendChild(interp, interpDataPtr, parentPtr,
	newChildPtr);
    if (result == TCL_OK) {
	    return TDP_OK;
    } else {
	    return TDP_HIERARCHY_REQUEST_ERR;
    }
}

/*
 * Element Methods
 */

TdpDomError Tdp_SetAttribute(Tcl_Interp *interp, Tdp_Element element,
    char *name, char *value)
{
    TclDomInterpData *interpDataPtr;
    TdpDomError domError;
    interpDataPtr = (TclDomInterpData *) Tcl_GetAssocData(interp, 
	    PACKAGE_NAME, NULL);

    domError = TclDomSetAttribute(interp, interpDataPtr, (TclDomNode *)
	element, name, value);
    return domError;
}

/*
 * TclDomPro Extension Methods
 */

Tcl_Obj* Tdp_GetDocumentObj(Tcl_Interp *interp, Tdp_Document document)
{
    Tcl_Obj *docObjPtr;
    TclDomInterpData *interpDataPtr;
    TclDomDocument *documentPtr;
    interpDataPtr = (TclDomInterpData *) Tcl_GetAssocData(interp, 
	    PACKAGE_NAME, NULL);
    documentPtr = (TclDomDocument *) document;
    docObjPtr = TclDomGetNodeObj(interpDataPtr, documentPtr->selfPtr);
    return docObjPtr;
}

void Tdp_SetStartLocation(Tdp_Node node, 
    unsigned int line,
    unsigned int column, 
    unsigned int width, 
    unsigned int endLine, 
    unsigned int endColumn)
{
    TclDomNode *nodePtr = (TclDomNode *) node;
    nodePtr->startLine = line;
    nodePtr->startColumn = column;
    nodePtr->startWidth = width;
    nodePtr->startLineClose = endLine;
    nodePtr->startColumnClose = endColumn;
}

void Tdp_SetEndLocation(Tdp_Node node, 
    unsigned int line,
    unsigned int column, 
    unsigned int width,
    unsigned int endLine,
    unsigned int endColumn)
{
    TclDomNode *nodePtr = (TclDomNode *) node;
    nodePtr->endLine = line;
    nodePtr->endColumn = column;
    nodePtr->endWidth = width;
    nodePtr->endLineClose = endLine;
    nodePtr->endColumnClose = endColumn;
}

void Tdp_GetStartLocation(Tdp_Node node, unsigned int* linePtr,
    unsigned int* columnPtr, unsigned int* widthPtr)
{
    TclDomNode *nodePtr = (TclDomNode *) node;
    *linePtr = nodePtr->startLine;
    *columnPtr = nodePtr->startColumn;
    *widthPtr = nodePtr->startWidth;
}

void Tdp_GetEndLocation(Tdp_Node node, unsigned int* linePtr,
    unsigned int* columnPtr, unsigned int* widthPtr)
{
    TclDomNode *nodePtr = (TclDomNode *) node;
    *linePtr = nodePtr->endLine;
    *columnPtr = nodePtr->endColumn;
    *widthPtr = nodePtr->endWidth;
}

void Tdp_SetDocumentType(Tcl_Interp *interp, Tdp_Document document, 
	Tdp_DocumentType docType)
{
    TclDomInterpData *interpDataPtr;
    TclDomDocument *documentPtr;
    TclDomDocTypeNode *docTypePtr;
    int result;

    interpDataPtr = (TclDomInterpData *) Tcl_GetAssocData(interp, 
	    PACKAGE_NAME, NULL);
    documentPtr = (TclDomDocument *) document;
    docTypePtr = (TclDomDocTypeNode *) docType;

    result = TclDomAppendChild(interp, interpDataPtr, 
	    (TclDomNode *) documentPtr, (TclDomNode *) docTypePtr);
}
