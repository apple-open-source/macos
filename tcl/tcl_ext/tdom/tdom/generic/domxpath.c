/*----------------------------------------------------------------------------
|   Copyright (c) 1999-2001 Jochen Loewer (loewerj@hotmail.com)
|-----------------------------------------------------------------------------
|
|   $Header: /usr/local/pubcvs/tdom/generic/domxpath.c,v 1.77 2004/05/26 01:49:37 rolf Exp $
|
|
|   A XPath implementation (lexer/parser/evaluator) for tDOM,
|   the DOM implementation for Tcl.
|   Based on November 16 1999 Recommendation of the W3C
|   (http://www.w3.org/TR/1999/REC-xslt-19991116)
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
|   Portions created by Jochen Loewer are Copyright (C) 1999 - 2001
|   Jochen Loewer. All Rights Reserved.
|
|   Portions created by Zoran Vasiljevic are Copyright (C) 2000-2002
|   Zoran Vasiljevic. All Rights Reserved.
|
|   Portions created by Rolf Ade are Copyright (C) 1999-2002
|   Rolf Ade. All Rights Reserved.
|
|   Contributor(s):
|       April00  Rolf Ade   Add support for following/preceding/
|                           precedingSibling axis plus several
|                           bug fixes
|
|       Aug00    Rolf Ade   Rewrite of comparisons plus several
|                           bug fixes/reports
|
|       Aug01    Rolf Ade   id(), unparsed-entity(), lang(), fixes
|       
|        2002    Rolf Ade   Namespace aware nodetests and NS wildcard
|                           expr, namespace aware variables, keys and
|                           function, made lexer utf-8 aware, node sets
|                           could now include nodes of different types,
|                           better IEEE 754 rules support, code
|                           restructured, serveral optimizations and bug
|                           fixes.
|
|   written by Jochen Loewer
|   July, 1999
|
\---------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------
|   Includes
|
\---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>
#include <dom.h>
#include <domxpath.h>
#include <domxslt.h>


/*----------------------------------------------------------------------------
|   Macros
|
\---------------------------------------------------------------------------*/
#define JDBG(x)
#define DBG(x)           
#define DDBG(x)          
#define TRACE(x)         DDBG(fprintf(stderr,(x)))
#define TRACE1(x,a)      DDBG(fprintf(stderr,(x),(a)))
#define TRACE2(x,a,b)    DDBG(fprintf(stderr,(x),(a),(b)))
#define TRACE3(x,a,b,c)  DDBG(fprintf(stderr,(x),(a),(b),(c)))

#define INITIAL_SIZE     100

#define ADD_TOKEN(t)  if ((l+1)>=allocated) {                                \
                       tokens=(XPathTokens)REALLOC((char*)tokens, 2*allocated\
                                                      *sizeof(XPathToken));  \
                          allocated = allocated * 2;                         \
                      }                                                      \
                      tokens[l].token     = (t);                             \
                      tokens[l++].pos     = i;                               \
                      tokens[l].token     = EOS;                             \
                      tokens[l].strvalue  = NULL;                            \
                      tokens[l].intvalue  = 0;                               \
                      tokens[l].realvalue = 0.0;


#define DeclProduction(name) static ast name (int *l,XPathTokens tokens,char **errMsg)

#define Production(name)  static ast name (int *l,XPathTokens tokens,char **errMsg) \
                          { char *__func = #name;                                   \
                            ast a = NULL;                                           \
                            TRACE2("\nProduction "#name": start l=%d next:%s\n",    \
                                                *l,token2str[tokens[*l].token]);

#define EndProduction       TRACE3("EndProd %s: start l=%d next:%s\n",    \
                                __func, *l,token2str[tokens[*l].token]);  \
                            DDBG(printAst(0,a);)                          \
                            return a;                                     \
                          }

#define LA                tokens[*l].token
#define LA2               tokens[*l+1].token
#define LA3               tokens[*l+2].token
/* #define Recurse(p)        rc=p(l,tokens,errMsg);if(rc==NULL)return rc;*/
#define Recurse(p)        p(l,tokens,errMsg)

#define Consume(tk)       if (tokens[*l].token == tk) {      \
                              TRACE2("Production %s:   %s consumed\n", \
                                 __func, token2str[tokens[*l].token]); \
                              (*l)++;                         \
                          } else {                            \
                              if (*errMsg==NULL) {ErrExpected(#tk);} \
                              else return a; \
                          }
#define STRVAL            tokens[(*l)-1].strvalue
#define INTVAL            tokens[(*l)-1].intvalue
#define REALVAL           tokens[(*l)-1].realvalue
#define NEWCONS           ((ast)MALLOC(sizeof(astElem)))

#define IS_STR(c,s)       (c==*(tokens[(*l)-1].strvalue))&&(strcmp(tokens[(*l)-1].strvalue,s)==0)
#define IS_FUNC(c,s)      ((*(step->strvalue)==(c)) && (strcmp((s),step->strvalue)==0))


#define ErrExpected(msg)  *errMsg = (char*)MALLOC(255);        \
                          **errMsg = '\0';                     \
                          strcpy(*errMsg, __func);             \
                          strcat(*errMsg, ": Expected " #msg); \
                          return a;

#define CHECK_RC          if (rc) return rc
#define checkRsAddNode(rs,node)    if (useFastAdd) rsAddNodeFast( rs,node); \
                                   else rsAddNode (rs,node);

/*----------------------------------------------------------------------------
|   Types for Lexer
|
\---------------------------------------------------------------------------*/
typedef enum {
    LPAR, RPAR, LBRACKET, RBRACKET, DOT, DOTDOT, ATTRIBUTEPREFIX,
    ATTRIBUTE, COMMA,  COLONCOLON, LITERAL, NSPREFIX, NSWC,
    INTNUMBER, REALNUMBER, SLASH, SLASHSLASH,
    PIPE, PLUS, MINUS, EQUAL, NOTEQ, LT, LTE, GT, GTE,
    AND, OR, MOD, DIV, MULTIPLY, FUNCTION, VARIABLE,
    FQVARIABLE, WCARDNAME, COMMENT, TEXT, PINSTR, NODE, AXISNAME, 
    EOS
} Token;

static char *token2str[] = {
    "LPAR", "RPAR", "LBRACKET", "RBRACKET", "DOT", "DOTDOT", "ATTRIBUTEPREFIX",
    "ATTRIBUTE", "COMMA", "COLONCOLON", "LITERAL", "NSPREFIX", "NSWC",
    "INTNUMBER", "REALNUMBER", "SLASH", "SLASHSLASH",
    "PIPE", "PLUS", "MINUS", "EQUAL", "NOTEQ", "LT", "LTE", "GT", "GTE",
    "AND", "OR", "MOD", "DIV", "MULTIPLY", "FUNCTION", "VARIABLE",
    "FQVARIABLE", "WCARDNAME", "COMMENT", "TEXT", "PI", "NODE", "AXISNAME",
    "EOS"
};


typedef struct {

    Token  token;
    char  *strvalue;
    int    intvalue;
    double realvalue;
    int    pos;

} XPathToken;

typedef XPathToken *XPathTokens;


/*----------------------------------------------------------------------------
|   Types for abstract syntax trees
|
\---------------------------------------------------------------------------*/
static char *astType2str[] = {
    "Int", "Real", "Mult", "Div", "Mod", "UnaryMinus", "IsNSElement",
    "IsNode", "IsComment", "IsText", "IsPI", "IsSpecificPI", "IsElement",
    "IsFQElement", "GetVar", "GetFQVar", "Literal", "ExecFunction", "Pred",
    "EvalSteps", "SelectRoot", "CombineSets", "Add", "Substract", "Less",
    "LessOrEq", "Greater", "GreaterOrEq", "Equal", "NotEqual", "And", "Or",
    "IsNSAttr", "IsAttr", "AxisAncestor", "AxisAncestorOrSelf",
    "AxisAttribute", "AxisChild",
    "AxisDescendant", "AxisDescendantOrSelf", "AxisFollowing",
    "AxisFollowingSibling", "AxisNamespace", "AxisParent",
    "AxisPreceding", "AxisPrecedingSilbing", "AxisSelf",
    "GetContextNode", "GetParentNode", "AxisDescendantOrSelfLit",
    "AxisDescendantLit", "SlashSlash",

    "CombinePath", "IsRoot", "ToParent", "ToAncestors", "FillNodeList",
    "FillWithCurrentNode",
    "ExecIdKey"
};

/*----------------------------------------------------------------------------
|   functionTag
|
\---------------------------------------------------------------------------*/
typedef enum {

    f_unknown = 1,
    f_boolean, f_ceiling, f_concat, f_contains, f_count, f_false, f_floor,
    f_generateId, f_id, f_lang, f_last, f_localName, f_name, f_namespaceUri,
    f_normalizeSpace, f_not, f_number, f_position, f_round, f_startsWith,
    f_string, f_stringLength, f_substring, f_substringAfter,
    f_substringBefore, f_sum, f_translate, f_true, f_unparsedEntityUri,
    f_fqfunction

} functionTag;

/*----------------------------------------------------------------------------
|   Prototypes / Forwards
|
\---------------------------------------------------------------------------*/
DeclProduction(OrExpr);
DeclProduction(Predicate);
DeclProduction(RelativeLocationPath);
DeclProduction(AbsoluteLocationPath);

char *xpathFuncString (xpathResultSet  *rs );
static int xpathEvalStep (ast step, domNode *ctxNode, domNode *exprContext,
                          int position, xpathResultSet *nodeList,
                          xpathCBs *cbs, xpathResultSet *result,
                          int *docOrder, char **errMsg);

static int xpathEvalPredicate (ast steps, domNode *exprContext, 
                               xpathResultSet *result, 
                               xpathResultSet *stepResult,
                               xpathCBs *cbs, int *docOrder, char **errMsg);

/*----------------------------------------------------------------------------
|   xpath result set functions
|
\---------------------------------------------------------------------------*/

void xpathRSFree ( xpathResultSet *rs ) {

    if (rs->type == xNodeSetResult) {
        if (!rs->intvalue) {
            if (rs->nodes) FREE((char*)rs->nodes);
        }
        rs->nr_nodes  = 0;
    } else 
    if (rs->type == StringResult) {
        if (rs->string) FREE((char*)rs->string);
    }
    rs->type = EmptyResult;
}
void rsPrint ( xpathResultSet *rs ) {
    int i = 0,l;  char tmp[80];
    switch (rs->type) {
        case EmptyResult:
             fprintf(stderr, "empty result \n");
             break;

        case BoolResult:
             fprintf(stderr, "boolean result: %d \n", rs->intvalue);
             break;

        case IntResult:
             fprintf(stderr, "int result: %d \n", rs->intvalue);
             break;

        case RealResult:
             fprintf(stderr, "real result: %f \n", rs->realvalue);
             break;

        case StringResult:
             fprintf(stderr, "string result: -%*s-\n", rs->string_len,
                                                       rs->string);
             break;

        case xNodeSetResult:
             if (!i) fprintf(stderr,"nodeSet result (len %d):\n",rs->nr_nodes);
             for (i=0; i<rs->nr_nodes; i++) {
                 if (rs->nodes[i]->nodeType == ELEMENT_NODE) {
                     fprintf(stderr, "%2d domNode0x%x %s ",
                             i, rs->nodes[i], rs->nodes[i]->nodeName);
                     if (rs->nodes[i]->firstChild &&
                         rs->nodes[i]->firstChild->nodeType == TEXT_NODE)
                     {
                         l = ((domTextNode*)rs->nodes[i]->firstChild)->valueLength;
                         if (l > 25) l = 25;
                         memcpy(tmp, ((domTextNode*)rs->nodes[i]->firstChild)->nodeValue, l);
                         tmp[l] = '\0';
                         fprintf(stderr, "'%s'", tmp);
                     }
                     fprintf(stderr, "\n");
                 } else
                 if (rs->nodes[i]->nodeType == TEXT_NODE) {
                     l = ((domTextNode*)rs->nodes[i])->valueLength;
                     if (l > 60) l = 60;
                     memcpy(tmp, ((domTextNode*)rs->nodes[i])->nodeValue, l);
                     tmp[l] = '\0';
                     fprintf(stderr, "%2d domNode0x%x text:'%s' \n",
                             i, rs->nodes[i], tmp);
                 } else
                 if (rs->nodes[i]->nodeType == COMMENT_NODE) {
                     l = ((domTextNode*)rs->nodes[i])->valueLength;
                     memcpy (tmp, "<!--", 4);
                     if (l > 60) l = 60;
                     memcpy(&tmp[4], ((domTextNode*)rs->nodes[i])->nodeValue, l);
                     memcpy(&tmp[4+l], "-->", 3);
                     tmp[7+l] = '\0';
                     fprintf(stderr, "%2d domNode0x%x text:'%s' \n",
                             i, rs->nodes[i], tmp);
                 } else
                 if (rs->nodes[i]->nodeType == ATTRIBUTE_NODE) {
                     fprintf(stderr, "%2d Attr %s='%*s'\n", i,
                             ((domAttrNode*)rs->nodes[i])->nodeName,
                             ((domAttrNode*)rs->nodes[i])->valueLength,
                             ((domAttrNode*)rs->nodes[i])->nodeValue);
                 }
             }
             break;
         case NaNResult: fprintf (stderr, "NaN result\n"); break;
         case InfResult: fprintf (stderr, "Inf result\n"); break;
         case NInfResult: fprintf (stderr, "-Inf result\n"); break;
        
         default:
             fprintf (stderr, "unknown result type: '%d'!!!\n", rs->type);
             break;

    }
}
void rsSetReal ( xpathResultSet *rs, double d) {

    rs->type = RealResult;
    rs->realvalue = d;
}
void rsSetNaN ( xpathResultSet *rs ) {
    rs->type = NaNResult;
}
void rsSetInf ( xpathResultSet *rs ) {
    rs->type = InfResult;
}
void rsSetNInf ( xpathResultSet *rs ) {
    rs->type = NInfResult;
}
void rsSetInt ( xpathResultSet *rs, int i) {

    rs->type = IntResult;
    rs->intvalue = i;
}
void rsSetBool ( xpathResultSet *rs, int i) {

    rs->type = BoolResult;
    rs->intvalue = (i ? 1 : 0);
}
void rsSetString ( xpathResultSet *rs, char *s) {

    rs->type = StringResult;
    if (s) {
        rs->string     = tdomstrdup(s);
        rs->string_len = strlen(s);
    } else {
        rs->string     = tdomstrdup("");
        rs->string_len = 0;
    }
    rs->nr_nodes = 0;
}

void rsAddNode ( xpathResultSet *rs, domNode *node) {

    if ((rs->type != EmptyResult) && (rs->type != xNodeSetResult)) {
        domPanic("Can not add node to non NodeSetResult xpathResultSet!");
    }
    if (rs->type == EmptyResult) {

        rs->type      = xNodeSetResult;
        rs->nodes     = (domNode**)MALLOC(INITIAL_SIZE * sizeof(domNode*));
        rs->allocated = INITIAL_SIZE;
        rs->nr_nodes  = 1;
        rs->nodes[0]  = node;

    } else {
        int insertIndex;
        int i;

        if (rs->intvalue) {
            /* we must do a copy-on-write */
            domNode **nodes;
            nodes = (domNode**)MALLOC(rs->allocated * sizeof(domNode*));
            memcpy (nodes, rs->nodes, sizeof(domNode*) * rs->nr_nodes);
            rs->nodes = nodes;
            rs->intvalue = 0;
        }

        insertIndex = rs->nr_nodes;
        for (i = rs->nr_nodes - 1; i >= 0; i--) {
            if (node == rs->nodes[i]) return;
            if (!domPrecedes (node, rs->nodes[i])) {
                break;
            }
            insertIndex--;
        }

        if ((rs->nr_nodes+1) >= rs->allocated) {
            rs->nodes = (domNode**)REALLOC((void*)rs->nodes,
                                         2 * rs->allocated * sizeof(domNode*));
            rs->allocated = rs->allocated * 2;
        }
        if (insertIndex == rs->nr_nodes) {
            rs->nodes[rs->nr_nodes++] = node;
        } else {
            for (i = rs->nr_nodes - 1; i >= insertIndex; i--) {
                rs->nodes[i+1] = rs->nodes[i];
            }
            rs->nodes[insertIndex] = node;
            rs->nr_nodes++;
        }
    }
}

void rsAddNodeFast ( xpathResultSet *rs, domNode *node) {

    if ((rs->type != EmptyResult) && (rs->type != xNodeSetResult)) {
        fprintf(stderr, "could not add node to non NodeSetResult xpathResultSet!"); return;
    }
    if (rs->type == EmptyResult) {

        rs->type      = xNodeSetResult;
        rs->nodes     = (domNode**)MALLOC(INITIAL_SIZE * sizeof(domNode*));
        rs->allocated = INITIAL_SIZE;
        rs->nr_nodes  = 1;
        rs->nodes[0]  = node;

    } else {
        if ((rs->nr_nodes+1) >= rs->allocated) {
            rs->nodes = (domNode**)REALLOC((void*)rs->nodes,
                                         2 * rs->allocated * sizeof(domNode*));
            rs->allocated = rs->allocated * 2;
        }
        rs->nodes[rs->nr_nodes++] = node;
    }
}

void rsCopy ( xpathResultSet *to, xpathResultSet *from ) {

    int i;

    to->type       = from->type;
    to->intvalue   = from->intvalue;
    if (from->type == RealResult) {
        to->realvalue  = from->realvalue;
    } else 
    if (from->type == StringResult) {
        to->string     = tdomstrdup(from->string);
        to->string_len = from->string_len;
    } else 
    if (from->type == xNodeSetResult) {
        to->nr_nodes = from->nr_nodes;
        to->nodes = (domNode**)MALLOC(from->nr_nodes * sizeof(domNode*));
        for (i=0; i<from->nr_nodes; i++)
            to->nodes[i] = from->nodes[i];
        to->intvalue = 0;
    }
}


/*----------------------------------------------------------------------------
|   AST construct functions
|
\---------------------------------------------------------------------------*/
static ast New( astType type ) {
    ast t = NEWCONS;
    t->type = type;
    t->next = t->child = NULL;
    t->strvalue  = NULL;
    t->intvalue  = 0;
    t->realvalue = 0.0;
    return t;
}
static ast New1( astType type, ast a) {
    ast t = NEWCONS;
    t->type = type;
    t->next = NULL;
    t->child = a;
    t->strvalue  = NULL;
    t->intvalue  = 0;
    t->realvalue = 0.0;
    return t;
}
static ast New1WithEvalSteps( astType type, ast a) {
    ast t = NEWCONS;
    t->type = type;
    t->next = NULL;
    if (a && a->next) {
        t->child =  New1(EvalSteps,a);
    } else {
        t->child =  a;
    }
    t->strvalue  = NULL;
    t->intvalue  = 0;
    t->realvalue = 0.0;
    return t;
}
static ast New2( astType type, ast a, ast b ) {
    ast t = NEWCONS;

    t->type      = type;
    t->next      = NULL;
    t->strvalue  = NULL;
    t->intvalue  = 0;
    t->realvalue = 0.0;

    if (a && a->next) {
        t->child =  New1(EvalSteps,a);
    } else {
        t->child =  a;
    }
    if (b && b->next) {
        t->child->next = New1(EvalSteps, b);
    } else {
        t->child->next = b;
    }
    return t;
}

static ast NewInt( int i ) {
    ast t = NEWCONS;

    t->type      = Int;
    t->strvalue  = NULL;
    t->intvalue  = i;
    t->realvalue = 0.0;

    t->next = t->child = NULL;
    return t;
}
static ast NewReal( double r ) {
    ast t = NEWCONS;

    t->type      = Real;
    t->strvalue  = NULL;
    t->intvalue  = 0;
    t->realvalue = r;

    t->next = t->child = NULL;
    return t;
}
static ast NewStr( astType type, char *str ) {
    ast t = NEWCONS;

    t->type      = type;
    t->strvalue  = tdomstrdup(str);
    t->intvalue  = 0;
    t->realvalue = 0.0;

    t->next = t->child = NULL;
    return t;
}
static ast Append( ast m, ast n ) {
    if (!n) return NULL;
    if (!m) return NULL;

    while (m->next != NULL) m = m->next;
    m->next = n;
    return m;
}
static ast AddChild( ast m, ast child ) {
    if (!child) return NULL;
    if (!m)     return NULL;

    if (m->child == NULL) {
        m->child = child;
    } else {
        ast c = m->child;
        while (c->next != NULL) c = c->next;
        c->next = child;
    }
    return m;
}
static ast AddChildWithEvalSteps( ast m, ast child ) {
    if (!child) return NULL;
    if (!m)     return NULL;

    if (child->next) {
        child =  New1(EvalSteps, child);
    }

    if (m->child == NULL) {
        m->child = child;
    } else {
        ast c = m->child;
        while (c->next != NULL) c = c->next;
        c->next = child;
    }
    /*child->next = NULL;*/
    return m;
}
static void freeAst (ast t)
{
    ast tmp;

    while (t) {
        tmp = t->next;
        if (t->strvalue) FREE(t->strvalue);
        if (t->child) freeAst (t->child);
        FREE((char*)t);
        t = tmp;
    }
}
void printAst (int depth, ast t)
{
    int i;

    while (t) {
        for (i=0; i<depth; i++) fprintf(stderr, "   ");
        fprintf(stderr, "%s ", astType2str[t->type]);
        switch (t->type) {

            case Int :        fprintf(stderr, "%d", t->intvalue);   break;
            case Real:        fprintf(stderr, "%f", t->realvalue);  break;
            case IsElement:
            case IsFQElement:
            case IsNSAttr:
            case IsAttr:
            case ExecFunction:
            case Literal:
            case GetFQVar:
            case GetVar:      fprintf(stderr, "'%s'", t->strvalue); break;

            default: break;
        }
        fprintf(stderr, "\n");
        if (t->child) printAst (depth+1, t->child);
        t = t->next;
    }
}




/*----------------------------------------------------------------------------
|   xpathFreeAst
|
\---------------------------------------------------------------------------*/
void xpathFreeAst(
    ast t
)
{
    freeAst(t);
}


/*----------------------------------------------------------------------------
|   xpathLexer
|
\---------------------------------------------------------------------------*/
static XPathTokens xpathLexer (
    char  *xpath,
    char **errMsg
)
{
    int  l, allocated;
    int  i, k, start;
    char delim, *ps, save;
    XPathTokens tokens;
    int token = EOS;


    tokens = (XPathTokens)MALLOC(INITIAL_SIZE * sizeof(XPathToken));
    if (tokens == NULL) {
        *errMsg = tdomstrdup("Unable to alloc initial memory!");
        return NULL;
    }
    allocated = INITIAL_SIZE;
    l = 0;
    tokens[l].token     = EOS;
    tokens[l].strvalue  = NULL;
    tokens[l].intvalue  = 0;
    tokens[l].realvalue = 0.0;

    i = 0;
    while (xpath[i]) {
        switch (xpath[i]) {

            case ' ' :
            case '\n':
            case '\r':
            case '\t': i++; continue;

            case '(':  token = LPAR;     break;
            case ')':  token = RPAR;     break;
            case '[':  token = LBRACKET; break;
            case ']':  token = RBRACKET; break;

            case '@':  i++;
                       if ( isNCNameStart (&xpath[i]) ) {
                           ps = &(xpath[i]);
                           i += UTF8_CHAR_LEN (xpath[i]);
                           while (xpath[i] && isNCNameChar (&xpath[i]))
                               i += UTF8_CHAR_LEN (xpath[i]);
                           save = xpath[i];
                           xpath[i] = '\0';
                           tokens[l].strvalue = tdomstrdup(ps);
                           xpath[i] = save;
                           if (save == ':' && xpath[i+1] != ':') {
                               token = ATTRIBUTEPREFIX;
                               ADD_TOKEN (token);
                               if (xpath[i+1] == '*') {
                                   token = ATTRIBUTE;
                                   tokens[l].strvalue = tdomstrdup("*");
                                   i++;
                               } else {
                                   ps = &(xpath[++i]);
                                   if (!(isNCNameStart (&xpath[i]))) {
                                       *errMsg = tdomstrdup ("Illegal attribute name");
                                       return tokens;
                                   }
                                   i += UTF8_CHAR_LEN (xpath[i]);
                                   while (xpath[i] && isNCNameChar (&xpath[i]))
                                       i += UTF8_CHAR_LEN (xpath[i]);
                                   save = xpath[i];
                                   xpath[i] = '\0';
                                   token = ATTRIBUTE;
                                   tokens[l].strvalue = tdomstrdup(ps);
                                   xpath[i--] = save;
                               }
                           } else {
                               token = ATTRIBUTE;
                               i--;
                           }
                       } else if (xpath[i]=='*') {
                           tokens[l].strvalue = tdomstrdup("*");
                           token = ATTRIBUTE;
                       } else {
                           *errMsg = tdomstrdup("Expected attribute name");
                           return tokens;
                       }; break;

            case ',':  token = COMMA; break;

            case ':':  if (xpath[i+1] == ':') {
                           token = COLONCOLON;
                           i++;
                       } else {
                           *errMsg = tdomstrdup("Unexpected token ':'");
                           return tokens;
                       }; break;

            case '"' :
            case '\'': delim = xpath[i]; start = ++i;
                       while (xpath[i] && (xpath[i] != delim)) i++;
                       if (!xpath[i]) {
                           *errMsg = tdomstrdup("Undetermined string");
                           return tokens;
                       }
                       xpath[i] = '\0'; /* terminate string */
                       tokens[l].strvalue = tdomstrdup(&xpath[start]);
                       token = LITERAL;
                       xpath[i] = delim;
                       break;

            case '/':  if (xpath[i+1] == '/') {
                           token = SLASHSLASH;
                           i++;
                       } else {
                           token = SLASH;
                       };
                       break;

            case '|':  token = PIPE;     break;
            case '+':  token = PLUS;     break;
            case '-':  token = MINUS;    break;
            case '=':  token = EQUAL;    break;
            case '!':  if (xpath[i+1] == '=') {
                           token = NOTEQ;
                           i++;
                       } else {
                           *errMsg = tdomstrdup("Unexpected token '!'");
                           return tokens;
                       }; break;

            case '<':  if (xpath[i+1] == '=') {
                           token = LTE;
                           i++;
                       } else {
                           token = LT;
                       };break;

            case '>':  if (xpath[i+1] == '=') {
                           token = GTE;
                           i++;
                       } else {
                           token = GT;
                       }; break;


            case '*':  if ((l>0)
                          && (tokens[l-1].token != COLONCOLON)
                          && (tokens[l-1].token != LPAR)
                          && (tokens[l-1].token != LBRACKET)
                          && (tokens[l-1].token != COMMA)
                          && (tokens[l-1].token != SLASH)
                          && (tokens[l-1].token != SLASHSLASH)
                        ) {
                           token = MULTIPLY;
                       } else {
                           token = WCARDNAME;
                           tokens[l].strvalue = tdomstrdup("*");
                       }; break;

            case '$':  i++;
                       if ( isNCNameStart (&xpath[i])) {
                           ps = &(xpath[i]);
                           i += UTF8_CHAR_LEN (xpath[i]);
                           while (xpath[i] && isNCNameChar(&xpath[i]))
                               i +=  UTF8_CHAR_LEN(xpath[i]);
                           if (xpath[i] == ':' && xpath[i+1] != ':') {
                               token = FQVARIABLE;
                               if (!isNCNameStart (&xpath[++i])) {
                                   *errMsg = tdomstrdup ("Illegal variable name");
                                   return tokens;
                               }
                               i += UTF8_CHAR_LEN (xpath[i]);
                               while (xpath[i] && isNCNameChar (&xpath[i]))
                                   i += UTF8_CHAR_LEN (xpath[i]);
                           } else {
                               token = VARIABLE;
                           }
                           save = xpath[i];
                           xpath[i] = '\0';
                           tokens[l].strvalue = tdomstrdup(ps);
                           xpath[i--] = save;
                       } else {
                           *errMsg = tdomstrdup("Expected variable name");
                           return tokens;
                       }; break;

            case '.':  if (xpath[i+1] == '.') {
                           token = DOTDOT;
                           i++;
                           break;
                       } else if (!isdigit(xpath[i+1])) {
                           token = DOT;
                           break;
                       }
                       /* DOT followed by digit, ie a REAL.
                          Handled by default. Fall throu */

            default:   if ( isNCNameStart (&xpath[i])) {
                           ps = &(xpath[i]);
                           i += UTF8_CHAR_LEN (xpath[i]);
                           while (xpath[i] && isNCNameChar(&xpath[i])) {
                               i += UTF8_CHAR_LEN(xpath[i]);
                           }

                           k = i;
                           if (xpath[i] == ':') {
                               if (xpath[i+1] == '*') {
                                   save = xpath[i];
                                   xpath[i] = '\0'; /* terminate */
                                   token = NSWC;
                                   tokens[l].strvalue = tdomstrdup (ps);
                                   xpath[i] = save;
                                   i++;
                                   break;
                               }
                               if (xpath[i+1] != ':') {
                                   save = xpath[i];
                                   xpath[i] = '\0'; /* terminate */
                                   token = NSPREFIX;
                                   tokens[l].strvalue = tdomstrdup (ps);
                                   xpath[i] = save;
                                   ADD_TOKEN (token);
                                   ps = &(xpath[++i]);
                                   if (!(isNCNameStart (&xpath[i]))) {
                                       *errMsg = tdomstrdup ("Illegal character in localname");
                                       return tokens;
                                   }
                                   i += UTF8_CHAR_LEN (xpath[i]);
                                   while (xpath[i] && isNCNameChar (&xpath[i]))
                                       i += UTF8_CHAR_LEN (xpath[i]);
                                   k = i;
                               }
                           }
                           /* read over white space */
                           while ((xpath[k] == ' ')  ||
                                  (xpath[k] == '\n') ||
                                  (xpath[k] == '\r') ||
                                  (xpath[k] == '\t')   )  k++;

                           if (l>0 && tokens[l-1].token == NSPREFIX) {
                               if (xpath[k]!='(') token = WCARDNAME;
                               else               token = FUNCTION;
                               save = xpath[i];
                               xpath[i] = '\0';
                               tokens[l].strvalue = tdomstrdup(ps);
                               xpath[i--] = save;
                               break;
                           }
                               
                           if (xpath[k]=='(') {
                               save = xpath[i];
                               xpath[i] = '\0'; /* terminate */
                               if (strcmp(ps,"text")==0) {
                                   token = TEXT;
                               } else if (strcmp(ps,"node")==0) {
                                   token = NODE;
                               } else if (strcmp(ps,"comment")==0) {
                                   token = COMMENT;
                               } else if (strcmp(ps,"processing-instruction")==0) {
                                   token = PINSTR;
                               } else {
                                   if ((save!='(') && (strcmp(ps,"and")==0)) token = AND;
                                   else
                                   if ((save!='(') && (strcmp(ps,"or")==0))  token = OR;
                                   else
                                   if ((save!='(') && (strcmp(ps,"mod")==0)) token = MOD;
                                   else
                                   if ((save!='(') && (strcmp(ps,"div")==0)) token = DIV;
                                   else {
                                       token = FUNCTION;
                                       tokens[l].strvalue = tdomstrdup(ps);
                                   }
                               }
                               xpath[i] = save;
                           } else if ((xpath[k]==':') && (xpath[k+1]==':')) {
                               token = AXISNAME;
                               save = xpath[i];
                               xpath[i] = '\0'; /* terminate */
                               tokens[l].strvalue = tdomstrdup(ps);
                               xpath[i] = save;
                           } else {
                               save = xpath[i];
                               xpath[i] = '\0';
                               if ((l>0)
                                   && (tokens[l-1].token != COLONCOLON)
                                   && (tokens[l-1].token != LPAR)
                                   && (tokens[l-1].token != LBRACKET)
                                   && (tokens[l-1].token != COMMA)
                                   && (tokens[l-1].token != SLASH)
                                   && (tokens[l-1].token != SLASHSLASH)
                                   && (tokens[l-1].token != PIPE)
                                   && (tokens[l-1].token != PLUS)
                                   && (tokens[l-1].token != MINUS)
                                   && (tokens[l-1].token != EQUAL)
                                   && (tokens[l-1].token != NOTEQ)
                                   && (tokens[l-1].token != LT)
                                   && (tokens[l-1].token != LTE)
                                   && (tokens[l-1].token != GT)
                                   && (tokens[l-1].token != GTE)
                                   && (tokens[l-1].token != AND)
                                   && (tokens[l-1].token != OR)
                                   && (tokens[l-1].token != MOD)
                                   && (tokens[l-1].token != DIV)
                                   && (tokens[l-1].token != MULTIPLY)
                               ) {
                                   if (strcmp(ps,"and")==0) {
                                       token = AND;
                                   } else if (strcmp(ps,"or")==0) {
                                       token = OR;
                                   } else if (strcmp(ps,"mod")==0) {
                                       token = MOD;
                                   } else if (strcmp(ps,"div")==0) {
                                       token = DIV;
                                   } else {
                                       token = WCARDNAME;
                                       tokens[l].strvalue = tdomstrdup(ps);
                                   }
                               } else {
                                   token = WCARDNAME;
                                   tokens[l].strvalue = tdomstrdup(ps);
                               }
                               xpath[i] = save;
                           }
                           i--;
                       } else if (isdigit(xpath[i]) || (xpath[i] == '.')) {
                           if (xpath[i] == '.') {
                               token = REALNUMBER;
                           } else {
                               token = INTNUMBER;
                           }
                           ps = &(xpath[i++]);
                           while (xpath[i] && isdigit(xpath[i]))  i++;
                           if (xpath[i]=='.') {
                               token = REALNUMBER;
                               i++;
                               while (xpath[i] && isdigit(xpath[i]))  i++;
                           }
                           save = xpath[i];
                           xpath[i] = '\0';
                           if (token == INTNUMBER) {
                               tokens[l].intvalue = atoi(ps);
                           }
                           tokens[l].realvalue = (double)atof(ps);
                           xpath[i--] = save;
                       };
                       break;

        } /* switch */
        ADD_TOKEN(token);
        i++;
    }
    ADD_TOKEN(EOS);
    return tokens;

} /* xpathLexer */



/*-----------------------------------------------------------------
|   NodeTest  production
|
\----------------------------------------------------------------*/
Production(NodeTest)

    if (LA==NODE) {
        Consume(NODE);
        Consume(LPAR);
        Consume(RPAR);
        a = New (IsNode);
    } else
    if (LA==TEXT) {
        Consume(TEXT);
        Consume(LPAR);
        Consume(RPAR);
        a = New (IsText);
    } else
    if (LA==COMMENT) {
        Consume(COMMENT);
        Consume(LPAR);
        Consume(RPAR);
        a = New (IsComment);
    } else
    if (LA==PINSTR) {
        Consume(PINSTR);
        Consume(LPAR);
        if (LA==LITERAL) {
            Consume(LITERAL);
            a = NewStr (IsSpecificPI, STRVAL);
        } else {
            a = New (IsPI);
        }
        Consume(RPAR);
    } else
    if (LA==MULTIPLY) {
        Consume(MULTIPLY);
        a = NewStr (IsElement, "*");
    } else
    if (LA==NSPREFIX) {
        ast b;
        Consume (NSPREFIX);
        a = NewStr (IsFQElement, STRVAL);
        Consume (WCARDNAME);
        b = NewStr (IsElement, STRVAL);
        AddChild (a, b);
    } else
    if (LA==NSWC) {
        Consume (NSWC);
        a = NewStr (IsNSElement, STRVAL);
    } else {
        Consume(WCARDNAME);
        a = NewStr (IsElement, STRVAL);
    }
EndProduction


/*-----------------------------------------------------------------
|   AbbreviatedBasis  production
|
\----------------------------------------------------------------*/
Production(AbbreviatedBasis)
    if (LA==ATTRIBUTE) {
        Consume(ATTRIBUTE);
        a = New1( AxisAttribute, NewStr(IsAttr, STRVAL) );
    } else
    if (LA==ATTRIBUTEPREFIX) {
        ast b, c;
        Consume(ATTRIBUTEPREFIX);
        a = New (AxisAttribute);
        b = NewStr (IsNSAttr, STRVAL);
        AddChild (a, b);
        Consume(ATTRIBUTE);
        c = NewStr (IsAttr, STRVAL);
        AddChild (b, c);
    }
    else {
        a = New1( AxisChild, Recurse(NodeTest));
    }
EndProduction

/*-----------------------------------------------------------------
|   getFunctionTag
|
\----------------------------------------------------------------*/
static functionTag
getFunctionTag (char *funcName)
{
    switch (funcName[0]) {
    case 'b':
        if (strcmp (funcName, "boolean")==0) return f_boolean;
        break;
    case 'c':
        if (strcmp (funcName, "ceiling")==0) return f_ceiling;
        else if (strcmp (funcName, "concat")==0) return f_concat;
        else if (strcmp (funcName, "contains")==0) return f_contains;
        else if (strcmp (funcName, "count")==0) return f_count;
        break;
    case 'f':
        if (strcmp (funcName, "false")==0) return f_false;
        else if (strcmp (funcName, "floor")==0) return f_floor;
        break;
    case 'g':
        if (strcmp (funcName, "generate-id")==0) return f_generateId;
        break;
    case 'i':
        if (strcmp (funcName, "id")==0) return f_id;
    case 'l':
        if (strcmp (funcName, "lang")==0) return f_lang;
        else if (strcmp (funcName, "last")==0) return f_last;
        else if (strcmp (funcName, "local-name")==0) return f_localName;
        break;
    case 'n':
        if (strcmp (funcName, "name")==0) return f_name;
        else if (strcmp (funcName, "namespace-uri")==0) return f_namespaceUri;
        else if (strcmp (funcName, "normalize-space")==0) return f_normalizeSpace;
        else if (strcmp (funcName, "not")==0) return f_not;
        else if (strcmp (funcName, "number")==0) return f_number;
        break;
    case 'p':
        if (strcmp (funcName, "position")==0) return f_position;
        break;
    case 'r':
        if (strcmp (funcName, "round")==0) return f_round;
        break;
    case 's':
        if (strcmp (funcName, "starts-with")==0) return f_startsWith;
        else if (strcmp (funcName, "string")==0) return f_string;
        else if (strcmp (funcName, "string-length")==0) return f_stringLength;
        else if (strcmp (funcName, "substring")==0) return f_substring;
        else if (strcmp (funcName, "substring-after")==0) return f_substringAfter;
        else if (strcmp (funcName, "substring-before")==0) return f_substringBefore;
        else if (strcmp (funcName, "sum")==0) return f_sum;
        break;
    case 't':
        if (strcmp (funcName, "translate")==0) return f_translate;
        else if (strcmp (funcName, "true")==0) return f_true;
        break;
    case 'u':
        if (strcmp (funcName, "unparsed-entity-uri")==0) return f_unparsedEntityUri;
        break;
    default:
        break;
    }
    return f_unknown;
}

/*-----------------------------------------------------------------
|   FilterExpr  production
|
\----------------------------------------------------------------*/
Production(FilterExpr)

    if (LA==VARIABLE) {
        Consume(VARIABLE);
        a = NewStr( GetVar, STRVAL);

    } else if (LA==FQVARIABLE) {
        Consume(FQVARIABLE);
        a = NewStr( GetFQVar, STRVAL);

    } else if (LA==LPAR) {
        Consume(LPAR);
        a = New1(EvalSteps, Recurse(OrExpr));
        Consume(RPAR);

    } else if (LA==LITERAL) {
        Consume(LITERAL);
        a = NewStr( Literal, STRVAL);

    } else if (LA==INTNUMBER) {
        Consume(INTNUMBER);
        a = NewInt( INTVAL );

    } else if (LA==REALNUMBER) {
        Consume(REALNUMBER);
        a = NewReal( REALVAL );

    } else if (LA==FUNCTION || LA==NSPREFIX) {
        if (LA==FUNCTION) {
            Consume(FUNCTION);
            a = NewStr( ExecFunction, STRVAL);
            a->intvalue = getFunctionTag (STRVAL);
        } else {
            Consume(NSPREFIX);
            a = NewStr( ExecFunction, STRVAL);
            a->intvalue = f_fqfunction;
            Consume(FUNCTION);
            AddChild (a, NewStr( ExecFunction, STRVAL));
        }
        Consume(LPAR);
        if (LA!=RPAR) {
            AddChildWithEvalSteps (a, Recurse(OrExpr));
            while(LA==COMMA) {
                Consume(COMMA);
                AddChildWithEvalSteps  (a, Recurse(OrExpr) );
            }
        }
        Consume(RPAR);
    } else {
        ErrExpected("$var or (expr) or literal or number or func");
    }
    while (LA==LBRACKET) {
        ast b;
        b = Recurse(Predicate);
        if (!b) return NULL;
        Append( a, New1WithEvalSteps( Pred, b));
    }
EndProduction


/*-----------------------------------------------------------------
|   PathExpr  production
|
\----------------------------------------------------------------*/
Production(PathExpr)

    if ( (LA==VARIABLE)
       ||(LA==FQVARIABLE)
       ||(LA==LPAR)
       ||(LA==LITERAL)
       ||(LA==INTNUMBER)
       ||(LA==REALNUMBER)
       ||(LA==FUNCTION)
       ||(LA==NSPREFIX && LA2==FUNCTION)  
    ) {
        a = Recurse(FilterExpr);
        if (LA==SLASH) {
            Consume(SLASH);
            Append(a, Recurse(RelativeLocationPath));

        } else if (LA==SLASHSLASH) {
            ast b;
            Consume(SLASHSLASH);
            b = Recurse(RelativeLocationPath);
            if (b->type == AxisChild) {
                b->type = AxisDescendant;
            } else {
                Append(a, New( AxisDescendantOrSelf ) );
            }
            Append(a, b );
        }

    } else {
        if ( (LA==SLASH) || (LA==SLASHSLASH)) {
            return Recurse(AbsoluteLocationPath);
        } else {
            return Recurse(RelativeLocationPath);
        }
    }

EndProduction


/*-----------------------------------------------------------------
|   UnionExpr  production
|
\----------------------------------------------------------------*/
Production(UnionExpr)

    a = Recurse(PathExpr);
    while (LA==PIPE) {
        Consume(PIPE);
        a = New2( CombineSets, a, Recurse(PathExpr));
    }

EndProduction


/*-----------------------------------------------------------------
|   UnaryExpr  production
|
\----------------------------------------------------------------*/
Production(UnaryExpr)

    if (LA==MINUS) {
        Consume(MINUS);
        a = Recurse(UnionExpr);
        if ((a->type == Int) && (a->next == NULL)) {
            a->intvalue = a->intvalue * -1;
        } else
        if ((a->type == Real) && (a->next == NULL)) {
            a->realvalue = a->realvalue * -1;
        } else {
            a = New1( UnaryMinus, a);
        }

    } else {
        a = Recurse(UnionExpr);
    }
EndProduction


/*-----------------------------------------------------------------
|   MultiplicativeExpr  production
|
\----------------------------------------------------------------*/
Production(MultiplicativeExpr)

    a = Recurse(UnaryExpr);
    while ( (LA==MULTIPLY)
          ||(LA==DIV)
          ||(LA==MOD)
    ) {
        if (LA==MULTIPLY) {
            Consume(MULTIPLY);
            a = New2( Mult, a, Recurse(UnaryExpr));
        } else if (LA==DIV) {
            Consume(DIV);
            a = New2( Div, a, Recurse(UnaryExpr));
        } else {
            Consume(MOD);
            a = New2( Mod, a, Recurse(UnaryExpr));
        }
    }
EndProduction


/*-----------------------------------------------------------------
|   AdditiveExpr  production
|
\----------------------------------------------------------------*/
Production(AdditiveExpr)

    a = Recurse(MultiplicativeExpr);
    while ( (LA==PLUS)
          ||(LA==MINUS)
    ) {
        if (LA==PLUS) {
            Consume(PLUS);
            a = New2( Add, a, Recurse(MultiplicativeExpr));
        } else {
            Consume(MINUS);
            a = New2( Substract, a, Recurse(MultiplicativeExpr));
        }
    }
EndProduction


/*-----------------------------------------------------------------
|   RelationalExpr  production
|
\----------------------------------------------------------------*/
Production(RelationalExpr)

    a = Recurse(AdditiveExpr);
    while ( (LA==LT)
          ||(LA==LTE)
          ||(LA==GT)
          ||(LA==GTE)
    ) {
        if (LA==LT) {
            Consume(LT);
            a = New2( Less, a, Recurse(AdditiveExpr));
        } else if (LA==LTE) {
            Consume(LTE);
            a = New2( LessOrEq, a, Recurse(AdditiveExpr));
        } else if (LA==GT) {
            Consume(GT);
            a = New2( Greater, a, Recurse(AdditiveExpr));
        } else {
            Consume(GTE);
            a = New2( GreaterOrEq, a, Recurse(AdditiveExpr));
        }
    }
EndProduction


/*-----------------------------------------------------------------
|   EqualityExpr  production
|
\----------------------------------------------------------------*/
Production(EqualityExpr)

    a = Recurse(RelationalExpr);
    while ( (LA==EQUAL)
          ||(LA==NOTEQ)
    ) {
        if (LA==EQUAL) {
            Consume(EQUAL);
            a = New2( Equal, a, Recurse(RelationalExpr));
        } else {
            Consume(NOTEQ);
            a = New2( NotEqual, a, Recurse(RelationalExpr));
        }
    }
EndProduction


/*-----------------------------------------------------------------
|   AndExpr  production
|
\----------------------------------------------------------------*/
Production(AndExpr)

    a = Recurse(EqualityExpr);
    while (LA==AND) {
        Consume(AND);
        a = New2( And, a, Recurse(EqualityExpr));
    }
EndProduction


/*-----------------------------------------------------------------
|   OrExpr  production
|
\----------------------------------------------------------------*/
Production(OrExpr)

    a = Recurse(AndExpr);
    while (LA==OR) {
        Consume(OR);
        a = New2( Or, a, Recurse(AndExpr));
    }
EndProduction


/*-----------------------------------------------------------------
|   Predicate  production
|
\----------------------------------------------------------------*/
Production(Predicate)

    Consume(LBRACKET);
    /*a = Recurse(PredicateExpr);*/
    a = Recurse(OrExpr);
    Consume(RBRACKET);

EndProduction


/*-----------------------------------------------------------------
|   Basis  production
|
\----------------------------------------------------------------*/
Production(Basis)

    if (LA==AXISNAME) {
        astType t;
        Consume(AXISNAME);
        if        (IS_STR('c',"child"))              { t = AxisChild;
        } else if (IS_STR('d',"descendant"))         { t = AxisDescendantLit;
        } else if (IS_STR('d',"descendant-or-self")) { t = AxisDescendantOrSelfLit;
        } else if (IS_STR('s',"self"))               { t = AxisSelf;
        } else if (IS_STR('a',"attribute"))          { t = AxisAttribute;
        } else if (IS_STR('a',"ancestor"))           { t = AxisAncestor;
        } else if (IS_STR('a',"ancestor-or-self"))   { t = AxisAncestorOrSelf;
        } else if (IS_STR('f',"following"))          { t = AxisFollowing;
        } else if (IS_STR('f',"following-sibling"))  { t = AxisFollowingSibling;
        } else if (IS_STR('n',"namespace"))          { t = AxisNamespace;
        } else if (IS_STR('p',"parent"))             { t = AxisParent;
        } else if (IS_STR('p',"preceding"))          { t = AxisPreceding;
        } else if (IS_STR('p',"preceding-sibling"))  { t = AxisPrecedingSibling;
        } else {
            ErrExpected("correct axis name");
        }
        a = New( t );
        Consume(COLONCOLON);
        AddChild( a, Recurse(NodeTest));
    } else {
        a = Recurse(AbbreviatedBasis);
    }
EndProduction


/*-----------------------------------------------------------------
|   Step  production
|
\----------------------------------------------------------------*/
static int IsStepPredOptimizable (ast a) {
    ast b;
    int left;
    
    DBG (
        fprintf (stderr, "IsStepPredOptimizable:\n");
        printAst (0,a);
    )
    switch (a->type) {
    case Int: return a->intvalue;
    case Less:
    case LessOrEq:
        b = a->child;
        if (b->type != ExecFunction 
            || b->intvalue != f_position) return 0;
        b = b->next;
        if (b->type != Int) return 0;
        if (a->type == Less) return b->intvalue;
        else                 return b->intvalue + 1;
    case Greater:
    case GreaterOrEq:
        b = a->child;
        if (b->type != Int) return 0;
        left = b->intvalue;
        b = b->next;
        if (b->type != ExecFunction 
            || b->intvalue != f_position) return 0;
        if (a->type == Greater) return left;
        else                    return left + 1;
    case Equal:
        b = a->child;
        if (b->type == Int
            && b->next->type == ExecFunction
            && b->next->intvalue == f_position) return b->intvalue;
        if (b->type == ExecFunction
            && b->intvalue == f_position
            && b->next->type == Int) return b->next->intvalue;
        return 0;
    default: return 0;
    }
}

Production(Step)

    if (LA==DOT) {
        Consume(DOT);
        a = New( GetContextNode );
        /* a = New1( AxisSelf, New (IsNode)); */

    } else if (LA==DOTDOT) {
        Consume(DOTDOT);
        a = New( GetParentNode );
        /* a = New1( AxisParent, New (IsNode)); */

    } else {
        ast b;
        int isFirst = 1;
        a = Recurse(Basis);
        while (LA==LBRACKET) {
            b = Recurse (Predicate);
            if (!b) return NULL;
            if (isFirst) {
                a->intvalue = IsStepPredOptimizable (b);
                DBG (fprintf (stderr, "step type %s, intvalue: %d\n", astType2str[a->type], a->intvalue);)
                isFirst = 0;
            }
            Append( a, New1WithEvalSteps( Pred, b));
        }
    }
EndProduction


/*-----------------------------------------------------------------
|   RelativeLocationPath  production
|
\----------------------------------------------------------------*/
Production(RelativeLocationPath)

    a = Recurse(Step);
    while ((LA==SLASH) || (LA==SLASHSLASH)) {
        if (LA==SLASH) {
            Consume(SLASH);
            Append(a, Recurse(Step));
        } else {
            ast b;
            Consume(SLASHSLASH);
            b = Recurse(Step);
            if (b->type == AxisChild) {
                b->type = AxisDescendant;
            } else {
                Append(a, New( AxisDescendantOrSelf ) );
            }
            Append(a, b );
        }
    }
EndProduction


/*-----------------------------------------------------------------
|   AbsoluteLocationPath  production
|
\----------------------------------------------------------------*/
Production(AbsoluteLocationPath)

    if (LA==SLASH) {
        ast b;

        Consume(SLASH);
        a = New(SelectRoot);
        if ( (LA==AXISNAME)
           ||(LA==WCARDNAME)
           ||(LA==NSPREFIX)
           ||(LA==NSWC)
           ||(LA==NODE)
           ||(LA==TEXT)
           ||(LA==COMMENT)
           ||(LA==PINSTR)
           ||(LA==DOT)
           ||(LA==DOTDOT)
           ||(LA==ATTRIBUTE)
           ||(LA==ATTRIBUTEPREFIX)
        ) {
            b =  Recurse(RelativeLocationPath);
            Append(a, b);
        }

    } else if (LA==SLASHSLASH) {
        ast b;

        Consume(SLASHSLASH);
        a = New(SelectRoot);

        b = Recurse(RelativeLocationPath);
        if (b->type == AxisChild) {
            b->type = AxisDescendant;
        } else {
            Append(a, New( AxisDescendantOrSelf ) );
        }

        Append(a, b );

    } else {
        ErrExpected("/ or //");
    }
EndProduction


/*-----------------------------------------------------------------
|   XSLT StepPattern  production
|
\----------------------------------------------------------------*/
static int usesPositionInformation ( ast a) {

    while (a) {
        if (a->type == ExecFunction 
            && (a->intvalue == f_position
                || a->intvalue == f_last
                || a->intvalue == f_unknown)) {
            return 1;
        }
        if (a->child) {
            if (usesPositionInformation (a->child)) return 1;
        }
        a = a->next;
    }
    return 0;
}

static int checkStepPatternPredOptimizability ( ast a , int *max) {
    ast b;

    switch (a->type) {
        case Literal:
        case AxisAncestor:
        case AxisAncestorOrSelf:
        case AxisChild:
        case AxisAttribute:
        case AxisDescendant:
        case AxisDescendantLit:
        case AxisDescendantOrSelf:
        case AxisDescendantOrSelfLit:
        case AxisFollowing:
        case AxisFollowingSibling:
        case AxisNamespace:
        case AxisParent:
        case AxisPreceding:
        case AxisPrecedingSibling:
        case AxisSelf:
        case IsNode:
        case IsComment:
        case IsText:
        case IsPI:
        case IsSpecificPI:
        case IsElement:
        case GetContextNode: 
        case GetParentNode:
            break;
            
        case And:
        case Or:
            if (usesPositionInformation(a->child)) return 0;
            return 1;
        case Less:
        case LessOrEq:
            b = a->child;
            if (b->type == ExecFunction
                && b->intvalue == f_position
                && b->next->type == Int) {
                if (a->type == Less) *max = b->next->intvalue;
                else *max = b->next->intvalue + 1;
                return 0;
            }
            if (usesPositionInformation(a->child)) return 0;
            return 1;            
        case Greater:
        case GreaterOrEq:
            b = a->child;
            if (b->type == Int 
                && b->next->type == ExecFunction
                && b->next->intvalue == f_position) {
                if (a->type == Greater) *max = b->intvalue;
                else *max = b->intvalue + 1;
                return 0;
            }
            if (usesPositionInformation(a->child)) return 0;
            return 1;            
        case Equal:
            b = a->child;
            if (b->type == Int
                && b->next->type == ExecFunction
                && b->next->intvalue == f_position) {
                *max = b->intvalue;
                return 0;
            }
            if (b->type == ExecFunction 
                && b->intvalue == f_position
                && b->next->type == Int) {
                *max = b->next->intvalue;
                return 0;
            }
            if (usesPositionInformation(a->child)) return 0;
            return 1;
        case NotEqual:
            if (usesPositionInformation(a->child)) return 0;
            return 1;

        case Int:
            *max = a->intvalue;
            return 0;

        case ExecFunction:
            return !usesPositionInformation(a);
            
        default: 
            return 0;
    }
    return 1;
}

static int IsStepPatternPredOptimizable ( ast a, int *max ) {
    int f;

    *max = 0;
    f = checkStepPatternPredOptimizability(a, max);
    DBG(
        if (f) {
            fprintf(stderr, "\nStepPattern Pred is optimizable:\n");
        } else {
            fprintf(stderr, "\nStepPattern Pred is not optimizable, max = %d:\n", *max);
        }
        printAst(0,a);
    )
    return f;
}


/*-----------------------------------------------------------------
|   XSLT StepPattern  production
|
\----------------------------------------------------------------*/
Production(StepPattern)

    if (LA==AXISNAME) {
        astType t;
        Consume(AXISNAME);
        if        (IS_STR('c',"child"))      { t = AxisChild;
        } else if (IS_STR('a',"attribute"))  { t = AxisAttribute;
        } else {
            ErrExpected("correct axis name (child/attribute)");
        }
        Consume(COLONCOLON);
        a = New1( t, Recurse(NodeTest));

    } else
    if (LA==ATTRIBUTE) {
        Consume(ATTRIBUTE);
        a = New1( AxisAttribute, NewStr(IsAttr, STRVAL) );
    } else
    if (LA==ATTRIBUTEPREFIX) {
        ast b, c;
        Consume(ATTRIBUTEPREFIX);
        a = New ( AxisAttribute);
        b = NewStr (IsNSAttr, STRVAL);
        AddChild (a, b);
        Consume(ATTRIBUTE);
        c = NewStr (IsAttr, STRVAL);
        AddChild (b, c);
    } else {
        a = Recurse(NodeTest);
    }
    {
        ast b, c;
        int stepIsOptimizable = 1, isFirst = 1, max, savedmax;
        while (LA==LBRACKET) {
            b = Recurse (Predicate);
            if (!b) return NULL;
            if (stepIsOptimizable) {
                if (!IsStepPatternPredOptimizable(b, &max)) 
                    stepIsOptimizable = 0;
            }
            if (isFirst) {
                savedmax = max;
                c = New1WithEvalSteps( Pred, b);
                isFirst = 0;
            } else {
                Append (c, New1WithEvalSteps( Pred, b));
            }
        }
        if (!isFirst) {
            if (stepIsOptimizable) {
                Append (a, New (FillWithCurrentNode));
            } else {
                /* copy the step before the Predicate */
                ast aCopy = NEWCONS;
                aCopy->type      = a->type;
                aCopy->next      = NULL;
                if (a->strvalue) aCopy->strvalue = tdomstrdup(a->strvalue);
                else             aCopy->strvalue = NULL;
                aCopy->intvalue  = a->intvalue;
                aCopy->realvalue = a->realvalue;
                aCopy->child     = NULL;
                if (a->child) {
                    ast aCopyChild = NEWCONS;
                    aCopyChild->type      = a->child->type;
                    aCopyChild->next      = NULL;
                    aCopyChild->child     = NULL;
                    if (a->child->strvalue) {
                        aCopyChild->strvalue  = tdomstrdup(a->child->strvalue);
                    } else {
                        aCopyChild->strvalue  = NULL;
                    }
                    aCopyChild->intvalue  = a->child->intvalue;
                    aCopyChild->realvalue = a->child->realvalue;
                    aCopy->child = aCopyChild;
                }
                b = New1( FillNodeList, aCopy);
                b->intvalue = savedmax;
                Append( a, b);
            }
            Append (a, c);
        }
    }

EndProduction


/*-----------------------------------------------------------------
|   XSLT RelativePathPattern  production
|
\----------------------------------------------------------------*/
Production(RelativePathPattern)

    a = Recurse(StepPattern);
    while ((LA==SLASH) || (LA==SLASHSLASH)) {
        ast b;
        if (LA==SLASH) {
            Consume(SLASH);
            b = Recurse(StepPattern);
            Append(b, New(ToParent) );
            Append(b, a);
            a = b;
        } else {
            Consume(SLASHSLASH);
            b = Recurse(StepPattern);
            Append(b, New(ToAncestors) );
            Append(b, a);
            a = b;
        }
    }

EndProduction


/*-----------------------------------------------------------------
|   XSLT IdKeyPattern  production
|
\----------------------------------------------------------------*/
Production(IdKeyPattern)

    Consume(FUNCTION);
    if (strcmp(STRVAL, "id" )==0) {
        ast b;
        /* id */
        a = NewStr( ExecIdKey, STRVAL);
        a->intvalue = f_id;
        Consume(LPAR);
        Consume(LITERAL);
        b = NewStr( Literal, STRVAL);
        AddChild (a, b);
        Consume(RPAR);
    } else {
        ast b;
        /* key */
        a = NewStr( ExecIdKey, STRVAL);
        Consume(LPAR);
        Consume(LITERAL);
        b = NewStr( Literal, STRVAL);
        AddChild (a, b);
        Consume(COMMA);
        Consume(LITERAL);
        b = NewStr( Literal, STRVAL);
        AddChild (a, b);
        Consume(RPAR);
    }

EndProduction


/*-----------------------------------------------------------------
|   XSLT LocationPathPattern  production
|
\----------------------------------------------------------------*/
Production(LocationPathPattern)

    if (LA==SLASH) {
        Consume(SLASH);
        if (LA==EOS || LA==PIPE) {
            a = New(IsRoot);
        } else {
            a = Recurse(RelativePathPattern);
            Append( a, New(ToParent) );
            Append( a, New(IsRoot) );
        }
    } else
    if ((LA==FUNCTION)
        && (  (strcmp(tokens[*l].strvalue, "id" )==0)
            ||(strcmp(tokens[*l].strvalue, "key")==0) ) )
    {
        ast b;

        b = Recurse(IdKeyPattern);
        if (LA==SLASH) {
            Consume(SLASH);
            a = Recurse(RelativePathPattern);
            Append( a, New(ToParent) );
        } else
        if (LA==SLASHSLASH) {
            Consume(SLASHSLASH);
            a = Recurse(RelativePathPattern);
            Append( a, New(ToAncestors) );
        }
        if (!a) {
            a = b;
        } else {
            Append( a, b);
        }
    } else {
        if (LA==SLASHSLASH) {
            Consume(SLASHSLASH);
            a = Recurse(RelativePathPattern);
            Append( a, New(ToAncestors) );
            Append( a, New(IsRoot) );
        } else {
            a = Recurse(RelativePathPattern);
        }
    }

EndProduction

/*-----------------------------------------------------------------
|   XSLT Pattern  production
|
\----------------------------------------------------------------*/
Production(Pattern)

    a = Recurse(LocationPathPattern);
    while (LA==PIPE) {
        Consume(PIPE);
        a = New2( CombinePath, New1(EvalSteps, a),
                               New1(EvalSteps, Recurse(LocationPathPattern) ) );
    }

EndProduction



/*----------------------------------------------------------------------------
|   xpathFreeTokens
|
\---------------------------------------------------------------------------*/
static
int xpathFreeTokens (
    XPathTokens tokens
)
{
    int i;

    for (i=0; tokens[i].token != EOS; i++) {
        if (tokens[i].strvalue) {
            FREE(tokens[i].strvalue);
        }
    }
    FREE((char*)tokens);
    return 0;
}

/*----------------------------------------------------------------------------
|   checkPatternConstraints
|
\---------------------------------------------------------------------------*/
static
int checkPatternConstraints (
    ast           t,
    xpathExprType type,
    char        **errMsg
    )
{
    while (t) {
        if (type != XPATH_KEY_USE_EXPR) {
            /* 12.4: "It is an error to use the current function in a
               pattern." */
            if (t->type == ExecFunction 
                && t->intvalue == f_unknown
                && (strcmp (t->strvalue, "current")==0)) {
                *errMsg = tdomstrdup(
                    "The 'current' function is not allowed in Pattern."
                    );
                return 0;
            }
        }
        if (type == XPATH_KEY_MATCH_PATTERN || type == XPATH_KEY_USE_EXPR) {
            /* 12.2: "It is an error for the value of either the use
               attribute or the match attribute to contain a
               VariableReference, or a call to the key function." */
            if (t->type == ExecFunction 
                && t->intvalue == f_unknown
                && (strcmp (t->strvalue, "key")==0)) {
                *errMsg = tdomstrdup(
                    "The 'key' function is not allowed in the use and match attribute pattern of xsl:key."
                    );
                return 0;
            }
            if (t->type == GetVar || t->type == GetFQVar) {
                return 0;
                *errMsg = tdomstrdup(
                    "Variable references are not allowed in the use and match attribute of xsl:key."
                    );
                return 0;
            }
        }
        if (type == XPATH_TEMPMATCH_PATTERN) {
            /* 5.3: "It is an error for the value of the match
               attribute to contain a VariableReference." */
            if (t->type == GetVar || t->type == GetFQVar) {
                *errMsg = tdomstrdup(
                    "Variable references are not allowed in the match attribute of xsl:template."
                    );
                return 0;
            }
        }
        if (t->child) {
            if (!checkPatternConstraints (t->child, type, errMsg)) return 0;
        }
        t = t->next;
    }
    return 1;
}

/*----------------------------------------------------------------------------
|   xpathParse
|
\---------------------------------------------------------------------------*/
int xpathParse (
    char         *xpath,
    char        **errMsg,
    ast          *t,
    xpathExprType type
)
{
    XPathTokens tokens;
    int  i, l, len, newlen, slen;
    char tmp[200];

    DDBG(fprintf(stderr, "\nLex output following tokens for '%s':\n", xpath);)
    *errMsg = NULL;
    tokens = xpathLexer(xpath, errMsg);
    if (*errMsg != NULL) {
        if (tokens != NULL) xpathFreeTokens (tokens);
        return XPATH_LEX_ERR;
    }
    DDBG(
        for (i=0; tokens[i].token != EOS; i++) {
            fprintf(stderr, "%3d %-12s %5d %8.3f %5d  %s\n",
                            i,
                            token2str[tokens[i].token-LPAR],
                            tokens[i].intvalue,
                            tokens[i].realvalue,
                            tokens[i].pos,
                            tokens[i].strvalue
            );
        }
    )
    l = 0;

    *t = NULL;
    if (type == XPATH_EXPR || type == XPATH_KEY_USE_EXPR) {
        *t = OrExpr (&l, tokens, errMsg);
    } else {
        *t = Pattern (&l, tokens, errMsg);
    }
    if (*errMsg == NULL && type != XPATH_EXPR) {
        checkPatternConstraints (*t, type, errMsg);
    }
    if ((*errMsg == NULL) && (tokens[l].token != EOS)) {
        *errMsg = tdomstrdup("Unexpected tokens (beyond end)!");
    }
    if (*errMsg) {
        len    = strlen(*errMsg);
        newlen = strlen(xpath);
        *errMsg = (char*)REALLOC(*errMsg, len+newlen+10);
        memmove(*errMsg + len, " for '", 6);
        memmove(*errMsg + len+6, xpath, newlen);
        memmove(*errMsg + len+6+newlen, "' ", 3);

        for (i=0; tokens[i].token != EOS; i++) {
            sprintf(tmp, "%s\n%3s%3d %-12s %5d %8.3f %5d  ",
                         (i==0) ? "\n\nParsed symbols:" : "",
                         (i==l) ? "-->" : "   ",
                          i,
                         token2str[tokens[i].token-LPAR],
                         tokens[i].intvalue,
                         tokens[i].realvalue,
                         tokens[i].pos
            );
            len    = strlen(*errMsg);
            newlen = strlen(tmp);
            slen = 0;
            if (tokens[i].strvalue) {
                slen = strlen(tokens[i].strvalue);
            }

            *errMsg = (char*)REALLOC(*errMsg, len+newlen+slen+1);
            memmove(*errMsg + len, tmp, newlen);
            memmove(*errMsg + len + newlen, tokens[i].strvalue, slen);
            (*errMsg)[len + newlen + slen] = '\0';
        }
    }
    DBG(
        if (type) {
            fprintf(stderr, "\nPattern AST for '%s': \n", xpath);
            printAst (0, *t);
        } else {
            fprintf(stderr, "AST (xpathParse):\n");
            printAst (0, *t);
        }
    )
    xpathFreeTokens (tokens);
    if (*errMsg!=NULL) {
        if (*t) freeAst (*t);
        return XPATH_SYNTAX_ERR;
    }
    return 0;

} /* xpathParse */


/*----------------------------------------------------------------------------
|   xpathNodeTest
|
\---------------------------------------------------------------------------*/
int xpathNodeTest (
    domNode        *node,
    domNode        *exprContext,
    ast             step
)
{
    domNS  *contextNS;
    char   *localName, *nodeUri;

    if (!(step->child)) return 1;
    if (step->child->type == IsElement) {
        if (node->nodeType == ELEMENT_NODE) {
            if ((step->child->strvalue[0] == '*') &&
                (step->child->strvalue[1] == '\0') &&
                (node->ownerDocument->rootNode != node)) return 1;
            if (node->namespace) return 0;
            return (strcmp(node->nodeName, step->child->strvalue)==0);
        }
        return 0;
    } else
    if (step->child->type == IsAttr) {
        if (node->nodeType == ATTRIBUTE_NODE) {
            if (node->nodeFlags & IS_NS_NODE) return 0;
            if (  (step->child->strvalue[0] == '*')
                &&(step->child->strvalue[1] == '\0')
            ) {
                return 1;
            }
            return (strcmp( ((domAttrNode*)node)->nodeName,
                            step->child->strvalue)==0);
        }
        return 0;
    } else
    if (step->child->type == IsFQElement) {
        if (node->nodeType != ELEMENT_NODE || node->namespace == 0) return 0;
        contextNS = domLookupPrefix (exprContext, step->child->strvalue);
        if (!contextNS) return 0; /* Hmmm, that's more an error, than a not match */
        nodeUri = domNamespaceURI (node);
        if (!nodeUri) return 0;
        if (strcmp (contextNS->uri, nodeUri) != 0) return 0;
        localName = domGetLocalName (node->nodeName);
        if (strcmp (step->child->child->strvalue, localName)==0) return 1;
        return 0;
    } else
    if (step->child->type == IsNSElement) {
        contextNS = domLookupPrefix (exprContext, step->child->strvalue);
        if (!contextNS) return 0; /* Hmmm, that's more an error, than a not match */
        nodeUri = domNamespaceURI (node);
        if (!nodeUri) return 0;
        if (nodeUri && (strcmp (contextNS->uri, nodeUri)==0)) return 1;
        return 0;
    } else
    if (step->child->type == IsNSAttr) {
        if (node->nodeType != ATTRIBUTE_NODE
            || node->nodeFlags & IS_NS_NODE) return 0;
        contextNS = domLookupPrefix (exprContext, step->child->strvalue);
        if (!contextNS) return 0;
        nodeUri = domNamespaceURI (node);
        if (!nodeUri) return 0;
        if (strcmp (contextNS->uri, nodeUri) != 0) return 0;
        if (strcmp (step->child->child->strvalue, "*")==0) return 1;
        localName = domGetLocalName (((domAttrNode *) node)->nodeName);
        if (strcmp (step->child->child->strvalue, localName)==0) return 1;
        return 0;
    } else
    if (step->child->type == IsNode) {
        DBG(fprintf(stderr, "nodeTest: nodeType=%d \n", node->nodeType);)
        return 1;
    } else
    if (step->child->type == IsText) {
        DBG(fprintf(stderr, "nodeTest: nodeType=%d == %d?? \n", node->nodeType, TEXT_NODE);)
        return (node->nodeType == TEXT_NODE);
    } else
    if (step->child->type == IsPI) {
        return (node->nodeType == PROCESSING_INSTRUCTION_NODE);
    } else
    if (step->child->type == IsSpecificPI) {
        return (strncmp (((domProcessingInstructionNode*)node)->targetValue,
                         step->child->strvalue,
                         ((domProcessingInstructionNode*)node)->targetLength)
            == 0);
    } else
    if (step->child->type == IsComment) {
        return (node->nodeType == COMMENT_NODE);
    }
    return 1;
}


/*----------------------------------------------------------------------------
|   xpathFuncBoolean
|
\---------------------------------------------------------------------------*/
int xpathFuncBoolean (
    xpathResultSet  *rs
)
{
    switch (rs->type) {
        case BoolResult:         return ( rs->intvalue         );
        case IntResult:          return ( rs->intvalue ? 1 : 0 );
        case RealResult:         return ((rs->realvalue != 0.0 ) && !IS_NAN (rs->realvalue));
        case StringResult:       return ( rs->string_len > 0   );
        case xNodeSetResult:     return ( rs->nr_nodes > 0     );
        case InfResult:
        case NInfResult:         return 1;
        /* NaNResult and EmptyResult are 'false' */
        default:                 return 0;
    }
}

static int
xpathIsNumber (
    char *str
    )
{
    int dotseen = 0;
    
    while (*str && IS_XML_WHITESPACE(*str)) str++;
    if (!*str) return 0;
    if (*str == '-') {
        str++;
        if (!*str) return 0;
    } else if (*str == '.') {
        dotseen = 1;
        str++;
        if (!*str) return 0;
    }
    if (!isdigit(*str)) return 0;
    while (*str) {
        if (isdigit(*str)) {
            str++;
            continue;
        }
        if (*str == '.' && !dotseen) {
            dotseen = 1;
            str++;
            continue;
        }
        break;
    }
    while (*str && IS_XML_WHITESPACE(*str)) str++;
    if (*str) return 0;
    else return 1;
}

/*----------------------------------------------------------------------------
|   xpathFuncNumber
|
\---------------------------------------------------------------------------*/
double xpathFuncNumber (
    xpathResultSet  *rs,
    int             *NaN
)
{
    double d;
    char   tmp[80], *pc, *tailptr;

    *NaN = 0;
    switch (rs->type) {
        case BoolResult:   return (rs->intvalue? 1.0 : 0.0);
        case IntResult:    return rs->intvalue;
        case RealResult:   
            if (IS_NAN(rs->realvalue)) *NaN = 2;
            else if (IS_INF(rs->realvalue)!=0) *NaN = IS_INF(rs->realvalue);
            return rs->realvalue;
        case NaNResult:
            *NaN = 2;
            return 0.0;
        case InfResult:
            *NaN = 1;
#ifdef INFINITY
            return INFINITY;
#else
#   ifdef DBL_MAX                    
            return DBL_MAX;
#   else
            /* well, what? */
            return 0.0
#   endif
#endif
        case NInfResult:
            *NaN = -1;
#ifdef INFINITY
            return -INFINITY;
#else
#   ifdef DBL_MAX                    
            return -DBL_MAX;
#   else
            /* well, what? */
            return 0.0
#   endif
#endif
        case StringResult:
              if (!xpathIsNumber (rs->string)) {
                  d = strtod ("nan", &tailptr);
                  *NaN = 2;
                  return d;
              }
              strncpy(tmp, rs->string, (rs->string_len<79) ? rs->string_len : 79);
              tmp[(rs->string_len<79) ? rs->string_len : 79] = '\0';
              d = strtod (tmp, &tailptr);
              if (d == 0.0 && tailptr == tmp) {
                  d = strtod ("nan", &tailptr);
                  *NaN = 2;
              } else
              if (IS_NAN(d)) {
                  *NaN = 2;
              } else
              if (tailptr) {
                  while (*tailptr) {
                      switch (*tailptr) {
                      case ' ' :
                      case '\n':
                      case '\r':
                      case '\t': tailptr++; continue;
                      default: break; /*do nothing */
                      }
                      d = strtod ("nan", &tailptr);
                      *NaN = 2;
                      break;
                  }
              }
              return d;
        case xNodeSetResult:
              pc = xpathFuncString(rs);
              if (!xpathIsNumber (pc)) {
                  d = strtod ("nan", &tailptr);
                  *NaN = 2;
                  FREE(pc);
                  return d;
              }
              d = strtod (pc, &tailptr);
              if (d == 0.0 && tailptr == pc) {
                  d = strtod ("nan", &tailptr);
                  *NaN = 2;
              } else
              if (IS_NAN(d)) {
                  *NaN = 2;
              } else
              if (tailptr) {
                  while (*tailptr) {
                      switch (*tailptr) {
                      case ' ' :
                      case '\n':
                      case '\r':
                      case '\t': tailptr++; continue;
                      default: break; /*do nothing */
                      }
                      d = strtod ("nan", &tailptr);
                      *NaN = 2;
                      break;
                  }
              }
              FREE(pc);
              return d;
        default:
              DBG(fprintf(stderr, "funcNumber: default: 0.0\n");)
              d = strtod ("nan", &tailptr);
              *NaN = 2;
              return d;
    }
}


/*----------------------------------------------------------------------------
|   xpathGetStringValue
|
\---------------------------------------------------------------------------*/
char * xpathGetStringValueForElement (
    domNode *node,
    int     *len
)
{
    char        *pc, *t;
    int          l;
    domNode     *child;

    if (node->nodeType == ELEMENT_NODE) {
        DBG(fprintf(stderr,"GetTextValue: tag='%s' \n", node->nodeName);)
        pc = MALLOC(1); *pc = '\0'; *len = 0;
        child = node->firstChild;
        while (child) {
            t = xpathGetStringValueForElement(child, &l);
            pc = (char*)REALLOC(pc, 1 + *len + l);
            memmove(pc + *len, t, l );
            *len += l;
            pc[*len] = '\0';
            FREE((char*)t);
            child = child->nextSibling;
        }
    } else
    if (node->nodeType == TEXT_NODE) {

        *len = ((domTextNode*)node)->valueLength;
        pc   = (char*)MALLOC(1+*len);
        memmove(pc, ((domTextNode*)node)->nodeValue, *len);
        pc[*len] = '\0';
        DBG(fprintf(stderr,"GetTextValue: text='%s' \n", pc);)
    } else {
        pc   = tdomstrdup ("");
        *len = 0;
    }
    return pc;
}

char * xpathGetStringValue (
    domNode *node,
    int     *len
)
{
    char         *pc, *t;
    int          l;
    domNode     *child;
    domAttrNode *attr;

    if (node->nodeType == ELEMENT_NODE) {
        DBG(fprintf(stderr,"GetTextValue: tag='%s' \n", node->nodeName);)
        pc = MALLOC(1); *pc = '\0'; *len = 0;
        child = node->firstChild;
        while (child) {
            t = xpathGetStringValueForElement(child, &l);
            pc = (char*)REALLOC(pc, 1 + *len + l);
            memmove(pc + *len, t, l );
            *len += l;
            pc[*len] = '\0';
            FREE((char*)t);
            child = child->nextSibling;
        }
    } else
    if ((node->nodeType == TEXT_NODE) ||
        (node->nodeType == CDATA_SECTION_NODE) ||
        (node->nodeType == COMMENT_NODE)
    ) {

        *len = ((domTextNode*)node)->valueLength;
        pc   = (char*)MALLOC(1+*len);
        memmove(pc, ((domTextNode*)node)->nodeValue, *len);
        pc[*len] = '\0';
        DBG(fprintf(stderr,"GetTextValue: text='%s' \n", pc);)
    } else
    if (node->nodeType == PROCESSING_INSTRUCTION_NODE) {
        *len = ((domProcessingInstructionNode*)node)->dataLength;
        pc   = (char*)MALLOC(1+*len);
        memmove(pc, ((domProcessingInstructionNode*)node)->dataValue, *len);
        pc[*len] = '\0';
    } else
    if (node->nodeType == ATTRIBUTE_NODE) {
        attr = (domAttrNode*)node;
        DBG(fprintf(stderr,"GetTextValue: attr='%s' \n", attr->nodeName);)
        pc = MALLOC(attr->valueLength +1);
        memmove(pc, attr->nodeValue, attr->valueLength);
        *(pc + attr->valueLength) = '\0';
        *len = attr->valueLength;
    }
    else {
        pc   = tdomstrdup ("");
        *len = 0;
    }
    return pc;
}

/*----------------------------------------------------------------------------
|   xpathFuncString
|
\---------------------------------------------------------------------------*/
char * xpathFuncString (
    xpathResultSet  *rs
)
{
    char         tmp[80], *pc;
    int          len;

    switch (rs->type) {
        case BoolResult:
            if (rs->intvalue) return (tdomstrdup("true"));
                         else return (tdomstrdup("false"));
        case IntResult:
            sprintf(tmp, "%d", rs->intvalue);
            return (tdomstrdup(tmp));

        case RealResult:
            if (IS_NAN (rs->realvalue)) return tdomstrdup ("NaN");
            else if (IS_INF (rs->realvalue)) {
                if (IS_INF (rs->realvalue) == 1) return tdomstrdup ("Infinity");
                else                             return tdomstrdup ("-Infinity");
            }
            sprintf(tmp, "%f", rs->realvalue);
            /* strip trailing 0 and . */
            len = strlen(tmp);
            for (; (len > 0) && (tmp[len-1] == '0'); len--) tmp[len-1] = '\0';
            if ((len > 0) && (tmp[len-1] == '.'))   tmp[len-1] = '\0';
            return (tdomstrdup(tmp));
            
        case NaNResult:
            return tdomstrdup ("NaN");

        case InfResult:
            return tdomstrdup ("Infinity");

        case NInfResult:
            return tdomstrdup ("-Infinity");

        case StringResult:
            pc = MALLOC(rs->string_len +1);
            memmove(pc, rs->string, rs->string_len);
            *(pc + rs->string_len) = '\0';
            return pc;

        case xNodeSetResult:
            if (rs->nr_nodes == 0) {
                pc = tdomstrdup ("");
            } else {
                pc = xpathGetStringValue (rs->nodes[0], &len);
            }
            return pc;

        default:
            return (tdomstrdup (""));
    }
}

/*----------------------------------------------------------------------------
|   xpathFuncStringForNode
|
\---------------------------------------------------------------------------*/
char * xpathFuncStringForNode (
    domNode *node
)
{
    int          len;

    return xpathGetStringValue (node, &len);
}

/*----------------------------------------------------------------------------
|   xpathFuncNumberForNode
|
\---------------------------------------------------------------------------*/
double xpathFuncNumberForNode (
    domNode *node,
    int      *NaN
)
{
    char        *pc;
    int          len, rc;
    double       d;

    *NaN = 0;
    pc = xpathGetStringValue (node, &len);
    rc = sscanf (pc,"%lf", &d);
    if (rc != 1) *NaN = 2;
    FREE(pc);
    return d;
}


/*----------------------------------------------------------------------------
|   xpathArity
|
\---------------------------------------------------------------------------*/
static int xpathArity (
    ast step
)
{
    int parms = 0;

    step = step->child;
    while (step) {
        parms++;
        step = step->next;
    }
    return parms;
}


/*----------------------------------------------------------------------------
|   xpathArityCheck
|
\---------------------------------------------------------------------------*/
static int xpathArityCheck (
    ast    step,
    int    arity,
    char **errMsg
)
{
    int parms = 0;

    step = step->child;
    while (step) {
        parms++;
        step = step->next;
    }
    if (arity!=parms) {
        *errMsg = tdomstrdup("wrong number of parameters!");
        return 1;
    }
    return 0;
}
#define XPATH_ARITYCHECK(s,a,m) if (xpathArityCheck(s,a,m)) return XPATH_EVAL_ERR




/*----------------------------------------------------------------------------
|   xpathRound
|
\---------------------------------------------------------------------------*/
int xpathRound (double r) {
    if (r < 0.0) {
        return (int)floor (r + 0.5);
    } else {
        return (int)(r + 0.5);
    }
}

/*----------------------------------------------------------------------------
|   xpathEvalFunction
|
\---------------------------------------------------------------------------*/
static int
xpathEvalFunction (
    ast                step,
    domNode           *ctxNode,
    domNode           *exprContext,
    int                position,
    xpathResultSet    *nodeList,
    xpathCBs          *cbs,
    xpathResultSet    *result,
    int               *docOrder,
    char             **errMsg
    )
{
    xpathResultSet   leftResult, rightResult, replaceResult;
    int              i, rc, pwhite, len,  NaN;
    char            *replaceStr, *pfrom, *pto, tmp[80], tmp1[80];
    domNode         *node;
    domAttrNode     *attr;
    double           leftReal;
    ast              nextStep;
    int              argc, savedDocOrder, from;
    xpathResultSets *args;
    xpathResultSet  *arg;
    Tcl_HashEntry   *entryPtr;
    int              left = 0, useFastAdd;
    double           dRight = 0.0;
    char            *leftStr = NULL, *rightStr = NULL;
    domNS           *ns;
    Tcl_DString      dStr;
#if TclOnly8Bits
    char            *fStr;
#else
    int              found, j;
    int              lenstr, fromlen, utfCharLen;
    char             utfBuf[TCL_UTF_MAX];
    Tcl_DString      tstr, tfrom, tto, tresult;
    Tcl_UniChar     *ufStr, *upfrom, unichar;
#endif

    if (result->type == EmptyResult) useFastAdd = 1;
    else useFastAdd = 0;


    switch (step->intvalue) {

    case f_position:
        XPATH_ARITYCHECK(step,0,errMsg);
        if (*docOrder) {
            rsSetInt (result, position+1);
        } else {
            rsSetInt (result, nodeList->nr_nodes - position);
        }
        break;

    case f_last:
        XPATH_ARITYCHECK(step,0,errMsg);
        rsSetInt (result, nodeList->nr_nodes);
        break;

    case f_number:
        xpathRSInit (&leftResult);
        if (xpathArity(step) == 0) {
            /*  no parameter, the context node is the nodeset to
             *  operate with
             */
            rsAddNodeFast( &leftResult, ctxNode);
        } else {
            XPATH_ARITYCHECK(step,1,errMsg);
            xpathRSInit (&leftResult);
            rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                                nodeList, cbs, &leftResult, docOrder, errMsg);
            if (rc) {
                xpathRSFree( &leftResult );
                return rc;
            }
        }
        leftReal = xpathFuncNumber(&leftResult, &NaN);
        if (NaN) {
            if      (NaN == 2)  rsSetNaN (result);
            else if (NaN == 1)  rsSetInf (result);
            else                rsSetNInf (result);
        } else {
            rsSetReal (result, leftReal);
        }
        xpathRSFree( &leftResult );
        break;

    case f_floor:
    case f_ceiling:
    case f_round:
        XPATH_ARITYCHECK(step,1,errMsg);
        xpathRSInit (&leftResult);
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        if (rc) {
            xpathRSFree( &leftResult );
            return rc;
        }
        leftReal = xpathFuncNumber(&leftResult, &NaN);

        if (NaN) { 
            if      (NaN == 2)  rsSetNaN (result);
            else if (NaN == 1)  rsSetInf (result);
            else                rsSetNInf (result);
        } else {
            if      (step->intvalue == f_floor)   leftReal = floor(leftReal);
            else if (step->intvalue == f_ceiling) leftReal = ceil(leftReal);
            else                                  leftReal = 
                                                      xpathRound(leftReal);
            rsSetReal (result, leftReal);
        }
        xpathRSFree( &leftResult );
        break;

    case f_boolean:
        XPATH_ARITYCHECK(step,1,errMsg);
        xpathRSInit (&leftResult);
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        if (rc) {
            xpathRSFree( &leftResult );
            return rc;
        }
        left = xpathFuncBoolean(&leftResult);
        rsSetBool (result, left);
        xpathRSFree( &leftResult );
        break;

    case f_string:
    case f_normalizeSpace:
    case f_stringLength:
        xpathRSInit (&leftResult);
        if (step->child == NULL) {
            /*  no parameter, the context node is the nodeset to
             *  operate with
             */
            rsAddNodeFast( &leftResult, ctxNode);

        } else {
            XPATH_ARITYCHECK(step,1,errMsg);
            xpathRSInit (&leftResult);
            rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                                nodeList, cbs, &leftResult, docOrder, errMsg);
            if (rc) {
                xpathRSFree( &leftResult );
                return rc;
            }
            DBG(fprintf(stderr, "normalize-space: \n");
                rsPrint(&leftResult);
                )
        }

        leftStr = xpathFuncString (&leftResult );
        DBG(fprintf(stderr, "leftStr='%s'\n", leftStr);)
        if      (step->intvalue == f_string)
            rsSetString (result, leftStr);
        else if (step->intvalue == f_stringLength)
            rsSetInt (result, strlen(leftStr));
        else {
            pwhite = 1;
            pfrom = pto = leftStr;
            while (*pfrom) {
                switch (*pfrom) {
                case ' ' : case '\n': case '\r': case '\t':
                    if (!pwhite) {
                        *pto++ = ' ';
                        pwhite = 1;
                    }
                    break;
                default:
                    *pto++ = *pfrom;
                    pwhite = 0;
                    break;
                }
                pfrom++;
            }
            if ((pto > leftStr) && (*(pto-1) == ' ')) {
                pto--;  /* cut last empty space */
            }
            *pto = '\0';
            rsSetString (result, leftStr);
        }
        xpathRSFree( &leftResult );
        FREE(leftStr);
        break;

    case f_not:
        XPATH_ARITYCHECK(step,1,errMsg);
        xpathRSInit (&leftResult);
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        if (rc) {
            xpathRSFree (&leftResult);
            return rc;
        }
        left = xpathFuncBoolean(&leftResult);
        xpathRSFree (&leftResult);
        rsSetBool (result, !left);
        break;

    case f_true:
        XPATH_ARITYCHECK(step,0,errMsg);
        rsSetBool (result, 1);
        break;

    case f_false:
        XPATH_ARITYCHECK(step,0,errMsg);
        rsSetBool (result, 0);
        break;

    case f_id:
        XPATH_ARITYCHECK(step,1,errMsg);
        xpathRSInit (&leftResult);
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        if (rc) {
            xpathRSFree( &leftResult );
            return rc;
        }
        DBG(fprintf(stderr, "id: \n");
            rsPrint(&leftResult);
            )
        if (leftResult.type == EmptyResult) {
            *errMsg = tdomstrdup ("id() requires an argument!");
            return XPATH_EVAL_ERR;
        }
        if (leftResult.type == xNodeSetResult) {
            for (i=0; i < leftResult.nr_nodes; i++) {
                leftStr = xpathFuncStringForNode (leftResult.nodes[i]);
                entryPtr = Tcl_FindHashEntry (&ctxNode->ownerDocument->ids,
                                              leftStr);
                if (entryPtr) {
                    node = (domNode*) Tcl_GetHashValue (entryPtr);
                    /* Don't report nodes out of the fragment list */
                    if (node->parentNode != NULL ||
                        (node == node->ownerDocument->documentElement)) {
                        rsAddNode (result, node);
                    }
                }
                FREE(leftStr);
                /*xpathRSFree (&newNodeList);*/
            }
        } else {
            leftStr = xpathFuncString (&leftResult);
            from = 0;
            pwhite = 0;
            pfrom = pto = leftStr;
            while (*pto) {
                switch (*pto) {
                case ' ' : case '\n': case '\r': case '\t':
                    if (pwhite) {
                        pto++;
                        continue;
                    }
                    *pto = '\0';
                    entryPtr = Tcl_FindHashEntry (&ctxNode->ownerDocument->ids,
                                                  pfrom);
                    if (entryPtr) {
                        node = (domNode*) Tcl_GetHashValue (entryPtr);
                        /* Don't report nodes out of the fragment list */
                        if (node->parentNode != NULL ||
                            (node == node->ownerDocument->documentElement)) {
                            rsAddNode (result, node);
                        }
                    }
                    pwhite = 1;
                    pto++;
                    continue;
                default:
                    if (pwhite) {
                        pfrom = pto;
                        pwhite = 0;
                    }
                    pto++;
                }
            }
            if (!pwhite) {
                entryPtr = Tcl_FindHashEntry (&ctxNode->ownerDocument->ids,
                                              pfrom);
                if (entryPtr) {
                    node = (domNode*) Tcl_GetHashValue (entryPtr);
                    /* Don't report nodes out of the fragment list */
                    if (node->parentNode != NULL ||
                        (node == node->ownerDocument->documentElement)) {
                        rsAddNode (result, node);
                    }
                }
            }
            FREE(leftStr);
        }
        sortByDocOrder (result);
        xpathRSFree (&leftResult);
        break;

    case f_sum:
        XPATH_ARITYCHECK(step,1,errMsg);
        xpathRSInit (&leftResult);
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        if (rc) {
            xpathRSFree( &leftResult );
            return rc;
        }
        if (leftResult.type != xNodeSetResult) {
            if (leftResult.type == EmptyResult) {
                rsSetInt (result, 0);
                return XPATH_OK;
            } else {
                xpathRSFree( &leftResult );
                *errMsg = tdomstrdup("sum() requires a node set!");
                xpathRSFree( &leftResult );
                return XPATH_EVAL_ERR;
            }
        }

        xpathRSInit(&rightResult);
        rightResult.nr_nodes = 1;
        rightResult.type     = leftResult.type;
        leftReal = 0.0;
        for (i=0; i<leftResult.nr_nodes; i++) {
            rightResult.nodes = &(leftResult.nodes[i]);
            DBG(fprintf(stderr, "leftReal = %f \n", leftReal);)
            leftReal += xpathFuncNumber(&rightResult, &NaN);
            if (NaN) {
                rsSetNaN (result);
                xpathRSFree( &leftResult );
                return XPATH_OK;
            }
            DBG(fprintf(stderr, "leftReal = %f \n", leftReal);)
        }
        rsSetReal (result, leftReal);
        xpathRSFree( &leftResult );
        break;

    case f_lang:
        XPATH_ARITYCHECK(step,1,errMsg);
        xpathRSInit (&leftResult);
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        if (rc) {
            xpathRSFree (&leftResult);
            return rc;
        }
        leftStr = xpathFuncString (&leftResult);
        if (ctxNode->nodeType != ELEMENT_NODE) {
            node = ctxNode->parentNode;
        } else {
            node = ctxNode;
        }
        while (node) {
            attr = node->firstAttr;
            while (attr) {
                if (strcmp (attr->nodeName, "xml:lang")!=0) {
                    attr = attr->nextSibling;
                    continue;
                }
                tcldom_tolower (attr->nodeValue, tmp, 80);
                tcldom_tolower (leftStr, tmp1, 80);
                if (strcmp (tmp, tmp1)==0) {
                    rsSetBool (result, 1);
                    FREE(leftStr);
                    xpathRSFree (&leftResult);
                    return XPATH_OK;
                } else {
                    pfrom = tmp;
                    i = 0;
                    while (*pfrom && i < 79) {
                        if (*pfrom == '-') {
                            *pfrom = '\0';
                            break;
                        }
                        pfrom++;
                        i++;
                    }
                    if (strcmp (tmp, tmp1)==0) {
                        rsSetBool (result, 1);
                        FREE(leftStr);
                        xpathRSFree (&leftResult);
                        return XPATH_OK;
                    } else {
                        rsSetBool (result, 0);
                        FREE(leftStr);
                        xpathRSFree (&leftResult);
                        return XPATH_OK;
                    }
                }
            }
            node = node->parentNode;
        }
        rsSetBool (result, 0);
        FREE(leftStr);
        xpathRSFree (&leftResult);
        break;

    case f_startsWith:
    case f_contains:
    case f_substringBefore:
    case f_substringAfter:
        XPATH_ARITYCHECK(step,2,errMsg);
        xpathRSInit (&leftResult);
        xpathRSInit (&rightResult);

        savedDocOrder = *docOrder;
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position, nodeList,
                            cbs, &leftResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;

        rc = xpathEvalStep( step->child->next, ctxNode, exprContext, position,
                            nodeList, cbs, &rightResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;


        DBG(fprintf(stderr, "\nsubstring-* left,right:\n");
            rsPrint(&leftResult);
            rsPrint(&rightResult);
            )
        leftStr  = xpathFuncString( &leftResult  );
        rightStr = xpathFuncString( &rightResult );
        DBG(fprintf(stderr, "substring-* '%s' '%s' \n", leftStr, rightStr);)
        if (step->intvalue == f_contains) {
            if (strstr(leftStr, rightStr) != NULL) {
                rsSetBool (result, 1);
            } else {
                rsSetBool (result, 0);
            }
        } else
        if (step->intvalue == f_substringBefore) {
            pfrom = strstr(leftStr, rightStr);
            if (pfrom != NULL) {
                DBG(fprintf(stderr, "substring-before '%s' '%s' : ", leftStr, rightStr);)
                    *pfrom = '\0';
                DBG(fprintf(stderr, "'%s' \n", leftStr);)
                    rsSetString (result, leftStr);
            } else {
                rsSetString (result, "");
            }
        } else
        if (step->intvalue == f_substringAfter) {
            pfrom = strstr(leftStr, rightStr);
            if (pfrom != NULL) {
                rsSetString (result, pfrom + strlen (rightStr));
            } else {
                rsSetString (result, "");
            }
        } else {
            /* starts-with */
            i = strlen(rightStr);
            if(strncmp(leftStr, rightStr, i)==0) {
                rsSetBool (result, 1);
            } else {
                rsSetBool (result, 0);
            }
        }
        xpathRSFree (&leftResult);
        xpathRSFree (&rightResult);
        FREE(rightStr);
        FREE(leftStr);
        break;

    case f_concat:
        if (xpathArity(step) < 2) {
            *errMsg = tdomstrdup("wrong number of parameters!");
            return XPATH_EVAL_ERR;
        }
        nextStep = step->child;
        pto = MALLOC(1);
        *pto = '\0';
        len = 0;
        while (nextStep) {
            xpathRSInit (&leftResult);
            savedDocOrder = *docOrder;
            rc = xpathEvalStep( nextStep, ctxNode, exprContext, position,
                                nodeList, cbs, &leftResult, docOrder, errMsg);
            CHECK_RC;
            *docOrder = savedDocOrder;

            leftStr  = xpathFuncString( &leftResult  );
            pto = (char*)REALLOC(pto, 1+len+strlen(leftStr));
            memmove(pto + len, leftStr, strlen(leftStr));
            len += strlen(leftStr);
            *(pto + len) = '\0';
            xpathRSFree( &leftResult );
            FREE(leftStr);
            nextStep = nextStep->next;
        }
        rsSetString (result, pto);
        FREE(pto);
        break;

    case f_substring:
        i = xpathArity(step);
        if (i != 2 && i != 3) {
            *errMsg = tdomstrdup("wrong number of parameters!");
            return XPATH_EVAL_ERR;
        }
        xpathRSInit (&leftResult);
        savedDocOrder = *docOrder;
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position, 
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;

        xpathRSInit (&rightResult);
        rc = xpathEvalStep( step->child->next, ctxNode, exprContext, position,
                            nodeList, cbs, &rightResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;

        leftStr  = xpathFuncString( &leftResult );
        xpathRSFree (&leftResult);
        from = xpathRound(xpathFuncNumber(&rightResult, &NaN))-1;
        xpathRSFree( &rightResult );
        if (NaN) {
            FREE(leftStr);
            rsSetString (result, "");
            return XPATH_OK;
        }

        if (step->child->next->next) {
            xpathRSInit (&rightResult);
            savedDocOrder = *docOrder;
            rc = xpathEvalStep( step->child->next->next, ctxNode, exprContext,
                                position, nodeList, cbs, &rightResult, docOrder,
                                errMsg);
            CHECK_RC;
            *docOrder = savedDocOrder;

            dRight = xpathFuncNumber (&rightResult, &NaN);
            len = xpathRound(dRight);
            xpathRSFree (&rightResult);
            if (NaN) {
                if (NaN == 1) {
                    len = INT_MAX;
                } else {
                    FREE(leftStr);
                    rsSetString (result, "");
                    return XPATH_OK;
                }
            }
            xpathRSFree (&rightResult);
            if (from < 0) {
                len = len + from;
                if (len <= 0) {
                    FREE(leftStr);
                    rsSetString (result, "");
                    return XPATH_OK;
                }
                from = 0;
            }
        } else {
            if (from < 0) from = 0;
            len = strlen(leftStr) - from;
        }

        if (from >= (int) strlen(leftStr)) {
            rsSetString (result, "");
            FREE(leftStr);
            return XPATH_OK;
        } else {
            if ( (len == INT_MAX) || ((from + len) > (int) strlen(leftStr)) ) {
                len =  strlen(leftStr) - from;
            }
        }
        DBG(fprintf(stderr, "substring leftStr='%s' from=%d len=%d \n",
                    leftStr, from, len);
            )

            *(leftStr+from+len) = '\0';
        rsSetString (result, (leftStr+from));
        FREE(leftStr);
        break;

    case f_translate:
        XPATH_ARITYCHECK(step,3,errMsg);
        xpathRSInit (&leftResult);
        savedDocOrder = *docOrder;
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position, nodeList,
                            cbs, &leftResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;
        xpathRSInit (&rightResult);
        rc = xpathEvalStep( step->child->next, ctxNode, exprContext, position,
                            nodeList, cbs, &rightResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;
        xpathRSInit (&replaceResult);
        rc = xpathEvalStep( step->child->next->next, ctxNode, exprContext,
                            position, nodeList, cbs, &replaceResult, docOrder,
                            errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;
        leftStr    = xpathFuncString( &leftResult    );
        rightStr   = xpathFuncString( &rightResult   );
        replaceStr = xpathFuncString( &replaceResult );


#if TclOnly8Bits
        len = strlen(replaceStr);
        pfrom = pto = leftStr;
        while (*pfrom) {
            fStr = strchr(rightStr, *pfrom);
            if (fStr == NULL) {
                *pto++ = *pfrom;
            } else {
                i = (fStr - rightStr);
                if (i < len) {
                    *pto++ = *(replaceStr+i);
                }
            }
            pfrom++;
        }
        *pto = '\0';
        rsSetString (result, leftStr);
#else
        Tcl_DStringInit (&tstr);
        Tcl_DStringInit (&tfrom);
        Tcl_DStringInit (&tto);
        Tcl_DStringInit (&tresult);

        Tcl_UtfToUniCharDString (leftStr, -1, &tstr);
        Tcl_UtfToUniCharDString (rightStr, -1, &tfrom);
        Tcl_UtfToUniCharDString (replaceStr, -1, &tto);

        lenstr  = Tcl_DStringLength (&tstr) / sizeof (Tcl_UniChar);
        fromlen = Tcl_DStringLength (&tfrom) / sizeof (Tcl_UniChar);
        len     = Tcl_DStringLength (&tto) / sizeof (Tcl_UniChar);

        upfrom = (Tcl_UniChar *)Tcl_DStringValue (&tstr);
        for (i = 0; i < lenstr; i++) {
            found = 0;
            ufStr = (Tcl_UniChar *)Tcl_DStringValue (&tfrom);
            for (j = 0; j < fromlen; j++) {
                if (*ufStr == *upfrom) {
                    found = 1;
                    break;
                }
                ufStr++;
            }
            if (found) {
                if (j < len) {
                    unichar = Tcl_UniCharAtIndex (replaceStr, j);
                    utfCharLen = Tcl_UniCharToUtf (unichar, utfBuf);
                    Tcl_DStringAppend (&tresult, utfBuf, utfCharLen);
                }
            } else {
                utfCharLen = Tcl_UniCharToUtf (*upfrom, utfBuf);
                Tcl_DStringAppend (&tresult, utfBuf, utfCharLen);
            }
            upfrom++;
        }
        rsSetString (result, Tcl_DStringValue (&tresult));
        Tcl_DStringFree (&tstr);
        Tcl_DStringFree (&tfrom);
        Tcl_DStringFree (&tto);
        Tcl_DStringFree (&tresult);
#endif

        xpathRSFree( &replaceResult );
        xpathRSFree( &rightResult   );
        xpathRSFree( &leftResult    );
        FREE(leftStr); FREE(rightStr); FREE(replaceStr);
        break;

    case f_count:
        XPATH_ARITYCHECK(step,1,errMsg);
        xpathRSInit (&leftResult);
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        if (rc) {
            xpathRSFree( &leftResult );
            return rc;
        }
        if (leftResult.type == EmptyResult) {
            rsSetInt (result, 0);
            return XPATH_OK;
        }
        if (leftResult.type != xNodeSetResult) {
            *errMsg = tdomstrdup("count() requires a node set!");
            xpathRSFree( &leftResult );
            return XPATH_EVAL_ERR;
        }
        rsSetInt (result, leftResult.nr_nodes);
        xpathRSFree (&leftResult);
        break;

    case f_unparsedEntityUri:
        XPATH_ARITYCHECK(step,1,errMsg);
        xpathRSInit (&leftResult);
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        if (rc) {
            xpathRSFree( &leftResult );
            return rc;
        }
        leftStr = xpathFuncString (&leftResult);
        entryPtr = Tcl_FindHashEntry (&ctxNode->ownerDocument->unparsedEntities,
                                      leftStr);
        if (entryPtr) {
            rsSetString (result, (char *)Tcl_GetHashValue (entryPtr));
        } else {
            rsSetString (result, "");
        }
        FREE(leftStr);
        xpathRSFree (&leftResult);
        break;

    case f_localName:
    case f_name:
    case f_namespaceUri:
    case f_generateId:
        xpathRSInit (&leftResult);
        if (step->child == NULL) {
            /*  no parameter, the context node is the nodeset to
             *  operate with
             */
            rsAddNodeFast( &leftResult, ctxNode);
        } else {
            XPATH_ARITYCHECK(step,1,errMsg);
            xpathRSInit (&leftResult);
            rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                                nodeList, cbs, &leftResult, docOrder, errMsg);
            if (rc) {
                xpathRSFree( &leftResult );
                return rc;
            }
        }
        if (leftResult.type == EmptyResult) {
            rsSetString (result, "");
            return XPATH_OK;
        }

        if (step->intvalue == f_generateId) {
            if (leftResult.type != xNodeSetResult) {
                *errMsg = tdomstrdup("generate-id() requires a nodeset or no argument!");
                xpathRSFree (&leftResult);
                return XPATH_EVAL_ERR;
            }
            if (leftResult.nodes[0]->nodeType == ATTRIBUTE_NODE) {
                node = ((domAttrNode*)leftResult.nodes[0])->parentNode;
                i = 0;
                attr = node->firstAttr;
                while (attr) {
                    if ((domNode*)attr == leftResult.nodes[0]) break;
                    attr = attr->nextSibling;
                    i++;
                }
                sprintf(tmp,"id%x-%d", node, i);
            } else {
                sprintf(tmp,"id%x", leftResult.nodes[0]);
            }
            rsSetString (result, tmp);
        } else

        if (step->intvalue == f_namespaceUri) {
            if (leftResult.type != xNodeSetResult) {
                *errMsg = tdomstrdup("namespace-uri() requires a node set!");
                xpathRSFree( &leftResult );
                return XPATH_EVAL_ERR;
            }
            if ( (leftResult.nr_nodes <= 0)
                 || (   leftResult.nodes[0]->nodeType != ELEMENT_NODE
                        && leftResult.nodes[0]->nodeType != ATTRIBUTE_NODE )
                )
                {
                    rsSetString (result, "");
                } else {
                    rsSetString (result, domNamespaceURI(leftResult.nodes[0]));
                }
        } else

        if (step->intvalue == f_localName) {
            if (leftResult.type != xNodeSetResult) {
                *errMsg = tdomstrdup("local-name() requires a node set!");
                xpathRSFree( &leftResult );
                return XPATH_EVAL_ERR;
            }
            if (leftResult.nodes[0]->nodeType == ELEMENT_NODE) {
                if (leftResult.nodes[0] ==
                    leftResult.nodes[0]->ownerDocument->rootNode) {
                    rsSetString (result, "");
                } else {
                    rsSetString (result, domGetLocalName(leftResult.nodes[0]->nodeName));
                }
            } else
            if (leftResult.nodes[0]->nodeType == ATTRIBUTE_NODE) {
                leftStr = domGetLocalName(((domAttrNode*)leftResult.nodes[0])->nodeName);
                if (leftStr[0] == 'x' && strcmp(leftStr, "xmlns")==0) {
                    rsSetString (result, "");
                } else {
                    rsSetString (result, leftStr);
                }
            } else
            if (leftResult.nodes[0]->nodeType == PROCESSING_INSTRUCTION_NODE) {
                if (((domProcessingInstructionNode*)leftResult.nodes[0])->targetLength > 79) {
                    memmove(tmp, ((domProcessingInstructionNode*)leftResult.nodes[0])->targetValue, 79);
                    tmp[79]= '\0';
                } else {
                    memmove(tmp, ((domProcessingInstructionNode*)leftResult.nodes[0])->targetValue,
                            ((domProcessingInstructionNode*)leftResult.nodes[0])->targetLength);
                    tmp[((domProcessingInstructionNode*)leftResult.nodes[0])->targetLength] = '\0';
                }
                rsSetString (result, tmp);
            } else {
                rsSetString (result, "");
            }
        } else

        if (step->intvalue == f_name) {
            if (   leftResult.type != xNodeSetResult ) {
                *errMsg = tdomstrdup("name() requires a node set!");
                xpathRSFree( &leftResult );
                return XPATH_EVAL_ERR;
            }
            if (leftResult.nodes[0]->nodeType == ELEMENT_NODE) {
                if (leftResult.nodes[0] ==
                    leftResult.nodes[0]->ownerDocument->rootNode) {
                    rsSetString (result, "");
                } else {
                    rsSetString (result, leftResult.nodes[0]->nodeName);
                }
            } else
            if (leftResult.nodes[0]->nodeType == ATTRIBUTE_NODE) {
                if (leftResult.nodes[0]->nodeFlags & IS_NS_NODE) {
                    if (((domAttrNode *)leftResult.nodes[0])->nodeName[5] == '\0') {
                        rsSetString (result, "");
                    } else {
                        rsSetString (result, &((domAttrNode*)leftResult.nodes[0])->nodeName[6]);
                    }
                } else {
                    rsSetString (result, ((domAttrNode*)leftResult.nodes[0])->nodeName );
                }
            } else
            if (leftResult.nodes[0]->nodeType == PROCESSING_INSTRUCTION_NODE) {
                if (((domProcessingInstructionNode*)leftResult.nodes[0])->targetLength > 79) {
                    memmove(tmp, ((domProcessingInstructionNode*)leftResult.nodes[0])->targetValue, 79);
                    tmp[79]= '\0';
                } else {
                    memmove(tmp, ((domProcessingInstructionNode*)leftResult.nodes[0])->targetValue,
                            ((domProcessingInstructionNode*)leftResult.nodes[0])->targetLength);
                    tmp[((domProcessingInstructionNode*)leftResult.nodes[0])->targetLength] = '\0';
                }
                rsSetString (result, tmp);
            } else {
                rsSetString (result, "");
            }
        }
        xpathRSFree( &leftResult );
        break;

    case f_fqfunction: 
        ns = domLookupPrefix (exprContext, step->strvalue);
        if (!ns) {
            *errMsg = tdomstrdup ("There isn't a namespace bound to the prefix.");
            return XPATH_EVAL_ERR;
        }
        Tcl_DStringInit (&dStr);
        if (cbs->funcCB == NULL) {
            Tcl_DStringAppend (&dStr, "Unknown function ", -1);
        }
        Tcl_DStringAppend (&dStr, ns->uri, -1);
        Tcl_DStringAppend (&dStr, "::", 2);
        nextStep = step->child;
        Tcl_DStringAppend (&dStr, nextStep->strvalue, -1);
        if (cbs->funcCB == NULL) {
            *errMsg = tdomstrdup (Tcl_DStringValue (&dStr));
            Tcl_DStringFree (&dStr);
            return XPATH_EVAL_ERR;
        }
        /* fall throu */
           
    default:
        if (cbs->funcCB == NULL) {
            if (strlen(step->strvalue)>50) *(step->strvalue + 50) = '\0';
            sprintf(tmp, "Unknown function '%s'!", step->strvalue);
            *errMsg = tdomstrdup(tmp);
            return XPATH_EVAL_ERR;
        }
           
        /* count number of arguments (to be able to allocate later) */
        argc = 0;
        if (step->intvalue == f_fqfunction) {
            nextStep = step->child->next;
        } else {
            nextStep = step->child;
        }
        while (nextStep) {
            argc++;
            nextStep = nextStep->next;
        }
        args = (xpathResultSets*)MALLOC((argc+1) * sizeof(xpathResultSets));
        args[0] = NULL;
        argc = 0;
        if (step->intvalue == f_fqfunction) {
            nextStep = step->child->next;
        } else {
            nextStep = step->child;
        }
        savedDocOrder = *docOrder;
        while (nextStep) {
            arg = (xpathResultSet*)MALLOC(sizeof(xpathResultSet));
            args[argc++] = arg;
            args[argc]   = NULL;
            xpathRSInit (arg);
            rc = xpathEvalStep( nextStep, ctxNode, exprContext, position,
                                nodeList, cbs, arg, docOrder, errMsg);
            if (rc) goto unknownfunccleanup;
            *docOrder = savedDocOrder;
            nextStep = nextStep->next;
        }
        if (step->intvalue == f_fqfunction) {
            rc = (cbs->funcCB) (cbs->funcClientData, Tcl_DStringValue (&dStr),
                                ctxNode, position, nodeList, exprContext,
                                argc, args, result, errMsg);
        } else {
            rc = (cbs->funcCB) (cbs->funcClientData, step->strvalue,
                                ctxNode, position, nodeList, exprContext,
                                argc, args, result, errMsg);
        }
    unknownfunccleanup:
        argc = 0;
        while ( args[argc] != NULL) {
            xpathRSFree( args[argc] );
            FREE((char*)args[argc++]);
        }
        FREE((char*)args);
        if (step->intvalue == f_fqfunction) {
            Tcl_DStringFree (&dStr);
        }
        return rc;
    }
    return XPATH_OK;
}

/*----------------------------------------------------------------------------
|   xpathEvalStep
|
\---------------------------------------------------------------------------*/
static int xpathEvalStep (
    ast                step,
    domNode           *ctxNode,
    domNode           *exprContext,
    int                position,
    xpathResultSet    *nodeList,
    xpathCBs          *cbs,
    xpathResultSet    *result,
    int               *docOrder,
    char             **errMsg
)
{
    xpathResultSet   leftResult, rightResult;
    xpathResultSet  *pleftResult, *prightResult, tResult;
    int              i, j, k, rc, res, NaN, NaN1, switchResult, count = 0;
    domNode         *node, *child, *startingNode, *ancestor;
    domAttrNode     *attr;
    domNS           *ns;
    int              savedDocOrder, predLimit;
    int              left = 0, right = 0, useFastAdd;
    double           dLeft = 0.0, dRight = 0.0, dTmp;
    char            *leftStr = NULL, *rightStr = NULL;
    char            *localName, prefix[MAX_PREFIX_LEN];
    astType          savedAstType;

    if (result->type == EmptyResult) useFastAdd = 1;
    else useFastAdd = 0;

    switch (step->type) {

    case AxisChild:
        DBG(fprintf(stderr, "AxisChild ctxNode->nodeType = %d \n", 
                    ctxNode->nodeType);)
        *docOrder = 1;
        if (ctxNode->nodeType != ELEMENT_NODE) return XPATH_OK;
        DBG(fprintf(stderr, "AxisChild: scanning \n");)
        child = ctxNode->firstChild;
        if (step->intvalue) {
            while (child && (count < step->intvalue)) {
                DBG(fprintf(stderr, "AxisChild: child '%s' domNode0x%x \n", 
                            child->nodeName, child);)
                if (xpathNodeTest(child, exprContext, step)) {
                    DBG(fprintf(stderr, 
                      "AxisChild: after node taking child '%s' domNode0x%x \n",
                                child->nodeName, child);)
                    rsAddNodeFast( result, child);
                    count++;
                }
                child = child->nextSibling;
            }
        } else {
            while (child) {
                DBG(fprintf(stderr, "AxisChild: child '%s' domNode0x%x \n",
                            child->nodeName, child);)
                if (xpathNodeTest(child, exprContext, step)) {
                    DBG(fprintf(stderr,
                      "AxisChild: after node taking child '%s' domNode0x%x \n",
                                child->nodeName, child);)
                    rsAddNodeFast( result, child);
                }
                child = child->nextSibling;
            }
        }
        DBG( fprintf(stderr,"AxisChild result:\n");
             rsPrint(result);
        )
        break;

    case SlashSlash:
        if (step->intvalue) predLimit = 1;
        else predLimit = 0;
        predLimit = 0;
        xpathRSInit (&tResult);
        node = ctxNode->firstChild;
        while (node) {
            if (node->nodeType == ELEMENT_NODE) {
                rc = xpathEvalStep (step, node, exprContext, position,
                                    nodeList, cbs, result, docOrder, errMsg);
                if (rc) {
                    xpathRSFree (&tResult);
                    return rc;
                }
            }
            if (xpathNodeTest(node, exprContext, step)) {
                rsAddNodeFast( &tResult, node);
                if (predLimit) {
                    count++;
                    if (count >= step->intvalue) break;
                }
            }
            node = node->nextSibling;
        }
        if (node) {
            node = node->nextSibling;
            while (node) {
                if (node->nodeType == ELEMENT_NODE) {
                    rc = xpathEvalStep (step, node, exprContext, position,
                                        nodeList, cbs, result, docOrder,
                                        errMsg);
                    if (rc) {
                        xpathRSFree (&tResult);
                        return rc;
                    }
                }
                node = node->nextSibling;
            }
        }
        rc = xpathEvalPredicate (step->next, exprContext, result, &tResult,
                                  cbs, docOrder, errMsg);
        xpathRSFree (&tResult);
        CHECK_RC;
        break;

    case AxisDescendant:
    case AxisDescendantOrSelf:
        if (step->next && step->next->type == Pred) {
            *docOrder = 1;
            if (step->intvalue
                && (step->type == AxisDescendantLit 
                    || step->type == AxisDescendantOrSelfLit)) predLimit = 1;
            else predLimit = 0;
            if (ctxNode->nodeType == ATTRIBUTE_NODE
                && step->type == AxisDescendantOrSelf) {
                if (xpathNodeTest(ctxNode, exprContext, step))
                    checkRsAddNode( result, ctxNode);
                break;
            }
            if (ctxNode->nodeType != ELEMENT_NODE) return XPATH_OK;
            if (step->type == AxisDescendantOrSelf
                || step->type == AxisDescendantOrSelfLit) {
                if (xpathNodeTest(ctxNode, exprContext, step)) {
                    xpathRSInit (&tResult);
                    rsAddNodeFast( &tResult, ctxNode);
                    rc = xpathEvalPredicate (step->next, exprContext, result,
                                              &tResult, cbs, docOrder, errMsg);
                    xpathRSFree (&tResult);
                    CHECK_RC;
                }
            }
            savedAstType = step->type;
            step->type = SlashSlash;
            rc = xpathEvalStep (step, ctxNode, exprContext, position, 
                                nodeList, cbs, result, docOrder, errMsg);
            step->type = savedAstType;
            CHECK_RC;
            break;
        }
        /* whithout following Pred step, // is the same as 
           AxisDescendantOrSelf, fall throu */

    case AxisDescendantLit:
    case AxisDescendantOrSelfLit:
        *docOrder = 1;
        if (step->intvalue
            && (step->type == AxisDescendantLit 
                || step->type == AxisDescendantOrSelfLit)) predLimit = 1;
        else predLimit = 0;
        if (ctxNode->nodeType == ATTRIBUTE_NODE
            && (step->type == AxisDescendantOrSelf
                || step->type == AxisDescendantOrSelfLit)) {
            if (xpathNodeTest(ctxNode, exprContext, step))
                checkRsAddNode( result, ctxNode);
            break;
        }
        if (ctxNode->nodeType != ELEMENT_NODE) return XPATH_OK;
        if (step->type == AxisDescendantOrSelf
            || step->type == AxisDescendantOrSelfLit) {
            if (xpathNodeTest(ctxNode, exprContext, step)) {
                checkRsAddNode( result, ctxNode);
                if (predLimit) {
                    count++;
                }
            }
        }

        startingNode = ctxNode;
        node = ctxNode->firstChild;
        while (node && node != startingNode) {
            if (xpathNodeTest(node, exprContext, step)) {
                 checkRsAddNode( result, node);
                 if (predLimit) {
                     count++;
                     if (count >= step->intvalue) break;
                 }
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
                    (node->parentNode != startingNode) &&
                    (node->parentNode->nextSibling == NULL) ) {

                node = node->parentNode;
            }
            if ((node != startingNode) &&
                (node->parentNode)     &&
                (node->parentNode != startingNode)
                ) {
                node = node->parentNode->nextSibling;
            } else {
                break;
            }
        }
        break;

    case AxisSelf:
        *docOrder = 1;
        DBG(fprintf(stderr, "AxisSelf :: \n");)
        if (xpathNodeTest(ctxNode, exprContext, step)) {
            checkRsAddNode( result, ctxNode);
        }
        break;

    case GetContextNode:
        checkRsAddNode( result, ctxNode);
        break;

    case AxisAttribute:
        *docOrder = 1;
        DBG(fprintf(stderr, "AxisAttribute %s \n", step->child->strvalue);)
        if (ctxNode->nodeType != ELEMENT_NODE) return XPATH_OK;
        if (step->child->type == IsElement) {
            step->child->type = IsAttr;
        }
        if (step->child->type == IsAttr) {
            if (strcmp(step->child->strvalue, "*")==0) {
                attr = ctxNode->firstAttr;
                while (attr) {
                    if (!(attr->nodeFlags & IS_NS_NODE)) {
                        rsAddNodeFast (result, (domNode *)attr);
                    }
                    attr = attr->nextSibling;
                }
            } else {
                attr = ctxNode->firstAttr;
                while (attr && (attr->nodeFlags & IS_NS_NODE))
                    attr = attr->nextSibling;
                while (attr) {
                    if (xpathNodeTest( (domNode*)attr, exprContext, step)) {
                        rsAddNodeFast (result, (domNode *)attr);
                    }
                    attr = attr->nextSibling;
                }
            }
        } else
        if (step->child->type == IsNSAttr) {
            attr = ctxNode->firstAttr;
            while (attr && (attr->nodeFlags & IS_NS_NODE))
                attr = attr->nextSibling;
            while (attr) {
                if (xpathNodeTest ( (domNode*)attr, exprContext, step)) {
                    rsAddNodeFast (result, (domNode *)attr);
                }
                attr = attr->nextSibling;
            }
        }
        break;

    case AxisParent:
        *docOrder = 1;
        if (ctxNode->nodeType == ATTRIBUTE_NODE) {
            if (xpathNodeTest(((domAttrNode *)ctxNode)->parentNode,
                              exprContext, step)) {
                rsAddNode(result,((domAttrNode *)ctxNode)->parentNode);
            }
        } else {
            if (ctxNode->parentNode) {
                if (xpathNodeTest(ctxNode->parentNode, exprContext, step)) {
                    rsAddNode(result,ctxNode->parentNode);
                }
            } else {
                if (xpathNodeTest (ctxNode->ownerDocument->rootNode,
                                   exprContext, step)) {
                    rsAddNode (result, ctxNode->ownerDocument->rootNode);
                }
            }
        }
        break;

    case GetParentNode:
        if (ctxNode->nodeType == ATTRIBUTE_NODE) {
            rsAddNode(result,((domAttrNode*)ctxNode)->parentNode);
        } else {
            if (ctxNode->parentNode) {
                rsAddNode(result,ctxNode->parentNode);
            } else {
                if (ctxNode != ctxNode->ownerDocument->rootNode) {
                    rsAddNode (result, ctxNode->ownerDocument->rootNode);
                }
            }
        }
        break;

    case AxisAncestor:
    case AxisAncestorOrSelf:
        *docOrder = 0;
        xpathRSInit (&tResult);
        if (step->type == AxisAncestorOrSelf) {
            if (xpathNodeTest(ctxNode, exprContext, step))
                rsAddNodeFast(&tResult, ctxNode);
        }
        if (ctxNode->nodeType == ATTRIBUTE_NODE) {
            ctxNode = ((domAttrNode *)ctxNode)->parentNode;
            if (xpathNodeTest(ctxNode, exprContext, step))
                rsAddNodeFast(&tResult, ctxNode);
        }
        while (ctxNode->parentNode) {
            ctxNode = ctxNode->parentNode;
            if (xpathNodeTest(ctxNode, exprContext, step))
                rsAddNodeFast(&tResult, ctxNode);
        }
        if (xpathNodeTest (ctxNode->ownerDocument->rootNode, exprContext, 
                           step)) {
            rsAddNodeFast (&tResult, ctxNode->ownerDocument->rootNode);
        }
        for (i = tResult.nr_nodes - 1; i >= 0;  i--) {
            checkRsAddNode (result, tResult.nodes[i]);
        }
        xpathRSFree (&tResult);
        break;

    case AxisFollowingSibling:
        *docOrder = 1;
        if (ctxNode->nodeType == ATTRIBUTE_NODE) {
            return XPATH_OK;
        }
        if (step->intvalue) {
            while (ctxNode->nextSibling && (count < step->intvalue)) {
                ctxNode = ctxNode->nextSibling;
                if (xpathNodeTest(ctxNode, exprContext, step)) {
                    checkRsAddNode(result, ctxNode);
                    count++;
                }
            }
        } else {
            while (ctxNode->nextSibling) {
                ctxNode = ctxNode->nextSibling;
                if (xpathNodeTest(ctxNode, exprContext, step)) {
                    checkRsAddNode(result, ctxNode);
                }
            }
        }
        break;

    case AxisPrecedingSibling:
        *docOrder = 0;
        if (ctxNode->nodeType == ATTRIBUTE_NODE) {
            return XPATH_OK;
        }
        if (step->intvalue) {
            node = ctxNode->previousSibling;
            xpathRSInit (&tResult);
            while (node && (count < step->intvalue)) {
                if (xpathNodeTest (node, exprContext, step)) {
                    rsAddNodeFast (&tResult, node);
                    count++;
                }
                node = node->previousSibling;
            }
            for (i = tResult.nr_nodes -1; i >= 0; i--) {
                checkRsAddNode (result, tResult.nodes[i]);
            }
            xpathRSFree (&tResult);
        } else {
            startingNode = ctxNode;
            if (startingNode->parentNode) {
                node = (startingNode->parentNode)->firstChild;
            } else {
                if (ctxNode == ctxNode->ownerDocument->rootNode)
                    return XPATH_OK;
                node = startingNode->ownerDocument->rootNode->firstChild;
            }
            while (node && (node != startingNode)) {
                if (xpathNodeTest(node, exprContext, step)) {
                    checkRsAddNode(result, node);
                } 
                node = node->nextSibling;
            }
        }
        break;

    case AxisFollowing:
        *docOrder = 1;
        if (ctxNode->nodeType == ATTRIBUTE_NODE) {
            node = ((domAttrNode *)ctxNode)->parentNode->firstChild;
        } else {
            node = ctxNode;
            if (node->nextSibling) {
                node = node->nextSibling;
            } else {
                while (node->parentNode) {
                    node = node->parentNode;
                    if (node->nextSibling) break;
                }
                if (!node->nextSibling) return XPATH_OK;
                else node = node->nextSibling;
            }
        }
        while (1) {
            if (xpathNodeTest (node, exprContext, step)) {
                checkRsAddNode (result, node);
                if (step->intvalue) {
                    count++;
                    if (count >= step->intvalue) break;
                }
            }
            if (node->nodeType == ELEMENT_NODE &&
                node->firstChild) {
                node = node->firstChild;
            } else
            if (node->nextSibling) {
                node = node->nextSibling;
            } else {
                while (node->parentNode) {
                    node = node->parentNode;
                    if (node->nextSibling) break;
                }
                if (!node->nextSibling) break;
                node = node->nextSibling;
            }
        }
        break;

    case AxisPreceding:
        *docOrder = 0;
        if (ctxNode->nodeType == ATTRIBUTE_NODE) {
            ancestor = node = ((domAttrNode *)ctxNode)->parentNode;
        } else {
            ancestor = node = ctxNode;
        }
        i = 0;
        while (node->parentNode) {
            ancestor = node;
            node = node->parentNode;
            i++;
        }
        startingNode = node->firstChild;
        for (; i > 0; i--) {
            if (!startingNode)
                continue;
            if (startingNode->nodeType == ELEMENT_NODE) {
                node = startingNode->firstChild;
            } else {
                node = NULL;
            }
            while (startingNode != ancestor) {
                if (xpathNodeTest(startingNode, exprContext, step)) {
                    checkRsAddNode(result, startingNode);
                }
                while ((node) && (node != startingNode)) {
                   if (xpathNodeTest(node,exprContext, step)) {
                       checkRsAddNode(result, node);
                   }
                   if ((node->nodeType == ELEMENT_NODE) &&
                       (node->firstChild)) {
                       node = node->firstChild;
                       continue;
                   }
                   if (node->nextSibling) {
                       node = node->nextSibling;
                       continue;
                   }
                   while ((node->parentNode != startingNode) &&
                          (node->parentNode->nextSibling == NULL)) {

                       node = node->parentNode;
                   }
                   if (node->parentNode != startingNode) {
                       node = node->parentNode->nextSibling;
                   } else {
                       break;
                   }
                }
                startingNode = startingNode->nextSibling;
                if (startingNode->nodeType == ELEMENT_NODE) {
                    node = startingNode->firstChild;
                } else {
                    node = NULL;
                }
            }
            if (ctxNode->nodeType == ATTRIBUTE_NODE) {
                node = ((domAttrNode *)ctxNode)->parentNode;
            } else {
                node = ctxNode;
            }
            for (j = 0; j < i - 1 ; j++) {
                ancestor = node;
                node = node->parentNode;
            }
            if (node->nodeType == ELEMENT_NODE) {
                startingNode = node->firstChild;
            } else {
                startingNode = NULL;
            }
        }
        break;

    case AxisNamespace:
        *docOrder = 1;
        if (ctxNode->nodeType == ELEMENT_NODE) {
            node = ctxNode;
        } else
        if (ctxNode->nodeType == ATTRIBUTE_NODE) {
            node = ((domAttrNode*) ctxNode)->parentNode;
        } else return XPATH_OK;

        while (node) {
            attr = node->firstAttr;
            while (attr && (attr->nodeFlags & IS_NS_NODE)) {
                if (step->child->type == IsElement) {
                    if ((step->child->strvalue[0] != '*')) {
                        ns = domLookupPrefix (exprContext,
                                              step->child->strvalue);
                        if (!ns) {
                            /* This is more an error: the prefix should be
                               bound to a namespace. TODO: error msg. */
                            return XPATH_OK;
                        }
                        if (strcmp(attr->nodeValue, ns->uri)!=0) {
                            attr = attr->nextSibling;
                            continue;
                        }
                    }
                }
                rc = 0;
                for (i = 0; i < result->nr_nodes; i++) {
                    if (strcmp (attr->nodeName, ((domAttrNode*)result->nodes[i])->nodeName)==0) {
                        rc = 1; break;
                    }
                }
                if (rc) {attr = attr->nextSibling; continue;}
                rsAddNodeFast (result, (domNode *)attr);
                attr = attr->nextSibling;
            }

            if (node == node->ownerDocument->documentElement) {
                if (ctxNode != ctxNode->ownerDocument->rootNode) {
                    node = ctxNode->ownerDocument->rootNode;
                } else {
                    node = NULL;
                }
            } else {
                node = node->parentNode;
            }
        }
        break;

    case GetFQVar:
    case GetVar:
        if (cbs->varCB) {
            if (step->type == GetFQVar) {
                domSplitQName (step->strvalue, prefix, &localName);
                /* There must be a prefix, otherwise the Step type would
                   not be GetFQVar. Omit test prefix[0] != '\0'. */
                ns = domLookupPrefix (exprContext, prefix);
                if (!ns) {
                    *errMsg = tdomstrdup ("There isn't a namespace bound to the prefix.");
                    return XPATH_EVAL_ERR;
                }
                leftStr  = localName;
                rightStr = ns->uri;
            } else {
                leftStr  = step->strvalue;
                rightStr = NULL;
            }
            rc = (cbs->varCB)(cbs->varClientData, leftStr, rightStr, result,
                              errMsg);
            CHECK_RC;
        }
        break;

    case Literal:
        rsSetString (result, step->strvalue);
        break;

    case Int:
        rsSetInt (result, step->intvalue);
        break;

    case Real:
        rsSetReal (result, step->realvalue);
        break;

    case Add:
    case Substract:
    case Mult:
    case Div:
    case Mod:
        xpathRSInit (&leftResult);
        xpathRSInit (&rightResult);

        savedDocOrder = *docOrder;
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        CHECK_RC;
        DBG( fprintf(stderr,"left:\n");
             rsPrint(&leftResult);
        )

        *docOrder = savedDocOrder;
        rc = xpathEvalStep( step->child->next, ctxNode, exprContext,  position,
                            nodeList, cbs, &rightResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;

        DBG( fprintf(stderr,"right:\n");
             rsPrint(&rightResult);
        )
        if ((&leftResult)->type == RealResult) {
            dLeft = (&leftResult)->realvalue;
            NaN = 0;
            if (IS_NAN(dLeft)) NaN = 2;
            else if (IS_INF(dLeft)!=0) NaN = IS_INF(dLeft);
        } else {
            dLeft  = xpathFuncNumber(&leftResult, &NaN);
        }
        if ((&rightResult)->type == RealResult) {
            dRight = (&rightResult)->realvalue;
            NaN1 = 0;
            if (IS_NAN(dRight)) NaN1= 2;
            else if (IS_INF(dRight) !=0) NaN1 = IS_INF(dRight);
        } else {
            dRight = xpathFuncNumber(&rightResult, &NaN1);
        }
        if (NaN || NaN1) {
            if ((NaN == 2) || (NaN1 == 2)) {
                rsSetNaN (result);
            } else {
                switch (step->type) {
                case Substract:
                    NaN1 = -1 * NaN1;
                    /* fall throu */   
                case Add:
                    if (NaN == NaN1) {
                        if (NaN == 1) rsSetInf (result);
                        else          rsSetNInf (result);
                    } else if ((rc = NaN + NaN1) != 0) {
                        if (rc == 1)  rsSetInf (result);
                        else          rsSetNInf (result);
                    } else {
                        rsSetNaN (result);
                    }
                    break;
                case Mult:
                    if ((dLeft == 0.0) || (dRight == 0.0)) rsSetNaN (result);
                    else if (NaN && NaN1) {
                        rc = NaN * NaN1;
                        if (rc == 1)  rsSetInf (result);
                        else          rsSetNInf (result);
                    } else {
                        if (NaN) dTmp = NaN * dRight;
                        else     dTmp = NaN1 * dLeft;
                        if (dTmp > 0.0)  rsSetInf (result);
                        else             rsSetNInf (result);
                    }
                    break;
                case Div:
                    if (NaN && NaN1)   rsSetNaN (result);
                    else if (NaN1)     rsSetInt (result, 0);
                    else {
                        if (dRight == 0.0) dTmp = NaN;
                        else dTmp = NaN * dRight;
                        if (dTmp > 0.0)  rsSetInf (result);
                        else             rsSetNInf (result);
                    }
                    break;
                case Mod:
                    rsSetNaN (result);
                    break;
                default: break;
                }
            }
            xpathRSFree (&rightResult);
            xpathRSFree (&leftResult);
            return XPATH_OK;
        }
        switch (step->type) {
        case Add:       rsSetReal (result, dLeft + dRight); break;
        case Substract: rsSetReal (result, dLeft - dRight); break;
        case Mult:      rsSetReal (result, dLeft * dRight); break;
        case Div:
            if (dRight == 0.0) {
                if (dLeft == 0.0) {
                    rsSetNaN (result);
                } else {
                    if (dLeft > 0) {
                        rsSetInf (result);
                    } else {
                        rsSetNInf (result);
                    }
                }
            } else {
                rsSetReal (result, dLeft / dRight);
            }
            break;
        case Mod:
            if (dRight == 0.0) {
                rsSetNaN (result);
            } else {
                rsSetInt  (result, ((int)dLeft) % ((int)dRight));
            }
            break;
        default:        break;
        }
        xpathRSFree (&rightResult);
        xpathRSFree (&leftResult);
        return XPATH_OK;

    case CombineSets:
        xpathRSInit (&leftResult);
        xpathRSInit (&rightResult);

        savedDocOrder = *docOrder;
        rc = xpathEvalStep (step->child, ctxNode, exprContext, position, 
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        CHECK_RC;
        DBG( fprintf(stderr,"left:\n");
             rsPrint(&leftResult);
        )

        *docOrder = savedDocOrder;
        rc = xpathEvalStep( step->child->next, ctxNode, exprContext,  position,
                            nodeList, cbs, &rightResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;

        DBG( fprintf(stderr,"right:\n");
             rsPrint(&rightResult);
        )

        if (((leftResult.type != xNodeSetResult)
             && (leftResult.type != EmptyResult))
            ||
            ((rightResult.type != xNodeSetResult)
             && (rightResult.type != EmptyResult)))
        {
            *errMsg = tdomstrdup("| requires node sets!");
            xpathRSFree (&rightResult);
            xpathRSFree (&leftResult);
            return XPATH_EVAL_ERR;
        }
        if (leftResult.type == EmptyResult) {
            rsCopy (result, &rightResult);
            goto combineSetCleanup;
        }
        if (rightResult.type == EmptyResult) {
            rsCopy (result, &leftResult);
            goto combineSetCleanup;
        }
        *docOrder = 1;
        j = k = 0;
        for (i=0; i<(leftResult.nr_nodes+rightResult.nr_nodes); i++) {
            if (leftResult.nodes[j] == rightResult.nodes[k]) {
                rsAddNodeFast (result, leftResult.nodes[j]);
                j++; k++; 
                if (j == leftResult.nr_nodes) break;
                if (k == rightResult.nr_nodes) break;
                i++;
                continue;
            }
            if (domPrecedes (leftResult.nodes[j], rightResult.nodes[k])) {
                rsAddNodeFast (result, leftResult.nodes[j]);
                j++;
                if (j == leftResult.nr_nodes) break;
            } else {
                rsAddNodeFast (result, rightResult.nodes[k]);
                k++;
                if (k == rightResult.nr_nodes) break;
            }
        }

        if (j < leftResult.nr_nodes) {
            for (i=j; i< leftResult.nr_nodes; i++) {
                rsAddNodeFast ( result, leftResult.nodes[i]);
            }
        } else {
            for (i=k; i< rightResult.nr_nodes; i++) {
                rsAddNodeFast ( result, rightResult.nodes[i]);
            }
        }
    combineSetCleanup:
        xpathRSFree (&rightResult);
        xpathRSFree (&leftResult);
        return XPATH_OK;

    case And:
    case Or:
    case Equal:
    case NotEqual:
        xpathRSInit (&leftResult);
        xpathRSInit (&rightResult);

        savedDocOrder = *docOrder;
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position,
                            nodeList, cbs, &leftResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;

        DBG( fprintf(stderr,"left:\n");
             rsPrint(&leftResult);
        )

        /*----------------------------------------------
        |   short circuit evalution for AND/OR
        \---------------------------------------------*/
        if (step->type == And) {
            left = xpathFuncBoolean(&leftResult);
            if (!left) {
                /* left result is false, so AND result is also false */
                rsSetBool (result, left);
                xpathRSFree (&leftResult);
                return XPATH_OK;
            }
        }
        if (step->type == Or) {
            left = xpathFuncBoolean(&leftResult);
            if (left) {
                /* left result is true, so OR result is also true */
                rsSetBool (result, left);
                xpathRSFree (&leftResult);
                return XPATH_OK;
            }
        }

        rc = xpathEvalStep( step->child->next, ctxNode, exprContext,  position,
                            nodeList, cbs, &rightResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;

        DBG( fprintf(stderr,"right:\n");
             rsPrint(&rightResult);
        )
        res = 0;
        if ((step->type == And) || (step->type == Or)) {
            right = xpathFuncBoolean(&rightResult);
            if (step->type == And) res = (left && right);
            if (step->type == Or) res = (left || right);
            rsSetBool (result, res);
            xpathRSFree (&rightResult);
            xpathRSFree (&leftResult);
            return XPATH_OK;
        }

        if (   leftResult.type == xNodeSetResult
            || rightResult.type == xNodeSetResult) {
            if (leftResult.type == xNodeSetResult) {
                pleftResult = &leftResult;
                prightResult = &rightResult;
            } else {
                pleftResult = &rightResult;
                prightResult = &leftResult;
            }
            switch (prightResult->type) {
            case EmptyResult:
                res = 0;
                break;
            case xNodeSetResult:
                JDBG( fprintf(stderr,"\nleft+right result:\n");
                     rsPrint(pleftResult);
                     rsPrint(prightResult);
                )
                for (i=0; i < pleftResult->nr_nodes; i++) {
                    leftStr = xpathFuncStringForNode (pleftResult->nodes[i]);
                    for (j=0; j < prightResult->nr_nodes; j++) {
                        rightStr = xpathFuncStringForNode (prightResult->nodes[j]);
                        JDBG(fprintf(stderr, "leftStr='%s' rightStr='%s'\n", leftStr, rightStr);)
                        res = strcmp (leftStr, rightStr);
                        if      (step->type == Equal)    res = (res==0);
                        else if (step->type == NotEqual) res = (res!=0);
                        FREE(rightStr);
                        if (res) break;
                    }
                    FREE(leftStr);
                    if (res) break;
                }
                break;
            case BoolResult:
                if (step->type == Equal) res = (prightResult->intvalue != 0);
                else                     res = (prightResult->intvalue == 0);
                break;
            case IntResult:
            case RealResult:
                dRight = xpathFuncNumber (prightResult, &NaN);
                if (NaN) {
                    /* The real value of a node could never be inf/-inf */
                    /* And NaN is always != NaN (and != everything else) */
                    if (step->type == Equal) res = 0;
                    else                     res = 1;
                    break;
                }
                for (i=0; i < pleftResult->nr_nodes; i++) {
                    dLeft = xpathFuncNumberForNode (pleftResult->nodes[i], &NaN);
                    if (NaN) continue;
                    if (step->type == Equal) res = (dLeft == dRight);
                    else                     res = (dLeft != dRight);
                    if (res) break;
                }
                break;
            case NaNResult:
            case InfResult:
            case NInfResult:
                res = 0;
                break;
            case StringResult:
                rightStr = xpathFuncString (prightResult);
                for (i=0; i < pleftResult->nr_nodes; i++) {
                    leftStr = xpathFuncStringForNode (pleftResult->nodes[i]);
                    res = strcmp (leftStr, rightStr);
                    if (step->type == Equal) res = (res == 0);
                    else                     res = (res != 0);
                    FREE(leftStr);
                    if (res) break;
                }
                FREE(rightStr);
                break;
            }
        } else
        if (leftResult.type == BoolResult || rightResult.type == BoolResult) {
            left  = xpathFuncBoolean (&leftResult);
            right = xpathFuncBoolean (&rightResult);
            if (step->type == Equal) res = (left == right);
            else                     res = (left != right);
        } else
        if (
               leftResult.type  == IntResult
            || leftResult.type  == RealResult
            || rightResult.type == IntResult
            || rightResult.type == RealResult
            || rightResult.type == NaNResult
            || rightResult.type == InfResult
            || rightResult.type == NInfResult
            || leftResult.type  == NaNResult
            || leftResult.type  == InfResult
            || leftResult.type  == NInfResult
            ) {
            if (   leftResult.type == EmptyResult
                || rightResult.type == EmptyResult) {
                res = 0;
            } else {
                dLeft  = xpathFuncNumber (&leftResult, &NaN);
                dRight = xpathFuncNumber (&rightResult, &NaN1);
                if (NaN || NaN1) {
                    if (NaN == NaN1) {
                        if (NaN != 2) {
                            if (step->type == Equal) res = 1;
                        } else {
                            if (step->type == NotEqual) res = 1;
                        }
                    } else {
                        if (step->type == NotEqual) res = 1;
                    }
                } else {
                    if (step->type == Equal) res = (dLeft == dRight);
                    else                     res = (dLeft != dRight);
                }
            }
        } else {
            if (   leftResult.type == EmptyResult
                || rightResult.type == EmptyResult) {
                res = 0;
            } else {
                leftStr  = xpathFuncString (&leftResult);
                rightStr = xpathFuncString (&rightResult);
                res = strcmp (leftStr, rightStr);
                if (step->type == Equal) res = (res == 0);
                else                     res = (res != 0);
                FREE(leftStr);
                FREE(rightStr);
            }
        }
        rsSetBool (result, res);
        xpathRSFree (&rightResult);
        xpathRSFree (&leftResult);
        return XPATH_OK;

    case Less:
    case LessOrEq:
    case Greater:
    case GreaterOrEq:
        xpathRSInit (&leftResult);
        xpathRSInit (&rightResult);

        savedDocOrder = *docOrder;
        rc = xpathEvalStep( step->child, ctxNode, exprContext, position, nodeList,
                               cbs, &leftResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;

        DBG( fprintf(stderr,"left:\n");
             rsPrint(&leftResult);
        )
        rc = xpathEvalStep( step->child->next, ctxNode, exprContext,  position,
                            nodeList, cbs, &rightResult, docOrder, errMsg);
        CHECK_RC;
        *docOrder = savedDocOrder;

        DBG( fprintf(stderr,"right:\n");
             rsPrint(&rightResult);
        )
        res = 0;

        if (   leftResult.type == xNodeSetResult
            || rightResult.type == xNodeSetResult) {
            if (leftResult.type == xNodeSetResult) {
                pleftResult = &leftResult;
                prightResult = &rightResult;
                switchResult = 0;
            } else {
                pleftResult = &rightResult;
                prightResult = &leftResult;
                switchResult = 1;
            }
            switch (prightResult->type) {
            case EmptyResult:
                res = 0;
                break;
            case xNodeSetResult:
                JDBG( fprintf(stderr,"\nleft+right result:\n");
                     rsPrint(pleftResult);
                     rsPrint(prightResult);
                )
                for (i=0; i < pleftResult->nr_nodes; i++) {
                    dLeft = xpathFuncNumberForNode (pleftResult->nodes[i], &NaN);
                    if (NaN) continue;
                    for (j=0; j < prightResult->nr_nodes; j++) {
                        dRight = xpathFuncNumberForNode (prightResult->nodes[j], &NaN);
                        if (NaN)  continue;
                        if (switchResult) {
                            dTmp   = dLeft;
                            dLeft  = dRight;
                            dRight = dTmp;
                        }
                        if      (step->type == Less)     res = (dLeft < dRight);
                        else if (step->type == LessOrEq) res = (dLeft <= dRight);
                        else if (step->type == Greater)  res = (dLeft >  dRight);
                        else                             res = (dLeft >= dRight);

                        if (res) break;
                    }
                    if (res) break;
                }
                break;
            case BoolResult:
                /* pleftResult is a non-emtpy nodeset, therefor: */
                dLeft = 1.0;
                dRight = xpathFuncNumber (prightResult, &NaN);
                if (NaN) break;
                if      (step->type == Less)     res = (dLeft < dRight);
                else if (step->type == LessOrEq) res = (dLeft <= dRight);
                else if (step->type == Greater)  res = (dLeft >  dRight);
                else                             res = (dLeft >= dRight);

                if (switchResult) {
                    res = !res;
                }
                break;
            case NaNResult:
                break;
            case InfResult:
            case NInfResult:
            case IntResult:
            case RealResult:
            case StringResult:
                dRight = xpathFuncNumber (prightResult, &NaN);
                if (NaN) {
                    if (NaN == 2) break;
#ifdef DBL_MAX                    
                    if (NaN == 1) dRight = DBL_MAX;
                    else          dRight = -DBL_MAX;
#endif
                }
                for (i=0; i < pleftResult->nr_nodes; i++) {
                    dLeft = xpathFuncNumberForNode (pleftResult->nodes[i], &NaN);
                    if (NaN) continue;
                    if (switchResult) {
                        dTmp   = dLeft;
                        dLeft  = dRight;
                        dRight = dTmp;
                    }
                    if      (step->type == Less)     res = (dLeft < dRight);
                    else if (step->type == LessOrEq) res = (dLeft <= dRight);
                    else if (step->type == Greater)  res = (dLeft >  dRight);
                    else                             res = (dLeft >= dRight);

                    if (res) break;
                }
                break;
            }
        } else {
            if (   leftResult.type == EmptyResult
                || rightResult.type == EmptyResult) {
                res = 0;
            } else {
                dLeft  = xpathFuncNumber (&leftResult, &NaN);
                if (NaN) {
                    if (NaN == 2) goto compareFinish;
#ifdef DBL_MAX                    
                    if (NaN == 1) dLeft = DBL_MAX;
                    else          dLeft = -DBL_MAX;
#endif
                }
                dRight = xpathFuncNumber (&rightResult, &NaN);
                if (NaN) {
                    if (NaN == 2) goto compareFinish;
#ifdef DBL_MAX                    
                    if (NaN == 1) dRight = DBL_MAX;
                    else          dRight = -DBL_MAX;
#endif
                }
                if      (step->type == Less)     res = (dLeft < dRight);
                else if (step->type == LessOrEq) res = (dLeft <= dRight);
                else if (step->type == Greater)  res = (dLeft >  dRight);
                else                             res = (dLeft >= dRight);
            }
        }
    compareFinish:
        rsSetBool (result, res);
        xpathRSFree (&rightResult);
        xpathRSFree (&leftResult);
        return XPATH_OK;

    case SelectRoot:
        if (ctxNode->nodeType == ATTRIBUTE_NODE) {
            node = ((domAttrNode *)ctxNode)->parentNode;
            checkRsAddNode(result, node->ownerDocument->rootNode);
        } else {
            checkRsAddNode(result, ctxNode->ownerDocument->rootNode);
        }
        break;

    case UnaryMinus:
        xpathRSInit (&leftResult);
        rc = xpathEvalSteps (step->child, nodeList, ctxNode, exprContext,
                             position, docOrder,cbs, &leftResult, errMsg);
        CHECK_RC;
        dLeft = xpathFuncNumber (&leftResult, &NaN);
        if (NaN) {
            if (NaN == 2)      rsSetNaN (result);
            else if (NaN == 1) rsSetNInf (result);
            else               rsSetInf (result);
        } else rsSetReal (result , -1 * dLeft);
        xpathRSFree (&leftResult);
        break;

    case EvalSteps:
        rc = xpathEvalSteps (step->child, nodeList, ctxNode, exprContext, position,
                             docOrder,cbs, &leftResult, errMsg);
        CHECK_RC;
        if ((result->type != EmptyResult) && (leftResult.type != result->type)) {
            DBG( fprintf (stderr, "EvalSteps:\nresult:\n");
            rsPrint (result);
            fprintf (stderr, "leftResult:\n");
            rsPrint (&leftResult); )
            *errMsg = tdomstrdup("can not merge different result types!");
            return XPATH_EVAL_ERR;
        }
        switch (leftResult.type) {
            case xNodeSetResult:
                for (i=0; i<leftResult.nr_nodes; i++) {
                    DBG(fprintf(stderr, "EvalSteps: adding %d \n", i);)
                    checkRsAddNode (result, leftResult.nodes[i]);
                }
                break;
            case BoolResult:   rsSetBool(result, leftResult.intvalue);
                               break;
            case IntResult:    rsSetInt(result, leftResult.intvalue);
                               break;
            case RealResult:   rsSetReal(result, leftResult.realvalue);
                               break;
            case StringResult: rsSetString (result, leftResult.string);
                               break;
            case NaNResult:    rsSetNaN (result); break;
            case InfResult:    rsSetInf (result); break;
            case NInfResult:   rsSetNInf (result); break;
            default:           break;
        }
        xpathRSFree( &leftResult  );
        break;

    case ExecFunction:
    case ExecIdKey:
        rc = xpathEvalFunction (step, ctxNode, exprContext, position, nodeList,
                                cbs, result, docOrder, errMsg);
        CHECK_RC;
        break;
    default:
        fprintf (stderr, "!! xpathEvalStep: not handled switch %s !!\n", astType2str[step->type]);
        return XPATH_EVAL_ERR;
    }
    return XPATH_OK;

} /* xpathEvalStep */

/*----------------------------------------------------------------------------
|   xpathEvalPredicate
|
\---------------------------------------------------------------------------*/
static int xpathEvalPredicate (
    ast                step,
    domNode           *exprContext,
    xpathResultSet    *result,
    xpathResultSet    *stepResult,
    xpathCBs          *cbs,
    int               *docOrder,
    char              **errMsg
)
{
    xpathResultSet predResult, tmpResult;
    int            i, rc, savedDocOrder, useFastAdd;

    if (result->nr_nodes == 0) useFastAdd = 1;
    else useFastAdd = 0;
    savedDocOrder = *docOrder;
    while (step && step->type == Pred) {
        xpathRSInit (&tmpResult);
        if (step->child->type == Int) {
            if (stepResult->nr_nodes >= step->child->intvalue
                && step->child->intvalue > 0) {
                if (*docOrder) {
                    rsAddNode (&tmpResult, 
                               stepResult->nodes[step->child->intvalue - 1]);
                } else {
                    rsAddNode (&tmpResult, 
                               stepResult->nodes[stepResult->nr_nodes - 
                                                 step->child->intvalue]);
                }
            }
            goto nextPred;
        }
        for (i=0; i<stepResult->nr_nodes; i++) {
            xpathRSInit (&predResult);
            rc = xpathEvalStep( step->child, stepResult->nodes[i],
                                exprContext, i, stepResult, cbs, &predResult,
                                docOrder, errMsg);
            CHECK_RC;
            *docOrder = savedDocOrder;
            DBG( fprintf(stderr, "after eval for Predicate: \n"); )
            DBG( rsPrint( &predResult); )

            if (predResult.type == RealResult) {
                predResult.type = IntResult;
                predResult.intvalue = xpathRound(predResult.realvalue);
            }
            if (predResult.type == IntResult) {
                if (predResult.intvalue < 0) {
                    predResult.intvalue = 
                        stepResult->nr_nodes + predResult.intvalue;
                }
                if (savedDocOrder) {
                    if (predResult.intvalue == (i+1)) {
                        rsAddNodeFast (&tmpResult, stepResult->nodes[i]);
                    }
                } else {
                    if (predResult.intvalue == (stepResult->nr_nodes - i)){
                        rsAddNodeFast (&tmpResult, stepResult->nodes[i]);
                    }
                }
            } else if (xpathFuncBoolean(&predResult)) {
                rsAddNodeFast (&tmpResult, stepResult->nodes[i]);
            }
            xpathRSFree (&predResult);
        }
    nextPred:
        DBG( fprintf(stderr, "result after Predicate: \n"); )
        DBG( rsPrint( &tmpResult); )
        xpathRSFree( stepResult );
        *stepResult = tmpResult;
        step = step->next;
    }

    /* add remaining result set to overall result set */
    for (i=0; i<stepResult->nr_nodes; i++) {
        checkRsAddNode (result, stepResult->nodes[i]);
    }

    return 0;
}


/*----------------------------------------------------------------------------
|   xpathEvalStepAndPredicates
|
\---------------------------------------------------------------------------*/
static int xpathEvalStepAndPredicates (
    ast                steps,
    xpathResultSet    *nodeList,
    domNode           *currentNode,
    domNode           *exprContext,
    int                currentPos,
    int               *docOrder,
    xpathCBs          *cbs,
    xpathResultSet    *result,
    char              **errMsg
)
{
    xpathResultSet  stepResult;
    int             rc;


    /* For AxisDescendantOrSelf/AxisDescendant, the predicate filtering
       is already done 'inlined' during xpathEvalStep, to do the filtering
       "with respect to the child axis" (XPath rec. 3.3) in an efficienter
       way, then up to now. */
    if (   steps->next 
        && steps->next->type == Pred
        && steps->type != AxisDescendantOrSelf 
        && steps->type != AxisDescendant) {
        xpathRSInit (&stepResult);
        rc = xpathEvalStep( steps, currentNode, exprContext, currentPos,
                            nodeList, cbs, &stepResult, docOrder, errMsg);
        if (rc) {
            xpathRSFree (&stepResult);
            return rc;
        }
        rc = xpathEvalPredicate (steps->next, exprContext, result, &stepResult,
                                 cbs, docOrder, errMsg);
        xpathRSFree (&stepResult);
        CHECK_RC;
    } else {
        /* for steps not followed by a predicate immediately add to
           the final result set */
        rc = xpathEvalStep( steps, currentNode, exprContext, currentPos, nodeList,
                            cbs, result, docOrder, errMsg);
        CHECK_RC;
        DBG( rsPrint( result); )
    }
    return 0;
}


/*----------------------------------------------------------------------------
|   xpathEvalSteps
|
\---------------------------------------------------------------------------*/
int xpathEvalSteps (
    ast                steps,
    xpathResultSet    *nodeList,
    domNode           *currentNode,
    domNode           *exprContext,
    int                currentPos,
    int               *docOrder,
    xpathCBs          *cbs,
    xpathResultSet    *result,
    char              **errMsg
)
{
    int i, rc, first = 1;
    xpathResultSet savedContext;

    DBG (fprintf (stderr, "xpathEvalSteps start\n");)
    savedContext = *nodeList;
    xpathRSInit (result);
    while (steps) {
        DBG (fprintf (stderr, "xpathEvalSteps: eval step '%s'\n", 
                      astType2str[steps->type]);)
        if (steps->type == Pred) {
            *errMsg = "Pred step not expected now!";
            return XPATH_EVAL_ERR;
        }
        if (first) {
            rc = xpathEvalStepAndPredicates (steps, nodeList, currentNode,
                                             exprContext, currentPos, docOrder,
                                             cbs, result, errMsg);
            CHECK_RC;
            first = 0;
        } else {
            DBG( fprintf(stderr, "doing location step nodeList->nr_nodes=%d \n",
                                 nodeList->nr_nodes);
            )
            if (result->type != xNodeSetResult) {
                xpathRSFree (result);
                xpathRSInit (result);
                *nodeList = savedContext;
                return 0;
            }

            *nodeList = *result;
            xpathRSInit (result);
            for (i=0; i<nodeList->nr_nodes; i++) {
                rc = xpathEvalStepAndPredicates (steps, nodeList,
                                                 nodeList->nodes[i],
                                                 exprContext, i, docOrder, cbs,
                                                 result, errMsg);
                if (rc) {
                    xpathRSFree (result);
                    xpathRSFree (nodeList);
                    return rc;
                }
            }
            xpathRSFree (nodeList);
        }
        DBG( fprintf(stderr, "result after location step: \n"); )
        DBG( rsPrint( result); )

        steps = steps->next;
        /* skip the already processed Predicate parts */
        while (steps && steps->type == Pred) steps = steps->next;
        *docOrder = 1;
    }
    *nodeList = savedContext;
    return 0;
}


/*----------------------------------------------------------------------------
|   xpathEval
|
\---------------------------------------------------------------------------*/
int xpathEval (
    domNode          * node,
    domNode          * exprContext,
    char             * xpath,
    xpathCBs         * cbs,
    char            ** errMsg,
    xpathResultSet   * result
)
{
    xpathResultSet nodeList;
    int            rc, docOrder = 1;
    ast            t;

    *errMsg = NULL;
    rc = xpathParse(xpath, errMsg, &t, 0);
    CHECK_RC;

    xpathRSInit( &nodeList);
    rsAddNodeFast( &nodeList, node);

    rc = xpathEvalSteps( t, &nodeList, node, exprContext, 1, &docOrder, cbs,
                         result, errMsg);
    freeAst(t);
    xpathRSFree( &nodeList );
    CHECK_RC;

    DBG(rsPrint( result );)
    return 0;

} /* xpathEval */

/*----------------------------------------------------------------------------
|   xpathMatches
|
\---------------------------------------------------------------------------*/
int xpathMatches (
    ast                 step,
    domNode           * exprContext,
    domNode           * nodeToMatch,
    xpathCBs          * cbs,
    char             ** errMsg
)
{
    xpathResultSet  stepResult, nodeList, newNodeList;
    ast             childSteps;
    int             rc, i, j, currentPos = 0, nodeMatches, docOrder = 1;
    int             useFastAdd;
    char           *localName = NULL, *nodeUri;
    domAttrNode    *attr;
    domNode        *child;
    domNS          *contextNS;

    DBG(printAst(3,step));
    xpathRSInit (&nodeList);
    while (step) {
        TRACE1("xpathMatches type=%d \n", step->type);
        if (nodeList.nr_nodes == 0) useFastAdd = 1;
        else useFastAdd = 0;
        switch (step->type) {

            case AxisAttribute:
                if (step->child->type != IsAttr && step->child->type != IsNSAttr) {
                    if (step->child->type == IsElement) {
                        step->child->type = IsAttr;
                    } else {
                        DBG(fprintf(stderr, "strange: AxisAttribute with no IsAttr below!\n");)
                        xpathRSFree (&nodeList); return 0;
                    }
                }
                attr = NULL;
                if (nodeToMatch->nodeType == ATTRIBUTE_NODE) {
                    rc = xpathMatches (step->child, exprContext, nodeToMatch, cbs,
                                       errMsg);
                    DBG(fprintf(stderr, "rc=%d attribute match\n", rc); )
                    if (rc != 1) { xpathRSFree (&nodeList); return 0; }
                    attr = (domAttrNode*) nodeToMatch;
                } else { xpathRSFree (&nodeList); return 0; }
                if (attr == NULL) { xpathRSFree (&nodeList); return 0; }
                break;

            case AxisChild:
                if (step->child->type == IsElement) {
                    if (nodeToMatch->nodeType != ELEMENT_NODE) {
                        xpathRSFree (&nodeList); return 0;
                    }
                    if (nodeToMatch == nodeToMatch->ownerDocument->rootNode) {
                        xpathRSFree (&nodeList); return 0;
                    }
                    if ((step->child->strvalue[0] != '*') ||
                        (step->child->strvalue[1] != '\0'))
                    {
                        if (nodeToMatch->namespace) return 0;
                        if (strcmp(nodeToMatch->nodeName, step->child->strvalue)!=0) {
                            xpathRSFree (&nodeList); return 0;
                        }
                    }
                    break;
                }
                if (step->child->type == IsFQElement) {
                    if (nodeToMatch->nodeType != ELEMENT_NODE) {
                        xpathRSFree (&nodeList); return 0;
                    }
                    contextNS = domLookupPrefix (exprContext, step->child->strvalue);
                    if (!contextNS) { xpathRSFree (&nodeList); return 0; }
                    nodeUri = domNamespaceURI (nodeToMatch);
                    if (!nodeUri) { xpathRSFree (&nodeList); return 0; }
                    if (strcmp (contextNS->uri, nodeUri) != 0) {
                        xpathRSFree (&nodeList); return 0;
                    }
                    localName = domGetLocalName (nodeToMatch->nodeName);
                    if (!localName) { xpathRSFree (&nodeList); return 0; }
                    if (strcmp (step->child->child->strvalue, localName)!=0) {
                        xpathRSFree (&nodeList); return 0;
                    }
                    break;
                }
                if (step->child->type == IsNSElement) {
                    contextNS = domLookupPrefix (exprContext, step->child->strvalue);
                    if (!contextNS) { xpathRSFree (&nodeList); return 0; }
                    nodeUri = domNamespaceURI (nodeToMatch);
                    if (!nodeUri) { xpathRSFree (&nodeList); return 0; }
                    if (strcmp (contextNS->uri, nodeUri)!=0) {
                        xpathRSFree (&nodeList); return 0;
                    }
                    break;
                }
                DBG(fprintf(stderr, "strange: AxisChild with no IsElement, IsFQElement or IsNSElement below!\n");)
                return 0;

            case IsElement:
                if (nodeToMatch->nodeType != ELEMENT_NODE) {
                    xpathRSFree (&nodeList); return 0;
                }
                if (nodeToMatch == nodeToMatch->ownerDocument->rootNode) {
                    xpathRSFree (&nodeList); return 0;
                }
                if ((step->strvalue[0] != '*') ||
                    (step->strvalue[1] != '\0'))
                {
                    if (nodeToMatch->namespace) return 0;
                    if (strcmp(nodeToMatch->nodeName, step->strvalue)!=0) {
                        xpathRSFree (&nodeList); return 0;
                    }
                }
                break;

            case IsFQElement:
                if (nodeToMatch->nodeType != ELEMENT_NODE) {
                    xpathRSFree (&nodeList); return 0;
                }
                contextNS = domLookupPrefix (exprContext, step->strvalue);
                if (!contextNS) { xpathRSFree (&nodeList); return 0; }
                nodeUri = domNamespaceURI (nodeToMatch);
                if (!nodeUri) { xpathRSFree (&nodeList); return 0; }
                if (strcmp (contextNS->uri, nodeUri) != 0) {
                    xpathRSFree (&nodeList); return 0;
                }
                localName = domGetLocalName (nodeToMatch->nodeName);
                if (!localName) { xpathRSFree (&nodeList); return 0; }
                if (strcmp (step->child->strvalue, localName)!=0)  {
                    xpathRSFree (&nodeList); return 0;
                }
                break;

            case IsNSElement:
                contextNS = domLookupPrefix (exprContext, step->strvalue);
                if (!contextNS) { xpathRSFree (&nodeList); return 0; }
                nodeUri = domNamespaceURI (nodeToMatch);
                if (!nodeUri) { xpathRSFree (&nodeList); return 0; }
                if (strcmp (contextNS->uri, nodeUri)!=0) {
                    xpathRSFree (&nodeList); return 0;
                }
                break;

            case IsAttr:
                if ((nodeToMatch->nodeType != ATTRIBUTE_NODE)
                    || (nodeToMatch->nodeFlags & IS_NS_NODE)) {
                    xpathRSFree (&nodeList); return 0;
                }
                if (!((step->strvalue[0] == '*') && (step->strvalue[1] == '\0')))  {
                    if (strcmp( ((domAttrNode*)nodeToMatch)->nodeName, step->strvalue)!=0) {
                        xpathRSFree (&nodeList); return 0;
                    }
                }
                break;

            case IsNSAttr:
                if ((nodeToMatch->nodeType != ATTRIBUTE_NODE)
                    || (nodeToMatch->nodeFlags & IS_NS_NODE)) {
                    xpathRSFree (&nodeList); return 0;
                }
                contextNS = domLookupPrefix (exprContext, step->strvalue);
                if (!contextNS) { xpathRSFree (&nodeList); return 0; }
                nodeUri = domNamespaceURI (nodeToMatch);
                if (!nodeUri) { xpathRSFree (&nodeList); return 0; }
                if (strcmp (contextNS->uri, nodeUri) != 0) {
                    xpathRSFree (&nodeList); return 0;
                }
                if (strcmp (step->child->strvalue, "*")==0) break;
                localName = domGetLocalName (((domAttrNode *)nodeToMatch)->nodeName);
                if (!localName) { xpathRSFree (&nodeList); return 0; }
                if (strcmp (step->child->strvalue, localName)!=0)  {
                    xpathRSFree (&nodeList); return 0;
                }
                break;

            case IsNode:
                DBG(fprintf(stderr, "IsNode: nodeTpye=%d \n", nodeToMatch->nodeType);)
                if (nodeToMatch->nodeType == ATTRIBUTE_NODE) {
                    xpathRSFree (&nodeList); return 0;
                }
                if ((nodeToMatch->nodeType == ELEMENT_NODE) &&
                    (nodeToMatch->ownerDocument->rootNode == nodeToMatch)) {
                    xpathRSFree (&nodeList); return 0;
                }
                break;

            case IsText:
                if (nodeToMatch->nodeType != TEXT_NODE) {
                    xpathRSFree (&nodeList); return 0;
                }
                break;

            case IsPI:
                if (nodeToMatch->nodeType != PROCESSING_INSTRUCTION_NODE) {
                    xpathRSFree (&nodeList); return 0;
                }
                break;

            case IsSpecificPI:
                if (nodeToMatch->nodeType != PROCESSING_INSTRUCTION_NODE) {
                    xpathRSFree (&nodeList); return 0;
                }
                if (strncmp(((domProcessingInstructionNode*)nodeToMatch)->targetValue, step->strvalue, ((domProcessingInstructionNode*)nodeToMatch)->targetLength) != 0) {
                    xpathRSFree (&nodeList); return 0;
                }
                break;

            case IsComment:
                if (nodeToMatch->nodeType != COMMENT_NODE) {
                    xpathRSFree (&nodeList); return 0;
                }
                break;

            case IsRoot:
                if (nodeToMatch->nodeType == ATTRIBUTE_NODE) {
                    xpathRSFree (&nodeList); return 0;
                }
                if (nodeToMatch != nodeToMatch->ownerDocument->rootNode) {
                    xpathRSFree (&nodeList); return 0;
                }
                break;

            case ToParent:
                if (nodeToMatch->nodeType == ATTRIBUTE_NODE) {
                    nodeToMatch = ((domAttrNode *)nodeToMatch)->parentNode;
                    break;
                }
                if (nodeToMatch == nodeToMatch->ownerDocument->rootNode) {
                    xpathRSFree (&nodeList); return 0;
                }
                if (nodeToMatch->parentNode) {
                    nodeToMatch = nodeToMatch->parentNode;
                } else {
                    nodeToMatch = nodeToMatch->ownerDocument->rootNode;
                }
                break;

            case ToAncestors:
                if (step->next == NULL) { xpathRSFree (&nodeList); return 1;}
                while (1) {
                    if (nodeToMatch->nodeType == ATTRIBUTE_NODE) {
                        nodeToMatch = ((domAttrNode *)nodeToMatch)->parentNode;
                    } else {
                        if (nodeToMatch == nodeToMatch->ownerDocument->rootNode) {
                            xpathRSFree (&nodeList); return 0;
                        }
                        if (nodeToMatch->parentNode) {
                            nodeToMatch = nodeToMatch->parentNode;
                        } else {
                            nodeToMatch = nodeToMatch->ownerDocument->rootNode;
                        }
                    }
                    if (step->next == NULL) {
                        xpathRSFree (&nodeList); return 0;
                    }
                    rc = xpathMatches (step->next, exprContext, nodeToMatch,
                                       cbs, errMsg);
                    if (rc == 1) break;
                }
                break;

            case FillWithCurrentNode:
                checkRsAddNode( &nodeList, nodeToMatch);
                currentPos = 0;
                DBG(
                  fprintf(stderr, "FillWithCurrentNode:\n");
                  rsPrint(&nodeList);
                )
                break;

            case FillNodeList:
                if (nodeToMatch->nodeType == ATTRIBUTE_NODE) {
                    child = (domNode*)  ((domAttrNode*)nodeToMatch)->parentNode->firstAttr;
                    DBG(fprintf(stderr, "FillNodeList all attribute\n");)
                } else {
                    if (nodeToMatch->ownerDocument->rootNode == nodeToMatch) {
                        xpathRSFree (&nodeList); return 0;
                    }
                    DBG(if (nodeToMatch->parentNode) {
                            fprintf(stderr, "FillNodeList on %s/%s...\n", nodeToMatch->parentNode->nodeName,nodeToMatch->nodeName);
                        } else {
                            fprintf(stderr, "FillNodeList on (null)/%s...\n", nodeToMatch->nodeName);
                        }
                        fprintf (stderr, "Current nodeList is:\n");
                        rsPrint (&nodeList);
                    )
                    if (nodeToMatch->parentNode == NULL) {
                        child = nodeToMatch->ownerDocument->rootNode->firstChild;
                    } else {
                        child = nodeToMatch->parentNode->firstChild;
                    }
                }
                currentPos = -1;
                i = 0;
                while (child) {
                    rc = xpathMatches (step->child, exprContext, child, cbs, errMsg);
                    if (rc == 1) {
                        checkRsAddNode( &nodeList, child);
                        if (child == nodeToMatch) currentPos = i;
                        i++;
                        if (step->intvalue && i >= step->intvalue) break;
                    }
                    if (child->nodeType == ATTRIBUTE_NODE) {
                        child = (domNode*) ((domAttrNode*)child)->nextSibling;
                    } else {
                        child = child->nextSibling;
                    }
                }
                DBG(
                  rsPrint(&nodeList);
                  fprintf(stderr, "currentPos = %d \n", currentPos);
                )
                break;

            case Pred:
                xpathRSInit (&stepResult);
                DBG(fprintf(stderr, "Befor Pred inner EvalStep. currentPos = %d\n", currentPos);)
                rc = xpathEvalStep (step->child, nodeToMatch, exprContext,
                                    currentPos, &nodeList, cbs, &stepResult,
                                    &docOrder, errMsg);
                CHECK_RC;
                DBG(
                  fprintf(stderr, "Pred inner EvalStep returned\n");
                  rsPrint(&stepResult);
                )
                nodeMatches = 0;

                if (stepResult.type == RealResult) {
                    stepResult.type = IntResult;
                    stepResult.intvalue = xpathRound(stepResult.realvalue);
                }
                if (stepResult.type == IntResult) {
                    if (stepResult.intvalue < 0) {
                        stepResult.intvalue = nodeList.nr_nodes + stepResult.intvalue;
                    }
                    if ((stepResult.intvalue > 0 ) &&
                        (stepResult.intvalue <= nodeList.nr_nodes) &&
                        (stepResult.intvalue == (currentPos+1)) ) {
                         nodeMatches = 1;
                    }
                } else if (xpathFuncBoolean(&stepResult)) {
                    nodeMatches = 1;
                }
                xpathRSFree (&stepResult);

                /* if nodeMatches == false we don't have to continue to filter */
                if (!nodeMatches) {
                    xpathRSFree (&nodeList); return 0;
                }

                /* if the nr_nodes of nodeList is > 1 (ie. we filter a
                   FillNodeList step, not only a FillWithCurrentNode
                   step), build the resulting nodeList context after
                   this predicate */

                if (nodeList.nr_nodes > 1) {
                    xpathRSInit (&newNodeList);
                    currentPos = -1;
                    j = 0;
                    for (i = 0; i < nodeList.nr_nodes; i++) {
                        xpathRSInit (&stepResult);
                        docOrder = 1;
                        rc = xpathEvalStep (step->child, nodeList.nodes[i],
                                            exprContext, i, &nodeList, cbs,
                                            &stepResult, &docOrder, errMsg);
                        if (rc) {
                            xpathRSFree (&stepResult);
                            xpathRSFree (&nodeList);
                            return rc;
                        }

                        nodeMatches = 0;

                        if (stepResult.type == RealResult) {
                            stepResult.type = IntResult;
                            stepResult.intvalue = 
                                xpathRound(stepResult.realvalue);
                        }
                        if (stepResult.type == IntResult) {
                            if (stepResult.intvalue < 0) {
                                stepResult.intvalue = 
                                    nodeList.nr_nodes + stepResult.intvalue;
                            }
                            if ((stepResult.intvalue > 0 ) &&
                                (stepResult.intvalue <= nodeList.nr_nodes) &&
                                (stepResult.intvalue == (currentPos+1)) ) {
                                nodeMatches = 1;
                            }
                        } else if (xpathFuncBoolean(&stepResult)) {
                            nodeMatches = 1;
                        }
                        if (nodeMatches) {
                            checkRsAddNode (&newNodeList, nodeList.nodes[i]);
                            if (nodeList.nodes[i] == nodeToMatch) {
                                currentPos = j;
                            }
                            j++;
                        }
                        xpathRSFree (&stepResult);
                    }
                    xpathRSFree (&nodeList);
                    nodeList = newNodeList;
                }
                break;

            case CombinePath:
                childSteps = step->child;
                while (childSteps) {
                    rc = xpathMatches (childSteps->child, exprContext,
                                       nodeToMatch, cbs, errMsg);
                    if (rc == 1) break;
                    childSteps = childSteps->next;
                }
                if (childSteps == NULL) {
                    xpathRSFree (&nodeList); return 0;
                }
                break;

            case ExecIdKey:
                xpathRSInit (&stepResult);
                rc = xpathEvalStep (step, nodeToMatch, exprContext,
                                    currentPos, &nodeList, cbs, &stepResult,
                                    &docOrder, errMsg);
                CHECK_RC;
                if (stepResult.type != xNodeSetResult) {
                    xpathRSFree (&stepResult);
                    xpathRSFree (&nodeList); return 0;
                }
                nodeMatches = 0;
                for (i = 0; i < stepResult.nr_nodes; i++) {
                    if (nodeToMatch == stepResult.nodes[i]) {
                        nodeMatches = 1;
                        break;
                    }
                }
                xpathRSFree (&stepResult);
                if (!nodeMatches) { xpathRSFree (&nodeList); return 0; }
                break;

            default:
                DBG(
                fprintf(stderr, "wrong type %d for xpathMatches \n", step->type);
                fprintf(stderr, "AST:\n");
                )
                printAst (0, step);
                xpathRSFree (&nodeList); return 0;
                break;
        }
        step = step->next;
    }
    xpathRSFree (&nodeList);
    return 1;
}

/*----------------------------------------------------------------------------
|   xpathGetPrio
|
\---------------------------------------------------------------------------*/
double xpathGetPrio (
    ast steps
)
{
    double prio, max;

    if (!steps) return 0.0;

    DBG(printAst(0, steps);)

    if (steps->next == NULL) {
        if (steps->type == IsElement) {
            if (strcmp(steps->strvalue, "*")==0) {
                return -0.5;
            } else {
                return 0.0;
            }
        } else
        if (steps->type == IsFQElement) {
            return 0.0;
        } else
        if (steps->type == IsNSElement) {
            return -0.25;
        } else
        if (steps->type == IsAttr) {
            if (strcmp(steps->strvalue, "*")==0) {
                return -0.5;
            } else {
                return 0.0;
            }
        } else
        if (steps->type == IsNSAttr) {
            if (strcmp(steps->child->strvalue, "*")==0) {
                return -0.25;
            } else {
                return 0.0;
            }
        } else
        if ( steps->type == IsNode
             || steps->type == IsText
             || steps->type == IsPI
             || steps->type == IsComment
             || steps->type == IsSpecificPI
            ) {
            return -0.5;
        } else 
        if ( steps->type == AxisChild
             || steps->type == AxisAttribute
             || steps->type == EvalSteps
            ) {
            return (xpathGetPrio (steps->child));
        }
    }
    return 0.5;

} /* xpathGetPrio */


/*----------------------------------------------------------------------------
|   nodeToXPath  -  returns a XPath addressing exactly the given node
|
\---------------------------------------------------------------------------*/
static void nodeToXPath (
    domNode  * node,
    char    ** xpath,
    int      * xpathLen,
    int      * xpathAllocated
)
{
    domNode *parent, *child;
    char    step[200], *nTest;
    int     sameNodes, nodeIndex, len;


    parent = node->parentNode;
    if (parent == NULL) {

        sprintf(step, "/%s", node->nodeName);

    } else {

        nodeToXPath (parent, xpath, xpathLen, xpathAllocated);

        step[0] = '\0';
        switch (node->nodeType) {

            case ELEMENT_NODE:
                nodeIndex = 0;
                sameNodes = 0;
                child = parent->firstChild;
                while (child) {
                    if (strcmp(child->nodeName, node->nodeName)==0) {
                        sameNodes++;
                        if (node == child) nodeIndex = sameNodes;
                        if ((nodeIndex != 0) && (sameNodes > 2)) break;
                    }
                    child = child->nextSibling;
                }
                if (sameNodes == 1) {
                    sprintf(step, "/%s", node->nodeName);
                } else {
                    sprintf(step, "/%s[%d]", node->nodeName, nodeIndex);
                }
                break;

            case TEXT_NODE:
            case COMMENT_NODE:
            case PROCESSING_INSTRUCTION_NODE:
                nodeIndex = 0;
                sameNodes = 0;
                child = parent->firstChild;
                while (child) {
                    if (child->nodeType == node->nodeType) {
                        sameNodes++;
                        if (node == child) nodeIndex = sameNodes;
                        if ((nodeIndex != 0) && (sameNodes > 2)) break;
                    }
                    child = child->nextSibling;
                }
                switch (node->nodeType) {
                    case TEXT_NODE:                   nTest = "text()";  break;
                    case COMMENT_NODE:                nTest = "comment"; break;
                    case PROCESSING_INSTRUCTION_NODE: nTest = "processing-instruction"; break;
                    default:                          nTest = "unknownNodeType()";
                }
                if (sameNodes == 1) {
                    sprintf(step, "/child::%s", nTest);
                } else {
                    sprintf(step, "/child::%s[%d]", nTest, nodeIndex);
                }
                break;

            default:
                break;
        }
    }
    len = strlen(step);
    if ( (len + *xpathLen) > *xpathAllocated ) {
        *xpathAllocated = *xpathAllocated * 2;
        *xpath = REALLOC(*xpath, *xpathAllocated + 1);
    }
    strcpy( *xpath + *xpathLen, step);
    *xpathLen += len;

} /* nodeToXPath */


/*----------------------------------------------------------------------------
|   xpathNodeToXPath
|
\---------------------------------------------------------------------------*/
char * xpathNodeToXPath (
    domNode *node
)
{
    char  * xpath;
    int     xpathLen, xpathAllocated;


    xpathAllocated = 100;
    xpathLen       = 0;
    xpath          = MALLOC(xpathAllocated + 1);

    nodeToXPath (node, &xpath, &xpathLen, &xpathAllocated);

    return xpath;

} /* xpathNodeToXPath */

