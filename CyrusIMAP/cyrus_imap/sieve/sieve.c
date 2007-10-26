/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

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

/* Bison version.  */
#define YYBISON_VERSION "2.1"

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
     NUMBER = 258,
     STRING = 259,
     IF = 260,
     ELSIF = 261,
     ELSE = 262,
     REJCT = 263,
     FILEINTO = 264,
     REDIRECT = 265,
     KEEP = 266,
     STOP = 267,
     DISCARD = 268,
     VACATION = 269,
     REQUIRE = 270,
     SETFLAG = 271,
     ADDFLAG = 272,
     REMOVEFLAG = 273,
     MARK = 274,
     UNMARK = 275,
     NOTIFY = 276,
     DENOTIFY = 277,
     ANYOF = 278,
     ALLOF = 279,
     EXISTS = 280,
     SFALSE = 281,
     STRUE = 282,
     HEADER = 283,
     NOT = 284,
     SIZE = 285,
     ADDRESS = 286,
     ENVELOPE = 287,
     BODY = 288,
     COMPARATOR = 289,
     IS = 290,
     CONTAINS = 291,
     MATCHES = 292,
     REGEX = 293,
     COUNT = 294,
     VALUE = 295,
     OVER = 296,
     UNDER = 297,
     GT = 298,
     GE = 299,
     LT = 300,
     LE = 301,
     EQ = 302,
     NE = 303,
     ALL = 304,
     LOCALPART = 305,
     DOMAIN = 306,
     USER = 307,
     DETAIL = 308,
     RAW = 309,
     TEXT = 310,
     CONTENT = 311,
     DAYS = 312,
     ADDRESSES = 313,
     SUBJECT = 314,
     FROM = 315,
     HANDLE = 316,
     MIME = 317,
     METHOD = 318,
     ID = 319,
     OPTIONS = 320,
     LOW = 321,
     NORMAL = 322,
     HIGH = 323,
     ANY = 324,
     MESSAGE = 325,
     INCLUDE = 326,
     PERSONAL = 327,
     GLOBAL = 328,
     RETURN = 329,
     COPY = 330
   };
#endif
/* Tokens.  */
#define NUMBER 258
#define STRING 259
#define IF 260
#define ELSIF 261
#define ELSE 262
#define REJCT 263
#define FILEINTO 264
#define REDIRECT 265
#define KEEP 266
#define STOP 267
#define DISCARD 268
#define VACATION 269
#define REQUIRE 270
#define SETFLAG 271
#define ADDFLAG 272
#define REMOVEFLAG 273
#define MARK 274
#define UNMARK 275
#define NOTIFY 276
#define DENOTIFY 277
#define ANYOF 278
#define ALLOF 279
#define EXISTS 280
#define SFALSE 281
#define STRUE 282
#define HEADER 283
#define NOT 284
#define SIZE 285
#define ADDRESS 286
#define ENVELOPE 287
#define BODY 288
#define COMPARATOR 289
#define IS 290
#define CONTAINS 291
#define MATCHES 292
#define REGEX 293
#define COUNT 294
#define VALUE 295
#define OVER 296
#define UNDER 297
#define GT 298
#define GE 299
#define LT 300
#define LE 301
#define EQ 302
#define NE 303
#define ALL 304
#define LOCALPART 305
#define DOMAIN 306
#define USER 307
#define DETAIL 308
#define RAW 309
#define TEXT 310
#define CONTENT 311
#define DAYS 312
#define ADDRESSES 313
#define SUBJECT 314
#define FROM 315
#define HANDLE 316
#define MIME 317
#define METHOD 318
#define ID 319
#define OPTIONS 320
#define LOW 321
#define NORMAL 322
#define HIGH 323
#define ANY 324
#define MESSAGE 325
#define INCLUDE 326
#define PERSONAL 327
#define GLOBAL 328
#define RETURN 329
#define COPY 330




/* Copy the first part of user declarations.  */
#line 1 "sieve.y"

/* sieve.y -- sieve parser
 * Larry Greenfield
 * $Id: sieve.y,v 1.33 2006/11/30 17:11:25 murch Exp $
 */
/***********************************************************
        Copyright 1999 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Carnegie Mellon
University not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE FOR
ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
******************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "xmalloc.h"
#include "comparator.h"
#include "interp.h"
#include "script.h"
#include "tree.h"

#include "../lib/util.h"
#include "../lib/imparse.h"
#include "../lib/libconfig.h"

    /* definitions */
    extern int addrparse(void);

struct vtags {
    int days;
    stringlist_t *addresses;
    char *subject;
    char *from;
    char *handle;
    int mime;
};

struct htags {
    char *comparator;
    int comptag;
    int relation;
};

struct aetags {
    int addrtag;
    char *comparator;
    int comptag;
    int relation;
};

struct btags {
    int transform;
    int offset;
    stringlist_t *content_types;
    char *comparator;
    int comptag;
    int relation;
};

struct ntags {
    char *method;
    char *id;
    stringlist_t *options;
    int priority;
    char *message;
};

struct dtags {
    int comptag;
    int relation;
    void *pattern;
    int priority;
};

static commandlist_t *ret;
static sieve_script_t *parse_script;
static int check_reqs(stringlist_t *sl);
static test_t *build_address(int t, struct aetags *ae,
			     stringlist_t *sl, stringlist_t *pl);
static test_t *build_header(int t, struct htags *h,
			    stringlist_t *sl, stringlist_t *pl);
static test_t *build_body(int t, struct btags *b, stringlist_t *pl);
static commandlist_t *build_vacation(int t, struct vtags *h, char *s);
static commandlist_t *build_notify(int t, struct ntags *n);
static commandlist_t *build_denotify(int t, struct dtags *n);
static commandlist_t *build_fileinto(int t, int c, char *f);
static commandlist_t *build_redirect(int t, int c, char *a);
static struct aetags *new_aetags(void);
static struct aetags *canon_aetags(struct aetags *ae);
static void free_aetags(struct aetags *ae);
static struct htags *new_htags(void);
static struct htags *canon_htags(struct htags *h);
static void free_htags(struct htags *h);
static struct btags *new_btags(void);
static struct btags *canon_btags(struct btags *b);
static void free_btags(struct btags *b);
static struct vtags *new_vtags(void);
static struct vtags *canon_vtags(struct vtags *v);
static void free_vtags(struct vtags *v);
static struct ntags *new_ntags(void);
static struct ntags *canon_ntags(struct ntags *n);
static void free_ntags(struct ntags *n);
static struct dtags *new_dtags(void);
static struct dtags *canon_dtags(struct dtags *d);
static void free_dtags(struct dtags *d);

static int verify_stringlist(stringlist_t *sl, int (*verify)(char *));
static int verify_mailbox(char *s);
static int verify_address(char *s);
static int verify_header(char *s);
static int verify_addrheader(char *s);
static int verify_envelope(char *s);
static int verify_flag(char *s);
static int verify_relat(char *s);
#ifdef ENABLE_REGEX
static int verify_regex(char *s, int cflags);
static int verify_regexs(stringlist_t *sl, char *comp);
#endif
static int verify_utf8(char *s);

int yyerror(char *msg);
extern int yylex(void);
extern void yyrestart(FILE *f);

#define YYERROR_VERBOSE /* i want better error messages! */


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

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 149 "sieve.y"
typedef union YYSTYPE {
    int nval;
    char *sval;
    stringlist_t *sl;
    test_t *test;
    testlist_t *testl;
    commandlist_t *cl;
    struct vtags *vtag;
    struct aetags *aetag;
    struct htags *htag;
    struct btags *btag;
    struct ntags *ntag;
    struct dtags *dtag;
} YYSTYPE;
/* Line 196 of yacc.c.  */
#line 398 "y.tab.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 219 of yacc.c.  */
#line 410 "y.tab.c"

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T) && (defined (__STDC__) || defined (__cplusplus))
# include <stddef.h> /* INFRINGES ON USER NAME SPACE */
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

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

#if ! defined (yyoverflow) || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if defined (__STDC__) || defined (__cplusplus)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     define YYINCLUDED_STDLIB_H
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2005 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM ((YYSIZE_T) -1)
#  endif
#  ifdef __cplusplus
extern "C" {
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if (! defined (malloc) && ! defined (YYINCLUDED_STDLIB_H) \
	&& (defined (__STDC__) || defined (__cplusplus)))
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if (! defined (free) && ! defined (YYINCLUDED_STDLIB_H) \
	&& (defined (__STDC__) || defined (__cplusplus)))
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifdef __cplusplus
}
#  endif
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (defined (YYSTYPE_IS_TRIVIAL) && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short int yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short int) + sizeof (YYSTYPE))			\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined (__GNUC__) && 1 < __GNUC__
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
   typedef short int yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  8
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   248

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  84
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  28
/* YYNRULES -- Number of rules. */
#define YYNRULES  106
/* YYNRULES -- Number of states. */
#define YYNSTATES  164

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   330

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      82,    83,     2,     2,    79,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    76,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    77,     2,    78,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    80,     2,    81,     2,     2,     2,     2,
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
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     5,     8,     9,    12,    16,    18,    21,
      24,    29,    32,    33,    38,    41,    44,    48,    52,    54,
      56,    58,    62,    65,    68,    71,    73,    75,    78,    81,
      85,    87,    88,    90,    92,    93,    97,   101,   105,   108,
     112,   113,   116,   120,   124,   126,   128,   130,   131,   135,
     139,   143,   147,   151,   154,   158,   160,   162,   166,   170,
     173,   176,   179,   182,   184,   186,   191,   196,   200,   203,
     207,   209,   211,   213,   214,   217,   220,   224,   228,   229,
     232,   236,   240,   241,   244,   247,   251,   254,   258,   262,
     264,   266,   268,   270,   272,   274,   276,   278,   280,   282,
     284,   286,   288,   289,   291,   295,   297
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      85,     0,    -1,    86,    -1,    86,    88,    -1,    -1,    87,
      86,    -1,    15,    97,    76,    -1,    89,    -1,    89,    88,
      -1,    91,    76,    -1,     5,   100,    99,    90,    -1,     1,
      76,    -1,    -1,     6,   100,    99,    90,    -1,     7,    99,
      -1,     8,     4,    -1,     9,   109,     4,    -1,    10,   109,
       4,    -1,    11,    -1,    12,    -1,    13,    -1,    14,    96,
       4,    -1,    16,    97,    -1,    17,    97,    -1,    18,    97,
      -1,    19,    -1,    20,    -1,    21,    93,    -1,    22,    94,
      -1,    71,    92,     4,    -1,    74,    -1,    -1,    72,    -1,
      73,    -1,    -1,    93,    64,     4,    -1,    93,    63,     4,
      -1,    93,    65,    97,    -1,    93,    95,    -1,    93,    70,
       4,    -1,    -1,    94,    95,    -1,    94,   106,     4,    -1,
      94,   107,     4,    -1,    66,    -1,    67,    -1,    68,    -1,
      -1,    96,    57,     3,    -1,    96,    58,    97,    -1,    96,
      59,     4,    -1,    96,    60,     4,    -1,    96,    61,     4,
      -1,    96,    62,    -1,    77,    98,    78,    -1,     4,    -1,
       4,    -1,     4,    79,    98,    -1,    80,    88,    81,    -1,
      80,    81,    -1,    23,   110,    -1,    24,   110,    -1,    25,
      97,    -1,    26,    -1,    27,    -1,    28,   103,    97,    97,
      -1,   101,   102,    97,    97,    -1,    33,   104,    97,    -1,
      29,   100,    -1,    30,   108,     3,    -1,     1,    -1,    31,
      -1,    32,    -1,    -1,   102,   105,    -1,   102,   106,    -1,
     102,   107,     4,    -1,   102,    34,     4,    -1,    -1,   103,
     106,    -1,   103,   107,     4,    -1,   103,    34,     4,    -1,
      -1,   104,    54,    -1,   104,    55,    -1,   104,    56,    97,
      -1,   104,   106,    -1,   104,   107,     4,    -1,   104,    34,
       4,    -1,    49,    -1,    50,    -1,    51,    -1,    52,    -1,
      53,    -1,    35,    -1,    36,    -1,    37,    -1,    38,    -1,
      39,    -1,    40,    -1,    41,    -1,    42,    -1,    -1,    75,
      -1,    82,   111,    83,    -1,   100,    -1,   100,    79,   111,
      -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   195,   195,   196,   199,   200,   203,   209,   210,   213,
     214,   215,   218,   219,   220,   223,   232,   240,   244,   245,
     246,   247,   256,   265,   274,   283,   288,   294,   302,   312,
     319,   326,   327,   328,   331,   332,   335,   338,   341,   344,
     349,   350,   353,   368,   378,   379,   380,   383,   384,   387,
     395,   401,   407,   413,   419,   420,   423,   424,   427,   428,
     431,   432,   433,   434,   435,   436,   460,   482,   508,   509,
     511,   514,   515,   522,   523,   528,   532,   540,   550,   551,
     555,   563,   573,   574,   579,   584,   592,   596,   604,   615,
     616,   617,   618,   623,   629,   630,   631,   632,   639,   644,
     652,   653,   656,   657,   664,   667,   668
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "NUMBER", "STRING", "IF", "ELSIF",
  "ELSE", "REJCT", "FILEINTO", "REDIRECT", "KEEP", "STOP", "DISCARD",
  "VACATION", "REQUIRE", "SETFLAG", "ADDFLAG", "REMOVEFLAG", "MARK",
  "UNMARK", "NOTIFY", "DENOTIFY", "ANYOF", "ALLOF", "EXISTS", "SFALSE",
  "STRUE", "HEADER", "NOT", "SIZE", "ADDRESS", "ENVELOPE", "BODY",
  "COMPARATOR", "IS", "CONTAINS", "MATCHES", "REGEX", "COUNT", "VALUE",
  "OVER", "UNDER", "GT", "GE", "LT", "LE", "EQ", "NE", "ALL", "LOCALPART",
  "DOMAIN", "USER", "DETAIL", "RAW", "TEXT", "CONTENT", "DAYS",
  "ADDRESSES", "SUBJECT", "FROM", "HANDLE", "MIME", "METHOD", "ID",
  "OPTIONS", "LOW", "NORMAL", "HIGH", "ANY", "MESSAGE", "INCLUDE",
  "PERSONAL", "GLOBAL", "RETURN", "COPY", "';'", "'['", "']'", "','",
  "'{'", "'}'", "'('", "')'", "$accept", "start", "reqs", "require",
  "commands", "command", "elsif", "action", "location", "ntags", "dtags",
  "priority", "vtags", "stringlist", "strings", "block", "test",
  "addrorenv", "aetags", "htags", "btags", "addrparttag", "comptag",
  "relcomp", "sizetag", "copy", "testlist", "tests", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short int yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,    59,    91,    93,    44,
     123,   125,    40,    41
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    84,    85,    85,    86,    86,    87,    88,    88,    89,
      89,    89,    90,    90,    90,    91,    91,    91,    91,    91,
      91,    91,    91,    91,    91,    91,    91,    91,    91,    91,
      91,    92,    92,    92,    93,    93,    93,    93,    93,    93,
      94,    94,    94,    94,    95,    95,    95,    96,    96,    96,
      96,    96,    96,    96,    97,    97,    98,    98,    99,    99,
     100,   100,   100,   100,   100,   100,   100,   100,   100,   100,
     100,   101,   101,   102,   102,   102,   102,   102,   103,   103,
     103,   103,   104,   104,   104,   104,   104,   104,   104,   105,
     105,   105,   105,   105,   106,   106,   106,   106,   107,   107,
     108,   108,   109,   109,   110,   111,   111
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     2,     0,     2,     3,     1,     2,     2,
       4,     2,     0,     4,     2,     2,     3,     3,     1,     1,
       1,     3,     2,     2,     2,     1,     1,     2,     2,     3,
       1,     0,     1,     1,     0,     3,     3,     3,     2,     3,
       0,     2,     3,     3,     1,     1,     1,     0,     3,     3,
       3,     3,     3,     2,     3,     1,     1,     3,     3,     2,
       2,     2,     2,     1,     1,     4,     4,     3,     2,     3,
       1,     1,     1,     0,     2,     2,     3,     3,     0,     2,
       3,     3,     0,     2,     2,     3,     2,     3,     3,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     0,     1,     3,     1,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       4,     0,     0,     0,     4,    55,     0,     0,     1,     0,
       0,     0,   102,   102,    18,    19,    20,    47,     0,     0,
       0,    25,    26,    34,    40,    31,    30,     3,     0,     0,
       5,    56,     0,     6,    11,    70,     0,     0,     0,    63,
      64,    78,     0,     0,    71,    72,    82,     0,    73,    15,
     103,     0,     0,     0,    22,    23,    24,    27,    28,    32,
      33,     0,     8,     9,     0,    54,     0,    60,    61,    62,
       0,    68,   100,   101,     0,     0,     0,    12,     0,    16,
      17,    21,     0,     0,     0,     0,     0,    53,     0,     0,
       0,    44,    45,    46,     0,    38,    94,    95,    96,    97,
      98,    99,    41,     0,     0,    29,    57,   105,     0,     0,
       0,    79,     0,    69,     0,    83,    84,     0,    67,    86,
       0,    59,     0,     0,     0,    10,     0,    89,    90,    91,
      92,    93,     0,    74,    75,     0,    48,    49,    50,    51,
      52,    36,    35,    37,    39,    42,    43,     0,   104,    81,
      65,    80,    88,    85,    87,    58,     0,    14,    77,    66,
      76,   106,    12,    13
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,     2,     3,     4,    27,    28,   125,    29,    61,    57,
      58,    95,    53,     7,    32,    77,   107,    48,    78,    70,
      75,   133,   103,   104,    74,    51,    67,   108
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -113
static const short int yypact[] =
{
      -4,     0,    54,   112,    -4,  -113,    25,   -20,  -113,   -12,
     215,    55,    -1,    -1,  -113,  -113,  -113,  -113,     0,     0,
       0,  -113,  -113,  -113,  -113,   -64,  -113,  -113,     5,     2,
    -113,     1,    -3,  -113,  -113,  -113,     6,     6,     0,  -113,
    -113,  -113,   215,   -10,  -113,  -113,  -113,     4,  -113,  -113,
    -113,    77,    78,    85,  -113,  -113,  -113,     3,   135,  -113,
    -113,    79,  -113,  -113,    25,  -113,   215,  -113,  -113,  -113,
     160,  -113,  -113,  -113,    82,   153,    29,    56,   101,  -113,
    -113,  -113,    84,     0,    86,    87,    89,  -113,    90,    91,
       0,  -113,  -113,  -113,    92,  -113,  -113,  -113,  -113,  -113,
    -113,  -113,  -113,    93,    94,  -113,  -113,    22,    19,   100,
       0,  -113,   102,  -113,   103,  -113,  -113,     0,  -113,  -113,
     104,  -113,    28,   215,     4,  -113,   107,  -113,  -113,  -113,
    -113,  -113,     0,  -113,  -113,   111,  -113,  -113,  -113,  -113,
    -113,  -113,  -113,  -113,  -113,  -113,  -113,   215,  -113,  -113,
    -113,  -113,  -113,  -113,  -113,  -113,     4,  -113,  -113,  -113,
    -113,  -113,    56,  -113
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
    -113,  -113,   114,  -113,   -21,  -113,   -43,  -113,  -113,  -113,
    -113,    69,  -113,   -18,    95,  -112,    -7,  -113,  -113,  -113,
    -113,  -113,   -42,   -17,  -113,   136,   118,     9
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -8
static const short int yytable[] =
{
      54,    55,    56,    47,     5,    -7,     9,    62,    59,    60,
      10,     1,   157,    11,    12,    13,    14,    15,    16,    17,
      69,    18,    19,    20,    21,    22,    23,    24,   111,    31,
       9,    72,    73,   119,    10,    71,   134,    11,    12,    13,
      14,    15,    16,    17,   162,    18,    19,    20,    21,    22,
      23,    24,   110,   112,     8,   122,    33,   118,   120,    49,
     132,   135,   123,   124,    34,   137,    88,    89,    90,    91,
      92,    93,   143,    94,    50,    65,    25,     6,    63,    26,
      64,    79,    80,   105,    76,   113,    -7,   136,    66,    81,
     138,   139,   150,   140,   141,   142,   144,   145,   146,   153,
      25,   147,   148,    26,   149,     5,   151,   152,   154,   155,
     121,   158,    -2,     9,   159,   160,   156,    10,    30,   163,
      11,    12,    13,    14,    15,    16,    17,   102,    18,    19,
      20,    21,    22,    23,    24,   126,    96,    97,    98,    99,
     100,   101,    82,    83,    84,    85,    86,    87,     0,    52,
     127,   128,   129,   130,   131,    68,   161,     5,     0,   106,
       0,     0,     0,     0,     5,     0,     0,     0,     0,     0,
      96,    97,    98,    99,   100,   101,     0,     0,     6,     0,
       0,     0,     0,    25,     0,     0,    26,   114,    96,    97,
      98,    99,   100,   101,   109,    96,    97,    98,    99,   100,
     101,    91,    92,    93,     0,     0,     0,   115,   116,   117,
       0,     0,     0,     0,     0,     0,    35,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       6,     0,     0,     0,     0,     0,     0,     6,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46
};

static const short int yycheck[] =
{
      18,    19,    20,    10,     4,     0,     1,    28,    72,    73,
       5,    15,   124,     8,     9,    10,    11,    12,    13,    14,
      38,    16,    17,    18,    19,    20,    21,    22,    70,     4,
       1,    41,    42,    75,     5,    42,    78,     8,     9,    10,
      11,    12,    13,    14,   156,    16,    17,    18,    19,    20,
      21,    22,    70,    70,     0,    76,    76,    75,    75,     4,
      78,    78,     6,     7,    76,    83,    63,    64,    65,    66,
      67,    68,    90,    70,    75,    78,    71,    77,    76,    74,
      79,     4,     4,     4,    80,     3,    81,     3,    82,     4,
       4,     4,   110,     4,     4,     4,     4,     4,     4,   117,
      71,    79,    83,    74,     4,     4,     4,     4,     4,    81,
      81,     4,     0,     1,   132,     4,   123,     5,     4,   162,
       8,     9,    10,    11,    12,    13,    14,    58,    16,    17,
      18,    19,    20,    21,    22,    34,    35,    36,    37,    38,
      39,    40,    57,    58,    59,    60,    61,    62,    -1,    13,
      49,    50,    51,    52,    53,    37,   147,     4,    -1,    64,
      -1,    -1,    -1,    -1,     4,    -1,    -1,    -1,    -1,    -1,
      35,    36,    37,    38,    39,    40,    -1,    -1,    77,    -1,
      -1,    -1,    -1,    71,    -1,    -1,    74,    34,    35,    36,
      37,    38,    39,    40,    34,    35,    36,    37,    38,    39,
      40,    66,    67,    68,    -1,    -1,    -1,    54,    55,    56,
      -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      77,    -1,    -1,    -1,    -1,    -1,    -1,    77,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    15,    85,    86,    87,     4,    77,    97,     0,     1,
       5,     8,     9,    10,    11,    12,    13,    14,    16,    17,
      18,    19,    20,    21,    22,    71,    74,    88,    89,    91,
      86,     4,    98,    76,    76,     1,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,   100,   101,     4,
      75,   109,   109,    96,    97,    97,    97,    93,    94,    72,
      73,    92,    88,    76,    79,    78,    82,   110,   110,    97,
     103,   100,    41,    42,   108,   104,    80,    99,   102,     4,
       4,     4,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    70,    95,    35,    36,    37,    38,
      39,    40,    95,   106,   107,     4,    98,   100,   111,    34,
      97,   106,   107,     3,    34,    54,    55,    56,    97,   106,
     107,    81,    88,     6,     7,    90,    34,    49,    50,    51,
      52,    53,    97,   105,   106,   107,     3,    97,     4,     4,
       4,     4,     4,    97,     4,     4,     4,    79,    83,     4,
      97,     4,     4,    97,     4,    81,   100,    99,     4,    97,
       4,   111,    99,    90
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
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (0)


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (N)								\
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
    while (0)
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
} while (0)

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr,					\
                  Type, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short int *bottom, short int *top)
#else
static void
yy_stack_print (bottom, top)
    short int *bottom;
    short int *top;
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
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu), ",
             yyrule - 1, yylno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname[yyr1[yyrule]]);
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
  const char *yys = yystr;

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
      size_t yyn = 0;
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

#endif /* YYERROR_VERBOSE */



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
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);


# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

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
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  short int *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



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
	short int *yyss1 = yyss;


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
	short int *yyss1 = yyss;
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

/* Do appropriate processing given the current state.  */
/* Read a look-ahead token if we need one and don't already have one.  */
/* yyresume: */

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

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

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
        case 2:
#line 195 "sieve.y"
    { ret = NULL; }
    break;

  case 3:
#line 196 "sieve.y"
    { ret = (yyvsp[0].cl); }
    break;

  case 6:
#line 203 "sieve.y"
    { if (!check_reqs((yyvsp[-1].sl))) {
                                    yyerror("unsupported feature");
				    YYERROR; 
                                  } }
    break;

  case 7:
#line 209 "sieve.y"
    { (yyval.cl) = (yyvsp[0].cl); }
    break;

  case 8:
#line 210 "sieve.y"
    { (yyvsp[-1].cl)->next = (yyvsp[0].cl); (yyval.cl) = (yyvsp[-1].cl); }
    break;

  case 9:
#line 213 "sieve.y"
    { (yyval.cl) = (yyvsp[-1].cl); }
    break;

  case 10:
#line 214 "sieve.y"
    { (yyval.cl) = new_if((yyvsp[-2].test), (yyvsp[-1].cl), (yyvsp[0].cl)); }
    break;

  case 11:
#line 215 "sieve.y"
    { (yyval.cl) = new_command(STOP); }
    break;

  case 12:
#line 218 "sieve.y"
    { (yyval.cl) = NULL; }
    break;

  case 13:
#line 219 "sieve.y"
    { (yyval.cl) = new_if((yyvsp[-2].test), (yyvsp[-1].cl), (yyvsp[0].cl)); }
    break;

  case 14:
#line 220 "sieve.y"
    { (yyval.cl) = (yyvsp[0].cl); }
    break;

  case 15:
#line 223 "sieve.y"
    { if (!parse_script->support.reject) {
				     yyerror("reject not required");
				     YYERROR;
				   }
				   if (!verify_utf8((yyvsp[0].sval))) {
				     YYERROR; /* vu should call yyerror() */
				   }
				   (yyval.cl) = new_command(REJCT);
				   (yyval.cl)->u.str = (yyvsp[0].sval); }
    break;

  case 16:
#line 232 "sieve.y"
    { if (!parse_script->support.fileinto) {
				     yyerror("fileinto not required");
	                             YYERROR;
                                   }
				   if (!verify_mailbox((yyvsp[0].sval))) {
				     YYERROR; /* vm should call yyerror() */
				   }
	                           (yyval.cl) = build_fileinto(FILEINTO, (yyvsp[-1].nval), (yyvsp[0].sval)); }
    break;

  case 17:
#line 240 "sieve.y"
    { if (!verify_address((yyvsp[0].sval))) {
				     YYERROR; /* va should call yyerror() */
				   }
	                           (yyval.cl) = build_redirect(REDIRECT, (yyvsp[-1].nval), (yyvsp[0].sval)); }
    break;

  case 18:
#line 244 "sieve.y"
    { (yyval.cl) = new_command(KEEP); }
    break;

  case 19:
#line 245 "sieve.y"
    { (yyval.cl) = new_command(STOP); }
    break;

  case 20:
#line 246 "sieve.y"
    { (yyval.cl) = new_command(DISCARD); }
    break;

  case 21:
#line 247 "sieve.y"
    { if (!parse_script->support.vacation) {
				     yyerror("vacation not required");
				     YYERROR;
				   }
				   if (((yyvsp[-1].vtag)->mime == -1) && !verify_utf8((yyvsp[0].sval))) {
				     YYERROR; /* vu should call yyerror() */
				   }
  				   (yyval.cl) = build_vacation(VACATION,
					    canon_vtags((yyvsp[-1].vtag)), (yyvsp[0].sval)); }
    break;

  case 22:
#line 256 "sieve.y"
    { if (!parse_script->support.imapflags) {
                                    yyerror("imapflags not required");
                                    YYERROR;
                                   }
                                  if (!verify_stringlist((yyvsp[0].sl), verify_flag)) {
                                    YYERROR; /* vf should call yyerror() */
                                  }
                                  (yyval.cl) = new_command(SETFLAG);
                                  (yyval.cl)->u.sl = (yyvsp[0].sl); }
    break;

  case 23:
#line 265 "sieve.y"
    { if (!parse_script->support.imapflags) {
                                    yyerror("imapflags not required");
                                    YYERROR;
                                    }
                                  if (!verify_stringlist((yyvsp[0].sl), verify_flag)) {
                                    YYERROR; /* vf should call yyerror() */
                                  }
                                  (yyval.cl) = new_command(ADDFLAG);
                                  (yyval.cl)->u.sl = (yyvsp[0].sl); }
    break;

  case 24:
#line 274 "sieve.y"
    { if (!parse_script->support.imapflags) {
                                    yyerror("imapflags not required");
                                    YYERROR;
                                    }
                                  if (!verify_stringlist((yyvsp[0].sl), verify_flag)) {
                                    YYERROR; /* vf should call yyerror() */
                                  }
                                  (yyval.cl) = new_command(REMOVEFLAG);
                                  (yyval.cl)->u.sl = (yyvsp[0].sl); }
    break;

  case 25:
#line 283 "sieve.y"
    { if (!parse_script->support.imapflags) {
                                    yyerror("imapflags not required");
                                    YYERROR;
                                    }
                                  (yyval.cl) = new_command(MARK); }
    break;

  case 26:
#line 288 "sieve.y"
    { if (!parse_script->support.imapflags) {
                                    yyerror("imapflags not required");
                                    YYERROR;
                                    }
                                  (yyval.cl) = new_command(UNMARK); }
    break;

  case 27:
#line 294 "sieve.y"
    { if (!parse_script->support.notify) {
				       yyerror("notify not required");
				       (yyval.cl) = new_command(NOTIFY); 
				       YYERROR;
	 			    } else {
				      (yyval.cl) = build_notify(NOTIFY,
				             canon_ntags((yyvsp[0].ntag)));
				    } }
    break;

  case 28:
#line 302 "sieve.y"
    { if (!parse_script->support.notify) {
                                       yyerror("notify not required");
				       (yyval.cl) = new_command(DENOTIFY);
				       YYERROR;
				    } else {
					(yyval.cl) = build_denotify(DENOTIFY, canon_dtags((yyvsp[0].dtag)));
					if ((yyval.cl) == NULL) { 
			yyerror("unable to find a compatible comparator");
			YYERROR; } } }
    break;

  case 29:
#line 312 "sieve.y"
    { if (!parse_script->support.include) {
				     yyerror("include not required");
	                             YYERROR;
                                   }
	                           (yyval.cl) = new_command(INCLUDE);
				   (yyval.cl)->u.inc.location = (yyvsp[-1].nval);
				   (yyval.cl)->u.inc.script = (yyvsp[0].sval); }
    break;

  case 30:
#line 319 "sieve.y"
    { if (!parse_script->support.include) {
                                    yyerror("include not required");
                                    YYERROR;
                                  }
                                   (yyval.cl) = new_command(RETURN); }
    break;

  case 31:
#line 326 "sieve.y"
    { (yyval.nval) = PERSONAL; }
    break;

  case 32:
#line 327 "sieve.y"
    { (yyval.nval) = PERSONAL; }
    break;

  case 33:
#line 328 "sieve.y"
    { (yyval.nval) = GLOBAL; }
    break;

  case 34:
#line 331 "sieve.y"
    { (yyval.ntag) = new_ntags(); }
    break;

  case 35:
#line 332 "sieve.y"
    { if ((yyval.ntag)->id != NULL) { 
					yyerror("duplicate :method"); YYERROR; }
				   else { (yyval.ntag)->id = (yyvsp[0].sval); } }
    break;

  case 36:
#line 335 "sieve.y"
    { if ((yyval.ntag)->method != NULL) { 
					yyerror("duplicate :method"); YYERROR; }
				   else { (yyval.ntag)->method = (yyvsp[0].sval); } }
    break;

  case 37:
#line 338 "sieve.y"
    { if ((yyval.ntag)->options != NULL) { 
					yyerror("duplicate :options"); YYERROR; }
				     else { (yyval.ntag)->options = (yyvsp[0].sl); } }
    break;

  case 38:
#line 341 "sieve.y"
    { if ((yyval.ntag)->priority != -1) { 
                                 yyerror("duplicate :priority"); YYERROR; }
                                   else { (yyval.ntag)->priority = (yyvsp[0].nval); } }
    break;

  case 39:
#line 344 "sieve.y"
    { if ((yyval.ntag)->message != NULL) { 
					yyerror("duplicate :message"); YYERROR; }
				   else { (yyval.ntag)->message = (yyvsp[0].sval); } }
    break;

  case 40:
#line 349 "sieve.y"
    { (yyval.dtag) = new_dtags(); }
    break;

  case 41:
#line 350 "sieve.y"
    { if ((yyval.dtag)->priority != -1) { 
				yyerror("duplicate priority level"); YYERROR; }
				   else { (yyval.dtag)->priority = (yyvsp[0].nval); } }
    break;

  case 42:
#line 353 "sieve.y"
    { if ((yyval.dtag)->comptag != -1)
	                             { 
					 yyerror("duplicate comparator type tag"); YYERROR;
				     }
	                           (yyval.dtag)->comptag = (yyvsp[-1].nval);
#ifdef ENABLE_REGEX
				   if ((yyval.dtag)->comptag == REGEX)
				   {
				       int cflags = REG_EXTENDED |
					   REG_NOSUB | REG_ICASE;
				       if (!verify_regex((yyvsp[0].sval), cflags)) { YYERROR; }
				   }
#endif
				   (yyval.dtag)->pattern = (yyvsp[0].sval);
	                          }
    break;

  case 43:
#line 368 "sieve.y"
    { (yyval.dtag) = (yyvsp[-2].dtag);
				   if ((yyval.dtag)->comptag != -1) { 
			yyerror("duplicate comparator type tag"); YYERROR; }
				   else { (yyval.dtag)->comptag = (yyvsp[-1].nval);
				   (yyval.dtag)->relation = verify_relat((yyvsp[0].sval));
				   if ((yyval.dtag)->relation==-1) 
				     {YYERROR; /*vr called yyerror()*/ }
				   } }
    break;

  case 44:
#line 378 "sieve.y"
    { (yyval.nval) = LOW; }
    break;

  case 45:
#line 379 "sieve.y"
    { (yyval.nval) = NORMAL; }
    break;

  case 46:
#line 380 "sieve.y"
    { (yyval.nval) = HIGH; }
    break;

  case 47:
#line 383 "sieve.y"
    { (yyval.vtag) = new_vtags(); }
    break;

  case 48:
#line 384 "sieve.y"
    { if ((yyval.vtag)->days != -1) { 
					yyerror("duplicate :days"); YYERROR; }
				   else { (yyval.vtag)->days = (yyvsp[0].nval); } }
    break;

  case 49:
#line 387 "sieve.y"
    { if ((yyval.vtag)->addresses != NULL) { 
					yyerror("duplicate :addresses"); 
					YYERROR;
				       } else if (!verify_stringlist((yyvsp[0].sl),
							verify_address)) {
					  YYERROR;
				       } else {
					 (yyval.vtag)->addresses = (yyvsp[0].sl); } }
    break;

  case 50:
#line 395 "sieve.y"
    { if ((yyval.vtag)->subject != NULL) { 
					yyerror("duplicate :subject"); 
					YYERROR;
				   } else if (!verify_utf8((yyvsp[0].sval))) {
				        YYERROR; /* vu should call yyerror() */
				   } else { (yyval.vtag)->subject = (yyvsp[0].sval); } }
    break;

  case 51:
#line 401 "sieve.y"
    { if ((yyval.vtag)->from != NULL) { 
					yyerror("duplicate :from"); 
					YYERROR;
				   } else if (!verify_address((yyvsp[0].sval))) {
				        YYERROR; /* vu should call yyerror() */
				   } else { (yyval.vtag)->from = (yyvsp[0].sval); } }
    break;

  case 52:
#line 407 "sieve.y"
    { if ((yyval.vtag)->handle != NULL) { 
					yyerror("duplicate :handle"); 
					YYERROR;
				   } else if (!verify_utf8((yyvsp[0].sval))) {
				        YYERROR; /* vu should call yyerror() */
				   } else { (yyval.vtag)->handle = (yyvsp[0].sval); } }
    break;

  case 53:
#line 413 "sieve.y"
    { if ((yyval.vtag)->mime != -1) { 
					yyerror("duplicate :mime"); 
					YYERROR; }
				   else { (yyval.vtag)->mime = MIME; } }
    break;

  case 54:
#line 419 "sieve.y"
    { (yyval.sl) = (yyvsp[-1].sl); }
    break;

  case 55:
#line 420 "sieve.y"
    { (yyval.sl) = new_sl((yyvsp[0].sval), NULL); }
    break;

  case 56:
#line 423 "sieve.y"
    { (yyval.sl) = new_sl((yyvsp[0].sval), NULL); }
    break;

  case 57:
#line 424 "sieve.y"
    { (yyval.sl) = new_sl((yyvsp[-2].sval), (yyvsp[0].sl)); }
    break;

  case 58:
#line 427 "sieve.y"
    { (yyval.cl) = (yyvsp[-1].cl); }
    break;

  case 59:
#line 428 "sieve.y"
    { (yyval.cl) = NULL; }
    break;

  case 60:
#line 431 "sieve.y"
    { (yyval.test) = new_test(ANYOF); (yyval.test)->u.tl = (yyvsp[0].testl); }
    break;

  case 61:
#line 432 "sieve.y"
    { (yyval.test) = new_test(ALLOF); (yyval.test)->u.tl = (yyvsp[0].testl); }
    break;

  case 62:
#line 433 "sieve.y"
    { (yyval.test) = new_test(EXISTS); (yyval.test)->u.sl = (yyvsp[0].sl); }
    break;

  case 63:
#line 434 "sieve.y"
    { (yyval.test) = new_test(SFALSE); }
    break;

  case 64:
#line 435 "sieve.y"
    { (yyval.test) = new_test(STRUE); }
    break;

  case 65:
#line 437 "sieve.y"
    {
				     if (!verify_stringlist((yyvsp[-1].sl), verify_header)) {
					 YYERROR; /* vh should call yyerror() */
				     }
				     if (!verify_stringlist((yyvsp[0].sl), verify_utf8)) {
					 YYERROR; /* vu should call yyerror() */
				     }
				     
				     (yyvsp[-2].htag) = canon_htags((yyvsp[-2].htag));
#ifdef ENABLE_REGEX
				     if ((yyvsp[-2].htag)->comptag == REGEX)
				     {
					 if (!(verify_regexs((yyvsp[0].sl), (yyvsp[-2].htag)->comparator)))
					 { YYERROR; }
				     }
#endif
				     (yyval.test) = build_header(HEADER, (yyvsp[-2].htag), (yyvsp[-1].sl), (yyvsp[0].sl));
				     if ((yyval.test) == NULL) { 
					 yyerror("unable to find a compatible comparator");
					 YYERROR; } 
				 }
    break;

  case 66:
#line 461 "sieve.y"
    { 
				     if (((yyvsp[-3].nval) == ADDRESS) &&
					 !verify_stringlist((yyvsp[-1].sl), verify_addrheader))
					 { YYERROR; }
				     else if (((yyvsp[-3].nval) == ENVELOPE) &&
					      !verify_stringlist((yyvsp[-1].sl), verify_envelope))
					 { YYERROR; }
				     (yyvsp[-2].aetag) = canon_aetags((yyvsp[-2].aetag));
#ifdef ENABLE_REGEX
				     if ((yyvsp[-2].aetag)->comptag == REGEX)
				     {
					 if (!( verify_regexs((yyvsp[0].sl), (yyvsp[-2].aetag)->comparator)))
					 { YYERROR; }
				     }
#endif
				     (yyval.test) = build_address((yyvsp[-3].nval), (yyvsp[-2].aetag), (yyvsp[-1].sl), (yyvsp[0].sl));
				     if ((yyval.test) == NULL) { 
					 yyerror("unable to find a compatible comparator");
					 YYERROR; } 
				 }
    break;

  case 67:
#line 483 "sieve.y"
    {
				     if (!parse_script->support.body) {
                                       yyerror("body not required");
				       YYERROR;
				     }
					
				     if (!verify_stringlist((yyvsp[0].sl), verify_utf8)) {
					 YYERROR; /* vu should call yyerror() */
				     }
				     
				     (yyvsp[-1].btag) = canon_btags((yyvsp[-1].btag));
#ifdef ENABLE_REGEX
				     if ((yyvsp[-1].btag)->comptag == REGEX)
				     {
					 if (!(verify_regexs((yyvsp[0].sl), (yyvsp[-1].btag)->comparator)))
					 { YYERROR; }
				     }
#endif
				     (yyval.test) = build_body(BODY, (yyvsp[-1].btag), (yyvsp[0].sl));
				     if ((yyval.test) == NULL) { 
					 yyerror("unable to find a compatible comparator");
					 YYERROR; } 
				 }
    break;

  case 68:
#line 508 "sieve.y"
    { (yyval.test) = new_test(NOT); (yyval.test)->u.t = (yyvsp[0].test); }
    break;

  case 69:
#line 509 "sieve.y"
    { (yyval.test) = new_test(SIZE); (yyval.test)->u.sz.t = (yyvsp[-1].nval);
		                   (yyval.test)->u.sz.n = (yyvsp[0].nval); }
    break;

  case 70:
#line 511 "sieve.y"
    { (yyval.test) = NULL; }
    break;

  case 71:
#line 514 "sieve.y"
    { (yyval.nval) = ADDRESS; }
    break;

  case 72:
#line 515 "sieve.y"
    {if (!parse_script->support.envelope)
	                              {yyerror("envelope not required"); YYERROR;}
	                          else{(yyval.nval) = ENVELOPE; }
	                         }
    break;

  case 73:
#line 522 "sieve.y"
    { (yyval.aetag) = new_aetags(); }
    break;

  case 74:
#line 523 "sieve.y"
    { (yyval.aetag) = (yyvsp[-1].aetag);
				   if ((yyval.aetag)->addrtag != -1) { 
			yyerror("duplicate or conflicting address part tag");
			YYERROR; }
				   else { (yyval.aetag)->addrtag = (yyvsp[0].nval); } }
    break;

  case 75:
#line 528 "sieve.y"
    { (yyval.aetag) = (yyvsp[-1].aetag);
				   if ((yyval.aetag)->comptag != -1) { 
			yyerror("duplicate comparator type tag"); YYERROR; }
				   else { (yyval.aetag)->comptag = (yyvsp[0].nval); } }
    break;

  case 76:
#line 532 "sieve.y"
    { (yyval.aetag) = (yyvsp[-2].aetag);
				   if ((yyval.aetag)->comptag != -1) { 
			yyerror("duplicate comparator type tag"); YYERROR; }
				   else { (yyval.aetag)->comptag = (yyvsp[-1].nval);
				   (yyval.aetag)->relation = verify_relat((yyvsp[0].sval));
				   if ((yyval.aetag)->relation==-1) 
				     {YYERROR; /*vr called yyerror()*/ }
				   } }
    break;

  case 77:
#line 540 "sieve.y"
    { (yyval.aetag) = (yyvsp[-2].aetag);
	if ((yyval.aetag)->comparator != NULL) { 
			yyerror("duplicate comparator tag"); YYERROR; }
				   else if (!strcmp((yyvsp[0].sval), "i;ascii-numeric") &&
					    !parse_script->support.i_ascii_numeric) {
			yyerror("comparator-i;ascii-numeric not required");
			YYERROR; }
				   else { (yyval.aetag)->comparator = (yyvsp[0].sval); } }
    break;

  case 78:
#line 550 "sieve.y"
    { (yyval.htag) = new_htags(); }
    break;

  case 79:
#line 551 "sieve.y"
    { (yyval.htag) = (yyvsp[-1].htag);
				   if ((yyval.htag)->comptag != -1) { 
			yyerror("duplicate comparator type tag"); YYERROR; }
				   else { (yyval.htag)->comptag = (yyvsp[0].nval); } }
    break;

  case 80:
#line 555 "sieve.y"
    { (yyval.htag) = (yyvsp[-2].htag);
				   if ((yyval.htag)->comptag != -1) { 
			yyerror("duplicate comparator type tag"); YYERROR; }
				   else { (yyval.htag)->comptag = (yyvsp[-1].nval);
				   (yyval.htag)->relation = verify_relat((yyvsp[0].sval));
				   if ((yyval.htag)->relation==-1) 
				     {YYERROR; /*vr called yyerror()*/ }
				   } }
    break;

  case 81:
#line 563 "sieve.y"
    { (yyval.htag) = (yyvsp[-2].htag);
				   if ((yyval.htag)->comparator != NULL) { 
			 yyerror("duplicate comparator tag"); YYERROR; }
				   else if (!strcmp((yyvsp[0].sval), "i;ascii-numeric") &&
					    !parse_script->support.i_ascii_numeric) { 
			 yyerror("comparator-i;ascii-numeric not required");  YYERROR; }
				   else { 
				     (yyval.htag)->comparator = (yyvsp[0].sval); } }
    break;

  case 82:
#line 573 "sieve.y"
    { (yyval.btag) = new_btags(); }
    break;

  case 83:
#line 574 "sieve.y"
    { (yyval.btag) = (yyvsp[-1].btag);
				   if ((yyval.btag)->transform != -1) {
			yyerror("duplicate or conflicting transform tag");
			YYERROR; }
				   else { (yyval.btag)->transform = RAW; } }
    break;

  case 84:
#line 579 "sieve.y"
    { (yyval.btag) = (yyvsp[-1].btag);
				   if ((yyval.btag)->transform != -1) {
			yyerror("duplicate or conflicting transform tag");
			YYERROR; }
				   else { (yyval.btag)->transform = TEXT; } }
    break;

  case 85:
#line 584 "sieve.y"
    { (yyval.btag) = (yyvsp[-2].btag);
				   if ((yyval.btag)->transform != -1) {
			yyerror("duplicate or conflicting transform tag");
			YYERROR; }
				   else {
				       (yyval.btag)->transform = CONTENT;
				       (yyval.btag)->content_types = (yyvsp[0].sl);
				   } }
    break;

  case 86:
#line 592 "sieve.y"
    { (yyval.btag) = (yyvsp[-1].btag);
				   if ((yyval.btag)->comptag != -1) { 
			yyerror("duplicate comparator type tag"); YYERROR; }
				   else { (yyval.btag)->comptag = (yyvsp[0].nval); } }
    break;

  case 87:
#line 596 "sieve.y"
    { (yyval.btag) = (yyvsp[-2].btag);
				   if ((yyval.btag)->comptag != -1) { 
			yyerror("duplicate comparator type tag"); YYERROR; }
				   else { (yyval.btag)->comptag = (yyvsp[-1].nval);
				   (yyval.btag)->relation = verify_relat((yyvsp[0].sval));
				   if ((yyval.btag)->relation==-1) 
				     {YYERROR; /*vr called yyerror()*/ }
				   } }
    break;

  case 88:
#line 604 "sieve.y"
    { (yyval.btag) = (yyvsp[-2].btag);
				   if ((yyval.btag)->comparator != NULL) { 
			 yyerror("duplicate comparator tag"); YYERROR; }
				   else if (!strcmp((yyvsp[0].sval), "i;ascii-numeric") &&
					    !parse_script->support.i_ascii_numeric) { 
			 yyerror("comparator-i;ascii-numeric not required");  YYERROR; }
				   else { 
				     (yyval.btag)->comparator = (yyvsp[0].sval); } }
    break;

  case 89:
#line 615 "sieve.y"
    { (yyval.nval) = ALL; }
    break;

  case 90:
#line 616 "sieve.y"
    { (yyval.nval) = LOCALPART; }
    break;

  case 91:
#line 617 "sieve.y"
    { (yyval.nval) = DOMAIN; }
    break;

  case 92:
#line 618 "sieve.y"
    { if (!parse_script->support.subaddress) {
				     yyerror("subaddress not required");
				     YYERROR;
				   }
				   (yyval.nval) = USER; }
    break;

  case 93:
#line 623 "sieve.y"
    { if (!parse_script->support.subaddress) {
				     yyerror("subaddress not required");
				     YYERROR;
				   }
				   (yyval.nval) = DETAIL; }
    break;

  case 94:
#line 629 "sieve.y"
    { (yyval.nval) = IS; }
    break;

  case 95:
#line 630 "sieve.y"
    { (yyval.nval) = CONTAINS; }
    break;

  case 96:
#line 631 "sieve.y"
    { (yyval.nval) = MATCHES; }
    break;

  case 97:
#line 632 "sieve.y"
    { if (!parse_script->support.regex) {
				     yyerror("regex not required");
				     YYERROR;
				   }
				   (yyval.nval) = REGEX; }
    break;

  case 98:
#line 639 "sieve.y"
    { if (!parse_script->support.relational) {
				     yyerror("relational not required");
				     YYERROR;
				   }
				   (yyval.nval) = COUNT; }
    break;

  case 99:
#line 644 "sieve.y"
    { if (!parse_script->support.relational) {
				     yyerror("relational not required");
				     YYERROR;
				   }
				   (yyval.nval) = VALUE; }
    break;

  case 100:
#line 652 "sieve.y"
    { (yyval.nval) = OVER; }
    break;

  case 101:
#line 653 "sieve.y"
    { (yyval.nval) = UNDER; }
    break;

  case 102:
#line 656 "sieve.y"
    { (yyval.nval) = 0; }
    break;

  case 103:
#line 657 "sieve.y"
    { if (!parse_script->support.copy) {
				     yyerror("copy not required");
	                             YYERROR;
                                   }
				   (yyval.nval) = COPY; }
    break;

  case 104:
#line 664 "sieve.y"
    { (yyval.testl) = (yyvsp[-1].testl); }
    break;

  case 105:
#line 667 "sieve.y"
    { (yyval.testl) = new_testlist((yyvsp[0].test), NULL); }
    break;

  case 106:
#line 668 "sieve.y"
    { (yyval.testl) = new_testlist((yyvsp[-2].test), (yyvsp[0].testl)); }
    break;


      default: break;
    }

/* Line 1126 of yacc.c.  */
#line 2403 "y.tab.c"

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
	  int yytype = YYTRANSLATE (yychar);
	  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
	  YYSIZE_T yysize = yysize0;
	  YYSIZE_T yysize1;
	  int yysize_overflow = 0;
	  char *yymsg = 0;
#	  define YYERROR_VERBOSE_ARGS_MAXIMUM 5
	  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
	  int yyx;

#if 0
	  /* This is so xgettext sees the translatable formats that are
	     constructed on the fly.  */
	  YY_("syntax error, unexpected %s");
	  YY_("syntax error, unexpected %s, expecting %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s or %s");
	  YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
#endif
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
	  int yychecklim = YYLAST - yyn;
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
		yysize_overflow |= yysize1 < yysize;
		yysize = yysize1;
		yyfmt = yystpcpy (yyfmt, yyprefix);
		yyprefix = yyor;
	      }

	  yyf = YY_(yyformat);
	  yysize1 = yysize + yystrlen (yyf);
	  yysize_overflow |= yysize1 < yysize;
	  yysize = yysize1;

	  if (!yysize_overflow && yysize <= YYSTACK_ALLOC_MAXIMUM)
	    yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg)
	    {
	      /* Avoid sprintf, as that infringes on the user's name space.
		 Don't have undefined behavior even if the translation
		 produced a string with the wrong number of "%s"s.  */
	      char *yyp = yymsg;
	      int yyi = 0;
	      while ((*yyp = *yyf))
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
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    {
	      yyerror (YY_("syntax error"));
	      goto yyexhaustedlab;
	    }
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror (YY_("syntax error"));
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
	  yydestruct ("Error: discarding", yytoken, &yylval);
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
  if (0)
     goto yyerrorlab;

yyvsp -= yylen;
  yyssp -= yylen;
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


      yydestruct ("Error: popping", yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token. */
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
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK;
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 671 "sieve.y"

commandlist_t *sieve_parse(sieve_script_t *script, FILE *f)
{
    commandlist_t *t;

    parse_script = script;
    yyrestart(f);
    if (yyparse()) {
	t = NULL;
    } else {
	t = ret;
    }
    ret = NULL;
    return t;
}

int yyerror(char *msg)
{
    extern int yylineno;
    int ret;

    parse_script->err++;
    if (parse_script->interp.err) {
	ret = parse_script->interp.err(yylineno, msg, 
				       parse_script->interp.interp_context,
				       parse_script->script_context);
    }

    return 0;
}

static int check_reqs(stringlist_t *sl)
{
    int i = 1;
    stringlist_t *s;
    
    while (sl != NULL) {
	s = sl;
	sl = sl->next;

	i &= script_require(parse_script, s->s);

	if (s->s) free(s->s);
	free(s);
    }
    return i;
}

static test_t *build_address(int t, struct aetags *ae,
			     stringlist_t *sl, stringlist_t *pl)
{
    test_t *ret = new_test(t);	/* can be either ADDRESS or ENVELOPE */

    assert((t == ADDRESS) || (t == ENVELOPE));

    if (ret) {
	ret->u.ae.comptag = ae->comptag;
	ret->u.ae.relation=ae->relation;
	ret->u.ae.comparator=xstrdup(ae->comparator);
	ret->u.ae.sl = sl;
	ret->u.ae.pl = pl;
	ret->u.ae.addrpart = ae->addrtag;
	free_aetags(ae);

    }
    return ret;
}

static test_t *build_header(int t, struct htags *h,
			    stringlist_t *sl, stringlist_t *pl)
{
    test_t *ret = new_test(t);	/* can be HEADER */

    assert(t == HEADER);

    if (ret) {
	ret->u.h.comptag = h->comptag;
	ret->u.h.relation=h->relation;
	ret->u.h.comparator=xstrdup(h->comparator);
	ret->u.h.sl = sl;
	ret->u.h.pl = pl;
	free_htags(h);
    }
    return ret;
}

static test_t *build_body(int t, struct btags *b, stringlist_t *pl)
{
    test_t *ret = new_test(t);	/* can be BODY */

    assert(t == BODY);

    if (ret) {
	ret->u.b.comptag = b->comptag;
	ret->u.b.relation = b->relation;
	ret->u.b.comparator = xstrdup(b->comparator);
	ret->u.b.transform = b->transform;
	ret->u.b.offset = b->offset;
	ret->u.b.content_types = b->content_types; b->content_types = NULL;
	ret->u.b.pl = pl;
	free_btags(b);
    }
    return ret;
}

static commandlist_t *build_vacation(int t, struct vtags *v, char *reason)
{
    commandlist_t *ret = new_command(t);

    assert(t == VACATION);

    if (ret) {
	ret->u.v.subject = v->subject; v->subject = NULL;
	ret->u.v.from = v->from; v->from = NULL;
	ret->u.v.handle = v->handle; v->handle = NULL;
	ret->u.v.days = v->days;
	ret->u.v.mime = v->mime;
	ret->u.v.addresses = v->addresses; v->addresses = NULL;
	free_vtags(v);
	ret->u.v.message = reason;
    }
    return ret;
}

static commandlist_t *build_notify(int t, struct ntags *n)
{
    commandlist_t *ret = new_command(t);

    assert(t == NOTIFY);
       if (ret) {
	ret->u.n.method = n->method; n->method = NULL;
	ret->u.n.id = n->id; n->id = NULL;
	ret->u.n.options = n->options; n->options = NULL;
	ret->u.n.priority = n->priority;
	ret->u.n.message = n->message; n->message = NULL;
	free_ntags(n);
    }
    return ret;
}

static commandlist_t *build_denotify(int t, struct dtags *d)
{
    commandlist_t *ret = new_command(t);

    assert(t == DENOTIFY);

    if (ret) {
	ret->u.d.comptag = d->comptag;
	ret->u.d.relation=d->relation;
	ret->u.d.pattern = d->pattern; d->pattern = NULL;
	ret->u.d.priority = d->priority;
	free_dtags(d);
    }
    return ret;
}

static commandlist_t *build_fileinto(int t, int copy, char *folder)
{
    commandlist_t *ret = new_command(t);

    assert(t == FILEINTO);

    if (ret) {
	ret->u.f.copy = copy;
	ret->u.f.folder = folder;
    }
    return ret;
}

static commandlist_t *build_redirect(int t, int copy, char *address)
{
    commandlist_t *ret = new_command(t);

    assert(t == REDIRECT);

    if (ret) {
	ret->u.r.copy = copy;
	ret->u.r.address = address;
    }
    return ret;
}

static struct aetags *new_aetags(void)
{
    struct aetags *r = (struct aetags *) xmalloc(sizeof(struct aetags));

    r->addrtag = r->comptag = r->relation=-1;
    r->comparator=NULL;

    return r;
}

static struct aetags *canon_aetags(struct aetags *ae)
{
    if (ae->addrtag == -1) { ae->addrtag = ALL; }
    if (ae->comparator == NULL) {
        ae->comparator = xstrdup("i;ascii-casemap");
    }
    if (ae->comptag == -1) { ae->comptag = IS; }
    return ae;
}

static void free_aetags(struct aetags *ae)
{
    free(ae->comparator);
     free(ae);
}

static struct htags *new_htags(void)
{
    struct htags *r = (struct htags *) xmalloc(sizeof(struct htags));

    r->comptag = r->relation= -1;
    
    r->comparator = NULL;

    return r;
}

static struct htags *canon_htags(struct htags *h)
{
    if (h->comparator == NULL) {
	h->comparator = xstrdup("i;ascii-casemap");
    }
    if (h->comptag == -1) { h->comptag = IS; }
    return h;
}

static void free_htags(struct htags *h)
{
    free(h->comparator);
    free(h);
}

static struct btags *new_btags(void)
{
    struct btags *r = (struct btags *) xmalloc(sizeof(struct btags));

    r->transform = r->offset = r->comptag = r->relation = -1;
    r->content_types = NULL;
    r->comparator = NULL;

    return r;
}

static struct btags *canon_btags(struct btags *b)
{
    if (b->transform == -1) { b->transform = TEXT; }
    if (b->content_types == NULL) {
	if (b->transform == RAW) {
	    b->content_types = new_sl(xstrdup(""), NULL);
	} else {
	    b->content_types = new_sl(xstrdup("text"), NULL);
	}
    }
    if (b->offset == -1) { b->offset = 0; }
    if (b->comparator == NULL) { b->comparator = xstrdup("i;ascii-casemap"); }
    if (b->comptag == -1) { b->comptag = IS; }
    return b;
}

static void free_btags(struct btags *b)
{
    if (b->content_types) { free_sl(b->content_types); }
    free(b->comparator);
    free(b);
}

static struct vtags *new_vtags(void)
{
    struct vtags *r = (struct vtags *) xmalloc(sizeof(struct vtags));

    r->days = -1;
    r->addresses = NULL;
    r->subject = NULL;
    r->from = NULL;
    r->handle = NULL;
    r->mime = -1;

    return r;
}

static struct vtags *canon_vtags(struct vtags *v)
{
    assert(parse_script->interp.vacation != NULL);

    if (v->days == -1) { v->days = 7; }
    if (v->days < parse_script->interp.vacation->min_response) 
       { v->days = parse_script->interp.vacation->min_response; }
    if (v->days > parse_script->interp.vacation->max_response)
       { v->days = parse_script->interp.vacation->max_response; }
    if (v->mime == -1) { v->mime = 0; }

    return v;
}

static void free_vtags(struct vtags *v)
{
    if (v->addresses) { free_sl(v->addresses); }
    if (v->subject) { free(v->subject); }
    if (v->from) { free(v->from); }
    if (v->handle) { free(v->handle); }
    free(v);
}

static struct ntags *new_ntags(void)
{
    struct ntags *r = (struct ntags *) xmalloc(sizeof(struct ntags));

    r->method = NULL;
    r->id = NULL;
    r->options = NULL;
    r->priority = -1;
    r->message = NULL;

    return r;
}

static struct ntags *canon_ntags(struct ntags *n)
{
    if (n->priority == -1) { n->priority = NORMAL; }
    if (n->message == NULL) { n->message = xstrdup("$from$: $subject$"); }
    if (n->method == NULL) { n->method = xstrdup("default"); }
    return n;
}
static struct dtags *canon_dtags(struct dtags *d)
{
    if (d->priority == -1) { d->priority = ANY; }
    if (d->comptag == -1) { d->comptag = ANY; }
       return d;
}

static void free_ntags(struct ntags *n)
{
    if (n->method) { free(n->method); }
    if (n->id) { free(n->id); }
    if (n->options) { free_sl(n->options); }
    if (n->message) { free(n->message); }
    free(n);
}

static struct dtags *new_dtags(void)
{
    struct dtags *r = (struct dtags *) xmalloc(sizeof(struct dtags));

    r->comptag = r->priority= r->relation = -1;
    r->pattern  = NULL;

    return r;
}

static void free_dtags(struct dtags *d)
{
    if (d->pattern) free(d->pattern);
    free(d);
}

static int verify_stringlist(stringlist_t *sl, int (*verify)(char *))
{
    for (; sl != NULL && verify(sl->s); sl = sl->next) ;
    return (sl == NULL);
}

char *addrptr;		/* pointer to address string for address lexer */
char addrerr[500];	/* buffer for address parser error messages */

static int verify_address(char *s)
{
    char errbuf[500];

    addrptr = s;
    addrerr[0] = '\0';	/* paranoia */
    if (addrparse()) {
	snprintf(errbuf, sizeof(errbuf), "address '%s': %s", s, addrerr);
	yyerror(errbuf);
	return 0;
    }
    return 1;
}

static int verify_mailbox(char *s)
{
    if (!verify_utf8(s)) return 0;

    /* xxx if not a mailbox, call yyerror */
    return 1;
}

static int verify_header(char *hdr)
{
    char *h = hdr;
    char errbuf[100];

    while (*h) {
	/* field-name      =       1*ftext
	   ftext           =       %d33-57 / %d59-126         
	   ; Any character except
	   ;  controls, SP, and
	   ;  ":". */
	if (!((*h >= 33 && *h <= 57) || (*h >= 59 && *h <= 126))) {
	    snprintf(errbuf, sizeof(errbuf),
		     "header '%s': not a valid header", hdr);
	    yyerror(errbuf);
	    return 0;
	}
	h++;
    }
    return 1;
}
 
static int verify_addrheader(char *hdr)
{
    const char **h, *hdrs[] = {
	"from", "sender", "reply-to",	/* RFC2822 originator fields */
	"to", "cc", "bcc",		/* RFC2822 destination fields */
	"resent-from", "resent-sender",	/* RFC2822 resent fields */
	"resent-to", "resent-cc", "resent-bcc",
	"return-path",			/* RFC2822 trace fields */
	"disposition-notification-to",	/* RFC2298 MDN request fields */
	"delivered-to",			/* non-standard (loop detection) */
	"approved",			/* RFC1036 moderator/control fields */
	NULL
    };
    char errbuf[100];

    if (!config_getswitch(IMAPOPT_RFC3028_STRICT))
	return verify_header(hdr);

    for (lcase(hdr), h = hdrs; *h; h++) {
	if (!strcmp(*h, hdr)) return 1;
    }

    snprintf(errbuf, sizeof(errbuf),
	     "header '%s': not a valid header for an address test", hdr);
    yyerror(errbuf);
    return 0;
}
 
static int verify_envelope(char *env)
{
    char errbuf[100];

    lcase(env);
    if (!config_getswitch(IMAPOPT_RFC3028_STRICT) ||
	!strcmp(env, "from") || !strcmp(env, "to") || !strcmp(env, "auth")) {
	return 1;
    }

    snprintf(errbuf, sizeof(errbuf),
	     "env-part '%s': not a valid part for an envelope test", env);
    yyerror(errbuf);
    return 0;
}
 
static int verify_relat(char *r)
{/* this really should have been a token to begin with.*/
    char errbuf[100];
	lcase(r);
	if (!strcmp(r, "gt")) {return GT;}
	else if (!strcmp(r, "ge")) {return GE;}
	else if (!strcmp(r, "lt")) {return LT;}
	else if (!strcmp(r, "le")) {return LE;}
	else if (!strcmp(r, "ne")) {return NE;}
	else if (!strcmp(r, "eq")) {return EQ;}
	else{
	  sprintf(errbuf, "flag '%s': not a valid relational operation", r);
	  yyerror(errbuf);
	  return -1;
	}
	
}




static int verify_flag(char *f)
{
    char errbuf[100];
 
    if (f[0] == '\\') {
	lcase(f);
	if (strcmp(f, "\\seen") && strcmp(f, "\\answered") &&
	    strcmp(f, "\\flagged") && strcmp(f, "\\draft") &&
	    strcmp(f, "\\deleted")) {
	    snprintf(errbuf, sizeof(errbuf),
		     "flag '%s': not a system flag", f);
	    yyerror(errbuf);
	    return 0;
	}
	return 1;
    }
    if (!imparse_isatom(f)) {
	snprintf(errbuf, sizeof(errbuf), "flag '%s': not a valid keyword", f);
	yyerror(errbuf);
	return 0;
    }
    return 1;
}
 
#ifdef ENABLE_REGEX
static int verify_regex(char *s, int cflags)
{
    int ret;
    char errbuf[100];
    regex_t *reg = (regex_t *) xmalloc(sizeof(regex_t));

     if ((ret = regcomp(reg, s, cflags)) != 0) {
	(void) regerror(ret, reg, errbuf, sizeof(errbuf));
	yyerror(errbuf);
	free(reg);
	return 0;
	}
    free(reg);
    return 1;
}

static int verify_regexs(stringlist_t *sl, char *comp)
{
    stringlist_t *sl2;
    int cflags = REG_EXTENDED | REG_NOSUB;
 

    if (!strcmp(comp, "i;ascii-casemap")) {
	cflags |= REG_ICASE;
    }

    for (sl2 = sl; sl2 != NULL; sl2 = sl2->next) {
	if ((verify_regex(sl2->s, cflags)) == 0) {
	    break;
	}
    }
    if (sl2 == NULL) {
	return 1;
    }
    return 0;
}
#endif

/*
 * Valid UTF-8 check (from RFC 2640 Annex B.1)
 *
 * The following routine checks if a byte sequence is valid UTF-8. This
 * is done by checking for the proper tagging of the first and following
 * bytes to make sure they conform to the UTF-8 format. It then checks
 * to assure that the data part of the UTF-8 sequence conforms to the
 * proper range allowed by the encoding. Note: This routine will not
 * detect characters that have not been assigned and therefore do not
 * exist.
 */
static int verify_utf8(char *s)
{
    const char *buf = s;
    const char *endbuf = s + strlen(s);
    unsigned char byte2mask = 0x00, c;
    int trailing = 0;  /* trailing (continuation) bytes to follow */

    while (buf != endbuf) {
	c = *buf++;
	if (trailing) {
	    if ((c & 0xC0) == 0x80) {		/* Does trailing byte
						   follow UTF-8 format? */
		if (byte2mask) {		/* Need to check 2nd byte
						   for proper range? */
		    if (c & byte2mask)		/* Are appropriate bits set? */
			byte2mask = 0x00;
		    else
			break;
		}
		trailing--;
	    }
	    else
		break;
	}
	else {
	    if ((c & 0x80) == 0x00)		/* valid 1 byte UTF-8 */
		continue;
	    else if ((c & 0xE0) == 0xC0)	/* valid 2 byte UTF-8 */
		if (c & 0x1E) {			/* Is UTF-8 byte
						   in proper range? */
		    trailing = 1;
		}
		else
		    break;
	    else if ((c & 0xF0) == 0xE0) {	/* valid 3 byte UTF-8 */
		if (!(c & 0x0F)) {		/* Is UTF-8 byte
						   in proper range? */
		    byte2mask = 0x20;		/* If not, set mask
						   to check next byte */
		}
		trailing = 2;
	    }
	    else if ((c & 0xF8) == 0xF0) {	/* valid 4 byte UTF-8 */
		if (!(c & 0x07)) {		/* Is UTF-8 byte
						   in proper range? */
		    byte2mask = 0x30;		/* If not, set mask
						   to check next byte */
		}
		trailing = 3;
	    }
	    else if ((c & 0xFC) == 0xF8) {	/* valid 5 byte UTF-8 */
		if (!(c & 0x03)) {		/* Is UTF-8 byte
						   in proper range? */
		    byte2mask = 0x38;		/* If not, set mask
						   to check next byte */
		}
		trailing = 4;
	    }
	    else if ((c & 0xFE) == 0xFC) {	/* valid 6 byte UTF-8 */
		if (!(c & 0x01)) {		/* Is UTF-8 byte
						   in proper range? */
		    byte2mask = 0x3C;		/* If not, set mask
						   to check next byte */
		}
		trailing = 5;
	    }
	    else
		break;
	}
    }

    if ((buf != endbuf) || trailing) {
	char errbuf[100];

	snprintf(errbuf, sizeof(errbuf),
		 "string '%s': not valid utf8", s);
	yyerror(errbuf);
	return 0;
    }

    return 1;
}

