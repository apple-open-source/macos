
/*  A Bison parser, made from gram.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define	tSYMBOL	258
#define	tREGEXP	259
#define	tSTRING	260
#define	tINTEGER	261
#define	tREAL	262
#define	tSUB	263
#define	tSTATE	264
#define	tSTART	265
#define	tSTARTRULES	266
#define	tNAMERULES	267
#define	tBEGIN	268
#define	tEND	269
#define	tRETURN	270
#define	tIF	271
#define	tELSE	272
#define	tLOCAL	273
#define	tWHILE	274
#define	tFOR	275
#define	tADDASSIGN	276
#define	tSUBASSIGN	277
#define	tMULASSIGN	278
#define	tDIVASSIGN	279
#define	tOR	280
#define	tAND	281
#define	tEQ	282
#define	tNE	283
#define	tGE	284
#define	tLE	285
#define	tDIV	286
#define	tPLUSPLUS	287
#define	tMINUSMINUS	288

#line 1 "gram.y"

/* 								-*- c -*-
 * Grammar for states.
 * Copyright (c) 1997 Markku Rossi.
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
 * $Id: gram.c,v 1.3 2003/01/31 19:18:51 melville Exp $
 */

#include "defs.h"

#line 35 "gram.y"
typedef union
{
  List *lst;
  Node *node;
  Cons *cons;
  Stmt *stmt;
  Expr *expr;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		162
#define	YYFLAG		-32768
#define	YYNTBASE	51

#define YYTRANSLATE(x) ((unsigned)(x) <= 288 ? yytranslate[x] : 67)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    40,     2,     2,     2,     2,     2,     2,    49,
    50,    38,    36,    48,    37,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    27,    47,    32,
    21,    33,    26,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    43,     2,    44,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    45,     2,    46,     2,     2,     2,     2,     2,
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
    16,    17,    18,    19,    20,    22,    23,    24,    25,    28,
    29,    30,    31,    34,    35,    39,    41,    42
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     4,     9,    14,    19,    25,    27,    28,    33,
    34,    37,    42,    47,    52,    57,    58,    60,    62,    66,
    67,    71,    73,    77,    79,    83,    84,    87,    90,    94,
   104,   108,   114,   122,   128,   138,   141,   143,   145,   147,
   149,   151,   154,   158,   162,   167,   171,   175,   179,   183,
   187,   190,   193,   196,   199,   206,   210,   215,   221,   225,
   229,   233,   237,   241,   245,   249,   253,   257,   261,   262,
   264,   265,   267,   269
};

static const short yyrhs[] = {    -1,
    51,    52,     0,    10,    45,    61,    46,     0,    11,    45,
    53,    46,     0,    12,    45,    53,    46,     0,     9,     3,
    45,    54,    46,     0,    62,     0,     0,    53,     4,     3,
    47,     0,     0,    54,    55,     0,    13,    45,    61,    46,
     0,    14,    45,    61,    46,     0,     4,    45,    61,    46,
     0,     3,    45,    61,    46,     0,     0,    57,     0,     3,
     0,    57,    48,     3,     0,     0,    18,    59,    47,     0,
    60,     0,    59,    48,    60,     0,     3,     0,     3,    21,
    63,     0,     0,    61,    62,     0,    15,    47,     0,    15,
    63,    47,     0,     8,     3,    49,    56,    50,    45,    58,
    61,    46,     0,    45,    61,    46,     0,    16,    49,    63,
    50,    62,     0,    16,    49,    63,    50,    62,    17,    62,
     0,    19,    49,    63,    50,    62,     0,    20,    49,    64,
    47,    63,    47,    64,    50,    62,     0,    63,    47,     0,
     5,     0,     4,     0,     6,     0,     7,     0,     3,     0,
    40,    63,     0,    63,    29,    63,     0,    63,    28,    63,
     0,     3,    49,    65,    50,     0,     3,    21,    63,     0,
     3,    22,    63,     0,     3,    23,    63,     0,     3,    24,
    63,     0,     3,    25,    63,     0,     3,    41,     0,     3,
    42,     0,    41,     3,     0,    42,     3,     0,    63,    43,
    63,    44,    21,    63,     0,    49,    63,    50,     0,    63,
    43,    63,    44,     0,    63,    26,    63,    27,    63,     0,
    63,    38,    63,     0,    63,    39,    63,     0,    63,    36,
    63,     0,    63,    37,    63,     0,    63,    32,    63,     0,
    63,    33,    63,     0,    63,    30,    63,     0,    63,    31,
    63,     0,    63,    34,    63,     0,    63,    35,    63,     0,
     0,    63,     0,     0,    66,     0,    63,     0,    66,    48,
    63,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
    67,    68,    71,    72,    74,    76,    78,    81,    82,    86,
    87,    89,    90,    91,    92,    95,    96,    99,   100,   103,
   104,   107,   108,   111,   112,   115,   116,   119,   121,   123,
   128,   130,   132,   135,   137,   140,   144,   146,   148,   150,
   152,   154,   156,   157,   158,   160,   162,   164,   166,   168,
   170,   172,   174,   176,   178,   180,   181,   183,   185,   186,
   187,   188,   190,   191,   192,   193,   194,   195,   198,   199,
   202,   203,   206,   207
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","tSYMBOL",
"tREGEXP","tSTRING","tINTEGER","tREAL","tSUB","tSTATE","tSTART","tSTARTRULES",
"tNAMERULES","tBEGIN","tEND","tRETURN","tIF","tELSE","tLOCAL","tWHILE","tFOR",
"'='","tADDASSIGN","tSUBASSIGN","tMULASSIGN","tDIVASSIGN","'?'","':'","tOR",
"tAND","tEQ","tNE","'<'","'>'","tGE","tLE","'+'","'-'","'*'","tDIV","'!'","tPLUSPLUS",
"tMINUSMINUS","'['","']'","'{'","'}'","';'","','","'('","')'","file","toplevel",
"regexp_sym_list","staterules","staterule","symbol_list","rest_symbol_list",
"locals","locals_rest","local_def","stmt_list","stmt","expr","cond_expr","expr_list",
"rest_expr_list", NULL
};
#endif

static const short yyr1[] = {     0,
    51,    51,    52,    52,    52,    52,    52,    53,    53,    54,
    54,    55,    55,    55,    55,    56,    56,    57,    57,    58,
    58,    59,    59,    60,    60,    61,    61,    62,    62,    62,
    62,    62,    62,    62,    62,    62,    63,    63,    63,    63,
    63,    63,    63,    63,    63,    63,    63,    63,    63,    63,
    63,    63,    63,    63,    63,    63,    63,    63,    63,    63,
    63,    63,    63,    63,    63,    63,    63,    63,    64,    64,
    65,    65,    66,    66
};

static const short yyr2[] = {     0,
     0,     2,     4,     4,     4,     5,     1,     0,     4,     0,
     2,     4,     4,     4,     4,     0,     1,     1,     3,     0,
     3,     1,     3,     1,     3,     0,     2,     2,     3,     9,
     3,     5,     7,     5,     9,     2,     1,     1,     1,     1,
     1,     2,     3,     3,     4,     3,     3,     3,     3,     3,
     2,     2,     2,     2,     6,     3,     4,     5,     3,     3,
     3,     3,     3,     3,     3,     3,     3,     3,     0,     1,
     0,     1,     1,     3
};

static const short yydefact[] = {     1,
     0,    41,    38,    37,    39,    40,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,    26,     0,
     2,     7,     0,     0,     0,     0,     0,     0,    51,    52,
    71,     0,     0,    26,     8,     8,    28,     0,     0,     0,
    69,    42,    53,    54,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    36,    46,    47,    48,    49,    50,    73,     0,    72,    16,
    10,     0,     0,     0,    29,     0,     0,    70,     0,    31,
    27,    56,     0,    44,    43,    65,    66,    63,    64,    67,
    68,    61,    62,    59,    60,     0,    45,     0,    18,     0,
    17,     0,     3,     0,     4,     5,     0,     0,     0,     0,
    57,    74,     0,     0,     0,     0,     0,     0,     6,    11,
     0,    32,    34,     0,    58,     0,    20,    19,    26,    26,
    26,    26,     9,     0,    69,    55,     0,    26,     0,     0,
     0,     0,    33,     0,    24,     0,    22,     0,    15,    14,
    12,    13,     0,     0,    21,     0,    30,    35,    25,    23,
     0,     0
};

static const short yydefgoto[] = {     1,
    21,    73,   102,   120,   100,   101,   138,   146,   147,    45,
    81,    23,    79,    68,    69
};

static const short yypact[] = {-32768,
    51,   312,-32768,-32768,-32768,-32768,     1,     8,   -33,   -22,
   -21,   281,   -24,   -23,   -16,   303,    24,    31,-32768,   303,
-32768,-32768,   380,   303,   303,   303,   303,   303,-32768,-32768,
   303,   -14,    20,-32768,-32768,-32768,-32768,   400,   303,   303,
   303,    10,-32768,-32768,   115,    47,   303,   303,   303,   303,
   303,   303,   303,   303,   303,   303,   303,   303,   303,   303,
-32768,   475,   475,   475,   475,   475,   475,    19,    26,    65,
-32768,   134,     4,     6,-32768,   334,   357,   475,    25,-32768,
-32768,-32768,   459,   490,   504,   516,   516,    89,    89,    89,
    89,   -36,   -36,    10,    10,   440,-32768,   303,-32768,    38,
    41,    18,-32768,    91,-32768,-32768,   275,   275,   303,   303,
    74,   475,    60,   105,    64,    66,    67,    68,-32768,-32768,
    69,    97,-32768,   420,   475,   303,    99,-32768,-32768,-32768,
-32768,-32768,-32768,   275,   303,   475,   126,-32768,   162,   181,
   209,   228,-32768,    86,   123,   -29,-32768,   256,-32768,-32768,
-32768,-32768,   275,   303,-32768,   126,-32768,-32768,   475,-32768,
   145,-32768
};

static const short yypgoto[] = {-32768,
-32768,   110,-32768,-32768,-32768,-32768,-32768,-32768,    -9,   -28,
    -1,   -11,    13,-32768,-32768
};


#define	YYLAST		559


static const short yytable[] = {    22,
    38,    58,    59,    32,    42,    72,    60,   104,    46,   104,
    33,    34,    62,    63,    64,    65,    66,   155,   156,    67,
   115,   116,    35,    36,    39,    40,    43,    76,    77,    78,
   117,   118,    41,    44,    70,    83,    84,    85,    86,    87,
    88,    89,    90,    91,    92,    93,    94,    95,    96,   105,
   161,   106,    60,     2,     3,     4,     5,     6,     7,     8,
     9,    10,    11,   119,    71,    12,    13,    99,    97,    14,
    15,   109,    47,    98,    48,    49,    50,    51,    52,    53,
    54,    55,    56,    57,    58,    59,   112,   113,   114,    60,
    16,    17,    18,   121,   126,    19,    82,   124,   125,    20,
   139,   140,   141,   142,   127,   122,   123,   128,   129,   148,
   130,   131,   132,   134,   136,   133,   137,     2,     3,     4,
     5,     6,     7,    78,    56,    57,    58,    59,   145,    12,
    13,    60,   143,    14,    15,   153,     2,     3,     4,     5,
     6,     7,   159,   154,   162,    74,   160,   144,    12,    13,
     0,   158,    14,    15,    16,    17,    18,     0,     0,    19,
    80,     0,     0,    20,     2,     3,     4,     5,     6,     7,
     0,     0,     0,    16,    17,    18,    12,    13,    19,   103,
    14,    15,    20,     2,     3,     4,     5,     6,     7,     0,
     0,     0,     0,     0,     0,    12,    13,     0,     0,    14,
    15,    16,    17,    18,     0,     0,    19,   149,     0,     0,
    20,     2,     3,     4,     5,     6,     7,     0,     0,     0,
    16,    17,    18,    12,    13,    19,   150,    14,    15,    20,
     2,     3,     4,     5,     6,     7,     0,     0,     0,     0,
     0,     0,    12,    13,     0,     0,    14,    15,    16,    17,
    18,     0,     0,    19,   151,     0,     0,    20,     2,     3,
     4,     5,     6,     7,     0,     0,     0,    16,    17,    18,
    12,    13,    19,   152,    14,    15,    20,     2,     3,     4,
     5,     6,     7,     2,     3,     4,     5,     6,     0,    12,
    13,     0,     0,    14,    15,    16,    17,    18,     0,     0,
    19,   157,     0,     0,    20,     2,     3,     4,     5,     6,
     0,     0,     0,     0,    16,    17,    18,     0,     0,    19,
    16,    17,    18,    20,     0,     0,     0,    37,     0,    20,
     0,     0,    24,    25,    26,    27,    28,     0,     0,     0,
     0,     0,    16,    17,    18,     0,     0,     0,     0,     0,
     0,    20,    29,    30,     0,     0,     0,     0,     0,    47,
    31,    48,    49,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,     0,     0,     0,    60,     0,     0,     0,
     0,     0,    47,   107,    48,    49,    50,    51,    52,    53,
    54,    55,    56,    57,    58,    59,     0,     0,     0,    60,
     0,     0,     0,     0,     0,    47,   108,    48,    49,    50,
    51,    52,    53,    54,    55,    56,    57,    58,    59,     0,
     0,     0,    60,     0,     0,    47,    61,    48,    49,    50,
    51,    52,    53,    54,    55,    56,    57,    58,    59,     0,
     0,     0,    60,     0,     0,    47,    75,    48,    49,    50,
    51,    52,    53,    54,    55,    56,    57,    58,    59,     0,
     0,     0,    60,     0,     0,    47,   135,    48,    49,    50,
    51,    52,    53,    54,    55,    56,    57,    58,    59,     0,
     0,     0,    60,   111,    47,   110,    48,    49,    50,    51,
    52,    53,    54,    55,    56,    57,    58,    59,     0,     0,
    47,    60,    48,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,     0,     0,     0,    60,    49,    50,
    51,    52,    53,    54,    55,    56,    57,    58,    59,     0,
     0,     0,    60,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,     0,     0,     0,    60,    52,    53,    54,
    55,    56,    57,    58,    59,     0,     0,     0,    60
};

static const short yycheck[] = {     1,
    12,    38,    39,     3,    16,    34,    43,     4,    20,     4,
     3,    45,    24,    25,    26,    27,    28,    47,    48,    31,
     3,     4,    45,    45,    49,    49,     3,    39,    40,    41,
    13,    14,    49,     3,    49,    47,    48,    49,    50,    51,
    52,    53,    54,    55,    56,    57,    58,    59,    60,    46,
     0,    46,    43,     3,     4,     5,     6,     7,     8,     9,
    10,    11,    12,    46,    45,    15,    16,     3,    50,    19,
    20,    47,    26,    48,    28,    29,    30,    31,    32,    33,
    34,    35,    36,    37,    38,    39,    98,    50,    48,    43,
    40,    41,    42,     3,    21,    45,    50,   109,   110,    49,
   129,   130,   131,   132,    45,   107,   108,     3,    45,   138,
    45,    45,    45,    17,   126,    47,    18,     3,     4,     5,
     6,     7,     8,   135,    36,    37,    38,    39,     3,    15,
    16,    43,   134,    19,    20,    50,     3,     4,     5,     6,
     7,     8,   154,    21,     0,    36,   156,   135,    15,    16,
    -1,   153,    19,    20,    40,    41,    42,    -1,    -1,    45,
    46,    -1,    -1,    49,     3,     4,     5,     6,     7,     8,
    -1,    -1,    -1,    40,    41,    42,    15,    16,    45,    46,
    19,    20,    49,     3,     4,     5,     6,     7,     8,    -1,
    -1,    -1,    -1,    -1,    -1,    15,    16,    -1,    -1,    19,
    20,    40,    41,    42,    -1,    -1,    45,    46,    -1,    -1,
    49,     3,     4,     5,     6,     7,     8,    -1,    -1,    -1,
    40,    41,    42,    15,    16,    45,    46,    19,    20,    49,
     3,     4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,
    -1,    -1,    15,    16,    -1,    -1,    19,    20,    40,    41,
    42,    -1,    -1,    45,    46,    -1,    -1,    49,     3,     4,
     5,     6,     7,     8,    -1,    -1,    -1,    40,    41,    42,
    15,    16,    45,    46,    19,    20,    49,     3,     4,     5,
     6,     7,     8,     3,     4,     5,     6,     7,    -1,    15,
    16,    -1,    -1,    19,    20,    40,    41,    42,    -1,    -1,
    45,    46,    -1,    -1,    49,     3,     4,     5,     6,     7,
    -1,    -1,    -1,    -1,    40,    41,    42,    -1,    -1,    45,
    40,    41,    42,    49,    -1,    -1,    -1,    47,    -1,    49,
    -1,    -1,    21,    22,    23,    24,    25,    -1,    -1,    -1,
    -1,    -1,    40,    41,    42,    -1,    -1,    -1,    -1,    -1,
    -1,    49,    41,    42,    -1,    -1,    -1,    -1,    -1,    26,
    49,    28,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    -1,    -1,    -1,    43,    -1,    -1,    -1,
    -1,    -1,    26,    50,    28,    29,    30,    31,    32,    33,
    34,    35,    36,    37,    38,    39,    -1,    -1,    -1,    43,
    -1,    -1,    -1,    -1,    -1,    26,    50,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
    -1,    -1,    43,    -1,    -1,    26,    47,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
    -1,    -1,    43,    -1,    -1,    26,    47,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
    -1,    -1,    43,    -1,    -1,    26,    47,    28,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
    -1,    -1,    43,    44,    26,    27,    28,    29,    30,    31,
    32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
    26,    43,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    -1,    -1,    -1,    43,    29,    30,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
    -1,    -1,    43,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    -1,    -1,    -1,    43,    32,    33,    34,
    35,    36,    37,    38,    39,    -1,    -1,    -1,    43
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/local/share/bison.simple"

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

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
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

#line 196 "/usr/local/share/bison.simple"

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

int
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

case 3:
#line 71 "gram.y"
{ start_stmts = yyvsp[-1].lst; ;
    break;}
case 4:
#line 73 "gram.y"
{ startrules = yyvsp[-1].lst; ;
    break;}
case 5:
#line 75 "gram.y"
{ namerules = yyvsp[-1].lst; ;
    break;}
case 6:
#line 77 "gram.y"
{ define_state (yyvsp[-3].node, yyvsp[-1].lst); ;
    break;}
case 7:
#line 78 "gram.y"
{ list_append (global_stmts, yyvsp[0].stmt); ;
    break;}
case 8:
#line 81 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 9:
#line 83 "gram.y"
{ list_append (yyvsp[-3].lst, cons (yyvsp[-2].node, yyvsp[-1].node)); ;
    break;}
case 10:
#line 86 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 11:
#line 87 "gram.y"
{ list_append (yyvsp[-1].lst, yyvsp[0].cons); ;
    break;}
case 12:
#line 89 "gram.y"
{ yyval.cons = cons (RULE_BEGIN, yyvsp[-1].lst); ;
    break;}
case 13:
#line 90 "gram.y"
{ yyval.cons = cons (RULE_END, yyvsp[-1].lst); ;
    break;}
case 14:
#line 91 "gram.y"
{ yyval.cons = cons (yyvsp[-3].node, yyvsp[-1].lst); ;
    break;}
case 15:
#line 92 "gram.y"
{ yyval.cons = cons (yyvsp[-3].node, yyvsp[-1].lst); ;
    break;}
case 16:
#line 95 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 17:
#line 96 "gram.y"
{ yyval.lst = yyvsp[0].lst; ;
    break;}
case 18:
#line 99 "gram.y"
{ yyval.lst = list (); list_append (yyval.lst, yyvsp[0].node); ;
    break;}
case 19:
#line 100 "gram.y"
{ list_append (yyvsp[-2].lst, yyvsp[0].node); ;
    break;}
case 20:
#line 103 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 21:
#line 104 "gram.y"
{ yyval.lst = yyvsp[-1].lst; ;
    break;}
case 22:
#line 107 "gram.y"
{ yyval.lst = list (); list_append (yyval.lst, yyvsp[0].cons); ;
    break;}
case 23:
#line 108 "gram.y"
{ list_append (yyvsp[-2].lst, yyvsp[0].cons); ;
    break;}
case 24:
#line 111 "gram.y"
{ yyval.cons = cons (yyvsp[0].node, NULL); ;
    break;}
case 25:
#line 112 "gram.y"
{ yyval.cons = cons (yyvsp[-2].node, yyvsp[0].expr); ;
    break;}
case 26:
#line 115 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 27:
#line 116 "gram.y"
{ list_append (yyvsp[-1].lst, yyvsp[0].stmt); ;
    break;}
case 28:
#line 119 "gram.y"
{ yyval.stmt = mk_stmt (sRETURN, NULL, NULL,
							NULL, NULL); ;
    break;}
case 29:
#line 121 "gram.y"
{ yyval.stmt = mk_stmt (sRETURN, yyvsp[-1].expr, NULL,
							NULL, NULL); ;
    break;}
case 30:
#line 124 "gram.y"
{ yyval.stmt = mk_stmt (sDEFSUB, yyvsp[-7].node,
							cons (cons (yyvsp[-5].lst, yyvsp[-2].lst),
							      yyvsp[-1].lst),
							NULL, NULL); ;
    break;}
case 31:
#line 128 "gram.y"
{ yyval.stmt = mk_stmt (sBLOCK, yyvsp[-1].lst, NULL,
							NULL, NULL); ;
    break;}
case 32:
#line 130 "gram.y"
{ yyval.stmt = mk_stmt (sIF, yyvsp[-2].expr, yyvsp[0].stmt, NULL,
							NULL); ;
    break;}
case 33:
#line 133 "gram.y"
{ yyval.stmt = mk_stmt (sIF, yyvsp[-4].expr, yyvsp[-2].stmt, yyvsp[0].stmt,
							NULL); ;
    break;}
case 34:
#line 135 "gram.y"
{ yyval.stmt = mk_stmt (sWHILE, yyvsp[-2].expr, yyvsp[0].stmt,
							NULL, NULL); ;
    break;}
case 35:
#line 138 "gram.y"
{ yyval.stmt = mk_stmt (sFOR, yyvsp[-6].expr, yyvsp[-4].expr, yyvsp[-2].expr,
							yyvsp[0].stmt); ;
    break;}
case 36:
#line 140 "gram.y"
{ yyval.stmt = mk_stmt (sEXPR, yyvsp[-1].expr, NULL,
							NULL, NULL); ;
    break;}
case 37:
#line 144 "gram.y"
{ yyval.expr = mk_expr (eSTRING, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 38:
#line 146 "gram.y"
{ yyval.expr = mk_expr (eREGEXP, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 39:
#line 148 "gram.y"
{ yyval.expr = mk_expr (eINTEGER, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 40:
#line 150 "gram.y"
{ yyval.expr = mk_expr (eREAL, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 41:
#line 152 "gram.y"
{ yyval.expr = mk_expr (eSYMBOL, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 42:
#line 154 "gram.y"
{ yyval.expr = mk_expr (eNOT, yyvsp[0].expr, NULL,
							NULL); ;
    break;}
case 43:
#line 156 "gram.y"
{ yyval.expr = mk_expr (eAND, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 44:
#line 157 "gram.y"
{ yyval.expr = mk_expr (eOR, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 45:
#line 158 "gram.y"
{ yyval.expr = mk_expr (eFCALL, yyvsp[-3].node, yyvsp[-1].lst,
							NULL); ;
    break;}
case 46:
#line 160 "gram.y"
{ yyval.expr = mk_expr (eASSIGN, yyvsp[-2].node, yyvsp[0].expr,
							NULL); ;
    break;}
case 47:
#line 162 "gram.y"
{ yyval.expr = mk_expr (eADDASSIGN, yyvsp[-2].node, yyvsp[0].expr,
							NULL); ;
    break;}
case 48:
#line 164 "gram.y"
{ yyval.expr = mk_expr (eSUBASSIGN, yyvsp[-2].node, yyvsp[0].expr,
							NULL); ;
    break;}
case 49:
#line 166 "gram.y"
{ yyval.expr = mk_expr (eMULASSIGN, yyvsp[-2].node, yyvsp[0].expr,
							NULL); ;
    break;}
case 50:
#line 168 "gram.y"
{ yyval.expr = mk_expr (eDIVASSIGN, yyvsp[-2].node, yyvsp[0].expr,
							NULL); ;
    break;}
case 51:
#line 170 "gram.y"
{ yyval.expr = mk_expr (ePOSTFIXADD, yyvsp[-1].node, NULL,
							NULL); ;
    break;}
case 52:
#line 172 "gram.y"
{ yyval.expr = mk_expr (ePOSTFIXSUB, yyvsp[-1].node, NULL,
							NULL); ;
    break;}
case 53:
#line 174 "gram.y"
{ yyval.expr = mk_expr (ePREFIXADD, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 54:
#line 176 "gram.y"
{ yyval.expr = mk_expr (ePREFIXSUB, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 55:
#line 178 "gram.y"
{ yyval.expr = mk_expr (eARRAYASSIGN, yyvsp[-5].expr, yyvsp[-3].expr,
							yyvsp[0].expr); ;
    break;}
case 56:
#line 180 "gram.y"
{ yyval.expr = yyvsp[-1].expr; ;
    break;}
case 57:
#line 181 "gram.y"
{ yyval.expr = mk_expr (eARRAYREF, yyvsp[-3].expr, yyvsp[-1].expr,
							NULL); ;
    break;}
case 58:
#line 183 "gram.y"
{ yyval.expr = mk_expr (eQUESTCOLON, yyvsp[-4].expr, yyvsp[-2].expr,
							yyvsp[0].expr); ;
    break;}
case 59:
#line 185 "gram.y"
{ yyval.expr = mk_expr (eMULT, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 60:
#line 186 "gram.y"
{ yyval.expr = mk_expr (eDIV, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 61:
#line 187 "gram.y"
{ yyval.expr = mk_expr (ePLUS, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 62:
#line 188 "gram.y"
{ yyval.expr = mk_expr (eMINUS, yyvsp[-2].expr, yyvsp[0].expr,
							NULL); ;
    break;}
case 63:
#line 190 "gram.y"
{ yyval.expr = mk_expr (eLT, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 64:
#line 191 "gram.y"
{ yyval.expr = mk_expr (eGT, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 65:
#line 192 "gram.y"
{ yyval.expr = mk_expr (eEQ, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 66:
#line 193 "gram.y"
{ yyval.expr = mk_expr (eNE, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 67:
#line 194 "gram.y"
{ yyval.expr = mk_expr (eGE, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 68:
#line 195 "gram.y"
{ yyval.expr = mk_expr (eLE, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 69:
#line 198 "gram.y"
{ yyval.expr = NULL; ;
    break;}
case 70:
#line 199 "gram.y"
{ yyval.expr = yyvsp[0].expr; ;
    break;}
case 71:
#line 202 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 72:
#line 203 "gram.y"
{ yyval.lst = yyvsp[0].lst; ;
    break;}
case 73:
#line 206 "gram.y"
{ yyval.lst = list (); list_append (yyval.lst, yyvsp[0].expr); ;
    break;}
case 74:
#line 207 "gram.y"
{ list_append (yyvsp[-2].lst, yyvsp[0].expr); ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "/usr/local/share/bison.simple"

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
#line 210 "gram.y"


void
yyerror (msg)
     char *msg;
{
  fprintf (stderr, "%s:%d: %s\n", defs_file, linenum, msg);
}
