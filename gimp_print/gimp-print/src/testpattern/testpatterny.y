/*
 * "$Id: testpatterny.y,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
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

extern int mylineno;

extern int yylex(void);
char *quotestrip(const char *i);
char *endstrip(const char *i);

extern int mylineno;
extern char* yytext;

static int yyerror( const char *s )
{
	fprintf(stderr,"stdin:%d: %s before '%s'\n",mylineno,s,yytext);
	return 0;
}

%}

%token <ival> tINT
%token <dval> tDOUBLE
%token <sval> tSTRING

%token C_GAMMA
%token M_GAMMA
%token Y_GAMMA
%token K_GAMMA
%token GAMMA
%token C_LEVEL
%token M_LEVEL
%token Y_LEVEL
%token LEVELS
%token INK_LIMIT
%token INK
%token WIDTH
%token PRINTER
%token INK_TYPE
%token RESOLUTION
%token MEDIA_SOURCE
%token MEDIA_TYPE
%token MEDIA_SIZE
%token DITHER_ALGORITHM
%token DENSITY
%token TOP
%token LEFT
%token HSIZE
%token VSIZE
%token BLACKLINE
%token PATTERN
%token IMAGE

%start Thing

%%

global_c_level: C_LEVEL tDOUBLE
	{ global_c_level = $2; }
;
global_m_level: M_LEVEL tDOUBLE
	{ global_m_level = $2; }
;
global_y_level: Y_LEVEL tDOUBLE
	{ global_y_level = $2; }
;
global_c_gamma: C_GAMMA tDOUBLE
	{ global_c_gamma = $2; }
;
global_m_gamma: M_GAMMA tDOUBLE
	{ global_m_gamma = $2; }
;
global_y_gamma: Y_GAMMA tDOUBLE
	{ global_y_gamma = $2; }
;
global_k_gamma: K_GAMMA tDOUBLE
	{ global_k_gamma = $2; }
;
global_gamma: GAMMA tDOUBLE
	{ global_gamma = $2; }
;
levels: LEVELS tINT
	{ levels = $2; }
;
ink_limit: INK_LIMIT tDOUBLE
	{ ink_limit = $2; }
;
printer: PRINTER tSTRING
	{ printer = c_strdup($2); }
;
ink_type: INK_TYPE tSTRING
	{ ink_type = c_strdup($2); }
;
resolution: RESOLUTION tSTRING
	{ resolution = c_strdup($2); }
;
media_source: MEDIA_SOURCE tSTRING
	{ media_source = c_strdup($2); }
;
media_type: MEDIA_TYPE tSTRING
	{ media_type = c_strdup($2); }
;
media_size: MEDIA_SIZE tSTRING
	{ media_size = c_strdup($2); }
;
dither_algorithm: DITHER_ALGORITHM tSTRING
	{ dither_algorithm = c_strdup($2); }
;
density: DENSITY tDOUBLE
	{ density = $2; }
;
top: TOP tDOUBLE
	{ xtop = $2; }
;
left: LEFT tDOUBLE
	{ xleft = $2; }
;
hsize: HSIZE tDOUBLE
	{ hsize = $2; }
;
vsize: VSIZE tDOUBLE
	{ vsize = $2; }
;
blackline: BLACKLINE tINT
	{ noblackline = !($2); }
;

pattern: PATTERN tDOUBLE tDOUBLE tDOUBLE tDOUBLE tDOUBLE tDOUBLE tDOUBLE
	tDOUBLE tDOUBLE tDOUBLE tDOUBLE tDOUBLE tDOUBLE tDOUBLE tDOUBLE
	tDOUBLE tDOUBLE
	{
	  testpattern_t *t = get_next_testpattern();
	  t->t = E_PATTERN;
	  t->d.p.c_min = $2;
	  t->d.p.c = $3;
	  t->d.p.c_gamma = $4;
	  t->d.p.m_min = $5;
	  t->d.p.m = $6;
	  t->d.p.m_gamma = $7;
	  t->d.p.y_min = $8;
	  t->d.p.y = $9;
	  t->d.p.y_gamma = $10;
	  t->d.p.k_min = $11;
	  t->d.p.k = $12;
	  t->d.p.k_gamma = $13;
	  t->d.p.c_level = $14;
	  t->d.p.m_level = $15;
	  t->d.p.y_level = $16;
	  t->d.p.lower = $17;
	  t->d.p.upper = $18;
	}
;

image: IMAGE tINT tINT
	{
	  testpattern_t *t = get_next_testpattern();
	  t->t = E_IMAGE;
	  t->d.i.x = $2;
	  t->d.i.y = $3;
	  if (t->d.i.x <= 0 || t->d.i.y <= 0)
	    {
	      fprintf(stderr, "image width and height must be greater than zero\n");
	      exit(1);
	    }
	  return 0;
	}

Empty:

Rule: global_c_level | global_m_level | global_y_level
	| global_c_gamma | global_m_gamma | global_y_gamma | global_k_gamma
	| global_gamma | levels | ink_limit | printer | ink_type | resolution
	| media_source | media_type | media_size | dither_algorithm | density
	| top | left | hsize | vsize | blackline

Patterns: Patterns pattern | Empty

Image: image

Rules: Rules Rule | Empty

Output: Patterns | Image

Thing: Rules Output Empty

%%
