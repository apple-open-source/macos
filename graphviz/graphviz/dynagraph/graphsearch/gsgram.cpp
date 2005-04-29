/* A Bison parser, made from gsgram.ypp
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	T_graph	257
# define	T_node	258
# define	T_edge	259
# define	T_view	260
# define	T_pattern	261
# define	T_search	262
# define	T_input	263
# define	T_open	264
# define	T_close	265
# define	T_insert	266
# define	T_delete	267
# define	T_modify	268
# define	T_lock	269
# define	T_unlock	270
# define	T_segue	271
# define	T_define	272
# define	T_id	273
# define	T_edgeop	274
# define	T_subgraph	275

#line 1 "gsgram.ypp"

#pragma prototyped
#include "graphsearch/gscmds.h"

#line 12 "gsgram.ypp"
#ifndef YYSTYPE
typedef union	{
			int				i;
			char			*str;
} gs_yystype;
# define YYSTYPE gs_yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		85
#define	YYFLAG		-32768
#define	YYNTBASE	28

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 275 ? gs_yytranslate[x] : 55)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char gs_yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      22,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    27,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    26,
       2,    25,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    23,     2,    24,     2,     2,     2,     2,     2,     2,
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
      16,    17,    18,    19,    20,    21
};

#if YYDEBUG
static const short gs_yyprhs[] =
{
       0,     0,     2,     6,     7,     9,    11,    13,    15,    17,
      19,    21,    23,    25,    27,    29,    31,    33,    35,    37,
      39,    41,    42,    46,    49,    53,    58,    62,    66,    70,
      73,    76,    79,    85,    91,    96,   104,   110,   115,   116,
     121,   122,   124,   125,   127,   131,   135,   137,   139,   140
};
static const short gs_yyrhs[] =
{
      29,     0,    29,    30,    22,     0,     0,    42,     0,    45,
       0,    43,     0,    46,     0,    44,     0,    47,     0,    32,
       0,    34,     0,    35,     0,    36,     0,    37,     0,    38,
       0,    39,     0,    40,     0,    41,     0,    31,     0,     1,
       0,     0,    10,     6,    33,     0,    54,    48,     0,    11,
       6,    54,     0,    14,     6,    54,    48,     0,    15,     6,
      54,     0,    16,     6,    54,     0,    17,     6,    54,     0,
      18,     7,     0,    18,     8,     0,    18,     9,     0,    12,
      54,     4,    19,    48,     0,    14,    54,     4,    19,    48,
       0,    13,    54,     4,    19,     0,    12,    54,     5,    19,
      19,    19,    48,     0,    14,    54,     5,    19,    48,     0,
      13,    54,     5,    19,     0,     0,    23,    49,    50,    24,
       0,     0,    51,     0,     0,    52,     0,    51,    53,    52,
       0,    19,    25,    19,     0,    26,     0,    27,     0,     0,
      19,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short gs_yyrline[] =
{
       0,    19,    23,    24,    27,    28,    29,    29,    30,    30,
      31,    31,    31,    32,    32,    33,    34,    34,    34,    35,
      36,    39,    42,    45,    48,    51,    54,    57,    60,    63,
      66,    69,    72,    75,    78,    81,    84,    87,    90,    90,
      91,    94,    95,    98,    99,   102,   105,   105,   105,   107
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const gs_yytname[] =
{
  "$", "error", "$undefined.", "T_graph", "T_node", "T_edge", "T_view", 
  "T_pattern", "T_search", "T_input", "T_open", "T_close", "T_insert", 
  "T_delete", "T_modify", "T_lock", "T_unlock", "T_segue", "T_define", 
  "T_id", "T_edgeop", "T_subgraph", "'\\n'", "'['", "']'", "'='", "';'", 
  "','", "session", "commands", "command", "nop", "open_view", 
  "open_view2", "close_view", "mod_view", "lock_view", "unlock_view", 
  "segue", "define_pattern", "define_search", "define_input", "ins_node", 
  "mod_node", "del_node", "ins_edge", "mod_edge", "del_edge", "attrlist", 
  "@1", "optattrdefs", "attrdefs", "attritem", "optsep", "viewid", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short gs_yyr1[] =
{
       0,    28,    29,    29,    30,    30,    30,    30,    30,    30,
      30,    30,    30,    30,    30,    30,    30,    30,    30,    30,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    49,    48,
      48,    50,    50,    51,    51,    52,    53,    53,    53,    54
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short gs_yyr2[] =
{
       0,     1,     3,     0,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     0,     3,     2,     3,     4,     3,     3,     3,     2,
       2,     2,     5,     5,     4,     7,     5,     4,     0,     4,
       0,     1,     0,     1,     3,     3,     1,     1,     0,     1
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short gs_yydefact[] =
{
       3,     0,    20,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    19,    10,    11,    12,    13,    14,    15,
      16,    17,    18,     4,     6,     8,     5,     7,     9,     0,
       0,    49,     0,     0,     0,     0,     0,     0,     0,    29,
      30,    31,     2,    22,    40,    24,     0,     0,     0,     0,
      40,     0,     0,    26,    27,    28,    38,    23,    40,     0,
      34,    37,    25,    40,    40,    42,    32,     0,    33,    36,
       0,     0,    41,    43,    40,     0,    39,    46,    47,     0,
      35,    45,    44,     0,     0,     0
};

static const short gs_yydefgoto[] =
{
      83,     1,    12,    13,    14,    43,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      57,    65,    71,    72,    73,    79,    32
};

static const short gs_yypact[] =
{
  -32768,    33,-32768,    -3,     3,    -8,    -8,    -2,     8,    19,
      21,    -1,     0,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,    -8,
      -8,-32768,    14,    16,    -8,    31,    -8,    -8,    -8,-32768,
  -32768,-32768,-32768,-32768,     6,-32768,    18,    20,    22,    23,
       6,    34,    35,-32768,-32768,-32768,-32768,-32768,     6,    37,
  -32768,-32768,-32768,     6,     6,    38,-32768,    39,-32768,-32768,
      13,    28,   -14,-32768,     6,    40,-32768,-32768,-32768,    38,
  -32768,-32768,-32768,    60,    61,-32768
};

static const short gs_yypgoto[] =
{
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
  -32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
     -48,-32768,-32768,-32768,   -39,-32768,    -6
};


#define	YYLAST		61


static const short gs_yytable[] =
{
      33,    35,    62,    29,    34,   -48,    39,    40,    41,    30,
      66,    31,    77,    78,    36,    68,    69,    31,    46,    47,
      48,    49,    42,    44,    45,    37,    80,    38,    50,    56,
      53,    54,    55,    -1,     2,    51,    52,    58,    75,    59,
      82,    60,    61,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    76,    63,    64,   -21,    67,    70,    74,    81,
      84,    85
};

static const short gs_yycheck[] =
{
       6,     7,    50,     6,     6,    19,     7,     8,     9,     6,
      58,    19,    26,    27,     6,    63,    64,    19,     4,     5,
       4,     5,    22,    29,    30,     6,    74,     6,    34,    23,
      36,    37,    38,     0,     1,     4,     5,    19,    25,    19,
      79,    19,    19,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    24,    19,    19,    22,    19,    19,    19,    19,
       0,     0
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

/* All symbols defined below should begin with gs_yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (gs_yyoverflow) || defined (YYERROR_VERBOSE)

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
#endif /* ! defined (gs_yyoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (gs_yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union gs_yyalloc
{
  short gs_yyss;
  YYSTYPE gs_yyvs;
# if YYLSP_NEEDED
  YYLTYPE gs_yyls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union gs_yyalloc) - 1)

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
	  register YYSIZE_T gs_yyi;		\
	  for (gs_yyi = 0; gs_yyi < (Count); gs_yyi++)	\
	    (To)[gs_yyi] = (From)[gs_yyi];		\
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
	YYSIZE_T gs_yynewbytes;						\
	YYCOPY (&gs_yyptr->Stack, Stack, gs_yysize);				\
	Stack = &gs_yyptr->Stack;						\
	gs_yynewbytes = gs_yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	gs_yyptr += gs_yynewbytes / sizeof (*gs_yyptr);				\
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

#define gs_yyerrok		(gs_yyerrstatus = 0)
#define gs_yyclearin	(gs_yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto gs_yyacceptlab
#define YYABORT 	goto gs_yyabortlab
#define YYERROR		goto gs_yyerrlab1
/* Like YYERROR except do call gs_yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto gs_yyerrlab
#define YYRECOVERING()  (!!gs_yyerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (gs_yychar == YYEMPTY && gs_yylen == 1)				\
    {								\
      gs_yychar = (Token);						\
      gs_yylval = (Value);						\
      gs_yychar1 = YYTRANSLATE (gs_yychar);				\
      YYPOPSTACK;						\
      goto gs_yybackup;						\
    }								\
  else								\
    { 								\
      gs_yyerror ("syntax error: cannot back up");			\
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


/* YYLEX -- calling `gs_yylex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		gs_yylex (&gs_yylval, &gs_yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		gs_yylex (&gs_yylval, &gs_yylloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		gs_yylex (&gs_yylval, YYLEX_PARAM)
#  else
#   define YYLEX		gs_yylex (&gs_yylval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			gs_yylex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (gs_yydebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int gs_yydebug;
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

# ifndef gs_yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define gs_yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
gs_yystrlen (const char *gs_yystr)
#   else
gs_yystrlen (gs_yystr)
     const char *gs_yystr;
#   endif
{
  register const char *gs_yys = gs_yystr;

  while (*gs_yys++ != '\0')
    continue;

  return gs_yys - gs_yystr - 1;
}
#  endif
# endif

# ifndef gs_yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define gs_yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
gs_yystpcpy (char *gs_yydest, const char *gs_yysrc)
#   else
gs_yystpcpy (gs_yydest, gs_yysrc)
     char *gs_yydest;
     const char *gs_yysrc;
#   endif
{
  register char *gs_yyd = gs_yydest;
  register const char *gs_yys = gs_yysrc;

  while ((*gs_yyd++ = *gs_yys++) != '\0')
    continue;

  return gs_yyd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into gs_yyparse.  The argument should have type void *.
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
int gs_yyparse (void *);
# else
int gs_yyparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int gs_yychar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE gs_yylval;						\
							\
/* Number of parse errors so far.  */			\
int gs_yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE gs_yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
gs_yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int gs_yystate;
  register int gs_yyn;
  int gs_yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int gs_yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int gs_yychar1 = 0;

  /* Three stacks and their tools:
     `gs_yyss': related to states,
     `gs_yyvs': related to semantic values,
     `gs_yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow gs_yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	gs_yyssa[YYINITDEPTH];
  short *gs_yyss = gs_yyssa;
  register short *gs_yyssp;

  /* The semantic value stack.  */
  YYSTYPE gs_yyvsa[YYINITDEPTH];
  YYSTYPE *gs_yyvs = gs_yyvsa;
  register YYSTYPE *gs_yyvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE gs_yylsa[YYINITDEPTH];
  YYLTYPE *gs_yyls = gs_yylsa;
  YYLTYPE *gs_yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (gs_yyvsp--, gs_yyssp--, gs_yylsp--)
#else
# define YYPOPSTACK   (gs_yyvsp--, gs_yyssp--)
#endif

  YYSIZE_T gs_yystacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE gs_yyval;
#if YYLSP_NEEDED
  YYLTYPE gs_yyloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int gs_yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  gs_yystate = 0;
  gs_yyerrstatus = 0;
  gs_yynerrs = 0;
  gs_yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  gs_yyssp = gs_yyss;
  gs_yyvsp = gs_yyvs;
#if YYLSP_NEEDED
  gs_yylsp = gs_yyls;
#endif
  goto gs_yysetstate;

/*------------------------------------------------------------.
| gs_yynewstate -- Push a new state, which is found in gs_yystate.  |
`------------------------------------------------------------*/
 gs_yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  gs_yyssp++;

 gs_yysetstate:
  *gs_yyssp = gs_yystate;

  if (gs_yyssp >= gs_yyss + gs_yystacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T gs_yysize = gs_yyssp - gs_yyss + 1;

#ifdef gs_yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *gs_yyvs1 = gs_yyvs;
	short *gs_yyss1 = gs_yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *gs_yyls1 = gs_yyls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if gs_yyoverflow is a macro.  */
	gs_yyoverflow ("parser stack overflow",
		    &gs_yyss1, gs_yysize * sizeof (*gs_yyssp),
		    &gs_yyvs1, gs_yysize * sizeof (*gs_yyvsp),
		    &gs_yyls1, gs_yysize * sizeof (*gs_yylsp),
		    &gs_yystacksize);
	gs_yyls = gs_yyls1;
# else
	gs_yyoverflow ("parser stack overflow",
		    &gs_yyss1, gs_yysize * sizeof (*gs_yyssp),
		    &gs_yyvs1, gs_yysize * sizeof (*gs_yyvsp),
		    &gs_yystacksize);
# endif
	gs_yyss = gs_yyss1;
	gs_yyvs = gs_yyvs1;
      }
#else /* no gs_yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto gs_yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (gs_yystacksize >= YYMAXDEPTH)
	goto gs_yyoverflowlab;
      gs_yystacksize *= 2;
      if (gs_yystacksize > YYMAXDEPTH)
	gs_yystacksize = YYMAXDEPTH;

      {
	short *gs_yyss1 = gs_yyss;
	union gs_yyalloc *gs_yyptr =
	  (union gs_yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (gs_yystacksize));
	if (! gs_yyptr)
	  goto gs_yyoverflowlab;
	YYSTACK_RELOCATE (gs_yyss);
	YYSTACK_RELOCATE (gs_yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (gs_yyls);
# endif
# undef YYSTACK_RELOCATE
	if (gs_yyss1 != gs_yyssa)
	  YYSTACK_FREE (gs_yyss1);
      }
# endif
#endif /* no gs_yyoverflow */

      gs_yyssp = gs_yyss + gs_yysize - 1;
      gs_yyvsp = gs_yyvs + gs_yysize - 1;
#if YYLSP_NEEDED
      gs_yylsp = gs_yyls + gs_yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) gs_yystacksize));

      if (gs_yyssp >= gs_yyss + gs_yystacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", gs_yystate));

  goto gs_yybackup;


/*-----------.
| gs_yybackup.  |
`-----------*/
gs_yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* gs_yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  gs_yyn = gs_yypact[gs_yystate];
  if (gs_yyn == YYFLAG)
    goto gs_yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* gs_yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (gs_yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      gs_yychar = YYLEX;
    }

  /* Convert token to internal form (in gs_yychar1) for indexing tables with */

  if (gs_yychar <= 0)		/* This means end of input. */
    {
      gs_yychar1 = 0;
      gs_yychar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      gs_yychar1 = YYTRANSLATE (gs_yychar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (gs_yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     gs_yychar, gs_yytname[gs_yychar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, gs_yychar, gs_yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  gs_yyn += gs_yychar1;
  if (gs_yyn < 0 || gs_yyn > YYLAST || gs_yycheck[gs_yyn] != gs_yychar1)
    goto gs_yydefault;

  gs_yyn = gs_yytable[gs_yyn];

  /* gs_yyn is what to do for this token type in this state.
     Negative => reduce, -gs_yyn is rule number.
     Positive => shift, gs_yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (gs_yyn < 0)
    {
      if (gs_yyn == YYFLAG)
	goto gs_yyerrlab;
      gs_yyn = -gs_yyn;
      goto gs_yyreduce;
    }
  else if (gs_yyn == 0)
    goto gs_yyerrlab;

  if (gs_yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      gs_yychar, gs_yytname[gs_yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (gs_yychar != YYEOF)
    gs_yychar = YYEMPTY;

  *++gs_yyvsp = gs_yylval;
#if YYLSP_NEEDED
  *++gs_yylsp = gs_yylloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (gs_yyerrstatus)
    gs_yyerrstatus--;

  gs_yystate = gs_yyn;
  goto gs_yynewstate;


/*-----------------------------------------------------------.
| gs_yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
gs_yydefault:
  gs_yyn = gs_yydefact[gs_yystate];
  if (gs_yyn == 0)
    goto gs_yyerrlab;
  goto gs_yyreduce;


/*-----------------------------.
| gs_yyreduce -- Do a reduction.  |
`-----------------------------*/
gs_yyreduce:
  /* gs_yyn is the number of a rule to reduce with.  */
  gs_yylen = gs_yyr2[gs_yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  gs_yyval = gs_yyvsp[1-gs_yylen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  gs_yyloc = gs_yylsp[1-gs_yylen];
  YYLLOC_DEFAULT (gs_yyloc, (gs_yylsp - gs_yylen), gs_yylen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (gs_yydebug)
    {
      int gs_yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 gs_yyn, gs_yyrline[gs_yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (gs_yyi = gs_yyprhs[gs_yyn]; gs_yyrhs[gs_yyi] > 0; gs_yyi++)
	YYFPRINTF (stderr, "%s ", gs_yytname[gs_yyrhs[gs_yyi]]);
      YYFPRINTF (stderr, " -> %s\n", gs_yytname[gs_yyr1[gs_yyn]]);
    }
#endif

  switch (gs_yyn) {

case 20:
#line 36 "gsgram.ypp"
{gs_abort(IF_ERR_SYNTAX);}
    break;
case 22:
#line 42 "gsgram.ypp"
{}
    break;
case 23:
#line 45 "gsgram.ypp"
{gs_open_view(gs_yyvsp[-1].str);}
    break;
case 24:
#line 48 "gsgram.ypp"
{gs_close_view(gs_yyvsp[0].str);}
    break;
case 25:
#line 51 "gsgram.ypp"
{gs_mod_view(gs_yyvsp[-1].str);}
    break;
case 26:
#line 54 "gsgram.ypp"
{gs_lock(gs_yyvsp[0].str);}
    break;
case 27:
#line 57 "gsgram.ypp"
{gs_unlock(gs_yyvsp[0].str);}
    break;
case 28:
#line 60 "gsgram.ypp"
{gs_segue(gs_yyvsp[0].str);}
    break;
case 29:
#line 63 "gsgram.ypp"
{ gs_define_pattern(); }
    break;
case 30:
#line 66 "gsgram.ypp"
{ gs_define_search(); }
    break;
case 31:
#line 69 "gsgram.ypp"
{ gs_define_input(); }
    break;
case 32:
#line 72 "gsgram.ypp"
{gs_ins_node(gs_yyvsp[-3].str,gs_yyvsp[-1].str);}
    break;
case 33:
#line 75 "gsgram.ypp"
{gs_mod_node(gs_yyvsp[-3].str,gs_yyvsp[-1].str);}
    break;
case 34:
#line 78 "gsgram.ypp"
{gs_del_node(gs_yyvsp[-2].str,gs_yyvsp[0].str);}
    break;
case 35:
#line 81 "gsgram.ypp"
{gs_ins_edge(gs_yyvsp[-5].str,gs_yyvsp[-3].str,gs_yyvsp[-2].str,gs_yyvsp[-1].str);}
    break;
case 36:
#line 84 "gsgram.ypp"
{gs_mod_edge(gs_yyvsp[-3].str,gs_yyvsp[-1].str);}
    break;
case 37:
#line 87 "gsgram.ypp"
{gs_del_edge(gs_yyvsp[-2].str,gs_yyvsp[0].str);}
    break;
case 38:
#line 90 "gsgram.ypp"
{gs_reset_attrs();}
    break;
case 40:
#line 91 "gsgram.ypp"
{gs_reset_attrs();}
    break;
case 45:
#line 102 "gsgram.ypp"
{gs_append_attr(gs_yyvsp[-2].str,gs_yyvsp[0].str);}
    break;
case 49:
#line 107 "gsgram.ypp"
{gs_yyval.str = gs_yyvsp[0].str; }
    break;
}

#line 705 "/usr/share/bison/bison.simple"


  gs_yyvsp -= gs_yylen;
  gs_yyssp -= gs_yylen;
#if YYLSP_NEEDED
  gs_yylsp -= gs_yylen;
#endif

#if YYDEBUG
  if (gs_yydebug)
    {
      short *gs_yyssp1 = gs_yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (gs_yyssp1 != gs_yyssp)
	YYFPRINTF (stderr, " %d", *++gs_yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++gs_yyvsp = gs_yyval;
#if YYLSP_NEEDED
  *++gs_yylsp = gs_yyloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  gs_yyn = gs_yyr1[gs_yyn];

  gs_yystate = gs_yypgoto[gs_yyn - YYNTBASE] + *gs_yyssp;
  if (gs_yystate >= 0 && gs_yystate <= YYLAST && gs_yycheck[gs_yystate] == *gs_yyssp)
    gs_yystate = gs_yytable[gs_yystate];
  else
    gs_yystate = gs_yydefgoto[gs_yyn - YYNTBASE];

  goto gs_yynewstate;


/*------------------------------------.
| gs_yyerrlab -- here on detecting error |
`------------------------------------*/
gs_yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!gs_yyerrstatus)
    {
      ++gs_yynerrs;

#ifdef YYERROR_VERBOSE
      gs_yyn = gs_yypact[gs_yystate];

      if (gs_yyn > YYFLAG && gs_yyn < YYLAST)
	{
	  YYSIZE_T gs_yysize = 0;
	  char *gs_yymsg;
	  int gs_yyx, gs_yycount;

	  gs_yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (gs_yyx = gs_yyn < 0 ? -gs_yyn : 0;
	       gs_yyx < (int) (sizeof (gs_yytname) / sizeof (char *)); gs_yyx++)
	    if (gs_yycheck[gs_yyx + gs_yyn] == gs_yyx)
	      gs_yysize += gs_yystrlen (gs_yytname[gs_yyx]) + 15, gs_yycount++;
	  gs_yysize += gs_yystrlen ("parse error, unexpected ") + 1;
	  gs_yysize += gs_yystrlen (gs_yytname[YYTRANSLATE (gs_yychar)]);
	  gs_yymsg = (char *) YYSTACK_ALLOC (gs_yysize);
	  if (gs_yymsg != 0)
	    {
	      char *gs_yyp = gs_yystpcpy (gs_yymsg, "parse error, unexpected ");
	      gs_yyp = gs_yystpcpy (gs_yyp, gs_yytname[YYTRANSLATE (gs_yychar)]);

	      if (gs_yycount < 5)
		{
		  gs_yycount = 0;
		  for (gs_yyx = gs_yyn < 0 ? -gs_yyn : 0;
		       gs_yyx < (int) (sizeof (gs_yytname) / sizeof (char *));
		       gs_yyx++)
		    if (gs_yycheck[gs_yyx + gs_yyn] == gs_yyx)
		      {
			const char *gs_yyq = ! gs_yycount ? ", expecting " : " or ";
			gs_yyp = gs_yystpcpy (gs_yyp, gs_yyq);
			gs_yyp = gs_yystpcpy (gs_yyp, gs_yytname[gs_yyx]);
			gs_yycount++;
		      }
		}
	      gs_yyerror (gs_yymsg);
	      YYSTACK_FREE (gs_yymsg);
	    }
	  else
	    gs_yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	gs_yyerror ("parse error");
    }
  goto gs_yyerrlab1;


/*--------------------------------------------------.
| gs_yyerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
gs_yyerrlab1:
  if (gs_yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (gs_yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  gs_yychar, gs_yytname[gs_yychar1]));
      gs_yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  gs_yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto gs_yyerrhandle;


/*-------------------------------------------------------------------.
| gs_yyerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
gs_yyerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  gs_yyn = gs_yydefact[gs_yystate];
  if (gs_yyn)
    goto gs_yydefault;
#endif


/*---------------------------------------------------------------.
| gs_yyerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
gs_yyerrpop:
  if (gs_yyssp == gs_yyss)
    YYABORT;
  gs_yyvsp--;
  gs_yystate = *--gs_yyssp;
#if YYLSP_NEEDED
  gs_yylsp--;
#endif

#if YYDEBUG
  if (gs_yydebug)
    {
      short *gs_yyssp1 = gs_yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (gs_yyssp1 != gs_yyssp)
	YYFPRINTF (stderr, " %d", *++gs_yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| gs_yyerrhandle.  |
`--------------*/
gs_yyerrhandle:
  gs_yyn = gs_yypact[gs_yystate];
  if (gs_yyn == YYFLAG)
    goto gs_yyerrdefault;

  gs_yyn += YYTERROR;
  if (gs_yyn < 0 || gs_yyn > YYLAST || gs_yycheck[gs_yyn] != YYTERROR)
    goto gs_yyerrdefault;

  gs_yyn = gs_yytable[gs_yyn];
  if (gs_yyn < 0)
    {
      if (gs_yyn == YYFLAG)
	goto gs_yyerrpop;
      gs_yyn = -gs_yyn;
      goto gs_yyreduce;
    }
  else if (gs_yyn == 0)
    goto gs_yyerrpop;

  if (gs_yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++gs_yyvsp = gs_yylval;
#if YYLSP_NEEDED
  *++gs_yylsp = gs_yylloc;
#endif

  gs_yystate = gs_yyn;
  goto gs_yynewstate;


/*-------------------------------------.
| gs_yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
gs_yyacceptlab:
  gs_yyresult = 0;
  goto gs_yyreturn;

/*-----------------------------------.
| gs_yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
gs_yyabortlab:
  gs_yyresult = 1;
  goto gs_yyreturn;

/*---------------------------------------------.
| gs_yyoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
gs_yyoverflowlab:
  gs_yyerror ("parser stack overflow");
  gs_yyresult = 2;
  /* Fall through.  */

gs_yyreturn:
#ifndef gs_yyoverflow
  if (gs_yyss != gs_yyssa)
    YYSTACK_FREE (gs_yyss);
#endif
  return gs_yyresult;
}
#line 109 "gsgram.ypp"
