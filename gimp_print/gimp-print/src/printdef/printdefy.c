
/*  A Bison parser, made from printdefy.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	tINT	258
#define	tDOUBLE	259
#define	tSTRING	260
#define	tCLASS	261
#define	tBEGIN	262
#define	tEND	263
#define	ASSIGN	264
#define	PRINTER	265
#define	NAME	266
#define	DRIVER	267
#define	COLOR	268
#define	NOCOLOR	269
#define	MODEL	270
#define	LANGUAGE	271
#define	BRIGHTNESS	272
#define	GAMMA	273
#define	CONTRAST	274
#define	CYAN	275
#define	MAGENTA	276
#define	YELLOW	277
#define	SATURATION	278
#define	DENSITY	279
#define	ENDPRINTER	280
#define	VALUE	281

#line 23 "printdefy.y"


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
int yyerror(const char *s);

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

#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		98
#define	YYFLAG		-32768
#define	YYNTBASE	27

#define YYTRANSLATE(x) ((unsigned)(x) <= 281 ? yytranslate[x] : 48)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,    10,    20,    24,    28,    32,    39,    46,    53,    60,
    67,    74,    81,    88,    95,   102,   103,   105,   107,   109,
   111,   113,   115,   117,   119,   121,   123,   125,   127,   129,
   131,   134,   136,   140,   143,   146
};

static const short yyrhs[] = {     7,
    10,    11,     9,     5,    12,     9,     5,     8,     0,     7,
    10,    12,     9,     5,    11,     9,     5,     8,     0,     7,
    25,     8,     0,     7,    13,     8,     0,     7,    14,     8,
     0,     7,    15,    26,     9,     3,     8,     0,     7,    16,
    26,     9,     6,     8,     0,     7,    17,    26,     9,     4,
     8,     0,     7,    18,    26,     9,     4,     8,     0,     7,
    19,    26,     9,     4,     8,     0,     7,    20,    26,     9,
     4,     8,     0,     7,    21,    26,     9,     4,     8,     0,
     7,    22,    26,     9,     4,     8,     0,     7,    23,    26,
     9,     4,     8,     0,     7,    24,    26,     9,     4,     8,
     0,     0,    27,     0,    28,     0,    30,     0,    31,     0,
    32,     0,    33,     0,    34,     0,    35,     0,    36,     0,
    37,     0,    38,     0,    39,     0,    40,     0,    41,     0,
    45,    44,     0,    44,     0,    43,    45,    29,     0,    43,
    29,     0,    47,    46,     0,    42,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   125,   128,   131,   134,   137,   140,   143,   156,   159,   162,
   165,   168,   171,   174,   177,   181,   183,   183,   186,   186,
   186,   186,   186,   186,   186,   187,   187,   187,   187,   187,
   189,   189,   191,   191,   193,   193
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","tINT","tDOUBLE",
"tSTRING","tCLASS","tBEGIN","tEND","ASSIGN","PRINTER","NAME","DRIVER","COLOR",
"NOCOLOR","MODEL","LANGUAGE","BRIGHTNESS","GAMMA","CONTRAST","CYAN","MAGENTA",
"YELLOW","SATURATION","DENSITY","ENDPRINTER","VALUE","printerstart","printerstartalt",
"printerend","color","nocolor","model","language","brightness","gamma","contrast",
"cyan","magenta","yellow","saturation","density","Empty","pstart","parg","pargs",
"Printer","Printers", NULL
};
#endif

static const short yyr1[] = {     0,
    27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    41,    42,    43,    43,    44,    44,
    44,    44,    44,    44,    44,    44,    44,    44,    44,    44,
    45,    45,    46,    46,    47,    47
};

static const short yyr2[] = {     0,
     9,     9,     3,     3,     3,     6,     6,     6,     6,     6,
     6,     6,     6,     6,     6,     0,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     2,     1,     3,     2,     2,     1
};

static const short yydefact[] = {    16,
    36,     0,     0,    17,    18,     0,    35,     0,     0,    34,
    19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
    29,    30,    32,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,    33,
    31,     0,     0,     4,     5,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     3,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     6,     7,     8,     9,    10,    11,    12,    13,
    14,    15,     0,     0,     1,     2,     0,     0
};

static const short yydefgoto[] = {     4,
     5,    10,    11,    12,    13,    14,    15,    16,    17,    18,
    19,    20,    21,    22,     1,     6,    23,    24,     7,     2
};

static const short yypact[] = {-32768,
-32768,    13,     6,-32768,-32768,    10,-32768,     3,   -13,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,    10,     9,    12,    11,    14,    -3,    -2,
    -1,     0,     1,     2,     4,     5,     7,     8,    21,-32768,
-32768,    27,    30,-32768,-32768,    28,    29,    31,    32,    33,
    34,    35,    36,    37,    38,-32768,    24,    39,    45,    43,
    47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
    58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
    57,    71,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,    69,    70,-32768,-32768,    79,-32768
};

static const short yypgoto[] = {-32768,
-32768,    15,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,    68,-32768,-32768,-32768
};


#define	YYLAST		92


static const short yytable[] = {    27,
    28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
    38,    39,    97,    25,    26,     8,     9,    42,    44,     3,
    43,    45,    46,    47,    48,    49,    50,    51,    56,    52,
    53,    57,    54,    55,    58,    69,    59,    60,    40,    61,
    62,    63,    64,    65,    66,    67,    68,    71,    72,    70,
    73,    74,    75,    76,    77,    78,    79,    80,     0,     0,
     0,    93,     0,    81,    82,    83,    84,    85,    86,    87,
    88,    89,    90,    91,    92,    94,    95,    96,    98,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,    41
};

static const short yycheck[] = {    13,
    14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
    24,    25,     0,    11,    12,    10,     7,     9,     8,     7,
     9,     8,    26,    26,    26,    26,    26,    26,     8,    26,
    26,     5,    26,    26,     5,    12,     9,     9,    24,     9,
     9,     9,     9,     9,     9,     9,     9,     3,     6,    11,
     4,     4,     4,     4,     4,     4,     4,     4,    -1,    -1,
    -1,     5,    -1,     9,     9,     8,     8,     8,     8,     8,
     8,     8,     8,     8,     8,     5,     8,     8,     0,    -1,
    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    24
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison.simple"

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

#ifndef YYPARSE_RETURN_TYPE
#define YYPARSE_RETURN_TYPE int
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
YYPARSE_RETURN_TYPE yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 196 "/usr/share/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

YYPARSE_RETURN_TYPE
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
#line 126 "printdefy.y"
{ initialize_the_printer(yyvsp[-4].sval, yyvsp[-1].sval); ;
    break;}
case 2:
#line 129 "printdefy.y"
{ initialize_the_printer(yyvsp[-1].sval, yyvsp[-4].sval); ;
    break;}
case 3:
#line 132 "printdefy.y"
{ output_the_printer(); ;
    break;}
case 4:
#line 135 "printdefy.y"
{ thePrinter.printvars.output_type = OUTPUT_COLOR; ;
    break;}
case 5:
#line 138 "printdefy.y"
{ thePrinter.printvars.output_type = OUTPUT_GRAY; ;
    break;}
case 6:
#line 141 "printdefy.y"
{ thePrinter.model = yyvsp[-1].ival; ;
    break;}
case 7:
#line 144 "printdefy.y"
{
	  int i;
	  for (i = 0; i < nprintfuncs; i++)
	    {
	      if (!strcmp(yyvsp[-1].sval, printfuncs[i]))
		{
		  thePrinter.printvars.top = i;
		  break;
		}
	    }
	;
    break;}
case 8:
#line 157 "printdefy.y"
{ thePrinter.printvars.brightness = yyvsp[-1].dval; ;
    break;}
case 9:
#line 160 "printdefy.y"
{ thePrinter.printvars.gamma = yyvsp[-1].dval; ;
    break;}
case 10:
#line 163 "printdefy.y"
{ thePrinter.printvars.contrast = yyvsp[-1].dval; ;
    break;}
case 11:
#line 166 "printdefy.y"
{ thePrinter.printvars.cyan = yyvsp[-1].dval; ;
    break;}
case 12:
#line 169 "printdefy.y"
{ thePrinter.printvars.magenta = yyvsp[-1].dval; ;
    break;}
case 13:
#line 172 "printdefy.y"
{ thePrinter.printvars.yellow = yyvsp[-1].dval; ;
    break;}
case 14:
#line 175 "printdefy.y"
{ thePrinter.printvars.saturation = yyvsp[-1].dval; ;
    break;}
case 15:
#line 178 "printdefy.y"
{ thePrinter.printvars.density = yyvsp[-1].dval; ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "/usr/share/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}
#line 195 "printdefy.y"


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
