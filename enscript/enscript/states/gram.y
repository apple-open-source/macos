%{
/* 								-*- c -*-
 * Grammar for states.
 * Copyright (c) 1997-1998 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This file is part of GNU enscript.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * $Id: gram.y,v 1.1.1.1 2003/03/05 07:25:52 mtr Exp $
 */

#include "defs.h"
%}

%union
{
  List *lst;
  Node *node;
  Cons *cons;
  Stmt *stmt;
  Expr *expr;
}

%token <node> tSYMBOL tREGEXP tSTRING tINTEGER tREAL
%token tSUB tSTATE tSTART tSTARTRULES tNAMERULES tBEGIN tEND tRETURN tIF tELSE
%token tLOCAL tWHILE tFOR tEXTENDS

%right '=' tADDASSIGN tSUBASSIGN tMULASSIGN tDIVASSIGN
%right '?' ':'
%left tOR
%left tAND
%left tEQ tNE
%left '<' '>' tGE tLE
%left '+' '-'
%left '*' tDIV
%right '!' tPLUSPLUS tMINUSMINUS
%left '[' ']'

%type <lst> regexp_sym_list symbol_list rest_symbol_list staterules
%type <lst> stmt_list expr_list rest_expr_list locals locals_rest
%type <stmt> stmt
%type <expr> expr cond_expr
%type <cons> staterule local_def

%%

file	: /* empty */
	| file toplevel
	;

toplevel : tSTART '{' stmt_list '}'	{ start_stmts = $3; }
	| tSTARTRULES '{' regexp_sym_list '}'
					{ startrules = $3; }
	| tNAMERULES '{' regexp_sym_list '}'
					{ namerules = $3; }
	| tSTATE tSYMBOL '{' staterules '}'
					{ define_state ($2, NULL, $4); }
	| tSTATE tSYMBOL tEXTENDS tSYMBOL '{' staterules '}'
					{ define_state ($2, $4, $6); }
	| stmt				{ list_append (global_stmts, $1); }
	;

regexp_sym_list : /* empty */ 		{ $$ = list (); }
	| regexp_sym_list tREGEXP tSYMBOL ';'
					{ list_append ($1, cons ($2, $3)); }
	;

staterules : /* empty */ 		{ $$ = list (); }
	| staterules staterule		{ list_append ($1, $2); }

staterule : tBEGIN '{' stmt_list '}' 	{ $$ = cons (RULE_BEGIN, $3); }
	| tEND '{' stmt_list '}' 	{ $$ = cons (RULE_END, $3); }
	| tREGEXP '{' stmt_list '}'	{ $$ = cons ($1, $3); }
	| tSYMBOL '{' stmt_list '}'	{ $$ = cons ($1, $3); }
	;

symbol_list : /* empty */		{ $$ = list (); }
	| rest_symbol_list	 	{ $$ = $1; }
	;

rest_symbol_list : tSYMBOL		{ $$ = list (); list_append ($$, $1); }
	| rest_symbol_list ',' tSYMBOL 	{ list_append ($1, $3); }
	;

locals	: /* empty */			{ $$ = list (); }
	| tLOCAL locals_rest ';'	{ $$ = $2; }
	;

locals_rest : local_def			{ $$ = list (); list_append ($$, $1); }
	| locals_rest ',' local_def	{ list_append ($1, $3); }
	;

local_def : tSYMBOL			{ $$ = cons ($1, NULL); }
	| tSYMBOL '=' expr		{ $$ = cons ($1, $3); }
	;

stmt_list : /* empty */			{ $$ = list (); }
	| stmt_list stmt 		{ list_append ($1, $2); }
	;

stmt	: tRETURN ';'			{ $$ = mk_stmt (sRETURN, NULL, NULL,
							NULL, NULL); }
	| tRETURN expr ';'		{ $$ = mk_stmt (sRETURN, $2, NULL,
							NULL, NULL); }
	| tSUB tSYMBOL '(' symbol_list ')' '{' locals stmt_list '}'
					{ $$ = mk_stmt (sDEFSUB, $2,
							cons (cons ($4, $7),
							      $8),
							NULL, NULL); }
	| '{' stmt_list '}'		{ $$ = mk_stmt (sBLOCK, $2, NULL,
							NULL, NULL); }
	| tIF '(' expr ')' stmt		{ $$ = mk_stmt (sIF, $3, $5, NULL,
							NULL); }
	| tIF '(' expr ')' stmt tELSE stmt
					{ $$ = mk_stmt (sIF, $3, $5, $7,
							NULL); }
	| tWHILE '(' expr ')' stmt 	{ $$ = mk_stmt (sWHILE, $3, $5,
							NULL, NULL); }
	| tFOR '(' cond_expr ';' expr ';' cond_expr ')' stmt
					{ $$ = mk_stmt (sFOR, $3, $5, $7,
							$9); }
	| expr ';'			{ $$ = mk_stmt (sEXPR, $1, NULL,
							NULL, NULL); }
	;

expr	: tSTRING 			{ $$ = mk_expr (eSTRING, $1, NULL,
							NULL); }
	| tREGEXP			{ $$ = mk_expr (eREGEXP, $1, NULL,
							NULL); }
	| tINTEGER			{ $$ = mk_expr (eINTEGER, $1, NULL,
							NULL); }
	| tREAL				{ $$ = mk_expr (eREAL, $1, NULL,
							NULL); }
	| tSYMBOL			{ $$ = mk_expr (eSYMBOL, $1, NULL,
							NULL); }
	| '!' expr			{ $$ = mk_expr (eNOT, $2, NULL,
							NULL); }
	| expr tAND expr		{ $$ = mk_expr (eAND, $1, $3, NULL); }
	| expr tOR expr			{ $$ = mk_expr (eOR, $1, $3, NULL); }
	| tSYMBOL '(' expr_list ')'	{ $$ = mk_expr (eFCALL, $1, $3,
							NULL); }
	| tSYMBOL '=' expr		{ $$ = mk_expr (eASSIGN, $1, $3,
							NULL); }
	| tSYMBOL tADDASSIGN expr	{ $$ = mk_expr (eADDASSIGN, $1, $3,
							NULL); }
	| tSYMBOL tSUBASSIGN expr	{ $$ = mk_expr (eSUBASSIGN, $1, $3,
							NULL); }
	| tSYMBOL tMULASSIGN expr	{ $$ = mk_expr (eMULASSIGN, $1, $3,
							NULL); }
	| tSYMBOL tDIVASSIGN expr	{ $$ = mk_expr (eDIVASSIGN, $1, $3,
							NULL); }
	| tSYMBOL tPLUSPLUS		{ $$ = mk_expr (ePOSTFIXADD, $1, NULL,
							NULL); }
	| tSYMBOL tMINUSMINUS		{ $$ = mk_expr (ePOSTFIXSUB, $1, NULL,
							NULL); }
	| tPLUSPLUS tSYMBOL		{ $$ = mk_expr (ePREFIXADD, $2, NULL,
							NULL); }
	| tMINUSMINUS tSYMBOL		{ $$ = mk_expr (ePREFIXSUB, $2, NULL,
							NULL); }
	| expr '[' expr ']' '=' expr	{ $$ = mk_expr (eARRAYASSIGN, $1, $3,
							$6); }
	| '(' expr ')'			{ $$ = $2; }
	| expr '[' expr ']'		{ $$ = mk_expr (eARRAYREF, $1, $3,
							NULL); }
	| expr '?' expr ':' expr	{ $$ = mk_expr (eQUESTCOLON, $1, $3,
							$5); }
	| expr '*' expr			{ $$ = mk_expr (eMULT, $1, $3, NULL); }
	| expr tDIV expr		{ $$ = mk_expr (eDIV, $1, $3, NULL); }
	| expr '+' expr			{ $$ = mk_expr (ePLUS, $1, $3, NULL); }
	| expr '-' expr			{ $$ = mk_expr (eMINUS, $1, $3,
							NULL); }
	| expr '<' expr			{ $$ = mk_expr (eLT, $1, $3, NULL); }
	| expr '>' expr			{ $$ = mk_expr (eGT, $1, $3, NULL); }
	| expr tEQ expr			{ $$ = mk_expr (eEQ, $1, $3, NULL); }
	| expr tNE expr			{ $$ = mk_expr (eNE, $1, $3, NULL); }
	| expr tGE expr			{ $$ = mk_expr (eGE, $1, $3, NULL); }
	| expr tLE expr			{ $$ = mk_expr (eLE, $1, $3, NULL); }
	;

cond_expr : /* empty */			{ $$ = NULL; }
	| expr				{ $$ = $1; }
	;

expr_list : /* empty */ 		{ $$ = list (); }
	| rest_expr_list 		{ $$ = $1; }
	;

rest_expr_list: expr 	 		{ $$ = list (); list_append ($$, $1); }
	| rest_expr_list ',' expr  	{ list_append ($1, $3); }
	;

%%

void
yyerror (msg)
     char *msg;
{
  fprintf (stderr, "%s:%d: %s\n", yyin_name, linenum, msg);
}
