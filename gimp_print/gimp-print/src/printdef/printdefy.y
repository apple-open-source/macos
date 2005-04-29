/*
 * "$Id: printdefy.y,v 1.1.1.2 2004/05/03 21:30:32 jlovell Exp $"
 *
 *   Parse printer definition pseudo-XML
 *
 *   Copyright 2000 Robert Krawitz (rlk@alum.mit.edu)
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "printdef.h"

extern int mylineno;
stp_printer_t thePrinter;
char *quotestrip(const char *i);
char *endstrip(const char *i);

extern int yylex(void);
void initialize_the_printer(const char *name, const char *driver);
void output_the_printer(void);
static int yyerror(const char *s);

const char *printfuncs[] =
{
  "canon",
  "escp2",
  "pcl",
  "ps",
  "lexmark"
};

const size_t nprintfuncs = sizeof(printfuncs) / sizeof(const char *);

void
initialize_the_printer(const char *name, const char *driver)
{
  strncpy(thePrinter.printvars.output_to, name, 63);
  strncpy(thePrinter.printvars.driver, driver, 63);
  thePrinter.printvars.top = -1;
  thePrinter.model = -1;
  thePrinter.printvars.brightness = 1.0;
  thePrinter.printvars.gamma = 1.0;
  thePrinter.printvars.contrast = 1.0;
  thePrinter.printvars.cyan = 1.0;
  thePrinter.printvars.magenta = 1.0;
  thePrinter.printvars.yellow = 1.0;
  thePrinter.printvars.saturation = 1.0;
  thePrinter.printvars.density = 1.0;
}

void
output_the_printer(void)
{
  printf("  {\n");
  printf("    %s,\n", thePrinter.printvars.output_to);
  printf("    %s,\n", thePrinter.printvars.driver);
  printf("    %d,\n", thePrinter.model);
  printf("    &stp_%s_printfuncs,\n", printfuncs[thePrinter.printvars.top]);
  printf("    {\n");
  printf("      \"\",\n");	/* output_to */
  printf("      %s,\n", thePrinter.printvars.driver);	/* driver */
  printf("      \"\",\n");	/* ppd_file */
  printf("      \"\",\n");	/* resolution */
  printf("      \"\",\n");	/* media_size */
  printf("      \"\",\n");	/* media_type */
  printf("      \"\",\n");	/* media_source */
  printf("      \"\",\n");	/* ink_type */
  printf("      \"\",\n");	/* dither_algorithm */
  printf("      %d,\n", thePrinter.printvars.output_type);
  printf("      %.3f,\n", thePrinter.printvars.brightness);
  printf("      1.0,\n");	/* scaling */
  printf("      -1,\n");	/* orientation */
  printf("      0,\n");		/* top */
  printf("      0,\n");		/* left */
  printf("      %.3f,\n", thePrinter.printvars.gamma);
  printf("      %.3f,\n", thePrinter.printvars.contrast);
  printf("      %.3f,\n", thePrinter.printvars.cyan);
  printf("      %.3f,\n", thePrinter.printvars.magenta);
  printf("      %.3f,\n", thePrinter.printvars.yellow);
  printf("      %.3f,\n", thePrinter.printvars.saturation);
  printf("      %.3f,\n", thePrinter.printvars.density);
  printf("    }\n");
  printf("  },\n");
}

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
%token <sval> tSTRING tCLASS
%token tBEGIN tEND ASSIGN PRINTER NAME DRIVER COLOR NOCOLOR MODEL
%token LANGUAGE BRIGHTNESS GAMMA CONTRAST
%token CYAN MAGENTA YELLOW SATURATION DENSITY ENDPRINTER VALUE

%start Printers

%%

printerstart:	tBEGIN PRINTER NAME ASSIGN tSTRING DRIVER ASSIGN tSTRING tEND
	{ initialize_the_printer($5, $8); }
;
printerstartalt: tBEGIN PRINTER DRIVER ASSIGN tSTRING NAME ASSIGN tSTRING tEND
	{ initialize_the_printer($8, $5); }
;
printerend: 		tBEGIN ENDPRINTER tEND
	{ output_the_printer(); }
;
color:			tBEGIN COLOR tEND
	{ thePrinter.printvars.output_type = OUTPUT_COLOR; }
;
nocolor:		tBEGIN NOCOLOR tEND
	{ thePrinter.printvars.output_type = OUTPUT_GRAY; }
;
model:			tBEGIN MODEL VALUE ASSIGN tINT tEND
	{ thePrinter.model = $5; }
;
language:		tBEGIN LANGUAGE VALUE ASSIGN tCLASS tEND
	{
	  int i;
	  for (i = 0; i < nprintfuncs; i++)
	    {
	      if (!strcmp($5, printfuncs[i]))
		{
		  thePrinter.printvars.top = i;
		  break;
		}
	    }
	}
;
brightness:		tBEGIN BRIGHTNESS VALUE ASSIGN tDOUBLE tEND
	{ thePrinter.printvars.brightness = $5; }
;
gamma:			tBEGIN GAMMA VALUE ASSIGN tDOUBLE tEND
	{ thePrinter.printvars.gamma = $5; }
;
contrast:		tBEGIN CONTRAST VALUE ASSIGN tDOUBLE tEND
	{ thePrinter.printvars.contrast = $5; }
;
cyan:			tBEGIN CYAN VALUE ASSIGN tDOUBLE tEND
	{ thePrinter.printvars.cyan = $5; }
;
magenta:			tBEGIN MAGENTA VALUE ASSIGN tDOUBLE tEND
	{ thePrinter.printvars.magenta = $5; }
;
yellow:			tBEGIN YELLOW VALUE ASSIGN tDOUBLE tEND
	{ thePrinter.printvars.yellow = $5; }
;
saturation:		tBEGIN SATURATION VALUE ASSIGN tDOUBLE tEND
	{ thePrinter.printvars.saturation = $5; }
;
density:		tBEGIN DENSITY VALUE ASSIGN tDOUBLE tEND
	{ thePrinter.printvars.density = $5; }
;

Empty:
;

pstart: printerstart | printerstartalt
;

parg: color | nocolor | model | language | brightness | gamma | contrast
	| cyan | magenta | yellow | saturation | density
;

pargs: pargs parg | parg
;

Printer: pstart pargs printerend | pstart printerend
;

Printers: Printers Printer | Empty
;

%%

int
main(int argc, char **argv)
{
  int retval;
  int i;
  printf("/* This file is automatically generated.  See printers.xml.\n");
  printf("   DO NOT EDIT! */\n\n");
  for (i = 0; i < nprintfuncs; i++)
    printf("const extern stp_printfuncs_t stp_%s_printfuncs;\n",
	   printfuncs[i]);
  printf("\nstatic const stp_internal_printer_t printers[] =\n");
  printf("{\n");
  retval = yyparse();
  printf("};\n");
  printf("static const int printer_count = sizeof(printers) / sizeof(stp_internal_printer_t);\n");
  return retval;
}
