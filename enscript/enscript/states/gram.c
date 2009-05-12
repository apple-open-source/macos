
/*  A Bison parser, made from gram.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	tSYMBOL	257
#define	tREGEXP	258
#define	tSTRING	259
#define	tINTEGER	260
#define	tREAL	261
#define	tSUB	262
#define	tSTATE	263
#define	tSTART	264
#define	tSTARTRULES	265
#define	tNAMERULES	266
#define	tBEGIN	267
#define	tEND	268
#define	tRETURN	269
#define	tIF	270
#define	tELSE	271
#define	tLOCAL	272
#define	tWHILE	273
#define	tFOR	274
#define	tEXTENDS	275
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
 * $Id: gram.c,v 1.1.1.1 2003/03/05 07:25:52 mtr Exp $
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



#define	YYFINAL		167
#define	YYFLAG		-32768
#define	YYNTBASE	52

#define YYTRANSLATE(x) ((unsigned)(x) <= 288 ? yytranslate[x] : 68)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    41,     2,     2,     2,     2,     2,     2,    50,
    51,    39,    37,    49,    38,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    28,    48,    33,
    22,    34,    27,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    44,     2,    45,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    46,     2,    47,     2,     2,     2,     2,     2,
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
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
    17,    18,    19,    20,    21,    23,    24,    25,    26,    29,
    30,    31,    32,    35,    36,    40,    42,    43
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     4,     9,    14,    19,    25,    33,    35,    36,
    41,    42,    45,    50,    55,    60,    65,    66,    68,    70,
    74,    75,    79,    81,    85,    87,    91,    92,    95,    98,
   102,   112,   116,   122,   130,   136,   146,   149,   151,   153,
   155,   157,   159,   162,   166,   170,   175,   179,   183,   187,
   191,   195,   198,   201,   204,   207,   214,   218,   223,   229,
   233,   237,   241,   245,   249,   253,   257,   261,   265,   269,
   270,   272,   273,   275,   277
};

static const short yyrhs[] = {    -1,
    52,    53,     0,    10,    46,    62,    47,     0,    11,    46,
    54,    47,     0,    12,    46,    54,    47,     0,     9,     3,
    46,    55,    47,     0,     9,     3,    21,     3,    46,    55,
    47,     0,    63,     0,     0,    54,     4,     3,    48,     0,
     0,    55,    56,     0,    13,    46,    62,    47,     0,    14,
    46,    62,    47,     0,     4,    46,    62,    47,     0,     3,
    46,    62,    47,     0,     0,    58,     0,     3,     0,    58,
    49,     3,     0,     0,    18,    60,    48,     0,    61,     0,
    60,    49,    61,     0,     3,     0,     3,    22,    64,     0,
     0,    62,    63,     0,    15,    48,     0,    15,    64,    48,
     0,     8,     3,    50,    57,    51,    46,    59,    62,    47,
     0,    46,    62,    47,     0,    16,    50,    64,    51,    63,
     0,    16,    50,    64,    51,    63,    17,    63,     0,    19,
    50,    64,    51,    63,     0,    20,    50,    65,    48,    64,
    48,    65,    51,    63,     0,    64,    48,     0,     5,     0,
     4,     0,     6,     0,     7,     0,     3,     0,    41,    64,
     0,    64,    30,    64,     0,    64,    29,    64,     0,     3,
    50,    66,    51,     0,     3,    22,    64,     0,     3,    23,
    64,     0,     3,    24,    64,     0,     3,    25,    64,     0,
     3,    26,    64,     0,     3,    42,     0,     3,    43,     0,
    42,     3,     0,    43,     3,     0,    64,    44,    64,    45,
    22,    64,     0,    50,    64,    51,     0,    64,    44,    64,
    45,     0,    64,    27,    64,    28,    64,     0,    64,    39,
    64,     0,    64,    40,    64,     0,    64,    37,    64,     0,
    64,    38,    64,     0,    64,    33,    64,     0,    64,    34,
    64,     0,    64,    31,    64,     0,    64,    32,    64,     0,
    64,    35,    64,     0,    64,    36,    64,     0,     0,    64,
     0,     0,    67,     0,    64,     0,    67,    49,    64,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
    67,    68,    71,    72,    74,    76,    78,    80,    83,    84,
    88,    89,    91,    92,    93,    94,    97,    98,   101,   102,
   105,   106,   109,   110,   113,   114,   117,   118,   121,   123,
   125,   130,   132,   134,   137,   139,   142,   146,   148,   150,
   152,   154,   156,   158,   159,   160,   162,   164,   166,   168,
   170,   172,   174,   176,   178,   180,   182,   183,   185,   187,
   188,   189,   190,   192,   193,   194,   195,   196,   197,   200,
   201,   204,   205,   208,   209
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","tSYMBOL",
"tREGEXP","tSTRING","tINTEGER","tREAL","tSUB","tSTATE","tSTART","tSTARTRULES",
"tNAMERULES","tBEGIN","tEND","tRETURN","tIF","tELSE","tLOCAL","tWHILE","tFOR",
"tEXTENDS","'='","tADDASSIGN","tSUBASSIGN","tMULASSIGN","tDIVASSIGN","'?'","':'",
"tOR","tAND","tEQ","tNE","'<'","'>'","tGE","tLE","'+'","'-'","'*'","tDIV","'!'",
"tPLUSPLUS","tMINUSMINUS","'['","']'","'{'","'}'","';'","','","'('","')'","file",
"toplevel","regexp_sym_list","staterules","staterule","symbol_list","rest_symbol_list",
"locals","locals_rest","local_def","stmt_list","stmt","expr","cond_expr","expr_list",
"rest_expr_list", NULL
};
#endif

static const short yyr1[] = {     0,
    52,    52,    53,    53,    53,    53,    53,    53,    54,    54,
    55,    55,    56,    56,    56,    56,    57,    57,    58,    58,
    59,    59,    60,    60,    61,    61,    62,    62,    63,    63,
    63,    63,    63,    63,    63,    63,    63,    64,    64,    64,
    64,    64,    64,    64,    64,    64,    64,    64,    64,    64,
    64,    64,    64,    64,    64,    64,    64,    64,    64,    64,
    64,    64,    64,    64,    64,    64,    64,    64,    64,    65,
    65,    66,    66,    67,    67
};

static const short yyr2[] = {     0,
     0,     2,     4,     4,     4,     5,     7,     1,     0,     4,
     0,     2,     4,     4,     4,     4,     0,     1,     1,     3,
     0,     3,     1,     3,     1,     3,     0,     2,     2,     3,
     9,     3,     5,     7,     5,     9,     2,     1,     1,     1,
     1,     1,     2,     3,     3,     4,     3,     3,     3,     3,
     3,     2,     2,     2,     2,     6,     3,     4,     5,     3,
     3,     3,     3,     3,     3,     3,     3,     3,     3,     0,
     1,     0,     1,     1,     3
};

static const short yydefact[] = {     1,
     0,    42,    39,    38,    40,    41,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,    27,     0,
     2,     8,     0,     0,     0,     0,     0,     0,    52,    53,
    72,     0,     0,    27,     9,     9,    29,     0,     0,     0,
    70,    43,    54,    55,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    37,    47,    48,    49,    50,    51,    74,     0,    73,    17,
     0,    11,     0,     0,     0,    30,     0,     0,    71,     0,
    32,    28,    57,     0,    45,    44,    66,    67,    64,    65,
    68,    69,    62,    63,    60,    61,     0,    46,     0,    19,
     0,    18,     0,     0,     3,     0,     4,     5,     0,     0,
     0,     0,    58,    75,     0,     0,    11,     0,     0,     0,
     0,     6,    12,     0,    33,    35,     0,    59,     0,    21,
    20,     0,    27,    27,    27,    27,    10,     0,    70,    56,
     0,    27,     7,     0,     0,     0,     0,    34,     0,    25,
     0,    23,     0,    16,    15,    13,    14,     0,     0,    22,
     0,    31,    36,    26,    24,     0,     0
};

static const short yydefgoto[] = {     1,
    21,    74,   104,   123,   101,   102,   142,   151,   152,    45,
    82,    23,    80,    68,    69
};

static const short yypact[] = {-32768,
    71,   270,-32768,-32768,-32768,-32768,     1,     9,   -40,   -28,
   -27,    18,   -24,   -18,   -17,    90,    28,    31,-32768,    90,
-32768,-32768,   374,    90,    90,    90,    90,    90,-32768,-32768,
    90,   -15,   -19,-32768,-32768,-32768,-32768,   394,    90,    90,
    90,    12,-32768,-32768,   119,   305,    90,    90,    90,    90,
    90,    90,    90,    90,    90,    90,    90,    90,    90,    90,
-32768,   469,   469,   469,   469,   469,   469,     7,    20,    64,
    67,-32768,   139,     3,     4,-32768,   328,   351,   469,    36,
-32768,-32768,-32768,   453,   484,   498,   510,   510,   112,   112,
   112,   112,    13,    13,    12,    12,   434,-32768,    90,-32768,
    34,    40,    46,    51,-32768,    96,-32768,-32768,   283,   283,
    90,    90,    85,   469,    69,   107,-32768,    70,    73,    74,
    83,-32768,-32768,    82,   124,-32768,   414,   469,    90,   118,
-32768,    59,-32768,-32768,-32768,-32768,-32768,   283,    90,   469,
   150,-32768,-32768,   168,   187,   216,   235,-32768,   113,   141,
   -38,-32768,   264,-32768,-32768,-32768,-32768,   283,    90,-32768,
   150,-32768,-32768,   469,-32768,   167,-32768
};

static const short yypgoto[] = {-32768,
-32768,   132,    53,-32768,-32768,-32768,-32768,-32768,    16,   -31,
    -1,   -11,    39,-32768,-32768
};


#define	YYLAST		554


static const short yytable[] = {    22,
    38,    71,    73,    32,    42,    34,   106,   106,    46,   160,
   161,    33,    62,    63,    64,    65,    66,    35,    36,    67,
     2,     3,     4,     5,     6,    39,    72,    77,    78,    79,
    43,    40,    41,    44,    70,    84,    85,    86,    87,    88,
    89,    90,    91,    92,    93,    94,    95,    96,    97,   107,
   108,    58,    59,   118,   119,    60,    60,    98,    16,    17,
    18,   118,   119,   120,   121,    37,   100,    20,    99,   103,
   166,   120,   121,     2,     3,     4,     5,     6,     7,     8,
     9,    10,    11,   111,   115,    12,    13,   114,   116,    14,
    15,   117,     2,     3,     4,     5,     6,   122,   124,   127,
   128,   144,   145,   146,   147,   143,   129,   125,   126,   131,
   153,    16,    17,    18,   130,   133,    19,   140,   134,   135,
    20,     2,     3,     4,     5,     6,     7,    79,   136,   137,
    16,    17,    18,    12,    13,   141,   148,    14,    15,    20,
   138,     2,     3,     4,     5,     6,     7,   164,    56,    57,
    58,    59,   150,    12,    13,    60,   163,    14,    15,    16,
    17,    18,   159,   158,    19,    81,   167,    75,    20,   132,
     2,     3,     4,     5,     6,     7,   165,   149,     0,    16,
    17,    18,    12,    13,    19,   105,    14,    15,    20,     2,
     3,     4,     5,     6,     7,     0,     0,     0,     0,     0,
     0,    12,    13,     0,     0,    14,    15,     0,    16,    17,
    18,     0,     0,    19,   154,     0,     0,    20,     2,     3,
     4,     5,     6,     7,     0,     0,     0,    16,    17,    18,
    12,    13,    19,   155,    14,    15,    20,     2,     3,     4,
     5,     6,     7,     0,     0,     0,     0,     0,     0,    12,
    13,     0,     0,    14,    15,     0,    16,    17,    18,     0,
     0,    19,   156,     0,     0,    20,     2,     3,     4,     5,
     6,     7,     0,     0,     0,    16,    17,    18,    12,    13,
    19,   157,    14,    15,    20,     2,     3,     4,     5,     6,
     7,    24,    25,    26,    27,    28,     0,    12,    13,     0,
     0,    14,    15,     0,    16,    17,    18,     0,     0,    19,
   162,    29,    30,    20,     0,     0,     0,     0,     0,    31,
     0,     0,     0,    16,    17,    18,     0,     0,    19,     0,
     0,    47,    20,    48,    49,    50,    51,    52,    53,    54,
    55,    56,    57,    58,    59,     0,     0,     0,    60,     0,
     0,     0,     0,     0,    47,    83,    48,    49,    50,    51,
    52,    53,    54,    55,    56,    57,    58,    59,     0,     0,
     0,    60,     0,     0,     0,     0,     0,    47,   109,    48,
    49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
    59,     0,     0,     0,    60,     0,     0,     0,     0,     0,
    47,   110,    48,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,     0,     0,     0,    60,     0,     0,
    47,    61,    48,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,     0,     0,     0,    60,     0,     0,
    47,    76,    48,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,     0,     0,     0,    60,     0,     0,
    47,   139,    48,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,     0,     0,     0,    60,   113,    47,
   112,    48,    49,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,     0,     0,    47,    60,    48,    49,    50,
    51,    52,    53,    54,    55,    56,    57,    58,    59,     0,
     0,     0,    60,    49,    50,    51,    52,    53,    54,    55,
    56,    57,    58,    59,     0,     0,     0,    60,    50,    51,
    52,    53,    54,    55,    56,    57,    58,    59,     0,     0,
     0,    60,    52,    53,    54,    55,    56,    57,    58,    59,
     0,     0,     0,    60
};

static const short yycheck[] = {     1,
    12,    21,    34,     3,    16,    46,     4,     4,    20,    48,
    49,     3,    24,    25,    26,    27,    28,    46,    46,    31,
     3,     4,     5,     6,     7,    50,    46,    39,    40,    41,
     3,    50,    50,     3,    50,    47,    48,    49,    50,    51,
    52,    53,    54,    55,    56,    57,    58,    59,    60,    47,
    47,    39,    40,     3,     4,    44,    44,    51,    41,    42,
    43,     3,     4,    13,    14,    48,     3,    50,    49,     3,
     0,    13,    14,     3,     4,     5,     6,     7,     8,     9,
    10,    11,    12,    48,    51,    15,    16,    99,    49,    19,
    20,    46,     3,     4,     5,     6,     7,    47,     3,   111,
   112,   133,   134,   135,   136,    47,    22,   109,   110,     3,
   142,    41,    42,    43,    46,    46,    46,   129,    46,    46,
    50,     3,     4,     5,     6,     7,     8,   139,    46,    48,
    41,    42,    43,    15,    16,    18,   138,    19,    20,    50,
    17,     3,     4,     5,     6,     7,     8,   159,    37,    38,
    39,    40,     3,    15,    16,    44,   158,    19,    20,    41,
    42,    43,    22,    51,    46,    47,     0,    36,    50,   117,
     3,     4,     5,     6,     7,     8,   161,   139,    -1,    41,
    42,    43,    15,    16,    46,    47,    19,    20,    50,     3,
     4,     5,     6,     7,     8,    -1,    -1,    -1,    -1,    -1,
    -1,    15,    16,    -1,    -1,    19,    20,    -1,    41,    42,
    43,    -1,    -1,    46,    47,    -1,    -1,    50,     3,     4,
     5,     6,     7,     8,    -1,    -1,    -1,    41,    42,    43,
    15,    16,    46,    47,    19,    20,    50,     3,     4,     5,
     6,     7,     8,    -1,    -1,    -1,    -1,    -1,    -1,    15,
    16,    -1,    -1,    19,    20,    -1,    41,    42,    43,    -1,
    -1,    46,    47,    -1,    -1,    50,     3,     4,     5,     6,
     7,     8,    -1,    -1,    -1,    41,    42,    43,    15,    16,
    46,    47,    19,    20,    50,     3,     4,     5,     6,     7,
     8,    22,    23,    24,    25,    26,    -1,    15,    16,    -1,
    -1,    19,    20,    -1,    41,    42,    43,    -1,    -1,    46,
    47,    42,    43,    50,    -1,    -1,    -1,    -1,    -1,    50,
    -1,    -1,    -1,    41,    42,    43,    -1,    -1,    46,    -1,
    -1,    27,    50,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    -1,    -1,    -1,    44,    -1,
    -1,    -1,    -1,    -1,    27,    51,    29,    30,    31,    32,
    33,    34,    35,    36,    37,    38,    39,    40,    -1,    -1,
    -1,    44,    -1,    -1,    -1,    -1,    -1,    27,    51,    29,
    30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
    40,    -1,    -1,    -1,    44,    -1,    -1,    -1,    -1,    -1,
    27,    51,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    -1,    -1,    -1,    44,    -1,    -1,
    27,    48,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    -1,    -1,    -1,    44,    -1,    -1,
    27,    48,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    -1,    -1,    -1,    44,    -1,    -1,
    27,    48,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    -1,    -1,    -1,    44,    45,    27,
    28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
    38,    39,    40,    -1,    -1,    27,    44,    29,    30,    31,
    32,    33,    34,    35,    36,    37,    38,    39,    40,    -1,
    -1,    -1,    44,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    -1,    -1,    -1,    44,    31,    32,
    33,    34,    35,    36,    37,    38,    39,    40,    -1,    -1,
    -1,    44,    33,    34,    35,    36,    37,    38,    39,    40,
    -1,    -1,    -1,    44
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/l/mtr/gnu/share/bison.simple"
/* This file comes from bison-1.28.  */

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

#ifndef YYSTACK_USE_ALLOCA
#ifdef alloca
#define YYSTACK_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define YYSTACK_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
/* We think this test detects Watcom and Microsoft C.  */
/* This used to test MSDOS, but that is a bad idea
   since that symbol is in the user namespace.  */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#if 0 /* No need for malloc.h, which pollutes the namespace;
	 instead, just don't use alloca.  */
#include <malloc.h>
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
/* #include <malloc.h>  */
 #pragma alloca
#define YYSTACK_USE_ALLOCA
#else /* not MSDOS, or __TURBOC__, or _AIX */
#if 0
#ifdef __hpux /* haible@ilog.fr says this works for HPUX 9.05 and up,
		 and on HPUX 10.  Eventually we can turn this on.  */
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#endif /* __hpux */
#endif
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* YYSTACK_USE_ALLOCA not defined */

#ifdef YYSTACK_USE_ALLOCA
#define YYSTACK_ALLOC alloca
#else
#define YYSTACK_ALLOC malloc
#endif

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
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

/* Define __yy_memcpy.  Note that the size argument
   should be passed with type unsigned int, because that is what the non-GCC
   definitions require.  With GCC, __builtin_memcpy takes an arg
   of type size_t, but it can handle unsigned int.  */

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
     unsigned int count;
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
__yy_memcpy (char *to, char *from, unsigned int count)
{
  register char *t = to;
  register char *f = from;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 217 "/l/mtr/gnu/share/bison.simple"

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

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
#ifdef YYPARSE_PARAM
int yyparse (void *);
#else
int yyparse (void);
#endif
#endif

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
  int yyfree_stacks = 0;

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
	  if (yyfree_stacks)
	    {
	      free (yyss);
	      free (yyvs);
#ifdef YYLSP_NEEDED
	      free (yyls);
#endif
	    }
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
#ifndef YYSTACK_USE_ALLOCA
      yyfree_stacks = 1;
#endif
      yyss = (short *) YYSTACK_ALLOC (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1,
		   size * (unsigned int) sizeof (*yyssp));
      yyvs = (YYSTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1,
		   size * (unsigned int) sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1,
		   size * (unsigned int) sizeof (*yylsp));
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
{ define_state (yyvsp[-3].node, NULL, yyvsp[-1].lst); ;
    break;}
case 7:
#line 79 "gram.y"
{ define_state (yyvsp[-5].node, yyvsp[-3].node, yyvsp[-1].lst); ;
    break;}
case 8:
#line 80 "gram.y"
{ list_append (global_stmts, yyvsp[0].stmt); ;
    break;}
case 9:
#line 83 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 10:
#line 85 "gram.y"
{ list_append (yyvsp[-3].lst, cons (yyvsp[-2].node, yyvsp[-1].node)); ;
    break;}
case 11:
#line 88 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 12:
#line 89 "gram.y"
{ list_append (yyvsp[-1].lst, yyvsp[0].cons); ;
    break;}
case 13:
#line 91 "gram.y"
{ yyval.cons = cons (RULE_BEGIN, yyvsp[-1].lst); ;
    break;}
case 14:
#line 92 "gram.y"
{ yyval.cons = cons (RULE_END, yyvsp[-1].lst); ;
    break;}
case 15:
#line 93 "gram.y"
{ yyval.cons = cons (yyvsp[-3].node, yyvsp[-1].lst); ;
    break;}
case 16:
#line 94 "gram.y"
{ yyval.cons = cons (yyvsp[-3].node, yyvsp[-1].lst); ;
    break;}
case 17:
#line 97 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 18:
#line 98 "gram.y"
{ yyval.lst = yyvsp[0].lst; ;
    break;}
case 19:
#line 101 "gram.y"
{ yyval.lst = list (); list_append (yyval.lst, yyvsp[0].node); ;
    break;}
case 20:
#line 102 "gram.y"
{ list_append (yyvsp[-2].lst, yyvsp[0].node); ;
    break;}
case 21:
#line 105 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 22:
#line 106 "gram.y"
{ yyval.lst = yyvsp[-1].lst; ;
    break;}
case 23:
#line 109 "gram.y"
{ yyval.lst = list (); list_append (yyval.lst, yyvsp[0].cons); ;
    break;}
case 24:
#line 110 "gram.y"
{ list_append (yyvsp[-2].lst, yyvsp[0].cons); ;
    break;}
case 25:
#line 113 "gram.y"
{ yyval.cons = cons (yyvsp[0].node, NULL); ;
    break;}
case 26:
#line 114 "gram.y"
{ yyval.cons = cons (yyvsp[-2].node, yyvsp[0].expr); ;
    break;}
case 27:
#line 117 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 28:
#line 118 "gram.y"
{ list_append (yyvsp[-1].lst, yyvsp[0].stmt); ;
    break;}
case 29:
#line 121 "gram.y"
{ yyval.stmt = mk_stmt (sRETURN, NULL, NULL,
							NULL, NULL); ;
    break;}
case 30:
#line 123 "gram.y"
{ yyval.stmt = mk_stmt (sRETURN, yyvsp[-1].expr, NULL,
							NULL, NULL); ;
    break;}
case 31:
#line 126 "gram.y"
{ yyval.stmt = mk_stmt (sDEFSUB, yyvsp[-7].node,
							cons (cons (yyvsp[-5].lst, yyvsp[-2].lst),
							      yyvsp[-1].lst),
							NULL, NULL); ;
    break;}
case 32:
#line 130 "gram.y"
{ yyval.stmt = mk_stmt (sBLOCK, yyvsp[-1].lst, NULL,
							NULL, NULL); ;
    break;}
case 33:
#line 132 "gram.y"
{ yyval.stmt = mk_stmt (sIF, yyvsp[-2].expr, yyvsp[0].stmt, NULL,
							NULL); ;
    break;}
case 34:
#line 135 "gram.y"
{ yyval.stmt = mk_stmt (sIF, yyvsp[-4].expr, yyvsp[-2].stmt, yyvsp[0].stmt,
							NULL); ;
    break;}
case 35:
#line 137 "gram.y"
{ yyval.stmt = mk_stmt (sWHILE, yyvsp[-2].expr, yyvsp[0].stmt,
							NULL, NULL); ;
    break;}
case 36:
#line 140 "gram.y"
{ yyval.stmt = mk_stmt (sFOR, yyvsp[-6].expr, yyvsp[-4].expr, yyvsp[-2].expr,
							yyvsp[0].stmt); ;
    break;}
case 37:
#line 142 "gram.y"
{ yyval.stmt = mk_stmt (sEXPR, yyvsp[-1].expr, NULL,
							NULL, NULL); ;
    break;}
case 38:
#line 146 "gram.y"
{ yyval.expr = mk_expr (eSTRING, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 39:
#line 148 "gram.y"
{ yyval.expr = mk_expr (eREGEXP, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 40:
#line 150 "gram.y"
{ yyval.expr = mk_expr (eINTEGER, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 41:
#line 152 "gram.y"
{ yyval.expr = mk_expr (eREAL, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 42:
#line 154 "gram.y"
{ yyval.expr = mk_expr (eSYMBOL, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 43:
#line 156 "gram.y"
{ yyval.expr = mk_expr (eNOT, yyvsp[0].expr, NULL,
							NULL); ;
    break;}
case 44:
#line 158 "gram.y"
{ yyval.expr = mk_expr (eAND, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 45:
#line 159 "gram.y"
{ yyval.expr = mk_expr (eOR, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 46:
#line 160 "gram.y"
{ yyval.expr = mk_expr (eFCALL, yyvsp[-3].node, yyvsp[-1].lst,
							NULL); ;
    break;}
case 47:
#line 162 "gram.y"
{ yyval.expr = mk_expr (eASSIGN, yyvsp[-2].node, yyvsp[0].expr,
							NULL); ;
    break;}
case 48:
#line 164 "gram.y"
{ yyval.expr = mk_expr (eADDASSIGN, yyvsp[-2].node, yyvsp[0].expr,
							NULL); ;
    break;}
case 49:
#line 166 "gram.y"
{ yyval.expr = mk_expr (eSUBASSIGN, yyvsp[-2].node, yyvsp[0].expr,
							NULL); ;
    break;}
case 50:
#line 168 "gram.y"
{ yyval.expr = mk_expr (eMULASSIGN, yyvsp[-2].node, yyvsp[0].expr,
							NULL); ;
    break;}
case 51:
#line 170 "gram.y"
{ yyval.expr = mk_expr (eDIVASSIGN, yyvsp[-2].node, yyvsp[0].expr,
							NULL); ;
    break;}
case 52:
#line 172 "gram.y"
{ yyval.expr = mk_expr (ePOSTFIXADD, yyvsp[-1].node, NULL,
							NULL); ;
    break;}
case 53:
#line 174 "gram.y"
{ yyval.expr = mk_expr (ePOSTFIXSUB, yyvsp[-1].node, NULL,
							NULL); ;
    break;}
case 54:
#line 176 "gram.y"
{ yyval.expr = mk_expr (ePREFIXADD, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 55:
#line 178 "gram.y"
{ yyval.expr = mk_expr (ePREFIXSUB, yyvsp[0].node, NULL,
							NULL); ;
    break;}
case 56:
#line 180 "gram.y"
{ yyval.expr = mk_expr (eARRAYASSIGN, yyvsp[-5].expr, yyvsp[-3].expr,
							yyvsp[0].expr); ;
    break;}
case 57:
#line 182 "gram.y"
{ yyval.expr = yyvsp[-1].expr; ;
    break;}
case 58:
#line 183 "gram.y"
{ yyval.expr = mk_expr (eARRAYREF, yyvsp[-3].expr, yyvsp[-1].expr,
							NULL); ;
    break;}
case 59:
#line 185 "gram.y"
{ yyval.expr = mk_expr (eQUESTCOLON, yyvsp[-4].expr, yyvsp[-2].expr,
							yyvsp[0].expr); ;
    break;}
case 60:
#line 187 "gram.y"
{ yyval.expr = mk_expr (eMULT, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 61:
#line 188 "gram.y"
{ yyval.expr = mk_expr (eDIV, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 62:
#line 189 "gram.y"
{ yyval.expr = mk_expr (ePLUS, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 63:
#line 190 "gram.y"
{ yyval.expr = mk_expr (eMINUS, yyvsp[-2].expr, yyvsp[0].expr,
							NULL); ;
    break;}
case 64:
#line 192 "gram.y"
{ yyval.expr = mk_expr (eLT, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 65:
#line 193 "gram.y"
{ yyval.expr = mk_expr (eGT, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 66:
#line 194 "gram.y"
{ yyval.expr = mk_expr (eEQ, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 67:
#line 195 "gram.y"
{ yyval.expr = mk_expr (eNE, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 68:
#line 196 "gram.y"
{ yyval.expr = mk_expr (eGE, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 69:
#line 197 "gram.y"
{ yyval.expr = mk_expr (eLE, yyvsp[-2].expr, yyvsp[0].expr, NULL); ;
    break;}
case 70:
#line 200 "gram.y"
{ yyval.expr = NULL; ;
    break;}
case 71:
#line 201 "gram.y"
{ yyval.expr = yyvsp[0].expr; ;
    break;}
case 72:
#line 204 "gram.y"
{ yyval.lst = list (); ;
    break;}
case 73:
#line 205 "gram.y"
{ yyval.lst = yyvsp[0].lst; ;
    break;}
case 74:
#line 208 "gram.y"
{ yyval.lst = list (); list_append (yyval.lst, yyvsp[0].expr); ;
    break;}
case 75:
#line 209 "gram.y"
{ list_append (yyvsp[-2].lst, yyvsp[0].expr); ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 543 "/l/mtr/gnu/share/bison.simple"

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

 yyacceptlab:
  /* YYACCEPT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 0;

 yyabortlab:
  /* YYABORT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 1;
}
#line 212 "gram.y"


void
yyerror (msg)
     char *msg;
{
  fprintf (stderr, "%s:%d: %s\n", yyin_name, linenum, msg);
}
