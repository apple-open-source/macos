%{
/*
 * Lexer for states.
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
 * $Id: lex.l,v 1.1.1.1 2003/03/05 07:25:52 mtr Exp $
 */

#include "defs.h"
#include "gram.h"

static void eat_comment ();
static char *read_string ___P ((unsigned int *len_return));
static void read_regexp ___P ((Node *node));
%}

real	[+-]?[0-9]+\.[0-9]*|[+-]?\.[0-9]+
integer	[+-]?[0-9]+
symbol	[a-zA-Z_][a-zA-Z_0-9]*|\$.

%%

"/*" 		{ eat_comment (); }
[ \t\r\f]	{ ; }
\n		{ linenum++; }

\"		{ yylval.node = node_alloc (nSTRING);
		  yylval.node->u.str.data
	            = read_string (&yylval.node->u.str.len);
		  return tSTRING;
		}

'[^\\]' 	{ yylval.node = node_alloc (nINTEGER);
		  yylval.node->u.integer = yytext[1];
		  return tINTEGER;
		}

'\\.'		{ yylval.node = node_alloc (nINTEGER);
		  switch (yytext[2])
		    {
		    case 'n':
		      yylval.node->u.integer = '\n';
		      break;

		    case 't':
		      yylval.node->u.integer = '\t';
		      break;

		    case 'v':
		      yylval.node->u.integer = '\v';
		      break;

		    case 'b':
		      yylval.node->u.integer = '\b';
		      break;

		    case 'r':
		      yylval.node->u.integer = '\r';
		      break;

		    case 'f':
		      yylval.node->u.integer = '\f';
		      break;

		    case 'a':
		      yylval.node->u.integer = '\a';
		      break;

		    default:
		      yylval.node->u.integer = yytext[2];
		      break;
		    }

		  return tINTEGER;
		}

\/		{ yylval.node = node_alloc (nREGEXP);
		  read_regexp (yylval.node);
		  return tREGEXP;
		}

"BEGIN" 	{ return tBEGIN; }
"END" 		{ return tEND; }
"div" 		{ return tDIV; }
"else" 		{ return tELSE; }
"extends"	{ return tEXTENDS; }
"for" 		{ return tFOR; }
"if" 		{ return tIF; }
"local"		{ return tLOCAL; }
"namerules" 	{ return tNAMERULES; }
"return"	{ return tRETURN; }
"start" 	{ return tSTART; }
"startrules" 	{ return tSTARTRULES; }
"state" 	{ return tSTATE; }
"sub" 		{ return tSUB; }
"while"		{ return tWHILE; }

"==" 		{ return tEQ; }
"!=" 		{ return tNE; }
"<=" 		{ return tLE; }
">=" 		{ return tGE; }
"&&" 		{ return tAND; }
"||" 		{ return tOR; }
"++" 		{ return tPLUSPLUS; }
"--" 		{ return tMINUSMINUS; }
"+="		{ return tADDASSIGN; }
"-="		{ return tSUBASSIGN; }
"*="		{ return tMULASSIGN; }
"div="		{ return tDIVASSIGN; }

{real} 		{ yylval.node = node_alloc (nREAL);
		  yylval.node->u.real = atof (yytext);
		  return tREAL;
		}
{integer}	{ yylval.node = node_alloc (nINTEGER);
		  yylval.node->u.integer = atoi (yytext);
		  return tINTEGER;
		}
{symbol}	{ yylval.node = node_alloc (nSYMBOL);
		  yylval.node->u.sym = xstrdup (yytext);
		  return tSYMBOL;
		}

.		{ return yytext[0]; }

%%

static void
eat_comment ()
{
  int c;

  while ((c = input ()) != EOF)
    {
      if (c == '\n')
	linenum++;
      else if (c == '*')
	{
	  c = input ();
	  if (c == '/')
	    /* All done. */
	    return;

	  if (c == EOF)
	    {
	      yyerror (_("error: EOF in comment"));
	      break;
	    }
	  unput (c);
	}
    }
  yyerror (_("error: EOF in comment"));
}


int
yywrap ()
{
  return 1;
}

static char *
read_string (len_return)
     unsigned int *len_return;
{
  char *buf = NULL;
  char *buf2;
  int buflen = 0;
  int bufpos = 0;
  int ch;
  int done = 0;

  while (!done)
    {
      ch = input ();
      if (ch == '\n')
	linenum++;

      switch (ch)
        {
        case EOF:
        unexpected_eof:
          yyerror (_("error: EOF in string constant"));
	  done = 1;
          break;

        case '"':
          done = 1;
          break;

        case '\\':
          ch = input ();
          switch (ch)
            {
            case 'n':
              ch = '\n';
              break;

            case 't':
              ch = '\t';
              break;

            case 'v':
              ch = '\v';
              break;

            case 'b':
              ch = '\b';
              break;

            case 'r':
              ch = '\r';
              break;

            case 'f':
              ch = '\f';
              break;

            case 'a':
              ch = '\a';
              break;

            case EOF:
              goto unexpected_eof;
              break;

            default:
	      if (ch == '0')
		{
		  int i;
		  int val = 0;

		  for (i = 0; i < 3; i++)
		    {
		      ch = input ();
		      if ('0' <= ch && ch <= '7')
			val = val * 8 + ch - '0';
		      else
			{
			  unput (ch);
			  break;
			}
		    }
		  ch = val;
		}
              break;
            }
          /* FALLTHROUGH */

        default:
          if (bufpos >= buflen)
            {
              buflen += 1024;
              buf = (char *) xrealloc (buf, buflen);
            }
          buf[bufpos++] = ch;
          break;
        }
    }

  buf2 = (char *) xmalloc (bufpos + 1);
  memcpy (buf2, buf, bufpos);
  buf2[bufpos] = '\0';
  xfree (buf);

  *len_return = bufpos;

  return buf2;
}


static void
read_regexp (node)
     Node *node;
{
  char *buf = NULL;
  char *buf2;
  int buflen = 0;
  int bufpos = 0;
  int ch;
  int done = 0;

  while (!done)
    {
      ch = input ();
      switch (ch)
        {
        case EOF:
        unexpected_eof:
	  yyerror (_("error: EOF in regular expression"));
	  done = 1;
          break;

        case '/':
          done = 1;
          break;

        case '\\':
          ch = input ();
          switch (ch)
            {
	    case '\n':
	      /* Line break. */
	      linenum++;
	      continue;
	      break;

            case 'n':
              ch = '\n';
              break;

            case 'r':
              ch = '\r';
              break;

            case 'f':
              ch = '\f';
              break;

            case 't':
              ch = '\t';
              break;

	    case '/':
	    case '\\':
	      /* Quote these. */
	      break;

            case EOF:
              goto unexpected_eof;
              break;

            default:
	      if (ch == '0')
		{
		  int i;
		  int val = 0;

		  for (i = 0; i < 3; i++)
		    {
		      ch = input ();
		      if ('0' <= ch && ch <= '7')
			val = val * 8 + ch - '0';
		      else
			{
			  unput (ch);
			  break;
			}
		    }
		  ch = val;
		}
	      else
		{
		  /* Pass it through. */
		  unput (ch);
		  ch = '\\';
		}
	      break;
            }
          /* FALLTHROUGH */

        default:
          if (bufpos >= buflen)
            {
              buflen += 1024;
              buf = (char *) xrealloc (buf, buflen);
            }
          buf[bufpos++] = ch;
          break;
        }
    }

  /* Possible options. */
  done = 0;
  while (!done)
    {
      ch = input ();
      switch (ch)
	{
	case 'i':
	  /* Case-insensitive regular expression. */
	  node->u.re.flags |= fRE_CASE_INSENSITIVE;
	  break;

	default:
	  /* Unknown option => this belongs to the next token. */
	  unput (ch);
	  done = 1;
	  break;
	}
    }

  buf2 = (char *) xmalloc (bufpos + 1);
  memcpy (buf2, buf, bufpos);
  buf2[bufpos] = '\0';
  xfree (buf);

  node->u.re.data = buf2;
  node->u.re.len = bufpos;
}


/*
Local variables:
mode: c
End:
*/
