/* A Bison parser, made from /home/ellson/graphviz/agraph/grammar.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	T_graph	257
# define	T_node	258
# define	T_edge	259
# define	T_digraph	260
# define	T_subgraph	261
# define	T_strict	262
# define	T_edgeop	263
# define	T_list	264
# define	T_attr	265
# define	T_atom	266
# define	T_qatom	267

#line 1 "/home/ellson/graphviz/agraph/grammar.y"

/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#pragma prototyped

#include <stdio.h>  /* SAFE */
#include <aghdr.h>	/* SAFE */

#ifdef _WIN32
#define gettxt(a,b)	(b)
#endif

static char Key[] = "key";

typedef union s {					/* possible items in generic list */
		Agnode_t		*n;
		Agraph_t		*subg;
		Agedge_t		*e;
		Agsym_t			*asym;	/* bound attribute */
		char			*name;	/* unbound attribute */
		struct item_s	*list;	/* list-of-lists (for edgestmt) */
} val_t;

typedef struct item_s {		/* generic list */
	int				tag;	/* T_node, T_subgraph, T_edge, T_attr */
	val_t			u;		/* primary element */
	char			*str;	/* secondary value - port or attr value */
	struct item_s	*next;
} item;

typedef struct list_s {		/* maintain head and tail ptrs for fast append */
	item			*first;
	item			*last;
} list_t;

/* functions */
static void appendnode(char *name, char *port);
static void attrstmt(int tkind, char *macroname);
static void startgraph(char *name, int directed, int strict);
static void bufferedges(void);
static void newedge(Agnode_t *t, char *tport, Agnode_t *h, char *hport, char *key);
static void edgerhs(Agnode_t *n, char *tport, item *hlist, char *key);
static void appendattr(char *name, char *value);
static void bindattrs(int kind);
static void applyattrs(void *obj);
static void endgraph(void);
static void endnode(void);
static void endedge(void);
static char* concat(char*, char*);

static void opensubg(char *name);
static void closesubg(void);

/* global */
static Agraph_t *G;


#line 66 "/home/ellson/graphviz/agraph/grammar.y"
#ifndef YYSTYPE
typedef union	{
			int				i;
			char			*str;
			struct Agnode_s	*n;
} aagstype;
# define YYSTYPE aagstype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		78
#define	YYFLAG		-32768
#define	YYNTBASE	24

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 267 ? aagtranslate[x] : 58)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char aagtranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    23,    17,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    18,    16,
       2,    19,     2,     2,    22,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    20,     2,    21,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    14,     2,    15,     2,     2,     2,     2,
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
       6,     7,     8,     9,    10,    11,    12,    13
};

#if YYDEBUG
static const short aagprhs[] =
{
       0,     0,     3,     5,     6,    10,    14,    16,    17,    19,
      20,    22,    24,    26,    27,    30,    32,    34,    35,    38,
      41,    45,    47,    49,    50,    55,    56,    58,    62,    65,
      68,    69,    73,    75,    77,    79,    81,    84,    85,    87,
      88,    93,    96,    97,   100,   102,   104,   108,   111,   113,
     114,   118,   121,   123,   124,   126,   128,   129,   131,   133,
     135
};
static const short aagrhs[] =
{
      26,    25,     0,     1,     0,     0,    14,    30,    15,     0,
      28,    29,    27,     0,    56,     0,     0,     8,     0,     0,
       3,     0,     6,     0,    31,     0,     0,    31,    33,     0,
      33,     0,    16,     0,     0,    41,    32,     0,    34,    32,
       0,    35,    36,    44,     0,    38,     0,    52,     0,     0,
       9,    37,    35,    36,     0,     0,    39,     0,    38,    17,
      39,     0,    56,    40,     0,    18,    56,     0,     0,    42,
      43,    45,     0,    51,     0,     3,     0,     4,     0,     5,
       0,    56,    19,     0,     0,    45,     0,     0,    44,    20,
      46,    21,     0,    46,    47,     0,     0,    48,    55,     0,
      49,     0,    50,     0,    56,    19,    56,     0,    22,    56,
       0,    49,     0,     0,    54,    53,    25,     0,     7,    56,
       0,     7,     0,     0,    16,     0,    17,     0,     0,    12,
       0,    57,     0,    13,     0,    57,    23,    13,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short aagrline[] =
{
       0,    83,    84,    85,    88,    90,    93,    93,    95,    95,
      97,    97,    99,    99,   101,   101,   103,   103,   105,   106,
     109,   113,   113,   115,   115,   116,   120,   120,   122,   125,
     126,   129,   130,   133,   134,   135,   138,   139,   142,   142,
     144,   146,   147,   149,   152,   152,   154,   157,   160,   163,
     163,   166,   167,   168,   171,   171,   171,   173,   174,   177,
     178
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const aagtname[] =
{
  "$", "error", "$undefined.", "T_graph", "T_node", "T_edge", "T_digraph", 
  "T_subgraph", "T_strict", "T_edgeop", "T_list", "T_attr", "T_atom", 
  "T_qatom", "'{'", "'}'", "';'", "','", "':'", "'='", "'['", "']'", 
  "'@'", "'+'", "graph", "body", "hdr", "optgraphname", "optstrict", 
  "graphtype", "optstmtlist", "stmtlist", "optsemi", "stmt", "compound", 
  "simple", "rcompound", "@1", "nodelist", "node", "optport", "attrstmt", 
  "attrtype", "optmacroname", "optattr", "attrlist", "optattrdefs", 
  "attrdefs", "attritem", "attrassignment", "attrmacro", "graphattrdefs", 
  "subgraph", "@2", "optsubghdr", "optseparator", "atom", "qatom", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short aagr1[] =
{
       0,    24,    24,    24,    25,    26,    27,    27,    28,    28,
      29,    29,    30,    30,    31,    31,    32,    32,    33,    33,
      34,    35,    35,    37,    36,    36,    38,    38,    39,    40,
      40,    41,    41,    42,    42,    42,    43,    43,    44,    44,
      45,    46,    46,    47,    48,    48,    49,    50,    51,    53,
      52,    54,    54,    54,    55,    55,    55,    56,    56,    57,
      57
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short aagr2[] =
{
       0,     2,     1,     0,     3,     3,     1,     0,     1,     0,
       1,     1,     1,     0,     2,     1,     1,     0,     2,     2,
       3,     1,     1,     0,     4,     0,     1,     3,     2,     2,
       0,     3,     1,     1,     1,     1,     2,     0,     1,     0,
       4,     2,     0,     2,     1,     1,     3,     2,     1,     0,
       3,     2,     1,     0,     1,     1,     0,     1,     1,     1,
       3
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short aagdefact[] =
{
       0,     2,     8,     0,     0,    13,     1,    10,    11,     7,
      33,    34,    35,    52,    57,    59,     0,    12,    15,    17,
      25,    21,    26,    17,    37,    48,    32,    22,    49,    30,
      58,     5,     6,    51,     4,    14,    16,    19,    23,    39,
       0,    18,    39,     0,     0,     0,     0,    28,     0,    53,
      20,    38,    27,    30,     0,    31,    36,    50,    29,    46,
      60,    25,    42,    24,     0,    40,     0,    41,    56,    44,
      45,     0,    47,    54,    55,    43,     0,     0,     0
};

static const short aagdefgoto[] =
{
      76,     6,     3,    31,     4,     9,    16,    17,    37,    18,
      19,    20,    39,    49,    21,    22,    47,    23,    24,    42,
      50,    51,    64,    67,    68,    25,    70,    26,    27,    44,
      28,    75,    29,    30
};

static const short aagpact[] =
{
      18,-32768,-32768,     6,     3,    -2,-32768,-32768,-32768,     1,
  -32768,-32768,-32768,     1,-32768,-32768,     8,    -2,-32768,     9,
      25,    21,-32768,     9,     1,-32768,-32768,-32768,-32768,    11,
      12,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
       1,-32768,-32768,    20,     6,     1,     1,-32768,    28,    15,
      22,-32768,-32768,    26,    22,    23,-32768,-32768,-32768,-32768,
  -32768,    25,-32768,-32768,    -5,-32768,     1,-32768,    16,-32768,
  -32768,    27,-32768,-32768,-32768,-32768,    45,    47,-32768
};

static const short aagpgoto[] =
{
  -32768,     4,-32768,-32768,-32768,-32768,-32768,-32768,    29,    32,
  -32768,     2,   -11,-32768,-32768,    13,-32768,-32768,-32768,-32768,
      14,    17,-32768,-32768,-32768,   -10,-32768,-32768,-32768,-32768,
  -32768,-32768,    -9,-32768
};


#define	YYLAST		59


static const short aagtable[] =
{
      32,    10,    11,    12,    33,    13,     7,    14,    15,     8,
      14,    15,   -53,    14,    15,    43,    65,    66,    -3,     1,
       5,    -9,    13,    34,    -9,    36,     2,    14,    15,    45,
      46,    53,    73,    74,    38,    48,    58,    59,    40,    56,
      53,    60,    62,   -38,    45,    77,    46,    78,    57,    35,
      63,    61,    41,    52,    69,    71,    54,    72,     0,    55
};

static const short aagcheck[] =
{
       9,     3,     4,     5,    13,     7,     3,    12,    13,     6,
      12,    13,    14,    12,    13,    24,    21,    22,     0,     1,
      14,     3,     7,    15,     6,    16,     8,    12,    13,    18,
      19,    40,    16,    17,     9,    23,    45,    46,    17,    19,
      49,    13,    20,    20,    18,     0,    19,     0,    44,    17,
      61,    49,    23,    40,    64,    64,    42,    66,    -1,    42
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

/* All symbols defined below should begin with aag or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (aagoverflow) || defined (YYERROR_VERBOSE)

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
#endif /* ! defined (aagoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (aagoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union aagalloc
{
  short aagss;
  YYSTYPE aagvs;
# if YYLSP_NEEDED
  YYLTYPE aagls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union aagalloc) - 1)

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
	  register YYSIZE_T aagi;		\
	  for (aagi = 0; aagi < (Count); aagi++)	\
	    (To)[aagi] = (From)[aagi];		\
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
	YYSIZE_T aagnewbytes;						\
	YYCOPY (&aagptr->Stack, Stack, aagsize);				\
	Stack = &aagptr->Stack;						\
	aagnewbytes = aagstacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	aagptr += aagnewbytes / sizeof (*aagptr);				\
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

#define aagerrok		(aagerrstatus = 0)
#define aagclearin	(aagchar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto aagacceptlab
#define YYABORT 	goto aagabortlab
#define YYERROR		goto aagerrlab1
/* Like YYERROR except do call aagerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto aagerrlab
#define YYRECOVERING()  (!!aagerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (aagchar == YYEMPTY && aaglen == 1)				\
    {								\
      aagchar = (Token);						\
      aaglval = (Value);						\
      aagchar1 = YYTRANSLATE (aagchar);				\
      YYPOPSTACK;						\
      goto aagbackup;						\
    }								\
  else								\
    { 								\
      aagerror ("syntax error: cannot back up");			\
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


/* YYLEX -- calling `aaglex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		aaglex (&aaglval, &aaglloc, YYLEX_PARAM)
#  else
#   define YYLEX		aaglex (&aaglval, &aaglloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		aaglex (&aaglval, YYLEX_PARAM)
#  else
#   define YYLEX		aaglex (&aaglval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			aaglex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (aagdebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int aagdebug;
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

# ifndef aagstrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define aagstrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
aagstrlen (const char *aagstr)
#   else
aagstrlen (aagstr)
     const char *aagstr;
#   endif
{
  register const char *aags = aagstr;

  while (*aags++ != '\0')
    continue;

  return aags - aagstr - 1;
}
#  endif
# endif

# ifndef aagstpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define aagstpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
aagstpcpy (char *aagdest, const char *aagsrc)
#   else
aagstpcpy (aagdest, aagsrc)
     char *aagdest;
     const char *aagsrc;
#   endif
{
  register char *aagd = aagdest;
  register const char *aags = aagsrc;

  while ((*aagd++ = *aags++) != '\0')
    continue;

  return aagd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into aagparse.  The argument should have type void *.
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
int aagparse (void *);
# else
int aagparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int aagchar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE aaglval;						\
							\
/* Number of parse errors so far.  */			\
int aagnerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE aaglloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
aagparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int aagstate;
  register int aagn;
  int aagresult;
  /* Number of tokens to shift before error messages enabled.  */
  int aagerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int aagchar1 = 0;

  /* Three stacks and their tools:
     `aagss': related to states,
     `aagvs': related to semantic values,
     `aagls': related to locations.

     Refer to the stacks thru separate pointers, to allow aagoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	aagssa[YYINITDEPTH];
  short *aagss = aagssa;
  register short *aagssp;

  /* The semantic value stack.  */
  YYSTYPE aagvsa[YYINITDEPTH];
  YYSTYPE *aagvs = aagvsa;
  register YYSTYPE *aagvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE aaglsa[YYINITDEPTH];
  YYLTYPE *aagls = aaglsa;
  YYLTYPE *aaglsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (aagvsp--, aagssp--, aaglsp--)
#else
# define YYPOPSTACK   (aagvsp--, aagssp--)
#endif

  YYSIZE_T aagstacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE aagval;
#if YYLSP_NEEDED
  YYLTYPE aagloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int aaglen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  aagstate = 0;
  aagerrstatus = 0;
  aagnerrs = 0;
  aagchar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  aagssp = aagss;
  aagvsp = aagvs;
#if YYLSP_NEEDED
  aaglsp = aagls;
#endif
  goto aagsetstate;

/*------------------------------------------------------------.
| aagnewstate -- Push a new state, which is found in aagstate.  |
`------------------------------------------------------------*/
 aagnewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  aagssp++;

 aagsetstate:
  *aagssp = aagstate;

  if (aagssp >= aagss + aagstacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T aagsize = aagssp - aagss + 1;

#ifdef aagoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *aagvs1 = aagvs;
	short *aagss1 = aagss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *aagls1 = aagls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if aagoverflow is a macro.  */
	aagoverflow ("parser stack overflow",
		    &aagss1, aagsize * sizeof (*aagssp),
		    &aagvs1, aagsize * sizeof (*aagvsp),
		    &aagls1, aagsize * sizeof (*aaglsp),
		    &aagstacksize);
	aagls = aagls1;
# else
	aagoverflow ("parser stack overflow",
		    &aagss1, aagsize * sizeof (*aagssp),
		    &aagvs1, aagsize * sizeof (*aagvsp),
		    &aagstacksize);
# endif
	aagss = aagss1;
	aagvs = aagvs1;
      }
#else /* no aagoverflow */
# ifndef YYSTACK_RELOCATE
      goto aagoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (aagstacksize >= YYMAXDEPTH)
	goto aagoverflowlab;
      aagstacksize *= 2;
      if (aagstacksize > YYMAXDEPTH)
	aagstacksize = YYMAXDEPTH;

      {
	short *aagss1 = aagss;
	union aagalloc *aagptr =
	  (union aagalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (aagstacksize));
	if (! aagptr)
	  goto aagoverflowlab;
	YYSTACK_RELOCATE (aagss);
	YYSTACK_RELOCATE (aagvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (aagls);
# endif
# undef YYSTACK_RELOCATE
	if (aagss1 != aagssa)
	  YYSTACK_FREE (aagss1);
      }
# endif
#endif /* no aagoverflow */

      aagssp = aagss + aagsize - 1;
      aagvsp = aagvs + aagsize - 1;
#if YYLSP_NEEDED
      aaglsp = aagls + aagsize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) aagstacksize));

      if (aagssp >= aagss + aagstacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", aagstate));

  goto aagbackup;


/*-----------.
| aagbackup.  |
`-----------*/
aagbackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* aagresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  aagn = aagpact[aagstate];
  if (aagn == YYFLAG)
    goto aagdefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* aagchar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (aagchar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      aagchar = YYLEX;
    }

  /* Convert token to internal form (in aagchar1) for indexing tables with */

  if (aagchar <= 0)		/* This means end of input. */
    {
      aagchar1 = 0;
      aagchar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      aagchar1 = YYTRANSLATE (aagchar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (aagdebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     aagchar, aagtname[aagchar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, aagchar, aaglval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  aagn += aagchar1;
  if (aagn < 0 || aagn > YYLAST || aagcheck[aagn] != aagchar1)
    goto aagdefault;

  aagn = aagtable[aagn];

  /* aagn is what to do for this token type in this state.
     Negative => reduce, -aagn is rule number.
     Positive => shift, aagn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (aagn < 0)
    {
      if (aagn == YYFLAG)
	goto aagerrlab;
      aagn = -aagn;
      goto aagreduce;
    }
  else if (aagn == 0)
    goto aagerrlab;

  if (aagn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      aagchar, aagtname[aagchar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (aagchar != YYEOF)
    aagchar = YYEMPTY;

  *++aagvsp = aaglval;
#if YYLSP_NEEDED
  *++aaglsp = aaglloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (aagerrstatus)
    aagerrstatus--;

  aagstate = aagn;
  goto aagnewstate;


/*-----------------------------------------------------------.
| aagdefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
aagdefault:
  aagn = aagdefact[aagstate];
  if (aagn == 0)
    goto aagerrlab;
  goto aagreduce;


/*-----------------------------.
| aagreduce -- Do a reduction.  |
`-----------------------------*/
aagreduce:
  /* aagn is the number of a rule to reduce with.  */
  aaglen = aagr2[aagn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  aagval = aagvsp[1-aaglen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  aagloc = aaglsp[1-aaglen];
  YYLLOC_DEFAULT (aagloc, (aaglsp - aaglen), aaglen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (aagdebug)
    {
      int aagi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 aagn, aagrline[aagn]);

      /* Print the symbols being reduced, and their result.  */
      for (aagi = aagprhs[aagn]; aagrhs[aagi] > 0; aagi++)
	YYFPRINTF (stderr, "%s ", aagtname[aagrhs[aagi]]);
      YYFPRINTF (stderr, " -> %s\n", aagtname[aagr1[aagn]]);
    }
#endif

  switch (aagn) {

case 1:
#line 83 "/home/ellson/graphviz/agraph/grammar.y"
{endgraph();}
    break;
case 2:
#line 84 "/home/ellson/graphviz/agraph/grammar.y"
{if (G) {agclose(G); G = Ag_G_global = NIL(Agraph_t*);}}
    break;
case 5:
#line 90 "/home/ellson/graphviz/agraph/grammar.y"
{startgraph(aagvsp[0].str,aagvsp[-1].i,aagvsp[-2].i);}
    break;
case 6:
#line 93 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str=aagvsp[0].str;}
    break;
case 7:
#line 93 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str=0;}
    break;
case 8:
#line 95 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.i=1;}
    break;
case 9:
#line 95 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.i=0;}
    break;
case 10:
#line 97 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.i = 0;}
    break;
case 11:
#line 97 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.i = 1;}
    break;
case 20:
#line 110 "/home/ellson/graphviz/agraph/grammar.y"
{if (aagvsp[-1].i) endedge(); else endnode();}
    break;
case 23:
#line 115 "/home/ellson/graphviz/agraph/grammar.y"
{bufferedges();}
    break;
case 24:
#line 115 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.i = 1;}
    break;
case 25:
#line 116 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.i = 0;}
    break;
case 28:
#line 122 "/home/ellson/graphviz/agraph/grammar.y"
{appendnode(aagvsp[-1].str,aagvsp[0].str);}
    break;
case 29:
#line 125 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str = aagvsp[0].str;}
    break;
case 30:
#line 126 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str = NIL(char*);}
    break;
case 31:
#line 129 "/home/ellson/graphviz/agraph/grammar.y"
{attrstmt(aagvsp[-2].i,aagvsp[-1].str);}
    break;
case 32:
#line 130 "/home/ellson/graphviz/agraph/grammar.y"
{attrstmt(T_graph,NIL(char*));}
    break;
case 33:
#line 133 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.i = T_graph;}
    break;
case 34:
#line 134 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.i = T_node;}
    break;
case 35:
#line 135 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.i = T_edge;}
    break;
case 36:
#line 138 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str = aagvsp[-1].str;}
    break;
case 37:
#line 139 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str = NIL(char*); }
    break;
case 46:
#line 154 "/home/ellson/graphviz/agraph/grammar.y"
{appendattr(aagvsp[-2].str,aagvsp[0].str);}
    break;
case 47:
#line 157 "/home/ellson/graphviz/agraph/grammar.y"
{appendattr(aagvsp[0].str,NIL(char*));}
    break;
case 49:
#line 163 "/home/ellson/graphviz/agraph/grammar.y"
{opensubg(aagvsp[0].str);}
    break;
case 50:
#line 163 "/home/ellson/graphviz/agraph/grammar.y"
{closesubg();}
    break;
case 51:
#line 166 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str=aagvsp[0].str;}
    break;
case 52:
#line 167 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str=NIL(char*);}
    break;
case 53:
#line 168 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str=NIL(char*);}
    break;
case 57:
#line 173 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str = aagvsp[0].str;}
    break;
case 58:
#line 174 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str = aagvsp[0].str;}
    break;
case 59:
#line 177 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str = aagvsp[0].str;}
    break;
case 60:
#line 179 "/home/ellson/graphviz/agraph/grammar.y"
{aagval.str = concat(aagvsp[-2].str,aagvsp[0].str); agstrfree(G,aagvsp[-2].str); agstrfree(G,aagvsp[0].str);}
    break;
}

#line 705 "/usr/share/bison/bison.simple"


  aagvsp -= aaglen;
  aagssp -= aaglen;
#if YYLSP_NEEDED
  aaglsp -= aaglen;
#endif

#if YYDEBUG
  if (aagdebug)
    {
      short *aagssp1 = aagss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (aagssp1 != aagssp)
	YYFPRINTF (stderr, " %d", *++aagssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++aagvsp = aagval;
#if YYLSP_NEEDED
  *++aaglsp = aagloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  aagn = aagr1[aagn];

  aagstate = aagpgoto[aagn - YYNTBASE] + *aagssp;
  if (aagstate >= 0 && aagstate <= YYLAST && aagcheck[aagstate] == *aagssp)
    aagstate = aagtable[aagstate];
  else
    aagstate = aagdefgoto[aagn - YYNTBASE];

  goto aagnewstate;


/*------------------------------------.
| aagerrlab -- here on detecting error |
`------------------------------------*/
aagerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!aagerrstatus)
    {
      ++aagnerrs;

#ifdef YYERROR_VERBOSE
      aagn = aagpact[aagstate];

      if (aagn > YYFLAG && aagn < YYLAST)
	{
	  YYSIZE_T aagsize = 0;
	  char *aagmsg;
	  int aagx, aagcount;

	  aagcount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (aagx = aagn < 0 ? -aagn : 0;
	       aagx < (int) (sizeof (aagtname) / sizeof (char *)); aagx++)
	    if (aagcheck[aagx + aagn] == aagx)
	      aagsize += aagstrlen (aagtname[aagx]) + 15, aagcount++;
	  aagsize += aagstrlen ("parse error, unexpected ") + 1;
	  aagsize += aagstrlen (aagtname[YYTRANSLATE (aagchar)]);
	  aagmsg = (char *) YYSTACK_ALLOC (aagsize);
	  if (aagmsg != 0)
	    {
	      char *aagp = aagstpcpy (aagmsg, "parse error, unexpected ");
	      aagp = aagstpcpy (aagp, aagtname[YYTRANSLATE (aagchar)]);

	      if (aagcount < 5)
		{
		  aagcount = 0;
		  for (aagx = aagn < 0 ? -aagn : 0;
		       aagx < (int) (sizeof (aagtname) / sizeof (char *));
		       aagx++)
		    if (aagcheck[aagx + aagn] == aagx)
		      {
			const char *aagq = ! aagcount ? ", expecting " : " or ";
			aagp = aagstpcpy (aagp, aagq);
			aagp = aagstpcpy (aagp, aagtname[aagx]);
			aagcount++;
		      }
		}
	      aagerror (aagmsg);
	      YYSTACK_FREE (aagmsg);
	    }
	  else
	    aagerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	aagerror ("parse error");
    }
  goto aagerrlab1;


/*--------------------------------------------------.
| aagerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
aagerrlab1:
  if (aagerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (aagchar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  aagchar, aagtname[aagchar1]));
      aagchar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  aagerrstatus = 3;		/* Each real token shifted decrements this */

  goto aagerrhandle;


/*-------------------------------------------------------------------.
| aagerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
aagerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  aagn = aagdefact[aagstate];
  if (aagn)
    goto aagdefault;
#endif


/*---------------------------------------------------------------.
| aagerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
aagerrpop:
  if (aagssp == aagss)
    YYABORT;
  aagvsp--;
  aagstate = *--aagssp;
#if YYLSP_NEEDED
  aaglsp--;
#endif

#if YYDEBUG
  if (aagdebug)
    {
      short *aagssp1 = aagss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (aagssp1 != aagssp)
	YYFPRINTF (stderr, " %d", *++aagssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| aagerrhandle.  |
`--------------*/
aagerrhandle:
  aagn = aagpact[aagstate];
  if (aagn == YYFLAG)
    goto aagerrdefault;

  aagn += YYTERROR;
  if (aagn < 0 || aagn > YYLAST || aagcheck[aagn] != YYTERROR)
    goto aagerrdefault;

  aagn = aagtable[aagn];
  if (aagn < 0)
    {
      if (aagn == YYFLAG)
	goto aagerrpop;
      aagn = -aagn;
      goto aagreduce;
    }
  else if (aagn == 0)
    goto aagerrpop;

  if (aagn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++aagvsp = aaglval;
#if YYLSP_NEEDED
  *++aaglsp = aaglloc;
#endif

  aagstate = aagn;
  goto aagnewstate;


/*-------------------------------------.
| aagacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
aagacceptlab:
  aagresult = 0;
  goto aagreturn;

/*-----------------------------------.
| aagabortlab -- YYABORT comes here.  |
`-----------------------------------*/
aagabortlab:
  aagresult = 1;
  goto aagreturn;

/*---------------------------------------------.
| aagoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
aagoverflowlab:
  aagerror ("parser stack overflow");
  aagresult = 2;
  /* Fall through.  */

aagreturn:
#ifndef aagoverflow
  if (aagss != aagssa)
    YYSTACK_FREE (aagss);
#endif
  return aagresult;
}
#line 181 "/home/ellson/graphviz/agraph/grammar.y"


#define NILitem  NIL(item*)

/* globals */
static	Agraph_t	*Subgraph;	/* most recent subgraph that was opened */
static	Agdisc_t	*Disc;		/* discipline passed to agread or agconcat */
static	list_t	Nodelist,Edgelist,Attrlist;

static item *newitem(int tag, void *p0, char *p1)
{
	item	*rv = agalloc(G,sizeof(item));
	rv->tag = tag; rv->u.name = (char*)p0; rv->str = p1;
	return rv;
}

static item *cons_node(Agnode_t *n, char *port)
	{ return newitem(T_node,n,port); }

static item *cons_attr(char *name, char *value)
	{ return newitem(T_atom,name,value); }

static item *cons_list(item *list)
	{ return newitem(T_list,list,NIL(char*)); }

static item *cons_subg(Agraph_t *subg)
	{ return newitem(T_subgraph,subg,NIL(char*)); }

#ifdef NOTDEF
static item *cons_edge(Agedge_t *e)
	{ return newitem(T_edge,e,NIL(char*)); }
#endif

static void delete_items(item *ilist)
{
	item	*p,*pn;

	for (p = ilist; p; p = pn) {
		pn = p->next;
		switch(p->tag) {
			case T_list: delete_items(p->u.list); break;
			case T_atom: case T_attr: agstrfree(G,p->str); break;
		}
		agfree(G,p);
	}
}

static void deletelist(list_t *list)
{
	delete_items(list->first);
	list->first = list->last = NILitem;
}

#ifdef NOTDEF
static void listins(list_t *list, item *v)
{
	v->next = list->first;
	list->first = v;
	if (list->last == NILitem) list->last = v;
}
#endif

static void listapp(list_t *list, item *v)
{
	if (list->last) list->last->next = v;
	list->last = v;
	if (list->first == NILitem) list->first = v;
}


/* attrs */
static void appendattr(char *name, char *value)
{
	item		*v;

	assert(value != NIL(char*));
	v = cons_attr(name,value);
	listapp(&Attrlist,v);
}

static void bindattrs(int kind)
{
	item		*aptr;
	char		*name;

	for (aptr = Attrlist.first; aptr; aptr = aptr->next) {
		assert(aptr->tag == T_atom);	/* signifies unbound attr */
		name = aptr->u.name;
		if ((kind == AGEDGE) && streq(name,Key)) continue;
		if ((aptr->u.asym = agattr(G,kind,name,NIL(char*))) == NILsym)
			aptr->u.asym = agattr(G,kind,name,"");
		aptr->tag = T_attr;				/* signifies bound attr */
		agstrfree(G,name);
	}
}

static void applyattrs(void *obj)
{
	item		*aptr;

	for (aptr = Attrlist.first; aptr; aptr = aptr->next) {
		if (aptr->tag == T_attr) {
			if (aptr->u.asym) {
				agxset(obj,aptr->u.asym,aptr->str);
			}
		}
		else {
			assert(AGTYPE(obj) == AGEDGE);
			assert(aptr->tag == T_atom);
			assert(streq(aptr->u.name,Key));
		}
	}
}

static void nomacros(void)
{
	agerror(AGERROR_UNIMPL,"attribute macros");
}

static void attrstmt(int tkind, char *macroname)
{
	item			*aptr;
	int				kind;

		/* creating a macro def */
	if (macroname) nomacros();
		/* invoking a macro def */
	for (aptr = Attrlist.first; aptr; aptr = aptr->next)
		if (aptr->str == NIL(char*)) nomacros();

	switch(tkind) {
		case T_graph: kind = AGRAPH; break;
		case T_node: kind = AGNODE; break;
		case T_edge: kind = AGEDGE; break;
		default : abort();
	}
	bindattrs(kind);	/* set up defaults for new attributes */
	for (aptr = Attrlist.first; aptr; aptr = aptr->next)
		agattr(G,kind,aptr->u.asym->name,aptr->str);
	deletelist(&Attrlist);
}

/* nodes */

static void appendnode(char *name, char *port)
{
	item		*elt;

	elt = cons_node(agnode(G,name,TRUE),port);
	listapp(&Nodelist,elt);
	agstrfree(G,name);
}

	/* apply current optional attrs to Nodelist and clean up lists */
static void endnode()
{
	item	*ptr;

	bindattrs(AGNODE);
	for (ptr = Nodelist.first; ptr; ptr = ptr->next)
		applyattrs(ptr->u.n);
	deletelist(&Nodelist);
	deletelist(&Attrlist);
}

/* edges - store up node/subg lists until optional edge key can be seen */

static void bufferedges()
{
	item	*v;

	if (Nodelist.first) {
		v = cons_list(Nodelist.first);
		Nodelist.first = Nodelist.last = NILitem;
	}
	else {v = cons_subg(Subgraph); Subgraph = NIL(Agraph_t*);}
	listapp(&Edgelist,v);
}

static void endedge(void)
{
	char			*key;
	item			*aptr,*tptr,*p;

	Agnode_t		*t;
	Agraph_t		*subg;

	bufferedges();	/* pick up the terminal nodelist or subg */
	bindattrs(AGEDGE);

	/* look for "key" pseudo-attribute */
	key = NIL(char*);
	for (aptr = Attrlist.first; aptr; aptr = aptr->next) {
		if ((aptr->tag == T_atom) && streq(aptr->u.name,Key))
			key = aptr->str;
	}

	/* can make edges with node lists or subgraphs */
	for (p = Edgelist.first; p->next; p = p->next) {
		if (p->tag == T_subgraph) {
			subg = p->u.subg;
			for (t = agfstnode(subg); t; t = agnxtnode(t))
				edgerhs(agsubnode(G,t,FALSE),NIL(char*),p->next,key);
		}
		else {
			for (tptr = p->u.list; tptr; tptr = tptr->next)
				edgerhs(tptr->u.n,tptr->str,p->next,key);
		}
	}
	deletelist(&Edgelist);
	deletelist(&Attrlist);
}

/* concat:
 */
static char*
concat (char* s1, char* s2)
{
  char*  s;
  char   buf[BUFSIZ];
  char*  sym;
  int    len = strlen(s1) + strlen(s2) + 1;

  if (len <= BUFSIZ) sym = buf;
  else sym = (char*)malloc(len);
  strcpy(sym,s1);
  strcat(sym,s2);
  s = agstrdup (G,sym);
  agstrfree (G,s1);
  agstrfree (G,s2);
  if (sym != buf) free (sym);
  return s;
}


static void edgerhs(Agnode_t *tail, char *tport, item *hlist, char *key)
{
	Agnode_t		*head;
	Agraph_t		*subg;
	item			*hptr;

	if (hlist->tag == T_subgraph) {
		subg = hlist->u.subg;
		for (head = agfstnode(subg); head; head = agnxtnode(head))
			newedge(tail,tport,agsubnode(G,head,FALSE),NIL(char*),key);
	}
	else {
		for (hptr = hlist->u.list; hptr; hptr = hptr->next)
			newedge(tail,tport,agsubnode(G,hptr->u.n,FALSE),hptr->str,key);
	}
}

static void mkport(Agedge_t *e, char *name, char *val)
{
	Agsym_t *attr;
	if (val) {
		if ((attr = agattr(G,AGEDGE,name,NIL(char*))) == NILsym)
			attr = agattr(G,AGEDGE,name,"");
		agxset(e,attr,val);
	}
}

static void newedge(Agnode_t *t, char *tport, Agnode_t *h, char *hport, char *key)
{
	Agedge_t 	*e;

	e = agedge(t,h,key,TRUE);
	if (e) {		/* can fail if graph is strict and t==h */
		char    *tp = tport;
		char    *hp = hport;
		if ((agtail(e) != aghead(e)) && (aghead(e) == t)) {
			/* could happen with an undirected edge */
			char    *temp;
			temp = tp; tp = hp; hp = temp;
		}
		mkport(e,"tailport",tp);
		mkport(e,"headport",hp);
		applyattrs(e);
	}
}

/* graphs and subgraphs */


static void startgraph(char *name, int directed, int strict)
{
	static Agdesc_t	req;	/* get rid of warnings */

	if (G == NILgraph) {
		req.directed = directed;
		req.strict = strict;
		req.flatlock = FALSE;
		req.maingraph = TRUE;
		Ag_G_global = G = agopen(name,req,Disc);
	}
	else {
		Ag_G_global = G;
	}
	agstrfree(NIL(Agraph_t*),name);
}

static void endgraph()
{
	aglexeof();
	aginternalmapclearlocalnames(G);
}

static void opensubg(char *name)
{
	G = agsubg(G,name,TRUE);
	agstrfree(G,name);
}

static void closesubg()
{
	Subgraph = G;
	if ((G = agparent(G)) == NIL(Agraph_t*))
		aagerror("libgraph: parser lost root graph\n");
}

extern void *aagin;
Agraph_t *agconcat(Agraph_t *g, void *chan, Agdisc_t *disc)
{
	aagin = chan;
	G = g;
	Ag_G_global = NILgraph;
	Disc = (disc? disc :  &AgDefaultDisc);
	aglexinit(Disc, chan);
	aagparse();
	return Ag_G_global;
}

Agraph_t *agread(void *fp, Agdisc_t *disc) {return agconcat(NILgraph,fp,disc); }
