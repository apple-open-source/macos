%token _BANG
%token _BANG_EQUALS
%token _AMPERAMPER
%token _LPAREN
%token _RPAREN
%token _PLUS_EQUALS
%token _COLON
%token _SEMIC
%token _LANGLE
%token _LANGLE_EQUALS
%token _EQUALS
%token _RANGLE
%token _RANGLE_EQUALS
%token _QUESTION_EQUALS
%token ACTIONS
%token BIND
%token CASE
%token COMMITDEFERRED     /* Apple Dec 2000 */
%token DEFAULT
%token DEFERRED           /* Apple Dec 2000 */
%token ELSE
%token EXISTING
%token EXPORT             /* Apple Nov 2000 */
%token EXPORTVARS         /* Apple Nov 2000 */
%token FOR
%token IF
%token IGNORE
%token IN
%token INCLUDE
%token LOCAL
%token ON
%token PIECEMEAL
%token QUIETLY
%token RULE
%token SWITCH
%token TOGETHER
%token UPDATED
%token _LBRACE
%token _BARBAR
%token _RBRACE
/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * jamgram.yy - jam grammar
 *
 * 12/09/00 (andersb) - added support for make-style deferred variables
 * 11/11/00 (andersb) - added support for 'export' and 'exportvars'
 * 06/14/00 (cmolick) - define large YYSTACKSIZE to prevent yacc stack overflow
 * 04/13/94 (seiwald) - added shorthand L0 for null list pointer
 * 06/01/94 (seiwald) - new 'actions existing' does existing sources
 * 08/23/94 (seiwald) - Support for '+=' (append to variable)
 * 08/31/94 (seiwald) - Allow ?= as alias for "default =".
 * 09/15/94 (seiwald) - if conditionals take only single arguments, so
 *			that 'if foo == bar' gives syntax error (use =).
 * 02/11/95 (seiwald) - when scanning arguments to rules, only treat
 *			punctuation keywords as keywords.  All arg lists
 *			are terminated with punctuation keywords.
 */

%token ARG STRING

%left _BARBAR
%left _AMPERAMPER
%left _BANG

%{
#include "jam.h"

#include "lists.h"
#include "parse.h"
#include "scan.h"
#include "compile.h"
#include "newstr.h"

# define YYSTACKSIZE 1000000

# define F0 (void (*)())0
# define P0 (PARSE *)0
# define S0 (char *)0

# define pset( l,r,a ) 	  parse_make( compile_set,P0,P0,S0,S0,l,r,a )
# define pset1( l,p,a )	  parse_make( compile_settings,p,P0,S0,S0,l,L0,a )
# define pstng( p,l,r,a ) pset1( p, parse_make( F0,P0,P0,S0,S0,l,r,0 ), a )
# define pvrule( l,p )    parse_make( compile_vrule,p,P0,S0,S0,l,L0,0 )
# define prule( s,p )     parse_make( compile_rule,p,P0,s,S0,L0,L0,0 )
# define prules( l,r )	  parse_make( compile_rules,l,r,S0,S0,L0,L0,0 )
# define pfor( s,p,l )    parse_make( compile_foreach,p,P0,s,S0,l,L0,0 )
# define psetc( s,p )     parse_make( compile_setcomp,p,P0,s,S0,L0,L0,0 )
# define psete( s,l,s1,f ) parse_make( compile_setexec,P0,P0,s,s1,l,L0,f )
# define pincl( l )       parse_make( compile_include,P0,P0,S0,S0,l,L0,0 )
# define pswitch( l,p )   parse_make( compile_switch,p,P0,S0,S0,l,L0,0 )
# define plocal( l,r,p )  parse_make( compile_local,p,P0,S0,S0,l,r,0 )
# define pnull()	  parse_make( compile_null,P0,P0,S0,S0,L0,L0,0 )
# define pcases( l,r )    parse_make( F0,l,r,S0,S0,L0,L0,0 )
# define pcase( s,p )     parse_make( F0,p,P0,s,S0,L0,L0,0 )
# define pif( l,r )	  parse_make( compile_if,l,r,S0,S0,L0,L0,0 )
# define pthen( l,r )	  parse_make( F0,l,r,S0,S0,L0,L0,0 )
# define pcond( c,l,r )	  parse_make( F0,l,r,S0,S0,L0,L0,c )
# define pcomp( c,l,r )	  parse_make( F0,P0,P0,S0,S0,l,r,c )
# define plol( p,l )	  parse_make( F0,p,P0,S0,S0,l,L0,0 )
# define pcommitdefr( )	  parse_make( compile_commitdeferred,P0,P0,S0,S0,L0,L0,0 )         /* Apple Dec 2000 */


%}

%%

run	: block
		{ 
		    if( $1.parse->func == compile_null )
		    {
			parse_free( $1.parse );
		    }
		    else
		    {
			parse_save( $1.parse ); 
		    }
		}
	;


/*
 * block - one or more rules
 * rule - any one of jam's rules
 */

block	: /* empty */
		{ $$.parse = pnull(); }
	| rule block
		{ $$.parse = prules( $1.parse, $2.parse ); }
	| LOCAL args _SEMIC block
		{ $$.parse = plocal( $2.list, L0, $4.parse ); }
	| LOCAL args _EQUALS args _SEMIC block
		{ $$.parse = plocal( $2.list, $4.list, $6.parse ); }
	;

rule	: _LBRACE block _RBRACE
		{ $$.parse = $2.parse; }
	| INCLUDE args _SEMIC
		{ $$.parse = pincl( $2.list ); }
	| arg1 lol _SEMIC
		{ $$.parse = pvrule( $1.list, $2.parse ); }
	| EXPORT arg1 assign args _SEMIC                             /* Apple Nov 2000 */
		{ $$.parse = pset( $2.list, $4.list, $3.number | ASSIGN_EXPORT ); }
	| EXPORT arg1 DEFAULT _EQUALS args _SEMIC                    /* Apple Nov 2000 */
		{ $$.parse = pset( $2.list, $5.list, ASSIGN_DEFAULT | ASSIGN_EXPORT ); }
	| DEFERRED arg1 assign args _SEMIC                           /* Apple Dec 2000 */
		{ $$.parse = pset( $2.list, $4.list, $3.number | ASSIGN_DEFERRED ); }
	| EXPORT DEFERRED arg1 assign args _SEMIC                    /* Apple Dec 2000 */
		{ $$.parse = pset( $3.list, $5.list, $4.number | ASSIGN_EXPORT | ASSIGN_DEFERRED ); }
	| COMMITDEFERRED _SEMIC                                      /* Apple Dec 2000 */
		{ $$.parse = pcommitdefr( ); }
	| arg1 assign args _SEMIC
		{ $$.parse = pset( $1.list, $3.list, $2.number ); }
	| arg1 ON args assign args _SEMIC
		{ $$.parse = pstng( $3.list, $1.list, $5.list, $4.number ); }
	| arg1 DEFAULT _EQUALS args _SEMIC
		{ $$.parse = pset( $1.list, $4.list, ASSIGN_DEFAULT ); }
	| FOR ARG IN args _LBRACE block _RBRACE
		{ $$.parse = pfor( $2.string, $6.parse, $4.list ); }
	| SWITCH args _LBRACE cases _RBRACE
		{ $$.parse = pswitch( $2.list, $4.parse ); }
	| IF cond _LBRACE block _RBRACE 
		{ $$.parse = pif( $2.parse, pthen( $4.parse, pnull() ) ); }
	| IF cond _LBRACE block _RBRACE ELSE rule
		{ $$.parse = pif( $2.parse, pthen( $4.parse, $7.parse ) ); }
	| RULE ARG rule
		{ $$.parse = psetc( $2.string, $3.parse ); }
	| ACTIONS eflags ARG bindlist _LBRACE
		{ yymode( SCAN_STRING ); }
	  STRING 
		{ yymode( SCAN_NORMAL ); }
	  _RBRACE
		{ $$.parse = psete( $3.string,$4.list,$7.string,$2.number ); }
	;

/*
 * assign - = or +=
 */

assign	: _EQUALS
		{ $$.number = ASSIGN_SET; }
	| _PLUS_EQUALS
		{ $$.number = ASSIGN_APPEND; }
	| _QUESTION_EQUALS
		{ $$.number = ASSIGN_DEFAULT; }
	;

/*
 * cond - a conditional for 'if'
 */

cond	: arg1 
		{ $$.parse = pcomp( COND_EXISTS, $1.list, L0 ); }
	| arg1 _EQUALS arg1 
		{ $$.parse = pcomp( COND_EQUALS, $1.list, $3.list ); }
	| arg1 _BANG_EQUALS arg1
		{ $$.parse = pcomp( COND_NOTEQ, $1.list, $3.list ); }
	| arg1 _LANGLE arg1
		{ $$.parse = pcomp( COND_LESS, $1.list, $3.list ); }
	| arg1 _LANGLE_EQUALS arg1 
		{ $$.parse = pcomp( COND_LESSEQ, $1.list, $3.list ); }
	| arg1 _RANGLE arg1 
		{ $$.parse = pcomp( COND_MORE, $1.list, $3.list ); }
	| arg1 _RANGLE_EQUALS arg1 
		{ $$.parse = pcomp( COND_MOREEQ, $1.list, $3.list ); }
	| arg1 IN args
		{ $$.parse = pcomp( COND_IN, $1.list, $3.list ); }
	| _BANG cond
		{ $$.parse = pcond( COND_NOT, $2.parse, P0 ); }
	| cond _AMPERAMPER cond 
		{ $$.parse = pcond( COND_AND, $1.parse, $3.parse ); }
	| cond _BARBAR cond
		{ $$.parse = pcond( COND_OR, $1.parse, $3.parse ); }
	| _LPAREN cond _RPAREN
		{ $$.parse = $2.parse; }
	;

/*
 * cases - action elements inside a 'switch'
 * case - a single action element inside a 'switch'
 *
 * Unfortunately, a right-recursive rule.
 */

cases	: /* empty */
		{ $$.parse = P0; }
	| case cases
		{ $$.parse = pcases( $1.parse, $2.parse ); }
	;

case	: CASE ARG _COLON block
		{ $$.parse = pcase( $2.string, $4.parse ); }
	;

/*
 * lol - list of lists
 */

lol	: args
		{ $$.parse = plol( P0, $1.list ); }
	| args _COLON lol
		{ $$.parse = plol( $3.parse, $1.list ); }
	;

/*
 * args - zero or more ARGs in a LIST
 * arg1 - exactly one ARG in a LIST 
 */

args	: argsany
		{ yymode( SCAN_NORMAL ); }
	;

argsany	: /* empty */
		{ $$.list = L0; yymode( SCAN_PUNCT ); }
	| argsany ARG
		{ $$.list = list_new( $1.list, copystr( $2.string ) ); }
	;

arg1	: ARG 
		{ $$.list = list_new( L0, copystr( $1.string ) ); }
	;

/*
 * eflags - zero or more modifiers to 'executes'
 * eflag - a single modifier to 'executes'
 */

eflags	: /* empty */
		{ $$.number = 0; }
	| eflags eflag
		{ $$.number = $1.number | $2.number; }
	;

eflag	: UPDATED
		{ $$.number = EXEC_UPDATED; }
	| TOGETHER
		{ $$.number = EXEC_TOGETHER; }
	| IGNORE
		{ $$.number = EXEC_IGNORE; }
	| QUIETLY
		{ $$.number = EXEC_QUIETLY; }
	| PIECEMEAL
		{ $$.number = EXEC_PIECEMEAL; }
	| EXISTING
		{ $$.number = EXEC_EXISTING; }
	| EXPORTVARS
		{ $$.number = EXEC_EXPORTVARS; }
	;


/*
 * bindlist - list of variable to bind for an action
 */

bindlist : /* empty */
		{ $$.list = L0; }
	| BIND args
		{ $$.list = $2.list; }
	;


