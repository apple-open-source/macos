/*----------------------------------------------------------------------------
|   Copyright (c) 1999 Jochen Loewer (loewerj@hotmail.com)
|-----------------------------------------------------------------------------
|
|   $Header: /usr/local/pubcvs/tdom/generic/domxpath.h,v 1.16 2003/12/30 19:24:29 rolf Exp $
|
| 
|   A (partial) XPath implementation (lexer/parser/evaluator) for tDOM, 
|   the DOM implementation for Tcl.
|   Based on the August 13 working draft of the W3C 
|   (http://www.w3.org/1999/08/WD-xpath-19990813.html)
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
|
|
|   written by Jochen Loewer
|   July, 1999
|
\---------------------------------------------------------------------------*/


#ifndef __DOMXPATH_H__
#define __DOMXPATH_H__

#include <float.h>
#include <dom.h>

/*----------------------------------------------------------------------------
|   Macros
|
\---------------------------------------------------------------------------*/
#define XPATH_OK             0
#define XPATH_LEX_ERR       -1
#define XPATH_SYNTAX_ERR    -2
#define XPATH_EVAL_ERR      -3
#define XPATH_VAR_NOT_FOUND -4

/*
 * Macros for testing floating-point values for certain special cases. Test
 * for not-a-number by comparing a value against itself; test for infinity
 * by comparing against the largest floating-point value.
 */

#define IS_NAN(v) ((v) != (v))
#ifdef DBL_MAX
#   define IS_INF(v) ((v) > DBL_MAX ? 1 : ((v) < -DBL_MAX ? -1 : 0))
#else
#   define IS_INF(v) 0
#endif

/*----------------------------------------------------------------------------
|   Types for abstract syntax trees
|
\---------------------------------------------------------------------------*/
typedef enum {
    Int, Real, Mult, Div, Mod, UnaryMinus, IsNSElement,
    IsNode, IsComment, IsText, IsPI, IsSpecificPI, IsElement,
    IsFQElement, GetVar, GetFQVar, Literal, ExecFunction, Pred,
    EvalSteps, SelectRoot, CombineSets, Add, Substract, Less,
    LessOrEq, Greater, GreaterOrEq, Equal,  NotEqual, And, Or,
    IsNSAttr, IsAttr, AxisAncestor, AxisAncestorOrSelf, 
    AxisAttribute, AxisChild,
    AxisDescendant, AxisDescendantOrSelf, AxisFollowing,
    AxisFollowingSibling, AxisNamespace, AxisParent,
    AxisPreceding, AxisPrecedingSibling, AxisSelf,
    GetContextNode, GetParentNode, AxisDescendantOrSelfLit,
    AxisDescendantLit, SlashSlash,
        
    CombinePath, IsRoot, ToParent, ToAncestors, FillNodeList,
    FillWithCurrentNode,
    ExecIdKey
    
} astType;


typedef struct astElem {
    astType         type;
    struct astElem *child;
    struct astElem *next;
    char           *strvalue;
    int             intvalue;
    double          realvalue;
} astElem;

typedef astElem *ast;


/*----------------------------------------------------------------------------
|   Types for XPath result set
|
\---------------------------------------------------------------------------*/
typedef enum { 
    EmptyResult, BoolResult, IntResult, RealResult, StringResult, 
    xNodeSetResult, NaNResult, InfResult, NInfResult
} xpathResultType;


typedef struct xpathResultSet {

    xpathResultType type;
    char           *string;
    int             string_len;
    int             intvalue;
    double          realvalue;          
    domNode       **nodes;
    int             nr_nodes;
    int             allocated;

} xpathResultSet;

typedef xpathResultSet *xpathResultSets;

typedef int (*xpathFuncCallback) 
                (void *clientData, char *functionName, 
                 domNode *ctxNode, int position, xpathResultSet *nodeList,
                 domNode *exprContext, int argc, xpathResultSets *args,
                 xpathResultSet *result, char  **errMsg);
                              
typedef int (*xpathVarCallback) 
                (void *clientData, char *variableName, char *varURI,
                 xpathResultSet *result, char  **errMsg);
                              
typedef struct xpathCBs {               /* all xpath callbacks + clientData */

    xpathVarCallback    varCB;
    void              * varClientData;
    xpathFuncCallback   funcCB;
    void              * funcClientData;

} xpathCBs;

/* XPath expr/pattern types */
typedef enum {
    XPATH_EXPR, XPATH_FORMAT_PATTERN, XPATH_TEMPMATCH_PATTERN, 
    XPATH_KEY_USE_EXPR, XPATH_KEY_MATCH_PATTERN
} xpathExprType;

/*----------------------------------------------------------------------------
|   Prototypes
|
\---------------------------------------------------------------------------*/
int    xpathParse   (char *xpath, char **errMsg, ast *t, xpathExprType type);
void   xpathFreeAst (ast t);
double xpathGetPrio (ast t);
int    xpathEval    (domNode *node, domNode *exprContext, char *xpath, xpathCBs *cbs,
                     char **errMsg, xpathResultSet *rs
                     );
int    xpathMatches (ast steps, domNode * exprContext, domNode *nodeToMatch,
                     xpathCBs *cbs, char **errMsg 
                    );
                     
int xpathEvalSteps (ast steps, xpathResultSet *nodeList,
                    domNode *currentNode, domNode *exprContext, int currentPos,
                    int *docOrder,
                    xpathCBs *cbs,
                    xpathResultSet *result, char **errMsg);
                    
#define xpathRSInit(x) (x)->type = EmptyResult; \
                       (x)->intvalue = 0; \
                       (x)->nr_nodes = 0;
void   xpathRSFree (xpathResultSet *rs );

int    xpathFuncBoolean  (xpathResultSet *rs);
double xpathFuncNumber   (xpathResultSet *rs, int *NaN);
char * xpathFuncString   (xpathResultSet *rs); 
char * xpathFuncStringForNode (domNode *node);
int    xpathRound        (double r);

char * xpathGetStringValue (domNode *node, int *strLen);

char * xpathNodeToXPath  (domNode *node);
    
void rsSetBool      ( xpathResultSet *rs, int          i    );
void rsSetInt       ( xpathResultSet *rs, int          i    );
void rsSetReal      ( xpathResultSet *rs, double       d    );
void rsSetString    ( xpathResultSet *rs, char        *s    );
void rsAddNode      ( xpathResultSet *rs, domNode     *node );
void rsAddNodeFast  ( xpathResultSet *rs, domNode     *node );
void rsCopy         ( xpathResultSet *to, xpathResultSet *from );

/* This function is only used for debugging code */
void rsPrint        ( xpathResultSet *rs );

#endif

