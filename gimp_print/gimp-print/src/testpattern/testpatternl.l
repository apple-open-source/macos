/*
 * "$Id: testpatternl.l,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
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

#if 0
#define DBG(x) fprintf(stderr, "'%s'%s\n", yytext, #x);
#else
#define DBG(x)
#endif

%}

%option noyywrap

digit		[0-9]
integer		[-+]?{digit}+
float		[-+]?{digit}+(\.{digit}+)?([eE][-+]?{digit}+)?
string		([\"][^\"\n]+[\"])|([^ \t\n"]+)
ws		[ \t]+

%%

c_gamma			DBG(C_GAMMA) return C_GAMMA;
m_gamma			DBG(M_GAMMA) return M_GAMMA;
y_gamma			DBG(Y_GAMMA) return Y_GAMMA;
k_gamma			DBG(K_GAMMA) return K_GAMMA;
gamma			DBG(GAMMA) return GAMMA;
c_level			DBG(C_LEVEL) return C_LEVEL;
m_level			DBG(M_LEVEL) return M_LEVEL;
y_level			DBG(Y_LEVEL) return Y_LEVEL;
levels			DBG(LEVELS) return LEVELS;
ink_limit		DBG(INK_LIMIT) return INK_LIMIT;
width			DBG(WIDTH) return WIDTH;
printer			DBG(PRINTER) return PRINTER;
ink_type		DBG(INK_TYPE) return INK_TYPE;
resolution		DBG(RESOLUTION) return RESOLUTION;
media_source		DBG(MEDIA_SOURCE) return MEDIA_SOURCE;
media_type		DBG(MEDIA_TYPE) return MEDIA_TYPE;
media_size		DBG(MEDIA_SIZE) return MEDIA_SIZE;
dither_algorithm	DBG(DITHER_ALGORITHM) return DITHER_ALGORITHM;
density			DBG(DENSITY) return DENSITY;
top			DBG(TOP) return TOP;
left			DBG(LEFT) return LEFT;
hsize			DBG(HSIZE) return HSIZE;
vsize			DBG(VSIZE) return VSIZE;
blackline		DBG(BLACKLINE) return BLACKLINE;
pattern			DBG(PATTERN) return PATTERN;
image			DBG(IMAGE) return IMAGE;

{integer}		yylval.ival = atoi(yytext); DBG(tINT) return tINT;
{float}			yylval.dval = strtod(yytext, NULL); DBG(tDOUBLE) return tDOUBLE;
{string}		yylval.sval = c_strdup(yytext); DBG(tSTRING) return tSTRING;
{ws}			DBG(whitespace1) 	/* Skip blanks/tabs */
#[^\n]*			DBG(comment1) 	/* Skip comments */
\n			DBG(newline) mylineno++;
