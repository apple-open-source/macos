/* A Bison parser, made by GNU Bison 1.875.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software Foundation, Inc.

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

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

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
     tINT = 258,
     tDOUBLE = 259,
     tSTRING = 260,
     CYAN = 261,
     L_CYAN = 262,
     MAGENTA = 263,
     L_MAGENTA = 264,
     YELLOW = 265,
     D_YELLOW = 266,
     BLACK = 267,
     L_BLACK = 268,
     GAMMA = 269,
     LEVEL = 270,
     STEPS = 271,
     INK_LIMIT = 272,
     PRINTER = 273,
     PARAMETER = 274,
     PARAMETER_INT = 275,
     PARAMETER_FLOAT = 276,
     PARAMETER_CURVE = 277,
     DENSITY = 278,
     TOP = 279,
     LEFT = 280,
     HSIZE = 281,
     VSIZE = 282,
     BLACKLINE = 283,
     PATTERN = 284,
     XPATTERN = 285,
     EXTENDED = 286,
     IMAGE = 287,
     GRID = 288,
     SEMI = 289,
     CHANNEL = 290,
     CMYK = 291,
     KCMY = 292,
     RGB = 293,
     CMY = 294,
     GRAY = 295,
     WHITE = 296,
     RAW = 297,
     MODE = 298,
     PAGESIZE = 299,
     MESSAGE = 300,
     END = 301
   };
#endif
#define tINT 258
#define tDOUBLE 259
#define tSTRING 260
#define CYAN 261
#define L_CYAN 262
#define MAGENTA 263
#define L_MAGENTA 264
#define YELLOW 265
#define D_YELLOW 266
#define BLACK 267
#define L_BLACK 268
#define GAMMA 269
#define LEVEL 270
#define STEPS 271
#define INK_LIMIT 272
#define PRINTER 273
#define PARAMETER 274
#define PARAMETER_INT 275
#define PARAMETER_FLOAT 276
#define PARAMETER_CURVE 277
#define DENSITY 278
#define TOP 279
#define LEFT 280
#define HSIZE 281
#define VSIZE 282
#define BLACKLINE 283
#define PATTERN 284
#define XPATTERN 285
#define EXTENDED 286
#define IMAGE 287
#define GRID 288
#define SEMI 289
#define CHANNEL 290
#define CMYK 291
#define KCMY 292
#define RGB 293
#define CMY 294
#define GRAY 295
#define WHITE 296
#define RAW 297
#define MODE 298
#define PAGESIZE 299
#define MESSAGE 300
#define END 301




/* Copy the first part of user declarations.  */
#line 23 "testpatterny.y"


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "testpattern.h"

extern int mylineno;

extern int yylex(void);
char *quotestrip(const char *i);
char *endstrip(const char *i);

extern char* yytext;

static int yyerror( const char *s )
{
	fprintf(stderr,"stdin:%d: %s before '%s'\n",mylineno,s,yytext);
	return 0;
}

typedef struct
{
  const char *name;
  int channel;
} color_t;

static color_t color_map[] =
  {
    { "black", 0 },
    { "cyan", 1 },
    { "red", 1 },
    { "magenta", 2 },
    { "green", 2 },
    { "yellow", 3 },
    { "blue", 3 },
    { "l_black", 4 },
    { "l_cyan", 5 },
    { "l_magenta", 6 },
    { "d_yellow", 4 },
    { NULL, -1 }
  };

static int current_index = 0;
static testpattern_t *current_testpattern;
extern FILE *yyin;

static int
find_color(const char *name)
{
  int i = 0;
  while (color_map[i].name)
    {
      if (strcmp(color_map[i].name, name) == 0)
	return color_map[i].channel;
      i++;
    }
  return -1;
}



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

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 214 of yacc.c.  */
#line 244 "testpatterny.c"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

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
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAXIMUM)

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
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
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
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  52
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   131

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  47
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  65
/* YYNRULES -- Number of rules. */
#define YYNRULES  115
/* YYNRULES -- Number of states. */
#define YYNSTATES  170

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   301

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
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
      45,    46
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short yyprhs[] =
{
       0,     0,     3,     5,     7,     9,    11,    13,    15,    17,
      19,    22,    25,    28,    31,    34,    37,    41,    43,    45,
      47,    49,    51,    53,    55,    58,    62,    66,    70,    74,
      77,    80,    83,    86,    89,    93,    95,    97,   101,   105,
     109,   111,   113,   115,   118,   121,   124,   127,   130,   133,
     137,   139,   142,   143,   145,   148,   153,   159,   161,   163,
     165,   168,   169,   171,   173,   175,   181,   185,   188,   191,
     195,   198,   202,   207,   213,   220,   222,   224,   226,   228,
     230,   232,   234,   236,   238,   240,   242,   244,   246,   248,
     250,   252,   254,   256,   258,   260,   262,   264,   266,   268,
     271,   273,   275,   277,   279,   282,   283,   286,   288,   289,
     292,   294,   296,   297,   300,   301
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
     110,     0,    -1,     6,    -1,     7,    -1,     8,    -1,     9,
      -1,    10,    -1,    11,    -1,    12,    -1,    13,    -1,    36,
       3,    -1,    37,     3,    -1,    38,     3,    -1,    39,     3,
      -1,    40,     3,    -1,    41,     3,    -1,    31,     3,     3,
      -1,    49,    -1,    50,    -1,    51,    -1,    52,    -1,    53,
      -1,    54,    -1,    55,    -1,    43,    56,    -1,    15,    48,
       4,    -1,    15,     3,     4,    -1,    14,    48,     4,    -1,
      14,     3,     4,    -1,    14,     4,    -1,    16,     3,    -1,
      17,     4,    -1,    18,     5,    -1,    44,     5,    -1,    44,
       3,     3,    -1,    66,    -1,    67,    -1,    19,     5,     5,
      -1,    20,     5,     3,    -1,    21,     5,     4,    -1,    69,
      -1,    70,    -1,    71,    -1,    23,     4,    -1,    24,     4,
      -1,    25,     4,    -1,    26,     4,    -1,    27,     4,    -1,
      28,     3,    -1,     4,     4,     4,    -1,    79,    -1,    80,
      79,    -1,    -1,    80,    -1,    79,    81,    -1,    48,     4,
       4,     4,    -1,    35,     3,     4,     4,     4,    -1,    83,
      -1,    84,    -1,    85,    -1,    86,    85,    -1,    -1,    86,
      -1,    82,    -1,    87,    -1,     4,     4,     4,     4,     4,
      -1,    29,    89,    88,    -1,    30,    88,    -1,    33,     3,
      -1,    32,     3,     3,    -1,    45,     5,    -1,    45,     5,
       5,    -1,    45,     5,     5,     5,    -1,    45,     5,     5,
       5,     5,    -1,    45,     5,     5,     5,     5,     5,    -1,
      94,    -1,    95,    -1,    96,    -1,    97,    -1,    98,    -1,
      99,    -1,    60,    -1,    61,    -1,    58,    -1,    59,    -1,
      62,    -1,    63,    -1,    64,    -1,    65,    -1,    72,    -1,
      73,    -1,    74,    -1,    75,    -1,    76,    -1,    77,    -1,
      78,    -1,    57,    -1,    68,    -1,   100,    -1,   101,    34,
      -1,    90,    -1,    91,    -1,    92,    -1,   100,    -1,   103,
      34,    -1,    -1,   105,   104,    -1,    93,    -1,    -1,   107,
     102,    -1,   105,    -1,   106,    -1,    -1,    46,    34,    -1,
      -1,   107,   111,   108,   109,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short yyrline[] =
{
       0,   139,   139,   139,   139,   139,   140,   140,   140,   140,
     143,   155,   167,   179,   191,   203,   215,   227,   227,   227,
     227,   227,   227,   227,   230,   233,   243,   252,   262,   271,
     278,   285,   292,   301,   310,   319,   319,   322,   332,   341,
     364,   364,   364,   366,   373,   380,   387,   394,   401,   409,
     424,   424,   427,   427,   430,   433,   447,   460,   460,   463,
     463,   466,   466,   469,   469,   472,   487,   490,   505,   516,
     533,   539,   546,   554,   563,   574,   574,   574,   574,   574,
     577,   580,   580,   580,   580,   580,   580,   581,   581,   581,
     581,   581,   581,   581,   582,   582,   582,   582,   582,   585,
     589,   589,   589,   589,   592,   596,   596,   599,   603,   603,
     606,   606,   609,   609,   614,   613
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "tINT", "tDOUBLE", "tSTRING", "CYAN", 
  "L_CYAN", "MAGENTA", "L_MAGENTA", "YELLOW", "D_YELLOW", "BLACK", 
  "L_BLACK", "GAMMA", "LEVEL", "STEPS", "INK_LIMIT", "PRINTER", 
  "PARAMETER", "PARAMETER_INT", "PARAMETER_FLOAT", "PARAMETER_CURVE", 
  "DENSITY", "TOP", "LEFT", "HSIZE", "VSIZE", "BLACKLINE", "PATTERN", 
  "XPATTERN", "EXTENDED", "IMAGE", "GRID", "SEMI", "CHANNEL", "CMYK", 
  "KCMY", "RGB", "CMY", "GRAY", "WHITE", "RAW", "MODE", "PAGESIZE", 
  "MESSAGE", "END", "$accept", "COLOR", "cmykspec", "kcmyspec", "rgbspec", 
  "cmyspec", "grayspec", "whitespec", "extendedspec", "modespec", 
  "inputspec", "level", "channel_level", "gamma", "channel_gamma", 
  "global_gamma", "steps", "ink_limit", "printer", "page_size_name", 
  "page_size_custom", "page_size", "parameter_string", "parameter_int", 
  "parameter_float", "parameter", "density", "top", "left", "hsize", 
  "vsize", "blackline", "color_block1", "color_blocks1a", 
  "color_blocks1b", "color_blocks1", "color_block2a", "color_block2b", 
  "color_block2", "color_blocks2a", "color_blocks2", "color_blocks", 
  "patvars", "pattern", "xpattern", "grid", "image", "Message0", 
  "Message1", "Message2", "Message3", "Message4", "A_Message", "message", 
  "A_Rule", "Rule", "A_Pattern", "Pattern", "Patterns", "Image", "Rules", 
  "Output", "EOF", "Thing", "@1", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    47,    48,    48,    48,    48,    48,    48,    48,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    56,    56,
      56,    56,    56,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    68,    69,    70,    71,
      72,    72,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    80,    81,    81,    82,    83,    84,    85,    85,    86,
      86,    87,    87,    88,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,    99,    99,    99,    99,
     100,   101,   101,   101,   101,   101,   101,   101,   101,   101,
     101,   101,   101,   101,   101,   101,   101,   101,   101,   102,
     103,   103,   103,   103,   104,   105,   105,   106,   107,   107,
     108,   108,   109,   109,   111,   110
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     2,     2,     2,     2,     2,     3,     1,     1,     1,
       1,     1,     1,     1,     2,     3,     3,     3,     3,     2,
       2,     2,     2,     2,     3,     1,     1,     3,     3,     3,
       1,     1,     1,     2,     2,     2,     2,     2,     2,     3,
       1,     2,     0,     1,     2,     4,     5,     1,     1,     1,
       2,     0,     1,     1,     1,     5,     3,     2,     2,     3,
       2,     3,     4,     5,     6,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     1,     1,     1,     2,     0,     2,     1,     0,     2,
       1,     1,     0,     2,     0,     4
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
     108,   114,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      96,    83,    84,    81,    82,    85,    86,    87,    88,    35,
      36,    97,    40,    41,    42,    89,    90,    91,    92,    93,
      94,    95,    75,    76,    77,    78,    79,    80,    98,     0,
     109,   105,     1,     0,    29,     2,     3,     4,     5,     6,
       7,     8,     9,     0,     0,     0,    30,    31,    32,     0,
       0,     0,    43,    44,    45,    46,    47,    48,     0,     0,
       0,     0,     0,     0,     0,    17,    18,    19,    20,    21,
      22,    23,    24,     0,    33,    70,    99,     0,   107,   110,
     111,   112,    28,    27,    26,    25,    37,    38,    39,     0,
      10,    11,    12,    13,    14,    15,    34,    71,     0,     0,
      61,     0,   100,   101,   102,   103,     0,   106,     0,   115,
      16,    72,    69,     0,    61,     0,     0,     0,    52,    63,
      57,    58,    59,    62,    64,    67,    68,   104,   113,    73,
       0,    66,     0,     0,     0,    50,    53,    54,    60,    74,
       0,    49,     0,     0,    51,     0,     0,    55,    65,    56
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short yydefgoto[] =
{
      -1,   137,    85,    86,    87,    88,    89,    90,    91,    92,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,   138,   156,   157,   139,   140,   141,   142,   143,
     144,   145,   134,   122,   123,   124,    98,    42,    43,    44,
      45,    46,    47,    48,    49,    50,   126,   127,    99,   100,
       1,   101,   129,     2,    51
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -131
static const short yypact[] =
{
    -131,   -14,    16,    56,    41,    25,    36,    38,    53,    66,
      67,    69,    70,    77,    78,    79,    81,    39,    22,    80,
    -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,
    -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,
    -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,    54,
    -131,    55,  -131,    82,  -131,  -131,  -131,  -131,  -131,  -131,
    -131,  -131,  -131,    85,    86,    87,  -131,  -131,  -131,    88,
      89,    90,  -131,  -131,  -131,  -131,  -131,  -131,    92,    93,
      94,    95,    96,    97,    98,  -131,  -131,  -131,  -131,  -131,
    -131,  -131,  -131,    99,  -131,   100,  -131,   101,  -131,    12,
    -131,    57,  -131,  -131,  -131,  -131,  -131,  -131,  -131,   103,
    -131,  -131,  -131,  -131,  -131,  -131,  -131,   102,   105,   106,
      11,   108,  -131,  -131,  -131,  -131,    75,  -131,    83,  -131,
    -131,   107,  -131,   109,    11,   110,   112,   114,   115,  -131,
    -131,  -131,  -131,    26,  -131,  -131,  -131,  -131,  -131,   111,
     116,  -131,   117,   118,   119,  -131,   115,  -131,  -131,  -131,
     120,  -131,   121,   122,  -131,   123,   124,  -131,  -131,  -131
};

/* YYPGOTO[NTERM-NUM].  */
static const short yypgoto[] =
{
    -131,    52,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,
    -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,
    -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,
    -131,  -131,  -130,  -131,  -131,  -131,  -131,  -131,   -13,  -131,
    -131,    -5,  -131,  -131,  -131,  -131,  -131,  -131,  -131,  -131,
    -131,  -131,  -131,    32,  -131,  -131,  -131,  -131,  -131,  -131,
    -131,  -131,  -131,  -131,  -131
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
       3,     4,     5,     6,     7,     8,     9,    10,   155,    11,
      12,    13,    14,    15,    16,   135,    52,    55,    56,    57,
      58,    59,    60,    61,    62,    93,   164,    94,    66,    17,
      18,    19,    55,    56,    57,    58,    59,    60,    61,    62,
      67,   119,   120,    68,    64,   121,   136,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    65,    19,    69,    53,
      54,   136,    55,    56,    57,    58,    59,    60,    61,    62,
      78,    70,    71,    72,    73,    79,    80,    81,    82,    83,
      84,    74,    75,    76,    77,    95,   102,    97,    96,   103,
     104,   105,   107,   106,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   128,   118,   117,   130,   131,   132,   147,
     133,   146,   149,   150,   152,   153,   159,   148,   154,   135,
     160,   161,   162,   163,   165,   166,   167,   168,   169,   151,
     158,   125
};

static const unsigned char yycheck[] =
{
      14,    15,    16,    17,    18,    19,    20,    21,   138,    23,
      24,    25,    26,    27,    28,     4,     0,     6,     7,     8,
       9,    10,    11,    12,    13,     3,   156,     5,     3,    43,
      44,    45,     6,     7,     8,     9,    10,    11,    12,    13,
       4,    29,    30,     5,     3,    33,    35,     6,     7,     8,
       9,    10,    11,    12,    13,     3,     4,    45,     5,     3,
       4,    35,     6,     7,     8,     9,    10,    11,    12,    13,
      31,     5,     5,     4,     4,    36,    37,    38,    39,    40,
      41,     4,     4,     4,     3,     5,     4,    32,    34,     4,
       4,     4,     3,     5,     4,     3,     3,     3,     3,     3,
       3,     3,     3,    46,     3,     5,     3,     5,     3,    34,
       4,     3,     5,     4,     4,     3,     5,    34,     4,     4,
       4,     4,     4,     4,     4,     4,     4,     4,     4,   134,
     143,    99
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,   107,   110,    14,    15,    16,    17,    18,    19,    20,
      21,    23,    24,    25,    26,    27,    28,    43,    44,    45,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   111,     0,     3,     4,     6,     7,     8,     9,    10,
      11,    12,    13,    48,     3,    48,     3,     4,     5,     5,
       5,     5,     4,     4,     4,     4,     4,     3,    31,    36,
      37,    38,    39,    40,    41,    49,    50,    51,    52,    53,
      54,    55,    56,     3,     5,     5,    34,    32,    93,   105,
     106,   108,     4,     4,     4,     4,     5,     3,     4,     3,
       3,     3,     3,     3,     3,     3,     3,     5,     3,    29,
      30,    33,    90,    91,    92,   100,   103,   104,    46,   109,
       3,     5,     3,     4,    89,     4,    35,    48,    79,    82,
      83,    84,    85,    86,    87,    88,     3,    34,    34,     5,
       4,    88,     4,     3,     4,    79,    80,    81,    85,     5,
       4,     4,     4,     4,    79,     4,     4,     4,     4,     4
};

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

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrlab1

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
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)         \
  Current.first_line   = Rhs[1].first_line;      \
  Current.first_column = Rhs[1].first_column;    \
  Current.last_line    = Rhs[N].last_line;       \
  Current.last_column  = Rhs[N].last_column;
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
} while (0)

# define YYDSYMPRINT(Args)			\
do {						\
  if (yydebug)					\
    yysymprint Args;				\
} while (0)

# define YYDSYMPRINTF(Title, Token, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Token, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (cinluded).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short *bottom, short *top)
#else
static void
yy_stack_print (bottom, top)
    short *bottom;
    short *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylineno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylineno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
# define YYDSYMPRINTF(Title, Token, Value, Location)
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
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
    }
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yytype, yyvaluep)
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

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
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
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

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
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
      YYDSYMPRINTF ("Next token is", yytoken, &yylval, &yylloc);
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

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %s, ", yytname[yytoken]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
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
#line 144 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>cmykspec %d\n", yyvsp[0].ival);
	  global_image_type = "CMYK";
	  global_channel_depth = 4;
	  global_invert_data = 0;
	  if (yyvsp[0].ival == 8 || yyvsp[0].ival == 16)
	    global_bit_depth = yyvsp[0].ival;
	}
    break;

  case 11:
#line 156 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>kcmyspec %d\n", yyvsp[0].ival);
	  global_image_type = "KCMY";
	  global_channel_depth = 4;
	  global_invert_data = 0;
	  if (yyvsp[0].ival == 8 || yyvsp[0].ival == 16)
	    global_bit_depth = yyvsp[0].ival;
	}
    break;

  case 12:
#line 168 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>rgbspec %d\n", yyvsp[0].ival);
	  global_image_type = "RGB";
	  global_channel_depth = 3;
	  global_invert_data = 1;
	  if (yyvsp[0].ival == 8 || yyvsp[0].ival == 16)
	    global_bit_depth = yyvsp[0].ival;
	}
    break;

  case 13:
#line 180 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>cmyspec %d\n", yyvsp[0].ival);
	  global_image_type = "CMY";
	  global_channel_depth = 3;
	  global_invert_data = 0;
	  if (yyvsp[0].ival == 8 || yyvsp[0].ival == 16)
	    global_bit_depth = yyvsp[0].ival;
	}
    break;

  case 14:
#line 192 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>grayspec %d\n", yyvsp[0].ival);
	  global_image_type = "Grayscale";
	  global_channel_depth = 1;
	  global_invert_data = 0;
	  if (yyvsp[0].ival == 8 || yyvsp[0].ival == 16)
	    global_bit_depth = yyvsp[0].ival;
	}
    break;

  case 15:
#line 204 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>whitespec %d\n", yyvsp[0].ival);
	  global_image_type = "Whitescale";
	  global_channel_depth = 1;
	  global_invert_data = 1;
	  if (yyvsp[0].ival == 8 || yyvsp[0].ival == 16)
	    global_bit_depth = yyvsp[0].ival;
	}
    break;

  case 16:
#line 216 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>extendedspec %d\n", yyvsp[-1].ival);
	  global_image_type = "Raw";
	  global_invert_data = 0;
	  global_channel_depth = yyvsp[-1].ival;
	  if (yyvsp[-1].ival == 8 || yyvsp[-1].ival == 16)
	    global_bit_depth = yyvsp[0].ival;
	}
    break;

  case 25:
#line 234 "testpatterny.y"
    {
	  int channel = find_color(yyvsp[-1].sval);
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>level %s %f\n", yyvsp[-1].sval, yyvsp[0].dval);
	  if (channel >= 0)
	    global_levels[channel] = yyvsp[0].dval;
	}
    break;

  case 26:
#line 244 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>channel_level %d %f\n", yyvsp[-1].ival, yyvsp[0].dval);
	  if (yyvsp[-1].ival >= 0 && yyvsp[-1].ival <= STP_CHANNEL_LIMIT)
	    global_levels[yyvsp[-1].ival] = yyvsp[0].dval;
	}
    break;

  case 27:
#line 253 "testpatterny.y"
    {
	  int channel = find_color(yyvsp[-1].sval);
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>gamma %s %f\n", yyvsp[-1].sval, yyvsp[0].dval);
	  if (channel >= 0)
	    global_gammas[channel] = yyvsp[0].dval;
	}
    break;

  case 28:
#line 263 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>channel_gamma %d %f\n", yyvsp[-1].ival, yyvsp[0].dval);
	  if (yyvsp[-1].ival >= 0 && yyvsp[-1].ival <= STP_CHANNEL_LIMIT)
	    global_gammas[yyvsp[-1].ival] = yyvsp[0].dval;
	}
    break;

  case 29:
#line 272 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>global_gamma %f\n", yyvsp[0].dval);
	  global_gamma = yyvsp[0].dval;
	}
    break;

  case 30:
#line 279 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>steps %d\n", yyvsp[0].ival);
	  global_steps = yyvsp[0].ival;
	}
    break;

  case 31:
#line 286 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>ink_limit %f\n", yyvsp[0].dval);
	  global_ink_limit = yyvsp[0].dval;
	}
    break;

  case 32:
#line 293 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>printer %s\n", yyvsp[0].sval);
	  global_printer = strdup(yyvsp[0].sval);
	  free(yyvsp[0].sval);
	}
    break;

  case 33:
#line 302 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>page_size_name %s\n", yyvsp[0].sval);
	  stp_set_string_parameter(global_vars, "PageSize", yyvsp[0].sval);
	  free(yyvsp[0].sval);
	}
    break;

  case 34:
#line 311 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>page_size_custom %d %d\n", yyvsp[-1].ival, yyvsp[0].ival);
	  stp_set_page_width(global_vars, yyvsp[-1].ival);
	  stp_set_page_height(global_vars, yyvsp[0].ival);
	}
    break;

  case 37:
#line 323 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>parameter_string %s %s\n", yyvsp[-1].sval, yyvsp[0].sval);
	  stp_set_string_parameter(global_vars, yyvsp[-1].sval, yyvsp[0].sval);
	  free(yyvsp[-1].sval);
	  free(yyvsp[0].sval);
	}
    break;

  case 38:
#line 333 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>parameter_int %s %d\n", yyvsp[-1].sval, yyvsp[0].ival);
	  stp_set_int_parameter(global_vars, yyvsp[-1].sval, yyvsp[0].ival);
	  free(yyvsp[-1].sval);
	}
    break;

  case 39:
#line 342 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>parameter_float %s %f\n", yyvsp[-1].sval, yyvsp[0].dval);
	  stp_set_float_parameter(global_vars, yyvsp[-1].sval, yyvsp[0].dval);
	  free(yyvsp[-1].sval);
	}
    break;

  case 43:
#line 367 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>density %f\n", yyvsp[0].dval);
	  global_density = yyvsp[0].dval;
	}
    break;

  case 44:
#line 374 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>top %f\n", yyvsp[0].dval);
	  global_xtop = yyvsp[0].dval;
	}
    break;

  case 45:
#line 381 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>left %f\n", yyvsp[0].dval);
	  global_xleft = yyvsp[0].dval;
	}
    break;

  case 46:
#line 388 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>hsize %f\n", yyvsp[0].dval);
	  global_hsize = yyvsp[0].dval;
	}
    break;

  case 47:
#line 395 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>vsize %f\n", yyvsp[0].dval);
	  global_vsize = yyvsp[0].dval;
	}
    break;

  case 48:
#line 402 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>blackline %d\n", yyvsp[0].ival);
	  global_noblackline = !(yyvsp[0].ival);
	}
    break;

  case 49:
#line 410 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>color_block1 %f %f %f (%d)\n", yyvsp[-2].dval, yyvsp[-1].dval, yyvsp[0].dval,
		    current_index);
	  if (current_index < STP_CHANNEL_LIMIT)
	    {
	      current_testpattern->d.p.mins[current_index] = yyvsp[-2].dval;
	      current_testpattern->d.p.vals[current_index] = yyvsp[-1].dval;
	      current_testpattern->d.p.gammas[current_index] = yyvsp[0].dval;
	      current_index++;
	    }
	}
    break;

  case 55:
#line 434 "testpatterny.y"
    {
	  int channel = find_color(yyvsp[-3].sval);
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>color_block2a %s %f %f %f\n", yyvsp[-3].sval, yyvsp[-2].dval, yyvsp[-1].dval, yyvsp[0].dval);
	  if (channel >= 0 && channel < STP_CHANNEL_LIMIT)
	    {
	      current_testpattern->d.p.mins[channel] = yyvsp[-2].dval;
	      current_testpattern->d.p.vals[channel] = yyvsp[-1].dval;
	      current_testpattern->d.p.gammas[channel] = yyvsp[0].dval;
	    }
	}
    break;

  case 56:
#line 448 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>color_block2b %d %f %f %f\n", yyvsp[-3].ival, yyvsp[-2].dval, yyvsp[-1].dval, yyvsp[0].dval);
	  if (yyvsp[-3].ival >= 0 && yyvsp[-3].ival < STP_CHANNEL_LIMIT)
	    {
	      current_testpattern->d.p.mins[yyvsp[-3].ival] = yyvsp[-2].dval;
	      current_testpattern->d.p.vals[yyvsp[-3].ival] = yyvsp[-1].dval;
	      current_testpattern->d.p.gammas[yyvsp[-3].ival] = yyvsp[0].dval;
	    }
	}
    break;

  case 65:
#line 473 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>patvars %f %f %f %f %f\n", yyvsp[-4].dval, yyvsp[-3].dval, yyvsp[-2].dval, yyvsp[-1].dval, yyvsp[0].dval);
	  current_testpattern->t = E_PATTERN;
	  current_testpattern->d.p.lower = yyvsp[-4].dval;
	  current_testpattern->d.p.upper = yyvsp[-3].dval;
	  current_testpattern->d.p.levels[1] = yyvsp[-2].dval;
	  current_testpattern->d.p.levels[2] = yyvsp[-1].dval;
	  current_testpattern->d.p.levels[3] = yyvsp[0].dval;
	  current_testpattern = get_next_testpattern();
	  current_index = 0;
	}
    break;

  case 67:
#line 491 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>xpattern\n");
	  if (global_channel_depth == 0)
	    {
	      fprintf(stderr, "xpattern may only be used with extended color depth\n");
	      exit(1);
	    }
	  current_testpattern->t = E_XPATTERN;
	  current_testpattern = get_next_testpattern();
	  current_index = 0;
	}
    break;

  case 68:
#line 506 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>grid %d\n", yyvsp[0].ival);
	  current_testpattern->t = E_GRID;
	  current_testpattern->d.g.ticks = yyvsp[0].ival;
	  current_testpattern = get_next_testpattern();
	  current_index = 0;
	}
    break;

  case 69:
#line 517 "testpatterny.y"
    {
	  if (getenv("STP_TESTPATTERN_DEBUG"))
	    fprintf(stderr, ">>>image %d %d\n", yyvsp[-1].ival, yyvsp[0].ival);
	  current_testpattern->t = E_IMAGE;
	  current_testpattern->d.i.x = yyvsp[-1].ival;
	  current_testpattern->d.i.y = yyvsp[0].ival;
	  if (current_testpattern->d.i.x <= 0 ||
	      current_testpattern->d.i.y <= 0)
	    {
	      fprintf(stderr, "image width and height must be greater than zero\n");
	      exit(1);
	    }
	  return 0;
	}
    break;

  case 70:
#line 534 "testpatterny.y"
    {
	  fprintf(stderr, yyvsp[0].sval);
	  free(yyvsp[0].sval);
	}
    break;

  case 71:
#line 540 "testpatterny.y"
    {
	  fprintf(stderr, yyvsp[-1].sval, yyvsp[0].sval);
	  free(yyvsp[-1].sval);
	  free(yyvsp[0].sval);
	}
    break;

  case 72:
#line 547 "testpatterny.y"
    {
	  fprintf(stderr, yyvsp[-2].sval, yyvsp[-1].sval, yyvsp[0].sval);
	  free(yyvsp[-2].sval);
	  free(yyvsp[-1].sval);
	  free(yyvsp[0].sval);
	}
    break;

  case 73:
#line 555 "testpatterny.y"
    {
	  fprintf(stderr, yyvsp[-3].sval, yyvsp[-2].sval, yyvsp[-1].sval, yyvsp[0].sval);
	  free(yyvsp[-3].sval);
	  free(yyvsp[-2].sval);
	  free(yyvsp[-1].sval);
	  free(yyvsp[0].sval);
	}
    break;

  case 74:
#line 564 "testpatterny.y"
    {
	  fprintf(stderr, yyvsp[-4].sval, yyvsp[-3].sval, yyvsp[-2].sval, yyvsp[-1].sval, yyvsp[0].sval);
	  free(yyvsp[-4].sval);
	  free(yyvsp[-3].sval);
	  free(yyvsp[-2].sval);
	  free(yyvsp[-1].sval);
	  free(yyvsp[0].sval);
	}
    break;

  case 99:
#line 586 "testpatterny.y"
    { global_did_something = 1; }
    break;

  case 104:
#line 593 "testpatterny.y"
    { global_did_something = 1; }
    break;

  case 107:
#line 600 "testpatterny.y"
    { global_did_something = 1; }
    break;

  case 113:
#line 610 "testpatterny.y"
    { return 0; }
    break;

  case 114:
#line 614 "testpatterny.y"
    {
	  current_testpattern = get_next_testpattern();
	}
    break;


    }

/* Line 991 of yacc.c.  */
#line 1750 "testpatterny.c"

  yyvsp -= yylen;
  yyssp -= yylen;


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
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("syntax error, unexpected ") + 1;
	  yysize += yystrlen (yytname[yytype]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("syntax error");
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* Return failure if at end of input.  */
      if (yychar == YYEOF)
        {
	  /* Pop the error token.  */
          YYPOPSTACK;
	  /* Pop the rest of the stack.  */
	  while (yyss < yyssp)
	    {
	      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
	      yydestruct (yystos[*yyssp], yyvsp);
	      YYPOPSTACK;
	    }
	  YYABORT;
        }

      YYDSYMPRINTF ("Error: discarding", yytoken, &yylval, &yylloc);
      yydestruct (yytoken, &yylval);
      yychar = YYEMPTY;

    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab2;


/*----------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action.  |
`----------------------------------------------------*/
yyerrlab1:

  /* Suppress GCC warning that yyerrlab1 is unused when no action
     invokes YYERROR.  */
#if defined (__GNUC_MINOR__) && 2093 <= (__GNUC__ * 1000 + __GNUC_MINOR__) \
    && !defined __cplusplus
  __attribute__ ((__unused__))
#endif


  goto yyerrlab2;


/*---------------------------------------------------------------.
| yyerrlab2 -- pop states until the error token can be shifted.  |
`---------------------------------------------------------------*/
yyerrlab2:
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

      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
      yydestruct (yystos[yystate], yyvsp);
      yyvsp--;
      yystate = *--yyssp;

      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;


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
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 620 "testpatterny.y"


