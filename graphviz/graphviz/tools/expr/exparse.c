/* A Bison parser, made from /home/ellson/graphviz/tools/expr/exparse.y
   by GNU bison 1.35.  */

#define EXBISON 1  /* Identify Bison output.  */

# define	MINTOKEN	257
# define	CHAR	258
# define	INT	259
# define	INTEGER	260
# define	UNSIGNED	261
# define	FLOATING	262
# define	STRING	263
# define	VOID	264
# define	ARRAY	265
# define	BREAK	266
# define	CALL	267
# define	CASE	268
# define	CONSTANT	269
# define	CONTINUE	270
# define	DECLARE	271
# define	DEFAULT	272
# define	DYNAMIC	273
# define	ELSE	274
# define	EXIT	275
# define	FOR	276
# define	FUNCTION	277
# define	GSUB	278
# define	ITERATE	279
# define	ID	280
# define	IF	281
# define	LABEL	282
# define	MEMBER	283
# define	NAME	284
# define	POS	285
# define	PRAGMA	286
# define	PRE	287
# define	PRINT	288
# define	PRINTF	289
# define	PROCEDURE	290
# define	QUERY	291
# define	RAND	292
# define	RETURN	293
# define	SRAND	294
# define	SUB	295
# define	SUBSTR	296
# define	SPRINTF	297
# define	SWITCH	298
# define	WHILE	299
# define	F2I	300
# define	F2S	301
# define	I2F	302
# define	I2S	303
# define	S2B	304
# define	S2F	305
# define	S2I	306
# define	F2X	307
# define	I2X	308
# define	S2X	309
# define	X2F	310
# define	X2I	311
# define	X2S	312
# define	X2X	313
# define	XPRINT	314
# define	OR	315
# define	AND	316
# define	EQ	317
# define	NE	318
# define	LE	319
# define	GE	320
# define	LS	321
# define	RS	322
# define	UNARY	323
# define	INC	324
# define	DEC	325
# define	CAST	326
# define	MAXTOKEN	327

#line 1 "/home/ellson/graphviz/tools/expr/exparse.y"


#pragma prototyped

/*
 * Glenn Fowler
 * AT&T Research
 *
 * expression library grammar and compiler
 *
 * NOTE: procedure arguments not implemented yet
 */

#include <stdio.h>
#include <ast.h>

#undef	RS	/* hp.pa <signal.h> grabs this!! */


#line 21 "/home/ellson/graphviz/tools/expr/exparse.y"
#ifndef EXSTYPE
typedef union
{
	struct Exnode_s*expr;
	double		floating;
	struct Exref_s*	reference;
	struct Exid_s*	id;
	Sflong_t	integer;
	int		op;
	char*		string;
	void*		user;
	struct Exbuf_s*	buffer;
} exstype;
# define EXSTYPE exstype
# define EXSTYPE_IS_TRIVIAL 1
#endif
#line 138 "/home/ellson/graphviz/tools/expr/exparse.y"


#include "exgram.h"

#ifndef EXDEBUG
# define EXDEBUG 1
#endif



#define	EXFINAL		248
#define	EXFLAG		-32768
#define	EXNTBASE	98

/* EXTRANSLATE(EXLEX) -- Bison token number corresponding to EXLEX. */
#define EXTRANSLATE(x) ((unsigned)(x) <= 327 ? extranslate[x] : 137)

/* EXTRANSLATE[EXLEX] -- Bison token number corresponding to EXLEX. */
static const char extranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    83,     2,     2,     2,    82,    69,     2,
      89,    94,    80,    78,    61,    79,    97,    81,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    64,    93,
      72,    62,    73,    63,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    95,     2,    96,    68,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    91,    67,    92,    84,     2,     2,     2,
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
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    65,    66,    70,    71,    74,
      75,    76,    77,    85,    86,    87,    88,    90
};

#if EXDEBUG
static const short exprhs[] =
{
       0,     0,     3,     4,     7,     8,    13,    14,    17,    21,
      24,    25,    30,    37,    43,    53,    59,    60,    69,    73,
      77,    81,    82,    85,    88,    90,    93,    97,   100,   102,
     106,   107,   112,   114,   116,   118,   120,   122,   124,   125,
     128,   129,   131,   135,   140,   144,   148,   152,   156,   160,
     164,   168,   172,   176,   180,   184,   188,   192,   196,   200,
     204,   208,   212,   216,   217,   218,   226,   229,   232,   235,
     238,   243,   248,   253,   258,   263,   268,   272,   276,   281,
     286,   291,   296,   299,   302,   305,   308,   311,   313,   315,
     317,   319,   321,   323,   325,   327,   329,   332,   336,   338,
     339,   342,   346,   347,   351,   352,   354,   356,   360,   361,
     363,   365,   367,   371,   372,   376,   377,   379,   383,   386,
     389,   390,   393,   395,   396,   397
};
static const short exrhs[] =
{
     102,    99,     0,     0,    99,   100,     0,     0,    28,    64,
     101,   102,     0,     0,   102,   103,     0,    91,   102,    92,
       0,   116,    93,     0,     0,    17,   104,   110,    93,     0,
      27,    89,   117,    94,   103,   115,     0,    22,    89,   122,
      94,   103,     0,    22,    89,   116,    93,   116,    93,   116,
      94,   103,     0,    45,    89,   117,    94,   103,     0,     0,
      44,    89,   117,   105,    94,    91,   106,    92,     0,    12,
     116,    93,     0,    16,   116,    93,     0,    39,   116,    93,
       0,     0,   106,   107,     0,   108,   102,     0,   109,     0,
     108,   109,     0,    14,   120,    64,     0,    18,    64,     0,
     111,     0,   110,    61,   111,     0,     0,   113,   112,   123,
     134,     0,    30,     0,    19,     0,    26,     0,    23,     0,
      30,     0,    19,     0,     0,    20,   103,     0,     0,   117,
       0,    89,   117,    94,     0,    89,    17,    94,   117,     0,
     117,    72,   117,     0,   117,    79,   117,     0,   117,    80,
     117,     0,   117,    81,   117,     0,   117,    82,   117,     0,
     117,    76,   117,     0,   117,    77,   117,     0,   117,    73,
     117,     0,   117,    74,   117,     0,   117,    75,   117,     0,
     117,    70,   117,     0,   117,    71,   117,     0,   117,    69,
     117,     0,   117,    67,   117,     0,   117,    68,   117,     0,
     117,    78,   117,     0,   117,    66,   117,     0,   117,    65,
     117,     0,   117,    61,   117,     0,     0,     0,   117,    63,
     118,   117,    64,   119,   117,     0,    83,   117,     0,    84,
     117,     0,    79,   117,     0,    78,   117,     0,    11,    95,
     125,    96,     0,    23,    89,   125,    94,     0,    24,    89,
     125,    94,     0,    41,    89,   125,    94,     0,    42,    89,
     125,    94,     0,    21,    89,   117,    94,     0,    38,    89,
      94,     0,    40,    89,    94,     0,    40,    89,   117,    94,
       0,    36,    89,   125,    94,     0,    34,    89,   125,    94,
       0,   121,    89,   125,    94,     0,   122,   133,     0,    86,
     122,     0,   122,    86,     0,    87,   122,     0,   122,    87,
       0,   120,     0,    15,     0,     8,     0,     6,     0,     9,
       0,     7,     0,    35,     0,    37,     0,    43,     0,    26,
     131,     0,    19,   124,   131,     0,    30,     0,     0,    95,
      96,     0,    95,    17,    96,     0,     0,    95,   117,    96,
       0,     0,   126,     0,   117,     0,   126,    61,   117,     0,
       0,    17,     0,   128,     0,   129,     0,   128,    61,   129,
       0,     0,    17,   130,   114,     0,     0,   132,     0,    97,
      26,   132,     0,    97,    26,     0,    97,    30,     0,     0,
      62,   117,     0,   133,     0,     0,     0,    89,   135,   127,
     136,    94,    91,   102,    92,     0
};

#endif

#if EXDEBUG
/* EXRLINE[EXN] -- source line where rule number EXN was defined. */
static const short exrline[] =
{
       0,   146,   167,   168,   171,   171,   206,   210,   225,   229,
     233,   233,   237,   247,   260,   275,   285,   285,   296,   308,
     312,   324,   354,   357,   386,   387,   390,   411,   417,   418,
     425,   425,   473,   474,   475,   476,   479,   480,   483,   487,
     493,   497,   500,   504,   508,   553,   557,   561,   565,   569,
     573,   577,   581,   585,   589,   593,   597,   601,   605,   609,
     613,   626,   630,   639,   639,   639,   678,   698,   702,   706,
     710,   714,   718,   722,   726,   730,   736,   740,   744,   750,
     755,   759,   783,   807,   815,   823,   827,   831,   834,   841,
     846,   851,   856,   863,   864,   865,   868,   872,   892,   904,
     908,   912,   925,   929,   935,   939,   947,   952,   958,   962,
     968,   971,   975,   986,   986,   996,  1000,  1012,  1031,  1035,
    1041,  1045,  1052,  1053,  1053,  1053
};
#endif


#if (EXDEBUG) || defined EXERROR_VERBOSE

/* EXTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const extname[] =
{
  "$", "error", "$undefined.", "MINTOKEN", "CHAR", "INT", "INTEGER", 
  "UNSIGNED", "FLOATING", "STRING", "VOID", "ARRAY", "BREAK", "CALL", 
  "CASE", "CONSTANT", "CONTINUE", "DECLARE", "DEFAULT", "DYNAMIC", "ELSE", 
  "EXIT", "FOR", "FUNCTION", "GSUB", "ITERATE", "ID", "IF", "LABEL", 
  "MEMBER", "NAME", "POS", "PRAGMA", "PRE", "PRINT", "PRINTF", 
  "PROCEDURE", "QUERY", "RAND", "RETURN", "SRAND", "SUB", "SUBSTR", 
  "SPRINTF", "SWITCH", "WHILE", "F2I", "F2S", "I2F", "I2S", "S2B", "S2F", 
  "S2I", "F2X", "I2X", "S2X", "X2F", "X2I", "X2S", "X2X", "XPRINT", "','", 
  "'='", "'?'", "':'", "OR", "AND", "'|'", "'^'", "'&'", "EQ", "NE", 
  "'<'", "'>'", "LE", "GE", "LS", "RS", "'+'", "'-'", "'*'", "'/'", "'%'", 
  "'!'", "'~'", "UNARY", "INC", "DEC", "CAST", "'('", "MAXTOKEN", "'{'", 
  "'}'", "';'", "')'", "'['", "']'", "'.'", "program", "action_list", 
  "action", "@1", "statement_list", "statement", "@2", "@3", 
  "switch_list", "switch_item", "case_list", "case_item", "dcl_list", 
  "dcl_item", "@4", "dcl_name", "name", "else_opt", "expr_opt", "expr", 
  "@5", "@6", "constant", "print", "variable", "array", "index", "args", 
  "arg_list", "formals", "formal_list", "formal_item", "@7", "members", 
  "member", "assign", "initialize", "@8", "@9", 0
};
#endif

/* EXR1[EXN] -- Symbol number of symbol that rule EXN derives. */
static const short exr1[] =
{
       0,    98,    99,    99,   101,   100,   102,   102,   103,   103,
     104,   103,   103,   103,   103,   103,   105,   103,   103,   103,
     103,   106,   106,   107,   108,   108,   109,   109,   110,   110,
     112,   111,   113,   113,   113,   113,   114,   114,   115,   115,
     116,   116,   117,   117,   117,   117,   117,   117,   117,   117,
     117,   117,   117,   117,   117,   117,   117,   117,   117,   117,
     117,   117,   117,   118,   119,   117,   117,   117,   117,   117,
     117,   117,   117,   117,   117,   117,   117,   117,   117,   117,
     117,   117,   117,   117,   117,   117,   117,   117,   120,   120,
     120,   120,   120,   121,   121,   121,   122,   122,   122,   123,
     123,   123,   124,   124,   125,   125,   126,   126,   127,   127,
     127,   128,   128,   130,   129,   131,   131,   131,   132,   132,
     133,   133,   134,   135,   136,   134
};

/* EXR2[EXN] -- Number of symbols composing right hand side of rule EXN. */
static const short exr2[] =
{
       0,     2,     0,     2,     0,     4,     0,     2,     3,     2,
       0,     4,     6,     5,     9,     5,     0,     8,     3,     3,
       3,     0,     2,     2,     1,     2,     3,     2,     1,     3,
       0,     4,     1,     1,     1,     1,     1,     1,     0,     2,
       0,     1,     3,     4,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     0,     0,     7,     2,     2,     2,     2,
       4,     4,     4,     4,     4,     4,     3,     3,     4,     4,
       4,     4,     2,     2,     2,     2,     2,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     3,     1,     0,
       2,     3,     0,     3,     0,     1,     1,     3,     0,     1,
       1,     1,     3,     0,     3,     0,     1,     3,     2,     2,
       0,     2,     1,     0,     0,     8
};

/* EXDEFACT[S] -- default rule to reduce with in state S when EXTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short exdefact[] =
{
       6,     2,    90,    92,    89,    91,     0,    40,    88,    40,
      10,   102,     0,     0,     0,     0,   115,     0,    98,     0,
      93,     0,    94,     0,    40,     0,     0,     0,    95,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     6,     1,
       7,     0,    41,    87,     0,   120,   104,     0,     0,     0,
       0,   115,     0,    40,   104,   104,     0,    96,   116,     0,
     104,   104,     0,     0,     0,   104,   104,     0,     0,    69,
      68,    66,    67,    83,    85,     0,     0,    40,     0,     3,
       9,     0,    63,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   104,     0,    84,    86,    82,   106,     0,   105,    18,
      19,    33,    35,    34,    32,     0,    28,    30,     0,    97,
       0,     0,   120,     0,     0,   118,   119,     0,     0,     0,
      76,    20,    77,     0,     0,     0,    16,     0,     0,    42,
       8,     4,    62,     0,    61,    60,    57,    58,    56,    54,
      55,    44,    51,    52,    53,    49,    50,    59,    45,    46,
      47,    48,     0,   121,    70,     0,     0,    11,    99,   103,
      75,    40,    40,    71,    72,     0,   117,    40,    80,    79,
      78,    73,    74,     0,    40,    43,     6,     0,    81,   107,
      29,     0,   120,     0,    13,   118,    38,     0,    15,     5,
      64,     0,   100,   123,   122,    31,    40,    40,    12,    21,
       0,   101,   108,     0,    39,     0,    65,   113,   124,   110,
     111,    40,     0,     0,    17,    22,     6,    24,     0,     0,
       0,    14,     0,    27,    23,    25,    37,    36,   114,     0,
     113,   112,    26,     6,    40,   125,     0,     0,     0
};

static const short exdefgoto[] =
{
     246,    39,    79,   186,     1,    40,    49,   183,   215,   225,
     226,   227,   115,   116,   168,   117,   238,   208,    41,    42,
     143,   210,    43,    44,    45,   192,    51,   107,   108,   218,
     219,   220,   228,    57,    58,   105,   205,   212,   229
};

static const short expact[] =
{
  -32768,   187,-32768,-32768,-32768,-32768,   -79,   619,-32768,   619,
  -32768,   -73,   -81,   -53,   -46,   -41,   -48,   -34,-32768,   -13,
  -32768,   -12,-32768,    -5,   619,     3,    11,    12,-32768,    15,
      16,   619,   619,   619,   619,     5,     5,   535,-32768,    66,
  -32768,    26,   668,-32768,    32,   -36,   619,    29,    34,   107,
     619,   -48,   619,   619,   619,   619,    -9,-32768,-32768,   619,
     619,   619,    31,    38,    72,   619,   619,   619,   619,-32768,
  -32768,-32768,-32768,-32768,-32768,    41,   261,   275,    68,-32768,
  -32768,   619,-32768,   619,   619,   619,   619,   619,   619,   619,
     619,   619,   619,   619,   619,   619,   619,   619,   619,   619,
     619,   619,   619,-32768,-32768,-32768,   688,    40,    78,-32768,
  -32768,-32768,-32768,-32768,-32768,   -47,-32768,-32768,   172,-32768,
     348,    54,   -42,    58,    59,    57,-32768,   435,    69,    90,
  -32768,-32768,-32768,   518,    91,    92,   668,   602,   619,-32768,
  -32768,-32768,   688,   619,   705,   100,   720,   734,   747,   760,
     760,   529,   529,   529,   529,   177,   177,    64,    64,-32768,
  -32768,-32768,    93,   688,-32768,   619,   107,-32768,    62,-32768,
  -32768,   619,   449,-32768,-32768,    56,-32768,   449,-32768,-32768,
  -32768,-32768,-32768,    95,   449,-32768,-32768,   646,-32768,   688,
  -32768,    -8,   -60,    67,-32768,-32768,   142,    99,-32768,   187,
  -32768,    96,-32768,-32768,-32768,-32768,   619,   449,-32768,-32768,
     619,-32768,   174,   103,-32768,    -3,   688,   113,-32768,   140,
  -32768,   449,   134,   148,-32768,-32768,    85,-32768,    23,   121,
     199,-32768,   154,-32768,   187,-32768,-32768,-32768,-32768,   128,
  -32768,-32768,-32768,-32768,   362,-32768,   220,   234,-32768
};

static const short expgoto[] =
{
  -32768,-32768,-32768,-32768,   -38,   -87,-32768,-32768,-32768,-32768,
  -32768,    10,-32768,    94,-32768,-32768,-32768,-32768,    -6,   -27,
  -32768,-32768,    39,-32768,   -23,-32768,-32768,    63,-32768,-32768,
  -32768,    33,-32768,   211,   139,    75,-32768,-32768,-32768
};


#define	EXLAST		842


static const short extable[] =
{
      77,    47,   102,    48,    69,    70,    71,    72,    52,   201,
      76,   222,    73,    74,   166,   223,    46,   125,    63,   106,
     102,   126,    50,   118,    11,   120,   102,   106,   106,   203,
     122,    16,   127,   106,   106,    18,    53,   133,   106,   106,
     136,   137,   236,    54,   103,   104,   167,   121,    55,    56,
     103,   104,   172,   237,   142,    59,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   106,   163,    60,    61,     2,     3,
       4,     5,   195,     6,    62,   194,   126,     8,   202,   224,
     196,    11,    64,    12,    78,    14,    15,   198,    16,   222,
      65,    66,    18,   223,    67,    68,    19,    20,    21,    22,
      23,   185,    25,    26,    27,    28,   187,   123,   124,    80,
     214,   101,   109,   128,   129,   130,   111,   110,   134,   135,
     112,   131,   141,   113,   231,   138,   164,   114,   189,   165,
       2,     3,     4,     5,    98,    99,   100,   171,   199,     8,
      31,    32,   173,   174,   175,    33,    34,   191,    35,    36,
     206,    37,   207,   178,   162,   193,   132,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   216,   179,   181,   182,   188,   234,   197,
     209,   217,   211,     2,     3,     4,     5,   221,     6,     7,
     213,   230,     8,     9,    10,   244,    11,  -109,    12,    13,
      14,    15,   233,    16,    17,   239,   240,    18,   242,   243,
     247,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    81,   248,    82,   235,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,    96,    97,    98,    99,   100,
     190,   232,   119,   241,   176,    31,    32,   204,   169,     0,
      33,    34,     0,    35,    36,     0,    37,     0,    38,     0,
     -40,     2,     3,     4,     5,     0,     6,     7,     0,     0,
       8,     9,    10,     0,    11,     0,    12,    13,    14,    15,
       0,    16,    17,     0,     0,    18,     0,     0,     0,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,     0,    81,     0,    82,     0,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    31,    32,   139,     0,     0,    33,    34,
       0,    35,    36,     0,    37,     0,    38,   140,     2,     3,
       4,     5,     0,     6,     7,     0,     0,     8,     9,    10,
       0,    11,     0,    12,    13,    14,    15,     0,    16,    17,
       0,     0,    18,     0,     0,     0,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,     0,    81,
       0,    82,     0,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      31,    32,   170,     0,     0,    33,    34,     0,    35,    36,
       0,    37,     0,    38,   245,     2,     3,     4,     5,     0,
       6,     7,     0,     0,     8,     9,    10,     0,    11,     0,
      12,    13,    14,    15,     0,    16,    17,     0,     0,    18,
       0,     0,     0,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,     0,    81,     0,    82,     0,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    31,    32,   177,
       0,     0,    33,    34,     0,    35,    36,     0,    37,     0,
      38,     2,     3,     4,     5,     0,     6,     0,     0,     0,
       8,     0,    75,     0,    11,     0,    12,     0,    14,    15,
       0,    16,     0,     0,     0,    18,     0,     0,     0,    19,
      20,    21,    22,    23,     0,    25,    26,    27,    28,    81,
       0,    82,     0,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,-32768,-32768,-32768,-32768,    94,    95,    96,    97,    98,
      99,   100,   180,    31,    32,     0,     0,     0,    33,    34,
       0,    35,    36,     0,    37,     2,     3,     4,     5,     0,
       6,     0,     0,     0,     8,     0,     0,     0,    11,     0,
      12,     0,    14,    15,     0,    16,     0,     0,     0,    18,
       0,     0,     0,    19,    20,    21,    22,    23,     0,    25,
      26,    27,    28,    81,     0,    82,     0,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   184,    31,    32,     0,
       0,     0,    33,    34,     0,    35,    36,    81,    37,    82,
     200,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,    81,
       0,    82,     0,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,    82,     0,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
  -32768,-32768,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100
};

static const short excheck[] =
{
      38,     7,    62,     9,    31,    32,    33,    34,    89,    17,
      37,    14,    35,    36,    61,    18,    95,    26,    24,    46,
      62,    30,    95,    50,    19,    52,    62,    54,    55,    89,
      53,    26,    59,    60,    61,    30,    89,    64,    65,    66,
      67,    68,    19,    89,    86,    87,    93,    53,    89,    97,
      86,    87,    94,    30,    81,    89,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,    89,    89,     6,     7,
       8,     9,    26,    11,    89,   172,    30,    15,    96,    92,
     177,    19,    89,    21,    28,    23,    24,   184,    26,    14,
      89,    89,    30,    18,    89,    89,    34,    35,    36,    37,
      38,   138,    40,    41,    42,    43,   143,    54,    55,    93,
     207,    89,    93,    60,    61,    94,    19,    93,    65,    66,
      23,    93,    64,    26,   221,    94,    96,    30,   165,    61,
       6,     7,     8,     9,    80,    81,    82,    93,   186,    15,
      78,    79,    94,    94,    97,    83,    84,    95,    86,    87,
      93,    89,    20,    94,   101,   171,    94,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,   210,    94,    94,    94,    94,   226,    94,
      91,    17,    96,     6,     7,     8,     9,    94,    11,    12,
     206,    61,    15,    16,    17,   243,    19,    94,    21,    22,
      23,    24,    64,    26,    27,    94,    17,    30,    64,    91,
       0,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    61,     0,    63,   226,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    78,    79,    80,    81,    82,
     166,   222,    51,   230,   125,    78,    79,   192,    96,    -1,
      83,    84,    -1,    86,    87,    -1,    89,    -1,    91,    -1,
      93,     6,     7,     8,     9,    -1,    11,    12,    -1,    -1,
      15,    16,    17,    -1,    19,    -1,    21,    22,    23,    24,
      -1,    26,    27,    -1,    -1,    30,    -1,    -1,    -1,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    -1,    61,    -1,    63,    -1,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    78,    79,    94,    -1,    -1,    83,    84,
      -1,    86,    87,    -1,    89,    -1,    91,    92,     6,     7,
       8,     9,    -1,    11,    12,    -1,    -1,    15,    16,    17,
      -1,    19,    -1,    21,    22,    23,    24,    -1,    26,    27,
      -1,    -1,    30,    -1,    -1,    -1,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    -1,    61,
      -1,    63,    -1,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      78,    79,    94,    -1,    -1,    83,    84,    -1,    86,    87,
      -1,    89,    -1,    91,    92,     6,     7,     8,     9,    -1,
      11,    12,    -1,    -1,    15,    16,    17,    -1,    19,    -1,
      21,    22,    23,    24,    -1,    26,    27,    -1,    -1,    30,
      -1,    -1,    -1,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    -1,    61,    -1,    63,    -1,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    94,
      -1,    -1,    83,    84,    -1,    86,    87,    -1,    89,    -1,
      91,     6,     7,     8,     9,    -1,    11,    -1,    -1,    -1,
      15,    -1,    17,    -1,    19,    -1,    21,    -1,    23,    24,
      -1,    26,    -1,    -1,    -1,    30,    -1,    -1,    -1,    34,
      35,    36,    37,    38,    -1,    40,    41,    42,    43,    61,
      -1,    63,    -1,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    94,    78,    79,    -1,    -1,    -1,    83,    84,
      -1,    86,    87,    -1,    89,     6,     7,     8,     9,    -1,
      11,    -1,    -1,    -1,    15,    -1,    -1,    -1,    19,    -1,
      21,    -1,    23,    24,    -1,    26,    -1,    -1,    -1,    30,
      -1,    -1,    -1,    34,    35,    36,    37,    38,    -1,    40,
      41,    42,    43,    61,    -1,    63,    -1,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    94,    78,    79,    -1,
      -1,    -1,    83,    84,    -1,    86,    87,    61,    89,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    61,
      -1,    63,    -1,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    63,    -1,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82
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

/* All symbols defined below should begin with ex or EX, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (exoverflow) || defined (EXERROR_VERBOSE)

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if EXSTACK_USE_ALLOCA
#  define EXSTACK_ALLOC alloca
# else
#  ifndef EXSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define EXSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define EXSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef EXSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define EXSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define EXSIZE_T size_t
#  endif
#  define EXSTACK_ALLOC malloc
#  define EXSTACK_FREE free
# endif
#endif /* ! defined (exoverflow) || defined (EXERROR_VERBOSE) */


#if (! defined (exoverflow) \
     && (! defined (__cplusplus) \
	 || (EXLTYPE_IS_TRIVIAL && EXSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union exalloc
{
  short exss;
  EXSTYPE exvs;
# if EXLSP_NEEDED
  EXLTYPE exls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define EXSTACK_GAP_MAX (sizeof (union exalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# if EXLSP_NEEDED
#  define EXSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (EXSTYPE) + sizeof (EXLTYPE))	\
      + 2 * EXSTACK_GAP_MAX)
# else
#  define EXSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (EXSTYPE))				\
      + EXSTACK_GAP_MAX)
# endif

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef EXCOPY
#  if 1 < __GNUC__
#   define EXCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define EXCOPY(To, From, Count)		\
      do					\
	{					\
	  register EXSIZE_T exi;		\
	  for (exi = 0; exi < (Count); exi++)	\
	    (To)[exi] = (From)[exi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables EXSIZE and EXSTACKSIZE give the old and new number of
   elements in the stack, and EXPTR gives the new location of the
   stack.  Advance EXPTR to a properly aligned location for the next
   stack.  */
# define EXSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	EXSIZE_T exnewbytes;						\
	EXCOPY (&exptr->Stack, Stack, exsize);				\
	Stack = &exptr->Stack;						\
	exnewbytes = exstacksize * sizeof (*Stack) + EXSTACK_GAP_MAX;	\
	exptr += exnewbytes / sizeof (*exptr);				\
      }									\
    while (0)

#endif


#if ! defined (EXSIZE_T) && defined (__SIZE_TYPE__)
# define EXSIZE_T __SIZE_TYPE__
#endif
#if ! defined (EXSIZE_T) && defined (size_t)
# define EXSIZE_T size_t
#endif
#if ! defined (EXSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define EXSIZE_T size_t
# endif
#endif
#if ! defined (EXSIZE_T)
# define EXSIZE_T unsigned int
#endif

#define exerrok		(exerrstatus = 0)
#define exclearin	(exchar = EXEMPTY)
#define EXEMPTY		-2
#define EXEOF		0
#define EXACCEPT	goto exacceptlab
#define EXABORT 	goto exabortlab
#define EXERROR		goto exerrlab1
/* Like EXERROR except do call exerror.  This remains here temporarily
   to ease the transition to the new meaning of EXERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define EXFAIL		goto exerrlab
#define EXRECOVERING()  (!!exerrstatus)
#define EXBACKUP(Token, Value)					\
do								\
  if (exchar == EXEMPTY && exlen == 1)				\
    {								\
      exchar = (Token);						\
      exlval = (Value);						\
      exchar1 = EXTRANSLATE (exchar);				\
      EXPOPSTACK;						\
      goto exbackup;						\
    }								\
  else								\
    { 								\
      exerror ("syntax error: cannot back up");			\
      EXERROR;							\
    }								\
while (0)

#define EXTERROR	1
#define EXERRCODE	256


/* EXLLOC_DEFAULT -- Compute the default location (before the actions
   are run).

   When EXLLOC_DEFAULT is run, CURRENT is set the location of the
   first token.  By default, to implement support for ranges, extend
   its range to the last symbol.  */

#ifndef EXLLOC_DEFAULT
# define EXLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif


/* EXLEX -- calling `exlex' with the right arguments.  */

#if EXPURE
# if EXLSP_NEEDED
#  ifdef EXLEX_PARAM
#   define EXLEX		exlex (&exlval, &exlloc, EXLEX_PARAM)
#  else
#   define EXLEX		exlex (&exlval, &exlloc)
#  endif
# else /* !EXLSP_NEEDED */
#  ifdef EXLEX_PARAM
#   define EXLEX		exlex (&exlval, EXLEX_PARAM)
#  else
#   define EXLEX		exlex (&exlval)
#  endif
# endif /* !EXLSP_NEEDED */
#else /* !EXPURE */
# define EXLEX			exlex ()
#endif /* !EXPURE */


/* Enable debugging if requested.  */
#if EXDEBUG

# ifndef EXFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define EXFPRINTF sfprintf
# endif

# define EXDPRINTF(Args)			\
do {						\
  if (exdebug)					\
    EXFPRINTF Args;				\
} while (0)
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int exdebug;
#else /* !EXDEBUG */
# define EXDPRINTF(Args)
#endif /* !EXDEBUG */

/* EXINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	EXINITDEPTH
# define EXINITDEPTH 200
#endif

/* EXMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < EXSTACK_BYTES (EXMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if EXMAXDEPTH == 0
# undef EXMAXDEPTH
#endif

#ifndef EXMAXDEPTH
# define EXMAXDEPTH 10000
#endif

#ifdef EXERROR_VERBOSE

# ifndef exstrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define exstrlen strlen
#  else
/* Return the length of EXSTR.  */
static EXSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
exstrlen (const char *exstr)
#   else
exstrlen (exstr)
     const char *exstr;
#   endif
{
  register const char *exs = exstr;

  while (*exs++ != '\0')
    continue;

  return exs - exstr - 1;
}
#  endif
# endif

# ifndef exstpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define exstpcpy stpcpy
#  else
/* Copy EXSRC to EXDEST, returning the address of the terminating '\0' in
   EXDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
exstpcpy (char *exdest, const char *exsrc)
#   else
exstpcpy (exdest, exsrc)
     char *exdest;
     const char *exsrc;
#   endif
{
  register char *exd = exdest;
  register const char *exs = exsrc;

  while ((*exd++ = *exs++) != '\0')
    continue;

  return exd - 1;
}
#  endif
# endif
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define EXPARSE_PARAM as the name of an argument to be passed
   into exparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef EXPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define EXPARSE_PARAM_ARG void *EXPARSE_PARAM
#  define EXPARSE_PARAM_DECL
# else
#  define EXPARSE_PARAM_ARG EXPARSE_PARAM
#  define EXPARSE_PARAM_DECL void *EXPARSE_PARAM;
# endif
#else /* !EXPARSE_PARAM */
# define EXPARSE_PARAM_ARG
# define EXPARSE_PARAM_DECL
#endif /* !EXPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef EXPARSE_PARAM
int exparse (void *);
# else
int exparse (void);
# endif
#endif

/* EX_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to EXPARSE.  */

#define EX_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int exchar;						\
							\
/* The semantic value of the lookahead symbol. */	\
EXSTYPE exlval;						\
							\
/* Number of parse errors so far.  */			\
int exnerrs;

#if EXLSP_NEEDED
# define EX_DECL_VARIABLES			\
EX_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
EXLTYPE exlloc;
#else
# define EX_DECL_VARIABLES			\
EX_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !EXPURE
EX_DECL_VARIABLES
#endif  /* !EXPURE */

int
exparse (EXPARSE_PARAM_ARG)
     EXPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if EXPURE
  EX_DECL_VARIABLES
#endif  /* !EXPURE */

  register int exstate;
  register int exn;
  int exresult;
  /* Number of tokens to shift before error messages enabled.  */
  int exerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int exchar1 = 0;

  /* Three stacks and their tools:
     `exss': related to states,
     `exvs': related to semantic values,
     `exls': related to locations.

     Refer to the stacks thru separate pointers, to allow exoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	exssa[EXINITDEPTH];
  short *exss = exssa;
  register short *exssp;

  /* The semantic value stack.  */
  EXSTYPE exvsa[EXINITDEPTH];
  EXSTYPE *exvs = exvsa;
  register EXSTYPE *exvsp;

#if EXLSP_NEEDED
  /* The location stack.  */
  EXLTYPE exlsa[EXINITDEPTH];
  EXLTYPE *exls = exlsa;
  EXLTYPE *exlsp;
#endif

#if EXLSP_NEEDED
# define EXPOPSTACK   (exvsp--, exssp--, exlsp--)
#else
# define EXPOPSTACK   (exvsp--, exssp--)
#endif

  EXSIZE_T exstacksize = EXINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  EXSTYPE exval;
#if EXLSP_NEEDED
  EXLTYPE exloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
  int exlen;

  EXDPRINTF ((sfstderr, "Starting parse\n"));

  exstate = 0;
  exerrstatus = 0;
  exnerrs = 0;
  exchar = EXEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  exssp = exss;
  exvsp = exvs;
#if EXLSP_NEEDED
  exlsp = exls;
#endif
  goto exsetstate;

/*------------------------------------------------------------.
| exnewstate -- Push a new state, which is found in exstate.  |
`------------------------------------------------------------*/
 exnewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  exssp++;

 exsetstate:
  *exssp = exstate;

  if (exssp >= exss + exstacksize - 1)
    {
      /* Get the current used size of the three stacks, in elements.  */
      EXSIZE_T exsize = exssp - exss + 1;

#ifdef exoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	EXSTYPE *exvs1 = exvs;
	short *exss1 = exss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  */
# if EXLSP_NEEDED
	EXLTYPE *exls1 = exls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if exoverflow is a macro.  */
	exoverflow ("parser stack overflow",
		    &exss1, exsize * sizeof (*exssp),
		    &exvs1, exsize * sizeof (*exvsp),
		    &exls1, exsize * sizeof (*exlsp),
		    &exstacksize);
	exls = exls1;
# else
	exoverflow ("parser stack overflow",
		    &exss1, exsize * sizeof (*exssp),
		    &exvs1, exsize * sizeof (*exvsp),
		    &exstacksize);
# endif
	exss = exss1;
	exvs = exvs1;
      }
#else /* no exoverflow */
# ifndef EXSTACK_RELOCATE
      goto exoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (exstacksize >= EXMAXDEPTH)
	goto exoverflowlab;
      exstacksize *= 2;
      if (exstacksize > EXMAXDEPTH)
	exstacksize = EXMAXDEPTH;

      {
	short *exss1 = exss;
	union exalloc *exptr =
	  (union exalloc *) EXSTACK_ALLOC (EXSTACK_BYTES (exstacksize));
	if (! exptr)
	  goto exoverflowlab;
	EXSTACK_RELOCATE (exss);
	EXSTACK_RELOCATE (exvs);
# if EXLSP_NEEDED
	EXSTACK_RELOCATE (exls);
# endif
# undef EXSTACK_RELOCATE
	if (exss1 != exssa)
	  EXSTACK_FREE (exss1);
      }
# endif
#endif /* no exoverflow */

      exssp = exss + exsize - 1;
      exvsp = exvs + exsize - 1;
#if EXLSP_NEEDED
      exlsp = exls + exsize - 1;
#endif

      EXDPRINTF ((sfstderr, "Stack size increased to %lu\n",
		  (unsigned long int) exstacksize));

      if (exssp >= exss + exstacksize - 1)
	EXABORT;
    }

  EXDPRINTF ((sfstderr, "Entering state %d\n", exstate));

  goto exbackup;


/*-----------.
| exbackup.  |
`-----------*/
exbackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* exresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  exn = expact[exstate];
  if (exn == EXFLAG)
    goto exdefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* exchar is either EXEMPTY or EXEOF
     or a valid token in external form.  */

  if (exchar == EXEMPTY)
    {
      EXDPRINTF ((sfstderr, "Reading a token: "));
      exchar = EXLEX;
    }

  /* Convert token to internal form (in exchar1) for indexing tables with */

  if (exchar <= 0)		/* This means end of input. */
    {
      exchar1 = 0;
      exchar = EXEOF;		/* Don't call EXLEX any more */

      EXDPRINTF ((sfstderr, "Now at end of input.\n"));
    }
  else
    {
      exchar1 = EXTRANSLATE (exchar);

#if EXDEBUG
     /* We have to keep this `#if EXDEBUG', since we use variables
	which are defined only if `EXDEBUG' is set.  */
      if (exdebug)
	{
	  EXFPRINTF (sfstderr, "Next token is %d (%s",
		     exchar, extname[exchar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef EXPRINT
	  EXPRINT (sfstderr, exchar, exlval);
# endif
	  EXFPRINTF (sfstderr, ")\n");
	}
#endif
    }

  exn += exchar1;
  if (exn < 0 || exn > EXLAST || excheck[exn] != exchar1)
    goto exdefault;

  exn = extable[exn];

  /* exn is what to do for this token type in this state.
     Negative => reduce, -exn is rule number.
     Positive => shift, exn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (exn < 0)
    {
      if (exn == EXFLAG)
	goto exerrlab;
      exn = -exn;
      goto exreduce;
    }
  else if (exn == 0)
    goto exerrlab;

  if (exn == EXFINAL)
    EXACCEPT;

  /* Shift the lookahead token.  */
  EXDPRINTF ((sfstderr, "Shifting token %d (%s), ",
	      exchar, extname[exchar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (exchar != EXEOF)
    exchar = EXEMPTY;

  *++exvsp = exlval;
#if EXLSP_NEEDED
  *++exlsp = exlloc;
#endif

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (exerrstatus)
    exerrstatus--;

  exstate = exn;
  goto exnewstate;


/*-----------------------------------------------------------.
| exdefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
exdefault:
  exn = exdefact[exstate];
  if (exn == 0)
    goto exerrlab;
  goto exreduce;


/*-----------------------------.
| exreduce -- Do a reduction.  |
`-----------------------------*/
exreduce:
  /* exn is the number of a rule to reduce with.  */
  exlen = exr2[exn];

  /* If EXLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets EXVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to EXVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that EXVAL may be used uninitialized.  */
  exval = exvsp[1-exlen];

#if EXLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  exloc = exlsp[1-exlen];
  EXLLOC_DEFAULT (exloc, (exlsp - exlen), exlen);
#endif

#if EXDEBUG
  /* We have to keep this `#if EXDEBUG', since we use variables which
     are defined only if `EXDEBUG' is set.  */
  if (exdebug)
    {
      int exi;

      EXFPRINTF (sfstderr, "Reducing via rule %d (line %d), ",
		 exn, exrline[exn]);

      /* Print the symbols being reduced, and their result.  */
      for (exi = exprhs[exn]; exrhs[exi] > 0; exi++)
	EXFPRINTF (sfstderr, "%s ", extname[exrhs[exi]]);
      EXFPRINTF (sfstderr, " -> %s\n", extname[exr1[exn]]);
    }
#endif

  switch (exn) {

case 1:
#line 147 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (exvsp[-1].expr && !(expr.program->disc->flags & EX_STRICT))
			{
				if (expr.program->main.value && !(expr.program->disc->flags & EX_RETAIN))
					exfreenode(expr.program, expr.program->main.value);
				if (exvsp[-1].expr->op == S2B)
				{
					Exnode_t*	x;

					x = exvsp[-1].expr;
					exvsp[-1].expr = x->data.operand.left;
					x->data.operand.left = 0;
					exfreenode(expr.program, x);
				}
				expr.program->main.lex = PROCEDURE;
				expr.program->main.value = exnewnode(expr.program, PROCEDURE, 1, exvsp[-1].expr->type, NiL, exvsp[-1].expr);
			}
		}
    break;
case 4:
#line 171 "/home/ellson/graphviz/tools/expr/exparse.y"
{
				register Dtdisc_t*	disc;

				if (expr.procedure)
					exerror("no nested function definitions");
				exvsp[-1].id->lex = PROCEDURE;
				expr.procedure = exvsp[-1].id->value = exnewnode(expr.program, PROCEDURE, 1, exvsp[-1].id->type, NiL, NiL);
				expr.procedure->type = INTEGER;
				if (!(disc = newof(0, Dtdisc_t, 1, 0)))
					exerror("out of space [frame discipline]");
				disc->key = offsetof(Exid_t, name);
				if (!(expr.procedure->data.procedure.frame = dtopen(disc, Dtset)) || !dtview(expr.procedure->data.procedure.frame, expr.program->symbols))
					exerror("out of space [frame table]");
				expr.program->symbols = expr.program->frame = expr.procedure->data.procedure.frame;
			}
    break;
case 5:
#line 186 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			expr.procedure = 0;
			if (expr.program->frame)
			{
				expr.program->symbols = expr.program->frame->view;
				dtview(expr.program->frame, NiL);
			}
			if (exvsp[0].expr && exvsp[0].expr->op == S2B)
			{
				Exnode_t*	x;

				x = exvsp[0].expr;
				exvsp[0].expr = x->data.operand.left;
				x->data.operand.left = 0;
				exfreenode(expr.program, x);
			}
			exvsp[-3].id->value->data.operand.right = excast(expr.program, exvsp[0].expr, exvsp[-3].id->type, NiL, 0);
		}
    break;
case 6:
#line 207 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = 0;
		}
    break;
case 7:
#line 211 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (!exvsp[-1].expr)
				exval.expr = exvsp[0].expr;
			else if (!exvsp[0].expr)
				exval.expr = exvsp[-1].expr;
			else if (exvsp[-1].expr->op == CONSTANT)
			{
				exfreenode(expr.program, exvsp[-1].expr);
				exval.expr = exvsp[0].expr;
			}
			else exval.expr = exnewnode(expr.program, ';', 1, exvsp[0].expr->type, exvsp[-1].expr, exvsp[0].expr);
		}
    break;
case 8:
#line 226 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exvsp[-1].expr;
		}
    break;
case 9:
#line 230 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = (exvsp[-1].expr && exvsp[-1].expr->type == STRING) ? exnewnode(expr.program, S2B, 1, INTEGER, exvsp[-1].expr, NiL) : exvsp[-1].expr;
		}
    break;
case 10:
#line 233 "/home/ellson/graphviz/tools/expr/exparse.y"
{expr.declare=exvsp[0].id->type;}
    break;
case 11:
#line 234 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exvsp[-1].expr;
		}
    break;
case 12:
#line 238 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (exisAssign (exvsp[-3].expr))
				exwarn ("assignment used as boolean in if statement");
			if (exvsp[-3].expr->type == STRING)
				exvsp[-3].expr = exnewnode(expr.program, S2B, 1, INTEGER, exvsp[-3].expr, NiL);
			else if (!INTEGRAL(exvsp[-3].expr->type))
				exvsp[-3].expr = excast(expr.program, exvsp[-3].expr, INTEGER, NiL, 0);
			exval.expr = exnewnode(expr.program, exvsp[-5].id->index, 1, INTEGER, exvsp[-3].expr, exnewnode(expr.program, ':', 1, exvsp[-1].expr ? exvsp[-1].expr->type : 0, exvsp[-1].expr, exvsp[0].expr));
		}
    break;
case 13:
#line 248 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, ITERATE, 0, INTEGER, NiL, NiL);
			exval.expr->data.generate.array = exvsp[-2].expr;
			if (!exvsp[-2].expr->data.variable.index || exvsp[-2].expr->data.variable.index->op != DYNAMIC)
				exerror("simple index variable expected");
			exval.expr->data.generate.index = exvsp[-2].expr->data.variable.index->data.variable.symbol;
			if (exvsp[-2].expr->op == ID && exval.expr->data.generate.index->type != INTEGER)
				exerror("integer index variable expected");
			exfreenode(expr.program, exvsp[-2].expr->data.variable.index);
			exvsp[-2].expr->data.variable.index = 0;
			exval.expr->data.generate.statement = exvsp[0].expr;
		}
    break;
case 14:
#line 261 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (!exvsp[-4].expr)
			{
				exvsp[-4].expr = exnewnode(expr.program, CONSTANT, 0, INTEGER, NiL, NiL);
				exvsp[-4].expr->data.constant.value.integer = 1;
			}
			else if (exvsp[-4].expr->type == STRING)
				exvsp[-4].expr = exnewnode(expr.program, S2B, 1, INTEGER, exvsp[-4].expr, NiL);
			else if (!INTEGRAL(exvsp[-4].expr->type))
				exvsp[-4].expr = excast(expr.program, exvsp[-4].expr, INTEGER, NiL, 0);
			exval.expr = exnewnode(expr.program, exvsp[-8].id->index, 1, INTEGER, exvsp[-4].expr, exnewnode(expr.program, ';', 1, 0, exvsp[-2].expr, exvsp[0].expr));
			if (exvsp[-6].expr)
				exval.expr = exnewnode(expr.program, ';', 1, INTEGER, exvsp[-6].expr, exval.expr);
		}
    break;
case 15:
#line 276 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (exisAssign (exvsp[-2].expr))
				exwarn ("assignment used as boolean in while statement");
			if (exvsp[-2].expr->type == STRING)
				exvsp[-2].expr = exnewnode(expr.program, S2B, 1, INTEGER, exvsp[-2].expr, NiL);
			else if (!INTEGRAL(exvsp[-2].expr->type))
				exvsp[-2].expr = excast(expr.program, exvsp[-2].expr, INTEGER, NiL, 0);
			exval.expr = exnewnode(expr.program, exvsp[-4].id->index, 1, INTEGER, exvsp[-2].expr, exnewnode(expr.program, ';', 1, 0, NiL, exvsp[0].expr));
		}
    break;
case 16:
#line 285 "/home/ellson/graphviz/tools/expr/exparse.y"
{expr.declare=exvsp[0].expr->type;}
    break;
case 17:
#line 286 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			register Switch_t*	sw = expr.swstate;

			exval.expr = exnewnode(expr.program, exvsp[-7].id->index, 1, INTEGER, exvsp[-5].expr, exnewnode(expr.program, DEFAULT, 1, 0, sw->defcase, sw->firstcase));
			expr.swstate = expr.swstate->prev;
			if (sw->base)
				free(sw->base);
			if (sw != &swstate)
				free(sw);
		}
    break;
case 18:
#line 297 "/home/ellson/graphviz/tools/expr/exparse.y"
{
		loopop:
			if (!exvsp[-1].expr)
			{
				exvsp[-1].expr = exnewnode(expr.program, CONSTANT, 0, INTEGER, NiL, NiL);
				exvsp[-1].expr->data.constant.value.integer = 1;
			}
			else if (!INTEGRAL(exvsp[-1].expr->type))
				exvsp[-1].expr = excast(expr.program, exvsp[-1].expr, INTEGER, NiL, 0);
			exval.expr = exnewnode(expr.program, exvsp[-2].id->index, 1, INTEGER, exvsp[-1].expr, NiL);
		}
    break;
case 19:
#line 309 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto loopop;
		}
    break;
case 20:
#line 313 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (exvsp[-1].expr)
			{
				if (expr.procedure && !expr.procedure->type)
					exerror("return in void function");
				exvsp[-1].expr = excast(expr.program, exvsp[-1].expr, expr.procedure ? expr.procedure->type : INTEGER, NiL, 0);
			}
			exval.expr = exnewnode(expr.program, RETURN, 1, exvsp[-1].expr ? exvsp[-1].expr->type : 0, exvsp[-1].expr, NiL);
		}
    break;
case 21:
#line 325 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			register Switch_t*		sw;
			int				n;

			if (expr.swstate)
			{
				if (!(sw = newof(0, Switch_t, 1, 0)))
				{
					exerror("out of space [switch]");
					sw = &swstate;
				}
				sw->prev = expr.swstate;
			}
			else sw = &swstate;
			expr.swstate = sw;
			sw->type = expr.declare;
			sw->firstcase = 0;
			sw->lastcase = 0;
			sw->defcase = 0;
			sw->def = 0;
			n = 8;
			if (!(sw->base = newof(0, Extype_t*, n, 0)))
			{
				exerror("out of space [case]");
				n = 0;
			}
			sw->cur = sw->base;
			sw->last = sw->base + n;
		}
    break;
case 23:
#line 358 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			register Switch_t*	sw = expr.swstate;
			int			n;

			exval.expr = exnewnode(expr.program, CASE, 1, 0, exvsp[0].expr, NiL);
			if (sw->cur > sw->base)
			{
				if (sw->lastcase)
					sw->lastcase->data.select.next = exval.expr;
				else sw->firstcase = exval.expr;
				sw->lastcase = exval.expr;
				n = sw->cur - sw->base;
				sw->cur = sw->base;
				exval.expr->data.select.constant = (Extype_t**)exalloc(expr.program, (n + 1) * sizeof(Extype_t*));
				memcpy(exval.expr->data.select.constant, sw->base, n * sizeof(Extype_t*));
				exval.expr->data.select.constant[n] = 0;
			}
			else exval.expr->data.select.constant = 0;
			if (sw->def)
			{
				sw->def = 0;
				if (sw->defcase)
					exerror("duplicate default in switch");
				else sw->defcase = exvsp[0].expr;
			}
		}
    break;
case 26:
#line 391 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			int	n;

			if (expr.swstate->cur >= expr.swstate->last)
			{
				n = expr.swstate->cur - expr.swstate->base;
				if (!(expr.swstate->base = newof(expr.swstate->base, Extype_t*, 2 * n, 0)))
				{
					exerror("too many case labels for switch");
					n = 0;
				}
				expr.swstate->cur = expr.swstate->base + n;
				expr.swstate->last = expr.swstate->base + 2 * n;
			}
			if (expr.swstate->cur)
			{
				exvsp[-1].expr = excast(expr.program, exvsp[-1].expr, expr.swstate->type, NiL, 0);
				*expr.swstate->cur++ = &(exvsp[-1].expr->data.constant.value);
			}
		}
    break;
case 27:
#line 412 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			expr.swstate->def = 1;
		}
    break;
case 29:
#line 419 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (exvsp[0].expr)
				exval.expr = exvsp[-2].expr ? exnewnode(expr.program, ',', 1, exvsp[0].expr->type, exvsp[-2].expr, exvsp[0].expr) : exvsp[0].expr;
		}
    break;
case 30:
#line 425 "/home/ellson/graphviz/tools/expr/exparse.y"
{checkName (exvsp[0].id); expr.id=exvsp[0].id;}
    break;
case 31:
#line 426 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = 0;
			exvsp[-3].id->type = expr.declare;
			if (exvsp[0].expr && exvsp[0].expr->op == PROCEDURE)
			{
				exvsp[-3].id->lex = PROCEDURE;
				exvsp[-3].id->value = exvsp[0].expr;
			}
			else
			{
				exvsp[-3].id->lex = DYNAMIC;
				exvsp[-3].id->value = exnewnode(expr.program, 0, 0, 0, NiL, NiL);
				if (exvsp[-1].integer && !exvsp[-3].id->local.pointer)
				{
					Dtdisc_t*	disc;

					if (!(disc = newof(0, Dtdisc_t, 1, 0)))
						exerror("out of space [associative array]");
					if (exvsp[-1].integer == INTEGER) {
						disc->key = offsetof(Exassoc_t, key);
						disc->size = sizeof(Extype_t);
						disc->comparf = (Dtcompar_f)cmpKey;
					}
					else {
						disc->key = offsetof(Exassoc_t, name);
					}
					if (!(exvsp[-3].id->local.pointer = (char*)dtopen(disc, Dtoset)))
						exerror("%s: cannot initialize associative array", exvsp[-3].id->name);
					exvsp[-3].id->index_type = exvsp[-1].integer; /* -1 indicates no typechecking */
				}
				if (exvsp[0].expr)
				{
					if (exvsp[0].expr->type != exvsp[-3].id->type)
					{
						exvsp[0].expr->type = exvsp[-3].id->type;
						exvsp[0].expr->data.operand.right = excast(expr.program, exvsp[0].expr->data.operand.right, exvsp[-3].id->type, NiL, 0);
					}
					exvsp[0].expr->data.operand.left = exnewnode(expr.program, DYNAMIC, 0, exvsp[-3].id->type, NiL, NiL);
					exvsp[0].expr->data.operand.left->data.variable.symbol = exvsp[-3].id;
					exval.expr = exvsp[0].expr;
				}
				else if (!exvsp[-1].integer)
					exvsp[-3].id->value->data.value = exzero(exvsp[-3].id->type);
			}
		}
    break;
case 38:
#line 484 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = 0;
		}
    break;
case 39:
#line 488 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exvsp[0].expr;
		}
    break;
case 40:
#line 494 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = 0;
		}
    break;
case 42:
#line 501 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exvsp[-1].expr;
		}
    break;
case 43:
#line 505 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = (exvsp[0].expr->type == exvsp[-2].id->type) ? exvsp[0].expr : excast(expr.program, exvsp[0].expr, exvsp[-2].id->type, NiL, 0);
		}
    break;
case 44:
#line 509 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			int	rel;

		relational:
			rel = INTEGER;
			goto coerce;
		binary:
			rel = 0;
		coerce:
			if (!exvsp[-2].expr->type)
			{
				if (!exvsp[0].expr->type)
					exvsp[-2].expr->type = exvsp[0].expr->type = rel ? STRING : INTEGER;
				else exvsp[-2].expr->type = exvsp[0].expr->type;
			}
			else if (!exvsp[0].expr->type) exvsp[0].expr->type = exvsp[-2].expr->type;
			if (exvsp[-2].expr->type != exvsp[0].expr->type)
			{
				if (exvsp[-2].expr->type == STRING)
					exvsp[-2].expr = excast(expr.program, exvsp[-2].expr, exvsp[0].expr->type, exvsp[0].expr, 0);
				else if (exvsp[0].expr->type == STRING)
					exvsp[0].expr = excast(expr.program, exvsp[0].expr, exvsp[-2].expr->type, exvsp[-2].expr, 0);
				else if (exvsp[-2].expr->type == FLOATING)
					exvsp[0].expr = excast(expr.program, exvsp[0].expr, FLOATING, exvsp[-2].expr, 0);
				else if (exvsp[0].expr->type == FLOATING)
					exvsp[-2].expr = excast(expr.program, exvsp[-2].expr, FLOATING, exvsp[0].expr, 0);
			}
			if (!rel)
				rel = (exvsp[-2].expr->type == STRING) ? STRING : ((exvsp[-2].expr->type == UNSIGNED) ? UNSIGNED : exvsp[0].expr->type);
			exval.expr = exnewnode(expr.program, exvsp[-1].op, 1, rel, exvsp[-2].expr, exvsp[0].expr);
			if (!expr.program->errors && exvsp[-2].expr->op == CONSTANT && exvsp[0].expr->op == CONSTANT)
			{
				expr.program->vc = expr.program->vm;
				exval.expr->data.constant.value = exeval(expr.program, exval.expr, NiL);
				expr.program->vc = expr.program->ve;
				exval.expr->binary = 0;
				exval.expr->op = CONSTANT;
				exfreenode(expr.program, exvsp[-2].expr);
				exfreenode(expr.program, exvsp[0].expr);
			}
			else if (!BUILTIN(exvsp[-2].expr->type) || !BUILTIN(exvsp[0].expr->type)) {
				checkBinary(expr.program, exvsp[-2].expr, exval.expr, exvsp[0].expr);
			}
		}
    break;
case 45:
#line 554 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto binary;
		}
    break;
case 46:
#line 558 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto binary;
		}
    break;
case 47:
#line 562 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto binary;
		}
    break;
case 48:
#line 566 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto binary;
		}
    break;
case 49:
#line 570 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto binary;
		}
    break;
case 50:
#line 574 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto binary;
		}
    break;
case 51:
#line 578 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto relational;
		}
    break;
case 52:
#line 582 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto relational;
		}
    break;
case 53:
#line 586 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto relational;
		}
    break;
case 54:
#line 590 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto relational;
		}
    break;
case 55:
#line 594 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto relational;
		}
    break;
case 56:
#line 598 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto binary;
		}
    break;
case 57:
#line 602 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto binary;
		}
    break;
case 58:
#line 606 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto binary;
		}
    break;
case 59:
#line 610 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto binary;
		}
    break;
case 60:
#line 614 "/home/ellson/graphviz/tools/expr/exparse.y"
{
		logical:
			if (exvsp[-2].expr->type == STRING)
				exvsp[-2].expr = exnewnode(expr.program, S2B, 1, INTEGER, exvsp[-2].expr, NiL);
			else if (!BUILTIN(exvsp[-2].expr->type))
				exvsp[-2].expr = excast(expr.program, exvsp[-2].expr, INTEGER, NiL, 0);
			if (exvsp[0].expr->type == STRING)
				exvsp[0].expr = exnewnode(expr.program, S2B, 1, INTEGER, exvsp[0].expr, NiL);
			else if (!BUILTIN(exvsp[0].expr->type))
				exvsp[0].expr = excast(expr.program, exvsp[0].expr, INTEGER, NiL, 0);
			goto binary;
		}
    break;
case 61:
#line 627 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto logical;
		}
    break;
case 62:
#line 631 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (exvsp[-2].expr->op == CONSTANT)
			{
				exfreenode(expr.program, exvsp[-2].expr);
				exval.expr = exvsp[0].expr;
			}
			else exval.expr = exnewnode(expr.program, ',', 1, exvsp[0].expr->type, exvsp[-2].expr, exvsp[0].expr);
		}
    break;
case 63:
#line 639 "/home/ellson/graphviz/tools/expr/exparse.y"
{expr.nolabel=1;}
    break;
case 64:
#line 639 "/home/ellson/graphviz/tools/expr/exparse.y"
{expr.nolabel=0;}
    break;
case 65:
#line 640 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (!exvsp[-3].expr->type)
			{
				if (!exvsp[0].expr->type)
					exvsp[-3].expr->type = exvsp[0].expr->type = INTEGER;
				else exvsp[-3].expr->type = exvsp[0].expr->type;
			}
			else if (!exvsp[0].expr->type)
				exvsp[0].expr->type = exvsp[-3].expr->type;
			if (exvsp[-6].expr->type == STRING)
				exvsp[-6].expr = exnewnode(expr.program, S2B, 1, INTEGER, exvsp[-6].expr, NiL);
			else if (!INTEGRAL(exvsp[-6].expr->type))
				exvsp[-6].expr = excast(expr.program, exvsp[-6].expr, INTEGER, NiL, 0);
			if (exvsp[-3].expr->type != exvsp[0].expr->type)
			{
				if (exvsp[-3].expr->type == STRING || exvsp[0].expr->type == STRING)
					exerror("if statement string type mismatch");
				else if (exvsp[-3].expr->type == FLOATING)
					exvsp[0].expr = excast(expr.program, exvsp[0].expr, FLOATING, NiL, 0);
				else if (exvsp[0].expr->type == FLOATING)
					exvsp[-3].expr = excast(expr.program, exvsp[-3].expr, FLOATING, NiL, 0);
			}
			if (exvsp[-6].expr->op == CONSTANT)
			{
				if (exvsp[-6].expr->data.constant.value.integer)
				{
					exval.expr = exvsp[-3].expr;
					exfreenode(expr.program, exvsp[0].expr);
				}
				else
				{
					exval.expr = exvsp[0].expr;
					exfreenode(expr.program, exvsp[-3].expr);
				}
				exfreenode(expr.program, exvsp[-6].expr);
			}
			else exval.expr = exnewnode(expr.program, '?', 1, exvsp[-3].expr->type, exvsp[-6].expr, exnewnode(expr.program, ':', 1, exvsp[-3].expr->type, exvsp[-3].expr, exvsp[0].expr));
		}
    break;
case 66:
#line 679 "/home/ellson/graphviz/tools/expr/exparse.y"
{
		iunary:
			if (exvsp[0].expr->type == STRING)
				exvsp[0].expr = exnewnode(expr.program, S2B, 1, INTEGER, exvsp[0].expr, NiL);
			else if (!INTEGRAL(exvsp[0].expr->type))
				exvsp[0].expr = excast(expr.program, exvsp[0].expr, INTEGER, NiL, 0);
		unary:
			exval.expr = exnewnode(expr.program, exvsp[-1].op, 1, exvsp[0].expr->type == UNSIGNED ? INTEGER : exvsp[0].expr->type, exvsp[0].expr, NiL);
			if (exvsp[0].expr->op == CONSTANT)
			{
				exval.expr->data.constant.value = exeval(expr.program, exval.expr, NiL);
				exval.expr->binary = 0;
				exval.expr->op = CONSTANT;
				exfreenode(expr.program, exvsp[0].expr);
			}
			else if (!BUILTIN(exvsp[0].expr->type)) {
				checkBinary(expr.program, exvsp[0].expr, exval.expr, 0);
			}
		}
    break;
case 67:
#line 699 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto iunary;
		}
    break;
case 68:
#line 703 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto unary;
		}
    break;
case 69:
#line 707 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exvsp[0].expr;
		}
    break;
case 70:
#line 711 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, ARRAY, 1, T(exvsp[-3].id->type), call(0, exvsp[-3].id, exvsp[-1].expr), exvsp[-1].expr);
		}
    break;
case 71:
#line 715 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, FUNCTION, 1, T(exvsp[-3].id->type), call(0, exvsp[-3].id, exvsp[-1].expr), exvsp[-1].expr);
		}
    break;
case 72:
#line 719 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewsub (expr.program, exvsp[-1].expr, GSUB);
		}
    break;
case 73:
#line 723 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewsub (expr.program, exvsp[-1].expr, SUB);
		}
    break;
case 74:
#line 727 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewsubstr (expr.program, exvsp[-1].expr);
		}
    break;
case 75:
#line 731 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (!INTEGRAL(exvsp[-1].expr->type))
				exvsp[-1].expr = excast(expr.program, exvsp[-1].expr, INTEGER, NiL, 0);
			exval.expr = exnewnode(expr.program, EXIT, 1, INTEGER, exvsp[-1].expr, NiL);
		}
    break;
case 76:
#line 737 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, RAND, 0, FLOATING, NiL, NiL);
		}
    break;
case 77:
#line 741 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, SRAND, 0, INTEGER, NiL, NiL);
		}
    break;
case 78:
#line 745 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (!INTEGRAL(exvsp[-1].expr->type))
				exvsp[-1].expr = excast(expr.program, exvsp[-1].expr, INTEGER, NiL, 0);
			exval.expr = exnewnode(expr.program, SRAND, 1, INTEGER, exvsp[-1].expr, NiL);
		}
    break;
case 79:
#line 751 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, CALL, 1, exvsp[-3].id->type, NiL, exvsp[-1].expr);
			exval.expr->data.call.procedure = exvsp[-3].id;
		}
    break;
case 80:
#line 756 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exprint(expr.program, exvsp[-3].id, exvsp[-1].expr);
		}
    break;
case 81:
#line 760 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, exvsp[-3].id->index, 0, exvsp[-3].id->type, NiL, NiL);
			if (exvsp[-1].expr && exvsp[-1].expr->data.operand.left->type == INTEGER)
			{
				exval.expr->data.print.descriptor = exvsp[-1].expr->data.operand.left;
				exvsp[-1].expr = exvsp[-1].expr->data.operand.right;
			}
			else switch (exvsp[-3].id->index)
			{
			case QUERY:
				exval.expr->data.print.descriptor = exnewnode(expr.program, CONSTANT, 0, INTEGER, NiL, NiL);
				exval.expr->data.print.descriptor->data.constant.value.integer = 2;
				break;
			case PRINTF:
				exval.expr->data.print.descriptor = exnewnode(expr.program, CONSTANT, 0, INTEGER, NiL, NiL);
				exval.expr->data.print.descriptor->data.constant.value.integer = 1;
				break;
			case SPRINTF:
				exval.expr->data.print.descriptor = 0;
				break;
			}
			exval.expr->data.print.args = preprint(exvsp[-1].expr);
		}
    break;
case 82:
#line 784 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			if (exvsp[0].expr)
			{
				if (exvsp[-1].expr->op == ID && !expr.program->disc->setf)
					exerror("%s: variable assignment not supported", exvsp[-1].expr->data.variable.symbol->name);
				else
				{
					if (!exvsp[-1].expr->type)
						exvsp[-1].expr->type = exvsp[0].expr->type;
#if 0
					else if (exvsp[0].expr->type != exvsp[-1].expr->type && exvsp[-1].expr->type >= 0200)
#else
					else if (exvsp[0].expr->type != exvsp[-1].expr->type)
#endif
					{
						exvsp[0].expr->type = exvsp[-1].expr->type;
						exvsp[0].expr->data.operand.right = excast(expr.program, exvsp[0].expr->data.operand.right, exvsp[-1].expr->type, NiL, 0);
					}
					exvsp[0].expr->data.operand.left = exvsp[-1].expr;
					exval.expr = exvsp[0].expr;
				}
			}
		}
    break;
case 83:
#line 808 "/home/ellson/graphviz/tools/expr/exparse.y"
{
		pre:
			if (exvsp[0].expr->type == STRING)
				exerror("++ and -- invalid for string variables");
			exval.expr = exnewnode(expr.program, exvsp[-1].op, 0, exvsp[0].expr->type, exvsp[0].expr, NiL);
			exval.expr->subop = PRE;
		}
    break;
case 84:
#line 816 "/home/ellson/graphviz/tools/expr/exparse.y"
{
		pos:
			if (exvsp[-1].expr->type == STRING)
				exerror("++ and -- invalid for string variables");
			exval.expr = exnewnode(expr.program, exvsp[0].op, 0, exvsp[-1].expr->type, exvsp[-1].expr, NiL);
			exval.expr->subop = POS;
		}
    break;
case 85:
#line 824 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto pre;
		}
    break;
case 86:
#line 828 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			goto pos;
		}
    break;
case 88:
#line 835 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, CONSTANT, 0, exvsp[0].id->type, NiL, NiL);
			if (!expr.program->disc->reff)
				exerror("%s: identifier references not supported", exvsp[0].id->name);
			else exval.expr->data.constant.value = (*expr.program->disc->reff)(expr.program, exval.expr, exvsp[0].id, NiL, NiL, EX_SCALAR, expr.program->disc);
		}
    break;
case 89:
#line 842 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, CONSTANT, 0, FLOATING, NiL, NiL);
			exval.expr->data.constant.value.floating = exvsp[0].floating;
		}
    break;
case 90:
#line 847 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, CONSTANT, 0, INTEGER, NiL, NiL);
			exval.expr->data.constant.value.integer = exvsp[0].integer;
		}
    break;
case 91:
#line 852 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, CONSTANT, 0, STRING, NiL, NiL);
			exval.expr->data.constant.value.string = exvsp[0].string;
		}
    break;
case 92:
#line 857 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, CONSTANT, 0, UNSIGNED, NiL, NiL);
			exval.expr->data.constant.value.integer = exvsp[0].integer;
		}
    break;
case 96:
#line 869 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = makeVar(expr.program, exvsp[-1].id, 0, 0, exvsp[0].reference);
		}
    break;
case 97:
#line 873 "/home/ellson/graphviz/tools/expr/exparse.y"
{
            Exnode_t*   n;

            n = exnewnode(expr.program, DYNAMIC, 0, exvsp[-2].id->type, NiL, NiL);
            n->data.variable.symbol = exvsp[-2].id;
            n->data.variable.reference = 0;
            if (((n->data.variable.index = exvsp[-1].expr) == 0) != (exvsp[-2].id->local.pointer == 0))
              exerror("%s: is%s an array", exvsp[-2].id->name, exvsp[-2].id->local.pointer ? "" : " not");
			if (exvsp[-2].id->local.pointer && (exvsp[-2].id->index_type > 0)) {
				if (exvsp[-1].expr->type != exvsp[-2].id->index_type)
            		exerror("%s: indices must have type %s", 
						exvsp[-2].id->name, extypename(expr.program, exvsp[-2].id->index_type));
			}
			if (exvsp[0].reference) {
              n->data.variable.dyna =exnewnode(expr.program, 0, 0, 0, NiL, NiL);
              exval.expr = makeVar(expr.program, exvsp[-2].id, exvsp[-1].expr, n, exvsp[0].reference);
            }
            else exval.expr = n;
		}
    break;
case 98:
#line 893 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, ID, 0, STRING, NiL, NiL);
			exval.expr->data.variable.symbol = exvsp[0].id;
			exval.expr->data.variable.reference = 0;
			exval.expr->data.variable.index = 0;
			exval.expr->data.variable.dyna = 0;
			if (!(expr.program->disc->flags & EX_UNDECLARED))
				exerror("unknown identifier");
		}
    break;
case 99:
#line 905 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.integer = 0;
		}
    break;
case 100:
#line 909 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.integer = -1;
		}
    break;
case 101:
#line 913 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			/* If DECLARE is VOID, its type is 0, so this acts like
			 * the empty case.
			 */
			if (INTEGRAL(exvsp[-1].id->type))
				exval.integer = INTEGER;
			else
				exval.integer = exvsp[-1].id->type;
				
		}
    break;
case 102:
#line 926 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = 0;
		}
    break;
case 103:
#line 930 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exvsp[-1].expr;
		}
    break;
case 104:
#line 936 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = 0;
		}
    break;
case 105:
#line 940 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exvsp[0].expr->data.operand.left;
			exvsp[0].expr->data.operand.left = exvsp[0].expr->data.operand.right = 0;
			exfreenode(expr.program, exvsp[0].expr);
		}
    break;
case 106:
#line 948 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, ',', 1, 0, exnewnode(expr.program, ',', 1, exvsp[0].expr->type, exvsp[0].expr, NiL), NiL);
			exval.expr->data.operand.right = exval.expr->data.operand.left;
		}
    break;
case 107:
#line 953 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exvsp[-2].expr->data.operand.right = exvsp[-2].expr->data.operand.right->data.operand.right = exnewnode(expr.program, ',', 1, exvsp[-2].expr->type, exvsp[0].expr, NiL);
		}
    break;
case 108:
#line 959 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = 0;
		}
    break;
case 109:
#line 963 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = 0;
			if (exvsp[0].id->type)
				exerror("(void) expected");
		}
    break;
case 111:
#line 972 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, ',', 1, exvsp[0].expr->type, exvsp[0].expr, NiL);
		}
    break;
case 112:
#line 976 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			register Exnode_t*	x;
			register Exnode_t*	y;

			exval.expr = exvsp[-2].expr;
			for (x = exvsp[-2].expr; (y = x->data.operand.right); x = y);
			x->data.operand.right = exnewnode(expr.program, ',', 1, exvsp[0].expr->type, exvsp[0].expr, NiL);
		}
    break;
case 113:
#line 986 "/home/ellson/graphviz/tools/expr/exparse.y"
{expr.declare=exvsp[0].id->type;}
    break;
case 114:
#line 987 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, ID, 0, exvsp[0].id->type, NiL, NiL);
			exval.expr->data.variable.symbol = exvsp[0].id;
			exvsp[0].id->lex = DYNAMIC;
			exvsp[0].id->value = exnewnode(expr.program, 0, 0, 0, NiL, NiL);
			expr.procedure->data.procedure.arity++;
		}
    break;
case 115:
#line 997 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.reference = expr.refs = expr.lastref = 0;
		}
    break;
case 116:
#line 1001 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			Exref_t*	r;

			r = ALLOCATE(expr.program, Exref_t);
			r->symbol = exvsp[0].id;
			expr.refs = r;
			expr.lastref = r;
			r->next = 0;
			r->index = 0;
			exval.reference = expr.refs;
		}
    break;
case 117:
#line 1013 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			Exref_t*	r;
			Exref_t*	l;

			r = ALLOCATE(expr.program, Exref_t);
			r->symbol = exvsp[0].id;
			r->index = 0;
			r->next = 0;
			l = ALLOCATE(expr.program, Exref_t);
			l->symbol = exvsp[-1].id;
			l->index = 0;
			l->next = r;
			expr.refs = l;
			expr.lastref = r;
			exval.reference = expr.refs;
        }
    break;
case 118:
#line 1032 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.id = exvsp[0].id;
		}
    break;
case 119:
#line 1036 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.id = exvsp[0].id;
		}
    break;
case 120:
#line 1042 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = 0;
		}
    break;
case 121:
#line 1046 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = exnewnode(expr.program, '=', 1, exvsp[0].expr->type, NiL, exvsp[0].expr);
			exval.expr->subop = exvsp[-1].op;
		}
    break;
case 123:
#line 1053 "/home/ellson/graphviz/tools/expr/exparse.y"
{
				register Dtdisc_t*	disc;

				if (expr.procedure)
					exerror("no nested function definitions");
				expr.procedure = exnewnode(expr.program, PROCEDURE, 1, expr.declare, NiL, NiL);
				if (!(disc = newof(0, Dtdisc_t, 1, 0)))
					exerror("out of space [frame discipline]");
				disc->key = offsetof(Exid_t, name);
				if (!(expr.procedure->data.procedure.frame = dtopen(disc, Dtset)) || !dtview(expr.procedure->data.procedure.frame, expr.program->symbols))
					exerror("out of space [frame table]");
				expr.program->symbols = expr.program->frame = expr.procedure->data.procedure.frame;
				expr.program->formals = 1;
			}
    break;
case 124:
#line 1066 "/home/ellson/graphviz/tools/expr/exparse.y"
{
				expr.program->formals = 0;
				expr.id->lex = PROCEDURE;
				expr.id->type = expr.declare;
			}
    break;
case 125:
#line 1071 "/home/ellson/graphviz/tools/expr/exparse.y"
{
			exval.expr = expr.procedure;
			expr.procedure = 0;
			if (expr.program->frame)
			{
				expr.program->symbols = expr.program->frame->view;
				dtview(expr.program->frame, NiL);
			}
			exval.expr->data.operand.left = exvsp[-5].expr;
			exval.expr->data.operand.right = excast(expr.program, exvsp[-1].expr, exval.expr->type, NiL, 0);

			/*
			 * NOTE: procedure definition was slipped into the
			 *	 declaration initializer statement production,
			 *	 therefore requiring the statement terminator
			 */

			exunlex(expr.program, ';');
		}
    break;
}

#line 705 "/usr/share/bison/bison.simple"


  exvsp -= exlen;
  exssp -= exlen;
#if EXLSP_NEEDED
  exlsp -= exlen;
#endif

#if EXDEBUG
  if (exdebug)
    {
      short *exssp1 = exss - 1;
      EXFPRINTF (sfstderr, "state stack now");
      while (exssp1 != exssp)
	EXFPRINTF (sfstderr, " %d", *++exssp1);
      EXFPRINTF (sfstderr, "\n");
    }
#endif

  *++exvsp = exval;
#if EXLSP_NEEDED
  *++exlsp = exloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  exn = exr1[exn];

  exstate = expgoto[exn - EXNTBASE] + *exssp;
  if (exstate >= 0 && exstate <= EXLAST && excheck[exstate] == *exssp)
    exstate = extable[exstate];
  else
    exstate = exdefgoto[exn - EXNTBASE];

  goto exnewstate;


/*------------------------------------.
| exerrlab -- here on detecting error |
`------------------------------------*/
exerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!exerrstatus)
    {
      ++exnerrs;

#ifdef EXERROR_VERBOSE
      exn = expact[exstate];

      if (exn > EXFLAG && exn < EXLAST)
	{
	  EXSIZE_T exsize = 0;
	  char *exmsg;
	  int exx, excount;

	  excount = 0;
	  /* Start EXX at -EXN if negative to avoid negative indexes in
	     EXCHECK.  */
	  for (exx = exn < 0 ? -exn : 0;
	       exx < (int) (sizeof (extname) / sizeof (char *)); exx++)
	    if (excheck[exx + exn] == exx)
	      exsize += exstrlen (extname[exx]) + 15, excount++;
	  exsize += exstrlen ("parse error, unexpected ") + 1;
	  exsize += exstrlen (extname[EXTRANSLATE (exchar)]);
	  exmsg = (char *) EXSTACK_ALLOC (exsize);
	  if (exmsg != 0)
	    {
	      char *exp = exstpcpy (exmsg, "parse error, unexpected ");
	      exp = exstpcpy (exp, extname[EXTRANSLATE (exchar)]);

	      if (excount < 5)
		{
		  excount = 0;
		  for (exx = exn < 0 ? -exn : 0;
		       exx < (int) (sizeof (extname) / sizeof (char *));
		       exx++)
		    if (excheck[exx + exn] == exx)
		      {
			const char *exq = ! excount ? ", expecting " : " or ";
			exp = exstpcpy (exp, exq);
			exp = exstpcpy (exp, extname[exx]);
			excount++;
		      }
		}
	      exerror (exmsg);
	      EXSTACK_FREE (exmsg);
	    }
	  else
	    exerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (EXERROR_VERBOSE) */
	exerror ("parse error");
    }
  goto exerrlab1;


/*--------------------------------------------------.
| exerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
exerrlab1:
  if (exerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (exchar == EXEOF)
	EXABORT;
      EXDPRINTF ((sfstderr, "Discarding token %d (%s).\n",
		  exchar, extname[exchar1]));
      exchar = EXEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  exerrstatus = 3;		/* Each real token shifted decrements this */

  goto exerrhandle;


/*-------------------------------------------------------------------.
| exerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
exerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  exn = exdefact[exstate];
  if (exn)
    goto exdefault;
#endif


/*---------------------------------------------------------------.
| exerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
exerrpop:
  if (exssp == exss)
    EXABORT;
  exvsp--;
  exstate = *--exssp;
#if EXLSP_NEEDED
  exlsp--;
#endif

#if EXDEBUG
  if (exdebug)
    {
      short *exssp1 = exss - 1;
      EXFPRINTF (sfstderr, "Error: state stack now");
      while (exssp1 != exssp)
	EXFPRINTF (sfstderr, " %d", *++exssp1);
      EXFPRINTF (sfstderr, "\n");
    }
#endif

/*--------------.
| exerrhandle.  |
`--------------*/
exerrhandle:
  exn = expact[exstate];
  if (exn == EXFLAG)
    goto exerrdefault;

  exn += EXTERROR;
  if (exn < 0 || exn > EXLAST || excheck[exn] != EXTERROR)
    goto exerrdefault;

  exn = extable[exn];
  if (exn < 0)
    {
      if (exn == EXFLAG)
	goto exerrpop;
      exn = -exn;
      goto exreduce;
    }
  else if (exn == 0)
    goto exerrpop;

  if (exn == EXFINAL)
    EXACCEPT;

  EXDPRINTF ((sfstderr, "Shifting error token, "));

  *++exvsp = exlval;
#if EXLSP_NEEDED
  *++exlsp = exlloc;
#endif

  exstate = exn;
  goto exnewstate;


/*-------------------------------------.
| exacceptlab -- EXACCEPT comes here.  |
`-------------------------------------*/
exacceptlab:
  exresult = 0;
  goto exreturn;

/*-----------------------------------.
| exabortlab -- EXABORT comes here.  |
`-----------------------------------*/
exabortlab:
  exresult = 1;
  goto exreturn;

/*---------------------------------------------.
| exoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
exoverflowlab:
  exerror ("parser stack overflow");
  exresult = 2;
  /* Fall through.  */

exreturn:
#ifndef exoverflow
  if (exss != exssa)
    EXSTACK_FREE (exss);
#endif
  return exresult;
}
#line 1092 "/home/ellson/graphviz/tools/expr/exparse.y"


#include "exgram.h"
