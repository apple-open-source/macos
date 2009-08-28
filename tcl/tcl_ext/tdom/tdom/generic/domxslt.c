/*----------------------------------------------------------------------------
|   Copyright (c) 2000 Jochen Loewer (loewerj@hotmail.com)
|-----------------------------------------------------------------------------
|
|   $Id: domxslt.c,v 1.119 2008/03/04 21:03:09 rolf Exp $
|
|
|   A XSLT implementation for tDOM, according to the W3C
|   recommendation (16 Nov 1999).
|   See http://www.w3.org/TR/1999/REC-xslt-19991116 for details.
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
|   Portions created by Jochen Loewer are Copyright (C) 1999, 2000
|   Jochen Loewer. All Rights Reserved.
|
|   Contributor(s):
|       Aug01    Rolf Ade   xsl:include, xsl:import, xsl:apply-imports,
|                           document() plus several bug fixes
|
|       Fall/Winter 01 Rolf Ade rewrite of xsl:number, xsl:key/key(),
|                               handling of toplevel var/parameter,
|                               plenty of fixes and enhancements all
|                               over the place.
|
|    2001-2007   Rolf Ade   All changes and enhancements.
|
|   written by Jochen Loewer
|   June, 2000
|
\---------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------
|   Includes
|
\---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>
#include <locale.h>
#include <dom.h>
#include <domxpath.h>
#include <domxslt.h>
#include <tcl.h>         /* for hash tables */

/*----------------------------------------------------------------------------
|   Defines
|
\---------------------------------------------------------------------------*/
#define XSLT_NAMESPACE  "http://www.w3.org/1999/XSL/Transform"
#define INITIAL_SIZE_FOR_KEYSETS 10


/*----------------------------------------------------------------------------
|   Macros
|
\---------------------------------------------------------------------------*/
#define DBG(x)              
#define TRACE(x)            DBG(fprintf(stderr,(x)))
#define TRACE1(x,a)         DBG(fprintf(stderr,(x),(a)))
#define TRACE2(x,a,b)       DBG(fprintf(stderr,(x),(a),(b)))
#define TRACE3(x,a,b,c)     DBG(fprintf(stderr,(x),(a),(b),(c)))
#define TRACE4(x,a,b,c,d)   DBG(fprintf(stderr,(x),(a),(b),(c),(d)))
#define TRACE5(x,a,b,c,d,e) DBG(fprintf(stderr,(x),(a),(b),(c),(d),(e)))

#define CHECK_RC            if (rc < 0) return rc
#define CHECK_RC1(x)        if (rc < 0) {FREE((char*)(x)); return rc;}
#define SET_TAG(t,n,s,v)    if (strcmp(n,s)==0) { t->info = v; return v; }
#define SETSCOPESTART       xs->varFramesStack[xs->varFramesStackPtr].stop=1
#define SETPARAMDEF         xs->varFramesStack[xs->varFramesStackPtr].stop=2

#if defined(_MSC_VER)
# define STRCASECMP(a,b)  stricmp (a,b)
#else
# define STRCASECMP(a,b)  strcasecmp (a,b)
#endif

extern void printAst (int depth, ast t);

/*--------------------------------------------------------------------------
|   xsltTag
|
\-------------------------------------------------------------------------*/
typedef enum {

    unknown = 1,
    applyImports, applyTemplates, attribute, attributeSet, callTemplate,
    choose, comment, copy, copyOf, decimalFormat, element, fallback, forEach,
    xsltIf, import,
    include, key, message, namespaceAlias, number, output, otherwise,
    param, procinstr,
    preserveSpace, sort, stylesheet, stripSpace, text, template,
    transform, valueOf, variable, when, withParam

} xsltTag;


/*--------------------------------------------------------------------------
|   xsltAttr
|
\-------------------------------------------------------------------------*/
typedef enum {

    a_caseorder = 1,
    a_count, a_dataType, a_disableOutputEscaping,
    a_doctypePublic, a_doctypeSystem, a_elements, a_encoding,
    a_format, a_from, a_href, a_lang, a_level, a_match, a_mediaType, a_method,
    a_mode, a_name, a_namespace, a_order, a_prio, a_select, a_space,
    a_terminate, a_test, a_use, a_useAttributeSets, a_value,
    a_groupingSeparator, a_groupingSize,
    a_decimalSeparator, a_infinity, a_minusSign, a_nan, a_percent,
    a_perMille, a_zeroDigit, a_digit, a_patternSeparator, a_version,
    a_excludeResultPrefixes, a_extensionElementPrefixes,
    a_stylesheetPrefix, a_resultPrefix, a_indent, a_omitXMLDeclaration,
    a_standalone, a_cdataSectionElements

} xsltAttr;


/*--------------------------------------------------------------------------
|   xsltExclExtNS
|
\-------------------------------------------------------------------------*/
typedef struct xsltExclExtNS
{
    char                 * uri;

    struct xsltExclExtNS * next;

} xsltExclExtNS;

/*--------------------------------------------------------------------------
|   xsltSubDocs
|
\-------------------------------------------------------------------------*/
typedef struct xsltSubDoc
{
    domDocument        * doc;
    char               * baseURI;
    Tcl_HashTable        keyData;
    xsltExclExtNS      * excludeNS;
    xsltExclExtNS      * extensionNS;
    int                  fwCmpProcessing;
    int                  isStylesheet;
    int                  fixedXMLSource;
    int                  mustFree;
    
    struct xsltSubDoc  * next;

} xsltSubDoc;

/*--------------------------------------------------------------------------
|   xsltTemplate
|
\-------------------------------------------------------------------------*/
typedef struct xsltTemplate {

    char       * match;
    const char * name;
    const char * nameURI;
    ast          ast;
    const char * mode;
    const char * modeURI;
    double       prio;
    domNode    * content;
    double       precedence;
    ast          freeAst;
    xsltSubDoc * sDoc;

    struct xsltTemplate *next;

} xsltTemplate;


/*--------------------------------------------------------------------------
|   xsltAttrSet
|
\-------------------------------------------------------------------------*/
typedef struct xsltAttrSet {

    const char * name;
    const char * uri;
    domNode    * content;

    struct xsltAttrSet *next;

} xsltAttrSet;

/*--------------------------------------------------------------------------
|   xsltNodeSet
|
\-------------------------------------------------------------------------*/
typedef struct xsltNodeSet {
    
    domNode       **nodes;
    int             nr_nodes;
    int             allocated;
    
} xsltNodeSet;

/*--------------------------------------------------------------------------
|   xsltKeyInfo
|
\-------------------------------------------------------------------------*/
typedef struct xsltKeyInfo {

    domNode       * node;
    char          * match;
    ast             matchAst;
    char          * use;
    ast             useAst;

    struct xsltKeyInfo * next;

} xsltKeyInfo;

/*--------------------------------------------------------------------------
|   xsltVariable
|
\-------------------------------------------------------------------------*/
typedef struct xsltVariable {

    const char           * name;
    const char           * uri;
    domNode        * node;
    xpathResultSet   rs;
    int              active;

} xsltVariable;

/*--------------------------------------------------------------------------
|   xsltVarFrame
|
\-------------------------------------------------------------------------*/
typedef struct xsltVarFrame {

    xsltVariable        * vars;
    int                   polluted;
    int                   nrOfVars;
    int                   varStartIndex;
    int                   stop;

} xsltVarFrame;

/*--------------------------------------------------------------------------
|   xsltTopLevelVar
|
\-------------------------------------------------------------------------*/
typedef struct xsltTopLevelVar
{

    domNode                 * node;
    char                    * name;
    int                       isParameter;
    double                    precedence;

} xsltTopLevelVar;

/*--------------------------------------------------------------------------
|   xsltVarInProcess
|
\-------------------------------------------------------------------------*/
typedef struct xsltVarInProcess

{
    char                    *name;

    struct xsltVarInProcess *next;

} xsltVarInProcess;


/*--------------------------------------------------------------------------
|   xsltDecimalFormat
|
\-------------------------------------------------------------------------*/
typedef struct xsltDecimalFormat
{
#if TclOnly8Bits
    char   * name;
    char   * uri;
    char     decimalSeparator;
    char     groupingSeparator;
    char   * infinity;
    char     minusSign;
    char   * NaN;
    char     percent;
    char     zeroDigit;
    char     digit;
    char     patternSeparator;
#else 
    char        * name;
    char        * uri;
    Tcl_UniChar   decimalSeparator;
    Tcl_UniChar   groupingSeparator;
    char        * infinity;
    Tcl_UniChar   minusSign;
    char        * NaN;
    Tcl_UniChar   percent;
    Tcl_UniChar   perMille;
    Tcl_UniChar   zeroDigit;
    Tcl_UniChar   digit;
    Tcl_UniChar   patternSeparator;
#endif /* TclOnly8Bits */

    struct xsltDecimalFormat * next;

} xsltDecimalFormat;


/*--------------------------------------------------------------------------
|   xsltWSInfo
|
\-------------------------------------------------------------------------*/
typedef struct xsltWSInfo
{

    int            hasData;
    int            stripAll;
    double         wildcardPrec;
    Tcl_HashTable  stripTokens;
    Tcl_HashTable  preserveTokens;

} xsltWSInfo;


typedef struct xsltNSAlias
{

    char    *fromUri;
    char    *toUri;
    double   precedence;

    struct xsltNSAlias *next;

} xsltNSAlias;


/*--------------------------------------------------------------------------
|   xsltState
|
\-------------------------------------------------------------------------*/
typedef struct {

    xsltTemplate      * templates;
    Tcl_HashTable       namedTemplates;
    Tcl_HashTable       isElementTpls;
    xsltWSInfo          wsInfo;
    domNode           * xmlRootNode;
    domDocInfo          doctype;
    int                 indentOutput;
    domDocument       * resultDoc;
    domNode           * lastNode;
    xsltVarFrame      * varFramesStack;
    int                 varFramesStackPtr;
    int                 varFramesStackLen;
    xsltVariable      * varStack;
    int                 varStackPtr;
    int                 varStackLen;
    xsltAttrSet       * attrSets;
    Tcl_HashTable       xpaths;
    Tcl_HashTable       pattern;
    Tcl_HashTable       formats;
    Tcl_HashTable       topLevelVars;
    Tcl_HashTable       keyInfos;
    xsltNSAlias       * nsAliases;
    int                 nsUniqeNr;
    xsltVarInProcess  * varsInProcess;
    xpathCBs            cbs;
    xpathFuncCallback   orig_funcCB;
    void              * orig_funcClientData;
    xsltMsgCB           xsltMsgCB;
    void              * xsltMsgClientData;
    xsltDecimalFormat * decimalFormats;
    domNode           * current;
    xsltSubDoc        * subDocs;
    xsltSubDoc        * currentSubDoc;
    xsltTemplate      * currentTplRule;
    domNode           * currentXSLTNode;
    domDocument       * xsltDoc;

} xsltState;


typedef enum {
    latin_number, latin_upper, latin_lower,
    roman_upper, roman_lower
} xsltNumberingType;


/*--------------------------------------------------------------------------
|   xsltNumberFormatToken
|
\-------------------------------------------------------------------------*/
typedef struct {

    xsltNumberingType  type;
    int                minlength;
    char              *sepStart;
    int                sepLen;

} xsltNumberFormatToken;


/*--------------------------------------------------------------------------
|   xsltNumberFormat
|
\-------------------------------------------------------------------------*/
typedef struct {

    char                  *formatStr;
    int                    prologLen;
    xsltNumberFormatToken *tokens;
    int                    maxtokens;
    char                  *epilogStart;
    int                    epilogLen;

} xsltNumberFormat;


/*--------------------------------------------------------------------------
|   Prototypes
|
\-------------------------------------------------------------------------*/
static int ApplyTemplates ( xsltState *xs, xpathResultSet *context,
                            domNode *currentNode, int currentPos,
                            domNode *actionNode, xpathResultSet *nodeList,
                            const char *mode, const char *modeURI, 
                            char **errMsg);

static int ApplyTemplate (  xsltState *xs, xpathResultSet *context,
                            domNode *currentNode, domNode *exprContext,
                            int currentPos, const char *mode, 
                            const char *modeURI, char **errMsg);

static int ExecActions (xsltState *xs, xpathResultSet *context,
                        domNode *currentNode, int currentPos,
                        domNode *actionNode,  char **errMsg);

static domDocument * getExternalDocument (Tcl_Interp *interp, xsltState *xs,
                                          domDocument *xsltDoc, 
                                          const char *baseURI,
                                          const char *href, int isStylesheet,
                                          int fixedXMLSource, char **errMsg);


/*----------------------------------------------------------------------------
|   printXML
|
\---------------------------------------------------------------------------*/
static void printXML (domNode *node, int level, int maxlevel) {

    domTextNode *tnode;
    domProcessingInstructionNode *pi;
    char tmp[80];
    int  i, l, n;

    n = 0;
    while (node) {

        for (i=0;i<level;i++) fprintf(stderr, "  ");
        if (node->nodeType == ELEMENT_NODE) {
            if (node->firstChild && node->firstChild->nodeType == TEXT_NODE) {
                tnode = (domTextNode*)node->firstChild;
                l = tnode->valueLength;
                if (l > 30) l = 30;
                memmove(tmp, tnode->nodeValue, l);
                tmp[l] = '\0';
                fprintf(stderr, "<%s/domNode0x%p> '%s'\n", node->nodeName, 
                        node, tmp);
            } else {
                tmp[0] = '\0';
                if ((level>=maxlevel) && (node->firstChild)) {
                    strcpy( tmp, "...");
                }
                fprintf(stderr, "<%s/domNode0x%p> %s\n", node->nodeName,
                        node, tmp);
            }
            if (level<maxlevel) {
                if (node->firstChild) printXML(node->firstChild, level+1,
                                               maxlevel);
            }
        }
        if (node->nodeType == TEXT_NODE) {
            tnode = (domTextNode*)node;
            l = tnode->valueLength;
            if (l > 70) l = 70;
            memmove(tmp, tnode->nodeValue, l);
            tmp[l] = '\0';
            fprintf(stderr, "'%s'\n", tmp);
        }
        if (node->nodeType == COMMENT_NODE) {
            tnode = (domTextNode*)node;
            l = tnode->valueLength;
            memmove (tmp, "<!--", 4);
            if (l >70) l = 70;
            memmove (&tmp[4], tnode->nodeValue, l);
            memmove (&tmp[4+l], "-->", 3);
            tmp[7+l] = '\0';
            fprintf(stderr, "'%s'\n", tmp);
        }
        if (node->nodeType == PROCESSING_INSTRUCTION_NODE) {
            pi = (domProcessingInstructionNode*)node;
            l = pi->targetLength;
            if (l > 70) l = 70;
            memmove(tmp, pi->targetValue, l);
            tmp[l] = '\0';
            fprintf(stderr, "<?%s ", tmp);
            l = pi->dataLength;
            if (l > 70) l = 70;
            memmove(tmp, pi->dataValue, l);
            tmp[l] = '\0';
            fprintf(stderr, "%s?>\n", tmp);
        }
        node = node->nextSibling;
        n++;
        if (n>8) { fprintf(stderr, "...\n"); return; }
    }
}
        
/*----------------------------------------------------------------------------
|   reportError
|
\---------------------------------------------------------------------------*/
static void
reportError (
    domNode * node,
    char    * str,
    char   ** errMsg)
{
    Tcl_DString dStr;
    char buffer[1024];
    const char *baseURI;
    int  line, column;

    Tcl_DStringInit (&dStr);
    baseURI = findBaseURI (node);
    if (baseURI) {
        Tcl_DStringAppend (&dStr, "In entity ", 10);
        Tcl_DStringAppend (&dStr, baseURI, -1);
    }
    if (node->nodeFlags & HAS_LINE_COLUMN) {
        domGetLineColumn (node, &line, &column);
        sprintf (buffer, " at line %d, column %d:\n", line, column);
        Tcl_DStringAppend (&dStr, buffer, -1);
        Tcl_DStringAppend (&dStr, str, -1);
    } else {
        if (baseURI) Tcl_DStringAppend (&dStr, ": ", 2);
        Tcl_DStringAppend (&dStr, str, -1);
    }
    if (*errMsg) FREE (*errMsg);
    *errMsg = tdomstrdup (Tcl_DStringValue (&dStr));
    Tcl_DStringFree (&dStr);
}

/*----------------------------------------------------------------------------
|   getAttr
|
\---------------------------------------------------------------------------*/
static char * getAttr (
    domNode  *node,
    char     *name,
    xsltAttr  attrTypeNo
)
{
    domAttrNode *attr;

    attr = node->firstAttr;
    while (attr) {

        if (attr->info == (unsigned int)attrTypeNo) {
            return attr->nodeValue;
        } else if (attr->info == 0) {
            if (strcmp ((char*)attr->nodeName, name)==0) {
                attr->info = (unsigned int)attrTypeNo;
                return attr->nodeValue;
            }
        }
        attr = attr->nextSibling;
    }
    return NULL;
}


/*----------------------------------------------------------------------------
|   getTag
|
\---------------------------------------------------------------------------*/
static xsltTag getTag (
    domNode  *node
)
{
    const char *name;

    if (node->nodeType != ELEMENT_NODE) {
        node->info = (int)unknown;
        return unknown;
    }
    if (node->info != 0) {
        return (xsltTag)node->info;
    }
    name = domNamespaceURI(node);
    if ((name == NULL) || (strcmp(name, XSLT_NAMESPACE)!=0)) {
       node->info = (int)unknown;
       return unknown;
    }
    name = domGetLocalName(node->nodeName);

    switch (*name) {
        case 'a': SET_TAG(node,name,"apply-imports",  applyImports);
                  SET_TAG(node,name,"apply-templates",applyTemplates);
                  SET_TAG(node,name,"attribute",      attribute);
                  SET_TAG(node,name,"attribute-set",  attributeSet);
                  break;
        case 'c': SET_TAG(node,name,"call-template",  callTemplate);
                  SET_TAG(node,name,"choose",         choose);
                  SET_TAG(node,name,"comment",        comment);
                  SET_TAG(node,name,"copy",           copy);
                  SET_TAG(node,name,"copy-of",        copyOf);
                  break;
        case 'd': SET_TAG(node,name,"decimal-format", decimalFormat);
                  break;
        case 'e': SET_TAG(node,name,"element",        element);
                  break;
        case 'f': SET_TAG(node,name,"fallback",       fallback);
                  SET_TAG(node,name,"for-each",       forEach);
                  break;
        case 'i': SET_TAG(node,name,"if",             xsltIf);
                  SET_TAG(node,name,"import",         import);
                  SET_TAG(node,name,"include",        include);
                  break;
        case 'k': SET_TAG(node,name,"key",            key);
                  break;
        case 'm': SET_TAG(node,name,"message",        message);
                  break;
        case 'n': SET_TAG(node,name,"namespace-alias",namespaceAlias);
                  SET_TAG(node,name,"number",         number);
                  break;
        case 'o': SET_TAG(node,name,"output",         output);
                  SET_TAG(node,name,"otherwise",      otherwise);
                  break;
        case 'p': SET_TAG(node,name,"param",          param);
                  SET_TAG(node,name,"preserve-space", preserveSpace);
                  SET_TAG(node,name,"processing-instruction", procinstr);
                  break;
        case 's': SET_TAG(node,name,"sort",           sort);
                  SET_TAG(node,name,"stylesheet",     stylesheet);
                  SET_TAG(node,name,"strip-space",    stripSpace);
                  break;
        case 't': SET_TAG(node,name,"template",       template);
                  SET_TAG(node,name,"text",           text);
                  SET_TAG(node,name,"transform",      transform);
                  break;
        case 'v': SET_TAG(node,name,"value-of",       valueOf);
                  SET_TAG(node,name,"variable",       variable);
                  break;
        case 'w': SET_TAG(node,name,"when",           when);
                  SET_TAG(node,name,"with-param",     withParam);
                  break;
    }
    node->info = (int)unknown;
    return unknown;
}


/*----------------------------------------------------------------------------
|   xsltPopVarFrame
|
\---------------------------------------------------------------------------*/
static void xsltPopVarFrame (
    xsltState  * xs
)
{
    int             i;
    xsltVarFrame   *frame;

    if (xs->varFramesStackPtr >= 0) {    
        frame = &xs->varFramesStack[xs->varFramesStackPtr];
        if (frame->nrOfVars) {
            for (i = frame->varStartIndex;
                 i < frame->varStartIndex + frame->nrOfVars;
                 i++) {
                xpathRSFree (&(&xs->varStack[i])->rs);
            }
        }
        xs->varStackPtr -= frame->nrOfVars;
        xs->varFramesStackPtr--;
    }
}


/*----------------------------------------------------------------------------
|   xsltPushVarFrame
|
\---------------------------------------------------------------------------*/
static void xsltPushVarFrame (
    xsltState  * xs
)
{
    xsltVarFrame  * frame;
    
    xs->varFramesStackPtr++;
    if (xs->varFramesStackPtr >= xs->varFramesStackLen) {
        xs->varFramesStack = (xsltVarFrame *) REALLOC ((char*)xs->varFramesStack,
                                                        sizeof (xsltVarFrame)
                                                        * 2 * xs->varFramesStackLen);
        xs->varFramesStackLen *= 2;
    }
    frame = &(xs->varFramesStack[xs->varFramesStackPtr]);
    frame->polluted = 0;
    frame->nrOfVars = 0;
    frame->varStartIndex = -1;
    frame->stop = 0;
}


/*----------------------------------------------------------------------------
|   xsltAddExternalDocument
|
\---------------------------------------------------------------------------*/
static int xsltAddExternalDocument (
    xsltState       * xs,
    const char      * baseURI,
    const char      * str,
    int               fixedXMLSource,
    xpathResultSet  * result,
    char           ** errMsg
)
{
    xsltSubDoc  * sdoc;
    domDocument * extDocument;
    int           found;

    DBG(
        fprintf (stderr, "xsltAddExternalDocument: baseURI '%s'\n", baseURI);
        fprintf (stderr, "xsltAddExternalDocument: systemID '%s'\n", str);
    )

    found = 0;
    sdoc = xs->subDocs;
    if (str) {
        while (sdoc) {
            if (!sdoc->isStylesheet
                && sdoc->baseURI 
                && strcmp (sdoc->baseURI, str)==0) {
                rsAddNode (result, sdoc->doc->rootNode);
                found = 1;
                break;
            }
            sdoc = sdoc->next;
        }
    }
    if (!found) {
        if (!xs->xsltDoc->extResolver) {
            *errMsg = tdomstrdup("need resolver Script to include Stylesheet! "
                                 "(use \"-externalentitycommand\")");
            return -1;
        }
        extDocument = getExternalDocument (
                         (Tcl_Interp*)xs->orig_funcClientData,
                         xs, xs->xsltDoc, baseURI, str, 0, fixedXMLSource,
                         errMsg);
        if (extDocument) {
            rsAddNode (result, extDocument->rootNode);
        } else {
            return -1;
        }
    }
    return found;
}


/*----------------------------------------------------------------------------
|   xsltNumberFormatTokenizer
|
\---------------------------------------------------------------------------*/
static xsltNumberFormat* xsltNumberFormatTokenizer (
    xsltState  *xs,
    char       *formatStr,
    char      **errMsg
)
{
    char             *p;
    int               hnew, clen, nrOfTokens = 0;
    Tcl_HashEntry    *h;
    xsltNumberFormat *format;

    /* TODO: make it l18n aware. */

    h = Tcl_CreateHashEntry (&xs->formats, formatStr, &hnew);
    if (!hnew) {
        return (xsltNumberFormat *) Tcl_GetHashValue (h);
    } else {
        format = (xsltNumberFormat *)MALLOC(sizeof (xsltNumberFormat));
        memset (format, 0 , sizeof (xsltNumberFormat));
        format->tokens = (xsltNumberFormatToken *)
            MALLOC(sizeof (xsltNumberFormatToken) * 20);
        memset (format->tokens, 0, sizeof (xsltNumberFormatToken) * 20);
        format->maxtokens = 20;
        Tcl_SetHashValue (h, format);
        format->formatStr = p = Tcl_GetHashKey (&(xs->formats), h);
    }
    while (*p) {
        clen = UTF8_CHAR_LEN(*p);
        if (!clen) {
            reportError (xs->currentXSLTNode, "xsl:number: UTF-8 form of"
                         " character longer than 3 Byte", errMsg);
            return NULL;
        }
        if (clen > 1) {
            /* hack: skip over multibyte chars - this may be wrong */
            format->prologLen += clen;
            p += clen;
            continue;
        }
        if (isalnum((unsigned char)*p)) break;
        format->prologLen++;
        p++;
    }

    format->tokens[0].minlength = 1;
    if (!*p) {
        format->tokens[0].type = latin_number;
        return format;
    }

#define addSeperator  \
    p++;                                         \
    if (*p) {                                    \
        format->tokens[nrOfTokens].sepStart = p; \
    }                                            \
    while (*p) {                                 \
        clen = UTF8_CHAR_LEN(*p);                \
        if (!clen) {                             \
            reportError (xs->currentXSLTNode, "xsl:number: UTF-8 form of character longer than 3 Byte", errMsg); \
            return NULL;                         \
        }                                        \
        if (clen > 1) {                          \
            /* hack: skip over multibyte chars - this may be wrong */ \
            format->tokens[nrOfTokens].sepLen += clen;  \
            p += clen;                           \
            continue;                            \
        }                                        \
        if (isalnum((unsigned char)*p)) break;                  \
        format->tokens[nrOfTokens].sepLen++;     \
        p++;                                     \
    }                                            \
    if (*p) {                                    \
        if (format->tokens[nrOfTokens].sepLen == 0) goto wrongSyntax; \
    }                                            \
    nrOfTokens++;                                \
    if (nrOfTokens == format->maxtokens) {       \
        format->tokens = (xsltNumberFormatToken *) REALLOC ((char *)format->tokens, sizeof (xsltNumberFormatToken) * format->maxtokens * 2);  \
        format->maxtokens *= 2;                  \
    }                                            \
    format->tokens[nrOfTokens].minlength = 1;    \
    continue;

    while (*p) {
        if (*p == '0') {
            format->tokens[nrOfTokens].minlength++;
            p++;
            continue;
        }
        if (*p == '1') {
            format->tokens[nrOfTokens].type = latin_number;
            addSeperator;
        }
        if (*p == 'A') {
            if (isalnum((unsigned char)*(p+1))) goto wrongSyntax;
            format->tokens[nrOfTokens].type = latin_upper;
            addSeperator;
        }
        if (*p == 'a') {
            if (isalnum((unsigned char)*(p+1))) goto wrongSyntax;
            format->tokens[nrOfTokens].type = latin_lower;
            addSeperator;
        }
        if (*p == 'I') {
            if (isalnum((unsigned char)*(p+1))) goto wrongSyntax;
            format->tokens[nrOfTokens].type = roman_upper;
            addSeperator;
        }
        if (*p == 'i') {
            if (isalnum((unsigned char)*(p+1))) goto wrongSyntax;
            format->tokens[nrOfTokens].type = roman_lower;
            addSeperator;
        }
        format->tokens[nrOfTokens].type = latin_number;
        while (isalnum((unsigned char)*(p+1))) {
            p++;
        }
        addSeperator;
    }
    format->epilogStart = format->tokens[nrOfTokens-1].sepStart;
    format->tokens[nrOfTokens-1].sepStart = NULL;
    format->epilogLen = format->tokens[nrOfTokens-1].sepLen;
    format->tokens[nrOfTokens-1].sepLen = 0;
    return format;

 wrongSyntax:
    reportError (xs->currentXSLTNode, "xsl:number: Wrong syntax in"
                 " format attribute", errMsg);
    return NULL;
}

/*----------------------------------------------------------------------------
|   formatValue
|
\---------------------------------------------------------------------------*/
static void formatValue (
    xsltNumberFormat *f,
    int              *useFormatToken,
    int               value,
    Tcl_DString      *str,
    char             *groupingSeparator,
    long              groupingSize,
    int               addSeperater
)
{
    int         len, fulllen, gslen, upper = 0, e, m, b, i, z, v;
    char        tmp[80], *pt;
    Tcl_DString tmp1;
    static struct { char *digit; char *ldigit; int value; } RomanDigit[] = {
          { "M" , "m" , 1000, },
          { "CM", "cm",  900, },
          { "D" , "d" ,  500, },
          { "CD", "cd",  400, },
          { "C" , "c" ,  100, },
          { "XC", "xc",   90, },
          { "L" , "l" ,   50, },
          { "XL", "xl",   40, },
          { "X" , "x" ,   10, },
          { "IX", "ix",    9, },
          { "V" , "v" ,    5, },
          { "IV", "iv",    4, },
          { "I" , "i" ,    1  }
    };

    switch (f->tokens[*useFormatToken].type) {
    case latin_number:
        sprintf (tmp, "%d", value);
        fulllen = len = strlen (tmp);
        if (f->tokens[*useFormatToken].minlength > fulllen) {
            fulllen = f->tokens[*useFormatToken].minlength;
        }
        if (groupingSeparator) {
            gslen = strlen (groupingSeparator);
            Tcl_DStringInit (&tmp1);
            if (len < f->tokens[*useFormatToken].minlength) {
                for (i = 0; i <  f->tokens[*useFormatToken].minlength - len; i++) {
                    Tcl_DStringAppend (&tmp1, "0", 1);
                }
            }
            Tcl_DStringAppend (&tmp1, tmp, len);
            pt = Tcl_DStringValue (&tmp1);
            len = Tcl_DStringLength (&tmp1);
            m = len % groupingSize;
            if (m) {
                Tcl_DStringAppend (str, pt, m);
                pt += m;
            }
            i = len - m;
            while (i) {
                if (i != len) {
                    Tcl_DStringAppend (str, groupingSeparator, gslen);
                }
                Tcl_DStringAppend (str, pt, groupingSize);
                pt += groupingSize;
                i -= groupingSize;
            }
            Tcl_DStringFree (&tmp1);
        } else {
            for (i = 0; i < fulllen - len; i++) {
                Tcl_DStringAppend (str, "0", 1);
            }
            Tcl_DStringAppend (str, tmp, len);
        }
        goto appendSeperator;
        break;

    case latin_upper:
        upper = 1;
        /* fall thru */
    case latin_lower:
        /* Home grown algorithm. (And I'm really not happy with it.)
           Please let rolf@pointsman.de know how to do this better /
           faster / more clever. */

        if (value <= 0) {
            /* Hm, zero can't be expressed with letter sequences...
               What to do? One of the several cases, not mentioned
               by the spec. */
            /* fall back to latin numbers */
            sprintf (tmp, "%d", value);
            break;
        }
        e = 1;
        m = b = 26;
        while (value > m) {
            b *= 26;
            m += b;
            e++;
        }
        m -= b;
        value -= m;
        for (i = 0; i < e; i++) {
            b /= 26;
            z = value / b;
            value = value - z*b;
            if (i < e -1) {
                if (value == 0) {
                    value += b;
                } else {
                    z++;
                }
            }
            if (upper) {
                tmp[i] = 64+z;
            } else {
                tmp[i] = 96+z;
            }
        }
        tmp[i] = '\0';
        break;

    case roman_upper:
        upper = 1;
        /* fall thru */
    case roman_lower:
        /* Algorithm follows the idear of the converter
           at http://mini.net/cgi-bin/wikit/1749.html */

        /* Side note: There exists a rarely used roman notation
           to express figures up to a few millions. Does somebody
           really need this? */

        if (value > 3999 || value <= 0) {
            /* fall back to latin numbers */
            sprintf (tmp, "%d", value);
            break;
        }
        if (value == 0) {
            /* what to do with zero??? */
            sprintf (tmp, "%d", 0);
            break;
        }
        v = 0;  tmp[0] = '\0';
        while (value > 0) {
            while (value >= RomanDigit[v].value) {
                if (upper) { strcat(tmp,  RomanDigit[v].digit);  }
                      else { strcat(tmp,  RomanDigit[v].ldigit); }
                value -= RomanDigit[v].value;
            }
            v++;
        }
        break;

    default:
        sprintf (tmp, "%d", value);
        break;
    }
    len = strlen (tmp);
    Tcl_DStringAppend (str, tmp, len);
 appendSeperator:
    if (addSeperater) {
        if (f->tokens[*useFormatToken].sepStart) {
            Tcl_DStringAppend (str, f->tokens[*useFormatToken].sepStart,
                               f->tokens[*useFormatToken].sepLen);
            *useFormatToken += 1;
        } else {
            if (*useFormatToken > 0) {
                Tcl_DStringAppend (str, f->tokens[*useFormatToken-1].sepStart,
                                   f->tokens[*useFormatToken-1].sepLen);
            } else {
                /* insert default seperator '.' */
                Tcl_DStringAppend (str, ".", 1);
            }
        }
    }

    return;
}

/*----------------------------------------------------------------------------
|   xsltFormatNumber
|
\---------------------------------------------------------------------------*/
#if TclOnly8Bits
static int xsltFormatNumber (
    double              number,
    char              * formatStr,
    xsltDecimalFormat * df,
    char             ** resultStr,
    int               * resultLen,
    char             ** errMsg
)
{
    char *p, prefix[800], suffix[800], s[2400], n[800], f[800];
    char *negformat = NULL, save = '\0', save1;
    int i, l, zl, g, nHash, nZero, fHash, fZero, gLen, isNeg;
/*      struct lconv *lc = NULL;  */
    char wrongFormat[] = "Unable to interpret format pattern.";

    DBG(fprintf(stderr, "\nformatStr='%s' \n", formatStr);)
    if (number < 0.0) {
        isNeg = 1;
        number *= -1.0;
    } else {
        isNeg = 0;
    }
    p = formatStr;
    while (*p) {
        if (*p == df->patternSeparator) {
            *p = '\0';
            negformat = ++p;
            break;
        }
        p++;
    }
    /* Check for more than one patternSeparator in the formatStr */
    while (*p) {
        if (*p == df->patternSeparator) {
            *errMsg = tdomstrdup(wrongFormat);
            return -1;
        }
        p++;
    }
    p = formatStr;

    i = 0;
    while (*p 
           && (*p!=df->zeroDigit) 
           && (*p!=df->digit) 
           && (*p!=df->groupingSeparator) 
           && (*p!=df->decimalSeparator)) {
        if (i<79) { prefix[i++] = *p; }
        p++;
    }
    prefix[i] = '\0';
    nHash = nZero = fHash = fZero = 0;
    gLen = -2222;
    while (*p) {
             if (*p==df->digit) {
                 if (nZero) {*errMsg = tdomstrdup(wrongFormat); return -1;}
                 nHash++;}
        else if (*p==df->zeroDigit) { nZero++; }
        else if (*p==df->groupingSeparator) { gLen=-1; }
        else break;
        p++; gLen++;
    }
    if (*p && (*p==df->decimalSeparator)) {
        p++;
        while (*p && (*p==df->zeroDigit)) { p++; fZero++; }
        while (*p && (*p==df->digit)) { p++; fHash++; }
    }
    i = 0;
    while (*p) {
        if (i<79) { suffix[i++] = *p; }
        p++;
    }
    suffix[i] = '\0';
    if (save) *p = save;

    if (isNeg && negformat) {
        /* Only prefix and suffix are taken from the second format string */
        p++;
        i = 0;
        while (*p 
               && *p!=df->zeroDigit
               && *p!=df->digit 
               && *p!=df->groupingSeparator 
               && *p!=df->decimalSeparator) {
            if (i<79) { prefix[i++] = *p; }
            p++;
        }
        prefix[i] = '\0';
        while (*p 
               && ((*p==df->zeroDigit) 
                   || (*p==df->digit) 
                   || (*p==df->groupingSeparator) 
                   || (*p==df->decimalSeparator))) p++;
        i = 0;
        while (*p) {
            if (i<79) { suffix[i++] = *p; }
            p++;
        }
        suffix[i] = '\0';
    }

    if (isNeg) {
        if (negformat) {
            if (prefix[0]=='\0' && suffix[0]=='\0') {
                prefix[0] = df->minusSign;
                prefix[1] = '\0';
            }
        } else {
            i = 0;
            save = prefix[0];
            prefix[0] = df->minusSign;
            while (i < 79) {
                i++;
                if (save == '\0') {
                    prefix[i] = save;
                    break;
                }
                save1 = prefix[i];
                prefix[i] = save;
                save = save1;
            }
            if (i == 79) prefix[79] = '\0';
        }
    }
    if (prefix[0]=='\xc2' && prefix[1]=='\xa4') {
/*          lc = localeconv(); */
/*          if (strlen (lc->currency_symbol) > 79 */
/*              || lc->currency_symbol[0] == '\0') { */
            prefix[0] = '$';
            prefix[1] = '\0';
/*          } else { */
/*              strcpy (prefix, lc->currency_symbol); */
/*          } */
    }

    if (suffix[0] == df->percent) {
        number *= 100.0;
    } else 
    if (suffix[0]=='\xe2' && suffix[1]=='\x80' && suffix[2]=='\xb0') {
        number *= 1000.0;
    }
    
    if (fHash + fZero == 0) {
        i = (int) (number+0.5);
    } else {
        i = (int) number;
    }
    DBG(fprintf(stderr,"normal part nZero=%d i=%d glen=%d\n", nZero, i, gLen);)
    /* fill in grouping char */
    if (gLen > 0) {
        if (i < 0.0) {isNeg = 1; i *= -1;}
        else isNeg = 0;
        sprintf(s,"%0*d", nZero, i);
        l = strlen(s);
        /* if (l > (nHash+nZero)) { l = nHash+nZero; } */
        DBG(fprintf(stderr,"s='%s isNeg=%d'\n", s, isNeg);)
        zl = l + ((l-1) / gLen);
        DBG(fprintf(stderr, "l=%d zl=%d \n", l, zl);)
        n[zl--] = '\0';
        p = s + strlen(s) -1;
        g = 0;
        while (zl>=0) {
            g++;
            n[zl--] = *p--;
            if ((g == gLen) && (zl>=1)) {
                n[zl--] = df->groupingSeparator;
                g = 0;
            }
        }
        DBG(fprintf(stderr,"s='%s' --> n='%s'\n", s, n);)

    } else {
        sprintf(n,"%0*d", nZero, i);
        DBG(fprintf(stderr,"n='%s'\n", n);)
    }

    DBG(fprintf(stderr, "number=%f Hash=%d fZero=%d \n", number, fHash, fZero);)
    if ((fHash+fZero) > 0) {
        i = (int) number;
        /* format fraction part */
        if (number >= 0.0) {
            sprintf(f,"%0.*f", fZero+fHash, number -i);
        } else {
            sprintf(f,"%0.*f", fZero+fHash, -1.0 * (number -i) );
        }
        l = strlen(f);
        while (l>0 && fHash>0) {   /* strip not need 0's */
            if (f[l-1] == '0') {
                f[l-1]='\0'; l--; fHash--;
            } else {
                break;
            }
        }
        DBG(fprintf(stderr, "f='%s'\n", f);)
        sprintf(s,"%s%s%c%s%s", prefix, n, df->decimalSeparator, &(f[2]), suffix);
    } else {
        sprintf(s,"%s%s%s", prefix, n, suffix);
    }
    DBG(fprintf(stderr, "returning s='%s' \n\n", s);)
    *resultStr = tdomstrdup(s);
    *resultLen = strlen(s);
    return 0;
}

#else

static int addCurrencySymbol (
    Tcl_UniChar  *p,
    Tcl_UniChar  *result,
    int          *i
)
{
    Tcl_DString dStr;
    Tcl_UniChar *p1, *currencySymbol;
    int move = 0;
    struct lconv *lc; 

    setlocale (LC_MONETARY, "");
    lc = localeconv();
    Tcl_DStringInit (&dStr);
    if (*(p+1) == 0xa4) {
        if (lc->int_curr_symbol[0] == '\0') {
            currencySymbol = Tcl_UtfToUniCharDString ("$", -1, &dStr);
        } else {
            currencySymbol = 
                Tcl_UtfToUniCharDString (lc->int_curr_symbol, -1, &dStr);
        }
        move = 1;
    } else {
        if (lc->currency_symbol[0] == '\0') {
            currencySymbol = Tcl_UtfToUniCharDString ("$", -1, &dStr);
        } else {
            currencySymbol = 
                Tcl_UtfToUniCharDString (lc->currency_symbol, -1, &dStr);
        }
    }
    p1 = currencySymbol;
    while (*p1 && (*i < 79)) {
        result[(*i)++] = *p1;
        p1++;
    }
    Tcl_DStringFree (&dStr);
    return move;
}

static int xsltFormatNumber (
    double              number,
    char              * formatStr,
    xsltDecimalFormat * df,
    char             ** resultStr,
    int               * resultLen,
    char             ** errMsg
)
{
    Tcl_UniChar prefix1[800], prefix2[800], suffix1[800], suffix2[800];
    Tcl_UniChar save = '\0', save1, t, *prefix, *suffix, n[800], f[800];
    Tcl_UniChar uniCharNull = '\0';
    char stmp[240], ftmp[80];
    char wrongFormat[] = "Unable to interpret format pattern.";
    int i, j, k, l, zl, g, nHash, nZero, fHash, fZero, gLen, isNeg;
    int prefixMinux, percentMul = 0, perMilleMul = 0;
    Tcl_DString  dStr, s;
    Tcl_UniChar *format, *negformat = NULL, *p, *p1;
    DBG(Tcl_DString dbStr;)

    DBG(fprintf(stderr, "number: '%f'\nformatStr='%s' \n", number, formatStr);)
    prefix1[0] = '\0';
    prefix2[0] = '\0';
    suffix1[0] = '\0';
    suffix2[0] = '\0';
    n[0] = '\0';
    f[0] = '\n';
    prefix = NULL;
    suffix = NULL;
    Tcl_DStringInit (&s);
    Tcl_DStringInit (&dStr);
    if (number < 0.0) {
        isNeg = 1;
        number *= -1.0;
    } else if (number == 0.0) {
        sprintf (stmp, "%f", number);
        if (stmp[0] == '-') isNeg = 1;
        else isNeg = 0;
    } else {
        isNeg = 0;
    }
    format = Tcl_UtfToUniCharDString (formatStr, -1, &dStr);
    p = format;
    while (*p) {
        if (*p == df->patternSeparator) {
            save = *p;
            *p = '\0';
            negformat = ++p;
            break;
        }
        p++;
    }
    /* Check for more than one patternSeparator in the formatStr */
    while (*p) {
        if (*p == df->patternSeparator) {
            *errMsg = 
                tdomstrdup("More than one patternSeparator in the pattern");
            goto xsltFormatNumberError;
        }
        p++;
    }
    p = format;

    i = 0;
    while (*p 
           && (*p!=df->zeroDigit) 
           && (*p!=df->digit) 
           && (*p!=df->groupingSeparator) 
           && (*p!=df->decimalSeparator)) {
        if (*p == df->percent) (percentMul = 1);
        else if (*p == df->perMille) (perMilleMul = 1);
        if (i<79) { 
            if (*p == 0xa4) {
                p += addCurrencySymbol (p, prefix1, &i);
           } else {
                prefix1[i++] = *p;
            }
        }
        p++;
    }
    prefix1[i] = '\0';
    nHash = nZero = fHash = fZero = 0;
    gLen = -2222;
    while (*p) {
        if (*p==df->digit) {
            if (nZero) {
                *errMsg = tdomstrdup(wrongFormat);
                goto xsltFormatNumberError;
            }
            nHash++; 
        }
        else if (*p==df->zeroDigit) { nZero++; }
        else if (*p==df->groupingSeparator) { gLen=-1; }
        else break;
        p++; gLen++;
    }
    if (*p && (*p==df->decimalSeparator)) {
        p++;
        while (*p && (*p==df->zeroDigit)) { p++; fZero++; }
        while (*p && (*p==df->digit)) { p++; fHash++; }
    }
    i = 0;
    while (*p) {
        /* Check for more than one decimalSeparator */
        if (*p == df->decimalSeparator) {
            *errMsg = 
                tdomstrdup("More than one decimalSeparator in subpattern");
            goto xsltFormatNumberError;
        }
        /* Check for groupingSeparator after decimalSeparator */
        if (*p == df->groupingSeparator) {
            *errMsg = tdomstrdup("GroupingSeparator after decimalSeparator");
            goto xsltFormatNumberError;
        }
        if (*p == df->percent) (percentMul = 1);
        else if (*p == df->perMille) (perMilleMul = 1);
        if (i<79) {
            if (*p == 0xa4) {
                p += addCurrencySymbol (p, suffix1, &i);
            } else {
                suffix1[i++] = *p;
            }
        }
        p++;
    }
    suffix1[i] = '\0';
    if (save) *p = save;

    if (isNeg && negformat) {
        /* Only prefix and suffix are taken from the second format string */
        percentMul = 0; perMilleMul = 0;
        p++;
        i = 0;
        while (*p 
               && *p!=df->zeroDigit 
               && *p!=df->digit 
               && *p!=df->groupingSeparator 
               && *p!=df->decimalSeparator) {
            if (*p == df->percent) (percentMul = 1);
            else if (*p == df->perMille) (perMilleMul = 1);
            if (i<79) { 
                if (*p == 0xa4) {
                    p += addCurrencySymbol (p, prefix2, &i);
                } else {
                    prefix2[i++] = *p;
                }
            }
            p++;
        }
        prefix2[i] = '\0';
        while (*p 
               && ((*p==df->zeroDigit) 
                   || (*p==df->digit) 
                   || (*p==df->groupingSeparator) 
                   || (*p==df->decimalSeparator))) p++;
        i = 0;
        while (*p) {
            if (*p == df->percent) (percentMul = 1);
            else if (*p == df->perMille) (perMilleMul = 1);
            if (i<79) {
                if (*p == 0xa4) {
                    p += addCurrencySymbol (p, suffix2, &i);
                } else {
                    suffix2[i++] = *p;
                }
            }
            p++;
        }
        suffix2[i] = '\0';
    }

    if (isNeg) {
        if (negformat) {
            prefixMinux = 1;
            p = prefix1;
            p1 = prefix2;
            while (prefixMinux) {
                if (*p != *p1) {
                    prefixMinux = 0;
                    break;
                }
                if (*p == 0) break;
                p++; p1++;
            }
            if (prefixMinux) {
                p = suffix1;
                p1 = suffix2;
                while (prefixMinux) {
                    if (*p != *p1) {
                        prefixMinux = 0;
                        break;
                    }
                    if (*p == 0) break;
                    p++; p1++;
                }
            }
            prefix = prefix2;
            suffix = suffix2;
        } else {
            prefixMinux = 1;
            prefix = prefix1;
            suffix = suffix1;
        }
        if (prefixMinux) {
            i = 0;
            save = prefix[0];
            prefix[0] = df->minusSign;
            while (i < 79) {
                i++;
                save1 = prefix[i];
                prefix[i] = save;
                if (save == 0) break;
                save = save1;
            }
            if (i == 79) prefix[79] = '\0';
        }
    } else {
        prefix = prefix1;
        suffix = suffix1;
    }
    
    DBG(
        Tcl_DStringInit (&dbStr);
        fprintf (stderr, "prefix: '%s' ", Tcl_UniCharToUtfDString(prefix, Tcl_UniCharLen (prefix), &dbStr));
        Tcl_DStringFree (&dbStr);
        Tcl_DStringInit (&dbStr);
        fprintf (stderr, "suffix: '%s'\n", Tcl_UniCharToUtfDString(suffix, Tcl_UniCharLen (suffix), &dbStr));
        Tcl_DStringFree (&dbStr);
    )

    if (percentMul) {
        number *= 100.0;
    } else if (perMilleMul) { 
        number *= 1000.0; 
    }
    
    if (fHash + fZero == 0) {
        i = (int) (number+0.5);
    } else {
        i = (int) number;
        /* format fraction part */
        DBG(fprintf(stderr, "formating fraction part: '%f', fZero+fHash: '%d'\n",
                    number - i, fZero+fHash);)
        sprintf(ftmp,"%.*f", fZero+fHash, number -i);
        DBG(fprintf(stderr, "raw formated fraction part: '%s'\n", ftmp);)
        if (ftmp[0] == '1') {
            i++;
        }
    }
    
    DBG(fprintf(stderr,"normal part nZero=%d i=%d glen=%d\n", nZero, i, gLen);)
    /* fill in grouping char */
    if (gLen > 0) {
        sprintf(stmp,"%0*d", nZero, i);
        l = strlen (stmp);
        for (j = 0; j < l; j++) {
            t = df->zeroDigit + stmp[j] - 48;
            Tcl_DStringAppend (&s, (char*)&t, sizeof (Tcl_UniChar));
        }
        DBG(
            Tcl_DStringInit(&dbStr);
            fprintf (stderr, "'%s' ---> ..\n", stmp);
            fprintf(stderr,"s='%s' isNeg=%d'\n", 
                    Tcl_UniCharToUtfDString (
                        (Tcl_UniChar*)Tcl_DStringValue (&s),
                        Tcl_UniCharLen((Tcl_UniChar*)Tcl_DStringValue(&s)), &dStr
                        ), 
                    isNeg);
            Tcl_DStringFree (&dbStr);
        )
        zl = l + ((l-1) / gLen);
        DBG(fprintf(stderr, "l=%d zl=%d \n", l, zl);)
        n[zl--] = '\0';
        p = (Tcl_UniChar*)Tcl_DStringValue (&s) + l - 1;
        g = 0;
        while (zl>=0) {
            g++;
            n[zl--] = *p--;
            if ((g == gLen) && (zl>=1)) {
                n[zl--] = df->groupingSeparator;
                g = 0;
            }
        }
        Tcl_DStringSetLength (&s, 0);
        DBG(
            Tcl_DStringInit (&dbStr);
            fprintf(stderr,"s='%s' --> ", 
                    Tcl_UniCharToUtfDString (
                        (Tcl_UniChar*)Tcl_DStringValue (&s),
                        Tcl_UniCharLen((Tcl_UniChar*)Tcl_DStringValue(&s)),
                        &dStr));
            Tcl_DStringFree (&dbStr); 
            Tcl_DStringInit (&dbStr);
            fprintf(stderr,"n='%s'\n", 
                    Tcl_UniCharToUtfDString (n, Tcl_UniCharLen (n), &dbStr));
            Tcl_DStringFree (&dbStr); 
        )
    } else {
        sprintf(stmp,"%0*d", nZero, i);
        l = strlen (stmp);
        for (j = 0; j < l; j++) {
            n[j] = df->zeroDigit + (int) stmp[j] - 48;
        }
        n[l] = '\0';
        DBG(
            Tcl_DStringInit (&dbStr);
            fprintf(stderr,"n='%s'\n", 
                    Tcl_UniCharToUtfDString(n, Tcl_UniCharLen (n), &dbStr));
            Tcl_DStringFree (&dbStr);
        )
    }
    DBG(fprintf(stderr, "number=%f fHash=%d fZero=%d \n", number, fHash, 
                fZero);)
    if ((fHash+fZero) > 0) {
        l = strlen(ftmp);
        while (l>0 && fHash>0) {   /* strip not need 0's */
            if (ftmp[l-1] == '0') {
                ftmp[l-1]='\0'; l--; fHash--;
            } else {
                break;
            }
        }
        k = 0;
        if ((number - i != 0.0) || (fZero > 0)) {
            while (ftmp[k] != '.') k++;
            k++;
            for (j = k ; j < l; j++) {
                f[j] = df->zeroDigit + (int) ftmp[j] - 48;
            }
            f[l] = '\0';
        }
        DBG(fprintf(stderr, "f='%s'\n", f);)

        if (prefix) {
            Tcl_DStringAppend (&s, (char*) prefix,
                               Tcl_UniCharLen (prefix) * sizeof(Tcl_UniChar));
        }
        Tcl_DStringAppend (&s, (char*) n,
                           Tcl_UniCharLen (n) * sizeof(Tcl_UniChar));
        if (k) {
            Tcl_DStringAppend (&s, (char*)&df->decimalSeparator,
                               sizeof (Tcl_UniChar));
            Tcl_DStringAppend (&s, (char*)&(f[k]),
                               Tcl_UniCharLen (&(f[k])) * sizeof(Tcl_UniChar));
        }
        if (suffix) {
            Tcl_DStringAppend (&s, (char *) suffix,
                               Tcl_UniCharLen (suffix) * sizeof(Tcl_UniChar));
        }
        Tcl_DStringAppend (&s, (char *)&uniCharNull, sizeof (Tcl_UniChar));
    } else {
        if (prefix) {
            Tcl_DStringAppend (&s, (char*) prefix,
                               Tcl_UniCharLen (prefix) * sizeof(Tcl_UniChar));
        }
        Tcl_DStringAppend (&s, (char*) n,
                           Tcl_UniCharLen (n) * sizeof(Tcl_UniChar));
        if (suffix) {
            Tcl_DStringAppend (&s, (char *) suffix,
                               Tcl_UniCharLen (suffix) * sizeof(Tcl_UniChar));
        }
        Tcl_DStringAppend (&s, (char *)&uniCharNull, sizeof (Tcl_UniChar));
    }
    DBG(
        Tcl_DStringInit (&dbStr);
        fprintf(stderr, "returning s='%s' \n\n", 
                Tcl_UniCharToUtfDString(
                    (Tcl_UniChar*)Tcl_DStringValue (&s),
                    Tcl_UniCharLen((Tcl_UniChar*)Tcl_DStringValue(&s)), &dStr
                    ));
        Tcl_DStringFree (&dbStr);
    )
    Tcl_DStringSetLength (&dStr, 0);
    *resultStr = tdomstrdup(
        Tcl_UniCharToUtfDString(
            (Tcl_UniChar*)Tcl_DStringValue (&s),
            Tcl_UniCharLen((Tcl_UniChar*)Tcl_DStringValue(&s)), &dStr
            )
        );
    Tcl_DStringFree (&dStr);
    Tcl_DStringFree (&s);
    *resultLen = strlen(*resultStr);
    return 0;

 xsltFormatNumberError:
    Tcl_DStringFree (&dStr);
    Tcl_DStringFree (&s);
    return -1;
}

#endif /* TclOnly8Bits */


static xsltNodeSet *
createXsltNodeSet () 
{
    xsltNodeSet * ns;

    ns = (xsltNodeSet *) MALLOC (sizeof(xsltNodeSet));
    ns->nodes = (domNode**)MALLOC(INITIAL_SIZE_FOR_KEYSETS * sizeof(domNode*));
    ns->allocated = INITIAL_SIZE_FOR_KEYSETS;
    ns->nr_nodes = 0;
    return ns;
}

    

/* Helper proc for buildKeyInfoForDoc. Adds node to the node set ns at
   the right position (in document order), if not already
   present. This is the same as the core of rsAddNode does. The used
   method to add may look a bit simpleminded, but experience shows,
   that in the vast majority of the cases node simply has to be
   appended to the array. */

static void nsAddNode ( 
    xsltNodeSet *ns,
    domNode *node 
    ) 
{
    int insertIndex, i;
    
    insertIndex = ns->nr_nodes;
    for (i = ns->nr_nodes - 1; i >= 0; i--) {
        if (node == ns->nodes[i]) return;
        if (!domPrecedes (node, ns->nodes[i])) {
            break;
        }
        insertIndex--;
    }
    if (ns->nr_nodes + 1 >= ns->allocated) {
        ns->nodes = (domNode**)REALLOC((void*)ns->nodes,
                               2 * ns->allocated * sizeof(domNode*));
        ns->allocated *= 2;
    }
    if (insertIndex == ns->nr_nodes) {
        ns->nodes[ns->nr_nodes++] = node;
    } else {
        for (i = ns->nr_nodes - 1; i >= insertIndex; i--) {
            ns->nodes[i+1] = ns->nodes[i];
        }
        ns->nodes[insertIndex] = node;
        ns->nr_nodes++;
    }
}

static int buildKeyInfoForDoc (
    xsltSubDoc     *sd,
    char           *keyId,
    Tcl_HashTable  *keyInfos,
    xsltState      *xs,
    char          **errMsg
)
{
    int             hnew, rc, docOrder, i;
    char           *useValue;
    domNode        *node, *savedCurrent;
    xpathResultSet  rs, context;
    Tcl_HashTable  *valueTable;
    Tcl_HashEntry  *h;
    xsltKeyInfo    *kinfo, *kinfoStart;
    xsltNodeSet    *keyValues;

    h = Tcl_FindHashEntry (keyInfos, keyId);
    /* key must exist, this is already checked */
    kinfoStart = (xsltKeyInfo *) Tcl_GetHashValue (h);

    /* this must be a new entry, no check for hnew==1 needed */
    h = Tcl_CreateHashEntry (&(sd->keyData), keyId, &hnew);
    valueTable = (Tcl_HashTable *)MALLOC(sizeof (Tcl_HashTable));
    Tcl_InitHashTable (valueTable, TCL_STRING_KEYS);
    Tcl_SetHashValue (h, valueTable);

    savedCurrent = xs->current;
    node = sd->doc->rootNode;
    while (node) {
        kinfo = kinfoStart;
        while (kinfo) {
            rc = xpathMatches (kinfo->matchAst, kinfo->node, node, &(xs->cbs),
                               errMsg);
            if (rc < 0) {
                TRACE1("xpathMatches had errors '%s' \n", *errMsg);
                return rc;
            }
            if (rc > 0) {
                TRACE("found match for key !\n");
                xpathRSInit (&rs);
                xpathRSInit (&context);
                rsAddNode (&context, node);
                DBG(printXML(node, 0, 2);)
                docOrder = 1;
                xs->current = node;
                rc = xpathEvalSteps (kinfo->useAst, &context, node, 
                                     kinfo->node, 0, &docOrder, &(xs->cbs),
                                     &rs, errMsg);
                if (rc != XPATH_OK) {
                    xpathRSFree (&rs);
                    xpathRSFree (&context);
                    return rc;
                }
                DBG(rsPrint(&rs));
                if (rs.type == xNodeSetResult) {
                    for (i = 0; i < rs.nr_nodes; i++) {
                        useValue = xpathFuncStringForNode (rs.nodes[i]);
                        TRACE1("use value = '%s'\n", useValue);
                        h = Tcl_CreateHashEntry (valueTable, useValue, &hnew);
                        if (hnew) {
                            keyValues = createXsltNodeSet();
                        } else {
                            keyValues = (xsltNodeSet *) Tcl_GetHashValue (h);
                        }
                        nsAddNode (keyValues, node);
                        if (hnew) Tcl_SetHashValue (h, keyValues);
                        FREE(useValue);
                    }
                }
                else if (rs.type != EmptyResult) {
                    useValue = xpathFuncString (&rs);
                    TRACE1("use value = '%s'\n", useValue);
                    h = Tcl_CreateHashEntry (valueTable, useValue, &hnew);
                    if (hnew) {
                        keyValues = createXsltNodeSet();
                    } else {
                        keyValues = (xsltNodeSet *) Tcl_GetHashValue (h);
                    }
                    nsAddNode (keyValues, node);
                    if (hnew) Tcl_SetHashValue (h, keyValues);
                    FREE(useValue);
                }
                xpathRSFree( &context );
                xpathRSFree( &rs );
            }
            kinfo = kinfo->next;
        }
        if ((node->nodeType == ELEMENT_NODE) && (node->firstAttr)) {
            node = (domNode*) node->firstAttr;
            continue;
        }
        if ((node->nodeType == ATTRIBUTE_NODE)) {
            if (((domAttrNode*)node)->nextSibling) {
                node = (domNode*) ((domAttrNode*)node)->nextSibling;
                continue;
            }
            node = ((domAttrNode*)node)->parentNode;
        }
        if ((node->nodeType == ELEMENT_NODE) && (node->firstChild)) {
            node = node->firstChild;
            continue;
        }
        if (node->nextSibling) {
            node = node->nextSibling;
            continue;
        }
        while ( node->parentNode &&
                (node->parentNode->nextSibling == NULL) ) {
            node = node->parentNode;
        }
        if (node->parentNode) {
            node = node->parentNode->nextSibling;
        } else {
            break;
        }
    }
    xs->current = savedCurrent;
    return 0;
}


/*----------------------------------------------------------------------------
|   sortNodeSetByNodeNumber
|
\---------------------------------------------------------------------------*/
static void sortNodeSetByNodeNumber(
    domNode *nodes[],
    int      n
)
{
    int i, j, ln, rn;
    domNode *tmp;

    while (n > 1) {
        tmp = nodes[0]; nodes[0] = nodes[n/2]; nodes[n/2] = tmp;
        for (i = 0, j = n; ; ) {
            do {
                --j;
            } while (domPrecedes (nodes[0], nodes[j]));
            do {
                ++i;
            } while (i < j && domPrecedes (nodes[i], nodes[0]));
            if (i >= j)  break;
            tmp = nodes[i]; nodes[i] = nodes[j]; nodes[j] = tmp;
        }
        tmp = nodes[j]; nodes[j] = nodes[0]; nodes[0] = tmp;
        ln = j;
        rn = n - ++j;
        if (ln < rn) {
            sortNodeSetByNodeNumber(nodes, ln);
            nodes += j;
            n = rn;
        } else {
            sortNodeSetByNodeNumber(&(nodes[j]), rn);
            n = ln;
        }
    }
}

/*----------------------------------------------------------------------------
|   sortByDocOrder
|
\---------------------------------------------------------------------------*/
void sortByDocOrder (
    xpathResultSet  * rs
)
{
    if (rs->type != xNodeSetResult) return;
    sortNodeSetByNodeNumber(rs->nodes, rs->nr_nodes);
}

/*----------------------------------------------------------------------------
|   StripXMLSpace
|
\---------------------------------------------------------------------------*/
static void StripXMLSpace (
    xsltState  * xs,
    domNode    * node
)
{
    domNode       *child, *newChild, *parent;
    int            i, len, onlySpace, found, strip;
    char          *p, prefix[MAX_PREFIX_LEN];
    const char    *localName;
    double        *f;
    domNS         *ns;
    Tcl_HashEntry *h;
    Tcl_DString    dStr;


    if (node->nodeType == TEXT_NODE) {
        p = ((domTextNode*)node)->nodeValue;
        len = ((domTextNode*)node)->valueLength;
        onlySpace = 1;
        for (i=0; i<len; i++) {
            if (!IS_XML_WHITESPACE(*p)) {
                onlySpace = 0;
                break;
            }
            p++;
        }
        if (onlySpace) {
            parent = node->parentNode;
            while (parent) {
                p = getAttr(parent,"xml:space", a_space);
                if (p!=NULL) {
                    if (strcmp(p,"preserve")==0) return;
                    if (strcmp(p,"default")==0)  break;
                }
                parent = parent->parentNode;
            }
            DBG(fprintf(stderr, "removing domNode0x%x(len %d) under '%s' \n", 
                        node, len, node->parentNode->nodeName);)
                domDeleteNode (node, NULL, NULL);
        }
    } else
    if (node->nodeType == ELEMENT_NODE) {
        if (node->firstChild == NULL) return;
        strip = xs->wsInfo.stripAll;
        found = 0;
        if (node->namespace) {
            domSplitQName (node->nodeName, prefix, &localName);
        } else {
            prefix[0] = '\0';
            localName = node->nodeName;
        }
        ns = NULL;
        Tcl_DStringInit (&dStr);
        if (prefix[0] != '\0') {
            ns =  domLookupPrefix (node, prefix);
            if (ns) {
                Tcl_DStringAppend (&dStr, ns->uri, -1);
                Tcl_DStringAppend (&dStr, ":*", 2);
                if (xs->wsInfo.stripAll) {
                    h = Tcl_FindHashEntry (&xs->wsInfo.preserveTokens,
                                           Tcl_DStringValue (&dStr));
                } else {
                    h = Tcl_FindHashEntry (&xs->wsInfo.stripTokens,
                                           Tcl_DStringValue (&dStr));
                }
                if (h) {
                    f = Tcl_GetHashValue (h);
                    if (*f >= xs->wsInfo.wildcardPrec) {
                        strip = !xs->wsInfo.stripAll;
                        found = 1;
                    }
                }
                if (!found) {
                    Tcl_DStringFree (&dStr);
                    Tcl_DStringInit (&dStr);
                    Tcl_DStringAppend (&dStr, ns->uri, -1);
                    Tcl_DStringAppend (&dStr, ":", 1);
                }
            }
        }
        if (!found) {
            Tcl_DStringAppend (&dStr, localName, -1);
            if (xs->wsInfo.stripAll) {
                h = Tcl_FindHashEntry (&xs->wsInfo.preserveTokens,
                                       Tcl_DStringValue (&dStr));
            } else {
                h = Tcl_FindHashEntry (&xs->wsInfo.stripTokens,
                                       Tcl_DStringValue (&dStr));
            }
            if (h) {
                f = Tcl_GetHashValue (h);
                if (*f >= xs->wsInfo.wildcardPrec) {
                    strip = !xs->wsInfo.stripAll;
                }
            }
        }
        Tcl_DStringFree (&dStr);
        if (strip) {
            child = node->firstChild;
            while (child) {
                newChild = child->nextSibling;
                StripXMLSpace (xs, child);
                child = newChild;
            }
        } else {
            child = node->firstChild;
            while (child) {
                if (child->nodeType == ELEMENT_NODE) {
                    StripXMLSpace (xs, child);
                }
                child = child->nextSibling;
            }
        }
    }
}

/*----------------------------------------------------------------------------
|   xsltXPathFuncs
|
\---------------------------------------------------------------------------*/
static int xsltXPathFuncs (
    void            * clientData,
    char            * funcName,
    domNode         * ctxNode,
    int               ctxPos,
    xpathResultSet  * ctx,
    domNode         * exprContext,
    int               argc,
    xpathResultSets * argv,
    xpathResultSet  * result,
    char           ** errMsg
)
{
    xsltState         * xs = clientData;
    char              * keyId, *filterValue, *str = NULL;
    char                prefix[MAX_PREFIX_LEN];
    const char        * localName, *baseURI, *nsStr;
    int                 rc, i, len, NaN, freeStr, x;
    double              n;
    xsltNodeSet       * keyValues;
    Tcl_HashEntry     * h;
    Tcl_HashTable     * docKeyData;
    xsltSubDoc        * sdoc;
    domDocument       * ownerDoc;
    Tcl_DString         dStr;
    domNS             * ns;
    xsltDecimalFormat * df;

    DBG ( fprintf(stderr,"xsltXPathFuncs funcName='%s'\n",funcName); )

    if (strcmp(funcName, "key")==0) {
        /*--------------------------------------------------------------------
        |   'key' function
        \-------------------------------------------------------------------*/
        DBG(fprintf(stderr,"xslt key function called!\n");)
        if (argc != 2) {
            reportError (exprContext, "key() needs two arguments!", errMsg);
            return -1;
        }
        /* check, if there is a key definition with the given name */
        keyId = xpathFuncString(argv[0]);
        TRACE1("keyId='%s' \n", keyId);
        domSplitQName (keyId, prefix, &localName);
        Tcl_DStringInit (&dStr);
        if (prefix[0] != '\0') {
            ns = domLookupPrefix (exprContext, prefix);
            if (!ns) {
                reportError (exprContext, "There isn't a namespace bound to"
                             " the prefix.", errMsg);
                FREE(keyId);
                return -1;
            }
            Tcl_DStringAppend (&dStr, ns->uri, -1);
        }
        Tcl_DStringAppend (&dStr, localName, -1);
        FREE(keyId);
        h = Tcl_FindHashEntry (&xs->keyInfos, Tcl_DStringValue (&dStr));
        if (!h) {
            reportError (exprContext, "Unknown key in key() function call!",
                         errMsg);
            Tcl_DStringFree (&dStr);
            return -1;
        }

        /* Short cut for empty result sets. */
        if (argv[1]->type == EmptyResult) {
            Tcl_DStringFree (&dStr);
            return 0;
        }
            
        /* find the doc, the context node belongs to */
        sdoc = xs->subDocs;
        if (ctxNode->nodeType == ATTRIBUTE_NODE) {
            ownerDoc = ((domAttrNode *)ctxNode)->parentNode->ownerDocument;
        } else {
            ownerDoc = ctxNode->ownerDocument;
        }
        while (sdoc) {
            if (sdoc->doc == ownerDoc) break;
            sdoc = sdoc->next;
        }
        DBG(if (!sdoc) fprintf (stderr, "key() function: ctxNode doesn't belong to a doc out of subDocs!!! This could not happen!. ERROR\n");
            else (fprintf (stderr, "key() function: ctxNode belongs to doc %s\n", sdoc->baseURI));)

        h = Tcl_FindHashEntry (&(sdoc->keyData), Tcl_DStringValue (&dStr));
        if (!h) {
            if (buildKeyInfoForDoc(sdoc, Tcl_DStringValue (&dStr),
                                   &(xs->keyInfos),xs,errMsg)<0) {
                Tcl_DStringFree (&dStr);
                return -1;
            }
            h = Tcl_FindHashEntry (&(sdoc->keyData), Tcl_DStringValue (&dStr));
        }
        Tcl_DStringFree (&dStr);

        docKeyData = (Tcl_HashTable *) Tcl_GetHashValue (h);

        if (argv[1]->type == xNodeSetResult) {
            for (i = 0; i < argv[1]->nr_nodes; i++) {
                filterValue = xpathFuncStringForNode (argv[1]->nodes[i]);
                TRACE1("filterValue='%s' \n", filterValue);
                h = Tcl_FindHashEntry (docKeyData, filterValue);
                if (h) {
                    keyValues = (xsltNodeSet *) Tcl_GetHashValue (h);
                    if (result->type == EmptyResult) {
                        result->type = xNodeSetResult;
                        result->nodes = keyValues->nodes;
                        result->intvalue = 1;
                        result->nr_nodes = keyValues->nr_nodes;
                        result->allocated = keyValues->allocated;
                    } else {
                        for (x = 0; x < keyValues->nr_nodes; x++) {
                            rsAddNode(result, keyValues->nodes[x]);
                        }
                    }
                }
                FREE(filterValue);
            }
        } else {
           filterValue = xpathFuncString(argv[1]);
           TRACE1("filterValue='%s' \n", filterValue);
           h = Tcl_FindHashEntry (docKeyData, filterValue);
           if (h) {
               keyValues = (xsltNodeSet *) Tcl_GetHashValue (h);
               if (result->type == EmptyResult) {
                   result->type = xNodeSetResult;
                   result->nodes = keyValues->nodes;
                   result->intvalue = 1;
                   result->nr_nodes = keyValues->nr_nodes;
                   result->allocated = keyValues->allocated;
               } else {
                   for (x = 0; x < keyValues->nr_nodes; x++) {
                       rsAddNode(result, keyValues->nodes[x]);
                   }
               }
           }
           FREE(filterValue);
        }
        return 0;
    } else
    if (strcmp(funcName, "current")==0) {
        /*--------------------------------------------------------------------
        |   'current' function
        \-------------------------------------------------------------------*/
        DBG(fprintf(stderr, "xsltXPathFuncs 'current' = 'domNode0x%x' \n",
                    xs->current);)
        if (argc != 0) {
            reportError (exprContext, "current() must not have any arguments",
                         errMsg);
            return -1;
        }
        rsAddNode(result, xs->current);
        return 0;
    } else
    if (strcmp (funcName, "format-number")==0) {
        /*--------------------------------------------------------------------
        |   'format-number' function
        \-------------------------------------------------------------------*/
        DBG(fprintf(stderr, "before format-number argc=%d \n", argc);)
        if (argc == 3) {
            str = xpathFuncString (argv[2]);
            domSplitQName (str, prefix, &localName);
            ns = NULL;
            if (prefix[0] != '\0') {
                ns = domLookupPrefix (exprContext, prefix);
                if (!ns) {
                    reportError (exprContext, "There isn't a namespace bound"
                                 " to the prefix.", errMsg);
                    FREE(str);
                    return -1;
                }
            }
            df = xs->decimalFormats->next;
            while (df) {
                if (strcmp(df->name, str)==0
                    && ((df->uri == NULL && ns == NULL)
                        || (df->uri != NULL 
                            && ns != NULL
                            && (strcmp (df->uri, ns->uri)==0)))) {
                    break;
                }
                df = df->next;
            }
            FREE(str);
            if (df == NULL) {
                reportError (exprContext, "There isn't a decimal format with"
                             " this name.", errMsg);
                return -1;
            }
        } else
        if (argc == 2) {
            df = xs->decimalFormats;
        } else {
            reportError (exprContext, "format-number: wrong # parameters:"
                         " format-number(number, string, ?string?)!", errMsg);
            return -1;
        }
        NaN = 0;
        n   = xpathFuncNumber (argv[0], &NaN);
        if (NaN) {
            if      (NaN == 2) rsSetString (result, df->NaN);
            else if (NaN == 1) rsSetString (result, df->infinity);
            else {
                Tcl_DStringInit (&dStr);
                Tcl_DStringAppend (&dStr, "-", 1);
                Tcl_DStringAppend (&dStr, df->infinity, -1);
                rsSetString (result, Tcl_DStringValue (&dStr));
            }
            return 0;
        }
        str = xpathFuncString (argv[1]);
        DBG(fprintf(stderr, "1 str='%s' \n", str);)
        result->type = StringResult;
        rc = xsltFormatNumber(n, str, df, &(result->string),
                              &(result->string_len), errMsg);
        FREE(str);
        if (rc < 0) {
            result->type = EmptyResult;
            return rc;
        }
        DBG(fprintf(stderr, "after format-number \n");)
        return 0;
    } else
    if (strcmp (funcName, "document")==0) {
        /*--------------------------------------------------------------------
        |   'document' function
        \-------------------------------------------------------------------*/
        DBG(fprintf(stderr, "xsltXPathFuncs 'document' \n");)
        if (argc == 1) {
            if (argv[0]->type == xNodeSetResult) {
                for (i = 0; i < argv[0]->nr_nodes; i++) {
                    freeStr = 0;
                    if (argv[0]->nodes[i]->nodeType == ATTRIBUTE_NODE) {
                        nsStr = ((domAttrNode*)argv[0]->nodes[i])->nodeValue;
                        baseURI = findBaseURI (((domAttrNode*)argv[0]->nodes[i])->parentNode);
                    } else {
                        str = xpathGetStringValue (argv[0]->nodes[i], &len);
                        nsStr = str;
                        freeStr = 1;
                        baseURI = findBaseURI (argv[0]->nodes[i]);
                    }
                    /* the case document('') */
                    if (*nsStr == '\0') {
                        if (freeStr) {
                            FREE(str);
                            freeStr = 0;
                        }
                        nsStr = baseURI;
                    }
                    if (xsltAddExternalDocument(xs, baseURI, nsStr, 0,
                                                result, errMsg) < 0) {
                        if (freeStr) FREE(str);
                        return -1;
                    }
                    if (xs->wsInfo.hasData) {
                        StripXMLSpace (xs, xs->subDocs->doc->documentElement);
                    }
                    if (freeStr) FREE(str);
                }
            } else {
                str = xpathFuncString (argv[0]);
                nsStr = str;
                if (xs->currentXSLTNode) {
                    baseURI = findBaseURI (xs->currentXSLTNode);
                } else
                if (xs->currentTplRule) {
                    baseURI = findBaseURI (xs->currentTplRule->content);
                } else {
                    baseURI = findBaseURI (xs->xsltDoc->rootNode);
                }
                if (*nsStr == '\0') {
                    nsStr = baseURI;
                }
                DBG (fprintf (stderr, "document() call, with 1 string arg = '%s'\n", str);)
                if (xsltAddExternalDocument(xs, baseURI, nsStr, 1,
                                            result, errMsg) < 0) {
                    FREE(str);
                    return -1;
                }
                if (xs->wsInfo.hasData) {
                    StripXMLSpace (xs, xs->subDocs->doc->documentElement);
                }
                FREE(str);
            }
        } else 
        if (argc == 2) {
            if (argv[1]->type != xNodeSetResult) {
                reportError (exprContext, "second arg of document() has to be"
                             " a nodeset!", errMsg);
                return -1;
            }
            if (argv[1]->nodes[0]->nodeType == ATTRIBUTE_NODE) {
                baseURI = findBaseURI (((domAttrNode*)argv[1]->nodes[0])->parentNode);
            } else {
                baseURI = findBaseURI (argv[1]->nodes[0]);
            }
            if (argv[0]->type == xNodeSetResult) {
                for (i = 0; i < argv[0]->nr_nodes; i++) {
                    freeStr = 0;
                    if (argv[0]->nodes[i]->nodeType == ATTRIBUTE_NODE) {
                        nsStr = ((domAttrNode*)argv[0]->nodes[i])->nodeValue;
                    } else {
                        str = xpathGetStringValue (argv[0]->nodes[i], &len);
                        freeStr = 1;
                        nsStr = str;
                    }
                    if (*nsStr == '\0') {
                        FREE(str);
                        freeStr = 0;
                        nsStr = baseURI;
                    }
                    if (xsltAddExternalDocument(xs, baseURI, nsStr, 0,
                                                result, errMsg) < 0) {
                        if (freeStr) FREE(str);
                        return -1;
                    }
                    if (xs->wsInfo.hasData) {
                        StripXMLSpace (xs, xs->subDocs->doc->documentElement);
                    }
                    if (freeStr) FREE(str);
                }
            } else {
                str = xpathFuncString (argv[0]);
                nsStr = str;
                if (*str == '\0') {
                    nsStr = baseURI;
                }
                if (xsltAddExternalDocument(xs, baseURI, nsStr, 0,
                                            result, errMsg) < 0) {
                    FREE(str);
                    return -1;
                }
                if (xs->wsInfo.hasData) {
                    StripXMLSpace (xs, xs->subDocs->doc->documentElement);
                }
                FREE(str);
            }
        } else {
            reportError (exprContext, "wrong # of args in document() call!",
                         errMsg);
            return -1;
        }
        return 0;
     } else {
        /* chain back to original callback */
        if (xs->orig_funcCB) {
            return (xs->orig_funcCB)(xs->orig_funcClientData, funcName,
                                     ctxNode, ctxPos, ctx, exprContext,
                                     argc, argv, result, errMsg);
        }
    }
    return 0;
}



/*----------------------------------------------------------------------------
|   evalXPath
|
\---------------------------------------------------------------------------*/
static int evalXPath (
    xsltState       * xs,
    xpathResultSet  * context,
    domNode         * currentNode,
    int               currentPos,
    char            * xpath,
    xpathResultSet  * rs,
    char           ** errMsg
)
{
    int rc, hnew, docOrder = 1;
    ast t;
    domNode *savedCurrent;
    Tcl_HashEntry *h;

    h = Tcl_CreateHashEntry (&(xs->xpaths), xpath, &hnew);
    if (!hnew) {
        t = (ast)Tcl_GetHashValue(h);
    } else {
        rc = xpathParse (xpath, xs->currentXSLTNode, XPATH_EXPR, NULL, NULL,
                         &t, errMsg);
        if (rc < 0) {
            reportError (xs->currentXSLTNode, *errMsg, errMsg);
            return rc;
        }
        Tcl_SetHashValue(h, t);
    }
    xpathRSInit( rs );

    DBG(fprintf (stderr, "evalXPath evaluating xpath:\n");)
    DBG(printAst(3,t);)
    savedCurrent = xs->current;
    xs->current = currentNode;
    rc = xpathEvalSteps( t, context, currentNode, xs->currentXSLTNode,
                         currentPos, &docOrder, &(xs->cbs), rs, errMsg);
    xs->current = savedCurrent;
    if (rc != XPATH_OK) {
        reportError (xs->currentXSLTNode, *errMsg, errMsg);
        xpathRSFree( rs );
    }

    return rc;
}


/*----------------------------------------------------------------------------
|   nodeGreater
|
\---------------------------------------------------------------------------*/
static int nodeGreater (
    int         typeText,
    int         asc,
    int         upperFirst,
    char      * strA,
    char      * strB,
    double      realA,
    double      realB,
    int       * greater
)
{
    int             rc;
#if TclOnly8Bits == 0
    char           *strAptr, *strBptr;
    int             lenA, lenB, len;
    Tcl_UniChar     unicharA, unicharB;
#endif

    *greater = 0;

    if (typeText) {

#if TclOnly8Bits
        /* TODO: this only works for 7 bit ASCII */
        rc = STRCASECMP(strA, strB);
        if (rc == 0) {
            rc = strcmp (strA, strB);
            if (!upperFirst) {
                rc *= -1;
            }
        }
DBG(   fprintf(stderr, "nodeGreater %d <-- strA='%s' strB='%s'\n", rc, strA, strB);)
#else
        lenA = Tcl_NumUtfChars (strA, -1);
        lenB = Tcl_NumUtfChars (strB, -1);
        len = (lenA < lenB ? lenA : lenB);
        rc = Tcl_UtfNcasecmp (strA, strB, len);
        if (rc == 0) {
            if (lenA > lenB) {
                rc = 1;
            } else if (lenA < lenB) {
                rc = -1;
            }
        }
        if (rc == 0) {
            strAptr = strA;
            strBptr = strB;
            while (len-- > 0) {
                strAptr += Tcl_UtfToUniChar(strAptr, &unicharA);
                strBptr += Tcl_UtfToUniChar(strBptr, &unicharB);
                if (unicharA != unicharB) {
                    rc = unicharA - unicharB;
                    break;
                }
            }
            if (!upperFirst) {
                rc *= -1;
            }
        }
#endif
        if (asc) *greater = (rc > 0);
            else *greater = (rc < 0);

    } else {
DBG(   fprintf(stderr, "nodeGreater  realA='%f' realB='%f'\n",realA, realB);)
        if (IS_NAN (realA) || IS_NAN (realB)) {
            if (asc) {
                if (IS_NAN (realA) && !IS_NAN (realB)) {
                    *greater = 0;
                } else {
                    if (IS_NAN (realB) && !IS_NAN (realA)) *greater = 1;
                }
            } else {
                if (IS_NAN (realA) && !IS_NAN(realB)) {
                    *greater = 1;
                } else {
                    if (IS_NAN (realB) && !IS_NAN(realA)) *greater = 0;
                }
            }
        } else {
            if (asc) *greater = (realA > realB);
            else *greater = (realA < realB);
        }
    }
    return 0;
}

static int fastMergeSort (
    int         txt,
    int         asc,
    int         upperFirst,
    domNode   * a[],
    int       * posa,
    domNode   * b[],
    int       * posb,
    char     ** vs,
    double    * vd,
    char     ** vstmp,
    double    * vdtmp,
    int         size,
    char     ** errMsg
) {
    domNode *tmp;
    int tmpPos, lptr, rptr, middle, i, j, gt, rc;
    char    *tmpVs;
    double   tmpVd;

    if (size < 10) {
          /* use simple and fast insertion for small sizes ! */
        for (i = 1; i < size; i++) {
            tmp    = a    [i];
            tmpPos = posa [i];
            tmpVs  = vs   [i];
            tmpVd  = vd   [i];
            j = i;
            if (j>0) {
                rc = nodeGreater(txt, asc, upperFirst, vs[j-1], tmpVs,
                                   vd[j-1], tmpVd, &gt);
                CHECK_RC;
            }
            while ( j > 0 && gt) {
                a   [j] = a   [j-1];
                posa[j] = posa[j-1];
                vs  [j] = vs  [j-1];
                vd  [j] = vd  [j-1];
                j--;
                if (j>0) {
                    rc = nodeGreater(txt, asc, upperFirst, vs[j-1], tmpVs,
                                       vd[j-1], tmpVd, &gt);
                    CHECK_RC;
                }
            }
            a   [j] = tmp;
            posa[j] = tmpPos;
            vs  [j] = tmpVs;
            vd  [j] = tmpVd;
        }
        return 0;
    }
    middle = size/2;

    rc = fastMergeSort(txt, asc, upperFirst, a, posa, b, posb, vs, vd,
                         vstmp, vdtmp, middle, errMsg);
    CHECK_RC;
    rc = fastMergeSort(txt, asc, upperFirst, a+middle, posa+middle, b+middle,
                         posb+middle, vs+middle, vd+middle, vstmp+middle,
                         vdtmp+middle, size-middle, errMsg);
    CHECK_RC;

    lptr = 0;
    rptr = middle;

    for (i = 0; i < size; i++) {
        if (lptr == middle) {
            b    [i] = a   [rptr  ];
            posb [i] = posa[rptr  ];
            vstmp[i] = vs  [rptr  ];
            vdtmp[i] = vd  [rptr++];
        } else if (rptr < size) {
            rc = nodeGreater(txt, asc, upperFirst, vs[lptr], vs[rptr],
                             vd[lptr], vd[rptr], &gt);
            if (gt) {
                b    [i] = a   [rptr  ];
                posb [i] = posa[rptr  ];
                vstmp[i] = vs  [rptr  ];
                vdtmp[i] = vd  [rptr++];
            } else {
                b    [i] = a   [lptr  ];
                posb [i] = posa[lptr  ];
                vstmp[i] = vs  [lptr  ];
                vdtmp[i] = vd  [lptr++];
            }
        } else {
            b    [i] = a   [lptr  ];
            posb [i] = posa[lptr  ];
            vstmp[i] = vs  [lptr  ];
            vdtmp[i] = vd  [lptr++];
        }
    }
    memcpy(a,    b,     size*sizeof(domNode*));
    memcpy(posa, posb,  size*sizeof(int*));
    memcpy(vs,   vstmp, size*sizeof(char*));
    memcpy(vd,   vdtmp, size*sizeof(double));
    return 0;
}

static int sortNodeSetFastMerge(
    int         txt,
    int         asc,
    int         upperFirst,
    domNode   * nodes[],
    int         n,
    char     ** vs,
    double    * vd,
    int       * pos,
    char     ** errMsg
)
{
    domNode **b;
    int      *posb;
    char    **vstmp;
    double   *vdtmp;
    int       rc;

    b = (domNode **)MALLOC(n * sizeof(domNode *));
    posb = (int *)MALLOC(n * sizeof(int));
    vstmp = (char **)MALLOC(sizeof (char *) * n);
    vdtmp = (double *)MALLOC(sizeof (double) * n);

    rc = fastMergeSort(txt, asc, upperFirst, nodes, pos, b, posb, vs, vd,
                         vstmp, vdtmp, n, errMsg);
    FREE((char*)posb);
    FREE((char*)b);
    FREE((char*)vstmp);
    FREE((char*)vdtmp);
    CHECK_RC;
    return 0;
}

/*----------------------------------------------------------------------------
|   xsltSetVar
|
\---------------------------------------------------------------------------*/
static int xsltSetVar (
    xsltState       * xs,
    char            * variableName,
    xpathResultSet  * context,
    domNode         * currentNode,
    int               currentPos,
    char            * select,
    domNode         * actionNode,
    int               active,
    char           ** errMsg
)
{
    xsltVariable   * var;
    int              rc;
    xpathResultSet   rs;
    xsltVarFrame    *tmpFrame = NULL;
    domNode         *fragmentNode, *savedLastNode;
    char             prefix[MAX_PREFIX_LEN];
    const char      *localName;
    domNS           *ns;

    TRACE1("xsltSetVar variableName='%s' \n", variableName);
    if (select!=NULL) {
        TRACE2("xsltSetVar variableName='%s' select='%s'\n", variableName, select);
        rc = evalXPath (xs, context, currentNode, currentPos, select, &rs,
                        errMsg);
        CHECK_RC;
    } else {
        if (!actionNode->firstChild) {
            xpathRSInit (&rs);
            rsSetString (&rs, "");
        } else {
            fragmentNode = domNewElementNode(xs->resultDoc, "",
                                             ELEMENT_NODE);
            savedLastNode = xs->lastNode;
            xs->lastNode = fragmentNode;
            /* process the children as well */
            xsltPushVarFrame (xs);
            rc = ExecActions(xs, context, currentNode, currentPos,
                             actionNode->firstChild, errMsg);
            xsltPopVarFrame (xs);
            CHECK_RC;
            xpathRSInit(&rs);
            rsAddNodeFast(&rs, fragmentNode);
            xs->lastNode = savedLastNode;
        }
    }
    tmpFrame = &xs->varFramesStack[xs->varFramesStackPtr];
    
    xs->varStackPtr++;
    if (xs->varStackPtr >= xs->varStackLen) {
        xs->varStack = (xsltVariable *) REALLOC ((char*)xs->varStack,
                                                 sizeof (xsltVariable)
                                                 * 2 * xs->varStackLen);
        xs->varStackLen *= 2;
    }
    var = &(xs->varStack[xs->varStackPtr]);
    if (tmpFrame->varStartIndex == -1) {
        tmpFrame->varStartIndex = xs->varStackPtr;
    }
    tmpFrame->nrOfVars++;
    domSplitQName (variableName, prefix, &localName);
    if (prefix[0] != '\0') {
        ns = domLookupPrefix (actionNode, prefix);
        if (!ns) {
            reportError (actionNode, "There isn't a namespace bound to"
                         " the prefix.", errMsg);
            return -1;
        }
        var->uri  = ns->uri;
        var->name = localName;
    } else {
        var->uri  = NULL;
        var->name = variableName;
    }
    tmpFrame->polluted = 1;
    var->node   = actionNode;
    var->rs     = rs;
    var->active = active;
    DBG(rsPrint(&(var->rs)));
    return 0;
}

/*----------------------------------------------------------------------------
|   xsltVarExists
|
\---------------------------------------------------------------------------*/
static int xsltVarExists (
    xsltState  * xs,
    char       * variableName,
    domNode    * exprContext
)
{
    int           i, frameIndex, found = 0;
    char          prefix[MAX_PREFIX_LEN];
    const char   *localName, *uri, *varName;
    domNS        *ns;
    xsltVarFrame *frame;

    TRACE1("xsltVarExists variableName='%s' \n", variableName);
    domSplitQName (variableName, prefix, &localName);
    if (prefix[0]) {
        ns = domLookupPrefix (exprContext, prefix);
        if (!ns) {
            /* TODO: this is an error, not only 'not found' */
            return 0;
        }
        uri = ns->uri;
        varName = localName;
    } else {
        uri = NULL;
        varName = variableName;
    }
    frameIndex = xs->varFramesStackPtr;
    while (frameIndex >= 0) {
        frame = &xs->varFramesStack[frameIndex];
        for (i = frame->varStartIndex;
             i < frame->varStartIndex + frame->nrOfVars;
             i++) {
            if ( (uri && !((&xs->varStack[i])->uri))
                 || (!uri && (&xs->varStack[i])->uri)
                 || (uri && (&xs->varStack[i])->uri 
                     && (strcmp (uri, (&xs->varStack[i])->uri)!=0))
                ) continue;
            if (strcmp((&xs->varStack[i])->name, varName)==0) {
                found = 1;
                (&xs->varStack[i])->active = 1;
                break; /* found the variable */
            }
        }
        if (found) return 1;
        if (frame->stop) break;
        frameIndex--;
    }
    
    return 0;
}


/*----------------------------------------------------------------------------
|   xsltGetVar
|
\---------------------------------------------------------------------------*/
static int xsltGetVar (
    void           * clientData,
    char           * variableName,
    char           * varURI,
    xpathResultSet * result,
    char           **errMsg
)
{
    xsltState        *xs = clientData;
    xsltVarFrame     *frame;
    xsltVariable     *var;
    int               rc, i, frameIndex, parFrameSkiped = 0; 
    char             *select;
    Tcl_HashEntry    *h;
    xsltTopLevelVar  *topLevelVar;
    xsltVarInProcess *varInProcess, thisVarInProcess;
    xpathResultSet    nodeList;
    domNode          *savedCurrentXSLTNode;
    Tcl_DString       dErrMsg;

    TRACE1("xsltGetVar variableName='%s' \n", variableName);
    frameIndex = xs->varFramesStackPtr;
    while (frameIndex >= 0) {
        frame = &xs->varFramesStack[frameIndex];
        if (frame->stop == 2 && !parFrameSkiped) {
            parFrameSkiped = 1;
            frameIndex--;
            continue;
        }
        for (i = frame->varStartIndex;
             i < frame->varStartIndex + frame->nrOfVars;
             i++) {
            var = &xs->varStack[i];
            if (!var->active) continue;
            if ( (varURI && !var->uri)
                 || (!varURI && var->uri)
                 || (varURI && var->uri && (strcmp (varURI, var->uri)!=0))
                ) continue;
            if (strcmp(var->name, variableName)==0) {
                TRACE1("xsltGetVar '%s':\n", variableName);
                DBG(rsPrint(&(var->rs)));
                rsCopy(result, &(var->rs) );
                return XPATH_OK;
            }
        }
        if ((frame->stop == 1) && frameIndex > 1) frameIndex = 1;
        frameIndex--;
    }
    
    if (xs->varsInProcess) {
        h = Tcl_FindHashEntry (&xs->topLevelVars, variableName);
        if (h) {
            topLevelVar = (xsltTopLevelVar *) Tcl_GetHashValue (h);
            /* check for circular definitions */
            varInProcess = xs->varsInProcess;
            while (varInProcess) {
                if (strcmp(varInProcess->name, variableName)==0) {
                    reportError (topLevelVar->node, "circular top level"
                                 " variabale definition detected", errMsg);
                    return XPATH_EVAL_ERR;
                }
                varInProcess = varInProcess->next;
            }
            thisVarInProcess.name = variableName;
            thisVarInProcess.next = xs->varsInProcess;
            xs->varsInProcess = &thisVarInProcess;

            xpathRSInit( &nodeList );
            rsAddNodeFast( &nodeList, xs->xmlRootNode);
            savedCurrentXSLTNode = xs->currentXSLTNode;
            xs->currentXSLTNode = topLevelVar->node;
            select = getAttr (topLevelVar->node, "select", a_select);
            rc = xsltSetVar (xs, variableName, &nodeList, xs->xmlRootNode,
                             0, select, topLevelVar->node, 1, errMsg);
            xpathRSFree ( &nodeList );
            CHECK_RC;
            rc = xsltGetVar (xs, variableName, varURI, result, errMsg);
            CHECK_RC;
            /* remove var out of the varsInProcess list. Should be first
               in the list, shouldn't it? */
            varInProcess = xs->varsInProcess;
            if (varInProcess != &thisVarInProcess) {
                domPanic ("error in top level vars processing");
            }
            xs->varsInProcess = varInProcess->next;
            xs->currentXSLTNode = savedCurrentXSLTNode;
            return XPATH_OK;
        }
    }
    Tcl_DStringInit (&dErrMsg);
    Tcl_DStringAppend (&dErrMsg, "Variable \"", -1);
    Tcl_DStringAppend (&dErrMsg, variableName, -1);
    Tcl_DStringAppend (&dErrMsg, "\" has not been declared.", -1);
    reportError (xs->currentXSLTNode, Tcl_DStringValue (&dErrMsg), errMsg);
    Tcl_DStringFree (&dErrMsg);
    return XPATH_EVAL_ERR;
}

/*----------------------------------------------------------------------------
|   addMatch
|
\---------------------------------------------------------------------------*/
static int addMatch (
    xsltState     *xs,
    domNode       *node,
    xsltTemplate  *tpl,
    char          *prioStr,
    ast            a,
    char         **errMsg
    )
{
    xsltTemplate  *t, *prevTpl;
    int            rc, hnew;
    Tcl_DString    dStr;
    Tcl_HashEntry *h;
    
    if (a->type == CombinePath) {
        t = (xsltTemplate *)MALLOC(sizeof(xsltTemplate));
        t->freeAst    = NULL;
        t->name       = NULL;
        t->nameURI    = NULL;
        t->mode       = tpl->mode;
        t->modeURI    = tpl->modeURI;
        t->content    = tpl->content;
        t->precedence = tpl->precedence;
        t->sDoc       = tpl->sDoc;
        t->next       = NULL;
        if (prioStr) {
            t->prio   = tpl->prio;
        }
        rc = addMatch (xs, node, t, prioStr, a->child->child, errMsg);
        CHECK_RC1(t);
        tpl->ast = a->child->next->child;
    } else {
        tpl->ast = a;
    }

    if (!prioStr) {
        tpl->prio = xpathGetPrio(tpl->ast);
        TRACE1("prio = %f for \n", tpl->prio);
        DBG(printAst( 0, tpl->ast);)
        TRACE("\n");
    }   
 
    if ((tpl->ast->type == IsElement && tpl->ast->strvalue[0] != '*')
        || tpl->ast->type == IsFQElement) {
        Tcl_DStringInit (&dStr);
        if (tpl->ast->type == IsFQElement) {
            Tcl_DStringAppend (&dStr, tpl->ast->strvalue, -1);
            Tcl_DStringAppend (&dStr, ":", 1);
        }
        if (tpl->mode) {
            if (tpl->modeURI) {
                Tcl_DStringAppend (&dStr, tpl->modeURI, -1);
                Tcl_DStringAppend (&dStr, ":", 1);
            }
            Tcl_DStringAppend (&dStr, tpl->mode, -1);
            Tcl_DStringAppend (&dStr, ":", 1);
        }
        if (tpl->ast->type == IsFQElement) {
            Tcl_DStringAppend (&dStr, tpl->ast->child->strvalue, -1);
        } else {
            Tcl_DStringAppend (&dStr, tpl->ast->strvalue, -1);
        }
        h = Tcl_CreateHashEntry (&(xs->isElementTpls),
                                 Tcl_DStringValue (&dStr), &hnew);
        Tcl_DStringFree (&dStr);

        if (hnew) {
            tpl->next = NULL;
            Tcl_SetHashValue (h, tpl);
        } else {
            t = (xsltTemplate *) Tcl_GetHashValue (h);
            prevTpl = NULL;
            while (   t 
                      && t->precedence >= tpl->precedence
                      && t->prio > tpl->prio) {
                prevTpl = t;
                t = t->next;
            }
            if (prevTpl) {
                tpl->next = t;
                prevTpl->next = tpl;
            } else {
                tpl->next = Tcl_GetHashValue (h);
                Tcl_SetHashValue (h, tpl);
            }
        }
    } else {
        if (xs->templates == NULL) {
            xs->templates = tpl;
        } else {
            t = xs->templates;
            prevTpl = NULL;
            while (   t
                      && t->precedence >= tpl->precedence
                      && t->prio > tpl->prio) {
                prevTpl = t;
                t = t->next;
            }
            if (prevTpl) {
                tpl->next = t;
                prevTpl->next = tpl;
            } else {
                tpl->next = xs->templates;
                xs->templates = tpl;
            }
        }
    }
    TRACE5("AddTemplate '%s' '%s' '%s' '%s' '%2.2f' \n\n",
            tpl->match, tpl->name, tpl->mode, tpl->modeURI, tpl->prio);
    return 0;
}
    
/*----------------------------------------------------------------------------
|   xsltAddTemplate
|
\---------------------------------------------------------------------------*/
static int xsltAddTemplate (
    xsltState *xs,
    domNode   *node,
    double     precedence,
    char     **errMsg
)
{
    xsltTemplate  *tpl, *t;
    char          *prioStr, *str, prefix[MAX_PREFIX_LEN];
    const char    *localName;
    int            rc, hnew;
    domNS         *ns;
    Tcl_HashEntry *h;
    Tcl_DString    dStr;
    xsltSubDoc    *sDoc;

    tpl = (xsltTemplate *)MALLOC(sizeof(xsltTemplate));

    tpl->match      = getAttr(node,"match", a_match);
    str = getAttr(node, "name", a_name);
    if (!tpl->match && !str) {
        reportError (node, " xsl:template must have a name or" 
                     " match attribute (or both)", errMsg);
        FREE ((char*)tpl);
        return -1;
    }
    tpl->name       = NULL;
    tpl->nameURI    = NULL;
    if (str) {
        if (!domIsQNAME (str)) {
            reportError (node, "The value of the \"name\" attribute must"
                         " be a qname", errMsg);
            FREE ((char*)tpl);
            return -1;
        }
        domSplitQName (str, prefix, &localName);
        if (prefix[0] != '\0') {
            ns = domLookupPrefix (node, prefix);
            if (!ns) {
                reportError (node, "The prefix of the \"name\" attribute"
                             " value isn't bound to a namespace.", errMsg);
                FREE ((char*)tpl);
                return -1;
            }
            tpl->nameURI = ns->uri;
            Tcl_DStringInit (&dStr);
            Tcl_DStringAppend (&dStr, ns->uri, -1);
            Tcl_DStringAppend (&dStr, ":", 1);
            Tcl_DStringAppend (&dStr, localName, -1);
            h = Tcl_CreateHashEntry (&(xs->namedTemplates), 
                                     Tcl_DStringValue (&dStr), &hnew);
            Tcl_DStringFree (&dStr);
        } else {
            h = Tcl_CreateHashEntry (&(xs->namedTemplates), localName, &hnew);
        }
        tpl->name   = localName;
        if (!hnew) {
            t = (xsltTemplate *) Tcl_GetHashValue (h);
            if (t->precedence == precedence) {
                reportError (node, "There is already a template with the"
                             " same name and precedence.", errMsg);
                FREE ((char*)tpl);
                return -1;
            }
            if (!t->match) {
                FREE ((char*)t);
            }
        }
        Tcl_SetHashValue (h, tpl);
        TRACE3("Added named Template '%s' '%2.2f' '%2.2f' \n\n",
            tpl->name, tpl->precedence, tpl->prio);
    }
    tpl->ast        = NULL;
    tpl->mode       = NULL;
    tpl->modeURI    = NULL;
    str = getAttr (node, "mode", a_mode);
    if (str) {
        rc = 0;
        if (!domIsQNAME (str)) {
            reportError (node, "The value of the \"mode\" attribute must"
                         " be a qname.", errMsg);
            rc = -1;
        }
        if (!tpl->match) {
            reportError (node, "A template without a \"match\" attribute must"
                         " not have a \"mode\" attribute.", errMsg);
            rc = -1;
        }
        domSplitQName (str, prefix, &localName);
        if (prefix[0] != '\0') {
            ns = domLookupPrefix (node, prefix);
            if (!ns) {
                reportError (node, "The prefix of the \"mode\" attribute value"
                             " isn't bound to a namespace.", errMsg);
                rc = -1;
            }
            tpl->modeURI = ns->uri;
        }
        tpl->mode = localName;
        if (rc < 0) {
            /* If the template has a name attribute, it is already stored in
               in the namedTemplates hash table and will be freed. */
            if (!tpl->name) {
                FREE ((char*)tpl);
            }
            return -1;
        }
    }
    tpl->prio       = 0.5;
    tpl->content    = node;
    tpl->precedence = precedence;
    tpl->next       = NULL;

    prioStr = getAttr(node,"priority", a_prio);
    if (prioStr) {
        tpl->prio = (double)atof(prioStr);
    } 
    
    sDoc = xs->subDocs;
    while (sDoc) {
        if (sDoc->doc == node->ownerDocument) break;
        sDoc = sDoc->next;
    }
    tpl->sDoc = sDoc;
    
    TRACE1("compiling XPATH '%s' ...\n", tpl->match);
    if (tpl->match) {
        rc = xpathParse(tpl->match, node, XPATH_TEMPMATCH_PATTERN, NULL, NULL,
                        &(tpl->freeAst), errMsg);
        if (rc < 0) {
            reportError (node, *errMsg, errMsg);
        } else {
            rc = addMatch (xs, node, tpl, prioStr, tpl->freeAst, errMsg);
        }
        if (rc < 0) {
            if (tpl->name) {
                /* The template is already stored in the namedTemplates
                   hash table. Therefor we don't free tpl here, but
                   set tpl->match to NULL, which ensures, that the
                   tpl will be freed while the namedTemplates hash table
                   is cleand up. */
                tpl->match = NULL;
            } else {
                free ((char*)tpl);
            }
            return rc;
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
|   ExecUseAttributeSets
|
\---------------------------------------------------------------------------*/
static int ExecUseAttributeSets (
    xsltState         * xs,
    xpathResultSet    * context,
    domNode           * currentNode,
    int                 currentPos,
    domNode           * actionNode,
    char              * styles,
    char             ** errMsg
)
{
    xsltAttrSet *attrSet;
    char        *pc, *aSet, save, *str, prefix[MAX_PREFIX_LEN];
    const char  *localName;
    int          rc;
    domNS       *ns;

    pc = styles;
    while (*pc) {
        while (*pc && IS_XML_WHITESPACE(*pc)) pc++;
        if (*pc == '\0') break;
        aSet = pc;
        while (*pc && !IS_XML_WHITESPACE(*pc)) pc++;
        save = *pc;
        *pc = '\0';
        TRACE1("use-attribute-set '%s' \n", aSet);
        attrSet = xs->attrSets;
        while (attrSet) {
            TRACE2("use-Attr: '%s' == '%s' ? \n", attrSet->name, aSet);
            rc = 0;
            if (!attrSet->uri) {
                if (strcmp(attrSet->name, aSet)==0) rc = 1;
            }
            else {
                domSplitQName (aSet, prefix, &localName);
                if (prefix[0] != '\0') {
                    ns = domLookupPrefix (actionNode, prefix);
                    if (ns) {
                        if (strcmp (ns->uri, attrSet->uri)==0) {
                            if (strcmp (attrSet->name, localName)==0) rc = 1;
                        }
                    }
                }
            }
            if (rc) {
                str = getAttr (attrSet->content, "use-attribute-sets",
                               a_useAttributeSets);
                if (str) {
                    rc = ExecUseAttributeSets (xs, context, currentNode,
                                               currentPos, attrSet->content,
                                               str, errMsg);
                    CHECK_RC;
                }
                rc = ExecActions(xs, context, currentNode, currentPos,
                                 attrSet->content->firstChild, errMsg);
                CHECK_RC;
            }
            attrSet = attrSet->next;
        }
        *pc = save;
    }
    return 0;
}

/*----------------------------------------------------------------------------
|   evalAttrTemplates
|
\---------------------------------------------------------------------------*/
static int evalAttrTemplates (
    xsltState       * xs,
    xpathResultSet  * context,
    domNode         * currentNode,
    int               currentPos,
    char            * str,
    char           ** out,
    char           ** errMsg
)
{
    xpathResultSet  rs;
    char           *tplStart = NULL, *tplResult, *pc, literalChar;
    int             rc, aLen, inTpl = 0, p = 0, inLiteral = 0;

    aLen = 500;
    *out = MALLOC(aLen);
    while (*str) {
        if (inTpl) {
            if (!inLiteral) {
                if (*str == '\'') {
                    inLiteral = 1;
                    literalChar = '\'';
                } else
                if (*str == '"') {
                    inLiteral = 1;
                    literalChar = '"';
                }
            } else {
                if (*str == literalChar) {
                    inLiteral = 0;
                }
            }
            if (*str == '}' && !inLiteral) {

                *str = '\0';
                TRACE1("attrTpl: '%s' \n", tplStart);
                rc = evalXPath (xs, context, currentNode, currentPos,
                                tplStart, &rs, errMsg);
                *str = '}';
                CHECK_RC1(*out);
                tplResult = xpathFuncString( &rs );
                DBG(fprintf(stderr, "attrTpl tplResult='%s' \n", tplResult);)
                xpathRSFree( &rs );
                pc = tplResult;
                while (*pc) {
                   (*out)[p++] = *pc++;
                    if (p>=aLen) { /* enlarge output buffer */
                         *out = REALLOC(*out, 2*aLen);
                         aLen += aLen;
                    }
                }
                inTpl = 0;
                FREE(tplResult);
            }
        } else {
            if (*str == '{') {
                if (*(str+1) == '{') {
                    /*-----------------------------------------------------
                    |    read over escaped '{':
                    |        '{{text text}}' -> '{text text}'
                    \----------------------------------------------------*/
                    str++;
                    (*out)[p++] = *str++;
                    if (p>=aLen) { /* enlarge output buffer */
                        *out = REALLOC(*out, 2*aLen);
                        aLen += aLen;
                    }
                    while (*str && (*str != '}') && (*(str-1) != '}')) {
                        (*out)[p++] = *str++;
                        if (p>=aLen) { /* enlarge output buffer */
                            *out = REALLOC(*out, 2*aLen);
                            aLen += aLen;
                        }
                    }
                    if (!*str) break;
                } else {
                    tplStart = str+1;
                    inTpl = 1;
                    inLiteral = 0;
                }
            } else {
                if (*str == '}' && *(str+1) == '}') {
                    str++;
                }
                (*out)[p++] = *str;
                if (p>=aLen) { /* enlarge output buffer */
                    *out = REALLOC(*out, 2*aLen);
                    aLen += aLen;
                }
            }
        }
        str++;
    }
    (*out)[p] = '\0';
    DBG(fprintf(stderr, "evalAttrTemplates out='%s' \n", (*out) );)
    return 0;
}


/*----------------------------------------------------------------------------
|   setParamVars
|
\---------------------------------------------------------------------------*/
static int setParamVars (
    xsltState       * xs,
    xpathResultSet  * context,
    domNode         * currentNode,
    int               currentPos,
    domNode         * actionNode,
    char           ** errMsg
)
{
    domNode *child;
    char    *str, *select;
    int      rc;

    child = actionNode->firstChild;
    while (child) {
        if (child->nodeType == ELEMENT_NODE) {
            TRACE1("setParamVars child '%s' \n", child->nodeName);
            if (child->info == withParam) {
                str = getAttr(child, "name", a_name);
                if (str) {
                    TRACE1("setting with-param '%s' \n", str);
                    xs->currentXSLTNode = child;
                    select = getAttr(child, "select", a_select);
                    if (select && child->firstChild) {
                        reportError (child, "An xsl:parameter element with a"
                                     " select attribute must be empty",
                                     errMsg);
                        return -1;
                    }
                    TRACE1("with-param select='%s'\n", select);
                    rc = xsltSetVar(xs, str, context, currentNode,
                                    currentPos, select, child, 0, errMsg);
                    CHECK_RC;
                } else {
                    reportError (child, "xsl:with-param: missing mandatory"
                                 " attribute \"name\".", errMsg);
                    return -1;
                }
            }
        }
        child = child->nextSibling;
    }
    return 0;
}


/*----------------------------------------------------------------------------
|   doSortActions
|
\---------------------------------------------------------------------------*/
static int doSortActions (
    xsltState       * xs,
    xpathResultSet  * nodelist,
    domNode         * actionNode,
    xpathResultSet  * context,
    domNode         * currentNode,
    int               currentPos,
    char           ** errMsg
)
{
    domNode       *child;
    char          *str, *evStr, *select, *lang;
    char         **vs = NULL;
    char           prefix[MAX_PREFIX_LEN];
    const char    *localName;
    double        *vd = NULL;
    int            rc = 0, typeText, ascending, upperFirst, *pos = NULL, i, NaN;
    xpathResultSet rs;

    child = actionNode->lastChild; /* do it backwards, so that multiple sort
                                      levels are correctly processed */
    while (child) {
        if (child->nodeType == ELEMENT_NODE) {
            TRACE1("doSortActions child '%s' \n", child->nodeName);
            if (child->info == sort) {
                if (child->firstChild) {
                    reportError (child, "xsl:sort has to be empty.", errMsg);
                    rc = -1;
                    break;
                }
                typeText  = 1;
                ascending = 1;
                upperFirst = 1;
                select = getAttr(child, "select", a_select);
                if (!select) select = ".";
                xs->currentXSLTNode = child;
                str = getAttr(child, "data-type", a_dataType);
                if (str) {
                    rc = evalAttrTemplates (xs, context, currentNode,
                                            currentPos, str, &evStr, errMsg);
                    CHECK_RC;
                    if (strcmp(evStr,"text")==0) typeText = 1;
                    else if (strcmp(evStr,"number")==0) typeText = 0;
                    else {
                        domSplitQName (evStr, prefix, &localName);
                        if (prefix[0] == '\0') {
                            reportError (child, "data-type must be text, "
                                         "number or a prefixed name", errMsg);
                            FREE(evStr);
                            rc = -1;
                            break;
                        }
                        /* OK, so it is a legal value. But we currently
                           don't support non-standard data-types. We use
                           the default, that is typeText = 1. */
                    }
                    FREE(evStr);
                }
                str = getAttr(child, "order", a_order);
                if (str) {
                    rc = evalAttrTemplates (xs, context, currentNode,
                                            currentPos, str, &evStr, errMsg);
                    CHECK_RC;
                    if (strcmp(evStr,"descending")==0) ascending = 0;
                    else if (strcmp(evStr, "ascending")==0) ascending = 1;
                    else {
                        reportError (child, "order must be ascending or"
                                     " descending", errMsg);
                        FREE(evStr);
                        rc = -1;
                        break;
                    }
                    FREE(evStr);
                }
                str = getAttr(child, "case-order", a_caseorder);
                if (str) {
                    rc = evalAttrTemplates (xs, context, currentNode,
                                            currentPos, str, &evStr, errMsg);
                    CHECK_RC;
                    if (strcmp(evStr,"lower-first")==0) upperFirst = 0;
                    else if (strcmp(evStr, "upper-first")==0) upperFirst = 1;
                    else {
                        reportError (child, "case-order must be lower-first"
                                     " or upper-first", errMsg);
                        FREE(evStr);
                        rc = -1;
                        break;
                    }
                    FREE(evStr);
                }
                /* jcl: TODO */
                lang = getAttr(child, "lang", a_lang);

                TRACE4("sorting with '%s' typeText %d ascending %d nodeSetLen=%d\n",
                       select, typeText, ascending, nodelist->nr_nodes);
                CHECK_RC;
                if (!pos)
                    pos = (int*)MALLOC(sizeof(int) * nodelist->nr_nodes);
                for (i=0; i<nodelist->nr_nodes;i++) pos[i] = i;

                xs->currentXSLTNode = child;

                if (!vs) {
                    vs = (char **)MALLOC(sizeof (char *) * nodelist->nr_nodes);
                    for (i=0; i<nodelist->nr_nodes;i++) vs[i] = NULL;
                    vd = (double *)MALLOC(sizeof (double) * nodelist->nr_nodes);
                    for (i=0; i<nodelist->nr_nodes;i++) vd[i] = 0.0;
                }
                for (i = 0; i < nodelist->nr_nodes; i++) {
                    xpathRSInit (&rs);
                    rc = evalXPath (xs, nodelist, nodelist->nodes[i], i,
                                    select, &rs, errMsg);
                    if (rc < 0)
                        goto doSortActionCleanUp;

                    if (typeText) {
                        vs[i] = xpathFuncString (&rs);
                    } else {
                        vd[i] = xpathFuncNumber (&rs, &NaN);
                    }
                    xpathRSFree (&rs);
                }
                rc = sortNodeSetFastMerge (typeText, ascending, upperFirst,
                                           nodelist->nodes, nodelist->nr_nodes,
                                           vs, vd, pos, errMsg);
                if (typeText) {
                    for (i = 0; i < nodelist->nr_nodes; i++) {
                        FREE(vs[i]);
                    }
                }
                if (rc < 0)
                    goto doSortActionCleanUp;
            }
        }
        child = child->previousSibling;
    }
 doSortActionCleanUp:
    if (pos) FREE((char*)pos);
    if (vs) FREE((char*)vs);
    if (vd) FREE((char*)vd);
    return rc;
}


/*----------------------------------------------------------------------------
|   xsltNumber
|
\---------------------------------------------------------------------------*/
static int xsltNumber (
    xsltState       * xs,
    xpathResultSet  * context,
    domNode         * currentNode,
    int               currentPos,
    domNode         * actionNode,
    char           ** errMsg
)
{
    xpathResultSet    rs;
    int               rc, vs[20], NaN, hnew, i, useFormatToken, vVals = 0;
    int              *v, *vd = NULL;
    long              groupingSize = 0;
    char             *value, *level, *count, *from, *str, *str1, *format;
    char             *groupingSeparator = NULL, *groupingSizeStr = NULL;
    char             *tail;
    ast               t_count, t_from;
    domNode          *node, *start;
    Tcl_HashEntry    *h;
    xsltNumberFormat *f;
    Tcl_DString       dStr;
    domProcessingInstructionNode *pi;

    v = vs;
    value = getAttr(actionNode, "value",  a_value);
    str   = getAttr(actionNode, "format", a_format); if (!str) str = "1";
    xs->currentXSLTNode = actionNode;
    rc = evalAttrTemplates( xs, context, currentNode, currentPos,
                            str, &format, errMsg);
    CHECK_RC;
    f = xsltNumberFormatTokenizer (xs, format, errMsg);
    if (!f) {
        FREE(format);
        return -1;
    }
    str = getAttr(actionNode, "grouping-separator", a_groupingSeparator);
    if (str) {
        str1 = getAttr (actionNode, "grouping-size", a_groupingSize);
        if (str1) {
            rc = evalAttrTemplates (xs, context, currentNode, currentPos, str,
                                    &groupingSeparator, errMsg);
            if (rc < 0) goto xsltNumberError;
            rc = evalAttrTemplates (xs, context, currentNode, currentPos, str1,
                                    &groupingSizeStr, errMsg);
            if (rc < 0) goto xsltNumberError;
            groupingSize = strtol (groupingSizeStr, &tail, 10);
            if (groupingSize <= 0) {
                /* This covers both cases: non integer value after evaluation
                   and wrong (<= 0) integer value. */
                reportError (actionNode, "The value of \"grouping-size\" must"
                             " evaluate to a positiv integer.", errMsg);
                goto xsltNumberError;
            }
        }
    }
    
    if (value) {
        TRACE2("xsltNumber value='%s' format='%s' \n", value, format);
        rc = evalXPath(xs, context, currentNode, currentPos,
                       value, &rs, errMsg);
        if (rc < 0) goto xsltNumberError;
        vVals = 1;
        v[0] = xpathRound(xpathFuncNumber( &rs, &NaN ));
        /* MARK recoverable error */
        /* This is one of the not so satisfying corners of the xslt
         * rec. The rec doesn't say, what to do, if the value isn't a
         * (finit) number. E24 from the erratas doesn't makes things
         * much better - a little bit dubious wording and a not very
         * convincing decision. Well, at least saxon seems to follow
         * the words of E24. I'll postpone this topic. */
        if (NaN) v[0] = 0;
        xpathRSFree( &rs );
    } else {
        level = getAttr(actionNode, "level",  a_level);
        if (!level) level = "single";
        count = getAttr(actionNode, "count",  a_count);
        from  = getAttr(actionNode, "from",   a_from);
        TRACE3("xsltNumber  format='%s' count='%s' from='%s' \n", format, count, from);
        if (count) {
            h = Tcl_CreateHashEntry (&(xs->pattern), count, &hnew);
            if (!hnew) {
                t_count = (ast) Tcl_GetHashValue (h);
            } else {
                rc = xpathParse (count, actionNode, XPATH_FORMAT_PATTERN, NULL,
                                 NULL, &t_count, errMsg);
                if (rc < 0) goto xsltNumberError;
                Tcl_SetHashValue (h, t_count);
            }
        } else {
            Tcl_DStringInit (&dStr);
            if (currentNode->nodeType == ELEMENT_NODE) {
                /* TODO: This is wrong. Instead this should use the
                   "expanded-name" of the current node. */
                Tcl_DStringAppend (&dStr, currentNode->nodeName, -1);
            } else 
            if (currentNode->nodeType == ATTRIBUTE_NODE) {
                Tcl_DStringAppend (&dStr, "@", 1);
                Tcl_DStringAppend (&dStr, currentNode->nodeName, -1);
            } else 
            if (currentNode->nodeType == COMMENT_NODE) {
                Tcl_DStringAppend (&dStr, "comment()", -1);
            } else
            if (currentNode->nodeType == TEXT_NODE) {
                Tcl_DStringAppend (&dStr, "text()", -1);
            } else 
            if (currentNode->nodeType == PROCESSING_INSTRUCTION_NODE) {
                Tcl_DStringAppend (&dStr, "processing-instruction('", -1);
                pi = (domProcessingInstructionNode *)currentNode;
                Tcl_DStringAppend (&dStr, pi->targetValue, pi->targetLength);
                Tcl_DStringAppend (&dStr, "')", 2);
            } else {
                reportError (actionNode, "unknown node type!!!", errMsg);
                return -1;
            }
            h = Tcl_CreateHashEntry (&(xs->pattern), Tcl_DStringValue(&dStr),
                                     &hnew);
            if (!hnew) {
                t_count = (ast) Tcl_GetHashValue (h);
            } else {
                rc = xpathParse (Tcl_DStringValue (&dStr), actionNode, 
                                 XPATH_FORMAT_PATTERN, NULL, NULL, &t_count,
                                 errMsg);
                if (rc < 0) {
                    Tcl_DStringFree (&dStr);
                    goto xsltNumberError;
                }
                Tcl_SetHashValue (h, t_count);
            }
            Tcl_DStringFree (&dStr);
        }
        if (from) {
            h = Tcl_CreateHashEntry (&(xs->pattern), from, &hnew);
            if (!hnew) {
                t_from = (ast) Tcl_GetHashValue (h);
            } else {
                rc = xpathParse (from, actionNode, XPATH_FORMAT_PATTERN, NULL,
                                 NULL, &t_from, errMsg);
                if (rc < 0) goto xsltNumberError;
                Tcl_SetHashValue (h, t_from);
            }
        }

        if (strcmp (level, "single")==0) {
            node = currentNode;
            start = NULL;
            if (from) {
                while (node) {
                    rc = xpathMatches (t_from, actionNode, node, &(xs->cbs),
                                       errMsg);
                    if (rc < 0) goto xsltNumberError;
                    if (rc) break;
                    if (node->nodeType == ATTRIBUTE_NODE) 
                        node = ((domAttrNode *)node)->parentNode;
                    else node = node->parentNode;
                }
            }
            node = currentNode;
            while (node != start) {
                rc = xpathMatches (t_count, actionNode, node, &(xs->cbs),
                                   errMsg);
                if (rc < 0) goto xsltNumberError;
                if (rc) break;
                if (node->nodeType == ATTRIBUTE_NODE) 
                    node = ((domAttrNode *)node)->parentNode;
                else node = node->parentNode;
            }
            if (node == start) {
                domAppendNewTextNode (xs->lastNode, "", 0, TEXT_NODE, 0);
                FREE(format);
                return 0;
            } else {
                vVals = 1;
                v[0] = 1;
                node = domPreviousSibling (node);
                while (node) {
                    rc = xpathMatches (t_count, actionNode, node, &(xs->cbs),
                                       errMsg);
                    if (rc < 0) goto xsltNumberError;
                    if (rc) v[0]++;
                    node = domPreviousSibling (node);
                }
            }
        } else
        if (strcmp (level, "multiple")==0) {
            xpathRSInit (&rs);
            node = currentNode;
            while (node) {
                if (from) {
                    rc = xpathMatches (t_from, actionNode, node, &(xs->cbs),
                                       errMsg);
                    if (rc < 0) goto xsltNumberError;
                    if (rc) break;
                }
                rc = xpathMatches (t_count, actionNode, node, &(xs->cbs), 
                                   errMsg);
                if (rc < 0) goto xsltNumberError;
                if (rc) rsAddNode (&rs, node);
                if (node->nodeType == ATTRIBUTE_NODE)
                    node = ((domAttrNode *)node)->parentNode;
                else node = node->parentNode;
            }
            if (rs.nr_nodes > 20) {
                vd = (int *)MALLOC(sizeof (int) * rs.nr_nodes);
                v = vd;
            }
            vVals = rs.nr_nodes;
            v[0] = 0;
            for (i = 0;  i < rs.nr_nodes; i++) {
                node = domPreviousSibling (rs.nodes[i]);
                v[i] = 1;
                while (node) {
                    rc = xpathMatches (t_count, actionNode, node, &(xs->cbs),
                                       errMsg);
                    if (rc < 0) goto xsltNumberError;
                    if (rc) v[i]++;
                    node = domPreviousSibling (node);
                }
            }
            xpathRSFree (&rs);
        } else
        if (strcmp (level, "any")==0) {
            v[0] = 0;
            vVals = 1;
            node = currentNode;
            while (node) {
                if (from) {
                    rc = xpathMatches (t_from, actionNode, node, &(xs->cbs),
                                       errMsg);
                    if (rc < 0) goto xsltNumberError;
                    if (rc) break;
                }
                rc = xpathMatches (t_count, actionNode, node, &(xs->cbs),
                                   errMsg);
                if (rc < 0) goto xsltNumberError;
                if (rc) v[0]++;
                if (domPreviousSibling (node)) {
                    node = domPreviousSibling (node);
                    while ((node->nodeType == ELEMENT_NODE)
                           && node->lastChild) {
                        node = node->lastChild;
                    }
                    continue;
                }
                if (node->nodeType == ATTRIBUTE_NODE) {
                    node = ((domAttrNode *)node)->parentNode;
                } else {
                    node = node->parentNode;
                }
            }
        } else {
            reportError (actionNode, "xsl:number: Wrong \"level\" attribute"
                         " value!", errMsg);
            goto xsltNumberError;
        }
    }

    Tcl_DStringInit (&dStr);
    useFormatToken = 0;
    if (f->prologLen) {
        Tcl_DStringAppend (&dStr, f->formatStr, f->prologLen);
    }
    for (i = 0; i < vVals -1; i++) {
        formatValue (f, &useFormatToken, v[i], &dStr,
                     groupingSeparator, groupingSize, 1);
    }
    if (vVals > 0) {
        formatValue (f, &useFormatToken, v[vVals-1], &dStr,
                     groupingSeparator, groupingSize, 0);
        if (f->epilogLen) {
            Tcl_DStringAppend (&dStr, f->epilogStart, f->epilogLen);
        }
        domAppendNewTextNode(xs->lastNode, Tcl_DStringValue (&dStr),
                             Tcl_DStringLength (&dStr), TEXT_NODE, 0);
    }
    FREE(format);
    if (groupingSeparator) FREE (groupingSeparator);
    if (groupingSizeStr) FREE (groupingSizeStr);
    if (vd) {
        FREE((char *)vd);
    }
    Tcl_DStringFree (&dStr);
    return 0;

 xsltNumberError:
    if (format) FREE (format);
    if (groupingSeparator) FREE (groupingSeparator);
    if (groupingSizeStr) FREE (groupingSizeStr);
    return -1;
}

/*----------------------------------------------------------------------------
|   ExecAction
|
\---------------------------------------------------------------------------*/
static int ExecAction (
    xsltState       * xs,
    xpathResultSet  * context,
    domNode         * currentNode,
    int               currentPos,
    domNode         * actionNode,
    char           ** errMsg
)
{
    domNode        *child, *n, *n1, *savedLastNode, *fragmentNode;
    xsltTemplate   *tpl, *currentTplRule, *tplChoosen;
    domAttrNode    *attr, *attr1;
    domTextNode    *tnode;
    domNS          *ns, *ns1;
    xsltSubDoc     *sDoc, *currentSubDoc;
    xsltExclExtNS  *eNS;
    xsltNSAlias    *nsAlias;
    Tcl_DString     dStr;
    domProcessingInstructionNode *pi;
    xpathResultSet  rs, nodeList;
    char           *str, *str2, *select, *pc, *nsAT, *out;
    const char     *mode, *modeURI, *localName, *uri, *nsStr;
    char            prefix[MAX_PREFIX_LEN];
    int             rc, b, i, len, terminate, chooseState, disableEsc = 0;
    double          currentPrio, currentPrec;
    Tcl_HashEntry  *h;

    if (actionNode->nodeType == TEXT_NODE) {
        domAppendNewTextNode(xs->lastNode,
                             ((domTextNode*)actionNode)->nodeValue,
                             ((domTextNode*)actionNode)->valueLength,
                             TEXT_NODE, 0);
        return 0;
    }
    if (actionNode->nodeType != ELEMENT_NODE) return 0;

    TRACE1("\nExecAction '%s' \n", actionNode->nodeName);
    DBG (printXML (currentNode, 3, 5);)
    xs->currentXSLTNode = actionNode;
    switch ( actionNode->info ) {

        case applyImports:
            if (actionNode->firstChild) {
                reportError(actionNode, "xsl:apply-imports has to be empty!", 
                            errMsg);
                return -1;
            }
            if (!xs->currentTplRule) {
                reportError(actionNode, "xsl:apply-imports not allowed here!",
                            errMsg);
                return -1;
            }
            tplChoosen = NULL;
            currentPrio = -100000.0;
            currentPrec = 0.0;
            mode = xs->currentTplRule->mode;
            modeURI = xs->currentTplRule->modeURI;
            
            if (currentNode->nodeType == ELEMENT_NODE) {
                Tcl_DStringInit (&dStr);
                if (currentNode->namespace) {
                    domSplitQName (currentNode->nodeName, prefix, &localName);
                    Tcl_DStringAppend (&dStr, domNamespaceURI (currentNode),
                                       -1);
                    Tcl_DStringAppend (&dStr, ":", 1);
                }
                if (mode) {
                    if (modeURI) {
                        Tcl_DStringAppend (&dStr, modeURI, -1);
                        Tcl_DStringAppend (&dStr, ":", 1);
                    }
                    Tcl_DStringAppend (&dStr, mode, -1);
                    Tcl_DStringAppend (&dStr, ":", 1);
                }
                if (currentNode->namespace) {
                    Tcl_DStringAppend (&dStr, localName, -1);
                } else {
                    Tcl_DStringAppend (&dStr, currentNode->nodeName, -1);
                }
                h = Tcl_FindHashEntry (&xs->isElementTpls, 
                                       Tcl_DStringValue (&dStr));
                Tcl_DStringFree (&dStr);

                if (h) {
                    for (tpl = (xsltTemplate *) Tcl_GetHashValue (h);
                         tpl != NULL;
                         tpl = tpl->next) {
                        if (tpl->precedence > xs->currentTplRule->precedence
                            || tpl == xs->currentTplRule) continue;
                        TRACE3("find element tpl match='%s' mode='%s' name='%s'\n",
                               tpl->match, tpl->mode, tpl->name);
                        TRACE4("tpl has prio='%f' precedence='%f'\n", tpl->prio, tpl->precedence, currentPrio, currentPrec);
                        rc = xpathMatches ( tpl->ast, actionNode, currentNode,
                                            &(xs->cbs), errMsg);
                        if (rc < 0) {
                            TRACE1("xpathMatches had errors '%s' \n", *errMsg);
                            return rc;
                        }
                        if (rc == 0) continue;
                        TRACE3("matching '%s': %f > %f ? \n", tpl->match, tpl->prio , currentPrio);
                        tplChoosen = tpl;
                        currentPrio = tpl->prio;
                        currentPrec = tpl->precedence;
                        break;
                    }
                }
            }

            TRACE2("apply-imports: current template precedence='%f' mode='%s'\n", xs->currentTplRule->precedence, xs->currentTplRule->mode);
            for (tpl = xs->templates; tpl != NULL; tpl = tpl->next) {
                TRACE4("find tpl match='%s' mode='%s' modeURI='%s' name='%s'\n",
                       tpl->match, tpl->mode, tpl->modeURI, tpl->name);
                /* exclude those, which don't match the current mode
                   and the currentTplRule */
                if (   ( mode && !tpl->mode)
                       || (!mode &&  tpl->mode)
                       || ( mode &&  tpl->mode && (strcmp(mode,tpl->mode)!=0))
                       || (!modeURI && tpl->modeURI)
                       || ( modeURI && !tpl->modeURI)
                       || ( modeURI && tpl->modeURI && (strcmp(modeURI, tpl->modeURI)!=0))
                       || (tpl == xs->currentTplRule)
                    ) {
                    TRACE("doesn't match mode\n");
                    continue;
                }
                TRACE4("tpl has prio='%f' precedence='%f', currentPrio='%f', currentPrec='%f'\n", tpl->prio, tpl->precedence, currentPrio, currentPrec);
                if (tpl->match && tpl->precedence < xs->currentTplRule->precedence
                    && tpl->precedence >= currentPrec) {
                    if (tpl->precedence > currentPrec
                        || tpl->prio >= currentPrio) {
                        rc = xpathMatches (tpl->ast, actionNode, currentNode,
                                           &(xs->cbs), errMsg);
                        CHECK_RC;
                        if (rc == 0) continue;
                        TRACE3("matching '%s': %f > %f ? \n", tpl->match, tpl->prio , currentPrio);
                        tplChoosen = tpl;
                        currentPrec = tpl->precedence;
                        currentPrio = tpl->prio;
                        TRACE1("TAKING '%s' \n", tpl->match);
                    }
                }
            }

            if (tplChoosen == NULL) {

                TRACE("nothing matches -> execute built-in template \n");

                /*--------------------------------------------------------------------
                |   execute built-in template
                \-------------------------------------------------------------------*/
                if (currentNode->nodeType == TEXT_NODE) {
                    domAppendNewTextNode(xs->lastNode,
                                         ((domTextNode*)currentNode)->nodeValue,
                                         ((domTextNode*)currentNode)->valueLength,
                                         TEXT_NODE, 0);
                    break;
                } else
                if (currentNode->nodeType == ELEMENT_NODE) {
                    child = currentNode->firstChild;
                } else
                if (currentNode->nodeType == ATTRIBUTE_NODE) {
                    domAppendNewTextNode (xs->lastNode,
                                          ((domAttrNode*)currentNode)->nodeValue,
                                          ((domAttrNode*)currentNode)->valueLength,
                                          TEXT_NODE, 0);
                    break;
                } else {
                    /* for all other node we don't have to recurse deeper */
                    break;
                }
                xpathRSInit( &rs );
                while (child) {
                    rsAddNodeFast ( &rs, child);
                    child = child->nextSibling;
                }
                rc = ApplyTemplates (xs, context, currentNode, currentPos,
                                actionNode, &rs, mode, modeURI, errMsg);
                xpathRSFree( &rs );
                CHECK_RC;

                break;
            }

            xsltPushVarFrame (xs);
            SETSCOPESTART;
            currentTplRule = xs->currentTplRule;
            currentSubDoc = xs->currentSubDoc;
            xs->currentTplRule = tplChoosen;
            xs->currentSubDoc = tplChoosen->sDoc;
            rc = ExecActions(xs, context, currentNode, currentPos,
                             tplChoosen->content->firstChild, errMsg);
            xsltPopVarFrame (xs);
            CHECK_RC;
            xs->currentTplRule = currentTplRule;
            xs->currentSubDoc = currentSubDoc;
            break;

        case applyTemplates:
            mode    = NULL;
            modeURI = NULL;
            str     = getAttr (actionNode, "mode", a_mode);
            if (str) {
                domSplitQName (str, prefix, &localName);
                if (prefix[0] != '\0') {
                    ns = domLookupPrefix (actionNode, prefix);
                    if (!ns) {
                        reportError (actionNode, "The prefix of the \"name\""
                                     " attribute value isn't bound to a"
                                     " namespace.", errMsg);
                        return -1;
                    }
                    modeURI = ns->uri;
                }
                mode = localName;
            }
            select  = getAttr(actionNode, "select", a_select);
            if (!select) {
                xpathRSInit (&rs);
                if (currentNode->nodeType == ELEMENT_NODE) {
                    child = currentNode->firstChild;
                    while (child) {
                        rsAddNodeFast (&rs, child);
                        child = child->nextSibling;
                    }
                }
            } else {
                xpathRSInit( &nodeList );
                rsAddNodeFast( &nodeList, currentNode );
                DBG(rsPrint( &nodeList ));
                TRACE2("applyTemplates: select = '%s' mode='%s'\n", select, mode);
                rc = evalXPath(xs, &nodeList, currentNode, 1, select, &rs, errMsg);
                xpathRSFree( &nodeList );
                CHECK_RC;
                TRACE1("applyTemplates: evalXPath for select = '%s' gave back:\n", select);
                DBG(rsPrint(&rs));
            }

            if (rs.type == EmptyResult) break;
            else if (rs.type != xNodeSetResult) {
                reportError (actionNode, "The \"select\" expression of"
                             " xsl:apply-templates elements must evaluate to"
                             " a node set.", errMsg);
                xpathRSFree (&rs);
                return -1;
            }

            rc = doSortActions (xs, &rs, actionNode, context, currentNode,
                                currentPos, errMsg);
            if (rc < 0) {
                xpathRSFree (&rs);
                return rc;
            }
            /* should not be necessary, because every node set is
               returned already in doc Order */
            /*  if (!sorted) sortByDocOrder(&rs); */

            TRACE1("                evalXPath for select = '%s': (SORTED)\n", select);
            DBG(rsPrint(&rs));

            rc = ApplyTemplates(xs, context, currentNode, currentPos,
                                actionNode, &rs, mode, modeURI, errMsg);
            xpathRSFree( &rs );
            CHECK_RC;
            break;

        case attribute:
            if (xs->lastNode->firstChild) {
                /* Adding an Attribute to an element after
                   children have been added to it is an error.
                   Ignore the attribute. */
                break;
            }
            nsAT = getAttr(actionNode, "namespace", a_namespace);
            str = getAttr(actionNode, "name", a_name);
            if (!str) {
                reportError (actionNode, "xsl:attribute: missing mandatory"
                             " attribute \"name\".", errMsg);
                return -1;
            }

            rc = evalAttrTemplates( xs, context, currentNode, currentPos,
                                    str, &str2, errMsg);
            CHECK_RC;
            domSplitQName (str2, prefix, &localName);
            if ((prefix[0] != '\0' &&  !domIsNCNAME (prefix))
                 || !domIsNCNAME (localName)) {
                reportError (actionNode, "xsl:attribute: Attribute name is not"
                             " a valid QName.", errMsg);
                FREE(str2);
                return -1;
            }
            out = NULL;
            if (nsAT) {
                rc = evalAttrTemplates( xs, context, currentNode, currentPos,
                                        nsAT, &out, errMsg);
                CHECK_RC1(str2);
            }

            Tcl_DStringInit (&dStr);
            if (prefix[0] == '\0' && (strcmp(str2, "xmlns")==0)) {
                goto ignoreAttribute;
            }
            /* It isn't allowed to create namespace attributes with
               xsl:attribute, a "xmlns" prefix must be rewritten; see
               XSLT rec 7.1.3 */
            if (prefix[0] && (strcmp(prefix, "xmlns")==0)) {
                sprintf (prefix, "ns%d", xs->nsUniqeNr++);
                Tcl_DStringAppend (&dStr, prefix, -1);
                Tcl_DStringAppend (&dStr, ":", 1);
                Tcl_DStringAppend (&dStr, localName, -1);
            } else {
                if (out) {
                    if (out[0] == '\0') {
                        if (prefix[0] != '\0') {
                            Tcl_DStringAppend (&dStr, localName, -1);
                        } else {
                            Tcl_DStringAppend (&dStr, str2, -1);
                        }
                        FREE(out);
                        out = NULL;
                    } else {
                        if (prefix[0] == '\0') {
                            ns = domLookupURI (xs->lastNode, out);
                            if (ns && (ns->prefix[0] != '\0')) {
                                Tcl_DStringAppend (&dStr, ns->prefix, -1);
                                Tcl_DStringAppend (&dStr, ":", 1);
                            } else {
                                sprintf (prefix, "ns%d", xs->nsUniqeNr++);
                                Tcl_DStringAppend (&dStr, prefix, -1);
                                Tcl_DStringAppend (&dStr, ":", 1);
                            }
                        }
                        Tcl_DStringAppend (&dStr, str2, -1);
                    }
                } else {
                    if (prefix[0] != '\0') {
                        ns = domLookupPrefix (actionNode, prefix);
                        if (ns) out = tdomstrdup (ns->uri);
                        else goto ignoreAttribute;
                    }
                    Tcl_DStringAppend (&dStr, str2, -1);
                }
            }

            savedLastNode = xs->lastNode;
            xs->lastNode  = domNewElementNode (xs->resultDoc,
                                               "container", ELEMENT_NODE);
            xsltPushVarFrame (xs);
            rc = ExecActions(xs, context, currentNode, currentPos,
                             actionNode->firstChild, errMsg);
            xsltPopVarFrame (xs);
            if (rc < 0) {
                if (out) FREE(out);
                FREE(str2);
                return rc;
            }
            pc = xpathGetStringValue (xs->lastNode, &len);
            DBG(fprintf (stderr, "xsl:attribute: create attribute \"%s\" with value \"%s\" in namespace \"%s\"\n", Tcl_DStringValue (&dStr), pc, out);)
            domSetAttributeNS (savedLastNode, Tcl_DStringValue (&dStr), pc,
                               out, 1);
            FREE(pc);
            Tcl_DStringFree (&dStr);
            domDeleteNode (xs->lastNode, NULL, NULL);
            xs->lastNode = savedLastNode;
    ignoreAttribute:
            if (out) FREE(out);
            FREE(str2);
            break;

        case attributeSet: 
            reportError (actionNode, "xsl:attribute-set is only allowed at"
                         " top-level.", errMsg);
            return -1;

        case callTemplate:
            tplChoosen = NULL;
            currentPrec = INT_MIN;
            str = getAttr(actionNode, "name", a_name);
            if (!str) {
                reportError (actionNode, "xsl:call-template must have a"
                             " \"name\" attribute", errMsg);
                return -1;
            }
            domSplitQName (str, prefix, &localName);
            uri = NULL;
            if (prefix[0] != '\0') {
                ns = domLookupPrefix (actionNode, prefix);
                if (!ns) {
                    reportError (actionNode, "The prefix of the \"name\""
                                 " attribute value isn't bound to a"
                                 " namespace.", errMsg);
                    return -1;
                }
                uri = ns->uri;
            }
            if (uri) {
                Tcl_DStringInit (&dStr);
                Tcl_DStringAppend (&dStr, uri, -1);
                Tcl_DStringAppend (&dStr, ":", 1);
                Tcl_DStringAppend (&dStr, localName, -1);
                h = Tcl_FindHashEntry (&(xs->namedTemplates),
                                       Tcl_DStringValue (&dStr));
                Tcl_DStringFree (&dStr);
            } else {
                h = Tcl_FindHashEntry (&(xs->namedTemplates), localName);
            }
            if (!h) {
                reportError (actionNode, "xsl:call-template calls a non"
                             " existend template!", errMsg);
                return -1;
            } 
            tplChoosen = (xsltTemplate *) Tcl_GetHashValue (h);
            xsltPushVarFrame (xs);
            SETPARAMDEF;
            TRACE3("call template %s match='%s' name='%s' \n", localName, 
                   tplChoosen->match, tplChoosen->name);
            DBG(printXML(xs->lastNode, 0, 2);)
            rc = setParamVars (xs, context, currentNode, currentPos,
                               actionNode, errMsg);
            if (rc < 0) {
                xsltPopVarFrame (xs);
                return rc;
            }
            currentSubDoc = xs->currentSubDoc;
            xs->currentSubDoc = tplChoosen->sDoc;
            SETSCOPESTART;
            rc = ExecActions(xs, context, currentNode, currentPos, 
                             tplChoosen->content->firstChild, errMsg);
            TRACE2("called template '%s': ApplyTemplate/ExecActions rc = %d \n", localName, rc);
            xsltPopVarFrame (xs);
            CHECK_RC;
            xs->currentSubDoc = currentSubDoc;
            DBG(printXML(xs->lastNode, 0, 2);)
            break;

       case choose:
            chooseState = 0;
            for( child = actionNode->firstChild;  child != NULL;
                 child = child->nextSibling)
            {
                if (child->nodeType != ELEMENT_NODE) continue;
                switch (child->info) {
                    case when:
                        if (chooseState > 1) {
                            reportError (actionNode, "\"otherwise\" clause"
                                         " must be after all \"when\""
                                         " clauses", errMsg);
                            return -1;
                        } else {
                            chooseState = 1;
                        }
                        str = getAttr(child, "test", a_test);
                        if (str) {
                            TRACE1("checking when test '%s' \n", str);
                            rc = evalXPath(xs, context, currentNode, 
                                           currentPos, str, &rs, errMsg);
                            CHECK_RC;
                            b = xpathFuncBoolean( &rs );
                            xpathRSFree( &rs );
                            if (b) {
                                TRACE("test is true!\n");
                                /* process the children as well */
                                xsltPushVarFrame (xs);
                                rc = ExecActions(xs, context,
                                                 currentNode, currentPos,
                                                 child->firstChild, errMsg);
                                xsltPopVarFrame (xs);
                                CHECK_RC;
                                return 0;
                            }
                        } else {
                            reportError (child, "xsl:when: missing mandatory"
                                         " attribute \"test\".", errMsg);
                            return -1;
                        }
                        break;

                    case otherwise:
                        if (chooseState != 1) {
                            if (chooseState == 0) {
                                reportError (actionNode, "\"choose\" must"
                                             " have at least one \"when\""
                                             " clause", errMsg);
                            } else {
                                reportError (child, "only one \"otherwise\""
                                             " clause allowed inside a"
                                             " \"choose\"", errMsg);
                            }
                            return -1;
                        } else {
                            chooseState = 2;
                        }
                        /* process the children as well */
                        xsltPushVarFrame (xs);
                        rc = ExecActions(xs, context, currentNode, currentPos,
                                         child->firstChild, errMsg);
                        xsltPopVarFrame (xs);
                        CHECK_RC;
                        break;

                    default:
                        reportError (actionNode, "only otherwise or when"
                                     " allowed in choose!",
                                     errMsg);
                        return -1;
                }
            }
            if (chooseState == 0) {
                reportError (actionNode, "\"choose\" must have at least"
                             " one \"when\" clause", errMsg);
                return -1;
            }
            break;

        case comment:
            fragmentNode = domNewElementNode(xs->resultDoc, "", ELEMENT_NODE);
            savedLastNode = xs->lastNode;
            xs->lastNode = fragmentNode;
            xsltPushVarFrame (xs);
            rc = ExecActions(xs, context, currentNode, currentPos,
                             actionNode->firstChild, errMsg);
            xsltPopVarFrame (xs);
            CHECK_RC;
            child = fragmentNode->firstChild;
            while (child) {
                if (child->nodeType != TEXT_NODE) {
                    domDeleteNode (fragmentNode, NULL, NULL);
                    reportError (actionNode, "xsl:comment must not create"
                                 " nodes other than text nodes.", errMsg);
                    return -1;
                }
                child = child->nextSibling;
            }
            str = xpathGetStringValue (fragmentNode, &len);
            if (!domIsComment (str)) {
                reportError (actionNode, "Invalide comment value", errMsg);
                domDeleteNode (fragmentNode, NULL, NULL);
                FREE(str);
                return -1;
            }
            xs->lastNode = savedLastNode;
            domAppendNewTextNode(xs->lastNode, str, len, COMMENT_NODE, 0);
            domDeleteNode (fragmentNode, NULL, NULL);
            FREE(str);
            break;

        case copy:
            DBG(if (currentNode->nodeType == ATTRIBUTE_NODE) {
                    fprintf(stderr, "copy '%s' \n", ((domAttrNode*)currentNode)->nodeName);
                } else {
                    fprintf(stderr, "copy '%s' \n", currentNode->nodeName);
                })
            if (currentNode->nodeType == TEXT_NODE) {
                DBG(fprintf(stderr, "node is TEXT_NODE \n");)
                tnode = (domTextNode*)currentNode;
                n = (domNode*)
                    domAppendNewTextNode(xs->lastNode,
                                         tnode->nodeValue,
                                         tnode->valueLength,
                                         TEXT_NODE, 0);
            } else
            if (currentNode->nodeType == ELEMENT_NODE) {
                DBG(fprintf(stderr, "node is ELEMENT_NODE \n");)
                savedLastNode = xs->lastNode;
                if (currentNode != currentNode->ownerDocument->rootNode) {
                    n = domAppendNewElementNode(xs->lastNode,
                                                currentNode->nodeName,
                                                domNamespaceURI(currentNode) );
                    xs->lastNode = n;
                    str = getAttr(actionNode, "use-attribute-sets",
                              a_useAttributeSets);
                    domCopyNS (currentNode, xs->lastNode);
                    if (str) {
                        rc = ExecUseAttributeSets (xs, context, currentNode,
                                                   currentPos, actionNode,
                                                   str, errMsg);
                        CHECK_RC;
                    }
                }
                /* process the children only for root and element nodes */
                xsltPushVarFrame (xs);
                rc = ExecActions(xs, context, currentNode, currentPos,
                                 actionNode->firstChild, errMsg);
                xsltPopVarFrame (xs);
                CHECK_RC;
                xs->lastNode = savedLastNode;
            } else
            if (currentNode->nodeType == PROCESSING_INSTRUCTION_NODE) {
                pi = (domProcessingInstructionNode*)currentNode;
                n = (domNode*)
                    domNewProcessingInstructionNode (xs->lastNode->ownerDocument,
                                                     pi->targetValue,
                                                     pi->targetLength,
                                                     pi->dataValue,
                                                     pi->dataLength);
                domAppendChild (xs->lastNode, n);

            } else
            if (currentNode->nodeType == COMMENT_NODE) {
                DBG(fprintf(stderr, "node is COMMENT_NODE \n");)
                tnode = (domTextNode *)currentNode;
                n = (domNode *) domAppendNewTextNode (xs->lastNode,
                                                      tnode->nodeValue,
                                                      tnode->valueLength,
                                                      COMMENT_NODE, 0);
            } else
            if (currentNode->nodeType == ATTRIBUTE_NODE) {
                DBG(fprintf(stderr, "node is ATTRIBUTE_NODE \n");)
                if (xs->lastNode->firstChild) {
                    /* Adding an Attribute to an element after
                       children have been added to it is an error.
                       Ignore the attribute. */
                    break;
                }
                if (xs->lastNode == xs->resultDoc->rootNode) {
                    reportError (actionNode, "Cannot write an attribute"
                                 " when there is no open start tag", errMsg);
                    return -1;
                }
                attr = (domAttrNode *)currentNode;
                domSetAttributeNS (xs->lastNode, attr->nodeName,
                                   attr->nodeValue,
                                   domNamespaceURI (currentNode), 1);
            }
            break;

        case copyOf:
            if (actionNode->firstChild) {
                reportError (actionNode, "xsl:copy-of has to be empty.", 
                             errMsg);
                return -1;
            }
            select = getAttr(actionNode, "select", a_select);
            if (!select) {
                reportError (actionNode, "xsl:copy-of: missing mandatory"
                             " attribute \"select\".", errMsg);
                return -1;
            }

            rc = evalXPath(xs, context, currentNode, currentPos, select,
                           &rs, errMsg);
            CHECK_RC;
            TRACE1(" copyOf select='%s':\n", select);
            DBG(rsPrint(&rs));
            if (rs.type == xNodeSetResult) {
                for (i=0; i<rs.nr_nodes; i++) {
                    if (rs.nodes[i]->nodeType == ATTRIBUTE_NODE) {
                        attr = (domAttrNode*)rs.nodes[i];
                        if (attr ->nodeFlags & IS_NS_NODE) {
                            /* If someone selects explicitely namespace nodes
                               to copy-of with e.g namespace::* (remember: @*
                               doesn't select namespace nodes), we must this
                               handle seperately.*/
                            /* The xmlns:xml namespace node will always
                               be in scope, but never needed to be copied,
                               because the result tree will also always
                               already have it. To suppress, that the result
                               tree gets glutted with xmlns:xml declarations
                               (they would not harm, but ev. irritate some and
                               are unnecessary, we check this here as a 
                               special case */
                            if (attr->namespace == 1) {
                                continue;
                            }
                            ns = NULL;
                        } else {
                            ns = domGetNamespaceByIndex (
                                attr->parentNode->ownerDocument, 
                                attr->namespace
                                );
                        }
                        if (ns) uri = ns->uri;
                        else uri = NULL;
                        if (xs->lastNode == xs->resultDoc->rootNode) {
                            reportError (actionNode, "Cannot write an"
                                         " attribute when there is no open"
                                         " start tag", errMsg);
                            xpathRSFree (&rs);
                            return -1;
                        }
                        domSetAttributeNS(xs->lastNode, attr->nodeName,
                                          attr->nodeValue, uri, 1);
                    } else {
                        if (*(rs.nodes[i]->nodeName) == '\0') {
                            /* The rootNode of the Document or the rootNode
                               of a result tree fragment */
                            child = rs.nodes[i]->firstChild;
                            while (child) {
                                domCopyTo(child, xs->lastNode, 1);
                                child = child->nextSibling;
                            }
                        } else {
                            domCopyTo (rs.nodes[i], xs->lastNode, 1);
                        }
                    }
                }
            } else {
                str = xpathFuncString( &rs );
                TRACE1("copyOf: xpathString='%s' \n", str);
                domAppendNewTextNode(xs->lastNode, str, strlen(str),
                                     TEXT_NODE, 0);
                FREE(str);
            }
            xpathRSFree( &rs );
            break;

        case decimalFormat: 
            reportError (actionNode, "xsl:decimal-format is only allowed"
                         " at toplevel.", errMsg);
            return -1;

        case element:
            nsAT = getAttr(actionNode, "namespace", a_namespace);
            str  = getAttr(actionNode, "name", a_name);
            if (!str) {
                reportError (actionNode, "xsl:element: missing mandatory"
                             " attribute \"name\".", errMsg);
                return -1;
            }

            rc = evalAttrTemplates( xs, context, currentNode, currentPos,
                                    str, &str2, errMsg);
            CHECK_RC;
            if (!domIsNAME (str2)) {
                reportError (actionNode, "xsl:element: Element name is not a"
                             " valid QName.", errMsg);
                FREE(str2);
                return -1;
            }
            out = NULL;
            nsStr = NULL;
            if (nsAT) {
                rc = evalAttrTemplates( xs, context, currentNode, currentPos,
                                        nsAT, &out, errMsg);
                CHECK_RC1(str2);
                nsStr = out;
            } else {
                domSplitQName (str2, prefix, &localName);
                if ((prefix[0] != '\0' &&  !domIsNCNAME (prefix))
                    || !domIsNCNAME (localName)) {
                    reportError (actionNode, "xsl:element: Element name is"
                                 " not a valid QName.", errMsg);
                    FREE(str2);
                    return -1;
                }
                ns = domLookupPrefix (actionNode, prefix);
                if (ns) nsStr = ns->uri;
                else {
                    if (prefix[0] != '\0') {
                        reportError (actionNode, "xsl:element: there isn't"
                                     " a URI associated with the prefix of"
                                     " the element name.", errMsg);
                        FREE(str2);
                        return -1;
                    }
                }
            }
            savedLastNode = xs->lastNode;
            xs->lastNode = domAppendNewElementNode (xs->lastNode, str2, nsStr);
            FREE(str2);
            if (nsAT) FREE(out);
            str = getAttr(actionNode, "use-attribute-sets", a_useAttributeSets);
            if (str) {
                TRACE1("use-attribute-sets = '%s' \n", str);
                rc = ExecUseAttributeSets (xs, context, currentNode,
                                           currentPos, actionNode,
                                           str, errMsg);
                CHECK_RC;
            }
            /* process the children as well */
            if (actionNode->firstChild) {
                xsltPushVarFrame (xs);
                rc = ExecActions(xs, context, currentNode, currentPos,
                                 actionNode->firstChild, errMsg);
                xsltPopVarFrame (xs);
            }
            xs->lastNode = savedLastNode;
            CHECK_RC;
            break;

        case fallback: return 0;

        case forEach:
            select = getAttr(actionNode, "select", a_select);
            if (!select) {
                reportError (actionNode, "xsl:for-each: The select attribute"
                             " is required.", errMsg);
                return -1;
            }
            DBG (
              if (currentNode->nodeType == ELEMENT_NODE) {
                  fprintf (stderr, 
                        "forEach select from Element Node '%s' domNode0x%x:\n",
                           currentNode->nodeName, currentNode);
                  if (currentNode->firstChild) {
                      fprintf(stderr, 
                              "forEach select from child '%s' domNode0x%x:\n",
                              currentNode->firstChild->nodeName, 
                              currentNode->firstChild);
                  }
              } else if (currentNode->nodeType == ATTRIBUTE_NODE) {
                  fprintf (stderr, "forEach select from Attribute Node '%s' Value '%s'\n", ((domAttrNode *)currentNode)->nodeName, ((domAttrNode *)currentNode)->nodeValue);
              } else {
                  fprintf (stderr, "forEach select from nodetype %d\n", currentNode->nodeType);
              }
            )
            xpathRSInit( &nodeList );
            rsAddNodeFast( &nodeList, currentNode );
            DBG(rsPrint( &nodeList ));
            rc = evalXPath(xs, &nodeList, currentNode, 1, select, &rs, errMsg);
            xpathRSFree (&nodeList);
            if (rc < 0) {
                return rc;
            }
            CHECK_RC;
            TRACE1("forEach: evalXPath for select = '%s' gave back:\n", select);
            DBG(rsPrint(&rs));

            if (rs.type == xNodeSetResult) {
                rc = doSortActions (xs, &rs, actionNode, context, currentNode,
                                    currentPos, errMsg);
                if (rc < 0) {
                    xpathRSFree (&rs);
                    return rc;
                }
                /* should not be necessary, because every node set is
                   returned already in doc Order */
                /*  if (!sorted) sortByDocOrder(&rs); */

                TRACE1(" forEach for select = '%s': (SORTED)\n", select);
                DBG(rsPrint(&rs));
                currentTplRule = xs->currentTplRule;
                xs->currentTplRule = NULL;
                xsltPushVarFrame (xs);
                for (i=0; i<rs.nr_nodes; i++) {
                    /* process the children as well */
                    rc = ExecActions(xs, &rs, rs.nodes[i], i,
                                     actionNode->firstChild, errMsg);
                    if (rc < 0) {
                        xsltPopVarFrame (xs);
                        xpathRSFree( &rs );
                        return rc;
                    }
                    if ((&xs->varFramesStack[xs->varFramesStackPtr])->polluted) {
                        xsltPopVarFrame (xs);
                        xsltPushVarFrame (xs);
                    }
                }
                xsltPopVarFrame (xs);
                xs->currentTplRule = currentTplRule;
            } else {
                if (rs.type != EmptyResult) {
                    reportError (actionNode, "The \"select\" expression of"
                                 " xsl:for-each elements must evaluate to a"
                                 " node set.", errMsg);
                    xpathRSFree (&rs);
                    return -1;
                }
            }
            xpathRSFree( &rs );
            break;

        case xsltIf:
            str = getAttr(actionNode, "test", a_test);
            if (str) {
                rc = evalXPath(xs, context, currentNode, currentPos, str,
                               &rs, errMsg);
                CHECK_RC;
                b = xpathFuncBoolean( &rs );
                xpathRSFree( &rs );
                if (b) {
                    /* process the children as well */
                    xsltPushVarFrame (xs);
                    rc = ExecActions(xs, context, currentNode, currentPos,
                                     actionNode->firstChild, errMsg);
                    xsltPopVarFrame (xs);
                    CHECK_RC;
                }
            } else {
                reportError (actionNode, "xsl:if: missing mandatory attribute"
                             " \"test\".", errMsg);
                return -1;
            }
            break;

        case import:
            reportError (actionNode, "xsl:import is only allowed at toplevel.",
                         errMsg);
            return -1;
        case include:
            reportError (actionNode, "xsl:include is only allowed at"
                         " toplevel.", errMsg);
            return -1;
        case key:
            reportError (actionNode, "xsl:key is only allowed at toplevel.", 
                         errMsg);
            return -1;

        case message:
            str  = getAttr(actionNode,"terminate", a_terminate);
            if (!str) terminate = 0;
            else if (strcmp (str, "yes") == 0) terminate = 1;
            else if (strcmp (str, "no")  == 0) terminate = 0;
            else {
                reportError (actionNode, "Value for terminate should equal"
                             " 'yes' or 'no'", errMsg);
                return -1;
            }
            fragmentNode = domNewElementNode(xs->resultDoc, "",
                                             ELEMENT_NODE);
            savedLastNode = xs->lastNode;
            xs->lastNode = fragmentNode;
            xsltPushVarFrame (xs);
            rc = ExecActions(xs, context, currentNode, currentPos,
                             actionNode->firstChild, errMsg);
            xsltPopVarFrame (xs);
            CHECK_RC;

            str2 = xpathGetStringValue(fragmentNode, &len);
            xs->xsltMsgCB (xs->xsltMsgClientData, str2, len, terminate);
            FREE(str2);
            xs->lastNode = savedLastNode;
            domDeleteNode (fragmentNode, NULL, NULL);
            if (terminate) {
                reportError (actionNode, "xsl:message with attribute"
                             " \"terminate\"=\"yes\"", errMsg);
                return -1;
            }
            return 0;

        case namespaceAlias: 
            reportError (actionNode, "xsl:namespaceAlias is only allowed"
                         " at toplevel.", errMsg);
            return -1;

        case number:
            if (actionNode->firstChild) {
                reportError (actionNode, "xsl:number has to be empty.",
                             errMsg);
                return -1;
            }
            rc = xsltNumber(xs, context, currentNode, currentPos,
                            actionNode, errMsg);
            CHECK_RC;
            break;

        case output:  return 0;

        case otherwise:
            reportError (actionNode, "xsl:otherwise must be immediately"
                         " within xsl:choose", errMsg);
            return -1;
            
        case param:
            str = getAttr(actionNode, "name", a_name);
            if (str) {
                TRACE1("setting param '%s' ??\n", str);
                if (!xsltVarExists(xs, str, actionNode)) {
                    TRACE1("setting param '%s': yes \n", str);
                    select = getAttr(actionNode, "select", a_select);
                    if (select && actionNode->firstChild) {
                        reportError (actionNode, "An xsl:parameter element "
                                     " with a select attribute must be empty",
                                     errMsg);
                        return -1;
                    }
                    TRACE1("param select='%s'\n", select);
                    rc = xsltSetVar(xs, str, context, currentNode,
                                    currentPos, select, actionNode, 1, errMsg);
                    CHECK_RC;
                } 
            } else {
                reportError (actionNode, "xsl:param: missing mandatory "
                             " attribute \"name\".", errMsg);
                return -1;
            }
            break;

        case preserveSpace: return 0;

        case procinstr:
            str = getAttr(actionNode, "name", a_name);
            if (str) {
                rc = evalAttrTemplates( xs, context, currentNode, currentPos,
                                        str, &str2, errMsg);
                CHECK_RC;
                if (!domIsPINAME (str2) || !domIsNCNAME(str2)) {
                    reportError (actionNode, "xsl:processing-instruction: "
                                 "Processing instruction name is invalid.",
                                 errMsg);
                    FREE(str2);
                    return -1;
                }
            } else {
                reportError (actionNode, "xsl:processing-instruction:"
                             " missing mandatory attribute \"name\".", errMsg);
                return -1;
            }
            fragmentNode = domNewElementNode(xs->resultDoc, "", ELEMENT_NODE);
            savedLastNode = xs->lastNode;
            xs->lastNode = fragmentNode;
            xsltPushVarFrame (xs);
            rc = ExecActions(xs, context, currentNode, currentPos,
                             actionNode->firstChild, errMsg);
            xsltPopVarFrame (xs);
            CHECK_RC;
            child = fragmentNode->firstChild;
            while (child) {
                if (child->nodeType != TEXT_NODE) {
                    domDeleteNode (fragmentNode, NULL, NULL);
                    reportError (actionNode, "xsl:processing-instruction must "
                                 "not create nodes other than text nodes.",
                                 errMsg);
                    FREE(str2);
                    return -1;
                }
                child = child->nextSibling;
            }
            str = xpathGetStringValue (fragmentNode, &len);
            if (!domIsPIValue (str)) {
                reportError (actionNode, "Invalide processing instruction "
                             "value", errMsg);
                domDeleteNode (fragmentNode, NULL, NULL);
                FREE(str);
                FREE(str2);
                return -1;
            }
            xs->lastNode = savedLastNode;
            
            n = (domNode*)domNewProcessingInstructionNode( 
                xs->resultDoc, str2, strlen(str2), str, len);
            domAppendChild(xs->lastNode, n);
            domDeleteNode (fragmentNode, NULL, NULL);
            FREE(str2);
            FREE(str);
            break;

        case sort:
        case stylesheet:
        case stripSpace:
        case template:
            return 0;

        case text:
            str = getAttr(actionNode, "disable-output-escaping",
                          a_disableOutputEscaping);
            if (str) {
                if (strcmp (str, "yes")==0) disableEsc = 1;
            }
            pc = xpathGetStringValue (actionNode, &len);
            DBG(fprintf(stderr, "text: pc='%s'%d \n", pc, len);)
            domAppendNewTextNode(xs->lastNode, pc, len, TEXT_NODE, disableEsc);
            FREE(pc);
            break;

        case transform: return 0;

        case valueOf:
            if (actionNode->firstChild) {
                reportError (actionNode, "xsl:value-of has to be empty.",
                             errMsg);
                return -1;
            }
            str = getAttr(actionNode, "disable-output-escaping",
                          a_disableOutputEscaping);
            if (str) {
                if (strcmp (str, "yes")==0) disableEsc = 1;
            }
            str = getAttr(actionNode, "select", a_select);
            if (str) {
                TRACE1("valueOf: str='%s' \n", str);
                rc = evalXPath(xs, context, currentNode, currentPos, str,
                               &rs, errMsg);
                CHECK_RC;
                DBG(rsPrint(&rs));
                str = xpathFuncString( &rs );
                TRACE1("valueOf: xpathString='%s' \n", str);
                domAppendNewTextNode(xs->lastNode, str, strlen(str),
                                     TEXT_NODE, disableEsc);
                xpathRSFree( &rs );
                FREE(str);
            } else {
                reportError (actionNode, "xsl:value-of must have a"
                             " \"select\" attribute!", errMsg);
                return -1;
            }
            break;

        case variable:
            str = getAttr(actionNode, "name", a_name);
            if (str) {
                if (xsltVarExists (xs, str, actionNode)) {
                    Tcl_DStringInit (&dStr);
                    Tcl_DStringAppend (&dStr, "Variable '", -1);
                    Tcl_DStringAppend (&dStr, str, -1);
                    Tcl_DStringAppend (
                        &dStr, "' is already declared in this template", -1);
                    reportError (actionNode, Tcl_DStringValue (&dStr), errMsg);
                    return -1;
                }
                select = getAttr(actionNode, "select", a_select);
                if (select && actionNode->firstChild) {
                    reportError (actionNode, "An xsl:variable element with a"
                                 " select attribute must be empty", errMsg);
                    return -1;
                }
                TRACE1("variable select='%s'\n", select);
                rc = xsltSetVar(xs, str, context, currentNode, currentPos, 
                                select, actionNode, 1, errMsg);
                CHECK_RC;
            } else {
                reportError (actionNode, "xsl:variable must have a \"name\""
                             " attribute!", errMsg);
                return -1;
            }
            break;

        case when:
            reportError (actionNode, "xsl:when must be immediately within"
                         " xsl:choose", errMsg);
            return -1;
            
        case withParam:
            return 0;

        default:
            sDoc = xs->currentSubDoc;
            if (actionNode->namespace) {
                ns = actionNode->ownerDocument->namespaces[actionNode->namespace - 1];
                eNS = sDoc->extensionNS;
                while (eNS) {
                    if (eNS->uri) {
                        if (strcmp (eNS->uri, ns->uri)==0) break;
                    } else {
                        if (ns->prefix[0] == '\0') break;
                    }
                    eNS = eNS->next;
                }
                if (eNS) {
                    /* An extension element; process fallback */
                    child = actionNode->firstChild;
                    while (child) {
                        if (child->info == fallback) {
                            xsltPushVarFrame (xs);
                            rc = ExecActions (xs, context, currentNode,
                                              currentPos, child->firstChild,
                                              errMsg);
                            xsltPopVarFrame (xs);
                            CHECK_RC;
                        }
                        child = child->nextSibling;
                    }
                    return 0;
                }
            }
            savedLastNode = xs->lastNode;
            DBG(fprintf(stderr, "append new tag '%s' uri='%s' \n",
                        actionNode->nodeName, domNamespaceURI(actionNode) ););
            xs->lastNode = domAppendLiteralNode (xs->lastNode, actionNode);
            n = actionNode;

            while (n) {
                attr = n->firstAttr;
                while (attr && (attr->nodeFlags & IS_NS_NODE)) {
                    /* xslt namespace isn't copied */
                    /* Well, xslt implementors doesn't seem to agree
                       at which point this rule out of the second paragraph
                       of 7.1.1 must be applied: before or after applying
                       the namespace aliases (or, in other words: is this
                       rule (of not copying the XSLT namespace for lre)
                       considered, at the time, the lre is found in the
                       stylesheet or at the time, the lre is written to the
                       result doc). In deed the rec doesn't clarify this
                       explicitly. */
                    if (strcmp (attr->nodeValue, XSLT_NAMESPACE)==0){
                        attr = attr->nextSibling;
                        continue;
                    }
                    ns = n->ownerDocument->namespaces[attr->namespace-1];
                    rc = 0;
                    n1 = actionNode;
                    while (n1 != n) {
                        attr1 = n1->firstAttr;
                        while (attr1 && (attr1->nodeFlags & IS_NS_NODE)) {
                            ns1 = n1->ownerDocument->namespaces[attr1->namespace-1];
                            if (strcmp (ns1->prefix, ns->prefix)==0) {
                                rc = 1;
                                break;
                            }
                            attr1 = attr1->nextSibling;
                        }
                        if (rc) break;
                        n1 = n1->parentNode;
                    }
                    if (rc) {
                        attr = attr->nextSibling;
                        continue;
                    }

                    str = ns->uri;
                    nsAlias = xs->nsAliases;
                    while (nsAlias) {
                        if (strcmp (nsAlias->fromUri, ns->uri)==0) {
                            ns->uri = nsAlias->toUri;
                            break;
                        }
                        nsAlias = nsAlias->next;
                    }
                    eNS = sDoc->excludeNS;
                    while (eNS) {
                        if (eNS->uri) {
                            if (strcmp (eNS->uri, ns->uri)==0) break;
                        } else {
                            if (ns->prefix[0] == '\0') break;
                        }
                        eNS = eNS->next;
                    }
                    if (!eNS) {
                        eNS = sDoc->extensionNS;
                        while (eNS) {
                            if (eNS->uri) {
                                if (strcmp (eNS->uri, ns->uri)==0) break;
                            } else {
                                if (ns->prefix[0] == '\0') break;
                            }
                            eNS = eNS->next;
                        }
                        if (!eNS) {
                            domAddNSToNode (xs->lastNode, ns);
                        }
                    }
                    ns->uri = str;
                    attr = attr->nextSibling;
                }
                n = n->parentNode;
            }
            /* It's not clear, what to do, if the literal result
               element is in a namespace, that should be excluded. We
               follow saxon and xalan, which both add the namespace of
               the literal result element always to the result tree,
               to ensure, that the result tree is conform to the XML
               namespace recommendation. */
            if (actionNode->namespace) {
                ns = actionNode->ownerDocument->namespaces[actionNode->namespace-1];
                str = ns->uri;
                nsAlias = xs->nsAliases;
                while (nsAlias) {
                    if (strcmp (nsAlias->fromUri, ns->uri)==0) {
                        ns->uri = nsAlias->toUri;
                        break;
                    }
                    nsAlias = nsAlias->next;
                }
                ns1 = domAddNSToNode (xs->lastNode, ns);
                if (ns1) {
                    xs->lastNode->namespace = ns1->index;
                }
                ns->uri = str;
            } else {
                ns = domLookupPrefix (xs->lastNode, "");
                if (ns) {
                    if (strcmp (ns->uri, "")!=0) {
                        attr = domSetAttributeNS (xs->lastNode, "xmlns", "", 
                                                  NULL, 1);
                        if (attr) {
                            xs->lastNode->namespace = attr->namespace;
                        }
                    } else {
                        xs->lastNode->namespace = ns->index;
                    }
                }
            }

            n = xs->lastNode;
            /* process the attributes */
            attr = actionNode->firstAttr;
            while (attr) {
                if (attr->nodeFlags & IS_NS_NODE) {
                    attr = attr->nextSibling;
                    continue;
                }
                /* TODO: xsl:exclude-result-prefixes attribute on literal
                         elements on the ancestor-or-self axis */
                uri = domNamespaceURI((domNode*)attr);
                if (uri && strcmp(uri, XSLT_NAMESPACE)==0) {
                    domSplitQName((char*)attr->nodeName, prefix, &localName);
                    if (strcmp(localName,"use-attribute-sets")==0) {
                        str = attr->nodeValue;
                        rc = ExecUseAttributeSets (xs, context, currentNode,
                                                   currentPos, actionNode,
                                                   str, errMsg);
                        CHECK_RC;
                    }
                } else {
                    rc = evalAttrTemplates( xs, context, currentNode,
                                            currentPos, attr->nodeValue, &str,
                                            errMsg);
                    CHECK_RC;
                    if (uri) {
                        nsAlias = xs->nsAliases;
                        while (nsAlias) {
                            if (strcmp (nsAlias->fromUri, uri)==0) {
                                uri = nsAlias->toUri;
                                break;
                            }
                            nsAlias = nsAlias->next;
                        }
                    }
                    domSetAttributeNS (n, attr->nodeName, str, uri, 1);
                    FREE(str);
                }
                attr = attr->nextSibling;
            }
            /* process the children as well */
            xsltPushVarFrame (xs);
            rc = ExecActions(xs, context, currentNode, currentPos,
                             actionNode->firstChild, errMsg);
            xsltPopVarFrame (xs);
            CHECK_RC;
            xs->lastNode = savedLastNode;
            return 0;
    }
    return 0;
}

/*----------------------------------------------------------------------------
|   ExecActions
|
\---------------------------------------------------------------------------*/
static int ExecActions (
    xsltState       * xs,
    xpathResultSet  * context,
    domNode         * currentNode,
    int               currentPos,
    domNode         * actionNode,
    char           ** errMsg
)
{
    domNode *savedLastNode, *savedCurrentNode;
    int rc;

    savedLastNode    = xs->lastNode;
    savedCurrentNode = xs->current;

    while (actionNode) {
        xs->current = currentNode;
        rc = ExecAction (xs, context, currentNode, currentPos, actionNode,
                         errMsg);
        if (rc < 0) {
            xs->lastNode = savedLastNode;
            xs->current  = savedCurrentNode;
            return rc;
        }
        actionNode = actionNode->nextSibling;
    }
    xs->lastNode = savedLastNode;
    xs->current  = savedCurrentNode;
    return 0;
}

/*----------------------------------------------------------------------------
|   ApplyTemplate
|
\---------------------------------------------------------------------------*/
static int ApplyTemplate (
    xsltState      * xs,
    xpathResultSet * context,
    domNode        * currentNode,
    domNode        * exprContext,
    int              currentPos,
    const char     * mode,
    const char     * modeURI,
    char          ** errMsg
)
{
    xsltTemplate   *tpl;
    xsltTemplate   *tplChoosen, *currentTplRule;
    domNode        *child;
    xpathResultSet  rs;
    int             rc;
    double          currentPrio, currentPrec;
    char            prefix[MAX_PREFIX_LEN];
    const char     *localName;
    Tcl_HashEntry  *h;
    Tcl_DString     dStr;
    xsltSubDoc     *currentSubDoc;

    TRACE2("\n\nApplyTemplate mode='%s' currentPos=%d \n", mode, currentPos);
    DBG(printXML (currentNode, 0, 1);)

    /*--------------------------------------------------------------
    |   find template
    \-------------------------------------------------------------*/
    tplChoosen  = NULL;
    currentPrio = -100000.0;
    currentPrec = 0.0;


    if (currentNode->nodeType == ELEMENT_NODE) {
        Tcl_DStringInit (&dStr);
        if (currentNode->namespace) {
            domSplitQName (currentNode->nodeName, prefix, &localName);
            Tcl_DStringAppend (&dStr, domNamespaceURI (currentNode), -1);
            Tcl_DStringAppend (&dStr, ":", 1);
        }
        if (mode) {
            if (modeURI) {
                Tcl_DStringAppend (&dStr, modeURI, -1);
                Tcl_DStringAppend (&dStr, ":", 1);
            }
            Tcl_DStringAppend (&dStr, mode, -1);
            Tcl_DStringAppend (&dStr, ":", 1);
        }
        if (currentNode->namespace) {
            Tcl_DStringAppend (&dStr, localName, -1);
        } else {
            Tcl_DStringAppend (&dStr, currentNode->nodeName, -1);
        }
        h = Tcl_FindHashEntry (&xs->isElementTpls, Tcl_DStringValue (&dStr));
        Tcl_DStringFree (&dStr);

        if (h) {
            for (tpl = (xsltTemplate *) Tcl_GetHashValue (h);
                 tpl != NULL;
                 tpl = tpl->next) {
                TRACE3("find element tpl match='%s' mode='%s' name='%s'\n",
                       tpl->match, tpl->mode, tpl->name);
                TRACE4("tpl has prio='%f' precedence='%f'\n", tpl->prio, tpl->precedence, currentPrio, currentPrec);
                rc = xpathMatches ( tpl->ast, tpl->content, currentNode,
                                    &(xs->cbs), errMsg);
                if (rc < 0) {
                    TRACE1("xpathMatches had errors '%s' \n", *errMsg);
                    return rc;
                }
                if (rc == 0) continue;
                TRACE3("matching '%s': %f > %f ? \n", tpl->match, tpl->prio , currentPrio);
                tplChoosen = tpl;
                currentPrio = tpl->prio;
                currentPrec = tpl->precedence;
                break;
            }
        }
    }

    for( tpl = xs->templates; tpl != NULL; tpl = tpl->next) {

        TRACE3("find tpl match='%s' mode='%s' name='%s'\n",
               tpl->match, tpl->mode, tpl->name);

        /* exclude those, which don't match the current mode */
        if (   ( mode && !tpl->mode)
            || (!mode &&  tpl->mode)
            || ( mode &&  tpl->mode && (strcmp(mode,tpl->mode)!=0))
            || (!modeURI && tpl->modeURI)
            || ( modeURI && !tpl->modeURI)
            || ( modeURI && tpl->modeURI && (strcmp(modeURI, tpl->modeURI)!=0))
        ) {
            TRACE("doesn't match mode\n");
            continue; /* doesn't match mode */
        }
        TRACE4("tpl has prio='%f' precedence='%f', currentPrio='%f', currentPrec='%f'\n", tpl->prio, tpl->precedence, currentPrio, currentPrec);
        /* According to xslt rec 5.5: First test precedence */
        if (tpl->precedence < currentPrec) break;
        if (tpl->precedence == currentPrec) {
            if (tpl->prio < currentPrio) break;
            if (tpl->prio == currentPrio
                && domPrecedes (tpl->content, tplChoosen->content))
                break;
        }
        rc = xpathMatches ( tpl->ast, tpl->content, currentNode, &(xs->cbs),
                            errMsg);
        TRACE1("xpathMatches = %d \n", rc);
        if (rc < 0) {
            TRACE1("xpathMatches had errors '%s' \n", *errMsg);
            return rc;
        }
        if (rc == 0) continue;
        TRACE3("matching '%s': %f > %f ? \n", tpl->match, tpl->prio , currentPrio);
        tplChoosen = tpl;
        TRACE1("TAKING '%s' \n", tpl->match);
        break;
    }

    if (tplChoosen == NULL) {
        TRACE("nothing matches -> execute built-in template \n");

        /*--------------------------------------------------------------------
        |   execute built-in template
        \-------------------------------------------------------------------*/
        if (currentNode->nodeType == TEXT_NODE) {
            domAppendNewTextNode(xs->lastNode,
                                 ((domTextNode*)currentNode)->nodeValue,
                                 ((domTextNode*)currentNode)->valueLength,
                                 TEXT_NODE, 0);
            return 0;
        } else
        if (currentNode->nodeType == DOCUMENT_NODE) {
            child = ((domDocument*)currentNode)->documentElement;
        } else
        if (currentNode->nodeType == ELEMENT_NODE) {
            child = currentNode->firstChild;
        } else
        if (currentNode->nodeType == ATTRIBUTE_NODE) {
            domAppendNewTextNode (xs->lastNode,
                                  ((domAttrNode*)currentNode)->nodeValue,
                                  ((domAttrNode*)currentNode)->valueLength,
                                  TEXT_NODE, 0);
            return 0;
        } else {
            return 0; /* for all other nodes we don't have to recurse deeper */
        }
        xpathRSInit( &rs );
        while (child) {
            rsAddNodeFast ( &rs, child);
            child = child->nextSibling;
        }
        rc = ApplyTemplates (xs, context, currentNode, currentPos, exprContext,
                             &rs, mode, modeURI, errMsg);
        xpathRSFree( &rs );
        CHECK_RC;

    } else {
        TRACE1("tplChoosen '%s' \n", tplChoosen->match);
        currentTplRule = xs->currentTplRule;
        currentSubDoc = xs->currentSubDoc;
        xs->currentTplRule = tplChoosen;
        xs->currentSubDoc = tplChoosen->sDoc;
        DBG(printXML (tplChoosen->content->firstChild, 0, 1);)
        rc = ExecActions(xs, context, currentNode, currentPos,
                         tplChoosen->content->firstChild, errMsg);
        TRACE1("ApplyTemplate/ExecActions rc = %d \n", rc);
        xs->currentTplRule = currentTplRule;
        xs->currentSubDoc = currentSubDoc;
        CHECK_RC;
    }
    return 0;
}


/*----------------------------------------------------------------------------
|   ApplyTemplates
|
\---------------------------------------------------------------------------*/
static int ApplyTemplates (
    xsltState      * xs,
    xpathResultSet * context,
    domNode        * currentNode,
    int              currentPos,
    domNode        * actionNode,
    xpathResultSet * nodeList,
    const char     * mode,
    const char     * modeURI,
    char          ** errMsg
)
{
    domNode  * savedLastNode;
    int        i, rc, needNewVarFrame = 1;

    if (nodeList->type == xNodeSetResult) {
        savedLastNode = xs->lastNode;
        for (i=0; i < nodeList->nr_nodes; i++) {
            if (needNewVarFrame) {
                xsltPushVarFrame (xs);
                SETPARAMDEF;
                rc = setParamVars (xs, context, currentNode, currentPos,
                                   actionNode, errMsg);
                if (rc < 0) {
                    xsltPopVarFrame (xs);
                    xs->lastNode = savedLastNode;
                    return rc;
                }
                SETSCOPESTART;
                (&xs->varFramesStack[xs->varFramesStackPtr])->polluted = 0;
            }
            rc = ApplyTemplate (xs, nodeList, nodeList->nodes[i], actionNode,
                                i, mode, modeURI, errMsg);
            if (rc < 0) {
                xsltPopVarFrame (xs);
                xs->lastNode = savedLastNode;
                return rc;
            }
            if ((&xs->varFramesStack[xs->varFramesStackPtr])->polluted) {
                xsltPopVarFrame (xs);
                needNewVarFrame = 1;
            } else needNewVarFrame = 0;
        }
        if (!needNewVarFrame) {
            xsltPopVarFrame (xs);
        }
        xs->lastNode = savedLastNode;
    } else {
        TRACE("ApplyTemplates: nodeList not a NodeSetResult !!!\n");
        DBG(rsPrint(nodeList);)
    }
    return 0;
}


/*----------------------------------------------------------------------------
|   fillElementList
|
\---------------------------------------------------------------------------*/
static int fillElementList (
    xsltWSInfo   * wsInfo,
    int            strip,
    double         precedence,
    domNode      * node,
    char         * str,
    char        ** errMsg
)
{
    char *pc, *start, save;
    char prefix[MAX_PREFIX_LEN];
    const char *localName;
    double *f;
    int   hnew;
    Tcl_HashEntry *h;
    Tcl_DString dStr;
    domNS  *ns;

    pc = str;
    while (*pc) {
        while (*pc && IS_XML_WHITESPACE(*pc)) pc++;
        if (*pc == '\0') break;
        start = pc;
        while (*pc && !IS_XML_WHITESPACE(*pc)) pc++;
        save = *pc;
        *pc = '\0';
        wsInfo->hasData = 1;
        if (strcmp (start, "*")==0) {
            if (strip) wsInfo->stripAll = 1;
            else       wsInfo->stripAll = 0;
            wsInfo->wildcardPrec = precedence;
        } else {
            Tcl_DStringInit (&dStr);
            ns = NULL;
            domSplitQName (start, prefix, &localName);
            if (prefix[0] != '\0') {
                if (!domIsNCNAME (prefix)) {
                    reportError (node, "Invalid token", errMsg);
                    *pc = save;
                    Tcl_DStringFree (&dStr);
                    return -1;
                }
                ns = domLookupPrefix (node, prefix);
                if (!ns) {
                    reportError (node, "prefix isn't bound to a namespace",
                                 errMsg);
                    *pc = save;
                    Tcl_DStringFree (&dStr);
                    return -1;
                }
                Tcl_DStringAppend (&dStr, ns->uri, -1);
                Tcl_DStringAppend (&dStr, ":", 1);
            }
            if (strcmp ("*", localName) != 0) {
                if (!domIsNCNAME (localName)) {
                    reportError (node, "Invalid token", errMsg);
                    *pc = save;
                    Tcl_DStringFree (&dStr);
                    return -1;
                }
            }
            Tcl_DStringAppend (&dStr, localName, -1);
            if (strip) {
                h = Tcl_FindHashEntry (&wsInfo->preserveTokens, 
                                       Tcl_DStringValue (&dStr));
            } else {
                h = Tcl_FindHashEntry (&wsInfo->stripTokens,
                                       Tcl_DStringValue (&dStr));
            }
            if (h) {
                FREE (Tcl_GetHashValue (h));
                Tcl_DeleteHashEntry (h);
            }
            if (strip) {
                h = Tcl_CreateHashEntry (&wsInfo->stripTokens, 
                                         Tcl_DStringValue (&dStr), &hnew);
            } else {
                h = Tcl_CreateHashEntry (&wsInfo->preserveTokens,
                                         Tcl_DStringValue (&dStr), &hnew);
            }
            if (hnew) {
                f = (double *)MALLOC(sizeof (double));
                *f = precedence;
                Tcl_SetHashValue (h, f);
            } else {
                f = (double *)Tcl_GetHashValue (h);
                *f = precedence;
            }
            Tcl_DStringFree (&dStr);
        }
        *pc = save;
    }
    return 1;
}

/*----------------------------------------------------------------------------
|   getCdataSectionElements
|
\---------------------------------------------------------------------------*/
static int
getCdataSectionElements (
    domNode        * node,
    char           * qnameList,
    Tcl_HashTable  * HashTable,
    char          ** errMsg
    )
{
    char *pc, *start, save, prefix[MAX_PREFIX_LEN];
    const char *localName;
    int hnew;
    Tcl_HashEntry *h;

    Tcl_DString dStr;
    domNS  *ns;

    Tcl_DStringInit (&dStr);
    pc = qnameList;
    while (*pc) {
        while (*pc && IS_XML_WHITESPACE(*pc)) pc++;
        if (*pc == '\0') break;
        start = pc;
        while (*pc && !IS_XML_WHITESPACE(*pc)) pc++;
        save = *pc;
        *pc = '\0';
        domSplitQName (start, prefix, &localName);
        if (prefix[0] != '\0') {
            if (!domIsNCNAME (prefix)) {
                Tcl_DStringSetLength (&dStr, 0);
                Tcl_DStringAppend (&dStr, "Invalid prefix '", -1);
                Tcl_DStringAppend (&dStr, prefix, -1);
                Tcl_DStringAppend (&dStr, "'.", 2);
                reportError (node, Tcl_DStringValue (&dStr), errMsg);
                Tcl_DStringFree (&dStr);
                return 0;
            }
            ns = domLookupPrefix (node, prefix);
            if (!ns) {
                Tcl_DStringSetLength (&dStr, 0);
                Tcl_DStringAppend (&dStr, "There isn't a namespace bound to"
                                   " the prefix '", -1);
                Tcl_DStringAppend (&dStr, prefix, -1);
                Tcl_DStringAppend (&dStr, "'.", 2);
                reportError (node, Tcl_DStringValue (&dStr), errMsg);
                Tcl_DStringFree (&dStr);
                return 0;
            }
            Tcl_DStringAppend (&dStr, ns->uri, -1);
            Tcl_DStringAppend (&dStr, ":", 1);
        }
        if (!domIsNCNAME (localName)) {
            Tcl_DStringSetLength (&dStr, 0);
            Tcl_DStringAppend (&dStr, "Invalid name '", -1);
            Tcl_DStringAppend (&dStr, prefix, -1);
            Tcl_DStringAppend (&dStr, "'.", 2);
            reportError (node, Tcl_DStringValue (&dStr), errMsg);
            Tcl_DStringFree (&dStr);
            return 0;
        }
        Tcl_DStringAppend (&dStr, localName, -1);
        h = Tcl_CreateHashEntry (HashTable, Tcl_DStringValue (&dStr), &hnew);
        Tcl_DStringSetLength (&dStr, 0);
        *pc = save;
    }
    return 1;
}
        
/*----------------------------------------------------------------------------
|   StripXSLTSpace
|
\---------------------------------------------------------------------------*/
static void StripXSLTSpace (
    domNode    * node
)
{
    domNode *child, *newChild, *parent;
    int     i, len, onlySpace;
    char   *p;

    if (node->nodeType == TEXT_NODE) {
        node->info = (int)unknown;
        p = ((domTextNode*)node)->nodeValue;
        len = ((domTextNode*)node)->valueLength;
        onlySpace = 1;
        for (i=0; i<len; i++) {
            if (!IS_XML_WHITESPACE(*p)) {
                onlySpace = 0;
                break;
            }
            p++;
        }
        if (onlySpace) {
            if (node->parentNode && (node->parentNode->info == text)) {
                /* keep white texts below xsl:text elements */
                return;
            }
            parent = node->parentNode;
            while (parent) {
                p = getAttr(parent,"xml:space", a_space);
                if (p!=NULL) {
                    if (strcmp(p,"preserve")==0) return;
                    if (strcmp(p,"default")==0)  break;
                }
                parent = parent->parentNode;
            }
            DBG(fprintf(stderr, "removing domNode0x%x(len %d) under '%s' \n",
                        node, len, node->parentNode->nodeName);)
            domDeleteNode (node, NULL, NULL);
        }
    } else
    if (node->nodeType == ELEMENT_NODE) {
        getTag(node);
        child = node->firstChild;
        while (child) {
            newChild = child->nextSibling;
            StripXSLTSpace (child);
            child = newChild;
        }
    } else {
        node->info = (int)unknown;
    }
}


/*----------------------------------------------------------------------------
|   addExclExtNS
|
\---------------------------------------------------------------------------*/
static int
parseList (
    xsltSubDoc  *docData,
    domNode     *xsltRoot,
    char        *str,
    int          extensionNS,
    char       **errMsg
    )
{
    xsltExclExtNS *eNS;
    char          *pc, *start, save;
    domNS         *ns;

    if (str) {
        pc = str;
        while (*pc) {
            while (*pc && IS_XML_WHITESPACE(*pc))
                pc++;
            if (*pc == '\0') break;
            start = pc;
            while (*pc && !IS_XML_WHITESPACE(*pc))
                pc++;
            save = *pc;
            *pc = '\0';
            eNS = (xsltExclExtNS *)MALLOC(sizeof (xsltExclExtNS));
            eNS->uri = NULL;
            if (extensionNS) {
                eNS->next = docData->extensionNS;
                docData->extensionNS = eNS;
            } else {
                eNS->next = docData->excludeNS;
                docData->excludeNS = eNS;
            }
            if (strcmp (start, "#default")==0) {
                ns = domLookupPrefix (xsltRoot, "");
                if (!ns) {
                    reportError (xsltRoot, "All prefixes listed in"
                                 " exclude-result-prefixes and"
                                 " extension-element-prefixes must be"
                                 " bound to a namespace.",
                                 errMsg);
                    return -1;
                }
            } else {
                ns = domLookupPrefix (xsltRoot, start);
                if (!ns) {
                    reportError (xsltRoot, "All prefixes listed in"
                                 " exclude-result-prefixes and"
                                 " extension-element-prefixes must be"
                                 " bound to a namespace.",
                                 errMsg);
                    return -1;
                }
                eNS->uri = tdomstrdup (ns->uri);
            }
            *pc = save;
        }
    }
    return 1;
}

static int
addExclExtNS (
    xsltSubDoc  *docData,
    domNode     *xsltRoot,
    char       **errMsg
    )
{
    char  *str, *tailptr;
    int    rc;
    double d;

    str = getAttr (xsltRoot, "version", a_version);
    if (!str) {
        reportError (xsltRoot, "missing mandatory attribute \"version\".",
                     errMsg);
        return -1;
    }
    d = strtod (str, &tailptr);
    if (d == 0.0 && tailptr == str) {
        reportError (xsltRoot, "The value of the attribute \"version\" must"
                     " be a number.", errMsg);
        return -1;
    }
    if (d > 1.0) {
        docData->fwCmpProcessing = 1;
    } else {
        if (d != 1.0) {
            reportError (xsltRoot, "Strange \"version\" value.", errMsg);
            return -1;
            docData->fwCmpProcessing = 0;
        }
    }

    str = getAttr (xsltRoot, "exclude-result-prefixes",
                   a_excludeResultPrefixes);
    rc = parseList (docData, xsltRoot, str, 0, errMsg);
    CHECK_RC;

    str = getAttr (xsltRoot, "extension-element-prefixes",
                   a_extensionElementPrefixes);
    rc = parseList (docData, xsltRoot, str, 1, errMsg);
    CHECK_RC;
    return 1;
}

/*----------------------------------------------------------------------------
|   getExternalDocument
|
\---------------------------------------------------------------------------*/
static domDocument *
getExternalDocument (
    Tcl_Interp  *interp,
    xsltState   *xs,
    domDocument *xsltDoc,
    const char  *baseURI,
    const char  *href,
    int          isStylesheet,
    int          fixedXMLSource,
    char       **errMsg
    )
{
    Tcl_Obj      *cmdPtr, *resultObj, *extbaseObj, *xmlstringObj;
    Tcl_Obj      *channelIdObj, *resultTypeObj;
    int           len, mode, result, storeLineColumn;
    char         *resultType, *extbase, *xmlstring, *channelId, s[20];
    char         *extResolver = NULL;
    CONST84 char *str;
    domDocument  *doc;
    xsltSubDoc   *sdoc;
    XML_Parser    parser;
    Tcl_Channel   chan;
    Tcl_DString   dStr;
    
    if (isStylesheet && (href[0] == '\0')) {
        *errMsg = tdomstrdup("Recursive import/include: stylesheet tries "
                             "to access itself.");
        return NULL;
    }
    cmdPtr = Tcl_NewStringObj (xsltDoc->extResolver, -1);
    Tcl_IncrRefCount (cmdPtr);
    if (baseURI) {
        Tcl_ListObjAppendElement(interp, cmdPtr,
                                 Tcl_NewStringObj (baseURI,
                                                   strlen(baseURI)));
    } else {
        Tcl_ListObjAppendElement(interp, cmdPtr,
                                 Tcl_NewStringObj ("", 0));
    }
    Tcl_ListObjAppendElement (interp, cmdPtr, (href ?
                              Tcl_NewStringObj (href, strlen (href))
                              : Tcl_NewStringObj ("", 0)));
    Tcl_ListObjAppendElement (interp, cmdPtr,
                              Tcl_NewStringObj ("", 0));

#if TclOnly8Bits
    result = Tcl_GlobalEvalObj(interp, cmdPtr);
#else 
    result = Tcl_EvalObjEx (interp, cmdPtr, TCL_EVAL_DIRECT | TCL_EVAL_GLOBAL);
#endif    

    Tcl_DecrRefCount (cmdPtr);
    resultObj = Tcl_GetObjResult (interp);
    Tcl_IncrRefCount (resultObj);

    if (result != TCL_OK) {
        goto wrongScriptResult;
    }

    result = Tcl_ListObjLength (interp, resultObj, &len);
    if ((result != TCL_OK) || (len != 3)) {
        goto wrongScriptResult;
    }
    result = Tcl_ListObjIndex (interp, resultObj, 0, &resultTypeObj);
    if (result != TCL_OK) {
        goto wrongScriptResult;
    }
    resultType = Tcl_GetString(resultTypeObj);
    if (strcmp (resultType, "string") == 0) {
        result = Tcl_ListObjIndex (interp, resultObj, 2, &xmlstringObj);
        xmlstring = Tcl_GetStringFromObj (xmlstringObj, &len);
        chan = NULL;
    } else if (strcmp (resultType, "channel") == 0) {
        xmlstring = NULL;
        len = 0;
        result = Tcl_ListObjIndex (interp, resultObj, 2, &channelIdObj);
        channelId = Tcl_GetString(channelIdObj);
        chan = Tcl_GetChannel (interp, channelId, &mode);
        if (chan == (Tcl_Channel) NULL) {
            goto wrongScriptResult;
        }
        if ((mode & TCL_READABLE) == 0) {
            *errMsg = tdomstrdup("-externalentitycommand returned a channel that wasn't opened for reading");
            return NULL;
        }
    } else if (strcmp (resultType, "filename") == 0) {
          *errMsg = tdomstrdup("-externalentitycommand result type \"filename\" not yet implemented");
          return NULL;
    } else {
        goto wrongScriptResult;
    }
    result = Tcl_ListObjIndex (interp, resultObj, 1, &extbaseObj);
    extbase = Tcl_GetString(extbaseObj);

    /* Since stylesheets and source docouments have different white space
       stripping rules, an already parsed tree could only reused, if the
       'usage type' of the already present tree is the same as for the
       currently requested document */
    sdoc = xs->subDocs;
    while (sdoc) {
        if (isStylesheet == sdoc->isStylesheet
            && sdoc->baseURI
            && strcmp(sdoc->baseURI, extbase) == 0) {
            Tcl_DecrRefCount (resultObj);
            return sdoc->doc;
        }
        sdoc = sdoc->next;
    }
    
    if (xsltDoc->documentElement->nodeFlags & HAS_LINE_COLUMN) {
        storeLineColumn = 1;
    } else {
        storeLineColumn = 0;
    }

    parser = XML_ParserCreate_MM (NULL, MEM_SUITE, NULL);

    Tcl_ResetResult (interp);
    if (xsltDoc->extResolver) {
        extResolver = tdomstrdup (xsltDoc->extResolver);
    }
    /* keep white space, no fiddling with the encoding (is this
       a good idea?) */
    doc = domReadDocument (parser, xmlstring, len, 0, 0, storeLineColumn, 0,
                           chan, extbase, extResolver, 0, 
                           (int) XML_PARAM_ENTITY_PARSING_ALWAYS, interp);

    if (doc == NULL) {
        DBG(fprintf (stderr, "parse error, str len %d, xmlstring: -->%s<--\n",
                     strlen (xmlstring), xmlstring);)
        Tcl_DStringInit (&dStr);
        Tcl_DStringAppend (&dStr, "Error while processing external entity \"",
                           -1);
        Tcl_DStringAppend (&dStr, href, -1);
        Tcl_DStringAppend (&dStr, "\":\n", -1);
        str = Tcl_GetStringResult (interp);
        if (str[0] == '\0') {
            Tcl_DStringAppend (&dStr, "At line ", -1);
            sprintf (s, "%ld", XML_GetCurrentLineNumber (parser));
            Tcl_DStringAppend (&dStr, s, -1);
            Tcl_DStringAppend (&dStr, " character ", -1);
            sprintf (s, "%ld", XML_GetCurrentColumnNumber (parser));
            Tcl_DStringAppend (&dStr, s, -1);
            Tcl_DStringAppend (&dStr, ": ", 2);
            Tcl_DStringAppend (&dStr, 
                               XML_ErrorString (XML_GetErrorCode(parser)), -1);
        } else {
            Tcl_DStringAppend (&dStr, str, -1);
        }
        *errMsg = tdomstrdup (Tcl_DStringValue (&dStr));
        Tcl_DStringFree (&dStr);
        XML_ParserFree (parser);
        Tcl_DecrRefCount (resultObj);
        return NULL;
    }
    XML_ParserFree (parser);

    /* TODO: If the stylesheet use the
       literal-result-element-as-stylesheet form, rewrite it to a
       "ordinary" stylesheet with root element xsl:stylesheet, with
       one template child with match pattern "/". */

    sdoc = (xsltSubDoc*)MALLOC(sizeof (xsltSubDoc));
    sdoc->doc = doc;
    sdoc->baseURI = tdomstrdup (extbase);
    Tcl_InitHashTable (&(sdoc->keyData), TCL_STRING_KEYS);
    sdoc->excludeNS = NULL;
    sdoc->extensionNS = NULL;
    sdoc->fwCmpProcessing = 0;
    sdoc->mustFree = 1;
    sdoc->isStylesheet = isStylesheet;
    sdoc->fixedXMLSource = fixedXMLSource;
    if (isStylesheet) {
        if (addExclExtNS (sdoc, doc->documentElement, errMsg) < 0) {
            Tcl_DeleteHashTable (&(sdoc->keyData));
            domFreeDocument (sdoc->doc, NULL, NULL);
            FREE (sdoc->baseURI);
            FREE (sdoc);
            Tcl_DecrRefCount (resultObj);
            return NULL;
        }
        StripXSLTSpace (doc->rootNode);
    }
    sdoc->next = xs->subDocs;
    xs->subDocs = sdoc;
    Tcl_DecrRefCount (resultObj);

    return doc;

 wrongScriptResult:
    *errMsg = tdomstrdup(Tcl_GetStringResult(interp));
    Tcl_DecrRefCount (resultObj);
    return NULL;
}

/*----------------------------------------------------------------------------
|   processTopLevelVars
|
\---------------------------------------------------------------------------*/
static int processTopLevelVars (
    domNode       * xmlNode,
    xsltState     * xs,
    char         ** parameters,
    int             ignoreUndeclaredParameters,
    char         ** errMsg
    )
{
    int                rc, i;
    char              *select, prefix[MAX_PREFIX_LEN];
    const char        *localName;
    xpathResultSet     nodeList, rs;
    Tcl_HashEntry     *entryPtr;
    Tcl_HashSearch     search;
    xsltTopLevelVar   *topLevelVar;
    xsltVarInProcess   varInProcess;
    xsltVariable      *var;
    domNS             *ns;
    Tcl_DString        dStr;

    xpathRSInit (&nodeList);
    rsAddNodeFast (&nodeList, xmlNode);

    if (parameters) {
        i = 0;
        while (parameters[i]) {
            domSplitQName (parameters[i], prefix, &localName);
            ns = NULL;
            if (prefix[0] != '\0') {
                ns = domLookupPrefix (xs->xsltDoc->documentElement, prefix);
                if (!ns) {
                    Tcl_DStringInit (&dStr);
                    Tcl_DStringAppend (&dStr, "No namespace bound to prefix"
                                       " (passed parameter \"", -1);
                    Tcl_DStringAppend (&dStr, parameters[i], -1);
                    Tcl_DStringAppend (&dStr, "\")", -1);
                    *errMsg = tdomstrdup (Tcl_DStringValue (&dStr));
                    Tcl_DStringFree (&dStr);
                    xpathRSFree (&nodeList);
                    return -1;
                }
            }
            Tcl_DStringInit (&dStr);
            if (ns) Tcl_DStringAppend (&dStr, ns->uri, -1);
            Tcl_DStringAppend (&dStr, localName, -1);
            entryPtr = Tcl_FindHashEntry (&xs->topLevelVars,
                                          Tcl_DStringValue (&dStr));
            Tcl_DStringFree (&dStr);
            if (!entryPtr) {
                if (ignoreUndeclaredParameters) {
                    i += 2;
                    continue;
                }
                Tcl_DStringInit (&dStr);
                Tcl_DStringAppend (&dStr, "There isn't a parameter named \"", -1);
                Tcl_DStringAppend (&dStr, parameters[i], -1);
                Tcl_DStringAppend (&dStr, "\" defined at top level in the stylesheet.", -1);
                *errMsg = tdomstrdup (Tcl_DStringValue (&dStr));
                Tcl_DStringFree (&dStr);
                xpathRSFree (&nodeList);
                return -1;
            }
            topLevelVar = (xsltTopLevelVar *) Tcl_GetHashValue (entryPtr);
            if (!topLevelVar->isParameter) {
                Tcl_DStringInit (&dStr);
                Tcl_DStringAppend (&dStr, "\"", 1);
                Tcl_DStringAppend (&dStr, parameters[i], -1);
                Tcl_DStringAppend (&dStr, "\" is defined as variable, not as parameter.", -1);
                *errMsg = tdomstrdup (Tcl_DStringValue (&dStr));
                Tcl_DStringFree (&dStr);
                xpathRSFree (&nodeList);
                return -1;
            }
            if (xsltVarExists (xs, parameters[i], NULL)) {
                i += 2;
                continue;
            }

            xpathRSInit (&rs);
            rsSetString (&rs, parameters[i+1]);

            xs->varStackPtr++;
            if (xs->varStackPtr >= xs->varStackLen) {
                xs->varStack = (xsltVariable *) REALLOC ((char*)xs->varStack,
                                                         sizeof (xsltVariable)
                                                         * 2*xs->varStackLen);
                xs->varStackLen *= 2;
            }
            var = &(xs->varStack[xs->varStackPtr]);
            if (!xs->varFramesStack->nrOfVars) {
                xs->varFramesStack->varStartIndex = xs->varStackPtr;
            }
            xs->varFramesStack->nrOfVars++;
            var->name   = localName;
            if (ns) var->uri = ns->uri;
            else    var->uri = NULL;
            var->node   = topLevelVar->node;
            var->rs     = rs;
            var->active = 1;

            i += 2;
        }
    }
    for (entryPtr = Tcl_FirstHashEntry(&xs->topLevelVars, &search);
            entryPtr != (Tcl_HashEntry*) NULL;
            entryPtr = Tcl_NextHashEntry(&search)) {
        topLevelVar = (xsltTopLevelVar *)Tcl_GetHashValue (entryPtr);
        if (xsltVarExists (xs, topLevelVar->name, topLevelVar->node)) {
            continue;
        }
        varInProcess.name = topLevelVar->name;
        varInProcess.next = NULL;
        xs->varsInProcess = &varInProcess;

        xs->currentXSLTNode = topLevelVar->node;
        select = getAttr (topLevelVar->node, "select", a_select);
        if (select && topLevelVar->node->firstChild) {
            xpathRSFree (&nodeList);
            reportError (topLevelVar->node, "xsl:variable and xsl:param"
                         " elements with a select attribute must be empty",
                         errMsg);
            return -1;
        }
        rc = xsltSetVar(xs, topLevelVar->name, &nodeList, xmlNode, 0, select,
                        topLevelVar->node, 1, errMsg);
        if (rc < 0) {
            xpathRSFree (&nodeList);
            return rc;
        }
    }
    xpathRSFree (&nodeList);
    xs->currentXSLTNode = NULL;
    xs->varsInProcess = NULL;
    return 0;
}

/*----------------------------------------------------------------------------
|   processTopLevel
|
\---------------------------------------------------------------------------*/
static int processTopLevel (
    Tcl_Interp    * interp,
    domNode       * xsltDocumentElement,
    xsltState     * xs,
    double          precedence,
    double        * precedenceLowBound,
    char         ** errMsg
)
{
    domNode           *node;
    domDocument       *extStyleSheet;
    int                rc, hnew, clen, newdf = 0, nonImportElemSeen = 0;
    int                ignore;
    double             childPrecedence, childLowBound;
    char              *str, *name, *match, *use, *href;
    char               prefix[MAX_PREFIX_LEN];
    const char        *localName, *baseURI;
    xsltTag            tag;
    xsltAttrSet       *attrSet;
    xsltKeyInfo       *keyInfo;
    xsltDecimalFormat *df;
    xsltTopLevelVar   *topLevelVar;
    xsltNSAlias       *nsAlias;
    domNS             *ns, *nsFrom, *nsTo;
    Tcl_HashEntry     *h;
    Tcl_DString        dStr;


    DBG(fprintf (stderr, "start processTopLevel. precedence: %f precedenceLowBound %f\n", precedence, *precedenceLowBound););
    node = xsltDocumentElement->firstChild;
    while (node) {
        tag = getTag (node);
        if (!nonImportElemSeen && tag != unknown && tag != import) {
            nonImportElemSeen = 1;
        }
        switch ( tag ) {

            case attributeSet:
                str = getAttr(node, "name", a_name);
                if (str) {
                    domSplitQName (str, prefix, &localName);
                    ns = NULL;
                    if (prefix[0] != '\0') {
                        ns = domLookupPrefix (node, prefix);
                        if (!ns) {
                            reportError (node, "There isn't a namespace"
                                         " bound to the prefix.", errMsg);
                            return -1;
                        }
                    }
                    if (xs->attrSets) {
                        attrSet = xs->attrSets;
                        while (attrSet->next) attrSet = attrSet->next;
                        attrSet->next = 
                            (xsltAttrSet*)MALLOC(sizeof(xsltAttrSet));
                        attrSet = attrSet->next;
                    } else {
                        attrSet = (xsltAttrSet*)MALLOC(sizeof(xsltAttrSet));
                        xs->attrSets = attrSet;
                    }
                    attrSet->next    = NULL;
                    attrSet->content = node;
                    attrSet->name    = localName;
                    if (ns) {
                        attrSet->uri = ns->uri;
                    } else {
                        attrSet->uri = NULL;
                    }
                } else {
                    reportError (node, "xsl:attribute-set: missing mandatory"
                                 " attribute \"name\".", errMsg);
                    return -1;
                }
                break;

            case decimalFormat:
                if (node->firstChild) {
                    reportError (node, "xsl:decimal-format has to be empty.", 
                                 errMsg);
                    return -1;
                }
                str = getAttr(node, "name", a_name);
                if (str) {
                    domSplitQName (str, prefix, &localName);
                    ns = NULL;
                    if (prefix[0] != '\0') {
                        ns = domLookupPrefix (node, prefix);
                        if (!ns) {
                            reportError (node, "There isn't a namespace bound"
                                         " to the prefix.", errMsg);
                            return -1;
                        }
                    }
                    /* a named decimal format */
                    df = xs->decimalFormats->next;
                    while (df) {
                        if (strcmp(df->name, str)==0
                            && ((df->uri == NULL && ns == NULL)
                                || (df->uri != NULL 
                                    && ns != NULL
                                    && (strcmp (df->uri, ns->uri)==0)))) {
                            /* already existing, override it */
                            break;
                        }
                        df = df->next;
                    }
                    if (df == NULL) {
                        df = (xsltDecimalFormat*)MALLOC(sizeof(xsltDecimalFormat));
                        memset (df, 0, sizeof (xsltDecimalFormat));
                        newdf = 1;
                        /* initialize to defaults */
#if TclOnly8Bits
                        df->decimalSeparator  = '.';
                        df->groupingSeparator = ',';
                        df->infinity          = "Infinity";
                        df->minusSign         = '-';
                        df->NaN               = "NaN";
                        df->percent           = '%';
                        df->zeroDigit         = '0';
                        df->digit             = '#';
                        df->patternSeparator  = ';';
#else 
                        df->decimalSeparator  = 46;          
                        df->groupingSeparator = 44;          
                        df->infinity          = "Infinity";  
                        df->minusSign         = 45;          
                        df->NaN               = "NaN";       
                        df->percent           = 37;          
                        df->perMille          = 0x2030;      
                        df->zeroDigit         = 48;          
                        df->digit             = 35;          
                        df->patternSeparator  = 59;
#endif /* TclOnly8Bits */
                        df->name = tdomstrdup(str);
                        if (ns) df->uri = tdomstrdup(ns->uri);
                        else df->uri = NULL;
                        /* prepend into list of decimal format
                           after the default one */
                        df->next = xs->decimalFormats->next;
                        xs->decimalFormats->next = df;
                    }
                } else {
                    /* definitions for default decimal format */
                    df = xs->decimalFormats;
                }
                str = getAttr(node, "decimal-separator",  a_decimalSeparator);
                if (str) {
#if TclOnly8Bits
                    if (str[1] != '\0') {
                        reportError (node, "decimal-separator has to be a"
                                     " single char", errMsg);
                        if (newdf) FREE((char*)df);
                        return -1;
                    }
                    df->decimalSeparator = str[0];
#else
                    clen = UTF8_CHAR_LEN (str[0]);
                    if (str[clen] != '\0') {
                        reportError (node, "decimal-separator has to be a"
                                     " single char", errMsg);
                        if (newdf) FREE((char*)df);
                        return -1;
                    }
                    Tcl_UtfToUniChar (str, &df->decimalSeparator);
#endif /* TclOnly8Bits */
                }
                str = getAttr(node, "grouping-separator", a_groupingSeparator);
                if (str) {
#if TclOnly8Bits
                    if (str[1] != '\0') {
                        reportError (node, "grouping-separator has to be a"
                                     " single char", errMsg);
                        if (newdf) FREE((char*)df);
                        return -1;
                    }
                    df->groupingSeparator = str[0];
#else 
                    clen = UTF8_CHAR_LEN (str[0]);
                    if (str[clen] != '\0') {
                        reportError (node, "groupingSeparator has to be a"
                                     " single char", errMsg);
                        if (newdf) FREE((char*)df);
                        return -1;
                    }
                    Tcl_UtfToUniChar (str, &df->groupingSeparator);
#endif /* TclOnly8Bits */                    
                }
                str = getAttr(node, "infinity",           a_infinity);
                if (str) df->infinity = str;
                str = getAttr(node, "minus-sign",         a_minusSign);
                if (str) {
#if TclOnly8Bits
                    if (str[1] != '\0') {
                        reportError (node, "minus-sign has to be a single"
                                     " char", errMsg);
                        return -1;
                    }
                    df->minusSign = str[0];
#else                     
                    clen = UTF8_CHAR_LEN (str[0]);
                    if (str[clen] != '\0') {
                        reportError (node, "minus-sign has to be a single"
                                     " char", errMsg);
                        if (newdf) FREE((char*)df);
                        return -1;
                    }
                    Tcl_UtfToUniChar (str, &df->minusSign);
#endif /* TclOnly8Bits */         
                }
                str = getAttr(node, "NaN",                a_nan);
                if (str) df->NaN = str;
                str = getAttr(node, "percent",            a_percent);
                if (str) {
#if TclOnly8Bits
                    if (str[1] != '\0') {
                        reportError (node, "percent has to be a single"
                                     " char", errMsg);
                        return -1;
                    }
                    df->percent = str[0];
#else
                    clen = UTF8_CHAR_LEN (str[0]);
                    if (str[clen] != '\0') {
                        reportError (node, "percent has to be a single"
                                     " char", errMsg);
                        if (newdf) FREE((char*)df);
                        return -1;
                    }
                    Tcl_UtfToUniChar (str, &df->percent);                    
#endif /* TclOnly8Bits */
                }
                str = getAttr(node, "per-mille",          a_perMille);
                if (str) {
#if TclOnly8Bits
                    reportError (node, "User defined per-mille sign not"
                                 " supported, sorry.", errMsg);
                    return -1;
#else
                    clen = UTF8_CHAR_LEN (str[0]);
                    if (str[clen] != '\0') {
                        reportError (node, "per-mille has to be a single"
                                     " char", errMsg);
                        if (newdf) FREE((char*)df);
                        return -1;
                    }
                    Tcl_UtfToUniChar (str, &df->perMille);                    
#endif /* TclOnly8Bits */
                }
                str = getAttr(node, "zero-digit",         a_zeroDigit);
                if (str) {
#if TclOnly8Bits                    
                    if (str[1] != '\0') {
                        reportError (node, "zero-digit has to be a single"
                                     " char", errMsg);
                        return -1;
                    }
                    df->zeroDigit = str[0];
#else
                    clen = UTF8_CHAR_LEN (str[0]);
                    if (str[clen] != '\0') {
                        reportError (node, "zero-digit has to be a single"
                                     " char", errMsg);
                        if (newdf) FREE((char*)df);
                        return -1;
                    }
                    Tcl_UtfToUniChar (str, &df->zeroDigit);                    
#endif /* TclOnly8Bits */
                }
                str = getAttr(node, "digit",              a_digit);
                if (str) {
#if TclOnly8Bits
                    if (str[1] != '\0') {
                        reportError (node, "digit has to be a single char",
                                     errMsg);
                        return -1;
                    }
                    df->digit = str[0];
#else 
                    clen = UTF8_CHAR_LEN (str[0]);
                    if (str[clen] != '\0') {
                        reportError (node, "digit has to be a single char",
                                     errMsg);
                        if (newdf) FREE((char*)df);
                        return -1;
                    }
                    Tcl_UtfToUniChar (str, &df->digit);
#endif /* TclOnly8Bits */
                }
                str = getAttr(node, "pattern-separator",  a_patternSeparator);
                if (str) {
#if TclOnly8Bits
                    if (str[1] != '\0') {
                        reportError (node, "pattern-separator has to be a"
                                     " single char", errMsg);
                        return -1;
                    }
                    df->patternSeparator = str[0];
#else
                    clen = UTF8_CHAR_LEN (str[0]);
                    if (str[clen] != '\0') {
                        reportError (node, "pattern-separator has to be a"
                                     " single char", errMsg);
                        return -1;
                    }
                    Tcl_UtfToUniChar (str, &df->patternSeparator);
#endif /* TclOnly8Bits */
                }
                break;

            case import:
                if (nonImportElemSeen) {
                    reportError (node, "xsl:import elements must come first",
                                 errMsg);
                    return -1;
                }
                if (node->firstChild) {
                    reportError (node, "xsl:import has to be empty!", errMsg);
                    return -1;
                }
                if (!node->ownerDocument->extResolver) {
                    reportError (node, "need resolver Script to include"
                                 " Stylesheet! (use"
                                 " \"-externalentitycommand\")", errMsg);
                    return -1;
                }
                baseURI = findBaseURI (node);
                href = getAttr (node, "href", a_href);
                if (!href) {
                    reportError (node, "xsl:import: missing mandatory"
                                 " attribute \"href\".", errMsg);
                    return -1;
                }
                extStyleSheet = getExternalDocument (interp, xs,
                                                     node->ownerDocument,
                                                     baseURI, href, 1, 0, 
                                                     errMsg);
                if (!extStyleSheet) {
                    return -1;
                }
                childPrecedence = (precedence + *precedenceLowBound) / 2;
                childLowBound = *precedenceLowBound;
                rc = processTopLevel (interp, extStyleSheet->documentElement,
                                      xs, childPrecedence, &childLowBound,
                                      errMsg);
                *precedenceLowBound = childPrecedence;
                if (rc != 0) {
                    return rc;
                }
                break;

            case include:
                if (node->firstChild) {
                    reportError (node, "xsl:include has to be empty.", errMsg);
                    return -1;
                }
                if (!node->ownerDocument->extResolver) {
                    reportError (node, "need resolver Script to include"
                                 "Stylesheet. (use"
                                 " \"-externalentitycommand\")", errMsg);
                    return -1;
                }
                baseURI = findBaseURI (node);
                href = getAttr (node, "href", a_href);
                if (!href) {
                    reportError (node, "xsl:include: missing mandatory"
                                 " attribute \"href\".", errMsg);
                    return -1;
                }
                extStyleSheet = getExternalDocument (interp, xs,
                                                     node->ownerDocument,
                                                     baseURI, href, 1, 0,
                                                     errMsg);
                if (!extStyleSheet) {
                    return -1;
                }
                xs->currentXSLTNode = extStyleSheet->documentElement;
                rc = processTopLevel (interp, extStyleSheet->documentElement,
                                      xs, precedence, precedenceLowBound,
                                      errMsg);
                if (rc != 0) {
                    return rc;
                }
                break;

            case key:
                if (node->firstChild) {
                    reportError (node, "xsl:key has to be empty.", errMsg);
                    return -1;
                }
                name = getAttr(node, "name", a_name);
                if (!name) {
                    reportError (node, "xsl:key: missing mandatory"
                                 " attribute \"name\".", errMsg);
                    return -1;
                }
                match = getAttr(node, "match", a_match);
                if (!match) {
                    reportError (node, "xsl:key: missing mandatory"
                                 " attribute \"match\".", errMsg);
                    return -1;
                }
                use = getAttr(node, "use", a_use);
                if (!use) {
                    reportError (node, "xsl:key: missing mandatory"
                                 " attribute \"use\".", errMsg);
                    return -1;
                }

                keyInfo = (xsltKeyInfo *)MALLOC(sizeof(xsltKeyInfo));
                keyInfo->node = node;
                rc = xpathParse (match, node, XPATH_KEY_MATCH_PATTERN, NULL,
                                 NULL, &(keyInfo->matchAst), errMsg);
                if (rc < 0) {
                    reportError (node, *errMsg, errMsg);
                    free ((char*)keyInfo);
                    return rc;
                }
                keyInfo->use       = use;
                rc = xpathParse (use, node, XPATH_KEY_USE_EXPR, NULL,
                                 NULL, &(keyInfo->useAst), errMsg);
                if (rc < 0) {
                    reportError (node, *errMsg, errMsg);
                    xpathFreeAst (keyInfo->matchAst);
                    free ((char*)keyInfo);
                    return rc;
                }
                domSplitQName (name, prefix, &localName);
                Tcl_DStringInit (&dStr);
                if (prefix[0] != '\0') {
                    ns = domLookupPrefix (node, prefix);
                    if (!ns) {
                        reportError (node, "There isn't a namespace bound"
                                     " to the prefix.", errMsg);
                        xpathFreeAst (keyInfo->matchAst);
                        xpathFreeAst (keyInfo->useAst);
                        FREE((char*)keyInfo);
                        return -1;
                    }
                    Tcl_DStringAppend (&dStr, ns->uri, -1);
                }
                Tcl_DStringAppend (&dStr, localName, -1);
                h = Tcl_CreateHashEntry (&(xs->keyInfos),
                                         Tcl_DStringValue (&dStr), &hnew);
                Tcl_DStringFree (&dStr);
                if (hnew) {
                    keyInfo->next  = NULL;
                } else {
                    keyInfo->next  = (xsltKeyInfo *)Tcl_GetHashValue (h);
                }
                Tcl_SetHashValue (h, keyInfo);
                break;

            case namespaceAlias:
                if (node->firstChild) {
                    reportError (node, "xsl:namespace-alias has to be empty.",
                                 errMsg);
                    return -1;
                }

                str = getAttr (node, "stylesheet-prefix", a_stylesheetPrefix);
                if (!str) {
                    reportError (node, "xsl:namespace-alias: missing"
                                 " mandatory attribute"
                                 " \"stylesheet-prefix\".", errMsg);
                    return -1 ;
                }
                if (strcmp (str, "#default")==0) {
                    str = NULL;
                    nsFrom = domLookupPrefix (node, "");
                } else {
                    nsFrom = domLookupPrefix (node, str);
                }
                if (!nsFrom) {
                    reportError (node, "xsl:namespace-alias: no namespace"
                                 " bound to the \"stylesheet-prefix\".", 
                                 errMsg);
                    return -1;
                }

                str = getAttr (node, "result-prefix", a_resultPrefix);
                if (!str) {
                    reportError (node, "xsl:namespace-alias: missing mandatory"
                                 " attribute \"result-prefix\".", errMsg);
                    return -1;
                }
                if (strcmp (str, "#default")==0) {
                    nsTo = domLookupPrefix (node, "");
                } else {
                    nsTo = domLookupPrefix (node, str);
                }
                if (!nsTo) {
                    reportError (node, "xsl:namespace-alias: no namespace"
                                 " bound to the \"result-prefix\".", errMsg);
                    return -1;
                }

                nsAlias = xs->nsAliases;
                ignore = 0;
                while (nsAlias) {
                    if (strcmp (nsAlias->fromUri, nsFrom->uri)==0) {
                        if (nsAlias->precedence > precedence) {
                            ignore = 1;
                        }
                        break;
                    }
                    nsAlias = nsAlias->next;
                }
                if (ignore) break;
                if (nsAlias) {
                    FREE(nsAlias->toUri);
                } else {
                    nsAlias = (xsltNSAlias *)MALLOC(sizeof (xsltNSAlias));
                    nsAlias->fromUri = tdomstrdup (nsFrom->uri);
                    nsAlias->next = xs->nsAliases;
                    xs->nsAliases = nsAlias;
                }
                nsAlias->toUri = tdomstrdup (nsTo->uri);
                nsAlias->precedence = precedence;
                break;

            case output:
                if (node->firstChild) {
                    reportError (node, "xsl:output has to be empty.", errMsg);
                    return -1;
                }
                str = getAttr(node, "method", a_method);
                if (str) { 
                    if (xs->doctype.method) FREE(xs->doctype.method);
                    xs->doctype.method    = tdomstrdup(str);
                }
                str = getAttr(node, "encoding", a_encoding);
                if (str) {
                    if (xs->doctype.encoding) FREE(xs->doctype.encoding);
                    xs->doctype.encoding  = tdomstrdup(str);
                }
                str = getAttr(node, "omit-xml-declaration", 
                              a_omitXMLDeclaration);
                if (str) {
                    if (strcmp (str, "yes") == 0) {
                        xs->doctype.omitXMLDeclaration = 1;
                    } else if (strcmp (str, "no") == 0) {
                        xs->doctype.omitXMLDeclaration = 0;
                    } else {
                        reportError (node, "Unexpected value for"
                                     " 'omit-xml-declaration' attribute",
                                     errMsg);
                        return -1;
                    }
                }
                str = getAttr(node, "standalone", a_standalone);
                if (str) {
                    if (strcmp (str, "yes") == 0) {
                        xs->doctype.standalone = 1;
                    } else if (strcmp (str, "no") == 0) {
                        xs->doctype.standalone = 0;
                    } else {
                        reportError (node, "Unexpected value for 'standalone'"
                                     " attribute", errMsg);
                        return -1;
                    }
                }
                str = getAttr(node, "doctype-public", a_doctypePublic);
                if (str) {
                    if (xs->doctype.publicId) {
                        FREE ((char*) xs->doctype.publicId);
                    }
                    xs->doctype.publicId = tdomstrdup(str);
                }
                str = getAttr(node, "doctype-system", a_doctypeSystem);
                if (str) {
                    if (xs->doctype.systemId) {
                        FREE ((char*) xs->doctype.systemId);
                    }
                    xs->doctype.systemId = tdomstrdup(str);
                }
                str = getAttr(node, "cdata-section-elements", 
                              a_cdataSectionElements);
                if (str) {
                    if (!xs->doctype.cdataSectionElements) {
                        xs->doctype.cdataSectionElements = 
                            (Tcl_HashTable *) MALLOC (sizeof (Tcl_HashTable));
                        Tcl_InitHashTable (xs->doctype.cdataSectionElements,
                                           TCL_STRING_KEYS);
                    }
                    if (!getCdataSectionElements (node, str,
                            xs->doctype.cdataSectionElements, errMsg)) {
                        return -1;
                    }
                }
                str = getAttr(node, "indent", a_indent);
                if (str) {
                    if (strcmp (str, "yes") == 0) {
                        xs->indentOutput = 1;
                    } else if (strcmp (str, "no") == 0) {
                        xs->indentOutput = 0;
                    } else {
                        reportError (node, "Unexpected value for 'indent'"
                                     " attribute.", errMsg);
                        return -1;
                    }
                }
                str = getAttr(node, "media-type", a_mediaType);
                if (str) { 
                    if (xs->doctype.mediaType) FREE(xs->doctype.mediaType);
                    xs->doctype.mediaType = tdomstrdup(str); 
                }
                break;

            case preserveSpace:
                if (node->firstChild) {
                    reportError (node, "xsl:preserve-space has to be empty.",
                                 errMsg);
                    return -1;
                }
                str = getAttr(node, "elements", a_elements);
                if (str) {
                    rc = fillElementList(&xs->wsInfo, 0, precedence,
                                         node, str, errMsg);
                    CHECK_RC;
                } else {
                    reportError (node, "xsl:preserve-space: missing required"
                                 " attribute \"elements\".", errMsg);
                    return -1;
                }
                break;

            case stripSpace:
                if (node->firstChild) {
                    reportError (node, "xsl:strip-space has to be empty.", 
                                 errMsg);
                    return -1;
                }
                str = getAttr(node, "elements", a_elements);
                if (str) {
                    rc = fillElementList(&xs->wsInfo, 1, precedence, node,
                                         str, errMsg);
                    CHECK_RC;
                } else {
                    reportError (node, "xsl:strip-space: missing required"
                                 " attribute \"elements\".", errMsg);
                    return -1;
                }
                break;

            case template:
                rc = xsltAddTemplate (xs, node, precedence, errMsg);
                CHECK_RC;
                break;

            case param:
            case variable:
                str = getAttr(node, "name", a_name);
                if (!str) {
                    reportError (node, "xsl:variable and xsl:param elements"
                                 " must have a \"name\" attribute.", errMsg);
                    return -1;
                }
                domSplitQName (str, prefix, &localName);
                ns = NULL;
                if (prefix[0] != '\0') {
                    ns = domLookupPrefix (node, prefix);
                    if (!ns) {
                        reportError (node, "There isn't a namespace bound"
                                     " to the prefix.", errMsg);
                        return -1;
                    }
                }
                Tcl_DStringInit (&dStr);
                if (ns) {
                    Tcl_DStringAppend (&dStr, ns->uri, -1);
                }
                Tcl_DStringAppend (&dStr, localName, -1);
                h = Tcl_CreateHashEntry (&(xs->topLevelVars),
                                         Tcl_DStringValue (&dStr), &hnew);
                Tcl_DStringFree (&dStr);
                if (!hnew) {
                    topLevelVar = (xsltTopLevelVar *)Tcl_GetHashValue (h);
                    /* Since imported stylesheets are processed at the
                       point at which they encounters the definitions are
                       already in increasing order of import precedence.
                       Therefor we have only to check, if there is a
                       top level var or parm with the same precedence */
                    if (topLevelVar->precedence == precedence) {
                        reportError (node, "There is already a variable"
                                     " or parameter with this name with the"
                                     " same import precedence.", errMsg);
                        return -1;
                    }
                } else {
                    topLevelVar = (xsltTopLevelVar *)
                        MALLOC (sizeof (xsltTopLevelVar));
                    Tcl_SetHashValue (h, topLevelVar);
                }
                topLevelVar->node = node;
                topLevelVar->name = str;
                if (tag == param) {
                    topLevelVar->isParameter = 1;
                } else {
                    topLevelVar->isParameter = 0;
                }
                topLevelVar->precedence = precedence;

                break;

            default:
                if (node->nodeType == ELEMENT_NODE) {
                    if (!node->namespace) {
                        reportError (node, "Top level elements must have a"
                                     " non-null namespace URI.", errMsg);
                        return -1;
                    }
                    if (strcmp (XSLT_NAMESPACE, domNamespaceURI (node))==0) {
                        if (!xs->currentSubDoc->fwCmpProcessing) {
                            reportError (node, "XSLT element not allowed"
                                         " on top level or unknown XSLT"
                                         " element.", errMsg);
                            
                            return -1;
                        }
                    }
                } else if (node->nodeType == TEXT_NODE) {
                    char *pc;
                    int   i, only_whites;
                    
                    only_whites = 1;
                    for (i=0, pc = ((domTextNode*)node)->nodeValue;
                         i < ((domTextNode*)node)->valueLength;
                         i++, pc++) {
                        if (!IS_XML_WHITESPACE(*pc)) {
                            only_whites = 0;
                            break;
                        }
                    }
                    if (!only_whites) {
                        reportError (node, "Non-whitespace text is not"
                                     " allowed between top level elements.",
                                     errMsg);
                        return -1;
                    }
                }
                    
                break;
        }
        node = node->nextSibling;
    }
    return 0;
}

/*----------------------------------------------------------------------------
|   xsltFreeState
|
\---------------------------------------------------------------------------*/
static void
xsltFreeState (
    xsltState      * xs
) {
    xsltDecimalFormat *df,  *dfsave;
    xsltKeyInfo       *ki,  *kisave;
    xsltNodeSet       *kvalues;
    xsltSubDoc        *sd,  *sdsave;
    xsltAttrSet       *as,  *assave;
    xsltTemplate      *tpl, *tplsave;
    xsltNumberFormat  *nf;
    ast                t;
    xsltTopLevelVar   *tlv;
    xsltNSAlias       *nsAlias, *nsAliasSave;
    xsltExclExtNS     *eNS, *eNSsave;
    Tcl_HashEntry     *entryPtr, *entryPtr1;
    Tcl_HashSearch     search, search1;
    Tcl_HashTable     *htable;
    double            *f;


    if (xs->doctype.systemId) FREE(xs->doctype.systemId);
    if (xs->doctype.publicId) FREE(xs->doctype.publicId);
    if (xs->doctype.internalSubset) FREE(xs->doctype.internalSubset);
    if (xs->doctype.cdataSectionElements) {
        Tcl_DeleteHashTable (xs->doctype.cdataSectionElements);
        FREE (xs->doctype.cdataSectionElements);
    }
    for (entryPtr = Tcl_FirstHashEntry (&xs->namedTemplates, &search);
         entryPtr != (Tcl_HashEntry*) NULL;
         entryPtr = Tcl_NextHashEntry (&search)) {
        tpl = (xsltTemplate *) Tcl_GetHashValue (entryPtr);
        if (!tpl->match) {
            FREE(tpl);
        }
    }
    Tcl_DeleteHashTable (&xs->namedTemplates);

    for (entryPtr = Tcl_FirstHashEntry (&xs->isElementTpls, &search);
         entryPtr != (Tcl_HashEntry*) NULL;
         entryPtr = Tcl_NextHashEntry (&search)) {
        tpl = (xsltTemplate *) Tcl_GetHashValue (entryPtr);
        while (tpl) {
            if (tpl->freeAst) xpathFreeAst (tpl->freeAst);        
            tplsave = tpl;
            tpl = tpl->next;
            FREE(tplsave);
        }
    }
    Tcl_DeleteHashTable (&xs->isElementTpls);

    for (entryPtr = Tcl_FirstHashEntry(&xs->xpaths, &search);
            entryPtr != (Tcl_HashEntry*) NULL;
            entryPtr = Tcl_NextHashEntry(&search)) {
        t = (ast) Tcl_GetHashValue (entryPtr);
        xpathFreeAst (t);
    }
    Tcl_DeleteHashTable(&xs->xpaths);

    for (entryPtr = Tcl_FirstHashEntry(&xs->pattern, &search);
            entryPtr != (Tcl_HashEntry*) NULL;
            entryPtr = Tcl_NextHashEntry(&search)) {
        t = (ast) Tcl_GetHashValue (entryPtr);
        xpathFreeAst (t);
    }
    Tcl_DeleteHashTable(&xs->pattern);

    for (entryPtr = Tcl_FirstHashEntry(&xs->formats, &search);
            entryPtr != (Tcl_HashEntry*) NULL;
            entryPtr = Tcl_NextHashEntry(&search)) {
        nf = (xsltNumberFormat *) Tcl_GetHashValue (entryPtr);
        FREE(nf->tokens);
        FREE(nf);
    }
    Tcl_DeleteHashTable(&xs->formats);

    if (&xs->topLevelVars) {
        for (entryPtr = Tcl_FirstHashEntry(&xs->topLevelVars, &search);
             entryPtr != (Tcl_HashEntry*) NULL;
             entryPtr = Tcl_NextHashEntry(&search)) {
            tlv = (xsltTopLevelVar *) Tcl_GetHashValue (entryPtr);
            FREE(tlv);
        }
        Tcl_DeleteHashTable (&xs->topLevelVars);
    }

    /*--- free key definition information ---*/
    for (entryPtr = Tcl_FirstHashEntry (&xs->keyInfos, &search);
         entryPtr != (Tcl_HashEntry*) NULL;
         entryPtr = Tcl_NextHashEntry (&search)) {
        ki = (xsltKeyInfo *) Tcl_GetHashValue (entryPtr);
        while (ki) {
            kisave = ki;
            ki = ki->next;
            xpathFreeAst (kisave->matchAst);
            xpathFreeAst (kisave->useAst);
            FREE(kisave);
        }
    }
    Tcl_DeleteHashTable (&xs->keyInfos);

    /*--- free sub documents ---*/
    sd = xs->subDocs;
    while (sd)  {
        sdsave = sd;
        sd = sd->next;
        for (entryPtr = Tcl_FirstHashEntry (&sdsave->keyData, &search);
             entryPtr != (Tcl_HashEntry*) NULL;
             entryPtr = Tcl_NextHashEntry (&search)) {
            htable = (Tcl_HashTable *) Tcl_GetHashValue (entryPtr);
            for (entryPtr1 = Tcl_FirstHashEntry (htable, &search1);
                 entryPtr1 != (Tcl_HashEntry*) NULL;
                 entryPtr1 = Tcl_NextHashEntry (&search1)) {
                kvalues = (xsltNodeSet *) Tcl_GetHashValue (entryPtr1);
                FREE(kvalues->nodes);
                FREE(kvalues);
            }
            Tcl_DeleteHashTable (htable);
            FREE(htable);
        }
        Tcl_DeleteHashTable (&sdsave->keyData);
        eNS = sdsave->excludeNS;
        while (eNS) {
            if (eNS->uri) FREE(eNS->uri);
            eNSsave = eNS;
            eNS = eNS->next;
            FREE(eNSsave);
        }
        eNS = sdsave->extensionNS;
        while (eNS) {
            if (eNS->uri) FREE(eNS->uri);
            eNSsave = eNS;
            eNS = eNS->next;
            FREE(eNSsave);
        }
        if (sdsave->baseURI) FREE(sdsave->baseURI);
        if (sdsave->mustFree) {
            domFreeDocument (sdsave->doc, NULL, NULL);
        }
        FREE(sdsave);
    }

    nsAlias = xs->nsAliases;
    while (nsAlias) {
        nsAliasSave = nsAlias;
        nsAlias = nsAlias->next;
        if (nsAliasSave->fromUri) FREE(nsAliasSave->fromUri);
        if (nsAliasSave->toUri) FREE(nsAliasSave->toUri);
        FREE(nsAliasSave);
    }

    /*--- free decimal formats ---*/
    df = xs->decimalFormats;
    while (df) {
        dfsave = df;
        df = df->next;
        if (dfsave->name) FREE(dfsave->name);
        if (dfsave->uri) FREE(dfsave->uri);
        FREE(dfsave);
    }

    /*--- free attribute sets ---*/
    as = xs->attrSets;
    while (as) {
       assave = as;
       as = as->next;
       FREE(assave);
    }

    /*--- free templates ---*/
    tpl = xs->templates;
    while (tpl) {
       tplsave = tpl;
       if (tpl->freeAst) xpathFreeAst (tpl->freeAst);
       tpl = tpl->next;
       FREE(tplsave);
    }

    for (entryPtr = Tcl_FirstHashEntry (&(xs->wsInfo.stripTokens), &search);
         entryPtr != (Tcl_HashEntry*) NULL;
         entryPtr = Tcl_NextHashEntry (&search)) {
        f = (double *) Tcl_GetHashValue (entryPtr);
        FREE(f);
    }
    Tcl_DeleteHashTable (&(xs->wsInfo.stripTokens));

    for (entryPtr = Tcl_FirstHashEntry (&(xs->wsInfo.preserveTokens), &search);
         entryPtr != (Tcl_HashEntry*) NULL;
         entryPtr = Tcl_NextHashEntry (&search)) {
        f = (double *) Tcl_GetHashValue (entryPtr);
        FREE(f);
    }
    Tcl_DeleteHashTable (&(xs->wsInfo.preserveTokens));

    FREE(xs->varFramesStack);
    FREE(xs->varStack);
    if (xs->doctype.method) FREE(xs->doctype.method);
    if (xs->doctype.encoding) FREE(xs->doctype.encoding);
    if (xs->doctype.mediaType) FREE(xs->doctype.mediaType);
    FREE (xs);
}

void
xsltFreeStateWrapper (
    void      *clientData
    )
{
    xsltFreeState ((xsltState *)clientData);
}

/*----------------------------------------------------------------------------
|   xsltResetState
|
\---------------------------------------------------------------------------*/
static void
xsltResetState (
    xsltState      * xs
    )
{
    xsltSubDoc        *sd,  *sdsave, *lastSubDoc = NULL;
    xsltNodeSet       *kvalues;
    Tcl_HashEntry     *entryPtr, *entryPtr1;
    Tcl_HashSearch     search, search1;
    Tcl_HashTable     *htable;
    



    /* Free the sub documents, which are resolved relative to nodes in
     * the xml source. */
    /* XML documents don't have excludeNS and extensionNS information
       and the already parsed XSLT documents information is
       preserved, therefor we don't touch excludeNS and extensionNS
       information */
    /* This loop works only as coded, because, the first subdoc will
     * always be the primary xslt doc, so xs->subDocs will not
     * change. Crusty stuff, this code. */
    sd = xs->subDocs;
    while (sd)  {
        sdsave = sd;
        sd = sd->next;
        if (sdsave->isStylesheet || sdsave->fixedXMLSource) {
            if (lastSubDoc) {
                lastSubDoc->next = sdsave;
            } else {
                xs->subDocs = sdsave;
            }
            lastSubDoc = sdsave;
            sdsave->next = NULL;
        } else {
            for (entryPtr = Tcl_FirstHashEntry (&sdsave->keyData, &search);
                 entryPtr != (Tcl_HashEntry*) NULL;
                 entryPtr = Tcl_NextHashEntry (&search)) {
                htable = (Tcl_HashTable *) Tcl_GetHashValue (entryPtr);
                for (entryPtr1 = Tcl_FirstHashEntry (htable, &search1);
                     entryPtr1 != (Tcl_HashEntry*) NULL;
                     entryPtr1 = Tcl_NextHashEntry (&search1)) {
                    kvalues = (xsltNodeSet *) Tcl_GetHashValue (entryPtr1);
                    FREE(kvalues->nodes);
                    FREE(kvalues);
                }
                Tcl_DeleteHashTable (htable);
                FREE(htable);
            }
            Tcl_DeleteHashTable (&sdsave->keyData);

            if (sdsave->mustFree) {
                domFreeDocument (sdsave->doc, NULL, NULL);
            }
            if (sdsave->baseURI) FREE(sdsave->baseURI);
            
            FREE(sdsave);
        }
    }
    xs->nsUniqeNr = 0;
    /* In theory, the varFramesStack and varStack pointers should
       be always back to there inital state. But to be sure, we
       re-initialize, just in case of a bizarre error or something. */
    xs->varFramesStackPtr = -1;
    xs->varStackPtr       = -1;
}

/*----------------------------------------------------------------------------
|   xsltCompileStylesheet
|
\---------------------------------------------------------------------------*/
void *
xsltCompileStylesheet (
    domDocument       * xsltDoc,
    xpathFuncCallback   funcCB,
    void              * xpathFuncClientData,
    int                 guardXSLTTree,
    char             ** errMsg
    )
{
    domNode        *node;
    int             rc;
    char           *tailptr;
    const char     *baseURI;
    double          d, precedence, precedenceLowBound;
    xsltState      *xs;
    xsltSubDoc     *sdoc;
    domAttrNode    *attr;
    xsltTemplate   *tpl;

    *errMsg = NULL;
    xs = (xsltState *) MALLOC (sizeof (xsltState));
    
    Tcl_InitHashTable ( &(xs->namedTemplates), TCL_STRING_KEYS);
    Tcl_InitHashTable ( &(xs->isElementTpls), TCL_STRING_KEYS);
    xs->cbs.varCB           = xsltGetVar;
    xs->cbs.varClientData   = (void*)xs;
    xs->cbs.funcCB          = xsltXPathFuncs;
    xs->cbs.funcClientData  = xs;
    xs->orig_funcCB         = funcCB;
    xs->orig_funcClientData = xpathFuncClientData;
    xs->xsltMsgCB           = NULL;
    xs->xsltMsgClientData   = NULL;
    xs->varFramesStack      = (xsltVarFrame *)MALLOC(sizeof (xsltVarFrame)*4);
    xs->varFramesStackPtr   = -1;
    xs->varFramesStackLen   = 4;
    xs->varStack            = (xsltVariable *)MALLOC(sizeof (xsltVariable)*8);
    xs->varStackPtr         = -1;
    xs->varStackLen         = 8;
    xs->templates           = NULL;
    xs->lastNode            = NULL;
    xs->attrSets            = NULL;
    xs->decimalFormats      = (xsltDecimalFormat*)MALLOC(sizeof (xsltDecimalFormat));
    xs->subDocs             = NULL;
    xs->currentTplRule      = NULL;
    xs->currentXSLTNode     = NULL;
    xs->xsltDoc             = xsltDoc;
    xs->varsInProcess       = NULL;
    xs->nsAliases           = NULL;
    xs->nsUniqeNr           = 0;
    Tcl_InitHashTable ( &(xs->wsInfo.stripTokens), TCL_STRING_KEYS);
    Tcl_InitHashTable ( &(xs->wsInfo.preserveTokens), TCL_STRING_KEYS);
    xs->wsInfo.hasData      = 0;
    xs->wsInfo.stripAll     = 0;
    xs->wsInfo.wildcardPrec = 0.0;
    Tcl_InitHashTable ( &(xs->xpaths), TCL_STRING_KEYS);
    Tcl_InitHashTable ( &(xs->pattern), TCL_STRING_KEYS);
    Tcl_InitHashTable ( &(xs->formats), TCL_STRING_KEYS);
    Tcl_InitHashTable ( &(xs->topLevelVars), TCL_STRING_KEYS);
    Tcl_InitHashTable ( &(xs->keyInfos), TCL_STRING_KEYS);
    xs->decimalFormats->name              = NULL;
    xs->decimalFormats->uri               = NULL;
#if TclOnly8Bits
    xs->decimalFormats->decimalSeparator  = '.';
    xs->decimalFormats->groupingSeparator = ',';
    xs->decimalFormats->minusSign         = '-';
    xs->decimalFormats->percent           = '%';
    xs->decimalFormats->zeroDigit         = '0';
    xs->decimalFormats->digit             = '#';
    xs->decimalFormats->patternSeparator  = ';';
#else 
    xs->decimalFormats->decimalSeparator  = 46;          
    xs->decimalFormats->groupingSeparator = 44;          
    xs->decimalFormats->minusSign         = 45;          
    xs->decimalFormats->percent           = 37;          
    xs->decimalFormats->perMille          = 0x2030;          
    xs->decimalFormats->zeroDigit         = 48;          
    xs->decimalFormats->digit             = 35;          
    xs->decimalFormats->patternSeparator  = 59;          
#endif /* TclOnly8Bits */
    xs->decimalFormats->infinity          = "Infinity";
    xs->decimalFormats->NaN               = "NaN";
    xs->decimalFormats->next              = NULL;
    xs->indentOutput = 0;
    memset (&xs->doctype, 0, sizeof (domDocInfo));
    
    node = xsltDoc->documentElement;

    /* add the xslt doc to the doc list */
    sdoc = (xsltSubDoc*)MALLOC(sizeof (xsltSubDoc));
    sdoc->doc = xsltDoc;
    baseURI = findBaseURI (xsltDoc->documentElement);
    if (baseURI) {
        sdoc->baseURI = tdomstrdup (baseURI);
    } else {
        sdoc->baseURI = NULL;
    }
    Tcl_InitHashTable (&(sdoc->keyData), TCL_STRING_KEYS);
    sdoc->excludeNS = NULL;
    sdoc->extensionNS = NULL;
    sdoc->fwCmpProcessing = 0;
    sdoc->isStylesheet = 1;
    sdoc->next = xs->subDocs;
    sdoc->mustFree = !guardXSLTTree;
    sdoc->fixedXMLSource = 0;
    xs->subDocs = sdoc;

    xs->currentSubDoc = sdoc;
    
    if ((getTag(node) != stylesheet) && (getTag(node) != transform)) {
        /* Check for "Literal Result Element as Stylesheet" (XSLT rec 2.3) */
        attr = domGetAttributeNodeNS (node, XSLT_NAMESPACE, "version");
        if (!attr) {
            reportError (node, "The supplied DOM tree does not appear to be"
                         " a stylesheet.", errMsg);
            goto error;
        }
        d = strtod (attr->nodeValue, &tailptr);
        if (d == 0.0 && tailptr == attr->nodeValue) {
            reportError (node, "The value of the attribute \"version\" must"
                         " be a number.", errMsg);
            goto error;
        }
        if (d > 1.0) {
            sdoc->fwCmpProcessing = 1;
        } else if (d < 1.0) {
            reportError (node, "Strange \"xsl:version\" value, don't know,"
                         " how to handle.", errMsg);
            goto error;
        }
        StripXSLTSpace (xsltDoc->rootNode);
        /* According to XSLT rec 2.3 we add the literal result element as
           template, which matches "/" */
        tpl = (xsltTemplate *) MALLOC (sizeof (xsltTemplate));
        tpl->match      = "/";
        tpl->name       = NULL;
        tpl->nameURI    = NULL;
        tpl->mode       = NULL;
        tpl->modeURI    = NULL;
        tpl->prio       = 0.5;
        tpl->content    = node->ownerDocument->rootNode;
        tpl->precedence = 1.0;
        tpl->next       = NULL;
        tpl->sDoc       = sdoc;
        rc = xpathParse (tpl->match, node, XPATH_TEMPMATCH_PATTERN, NULL, NULL,
                         &(tpl->freeAst), errMsg);
        tpl->ast        = tpl->freeAst;
        xs->templates = tpl;
        if (rc < 0) goto error;
    } else {
        rc = addExclExtNS (sdoc, node, errMsg);
        if (rc < 0) goto error;
        
        StripXSLTSpace (xsltDoc->rootNode);
        precedence = 1.0;
        precedenceLowBound = 0.0;
        rc = 0;
        rc = processTopLevel (xpathFuncClientData, node, xs, precedence, 
                              &precedenceLowBound, errMsg);
        if (rc != 0) goto error;
    }
        
    return xs;

 error:
    xsltFreeState (xs);
    return NULL;
}

/*----------------------------------------------------------------------------
|   xsltProcess
|
\---------------------------------------------------------------------------*/
int xsltProcess (
    domDocument       * xsltDoc,
    domNode           * xmlNode,
    void              * xsltCmdData,
    char             ** parameters,
    int                 ignoreUndeclaredParameters,
    xpathFuncCallback   funcCB,
    void              * xpathFuncClientData,
    xsltMsgCB           xsltMsgCB,
    void              * xsltMsgClientData,
    char             ** errMsg,
    domDocument      ** resultDoc   
    )
{
    xpathResultSet  nodeList;
    domNode        *node;
    int             rc, hnew;
    const char     *baseURI;
    xsltState      *xs;
    xsltSubDoc     *sdoc;
    Tcl_HashEntry  *entryPtr;
    Tcl_HashSearch  search;

    *errMsg = NULL;
    if (xsltCmdData) {
        xs = (xsltState *) xsltCmdData;
    } else {
        xs = (xsltState *) xsltCompileStylesheet (xsltDoc, funcCB,
                                                  xpathFuncClientData,
                                                  1, errMsg);
        if (!xs) return -1;
    }

    if (xmlNode->nodeType == DOCUMENT_NODE) {
        xmlNode = ((domDocument *)xmlNode)->rootNode;
    } else {
        xmlNode = xmlNode->ownerDocument->rootNode;
    }
    DBG(printXML(xmlNode, 0, 1);)

    if (xmlNode->ownerDocument->nodeFlags & NEEDS_RENUMBERING) {
        domRenumberTree (xmlNode->ownerDocument->rootNode);
        xmlNode->ownerDocument->nodeFlags &= ~NEEDS_RENUMBERING;
    }

    xs->resultDoc           = domCreateDoc(NULL, 0);
    if (xs->doctype.systemId) {
        xs->resultDoc->doctype = (domDocInfo *)MALLOC (sizeof (domDocInfo));
        memset (xs->resultDoc->doctype, 0, (sizeof (domDocInfo)));
        xs->resultDoc->doctype->systemId = tdomstrdup (xs->doctype.systemId);
    }
    if (xs->doctype.publicId) {
        if (!xs->resultDoc->doctype) {
            xs->resultDoc->doctype = (domDocInfo*)MALLOC (sizeof (domDocInfo));
            memset (xs->resultDoc->doctype, 0, (sizeof (domDocInfo)));
        }
        xs->resultDoc->doctype->publicId = tdomstrdup (xs->doctype.publicId);
    }
    if (xs->doctype.encoding) {
        if (!xs->resultDoc->doctype) {
            xs->resultDoc->doctype = (domDocInfo*)MALLOC (sizeof (domDocInfo));
            memset (xs->resultDoc->doctype, 0, (sizeof (domDocInfo)));
        }
        xs->resultDoc->doctype->encoding = tdomstrdup (xs->doctype.encoding);
    }
    if (xs->doctype.mediaType) {
        if (!xs->resultDoc->doctype) {
            xs->resultDoc->doctype = (domDocInfo*)MALLOC (sizeof (domDocInfo));
            memset (xs->resultDoc->doctype, 0, (sizeof (domDocInfo)));
        }
        xs->resultDoc->doctype->mediaType = tdomstrdup (xs->doctype.mediaType);
    }
    if (xs->doctype.standalone) {
        if (!xs->resultDoc->doctype) {
            xs->resultDoc->doctype = (domDocInfo*)MALLOC (sizeof (domDocInfo));
            memset (xs->resultDoc->doctype, 0, (sizeof (domDocInfo)));
        }
        xs->resultDoc->doctype->standalone = 1;
    }
    if (xs->indentOutput) {
        xs->resultDoc->nodeFlags |= OUTPUT_DEFAULT_INDENT;
    }
    if (xs->doctype.cdataSectionElements) {
        if (!xs->resultDoc->doctype) {
            xs->resultDoc->doctype = (domDocInfo*)MALLOC (sizeof (domDocInfo));
            memset (xs->resultDoc->doctype, 0, (sizeof (domDocInfo)));
        }
        xs->resultDoc->doctype->cdataSectionElements =
            (Tcl_HashTable *) MALLOC (sizeof (Tcl_HashTable));
        Tcl_InitHashTable (xs->resultDoc->doctype->cdataSectionElements,
                           TCL_STRING_KEYS);
        for (entryPtr = Tcl_FirstHashEntry (xs->doctype.cdataSectionElements,
                                            &search);
             entryPtr != (Tcl_HashEntry*) NULL;
             entryPtr = Tcl_NextHashEntry (&search)) {
            Tcl_CreateHashEntry (xs->resultDoc->doctype->cdataSectionElements,
                                 Tcl_GetHashKey (
                                     xs->doctype.cdataSectionElements, entryPtr
                                     ), &hnew);
        }
    }
    
    xs->xmlRootNode         = xmlNode;
    xs->lastNode            = xs->resultDoc->rootNode;
    xs->xsltMsgCB           = xsltMsgCB;
    xs->xsltMsgClientData   = xsltMsgClientData;


    xsltPushVarFrame(xs);
    xpathRSInit( &nodeList );
    rsAddNodeFast( &nodeList, xmlNode);

    /*  strip space from the XML document, if necessary */
    if (xs->wsInfo.hasData) {
        StripXMLSpace (xs, xmlNode);
    }

    /* add the xml doc to the doc list */
    sdoc = (xsltSubDoc*)MALLOC(sizeof (xsltSubDoc));
    sdoc->doc = xmlNode->ownerDocument;
    baseURI = findBaseURI (xmlNode);
    if (baseURI) {
        sdoc->baseURI = tdomstrdup (baseURI);
    } else {
        sdoc->baseURI = NULL;
    }
    Tcl_InitHashTable (&(sdoc->keyData), TCL_STRING_KEYS);
    sdoc->excludeNS = NULL;
    sdoc->extensionNS = NULL;
    sdoc->fwCmpProcessing = 0;
    sdoc->isStylesheet = 0;
    sdoc->mustFree = 0;
    sdoc->fixedXMLSource = 0;
    sdoc->next = xs->subDocs;
    xs->subDocs = sdoc;

    xs->currentSubDoc = sdoc;
    
    rc = processTopLevelVars (xmlNode, xs, parameters, 
                              ignoreUndeclaredParameters, errMsg);
    if (rc != 0) goto error;

    node = xs->xsltDoc->documentElement;
    rc = ApplyTemplates (xs, &nodeList, xmlNode, 0, node, &nodeList, NULL,
                         NULL, errMsg);
    if (rc != 0) goto error;

    /* Rudimentary xsl:output support */
    if (xs->doctype.method) {
        if (!xs->resultDoc->doctype) {
            xs->resultDoc->doctype = (domDocInfo*)MALLOC (sizeof (domDocInfo));
            memset (xs->resultDoc->doctype, 0, (sizeof (domDocInfo)));
        }
        xs->resultDoc->doctype->method = tdomstrdup (xs->doctype.method);
    } else {
        /* default output method */
        node = xs->resultDoc->rootNode->firstChild;
        while (node) {
            if (node->nodeType == TEXT_NODE) {
                char *pc;
                int   i, only_whites;

                only_whites = 1;
                for (i=0, pc = ((domTextNode*)node)->nodeValue;
                     i < ((domTextNode*)node)->valueLength;
                     i++, pc++) {
                    if (!IS_XML_WHITESPACE(*pc)) {
                        only_whites = 0;
                        break;
                    }
                }
                if (!only_whites) break;
            }
            if (node->nodeType == ELEMENT_NODE) {
                if (STRCASECMP(node->nodeName, "html")==0) {
                    if (!xs->resultDoc->doctype) {
                        xs->resultDoc->doctype = 
                            (domDocInfo*)MALLOC (sizeof (domDocInfo));
                        memset (xs->resultDoc->doctype, 0, 
                                (sizeof (domDocInfo)));
                    }
                    xs->resultDoc->doctype->method = tdomstrdup ("html");
                }
                break;
            }
            node = node->nextSibling;
        }
    }
    /* Make the first ELEMENT_NODE the documentElement. There could
       be text, comment or PI nodes before the first element node.
       If the root node doesn't have an element node under it's childs,
       fall back to the firstChild as documentElement. */
    domSetDocumentElement (xs->resultDoc);

    *resultDoc = xs->resultDoc;

    xsltPopVarFrame (xs);
    xpathRSFree( &nodeList );
    if (xsltCmdData) {
        xsltResetState (xs);
    } else {
        xsltFreeState (xs);
    }
    return 0;

 error:
    xsltPopVarFrame (xs);
    xpathRSFree( &nodeList );
    domFreeDocument (xs->resultDoc, NULL, NULL);
    if (xsltCmdData) {
        xsltResetState (xs);
    } else {
        xsltFreeState (xs);
    }
    return -1;

} /* xsltProcess */

