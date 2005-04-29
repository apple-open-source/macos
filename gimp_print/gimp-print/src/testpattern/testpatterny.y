/*
 * "$Id: testpatterny.y,v 1.1.1.3 2004/12/22 23:49:40 jlovell Exp $"
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

extern char* yytext;

static int yyerror( const char *s )
{
	fprintf(stderr,"stdin:%d: %s before '%s'\n",mylineno,s,yytext);
	return 0;
}

typedef struct
{
  const char *name;
  int channel;
} color_t;

static color_t color_map[] =
  {
    { "black", 0 },
    { "cyan", 1 },
    { "red", 1 },
    { "magenta", 2 },
    { "green", 2 },
    { "yellow", 3 },
    { "blue", 3 },
    { "l_black", 4 },
    { "l_cyan", 5 },
    { "l_magenta", 6 },
    { "d_yellow", 4 },
    { NULL, -1 }
  };

static int current_index = 0;
static testpattern_t *current_testpattern;
extern FILE *yyin;

static int
find_color(const char *name)
{
  int i = 0;
  while (color_map[i].name)
    {
      if (strcmp(color_map[i].name, name) == 0)
	return color_map[i].channel;
      i++;
    }
  return -1;
}

%}

%token <ival> tINT
%token <dval> tDOUBLE
%token <sval> tSTRING

%token CYAN
%token L_CYAN
%token MAGENTA
%token L_MAGENTA
%token YELLOW
%token D_YELLOW
%token BLACK
%token L_BLACK
%token GAMMA
%token LEVEL
%token STEPS
%token INK_LIMIT
%token PRINTER
%token PARAMETER
%token PARAMETER_INT
%token PARAMETER_FLOAT
%token PARAMETER_CURVE
%token DENSITY
%token TOP
%token LEFT
%token HSIZE
%token VSIZE
%token BLACKLINE
%token PATTERN
%token XPATTERN
%token EXTENDED
%token IMAGE
%token GRID
%token SEMI
%token CHANNEL
%token CMYK
%token KCMY
%token RGB
%token CMY
%token GRAY
%token WHITE
%token RAW
%token MODE
%token PAGESIZE
%token MESSAGE
%token END

%start Thing

%%

COLOR: CYAN | L_CYAN | MAGENTA | L_MAGENTA
	| YELLOW | D_YELLOW | BLACK | L_BLACK
;

cmykspec: CMYK tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>cmykspec %d\n", $2);
	  global_image_type = "CMYK";
	  global_channel_depth = 4;
	  global_invert_data = 0;
	  if ($2 == 8 || $2 == 16)
	    global_bit_depth = $2;
	}
;

kcmyspec: KCMY tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>kcmyspec %d\n", $2);
	  global_image_type = "KCMY";
	  global_channel_depth = 4;
	  global_invert_data = 0;
	  if ($2 == 8 || $2 == 16)
	    global_bit_depth = $2;
	}
;

rgbspec: RGB tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>rgbspec %d\n", $2);
	  global_image_type = "RGB";
	  global_channel_depth = 3;
	  global_invert_data = 1;
	  if ($2 == 8 || $2 == 16)
	    global_bit_depth = $2;
	}
;

cmyspec: CMY tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>cmyspec %d\n", $2);
	  global_image_type = "CMY";
	  global_channel_depth = 3;
	  global_invert_data = 0;
	  if ($2 == 8 || $2 == 16)
	    global_bit_depth = $2;
	}
;

grayspec: GRAY tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>grayspec %d\n", $2);
	  global_image_type = "Grayscale";
	  global_channel_depth = 1;
	  global_invert_data = 0;
	  if ($2 == 8 || $2 == 16)
	    global_bit_depth = $2;
	}
;

whitespec: WHITE tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>whitespec %d\n", $2);
	  global_image_type = "Whitescale";
	  global_channel_depth = 1;
	  global_invert_data = 1;
	  if ($2 == 8 || $2 == 16)
	    global_bit_depth = $2;
	}
;

extendedspec: EXTENDED tINT tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>extendedspec %d\n", $2);
	  global_image_type = "Raw";
	  global_invert_data = 0;
	  global_channel_depth = $2;
	  if ($2 == 8 || $2 == 16)
	    global_bit_depth = $3;
	}
;

modespec: cmykspec | kcmyspec | rgbspec | cmyspec | grayspec | whitespec | extendedspec
;

inputspec: MODE modespec
;

level: LEVEL COLOR tDOUBLE
	{
	  int channel = find_color($2.sval);
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>level %s %f\n", $2.sval, $3);
	  if (channel >= 0)
	    global_levels[channel] = $3;
	}
;

channel_level: LEVEL tINT tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>channel_level %d %f\n", $2, $3);
	  if ($2 >= 0 && $2 <= STP_CHANNEL_LIMIT)
	    global_levels[$2] = $3;
	}
;

gamma: GAMMA COLOR tDOUBLE
	{
	  int channel = find_color($2.sval);
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>gamma %s %f\n", $2.sval, $3);
	  if (channel >= 0)
	    global_gammas[channel] = $3;
	}
;

channel_gamma: GAMMA tINT tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>channel_gamma %d %f\n", $2, $3);
	  if ($2 >= 0 && $2 <= STP_CHANNEL_LIMIT)
	    global_gammas[$2] = $3;
	}
;

global_gamma: GAMMA tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>global_gamma %f\n", $2);
	  global_gamma = $2;
	}
;
steps: STEPS tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>steps %d\n", $2);
	  global_steps = $2;
	}
;
ink_limit: INK_LIMIT tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>ink_limit %f\n", $2);
	  global_ink_limit = $2;
	}
;
printer: PRINTER tSTRING
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>printer %s\n", $2);
	  global_printer = strdup($2);
	  free($2);
	}
;

page_size_name: PAGESIZE tSTRING
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>page_size_name %s\n", $2);
	  stp_set_string_parameter(global_vars, "PageSize", $2);
	  free($2);
	}
;

page_size_custom: PAGESIZE tINT tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>page_size_custom %d %d\n", $2, $3);
	  stp_set_page_width(global_vars, $2);
	  stp_set_page_height(global_vars, $3);
	}
;

page_size: page_size_name | page_size_custom
;

parameter_string: PARAMETER tSTRING tSTRING
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>parameter_string %s %s\n", $2, $3);
	  stp_set_string_parameter(global_vars, $2, $3);
	  free($2);
	  free($3);
	}
;

parameter_int: PARAMETER_INT tSTRING tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>parameter_int %s %d\n", $2, $3);
	  stp_set_int_parameter(global_vars, $2, $3);
	  free($2);
	}
;

parameter_float: PARAMETER_FLOAT tSTRING tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>parameter_float %s %f\n", $2, $3);
	  stp_set_float_parameter(global_vars, $2, $3);
	  free($2);
	}
;

parameter_curve: PARAMETER_CURVE tSTRING tSTRING
	{
	  stp_curve_t *curve = stp_curve_create_from_string($3);
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>parameter_curve %s %s\n", $2, $3);
	  if (curve)
	    {
	      stp_set_curve_parameter(global_vars, $2, curve);
	      stp_curve_destroy(curve);
	    }
	  free($2);
	}
;

parameter: parameter_string | parameter_int | parameter_float
;
density: DENSITY tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>density %f\n", $2);
	  global_density = $2;
	}
;
top: TOP tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>top %f\n", $2);
	  global_xtop = $2;
	}
;
left: LEFT tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>left %f\n", $2);
	  global_xleft = $2;
	}
;
hsize: HSIZE tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>hsize %f\n", $2);
	  global_hsize = $2;
	}
;
vsize: VSIZE tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>vsize %f\n", $2);
	  global_vsize = $2;
	}
;
blackline: BLACKLINE tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>blackline %d\n", $2);
	  global_noblackline = !($2);
	}
;

color_block1: tDOUBLE tDOUBLE tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>color_block1 %f %f %f (%d)\n", $1, $2, $3,
		    current_index);
	  if (current_index < STP_CHANNEL_LIMIT)
	    {
	      current_testpattern->d.p.mins[current_index] = $1;
	      current_testpattern->d.p.vals[current_index] = $2;
	      current_testpattern->d.p.gammas[current_index] = $3;
	      current_index++;
	    }
	}
;

color_blocks1a: color_block1 | color_blocks1a color_block1
;

color_blocks1b: /* empty */ | color_blocks1a
;

color_blocks1: color_block1 color_blocks1b
;

color_block2a: COLOR tDOUBLE tDOUBLE tDOUBLE
	{
	  int channel = find_color($1.sval);
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>color_block2a %s %f %f %f\n", $1.sval, $2, $3, $4);
	  if (channel >= 0 && channel < STP_CHANNEL_LIMIT)
	    {
	      current_testpattern->d.p.mins[channel] = $2;
	      current_testpattern->d.p.vals[channel] = $3;
	      current_testpattern->d.p.gammas[channel] = $4;
	    }
	}
;

color_block2b: CHANNEL tINT tDOUBLE tDOUBLE tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>color_block2b %d %f %f %f\n", $2, $3, $4, $5);
	  if ($2 >= 0 && $2 < STP_CHANNEL_LIMIT)
	    {
	      current_testpattern->d.p.mins[$2] = $3;
	      current_testpattern->d.p.vals[$2] = $4;
	      current_testpattern->d.p.gammas[$2] = $5;
	    }
	}
;

color_block2: color_block2a | color_block2b
;

color_blocks2a: color_block2 | color_blocks2a color_block2
;

color_blocks2: /* empty */ | color_blocks2a
;

color_blocks: color_blocks1 | color_blocks2
;

patvars: tDOUBLE tDOUBLE tDOUBLE tDOUBLE tDOUBLE
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>patvars %f %f %f %f %f\n", $1, $2, $3, $4, $5);
	  current_testpattern->t = E_PATTERN;
	  current_testpattern->d.p.lower = $1;
	  current_testpattern->d.p.upper = $2;
	  current_testpattern->d.p.levels[1] = $3;
	  current_testpattern->d.p.levels[2] = $4;
	  current_testpattern->d.p.levels[3] = $5;
	  current_testpattern = get_next_testpattern();
	  current_index = 0;
	}
;

pattern: PATTERN patvars color_blocks
;

xpattern: XPATTERN color_blocks
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>xpattern\n");
	  if (global_channel_depth == 0)
	    {
	      fprintf(stderr, "xpattern may only be used with extended color depth\n");
	      exit(1);
	    }
	  current_testpattern->t = E_XPATTERN;
	  current_testpattern = get_next_testpattern();
	  current_index = 0;
	}
;

grid: GRID tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>grid %d\n", $2);
	  current_testpattern->t = E_GRID;
	  current_testpattern->d.g.ticks = $2;
	  current_testpattern = get_next_testpattern();
	  current_index = 0;
	}
;

image: IMAGE tINT tINT
	{
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>image %d %d\n", $2, $3);
	  current_testpattern->t = E_IMAGE;
	  current_testpattern->d.i.x = $2;
	  current_testpattern->d.i.y = $3;
	  if (current_testpattern->d.i.x <= 0 ||
	      current_testpattern->d.i.y <= 0)
	    {
	      fprintf(stderr, "image width and height must be greater than zero\n");
	      exit(1);
	    }
	  return 0;
	}
;

Message0: MESSAGE tSTRING
	{
	  fprintf(stderr, $2);
	  free($2);
	}
;
Message1: MESSAGE tSTRING tSTRING
	{
	  fprintf(stderr, $2, $3);
	  free($2);
	  free($3);
	}
;
Message2: MESSAGE tSTRING tSTRING tSTRING
	{
	  fprintf(stderr, $2, $3, $4);
	  free($2);
	  free($3);
	  free($4);
	}
;
Message3: MESSAGE tSTRING tSTRING tSTRING tSTRING
	{
	  fprintf(stderr, $2, $3, $4, $5);
	  free($2);
	  free($3);
	  free($4);
	  free($5);
	}
;
Message4: MESSAGE tSTRING tSTRING tSTRING tSTRING tSTRING
	{
	  fprintf(stderr, $2, $3, $4, $5, $6);
	  free($2);
	  free($3);
	  free($4);
	  free($5);
	  free($6);
	}
;

A_Message: Message0 | Message1 | Message2 | Message3 | Message4
;

message: A_Message
;

A_Rule: gamma | channel_gamma | level | channel_level | global_gamma | steps
	| ink_limit | printer | parameter | density | top | left | hsize
	| vsize | blackline | inputspec | page_size | message
;

Rule: A_Rule SEMI
	{ global_did_something = 1; }
;

A_Pattern: pattern | xpattern | grid | message
;

Pattern: A_Pattern SEMI
	{ global_did_something = 1; }
;

Patterns: /* empty */ | Patterns Pattern
;

Image: image
	{ global_did_something = 1; }
;

Rules: /* empty */ | Rules Rule
;

Output: Patterns | Image
;

EOF: /* empty */ | END SEMI
	{ return 0; }
;

Thing: 	Rules
	{
	  current_testpattern = get_next_testpattern();
	}
	Output EOF
;

%%
