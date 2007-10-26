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
#define YYPURE 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse zendparse
#define yylex   zendlex
#define yyerror zenderror
#define yylval  zendlval
#define yychar  zendchar
#define yydebug zenddebug
#define yynerrs zendnerrs


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     T_REQUIRE_ONCE = 258,
     T_REQUIRE = 259,
     T_EVAL = 260,
     T_INCLUDE_ONCE = 261,
     T_INCLUDE = 262,
     T_LOGICAL_OR = 263,
     T_LOGICAL_XOR = 264,
     T_LOGICAL_AND = 265,
     T_PRINT = 266,
     T_SR_EQUAL = 267,
     T_SL_EQUAL = 268,
     T_XOR_EQUAL = 269,
     T_OR_EQUAL = 270,
     T_AND_EQUAL = 271,
     T_MOD_EQUAL = 272,
     T_CONCAT_EQUAL = 273,
     T_DIV_EQUAL = 274,
     T_MUL_EQUAL = 275,
     T_MINUS_EQUAL = 276,
     T_PLUS_EQUAL = 277,
     T_BOOLEAN_OR = 278,
     T_BOOLEAN_AND = 279,
     T_IS_NOT_IDENTICAL = 280,
     T_IS_IDENTICAL = 281,
     T_IS_NOT_EQUAL = 282,
     T_IS_EQUAL = 283,
     T_IS_GREATER_OR_EQUAL = 284,
     T_IS_SMALLER_OR_EQUAL = 285,
     T_SR = 286,
     T_SL = 287,
     T_UNSET_CAST = 288,
     T_BOOL_CAST = 289,
     T_OBJECT_CAST = 290,
     T_ARRAY_CAST = 291,
     T_STRING_CAST = 292,
     T_DOUBLE_CAST = 293,
     T_INT_CAST = 294,
     T_DEC = 295,
     T_INC = 296,
     T_NEW = 297,
     T_EXIT = 298,
     T_IF = 299,
     T_ELSEIF = 300,
     T_ELSE = 301,
     T_ENDIF = 302,
     T_LNUMBER = 303,
     T_DNUMBER = 304,
     T_STRING = 305,
     T_STRING_VARNAME = 306,
     T_VARIABLE = 307,
     T_NUM_STRING = 308,
     T_INLINE_HTML = 309,
     T_CHARACTER = 310,
     T_BAD_CHARACTER = 311,
     T_ENCAPSED_AND_WHITESPACE = 312,
     T_CONSTANT_ENCAPSED_STRING = 313,
     T_ECHO = 314,
     T_DO = 315,
     T_WHILE = 316,
     T_ENDWHILE = 317,
     T_FOR = 318,
     T_ENDFOR = 319,
     T_FOREACH = 320,
     T_ENDFOREACH = 321,
     T_DECLARE = 322,
     T_ENDDECLARE = 323,
     T_AS = 324,
     T_SWITCH = 325,
     T_ENDSWITCH = 326,
     T_CASE = 327,
     T_DEFAULT = 328,
     T_BREAK = 329,
     T_CONTINUE = 330,
     T_OLD_FUNCTION = 331,
     T_FUNCTION = 332,
     T_CONST = 333,
     T_RETURN = 334,
     T_USE = 335,
     T_GLOBAL = 336,
     T_STATIC = 337,
     T_VAR = 338,
     T_UNSET = 339,
     T_ISSET = 340,
     T_EMPTY = 341,
     T_CLASS = 342,
     T_EXTENDS = 343,
     T_OBJECT_OPERATOR = 344,
     T_DOUBLE_ARROW = 345,
     T_LIST = 346,
     T_ARRAY = 347,
     T_CLASS_C = 348,
     T_FUNC_C = 349,
     T_LINE = 350,
     T_FILE = 351,
     T_COMMENT = 352,
     T_ML_COMMENT = 353,
     T_OPEN_TAG = 354,
     T_OPEN_TAG_WITH_ECHO = 355,
     T_CLOSE_TAG = 356,
     T_WHITESPACE = 357,
     T_START_HEREDOC = 358,
     T_END_HEREDOC = 359,
     T_DOLLAR_OPEN_CURLY_BRACES = 360,
     T_CURLY_OPEN = 361,
     T_PAAMAYIM_NEKUDOTAYIM = 362
   };
#endif
/* Tokens.  */
#define T_REQUIRE_ONCE 258
#define T_REQUIRE 259
#define T_EVAL 260
#define T_INCLUDE_ONCE 261
#define T_INCLUDE 262
#define T_LOGICAL_OR 263
#define T_LOGICAL_XOR 264
#define T_LOGICAL_AND 265
#define T_PRINT 266
#define T_SR_EQUAL 267
#define T_SL_EQUAL 268
#define T_XOR_EQUAL 269
#define T_OR_EQUAL 270
#define T_AND_EQUAL 271
#define T_MOD_EQUAL 272
#define T_CONCAT_EQUAL 273
#define T_DIV_EQUAL 274
#define T_MUL_EQUAL 275
#define T_MINUS_EQUAL 276
#define T_PLUS_EQUAL 277
#define T_BOOLEAN_OR 278
#define T_BOOLEAN_AND 279
#define T_IS_NOT_IDENTICAL 280
#define T_IS_IDENTICAL 281
#define T_IS_NOT_EQUAL 282
#define T_IS_EQUAL 283
#define T_IS_GREATER_OR_EQUAL 284
#define T_IS_SMALLER_OR_EQUAL 285
#define T_SR 286
#define T_SL 287
#define T_UNSET_CAST 288
#define T_BOOL_CAST 289
#define T_OBJECT_CAST 290
#define T_ARRAY_CAST 291
#define T_STRING_CAST 292
#define T_DOUBLE_CAST 293
#define T_INT_CAST 294
#define T_DEC 295
#define T_INC 296
#define T_NEW 297
#define T_EXIT 298
#define T_IF 299
#define T_ELSEIF 300
#define T_ELSE 301
#define T_ENDIF 302
#define T_LNUMBER 303
#define T_DNUMBER 304
#define T_STRING 305
#define T_STRING_VARNAME 306
#define T_VARIABLE 307
#define T_NUM_STRING 308
#define T_INLINE_HTML 309
#define T_CHARACTER 310
#define T_BAD_CHARACTER 311
#define T_ENCAPSED_AND_WHITESPACE 312
#define T_CONSTANT_ENCAPSED_STRING 313
#define T_ECHO 314
#define T_DO 315
#define T_WHILE 316
#define T_ENDWHILE 317
#define T_FOR 318
#define T_ENDFOR 319
#define T_FOREACH 320
#define T_ENDFOREACH 321
#define T_DECLARE 322
#define T_ENDDECLARE 323
#define T_AS 324
#define T_SWITCH 325
#define T_ENDSWITCH 326
#define T_CASE 327
#define T_DEFAULT 328
#define T_BREAK 329
#define T_CONTINUE 330
#define T_OLD_FUNCTION 331
#define T_FUNCTION 332
#define T_CONST 333
#define T_RETURN 334
#define T_USE 335
#define T_GLOBAL 336
#define T_STATIC 337
#define T_VAR 338
#define T_UNSET 339
#define T_ISSET 340
#define T_EMPTY 341
#define T_CLASS 342
#define T_EXTENDS 343
#define T_OBJECT_OPERATOR 344
#define T_DOUBLE_ARROW 345
#define T_LIST 346
#define T_ARRAY 347
#define T_CLASS_C 348
#define T_FUNC_C 349
#define T_LINE 350
#define T_FILE 351
#define T_COMMENT 352
#define T_ML_COMMENT 353
#define T_OPEN_TAG 354
#define T_OPEN_TAG_WITH_ECHO 355
#define T_CLOSE_TAG 356
#define T_WHITESPACE 357
#define T_START_HEREDOC 358
#define T_END_HEREDOC 359
#define T_DOLLAR_OPEN_CURLY_BRACES 360
#define T_CURLY_OPEN 361
#define T_PAAMAYIM_NEKUDOTAYIM 362




/* Copy the first part of user declarations.  */


/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2002 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        | 
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@zend.com>                                |
   |          Zeev Suraski <zeev@zend.com>                                |
   +----------------------------------------------------------------------+
*/

/* 
 * LALR shift/reduce conflicts and how they are resolved:
 *
 * - 2 shift/reduce conflicts due to the dangeling elseif/else ambiguity.  Solved by shift.
 * - 1 shift/reduce conflict due to arrays within encapsulated strings. Solved by shift. 
 * - 1 shift/reduce conflict due to objects within encapsulated strings.  Solved by shift.
 * 
 */


#include "zend_compile.h"
#include "zend.h"
#include "zend_list.h"
#include "zend_globals.h"
#include "zend_API.h"

#define YYERROR_VERBOSE
#define YYSTYPE znode
#ifdef ZTS
# define YYPARSE_PARAM tsrm_ls
# define YYLEX_PARAM tsrm_ls
#endif




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
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */


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
#define YYLAST   3479

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  137
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  127
/* YYNRULES -- Number of rules.  */
#define YYNRULES  347
/* YYNRULES -- Number of states.  */
#define YYNSTATES  664

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   362

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    48,   134,     2,   132,    47,    31,   135,
     129,   130,    45,    42,     8,    43,    44,    46,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    26,   131,
      36,    13,    37,    25,    50,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    60,     2,   136,    30,     2,   133,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   127,    29,   128,    49,     2,     2,     2,
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
       5,     6,     7,     9,    10,    11,    12,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    27,    28,
      32,    33,    34,    35,    38,    39,    40,    41,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     6,    10,    11,    13,    15,    16,
      20,    21,    23,    25,    27,    31,    32,    33,    43,    44,
      45,    58,    59,    60,    68,    69,    70,    80,    81,    82,
      83,    96,    97,   104,   107,   111,   114,   118,   121,   125,
     129,   133,   137,   141,   143,   146,   150,   156,   157,   158,
     169,   170,   171,   182,   183,   190,   192,   194,   198,   200,
     202,   206,   208,   209,   210,   222,   223,   224,   235,   236,
     243,   244,   253,   254,   257,   259,   264,   266,   271,   273,
     278,   282,   288,   292,   297,   302,   308,   309,   310,   317,
     318,   324,   326,   328,   330,   335,   336,   337,   345,   346,
     347,   356,   357,   360,   361,   365,   367,   368,   370,   373,
     376,   380,   384,   389,   394,   400,   402,   403,   405,   407,
     410,   414,   418,   423,   427,   429,   431,   434,   439,   443,
     449,   451,   455,   458,   459,   463,   464,   465,   477,   478,
     479,   490,   491,   493,   497,   503,   505,   509,   510,   514,
     516,   517,   519,   520,   525,   527,   528,   536,   540,   545,
     550,   551,   559,   560,   565,   569,   573,   577,   581,   585,
     589,   593,   597,   601,   605,   609,   612,   615,   618,   621,
     622,   627,   628,   633,   634,   639,   640,   645,   649,   653,
     657,   661,   665,   669,   673,   677,   681,   685,   689,   693,
     696,   699,   702,   705,   709,   713,   717,   721,   725,   729,
     733,   737,   741,   742,   743,   751,   753,   755,   758,   761,
     764,   767,   770,   773,   776,   779,   780,   784,   786,   791,
     795,   798,   799,   805,   806,   812,   813,   821,   823,   825,
     826,   829,   833,   834,   838,   840,   842,   844,   846,   848,
     850,   852,   854,   856,   859,   862,   867,   869,   871,   873,
     877,   881,   885,   886,   889,   890,   892,   898,   902,   906,
     908,   910,   912,   914,   916,   918,   920,   921,   926,   928,
     931,   936,   941,   943,   945,   950,   951,   953,   955,   956,
     961,   963,   964,   967,   972,   977,   979,   981,   985,   987,
     990,   994,   996,   998,   999,  1005,  1006,  1007,  1010,  1016,
    1020,  1024,  1026,  1033,  1038,  1043,  1046,  1049,  1052,  1055,
    1058,  1061,  1064,  1067,  1070,  1073,  1076,  1079,  1080,  1082,
    1083,  1089,  1093,  1097,  1104,  1108,  1110,  1112,  1114,  1119,
    1124,  1127,  1130,  1135,  1138,  1141,  1143,  1144
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     138,     0,    -1,   139,    -1,    -1,   139,   140,   141,    -1,
      -1,   145,    -1,   167,    -1,    -1,   142,   143,   144,    -1,
      -1,   145,    -1,   167,    -1,   146,    -1,   127,   142,   128,
      -1,    -1,    -1,    63,   129,   235,   130,   147,   145,   148,
     186,   190,    -1,    -1,    -1,    63,   129,   235,   130,    26,
     149,   142,   150,   188,   191,    66,   131,    -1,    -1,    -1,
      80,   129,   151,   235,   130,   152,   185,    -1,    -1,    -1,
      79,   153,   145,    80,   129,   154,   235,   130,   131,    -1,
      -1,    -1,    -1,    82,   129,   208,   131,   155,   208,   131,
     156,   208,   130,   157,   176,    -1,    -1,    89,   129,   235,
     130,   158,   180,    -1,    93,   131,    -1,    93,   235,   131,
      -1,    94,   131,    -1,    94,   235,   131,    -1,    98,   131,
      -1,    98,   211,   131,    -1,    98,   239,   131,    -1,   100,
     196,   131,    -1,   101,   198,   131,    -1,    78,   207,   131,
      -1,    73,    -1,   235,   131,    -1,    99,   166,   131,    -1,
     103,   129,   164,   130,   131,    -1,    -1,    -1,    84,   129,
     237,    88,   159,   237,   175,   130,   160,   177,    -1,    -1,
      -1,    84,   129,   211,    88,   161,   237,   175,   130,   162,
     177,    -1,    -1,    86,   163,   129,   179,   130,   178,    -1,
     131,    -1,   165,    -1,   164,     8,   165,    -1,   239,    -1,
      77,    -1,   129,    77,   130,    -1,   168,    -1,    -1,    -1,
      96,   169,   205,    69,   170,   129,   192,   130,   127,   142,
     128,    -1,    -1,    -1,    95,   171,   205,    69,   172,   192,
     129,   142,   130,   131,    -1,    -1,   106,    69,   173,   127,
     199,   128,    -1,    -1,   106,    69,   107,    69,   174,   127,
     199,   128,    -1,    -1,   109,   237,    -1,   145,    -1,    26,
     142,    83,   131,    -1,   145,    -1,    26,   142,    85,   131,
      -1,   145,    -1,    26,   142,    87,   131,    -1,    69,    13,
     230,    -1,   179,     8,    69,    13,   230,    -1,   127,   181,
     128,    -1,   127,   131,   181,   128,    -1,    26,   181,    90,
     131,    -1,    26,   131,   181,    90,   131,    -1,    -1,    -1,
     181,    91,   235,   184,   182,   142,    -1,    -1,   181,    92,
     184,   183,   142,    -1,    26,    -1,   131,    -1,   145,    -1,
      26,   142,    81,   131,    -1,    -1,    -1,   186,    64,   129,
     235,   130,   187,   145,    -1,    -1,    -1,   188,    64,   129,
     235,   130,    26,   189,   142,    -1,    -1,    65,   145,    -1,
      -1,    65,    26,   142,    -1,   193,    -1,    -1,    71,    -1,
      31,    71,    -1,    97,    71,    -1,    71,    13,   230,    -1,
     193,     8,    71,    -1,   193,     8,    31,    71,    -1,   193,
       8,    97,    71,    -1,   193,     8,    71,    13,   230,    -1,
     195,    -1,    -1,   211,    -1,   239,    -1,    31,   237,    -1,
     195,     8,   211,    -1,   195,     8,   239,    -1,   195,     8,
      31,   237,    -1,   196,     8,   197,    -1,   197,    -1,    71,
      -1,   132,   236,    -1,   132,   127,   235,   128,    -1,   198,
       8,    71,    -1,   198,     8,    71,    13,   230,    -1,    71,
      -1,    71,    13,   230,    -1,   199,   200,    -1,    -1,   102,
     206,   131,    -1,    -1,    -1,    96,   201,   205,    69,   202,
     129,   192,   130,   127,   142,   128,    -1,    -1,    -1,    95,
     203,   205,    69,   204,   192,   129,   142,   130,   131,    -1,
      -1,    31,    -1,   206,     8,    71,    -1,   206,     8,    71,
      13,   230,    -1,    71,    -1,    71,    13,   230,    -1,    -1,
     207,     8,   235,    -1,   235,    -1,    -1,   209,    -1,    -1,
     209,     8,   210,   235,    -1,   235,    -1,    -1,   110,   129,
     212,   252,   130,    13,   235,    -1,   239,    13,   235,    -1,
     239,    13,    31,   237,    -1,   239,    13,    31,   222,    -1,
      -1,   239,    13,    31,    61,   226,   213,   228,    -1,    -1,
      61,   226,   214,   228,    -1,   239,    24,   235,    -1,   239,
      23,   235,    -1,   239,    22,   235,    -1,   239,    21,   235,
      -1,   239,    20,   235,    -1,   239,    19,   235,    -1,   239,
      18,   235,    -1,   239,    17,   235,    -1,   239,    16,   235,
      -1,   239,    15,   235,    -1,   239,    14,   235,    -1,   238,
      59,    -1,    59,   238,    -1,   238,    58,    -1,    58,   238,
      -1,    -1,   235,    27,   215,   235,    -1,    -1,   235,    28,
     216,   235,    -1,    -1,   235,     9,   217,   235,    -1,    -1,
     235,    11,   218,   235,    -1,   235,    10,   235,    -1,   235,
      29,   235,    -1,   235,    31,   235,    -1,   235,    30,   235,
      -1,   235,    44,   235,    -1,   235,    42,   235,    -1,   235,
      43,   235,    -1,   235,    45,   235,    -1,   235,    46,   235,
      -1,   235,    47,   235,    -1,   235,    41,   235,    -1,   235,
      40,   235,    -1,    42,   235,    -1,    43,   235,    -1,    48,
     235,    -1,    49,   235,    -1,   235,    33,   235,    -1,   235,
      32,   235,    -1,   235,    35,   235,    -1,   235,    34,   235,
      -1,   235,    36,   235,    -1,   235,    39,   235,    -1,   235,
      37,   235,    -1,   235,    38,   235,    -1,   129,   235,   130,
      -1,    -1,    -1,   235,    25,   219,   235,    26,   220,   235,
      -1,   222,    -1,   261,    -1,    57,   235,    -1,    56,   235,
      -1,    55,   235,    -1,    54,   235,    -1,    53,   235,    -1,
      52,   235,    -1,    51,   235,    -1,    62,   227,    -1,    -1,
      50,   221,   235,    -1,   231,    -1,   111,   129,   255,   130,
      -1,   133,   257,   133,    -1,    12,   235,    -1,    -1,    69,
     129,   223,   194,   130,    -1,    -1,   239,   129,   224,   194,
     130,    -1,    -1,    69,   126,   226,   129,   225,   194,   130,
      -1,    69,    -1,   236,    -1,    -1,   129,   130,    -1,   129,
     235,   130,    -1,    -1,   129,   194,   130,    -1,    67,    -1,
      68,    -1,    77,    -1,   114,    -1,   115,    -1,   112,    -1,
     113,    -1,   229,    -1,    69,    -1,    42,   230,    -1,    43,
     230,    -1,   111,   129,   232,   130,    -1,    69,    -1,    70,
      -1,   229,    -1,   134,   257,   134,    -1,   135,   257,   135,
      -1,   122,   257,   123,    -1,    -1,   234,   233,    -1,    -1,
       8,    -1,   234,     8,   230,   109,   230,    -1,   234,     8,
     230,    -1,   230,   109,   230,    -1,   230,    -1,   236,    -1,
     211,    -1,   239,    -1,   239,    -1,   239,    -1,   241,    -1,
      -1,   241,   108,   240,   245,    -1,   242,    -1,   251,   242,
      -1,   242,    60,   244,   136,    -1,   242,   127,   235,   128,
      -1,   243,    -1,    71,    -1,   132,   127,   235,   128,    -1,
      -1,   235,    -1,   247,    -1,    -1,   245,   108,   246,   247,
      -1,   249,    -1,    -1,   241,   248,    -1,   249,    60,   244,
     136,    -1,   249,   127,   235,   128,    -1,   250,    -1,    69,
      -1,   127,   235,   128,    -1,   132,    -1,   251,   132,    -1,
     252,     8,   253,    -1,   253,    -1,   239,    -1,    -1,   110,
     129,   254,   252,   130,    -1,    -1,    -1,   256,   233,    -1,
     256,     8,   235,   109,   235,    -1,   256,     8,   235,    -1,
     235,   109,   235,    -1,   235,    -1,   256,     8,   235,   109,
      31,   237,    -1,   256,     8,    31,   237,    -1,   235,   109,
      31,   237,    -1,    31,   237,    -1,   257,   258,    -1,   257,
      69,    -1,   257,    72,    -1,   257,    76,    -1,   257,    74,
      -1,   257,    75,    -1,   257,    60,    -1,   257,   136,    -1,
     257,   127,    -1,   257,   128,    -1,   257,   108,    -1,    -1,
      71,    -1,    -1,    71,    60,   259,   260,   136,    -1,    71,
     108,    69,    -1,   124,   235,   128,    -1,   124,    70,    60,
     235,   136,   128,    -1,   125,   239,   128,    -1,    69,    -1,
      72,    -1,    71,    -1,   104,   129,   262,   130,    -1,   105,
     129,   239,   130,    -1,     7,   235,    -1,     6,   235,    -1,
       5,   129,   235,   130,    -1,     4,   235,    -1,     3,   235,
      -1,   239,    -1,    -1,   262,     8,   263,   239,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   140,   140,   144,   144,   145,   150,   151,   156,   156,
     157,   162,   163,   168,   173,   174,   174,   174,   175,   175,
     175,   176,   176,   176,   177,   177,   177,   181,   183,   185,
     178,   187,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   202,   202,
     203,   203,   203,   204,   204,   205,   209,   210,   214,   218,
     219,   224,   229,   229,   229,   231,   231,   231,   233,   233,
     234,   234,   239,   240,   245,   246,   251,   252,   257,   258,
     263,   264,   269,   270,   271,   272,   277,   278,   278,   279,
     279,   284,   285,   290,   291,   296,   298,   298,   302,   304,
     304,   308,   310,   314,   316,   321,   322,   327,   328,   329,
     330,   331,   332,   333,   334,   339,   340,   345,   346,   347,
     348,   349,   350,   354,   355,   360,   361,   362,   367,   368,
     369,   370,   376,   377,   382,   383,   383,   383,   385,   385,
     385,   391,   392,   396,   397,   398,   399,   403,   404,   405,
     410,   411,   415,   415,   416,   421,   421,   422,   423,   424,
     425,   425,   426,   426,   427,   428,   429,   430,   431,   432,
     433,   434,   435,   436,   437,   438,   439,   440,   441,   442,
     442,   443,   443,   444,   444,   445,   445,   446,   447,   448,
     449,   450,   451,   452,   453,   454,   455,   456,   457,   458,
     459,   460,   461,   462,   463,   464,   465,   466,   467,   468,
     469,   470,   471,   472,   471,   474,   475,   476,   477,   478,
     479,   480,   481,   482,   483,   484,   484,   485,   486,   487,
     488,   492,   492,   495,   495,   498,   498,   505,   506,   511,
     512,   513,   518,   519,   524,   525,   526,   527,   528,   529,
     530,   535,   536,   537,   538,   539,   544,   545,   546,   547,
     548,   549,   554,   555,   558,   560,   564,   565,   566,   567,
     571,   572,   577,   582,   587,   592,   593,   593,   598,   599,
     604,   605,   606,   611,   612,   616,   617,   621,   622,   622,
     626,   627,   627,   631,   632,   633,   637,   638,   643,   644,
     648,   649,   654,   655,   655,   656,   661,   662,   666,   667,
     668,   669,   670,   671,   672,   673,   677,   678,   679,   680,
     681,   682,   683,   684,   685,   686,   687,   688,   695,   696,
     696,   697,   698,   699,   700,   705,   706,   707,   712,   713,
     714,   715,   716,   717,   718,   722,   723,   723
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "T_REQUIRE_ONCE", "T_REQUIRE", "T_EVAL",
  "T_INCLUDE_ONCE", "T_INCLUDE", "','", "T_LOGICAL_OR", "T_LOGICAL_XOR",
  "T_LOGICAL_AND", "T_PRINT", "'='", "T_SR_EQUAL", "T_SL_EQUAL",
  "T_XOR_EQUAL", "T_OR_EQUAL", "T_AND_EQUAL", "T_MOD_EQUAL",
  "T_CONCAT_EQUAL", "T_DIV_EQUAL", "T_MUL_EQUAL", "T_MINUS_EQUAL",
  "T_PLUS_EQUAL", "'?'", "':'", "T_BOOLEAN_OR", "T_BOOLEAN_AND", "'|'",
  "'^'", "'&'", "T_IS_NOT_IDENTICAL", "T_IS_IDENTICAL", "T_IS_NOT_EQUAL",
  "T_IS_EQUAL", "'<'", "'>'", "T_IS_GREATER_OR_EQUAL",
  "T_IS_SMALLER_OR_EQUAL", "T_SR", "T_SL", "'+'", "'-'", "'.'", "'*'",
  "'/'", "'%'", "'!'", "'~'", "'@'", "T_UNSET_CAST", "T_BOOL_CAST",
  "T_OBJECT_CAST", "T_ARRAY_CAST", "T_STRING_CAST", "T_DOUBLE_CAST",
  "T_INT_CAST", "T_DEC", "T_INC", "'['", "T_NEW", "T_EXIT", "T_IF",
  "T_ELSEIF", "T_ELSE", "T_ENDIF", "T_LNUMBER", "T_DNUMBER", "T_STRING",
  "T_STRING_VARNAME", "T_VARIABLE", "T_NUM_STRING", "T_INLINE_HTML",
  "T_CHARACTER", "T_BAD_CHARACTER", "T_ENCAPSED_AND_WHITESPACE",
  "T_CONSTANT_ENCAPSED_STRING", "T_ECHO", "T_DO", "T_WHILE", "T_ENDWHILE",
  "T_FOR", "T_ENDFOR", "T_FOREACH", "T_ENDFOREACH", "T_DECLARE",
  "T_ENDDECLARE", "T_AS", "T_SWITCH", "T_ENDSWITCH", "T_CASE", "T_DEFAULT",
  "T_BREAK", "T_CONTINUE", "T_OLD_FUNCTION", "T_FUNCTION", "T_CONST",
  "T_RETURN", "T_USE", "T_GLOBAL", "T_STATIC", "T_VAR", "T_UNSET",
  "T_ISSET", "T_EMPTY", "T_CLASS", "T_EXTENDS", "T_OBJECT_OPERATOR",
  "T_DOUBLE_ARROW", "T_LIST", "T_ARRAY", "T_CLASS_C", "T_FUNC_C", "T_LINE",
  "T_FILE", "T_COMMENT", "T_ML_COMMENT", "T_OPEN_TAG",
  "T_OPEN_TAG_WITH_ECHO", "T_CLOSE_TAG", "T_WHITESPACE", "T_START_HEREDOC",
  "T_END_HEREDOC", "T_DOLLAR_OPEN_CURLY_BRACES", "T_CURLY_OPEN",
  "T_PAAMAYIM_NEKUDOTAYIM", "'{'", "'}'", "'('", "')'", "';'", "'$'",
  "'`'", "'\"'", "'''", "']'", "$accept", "start", "top_statement_list",
  "@1", "top_statement", "inner_statement_list", "@2", "inner_statement",
  "statement", "unticked_statement", "@3", "@4", "@5", "@6", "@7", "@8",
  "@9", "@10", "@11", "@12", "@13", "@14", "@15", "@16", "@17", "@18",
  "@19", "unset_variables", "unset_variable", "use_filename",
  "declaration_statement", "unticked_declaration_statement", "@20", "@21",
  "@22", "@23", "@24", "@25", "foreach_optional_arg", "for_statement",
  "foreach_statement", "declare_statement", "declare_list",
  "switch_case_list", "case_list", "@26", "@27", "case_separator",
  "while_statement", "elseif_list", "@28", "new_elseif_list", "@29",
  "else_single", "new_else_single", "parameter_list",
  "non_empty_parameter_list", "function_call_parameter_list",
  "non_empty_function_call_parameter_list", "global_var_list",
  "global_var", "static_var_list", "class_statement_list",
  "class_statement", "@30", "@31", "@32", "@33", "is_reference",
  "class_variable_decleration", "echo_expr_list", "for_expr",
  "non_empty_for_expr", "@34", "expr_without_variable", "@35", "@36",
  "@37", "@38", "@39", "@40", "@41", "@42", "@43", "@44", "function_call",
  "@45", "@46", "@47", "static_or_variable_string", "exit_expr",
  "ctor_arguments", "common_scalar", "static_scalar", "scalar",
  "static_array_pair_list", "possible_comma",
  "non_empty_static_array_pair_list", "expr", "r_cvar", "w_cvar",
  "rw_cvar", "cvar", "@48", "cvar_without_objects", "reference_variable",
  "compound_variable", "dim_offset", "ref_list", "@49", "object_property",
  "@50", "object_dim_list", "variable_name", "simple_indirect_reference",
  "assignment_list", "assignment_list_element", "@51", "array_pair_list",
  "non_empty_array_pair_list", "encaps_list", "encaps_var", "@52",
  "encaps_var_offset", "internal_functions_in_yacc", "isset_variables",
  "@53", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,    44,   263,
     264,   265,   266,    61,   267,   268,   269,   270,   271,   272,
     273,   274,   275,   276,   277,    63,    58,   278,   279,   124,
      94,    38,   280,   281,   282,   283,    60,    62,   284,   285,
     286,   287,    43,    45,    46,    42,    47,    37,    33,   126,
      64,   288,   289,   290,   291,   292,   293,   294,   295,   296,
      91,   297,   298,   299,   300,   301,   302,   303,   304,   305,
     306,   307,   308,   309,   310,   311,   312,   313,   314,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,   338,   339,   340,   341,   342,   343,   344,   345,
     346,   347,   348,   349,   350,   351,   352,   353,   354,   355,
     356,   357,   358,   359,   360,   361,   362,   123,   125,    40,
      41,    59,    36,    96,    34,    39,    93
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   137,   138,   140,   139,   139,   141,   141,   143,   142,
     142,   144,   144,   145,   146,   147,   148,   146,   149,   150,
     146,   151,   152,   146,   153,   154,   146,   155,   156,   157,
     146,   158,   146,   146,   146,   146,   146,   146,   146,   146,
     146,   146,   146,   146,   146,   146,   146,   159,   160,   146,
     161,   162,   146,   163,   146,   146,   164,   164,   165,   166,
     166,   167,   169,   170,   168,   171,   172,   168,   173,   168,
     174,   168,   175,   175,   176,   176,   177,   177,   178,   178,
     179,   179,   180,   180,   180,   180,   181,   182,   181,   183,
     181,   184,   184,   185,   185,   186,   187,   186,   188,   189,
     188,   190,   190,   191,   191,   192,   192,   193,   193,   193,
     193,   193,   193,   193,   193,   194,   194,   195,   195,   195,
     195,   195,   195,   196,   196,   197,   197,   197,   198,   198,
     198,   198,   199,   199,   200,   201,   202,   200,   203,   204,
     200,   205,   205,   206,   206,   206,   206,   207,   207,   207,
     208,   208,   210,   209,   209,   212,   211,   211,   211,   211,
     213,   211,   214,   211,   211,   211,   211,   211,   211,   211,
     211,   211,   211,   211,   211,   211,   211,   211,   211,   215,
     211,   216,   211,   217,   211,   218,   211,   211,   211,   211,
     211,   211,   211,   211,   211,   211,   211,   211,   211,   211,
     211,   211,   211,   211,   211,   211,   211,   211,   211,   211,
     211,   211,   219,   220,   211,   211,   211,   211,   211,   211,
     211,   211,   211,   211,   211,   221,   211,   211,   211,   211,
     211,   223,   222,   224,   222,   225,   222,   226,   226,   227,
     227,   227,   228,   228,   229,   229,   229,   229,   229,   229,
     229,   230,   230,   230,   230,   230,   231,   231,   231,   231,
     231,   231,   232,   232,   233,   233,   234,   234,   234,   234,
     235,   235,   236,   237,   238,   239,   240,   239,   241,   241,
     242,   242,   242,   243,   243,   244,   244,   245,   246,   245,
     247,   248,   247,   249,   249,   249,   250,   250,   251,   251,
     252,   252,   253,   254,   253,   253,   255,   255,   256,   256,
     256,   256,   256,   256,   256,   256,   257,   257,   257,   257,
     257,   257,   257,   257,   257,   257,   257,   257,   258,   259,
     258,   258,   258,   258,   258,   260,   260,   260,   261,   261,
     261,   261,   261,   261,   261,   262,   263,   262
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     3,     0,     1,     1,     0,     3,
       0,     1,     1,     1,     3,     0,     0,     9,     0,     0,
      12,     0,     0,     7,     0,     0,     9,     0,     0,     0,
      12,     0,     6,     2,     3,     2,     3,     2,     3,     3,
       3,     3,     3,     1,     2,     3,     5,     0,     0,    10,
       0,     0,    10,     0,     6,     1,     1,     3,     1,     1,
       3,     1,     0,     0,    11,     0,     0,    10,     0,     6,
       0,     8,     0,     2,     1,     4,     1,     4,     1,     4,
       3,     5,     3,     4,     4,     5,     0,     0,     6,     0,
       5,     1,     1,     1,     4,     0,     0,     7,     0,     0,
       8,     0,     2,     0,     3,     1,     0,     1,     2,     2,
       3,     3,     4,     4,     5,     1,     0,     1,     1,     2,
       3,     3,     4,     3,     1,     1,     2,     4,     3,     5,
       1,     3,     2,     0,     3,     0,     0,    11,     0,     0,
      10,     0,     1,     3,     5,     1,     3,     0,     3,     1,
       0,     1,     0,     4,     1,     0,     7,     3,     4,     4,
       0,     7,     0,     4,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     2,     2,     2,     2,     0,
       4,     0,     4,     0,     4,     0,     4,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     2,
       2,     2,     2,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     0,     0,     7,     1,     1,     2,     2,     2,
       2,     2,     2,     2,     2,     0,     3,     1,     4,     3,
       2,     0,     5,     0,     5,     0,     7,     1,     1,     0,
       2,     3,     0,     3,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     2,     4,     1,     1,     1,     3,
       3,     3,     0,     2,     0,     1,     5,     3,     3,     1,
       1,     1,     1,     1,     1,     1,     0,     4,     1,     2,
       4,     4,     1,     1,     4,     0,     1,     1,     0,     4,
       1,     0,     2,     4,     4,     1,     1,     3,     1,     2,
       3,     1,     1,     0,     5,     0,     0,     2,     5,     3,
       3,     1,     6,     4,     4,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     0,     1,     0,
       5,     3,     3,     6,     3,     1,     1,     1,     4,     4,
       2,     2,     4,     2,     2,     1,     0,     4
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       5,     0,     3,     1,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   225,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   239,     0,   244,   245,
     256,   257,   283,    43,   246,   147,    24,     0,     0,     0,
      53,     0,     0,     0,    65,    62,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   249,   250,   247,   248,
     327,    10,     0,    55,   298,   327,   327,   327,     4,     6,
      13,     7,    61,   271,   215,   258,   227,     0,   270,     0,
     272,   275,   278,   282,     0,   216,   344,   343,     0,   341,
     340,   230,   199,   200,   201,   202,     0,   223,   222,   221,
     220,   219,   218,   217,   178,   274,   176,   237,   162,   238,
     272,     0,   224,     0,     0,   231,     0,   149,     0,    21,
     150,     0,     0,     0,    33,     0,    35,     0,   141,   141,
      37,   271,     0,   272,    59,     0,     0,   125,     0,     0,
     124,   130,     0,     0,     0,     0,    68,   155,   306,     0,
       8,     0,     0,     0,     0,     0,   183,     0,   185,   212,
     179,   181,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    44,   177,   175,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   233,   276,   285,     0,
     299,   279,     0,   226,   242,   240,     0,     0,     0,   116,
       0,    42,     0,     0,     0,   151,   154,   271,     0,   272,
       0,     0,    34,    36,   142,     0,     0,    38,    39,     0,
      45,     0,   126,     0,    40,     0,     0,    41,     0,    56,
      58,   345,     0,     0,     0,     0,   305,     0,   311,     0,
     264,   322,   317,   328,   318,   320,   321,   319,   326,   261,
       0,     0,   324,   325,   323,   316,    14,     0,   211,     0,
     229,   259,   260,     0,   187,     0,     0,     0,     0,   188,
     190,   189,   204,   203,   206,   205,   207,   209,   210,   208,
     198,   197,   192,   193,   191,   194,   195,   196,     0,   157,
     174,   173,   172,   171,   170,   169,   168,   167,   166,   165,
     164,   116,     0,   286,     0,     0,   342,   116,   163,   241,
      15,   235,     0,     0,   115,   271,   272,   148,     0,     0,
      27,   152,    50,    47,     0,     0,    31,    66,    63,    60,
       0,   123,     0,     0,   252,     0,   251,   131,   128,     0,
       0,   346,   338,   339,    70,   133,     0,   302,     0,   301,
     315,   273,     0,   228,   265,   307,   329,     0,   257,     0,
       0,     9,    11,    12,   284,   184,   186,     0,   180,   182,
       0,     0,   159,   158,   273,     0,   296,     0,   291,   277,
     287,   290,   295,   280,   281,     0,    18,     0,   116,   119,
     232,     0,    25,    22,   150,     0,     0,     0,     0,     0,
       0,     0,   106,     0,   127,   253,   254,   262,     0,    57,
      46,     0,     0,     0,   303,   305,     0,     0,   310,     0,
     309,     0,   331,     0,   332,   334,   213,   160,   234,     0,
     292,   288,   285,     0,   243,    10,    16,     0,     0,   271,
     272,     0,     0,     0,   153,    72,    72,    80,     0,    10,
      78,    54,    86,    86,    32,     0,   107,     0,     0,   105,
     106,   269,     0,   264,   129,   347,   133,   138,   135,     0,
      69,   132,   305,   300,     0,   314,   313,     0,   335,   337,
     336,     0,     0,     0,   242,   297,     0,     0,     0,     8,
      95,   236,   122,     0,    10,    93,    23,    28,     0,     0,
       0,     0,     8,    86,     0,    86,     0,   108,     0,   109,
      10,     0,     0,     0,   255,   265,   263,     0,   141,   141,
     145,     0,     0,   156,     0,   308,   330,     0,   214,   161,
     289,   293,   294,    98,   101,     0,     8,   150,    73,    51,
      48,    81,     0,     0,     0,     0,     0,     0,    82,   110,
       8,     0,   111,     0,     0,   268,   267,    71,     0,     0,
       0,     0,   134,   304,   312,   333,   103,     0,     0,    17,
      26,     0,     0,     0,     0,    79,     0,    84,     0,    91,
      92,    89,    83,     0,   112,     0,   113,    10,     0,   139,
     136,   146,   143,     0,     0,     0,     0,   102,    94,    29,
      10,    76,    52,    49,    85,    87,    10,    67,   114,     8,
     266,   106,     0,     0,     0,    10,     0,     0,     0,     8,
      10,     8,    64,     0,   106,   144,     0,     8,    20,    96,
      10,    74,    30,     0,     8,    10,     0,     0,     0,     8,
      77,     8,     0,    99,    97,     0,     0,    10,    10,    75,
     140,     8,     8,   137
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     4,    68,   150,   267,   371,   611,    70,
     397,   500,   445,   543,   213,   452,   118,   451,   404,   547,
     628,   411,   407,   584,   406,   583,   122,   238,   239,   136,
      71,    72,   129,   413,   128,   412,   245,   422,   509,   642,
     612,   461,   335,   464,   514,   630,   616,   591,   506,   544,
     648,   576,   658,   579,   605,   468,   469,   323,   324,   139,
     140,   142,   423,   481,   529,   622,   528,   621,   225,   531,
     116,   214,   215,   405,    73,   246,   494,   204,   277,   278,
     273,   275,   276,   493,    96,    74,   209,   311,   398,   108,
     112,   318,    75,   347,    76,   472,   365,   473,    77,    78,
     218,    79,    80,   312,    81,    82,    83,   314,   389,   496,
     390,   440,   391,   392,    84,   358,   359,   482,   249,   250,
     149,   265,   431,   491,    85,   242,   421
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -421
static const yytype_int16 yypact[] =
{
    -421,    62,    74,  -421,   603,  2038,  2038,   -78,  2038,  2038,
    2038,  2038,  2038,  2038,  2038,  -421,  2038,  2038,  2038,  2038,
    2038,  2038,  2038,   -19,   -19,     2,   -40,   -29,  -421,  -421,
     -42,  -421,  -421,  -421,  -421,  2038,  -421,   -27,   -21,    -9,
    -421,    24,  1281,  1313,  -421,  -421,  1426,   -31,    -8,    14,
      65,    83,    92,    56,    94,    96,  -421,  -421,  -421,  -421,
    -421,  -421,  2038,  -421,   112,  -421,  -421,  -421,  -421,  -421,
    -421,  -421,  -421,  -421,  -421,  -421,  -421,  2265,  -421,    47,
     520,    36,   -30,  -421,     4,  -421,  3334,  3334,  2038,  3334,
    3334,  3432,    23,    23,  -421,  -421,  2038,  -421,  -421,  -421,
    -421,  -421,  -421,  -421,  -421,  -421,  -421,  -421,  -421,  -421,
    -421,  1458,  -421,  2038,     2,  -421,    16,  3334,  1168,  -421,
    2038,  2038,   126,  2038,  -421,  2310,  -421,  2370,   106,   106,
    -421,   141,  3334,   732,  -421,   180,   152,  -421,   -44,    17,
    -421,   246,    20,   -19,   -19,   -19,   177,  -421,  1571,   219,
     158,  2415,  2038,   845,   489,   958,  -421,  2038,  -421,  -421,
    -421,  -421,  2038,  2038,  2038,  2038,  2038,  2038,  2038,  2038,
    2038,  2038,  2038,  2038,  2038,  2038,  2038,  2038,  2038,  2038,
    2038,  -421,  -421,  -421,  1603,  2038,  2038,  2038,  2038,  2038,
    2038,  2038,  2038,  2038,  2038,  2038,  -421,  -421,  2038,  2038,
     112,   -30,  2475,  -421,   160,  -421,  2519,  2579,   167,  1716,
    2038,  -421,   218,  2038,   156,   291,  3334,   212,   214,  3174,
     232,  2623,  -421,  -421,  -421,   235,   237,  -421,  -421,   186,
    -421,  2038,  -421,    -8,  -421,   365,   247,  -421,    15,  -421,
    -421,  -421,    27,   187,   253,   196,   -33,   -19,  3209,   194,
     318,  -421,  -421,    -7,  -421,  -421,  -421,  -421,  -421,  -421,
    2151,   -19,  -421,  -421,  -421,  -421,  -421,   603,  -421,  2891,
    -421,  -421,  -421,  2038,  3409,  2038,  2038,  2038,  2038,  2144,
    2536,  2639,  2743,  2743,  2743,  2743,  1268,  1268,  1268,  1268,
     207,   207,    23,    23,    23,  -421,  -421,  -421,   -22,  3432,
    3432,  3432,  3432,  3432,  3432,  3432,  3432,  3432,  3432,  3432,
    3432,  1716,   -10,  3334,   192,  2935,  -421,  1716,  -421,  -421,
     303,  -421,   -19,   201,   322,    28,   357,  3334,   204,  2683,
    -421,  -421,  -421,  -421,   319,    29,  -421,  -421,  -421,  -421,
    2993,  -421,   365,   365,  -421,   205,  -421,  -421,   323,   -19,
     206,  -421,  -421,  -421,  -421,  -421,   210,  -421,    68,  -421,
    -421,  -421,  1748,  -421,  1861,  -421,  -421,   272,   285,  3037,
     220,  -421,  -421,  -421,  -421,  3372,  3432,  3295,  2435,  1162,
       2,   -42,  -421,  -421,   222,   224,  -421,  2038,  -421,   248,
    -421,   -15,  -421,  -421,  -421,   228,  -421,  1168,  1716,  -421,
    -421,  1893,  -421,  -421,  2038,  2038,   -19,   -19,   365,   290,
     716,     8,    19,   234,  -421,  -421,  -421,   365,   365,  -421,
    -421,   -19,   240,   114,  -421,   -33,   348,   -19,  3334,   -19,
    3248,    10,  -421,  2038,  -421,  -421,  -421,  -421,  -421,  3095,
    -421,  -421,  2038,  2038,  -421,  -421,  -421,   238,   -19,    70,
     498,  2038,   829,   251,  3334,   260,   260,  -421,   370,  -421,
    -421,  -421,   255,   258,  -421,   317,   377,   321,   266,   389,
      19,   289,   270,   401,  -421,  -421,  -421,  -421,  -421,   339,
    -421,  -421,   -33,  -421,  2038,  -421,  -421,  2006,  -421,  -421,
    -421,   275,   426,  2038,   160,  -421,   -10,   276,  3139,   166,
    -421,  -421,  -421,  2727,  -421,  -421,  -421,  -421,   -19,   283,
     287,   365,   331,  -421,   145,  -421,   -36,  -421,   365,  -421,
    -421,    33,   292,   365,  -421,   365,  -421,   132,   106,   106,
     408,    21,    72,  3432,   -19,  3334,  -421,   295,  2331,  -421,
    -421,  -421,  -421,  -421,   197,   293,   345,  2038,  -421,  -421,
    -421,  -421,   296,   153,   297,  2038,     0,   128,  -421,  -421,
     299,   359,   425,   368,   316,  -421,   335,  -421,   376,   378,
     365,   375,  -421,  -421,  -421,  -421,   213,   346,  1168,  -421,
    -421,   343,   320,   942,   942,  -421,   350,  -421,  2205,  -421,
    -421,  -421,  -421,   353,  -421,   365,  -421,  -421,   365,  -421,
    -421,  -421,   475,   360,   464,   427,  2038,  -421,  -421,  -421,
    -421,  -421,  -421,  -421,  -421,  -421,  -421,  -421,  -421,   364,
    -421,    19,   366,   365,  2038,  -421,   363,  2787,  1055,   411,
    -421,   -25,  -421,   369,    19,  -421,  2831,   431,  -421,  -421,
    -421,  -421,  -421,   371,    51,  -421,   373,   473,  1168,   417,
    -421,   393,   380,  -421,  -421,   374,   394,  -421,  -421,  -421,
    -421,   398,   200,  -421
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -421,  -421,  -421,  -421,  -421,  -305,  -421,  -421,    -4,  -421,
    -421,  -421,  -421,  -421,  -421,  -421,  -421,  -421,  -421,  -421,
    -421,  -421,  -421,  -421,  -421,  -421,  -421,  -421,   159,  -421,
     262,  -421,  -421,  -421,  -421,  -421,  -421,  -421,    71,  -421,
     -53,  -421,  -421,  -421,  -420,  -421,  -421,   -41,  -421,  -421,
    -421,  -421,  -421,  -421,  -421,  -416,  -421,  -269,  -421,  -421,
     313,  -421,    76,  -421,  -421,  -421,  -421,  -421,  -124,  -421,
    -421,  -391,  -421,  -421,   -14,  -421,  -421,  -421,  -421,  -421,
    -421,  -421,  -421,  -421,  -421,   250,  -421,  -421,  -421,  -113,
    -421,    60,  -204,  -285,  -421,  -421,    77,  -421,    -2,   -23,
    -226,   257,   103,  -421,  -290,   471,  -421,   117,  -421,  -421,
      73,  -421,  -421,  -421,  -421,    84,   142,  -421,  -421,  -421,
     203,  -421,  -421,  -421,  -421,  -421,  -421
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -275
static const yytype_int16 yytable[] =
{
      69,   208,   109,    86,    87,   226,    89,    90,    91,    92,
      93,    94,    95,   453,    97,    98,    99,   100,   101,   102,
     103,   360,   388,   349,   210,   233,   589,    32,   236,   571,
     198,   346,   131,   117,   462,   351,  -117,   409,    32,   380,
     125,   127,   385,   516,   132,   442,   134,   381,   395,    32,
     465,    88,    32,   366,   522,   555,   556,   415,   416,   386,
     151,    32,     3,   137,   561,   -90,   -90,   -90,   178,   179,
     180,   107,   383,    32,    -2,    32,   425,   356,  -120,   488,
     425,   489,   490,   231,   114,   141,   202,   115,    64,   111,
     466,   109,   558,   553,   203,   557,   399,   199,   135,    64,
     113,   367,   119,   -90,   562,   182,   183,   217,   120,   206,
      64,   207,   443,    64,   212,   232,   467,   387,   216,   132,
     121,   221,    64,   457,   138,   146,   105,   105,   110,   447,
     563,   590,   471,   474,    64,   463,   200,   224,   346,   346,
     499,   -88,   -88,   -88,   197,   350,   248,   211,   234,   133,
     269,   237,   572,   123,   512,   274,   582,   352,  -117,   410,
     279,   280,   281,   282,   283,   284,   285,   286,   287,   288,
     289,   290,   291,   292,   293,   294,   295,   296,   297,   -88,
     455,   456,   299,   300,   301,   302,   303,   304,   305,   306,
     307,   308,   309,   310,   143,   325,   313,   315,   426,   546,
    -120,   485,   573,   486,   346,   633,   388,   132,   327,   477,
     478,   329,   144,   346,   346,   560,   479,   110,   646,   555,
     556,   145,   502,   147,   219,   148,   551,   477,   478,   340,
     -19,   -19,   -19,   559,   479,   554,   555,   556,   565,   152,
     566,   110,   480,   586,   555,   556,   240,   241,   243,   175,
     176,   177,   178,   179,   180,   220,   592,   229,   369,   235,
     567,   577,   578,   372,  -100,  -100,  -100,   437,   153,   154,
     155,   375,   227,   376,   377,   378,   379,   603,   604,   251,
     104,   106,   548,   230,   244,   601,   266,   330,   252,   317,
     253,   254,   619,   255,   256,   257,   321,   325,   328,   331,
     332,   334,   333,   325,   337,   629,   338,   346,   574,   132,
     618,   631,   326,   620,   346,   132,   339,   353,   348,   346,
     637,   346,   354,   355,   363,   644,   364,   258,   393,   396,
     401,   400,   408,   402,   417,   649,   418,   420,   635,   424,
     651,   432,   259,   260,   261,   433,   262,   263,   435,   357,
     361,   196,   661,   662,   438,   264,   441,   109,   444,   458,
     428,   484,   430,   470,   370,  -118,   346,   476,   501,   508,
     184,   185,   186,   187,   188,   189,   190,   191,   192,   193,
     194,   195,   507,   511,   325,   439,   513,   449,   517,   515,
     518,   346,   519,   446,   346,   520,   132,   521,   523,   132,
     524,   384,   216,   454,   568,   569,   460,   342,   343,   525,
     530,   536,   541,   549,   326,  -274,  -274,   550,   552,   346,
     326,   570,   564,   575,   580,   361,   581,   585,   587,   593,
     594,   492,    28,    29,   344,   156,   157,   158,   595,   596,
     313,   498,    34,   597,   598,   599,   602,   600,   505,   503,
     609,   159,   240,   160,   161,   162,   163,   164,   165,   166,
     167,   168,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,   608,   606,   345,    56,    57,    58,
      59,   614,   533,   110,   617,   535,   196,  -118,   623,   624,
     625,   538,   632,   626,   638,   634,   643,  -104,   645,   653,
     655,   326,   650,   652,   450,   659,  -121,   657,   419,   361,
     361,   184,   185,   186,   187,   188,   189,   190,   191,   192,
     193,   194,   195,   656,   475,   660,   663,   510,   357,   373,
     361,   613,   361,   184,   185,   186,   187,   188,   189,   190,
     191,   192,   193,   194,   195,   216,   341,   615,   382,   251,
     526,   361,   527,   588,   539,   201,  -274,  -274,   252,   497,
     253,   254,   537,   255,   256,   257,   532,   483,     0,   540,
       0,     0,     0,     0,   607,     0,     0,     0,  -274,  -274,
       0,     0,     0,     0,     0,   357,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   258,     0,     0,
       0,     0,     0,     0,   627,     0,     5,     6,     7,     8,
       9,   361,     0,   260,   261,    10,   262,   263,     0,     0,
       0,     0,   636,   271,   641,   264,     0,   196,  -121,     0,
       0,     0,     0,     0,     0,     0,     0,   361,     0,     0,
       0,     0,     0,     0,   654,    11,    12,     0,     0,   196,
       0,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,    25,    26,    27,     0,     0,     0,
      28,    29,    30,    31,    32,     0,    33,     0,     0,     0,
      34,    35,    36,    37,     0,    38,     0,    39,     0,    40,
       0,     0,    41,     0,     0,     0,    42,    43,    44,    45,
       0,    46,    47,    48,    49,     0,    50,    51,    52,    53,
       0,     0,     0,    54,    55,    56,    57,    58,    59,     5,
       6,     7,     8,     9,     0,    60,     0,     0,    10,     0,
      61,     0,    62,     0,    63,    64,    65,    66,    67,     0,
       0,     0,   459,     0,     0,   184,   185,   186,   187,   188,
     189,   190,   191,   192,   193,   194,   195,     0,    11,    12,
       0,     0,     0,     0,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,     0,    25,    26,    27,
       0,     0,     0,    28,    29,    30,    31,    32,     0,    33,
    -274,  -274,     0,    34,    35,    36,    37,     0,    38,     0,
      39,     0,    40,     0,     0,    41,     0,     0,     0,    42,
      43,     0,     0,     0,    46,    47,    48,    49,     0,    50,
      51,    52,     0,     0,     0,     0,    54,    55,    56,    57,
      58,    59,     5,     6,     7,     8,     9,     0,    60,     0,
       0,    10,     0,    61,     0,    62,     0,    63,    64,    65,
      66,    67,     0,     0,     0,   504,     0,     0,     0,     0,
       0,   196,     0,   228,     0,     0,     0,     0,     0,     0,
       0,    11,    12,     0,     0,     0,     0,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
      25,    26,    27,     0,     0,     0,    28,    29,    30,    31,
      32,     0,    33,     0,     0,   251,    34,    35,    36,    37,
       0,    38,     0,    39,   252,    40,   253,   254,    41,   255,
     256,   257,    42,    43,     0,     0,     0,    46,    47,    48,
      49,     0,    50,    51,    52,     0,     0,     0,     0,    54,
      55,    56,    57,    58,    59,     5,     6,     7,     8,     9,
       0,    60,     0,   258,    10,     0,    61,     0,    62,     0,
      63,    64,    65,    66,    67,     0,     0,     0,   610,   260,
     261,     0,   262,   263,     0,     0,     0,     0,   270,     0,
       0,   264,     0,     0,    11,    12,     0,     0,     0,     0,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,    25,    26,    27,     0,     0,     0,    28,
      29,    30,    31,    32,     0,    33,     0,     0,   251,    34,
      35,    36,    37,     0,    38,     0,    39,   252,    40,   253,
     254,    41,   255,   256,   257,    42,    43,     0,     0,     0,
      46,    47,    48,    49,     0,    50,    51,    52,     0,     0,
       0,     0,    54,    55,    56,    57,    58,    59,     5,     6,
       7,     8,     9,     0,    60,     0,   258,    10,     0,    61,
       0,    62,     0,    63,    64,    65,    66,    67,     0,     0,
       0,   640,   260,   261,     0,   262,   263,     0,     0,     0,
       0,     0,     0,   272,   264,     0,     0,    11,    12,     0,
       0,     0,     0,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,     0,    25,    26,    27,     0,
       0,     0,    28,    29,    30,    31,    32,     0,    33,     0,
       0,     0,    34,    35,    36,    37,     0,    38,     0,    39,
       0,    40,     0,     0,    41,     0,     0,     0,    42,    43,
       0,     0,     0,    46,    47,    48,    49,     0,    50,    51,
      52,     0,     0,     0,     0,    54,    55,    56,    57,    58,
      59,     5,     6,     7,     8,     9,     0,    60,     0,     0,
      10,     0,    61,     0,    62,     0,    63,    64,    65,    66,
      67,   162,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180,
      11,    12,     0,     0,     0,     0,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,     0,    25,
      26,    27,     0,     0,     0,    28,    29,    30,    31,    32,
       0,    33,     0,     0,     0,    34,    35,    36,    37,     0,
      38,     0,    39,     0,    40,     0,     0,    41,     0,     0,
       0,    42,    43,     0,     0,     0,    46,    47,    48,    49,
       0,    50,    51,    52,     0,     0,     0,     0,    54,    55,
      56,    57,    58,    59,     5,     6,     7,     8,     9,     0,
      60,     0,     0,    10,     0,    61,     0,    62,     0,    63,
      64,    65,    66,    67,  -275,  -275,  -275,  -275,   173,   174,
     175,   176,   177,   178,   179,   180,     5,     6,     7,     8,
       9,     0,     0,    11,    12,    10,     0,     0,     0,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,     0,    25,    26,     0,     0,     0,     0,    28,    29,
      30,    31,    32,     0,     0,    11,    12,     0,    34,     0,
       0,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,    25,    26,     0,     0,     0,     0,
      28,    29,    30,    31,    32,    51,    52,     0,     0,     0,
      34,    54,    55,    56,    57,    58,    59,     0,     0,     0,
       0,     0,     0,    60,     0,     0,     0,     0,     0,     0,
      62,     0,   124,    64,    65,    66,    67,    51,    52,     0,
       0,     0,     0,    54,    55,    56,    57,    58,    59,     5,
       6,     7,     8,     9,     0,    60,     0,     0,    10,     0,
       0,     0,    62,     0,   126,    64,    65,    66,    67,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     8,     9,     0,     0,    11,    12,
      10,     0,     0,     0,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,     0,    25,    26,     0,
       0,     0,     0,    28,    29,    30,    31,    32,     0,     0,
      11,    12,     0,    34,     0,     0,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,     0,    25,
      26,     0,     0,     0,     0,    28,    29,    30,    31,    32,
      51,    52,     0,     0,     0,    34,    54,    55,    56,    57,
      58,    59,     0,     0,     0,     0,     0,     0,    60,     0,
       0,     0,     0,     0,     0,    62,     0,   130,    64,    65,
      66,    67,    51,    52,     0,     0,     0,     0,    54,    55,
      56,    57,    58,    59,     5,     6,     7,     8,     9,     0,
      60,     0,     0,    10,     0,     0,     0,    62,   205,     0,
      64,    65,    66,    67,     0,     0,     0,     0,     0,     0,
       0,     0,   247,     0,     0,     0,     5,     6,     7,     8,
       9,     0,     0,    11,    12,    10,     0,     0,     0,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,     0,    25,    26,   298,     0,     0,     0,    28,    29,
      30,    31,    32,     0,     0,    11,    12,     0,    34,     0,
       0,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,    25,    26,     0,     0,     0,     0,
      28,    29,    30,    31,    32,    51,    52,     0,     0,     0,
      34,    54,    55,    56,    57,    58,    59,     0,     0,     0,
       0,     0,     0,    60,     0,     0,     0,     0,     0,     0,
      62,     0,     0,    64,    65,    66,    67,    51,    52,     0,
       0,     0,     0,    54,    55,    56,    57,    58,    59,     5,
       6,     7,     8,     9,     0,    60,     0,     0,    10,     0,
       0,     0,    62,     0,     0,    64,    65,    66,    67,     0,
       0,     0,     0,     0,     0,     0,     0,   322,     0,     0,
       0,     5,     6,     7,     8,     9,     0,     0,    11,    12,
      10,     0,     0,     0,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,     0,    25,    26,   427,
       0,     0,     0,    28,    29,    30,    31,    32,     0,     0,
      11,    12,     0,    34,     0,     0,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,     0,    25,
      26,     0,     0,     0,     0,    28,    29,    30,    31,    32,
      51,    52,     0,     0,     0,    34,    54,    55,    56,    57,
      58,    59,     0,     0,     0,     0,     0,     0,    60,     0,
       0,     0,     0,     0,     0,    62,     0,     0,    64,    65,
      66,    67,    51,    52,     0,     0,     0,     0,    54,    55,
      56,    57,    58,    59,     5,     6,     7,     8,     9,     0,
      60,     0,     0,    10,     0,     0,     0,    62,     0,     0,
      64,    65,    66,    67,     0,     0,     0,     0,     0,     0,
       0,     0,   429,     0,     0,     0,     5,     6,     7,     8,
       9,     0,     0,    11,    12,    10,     0,     0,     0,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,     0,    25,    26,   448,     0,     0,     0,    28,    29,
      30,    31,    32,     0,     0,    11,    12,     0,    34,     0,
       0,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,     0,    25,    26,     0,     0,     0,     0,
      28,    29,    30,    31,    32,    51,    52,     0,     0,     0,
      34,    54,    55,    56,    57,    58,    59,     0,     0,     0,
       0,     0,     0,    60,     0,     0,     0,     0,     0,     0,
      62,     0,     0,    64,    65,    66,    67,    51,    52,     0,
       0,     0,     0,    54,    55,    56,    57,    58,    59,     5,
       6,     7,     8,     9,     0,    60,     0,     0,    10,     0,
       0,     0,    62,     0,     0,    64,    65,    66,    67,     0,
       0,     0,     0,     0,     0,     0,     0,   534,     0,     0,
       0,     5,     6,     7,     8,     9,     0,     0,    11,    12,
      10,     0,     0,     0,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,     0,    25,    26,     0,
       0,     0,     0,    28,    29,    30,    31,    32,     0,     0,
      11,    12,     0,    34,     0,     0,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,     0,    25,
      26,     0,     0,     0,     0,    28,    29,    30,    31,    32,
      51,    52,     0,     0,     0,    34,    54,    55,    56,    57,
      58,    59,     0,     0,     0,     0,     0,     0,    60,     0,
       0,     0,     0,     0,     0,    62,     0,     0,    64,    65,
      66,    67,    51,    52,     0,     0,     0,     0,    54,    55,
      56,    57,    58,    59,     5,     6,     7,     8,     9,     0,
      60,     0,     0,    10,     0,     0,     0,    62,     0,     0,
      64,    65,    66,    67,   163,   164,   165,   166,   167,   168,
     169,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,     0,    11,    12,     0,     0,     0,     0,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,     0,    25,    26,   156,   157,   158,     0,    28,    29,
      30,   368,    32,     0,     0,     0,     0,     0,    34,     0,
     159,   589,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,     0,     0,    51,    52,     0,     0,     0,
       0,    54,    55,    56,    57,    58,    59,     0,     0,     0,
       0,     0,     0,    60,   156,   157,   158,     0,     0,     0,
      62,     0,     0,    64,    65,    66,    67,     0,     0,     0,
     159,     0,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,     0,     0,     0,     0,     0,     0,   156,
     157,   158,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   159,   590,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   156,
     157,   158,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   159,   181,   160,   161,   162,
     163,   164,   165,   166,   167,   168,   169,   170,   171,   172,
     173,   174,   175,   176,   177,   178,   179,   180,     0,     0,
       0,     0,     0,     0,   156,   157,   158,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     159,   222,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,     0,   156,   157,   158,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     159,   223,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,     0,     0,     0,     0,     0,   156,   157,
     158,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   159,   268,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   164,   165,   166,
     167,   168,   169,   170,   171,   172,   173,   174,   175,   176,
     177,   178,   179,   180,     0,     0,     0,     0,   156,   157,
     158,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   159,   316,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,     0,     0,     0,
       0,     0,   156,   157,   158,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   159,   319,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,     0,     0,     0,
       0,     0,   156,   157,   158,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   159,   320,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,     0,     0,     0,     0,     0,   156,   157,   158,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   159,   336,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,  -275,  -275,  -275,  -275,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,     0,     0,     0,     0,     0,   156,   157,   158,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   159,   403,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,     0,     0,     0,     0,     0,
     156,   157,   158,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   159,   545,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     156,   157,   158,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   159,   639,   160,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,     0,
       0,     0,     0,     0,   156,   157,   158,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     159,   647,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   156,   157,   158,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   159,   374,
     160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
     170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
     180,     0,     0,     0,     0,     0,   156,   157,   158,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   159,   394,   160,   161,   162,   163,   164,   165,
     166,   167,   168,   169,   170,   171,   172,   173,   174,   175,
     176,   177,   178,   179,   180,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   156,   157,   158,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     159,   414,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,     0,     0,     0,     0,     0,   156,   157,
     158,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   159,   434,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   184,   185,   186,
     187,   188,   189,   190,   191,   192,   193,   194,   195,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   156,   157,
     158,     0,     0,   495,     0,     0,     0,     0,     0,     0,
       0,     0,  -274,  -274,   159,     0,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   156,   157,   158,
       0,     0,  -273,     0,     0,     0,     0,   542,     0,     0,
       0,     0,     0,   159,     0,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,     0,     0,     0,     0,
       0,     0,     0,   196,   156,   157,   158,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   362,     0,
     159,   436,   160,   161,   162,   163,   164,   165,   166,   167,
     168,   169,   170,   171,   172,   173,   174,   175,   176,   177,
     178,   179,   180,   156,   157,   158,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   487,     0,   159,
       0,   160,   161,   162,   163,   164,   165,   166,   167,   168,
     169,   170,   171,   172,   173,   174,   175,   176,   177,   178,
     179,   180,   157,   158,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   159,     0,   160,
     161,   162,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180,
     158,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   159,     0,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   175,   176,   177,   178,   179,   180,   159,     0,   160,
     161,   162,   163,   164,   165,   166,   167,   168,   169,   170,
     171,   172,   173,   174,   175,   176,   177,   178,   179,   180
};

static const yytype_int16 yycheck[] =
{
       4,   114,    25,     5,     6,   129,     8,     9,    10,    11,
      12,    13,    14,   404,    16,    17,    18,    19,    20,    21,
      22,   247,   312,     8,     8,     8,    26,    71,     8,     8,
      60,   235,    46,    35,    26,     8,     8,     8,    71,    61,
      42,    43,   311,   463,    46,    60,    77,    69,   317,    71,
      31,   129,    71,    60,   470,    91,    92,   342,   343,    69,
      62,    71,     0,    71,    31,    90,    91,    92,    45,    46,
      47,    69,   298,    71,     0,    71,     8,   110,     8,    69,
       8,    71,    72,   127,   126,    71,    88,   129,   132,   129,
      71,   114,   128,   513,    96,   515,   322,   127,   129,   132,
     129,   108,   129,   128,    71,    58,    59,   121,   129,   111,
     132,   113,   127,   132,   118,   138,    97,   127,   120,   121,
     129,   123,   132,   408,   132,    69,    23,    24,    25,   398,
      97,   131,   417,   418,   132,   127,   132,    31,   342,   343,
     445,    90,    91,    92,   108,   130,   148,   131,   131,    46,
     152,   131,   131,   129,   459,   157,   547,   130,   130,   130,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   176,   177,   178,   179,   180,   128,
     406,   407,   184,   185,   186,   187,   188,   189,   190,   191,
     192,   193,   194,   195,   129,   209,   198,   199,   130,   504,
     130,   427,   130,   429,   408,   621,   496,   209,   210,    95,
      96,   213,   129,   417,   418,   520,   102,   114,   634,    91,
      92,   129,   448,   129,   121,   129,   511,    95,    96,   231,
      64,    65,    66,   518,   102,    90,    91,    92,   523,   127,
     525,   138,   128,    90,    91,    92,   143,   144,   145,    42,
      43,    44,    45,    46,    47,   129,   128,    77,   260,    13,
     128,    64,    65,   267,    64,    65,    66,   380,    65,    66,
      67,   273,   131,   275,   276,   277,   278,    64,    65,    60,
      23,    24,   508,   131,   107,   570,   128,   131,    69,   129,
      71,    72,   597,    74,    75,    76,   129,   311,    80,     8,
      88,    69,    88,   317,    69,   610,    69,   511,   534,   311,
     595,   616,   209,   598,   518,   317,   130,   130,    71,   523,
     625,   525,    69,   127,   130,   630,     8,   108,   136,    26,
       8,   130,    13,   129,   129,   640,    13,   131,   623,   129,
     645,    69,   123,   124,   125,    60,   127,   128,   128,   246,
     247,   129,   657,   658,   130,   136,   108,   380,   130,    69,
     362,    13,   364,   129,   261,     8,   570,   127,   130,   109,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,   131,    13,   398,   387,   131,   401,    71,   131,
      13,   595,    71,   397,   598,   129,   398,     8,   109,   401,
     130,   298,   404,   405,   528,   529,   410,    42,    43,     8,
      71,   136,   136,   130,   311,    58,    59,   130,    87,   623,
     317,    13,   130,   128,   131,   322,    81,   131,   131,   130,
      71,   433,    67,    68,    69,     9,    10,    11,    13,    71,
     442,   443,    77,   127,   109,    69,    71,    69,   452,   451,
     130,    25,   349,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,   131,   129,   111,   112,   113,   114,
     115,   131,   484,   380,   131,   487,   129,   130,    13,   129,
      26,   493,   128,    66,   131,   129,    85,    66,   129,    26,
      83,   398,   131,   130,   401,   131,     8,   127,   349,   406,
     407,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,   130,   421,   131,   128,   456,   425,   267,
     427,   584,   429,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,   547,   233,   588,   298,    60,
     473,   448,   476,   555,   494,    84,    58,    59,    69,   442,
      71,    72,   136,    74,    75,    76,   482,   425,    -1,   496,
      -1,    -1,    -1,    -1,   578,    -1,    -1,    -1,    58,    59,
      -1,    -1,    -1,    -1,    -1,   482,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,    -1,    -1,
      -1,    -1,    -1,    -1,   606,    -1,     3,     4,     5,     6,
       7,   508,    -1,   124,   125,    12,   127,   128,    -1,    -1,
      -1,    -1,   624,   134,   628,   136,    -1,   129,   130,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   534,    -1,    -1,
      -1,    -1,    -1,    -1,   648,    42,    43,    -1,    -1,   129,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    -1,    61,    62,    63,    -1,    -1,    -1,
      67,    68,    69,    70,    71,    -1,    73,    -1,    -1,    -1,
      77,    78,    79,    80,    -1,    82,    -1,    84,    -1,    86,
      -1,    -1,    89,    -1,    -1,    -1,    93,    94,    95,    96,
      -1,    98,    99,   100,   101,    -1,   103,   104,   105,   106,
      -1,    -1,    -1,   110,   111,   112,   113,   114,   115,     3,
       4,     5,     6,     7,    -1,   122,    -1,    -1,    12,    -1,
     127,    -1,   129,    -1,   131,   132,   133,   134,   135,    -1,
      -1,    -1,    26,    -1,    -1,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    -1,    42,    43,
      -1,    -1,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    -1,    61,    62,    63,
      -1,    -1,    -1,    67,    68,    69,    70,    71,    -1,    73,
      58,    59,    -1,    77,    78,    79,    80,    -1,    82,    -1,
      84,    -1,    86,    -1,    -1,    89,    -1,    -1,    -1,    93,
      94,    -1,    -1,    -1,    98,    99,   100,   101,    -1,   103,
     104,   105,    -1,    -1,    -1,    -1,   110,   111,   112,   113,
     114,   115,     3,     4,     5,     6,     7,    -1,   122,    -1,
      -1,    12,    -1,   127,    -1,   129,    -1,   131,   132,   133,
     134,   135,    -1,    -1,    -1,    26,    -1,    -1,    -1,    -1,
      -1,   129,    -1,   131,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    42,    43,    -1,    -1,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    -1,
      61,    62,    63,    -1,    -1,    -1,    67,    68,    69,    70,
      71,    -1,    73,    -1,    -1,    60,    77,    78,    79,    80,
      -1,    82,    -1,    84,    69,    86,    71,    72,    89,    74,
      75,    76,    93,    94,    -1,    -1,    -1,    98,    99,   100,
     101,    -1,   103,   104,   105,    -1,    -1,    -1,    -1,   110,
     111,   112,   113,   114,   115,     3,     4,     5,     6,     7,
      -1,   122,    -1,   108,    12,    -1,   127,    -1,   129,    -1,
     131,   132,   133,   134,   135,    -1,    -1,    -1,    26,   124,
     125,    -1,   127,   128,    -1,    -1,    -1,    -1,   133,    -1,
      -1,   136,    -1,    -1,    42,    43,    -1,    -1,    -1,    -1,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    -1,    61,    62,    63,    -1,    -1,    -1,    67,
      68,    69,    70,    71,    -1,    73,    -1,    -1,    60,    77,
      78,    79,    80,    -1,    82,    -1,    84,    69,    86,    71,
      72,    89,    74,    75,    76,    93,    94,    -1,    -1,    -1,
      98,    99,   100,   101,    -1,   103,   104,   105,    -1,    -1,
      -1,    -1,   110,   111,   112,   113,   114,   115,     3,     4,
       5,     6,     7,    -1,   122,    -1,   108,    12,    -1,   127,
      -1,   129,    -1,   131,   132,   133,   134,   135,    -1,    -1,
      -1,    26,   124,   125,    -1,   127,   128,    -1,    -1,    -1,
      -1,    -1,    -1,   135,   136,    -1,    -1,    42,    43,    -1,
      -1,    -1,    -1,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    -1,    61,    62,    63,    -1,
      -1,    -1,    67,    68,    69,    70,    71,    -1,    73,    -1,
      -1,    -1,    77,    78,    79,    80,    -1,    82,    -1,    84,
      -1,    86,    -1,    -1,    89,    -1,    -1,    -1,    93,    94,
      -1,    -1,    -1,    98,    99,   100,   101,    -1,   103,   104,
     105,    -1,    -1,    -1,    -1,   110,   111,   112,   113,   114,
     115,     3,     4,     5,     6,     7,    -1,   122,    -1,    -1,
      12,    -1,   127,    -1,   129,    -1,   131,   132,   133,   134,
     135,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      42,    43,    -1,    -1,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    -1,    61,
      62,    63,    -1,    -1,    -1,    67,    68,    69,    70,    71,
      -1,    73,    -1,    -1,    -1,    77,    78,    79,    80,    -1,
      82,    -1,    84,    -1,    86,    -1,    -1,    89,    -1,    -1,
      -1,    93,    94,    -1,    -1,    -1,    98,    99,   100,   101,
      -1,   103,   104,   105,    -1,    -1,    -1,    -1,   110,   111,
     112,   113,   114,   115,     3,     4,     5,     6,     7,    -1,
     122,    -1,    -1,    12,    -1,   127,    -1,   129,    -1,   131,
     132,   133,   134,   135,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,     3,     4,     5,     6,
       7,    -1,    -1,    42,    43,    12,    -1,    -1,    -1,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    -1,    61,    62,    -1,    -1,    -1,    -1,    67,    68,
      69,    70,    71,    -1,    -1,    42,    43,    -1,    77,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    -1,    61,    62,    -1,    -1,    -1,    -1,
      67,    68,    69,    70,    71,   104,   105,    -1,    -1,    -1,
      77,   110,   111,   112,   113,   114,   115,    -1,    -1,    -1,
      -1,    -1,    -1,   122,    -1,    -1,    -1,    -1,    -1,    -1,
     129,    -1,   131,   132,   133,   134,   135,   104,   105,    -1,
      -1,    -1,    -1,   110,   111,   112,   113,   114,   115,     3,
       4,     5,     6,     7,    -1,   122,    -1,    -1,    12,    -1,
      -1,    -1,   129,    -1,   131,   132,   133,   134,   135,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    42,    43,
      12,    -1,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    -1,    61,    62,    -1,
      -1,    -1,    -1,    67,    68,    69,    70,    71,    -1,    -1,
      42,    43,    -1,    77,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    -1,    61,
      62,    -1,    -1,    -1,    -1,    67,    68,    69,    70,    71,
     104,   105,    -1,    -1,    -1,    77,   110,   111,   112,   113,
     114,   115,    -1,    -1,    -1,    -1,    -1,    -1,   122,    -1,
      -1,    -1,    -1,    -1,    -1,   129,    -1,   131,   132,   133,
     134,   135,   104,   105,    -1,    -1,    -1,    -1,   110,   111,
     112,   113,   114,   115,     3,     4,     5,     6,     7,    -1,
     122,    -1,    -1,    12,    -1,    -1,    -1,   129,   130,    -1,
     132,   133,   134,   135,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    31,    -1,    -1,    -1,     3,     4,     5,     6,
       7,    -1,    -1,    42,    43,    12,    -1,    -1,    -1,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    -1,    61,    62,    31,    -1,    -1,    -1,    67,    68,
      69,    70,    71,    -1,    -1,    42,    43,    -1,    77,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    -1,    61,    62,    -1,    -1,    -1,    -1,
      67,    68,    69,    70,    71,   104,   105,    -1,    -1,    -1,
      77,   110,   111,   112,   113,   114,   115,    -1,    -1,    -1,
      -1,    -1,    -1,   122,    -1,    -1,    -1,    -1,    -1,    -1,
     129,    -1,    -1,   132,   133,   134,   135,   104,   105,    -1,
      -1,    -1,    -1,   110,   111,   112,   113,   114,   115,     3,
       4,     5,     6,     7,    -1,   122,    -1,    -1,    12,    -1,
      -1,    -1,   129,    -1,    -1,   132,   133,   134,   135,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    31,    -1,    -1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    42,    43,
      12,    -1,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    -1,    61,    62,    31,
      -1,    -1,    -1,    67,    68,    69,    70,    71,    -1,    -1,
      42,    43,    -1,    77,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    -1,    61,
      62,    -1,    -1,    -1,    -1,    67,    68,    69,    70,    71,
     104,   105,    -1,    -1,    -1,    77,   110,   111,   112,   113,
     114,   115,    -1,    -1,    -1,    -1,    -1,    -1,   122,    -1,
      -1,    -1,    -1,    -1,    -1,   129,    -1,    -1,   132,   133,
     134,   135,   104,   105,    -1,    -1,    -1,    -1,   110,   111,
     112,   113,   114,   115,     3,     4,     5,     6,     7,    -1,
     122,    -1,    -1,    12,    -1,    -1,    -1,   129,    -1,    -1,
     132,   133,   134,   135,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    31,    -1,    -1,    -1,     3,     4,     5,     6,
       7,    -1,    -1,    42,    43,    12,    -1,    -1,    -1,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    -1,    61,    62,    31,    -1,    -1,    -1,    67,    68,
      69,    70,    71,    -1,    -1,    42,    43,    -1,    77,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    -1,    61,    62,    -1,    -1,    -1,    -1,
      67,    68,    69,    70,    71,   104,   105,    -1,    -1,    -1,
      77,   110,   111,   112,   113,   114,   115,    -1,    -1,    -1,
      -1,    -1,    -1,   122,    -1,    -1,    -1,    -1,    -1,    -1,
     129,    -1,    -1,   132,   133,   134,   135,   104,   105,    -1,
      -1,    -1,    -1,   110,   111,   112,   113,   114,   115,     3,
       4,     5,     6,     7,    -1,   122,    -1,    -1,    12,    -1,
      -1,    -1,   129,    -1,    -1,   132,   133,   134,   135,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    31,    -1,    -1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    42,    43,
      12,    -1,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    -1,    61,    62,    -1,
      -1,    -1,    -1,    67,    68,    69,    70,    71,    -1,    -1,
      42,    43,    -1,    77,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    -1,    61,
      62,    -1,    -1,    -1,    -1,    67,    68,    69,    70,    71,
     104,   105,    -1,    -1,    -1,    77,   110,   111,   112,   113,
     114,   115,    -1,    -1,    -1,    -1,    -1,    -1,   122,    -1,
      -1,    -1,    -1,    -1,    -1,   129,    -1,    -1,   132,   133,
     134,   135,   104,   105,    -1,    -1,    -1,    -1,   110,   111,
     112,   113,   114,   115,     3,     4,     5,     6,     7,    -1,
     122,    -1,    -1,    12,    -1,    -1,    -1,   129,    -1,    -1,
     132,   133,   134,   135,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    -1,    42,    43,    -1,    -1,    -1,    -1,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    -1,    61,    62,     9,    10,    11,    -1,    67,    68,
      69,    70,    71,    -1,    -1,    -1,    -1,    -1,    77,    -1,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    -1,    -1,   104,   105,    -1,    -1,    -1,
      -1,   110,   111,   112,   113,   114,   115,    -1,    -1,    -1,
      -1,    -1,    -1,   122,     9,    10,    11,    -1,    -1,    -1,
     129,    -1,    -1,   132,   133,   134,   135,    -1,    -1,    -1,
      25,    -1,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    -1,    -1,    -1,    -1,    -1,    -1,     9,
      10,    11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    25,   131,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,     9,
      10,    11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    25,   131,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    -1,    -1,
      -1,    -1,    -1,    -1,     9,    10,    11,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      25,   131,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    -1,     9,    10,    11,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      25,   131,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    -1,    -1,    -1,    -1,    -1,     9,    10,
      11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    25,   130,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    -1,    -1,    -1,    -1,     9,    10,
      11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    25,   130,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    -1,    -1,    -1,
      -1,    -1,     9,    10,    11,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,   130,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    -1,    -1,    -1,
      -1,    -1,     9,    10,    11,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,   130,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    -1,    -1,    -1,    -1,    -1,     9,    10,    11,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    25,   130,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    -1,    -1,    -1,    -1,    -1,     9,    10,    11,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    25,   130,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    -1,    -1,    -1,    -1,    -1,
       9,    10,    11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    25,   130,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       9,    10,    11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    25,   130,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    -1,
      -1,    -1,    -1,    -1,     9,    10,    11,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      25,   130,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     9,    10,    11,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,   128,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    -1,    -1,    -1,    -1,    -1,     9,    10,    11,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    25,   128,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     9,    10,    11,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      25,   128,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    -1,    -1,    -1,    -1,    -1,     9,    10,
      11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    25,   128,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     9,    10,
      11,    -1,    -1,   128,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    58,    59,    25,    -1,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,     9,    10,    11,
      -1,    -1,    88,    -1,    -1,    -1,    -1,   128,    -1,    -1,
      -1,    -1,    -1,    25,    -1,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   129,     9,    10,    11,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   109,    -1,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,     9,    10,    11,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   109,    -1,    25,
      -1,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    10,    11,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    25,    -1,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    25,    -1,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   138,   139,     0,   140,     3,     4,     5,     6,     7,
      12,    42,    43,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    61,    62,    63,    67,    68,
      69,    70,    71,    73,    77,    78,    79,    80,    82,    84,
      86,    89,    93,    94,    95,    96,    98,    99,   100,   101,
     103,   104,   105,   106,   110,   111,   112,   113,   114,   115,
     122,   127,   129,   131,   132,   133,   134,   135,   141,   145,
     146,   167,   168,   211,   222,   229,   231,   235,   236,   238,
     239,   241,   242,   243,   251,   261,   235,   235,   129,   235,
     235,   235,   235,   235,   235,   235,   221,   235,   235,   235,
     235,   235,   235,   235,   238,   239,   238,    69,   226,   236,
     239,   129,   227,   129,   126,   129,   207,   235,   153,   129,
     129,   129,   163,   129,   131,   235,   131,   235,   171,   169,
     131,   211,   235,   239,    77,   129,   166,    71,   132,   196,
     197,    71,   198,   129,   129,   129,    69,   129,   129,   257,
     142,   235,   127,   257,   257,   257,     9,    10,    11,    25,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,   131,    58,    59,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,   129,   108,    60,   127,
     132,   242,   235,   235,   214,   130,   235,   235,   226,   223,
       8,   131,   145,   151,   208,   209,   235,   211,   237,   239,
     129,   235,   131,   131,    31,   205,   205,   131,   131,    77,
     131,   127,   236,     8,   131,    13,     8,   131,   164,   165,
     239,   239,   262,   239,   107,   173,   212,    31,   235,   255,
     256,    60,    69,    71,    72,    74,    75,    76,   108,   123,
     124,   125,   127,   128,   136,   258,   128,   143,   130,   235,
     133,   134,   135,   217,   235,   218,   219,   215,   216,   235,
     235,   235,   235,   235,   235,   235,   235,   235,   235,   235,
     235,   235,   235,   235,   235,   235,   235,   235,    31,   235,
     235,   235,   235,   235,   235,   235,   235,   235,   235,   235,
     235,   224,   240,   235,   244,   235,   130,   129,   228,   130,
     130,   129,    31,   194,   195,   211,   239,   235,    80,   235,
     131,     8,    88,    88,    69,   179,   130,    69,    69,   130,
     235,   197,    42,    43,    69,   111,   229,   230,    71,     8,
     130,     8,   130,   130,    69,   127,   110,   239,   252,   253,
     237,   239,   109,   130,     8,   233,    60,   108,    70,   235,
     239,   144,   145,   167,   128,   235,   235,   235,   235,   235,
      61,    69,   222,   237,   239,   194,    69,   127,   241,   245,
     247,   249,   250,   136,   128,   194,    26,   147,   225,   237,
     130,     8,   129,   130,   155,   210,   161,   159,    13,     8,
     130,   158,   172,   170,   128,   230,   230,   129,    13,   165,
     131,   263,   174,   199,   129,     8,   130,    31,   235,    31,
     235,   259,    69,    60,   128,   128,    26,   226,   130,   235,
     248,   108,    60,   127,   130,   149,   145,   194,    31,   211,
     239,   154,   152,   208,   235,   237,   237,   230,    69,    26,
     145,   178,    26,   127,   180,    31,    71,    97,   192,   193,
     129,   230,   232,   234,   230,   239,   127,    95,    96,   102,
     128,   200,   254,   253,    13,   237,   237,   109,    69,    71,
      72,   260,   235,   220,   213,   128,   246,   244,   235,   142,
     148,   130,   237,   235,    26,   145,   185,   131,   109,   175,
     175,    13,   142,   131,   181,   131,   181,    71,    13,    71,
     129,     8,   192,   109,   130,     8,   233,   199,   203,   201,
      71,   206,   252,   235,    31,   235,   136,   136,   235,   228,
     247,   136,   128,   150,   186,   130,   142,   156,   237,   130,
     130,   230,    87,   181,    90,    91,    92,   181,   128,   230,
     142,    31,    71,    97,   130,   230,   230,   128,   205,   205,
      13,     8,   131,   130,   237,   128,   188,    64,    65,   190,
     131,    81,   208,   162,   160,   131,    90,   131,   235,    26,
     131,   184,   128,   130,    71,    13,    71,   127,   109,    69,
      69,   230,    71,    64,    65,   191,   129,   145,   131,   130,
      26,   145,   177,   177,   131,   184,   183,   131,   230,   142,
     230,   204,   202,    13,   129,    26,    66,   235,   157,   142,
     182,   142,   128,   192,   129,   230,   235,   142,   131,   130,
      26,   145,   176,    85,   142,   129,   192,   130,   187,   142,
     131,   142,   130,    26,   145,    83,   130,   127,   189,   131,
     131,   142,   142,   128
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
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval)
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
  /* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;

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
        case 3:

    { zend_do_extended_info(TSRMLS_C); }
    break;

  case 4:

    { HANDLE_INTERACTIVE(); }
    break;

  case 7:

    { zend_do_early_binding(TSRMLS_C); }
    break;

  case 8:

    { zend_do_extended_info(TSRMLS_C); }
    break;

  case 9:

    { HANDLE_INTERACTIVE(); }
    break;

  case 13:

    { zend_do_ticks(TSRMLS_C); }
    break;

  case 15:

    { zend_do_if_cond(&(yyvsp[(3) - (4)]), &(yyvsp[(4) - (4)]) TSRMLS_CC); }
    break;

  case 16:

    { zend_do_if_after_statement(&(yyvsp[(4) - (6)]), 1 TSRMLS_CC); }
    break;

  case 17:

    { zend_do_if_end(TSRMLS_C); }
    break;

  case 18:

    { zend_do_if_cond(&(yyvsp[(3) - (5)]), &(yyvsp[(4) - (5)]) TSRMLS_CC); }
    break;

  case 19:

    { zend_do_if_after_statement(&(yyvsp[(4) - (7)]), 1 TSRMLS_CC); }
    break;

  case 20:

    { zend_do_if_end(TSRMLS_C); }
    break;

  case 21:

    { (yyvsp[(1) - (2)]).u.opline_num = get_next_op_number(CG(active_op_array));  }
    break;

  case 22:

    { zend_do_while_cond(&(yyvsp[(4) - (5)]), &(yyvsp[(5) - (5)]) TSRMLS_CC); }
    break;

  case 23:

    { zend_do_while_end(&(yyvsp[(1) - (7)]), &(yyvsp[(5) - (7)]) TSRMLS_CC); }
    break;

  case 24:

    { (yyvsp[(1) - (1)]).u.opline_num = get_next_op_number(CG(active_op_array));  zend_do_do_while_begin(TSRMLS_C); }
    break;

  case 25:

    { (yyvsp[(5) - (5)]).u.opline_num = get_next_op_number(CG(active_op_array)); }
    break;

  case 26:

    { zend_do_do_while_end(&(yyvsp[(1) - (9)]), &(yyvsp[(5) - (9)]), &(yyvsp[(7) - (9)]) TSRMLS_CC); }
    break;

  case 27:

    { zend_do_free(&(yyvsp[(3) - (4)]) TSRMLS_CC); (yyvsp[(4) - (4)]).u.opline_num = get_next_op_number(CG(active_op_array)); }
    break;

  case 28:

    { zend_do_extended_info(TSRMLS_C); zend_do_for_cond(&(yyvsp[(6) - (7)]), &(yyvsp[(7) - (7)]) TSRMLS_CC); }
    break;

  case 29:

    { zend_do_free(&(yyvsp[(9) - (10)]) TSRMLS_CC); zend_do_for_before_statement(&(yyvsp[(4) - (10)]), &(yyvsp[(7) - (10)]) TSRMLS_CC); }
    break;

  case 30:

    { zend_do_for_end(&(yyvsp[(7) - (12)]) TSRMLS_CC); }
    break;

  case 31:

    { zend_do_switch_cond(&(yyvsp[(3) - (4)]) TSRMLS_CC); }
    break;

  case 32:

    { zend_do_switch_end(&(yyvsp[(6) - (6)]) TSRMLS_CC); }
    break;

  case 33:

    { zend_do_brk_cont(ZEND_BRK, NULL TSRMLS_CC); }
    break;

  case 34:

    { zend_do_brk_cont(ZEND_BRK, &(yyvsp[(2) - (3)]) TSRMLS_CC); }
    break;

  case 35:

    { zend_do_brk_cont(ZEND_CONT, NULL TSRMLS_CC); }
    break;

  case 36:

    { zend_do_brk_cont(ZEND_CONT, &(yyvsp[(2) - (3)]) TSRMLS_CC); }
    break;

  case 37:

    { zend_do_return(NULL, 0 TSRMLS_CC); }
    break;

  case 38:

    { zend_do_return(&(yyvsp[(2) - (3)]), 0 TSRMLS_CC); }
    break;

  case 39:

    { zend_do_return(&(yyvsp[(2) - (3)]), 1 TSRMLS_CC); }
    break;

  case 43:

    { zend_do_echo(&(yyvsp[(1) - (1)]) TSRMLS_CC); }
    break;

  case 44:

    { zend_do_free(&(yyvsp[(1) - (2)]) TSRMLS_CC); }
    break;

  case 45:

    { zend_error(E_COMPILE_ERROR,"use: Not yet supported. Please use include_once() or require_once()");  zval_dtor(&(yyvsp[(2) - (3)]).u.constant); }
    break;

  case 47:

    { zend_do_foreach_begin(&(yyvsp[(1) - (4)]), &(yyvsp[(3) - (4)]), &(yyvsp[(2) - (4)]), &(yyvsp[(4) - (4)]), 1 TSRMLS_CC); }
    break;

  case 48:

    { zend_do_foreach_cont(&(yyvsp[(6) - (8)]), &(yyvsp[(7) - (8)]), &(yyvsp[(4) - (8)]), &(yyvsp[(1) - (8)]) TSRMLS_CC); }
    break;

  case 49:

    { zend_do_foreach_end(&(yyvsp[(1) - (10)]), &(yyvsp[(2) - (10)]) TSRMLS_CC); }
    break;

  case 50:

    { zend_do_foreach_begin(&(yyvsp[(1) - (4)]), &(yyvsp[(3) - (4)]), &(yyvsp[(2) - (4)]), &(yyvsp[(4) - (4)]), 0 TSRMLS_CC); }
    break;

  case 51:

    { zend_do_foreach_cont(&(yyvsp[(6) - (8)]), &(yyvsp[(7) - (8)]), &(yyvsp[(4) - (8)]), &(yyvsp[(1) - (8)]) TSRMLS_CC); }
    break;

  case 52:

    { zend_do_foreach_end(&(yyvsp[(1) - (10)]), &(yyvsp[(2) - (10)]) TSRMLS_CC); }
    break;

  case 53:

    { (yyvsp[(1) - (1)]).u.opline_num = get_next_op_number(CG(active_op_array)); zend_do_declare_begin(TSRMLS_C); }
    break;

  case 54:

    {zend_do_declare_end(&(yyvsp[(1) - (6)]) TSRMLS_CC); }
    break;

  case 58:

    { zend_do_end_variable_parse(BP_VAR_UNSET, 0 TSRMLS_CC); zend_do_unset(&(yyvsp[(1) - (1)]) TSRMLS_CC); }
    break;

  case 59:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 60:

    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 61:

    { zend_do_ticks(TSRMLS_C); }
    break;

  case 62:

    { (yyvsp[(1) - (1)]).u.opline_num = CG(zend_lineno); }
    break;

  case 63:

    { zend_do_begin_function_declaration(&(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]), 0, (yyvsp[(3) - (4)]).op_type TSRMLS_CC); }
    break;

  case 64:

    { zend_do_end_function_declaration(&(yyvsp[(1) - (11)]) TSRMLS_CC); }
    break;

  case 65:

    { (yyvsp[(1) - (1)]).u.opline_num = CG(zend_lineno); }
    break;

  case 66:

    { zend_do_begin_function_declaration(&(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]), 0, (yyvsp[(3) - (4)]).op_type TSRMLS_CC); }
    break;

  case 67:

    { zend_do_end_function_declaration(&(yyvsp[(1) - (10)]) TSRMLS_CC); }
    break;

  case 68:

    { zend_do_begin_class_declaration(&(yyvsp[(2) - (2)]), NULL TSRMLS_CC); }
    break;

  case 69:

    { zend_do_end_class_declaration(TSRMLS_C); }
    break;

  case 70:

    { zend_do_begin_class_declaration(&(yyvsp[(2) - (4)]), &(yyvsp[(4) - (4)]) TSRMLS_CC); }
    break;

  case 71:

    { zend_do_end_class_declaration(TSRMLS_C); }
    break;

  case 72:

    { (yyval).op_type = IS_UNUSED; }
    break;

  case 73:

    { (yyval) = (yyvsp[(2) - (2)]); }
    break;

  case 80:

    { zend_do_declare_stmt(&(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 81:

    { zend_do_declare_stmt(&(yyvsp[(3) - (5)]), &(yyvsp[(5) - (5)]) TSRMLS_CC); }
    break;

  case 82:

    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 83:

    { (yyval) = (yyvsp[(3) - (4)]); }
    break;

  case 84:

    { (yyval) = (yyvsp[(2) - (4)]); }
    break;

  case 85:

    { (yyval) = (yyvsp[(3) - (5)]); }
    break;

  case 86:

    { (yyval).op_type = IS_UNUSED; }
    break;

  case 87:

    { zend_do_extended_info(TSRMLS_C);  zend_do_case_before_statement(&(yyvsp[(1) - (4)]), &(yyvsp[(2) - (4)]), &(yyvsp[(3) - (4)]) TSRMLS_CC); }
    break;

  case 88:

    { zend_do_case_after_statement(&(yyval), &(yyvsp[(2) - (6)]) TSRMLS_CC); (yyval).op_type = IS_CONST; }
    break;

  case 89:

    { zend_do_extended_info(TSRMLS_C);  zend_do_default_before_statement(&(yyvsp[(1) - (3)]), &(yyvsp[(2) - (3)]) TSRMLS_CC); }
    break;

  case 90:

    { zend_do_case_after_statement(&(yyval), &(yyvsp[(2) - (5)]) TSRMLS_CC); (yyval).op_type = IS_CONST; }
    break;

  case 96:

    { zend_do_if_cond(&(yyvsp[(4) - (5)]), &(yyvsp[(5) - (5)]) TSRMLS_CC); }
    break;

  case 97:

    { zend_do_if_after_statement(&(yyvsp[(5) - (7)]), 0 TSRMLS_CC); }
    break;

  case 99:

    { zend_do_if_cond(&(yyvsp[(4) - (6)]), &(yyvsp[(5) - (6)]) TSRMLS_CC); }
    break;

  case 100:

    { zend_do_if_after_statement(&(yyvsp[(5) - (8)]), 0 TSRMLS_CC); }
    break;

  case 107:

    { znode tmp;  fetch_simple_variable(&tmp, &(yyvsp[(1) - (1)]), 0 TSRMLS_CC); (yyval).op_type = IS_CONST; (yyval).u.constant.value.lval=1; (yyval).u.constant.type=IS_LONG; INIT_PZVAL(&(yyval).u.constant); zend_do_receive_arg(ZEND_RECV, &tmp, &(yyval), NULL, BYREF_NONE TSRMLS_CC); }
    break;

  case 108:

    { znode tmp;  fetch_simple_variable(&tmp, &(yyvsp[(2) - (2)]), 0 TSRMLS_CC); (yyval).op_type = IS_CONST; (yyval).u.constant.value.lval=1; (yyval).u.constant.type=IS_LONG; INIT_PZVAL(&(yyval).u.constant); zend_do_receive_arg(ZEND_RECV, &tmp, &(yyval), NULL, BYREF_FORCE TSRMLS_CC); }
    break;

  case 109:

    { znode tmp;  fetch_simple_variable(&tmp, &(yyvsp[(2) - (2)]), 0 TSRMLS_CC); (yyval).op_type = IS_CONST; (yyval).u.constant.value.lval=1; (yyval).u.constant.type=IS_LONG; INIT_PZVAL(&(yyval).u.constant); zend_do_receive_arg(ZEND_RECV, &tmp, &(yyval), NULL, BYREF_NONE TSRMLS_CC); }
    break;

  case 110:

    { znode tmp;  fetch_simple_variable(&tmp, &(yyvsp[(1) - (3)]), 0 TSRMLS_CC); (yyval).op_type = IS_CONST; (yyval).u.constant.value.lval=1; (yyval).u.constant.type=IS_LONG; INIT_PZVAL(&(yyval).u.constant); zend_do_receive_arg(ZEND_RECV_INIT, &tmp, &(yyval), &(yyvsp[(3) - (3)]), BYREF_NONE TSRMLS_CC); }
    break;

  case 111:

    { znode tmp;  fetch_simple_variable(&tmp, &(yyvsp[(3) - (3)]), 0 TSRMLS_CC); (yyval)=(yyvsp[(1) - (3)]); (yyval).u.constant.value.lval++; zend_do_receive_arg(ZEND_RECV, &tmp, &(yyval), NULL, BYREF_NONE TSRMLS_CC); }
    break;

  case 112:

    { znode tmp;  fetch_simple_variable(&tmp, &(yyvsp[(4) - (4)]), 0 TSRMLS_CC); (yyval)=(yyvsp[(1) - (4)]); (yyval).u.constant.value.lval++; zend_do_receive_arg(ZEND_RECV, &tmp, &(yyval), NULL, BYREF_FORCE TSRMLS_CC); }
    break;

  case 113:

    { znode tmp;  fetch_simple_variable(&tmp, &(yyvsp[(4) - (4)]), 0 TSRMLS_CC); (yyval)=(yyvsp[(1) - (4)]); (yyval).u.constant.value.lval++; zend_do_receive_arg(ZEND_RECV, &tmp, &(yyval), NULL, BYREF_NONE TSRMLS_CC); }
    break;

  case 114:

    { znode tmp;  fetch_simple_variable(&tmp, &(yyvsp[(3) - (5)]), 0 TSRMLS_CC); (yyval)=(yyvsp[(1) - (5)]); (yyval).u.constant.value.lval++; zend_do_receive_arg(ZEND_RECV_INIT, &tmp, &(yyval), &(yyvsp[(5) - (5)]), BYREF_NONE TSRMLS_CC); }
    break;

  case 115:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 116:

    { (yyval).u.constant.value.lval = 0; }
    break;

  case 117:

    { (yyval).u.constant.value.lval = 1;  zend_do_pass_param(&(yyvsp[(1) - (1)]), ZEND_SEND_VAL, (yyval).u.constant.value.lval TSRMLS_CC); }
    break;

  case 118:

    { (yyval).u.constant.value.lval = 1;  zend_do_pass_param(&(yyvsp[(1) - (1)]), ZEND_SEND_VAR, (yyval).u.constant.value.lval TSRMLS_CC); }
    break;

  case 119:

    { (yyval).u.constant.value.lval = 1;  zend_do_pass_param(&(yyvsp[(2) - (2)]), ZEND_SEND_REF, (yyval).u.constant.value.lval TSRMLS_CC); }
    break;

  case 120:

    { (yyval).u.constant.value.lval=(yyvsp[(1) - (3)]).u.constant.value.lval+1;  zend_do_pass_param(&(yyvsp[(3) - (3)]), ZEND_SEND_VAL, (yyval).u.constant.value.lval TSRMLS_CC); }
    break;

  case 121:

    { (yyval).u.constant.value.lval=(yyvsp[(1) - (3)]).u.constant.value.lval+1;  zend_do_pass_param(&(yyvsp[(3) - (3)]), ZEND_SEND_VAR, (yyval).u.constant.value.lval TSRMLS_CC); }
    break;

  case 122:

    { (yyval).u.constant.value.lval=(yyvsp[(1) - (4)]).u.constant.value.lval+1;  zend_do_pass_param(&(yyvsp[(4) - (4)]), ZEND_SEND_REF, (yyval).u.constant.value.lval TSRMLS_CC); }
    break;

  case 123:

    { zend_do_fetch_global_or_static_variable(&(yyvsp[(3) - (3)]), NULL, ZEND_FETCH_GLOBAL TSRMLS_CC); }
    break;

  case 124:

    { zend_do_fetch_global_or_static_variable(&(yyvsp[(1) - (1)]), NULL, ZEND_FETCH_GLOBAL TSRMLS_CC); }
    break;

  case 125:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 126:

    { (yyval) = (yyvsp[(2) - (2)]); }
    break;

  case 127:

    { (yyval) = (yyvsp[(3) - (4)]); }
    break;

  case 128:

    { zend_do_fetch_global_or_static_variable(&(yyvsp[(3) - (3)]), NULL, ZEND_FETCH_STATIC TSRMLS_CC); }
    break;

  case 129:

    { zend_do_fetch_global_or_static_variable(&(yyvsp[(3) - (5)]), &(yyvsp[(5) - (5)]), ZEND_FETCH_STATIC TSRMLS_CC); }
    break;

  case 130:

    { zend_do_fetch_global_or_static_variable(&(yyvsp[(1) - (1)]), NULL, ZEND_FETCH_STATIC TSRMLS_CC); }
    break;

  case 131:

    { zend_do_fetch_global_or_static_variable(&(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]), ZEND_FETCH_STATIC TSRMLS_CC); }
    break;

  case 135:

    { (yyvsp[(1) - (1)]).u.opline_num = CG(zend_lineno); }
    break;

  case 136:

    { zend_do_begin_function_declaration(&(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]), 1, (yyvsp[(3) - (4)]).op_type TSRMLS_CC); }
    break;

  case 137:

    { zend_do_end_function_declaration(&(yyvsp[(1) - (11)]) TSRMLS_CC); }
    break;

  case 138:

    { (yyvsp[(1) - (1)]).u.opline_num = CG(zend_lineno); }
    break;

  case 139:

    { zend_do_begin_function_declaration(&(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]), 1, (yyvsp[(3) - (4)]).op_type TSRMLS_CC); }
    break;

  case 140:

    { zend_do_end_function_declaration(&(yyvsp[(1) - (10)]) TSRMLS_CC); }
    break;

  case 141:

    { (yyval).op_type = ZEND_RETURN_VAL; }
    break;

  case 142:

    { (yyval).op_type = ZEND_RETURN_REF; }
    break;

  case 143:

    { zend_do_declare_property(&(yyvsp[(3) - (3)]), NULL TSRMLS_CC); }
    break;

  case 144:

    { zend_do_declare_property(&(yyvsp[(3) - (5)]), &(yyvsp[(5) - (5)]) TSRMLS_CC); }
    break;

  case 145:

    { zend_do_declare_property(&(yyvsp[(1) - (1)]), NULL TSRMLS_CC); }
    break;

  case 146:

    { zend_do_declare_property(&(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 148:

    { zend_do_echo(&(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 149:

    { zend_do_echo(&(yyvsp[(1) - (1)]) TSRMLS_CC); }
    break;

  case 150:

    { (yyval).op_type = IS_CONST;  (yyval).u.constant.type = IS_BOOL;  (yyval).u.constant.value.lval = 1; }
    break;

  case 151:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 152:

    { zend_do_free(&(yyvsp[(1) - (2)]) TSRMLS_CC); }
    break;

  case 153:

    { (yyval) = (yyvsp[(4) - (4)]); }
    break;

  case 154:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 155:

    { zend_do_list_init(TSRMLS_C); }
    break;

  case 156:

    { zend_do_list_end(&(yyval), &(yyvsp[(7) - (7)]) TSRMLS_CC); }
    break;

  case 157:

    { zend_do_end_variable_parse(BP_VAR_W, 0 TSRMLS_CC); zend_do_assign(&(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 158:

    { zend_do_end_variable_parse(BP_VAR_W, 0 TSRMLS_CC); zend_do_assign_ref(&(yyval), &(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]) TSRMLS_CC); }
    break;

  case 159:

    { zend_do_end_variable_parse(BP_VAR_W, 0 TSRMLS_CC); (yyvsp[(4) - (4)]).u.EA.type = ZEND_PARSED_FCALL; zend_do_assign_ref(&(yyval), &(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]) TSRMLS_CC); }
    break;

  case 160:

    { zend_do_extended_fcall_begin(TSRMLS_C); zend_do_begin_new_object(&(yyvsp[(4) - (5)]), &(yyvsp[(5) - (5)]) TSRMLS_CC); }
    break;

  case 161:

    { zend_do_end_new_object(&(yyvsp[(3) - (7)]), &(yyvsp[(5) - (7)]), &(yyvsp[(4) - (7)]), &(yyvsp[(7) - (7)]) TSRMLS_CC); zend_do_extended_fcall_end(TSRMLS_C); zend_do_end_variable_parse(BP_VAR_W, 0 TSRMLS_CC); zend_do_assign_ref(&(yyval), &(yyvsp[(1) - (7)]), &(yyvsp[(3) - (7)]) TSRMLS_CC); }
    break;

  case 162:

    { zend_do_extended_fcall_begin(TSRMLS_C); zend_do_begin_new_object(&(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 163:

    { zend_do_end_new_object(&(yyval), &(yyvsp[(2) - (4)]), &(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]) TSRMLS_CC); zend_do_extended_fcall_end(TSRMLS_C);}
    break;

  case 164:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_ADD, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 165:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_SUB, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 166:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_MUL, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 167:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_DIV, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 168:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_CONCAT, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 169:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_MOD, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 170:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_BW_AND, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 171:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_BW_OR, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 172:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_BW_XOR, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 173:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_SL, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 174:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); zend_do_binary_assign_op(ZEND_ASSIGN_SR, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 175:

    { zend_do_post_incdec(&(yyval), &(yyvsp[(1) - (2)]), ZEND_POST_INC TSRMLS_CC); }
    break;

  case 176:

    { zend_do_pre_incdec(&(yyval), &(yyvsp[(2) - (2)]), ZEND_PRE_INC TSRMLS_CC); }
    break;

  case 177:

    { zend_do_post_incdec(&(yyval), &(yyvsp[(1) - (2)]), ZEND_POST_DEC TSRMLS_CC); }
    break;

  case 178:

    { zend_do_pre_incdec(&(yyval), &(yyvsp[(2) - (2)]), ZEND_PRE_DEC TSRMLS_CC); }
    break;

  case 179:

    { zend_do_boolean_or_begin(&(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 180:

    { zend_do_boolean_or_end(&(yyval), &(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]), &(yyvsp[(2) - (4)]) TSRMLS_CC); }
    break;

  case 181:

    { zend_do_boolean_and_begin(&(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 182:

    { zend_do_boolean_and_end(&(yyval), &(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]), &(yyvsp[(2) - (4)]) TSRMLS_CC); }
    break;

  case 183:

    { zend_do_boolean_or_begin(&(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 184:

    { zend_do_boolean_or_end(&(yyval), &(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]), &(yyvsp[(2) - (4)]) TSRMLS_CC); }
    break;

  case 185:

    { zend_do_boolean_and_begin(&(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 186:

    { zend_do_boolean_and_end(&(yyval), &(yyvsp[(1) - (4)]), &(yyvsp[(4) - (4)]), &(yyvsp[(2) - (4)]) TSRMLS_CC); }
    break;

  case 187:

    { zend_do_binary_op(ZEND_BOOL_XOR, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 188:

    { zend_do_binary_op(ZEND_BW_OR, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 189:

    { zend_do_binary_op(ZEND_BW_AND, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 190:

    { zend_do_binary_op(ZEND_BW_XOR, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 191:

    { zend_do_binary_op(ZEND_CONCAT, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 192:

    { zend_do_binary_op(ZEND_ADD, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 193:

    { zend_do_binary_op(ZEND_SUB, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 194:

    { zend_do_binary_op(ZEND_MUL, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 195:

    { zend_do_binary_op(ZEND_DIV, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 196:

    { zend_do_binary_op(ZEND_MOD, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 197:

    { zend_do_binary_op(ZEND_SL, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 198:

    { zend_do_binary_op(ZEND_SR, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 199:

    { (yyvsp[(1) - (2)]).u.constant.value.lval=0; (yyvsp[(1) - (2)]).u.constant.type=IS_LONG; (yyvsp[(1) - (2)]).op_type = IS_CONST; INIT_PZVAL(&(yyvsp[(1) - (2)]).u.constant); zend_do_binary_op(ZEND_ADD, &(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 200:

    { (yyvsp[(1) - (2)]).u.constant.value.lval=0; (yyvsp[(1) - (2)]).u.constant.type=IS_LONG; (yyvsp[(1) - (2)]).op_type = IS_CONST; INIT_PZVAL(&(yyvsp[(1) - (2)]).u.constant); zend_do_binary_op(ZEND_SUB, &(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 201:

    { zend_do_unary_op(ZEND_BOOL_NOT, &(yyval), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 202:

    { zend_do_unary_op(ZEND_BW_NOT, &(yyval), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 203:

    { zend_do_binary_op(ZEND_IS_IDENTICAL, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 204:

    { zend_do_binary_op(ZEND_IS_NOT_IDENTICAL, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 205:

    { zend_do_binary_op(ZEND_IS_EQUAL, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 206:

    { zend_do_binary_op(ZEND_IS_NOT_EQUAL, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 207:

    { zend_do_binary_op(ZEND_IS_SMALLER, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 208:

    { zend_do_binary_op(ZEND_IS_SMALLER_OR_EQUAL, &(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 209:

    { zend_do_binary_op(ZEND_IS_SMALLER, &(yyval), &(yyvsp[(3) - (3)]), &(yyvsp[(1) - (3)]) TSRMLS_CC); }
    break;

  case 210:

    { zend_do_binary_op(ZEND_IS_SMALLER_OR_EQUAL, &(yyval), &(yyvsp[(3) - (3)]), &(yyvsp[(1) - (3)]) TSRMLS_CC); }
    break;

  case 211:

    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 212:

    { zend_do_begin_qm_op(&(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 213:

    { zend_do_qm_true(&(yyvsp[(4) - (5)]), &(yyvsp[(2) - (5)]), &(yyvsp[(5) - (5)]) TSRMLS_CC); }
    break;

  case 214:

    { zend_do_qm_false(&(yyval), &(yyvsp[(7) - (7)]), &(yyvsp[(2) - (7)]), &(yyvsp[(5) - (7)]) TSRMLS_CC); }
    break;

  case 215:

    { (yyval) = (yyvsp[(1) - (1)]); (yyval).u.EA.type = ZEND_PARSED_FCALL; }
    break;

  case 216:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 217:

    { zend_do_cast(&(yyval), &(yyvsp[(2) - (2)]), IS_LONG TSRMLS_CC); }
    break;

  case 218:

    { zend_do_cast(&(yyval), &(yyvsp[(2) - (2)]), IS_DOUBLE TSRMLS_CC); }
    break;

  case 219:

    { zend_do_cast(&(yyval), &(yyvsp[(2) - (2)]), IS_STRING TSRMLS_CC); }
    break;

  case 220:

    { zend_do_cast(&(yyval), &(yyvsp[(2) - (2)]), IS_ARRAY TSRMLS_CC); }
    break;

  case 221:

    { zend_do_cast(&(yyval), &(yyvsp[(2) - (2)]), IS_OBJECT TSRMLS_CC); }
    break;

  case 222:

    { zend_do_cast(&(yyval), &(yyvsp[(2) - (2)]), IS_BOOL TSRMLS_CC); }
    break;

  case 223:

    { zend_do_cast(&(yyval), &(yyvsp[(2) - (2)]), IS_NULL TSRMLS_CC); }
    break;

  case 224:

    { zend_do_exit(&(yyval), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 225:

    { zend_do_begin_silence(&(yyvsp[(1) - (1)]) TSRMLS_CC); }
    break;

  case 226:

    { zend_do_end_silence(&(yyvsp[(1) - (3)]) TSRMLS_CC); (yyval) = (yyvsp[(3) - (3)]); }
    break;

  case 227:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 228:

    { (yyval) = (yyvsp[(3) - (4)]); }
    break;

  case 229:

    { zend_do_shell_exec(&(yyval), &(yyvsp[(2) - (3)]) TSRMLS_CC); }
    break;

  case 230:

    { zend_do_print(&(yyval), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 231:

    { (yyvsp[(2) - (2)]).u.opline_num = zend_do_begin_function_call(&(yyvsp[(1) - (2)]) TSRMLS_CC); }
    break;

  case 232:

    { zend_do_end_function_call(&(yyvsp[(1) - (5)]), &(yyval), &(yyvsp[(4) - (5)]), 0, (yyvsp[(2) - (5)]).u.opline_num TSRMLS_CC); zend_do_extended_fcall_end(TSRMLS_C); }
    break;

  case 233:

    { zend_do_begin_dynamic_function_call(&(yyvsp[(1) - (2)]) TSRMLS_CC); }
    break;

  case 234:

    { zend_do_end_function_call(&(yyvsp[(1) - (5)]), &(yyval), &(yyvsp[(4) - (5)]), 0, 1 TSRMLS_CC); zend_do_extended_fcall_end(TSRMLS_C);}
    break;

  case 235:

    { zend_do_extended_fcall_begin(TSRMLS_C); zend_do_begin_class_member_function_call(&(yyvsp[(1) - (4)]), &(yyvsp[(3) - (4)]) TSRMLS_CC); }
    break;

  case 236:

    { zend_do_end_function_call(&(yyvsp[(3) - (7)]), &(yyval), &(yyvsp[(6) - (7)]), 1, 1 TSRMLS_CC); zend_do_extended_fcall_end(TSRMLS_C);}
    break;

  case 237:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 238:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 239:

    { memset(&(yyval), 0, sizeof(znode)); (yyval).op_type = IS_UNUSED; }
    break;

  case 240:

    { memset(&(yyval), 0, sizeof(znode)); (yyval).op_type = IS_UNUSED; }
    break;

  case 241:

    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 242:

    { (yyval).u.constant.value.lval=0; }
    break;

  case 243:

    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 244:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 245:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 246:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 247:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 248:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 249:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 250:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 251:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 252:

    { zend_do_fetch_constant(&(yyval), &(yyvsp[(1) - (1)]), ZEND_CT TSRMLS_CC); }
    break;

  case 253:

    { (yyval) = (yyvsp[(2) - (2)]); }
    break;

  case 254:

    { zval minus_one;  minus_one.type = IS_LONG; minus_one.value.lval = -1;  mul_function(&(yyvsp[(2) - (2)]).u.constant, &(yyvsp[(2) - (2)]).u.constant, &minus_one TSRMLS_CC);  (yyval) = (yyvsp[(2) - (2)]); }
    break;

  case 255:

    { (yyval) = (yyvsp[(3) - (4)]); (yyval).u.constant.type = IS_CONSTANT_ARRAY; }
    break;

  case 256:

    { zend_do_fetch_constant(&(yyval), &(yyvsp[(1) - (1)]), ZEND_RT TSRMLS_CC); }
    break;

  case 257:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 258:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 259:

    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 260:

    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 261:

    { (yyval) = (yyvsp[(2) - (3)]); zend_do_end_heredoc(TSRMLS_C); }
    break;

  case 262:

    { (yyval).op_type = IS_CONST; INIT_PZVAL(&(yyval).u.constant); array_init(&(yyval).u.constant); }
    break;

  case 263:

    { (yyval) = (yyvsp[(1) - (2)]); }
    break;

  case 266:

    { zend_do_add_static_array_element(&(yyval), &(yyvsp[(3) - (5)]), &(yyvsp[(5) - (5)])); }
    break;

  case 267:

    { zend_do_add_static_array_element(&(yyval), NULL, &(yyvsp[(3) - (3)])); }
    break;

  case 268:

    { (yyval).op_type = IS_CONST; INIT_PZVAL(&(yyval).u.constant); array_init(&(yyval).u.constant); zend_do_add_static_array_element(&(yyval), &(yyvsp[(1) - (3)]), &(yyvsp[(3) - (3)])); }
    break;

  case 269:

    { (yyval).op_type = IS_CONST; INIT_PZVAL(&(yyval).u.constant); array_init(&(yyval).u.constant); zend_do_add_static_array_element(&(yyval), NULL, &(yyvsp[(1) - (1)])); }
    break;

  case 270:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 271:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 272:

    { zend_do_end_variable_parse(BP_VAR_R, 0 TSRMLS_CC); (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 273:

    { zend_do_end_variable_parse(BP_VAR_W, 0 TSRMLS_CC); (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 274:

    { zend_do_end_variable_parse(BP_VAR_RW, 0 TSRMLS_CC); (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 275:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 276:

    { zend_do_push_object(&(yyvsp[(1) - (2)]) TSRMLS_CC); }
    break;

  case 277:

    { (yyval) = (yyvsp[(4) - (4)]); }
    break;

  case 278:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 279:

    { zend_do_indirect_references(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 280:

    { fetch_array_dim(&(yyval), &(yyvsp[(1) - (4)]), &(yyvsp[(3) - (4)]) TSRMLS_CC); }
    break;

  case 281:

    { fetch_string_offset(&(yyval), &(yyvsp[(1) - (4)]), &(yyvsp[(3) - (4)]) TSRMLS_CC); }
    break;

  case 282:

    { zend_do_fetch_globals(&(yyvsp[(1) - (1)]) TSRMLS_CC); zend_do_begin_variable_parse(TSRMLS_C); fetch_simple_variable(&(yyval), &(yyvsp[(1) - (1)]), 1 TSRMLS_CC); }
    break;

  case 283:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 284:

    { (yyval) = (yyvsp[(3) - (4)]); }
    break;

  case 285:

    { (yyval).op_type = IS_UNUSED; }
    break;

  case 286:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 287:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 288:

    { zend_do_push_object(&(yyvsp[(1) - (2)]) TSRMLS_CC); }
    break;

  case 289:

    { (yyval) = (yyvsp[(4) - (4)]); }
    break;

  case 290:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 291:

    { zend_do_end_variable_parse(BP_VAR_R, 0 TSRMLS_CC); }
    break;

  case 292:

    { znode tmp_znode;  zend_do_pop_object(&tmp_znode TSRMLS_CC);  zend_do_fetch_property(&(yyval), &tmp_znode, &(yyvsp[(1) - (2)]) TSRMLS_CC);}
    break;

  case 293:

    { fetch_array_dim(&(yyval), &(yyvsp[(1) - (4)]), &(yyvsp[(3) - (4)]) TSRMLS_CC); }
    break;

  case 294:

    { fetch_string_offset(&(yyval), &(yyvsp[(1) - (4)]), &(yyvsp[(3) - (4)]) TSRMLS_CC); }
    break;

  case 295:

    { znode tmp_znode;  zend_do_pop_object(&tmp_znode TSRMLS_CC);  zend_do_fetch_property(&(yyval), &tmp_znode, &(yyvsp[(1) - (1)]) TSRMLS_CC);}
    break;

  case 296:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 297:

    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 298:

    { (yyval).u.constant.value.lval = 1; }
    break;

  case 299:

    { (yyval).u.constant.value.lval++; }
    break;

  case 302:

    { zend_do_add_list_element(&(yyvsp[(1) - (1)]) TSRMLS_CC); }
    break;

  case 303:

    { zend_do_new_list_begin(TSRMLS_C); }
    break;

  case 304:

    { zend_do_new_list_end(TSRMLS_C); }
    break;

  case 305:

    { zend_do_add_list_element(NULL TSRMLS_CC); }
    break;

  case 306:

    { zend_do_init_array(&(yyval), NULL, NULL, 0 TSRMLS_CC); }
    break;

  case 307:

    { (yyval) = (yyvsp[(1) - (2)]); }
    break;

  case 308:

    { zend_do_add_array_element(&(yyval), &(yyvsp[(5) - (5)]), &(yyvsp[(3) - (5)]), 0 TSRMLS_CC); }
    break;

  case 309:

    { zend_do_add_array_element(&(yyval), &(yyvsp[(3) - (3)]), NULL, 0 TSRMLS_CC); }
    break;

  case 310:

    { zend_do_init_array(&(yyval), &(yyvsp[(3) - (3)]), &(yyvsp[(1) - (3)]), 0 TSRMLS_CC); }
    break;

  case 311:

    { zend_do_init_array(&(yyval), &(yyvsp[(1) - (1)]), NULL, 0 TSRMLS_CC); }
    break;

  case 312:

    { zend_do_add_array_element(&(yyval), &(yyvsp[(6) - (6)]), &(yyvsp[(3) - (6)]), 1 TSRMLS_CC); }
    break;

  case 313:

    { zend_do_add_array_element(&(yyval), &(yyvsp[(4) - (4)]), NULL, 1 TSRMLS_CC); }
    break;

  case 314:

    { zend_do_init_array(&(yyval), &(yyvsp[(4) - (4)]), &(yyvsp[(1) - (4)]), 1 TSRMLS_CC); }
    break;

  case 315:

    { zend_do_init_array(&(yyval), &(yyvsp[(2) - (2)]), NULL, 1 TSRMLS_CC); }
    break;

  case 316:

    { zend_do_end_variable_parse(BP_VAR_R, 0 TSRMLS_CC);  zend_do_add_variable(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 317:

    { zend_do_add_string(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 318:

    { zend_do_add_string(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 319:

    { zend_do_add_string(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 320:

    { zend_do_add_char(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 321:

    { zend_do_add_string(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 322:

    { (yyvsp[(2) - (2)]).u.constant.value.lval = (long) '['; zend_do_add_char(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 323:

    { (yyvsp[(2) - (2)]).u.constant.value.lval = (long) ']'; zend_do_add_char(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 324:

    { (yyvsp[(2) - (2)]).u.constant.value.lval = (long) '{'; zend_do_add_char(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 325:

    { (yyvsp[(2) - (2)]).u.constant.value.lval = (long) '}'; zend_do_add_char(&(yyval), &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 326:

    { znode tmp;  (yyvsp[(2) - (2)]).u.constant.value.lval = (long) '-';  zend_do_add_char(&tmp, &(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC);  (yyvsp[(2) - (2)]).u.constant.value.lval = (long) '>'; zend_do_add_char(&(yyval), &tmp, &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 327:

    { zend_do_init_string(&(yyval) TSRMLS_CC); }
    break;

  case 328:

    { zend_do_fetch_globals(&(yyvsp[(1) - (1)]) TSRMLS_CC); zend_do_begin_variable_parse(TSRMLS_C); fetch_simple_variable(&(yyval), &(yyvsp[(1) - (1)]), 1 TSRMLS_CC); }
    break;

  case 329:

    { zend_do_begin_variable_parse(TSRMLS_C); }
    break;

  case 330:

    { zend_do_fetch_globals(&(yyvsp[(1) - (5)]) TSRMLS_CC);  fetch_array_begin(&(yyval), &(yyvsp[(1) - (5)]), &(yyvsp[(4) - (5)]) TSRMLS_CC); }
    break;

  case 331:

    { zend_do_begin_variable_parse(TSRMLS_C); fetch_simple_variable(&(yyvsp[(2) - (3)]), &(yyvsp[(1) - (3)]), 1 TSRMLS_CC); zend_do_fetch_property(&(yyval), &(yyvsp[(2) - (3)]), &(yyvsp[(3) - (3)]) TSRMLS_CC); }
    break;

  case 332:

    { zend_do_begin_variable_parse(TSRMLS_C);  fetch_simple_variable(&(yyval), &(yyvsp[(2) - (3)]), 1 TSRMLS_CC); }
    break;

  case 333:

    { zend_do_begin_variable_parse(TSRMLS_C);  fetch_array_begin(&(yyval), &(yyvsp[(2) - (6)]), &(yyvsp[(4) - (6)]) TSRMLS_CC); }
    break;

  case 334:

    { (yyval) = (yyvsp[(2) - (3)]); }
    break;

  case 335:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 336:

    { (yyval) = (yyvsp[(1) - (1)]); }
    break;

  case 337:

    { fetch_simple_variable(&(yyval), &(yyvsp[(1) - (1)]), 1 TSRMLS_CC); }
    break;

  case 338:

    { (yyval) = (yyvsp[(3) - (4)]); }
    break;

  case 339:

    { zend_do_isset_or_isempty(ZEND_ISEMPTY, &(yyval), &(yyvsp[(3) - (4)]) TSRMLS_CC); }
    break;

  case 340:

    { zend_do_include_or_eval(ZEND_INCLUDE, &(yyval), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 341:

    { zend_do_include_or_eval(ZEND_INCLUDE_ONCE, &(yyval), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 342:

    { zend_do_include_or_eval(ZEND_EVAL, &(yyval), &(yyvsp[(3) - (4)]) TSRMLS_CC); }
    break;

  case 343:

    { zend_do_include_or_eval(ZEND_REQUIRE, &(yyval), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 344:

    { zend_do_include_or_eval(ZEND_REQUIRE_ONCE, &(yyval), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 345:

    { zend_do_isset_or_isempty(ZEND_ISSET, &(yyval), &(yyvsp[(1) - (1)]) TSRMLS_CC); }
    break;

  case 346:

    { zend_do_boolean_and_begin(&(yyvsp[(1) - (2)]), &(yyvsp[(2) - (2)]) TSRMLS_CC); }
    break;

  case 347:

    { znode tmp; zend_do_isset_or_isempty(ZEND_ISSET, &tmp, &(yyvsp[(4) - (4)]) TSRMLS_CC); zend_do_boolean_and_end(&(yyval), &(yyvsp[(1) - (4)]), &tmp, &(yyvsp[(2) - (4)]) TSRMLS_CC); }
    break;


/* Line 1267 of yacc.c.  */

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






