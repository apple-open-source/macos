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
     LB = 258,
     RB = 259,
     LP = 260,
     RP = 261,
     MENUS = 262,
     MENU = 263,
     BUTTON = 264,
     DEFAULT_FUNCTION = 265,
     PLUS = 266,
     MINUS = 267,
     ALL = 268,
     OR = 269,
     CURSORS = 270,
     PIXMAPS = 271,
     ICONS = 272,
     COLOR = 273,
     SAVECOLOR = 274,
     MONOCHROME = 275,
     FUNCTION = 276,
     ICONMGR_SHOW = 277,
     ICONMGR = 278,
     WINDOW_FUNCTION = 279,
     ZOOM = 280,
     ICONMGRS = 281,
     ICONMGR_GEOMETRY = 282,
     ICONMGR_NOSHOW = 283,
     MAKE_TITLE = 284,
     GRAYSCALE = 285,
     ICONIFY_BY_UNMAPPING = 286,
     DONT_ICONIFY_BY_UNMAPPING = 287,
     NO_TITLE = 288,
     AUTO_RAISE = 289,
     NO_HILITE = 290,
     ICON_REGION = 291,
     META = 292,
     SHIFT = 293,
     LOCK = 294,
     CONTROL = 295,
     WINDOW = 296,
     TITLE = 297,
     ICON = 298,
     ROOT = 299,
     FRAME = 300,
     COLON = 301,
     EQUALS = 302,
     SQUEEZE_TITLE = 303,
     DONT_SQUEEZE_TITLE = 304,
     START_ICONIFIED = 305,
     NO_TITLE_HILITE = 306,
     TITLE_HILITE = 307,
     MOVE = 308,
     RESIZE = 309,
     WAIT = 310,
     SELECT = 311,
     KILL = 312,
     LEFT_TITLEBUTTON = 313,
     RIGHT_TITLEBUTTON = 314,
     NUMBER = 315,
     KEYWORD = 316,
     NKEYWORD = 317,
     CKEYWORD = 318,
     CLKEYWORD = 319,
     FKEYWORD = 320,
     FSKEYWORD = 321,
     SKEYWORD = 322,
     DKEYWORD = 323,
     JKEYWORD = 324,
     WINDOW_RING = 325,
     WARP_CURSOR = 326,
     ERRORTOKEN = 327,
     NO_STACKMODE = 328,
     STRING = 329
   };
#endif
/* Tokens.  */
#define LB 258
#define RB 259
#define LP 260
#define RP 261
#define MENUS 262
#define MENU 263
#define BUTTON 264
#define DEFAULT_FUNCTION 265
#define PLUS 266
#define MINUS 267
#define ALL 268
#define OR 269
#define CURSORS 270
#define PIXMAPS 271
#define ICONS 272
#define COLOR 273
#define SAVECOLOR 274
#define MONOCHROME 275
#define FUNCTION 276
#define ICONMGR_SHOW 277
#define ICONMGR 278
#define WINDOW_FUNCTION 279
#define ZOOM 280
#define ICONMGRS 281
#define ICONMGR_GEOMETRY 282
#define ICONMGR_NOSHOW 283
#define MAKE_TITLE 284
#define GRAYSCALE 285
#define ICONIFY_BY_UNMAPPING 286
#define DONT_ICONIFY_BY_UNMAPPING 287
#define NO_TITLE 288
#define AUTO_RAISE 289
#define NO_HILITE 290
#define ICON_REGION 291
#define META 292
#define SHIFT 293
#define LOCK 294
#define CONTROL 295
#define WINDOW 296
#define TITLE 297
#define ICON 298
#define ROOT 299
#define FRAME 300
#define COLON 301
#define EQUALS 302
#define SQUEEZE_TITLE 303
#define DONT_SQUEEZE_TITLE 304
#define START_ICONIFIED 305
#define NO_TITLE_HILITE 306
#define TITLE_HILITE 307
#define MOVE 308
#define RESIZE 309
#define WAIT 310
#define SELECT 311
#define KILL 312
#define LEFT_TITLEBUTTON 313
#define RIGHT_TITLEBUTTON 314
#define NUMBER 315
#define KEYWORD 316
#define NKEYWORD 317
#define CKEYWORD 318
#define CLKEYWORD 319
#define FKEYWORD 320
#define FSKEYWORD 321
#define SKEYWORD 322
#define DKEYWORD 323
#define JKEYWORD 324
#define WINDOW_RING 325
#define WARP_CURSOR 326
#define ERRORTOKEN 327
#define NO_STACKMODE 328
#define STRING 329




/* Copy the first part of user declarations.  */
#line 67 "gram.y"

#include <stdio.h>
#include <ctype.h>
#include "twm.h"
#include "menus.h"
#include "list.h"
#include "util.h"
#include "screen.h"
#include "parse.h"
#include "add_window.h"
#include "icons.h"
#include <X11/Xos.h>
#include <X11/Xmu/CharSet.h>

static char *Action = "";
static char *Name = "";
static MenuRoot	*root, *pull = NULL;

static MenuRoot *GetRoot ( char *name, char *fore, char *back );
static void GotButton ( int butt, int func );
static void GotKey ( char *key, int func );
static void GotTitleButton ( char *bitmapname, int func, Bool rightside );
static Bool CheckWarpScreenArg ( char *s );
static Bool CheckWarpRingArg ( char *s );
static Bool CheckColormapArg ( char *s );

static char *ptr;
static name_list **list;
static int cont = 0;
static int color;
int mods = 0;
unsigned int mods_used = (ShiftMask | ControlMask | Mod1Mask);

extern int yylineno;


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
#line 104 "gram.y"
{
    int num;
    char *ptr;
}
/* Line 193 of yacc.c.  */
#line 285 "gram.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 298 "gram.c"

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
# if YYENABLE_NLS
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
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   339

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  75
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  76
/* YYNRULES -- Number of rules.  */
#define YYNRULES  193
/* YYNRULES -- Number of states.  */
#define YYNSTATES  285

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   329

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
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
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     6,     9,    11,    13,    15,    17,
      19,    26,    30,    33,    36,    38,    41,    44,    45,    49,
      51,    56,    61,    64,    67,    70,    73,    74,    78,    79,
      83,    85,    86,    90,    91,    95,    96,   100,   102,   103,
     107,   109,   110,   114,   116,   117,   121,   123,   124,   128,
     129,   133,   134,   138,   139,   149,   150,   155,   156,   161,
     162,   166,   167,   171,   172,   176,   179,   180,   184,   187,
     190,   191,   195,   197,   198,   202,   204,   207,   210,   217,
     224,   225,   228,   230,   232,   234,   236,   239,   241,   242,
     245,   247,   249,   251,   253,   255,   257,   259,   261,   263,
     264,   267,   269,   271,   273,   275,   277,   279,   281,   283,
     285,   287,   291,   292,   295,   298,   302,   303,   306,   310,
     313,   317,   320,   324,   327,   331,   334,   338,   341,   345,
     348,   352,   355,   359,   362,   366,   369,   373,   376,   380,
     383,   387,   388,   391,   394,   395,   400,   403,   407,   408,
     411,   413,   415,   419,   420,   423,   426,   428,   429,   435,
     437,   438,   442,   443,   449,   453,   454,   457,   461,   466,
     470,   471,   474,   476,   480,   481,   484,   487,   491,   492,
     495,   497,   501,   502,   505,   508,   516,   518,   521,   523,
     526,   529,   532,   534
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
      76,     0,    -1,    77,    -1,    -1,    77,    78,    -1,     1,
      -1,   100,    -1,   101,    -1,   102,    -1,   127,    -1,    36,
     149,    68,    68,   150,   150,    -1,    27,   149,   150,    -1,
      27,   149,    -1,    25,   150,    -1,    25,    -1,    16,   111,
      -1,    15,   114,    -1,    -1,    31,    79,   134,    -1,    31,
      -1,    58,   149,    47,   146,    -1,    59,   149,    47,   146,
      -1,   148,   149,    -1,   148,   146,    -1,   149,   104,    -1,
     148,   103,    -1,    -1,    32,    80,   134,    -1,    -1,    28,
      81,   134,    -1,    28,    -1,    -1,    26,    82,   131,    -1,
      -1,    22,    83,   134,    -1,    -1,    51,    84,   134,    -1,
      51,    -1,    -1,    35,    85,   134,    -1,    35,    -1,    -1,
      73,    86,   134,    -1,    73,    -1,    -1,    33,    87,   134,
      -1,    33,    -1,    -1,    29,    88,   134,    -1,    -1,    50,
      89,   134,    -1,    -1,    34,    90,   134,    -1,    -1,     8,
     149,     5,   149,    46,   149,     6,    91,   143,    -1,    -1,
       8,   149,    92,   143,    -1,    -1,    21,   149,    93,   140,
      -1,    -1,    17,    94,   137,    -1,    -1,    18,    95,   117,
      -1,    -1,    30,    96,   117,    -1,    19,   121,    -1,    -1,
      20,    97,   117,    -1,    10,   146,    -1,    24,   146,    -1,
      -1,    71,    98,   134,    -1,    71,    -1,    -1,    70,    99,
     134,    -1,    61,    -1,    67,   149,    -1,    62,   150,    -1,
      47,   105,    46,   107,    46,   146,    -1,    47,   105,    46,
     109,    46,   146,    -1,    -1,   105,   106,    -1,    37,    -1,
      38,    -1,    39,    -1,    40,    -1,    37,   150,    -1,    14,
      -1,    -1,   107,   108,    -1,    41,    -1,    42,    -1,    43,
      -1,    44,    -1,    45,    -1,    23,    -1,    37,    -1,    13,
      -1,    14,    -1,    -1,   109,   110,    -1,    41,    -1,    42,
      -1,    43,    -1,    44,    -1,    45,    -1,    23,    -1,    37,
      -1,    13,    -1,    14,    -1,   149,    -1,     3,   112,     4,
      -1,    -1,   112,   113,    -1,    52,   149,    -1,     3,   115,
       4,    -1,    -1,   115,   116,    -1,    45,   149,   149,    -1,
      45,   149,    -1,    42,   149,   149,    -1,    42,   149,    -1,
      43,   149,   149,    -1,    43,   149,    -1,    23,   149,   149,
      -1,    23,   149,    -1,     9,   149,   149,    -1,     9,   149,
      -1,    53,   149,   149,    -1,    53,   149,    -1,    54,   149,
     149,    -1,    54,   149,    -1,    55,   149,   149,    -1,    55,
     149,    -1,     8,   149,   149,    -1,     8,   149,    -1,    56,
     149,   149,    -1,    56,   149,    -1,    57,   149,   149,    -1,
      57,   149,    -1,     3,   118,     4,    -1,    -1,   118,   119,
      -1,    64,   149,    -1,    -1,    64,   149,   120,   124,    -1,
      63,   149,    -1,     3,   122,     4,    -1,    -1,   122,   123,
      -1,   149,    -1,    64,    -1,     3,   125,     4,    -1,    -1,
     125,   126,    -1,   149,   149,    -1,    48,    -1,    -1,    48,
     128,     3,   130,     4,    -1,    49,    -1,    -1,    49,   129,
     134,    -1,    -1,   130,   149,    69,   147,   150,    -1,     3,
     132,     4,    -1,    -1,   132,   133,    -1,   149,   149,   150,
      -1,   149,   149,   149,   150,    -1,     3,   135,     4,    -1,
      -1,   135,   136,    -1,   149,    -1,     3,   138,     4,    -1,
      -1,   138,   139,    -1,   149,   149,    -1,     3,   141,     4,
      -1,    -1,   141,   142,    -1,   146,    -1,     3,   144,     4,
      -1,    -1,   144,   145,    -1,   149,   146,    -1,   149,     5,
     149,    46,   149,     6,   146,    -1,    65,    -1,    66,   149,
      -1,   150,    -1,    11,   150,    -1,    12,   150,    -1,     9,
     150,    -1,    74,    -1,    60,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   133,   133,   136,   137,   140,   141,   142,   143,   144,
     145,   147,   153,   156,   162,   164,   165,   166,   166,   168,
     170,   173,   176,   180,   196,   197,   198,   198,   200,   200,
     202,   203,   203,   205,   205,   207,   207,   209,   211,   211,
     213,   215,   215,   217,   219,   219,   221,   223,   223,   225,
     225,   227,   227,   229,   229,   232,   232,   234,   234,   236,
     236,   238,   238,   240,   240,   242,   244,   244,   246,   262,
     270,   270,   272,   274,   274,   279,   289,   299,   311,   314,
     317,   318,   321,   322,   323,   324,   325,   335,   338,   339,
     342,   343,   344,   345,   346,   347,   348,   349,   350,   353,
     354,   357,   358,   359,   360,   361,   362,   363,   364,   365,
     366,   370,   373,   374,   377,   381,   384,   385,   388,   390,
     392,   394,   396,   398,   400,   402,   404,   406,   408,   410,
     412,   414,   416,   418,   420,   422,   424,   426,   428,   430,
     434,   438,   439,   442,   451,   451,   462,   473,   476,   477,
     480,   481,   484,   487,   488,   491,   496,   499,   499,   504,
     505,   505,   509,   510,   518,   521,   522,   525,   530,   538,
     541,   542,   545,   550,   553,   554,   557,   560,   563,   564,
     567,   573,   576,   577,   580,   585,   593,   594,   635,   636,
     637,   640,   652,   658
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "LB", "RB", "LP", "RP", "MENUS", "MENU",
  "BUTTON", "DEFAULT_FUNCTION", "PLUS", "MINUS", "ALL", "OR", "CURSORS",
  "PIXMAPS", "ICONS", "COLOR", "SAVECOLOR", "MONOCHROME", "FUNCTION",
  "ICONMGR_SHOW", "ICONMGR", "WINDOW_FUNCTION", "ZOOM", "ICONMGRS",
  "ICONMGR_GEOMETRY", "ICONMGR_NOSHOW", "MAKE_TITLE", "GRAYSCALE",
  "ICONIFY_BY_UNMAPPING", "DONT_ICONIFY_BY_UNMAPPING", "NO_TITLE",
  "AUTO_RAISE", "NO_HILITE", "ICON_REGION", "META", "SHIFT", "LOCK",
  "CONTROL", "WINDOW", "TITLE", "ICON", "ROOT", "FRAME", "COLON", "EQUALS",
  "SQUEEZE_TITLE", "DONT_SQUEEZE_TITLE", "START_ICONIFIED",
  "NO_TITLE_HILITE", "TITLE_HILITE", "MOVE", "RESIZE", "WAIT", "SELECT",
  "KILL", "LEFT_TITLEBUTTON", "RIGHT_TITLEBUTTON", "NUMBER", "KEYWORD",
  "NKEYWORD", "CKEYWORD", "CLKEYWORD", "FKEYWORD", "FSKEYWORD", "SKEYWORD",
  "DKEYWORD", "JKEYWORD", "WINDOW_RING", "WARP_CURSOR", "ERRORTOKEN",
  "NO_STACKMODE", "STRING", "$accept", "twmrc", "stmts", "stmt", "@1",
  "@2", "@3", "@4", "@5", "@6", "@7", "@8", "@9", "@10", "@11", "@12",
  "@13", "@14", "@15", "@16", "@17", "@18", "@19", "@20", "@21", "noarg",
  "sarg", "narg", "full", "fullkey", "keys", "key", "contexts", "context",
  "contextkeys", "contextkey", "pixmap_list", "pixmap_entries",
  "pixmap_entry", "cursor_list", "cursor_entries", "cursor_entry",
  "color_list", "color_entries", "color_entry", "@22", "save_color_list",
  "s_color_entries", "s_color_entry", "win_color_list",
  "win_color_entries", "win_color_entry", "squeeze", "@23", "@24",
  "win_sqz_entries", "iconm_list", "iconm_entries", "iconm_entry",
  "win_list", "win_entries", "win_entry", "icon_list", "icon_entries",
  "icon_entry", "function", "function_entries", "function_entry", "menu",
  "menu_entries", "menu_entry", "action", "signed_number", "button",
  "string", "number", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    75,    76,    77,    77,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    79,    78,    78,
      78,    78,    78,    78,    78,    78,    80,    78,    81,    78,
      78,    82,    78,    83,    78,    84,    78,    78,    85,    78,
      78,    86,    78,    78,    87,    78,    78,    88,    78,    89,
      78,    90,    78,    91,    78,    92,    78,    93,    78,    94,
      78,    95,    78,    96,    78,    78,    97,    78,    78,    78,
      98,    78,    78,    99,    78,   100,   101,   102,   103,   104,
     105,   105,   106,   106,   106,   106,   106,   106,   107,   107,
     108,   108,   108,   108,   108,   108,   108,   108,   108,   109,
     109,   110,   110,   110,   110,   110,   110,   110,   110,   110,
     110,   111,   112,   112,   113,   114,   115,   115,   116,   116,
     116,   116,   116,   116,   116,   116,   116,   116,   116,   116,
     116,   116,   116,   116,   116,   116,   116,   116,   116,   116,
     117,   118,   118,   119,   120,   119,   119,   121,   122,   122,
     123,   123,   124,   125,   125,   126,   127,   128,   127,   127,
     129,   127,   130,   130,   131,   132,   132,   133,   133,   134,
     135,   135,   136,   137,   138,   138,   139,   140,   141,   141,
     142,   143,   144,   144,   145,   145,   146,   146,   147,   147,
     147,   148,   149,   150
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     2,     1,     1,     1,     1,     1,
       6,     3,     2,     2,     1,     2,     2,     0,     3,     1,
       4,     4,     2,     2,     2,     2,     0,     3,     0,     3,
       1,     0,     3,     0,     3,     0,     3,     1,     0,     3,
       1,     0,     3,     1,     0,     3,     1,     0,     3,     0,
       3,     0,     3,     0,     9,     0,     4,     0,     4,     0,
       3,     0,     3,     0,     3,     2,     0,     3,     2,     2,
       0,     3,     1,     0,     3,     1,     2,     2,     6,     6,
       0,     2,     1,     1,     1,     1,     2,     1,     0,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     3,     0,     2,     2,     3,     0,     2,     3,     2,
       3,     2,     3,     2,     3,     2,     3,     2,     3,     2,
       3,     2,     3,     2,     3,     2,     3,     2,     3,     2,
       3,     0,     2,     2,     0,     4,     2,     3,     0,     2,
       1,     1,     3,     0,     2,     2,     1,     0,     5,     1,
       0,     3,     0,     5,     3,     0,     2,     3,     4,     3,
       0,     2,     1,     3,     0,     2,     2,     3,     0,     2,
       1,     3,     0,     2,     2,     7,     1,     2,     1,     2,
       2,     2,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     0,     1,     5,     0,     0,     0,     0,     0,
      59,    61,     0,    66,     0,    33,     0,    14,    31,     0,
      30,    47,    63,    19,    26,    46,    51,    40,     0,   156,
     159,    49,    37,     0,     0,    75,     0,     0,    73,    72,
      43,   192,     4,     6,     7,     8,     9,     0,     0,    55,
     193,   191,   186,     0,    68,   116,    16,   112,    15,     0,
       0,   148,    65,     0,    57,     0,    69,    13,     0,    12,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    77,    76,     0,     0,     0,
      80,    25,    23,    22,    80,    24,     0,     0,   187,     0,
       0,   174,    60,   141,    62,     0,    67,     0,   170,    34,
     165,    32,    11,    29,    48,    64,    18,    27,    45,    52,
      39,     0,   162,   161,    50,    36,     0,     0,    74,    71,
      42,     0,     0,     0,   182,    56,   115,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   117,   111,
       0,   113,     0,     0,   147,   151,   149,   150,   178,    58,
       0,     0,     0,     0,    20,    21,    87,    82,    83,    84,
      85,    88,    81,    99,     0,     0,   135,   127,   125,   121,
     123,   119,   129,   131,   133,   137,   139,   114,   173,   175,
       0,   140,     0,     0,   142,     0,   169,   171,   172,   164,
     166,     0,     0,   158,     0,    86,     0,     0,     0,   181,
     183,     0,   134,   126,   124,   120,   122,   118,   128,   130,
     132,   136,   138,   176,   146,   143,   177,   179,   180,     0,
      10,     0,    97,    98,    95,    96,    90,    91,    92,    93,
      94,     0,    89,   108,   109,   106,   107,   101,   102,   103,
     104,   105,     0,   100,   110,    53,     0,   184,     0,     0,
     167,     0,     0,     0,   188,    78,    79,     0,     0,   153,
     145,   168,   189,   190,   163,    54,     0,     0,     0,   152,
     154,     0,     0,   155,   185
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    42,    73,    74,    70,    68,    65,    82,
      77,    89,    75,    71,    81,    76,   267,    97,   107,    59,
      60,    72,    63,    88,    87,    43,    44,    45,    91,    95,
     131,   172,   206,   242,   207,   253,    58,   100,   151,    56,
      99,   148,   104,   153,   194,   258,    62,   105,   156,   270,
     277,   280,    46,    79,    80,   163,   111,   161,   200,   109,
     160,   197,   102,   152,   189,   159,   195,   227,   135,   175,
     210,    54,   263,    47,    48,    51
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -159
static const yytype_int16 yypact[] =
{
    -159,    15,   265,  -159,  -159,   -56,   -30,   -25,    28,    30,
    -159,  -159,    32,  -159,   -56,  -159,   -25,   -30,  -159,   -56,
      40,  -159,  -159,    41,  -159,    42,  -159,    44,   -56,    46,
      48,  -159,    49,   -56,   -56,  -159,   -30,   -56,  -159,    58,
      60,  -159,  -159,  -159,  -159,  -159,  -159,   -28,    20,    61,
    -159,  -159,  -159,   -56,  -159,  -159,  -159,  -159,  -159,    65,
      66,  -159,  -159,    66,  -159,    76,  -159,  -159,    78,   -30,
      76,    76,    66,    76,    76,    76,    76,    76,    -4,    83,
      76,    76,    76,    43,    45,  -159,  -159,    76,    76,    76,
    -159,  -159,  -159,  -159,  -159,  -159,   -56,    85,  -159,   107,
       8,  -159,  -159,  -159,  -159,    -2,  -159,    86,  -159,  -159,
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,    27,  -159,  -159,  -159,  -159,   -25,   -25,  -159,  -159,
    -159,    59,   153,    55,  -159,  -159,  -159,   -56,   -56,   -56,
     -56,   -56,   -56,   -56,   -56,   -56,   -56,   -56,  -159,  -159,
     -56,  -159,     0,     7,  -159,  -159,  -159,  -159,  -159,  -159,
       1,     2,   -30,     3,  -159,  -159,  -159,   -30,  -159,  -159,
    -159,  -159,  -159,  -159,   -56,     4,   -56,   -56,   -56,   -56,
     -56,   -56,   -56,   -56,   -56,   -56,   -56,  -159,  -159,  -159,
     -56,  -159,   -56,   -56,  -159,    17,  -159,  -159,  -159,  -159,
    -159,   -56,   -30,  -159,    33,  -159,    80,    13,    98,  -159,
    -159,    19,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,  -159,  -159,  -159,   103,  -159,  -159,  -159,   -35,
    -159,     5,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,   -25,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,   -25,  -159,  -159,  -159,   -56,  -159,   104,   -30,
    -159,   -30,   -30,   -30,  -159,  -159,  -159,    85,    62,  -159,
    -159,  -159,  -159,  -159,  -159,  -159,   -56,     6,   106,  -159,
    -159,   -56,   -25,  -159,  -159
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
      16,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,   -50,  -159,  -159,  -159,  -159,  -159,  -159,  -159,
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,   130,
    -159,  -159,  -159,  -159,  -159,  -159,  -159,  -159,  -158,  -159,
    -159,   -13,  -159,  -159,    -5,   -16
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -161
static const yytype_int16 yytable[] =
{
      49,    67,   154,    66,   188,   196,   199,   203,   209,    64,
     279,   191,   149,   106,    69,     3,   261,   262,    41,    90,
      85,   226,   115,    78,   256,    50,   243,   244,    83,    84,
      50,    55,    86,    57,    92,    61,   245,    52,    53,    41,
      52,    53,    93,   -28,   -17,   -44,    41,   -38,    98,  -157,
     246,  -160,   -35,   112,   247,   248,   249,   250,   251,   252,
     150,   -70,   155,   -41,   121,    50,    96,    94,   101,   103,
     192,   193,    41,   166,    41,    41,    41,    41,    41,   108,
      41,   110,    52,    53,    52,    53,   122,    41,   134,   158,
     126,   133,   127,   232,   233,   162,   167,   168,   169,   170,
     157,   174,   231,   234,   255,   171,  -144,   269,   276,   275,
     132,   136,   282,   164,   165,   137,   138,   235,     0,     0,
       0,   236,   237,   238,   239,   240,   241,     0,     0,     0,
     139,     0,   176,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,     0,     0,   187,   202,   190,     0,   140,
     141,   205,   142,     0,     0,   198,   201,     0,   204,     0,
     143,   144,   145,   146,   147,     0,     0,   166,     0,   208,
     211,   212,   213,   214,   215,   216,   217,   218,   219,   220,
     221,   222,   228,     0,     0,   223,   230,   224,   225,     0,
     167,   168,   169,   170,     0,     0,   229,     0,   257,   173,
     113,   114,   254,   116,   117,   118,   119,   120,     0,     0,
     123,   124,   125,   260,     0,   264,     0,   128,   129,   130,
       0,     0,     0,     0,   259,     0,     0,     0,   265,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   266,
       0,     0,     0,   271,     0,   272,   273,   274,     0,     0,
       0,   268,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    -2,     4,     0,     0,   284,
       0,   278,   281,     5,     6,     7,   283,     0,     0,     0,
       8,     9,    10,    11,    12,    13,    14,    15,     0,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,    30,    31,    32,     0,     0,     0,
       0,     0,     0,    33,    34,     0,    35,    36,     0,     0,
       0,     0,    37,     0,     0,    38,    39,     0,    40,    41
};

static const yytype_int16 yycheck[] =
{
       5,    17,     4,    16,     4,     4,     4,     4,     4,    14,
       4,     4,     4,    63,    19,     0,    11,    12,    74,    47,
      36,     4,    72,    28,     5,    60,    13,    14,    33,    34,
      60,     3,    37,     3,    47,     3,    23,    65,    66,    74,
      65,    66,    47,     3,     3,     3,    74,     3,    53,     3,
      37,     3,     3,    69,    41,    42,    43,    44,    45,    46,
      52,     3,    64,     3,    68,    60,     5,    47,     3,     3,
      63,    64,    74,    14,    74,    74,    74,    74,    74,     3,
      74,     3,    65,    66,    65,    66,     3,    74,     3,     3,
      47,    96,    47,    13,    14,    68,    37,    38,    39,    40,
     105,    46,    69,    23,     6,    46,     3,     3,    46,   267,
      94,     4,     6,   126,   127,     8,     9,    37,    -1,    -1,
      -1,    41,    42,    43,    44,    45,    46,    -1,    -1,    -1,
      23,    -1,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,    -1,    -1,   150,   162,   152,    -1,    42,
      43,   167,    45,    -1,    -1,   160,   161,    -1,   163,    -1,
      53,    54,    55,    56,    57,    -1,    -1,    14,    -1,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   195,    -1,    -1,   190,   202,   192,   193,    -1,
      37,    38,    39,    40,    -1,    -1,   201,    -1,   211,    46,
      70,    71,   207,    73,    74,    75,    76,    77,    -1,    -1,
      80,    81,    82,   229,    -1,   231,    -1,    87,    88,    89,
      -1,    -1,    -1,    -1,   229,    -1,    -1,    -1,   241,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   252,
      -1,    -1,    -1,   259,    -1,   261,   262,   263,    -1,    -1,
      -1,   256,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     0,     1,    -1,    -1,   282,
      -1,   276,   277,     8,     9,    10,   281,    -1,    -1,    -1,
      15,    16,    17,    18,    19,    20,    21,    22,    -1,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    48,    49,    50,    51,    -1,    -1,    -1,
      -1,    -1,    -1,    58,    59,    -1,    61,    62,    -1,    -1,
      -1,    -1,    67,    -1,    -1,    70,    71,    -1,    73,    74
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    76,    77,     0,     1,     8,     9,    10,    15,    16,
      17,    18,    19,    20,    21,    22,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    48,
      49,    50,    51,    58,    59,    61,    62,    67,    70,    71,
      73,    74,    78,   100,   101,   102,   127,   148,   149,   149,
      60,   150,    65,    66,   146,     3,   114,     3,   111,    94,
      95,     3,   121,    97,   149,    83,   146,   150,    82,   149,
      81,    88,    96,    79,    80,    87,    90,    85,   149,   128,
     129,    89,    84,   149,   149,   150,   149,    99,    98,    86,
      47,   103,   146,   149,    47,   104,     5,    92,   149,   115,
     112,     3,   137,     3,   117,   122,   117,    93,     3,   134,
       3,   131,   150,   134,   134,   117,   134,   134,   134,   134,
     134,    68,     3,   134,   134,   134,    47,    47,   134,   134,
     134,   105,   105,   149,     3,   143,     4,     8,     9,    23,
      42,    43,    45,    53,    54,    55,    56,    57,   116,     4,
      52,   113,   138,   118,     4,    64,   123,   149,     3,   140,
     135,   132,    68,   130,   146,   146,    14,    37,    38,    39,
      40,    46,   106,    46,    46,   144,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,     4,   139,
     149,     4,    63,    64,   119,   141,     4,   136,   149,     4,
     133,   149,   150,     4,   149,   150,   107,   109,   149,     4,
     145,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,     4,   142,   146,   149,
     150,    69,    13,    14,    23,    37,    41,    42,    43,    44,
      45,    46,   108,    13,    14,    23,    37,    41,    42,    43,
      44,    45,    46,   110,   149,     6,     5,   146,   120,   149,
     150,    11,    12,   147,   150,   146,   146,    91,   149,     3,
     124,   150,   150,   150,   150,   143,    46,   125,   149,     4,
     126,   149,     6,   149,   146
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
# if YYLTYPE_IS_TRIVIAL
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
        case 10:
#line 146 "gram.y"
    { AddIconRegion((yyvsp[(2) - (6)].ptr), (yyvsp[(3) - (6)].num), (yyvsp[(4) - (6)].num), (yyvsp[(5) - (6)].num), (yyvsp[(6) - (6)].num)); }
    break;

  case 11:
#line 147 "gram.y"
    { if (Scr->FirstTime)
						  {
						    Scr->iconmgr.geometry=(yyvsp[(2) - (3)].ptr);
						    Scr->iconmgr.columns=(yyvsp[(3) - (3)].num);
						  }
						}
    break;

  case 12:
#line 153 "gram.y"
    { if (Scr->FirstTime)
						    Scr->iconmgr.geometry = (yyvsp[(2) - (2)].ptr);
						}
    break;

  case 13:
#line 156 "gram.y"
    { if (Scr->FirstTime)
					  {
						Scr->DoZoom = TRUE;
						Scr->ZoomCount = (yyvsp[(2) - (2)].num);
					  }
					}
    break;

  case 14:
#line 162 "gram.y"
    { if (Scr->FirstTime) 
						Scr->DoZoom = TRUE; }
    break;

  case 15:
#line 164 "gram.y"
    {}
    break;

  case 16:
#line 165 "gram.y"
    {}
    break;

  case 17:
#line 166 "gram.y"
    { list = &Scr->IconifyByUn; }
    break;

  case 19:
#line 168 "gram.y"
    { if (Scr->FirstTime) 
		    Scr->IconifyByUnmapping = TRUE; }
    break;

  case 20:
#line 170 "gram.y"
    { 
					  GotTitleButton ((yyvsp[(2) - (4)].ptr), (yyvsp[(4) - (4)].num), False);
					}
    break;

  case 21:
#line 173 "gram.y"
    { 
					  GotTitleButton ((yyvsp[(2) - (4)].ptr), (yyvsp[(4) - (4)].num), True);
					}
    break;

  case 22:
#line 176 "gram.y"
    { root = GetRoot((yyvsp[(2) - (2)].ptr), NULLSTR, NULLSTR);
					  Scr->Mouse[(yyvsp[(1) - (2)].num)][C_ROOT][0].func = F_MENU;
					  Scr->Mouse[(yyvsp[(1) - (2)].num)][C_ROOT][0].menu = root;
					}
    break;

  case 23:
#line 180 "gram.y"
    { Scr->Mouse[(yyvsp[(1) - (2)].num)][C_ROOT][0].func = (yyvsp[(2) - (2)].num);
					  if ((yyvsp[(2) - (2)].num) == F_MENU)
					  {
					    pull->prev = NULL;
					    Scr->Mouse[(yyvsp[(1) - (2)].num)][C_ROOT][0].menu = pull;
					  }
					  else
					  {
					    root = GetRoot(TWM_ROOT,NULLSTR,NULLSTR);
					    Scr->Mouse[(yyvsp[(1) - (2)].num)][C_ROOT][0].item = 
						AddToMenu(root,"x",Action,
							  NULL,(yyvsp[(2) - (2)].num),NULLSTR,NULLSTR);
					  }
					  Action = "";
					  pull = NULL;
					}
    break;

  case 24:
#line 196 "gram.y"
    { GotKey((yyvsp[(1) - (2)].ptr), (yyvsp[(2) - (2)].num)); }
    break;

  case 25:
#line 197 "gram.y"
    { GotButton((yyvsp[(1) - (2)].num), (yyvsp[(2) - (2)].num)); }
    break;

  case 26:
#line 198 "gram.y"
    { list = &Scr->DontIconify; }
    break;

  case 28:
#line 200 "gram.y"
    { list = &Scr->IconMgrNoShow; }
    break;

  case 30:
#line 202 "gram.y"
    { Scr->IconManagerDontShow = TRUE; }
    break;

  case 31:
#line 203 "gram.y"
    { list = &Scr->IconMgrs; }
    break;

  case 33:
#line 205 "gram.y"
    { list = &Scr->IconMgrShow; }
    break;

  case 35:
#line 207 "gram.y"
    { list = &Scr->NoTitleHighlight; }
    break;

  case 37:
#line 209 "gram.y"
    { if (Scr->FirstTime)
						Scr->TitleHighlight = FALSE; }
    break;

  case 38:
#line 211 "gram.y"
    { list = &Scr->NoHighlight; }
    break;

  case 40:
#line 213 "gram.y"
    { if (Scr->FirstTime)
						Scr->Highlight = FALSE; }
    break;

  case 41:
#line 215 "gram.y"
    { list = &Scr->NoStackModeL; }
    break;

  case 43:
#line 217 "gram.y"
    { if (Scr->FirstTime)
						Scr->StackMode = FALSE; }
    break;

  case 44:
#line 219 "gram.y"
    { list = &Scr->NoTitle; }
    break;

  case 46:
#line 221 "gram.y"
    { if (Scr->FirstTime)
						Scr->NoTitlebar = TRUE; }
    break;

  case 47:
#line 223 "gram.y"
    { list = &Scr->MakeTitle; }
    break;

  case 49:
#line 225 "gram.y"
    { list = &Scr->StartIconified; }
    break;

  case 51:
#line 227 "gram.y"
    { list = &Scr->AutoRaise; }
    break;

  case 53:
#line 229 "gram.y"
    {
					root = GetRoot((yyvsp[(2) - (7)].ptr), (yyvsp[(4) - (7)].ptr), (yyvsp[(6) - (7)].ptr)); }
    break;

  case 54:
#line 231 "gram.y"
    { root->real_menu = TRUE;}
    break;

  case 55:
#line 232 "gram.y"
    { root = GetRoot((yyvsp[(2) - (2)].ptr), NULLSTR, NULLSTR); }
    break;

  case 56:
#line 233 "gram.y"
    { root->real_menu = TRUE; }
    break;

  case 57:
#line 234 "gram.y"
    { root = GetRoot((yyvsp[(2) - (2)].ptr), NULLSTR, NULLSTR); }
    break;

  case 59:
#line 236 "gram.y"
    { list = &Scr->IconNames; }
    break;

  case 61:
#line 238 "gram.y"
    { color = COLOR; }
    break;

  case 63:
#line 240 "gram.y"
    { color = GRAYSCALE; }
    break;

  case 66:
#line 244 "gram.y"
    { color = MONOCHROME; }
    break;

  case 68:
#line 246 "gram.y"
    { Scr->DefaultFunction.func = (yyvsp[(2) - (2)].num);
					  if ((yyvsp[(2) - (2)].num) == F_MENU)
					  {
					    pull->prev = NULL;
					    Scr->DefaultFunction.menu = pull;
					  }
					  else
					  {
					    root = GetRoot(TWM_ROOT,NULLSTR,NULLSTR);
					    Scr->DefaultFunction.item = 
						AddToMenu(root,"x",Action,
							  NULL,(yyvsp[(2) - (2)].num), NULLSTR, NULLSTR);
					  }
					  Action = "";
					  pull = NULL;
					}
    break;

  case 69:
#line 262 "gram.y"
    { Scr->WindowFunction.func = (yyvsp[(2) - (2)].num);
					   root = GetRoot(TWM_ROOT,NULLSTR,NULLSTR);
					   Scr->WindowFunction.item = 
						AddToMenu(root,"x",Action,
							  NULL,(yyvsp[(2) - (2)].num), NULLSTR, NULLSTR);
					   Action = "";
					   pull = NULL;
					}
    break;

  case 70:
#line 270 "gram.y"
    { list = &Scr->WarpCursorL; }
    break;

  case 72:
#line 272 "gram.y"
    { if (Scr->FirstTime) 
					    Scr->WarpCursor = TRUE; }
    break;

  case 73:
#line 274 "gram.y"
    { list = &Scr->WindowRingL; }
    break;

  case 75:
#line 279 "gram.y"
    { if (!do_single_keyword ((yyvsp[(1) - (1)].num))) {
					    twmrc_error_prefix();
					    fprintf (stderr,
					"unknown singleton keyword %d\n",
						     (yyvsp[(1) - (1)].num));
					    ParseError = 1;
					  }
					}
    break;

  case 76:
#line 289 "gram.y"
    { if (!do_string_keyword ((yyvsp[(1) - (2)].num), (yyvsp[(2) - (2)].ptr))) {
					    twmrc_error_prefix();
					    fprintf (stderr,
				"unknown string keyword %d (value \"%s\")\n",
						     (yyvsp[(1) - (2)].num), (yyvsp[(2) - (2)].ptr));
					    ParseError = 1;
					  }
					}
    break;

  case 77:
#line 299 "gram.y"
    { if (!do_number_keyword ((yyvsp[(1) - (2)].num), (yyvsp[(2) - (2)].num))) {
					    twmrc_error_prefix();
					    fprintf (stderr,
				"unknown numeric keyword %d (value %d)\n",
						     (yyvsp[(1) - (2)].num), (yyvsp[(2) - (2)].num));
					    ParseError = 1;
					  }
					}
    break;

  case 78:
#line 311 "gram.y"
    { (yyval.num) = (yyvsp[(6) - (6)].num); }
    break;

  case 79:
#line 314 "gram.y"
    { (yyval.num) = (yyvsp[(6) - (6)].num); }
    break;

  case 82:
#line 321 "gram.y"
    { mods |= Mod1Mask; }
    break;

  case 83:
#line 322 "gram.y"
    { mods |= ShiftMask; }
    break;

  case 84:
#line 323 "gram.y"
    { mods |= LockMask; }
    break;

  case 85:
#line 324 "gram.y"
    { mods |= ControlMask; }
    break;

  case 86:
#line 325 "gram.y"
    { if ((yyvsp[(2) - (2)].num) < 1 || (yyvsp[(2) - (2)].num) > 5) {
					     twmrc_error_prefix();
					     fprintf (stderr, 
				"bad modifier number (%d), must be 1-5\n",
						      (yyvsp[(2) - (2)].num));
					     ParseError = 1;
					  } else {
					     mods |= (Mod1Mask << ((yyvsp[(2) - (2)].num) - 1));
					  }
					}
    break;

  case 87:
#line 335 "gram.y"
    { }
    break;

  case 90:
#line 342 "gram.y"
    { cont |= C_WINDOW_BIT; }
    break;

  case 91:
#line 343 "gram.y"
    { cont |= C_TITLE_BIT; }
    break;

  case 92:
#line 344 "gram.y"
    { cont |= C_ICON_BIT; }
    break;

  case 93:
#line 345 "gram.y"
    { cont |= C_ROOT_BIT; }
    break;

  case 94:
#line 346 "gram.y"
    { cont |= C_FRAME_BIT; }
    break;

  case 95:
#line 347 "gram.y"
    { cont |= C_ICONMGR_BIT; }
    break;

  case 96:
#line 348 "gram.y"
    { cont |= C_ICONMGR_BIT; }
    break;

  case 97:
#line 349 "gram.y"
    { cont |= C_ALL_BITS; }
    break;

  case 98:
#line 350 "gram.y"
    {  }
    break;

  case 101:
#line 357 "gram.y"
    { cont |= C_WINDOW_BIT; }
    break;

  case 102:
#line 358 "gram.y"
    { cont |= C_TITLE_BIT; }
    break;

  case 103:
#line 359 "gram.y"
    { cont |= C_ICON_BIT; }
    break;

  case 104:
#line 360 "gram.y"
    { cont |= C_ROOT_BIT; }
    break;

  case 105:
#line 361 "gram.y"
    { cont |= C_FRAME_BIT; }
    break;

  case 106:
#line 362 "gram.y"
    { cont |= C_ICONMGR_BIT; }
    break;

  case 107:
#line 363 "gram.y"
    { cont |= C_ICONMGR_BIT; }
    break;

  case 108:
#line 364 "gram.y"
    { cont |= C_ALL_BITS; }
    break;

  case 109:
#line 365 "gram.y"
    { }
    break;

  case 110:
#line 366 "gram.y"
    { Name = (yyvsp[(1) - (1)].ptr); cont |= C_NAME_BIT; }
    break;

  case 114:
#line 377 "gram.y"
    { SetHighlightPixmap ((yyvsp[(2) - (2)].ptr)); }
    break;

  case 118:
#line 388 "gram.y"
    {
			NewBitmapCursor(&Scr->FrameCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 119:
#line 390 "gram.y"
    {
			NewFontCursor(&Scr->FrameCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 120:
#line 392 "gram.y"
    {
			NewBitmapCursor(&Scr->TitleCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 121:
#line 394 "gram.y"
    {
			NewFontCursor(&Scr->TitleCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 122:
#line 396 "gram.y"
    {
			NewBitmapCursor(&Scr->IconCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 123:
#line 398 "gram.y"
    {
			NewFontCursor(&Scr->IconCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 124:
#line 400 "gram.y"
    {
			NewBitmapCursor(&Scr->IconMgrCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 125:
#line 402 "gram.y"
    {
			NewFontCursor(&Scr->IconMgrCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 126:
#line 404 "gram.y"
    {
			NewBitmapCursor(&Scr->ButtonCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 127:
#line 406 "gram.y"
    {
			NewFontCursor(&Scr->ButtonCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 128:
#line 408 "gram.y"
    {
			NewBitmapCursor(&Scr->MoveCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 129:
#line 410 "gram.y"
    {
			NewFontCursor(&Scr->MoveCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 130:
#line 412 "gram.y"
    {
			NewBitmapCursor(&Scr->ResizeCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 131:
#line 414 "gram.y"
    {
			NewFontCursor(&Scr->ResizeCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 132:
#line 416 "gram.y"
    {
			NewBitmapCursor(&Scr->WaitCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 133:
#line 418 "gram.y"
    {
			NewFontCursor(&Scr->WaitCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 134:
#line 420 "gram.y"
    {
			NewBitmapCursor(&Scr->MenuCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 135:
#line 422 "gram.y"
    {
			NewFontCursor(&Scr->MenuCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 136:
#line 424 "gram.y"
    {
			NewBitmapCursor(&Scr->SelectCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 137:
#line 426 "gram.y"
    {
			NewFontCursor(&Scr->SelectCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 138:
#line 428 "gram.y"
    {
			NewBitmapCursor(&Scr->DestroyCursor, (yyvsp[(2) - (3)].ptr), (yyvsp[(3) - (3)].ptr)); }
    break;

  case 139:
#line 430 "gram.y"
    {
			NewFontCursor(&Scr->DestroyCursor, (yyvsp[(2) - (2)].ptr)); }
    break;

  case 143:
#line 442 "gram.y"
    { if (!do_colorlist_keyword ((yyvsp[(1) - (2)].num), color,
								     (yyvsp[(2) - (2)].ptr))) {
					    twmrc_error_prefix();
					    fprintf (stderr,
			"unhandled list color keyword %d (string \"%s\")\n",
						     (yyvsp[(1) - (2)].num), (yyvsp[(2) - (2)].ptr));
					    ParseError = 1;
					  }
					}
    break;

  case 144:
#line 451 "gram.y"
    { list = do_colorlist_keyword((yyvsp[(1) - (2)].num),color,
								      (yyvsp[(2) - (2)].ptr));
					  if (!list) {
					    twmrc_error_prefix();
					    fprintf (stderr,
			"unhandled color list keyword %d (string \"%s\")\n",
						     (yyvsp[(1) - (2)].num), (yyvsp[(2) - (2)].ptr));
					    ParseError = 1;
					  }
					}
    break;

  case 145:
#line 461 "gram.y"
    { /* No action */; }
    break;

  case 146:
#line 462 "gram.y"
    { if (!do_color_keyword ((yyvsp[(1) - (2)].num), color,
								 (yyvsp[(2) - (2)].ptr))) {
					    twmrc_error_prefix();
					    fprintf (stderr,
			"unhandled color keyword %d (string \"%s\")\n",
						     (yyvsp[(1) - (2)].num), (yyvsp[(2) - (2)].ptr));
					    ParseError = 1;
					  }
					}
    break;

  case 150:
#line 480 "gram.y"
    { do_string_savecolor(color, (yyvsp[(1) - (1)].ptr)); }
    break;

  case 151:
#line 481 "gram.y"
    { do_var_savecolor((yyvsp[(1) - (1)].num)); }
    break;

  case 155:
#line 491 "gram.y"
    { if (Scr->FirstTime &&
					      color == Scr->Monochrome)
					    AddToList(list, (yyvsp[(1) - (2)].ptr), (yyvsp[(2) - (2)].ptr)); }
    break;

  case 156:
#line 496 "gram.y"
    { 
				    if (HasShape) Scr->SqueezeTitle = TRUE;
				}
    break;

  case 157:
#line 499 "gram.y"
    { list = &Scr->SqueezeTitleL; 
				  if (HasShape && Scr->SqueezeTitle == -1)
				    Scr->SqueezeTitle = TRUE;
				}
    break;

  case 159:
#line 504 "gram.y"
    { Scr->SqueezeTitle = FALSE; }
    break;

  case 160:
#line 505 "gram.y"
    { list = &Scr->DontSqueezeTitleL; }
    break;

  case 163:
#line 510 "gram.y"
    {
				if (Scr->FirstTime) {
				   do_squeeze_entry (list, (yyvsp[(2) - (5)].ptr), (yyvsp[(3) - (5)].num), (yyvsp[(4) - (5)].num), (yyvsp[(5) - (5)].num));
				}
			}
    break;

  case 167:
#line 525 "gram.y"
    { if (Scr->FirstTime)
					    AddToList(list, (yyvsp[(1) - (3)].ptr), (char *)
						AllocateIconManager((yyvsp[(1) - (3)].ptr), NULLSTR,
							(yyvsp[(2) - (3)].ptr),(yyvsp[(3) - (3)].num)));
					}
    break;

  case 168:
#line 531 "gram.y"
    { if (Scr->FirstTime)
					    AddToList(list, (yyvsp[(1) - (4)].ptr), (char *)
						AllocateIconManager((yyvsp[(1) - (4)].ptr),(yyvsp[(2) - (4)].ptr),
						(yyvsp[(3) - (4)].ptr), (yyvsp[(4) - (4)].num)));
					}
    break;

  case 172:
#line 545 "gram.y"
    { if (Scr->FirstTime)
					    AddToList(list, (yyvsp[(1) - (1)].ptr), 0);
					}
    break;

  case 176:
#line 557 "gram.y"
    { if (Scr->FirstTime) AddToList(list, (yyvsp[(1) - (2)].ptr), (yyvsp[(2) - (2)].ptr)); }
    break;

  case 180:
#line 567 "gram.y"
    { AddToMenu(root, "", Action, NULL, (yyvsp[(1) - (1)].num),
						NULLSTR, NULLSTR);
					  Action = "";
					}
    break;

  case 184:
#line 580 "gram.y"
    { AddToMenu(root, (yyvsp[(1) - (2)].ptr), Action, pull, (yyvsp[(2) - (2)].num),
						NULLSTR, NULLSTR);
					  Action = "";
					  pull = NULL;
					}
    break;

  case 185:
#line 585 "gram.y"
    {
					  AddToMenu(root, (yyvsp[(1) - (7)].ptr), Action, pull, (yyvsp[(7) - (7)].num),
						(yyvsp[(3) - (7)].ptr), (yyvsp[(5) - (7)].ptr));
					  Action = "";
					  pull = NULL;
					}
    break;

  case 186:
#line 593 "gram.y"
    { (yyval.num) = (yyvsp[(1) - (1)].num); }
    break;

  case 187:
#line 594 "gram.y"
    {
				(yyval.num) = (yyvsp[(1) - (2)].num);
				Action = (yyvsp[(2) - (2)].ptr);
				switch ((yyvsp[(1) - (2)].num)) {
				  case F_MENU:
				    pull = GetRoot ((yyvsp[(2) - (2)].ptr), NULLSTR,NULLSTR);
				    pull->prev = root;
				    break;
				  case F_WARPRING:
				    if (!CheckWarpRingArg (Action)) {
					twmrc_error_prefix();
					fprintf (stderr,
			"ignoring invalid f.warptoring argument \"%s\"\n",
						 Action);
					(yyval.num) = F_NOP;
				    }
				  case F_WARPTOSCREEN:
				    if (!CheckWarpScreenArg (Action)) {
					twmrc_error_prefix();
					fprintf (stderr, 
			"ignoring invalid f.warptoscreen argument \"%s\"\n", 
					         Action);
					(yyval.num) = F_NOP;
				    }
				    break;
				  case F_COLORMAP:
				    if (CheckColormapArg (Action)) {
					(yyval.num) = F_COLORMAP;
				    } else {
					twmrc_error_prefix();
					fprintf (stderr,
			"ignoring invalid f.colormap argument \"%s\"\n", 
						 Action);
					(yyval.num) = F_NOP;
				    }
				    break;
				} /* end switch */
				   }
    break;

  case 188:
#line 635 "gram.y"
    { (yyval.num) = (yyvsp[(1) - (1)].num); }
    break;

  case 189:
#line 636 "gram.y"
    { (yyval.num) = (yyvsp[(2) - (2)].num); }
    break;

  case 190:
#line 637 "gram.y"
    { (yyval.num) = -((yyvsp[(2) - (2)].num)); }
    break;

  case 191:
#line 640 "gram.y"
    { (yyval.num) = (yyvsp[(2) - (2)].num);
					  if ((yyvsp[(2) - (2)].num) == 0)
						yyerror("bad button 0");

					  if ((yyvsp[(2) - (2)].num) > MAX_BUTTONS)
					  {
						(yyval.num) = 0;
						yyerror("button number too large");
					  }
					}
    break;

  case 192:
#line 652 "gram.y"
    { ptr = (char *)malloc(strlen((yyvsp[(1) - (1)].ptr))+1);
					  strcpy(ptr, (yyvsp[(1) - (1)].ptr));
					  RemoveDQuote(ptr);
					  (yyval.ptr) = ptr;
					}
    break;

  case 193:
#line 658 "gram.y"
    { (yyval.num) = (yyvsp[(1) - (1)].num); }
    break;


/* Line 1267 of yacc.c.  */
#line 2658 "gram.c"
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


#line 661 "gram.y"

void
yyerror(char *s)
{
    twmrc_error_prefix();
    fprintf (stderr, "error in input file:  %s\n", s ? s : "");
    ParseError = 1;
}

void
RemoveDQuote(char *str)
{
    register char *i, *o;
    register int n;
    register int count;

    for (i=str+1, o=str; *i && *i != '\"'; o++)
    {
	if (*i == '\\')
	{
	    switch (*++i)
	    {
	    case 'n':
		*o = '\n';
		i++;
		break;
	    case 'b':
		*o = '\b';
		i++;
		break;
	    case 'r':
		*o = '\r';
		i++;
		break;
	    case 't':
		*o = '\t';
		i++;
		break;
	    case 'f':
		*o = '\f';
		i++;
		break;
	    case '0':
		if (*++i == 'x')
		    goto hex;
		else
		    --i;
	    case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
		n = 0;
		count = 0;
		while (*i >= '0' && *i <= '7' && count < 3)
		{
		    n = (n<<3) + (*i++ - '0');
		    count++;
		}
		*o = n;
		break;
	    hex:
	    case 'x':
		n = 0;
		count = 0;
		while (i++, count++ < 2)
		{
		    if (*i >= '0' && *i <= '9')
			n = (n<<4) + (*i - '0');
		    else if (*i >= 'a' && *i <= 'f')
			n = (n<<4) + (*i - 'a') + 10;
		    else if (*i >= 'A' && *i <= 'F')
			n = (n<<4) + (*i - 'A') + 10;
		    else
			break;
		}
		*o = n;
		break;
	    case '\n':
		i++;	/* punt */
		o--;	/* to account for o++ at end of loop */
		break;
	    case '\"':
	    case '\'':
	    case '\\':
	    default:
		*o = *i++;
		break;
	    }
	}
	else
	    *o = *i++;
    }
    *o = '\0';
}

static MenuRoot *GetRoot(char *name, char* fore, char *back)
{
    MenuRoot *tmp;

    tmp = FindMenuRoot(name);
    if (tmp == NULL)
	tmp = NewMenuRoot(name);

    if (fore)
    {
	int save;

	save = Scr->FirstTime;
	Scr->FirstTime = TRUE;
	GetColor(COLOR, &tmp->hi_fore, fore);
	GetColor(COLOR, &tmp->hi_back, back);
	Scr->FirstTime = save;
    }

    return tmp;
}

static void GotButton(int butt, int func)
{
    int i;

    for (i = 0; i < NUM_CONTEXTS; i++)
    {
	if ((cont & (1 << i)) == 0)
	    continue;

	Scr->Mouse[butt][i][mods].func = func;
	if (func == F_MENU)
	{
	    pull->prev = NULL;
	    Scr->Mouse[butt][i][mods].menu = pull;
	}
	else
	{
	    root = GetRoot(TWM_ROOT, NULLSTR, NULLSTR);
	    Scr->Mouse[butt][i][mods].item = AddToMenu(root,"x",Action,
		    NULL, func, NULLSTR, NULLSTR);
	}
    }
    Action = "";
    pull = NULL;
    cont = 0;
    mods_used |= mods;
    mods = 0;
}

static void GotKey(char *key, int func)
{
    int i;

    for (i = 0; i < NUM_CONTEXTS; i++)
    {
	if ((cont & (1 << i)) == 0) 
	  continue;
	if (!AddFuncKey(key, i, mods, func, Name, Action)) 
	  break;
    }

    Action = "";
    pull = NULL;
    cont = 0;
    mods_used |= mods;
    mods = 0;
}


static void GotTitleButton (char *bitmapname, int func, Bool rightside)
{
    if (!CreateTitleButton (bitmapname, func, Action, pull, rightside, True)) {
	twmrc_error_prefix();
	fprintf (stderr, 
		 "unable to create %s titlebutton \"%s\"\n",
		 rightside ? "right" : "left", bitmapname);
    }
    Action = "";
    pull = NULL;
}

static Bool CheckWarpScreenArg (char *s)
{
    XmuCopyISOLatin1Lowered (s, s);

    if (strcmp (s,  WARPSCREEN_NEXT) == 0 ||
	strcmp (s,  WARPSCREEN_PREV) == 0 ||
	strcmp (s,  WARPSCREEN_BACK) == 0)
      return True;

    for (; *s && isascii(*s) && isdigit(*s); s++) ; /* SUPPRESS 530 */
    return (*s ? False : True);
}


static Bool CheckWarpRingArg (char *s)
{
    XmuCopyISOLatin1Lowered (s, s);

    if (strcmp (s,  WARPSCREEN_NEXT) == 0 ||
	strcmp (s,  WARPSCREEN_PREV) == 0)
      return True;

    return False;
}


static Bool CheckColormapArg (char *s)
{
    XmuCopyISOLatin1Lowered (s, s);

    if (strcmp (s, COLORMAP_NEXT) == 0 ||
	strcmp (s, COLORMAP_PREV) == 0 ||
	strcmp (s, COLORMAP_DEFAULT) == 0)
      return True;

    return False;
}


void
twmrc_error_prefix ()
{
    fprintf (stderr, "%s:  line %d:  ", ProgramName, yylineno);
}

