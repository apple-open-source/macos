/* A Bison parser, made by GNU Bison 1.875d.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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
     kCLASS = 258,
     kMODULE = 259,
     kDEF = 260,
     kUNDEF = 261,
     kBEGIN = 262,
     kRESCUE = 263,
     kENSURE = 264,
     kEND = 265,
     kIF = 266,
     kUNLESS = 267,
     kTHEN = 268,
     kELSIF = 269,
     kELSE = 270,
     kCASE = 271,
     kWHEN = 272,
     kWHILE = 273,
     kUNTIL = 274,
     kFOR = 275,
     kBREAK = 276,
     kNEXT = 277,
     kREDO = 278,
     kRETRY = 279,
     kIN = 280,
     kDO = 281,
     kDO_COND = 282,
     kDO_BLOCK = 283,
     kRETURN = 284,
     kYIELD = 285,
     kSUPER = 286,
     kSELF = 287,
     kNIL = 288,
     kTRUE = 289,
     kFALSE = 290,
     kAND = 291,
     kOR = 292,
     kNOT = 293,
     kIF_MOD = 294,
     kUNLESS_MOD = 295,
     kWHILE_MOD = 296,
     kUNTIL_MOD = 297,
     kRESCUE_MOD = 298,
     kALIAS = 299,
     kDEFINED = 300,
     klBEGIN = 301,
     klEND = 302,
     k__LINE__ = 303,
     k__FILE__ = 304,
     tIDENTIFIER = 305,
     tFID = 306,
     tGVAR = 307,
     tIVAR = 308,
     tCONSTANT = 309,
     tCVAR = 310,
     tINTEGER = 311,
     tFLOAT = 312,
     tSTRING_CONTENT = 313,
     tNTH_REF = 314,
     tBACK_REF = 315,
     tREGEXP_END = 316,
     tUPLUS = 317,
     tUMINUS = 318,
     tPOW = 319,
     tCMP = 320,
     tEQ = 321,
     tEQQ = 322,
     tNEQ = 323,
     tGEQ = 324,
     tLEQ = 325,
     tANDOP = 326,
     tOROP = 327,
     tMATCH = 328,
     tNMATCH = 329,
     tDOT2 = 330,
     tDOT3 = 331,
     tAREF = 332,
     tASET = 333,
     tLSHFT = 334,
     tRSHFT = 335,
     tCOLON2 = 336,
     tCOLON3 = 337,
     tOP_ASGN = 338,
     tASSOC = 339,
     tLPAREN = 340,
     tLPAREN_ARG = 341,
     tRPAREN = 342,
     tLBRACK = 343,
     tLBRACE = 344,
     tLBRACE_ARG = 345,
     tSTAR = 346,
     tAMPER = 347,
     tSYMBEG = 348,
     tSTRING_BEG = 349,
     tXSTRING_BEG = 350,
     tREGEXP_BEG = 351,
     tWORDS_BEG = 352,
     tQWORDS_BEG = 353,
     tSTRING_DBEG = 354,
     tSTRING_DVAR = 355,
     tSTRING_END = 356,
     tLOWEST = 357,
     tUMINUS_NUM = 358,
     tLAST_TOKEN = 359
   };
#endif
#define kCLASS 258
#define kMODULE 259
#define kDEF 260
#define kUNDEF 261
#define kBEGIN 262
#define kRESCUE 263
#define kENSURE 264
#define kEND 265
#define kIF 266
#define kUNLESS 267
#define kTHEN 268
#define kELSIF 269
#define kELSE 270
#define kCASE 271
#define kWHEN 272
#define kWHILE 273
#define kUNTIL 274
#define kFOR 275
#define kBREAK 276
#define kNEXT 277
#define kREDO 278
#define kRETRY 279
#define kIN 280
#define kDO 281
#define kDO_COND 282
#define kDO_BLOCK 283
#define kRETURN 284
#define kYIELD 285
#define kSUPER 286
#define kSELF 287
#define kNIL 288
#define kTRUE 289
#define kFALSE 290
#define kAND 291
#define kOR 292
#define kNOT 293
#define kIF_MOD 294
#define kUNLESS_MOD 295
#define kWHILE_MOD 296
#define kUNTIL_MOD 297
#define kRESCUE_MOD 298
#define kALIAS 299
#define kDEFINED 300
#define klBEGIN 301
#define klEND 302
#define k__LINE__ 303
#define k__FILE__ 304
#define tIDENTIFIER 305
#define tFID 306
#define tGVAR 307
#define tIVAR 308
#define tCONSTANT 309
#define tCVAR 310
#define tINTEGER 311
#define tFLOAT 312
#define tSTRING_CONTENT 313
#define tNTH_REF 314
#define tBACK_REF 315
#define tREGEXP_END 316
#define tUPLUS 317
#define tUMINUS 318
#define tPOW 319
#define tCMP 320
#define tEQ 321
#define tEQQ 322
#define tNEQ 323
#define tGEQ 324
#define tLEQ 325
#define tANDOP 326
#define tOROP 327
#define tMATCH 328
#define tNMATCH 329
#define tDOT2 330
#define tDOT3 331
#define tAREF 332
#define tASET 333
#define tLSHFT 334
#define tRSHFT 335
#define tCOLON2 336
#define tCOLON3 337
#define tOP_ASGN 338
#define tASSOC 339
#define tLPAREN 340
#define tLPAREN_ARG 341
#define tRPAREN 342
#define tLBRACK 343
#define tLBRACE 344
#define tLBRACE_ARG 345
#define tSTAR 346
#define tAMPER 347
#define tSYMBEG 348
#define tSTRING_BEG 349
#define tXSTRING_BEG 350
#define tREGEXP_BEG 351
#define tWORDS_BEG 352
#define tQWORDS_BEG 353
#define tSTRING_DBEG 354
#define tSTRING_DVAR 355
#define tSTRING_END 356
#define tLOWEST 357
#define tUMINUS_NUM 358
#define tLAST_TOKEN 359




/* Copy the first part of user declarations.  */
#line 13 "parse.y"


#define YYDEBUG 1

#include "ruby.h"
#include "env.h"
#include "intern.h"
#include "node.h"
#include "st.h"
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#define yyparse ruby_yyparse
#define yylex ruby_yylex
#define yyerror ruby_yyerror
#define yylval ruby_yylval
#define yychar ruby_yychar
#define yydebug ruby_yydebug

#define ID_SCOPE_SHIFT 3
#define ID_SCOPE_MASK 0x07
#define ID_LOCAL    0x01
#define ID_INSTANCE 0x02
#define ID_GLOBAL   0x03
#define ID_ATTRSET  0x04
#define ID_CONST    0x05
#define ID_CLASS    0x06
#define ID_JUNK     0x07
#define ID_INTERNAL ID_JUNK

#define is_notop_id(id) ((id)>tLAST_TOKEN)
#define is_local_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_LOCAL)
#define is_global_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_GLOBAL)
#define is_instance_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_INSTANCE)
#define is_attrset_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_ATTRSET)
#define is_const_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_CONST)
#define is_class_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_CLASS)
#define is_junk_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_JUNK)

#define is_asgn_or_id(id) ((is_notop_id(id)) && \
	(((id)&ID_SCOPE_MASK) == ID_GLOBAL || \
	 ((id)&ID_SCOPE_MASK) == ID_INSTANCE || \
	 ((id)&ID_SCOPE_MASK) == ID_CLASS))

NODE *ruby_eval_tree_begin = 0;
NODE *ruby_eval_tree = 0;

char *ruby_sourcefile;		/* current source file */
int   ruby_sourceline;		/* current line no. */

static int yylex();
static int yyerror();

static enum lex_state {
    EXPR_BEG,			/* ignore newline, +/- is a sign. */
    EXPR_END,			/* newline significant, +/- is a operator. */
    EXPR_ARG,			/* newline significant, +/- is a operator. */
    EXPR_CMDARG,		/* newline significant, +/- is a operator. */
    EXPR_ENDARG,		/* newline significant, +/- is a operator. */
    EXPR_MID,			/* newline significant, +/- is a operator. */
    EXPR_FNAME,			/* ignore newline, no reserved words. */
    EXPR_DOT,			/* right after `.' or `::', no reserved words. */
    EXPR_CLASS,			/* immediate after `class', no here document. */
} lex_state;
static NODE *lex_strterm;

#ifdef HAVE_LONG_LONG
typedef unsigned LONG_LONG stack_type;
#else
typedef unsigned long stack_type;
#endif

#define BITSTACK_PUSH(stack, n)	(stack = (stack<<1)|((n)&1))
#define BITSTACK_POP(stack)	(stack >>= 1)
#define BITSTACK_LEXPOP(stack)	(stack = (stack >> 1) | (stack & 1))
#define BITSTACK_SET_P(stack)	(stack&1)

static stack_type cond_stack = 0;
#define COND_PUSH(n)	BITSTACK_PUSH(cond_stack, n)
#define COND_POP()	BITSTACK_POP(cond_stack)
#define COND_LEXPOP()	BITSTACK_LEXPOP(cond_stack)
#define COND_P()	BITSTACK_SET_P(cond_stack)

static stack_type cmdarg_stack = 0;
#define CMDARG_PUSH(n)	BITSTACK_PUSH(cmdarg_stack, n)
#define CMDARG_POP()	BITSTACK_POP(cmdarg_stack)
#define CMDARG_LEXPOP()	BITSTACK_LEXPOP(cmdarg_stack)
#define CMDARG_P()	BITSTACK_SET_P(cmdarg_stack)

static int class_nest = 0;
static int in_single = 0;
static int in_def = 0;
static int compile_for_eval = 0;
static ID cur_mid = 0;

static NODE *cond();
static NODE *logop();
static int cond_negative();

static NODE *newline_node();
static void fixpos();

static int value_expr0();
static void void_expr0();
static void void_stmts();
static NODE *remove_begin();
#define value_expr(node) value_expr0((node) = remove_begin(node))
#define void_expr(node) void_expr0((node) = remove_begin(node))

static NODE *block_append();
static NODE *list_append();
static NODE *list_concat();
static NODE *arg_concat();
static NODE *arg_prepend();
static NODE *literal_concat();
static NODE *new_evstr();
static NODE *evstr2dstr();
static NODE *call_op();
static int in_defined = 0;

static NODE *negate_lit();
static NODE *ret_args();
static NODE *arg_blk_pass();
static NODE *new_call();
static NODE *new_fcall();
static NODE *new_super();
static NODE *new_yield();

static NODE *gettable();
static NODE *assignable();
static NODE *aryset();
static NODE *attrset();
static void rb_backref_error();
static NODE *node_assign();

static NODE *match_gen();
static void local_push();
static void local_pop();
static int  local_append();
static int  local_cnt();
static int  local_id();
static ID  *local_tbl();
static ID   internal_id();

static struct RVarmap *dyna_push();
static void dyna_pop();
static int dyna_in_block();
static NODE *dyna_init();

static void top_local_init();
static void top_local_setup();

#define RE_OPTION_ONCE 0x80

#define NODE_STRTERM NODE_ZARRAY	/* nothing to gc */
#define NODE_HEREDOC NODE_ARRAY 	/* 1, 3 to gc */
#define SIGN_EXTEND(x,n) (((1<<(n)-1)^((x)&~(~0<<(n))))-(1<<(n)-1))
#define nd_func u1.id
#if SIZEOF_SHORT == 2
#define nd_term(node) ((signed short)(node)->u2.id)
#else
#define nd_term(node) SIGN_EXTEND((node)->u2.id, CHAR_BIT*2)
#endif
#define nd_paren(node) (char)((node)->u2.id >> CHAR_BIT*2)
#define nd_nest u3.id

/* Older versions of Yacc set YYMAXDEPTH to a very low value by default (150,
   for instance).  This is too low for Ruby to parse some files, such as
   date/format.rb, therefore bump the value up to at least Bison's default. */
#ifdef OLD_YACC
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif
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

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 191 "parse.y"
typedef union YYSTYPE {
    NODE *node;
    ID id;
    int num;
    struct RVarmap *vars;
} YYSTYPE;
/* Line 191 of yacc.c.  */
#line 469 "parse.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 214 of yacc.c.  */
#line 481 "parse.c"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

# ifndef YYFREE
#  define YYFREE free
# endif
# ifndef YYMALLOC
#  define YYMALLOC malloc
# endif

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   define YYSTACK_ALLOC alloca
#  endif
# else
#  if defined (alloca) || defined (_ALLOCA_H)
#   define YYSTACK_ALLOC alloca
#  else
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
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
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
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
   typedef short int yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   9503

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  132
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  143
/* YYNRULES -- Number of rules. */
#define YYNRULES  500
/* YYNRULES -- Number of states. */
#define YYNSTATES  895

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   359

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     130,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,   129,   117,     2,     2,     2,   115,   110,     2,
     128,   125,   113,   111,   126,   112,   124,   114,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   105,   131,
     107,   103,   106,   104,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   122,     2,   123,   109,     2,   127,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   120,   108,   121,   118,     2,     2,     2,
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
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   116,   119
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short int yyprhs[] =
{
       0,     0,     3,     4,     7,    12,    15,    17,    19,    23,
      26,    27,    32,    36,    40,    44,    47,    51,    55,    59,
      63,    67,    68,    74,    79,    83,    87,    91,    98,   104,
     110,   116,   120,   124,   128,   132,   134,   136,   140,   144,
     147,   150,   152,   154,   156,   158,   161,   164,   167,   169,
     174,   179,   180,   181,   188,   191,   195,   200,   206,   211,
     217,   220,   223,   225,   229,   231,   235,   237,   240,   244,
     247,   250,   252,   254,   258,   261,   265,   267,   272,   276,
     280,   284,   288,   291,   293,   295,   300,   304,   308,   312,
     316,   319,   321,   323,   325,   328,   330,   334,   336,   338,
     340,   342,   344,   346,   348,   350,   351,   356,   358,   360,
     362,   364,   366,   368,   370,   372,   374,   376,   378,   380,
     382,   384,   386,   388,   390,   392,   394,   396,   398,   400,
     402,   404,   406,   408,   410,   412,   414,   416,   418,   420,
     422,   424,   426,   428,   430,   432,   434,   436,   438,   440,
     442,   444,   446,   448,   450,   452,   454,   456,   458,   460,
     462,   464,   466,   468,   470,   472,   474,   476,   478,   480,
     482,   484,   486,   488,   490,   494,   500,   504,   511,   517,
     523,   529,   535,   540,   544,   548,   552,   556,   560,   564,
     568,   572,   576,   581,   586,   589,   592,   596,   600,   604,
     608,   612,   616,   620,   624,   628,   632,   636,   640,   644,
     647,   650,   654,   658,   662,   666,   667,   672,   678,   680,
     682,   684,   687,   690,   696,   699,   703,   707,   712,   717,
     724,   726,   728,   730,   733,   739,   742,   748,   753,   761,
     765,   767,   772,   776,   782,   790,   793,   799,   804,   811,
     819,   829,   833,   835,   836,   839,   841,   842,   846,   847,
     852,   855,   858,   860,   862,   866,   870,   875,   878,   880,
     882,   884,   886,   888,   890,   892,   894,   896,   897,   902,
     903,   909,   913,   917,   920,   925,   929,   933,   935,   940,
     944,   946,   947,   954,   957,   959,   962,   969,   976,   977,
     978,   986,   987,   988,   996,  1002,  1007,  1013,  1014,  1015,
    1025,  1026,  1033,  1034,  1035,  1044,  1045,  1051,  1052,  1059,
    1060,  1061,  1071,  1073,  1075,  1077,  1079,  1081,  1083,  1085,
    1087,  1090,  1092,  1094,  1096,  1098,  1104,  1106,  1109,  1111,
    1113,  1115,  1118,  1120,  1124,  1125,  1126,  1133,  1136,  1141,
    1146,  1149,  1154,  1159,  1163,  1166,  1168,  1169,  1170,  1177,
    1178,  1179,  1186,  1192,  1194,  1199,  1202,  1204,  1206,  1213,
    1215,  1217,  1219,  1221,  1224,  1226,  1229,  1231,  1233,  1235,
    1237,  1239,  1241,  1244,  1248,  1252,  1256,  1260,  1264,  1265,
    1269,  1271,  1274,  1278,  1282,  1283,  1287,  1288,  1291,  1292,
    1295,  1297,  1298,  1302,  1303,  1308,  1310,  1312,  1314,  1316,
    1319,  1321,  1323,  1325,  1327,  1331,  1333,  1335,  1338,  1341,
    1343,  1345,  1347,  1349,  1351,  1353,  1355,  1357,  1359,  1361,
    1363,  1365,  1367,  1369,  1371,  1373,  1374,  1379,  1382,  1387,
    1390,  1397,  1402,  1407,  1410,  1415,  1418,  1421,  1423,  1424,
    1426,  1428,  1430,  1432,  1434,  1436,  1440,  1444,  1446,  1450,
    1452,  1454,  1457,  1459,  1461,  1463,  1466,  1469,  1471,  1473,
    1474,  1480,  1482,  1485,  1488,  1490,  1494,  1498,  1500,  1502,
    1504,  1506,  1508,  1510,  1512,  1514,  1516,  1518,  1520,  1522,
    1523,  1525,  1526,  1528,  1529,  1531,  1533,  1535,  1537,  1539,
    1542
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short int yyrhs[] =
{
     133,     0,    -1,    -1,   134,   136,    -1,   136,   218,   202,
     221,    -1,   137,   269,    -1,   274,    -1,   138,    -1,   137,
     273,   138,    -1,     1,   138,    -1,    -1,    44,   159,   139,
     159,    -1,    44,    52,    52,    -1,    44,    52,    60,    -1,
      44,    52,    59,    -1,     6,   160,    -1,   138,    39,   142,
      -1,   138,    40,   142,    -1,   138,    41,   142,    -1,   138,
      42,   142,    -1,   138,    43,   138,    -1,    -1,    46,   140,
     120,   136,   121,    -1,    47,   120,   136,   121,    -1,   155,
     103,   143,    -1,   149,   103,   143,    -1,   245,    83,   143,
      -1,   198,   122,   167,   123,    83,   143,    -1,   198,   124,
      50,    83,   143,    -1,   198,   124,    54,    83,   143,    -1,
     198,    81,    50,    83,   143,    -1,   246,    83,   143,    -1,
     155,   103,   180,    -1,   149,   103,   166,    -1,   149,   103,
     180,    -1,   141,    -1,   143,    -1,   141,    36,   141,    -1,
     141,    37,   141,    -1,    38,   141,    -1,   117,   143,    -1,
     164,    -1,   141,    -1,   148,    -1,   144,    -1,    29,   170,
      -1,    21,   170,    -1,    22,   170,    -1,   208,    -1,   208,
     124,   266,   172,    -1,   208,    81,   266,   172,    -1,    -1,
      -1,    90,   146,   204,   147,   136,   121,    -1,   265,   172,
      -1,   265,   172,   145,    -1,   198,   124,   266,   172,    -1,
     198,   124,   266,   172,   145,    -1,   198,    81,   266,   172,
      -1,   198,    81,   266,   172,   145,    -1,    31,   172,    -1,
      30,   172,    -1,   151,    -1,    85,   150,   125,    -1,   151,
      -1,    85,   150,   125,    -1,   153,    -1,   153,   152,    -1,
     153,    91,   154,    -1,   153,    91,    -1,    91,   154,    -1,
      91,    -1,   154,    -1,    85,   150,   125,    -1,   152,   126,
      -1,   153,   152,   126,    -1,   243,    -1,   198,   122,   167,
     123,    -1,   198,   124,    50,    -1,   198,    81,    50,    -1,
     198,   124,    54,    -1,   198,    81,    54,    -1,    82,    54,
      -1,   246,    -1,   243,    -1,   198,   122,   167,   123,    -1,
     198,   124,    50,    -1,   198,    81,    50,    -1,   198,   124,
      54,    -1,   198,    81,    54,    -1,    82,    54,    -1,   246,
      -1,    50,    -1,    54,    -1,    82,   156,    -1,   156,    -1,
     198,    81,   156,    -1,    50,    -1,    54,    -1,    51,    -1,
     162,    -1,   163,    -1,   158,    -1,   239,    -1,   159,    -1,
      -1,   160,   126,   161,   159,    -1,   108,    -1,   109,    -1,
     110,    -1,    65,    -1,    66,    -1,    67,    -1,    73,    -1,
     106,    -1,    69,    -1,   107,    -1,    70,    -1,    79,    -1,
      80,    -1,   111,    -1,   112,    -1,   113,    -1,    91,    -1,
     114,    -1,   115,    -1,    64,    -1,   118,    -1,    62,    -1,
      63,    -1,    77,    -1,    78,    -1,   127,    -1,    48,    -1,
      49,    -1,    46,    -1,    47,    -1,    44,    -1,    36,    -1,
       7,    -1,    21,    -1,    16,    -1,     3,    -1,     5,    -1,
      45,    -1,    26,    -1,    15,    -1,    14,    -1,    10,    -1,
       9,    -1,    35,    -1,    20,    -1,    25,    -1,     4,    -1,
      22,    -1,    33,    -1,    38,    -1,    37,    -1,    23,    -1,
       8,    -1,    24,    -1,    29,    -1,    32,    -1,    31,    -1,
      13,    -1,    34,    -1,     6,    -1,    17,    -1,    30,    -1,
      39,    -1,    40,    -1,    41,    -1,    42,    -1,    43,    -1,
     155,   103,   164,    -1,   155,   103,   164,    43,   164,    -1,
     245,    83,   164,    -1,   198,   122,   167,   123,    83,   164,
      -1,   198,   124,    50,    83,   164,    -1,   198,   124,    54,
      83,   164,    -1,   198,    81,    50,    83,   164,    -1,   198,
      81,    54,    83,   164,    -1,    82,    54,    83,   164,    -1,
     246,    83,   164,    -1,   164,    75,   164,    -1,   164,    76,
     164,    -1,   164,   111,   164,    -1,   164,   112,   164,    -1,
     164,   113,   164,    -1,   164,   114,   164,    -1,   164,   115,
     164,    -1,   164,    64,   164,    -1,   116,    56,    64,   164,
      -1,   116,    57,    64,   164,    -1,    62,   164,    -1,    63,
     164,    -1,   164,   108,   164,    -1,   164,   109,   164,    -1,
     164,   110,   164,    -1,   164,    65,   164,    -1,   164,   106,
     164,    -1,   164,    69,   164,    -1,   164,   107,   164,    -1,
     164,    70,   164,    -1,   164,    66,   164,    -1,   164,    67,
     164,    -1,   164,    68,   164,    -1,   164,    73,   164,    -1,
     164,    74,   164,    -1,   117,   164,    -1,   118,   164,    -1,
     164,    79,   164,    -1,   164,    80,   164,    -1,   164,    71,
     164,    -1,   164,    72,   164,    -1,    -1,    45,   270,   165,
     164,    -1,   164,   104,   164,   105,   164,    -1,   181,    -1,
     164,    -1,   274,    -1,   148,   270,    -1,   179,   271,    -1,
     179,   126,    91,   164,   270,    -1,   263,   271,    -1,    91,
     164,   270,    -1,   128,   274,   125,    -1,   128,   170,   270,
     125,    -1,   128,   208,   270,   125,    -1,   128,   179,   126,
     208,   270,   125,    -1,   274,    -1,   168,    -1,   148,    -1,
     179,   178,    -1,   179,   126,    91,   166,   178,    -1,   263,
     178,    -1,   263,   126,    91,   166,   178,    -1,   179,   126,
     263,   178,    -1,   179,   126,   263,   126,    91,   164,   178,
      -1,    91,   166,   178,    -1,   177,    -1,   166,   126,   179,
     178,    -1,   166,   126,   177,    -1,   166,   126,    91,   166,
     178,    -1,   166,   126,   179,   126,    91,   166,   178,    -1,
     263,   178,    -1,   263,   126,    91,   166,   178,    -1,   166,
     126,   263,   178,    -1,   166,   126,   179,   126,   263,   178,
      -1,   166,   126,   263,   126,    91,   166,   178,    -1,   166,
     126,   179,   126,   263,   126,    91,   166,   178,    -1,    91,
     166,   178,    -1,   177,    -1,    -1,   173,   174,    -1,   170,
      -1,    -1,    86,   175,   125,    -1,    -1,    86,   171,   176,
     125,    -1,    92,   166,    -1,   126,   177,    -1,   274,    -1,
     166,    -1,   179,   126,   166,    -1,   179,   126,   166,    -1,
     179,   126,    91,   166,    -1,    91,   166,    -1,   222,    -1,
     223,    -1,   226,    -1,   227,    -1,   228,    -1,   231,    -1,
     244,    -1,   246,    -1,    51,    -1,    -1,     7,   182,   135,
      10,    -1,    -1,    86,   141,   183,   270,   125,    -1,    85,
     136,   125,    -1,   198,    81,    54,    -1,    82,    54,    -1,
     198,   122,   167,   123,    -1,    88,   167,   123,    -1,    89,
     262,   121,    -1,    29,    -1,    30,   128,   170,   125,    -1,
      30,   128,   125,    -1,    30,    -1,    -1,    45,   270,   128,
     184,   141,   125,    -1,   265,   210,    -1,   209,    -1,   209,
     210,    -1,    11,   142,   199,   136,   201,    10,    -1,    12,
     142,   199,   136,   202,    10,    -1,    -1,    -1,    18,   185,
     142,   200,   186,   136,    10,    -1,    -1,    -1,    19,   187,
     142,   200,   188,   136,    10,    -1,    16,   142,   269,   215,
      10,    -1,    16,   269,   215,    10,    -1,    16,   269,    15,
     136,    10,    -1,    -1,    -1,    20,   203,    25,   189,   142,
     200,   190,   136,    10,    -1,    -1,     3,   157,   247,   191,
     135,    10,    -1,    -1,    -1,     3,    79,   141,   192,   272,
     193,   135,    10,    -1,    -1,     4,   157,   194,   135,    10,
      -1,    -1,     5,   158,   195,   249,   135,    10,    -1,    -1,
      -1,     5,   260,   268,   196,   158,   197,   249,   135,    10,
      -1,    21,    -1,    22,    -1,    23,    -1,    24,    -1,   181,
      -1,   272,    -1,   105,    -1,    13,    -1,   272,    13,    -1,
     272,    -1,   105,    -1,    27,    -1,   202,    -1,    14,   142,
     199,   136,   201,    -1,   274,    -1,    15,   136,    -1,   155,
      -1,   149,    -1,   274,    -1,   108,   108,    -1,    72,    -1,
     108,   203,   108,    -1,    -1,    -1,    28,   206,   204,   207,
     136,    10,    -1,   148,   205,    -1,   208,   124,   266,   169,
      -1,   208,    81,   266,   169,    -1,   265,   168,    -1,   198,
     124,   266,   169,    -1,   198,    81,   266,   168,    -1,   198,
      81,   267,    -1,    31,   168,    -1,    31,    -1,    -1,    -1,
     120,   211,   204,   212,   136,   121,    -1,    -1,    -1,    26,
     213,   204,   214,   136,    10,    -1,    17,   216,   199,   136,
     217,    -1,   179,    -1,   179,   126,    91,   166,    -1,    91,
     166,    -1,   202,    -1,   215,    -1,     8,   219,   220,   199,
     136,   218,    -1,   274,    -1,   166,    -1,   180,    -1,   274,
      -1,    84,   155,    -1,   274,    -1,     9,   136,    -1,   274,
      -1,   242,    -1,   239,    -1,   241,    -1,   224,    -1,   225,
      -1,   224,   225,    -1,    94,   233,   101,    -1,    95,   234,
     101,    -1,    96,   234,    61,    -1,    97,   129,   101,    -1,
      97,   229,   101,    -1,    -1,   229,   230,   129,    -1,   235,
      -1,   230,   235,    -1,    98,   129,   101,    -1,    98,   232,
     101,    -1,    -1,   232,    58,   129,    -1,    -1,   233,   235,
      -1,    -1,   234,   235,    -1,    58,    -1,    -1,   100,   236,
     238,    -1,    -1,    99,   237,   136,   121,    -1,    52,    -1,
      53,    -1,    55,    -1,   246,    -1,    93,   240,    -1,   158,
      -1,    53,    -1,    52,    -1,    55,    -1,    93,   234,   101,
      -1,    56,    -1,    57,    -1,   116,    56,    -1,   116,    57,
      -1,    50,    -1,    53,    -1,    52,    -1,    54,    -1,    55,
      -1,    33,    -1,    32,    -1,    34,    -1,    35,    -1,    49,
      -1,    48,    -1,   243,    -1,   243,    -1,    59,    -1,    60,
      -1,   272,    -1,    -1,   107,   248,   142,   272,    -1,     1,
     272,    -1,   128,   250,   270,   125,    -1,   250,   272,    -1,
     252,   126,   254,   126,   256,   259,    -1,   252,   126,   254,
     259,    -1,   252,   126,   256,   259,    -1,   252,   259,    -1,
     254,   126,   256,   259,    -1,   254,   259,    -1,   256,   259,
      -1,   258,    -1,    -1,    54,    -1,    53,    -1,    52,    -1,
      55,    -1,    50,    -1,   251,    -1,   252,   126,   251,    -1,
      50,   103,   166,    -1,   253,    -1,   254,   126,   253,    -1,
     113,    -1,    91,    -1,   255,    50,    -1,   255,    -1,   110,
      -1,    92,    -1,   257,    50,    -1,   126,   258,    -1,   274,
      -1,   244,    -1,    -1,   128,   261,   141,   270,   125,    -1,
     274,    -1,   263,   271,    -1,   179,   271,    -1,   264,    -1,
     263,   126,   264,    -1,   166,    84,   166,    -1,    50,    -1,
      54,    -1,    51,    -1,    50,    -1,    54,    -1,    51,    -1,
     162,    -1,    50,    -1,    51,    -1,   162,    -1,   124,    -1,
      81,    -1,    -1,   273,    -1,    -1,   130,    -1,    -1,   130,
      -1,   126,    -1,   131,    -1,   130,    -1,   272,    -1,   273,
     131,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short int yyrline[] =
{
       0,   328,   328,   328,   353,   373,   380,   381,   385,   389,
     395,   395,   399,   403,   410,   415,   419,   428,   437,   449,
     461,   466,   465,   479,   487,   491,   497,   522,   541,   553,
     565,   577,   582,   586,   591,   596,   599,   600,   604,   608,
     612,   616,   619,   626,   627,   628,   632,   636,   642,   643,
     647,   654,   658,   653,   668,   673,   685,   690,   702,   707,
     719,   724,   731,   732,   738,   739,   745,   749,   753,   757,
     761,   765,   771,   772,   778,   782,   788,   792,   796,   800,
     804,   808,   814,   820,   827,   831,   835,   839,   843,   847,
     853,   859,   866,   870,   873,   877,   881,   887,   888,   889,
     890,   895,   902,   903,   906,   910,   910,   916,   917,   918,
     919,   920,   921,   922,   923,   924,   925,   926,   927,   928,
     929,   930,   931,   932,   933,   934,   935,   936,   937,   938,
     939,   940,   941,   944,   944,   944,   944,   945,   945,   945,
     945,   945,   945,   945,   946,   946,   946,   946,   946,   946,
     946,   947,   947,   947,   947,   947,   947,   948,   948,   948,
     948,   948,   948,   948,   949,   949,   949,   949,   949,   950,
     950,   950,   950,   950,   953,   957,   961,   986,  1005,  1017,
    1029,  1041,  1046,  1051,  1056,  1062,  1068,  1072,  1076,  1080,
    1084,  1088,  1092,  1096,  1100,  1109,  1113,  1117,  1121,  1125,
    1129,  1133,  1137,  1141,  1145,  1149,  1153,  1157,  1161,  1165,
    1169,  1173,  1177,  1181,  1185,  1189,  1189,  1194,  1199,  1205,
    1212,  1213,  1218,  1222,  1227,  1231,  1238,  1242,  1246,  1251,
    1258,  1259,  1262,  1267,  1271,  1276,  1281,  1286,  1291,  1297,
    1301,  1304,  1308,  1312,  1317,  1322,  1327,  1332,  1337,  1342,
    1347,  1352,  1356,  1359,  1359,  1371,  1372,  1372,  1377,  1377,
    1384,  1390,  1394,  1397,  1401,  1407,  1411,  1415,  1421,  1422,
    1423,  1424,  1425,  1426,  1427,  1428,  1429,  1434,  1433,  1446,
    1446,  1451,  1455,  1459,  1463,  1471,  1480,  1484,  1488,  1492,
    1496,  1500,  1500,  1505,  1511,  1512,  1521,  1534,  1547,  1547,
    1547,  1557,  1557,  1557,  1567,  1574,  1578,  1582,  1582,  1582,
    1590,  1589,  1606,  1611,  1605,  1628,  1627,  1644,  1643,  1661,
    1662,  1661,  1676,  1680,  1684,  1688,  1694,  1701,  1702,  1703,
    1704,  1707,  1708,  1709,  1712,  1713,  1722,  1723,  1729,  1730,
    1733,  1734,  1738,  1742,  1749,  1753,  1748,  1763,  1772,  1776,
    1782,  1787,  1792,  1797,  1801,  1805,  1812,  1816,  1811,  1824,
    1828,  1823,  1837,  1844,  1845,  1849,  1855,  1856,  1859,  1870,
    1873,  1877,  1878,  1881,  1885,  1888,  1896,  1899,  1900,  1904,
    1907,  1920,  1921,  1927,  1933,  1956,  1989,  1993,  2000,  2003,
    2009,  2010,  2016,  2020,  2027,  2030,  2037,  2040,  2047,  2050,
    2056,  2058,  2057,  2069,  2068,  2089,  2090,  2091,  2092,  2095,
    2102,  2103,  2104,  2105,  2108,  2134,  2135,  2136,  2140,  2146,
    2147,  2148,  2149,  2150,  2151,  2152,  2153,  2154,  2155,  2156,
    2159,  2165,  2171,  2172,  2175,  2180,  2179,  2187,  2190,  2195,
    2201,  2205,  2209,  2213,  2217,  2221,  2225,  2229,  2234,  2239,
    2243,  2247,  2251,  2255,  2266,  2267,  2273,  2283,  2288,  2294,
    2295,  2298,  2306,  2312,  2313,  2316,  2326,  2330,  2333,  2343,
    2343,  2368,  2369,  2373,  2382,  2383,  2389,  2395,  2396,  2397,
    2400,  2401,  2402,  2403,  2406,  2407,  2408,  2411,  2412,  2415,
    2416,  2419,  2420,  2423,  2424,  2425,  2428,  2429,  2432,  2433,
    2436
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "kCLASS", "kMODULE", "kDEF", "kUNDEF",
  "kBEGIN", "kRESCUE", "kENSURE", "kEND", "kIF", "kUNLESS", "kTHEN",
  "kELSIF", "kELSE", "kCASE", "kWHEN", "kWHILE", "kUNTIL", "kFOR",
  "kBREAK", "kNEXT", "kREDO", "kRETRY", "kIN", "kDO", "kDO_COND",
  "kDO_BLOCK", "kRETURN", "kYIELD", "kSUPER", "kSELF", "kNIL", "kTRUE",
  "kFALSE", "kAND", "kOR", "kNOT", "kIF_MOD", "kUNLESS_MOD", "kWHILE_MOD",
  "kUNTIL_MOD", "kRESCUE_MOD", "kALIAS", "kDEFINED", "klBEGIN", "klEND",
  "k__LINE__", "k__FILE__", "tIDENTIFIER", "tFID", "tGVAR", "tIVAR",
  "tCONSTANT", "tCVAR", "tINTEGER", "tFLOAT", "tSTRING_CONTENT",
  "tNTH_REF", "tBACK_REF", "tREGEXP_END", "tUPLUS", "tUMINUS", "tPOW",
  "tCMP", "tEQ", "tEQQ", "tNEQ", "tGEQ", "tLEQ", "tANDOP", "tOROP",
  "tMATCH", "tNMATCH", "tDOT2", "tDOT3", "tAREF", "tASET", "tLSHFT",
  "tRSHFT", "tCOLON2", "tCOLON3", "tOP_ASGN", "tASSOC", "tLPAREN",
  "tLPAREN_ARG", "tRPAREN", "tLBRACK", "tLBRACE", "tLBRACE_ARG", "tSTAR",
  "tAMPER", "tSYMBEG", "tSTRING_BEG", "tXSTRING_BEG", "tREGEXP_BEG",
  "tWORDS_BEG", "tQWORDS_BEG", "tSTRING_DBEG", "tSTRING_DVAR",
  "tSTRING_END", "tLOWEST", "'='", "'?'", "':'", "'>'", "'<'", "'|'",
  "'^'", "'&'", "'+'", "'-'", "'*'", "'/'", "'%'", "tUMINUS_NUM", "'!'",
  "'~'", "tLAST_TOKEN", "'{'", "'}'", "'['", "']'", "'.'", "')'", "','",
  "'`'", "'('", "' '", "'\\n'", "';'", "$accept", "program", "@1",
  "bodystmt", "compstmt", "stmts", "stmt", "@2", "@3", "expr",
  "expr_value", "command_call", "block_command", "cmd_brace_block", "@4",
  "@5", "command", "mlhs", "mlhs_entry", "mlhs_basic", "mlhs_item",
  "mlhs_head", "mlhs_node", "lhs", "cname", "cpath", "fname", "fitem",
  "undef_list", "@6", "op", "reswords", "arg", "@7", "arg_value",
  "aref_args", "paren_args", "opt_paren_args", "call_args", "call_args2",
  "command_args", "@8", "open_args", "@9", "@10", "block_arg",
  "opt_block_arg", "args", "mrhs", "primary", "@11", "@12", "@13", "@14",
  "@15", "@16", "@17", "@18", "@19", "@20", "@21", "@22", "@23", "@24",
  "@25", "@26", "primary_value", "then", "do", "if_tail", "opt_else",
  "block_var", "opt_block_var", "do_block", "@27", "@28", "block_call",
  "method_call", "brace_block", "@29", "@30", "@31", "@32", "case_body",
  "when_args", "cases", "opt_rescue", "exc_list", "exc_var", "opt_ensure",
  "literal", "strings", "string", "string1", "xstring", "regexp", "words",
  "word_list", "word", "qwords", "qword_list", "string_contents",
  "xstring_contents", "string_content", "@33", "@34", "string_dvar",
  "symbol", "sym", "dsym", "numeric", "variable", "var_ref", "var_lhs",
  "backref", "superclass", "@35", "f_arglist", "f_args", "f_norm_arg",
  "f_arg", "f_opt", "f_optarg", "restarg_mark", "f_rest_arg",
  "blkarg_mark", "f_block_arg", "opt_f_block_arg", "singleton", "@36",
  "assoc_list", "assocs", "assoc", "operation", "operation2", "operation3",
  "dot_or_colon", "opt_terms", "opt_nl", "trailer", "term", "terms",
  "none", 0
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
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,    61,    63,    58,    62,    60,   124,    94,
      38,    43,    45,    42,    47,    37,   358,    33,   126,   359,
     123,   125,    91,    93,    46,    41,    44,    96,    40,    32,
      10,    59
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned short int yyr1[] =
{
       0,   132,   134,   133,   135,   136,   137,   137,   137,   137,
     139,   138,   138,   138,   138,   138,   138,   138,   138,   138,
     138,   140,   138,   138,   138,   138,   138,   138,   138,   138,
     138,   138,   138,   138,   138,   138,   141,   141,   141,   141,
     141,   141,   142,   143,   143,   143,   143,   143,   144,   144,
     144,   146,   147,   145,   148,   148,   148,   148,   148,   148,
     148,   148,   149,   149,   150,   150,   151,   151,   151,   151,
     151,   151,   152,   152,   153,   153,   154,   154,   154,   154,
     154,   154,   154,   154,   155,   155,   155,   155,   155,   155,
     155,   155,   156,   156,   157,   157,   157,   158,   158,   158,
     158,   158,   159,   159,   160,   161,   160,   162,   162,   162,
     162,   162,   162,   162,   162,   162,   162,   162,   162,   162,
     162,   162,   162,   162,   162,   162,   162,   162,   162,   162,
     162,   162,   162,   163,   163,   163,   163,   163,   163,   163,
     163,   163,   163,   163,   163,   163,   163,   163,   163,   163,
     163,   163,   163,   163,   163,   163,   163,   163,   163,   163,
     163,   163,   163,   163,   163,   163,   163,   163,   163,   163,
     163,   163,   163,   163,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   165,   164,   164,   164,   166,
     167,   167,   167,   167,   167,   167,   168,   168,   168,   168,
     169,   169,   170,   170,   170,   170,   170,   170,   170,   170,
     170,   171,   171,   171,   171,   171,   171,   171,   171,   171,
     171,   171,   171,   173,   172,   174,   175,   174,   176,   174,
     177,   178,   178,   179,   179,   180,   180,   180,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   182,   181,   183,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   184,   181,   181,   181,   181,   181,   181,   185,   186,
     181,   187,   188,   181,   181,   181,   181,   189,   190,   181,
     191,   181,   192,   193,   181,   194,   181,   195,   181,   196,
     197,   181,   181,   181,   181,   181,   198,   199,   199,   199,
     199,   200,   200,   200,   201,   201,   202,   202,   203,   203,
     204,   204,   204,   204,   206,   207,   205,   208,   208,   208,
     209,   209,   209,   209,   209,   209,   211,   212,   210,   213,
     214,   210,   215,   216,   216,   216,   217,   217,   218,   218,
     219,   219,   219,   220,   220,   221,   221,   222,   222,   222,
     223,   224,   224,   225,   226,   227,   228,   228,   229,   229,
     230,   230,   231,   231,   232,   232,   233,   233,   234,   234,
     235,   236,   235,   237,   235,   238,   238,   238,   238,   239,
     240,   240,   240,   240,   241,   242,   242,   242,   242,   243,
     243,   243,   243,   243,   243,   243,   243,   243,   243,   243,
     244,   245,   246,   246,   247,   248,   247,   247,   249,   249,
     250,   250,   250,   250,   250,   250,   250,   250,   250,   251,
     251,   251,   251,   251,   252,   252,   253,   254,   254,   255,
     255,   256,   256,   257,   257,   258,   259,   259,   260,   261,
     260,   262,   262,   262,   263,   263,   264,   265,   265,   265,
     266,   266,   266,   266,   267,   267,   267,   268,   268,   269,
     269,   270,   270,   271,   271,   271,   272,   272,   273,   273,
     274
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     0,     2,     4,     2,     1,     1,     3,     2,
       0,     4,     3,     3,     3,     2,     3,     3,     3,     3,
       3,     0,     5,     4,     3,     3,     3,     6,     5,     5,
       5,     3,     3,     3,     3,     1,     1,     3,     3,     2,
       2,     1,     1,     1,     1,     2,     2,     2,     1,     4,
       4,     0,     0,     6,     2,     3,     4,     5,     4,     5,
       2,     2,     1,     3,     1,     3,     1,     2,     3,     2,
       2,     1,     1,     3,     2,     3,     1,     4,     3,     3,
       3,     3,     2,     1,     1,     4,     3,     3,     3,     3,
       2,     1,     1,     1,     2,     1,     3,     1,     1,     1,
       1,     1,     1,     1,     1,     0,     4,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     3,     5,     3,     6,     5,     5,
       5,     5,     4,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     4,     4,     2,     2,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     2,
       2,     3,     3,     3,     3,     0,     4,     5,     1,     1,
       1,     2,     2,     5,     2,     3,     3,     4,     4,     6,
       1,     1,     1,     2,     5,     2,     5,     4,     7,     3,
       1,     4,     3,     5,     7,     2,     5,     4,     6,     7,
       9,     3,     1,     0,     2,     1,     0,     3,     0,     4,
       2,     2,     1,     1,     3,     3,     4,     2,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     0,     4,     0,
       5,     3,     3,     2,     4,     3,     3,     1,     4,     3,
       1,     0,     6,     2,     1,     2,     6,     6,     0,     0,
       7,     0,     0,     7,     5,     4,     5,     0,     0,     9,
       0,     6,     0,     0,     8,     0,     5,     0,     6,     0,
       0,     9,     1,     1,     1,     1,     1,     1,     1,     1,
       2,     1,     1,     1,     1,     5,     1,     2,     1,     1,
       1,     2,     1,     3,     0,     0,     6,     2,     4,     4,
       2,     4,     4,     3,     2,     1,     0,     0,     6,     0,
       0,     6,     5,     1,     4,     2,     1,     1,     6,     1,
       1,     1,     1,     2,     1,     2,     1,     1,     1,     1,
       1,     1,     2,     3,     3,     3,     3,     3,     0,     3,
       1,     2,     3,     3,     0,     3,     0,     2,     0,     2,
       1,     0,     3,     0,     4,     1,     1,     1,     1,     2,
       1,     1,     1,     1,     3,     1,     1,     2,     2,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     0,     4,     2,     4,     2,
       6,     4,     4,     2,     4,     2,     2,     1,     0,     1,
       1,     1,     1,     1,     1,     3,     3,     1,     3,     1,
       1,     2,     1,     1,     1,     2,     2,     1,     1,     0,
       5,     1,     2,     2,     1,     3,     3,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     0,
       1,     0,     1,     0,     1,     1,     1,     1,     1,     2,
       0
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned short int yydefact[] =
{
       2,     0,     0,     1,     0,     0,     0,     0,     0,   277,
       0,     0,   489,   298,   301,     0,   322,   323,   324,   325,
     287,   290,   355,   425,   424,   426,   427,     0,     0,   491,
      21,     0,   429,   428,   419,   276,   421,   420,   422,   423,
     415,   416,   432,   433,     0,     0,     0,     0,     0,   500,
     500,    71,   398,   396,   398,   398,   388,   394,     0,     0,
       0,     3,   489,     7,    35,    36,    44,    43,     0,    62,
       0,    66,    72,     0,    41,   218,     0,    48,   294,   268,
     269,   380,   381,   270,   271,   272,   273,   378,   379,   377,
     430,   274,     0,   275,   253,     6,     9,   322,   323,   287,
     290,   355,   491,    92,    93,     0,     0,     0,     0,    95,
       0,   326,     0,   430,   275,     0,   315,   142,   153,   143,
     166,   139,   159,   149,   148,   164,   147,   146,   141,   167,
     151,   140,   154,   158,   160,   152,   145,   161,   168,   163,
     162,   155,   165,   150,   138,   157,   156,   169,   170,   171,
     172,   173,   137,   144,   135,   136,   133,   134,    97,    99,
      98,   128,   129,   126,   110,   111,   112,   115,   117,   113,
     130,   131,   118,   119,   123,   114,   116,   107,   108,   109,
     120,   121,   122,   124,   125,   127,   132,   469,   317,   100,
     101,   468,     0,   162,   155,   165,   150,   133,   134,    97,
      98,     0,   102,   104,    15,   103,     0,     0,    42,     0,
       0,     0,   430,     0,   275,     0,   497,   496,   489,     0,
     498,   490,     0,     0,     0,   339,   338,     0,     0,   430,
     275,     0,     0,     0,   232,   219,   263,    46,   240,   500,
     500,   474,    47,    45,     0,    61,     0,   500,   354,    60,
      39,     0,    10,   492,   215,     0,     0,   194,     0,   195,
     283,     0,     0,     0,    62,   279,     0,   491,     0,   493,
     493,   220,   493,     0,   493,   471,     0,    70,     0,    76,
      83,   412,   411,   413,   410,     0,   409,     0,     0,     0,
       0,     0,     0,     0,   417,   418,    40,   209,   210,     5,
     490,     0,     0,     0,     0,     0,     0,     0,   344,   347,
       0,    74,     0,    69,    67,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   500,     0,     0,     0,   359,   356,   295,
     382,     0,     0,   350,    54,   293,     0,   312,    92,    93,
      94,   417,   418,     0,   435,   310,   434,     0,   500,     0,
       0,     0,   448,   488,   487,   319,   105,     0,   500,   283,
     329,   328,     0,   327,     0,     0,   500,     0,     0,     0,
       0,     0,     0,     0,     0,   499,     0,     0,   283,     0,
     500,     0,   307,   500,   260,     0,     0,   233,   262,     0,
     235,   289,     0,   256,   255,   254,   232,   491,   500,   491,
       0,    12,    14,    13,     0,   291,     0,     0,     0,     0,
       0,     0,     0,   281,    63,   491,   491,   221,   285,   495,
     494,   222,   495,   224,   495,   473,   286,   472,    82,     0,
     500,     0,   400,   403,   401,   414,   399,   383,   397,   384,
     385,   386,   387,     0,   390,   392,     0,   393,     0,     0,
       8,    16,    17,    18,    19,    20,    37,    38,   500,     0,
      25,    33,     0,    34,     0,    68,    75,    24,   174,   263,
      32,   191,   199,   204,   205,   206,   201,   203,   213,   214,
     207,   208,   184,   185,   211,   212,     0,   200,   202,   196,
     197,   198,   186,   187,   188,   189,   190,   480,   485,   481,
     486,   253,   353,     0,   480,   482,   481,   483,   500,   480,
     481,   253,   253,   500,   500,    26,   176,    31,   183,    51,
      55,     0,   437,     0,     0,    92,    93,    96,     0,     0,
     500,     0,   491,   453,   451,   450,   449,   452,   460,   464,
     463,   459,   448,     0,     0,   454,   500,   457,   500,   462,
     500,     0,   447,     0,     0,   278,   500,   500,   369,   500,
     330,   174,   484,   282,     0,   480,   481,   500,     0,     0,
       0,   363,     0,   305,   333,   332,   299,   331,   302,   484,
     282,     0,   480,   481,     0,     0,   239,   476,     0,   264,
     261,   500,     0,     0,   475,   288,     0,    41,     0,   258,
       0,   252,   500,     0,     0,     0,     0,     0,   226,    11,
       0,   216,     0,    23,   182,    63,     0,   225,     0,   264,
      79,    81,     0,   480,   481,     0,     0,   389,   391,   395,
     192,   193,   342,     0,   345,   340,   267,     0,    73,     0,
       0,     0,     0,   352,    58,   284,     0,     0,   231,   351,
      56,   230,   349,    50,   348,    49,   360,   357,   500,   313,
       0,     0,   284,   316,     0,     0,   491,     0,   439,     0,
     443,   467,     0,   445,   461,     0,   446,   465,   320,   106,
     370,   371,   500,   372,     0,   500,   336,     0,     0,   334,
       0,   284,     0,     0,     0,   304,   306,   365,     0,     0,
       0,     0,   284,     0,   500,     0,   237,   500,   500,     0,
       0,   257,     0,   245,   227,     0,   491,   500,   500,   228,
       0,    22,   280,   491,    77,     0,   405,   406,   407,   402,
     408,   341,     0,     0,     0,   265,   175,   217,    30,   180,
     181,    59,     0,    28,   178,    29,   179,    57,     0,     0,
      52,     0,   436,   311,   470,   456,     0,   318,   455,   500,
     500,   466,     0,   458,   500,   448,     0,     0,   374,   337,
       0,     4,   376,     0,   296,     0,   297,     0,   500,     0,
       0,   308,   234,     0,   236,   251,     0,   242,   500,   500,
     259,     0,     0,   292,   223,   404,   343,     0,   266,    27,
     177,     0,     0,     0,     0,   438,     0,   441,   442,   444,
       0,     0,   373,     0,    84,    91,     0,   375,     0,   364,
     366,   367,   362,   300,   303,     0,   500,   500,     0,   241,
       0,   247,   500,   229,   346,   361,   358,     0,   314,   500,
       0,    90,     0,   500,     0,   500,   500,     0,   238,   243,
       0,   500,     0,   246,    53,   440,   321,   484,    89,     0,
     480,   481,   368,   335,   309,   500,     0,   248,   500,    85,
     244,     0,   249,   500,   250
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short int yydefgoto[] =
{
      -1,     1,     2,   377,   378,    62,    63,   424,   255,    64,
     209,    65,    66,   540,   678,   823,    67,    68,   263,    69,
      70,    71,    72,   210,   109,   110,   202,   203,   204,   574,
     527,   190,    74,   426,   236,   268,   668,   669,   237,   619,
     245,   246,   415,   620,   730,   610,   407,   269,   483,    75,
     206,   435,   630,   222,   720,   223,   721,   604,   845,   544,
     541,   771,   370,   372,   573,   785,   258,   382,   596,   708,
     709,   228,   654,   309,   478,   753,    77,    78,   355,   534,
     769,   533,   768,   394,   592,   842,   577,   702,   787,   791,
      79,    80,    81,    82,    83,    84,    85,   291,   463,    86,
     293,   287,   285,   456,   646,   645,   749,    87,   286,    88,
      89,   212,    91,   213,   214,   365,   543,   563,   564,   565,
     566,   567,   568,   569,   570,   571,   781,   690,   192,   371,
     273,   270,   241,   115,   548,   522,   375,   219,   254,   441,
     383,   221,    95
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -672
static const short int yypact[] =
{
    -672,   107,  2254,  -672,  5424,  7892,  8189,  3896,  5083,  -672,
    6763,  6763,  3777,  -672,  -672,  7991,  5630,  5630,  -672,  -672,
    5630,  4426,  4529,  -672,  -672,  -672,  -672,  6763,  4967,    24,
    -672,    73,  -672,  -672,  2117,  4014,  -672,  -672,  4117,  -672,
    -672,  -672,  -672,  -672,  7587,  7587,   115,  3187,  6763,  6866,
    7587,  8288,  4851,  -672,  -672,  -672,   123,   139,    21,  7690,
    7587,  -672,    89,   625,   238,  -672,  -672,   248,   111,  -672,
     171,  8090,  -672,   247,  9302,   217,   281,     4,    57,  -672,
    -672,   263,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,
     317,  -672,   284,   390,    36,  -672,   625,  -672,  -672,  -672,
     236,   241,    24,   337,   340,  6763,    60,  3318,   231,  -672,
      41,  -672,   318,  -672,  -672,    36,  -672,  -672,  -672,  -672,
    -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,
    -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,
      11,    59,   210,   290,  -672,  -672,  -672,  -672,  -672,  -672,
    -672,  -672,  -672,  -672,  -672,  -672,   301,   394,   425,  -672,
     514,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,
    -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,
    -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,
    -672,  -672,   532,  -672,  -672,  -672,  -672,  -672,  -672,  -672,
    -672,  4851,  -672,  -672,   250,  -672,  2615,   324,   238,    80,
     285,   402,   186,   311,   276,    80,  -672,  -672,    89,   122,
    -672,   305,  6763,  6763,   391,  -672,  -672,   403,   423,    39,
      66,  7587,  7587,  7587,  -672,  9302,   366,  -672,  -672,   332,
     351,  -672,  -672,  -672,  5316,  -672,  5733,  5630,  -672,  -672,
    -672,   213,  -672,  -672,   374,   411,  3449,  -672,   440,   446,
     439,  3187,   410,   415,   443,   238,  7587,    24,   434,   321,
     346,  -672,   387,   455,   346,  -672,   525,  -672,   448,   467,
     482,  -672,  -672,  -672,  -672,    12,  -672,    22,   381,   355,
     496,   408,   504,    72,   535,   551,  -672,  -672,  -672,  -672,
    3674,  6763,  6763,  6763,  6763,  5424,  6763,  6763,  -672,  -672,
    6969,  -672,  3187,  8288,   491,  6969,  7587,  7587,  7587,  7587,
    7587,  7587,  7587,  7587,  7587,  7587,  7587,  7587,  7587,  7587,
    7587,  7587,  7587,  7587,  7587,  7587,  7587,  7587,  7587,  7587,
    7587,  7587,  1612,  6866,  8442,  8508,  8508,  -672,  -672,  -672,
    -672,  7690,  7690,  -672,   531,  -672,   374,   238,  -672,   485,
    -672,  -672,  -672,    89,  -672,  -672,  -672,  8574,  6866,  8508,
    2615,  6763,   608,  -672,  -672,  -672,  -672,   615,   622,   277,
    -672,  -672,  2737,   624,  7587,  8640,  6866,  8706,  7587,  7587,
    2981,   630,  3571,  7072,   629,  -672,    20,    20,    99,  8772,
    6866,  8838,  -672,   527,  -672,  7587,  5836,  -672,  -672,  5939,
    -672,  -672,   534,  5527,  -672,  -672,   248,    24,   544,    19,
     546,  -672,  -672,  -672,  5083,  -672,  7587,  3449,   554,  8640,
    8706,  7587,   552,  -672,   550,    24,  9166,  -672,  -672,  7175,
    -672,  -672,  7587,  -672,  7587,  -672,  -672,  -672,   485,  8904,
    6866,  8970,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,
    -672,  -672,  -672,   338,  -672,  -672,   553,  -672,  7587,  7587,
     625,  -672,  -672,  -672,  -672,  -672,  -672,  -672,   169,  7587,
    -672,   555,   559,  -672,   561,  -672,  -672,  -672,  1775,  -672,
    -672,   446,  9371,  9371,  9371,  9371,   879,   879,  9388,  1667,
    9371,  9371,  9319,  9319,   432,   432,  2464,   879,   879,   869,
     869,   960,   149,   149,   446,   446,   446,  2365,  4632,  2484,
    4735,   241,  -672,   556,   456,  -672,   490,  -672,  4529,  -672,
    -672,  1899,  1899,   169,   169,  -672,  9302,  -672,  9302,  -672,
    -672,    89,  -672,  6763,  2615,   430,    14,  -672,   241,   557,
     241,   678,    52,   588,  -672,  -672,  -672,  -672,  -672,  -672,
    -672,  -672,   787,  2615,    89,  -672,   567,  -672,   569,   655,
     575,   657,  -672,  5199,  5083,  -672,  7278,   693,  -672,   587,
    -672,  2345,  4220,  4323,   589,   282,   303,   693,   704,   705,
    7587,   591,    80,  -672,  -672,  -672,  -672,  -672,  -672,     1,
      33,   596,   121,   153,  6763,   631,  -672,  -672,  7587,   366,
    -672,   598,  7587,   366,  -672,  -672,  7587,  9218,     6,  -672,
     597,  -672,   600,   602,  6042,  8508,  8508,   605,  -672,  -672,
    6763,  9302,   610,  -672,  9302,   498,   607,  -672,  7587,  -672,
     430,    14,   612,    54,   101,  3449,   651,  -672,  -672,  -672,
     446,   446,  -672,  7793,  -672,  -672,  -672,  7381,  -672,  7587,
    7587,  7690,  7587,  -672,   531,   500,  7690,  7690,  -672,  -672,
     531,  -672,  -672,  -672,  -672,  -672,  -672,  -672,   169,  -672,
      89,   723,  -672,  -672,   614,  7587,    24,   727,  -672,   787,
    -672,  -672,    53,  -672,  -672,    96,  -672,  -672,  -672,  -672,
     555,  -672,   656,  -672,  3084,   732,  -672,  6763,   734,  -672,
    7587,   429,  7587,  7587,   735,  -672,  -672,  -672,  7484,  2859,
    3571,  3571,   232,    20,   527,  6145,  -672,   527,   527,  6248,
     617,  -672,  6351,  -672,  -672,   248,    19,   241,   241,  -672,
      97,  -672,  -672,  9166,   530,   626,  -672,  -672,  -672,  -672,
    -672,  -672,   641,  3571,  7587,   628,  9302,  9302,  -672,  9302,
    9302,  -672,  7690,  -672,  9302,  -672,  9302,  -672,  3571,  3449,
    -672,  2615,  -672,  -672,  -672,  -672,   633,  -672,  -672,   634,
     575,  -672,   588,  -672,   575,   608,  8387,    80,  -672,  -672,
    3571,  -672,  -672,    80,  -672,  7587,  -672,  7587,   243,   740,
     742,  -672,  -672,  7587,  -672,  -672,  7587,  -672,   635,   636,
    -672,  7587,   639,  -672,  -672,  -672,  -672,   743,  -672,  -672,
    9302,   745,   647,  3449,   746,  -672,    53,  -672,  -672,  -672,
    2615,   719,  -672,   533,   467,   482,  2615,  -672,  2737,  -672,
    -672,  -672,  -672,  -672,  -672,  3571,  9239,   527,  6454,  -672,
    6557,  -672,   527,  -672,  -672,  -672,  -672,   653,  -672,   575,
     766,   485,  9036,  6866,  9102,   622,   587,   767,  -672,  -672,
    7587,   652,  7587,  -672,  -672,  -672,  -672,    93,    14,   660,
     140,   154,  -672,  -672,  -672,   527,  6660,  -672,   527,   530,
    -672,  7587,  -672,   527,  -672
};

/* YYPGOTO[NTERM-NUM].  */
static const short int yypgoto[] =
{
    -672,  -672,  -672,  -360,   431,  -672,    37,  -672,  -672,   958,
      44,    23,  -672,  -512,  -672,  -672,    49,   -13,  -193,    69,
     713,  -672,   -31,   927,   -84,   780,     7,   -23,  -672,  -672,
      45,  -672,   -16,  -672,  1378,  -282,    -7,  -483,    85,  -672,
       2,  -672,  -672,  -672,  -672,    -9,   158,    34,  -290,    30,
    -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,  -672,
    -672,  -672,  -672,  -672,  -672,  -672,   185,  -209,  -370,   -75,
    -547,   141,  -502,  -672,  -672,  -672,  -229,  -672,   714,  -672,
    -672,  -672,  -672,  -374,  -672,  -672,   -72,  -672,  -672,  -672,
    -672,  -672,  -672,   715,  -672,  -672,  -672,  -672,  -672,  -672,
    -672,  -672,   573,  -224,  -672,  -672,  -672,    -5,  -672,  -672,
    -672,   627,   790,  1246,  1107,  -672,  -672,    13,   237,   112,
    -672,  -671,   118,  -672,  -632,  -672,  -363,  -531,  -672,  -672,
    -672,    -4,  -371,   755,  -270,  -672,  -672,   -43,    84,   316,
     108,   738,   756
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -501
static const short int yytable[] =
{
     235,   235,   225,   205,   235,   252,   390,   238,   238,   572,
     551,   238,   240,   240,   188,   248,   240,   588,   419,   299,
     277,   783,   360,   205,   249,   490,   -87,   598,   257,   259,
     705,   676,   677,   235,   235,   111,   111,   693,   614,   696,
     714,    96,   363,   297,   298,   111,   274,   594,   672,   674,
     239,   239,   189,   189,   239,   215,   218,   780,   -89,   284,
     784,   523,   347,   458,   -84,   234,   234,   464,   432,   234,
     452,   614,   521,   189,   528,   531,   532,   294,   295,   -78,
     452,   111,   296,   347,   272,   345,   549,   353,   306,   307,
     405,   -91,  -425,   380,   248,  -282,   354,   189,   267,   550,
     625,   111,   242,   782,   584,   243,   -87,     3,   353,   -87,
     358,   453,   454,   455,   359,   521,   264,   528,   601,   484,
     220,   453,   454,   457,   -90,   595,   -80,   -79,   346,  -480,
     466,   550,   729,   306,   307,  -425,  -282,   392,  -282,   393,
    -424,   -89,  -481,   626,   558,   559,   -86,   -84,   364,   253,
     216,   217,   761,   -86,   253,   783,   348,   -78,   767,   -81,
     550,  -481,   -78,   560,   247,   -76,   561,   -88,   642,   260,
     220,   216,   217,   467,   -91,   391,   770,   348,   -88,   -78,
     -78,   550,   253,  -424,   681,   381,   356,    76,   559,    76,
     112,   112,   -83,   256,   859,   211,   211,   211,   -87,   572,
     227,   211,   211,   687,   -80,   211,   560,   -90,   284,   -80,
     216,   217,   211,   316,   310,   235,   235,   297,   366,   216,
     217,  -480,   813,   -87,   -87,   -82,   -80,   -80,   235,   -86,
     235,   235,    76,   211,   211,   238,   278,   238,   238,   648,
     240,   652,   240,   240,   211,   -86,   189,   -78,   827,   828,
     436,   840,   290,   829,   672,   674,   278,   -85,   704,   -88,
     393,   -88,   339,   340,   341,   421,   396,   397,   292,  -431,
     -86,   -86,   422,   423,   306,   307,   308,   653,   239,   -80,
     239,   418,   485,   547,   -88,   -88,   701,   361,   362,   -84,
     211,  -426,    76,   234,   235,   234,   416,   311,  -326,   488,
     491,   492,   493,   494,   495,   496,   497,   498,   499,   500,
     501,   502,   503,   504,   505,   506,   507,   508,   509,   510,
     511,   512,   513,   514,   515,   516,   220,   235,   875,   412,
     264,   414,   417,   480,  -426,   536,   538,   470,   487,  -326,
     -85,  -326,   475,   111,   482,   471,   472,   473,   474,   482,
     315,   437,   235,   801,   614,   737,   738,    53,   -77,   389,
     431,   614,   342,  -477,   244,   712,  -478,   351,   581,   247,
     235,  -427,   536,   538,   535,   537,   376,   235,   379,   -91,
     -90,   264,  -429,   719,   235,   -86,   713,   520,   384,   235,
     235,    76,   267,   235,   388,   736,   452,   617,   410,   367,
    -431,   629,   611,   343,   621,   344,   -88,   211,   211,   622,
     631,   824,   520,   452,  -427,   634,   460,   267,  -419,   205,
     -84,  -422,   572,   235,   841,  -429,   235,   591,   235,   211,
     520,   211,   211,    61,   235,   267,   395,   453,   454,   452,
     368,    76,   369,   -76,   520,   398,    76,   439,   402,   267,
     405,   440,   650,   651,   453,   454,  -484,  -477,   406,  -419,
    -478,  -419,  -422,   235,  -422,  -477,   452,   647,  -478,   189,
     860,   542,   442,   352,   520,  -428,   440,   409,   262,   614,
     453,   454,   459,   385,   399,    76,   211,   211,   211,   211,
      76,   211,   211,   -91,   520,   211,   316,    76,   278,   267,
     211,   623,   425,   627,   597,   597,  -419,   453,   454,   462,
     316,  -484,   795,   444,   663,   614,   -83,   440,  -428,   636,
     637,   429,   431,   664,   386,   400,   387,   401,   211,   449,
     670,   427,   -85,   673,   675,   433,   211,   211,   262,   666,
     434,   663,   -90,   337,   338,   339,   340,   341,  -430,  -419,
    -484,   699,  -484,   211,  -484,    76,   211,   438,  -480,   -86,
     235,   606,   386,  -275,   430,   -82,  -283,    76,   -64,   205,
     450,   211,   451,   667,   235,    76,   446,    76,   836,   448,
     698,   879,   -78,   762,   838,   211,   443,   680,   445,  -430,
     447,  -430,   235,   -88,   550,  -422,   235,   461,   211,   468,
     235,   707,   704,   -85,  -275,   465,  -275,  -283,   235,  -283,
     482,  -284,    76,   373,   862,   469,   -80,   486,   189,   189,
     611,   539,   743,   -65,   -73,   575,   -77,   288,   289,    90,
     576,    90,   113,   113,   113,   211,   684,   580,  -422,   593,
     225,   235,   229,   756,   757,   759,   760,   393,   723,   679,
     764,   766,  -284,   605,  -284,   863,   374,   864,   553,   615,
     554,   555,   556,   557,   301,   302,   303,   304,   305,   235,
     624,   628,   688,   735,    90,   633,   -73,   635,   279,   665,
     682,  -263,   649,   111,   758,   657,   658,   428,   683,   763,
     765,   685,   262,   689,   759,   692,   764,   766,   279,   558,
     559,   695,   235,   746,   747,   694,   748,   697,   704,   235,
      42,    43,   711,   235,   715,   716,   235,   718,   560,   722,
     807,   561,   731,   232,   725,   809,   732,   734,   211,    76,
     739,   741,   742,   773,    90,   744,   562,   777,   235,   774,
     786,   790,   810,   262,   794,   796,   820,   815,    76,   816,
     843,   793,   844,   854,  -264,   855,   858,    94,   825,    94,
     826,   848,   850,   808,   853,    94,    94,    94,   856,   726,
     776,    94,    94,   861,   874,    94,   876,   884,   886,   820,
     733,   235,    94,   889,   314,   819,   116,   846,   772,   211,
     235,   883,   349,   882,   752,   235,   350,   191,   830,   686,
     300,   778,    94,    94,    94,   271,   275,   779,     0,   211,
       0,     0,     0,   579,    94,   211,   111,     0,     0,     0,
     812,   587,     0,   589,     0,     0,     0,   814,     0,     0,
      76,   597,   235,    90,   235,     0,     0,   553,   227,   554,
     555,   556,   557,     0,   871,     0,   211,   235,     0,     0,
       0,   211,   211,     0,   235,     0,   235,     0,   632,     0,
      94,     0,    94,     0,     0,     0,     0,     0,     0,     0,
     235,     0,     0,     0,     0,   235,     0,     0,   558,   559,
       0,     0,   802,    90,     0,   804,   805,     0,    90,    76,
       0,     0,   211,     0,     0,     0,     0,   560,     0,     0,
     561,     0,     0,     0,    76,    76,    76,   520,     0,     0,
       0,     0,   267,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    90,     0,    73,
       0,    73,    90,   316,     0,     0,     0,     0,    76,    90,
     279,     0,   226,   316,     0,     0,     0,   211,   329,   330,
       0,     0,     0,    76,    76,     0,    76,     0,   329,   330,
       0,    94,     0,     0,     0,     0,   849,   851,   208,   208,
     208,   833,     0,     0,    73,    76,     0,    94,    94,   336,
     337,   338,   339,   340,   341,   250,     0,   334,   335,   336,
     337,   338,   339,   340,   341,   408,   408,    90,     0,    94,
       0,    94,    94,   420,   868,   869,   265,     0,    76,    90,
     873,    94,     0,     0,     0,    76,    94,    90,     0,    90,
       0,    76,     0,    76,   316,     0,     0,     0,     0,   887,
      76,     0,     0,     0,    73,     0,     0,     0,     0,   329,
     330,     0,     0,   890,     0,     0,   892,     0,   211,     0,
       0,   894,     0,     0,    90,    94,    94,    94,    94,    94,
      94,    94,    94,   357,     0,    94,     0,    94,     0,     0,
      94,   337,   338,   339,   340,   341,   745,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    94,   271,
       0,     0,     0,     0,     0,     0,    94,    94,     0,    93,
       0,    93,   114,   114,     0,     0,     0,     0,     0,     0,
       0,     0,   230,    94,   271,    94,    94,     0,     0,     0,
       0,     0,     0,    73,   578,   789,     0,    94,     0,     0,
       0,    94,   271,     0,     0,    94,     0,    94,     0,     0,
     798,   799,   800,     0,    93,    94,   271,     0,   280,   408,
       0,     0,     0,     0,     0,     0,     0,     0,    94,     0,
       0,    90,     0,     0,   408,     0,     0,     0,   280,     0,
     208,   208,    94,    73,   817,     0,     0,     0,    73,     0,
      90,     0,     0,     0,     0,     0,     0,     0,     0,   821,
     822,     0,     0,     0,     0,    94,   271,     0,     0,     0,
       0,     0,     0,     0,    93,     0,     0,     0,     0,     0,
       0,   837,     0,     0,     0,     0,     0,    73,     0,     0,
       0,     0,    73,     0,   655,     0,     0,     0,     0,    73,
       0,     0,     0,     0,     0,     0,     0,     0,    92,     0,
      92,     0,     0,     0,   857,     0,     0,     0,     0,   208,
     208,   208,   208,     0,   476,   477,     0,   865,     0,   866,
       0,     0,    90,     0,     0,     0,   867,     0,     0,     0,
     229,     0,     0,     0,   671,     0,     0,   671,   671,   655,
     655,     0,     0,    92,     0,     0,     0,    73,    94,    94,
       0,     0,     0,     0,     0,     0,   671,     0,     0,    73,
       0,     0,     0,    93,     0,     0,     0,    73,    94,    73,
       0,     0,   691,     0,   691,     0,   691,     0,     0,   552,
       0,    90,   703,   706,     0,   706,     0,     0,     0,     0,
       0,     0,     0,   706,     0,     0,    90,    90,    90,     0,
       0,     0,     0,    92,    73,     0,     0,     0,     0,    94,
       0,     0,     0,    93,     0,     0,     0,   408,    93,     0,
       0,   265,     0,     0,     0,     0,     0,     0,   408,    94,
      90,     0,     0,     0,     0,    94,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    90,    90,     0,    90,     0,
      94,     0,     0,     0,     0,     0,     0,    93,     0,     0,
       0,     0,    93,   834,     0,     0,    94,    90,     0,    93,
     280,    94,    94,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   655,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      90,     0,    92,     0,     0,     0,     0,    90,   788,    94,
       0,   792,    94,    90,     0,    90,     0,     0,     0,     0,
       0,    73,    90,     0,    94,    94,    94,    93,     0,     0,
     408,     0,     0,   408,   408,     0,     0,     0,     0,    93,
      73,     0,     0,   671,   671,     0,     0,    93,     0,    93,
       0,   208,    92,     0,     0,     0,     0,    92,    94,     0,
       0,     0,     0,     0,     0,     0,     0,    94,     0,     0,
       0,     0,     0,    94,    94,     0,    94,     0,     0,     0,
       0,     0,     0,     0,    93,   691,   691,     0,     0,     0,
     691,     0,     0,     0,     0,    94,    92,     0,     0,     0,
       0,    92,     0,     0,   706,     0,     0,     0,    92,     0,
       0,     0,   208,     0,   408,   408,     0,     0,     0,     0,
       0,     0,    73,     0,     0,     0,     0,     0,    94,     0,
     226,     0,     0,     0,     0,    94,     0,     0,   740,     0,
       0,    94,     0,    94,     0,     0,     0,     0,     0,     0,
      94,     0,   408,   408,     0,     0,     0,     0,   408,   403,
     404,     0,     0,     0,     0,   691,    92,     0,    94,   271,
       0,   578,   706,     0,     0,     0,     0,   408,    92,     0,
       0,    73,     0,     0,     0,     0,    92,     0,    92,     0,
       0,   408,     0,     0,   408,     0,    73,    73,    73,   408,
       0,    93,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   517,   518,     0,   208,   519,     0,     0,     0,
      93,     0,     0,    92,   161,   162,   163,   164,   165,   166,
      73,   167,   168,     0,     0,   169,     0,     0,   481,   170,
     171,   172,   173,   489,     0,    73,    73,     0,    73,     0,
       0,     0,     0,   174,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   832,     0,     0,     0,    73,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,     0,     0,
     185,   316,   317,   318,   319,   320,   321,   322,   323,   186,
     325,   326,     0,     0,     0,     0,   329,   330,     0,     0,
      73,     0,    93,   750,     0,     0,     0,    73,     0,     0,
     230,     0,     0,    73,     0,    73,     0,     0,     0,     0,
       0,   489,    73,   332,   333,   334,   335,   336,   337,   338,
     339,   340,   341,   607,   609,     0,     0,   613,     0,     0,
      92,   618,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    92,
       0,    93,     0,     0,     0,     0,     0,   639,   659,     0,
     613,     0,   639,     0,     0,     0,    93,    93,    93,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   316,
     317,   318,   319,   320,   321,   322,   323,   324,   325,   326,
     327,   328,     0,     0,   329,   330,     0,   656,     0,     0,
      93,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    93,    93,     0,    93,   331,
       0,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,    92,     0,   835,     0,     0,     0,    93,     0,  -500,
       0,  -219,     0,     0,     0,     0,     0,  -500,  -500,  -500,
       0,     0,  -500,  -500,  -500,     0,  -500,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -500,     0,     0,     0,
      93,     0,     0,     0,     0,  -500,  -500,    93,  -500,  -500,
    -500,  -500,  -500,    93,     0,    93,     0,     0,     0,     0,
      92,     0,    93,     0,   700,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    92,    92,    92,   717,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -500,     0,     0,     0,     0,     0,   724,     0,     0,     0,
     727,     0,     0,     0,   728,     0,     0,     0,     0,    92,
       0,     0,   609,     0,  -500,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    92,    92,     0,    92,     0,     0,
    -500,     0,     0,  -500,  -500,     0,     0,   247,     0,  -500,
    -500,     0,     0,     0,     0,   755,    92,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   775,     0,     0,     0,     0,     0,    92,
       0,     0,     0,     0,     0,     0,    92,     0,     0,     0,
       0,     0,    92,     0,    92,     0,     0,     0,     0,     0,
       0,    92,     0,     0,     0,     0,   639,     0,     0,     0,
       0,     0,     0,   613,     0,     0,     0,     0,     0,     0,
     613,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -477,  -477,  -477,     0,  -477,     0,     0,     0,  -477,  -477,
       0,     0,   818,  -477,     0,  -477,  -477,  -477,  -477,  -477,
    -477,  -477,     0,  -477,     0,     0,  -477,  -477,  -477,  -477,
    -477,  -477,  -477,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -477,     0,     0,  -477,  -477,  -477,  -477,  -477,
    -477,  -477,  -477,  -477,  -477,   839,  -477,  -477,     0,  -477,
    -477,     0,     0,     0,   847,     0,     0,     0,     0,   852,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -477,
       0,     0,  -477,  -477,     0,  -477,  -477,     0,  -477,  -477,
    -477,  -477,  -477,  -477,  -477,  -477,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   609,     0,   613,     0,
       0,     0,     0,  -477,  -477,  -477,     0,  -477,     0,     0,
       0,     0,     0,     0,     0,  -477,     0,     0,   885,     0,
     888,     0,     0,     0,  -500,     4,     0,     5,     6,     7,
       8,     9,     0,     0,   613,    10,    11,     0,     0,   893,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    46,     0,     0,    47,
      48,     0,    49,    50,     0,    51,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -484,     0,     0,     0,     0,
      58,    59,    60,  -484,  -484,  -484,     0,     0,     0,  -484,
    -484,     0,  -484,     0,  -500,  -500,     0,     0,   659,     0,
       0,  -484,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -484,  -484,     0,  -484,  -484,  -484,  -484,  -484,   316,
     317,   318,   319,   320,   321,   322,   323,   324,   325,   326,
     327,   328,     0,     0,   329,   330,     0,     0,     0,  -484,
    -484,  -484,  -484,  -484,  -484,  -484,  -484,  -484,  -484,  -484,
    -484,  -484,     0,     0,  -484,  -484,  -484,     0,   661,   331,
       0,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,     0,     0,     0,     0,     0,     0,     0,   -87,  -484,
       0,  -484,  -484,  -484,  -484,  -484,  -484,  -484,  -484,  -484,
    -484,     0,     0,     0,  -282,  -484,  -484,  -484,     0,  -484,
    -484,   -79,  -282,  -282,  -282,  -484,  -484,     0,  -282,  -282,
       0,  -282,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -282,  -282,     0,  -282,  -282,  -282,  -282,  -282,   316,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,     0,     0,   329,   330,     0,     0,     0,  -282,  -282,
    -282,  -282,  -282,  -282,  -282,  -282,  -282,  -282,  -282,  -282,
    -282,     0,     0,  -282,  -282,  -282,     0,   662,   331,   660,
     332,   333,   334,   335,   336,   337,   338,   339,   340,   341,
       0,     0,     0,     0,     0,     0,     0,   -89,  -282,     0,
    -282,  -282,  -282,  -282,  -282,  -282,  -282,  -282,  -282,  -282,
       0,     0,     0,     0,     0,  -282,  -282,     0,  -282,  -282,
     -81,     0,     0,     0,  -282,  -282,     4,     0,     5,     6,
       7,     8,     9,  -500,  -500,  -500,    10,    11,     0,     0,
    -500,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,     0,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,     0,    42,    43,     0,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    46,     0,     0,
      47,    48,     0,    49,    50,     0,    51,     0,    52,    53,
      54,    55,    56,    57,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    58,    59,    60,     0,     0,     0,     0,     4,     0,
       5,     6,     7,     8,     9,  -500,  -500,  -500,    10,    11,
       0,  -500,  -500,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    46,
       0,     0,    47,    48,     0,    49,    50,     0,    51,     0,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    58,    59,    60,     0,     0,     0,     0,
       4,     0,     5,     6,     7,     8,     9,  -500,  -500,  -500,
      10,    11,     0,     0,  -500,    12,  -500,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,     0,    42,    43,
       0,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    46,     0,     0,    47,    48,     0,    49,    50,     0,
      51,     0,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    58,    59,    60,     0,     0,
       0,     0,     4,     0,     5,     6,     7,     8,     9,  -500,
    -500,  -500,    10,    11,     0,     0,  -500,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,     0,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    46,     0,     0,    47,    48,     0,    49,
      50,     0,    51,     0,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     4,     0,     5,     6,     7,
       8,     9,     0,  -500,  -500,    10,    11,    58,    59,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,  -500,  -500,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    46,     0,     0,    47,
      48,     0,    49,    50,     0,    51,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     4,     0,
       5,     6,     7,     8,     9,     0,     0,     0,    10,    11,
      58,    59,    60,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,  -500,  -500,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    46,
       0,     0,   261,    48,     0,    49,    50,     0,    51,     0,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    58,    59,    60,     0,     0,     0,     0,
       0,     0,  -500,     0,     0,     0,     0,  -500,  -500,     4,
       0,     5,     6,     7,     8,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,    20,    21,    22,
      23,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,     0,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,     0,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      46,     0,     0,    47,    48,     0,    49,    50,     0,    51,
       0,    52,    53,    54,    55,    56,    57,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    58,    59,    60,     0,     0,     0,
       0,     0,     0,  -500,     0,     0,     0,     0,  -500,  -500,
       4,     0,     5,     6,     7,     8,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,     0,    42,    43,
       0,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    46,     0,     0,    47,    48,     0,    49,    50,     0,
      51,     0,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    58,    59,    60,     0,     0,
    -500,     0,     4,     0,     5,     6,     7,     8,     9,  -500,
    -500,  -500,    10,    11,     0,     0,     0,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,     0,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    46,     0,     0,    47,    48,     0,    49,
      50,     0,    51,     0,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       8,     9,     0,     0,     0,    10,    11,    58,    59,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,  -500,  -500,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    46,     0,     0,    47,
      48,     0,    49,    50,     0,    51,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      58,    59,    60,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,   395,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   107,    48,     0,    49,    50,     0,     0,     0,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    58,    59,    60,     0,     0,     0,   117,
     118,   119,   120,   121,   122,   123,   124,   216,   217,   125,
     126,   127,   128,   129,     0,     0,   130,   131,   132,   133,
     134,   135,   136,     0,     0,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,   157,   158,   159,    36,    37,
     160,    39,     0,     0,     0,     0,     0,     0,   161,   162,
     163,   164,   165,   166,     0,   167,   168,     0,     0,   169,
       0,     0,     0,   170,   171,   172,   173,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   174,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   175,   176,   177,   178,   179,   180,   181,   182,
     183,   184,     0,     0,   185,     0,     0,  -479,  -479,  -479,
       0,  -479,     0,   186,   187,  -479,  -479,     0,     0,     0,
    -479,     0,  -479,  -479,  -479,  -479,  -479,  -479,  -479,     0,
    -479,     0,     0,  -479,  -479,  -479,  -479,  -479,  -479,  -479,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -479,
       0,     0,  -479,  -479,  -479,  -479,  -479,  -479,  -479,  -479,
    -479,  -479,     0,  -479,  -479,     0,  -479,  -479,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -479,     0,     0,  -479,
    -479,     0,  -479,  -479,     0,  -479,  -479,  -479,  -479,  -479,
    -479,  -479,  -479,     0,     0,     0,     0,     0,     0,     0,
    -478,  -478,  -478,     0,  -478,     0,     0,     0,  -478,  -478,
    -479,  -479,  -479,  -478,  -479,  -478,  -478,  -478,  -478,  -478,
    -478,  -478,  -479,  -478,     0,     0,  -478,  -478,  -478,  -478,
    -478,  -478,  -478,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -478,     0,     0,  -478,  -478,  -478,  -478,  -478,
    -478,  -478,  -478,  -478,  -478,     0,  -478,  -478,     0,  -478,
    -478,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -478,
       0,     0,  -478,  -478,     0,  -478,  -478,     0,  -478,  -478,
    -478,  -478,  -478,  -478,  -478,  -478,     0,     0,     0,     0,
       0,     0,     0,  -480,  -480,  -480,     0,  -480,     0,     0,
       0,  -480,  -480,  -478,  -478,  -478,  -480,  -478,  -480,  -480,
    -480,  -480,  -480,  -480,  -480,  -478,     0,     0,     0,  -480,
    -480,  -480,  -480,  -480,  -480,  -480,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -480,     0,     0,  -480,  -480,
    -480,  -480,  -480,  -480,  -480,  -480,  -480,  -480,     0,  -480,
    -480,     0,  -480,  -480,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -480,   710,     0,  -480,  -480,     0,  -480,  -480,
       0,  -480,  -480,  -480,  -480,  -480,  -480,  -480,  -480,     0,
       0,     0,     0,   -87,     0,     0,  -481,  -481,  -481,     0,
    -481,     0,     0,     0,  -481,  -481,  -480,  -480,  -480,  -481,
       0,  -481,  -481,  -481,  -481,  -481,  -481,  -481,  -480,     0,
       0,     0,  -481,  -481,  -481,  -481,  -481,  -481,  -481,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -481,     0,
       0,  -481,  -481,  -481,  -481,  -481,  -481,  -481,  -481,  -481,
    -481,     0,  -481,  -481,     0,  -481,  -481,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -481,   662,     0,  -481,  -481,
       0,  -481,  -481,     0,  -481,  -481,  -481,  -481,  -481,  -481,
    -481,  -481,     0,     0,     0,     0,   -89,     0,     0,  -253,
    -253,  -253,     0,  -253,     0,     0,     0,  -253,  -253,  -481,
    -481,  -481,  -253,     0,  -253,  -253,  -253,  -253,  -253,  -253,
    -253,  -481,     0,     0,     0,  -253,  -253,  -253,  -253,  -253,
    -253,  -253,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -253,     0,     0,  -253,  -253,  -253,  -253,  -253,  -253,
    -253,  -253,  -253,  -253,     0,  -253,  -253,     0,  -253,  -253,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -253,     0,
       0,  -253,  -253,     0,  -253,  -253,     0,  -253,  -253,  -253,
    -253,  -253,  -253,  -253,  -253,     0,     0,     0,     0,     0,
       0,     0,  -253,  -253,  -253,     0,  -253,     0,     0,     0,
    -253,  -253,  -253,  -253,  -253,  -253,     0,  -253,  -253,  -253,
    -253,  -253,  -253,  -253,   244,     0,     0,     0,  -253,  -253,
    -253,  -253,  -253,  -253,  -253,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -253,     0,     0,  -253,  -253,  -253,
    -253,  -253,  -253,  -253,  -253,  -253,  -253,     0,  -253,  -253,
       0,  -253,  -253,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -253,     0,     0,  -253,  -253,     0,  -253,  -253,     0,
    -253,  -253,  -253,  -253,  -253,  -253,  -253,  -253,     0,     0,
       0,     0,     0,     0,     0,  -482,  -482,  -482,     0,  -482,
       0,     0,     0,  -482,  -482,  -253,  -253,  -253,  -482,     0,
    -482,  -482,  -482,  -482,  -482,  -482,  -482,   247,     0,     0,
       0,  -482,  -482,  -482,  -482,  -482,  -482,  -482,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -482,     0,     0,
    -482,  -482,  -482,  -482,  -482,  -482,  -482,  -482,  -482,  -482,
       0,  -482,  -482,     0,  -482,  -482,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -482,     0,     0,  -482,  -482,     0,
    -482,  -482,     0,  -482,  -482,  -482,  -482,  -482,  -482,  -482,
    -482,     0,     0,     0,     0,     0,     0,     0,  -483,  -483,
    -483,     0,  -483,     0,     0,     0,  -483,  -483,  -482,  -482,
    -482,  -483,     0,  -483,  -483,  -483,  -483,  -483,  -483,  -483,
    -482,     0,     0,     0,  -483,  -483,  -483,  -483,  -483,  -483,
    -483,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -483,     0,     0,  -483,  -483,  -483,  -483,  -483,  -483,  -483,
    -483,  -483,  -483,     0,  -483,  -483,     0,  -483,  -483,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -483,     0,     0,
    -483,  -483,     0,  -483,  -483,     0,  -483,  -483,  -483,  -483,
    -483,  -483,  -483,  -483,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -483,  -483,  -483,   117,   118,   119,   120,   121,   122,
     123,   124,     0,  -483,   125,   126,   127,   128,   129,     0,
       0,   130,   131,   132,   133,   134,   135,   136,     0,     0,
     137,   138,   139,   193,   194,   195,   196,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,   155,   197,
     198,   199,   159,   281,   282,   200,   283,     0,     0,     0,
       0,     0,     0,   161,   162,   163,   164,   165,   166,     0,
     167,   168,     0,     0,   169,     0,     0,     0,   170,   171,
     172,   173,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   174,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   184,     0,     0,   185,
     117,   118,   119,   120,   121,   122,   123,   124,   186,     0,
     125,   126,   127,   128,   129,     0,     0,   130,   131,   132,
     133,   134,   135,   136,     0,     0,   137,   138,   139,   193,
     194,   195,   196,   144,   145,   146,   147,   148,   149,   150,
     151,   152,   153,   154,   155,   197,   198,   199,   159,   251,
       0,   200,     0,     0,     0,     0,     0,     0,     0,   161,
     162,   163,   164,   165,   166,     0,   167,   168,     0,     0,
     169,     0,     0,     0,   170,   171,   172,   173,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   174,     0,
     201,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   184,     0,     0,   185,   117,   118,   119,   120,
     121,   122,   123,   124,   186,     0,   125,   126,   127,   128,
     129,     0,     0,   130,   131,   132,   133,   134,   135,   136,
       0,     0,   137,   138,   139,   193,   194,   195,   196,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   197,   198,   199,   159,     0,     0,   200,     0,     0,
       0,     0,     0,     0,     0,   161,   162,   163,   164,   165,
     166,     0,   167,   168,     0,     0,   169,     0,     0,     0,
     170,   171,   172,   173,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   174,     0,   201,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   184,     0,
       0,   185,   117,   118,   119,   120,   121,   122,   123,   124,
     186,     0,   125,   126,   127,   128,   129,     0,     0,   130,
     131,   132,   133,   134,   135,   136,     0,     0,   137,   138,
     139,   193,   194,   195,   196,   144,   145,   146,   147,   148,
     149,   150,   151,   152,   153,   154,   155,   197,   198,   199,
     159,     0,     0,   200,     0,     0,     0,     0,     0,     0,
       0,   161,   162,   163,   164,   165,   166,     0,   167,   168,
       0,     0,   169,     0,     0,     0,   170,   171,   172,   173,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     174,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   184,     0,     0,   185,     0,     5,
       6,     7,     0,     9,     0,     0,   186,    10,    11,     0,
       0,     0,    12,     0,    13,    14,    15,    97,    98,    18,
      19,     0,     0,     0,     0,    99,    21,    22,    23,    24,
      25,    26,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     0,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   207,     0,
       0,   107,    48,     0,    49,    50,     0,   231,   232,    52,
      53,    54,    55,    56,    57,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       8,     9,    58,   233,    60,    10,    11,     0,     0,     0,
      12,   411,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    46,     0,     0,    47,
      48,     0,    49,    50,     0,    51,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      58,    59,    60,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   107,    48,     0,    49,    50,     0,   616,   232,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    58,    59,    60,    12,     0,    13,    14,
      15,    97,    98,    18,    19,     0,     0,     0,     0,    99,
      21,    22,    23,    24,    25,    26,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    29,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   207,     0,     0,   107,    48,     0,    49,    50,
       0,   231,   232,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    58,   233,    60,    12,
       0,    13,    14,    15,    97,    98,    18,    19,     0,     0,
       0,     0,    99,    21,    22,    23,    24,    25,    26,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,     0,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   207,     0,     0,   107,   413,
       0,    49,    50,     0,   231,   232,    52,    53,    54,    55,
      56,    57,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    58,
     233,    60,    12,     0,    13,    14,    15,    97,    98,    18,
      19,     0,     0,     0,     0,    99,   100,   101,    23,    24,
      25,    26,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     0,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   207,     0,
       0,   107,    48,     0,    49,    50,     0,   608,   232,    52,
      53,    54,    55,    56,    57,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    58,   233,    60,    12,     0,    13,    14,    15,
      97,    98,    18,    19,     0,     0,     0,     0,    99,   100,
     101,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,     0,    42,    43,
       0,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   207,     0,     0,   107,    48,     0,    49,    50,     0,
     612,   232,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    58,   233,    60,    12,     0,
      13,    14,    15,    97,    98,    18,    19,     0,     0,     0,
       0,    99,    21,    22,    23,    24,    25,    26,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   207,     0,     0,   107,    48,     0,
      49,    50,     0,   608,   232,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    58,   233,
      60,    12,     0,    13,    14,    15,    97,    98,    18,    19,
       0,     0,     0,     0,    99,   100,   101,    23,    24,    25,
      26,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      29,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,     0,    42,    43,     0,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   207,     0,     0,
     107,    48,     0,    49,    50,     0,   803,   232,    52,    53,
      54,    55,    56,    57,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    58,   233,    60,    12,     0,    13,    14,    15,    97,
      98,    18,    19,     0,     0,     0,     0,    99,   100,   101,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,     0,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     207,     0,     0,   107,    48,     0,    49,    50,     0,   806,
     232,    52,    53,    54,    55,    56,    57,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    58,   233,    60,    12,     0,    13,
      14,    15,    97,    98,    18,    19,     0,     0,     0,     0,
      99,   100,   101,    23,    24,    25,    26,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    29,     0,     0,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,     0,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   207,     0,     0,   107,    48,     0,    49,
      50,     0,   811,   232,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    58,   233,    60,
      12,     0,    13,    14,    15,    97,    98,    18,    19,     0,
       0,     0,     0,    99,   100,   101,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   207,     0,     0,   107,
      48,     0,    49,    50,     0,   870,   232,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      58,   233,    60,    12,     0,    13,    14,    15,    97,    98,
      18,    19,     0,     0,     0,     0,    99,   100,   101,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   107,    48,     0,    49,    50,     0,   872,   232,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    58,   233,    60,    12,     0,    13,    14,
      15,    97,    98,    18,    19,     0,     0,     0,     0,    99,
     100,   101,    23,    24,    25,    26,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    29,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   207,     0,     0,   107,    48,     0,    49,    50,
       0,   891,   232,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    58,   233,    60,    12,
       0,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,     0,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   207,     0,     0,   107,    48,
       0,    49,    50,     0,     0,     0,    52,    53,    54,    55,
      56,    57,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    58,
      59,    60,    12,     0,    13,    14,    15,    97,    98,    18,
      19,     0,     0,     0,     0,    99,    21,    22,    23,    24,
      25,    26,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     0,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   207,     0,
       0,   107,    48,     0,    49,    50,     0,   266,     0,    52,
      53,    54,    55,    56,    57,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    58,   233,    60,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,     0,    42,    43,
       0,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   207,     0,     0,   107,    48,     0,    49,    50,     0,
     479,     0,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    58,   233,    60,    12,     0,
      13,    14,    15,    97,    98,    18,    19,     0,     0,     0,
       0,    99,   100,   101,    23,    24,    25,    26,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   207,     0,     0,   107,    48,     0,
      49,    50,     0,   590,     0,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    58,   233,
      60,    12,     0,    13,    14,    15,    97,    98,    18,    19,
       0,     0,     0,     0,    99,   100,   101,    23,    24,    25,
      26,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      29,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,     0,    42,    43,     0,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   207,     0,     0,
     107,    48,     0,    49,    50,     0,   638,     0,    52,    53,
      54,    55,    56,    57,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    58,   233,    60,    12,     0,    13,    14,    15,    97,
      98,    18,    19,     0,     0,     0,     0,    99,   100,   101,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,     0,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     207,     0,     0,   107,    48,     0,    49,    50,     0,   479,
       0,    52,    53,    54,    55,    56,    57,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    58,   233,    60,    12,     0,    13,
      14,    15,    97,    98,    18,    19,     0,     0,     0,     0,
      99,   100,   101,    23,    24,    25,    26,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    29,     0,     0,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,     0,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   207,     0,     0,   107,    48,     0,    49,
      50,     0,   754,     0,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    58,   233,    60,
      12,     0,    13,    14,    15,    97,    98,    18,    19,     0,
       0,     0,     0,    99,   100,   101,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   207,     0,     0,   107,
      48,     0,    49,    50,     0,   797,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      58,   233,    60,    12,     0,    13,    14,    15,    97,    98,
      18,    19,     0,     0,     0,     0,    99,   100,   101,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   107,    48,     0,    49,    50,     0,     0,     0,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    58,   233,    60,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    29,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   207,     0,     0,   107,    48,     0,    49,    50,
       0,     0,     0,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    58,   233,    60,    12,
       0,    13,    14,    15,    97,    98,    18,    19,     0,     0,
       0,     0,    99,   100,   101,    23,    24,    25,    26,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   102,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   224,     0,     0,    47,    48,
       0,    49,    50,     0,    51,     0,    52,    53,    54,    55,
      56,    57,     0,     0,     0,     5,     6,     7,     0,     9,
       0,   751,     0,    10,    11,     0,     0,     0,    12,   108,
      13,    14,    15,    97,    98,    18,    19,     0,     0,     0,
       0,    99,   100,   101,    23,    24,    25,    26,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   102,     0,     0,
      32,    33,   103,    35,    36,    37,   104,    39,    40,    41,
       0,    42,    43,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   105,     0,     0,   106,     0,     0,   107,    48,     0,
      49,    50,     0,     0,     0,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,   108,    13,
      14,    15,    97,    98,    18,    19,     0,     0,     0,     0,
      99,   100,   101,    23,    24,    25,    26,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   102,     0,     0,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,     0,
      42,    43,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   224,     0,     0,    47,    48,     0,    49,
      50,     0,    51,     0,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,   108,    13,    14,
      15,    97,    98,    18,    19,     0,     0,     0,     0,    99,
     100,   101,    23,    24,    25,    26,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   102,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   276,     0,     0,   312,    48,     0,    49,    50,
       0,   313,     0,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,   108,    13,    14,    15,
      97,    98,    18,    19,     0,     0,     0,     0,    99,   100,
     101,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   102,     0,     0,    32,    33,   103,
      35,    36,    37,   104,    39,    40,    41,     0,    42,    43,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   106,     0,     0,   107,    48,     0,    49,    50,     0,
       0,     0,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,   108,    13,    14,    15,    97,
      98,    18,    19,     0,     0,     0,     0,    99,   100,   101,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   102,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,     0,    42,    43,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     276,     0,     0,   107,    48,     0,    49,    50,     0,     0,
       0,    52,    53,    54,    55,    56,    57,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,   108,    13,    14,    15,    97,    98,
      18,    19,     0,     0,     0,     0,    99,   100,   101,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   102,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   831,
       0,     0,   107,    48,     0,    49,    50,     0,     0,     0,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,   524,   525,     0,     0,   526,     0,     0,     0,
       0,     0,     0,   108,   161,   162,   163,   164,   165,   166,
       0,   167,   168,     0,     0,   169,     0,     0,     0,   170,
     171,   172,   173,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   174,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   529,   525,
     185,     0,   530,     0,     0,     0,     0,     0,     0,   186,
     161,   162,   163,   164,   165,   166,     0,   167,   168,     0,
       0,   169,     0,     0,     0,   170,   171,   172,   173,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   174,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   175,   176,   177,   178,   179,   180,
     181,   182,   183,   184,   545,   518,   185,     0,   546,     0,
       0,     0,     0,     0,     0,   186,   161,   162,   163,   164,
     165,   166,     0,   167,   168,     0,     0,   169,     0,     0,
       0,   170,   171,   172,   173,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   174,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     582,   518,   185,     0,   583,     0,     0,     0,     0,     0,
       0,   186,   161,   162,   163,   164,   165,   166,     0,   167,
     168,     0,     0,   169,     0,     0,     0,   170,   171,   172,
     173,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   174,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   175,   176,   177,   178,
     179,   180,   181,   182,   183,   184,   585,   525,   185,     0,
     586,     0,     0,     0,     0,     0,     0,   186,   161,   162,
     163,   164,   165,   166,     0,   167,   168,     0,     0,   169,
       0,     0,     0,   170,   171,   172,   173,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   174,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   175,   176,   177,   178,   179,   180,   181,   182,
     183,   184,   599,   518,   185,     0,   600,     0,     0,     0,
       0,     0,     0,   186,   161,   162,   163,   164,   165,   166,
       0,   167,   168,     0,     0,   169,     0,     0,     0,   170,
     171,   172,   173,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   174,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   602,   525,
     185,     0,   603,     0,     0,     0,     0,     0,     0,   186,
     161,   162,   163,   164,   165,   166,     0,   167,   168,     0,
       0,   169,     0,     0,     0,   170,   171,   172,   173,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   174,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   175,   176,   177,   178,   179,   180,
     181,   182,   183,   184,   640,   518,   185,     0,   641,     0,
       0,     0,     0,     0,     0,   186,   161,   162,   163,   164,
     165,   166,     0,   167,   168,     0,     0,   169,     0,     0,
       0,   170,   171,   172,   173,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   174,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     643,   525,   185,     0,   644,     0,     0,     0,     0,     0,
       0,   186,   161,   162,   163,   164,   165,   166,     0,   167,
     168,     0,     0,   169,     0,     0,     0,   170,   171,   172,
     173,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   174,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   175,   176,   177,   178,
     179,   180,   181,   182,   183,   184,   877,   518,   185,     0,
     878,     0,     0,     0,     0,     0,     0,   186,   161,   162,
     163,   164,   165,   166,     0,   167,   168,     0,     0,   169,
       0,     0,     0,   170,   171,   172,   173,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   174,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   175,   176,   177,   178,   179,   180,   181,   182,
     183,   184,   880,   525,   185,     0,   881,     0,     0,     0,
       0,     0,     0,   186,   161,   162,   163,   164,   165,   166,
       0,   167,   168,     0,     0,   169,     0,     0,     0,   170,
     171,   172,   173,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   174,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,     0,     0,
     185,     0,     0,     0,     0,     0,     0,     0,     0,   186,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,     0,     0,   329,   330,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     331,     0,   332,   333,   334,   335,   336,   337,   338,   339,
     340,   341,   316,   317,   318,   319,   320,   321,   322,   323,
     324,   325,   326,   327,   328,     0,   253,   329,   330,     0,
       0,     0,  -219,   316,   317,   318,   319,   320,   321,   322,
     323,   324,   325,   326,   327,   328,     0,     0,   329,   330,
       0,     0,   331,     0,   332,   333,   334,   335,   336,   337,
     338,   339,   340,   341,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   331,  -219,   332,   333,   334,   335,   336,
     337,   338,   339,   340,   341,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   605,   316,   317,   318,   319,
     320,   321,   322,   323,   324,   325,   326,   327,   328,     0,
       0,   329,   330,   316,   317,   318,   319,   320,   321,   322,
     323,   324,   325,   326,  -501,  -501,     0,     0,   329,   330,
       0,     0,     0,     0,     0,     0,   331,     0,   332,   333,
     334,   335,   336,   337,   338,   339,   340,   341,     0,     0,
       0,     0,     0,     0,     0,   332,   333,   334,   335,   336,
     337,   338,   339,   340,   341,   316,  -501,  -501,  -501,  -501,
     321,   322,     0,     0,  -501,  -501,     0,     0,     0,     0,
     329,   330,   316,   317,   318,   319,   320,   321,   322,     0,
       0,   325,   326,     0,     0,     0,     0,   329,   330,     0,
       0,     0,     0,     0,     0,     0,     0,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,     0,     0,     0,
       0,     0,     0,     0,   332,   333,   334,   335,   336,   337,
     338,   339,   340,   341
};

static const short int yycheck[] =
{
      16,    17,    15,     8,    20,    28,   215,    16,    17,   372,
     370,    20,    16,    17,     7,    22,    20,   391,   247,    62,
      51,   692,   106,    28,    22,   315,    25,   397,    44,    45,
     577,   533,   534,    49,    50,     5,     6,   568,   409,   570,
     587,     4,     1,    59,    60,    15,    50,    27,   531,   532,
      16,    17,     7,     8,    20,    11,    12,   689,    25,    52,
     692,   343,    26,   287,    25,    16,    17,   291,   261,    20,
      58,   442,   342,    28,   344,   345,   346,    56,    57,    25,
      58,    51,    59,    26,    50,    81,   368,    94,    36,    37,
      84,    25,    81,    13,   101,    81,    94,    52,    49,   369,
      81,    71,    17,    50,   386,    20,    13,     0,   115,   108,
      50,    99,   100,   101,    54,   385,    47,   387,   400,   312,
      12,    99,   100,   101,    25,   105,    25,   126,   124,   128,
      58,   401,   126,    36,    37,   124,   122,    15,   124,    17,
      81,   108,   128,   124,    91,    92,    25,   108,   107,   130,
     130,   131,   664,    13,   130,   826,   120,   103,   670,   126,
     430,   128,   108,   110,   128,   126,   113,    13,   450,    54,
      62,   130,   131,   101,   108,   218,   678,   120,    25,   125,
     126,   451,   130,   124,   544,   105,   102,     2,    92,     4,
       5,     6,   126,   120,   826,    10,    11,    12,   105,   562,
      15,    16,    17,   563,   103,    20,   110,   108,   201,   108,
     130,   131,    27,    64,   103,   231,   232,   233,   110,   130,
     131,   128,   125,   130,   131,   126,   125,   126,   244,   108,
     246,   247,    47,    48,    49,   244,    51,   246,   247,   463,
     244,    72,   246,   247,    59,   105,   201,   126,   779,   780,
     266,   798,   129,   784,   737,   738,    71,    25,    15,   105,
      17,   108,   113,   114,   115,    52,   222,   223,   129,    83,
     130,   131,    59,    60,    36,    37,    28,   108,   244,   126,
     246,   247,   313,   367,   130,   131,   576,    56,    57,   103,
     105,    81,   107,   244,   310,   246,   247,   126,    81,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,   338,   339,   340,   341,   218,   343,   859,   244,
     261,   246,   247,   310,   124,   351,   352,   300,   315,   122,
     108,   124,   305,   313,   310,   301,   302,   303,   304,   315,
     103,   267,   368,   723,   725,   625,   626,    94,   126,    83,
      83,   732,    81,    26,   128,    83,    26,    83,   384,   128,
     386,    81,   388,   389,   351,   352,   126,   393,    54,   103,
     103,   312,    81,   592,   400,   103,    83,   342,   103,   405,
     406,   206,   343,   409,    83,   624,    58,   413,   240,    81,
      83,   424,   406,   122,   413,   124,   103,   222,   223,   413,
     426,   771,   367,    58,   124,   431,    61,   368,    81,   424,
     103,    81,   785,   439,   798,   124,   442,   393,   444,   244,
     385,   246,   247,     2,   450,   386,   131,    99,   100,    58,
     122,   256,   124,   126,   399,    54,   261,   126,    25,   400,
      84,   130,   468,   469,    99,   100,    26,   120,   126,   122,
     120,   124,   122,   479,   124,   128,    58,   129,   128,   424,
     830,   363,   126,    83,   429,    81,   130,   126,    47,   850,
      99,   100,   101,    81,    81,   300,   301,   302,   303,   304,
     305,   306,   307,   103,   449,   310,    64,   312,   313,   450,
     315,   417,   128,   419,   396,   397,    81,    99,   100,   101,
      64,    81,    83,   126,   521,   886,   126,   130,   124,   435,
     436,    81,    83,   521,   122,   122,   124,   124,   343,    81,
     528,   120,   103,   531,   532,   125,   351,   352,   107,    83,
     125,   548,   103,   111,   112,   113,   114,   115,    81,   124,
     120,   574,   122,   368,   124,   370,   371,   123,   128,   103,
     576,   403,   122,    81,   124,   126,    81,   382,   125,   574,
     122,   386,   124,    83,   590,   390,   121,   392,   787,    54,
     573,   863,   126,    83,   793,   400,   270,   543,   272,   122,
     274,   124,   608,   103,   864,    81,   612,   101,   413,    64,
     616,    14,    15,   103,   122,   101,   124,   122,   624,   124,
     576,    81,   427,    81,    81,    64,   126,   126,   573,   574,
     624,    90,   638,   125,   126,    10,   126,    54,    55,     2,
       8,     4,     5,     6,     7,   450,   552,    13,   124,    10,
     653,   657,    15,   659,   660,   661,   662,    17,   604,   541,
     666,   667,   122,   126,   124,   122,   124,   124,    50,   125,
      52,    53,    54,    55,    39,    40,    41,    42,    43,   685,
     126,   125,   564,   624,    47,   121,   126,   125,    51,   123,
     123,   126,   129,   653,   661,   126,   125,   256,    10,   666,
     667,   103,   261,   126,   710,   126,   712,   713,    71,    91,
      92,   126,   718,    52,    53,    50,    55,    50,    15,   725,
      59,    60,   123,   729,    10,    10,   732,   126,   110,   123,
     729,   113,   125,    92,   126,   729,   126,   125,   543,   544,
     125,   121,   125,    10,   107,   123,   128,    10,   754,   125,
      84,     9,   125,   312,    10,    10,   762,   121,   563,   108,
      10,   707,    10,    10,   126,    10,    10,     2,   125,     4,
     126,   126,   126,   729,   125,    10,    11,    12,   121,   611,
     686,    16,    17,    54,   121,    20,    10,    10,   126,   795,
     622,   797,    27,   123,    71,   762,     6,   803,   680,   604,
     806,   866,    78,   865,   653,   811,    81,     7,   785,   562,
      62,   689,    47,    48,    49,    49,    50,   689,    -1,   624,
      -1,    -1,    -1,   382,    59,   630,   786,    -1,    -1,    -1,
     736,   390,    -1,   392,    -1,    -1,    -1,   743,    -1,    -1,
     645,   723,   848,   206,   850,    -1,    -1,    50,   653,    52,
      53,    54,    55,    -1,   848,    -1,   661,   863,    -1,    -1,
      -1,   666,   667,    -1,   870,    -1,   872,    -1,   427,    -1,
     105,    -1,   107,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     886,    -1,    -1,    -1,    -1,   891,    -1,    -1,    91,    92,
      -1,    -1,   724,   256,    -1,   727,   728,    -1,   261,   704,
      -1,    -1,   707,    -1,    -1,    -1,    -1,   110,    -1,    -1,
     113,    -1,    -1,    -1,   719,   720,   721,   862,    -1,    -1,
      -1,    -1,   863,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   300,    -1,     2,
      -1,     4,   305,    64,    -1,    -1,    -1,    -1,   753,   312,
     313,    -1,    15,    64,    -1,    -1,    -1,   762,    79,    80,
      -1,    -1,    -1,   768,   769,    -1,   771,    -1,    79,    80,
      -1,   206,    -1,    -1,    -1,    -1,   808,   809,    10,    11,
      12,   786,    -1,    -1,    47,   790,    -1,   222,   223,   110,
     111,   112,   113,   114,   115,    27,    -1,   108,   109,   110,
     111,   112,   113,   114,   115,   239,   240,   370,    -1,   244,
      -1,   246,   247,   247,   846,   847,    48,    -1,   823,   382,
     852,   256,    -1,    -1,    -1,   830,   261,   390,    -1,   392,
      -1,   836,    -1,   838,    64,    -1,    -1,    -1,    -1,   871,
     845,    -1,    -1,    -1,   107,    -1,    -1,    -1,    -1,    79,
      80,    -1,    -1,   885,    -1,    -1,   888,    -1,   863,    -1,
      -1,   893,    -1,    -1,   427,   300,   301,   302,   303,   304,
     305,   306,   307,   105,    -1,   310,    -1,   312,    -1,    -1,
     315,   111,   112,   113,   114,   115,   645,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   343,   343,
      -1,    -1,    -1,    -1,    -1,    -1,   351,   352,    -1,     2,
      -1,     4,     5,     6,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    15,   368,   368,   370,   371,    -1,    -1,    -1,
      -1,    -1,    -1,   206,   378,   704,    -1,   382,    -1,    -1,
      -1,   386,   386,    -1,    -1,   390,    -1,   392,    -1,    -1,
     719,   720,   721,    -1,    47,   400,   400,    -1,    51,   403,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   413,    -1,
      -1,   544,    -1,    -1,   418,    -1,    -1,    -1,    71,    -1,
     222,   223,   427,   256,   753,    -1,    -1,    -1,   261,    -1,
     563,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   768,
     769,    -1,    -1,    -1,    -1,   450,   450,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   107,    -1,    -1,    -1,    -1,    -1,
      -1,   790,    -1,    -1,    -1,    -1,    -1,   300,    -1,    -1,
      -1,    -1,   305,    -1,   478,    -1,    -1,    -1,    -1,   312,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     2,    -1,
       4,    -1,    -1,    -1,   823,    -1,    -1,    -1,    -1,   301,
     302,   303,   304,    -1,   306,   307,    -1,   836,    -1,   838,
      -1,    -1,   645,    -1,    -1,    -1,   845,    -1,    -1,    -1,
     653,    -1,    -1,    -1,   528,    -1,    -1,   531,   532,   533,
     534,    -1,    -1,    47,    -1,    -1,    -1,   370,   543,   544,
      -1,    -1,    -1,    -1,    -1,    -1,   550,    -1,    -1,   382,
      -1,    -1,    -1,   206,    -1,    -1,    -1,   390,   563,   392,
      -1,    -1,   566,    -1,   568,    -1,   570,    -1,    -1,   371,
      -1,   704,   576,   577,    -1,   579,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   587,    -1,    -1,   719,   720,   721,    -1,
      -1,    -1,    -1,   107,   427,    -1,    -1,    -1,    -1,   604,
      -1,    -1,    -1,   256,    -1,    -1,    -1,   611,   261,    -1,
      -1,   413,    -1,    -1,    -1,    -1,    -1,    -1,   622,   624,
     753,    -1,    -1,    -1,    -1,   630,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   768,   769,    -1,   771,    -1,
     645,    -1,    -1,    -1,    -1,    -1,    -1,   300,    -1,    -1,
      -1,    -1,   305,   786,    -1,    -1,   661,   790,    -1,   312,
     313,   666,   667,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   678,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     823,    -1,   206,    -1,    -1,    -1,    -1,   830,   702,   704,
      -1,   705,   707,   836,    -1,   838,    -1,    -1,    -1,    -1,
      -1,   544,   845,    -1,   719,   720,   721,   370,    -1,    -1,
     724,    -1,    -1,   727,   728,    -1,    -1,    -1,    -1,   382,
     563,    -1,    -1,   737,   738,    -1,    -1,   390,    -1,   392,
      -1,   543,   256,    -1,    -1,    -1,    -1,   261,   753,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   762,    -1,    -1,
      -1,    -1,    -1,   768,   769,    -1,   771,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   427,   779,   780,    -1,    -1,    -1,
     784,    -1,    -1,    -1,    -1,   790,   300,    -1,    -1,    -1,
      -1,   305,    -1,    -1,   798,    -1,    -1,    -1,   312,    -1,
      -1,    -1,   604,    -1,   808,   809,    -1,    -1,    -1,    -1,
      -1,    -1,   645,    -1,    -1,    -1,    -1,    -1,   823,    -1,
     653,    -1,    -1,    -1,    -1,   830,    -1,    -1,   630,    -1,
      -1,   836,    -1,   838,    -1,    -1,    -1,    -1,    -1,    -1,
     845,    -1,   846,   847,    -1,    -1,    -1,    -1,   852,   231,
     232,    -1,    -1,    -1,    -1,   859,   370,    -1,   863,   863,
      -1,   865,   866,    -1,    -1,    -1,    -1,   871,   382,    -1,
      -1,   704,    -1,    -1,    -1,    -1,   390,    -1,   392,    -1,
      -1,   885,    -1,    -1,   888,    -1,   719,   720,   721,   893,
      -1,   544,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    50,    51,    -1,   707,    54,    -1,    -1,    -1,
     563,    -1,    -1,   427,    62,    63,    64,    65,    66,    67,
     753,    69,    70,    -1,    -1,    73,    -1,    -1,   310,    77,
      78,    79,    80,   315,    -1,   768,   769,    -1,   771,    -1,
      -1,    -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   786,    -1,    -1,    -1,   790,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,    -1,    -1,
     118,    64,    65,    66,    67,    68,    69,    70,    71,   127,
      73,    74,    -1,    -1,    -1,    -1,    79,    80,    -1,    -1,
     823,    -1,   645,   646,    -1,    -1,    -1,   830,    -1,    -1,
     653,    -1,    -1,   836,    -1,   838,    -1,    -1,    -1,    -1,
      -1,   393,   845,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   405,   406,    -1,    -1,   409,    -1,    -1,
     544,   413,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   563,
      -1,   704,    -1,    -1,    -1,    -1,    -1,   439,    43,    -1,
     442,    -1,   444,    -1,    -1,    -1,   719,   720,   721,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    -1,    -1,    79,    80,    -1,   479,    -1,    -1,
     753,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   768,   769,    -1,   771,   104,
      -1,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   645,    -1,   786,    -1,    -1,    -1,   790,    -1,     0,
      -1,   126,    -1,    -1,    -1,    -1,    -1,     8,     9,    10,
      -1,    -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    27,    -1,    -1,    -1,
     823,    -1,    -1,    -1,    -1,    36,    37,   830,    39,    40,
      41,    42,    43,   836,    -1,   838,    -1,    -1,    -1,    -1,
     704,    -1,   845,    -1,   576,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   719,   720,   721,   590,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      81,    -1,    -1,    -1,    -1,    -1,   608,    -1,    -1,    -1,
     612,    -1,    -1,    -1,   616,    -1,    -1,    -1,    -1,   753,
      -1,    -1,   624,    -1,   105,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   768,   769,    -1,   771,    -1,    -1,
     121,    -1,    -1,   124,   125,    -1,    -1,   128,    -1,   130,
     131,    -1,    -1,    -1,    -1,   657,   790,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   685,    -1,    -1,    -1,    -1,    -1,   823,
      -1,    -1,    -1,    -1,    -1,    -1,   830,    -1,    -1,    -1,
      -1,    -1,   836,    -1,   838,    -1,    -1,    -1,    -1,    -1,
      -1,   845,    -1,    -1,    -1,    -1,   718,    -1,    -1,    -1,
      -1,    -1,    -1,   725,    -1,    -1,    -1,    -1,    -1,    -1,
     732,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,   754,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    26,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,   797,    59,    60,    -1,    62,
      63,    -1,    -1,    -1,   806,    -1,    -1,    -1,    -1,   811,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,    92,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   848,    -1,   850,    -1,
      -1,    -1,    -1,   116,   117,   118,    -1,   120,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   128,    -1,    -1,   870,    -1,
     872,    -1,    -1,    -1,     0,     1,    -1,     3,     4,     5,
       6,     7,    -1,    -1,   886,    11,    12,    -1,    -1,   891,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    91,    -1,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     0,    -1,    -1,    -1,    -1,
     116,   117,   118,     8,     9,    10,    -1,    -1,    -1,    14,
      15,    -1,    17,    -1,   130,   131,    -1,    -1,    43,    -1,
      -1,    26,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    36,    37,    -1,    39,    40,    41,    42,    43,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    -1,    -1,    79,    80,    -1,    -1,    -1,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    -1,    -1,    79,    80,    81,    -1,    83,   104,
      -1,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   103,   104,
      -1,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,    -1,    -1,    -1,     0,   120,   121,   122,    -1,   124,
     125,   126,     8,     9,    10,   130,   131,    -1,    14,    15,
      -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      36,    37,    -1,    39,    40,    41,    42,    43,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    -1,    -1,    79,    80,    -1,    -1,    -1,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    -1,    -1,    79,    80,    81,    -1,    83,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   103,   104,    -1,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
      -1,    -1,    -1,    -1,    -1,   121,   122,    -1,   124,   125,
     126,    -1,    -1,    -1,   130,   131,     1,    -1,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    -1,    -1,
      15,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,    34,
      35,    -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    62,    63,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
      85,    86,    -1,    88,    89,    -1,    91,    -1,    93,    94,
      95,    96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   116,   117,   118,    -1,    -1,    -1,    -1,     1,    -1,
       3,     4,     5,     6,     7,   130,   131,    10,    11,    12,
      -1,    14,    15,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    38,    -1,    -1,    -1,    -1,
      -1,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,    -1,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   116,   117,   118,    -1,    -1,    -1,    -1,
       1,    -1,     3,     4,     5,     6,     7,   130,   131,    10,
      11,    12,    -1,    -1,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    38,    -1,    -1,
      -1,    -1,    -1,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,
      91,    -1,    93,    94,    95,    96,    97,    98,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   116,   117,   118,    -1,    -1,
      -1,    -1,     1,    -1,     3,     4,     5,     6,     7,   130,
     131,    10,    11,    12,    -1,    -1,    15,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    38,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,
      89,    -1,    91,    -1,    93,    94,    95,    96,    97,    98,
      -1,    -1,    -1,    -1,    -1,     1,    -1,     3,     4,     5,
       6,     7,    -1,     9,    10,    11,    12,   116,   117,   118,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,   130,   131,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    91,    -1,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,    -1,    -1,     1,    -1,
       3,     4,     5,     6,     7,    -1,    -1,    -1,    11,    12,
     116,   117,   118,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,   130,   131,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    38,    -1,    -1,    -1,    -1,
      -1,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,    -1,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   116,   117,   118,    -1,    -1,    -1,    -1,
      -1,    -1,   125,    -1,    -1,    -1,    -1,   130,   131,     1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    38,    -1,    -1,    -1,
      -1,    -1,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    -1,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,
      -1,    93,    94,    95,    96,    97,    98,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   116,   117,   118,    -1,    -1,    -1,
      -1,    -1,    -1,   125,    -1,    -1,    -1,    -1,   130,   131,
       1,    -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    38,    -1,    -1,
      -1,    -1,    -1,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,
      91,    -1,    93,    94,    95,    96,    97,    98,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   116,   117,   118,    -1,    -1,
     121,    -1,     1,    -1,     3,     4,     5,     6,     7,   130,
     131,    10,    11,    12,    -1,    -1,    -1,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    38,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,
      89,    -1,    91,    -1,    93,    94,    95,    96,    97,    98,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
       6,     7,    -1,    -1,    -1,    11,    12,   116,   117,   118,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,   130,   131,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    91,    -1,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
     116,   117,   118,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,   131,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    38,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    -1,    -1,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   116,   117,   118,    -1,    -1,    -1,     3,
       4,     5,     6,     7,     8,     9,    10,   130,   131,    13,
      14,    15,    16,    17,    -1,    -1,    20,    21,    22,    23,
      24,    25,    26,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    -1,    -1,    -1,    -1,    -1,    -1,    62,    63,
      64,    65,    66,    67,    -1,    69,    70,    -1,    -1,    73,
      -1,    -1,    -1,    77,    78,    79,    80,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    -1,    -1,   118,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,   127,   128,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      26,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    91,    92,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
     116,   117,   118,    16,   120,    18,    19,    20,    21,    22,
      23,    24,   128,    26,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,    92,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,   116,   117,   118,    16,   120,    18,    19,
      20,    21,    22,    23,    24,   128,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    83,    -1,    85,    86,    -1,    88,    89,
      -1,    91,    92,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,    -1,   103,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,   116,   117,   118,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,   128,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    83,    -1,    85,    86,
      -1,    88,    89,    -1,    91,    92,    93,    94,    95,    96,
      97,    98,    -1,    -1,    -1,    -1,   103,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,   116,
     117,   118,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,   128,    -1,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    45,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    -1,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,
      -1,    85,    86,    -1,    88,    89,    -1,    91,    92,    93,
      94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,   116,   117,   118,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,   128,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,
      91,    92,    93,    94,    95,    96,    97,    98,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,   116,   117,   118,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,   128,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
      88,    89,    -1,    91,    92,    93,    94,    95,    96,    97,
      98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,   116,   117,
     118,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
     128,    -1,    -1,    -1,    29,    30,    31,    32,    33,    34,
      35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      45,    -1,    -1,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    62,    63,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
      85,    86,    -1,    88,    89,    -1,    91,    92,    93,    94,
      95,    96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   116,   117,   118,     3,     4,     5,     6,     7,     8,
       9,    10,    -1,   128,    13,    14,    15,    16,    17,    -1,
      -1,    20,    21,    22,    23,    24,    25,    26,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    -1,    -1,    -1,
      -1,    -1,    -1,    62,    63,    64,    65,    66,    67,    -1,
      69,    70,    -1,    -1,    73,    -1,    -1,    -1,    77,    78,
      79,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,    -1,    -1,   118,
       3,     4,     5,     6,     7,     8,     9,    10,   127,    -1,
      13,    14,    15,    16,    17,    -1,    -1,    20,    21,    22,
      23,    24,    25,    26,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    62,
      63,    64,    65,    66,    67,    -1,    69,    70,    -1,    -1,
      73,    -1,    -1,    -1,    77,    78,    79,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,
      93,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,    -1,    -1,   118,     3,     4,     5,     6,
       7,     8,     9,    10,   127,    -1,    13,    14,    15,    16,
      17,    -1,    -1,    20,    21,    22,    23,    24,    25,    26,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    -1,    -1,    54,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    62,    63,    64,    65,    66,
      67,    -1,    69,    70,    -1,    -1,    73,    -1,    -1,    -1,
      77,    78,    79,    80,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    91,    -1,    93,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,    -1,
      -1,   118,     3,     4,     5,     6,     7,     8,     9,    10,
     127,    -1,    13,    14,    15,    16,    17,    -1,    -1,    20,
      21,    22,    23,    24,    25,    26,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    -1,    -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    62,    63,    64,    65,    66,    67,    -1,    69,    70,
      -1,    -1,    73,    -1,    -1,    -1,    77,    78,    79,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,    -1,    -1,   118,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,   127,    11,    12,    -1,
      -1,    -1,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    45,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    -1,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,
      -1,    85,    86,    -1,    88,    89,    -1,    91,    92,    93,
      94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
       6,     7,   116,   117,   118,    11,    12,    -1,    -1,    -1,
      16,   125,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    91,    -1,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
     116,   117,   118,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    38,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,    92,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,   116,   117,   118,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    91,    92,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,   116,   117,   118,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
      -1,    88,    89,    -1,    91,    92,    93,    94,    95,    96,
      97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,   116,
     117,   118,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    45,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    -1,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,
      -1,    85,    86,    -1,    88,    89,    -1,    91,    92,    93,
      94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,   116,   117,   118,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,
      91,    92,    93,    94,    95,    96,    97,    98,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,   116,   117,   118,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
      88,    89,    -1,    91,    92,    93,    94,    95,    96,    97,
      98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,   116,   117,
     118,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,    34,
      35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      45,    -1,    -1,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    62,    63,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
      85,    86,    -1,    88,    89,    -1,    91,    92,    93,    94,
      95,    96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,   116,   117,   118,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    -1,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,
      92,    93,    94,    95,    96,    97,    98,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,   116,   117,   118,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,
      89,    -1,    91,    92,    93,    94,    95,    96,    97,    98,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,   116,   117,   118,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    91,    92,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
     116,   117,   118,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,    92,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,   116,   117,   118,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    91,    92,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,   116,   117,   118,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    38,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
      -1,    88,    89,    -1,    -1,    -1,    93,    94,    95,    96,
      97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,   116,
     117,   118,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    45,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    -1,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,
      -1,    85,    86,    -1,    88,    89,    -1,    91,    -1,    93,
      94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,   116,   117,   118,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,
      91,    -1,    93,    94,    95,    96,    97,    98,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,   116,   117,   118,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
      88,    89,    -1,    91,    -1,    93,    94,    95,    96,    97,
      98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,   116,   117,
     118,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,    34,
      35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      45,    -1,    -1,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    62,    63,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
      85,    86,    -1,    88,    89,    -1,    91,    -1,    93,    94,
      95,    96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,   116,   117,   118,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    -1,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,
      -1,    93,    94,    95,    96,    97,    98,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,   116,   117,   118,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,
      89,    -1,    91,    -1,    93,    94,    95,    96,    97,    98,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,   116,   117,   118,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    91,    -1,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
     116,   117,   118,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    -1,    -1,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,   116,   117,   118,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    -1,    -1,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,   116,   117,   118,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
      -1,    88,    89,    -1,    91,    -1,    93,    94,    95,    96,
      97,    98,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,   108,    -1,    11,    12,    -1,    -1,    -1,    16,   116,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    79,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
      88,    89,    -1,    -1,    -1,    93,    94,    95,    96,    97,
      98,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,   116,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,
      89,    -1,    91,    -1,    93,    94,    95,    96,    97,    98,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,   116,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    91,    -1,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,   116,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,
      -1,    -1,    93,    94,    95,    96,    97,    98,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,   116,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,    -1,
      -1,    93,    94,    95,    96,    97,    98,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,   116,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    -1,    -1,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    50,    51,    -1,    -1,    54,    -1,    -1,    -1,
      -1,    -1,    -1,   116,    62,    63,    64,    65,    66,    67,
      -1,    69,    70,    -1,    -1,    73,    -1,    -1,    -1,    77,
      78,    79,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,    50,    51,
     118,    -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,   127,
      62,    63,    64,    65,    66,    67,    -1,    69,    70,    -1,
      -1,    73,    -1,    -1,    -1,    77,    78,    79,    80,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,    50,    51,   118,    -1,    54,    -1,
      -1,    -1,    -1,    -1,    -1,   127,    62,    63,    64,    65,
      66,    67,    -1,    69,    70,    -1,    -1,    73,    -1,    -1,
      -1,    77,    78,    79,    80,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    91,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
      50,    51,   118,    -1,    54,    -1,    -1,    -1,    -1,    -1,
      -1,   127,    62,    63,    64,    65,    66,    67,    -1,    69,
      70,    -1,    -1,    73,    -1,    -1,    -1,    77,    78,    79,
      80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,    50,    51,   118,    -1,
      54,    -1,    -1,    -1,    -1,    -1,    -1,   127,    62,    63,
      64,    65,    66,    67,    -1,    69,    70,    -1,    -1,    73,
      -1,    -1,    -1,    77,    78,    79,    80,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    50,    51,   118,    -1,    54,    -1,    -1,    -1,
      -1,    -1,    -1,   127,    62,    63,    64,    65,    66,    67,
      -1,    69,    70,    -1,    -1,    73,    -1,    -1,    -1,    77,
      78,    79,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,    50,    51,
     118,    -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,   127,
      62,    63,    64,    65,    66,    67,    -1,    69,    70,    -1,
      -1,    73,    -1,    -1,    -1,    77,    78,    79,    80,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,    50,    51,   118,    -1,    54,    -1,
      -1,    -1,    -1,    -1,    -1,   127,    62,    63,    64,    65,
      66,    67,    -1,    69,    70,    -1,    -1,    73,    -1,    -1,
      -1,    77,    78,    79,    80,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    91,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
      50,    51,   118,    -1,    54,    -1,    -1,    -1,    -1,    -1,
      -1,   127,    62,    63,    64,    65,    66,    67,    -1,    69,
      70,    -1,    -1,    73,    -1,    -1,    -1,    77,    78,    79,
      80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,    50,    51,   118,    -1,
      54,    -1,    -1,    -1,    -1,    -1,    -1,   127,    62,    63,
      64,    65,    66,    67,    -1,    69,    70,    -1,    -1,    73,
      -1,    -1,    -1,    77,    78,    79,    80,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    50,    51,   118,    -1,    54,    -1,    -1,    -1,
      -1,    -1,    -1,   127,    62,    63,    64,    65,    66,    67,
      -1,    69,    70,    -1,    -1,    73,    -1,    -1,    -1,    77,
      78,    79,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,    -1,    -1,
     118,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   127,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    -1,    -1,    79,    80,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     104,    -1,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    -1,   130,    79,    80,    -1,
      -1,    -1,    84,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    -1,    -1,    79,    80,
      -1,    -1,   104,    -1,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   104,   126,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   126,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    -1,
      -1,    79,    80,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    -1,    -1,    79,    80,
      -1,    -1,    -1,    -1,    -1,    -1,   104,    -1,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,    64,    65,    66,    67,    68,
      69,    70,    -1,    -1,    73,    74,    -1,    -1,    -1,    -1,
      79,    80,    64,    65,    66,    67,    68,    69,    70,    -1,
      -1,    73,    74,    -1,    -1,    -1,    -1,    79,    80,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned short int yystos[] =
{
       0,   133,   134,     0,     1,     3,     4,     5,     6,     7,
      11,    12,    16,    18,    19,    20,    21,    22,    23,    24,
      29,    30,    31,    32,    33,    34,    35,    38,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    59,    60,    62,    63,    82,    85,    86,    88,
      89,    91,    93,    94,    95,    96,    97,    98,   116,   117,
     118,   136,   137,   138,   141,   143,   144,   148,   149,   151,
     152,   153,   154,   155,   164,   181,   198,   208,   209,   222,
     223,   224,   225,   226,   227,   228,   231,   239,   241,   242,
     243,   244,   245,   246,   265,   274,   138,    21,    22,    29,
      30,    31,    45,    50,    54,    79,    82,    85,   116,   156,
     157,   181,   198,   243,   246,   265,   157,     3,     4,     5,
       6,     7,     8,     9,    10,    13,    14,    15,    16,    17,
      20,    21,    22,    23,    24,    25,    26,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      54,    62,    63,    64,    65,    66,    67,    69,    70,    73,
      77,    78,    79,    80,    91,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   118,   127,   128,   158,   162,
     163,   244,   260,    32,    33,    34,    35,    48,    49,    50,
      54,    93,   158,   159,   160,   239,   182,    82,   141,   142,
     155,   198,   243,   245,   246,   142,   130,   131,   142,   269,
     272,   273,   185,   187,    82,   149,   155,   198,   203,   243,
     246,    91,    92,   117,   148,   164,   166,   170,   177,   179,
     263,   264,   170,   170,   128,   172,   173,   128,   168,   172,
     141,    52,   159,   130,   270,   140,   120,   164,   198,   164,
      54,    85,   136,   150,   151,   141,    91,   148,   167,   179,
     263,   274,   179,   262,   263,   274,    82,   154,   198,   243,
     246,    52,    53,    55,   158,   234,   240,   233,   234,   234,
     129,   229,   129,   232,    56,    57,   143,   164,   164,   269,
     273,    39,    40,    41,    42,    43,    36,    37,    28,   205,
     103,   126,    85,    91,   152,   103,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    79,
      80,   104,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    81,   122,   124,    81,   124,    26,   120,   210,
     225,    83,    83,   168,   172,   210,   270,   141,    50,    54,
     156,    56,    57,     1,   107,   247,   272,    81,   122,   124,
     194,   261,   195,    81,   124,   268,   126,   135,   136,    54,
      13,   105,   199,   272,   103,    81,   122,   124,    83,    83,
     199,   269,    15,    17,   215,   131,   142,   142,    54,    81,
     122,   124,    25,   166,   166,    84,   126,   178,   274,   126,
     178,   125,   170,    86,   170,   174,   148,   170,   179,   208,
     274,    52,    59,    60,   139,   128,   165,   120,   136,    81,
     124,    83,   150,   125,   125,   183,   164,   270,   123,   126,
     130,   271,   126,   271,   126,   271,   121,   271,    54,    81,
     122,   124,    58,    99,   100,   101,   235,   101,   235,   101,
      61,   101,   101,   230,   235,   101,    58,   101,    64,    64,
     138,   142,   142,   142,   142,   138,   141,   141,   206,    91,
     143,   166,   179,   180,   150,   154,   126,   143,   164,   166,
     180,   164,   164,   164,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   164,   164,    50,    51,    54,
     162,   266,   267,   167,    50,    51,    54,   162,   266,    50,
      54,   266,   266,   213,   211,   143,   164,   143,   164,    90,
     145,   192,   272,   248,   191,    50,    54,   156,   266,   167,
     266,   135,   141,    50,    52,    53,    54,    55,    91,    92,
     110,   113,   128,   249,   250,   251,   252,   253,   254,   255,
     256,   257,   258,   196,   161,    10,     8,   218,   274,   136,
      13,   164,    50,    54,   167,    50,    54,   136,   215,   136,
      91,   179,   216,    10,    27,   105,   200,   272,   200,    50,
      54,   167,    50,    54,   189,   126,   178,   166,    91,   166,
     177,   263,    91,   166,   264,   125,    91,   164,   166,   171,
     175,   177,   263,   270,   126,    81,   124,   270,   125,   159,
     184,   164,   136,   121,   164,   125,   270,   270,    91,   166,
      50,    54,   167,    50,    54,   237,   236,   129,   235,   129,
     164,   164,    72,   108,   204,   274,   166,   126,   125,    43,
     105,    83,    83,   168,   172,   123,    83,    83,   168,   169,
     172,   274,   169,   172,   169,   172,   204,   204,   146,   272,
     142,   135,   123,    10,   270,   103,   250,   135,   272,   126,
     259,   274,   126,   259,    50,   126,   259,    50,   158,   159,
     166,   180,   219,   274,    15,   202,   274,    14,   201,   202,
      83,   123,    83,    83,   202,    10,    10,   166,   126,   199,
     186,   188,   123,   142,   166,   126,   178,   166,   166,   126,
     176,   125,   126,   178,   125,   148,   208,   266,   266,   125,
     141,   121,   125,   164,   123,   136,    52,    53,    55,   238,
     246,   108,   203,   207,    91,   166,   164,   164,   143,   164,
     164,   145,    83,   143,   164,   143,   164,   145,   214,   212,
     204,   193,   272,    10,   125,   166,   270,    10,   251,   254,
     256,   258,    50,   253,   256,   197,    84,   220,   274,   136,
       9,   221,   274,   142,    10,    83,    10,    91,   136,   136,
     136,   200,   178,    91,   178,   178,    91,   177,   179,   263,
     125,    91,   270,   125,   270,   121,   108,   136,   166,   143,
     164,   136,   136,   147,   135,   125,   126,   259,   259,   259,
     249,    82,   155,   198,   243,   246,   199,   136,   199,   166,
     202,   215,   217,    10,    10,   190,   164,   166,   126,   178,
     126,   178,   166,   125,    10,    10,   121,   136,    10,   256,
     135,    54,    81,   122,   124,   136,   136,   136,   178,   178,
      91,   263,    91,   178,   121,   259,    10,    50,    54,   167,
      50,    54,   218,   201,    10,   166,   126,   178,   166,   123,
     178,    91,   178,   166,   178
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
# define YYLLOC_DEFAULT(Current, Rhs, N)		\
   ((Current).first_line   = (Rhs)[1].first_line,	\
    (Current).first_column = (Rhs)[1].first_column,	\
    (Current).last_line    = (Rhs)[N].last_line,	\
    (Current).last_column  = (Rhs)[N].last_column)
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
  unsigned int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylno);
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

#if defined (YYMAXDEPTH) && YYMAXDEPTH == 0
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
  short int yyssa[YYINITDEPTH];
  short int *yyss = yyssa;
  register short int *yyssp;

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
	short int *yyss1 = yyss;


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
	short int *yyss1 = yyss;
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
        case 2:
#line 328 "parse.y"
    {
			lex_state = EXPR_BEG;
                        top_local_init();
			if (ruby_class == rb_cObject) class_nest = 0;
			else class_nest = 1;
		    }
    break;

  case 3:
#line 335 "parse.y"
    {
			if (yyvsp[0].node && !compile_for_eval) {
                            /* last expression should not be void */
			    if (nd_type(yyvsp[0].node) != NODE_BLOCK) void_expr(yyvsp[0].node);
			    else {
				NODE *node = yyvsp[0].node;
				while (node->nd_next) {
				    node = node->nd_next;
				}
				void_expr(node->nd_head);
			    }
			}
			ruby_eval_tree = block_append(ruby_eval_tree, yyvsp[0].node);
                        top_local_setup();
			class_nest = 0;
		    }
    break;

  case 4:
#line 357 "parse.y"
    {
		        yyval.node = yyvsp[-3].node;
			if (yyvsp[-2].node) {
			    yyval.node = NEW_RESCUE(yyvsp[-3].node, yyvsp[-2].node, yyvsp[-1].node);
			}
			else if (yyvsp[-1].node) {
			    rb_warn("else without rescue is useless");
			    yyval.node = block_append(yyval.node, yyvsp[-1].node);
			}
			if (yyvsp[0].node) {
			    yyval.node = NEW_ENSURE(yyval.node, yyvsp[0].node);
			}
			fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 5:
#line 374 "parse.y"
    {
			void_stmts(yyvsp[-1].node);
		        yyval.node = yyvsp[-1].node;
		    }
    break;

  case 7:
#line 382 "parse.y"
    {
			yyval.node = newline_node(yyvsp[0].node);
		    }
    break;

  case 8:
#line 386 "parse.y"
    {
			yyval.node = block_append(yyvsp[-2].node, newline_node(yyvsp[0].node));
		    }
    break;

  case 9:
#line 390 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 10:
#line 395 "parse.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 11:
#line 396 "parse.y"
    {
		        yyval.node = NEW_ALIAS(yyvsp[-2].id, yyvsp[0].id);
		    }
    break;

  case 12:
#line 400 "parse.y"
    {
		        yyval.node = NEW_VALIAS(yyvsp[-1].id, yyvsp[0].id);
		    }
    break;

  case 13:
#line 404 "parse.y"
    {
			char buf[3];

			sprintf(buf, "$%c", (char)yyvsp[0].node->nd_nth);
		        yyval.node = NEW_VALIAS(yyvsp[-1].id, rb_intern(buf));
		    }
    break;

  case 14:
#line 411 "parse.y"
    {
		        yyerror("can't make alias for the number variables");
		        yyval.node = 0;
		    }
    break;

  case 15:
#line 416 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 16:
#line 420 "parse.y"
    {
			yyval.node = NEW_IF(cond(yyvsp[0].node), yyvsp[-2].node, 0);
		        fixpos(yyval.node, yyvsp[0].node);
			if (cond_negative(&yyval.node->nd_cond)) {
		            yyval.node->nd_else = yyval.node->nd_body;
		            yyval.node->nd_body = 0;
			}
		    }
    break;

  case 17:
#line 429 "parse.y"
    {
			yyval.node = NEW_UNLESS(cond(yyvsp[0].node), yyvsp[-2].node, 0);
		        fixpos(yyval.node, yyvsp[0].node);
			if (cond_negative(&yyval.node->nd_cond)) {
		            yyval.node->nd_body = yyval.node->nd_else;
		            yyval.node->nd_else = 0;
			}
		    }
    break;

  case 18:
#line 438 "parse.y"
    {
			if (yyvsp[-2].node && nd_type(yyvsp[-2].node) == NODE_BEGIN) {
			    yyval.node = NEW_WHILE(cond(yyvsp[0].node), yyvsp[-2].node->nd_body, 0);
			}
			else {
			    yyval.node = NEW_WHILE(cond(yyvsp[0].node), yyvsp[-2].node, 1);
			}
			if (cond_negative(&yyval.node->nd_cond)) {
			    nd_set_type(yyval.node, NODE_UNTIL);
			}
		    }
    break;

  case 19:
#line 450 "parse.y"
    {
			if (yyvsp[-2].node && nd_type(yyvsp[-2].node) == NODE_BEGIN) {
			    yyval.node = NEW_UNTIL(cond(yyvsp[0].node), yyvsp[-2].node->nd_body, 0);
			}
			else {
			    yyval.node = NEW_UNTIL(cond(yyvsp[0].node), yyvsp[-2].node, 1);
			}
			if (cond_negative(&yyval.node->nd_cond)) {
			    nd_set_type(yyval.node, NODE_WHILE);
			}
		    }
    break;

  case 20:
#line 462 "parse.y"
    {
			yyval.node = NEW_RESCUE(yyvsp[-2].node, NEW_RESBODY(0,yyvsp[0].node,0), 0);
		    }
    break;

  case 21:
#line 466 "parse.y"
    {
			if (in_def || in_single) {
			    yyerror("BEGIN in method");
			}
			local_push(0);
		    }
    break;

  case 22:
#line 473 "parse.y"
    {
			ruby_eval_tree_begin = block_append(ruby_eval_tree_begin,
						            NEW_PREEXE(yyvsp[-1].node));
		        local_pop();
		        yyval.node = 0;
		    }
    break;

  case 23:
#line 480 "parse.y"
    {
			if (in_def || in_single) {
			    rb_warn("END in method; use at_exit");
			}

			yyval.node = NEW_ITER(0, NEW_POSTEXE(), yyvsp[-1].node);
		    }
    break;

  case 24:
#line 488 "parse.y"
    {
			yyval.node = node_assign(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 25:
#line 492 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyvsp[-2].node->nd_value = (yyvsp[-2].node->nd_head) ? NEW_TO_ARY(yyvsp[0].node) : NEW_ARRAY(yyvsp[0].node);
			yyval.node = yyvsp[-2].node;
		    }
    break;

  case 26:
#line 498 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-2].node) {
			    ID vid = yyvsp[-2].node->nd_vid;
			    if (yyvsp[-1].id == tOROP) {
				yyvsp[-2].node->nd_value = yyvsp[0].node;
				yyval.node = NEW_OP_ASGN_OR(gettable(vid), yyvsp[-2].node);
				if (is_asgn_or_id(vid)) {
				    yyval.node->nd_aid = vid;
				}
			    }
			    else if (yyvsp[-1].id == tANDOP) {
				yyvsp[-2].node->nd_value = yyvsp[0].node;
				yyval.node = NEW_OP_ASGN_AND(gettable(vid), yyvsp[-2].node);
			    }
			    else {
				yyval.node = yyvsp[-2].node;
				yyval.node->nd_value = call_op(gettable(vid),yyvsp[-1].id,1,yyvsp[0].node);
			    }
			}
			else {
			    yyval.node = 0;
			}
		    }
    break;

  case 27:
#line 523 "parse.y"
    {
                        NODE *args;

			value_expr(yyvsp[0].node);
		        args = NEW_LIST(yyvsp[0].node);
			if (yyvsp[-3].node && nd_type(yyvsp[-3].node) != NODE_ARRAY)
			    yyvsp[-3].node = NEW_LIST(yyvsp[-3].node);
			yyvsp[-3].node = list_append(yyvsp[-3].node, NEW_NIL());
			list_concat(args, yyvsp[-3].node);
			if (yyvsp[-1].id == tOROP) {
			    yyvsp[-1].id = 0;
			}
			else if (yyvsp[-1].id == tANDOP) {
			    yyvsp[-1].id = 1;
			}
			yyval.node = NEW_OP_ASGN1(yyvsp[-5].node, yyvsp[-1].id, args);
		        fixpos(yyval.node, yyvsp[-5].node);
		    }
    break;

  case 28:
#line 542 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-1].id == tOROP) {
			    yyvsp[-1].id = 0;
			}
			else if (yyvsp[-1].id == tANDOP) {
			    yyvsp[-1].id = 1;
			}
			yyval.node = NEW_OP_ASGN2(yyvsp[-4].node, yyvsp[-2].id, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 29:
#line 554 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-1].id == tOROP) {
			    yyvsp[-1].id = 0;
			}
			else if (yyvsp[-1].id == tANDOP) {
			    yyvsp[-1].id = 1;
			}
			yyval.node = NEW_OP_ASGN2(yyvsp[-4].node, yyvsp[-2].id, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 30:
#line 566 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-1].id == tOROP) {
			    yyvsp[-1].id = 0;
			}
			else if (yyvsp[-1].id == tANDOP) {
			    yyvsp[-1].id = 1;
			}
			yyval.node = NEW_OP_ASGN2(yyvsp[-4].node, yyvsp[-2].id, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 31:
#line 578 "parse.y"
    {
		        rb_backref_error(yyvsp[-2].node);
			yyval.node = 0;
		    }
    break;

  case 32:
#line 583 "parse.y"
    {
			yyval.node = node_assign(yyvsp[-2].node, NEW_SVALUE(yyvsp[0].node));
		    }
    break;

  case 33:
#line 587 "parse.y"
    {
			yyvsp[-2].node->nd_value = (yyvsp[-2].node->nd_head) ? NEW_TO_ARY(yyvsp[0].node) : NEW_ARRAY(yyvsp[0].node);
			yyval.node = yyvsp[-2].node;
		    }
    break;

  case 34:
#line 592 "parse.y"
    {
			yyvsp[-2].node->nd_value = yyvsp[0].node;
			yyval.node = yyvsp[-2].node;
		    }
    break;

  case 37:
#line 601 "parse.y"
    {
			yyval.node = logop(NODE_AND, yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 38:
#line 605 "parse.y"
    {
			yyval.node = logop(NODE_OR, yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 39:
#line 609 "parse.y"
    {
			yyval.node = NEW_NOT(cond(yyvsp[0].node));
		    }
    break;

  case 40:
#line 613 "parse.y"
    {
			yyval.node = NEW_NOT(cond(yyvsp[0].node));
		    }
    break;

  case 42:
#line 620 "parse.y"
    {
			value_expr(yyval.node);
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 45:
#line 629 "parse.y"
    {
			yyval.node = NEW_RETURN(ret_args(yyvsp[0].node));
		    }
    break;

  case 46:
#line 633 "parse.y"
    {
			yyval.node = NEW_BREAK(ret_args(yyvsp[0].node));
		    }
    break;

  case 47:
#line 637 "parse.y"
    {
			yyval.node = NEW_NEXT(ret_args(yyvsp[0].node));
		    }
    break;

  case 49:
#line 644 "parse.y"
    {
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		    }
    break;

  case 50:
#line 648 "parse.y"
    {
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		    }
    break;

  case 51:
#line 654 "parse.y"
    {
			yyval.vars = dyna_push();
			yyvsp[0].num = ruby_sourceline;
		    }
    break;

  case 52:
#line 658 "parse.y"
    {yyval.vars = ruby_dyna_vars;}
    break;

  case 53:
#line 661 "parse.y"
    {
			yyval.node = NEW_ITER(yyvsp[-3].node, 0, dyna_init(yyvsp[-1].node, yyvsp[-2].vars));
			nd_set_line(yyval.node, yyvsp[-5].num);
			dyna_pop(yyvsp[-4].vars);
		    }
    break;

  case 54:
#line 669 "parse.y"
    {
			yyval.node = new_fcall(yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[0].node);
		   }
    break;

  case 55:
#line 674 "parse.y"
    {
			yyval.node = new_fcall(yyvsp[-2].id, yyvsp[-1].node);
			if (yyvsp[0].node) {
			    if (nd_type(yyval.node) == NODE_BLOCK_PASS) {
				rb_compile_error("both block arg and actual block given");
			    }
			    yyvsp[0].node->nd_iter = yyval.node;
			    yyval.node = yyvsp[0].node;
			}
		        fixpos(yyval.node, yyvsp[-1].node);
		   }
    break;

  case 56:
#line 686 "parse.y"
    {
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 57:
#line 691 "parse.y"
    {
			yyval.node = new_call(yyvsp[-4].node, yyvsp[-2].id, yyvsp[-1].node);
			if (yyvsp[0].node) {
			    if (nd_type(yyval.node) == NODE_BLOCK_PASS) {
				rb_compile_error("both block arg and actual block given");
			    }
			    yyvsp[0].node->nd_iter = yyval.node;
			    yyval.node = yyvsp[0].node;
			}
		        fixpos(yyval.node, yyvsp[-4].node);
		   }
    break;

  case 58:
#line 703 "parse.y"
    {
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 59:
#line 708 "parse.y"
    {
			yyval.node = new_call(yyvsp[-4].node, yyvsp[-2].id, yyvsp[-1].node);
			if (yyvsp[0].node) {
			    if (nd_type(yyval.node) == NODE_BLOCK_PASS) {
				rb_compile_error("both block arg and actual block given");
			    }
			    yyvsp[0].node->nd_iter = yyval.node;
			    yyval.node = yyvsp[0].node;
			}
		        fixpos(yyval.node, yyvsp[-4].node);
		   }
    break;

  case 60:
#line 720 "parse.y"
    {
			yyval.node = new_super(yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[0].node);
		    }
    break;

  case 61:
#line 725 "parse.y"
    {
			yyval.node = new_yield(yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[0].node);
		    }
    break;

  case 63:
#line 733 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 65:
#line 740 "parse.y"
    {
			yyval.node = NEW_MASGN(NEW_LIST(yyvsp[-1].node), 0);
		    }
    break;

  case 66:
#line 746 "parse.y"
    {
			yyval.node = NEW_MASGN(yyvsp[0].node, 0);
		    }
    break;

  case 67:
#line 750 "parse.y"
    {
			yyval.node = NEW_MASGN(list_append(yyvsp[-1].node,yyvsp[0].node), 0);
		    }
    break;

  case 68:
#line 754 "parse.y"
    {
			yyval.node = NEW_MASGN(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 69:
#line 758 "parse.y"
    {
			yyval.node = NEW_MASGN(yyvsp[-1].node, -1);
		    }
    break;

  case 70:
#line 762 "parse.y"
    {
			yyval.node = NEW_MASGN(0, yyvsp[0].node);
		    }
    break;

  case 71:
#line 766 "parse.y"
    {
			yyval.node = NEW_MASGN(0, -1);
		    }
    break;

  case 73:
#line 773 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 74:
#line 779 "parse.y"
    {
			yyval.node = NEW_LIST(yyvsp[-1].node);
		    }
    break;

  case 75:
#line 783 "parse.y"
    {
			yyval.node = list_append(yyvsp[-2].node, yyvsp[-1].node);
		    }
    break;

  case 76:
#line 789 "parse.y"
    {
			yyval.node = assignable(yyvsp[0].id, 0);
		    }
    break;

  case 77:
#line 793 "parse.y"
    {
			yyval.node = aryset(yyvsp[-3].node, yyvsp[-1].node);
		    }
    break;

  case 78:
#line 797 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 79:
#line 801 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 80:
#line 805 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 81:
#line 809 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			yyval.node = NEW_CDECL(0, 0, NEW_COLON2(yyvsp[-2].node, yyvsp[0].id));
		    }
    break;

  case 82:
#line 815 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			yyval.node = NEW_CDECL(0, 0, NEW_COLON3(yyvsp[0].id));
		    }
    break;

  case 83:
#line 821 "parse.y"
    {
		        rb_backref_error(yyvsp[0].node);
			yyval.node = 0;
		    }
    break;

  case 84:
#line 828 "parse.y"
    {
			yyval.node = assignable(yyvsp[0].id, 0);
		    }
    break;

  case 85:
#line 832 "parse.y"
    {
			yyval.node = aryset(yyvsp[-3].node, yyvsp[-1].node);
		    }
    break;

  case 86:
#line 836 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 87:
#line 840 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 88:
#line 844 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 89:
#line 848 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			yyval.node = NEW_CDECL(0, 0, NEW_COLON2(yyvsp[-2].node, yyvsp[0].id));
		    }
    break;

  case 90:
#line 854 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			yyval.node = NEW_CDECL(0, 0, NEW_COLON3(yyvsp[0].id));
		    }
    break;

  case 91:
#line 860 "parse.y"
    {
		        rb_backref_error(yyvsp[0].node);
			yyval.node = 0;
		    }
    break;

  case 92:
#line 867 "parse.y"
    {
			yyerror("class/module name must be CONSTANT");
		    }
    break;

  case 94:
#line 874 "parse.y"
    {
			yyval.node = NEW_COLON3(yyvsp[0].id);
		    }
    break;

  case 95:
#line 878 "parse.y"
    {
			yyval.node = NEW_COLON2(0, yyval.node);
		    }
    break;

  case 96:
#line 882 "parse.y"
    {
			yyval.node = NEW_COLON2(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 100:
#line 891 "parse.y"
    {
			lex_state = EXPR_END;
			yyval.id = yyvsp[0].id;
		    }
    break;

  case 101:
#line 896 "parse.y"
    {
			lex_state = EXPR_END;
			yyval.id = yyvsp[0].id;
		    }
    break;

  case 104:
#line 907 "parse.y"
    {
			yyval.node = NEW_UNDEF(yyvsp[0].id);
		    }
    break;

  case 105:
#line 910 "parse.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 106:
#line 911 "parse.y"
    {
			yyval.node = block_append(yyvsp[-3].node, NEW_UNDEF(yyvsp[0].id));
		    }
    break;

  case 107:
#line 916 "parse.y"
    { yyval.id = '|'; }
    break;

  case 108:
#line 917 "parse.y"
    { yyval.id = '^'; }
    break;

  case 109:
#line 918 "parse.y"
    { yyval.id = '&'; }
    break;

  case 110:
#line 919 "parse.y"
    { yyval.id = tCMP; }
    break;

  case 111:
#line 920 "parse.y"
    { yyval.id = tEQ; }
    break;

  case 112:
#line 921 "parse.y"
    { yyval.id = tEQQ; }
    break;

  case 113:
#line 922 "parse.y"
    { yyval.id = tMATCH; }
    break;

  case 114:
#line 923 "parse.y"
    { yyval.id = '>'; }
    break;

  case 115:
#line 924 "parse.y"
    { yyval.id = tGEQ; }
    break;

  case 116:
#line 925 "parse.y"
    { yyval.id = '<'; }
    break;

  case 117:
#line 926 "parse.y"
    { yyval.id = tLEQ; }
    break;

  case 118:
#line 927 "parse.y"
    { yyval.id = tLSHFT; }
    break;

  case 119:
#line 928 "parse.y"
    { yyval.id = tRSHFT; }
    break;

  case 120:
#line 929 "parse.y"
    { yyval.id = '+'; }
    break;

  case 121:
#line 930 "parse.y"
    { yyval.id = '-'; }
    break;

  case 122:
#line 931 "parse.y"
    { yyval.id = '*'; }
    break;

  case 123:
#line 932 "parse.y"
    { yyval.id = '*'; }
    break;

  case 124:
#line 933 "parse.y"
    { yyval.id = '/'; }
    break;

  case 125:
#line 934 "parse.y"
    { yyval.id = '%'; }
    break;

  case 126:
#line 935 "parse.y"
    { yyval.id = tPOW; }
    break;

  case 127:
#line 936 "parse.y"
    { yyval.id = '~'; }
    break;

  case 128:
#line 937 "parse.y"
    { yyval.id = tUPLUS; }
    break;

  case 129:
#line 938 "parse.y"
    { yyval.id = tUMINUS; }
    break;

  case 130:
#line 939 "parse.y"
    { yyval.id = tAREF; }
    break;

  case 131:
#line 940 "parse.y"
    { yyval.id = tASET; }
    break;

  case 132:
#line 941 "parse.y"
    { yyval.id = '`'; }
    break;

  case 174:
#line 954 "parse.y"
    {
			yyval.node = node_assign(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 175:
#line 958 "parse.y"
    {
			yyval.node = node_assign(yyvsp[-4].node, NEW_RESCUE(yyvsp[-2].node, NEW_RESBODY(0,yyvsp[0].node,0), 0));
		    }
    break;

  case 176:
#line 962 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-2].node) {
			    ID vid = yyvsp[-2].node->nd_vid;
			    if (yyvsp[-1].id == tOROP) {
				yyvsp[-2].node->nd_value = yyvsp[0].node;
				yyval.node = NEW_OP_ASGN_OR(gettable(vid), yyvsp[-2].node);
				if (is_asgn_or_id(vid)) {
				    yyval.node->nd_aid = vid;
				}
			    }
			    else if (yyvsp[-1].id == tANDOP) {
				yyvsp[-2].node->nd_value = yyvsp[0].node;
				yyval.node = NEW_OP_ASGN_AND(gettable(vid), yyvsp[-2].node);
			    }
			    else {
				yyval.node = yyvsp[-2].node;
				yyval.node->nd_value = call_op(gettable(vid),yyvsp[-1].id,1,yyvsp[0].node);
			    }
			}
			else {
			    yyval.node = 0;
			}
		    }
    break;

  case 177:
#line 987 "parse.y"
    {
                        NODE *args;

			value_expr(yyvsp[0].node);
			args = NEW_LIST(yyvsp[0].node);
			if (yyvsp[-3].node && nd_type(yyvsp[-3].node) != NODE_ARRAY)
			    yyvsp[-3].node = NEW_LIST(yyvsp[-3].node);
			yyvsp[-3].node = list_append(yyvsp[-3].node, NEW_NIL());
			list_concat(args, yyvsp[-3].node);
			if (yyvsp[-1].id == tOROP) {
			    yyvsp[-1].id = 0;
			}
			else if (yyvsp[-1].id == tANDOP) {
			    yyvsp[-1].id = 1;
			}
			yyval.node = NEW_OP_ASGN1(yyvsp[-5].node, yyvsp[-1].id, args);
		        fixpos(yyval.node, yyvsp[-5].node);
		    }
    break;

  case 178:
#line 1006 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-1].id == tOROP) {
			    yyvsp[-1].id = 0;
			}
			else if (yyvsp[-1].id == tANDOP) {
			    yyvsp[-1].id = 1;
			}
			yyval.node = NEW_OP_ASGN2(yyvsp[-4].node, yyvsp[-2].id, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 179:
#line 1018 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-1].id == tOROP) {
			    yyvsp[-1].id = 0;
			}
			else if (yyvsp[-1].id == tANDOP) {
			    yyvsp[-1].id = 1;
			}
			yyval.node = NEW_OP_ASGN2(yyvsp[-4].node, yyvsp[-2].id, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 180:
#line 1030 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-1].id == tOROP) {
			    yyvsp[-1].id = 0;
			}
			else if (yyvsp[-1].id == tANDOP) {
			    yyvsp[-1].id = 1;
			}
			yyval.node = NEW_OP_ASGN2(yyvsp[-4].node, yyvsp[-2].id, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 181:
#line 1042 "parse.y"
    {
			yyerror("constant re-assignment");
			yyval.node = 0;
		    }
    break;

  case 182:
#line 1047 "parse.y"
    {
			yyerror("constant re-assignment");
			yyval.node = 0;
		    }
    break;

  case 183:
#line 1052 "parse.y"
    {
		        rb_backref_error(yyvsp[-2].node);
			yyval.node = 0;
		    }
    break;

  case 184:
#line 1057 "parse.y"
    {
			value_expr(yyvsp[-2].node);
			value_expr(yyvsp[0].node);
			yyval.node = NEW_DOT2(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 185:
#line 1063 "parse.y"
    {
			value_expr(yyvsp[-2].node);
			value_expr(yyvsp[0].node);
			yyval.node = NEW_DOT3(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 186:
#line 1069 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '+', 1, yyvsp[0].node);
		    }
    break;

  case 187:
#line 1073 "parse.y"
    {
		        yyval.node = call_op(yyvsp[-2].node, '-', 1, yyvsp[0].node);
		    }
    break;

  case 188:
#line 1077 "parse.y"
    {
		        yyval.node = call_op(yyvsp[-2].node, '*', 1, yyvsp[0].node);
		    }
    break;

  case 189:
#line 1081 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '/', 1, yyvsp[0].node);
		    }
    break;

  case 190:
#line 1085 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '%', 1, yyvsp[0].node);
		    }
    break;

  case 191:
#line 1089 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tPOW, 1, yyvsp[0].node);
		    }
    break;

  case 192:
#line 1093 "parse.y"
    {
			yyval.node = call_op(call_op(yyvsp[-2].node, tPOW, 1, yyvsp[0].node), tUMINUS, 0, 0);
		    }
    break;

  case 193:
#line 1097 "parse.y"
    {
			yyval.node = call_op(call_op(yyvsp[-2].node, tPOW, 1, yyvsp[0].node), tUMINUS, 0, 0);
		    }
    break;

  case 194:
#line 1101 "parse.y"
    {
			if (yyvsp[0].node && nd_type(yyvsp[0].node) == NODE_LIT) {
			    yyval.node = yyvsp[0].node;
			}
			else {
			    yyval.node = call_op(yyvsp[0].node, tUPLUS, 0, 0);
			}
		    }
    break;

  case 195:
#line 1110 "parse.y"
    {
			yyval.node = call_op(yyvsp[0].node, tUMINUS, 0, 0);
		    }
    break;

  case 196:
#line 1114 "parse.y"
    {
		        yyval.node = call_op(yyvsp[-2].node, '|', 1, yyvsp[0].node);
		    }
    break;

  case 197:
#line 1118 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '^', 1, yyvsp[0].node);
		    }
    break;

  case 198:
#line 1122 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '&', 1, yyvsp[0].node);
		    }
    break;

  case 199:
#line 1126 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tCMP, 1, yyvsp[0].node);
		    }
    break;

  case 200:
#line 1130 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '>', 1, yyvsp[0].node);
		    }
    break;

  case 201:
#line 1134 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tGEQ, 1, yyvsp[0].node);
		    }
    break;

  case 202:
#line 1138 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '<', 1, yyvsp[0].node);
		    }
    break;

  case 203:
#line 1142 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tLEQ, 1, yyvsp[0].node);
		    }
    break;

  case 204:
#line 1146 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tEQ, 1, yyvsp[0].node);
		    }
    break;

  case 205:
#line 1150 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tEQQ, 1, yyvsp[0].node);
		    }
    break;

  case 206:
#line 1154 "parse.y"
    {
			yyval.node = NEW_NOT(call_op(yyvsp[-2].node, tEQ, 1, yyvsp[0].node));
		    }
    break;

  case 207:
#line 1158 "parse.y"
    {
			yyval.node = match_gen(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 208:
#line 1162 "parse.y"
    {
			yyval.node = NEW_NOT(match_gen(yyvsp[-2].node, yyvsp[0].node));
		    }
    break;

  case 209:
#line 1166 "parse.y"
    {
			yyval.node = NEW_NOT(cond(yyvsp[0].node));
		    }
    break;

  case 210:
#line 1170 "parse.y"
    {
			yyval.node = call_op(yyvsp[0].node, '~', 0, 0);
		    }
    break;

  case 211:
#line 1174 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tLSHFT, 1, yyvsp[0].node);
		    }
    break;

  case 212:
#line 1178 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tRSHFT, 1, yyvsp[0].node);
		    }
    break;

  case 213:
#line 1182 "parse.y"
    {
			yyval.node = logop(NODE_AND, yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 214:
#line 1186 "parse.y"
    {
			yyval.node = logop(NODE_OR, yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 215:
#line 1189 "parse.y"
    {in_defined = 1;}
    break;

  case 216:
#line 1190 "parse.y"
    {
		        in_defined = 0;
			yyval.node = NEW_DEFINED(yyvsp[0].node);
		    }
    break;

  case 217:
#line 1195 "parse.y"
    {
			yyval.node = NEW_IF(cond(yyvsp[-4].node), yyvsp[-2].node, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 218:
#line 1200 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 219:
#line 1206 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 221:
#line 1214 "parse.y"
    {
		        rb_warn("parenthesize argument(s) for future version");
			yyval.node = NEW_LIST(yyvsp[-1].node);
		    }
    break;

  case 222:
#line 1219 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 223:
#line 1223 "parse.y"
    {
			value_expr(yyvsp[-1].node);
			yyval.node = arg_concat(yyvsp[-4].node, yyvsp[-1].node);
		    }
    break;

  case 224:
#line 1228 "parse.y"
    {
			yyval.node = NEW_LIST(NEW_HASH(yyvsp[-1].node));
		    }
    break;

  case 225:
#line 1232 "parse.y"
    {
			value_expr(yyvsp[-1].node);
			yyval.node = NEW_NEWLINE(NEW_SPLAT(yyvsp[-1].node));
		    }
    break;

  case 226:
#line 1239 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 227:
#line 1243 "parse.y"
    {
			yyval.node = yyvsp[-2].node;
		    }
    break;

  case 228:
#line 1247 "parse.y"
    {
		        rb_warn("parenthesize argument for future version");
			yyval.node = NEW_LIST(yyvsp[-2].node);
		    }
    break;

  case 229:
#line 1252 "parse.y"
    {
		        rb_warn("parenthesize argument for future version");
			yyval.node = list_append(yyvsp[-4].node, yyvsp[-2].node);
		    }
    break;

  case 232:
#line 1263 "parse.y"
    {
		        rb_warn("parenthesize argument(s) for future version");
			yyval.node = NEW_LIST(yyvsp[0].node);
		    }
    break;

  case 233:
#line 1268 "parse.y"
    {
			yyval.node = arg_blk_pass(yyvsp[-1].node, yyvsp[0].node);
		    }
    break;

  case 234:
#line 1272 "parse.y"
    {
			yyval.node = arg_concat(yyvsp[-4].node, yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 235:
#line 1277 "parse.y"
    {
			yyval.node = NEW_LIST(NEW_HASH(yyvsp[-1].node));
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 236:
#line 1282 "parse.y"
    {
			yyval.node = arg_concat(NEW_LIST(NEW_HASH(yyvsp[-4].node)), yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 237:
#line 1287 "parse.y"
    {
			yyval.node = list_append(yyvsp[-3].node, NEW_HASH(yyvsp[-1].node));
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 238:
#line 1292 "parse.y"
    {
			value_expr(yyvsp[-1].node);
			yyval.node = arg_concat(list_append(yyvsp[-6].node, NEW_HASH(yyvsp[-4].node)), yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 239:
#line 1298 "parse.y"
    {
			yyval.node = arg_blk_pass(NEW_SPLAT(yyvsp[-1].node), yyvsp[0].node);
		    }
    break;

  case 241:
#line 1305 "parse.y"
    {
			yyval.node = arg_blk_pass(list_concat(NEW_LIST(yyvsp[-3].node),yyvsp[-1].node), yyvsp[0].node);
		    }
    break;

  case 242:
#line 1309 "parse.y"
    {
                        yyval.node = arg_blk_pass(yyvsp[-2].node, yyvsp[0].node);
                    }
    break;

  case 243:
#line 1313 "parse.y"
    {
			yyval.node = arg_concat(NEW_LIST(yyvsp[-4].node), yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 244:
#line 1318 "parse.y"
    {
                       yyval.node = arg_concat(list_concat(NEW_LIST(yyvsp[-6].node),yyvsp[-4].node), yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 245:
#line 1323 "parse.y"
    {
			yyval.node = NEW_LIST(NEW_HASH(yyvsp[-1].node));
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 246:
#line 1328 "parse.y"
    {
			yyval.node = arg_concat(NEW_LIST(NEW_HASH(yyvsp[-4].node)), yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 247:
#line 1333 "parse.y"
    {
			yyval.node = list_append(NEW_LIST(yyvsp[-3].node), NEW_HASH(yyvsp[-1].node));
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 248:
#line 1338 "parse.y"
    {
			yyval.node = list_append(list_concat(NEW_LIST(yyvsp[-5].node),yyvsp[-3].node), NEW_HASH(yyvsp[-1].node));
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 249:
#line 1343 "parse.y"
    {
			yyval.node = arg_concat(list_append(NEW_LIST(yyvsp[-6].node), NEW_HASH(yyvsp[-4].node)), yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 250:
#line 1348 "parse.y"
    {
			yyval.node = arg_concat(list_append(list_concat(NEW_LIST(yyvsp[-8].node), yyvsp[-6].node), NEW_HASH(yyvsp[-4].node)), yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 251:
#line 1353 "parse.y"
    {
			yyval.node = arg_blk_pass(NEW_SPLAT(yyvsp[-1].node), yyvsp[0].node);
		    }
    break;

  case 253:
#line 1359 "parse.y"
    {
			yyval.num = cmdarg_stack;
			CMDARG_PUSH(1);
		    }
    break;

  case 254:
#line 1364 "parse.y"
    {
			/* CMDARG_POP() */
		        cmdarg_stack = yyvsp[-1].num;
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 256:
#line 1372 "parse.y"
    {lex_state = EXPR_ENDARG;}
    break;

  case 257:
#line 1373 "parse.y"
    {
		        rb_warn("don't put space before argument parentheses");
			yyval.node = 0;
		    }
    break;

  case 258:
#line 1377 "parse.y"
    {lex_state = EXPR_ENDARG;}
    break;

  case 259:
#line 1378 "parse.y"
    {
		        rb_warn("don't put space before argument parentheses");
			yyval.node = yyvsp[-2].node;
		    }
    break;

  case 260:
#line 1385 "parse.y"
    {
			yyval.node = NEW_BLOCK_PASS(yyvsp[0].node);
		    }
    break;

  case 261:
#line 1391 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 263:
#line 1398 "parse.y"
    {
			yyval.node = NEW_LIST(yyvsp[0].node);
		    }
    break;

  case 264:
#line 1402 "parse.y"
    {
			yyval.node = list_append(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 265:
#line 1408 "parse.y"
    {
			yyval.node = list_append(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 266:
#line 1412 "parse.y"
    {
			yyval.node = arg_concat(yyvsp[-3].node, yyvsp[0].node);
		    }
    break;

  case 267:
#line 1416 "parse.y"
    {
			yyval.node = NEW_SPLAT(yyvsp[0].node);
		    }
    break;

  case 276:
#line 1430 "parse.y"
    {
			yyval.node = NEW_FCALL(yyvsp[0].id, 0);
		    }
    break;

  case 277:
#line 1434 "parse.y"
    {
			yyvsp[0].num = ruby_sourceline;
		    }
    break;

  case 278:
#line 1439 "parse.y"
    {
			if (yyvsp[-1].node == NULL)
			    yyval.node = NEW_NIL();
			else
			    yyval.node = NEW_BEGIN(yyvsp[-1].node);
			nd_set_line(yyval.node, yyvsp[-3].num);
		    }
    break;

  case 279:
#line 1446 "parse.y"
    {lex_state = EXPR_ENDARG;}
    break;

  case 280:
#line 1447 "parse.y"
    {
		        rb_warning("(...) interpreted as grouped expression");
			yyval.node = yyvsp[-3].node;
		    }
    break;

  case 281:
#line 1452 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 282:
#line 1456 "parse.y"
    {
			yyval.node = NEW_COLON2(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 283:
#line 1460 "parse.y"
    {
			yyval.node = NEW_COLON3(yyvsp[0].id);
		    }
    break;

  case 284:
#line 1464 "parse.y"
    {
			if (yyvsp[-3].node && nd_type(yyvsp[-3].node) == NODE_SELF)
			    yyval.node = NEW_FCALL(tAREF, yyvsp[-1].node);
			else
			    yyval.node = NEW_CALL(yyvsp[-3].node, tAREF, yyvsp[-1].node);
			fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 285:
#line 1472 "parse.y"
    {
		        if (yyvsp[-1].node == 0) {
			    yyval.node = NEW_ZARRAY(); /* zero length array*/
			}
			else {
			    yyval.node = yyvsp[-1].node;
			}
		    }
    break;

  case 286:
#line 1481 "parse.y"
    {
			yyval.node = NEW_HASH(yyvsp[-1].node);
		    }
    break;

  case 287:
#line 1485 "parse.y"
    {
			yyval.node = NEW_RETURN(0);
		    }
    break;

  case 288:
#line 1489 "parse.y"
    {
			yyval.node = new_yield(yyvsp[-1].node);
		    }
    break;

  case 289:
#line 1493 "parse.y"
    {
			yyval.node = NEW_YIELD(0, Qfalse);
		    }
    break;

  case 290:
#line 1497 "parse.y"
    {
			yyval.node = NEW_YIELD(0, Qfalse);
		    }
    break;

  case 291:
#line 1500 "parse.y"
    {in_defined = 1;}
    break;

  case 292:
#line 1501 "parse.y"
    {
		        in_defined = 0;
			yyval.node = NEW_DEFINED(yyvsp[-1].node);
		    }
    break;

  case 293:
#line 1506 "parse.y"
    {
			yyvsp[0].node->nd_iter = NEW_FCALL(yyvsp[-1].id, 0);
			yyval.node = yyvsp[0].node;
			fixpos(yyvsp[0].node->nd_iter, yyvsp[0].node);
		    }
    break;

  case 295:
#line 1513 "parse.y"
    {
			if (yyvsp[-1].node && nd_type(yyvsp[-1].node) == NODE_BLOCK_PASS) {
			    rb_compile_error("both block arg and actual block given");
			}
			yyvsp[0].node->nd_iter = yyvsp[-1].node;
			yyval.node = yyvsp[0].node;
		        fixpos(yyval.node, yyvsp[-1].node);
		    }
    break;

  case 296:
#line 1525 "parse.y"
    {
			yyval.node = NEW_IF(cond(yyvsp[-4].node), yyvsp[-2].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-4].node);
			if (cond_negative(&yyval.node->nd_cond)) {
		            NODE *tmp = yyval.node->nd_body;
		            yyval.node->nd_body = yyval.node->nd_else;
		            yyval.node->nd_else = tmp;
			}
		    }
    break;

  case 297:
#line 1538 "parse.y"
    {
			yyval.node = NEW_UNLESS(cond(yyvsp[-4].node), yyvsp[-2].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-4].node);
			if (cond_negative(&yyval.node->nd_cond)) {
		            NODE *tmp = yyval.node->nd_body;
		            yyval.node->nd_body = yyval.node->nd_else;
		            yyval.node->nd_else = tmp;
			}
		    }
    break;

  case 298:
#line 1547 "parse.y"
    {COND_PUSH(1);}
    break;

  case 299:
#line 1547 "parse.y"
    {COND_POP();}
    break;

  case 300:
#line 1550 "parse.y"
    {
			yyval.node = NEW_WHILE(cond(yyvsp[-4].node), yyvsp[-1].node, 1);
		        fixpos(yyval.node, yyvsp[-4].node);
			if (cond_negative(&yyval.node->nd_cond)) {
			    nd_set_type(yyval.node, NODE_UNTIL);
			}
		    }
    break;

  case 301:
#line 1557 "parse.y"
    {COND_PUSH(1);}
    break;

  case 302:
#line 1557 "parse.y"
    {COND_POP();}
    break;

  case 303:
#line 1560 "parse.y"
    {
			yyval.node = NEW_UNTIL(cond(yyvsp[-4].node), yyvsp[-1].node, 1);
		        fixpos(yyval.node, yyvsp[-4].node);
			if (cond_negative(&yyval.node->nd_cond)) {
			    nd_set_type(yyval.node, NODE_WHILE);
			}
		    }
    break;

  case 304:
#line 1570 "parse.y"
    {
			yyval.node = NEW_CASE(yyvsp[-3].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 305:
#line 1575 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 306:
#line 1579 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 307:
#line 1582 "parse.y"
    {COND_PUSH(1);}
    break;

  case 308:
#line 1582 "parse.y"
    {COND_POP();}
    break;

  case 309:
#line 1585 "parse.y"
    {
			yyval.node = NEW_FOR(yyvsp[-7].node, yyvsp[-4].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-7].node);
		    }
    break;

  case 310:
#line 1590 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("class definition in method body");
			class_nest++;
			local_push(0);
		        yyval.num = ruby_sourceline;
		    }
    break;

  case 311:
#line 1599 "parse.y"
    {
		        yyval.node = NEW_CLASS(yyvsp[-4].node, yyvsp[-1].node, yyvsp[-3].node);
		        nd_set_line(yyval.node, yyvsp[-2].num);
		        local_pop();
			class_nest--;
		    }
    break;

  case 312:
#line 1606 "parse.y"
    {
			yyval.num = in_def;
		        in_def = 0;
		    }
    break;

  case 313:
#line 1611 "parse.y"
    {
		        yyval.num = in_single;
		        in_single = 0;
			class_nest++;
			local_push(0);
		    }
    break;

  case 314:
#line 1619 "parse.y"
    {
		        yyval.node = NEW_SCLASS(yyvsp[-5].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-5].node);
		        local_pop();
			class_nest--;
		        in_def = yyvsp[-4].num;
		        in_single = yyvsp[-2].num;
		    }
    break;

  case 315:
#line 1628 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("module definition in method body");
			class_nest++;
			local_push(0);
		        yyval.num = ruby_sourceline;
		    }
    break;

  case 316:
#line 1637 "parse.y"
    {
		        yyval.node = NEW_MODULE(yyvsp[-3].node, yyvsp[-1].node);
		        nd_set_line(yyval.node, yyvsp[-2].num);
		        local_pop();
			class_nest--;
		    }
    break;

  case 317:
#line 1644 "parse.y"
    {
			yyval.id = cur_mid;
			cur_mid = yyvsp[0].id;
			in_def++;
			local_push(0);
		    }
    break;

  case 318:
#line 1653 "parse.y"
    {
			if (!yyvsp[-1].node) yyvsp[-1].node = NEW_NIL();
			yyval.node = NEW_DEFN(yyvsp[-4].id, yyvsp[-2].node, yyvsp[-1].node, NOEX_PRIVATE);
		        fixpos(yyval.node, yyvsp[-2].node);
		        local_pop();
			in_def--;
			cur_mid = yyvsp[-3].id;
		    }
    break;

  case 319:
#line 1661 "parse.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 320:
#line 1662 "parse.y"
    {
			in_single++;
			local_push(0);
		        lex_state = EXPR_END; /* force for args */
		    }
    break;

  case 321:
#line 1670 "parse.y"
    {
			yyval.node = NEW_DEFS(yyvsp[-7].node, yyvsp[-4].id, yyvsp[-2].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-7].node);
		        local_pop();
			in_single--;
		    }
    break;

  case 322:
#line 1677 "parse.y"
    {
			yyval.node = NEW_BREAK(0);
		    }
    break;

  case 323:
#line 1681 "parse.y"
    {
			yyval.node = NEW_NEXT(0);
		    }
    break;

  case 324:
#line 1685 "parse.y"
    {
			yyval.node = NEW_REDO();
		    }
    break;

  case 325:
#line 1689 "parse.y"
    {
			yyval.node = NEW_RETRY();
		    }
    break;

  case 326:
#line 1695 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 335:
#line 1716 "parse.y"
    {
			yyval.node = NEW_IF(cond(yyvsp[-3].node), yyvsp[-1].node, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 337:
#line 1724 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 341:
#line 1735 "parse.y"
    {
			yyval.node = (NODE*)1;
		    }
    break;

  case 342:
#line 1739 "parse.y"
    {
			yyval.node = (NODE*)1;
		    }
    break;

  case 343:
#line 1743 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 344:
#line 1749 "parse.y"
    {
		        yyval.vars = dyna_push();
			yyvsp[0].num = ruby_sourceline;
		    }
    break;

  case 345:
#line 1753 "parse.y"
    {yyval.vars = ruby_dyna_vars;}
    break;

  case 346:
#line 1756 "parse.y"
    {
			yyval.node = NEW_ITER(yyvsp[-3].node, 0, dyna_init(yyvsp[-1].node, yyvsp[-2].vars));
			nd_set_line(yyval.node, yyvsp[-5].num);
			dyna_pop(yyvsp[-4].vars);
		    }
    break;

  case 347:
#line 1764 "parse.y"
    {
			if (yyvsp[-1].node && nd_type(yyvsp[-1].node) == NODE_BLOCK_PASS) {
			    rb_compile_error("both block arg and actual block given");
			}
			yyvsp[0].node->nd_iter = yyvsp[-1].node;
			yyval.node = yyvsp[0].node;
		        fixpos(yyval.node, yyvsp[-1].node);
		    }
    break;

  case 348:
#line 1773 "parse.y"
    {
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		    }
    break;

  case 349:
#line 1777 "parse.y"
    {
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		    }
    break;

  case 350:
#line 1783 "parse.y"
    {
			yyval.node = new_fcall(yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[0].node);
		    }
    break;

  case 351:
#line 1788 "parse.y"
    {
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 352:
#line 1793 "parse.y"
    {
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 353:
#line 1798 "parse.y"
    {
			yyval.node = new_call(yyvsp[-2].node, yyvsp[0].id, 0);
		    }
    break;

  case 354:
#line 1802 "parse.y"
    {
			yyval.node = new_super(yyvsp[0].node);
		    }
    break;

  case 355:
#line 1806 "parse.y"
    {
			yyval.node = NEW_ZSUPER();
		    }
    break;

  case 356:
#line 1812 "parse.y"
    {
		        yyval.vars = dyna_push();
			yyvsp[0].num = ruby_sourceline;
		    }
    break;

  case 357:
#line 1816 "parse.y"
    {yyval.vars = ruby_dyna_vars;}
    break;

  case 358:
#line 1818 "parse.y"
    {
			yyval.node = NEW_ITER(yyvsp[-3].node, 0, dyna_init(yyvsp[-1].node, yyvsp[-2].vars));
			nd_set_line(yyval.node, yyvsp[-5].num);
			dyna_pop(yyvsp[-4].vars);
		    }
    break;

  case 359:
#line 1824 "parse.y"
    {
		        yyval.vars = dyna_push();
			yyvsp[0].num = ruby_sourceline;
		    }
    break;

  case 360:
#line 1828 "parse.y"
    {yyval.vars = ruby_dyna_vars;}
    break;

  case 361:
#line 1830 "parse.y"
    {
			yyval.node = NEW_ITER(yyvsp[-3].node, 0, dyna_init(yyvsp[-1].node, yyvsp[-2].vars));
			nd_set_line(yyval.node, yyvsp[-5].num);
			dyna_pop(yyvsp[-4].vars);
		    }
    break;

  case 362:
#line 1840 "parse.y"
    {
			yyval.node = NEW_WHEN(yyvsp[-3].node, yyvsp[-1].node, yyvsp[0].node);
		    }
    break;

  case 364:
#line 1846 "parse.y"
    {
			yyval.node = list_append(yyvsp[-3].node, NEW_WHEN(yyvsp[0].node, 0, 0));
		    }
    break;

  case 365:
#line 1850 "parse.y"
    {
			yyval.node = NEW_LIST(NEW_WHEN(yyvsp[0].node, 0, 0));
		    }
    break;

  case 368:
#line 1862 "parse.y"
    {
		        if (yyvsp[-3].node) {
		            yyvsp[-3].node = node_assign(yyvsp[-3].node, NEW_GVAR(rb_intern("$!")));
			    yyvsp[-1].node = block_append(yyvsp[-3].node, yyvsp[-1].node);
			}
			yyval.node = NEW_RESBODY(yyvsp[-4].node, yyvsp[-1].node, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-4].node?yyvsp[-4].node:yyvsp[-1].node);
		    }
    break;

  case 370:
#line 1874 "parse.y"
    {
			yyval.node = NEW_LIST(yyvsp[0].node);
		    }
    break;

  case 373:
#line 1882 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 375:
#line 1889 "parse.y"
    {
			if (yyvsp[0].node)
			    yyval.node = yyvsp[0].node;
			else
			    /* place holder */
			    yyval.node = NEW_NIL();
		    }
    break;

  case 378:
#line 1901 "parse.y"
    {
			yyval.node = NEW_LIT(ID2SYM(yyvsp[0].id));
		    }
    break;

  case 380:
#line 1908 "parse.y"
    {
			NODE *node = yyvsp[0].node;
			if (!node) {
			    node = NEW_STR(rb_str_new(0, 0));
			}
			else {
			    node = evstr2dstr(node);
			}
			yyval.node = node;
		    }
    break;

  case 382:
#line 1922 "parse.y"
    {
			yyval.node = literal_concat(yyvsp[-1].node, yyvsp[0].node);
		    }
    break;

  case 383:
#line 1928 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 384:
#line 1934 "parse.y"
    {
			NODE *node = yyvsp[-1].node;
			if (!node) {
			    node = NEW_XSTR(rb_str_new(0, 0));
			}
			else {
			    switch (nd_type(node)) {
			      case NODE_STR:
				nd_set_type(node, NODE_XSTR);
				break;
			      case NODE_DSTR:
				nd_set_type(node, NODE_DXSTR);
				break;
			      default:
				node = NEW_NODE(NODE_DXSTR, rb_str_new(0, 0), 1, NEW_LIST(node));
				break;
			    }
			}
			yyval.node = node;
		    }
    break;

  case 385:
#line 1957 "parse.y"
    {
			int options = yyvsp[0].num;
			NODE *node = yyvsp[-1].node;
			if (!node) {
			    node = NEW_LIT(rb_reg_new("", 0, options & ~RE_OPTION_ONCE));
			}
			else switch (nd_type(node)) {
			  case NODE_STR:
			    {
				VALUE src = node->nd_lit;
				nd_set_type(node, NODE_LIT);
				node->nd_lit = rb_reg_new(RSTRING(src)->ptr,
							  RSTRING(src)->len,
							  options & ~RE_OPTION_ONCE);
			    }
			    break;
			  default:
			    node = NEW_NODE(NODE_DSTR, rb_str_new(0, 0), 1, NEW_LIST(node));
			  case NODE_DSTR:
			    if (options & RE_OPTION_ONCE) {
				nd_set_type(node, NODE_DREGX_ONCE);
			    }
			    else {
				nd_set_type(node, NODE_DREGX);
			    }
			    node->nd_cflag = options & ~RE_OPTION_ONCE;
			    break;
			}
			yyval.node = node;
		    }
    break;

  case 386:
#line 1990 "parse.y"
    {
			yyval.node = NEW_ZARRAY();
		    }
    break;

  case 387:
#line 1994 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 388:
#line 2000 "parse.y"
    {
			yyval.node = 0;
		    }
    break;

  case 389:
#line 2004 "parse.y"
    {
			yyval.node = list_append(yyvsp[-2].node, evstr2dstr(yyvsp[-1].node));
		    }
    break;

  case 391:
#line 2011 "parse.y"
    {
			yyval.node = literal_concat(yyvsp[-1].node, yyvsp[0].node);
		    }
    break;

  case 392:
#line 2017 "parse.y"
    {
			yyval.node = NEW_ZARRAY();
		    }
    break;

  case 393:
#line 2021 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 394:
#line 2027 "parse.y"
    {
			yyval.node = 0;
		    }
    break;

  case 395:
#line 2031 "parse.y"
    {
			yyval.node = list_append(yyvsp[-2].node, yyvsp[-1].node);
		    }
    break;

  case 396:
#line 2037 "parse.y"
    {
			yyval.node = 0;
		    }
    break;

  case 397:
#line 2041 "parse.y"
    {
			yyval.node = literal_concat(yyvsp[-1].node, yyvsp[0].node);
		    }
    break;

  case 398:
#line 2047 "parse.y"
    {
			yyval.node = 0;
		    }
    break;

  case 399:
#line 2051 "parse.y"
    {
			yyval.node = literal_concat(yyvsp[-1].node, yyvsp[0].node);
		    }
    break;

  case 401:
#line 2058 "parse.y"
    {
			yyval.node = lex_strterm;
			lex_strterm = 0;
			lex_state = EXPR_BEG;
		    }
    break;

  case 402:
#line 2064 "parse.y"
    {
			lex_strterm = yyvsp[-1].node;
		        yyval.node = NEW_EVSTR(yyvsp[0].node);
		    }
    break;

  case 403:
#line 2069 "parse.y"
    {
			yyval.node = lex_strterm;
			lex_strterm = 0;
			lex_state = EXPR_BEG;
			COND_PUSH(0);
			CMDARG_PUSH(0);
		    }
    break;

  case 404:
#line 2077 "parse.y"
    {
			lex_strterm = yyvsp[-2].node;
			COND_LEXPOP();
			CMDARG_LEXPOP();
			if ((yyval.node = yyvsp[-1].node) && nd_type(yyval.node) == NODE_NEWLINE) {
			    yyval.node = yyval.node->nd_next;
			    rb_gc_force_recycle((VALUE)yyvsp[-1].node);
			}
			yyval.node = new_evstr(yyval.node);
		    }
    break;

  case 405:
#line 2089 "parse.y"
    {yyval.node = NEW_GVAR(yyvsp[0].id);}
    break;

  case 406:
#line 2090 "parse.y"
    {yyval.node = NEW_IVAR(yyvsp[0].id);}
    break;

  case 407:
#line 2091 "parse.y"
    {yyval.node = NEW_CVAR(yyvsp[0].id);}
    break;

  case 409:
#line 2096 "parse.y"
    {
		        lex_state = EXPR_END;
			yyval.id = yyvsp[0].id;
		    }
    break;

  case 414:
#line 2109 "parse.y"
    {
		        lex_state = EXPR_END;
			if (!(yyval.node = yyvsp[-1].node)) {
			    yyerror("empty symbol literal");
			}
			else {
			    switch (nd_type(yyval.node)) {
			      case NODE_DSTR:
				nd_set_type(yyval.node, NODE_DSYM);
				break;
			      case NODE_STR:
				if (strlen(RSTRING(yyval.node->nd_lit)->ptr) == RSTRING(yyval.node->nd_lit)->len) {
				    yyval.node->nd_lit = ID2SYM(rb_intern(RSTRING(yyval.node->nd_lit)->ptr));
				    nd_set_type(yyval.node, NODE_LIT);
				    break;
				}
				/* fall through */
			      default:
				yyval.node = NEW_NODE(NODE_DSYM, rb_str_new(0, 0), 1, NEW_LIST(yyval.node));
				break;
			    }
			}
		    }
    break;

  case 417:
#line 2137 "parse.y"
    {
			yyval.node = negate_lit(yyvsp[0].node);
		    }
    break;

  case 418:
#line 2141 "parse.y"
    {
			yyval.node = negate_lit(yyvsp[0].node);
		    }
    break;

  case 424:
#line 2151 "parse.y"
    {yyval.id = kNIL;}
    break;

  case 425:
#line 2152 "parse.y"
    {yyval.id = kSELF;}
    break;

  case 426:
#line 2153 "parse.y"
    {yyval.id = kTRUE;}
    break;

  case 427:
#line 2154 "parse.y"
    {yyval.id = kFALSE;}
    break;

  case 428:
#line 2155 "parse.y"
    {yyval.id = k__FILE__;}
    break;

  case 429:
#line 2156 "parse.y"
    {yyval.id = k__LINE__;}
    break;

  case 430:
#line 2160 "parse.y"
    {
			yyval.node = gettable(yyvsp[0].id);
		    }
    break;

  case 431:
#line 2166 "parse.y"
    {
			yyval.node = assignable(yyvsp[0].id, 0);
		    }
    break;

  case 434:
#line 2176 "parse.y"
    {
			yyval.node = 0;
		    }
    break;

  case 435:
#line 2180 "parse.y"
    {
			lex_state = EXPR_BEG;
		    }
    break;

  case 436:
#line 2184 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 437:
#line 2187 "parse.y"
    {yyerrok; yyval.node = 0;}
    break;

  case 438:
#line 2191 "parse.y"
    {
			yyval.node = yyvsp[-2].node;
			lex_state = EXPR_BEG;
		    }
    break;

  case 439:
#line 2196 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 440:
#line 2202 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(yyvsp[-5].num, yyvsp[-3].node, yyvsp[-1].id), yyvsp[0].node);
		    }
    break;

  case 441:
#line 2206 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(yyvsp[-3].num, yyvsp[-1].node, -1), yyvsp[0].node);
		    }
    break;

  case 442:
#line 2210 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(yyvsp[-3].num, 0, yyvsp[-1].id), yyvsp[0].node);
		    }
    break;

  case 443:
#line 2214 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(yyvsp[-1].num, 0, -1), yyvsp[0].node);
		    }
    break;

  case 444:
#line 2218 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(0, yyvsp[-3].node, yyvsp[-1].id), yyvsp[0].node);
		    }
    break;

  case 445:
#line 2222 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(0, yyvsp[-1].node, -1), yyvsp[0].node);
		    }
    break;

  case 446:
#line 2226 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(0, 0, yyvsp[-1].id), yyvsp[0].node);
		    }
    break;

  case 447:
#line 2230 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(0, 0, -1), yyvsp[0].node);
		    }
    break;

  case 448:
#line 2234 "parse.y"
    {
			yyval.node = NEW_ARGS(0, 0, -1);
		    }
    break;

  case 449:
#line 2240 "parse.y"
    {
			yyerror("formal argument cannot be a constant");
		    }
    break;

  case 450:
#line 2244 "parse.y"
    {
                        yyerror("formal argument cannot be an instance variable");
		    }
    break;

  case 451:
#line 2248 "parse.y"
    {
                        yyerror("formal argument cannot be a global variable");
		    }
    break;

  case 452:
#line 2252 "parse.y"
    {
                        yyerror("formal argument cannot be a class variable");
		    }
    break;

  case 453:
#line 2256 "parse.y"
    {
			if (!is_local_id(yyvsp[0].id))
			    yyerror("formal argument must be local variable");
			else if (local_id(yyvsp[0].id))
			    yyerror("duplicate argument name");
			local_cnt(yyvsp[0].id);
			yyval.num = 1;
		    }
    break;

  case 455:
#line 2268 "parse.y"
    {
			yyval.num += 1;
		    }
    break;

  case 456:
#line 2274 "parse.y"
    {
			if (!is_local_id(yyvsp[-2].id))
			    yyerror("formal argument must be local variable");
			else if (local_id(yyvsp[-2].id))
			    yyerror("duplicate optional argument name");
			yyval.node = assignable(yyvsp[-2].id, yyvsp[0].node);
		    }
    break;

  case 457:
#line 2284 "parse.y"
    {
			yyval.node = NEW_BLOCK(yyvsp[0].node);
			yyval.node->nd_end = yyval.node;
		    }
    break;

  case 458:
#line 2289 "parse.y"
    {
			yyval.node = block_append(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 461:
#line 2299 "parse.y"
    {
			if (!is_local_id(yyvsp[0].id))
			    yyerror("rest argument must be local variable");
			else if (local_id(yyvsp[0].id))
			    yyerror("duplicate rest argument name");
			yyval.id = local_cnt(yyvsp[0].id);
		    }
    break;

  case 462:
#line 2307 "parse.y"
    {
			yyval.id = -2;
		    }
    break;

  case 465:
#line 2317 "parse.y"
    {
			if (!is_local_id(yyvsp[0].id))
			    yyerror("block argument must be local variable");
			else if (local_id(yyvsp[0].id))
			    yyerror("duplicate block argument name");
			yyval.node = NEW_BLOCK_ARG(yyvsp[0].id);
		    }
    break;

  case 466:
#line 2327 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 468:
#line 2334 "parse.y"
    {
			if (nd_type(yyvsp[0].node) == NODE_SELF) {
			    yyval.node = NEW_SELF();
			}
			else {
			    yyval.node = yyvsp[0].node;
		            value_expr(yyval.node);
			}
		    }
    break;

  case 469:
#line 2343 "parse.y"
    {lex_state = EXPR_BEG;}
    break;

  case 470:
#line 2344 "parse.y"
    {
			if (yyvsp[-2].node == 0) {
			    yyerror("can't define singleton method for ().");
			}
			else {
			    switch (nd_type(yyvsp[-2].node)) {
			      case NODE_STR:
			      case NODE_DSTR:
			      case NODE_XSTR:
			      case NODE_DXSTR:
			      case NODE_DREGX:
			      case NODE_LIT:
			      case NODE_ARRAY:
			      case NODE_ZARRAY:
				yyerror("can't define singleton method for literals");
			      default:
				value_expr(yyvsp[-2].node);
				break;
			    }
			}
			yyval.node = yyvsp[-2].node;
		    }
    break;

  case 472:
#line 2370 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 473:
#line 2374 "parse.y"
    {
			if (yyvsp[-1].node->nd_alen%2 != 0) {
			    yyerror("odd number list for Hash");
			}
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 475:
#line 2384 "parse.y"
    {
			yyval.node = list_concat(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 476:
#line 2390 "parse.y"
    {
			yyval.node = list_append(NEW_LIST(yyvsp[-2].node), yyvsp[0].node);
		    }
    break;

  case 496:
#line 2428 "parse.y"
    {yyerrok;}
    break;

  case 499:
#line 2433 "parse.y"
    {yyerrok;}
    break;

  case 500:
#line 2436 "parse.y"
    {yyval.node = 0;}
    break;


    }

/* Line 1010 of yacc.c.  */
#line 6872 "parse.c"

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
	  const char* yyprefix;
	  char *yymsg;
	  int yyx;

	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  int yyxbegin = yyn < 0 ? -yyn : 0;

	  /* Stay within bounds of both yycheck and yytname.  */
	  int yychecklim = YYLAST - yyn;
	  int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
	  int yycount = 0;

	  yyprefix = ", expecting ";
	  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      {
		yysize += yystrlen (yyprefix) + yystrlen (yytname [yyx]);
		yycount += 1;
		if (yycount == 5)
		  {
		    yysize = 0;
		    break;
		  }
	      }
	  yysize += (sizeof ("syntax error, unexpected ")
		     + yystrlen (yytname[yytype]));
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yyprefix = ", expecting ";
		  for (yyx = yyxbegin; yyx < yyxend; ++yyx)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			yyp = yystpcpy (yyp, yyprefix);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yyprefix = " or ";
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

      if (yychar <= YYEOF)
        {
          /* If at end of input, pop the error token,
	     then the rest of the stack, then return failure.  */
	  if (yychar == YYEOF)
	     for (;;)
	       {
		 YYPOPSTACK;
		 if (yyssp == yyss)
		   YYABORT;
		 YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
		 yydestruct (yystos[*yyssp], yyvsp);
	       }
        }
      else
	{
	  YYDSYMPRINTF ("Error: discarding", yytoken, &yylval, &yylloc);
	  yydestruct (yytoken, &yylval);
	  yychar = YYEMPTY;

	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

#ifdef __GNUC__
  /* Pacify GCC when the user code never invokes YYERROR and the label
     yyerrorlab therefore never appears in user code.  */
  if (0)
     goto yyerrorlab;
#endif

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

      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
      yydestruct (yystos[yystate], yyvsp);
      YYPOPSTACK;
      yystate = *yyssp;
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


#line 2438 "parse.y"

#include "regex.h"
#include "util.h"

/* We remove any previous definition of `SIGN_EXTEND_CHAR',
   since ours (we hope) works properly with all combinations of
   machines, compilers, `char' and `unsigned char' argument types.
   (Per Bothner suggested the basic approach.)  */
#undef SIGN_EXTEND_CHAR
#if __STDC__
# define SIGN_EXTEND_CHAR(c) ((signed char)(c))
#else  /* not __STDC__ */
/* As in Harbison and Steele.  */
# define SIGN_EXTEND_CHAR(c) ((((unsigned char)(c)) ^ 128) - 128)
#endif
#define is_identchar(c) (SIGN_EXTEND_CHAR(c)!=-1&&(ISALNUM(c) || (c) == '_' || ismbchar(c)))

static char *tokenbuf = NULL;
static int   tokidx, toksiz = 0;

#define LEAVE_BS 1

static VALUE (*lex_gets)();	/* gets function */
static VALUE lex_input;		/* non-nil if File */
static VALUE lex_lastline;	/* gc protect */
static char *lex_pbeg;
static char *lex_p;
static char *lex_pend;

static int
yyerror(msg)
    const char *msg;
{
    char *p, *pe, *buf;
    int len, i;

    rb_compile_error("%s", msg);
    p = lex_p;
    while (lex_pbeg <= p) {
	if (*p == '\n') break;
	p--;
    }
    p++;

    pe = lex_p;
    while (pe < lex_pend) {
	if (*pe == '\n') break;
	pe++;
    }

    len = pe - p;
    if (len > 4) {
	buf = ALLOCA_N(char, len+2);
	MEMCPY(buf, p, char, len);
	buf[len] = '\0';
	rb_compile_error_append("%s", buf);

	i = lex_p - p;
	p = buf; pe = p + len;

	while (p < pe) {
	    if (*p != '\t') *p = ' ';
	    p++;
	}
	buf[i] = '^';
	buf[i+1] = '\0';
	rb_compile_error_append("%s", buf);
    }

    return 0;
}

static int heredoc_end;
static int command_start = Qtrue;

int ruby_in_compile = 0;
int ruby__end__seen;

static VALUE ruby_debug_lines;

static NODE*
yycompile(f, line)
    char *f;
    int line;
{
    int n;
    NODE *node = 0;
    struct RVarmap *vp, *vars = ruby_dyna_vars;

    ruby_in_compile = 1;
    if (!compile_for_eval && rb_safe_level() == 0 &&
	rb_const_defined(rb_cObject, rb_intern("SCRIPT_LINES__"))) {
	VALUE hash, fname;

	hash = rb_const_get(rb_cObject, rb_intern("SCRIPT_LINES__"));
	if (TYPE(hash) == T_HASH) {
	    fname = rb_str_new2(f);
	    ruby_debug_lines = rb_hash_aref(hash, fname);
	    if (NIL_P(ruby_debug_lines)) {
		ruby_debug_lines = rb_ary_new();
		rb_hash_aset(hash, fname, ruby_debug_lines);
	    }
	}
	if (line > 1) {
	    VALUE str = rb_str_new(0,0);
	    while (line > 1) {
		rb_ary_push(ruby_debug_lines, str);
		line--;
	    }
	}
    }

    ruby__end__seen = 0;
    ruby_eval_tree = 0;
    heredoc_end = 0;
    lex_strterm = 0;
    ruby_current_node = 0;
    ruby_sourcefile = rb_source_filename(f);
    n = yyparse();
    ruby_debug_lines = 0;
    compile_for_eval = 0;
    ruby_in_compile = 0;
    cond_stack = 0;
    cmdarg_stack = 0;
    command_start = 1;		  
    class_nest = 0;
    in_single = 0;
    in_def = 0;
    cur_mid = 0;

    vp = ruby_dyna_vars;
    ruby_dyna_vars = vars;
    lex_strterm = 0;
    while (vp && vp != vars) {
	struct RVarmap *tmp = vp;
	vp = vp->next;
	rb_gc_force_recycle((VALUE)tmp);
    }
    if (n == 0) node = ruby_eval_tree;
    else ruby_eval_tree_begin = 0;
    return node;
}

static int lex_gets_ptr;

static VALUE
lex_get_str(s)
    VALUE s;
{
    char *beg, *end, *pend;

    beg = RSTRING(s)->ptr;
    if (lex_gets_ptr) {
	if (RSTRING(s)->len == lex_gets_ptr) return Qnil;
	beg += lex_gets_ptr;
    }
    pend = RSTRING(s)->ptr + RSTRING(s)->len;
    end = beg;
    while (end < pend) {
	if (*end++ == '\n') break;
    }
    lex_gets_ptr = end - RSTRING(s)->ptr;
    return rb_str_new(beg, end - beg);
}

static VALUE
lex_getline()
{
    VALUE line = (*lex_gets)(lex_input);
    if (ruby_debug_lines && !NIL_P(line)) {
	rb_ary_push(ruby_debug_lines, line);
    }
    return line;
}

NODE*
rb_compile_string(f, s, line)
    const char *f;
    VALUE s;
    int line;
{
    lex_gets = lex_get_str;
    lex_gets_ptr = 0;
    lex_input = s;
    lex_pbeg = lex_p = lex_pend = 0;
    ruby_sourceline = line - 1;
    compile_for_eval = ruby_in_eval;

    return yycompile(f, line);
}

NODE*
rb_compile_cstr(f, s, len, line)
    const char *f, *s;
    int len, line;
{
    return rb_compile_string(f, rb_str_new(s, len), line);
}

NODE*
rb_compile_file(f, file, start)
    const char *f;
    VALUE file;
    int start;
{
    lex_gets = rb_io_gets;
    lex_input = file;
    lex_pbeg = lex_p = lex_pend = 0;
    ruby_sourceline = start - 1;

    return yycompile(f, start);
}

static inline int
nextc()
{
    int c;

    if (lex_p == lex_pend) {
	if (lex_input) {
	    VALUE v = lex_getline();

	    if (NIL_P(v)) return -1;
	    if (heredoc_end > 0) {
		ruby_sourceline = heredoc_end;
		heredoc_end = 0;
	    }
	    ruby_sourceline++;
	    lex_pbeg = lex_p = RSTRING(v)->ptr;
	    lex_pend = lex_p + RSTRING(v)->len;
	    lex_lastline = v;
	}
	else {
	    lex_lastline = 0;
	    return -1;
	}
    }
    c = (unsigned char)*lex_p++;
    if (c == '\r' && lex_p < lex_pend && *lex_p == '\n') {
	lex_p++;
	c = '\n';
    }

    return c;
}

static void
pushback(c)
    int c;
{
    if (c == -1) return;
    lex_p--;
}

#define was_bol() (lex_p == lex_pbeg + 1)
#define peek(c) (lex_p != lex_pend && (c) == *lex_p)

#define tokfix() (tokenbuf[tokidx]='\0')
#define tok() tokenbuf
#define toklen() tokidx
#define toklast() (tokidx>0?tokenbuf[tokidx-1]:0)

static char*
newtok()
{
    tokidx = 0;
    if (!tokenbuf) {
	toksiz = 60;
	tokenbuf = ALLOC_N(char, 60);
    }
    if (toksiz > 4096) {
	toksiz = 60;
	REALLOC_N(tokenbuf, char, 60);
    }
    return tokenbuf;
}

static void
tokadd(c)
    char c;
{
    tokenbuf[tokidx++] = c;
    if (tokidx >= toksiz) {
	toksiz *= 2;
	REALLOC_N(tokenbuf, char, toksiz);
    }
}

static int
read_escape()
{
    int c;

    switch (c = nextc()) {
      case '\\':	/* Backslash */
	return c;

      case 'n':	/* newline */
	return '\n';

      case 't':	/* horizontal tab */
	return '\t';

      case 'r':	/* carriage-return */
	return '\r';

      case 'f':	/* form-feed */
	return '\f';

      case 'v':	/* vertical tab */
	return '\13';

      case 'a':	/* alarm(bell) */
	return '\007';

      case 'e':	/* escape */
	return 033;

      case '0': case '1': case '2': case '3': /* octal constant */
      case '4': case '5': case '6': case '7':
	{
	    int numlen;

	    pushback(c);
	    c = scan_oct(lex_p, 3, &numlen);
	    lex_p += numlen;
	}
	return c;

      case 'x':	/* hex constant */
	{
	    int numlen;

	    c = scan_hex(lex_p, 2, &numlen);
	    if (numlen == 0) {
		yyerror("Invalid escape character syntax");
		return 0;
	    }
	    lex_p += numlen;
	}
	return c;

      case 'b':	/* backspace */
	return '\010';

      case 's':	/* space */
	return ' ';

      case 'M':
	if ((c = nextc()) != '-') {
	    yyerror("Invalid escape character syntax");
	    pushback(c);
	    return '\0';
	}
	if ((c = nextc()) == '\\') {
	    return read_escape() | 0x80;
	}
	else if (c == -1) goto eof;
	else {
	    return ((c & 0xff) | 0x80);
	}

      case 'C':
	if ((c = nextc()) != '-') {
	    yyerror("Invalid escape character syntax");
	    pushback(c);
	    return '\0';
	}
      case 'c':
	if ((c = nextc())== '\\') {
	    c = read_escape();
	}
	else if (c == '?')
	    return 0177;
	else if (c == -1) goto eof;
	return c & 0x9f;

      eof:
      case -1:
        yyerror("Invalid escape character syntax");
	return '\0';

      default:
	return c;
    }
}

static int
tokadd_escape(term)
    int term;
{
    int c;

    switch (c = nextc()) {
      case '\n':
	return 0;		/* just ignore */

      case '0': case '1': case '2': case '3': /* octal constant */
      case '4': case '5': case '6': case '7':
	{
	    int i;

	    tokadd('\\');
	    tokadd(c);
	    for (i=0; i<2; i++) {
		c = nextc();
		if (c == -1) goto eof;
		if (c < '0' || '7' < c) {
		    pushback(c);
		    break;
		}
		tokadd(c);
	    }
	}
	return 0;

      case 'x':	/* hex constant */
	{
	    int numlen;

	    tokadd('\\');
	    tokadd(c);
	    scan_hex(lex_p, 2, &numlen);
	    if (numlen == 0) {
		yyerror("Invalid escape character syntax");
		return -1;
	    }
	    while (numlen--)
		tokadd(nextc());
	}
	return 0;

      case 'M':
	if ((c = nextc()) != '-') {
	    yyerror("Invalid escape character syntax");
	    pushback(c);
	    return 0;
	}
	tokadd('\\'); tokadd('M'); tokadd('-');
	goto escaped;

      case 'C':
	if ((c = nextc()) != '-') {
	    yyerror("Invalid escape character syntax");
	    pushback(c);
	    return 0;
	}
	tokadd('\\'); tokadd('C'); tokadd('-');
	goto escaped;

      case 'c':
	tokadd('\\'); tokadd('c');
      escaped:
	if ((c = nextc()) == '\\') {
	    return tokadd_escape(term);
	}
	else if (c == -1) goto eof;
	tokadd(c);
	return 0;

      eof:
      case -1:
        yyerror("Invalid escape character syntax");
	return -1;

      default:
	if (c != '\\' || c != term)
	    tokadd('\\');
	tokadd(c);
    }
    return 0;
}

static int
regx_options()
{
    char kcode = 0;
    int options = 0;
    int c;

    newtok();
    while (c = nextc(), ISALPHA(c)) {
	switch (c) {
	  case 'i':
	    options |= RE_OPTION_IGNORECASE;
	    break;
	  case 'x':
	    options |= RE_OPTION_EXTENDED;
	    break;
	  case 'm':
	    options |= RE_OPTION_MULTILINE;
	    break;
	  case 'o':
	    options |= RE_OPTION_ONCE;
	    break;
	  case 'n':
	    kcode = 16;
	    break;
	  case 'e':
	    kcode = 32;
	    break;
	  case 's':
	    kcode = 48;
	    break;
	  case 'u':
	    kcode = 64;
	    break;
	  default:
	    tokadd(c);
	    break;
	}
    }
    pushback(c);
    if (toklen()) {
	tokfix();
	rb_compile_error("unknown regexp option%s - %s",
			 toklen() > 1 ? "s" : "", tok());
    }
    return options | kcode;
}

#define STR_FUNC_ESCAPE 0x01
#define STR_FUNC_EXPAND 0x02
#define STR_FUNC_REGEXP 0x04
#define STR_FUNC_QWORDS 0x08
#define STR_FUNC_SYMBOL 0x10
#define STR_FUNC_INDENT 0x20

enum string_type {
    str_squote = (0),
    str_dquote = (STR_FUNC_EXPAND),
    str_xquote = (STR_FUNC_EXPAND),
    str_regexp = (STR_FUNC_REGEXP|STR_FUNC_ESCAPE|STR_FUNC_EXPAND),
    str_sword  = (STR_FUNC_QWORDS),
    str_dword  = (STR_FUNC_QWORDS|STR_FUNC_EXPAND),
    str_ssym   = (STR_FUNC_SYMBOL),
    str_dsym   = (STR_FUNC_SYMBOL|STR_FUNC_EXPAND),
};

static void
dispose_string(str)
    VALUE str;
{
    free(RSTRING(str)->ptr);
    rb_gc_force_recycle(str);
}

static int
tokadd_string(func, term, paren, nest)
    int func, term, paren, *nest;
{
    int c;

    while ((c = nextc()) != -1) {
	if (paren && c == paren) {
	    ++*nest;
	}
	else if (c == term) {
	    if (!nest || !*nest) {
		pushback(c);
		break;
	    }
	    --*nest;
	}
	else if ((func & STR_FUNC_EXPAND) && c == '#' && lex_p < lex_pend) {
	    int c2 = *lex_p;
	    if (c2 == '$' || c2 == '@' || c2 == '{') {
		pushback(c);
		break;
	    }
	}
	else if (c == '\\') {
	    c = nextc();
	    switch (c) {
	      case '\n':
		if (func & STR_FUNC_QWORDS) break;
		if (func & STR_FUNC_EXPAND) continue;
		tokadd('\\');
		break;

	      case '\\':
		if (func & STR_FUNC_ESCAPE) tokadd(c);
		break;

	      default:
		if (func & STR_FUNC_REGEXP) {
		    pushback(c);
		    if (tokadd_escape(term) < 0)
			return -1;
		    continue;
		}
		else if (func & STR_FUNC_EXPAND) {
		    pushback(c);
		    if (func & STR_FUNC_ESCAPE) tokadd('\\');
		    c = read_escape();
		}
		else if ((func & STR_FUNC_QWORDS) && ISSPACE(c)) {
		    /* ignore backslashed spaces in %w */
		}
		else if (c != term && !(paren && c == paren)) {
		    tokadd('\\');
		}
	    }
	}
	else if (ismbchar(c)) {
	    int i, len = mbclen(c)-1;

	    for (i = 0; i < len; i++) {
		tokadd(c);
		c = nextc();
	    }
	}
	else if ((func & STR_FUNC_QWORDS) && ISSPACE(c)) {
	    pushback(c);
	    break;
	}
	if (!c && (func & STR_FUNC_SYMBOL)) {
	    func &= ~STR_FUNC_SYMBOL;
	    rb_compile_error("symbol cannot contain '\\0'");
	    continue;
	}
	tokadd(c);
    }
    return c;
}

#define NEW_STRTERM(func, term, paren) \
	rb_node_newnode(NODE_STRTERM, (func), (term) | ((paren) << (CHAR_BIT * 2)), 0)

static int
parse_string(quote)
    NODE *quote;
{
    int func = quote->nd_func;
    int term = nd_term(quote);
    int paren = nd_paren(quote);
    int c, space = 0;

    if (func == -1) return tSTRING_END;
    c = nextc();
    if ((func & STR_FUNC_QWORDS) && ISSPACE(c)) {
	do {c = nextc();} while (ISSPACE(c));
	space = 1;
    }
    if (c == term && !quote->nd_nest) {
	if (func & STR_FUNC_QWORDS) {
	    quote->nd_func = -1;
	    return ' ';
	}
	if (!(func & STR_FUNC_REGEXP)) return tSTRING_END;
	yylval.num = regx_options();
	return tREGEXP_END;
    }
    if (space) {
	pushback(c);
	return ' ';
    }
    newtok();
    if ((func & STR_FUNC_EXPAND) && c == '#') {
	switch (c = nextc()) {
	  case '$':
	  case '@':
	    pushback(c);
	    return tSTRING_DVAR;
	  case '{':
	    return tSTRING_DBEG;
	}
	tokadd('#');
    }
    pushback(c);
    if (tokadd_string(func, term, paren, &quote->nd_nest) == -1) {
	ruby_sourceline = nd_line(quote);
	rb_compile_error("unterminated string meets end of file");
	return tSTRING_END;
    }

    tokfix();
    yylval.node = NEW_STR(rb_str_new(tok(), toklen()));
    return tSTRING_CONTENT;
}

static int
heredoc_identifier()
{
    int c = nextc(), term, func = 0, len;

    if (c == '-') {
	c = nextc();
	func = STR_FUNC_INDENT;
    }
    switch (c) {
      case '\'':
	func |= str_squote; goto quoted;
      case '"':
	func |= str_dquote; goto quoted;
      case '`':
	func |= str_xquote;
      quoted:
	newtok();
	tokadd(func);
	term = c;
	while ((c = nextc()) != -1 && c != term) {
	    len = mbclen(c);
	    do {tokadd(c);} while (--len > 0 && (c = nextc()) != -1);
	}
	if (c == -1) {
	    rb_compile_error("unterminated here document identifier");
	    return 0;
	}
	break;

      default:
	if (!is_identchar(c)) {
	    pushback(c);
	    if (func & STR_FUNC_INDENT) {
		pushback('-');
	    }
	    return 0;
	}
	newtok();
	term = '"';
	tokadd(func |= str_dquote);
	do {
	    len = mbclen(c);
	    do {tokadd(c);} while (--len > 0 && (c = nextc()) != -1);
	} while ((c = nextc()) != -1 && is_identchar(c));
	pushback(c);
	break;
    }

    tokfix();
    len = lex_p - lex_pbeg;
    lex_p = lex_pend;
    lex_strterm = rb_node_newnode(NODE_HEREDOC,
				  rb_str_new(tok(), toklen()),	/* nd_lit */
				  len,				/* nd_nth */
				  lex_lastline);		/* nd_orig */
    return term == '`' ? tXSTRING_BEG : tSTRING_BEG;
}

static void
heredoc_restore(here)
    NODE *here;
{
    VALUE line = here->nd_orig;
    lex_lastline = line;
    lex_pbeg = RSTRING(line)->ptr;
    lex_pend = lex_pbeg + RSTRING(line)->len;
    lex_p = lex_pbeg + here->nd_nth;
    heredoc_end = ruby_sourceline;
    ruby_sourceline = nd_line(here);
    dispose_string(here->nd_lit);
    rb_gc_force_recycle((VALUE)here);
}

static int
whole_match_p(eos, len, indent)
    char *eos;
    int len, indent;
{
    char *p = lex_pbeg;
    int n;

    if (indent) {
	while (*p && ISSPACE(*p)) p++;
    }
    n= lex_pend - (p + len);
    if (n < 0 || (n > 0 && p[len] != '\n' && p[len] != '\r')) return Qfalse;
    if (strncmp(eos, p, len) == 0) return Qtrue;
    return Qfalse;
}

static int
here_document(here)
    NODE *here;
{
    int c, func, indent = 0;
    char *eos, *p, *pend;
    long len;
    VALUE str = 0;

    eos = RSTRING(here->nd_lit)->ptr;
    len = RSTRING(here->nd_lit)->len - 1;
    indent = (func = *eos++) & STR_FUNC_INDENT;

    if ((c = nextc()) == -1) {
      error:
	rb_compile_error("can't find string \"%s\" anywhere before EOF", eos);
	heredoc_restore(lex_strterm);
	lex_strterm = 0;
	return 0;
    }
    if (was_bol() && whole_match_p(eos, len, indent)) {
	heredoc_restore(lex_strterm);
	return tSTRING_END;
    }

    if (!(func & STR_FUNC_EXPAND)) {
	do {
	    p = RSTRING(lex_lastline)->ptr;
	    pend = lex_pend;
	    if (pend > p) {
		switch (pend[-1]) {
		  case '\n':
		    if (--pend == p || pend[-1] != '\r') {
			pend++;
			break;
		    }
		  case '\r':
		    --pend;
		}
	    }
	    if (str)
		rb_str_cat(str, p, pend - p);
	    else
		str = rb_str_new(p, pend - p);
	    if (pend < lex_pend) rb_str_cat(str, "\n", 1);
	    lex_p = lex_pend;
	    if (nextc() == -1) {
		if (str) dispose_string(str);
		goto error;
	    }
	} while (!whole_match_p(eos, len, indent));
    }
    else {
	newtok();
	if (c == '#') {
	    switch (c = nextc()) {
	      case '$':
	      case '@':
		pushback(c);
		return tSTRING_DVAR;
	      case '{':
		return tSTRING_DBEG;
	    }
	    tokadd('#');
	}
	do {
	    pushback(c);
	    if ((c = tokadd_string(func, '\n', 0, NULL)) == -1) goto error;
	    if (c != '\n') {
		yylval.node = NEW_STR(rb_str_new(tok(), toklen()));
		return tSTRING_CONTENT;
	    }
	    tokadd(nextc());
	    if ((c = nextc()) == -1) goto error;
	} while (!whole_match_p(eos, len, indent));
	str = rb_str_new(tok(), toklen());
    }
    heredoc_restore(lex_strterm);
    lex_strterm = NEW_STRTERM(-1, 0, 0);
    yylval.node = NEW_STR(str);
    return tSTRING_CONTENT;
}

#include "lex.c"

static void
arg_ambiguous()
{
    rb_warning("ambiguous first argument; put parentheses or even spaces");
}

#define IS_ARG() (lex_state == EXPR_ARG || lex_state == EXPR_CMDARG)

static int
yylex()
{
    register int c;
    int space_seen = 0;
    int cmd_state;

    if (lex_strterm) {
	int token;
	if (nd_type(lex_strterm) == NODE_HEREDOC) {
	    token = here_document(lex_strterm);
	    if (token == tSTRING_END) {
		lex_strterm = 0;
		lex_state = EXPR_END;
	    }
	}
	else {
	    token = parse_string(lex_strterm);
	    if (token == tSTRING_END || token == tREGEXP_END) {
		rb_gc_force_recycle((VALUE)lex_strterm);
		lex_strterm = 0;
		lex_state = EXPR_END;
	    }
	}
	return token;
    }
    cmd_state = command_start;
    command_start = Qfalse;
  retry:
    switch (c = nextc()) {
      case '\0':		/* NUL */
      case '\004':		/* ^D */
      case '\032':		/* ^Z */
      case -1:			/* end of script. */
	return 0;

	/* white spaces */
      case ' ': case '\t': case '\f': case '\r':
      case '\13': /* '\v' */
	space_seen++;
	goto retry;

      case '#':		/* it's a comment */
	while ((c = nextc()) != '\n') {
	    if (c == -1)
		return 0;
	}
	/* fall through */
      case '\n':
	switch (lex_state) {
	  case EXPR_BEG:
	  case EXPR_FNAME:
	  case EXPR_DOT:
	  case EXPR_CLASS:
	    goto retry;
	  default:
	    break;
	}
	command_start = Qtrue;
	lex_state = EXPR_BEG;
	return '\n';

      case '*':
	if ((c = nextc()) == '*') {
	    if ((c = nextc()) == '=') {
		yylval.id = tPOW;
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    c = tPOW;
	}
	else {
	    if (c == '=') {
		yylval.id = '*';
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    if (IS_ARG() && space_seen && !ISSPACE(c)){
		rb_warning("`*' interpreted as argument prefix");
		c = tSTAR;
	    }
	    else if (lex_state == EXPR_BEG || lex_state == EXPR_MID) {
		c = tSTAR;
	    }
	    else {
		c = '*';
	    }
	}
	switch (lex_state) {
	  case EXPR_FNAME: case EXPR_DOT:
	    lex_state = EXPR_ARG; break;
	  default:
	    lex_state = EXPR_BEG; break;
	}
	return c;

      case '!':
	lex_state = EXPR_BEG;
	if ((c = nextc()) == '=') {
	    return tNEQ;
	}
	if (c == '~') {
	    return tNMATCH;
	}
	pushback(c);
	return '!';

      case '=':
	if (was_bol()) {
	    /* skip embedded rd document */
	    if (strncmp(lex_p, "begin", 5) == 0 && ISSPACE(lex_p[5])) {
		for (;;) {
		    lex_p = lex_pend;
		    c = nextc();
		    if (c == -1) {
			rb_compile_error("embedded document meets end of file");
			return 0;
		    }
		    if (c != '=') continue;
		    if (strncmp(lex_p, "end", 3) == 0 &&
			(lex_p + 3 == lex_pend || ISSPACE(lex_p[3]))) {
			break;
		    }
		}
		lex_p = lex_pend;
		goto retry;
	    }
	}

	switch (lex_state) {
	  case EXPR_FNAME: case EXPR_DOT:
	    lex_state = EXPR_ARG; break;
	  default:
	    lex_state = EXPR_BEG; break;
	}
	if ((c = nextc()) == '=') {
	    if ((c = nextc()) == '=') {
		return tEQQ;
	    }
	    pushback(c);
	    return tEQ;
	}
	if (c == '~') {
	    return tMATCH;
	}
	else if (c == '>') {
	    return tASSOC;
	}
	pushback(c);
	return '=';

      case '<':
	c = nextc();
	if (c == '<' &&
	    lex_state != EXPR_END &&
	    lex_state != EXPR_DOT &&
	    lex_state != EXPR_ENDARG && 
	    lex_state != EXPR_CLASS &&
	    (!IS_ARG() || space_seen)) {
	    int token = heredoc_identifier();
	    if (token) return token;
	}
	switch (lex_state) {
	  case EXPR_FNAME: case EXPR_DOT:
	    lex_state = EXPR_ARG; break;
	  default:
	    lex_state = EXPR_BEG; break;
	}
	if (c == '=') {
	    if ((c = nextc()) == '>') {
		return tCMP;
	    }
	    pushback(c);
	    return tLEQ;
	}
	if (c == '<') {
	    if ((c = nextc()) == '=') {
		yylval.id = tLSHFT;
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    return tLSHFT;
	}
	pushback(c);
	return '<';

      case '>':
	switch (lex_state) {
	  case EXPR_FNAME: case EXPR_DOT:
	    lex_state = EXPR_ARG; break;
	  default:
	    lex_state = EXPR_BEG; break;
	}
	if ((c = nextc()) == '=') {
	    return tGEQ;
	}
	if (c == '>') {
	    if ((c = nextc()) == '=') {
		yylval.id = tRSHFT;
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    return tRSHFT;
	}
	pushback(c);
	return '>';

      case '"':
	lex_strterm = NEW_STRTERM(str_dquote, '"', 0);
	return tSTRING_BEG;

      case '`':
	if (lex_state == EXPR_FNAME) {
	    lex_state = EXPR_END;
	    return c;
	}
	if (lex_state == EXPR_DOT) {
	    if (cmd_state)
		lex_state = EXPR_CMDARG;
	    else
		lex_state = EXPR_ARG;
	    return c;
	}
	lex_strterm = NEW_STRTERM(str_xquote, '`', 0);
	return tXSTRING_BEG;

      case '\'':
	lex_strterm = NEW_STRTERM(str_squote, '\'', 0);
	return tSTRING_BEG;

      case '?':
	if (lex_state == EXPR_END || lex_state == EXPR_ENDARG) {
	    lex_state = EXPR_BEG;
	    return '?';
	}
	c = nextc();
	if (c == -1) {
	    rb_compile_error("incomplete character syntax");
	    return 0;
	}
	if (ISSPACE(c)){
	    if (!IS_ARG()){
		int c2 = 0;
		switch (c) {
		  case ' ':
		    c2 = 's';
		    break;
		  case '\n':
		    c2 = 'n';
		    break;
		  case '\t':
		    c2 = 't';
		    break;
		  case '\v':
		    c2 = 'v';
		    break;
		  case '\r':
		    c2 = 'r';
		    break;
		  case '\f':
		    c2 = 'f';
		    break;
		}
		if (c2) {
		    rb_warn("invalid character syntax; use ?\\%c", c2);
		}
	    }
	  ternary:
	    pushback(c);
	    lex_state = EXPR_BEG;
	    return '?';
	}
	else if (ismbchar(c)) {
	    rb_warn("multibyte character literal not supported yet; use ?\\%.3o", c);
	    goto ternary;
	}
	else if ((ISALNUM(c) || c == '_') && lex_p < lex_pend && is_identchar(*lex_p)) {
	    goto ternary;
	}
	else if (c == '\\') {
	    c = read_escape();
	}
	c &= 0xff;
	lex_state = EXPR_END;
	yylval.node = NEW_LIT(INT2FIX(c));
	return tINTEGER;

      case '&':
	if ((c = nextc()) == '&') {
	    lex_state = EXPR_BEG;
	    if ((c = nextc()) == '=') {
		yylval.id = tANDOP;
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    return tANDOP;
	}
	else if (c == '=') {
	    yylval.id = '&';
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	pushback(c);
	if (IS_ARG() && space_seen && !ISSPACE(c)){
	    rb_warning("`&' interpreted as argument prefix");
	    c = tAMPER;
	}
	else if (lex_state == EXPR_BEG || lex_state == EXPR_MID) {
	    c = tAMPER;
	}
	else {
	    c = '&';
	}
	switch (lex_state) {
	  case EXPR_FNAME: case EXPR_DOT:
	    lex_state = EXPR_ARG; break;
	  default:
	    lex_state = EXPR_BEG;
	}
	return c;

      case '|':
	if ((c = nextc()) == '|') {
	    lex_state = EXPR_BEG;
	    if ((c = nextc()) == '=') {
		yylval.id = tOROP;
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    return tOROP;
	}
	if (c == '=') {
	    yylval.id = '|';
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	if (lex_state == EXPR_FNAME || lex_state == EXPR_DOT) {
	    lex_state = EXPR_ARG;
	}
	else {
	    lex_state = EXPR_BEG;
	}
	pushback(c);
	return '|';

      case '+':
	c = nextc();
	if (lex_state == EXPR_FNAME || lex_state == EXPR_DOT) {
	    lex_state = EXPR_ARG;
	    if (c == '@') {
		return tUPLUS;
	    }
	    pushback(c);
	    return '+';
	}
	if (c == '=') {
	    yylval.id = '+';
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	if (lex_state == EXPR_BEG || lex_state == EXPR_MID ||
	    (IS_ARG() && space_seen && !ISSPACE(c))) {
	    if (IS_ARG()) arg_ambiguous();
	    lex_state = EXPR_BEG;
	    pushback(c);
	    if (ISDIGIT(c)) {
		c = '+';
		goto start_num;
	    }
	    return tUPLUS;
	}
	lex_state = EXPR_BEG;
	pushback(c);
	return '+';

      case '-':
	c = nextc();
	if (lex_state == EXPR_FNAME || lex_state == EXPR_DOT) {
	    lex_state = EXPR_ARG;
	    if (c == '@') {
		return tUMINUS;
	    }
	    pushback(c);
	    return '-';
	}
	if (c == '=') {
	    yylval.id = '-';
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	if (lex_state == EXPR_BEG || lex_state == EXPR_MID ||
	    (IS_ARG() && space_seen && !ISSPACE(c))) {
	    if (IS_ARG()) arg_ambiguous();
	    lex_state = EXPR_BEG;
	    pushback(c);
	    if (ISDIGIT(c)) {
		return tUMINUS_NUM;
	    }
	    return tUMINUS;
	}
	lex_state = EXPR_BEG;
	pushback(c);
	return '-';

      case '.':
	lex_state = EXPR_BEG;
	if ((c = nextc()) == '.') {
	    if ((c = nextc()) == '.') {
		return tDOT3;
	    }
	    pushback(c);
	    return tDOT2;
	}
	pushback(c);
	if (ISDIGIT(c)) {
	    yyerror("no .<digit> floating literal anymore; put 0 before dot");
	}
	lex_state = EXPR_DOT;
	return '.';

      start_num:
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
	{
	    int is_float, seen_point, seen_e, nondigit;

	    is_float = seen_point = seen_e = nondigit = 0;
	    lex_state = EXPR_END;
	    newtok();
	    if (c == '-' || c == '+') {
		tokadd(c);
		c = nextc();
	    }
	    if (c == '0') {
		int start = toklen();
		c = nextc();
		if (c == 'x' || c == 'X') {
		    /* hexadecimal */
		    c = nextc();
		    if (ISXDIGIT(c)) {
			do {
			    if (c == '_') {
				if (nondigit) break;
				nondigit = c;
				continue;
			    }
			    if (!ISXDIGIT(c)) break;
			    nondigit = 0;
			    tokadd(c);
			} while ((c = nextc()) != -1);
		    }
		    pushback(c);
		    tokfix();
		    if (toklen() == start) {
			yyerror("numeric literal without digits");
		    }
		    else if (nondigit) goto trailing_uc;
		    yylval.node = NEW_LIT(rb_cstr_to_inum(tok(), 16, Qfalse));
		    return tINTEGER;
		}
		if (c == 'b' || c == 'B') {
		    /* binary */
		    c = nextc();
		    if (c == '0' || c == '1') {
			do {
			    if (c == '_') {
				if (nondigit) break;
				nondigit = c;
				continue;
			    }
			    if (c != '0' && c != '1') break;
			    nondigit = 0;
			    tokadd(c);
			} while ((c = nextc()) != -1);
		    }
		    pushback(c);
		    tokfix();
		    if (toklen() == start) {
			yyerror("numeric literal without digits");
		    }
		    else if (nondigit) goto trailing_uc;
		    yylval.node = NEW_LIT(rb_cstr_to_inum(tok(), 2, Qfalse));
		    return tINTEGER;
		}
		if (c == 'd' || c == 'D') {
		    /* decimal */
		    c = nextc();
		    if (ISDIGIT(c)) {
			do {
			    if (c == '_') {
				if (nondigit) break;
				nondigit = c;
				continue;
			    }
			    if (!ISDIGIT(c)) break;
			    nondigit = 0;
			    tokadd(c);
			} while ((c = nextc()) != -1);
		    }
		    pushback(c);
		    tokfix();
		    if (toklen() == start) {
			yyerror("numeric literal without digits");
		    }
		    else if (nondigit) goto trailing_uc;
		    yylval.node = NEW_LIT(rb_cstr_to_inum(tok(), 10, Qfalse));
		    return tINTEGER;
		}
		if (c == '_') {
		    /* 0_0 */
		    goto octal_number;
		}
		if (c == 'o' || c == 'O') {
		    /* prefixed octal */
		    c = nextc();
		    if (c == '_') {
			yyerror("numeric literal without digits");
		    }
		}
		if (c >= '0' && c <= '7') {
		    /* octal */
		  octal_number:
	            do {
			if (c == '_') {
			    if (nondigit) break;
			    nondigit = c;
			    continue;
			}
			if (c < '0' || c > '7') break;
			nondigit = 0;
			tokadd(c);
		    } while ((c = nextc()) != -1);
		    if (toklen() > start) {
			pushback(c);
			tokfix();
			if (nondigit) goto trailing_uc;
			yylval.node = NEW_LIT(rb_cstr_to_inum(tok(), 8, Qfalse));
			return tINTEGER;
		    }
		    if (nondigit) {
			pushback(c);
			goto trailing_uc;
		    }
		}
		if (c > '7' && c <= '9') {
		    yyerror("Illegal octal digit");
		}
		else if (c == '.' || c == 'e' || c == 'E') {
		    tokadd('0');
		}
		else {
		    pushback(c);
		    yylval.node = NEW_LIT(INT2FIX(0));
		    return tINTEGER;
		}
	    }

	    for (;;) {
		switch (c) {
		  case '0': case '1': case '2': case '3': case '4':
		  case '5': case '6': case '7': case '8': case '9':
		    nondigit = 0;
		    tokadd(c);
		    break;

		  case '.':
		    if (nondigit) goto trailing_uc;
		    if (seen_point || seen_e) {
			goto decode_num;
		    }
		    else {
			int c0 = nextc();
			if (!ISDIGIT(c0)) {
			    pushback(c0);
			    goto decode_num;
			}
			c = c0;
		    }
		    tokadd('.');
		    tokadd(c);
		    is_float++;
		    seen_point++;
		    nondigit = 0;
		    break;

		  case 'e':
		  case 'E':
		    if (nondigit) {
			pushback(c);
			c = nondigit;
			goto decode_num;
		    }
		    if (seen_e) {
			goto decode_num;
		    }
		    tokadd(c);
		    seen_e++;
		    is_float++;
		    nondigit = c;
		    c = nextc();
		    if (c != '-' && c != '+') continue;
		    tokadd(c);
		    nondigit = c;
		    break;

		  case '_':	/* `_' in number just ignored */
		    if (nondigit) goto decode_num;
		    nondigit = c;
		    break;

		  default:
		    goto decode_num;
		}
		c = nextc();
	    }

	  decode_num:
	    pushback(c);
	    tokfix();
	    if (nondigit) {
		char tmp[30];
	      trailing_uc:
		sprintf(tmp, "trailing `%c' in number", nondigit);
		yyerror(tmp);
	    }
	    if (is_float) {
		double d = strtod(tok(), 0);
		if (errno == ERANGE) {
		    rb_warn("Float %s out of range", tok());
		    errno = 0;
		}
		yylval.node = NEW_LIT(rb_float_new(d));
		return tFLOAT;
	    }
	    yylval.node = NEW_LIT(rb_cstr_to_inum(tok(), 10, Qfalse));
	    return tINTEGER;
	}

      case ']':
      case '}':
      case ')':
	COND_LEXPOP();
	CMDARG_LEXPOP();
	lex_state = EXPR_END;
	return c;

      case ':':
	c = nextc();
	if (c == ':') {
	    if (lex_state == EXPR_BEG ||  lex_state == EXPR_MID ||
		lex_state == EXPR_CLASS || (IS_ARG() && space_seen)) {
		lex_state = EXPR_BEG;
		return tCOLON3;
	    }
	    lex_state = EXPR_DOT;
	    return tCOLON2;
	}
	if (lex_state == EXPR_END || lex_state == EXPR_ENDARG || ISSPACE(c)) {
	    pushback(c);
	    lex_state = EXPR_BEG;
	    return ':';
	}
	switch (c) {
	  case '\'':
	    lex_strterm = NEW_STRTERM(str_ssym, c, 0);
	    break;
	  case '"':
	    lex_strterm = NEW_STRTERM(str_dsym, c, 0);
	    break;
	  default:
	    pushback(c);
	    break;
	}
	lex_state = EXPR_FNAME;
	return tSYMBEG;

      case '/':
	if (lex_state == EXPR_BEG || lex_state == EXPR_MID) {
	    lex_strterm = NEW_STRTERM(str_regexp, '/', 0);
	    return tREGEXP_BEG;
	}
	if ((c = nextc()) == '=') {
	    yylval.id = '/';
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	pushback(c);
	if (IS_ARG() && space_seen) {
	    if (!ISSPACE(c)) {
		arg_ambiguous();
		lex_strterm = NEW_STRTERM(str_regexp, '/', 0);
		return tREGEXP_BEG;
	    }
	}
	switch (lex_state) {
	  case EXPR_FNAME: case EXPR_DOT:
	    lex_state = EXPR_ARG; break;
	  default:
	    lex_state = EXPR_BEG; break;
	}
	return '/';

      case '^':
	if ((c = nextc()) == '=') {
	    yylval.id = '^';
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	switch (lex_state) {
	  case EXPR_FNAME: case EXPR_DOT:
	    lex_state = EXPR_ARG; break;
	  default:
	    lex_state = EXPR_BEG; break;
	}
	pushback(c);
	return '^';

      case ';':
	command_start = Qtrue;
      case ',':
	lex_state = EXPR_BEG;
	return c;

      case '~':
	if (lex_state == EXPR_FNAME || lex_state == EXPR_DOT) {
	    if ((c = nextc()) != '@') {
		pushback(c);
	    }
	}
	switch (lex_state) {
	  case EXPR_FNAME: case EXPR_DOT:
	    lex_state = EXPR_ARG; break;
	  default:
	    lex_state = EXPR_BEG; break;
	}
	return '~';

      case '(':
	command_start = Qtrue;
	if (lex_state == EXPR_BEG || lex_state == EXPR_MID) {
	    c = tLPAREN;
	}
	else if (space_seen) {
	    if (lex_state == EXPR_CMDARG) {
		c = tLPAREN_ARG;
	    }
	    else if (lex_state == EXPR_ARG) {
		rb_warn("don't put space before argument parentheses");
		c = '(';
	    }
	}
	COND_PUSH(0);
	CMDARG_PUSH(0);
	lex_state = EXPR_BEG;
	return c;

      case '[':
	if (lex_state == EXPR_FNAME || lex_state == EXPR_DOT) {
	    lex_state = EXPR_ARG;
	    if ((c = nextc()) == ']') {
		if ((c = nextc()) == '=') {
		    return tASET;
		}
		pushback(c);
		return tAREF;
	    }
	    pushback(c);
	    return '[';
	}
	else if (lex_state == EXPR_BEG || lex_state == EXPR_MID) {
	    c = tLBRACK;
	}
	else if (IS_ARG() && space_seen) {
	    c = tLBRACK;
	}
	lex_state = EXPR_BEG;
	COND_PUSH(0);
	CMDARG_PUSH(0);
	return c;

      case '{':
	if (IS_ARG() || lex_state == EXPR_END)
	    c = '{';          /* block (primary) */
	else if (lex_state == EXPR_ENDARG)
	    c = tLBRACE_ARG;  /* block (expr) */
	else
	    c = tLBRACE;      /* hash */
	COND_PUSH(0);
	CMDARG_PUSH(0);
	lex_state = EXPR_BEG;
	return c;

      case '\\':
	c = nextc();
	if (c == '\n') {
	    space_seen = 1;
	    goto retry; /* skip \\n */
	}
	pushback(c);
	return '\\';

      case '%':
	if (lex_state == EXPR_BEG || lex_state == EXPR_MID) {
	    int term;
	    int paren;

	    c = nextc();
	  quotation:
	    if (!ISALNUM(c)) {
		term = c;
		c = 'Q';
	    }
	    else {
		term = nextc();
		if (ISALNUM(term) || ismbchar(term)) {
		    yyerror("unknown type of %string");
		    return 0;
		}
	    }
	    if (c == -1 || term == -1) {
		rb_compile_error("unterminated quoted string meets end of file");
		return 0;
	    }
	    paren = term;
	    if (term == '(') term = ')';
	    else if (term == '[') term = ']';
	    else if (term == '{') term = '}';
	    else if (term == '<') term = '>';
	    else paren = 0;

	    switch (c) {
	      case 'Q':
		lex_strterm = NEW_STRTERM(str_dquote, term, paren);
		return tSTRING_BEG;

	      case 'q':
		lex_strterm = NEW_STRTERM(str_squote, term, paren);
		return tSTRING_BEG;

	      case 'W':
		lex_strterm = NEW_STRTERM(str_dquote | STR_FUNC_QWORDS, term, paren);
		do {c = nextc();} while (ISSPACE(c));
		pushback(c);
		return tWORDS_BEG;

	      case 'w':
		lex_strterm = NEW_STRTERM(str_squote | STR_FUNC_QWORDS, term, paren);
		do {c = nextc();} while (ISSPACE(c));
		pushback(c);
		return tQWORDS_BEG;

	      case 'x':
		lex_strterm = NEW_STRTERM(str_xquote, term, paren);
		return tXSTRING_BEG;

	      case 'r':
		lex_strterm = NEW_STRTERM(str_regexp, term, paren);
		return tREGEXP_BEG;

	      case 's':
		lex_strterm = NEW_STRTERM(str_ssym, term, paren);
		lex_state = EXPR_FNAME;
		return tSYMBEG;

	      default:
		yyerror("unknown type of %string");
		return 0;
	    }
	}
	if ((c = nextc()) == '=') {
	    yylval.id = '%';
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	if (IS_ARG() && space_seen && !ISSPACE(c)) {
	    goto quotation;
	}
	switch (lex_state) {
	  case EXPR_FNAME: case EXPR_DOT:
	    lex_state = EXPR_ARG; break;
	  default:
	    lex_state = EXPR_BEG; break;
	}
	pushback(c);
	return '%';

      case '$':
	lex_state = EXPR_END;
	newtok();
	c = nextc();
	switch (c) {
	  case '_':		/* $_: last read line string */
	    c = nextc();
	    if (is_identchar(c)) {
		tokadd('$');
		tokadd('_');
		break;
	    }
	    pushback(c);
	    c = '_';
	    /* fall through */
	  case '~':		/* $~: match-data */
	    local_cnt(c);
	    /* fall through */
	  case '*':		/* $*: argv */
	  case '$':		/* $$: pid */
	  case '?':		/* $?: last status */
	  case '!':		/* $!: error string */
	  case '@':		/* $@: error position */
	  case '/':		/* $/: input record separator */
	  case '\\':		/* $\: output record separator */
	  case ';':		/* $;: field separator */
	  case ',':		/* $,: output field separator */
	  case '.':		/* $.: last read line number */
	  case '=':		/* $=: ignorecase */
	  case ':':		/* $:: load path */
	  case '<':		/* $<: reading filename */
	  case '>':		/* $>: default output handle */
	  case '\"':		/* $": already loaded files */
	    tokadd('$');
	    tokadd(c);
	    tokfix();
	    yylval.id = rb_intern(tok());
	    return tGVAR;

	  case '-':
	    tokadd('$');
	    tokadd(c);
	    c = nextc();
	    tokadd(c);
	    tokfix();
	    yylval.id = rb_intern(tok());
	    /* xxx shouldn't check if valid option variable */
	    return tGVAR;

	  case '&':		/* $&: last match */
	  case '`':		/* $`: string before last match */
	  case '\'':		/* $': string after last match */
	  case '+':		/* $+: string matches last paren. */
	    yylval.node = NEW_BACK_REF(c);
	    return tBACK_REF;

	  case '1': case '2': case '3':
	  case '4': case '5': case '6':
	  case '7': case '8': case '9':
	    tokadd('$');
	    do {
		tokadd(c);
		c = nextc();
	    } while (ISDIGIT(c));
	    pushback(c);
	    tokfix();
	    yylval.node = NEW_NTH_REF(atoi(tok()+1));
	    return tNTH_REF;

	  default:
	    if (!is_identchar(c)) {
		pushback(c);
		return '$';
	    }
	  case '0':
	    tokadd('$');
	}
	break;

      case '@':
	c = nextc();
	newtok();
	tokadd('@');
	if (c == '@') {
	    tokadd('@');
	    c = nextc();
	}
	if (ISDIGIT(c)) {
	    if (tokidx == 1) {
		rb_compile_error("`@%c' is not allowed as an instance variable name", c);
	    }
	    else {
		rb_compile_error("`@@%c' is not allowed as a class variable name", c);
	    }
	}
	if (!is_identchar(c)) {
	    pushback(c);
	    return '@';
	}
	break;

      case '_':
	if (was_bol() && whole_match_p("__END__", 7, 0)) {
	    ruby__end__seen = 1;
	    lex_lastline = 0;
	    return -1;
	}
	newtok();
	break;

      default:
	if (!is_identchar(c)) {
	    rb_compile_error("Invalid char `\\%03o' in expression", c);
	    goto retry;
	}

	newtok();
	break;
    }

    do {
	tokadd(c);
	if (ismbchar(c)) {
	    int i, len = mbclen(c)-1;

	    for (i = 0; i < len; i++) {
		c = nextc();
		tokadd(c);
	    }
	}
	c = nextc();
    } while (is_identchar(c));
    if ((c == '!' || c == '?') && is_identchar(tok()[0]) && !peek('=')) {
	tokadd(c);
    }
    else {
	pushback(c);
    }
    tokfix();

    {
	int result = 0;

	switch (tok()[0]) {
	  case '$':
	    lex_state = EXPR_END;
	    result = tGVAR;
	    break;
	  case '@':
	    lex_state = EXPR_END;
	    if (tok()[1] == '@')
		result = tCVAR;
	    else
		result = tIVAR;
	    break;

	  default:
	    if (toklast() == '!' || toklast() == '?') {
		result = tFID;
	    }
	    else {
		if (lex_state == EXPR_FNAME) {
		    if ((c = nextc()) == '=' && !peek('~') && !peek('>') &&
			(!peek('=') || (lex_p + 1 < lex_pend && lex_p[1] == '>'))) {
			result = tIDENTIFIER;
			tokadd(c);
			tokfix();
		    }
		    else {
			pushback(c);
		    }
		}
		if (result == 0 && ISUPPER(tok()[0])) {
		    result = tCONSTANT;
		}
		else {
		    result = tIDENTIFIER;
		}
	    }

	    if (lex_state != EXPR_DOT) {
		struct kwtable *kw;

		/* See if it is a reserved word.  */
		kw = rb_reserved_word(tok(), toklen());
		if (kw) {
		    enum lex_state state = lex_state;
		    lex_state = kw->state;
		    if (state == EXPR_FNAME) {
			yylval.id = rb_intern(kw->name);
		    }
		    if (kw->id[0] == kDO) {
			if (COND_P()) return kDO_COND;
			if (CMDARG_P() && state != EXPR_CMDARG)
			    return kDO_BLOCK;
			if (state == EXPR_ENDARG)
			    return kDO_BLOCK;
			return kDO;
		    }
		    if (state == EXPR_BEG)
			return kw->id[0];
		    else {
			if (kw->id[0] != kw->id[1])
			    lex_state = EXPR_BEG;
			return kw->id[1];
		    }
		}
	    }

	    if (lex_state == EXPR_BEG ||
		lex_state == EXPR_MID ||
		lex_state == EXPR_DOT ||
		lex_state == EXPR_ARG ||
		lex_state == EXPR_CMDARG) {
		if (cmd_state) {
		    lex_state = EXPR_CMDARG;
		}
		else {
		    lex_state = EXPR_ARG;
		}
	    }
	    else {
		lex_state = EXPR_END;
	    }
	}
	yylval.id = rb_intern(tok());
	if (is_local_id(yylval.id) &&
	    ((dyna_in_block() && rb_dvar_defined(yylval.id)) || local_id(yylval.id))) {
	    lex_state = EXPR_END;
	}
	return result;
    }
}

NODE*
rb_node_newnode(type, a0, a1, a2)
    enum node_type type;
    VALUE a0, a1, a2;
{
    NODE *n = (NODE*)rb_newobj();

    n->flags |= T_NODE;
    nd_set_type(n, type);
    nd_set_line(n, ruby_sourceline);
    n->nd_file = ruby_sourcefile;

    n->u1.value = a0;
    n->u2.value = a1;
    n->u3.value = a2;

    return n;
}

static enum node_type
nodetype(node)			/* for debug */
    NODE *node;
{
    return (enum node_type)nd_type(node);
}

static int
nodeline(node)
    NODE *node;
{
    return nd_line(node);
}

static NODE*
newline_node(node)
    NODE *node;
{
    NODE *nl = 0;
    if (node) {
	if (nd_type(node) == NODE_NEWLINE) return node;
        nl = NEW_NEWLINE(node);
        fixpos(nl, node);
        nl->nd_nth = nd_line(node);
    }
    return nl;
}

static void
fixpos(node, orig)
    NODE *node, *orig;
{
    if (!node) return;
    if (!orig) return;
    if (orig == (NODE*)1) return;
    node->nd_file = orig->nd_file;
    nd_set_line(node, nd_line(orig));
}

static void
parser_warning(node, mesg)
    NODE *node;
    const char *mesg;
{
    int line = ruby_sourceline;
    ruby_sourceline = nd_line(node);
    rb_warning(mesg);
    ruby_sourceline = line;
}

static void
parser_warn(node, mesg)
    NODE *node;
    const char *mesg;
{
    int line = ruby_sourceline;
    ruby_sourceline = nd_line(node);
    rb_warn(mesg);
    ruby_sourceline = line;
}

static NODE*
block_append(head, tail)
    NODE *head, *tail;
{
    NODE *end, *h = head;

    if (tail == 0) return head;

  again:
    if (h == 0) return tail;
    switch (nd_type(h)) {
      case NODE_NEWLINE:
	h = h->nd_next;
	goto again;
      case NODE_LIT:
      case NODE_STR:
	parser_warning(h, "unused literal ignored");
	return tail;
      default:
	h = end = NEW_BLOCK(head);
	end->nd_end = end;
	fixpos(end, head);
	head = end;
	break;
      case NODE_BLOCK:
	end = h->nd_end;
	break;
    }

    if (RTEST(ruby_verbose)) {
	NODE *nd = end->nd_head;
      newline:
	switch (nd_type(nd)) {
	  case NODE_RETURN:
	  case NODE_BREAK:
	  case NODE_NEXT:
	  case NODE_REDO:
	  case NODE_RETRY:
	    parser_warning(nd, "statement not reached");
	    break;

	case NODE_NEWLINE:
	    nd = nd->nd_next;
	    goto newline;

	  default:
	    break;
	}
    }

    if (nd_type(tail) != NODE_BLOCK) {
	tail = NEW_BLOCK(tail);
	tail->nd_end = tail;
    }
    end->nd_next = tail;
    h->nd_end = tail->nd_end;
    return head;
}

/* append item to the list */
static NODE*
list_append(list, item)
    NODE *list, *item;
{
    NODE *last;

    if (list == 0) return NEW_LIST(item);
    if (list->nd_next) {
	last = list->nd_next->nd_end;
    }
    else {
	last = list;
    }

    list->nd_alen += 1;
    last->nd_next = NEW_LIST(item);
    list->nd_next->nd_end = last->nd_next;
    return list;
}

/* concat two lists */
static NODE*
list_concat(head, tail)
    NODE *head, *tail;
{
    NODE *last;

    if (head->nd_next) {
	last = head->nd_next->nd_end;
    }
    else {
	last = head;
    }

    head->nd_alen += tail->nd_alen;
    last->nd_next = tail;
    if (tail->nd_next) {
	head->nd_next->nd_end = tail->nd_next->nd_end;
    }
    else {
	head->nd_next->nd_end = tail;
    }

    return head;
}

/* concat two string literals */
static NODE *
literal_concat(head, tail)
    NODE *head, *tail;
{
    enum node_type htype;

    if (!head) return tail;
    if (!tail) return head;

    htype = nd_type(head);
    if (htype == NODE_EVSTR) {
	NODE *node = NEW_DSTR(rb_str_new(0, 0));
	head = list_append(node, head);
    }
    switch (nd_type(tail)) {
      case NODE_STR:
	if (htype == NODE_STR) {
	    rb_str_concat(head->nd_lit, tail->nd_lit);
	    rb_gc_force_recycle((VALUE)tail);
	}
	else {
	    list_append(head, tail);
	}
	break;

      case NODE_DSTR:
	if (htype == NODE_STR) {
	    rb_str_concat(head->nd_lit, tail->nd_lit);
	    tail->nd_lit = head->nd_lit;
	    rb_gc_force_recycle((VALUE)head);
	    head = tail;
	}
	else {
	    nd_set_type(tail, NODE_ARRAY);
	    tail->nd_head = NEW_STR(tail->nd_lit);
	    list_concat(head, tail);
	}
	break;

      case NODE_EVSTR:
	if (htype == NODE_STR) {
	    nd_set_type(head, NODE_DSTR);
	    head->nd_alen = 1;
	}
	list_append(head, tail);
	break;
    }
    return head;
}

static NODE *
evstr2dstr(node)
    NODE *node;
{
    if (nd_type(node) == NODE_EVSTR) {
	node = list_append(NEW_DSTR(rb_str_new(0, 0)), node);
    }
    return node;
}

static NODE *
new_evstr(node)
    NODE *node;
{
    NODE *head = node;

  again:
    if (node) {
	switch (nd_type(node)) {
	  case NODE_STR: case NODE_DSTR: case NODE_EVSTR:
	    return node;
	  case NODE_NEWLINE:
	    node = node->nd_next;
	    goto again;
	}
    }
    return NEW_EVSTR(head);
}

static NODE *
call_op(recv, id, narg, arg1)
    NODE *recv;
    ID id;
    int narg;
    NODE *arg1;
{
    value_expr(recv);
    if (narg == 1) {
	value_expr(arg1);
	arg1 = NEW_LIST(arg1);
    }
    else {
	arg1 = 0;
    }
    return NEW_CALL(recv, id, arg1);
}

static NODE*
match_gen(node1, node2)
    NODE *node1;
    NODE *node2;
{
    local_cnt('~');

    value_expr(node1);
    value_expr(node2);
    if (node1) {
	switch (nd_type(node1)) {
	  case NODE_DREGX:
	  case NODE_DREGX_ONCE:
	    return NEW_MATCH2(node1, node2);

	  case NODE_LIT:
	    if (TYPE(node1->nd_lit) == T_REGEXP) {
		return NEW_MATCH2(node1, node2);
	    }
	}
    }

    if (node2) {
	switch (nd_type(node2)) {
	  case NODE_DREGX:
	  case NODE_DREGX_ONCE:
	    return NEW_MATCH3(node2, node1);

	  case NODE_LIT:
	    if (TYPE(node2->nd_lit) == T_REGEXP) {
		return NEW_MATCH3(node2, node1);
	    }
	}
    }

    return NEW_CALL(node1, tMATCH, NEW_LIST(node2));
}

static NODE*
gettable(id)
    ID id;
{
    if (id == kSELF) {
	return NEW_SELF();
    }
    else if (id == kNIL) {
	return NEW_NIL();
    }
    else if (id == kTRUE) {
	return NEW_TRUE();
    }
    else if (id == kFALSE) {
	return NEW_FALSE();
    }
    else if (id == k__FILE__) {
	return NEW_STR(rb_str_new2(ruby_sourcefile));
    }
    else if (id == k__LINE__) {
	return NEW_LIT(INT2FIX(ruby_sourceline));
    }
    else if (is_local_id(id)) {
	if (dyna_in_block() && rb_dvar_defined(id)) return NEW_DVAR(id);
	if (local_id(id)) return NEW_LVAR(id);
	/* method call without arguments */
#if 0
	/* Rite will warn this */
	rb_warn("ambiguous identifier; %s() or self.%s is better for method call",
		rb_id2name(id), rb_id2name(id));
#endif
	return NEW_VCALL(id);
    }
    else if (is_global_id(id)) {
	return NEW_GVAR(id);
    }
    else if (is_instance_id(id)) {
	return NEW_IVAR(id);
    }
    else if (is_const_id(id)) {
	return NEW_CONST(id);
    }
    else if (is_class_id(id)) {
	return NEW_CVAR(id);
    }
    rb_compile_error("identifier %s is not valid", rb_id2name(id));
    return 0;
}

static NODE*
assignable(id, val)
    ID id;
    NODE *val;
{
    value_expr(val);
    if (id == kSELF) {
	yyerror("Can't change the value of self");
    }
    else if (id == kNIL) {
	yyerror("Can't assign to nil");
    }
    else if (id == kTRUE) {
	yyerror("Can't assign to true");
    }
    else if (id == kFALSE) {
	yyerror("Can't assign to false");
    }
    else if (id == k__FILE__) {
	yyerror("Can't assign to __FILE__");
    }
    else if (id == k__LINE__) {
	yyerror("Can't assign to __LINE__");
    }
    else if (is_local_id(id)) {
	if (rb_dvar_curr(id)) {
	    return NEW_DASGN_CURR(id, val);
	}
	else if (rb_dvar_defined(id)) {
	    return NEW_DASGN(id, val);
	}
	else if (local_id(id) || !dyna_in_block()) {
	    return NEW_LASGN(id, val);
	}
	else{
	    rb_dvar_push(id, Qnil);
	    return NEW_DASGN_CURR(id, val);
	}
    }
    else if (is_global_id(id)) {
	return NEW_GASGN(id, val);
    }
    else if (is_instance_id(id)) {
	return NEW_IASGN(id, val);
    }
    else if (is_const_id(id)) {
	if (in_def || in_single)
	    yyerror("dynamic constant assignment");
	return NEW_CDECL(id, val, 0);
    }
    else if (is_class_id(id)) {
	if (in_def || in_single) return NEW_CVASGN(id, val);
	return NEW_CVDECL(id, val);
    }
    else {
	rb_compile_error("identifier %s is not valid", rb_id2name(id));
    }
    return 0;
}

static NODE *
aryset(recv, idx)
    NODE *recv, *idx;
{
    if (recv && nd_type(recv) == NODE_SELF)
	recv = (NODE *)1;
    else
	value_expr(recv);
    return NEW_ATTRASGN(recv, tASET, idx);
}

ID
rb_id_attrset(id)
    ID id;
{
    id &= ~ID_SCOPE_MASK;
    id |= ID_ATTRSET;
    return id;
}

static NODE *
attrset(recv, id)
    NODE *recv;
    ID id;
{
    if (recv && nd_type(recv) == NODE_SELF)
	recv = (NODE *)1;
    else
	value_expr(recv);
    return NEW_ATTRASGN(recv, rb_id_attrset(id), 0);
}

static void
rb_backref_error(node)
    NODE *node;
{
    switch (nd_type(node)) {
      case NODE_NTH_REF:
	rb_compile_error("Can't set variable $%d", node->nd_nth);
	break;
      case NODE_BACK_REF:
	rb_compile_error("Can't set variable $%c", (int)node->nd_nth);
	break;
    }
}

static NODE *
arg_concat(node1, node2)
    NODE *node1;
    NODE *node2;
{
    if (!node2) return node1;
    return NEW_ARGSCAT(node1, node2);
}

static NODE *
arg_add(node1, node2)
    NODE *node1;
    NODE *node2;
{
    if (!node1) return NEW_LIST(node2);
    if (nd_type(node1) == NODE_ARRAY) {
	return list_append(node1, node2);
    }
    else {
	return NEW_ARGSPUSH(node1, node2);
    }
}

static NODE*
node_assign(lhs, rhs)
    NODE *lhs, *rhs;
{
    if (!lhs) return 0;

    value_expr(rhs);
    switch (nd_type(lhs)) {
      case NODE_GASGN:
      case NODE_IASGN:
      case NODE_LASGN:
      case NODE_DASGN:
      case NODE_DASGN_CURR:
      case NODE_MASGN:
      case NODE_CDECL:
      case NODE_CVDECL:
      case NODE_CVASGN:
	lhs->nd_value = rhs;
	break;

      case NODE_ATTRASGN:
      case NODE_CALL:
	lhs->nd_args = arg_add(lhs->nd_args, rhs);
	break;

      default:
	/* should not happen */
	break;
    }

    return lhs;
}

static int
value_expr0(node)
    NODE *node;
{
    int cond = 0;

    while (node) {
	switch (nd_type(node)) {
	  case NODE_DEFN:
	  case NODE_DEFS:
	    parser_warning(node, "void value expression");
	    return Qfalse;

	  case NODE_RETURN:
	  case NODE_BREAK:
	  case NODE_NEXT:
	  case NODE_REDO:
	  case NODE_RETRY:
	    if (!cond) yyerror("void value expression");
	    /* or "control never reach"? */
	    return Qfalse;

	  case NODE_BLOCK:
	    while (node->nd_next) {
		node = node->nd_next;
	    }
	    node = node->nd_head;
	    break;

	  case NODE_BEGIN:
	    node = node->nd_body;
	    break;

	  case NODE_IF:
	    if (!value_expr(node->nd_body)) return Qfalse;
	    node = node->nd_else;
	    break;

	  case NODE_AND:
	  case NODE_OR:
	    cond = 1;
	    node = node->nd_2nd;
	    break;

	  case NODE_NEWLINE:
	    node = node->nd_next;
	    break;

	  default:
	    return Qtrue;
	}
    }

    return Qtrue;
}

static void
void_expr0(node)
    NODE *node;
{
    char *useless = 0;

    if (!RTEST(ruby_verbose)) return;

  again:
    if (!node) return;
    switch (nd_type(node)) {
      case NODE_NEWLINE:
	node = node->nd_next;
	goto again;

      case NODE_CALL:
	switch (node->nd_mid) {
	  case '+':
	  case '-':
	  case '*':
	  case '/':
	  case '%':
	  case tPOW:
	  case tUPLUS:
	  case tUMINUS:
	  case '|':
	  case '^':
	  case '&':
	  case tCMP:
	  case '>':
	  case tGEQ:
	  case '<':
	  case tLEQ:
	  case tEQ:
	  case tNEQ:
	    useless = rb_id2name(node->nd_mid);
	    break;
	}
	break;

      case NODE_LVAR:
      case NODE_DVAR:
      case NODE_GVAR:
      case NODE_IVAR:
      case NODE_CVAR:
      case NODE_NTH_REF:
      case NODE_BACK_REF:
	useless = "a variable";
	break;
      case NODE_CONST:
      case NODE_CREF:
	useless = "a constant";
	break;
      case NODE_LIT:
      case NODE_STR:
      case NODE_DSTR:
      case NODE_DREGX:
      case NODE_DREGX_ONCE:
	useless = "a literal";
	break;
      case NODE_COLON2:
      case NODE_COLON3:
	useless = "::";
	break;
      case NODE_DOT2:
	useless = "..";
	break;
      case NODE_DOT3:
	useless = "...";
	break;
      case NODE_SELF:
	useless = "self";
	break;
      case NODE_NIL:
	useless = "nil";
	break;
      case NODE_TRUE:
	useless = "true";
	break;
      case NODE_FALSE:
	useless = "false";
	break;
      case NODE_DEFINED:
	useless = "defined?";
	break;
    }

    if (useless) {
	int line = ruby_sourceline;

	ruby_sourceline = nd_line(node);
	rb_warn("useless use of %s in void context", useless);
	ruby_sourceline = line;
    }
}

static void
void_stmts(node)
    NODE *node;
{
    if (!RTEST(ruby_verbose)) return;
    if (!node) return;
    if (nd_type(node) != NODE_BLOCK) return;

    for (;;) {
	if (!node->nd_next) return;
	void_expr(node->nd_head);
	node = node->nd_next;
    }
}

static NODE *
remove_begin(node)
    NODE *node;
{
    NODE **n = &node;
    while (*n) {
	switch (nd_type(*n)) {
	  case NODE_NEWLINE:
	    n = &(*n)->nd_next;
	    continue;
	  case NODE_BEGIN:
	    *n = (*n)->nd_body;
	  default:
	    return node;
	}
    }
    return node;
}

static int
assign_in_cond(node)
    NODE *node;
{
    switch (nd_type(node)) {
      case NODE_MASGN:
	yyerror("multiple assignment in conditional");
	return 1;

      case NODE_LASGN:
      case NODE_DASGN:
      case NODE_GASGN:
      case NODE_IASGN:
	break;

      case NODE_NEWLINE:
      default:
	return 0;
    }

    switch (nd_type(node->nd_value)) {
      case NODE_LIT:
      case NODE_STR:
      case NODE_NIL:
      case NODE_TRUE:
      case NODE_FALSE:
	/* reports always */
	parser_warn(node->nd_value, "found = in conditional, should be ==");
	return 1;

      case NODE_DSTR:
      case NODE_XSTR:
      case NODE_DXSTR:
      case NODE_EVSTR:
      case NODE_DREGX:
      default:
	break;
    }
#if 0
    if (assign_in_cond(node->nd_value) == 0) {
	parser_warning(node->nd_value, "assignment in condition");
    }
#endif
    return 1;
}

static int
e_option_supplied()
{
    if (strcmp(ruby_sourcefile, "-e") == 0)
	return Qtrue;
    return Qfalse;
}

static void
warn_unless_e_option(node, str)
    NODE *node;
    const char *str;
{
    if (!e_option_supplied()) parser_warn(node, str);
}

static void
warning_unless_e_option(node, str)
    NODE *node;
    const char *str;
{
    if (!e_option_supplied()) parser_warning(node, str);
}

static NODE *cond0();

static NODE*
range_op(node)
    NODE *node;
{
    enum node_type type;

    if (!e_option_supplied()) return node;
    if (node == 0) return 0;

    value_expr(node);
    node = cond0(node);
    type = nd_type(node);
    if (type == NODE_NEWLINE) {
	node = node->nd_next;
	type = nd_type(node);
    }
    if (type == NODE_LIT && FIXNUM_P(node->nd_lit)) {
	warn_unless_e_option(node, "integer literal in conditional range");
	return call_op(node,tEQ,1,NEW_GVAR(rb_intern("$.")));
    }
    return node;
}

static int
literal_node(node)
    NODE *node;
{
    if (!node) return 1;	/* same as NODE_NIL */
    switch (nd_type(node)) {
      case NODE_LIT:
      case NODE_STR:
      case NODE_DSTR:
      case NODE_EVSTR:
      case NODE_DREGX:
      case NODE_DREGX_ONCE:
      case NODE_DSYM:
	return 2;
      case NODE_TRUE:
      case NODE_FALSE:
      case NODE_NIL:
	return 1;
    }
    return 0;
}

static NODE*
cond0(node)
    NODE *node;
{
    if (node == 0) return 0;
    assign_in_cond(node);

    switch (nd_type(node)) {
      case NODE_DSTR:
      case NODE_EVSTR:
      case NODE_STR:
	rb_warn("string literal in condition");
	break;

      case NODE_DREGX:
      case NODE_DREGX_ONCE:
	warning_unless_e_option(node, "regex literal in condition");
	local_cnt('_');
	local_cnt('~');
	return NEW_MATCH2(node, NEW_GVAR(rb_intern("$_")));

      case NODE_AND:
      case NODE_OR:
	node->nd_1st = cond0(node->nd_1st);
	node->nd_2nd = cond0(node->nd_2nd);
	break;

      case NODE_DOT2:
      case NODE_DOT3:
	node->nd_beg = range_op(node->nd_beg);
	node->nd_end = range_op(node->nd_end);
	if (nd_type(node) == NODE_DOT2) nd_set_type(node,NODE_FLIP2);
	else if (nd_type(node) == NODE_DOT3) nd_set_type(node, NODE_FLIP3);
	node->nd_cnt = local_append(internal_id());
	if (!e_option_supplied()) {
	    int b = literal_node(node->nd_beg);
	    int e = literal_node(node->nd_end);
	    if ((b == 1 && e == 1) || (b + e >= 2 && RTEST(ruby_verbose))) {
		parser_warn(node, "range literal in condition");
	    }
	}
	break;

      case NODE_DSYM:
	parser_warning(node, "literal in condition");
	break;

      case NODE_LIT:
	if (TYPE(node->nd_lit) == T_REGEXP) {
	    warn_unless_e_option(node, "regex literal in condition");
	    nd_set_type(node, NODE_MATCH);
	    local_cnt('_');
	    local_cnt('~');
	}
	else {
	    parser_warning(node, "literal in condition");
	}
      default:
	break;
    }
    return node;
}

static NODE*
cond(node)
    NODE *node;
{
    if (node == 0) return 0;
    value_expr(node);
    if (nd_type(node) == NODE_NEWLINE){
	node->nd_next = cond0(node->nd_next);
	return node;
    }
    return cond0(node);
}

static NODE*
logop(type, left, right)
    enum node_type type;
    NODE *left, *right;
{
    value_expr(left);
    if (left && nd_type(left) == type) {
	NODE *node = left, *second;
	while ((second = node->nd_2nd) != 0 && nd_type(second) == type) {
	    node = second;
	}
	node->nd_2nd = NEW_NODE(type, second, right, 0);
	return left;
    }
    return NEW_NODE(type, left, right, 0);
}

static int
cond_negative(nodep)
    NODE **nodep;
{
    NODE *c = *nodep;

    if (!c) return 0;
    switch (nd_type(c)) {
      case NODE_NOT:
	*nodep = c->nd_body;
	return 1;
      case NODE_NEWLINE:
	if (c->nd_next && nd_type(c->nd_next) == NODE_NOT) {
	    c->nd_next = c->nd_next->nd_body;
	    return 1;
	}
    }
    return 0;
}

static void
no_blockarg(node)
    NODE *node;
{
    if (node && nd_type(node) == NODE_BLOCK_PASS) {
	rb_compile_error("block argument should not be given");
    }
}

static NODE *
ret_args(node)
    NODE *node;
{
    if (node) {
	no_blockarg(node);
	if (nd_type(node) == NODE_ARRAY && node->nd_next == 0) {
	    node = node->nd_head;
	}
	if (node && nd_type(node) == NODE_SPLAT) {
	    node = NEW_SVALUE(node);
	}
    }
    return node;
}

static NODE *
new_yield(node)
    NODE *node;
{
    long state = Qtrue;

    if (node) {
        no_blockarg(node);
        if (nd_type(node) == NODE_ARRAY && node->nd_next == 0) {
            node = node->nd_head;
            state = Qfalse;
        }
        if (node && nd_type(node) == NODE_SPLAT) {
            state = Qtrue;
        }
    }
    else {
        state = Qfalse;
    }
    return NEW_YIELD(node, state);
}

static NODE*
negate_lit(node)
    NODE *node;
{
    switch (TYPE(node->nd_lit)) {
      case T_FIXNUM:
	node->nd_lit = LONG2FIX(-FIX2LONG(node->nd_lit));
	break;
      case T_BIGNUM:
	node->nd_lit = rb_funcall(node->nd_lit,tUMINUS,0,0);
	break;
      case T_FLOAT:
	RFLOAT(node->nd_lit)->value = -RFLOAT(node->nd_lit)->value;
	break;
      default:
	break;
    }
    return node;
}

static NODE *
arg_blk_pass(node1, node2)
    NODE *node1;
    NODE *node2;
{
    if (node2) {
	node2->nd_head = node1;
	return node2;
    }
    return node1;
}

static NODE*
arg_prepend(node1, node2)
    NODE *node1, *node2;
{
    switch (nd_type(node2)) {
      case NODE_ARRAY:
	return list_concat(NEW_LIST(node1), node2);

      case NODE_SPLAT:
	return arg_concat(node1, node2->nd_head);

      case NODE_BLOCK_PASS:
	node2->nd_body = arg_prepend(node1, node2->nd_body);
	return node2;

      default:
	rb_bug("unknown nodetype(%d) for arg_prepend", nd_type(node2));
    }
    return 0;			/* not reached */
}

static NODE*
new_call(r,m,a)
    NODE *r;
    ID m;
    NODE *a;
{
    if (a && nd_type(a) == NODE_BLOCK_PASS) {
	a->nd_iter = NEW_CALL(r,m,a->nd_head);
	return a;
    }
    return NEW_CALL(r,m,a);
}

static NODE*
new_fcall(m,a)
    ID m;
    NODE *a;
{
    if (a && nd_type(a) == NODE_BLOCK_PASS) {
	a->nd_iter = NEW_FCALL(m,a->nd_head);
	return a;
    }
    return NEW_FCALL(m,a);
}

static NODE*
new_super(a)
    NODE *a;
{
    if (a && nd_type(a) == NODE_BLOCK_PASS) {
	a->nd_iter = NEW_SUPER(a->nd_head);
	return a;
    }
    return NEW_SUPER(a);
}

static struct local_vars {
    ID *tbl;
    int nofree;
    int cnt;
    int dlev;
    struct RVarmap* dyna_vars;
    struct local_vars *prev;
} *lvtbl;

static void
local_push(top)
    int top;
{
    struct local_vars *local;

    local = ALLOC(struct local_vars);
    local->prev = lvtbl;
    local->nofree = 0;
    local->cnt = 0;
    local->tbl = 0;
    local->dlev = 0;
    local->dyna_vars = ruby_dyna_vars;
    lvtbl = local;
    if (!top) {
	/* preserve reference for GC, but link should be cut. */
	rb_dvar_push(0, (VALUE)ruby_dyna_vars);
	ruby_dyna_vars->next = 0;
    }
}

static void
local_pop()
{
    struct local_vars *local = lvtbl->prev;

    if (lvtbl->tbl) {
	if (!lvtbl->nofree) free(lvtbl->tbl);
	else lvtbl->tbl[0] = lvtbl->cnt;
    }
    ruby_dyna_vars = lvtbl->dyna_vars;
    free(lvtbl);
    lvtbl = local;
}

static ID*
local_tbl()
{
    lvtbl->nofree = 1;
    return lvtbl->tbl;
}

static int
local_append(id)
    ID id;
{
    if (lvtbl->tbl == 0) {
	lvtbl->tbl = ALLOC_N(ID, 4);
	lvtbl->tbl[0] = 0;
	lvtbl->tbl[1] = '_';
	lvtbl->tbl[2] = '~';
	lvtbl->cnt = 2;
	if (id == '_') return 0;
	if (id == '~') return 1;
    }
    else {
	REALLOC_N(lvtbl->tbl, ID, lvtbl->cnt+2);
    }

    lvtbl->tbl[lvtbl->cnt+1] = id;
    return lvtbl->cnt++;
}

static int
local_cnt(id)
    ID id;
{
    int cnt, max;

    if (id == 0) return lvtbl->cnt;

    for (cnt=1, max=lvtbl->cnt+1; cnt<max;cnt++) {
	if (lvtbl->tbl[cnt] == id) return cnt-1;
    }
    return local_append(id);
}

static int
local_id(id)
    ID id;
{
    int i, max;

    if (lvtbl == 0) return Qfalse;
    for (i=3, max=lvtbl->cnt+1; i<max; i++) {
	if (lvtbl->tbl[i] == id) return Qtrue;
    }
    return Qfalse;
}

static void
top_local_init()
{
    local_push(1);
    lvtbl->cnt = ruby_scope->local_tbl?ruby_scope->local_tbl[0]:0;
    if (lvtbl->cnt > 0) {
	lvtbl->tbl = ALLOC_N(ID, lvtbl->cnt+3);
	MEMCPY(lvtbl->tbl, ruby_scope->local_tbl, ID, lvtbl->cnt+1);
    }
    else {
	lvtbl->tbl = 0;
    }
    if (ruby_dyna_vars)
	lvtbl->dlev = 1;
    else
	lvtbl->dlev = 0;
}

static void
top_local_setup()
{
    int len = lvtbl->cnt;
    int i;

    if (len > 0) {
	i = ruby_scope->local_tbl?ruby_scope->local_tbl[0]:0;

	if (i < len) {
	    if (i == 0 || (ruby_scope->flags & SCOPE_MALLOC) == 0) {
		VALUE *vars = ALLOC_N(VALUE, len+1);
		if (ruby_scope->local_vars) {
		    *vars++ = ruby_scope->local_vars[-1];
		    MEMCPY(vars, ruby_scope->local_vars, VALUE, i);
		    rb_mem_clear(vars+i, len-i);
		}
		else {
		    *vars++ = 0;
		    rb_mem_clear(vars, len);
		}
		ruby_scope->local_vars = vars;
		ruby_scope->flags |= SCOPE_MALLOC;
	    }
	    else {
		VALUE *vars = ruby_scope->local_vars-1;
		REALLOC_N(vars, VALUE, len+1);
		ruby_scope->local_vars = vars+1;
		rb_mem_clear(ruby_scope->local_vars+i, len-i);
	    }
	    if (ruby_scope->local_tbl && ruby_scope->local_vars[-1] == 0) {
		free(ruby_scope->local_tbl);
	    }
	    ruby_scope->local_vars[-1] = 0;
	    ruby_scope->local_tbl = local_tbl();
	}
    }
    local_pop();
}

static struct RVarmap*
dyna_push()
{
    struct RVarmap* vars = ruby_dyna_vars;

    rb_dvar_push(0, 0);
    lvtbl->dlev++;
    return vars;
}

static void
dyna_pop(vars)
    struct RVarmap* vars;
{
    lvtbl->dlev--;
    ruby_dyna_vars = vars;
}

static int
dyna_in_block()
{
    return (lvtbl->dlev > 0);
}

static NODE *
dyna_init(node, pre)
    NODE *node;
    struct RVarmap *pre;
{
    struct RVarmap *post = ruby_dyna_vars;
    NODE *var;

    if (!node || !post || pre == post) return node;
    for (var = 0; post != pre && post->id; post = post->next) {
	var = NEW_DASGN_CURR(post->id, var);
    }
    return block_append(var, node);
}

int
ruby_parser_stack_on_heap()
{
#if defined(YYBISON) && !defined(C_ALLOCA)
    return Qfalse;
#else
    return Qtrue;
#endif
}

void
rb_gc_mark_parser()
{
    if (!ruby_in_compile) return;

    rb_gc_mark_maybe((VALUE)yylval.node);
    rb_gc_mark(ruby_debug_lines);
    rb_gc_mark(lex_lastline);
    rb_gc_mark(lex_input);
    rb_gc_mark((VALUE)lex_strterm);
}

void
rb_parser_append_print()
{
    ruby_eval_tree =
	block_append(ruby_eval_tree,
		     NEW_FCALL(rb_intern("print"),
			       NEW_ARRAY(NEW_GVAR(rb_intern("$_")))));
}

void
rb_parser_while_loop(chop, split)
    int chop, split;
{
    if (split) {
	ruby_eval_tree =
	    block_append(NEW_GASGN(rb_intern("$F"),
				   NEW_CALL(NEW_GVAR(rb_intern("$_")),
					    rb_intern("split"), 0)),
				   ruby_eval_tree);
    }
    if (chop) {
	ruby_eval_tree =
	    block_append(NEW_CALL(NEW_GVAR(rb_intern("$_")),
				  rb_intern("chop!"), 0), ruby_eval_tree);
    }
    ruby_eval_tree = NEW_OPT_N(ruby_eval_tree);
}

static struct {
    ID token;
    char *name;
} op_tbl[] = {
    {tDOT2,	".."},
    {tDOT3,	"..."},
    {'+',	"+"},
    {'-',	"-"},
    {'+',	"+(binary)"},
    {'-',	"-(binary)"},
    {'*',	"*"},
    {'/',	"/"},
    {'%',	"%"},
    {tPOW,	"**"},
    {tUPLUS,	"+@"},
    {tUMINUS,	"-@"},
    {tUPLUS,	"+(unary)"},
    {tUMINUS,	"-(unary)"},
    {'|',	"|"},
    {'^',	"^"},
    {'&',	"&"},
    {tCMP,	"<=>"},
    {'>',	">"},
    {tGEQ,	">="},
    {'<',	"<"},
    {tLEQ,	"<="},
    {tEQ,	"=="},
    {tEQQ,	"==="},
    {tNEQ,	"!="},
    {tMATCH,	"=~"},
    {tNMATCH,	"!~"},
    {'!',	"!"},
    {'~',	"~"},
    {'!',	"!(unary)"},
    {'~',	"~(unary)"},
    {'!',	"!@"},
    {'~',	"~@"},
    {tAREF,	"[]"},
    {tASET,	"[]="},
    {tLSHFT,	"<<"},
    {tRSHFT,	">>"},
    {tCOLON2,	"::"},
    {'`',	"`"},
    {0,	0}
};

static st_table *sym_tbl;
static st_table *sym_rev_tbl;

void
Init_sym()
{
    sym_tbl = st_init_strtable_with_size(200);
    sym_rev_tbl = st_init_numtable_with_size(200);
}

static ID last_id = tLAST_TOKEN;

static ID
internal_id()
{
    return ID_INTERNAL | (++last_id << ID_SCOPE_SHIFT);
}

ID
rb_intern(name)
    const char *name;
{
    const char *m = name;
    ID id;
    int last;

    if (st_lookup(sym_tbl, (st_data_t)name, (st_data_t *)&id))
	return id;

    last = strlen(name)-1;
    id = 0;
    switch (*name) {
      case '$':
	id |= ID_GLOBAL;
	m++;
	if (!is_identchar(*m)) m++;
	break;
      case '@':
	if (name[1] == '@') {
	    m++;
	    id |= ID_CLASS;
	}
	else {
	    id |= ID_INSTANCE;
	}
	m++;
	break;
      default:
	if (name[0] != '_' && !ISALPHA(name[0]) && !ismbchar(name[0])) {
	    /* operators */
	    int i;

	    for (i=0; op_tbl[i].token; i++) {
		if (*op_tbl[i].name == *name &&
		    strcmp(op_tbl[i].name, name) == 0) {
		    id = op_tbl[i].token;
		    goto id_regist;
		}
	    }
	}

	if (name[last] == '=') {
	    /* attribute assignment */
	    char *buf = ALLOCA_N(char,last+1);

	    strncpy(buf, name, last);
	    buf[last] = '\0';
	    id = rb_intern(buf);
	    if (id > tLAST_TOKEN && !is_attrset_id(id)) {
		id = rb_id_attrset(id);
		goto id_regist;
	    }
	    id = ID_ATTRSET;
	}
	else if (ISUPPER(name[0])) {
	    id = ID_CONST;
        }
	else {
	    id = ID_LOCAL;
	}
	break;
    }
    while (m <= name + last && is_identchar(*m)) {
	m += mbclen(*m);
    }
    if (*m) id = ID_JUNK;
    id |= ++last_id << ID_SCOPE_SHIFT;
  id_regist:
    name = strdup(name);
    st_add_direct(sym_tbl, (st_data_t)name, id);
    st_add_direct(sym_rev_tbl, id, (st_data_t)name);
    return id;
}

char *
rb_id2name(id)
    ID id;
{
    char *name;

    if (id < tLAST_TOKEN) {
	int i = 0;

	for (i=0; op_tbl[i].token; i++) {
	    if (op_tbl[i].token == id)
		return op_tbl[i].name;
	}
    }

    if (st_lookup(sym_rev_tbl, id, (st_data_t *)&name))
	return name;

    if (is_attrset_id(id)) {
	ID id2 = (id & ~ID_SCOPE_MASK) | ID_LOCAL;

      again:
	name = rb_id2name(id2);
	if (name) {
	    char *buf = ALLOCA_N(char, strlen(name)+2);

	    strcpy(buf, name);
	    strcat(buf, "=");
	    rb_intern(buf);
	    return rb_id2name(id);
	}
	if (is_local_id(id2)) {
	    id2 = (id & ~ID_SCOPE_MASK) | ID_CONST;
	    goto again;
	}
    }
    return 0;
}

static int
symbols_i(key, value, ary)
    char *key;
    ID value;
    VALUE ary;
{
    rb_ary_push(ary, ID2SYM(value));
    return ST_CONTINUE;
}

/*
 *  call-seq:
 *     Symbol.all_symbols    => array
 *  
 *  Returns an array of all the symbols currently in Ruby's symbol
 *  table.
 *     
 *     Symbol.all_symbols.size    #=> 903
 *     Symbol.all_symbols[1,20]   #=> [:floor, :ARGV, :Binding, :symlink,
 *                                     :chown, :EOFError, :$;, :String, 
 *                                     :LOCK_SH, :"setuid?", :$<, 
 *                                     :default_proc, :compact, :extend, 
 *                                     :Tms, :getwd, :$=, :ThreadGroup,
 *                                     :wait2, :$>]
 */

VALUE
rb_sym_all_symbols()
{
    VALUE ary = rb_ary_new2(sym_tbl->num_entries);

    st_foreach(sym_tbl, symbols_i, ary);
    return ary;
}

int
rb_is_const_id(id)
    ID id;
{
    if (is_const_id(id)) return Qtrue;
    return Qfalse;
}

int
rb_is_class_id(id)
    ID id;
{
    if (is_class_id(id)) return Qtrue;
    return Qfalse;
}

int
rb_is_instance_id(id)
    ID id;
{
    if (is_instance_id(id)) return Qtrue;
    return Qfalse;
}

int
rb_is_local_id(id)
    ID id;
{
    if (is_local_id(id)) return Qtrue;
    return Qfalse;
}

int
rb_is_junk_id(id)
    ID id;
{
    if (is_junk_id(id)) return Qtrue;
    return Qfalse;
}

static void
special_local_set(c, val)
    char c;
    VALUE val;
{
    int cnt;

    top_local_init();
    cnt = local_cnt(c);
    top_local_setup();
    ruby_scope->local_vars[cnt] = val;
}

VALUE
rb_backref_get()
{
    VALUE *var = rb_svar(1);
    if (var) {
	return *var;
    }
    return Qnil;
}

void
rb_backref_set(val)
    VALUE val;
{
    VALUE *var = rb_svar(1);
    if (var) {
	*var = val;
    }
    else {
	special_local_set('~', val);
    }
}

VALUE
rb_lastline_get()
{
    VALUE *var = rb_svar(0);
    if (var) {
	return *var;
    }
    return Qnil;
}

void
rb_lastline_set(val)
    VALUE val;
{
    VALUE *var = rb_svar(0);
    if (var) {
	*var = val;
    }
    else {
	special_local_set('_', val);
    }
}


