/*----------------------------------------------------------------------------
|   Copyright (c) 1999  Jochen Loewer (loewerj@hotmail.com)
|-----------------------------------------------------------------------------
|
|
|   A simple (hopefully fast) parser to build up a DOM structure in memory.
|   Initially based on Richard Hipp's XML parser for TMML.
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
|
|       June00  Zoran Vasiljevic  Made thread-safe.
|
|
|   adopted/written by Jochen Loewer
|   July 1999
|
|   ------------------------------------------------------------------------
|
|   A parser for XML.
|
|   Copyright (C) 1998 D. Richard Hipp
|
|   This library is free software; you can redistribute it and/or
|   modify it under the terms of the GNU Library General Public
|   License as published by the Free Software Foundation; either
|   version 2 of the License, or (at your option) any later version.
|
|   This library is distributed in the hope that it will be useful,
|   but WITHOUT ANY WARRANTY; without even the implied warranty of
|   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
|   Library General Public License for more details.
|
|   You should have received a copy of the GNU Library General Public
|   License along with this library; if not, write to the
|   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
|   Boston, MA  02111-1307, USA.
|
|   Author contact information:
|     drh@acm.org
|     http://www.hwaci.com/drh/
|
\---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
|   Includes
|
\---------------------------------------------------------------------------*/
#include <tcl.h>
#include <string.h>
#include <ctype.h>
#include <dom.h>

/*----------------------------------------------------------------------------
|   Defines
|
\---------------------------------------------------------------------------*/
#define DBG(x)          
#define TDOM_NS
#ifdef TDOM_NS
# define RetError(m,p) *errStr=m; *pos=p; FREE((char*)activeNS); return TCL_ERROR;
#else 
# define RetError(m,p) *errStr=m; *pos=p; return TCL_ERROR;
#endif
#define SPACE(c)       ((c)==' ' || (c)=='\n' || (c)=='\t' || (c)=='\r')

/*---------------------------------------------------------------------------
|   type domActiveNS
|
\--------------------------------------------------------------------------*/
typedef struct _domActiveNS {

    int    depth;
    domNS *namespace;

} domActiveNS;

/*----------------------------------------------------------------------------
|   Begin Character Entity Translator
|
|
|   The next section of code implements routines used to translate
|   character entity references into their corresponding strings.
|
|   Examples:
|
|         &amp;          "&"
|         &lt;           "<"
|         &gt;           ">"
|         &nbsp;         " "
|
\---------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
|   Each entity reference is recorded as an instance of the following
|   structure
\---------------------------------------------------------------------------*/
typedef struct Er Er;
struct Er {
    char *zName;     /* The name of this entity reference.  ex:  "amp" */
    char *zValue;    /* The value for this entity.          ex:  "&"   */
    Er *pNext;       /* Next entity with the same hash on zName        */
};


/*----------------------------------------------------------------------------
|   The size of the hash table.  For best results this should
|   be a prime number which is about the same size as the number of
|   character entity references known to the system.
|
\---------------------------------------------------------------------------*/
#define ER_HASH_SIZE 7


/*----------------------------------------------------------------------------
|   The following flag is TRUE if entity reference hash table needs
|   to be initialized.
|
|   Hash table is used read-only, therefore just one copy, protected with
|   mutex when used in threading environments. The mutex is used only for
|   initial setup of the table.
|
\---------------------------------------------------------------------------*/
static int bErNeedsInit = 1;
TDomThreaded(static Tcl_Mutex initMutex;)


/*----------------------------------------------------------------------------
|   The hash table
|
|   If the name of an entity reference hashes to the value H, then
|   apErHash[H] will point to a linked list of Er structures, one of
|   which will be the Er structure for that entity reference
|
\---------------------------------------------------------------------------*/
static Er *apErHash[ER_HASH_SIZE];


/*----------------------------------------------------------------------------
|   ErHash  --
|
|       Hash an entity reference name.  The value returned is an
|       integer between 0 and Er_HASH_SIZE-1, inclusive.
|
\---------------------------------------------------------------------------*/
static int ErHash(
    const char *zName
)
{
    int h = 0;      /* The hash value to be returned */
    char c;         /* The next character in the name being hashed */

    while( (c=*zName)!=0 ){
        h = h<<5 ^ h ^ c;
        zName++;
    }
    if( h<0 ) h = -h;
    return h % ER_HASH_SIZE;

} /* ErHash */


/*----------------------------------------------------------------------------
|   The following is a table of all entity references.  To create
|   new character entities, add entries to this table.
|
|   Note: For the decoder to work, the name of the entity reference
|   must not be shorter than the value.
|
\---------------------------------------------------------------------------*/
static Er er_sequences[] = {
    { "amp",       "&",        0 },
    { "lt",        "<",        0 },
    { "gt",        ">",        0 },
    { "apos",      "'",        0 },
    { "quot",      "\"",       0 },
#if TclOnly8Bits
    { "nbsp",      "\240",     0 },
#else
    { "nbsp",      "\xC2\xA0",    0 },
#endif
};


/*----------------------------------------------------------------------------
|   ErInit --
|
|       Initialize the entity reference hash table
|
\---------------------------------------------------------------------------*/
static void ErInit (void)
{
    size_t i;  /* For looping thru the list of entity references */
    int h;  /* The hash on a entity */

    for(i=0; i<sizeof(er_sequences)/sizeof(er_sequences[0]); i++){
        h = ErHash(er_sequences[i].zName);
        er_sequences[i].pNext = apErHash[h];
        apErHash[h] = &er_sequences[i];
    }

} /* ErInit */


/*----------------------------------------------------------------------------
|    TranslateEntityRefs  --
|
|        Translate entity references and character references in the string
|        "z".  "z" is overwritten with the translated sequence.
|
|        Unrecognized entity references are unaltered.
|
|        Example:
|
|          input =    "AT&amp;T &gt MCI"
|          output =   "AT&T > MCI"
|
\---------------------------------------------------------------------------*/
static int TranslateEntityRefs (
    char *z,
    int  *newLen
)
{
    int from;    /* Read characters from this position in z[] */
    int to;      /* Write characters into this position in z[] */
    int h;       /* A hash on the entity reference */
    char *zVal;  /* The substituted value */
    Er *p;       /* For looping down the entity reference collision chain */
    int value;

    from = to = 0;

    /*---------------------------------------------
     |   This is done only once per process
     \--------------------------------------------*/

    if (bErNeedsInit) {
        TDomThreaded(Tcl_MutexLock(&initMutex);)
        if (bErNeedsInit) {
            ErInit();
            bErNeedsInit = 0;
        }
        TDomThreaded(Tcl_MutexUnlock(&initMutex);)
    }

    while (z[from]) {
        if (z[from]=='&') {
            int i = from+1;
            int c;

            if (z[i] == '#') {
                /*---------------------------------------------
                |   convert character reference
                \--------------------------------------------*/
                value = 0;
                if (z[++i] == 'x') {
                    i++;
                    while (z[i] && (c=z[i]) && (c!=';')) {
                        value = value * 16;
                        if ((c>='0') && (c<='9')) {
                            value += c-'0';
                        } else
                        if ((c>='A') && (c<='F')) {
                            value += c-'A' + 10;
                        } else
                        if ((c>='a') && (c<='f')) {
                            value += c-'a' + 10;
                        } else {
                            /* error */
                            return 0;
                        }
                        i++;
                    }
                } else {
                    while (z[i] && (c=z[i]) && (c!=';')) {
                        value = value * 10;
                        if ((c>='0') && (c<='9')) {
                            value += c-'0';
                        } else {
                            /* error */
                            return 0;
                        }
                        i++;
                    }
                }
                if (!z[i] || (z[i]!=';')) {
                    return 0;
                    /* error */
                }
                from = i+1;
#if TclOnly8Bits
                z[to++] = value;
#else 
                if (value < 0x80) {
                    z[to++] = value;
                } else if (value <= 0x7FF) {
                    z[to++] = (char) ((value >> 6) | 0xC0);
                    z[to++] = (char) ((value | 0x80) & 0xBF);
                } else if (value <= 0xFFFF) {
                    z[to++] = (char) ((value >> 12) | 0xE0);
                    z[to++] = (char) (((value >> 6) | 0x80) & 0xBF);
                    z[to++] = (char) ((value | 0x80) & 0xBF);
                } else {
                    /* error */
                    return 0;
                }
#endif
            } else {
                while (z[i] && isalpha((unsigned char)z[i])) {
                   i++;
                }
                if (!z[i] || (z[i]!=';')) {
                    return 0;
                }
                c = z[i];
                z[i] = 0;
                h = ErHash(&z[from+1]);
                p = apErHash[h];
                while (p && strcmp(p->zName,&z[from+1])!=0 ) {
                    p = p->pNext;
                }
                z[i] = c;
                if (p) {
                    zVal = p->zValue;
                    while (*zVal) {
                        z[to++] = *(zVal++);
                    }
                    from = i;
                    if (c==';') from++;
                } else {
                    z[to++] = z[from++];
                }
            }
        } else {
            z[to++] = z[from++];
        }
    }
    z[to] = 0;
    *newLen = to;
    return 1;
}
/*----------------------------------------------------------------------------
|   End Of Character Entity Translator
\---------------------------------------------------------------------------*/


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

/*----------------------------------------------------------------------------
|   XML_SimpleParse (non recursive)
|
|       Parses the XML string starting at 'pos' and continuing to the
|       first encountered error.
|
\---------------------------------------------------------------------------*/
static int
XML_SimpleParse (
    char        *xml,   /* XML string  */
    int         *pos,   /* Index of next unparsed character in xml */
    domDocument *doc,
    domNode     *parent_nodeOld,
    int          ignoreWhiteSpaces,
    char       **errStr
) {
    register int   c;          /* Next character of the input file */
    register char *pn;
    register char *x, *start, *piSep;
    int            saved;
    int            hasContent;
    domNode       *node;
    domNode       *parent_node = NULL;
    domTextNode   *tnode;
    domAttrNode   *attrnode, *lastAttr, *attrList;
    int            ampersandSeen = 0;
    int            only_whites   = 0;
    domProcessingInstructionNode *pinode;
    int            hnew;
    Tcl_HashEntry *h;

#ifdef TDOM_NS    
    int            nspos, newNS;
    int            depth = 0;
    int            activeNSpos  = -1;
    int            activeNSsize = 8;
    domActiveNS   *activeNS     = (domActiveNS*) MALLOC (sizeof(domActiveNS) * activeNSsize);
    const char    *xmlns, *localname;
    domNS         *ns;
    char           tagPrefix[MAX_PREFIX_LEN];
    char           prefix[MAX_PREFIX_LEN];
    domAttrNode   *lastNSAttr, *NSattrList;
#endif

    x = &(xml[*pos]);

    while ( (c=*x)!=0 ) {

        start = x;

        if (c!='<') {
            /*----------------------------------------------------------------
            |   read text between tags
            |
            \---------------------------------------------------------------*/
            ampersandSeen = 0;
            only_whites = 1;
            while ( (c=*x)!=0 && c!='<' ) {
                if (c=='&') ampersandSeen = 1;
                if ( (c != ' ')  &&
                     (c != '\t') &&
                     (c != '\n') &&
                     (c != '\r') ) {
                    only_whites = 0;
                }
                x++;
            }
            if (!(only_whites && ignoreWhiteSpaces) && parent_node) {
                /*--------------------------------------------------------
                |   allocate new TEXT node
                 \-------------------------------------------------------*/
                tnode = (domTextNode*) domAlloc(sizeof(domTextNode));
                memset(tnode, 0, sizeof(domTextNode));
                tnode->nodeType    = TEXT_NODE;
                tnode->nodeFlags   = 0;
                tnode->namespace   = 0;
                tnode->ownerDocument = doc;
                tnode->nodeNumber  = NODE_NO(doc);
                tnode->valueLength = (x - start);
                tnode->nodeValue   = (char*)MALLOC((x - start)+1);
                memmove(tnode->nodeValue, start, (x - start));
                *(tnode->nodeValue + (x - start)) = 0;
                if (ampersandSeen) {
                    if (!TranslateEntityRefs(tnode->nodeValue, 
                                             &(tnode->valueLength) )) {
                        RetError("Entity parsing error", (x - xml));
                    }
                }
                tnode->parentNode = parent_node;
                if (parent_node->firstChild)  {
                    parent_node->lastChild->nextSibling = (domNode*)tnode;
                    tnode->previousSibling = parent_node->lastChild;
                    parent_node->lastChild = (domNode*)tnode;
                } else {
                    parent_node->firstChild = parent_node->lastChild = 
                        (domNode*)tnode;
                }
            }

        } else if (x[1]=='/') {
            /*------------------------------------------------------------
            |   read and check closing tag
            \-----------------------------------------------------------*/
            node = parent_node;
            if (!parent_node) {
                RetError("Syntax error",(x - xml));
            }
            parent_node = node->parentNode;
            pn = (char*)node->nodeName;

            x += 2;
            while (*x == *pn) { x++; pn++; }
            if ( *pn || (*x!='>' && !SPACE(*x) ) ) {
                RetError("Unterminated element",(x - xml));
            }
            while (SPACE(*x)) {
                x++;
            }
            if (*x=='>') {
                x++;
            } else {
                RetError("Missing \">\"",(x - xml)-1);
            }
#ifdef TDOM_NS 
            depth--;
            /* pop active namespaces */
            while ( (activeNSpos >= 0) &&
                    (activeNS[activeNSpos].depth == depth) )
            {
                activeNSpos--;
            } 
#endif                                   
            if (parent_node == NULL) {
                /* we return to main node and so finished parsing */
#ifdef TDOM_NS
                FREE ((char *) activeNS);
#endif                
                return TCL_OK;
            }
            continue;

        } else {

            x++;
            if (*x=='!') {
                if (x[1]=='-' && x[2]=='-') {
                    /*--------------------------------------------------------
                    |   read over a comment
                    \-------------------------------------------------------*/
                    x += 3;
                    while ( (c=*x)!=0 &&
                            (c!='-' || x[1]!='-' || x[2]!='>')) {
                        x++;
                    }
                    if (*x) {
                        /*----------------------------------------------------
                        |   allocate new COMMENT node for comments
                        \---------------------------------------------------*/
                        tnode = (domTextNode*) domAlloc(sizeof(domTextNode));
                        memset(tnode, 0, sizeof(domTextNode));
                        tnode->nodeType      = COMMENT_NODE;
                        tnode->nodeFlags     = 0;
                        tnode->namespace     = 0;
                        tnode->ownerDocument = doc;
                        tnode->nodeNumber    = NODE_NO(doc);
                        tnode->parentNode    = parent_node;
                        tnode->valueLength   = x - start - 4;
                        tnode->nodeValue     = (char*)MALLOC(tnode->valueLength+1);
                        memmove(tnode->nodeValue, start+4, tnode->valueLength);
                        *(tnode->nodeValue + tnode->valueLength) = 0;
                        if (parent_node == NULL) {
                            if (doc->rootNode->lastChild) {
                                tnode->previousSibling = 
                                    doc->rootNode->lastChild;
                                doc->rootNode->lastChild->nextSibling 
                                    = (domNode*)tnode;
                            } else {
                                doc->rootNode->firstChild = (domNode*) tnode;
                            }
                            doc->rootNode->lastChild = (domNode*) tnode;
                        } else {
                            if (parent_node->firstChild)  {
                                parent_node->lastChild->nextSibling = (domNode*)tnode;
                                tnode->previousSibling = parent_node->lastChild;
                                parent_node->lastChild = (domNode*)tnode;
                            } else {
                                parent_node->firstChild = parent_node->lastChild = (domNode*)tnode;
                            }
                        }
                        x += 3;
                    } else {
                        RetError("Unterminated comment",(start-xml));
                    }
                    continue;

                } else if (x[1]=='D' && x[2]=='O' &&
                           x[3]=='C' && x[4]=='T' &&
                           x[5]=='Y' && x[6]=='P' && x[7]=='E' ) {
                    /*--------------------------------------------------------
                    |   read over a DOCTYPE definition
                    \-------------------------------------------------------*/
                    x += 8;
                    start = x;
                    while (*x!=0) {
                        if (*x=='[') {
                            x++;
                            while ((*x!=0) && (*x!=']')) x++;
                        } else
                        if (*x=='>') {
                            break;
                        } else {
                            x++;
                        }
                    }
                    if (*x) {
                        x++;
                    } else {
                        RetError("Unterminated DOCTYPE definition",(start-xml));
                    }
                    continue;

                } else if (x[1]=='[' && x[2]=='C' &&
                           x[3]=='D' && x[4]=='A' &&
                           x[5]=='T' && x[6]=='A' && x[7]=='[' ) {
                    /*--------------------------------------------------------
                    |   read over a <![CDATA[ section
                    \-------------------------------------------------------*/
                    x += 8;
                    start = x;
                    while ( (*x!=0) &&
                            ((*x!=']') || (x[1]!=']') || (x[2]!='>'))) {
                        x++;
                    }
                    if (*x) {
                        if (parent_node && (x - start)) {
                            /*----------------------------------------------------
                            |   allocate new TEXT node for CDATA section data
                            \---------------------------------------------------*/
                            tnode = (domTextNode*) domAlloc(sizeof(domTextNode));
                            memset(tnode, 0, sizeof(domTextNode));
                            tnode->nodeType      = TEXT_NODE;
                            tnode->nodeFlags     = 0;
                            tnode->namespace     = 0;
                            tnode->ownerDocument = doc;
                            tnode->nodeNumber    = NODE_NO(doc);
                            tnode->parentNode    = parent_node;
                            tnode->valueLength   = (x - start);
                            tnode->nodeValue     = (char*)MALLOC((x - start)+1);
                            memmove(tnode->nodeValue, start, (x - start));
                            *(tnode->nodeValue + (x - start)) = 0;
                            if (parent_node->firstChild)  {
                                parent_node->lastChild->nextSibling = (domNode*)tnode;
                                tnode->previousSibling = parent_node->lastChild;
                                parent_node->lastChild = (domNode*)tnode;
                            } else {
                                parent_node->firstChild = parent_node->lastChild = (domNode*)tnode;
                            }
                        }
                        x += 3;
                    } else {
                        RetError("Unterminated CDATA definition",(start-xml) );
                    }
                    continue;
                 } else {
                        RetError("Incorrect <!... tag",(start-xml) );
                 }

            } else if (*x=='?') {
                /*--------------------------------------------------------
                |   read over a processing instructions(PI) / XMLDecl
                \-------------------------------------------------------*/
                x++;
                start = x;
                while ( (c=*x)!=0 &&
                        (c!='?' || x[1]!='>')) {
                    x++;
                }
                if (*x) {
                    /*------------------------------------------------------------
                    |   allocate new PI node for processing instruction section
                    \-----------------------------------------------------------*/
                    pinode = (domProcessingInstructionNode*)
                            domAlloc(sizeof(domProcessingInstructionNode));
                    memset(pinode, 0, sizeof(domProcessingInstructionNode));
                    pinode->nodeType      = PROCESSING_INSTRUCTION_NODE;
                    pinode->nodeFlags     = 0;
                    pinode->namespace     = 0;
                    pinode->ownerDocument = doc;
                    pinode->nodeNumber    = NODE_NO(doc);
                    pinode->parentNode    = parent_node;

                    /*-------------------------------------------------
                    |   extract PI target
                    \------------------------------------------------*/
                    piSep = start;
                    while ( (c=*piSep)!=0 && !SPACE(c) &&
                            (c!='?' || piSep[1]!='>')) {
                         piSep++;
                    }
                    *piSep = '\0'; /* temporarily terminate the string */

                    pinode->targetLength = strlen(start);
                    pinode->targetValue  = (char*)MALLOC(pinode->targetLength);
                    memmove(pinode->targetValue, start, pinode->targetLength);

                    *piSep = c;  /* remove temporarily termination */

                    /*-------------------------------------------------
                    |   extract PI data
                    \------------------------------------------------*/
                    while (SPACE(*piSep)) {
                        piSep++;
                    }
                    pinode->dataLength = x - piSep;
                    pinode->dataValue  = (char*)MALLOC(pinode->dataLength);
                    memmove(pinode->dataValue, piSep, pinode->dataLength);

                    if (parent_node == NULL) {
                        if (doc->rootNode->lastChild) {
                            pinode->previousSibling = doc->rootNode->lastChild;
                            doc->rootNode->lastChild->nextSibling 
                                = (domNode*) pinode;
                        } else {
                            doc->rootNode->firstChild = (domNode*) pinode;
                        }
                        doc->rootNode->lastChild = (domNode*) pinode;
                    } else {
                        if (parent_node->firstChild)  {
                            parent_node->lastChild->nextSibling = (domNode*)pinode;
                            pinode->previousSibling = parent_node->lastChild;
                            parent_node->lastChild = (domNode*)pinode;
                        } else {
                            parent_node->firstChild = parent_node->lastChild = (domNode*)pinode;
                        }
                    }
                    x += 2;
                } else {
                    RetError("Unterminated processing instruction(PI)",(start-xml) );
                }
                continue;
            }

            /*----------------------------------------------------------------
            |   new tag/element
            |
            \---------------------------------------------------------------*/
            hasContent = 1;
            while ((c=*x)!=0 && c!='/' && c!='>' && !SPACE(c) ) {
                x++;
            }
            if (c==0) {
                RetError("Missing \">\"",(start-xml) );
            }
            if ( (x-start)==1) {
                RetError("Null markup name",(start-xml) );
            }
            *x = '\0'; /* temporarily terminate the string */

            /*------------------------------------------------------
            |   create new DOM element node
            \-----------------------------------------------------*/
            h = Tcl_CreateHashEntry(&HASHTAB(doc,tdom_tagNames), start+1,
                                    &hnew);
            node = (domNode*) domAlloc(sizeof(domNode));
            memset(node, 0, sizeof(domNode));
            node->nodeType      = ELEMENT_NODE;
            node->nodeFlags     = 0;
            node->namespace     = 0;
            node->nodeName      = (char *)&(h->key);
            node->ownerDocument = doc;
            node->nodeNumber    = NODE_NO(doc);
            node->ownerDocument = doc;

            if (parent_node == NULL) {
                if (doc->rootNode->lastChild) {
                    node->previousSibling = doc->rootNode->lastChild;
                    doc->rootNode->lastChild->nextSibling = node;
                } else {
                    doc->rootNode->firstChild = node;
                }
                doc->rootNode->lastChild = node;
            } else {
                node->parentNode = parent_node;
                if (parent_node->firstChild)  {
                    parent_node->lastChild->nextSibling = node;
                    node->previousSibling = parent_node->lastChild;
                    parent_node->lastChild = node;
                } else {
                    parent_node->firstChild = parent_node->lastChild = node;
                }
            }

            *x = c;  /* remove temporarily termination */

            while (SPACE(*x) ) {
                x++;
            }
            /*-----------------------------------------------------------
            |   read attribute name-value pairs
            \----------------------------------------------------------*/
            lastAttr = NULL;
            attrList = NULL;
#ifdef TDOM_NS
            lastNSAttr = NULL;
            NSattrList = NULL;
#endif            
            while ( (c=*x) && (c!='/') && (c!='>') ) {
                char *ArgName = x;
                int nArgName;
                char *ArgVal = NULL;
                int nArgVal = 0;

                while ((c=*x)!=0 && c!='=' && c!='>' && !SPACE(c) ) {
                    x++;
                }
                nArgName = x - ArgName;
                while (SPACE(*x)) {
                    x++;
                }
                if (*x=='=') {
                    x++;
                }
                saved = *(ArgName + nArgName);
                *(ArgName + nArgName) = '\0'; /* terminate arg name */

                while (SPACE(*x)) {
                    x++;
                }
                if (*x=='>' || *x==0) {
                    ArgVal = ArgName;
                    nArgVal = nArgName;
                } else if ((c=*x)=='\"' || c=='\'') {
                    register int cDelim = c;
                    x++;
                    ArgVal = x;
                    ampersandSeen = 0;
                    while ((c=*x)!=0 && c!=cDelim) {
                        if (c=='&') {
                            ampersandSeen = 1;
                        }
                        x++;
                    }
                    nArgVal = x - ArgVal;
                    if (c==0) {
                        RetError("Unterminated string",(ArgVal - xml - 1) );
                    } else {
                        x++;
                    }
                } else if (c!=0 && c!='>') {
                    ArgVal = x;
                    while ((c=*x)!=0 && c!='>' && !SPACE(c)) {
                        if (c=='&') {
                            ampersandSeen = 1;
                        }
                        x++;
                    }
                    if (c==0) {
                        RetError("Missing \">\"",(start-xml));
                    }
                    nArgVal = x - ArgVal;
                }

                
#ifdef TDOM_NS
                /*------------------------------------------------------------
                |   handle namespace attributes or normal ones
                \------------------------------------------------------------*/
                if (strncmp((char *)ArgName, "xmlns", 5) == 0) {
                    xmlns = ArgName;
                    newNS = 1;
                    
                    h = Tcl_CreateHashEntry(&HASHTAB(doc, tdom_attrNames),
                                            ArgName, &hnew); 
                    attrnode = (domAttrNode*) domAlloc(sizeof(domAttrNode));
                    memset(attrnode, 0, sizeof(domAttrNode));
                    attrnode->parentNode  = node;
                    attrnode->nodeName    = (char *)&(h->key);
                    attrnode->nodeType    = ATTRIBUTE_NODE;
                    attrnode->nodeFlags   = IS_NS_NODE;
                    attrnode->nodeValue   = (char*)MALLOC(nArgVal+1);
                    attrnode->valueLength = nArgVal;
                    memmove(attrnode->nodeValue, ArgVal, nArgVal);
                    *(attrnode->nodeValue + nArgVal) = 0;
                    if (ampersandSeen) {
                        if (!TranslateEntityRefs(attrnode->nodeValue,
                                                 &(attrnode->valueLength) )) {
                            RetError("Entity parsing error",(start-xml));
                        }
                    }
                    
                    if (xmlns[5] == ':') {
                        if (domIsNamespaceInScope (activeNS, activeNSpos,
                                                   &(xmlns[6]),
                                                   (char*)attrnode->nodeValue))
                        {
                            ns = domLookupPrefix (node, &(xmlns[6]));
                            newNS = 0;
                        } else {
                            ns = domNewNamespace(doc, &(xmlns[6]),
                                                 (char*)attrnode->nodeValue);
                        }
                    } else {
                        ns = domNewNamespace(doc, "", 
                                             (char*)attrnode->nodeValue);
                    }
                    attrnode->namespace   = ns->index;
                    if (newNS) {
                        /* push active namespace */
                        activeNSpos++;
                        if (activeNSpos >= activeNSsize) {
                            activeNS = (domActiveNS*) REALLOC(
                                           (char*)activeNS,
                                           sizeof(domActiveNS) * 2 * activeNSsize);
                            activeNSsize = 2 * activeNSsize;
                        }
                        activeNS[activeNSpos].depth     = depth;
                        activeNS[activeNSpos].namespace = ns;
                    }
    
                    if (NSattrList) {
                        lastNSAttr->nextSibling = attrnode;
                    } else {
                        NSattrList = attrnode;
                    }
                    lastNSAttr = attrnode;
                    

                } else {
#endif
                
                    /*------------------------------------------------------------
                    |   allocate new attribute node
                    \------------------------------------------------------------*/
                    h = Tcl_CreateHashEntry(&HASHTAB(doc,tdom_attrNames),
                                            ArgName, &hnew);
                    attrnode = (domAttrNode*) domAlloc(sizeof(domAttrNode));
                    memset(attrnode, 0, sizeof(domAttrNode));
                    attrnode->parentNode  = node;
                    attrnode->nodeName    = (char *)&(h->key);
                    attrnode->nodeType    = ATTRIBUTE_NODE;
                    attrnode->nodeFlags   = 0;
                    attrnode->nodeValue   = (char*)MALLOC(nArgVal+1);
                    attrnode->valueLength = nArgVal;
                    memmove(attrnode->nodeValue, ArgVal, nArgVal);
                    *(attrnode->nodeValue + nArgVal) = 0;
                    if (ampersandSeen) {
                        if (!TranslateEntityRefs(attrnode->nodeValue,
                                                 &(attrnode->valueLength) )) {
                            RetError("Entity parsing error", (start - xml));
                        }
                    }
                    if (attrList) {
                        lastAttr->nextSibling = attrnode;
                    } else {
                        attrList = attrnode;
                    }
                    lastAttr = attrnode;
#ifdef TDOM_NS
                }
#endif 
                *(ArgName + nArgName) = saved;
                while (SPACE(*x)) {
                    x++;
                }
            }

#ifdef TDOM_NS
            /*----------------------------------------------------------
            |   look for namespace of element
            \---------------------------------------------------------*/
            domSplitQName (node->nodeName, tagPrefix,
                           &localname);
            for (nspos = activeNSpos; nspos >= 0; nspos--) {
                if (  ((tagPrefix[0] == '\0') && (activeNS[nspos].namespace->prefix[0] == '\0'))
                      || ((tagPrefix[0] != '\0') && (activeNS[nspos].namespace->prefix[0] != '\0')
                          && (strcmp(tagPrefix, activeNS[nspos].namespace->prefix) == 0))
                    ) {
                    if (activeNS[nspos].namespace->prefix[0] == '\0'
                        && activeNS[nspos].namespace->uri[0] == '\0'
                        && tagPrefix[0] == '\0') 
                    {
                        /* xml-names rec. 5.2: "The default namespace can be
                           set to the empty string. This has the same effect,
                           within the scope of the declaration, of there being
                           no default namespace." */
                        break;
                    }
                    node->namespace = activeNS[nspos].namespace->index;
                    DBG(fprintf(stderr, "tag='%s' uri='%s' \n",node->nodeName,
                                activeNS[nspos].namespace->uri);
                               )
                    break;
                }
            }

            /*----------------------------------------------------------
            |   look for attribute namespace
            \---------------------------------------------------------*/
            attrnode = attrList;
            while (attrnode) {
                domSplitQName ((char*)attrnode->nodeName, prefix, &localname);
                if (prefix[0] != '\0') {
                    for (nspos = activeNSpos; nspos >= 0; nspos--) {
                        if (  ((prefix[0] == '\0') && (activeNS[nspos].namespace->prefix[0] == '\0'))
                              || ((prefix[0] != '\0') && (activeNS[nspos].namespace->prefix[0] != '\0')
                                  && (strcmp(prefix, activeNS[nspos].namespace->prefix) == 0))
                            ) {
                            attrnode->namespace = activeNS[nspos].namespace->index;
                            DBG(fprintf(stderr, "attr='%s' uri='%s' \n",
                                        attrnode->nodeName,
                                        activeNS[nspos].namespace->uri);
                                )
                            break;
                        }
                    }
                }
                attrnode = attrnode->nextSibling;
            }
            if (lastNSAttr) {
                node->firstAttr = NSattrList;
                lastNSAttr->nextSibling = attrList;
            } else {
                node->firstAttr = attrList;
            }
#else
            node->firstAttr = attrList;

#endif
            if (*x=='/') {
                hasContent = 0;
                x++;
                if (*x!='>') {
                    RetError("Syntax Error",(x - xml - 1) );
                }
            }
            if (x[1] == 0) {
#ifdef TDOM_NS
                FREE ((char *) activeNS);
#endif                
                return TCL_OK;
            }
            if (*x=='>') {
                x++;
            }
            if (hasContent) {
#ifdef TDOM_NS
                depth++;
#endif
                /*------------------------------------------------------------
                |   recurs to read child tags/texts
                \-----------------------------------------------------------*/
                parent_node = node;
            }
        }
    }
    RetError("Unexpected end",(x - xml) );

} /* XML_SimpleParse */



/*----------------------------------------------------------------------------
|   XML_SimpleParseDocument
|
|       Create a document, parses the XML string starting at 'pos' and
|       continuing to the first encountered error.
|
\---------------------------------------------------------------------------*/
domDocument *
XML_SimpleParseDocument (
    char    *xml,              /* Complete text of the file being parsed  */
    int      ignoreWhiteSpaces,
    char    *baseURI,
    char    *extResolver,
    int     *pos,
    char   **errStr
) {
    domDocument   *doc = domCreateDoc(baseURI, 0);

    if (extResolver) {
        doc->extResolver = extResolver;
    }
    
    *pos = 0;
    XML_SimpleParse (xml, pos, doc, NULL, ignoreWhiteSpaces, errStr);
    domSetDocumentElement (doc);

    return doc;

} /* XML_SimpleParseDocument */

