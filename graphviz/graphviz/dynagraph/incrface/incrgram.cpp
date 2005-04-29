/* A Bison parser, made from incrgram.ypp
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	T_graph	257
# define	T_node	258
# define	T_edge	259
# define	T_view	260
# define	T_open	261
# define	T_close	262
# define	T_insert	263
# define	T_delete	264
# define	T_modify	265
# define	T_lock	266
# define	T_unlock	267
# define	T_segue	268
# define	T_message	269
# define	T_id	270
# define	T_edgeop	271
# define	T_subgraph	272

#line 1 "incrgram.ypp"

#pragma prototyped
#include "incrface/incrcmds.h"

#line 12 "incrgram.ypp"
#ifndef YYSTYPE
typedef union	{
			int				i;
			char			*str;
} incr_yystype;
# define YYSTYPE incr_yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		81
#define	YYFLAG		-32768
#define	YYNTBASE	25

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 272 ? incr_yytranslate[x] : 50)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char incr_yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      19,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    24,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    23,
       2,    22,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    20,     2,    21,     2,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18
};

#if YYDEBUG
static const short incr_yyprhs[] =
{
       0,     0,     2,     6,     7,     9,    11,    13,    15,    17,
      19,    21,    23,    25,    27,    29,    31,    33,    35,    37,
      38,    41,    45,    48,    52,    57,    61,    65,    69,    75,
      81,    86,    94,   100,   105,   106,   111,   112,   114,   115,
     117,   121,   125,   127,   129,   130
};
static const short incr_yyrhs[] =
{
      26,     0,    26,    27,    19,     0,     0,    37,     0,    40,
       0,    38,     0,    41,     0,    39,     0,    42,     0,    30,
       0,    32,     0,    33,     0,    34,     0,    35,     0,    36,
       0,    29,     0,    28,     0,     1,     0,     0,    15,    16,
       0,     7,     6,    31,     0,    49,    43,     0,     8,     6,
      49,     0,    11,     6,    49,    43,     0,    12,     6,    49,
       0,    13,     6,    49,     0,    14,     6,    49,     0,     9,
      49,     4,    16,    43,     0,    11,    49,     4,    16,    43,
       0,    10,    49,     4,    16,     0,     9,    49,     5,    16,
      16,    16,    43,     0,    11,    49,     5,    16,    43,     0,
      10,    49,     5,    16,     0,     0,    20,    44,    45,    21,
       0,     0,    46,     0,     0,    47,     0,    46,    48,    47,
       0,    16,    22,    16,     0,    23,     0,    24,     0,     0,
      16,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short incr_yyrline[] =
{
       0,    19,    23,    24,    27,    28,    29,    29,    30,    30,
      31,    31,    31,    32,    32,    33,    34,    35,    36,    39,
      42,    45,    48,    51,    54,    57,    60,    63,    66,    69,
      72,    75,    78,    81,    84,    84,    85,    88,    89,    92,
      93,    96,    99,    99,    99,   101
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const incr_yytname[] =
{
  "$", "error", "$undefined.", "T_graph", "T_node", "T_edge", "T_view", 
  "T_open", "T_close", "T_insert", "T_delete", "T_modify", "T_lock", 
  "T_unlock", "T_segue", "T_message", "T_id", "T_edgeop", "T_subgraph", 
  "'\\n'", "'['", "']'", "'='", "';'", "','", "session", "commands", 
  "command", "nop", "message", "open_view", "open_view2", "close_view", 
  "mod_view", "lock_view", "unlock_view", "segue", "ins_node", "mod_node", 
  "del_node", "ins_edge", "mod_edge", "del_edge", "attrlist", "@1", 
  "optattrdefs", "attrdefs", "attritem", "optsep", "viewid", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short incr_yyr1[] =
{
       0,    25,    26,    26,    27,    27,    27,    27,    27,    27,
      27,    27,    27,    27,    27,    27,    27,    27,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    44,    43,    43,    45,    45,    46,
      46,    47,    48,    48,    48,    49
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short incr_yyr2[] =
{
       0,     1,     3,     0,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       2,     3,     2,     3,     4,     3,     3,     3,     5,     5,
       4,     7,     5,     4,     0,     4,     0,     1,     0,     1,
       3,     3,     1,     1,     0,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short incr_yydefact[] =
{
       3,     0,    18,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    17,    16,    10,    11,    12,    13,    14,
      15,     4,     6,     8,     5,     7,     9,     0,     0,    45,
       0,     0,     0,     0,     0,     0,     0,    20,     2,    21,
      36,    23,     0,     0,     0,     0,    36,     0,     0,    25,
      26,    27,    34,    22,    36,     0,    30,    33,    24,    36,
      36,    38,    28,     0,    29,    32,     0,     0,    37,    39,
      36,     0,    35,    42,    43,     0,    31,    41,    40,     0,
       0,     0
};

static const short incr_yydefgoto[] =
{
      79,     1,    12,    13,    14,    15,    39,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    53,    61,
      67,    68,    69,    75,    30
};

static const short incr_yypact[] =
{
  -32768,    24,-32768,     9,    12,     4,     4,    -4,    13,    17,
      34,    25,    23,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,     4,     4,-32768,
       0,     3,     4,     5,     4,     4,     4,-32768,-32768,-32768,
      26,-32768,    28,    29,    31,    32,    26,    33,    35,-32768,
  -32768,-32768,-32768,-32768,    26,    36,-32768,-32768,-32768,    26,
      26,    37,-32768,    38,-32768,-32768,    39,    41,   -10,-32768,
      26,    40,-32768,-32768,-32768,    37,-32768,-32768,-32768,    50,
      55,-32768
};

static const short incr_yypgoto[] =
{
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,   -43,-32768,
  -32768,-32768,   -18,-32768,    -6
};


#define	YYLAST		62


static const short incr_yytable[] =
{
      31,    33,    32,    58,    42,    43,   -44,    44,    45,    47,
      48,    62,    29,    73,    74,    27,    64,    65,    28,    34,
      29,    40,    41,    35,    -1,     2,    46,    76,    49,    50,
      51,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      36,    37,    38,   -19,    54,    55,    52,    56,    57,    59,
      80,    60,    63,    66,    70,    81,    77,    78,     0,     0,
       0,    71,    72
};

static const short incr_yycheck[] =
{
       6,     7,     6,    46,     4,     5,    16,     4,     5,     4,
       5,    54,    16,    23,    24,     6,    59,    60,     6,     6,
      16,    27,    28,     6,     0,     1,    32,    70,    34,    35,
      36,     7,     8,     9,    10,    11,    12,    13,    14,    15,
       6,    16,    19,    19,    16,    16,    20,    16,    16,    16,
       0,    16,    16,    16,    16,     0,    16,    75,    -1,    -1,
      -1,    22,    21
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison/bison.simple"

/* Skeleton output parser for bison,

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software
   Foundation, Inc.

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

/* This is the parser code that is written into each bison parser when
   the %semantic_parser declaration is not specified in the grammar.
   It was written by Richard Stallman by simplifying the hairy parser
   used when %semantic_parser is specified.  */

/* All symbols defined below should begin with incr_yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (incr_yyoverflow) || defined (YYERROR_VERBOSE)

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (incr_yyoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (incr_yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union incr_yyalloc
{
  short incr_yyss;
  YYSTYPE incr_yyvs;
# if YYLSP_NEEDED
  YYLTYPE incr_yyls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union incr_yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# if YYLSP_NEEDED
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAX)
# else
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)
# endif

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T incr_yyi;		\
	  for (incr_yyi = 0; incr_yyi < (Count); incr_yyi++)	\
	    (To)[incr_yyi] = (From)[incr_yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T incr_yynewbytes;						\
	YYCOPY (&incr_yyptr->Stack, Stack, incr_yysize);				\
	Stack = &incr_yyptr->Stack;						\
	incr_yynewbytes = incr_yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	incr_yyptr += incr_yynewbytes / sizeof (*incr_yyptr);				\
      }									\
    while (0)

#endif


#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define incr_yyerrok		(incr_yyerrstatus = 0)
#define incr_yyclearin	(incr_yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto incr_yyacceptlab
#define YYABORT 	goto incr_yyabortlab
#define YYERROR		goto incr_yyerrlab1
/* Like YYERROR except do call incr_yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto incr_yyerrlab
#define YYRECOVERING()  (!!incr_yyerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (incr_yychar == YYEMPTY && incr_yylen == 1)				\
    {								\
      incr_yychar = (Token);						\
      incr_yylval = (Value);						\
      incr_yychar1 = YYTRANSLATE (incr_yychar);				\
      YYPOPSTACK;						\
      goto incr_yybackup;						\
    }								\
  else								\
    { 								\
      incr_yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).

   When YYLLOC_DEFAULT is run, CURRENT is set the location of the
   first token.  By default, to implement support for ranges, extend
   its range to the last symbol.  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif


/* YYLEX -- calling `incr_yylex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		incr_yylex (&incr_yylval, &incr_yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		incr_yylex (&incr_yylval, &incr_yylloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		incr_yylex (&incr_yylval, YYLEX_PARAM)
#  else
#   define YYLEX		incr_yylex (&incr_yylval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			incr_yylex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (incr_yydebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int incr_yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif

#ifdef YYERROR_VERBOSE

# ifndef incr_yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define incr_yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
incr_yystrlen (const char *incr_yystr)
#   else
incr_yystrlen (incr_yystr)
     const char *incr_yystr;
#   endif
{
  register const char *incr_yys = incr_yystr;

  while (*incr_yys++ != '\0')
    continue;

  return incr_yys - incr_yystr - 1;
}
#  endif
# endif

# ifndef incr_yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define incr_yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
incr_yystpcpy (char *incr_yydest, const char *incr_yysrc)
#   else
incr_yystpcpy (incr_yydest, incr_yysrc)
     char *incr_yydest;
     const char *incr_yysrc;
#   endif
{
  register char *incr_yyd = incr_yydest;
  register const char *incr_yys = incr_yysrc;

  while ((*incr_yyd++ = *incr_yys++) != '\0')
    continue;

  return incr_yyd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into incr_yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else /* !YYPARSE_PARAM */
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif /* !YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int incr_yyparse (void *);
# else
int incr_yyparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int incr_yychar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE incr_yylval;						\
							\
/* Number of parse errors so far.  */			\
int incr_yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE incr_yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
incr_yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int incr_yystate;
  register int incr_yyn;
  int incr_yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int incr_yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int incr_yychar1 = 0;

  /* Three stacks and their tools:
     `incr_yyss': related to states,
     `incr_yyvs': related to semantic values,
     `incr_yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow incr_yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	incr_yyssa[YYINITDEPTH];
  short *incr_yyss = incr_yyssa;
  register short *incr_yyssp;

  /* The semantic value stack.  */
  YYSTYPE incr_yyvsa[YYINITDEPTH];
  YYSTYPE *incr_yyvs = incr_yyvsa;
  register YYSTYPE *incr_yyvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE incr_yylsa[YYINITDEPTH];
  YYLTYPE *incr_yyls = incr_yylsa;
  YYLTYPE *incr_yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (incr_yyvsp--, incr_yyssp--, incr_yylsp--)
#else
# define YYPOPSTACK   (incr_yyvsp--, incr_yyssp--)
#endif

  YYSIZE_T incr_yystacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE incr_yyval;
#if YYLSP_NEEDED
  YYLTYPE incr_yyloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int incr_yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  incr_yystate = 0;
  incr_yyerrstatus = 0;
  incr_yynerrs = 0;
  incr_yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  incr_yyssp = incr_yyss;
  incr_yyvsp = incr_yyvs;
#if YYLSP_NEEDED
  incr_yylsp = incr_yyls;
#endif
  goto incr_yysetstate;

/*------------------------------------------------------------.
| incr_yynewstate -- Push a new state, which is found in incr_yystate.  |
`------------------------------------------------------------*/
 incr_yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  incr_yyssp++;

 incr_yysetstate:
  *incr_yyssp = incr_yystate;

  if (incr_yyssp >= incr_yyss + incr_yystacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T incr_yysize = incr_yyssp - incr_yyss + 1;

#ifdef incr_yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *incr_yyvs1 = incr_yyvs;
	short *incr_yyss1 = incr_yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *incr_yyls1 = incr_yyls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if incr_yyoverflow is a macro.  */
	incr_yyoverflow ("parser stack overflow",
		    &incr_yyss1, incr_yysize * sizeof (*incr_yyssp),
		    &incr_yyvs1, incr_yysize * sizeof (*incr_yyvsp),
		    &incr_yyls1, incr_yysize * sizeof (*incr_yylsp),
		    &incr_yystacksize);
	incr_yyls = incr_yyls1;
# else
	incr_yyoverflow ("parser stack overflow",
		    &incr_yyss1, incr_yysize * sizeof (*incr_yyssp),
		    &incr_yyvs1, incr_yysize * sizeof (*incr_yyvsp),
		    &incr_yystacksize);
# endif
	incr_yyss = incr_yyss1;
	incr_yyvs = incr_yyvs1;
      }
#else /* no incr_yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto incr_yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (incr_yystacksize >= YYMAXDEPTH)
	goto incr_yyoverflowlab;
      incr_yystacksize *= 2;
      if (incr_yystacksize > YYMAXDEPTH)
	incr_yystacksize = YYMAXDEPTH;

      {
	short *incr_yyss1 = incr_yyss;
	union incr_yyalloc *incr_yyptr =
	  (union incr_yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (incr_yystacksize));
	if (! incr_yyptr)
	  goto incr_yyoverflowlab;
	YYSTACK_RELOCATE (incr_yyss);
	YYSTACK_RELOCATE (incr_yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (incr_yyls);
# endif
# undef YYSTACK_RELOCATE
	if (incr_yyss1 != incr_yyssa)
	  YYSTACK_FREE (incr_yyss1);
      }
# endif
#endif /* no incr_yyoverflow */

      incr_yyssp = incr_yyss + incr_yysize - 1;
      incr_yyvsp = incr_yyvs + incr_yysize - 1;
#if YYLSP_NEEDED
      incr_yylsp = incr_yyls + incr_yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) incr_yystacksize));

      if (incr_yyssp >= incr_yyss + incr_yystacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", incr_yystate));

  goto incr_yybackup;


/*-----------.
| incr_yybackup.  |
`-----------*/
incr_yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* incr_yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  incr_yyn = incr_yypact[incr_yystate];
  if (incr_yyn == YYFLAG)
    goto incr_yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* incr_yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (incr_yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      incr_yychar = YYLEX;
    }

  /* Convert token to internal form (in incr_yychar1) for indexing tables with */

  if (incr_yychar <= 0)		/* This means end of input. */
    {
      incr_yychar1 = 0;
      incr_yychar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      incr_yychar1 = YYTRANSLATE (incr_yychar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (incr_yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     incr_yychar, incr_yytname[incr_yychar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, incr_yychar, incr_yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  incr_yyn += incr_yychar1;
  if (incr_yyn < 0 || incr_yyn > YYLAST || incr_yycheck[incr_yyn] != incr_yychar1)
    goto incr_yydefault;

  incr_yyn = incr_yytable[incr_yyn];

  /* incr_yyn is what to do for this token type in this state.
     Negative => reduce, -incr_yyn is rule number.
     Positive => shift, incr_yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (incr_yyn < 0)
    {
      if (incr_yyn == YYFLAG)
	goto incr_yyerrlab;
      incr_yyn = -incr_yyn;
      goto incr_yyreduce;
    }
  else if (incr_yyn == 0)
    goto incr_yyerrlab;

  if (incr_yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      incr_yychar, incr_yytname[incr_yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (incr_yychar != YYEOF)
    incr_yychar = YYEMPTY;

  *++incr_yyvsp = incr_yylval;
#if YYLSP_NEEDED
  *++incr_yylsp = incr_yylloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (incr_yyerrstatus)
    incr_yyerrstatus--;

  incr_yystate = incr_yyn;
  goto incr_yynewstate;


/*-----------------------------------------------------------.
| incr_yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
incr_yydefault:
  incr_yyn = incr_yydefact[incr_yystate];
  if (incr_yyn == 0)
    goto incr_yyerrlab;
  goto incr_yyreduce;


/*-----------------------------.
| incr_yyreduce -- Do a reduction.  |
`-----------------------------*/
incr_yyreduce:
  /* incr_yyn is the number of a rule to reduce with.  */
  incr_yylen = incr_yyr2[incr_yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  incr_yyval = incr_yyvsp[1-incr_yylen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  incr_yyloc = incr_yylsp[1-incr_yylen];
  YYLLOC_DEFAULT (incr_yyloc, (incr_yylsp - incr_yylen), incr_yylen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (incr_yydebug)
    {
      int incr_yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 incr_yyn, incr_yyrline[incr_yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (incr_yyi = incr_yyprhs[incr_yyn]; incr_yyrhs[incr_yyi] > 0; incr_yyi++)
	YYFPRINTF (stderr, "%s ", incr_yytname[incr_yyrhs[incr_yyi]]);
      YYFPRINTF (stderr, " -> %s\n", incr_yytname[incr_yyr1[incr_yyn]]);
    }
#endif

  switch (incr_yyn) {

case 18:
#line 36 "incrgram.ypp"
{incr_abort(IF_ERR_SYNTAX);}
    break;
case 20:
#line 42 "incrgram.ypp"
{incr_message(incr_yyvsp[0].str);}
    break;
case 21:
#line 45 "incrgram.ypp"
{}
    break;
case 22:
#line 48 "incrgram.ypp"
{incr_open_view(incr_yyvsp[-1].str);}
    break;
case 23:
#line 51 "incrgram.ypp"
{incr_close_view(incr_yyvsp[0].str);}
    break;
case 24:
#line 54 "incrgram.ypp"
{incr_mod_view(incr_yyvsp[-1].str);}
    break;
case 25:
#line 57 "incrgram.ypp"
{incr_lock(incr_yyvsp[0].str);}
    break;
case 26:
#line 60 "incrgram.ypp"
{incr_unlock(incr_yyvsp[0].str);}
    break;
case 27:
#line 63 "incrgram.ypp"
{incr_segue(incr_yyvsp[0].str);}
    break;
case 28:
#line 66 "incrgram.ypp"
{incr_ins_node(incr_yyvsp[-3].str,incr_yyvsp[-1].str);}
    break;
case 29:
#line 69 "incrgram.ypp"
{incr_mod_node(incr_yyvsp[-3].str,incr_yyvsp[-1].str);}
    break;
case 30:
#line 72 "incrgram.ypp"
{incr_del_node(incr_yyvsp[-2].str,incr_yyvsp[0].str);}
    break;
case 31:
#line 75 "incrgram.ypp"
{incr_ins_edge(incr_yyvsp[-5].str,incr_yyvsp[-3].str,incr_yyvsp[-2].str,incr_yyvsp[-1].str);}
    break;
case 32:
#line 78 "incrgram.ypp"
{incr_mod_edge(incr_yyvsp[-3].str,incr_yyvsp[-1].str);}
    break;
case 33:
#line 81 "incrgram.ypp"
{incr_del_edge(incr_yyvsp[-2].str,incr_yyvsp[0].str);}
    break;
case 34:
#line 84 "incrgram.ypp"
{incr_reset_attrs();}
    break;
case 36:
#line 85 "incrgram.ypp"
{incr_reset_attrs();}
    break;
case 41:
#line 96 "incrgram.ypp"
{incr_append_attr(incr_yyvsp[-2].str,incr_yyvsp[0].str);}
    break;
case 45:
#line 101 "incrgram.ypp"
{incr_yyval.str = incr_yyvsp[0].str; }
    break;
}

#line 705 "/usr/share/bison/bison.simple"


  incr_yyvsp -= incr_yylen;
  incr_yyssp -= incr_yylen;
#if YYLSP_NEEDED
  incr_yylsp -= incr_yylen;
#endif

#if YYDEBUG
  if (incr_yydebug)
    {
      short *incr_yyssp1 = incr_yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (incr_yyssp1 != incr_yyssp)
	YYFPRINTF (stderr, " %d", *++incr_yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++incr_yyvsp = incr_yyval;
#if YYLSP_NEEDED
  *++incr_yylsp = incr_yyloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  incr_yyn = incr_yyr1[incr_yyn];

  incr_yystate = incr_yypgoto[incr_yyn - YYNTBASE] + *incr_yyssp;
  if (incr_yystate >= 0 && incr_yystate <= YYLAST && incr_yycheck[incr_yystate] == *incr_yyssp)
    incr_yystate = incr_yytable[incr_yystate];
  else
    incr_yystate = incr_yydefgoto[incr_yyn - YYNTBASE];

  goto incr_yynewstate;


/*------------------------------------.
| incr_yyerrlab -- here on detecting error |
`------------------------------------*/
incr_yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!incr_yyerrstatus)
    {
      ++incr_yynerrs;

#ifdef YYERROR_VERBOSE
      incr_yyn = incr_yypact[incr_yystate];

      if (incr_yyn > YYFLAG && incr_yyn < YYLAST)
	{
	  YYSIZE_T incr_yysize = 0;
	  char *incr_yymsg;
	  int incr_yyx, incr_yycount;

	  incr_yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (incr_yyx = incr_yyn < 0 ? -incr_yyn : 0;
	       incr_yyx < (int) (sizeof (incr_yytname) / sizeof (char *)); incr_yyx++)
	    if (incr_yycheck[incr_yyx + incr_yyn] == incr_yyx)
	      incr_yysize += incr_yystrlen (incr_yytname[incr_yyx]) + 15, incr_yycount++;
	  incr_yysize += incr_yystrlen ("parse error, unexpected ") + 1;
	  incr_yysize += incr_yystrlen (incr_yytname[YYTRANSLATE (incr_yychar)]);
	  incr_yymsg = (char *) YYSTACK_ALLOC (incr_yysize);
	  if (incr_yymsg != 0)
	    {
	      char *incr_yyp = incr_yystpcpy (incr_yymsg, "parse error, unexpected ");
	      incr_yyp = incr_yystpcpy (incr_yyp, incr_yytname[YYTRANSLATE (incr_yychar)]);

	      if (incr_yycount < 5)
		{
		  incr_yycount = 0;
		  for (incr_yyx = incr_yyn < 0 ? -incr_yyn : 0;
		       incr_yyx < (int) (sizeof (incr_yytname) / sizeof (char *));
		       incr_yyx++)
		    if (incr_yycheck[incr_yyx + incr_yyn] == incr_yyx)
		      {
			const char *incr_yyq = ! incr_yycount ? ", expecting " : " or ";
			incr_yyp = incr_yystpcpy (incr_yyp, incr_yyq);
			incr_yyp = incr_yystpcpy (incr_yyp, incr_yytname[incr_yyx]);
			incr_yycount++;
		      }
		}
	      incr_yyerror (incr_yymsg);
	      YYSTACK_FREE (incr_yymsg);
	    }
	  else
	    incr_yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	incr_yyerror ("parse error");
    }
  goto incr_yyerrlab1;


/*--------------------------------------------------.
| incr_yyerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
incr_yyerrlab1:
  if (incr_yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (incr_yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  incr_yychar, incr_yytname[incr_yychar1]));
      incr_yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  incr_yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto incr_yyerrhandle;


/*-------------------------------------------------------------------.
| incr_yyerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
incr_yyerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  incr_yyn = incr_yydefact[incr_yystate];
  if (incr_yyn)
    goto incr_yydefault;
#endif


/*---------------------------------------------------------------.
| incr_yyerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
incr_yyerrpop:
  if (incr_yyssp == incr_yyss)
    YYABORT;
  incr_yyvsp--;
  incr_yystate = *--incr_yyssp;
#if YYLSP_NEEDED
  incr_yylsp--;
#endif

#if YYDEBUG
  if (incr_yydebug)
    {
      short *incr_yyssp1 = incr_yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (incr_yyssp1 != incr_yyssp)
	YYFPRINTF (stderr, " %d", *++incr_yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| incr_yyerrhandle.  |
`--------------*/
incr_yyerrhandle:
  incr_yyn = incr_yypact[incr_yystate];
  if (incr_yyn == YYFLAG)
    goto incr_yyerrdefault;

  incr_yyn += YYTERROR;
  if (incr_yyn < 0 || incr_yyn > YYLAST || incr_yycheck[incr_yyn] != YYTERROR)
    goto incr_yyerrdefault;

  incr_yyn = incr_yytable[incr_yyn];
  if (incr_yyn < 0)
    {
      if (incr_yyn == YYFLAG)
	goto incr_yyerrpop;
      incr_yyn = -incr_yyn;
      goto incr_yyreduce;
    }
  else if (incr_yyn == 0)
    goto incr_yyerrpop;

  if (incr_yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++incr_yyvsp = incr_yylval;
#if YYLSP_NEEDED
  *++incr_yylsp = incr_yylloc;
#endif

  incr_yystate = incr_yyn;
  goto incr_yynewstate;


/*-------------------------------------.
| incr_yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
incr_yyacceptlab:
  incr_yyresult = 0;
  goto incr_yyreturn;

/*-----------------------------------.
| incr_yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
incr_yyabortlab:
  incr_yyresult = 1;
  goto incr_yyreturn;

/*---------------------------------------------.
| incr_yyoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
incr_yyoverflowlab:
  incr_yyerror ("parser stack overflow");
  incr_yyresult = 2;
  /* Fall through.  */

incr_yyreturn:
#ifndef incr_yyoverflow
  if (incr_yyss != incr_yyssa)
    YYSTACK_FREE (incr_yyss);
#endif
  return incr_yyresult;
}
#line 102 "incrgram.ypp"
