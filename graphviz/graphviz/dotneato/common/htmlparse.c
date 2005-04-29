/* A Bison parser, made from /home/ellson/graphviz/dotneato/common/htmlparse.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	T_end_br	257
# define	T_row	258
# define	T_end_row	259
# define	T_html	260
# define	T_end_html	261
# define	T_end_table	262
# define	T_end_cell	263
# define	T_string	264
# define	T_error	265
# define	T_BR	266
# define	T_br	267
# define	T_table	268
# define	T_cell	269

#line 1 "/home/ellson/graphviz/dotneato/common/htmlparse.y"

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

#include "render.h"
#include "htmltable.h"
#include "htmllex.h"

extern int htmlparse();

static struct {
  htmllabel_t* lbl;       /* Generated label */
  htmltbl_t*   tblstack;  /* Stack of tables maintained during parsing */
  Dt_t*        lines;     /* Dictionary for lines of text */
  agxbuf*      str;       /* Buffer for text */
} HTMLstate;

/* free_ritem:
 * Free row. This closes and frees row's list, then
 * the pitem itself is freed.
 */
static void
free_ritem(Dt_t* d, pitem* p,Dtdisc_t* ds)
{
  dtclose (p->u.rp);
  free (p);
}

/* free_ritem:
 * Free cell item after cell has been copies into final table.
 * Only the pitem is freed.
 */
static void
free_item(Dt_t* d, pitem* p,Dtdisc_t* ds)
{
  free (p);
}

/* cleanTbl:
 * Clean up table if error in parsing.
 */
static void
cleanTbl (htmltbl_t* tp)
{
  dtclose (tp->u.p.rows);
  free_html_data (&tp->data);
  free (tp);
}

/* cleanCell:
 * Clean up cell if error in parsing.
 */
static void
cleanCell (htmlcell_t* cp)
{
  if (cp->child.kind == HTML_TBL) cleanTbl (cp->child.u.tbl);
  else if (cp->child.kind == HTML_TEXT) free_html_text (cp->child.u.txt);
  free_html_data (&cp->data);
  free (cp);
}

/* free_citem:
 * Free cell item during parsing. This frees cell and pitem.
 */
static void
free_citem(Dt_t* d, pitem* p,Dtdisc_t* ds)
{
  cleanCell (p->u.cp);
  free (p);
}

static Dtdisc_t rowDisc = {
    offsetof(pitem,u),
    sizeof(void*),
    offsetof(pitem,link),
    NIL(Dtmake_f),
    (Dtfree_f)free_ritem,
    NIL(Dtcompar_f),
    NIL(Dthash_f),
    NIL(Dtmemory_f),
    NIL(Dtevent_f)
};
static Dtdisc_t cellDisc = {
    offsetof(pitem,u),
    sizeof(void*),
    offsetof(pitem,link),
    NIL(Dtmake_f),
    (Dtfree_f)free_item,
    NIL(Dtcompar_f),
    NIL(Dthash_f),
    NIL(Dtmemory_f),
    NIL(Dtevent_f)
};

typedef struct {
  Dtlink_t      link;
  const char*   s;          /* line of text */
  char          c;          /* alignment of text */
} sitem;

static void
free_sitem(Dt_t* d,sitem* p,Dtdisc_t* ds)
{
  free (p);
}

static Dtdisc_t strDisc = {
    offsetof(sitem,s),
    sizeof(char*),
    offsetof(sitem,link),
    NIL(Dtmake_f),
    (Dtfree_f)free_sitem,
    NIL(Dtcompar_f),
    NIL(Dthash_f),
    NIL(Dtmemory_f),
    NIL(Dtevent_f)
};

static void
appendStrList(const char* p,int v)
{
  sitem*  sp = NEW(sitem);
  sp->s = strdup(p);
  sp->c = v;
  dtinsert (HTMLstate.lines, sp);
}

/* mkText:
 * Construct htmltext_t from list of lines in HTMLstate.lines.
 * lastl is a last, odd line with no <BR>, so we use n by default.
 */
static htmltxt_t*
mkText (const char* lastl)
{
  int         cnt;
  textline_t* lp;
  sitem*      sp;
  Dt_t*       lines = HTMLstate.lines;
  htmltxt_t* tp = NEW(htmltxt_t);

  if (lines)
    cnt = dtsize (lines);
  else
    cnt = 0;
  if (lastl) cnt++;

  tp->nlines = cnt;
  tp->line = N_NEW(cnt+1,textline_t);

  lp = tp->line;
  if (lines) {
    sp = (sitem*)dtflatten(lines);
    for (; sp; sp = (sitem*)dtlink(lines,(Dtlink_t*)sp)) {
      lp->str = (char*)(sp->s);
      lp->just = sp->c;
      lp++;
    }
  }
  if (lastl) {
    lp->str = strdup(lastl);
    lp->just = 'n';
  }

  dtclear (lines);
  return tp;
}

/* addRow:
 * Add new cell row to current table.
 */
static void
addRow ()
{
  Dt_t*      dp = dtopen(&cellDisc, Dtqueue);
  htmltbl_t* tbl = HTMLstate.tblstack;
  pitem*     sp = NEW(pitem);

  sp->u.rp = dp;
  dtinsert (tbl->u.p.rows, sp);
}

/* setCell:
 * Set cell body and type and attach to row
 */
static void
setCell (htmlcell_t* cp, void* obj, int kind)
{
  pitem*     sp = NEW(pitem);
  htmltbl_t* tbl = HTMLstate.tblstack;
  pitem*     rp = (pitem*)dtlast (tbl->u.p.rows);
  Dt_t*      row = rp->u.rp;

  sp->u.cp = cp;
  dtinsert (row, sp);
  cp->child.kind = kind;
  if (kind == HTML_TEXT)
    cp->child.u.txt = (htmltxt_t*)obj;
  else
    cp->child.u.tbl = (htmltbl_t*)obj;
}

/* mkLabel:
 * Create label, given body and type.
 */
static htmllabel_t*
mkLabel (void* obj, int kind)
{
  htmllabel_t* lp = NEW(htmllabel_t);

  lp->kind = kind;
  if (kind == HTML_TEXT)
    lp->u.txt = (htmltxt_t*)obj;
  else
    lp->u.tbl = (htmltbl_t*)obj;
    
  return lp;
}

/* cleanup:
 * Called on error. Frees resources allocated during parsing.
 * This includes a label, plus a walk down the stack of
 * tables. Note that we use the free_citem function to actually
 * free cells.
 */
static void
cleanup ()
{
  htmltbl_t* tp = HTMLstate.tblstack;
  htmltbl_t* next;

  if (HTMLstate.lbl) {
    free_html_label (HTMLstate.lbl,1);
    HTMLstate.lbl = NULL;
  }
  cellDisc.freef = (Dtfree_f)free_citem;
  while (tp) {
    next = tp->u.p.prev;
    cleanTbl (tp);
    tp = next;
  }
  cellDisc.freef = (Dtfree_f)free_item;
}

/* nonSpace:
 * Return 1 if s contains a non-space character.
 */
static int
nonSpace (char* s)
{
  char   c;

  while ((c = *s++)) {
    if (c != ' ') return 1;
  }
  return 0;
}


#line 268 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
#ifndef YYSTYPE
typedef union  {
  int    i;
  htmltxt_t*  txt;
  htmlcell_t*  cell;
  htmltbl_t*   tbl;
} htmlstype;
# define YYSTYPE htmlstype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		43
#define	YYFLAG		-32768
#define	YYNTBASE	16

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 269 ? htmltranslate[x] : 33)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char htmltranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15
};

#if YYDEBUG
static const short htmlprhs[] =
{
       0,     0,     1,     6,     7,    12,    14,    16,    19,    21,
      24,    28,    31,    33,    35,    38,    39,    46,    48,    49,
      51,    54,    55,    60,    62,    65,    66,    71,    72
};
static const short htmlrhs[] =
{
      -1,     6,    19,    17,     7,     0,     0,     6,    23,    18,
       7,     0,     1,     0,    20,     0,    20,    22,     0,    22,
       0,    22,    21,     0,    20,    22,    21,     0,    13,     3,
       0,    12,     0,    10,     0,    22,    10,     0,     0,    25,
      14,    24,    26,     8,    25,     0,    22,     0,     0,    27,
       0,    26,    27,     0,     0,     4,    28,    29,     5,     0,
      30,     0,    29,    30,     0,     0,    15,    23,    31,     9,
       0,     0,    15,    19,    32,     9,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short htmlrline[] =
{
       0,   289,   289,   290,   290,   291,   294,   296,   298,   302,
     304,   308,   309,   312,   313,   316,   316,   336,   337,   340,
     341,   344,   344,   347,   348,   351,   351,   352,   352
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const htmltname[] =
{
  "$", "error", "$undefined.", "T_end_br", "T_row", "T_end_row", "T_html", 
  "T_end_html", "T_end_table", "T_end_cell", "T_string", "T_error", 
  "T_BR", "T_br", "T_table", "T_cell", "html", "@1", "@2", "text", 
  "lines", "br", "string", "table", "@3", "opt_space", "rows", "row", 
  "@4", "cells", "cell", "@5", "@6", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short htmlr1[] =
{
       0,    17,    16,    18,    16,    16,    19,    19,    19,    20,
      20,    21,    21,    22,    22,    24,    23,    25,    25,    26,
      26,    28,    27,    29,    29,    31,    30,    32,    30
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short htmlr2[] =
{
       0,     0,     4,     0,     4,     1,     1,     2,     1,     2,
       3,     2,     1,     1,     2,     0,     6,     1,     0,     1,
       2,     0,     4,     1,     2,     0,     4,     0,     4
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short htmldefact[] =
{
       0,     5,    18,    13,     1,     6,     8,     3,     0,     0,
       7,    14,    12,     0,     9,     0,    15,     2,    10,    11,
       4,     0,    21,     0,    19,     0,    18,    20,    18,     0,
      23,    17,    16,    27,    25,    22,    24,     0,     0,    28,
      26,     0,     0,     0
};

static const short htmldefgoto[] =
{
      41,     9,    15,     4,     5,    14,     6,     7,    21,     8,
      23,    24,    25,    29,    30,    38,    37
};

static const short htmlpact[] =
{
       1,-32768,    -6,-32768,-32768,    -6,     0,-32768,     2,    10,
      -7,-32768,-32768,     5,-32768,    13,-32768,-32768,-32768,-32768,
  -32768,    14,-32768,    11,-32768,     7,    -6,-32768,    -6,    -4,
  -32768,    -1,-32768,-32768,-32768,-32768,-32768,    15,    16,-32768,
  -32768,    23,    26,-32768
};

static const short htmlpgoto[] =
{
  -32768,-32768,-32768,     3,-32768,    17,    -5,     4,-32768,     8,
  -32768,     6,-32768,-32768,     9,-32768,-32768
};


#define	YYLAST		38


static const short htmltable[] =
{
      10,    35,     1,    11,     3,    12,    13,     2,    19,    11,
      11,    28,    12,    13,   -17,    22,    16,    17,    22,    26,
      20,    31,    28,    42,    39,    40,    43,    18,     0,    27,
       0,    33,    34,     0,    32,     0,     0,     0,    36
};

static const short htmlcheck[] =
{
       5,     5,     1,    10,    10,    12,    13,     6,     3,    10,
      10,    15,    12,    13,    14,     4,    14,     7,     4,     8,
       7,    26,    15,     0,     9,     9,     0,    10,    -1,    23,
      -1,    28,    28,    -1,    26,    -1,    -1,    -1,    29
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

/* All symbols defined below should begin with html or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (htmloverflow) || defined (YYERROR_VERBOSE)

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
#endif /* ! defined (htmloverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (htmloverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union htmlalloc
{
  short htmlss;
  YYSTYPE htmlvs;
# if YYLSP_NEEDED
  YYLTYPE htmlls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union htmlalloc) - 1)

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
	  register YYSIZE_T htmli;		\
	  for (htmli = 0; htmli < (Count); htmli++)	\
	    (To)[htmli] = (From)[htmli];		\
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
	YYSIZE_T htmlnewbytes;						\
	YYCOPY (&htmlptr->Stack, Stack, htmlsize);				\
	Stack = &htmlptr->Stack;						\
	htmlnewbytes = htmlstacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	htmlptr += htmlnewbytes / sizeof (*htmlptr);				\
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

#define htmlerrok		(htmlerrstatus = 0)
#define htmlclearin	(htmlchar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto htmlacceptlab
#define YYABORT 	goto htmlabortlab
#define YYERROR		goto htmlerrlab1
/* Like YYERROR except do call htmlerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto htmlerrlab
#define YYRECOVERING()  (!!htmlerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (htmlchar == YYEMPTY && htmllen == 1)				\
    {								\
      htmlchar = (Token);						\
      htmllval = (Value);						\
      htmlchar1 = YYTRANSLATE (htmlchar);				\
      YYPOPSTACK;						\
      goto htmlbackup;						\
    }								\
  else								\
    { 								\
      htmlerror ("syntax error: cannot back up");			\
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


/* YYLEX -- calling `htmllex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		htmllex (&htmllval, &htmllloc, YYLEX_PARAM)
#  else
#   define YYLEX		htmllex (&htmllval, &htmllloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		htmllex (&htmllval, YYLEX_PARAM)
#  else
#   define YYLEX		htmllex (&htmllval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			htmllex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (htmldebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int htmldebug;
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

# ifndef htmlstrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define htmlstrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
htmlstrlen (const char *htmlstr)
#   else
htmlstrlen (htmlstr)
     const char *htmlstr;
#   endif
{
  register const char *htmls = htmlstr;

  while (*htmls++ != '\0')
    continue;

  return htmls - htmlstr - 1;
}
#  endif
# endif

# ifndef htmlstpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define htmlstpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
htmlstpcpy (char *htmldest, const char *htmlsrc)
#   else
htmlstpcpy (htmldest, htmlsrc)
     char *htmldest;
     const char *htmlsrc;
#   endif
{
  register char *htmld = htmldest;
  register const char *htmls = htmlsrc;

  while ((*htmld++ = *htmls++) != '\0')
    continue;

  return htmld - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into htmlparse.  The argument should have type void *.
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
int htmlparse (void *);
# else
int htmlparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int htmlchar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE htmllval;						\
							\
/* Number of parse errors so far.  */			\
int htmlnerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE htmllloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
htmlparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int htmlstate;
  register int htmln;
  int htmlresult;
  /* Number of tokens to shift before error messages enabled.  */
  int htmlerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int htmlchar1 = 0;

  /* Three stacks and their tools:
     `htmlss': related to states,
     `htmlvs': related to semantic values,
     `htmlls': related to locations.

     Refer to the stacks thru separate pointers, to allow htmloverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	htmlssa[YYINITDEPTH];
  short *htmlss = htmlssa;
  register short *htmlssp;

  /* The semantic value stack.  */
  YYSTYPE htmlvsa[YYINITDEPTH];
  YYSTYPE *htmlvs = htmlvsa;
  register YYSTYPE *htmlvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE htmllsa[YYINITDEPTH];
  YYLTYPE *htmlls = htmllsa;
  YYLTYPE *htmllsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (htmlvsp--, htmlssp--, htmllsp--)
#else
# define YYPOPSTACK   (htmlvsp--, htmlssp--)
#endif

  YYSIZE_T htmlstacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE htmlval;
#if YYLSP_NEEDED
  YYLTYPE htmlloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int htmllen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  htmlstate = 0;
  htmlerrstatus = 0;
  htmlnerrs = 0;
  htmlchar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  htmlssp = htmlss;
  htmlvsp = htmlvs;
#if YYLSP_NEEDED
  htmllsp = htmlls;
#endif
  goto htmlsetstate;

/*------------------------------------------------------------.
| htmlnewstate -- Push a new state, which is found in htmlstate.  |
`------------------------------------------------------------*/
 htmlnewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  htmlssp++;

 htmlsetstate:
  *htmlssp = htmlstate;

  if (htmlssp >= htmlss + htmlstacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T htmlsize = htmlssp - htmlss + 1;

#ifdef htmloverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *htmlvs1 = htmlvs;
	short *htmlss1 = htmlss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *htmlls1 = htmlls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if htmloverflow is a macro.  */
	htmloverflow ("parser stack overflow",
		    &htmlss1, htmlsize * sizeof (*htmlssp),
		    &htmlvs1, htmlsize * sizeof (*htmlvsp),
		    &htmlls1, htmlsize * sizeof (*htmllsp),
		    &htmlstacksize);
	htmlls = htmlls1;
# else
	htmloverflow ("parser stack overflow",
		    &htmlss1, htmlsize * sizeof (*htmlssp),
		    &htmlvs1, htmlsize * sizeof (*htmlvsp),
		    &htmlstacksize);
# endif
	htmlss = htmlss1;
	htmlvs = htmlvs1;
      }
#else /* no htmloverflow */
# ifndef YYSTACK_RELOCATE
      goto htmloverflowlab;
# else
      /* Extend the stack our own way.  */
      if (htmlstacksize >= YYMAXDEPTH)
	goto htmloverflowlab;
      htmlstacksize *= 2;
      if (htmlstacksize > YYMAXDEPTH)
	htmlstacksize = YYMAXDEPTH;

      {
	short *htmlss1 = htmlss;
	union htmlalloc *htmlptr =
	  (union htmlalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (htmlstacksize));
	if (! htmlptr)
	  goto htmloverflowlab;
	YYSTACK_RELOCATE (htmlss);
	YYSTACK_RELOCATE (htmlvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (htmlls);
# endif
# undef YYSTACK_RELOCATE
	if (htmlss1 != htmlssa)
	  YYSTACK_FREE (htmlss1);
      }
# endif
#endif /* no htmloverflow */

      htmlssp = htmlss + htmlsize - 1;
      htmlvsp = htmlvs + htmlsize - 1;
#if YYLSP_NEEDED
      htmllsp = htmlls + htmlsize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) htmlstacksize));

      if (htmlssp >= htmlss + htmlstacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", htmlstate));

  goto htmlbackup;


/*-----------.
| htmlbackup.  |
`-----------*/
htmlbackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* htmlresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  htmln = htmlpact[htmlstate];
  if (htmln == YYFLAG)
    goto htmldefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* htmlchar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (htmlchar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      htmlchar = YYLEX;
    }

  /* Convert token to internal form (in htmlchar1) for indexing tables with */

  if (htmlchar <= 0)		/* This means end of input. */
    {
      htmlchar1 = 0;
      htmlchar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      htmlchar1 = YYTRANSLATE (htmlchar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (htmldebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     htmlchar, htmltname[htmlchar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, htmlchar, htmllval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  htmln += htmlchar1;
  if (htmln < 0 || htmln > YYLAST || htmlcheck[htmln] != htmlchar1)
    goto htmldefault;

  htmln = htmltable[htmln];

  /* htmln is what to do for this token type in this state.
     Negative => reduce, -htmln is rule number.
     Positive => shift, htmln is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (htmln < 0)
    {
      if (htmln == YYFLAG)
	goto htmlerrlab;
      htmln = -htmln;
      goto htmlreduce;
    }
  else if (htmln == 0)
    goto htmlerrlab;

  if (htmln == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      htmlchar, htmltname[htmlchar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (htmlchar != YYEOF)
    htmlchar = YYEMPTY;

  *++htmlvsp = htmllval;
#if YYLSP_NEEDED
  *++htmllsp = htmllloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (htmlerrstatus)
    htmlerrstatus--;

  htmlstate = htmln;
  goto htmlnewstate;


/*-----------------------------------------------------------.
| htmldefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
htmldefault:
  htmln = htmldefact[htmlstate];
  if (htmln == 0)
    goto htmlerrlab;
  goto htmlreduce;


/*-----------------------------.
| htmlreduce -- Do a reduction.  |
`-----------------------------*/
htmlreduce:
  /* htmln is the number of a rule to reduce with.  */
  htmllen = htmlr2[htmln];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  htmlval = htmlvsp[1-htmllen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  htmlloc = htmllsp[1-htmllen];
  YYLLOC_DEFAULT (htmlloc, (htmllsp - htmllen), htmllen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (htmldebug)
    {
      int htmli;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 htmln, htmlrline[htmln]);

      /* Print the symbols being reduced, and their result.  */
      for (htmli = htmlprhs[htmln]; htmlrhs[htmli] > 0; htmli++)
	YYFPRINTF (stderr, "%s ", htmltname[htmlrhs[htmli]]);
      YYFPRINTF (stderr, " -> %s\n", htmltname[htmlr1[htmln]]);
    }
#endif

  switch (htmln) {

case 1:
#line 289 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ HTMLstate.lbl = mkLabel(htmlvsp[0].txt,HTML_TEXT); }
    break;
case 3:
#line 290 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ HTMLstate.lbl = mkLabel(htmlvsp[0].tbl,HTML_TBL); }
    break;
case 5:
#line 291 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ cleanup(); YYABORT; }
    break;
case 6:
#line 295 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ htmlval.txt = mkText (NULL); }
    break;
case 7:
#line 297 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ htmlval.txt = mkText (agxbuse(HTMLstate.str)); }
    break;
case 8:
#line 299 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ htmlval.txt = mkText (agxbuse(HTMLstate.str)); }
    break;
case 9:
#line 303 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ appendStrList (agxbuse(HTMLstate.str),htmlvsp[0].i); }
    break;
case 10:
#line 305 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ appendStrList (agxbuse(HTMLstate.str), htmlvsp[0].i); }
    break;
case 11:
#line 308 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ htmlval.i = htmlvsp[-1].i; }
    break;
case 12:
#line 309 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ htmlval.i = htmlvsp[0].i; }
    break;
case 15:
#line 316 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ 
          if (nonSpace(agxbuse(HTMLstate.str))) {
            htmlerror ("Syntax error: non-space string used before <TABLE>");
            cleanup(); YYABORT;
          }
          htmlvsp[0].tbl->u.p.prev = HTMLstate.tblstack;
          htmlvsp[0].tbl->u.p.rows = dtopen(&rowDisc, Dtqueue);
          HTMLstate.tblstack = htmlvsp[0].tbl;
          htmlval.tbl = htmlvsp[0].tbl;
        }
    break;
case 16:
#line 326 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{
          if (nonSpace(agxbuse(HTMLstate.str))) {
            htmlerror ("Syntax error: non-space string used after </TABLE>");
            cleanup(); YYABORT;
          }
          htmlval.tbl = HTMLstate.tblstack;
          HTMLstate.tblstack = HTMLstate.tblstack->u.p.prev;
        }
    break;
case 21:
#line 344 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ addRow (); }
    break;
case 25:
#line 351 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ setCell(htmlvsp[-1].cell,htmlvsp[0].tbl,HTML_TBL); }
    break;
case 27:
#line 352 "/home/ellson/graphviz/dotneato/common/htmlparse.y"
{ setCell(htmlvsp[-1].cell,htmlvsp[0].txt,HTML_TEXT); }
    break;
}

#line 705 "/usr/share/bison/bison.simple"


  htmlvsp -= htmllen;
  htmlssp -= htmllen;
#if YYLSP_NEEDED
  htmllsp -= htmllen;
#endif

#if YYDEBUG
  if (htmldebug)
    {
      short *htmlssp1 = htmlss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (htmlssp1 != htmlssp)
	YYFPRINTF (stderr, " %d", *++htmlssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++htmlvsp = htmlval;
#if YYLSP_NEEDED
  *++htmllsp = htmlloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  htmln = htmlr1[htmln];

  htmlstate = htmlpgoto[htmln - YYNTBASE] + *htmlssp;
  if (htmlstate >= 0 && htmlstate <= YYLAST && htmlcheck[htmlstate] == *htmlssp)
    htmlstate = htmltable[htmlstate];
  else
    htmlstate = htmldefgoto[htmln - YYNTBASE];

  goto htmlnewstate;


/*------------------------------------.
| htmlerrlab -- here on detecting error |
`------------------------------------*/
htmlerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!htmlerrstatus)
    {
      ++htmlnerrs;

#ifdef YYERROR_VERBOSE
      htmln = htmlpact[htmlstate];

      if (htmln > YYFLAG && htmln < YYLAST)
	{
	  YYSIZE_T htmlsize = 0;
	  char *htmlmsg;
	  int htmlx, htmlcount;

	  htmlcount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (htmlx = htmln < 0 ? -htmln : 0;
	       htmlx < (int) (sizeof (htmltname) / sizeof (char *)); htmlx++)
	    if (htmlcheck[htmlx + htmln] == htmlx)
	      htmlsize += htmlstrlen (htmltname[htmlx]) + 15, htmlcount++;
	  htmlsize += htmlstrlen ("parse error, unexpected ") + 1;
	  htmlsize += htmlstrlen (htmltname[YYTRANSLATE (htmlchar)]);
	  htmlmsg = (char *) YYSTACK_ALLOC (htmlsize);
	  if (htmlmsg != 0)
	    {
	      char *htmlp = htmlstpcpy (htmlmsg, "parse error, unexpected ");
	      htmlp = htmlstpcpy (htmlp, htmltname[YYTRANSLATE (htmlchar)]);

	      if (htmlcount < 5)
		{
		  htmlcount = 0;
		  for (htmlx = htmln < 0 ? -htmln : 0;
		       htmlx < (int) (sizeof (htmltname) / sizeof (char *));
		       htmlx++)
		    if (htmlcheck[htmlx + htmln] == htmlx)
		      {
			const char *htmlq = ! htmlcount ? ", expecting " : " or ";
			htmlp = htmlstpcpy (htmlp, htmlq);
			htmlp = htmlstpcpy (htmlp, htmltname[htmlx]);
			htmlcount++;
		      }
		}
	      htmlerror (htmlmsg);
	      YYSTACK_FREE (htmlmsg);
	    }
	  else
	    htmlerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	htmlerror ("parse error");
    }
  goto htmlerrlab1;


/*--------------------------------------------------.
| htmlerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
htmlerrlab1:
  if (htmlerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (htmlchar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  htmlchar, htmltname[htmlchar1]));
      htmlchar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  htmlerrstatus = 3;		/* Each real token shifted decrements this */

  goto htmlerrhandle;


/*-------------------------------------------------------------------.
| htmlerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
htmlerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  htmln = htmldefact[htmlstate];
  if (htmln)
    goto htmldefault;
#endif


/*---------------------------------------------------------------.
| htmlerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
htmlerrpop:
  if (htmlssp == htmlss)
    YYABORT;
  htmlvsp--;
  htmlstate = *--htmlssp;
#if YYLSP_NEEDED
  htmllsp--;
#endif

#if YYDEBUG
  if (htmldebug)
    {
      short *htmlssp1 = htmlss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (htmlssp1 != htmlssp)
	YYFPRINTF (stderr, " %d", *++htmlssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| htmlerrhandle.  |
`--------------*/
htmlerrhandle:
  htmln = htmlpact[htmlstate];
  if (htmln == YYFLAG)
    goto htmlerrdefault;

  htmln += YYTERROR;
  if (htmln < 0 || htmln > YYLAST || htmlcheck[htmln] != YYTERROR)
    goto htmlerrdefault;

  htmln = htmltable[htmln];
  if (htmln < 0)
    {
      if (htmln == YYFLAG)
	goto htmlerrpop;
      htmln = -htmln;
      goto htmlreduce;
    }
  else if (htmln == 0)
    goto htmlerrpop;

  if (htmln == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++htmlvsp = htmllval;
#if YYLSP_NEEDED
  *++htmllsp = htmllloc;
#endif

  htmlstate = htmln;
  goto htmlnewstate;


/*-------------------------------------.
| htmlacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
htmlacceptlab:
  htmlresult = 0;
  goto htmlreturn;

/*-----------------------------------.
| htmlabortlab -- YYABORT comes here.  |
`-----------------------------------*/
htmlabortlab:
  htmlresult = 1;
  goto htmlreturn;

/*---------------------------------------------.
| htmloverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
htmloverflowlab:
  htmlerror ("parser stack overflow");
  htmlresult = 2;
  /* Fall through.  */

htmlreturn:
#ifndef htmloverflow
  if (htmlss != htmlssa)
    YYSTACK_FREE (htmlss);
#endif
  return htmlresult;
}
#line 355 "/home/ellson/graphviz/dotneato/common/htmlparse.y"


htmllabel_t*
parseHTML (char* txt, int* warn)
{
  unsigned char buf[SMALLBUF];
  agxbuf        str;

  HTMLstate.tblstack = 0;
  HTMLstate.lbl = 0;
  HTMLstate.lines = dtopen(&strDisc, Dtqueue);
  agxbinit (&str, SMALLBUF, buf);
  HTMLstate.str = &str;
  
  initHTMLlexer (txt, &str);
  htmlparse();
  *warn = clearHTMLlexer ();

  dtclose (HTMLstate.lines);
  agxbfree (&str);

  return HTMLstate.lbl;
}

