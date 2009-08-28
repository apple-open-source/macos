/*---------------------------------------------------------------------------
|   Copyright (C) 1999  Jochen C. Loewer (loewerj@hotmail.com)
+----------------------------------------------------------------------------
|
|   $Id: dom.c,v 1.107 2007/12/25 23:18:41 rolf Exp $
|
|
|   A DOM interface upon the expat XML parser for the C language
|   according to the W3C recommendation REC-DOM-Level-1-19981001
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
|
|       June00  Zoran Vasiljevic  Made thread-safe.
|
|           01  Rolf Ade          baseURI stuff, ID support, external
|                                 entities, tdom command
|
|
|   written by Jochen Loewer
|   April 5, 1999
|
\--------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------
|   Includes
|
\--------------------------------------------------------------------------*/
#include <tcl.h>
#include <stdlib.h>
#include <string.h>
#include <dom.h>
#include <domxpath.h>
#include <utf8conv.h>
#include <tclexpat.h>



/*---------------------------------------------------------------------------
|   Defines
|
\--------------------------------------------------------------------------*/
#define DBG(x)
#define TDOM_NS
#define XSLT_NAMESPACE  "http://www.w3.org/1999/XSL/Transform"

#define MutationEvent()
#define MutationEvent2(type,node)
#define MutationEvent3(type,node,relatioNode)

#define MCHK(a)  if ((a)==NULL) { \
                     fprintf(stderr, \
                            "Memory alloc error line: %d",__LINE__); \
                     exit(1); \
                 }

#define INITIAL_BASEURISTACK_SIZE 4;

/*---------------------------------------------------------------------------
|   Globals
|   In threading environment, some are located in domDocument structure
|   and some are handled differently (domUniqueNodeNr, domUniqueDocNr)
|
\--------------------------------------------------------------------------*/

#ifndef TCL_THREADS
  unsigned long domUniqueNodeNr = 0;
  unsigned long domUniqueDocNr  = 0;
  Tcl_HashTable tdom_tagNames;
  Tcl_HashTable tdom_attrNames;
#endif

static int domModuleIsInitialized = 0;
TDomThreaded(static Tcl_Mutex initMutex;)

static char *domException2StringTable [] = {

    "OK - no expection",
    "INDEX_SIZE_ERR",
    "DOMSTRING_SIZE_ERR",
    "HIERARCHY_REQUEST_ERR",
    "WRONG_DOCUMENT_ERR",
    "INVALID_CHARACTER_ERR",
    "NO_DATA_ALLOWED_ERR",
    "NO_MODIFICATION_ALLOWED_ERR",
    "NOT_FOUND_ERR",
    "NOT_SUPPORTED_ERR",
    "INUSE_ATTRIBUTE_ERR"
};

static char tdom_usage[] =
                "Usage tdom <expat parser obj> <subCommand>, where subCommand can be:\n"
                "           enable             \n"
                "           getdoc             \n"
                "           setResultEncoding  \n"
                "           setStoreLineColumn \n"
                ;


/*---------------------------------------------------------------------------
|   type domActiveNS
|
\--------------------------------------------------------------------------*/
typedef struct _domActiveNS {

    int    depth;
    domNS *namespace;

} domActiveNS;

/*---------------------------------------------------------------------------
|   type domBaseURIstackElem
|
\--------------------------------------------------------------------------*/
typedef struct _domActiveBaseURI {

    int   depth;
    const char *baseURI;

} domActiveBaseURI;

/*---------------------------------------------------------------------------
|   type domReadInfo
|
\--------------------------------------------------------------------------*/
typedef struct _domReadInfo {

    XML_Parser        parser;
    domDocument      *document;
    domNode          *currentNode;
    int               depth;
    int               ignoreWhiteSpaces;
    Tcl_DString      *cdata;
    TEncoding        *encoding_8bit;
    int               storeLineColumn;
    int               feedbackAfter;
    int               lastFeedbackPosition;
    Tcl_Interp       *interp;
    int               activeNSsize;
    int               activeNSpos;
    domActiveNS      *activeNS;
    int               baseURIstackSize;
    int               baseURIstackPos;
    domActiveBaseURI *baseURIstack;
    int               insideDTD;

} domReadInfo;

/*----------------------------------------------------------------------------
|   Prototypes
|
\---------------------------------------------------------------------------*/
static void DispatchPCDATA (domReadInfo *info);


#ifndef TCL_THREADS

/*---------------------------------------------------------------------------
|   domModuleFinalize
|
\--------------------------------------------------------------------------*/
static void
domModuleFinalize(ClientData unused)
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;

    entryPtr = Tcl_FirstHashEntry(&tdom_tagNames, &search);
    while (entryPtr) {
        Tcl_DeleteHashEntry(entryPtr);
        entryPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&tdom_tagNames);

    entryPtr = Tcl_FirstHashEntry(&tdom_attrNames, &search);
    while (entryPtr) {
        Tcl_DeleteHashEntry(entryPtr);
        entryPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&tdom_attrNames);

    return;
}
#endif /* TCL_THREADS */

/*---------------------------------------------------------------------------
|   domModuleInitialize
|
\--------------------------------------------------------------------------*/
void
domModuleInitialize (
)
{
    if (domModuleIsInitialized == 0) {
        TDomThreaded(Tcl_MutexLock(&initMutex);)
        if (domModuleIsInitialized == 0) {
            domAllocInit();
            TDomNotThreaded (
                Tcl_InitHashTable(&tdom_tagNames, TCL_STRING_KEYS);
                Tcl_InitHashTable(&tdom_attrNames, TCL_STRING_KEYS);
                Tcl_CreateExitHandler(domModuleFinalize, NULL);
            )
            TDomThreaded(
                Tcl_CreateExitHandler(domLocksFinalize, NULL);
            )
            domModuleIsInitialized = 1;
        }
        TDomThreaded(Tcl_MutexUnlock(&initMutex);)
    }
}

/*---------------------------------------------------------------------------
|   coercion routines for calling from C++
|
\--------------------------------------------------------------------------*/
domAttrNode * coerceToAttrNode( domNode *n )  {
    return (domAttrNode *)n;
}

domTextNode * coerceToTextNode( domNode *n ) {
    return (domTextNode *)n;
}

domProcessingInstructionNode * coerceToProcessingInstructionNode( domNode *n ) {
    return (domProcessingInstructionNode *)n;
}

/*---------------------------------------------------------------------------
|   domIsNAME
|
\--------------------------------------------------------------------------*/
int
domIsNAME (
    const char *name
    )
{
    const char *p;

    p = name;
    if (!isNameStart(p)) return 0;
    p += UTF8_CHAR_LEN(*p);
    while (*p) {
        if (isNameChar(p))
            p += UTF8_CHAR_LEN(*p);
        else return 0;
    }
    return 1;
}

/*---------------------------------------------------------------------------
|   domIsPINAME
|
\--------------------------------------------------------------------------*/
int
domIsPINAME (
    const char *name
    )
{
    if (strlen (name) == 3
        && ((name[0] == 'x') || (name[0] == 'X'))
        && ((name[1] == 'm') || (name[1] == 'M'))
        && ((name[2] == 'l') || (name[2] == 'L')) ) {
        return 0;
    }
    return domIsNAME (name);
}

/*---------------------------------------------------------------------------
|   domIsQNAME
|
\--------------------------------------------------------------------------*/
int
domIsQNAME (
    const char *name
    )
{
    const char *p;
    
    p = name;
    if (!isNCNameStart(p)) return 0;
    p += UTF8_CHAR_LEN(*p);
    while (*p) {
        if (isNCNameChar(p))
            p += UTF8_CHAR_LEN(*p);
        else {
            if (*p == ':') {
                p += 1;
                if (!isNCNameStart(p)) return 0;
                p += UTF8_CHAR_LEN(*p);
                break;
            }
            else return 0;
        }
    }
    while (*p) {
        if (isNCNameChar(p))
            p += UTF8_CHAR_LEN(*p);
        else return 0;
    }
    return 1;
}

/*---------------------------------------------------------------------------
|   domIsNCNAME
|
\--------------------------------------------------------------------------*/
int
domIsNCNAME (
    const char *name
    )
{
    const char *p;

    p = name;
    if (!isNCNameStart(p)) return 0;
    p += UTF8_CHAR_LEN(*p);
    while (*p) {
        if (isNCNameChar(p))
            p += UTF8_CHAR_LEN(*p);
        else return 0;
    }
    return 1;
}

/*---------------------------------------------------------------------------
|   domIsChar 
|
\--------------------------------------------------------------------------*/
int
domIsChar (
    const char *str
    )
{
    const char *p;
    int   clen;
    
    p = str;
    while (*p) {
        clen = UTF8_CHAR_LEN(*p);
        if (UTF8_XMLCHAR((unsigned const char *)p,clen))
            p += clen;
        else return 0;
    }
    return 1;
}

/*---------------------------------------------------------------------------
|   domIsComment
|
\--------------------------------------------------------------------------*/
int
domIsComment (
    const char *str
    )
{
    const char *p;
    int   len, i = 0;
    
    p = str;
    len = strlen (str);
    while (i < len) {
        if (*p == '-') {
            if (i == len - 1) return 0;
            p++; i++;
            if (*p == '-') return 0;
        }
        p++; i++;
    }
    return domIsChar (str);
}

/*---------------------------------------------------------------------------
|   domIsCDATA
|
\--------------------------------------------------------------------------*/
int
domIsCDATA (
    const char *str
    )
{
    const char *p;
    int   len, i = 0;

    p = str;
    len = strlen (str);
    while (i < len - 2) {
        if (  *p == ']'
            && p[1] == ']'
            && p[2] == '>') return 0;
        p++; i++;
    }
    return domIsChar (str);
}

/*---------------------------------------------------------------------------
|   domIsPIValue
|
\--------------------------------------------------------------------------*/
int
domIsPIValue (
    const char *str
    )
{
    const char *p;
    int   len, i = 0;

    p = str;
    len = strlen (str);
    while (i < len - 1) {
        if (*p == '?' && p[1] == '>') return 0;
        p++; i++;
    }
    return domIsChar (str);
}

/*---------------------------------------------------------------------------
|   domLookupNamespace
|
\--------------------------------------------------------------------------*/
domNS *
domLookupNamespace (
    domDocument *doc,
    const char  *prefix,
    const char  *namespaceURI
)
{
    domNS *ns;
    int i;

    if (prefix==NULL) return NULL;
    for (i = 0; i <= doc->nsptr; i++) {
        ns = doc->namespaces[i];
        if (   (ns->prefix != NULL)
            && (strcmp(prefix,ns->prefix)==0)
            && (strcmp(namespaceURI, ns->uri)==0)
        ) {
            return ns;
        }
    }
    return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * domPrecedes --
 *
 *	This helper procedure returns if node precedes other with regard
 *      to their position in the document and according to the document
 *      order. The two nodes could be out of the two documents. Both
 *      nodes must not be out of the fragments list.
 *
 * Results:
 *	1 if node precedes other in document order, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
domPrecedes (
    domNode *node,
    domNode *other
    )
{
    domNode *nodeAncestor, *otherAncestor, *otherToplevel;
    domAttrNode *attrN, *attrO;
    
    if (node == other) {
        return 0;
    }
    
    if (node->nodeType == ATTRIBUTE_NODE) {
        attrN = (domAttrNode*)node;
        if (other->nodeType == ATTRIBUTE_NODE) {
            attrO = (domAttrNode*)other;
            if (attrN->parentNode == attrO->parentNode) {
                attrN = attrN->nextSibling;
                while (attrN) {
                    if (attrN == attrO) {
                        return 1;
                    }
                    attrN = attrN->nextSibling;
                }
                return 0;
            } else {
                node = attrN->parentNode;
                other = attrO->parentNode;
            }
        } else {
            if (attrN->parentNode == other) {
                return 0;
            } else {
                node = attrN->parentNode;                
            }
        }
    }
    if (other->nodeType == ATTRIBUTE_NODE) {
        attrO = (domAttrNode*)other;
        if (node == attrO->parentNode) {
            return 1;
        } else {
            other = attrO->parentNode;
        }
    }
    
    if (node->ownerDocument != other->ownerDocument) {
        /* For mt tdom, this does not, what it should:
           whatever relative order two nodes out of different
           documents ever have (that is not determined by the rec) it
           must return always the same order (that is required by the 
           rec). */
        return (node->ownerDocument->documentNumber < 
                other->ownerDocument->documentNumber);
    }

#ifndef TCL_THREADS
    if (node->ownerDocument->nodeFlags & NEEDS_RENUMBERING) {
        domRenumberTree (node->ownerDocument->rootNode);
        node->ownerDocument->nodeFlags &= ~NEEDS_RENUMBERING;
    }
    return (node->nodeNumber < other->nodeNumber);
# else 
    if (!(node->ownerDocument->nodeFlags & NEEDS_RENUMBERING)) {
        return (node->nodeNumber < other->nodeNumber);
    }
#endif
    
    otherAncestor = other;
    while (otherAncestor->parentNode) {
        otherAncestor = otherAncestor->parentNode;
        if (otherAncestor == node) {
            return 1;
        }
    }
    otherToplevel = otherAncestor;
    
    nodeAncestor = node;
    while (nodeAncestor->parentNode) {
        otherAncestor = other;
        while (otherAncestor->parentNode) {
            if (nodeAncestor->parentNode == otherAncestor->parentNode) {
                nodeAncestor = nodeAncestor->nextSibling;
                while (nodeAncestor) {
                    if (nodeAncestor == otherAncestor) {
                        return 1;
                    }
                    nodeAncestor = nodeAncestor->nextSibling;
                }
                return 0;
            }
            otherAncestor = otherAncestor->parentNode;
        }
        nodeAncestor = nodeAncestor->parentNode;
        if (nodeAncestor == other) {
            return 0;
        }
    }
    nodeAncestor = nodeAncestor->nextSibling;
    while (nodeAncestor) {
        if (nodeAncestor == otherAncestor) {
            return 1;
        }
        nodeAncestor = nodeAncestor->nextSibling;
    }
    if (node == node->ownerDocument->rootNode) {
        return 1;
    }
    return 0;
}

/*---------------------------------------------------------------------------
|   domRenumberTree
|
\--------------------------------------------------------------------------*/
void
domRenumberTree (
    domNode *node
)
{
    while (node) {
        node->nodeNumber = NODE_NO(node->ownerDocument);
        if (node->nodeType == ELEMENT_NODE) {
            domRenumberTree (node->firstChild);
        }
        node = node->nextSibling;
    }
}

/*---------------------------------------------------------------------------
|   domLookupPrefixWithMappings
|
\--------------------------------------------------------------------------*/
const char *
domLookupPrefixWithMappings (
    domNode    *node,
    const char *prefix,
    char      **prefixMappings 
    )
{
    int    i;
    domNS *ns;
    
    if (prefixMappings) {
        i = 0;
        while (prefixMappings[i]) {
            if (strcmp (prefix, prefixMappings[i]) == 0) {
                return prefixMappings[i+1];
            }
            i += 2;
        }
    }
    ns = domLookupPrefix (node, prefix);
    if (ns) return ns->uri;
    else    return NULL;
}

/*---------------------------------------------------------------------------
|   domLookupPrefix
|
\--------------------------------------------------------------------------*/
domNS *
domLookupPrefix (
    domNode *node,
    const char    *prefix
    )
{
    domAttrNode   *NSattr;
    domNode       *orgNode = node;
    int            found;

    found = 0;
    while (node) {
        if (node->firstAttr && !(node->firstAttr->nodeFlags & IS_NS_NODE)) {
            node = node->parentNode;
            continue;
        }
        NSattr = node->firstAttr;
        while (NSattr && (NSattr->nodeFlags & IS_NS_NODE)) {
            if (prefix[0] == '\0') {
                if (NSattr->nodeName[5] == '\0') {
                    found = 1;
                    break;
                }
            } else {
                if (NSattr->nodeName[5] != '\0'
                    && strcmp (&NSattr->nodeName[6], prefix)==0) {
                    found = 1;
                    break;
                }
            }
            NSattr = NSattr->nextSibling;
        }
        if (found) {
            return domGetNamespaceByIndex (node->ownerDocument,
                                           NSattr->namespace);
        }
        node = node->parentNode;
    }
    if (prefix && (strcmp (prefix, "xml")==0)) {
        NSattr = orgNode->ownerDocument->rootNode->firstAttr;
        return domGetNamespaceByIndex (orgNode->ownerDocument,
                                       NSattr->namespace);
    }
    return NULL;
}

/*---------------------------------------------------------------------------
|   domIsNamespaceInScope
|
\--------------------------------------------------------------------------*/
static int
domIsNamespaceInScope (
    domActiveNS *NSstack,
    int          NSstackPos,
    const char  *prefix,
    const char  *namespaceURI
)
{
    int    i;

    for (i = NSstackPos; i >= 0; i--) {
        if (NSstack[i].namespace->prefix[0] &&
            (strcmp(NSstack[i].namespace->prefix, prefix)==0)) {
            if (strcmp(NSstack[i].namespace->uri, namespaceURI)==0) {
                /* OK, exactly the same namespace declaration is in scope */
                return 1;
            } else {
                /* This prefix is currently assigned to another uri,
                   we need a new NS declaration, to override this one */
                return 0;
            }
        }
    }
    return 0;
}

/*---------------------------------------------------------------------------
|   domLookupURI
|
\--------------------------------------------------------------------------*/
domNS *
domLookupURI (
    domNode *node,
    char        *uri
    )
{
    domAttrNode   *NSattr;
    int            found, alreadyHaveDefault;

    found = 0;
    alreadyHaveDefault = 0;
    while (node) {
        if (node->firstAttr && !(node->firstAttr->nodeFlags & IS_NS_NODE)) {
            node = node->parentNode;
            continue;
        }
        NSattr = node->firstAttr;
        while (NSattr && (NSattr->nodeFlags & IS_NS_NODE)) {
            if (NSattr->nodeName[5] == '\0') {
                if (!alreadyHaveDefault) {
                    if (strcmp (NSattr->nodeValue, uri)==0) {
                        found = 1;
                        break;
                    } else {
                        alreadyHaveDefault = 1;
                    }
                }
            } else {
                if (strcmp (NSattr->nodeValue, uri)==0) {
                    found = 1;
                    break;
                }
            }
            NSattr = NSattr->nextSibling;
        }
        if (found) {
            return domGetNamespaceByIndex (node->ownerDocument,
                                           NSattr->namespace);
        }
        node = node->parentNode;
    }
    return NULL;
}


/*---------------------------------------------------------------------------
|   domGetNamespaceByIndex
|
\--------------------------------------------------------------------------*/
domNS *
domGetNamespaceByIndex (
    domDocument *doc,
    int          nsIndex
)
{
    if (!nsIndex) return NULL;
    return doc->namespaces[nsIndex-1];
}


/*---------------------------------------------------------------------------
|   domNewNamespace
|
\--------------------------------------------------------------------------*/
domNS* domNewNamespace (
    domDocument *doc,
    const char  *prefix,
    const char  *namespaceURI
)
{
    domNS *ns = NULL;

    DBG(fprintf(stderr, "domNewNamespace '%s' --> '%s' \n", prefix, namespaceURI);)

    ns = domLookupNamespace (doc, prefix, namespaceURI);
    if (ns != NULL) return ns;
    doc->nsptr++;
    if (doc->nsptr > 254) {
        DBG(fprintf (stderr, "maximum number of namespaces exceeded!!!\n");)
        domPanic("domNewNamespace: maximum number of namespaces exceeded!");
    }
    if (doc->nsptr >= doc->nslen) {
        doc->namespaces = (domNS**) REALLOC ((char*) doc->namespaces,
                                             sizeof (domNS*) * 2 * doc->nslen);
        doc->nslen *= 2;
    }
    doc->namespaces[doc->nsptr] = (domNS*)MALLOC (sizeof (domNS));
    ns = doc->namespaces[doc->nsptr];


    if (prefix == NULL) {
        ns->prefix = tdomstrdup("");
    } else {
        ns->prefix = tdomstrdup(prefix);
    }
    if (namespaceURI == NULL) {
        ns->uri = tdomstrdup("");
    } else {
        ns->uri   = tdomstrdup(namespaceURI);
    }
    ns->index = doc->nsptr + 1;

    return ns;
}


/*---------------------------------------------------------------------------
|   domSplitQName  -  extract namespace prefix (if any)
|
\--------------------------------------------------------------------------*/
int
domSplitQName (
    const char  *name,
    char        *prefix,
    const char **localName
)
{
    const char  *s;
    char        *p, *prefixEnd;

    s = name;
    p = prefix;
    prefixEnd = &prefix[MAX_PREFIX_LEN-1];
    while (*s && (*s != ':'))  {
        if (p < prefixEnd) *p++ = *s;
        s++;
    }
    if (*s != ':') {
        *prefix    = '\0';
        *localName = name;
        return 0;
    }
    *p++ = '\0';
    *localName = ++s;
    DBG(fprintf(stderr, "domSplitName %s -> '%s' '%s'\n",
                         name, prefix, *localName);
    )
    return 1;
}


/*---------------------------------------------------------------------------
|   domNamespaceURI
|
\--------------------------------------------------------------------------*/
const char *
domNamespaceURI (
    domNode *node
)
{
    domAttrNode *attr;
    domNS       *ns;

    if (!node->namespace) return NULL;
    if (node->nodeType == ATTRIBUTE_NODE) {
        attr = (domAttrNode*)node;
        if (attr->nodeFlags & IS_NS_NODE) return NULL;
        ns = attr->parentNode->ownerDocument->namespaces[attr->namespace-1];
    } else
    if (node->nodeType == ELEMENT_NODE) {
        ns = node->ownerDocument->namespaces[node->namespace-1];
    } else {
        return NULL;
    }
    return ns->uri;
}


/*---------------------------------------------------------------------------
|   domNamespacePrefix
|
\--------------------------------------------------------------------------*/
const char *
domNamespacePrefix (
    domNode *node
)
{
    domAttrNode *attr;
    domNS *ns;

    if (!node->namespace) return NULL;
    if (node->nodeType == ATTRIBUTE_NODE) {
        attr = (domAttrNode*)node;
        ns = attr->parentNode->ownerDocument->namespaces[attr->namespace-1];
    } else
    if (node->nodeType == ELEMENT_NODE) {
        ns = node->ownerDocument->namespaces[node->namespace-1];
    } else {
        return NULL;
    }
    if (ns) return ns->prefix;
    return NULL;
}


/*---------------------------------------------------------------------------
|   domGetLocalName
|
\--------------------------------------------------------------------------*/
const char *
domGetLocalName (
    const char *nodeName
)
{
    char prefix[MAX_PREFIX_LEN];
    const char *localName;

    domSplitQName (nodeName, prefix, &localName);
    return localName;
}

/*
 *----------------------------------------------------------------------
 *
 * domGetAttributeNodeNS --
 *
 *      Search a given node for an attribute with namespace "uri" and
 *      localname "localname".
 *
 * Results:
 *      Returns a pointer to the attribute, if there is one with the
 *      given namespace and localname. Otherwise returns NULL.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

domAttrNode *
domGetAttributeNodeNS (
    domNode    *node,         /* The attributes of this node are searched for a
                                 matching attribute; the node must exist */
    const char *uri,          /* The namespace of the demanded attribute */
    const char *localname     /* The localname of the demanded attribute */
    )
{
    domAttrNode *attr;
    domNS       *ns;
    int          noNS;
    char         prefix[MAX_PREFIX_LEN];
    const char  *attrLocalName;

    if (uri[0] == '\0') noNS = 1;
    else                noNS = 0;

    attr = node->firstAttr;
    while (attr) {
        if (noNS) {
            if (!attr->namespace 
                && strcmp (attr->nodeName, localname) == 0) {
                return attr;
                
            }
        } else {
            if (attr->namespace) {
                domSplitQName (attr->nodeName, prefix, &attrLocalName);
                if (strcmp (localname, attrLocalName) == 0) {
                    ns = domGetNamespaceByIndex (node->ownerDocument,
                                                 attr->namespace);
                    if (strcmp (ns->uri, uri) == 0) {
                        return attr;
                    }
                }
            }
        }
        attr = attr->nextSibling;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * domPreviousSibling --
 *
 *      Returns the previous node to the given node or NULL, if there
 *      is no previous node. This function is needed in situations,
 *      where the given node may also be an domAttrNode. Namespace
 *      declaring attributes are treated as any other
 *      attributes. Since the domAttrNode struct doesn't has an
 *      element for the previous attribute, we need a function for the
 *      relatively rare cases, the 'previous attribute' is
 *      needed. Remeber, that the XML rec say, that there is no
 *      specific order of the attributes of a node.
 *
 * Results: 
 *      A pointer to the previous node of the given one
 *      or NULL, if there isn't a previous node.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

domNode *
domPreviousSibling (
    domNode *node      /* The reference attribute */
    )
{
    domAttrNode *attr, *attr1;
    
    if (node->nodeType != ATTRIBUTE_NODE) {
        return node->previousSibling;
    }

    attr = (domAttrNode*) node;
    if (attr->parentNode->firstAttr == attr) {
        return NULL;
    }
    attr1 = attr->parentNode->firstAttr;
    while (attr1) {
        if (attr1->nextSibling == attr) {
            return (domNode*)attr1;
        }
        attr1 = attr1->nextSibling;
    }
    /* Not reached */
    return NULL;
}

#ifndef  TDOM_NO_EXPAT


/*---------------------------------------------------------------------------
|   startElement
|
\--------------------------------------------------------------------------*/
static void
startElement(
    void         *userData,
    const char   *name,
    const char  **atts
)
{
    domReadInfo   *info = userData;
    domNode       *node, *parentNode;
    domLineColumn *lc;
    domAttrNode   *attrnode, *lastAttr;
    const char   **atPtr, **idAttPtr;
    Tcl_HashEntry *h;
    int            hnew, len, pos, idatt, newNS;
    const char    *xmlns, *localname;
    char           tagPrefix[MAX_PREFIX_LEN];
    char           prefix[MAX_PREFIX_LEN];
    domNS         *ns;
    char           feedbackCmd[24];

    if (info->feedbackAfter) {

        if (info->lastFeedbackPosition
             < XML_GetCurrentByteIndex (info->parser)
        ) {
            sprintf(feedbackCmd, "%s", "::dom::domParseFeedback");
            if (Tcl_Eval(info->interp, feedbackCmd) != TCL_OK) {
                DBG(fprintf(stderr, "%s\n", 
                            Tcl_GetStringResult (info->interp));)
                /* FIXME: We simply ignore script errors in the
                   feedbackCmd, for now. One fine day, expat may provide
                   a way to cancel an already started parse run from
                   inside a handler. Then we should revisit this. */
                /* exit(1) */    
            }
            info->lastFeedbackPosition += info->feedbackAfter;
        }
    }

    DispatchPCDATA (info);
    
    h = Tcl_CreateHashEntry(&HASHTAB(info->document,tdom_tagNames), name,
                            &hnew);
    if (info->storeLineColumn) {
        node = (domNode*) domAlloc(sizeof(domNode)
                                    + sizeof(domLineColumn));
    } else {
        node = (domNode*) domAlloc(sizeof(domNode));
    }
    memset(node, 0, sizeof(domNode));
    node->nodeType      = ELEMENT_NODE;
    node->nodeFlags     = 0;
    node->namespace     = 0;
    node->nodeName      = (char *)&(h->key);
    node->nodeNumber    = NODE_NO(info->document);
    node->ownerDocument = info->document;

    if (info->baseURIstack[info->baseURIstackPos].baseURI 
        != XML_GetBase (info->parser)) {
        h = Tcl_CreateHashEntry (info->document->baseURIs,
                                 (char*) node,
                                 &hnew);
        Tcl_SetHashValue (h, tdomstrdup (XML_GetBase (info->parser)));
        node->nodeFlags |= HAS_BASEURI;
        info->baseURIstackPos++;
        if (info->baseURIstackPos >= info->baseURIstackSize) {
            info->baseURIstack = (domActiveBaseURI*) REALLOC(
                (char*)info->baseURIstack,
                sizeof(domActiveBaseURI) * 2 * info->baseURIstackSize);
            info->baseURIstackSize = 2 * info->baseURIstackSize;
        }
        info->baseURIstack[info->baseURIstackPos].baseURI
            = XML_GetBase (info->parser);
        info->baseURIstack[info->baseURIstackPos].depth 
            = info->depth;
    }

    if (info->depth == 0) {
        if (info->document->rootNode->lastChild) {
            info->document->rootNode->lastChild->nextSibling = node;
            node->previousSibling = info->document->rootNode->lastChild;
        } else {
            info->document->rootNode->firstChild = node;
        }
        info->document->rootNode->lastChild = node;
    } else {
        parentNode = info->currentNode;
        node->parentNode = parentNode;
        if (parentNode->firstChild)  {
            parentNode->lastChild->nextSibling = node;
            node->previousSibling = parentNode->lastChild;
            parentNode->lastChild = node;
        } else {
            parentNode->firstChild = parentNode->lastChild = node;
        }
    }
    info->currentNode = node;
    if (info->storeLineColumn) {
        lc = (domLineColumn*) ( ((char*)node) + sizeof(domNode));
        node->nodeFlags |= HAS_LINE_COLUMN;
        lc->line         = XML_GetCurrentLineNumber(info->parser);
        lc->column       = XML_GetCurrentColumnNumber(info->parser);
    }


    lastAttr = NULL;
    /*--------------------------------------------------------------
    |   process namespace declarations
    |
    \-------------------------------------------------------------*/
#ifdef TDOM_NS
    for (atPtr = atts; atPtr[0] && atPtr[1]; atPtr += 2) {

        if (strncmp(atPtr[0], "xmlns", 5) == 0) {
            xmlns = atPtr[0];
            newNS = 1;
            if (xmlns[5] == ':') {
                if (domIsNamespaceInScope (info->activeNS, info->activeNSpos,
                                           &(xmlns[6]), atPtr[1])) {
                    ns = domLookupPrefix (info->currentNode, &(xmlns[6]));
                    newNS = 0;
                }
                else {
                    ns = domNewNamespace(info->document, &xmlns[6], atPtr[1]);
                }
            } else {
                ns = domNewNamespace(info->document, "", atPtr[1]);
            }
            if (newNS) {
                /* push active namespace */
                info->activeNSpos++;
                if (info->activeNSpos >= info->activeNSsize) {
                    info->activeNS = (domActiveNS*) REALLOC(
                        (char*)info->activeNS,
                        sizeof(domActiveNS) * 2 * info->activeNSsize);
                    info->activeNSsize = 2 * info->activeNSsize;
                }
                info->activeNS[info->activeNSpos].depth     = info->depth;
                info->activeNS[info->activeNSpos].namespace = ns;
            }

            h = Tcl_CreateHashEntry(&HASHTAB(info->document, tdom_attrNames),
                                    atPtr[0], &hnew);
            attrnode = (domAttrNode*) domAlloc(sizeof(domAttrNode));
            memset(attrnode, 0, sizeof(domAttrNode));
            attrnode->nodeType    = ATTRIBUTE_NODE;
            attrnode->nodeFlags   = IS_NS_NODE;
            attrnode->namespace   = ns->index;
            attrnode->nodeName    = (char *)&(h->key);
            attrnode->parentNode  = node;
            len = strlen(atPtr[1]);
            if (TclOnly8Bits && info->encoding_8bit) {
                tdom_Utf8to8Bit(info->encoding_8bit, atPtr[1], &len);
            }
            attrnode->valueLength = len;
            attrnode->nodeValue   = (char*)MALLOC(len+1);
            strcpy(attrnode->nodeValue, atPtr[1]);
            if (node->firstAttr) {
                lastAttr->nextSibling = attrnode;
            } else {
                node->firstAttr = attrnode;
            }
            lastAttr = attrnode;
        }

    }

    /*----------------------------------------------------------
    |   look for namespace of element
    \---------------------------------------------------------*/
    domSplitQName (name, tagPrefix, &localname);
    for (pos = info->activeNSpos; pos >= 0; pos--) {
        if (  ((tagPrefix[0] == '\0') && (info->activeNS[pos].namespace->prefix[0] == '\0'))
           || ((tagPrefix[0] != '\0') && (info->activeNS[pos].namespace->prefix[0] != '\0')
               && (strcmp(tagPrefix, info->activeNS[pos].namespace->prefix) == 0))
        ) {
            if (info->activeNS[pos].namespace->prefix[0] == '\0'
                && info->activeNS[pos].namespace->uri[0] == '\0'
                && tagPrefix[0] == '\0') {
                /* xml-names rec. 5.2: "The default namespace can be
                   set to the empty string. This has the same effect,
                   within the scope of the declaration, of there being
                   no default namespace." */
                goto elemNSfound;
            }
            node->namespace = info->activeNS[pos].namespace->index;
            DBG(fprintf(stderr, "tag='%s' uri='%s' \n",
                        node->nodeName,
                        info->activeNS[pos].namespace->uri);
            )
            goto elemNSfound;
        }
    }
    if (tagPrefix[0] != '\0') {
        if (strcmp (tagPrefix, "xml")==0) {
            node->namespace = info->document->rootNode->firstAttr->namespace;
        } else {
            /* Since where here, this means, the element has a
               up to now not declared namespace prefix. We probably
               should return this as an error, shouldn't we?*/
        }
    }
 elemNSfound:
#endif

    /*--------------------------------------------------------------
    |   add the attribute nodes
    |
    \-------------------------------------------------------------*/
    if ((idatt = XML_GetIdAttributeIndex (info->parser)) != -1) {
        if (!info->document->ids) {
            info->document->ids = MALLOC (sizeof (Tcl_HashTable));
            Tcl_InitHashTable (info->document->ids, TCL_STRING_KEYS);
        }
        h = Tcl_CreateHashEntry (info->document->ids,
                                 atts[idatt+1],
                                 &hnew);
        /* if hnew isn't 1 this is a validation error. Hm, no clear way
           to report this. And more, xslt and xpath can process not
           valid XML, the spec mentioned this even within the context
           of id(). If some elements share the same ID, the first in
           document order should be used. Doing it this way, this is
           guaranteed for unchanged DOM trees. There are problems, if
           the DOM tree is changed, befor using id() */
        if (hnew) {
            Tcl_SetHashValue (h, node);
        }
        idAttPtr = atts + idatt;
    } else {
        idAttPtr = NULL;
    }
    /* lastAttr already set right, either to NULL above, or to the last
       NS attribute */
    for (atPtr = atts; atPtr[0] && atPtr[1]; atPtr += 2) {

#ifdef TDOM_NS
        if (strncmp(atPtr[0], "xmlns", 5) == 0) {
            continue;
        }
#endif
        h = Tcl_CreateHashEntry(&HASHTAB(info->document, tdom_attrNames),
                                atPtr[0], &hnew);
        attrnode = (domAttrNode*) domAlloc(sizeof(domAttrNode));
        memset(attrnode, 0, sizeof(domAttrNode));
        attrnode->nodeType = ATTRIBUTE_NODE;
        if (atPtr == idAttPtr) {
            attrnode->nodeFlags |= IS_ID_ATTRIBUTE;
        } else {
            attrnode->nodeFlags = 0;
        }
        attrnode->namespace   = 0;
        attrnode->nodeName    = (char *)&(h->key);
        attrnode->parentNode  = node;
        len = strlen(atPtr[1]);
        if (TclOnly8Bits && info->encoding_8bit) {
            tdom_Utf8to8Bit(info->encoding_8bit, atPtr[1], &len);
        }
        attrnode->valueLength = len;
        attrnode->nodeValue   = (char*)MALLOC(len+1);
        strcpy(attrnode->nodeValue, (char *)atPtr[1]);

        if (node->firstAttr) {
            lastAttr->nextSibling = attrnode;
        } else {
            node->firstAttr = attrnode;
        }
        lastAttr = attrnode;

#ifdef TDOM_NS
        /*----------------------------------------------------------
        |   look for attribute namespace
        \---------------------------------------------------------*/
        domSplitQName (attrnode->nodeName, prefix, &localname);
        if (prefix[0] != '\0') {
            for (pos = info->activeNSpos; pos >= 0; pos--) {
                if (  ((prefix[0] == '\0') && (info->activeNS[pos].namespace->prefix[0] == '\0'))
                      || ((prefix[0] != '\0') && (info->activeNS[pos].namespace->prefix[0] != '\0')
                          && (strcmp(prefix, info->activeNS[pos].namespace->prefix) == 0))
                    ) {
                    attrnode->namespace = info->activeNS[pos].namespace->index;
                    DBG(fprintf(stderr, "attr='%s' uri='%s' \n",
                                attrnode->nodeName,
                                info->activeNS[pos].namespace->uri);
                        )
                    goto attrNSfound;
                }
            }
            if (strcmp (prefix, "xml")==0) {
                attrnode->namespace = 
                    info->document->rootNode->firstAttr->namespace;
            } else {
                /* Since where here, this means, the attribute has a
                   up to now not declared namespace prefix. We probably
                   should return this as an error, shouldn't we?*/
            }
        attrNSfound:
            ;
        }
#endif
    }

    info->depth++;
}

/*---------------------------------------------------------------------------
|   endElement
|
\--------------------------------------------------------------------------*/
static void
endElement (
    void        *userData,
    const char  *name
)
{
    domReadInfo  *info = userData;

    DispatchPCDATA (info);
    
    info->depth--;
#ifdef TDOM_NS
    /* pop active namespaces */
    while ( (info->activeNSpos >= 0) &&
            (info->activeNS[info->activeNSpos].depth == info->depth) )
    {
        info->activeNSpos--;
    }
#endif

    if (info->depth != -1) {
        info->currentNode = info->currentNode->parentNode;
    } else {
        info->currentNode = NULL;
    }

    if (info->depth) {
        if (info->baseURIstack[info->baseURIstackPos].depth == info->depth) {
            info->baseURIstackPos--;
        }
    }
}

/*---------------------------------------------------------------------------
|   characterDataHandler
|
\--------------------------------------------------------------------------*/
static void
characterDataHandler (
    void        *userData,
    const char  *s,
    int          len
)
{
    domReadInfo   *info = userData;

    Tcl_DStringAppend (info->cdata, s, len);
    return;
    
}

/*---------------------------------------------------------------------------
|   DispatchPCDATA
|
\--------------------------------------------------------------------------*/
static void
DispatchPCDATA (
    domReadInfo *info
    )
{
    domTextNode   *node;
    domNode       *parentNode;
    domLineColumn *lc;
    Tcl_HashEntry *h;
    char          *s;
    int            len, hnew;
    
    s = Tcl_DStringValue (info->cdata);
    len = Tcl_DStringLength (info->cdata);
    if (!len) return;
    
    if (TclOnly8Bits && info->encoding_8bit) {
        tdom_Utf8to8Bit( info->encoding_8bit, s, &len);
    }
    parentNode = info->currentNode;
    if (!parentNode) return;
    
    if (   parentNode->lastChild 
        && parentNode->lastChild->nodeType == TEXT_NODE) {

        /* normalize text node, i.e. there are no adjacent text nodes */
        node = (domTextNode*)parentNode->lastChild;
        node->nodeValue = REALLOC(node->nodeValue, node->valueLength + len);
        memmove(node->nodeValue + node->valueLength, s, len);
        node->valueLength += len;

    } else {

        if (info->ignoreWhiteSpaces) {
            char *pc;
            int   i, only_whites;

            only_whites = 1;
            for (i=0, pc = s; i < len; i++, pc++) {
                if ( (*pc != ' ')  &&
                     (*pc != '\t') &&
                     (*pc != '\n') &&
                     (*pc != '\r') ) {
                    only_whites = 0;
                    break;
                }
            }
            if (only_whites) {
                Tcl_DStringSetLength (info->cdata, 0);
                return;
            }
        }

        if (info->storeLineColumn) {
            node = (domTextNode*) domAlloc(sizeof(domTextNode)
                                            + sizeof(domLineColumn));
        } else {
            node = (domTextNode*) domAlloc(sizeof(domTextNode));
        }
        memset(node, 0, sizeof(domTextNode));
        node->nodeType    = TEXT_NODE;
        node->nodeFlags   = 0;
        node->namespace   = 0;
        node->nodeNumber  = NODE_NO(info->document);
        node->valueLength = len;
        node->nodeValue   = (char*)MALLOC(len);
        memmove(node->nodeValue, s, len);

        node->ownerDocument = info->document;
        node->parentNode = parentNode;
        if (parentNode->nodeType == ELEMENT_NODE) {
            if (parentNode->firstChild)  {
                parentNode->lastChild->nextSibling = (domNode*)node;
                node->previousSibling = parentNode->lastChild;
            } else {
                parentNode->firstChild = (domNode*)node;
            }
            parentNode->lastChild = (domNode*)node;
        }

        if (info->baseURIstack[info->baseURIstackPos].baseURI 
            != XML_GetBase (info->parser)) {
            h = Tcl_CreateHashEntry (info->document->baseURIs,
                                     (char*) node,
                                     &hnew);
            Tcl_SetHashValue (h, tdomstrdup (XML_GetBase (info->parser)));
            node->nodeFlags |= HAS_BASEURI;
        }

        if (info->storeLineColumn) {
            lc = (domLineColumn*) ( ((char*)node) + sizeof(domTextNode) );
            node->nodeFlags |= HAS_LINE_COLUMN;
            lc->line         = XML_GetCurrentLineNumber(info->parser);
            lc->column       = XML_GetCurrentColumnNumber(info->parser);
        }
    }
    Tcl_DStringSetLength (info->cdata, 0);
}


/*---------------------------------------------------------------------------
|   commentHandler
|
\--------------------------------------------------------------------------*/
static void
commentHandler (
    void        *userData,
    const char  *s
)
{
    domReadInfo   *info = userData;
    domTextNode   *node;
    domNode       *parentNode;
    domLineColumn *lc;
    int            len, hnew;
    Tcl_HashEntry *h;

    if (info->insideDTD) {
        DBG(fprintf (stderr, "commentHandler: insideDTD, skipping\n");)
        return;
    }

    DispatchPCDATA (info);

    len = strlen(s);
    if (TclOnly8Bits && info->encoding_8bit) {
        tdom_Utf8to8Bit(info->encoding_8bit, s, &len);
    }
    parentNode = info->currentNode;

    if (info->storeLineColumn) {
        node = (domTextNode*) domAlloc(sizeof(domTextNode)
                                        + sizeof(domLineColumn));
    } else {
        node = (domTextNode*) domAlloc(sizeof(domTextNode));
    }
    memset(node, 0, sizeof(domTextNode));
    node->nodeType    = COMMENT_NODE;
    node->nodeFlags   = 0;
    node->namespace   = 0;
    node->nodeNumber  = NODE_NO(info->document);
    node->valueLength = len;
    node->nodeValue   = (char*)MALLOC(len);
    memmove(node->nodeValue, s, len);

    node->ownerDocument = info->document;
    node->parentNode = parentNode;
    if (parentNode == NULL) {
        if (info->document->rootNode->lastChild) {
            info->document->rootNode->lastChild->nextSibling = (domNode*)node;
            node->previousSibling = info->document->rootNode->lastChild;
        } else {
            info->document->rootNode->firstChild = (domNode*)node;
        }
        info->document->rootNode->lastChild = (domNode*)node;
    } else if(parentNode->nodeType == ELEMENT_NODE) {
        if (parentNode->firstChild)  {
            parentNode->lastChild->nextSibling = (domNode*)node;
            node->previousSibling = parentNode->lastChild;
            parentNode->lastChild = (domNode*)node;
        } else {
            parentNode->firstChild = parentNode->lastChild = (domNode*)node;
        }
    }

    if (info->baseURIstack[info->baseURIstackPos].baseURI 
        != XML_GetBase (info->parser)) {
        h = Tcl_CreateHashEntry (info->document->baseURIs,
                                 (char*) node,
                                 &hnew);
        Tcl_SetHashValue (h, tdomstrdup (XML_GetBase (info->parser)));
        node->nodeFlags |= HAS_BASEURI;
    }

    if (info->storeLineColumn) {
        lc = (domLineColumn*) ( ((char*)node) + sizeof(domTextNode) );
        node->nodeFlags |= HAS_LINE_COLUMN;
        lc->line         = XML_GetCurrentLineNumber(info->parser);
        lc->column       = XML_GetCurrentColumnNumber(info->parser);
    }
}


/*---------------------------------------------------------------------------
|   processingInstructionHandler
|
\--------------------------------------------------------------------------*/
static void
processingInstructionHandler(
    void       *userData,
    const char *target,
    const char *data
)
{
    domProcessingInstructionNode *node;
    domReadInfo                  *info = userData;
    domNode                      *parentNode;
    domLineColumn                *lc;
    int                           len,hnew;
    Tcl_HashEntry                *h;

    if (info->insideDTD) {
        DBG(fprintf (stderr, 
                     "processingInstructionHandler: insideDTD, skipping\n");)
        return;
    }

    DispatchPCDATA (info);
    
    parentNode = info->currentNode;

    if (info->storeLineColumn) {
        node = (domProcessingInstructionNode*)
               domAlloc(sizeof(domProcessingInstructionNode)
                         + sizeof(domLineColumn));
    } else {
        node = (domProcessingInstructionNode*)
               domAlloc(sizeof(domProcessingInstructionNode));
    }
    memset(node, 0, sizeof(domProcessingInstructionNode));
    node->nodeType    = PROCESSING_INSTRUCTION_NODE;
    node->nodeFlags   = 0;
    node->namespace   = 0;
    node->nodeNumber  = NODE_NO(info->document);

    if (info->baseURIstack[info->baseURIstackPos].baseURI 
        != XML_GetBase (info->parser)) {
        h = Tcl_CreateHashEntry (info->document->baseURIs,
                                 (char*) node,
                                 &hnew);
        Tcl_SetHashValue (h, tdomstrdup (XML_GetBase (info->parser)));
        node->nodeFlags |= HAS_BASEURI;
    }

    len = strlen(target);
    if (TclOnly8Bits && info->encoding_8bit) {
        tdom_Utf8to8Bit(info->encoding_8bit, target, &len);
    }
    node->targetLength = len;
    node->targetValue  = (char*)MALLOC(len);
    memmove(node->targetValue, target, len);

    len = strlen(data);
    if (TclOnly8Bits && info->encoding_8bit) {
        tdom_Utf8to8Bit(info->encoding_8bit, data, &len);
    }
    node->dataLength = len;
    node->dataValue  = (char*)MALLOC(len);
    memmove(node->dataValue, data, len);

    node->ownerDocument = info->document;
    node->parentNode = parentNode;
    if (parentNode == NULL) {
        if (info->document->rootNode->lastChild) {
            info->document->rootNode->lastChild->nextSibling = (domNode*)node;
            node->previousSibling = info->document->rootNode->lastChild;
        } else {
            info->document->rootNode->firstChild = (domNode*)node;
        }
        info->document->rootNode->lastChild = (domNode*)node;
    } else if(parentNode->nodeType == ELEMENT_NODE) {
        if (parentNode->firstChild)  {
            parentNode->lastChild->nextSibling = (domNode*)node;
            node->previousSibling = parentNode->lastChild;
            parentNode->lastChild = (domNode*)node;
        } else {
            parentNode->firstChild = parentNode->lastChild = (domNode*)node;
        }
    }
    if (info->storeLineColumn) {
        lc = (domLineColumn*)(((char*)node)+sizeof(domProcessingInstructionNode));
        node->nodeFlags |= HAS_LINE_COLUMN;
        lc->line         = XML_GetCurrentLineNumber(info->parser);
        lc->column       = XML_GetCurrentColumnNumber(info->parser);
    }
}

/*---------------------------------------------------------------------------
|  entityDeclHandler
|
\--------------------------------------------------------------------------*/
static void
entityDeclHandler (
    void       *userData,
    const char *entityName,
    int         is_parameter_entity,
    const char *value,
    int         value_length,
    const char *base,
    const char *systemId,
    const char *publicId,
    const char *notationName
)
{
    domReadInfo                  *info = (domReadInfo *) userData;
    Tcl_HashEntry                *entryPtr;
    int                           hnew;

    if (notationName) {
        if (!info->document->unparsedEntities) {
            info->document->unparsedEntities = MALLOC (sizeof (Tcl_HashTable));
            Tcl_InitHashTable (info->document->unparsedEntities, 
                               TCL_STRING_KEYS);
        }
        entryPtr = Tcl_CreateHashEntry (info->document->unparsedEntities,
                                        entityName, &hnew);
        if (hnew) {
            Tcl_SetHashValue (entryPtr, tdomstrdup (systemId));
        }
    }
}

/*---------------------------------------------------------------------------
|  externalEntityRefHandler
|
\--------------------------------------------------------------------------*/
static int
externalEntityRefHandler (
    XML_Parser  parser,
    CONST char *openEntityNames,
    CONST char *base,
    CONST char *systemId,
    CONST char *publicId
)
{
    domReadInfo   *info = (domReadInfo *) XML_GetUserData (parser);

    Tcl_Obj *cmdPtr, *resultObj, *resultTypeObj, *extbaseObj, *xmlstringObj;
    Tcl_Obj *channelIdObj;
    int result, mode, done, byteIndex, i;
    size_t len;
    int tclLen;
    XML_Parser extparser, oldparser = NULL;
    char buf[4096], *resultType, *extbase, *xmlstring, *channelId, s[50];
    Tcl_Channel chan = (Tcl_Channel) NULL;

    if (info->document->extResolver == NULL) {
        Tcl_AppendResult (info->interp, "Can't read external entity \"",
                          systemId, "\": No -externalentitycommand given",
                          NULL);
        return 0;
    }

    DispatchPCDATA (info);

    /*
     * Take a copy of the callback script so that arguments may be appended.
     */
    cmdPtr = Tcl_NewStringObj(info->document->extResolver, -1);
    Tcl_IncrRefCount(cmdPtr);

    if (base) {
        Tcl_ListObjAppendElement(info->interp, cmdPtr,
                                 Tcl_NewStringObj(base, strlen(base)));
    } else {
        Tcl_ListObjAppendElement(info->interp, cmdPtr,
                                 Tcl_NewObj());
    }

    /* For a document with doctype declaration, the systemId is always
       != NULL. But if the document doesn't have a doctype declaration
       and the user uses -useForeignDTD 1, the externalEntityRefHandler
       will be called with a systemId (and publicId and openEntityNames)
       == NULL. */
    if (systemId) {
        Tcl_ListObjAppendElement(info->interp, cmdPtr,
                                 Tcl_NewStringObj(systemId, strlen(systemId)));
    } else {
        Tcl_ListObjAppendElement(info->interp, cmdPtr,
                                 Tcl_NewObj());
    }

    if (publicId) {
        Tcl_ListObjAppendElement(info->interp, cmdPtr,
                                 Tcl_NewStringObj(publicId, strlen(publicId)));
    } else {
        Tcl_ListObjAppendElement(info->interp, cmdPtr,
                                 Tcl_NewObj());
    }

 
#if TclOnly8Bits
    result = Tcl_GlobalEvalObj(info->interp, cmdPtr);
#else
    result = Tcl_EvalObjEx (info->interp, cmdPtr, 
                            TCL_EVAL_DIRECT | TCL_EVAL_GLOBAL);
#endif

    Tcl_DecrRefCount(cmdPtr);

    if (result != TCL_OK) {
        return 0;
    }

    extparser = XML_ExternalEntityParserCreate (parser, openEntityNames, 0);

    resultObj = Tcl_GetObjResult (info->interp);
    Tcl_IncrRefCount (resultObj);

    result = Tcl_ListObjLength (info->interp, resultObj, &tclLen);
    if ((result != TCL_OK) || (tclLen != 3)) {
        goto wrongScriptResult;
    }
    result = Tcl_ListObjIndex (info->interp, resultObj, 0, &resultTypeObj);
    if (result != TCL_OK) {
        goto wrongScriptResult;
    }
    resultType = Tcl_GetString(resultTypeObj);

    if (strcmp (resultType, "string") == 0) {
        result = Tcl_ListObjIndex (info->interp, resultObj, 2, &xmlstringObj);
        xmlstring = Tcl_GetString(xmlstringObj);
        len = strlen (xmlstring);
        chan = NULL;
    } else if (strcmp (resultType, "channel") == 0) {
        xmlstring = NULL;
        len = 0;
        result = Tcl_ListObjIndex (info->interp, resultObj, 2, &channelIdObj);
        channelId = Tcl_GetString(channelIdObj);
        chan = Tcl_GetChannel (info->interp, channelId, &mode);
        if (chan == (Tcl_Channel) NULL) {
            goto wrongScriptResult;
        }
        if ((mode & TCL_READABLE) == 0) {
            return 0;
        }
    } else if (strcmp (resultType, "filename") == 0) {
        /* result type "filename" not yet implemented */
        return 0;
    } else {
        goto wrongScriptResult;
    }

    result = Tcl_ListObjIndex (info->interp, resultObj, 1, &extbaseObj);
    if (result != TCL_OK) {
        goto wrongScriptResult;
    }
    extbase = Tcl_GetString(extbaseObj);

    /* TODO: what to do, if this document was already parsed before ? */

    if (!extparser) {
        Tcl_DecrRefCount (resultObj);
        Tcl_SetResult (info->interp,
                       "unable to create expat external entity parser",
                       NULL);
        return 0;
    }

    oldparser = info->parser;
    info->parser = extparser;
    XML_SetBase (extparser, extbase);

    if (chan == NULL) {
        if (!XML_Parse(extparser, xmlstring, strlen (xmlstring), 1)) {
            Tcl_ResetResult (info->interp);
            sprintf(s, "%ld", XML_GetCurrentLineNumber(extparser));
            Tcl_AppendResult(info->interp, "error \"",
                             XML_ErrorString(XML_GetErrorCode(extparser)),
                             "\" in entity \"", systemId,
                             "\" at line ", s, " character ", NULL);
            sprintf(s, "%ld", XML_GetCurrentColumnNumber(extparser));
            Tcl_AppendResult(info->interp, s, NULL);
            byteIndex = XML_GetCurrentByteIndex(extparser);
            if (byteIndex != -1) {
                Tcl_AppendResult(info->interp, "\n\"", NULL);
                s[1] = '\0';
                for (i=-20; i < 40; i++) {
                    if ((byteIndex+i)>=0) {
                        if (xmlstring[byteIndex+i]) {
                            s[0] = xmlstring[byteIndex+i];
                            Tcl_AppendResult(info->interp, s, NULL);
                            if (i==0) {
                                Tcl_AppendResult(info->interp,
                                                 " <--Error-- ", NULL);
                            }
                        } else {
                            break;
                        }
                    }
                }
                Tcl_AppendResult(info->interp, "\"",NULL);
            } 
            Tcl_DecrRefCount (resultObj);
            XML_ParserFree (extparser);
            info->parser = oldparser;
            return 0;
        }
    } else {
        do {
            len = Tcl_Read (chan, buf, sizeof(buf));
            done = len < sizeof(buf);
            if (!XML_Parse (extparser, buf, len, done)) {
                Tcl_ResetResult (info->interp);
                sprintf(s, "%ld", XML_GetCurrentLineNumber(extparser));
                Tcl_AppendResult(info->interp, "error \"",
                                 XML_ErrorString(XML_GetErrorCode(extparser)),
                                 "\" in entity \"", systemId,
                                 "\" at line ", s, " character ", NULL);
                sprintf(s, "%ld", XML_GetCurrentColumnNumber(extparser));
                Tcl_AppendResult(info->interp, s, NULL);
                Tcl_DecrRefCount (resultObj);
                XML_ParserFree (extparser);
                info->parser = oldparser;
                return 0;
            }
        } while (!done);
    }

    DispatchPCDATA (info);

    XML_ParserFree (extparser);
    info->parser = oldparser;

    Tcl_DecrRefCount (resultObj);
    Tcl_ResetResult (info->interp);
    return 1;

 wrongScriptResult:
    Tcl_DecrRefCount (resultObj);
    Tcl_ResetResult (info->interp);
    XML_ParserFree (extparser);
    if (oldparser) {
        info->parser = oldparser;
    }
    Tcl_AppendResult (info->interp, "The -externalentitycommand script "
                      "has to return a Tcl list with 3 elements.\n"
                      "Syntax: {string|channel|filename <baseurl> <data>}\n",
                      NULL);
    return 0;
}

/*---------------------------------------------------------------------------
|   startDoctypeDeclHandler
|
\--------------------------------------------------------------------------*/
static void
startDoctypeDeclHandler (
    void       *userData,
    const char *doctypeName,
    const char *sysid,
    const char *pubid,
    int         has_internal_subset
)
{
    domReadInfo                  *info = (domReadInfo *) userData;

    if (pubid) {
        info->document->doctype = (domDocInfo*)MALLOC (sizeof (domDocInfo));
        memset (info->document->doctype, 0, sizeof (domDocInfo));
        info->document->doctype->systemId = tdomstrdup (sysid);
        info->document->doctype->publicId = tdomstrdup (pubid);
    } else if (sysid) {
        info->document->doctype = (domDocInfo*)MALLOC (sizeof (domDocInfo));
        memset (info->document->doctype, 0, sizeof (domDocInfo));
        info->document->doctype->systemId = tdomstrdup (sysid);
    }
    info->insideDTD = 1;
}

/*---------------------------------------------------------------------------
|   endDoctypeDeclHandler
|
\--------------------------------------------------------------------------*/
static void
endDoctypeDeclHandler (
    void *userData
)
{
    domReadInfo *info = (domReadInfo *) userData;

    info->insideDTD = 0;
}

/*---------------------------------------------------------------------------
|   domReadDocument
|
\--------------------------------------------------------------------------*/
domDocument *
domReadDocument (
    XML_Parser  parser,
    char       *xml,
    int         length,
    int         ignoreWhiteSpaces,
    TEncoding  *encoding_8bit,
    int         storeLineColumn,
    int         feedbackAfter,
    Tcl_Channel channel,
    const char *baseurl,
    char       *extResolver,
    int         useForeignDTD,
    int         paramEntityParsing,
    Tcl_Interp *interp
)
{
    int            done, tclLen;
    size_t         len;
    domReadInfo    info;
    char           buf[8192];
#if !TclOnly8Bits
    Tcl_Obj       *bufObj;
    Tcl_DString    dStr;
    int            useBinary;
    char          *str;
#endif
    domDocument   *doc = domCreateDoc(baseurl, storeLineColumn);

    doc->extResolver = extResolver;

    info.parser               = parser;
    info.document             = doc;
    info.currentNode          = NULL;
    info.depth                = 0;
    info.ignoreWhiteSpaces    = ignoreWhiteSpaces;
    info.cdata                = (Tcl_DString*) MALLOC (sizeof (Tcl_DString));
    Tcl_DStringInit (info.cdata);
    info.encoding_8bit        = encoding_8bit;
    info.storeLineColumn      = storeLineColumn;
    info.feedbackAfter        = feedbackAfter;
    info.lastFeedbackPosition = 0;
    info.interp               = interp;
    info.activeNSpos          = -1;
    info.activeNSsize         = 8;
    info.activeNS             = (domActiveNS*) MALLOC (sizeof(domActiveNS) 
                                                       * info.activeNSsize);
    info.baseURIstackPos      = 0;
    info.baseURIstackSize     = INITIAL_BASEURISTACK_SIZE;
    info.baseURIstack         = (domActiveBaseURI*) 
        MALLOC (sizeof(domActiveBaseURI) * info.baseURIstackSize);
    info.insideDTD            = 0;

    XML_SetUserData(parser, &info);
    XML_SetBase (parser, baseurl);
    /* We must use XML_GetBase(), because XML_SetBase copies the baseURI,
       and we want to compare the pointers */
    info.baseURIstack[0].baseURI = XML_GetBase (parser);
    info.baseURIstack[0].depth = 0;
    XML_UseForeignDTD (parser, (unsigned char) useForeignDTD);
    XML_SetElementHandler(parser, startElement, endElement);
    XML_SetCharacterDataHandler(parser, characterDataHandler);
    XML_SetCommentHandler(parser, commentHandler);
    XML_SetProcessingInstructionHandler(parser, processingInstructionHandler);
    XML_SetEntityDeclHandler (parser, entityDeclHandler);
    if (extResolver) {
        XML_SetExternalEntityRefHandler (parser, externalEntityRefHandler);
    }
    XML_SetParamEntityParsing (parser, 
                             (enum XML_ParamEntityParsing) paramEntityParsing);
    XML_SetDoctypeDeclHandler (parser, startDoctypeDeclHandler,
                               endDoctypeDeclHandler);

    if (channel == NULL) {
        if (!XML_Parse(parser, xml, length, 1)) {
            FREE ( info.activeNS );
            FREE ( info.baseURIstack );
            Tcl_DStringFree (info.cdata);
            FREE ( info.cdata);
            domFreeDocument (doc, NULL, NULL);
            return NULL;
        }
    } else {
#if !TclOnly8Bits
        Tcl_DStringInit (&dStr);
        if (Tcl_GetChannelOption (interp, channel, "-encoding", &dStr) != TCL_OK) {
            FREE ( (char*) info.activeNS );
            FREE ( info.baseURIstack );
            Tcl_DStringFree (info.cdata);
            FREE ( info.cdata);
            domFreeDocument (doc, NULL, NULL);
            return NULL;
        }
        if (strcmp (Tcl_DStringValue (&dStr), "identity")==0 ) useBinary = 1;
        else useBinary = 0;
        Tcl_DStringFree (&dStr);
        if (useBinary) {
            do {
                len = Tcl_Read (channel, buf, sizeof(buf));
                done = len < sizeof(buf);
                if (!XML_Parse (parser, buf, len, done)) {
                    FREE ( info.activeNS );
                    FREE ( info.baseURIstack );
                    Tcl_DStringFree (info.cdata);
                    FREE ( info.cdata);
                    domFreeDocument (doc, NULL, NULL);
                    return NULL;
                }
            } while (!done);
        } else {
            bufObj = Tcl_NewObj();
            Tcl_SetObjLength (bufObj, 6144);
            do {
                len = Tcl_ReadChars (channel, bufObj, 1024, 0);
                done = (len < 1024);
                str = Tcl_GetStringFromObj(bufObj, &tclLen);
                if (!XML_Parse (parser, str, tclLen, done)) {
                    FREE ( info.activeNS );
                    FREE ( info.baseURIstack );
                    Tcl_DStringFree (info.cdata);
                    FREE ( info.cdata);
                    domFreeDocument (doc, NULL, NULL);
                    Tcl_DecrRefCount (bufObj);
                    return NULL;
                }
            } while (!done);
            Tcl_DecrRefCount (bufObj);
        }
#else
        do {
            len = Tcl_Read (channel, buf, sizeof(buf));
            done = len < sizeof(buf);
            if (!XML_Parse (parser, buf, len, done)) {
                FREE ( info.activeNS );
                FREE ( info.baseURIstack );
                domFreeDocument (doc, NULL, NULL);
                Tcl_DStringFree (info.cdata);
                FREE ( info.cdata);
                return NULL;
            }
        } while (!done);
#endif
    }
    FREE ( info.activeNS );
    FREE ( info.baseURIstack );
    Tcl_DStringFree (info.cdata);
    FREE ( info.cdata);

    domSetDocumentElement (doc);

    return doc;
}


#endif /* ifndef TDOM_NO_EXPAT */



/*---------------------------------------------------------------------------
|   domException2String
|
\--------------------------------------------------------------------------*/
const char *
domException2String (
    domException exception
)
{
    return domException2StringTable[exception];
}


/*---------------------------------------------------------------------------
|   domGetLineColumn
|
\--------------------------------------------------------------------------*/
int
domGetLineColumn (
    domNode *node,
    int     *line,
    int     *column
)
{
    char *v;
    domLineColumn  *lc;

    *line   = -1;
    *column = -1;

    if (node->nodeFlags & HAS_LINE_COLUMN) {
        v = (char*)node;
        switch (node->nodeType) {
            case ELEMENT_NODE:
                v = v + sizeof(domNode);
                break;

            case TEXT_NODE:
            case CDATA_SECTION_NODE:
            case COMMENT_NODE:
                v = v + sizeof(domTextNode);
                break;

            case PROCESSING_INSTRUCTION_NODE:
                v = v + sizeof(domProcessingInstructionNode);
                break;

            default:
                return -1;
        }
        lc = (domLineColumn *)v;
        *line   = lc->line;
        *column = lc->column;
        return 0;
    } else {
        return -1;
    }
}

#ifdef TDOM_NS
domAttrNode *
domCreateXMLNamespaceNode (
    domNode  *parent
)
{
    Tcl_HashEntry  *h;
    int             hnew;
    domAttrNode    *attr;
    domNS          *ns;

    attr = (domAttrNode *) domAlloc (sizeof (domAttrNode));
    memset (attr, 0, sizeof (domAttrNode));
    h = Tcl_CreateHashEntry(&HASHTAB(parent->ownerDocument,tdom_attrNames),
                            "xmlns:xml", &hnew);
    ns = domNewNamespace (parent->ownerDocument, "xml", XML_NAMESPACE);
    attr->nodeType      = ATTRIBUTE_NODE;
    attr->nodeFlags     = IS_NS_NODE;
    attr->namespace     = ns->index;
    attr->nodeName      = (char *)&(h->key);
    attr->parentNode    = parent;
    attr->valueLength   = strlen (XML_NAMESPACE);
    attr->nodeValue     = tdomstrdup (XML_NAMESPACE);
    return attr;
}
#endif /* TDOM_NS */


/*
 *----------------------------------------------------------------------
 *
 * domCreateDoc --
 *
 *      This procedure allocates a new domDocument, initialize it and
 *      creates its rootNode (with initialization).
 *
 * Results:
 *	The domDocument node:
 *
 * Side effects:
 *	Allocates memory for the returned domDocument and its
 *	rootNode.
 *
 *----------------------------------------------------------------------
 */

domDocument *
domCreateDoc (
    const char * baseURI,
    int          storeLineColumn
    )
{
    Tcl_HashEntry *h;
    int            hnew;
    domNode       *rootNode;
    domDocument   *doc;
    domLineColumn *lc;

    doc = (domDocument *) MALLOC (sizeof (domDocument));
    memset(doc, 0, sizeof(domDocument));
    doc->nodeType       = DOCUMENT_NODE;
    doc->documentNumber = DOC_NO(doc);
    doc->nsptr          = -1;
    doc->nslen          =  4;
    doc->namespaces     = (domNS**) MALLOC (sizeof (domNS*) * doc->nslen);
    
    /* We malloc and initialze the baseURIs hash table here to avoid
       cluttering of the code all over the place with checks. */
    doc->baseURIs = MALLOC (sizeof (Tcl_HashTable));
    Tcl_InitHashTable (doc->baseURIs, TCL_ONE_WORD_KEYS);

    TDomThreaded(
        domLocksAttach(doc);
        Tcl_InitHashTable(&doc->tdom_tagNames, TCL_STRING_KEYS);
        Tcl_InitHashTable(&doc->tdom_attrNames, TCL_STRING_KEYS);
    )

    if (storeLineColumn) {
        rootNode = (domNode*) domAlloc(sizeof(domNode)+sizeof(domLineColumn));
    } else {
        rootNode = (domNode*) domAlloc(sizeof(domNode));
    }
    memset(rootNode, 0, sizeof(domNode));
    rootNode->nodeType      = ELEMENT_NODE;
    if (baseURI) {
        h = Tcl_CreateHashEntry (doc->baseURIs, (char*)rootNode, &hnew);
        Tcl_SetHashValue (h, tdomstrdup (baseURI));
        rootNode->nodeFlags |= HAS_BASEURI;
    } else {
        rootNode->nodeFlags = 0;
    }
    rootNode->namespace     = 0;
    h = Tcl_CreateHashEntry(&HASHTAB(doc,tdom_tagNames), "", &hnew);
    rootNode->nodeName      = (char *)&(h->key);
    rootNode->nodeNumber    = NODE_NO(doc);
    rootNode->ownerDocument = doc;
    rootNode->parentNode    = NULL;
    rootNode->firstChild    = rootNode->lastChild = NULL;
#ifdef TDOM_NS
    rootNode->firstAttr     = domCreateXMLNamespaceNode (rootNode);
#endif
    if (storeLineColumn) {
        lc = (domLineColumn*) ( ((char*)rootNode) + sizeof(domNode));
        rootNode->nodeFlags |= HAS_LINE_COLUMN;
        lc->line            = 0;
        lc->column          = 0;
    }
    doc->rootNode = rootNode;

    return doc;
}

/*---------------------------------------------------------------------------
|   domCreateDocument
|
\--------------------------------------------------------------------------*/
domDocument *
domCreateDocument (
    Tcl_Interp *interp,
    const char *uri,
    char       *documentElementTagName
)
{
    Tcl_HashEntry *h;
    int            hnew;
    domNode       *node;
    domDocument   *doc;
    char           prefix[MAX_PREFIX_LEN];
    const char    *localName;
    domNS         *ns = NULL;

    if (uri) {
        domSplitQName (documentElementTagName, prefix, &localName);
        DBG(fprintf(stderr, 
                    "rootName: -->%s<--, prefix: -->%s<--, localName: -->%s<--\n", 
                    documentElementTagName, prefix, localName);)
        if (prefix[0] != '\0') {
            if (!domIsNCNAME (prefix)) {
                if (interp) {
                    Tcl_SetObjResult(interp, 
                                     Tcl_NewStringObj("invalid prefix name", -1));
                }
                return NULL;
            }
        }
        if (!domIsNCNAME (localName)) {
            if (interp) {
                Tcl_SetObjResult(interp, 
                                 Tcl_NewStringObj("invalid local name", -1));
            }
            return NULL;
        }
    } else {
        if (!domIsNAME (documentElementTagName)) {
            if (interp) {
                Tcl_SetObjResult(interp, 
                                 Tcl_NewStringObj("invalid root element name", -1));
            }
            return NULL;
        }
    }
    doc = domCreateDoc (NULL, 0);

    h = Tcl_CreateHashEntry(&HASHTAB(doc, tdom_tagNames),
                            documentElementTagName, &hnew);
    node = (domNode*) domAlloc(sizeof(domNode));
    memset(node, 0, sizeof(domNode));
    node->nodeType        = ELEMENT_NODE;
    node->nodeFlags       = 0;
    node->nodeNumber      = NODE_NO(doc);
    node->ownerDocument   = doc;
    node->nodeName        = (char *)&(h->key);
    doc->documentElement  = node;
    if (uri) {
        ns = domNewNamespace (doc, prefix, uri);
        node->namespace   = ns->index;
        domAddNSToNode (node, ns);
    }
    doc->rootNode->firstChild = doc->rootNode->lastChild = doc->documentElement;

    return doc;
}


/*---------------------------------------------------------------------------
|   domSetDocumentElement
|
\--------------------------------------------------------------------------*/
void
domSetDocumentElement (
    domDocument     *doc
    )
{
    domNode *node;
    
    doc->documentElement = NULL;
    node = doc->rootNode->firstChild;
    while (node) {
        if (node->nodeType == ELEMENT_NODE) {
            doc->documentElement = node;
            break;
        }
        node = node->nextSibling;
    }
    if (!doc->documentElement) {
        doc->documentElement = doc->rootNode->firstChild;
    }
}

/*---------------------------------------------------------------------------
|   domFreeNode
|
\--------------------------------------------------------------------------*/
void
domFreeNode (
    domNode         * node,
    domFreeCallback   freeCB,
    void            * clientData,
    int               dontfree
)
{
    int            shared = 0;
    domNode       *child, *ctemp;
    domAttrNode   *atemp, *attr, *aprev;
    Tcl_HashEntry *entryPtr;

    if (node == NULL) {
        DBG(fprintf (stderr, "null ptr in domFreeNode (dom.c) !\n");)
        return;
    }
    TDomThreaded (
        shared = node->ownerDocument && node->ownerDocument->refCount > 1;
    )
 
    /*----------------------------------------------------------------
    |   dontfree instruct us to walk the node tree and apply the 
    |   user-supplied callback, *w/o* actually deleting nodes.
    |   This is normally done when a thread detaches from the
    |   shared DOM tree and wants to garbage-collect all nodecmds
    |   in it's interpreter which attached to the tree nodes.
    \---------------------------------------------------------------*/

    if (dontfree) {
        shared = 1;
    } else {
        node->nodeFlags |= IS_DELETED;
    }

    if (node->nodeType == ATTRIBUTE_NODE && !shared) {
        attr = ((domAttrNode*)node)->parentNode->firstAttr;
        aprev = NULL;
        while (attr && (attr != (domAttrNode*)node)) {
            aprev = attr;
            attr = attr->nextSibling;
        }
        if (attr) {
            if (aprev) {
                aprev->nextSibling = attr->nextSibling;
            } else {
                ((domAttrNode*)node)->parentNode->firstAttr = attr->nextSibling;
            }
            FREE (attr->nodeValue);
            domFree ((void*)attr);
        }
    } else if (node->nodeType == ELEMENT_NODE) {
        child = node->lastChild;
        while (child) {
            ctemp = child->previousSibling;
            if (freeCB) {
                freeCB(child, clientData);
            }
            domFreeNode (child, freeCB, clientData, dontfree);
            child = ctemp;
        }
        if (shared) {
            return;
        }
        attr = node->firstAttr;
        while (attr) {
            atemp = attr;
            attr = attr->nextSibling;
            FREE (atemp->nodeValue);
            domFree ((void*)atemp);
        }
        if (node->nodeFlags & HAS_BASEURI) {
            entryPtr = Tcl_FindHashEntry (node->ownerDocument->baseURIs,
                                          (char*)node);
            if (entryPtr) {
                FREE ((char *) Tcl_GetHashValue (entryPtr));
                Tcl_DeleteHashEntry (entryPtr);
            }
        }
        domFree ((void*)node);

    } else if (node->nodeType == PROCESSING_INSTRUCTION_NODE && !shared) {
        FREE (((domProcessingInstructionNode*)node)->dataValue);
        FREE (((domProcessingInstructionNode*)node)->targetValue);
        domFree ((void*)node);

    } else if (!shared) {
        FREE (((domTextNode*)node)->nodeValue);
        domFree ((void*)node);
    }
}


/*---------------------------------------------------------------------------
|   domDeleteNode    - unlinks node from tree and free all child nodes
|                      and itself
|
\--------------------------------------------------------------------------*/
domException
domDeleteNode (
    domNode         * node,
    domFreeCallback   freeCB,
    void            * clientData
)
{
    TDomThreaded(int shared = 0;)
    domDocument *doc;

    if (node->nodeType == ATTRIBUTE_NODE) {
        domPanic("domDeleteNode on ATTRIBUTE_NODE not supported!");
    }
    TDomThreaded (
        shared = node->ownerDocument->refCount > 1;
    )
    doc = node->ownerDocument;

    /*----------------------------------------------------------------
    |   unlink node from child or fragment list
    \---------------------------------------------------------------*/
    if (node->previousSibling) {
        node->previousSibling->nextSibling = node->nextSibling;
    } else {
        if (node->parentNode) {
            node->parentNode->firstChild = node->nextSibling;
        } else {
            /* Node may be a top level node */
            if (doc->rootNode->firstChild == node) {
                doc->rootNode->firstChild = node->nextSibling;
            }
        }
    }
    if (node->nextSibling) {
        node->nextSibling->previousSibling = node->previousSibling;
    } else {
        if (node->parentNode) {
            node->parentNode->lastChild = node->previousSibling;
        } else {
            /* Node may be a top level node */
            if (doc->rootNode->lastChild == node) {
                doc->rootNode->lastChild = node->previousSibling;
            }
        }
    }
    if (doc->fragments == node) {
        doc->fragments = node->nextSibling;
    }
    if (!node->parentNode) {
        domSetDocumentElement (doc);
    }

    /*----------------------------------------------------------------
    |   for shared docs, append node to the delete nodes list
    |   otherwise delete the node physically
    \---------------------------------------------------------------*/
    if (freeCB) {
        freeCB(node, clientData);
    }
    TDomThreaded (    
        if (shared) {
            if (doc->deletedNodes) {
                doc->deletedNodes->nextDeleted = node;
            } else {
                doc->deletedNodes = node;
            }
            node->nodeFlags |= IS_DELETED;
            node->nextDeleted = NULL;
        }
    )
    MutationEvent3(DOMNodeRemoved, childToRemove, node);
    MutationEvent2(DOMSubtreeModified, node);
    domFreeNode(node, freeCB, clientData, 0);

    return OK;
}


/*---------------------------------------------------------------------------
|   domFreeDocument
|
\--------------------------------------------------------------------------*/
void
domFreeDocument (
    domDocument     * doc,
    domFreeCallback   freeCB,
    void            * clientData
)
{
    domNode      *node, *next;
    domNS        *ns;
    int           i, dontfree = 0;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;

    if (doc->nodeFlags & DONT_FREE) {
        doc->nodeFlags &= ~DONT_FREE;
        dontfree = 1;
    }
    /*-----------------------------------------------------------
    |   delete main trees, including top level PIs, etc.
    \-----------------------------------------------------------*/
    node = doc->rootNode;
    if (node) {
        if (freeCB) {
            freeCB(node, clientData);
        }
        domFreeNode (node, freeCB, clientData, dontfree);
    }

    /*-----------------------------------------------------------
    | delete fragment trees
    \-----------------------------------------------------------*/
    node = doc->fragments;
    while (node) {
        next = node->nextSibling;
        if (freeCB) {
            freeCB(node, clientData);
        }
        domFreeNode (node, freeCB, clientData, dontfree);
        node = next;
    }

    if (dontfree) return;
    
    /*-----------------------------------------------------------
    | delete namespaces
    \-----------------------------------------------------------*/
    for (i = 0; i <= doc->nsptr; i++) {
        ns = doc->namespaces[i];
        FREE(ns->uri);
        FREE(ns->prefix);
        FREE ((char*) ns);
    }
    FREE ((char *)doc->namespaces);

    /*-----------------------------------------------------------
    | delete global selectNodes prefix namespace mappings
    \-----------------------------------------------------------*/
    if (doc->prefixNSMappings) {
        i = 0;
        while (doc->prefixNSMappings[i]) {
            FREE (doc->prefixNSMappings[i]);
            i++;
        }
        FREE (doc->prefixNSMappings);
    }

    /*-----------------------------------------------------------
    | delete doctype info
    \-----------------------------------------------------------*/
    if (doc->doctype) {
#define DOCINFO_FREE(item) if (doc->doctype->item) FREE(doc->doctype->item)
        DOCINFO_FREE(systemId);
        DOCINFO_FREE(publicId);
        DOCINFO_FREE(internalSubset);
        DOCINFO_FREE(encoding);
        DOCINFO_FREE(mediaType);
        DOCINFO_FREE(method);
        if (doc->doctype->cdataSectionElements) {
            Tcl_DeleteHashTable (doc->doctype->cdataSectionElements);
            FREE (doc->doctype->cdataSectionElements);
        }

        FREE((char*) doc->doctype);
    }

    /*-----------------------------------------------------------
    | delete ID hash table
    \-----------------------------------------------------------*/
    if (doc->ids) {
        Tcl_DeleteHashTable (doc->ids);
        FREE (doc->ids);
    }

    /*-----------------------------------------------------------
    | delete unparsed entities hash table
    \-----------------------------------------------------------*/
    if (doc->unparsedEntities) {
        entryPtr = Tcl_FirstHashEntry (doc->unparsedEntities, &search);
        while (entryPtr) {
            FREE (Tcl_GetHashValue (entryPtr));
            entryPtr = Tcl_NextHashEntry (&search);
        }
        Tcl_DeleteHashTable (doc->unparsedEntities);
        FREE (doc->unparsedEntities);
    }

    /*-----------------------------------------------------------
    | delete base URIs hash table
    \-----------------------------------------------------------*/
    entryPtr = Tcl_FirstHashEntry (doc->baseURIs, &search);
    while (entryPtr) {
        FREE (Tcl_GetHashValue (entryPtr));
        entryPtr = Tcl_NextHashEntry (&search);
    }
    Tcl_DeleteHashTable (doc->baseURIs);
    FREE (doc->baseURIs);
    
    /*-----------------------------------------------------------
    | delete xpath cache hash table
    \-----------------------------------------------------------*/
    if (doc->xpathCache) {
        entryPtr = Tcl_FirstHashEntry (doc->xpathCache, &search);
        while (entryPtr) {
            xpathFreeAst((ast)Tcl_GetHashValue (entryPtr));
            entryPtr = Tcl_NextHashEntry (&search);
        }
        Tcl_DeleteHashTable (doc->xpathCache);
        FREE (doc->xpathCache);
    }

    if (doc->extResolver) {
        FREE (doc->extResolver);
    }

    /*-----------------------------------------------------------
    | delete tag/attribute hash tables (for threaded builds only)
    \-----------------------------------------------------------*/
    TDomThreaded (
        {
            Tcl_HashEntry *entryPtr;
            Tcl_HashSearch search;
            entryPtr = Tcl_FirstHashEntry(&doc->tdom_tagNames, &search);
            while (entryPtr) {
                Tcl_DeleteHashEntry(entryPtr);
                entryPtr = Tcl_NextHashEntry(&search);
            }
            Tcl_DeleteHashTable(&doc->tdom_tagNames);
            entryPtr = Tcl_FirstHashEntry(&doc->tdom_attrNames, &search);
            while (entryPtr) {
                Tcl_DeleteHashEntry(entryPtr);
                entryPtr = Tcl_NextHashEntry(&search);
            }
            Tcl_DeleteHashTable(&doc->tdom_attrNames);
            domLocksDetach(doc);
            node = doc->deletedNodes;
            while (node) {
                next = node->nextSibling;
                domFreeNode (node, freeCB, clientData, 0);
                node = next;
            }
        }
    )

    FREE ((char*)doc);
}

/*---------------------------------------------------------------------------
|   domSetAttribute
|
\--------------------------------------------------------------------------*/
domAttrNode *
domSetAttribute (
    domNode    *node,
    const char *attributeName,
    const char *attributeValue
)
{
    domAttrNode   *attr, *lastAttr;
    Tcl_HashEntry *h;
    int            hnew;

    if (!node || node->nodeType != ELEMENT_NODE) {
        return NULL;
    }

    /*----------------------------------------------------
    |   try to find an existing attribute
    \---------------------------------------------------*/
    attr = node->firstAttr;
    while (attr && strcmp(attr->nodeName, attributeName)) {
        attr = attr->nextSibling;
    }
    if (attr) {
        if (attr->nodeFlags & IS_ID_ATTRIBUTE) {
            h = Tcl_FindHashEntry (node->ownerDocument->ids, attr->nodeValue);
            if (h) {
                Tcl_DeleteHashEntry (h);
                h = Tcl_CreateHashEntry (node->ownerDocument->ids,
                                         attributeValue, &hnew);
                /* XXX what to do, if hnew = 0  ??? */
                Tcl_SetHashValue (h, node);
            }
        }
        FREE (attr->nodeValue);
        attr->valueLength = strlen(attributeValue);
        attr->nodeValue   = (char*)MALLOC(attr->valueLength+1);
        strcpy(attr->nodeValue, attributeValue);
    } else {
        /*-----------------------------------------------
        |   add a complete new attribute node
        \----------------------------------------------*/
        attr = (domAttrNode*) domAlloc(sizeof(domAttrNode));
        memset(attr, 0, sizeof(domAttrNode));
        h = Tcl_CreateHashEntry(&HASHTAB(node->ownerDocument,tdom_attrNames),
                                attributeName, &hnew);
        attr->nodeType    = ATTRIBUTE_NODE;
        attr->nodeFlags   = 0;
        attr->namespace   = 0;
        attr->nodeName    = (char *)&(h->key);
        attr->parentNode  = node;
        attr->valueLength = strlen(attributeValue);
        attr->nodeValue   = (char*)MALLOC(attr->valueLength+1);
        strcpy(attr->nodeValue, attributeValue);

        if (node->firstAttr) {
            lastAttr = node->firstAttr;
            /* move to the end of the attribute list */
            while (lastAttr->nextSibling) lastAttr = lastAttr->nextSibling;
            lastAttr->nextSibling = attr;
        } else {
            node->firstAttr = attr;
        }
    }
    MutationEvent();
    return attr;
}

/*---------------------------------------------------------------------------
|   domSetAttributeNS
|
\--------------------------------------------------------------------------*/
domAttrNode *
domSetAttributeNS (
    domNode *node,
    const char *attributeName,
    const char *attributeValue,
    const char *uri,
    int         createNSIfNeeded
)
{
    domAttrNode   *attr, *lastAttr;
    Tcl_HashEntry *h;
    int            hnew, hasUri = 1, isNSAttr = 0, isDftNS = 0;
    domNS         *ns;
    char           prefix[MAX_PREFIX_LEN];
    const char    *localName, *newLocalName;
    Tcl_DString    dStr;
    
    DBG(fprintf (stderr, "domSetAttributeNS: attributeName %s, attributeValue %s, uri %s\n", attributeName, attributeValue, uri);)
    if (!node || node->nodeType != ELEMENT_NODE) {
        return NULL;
    }

    domSplitQName (attributeName, prefix, &localName);
    if (!uri || uri[0]=='\0') hasUri = 0;
    if (hasUri && (prefix[0] == '\0')) return NULL;
    if ((prefix[0] == '\0' && strcmp (localName, "xmlns")==0)
        || (strcmp (prefix, "xmlns")==0)) {
        isNSAttr = 1;
        createNSIfNeeded = 0;
        if (prefix[0] == '\0') {
            isDftNS = 1;
            ns = domLookupPrefix (node, "");
        } else {
            ns = domLookupPrefix (node, prefix);
        }
        if (ns && (strcmp (ns->uri, attributeValue)==0)) return NULL;
        if (!hasUri) {
            uri = attributeValue;
            isNSAttr = 1;
            hasUri = 1;
            if (strcmp (localName, "xmlns")==0) isDftNS = 1;
        } else {
            return NULL;
        }
    }
    if (!hasUri) {
        if (prefix[0] != '\0' && strcmp (prefix, "xml")==0) {
            uri = "http://www.w3.org/XML/1998/namespace";
            hasUri = 1;
        }
    }
    if (!hasUri && prefix[0] != '\0') return NULL;

    /*----------------------------------------------------
    |   try to find an existing attribute
    \---------------------------------------------------*/
    attr = node->firstAttr;
    while (attr) {
        if (hasUri) {
            if (attr->nodeFlags & IS_NS_NODE) {
                if (isNSAttr) {
                    if (strcmp (attributeName, attr->nodeName)==0) {
                        break;
                    }
                }
            } else {
                if (attr->namespace && !isNSAttr) {
                    ns = domGetNamespaceByIndex (node->ownerDocument,
                                                 attr->namespace);
                    if (strcmp (uri, ns->uri)==0) {
                        newLocalName = localName;
                        domSplitQName (attr->nodeName, prefix, &localName);
                        if (strcmp (newLocalName, localName)==0) break;
                    }
                }
            }
        } else {
            if (!attr->namespace) {
                if (strcmp (attr->nodeName, localName)==0) break;
            }
        }
        attr = attr->nextSibling;
    }
    if (attr) {
        DBG(fprintf (stderr, "domSetAttributeNS: reseting existing attribute %s ; old value: %s\n", attr->nodeName, attr->nodeValue);)
        if (attr->nodeFlags & IS_ID_ATTRIBUTE) {
            h = Tcl_FindHashEntry (node->ownerDocument->ids, attr->nodeValue);
            if (h) {
                Tcl_DeleteHashEntry (h);
                h = Tcl_CreateHashEntry (node->ownerDocument->ids,
                                         attributeValue, &hnew);
                Tcl_SetHashValue (h, node);
            }
        }
        FREE (attr->nodeValue);
        attr->valueLength = strlen(attributeValue);
        attr->nodeValue   = (char*)MALLOC(attr->valueLength+1);
        strcpy(attr->nodeValue, attributeValue);
    } else {
        /*--------------------------------------------------------
        |   add a complete new attribute node
        \-------------------------------------------------------*/
        attr = (domAttrNode*) domAlloc(sizeof(domAttrNode));
        memset(attr, 0, sizeof(domAttrNode));
        h = Tcl_CreateHashEntry(&HASHTAB(node->ownerDocument,tdom_attrNames),
                                attributeName, &hnew);
        attr->nodeType = ATTRIBUTE_NODE;
        if (hasUri) {
            if (isNSAttr) {
                if (isDftNS) {
                    ns = domLookupNamespace (node->ownerDocument, "", uri);
                } else {
                    ns = domLookupNamespace (node->ownerDocument, localName, uri);
                }
            } else {
                ns = domLookupPrefix (node, prefix);
                if (ns && (strcmp (ns->uri, uri)!=0)) ns = NULL;
            }
            if (!ns) {
                if (isNSAttr) {
                    if (isDftNS) {
                        ns = domNewNamespace (node->ownerDocument, "", uri);
                    } else {
                        ns = domNewNamespace (node->ownerDocument, localName, uri);
                    }
                } else {
                    ns = domNewNamespace (node->ownerDocument, prefix, uri);
                    if (createNSIfNeeded) {
                        if (prefix[0] == '\0') {
                            domSetAttributeNS (node, "xmlns", uri, NULL, 0);
                        } else {
                            Tcl_DStringInit (&dStr);
                            Tcl_DStringAppend (&dStr, "xmlns:", 6);
                            Tcl_DStringAppend (&dStr, prefix, -1);
                            domSetAttributeNS (node, Tcl_DStringValue (&dStr),
                                               uri, NULL, 0);
                        }
                    }
                }
            }
            attr->namespace = ns->index;
            if (isNSAttr) {
                attr->nodeFlags = IS_NS_NODE;
            }
        }
        attr->nodeName    = (char *)&(h->key);
        attr->parentNode  = node;
        attr->valueLength = strlen(attributeValue);
        attr->nodeValue   = (char*)MALLOC(attr->valueLength+1);
        strcpy(attr->nodeValue, attributeValue);

        if (isNSAttr) {
            if (node->firstAttr && (node->firstAttr->nodeFlags & IS_NS_NODE)) {
                lastAttr = node->firstAttr;
                while (lastAttr->nextSibling
                       && (lastAttr->nextSibling->nodeFlags & IS_NS_NODE)) {
                    lastAttr = lastAttr->nextSibling;
                }
                attr->nextSibling = lastAttr->nextSibling;
                lastAttr->nextSibling = attr;
            } else {
                attr->nextSibling = node->firstAttr;
                node->firstAttr = attr;
            }
        } else {
            if (node->firstAttr) {
                lastAttr = node->firstAttr;
                /* move to the end of the attribute list */
                while (lastAttr->nextSibling) lastAttr = lastAttr->nextSibling;
                lastAttr->nextSibling = attr;
            } else {
                node->firstAttr = attr;
            }
        }
    }
    MutationEvent();
    return attr;
}


/*---------------------------------------------------------------------------
|   domRemoveAttribute
|
\--------------------------------------------------------------------------*/
int
domRemoveAttribute (
    domNode    *node,
    const char *attributeName
)
{
    domAttrNode *attr, *previous = NULL;
    Tcl_HashEntry *h;

    if (!node || node->nodeType != ELEMENT_NODE) {
        return -1;
    }

    /*----------------------------------------------------
    |   try to find the attribute
    \---------------------------------------------------*/
    attr = node->firstAttr;
    while (attr && strcmp(attr->nodeName, attributeName)) {
        previous = attr;
        attr = attr->nextSibling;
    }
    if (attr) {
        if (previous) {
            previous->nextSibling = attr->nextSibling;
        } else {
            attr->parentNode->firstAttr = attr->nextSibling;
        }

        if (attr->nodeFlags & IS_ID_ATTRIBUTE) {
            h = Tcl_FindHashEntry (node->ownerDocument->ids, attr->nodeValue);
            if (h) Tcl_DeleteHashEntry (h);
        }
        FREE (attr->nodeValue);
        MutationEvent();

        domFree ((void*)attr);
        return 0;
    }
    return -1;
}


/*---------------------------------------------------------------------------
|   domRemoveAttributeNS
|
\--------------------------------------------------------------------------*/
int
domRemoveAttributeNS (
    domNode    *node,
    const char *uri,
    const char *localName
)
{
    domAttrNode *attr, *previous = NULL;
    domNS       *ns = NULL;
    char         prefix[MAX_PREFIX_LEN];
    const char  *str;
    Tcl_HashEntry *h;

    if (!node || node->nodeType != ELEMENT_NODE) {
        return -1;
    }

    attr = node->firstAttr;
    while (attr) {
        domSplitQName (attr->nodeName, prefix, &str);
        if (strcmp(localName,str)==0) {
            ns = domGetNamespaceByIndex(node->ownerDocument, attr->namespace);
            if (strcmp(ns->uri, uri)==0) {
                if (previous) {
                    previous->nextSibling = attr->nextSibling;
                } else {
                    attr->parentNode->firstAttr = attr->nextSibling;
                }

                if (attr->nodeFlags & IS_ID_ATTRIBUTE) {
                    h = Tcl_FindHashEntry (node->ownerDocument->ids, 
                                           attr->nodeValue);
                    if (h) Tcl_DeleteHashEntry (h);
                }
                FREE (attr->nodeValue);
                MutationEvent();
                domFree ((void*)attr);
                return 0;
            }
        }
        previous = attr;
        attr = attr->nextSibling;
    }
    return -1;
}


/*---------------------------------------------------------------------------
|   __dbgAttr
|
\--------------------------------------------------------------------------*/
DBG(
static void __dbgAttr (domAttrNode *node) {

    DBG(fprintf(stderr, " %s=%s", node->nodeName, node->nodeValue);)
    if (node->nextSibling) __dbgAttr(node->nextSibling);
}
)


/*---------------------------------------------------------------------------
|   domSetDocument
|
\--------------------------------------------------------------------------*/
void
domSetDocument (
    domNode     *node,
    domDocument *doc
)
{
    domNode *child;
    domNS   *ns, *origNS;
    domDocument *origDoc;
    domAttrNode *attr;
    Tcl_HashEntry *h;
    TDomThreaded (
        int hnew;
    )
    
    if (node->nodeFlags & HAS_BASEURI) {
        h = Tcl_FindHashEntry (node->ownerDocument->baseURIs, (char*)node);
        if (h) {
            FREE ((char *) Tcl_GetHashValue (h));
            Tcl_DeleteHashEntry (h);
        }
        node->nodeFlags &= ~HAS_BASEURI;
    }
    if (node->nodeType == ELEMENT_NODE) {
        origDoc = node->ownerDocument;
        node->ownerDocument = doc;
        for (attr = node->firstAttr; attr != NULL; attr = attr->nextSibling) {
            if (attr->nodeFlags & IS_NS_NODE) {
                origNS = origDoc->namespaces[attr->namespace-1];
                ns = domNewNamespace (doc, origNS->prefix, origNS->uri);
                attr->namespace = ns->index;
            } else if (attr->namespace) {
                ns = domAddNSToNode (node, 
                                     origDoc->namespaces[attr->namespace-1]);
                if (ns) attr->namespace = ns->index;
            }
        }
        if (node->namespace) {
            ns = domAddNSToNode (node, origDoc->namespaces[node->namespace-1]);
            if (ns) node->namespace = ns->index;
        } else {
            ns = domAddNSToNode (node, NULL);
            if (ns) {
                node->namespace = ns->index;
            }
        }
        DBG(fprintf(stderr, "domSetDocument node%s ", node->nodeName);
             __dbgAttr(node->firstAttr);
             fprintf(stderr, "\n");
        )
                
        TDomThreaded (
            if (origDoc != doc) {
                /* Make hash table entries as necessary for
                 * tdom_tagNames and tdom_attrNames. */
                h = Tcl_CreateHashEntry(&doc->tdom_tagNames, node->nodeName,
                                        &hnew);
                node->nodeName = (domString) &(h->key);
                for (attr = node->firstAttr; 
                     attr != NULL; 
                     attr = attr->nextSibling) {
                    h = Tcl_CreateHashEntry(&doc->tdom_attrNames, 
                                            attr->nodeName, &hnew);
                    attr->nodeName = (domString) &(h->key);
                }
            }
        )
        child = node->firstChild;
        while (child != NULL) {
            domSetDocument (child, doc);
            child = child->nextSibling;
        }
    } else {
        node->ownerDocument = doc;
    }

    DBG(fprintf(stderr, "end domSetDocument node %s\n", node->nodeName);)
}


/*---------------------------------------------------------------------------
|   domSetNodeValue
|
\--------------------------------------------------------------------------*/
domException
domSetNodeValue (
    domNode    *node,
    const char *nodeValue,
    int         valueLen
)
{
    domTextNode   *textnode;

    if ((node->nodeType != TEXT_NODE) &&
        (node->nodeType != CDATA_SECTION_NODE) &&
        (node->nodeType != COMMENT_NODE)
    ) {
        return NO_MODIFICATION_ALLOWED_ERR;
    }

    textnode = (domTextNode*) node;
    FREE(textnode->nodeValue);
    textnode->nodeValue   = MALLOC (valueLen);
    textnode->valueLength = valueLen;
    memmove(textnode->nodeValue, nodeValue, valueLen);
    MutationEvent();
    return OK;
}


/*
 *----------------------------------------------------------------------
 *
 * domRemoveChild --
 *
 *      This procedure implements the dom method removeChild. Removes
 *      child from the list of children of node.
 *
 * Results:
 *	Returns a domException:
 *
 *      NOT_FOUND_ERR: Raised if the node child is not a child of node.
 *
 *      OK: otherwise
 *
 * Side effects:
 *	Alters the involved document.
 *
 *----------------------------------------------------------------------
 */

domException
domRemoveChild (
    domNode *node,
    domNode *child
)
{
    domNode *n;

    /* check, if node is in deed the parent of child */
    if (child->parentNode != node) {
        /* If node is the root node of a document and child
           is in deed a child of this node, then 
           child->parentNode will be NULL. In this case, we
           loop throu the childs of node, to see, if the child
           is valid. */
        if (node->ownerDocument->rootNode == node) {
            n = node->firstChild;
            while (n) {
                if (n == child) {
                    /* child is in deed a child of node */
                    break;
                }
                n = n->nextSibling;
            }
            if (!n) {
                return NOT_FOUND_ERR;
            }
        } else {
            return NOT_FOUND_ERR;
        }
    }

    if (child->previousSibling) {
        child->previousSibling->nextSibling =  child->nextSibling;
    } else {
        node->firstChild = child->nextSibling;
    }
    if (child->nextSibling) {
        child->nextSibling->previousSibling =  child->previousSibling;
    } else {
        node->lastChild = child->previousSibling;
    }

    /* link child into the fragments list */
    if (child->ownerDocument->fragments) {
        child->nextSibling = child->ownerDocument->fragments;
        child->ownerDocument->fragments->previousSibling = child;
        child->ownerDocument->fragments = child;
    } else {
        child->ownerDocument->fragments = child;
        child->nextSibling = NULL;
    }
    child->parentNode = NULL;
    child->previousSibling = NULL;
    MutationEvent3(DOMNodeRemoved, child, node);
    MutationEvent2(DOMSubtreeModified, node);
    return OK;
}


/*
 *----------------------------------------------------------------------
 *
 * domAppendChild --
 *
 *      This procedure implements the dom method appendChild.  Adds the
 *      node newChild to the end of the list of children of this
 *      node. If the newChild is already in the tree, it is first
 *      removed.
 *
 * Results:
 *	Returns a domException:
 *
 *      HIERARCHY_REQUEST_ERR: Raised if node is of a type that does
 *      not allow children of the type of the childToAppend node, or
 *      if the node to append is one of node's ancestors or the
 *      rootNode of node's document.
 *
 *      NOT_SUPPORTED_ERR: Raised if the childToInsert is the rootNode
 *      of another document or if node is a rootNode.
 *
 *      OK: otherwise
 *
 * Side effects:
 *	Alters the involved document(s).
 *
 *----------------------------------------------------------------------
 */

domException
domAppendChild (
    domNode *node,
    domNode *childToAppend
)
{
    domNode *n;

    if (node->nodeType != ELEMENT_NODE) {
        return HIERARCHY_REQUEST_ERR;
    }

    /* check, whether childToAppend is node or one of node's ancestors */
    n = node;
    while (n) {
        if (n == childToAppend) {
            return HIERARCHY_REQUEST_ERR;
        }
        n = n->parentNode;
    }

    if (childToAppend == childToAppend->ownerDocument->rootNode) {
        if (childToAppend == node->ownerDocument->rootNode) {
            return HIERARCHY_REQUEST_ERR;
        } else {
            return NOT_SUPPORTED_ERR;
        }
    }

    /* unlink childToAppend */
    if (childToAppend->previousSibling) {
        childToAppend->previousSibling->nextSibling = 
            childToAppend->nextSibling;
    } else {
        if (childToAppend->parentNode) {
            childToAppend->parentNode->firstChild = childToAppend->nextSibling;
        } else {
            /* childToAppend is either out of the fragment list or
               a child of the rootNode of its document */
            if (childToAppend->ownerDocument->fragments == childToAppend) {
                childToAppend->ownerDocument->fragments = 
                    childToAppend->nextSibling;
            } else {
                childToAppend->ownerDocument->rootNode->firstChild =
                    childToAppend->nextSibling;
            }
        }
    }
    if (childToAppend->nextSibling) {
        childToAppend->nextSibling->previousSibling =
            childToAppend->previousSibling;
    } else {
        if (childToAppend->parentNode) {
            childToAppend->parentNode->lastChild = 
                childToAppend->previousSibling;
        } else {
            if (childToAppend->ownerDocument->rootNode->lastChild
                == childToAppend) {
                childToAppend->ownerDocument->rootNode->lastChild =
                    childToAppend->previousSibling;
            }
        }
    }

    if (node->lastChild) {
        node->lastChild->nextSibling = childToAppend;
        childToAppend->previousSibling = node->lastChild;
    } else {
        node->firstChild = childToAppend;
        childToAppend->previousSibling = NULL;
    }
    node->lastChild = childToAppend;
    childToAppend->nextSibling = NULL;
    if (!childToAppend->parentNode &&
        (childToAppend->ownerDocument->documentElement == childToAppend)) {
        childToAppend->ownerDocument->documentElement =
            childToAppend->ownerDocument->rootNode->firstChild;
    }
    if (node == node->ownerDocument->rootNode) {
        childToAppend->parentNode = NULL;
    } else {
        childToAppend->parentNode = node;
    }

    if ((node->ownerDocument != childToAppend->ownerDocument)
        || node->ownerDocument->nsptr
        || childToAppend->ownerDocument->baseURIs->numEntries) {
        domSetDocument (childToAppend, node->ownerDocument);
    }
    node->ownerDocument->nodeFlags |= NEEDS_RENUMBERING;
    MutationEvent();
    return OK;
}


/*
 *----------------------------------------------------------------------
 *
 * domInsertBefore --
 *
 *	This procedure implements the dom method insertBefore.
 *      It inserts the node childToInsert before the existing child
 *      node referenceChild. If referenceChild is null, insert
 *      childToInsert at the end of the list of children of node. The
 *      arguments node and childToInsert must be non NULL. The
 *      childToInsert is unlinked from its previous place (fragment
 *      list or tree).
 *
 * Results:
 *	Returns a domException:
 *
 *      HIERARCHY_REQUEST_ERR: Raised if node is of a type that does
 *      not allow children of the type of the childToInsert node, or
 *      if the node to insert is node or one of node's ancestors or the
 *      rootNode of node's document.
 *
 *      NOT_FOUND_ERR: Raised if refChild is not a child of this node.
 *
 *      NOT_SUPPORTED_ERR: Raised if the childToInsert is the rootNode
 *      of another document or if node is a rootNode.
 *
 *      OK: otherwise
 *
 * Side effects:
 *	Alters the involved document(s).
 *
 *----------------------------------------------------------------------
 */

domException
domInsertBefore (
    domNode *node,
    domNode *childToInsert,
    domNode *referenceChild
)
{
    domNode *n;


    if (node->nodeType != ELEMENT_NODE) {
        return HIERARCHY_REQUEST_ERR;
    }

    /* check, if node is in deed the parent of referenceChild */
    if (referenceChild) {
        if (referenceChild->parentNode != node) {
            /* If node is the root node of a document and referenceChild
               is in deed a child of this node, then 
               referenceChild->parentNode will be NULL. In this case, we
               loop throu the childs of node, to see, if the referenceChild
               is valid. */
            if (node->ownerDocument->rootNode == node) {
                n = node->firstChild;
                while (n) {
                    if (n == referenceChild) {
                        /* referenceChild is in deed a child of node */
                        break;
                    }
                    n = n->nextSibling;
                }
                if (!n) {
                    return NOT_FOUND_ERR;
                }
            } else {
                return NOT_FOUND_ERR;
            }
        }
    }
    
    if (childToInsert == referenceChild) {
        return OK;
    }

    /* check, whether childToInsert is one of node's ancestors */
    n = node;
    while (n) {
        if (n == childToInsert) {
            return HIERARCHY_REQUEST_ERR;
        }
        n = n->parentNode;
    }

    if (childToInsert == childToInsert->ownerDocument->rootNode) {
        if (childToInsert == node->ownerDocument->rootNode) {
            return HIERARCHY_REQUEST_ERR;
        } else {
            /* For now, we simply don't allow the rootNode of
               another element as childToInsert. The way to go may
               be simply to treat the rootNode as DocumentFragment
               and to insert all childs of that rootNode before the
               referenceChild.  This would result in a document
               without documentElement, which then should be
               handled right by other methods. This is planed, but
               not carefully considered, yet.  */
            return NOT_SUPPORTED_ERR;
        }
    }


    /* unlink childToInsert */
    if (childToInsert->previousSibling) {
        childToInsert->previousSibling->nextSibling = 
            childToInsert->nextSibling;
    } else {
        if (childToInsert->parentNode) {
            childToInsert->parentNode->firstChild = childToInsert->nextSibling;
        } else {
            /* childToInsert is either out of the fragment list or
               a child of the rootNode of its document */
            if (childToInsert->ownerDocument->fragments == childToInsert) {
                childToInsert->ownerDocument->fragments = 
                    childToInsert->nextSibling;
            } else {
                childToInsert->ownerDocument->rootNode->firstChild =
                    childToInsert->nextSibling;
            }
        }
    }
    if (childToInsert->nextSibling) {
        childToInsert->nextSibling->previousSibling =
            childToInsert->previousSibling;
    } else {
        if (childToInsert->parentNode) {
            childToInsert->parentNode->lastChild = 
                childToInsert->previousSibling;
        } else {
            if (childToInsert->ownerDocument->rootNode->lastChild
                == childToInsert) {
                childToInsert->ownerDocument->rootNode->lastChild =
                    childToInsert->previousSibling;
            }
        }
    }

    childToInsert->nextSibling = referenceChild;
    if (referenceChild) {
        if (referenceChild->previousSibling) {
            childToInsert->previousSibling = referenceChild->previousSibling;
            referenceChild->previousSibling->nextSibling = childToInsert;
        } else {
            node->firstChild = childToInsert;
            childToInsert->previousSibling = NULL;
        }
        referenceChild->previousSibling = childToInsert;
    } else {
        if (node->lastChild) {
            node->lastChild->nextSibling = childToInsert;
            childToInsert->previousSibling = node->lastChild;
        } else {
            node->firstChild = childToInsert;
            childToInsert->previousSibling = NULL;
        }
        node->lastChild = childToInsert;
    }
    if (!childToInsert->parentNode &&
        (childToInsert->ownerDocument->documentElement == childToInsert)) {
        childToInsert->ownerDocument->documentElement =
            childToInsert->ownerDocument->rootNode->firstChild;
    }
    if (node == node->ownerDocument->rootNode) {
        childToInsert->parentNode = NULL;
    } else {
        childToInsert->parentNode = node;
    }
    if (node->ownerDocument != childToInsert->ownerDocument
        || node->ownerDocument->nsptr
        || childToInsert->ownerDocument->baseURIs->numEntries) {
        domSetDocument (childToInsert, node->ownerDocument);
    }
    node->ownerDocument->nodeFlags |= NEEDS_RENUMBERING;
    MutationEvent3(DOMNodeInsert, childToInsert, node);
    MutationEvent2(DOMSubtreeModified, node);
    return OK;
}



/*
 *----------------------------------------------------------------------
 *
 * domReplaceChild --
 *
 *	This procedure implements the dom method replaceChild.
 *      Replaces the child node oldChild with newChild in the list of
 *      children of node 'node'.
 *
 * Results:
 *	Returns a domException:
 *
 *      HIERARCHY_REQUEST_ERR: Raised if node is of a type that does
 *      not allow children of the type of the newChild node, or
 *      if newChild is node or one of node's ancestors or the
 *      rootNode of node's document.
 *
 *      NOT_FOUND_ERR: Raised if oldChild is not a child of node.
 *
 *      NOT_SUPPORTED_ERR: Raised if the newChild is the rootNode
 *      of another document.
 *
 *      OK: otherwise
 *
 * Side effects:
 *	Alters the involved document(s).
 *
 *----------------------------------------------------------------------
 */

domException
domReplaceChild (
    domNode *node,
    domNode *newChild,
    domNode *oldChild
)
{
    domNode *n;


    if (node->nodeType != ELEMENT_NODE) {
        return HIERARCHY_REQUEST_ERR;
    }

    /* check, if node is in deed the parent of oldChild */
    if (oldChild->parentNode != node) {
        /* If node is the root node of a document and oldChild
           is in deed a child of this node, then 
           oldChild->parentNode will be NULL. In this case, we
           loop throu the childs of node, to see, if the oldChild
           is valid. */
        if (node->ownerDocument->rootNode == node) {
            n = node->firstChild;
            while (n) {
                if (n == oldChild) {
                    /* oldChild is in deed a child of node */
                    break;
                }
                n = n->nextSibling;
            }
            if (!n) {
                return NOT_FOUND_ERR;
            }
        } else {
            return NOT_FOUND_ERR;
        }
    }
    
    if (oldChild == newChild) {
        return OK;
    }
    
    /* check, whether newChild is node or one of node's ancestors */
    n = node;
    while (n) {
        if (n == newChild) {
            return HIERARCHY_REQUEST_ERR;
        }
        n = n->parentNode;
    }

    if (newChild == newChild->ownerDocument->rootNode) {
        if (newChild == node->ownerDocument->rootNode) {
            return HIERARCHY_REQUEST_ERR;
        } else {
            return NOT_SUPPORTED_ERR;
        }
    }

    /* unlink newChild */
    if (newChild->previousSibling) {
        newChild->previousSibling->nextSibling = newChild->nextSibling;
    } else {
        if (newChild->parentNode) {
            newChild->parentNode->firstChild = newChild->nextSibling;
        } else {
            /* newChild is either out of the fragment list or
               a child of the rootNode of its document */
            if (newChild->ownerDocument->fragments == newChild) {
                newChild->ownerDocument->fragments = newChild->nextSibling;
            } else {
                newChild->ownerDocument->rootNode->firstChild =
                    newChild->nextSibling;
            }
        }
    }
    if (newChild->nextSibling) {
        newChild->nextSibling->previousSibling = newChild->previousSibling;
    } else {
        if (newChild->parentNode) {
            newChild->parentNode->lastChild = newChild->previousSibling;
        } else {
            if (newChild->ownerDocument->rootNode->lastChild == newChild) {
                newChild->ownerDocument->rootNode->lastChild =
                    newChild->previousSibling;
            }
        }
    }

    newChild->nextSibling     = oldChild->nextSibling;
    newChild->previousSibling = oldChild->previousSibling;
    if (!newChild->parentNode &&
        (newChild->ownerDocument->documentElement == newChild)) {
        newChild->ownerDocument->documentElement =
            newChild->ownerDocument->rootNode->firstChild;
    }
    if (node == node->ownerDocument->rootNode) {
        newChild->parentNode  = NULL;
    } else {
        newChild->parentNode  = node;
    }
    if (oldChild->previousSibling) {
        oldChild->previousSibling->nextSibling = newChild;
    } else {
        node->firstChild = newChild;
    }
    if (oldChild->nextSibling) {
        oldChild->nextSibling->previousSibling = newChild;
    } else {
        node->lastChild = newChild;
    }

    if (node->ownerDocument != newChild->ownerDocument
        || node->ownerDocument->nsptr
        || newChild->ownerDocument->baseURIs->numEntries) {
        domSetDocument (newChild, node->ownerDocument);
    }

    /* add old child into his fragment list */
    if (oldChild->ownerDocument->fragments) {
        oldChild->nextSibling = oldChild->ownerDocument->fragments;
        oldChild->ownerDocument->fragments->previousSibling = oldChild;
        oldChild->ownerDocument->fragments = oldChild;
    } else {
        oldChild->ownerDocument->fragments = oldChild;
        oldChild->nextSibling = oldChild->previousSibling = NULL;
    }
    oldChild->parentNode = NULL;
    node->ownerDocument->nodeFlags |= NEEDS_RENUMBERING;
    MutationEvent();
    return OK;
}


/*---------------------------------------------------------------------------
|   domNewTextNode
|
\--------------------------------------------------------------------------*/
domTextNode *
domNewTextNode(
    domDocument *doc,
    const char  *value,
    int          length,
    domNodeType  nodeType	
)
{
    domTextNode   *node;

    node = (domTextNode*) domAlloc(sizeof(domTextNode));
    memset(node, 0, sizeof(domTextNode));
    node->nodeType      = nodeType;
    node->nodeFlags     = 0;
    node->namespace     = 0;
    node->nodeNumber    = NODE_NO(doc);
    node->ownerDocument = doc;
    node->valueLength   = length;
    node->nodeValue     = (char*)MALLOC(length);
    memmove(node->nodeValue, value, length);

    if (doc->fragments) {
        node->nextSibling = doc->fragments;
        doc->fragments->previousSibling = (domNode*)node;
        doc->fragments = (domNode*)node;
    } else {
        doc->fragments = (domNode*)node;

    }
    return node;
}



void
domEscapeCData (
    char        *value,
    int          length,
    Tcl_DString *escapedData
)
{
    int i, start = 0;
    char *pc;

    Tcl_DStringInit (escapedData);
    pc = value;
    for (i = 0; i < length; i++) {
        if (*pc == '&') {
            Tcl_DStringAppend (escapedData, &value[start], i - start);
            Tcl_DStringAppend (escapedData, "&amp;", 5);
            start = i+1;
        } else
        if (*pc == '<') {
            Tcl_DStringAppend (escapedData, &value[start], i - start);
            Tcl_DStringAppend (escapedData, "&lt;", 4);
            start = i+1;
        } else
        if (*pc == '>') {
            Tcl_DStringAppend (escapedData, &value[start], i - start);
            Tcl_DStringAppend (escapedData, "&gt;", 4);
            start = i+1;
        } 
        pc++;
    }
    if (start) {
        Tcl_DStringAppend (escapedData, &value[start], length - start);
    }
}


/*---------------------------------------------------------------------------
|   domAppendNewTextNode
|
\--------------------------------------------------------------------------*/
domTextNode *
domAppendNewTextNode(
    domNode     *parent,
    char        *value,
    int          length,
    domNodeType  nodeType,
    int          disableOutputEscaping
)
{
    domTextNode   *node;

    if (!length) {
        return NULL;
    }

    if (parent->lastChild
         && parent->lastChild->nodeType == TEXT_NODE
         && nodeType == TEXT_NODE
    ) {
        /*------------------------------------------------------------------
        |    append to already existing text node
        \-----------------------------------------------------------------*/
        domAppendData ((domTextNode *) (parent->lastChild), value, length,
                       disableOutputEscaping);
        MutationEvent();
        return (domTextNode*)parent->lastChild;
    }
    node = (domTextNode*) domAlloc(sizeof(domTextNode));
    memset(node, 0, sizeof(domTextNode));
    node->nodeType      = nodeType;
    node->nodeFlags     = 0;
    if (disableOutputEscaping) {
        node->nodeFlags |= DISABLE_OUTPUT_ESCAPING;
    }
    node->namespace     = 0;
    node->nodeNumber    = NODE_NO(parent->ownerDocument);
    node->ownerDocument = parent->ownerDocument;
    node->valueLength   = length;
    node->nodeValue     = (char*)MALLOC(length);
    memmove(node->nodeValue, value, length);

    if (parent->lastChild) {
        parent->lastChild->nextSibling = (domNode*)node;
        node->previousSibling          = parent->lastChild;
    } else {
        parent->firstChild    = (domNode*)node;
        node->previousSibling = NULL;
    }
    parent->lastChild = (domNode*)node;
    node->nextSibling = NULL;
    if (parent != parent->ownerDocument->rootNode) {
        node->parentNode  = parent;
    }

    MutationEvent();
    return node;
}


/*---------------------------------------------------------------------------
|   domAppendNewElementNode
|
\--------------------------------------------------------------------------*/
domNode *
domAppendNewElementNode(
    domNode     *parent,
    const char  *tagName,
    const char  *uri
)
{
    Tcl_HashEntry *h;
    domNode       *node;
    domNS         *ns;
    domAttrNode   *NSattr;
    int            hnew;
    char           prefix[MAX_PREFIX_LEN];
    const char    *localname;
    Tcl_DString    dStr;

    if (parent == NULL) { 
        DBG(fprintf(stderr, "dom.c: Error parent == NULL!\n");)
        return NULL;
    }

    h = Tcl_CreateHashEntry(&HASHTAB(parent->ownerDocument,tdom_tagNames),
                            tagName, &hnew);
    node = (domNode*) domAlloc(sizeof(domNode));
    memset(node, 0, sizeof(domNode));
    node->nodeType      = ELEMENT_NODE;
    node->nodeNumber    = NODE_NO(parent->ownerDocument);
    node->ownerDocument = parent->ownerDocument;
    node->nodeName      = (char *)&(h->key);

    if (parent->lastChild) {
        parent->lastChild->nextSibling = node;
        node->previousSibling          = parent->lastChild;
    } else {
        parent->firstChild    = node;
        node->previousSibling = NULL;
    }
    parent->lastChild = node;
    node->nextSibling = NULL;
    if (parent != parent->ownerDocument->rootNode) {
        node->parentNode  = parent;
    }

    /*--------------------------------------------------------
    |   re-use existing namespace or create a new one
    \-------------------------------------------------------*/
    if (uri) {
        domSplitQName (tagName, prefix, &localname);
        DBG(fprintf(stderr, "tag '%s' has prefix='%s' \n", tagName, prefix);)
        ns = domLookupPrefix (node, prefix);
        if (!ns || (strcmp (uri, ns->uri)!=0)) {
            ns = domNewNamespace(node->ownerDocument, prefix, uri);
            if (prefix[0] == '\0') {
                domSetAttributeNS (node, "xmlns", uri, NULL, 1);
            } else {
                Tcl_DStringInit (&dStr);
                Tcl_DStringAppend (&dStr, "xmlns:", 6);
                Tcl_DStringAppend (&dStr, prefix, -1);
                domSetAttributeNS (node, Tcl_DStringValue (&dStr), uri, NULL,
                                   1);
            }
        }
        node->namespace = ns->index;
    } else {
        ns = domLookupPrefix (node, "");
        if (ns) {
            if (strcmp (ns->uri, "")!=0) {
                NSattr = domSetAttributeNS (node, "xmlns", "", NULL, 1);
                if (NSattr) {
                    node->namespace = NSattr->namespace;
                }
            } else {
                node->namespace = ns->index;
            }
        }
    }
    MutationEvent();
    return node;
}


/*
 *----------------------------------------------------------------------
 *
 * domAppendData --
 *
 *      This procedure implements the dom method appendData. It is
 *      also used by domNormalize and domAppendNewTextNode.
 *
 * Results:
 *	A domException; currently always OK.
 *
 * Side effects:
 *	Appends the data to node.
 *
 *----------------------------------------------------------------------
 */

domException
domAppendData (
    domTextNode *node,          /* The node, to append value to. Must be
                                   a TEXT_NODE, COMMENT_NODE or 
                                   CDATA_SECTION_NODE*/
    char        *value,         /* The data to append */ 
    int          length,        /* The length of value in byte */
    int          disableOutputEscaping   /* If true, disable output 
                                            escaping on the node */
    )
{
    Tcl_DString    escData;

    if (node->nodeFlags & DISABLE_OUTPUT_ESCAPING) {
        if (disableOutputEscaping) {
            node->nodeValue = REALLOC (node->nodeValue,
                                        node->valueLength + length);
            memmove (node->nodeValue + node->valueLength, value, length);
            node->valueLength += length;
        } else {
            domEscapeCData (value, length, &escData);
            if (Tcl_DStringLength (&escData)) {
                node->nodeValue = REALLOC (node->nodeValue,
                                            node->valueLength +
                                            Tcl_DStringLength (&escData));
                memmove (node->nodeValue + node->valueLength,
                         Tcl_DStringValue (&escData),
                         Tcl_DStringLength (&escData));
                node->valueLength += Tcl_DStringLength (&escData);
            } else {
                node->nodeValue = REALLOC (node->nodeValue,
                                            node->valueLength+length);
                memmove (node->nodeValue + node->valueLength,
                         value, length);
                node->valueLength += length;
            }
            Tcl_DStringFree (&escData);
        }
    } else {
        if (disableOutputEscaping) {
            node->nodeFlags |= DISABLE_OUTPUT_ESCAPING;
            domEscapeCData (node->nodeValue, node->valueLength,
                            &escData);
            if (Tcl_DStringLength (&escData)) {
                FREE (node->nodeValue);
                node->nodeValue =
                    MALLOC (Tcl_DStringLength (&escData) + length);
                memmove (node->nodeValue, Tcl_DStringValue (&escData),
                         Tcl_DStringLength (&escData));
                node->valueLength = Tcl_DStringLength (&escData);
            } else {
                node->nodeValue = REALLOC (node->nodeValue,
                                            node->valueLength+length);
            }
            Tcl_DStringFree (&escData);
        } else {
            node->nodeValue = REALLOC (node->nodeValue,
                                        node->valueLength + length);
        }
        memmove (node->nodeValue + node->valueLength, value, length);
        node->valueLength += length;
    }

    return OK;
}


/*
 *----------------------------------------------------------------------
 *
 * domNormalize --
 *
 *      This procedure implements the dom method normalize. Puts all
 *      Text nodes in the full depth of the sub-tree underneath node,
 *      including attribute nodes, into a "normal" form where only
 *      structure (e.g., elements, comments, processing instructions,
 *      CDATA sections, and entity references) separates Text nodes,
 *      i.e., there are neither adjacent Text nodes nor empty Text
 *      nodes. If the flag forXPath is true, then CDATASection nodes
 *      are treated as if they are text nodes (and merged with
 *      circumjacent text nodes). Node must be an ELEMENT_NODE.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May alter the tree.
 *
 *----------------------------------------------------------------------
 */

void
domNormalize (
    domNode         *node,      /* root of the sub-tree to normalize */
    int              forXPath,  /* if true, treat CDATA_SECTION_NODEs as if
                                   they where TEXT_NODEs */
    domFreeCallback  freeCB,    /* Function to call, if a node must be freed */
    void            *clientData /* ClientData, to provide to the freeCB */
    
    )
{
    domNode     *child, *nextChild;
    int          merge = 0;
    
    if (node->nodeType != ELEMENT_NODE) return;
    
    child = node->firstChild;
    while (child) {
        merge = 0;
        switch (child->nodeType) {
        case ELEMENT_NODE:
            domNormalize (child, forXPath, freeCB, clientData);
            break;
        case TEXT_NODE:
            if (child->previousSibling 
                && child->previousSibling->nodeType == TEXT_NODE) {
                merge = 1;
            } else {
                if (((domTextNode *)child)->valueLength == 0) {
                    nextChild = child->nextSibling;
                    domDeleteNode (child, freeCB, clientData);
                    child = nextChild;
                    continue;
                }
            }
            break;
        case CDATA_SECTION_NODE:
            if (forXPath) {
                if (child->previousSibling
                    && child->previousSibling->nodeType == TEXT_NODE) {
                    merge = 1;
                } else {
                    if (((domTextNode *)child)->valueLength == 0) {
                        nextChild = child->nextSibling;
                        domDeleteNode (child, freeCB, clientData);
                        child = nextChild;
                        continue;
                    }
                    child->nodeType = TEXT_NODE;
                }
            }
            break;
        default:
            break;
        }
        if (merge) {
            domAppendData ( (domTextNode *)(child->previousSibling),
                            ((domTextNode *)child)->nodeValue,
                            ((domTextNode *)child)->valueLength,
                            (child->nodeFlags & DISABLE_OUTPUT_ESCAPING) );
            nextChild = child->nextSibling;
            domDeleteNode (child, freeCB, clientData);
            child = nextChild;
        } else {
            child = child->nextSibling;
        }
    }
}

/*---------------------------------------------------------------------------
|   domAddNSToNode
|
\--------------------------------------------------------------------------*/
domNS *
domAddNSToNode (
    domNode *node,
    domNS   *nsToAdd
    )
{
    domAttrNode   *attr, *lastNSAttr;
    domNS         *ns, noNS;
    Tcl_HashEntry *h;
    int            hnew;
    Tcl_DString    dStr;

    if (!nsToAdd) {
        noNS.uri    = "";
        noNS.prefix = "";
        noNS.index  = 0;
        nsToAdd = &noNS;
    }
    DBG(fprintf (stderr, "domAddNSToNode to node '%s': prefix: %s, uri: %s\n", node->nodeName, nsToAdd->prefix, nsToAdd->uri);)

    ns = domLookupPrefix (node, nsToAdd->prefix);
    if (ns) {
        if (strcmp (ns->uri, nsToAdd->uri)==0) {
            /* namespace already in scope, we're done. */
            return ns;
        }
    } else {
        /* If the NS to set was no NS and there isn't a default NS
           we're done */
        if (nsToAdd->prefix[0] == '\0' && nsToAdd->uri[0] == '\0') return NULL;
    }
    ns = domNewNamespace (node->ownerDocument, nsToAdd->prefix, nsToAdd->uri);
    Tcl_DStringInit (&dStr);
    if (nsToAdd->prefix[0] == '\0') {
        Tcl_DStringAppend (&dStr, "xmlns", 5);
    } else {
        Tcl_DStringAppend (&dStr, "xmlns:", 6);
        Tcl_DStringAppend (&dStr, nsToAdd->prefix, -1);
    }
    /* Add new namespace attribute */
    attr = (domAttrNode*) domAlloc(sizeof(domAttrNode));
    memset(attr, 0, sizeof(domAttrNode));
    h = Tcl_CreateHashEntry(&HASHTAB(node->ownerDocument,tdom_attrNames),
                            Tcl_DStringValue(&dStr), &hnew);
    attr->nodeType    = ATTRIBUTE_NODE;
    attr->nodeFlags   = IS_NS_NODE;
    attr->namespace   = ns->index;
    attr->nodeName    = (char *)&(h->key);
    attr->parentNode  = node;
    attr->valueLength = strlen(nsToAdd->uri);
    attr->nodeValue   = (char*)MALLOC(attr->valueLength+1);
    strcpy(attr->nodeValue, nsToAdd->uri);

    lastNSAttr = NULL;
    if (node->firstAttr && (node->firstAttr->nodeFlags & IS_NS_NODE)) {
        lastNSAttr = node->firstAttr;
        while (lastNSAttr->nextSibling
               && (lastNSAttr->nextSibling->nodeFlags & IS_NS_NODE)) {
            lastNSAttr = lastNSAttr->nextSibling;
        }
    }
    if (lastNSAttr) {
        attr->nextSibling = lastNSAttr->nextSibling;
        lastNSAttr->nextSibling = attr;
    } else {
        attr->nextSibling = node->firstAttr;
        node->firstAttr = attr;
    }
    Tcl_DStringFree (&dStr);
    return ns;
}

/*---------------------------------------------------------------------------
|   domAppendLiteralNode
|
\--------------------------------------------------------------------------*/
domNode *
domAppendLiteralNode(
    domNode     *parent,
    domNode     *literalNode
)
{
    Tcl_HashEntry *h;
    domNode       *node;
    int            hnew;

    if (parent == NULL) { 
        DBG(fprintf(stderr, "dom.c: Error parent == NULL!\n");)
        return NULL;
    }

    h = Tcl_CreateHashEntry(&HASHTAB(parent->ownerDocument, tdom_tagNames),
                             literalNode->nodeName, &hnew);
    node = (domNode*) domAlloc(sizeof(domNode));
    memset(node, 0, sizeof(domNode));
    node->nodeType      = ELEMENT_NODE;
    node->nodeNumber    = NODE_NO(parent->ownerDocument);
    node->ownerDocument = parent->ownerDocument;
    node->nodeName      = (char *)&(h->key);

    if (parent->lastChild) {
        parent->lastChild->nextSibling = node;
        node->previousSibling          = parent->lastChild;
    } else {
        parent->firstChild    = node;
        node->previousSibling = NULL;
    }
    parent->lastChild = node;
    node->nextSibling = NULL;
    if (parent != parent->ownerDocument->rootNode) {
        node->parentNode  = parent;
    }

    MutationEvent();
    return node;
}


/*---------------------------------------------------------------------------
|   domNewProcessingInstructionNode
|
\--------------------------------------------------------------------------*/
domProcessingInstructionNode *
domNewProcessingInstructionNode(
    domDocument *doc,
    const char  *targetValue,
    int          targetLength,
    const char  *dataValue,
    int          dataLength
)
{
    domProcessingInstructionNode   *node;

    node = (domProcessingInstructionNode*) domAlloc(sizeof(domProcessingInstructionNode));
    memset(node, 0, sizeof(domProcessingInstructionNode));
    node->nodeType      = PROCESSING_INSTRUCTION_NODE;
    node->nodeFlags     = 0;
    node->namespace     = 0;
    node->nodeNumber    = NODE_NO(doc);
    node->ownerDocument = doc;
    node->targetLength  = targetLength;
    node->targetValue   = (char*)MALLOC(targetLength);
    memmove(node->targetValue, targetValue, targetLength);

    node->dataLength    = dataLength;
    node->dataValue     = (char*)MALLOC(dataLength);
    memmove(node->dataValue, dataValue, dataLength);

    if (doc->fragments) {
        node->nextSibling = doc->fragments;
        doc->fragments->previousSibling = (domNode*)node;
        doc->fragments = (domNode*)node;
    } else {
        doc->fragments = (domNode*)node;

    }
    MutationEvent();
    return node;
}


/*---------------------------------------------------------------------------
|   domNewElementNode
|
\--------------------------------------------------------------------------*/
domNode *
domNewElementNode(
    domDocument *doc,
    const char  *tagName,
    domNodeType  nodeType		
)
{
    domNode       *node;
    Tcl_HashEntry *h;
    int           hnew;

    h = Tcl_CreateHashEntry(&HASHTAB(doc, tdom_tagNames), tagName, &hnew);
    node = (domNode*) domAlloc(sizeof(domNode));
    memset(node, 0, sizeof(domNode));
    node->nodeType      = nodeType;
    node->nodeFlags     = 0;
    node->namespace     = 0;
    node->nodeNumber    = NODE_NO(doc);
    node->ownerDocument = doc;
    node->nodeName      = (char *)&(h->key);

    if (doc->fragments) {
        node->nextSibling = doc->fragments;
        doc->fragments->previousSibling = node;
        doc->fragments = node;
    } else {
        doc->fragments = node;

    }
    return node;
}


/*---------------------------------------------------------------------------
|   domNewElementNodeNS
|
\--------------------------------------------------------------------------*/
domNode *
domNewElementNodeNS (
    domDocument *doc,
    const char  *tagName,
    const char  *uri,
    domNodeType  nodeType		
)
{
    domNode       *node;
    Tcl_HashEntry *h;
    int            hnew;
    char           prefix[MAX_PREFIX_LEN];
    const char    *localname;
    domNS         *ns;

    h = Tcl_CreateHashEntry(&HASHTAB(doc, tdom_tagNames), tagName, &hnew);
    node = (domNode*) domAlloc(sizeof(domNode));
    memset(node, 0, sizeof(domNode));
    node->nodeType      = nodeType;
    node->nodeFlags     = 0;
    node->namespace     = 0;
    node->nodeNumber    = NODE_NO(doc);
    node->ownerDocument = doc;
    node->nodeName      = (char *)&(h->key);

    domSplitQName (tagName, prefix, &localname);
    ns = domNewNamespace(doc, prefix, uri);
    node->namespace = ns->index;

    if (doc->fragments) {
        node->nextSibling = doc->fragments;
        doc->fragments->previousSibling = node;
        doc->fragments = node;
    } else {
        doc->fragments = node;

    }
    return node;
}

/*---------------------------------------------------------------------------
|   domCloneNode
|
\--------------------------------------------------------------------------*/
domNode *
domCloneNode (
    domNode *node,
    int      deep
)
{
    domAttrNode *attr, *nattr;
    domNode     *n, *child, *newChild;

    /*------------------------------------------------------------------
    |   create new node
    \-----------------------------------------------------------------*/
    if (node->nodeType == PROCESSING_INSTRUCTION_NODE) {
        domProcessingInstructionNode *pinode = (domProcessingInstructionNode*)node;
        return (domNode*) domNewProcessingInstructionNode(
                                         pinode->ownerDocument,
                                         pinode->targetValue,
                                         pinode->targetLength,
                                         pinode->dataValue,
                                         pinode->dataLength);
    }
    if (node->nodeType != ELEMENT_NODE) {
        domTextNode *tnode = (domTextNode*)node;
        return (domNode*) domNewTextNode(tnode->ownerDocument,
                                         tnode->nodeValue, tnode->valueLength,
					 tnode->nodeType);
    }

    n = domNewElementNode(node->ownerDocument, node->nodeName, node->nodeType);
    n->namespace = node->namespace;


    /*------------------------------------------------------------------
    |   copy attributes (if any)
    \-----------------------------------------------------------------*/
    attr = node->firstAttr;
    while (attr != NULL) {
        nattr = domSetAttribute (n, attr->nodeName, attr->nodeValue );
        nattr->namespace = attr->namespace;
        if (attr->nodeFlags & IS_NS_NODE) {
            nattr->nodeFlags |= IS_NS_NODE;
        }
        attr = attr->nextSibling;
    }

    if (deep) {
        child = node->firstChild;
        while (child) {
            newChild = domCloneNode(child, deep);

            /* append new (cloned)child to cloned node, its new parent.
               Don't use domAppendChild for this, because that would
               mess around with the namespaces */
            if (n->ownerDocument->fragments->nextSibling) {
                n->ownerDocument->fragments = 
                    n->ownerDocument->fragments->nextSibling;
                n->ownerDocument->fragments->previousSibling = NULL;
                newChild->nextSibling = NULL;
            } else {
                n->ownerDocument->fragments = NULL;
            }
            if (n->firstChild) {
                newChild->previousSibling = n->lastChild;
                n->lastChild->nextSibling = newChild;
            } else {
                n->firstChild = newChild;
            }
            n->lastChild = newChild;
            newChild->parentNode = n;

            /* clone next child */
            child = child->nextSibling;
        }
    }
    return n;
}

/*----------------------------------------------------------------------------
|   domCopyNS
|
\---------------------------------------------------------------------------*/
void
domCopyNS (
    domNode *from,
    domNode *to
    )
{
    domNode     *n, *n1;
    domNS       *ns, *ns1;
    domAttrNode *attr, *attr1;
    int          skip;

    n = from;
    while (n) {
        attr = n->firstAttr;
        while (attr && (attr->nodeFlags & IS_NS_NODE)) {
            ns = n->ownerDocument->namespaces[attr->namespace-1];
            skip = 0;
            n1 = from;
            while (n1 != n) {
                attr1 = n1->firstAttr;
                while (attr1 && (attr1->nodeFlags & IS_NS_NODE)) {
                    ns1 = n1->ownerDocument->namespaces[attr1->namespace-1];
                    if ((ns1->prefix == NULL && ns->prefix == NULL) 
                         || (strcmp (ns1->prefix, ns->prefix)==0)) {
                        skip = 1;
                        break;
                    }
                    attr1 = attr1->nextSibling;
                }
                if (skip) break;
                n1 = n1->parentNode;
            }
            if (!skip) {
                /* Add this prefix/uri combination only to the
                   destination, if it isn't already in scope */
                ns1 = domLookupPrefix (to, ns->prefix);
                if (!ns1 || (strcmp (ns->uri, ns1->uri)!=0)) {
                    domAddNSToNode (to, ns);
                }
            }
            attr = attr->nextSibling;
        }
        n = n->parentNode;
    }
}


/*---------------------------------------------------------------------------
|   domCopyTo
|
\--------------------------------------------------------------------------*/
void
domCopyTo (
    domNode *node,
    domNode *parent,
    int      copyNS
)
{
    domAttrNode *attr, *nattr;
    domNode     *n, *child;
    domNS       *ns, *ns1;

    /*------------------------------------------------------------------
    |   create new node
    \-----------------------------------------------------------------*/
    if (node->nodeType == PROCESSING_INSTRUCTION_NODE) {
        domProcessingInstructionNode *pinode = (domProcessingInstructionNode*)node;
        n = (domNode*) domNewProcessingInstructionNode(
                                         parent->ownerDocument,
                                         pinode->targetValue,
                                         pinode->targetLength,
                                         pinode->dataValue,
                                         pinode->dataLength);
        domAppendChild (parent, n);
        return;
    }
    if (node->nodeType != ELEMENT_NODE) {
        domTextNode *tnode = (domTextNode*)node;
        n =  (domNode*) domNewTextNode(parent->ownerDocument,
                                         tnode->nodeValue, tnode->valueLength,
					 tnode->nodeType);
        domAppendChild (parent, n);
        return;
    }

    n = domAppendLiteralNode (parent, node);
    if (copyNS) {
        domCopyNS (node, n);
    }
    
    /*------------------------------------------------------------------
    |   copy attributes (if any)
    \-----------------------------------------------------------------*/
    attr = node->firstAttr;
    while (attr != NULL) {
        if (attr->nodeFlags & IS_NS_NODE) {
            if (copyNS) {
                /* If copyNS is true, then all namespaces in scope
                 * (including the one declared with the node to copy)
                 * are allready copied over. */
                attr = attr->nextSibling;
                continue;
                
            }
            ns = node->ownerDocument->namespaces[attr->namespace-1];
            ns1 = domLookupPrefix (n, ns->prefix);
            if (ns1 && strcmp (ns->uri, ns1->uri)==0) {
                /* This namespace is already in scope, so we
                   don't copy the namespace attribute over */
                attr = attr->nextSibling;
                continue;
            }
            nattr = domSetAttribute (n, attr->nodeName, attr->nodeValue );
            nattr->nodeFlags = attr->nodeFlags;
            ns1 = domNewNamespace (n->ownerDocument, ns->prefix, ns->uri);
            nattr->namespace = ns1->index;
        } else {
            nattr = domSetAttribute (n, attr->nodeName, attr->nodeValue );
            nattr->nodeFlags = attr->nodeFlags;
            if (attr->namespace) {
                ns = node->ownerDocument->namespaces[attr->namespace-1];
                ns1 = domLookupPrefix (n, ns->prefix);
                if (ns1) {
                    nattr->namespace = ns1->index;
                }
            }
        }
        attr = attr->nextSibling;
    }

    /* We have to set the node namespace index after copying the
       attribute nodes over, because the node may be in a namespace,
       which is declared just at the node. */
    if (node->namespace) {
        ns = node->ownerDocument->namespaces[node->namespace-1];
        ns1 = domLookupPrefix (n, ns->prefix);
        n->namespace = ns1->index;
    }

    child = node->firstChild;
    while (child) {
        domCopyTo(child, n, 0);
        child = child->nextSibling;
    }
}


/*---------------------------------------------------------------------------
|   domXPointerChild
|
\--------------------------------------------------------------------------*/
int
domXPointerChild (
    domNode      * node,
    int            all,
    int            instance,
    domNodeType    type,
    char         * element,
    char         * attrName,
    char         * attrValue,
    int            attrLen,
    domAddCallback addCallback,
    void         * clientData
)
{
    domNode     *child;
    domAttrNode *attr;
    int          i=0, result;


    if (node->nodeType != ELEMENT_NODE) {
        return 0;
    }

    if (instance<0) {
        child = node->lastChild;
    } else {
        child = node->firstChild;
    }
    while (child) {
        if ((type == ALL_NODES) || (child->nodeType == type)) {
            if ((element == NULL) ||
                ((child->nodeType == ELEMENT_NODE) && (strcmp(child->nodeName,element)==0))
               ) {
                if (attrName == NULL) {
                    i = (instance<0) ? i-1 : i+1;
                    if (all || (i == instance)) {
                        result = addCallback (child, clientData);
                        if (result) {
                            return result;
                        }
                    }
                } else {
                    attr = child->firstAttr;
                    while (attr) {
                        if ((strcmp(attr->nodeName,attrName)==0) &&
                            ( (strcmp(attrValue,"*")==0) ||
                              ( (attr->valueLength == attrLen) &&
                               (strcmp(attr->nodeValue,attrValue)==0)
                              )
                            )
                           ) {
                            i = (instance<0) ? i-1 : i+1;
                            if (all || (i == instance)) {
                                result = addCallback (child, clientData);
                                if (result) {
                                    return result;
                                }
                            }
                        }
                        attr = attr->nextSibling;
                    }
                }
            }
        }
        if (instance<0) {
            child = child->previousSibling;
        } else {
            child = child->nextSibling;
        }
    }
    return 0;
}


/*---------------------------------------------------------------------------
|   domXPointerXSibling
|
\--------------------------------------------------------------------------*/
int
domXPointerXSibling (
    domNode      * node,
    int            forward_mode,
    int            all,
    int            instance,
    domNodeType    type,
    char         * element,
    char         * attrName,
    char         * attrValue,
    int            attrLen,
    domAddCallback addCallback,
    void         * clientData
)
{
    domNode     *sibling, *endSibling;
    domAttrNode *attr;
    int          i=0, result;


    if (forward_mode) {
        if (instance<0) {
            endSibling = node;
            sibling = node;
            if (node->parentNode) {
                sibling = node->parentNode->lastChild;
            }
        } else {
            sibling = node->nextSibling;
            endSibling = NULL;
        }
    } else {
        if (instance<0) {
            endSibling = node;
            sibling = node;
            if (node->parentNode) {
                sibling = node->parentNode->firstChild;
            }
        } else {
            sibling = node->previousSibling;
            endSibling = NULL;
        }
        instance = -1 * instance;
    }

    while (sibling != endSibling) {
        if ((type == ALL_NODES) || (sibling->nodeType == type)) {
            if ((element == NULL) ||
                ((sibling->nodeType == ELEMENT_NODE) && (strcmp(sibling->nodeName,element)==0))
               ) {
                if (attrName == NULL) {
                    i = (instance<0) ? i-1 : i+1;
                    if (all || (i == instance)) {
                        result = addCallback (sibling, clientData);
                        if (result) {
                            return result;
                        }
                    }
                } else {
                    attr = sibling->firstAttr;
                    while (attr) {
                        if ((strcmp(attr->nodeName,attrName)==0) &&
                            ( (strcmp(attrValue,"*")==0) ||
                              ( (attr->valueLength == attrLen) &&
                                (strcmp(attr->nodeValue,attrValue)==0)
                              )
                            )
                           ) {
                            i = (instance<0) ? i-1 : i+1;
                            if (all || (i == instance)) {
                                result = addCallback (sibling, clientData);
                                if (result) {
                                    return result;
                                }
                            }
                        }
                        attr = attr->nextSibling;
                    }
                }
            }
        }
        if (instance<0) {
            sibling = sibling->previousSibling;
        } else {
            sibling = sibling->nextSibling;
        }
    }
    return 0;
}


/*---------------------------------------------------------------------------
|   domXPointerDescendant
|
\--------------------------------------------------------------------------*/
int
domXPointerDescendant (
    domNode      * node,
    int            all,
    int            instance,
    int          * i,
    domNodeType    type,
    char         * element,
    char         * attrName,
    char         * attrValue,
    int            attrLen,
    domAddCallback addCallback,
    void         * clientData
)
{
    domNode     *child;
    domAttrNode *attr;
    int          found=0, result;


    if (node->nodeType != ELEMENT_NODE) {
        return 0;
    }

    if (instance<0) {
        child = node->lastChild;
    } else {
        child = node->firstChild;
    }
    while (child) {
        found = 0;
        if ((type == ALL_NODES) || (child->nodeType == type)) {
            if ((element == NULL) ||
                ((child->nodeType == ELEMENT_NODE) && (strcmp(child->nodeName,element)==0))
               ) {
                if (attrName == NULL) {
                    *i = (instance<0) ? (*i)-1 : (*i)+1;
                    if (all || (*i == instance)) {
                        result = addCallback (child, clientData);
                        if (result) {
                            return result;
                        }
                        found = 1;
                    }
                } else {
                    attr = child->firstAttr;
                    while (attr) {
                        if ((strcmp(attr->nodeName,attrName)==0) &&
                            ( (strcmp(attrValue,"*")==0) ||
                              ( (attr->valueLength == attrLen) &&
                               (strcmp(attr->nodeValue,attrValue)==0)
                              )
                            )
                           ) {
                            *i = (instance<0) ? (*i)-1 : (*i)+1;
                            if (all || (*i == instance)) {
                                result = addCallback (child, clientData);
                                if (result) {
                                    return result;
                                }
                                found = 1;
                            }
                        }
                        attr = attr->nextSibling;
                    }
                }
            }
        }
        if (!found) {
            /* recurs into childs */
            result = domXPointerDescendant (child, all, instance, i,
                                            type, element, attrName,
                                            attrValue, attrLen,
                                            addCallback, clientData);
            if (result) {
                return result;
            }
        }
        if (instance<0) {
            child = child->previousSibling;
        } else {
            child = child->nextSibling;
        }
    }
    return 0;
}


/*---------------------------------------------------------------------------
|   domXPointerAncestor
|
\--------------------------------------------------------------------------*/
int
domXPointerAncestor (
    domNode      * node,
    int            all,
    int            instance,
    int          * i,
    domNodeType    type,
    char         * element,
    char         * attrName,
    char         * attrValue,
    int            attrLen,
    domAddCallback addCallback,
    void         * clientData
)
{
    domNode     *ancestor;
    domAttrNode *attr;
    int          found=0, result;


    ancestor = node->parentNode;
    if (ancestor) {
        found = 0;
        if ((type == ALL_NODES) || (ancestor->nodeType == type)) {
            if ((element == NULL) ||
                ((ancestor->nodeType == ELEMENT_NODE) && (strcmp(ancestor->nodeName,element)==0))
               ) {
                if (attrName == NULL) {
                    *i = (instance<0) ? (*i)-1 : (*i)+1;
                    if (all || (*i == instance)) {
                        result = addCallback (ancestor, clientData);
                        if (result) {
                            return result;
                        }
                        found = 1;
                    }
                } else {
                    attr = ancestor->firstAttr;
                    while (attr) {
                        if ((strcmp(attr->nodeName,attrName)==0) &&
                            ( (strcmp(attrValue,"*")==0) ||
                              ( (attr->valueLength == attrLen) &&
                               (strcmp(attr->nodeValue,attrValue)==0)
                              )
                            )
                           ) {
                            *i = (instance<0) ? (*i)-1 : (*i)+1;
                            if (all || (*i == instance)) {
                                result = addCallback (ancestor, clientData);
                                if (result) {
                                    return result;
                                }
                                found = 1;
                            }
                        }
                        attr = attr->nextSibling;
                    }
                }
            }
        }

        /* go up */
        result = domXPointerAncestor (ancestor, all, instance, i,
                                      type, element, attrName,
                                      attrValue, attrLen,
                                      addCallback, clientData);
        if (result) {
            return result;
        }
    }
    return 0;
}



/*---------------------------------------------------------------------------
|   type tdomCmdReadInfo
|
\--------------------------------------------------------------------------*/
typedef struct _tdomCmdReadInfo {

    XML_Parser        parser;
    domDocument      *document;
    domNode          *currentNode;
    int               depth;
    int               ignoreWhiteSpaces;
    Tcl_DString      *cdata;
    TEncoding        *encoding_8bit;
    int               storeLineColumn;
    int               feedbackAfter;
    int               lastFeedbackPosition;
    Tcl_Interp       *interp;
    int               activeNSsize;
    int               activeNSpos;
    domActiveNS      *activeNS;
    int               baseURIstackSize;
    int               baseURIstackPos;
    domActiveBaseURI *baseURIstack;
    int               insideDTD;
    /* Now the tdom cmd specific elements */
    int               tdomStatus;
    Tcl_Obj          *extResolver;

} tdomCmdReadInfo;

EXTERN int tcldom_returnDocumentObj (Tcl_Interp *interp, 
                                     domDocument *document,
                                     int setVariable, Tcl_Obj *var_name,
                                     int trace, int forOwnerDocument);

void
tdom_freeProc (
    Tcl_Interp *interp,
    void       *userData
)
{
    tdomCmdReadInfo *info = (tdomCmdReadInfo *) userData;

    if (info->document) {
        domFreeDocument (info->document, NULL, NULL);
    }
    if (info->activeNS) {
        FREE (info->activeNS);
    }
    if (info->baseURIstack) {
        FREE (info->baseURIstack);
    }
        
    Tcl_DStringFree (info->cdata);
    FREE (info->cdata);
    if (info->extResolver) {
        Tcl_DecrRefCount (info->extResolver);
    }
    FREE (info);
}

void
tdom_parserResetProc (
    XML_Parser parser,
    void      *userData
)
{
    tdomCmdReadInfo *info = (tdomCmdReadInfo *) userData;

    info->parser = parser;
}

void
tdom_resetProc (
    Tcl_Interp *interp,
    void       *userData
)
{
    tdomCmdReadInfo *info = (tdomCmdReadInfo *) userData;

    if (!info->tdomStatus) return;

    if (info->document) {
        domFreeDocument (info->document, NULL, NULL);
    }

    info->document          = NULL;
    info->currentNode       = NULL;
    info->depth             = 0;
    info->feedbackAfter     = 0;
    Tcl_DStringSetLength (info->cdata, 0);
    info->lastFeedbackPosition = 0;
    info->interp            = interp;
    info->activeNSpos       = -1;
    info->insideDTD         = 0;
    info->baseURIstackPos   = 0;
    info->tdomStatus        = 0;

}

void
tdom_initParseProc (
    Tcl_Interp *interp,
    void       *userData
    )
{
    tdomCmdReadInfo *info = (tdomCmdReadInfo *) userData;

    info->document   = domCreateDoc(XML_GetBase (info->parser), 
                                    info->storeLineColumn);
    if (info->extResolver) {
        info->document->extResolver = 
            tdomstrdup (Tcl_GetString (info->extResolver));
    }
    info->baseURIstack[0].baseURI = XML_GetBase (info->parser);
    info->baseURIstack[0].depth = 0;
    info->tdomStatus = 2;
    
}

static void
tdom_charDataHandler (
    void        *userData,
    const char  *s,
    int          len
)
{
    domReadInfo   *info = userData;

    Tcl_DStringAppend (info->cdata, s, len);
    DispatchPCDATA (info);
    return;
}

int
TclTdomObjCmd (dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
    char            *method, *encodingName;
    CHandlerSet     *handlerSet;
    int              methodIndex, result, bool;
    tdomCmdReadInfo *info;
    TclGenExpatInfo *expat;
    Tcl_Obj         *newObjName = NULL;
    TEncoding       *encoding;

    static CONST84 char *tdomMethods[] = {
        "enable", "getdoc",
        "setResultEncoding", "setStoreLineColumn",
        "setExternalEntityResolver", "keepEmpties",
        "remove",
        NULL
    };
    enum tdomMethod {
        m_enable, m_getdoc,
        m_setResultEncoding, m_setStoreLineColumn,
        m_setExternalEntityResolver, m_keepEmpties,
        m_remove
    };

    if (objc < 3 || objc > 4) {
        Tcl_WrongNumArgs (interp, 1, objv, tdom_usage);
        return TCL_ERROR;
    }

    if (!CheckExpatParserObj (interp, objv[1])) {
        Tcl_SetResult (interp, "First argument has to be a expat parser object", NULL);
        return TCL_ERROR;
    }

    method = Tcl_GetString(objv[2]);
    if (Tcl_GetIndexFromObj (interp, objv[2], tdomMethods, "method", 0,
                             &methodIndex) != TCL_OK)
    {
        Tcl_SetResult (interp, tdom_usage, NULL);
        return TCL_ERROR;
    }

    switch ((enum tdomMethod) methodIndex) {

    default:
        Tcl_SetResult (interp, "unknown method", NULL);
        return TCL_ERROR;

    case m_enable:
        handlerSet = CHandlerSetCreate ("tdom");
        handlerSet->ignoreWhiteCDATAs       = 1;
        handlerSet->resetProc               = tdom_resetProc;
        handlerSet->freeProc                = tdom_freeProc;
        handlerSet->parserResetProc         = tdom_parserResetProc;
        handlerSet->initParseProc           = tdom_initParseProc;
        handlerSet->elementstartcommand     = startElement;
        handlerSet->elementendcommand       = endElement;
        handlerSet->datacommand             = tdom_charDataHandler;
/*         handlerSet->datacommand             = characterDataHandler; */
        handlerSet->commentCommand          = commentHandler;
        handlerSet->picommand               = processingInstructionHandler;
        handlerSet->entityDeclCommand       = entityDeclHandler;
        handlerSet->startDoctypeDeclCommand = startDoctypeDeclHandler;
        handlerSet->endDoctypeDeclCommand   = endDoctypeDeclHandler;

        expat = GetExpatInfo (interp, objv[1]);

        info = (tdomCmdReadInfo *) MALLOC (sizeof (tdomCmdReadInfo));
        info->parser            = expat->parser;
        info->document          = NULL;
        info->currentNode       = NULL;
        info->depth             = 0;
        info->ignoreWhiteSpaces = 1;
        info->cdata             = (Tcl_DString*) MALLOC (sizeof (Tcl_DString));
        Tcl_DStringInit (info->cdata);
        info->encoding_8bit     = 0;
        info->storeLineColumn   = 0;
        info->feedbackAfter     = 0;
        info->lastFeedbackPosition = 0;
        info->interp            = interp;
        info->activeNSpos       = -1;
        info->activeNSsize      = 8;
        info->activeNS          = 
            (domActiveNS*) MALLOC(sizeof(domActiveNS) * info->activeNSsize);
        info->baseURIstackPos   = 0;
        info->baseURIstackSize  = INITIAL_BASEURISTACK_SIZE;
        info->baseURIstack      = (domActiveBaseURI*) 
            MALLOC (sizeof(domActiveBaseURI) * info->baseURIstackSize);
        info->insideDTD         = 0;
        info->tdomStatus        = 0;
        info->extResolver       = NULL;

        handlerSet->userData    = info;

        CHandlerSetInstall (interp, objv[1], handlerSet);
        break;
        
    case m_getdoc:
        info = CHandlerSetGetUserData (interp, objv[1], "tdom");
        if (!info) {
            Tcl_SetResult (interp, "parser object isn't tdom enabled.", NULL);
            return TCL_ERROR;
        }
        expat = GetExpatInfo (interp, objv[1]);
        if (info->tdomStatus != 2 || !expat->finished) {
            Tcl_SetResult (interp, "No DOM tree avaliable.", NULL);
            return TCL_ERROR;
        }
        domSetDocumentElement (info->document);
        result = tcldom_returnDocumentObj (interp, info->document, 0,
                                           newObjName, 0, 0);
        info->document = NULL;
        return result;

    case m_setResultEncoding:
        info = CHandlerSetGetUserData (interp, objv[1], "tdom");
        if (!info) {
            Tcl_SetResult (interp, "parser object isn't tdom enabled.", NULL);
            return TCL_ERROR;
        }
        if (info->encoding_8bit == NULL) {
            Tcl_AppendResult (interp, "UTF-8", NULL);
        }
        else {
            Tcl_AppendResult (interp,
                              tdom_GetEncodingName (info->encoding_8bit),
                              NULL);
        }
        if (objc == 4) {
            encodingName = Tcl_GetString(objv[3]);

            if (   (strcmp(encodingName, "UTF-8") == 0)
                 ||(strcmp(encodingName, "UTF8" ) == 0)
                 ||(strcmp(encodingName, "utf-8") == 0)
                 ||(strcmp(encodingName, "utf8" ) == 0)) {

                info->encoding_8bit = NULL;
            } else {
                encoding = tdom_GetEncoding ( encodingName );
                if (encoding == NULL) {
                    Tcl_AppendResult(interp, "encoding not found", NULL);
                    return TCL_ERROR;
                }
                info->encoding_8bit = encoding;
            }
        }
        info->tdomStatus = 1;
        break;
        
    case m_setStoreLineColumn:
        info = CHandlerSetGetUserData (interp, objv[1], "tdom");
        if (!info) {
            Tcl_SetResult (interp, "parser object isn't tdom enabled.", NULL);
            return TCL_ERROR;
        }
        Tcl_SetIntObj (Tcl_GetObjResult (interp), info->storeLineColumn);
        if (objc == 4) {
            Tcl_GetBooleanFromObj (interp, objv[3], &bool);
            info->storeLineColumn = bool;
        }
        info->tdomStatus = 1;
        break;
        
    case m_remove:
        result = CHandlerSetRemove (interp, objv[1], "tdom");
        if (result == 2) {
            Tcl_SetResult (interp, "expat parser obj hasn't a C handler set named \"tdom\"", NULL);
            return TCL_ERROR;
        }
        break;

    case m_setExternalEntityResolver:
        if (objc != 4) {
            Tcl_SetResult (interp, "You must name a tcl command as external entity resolver for setExternalEntityResolver.", NULL);
            return TCL_ERROR;
        }
        info = CHandlerSetGetUserData (interp, objv[1], "tdom");
        if (!info) {
            Tcl_SetResult (interp, "parser object isn't tdom enabled.", NULL);
            return TCL_ERROR;
        }
        if (info->extResolver) {
            Tcl_DecrRefCount (info->extResolver);
        }
        if (strcmp (Tcl_GetString (objv[3]), "") == 0) {
            info->extResolver = NULL;
        } else {
            info->extResolver = objv[3];
            Tcl_IncrRefCount (info->extResolver);
        }
        info->tdomStatus = 1;
        break;

    case m_keepEmpties:
        if (objc != 4) {
            Tcl_SetResult (interp, "wrong # of args for method keepEmpties.",
                           NULL);
            return TCL_ERROR;
        }
        handlerSet = CHandlerSetGet (interp, objv[1], "tdom");
        info = handlerSet->userData;
        if (!info) {
            Tcl_SetResult (interp, "parser object isn't tdom enabled.", NULL);
            return TCL_ERROR;
        }
        Tcl_SetIntObj (Tcl_GetObjResult (interp), info->ignoreWhiteSpaces);
        Tcl_GetBooleanFromObj (interp, objv[3], &bool);
        info->ignoreWhiteSpaces = !bool;
        handlerSet->ignoreWhiteCDATAs = !bool;
        info->tdomStatus = 1;
        break;
    }

    return TCL_OK;
}
