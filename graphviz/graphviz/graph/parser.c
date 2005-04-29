/* A Bison parser, made from /home/ellson/graphviz/graph/parser.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	T_graph	257
# define	T_digraph	258
# define	T_strict	259
# define	T_node	260
# define	T_edge	261
# define	T_edgeop	262
# define	T_symbol	263
# define	T_qsymbol	264
# define	T_subgraph	265

#line 1 "/home/ellson/graphviz/graph/parser.y"

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

#include	"libgraph.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

static char		Port[SMALLBUF],*Symbol;
static char		In_decl,In_edge_stmt;
static int		Current_class,Agraph_type;
static Agraph_t		*G;
static Agnode_t		*N;
static Agedge_t		*E;
static objstack_t	*SP;
static Agraph_t		*Gstack[32];
static int			GSP;
static int			override;

/* agoverride:
 * If override == 1, initial attr_stmt is ignored if the attribute is
 *                   already defined.
 */
void agoverride(int ov)
{
  override = (ov > 0 ? ov : 0);
}

static void push_subg(Agraph_t *g)
{
	G = Gstack[GSP++] = g;
}

static Agraph_t *pop_subg(void)
{
	Agraph_t		*g;
	if (GSP == 0) {
		agerr (AGERR, "Gstack underflow in graph parser\n"); exit(1);
	}
	g = Gstack[--GSP];					/* graph being popped off */
	if (GSP > 0) G = Gstack[GSP - 1];	/* current graph */
	else G = 0;
	return g;
}

static objport_t pop_gobj(void)
{
	objport_t	rv;
	rv.obj = pop_subg();
	rv.port = NULL;
	return rv;
}

static void anonname(char* buf)
{
	static int		anon_id = 0;

	sprintf(buf,"_anonymous_%d",anon_id++);
}

static void begin_graph(char *name)
{
	Agraph_t		*g;
	char			buf[SMALLBUF];

	if (!name) {
		anonname(buf);
		name = buf;
    }
	g = AG.parsed_g = agopen(name,Agraph_type);
	Current_class = TAG_GRAPH;
	push_subg(g);
	In_decl = TRUE;
}

static void end_graph(void)
{
	pop_subg();
}

static Agnode_t *bind_node(char *name)
{
	Agnode_t	*n = agnode(G,name);
	In_decl = FALSE;
	return n;
}

static void anonsubg(void)
{
	char			buf[SMALLBUF];
	Agraph_t			*subg;

	In_decl = FALSE;
	anonname(buf);
	subg = agsubg(G,buf);
	push_subg(subg);
}

#if 0 /* NOT USED */
static int isanonsubg(Agraph_t *g)
{
	return (strncmp("_anonymous_",g->name,11) == 0);
}
#endif

static void begin_edgestmt(objport_t objp)
{
	struct objstack_t	*new_sp;

	new_sp = NEW(objstack_t);
	new_sp->link = SP;
	SP = new_sp;
	SP->list = SP->last = NEW(objlist_t);
	SP->list->data  = objp;
	SP->list->link = NULL;
	SP->in_edge_stmt = In_edge_stmt;
	SP->subg = G;
	agpushproto(G);
	In_edge_stmt = TRUE;
}

static void mid_edgestmt(objport_t objp)
{
	SP->last->link = NEW(objlist_t);
	SP->last = SP->last->link;
	SP->last->data = objp;
	SP->last->link = NULL;
}

static void end_edgestmt(void)
{
	objstack_t	*old_SP;
	objlist_t	*tailptr,*headptr,*freeptr;
	Agraph_t		*t_graph,*h_graph;
	Agnode_t	*t_node,*h_node,*t_first,*h_first;
	Agedge_t	*e;
	char		*tport,*hport;

	for (tailptr = SP->list; tailptr->link; tailptr = tailptr->link) {
		headptr = tailptr->link;
		tport = tailptr->data.port;
		hport = headptr->data.port;
		if (TAG_OF(tailptr->data.obj) == TAG_NODE) {
			t_graph = NULL;
			t_first = (Agnode_t*)(tailptr->data.obj);
		}
		else {
			t_graph = (Agraph_t*)(tailptr->data.obj);
			t_first = agfstnode(t_graph);
		}
		if (TAG_OF(headptr->data.obj) == TAG_NODE) {
			h_graph = NULL;
			h_first = (Agnode_t*)(headptr->data.obj);
		}
		else {
			h_graph = (Agraph_t*)(headptr->data.obj);
			h_first = agfstnode(h_graph);
		}

		for (t_node = t_first; t_node; t_node = t_graph ?
		  agnxtnode(t_graph,t_node) : NULL) {
			for (h_node = h_first; h_node; h_node = h_graph ?
			  agnxtnode(h_graph,h_node) : NULL ) {
				e = agedge(G,t_node,h_node);
				if (e) {
					char	*tp = tport;
					char 	*hp = hport;
					if ((e->tail != e->head) && (e->head == t_node)) {
						/* could happen with an undirected edge */
						char 	*temp;
						temp = tp; tp = hp; hp = temp;
					}
					if (tp && tp[0]) agxset(e,TAILX,tp);
					if (hp && hp[0]) agxset(e,HEADX,hp);
				}
			}
		}
	}
	tailptr = SP->list; 
	while (tailptr) {
		freeptr = tailptr;
		tailptr = tailptr->link;
		if (TAG_OF(freeptr->data.obj) == TAG_NODE)
		free(freeptr->data.port);
		free(freeptr);
	}
	if (G != SP->subg) abort();
	agpopproto(G);
	In_edge_stmt = SP->in_edge_stmt;
	old_SP = SP;
	SP = SP->link;
	In_decl = FALSE;
	free(old_SP);
	Current_class = TAG_GRAPH;
}

#if 0 /* NOT USED */
static Agraph_t *parent_of(Agraph_t *g)
{
	Agraph_t		*rv;
	rv = agusergraph(agfstin(g->meta_node->graph,g->meta_node)->tail);
	return rv;
}
#endif

static void attr_set(char *name, char *value)
{
	Agsym_t		*ap = NULL;
	char		*defval = "";

	if (In_decl && (G->root == G)) defval = value;
	switch (Current_class) {
		case TAG_NODE:
			ap = agfindattr(G->proto->n,name);
			if (ap == NULL)
				ap = agnodeattr(AG.parsed_g,name,defval);
            else if (override && In_decl)
              return;
			agxset(N,ap->index,value);
			break;
		case TAG_EDGE:
			ap = agfindattr(G->proto->e,name);
			if (ap == NULL)
				ap = agedgeattr(AG.parsed_g,name,defval);
            else if (override && In_decl)
              return;
			agxset(E,ap->index,value);
			break;
		case 0:		/* default */
		case TAG_GRAPH:
			ap = agfindattr(G,name);
			if (ap == NULL) 
				ap = agraphattr(AG.parsed_g,name,defval);
            else if (override && In_decl)
              return;
			agxset(G,ap->index,value);
			break;
	}
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
  s = agstrdup (sym);
  if (sym != buf) free (sym);
  return s;
}


#line 272 "/home/ellson/graphviz/graph/parser.y"
#ifndef YYSTYPE
typedef union	{
			int					i;
			char				*str;
			struct objport_t	obj;
			struct Agnode_t		*n;
} agstype;
# define YYSTYPE agstype
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		100
#define	YYFLAG		-32768
#define	YYNTBASE	24

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 265 ? agtranslate[x] : 63)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char agtranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      20,    21,     2,    23,    14,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    19,    18,
       2,    17,     2,     2,    22,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    15,     2,    16,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    12,     2,    13,     2,     2,     2,     2,
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
       6,     7,     8,     9,    10,    11
};

#if YYDEBUG
static const short agprhs[] =
{
       0,     0,     1,     8,    10,    11,    13,    14,    16,    19,
      21,    24,    26,    28,    30,    34,    35,    36,    38,    42,
      45,    46,    48,    52,    54,    56,    58,    59,    61,    64,
      66,    69,    71,    73,    75,    77,    79,    82,    84,    87,
      89,    90,    92,    94,    97,   100,   103,   104,   112,   115,
     116,   120,   121,   122,   128,   129,   130,   136,   139,   140,
     145,   148,   149,   154,   159,   160,   166,   167,   172,   174,
     177,   179,   181,   183
};
static const short agrhs[] =
{
      -1,    27,    26,    25,    12,    36,    13,     0,     1,     0,
       0,    61,     0,     0,     3,     0,     5,     3,     0,     4,
       0,     5,     4,     0,     3,     0,     6,     0,     7,     0,
      35,    30,    29,     0,     0,     0,    14,     0,    15,    29,
      16,     0,    32,    31,     0,     0,    32,     0,    61,    17,
      61,     0,    34,     0,    61,     0,    37,     0,     0,    38,
       0,    37,    38,     0,    39,     0,    39,    18,     0,     1,
       0,    47,     0,    49,     0,    40,     0,    57,     0,    28,
      31,     0,    34,     0,    42,    43,     0,    61,     0,     0,
      44,     0,    46,     0,    46,    44,     0,    44,    46,     0,
      19,    61,     0,     0,    19,    20,    61,    45,    14,    61,
      21,     0,    22,    61,     0,     0,    41,    48,    33,     0,
       0,     0,    41,    50,    54,    51,    33,     0,     0,     0,
      57,    52,    54,    53,    33,     0,     8,    41,     0,     0,
       8,    41,    55,    54,     0,     8,    57,     0,     0,     8,
      57,    56,    54,     0,    60,    12,    36,    13,     0,     0,
      11,    12,    58,    36,    13,     0,     0,    12,    59,    36,
      13,     0,    60,     0,    11,    61,     0,     9,     0,    62,
       0,    10,     0,    62,    23,    10,     0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short agrline[] =
{
       0,   289,   289,   293,   300,   303,   303,   306,   308,   310,
     312,   316,   318,   320,   324,   325,   328,   329,   332,   335,
     336,   339,   342,   346,   347,   351,   352,   355,   356,   359,
     360,   361,   364,   365,   366,   367,   370,   372,   376,   386,
     389,   390,   391,   392,   393,   396,   397,   397,   404,   411,
     411,   417,   417,   417,   424,   424,   424,   433,   434,   434,
     437,   439,   439,   445,   446,   446,   447,   447,   448,   451,
     461,   462,   465,   466
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const agtname[] =
{
  "$", "error", "$undefined.", "T_graph", "T_digraph", "T_strict", "T_node", 
  "T_edge", "T_edgeop", "T_symbol", "T_qsymbol", "T_subgraph", "'{'", 
  "'}'", "','", "'['", "']'", "'='", "';'", "':'", "'('", "')'", "'@'", 
  "'+'", "file", "@1", "optgraphname", "graph_type", "attr_class", 
  "inside_attr_list", "optcomma", "attr_list", "rec_attr_list", 
  "opt_attr_list", "attr_set", "iattr_set", "stmt_list", "stmt_list1", 
  "stmt", "stmt1", "attr_stmt", "node_id", "node_name", "node_port", 
  "port_location", "@2", "port_angle", "node_stmt", "@3", "edge_stmt", 
  "@4", "@5", "@6", "@7", "edgeRHS", "@8", "@9", "subg_stmt", "@10", 
  "@11", "subg_hdr", "symbol", "qsymbol", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short agr1[] =
{
       0,    25,    24,    24,    24,    26,    26,    27,    27,    27,
      27,    28,    28,    28,    29,    29,    30,    30,    31,    32,
      32,    33,    34,    35,    35,    36,    36,    37,    37,    38,
      38,    38,    39,    39,    39,    39,    40,    40,    41,    42,
      43,    43,    43,    43,    43,    44,    45,    44,    46,    48,
      47,    50,    51,    49,    52,    53,    49,    54,    55,    54,
      54,    56,    54,    57,    58,    57,    59,    57,    57,    60,
      61,    61,    62,    62
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short agr2[] =
{
       0,     0,     6,     1,     0,     1,     0,     1,     2,     1,
       2,     1,     1,     1,     3,     0,     0,     1,     3,     2,
       0,     1,     3,     1,     1,     1,     0,     1,     2,     1,
       2,     1,     1,     1,     1,     1,     2,     1,     2,     1,
       0,     1,     1,     2,     2,     2,     0,     7,     2,     0,
       3,     0,     0,     5,     0,     0,     5,     2,     0,     4,
       2,     0,     4,     4,     0,     5,     0,     4,     1,     2,
       1,     1,     1,     3
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short agdefact[] =
{
       0,     3,     7,     9,     0,     6,     8,    10,    70,    72,
       1,     5,    71,     0,     0,     0,    73,    31,    11,    12,
      13,     0,    66,     0,    37,     0,     0,    27,    29,    34,
      49,    40,    32,    33,    35,    68,    39,    64,    69,     0,
      15,    36,     2,    28,    30,    20,     0,     0,     0,    38,
      41,    42,     0,     0,     0,     0,     0,     0,    23,    16,
      24,    21,    50,     0,    52,     0,    45,    48,    44,    43,
      55,     0,    22,     0,    67,    18,    17,    15,    19,    57,
      60,    39,    20,    46,    20,    63,    65,    14,     0,     0,
      53,     0,    56,    59,    62,     0,     0,    47,     0,     0,
       0
};

static const short agdefgoto[] =
{
      98,    13,    10,     5,    23,    57,    77,    41,    61,    62,
      24,    59,    25,    26,    27,    28,    29,    30,    31,    49,
      50,    91,    51,    32,    45,    33,    46,    82,    52,    84,
      64,    88,    89,    34,    55,    39,    35,    36,    12
};

static const short agpact[] =
{
       4,-32768,-32768,-32768,    37,    53,-32768,-32768,-32768,-32768,
  -32768,-32768,   -17,    10,    15,    17,-32768,-32768,-32768,-32768,
  -32768,    22,-32768,    29,-32768,    33,    44,-32768,    30,-32768,
      56,    14,-32768,-32768,    57,    40,    49,-32768,-32768,    17,
      53,-32768,-32768,-32768,-32768,-32768,    59,     1,    53,-32768,
      46,    50,    59,    17,    53,    17,    58,    54,-32768,    60,
      49,    29,-32768,     3,-32768,    53,-32768,-32768,-32768,-32768,
  -32768,    62,-32768,    63,-32768,-32768,-32768,    53,-32768,    65,
      69,-32768,-32768,-32768,-32768,-32768,-32768,-32768,    59,    59,
  -32768,    64,-32768,-32768,-32768,    53,    61,-32768,    79,    80,
  -32768
};

static const short agpgoto[] =
{
  -32768,-32768,-32768,-32768,-32768,     6,-32768,    20,-32768,   -23,
     -38,-32768,   -36,-32768,    66,-32768,-32768,    21,-32768,-32768,
      34,-32768,    36,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
     -51,-32768,-32768,    24,-32768,-32768,-32768,    -5,-32768
};


#define	YYLAST		92


static const short agtable[] =
{
      11,    70,    58,    56,    -4,     1,    14,     2,     3,     4,
       8,     9,     8,     9,    21,    22,    38,    71,    17,    73,
      18,    65,    15,    19,    20,    16,     8,     9,    21,    22,
     -26,     8,     9,    47,    37,    60,    48,    93,    94,    58,
       6,     7,    66,    67,    40,    17,    42,    18,    44,    72,
      19,    20,    53,     8,     9,    21,    22,   -25,    81,    90,
      83,    92,     8,     9,   -51,   -54,    54,    63,    48,    47,
      75,    74,    60,   -58,    76,    85,    86,   -61,    95,    99,
     100,    78,    97,    87,    79,    69,    68,    80,     0,     0,
      96,     0,    43
};

static const short agcheck[] =
{
       5,    52,    40,    39,     0,     1,    23,     3,     4,     5,
       9,    10,     9,    10,    11,    12,    21,    53,     1,    55,
       3,    20,    12,     6,     7,    10,     9,    10,    11,    12,
      13,     9,    10,    19,    12,    40,    22,    88,    89,    77,
       3,     4,    47,    48,    15,     1,    13,     3,    18,    54,
       6,     7,    12,     9,    10,    11,    12,    13,    63,    82,
      65,    84,     9,    10,     8,     8,    17,     8,    22,    19,
      16,    13,    77,     8,    14,    13,    13,     8,    14,     0,
       0,    61,    21,    77,    63,    51,    50,    63,    -1,    -1,
      95,    -1,    26
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

/* All symbols defined below should begin with ag or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (agoverflow) || defined (YYERROR_VERBOSE)

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
#endif /* ! defined (agoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (agoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union agalloc
{
  short agss;
  YYSTYPE agvs;
# if YYLSP_NEEDED
  YYLTYPE agls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union agalloc) - 1)

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
	  register YYSIZE_T agi;		\
	  for (agi = 0; agi < (Count); agi++)	\
	    (To)[agi] = (From)[agi];		\
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
	YYSIZE_T agnewbytes;						\
	YYCOPY (&agptr->Stack, Stack, agsize);				\
	Stack = &agptr->Stack;						\
	agnewbytes = agstacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	agptr += agnewbytes / sizeof (*agptr);				\
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

#define agerrok		(agerrstatus = 0)
#define agclearin	(agchar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto agacceptlab
#define YYABORT 	goto agabortlab
#define YYERROR		goto agerrlab1
/* Like YYERROR except do call agerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto agerrlab
#define YYRECOVERING()  (!!agerrstatus)
#define YYBACKUP(Token, Value)					\
do								\
  if (agchar == YYEMPTY && aglen == 1)				\
    {								\
      agchar = (Token);						\
      aglval = (Value);						\
      agchar1 = YYTRANSLATE (agchar);				\
      YYPOPSTACK;						\
      goto agbackup;						\
    }								\
  else								\
    { 								\
      agerror ("syntax error: cannot back up");			\
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


/* YYLEX -- calling `aglex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		aglex (&aglval, &aglloc, YYLEX_PARAM)
#  else
#   define YYLEX		aglex (&aglval, &aglloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		aglex (&aglval, YYLEX_PARAM)
#  else
#   define YYLEX		aglex (&aglval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			aglex ()
#endif /* !YYPURE */


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (agdebug)					\
    YYFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int agdebug;
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

# ifndef agstrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define agstrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
agstrlen (const char *agstr)
#   else
agstrlen (agstr)
     const char *agstr;
#   endif
{
  register const char *ags = agstr;

  while (*ags++ != '\0')
    continue;

  return ags - agstr - 1;
}
#  endif
# endif

# ifndef agstpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define agstpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
agstpcpy (char *agdest, const char *agsrc)
#   else
agstpcpy (agdest, agsrc)
     char *agdest;
     const char *agsrc;
#   endif
{
  register char *agd = agdest;
  register const char *ags = agsrc;

  while ((*agd++ = *ags++) != '\0')
    continue;

  return agd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into agparse.  The argument should have type void *.
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
int agparse (void *);
# else
int agparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int agchar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE aglval;						\
							\
/* Number of parse errors so far.  */			\
int agnerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE aglloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
agparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int agstate;
  register int agn;
  int agresult;
  /* Number of tokens to shift before error messages enabled.  */
  int agerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int agchar1 = 0;

  /* Three stacks and their tools:
     `agss': related to states,
     `agvs': related to semantic values,
     `agls': related to locations.

     Refer to the stacks thru separate pointers, to allow agoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	agssa[YYINITDEPTH];
  short *agss = agssa;
  register short *agssp;

  /* The semantic value stack.  */
  YYSTYPE agvsa[YYINITDEPTH];
  YYSTYPE *agvs = agvsa;
  register YYSTYPE *agvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE aglsa[YYINITDEPTH];
  YYLTYPE *agls = aglsa;
  YYLTYPE *aglsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (agvsp--, agssp--, aglsp--)
#else
# define YYPOPSTACK   (agvsp--, agssp--)
#endif

  YYSIZE_T agstacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE agval;
#if YYLSP_NEEDED
  YYLTYPE agloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int aglen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  agstate = 0;
  agerrstatus = 0;
  agnerrs = 0;
  agchar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  agssp = agss;
  agvsp = agvs;
#if YYLSP_NEEDED
  aglsp = agls;
#endif
  goto agsetstate;

/*------------------------------------------------------------.
| agnewstate -- Push a new state, which is found in agstate.  |
`------------------------------------------------------------*/
 agnewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  agssp++;

 agsetstate:
  *agssp = agstate;

  if (agssp >= agss + agstacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T agsize = agssp - agss + 1;

#ifdef agoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *agvs1 = agvs;
	short *agss1 = agss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *agls1 = agls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if agoverflow is a macro.  */
	agoverflow ("parser stack overflow",
		    &agss1, agsize * sizeof (*agssp),
		    &agvs1, agsize * sizeof (*agvsp),
		    &agls1, agsize * sizeof (*aglsp),
		    &agstacksize);
	agls = agls1;
# else
	agoverflow ("parser stack overflow",
		    &agss1, agsize * sizeof (*agssp),
		    &agvs1, agsize * sizeof (*agvsp),
		    &agstacksize);
# endif
	agss = agss1;
	agvs = agvs1;
      }
#else /* no agoverflow */
# ifndef YYSTACK_RELOCATE
      goto agoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (agstacksize >= YYMAXDEPTH)
	goto agoverflowlab;
      agstacksize *= 2;
      if (agstacksize > YYMAXDEPTH)
	agstacksize = YYMAXDEPTH;

      {
	short *agss1 = agss;
	union agalloc *agptr =
	  (union agalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (agstacksize));
	if (! agptr)
	  goto agoverflowlab;
	YYSTACK_RELOCATE (agss);
	YYSTACK_RELOCATE (agvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (agls);
# endif
# undef YYSTACK_RELOCATE
	if (agss1 != agssa)
	  YYSTACK_FREE (agss1);
      }
# endif
#endif /* no agoverflow */

      agssp = agss + agsize - 1;
      agvsp = agvs + agsize - 1;
#if YYLSP_NEEDED
      aglsp = agls + agsize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) agstacksize));

      if (agssp >= agss + agstacksize - 1)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", agstate));

  goto agbackup;


/*-----------.
| agbackup.  |
`-----------*/
agbackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* agresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  agn = agpact[agstate];
  if (agn == YYFLAG)
    goto agdefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* agchar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (agchar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      agchar = YYLEX;
    }

  /* Convert token to internal form (in agchar1) for indexing tables with */

  if (agchar <= 0)		/* This means end of input. */
    {
      agchar1 = 0;
      agchar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      agchar1 = YYTRANSLATE (agchar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (agdebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     agchar, agtname[agchar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, agchar, aglval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  agn += agchar1;
  if (agn < 0 || agn > YYLAST || agcheck[agn] != agchar1)
    goto agdefault;

  agn = agtable[agn];

  /* agn is what to do for this token type in this state.
     Negative => reduce, -agn is rule number.
     Positive => shift, agn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (agn < 0)
    {
      if (agn == YYFLAG)
	goto agerrlab;
      agn = -agn;
      goto agreduce;
    }
  else if (agn == 0)
    goto agerrlab;

  if (agn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      agchar, agtname[agchar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (agchar != YYEOF)
    agchar = YYEMPTY;

  *++agvsp = aglval;
#if YYLSP_NEEDED
  *++aglsp = aglloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (agerrstatus)
    agerrstatus--;

  agstate = agn;
  goto agnewstate;


/*-----------------------------------------------------------.
| agdefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
agdefault:
  agn = agdefact[agstate];
  if (agn == 0)
    goto agerrlab;
  goto agreduce;


/*-----------------------------.
| agreduce -- Do a reduction.  |
`-----------------------------*/
agreduce:
  /* agn is the number of a rule to reduce with.  */
  aglen = agr2[agn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  agval = agvsp[1-aglen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  agloc = aglsp[1-aglen];
  YYLLOC_DEFAULT (agloc, (aglsp - aglen), aglen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (agdebug)
    {
      int agi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 agn, agrline[agn]);

      /* Print the symbols being reduced, and their result.  */
      for (agi = agprhs[agn]; agrhs[agi] > 0; agi++)
	YYFPRINTF (stderr, "%s ", agtname[agrhs[agi]]);
      YYFPRINTF (stderr, " -> %s\n", agtname[agr1[agn]]);
    }
#endif

  switch (agn) {

case 1:
#line 290 "/home/ellson/graphviz/graph/parser.y"
{begin_graph(agvsp[0].str); agstrfree(agvsp[0].str);}
    break;
case 2:
#line 292 "/home/ellson/graphviz/graph/parser.y"
{AG.accepting_state = TRUE; end_graph();}
    break;
case 3:
#line 294 "/home/ellson/graphviz/graph/parser.y"
{
					if (AG.parsed_g)
						agclose(AG.parsed_g);
					AG.parsed_g = NULL;
					/*exit(1);*/
				}
    break;
case 4:
#line 300 "/home/ellson/graphviz/graph/parser.y"
{AG.parsed_g = NULL;}
    break;
case 5:
#line 303 "/home/ellson/graphviz/graph/parser.y"
{agval.str=agvsp[0].str;}
    break;
case 6:
#line 303 "/home/ellson/graphviz/graph/parser.y"
{agval.str=0;}
    break;
case 7:
#line 307 "/home/ellson/graphviz/graph/parser.y"
{Agraph_type = AGRAPH; AG.edge_op = "--";}
    break;
case 8:
#line 309 "/home/ellson/graphviz/graph/parser.y"
{Agraph_type = AGRAPHSTRICT; AG.edge_op = "--";}
    break;
case 9:
#line 311 "/home/ellson/graphviz/graph/parser.y"
{Agraph_type = AGDIGRAPH; AG.edge_op = "->";}
    break;
case 10:
#line 313 "/home/ellson/graphviz/graph/parser.y"
{Agraph_type = AGDIGRAPHSTRICT; AG.edge_op = "->";}
    break;
case 11:
#line 317 "/home/ellson/graphviz/graph/parser.y"
{Current_class = TAG_GRAPH;}
    break;
case 12:
#line 319 "/home/ellson/graphviz/graph/parser.y"
{Current_class = TAG_NODE; N = G->proto->n;}
    break;
case 13:
#line 321 "/home/ellson/graphviz/graph/parser.y"
{Current_class = TAG_EDGE; E = G->proto->e;}
    break;
case 22:
#line 343 "/home/ellson/graphviz/graph/parser.y"
{attr_set(agvsp[-2].str,agvsp[0].str); agstrfree(agvsp[-2].str); agstrfree(agvsp[0].str);}
    break;
case 24:
#line 348 "/home/ellson/graphviz/graph/parser.y"
{attr_set(agvsp[0].str,"true"); agstrfree(agvsp[0].str); }
    break;
case 31:
#line 361 "/home/ellson/graphviz/graph/parser.y"
{agerror("syntax error, statement skipped");}
    break;
case 35:
#line 367 "/home/ellson/graphviz/graph/parser.y"
{}
    break;
case 36:
#line 371 "/home/ellson/graphviz/graph/parser.y"
{Current_class = TAG_GRAPH; /* reset */}
    break;
case 37:
#line 373 "/home/ellson/graphviz/graph/parser.y"
{Current_class = TAG_GRAPH;}
    break;
case 38:
#line 377 "/home/ellson/graphviz/graph/parser.y"
{
					objport_t		rv;
					rv.obj = agvsp[-1].n;
					rv.port = strdup(Port);
					Port[0] = '\0';
					agval.obj = rv;
				}
    break;
case 39:
#line 386 "/home/ellson/graphviz/graph/parser.y"
{agval.n = bind_node(agvsp[0].str); agstrfree(agvsp[0].str);}
    break;
case 45:
#line 396 "/home/ellson/graphviz/graph/parser.y"
{strcat(Port,":"); strcat(Port,agvsp[0].str);}
    break;
case 46:
#line 397 "/home/ellson/graphviz/graph/parser.y"
{Symbol = strdup(agvsp[0].str);}
    break;
case 47:
#line 398 "/home/ellson/graphviz/graph/parser.y"
{	char buf[SMALLBUF];
					sprintf(buf,":(%s,%s)",Symbol,agvsp[-1].str);
					strcat(Port,buf); free(Symbol);
				}
    break;
case 48:
#line 405 "/home/ellson/graphviz/graph/parser.y"
{	char buf[SMALLBUF];
					sprintf(buf,"@%s",agvsp[0].str);
					strcat(Port,buf);
				}
    break;
case 49:
#line 412 "/home/ellson/graphviz/graph/parser.y"
{Current_class = TAG_NODE; N = (Agnode_t*)(agvsp[0].obj.obj);}
    break;
case 50:
#line 414 "/home/ellson/graphviz/graph/parser.y"
{Current_class = TAG_GRAPH; /* reset */}
    break;
case 51:
#line 418 "/home/ellson/graphviz/graph/parser.y"
{begin_edgestmt(agvsp[0].obj);}
    break;
case 52:
#line 420 "/home/ellson/graphviz/graph/parser.y"
{ E = SP->subg->proto->e;
				  Current_class = TAG_EDGE; }
    break;
case 53:
#line 423 "/home/ellson/graphviz/graph/parser.y"
{end_edgestmt();}
    break;
case 54:
#line 425 "/home/ellson/graphviz/graph/parser.y"
{begin_edgestmt(agvsp[0].obj);}
    break;
case 55:
#line 427 "/home/ellson/graphviz/graph/parser.y"
{ E = SP->subg->proto->e;
				  Current_class = TAG_EDGE; }
    break;
case 56:
#line 430 "/home/ellson/graphviz/graph/parser.y"
{end_edgestmt();}
    break;
case 57:
#line 433 "/home/ellson/graphviz/graph/parser.y"
{mid_edgestmt(agvsp[0].obj);}
    break;
case 58:
#line 435 "/home/ellson/graphviz/graph/parser.y"
{mid_edgestmt(agvsp[0].obj);}
    break;
case 60:
#line 438 "/home/ellson/graphviz/graph/parser.y"
{mid_edgestmt(agvsp[0].obj);}
    break;
case 61:
#line 440 "/home/ellson/graphviz/graph/parser.y"
{mid_edgestmt(agvsp[0].obj);}
    break;
case 63:
#line 445 "/home/ellson/graphviz/graph/parser.y"
{agval.obj = pop_gobj();}
    break;
case 64:
#line 446 "/home/ellson/graphviz/graph/parser.y"
{ anonsubg(); }
    break;
case 65:
#line 446 "/home/ellson/graphviz/graph/parser.y"
{agval.obj = pop_gobj();}
    break;
case 66:
#line 447 "/home/ellson/graphviz/graph/parser.y"
{ anonsubg(); }
    break;
case 67:
#line 447 "/home/ellson/graphviz/graph/parser.y"
{agval.obj = pop_gobj();}
    break;
case 68:
#line 448 "/home/ellson/graphviz/graph/parser.y"
{agval.obj = pop_gobj();}
    break;
case 69:
#line 452 "/home/ellson/graphviz/graph/parser.y"
{ Agraph_t	 *subg;
				if ((subg = agfindsubg(AG.parsed_g,agvsp[0].str))) aginsert(G,subg);
				else subg = agsubg(G,agvsp[0].str); 
				push_subg(subg);
				In_decl = FALSE;
				agstrfree(agvsp[0].str);
				}
    break;
case 70:
#line 461 "/home/ellson/graphviz/graph/parser.y"
{agval.str = agvsp[0].str; }
    break;
case 71:
#line 462 "/home/ellson/graphviz/graph/parser.y"
{agval.str = agvsp[0].str; }
    break;
case 72:
#line 465 "/home/ellson/graphviz/graph/parser.y"
{agval.str = agvsp[0].str; }
    break;
case 73:
#line 466 "/home/ellson/graphviz/graph/parser.y"
{agval.str = concat(agvsp[-2].str,agvsp[0].str); agstrfree(agvsp[-2].str); agstrfree(agvsp[0].str);}
    break;
}

#line 705 "/usr/share/bison/bison.simple"


  agvsp -= aglen;
  agssp -= aglen;
#if YYLSP_NEEDED
  aglsp -= aglen;
#endif

#if YYDEBUG
  if (agdebug)
    {
      short *agssp1 = agss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (agssp1 != agssp)
	YYFPRINTF (stderr, " %d", *++agssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++agvsp = agval;
#if YYLSP_NEEDED
  *++aglsp = agloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  agn = agr1[agn];

  agstate = agpgoto[agn - YYNTBASE] + *agssp;
  if (agstate >= 0 && agstate <= YYLAST && agcheck[agstate] == *agssp)
    agstate = agtable[agstate];
  else
    agstate = agdefgoto[agn - YYNTBASE];

  goto agnewstate;


/*------------------------------------.
| agerrlab -- here on detecting error |
`------------------------------------*/
agerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!agerrstatus)
    {
      ++agnerrs;

#ifdef YYERROR_VERBOSE
      agn = agpact[agstate];

      if (agn > YYFLAG && agn < YYLAST)
	{
	  YYSIZE_T agsize = 0;
	  char *agmsg;
	  int agx, agcount;

	  agcount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (agx = agn < 0 ? -agn : 0;
	       agx < (int) (sizeof (agtname) / sizeof (char *)); agx++)
	    if (agcheck[agx + agn] == agx)
	      agsize += agstrlen (agtname[agx]) + 15, agcount++;
	  agsize += agstrlen ("parse error, unexpected ") + 1;
	  agsize += agstrlen (agtname[YYTRANSLATE (agchar)]);
	  agmsg = (char *) YYSTACK_ALLOC (agsize);
	  if (agmsg != 0)
	    {
	      char *agp = agstpcpy (agmsg, "parse error, unexpected ");
	      agp = agstpcpy (agp, agtname[YYTRANSLATE (agchar)]);

	      if (agcount < 5)
		{
		  agcount = 0;
		  for (agx = agn < 0 ? -agn : 0;
		       agx < (int) (sizeof (agtname) / sizeof (char *));
		       agx++)
		    if (agcheck[agx + agn] == agx)
		      {
			const char *agq = ! agcount ? ", expecting " : " or ";
			agp = agstpcpy (agp, agq);
			agp = agstpcpy (agp, agtname[agx]);
			agcount++;
		      }
		}
	      agerror (agmsg);
	      YYSTACK_FREE (agmsg);
	    }
	  else
	    agerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	agerror ("parse error");
    }
  goto agerrlab1;


/*--------------------------------------------------.
| agerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
agerrlab1:
  if (agerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (agchar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  agchar, agtname[agchar1]));
      agchar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  agerrstatus = 3;		/* Each real token shifted decrements this */

  goto agerrhandle;


/*-------------------------------------------------------------------.
| agerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
agerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  agn = agdefact[agstate];
  if (agn)
    goto agdefault;
#endif


/*---------------------------------------------------------------.
| agerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
agerrpop:
  if (agssp == agss)
    YYABORT;
  agvsp--;
  agstate = *--agssp;
#if YYLSP_NEEDED
  aglsp--;
#endif

#if YYDEBUG
  if (agdebug)
    {
      short *agssp1 = agss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (agssp1 != agssp)
	YYFPRINTF (stderr, " %d", *++agssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| agerrhandle.  |
`--------------*/
agerrhandle:
  agn = agpact[agstate];
  if (agn == YYFLAG)
    goto agerrdefault;

  agn += YYTERROR;
  if (agn < 0 || agn > YYLAST || agcheck[agn] != YYTERROR)
    goto agerrdefault;

  agn = agtable[agn];
  if (agn < 0)
    {
      if (agn == YYFLAG)
	goto agerrpop;
      agn = -agn;
      goto agreduce;
    }
  else if (agn == 0)
    goto agerrpop;

  if (agn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++agvsp = aglval;
#if YYLSP_NEEDED
  *++aglsp = aglloc;
#endif

  agstate = agn;
  goto agnewstate;


/*-------------------------------------.
| agacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
agacceptlab:
  agresult = 0;
  goto agreturn;

/*-----------------------------------.
| agabortlab -- YYABORT comes here.  |
`-----------------------------------*/
agabortlab:
  agresult = 1;
  goto agreturn;

/*---------------------------------------------.
| agoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
agoverflowlab:
  agerror ("parser stack overflow");
  agresult = 2;
  /* Fall through.  */

agreturn:
#ifndef agoverflow
  if (agss != agssa)
    YYSTACK_FREE (agss);
#endif
  return agresult;
}
#line 468 "/home/ellson/graphviz/graph/parser.y"
