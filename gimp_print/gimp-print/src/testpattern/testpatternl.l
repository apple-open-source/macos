/*
 * "$Id: testpatternl.l,v 1.1.1.2 2004/07/23 06:26:32 jlovell Exp $"
 *
 *   Test pattern generator for Gimp-Print
 *
 *   Copyright 2001 Robert Krawitz <rlk@alum.mit.edu>
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

%{

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "testpattern.h"

#define YY_NO_UNPUT

int mylineno = 1;

static char *
c_strstrip(char *s)
{
  size_t len = strlen(s);
  if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') ||
		   (s[0] == '\'' && s[len - 1] == '\'')))
    {
      char *sn = malloc(len - 1);
      memcpy(sn, s + 1, len - 2);
      sn[len - 2] = '\0';
      return sn;
    }
  else
    return (char *) s;
}

#define DBG(x)						\
do							\
  {							\
    if (getenv("STP_TESTPATTERN_DEBUG"))		\
      fprintf(stderr, "'%s' => %s\n", yytext, #x);	\
  }							\
 while (0)


%}

%option noyywrap

digit		[0-9]
integer		[-+]?{digit}+
float		[-+]?{digit}+(\.{digit}+)?([eE][-+]?{digit}+)?
string		([\"][^\"]+[\"])|([a-zA-Z0-9_][-a-zA-Z0-9_]*)|([\'][^\']*[\'])
ws		[ \t]+

%%

cyan			yylval.sval = yytext;DBG(CYAN); return CYAN;
l_cyan			yylval.sval = yytext;DBG(L_CYAN); return L_CYAN;
magenta			yylval.sval = yytext;DBG(MAGENTA); return MAGENTA;
l_magenta		yylval.sval = yytext;DBG(L_MAGENTA); return L_MAGENTA;
yellow			yylval.sval = yytext;DBG(YELLOW); return YELLOW;
d_yellow		yylval.sval = yytext;DBG(D_YELLOW); return D_YELLOW;
black			yylval.sval = yytext;DBG(BLACK); return BLACK;
l_black			yylval.sval = yytext;DBG(L_BLACK); return L_BLACK;
gamma			DBG(GAMMA); return GAMMA;
level			DBG(LEVEL); return LEVEL;
steps			DBG(STEPS); return STEPS;
ink_limit		DBG(INK_LIMIT); return INK_LIMIT;
printer			DBG(PRINTER); return PRINTER;
parameter		DBG(PARAMETER); return PARAMETER;
parameter_int		DBG(PARAMETER_INT); return PARAMETER_INT;
parameter_string	DBG(PARAMETER); return PARAMETER;
parameter_float		DBG(PARAMETER_FLOAT); return PARAMETER_FLOAT;
parameter_curve		DBG(PARAMETER_CURVE); return PARAMETER_CURVE;
density			DBG(DENSITY); return DENSITY;
top			DBG(TOP); return TOP;
left			DBG(LEFT); return LEFT;
hsize			DBG(HSIZE); return HSIZE;
vsize			DBG(VSIZE); return VSIZE;
blackline		DBG(BLACKLINE); return BLACKLINE;
pattern			DBG(PATTERN); return PATTERN;
xpattern		DBG(XPATTERN); return XPATTERN;
image			DBG(IMAGE); return IMAGE;
extended		DBG(EXTENDED); return EXTENDED;
grid			DBG(GRID); return GRID;
channel			DBG(CHANNEL); return CHANNEL;
page_size		DBG(PAGESIZE); return PAGESIZE;
;			DBG(SEMI); return SEMI;
mode			DBG(MODE); return MODE;
cmyk			yylval.ival = CMYK;DBG(CMYK); return CMYK;
kcmy			yylval.ival = CMYK;DBG(KCMY); return KCMY;
rgb			yylval.ival = RGB;DBG(RGB); return RGB;
cmy			yylval.ival = RGB;DBG(CMY); return CMY;
grey			yylval.ival = GRAY;DBG(GRAY); return GRAY;
gray			yylval.ival = GRAY;DBG(GRAY); return GRAY;
white			yylval.ival = WHITE;DBG(WHITE); return WHITE;
message			DBG(MESSAGE); return MESSAGE;
end			DBG(END); return END;


{integer}		yylval.ival = atoi(yytext); DBG(tINT); return tINT;
{float}			yylval.dval = strtod(yytext, NULL); DBG(tDOUBLE); return tDOUBLE;
{string}		yylval.sval = c_strstrip(yytext); DBG(tSTRING); return tSTRING;
{ws}			DBG(whitespace); 	/* Skip blanks/tabs */
#[^\n]*			DBG(comment); 	/* Skip comments */
\n			DBG(newline); mylineno++;
