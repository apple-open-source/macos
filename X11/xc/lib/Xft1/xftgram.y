/*
 * $XFree86: xc/lib/Xft1/xftgram.y,v 1.1.1.1 2002/02/15 01:26:16 keithp Exp $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

%{

#include <stdlib.h>
#include <stdio.h>
#include "xftint.h"

static XftMatrix   matrix;
    
%}

%union {
    int		ival;
    double	dval;
    char	*sval;
    XftExpr	*eval;
    XftPattern	*pval;
    XftValue	vval;
    XftEdit	*Eval;
    XftOp	oval;
    XftQual	qval;
    XftTest	*tval;
}

%token <ival>	INTEGER
%token <dval>	DOUBLE
%token <sval>	STRING NAME
%token <ival>	ANY ALL
%token <ival>	DIR CACHE INCLUDE INCLUDEIF MATCH EDIT
%token <ival>	TOK_TRUE TOK_FALSE TOK_NIL
%token <ival>	EQUAL SEMI OS CS

%type  <eval>	expr
%type  <vval>	value
%type  <sval>	field
%type  <Eval>	edit
%type  <Eval>	edits
%type  <oval>	eqop
%type  <qval>	qual
%type  <oval>	compare
%type  <tval>	tests test
%type  <dval>	number

%right <ival>	QUEST COLON
%left <ival>	OROR
%left <ival>	ANDAND
%left <ival>	EQEQ NOTEQ
%left <ival>	LESS LESSEQ MORE MOREEQ
%left <ival>	PLUS MINUS
%left <ival>	TIMES DIVIDE
%right <ival>	NOT

%%
configs	:   configs config
	|
	;
config	:   DIR STRING
		{ XftConfigAddDir ($2); }
	|   CACHE STRING
		{ XftConfigSetCache ($2); }
	|   INCLUDE STRING
		{ XftConfigPushInput ($2, True); }
	|   INCLUDEIF STRING
		{ XftConfigPushInput ($2, False); }
	|   MATCH tests EDIT edits
		{ XftConfigAddEdit ($2, $4); }
	;
tests	:   test tests
		{ $1->next = $2; $$ = $1; }
	|
		{ $$ = 0; }
	;
test	:   qual field compare value
		{ $$ = XftTestCreate ($1, $2, $3, $4); }
	;
qual	:   ANY
	    { $$ = XftQualAny; }
	|   ALL
	    { $$ = XftQualAll; }
	|
	    { $$ = XftQualAny; }
	;
field	:   NAME
		{ 
		    $$ = XftConfigSaveField ($1); 
		}
	;
compare	:   EQUAL
		{ $$ = XftOpEqual; }
	|   EQEQ
		{ $$ = XftOpEqual; }
	|   NOTEQ
		{ $$ = XftOpNotEqual; }
	|   LESS
		{ $$ = XftOpLess; }
	|   LESSEQ
		{ $$ = XftOpLessEqual; }
	|   MORE
		{ $$ = XftOpMore; }
	|   MOREEQ
		{ $$ = XftOpMoreEqual; }
	;
value	:   INTEGER
		{
		    $$.type = XftTypeInteger;
		    $$.u.i = $1;
		}
	|   DOUBLE		
		{
		    $$.type = XftTypeDouble;
		    $$.u.d = $1;
		}
	|   STRING
		{
		    $$.type = XftTypeString;
		    $$.u.s = $1;
		}
	|   TOK_TRUE
		{
		    $$.type = XftTypeBool;
		    $$.u.b = True;
		}
	|   TOK_FALSE
		{
		    $$.type = XftTypeBool;
		    $$.u.b = False;
		}
	|   TOK_NIL
		{
		    $$.type = XftTypeVoid;
		}
	|   matrix
		{
		    $$.type = XftTypeMatrix;
		    $$.u.m = &matrix;
		}
	;
matrix	:   OS number number number number CS
		{
		    matrix.xx = $2;
		    matrix.xy = $3;
		    matrix.yx = $4;
		    matrix.__REALLY_YY__ = $5;
		}
number	:   INTEGER
		{ $$ = (double) $1; }
	|   DOUBLE
	;
edits	:   edit edits
	    { $1->next = $2; $$ = $1; }
	|
	    { $$ = 0; }
	;
edit	:   field eqop expr SEMI
	    { $$ = XftEditCreate ($1, $2, $3); }
	;
eqop	:   EQUAL
	    { $$ = XftOpAssign; }
	|   PLUS EQUAL
	    { $$ = XftOpPrepend; }
	|   EQUAL PLUS
	    { $$ = XftOpAppend; }
	;
expr	:   INTEGER
	    { $$ = XftExprCreateInteger ($1); }
	|   DOUBLE
	    { $$ = XftExprCreateDouble ($1); }
	|   STRING
	    { $$ = XftExprCreateString ($1); }
	|   TOK_TRUE
	    { $$ = XftExprCreateBool (True); }
	|   TOK_FALSE
	    { $$ = XftExprCreateBool (False); }
	|   TOK_NIL
	    { $$ = XftExprCreateNil (); }
	|   matrix
	    { $$ = XftExprCreateMatrix (&matrix); }
	|   NAME
	    { $$ = XftExprCreateField ($1); }
	|   expr OROR expr
	    { $$ = XftExprCreateOp ($1, XftOpOr, $3); }
	|   expr ANDAND expr
	    { $$ = XftExprCreateOp ($1, XftOpAnd, $3); }
	|   expr EQEQ expr
	    { $$ = XftExprCreateOp ($1, XftOpEqual, $3); }
	|   expr NOTEQ expr
	    { $$ = XftExprCreateOp ($1, XftOpNotEqual, $3); }
	|   expr LESS expr
	    { $$ = XftExprCreateOp ($1, XftOpLess, $3); }
	|   expr LESSEQ expr
	    { $$ = XftExprCreateOp ($1, XftOpLessEqual, $3); }
	|   expr MORE expr
	    { $$ = XftExprCreateOp ($1, XftOpMore, $3); }
	|   expr MOREEQ expr
	    { $$ = XftExprCreateOp ($1, XftOpMoreEqual, $3); }
	|   expr PLUS expr
	    { $$ = XftExprCreateOp ($1, XftOpPlus, $3); }
	|   expr MINUS expr
	    { $$ = XftExprCreateOp ($1, XftOpMinus, $3); }
	|   expr TIMES expr
	    { $$ = XftExprCreateOp ($1, XftOpTimes, $3); }
	|   expr DIVIDE expr
	    { $$ = XftExprCreateOp ($1, XftOpDivide, $3); }
	|   NOT expr
	    { $$ = XftExprCreateOp ($2, XftOpNot, (XftExpr *) 0); }
	|   expr QUEST expr COLON expr
	    { $$ = XftExprCreateOp ($1, XftOpQuest, XftExprCreateOp ($3, XftOpQuest, $5)); }
	;
%%

int
XftConfigwrap (void)
{
    return 1;
}

void
XftConfigerror (char *fmt, ...)
{
    va_list	args;

    fprintf (stderr, "\"%s\": line %d, ", XftConfigFile, XftConfigLineno);
    va_start (args, fmt);
    vfprintf (stderr, fmt, args);
    va_end (args);
    fprintf (stderr, "\n");
}

XftTest *
XftTestCreate (XftQual qual, const char *field, XftOp compare, XftValue value)
{
    XftTest	*test = (XftTest *) malloc (sizeof (XftTest));;

    if (test)
    {
	test->next = 0;
	test->qual = qual;
	test->field = field;	/* already saved in grammar */
	test->op = compare;
	if (value.type == XftTypeString)
	    value.u.s = _XftSaveString (value.u.s);
	else if (value.type == XftTypeMatrix)
	    value.u.m = _XftSaveMatrix (value.u.m);
	test->value = value;
    }
    return test;
}

XftExpr *
XftExprCreateInteger (int i)
{
    XftExpr *e = (XftExpr *) malloc (sizeof (XftExpr));

    if (e)
    {
	e->op = XftOpInteger;
	e->u.ival = i;
    }
    return e;
}

XftExpr *
XftExprCreateDouble (double d)
{
    XftExpr *e = (XftExpr *) malloc (sizeof (XftExpr));

    if (e)
    {
	e->op = XftOpDouble;
	e->u.dval = d;
    }
    return e;
}

XftExpr *
XftExprCreateString (const char *s)
{
    XftExpr *e = (XftExpr *) malloc (sizeof (XftExpr));

    if (e)
    {
	e->op = XftOpString;
	e->u.sval = _XftSaveString (s);
    }
    return e;
}

XftExpr *
XftExprCreateMatrix (const XftMatrix *m)
{
    XftExpr *e = (XftExpr *) malloc (sizeof (XftExpr));

    if (e)
    {
	e->op = XftOpMatrix;
	e->u.mval = _XftSaveMatrix (m);
    }
    return e;
}

XftExpr *
XftExprCreateBool (Bool b)
{
    XftExpr *e = (XftExpr *) malloc (sizeof (XftExpr));

    if (e)
    {
	e->op = XftOpBool;
	e->u.bval = b;
    }
    return e;
}

XftExpr *
XftExprCreateNil (void)
{
    XftExpr *e = (XftExpr *) malloc (sizeof (XftExpr));

    if (e)
    {
	e->op = XftOpNil;
    }
    return e;
}

XftExpr *
XftExprCreateField (const char *field)
{
    XftExpr *e = (XftExpr *) malloc (sizeof (XftExpr));

    if (e)
    {
	e->op = XftOpField;
	e->u.field = _XftSaveString (field);
    }
    return e;
}

XftExpr *
XftExprCreateOp (XftExpr *left, XftOp op, XftExpr *right)
{
    XftExpr *e = (XftExpr *) malloc (sizeof (XftExpr));

    if (e)
    {
	e->op = op;
	e->u.tree.left = left;
	e->u.tree.right = right;
    }
    return e;
}

void
XftExprDestroy (XftExpr *e)
{
    switch (e->op) {
    case XftOpInteger:
	break;
    case XftOpDouble:
	break;
    case XftOpString:
	free (e->u.sval);
	break;
    case XftOpMatrix:
	free (e->u.mval);
	break;
    case XftOpBool:
	break;
    case XftOpField:
	free (e->u.field);
	break;
    case XftOpAssign:
    case XftOpPrepend:
    case XftOpAppend:
	break;
    case XftOpOr:
    case XftOpAnd:
    case XftOpEqual:
    case XftOpNotEqual:
    case XftOpLess:
    case XftOpLessEqual:
    case XftOpMore:
    case XftOpMoreEqual:
    case XftOpPlus:
    case XftOpMinus:
    case XftOpTimes:
    case XftOpDivide:
    case XftOpQuest:
	XftExprDestroy (e->u.tree.right);
	/* fall through */
    case XftOpNot:
	XftExprDestroy (e->u.tree.left);
	break;
    case XftOpNil:
	break;
    }
    free (e);
}

XftEdit *
XftEditCreate (const char *field, XftOp op, XftExpr *expr)
{
    XftEdit *e = (XftEdit *) malloc (sizeof (XftEdit));

    if (e)
    {
	e->next = 0;
	e->field = field;   /* already saved in grammar */
	e->op = op;
	e->expr = expr;
    }
    return e;
}

void
XftEditDestroy (XftEdit *e)
{
    if (e->next)
	XftEditDestroy (e->next);
    free ((void *) e->field);
    if (e->expr)
	XftExprDestroy (e->expr);
}

char *
XftConfigSaveField (const char *field)
{
    return _XftSaveString (field);
}
