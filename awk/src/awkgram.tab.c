/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     FIRSTTOKEN = 258,
     PROGRAM = 259,
     PASTAT = 260,
     PASTAT2 = 261,
     XBEGIN = 262,
     XEND = 263,
     NL = 264,
     ARRAY = 265,
     MATCH = 266,
     NOTMATCH = 267,
     MATCHOP = 268,
     FINAL = 269,
     DOT = 270,
     ALL = 271,
     CCL = 272,
     NCCL = 273,
     CHAR = 274,
     OR = 275,
     STAR = 276,
     QUEST = 277,
     PLUS = 278,
     EMPTYRE = 279,
     ZERO = 280,
     IGNORE_PRIOR_ATOM = 281,
     AND = 282,
     BOR = 283,
     APPEND = 284,
     EQ = 285,
     GE = 286,
     GT = 287,
     LE = 288,
     LT = 289,
     NE = 290,
     IN = 291,
     ARG = 292,
     BLTIN = 293,
     BREAK = 294,
     CLOSE = 295,
     CONTINUE = 296,
     DELETE = 297,
     DO = 298,
     EXIT = 299,
     FOR = 300,
     FUNC = 301,
     SUB = 302,
     GSUB = 303,
     IF = 304,
     INDEX = 305,
     LSUBSTR = 306,
     MATCHFCN = 307,
     NEXT = 308,
     NEXTFILE = 309,
     ADD = 310,
     MINUS = 311,
     MULT = 312,
     DIVIDE = 313,
     MOD = 314,
     ASSIGN = 315,
     ASGNOP = 316,
     ADDEQ = 317,
     SUBEQ = 318,
     MULTEQ = 319,
     DIVEQ = 320,
     MODEQ = 321,
     POWEQ = 322,
     PRINT = 323,
     PRINTF = 324,
     SPRINTF = 325,
     ELSE = 326,
     INTEST = 327,
     CONDEXPR = 328,
     POSTINCR = 329,
     PREINCR = 330,
     POSTDECR = 331,
     PREDECR = 332,
     VAR = 333,
     IVAR = 334,
     VARNF = 335,
     CALL = 336,
     NUMBER = 337,
     STRING = 338,
     REGEXPR = 339,
     GETLINE = 340,
     SUBSTR = 341,
     SPLIT = 342,
     RETURN = 343,
     WHILE = 344,
     CAT = 345,
     UPLUS = 346,
     UMINUS = 347,
     NOT = 348,
     POWER = 349,
     INCR = 350,
     DECR = 351,
     INDIRECT = 352,
     LASTTOKEN = 353
   };
#endif
/* Tokens.  */
#define FIRSTTOKEN 258
#define PROGRAM 259
#define PASTAT 260
#define PASTAT2 261
#define XBEGIN 262
#define XEND 263
#define NL 264
#define ARRAY 265
#define MATCH 266
#define NOTMATCH 267
#define MATCHOP 268
#define FINAL 269
#define DOT 270
#define ALL 271
#define CCL 272
#define NCCL 273
#define CHAR 274
#define OR 275
#define STAR 276
#define QUEST 277
#define PLUS 278
#define EMPTYRE 279
#define ZERO 280
#define IGNORE_PRIOR_ATOM 281
#define AND 282
#define BOR 283
#define APPEND 284
#define EQ 285
#define GE 286
#define GT 287
#define LE 288
#define LT 289
#define NE 290
#define IN 291
#define ARG 292
#define BLTIN 293
#define BREAK 294
#define CLOSE 295
#define CONTINUE 296
#define DELETE 297
#define DO 298
#define EXIT 299
#define FOR 300
#define FUNC 301
#define SUB 302
#define GSUB 303
#define IF 304
#define INDEX 305
#define LSUBSTR 306
#define MATCHFCN 307
#define NEXT 308
#define NEXTFILE 309
#define ADD 310
#define MINUS 311
#define MULT 312
#define DIVIDE 313
#define MOD 314
#define ASSIGN 315
#define ASGNOP 316
#define ADDEQ 317
#define SUBEQ 318
#define MULTEQ 319
#define DIVEQ 320
#define MODEQ 321
#define POWEQ 322
#define PRINT 323
#define PRINTF 324
#define SPRINTF 325
#define ELSE 326
#define INTEST 327
#define CONDEXPR 328
#define POSTINCR 329
#define PREINCR 330
#define POSTDECR 331
#define PREDECR 332
#define VAR 333
#define IVAR 334
#define VARNF 335
#define CALL 336
#define NUMBER 337
#define STRING 338
#define REGEXPR 339
#define GETLINE 340
#define SUBSTR 341
#define SPLIT 342
#define RETURN 343
#define WHILE 344
#define CAT 345
#define UPLUS 346
#define UMINUS 347
#define NOT 348
#define POWER 349
#define INCR 350
#define DECR 351
#define INDIRECT 352
#define LASTTOKEN 353




/* Copy the first part of user declarations.  */
#line 25 "awkgram.y"

#include <stdio.h>
#include <string.h>
#include "awk.h"

void checkdup(Node *list, Cell *item);
int yywrap(void) { return(1); }

Node	*beginloc = 0;
Node	*endloc = 0;
bool	infunc	= false;	/* = true if in arglist or body of func */
int	inloop	= 0;	/* >= 1 if in while, for, do; can't be bool, since loops can next */
char	*curfname = 0;	/* current function name */
Node	*arglist = 0;	/* list of args for current function */


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 41 "awkgram.y"
{
	Node	*p;
	Cell	*cp;
	int	i;
	char	*s;
}
/* Line 193 of yacc.c.  */
#line 315 "awkgram.tab.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 328 "awkgram.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
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
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  8
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   5004

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  115
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  50
/* YYNRULES -- Number of rules.  */
#define YYNRULES  188
/* YYNRULES -- Number of states.  */
#define YYNSTATES  372

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   353

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,   106,     2,     2,
      12,    16,   105,   103,     9,   104,     2,    15,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    96,    14,
       2,     2,     2,    95,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    18,     2,    19,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    11,    13,    17,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,    10,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      97,    98,    99,   100,   101,   102,   107,   108,   109,   110,
     111,   112,   113,   114
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     7,     9,    12,    14,    17,    19,
      22,    24,    27,    29,    32,    33,    46,    47,    58,    59,
      68,    70,    72,    77,    79,    82,    84,    87,    88,    90,
      91,    93,    94,    96,    98,   102,   104,   106,   111,   116,
     124,   128,   133,   138,   139,   149,   151,   155,   157,   161,
     165,   171,   175,   179,   183,   187,   191,   197,   200,   202,
     204,   208,   214,   218,   222,   226,   230,   234,   238,   242,
     246,   250,   254,   258,   264,   269,   273,   276,   278,   280,
     284,   288,   290,   294,   295,   297,   301,   303,   305,   307,
     309,   312,   315,   317,   320,   322,   325,   326,   331,   333,
     336,   341,   346,   351,   354,   360,   363,   365,   367,   369,
     372,   375,   378,   379,   380,   390,   394,   397,   399,   404,
     407,   411,   414,   417,   421,   424,   427,   428,   432,   435,
     437,   440,   442,   444,   446,   449,   454,   458,   462,   466,
     470,   474,   478,   482,   485,   488,   491,   495,   500,   502,
     506,   511,   514,   517,   520,   523,   526,   531,   535,   538,
     540,   547,   554,   558,   565,   572,   574,   583,   592,   599,
     604,   606,   613,   620,   629,   638,   647,   654,   656,   658,
     663,   665,   668,   669,   671,   675,   677,   679,   681
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     116,     0,    -1,   133,    -1,     1,    -1,    37,    -1,   117,
      10,    -1,    38,    -1,   118,    10,    -1,     9,    -1,   119,
      10,    -1,    53,    -1,   120,    10,    -1,    81,    -1,   121,
      10,    -1,    -1,    55,    12,   132,    14,   130,   140,    14,
     130,   132,   150,   123,   153,    -1,    -1,    55,    12,   132,
      14,    14,   130,   132,   150,   124,   153,    -1,    -1,    55,
      12,   163,    46,   163,   150,   125,   153,    -1,    88,    -1,
      91,    -1,    59,    12,   140,   150,    -1,    11,    -1,   128,
      10,    -1,    10,    -1,   129,    10,    -1,    -1,   129,    -1,
      -1,   145,    -1,    -1,   151,    -1,   131,    -1,   131,   137,
     131,    -1,   140,    -1,   134,    -1,   134,   128,   157,    17,
      -1,   134,     9,   130,   134,    -1,   134,     9,   130,   134,
     128,   157,    17,    -1,   128,   157,    17,    -1,     7,   128,
     157,    17,    -1,     8,   128,   157,    17,    -1,    -1,    56,
     126,    12,   162,   150,   136,   128,   157,    17,    -1,   135,
      -1,   137,   131,   135,    -1,   140,    -1,   138,   119,   140,
      -1,   161,    71,   139,    -1,   139,    95,   139,    96,   139,
      -1,   139,   118,   139,    -1,   139,   117,   139,    -1,   139,
      23,   148,    -1,   139,    23,   139,    -1,   139,    46,   163,
      -1,    12,   141,    16,    46,   163,    -1,   139,   160,    -1,
     147,    -1,   160,    -1,   161,    71,   140,    -1,   140,    95,
     140,    96,   140,    -1,   140,   118,   140,    -1,   140,   117,
     140,    -1,   140,    40,   140,    -1,   140,    41,   140,    -1,
     140,    42,   140,    -1,   140,    43,   140,    -1,   140,    44,
     140,    -1,   140,    45,   140,    -1,   140,    23,   148,    -1,
     140,    23,   140,    -1,   140,    46,   163,    -1,    12,   141,
      16,    46,   163,    -1,   140,    13,    97,   161,    -1,   140,
      13,    97,    -1,   140,   160,    -1,   147,    -1,   160,    -1,
     140,   119,   140,    -1,   141,   119,   140,    -1,   139,    -1,
     142,   119,   139,    -1,    -1,   142,    -1,    12,   141,    16,
      -1,    78,    -1,    79,    -1,    10,    -1,    14,    -1,   145,
      10,    -1,   145,    14,    -1,    17,    -1,   146,    10,    -1,
     148,    -1,   109,   147,    -1,    -1,    15,   149,    94,    15,
      -1,    16,    -1,   150,    10,    -1,   144,   143,    13,   160,
      -1,   144,   143,    39,   160,    -1,   144,   143,    42,   160,
      -1,   144,   143,    -1,    52,   163,    18,   138,    19,    -1,
      52,   163,    -1,   140,    -1,     1,    -1,   129,    -1,    14,
     130,    -1,    49,   152,    -1,    51,   152,    -1,    -1,    -1,
     120,   154,   153,   155,   101,    12,   140,    16,   152,    -1,
      54,   140,   152,    -1,    54,   152,    -1,   122,    -1,   127,
     153,   121,   153,    -1,   127,   153,    -1,   128,   157,   146,
      -1,    63,   152,    -1,    64,   152,    -1,   100,   140,   152,
      -1,   100,   152,    -1,   151,   152,    -1,    -1,   164,   156,
     153,    -1,    14,   130,    -1,   153,    -1,   157,   153,    -1,
      57,    -1,    58,    -1,    93,    -1,   159,    93,    -1,   160,
      15,    71,   160,    -1,   160,   103,   160,    -1,   160,   104,
     160,    -1,   160,   105,   160,    -1,   160,    15,   160,    -1,
     147,    15,   160,    -1,   160,   106,   160,    -1,   160,   110,
     160,    -1,   104,   160,    -1,   103,   160,    -1,   109,   160,
      -1,    48,    12,    16,    -1,    48,    12,   138,    16,    -1,
      48,    -1,    91,    12,    16,    -1,    91,    12,   138,    16,
      -1,    50,   160,    -1,   112,   161,    -1,   111,   161,    -1,
     161,   112,    -1,   161,   111,    -1,    97,   161,    44,   160,
      -1,    97,    44,   160,    -1,    97,   161,    -1,    97,    -1,
      60,    12,   140,   119,   140,    16,    -1,    60,    12,   140,
     119,   148,    16,    -1,    12,   140,    16,    -1,    62,    12,
     140,   119,   148,    16,    -1,    62,    12,   140,   119,   140,
      16,    -1,    92,    -1,    99,    12,   140,   119,   163,   119,
     140,    16,    -1,    99,    12,   140,   119,   163,   119,   148,
      16,    -1,    99,    12,   140,   119,   163,    16,    -1,    80,
      12,   138,    16,    -1,   159,    -1,   158,    12,   148,   119,
     140,    16,    -1,   158,    12,   140,   119,   140,    16,    -1,
     158,    12,   148,   119,   140,   119,   161,    16,    -1,   158,
      12,   140,   119,   140,   119,   161,    16,    -1,    98,    12,
     140,   119,   140,   119,   140,    16,    -1,    98,    12,   140,
     119,   140,    16,    -1,   161,    -1,   163,    -1,   163,    18,
     138,    19,    -1,    89,    -1,   113,   160,    -1,    -1,    88,
      -1,   162,   119,    88,    -1,    88,    -1,    47,    -1,    90,
      -1,   101,    12,   140,   150,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    99,    99,   101,   105,   105,   109,   109,   113,   113,
     117,   117,   121,   121,   125,   125,   127,   127,   129,   129,
     134,   135,   139,   143,   143,   147,   147,   151,   152,   156,
     157,   162,   163,   167,   168,   172,   176,   177,   178,   179,
     180,   181,   183,   185,   185,   190,   191,   195,   196,   200,
     201,   203,   205,   207,   208,   213,   214,   215,   216,   217,
     221,   222,   224,   226,   228,   229,   230,   231,   232,   233,
     234,   235,   240,   241,   242,   245,   248,   249,   250,   254,
     255,   259,   260,   264,   265,   266,   270,   270,   274,   274,
     274,   274,   278,   278,   282,   284,   288,   288,   292,   292,
     296,   299,   302,   305,   306,   307,   308,   309,   313,   314,
     318,   320,   322,   322,   322,   324,   325,   326,   327,   328,
     329,   330,   333,   336,   337,   338,   339,   339,   340,   344,
     345,   349,   349,   353,   354,   358,   359,   360,   361,   362,
     363,   364,   365,   366,   367,   368,   369,   370,   371,   372,
     373,   374,   375,   376,   377,   378,   379,   380,   381,   382,
     383,   385,   388,   389,   391,   396,   397,   399,   401,   403,
     404,   405,   407,   412,   414,   419,   421,   423,   427,   428,
     429,   430,   434,   435,   436,   442,   443,   444,   449
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "FIRSTTOKEN", "PROGRAM", "PASTAT",
  "PASTAT2", "XBEGIN", "XEND", "','", "NL", "'{'", "'('", "'|'", "';'",
  "'/'", "')'", "'}'", "'['", "']'", "ARRAY", "MATCH", "NOTMATCH",
  "MATCHOP", "FINAL", "DOT", "ALL", "CCL", "NCCL", "CHAR", "OR", "STAR",
  "QUEST", "PLUS", "EMPTYRE", "ZERO", "IGNORE_PRIOR_ATOM", "AND", "BOR",
  "APPEND", "EQ", "GE", "GT", "LE", "LT", "NE", "IN", "ARG", "BLTIN",
  "BREAK", "CLOSE", "CONTINUE", "DELETE", "DO", "EXIT", "FOR", "FUNC",
  "SUB", "GSUB", "IF", "INDEX", "LSUBSTR", "MATCHFCN", "NEXT", "NEXTFILE",
  "ADD", "MINUS", "MULT", "DIVIDE", "MOD", "ASSIGN", "ASGNOP", "ADDEQ",
  "SUBEQ", "MULTEQ", "DIVEQ", "MODEQ", "POWEQ", "PRINT", "PRINTF",
  "SPRINTF", "ELSE", "INTEST", "CONDEXPR", "POSTINCR", "PREINCR",
  "POSTDECR", "PREDECR", "VAR", "IVAR", "VARNF", "CALL", "NUMBER",
  "STRING", "REGEXPR", "'?'", "':'", "GETLINE", "SUBSTR", "SPLIT",
  "RETURN", "WHILE", "CAT", "'+'", "'-'", "'*'", "'%'", "UPLUS", "UMINUS",
  "NOT", "POWER", "INCR", "DECR", "INDIRECT", "LASTTOKEN", "$accept",
  "program", "and", "bor", "comma", "do", "else", "for", "@1", "@2", "@3",
  "funcname", "if", "lbrace", "nl", "opt_nl", "opt_pst", "opt_simple_stmt",
  "pas", "pa_pat", "pa_stat", "@4", "pa_stats", "patlist", "ppattern",
  "pattern", "plist", "pplist", "prarg", "print", "pst", "rbrace", "re",
  "reg_expr", "@5", "rparen", "simple_stmt", "st", "stmt", "@6", "@7",
  "@8", "stmtlist", "subop", "string", "term", "var", "varlist", "varname",
  "while", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,    44,
     264,   123,    40,   124,    59,    47,    41,   125,    91,    93,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,    63,    58,   340,   341,   342,
     343,   344,   345,    43,    45,    42,    37,   346,   347,   348,
     349,   350,   351,   352,   353
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,   115,   116,   116,   117,   117,   118,   118,   119,   119,
     120,   120,   121,   121,   123,   122,   124,   122,   125,   122,
     126,   126,   127,   128,   128,   129,   129,   130,   130,   131,
     131,   132,   132,   133,   133,   134,   135,   135,   135,   135,
     135,   135,   135,   136,   135,   137,   137,   138,   138,   139,
     139,   139,   139,   139,   139,   139,   139,   139,   139,   139,
     140,   140,   140,   140,   140,   140,   140,   140,   140,   140,
     140,   140,   140,   140,   140,   140,   140,   140,   140,   141,
     141,   142,   142,   143,   143,   143,   144,   144,   145,   145,
     145,   145,   146,   146,   147,   147,   149,   148,   150,   150,
     151,   151,   151,   151,   151,   151,   151,   151,   152,   152,
     153,   153,   154,   155,   153,   153,   153,   153,   153,   153,
     153,   153,   153,   153,   153,   153,   156,   153,   153,   157,
     157,   158,   158,   159,   159,   160,   160,   160,   160,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   160,   160,
     160,   160,   160,   160,   160,   160,   160,   160,   161,   161,
     161,   161,   162,   162,   162,   163,   163,   163,   164
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     1,     2,     1,     2,     1,     2,
       1,     2,     1,     2,     0,    12,     0,    10,     0,     8,
       1,     1,     4,     1,     2,     1,     2,     0,     1,     0,
       1,     0,     1,     1,     3,     1,     1,     4,     4,     7,
       3,     4,     4,     0,     9,     1,     3,     1,     3,     3,
       5,     3,     3,     3,     3,     3,     5,     2,     1,     1,
       3,     5,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     5,     4,     3,     2,     1,     1,     3,
       3,     1,     3,     0,     1,     3,     1,     1,     1,     1,
       2,     2,     1,     2,     1,     2,     0,     4,     1,     2,
       4,     4,     4,     2,     5,     2,     1,     1,     1,     2,
       2,     2,     0,     0,     9,     3,     2,     1,     4,     2,
       3,     2,     2,     3,     2,     2,     0,     3,     2,     1,
       2,     1,     1,     1,     2,     4,     3,     3,     3,     3,
       3,     3,     3,     2,     2,     2,     3,     4,     1,     3,
       4,     2,     2,     2,     2,     2,     4,     3,     2,     1,
       6,     6,     3,     6,     6,     1,     8,     8,     6,     4,
       1,     6,     6,     8,     8,     8,     6,     1,     1,     4,
       1,     2,     0,     1,     3,     1,     1,     1,     4
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     3,    88,    89,     0,    33,     2,    30,     1,     0,
       0,    23,     0,    96,   186,   148,     0,     0,   131,   132,
       0,     0,     0,   185,   180,   187,     0,   165,   133,   159,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    36,
      45,    29,    35,    77,    94,     0,   170,    78,   177,   178,
      90,    91,     0,     0,     0,     0,     0,     0,     0,     0,
     151,   177,    20,    21,     0,     0,     0,     0,     0,     0,
     158,     0,     0,   144,   143,    95,   145,   153,   152,   181,
     107,    24,    27,     0,     0,     0,    10,     0,     0,     0,
       0,     0,    86,    87,     0,     0,   112,   117,     0,     0,
     106,    83,     0,   129,     0,   126,    27,     0,    34,     0,
       0,     4,     6,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    76,     0,     0,   134,     0,     0,     0,
       0,     0,     0,     0,   155,   154,     0,     0,     0,     8,
     162,     0,     0,     0,     0,   146,     0,    47,     0,   182,
       0,     0,     0,   149,     0,   157,     0,     0,     0,    25,
      28,   128,    27,   108,   110,   111,   105,     0,   116,     0,
       0,   121,   122,     0,   124,     0,    11,     0,   119,     0,
       0,    81,    84,   103,    58,    59,   177,   125,    40,   130,
       0,     0,     0,    46,    75,    71,    70,    64,    65,    66,
      67,    68,    69,    72,     0,     5,    63,     7,    62,   140,
       0,    94,     0,   139,   136,   137,   138,   141,   142,    60,
       0,    41,    42,     9,    79,     0,    80,    97,   147,     0,
     183,     0,     0,     0,   169,   150,   156,     0,     0,    26,
     109,     0,   115,     0,    32,   178,     0,   123,     0,   113,
      12,     0,    92,   120,     0,     0,     0,     0,     0,     0,
      57,     0,     0,     0,     0,     0,   127,    38,    37,    74,
       0,     0,     0,   135,   179,    73,    48,    98,     0,    43,
       0,    94,     0,    94,     0,     0,     0,    27,     0,    22,
     188,     0,    13,   118,    93,    85,     0,    54,    53,    55,
       0,    52,    51,    82,   100,   101,   102,    49,     0,    61,
       0,     0,   184,    99,     0,   160,   161,   164,   163,   176,
       0,   168,     0,   104,    27,     0,     0,     0,     0,     0,
       0,     0,   172,     0,   171,     0,     0,     0,     0,    94,
       0,     0,    18,     0,    56,     0,    50,    39,     0,     0,
       0,   175,   166,   167,     0,    27,     0,     0,   174,   173,
      44,    16,     0,    19,     0,     0,     0,   114,    17,    14,
       0,    15
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     4,   121,   122,   229,    96,   251,    97,   370,   365,
     356,    64,    98,    99,   163,   161,     5,   243,     6,    39,
      40,   314,    41,   146,   181,   100,    55,   182,   183,   101,
       7,   253,    43,    44,    56,   279,   102,   164,   103,   177,
     291,   190,   104,    45,    46,    47,    48,   231,    49,   105
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -307
static const yytype_int16 yypact[] =
{
     875,  -307,  -307,  -307,    11,  1824,  -307,    81,  -307,    47,
      47,  -307,  4717,  -307,  -307,    49,  4775,   -39,  -307,  -307,
      60,    70,   108,  -307,  -307,  -307,   109,  -307,  -307,   214,
     141,   149,  4775,  4775,  4775,   -15,   -15,  4775,  1008,   147,
    -307,    87,  3840,    64,  -307,   162,    12,    63,   -46,   121,
    -307,  -307,  1008,  1008,  1929,    32,    83,  4483,  4717,    64,
      63,    95,  -307,  -307,   179,  4717,  4717,  4717,  4541,  4775,
     154,  4717,  4717,    91,    91,  -307,    91,  -307,  -307,  -307,
    -307,  -307,   195,    93,    93,    -2,  -307,  2499,   197,   199,
      93,    93,  -307,  -307,  2499,   200,   203,  -307,  1639,  1008,
    3840,  4833,    93,  -307,  1078,  -307,   195,  1008,  1824,   119,
    4717,  -307,  -307,  4717,  4717,  4717,  4717,  4717,  4717,    -2,
    4717,  2557,  2615,    63,  4775,  4717,  -307,  4599,  4775,  4775,
    4775,  4775,  4775,  4717,  -307,  -307,  4717,  1148,  1218,  -307,
    -307,  2673,   172,  2673,   202,  -307,    54,  3840,  2949,   131,
    2337,  2337,    67,  -307,    71,    63,  4775,  2337,  2337,  -307,
     210,  -307,   195,   210,  -307,  -307,   204,  2441,  -307,  1708,
    4717,  -307,  -307,  2441,  -307,  4717,  -307,  1639,   140,  1288,
    4717,  4294,   216,    18,    64,    63,   -12,  -307,  -307,  -307,
    1639,  4717,  1358,  -307,   -15,  4119,  -307,  4119,  4119,  4119,
    4119,  4119,  4119,  -307,  3042,  -307,  4026,  -307,  3933,    91,
    2337,   216,  4775,    91,    94,    94,    91,    91,    91,  3840,
      28,  -307,  -307,  -307,  3840,    -2,  3840,  -307,  -307,  2673,
    -307,   127,  2673,  2673,  -307,  -307,    63,  2673,    34,  -307,
    -307,  4717,  -307,   215,  -307,     9,  3144,  -307,  3144,  -307,
    -307,  1429,  -307,   218,   143,  4891,    -2,  4891,  2731,  2789,
      63,  2847,  4775,  4775,  4775,  4891,  -307,    47,  -307,  -307,
    4717,  2673,  2673,    63,  -307,  -307,  3840,  -307,    14,   221,
    3246,   219,  3348,   220,  2034,   146,    31,    96,    -2,   221,
     221,   133,  -307,  -307,  -307,   191,  4717,  4659,  -307,  -307,
    4201,  4425,  4365,  4294,    63,    63,    63,  4294,  1008,  3840,
    2139,  2244,  -307,  -307,    47,  -307,  -307,  -307,  -307,  -307,
    2673,  -307,  2673,  -307,   195,  4717,   224,   226,    -2,   171,
    4891,  1499,  -307,     4,  -307,     4,  1008,  3450,  3552,   234,
    1766,  3645,   221,  4717,  -307,   191,  4294,  -307,   236,   237,
    1569,  -307,  -307,  -307,   224,   195,  1639,  3747,  -307,  -307,
    -307,   221,  1766,  -307,    93,  1639,   224,  -307,  -307,   221,
    1639,  -307
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -307,  -307,  -174,  -153,    13,  -307,  -307,  -307,  -307,  -307,
    -307,  -307,  -307,    -4,   -73,   -98,   228,  -306,  -307,    65,
     151,  -307,  -307,   -65,    88,   721,  -151,  -307,  -307,  -307,
    -307,  -307,   238,   -95,  -307,  -236,  -165,   -48,   222,  -307,
    -307,  -307,   -30,  -307,  -307,   473,   -16,  -307,   -23,  -307
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -32
static const yytype_int16 yytable[] =
{
      61,    38,   152,   154,   244,    52,    53,   258,   191,   160,
     289,     8,   290,    70,   223,   196,    61,    61,    61,    77,
      78,    61,   137,   138,   223,   133,    61,   136,   259,   254,
     211,   262,    14,   160,   354,   107,   165,   139,    61,   168,
     139,   139,   171,   172,   223,    14,   174,   274,   142,    62,
     323,    14,    63,    61,   187,   288,   366,   263,    11,   265,
     264,    57,   166,   139,   240,   134,   135,   141,   143,   179,
     228,   220,    65,    23,    24,    25,   139,   192,   127,   124,
     139,    14,    66,   234,    61,   186,    23,   235,    25,   160,
     342,    50,    23,    24,    25,    51,   203,     2,    37,   134,
     135,     3,   312,   159,    38,   126,   159,   162,    61,   127,
     324,    61,    61,    61,    61,    61,    61,    37,   361,   242,
      67,    68,    23,   258,    25,   247,   258,   258,   258,   258,
     369,    61,    61,   258,    61,    61,   139,   281,   283,   136,
      61,    61,    61,   277,   259,   329,   245,   259,   259,   259,
     259,    61,   139,    71,   259,   139,   106,    61,    11,   295,
     298,    72,   321,   232,   233,    61,   128,   129,   130,   131,
     237,   238,   258,   132,   125,   244,   286,   144,   269,    61,
     139,    61,    61,    61,    61,    61,    61,   345,    61,   325,
      61,   149,    61,   259,    61,   261,    61,   244,   156,   130,
     131,   132,   275,    61,   132,   159,   134,   135,    61,   169,
      61,   170,   175,   176,   160,   285,   194,   227,   225,   230,
     239,   250,   241,   271,   272,   139,   340,   339,   294,   287,
      61,   313,    61,   299,   327,   316,   318,   328,   343,   186,
     277,   186,   186,   186,   278,   186,    61,    61,    61,   186,
     353,   160,   358,   359,    59,     0,   267,   362,    69,   193,
      61,    14,     0,   308,    61,   326,    61,   143,    61,   108,
      59,    59,    75,     0,     0,    59,     0,     0,   331,     0,
      59,    61,   160,     0,    61,    61,    61,    61,     0,     0,
       0,    61,    59,    61,    61,    61,     0,   320,   322,     0,
       0,     0,    23,    24,    25,   344,   350,    59,     0,     0,
     336,     0,     0,     0,   186,     0,   367,   348,     0,   349,
     178,    61,    61,   333,   335,    61,   189,    37,     0,     0,
      61,     0,     0,     0,     0,     0,     0,     0,    59,   184,
       0,    61,   143,   297,     0,   300,   301,   302,     0,   303,
       0,     0,     0,   307,     0,     0,     0,     0,     0,   189,
     189,     0,    59,     0,     0,    59,    59,    59,    59,    59,
      59,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    59,    59,     0,    59,    59,
       0,     0,     0,     0,    59,    59,    59,     0,     0,   249,
       0,   189,     0,     0,     0,    59,     0,     0,     0,     0,
       0,    59,   266,     0,   189,     0,     0,     0,   346,    59,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    59,     0,    59,    59,    59,    59,    59,
      59,     0,    59,     0,    59,     0,    59,     0,    59,     0,
      59,     0,     0,     0,     0,     0,     0,    59,     0,     0,
       0,     0,    59,     0,    59,     0,     0,     0,     0,     0,
       0,     0,     0,   293,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    59,     0,    59,     0,     0,    60,
       0,     0,     0,   184,     0,   184,   184,   184,     0,   184,
      59,    59,    59,   184,     0,    73,    74,    76,     0,     0,
      79,     0,     0,     0,    59,   123,     0,     0,    59,     0,
      59,     0,    59,     0,     0,     0,     0,   123,     0,     0,
       0,     0,     0,     0,     0,    59,     0,     0,    59,    59,
      59,    59,   155,     0,     0,    59,     0,    59,    59,    59,
       0,     0,     0,   189,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   184,     0,
       0,     0,   189,   123,   185,    59,    59,     0,   363,    59,
       0,     0,     0,     0,    59,     0,     0,   368,     0,     0,
       0,     0,   371,     0,     0,    59,     0,   209,     0,     0,
     213,   214,   215,   216,   217,   218,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     123,   123,     0,   123,   123,     0,     0,     0,     0,   236,
     123,   123,     0,     0,     0,     0,     0,     0,     0,     0,
     123,     0,     0,     0,     0,     0,   123,     0,     0,     0,
       0,     0,     0,     0,   260,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   123,     0,
     123,   123,   123,   123,   123,   123,     0,   123,     0,   123,
       0,   123,     0,   123,     0,   273,     0,     0,     0,     0,
       0,     0,   123,     0,     0,     0,     0,   123,     0,   123,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   123,
       0,   123,     0,     0,     0,     0,    42,     0,   185,     0,
     185,   185,   185,    54,   185,   304,   305,   306,   185,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   123,
       0,     0,     0,   123,     0,   123,     0,   123,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     260,     0,     0,   260,   260,   260,   260,     0,   147,   148,
     260,     0,   123,   123,   123,     0,   150,   151,   147,   147,
       0,     0,   157,   158,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   185,     0,     0,     0,     0,   167,     0,
     123,   123,     0,     0,   123,   173,     0,     0,     0,   260,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    42,
     123,   195,     0,     0,   197,   198,   199,   200,   201,   202,
       0,   204,   206,   208,     0,     0,   210,     0,     0,     0,
       0,     0,     0,     0,   219,     0,     0,   147,     0,     0,
       0,     0,   224,     0,   226,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   -29,     1,     0,     0,     0,
       0,     0,   -29,   -29,     0,     2,   -29,   -29,     0,     3,
     -29,   246,     0,     0,     0,     0,   248,     0,     0,     0,
       0,    54,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    42,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   -29,   -29,     0,   -29,     0,     0,     0,     0,
       0,   -29,   -29,   -29,     0,   -29,     0,   -29,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     276,     0,     0,   280,   282,   -29,     0,     0,   284,     0,
       0,     0,   147,   -29,   -29,   -29,   -29,   -29,   -29,     0,
       0,     0,   -29,   -29,   -29,     0,     0,     0,   -29,   -29,
       0,     0,     0,     0,   -29,     0,   -29,   -29,   -29,     0,
       0,   309,   310,   311,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    80,
       0,     0,     0,     0,     0,     0,     0,    54,    81,    11,
      12,     0,    82,    13,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   337,     0,   338,     0,     0,   341,     0,     0,     0,
       0,     0,     0,     0,     0,    14,    15,    83,    16,    84,
      85,    86,    87,    88,   357,    18,    19,    89,    20,     0,
      21,    90,    91,     0,     0,     0,     0,     0,     0,    80,
       0,     0,     0,     0,     0,     0,    92,    93,    22,    11,
      12,     0,    82,    13,     0,   188,    23,    24,    25,    26,
      27,    28,     0,     0,     0,    29,    30,    31,    94,    95,
       0,    32,    33,     0,     0,     0,     0,    34,     0,    35,
      36,    37,     0,     0,     0,    14,    15,    83,    16,    84,
      85,    86,    87,    88,     0,    18,    19,    89,    20,     0,
      21,    90,    91,     0,     0,     0,     0,     0,     0,    80,
       0,     0,     0,     0,     0,     0,    92,    93,    22,    11,
      12,     0,    82,    13,     0,   221,    23,    24,    25,    26,
      27,    28,     0,     0,     0,    29,    30,    31,    94,    95,
       0,    32,    33,     0,     0,     0,     0,    34,     0,    35,
      36,    37,     0,     0,     0,    14,    15,    83,    16,    84,
      85,    86,    87,    88,     0,    18,    19,    89,    20,     0,
      21,    90,    91,     0,     0,     0,     0,     0,     0,    80,
       0,     0,     0,     0,     0,     0,    92,    93,    22,    11,
      12,     0,    82,    13,     0,   222,    23,    24,    25,    26,
      27,    28,     0,     0,     0,    29,    30,    31,    94,    95,
       0,    32,    33,     0,     0,     0,     0,    34,     0,    35,
      36,    37,     0,     0,     0,    14,    15,    83,    16,    84,
      85,    86,    87,    88,     0,    18,    19,    89,    20,     0,
      21,    90,    91,     0,     0,     0,     0,     0,     0,    80,
       0,     0,     0,     0,     0,     0,    92,    93,    22,    11,
      12,     0,    82,    13,     0,   252,    23,    24,    25,    26,
      27,    28,     0,     0,     0,    29,    30,    31,    94,    95,
       0,    32,    33,     0,     0,     0,     0,    34,     0,    35,
      36,    37,     0,     0,     0,    14,    15,    83,    16,    84,
      85,    86,    87,    88,     0,    18,    19,    89,    20,     0,
      21,    90,    91,     0,     0,     0,     0,     0,     0,    80,
       0,     0,     0,     0,     0,     0,    92,    93,    22,    11,
      12,     0,    82,    13,     0,   268,    23,    24,    25,    26,
      27,    28,     0,     0,     0,    29,    30,    31,    94,    95,
       0,    32,    33,     0,     0,     0,     0,    34,     0,    35,
      36,    37,     0,     0,     0,    14,    15,    83,    16,    84,
      85,    86,    87,    88,     0,    18,    19,    89,    20,     0,
      21,    90,    91,     0,     0,     0,     0,     0,     0,     0,
      80,     0,     0,     0,     0,     0,    92,    93,    22,   292,
      11,    12,     0,    82,    13,     0,    23,    24,    25,    26,
      27,    28,     0,     0,     0,    29,    30,    31,    94,    95,
       0,    32,    33,     0,     0,     0,     0,    34,     0,    35,
      36,    37,     0,     0,     0,     0,    14,    15,    83,    16,
      84,    85,    86,    87,    88,     0,    18,    19,    89,    20,
       0,    21,    90,    91,     0,     0,     0,     0,     0,     0,
      80,     0,     0,     0,     0,     0,     0,    92,    93,    22,
      11,    12,     0,    82,    13,     0,   347,    23,    24,    25,
      26,    27,    28,     0,     0,     0,    29,    30,    31,    94,
      95,     0,    32,    33,     0,     0,     0,     0,    34,     0,
      35,    36,    37,     0,     0,     0,    14,    15,    83,    16,
      84,    85,    86,    87,    88,     0,    18,    19,    89,    20,
       0,    21,    90,    91,     0,     0,     0,     0,     0,     0,
      80,     0,     0,     0,     0,     0,     0,    92,    93,    22,
      11,    12,     0,    82,    13,     0,   360,    23,    24,    25,
      26,    27,    28,     0,     0,     0,    29,    30,    31,    94,
      95,     0,    32,    33,     0,     0,     0,     0,    34,     0,
      35,    36,    37,     0,     0,     0,    14,    15,    83,    16,
      84,    85,    86,    87,    88,     0,    18,    19,    89,    20,
       0,    21,    90,    91,     0,     0,     0,     0,     0,     0,
      80,     0,     0,     0,     0,     0,     0,    92,    93,    22,
      11,    12,     0,    82,    13,     0,     0,    23,    24,    25,
      26,    27,    28,     0,     0,     0,    29,    30,    31,    94,
      95,     0,    32,    33,     0,     0,     0,     0,    34,     0,
      35,    36,    37,     0,     0,     0,    14,    15,    83,    16,
      84,    85,    86,    87,    88,     0,    18,    19,    89,    20,
       0,    21,    90,    91,     0,     0,     0,     0,     0,    80,
       0,     0,     0,     0,     0,     0,     0,    92,    93,    22,
      12,     0,   -31,    13,     0,     0,     0,    23,    24,    25,
      26,    27,    28,     0,     0,     0,    29,    30,    31,    94,
      95,     0,    32,    33,     0,     0,     0,     0,    34,     0,
      35,    36,    37,     0,     0,    14,    15,     0,    16,     0,
      85,     0,     0,     0,     0,    18,    19,    80,    20,     0,
      21,     0,     0,     0,     0,     0,     0,     0,    12,     0,
       0,    13,   -31,     0,     0,     0,    92,    93,    22,     0,
       0,     0,     0,     0,     0,     0,    23,    24,    25,    26,
      27,    28,     0,     0,     0,    29,    30,    31,     0,     0,
       0,    32,    33,    14,    15,     0,    16,    34,    85,    35,
      36,    37,     0,    18,    19,     0,    20,     0,    21,     0,
       0,     9,    10,     0,     0,    11,    12,     0,     0,    13,
       0,     0,     0,     0,    92,    93,    22,     0,     0,     0,
       0,     0,     0,     0,    23,    24,    25,    26,    27,    28,
       0,     0,     0,    29,    30,    31,     0,     0,     0,    32,
      33,    14,    15,     0,    16,    34,     0,    35,    36,    37,
      17,    18,    19,     0,    20,     0,    21,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    22,     0,     0,     0,     0,     0,
       0,     0,    23,    24,    25,    26,    27,    28,     0,     0,
       0,    29,    30,    31,     0,     0,     0,    32,    33,     0,
       0,     0,     0,    34,     0,    35,    36,    37,   139,     0,
       0,    58,   109,     0,    13,   140,     0,     0,     0,     0,
       0,     0,   110,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   111,   112,     0,   113,
     114,   115,   116,   117,   118,   119,    14,    15,     0,    16,
       0,     0,     0,     0,     0,     0,    18,    19,     0,    20,
       0,    21,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    22,
       0,     0,     0,     0,     0,     0,     0,    23,    24,    25,
      26,    27,    28,     0,   120,     0,    29,    30,    31,     0,
       0,     0,    32,    33,     0,     0,     0,     0,    34,     0,
      35,    36,    37,   139,     0,     0,    58,   109,     0,    13,
     319,     0,     0,     0,     0,     0,     0,   110,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   111,   112,     0,   113,   114,   115,   116,   117,   118,
     119,    14,    15,     0,    16,     0,     0,     0,     0,     0,
       0,    18,    19,     0,    20,     0,    21,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    22,     0,     0,     0,     0,     0,
       0,     0,    23,    24,    25,    26,    27,    28,     0,   120,
       0,    29,    30,    31,     0,     0,     0,    32,    33,     0,
       0,     0,     0,    34,     0,    35,    36,    37,   139,     0,
       0,    58,   109,     0,    13,   332,     0,     0,     0,     0,
       0,     0,   110,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   111,   112,     0,   113,
     114,   115,   116,   117,   118,   119,    14,    15,     0,    16,
       0,     0,     0,     0,     0,     0,    18,    19,     0,    20,
       0,    21,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    22,
       0,     0,     0,     0,     0,     0,     0,    23,    24,    25,
      26,    27,    28,     0,   120,     0,    29,    30,    31,     0,
       0,     0,    32,    33,     0,     0,     0,     0,    34,     0,
      35,    36,    37,   139,     0,     0,    58,   109,     0,    13,
     334,     0,     0,     0,     0,     0,     0,   110,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   111,   112,     0,   113,   114,   115,   116,   117,   118,
     119,    14,    15,     0,    16,     0,     0,     0,     0,     0,
       0,    18,    19,     0,    20,     0,    21,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    22,     0,     0,     0,     0,     0,
       0,     0,    23,    24,    25,    26,    27,    28,     0,   120,
       0,    29,    30,    31,     0,     0,   139,    32,    33,    58,
     109,     0,    13,    34,     0,    35,    36,    37,     0,     0,
     110,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   111,   112,     0,   113,   114,   115,
     116,   117,   118,   119,    14,    15,     0,    16,     0,     0,
       0,     0,     0,     0,    18,    19,     0,    20,     0,    21,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    22,     0,     0,
       0,     0,     0,     0,     0,    23,    24,    25,    26,    27,
      28,     0,   120,     0,    29,    30,    31,     0,     0,     0,
      32,    33,     0,     0,     0,     0,    34,     0,    35,    36,
      37,   159,     0,    58,   109,   162,    13,     0,     0,     0,
       0,     0,     0,     0,   110,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   111,   112,
       0,   113,   114,   115,   116,   117,   118,   119,    14,    15,
       0,    16,     0,     0,     0,     0,     0,     0,    18,    19,
       0,    20,     0,    21,     0,     0,     0,     0,     0,   159,
       0,    12,     0,   162,    13,     0,     0,     0,     0,     0,
       0,    22,     0,     0,     0,     0,     0,     0,     0,    23,
      24,    25,    26,    27,    28,     0,   120,     0,    29,    30,
      31,     0,     0,     0,    32,    33,    14,    15,     0,    16,
      34,     0,    35,    36,    37,     0,    18,    19,     0,    20,
       0,    21,     0,     0,     0,     0,     0,   205,     0,    12,
       0,     0,    13,     0,     0,     0,     0,     0,     0,    22,
       0,     0,     0,     0,     0,     0,     0,    23,    24,    25,
      26,    27,    28,     0,     0,     0,    29,    30,    31,     0,
       0,     0,    32,    33,    14,    15,     0,    16,    34,     0,
      35,    36,    37,     0,    18,    19,     0,    20,     0,    21,
       0,     0,     0,     0,     0,   207,     0,    12,     0,     0,
      13,     0,     0,     0,     0,     0,     0,    22,     0,     0,
       0,     0,     0,     0,     0,    23,    24,    25,    26,    27,
      28,     0,     0,     0,    29,    30,    31,     0,     0,     0,
      32,    33,    14,    15,     0,    16,    34,     0,    35,    36,
      37,     0,    18,    19,     0,    20,     0,    21,     0,     0,
       0,     0,     0,   223,     0,    12,     0,     0,    13,     0,
       0,     0,     0,     0,     0,    22,     0,     0,     0,     0,
       0,     0,     0,    23,    24,    25,    26,    27,    28,     0,
       0,     0,    29,    30,    31,     0,     0,     0,    32,    33,
      14,    15,     0,    16,    34,     0,    35,    36,    37,     0,
      18,    19,     0,    20,     0,    21,     0,     0,     0,     0,
       0,   205,     0,   296,     0,     0,    13,     0,     0,     0,
       0,     0,     0,    22,     0,     0,     0,     0,     0,     0,
       0,    23,    24,    25,    26,    27,    28,     0,     0,     0,
      29,    30,    31,     0,     0,     0,    32,    33,    14,    15,
       0,    16,    34,     0,    35,    36,    37,     0,    18,    19,
       0,    20,     0,    21,     0,     0,     0,     0,     0,   207,
       0,   296,     0,     0,    13,     0,     0,     0,     0,     0,
       0,    22,     0,     0,     0,     0,     0,     0,     0,    23,
      24,    25,    26,    27,    28,     0,     0,     0,    29,    30,
      31,     0,     0,     0,    32,    33,    14,    15,     0,    16,
      34,     0,    35,    36,    37,     0,    18,    19,     0,    20,
       0,    21,     0,     0,     0,     0,     0,   223,     0,   296,
       0,     0,    13,     0,     0,     0,     0,     0,     0,    22,
       0,     0,     0,     0,     0,     0,     0,    23,    24,    25,
      26,    27,    28,     0,     0,     0,    29,    30,    31,     0,
       0,     0,    32,    33,    14,    15,     0,    16,    34,     0,
      35,    36,    37,     0,    18,    19,     0,    20,     0,    21,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    22,     0,     0,
       0,     0,     0,     0,     0,    23,    24,    25,    26,    27,
      28,     0,     0,     0,    29,    30,    31,     0,     0,     0,
      32,    33,     0,     0,     0,     0,    34,     0,    35,    36,
      37,    58,   109,     0,    13,   140,     0,     0,     0,     0,
       0,     0,   110,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   111,   112,     0,   113,
     114,   115,   116,   117,   118,   119,    14,    15,     0,    16,
       0,     0,     0,     0,     0,     0,    18,    19,     0,    20,
       0,    21,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    22,
       0,     0,     0,     0,     0,     0,     0,    23,    24,    25,
      26,    27,    28,     0,   120,     0,    29,    30,    31,     0,
       0,     0,    32,    33,    58,   109,     0,    13,    34,     0,
      35,    36,    37,     0,     0,   110,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   111,
     112,     0,   113,   114,   115,   116,   117,   118,   119,    14,
      15,     0,    16,     0,     0,     0,     0,     0,     0,    18,
      19,     0,    20,     0,    21,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    22,     0,     0,     0,     0,     0,     0,     0,
      23,    24,    25,    26,    27,    28,     0,   120,   270,    29,
      30,    31,     0,     0,     0,    32,    33,     0,     0,     0,
       0,    34,     0,    35,    36,    37,    58,   109,     0,    13,
     277,     0,     0,     0,     0,     0,     0,   110,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   111,   112,     0,   113,   114,   115,   116,   117,   118,
     119,    14,    15,     0,    16,     0,     0,     0,     0,     0,
       0,    18,    19,     0,    20,     0,    21,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    22,     0,     0,     0,     0,     0,
       0,     0,    23,    24,    25,    26,    27,    28,     0,   120,
       0,    29,    30,    31,     0,     0,     0,    32,    33,     0,
       0,     0,     0,    34,     0,    35,    36,    37,    58,   109,
       0,    13,   315,     0,     0,     0,     0,     0,     0,   110,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   111,   112,     0,   113,   114,   115,   116,
     117,   118,   119,    14,    15,     0,    16,     0,     0,     0,
       0,     0,     0,    18,    19,     0,    20,     0,    21,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    22,     0,     0,     0,
       0,     0,     0,     0,    23,    24,    25,    26,    27,    28,
       0,   120,     0,    29,    30,    31,     0,     0,     0,    32,
      33,     0,     0,     0,     0,    34,     0,    35,    36,    37,
      58,   109,     0,    13,   317,     0,     0,     0,     0,     0,
       0,   110,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   111,   112,     0,   113,   114,
     115,   116,   117,   118,   119,    14,    15,     0,    16,     0,
       0,     0,     0,     0,     0,    18,    19,     0,    20,     0,
      21,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    22,     0,
       0,     0,     0,     0,     0,     0,    23,    24,    25,    26,
      27,    28,     0,   120,     0,    29,    30,    31,     0,     0,
       0,    32,    33,     0,     0,     0,     0,    34,     0,    35,
      36,    37,    58,   109,     0,    13,   351,     0,     0,     0,
       0,     0,     0,   110,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   111,   112,     0,
     113,   114,   115,   116,   117,   118,   119,    14,    15,     0,
      16,     0,     0,     0,     0,     0,     0,    18,    19,     0,
      20,     0,    21,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      22,     0,     0,     0,     0,     0,     0,     0,    23,    24,
      25,    26,    27,    28,     0,   120,     0,    29,    30,    31,
       0,     0,     0,    32,    33,     0,     0,     0,     0,    34,
       0,    35,    36,    37,    58,   109,     0,    13,   352,     0,
       0,     0,     0,     0,     0,   110,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   111,
     112,     0,   113,   114,   115,   116,   117,   118,   119,    14,
      15,     0,    16,     0,     0,     0,     0,     0,     0,    18,
      19,     0,    20,     0,    21,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    22,     0,     0,     0,     0,     0,     0,     0,
      23,    24,    25,    26,    27,    28,     0,   120,     0,    29,
      30,    31,     0,     0,     0,    32,    33,    58,   109,   355,
      13,    34,     0,    35,    36,    37,     0,     0,   110,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   111,   112,     0,   113,   114,   115,   116,   117,
     118,   119,    14,    15,     0,    16,     0,     0,     0,     0,
       0,     0,    18,    19,     0,    20,     0,    21,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    22,     0,     0,     0,     0,
       0,     0,     0,    23,    24,    25,    26,    27,    28,     0,
     120,     0,    29,    30,    31,     0,     0,     0,    32,    33,
       0,     0,     0,     0,    34,     0,    35,    36,    37,    58,
     109,     0,    13,   364,     0,     0,     0,     0,     0,     0,
     110,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   111,   112,     0,   113,   114,   115,
     116,   117,   118,   119,    14,    15,     0,    16,     0,     0,
       0,     0,     0,     0,    18,    19,     0,    20,     0,    21,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    22,     0,     0,
       0,     0,     0,     0,     0,    23,    24,    25,    26,    27,
      28,     0,   120,     0,    29,    30,    31,     0,     0,     0,
      32,    33,    58,   109,     0,    13,    34,     0,    35,    36,
      37,     0,     0,   110,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   111,   112,     0,
     113,   114,   115,   116,   117,   118,   119,    14,    15,     0,
      16,     0,     0,     0,     0,     0,     0,    18,    19,     0,
      20,     0,    21,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      22,     0,     0,     0,     0,     0,     0,     0,    23,    24,
      25,    26,    27,    28,     0,   120,     0,    29,    30,    31,
       0,     0,     0,    32,    33,    58,   109,     0,    13,    34,
       0,    35,    36,    37,     0,     0,   110,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     111,     0,     0,   113,   114,   115,   116,   117,   118,   119,
      14,    15,     0,    16,     0,     0,     0,     0,     0,     0,
      18,    19,     0,    20,     0,    21,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    22,     0,     0,     0,     0,     0,     0,
       0,    23,    24,    25,    26,    27,    28,     0,     0,     0,
      29,    30,    31,     0,     0,     0,    32,    33,    58,   109,
       0,    13,    34,     0,    35,    36,    37,     0,     0,   110,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   113,   114,   115,   116,
     117,   118,   119,    14,    15,     0,    16,     0,     0,     0,
       0,     0,     0,    18,    19,     0,    20,     0,    21,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    22,     0,     0,     0,
       0,     0,     0,     0,    23,    24,    25,    26,    27,    28,
       0,     0,     0,    29,    30,    31,     0,     0,     0,    32,
      33,    58,   -32,     0,    13,    34,     0,    35,    36,    37,
       0,     0,   -32,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   -32,
     -32,   -32,   -32,   -32,   -32,   -32,    14,    15,     0,    16,
       0,     0,     0,     0,     0,     0,    18,    19,     0,    20,
       0,    21,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    22,
       0,     0,     0,     0,     0,     0,     0,    23,    24,    25,
      26,    27,    28,    58,     0,     0,    13,    30,    31,     0,
       0,     0,    32,    33,   255,     0,     0,     0,    34,     0,
      35,    36,    37,     0,     0,     0,     0,     0,   111,   112,
       0,     0,     0,     0,     0,     0,     0,   256,    14,    15,
       0,    16,     0,     0,     0,     0,     0,     0,    18,    19,
       0,    20,     0,    21,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    22,     0,     0,     0,     0,     0,     0,     0,    23,
      24,    25,    26,    27,    28,     0,   257,   330,    29,    30,
      31,     0,     0,     0,    32,    33,    58,     0,     0,    13,
      34,     0,    35,    36,    37,     0,     0,   255,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   111,   112,     0,     0,     0,     0,     0,     0,     0,
     256,    14,    15,     0,    16,     0,     0,     0,     0,     0,
       0,    18,    19,     0,    20,     0,    21,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    22,     0,     0,    58,     0,     0,
      13,     0,    23,    24,    25,    26,    27,    28,   255,   257,
       0,    29,    30,    31,     0,     0,     0,    32,    33,     0,
       0,     0,   111,    34,     0,    35,    36,    37,     0,     0,
       0,   256,    14,    15,     0,    16,     0,     0,     0,     0,
       0,     0,    18,    19,     0,    20,     0,    21,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    58,     0,     0,
      13,     0,     0,     0,     0,    22,     0,     0,   255,     0,
       0,     0,     0,    23,    24,    25,    26,    27,    28,     0,
       0,     0,    29,    30,    31,     0,     0,     0,    32,    33,
       0,   256,    14,    15,    34,    16,    35,    36,    37,     0,
       0,     0,    18,    19,     0,    20,     0,    21,     0,     0,
       0,     0,     0,     0,     0,    12,     0,     0,    13,   145,
       0,     0,     0,     0,     0,    22,     0,     0,     0,     0,
       0,     0,     0,    23,    24,    25,    26,    27,    28,     0,
       0,     0,    29,    30,    31,     0,     0,     0,    32,    33,
      14,    15,     0,    16,    34,     0,    35,    36,    37,     0,
      18,    19,     0,    20,     0,    21,     0,     0,     0,     0,
       0,     0,     0,    12,     0,     0,    13,   153,     0,     0,
       0,     0,     0,    22,     0,     0,     0,     0,     0,     0,
       0,    23,    24,    25,    26,    27,    28,     0,     0,     0,
      29,    30,    31,     0,     0,     0,    32,    33,    14,    15,
       0,    16,    34,     0,    35,    36,    37,     0,    18,    19,
       0,    20,     0,    21,     0,     0,     0,     0,     0,     0,
       0,    58,     0,     0,    13,     0,     0,     0,     0,     0,
       0,    22,     0,     0,     0,     0,     0,     0,     0,    23,
      24,    25,    26,    27,    28,     0,     0,     0,    29,    30,
      31,     0,     0,     0,    32,    33,    14,    15,     0,    16,
      34,     0,    35,    36,    37,     0,    18,    19,     0,    20,
       0,    21,     0,     0,     0,     0,     0,     0,     0,     0,
     212,    58,     0,     0,    13,     0,     0,     0,     0,    22,
       0,     0,   -32,     0,     0,     0,     0,    23,    24,    25,
      26,    27,    28,     0,     0,     0,    29,    30,    31,     0,
       0,     0,    32,    33,     0,   -32,    14,    15,    34,    16,
      35,    36,    37,     0,     0,     0,    18,    19,     0,    20,
       0,    21,     0,     0,     0,     0,     0,     0,     0,    12,
       0,     0,    13,     0,     0,     0,     0,     0,     0,    22,
       0,     0,     0,     0,     0,     0,     0,    23,    24,    25,
      26,    27,    28,     0,     0,     0,     0,    30,    31,     0,
       0,     0,    32,    33,    14,    15,     0,    16,    34,     0,
      35,    36,    37,     0,    18,    19,     0,    20,     0,    21,
       0,     0,     0,     0,     0,     0,     0,    58,     0,     0,
      13,     0,     0,     0,     0,     0,     0,    22,     0,     0,
       0,     0,     0,     0,     0,    23,    24,    25,    26,    27,
      28,     0,     0,     0,    29,    30,    31,     0,     0,     0,
      32,    33,    14,    15,     0,    16,    34,     0,    35,    36,
      37,     0,    18,    19,     0,    20,     0,    21,     0,     0,
       0,     0,     0,     0,     0,   180,     0,     0,    13,     0,
       0,     0,     0,     0,     0,    22,     0,     0,     0,     0,
       0,     0,     0,    23,    24,    25,    26,    27,    28,     0,
       0,     0,    29,    30,    31,     0,     0,     0,    32,    33,
      14,    15,     0,    16,    34,     0,    35,    36,    37,     0,
      18,    19,     0,    20,     0,    21,     0,     0,     0,     0,
       0,     0,     0,   296,     0,     0,    13,     0,     0,     0,
       0,     0,     0,    22,     0,     0,     0,     0,     0,     0,
       0,    23,    24,    25,    26,    27,    28,     0,     0,     0,
      29,    30,    31,     0,     0,     0,    32,    33,    14,    15,
       0,    16,    34,     0,    35,    36,    37,     0,    18,    19,
       0,    20,     0,    21,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    22,     0,     0,     0,     0,     0,     0,     0,    23,
      24,    25,    26,    27,    28,     0,     0,     0,    29,    30,
      31,     0,     0,     0,    32,    33,     0,     0,     0,     0,
      34,     0,    35,    36,    37
};

static const yytype_int16 yycheck[] =
{
      16,     5,    67,    68,   169,     9,    10,   181,   106,    82,
     246,     0,   248,    29,    10,   110,    32,    33,    34,    35,
      36,    37,    52,    53,    10,    71,    42,    18,   181,   180,
     125,    13,    47,   106,   340,    39,    84,     9,    54,    87,
       9,     9,    90,    91,    10,    47,    94,    19,    16,    88,
      19,    47,    91,    69,   102,    46,   362,    39,    11,    71,
      42,    12,    85,     9,   162,   111,   112,    54,    55,    99,
      16,   136,    12,    88,    89,    90,     9,   107,    15,    15,
       9,    47,    12,    16,   100,   101,    88,    16,    90,   162,
     326,    10,    88,    89,    90,    14,   119,    10,   113,   111,
     112,    14,    88,    10,   108,    93,    10,    14,   124,    15,
      14,   127,   128,   129,   130,   131,   132,   113,   354,   167,
      12,    12,    88,   297,    90,   173,   300,   301,   302,   303,
     366,   147,   148,   307,   150,   151,     9,   232,   233,    18,
     156,   157,   158,    16,   297,   296,   169,   300,   301,   302,
     303,   167,     9,    12,   307,     9,     9,   173,    11,    16,
     255,    12,    16,   150,   151,   181,   103,   104,   105,   106,
     157,   158,   346,   110,    12,   340,   241,    94,   194,   195,
       9,   197,   198,   199,   200,   201,   202,    16,   204,   287,
     206,    12,   208,   346,   210,   182,   212,   362,    44,   105,
     106,   110,   225,   219,   110,    10,   111,   112,   224,    12,
     226,    12,    12,    10,   287,   238,    97,    15,    46,    88,
      10,    81,    18,   210,   211,     9,   324,   322,    10,    14,
     246,    10,   248,   256,   101,    16,    16,    46,    12,   255,
      16,   257,   258,   259,   231,   261,   262,   263,   264,   265,
      16,   324,    16,    16,    16,    -1,   191,   355,    44,   108,
     276,    47,    -1,   267,   280,   288,   282,   254,   284,    41,
      32,    33,    34,    -1,    -1,    37,    -1,    -1,   308,    -1,
      42,   297,   355,    -1,   300,   301,   302,   303,    -1,    -1,
      -1,   307,    54,   309,   310,   311,    -1,   284,   285,    -1,
      -1,    -1,    88,    89,    90,   328,   336,    69,    -1,    -1,
     314,    -1,    -1,    -1,   330,    -1,   364,   333,    -1,   335,
      98,   337,   338,   310,   311,   341,   104,   113,    -1,    -1,
     346,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   100,   101,
      -1,   357,   329,   255,    -1,   257,   258,   259,    -1,   261,
      -1,    -1,    -1,   265,    -1,    -1,    -1,    -1,    -1,   137,
     138,    -1,   124,    -1,    -1,   127,   128,   129,   130,   131,
     132,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   147,   148,    -1,   150,   151,
      -1,    -1,    -1,    -1,   156,   157,   158,    -1,    -1,   177,
      -1,   179,    -1,    -1,    -1,   167,    -1,    -1,    -1,    -1,
      -1,   173,   190,    -1,   192,    -1,    -1,    -1,   330,   181,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   195,    -1,   197,   198,   199,   200,   201,
     202,    -1,   204,    -1,   206,    -1,   208,    -1,   210,    -1,
     212,    -1,    -1,    -1,    -1,    -1,    -1,   219,    -1,    -1,
      -1,    -1,   224,    -1,   226,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   251,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   246,    -1,   248,    -1,    -1,    16,
      -1,    -1,    -1,   255,    -1,   257,   258,   259,    -1,   261,
     262,   263,   264,   265,    -1,    32,    33,    34,    -1,    -1,
      37,    -1,    -1,    -1,   276,    42,    -1,    -1,   280,    -1,
     282,    -1,   284,    -1,    -1,    -1,    -1,    54,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   297,    -1,    -1,   300,   301,
     302,   303,    69,    -1,    -1,   307,    -1,   309,   310,   311,
      -1,    -1,    -1,   331,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   330,    -1,
      -1,    -1,   350,   100,   101,   337,   338,    -1,   356,   341,
      -1,    -1,    -1,    -1,   346,    -1,    -1,   365,    -1,    -1,
      -1,    -1,   370,    -1,    -1,   357,    -1,   124,    -1,    -1,
     127,   128,   129,   130,   131,   132,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     147,   148,    -1,   150,   151,    -1,    -1,    -1,    -1,   156,
     157,   158,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     167,    -1,    -1,    -1,    -1,    -1,   173,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   181,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   195,    -1,
     197,   198,   199,   200,   201,   202,    -1,   204,    -1,   206,
      -1,   208,    -1,   210,    -1,   212,    -1,    -1,    -1,    -1,
      -1,    -1,   219,    -1,    -1,    -1,    -1,   224,    -1,   226,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   246,
      -1,   248,    -1,    -1,    -1,    -1,     5,    -1,   255,    -1,
     257,   258,   259,    12,   261,   262,   263,   264,   265,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   276,
      -1,    -1,    -1,   280,    -1,   282,    -1,   284,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     297,    -1,    -1,   300,   301,   302,   303,    -1,    57,    58,
     307,    -1,   309,   310,   311,    -1,    65,    66,    67,    68,
      -1,    -1,    71,    72,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   330,    -1,    -1,    -1,    -1,    87,    -1,
     337,   338,    -1,    -1,   341,    94,    -1,    -1,    -1,   346,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,
     357,   110,    -1,    -1,   113,   114,   115,   116,   117,   118,
      -1,   120,   121,   122,    -1,    -1,   125,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   133,    -1,    -1,   136,    -1,    -1,
      -1,    -1,   141,    -1,   143,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     0,     1,    -1,    -1,    -1,
      -1,    -1,     7,     8,    -1,    10,    11,    12,    -1,    14,
      15,   170,    -1,    -1,    -1,    -1,   175,    -1,    -1,    -1,
      -1,   180,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   191,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    47,    48,    -1,    50,    -1,    -1,    -1,    -1,
      -1,    56,    57,    58,    -1,    60,    -1,    62,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     229,    -1,    -1,   232,   233,    80,    -1,    -1,   237,    -1,
      -1,    -1,   241,    88,    89,    90,    91,    92,    93,    -1,
      -1,    -1,    97,    98,    99,    -1,    -1,    -1,   103,   104,
      -1,    -1,    -1,    -1,   109,    -1,   111,   112,   113,    -1,
      -1,   270,   271,   272,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   296,    10,    11,
      12,    -1,    14,    15,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   320,    -1,   322,    -1,    -1,   325,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    47,    48,    49,    50,    51,
      52,    53,    54,    55,   343,    57,    58,    59,    60,    -1,
      62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,    11,
      12,    -1,    14,    15,    -1,    17,    88,    89,    90,    91,
      92,    93,    -1,    -1,    -1,    97,    98,    99,   100,   101,
      -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,   111,
     112,   113,    -1,    -1,    -1,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    -1,    57,    58,    59,    60,    -1,
      62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,    11,
      12,    -1,    14,    15,    -1,    17,    88,    89,    90,    91,
      92,    93,    -1,    -1,    -1,    97,    98,    99,   100,   101,
      -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,   111,
     112,   113,    -1,    -1,    -1,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    -1,    57,    58,    59,    60,    -1,
      62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,    11,
      12,    -1,    14,    15,    -1,    17,    88,    89,    90,    91,
      92,    93,    -1,    -1,    -1,    97,    98,    99,   100,   101,
      -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,   111,
     112,   113,    -1,    -1,    -1,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    -1,    57,    58,    59,    60,    -1,
      62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,    11,
      12,    -1,    14,    15,    -1,    17,    88,    89,    90,    91,
      92,    93,    -1,    -1,    -1,    97,    98,    99,   100,   101,
      -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,   111,
     112,   113,    -1,    -1,    -1,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    -1,    57,    58,    59,    60,    -1,
      62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,    11,
      12,    -1,    14,    15,    -1,    17,    88,    89,    90,    91,
      92,    93,    -1,    -1,    -1,    97,    98,    99,   100,   101,
      -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,   111,
     112,   113,    -1,    -1,    -1,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    -1,    57,    58,    59,    60,    -1,
      62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,    10,
      11,    12,    -1,    14,    15,    -1,    88,    89,    90,    91,
      92,    93,    -1,    -1,    -1,    97,    98,    99,   100,   101,
      -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,   111,
     112,   113,    -1,    -1,    -1,    -1,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    -1,    57,    58,    59,    60,
      -1,    62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,
       1,    -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,
      11,    12,    -1,    14,    15,    -1,    17,    88,    89,    90,
      91,    92,    93,    -1,    -1,    -1,    97,    98,    99,   100,
     101,    -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,
     111,   112,   113,    -1,    -1,    -1,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    -1,    57,    58,    59,    60,
      -1,    62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,
       1,    -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,
      11,    12,    -1,    14,    15,    -1,    17,    88,    89,    90,
      91,    92,    93,    -1,    -1,    -1,    97,    98,    99,   100,
     101,    -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,
     111,   112,   113,    -1,    -1,    -1,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    -1,    57,    58,    59,    60,
      -1,    62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,
       1,    -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,
      11,    12,    -1,    14,    15,    -1,    -1,    88,    89,    90,
      91,    92,    93,    -1,    -1,    -1,    97,    98,    99,   100,
     101,    -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,
     111,   112,   113,    -1,    -1,    -1,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    -1,    57,    58,    59,    60,
      -1,    62,    63,    64,    -1,    -1,    -1,    -1,    -1,     1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    78,    79,    80,
      12,    -1,    14,    15,    -1,    -1,    -1,    88,    89,    90,
      91,    92,    93,    -1,    -1,    -1,    97,    98,    99,   100,
     101,    -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,
     111,   112,   113,    -1,    -1,    47,    48,    -1,    50,    -1,
      52,    -1,    -1,    -1,    -1,    57,    58,     1,    60,    -1,
      62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    12,    -1,
      -1,    15,    16,    -1,    -1,    -1,    78,    79,    80,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    88,    89,    90,    91,
      92,    93,    -1,    -1,    -1,    97,    98,    99,    -1,    -1,
      -1,   103,   104,    47,    48,    -1,    50,   109,    52,   111,
     112,   113,    -1,    57,    58,    -1,    60,    -1,    62,    -1,
      -1,     7,     8,    -1,    -1,    11,    12,    -1,    -1,    15,
      -1,    -1,    -1,    -1,    78,    79,    80,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    88,    89,    90,    91,    92,    93,
      -1,    -1,    -1,    97,    98,    99,    -1,    -1,    -1,   103,
     104,    47,    48,    -1,    50,   109,    -1,   111,   112,   113,
      56,    57,    58,    -1,    60,    -1,    62,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    88,    89,    90,    91,    92,    93,    -1,    -1,
      -1,    97,    98,    99,    -1,    -1,    -1,   103,   104,    -1,
      -1,    -1,    -1,   109,    -1,   111,   112,   113,     9,    -1,
      -1,    12,    13,    -1,    15,    16,    -1,    -1,    -1,    -1,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    -1,    50,
      -1,    -1,    -1,    -1,    -1,    -1,    57,    58,    -1,    60,
      -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,    89,    90,
      91,    92,    93,    -1,    95,    -1,    97,    98,    99,    -1,
      -1,    -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,
     111,   112,   113,     9,    -1,    -1,    12,    13,    -1,    15,
      16,    -1,    -1,    -1,    -1,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    -1,    50,    -1,    -1,    -1,    -1,    -1,
      -1,    57,    58,    -1,    60,    -1,    62,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    88,    89,    90,    91,    92,    93,    -1,    95,
      -1,    97,    98,    99,    -1,    -1,    -1,   103,   104,    -1,
      -1,    -1,    -1,   109,    -1,   111,   112,   113,     9,    -1,
      -1,    12,    13,    -1,    15,    16,    -1,    -1,    -1,    -1,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    -1,    50,
      -1,    -1,    -1,    -1,    -1,    -1,    57,    58,    -1,    60,
      -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,    89,    90,
      91,    92,    93,    -1,    95,    -1,    97,    98,    99,    -1,
      -1,    -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,
     111,   112,   113,     9,    -1,    -1,    12,    13,    -1,    15,
      16,    -1,    -1,    -1,    -1,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    -1,    50,    -1,    -1,    -1,    -1,    -1,
      -1,    57,    58,    -1,    60,    -1,    62,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    88,    89,    90,    91,    92,    93,    -1,    95,
      -1,    97,    98,    99,    -1,    -1,     9,   103,   104,    12,
      13,    -1,    15,   109,    -1,   111,   112,   113,    -1,    -1,
      23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    -1,    50,    -1,    -1,
      -1,    -1,    -1,    -1,    57,    58,    -1,    60,    -1,    62,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    88,    89,    90,    91,    92,
      93,    -1,    95,    -1,    97,    98,    99,    -1,    -1,    -1,
     103,   104,    -1,    -1,    -1,    -1,   109,    -1,   111,   112,
     113,    10,    -1,    12,    13,    14,    15,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,
      -1,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      -1,    50,    -1,    -1,    -1,    -1,    -1,    -1,    57,    58,
      -1,    60,    -1,    62,    -1,    -1,    -1,    -1,    -1,    10,
      -1,    12,    -1,    14,    15,    -1,    -1,    -1,    -1,    -1,
      -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,
      89,    90,    91,    92,    93,    -1,    95,    -1,    97,    98,
      99,    -1,    -1,    -1,   103,   104,    47,    48,    -1,    50,
     109,    -1,   111,   112,   113,    -1,    57,    58,    -1,    60,
      -1,    62,    -1,    -1,    -1,    -1,    -1,    10,    -1,    12,
      -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,    -1,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,    89,    90,
      91,    92,    93,    -1,    -1,    -1,    97,    98,    99,    -1,
      -1,    -1,   103,   104,    47,    48,    -1,    50,   109,    -1,
     111,   112,   113,    -1,    57,    58,    -1,    60,    -1,    62,
      -1,    -1,    -1,    -1,    -1,    10,    -1,    12,    -1,    -1,
      15,    -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    88,    89,    90,    91,    92,
      93,    -1,    -1,    -1,    97,    98,    99,    -1,    -1,    -1,
     103,   104,    47,    48,    -1,    50,   109,    -1,   111,   112,
     113,    -1,    57,    58,    -1,    60,    -1,    62,    -1,    -1,
      -1,    -1,    -1,    10,    -1,    12,    -1,    -1,    15,    -1,
      -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    88,    89,    90,    91,    92,    93,    -1,
      -1,    -1,    97,    98,    99,    -1,    -1,    -1,   103,   104,
      47,    48,    -1,    50,   109,    -1,   111,   112,   113,    -1,
      57,    58,    -1,    60,    -1,    62,    -1,    -1,    -1,    -1,
      -1,    10,    -1,    12,    -1,    -1,    15,    -1,    -1,    -1,
      -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    88,    89,    90,    91,    92,    93,    -1,    -1,    -1,
      97,    98,    99,    -1,    -1,    -1,   103,   104,    47,    48,
      -1,    50,   109,    -1,   111,   112,   113,    -1,    57,    58,
      -1,    60,    -1,    62,    -1,    -1,    -1,    -1,    -1,    10,
      -1,    12,    -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,
      -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,
      89,    90,    91,    92,    93,    -1,    -1,    -1,    97,    98,
      99,    -1,    -1,    -1,   103,   104,    47,    48,    -1,    50,
     109,    -1,   111,   112,   113,    -1,    57,    58,    -1,    60,
      -1,    62,    -1,    -1,    -1,    -1,    -1,    10,    -1,    12,
      -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,    -1,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,    89,    90,
      91,    92,    93,    -1,    -1,    -1,    97,    98,    99,    -1,
      -1,    -1,   103,   104,    47,    48,    -1,    50,   109,    -1,
     111,   112,   113,    -1,    57,    58,    -1,    60,    -1,    62,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    88,    89,    90,    91,    92,
      93,    -1,    -1,    -1,    97,    98,    99,    -1,    -1,    -1,
     103,   104,    -1,    -1,    -1,    -1,   109,    -1,   111,   112,
     113,    12,    13,    -1,    15,    16,    -1,    -1,    -1,    -1,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    -1,    50,
      -1,    -1,    -1,    -1,    -1,    -1,    57,    58,    -1,    60,
      -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,    89,    90,
      91,    92,    93,    -1,    95,    -1,    97,    98,    99,    -1,
      -1,    -1,   103,   104,    12,    13,    -1,    15,   109,    -1,
     111,   112,   113,    -1,    -1,    23,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    -1,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    -1,    50,    -1,    -1,    -1,    -1,    -1,    -1,    57,
      58,    -1,    60,    -1,    62,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      88,    89,    90,    91,    92,    93,    -1,    95,    96,    97,
      98,    99,    -1,    -1,    -1,   103,   104,    -1,    -1,    -1,
      -1,   109,    -1,   111,   112,   113,    12,    13,    -1,    15,
      16,    -1,    -1,    -1,    -1,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    -1,    50,    -1,    -1,    -1,    -1,    -1,
      -1,    57,    58,    -1,    60,    -1,    62,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    88,    89,    90,    91,    92,    93,    -1,    95,
      -1,    97,    98,    99,    -1,    -1,    -1,   103,   104,    -1,
      -1,    -1,    -1,   109,    -1,   111,   112,   113,    12,    13,
      -1,    15,    16,    -1,    -1,    -1,    -1,    -1,    -1,    23,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    38,    -1,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    -1,    50,    -1,    -1,    -1,
      -1,    -1,    -1,    57,    58,    -1,    60,    -1,    62,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    88,    89,    90,    91,    92,    93,
      -1,    95,    -1,    97,    98,    99,    -1,    -1,    -1,   103,
     104,    -1,    -1,    -1,    -1,   109,    -1,   111,   112,   113,
      12,    13,    -1,    15,    16,    -1,    -1,    -1,    -1,    -1,
      -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    -1,    50,    -1,
      -1,    -1,    -1,    -1,    -1,    57,    58,    -1,    60,    -1,
      62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    88,    89,    90,    91,
      92,    93,    -1,    95,    -1,    97,    98,    99,    -1,    -1,
      -1,   103,   104,    -1,    -1,    -1,    -1,   109,    -1,   111,
     112,   113,    12,    13,    -1,    15,    16,    -1,    -1,    -1,
      -1,    -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    -1,
      50,    -1,    -1,    -1,    -1,    -1,    -1,    57,    58,    -1,
      60,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,    89,
      90,    91,    92,    93,    -1,    95,    -1,    97,    98,    99,
      -1,    -1,    -1,   103,   104,    -1,    -1,    -1,    -1,   109,
      -1,   111,   112,   113,    12,    13,    -1,    15,    16,    -1,
      -1,    -1,    -1,    -1,    -1,    23,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      38,    -1,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    -1,    50,    -1,    -1,    -1,    -1,    -1,    -1,    57,
      58,    -1,    60,    -1,    62,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      88,    89,    90,    91,    92,    93,    -1,    95,    -1,    97,
      98,    99,    -1,    -1,    -1,   103,   104,    12,    13,    14,
      15,   109,    -1,   111,   112,   113,    -1,    -1,    23,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    37,    38,    -1,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    -1,    50,    -1,    -1,    -1,    -1,
      -1,    -1,    57,    58,    -1,    60,    -1,    62,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    88,    89,    90,    91,    92,    93,    -1,
      95,    -1,    97,    98,    99,    -1,    -1,    -1,   103,   104,
      -1,    -1,    -1,    -1,   109,    -1,   111,   112,   113,    12,
      13,    -1,    15,    16,    -1,    -1,    -1,    -1,    -1,    -1,
      23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    -1,    50,    -1,    -1,
      -1,    -1,    -1,    -1,    57,    58,    -1,    60,    -1,    62,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    88,    89,    90,    91,    92,
      93,    -1,    95,    -1,    97,    98,    99,    -1,    -1,    -1,
     103,   104,    12,    13,    -1,    15,   109,    -1,   111,   112,
     113,    -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    -1,
      50,    -1,    -1,    -1,    -1,    -1,    -1,    57,    58,    -1,
      60,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,    89,
      90,    91,    92,    93,    -1,    95,    -1,    97,    98,    99,
      -1,    -1,    -1,   103,   104,    12,    13,    -1,    15,   109,
      -1,   111,   112,   113,    -1,    -1,    23,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    -1,    -1,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    -1,    50,    -1,    -1,    -1,    -1,    -1,    -1,
      57,    58,    -1,    60,    -1,    62,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    88,    89,    90,    91,    92,    93,    -1,    -1,    -1,
      97,    98,    99,    -1,    -1,    -1,   103,   104,    12,    13,
      -1,    15,   109,    -1,   111,   112,   113,    -1,    -1,    23,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    -1,    50,    -1,    -1,    -1,
      -1,    -1,    -1,    57,    58,    -1,    60,    -1,    62,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    88,    89,    90,    91,    92,    93,
      -1,    -1,    -1,    97,    98,    99,    -1,    -1,    -1,   103,
     104,    12,    13,    -1,    15,   109,    -1,   111,   112,   113,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    -1,    50,
      -1,    -1,    -1,    -1,    -1,    -1,    57,    58,    -1,    60,
      -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,    89,    90,
      91,    92,    93,    12,    -1,    -1,    15,    98,    99,    -1,
      -1,    -1,   103,   104,    23,    -1,    -1,    -1,   109,    -1,
     111,   112,   113,    -1,    -1,    -1,    -1,    -1,    37,    38,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    46,    47,    48,
      -1,    50,    -1,    -1,    -1,    -1,    -1,    -1,    57,    58,
      -1,    60,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,
      89,    90,    91,    92,    93,    -1,    95,    96,    97,    98,
      99,    -1,    -1,    -1,   103,   104,    12,    -1,    -1,    15,
     109,    -1,   111,   112,   113,    -1,    -1,    23,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    47,    48,    -1,    50,    -1,    -1,    -1,    -1,    -1,
      -1,    57,    58,    -1,    60,    -1,    62,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    80,    -1,    -1,    12,    -1,    -1,
      15,    -1,    88,    89,    90,    91,    92,    93,    23,    95,
      -1,    97,    98,    99,    -1,    -1,    -1,   103,   104,    -1,
      -1,    -1,    37,   109,    -1,   111,   112,   113,    -1,    -1,
      -1,    46,    47,    48,    -1,    50,    -1,    -1,    -1,    -1,
      -1,    -1,    57,    58,    -1,    60,    -1,    62,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    12,    -1,    -1,
      15,    -1,    -1,    -1,    -1,    80,    -1,    -1,    23,    -1,
      -1,    -1,    -1,    88,    89,    90,    91,    92,    93,    -1,
      -1,    -1,    97,    98,    99,    -1,    -1,    -1,   103,   104,
      -1,    46,    47,    48,   109,    50,   111,   112,   113,    -1,
      -1,    -1,    57,    58,    -1,    60,    -1,    62,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    12,    -1,    -1,    15,    16,
      -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    88,    89,    90,    91,    92,    93,    -1,
      -1,    -1,    97,    98,    99,    -1,    -1,    -1,   103,   104,
      47,    48,    -1,    50,   109,    -1,   111,   112,   113,    -1,
      57,    58,    -1,    60,    -1,    62,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    12,    -1,    -1,    15,    16,    -1,    -1,
      -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    88,    89,    90,    91,    92,    93,    -1,    -1,    -1,
      97,    98,    99,    -1,    -1,    -1,   103,   104,    47,    48,
      -1,    50,   109,    -1,   111,   112,   113,    -1,    57,    58,
      -1,    60,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    12,    -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,
      -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,
      89,    90,    91,    92,    93,    -1,    -1,    -1,    97,    98,
      99,    -1,    -1,    -1,   103,   104,    47,    48,    -1,    50,
     109,    -1,   111,   112,   113,    -1,    57,    58,    -1,    60,
      -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      71,    12,    -1,    -1,    15,    -1,    -1,    -1,    -1,    80,
      -1,    -1,    23,    -1,    -1,    -1,    -1,    88,    89,    90,
      91,    92,    93,    -1,    -1,    -1,    97,    98,    99,    -1,
      -1,    -1,   103,   104,    -1,    46,    47,    48,   109,    50,
     111,   112,   113,    -1,    -1,    -1,    57,    58,    -1,    60,
      -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    12,
      -1,    -1,    15,    -1,    -1,    -1,    -1,    -1,    -1,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,    89,    90,
      91,    92,    93,    -1,    -1,    -1,    -1,    98,    99,    -1,
      -1,    -1,   103,   104,    47,    48,    -1,    50,   109,    -1,
     111,   112,   113,    -1,    57,    58,    -1,    60,    -1,    62,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    12,    -1,    -1,
      15,    -1,    -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    88,    89,    90,    91,    92,
      93,    -1,    -1,    -1,    97,    98,    99,    -1,    -1,    -1,
     103,   104,    47,    48,    -1,    50,   109,    -1,   111,   112,
     113,    -1,    57,    58,    -1,    60,    -1,    62,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    12,    -1,    -1,    15,    -1,
      -1,    -1,    -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    88,    89,    90,    91,    92,    93,    -1,
      -1,    -1,    97,    98,    99,    -1,    -1,    -1,   103,   104,
      47,    48,    -1,    50,   109,    -1,   111,   112,   113,    -1,
      57,    58,    -1,    60,    -1,    62,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    12,    -1,    -1,    15,    -1,    -1,    -1,
      -1,    -1,    -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    88,    89,    90,    91,    92,    93,    -1,    -1,    -1,
      97,    98,    99,    -1,    -1,    -1,   103,   104,    47,    48,
      -1,    50,   109,    -1,   111,   112,   113,    -1,    57,    58,
      -1,    60,    -1,    62,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    88,
      89,    90,    91,    92,    93,    -1,    -1,    -1,    97,    98,
      99,    -1,    -1,    -1,   103,   104,    -1,    -1,    -1,    -1,
     109,    -1,   111,   112,   113
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,    10,    14,   116,   131,   133,   145,     0,     7,
       8,    11,    12,    15,    47,    48,    50,    56,    57,    58,
      60,    62,    80,    88,    89,    90,    91,    92,    93,    97,
      98,    99,   103,   104,   109,   111,   112,   113,   128,   134,
     135,   137,   140,   147,   148,   158,   159,   160,   161,   163,
      10,    14,   128,   128,   140,   141,   149,    12,    12,   147,
     160,   161,    88,    91,   126,    12,    12,    12,    12,    44,
     161,    12,    12,   160,   160,   147,   160,   161,   161,   160,
       1,    10,    14,    49,    51,    52,    53,    54,    55,    59,
      63,    64,    78,    79,   100,   101,   120,   122,   127,   128,
     140,   144,   151,   153,   157,   164,     9,   128,   131,    13,
      23,    37,    38,    40,    41,    42,    43,    44,    45,    46,
      95,   117,   118,   160,    15,    12,    93,    15,   103,   104,
     105,   106,   110,    71,   111,   112,    18,   157,   157,     9,
      16,   119,    16,   119,    94,    16,   138,   140,   140,    12,
     140,   140,   138,    16,   138,   160,    44,   140,   140,    10,
     129,   130,    14,   129,   152,   152,   163,   140,   152,    12,
      12,   152,   152,   140,   152,    12,    10,   154,   153,   157,
      12,   139,   142,   143,   147,   160,   161,   152,    17,   153,
     156,   130,   157,   135,    97,   140,   148,   140,   140,   140,
     140,   140,   140,   163,   140,    10,   140,    10,   140,   160,
     140,   148,    71,   160,   160,   160,   160,   160,   160,   140,
     138,    17,    17,    10,   140,    46,   140,    15,    16,   119,
      88,   162,   119,   119,    16,    16,   160,   119,   119,    10,
     130,    18,   152,   132,   151,   163,   140,   152,   140,   153,
      81,   121,    17,   146,   141,    23,    46,    95,   117,   118,
     160,   119,    13,    39,    42,    71,   153,   134,    17,   161,
      96,   119,   119,   160,    19,   163,   140,    16,   119,   150,
     140,   148,   140,   148,   140,   163,   138,    14,    46,   150,
     150,   155,    10,   153,    10,    16,    12,   139,   148,   163,
     139,   139,   139,   139,   160,   160,   160,   139,   128,   140,
     140,   140,    88,    10,   136,    16,    16,    16,    16,    16,
     119,    16,   119,    19,    14,   130,   163,   101,    46,   141,
      96,   157,    16,   119,    16,   119,   128,   140,   140,   148,
     130,   140,   150,    12,   163,    16,   139,    17,   161,   161,
     157,    16,    16,    16,   132,    14,   125,   140,    16,    16,
      17,   150,   130,   153,    16,   124,   132,   152,   153,   150,
     123,   153
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 99 "awkgram.y"
    { if (errorflag==0)
			winner = (Node *)stat3(PROGRAM, beginloc, (yyvsp[(1) - (1)].p), endloc); ;}
    break;

  case 3:
#line 101 "awkgram.y"
    { yyclearin; bracecheck(); SYNTAX("bailing out"); ;}
    break;

  case 14:
#line 125 "awkgram.y"
    {inloop++;;}
    break;

  case 15:
#line 126 "awkgram.y"
    { --inloop; (yyval.p) = stat4(FOR, (yyvsp[(3) - (12)].p), notnull((yyvsp[(6) - (12)].p)), (yyvsp[(9) - (12)].p), (yyvsp[(12) - (12)].p)); ;}
    break;

  case 16:
#line 127 "awkgram.y"
    {inloop++;;}
    break;

  case 17:
#line 128 "awkgram.y"
    { --inloop; (yyval.p) = stat4(FOR, (yyvsp[(3) - (10)].p), NIL, (yyvsp[(7) - (10)].p), (yyvsp[(10) - (10)].p)); ;}
    break;

  case 18:
#line 129 "awkgram.y"
    {inloop++;;}
    break;

  case 19:
#line 130 "awkgram.y"
    { --inloop; (yyval.p) = stat3(IN, (yyvsp[(3) - (8)].p), makearr((yyvsp[(5) - (8)].p)), (yyvsp[(8) - (8)].p)); ;}
    break;

  case 20:
#line 134 "awkgram.y"
    { setfname((yyvsp[(1) - (1)].cp)); ;}
    break;

  case 21:
#line 135 "awkgram.y"
    { setfname((yyvsp[(1) - (1)].cp)); ;}
    break;

  case 22:
#line 139 "awkgram.y"
    { (yyval.p) = notnull((yyvsp[(3) - (4)].p)); ;}
    break;

  case 27:
#line 151 "awkgram.y"
    { (yyval.i) = 0; ;}
    break;

  case 29:
#line 156 "awkgram.y"
    { (yyval.i) = 0; ;}
    break;

  case 31:
#line 162 "awkgram.y"
    { (yyval.p) = 0; ;}
    break;

  case 33:
#line 167 "awkgram.y"
    { (yyval.p) = 0; ;}
    break;

  case 34:
#line 168 "awkgram.y"
    { (yyval.p) = (yyvsp[(2) - (3)].p); ;}
    break;

  case 35:
#line 172 "awkgram.y"
    { (yyval.p) = notnull((yyvsp[(1) - (1)].p)); ;}
    break;

  case 36:
#line 176 "awkgram.y"
    { (yyval.p) = stat2(PASTAT, (yyvsp[(1) - (1)].p), stat2(PRINT, rectonode(), NIL)); ;}
    break;

  case 37:
#line 177 "awkgram.y"
    { (yyval.p) = stat2(PASTAT, (yyvsp[(1) - (4)].p), (yyvsp[(3) - (4)].p)); ;}
    break;

  case 38:
#line 178 "awkgram.y"
    { (yyval.p) = pa2stat((yyvsp[(1) - (4)].p), (yyvsp[(4) - (4)].p), stat2(PRINT, rectonode(), NIL)); ;}
    break;

  case 39:
#line 179 "awkgram.y"
    { (yyval.p) = pa2stat((yyvsp[(1) - (7)].p), (yyvsp[(4) - (7)].p), (yyvsp[(6) - (7)].p)); ;}
    break;

  case 40:
#line 180 "awkgram.y"
    { (yyval.p) = stat2(PASTAT, NIL, (yyvsp[(2) - (3)].p)); ;}
    break;

  case 41:
#line 182 "awkgram.y"
    { beginloc = linkum(beginloc, (yyvsp[(3) - (4)].p)); (yyval.p) = 0; ;}
    break;

  case 42:
#line 184 "awkgram.y"
    { endloc = linkum(endloc, (yyvsp[(3) - (4)].p)); (yyval.p) = 0; ;}
    break;

  case 43:
#line 185 "awkgram.y"
    {infunc = true;;}
    break;

  case 44:
#line 186 "awkgram.y"
    { infunc = false; curfname=0; defn((Cell *)(yyvsp[(2) - (9)].p), (yyvsp[(4) - (9)].p), (yyvsp[(8) - (9)].p)); (yyval.p) = 0; ;}
    break;

  case 46:
#line 191 "awkgram.y"
    { (yyval.p) = linkum((yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 48:
#line 196 "awkgram.y"
    { (yyval.p) = linkum((yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 49:
#line 200 "awkgram.y"
    { (yyval.p) = op2((yyvsp[(2) - (3)].i), (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 50:
#line 202 "awkgram.y"
    { (yyval.p) = op3(CONDEXPR, notnull((yyvsp[(1) - (5)].p)), (yyvsp[(3) - (5)].p), (yyvsp[(5) - (5)].p)); ;}
    break;

  case 51:
#line 204 "awkgram.y"
    { (yyval.p) = op2(BOR, notnull((yyvsp[(1) - (3)].p)), notnull((yyvsp[(3) - (3)].p))); ;}
    break;

  case 52:
#line 206 "awkgram.y"
    { (yyval.p) = op2(AND, notnull((yyvsp[(1) - (3)].p)), notnull((yyvsp[(3) - (3)].p))); ;}
    break;

  case 53:
#line 207 "awkgram.y"
    { (yyval.p) = op3((yyvsp[(2) - (3)].i), NIL, (yyvsp[(1) - (3)].p), (Node*)makedfa((yyvsp[(3) - (3)].s), 0)); ;}
    break;

  case 54:
#line 209 "awkgram.y"
    { if (constnode((yyvsp[(3) - (3)].p)))
			(yyval.p) = op3((yyvsp[(2) - (3)].i), NIL, (yyvsp[(1) - (3)].p), (Node*)makedfa(strnode((yyvsp[(3) - (3)].p)), 0));
		  else
			(yyval.p) = op3((yyvsp[(2) - (3)].i), (Node *)1, (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 55:
#line 213 "awkgram.y"
    { (yyval.p) = op2(INTEST, (yyvsp[(1) - (3)].p), makearr((yyvsp[(3) - (3)].p))); ;}
    break;

  case 56:
#line 214 "awkgram.y"
    { (yyval.p) = op2(INTEST, (yyvsp[(2) - (5)].p), makearr((yyvsp[(5) - (5)].p))); ;}
    break;

  case 57:
#line 215 "awkgram.y"
    { (yyval.p) = op2(CAT, (yyvsp[(1) - (2)].p), (yyvsp[(2) - (2)].p)); ;}
    break;

  case 60:
#line 221 "awkgram.y"
    { (yyval.p) = op2((yyvsp[(2) - (3)].i), (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 61:
#line 223 "awkgram.y"
    { (yyval.p) = op3(CONDEXPR, notnull((yyvsp[(1) - (5)].p)), (yyvsp[(3) - (5)].p), (yyvsp[(5) - (5)].p)); ;}
    break;

  case 62:
#line 225 "awkgram.y"
    { (yyval.p) = op2(BOR, notnull((yyvsp[(1) - (3)].p)), notnull((yyvsp[(3) - (3)].p))); ;}
    break;

  case 63:
#line 227 "awkgram.y"
    { (yyval.p) = op2(AND, notnull((yyvsp[(1) - (3)].p)), notnull((yyvsp[(3) - (3)].p))); ;}
    break;

  case 64:
#line 228 "awkgram.y"
    { (yyval.p) = op2((yyvsp[(2) - (3)].i), (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 65:
#line 229 "awkgram.y"
    { (yyval.p) = op2((yyvsp[(2) - (3)].i), (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 66:
#line 230 "awkgram.y"
    { (yyval.p) = op2((yyvsp[(2) - (3)].i), (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 67:
#line 231 "awkgram.y"
    { (yyval.p) = op2((yyvsp[(2) - (3)].i), (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 68:
#line 232 "awkgram.y"
    { (yyval.p) = op2((yyvsp[(2) - (3)].i), (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 69:
#line 233 "awkgram.y"
    { (yyval.p) = op2((yyvsp[(2) - (3)].i), (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 70:
#line 234 "awkgram.y"
    { (yyval.p) = op3((yyvsp[(2) - (3)].i), NIL, (yyvsp[(1) - (3)].p), (Node*)makedfa((yyvsp[(3) - (3)].s), 0)); ;}
    break;

  case 71:
#line 236 "awkgram.y"
    { if (constnode((yyvsp[(3) - (3)].p)))
			(yyval.p) = op3((yyvsp[(2) - (3)].i), NIL, (yyvsp[(1) - (3)].p), (Node*)makedfa(strnode((yyvsp[(3) - (3)].p)), 0));
		  else
			(yyval.p) = op3((yyvsp[(2) - (3)].i), (Node *)1, (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 72:
#line 240 "awkgram.y"
    { (yyval.p) = op2(INTEST, (yyvsp[(1) - (3)].p), makearr((yyvsp[(3) - (3)].p))); ;}
    break;

  case 73:
#line 241 "awkgram.y"
    { (yyval.p) = op2(INTEST, (yyvsp[(2) - (5)].p), makearr((yyvsp[(5) - (5)].p))); ;}
    break;

  case 74:
#line 242 "awkgram.y"
    {
			if (safe) SYNTAX("cmd | getline is unsafe");
			else (yyval.p) = op3(GETLINE, (yyvsp[(4) - (4)].p), itonp((yyvsp[(2) - (4)].i)), (yyvsp[(1) - (4)].p)); ;}
    break;

  case 75:
#line 245 "awkgram.y"
    {
			if (safe) SYNTAX("cmd | getline is unsafe");
			else (yyval.p) = op3(GETLINE, (Node*)0, itonp((yyvsp[(2) - (3)].i)), (yyvsp[(1) - (3)].p)); ;}
    break;

  case 76:
#line 248 "awkgram.y"
    { (yyval.p) = op2(CAT, (yyvsp[(1) - (2)].p), (yyvsp[(2) - (2)].p)); ;}
    break;

  case 79:
#line 254 "awkgram.y"
    { (yyval.p) = linkum((yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 80:
#line 255 "awkgram.y"
    { (yyval.p) = linkum((yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 82:
#line 260 "awkgram.y"
    { (yyval.p) = linkum((yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 83:
#line 264 "awkgram.y"
    { (yyval.p) = rectonode(); ;}
    break;

  case 85:
#line 266 "awkgram.y"
    { (yyval.p) = (yyvsp[(2) - (3)].p); ;}
    break;

  case 94:
#line 283 "awkgram.y"
    { (yyval.p) = op3(MATCH, NIL, rectonode(), (Node*)makedfa((yyvsp[(1) - (1)].s), 0)); ;}
    break;

  case 95:
#line 284 "awkgram.y"
    { (yyval.p) = op1(NOT, notnull((yyvsp[(2) - (2)].p))); ;}
    break;

  case 96:
#line 288 "awkgram.y"
    {startreg();;}
    break;

  case 97:
#line 288 "awkgram.y"
    { (yyval.s) = (yyvsp[(3) - (4)].s); ;}
    break;

  case 100:
#line 296 "awkgram.y"
    {
			if (safe) SYNTAX("print | is unsafe");
			else (yyval.p) = stat3((yyvsp[(1) - (4)].i), (yyvsp[(2) - (4)].p), itonp((yyvsp[(3) - (4)].i)), (yyvsp[(4) - (4)].p)); ;}
    break;

  case 101:
#line 299 "awkgram.y"
    {
			if (safe) SYNTAX("print >> is unsafe");
			else (yyval.p) = stat3((yyvsp[(1) - (4)].i), (yyvsp[(2) - (4)].p), itonp((yyvsp[(3) - (4)].i)), (yyvsp[(4) - (4)].p)); ;}
    break;

  case 102:
#line 302 "awkgram.y"
    {
			if (safe) SYNTAX("print > is unsafe");
			else (yyval.p) = stat3((yyvsp[(1) - (4)].i), (yyvsp[(2) - (4)].p), itonp((yyvsp[(3) - (4)].i)), (yyvsp[(4) - (4)].p)); ;}
    break;

  case 103:
#line 305 "awkgram.y"
    { (yyval.p) = stat3((yyvsp[(1) - (2)].i), (yyvsp[(2) - (2)].p), NIL, NIL); ;}
    break;

  case 104:
#line 306 "awkgram.y"
    { (yyval.p) = stat2(DELETE, makearr((yyvsp[(2) - (5)].p)), (yyvsp[(4) - (5)].p)); ;}
    break;

  case 105:
#line 307 "awkgram.y"
    { (yyval.p) = stat2(DELETE, makearr((yyvsp[(2) - (2)].p)), 0); ;}
    break;

  case 106:
#line 308 "awkgram.y"
    { (yyval.p) = exptostat((yyvsp[(1) - (1)].p)); ;}
    break;

  case 107:
#line 309 "awkgram.y"
    { yyclearin; SYNTAX("illegal statement"); ;}
    break;

  case 110:
#line 318 "awkgram.y"
    { if (!inloop) SYNTAX("break illegal outside of loops");
				  (yyval.p) = stat1(BREAK, NIL); ;}
    break;

  case 111:
#line 320 "awkgram.y"
    {  if (!inloop) SYNTAX("continue illegal outside of loops");
				  (yyval.p) = stat1(CONTINUE, NIL); ;}
    break;

  case 112:
#line 322 "awkgram.y"
    {inloop++;;}
    break;

  case 113:
#line 322 "awkgram.y"
    {--inloop;;}
    break;

  case 114:
#line 323 "awkgram.y"
    { (yyval.p) = stat2(DO, (yyvsp[(3) - (9)].p), notnull((yyvsp[(7) - (9)].p))); ;}
    break;

  case 115:
#line 324 "awkgram.y"
    { (yyval.p) = stat1(EXIT, (yyvsp[(2) - (3)].p)); ;}
    break;

  case 116:
#line 325 "awkgram.y"
    { (yyval.p) = stat1(EXIT, NIL); ;}
    break;

  case 118:
#line 327 "awkgram.y"
    { (yyval.p) = stat3(IF, (yyvsp[(1) - (4)].p), (yyvsp[(2) - (4)].p), (yyvsp[(4) - (4)].p)); ;}
    break;

  case 119:
#line 328 "awkgram.y"
    { (yyval.p) = stat3(IF, (yyvsp[(1) - (2)].p), (yyvsp[(2) - (2)].p), NIL); ;}
    break;

  case 120:
#line 329 "awkgram.y"
    { (yyval.p) = (yyvsp[(2) - (3)].p); ;}
    break;

  case 121:
#line 330 "awkgram.y"
    { if (infunc)
				SYNTAX("next is illegal inside a function");
			  (yyval.p) = stat1(NEXT, NIL); ;}
    break;

  case 122:
#line 333 "awkgram.y"
    { if (infunc)
				SYNTAX("nextfile is illegal inside a function");
			  (yyval.p) = stat1(NEXTFILE, NIL); ;}
    break;

  case 123:
#line 336 "awkgram.y"
    { (yyval.p) = stat1(RETURN, (yyvsp[(2) - (3)].p)); ;}
    break;

  case 124:
#line 337 "awkgram.y"
    { (yyval.p) = stat1(RETURN, NIL); ;}
    break;

  case 126:
#line 339 "awkgram.y"
    {inloop++;;}
    break;

  case 127:
#line 339 "awkgram.y"
    { --inloop; (yyval.p) = stat2(WHILE, (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 128:
#line 340 "awkgram.y"
    { (yyval.p) = 0; ;}
    break;

  case 130:
#line 345 "awkgram.y"
    { (yyval.p) = linkum((yyvsp[(1) - (2)].p), (yyvsp[(2) - (2)].p)); ;}
    break;

  case 134:
#line 354 "awkgram.y"
    { (yyval.cp) = catstr((yyvsp[(1) - (2)].cp), (yyvsp[(2) - (2)].cp)); ;}
    break;

  case 135:
#line 358 "awkgram.y"
    { (yyval.p) = op2(DIVEQ, (yyvsp[(1) - (4)].p), (yyvsp[(4) - (4)].p)); ;}
    break;

  case 136:
#line 359 "awkgram.y"
    { (yyval.p) = op2(ADD, (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 137:
#line 360 "awkgram.y"
    { (yyval.p) = op2(MINUS, (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 138:
#line 361 "awkgram.y"
    { (yyval.p) = op2(MULT, (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 139:
#line 362 "awkgram.y"
    { (yyval.p) = op2(DIVIDE, (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 140:
#line 363 "awkgram.y"
    { (yyval.p) = op2(DIVIDE, (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 141:
#line 364 "awkgram.y"
    { (yyval.p) = op2(MOD, (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 142:
#line 365 "awkgram.y"
    { (yyval.p) = op2(POWER, (yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 143:
#line 366 "awkgram.y"
    { (yyval.p) = op1(UMINUS, (yyvsp[(2) - (2)].p)); ;}
    break;

  case 144:
#line 367 "awkgram.y"
    { (yyval.p) = op1(UPLUS, (yyvsp[(2) - (2)].p)); ;}
    break;

  case 145:
#line 368 "awkgram.y"
    { (yyval.p) = op1(NOT, notnull((yyvsp[(2) - (2)].p))); ;}
    break;

  case 146:
#line 369 "awkgram.y"
    { (yyval.p) = op2(BLTIN, itonp((yyvsp[(1) - (3)].i)), rectonode()); ;}
    break;

  case 147:
#line 370 "awkgram.y"
    { (yyval.p) = op2(BLTIN, itonp((yyvsp[(1) - (4)].i)), (yyvsp[(3) - (4)].p)); ;}
    break;

  case 148:
#line 371 "awkgram.y"
    { (yyval.p) = op2(BLTIN, itonp((yyvsp[(1) - (1)].i)), rectonode()); ;}
    break;

  case 149:
#line 372 "awkgram.y"
    { (yyval.p) = op2(CALL, celltonode((yyvsp[(1) - (3)].cp),CVAR), NIL); ;}
    break;

  case 150:
#line 373 "awkgram.y"
    { (yyval.p) = op2(CALL, celltonode((yyvsp[(1) - (4)].cp),CVAR), (yyvsp[(3) - (4)].p)); ;}
    break;

  case 151:
#line 374 "awkgram.y"
    { (yyval.p) = op1(CLOSE, (yyvsp[(2) - (2)].p)); ;}
    break;

  case 152:
#line 375 "awkgram.y"
    { (yyval.p) = op1(PREDECR, (yyvsp[(2) - (2)].p)); ;}
    break;

  case 153:
#line 376 "awkgram.y"
    { (yyval.p) = op1(PREINCR, (yyvsp[(2) - (2)].p)); ;}
    break;

  case 154:
#line 377 "awkgram.y"
    { (yyval.p) = op1(POSTDECR, (yyvsp[(1) - (2)].p)); ;}
    break;

  case 155:
#line 378 "awkgram.y"
    { (yyval.p) = op1(POSTINCR, (yyvsp[(1) - (2)].p)); ;}
    break;

  case 156:
#line 379 "awkgram.y"
    { (yyval.p) = op3(GETLINE, (yyvsp[(2) - (4)].p), itonp((yyvsp[(3) - (4)].i)), (yyvsp[(4) - (4)].p)); ;}
    break;

  case 157:
#line 380 "awkgram.y"
    { (yyval.p) = op3(GETLINE, NIL, itonp((yyvsp[(2) - (3)].i)), (yyvsp[(3) - (3)].p)); ;}
    break;

  case 158:
#line 381 "awkgram.y"
    { (yyval.p) = op3(GETLINE, (yyvsp[(2) - (2)].p), NIL, NIL); ;}
    break;

  case 159:
#line 382 "awkgram.y"
    { (yyval.p) = op3(GETLINE, NIL, NIL, NIL); ;}
    break;

  case 160:
#line 384 "awkgram.y"
    { (yyval.p) = op2(INDEX, (yyvsp[(3) - (6)].p), (yyvsp[(5) - (6)].p)); ;}
    break;

  case 161:
#line 386 "awkgram.y"
    { SYNTAX("index() doesn't permit regular expressions");
		  (yyval.p) = op2(INDEX, (yyvsp[(3) - (6)].p), (Node*)(yyvsp[(5) - (6)].s)); ;}
    break;

  case 162:
#line 388 "awkgram.y"
    { (yyval.p) = (yyvsp[(2) - (3)].p); ;}
    break;

  case 163:
#line 390 "awkgram.y"
    { (yyval.p) = op3(MATCHFCN, NIL, (yyvsp[(3) - (6)].p), (Node*)makedfa((yyvsp[(5) - (6)].s), 1)); ;}
    break;

  case 164:
#line 392 "awkgram.y"
    { if (constnode((yyvsp[(5) - (6)].p)))
			(yyval.p) = op3(MATCHFCN, NIL, (yyvsp[(3) - (6)].p), (Node*)makedfa(strnode((yyvsp[(5) - (6)].p)), 1));
		  else
			(yyval.p) = op3(MATCHFCN, (Node *)1, (yyvsp[(3) - (6)].p), (yyvsp[(5) - (6)].p)); ;}
    break;

  case 165:
#line 396 "awkgram.y"
    { (yyval.p) = celltonode((yyvsp[(1) - (1)].cp), CCON); ;}
    break;

  case 166:
#line 398 "awkgram.y"
    { (yyval.p) = op4(SPLIT, (yyvsp[(3) - (8)].p), makearr((yyvsp[(5) - (8)].p)), (yyvsp[(7) - (8)].p), (Node*)STRING); ;}
    break;

  case 167:
#line 400 "awkgram.y"
    { (yyval.p) = op4(SPLIT, (yyvsp[(3) - (8)].p), makearr((yyvsp[(5) - (8)].p)), (Node*)makedfa((yyvsp[(7) - (8)].s), 1), (Node *)REGEXPR); ;}
    break;

  case 168:
#line 402 "awkgram.y"
    { (yyval.p) = op4(SPLIT, (yyvsp[(3) - (6)].p), makearr((yyvsp[(5) - (6)].p)), NIL, (Node*)STRING); ;}
    break;

  case 169:
#line 403 "awkgram.y"
    { (yyval.p) = op1((yyvsp[(1) - (4)].i), (yyvsp[(3) - (4)].p)); ;}
    break;

  case 170:
#line 404 "awkgram.y"
    { (yyval.p) = celltonode((yyvsp[(1) - (1)].cp), CCON); ;}
    break;

  case 171:
#line 406 "awkgram.y"
    { (yyval.p) = op4((yyvsp[(1) - (6)].i), NIL, (Node*)makedfa((yyvsp[(3) - (6)].s), 1), (yyvsp[(5) - (6)].p), rectonode()); ;}
    break;

  case 172:
#line 408 "awkgram.y"
    { if (constnode((yyvsp[(3) - (6)].p)))
			(yyval.p) = op4((yyvsp[(1) - (6)].i), NIL, (Node*)makedfa(strnode((yyvsp[(3) - (6)].p)), 1), (yyvsp[(5) - (6)].p), rectonode());
		  else
			(yyval.p) = op4((yyvsp[(1) - (6)].i), (Node *)1, (yyvsp[(3) - (6)].p), (yyvsp[(5) - (6)].p), rectonode()); ;}
    break;

  case 173:
#line 413 "awkgram.y"
    { (yyval.p) = op4((yyvsp[(1) - (8)].i), NIL, (Node*)makedfa((yyvsp[(3) - (8)].s), 1), (yyvsp[(5) - (8)].p), (yyvsp[(7) - (8)].p)); ;}
    break;

  case 174:
#line 415 "awkgram.y"
    { if (constnode((yyvsp[(3) - (8)].p)))
			(yyval.p) = op4((yyvsp[(1) - (8)].i), NIL, (Node*)makedfa(strnode((yyvsp[(3) - (8)].p)), 1), (yyvsp[(5) - (8)].p), (yyvsp[(7) - (8)].p));
		  else
			(yyval.p) = op4((yyvsp[(1) - (8)].i), (Node *)1, (yyvsp[(3) - (8)].p), (yyvsp[(5) - (8)].p), (yyvsp[(7) - (8)].p)); ;}
    break;

  case 175:
#line 420 "awkgram.y"
    { (yyval.p) = op3(SUBSTR, (yyvsp[(3) - (8)].p), (yyvsp[(5) - (8)].p), (yyvsp[(7) - (8)].p)); ;}
    break;

  case 176:
#line 422 "awkgram.y"
    { (yyval.p) = op3(SUBSTR, (yyvsp[(3) - (6)].p), (yyvsp[(5) - (6)].p), NIL); ;}
    break;

  case 179:
#line 428 "awkgram.y"
    { (yyval.p) = op2(ARRAY, makearr((yyvsp[(1) - (4)].p)), (yyvsp[(3) - (4)].p)); ;}
    break;

  case 180:
#line 429 "awkgram.y"
    { (yyval.p) = op1(INDIRECT, celltonode((yyvsp[(1) - (1)].cp), CVAR)); ;}
    break;

  case 181:
#line 430 "awkgram.y"
    { (yyval.p) = op1(INDIRECT, (yyvsp[(2) - (2)].p)); ;}
    break;

  case 182:
#line 434 "awkgram.y"
    { arglist = (yyval.p) = 0; ;}
    break;

  case 183:
#line 435 "awkgram.y"
    { arglist = (yyval.p) = celltonode((yyvsp[(1) - (1)].cp),CVAR); ;}
    break;

  case 184:
#line 436 "awkgram.y"
    {
			checkdup((yyvsp[(1) - (3)].p), (yyvsp[(3) - (3)].cp));
			arglist = (yyval.p) = linkum((yyvsp[(1) - (3)].p),celltonode((yyvsp[(3) - (3)].cp),CVAR)); ;}
    break;

  case 185:
#line 442 "awkgram.y"
    { (yyval.p) = celltonode((yyvsp[(1) - (1)].cp), CVAR); ;}
    break;

  case 186:
#line 443 "awkgram.y"
    { (yyval.p) = op1(ARG, itonp((yyvsp[(1) - (1)].i))); ;}
    break;

  case 187:
#line 444 "awkgram.y"
    { (yyval.p) = op1(VARNF, (Node *) (yyvsp[(1) - (1)].cp)); ;}
    break;

  case 188:
#line 449 "awkgram.y"
    { (yyval.p) = notnull((yyvsp[(3) - (4)].p)); ;}
    break;


/* Line 1267 of yacc.c.  */
#line 3552 "awkgram.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 452 "awkgram.y"


void setfname(Cell *p)
{
	if (isarr(p))
		SYNTAX("%s is an array, not a function", p->nval);
	else if (isfcn(p))
		SYNTAX("you can't define function %s more than once", p->nval);
	curfname = p->nval;
}

int constnode(Node *p)
{
	return isvalue(p) && ((Cell *) (p->narg[0]))->csub == CCON;
}

char *strnode(Node *p)
{
	return ((Cell *)(p->narg[0]))->sval;
}

Node *notnull(Node *n)
{
	switch (n->nobj) {
	case LE: case LT: case EQ: case NE: case GT: case GE:
	case BOR: case AND: case NOT:
		return n;
	default:
		return op2(NE, n, nullnode);
	}
}

void checkdup(Node *vl, Cell *cp)	/* check if name already in list */
{
	char *s = cp->nval;
	for ( ; vl; vl = vl->nnext) {
		if (strcmp(s, ((Cell *)(vl->narg[0]))->nval) == 0) {
			SYNTAX("duplicate argument %s", s);
			break;
		}
	}
}

