/* GNU gettext - internationalization aids
   Copyright (C) 1995-1996, 1998, 2001-2003 Free Software Foundation, Inc.

   This file was written by Peter Miller <pmiller@agso.gov.au>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

%{

/* The bison generated parser uses alloca.  AIX 3 forces us to put this
   declaration at the beginning of the file.  The declaration in bison's
   skeleton file comes too late.  This must come before <config.h>
   because <config.h> may include arbitrary system headers.  */
#if defined _AIX && !defined __GNUC__
 #pragma alloca
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Specification.  */
#include "po-hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "xalloc.h"
#include "read-po-abstract.h"

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in the same program.  Note that these are only
   the variables produced by yacc.  If other parser generators (bison,
   byacc, etc) produce additional global names that conflict at link time,
   then those parser generators need to be fixed instead of adding those
   names to this list. */

#define yymaxdepth po_hash_maxdepth
#define yyparse po_hash_parse
#define yylex   po_hash_lex
#define yyerror po_hash_error
#define yylval  po_hash_lval
#define yychar  po_hash_char
#define yydebug po_hash_debug
#define yypact  po_hash_pact
#define yyr1    po_hash_r1
#define yyr2    po_hash_r2
#define yydef   po_hash_def
#define yychk   po_hash_chk
#define yypgo   po_hash_pgo
#define yyact   po_hash_act
#define yyexca  po_hash_exca
#define yyerrflag po_hash_errflag
#define yynerrs po_hash_nerrs
#define yyps    po_hash_ps
#define yypv    po_hash_pv
#define yys     po_hash_s
#define yy_yys  po_hash_yys
#define yystate po_hash_state
#define yytmp   po_hash_tmp
#define yyv     po_hash_v
#define yy_yyv  po_hash_yyv
#define yyval   po_hash_val
#define yylloc  po_hash_lloc
#define yyreds  po_hash_reds          /* With YYDEBUG defined */
#define yytoks  po_hash_toks          /* With YYDEBUG defined */
#define yylhs   po_hash_yylhs
#define yylen   po_hash_yylen
#define yydefred po_hash_yydefred
#define yydgoto po_hash_yydgoto
#define yysindex po_hash_yysindex
#define yyrindex po_hash_yyrindex
#define yygindex po_hash_yygindex
#define yytable  po_hash_yytable
#define yycheck  po_hash_yycheck

%}

%token STRING
%token NUMBER
%token COLON
%token COMMA
%token FILE_KEYWORD
%token LINE_KEYWORD
%token NUMBER_KEYWORD

%union
{
  char *string;
  size_t number;
}

%type <number> NUMBER
%type <string> STRING

%{

/* Forward declarations, to avoid gcc warnings.  */
void yyerror (char *);
int yylex (void);


void
yyerror (char *s)
{
  /* Do nothing, the grammar is used as a recogniser.  */
}
%}

%%

filepos_line
	: /* empty */
	| filepos_line filepos
	;

filepos
	: STRING COLON NUMBER
		{
		  /* GNU style */
		  po_callback_comment_filepos ($1, $3);
		  free ($1);
		}
	| STRING
		{
		  /* GNU style, without line number (e.g. from Pascal .rst) */
		  po_callback_comment_filepos ($1, (size_t)(-1));
		  free ($1);
		}
	| FILE_KEYWORD COLON STRING COMMA LINE_KEYWORD COLON NUMBER
		{
		  /* SunOS style */
		  po_callback_comment_filepos ($3, $7);
		  free ($3);
		}
	| FILE_KEYWORD COLON STRING COMMA LINE_KEYWORD NUMBER_KEYWORD COLON NUMBER
		{
		  /* Solaris style */
		  po_callback_comment_filepos ($3, $8);
		  free ($3);
		}
	| FILE_KEYWORD COLON NUMBER
		{
		  /* GNU style, but STRING is `file'.  Esoteric, but it
		     happened.  */
		  po_callback_comment_filepos ("file", $3);
		}
	;

%%


/* Current position in the pseudo-comment line.  */
static const char *cur;

/* The NUMBER token must only be recognized after colon.  So we have to
   remember whether the last token was a colon.  */
static bool last_was_colon;

/* Returns the next token from the current pseudo-comment line.  */
int
yylex ()
{
  static char *buf;
  static size_t bufmax;

  bool look_for_number = last_was_colon;
  last_was_colon = false;

  for (;;)
    {
      int c = *cur++;
      switch (c)
	{
	case 0:
	  --cur;
	  return 0;

	case ' ':
	case '\t':
	case '\n':
	  break;

	case ':':
	  last_was_colon = true;
	  return COLON;

	case ',':
	  return COMMA;

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  if (look_for_number)
	    {
	      /* Accumulate a number.  */
	      size_t n = 0;

	      for (;;)
		{
		  n = n * 10 + c - '0';
		  c = *cur++;
		  switch (c)
		    {
		    default:
		      break;

		    case '0':
		    case '1':
		    case '2':
		    case '3':
		    case '4':
		    case '5':
		    case '6':
		    case '7':
		    case '8':
		    case '9':
		      continue;
		    }
		  break;
		}
	      --cur;
	      yylval.number = n;
	      return NUMBER;
	    }
	  /* FALLTHROUGH */

	default:
	  /* Accumulate a string.  */
	  {
	    size_t bufpos;

	    bufpos = 0;
	    for (;;)
	      {
		if (bufpos >= bufmax)
		  {
		    bufmax += 100;
		    buf = xrealloc (buf, bufmax);
		  }
		buf[bufpos++] = c;

		c = *cur++;
		switch (c)
		  {
		  default:
		    continue;

		  case 0:
		  case ':':
		  case ',':
		  case ' ':
		  case '\t':
		    --cur;
		    break;
		  }
		break;
	      }

	    if (bufpos >= bufmax)
	      {
		bufmax += 100;
		buf = xrealloc (buf, bufmax);
	      }
	    buf[bufpos] = 0;

	    if (strcmp (buf, "file") == 0 || strcmp (buf, "File") == 0)
	      return FILE_KEYWORD;
	    if (strcmp (buf, "line") == 0)
	      return LINE_KEYWORD;
	    if (strcmp (buf, "number") == 0)
	      return NUMBER_KEYWORD;
	    yylval.string = xstrdup (buf);
	    return STRING;
	  }
	}
    }
}


/* Analyze whether the string (a pseudo-comment line) contains file names
   and line numbers.  */
int
po_parse_comment_filepos (const char *s)
{
  cur = s;
  last_was_colon = false;
  return yyparse ();
}
