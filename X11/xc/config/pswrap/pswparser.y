/*
 *  pswparser.y
 *
 * (c) Copyright 1988-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */
/* $XFree86: xc/config/pswrap/pswparser.y,v 1.5 2000/09/26 15:56:28 tsi Exp $ */

/* 
 * Not all yaccs understand this.
 *
%expect 1
 */

%{

#include "pswpriv.h"
#include "pswsemantics.h"

%}

/* yylval type (from lexer and on stack) */

%union {
    char *object;
    long intobj;
    Token token;
    Item item;
    Header header;
    int flag;
    Arg arg;
    Subscript subscript;
}


%token <object> DEFINEPS ENDPS STATIC
%token <object> PSCONTEXT
%token <object> BOOLEAN FLOAT DOUBLE UNSIGNED SHORT LONG INT CHAR USEROBJECT
%token <object> NUMSTRING
%token <object> CNAME
%token <intobj> CINTEGER

%token <object> PSNAME PSLITNAME PSREAL PSBOOLEAN PSSTRING PSHEXSTRING
%token <intobj> PSINTEGER
%token <object> PSSUBNAME PSINDEX

%token <object> '(' ')' '|' ';' ',' '*' '[' ']' '{' '}' ':'

%type <token>	Token Tokens Body
%type <item>	Items Item
%type <header>	Header
%type <flag>	Def Type
%type <arg>	InArgs Args ContextArg SubArgs Arg
%type <subscript> Subscript

%start Module

%%

Module:
	/* empty */
	| Module Definition
	;

Definition:
	Header Body ENDPS
		{ FinalizePSWrapDef($1, $2); yyerrok; }
	| error ENDPS
		{ yyerrok; }
	;

Body:
	/* nothing */
		{ $$ = 0; }
	| Tokens
		/* { $$ = $1; }*/
	;

Tokens:
	Token
		{ $$ = AppendPSWToken($1, 0L); }
	| Tokens Token
		{ $$ = AppendPSWToken($2, $1); yyerrok; }
	/* | error
		{ $$ = 0; } */
	;

Header:
	Def ')'
		{ $$ = PSWHeader($1, 0, 0); yyerrok; }
	| Def InArgs ')'
		{ $$ = PSWHeader($1, $2, 0); yyerrok; }
	| Def InArgs '|' Args ')'
		{ $$ = PSWHeader($1, $2, $4); yyerrok; }
	| Def '|' Args ')'
		{ $$ = PSWHeader($1, 0, $3); yyerrok; }
	;

Def:
	DEFINEPS CNAME '('
		{ PSWName($2); $$ = 0; yyerrok; } 
	| DEFINEPS STATIC CNAME '('
		{ PSWName($3); $$ = 1; yyerrok; }
	| DEFINEPS error '('
		{ PSWName("error"); $$ = 0; yyerrok; }
	;

Semi:
	/* nothing */
	| ';' { yyerrok; }
	;

InArgs:
	ContextArg Semi
		/* { $$ = $1; } */
	| Args
		/* { $$ = $1; } */
	| ContextArg ';' Args
		{ $$ = ConsPSWArgs($1, $3); }
	;

ContextArg:
	PSCONTEXT CNAME
		{ $$ = PSWArg(T_CONTEXT, PSWItem($2)); }
	;

Args:
	SubArgs Semi
		/* { $$ = $1; }*/
	;

SubArgs:
	Arg
		/* { $$ = $1; }*/
	| SubArgs ';' Arg
		{ yyerrok; $$ = AppendPSWArgs($3, $1); }
	| SubArgs error
	| SubArgs error Arg
		{ yyerrok; $$ = AppendPSWArgs($3, $1); }
	| SubArgs ';' error
	;

Arg: Type Items
		{ $$ = PSWArg($1, $2); yyerrok; }
	;

Items:
	Item
		/* { $$ = $1; } */
	| Items ',' Item
		{ yyerrok; $$ = AppendPSWItems($3, $1); }
	| error { $$ = 0; }
	| Items error
	| Items error Item
		{ yyerrok; $$ = AppendPSWItems($3, $1); }
	| Items ',' error
	;

Item:
	'*' CNAME
		{ $$ = PSWStarItem($2); }
	| CNAME '[' Subscript ']'
		{ $$ = PSWSubscriptItem($1, $3); }
	| CNAME '[' Subscript ']' ':' CNAME
		{ $$ = PSWScaleItem($1, $3, $6, 0); }
	| CNAME '[' Subscript ']' ':' CINTEGER
		{ $$ = PSWScaleItem($1, $3, NULL, $6); }
	| CNAME
		{ $$ = PSWItem($1); }
	;

Subscript:
	CNAME
		{ $$ = PSWNameSubscript($1); }
	| CINTEGER
		{ $$ = PSWIntegerSubscript($1); }
	;
	
Type:
	BOOLEAN
		{ $$ = T_BOOLEAN; }
	| FLOAT
		{ $$ = T_FLOAT; }
	| DOUBLE
		{ $$ = T_DOUBLE; }
	| CHAR
		{ $$ = T_CHAR; }
	| UNSIGNED CHAR
		{ $$ = T_UCHAR; }
	| INT
		{ $$ = T_INT; }
	| LONG INT
		{ $$ = T_LONGINT; }
	| LONG
		{ $$ = T_LONGINT; }
	| SHORT INT
		{ $$ = T_SHORTINT; }
	| SHORT
		{ $$ = T_SHORTINT; }
	| UNSIGNED
		{ $$ = T_UINT; }
	| UNSIGNED LONG
		{ $$ = T_ULONGINT; }
	| UNSIGNED INT
		{ $$ = T_UINT; }
	| UNSIGNED LONG INT
		{ $$ = T_ULONGINT; }
	| UNSIGNED SHORT
		{ $$ = T_USHORTINT; }
	| UNSIGNED SHORT INT
		{ $$ = T_USHORTINT; }
	| USEROBJECT
		{ $$ = T_USEROBJECT; }
	| NUMSTRING
		{ $$ = T_NUMSTR; }		
	| INT NUMSTRING
		{ $$ = T_NUMSTR; }		
	| FLOAT NUMSTRING
		{ $$ = T_FLOATNUMSTR; }
	| LONG NUMSTRING
		{ $$ = T_LONGNUMSTR; }
	| SHORT NUMSTRING
		{ $$ = T_SHORTNUMSTR; }
	;

Token:
	PSINTEGER
		{ $$ = PSWToken(T_INT, (char *)$1); }
	| PSREAL
		{ $$ = PSWToken(T_FLOAT, $1); }
	| PSBOOLEAN
		{ $$ = PSWToken(T_BOOLEAN, $1); }
	| PSSTRING
		{ $$ = PSWToken(T_STRING, $1); }
	| PSHEXSTRING
		{ $$ = PSWToken(T_HEXSTRING, $1); }
	| PSNAME
		{ $$ = PSWToken(T_NAME, $1); }
	| PSLITNAME
		{ $$ = PSWToken(T_LITNAME, $1); }
	| PSSUBNAME PSINDEX
		{ $$ = PSWToken2(T_SUBSCRIPTED, $1, $2); }
	| '[' Body ']'
		{ $$ = PSWToken(T_ARRAY, (char *)$2); }
	| '{' Body '}'
		{ $$ = PSWToken(T_PROC, (char *)$2); }
	;
