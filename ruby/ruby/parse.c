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
/* Tokens.  */
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
#define YYERROR_VERBOSE 1
#ifndef YYSTACK_USE_ALLOCA
#define YYSTACK_USE_ALLOCA 0
#endif

#include "ruby.h"
#include "env.h"
#include "intern.h"
#include "node.h"
#include "st.h"
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#define YYMALLOC	rb_parser_malloc
#define YYREALLOC	rb_parser_realloc
#define YYCALLOC	rb_parser_calloc
#define YYFREE 	rb_parser_free
#define malloc	YYMALLOC
#define realloc	YYREALLOC
#define calloc	YYCALLOC
#define free	YYFREE
static void *rb_parser_malloc _((size_t));
static void *rb_parser_realloc _((void *, size_t));
static void *rb_parser_calloc _((size_t, size_t));
static void rb_parser_free _((void *));

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
    EXPR_END,			/* newline significant, +/- is an operator. */
    EXPR_ARG,			/* newline significant, +/- is an operator. */
    EXPR_CMDARG,		/* newline significant, +/- is an operator. */
    EXPR_ENDARG,		/* newline significant, +/- is an operator. */
    EXPR_MID,			/* newline significant, +/- is an operator. */
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
static int command_start = Qtrue;

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

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 209 "parse.y"
{
    NODE *node;
    ID id;
    int num;
    struct RVarmap *vars;
}
/* Line 187 of yacc.c.  */
#line 507 "parse.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 520 "parse.c"

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
#define YYLAST   9550

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  132
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  144
/* YYNRULES -- Number of rules.  */
#define YYNRULES  501
/* YYNRULES -- Number of states.  */
#define YYNSTATES  895

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   359

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
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
static const yytype_uint16 yyprhs[] =
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
     340,   342,   344,   346,   348,   350,   352,   354,   355,   360,
     362,   364,   366,   368,   370,   372,   374,   376,   378,   380,
     382,   384,   386,   388,   390,   392,   394,   396,   398,   400,
     402,   404,   406,   408,   410,   412,   414,   416,   418,   420,
     422,   424,   426,   428,   430,   432,   434,   436,   438,   440,
     442,   444,   446,   448,   450,   452,   454,   456,   458,   460,
     462,   464,   466,   468,   470,   472,   474,   476,   478,   480,
     482,   484,   486,   488,   490,   492,   496,   502,   506,   513,
     519,   525,   531,   537,   542,   546,   550,   554,   558,   562,
     566,   570,   574,   578,   583,   588,   591,   594,   598,   602,
     606,   610,   614,   618,   622,   626,   630,   634,   638,   642,
     646,   649,   652,   656,   660,   664,   668,   669,   674,   680,
     682,   684,   686,   689,   692,   698,   701,   705,   709,   714,
     719,   726,   728,   730,   732,   735,   741,   744,   750,   755,
     763,   767,   769,   774,   778,   784,   792,   795,   801,   806,
     813,   821,   831,   835,   837,   838,   841,   843,   844,   848,
     849,   854,   857,   860,   862,   864,   868,   872,   877,   880,
     882,   884,   886,   888,   890,   892,   894,   896,   898,   899,
     904,   905,   911,   915,   919,   922,   927,   931,   935,   937,
     942,   946,   948,   949,   956,   959,   961,   964,   971,   978,
     979,   980,   988,   989,   990,   998,  1004,  1009,  1015,  1016,
    1017,  1027,  1028,  1035,  1036,  1037,  1046,  1047,  1053,  1054,
    1061,  1062,  1063,  1073,  1075,  1077,  1079,  1081,  1083,  1085,
    1087,  1089,  1092,  1094,  1096,  1098,  1100,  1106,  1108,  1111,
    1113,  1115,  1117,  1120,  1122,  1126,  1127,  1128,  1135,  1138,
    1143,  1148,  1151,  1156,  1161,  1165,  1168,  1170,  1171,  1172,
    1179,  1180,  1181,  1188,  1194,  1196,  1201,  1204,  1206,  1208,
    1215,  1217,  1219,  1221,  1223,  1226,  1228,  1231,  1233,  1235,
    1237,  1239,  1241,  1243,  1246,  1250,  1254,  1258,  1262,  1266,
    1267,  1271,  1273,  1276,  1280,  1284,  1285,  1289,  1290,  1293,
    1294,  1297,  1299,  1300,  1304,  1305,  1310,  1312,  1314,  1316,
    1318,  1321,  1323,  1325,  1327,  1329,  1333,  1335,  1337,  1340,
    1343,  1345,  1347,  1349,  1351,  1353,  1355,  1357,  1359,  1361,
    1363,  1365,  1367,  1369,  1371,  1373,  1375,  1376,  1381,  1384,
    1389,  1392,  1399,  1404,  1409,  1412,  1417,  1420,  1423,  1425,
    1426,  1428,  1430,  1432,  1434,  1436,  1438,  1442,  1446,  1448,
    1452,  1454,  1456,  1459,  1461,  1463,  1465,  1468,  1471,  1473,
    1475,  1476,  1482,  1484,  1487,  1490,  1492,  1496,  1500,  1502,
    1504,  1506,  1508,  1510,  1512,  1514,  1516,  1518,  1520,  1522,
    1524,  1525,  1527,  1528,  1530,  1531,  1533,  1535,  1537,  1539,
    1541,  1544
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     133,     0,    -1,    -1,   134,   136,    -1,   136,   219,   203,
     222,    -1,   137,   270,    -1,   275,    -1,   138,    -1,   137,
     274,   138,    -1,     1,   138,    -1,    -1,    44,   160,   139,
     160,    -1,    44,    52,    52,    -1,    44,    52,    60,    -1,
      44,    52,    59,    -1,     6,   161,    -1,   138,    39,   142,
      -1,   138,    40,   142,    -1,   138,    41,   142,    -1,   138,
      42,   142,    -1,   138,    43,   138,    -1,    -1,    46,   140,
     120,   136,   121,    -1,    47,   120,   136,   121,    -1,   155,
     103,   143,    -1,   149,   103,   143,    -1,   246,    83,   143,
      -1,   199,   122,   168,   123,    83,   143,    -1,   199,   124,
      50,    83,   143,    -1,   199,   124,    54,    83,   143,    -1,
     199,    81,    50,    83,   143,    -1,   247,    83,   143,    -1,
     155,   103,   181,    -1,   149,   103,   167,    -1,   149,   103,
     181,    -1,   141,    -1,   143,    -1,   141,    36,   141,    -1,
     141,    37,   141,    -1,    38,   141,    -1,   117,   143,    -1,
     165,    -1,   141,    -1,   148,    -1,   144,    -1,    29,   171,
      -1,    21,   171,    -1,    22,   171,    -1,   209,    -1,   209,
     124,   267,   173,    -1,   209,    81,   267,   173,    -1,    -1,
      -1,    90,   146,   205,   147,   136,   121,    -1,   266,   173,
      -1,   266,   173,   145,    -1,   199,   124,   267,   173,    -1,
     199,   124,   267,   173,   145,    -1,   199,    81,   267,   173,
      -1,   199,    81,   267,   173,   145,    -1,    31,   173,    -1,
      30,   173,    -1,   151,    -1,    85,   150,   125,    -1,   151,
      -1,    85,   150,   125,    -1,   153,    -1,   153,   152,    -1,
     153,    91,   154,    -1,   153,    91,    -1,    91,   154,    -1,
      91,    -1,   154,    -1,    85,   150,   125,    -1,   152,   126,
      -1,   153,   152,   126,    -1,   244,    -1,   199,   122,   168,
     123,    -1,   199,   124,    50,    -1,   199,    81,    50,    -1,
     199,   124,    54,    -1,   199,    81,    54,    -1,    82,    54,
      -1,   247,    -1,   244,    -1,   199,   122,   168,   123,    -1,
     199,   124,    50,    -1,   199,    81,    50,    -1,   199,   124,
      54,    -1,   199,    81,    54,    -1,    82,    54,    -1,   247,
      -1,    50,    -1,    54,    -1,    82,   156,    -1,   156,    -1,
     199,    81,   156,    -1,    50,    -1,    54,    -1,    51,    -1,
     163,    -1,   164,    -1,   158,    -1,   240,    -1,   159,    -1,
     242,    -1,   160,    -1,    -1,   161,   126,   162,   160,    -1,
     108,    -1,   109,    -1,   110,    -1,    65,    -1,    66,    -1,
      67,    -1,    73,    -1,   106,    -1,    69,    -1,   107,    -1,
      70,    -1,    79,    -1,    80,    -1,   111,    -1,   112,    -1,
     113,    -1,    91,    -1,   114,    -1,   115,    -1,    64,    -1,
     118,    -1,    62,    -1,    63,    -1,    77,    -1,    78,    -1,
     127,    -1,    48,    -1,    49,    -1,    46,    -1,    47,    -1,
      44,    -1,    36,    -1,     7,    -1,    21,    -1,    16,    -1,
       3,    -1,     5,    -1,    45,    -1,    26,    -1,    15,    -1,
      14,    -1,    10,    -1,     9,    -1,    35,    -1,    20,    -1,
      25,    -1,     4,    -1,    22,    -1,    33,    -1,    38,    -1,
      37,    -1,    23,    -1,     8,    -1,    24,    -1,    29,    -1,
      32,    -1,    31,    -1,    13,    -1,    34,    -1,     6,    -1,
      17,    -1,    30,    -1,    11,    -1,    12,    -1,    18,    -1,
      19,    -1,   155,   103,   165,    -1,   155,   103,   165,    43,
     165,    -1,   246,    83,   165,    -1,   199,   122,   168,   123,
      83,   165,    -1,   199,   124,    50,    83,   165,    -1,   199,
     124,    54,    83,   165,    -1,   199,    81,    50,    83,   165,
      -1,   199,    81,    54,    83,   165,    -1,    82,    54,    83,
     165,    -1,   247,    83,   165,    -1,   165,    75,   165,    -1,
     165,    76,   165,    -1,   165,   111,   165,    -1,   165,   112,
     165,    -1,   165,   113,   165,    -1,   165,   114,   165,    -1,
     165,   115,   165,    -1,   165,    64,   165,    -1,   116,    56,
      64,   165,    -1,   116,    57,    64,   165,    -1,    62,   165,
      -1,    63,   165,    -1,   165,   108,   165,    -1,   165,   109,
     165,    -1,   165,   110,   165,    -1,   165,    65,   165,    -1,
     165,   106,   165,    -1,   165,    69,   165,    -1,   165,   107,
     165,    -1,   165,    70,   165,    -1,   165,    66,   165,    -1,
     165,    67,   165,    -1,   165,    68,   165,    -1,   165,    73,
     165,    -1,   165,    74,   165,    -1,   117,   165,    -1,   118,
     165,    -1,   165,    79,   165,    -1,   165,    80,   165,    -1,
     165,    71,   165,    -1,   165,    72,   165,    -1,    -1,    45,
     271,   166,   165,    -1,   165,   104,   165,   105,   165,    -1,
     182,    -1,   165,    -1,   275,    -1,   148,   271,    -1,   180,
     272,    -1,   180,   126,    91,   165,   271,    -1,   264,   272,
      -1,    91,   165,   271,    -1,   128,   275,   125,    -1,   128,
     171,   271,   125,    -1,   128,   209,   271,   125,    -1,   128,
     180,   126,   209,   271,   125,    -1,   275,    -1,   169,    -1,
     148,    -1,   180,   179,    -1,   180,   126,    91,   167,   179,
      -1,   264,   179,    -1,   264,   126,    91,   167,   179,    -1,
     180,   126,   264,   179,    -1,   180,   126,   264,   126,    91,
     165,   179,    -1,    91,   167,   179,    -1,   178,    -1,   167,
     126,   180,   179,    -1,   167,   126,   178,    -1,   167,   126,
      91,   167,   179,    -1,   167,   126,   180,   126,    91,   167,
     179,    -1,   264,   179,    -1,   264,   126,    91,   167,   179,
      -1,   167,   126,   264,   179,    -1,   167,   126,   180,   126,
     264,   179,    -1,   167,   126,   264,   126,    91,   167,   179,
      -1,   167,   126,   180,   126,   264,   126,    91,   167,   179,
      -1,    91,   167,   179,    -1,   178,    -1,    -1,   174,   175,
      -1,   171,    -1,    -1,    86,   176,   125,    -1,    -1,    86,
     172,   177,   125,    -1,    92,   167,    -1,   126,   178,    -1,
     275,    -1,   167,    -1,   180,   126,   167,    -1,   180,   126,
     167,    -1,   180,   126,    91,   167,    -1,    91,   167,    -1,
     223,    -1,   224,    -1,   227,    -1,   228,    -1,   229,    -1,
     232,    -1,   245,    -1,   247,    -1,    51,    -1,    -1,     7,
     183,   135,    10,    -1,    -1,    86,   141,   184,   271,   125,
      -1,    85,   136,   125,    -1,   199,    81,    54,    -1,    82,
      54,    -1,   199,   122,   168,   123,    -1,    88,   168,   123,
      -1,    89,   263,   121,    -1,    29,    -1,    30,   128,   171,
     125,    -1,    30,   128,   125,    -1,    30,    -1,    -1,    45,
     271,   128,   185,   141,   125,    -1,   266,   211,    -1,   210,
      -1,   210,   211,    -1,    11,   142,   200,   136,   202,    10,
      -1,    12,   142,   200,   136,   203,    10,    -1,    -1,    -1,
      18,   186,   142,   201,   187,   136,    10,    -1,    -1,    -1,
      19,   188,   142,   201,   189,   136,    10,    -1,    16,   142,
     270,   216,    10,    -1,    16,   270,   216,    10,    -1,    16,
     270,    15,   136,    10,    -1,    -1,    -1,    20,   204,    25,
     190,   142,   201,   191,   136,    10,    -1,    -1,     3,   157,
     248,   192,   135,    10,    -1,    -1,    -1,     3,    79,   141,
     193,   273,   194,   135,    10,    -1,    -1,     4,   157,   195,
     135,    10,    -1,    -1,     5,   158,   196,   250,   135,    10,
      -1,    -1,    -1,     5,   261,   269,   197,   158,   198,   250,
     135,    10,    -1,    21,    -1,    22,    -1,    23,    -1,    24,
      -1,   182,    -1,   273,    -1,   105,    -1,    13,    -1,   273,
      13,    -1,   273,    -1,   105,    -1,    27,    -1,   203,    -1,
      14,   142,   200,   136,   202,    -1,   275,    -1,    15,   136,
      -1,   155,    -1,   149,    -1,   275,    -1,   108,   108,    -1,
      72,    -1,   108,   204,   108,    -1,    -1,    -1,    28,   207,
     205,   208,   136,    10,    -1,   148,   206,    -1,   209,   124,
     267,   170,    -1,   209,    81,   267,   170,    -1,   266,   169,
      -1,   199,   124,   267,   170,    -1,   199,    81,   267,   169,
      -1,   199,    81,   268,    -1,    31,   169,    -1,    31,    -1,
      -1,    -1,   120,   212,   205,   213,   136,   121,    -1,    -1,
      -1,    26,   214,   205,   215,   136,    10,    -1,    17,   217,
     200,   136,   218,    -1,   180,    -1,   180,   126,    91,   167,
      -1,    91,   167,    -1,   203,    -1,   216,    -1,     8,   220,
     221,   200,   136,   219,    -1,   275,    -1,   167,    -1,   181,
      -1,   275,    -1,    84,   155,    -1,   275,    -1,     9,   136,
      -1,   275,    -1,   243,    -1,   240,    -1,   242,    -1,   225,
      -1,   226,    -1,   225,   226,    -1,    94,   234,   101,    -1,
      95,   235,   101,    -1,    96,   235,    61,    -1,    97,   129,
     101,    -1,    97,   230,   101,    -1,    -1,   230,   231,   129,
      -1,   236,    -1,   231,   236,    -1,    98,   129,   101,    -1,
      98,   233,   101,    -1,    -1,   233,    58,   129,    -1,    -1,
     234,   236,    -1,    -1,   235,   236,    -1,    58,    -1,    -1,
     100,   237,   239,    -1,    -1,    99,   238,   136,   121,    -1,
      52,    -1,    53,    -1,    55,    -1,   247,    -1,    93,   241,
      -1,   158,    -1,    53,    -1,    52,    -1,    55,    -1,    93,
     235,   101,    -1,    56,    -1,    57,    -1,   116,    56,    -1,
     116,    57,    -1,    50,    -1,    53,    -1,    52,    -1,    54,
      -1,    55,    -1,    33,    -1,    32,    -1,    34,    -1,    35,
      -1,    49,    -1,    48,    -1,   244,    -1,   244,    -1,    59,
      -1,    60,    -1,   273,    -1,    -1,   107,   249,   142,   273,
      -1,     1,   273,    -1,   128,   251,   271,   125,    -1,   251,
     273,    -1,   253,   126,   255,   126,   257,   260,    -1,   253,
     126,   255,   260,    -1,   253,   126,   257,   260,    -1,   253,
     260,    -1,   255,   126,   257,   260,    -1,   255,   260,    -1,
     257,   260,    -1,   259,    -1,    -1,    54,    -1,    53,    -1,
      52,    -1,    55,    -1,    50,    -1,   252,    -1,   253,   126,
     252,    -1,    50,   103,   167,    -1,   254,    -1,   255,   126,
     254,    -1,   113,    -1,    91,    -1,   256,    50,    -1,   256,
      -1,   110,    -1,    92,    -1,   258,    50,    -1,   126,   259,
      -1,   275,    -1,   245,    -1,    -1,   128,   262,   141,   271,
     125,    -1,   275,    -1,   264,   272,    -1,   180,   272,    -1,
     265,    -1,   264,   126,   265,    -1,   167,    84,   167,    -1,
      50,    -1,    54,    -1,    51,    -1,    50,    -1,    54,    -1,
      51,    -1,   163,    -1,    50,    -1,    51,    -1,   163,    -1,
     124,    -1,    81,    -1,    -1,   274,    -1,    -1,   130,    -1,
      -1,   130,    -1,   126,    -1,   131,    -1,   130,    -1,   273,
      -1,   274,   131,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   346,   346,   346,   371,   391,   398,   399,   403,   407,
     413,   413,   417,   421,   428,   433,   437,   446,   455,   467,
     479,   485,   484,   498,   506,   510,   516,   541,   557,   569,
     581,   593,   598,   602,   607,   612,   615,   616,   620,   624,
     628,   632,   635,   642,   643,   644,   648,   652,   658,   659,
     663,   670,   674,   669,   684,   689,   701,   706,   718,   723,
     735,   740,   747,   748,   754,   755,   761,   765,   769,   773,
     777,   781,   787,   788,   794,   798,   804,   808,   812,   816,
     820,   824,   830,   836,   843,   847,   851,   855,   859,   863,
     869,   875,   882,   886,   889,   893,   897,   903,   904,   905,
     906,   911,   918,   919,   922,   926,   929,   933,   933,   939,
     940,   941,   942,   943,   944,   945,   946,   947,   948,   949,
     950,   951,   952,   953,   954,   955,   956,   957,   958,   959,
     960,   961,   962,   963,   964,   967,   967,   967,   967,   968,
     968,   968,   968,   968,   968,   968,   969,   969,   969,   969,
     969,   969,   969,   970,   970,   970,   970,   970,   970,   971,
     971,   971,   971,   971,   971,   971,   972,   972,   972,   972,
     972,   973,   973,   973,   973,   976,   980,   984,  1009,  1025,
    1037,  1049,  1061,  1066,  1071,  1076,  1089,  1102,  1106,  1110,
    1114,  1118,  1122,  1126,  1130,  1134,  1143,  1147,  1151,  1155,
    1159,  1163,  1167,  1171,  1175,  1179,  1183,  1187,  1191,  1195,
    1199,  1203,  1207,  1211,  1215,  1219,  1223,  1223,  1228,  1233,
    1239,  1246,  1247,  1252,  1256,  1261,  1265,  1272,  1276,  1280,
    1285,  1292,  1293,  1296,  1301,  1305,  1310,  1315,  1320,  1325,
    1331,  1335,  1338,  1342,  1346,  1351,  1356,  1361,  1366,  1371,
    1376,  1381,  1386,  1390,  1393,  1393,  1405,  1406,  1406,  1411,
    1411,  1418,  1424,  1428,  1431,  1435,  1441,  1445,  1449,  1455,
    1456,  1457,  1458,  1459,  1460,  1461,  1462,  1463,  1468,  1467,
    1480,  1480,  1485,  1490,  1494,  1498,  1506,  1515,  1519,  1523,
    1527,  1531,  1535,  1535,  1540,  1546,  1547,  1556,  1569,  1582,
    1582,  1582,  1592,  1592,  1592,  1602,  1609,  1613,  1617,  1617,
    1617,  1625,  1624,  1641,  1646,  1640,  1663,  1662,  1679,  1678,
    1696,  1697,  1696,  1711,  1715,  1719,  1723,  1729,  1736,  1737,
    1738,  1739,  1742,  1743,  1744,  1747,  1748,  1757,  1758,  1764,
    1765,  1768,  1769,  1773,  1777,  1784,  1788,  1783,  1798,  1807,
    1811,  1817,  1822,  1827,  1832,  1836,  1840,  1847,  1851,  1846,
    1859,  1863,  1858,  1872,  1879,  1880,  1884,  1890,  1891,  1894,
    1905,  1908,  1912,  1913,  1916,  1920,  1923,  1931,  1934,  1935,
    1939,  1942,  1955,  1956,  1962,  1968,  1991,  2024,  2028,  2035,
    2038,  2044,  2045,  2051,  2055,  2062,  2065,  2072,  2075,  2082,
    2085,  2091,  2093,  2092,  2104,  2103,  2124,  2125,  2126,  2127,
    2130,  2137,  2138,  2139,  2140,  2143,  2177,  2178,  2179,  2183,
    2189,  2190,  2191,  2192,  2193,  2194,  2195,  2196,  2197,  2198,
    2199,  2202,  2208,  2214,  2215,  2218,  2223,  2222,  2230,  2233,
    2239,  2245,  2249,  2253,  2257,  2261,  2265,  2269,  2273,  2278,
    2283,  2287,  2291,  2295,  2299,  2310,  2311,  2317,  2327,  2332,
    2338,  2339,  2342,  2353,  2364,  2365,  2368,  2378,  2382,  2385,
    2390,  2390,  2415,  2416,  2420,  2429,  2430,  2436,  2442,  2443,
    2444,  2447,  2448,  2449,  2450,  2453,  2454,  2455,  2458,  2459,
    2462,  2463,  2466,  2467,  2470,  2471,  2472,  2475,  2476,  2479,
    2480,  2483
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
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
  "mlhs_head", "mlhs_node", "lhs", "cname", "cpath", "fname", "fsym",
  "fitem", "undef_list", "@6", "op", "reswords", "arg", "@7", "arg_value",
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
static const yytype_uint16 yytoknum[] =
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
static const yytype_uint16 yyr1[] =
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
     158,   158,   159,   159,   160,   160,   161,   162,   161,   163,
     163,   163,   163,   163,   163,   163,   163,   163,   163,   163,
     163,   163,   163,   163,   163,   163,   163,   163,   163,   163,
     163,   163,   163,   163,   163,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   166,   165,   165,   165,
     167,   168,   168,   168,   168,   168,   168,   169,   169,   169,
     169,   170,   170,   171,   171,   171,   171,   171,   171,   171,
     171,   171,   172,   172,   172,   172,   172,   172,   172,   172,
     172,   172,   172,   172,   174,   173,   175,   176,   175,   177,
     175,   178,   179,   179,   180,   180,   181,   181,   181,   182,
     182,   182,   182,   182,   182,   182,   182,   182,   183,   182,
     184,   182,   182,   182,   182,   182,   182,   182,   182,   182,
     182,   182,   185,   182,   182,   182,   182,   182,   182,   186,
     187,   182,   188,   189,   182,   182,   182,   182,   190,   191,
     182,   192,   182,   193,   194,   182,   195,   182,   196,   182,
     197,   198,   182,   182,   182,   182,   182,   199,   200,   200,
     200,   200,   201,   201,   201,   202,   202,   203,   203,   204,
     204,   205,   205,   205,   205,   207,   208,   206,   209,   209,
     209,   210,   210,   210,   210,   210,   210,   212,   213,   211,
     214,   215,   211,   216,   217,   217,   217,   218,   218,   219,
     219,   220,   220,   220,   221,   221,   222,   222,   223,   223,
     223,   224,   225,   225,   226,   227,   228,   229,   229,   230,
     230,   231,   231,   232,   232,   233,   233,   234,   234,   235,
     235,   236,   237,   236,   238,   236,   239,   239,   239,   239,
     240,   241,   241,   241,   241,   242,   243,   243,   243,   243,
     244,   244,   244,   244,   244,   244,   244,   244,   244,   244,
     244,   245,   246,   247,   247,   248,   249,   248,   248,   250,
     250,   251,   251,   251,   251,   251,   251,   251,   251,   251,
     252,   252,   252,   252,   252,   253,   253,   254,   255,   255,
     256,   256,   257,   257,   258,   258,   259,   260,   260,   261,
     262,   261,   263,   263,   263,   264,   264,   265,   266,   266,
     266,   267,   267,   267,   267,   268,   268,   268,   269,   269,
     270,   270,   271,   271,   272,   272,   272,   273,   273,   274,
     274,   275
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
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
       1,     1,     1,     1,     1,     1,     1,     0,     4,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     3,     5,     3,     6,     5,
       5,     5,     5,     4,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     4,     4,     2,     2,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       2,     2,     3,     3,     3,     3,     0,     4,     5,     1,
       1,     1,     2,     2,     5,     2,     3,     3,     4,     4,
       6,     1,     1,     1,     2,     5,     2,     5,     4,     7,
       3,     1,     4,     3,     5,     7,     2,     5,     4,     6,
       7,     9,     3,     1,     0,     2,     1,     0,     3,     0,
       4,     2,     2,     1,     1,     3,     3,     4,     2,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     0,     4,
       0,     5,     3,     3,     2,     4,     3,     3,     1,     4,
       3,     1,     0,     6,     2,     1,     2,     6,     6,     0,
       0,     7,     0,     0,     7,     5,     4,     5,     0,     0,
       9,     0,     6,     0,     0,     8,     0,     5,     0,     6,
       0,     0,     9,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     1,     1,     1,     1,     5,     1,     2,     1,
       1,     1,     2,     1,     3,     0,     0,     6,     2,     4,
       4,     2,     4,     4,     3,     2,     1,     0,     0,     6,
       0,     0,     6,     5,     1,     4,     2,     1,     1,     6,
       1,     1,     1,     1,     2,     1,     2,     1,     1,     1,
       1,     1,     1,     2,     3,     3,     3,     3,     3,     0,
       3,     1,     2,     3,     3,     0,     3,     0,     2,     0,
       2,     1,     0,     3,     0,     4,     1,     1,     1,     1,
       2,     1,     1,     1,     1,     3,     1,     1,     2,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     0,     4,     2,     4,
       2,     6,     4,     4,     2,     4,     2,     2,     1,     0,
       1,     1,     1,     1,     1,     1,     3,     3,     1,     3,
       1,     1,     2,     1,     1,     1,     2,     2,     1,     1,
       0,     5,     1,     2,     2,     1,     3,     3,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       0,     1,     0,     1,     0,     1,     1,     1,     1,     1,
       2,     0
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,     0,     0,     0,     0,     0,   278,
       0,     0,   490,   299,   302,     0,   323,   324,   325,   326,
     288,   291,   356,   426,   425,   427,   428,     0,     0,   492,
      21,     0,   430,   429,   420,   277,   422,   421,   423,   424,
     416,   417,   433,   434,     0,     0,     0,     0,     0,   501,
     501,    71,   399,   397,   399,   399,   389,   395,     0,     0,
       0,     3,   490,     7,    35,    36,    44,    43,     0,    62,
       0,    66,    72,     0,    41,   219,     0,    48,   295,   269,
     270,   381,   382,   271,   272,   273,   274,   379,   380,   378,
     431,   275,     0,   276,   254,     6,     9,   323,   324,   288,
     291,   356,   492,    92,    93,     0,     0,     0,     0,    95,
       0,   327,     0,   431,   276,     0,   316,   144,   155,   145,
     168,   141,   161,   151,   150,   171,   172,   166,   149,   148,
     143,   169,   173,   174,   153,   142,   156,   160,   162,   154,
     147,   163,   170,   165,   164,   157,   167,   152,   140,   159,
     158,   139,   146,   137,   138,   135,   136,    97,    99,    98,
     130,   131,   128,   112,   113,   114,   117,   119,   115,   132,
     133,   120,   121,   125,   116,   118,   109,   110,   111,   122,
     123,   124,   126,   127,   129,   134,   470,   318,   100,   101,
     469,     0,   164,   157,   167,   152,   135,   136,    97,    98,
     102,   104,   106,    15,   103,   105,     0,     0,    42,     0,
       0,     0,   431,     0,   276,     0,   498,   497,   490,     0,
     499,   491,     0,     0,     0,   340,   339,     0,     0,   431,
     276,     0,     0,     0,   233,   220,   264,    46,   241,   501,
     501,   475,    47,    45,     0,    61,     0,   501,   355,    60,
      39,     0,    10,   493,   216,     0,     0,   195,     0,   196,
     284,     0,     0,     0,    62,   280,     0,   492,     0,   494,
     494,   221,   494,     0,   494,   472,     0,    70,     0,    76,
      83,   413,   412,   414,   411,     0,   410,     0,     0,     0,
       0,     0,     0,     0,   418,   419,    40,   210,   211,     5,
     491,     0,     0,     0,     0,     0,     0,     0,   345,   348,
       0,    74,     0,    69,    67,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   501,     0,     0,     0,   360,   357,   296,
     383,     0,     0,   351,    54,   294,     0,   313,    92,    93,
      94,   418,   419,     0,   436,   311,   435,     0,   501,     0,
       0,     0,   449,   489,   488,   320,   107,     0,   501,   284,
     330,   329,     0,   328,     0,     0,   501,     0,     0,     0,
       0,     0,     0,     0,     0,   500,     0,     0,   284,     0,
     501,     0,   308,   501,   261,     0,     0,   234,   263,     0,
     236,   290,     0,   257,   256,   255,   233,   492,   501,   492,
       0,    12,    14,    13,     0,   292,     0,     0,     0,     0,
       0,     0,     0,   282,    63,   492,   492,   222,   286,   496,
     495,   223,   496,   225,   496,   474,   287,   473,    82,     0,
     501,     0,   401,   404,   402,   415,   400,   384,   398,   385,
     386,   387,   388,     0,   391,   393,     0,   394,     0,     0,
       8,    16,    17,    18,    19,    20,    37,    38,   501,     0,
      25,    33,     0,    34,     0,    68,    75,    24,   175,   264,
      32,   192,   200,   205,   206,   207,   202,   204,   214,   215,
     208,   209,   185,   186,   212,   213,     0,   201,   203,   197,
     198,   199,   187,   188,   189,   190,   191,   481,   486,   482,
     487,   254,   354,     0,   481,   483,   482,   484,   501,   481,
     482,   254,   254,   501,   501,    26,   177,    31,   184,    51,
      55,     0,   438,     0,     0,    92,    93,    96,     0,     0,
     501,     0,   492,   454,   452,   451,   450,   453,   461,   465,
     464,   460,   449,     0,     0,   455,   501,   458,   501,   463,
     501,     0,   448,     0,     0,   279,   501,   501,   370,   501,
     331,   175,   485,   283,     0,   481,   482,   501,     0,     0,
       0,   364,     0,   306,   334,   333,   300,   332,   303,   485,
     283,     0,   481,   482,     0,     0,   240,   477,     0,   265,
     262,   501,     0,     0,   476,   289,     0,    41,     0,   259,
       0,   253,   501,     0,     0,     0,     0,     0,   227,    11,
       0,   217,     0,    23,   183,    63,     0,   226,     0,   265,
      79,    81,     0,   481,   482,     0,     0,   390,   392,   396,
     193,   194,   343,     0,   346,   341,   268,     0,    73,     0,
       0,     0,     0,   353,    58,   285,     0,     0,   232,   352,
      56,   231,   350,    50,   349,    49,   361,   358,   501,   314,
       0,     0,   285,   317,     0,     0,   492,     0,   440,     0,
     444,   468,     0,   446,   462,     0,   447,   466,   321,   108,
     371,   372,   501,   373,     0,   501,   337,     0,     0,   335,
       0,   285,     0,     0,     0,   305,   307,   366,     0,     0,
       0,     0,   285,     0,   501,     0,   238,   501,   501,     0,
       0,   258,     0,   246,   228,     0,   492,   501,   501,   229,
       0,    22,   281,   492,    77,     0,   406,   407,   408,   403,
     409,   342,     0,     0,     0,   266,   176,   218,    30,   181,
     182,    59,     0,    28,   179,    29,   180,    57,     0,     0,
      52,     0,   437,   312,   471,   457,     0,   319,   456,   501,
     501,   467,     0,   459,   501,   449,     0,     0,   375,   338,
       0,     4,   377,     0,   297,     0,   298,     0,   501,     0,
       0,   309,   235,     0,   237,   252,     0,   243,   501,   501,
     260,     0,     0,   293,   224,   405,   344,     0,   267,    27,
     178,     0,     0,     0,     0,   439,     0,   442,   443,   445,
       0,     0,   374,     0,    84,    91,     0,   376,     0,   365,
     367,   368,   363,   301,   304,     0,   501,   501,     0,   242,
       0,   248,   501,   230,   347,   362,   359,     0,   315,   501,
       0,    90,     0,   501,     0,   501,   501,     0,   239,   244,
       0,   501,     0,   247,    53,   441,   322,   485,    89,     0,
     481,   482,   369,   336,   310,   501,     0,   249,   501,    85,
     245,     0,   250,   501,   251
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,   377,   378,    62,    63,   424,   255,    64,
     209,    65,    66,   540,   678,   823,    67,    68,   263,    69,
      70,    71,    72,   210,   109,   110,   200,   201,   202,   203,
     574,   527,   189,    74,   426,   236,   268,   668,   669,   237,
     619,   245,   246,   415,   620,   730,   610,   407,   269,   483,
      75,   206,   435,   630,   222,   720,   223,   721,   604,   845,
     544,   541,   771,   370,   372,   573,   785,   258,   382,   596,
     708,   709,   228,   654,   309,   478,   753,    77,    78,   355,
     534,   769,   533,   768,   394,   592,   842,   577,   702,   787,
     791,    79,    80,    81,    82,    83,    84,    85,   291,   463,
      86,   293,   287,   285,   456,   646,   645,   749,    87,   286,
      88,    89,   212,    91,   213,   214,   365,   543,   563,   564,
     565,   566,   567,   568,   569,   570,   571,   781,   690,   191,
     371,   273,   270,   241,   115,   548,   522,   375,   219,   254,
     441,   383,   221,    95
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -654
static const yytype_int16 yypact[] =
{
    -654,    73,  2254,  -654,  5471,  7939,  8236,  3906,  5121,  -654,
    6810,  6810,  3777,  -654,  -654,  8038,  5677,  5677,  -654,  -654,
    5677,  4436,  4539,  -654,  -654,  -654,  -654,  6810,  4996,   -44,
    -654,    -3,  -654,  -654,  2117,  4024,  -654,  -654,  4127,  -654,
    -654,  -654,  -654,  -654,  7634,  7634,    50,  3187,  6810,  6913,
    7634,  8335,  4871,  -654,  -654,  -654,   -18,    -1,    94,  7737,
    7634,  -654,   124,   757,   249,  -654,  -654,   104,    74,  -654,
      57,  8137,  -654,    96,  9349,   171,   485,    11,    32,  -654,
    -654,   120,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,
      10,  -654,    64,    38,    42,  -654,   757,  -654,  -654,  -654,
      99,   111,   -44,   129,   140,  6810,    90,  3318,   325,  -654,
      78,  -654,   549,  -654,  -654,    42,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,  -654,    95,   148,   282,   333,  -654,  -654,
    -654,  -654,  -654,  -654,  -654,   336,   337,   465,  -654,   473,
    -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,   495,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,   147,  -654,  -654,  2615,   225,   249,    48,
     188,   558,    31,   213,   258,    48,  -654,  -654,   124,    88,
    -654,   209,  6810,  6810,   288,  -654,  -654,   559,   324,    66,
     245,  7634,  7634,  7634,  -654,  9349,   274,  -654,  -654,   262,
     269,  -654,  -654,  -654,  5363,  -654,  5780,  5677,  -654,  -654,
    -654,   222,  -654,  -654,   275,   285,  3449,  -654,   571,   366,
     476,  3187,   310,   315,   342,   249,  7634,   -44,   371,   392,
     401,  -654,   480,   375,   401,  -654,   470,  -654,   586,   595,
     618,  -654,  -654,  -654,  -654,   231,  -654,   482,   515,   674,
     425,   565,   444,    30,   493,   499,  -654,  -654,  -654,  -654,
    3674,  6810,  6810,  6810,  6810,  5471,  6810,  6810,  -654,  -654,
    7016,  -654,  3187,  8335,   438,  7016,  7634,  7634,  7634,  7634,
    7634,  7634,  7634,  7634,  7634,  7634,  7634,  7634,  7634,  7634,
    7634,  7634,  7634,  7634,  7634,  7634,  7634,  7634,  7634,  7634,
    7634,  7634,  1612,  6913,  8489,  8555,  8555,  -654,  -654,  -654,
    -654,  7737,  7737,  -654,   501,  -654,   275,   249,  -654,   623,
    -654,  -654,  -654,   124,  -654,  -654,  -654,  8621,  6913,  8555,
    2615,  6810,   419,  -654,  -654,  -654,  -654,   583,   596,   283,
    -654,  -654,  2737,   590,  7634,  8687,  6913,  8753,  7634,  7634,
    2981,   601,  3571,  7119,   611,  -654,    18,    18,   252,  8819,
    6913,  8885,  -654,   498,  -654,  7634,  5883,  -654,  -654,  5986,
    -654,  -654,   503,  5574,  -654,  -654,   104,   -44,   510,     9,
     512,  -654,  -654,  -654,  5121,  -654,  7634,  3449,   528,  8687,
    8753,  7634,   529,  -654,   527,   -44,  9213,  -654,  -654,  7222,
    -654,  -654,  7634,  -654,  7634,  -654,  -654,  -654,   623,  8951,
    6913,  9017,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,    25,  -654,  -654,   534,  -654,  7634,  7634,
     757,  -654,  -654,  -654,  -654,  -654,  -654,  -654,    37,  7634,
    -654,   544,   546,  -654,   550,  -654,  -654,  -654,  1775,  -654,
    -654,   366,  9418,  9418,  9418,  9418,   547,   547,  9435,  1667,
    9418,  9418,  9366,  9366,   394,   394,  2464,   547,   547,   714,
     714,   437,    43,    43,   366,   366,   366,  2365,  4642,  2484,
    4745,   111,  -654,   554,   487,  -654,   585,  -654,  4539,  -654,
    -654,  1899,  1899,    37,    37,  -654,  9349,  -654,  9349,  -654,
    -654,   124,  -654,  6810,  2615,   141,   477,  -654,   111,   562,
     111,   681,    35,   597,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,   429,  2615,   124,  -654,   575,  -654,   577,   655,
     588,   665,  -654,  5246,  5121,  -654,  7325,   703,  -654,   398,
    -654,  2345,  4230,  4333,   599,   344,   416,   703,   726,   734,
    7634,   626,    48,  -654,  -654,  -654,  -654,  -654,  -654,    60,
      85,   635,   259,   354,  6810,   657,  -654,  -654,  7634,   274,
    -654,   628,  7634,   274,  -654,  -654,  7634,  9265,    16,  -654,
     636,  -654,   634,   643,  6089,  8555,  8555,   645,  -654,  -654,
    6810,  9349,   656,  -654,  9349,   296,   658,  -654,  7634,  -654,
     141,   477,   661,   340,   351,  3449,   671,  -654,  -654,  -654,
     366,   366,  -654,  7840,  -654,  -654,  -654,  7428,  -654,  7634,
    7634,  7737,  7634,  -654,   501,   624,  7737,  7737,  -654,  -654,
     501,  -654,  -654,  -654,  -654,  -654,  -654,  -654,    37,  -654,
     124,   776,  -654,  -654,   663,  7634,   -44,   781,  -654,   429,
    -654,  -654,   452,  -654,  -654,   -12,  -654,  -654,  -654,  -654,
     544,  -654,   708,  -654,  3084,   792,  -654,  6810,   797,  -654,
    7634,   432,  7634,  7634,   798,  -654,  -654,  -654,  7531,  2859,
    3571,  3571,   367,    18,   498,  6192,  -654,   498,   498,  6295,
     685,  -654,  6398,  -654,  -654,   104,     9,   111,   111,  -654,
     101,  -654,  -654,  9213,   631,   690,  -654,  -654,  -654,  -654,
    -654,  -654,   709,  3571,  7634,   692,  9349,  9349,  -654,  9349,
    9349,  -654,  7737,  -654,  9349,  -654,  9349,  -654,  3571,  3449,
    -654,  2615,  -654,  -654,  -654,  -654,   691,  -654,  -654,   693,
     588,  -654,   597,  -654,   588,   419,  8434,    48,  -654,  -654,
    3571,  -654,  -654,    48,  -654,  7634,  -654,  7634,   165,   810,
     812,  -654,  -654,  7634,  -654,  -654,  7634,  -654,   705,   710,
    -654,  7634,   712,  -654,  -654,  -654,  -654,   825,  -654,  -654,
    9349,   830,   720,  3449,   832,  -654,   452,  -654,  -654,  -654,
    2615,   789,  -654,   640,   595,   618,  2615,  -654,  2737,  -654,
    -654,  -654,  -654,  -654,  -654,  3571,  9286,   498,  6501,  -654,
    6604,  -654,   498,  -654,  -654,  -654,  -654,   724,  -654,   588,
     838,   623,  9083,  6913,  9149,   596,   398,   839,  -654,  -654,
    7634,   727,  7634,  -654,  -654,  -654,  -654,    41,   477,   732,
      76,    93,  -654,  -654,  -654,   498,  6707,  -654,   498,   631,
    -654,  7634,  -654,   498,  -654
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -654,  -654,  -654,  -360,   431,  -654,    59,  -654,  -654,   958,
      44,    23,  -654,  -569,  -654,  -654,     1,    -6,  -183,    22,
     779,  -654,   -25,   927,   -70,   853,     7,  -654,    21,  -654,
    -654,    -5,  -654,   -16,  -654,  1378,  -338,    -7,  -479,    82,
    -654,     2,  -654,  -654,  -654,  -654,    -9,   158,   110,  -278,
      26,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,  -654,  -654,  -654,  -654,   185,  -209,  -372,
       0,  -520,   208,  -458,  -654,  -654,  -654,  -228,  -654,   785,
    -654,  -654,  -654,  -654,  -356,  -654,  -654,     3,  -654,  -654,
    -654,  -654,  -654,  -654,   784,  -654,  -654,  -654,  -654,  -654,
    -654,  -654,  -654,   458,  -217,  -654,  -654,  -654,    12,  -654,
      14,  -654,   627,   860,  1246,  1107,  -654,  -654,    84,   309,
     183,  -654,  -653,   184,  -654,  -608,  -654,  -321,  -504,  -654,
    -654,  -654,    -4,  -382,   755,  -226,  -654,  -654,   -24,    20,
     467,    53,   814,   756
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -502
static const yytype_int16 yytable[] =
{
     235,   235,   188,   188,   235,   523,   390,   238,   238,   225,
     551,   238,   240,   240,   187,   248,   240,   234,   234,   419,
     204,   234,   205,   188,   249,   598,   277,   614,   257,   259,
     549,   111,   111,   235,   235,   588,   360,   490,   299,   783,
     204,   111,   205,   297,   298,   594,   274,   188,   584,   252,
     267,   572,   672,   674,   -87,   215,   218,   705,   347,   284,
     614,   380,   601,    96,   693,   220,   696,   714,   347,   264,
     458,   306,   307,     3,   464,   676,   677,   111,   432,   363,
     559,   780,   296,   452,   784,   -87,   253,   353,   466,   -86,
     625,   -84,   345,  -432,   248,   761,   354,   111,   560,   242,
     405,   767,   243,   392,   260,   393,   -88,   316,   353,   652,
     -89,   290,   642,   -84,  -432,   220,   521,   256,   528,   531,
     532,   352,   356,   595,   453,   454,   239,   239,   292,   484,
     239,   467,   308,   626,   -84,   346,   -76,   306,   307,   253,
     358,   -91,   729,   550,   359,   653,   -87,   351,   216,   217,
     294,   295,   348,   381,   647,  -478,   339,   340,   341,   521,
     272,   528,   348,   366,   -83,   253,  -479,  -485,   -87,  -481,
     247,   -87,   -87,   783,   -84,   550,  -426,   310,   216,   217,
     704,   -86,   393,   311,   681,   364,   -79,    76,  -481,    76,
     112,   112,   -76,   -89,   391,   211,   211,   211,   -88,   315,
     227,   211,   211,   687,   550,   211,   -86,   -86,   216,   217,
    -420,   -81,   211,  -482,    53,   235,   235,   297,   859,  -426,
     770,  -423,  -485,   -88,   -88,   550,   813,   244,   235,  -425,
     235,   235,    76,   211,   211,   238,   278,   238,   238,   247,
     240,   572,   240,   240,   211,   234,   648,   234,   416,  -478,
     436,  -420,  -327,  -420,   216,   217,   278,  -478,   672,   674,
    -479,  -485,  -423,  -485,  -423,  -485,   396,   397,  -479,  -481,
     -91,   220,  -425,   376,   421,   827,   828,   -90,   840,   379,
     829,   422,   423,   264,   -86,   306,   307,   437,   485,   452,
     211,   384,    76,  -327,   235,  -327,   388,   547,   701,   488,
     491,   492,   493,   494,   495,   496,   497,   498,   499,   500,
     501,   502,   503,   504,   505,   506,   507,   508,   509,   510,
     511,   512,   513,   514,   515,   516,   412,   235,   414,   417,
     453,   454,   455,   480,   264,   536,   538,   520,   487,   111,
     395,   389,   398,   614,   267,   471,   472,   473,   474,   402,
     614,   801,   235,   -91,   239,   875,   239,   418,   405,   470,
     -90,   -91,   520,  -427,   475,   -78,   431,   -86,   581,   267,
     235,   -83,   536,   538,   535,   537,   -80,   235,   -82,   -88,
     520,   361,   362,   719,   235,   -78,   -90,   267,   406,   235,
     235,    76,   -85,   235,   520,   409,   736,   617,   410,   737,
     738,   267,   611,   425,   621,   427,  -427,   211,   211,   622,
     631,   824,   707,   704,  -428,   634,   542,  -430,  -429,   188,
     482,   -65,   -73,   235,   520,   482,   235,   712,   235,   211,
     316,   211,   211,    61,   235,   433,   204,   623,   205,   627,
     434,    76,   841,   -78,   520,   629,    76,   -86,   -78,   597,
     597,   267,   650,   651,   -80,   636,   637,  -428,   316,   -80,
    -430,  -429,   -88,   235,   572,   -78,   -78,   -64,   614,   553,
     860,   554,   555,   556,   557,   -85,   -80,   -80,   262,   553,
     -80,   554,   555,   556,   557,    76,   211,   211,   211,   211,
      76,   211,   211,   -77,   438,   211,   446,    76,   278,   713,
     211,   316,   782,   591,   614,   337,   338,   339,   340,   341,
     558,   559,   288,   289,   663,   795,   329,   330,   439,   -88,
     558,   559,   440,   664,   448,   879,   461,   442,   211,   560,
     670,   440,   561,   673,   675,   -85,   211,   211,   262,   560,
     452,   663,   561,   558,   559,   465,  -420,   562,   337,   338,
     339,   340,   341,   211,  -423,    76,   211,   468,  -283,   431,
     235,   606,   560,   469,   486,   561,   342,    76,   188,   188,
     666,   211,   684,   452,   235,    76,   373,    76,   836,   -90,
     698,   453,   454,   457,   838,   211,   204,   680,   205,  -420,
     -86,   539,   235,   575,   679,   699,   235,  -423,   211,  -283,
     235,  -283,   -82,   580,   576,  -482,   444,   343,   235,   344,
     440,   316,    76,   -78,   453,   454,   459,   688,   393,   374,
     611,   593,   743,   452,   605,   735,   329,   330,   615,    90,
     367,    90,   113,   113,   113,   211,   624,   628,   550,   385,
     399,   235,   229,   756,   757,   759,   760,   225,   723,   633,
     764,   766,   429,   -73,   635,   334,   335,   336,   337,   338,
     339,   340,   341,   649,   453,   454,   462,   449,   667,   235,
    -264,   368,   657,   369,    90,   658,  -431,   665,   279,   111,
     386,   400,   387,   401,   758,   682,   482,   428,   -88,   763,
     765,   683,   262,   386,   759,   430,   764,   766,   279,  -276,
     685,   689,   235,   692,  -284,   694,   776,   762,   450,   235,
     451,   -80,  -285,   235,   695,   697,   235,  -431,   704,  -431,
     807,   862,   711,   746,   747,   809,   748,   -85,   211,    76,
      42,    43,   452,   772,    90,   460,   715,   443,   235,   445,
    -276,   447,  -276,   262,   716,  -284,   820,  -284,    76,   232,
     -77,   793,   718,  -285,   725,  -285,   812,    94,   722,    94,
     732,   731,   863,   814,   864,    94,    94,    94,   734,   726,
     739,    94,    94,   453,   454,    94,   597,   741,   316,   820,
     733,   235,    94,   742,   744,   819,   773,   846,   774,   211,
     235,   777,   786,   329,   330,   235,   301,   302,   303,   304,
     305,   790,    94,    94,    94,   271,   275,   794,   796,   211,
     810,   815,   111,   579,    94,   211,   825,   816,  -265,   826,
     843,   587,   844,   589,   336,   337,   338,   339,   340,   341,
      76,   848,   235,    90,   235,   854,   850,   853,   227,   808,
     855,   856,   858,   861,   871,   874,   211,   235,   876,   884,
     314,   211,   211,   886,   235,   889,   235,   520,   632,   116,
      94,   752,    94,   349,   267,   350,   883,   190,   882,   830,
     235,   686,   778,   779,     0,   235,   300,     0,     0,     0,
       0,     0,   802,    90,     0,   804,   805,     0,    90,    76,
       0,     0,   211,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    76,    76,    76,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    90,     0,    73,
       0,    73,    90,     0,     0,     0,     0,     0,    76,    90,
     279,     0,   226,     0,     0,     0,     0,   211,     0,     0,
       0,     0,     0,    76,    76,     0,    76,     0,     0,     0,
       0,    94,     0,     0,     0,     0,   849,   851,   208,   208,
     208,   833,     0,     0,    73,    76,     0,    94,    94,     0,
       0,     0,     0,     0,     0,   250,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   408,   408,    90,     0,    94,
       0,    94,    94,   420,   868,   869,   265,     0,    76,    90,
     873,    94,     0,     0,     0,    76,    94,    90,     0,    90,
       0,    76,     0,    76,     0,     0,     0,     0,     0,   887,
      76,     0,     0,     0,    73,     0,     0,     0,     0,     0,
       0,     0,     0,   890,     0,     0,   892,     0,   211,     0,
       0,   894,     0,     0,    90,    94,    94,    94,    94,    94,
      94,    94,    94,   357,     0,    94,     0,    94,     0,     0,
      94,     0,     0,     0,     0,     0,   745,     0,     0,     0,
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
      93,     0,     0,    92,   160,   161,   162,   163,   164,   165,
      73,   166,   167,     0,     0,   168,     0,     0,   481,   169,
     170,   171,   172,   489,     0,    73,    73,     0,    73,     0,
       0,     0,     0,   173,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   832,     0,     0,     0,    73,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,     0,     0,
     184,   316,   317,   318,   319,   320,   321,   322,   323,   185,
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
     341,    92,     0,   835,     0,     0,     0,    93,     0,  -501,
       0,  -220,     0,     0,     0,     0,     0,  -501,  -501,  -501,
       0,     0,  -501,  -501,  -501,     0,  -501,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -501,     0,     0,     0,
      93,     0,     0,     0,     0,  -501,  -501,    93,  -501,  -501,
    -501,  -501,  -501,    93,     0,    93,     0,     0,     0,     0,
      92,     0,    93,     0,   700,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    92,    92,    92,   717,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -501,     0,     0,     0,     0,     0,   724,     0,     0,     0,
     727,     0,     0,     0,   728,     0,     0,     0,     0,    92,
       0,     0,   609,     0,  -501,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    92,    92,     0,    92,     0,     0,
    -501,     0,     0,  -501,  -501,     0,     0,   247,     0,  -501,
    -501,     0,     0,     0,     0,   755,    92,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   775,     0,     0,     0,     0,     0,    92,
       0,     0,     0,     0,     0,     0,    92,     0,     0,     0,
       0,     0,    92,     0,    92,     0,     0,     0,     0,     0,
       0,    92,     0,     0,     0,     0,   639,     0,     0,     0,
       0,     0,     0,   613,     0,     0,     0,     0,     0,     0,
     613,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -478,  -478,  -478,     0,  -478,     0,     0,     0,  -478,  -478,
       0,     0,   818,  -478,     0,  -478,  -478,  -478,  -478,  -478,
    -478,  -478,     0,  -478,     0,     0,  -478,  -478,  -478,  -478,
    -478,  -478,  -478,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -478,     0,     0,  -478,  -478,  -478,  -478,  -478,
    -478,  -478,  -478,  -478,  -478,   839,  -478,  -478,     0,  -478,
    -478,     0,     0,     0,   847,     0,     0,     0,     0,   852,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -478,
       0,     0,  -478,  -478,     0,  -478,  -478,     0,  -478,  -478,
    -478,  -478,  -478,  -478,  -478,  -478,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   609,     0,   613,     0,
       0,     0,     0,  -478,  -478,  -478,     0,  -478,     0,     0,
       0,     0,     0,     0,     0,  -478,     0,     0,   885,     0,
     888,     0,     0,     0,  -501,     4,     0,     5,     6,     7,
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
       0,     0,     0,     0,     0,  -485,     0,     0,     0,     0,
      58,    59,    60,  -485,  -485,  -485,     0,     0,     0,  -485,
    -485,     0,  -485,     0,  -501,  -501,     0,     0,   659,     0,
       0,  -485,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -485,  -485,     0,  -485,  -485,  -485,  -485,  -485,   316,
     317,   318,   319,   320,   321,   322,   323,   324,   325,   326,
     327,   328,     0,     0,   329,   330,     0,     0,     0,  -485,
    -485,  -485,  -485,  -485,  -485,  -485,  -485,  -485,  -485,  -485,
    -485,  -485,     0,     0,  -485,  -485,  -485,     0,   661,   331,
       0,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,     0,     0,     0,     0,     0,     0,     0,   -87,  -485,
       0,  -485,  -485,  -485,  -485,  -485,  -485,  -485,  -485,  -485,
    -485,     0,     0,     0,  -283,  -485,  -485,  -485,     0,  -485,
    -485,   -79,  -283,  -283,  -283,  -485,  -485,     0,  -283,  -283,
       0,  -283,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -283,  -283,     0,  -283,  -283,  -283,  -283,  -283,   316,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,     0,     0,   329,   330,     0,     0,     0,  -283,  -283,
    -283,  -283,  -283,  -283,  -283,  -283,  -283,  -283,  -283,  -283,
    -283,     0,     0,  -283,  -283,  -283,     0,   662,   331,   660,
     332,   333,   334,   335,   336,   337,   338,   339,   340,   341,
       0,     0,     0,     0,     0,     0,     0,   -89,  -283,     0,
    -283,  -283,  -283,  -283,  -283,  -283,  -283,  -283,  -283,  -283,
       0,     0,     0,     0,     0,  -283,  -283,     0,  -283,  -283,
     -81,     0,     0,     0,  -283,  -283,     4,     0,     5,     6,
       7,     8,     9,  -501,  -501,  -501,    10,    11,     0,     0,
    -501,    12,     0,    13,    14,    15,    16,    17,    18,    19,
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
       5,     6,     7,     8,     9,  -501,  -501,  -501,    10,    11,
       0,  -501,  -501,    12,     0,    13,    14,    15,    16,    17,
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
       4,     0,     5,     6,     7,     8,     9,  -501,  -501,  -501,
      10,    11,     0,     0,  -501,    12,  -501,    13,    14,    15,
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
       0,     0,     4,     0,     5,     6,     7,     8,     9,  -501,
    -501,  -501,    10,    11,     0,     0,  -501,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,     0,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    46,     0,     0,    47,    48,     0,    49,
      50,     0,    51,     0,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     4,     0,     5,     6,     7,
       8,     9,     0,  -501,  -501,    10,    11,    58,    59,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,  -501,  -501,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    46,     0,     0,    47,
      48,     0,    49,    50,     0,    51,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     4,     0,
       5,     6,     7,     8,     9,     0,     0,     0,    10,    11,
      58,    59,    60,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,  -501,  -501,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    46,
       0,     0,   261,    48,     0,    49,    50,     0,    51,     0,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    58,    59,    60,     0,     0,     0,     0,
       0,     0,  -501,     0,     0,     0,     0,  -501,  -501,     4,
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
       0,     0,     0,  -501,     0,     0,     0,     0,  -501,  -501,
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
    -501,     0,     4,     0,     5,     6,     7,     8,     9,  -501,
    -501,  -501,    10,    11,     0,     0,     0,    12,     0,    13,
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
       0,  -501,  -501,    20,    21,    22,    23,    24,    25,    26,
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
       0,     0,     0,    58,    59,    60,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   216,   217,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,     0,     0,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,     0,     0,     0,     0,     0,
     151,   152,   153,   154,   155,   156,   157,   158,    36,    37,
     159,    39,     0,     0,     0,     0,     0,     0,   160,   161,
     162,   163,   164,   165,     0,   166,   167,     0,     0,   168,
       0,     0,     0,   169,   170,   171,   172,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   173,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,     0,     0,   184,     0,     0,  -480,  -480,  -480,
       0,  -480,     0,   185,   186,  -480,  -480,     0,     0,     0,
    -480,     0,  -480,  -480,  -480,  -480,  -480,  -480,  -480,     0,
    -480,     0,     0,  -480,  -480,  -480,  -480,  -480,  -480,  -480,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -480,
       0,     0,  -480,  -480,  -480,  -480,  -480,  -480,  -480,  -480,
    -480,  -480,     0,  -480,  -480,     0,  -480,  -480,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -480,     0,     0,  -480,
    -480,     0,  -480,  -480,     0,  -480,  -480,  -480,  -480,  -480,
    -480,  -480,  -480,     0,     0,     0,     0,     0,     0,     0,
    -479,  -479,  -479,     0,  -479,     0,     0,     0,  -479,  -479,
    -480,  -480,  -480,  -479,  -480,  -479,  -479,  -479,  -479,  -479,
    -479,  -479,  -480,  -479,     0,     0,  -479,  -479,  -479,  -479,
    -479,  -479,  -479,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -479,     0,     0,  -479,  -479,  -479,  -479,  -479,
    -479,  -479,  -479,  -479,  -479,     0,  -479,  -479,     0,  -479,
    -479,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -479,
       0,     0,  -479,  -479,     0,  -479,  -479,     0,  -479,  -479,
    -479,  -479,  -479,  -479,  -479,  -479,     0,     0,     0,     0,
       0,     0,     0,  -481,  -481,  -481,     0,  -481,     0,     0,
       0,  -481,  -481,  -479,  -479,  -479,  -481,  -479,  -481,  -481,
    -481,  -481,  -481,  -481,  -481,  -479,     0,     0,     0,  -481,
    -481,  -481,  -481,  -481,  -481,  -481,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -481,     0,     0,  -481,  -481,
    -481,  -481,  -481,  -481,  -481,  -481,  -481,  -481,     0,  -481,
    -481,     0,  -481,  -481,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -481,   710,     0,  -481,  -481,     0,  -481,  -481,
       0,  -481,  -481,  -481,  -481,  -481,  -481,  -481,  -481,     0,
       0,     0,     0,   -87,     0,     0,  -482,  -482,  -482,     0,
    -482,     0,     0,     0,  -482,  -482,  -481,  -481,  -481,  -482,
       0,  -482,  -482,  -482,  -482,  -482,  -482,  -482,  -481,     0,
       0,     0,  -482,  -482,  -482,  -482,  -482,  -482,  -482,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -482,     0,
       0,  -482,  -482,  -482,  -482,  -482,  -482,  -482,  -482,  -482,
    -482,     0,  -482,  -482,     0,  -482,  -482,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -482,   662,     0,  -482,  -482,
       0,  -482,  -482,     0,  -482,  -482,  -482,  -482,  -482,  -482,
    -482,  -482,     0,     0,     0,     0,   -89,     0,     0,  -254,
    -254,  -254,     0,  -254,     0,     0,     0,  -254,  -254,  -482,
    -482,  -482,  -254,     0,  -254,  -254,  -254,  -254,  -254,  -254,
    -254,  -482,     0,     0,     0,  -254,  -254,  -254,  -254,  -254,
    -254,  -254,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -254,     0,     0,  -254,  -254,  -254,  -254,  -254,  -254,
    -254,  -254,  -254,  -254,     0,  -254,  -254,     0,  -254,  -254,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -254,     0,
       0,  -254,  -254,     0,  -254,  -254,     0,  -254,  -254,  -254,
    -254,  -254,  -254,  -254,  -254,     0,     0,     0,     0,     0,
       0,     0,  -254,  -254,  -254,     0,  -254,     0,     0,     0,
    -254,  -254,  -254,  -254,  -254,  -254,     0,  -254,  -254,  -254,
    -254,  -254,  -254,  -254,   244,     0,     0,     0,  -254,  -254,
    -254,  -254,  -254,  -254,  -254,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -254,     0,     0,  -254,  -254,  -254,
    -254,  -254,  -254,  -254,  -254,  -254,  -254,     0,  -254,  -254,
       0,  -254,  -254,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -254,     0,     0,  -254,  -254,     0,  -254,  -254,     0,
    -254,  -254,  -254,  -254,  -254,  -254,  -254,  -254,     0,     0,
       0,     0,     0,     0,     0,  -483,  -483,  -483,     0,  -483,
       0,     0,     0,  -483,  -483,  -254,  -254,  -254,  -483,     0,
    -483,  -483,  -483,  -483,  -483,  -483,  -483,   247,     0,     0,
       0,  -483,  -483,  -483,  -483,  -483,  -483,  -483,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -483,     0,     0,
    -483,  -483,  -483,  -483,  -483,  -483,  -483,  -483,  -483,  -483,
       0,  -483,  -483,     0,  -483,  -483,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -483,     0,     0,  -483,  -483,     0,
    -483,  -483,     0,  -483,  -483,  -483,  -483,  -483,  -483,  -483,
    -483,     0,     0,     0,     0,     0,     0,     0,  -484,  -484,
    -484,     0,  -484,     0,     0,     0,  -484,  -484,  -483,  -483,
    -483,  -484,     0,  -484,  -484,  -484,  -484,  -484,  -484,  -484,
    -483,     0,     0,     0,  -484,  -484,  -484,  -484,  -484,  -484,
    -484,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -484,     0,     0,  -484,  -484,  -484,  -484,  -484,  -484,  -484,
    -484,  -484,  -484,     0,  -484,  -484,     0,  -484,  -484,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -484,     0,     0,
    -484,  -484,     0,  -484,  -484,     0,  -484,  -484,  -484,  -484,
    -484,  -484,  -484,  -484,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -484,  -484,  -484,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -484,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,     0,     0,
     141,   142,   143,   192,   193,   194,   195,   148,   149,   150,
       0,     0,     0,     0,     0,   151,   152,   153,   154,   196,
     197,   198,   158,   281,   282,   199,   283,     0,     0,     0,
       0,     0,     0,   160,   161,   162,   163,   164,   165,     0,
     166,   167,     0,     0,   168,     0,     0,     0,   169,   170,
     171,   172,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   173,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,     0,     0,   184,
       0,     0,     0,     0,     0,     0,     0,     0,   185,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,     0,     0,   141,   142,   143,   192,   193,
     194,   195,   148,   149,   150,     0,     0,     0,     0,     0,
     151,   152,   153,   154,   196,   197,   198,   158,   251,     0,
     199,     0,     0,     0,     0,     0,     0,     0,   160,   161,
     162,   163,   164,   165,     0,   166,   167,     0,     0,   168,
       0,     0,     0,   169,   170,   171,   172,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   173,     0,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,     0,     0,   184,     0,     0,     0,     0,     0,
       0,     0,     0,   185,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,     0,     0,
     141,   142,   143,   192,   193,   194,   195,   148,   149,   150,
       0,     0,     0,     0,     0,   151,   152,   153,   154,   196,
     197,   198,   158,     0,     0,   199,     0,     0,     0,     0,
       0,     0,     0,   160,   161,   162,   163,   164,   165,     0,
     166,   167,     0,     0,   168,     0,     0,     0,   169,   170,
     171,   172,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   173,     0,    52,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,     0,     0,   184,
       0,     0,     0,     0,     0,     0,     0,     0,   185,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,     0,     0,   141,   142,   143,   192,   193,
     194,   195,   148,   149,   150,     0,     0,     0,     0,     0,
     151,   152,   153,   154,   196,   197,   198,   158,     0,     0,
     199,     0,     0,     0,     0,     0,     0,     0,   160,   161,
     162,   163,   164,   165,     0,   166,   167,     0,     0,   168,
       0,     0,     0,   169,   170,   171,   172,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   173,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,     0,     0,   184,     0,     5,     6,     7,     0,
       9,     0,     0,   185,    10,    11,     0,     0,     0,    12,
       0,    13,    14,    15,    97,    98,    18,    19,     0,     0,
       0,     0,    99,    21,    22,    23,    24,    25,    26,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,     0,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   207,     0,     0,   107,    48,
       0,    49,    50,     0,   231,   232,    52,    53,    54,    55,
      56,    57,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     8,     9,    58,
     233,    60,    10,    11,     0,     0,     0,    12,   411,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,     0,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    46,     0,     0,    47,    48,     0,    49,
      50,     0,    51,     0,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    58,    59,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   207,     0,     0,   107,
      48,     0,    49,    50,     0,   616,   232,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      58,    59,    60,    12,     0,    13,    14,    15,    97,    98,
      18,    19,     0,     0,     0,     0,    99,    21,    22,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   107,    48,     0,    49,    50,     0,   231,   232,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    58,   233,    60,    12,     0,    13,    14,
      15,    97,    98,    18,    19,     0,     0,     0,     0,    99,
      21,    22,    23,    24,    25,    26,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    29,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   207,     0,     0,   107,   413,     0,    49,    50,
       0,   231,   232,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    58,   233,    60,    12,
       0,    13,    14,    15,    97,    98,    18,    19,     0,     0,
       0,     0,    99,   100,   101,    23,    24,    25,    26,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,     0,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   207,     0,     0,   107,    48,
       0,    49,    50,     0,   608,   232,    52,    53,    54,    55,
      56,    57,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    58,
     233,    60,    12,     0,    13,    14,    15,    97,    98,    18,
      19,     0,     0,     0,     0,    99,   100,   101,    23,    24,
      25,    26,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     0,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   207,     0,
       0,   107,    48,     0,    49,    50,     0,   612,   232,    52,
      53,    54,    55,    56,    57,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    58,   233,    60,    12,     0,    13,    14,    15,
      97,    98,    18,    19,     0,     0,     0,     0,    99,    21,
      22,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,     0,    42,    43,
       0,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   207,     0,     0,   107,    48,     0,    49,    50,     0,
     608,   232,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    58,   233,    60,    12,     0,
      13,    14,    15,    97,    98,    18,    19,     0,     0,     0,
       0,    99,   100,   101,    23,    24,    25,    26,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   207,     0,     0,   107,    48,     0,
      49,    50,     0,   803,   232,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    58,   233,
      60,    12,     0,    13,    14,    15,    97,    98,    18,    19,
       0,     0,     0,     0,    99,   100,   101,    23,    24,    25,
      26,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      29,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,     0,    42,    43,     0,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   207,     0,     0,
     107,    48,     0,    49,    50,     0,   806,   232,    52,    53,
      54,    55,    56,    57,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    58,   233,    60,    12,     0,    13,    14,    15,    97,
      98,    18,    19,     0,     0,     0,     0,    99,   100,   101,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,     0,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     207,     0,     0,   107,    48,     0,    49,    50,     0,   811,
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
      50,     0,   870,   232,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    58,   233,    60,
      12,     0,    13,    14,    15,    97,    98,    18,    19,     0,
       0,     0,     0,    99,   100,   101,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   207,     0,     0,   107,
      48,     0,    49,    50,     0,   872,   232,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      58,   233,    60,    12,     0,    13,    14,    15,    97,    98,
      18,    19,     0,     0,     0,     0,    99,   100,   101,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   107,    48,     0,    49,    50,     0,   891,   232,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    58,   233,    60,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,     0,    29,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   207,     0,     0,   107,    48,     0,    49,    50,
       0,     0,     0,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    58,    59,    60,    12,
       0,    13,    14,    15,    97,    98,    18,    19,     0,     0,
       0,     0,    99,    21,    22,    23,    24,    25,    26,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,     0,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   207,     0,     0,   107,    48,
       0,    49,    50,     0,   266,     0,    52,    53,    54,    55,
      56,    57,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    58,
     233,    60,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     0,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   207,     0,
       0,   107,    48,     0,    49,    50,     0,   479,     0,    52,
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
     590,     0,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    58,   233,    60,    12,     0,
      13,    14,    15,    97,    98,    18,    19,     0,     0,     0,
       0,    99,   100,   101,    23,    24,    25,    26,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   207,     0,     0,   107,    48,     0,
      49,    50,     0,   638,     0,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    58,   233,
      60,    12,     0,    13,    14,    15,    97,    98,    18,    19,
       0,     0,     0,     0,    99,   100,   101,    23,    24,    25,
      26,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      29,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,     0,    42,    43,     0,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   207,     0,     0,
     107,    48,     0,    49,    50,     0,   479,     0,    52,    53,
      54,    55,    56,    57,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    58,   233,    60,    12,     0,    13,    14,    15,    97,
      98,    18,    19,     0,     0,     0,     0,    99,   100,   101,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,     0,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     207,     0,     0,   107,    48,     0,    49,    50,     0,   754,
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
      50,     0,   797,     0,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    58,   233,    60,
      12,     0,    13,    14,    15,    97,    98,    18,    19,     0,
       0,     0,     0,    99,   100,   101,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   207,     0,     0,   107,
      48,     0,    49,    50,     0,     0,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      58,   233,    60,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   107,    48,     0,    49,    50,     0,     0,     0,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    58,   233,    60,    12,     0,    13,    14,
      15,    97,    98,    18,    19,     0,     0,     0,     0,    99,
     100,   101,    23,    24,    25,    26,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   102,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   224,     0,     0,    47,    48,     0,    49,    50,
       0,    51,     0,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     5,     6,     7,     0,     9,     0,   751,     0,
      10,    11,     0,     0,     0,    12,   108,    13,    14,    15,
      97,    98,    18,    19,     0,     0,     0,     0,    99,   100,
     101,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   102,     0,     0,    32,    33,   103,
      35,    36,    37,   104,    39,    40,    41,     0,    42,    43,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   105,     0,
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
     224,     0,     0,    47,    48,     0,    49,    50,     0,    51,
       0,    52,    53,    54,    55,    56,    57,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,   108,    13,    14,    15,    97,    98,
      18,    19,     0,     0,     0,     0,    99,   100,   101,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   102,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   276,
       0,     0,   312,    48,     0,    49,    50,     0,   313,     0,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,     0,
       0,     0,    12,   108,    13,    14,    15,    97,    98,    18,
      19,     0,     0,     0,     0,    99,   100,   101,    23,    24,
      25,    26,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   102,     0,     0,    32,    33,   103,    35,    36,    37,
     104,    39,    40,    41,     0,    42,    43,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   106,     0,
       0,   107,    48,     0,    49,    50,     0,     0,     0,    52,
      53,    54,    55,    56,    57,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,     0,     0,
       0,    12,   108,    13,    14,    15,    97,    98,    18,    19,
       0,     0,     0,     0,    99,   100,   101,    23,    24,    25,
      26,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     102,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,     0,    42,    43,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   276,     0,     0,
     107,    48,     0,    49,    50,     0,     0,     0,    52,    53,
      54,    55,    56,    57,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,     0,     0,
      12,   108,    13,    14,    15,    97,    98,    18,    19,     0,
       0,     0,     0,    99,   100,   101,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   102,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   831,     0,     0,   107,
      48,     0,    49,    50,     0,     0,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,   524,
     525,     0,     0,   526,     0,     0,     0,     0,     0,     0,
     108,   160,   161,   162,   163,   164,   165,     0,   166,   167,
       0,     0,   168,     0,     0,     0,   169,   170,   171,   172,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     173,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   174,   175,   176,   177,   178,
     179,   180,   181,   182,   183,   529,   525,   184,     0,   530,
       0,     0,     0,     0,     0,     0,   185,   160,   161,   162,
     163,   164,   165,     0,   166,   167,     0,     0,   168,     0,
       0,     0,   169,   170,   171,   172,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   173,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   174,   175,   176,   177,   178,   179,   180,   181,   182,
     183,   545,   518,   184,     0,   546,     0,     0,     0,     0,
       0,     0,   185,   160,   161,   162,   163,   164,   165,     0,
     166,   167,     0,     0,   168,     0,     0,     0,   169,   170,
     171,   172,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   173,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   582,   518,   184,
       0,   583,     0,     0,     0,     0,     0,     0,   185,   160,
     161,   162,   163,   164,   165,     0,   166,   167,     0,     0,
     168,     0,     0,     0,   169,   170,   171,   172,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   173,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   174,   175,   176,   177,   178,   179,   180,
     181,   182,   183,   585,   525,   184,     0,   586,     0,     0,
       0,     0,     0,     0,   185,   160,   161,   162,   163,   164,
     165,     0,   166,   167,     0,     0,   168,     0,     0,     0,
     169,   170,   171,   172,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   173,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   599,
     518,   184,     0,   600,     0,     0,     0,     0,     0,     0,
     185,   160,   161,   162,   163,   164,   165,     0,   166,   167,
       0,     0,   168,     0,     0,     0,   169,   170,   171,   172,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     173,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   174,   175,   176,   177,   178,
     179,   180,   181,   182,   183,   602,   525,   184,     0,   603,
       0,     0,     0,     0,     0,     0,   185,   160,   161,   162,
     163,   164,   165,     0,   166,   167,     0,     0,   168,     0,
       0,     0,   169,   170,   171,   172,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   173,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   174,   175,   176,   177,   178,   179,   180,   181,   182,
     183,   640,   518,   184,     0,   641,     0,     0,     0,     0,
       0,     0,   185,   160,   161,   162,   163,   164,   165,     0,
     166,   167,     0,     0,   168,     0,     0,     0,   169,   170,
     171,   172,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   173,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   643,   525,   184,
       0,   644,     0,     0,     0,     0,     0,     0,   185,   160,
     161,   162,   163,   164,   165,     0,   166,   167,     0,     0,
     168,     0,     0,     0,   169,   170,   171,   172,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   173,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   174,   175,   176,   177,   178,   179,   180,
     181,   182,   183,   877,   518,   184,     0,   878,     0,     0,
       0,     0,     0,     0,   185,   160,   161,   162,   163,   164,
     165,     0,   166,   167,     0,     0,   168,     0,     0,     0,
     169,   170,   171,   172,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   173,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   880,
     525,   184,     0,   881,     0,     0,     0,     0,     0,     0,
     185,   160,   161,   162,   163,   164,   165,     0,   166,   167,
       0,     0,   168,     0,     0,     0,   169,   170,   171,   172,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     173,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   174,   175,   176,   177,   178,
     179,   180,   181,   182,   183,     0,     0,   184,     0,     0,
       0,     0,     0,     0,     0,     0,   185,   316,   317,   318,
     319,   320,   321,   322,   323,   324,   325,   326,   327,   328,
       0,     0,   329,   330,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   331,     0,   332,
     333,   334,   335,   336,   337,   338,   339,   340,   341,   316,
     317,   318,   319,   320,   321,   322,   323,   324,   325,   326,
     327,   328,     0,   253,   329,   330,     0,     0,     0,  -220,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,     0,     0,   329,   330,     0,     0,   331,
       0,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     331,  -220,   332,   333,   334,   335,   336,   337,   338,   339,
     340,   341,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   605,   316,   317,   318,   319,   320,   321,   322,
     323,   324,   325,   326,   327,   328,     0,     0,   329,   330,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,  -502,  -502,     0,     0,   329,   330,     0,     0,     0,
       0,     0,     0,   331,     0,   332,   333,   334,   335,   336,
     337,   338,   339,   340,   341,     0,     0,     0,     0,     0,
       0,     0,   332,   333,   334,   335,   336,   337,   338,   339,
     340,   341,   316,  -502,  -502,  -502,  -502,   321,   322,     0,
       0,  -502,  -502,     0,     0,     0,     0,   329,   330,   316,
     317,   318,   319,   320,   321,   322,     0,     0,   325,   326,
       0,     0,     0,     0,   329,   330,     0,     0,     0,     0,
       0,     0,     0,     0,   332,   333,   334,   335,   336,   337,
     338,   339,   340,   341,     0,     0,     0,     0,     0,     0,
       0,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341
};

static const yytype_int16 yycheck[] =
{
      16,    17,     7,     8,    20,   343,   215,    16,    17,    15,
     370,    20,    16,    17,     7,    22,    20,    16,    17,   247,
       8,    20,     8,    28,    22,   397,    51,   409,    44,    45,
     368,     5,     6,    49,    50,   391,   106,   315,    62,   692,
      28,    15,    28,    59,    60,    27,    50,    52,   386,    28,
      49,   372,   531,   532,    13,    11,    12,   577,    26,    52,
     442,    13,   400,     4,   568,    12,   570,   587,    26,    47,
     287,    36,    37,     0,   291,   533,   534,    51,   261,     1,
      92,   689,    59,    58,   692,    25,   130,    94,    58,    13,
      81,    25,    81,    83,   101,   664,    94,    71,   110,    17,
      84,   670,    20,    15,    54,    17,    13,    64,   115,    72,
      25,   129,   450,   103,    83,    62,   342,   120,   344,   345,
     346,    83,   102,   105,    99,   100,    16,    17,   129,   312,
      20,   101,    28,   124,   103,   124,   126,    36,    37,   130,
      50,   103,   126,   369,    54,   108,   105,    83,   130,   131,
      56,    57,   120,   105,   129,    26,   113,   114,   115,   385,
      50,   387,   120,   110,   126,   130,    26,    26,   108,   128,
     128,   130,   131,   826,   108,   401,    81,   103,   130,   131,
      15,   105,    17,   126,   544,   107,   126,     2,   128,     4,
       5,     6,   126,   108,   218,    10,    11,    12,   105,   103,
      15,    16,    17,   563,   430,    20,   130,   131,   130,   131,
      81,   126,    27,   128,    94,   231,   232,   233,   826,   124,
     678,    81,    81,   130,   131,   451,   125,   128,   244,    81,
     246,   247,    47,    48,    49,   244,    51,   246,   247,   128,
     244,   562,   246,   247,    59,   244,   463,   246,   247,   120,
     266,   122,    81,   124,   130,   131,    71,   128,   737,   738,
     120,   120,   122,   122,   124,   124,   222,   223,   128,   128,
      25,   218,   124,   126,    52,   779,   780,    25,   798,    54,
     784,    59,    60,   261,    25,    36,    37,   267,   313,    58,
     105,   103,   107,   122,   310,   124,    83,   367,   576,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,   338,   339,   340,   341,   244,   343,   246,   247,
      99,   100,   101,   310,   312,   351,   352,   342,   315,   313,
     131,    83,    54,   725,   343,   301,   302,   303,   304,    25,
     732,   723,   368,   108,   244,   859,   246,   247,    84,   300,
     108,   103,   367,    81,   305,    25,    83,   108,   384,   368,
     386,   126,   388,   389,   351,   352,    25,   393,   126,    25,
     385,    56,    57,   592,   400,   126,   103,   386,   126,   405,
     406,   206,    25,   409,   399,   126,   624,   413,   240,   625,
     626,   400,   406,   128,   413,   120,   124,   222,   223,   413,
     426,   771,    14,    15,    81,   431,   363,    81,    81,   424,
     310,   125,   126,   439,   429,   315,   442,    83,   444,   244,
      64,   246,   247,     2,   450,   125,   424,   417,   424,   419,
     125,   256,   798,   103,   449,   424,   261,   103,   108,   396,
     397,   450,   468,   469,   103,   435,   436,   124,    64,   108,
     124,   124,   108,   479,   785,   125,   126,   125,   850,    50,
     830,    52,    53,    54,    55,   108,   125,   126,    47,    50,
     126,    52,    53,    54,    55,   300,   301,   302,   303,   304,
     305,   306,   307,   126,   123,   310,   121,   312,   313,    83,
     315,    64,    50,   393,   886,   111,   112,   113,   114,   115,
      91,    92,    54,    55,   521,    83,    79,    80,   126,   103,
      91,    92,   130,   521,    54,   863,   101,   126,   343,   110,
     528,   130,   113,   531,   532,   103,   351,   352,   107,   110,
      58,   548,   113,    91,    92,   101,    81,   128,   111,   112,
     113,   114,   115,   368,    81,   370,   371,    64,    81,    83,
     576,   403,   110,    64,   126,   113,    81,   382,   573,   574,
      83,   386,   552,    58,   590,   390,    81,   392,   787,   103,
     573,    99,   100,   101,   793,   400,   574,   543,   574,   124,
     103,    90,   608,    10,   541,   574,   612,   124,   413,   122,
     616,   124,   126,    13,     8,   128,   126,   122,   624,   124,
     130,    64,   427,   126,    99,   100,   101,   564,    17,   124,
     624,    10,   638,    58,   126,   624,    79,    80,   125,     2,
      81,     4,     5,     6,     7,   450,   126,   125,   864,    81,
      81,   657,    15,   659,   660,   661,   662,   653,   604,   121,
     666,   667,    81,   126,   125,   108,   109,   110,   111,   112,
     113,   114,   115,   129,    99,   100,   101,    81,    83,   685,
     126,   122,   126,   124,    47,   125,    81,   123,    51,   653,
     122,   122,   124,   124,   661,   123,   576,   256,   103,   666,
     667,    10,   261,   122,   710,   124,   712,   713,    71,    81,
     103,   126,   718,   126,    81,    50,   686,    83,   122,   725,
     124,   126,    81,   729,   126,    50,   732,   122,    15,   124,
     729,    81,   123,    52,    53,   729,    55,   103,   543,   544,
      59,    60,    58,   680,   107,    61,    10,   270,   754,   272,
     122,   274,   124,   312,    10,   122,   762,   124,   563,    92,
     126,   707,   126,   122,   126,   124,   736,     2,   123,     4,
     126,   125,   122,   743,   124,    10,    11,    12,   125,   611,
     125,    16,    17,    99,   100,    20,   723,   121,    64,   795,
     622,   797,    27,   125,   123,   762,    10,   803,   125,   604,
     806,    10,    84,    79,    80,   811,    39,    40,    41,    42,
      43,     9,    47,    48,    49,    49,    50,    10,    10,   624,
     125,   121,   786,   382,    59,   630,   125,   108,   126,   126,
      10,   390,    10,   392,   110,   111,   112,   113,   114,   115,
     645,   126,   848,   206,   850,    10,   126,   125,   653,   729,
      10,   121,    10,    54,   848,   121,   661,   863,    10,    10,
      71,   666,   667,   126,   870,   123,   872,   862,   427,     6,
     105,   653,   107,    78,   863,    81,   866,     7,   865,   785,
     886,   562,   689,   689,    -1,   891,    62,    -1,    -1,    -1,
      -1,    -1,   724,   256,    -1,   727,   728,    -1,   261,   704,
      -1,    -1,   707,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   719,   720,   721,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   300,    -1,     2,
      -1,     4,   305,    -1,    -1,    -1,    -1,    -1,   753,   312,
     313,    -1,    15,    -1,    -1,    -1,    -1,   762,    -1,    -1,
      -1,    -1,    -1,   768,   769,    -1,   771,    -1,    -1,    -1,
      -1,   206,    -1,    -1,    -1,    -1,   808,   809,    10,    11,
      12,   786,    -1,    -1,    47,   790,    -1,   222,   223,    -1,
      -1,    -1,    -1,    -1,    -1,    27,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   239,   240,   370,    -1,   244,
      -1,   246,   247,   247,   846,   847,    48,    -1,   823,   382,
     852,   256,    -1,    -1,    -1,   830,   261,   390,    -1,   392,
      -1,   836,    -1,   838,    -1,    -1,    -1,    -1,    -1,   871,
     845,    -1,    -1,    -1,   107,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   885,    -1,    -1,   888,    -1,   863,    -1,
      -1,   893,    -1,    -1,   427,   300,   301,   302,   303,   304,
     305,   306,   307,   105,    -1,   310,    -1,   312,    -1,    -1,
     315,    -1,    -1,    -1,    -1,    -1,   645,    -1,    -1,    -1,
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
      -1,    -1,    -1,   116,   117,   118,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   130,   131,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,    -1,
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
      -1,   116,   117,   118,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   128,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    -1,    -1,    -1,
      -1,    -1,    -1,    62,    63,    64,    65,    66,    67,    -1,
      69,    70,    -1,    -1,    73,    -1,    -1,    -1,    77,    78,
      79,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,    -1,    -1,   118,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   127,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,    -1,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    -1,
      54,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    62,    63,
      64,    65,    66,    67,    -1,    69,    70,    -1,    -1,    73,
      -1,    -1,    -1,    77,    78,    79,    80,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,    93,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    -1,    -1,   118,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   127,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,
      49,    50,    51,    -1,    -1,    54,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    62,    63,    64,    65,    66,    67,    -1,
      69,    70,    -1,    -1,    73,    -1,    -1,    -1,    77,    78,
      79,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    91,    -1,    93,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,    -1,    -1,   118,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   127,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,    -1,
      44,    45,    46,    47,    48,    49,    50,    51,    -1,    -1,
      54,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    62,    63,
      64,    65,    66,    67,    -1,    69,    70,    -1,    -1,    73,
      -1,    -1,    -1,    77,    78,    79,    80,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    -1,    -1,   118,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,   127,    11,    12,    -1,    -1,    -1,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
      -1,    88,    89,    -1,    91,    92,    93,    94,    95,    96,
      97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,   116,
     117,   118,    11,    12,    -1,    -1,    -1,    16,   125,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    38,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,
      89,    -1,    91,    -1,    93,    94,    95,    96,    97,    98,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,   116,   117,   118,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    -1,    45,
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
      30,    31,    32,    33,    34,    35,    -1,    -1,    38,    -1,
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
      57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
      -1,    88,    89,    -1,    91,    -1,    93,    94,    95,    96,
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
      86,    -1,    88,    89,    -1,    -1,    -1,    93,    94,    95,
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
      60,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    91,    -1,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,   108,    -1,
      11,    12,    -1,    -1,    -1,    16,   116,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    79,    -1,
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
      82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,
      -1,    93,    94,    95,    96,    97,    98,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,   116,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,    -1,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,
      -1,    -1,    16,   116,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    45,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,
      -1,    85,    86,    -1,    88,    89,    -1,    -1,    -1,    93,
      94,    95,    96,    97,    98,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,
      -1,    16,   116,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,    34,
      35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      45,    -1,    -1,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
      85,    86,    -1,    88,    89,    -1,    -1,    -1,    93,    94,
      95,    96,    97,    98,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,
      16,   116,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    -1,    -1,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    50,
      51,    -1,    -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,
     116,    62,    63,    64,    65,    66,    67,    -1,    69,    70,
      -1,    -1,    73,    -1,    -1,    -1,    77,    78,    79,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,    50,    51,   118,    -1,    54,
      -1,    -1,    -1,    -1,    -1,    -1,   127,    62,    63,    64,
      65,    66,    67,    -1,    69,    70,    -1,    -1,    73,    -1,
      -1,    -1,    77,    78,    79,    80,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,    50,    51,   118,    -1,    54,    -1,    -1,    -1,    -1,
      -1,    -1,   127,    62,    63,    64,    65,    66,    67,    -1,
      69,    70,    -1,    -1,    73,    -1,    -1,    -1,    77,    78,
      79,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,    50,    51,   118,
      -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,   127,    62,
      63,    64,    65,    66,    67,    -1,    69,    70,    -1,    -1,
      73,    -1,    -1,    -1,    77,    78,    79,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,    50,    51,   118,    -1,    54,    -1,    -1,
      -1,    -1,    -1,    -1,   127,    62,    63,    64,    65,    66,
      67,    -1,    69,    70,    -1,    -1,    73,    -1,    -1,    -1,
      77,    78,    79,    80,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,    50,
      51,   118,    -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,
     127,    62,    63,    64,    65,    66,    67,    -1,    69,    70,
      -1,    -1,    73,    -1,    -1,    -1,    77,    78,    79,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,    50,    51,   118,    -1,    54,
      -1,    -1,    -1,    -1,    -1,    -1,   127,    62,    63,    64,
      65,    66,    67,    -1,    69,    70,    -1,    -1,    73,    -1,
      -1,    -1,    77,    78,    79,    80,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,    50,    51,   118,    -1,    54,    -1,    -1,    -1,    -1,
      -1,    -1,   127,    62,    63,    64,    65,    66,    67,    -1,
      69,    70,    -1,    -1,    73,    -1,    -1,    -1,    77,    78,
      79,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,    50,    51,   118,
      -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,   127,    62,
      63,    64,    65,    66,    67,    -1,    69,    70,    -1,    -1,
      73,    -1,    -1,    -1,    77,    78,    79,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,    50,    51,   118,    -1,    54,    -1,    -1,
      -1,    -1,    -1,    -1,   127,    62,    63,    64,    65,    66,
      67,    -1,    69,    70,    -1,    -1,    73,    -1,    -1,    -1,
      77,    78,    79,    80,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,    50,
      51,   118,    -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,
     127,    62,    63,    64,    65,    66,    67,    -1,    69,    70,
      -1,    -1,    73,    -1,    -1,    -1,    77,    78,    79,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,    -1,    -1,   118,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   127,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      -1,    -1,    79,    80,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   104,    -1,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    -1,   130,    79,    80,    -1,    -1,    -1,    84,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    -1,    -1,    79,    80,    -1,    -1,   104,
      -1,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     104,   126,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   126,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    -1,    -1,    79,    80,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    -1,    -1,    79,    80,    -1,    -1,    -1,
      -1,    -1,    -1,   104,    -1,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    64,    65,    66,    67,    68,    69,    70,    -1,
      -1,    73,    74,    -1,    -1,    -1,    -1,    79,    80,    64,
      65,    66,    67,    68,    69,    70,    -1,    -1,    73,    74,
      -1,    -1,    -1,    -1,    79,    80,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   133,   134,     0,     1,     3,     4,     5,     6,     7,
      11,    12,    16,    18,    19,    20,    21,    22,    23,    24,
      29,    30,    31,    32,    33,    34,    35,    38,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    59,    60,    62,    63,    82,    85,    86,    88,
      89,    91,    93,    94,    95,    96,    97,    98,   116,   117,
     118,   136,   137,   138,   141,   143,   144,   148,   149,   151,
     152,   153,   154,   155,   165,   182,   199,   209,   210,   223,
     224,   225,   226,   227,   228,   229,   232,   240,   242,   243,
     244,   245,   246,   247,   266,   275,   138,    21,    22,    29,
      30,    31,    45,    50,    54,    79,    82,    85,   116,   156,
     157,   182,   199,   244,   247,   266,   157,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    44,    45,    46,    47,    48,    49,    50,    51,    54,
      62,    63,    64,    65,    66,    67,    69,    70,    73,    77,
      78,    79,    80,    91,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   118,   127,   128,   158,   163,   164,
     245,   261,    32,    33,    34,    35,    48,    49,    50,    54,
     158,   159,   160,   161,   240,   242,   183,    82,   141,   142,
     155,   199,   244,   246,   247,   142,   130,   131,   142,   270,
     273,   274,   186,   188,    82,   149,   155,   199,   204,   244,
     247,    91,    92,   117,   148,   165,   167,   171,   178,   180,
     264,   265,   171,   171,   128,   173,   174,   128,   169,   173,
     141,    52,   160,   130,   271,   140,   120,   165,   199,   165,
      54,    85,   136,   150,   151,   141,    91,   148,   168,   180,
     264,   275,   180,   263,   264,   275,    82,   154,   199,   244,
     247,    52,    53,    55,   158,   235,   241,   234,   235,   235,
     129,   230,   129,   233,    56,    57,   143,   165,   165,   270,
     274,    39,    40,    41,    42,    43,    36,    37,    28,   206,
     103,   126,    85,    91,   152,   103,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    79,
      80,   104,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    81,   122,   124,    81,   124,    26,   120,   211,
     226,    83,    83,   169,   173,   211,   271,   141,    50,    54,
     156,    56,    57,     1,   107,   248,   273,    81,   122,   124,
     195,   262,   196,    81,   124,   269,   126,   135,   136,    54,
      13,   105,   200,   273,   103,    81,   122,   124,    83,    83,
     200,   270,    15,    17,   216,   131,   142,   142,    54,    81,
     122,   124,    25,   167,   167,    84,   126,   179,   275,   126,
     179,   125,   171,    86,   171,   175,   148,   171,   180,   209,
     275,    52,    59,    60,   139,   128,   166,   120,   136,    81,
     124,    83,   150,   125,   125,   184,   165,   271,   123,   126,
     130,   272,   126,   272,   126,   272,   121,   272,    54,    81,
     122,   124,    58,    99,   100,   101,   236,   101,   236,   101,
      61,   101,   101,   231,   236,   101,    58,   101,    64,    64,
     138,   142,   142,   142,   142,   138,   141,   141,   207,    91,
     143,   167,   180,   181,   150,   154,   126,   143,   165,   167,
     181,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,    50,    51,    54,
     163,   267,   268,   168,    50,    51,    54,   163,   267,    50,
      54,   267,   267,   214,   212,   143,   165,   143,   165,    90,
     145,   193,   273,   249,   192,    50,    54,   156,   267,   168,
     267,   135,   141,    50,    52,    53,    54,    55,    91,    92,
     110,   113,   128,   250,   251,   252,   253,   254,   255,   256,
     257,   258,   259,   197,   162,    10,     8,   219,   275,   136,
      13,   165,    50,    54,   168,    50,    54,   136,   216,   136,
      91,   180,   217,    10,    27,   105,   201,   273,   201,    50,
      54,   168,    50,    54,   190,   126,   179,   167,    91,   167,
     178,   264,    91,   167,   265,   125,    91,   165,   167,   172,
     176,   178,   264,   271,   126,    81,   124,   271,   125,   160,
     185,   165,   136,   121,   165,   125,   271,   271,    91,   167,
      50,    54,   168,    50,    54,   238,   237,   129,   236,   129,
     165,   165,    72,   108,   205,   275,   167,   126,   125,    43,
     105,    83,    83,   169,   173,   123,    83,    83,   169,   170,
     173,   275,   170,   173,   170,   173,   205,   205,   146,   273,
     142,   135,   123,    10,   271,   103,   251,   135,   273,   126,
     260,   275,   126,   260,    50,   126,   260,    50,   158,   160,
     167,   181,   220,   275,    15,   203,   275,    14,   202,   203,
      83,   123,    83,    83,   203,    10,    10,   167,   126,   200,
     187,   189,   123,   142,   167,   126,   179,   167,   167,   126,
     177,   125,   126,   179,   125,   148,   209,   267,   267,   125,
     141,   121,   125,   165,   123,   136,    52,    53,    55,   239,
     247,   108,   204,   208,    91,   167,   165,   165,   143,   165,
     165,   145,    83,   143,   165,   143,   165,   145,   215,   213,
     205,   194,   273,    10,   125,   167,   271,    10,   252,   255,
     257,   259,    50,   254,   257,   198,    84,   221,   275,   136,
       9,   222,   275,   142,    10,    83,    10,    91,   136,   136,
     136,   201,   179,    91,   179,   179,    91,   178,   180,   264,
     125,    91,   271,   125,   271,   121,   108,   136,   167,   143,
     165,   136,   136,   147,   135,   125,   126,   260,   260,   260,
     250,    82,   155,   199,   244,   247,   200,   136,   200,   167,
     203,   216,   218,    10,    10,   191,   165,   167,   126,   179,
     126,   179,   167,   125,    10,    10,   121,   136,    10,   257,
     135,    54,    81,   122,   124,   136,   136,   136,   179,   179,
      91,   264,    91,   179,   121,   260,    10,    50,    54,   168,
      50,    54,   219,   202,    10,   167,   126,   179,   167,   123,
     179,    91,   179,   167,   179
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
        case 2:
#line 346 "parse.y"
    {
			lex_state = EXPR_BEG;
                        top_local_init();
			if (ruby_class == rb_cObject) class_nest = 0;
			else class_nest = 1;
		    }
    break;

  case 3:
#line 353 "parse.y"
    {
			if ((yyvsp[(2) - (2)].node) && !compile_for_eval) {
                            /* last expression should not be void */
			    if (nd_type((yyvsp[(2) - (2)].node)) != NODE_BLOCK) void_expr((yyvsp[(2) - (2)].node));
			    else {
				NODE *node = (yyvsp[(2) - (2)].node);
				while (node->nd_next) {
				    node = node->nd_next;
				}
				void_expr(node->nd_head);
			    }
			}
			ruby_eval_tree = block_append(ruby_eval_tree, (yyvsp[(2) - (2)].node));
                        top_local_setup();
			class_nest = 0;
		    }
    break;

  case 4:
#line 375 "parse.y"
    {
		        (yyval.node) = (yyvsp[(1) - (4)].node);
			if ((yyvsp[(2) - (4)].node)) {
			    (yyval.node) = NEW_RESCUE((yyvsp[(1) - (4)].node), (yyvsp[(2) - (4)].node), (yyvsp[(3) - (4)].node));
			}
			else if ((yyvsp[(3) - (4)].node)) {
			    rb_warn("else without rescue is useless");
			    (yyval.node) = block_append((yyval.node), (yyvsp[(3) - (4)].node));
			}
			if ((yyvsp[(4) - (4)].node)) {
			    (yyval.node) = NEW_ENSURE((yyval.node), (yyvsp[(4) - (4)].node));
			}
			fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    }
    break;

  case 5:
#line 392 "parse.y"
    {
			void_stmts((yyvsp[(1) - (2)].node));
		        (yyval.node) = (yyvsp[(1) - (2)].node);
		    }
    break;

  case 7:
#line 400 "parse.y"
    {
			(yyval.node) = newline_node((yyvsp[(1) - (1)].node));
		    }
    break;

  case 8:
#line 404 "parse.y"
    {
			(yyval.node) = block_append((yyvsp[(1) - (3)].node), newline_node((yyvsp[(3) - (3)].node)));
		    }
    break;

  case 9:
#line 408 "parse.y"
    {
			(yyval.node) = remove_begin((yyvsp[(2) - (2)].node));
		    }
    break;

  case 10:
#line 413 "parse.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 11:
#line 414 "parse.y"
    {
		        (yyval.node) = NEW_ALIAS((yyvsp[(2) - (4)].node), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 12:
#line 418 "parse.y"
    {
		        (yyval.node) = NEW_VALIAS((yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 13:
#line 422 "parse.y"
    {
			char buf[3];

			sprintf(buf, "$%c", (char)(yyvsp[(3) - (3)].node)->nd_nth);
		        (yyval.node) = NEW_VALIAS((yyvsp[(2) - (3)].id), rb_intern(buf));
		    }
    break;

  case 14:
#line 429 "parse.y"
    {
		        yyerror("can't make alias for the number variables");
		        (yyval.node) = 0;
		    }
    break;

  case 15:
#line 434 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    }
    break;

  case 16:
#line 438 "parse.y"
    {
			(yyval.node) = NEW_IF(cond((yyvsp[(3) - (3)].node)), remove_begin((yyvsp[(1) - (3)].node)), 0);
		        fixpos((yyval.node), (yyvsp[(3) - (3)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
		            (yyval.node)->nd_else = (yyval.node)->nd_body;
		            (yyval.node)->nd_body = 0;
			}
		    }
    break;

  case 17:
#line 447 "parse.y"
    {
			(yyval.node) = NEW_UNLESS(cond((yyvsp[(3) - (3)].node)), remove_begin((yyvsp[(1) - (3)].node)), 0);
		        fixpos((yyval.node), (yyvsp[(3) - (3)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
		            (yyval.node)->nd_body = (yyval.node)->nd_else;
		            (yyval.node)->nd_else = 0;
			}
		    }
    break;

  case 18:
#line 456 "parse.y"
    {
			if ((yyvsp[(1) - (3)].node) && nd_type((yyvsp[(1) - (3)].node)) == NODE_BEGIN) {
			    (yyval.node) = NEW_WHILE(cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node)->nd_body, 0);
			}
			else {
			    (yyval.node) = NEW_WHILE(cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node), 1);
			}
			if (cond_negative(&(yyval.node)->nd_cond)) {
			    nd_set_type((yyval.node), NODE_UNTIL);
			}
		    }
    break;

  case 19:
#line 468 "parse.y"
    {
			if ((yyvsp[(1) - (3)].node) && nd_type((yyvsp[(1) - (3)].node)) == NODE_BEGIN) {
			    (yyval.node) = NEW_UNTIL(cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node)->nd_body, 0);
			}
			else {
			    (yyval.node) = NEW_UNTIL(cond((yyvsp[(3) - (3)].node)), (yyvsp[(1) - (3)].node), 1);
			}
			if (cond_negative(&(yyval.node)->nd_cond)) {
			    nd_set_type((yyval.node), NODE_WHILE);
			}
		    }
    break;

  case 20:
#line 480 "parse.y"
    {
			NODE *resq = NEW_RESBODY(0, remove_begin((yyvsp[(3) - (3)].node)), 0);
			(yyval.node) = NEW_RESCUE(remove_begin((yyvsp[(1) - (3)].node)), resq, 0);
		    }
    break;

  case 21:
#line 485 "parse.y"
    {
			if (in_def || in_single) {
			    yyerror("BEGIN in method");
			}
			local_push(0);
		    }
    break;

  case 22:
#line 492 "parse.y"
    {
			ruby_eval_tree_begin = block_append(ruby_eval_tree_begin,
						            NEW_PREEXE((yyvsp[(4) - (5)].node)));
		        local_pop();
		        (yyval.node) = 0;
		    }
    break;

  case 23:
#line 499 "parse.y"
    {
			if (in_def || in_single) {
			    rb_warn("END in method; use at_exit");
			}

			(yyval.node) = NEW_ITER(0, NEW_POSTEXE(), (yyvsp[(3) - (4)].node));
		    }
    break;

  case 24:
#line 507 "parse.y"
    {
			(yyval.node) = node_assign((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 25:
#line 511 "parse.y"
    {
			value_expr((yyvsp[(3) - (3)].node));
			(yyvsp[(1) - (3)].node)->nd_value = ((yyvsp[(1) - (3)].node)->nd_head) ? NEW_TO_ARY((yyvsp[(3) - (3)].node)) : NEW_ARRAY((yyvsp[(3) - (3)].node));
			(yyval.node) = (yyvsp[(1) - (3)].node);
		    }
    break;

  case 26:
#line 517 "parse.y"
    {
			value_expr((yyvsp[(3) - (3)].node));
			if ((yyvsp[(1) - (3)].node)) {
			    ID vid = (yyvsp[(1) - (3)].node)->nd_vid;
			    if ((yyvsp[(2) - (3)].id) == tOROP) {
				(yyvsp[(1) - (3)].node)->nd_value = (yyvsp[(3) - (3)].node);
				(yyval.node) = NEW_OP_ASGN_OR(gettable(vid), (yyvsp[(1) - (3)].node));
				if (is_asgn_or_id(vid)) {
				    (yyval.node)->nd_aid = vid;
				}
			    }
			    else if ((yyvsp[(2) - (3)].id) == tANDOP) {
				(yyvsp[(1) - (3)].node)->nd_value = (yyvsp[(3) - (3)].node);
				(yyval.node) = NEW_OP_ASGN_AND(gettable(vid), (yyvsp[(1) - (3)].node));
			    }
			    else {
				(yyval.node) = (yyvsp[(1) - (3)].node);
				(yyval.node)->nd_value = call_op(gettable(vid),(yyvsp[(2) - (3)].id),1,(yyvsp[(3) - (3)].node));
			    }
			}
			else {
			    (yyval.node) = 0;
			}
		    }
    break;

  case 27:
#line 542 "parse.y"
    {
                        NODE *args;

			value_expr((yyvsp[(6) - (6)].node));
			if (!(yyvsp[(3) - (6)].node)) (yyvsp[(3) - (6)].node) = NEW_ZARRAY();
			args = arg_concat((yyvsp[(6) - (6)].node), (yyvsp[(3) - (6)].node));
			if ((yyvsp[(5) - (6)].id) == tOROP) {
			    (yyvsp[(5) - (6)].id) = 0;
			}
			else if ((yyvsp[(5) - (6)].id) == tANDOP) {
			    (yyvsp[(5) - (6)].id) = 1;
			}
			(yyval.node) = NEW_OP_ASGN1((yyvsp[(1) - (6)].node), (yyvsp[(5) - (6)].id), args);
		        fixpos((yyval.node), (yyvsp[(1) - (6)].node));
		    }
    break;

  case 28:
#line 558 "parse.y"
    {
			value_expr((yyvsp[(5) - (5)].node));
			if ((yyvsp[(4) - (5)].id) == tOROP) {
			    (yyvsp[(4) - (5)].id) = 0;
			}
			else if ((yyvsp[(4) - (5)].id) == tANDOP) {
			    (yyvsp[(4) - (5)].id) = 1;
			}
			(yyval.node) = NEW_OP_ASGN2((yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (5)].node));
		    }
    break;

  case 29:
#line 570 "parse.y"
    {
			value_expr((yyvsp[(5) - (5)].node));
			if ((yyvsp[(4) - (5)].id) == tOROP) {
			    (yyvsp[(4) - (5)].id) = 0;
			}
			else if ((yyvsp[(4) - (5)].id) == tANDOP) {
			    (yyvsp[(4) - (5)].id) = 1;
			}
			(yyval.node) = NEW_OP_ASGN2((yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (5)].node));
		    }
    break;

  case 30:
#line 582 "parse.y"
    {
			value_expr((yyvsp[(5) - (5)].node));
			if ((yyvsp[(4) - (5)].id) == tOROP) {
			    (yyvsp[(4) - (5)].id) = 0;
			}
			else if ((yyvsp[(4) - (5)].id) == tANDOP) {
			    (yyvsp[(4) - (5)].id) = 1;
			}
			(yyval.node) = NEW_OP_ASGN2((yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (5)].node));
		    }
    break;

  case 31:
#line 594 "parse.y"
    {
		        rb_backref_error((yyvsp[(1) - (3)].node));
			(yyval.node) = 0;
		    }
    break;

  case 32:
#line 599 "parse.y"
    {
			(yyval.node) = node_assign((yyvsp[(1) - (3)].node), NEW_SVALUE((yyvsp[(3) - (3)].node)));
		    }
    break;

  case 33:
#line 603 "parse.y"
    {
			(yyvsp[(1) - (3)].node)->nd_value = ((yyvsp[(1) - (3)].node)->nd_head) ? NEW_TO_ARY((yyvsp[(3) - (3)].node)) : NEW_ARRAY((yyvsp[(3) - (3)].node));
			(yyval.node) = (yyvsp[(1) - (3)].node);
		    }
    break;

  case 34:
#line 608 "parse.y"
    {
			(yyvsp[(1) - (3)].node)->nd_value = (yyvsp[(3) - (3)].node);
			(yyval.node) = (yyvsp[(1) - (3)].node);
		    }
    break;

  case 37:
#line 617 "parse.y"
    {
			(yyval.node) = logop(NODE_AND, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 38:
#line 621 "parse.y"
    {
			(yyval.node) = logop(NODE_OR, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 39:
#line 625 "parse.y"
    {
			(yyval.node) = NEW_NOT(cond((yyvsp[(2) - (2)].node)));
		    }
    break;

  case 40:
#line 629 "parse.y"
    {
			(yyval.node) = NEW_NOT(cond((yyvsp[(2) - (2)].node)));
		    }
    break;

  case 42:
#line 636 "parse.y"
    {
			value_expr((yyval.node));
			(yyval.node) = (yyvsp[(1) - (1)].node);
		    }
    break;

  case 45:
#line 645 "parse.y"
    {
			(yyval.node) = NEW_RETURN(ret_args((yyvsp[(2) - (2)].node)));
		    }
    break;

  case 46:
#line 649 "parse.y"
    {
			(yyval.node) = NEW_BREAK(ret_args((yyvsp[(2) - (2)].node)));
		    }
    break;

  case 47:
#line 653 "parse.y"
    {
			(yyval.node) = NEW_NEXT(ret_args((yyvsp[(2) - (2)].node)));
		    }
    break;

  case 49:
#line 660 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 50:
#line 664 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 51:
#line 670 "parse.y"
    {
			(yyval.vars) = dyna_push();
			(yyvsp[(1) - (1)].num) = ruby_sourceline;
		    }
    break;

  case 52:
#line 674 "parse.y"
    {(yyval.vars) = ruby_dyna_vars;}
    break;

  case 53:
#line 677 "parse.y"
    {
			(yyval.node) = NEW_ITER((yyvsp[(3) - (6)].node), 0, dyna_init((yyvsp[(5) - (6)].node), (yyvsp[(4) - (6)].vars)));
			nd_set_line((yyval.node), (yyvsp[(1) - (6)].num));
			dyna_pop((yyvsp[(2) - (6)].vars));
		    }
    break;

  case 54:
#line 685 "parse.y"
    {
			(yyval.node) = new_fcall((yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (2)].node));
		   }
    break;

  case 55:
#line 690 "parse.y"
    {
			(yyval.node) = new_fcall((yyvsp[(1) - (3)].id), (yyvsp[(2) - (3)].node));
			if ((yyvsp[(3) - (3)].node)) {
			    if (nd_type((yyval.node)) == NODE_BLOCK_PASS) {
				rb_compile_error("both block arg and actual block given");
			    }
			    (yyvsp[(3) - (3)].node)->nd_iter = (yyval.node);
			    (yyval.node) = (yyvsp[(3) - (3)].node);
			}
		        fixpos((yyval.node), (yyvsp[(2) - (3)].node));
		   }
    break;

  case 56:
#line 702 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    }
    break;

  case 57:
#line 707 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].node));
			if ((yyvsp[(5) - (5)].node)) {
			    if (nd_type((yyval.node)) == NODE_BLOCK_PASS) {
				rb_compile_error("both block arg and actual block given");
			    }
			    (yyvsp[(5) - (5)].node)->nd_iter = (yyval.node);
			    (yyval.node) = (yyvsp[(5) - (5)].node);
			}
		        fixpos((yyval.node), (yyvsp[(1) - (5)].node));
		   }
    break;

  case 58:
#line 719 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    }
    break;

  case 59:
#line 724 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].node));
			if ((yyvsp[(5) - (5)].node)) {
			    if (nd_type((yyval.node)) == NODE_BLOCK_PASS) {
				rb_compile_error("both block arg and actual block given");
			    }
			    (yyvsp[(5) - (5)].node)->nd_iter = (yyval.node);
			    (yyval.node) = (yyvsp[(5) - (5)].node);
			}
		        fixpos((yyval.node), (yyvsp[(1) - (5)].node));
		   }
    break;

  case 60:
#line 736 "parse.y"
    {
			(yyval.node) = new_super((yyvsp[(2) - (2)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 61:
#line 741 "parse.y"
    {
			(yyval.node) = new_yield((yyvsp[(2) - (2)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 63:
#line 749 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    }
    break;

  case 65:
#line 756 "parse.y"
    {
			(yyval.node) = NEW_MASGN(NEW_LIST((yyvsp[(2) - (3)].node)), 0);
		    }
    break;

  case 66:
#line 762 "parse.y"
    {
			(yyval.node) = NEW_MASGN((yyvsp[(1) - (1)].node), 0);
		    }
    break;

  case 67:
#line 766 "parse.y"
    {
			(yyval.node) = NEW_MASGN(list_append((yyvsp[(1) - (2)].node),(yyvsp[(2) - (2)].node)), 0);
		    }
    break;

  case 68:
#line 770 "parse.y"
    {
			(yyval.node) = NEW_MASGN((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 69:
#line 774 "parse.y"
    {
			(yyval.node) = NEW_MASGN((yyvsp[(1) - (2)].node), -1);
		    }
    break;

  case 70:
#line 778 "parse.y"
    {
			(yyval.node) = NEW_MASGN(0, (yyvsp[(2) - (2)].node));
		    }
    break;

  case 71:
#line 782 "parse.y"
    {
			(yyval.node) = NEW_MASGN(0, -1);
		    }
    break;

  case 73:
#line 789 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    }
    break;

  case 74:
#line 795 "parse.y"
    {
			(yyval.node) = NEW_LIST((yyvsp[(1) - (2)].node));
		    }
    break;

  case 75:
#line 799 "parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].node));
		    }
    break;

  case 76:
#line 805 "parse.y"
    {
			(yyval.node) = assignable((yyvsp[(1) - (1)].id), 0);
		    }
    break;

  case 77:
#line 809 "parse.y"
    {
			(yyval.node) = aryset((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node));
		    }
    break;

  case 78:
#line 813 "parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 79:
#line 817 "parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 80:
#line 821 "parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 81:
#line 825 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.node) = NEW_CDECL(0, 0, NEW_COLON2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id)));
		    }
    break;

  case 82:
#line 831 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.node) = NEW_CDECL(0, 0, NEW_COLON3((yyvsp[(2) - (2)].id)));
		    }
    break;

  case 83:
#line 837 "parse.y"
    {
		        rb_backref_error((yyvsp[(1) - (1)].node));
			(yyval.node) = 0;
		    }
    break;

  case 84:
#line 844 "parse.y"
    {
			(yyval.node) = assignable((yyvsp[(1) - (1)].id), 0);
		    }
    break;

  case 85:
#line 848 "parse.y"
    {
			(yyval.node) = aryset((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node));
		    }
    break;

  case 86:
#line 852 "parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 87:
#line 856 "parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 88:
#line 860 "parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 89:
#line 864 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.node) = NEW_CDECL(0, 0, NEW_COLON2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id)));
		    }
    break;

  case 90:
#line 870 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.node) = NEW_CDECL(0, 0, NEW_COLON3((yyvsp[(2) - (2)].id)));
		    }
    break;

  case 91:
#line 876 "parse.y"
    {
		        rb_backref_error((yyvsp[(1) - (1)].node));
			(yyval.node) = 0;
		    }
    break;

  case 92:
#line 883 "parse.y"
    {
			yyerror("class/module name must be CONSTANT");
		    }
    break;

  case 94:
#line 890 "parse.y"
    {
			(yyval.node) = NEW_COLON3((yyvsp[(2) - (2)].id));
		    }
    break;

  case 95:
#line 894 "parse.y"
    {
			(yyval.node) = NEW_COLON2(0, (yyval.node));
		    }
    break;

  case 96:
#line 898 "parse.y"
    {
			(yyval.node) = NEW_COLON2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 100:
#line 907 "parse.y"
    {
			lex_state = EXPR_END;
			(yyval.id) = (yyvsp[(1) - (1)].id);
		    }
    break;

  case 101:
#line 912 "parse.y"
    {
			lex_state = EXPR_END;
			(yyval.id) = (yyvsp[(1) - (1)].id);
		    }
    break;

  case 104:
#line 923 "parse.y"
    {
			(yyval.node) = NEW_LIT(ID2SYM((yyvsp[(1) - (1)].id)));
		    }
    break;

  case 106:
#line 930 "parse.y"
    {
			(yyval.node) = NEW_UNDEF((yyvsp[(1) - (1)].node));
		    }
    break;

  case 107:
#line 933 "parse.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 108:
#line 934 "parse.y"
    {
			(yyval.node) = block_append((yyvsp[(1) - (4)].node), NEW_UNDEF((yyvsp[(4) - (4)].node)));
		    }
    break;

  case 109:
#line 939 "parse.y"
    { (yyval.id) = '|'; }
    break;

  case 110:
#line 940 "parse.y"
    { (yyval.id) = '^'; }
    break;

  case 111:
#line 941 "parse.y"
    { (yyval.id) = '&'; }
    break;

  case 112:
#line 942 "parse.y"
    { (yyval.id) = tCMP; }
    break;

  case 113:
#line 943 "parse.y"
    { (yyval.id) = tEQ; }
    break;

  case 114:
#line 944 "parse.y"
    { (yyval.id) = tEQQ; }
    break;

  case 115:
#line 945 "parse.y"
    { (yyval.id) = tMATCH; }
    break;

  case 116:
#line 946 "parse.y"
    { (yyval.id) = '>'; }
    break;

  case 117:
#line 947 "parse.y"
    { (yyval.id) = tGEQ; }
    break;

  case 118:
#line 948 "parse.y"
    { (yyval.id) = '<'; }
    break;

  case 119:
#line 949 "parse.y"
    { (yyval.id) = tLEQ; }
    break;

  case 120:
#line 950 "parse.y"
    { (yyval.id) = tLSHFT; }
    break;

  case 121:
#line 951 "parse.y"
    { (yyval.id) = tRSHFT; }
    break;

  case 122:
#line 952 "parse.y"
    { (yyval.id) = '+'; }
    break;

  case 123:
#line 953 "parse.y"
    { (yyval.id) = '-'; }
    break;

  case 124:
#line 954 "parse.y"
    { (yyval.id) = '*'; }
    break;

  case 125:
#line 955 "parse.y"
    { (yyval.id) = '*'; }
    break;

  case 126:
#line 956 "parse.y"
    { (yyval.id) = '/'; }
    break;

  case 127:
#line 957 "parse.y"
    { (yyval.id) = '%'; }
    break;

  case 128:
#line 958 "parse.y"
    { (yyval.id) = tPOW; }
    break;

  case 129:
#line 959 "parse.y"
    { (yyval.id) = '~'; }
    break;

  case 130:
#line 960 "parse.y"
    { (yyval.id) = tUPLUS; }
    break;

  case 131:
#line 961 "parse.y"
    { (yyval.id) = tUMINUS; }
    break;

  case 132:
#line 962 "parse.y"
    { (yyval.id) = tAREF; }
    break;

  case 133:
#line 963 "parse.y"
    { (yyval.id) = tASET; }
    break;

  case 134:
#line 964 "parse.y"
    { (yyval.id) = '`'; }
    break;

  case 175:
#line 977 "parse.y"
    {
			(yyval.node) = node_assign((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 176:
#line 981 "parse.y"
    {
			(yyval.node) = node_assign((yyvsp[(1) - (5)].node), NEW_RESCUE((yyvsp[(3) - (5)].node), NEW_RESBODY(0,(yyvsp[(5) - (5)].node),0), 0));
		    }
    break;

  case 177:
#line 985 "parse.y"
    {
			value_expr((yyvsp[(3) - (3)].node));
			if ((yyvsp[(1) - (3)].node)) {
			    ID vid = (yyvsp[(1) - (3)].node)->nd_vid;
			    if ((yyvsp[(2) - (3)].id) == tOROP) {
				(yyvsp[(1) - (3)].node)->nd_value = (yyvsp[(3) - (3)].node);
				(yyval.node) = NEW_OP_ASGN_OR(gettable(vid), (yyvsp[(1) - (3)].node));
				if (is_asgn_or_id(vid)) {
				    (yyval.node)->nd_aid = vid;
				}
			    }
			    else if ((yyvsp[(2) - (3)].id) == tANDOP) {
				(yyvsp[(1) - (3)].node)->nd_value = (yyvsp[(3) - (3)].node);
				(yyval.node) = NEW_OP_ASGN_AND(gettable(vid), (yyvsp[(1) - (3)].node));
			    }
			    else {
				(yyval.node) = (yyvsp[(1) - (3)].node);
				(yyval.node)->nd_value = call_op(gettable(vid),(yyvsp[(2) - (3)].id),1,(yyvsp[(3) - (3)].node));
			    }
			}
			else {
			    (yyval.node) = 0;
			}
		    }
    break;

  case 178:
#line 1010 "parse.y"
    {
                        NODE *args;

			value_expr((yyvsp[(6) - (6)].node));
			if (!(yyvsp[(3) - (6)].node)) (yyvsp[(3) - (6)].node) = NEW_ZARRAY();
			args = arg_concat((yyvsp[(6) - (6)].node), (yyvsp[(3) - (6)].node));
			if ((yyvsp[(5) - (6)].id) == tOROP) {
			    (yyvsp[(5) - (6)].id) = 0;
			}
			else if ((yyvsp[(5) - (6)].id) == tANDOP) {
			    (yyvsp[(5) - (6)].id) = 1;
			}
			(yyval.node) = NEW_OP_ASGN1((yyvsp[(1) - (6)].node), (yyvsp[(5) - (6)].id), args);
		        fixpos((yyval.node), (yyvsp[(1) - (6)].node));
		    }
    break;

  case 179:
#line 1026 "parse.y"
    {
			value_expr((yyvsp[(5) - (5)].node));
			if ((yyvsp[(4) - (5)].id) == tOROP) {
			    (yyvsp[(4) - (5)].id) = 0;
			}
			else if ((yyvsp[(4) - (5)].id) == tANDOP) {
			    (yyvsp[(4) - (5)].id) = 1;
			}
			(yyval.node) = NEW_OP_ASGN2((yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (5)].node));
		    }
    break;

  case 180:
#line 1038 "parse.y"
    {
			value_expr((yyvsp[(5) - (5)].node));
			if ((yyvsp[(4) - (5)].id) == tOROP) {
			    (yyvsp[(4) - (5)].id) = 0;
			}
			else if ((yyvsp[(4) - (5)].id) == tANDOP) {
			    (yyvsp[(4) - (5)].id) = 1;
			}
			(yyval.node) = NEW_OP_ASGN2((yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (5)].node));
		    }
    break;

  case 181:
#line 1050 "parse.y"
    {
			value_expr((yyvsp[(5) - (5)].node));
			if ((yyvsp[(4) - (5)].id) == tOROP) {
			    (yyvsp[(4) - (5)].id) = 0;
			}
			else if ((yyvsp[(4) - (5)].id) == tANDOP) {
			    (yyvsp[(4) - (5)].id) = 1;
			}
			(yyval.node) = NEW_OP_ASGN2((yyvsp[(1) - (5)].node), (yyvsp[(3) - (5)].id), (yyvsp[(4) - (5)].id), (yyvsp[(5) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (5)].node));
		    }
    break;

  case 182:
#line 1062 "parse.y"
    {
			yyerror("constant re-assignment");
			(yyval.node) = 0;
		    }
    break;

  case 183:
#line 1067 "parse.y"
    {
			yyerror("constant re-assignment");
			(yyval.node) = 0;
		    }
    break;

  case 184:
#line 1072 "parse.y"
    {
		        rb_backref_error((yyvsp[(1) - (3)].node));
			(yyval.node) = 0;
		    }
    break;

  case 185:
#line 1077 "parse.y"
    {
			value_expr((yyvsp[(1) - (3)].node));
			value_expr((yyvsp[(3) - (3)].node));
			if (nd_type((yyvsp[(1) - (3)].node)) == NODE_LIT && FIXNUM_P((yyvsp[(1) - (3)].node)->nd_lit) &&
			    nd_type((yyvsp[(3) - (3)].node)) == NODE_LIT && FIXNUM_P((yyvsp[(3) - (3)].node)->nd_lit)) {
			    (yyvsp[(1) - (3)].node)->nd_lit = rb_range_new((yyvsp[(1) - (3)].node)->nd_lit, (yyvsp[(3) - (3)].node)->nd_lit, Qfalse);
			    (yyval.node) = (yyvsp[(1) - (3)].node);
			}
			else {
			    (yyval.node) = NEW_DOT2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
			}
		    }
    break;

  case 186:
#line 1090 "parse.y"
    {
			value_expr((yyvsp[(1) - (3)].node));
			value_expr((yyvsp[(3) - (3)].node));
			if (nd_type((yyvsp[(1) - (3)].node)) == NODE_LIT && FIXNUM_P((yyvsp[(1) - (3)].node)->nd_lit) &&
			    nd_type((yyvsp[(3) - (3)].node)) == NODE_LIT && FIXNUM_P((yyvsp[(3) - (3)].node)->nd_lit)) {
			    (yyvsp[(1) - (3)].node)->nd_lit = rb_range_new((yyvsp[(1) - (3)].node)->nd_lit, (yyvsp[(3) - (3)].node)->nd_lit, Qtrue);
			    (yyval.node) = (yyvsp[(1) - (3)].node);
			}
			else {
			    (yyval.node) = NEW_DOT3((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
			}
		    }
    break;

  case 187:
#line 1103 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '+', 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 188:
#line 1107 "parse.y"
    {
		        (yyval.node) = call_op((yyvsp[(1) - (3)].node), '-', 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 189:
#line 1111 "parse.y"
    {
		        (yyval.node) = call_op((yyvsp[(1) - (3)].node), '*', 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 190:
#line 1115 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '/', 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 191:
#line 1119 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '%', 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 192:
#line 1123 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tPOW, 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 193:
#line 1127 "parse.y"
    {
			(yyval.node) = call_op(call_op((yyvsp[(2) - (4)].node), tPOW, 1, (yyvsp[(4) - (4)].node)), tUMINUS, 0, 0);
		    }
    break;

  case 194:
#line 1131 "parse.y"
    {
			(yyval.node) = call_op(call_op((yyvsp[(2) - (4)].node), tPOW, 1, (yyvsp[(4) - (4)].node)), tUMINUS, 0, 0);
		    }
    break;

  case 195:
#line 1135 "parse.y"
    {
			if ((yyvsp[(2) - (2)].node) && nd_type((yyvsp[(2) - (2)].node)) == NODE_LIT) {
			    (yyval.node) = (yyvsp[(2) - (2)].node);
			}
			else {
			    (yyval.node) = call_op((yyvsp[(2) - (2)].node), tUPLUS, 0, 0);
			}
		    }
    break;

  case 196:
#line 1144 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(2) - (2)].node), tUMINUS, 0, 0);
		    }
    break;

  case 197:
#line 1148 "parse.y"
    {
		        (yyval.node) = call_op((yyvsp[(1) - (3)].node), '|', 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 198:
#line 1152 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '^', 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 199:
#line 1156 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '&', 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 200:
#line 1160 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tCMP, 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 201:
#line 1164 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '>', 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 202:
#line 1168 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tGEQ, 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 203:
#line 1172 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '<', 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 204:
#line 1176 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tLEQ, 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 205:
#line 1180 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tEQ, 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 206:
#line 1184 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tEQQ, 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 207:
#line 1188 "parse.y"
    {
			(yyval.node) = NEW_NOT(call_op((yyvsp[(1) - (3)].node), tEQ, 1, (yyvsp[(3) - (3)].node)));
		    }
    break;

  case 208:
#line 1192 "parse.y"
    {
			(yyval.node) = match_gen((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 209:
#line 1196 "parse.y"
    {
			(yyval.node) = NEW_NOT(match_gen((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node)));
		    }
    break;

  case 210:
#line 1200 "parse.y"
    {
			(yyval.node) = NEW_NOT(cond((yyvsp[(2) - (2)].node)));
		    }
    break;

  case 211:
#line 1204 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(2) - (2)].node), '~', 0, 0);
		    }
    break;

  case 212:
#line 1208 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tLSHFT, 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 213:
#line 1212 "parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tRSHFT, 1, (yyvsp[(3) - (3)].node));
		    }
    break;

  case 214:
#line 1216 "parse.y"
    {
			(yyval.node) = logop(NODE_AND, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 215:
#line 1220 "parse.y"
    {
			(yyval.node) = logop(NODE_OR, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 216:
#line 1223 "parse.y"
    {in_defined = 1;}
    break;

  case 217:
#line 1224 "parse.y"
    {
		        in_defined = 0;
			(yyval.node) = NEW_DEFINED((yyvsp[(4) - (4)].node));
		    }
    break;

  case 218:
#line 1229 "parse.y"
    {
			(yyval.node) = NEW_IF(cond((yyvsp[(1) - (5)].node)), (yyvsp[(3) - (5)].node), (yyvsp[(5) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (5)].node));
		    }
    break;

  case 219:
#line 1234 "parse.y"
    {
			(yyval.node) = (yyvsp[(1) - (1)].node);
		    }
    break;

  case 220:
#line 1240 "parse.y"
    {
			value_expr((yyvsp[(1) - (1)].node));
			(yyval.node) = (yyvsp[(1) - (1)].node);
		    }
    break;

  case 222:
#line 1248 "parse.y"
    {
		        rb_warn("parenthesize argument(s) for future version");
			(yyval.node) = NEW_LIST((yyvsp[(1) - (2)].node));
		    }
    break;

  case 223:
#line 1253 "parse.y"
    {
			(yyval.node) = (yyvsp[(1) - (2)].node);
		    }
    break;

  case 224:
#line 1257 "parse.y"
    {
			value_expr((yyvsp[(4) - (5)].node));
			(yyval.node) = arg_concat((yyvsp[(1) - (5)].node), (yyvsp[(4) - (5)].node));
		    }
    break;

  case 225:
#line 1262 "parse.y"
    {
			(yyval.node) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].node)));
		    }
    break;

  case 226:
#line 1266 "parse.y"
    {
			value_expr((yyvsp[(2) - (3)].node));
			(yyval.node) = NEW_NEWLINE(NEW_SPLAT((yyvsp[(2) - (3)].node)));
		    }
    break;

  case 227:
#line 1273 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    }
    break;

  case 228:
#line 1277 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (4)].node);
		    }
    break;

  case 229:
#line 1281 "parse.y"
    {
		        rb_warn("parenthesize argument for future version");
			(yyval.node) = NEW_LIST((yyvsp[(2) - (4)].node));
		    }
    break;

  case 230:
#line 1286 "parse.y"
    {
		        rb_warn("parenthesize argument for future version");
			(yyval.node) = list_append((yyvsp[(2) - (6)].node), (yyvsp[(4) - (6)].node));
		    }
    break;

  case 233:
#line 1297 "parse.y"
    {
		        rb_warn("parenthesize argument(s) for future version");
			(yyval.node) = NEW_LIST((yyvsp[(1) - (1)].node));
		    }
    break;

  case 234:
#line 1302 "parse.y"
    {
			(yyval.node) = arg_blk_pass((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 235:
#line 1306 "parse.y"
    {
			(yyval.node) = arg_concat((yyvsp[(1) - (5)].node), (yyvsp[(4) - (5)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(5) - (5)].node));
		    }
    break;

  case 236:
#line 1311 "parse.y"
    {
			(yyval.node) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].node)));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 237:
#line 1316 "parse.y"
    {
			(yyval.node) = arg_concat(NEW_LIST(NEW_HASH((yyvsp[(1) - (5)].node))), (yyvsp[(4) - (5)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(5) - (5)].node));
		    }
    break;

  case 238:
#line 1321 "parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (4)].node), NEW_HASH((yyvsp[(3) - (4)].node)));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 239:
#line 1326 "parse.y"
    {
			value_expr((yyvsp[(6) - (7)].node));
			(yyval.node) = arg_concat(list_append((yyvsp[(1) - (7)].node), NEW_HASH((yyvsp[(3) - (7)].node))), (yyvsp[(6) - (7)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(7) - (7)].node));
		    }
    break;

  case 240:
#line 1332 "parse.y"
    {
			(yyval.node) = arg_blk_pass(NEW_SPLAT((yyvsp[(2) - (3)].node)), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 242:
#line 1339 "parse.y"
    {
			(yyval.node) = arg_blk_pass(list_concat(NEW_LIST((yyvsp[(1) - (4)].node)),(yyvsp[(3) - (4)].node)), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 243:
#line 1343 "parse.y"
    {
                        (yyval.node) = arg_blk_pass((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
                    }
    break;

  case 244:
#line 1347 "parse.y"
    {
			(yyval.node) = arg_concat(NEW_LIST((yyvsp[(1) - (5)].node)), (yyvsp[(4) - (5)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(5) - (5)].node));
		    }
    break;

  case 245:
#line 1352 "parse.y"
    {
                       (yyval.node) = arg_concat(list_concat(NEW_LIST((yyvsp[(1) - (7)].node)),(yyvsp[(3) - (7)].node)), (yyvsp[(6) - (7)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(7) - (7)].node));
		    }
    break;

  case 246:
#line 1357 "parse.y"
    {
			(yyval.node) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].node)));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 247:
#line 1362 "parse.y"
    {
			(yyval.node) = arg_concat(NEW_LIST(NEW_HASH((yyvsp[(1) - (5)].node))), (yyvsp[(4) - (5)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(5) - (5)].node));
		    }
    break;

  case 248:
#line 1367 "parse.y"
    {
			(yyval.node) = list_append(NEW_LIST((yyvsp[(1) - (4)].node)), NEW_HASH((yyvsp[(3) - (4)].node)));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 249:
#line 1372 "parse.y"
    {
			(yyval.node) = list_append(list_concat(NEW_LIST((yyvsp[(1) - (6)].node)),(yyvsp[(3) - (6)].node)), NEW_HASH((yyvsp[(5) - (6)].node)));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(6) - (6)].node));
		    }
    break;

  case 250:
#line 1377 "parse.y"
    {
			(yyval.node) = arg_concat(list_append(NEW_LIST((yyvsp[(1) - (7)].node)), NEW_HASH((yyvsp[(3) - (7)].node))), (yyvsp[(6) - (7)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(7) - (7)].node));
		    }
    break;

  case 251:
#line 1382 "parse.y"
    {
			(yyval.node) = arg_concat(list_append(list_concat(NEW_LIST((yyvsp[(1) - (9)].node)), (yyvsp[(3) - (9)].node)), NEW_HASH((yyvsp[(5) - (9)].node))), (yyvsp[(8) - (9)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(9) - (9)].node));
		    }
    break;

  case 252:
#line 1387 "parse.y"
    {
			(yyval.node) = arg_blk_pass(NEW_SPLAT((yyvsp[(2) - (3)].node)), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 254:
#line 1393 "parse.y"
    {
			(yyval.num) = cmdarg_stack;
			CMDARG_PUSH(1);
		    }
    break;

  case 255:
#line 1398 "parse.y"
    {
			/* CMDARG_POP() */
		        cmdarg_stack = (yyvsp[(1) - (2)].num);
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    }
    break;

  case 257:
#line 1406 "parse.y"
    {lex_state = EXPR_ENDARG;}
    break;

  case 258:
#line 1407 "parse.y"
    {
		        rb_warn("don't put space before argument parentheses");
			(yyval.node) = 0;
		    }
    break;

  case 259:
#line 1411 "parse.y"
    {lex_state = EXPR_ENDARG;}
    break;

  case 260:
#line 1412 "parse.y"
    {
		        rb_warn("don't put space before argument parentheses");
			(yyval.node) = (yyvsp[(2) - (4)].node);
		    }
    break;

  case 261:
#line 1419 "parse.y"
    {
			(yyval.node) = NEW_BLOCK_PASS((yyvsp[(2) - (2)].node));
		    }
    break;

  case 262:
#line 1425 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    }
    break;

  case 264:
#line 1432 "parse.y"
    {
			(yyval.node) = NEW_LIST((yyvsp[(1) - (1)].node));
		    }
    break;

  case 265:
#line 1436 "parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 266:
#line 1442 "parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 267:
#line 1446 "parse.y"
    {
			(yyval.node) = arg_concat((yyvsp[(1) - (4)].node), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 268:
#line 1450 "parse.y"
    {
			(yyval.node) = NEW_SPLAT((yyvsp[(2) - (2)].node));
		    }
    break;

  case 277:
#line 1464 "parse.y"
    {
			(yyval.node) = NEW_FCALL((yyvsp[(1) - (1)].id), 0);
		    }
    break;

  case 278:
#line 1468 "parse.y"
    {
			(yyvsp[(1) - (1)].num) = ruby_sourceline;
		    }
    break;

  case 279:
#line 1473 "parse.y"
    {
			if ((yyvsp[(3) - (4)].node) == NULL)
			    (yyval.node) = NEW_NIL();
			else
			    (yyval.node) = NEW_BEGIN((yyvsp[(3) - (4)].node));
			nd_set_line((yyval.node), (yyvsp[(1) - (4)].num));
		    }
    break;

  case 280:
#line 1480 "parse.y"
    {lex_state = EXPR_ENDARG;}
    break;

  case 281:
#line 1481 "parse.y"
    {
		        rb_warning("(...) interpreted as grouped expression");
			(yyval.node) = (yyvsp[(2) - (5)].node);
		    }
    break;

  case 282:
#line 1486 "parse.y"
    {
			if (!(yyvsp[(2) - (3)].node)) (yyval.node) = NEW_NIL();
			else (yyval.node) = (yyvsp[(2) - (3)].node);
		    }
    break;

  case 283:
#line 1491 "parse.y"
    {
			(yyval.node) = NEW_COLON2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    }
    break;

  case 284:
#line 1495 "parse.y"
    {
			(yyval.node) = NEW_COLON3((yyvsp[(2) - (2)].id));
		    }
    break;

  case 285:
#line 1499 "parse.y"
    {
			if ((yyvsp[(1) - (4)].node) && nd_type((yyvsp[(1) - (4)].node)) == NODE_SELF)
			    (yyval.node) = NEW_FCALL(tAREF, (yyvsp[(3) - (4)].node));
			else
			    (yyval.node) = NEW_CALL((yyvsp[(1) - (4)].node), tAREF, (yyvsp[(3) - (4)].node));
			fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    }
    break;

  case 286:
#line 1507 "parse.y"
    {
		        if ((yyvsp[(2) - (3)].node) == 0) {
			    (yyval.node) = NEW_ZARRAY(); /* zero length array*/
			}
			else {
			    (yyval.node) = (yyvsp[(2) - (3)].node);
			}
		    }
    break;

  case 287:
#line 1516 "parse.y"
    {
			(yyval.node) = NEW_HASH((yyvsp[(2) - (3)].node));
		    }
    break;

  case 288:
#line 1520 "parse.y"
    {
			(yyval.node) = NEW_RETURN(0);
		    }
    break;

  case 289:
#line 1524 "parse.y"
    {
			(yyval.node) = new_yield((yyvsp[(3) - (4)].node));
		    }
    break;

  case 290:
#line 1528 "parse.y"
    {
			(yyval.node) = NEW_YIELD(0, Qfalse);
		    }
    break;

  case 291:
#line 1532 "parse.y"
    {
			(yyval.node) = NEW_YIELD(0, Qfalse);
		    }
    break;

  case 292:
#line 1535 "parse.y"
    {in_defined = 1;}
    break;

  case 293:
#line 1536 "parse.y"
    {
		        in_defined = 0;
			(yyval.node) = NEW_DEFINED((yyvsp[(5) - (6)].node));
		    }
    break;

  case 294:
#line 1541 "parse.y"
    {
			(yyvsp[(2) - (2)].node)->nd_iter = NEW_FCALL((yyvsp[(1) - (2)].id), 0);
			(yyval.node) = (yyvsp[(2) - (2)].node);
			fixpos((yyvsp[(2) - (2)].node)->nd_iter, (yyvsp[(2) - (2)].node));
		    }
    break;

  case 296:
#line 1548 "parse.y"
    {
			if ((yyvsp[(1) - (2)].node) && nd_type((yyvsp[(1) - (2)].node)) == NODE_BLOCK_PASS) {
			    rb_compile_error("both block arg and actual block given");
			}
			(yyvsp[(2) - (2)].node)->nd_iter = (yyvsp[(1) - (2)].node);
			(yyval.node) = (yyvsp[(2) - (2)].node);
		        fixpos((yyval.node), (yyvsp[(1) - (2)].node));
		    }
    break;

  case 297:
#line 1560 "parse.y"
    {
			(yyval.node) = NEW_IF(cond((yyvsp[(2) - (6)].node)), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (6)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
		            NODE *tmp = (yyval.node)->nd_body;
		            (yyval.node)->nd_body = (yyval.node)->nd_else;
		            (yyval.node)->nd_else = tmp;
			}
		    }
    break;

  case 298:
#line 1573 "parse.y"
    {
			(yyval.node) = NEW_UNLESS(cond((yyvsp[(2) - (6)].node)), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (6)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
		            NODE *tmp = (yyval.node)->nd_body;
		            (yyval.node)->nd_body = (yyval.node)->nd_else;
		            (yyval.node)->nd_else = tmp;
			}
		    }
    break;

  case 299:
#line 1582 "parse.y"
    {COND_PUSH(1);}
    break;

  case 300:
#line 1582 "parse.y"
    {COND_POP();}
    break;

  case 301:
#line 1585 "parse.y"
    {
			(yyval.node) = NEW_WHILE(cond((yyvsp[(3) - (7)].node)), (yyvsp[(6) - (7)].node), 1);
		        fixpos((yyval.node), (yyvsp[(3) - (7)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
			    nd_set_type((yyval.node), NODE_UNTIL);
			}
		    }
    break;

  case 302:
#line 1592 "parse.y"
    {COND_PUSH(1);}
    break;

  case 303:
#line 1592 "parse.y"
    {COND_POP();}
    break;

  case 304:
#line 1595 "parse.y"
    {
			(yyval.node) = NEW_UNTIL(cond((yyvsp[(3) - (7)].node)), (yyvsp[(6) - (7)].node), 1);
		        fixpos((yyval.node), (yyvsp[(3) - (7)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
			    nd_set_type((yyval.node), NODE_WHILE);
			}
		    }
    break;

  case 305:
#line 1605 "parse.y"
    {
			(yyval.node) = NEW_CASE((yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (5)].node));
		    }
    break;

  case 306:
#line 1610 "parse.y"
    {
			(yyval.node) = (yyvsp[(3) - (4)].node);
		    }
    break;

  case 307:
#line 1614 "parse.y"
    {
			(yyval.node) = (yyvsp[(4) - (5)].node);
		    }
    break;

  case 308:
#line 1617 "parse.y"
    {COND_PUSH(1);}
    break;

  case 309:
#line 1617 "parse.y"
    {COND_POP();}
    break;

  case 310:
#line 1620 "parse.y"
    {
			(yyval.node) = NEW_FOR((yyvsp[(2) - (9)].node), (yyvsp[(5) - (9)].node), (yyvsp[(8) - (9)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (9)].node));
		    }
    break;

  case 311:
#line 1625 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("class definition in method body");
			class_nest++;
			local_push(0);
		        (yyval.num) = ruby_sourceline;
		    }
    break;

  case 312:
#line 1634 "parse.y"
    {
		        (yyval.node) = NEW_CLASS((yyvsp[(2) - (6)].node), (yyvsp[(5) - (6)].node), (yyvsp[(3) - (6)].node));
		        nd_set_line((yyval.node), (yyvsp[(4) - (6)].num));
		        local_pop();
			class_nest--;
		    }
    break;

  case 313:
#line 1641 "parse.y"
    {
			(yyval.num) = in_def;
		        in_def = 0;
		    }
    break;

  case 314:
#line 1646 "parse.y"
    {
		        (yyval.num) = in_single;
		        in_single = 0;
			class_nest++;
			local_push(0);
		    }
    break;

  case 315:
#line 1654 "parse.y"
    {
		        (yyval.node) = NEW_SCLASS((yyvsp[(3) - (8)].node), (yyvsp[(7) - (8)].node));
		        fixpos((yyval.node), (yyvsp[(3) - (8)].node));
		        local_pop();
			class_nest--;
		        in_def = (yyvsp[(4) - (8)].num);
		        in_single = (yyvsp[(6) - (8)].num);
		    }
    break;

  case 316:
#line 1663 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("module definition in method body");
			class_nest++;
			local_push(0);
		        (yyval.num) = ruby_sourceline;
		    }
    break;

  case 317:
#line 1672 "parse.y"
    {
		        (yyval.node) = NEW_MODULE((yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node));
		        nd_set_line((yyval.node), (yyvsp[(3) - (5)].num));
		        local_pop();
			class_nest--;
		    }
    break;

  case 318:
#line 1679 "parse.y"
    {
			(yyval.id) = cur_mid;
			cur_mid = (yyvsp[(2) - (2)].id);
			in_def++;
			local_push(0);
		    }
    break;

  case 319:
#line 1688 "parse.y"
    {
			if (!(yyvsp[(5) - (6)].node)) (yyvsp[(5) - (6)].node) = NEW_NIL();
			(yyval.node) = NEW_DEFN((yyvsp[(2) - (6)].id), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node), NOEX_PRIVATE);
		        fixpos((yyval.node), (yyvsp[(4) - (6)].node));
		        local_pop();
			in_def--;
			cur_mid = (yyvsp[(3) - (6)].id);
		    }
    break;

  case 320:
#line 1696 "parse.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 321:
#line 1697 "parse.y"
    {
			in_single++;
			local_push(0);
		        lex_state = EXPR_END; /* force for args */
		    }
    break;

  case 322:
#line 1705 "parse.y"
    {
			(yyval.node) = NEW_DEFS((yyvsp[(2) - (9)].node), (yyvsp[(5) - (9)].id), (yyvsp[(7) - (9)].node), (yyvsp[(8) - (9)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (9)].node));
		        local_pop();
			in_single--;
		    }
    break;

  case 323:
#line 1712 "parse.y"
    {
			(yyval.node) = NEW_BREAK(0);
		    }
    break;

  case 324:
#line 1716 "parse.y"
    {
			(yyval.node) = NEW_NEXT(0);
		    }
    break;

  case 325:
#line 1720 "parse.y"
    {
			(yyval.node) = NEW_REDO();
		    }
    break;

  case 326:
#line 1724 "parse.y"
    {
			(yyval.node) = NEW_RETRY();
		    }
    break;

  case 327:
#line 1730 "parse.y"
    {
			value_expr((yyvsp[(1) - (1)].node));
			(yyval.node) = (yyvsp[(1) - (1)].node);
		    }
    break;

  case 336:
#line 1751 "parse.y"
    {
			(yyval.node) = NEW_IF(cond((yyvsp[(2) - (5)].node)), (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (5)].node));
		    }
    break;

  case 338:
#line 1759 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    }
    break;

  case 342:
#line 1770 "parse.y"
    {
			(yyval.node) = (NODE*)1;
		    }
    break;

  case 343:
#line 1774 "parse.y"
    {
			(yyval.node) = (NODE*)1;
		    }
    break;

  case 344:
#line 1778 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    }
    break;

  case 345:
#line 1784 "parse.y"
    {
		        (yyval.vars) = dyna_push();
			(yyvsp[(1) - (1)].num) = ruby_sourceline;
		    }
    break;

  case 346:
#line 1788 "parse.y"
    {(yyval.vars) = ruby_dyna_vars;}
    break;

  case 347:
#line 1791 "parse.y"
    {
			(yyval.node) = NEW_ITER((yyvsp[(3) - (6)].node), 0, dyna_init((yyvsp[(5) - (6)].node), (yyvsp[(4) - (6)].vars)));
			nd_set_line((yyval.node), (yyvsp[(1) - (6)].num));
			dyna_pop((yyvsp[(2) - (6)].vars));
		    }
    break;

  case 348:
#line 1799 "parse.y"
    {
			if ((yyvsp[(1) - (2)].node) && nd_type((yyvsp[(1) - (2)].node)) == NODE_BLOCK_PASS) {
			    rb_compile_error("both block arg and actual block given");
			}
			(yyvsp[(2) - (2)].node)->nd_iter = (yyvsp[(1) - (2)].node);
			(yyval.node) = (yyvsp[(2) - (2)].node);
		        fixpos((yyval.node), (yyvsp[(1) - (2)].node));
		    }
    break;

  case 349:
#line 1808 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 350:
#line 1812 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 351:
#line 1818 "parse.y"
    {
			(yyval.node) = new_fcall((yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 352:
#line 1823 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    }
    break;

  case 353:
#line 1828 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    }
    break;

  case 354:
#line 1833 "parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    }
    break;

  case 355:
#line 1837 "parse.y"
    {
			(yyval.node) = new_super((yyvsp[(2) - (2)].node));
		    }
    break;

  case 356:
#line 1841 "parse.y"
    {
			(yyval.node) = NEW_ZSUPER();
		    }
    break;

  case 357:
#line 1847 "parse.y"
    {
		        (yyval.vars) = dyna_push();
			(yyvsp[(1) - (1)].num) = ruby_sourceline;
		    }
    break;

  case 358:
#line 1851 "parse.y"
    {(yyval.vars) = ruby_dyna_vars;}
    break;

  case 359:
#line 1853 "parse.y"
    {
			(yyval.node) = NEW_ITER((yyvsp[(3) - (6)].node), 0, dyna_init((yyvsp[(5) - (6)].node), (yyvsp[(4) - (6)].vars)));
			nd_set_line((yyval.node), (yyvsp[(1) - (6)].num));
			dyna_pop((yyvsp[(2) - (6)].vars));
		    }
    break;

  case 360:
#line 1859 "parse.y"
    {
		        (yyval.vars) = dyna_push();
			(yyvsp[(1) - (1)].num) = ruby_sourceline;
		    }
    break;

  case 361:
#line 1863 "parse.y"
    {(yyval.vars) = ruby_dyna_vars;}
    break;

  case 362:
#line 1865 "parse.y"
    {
			(yyval.node) = NEW_ITER((yyvsp[(3) - (6)].node), 0, dyna_init((yyvsp[(5) - (6)].node), (yyvsp[(4) - (6)].vars)));
			nd_set_line((yyval.node), (yyvsp[(1) - (6)].num));
			dyna_pop((yyvsp[(2) - (6)].vars));
		    }
    break;

  case 363:
#line 1875 "parse.y"
    {
			(yyval.node) = NEW_WHEN((yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
		    }
    break;

  case 365:
#line 1881 "parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (4)].node), NEW_WHEN((yyvsp[(4) - (4)].node), 0, 0));
		    }
    break;

  case 366:
#line 1885 "parse.y"
    {
			(yyval.node) = NEW_LIST(NEW_WHEN((yyvsp[(2) - (2)].node), 0, 0));
		    }
    break;

  case 369:
#line 1897 "parse.y"
    {
		        if ((yyvsp[(3) - (6)].node)) {
		            (yyvsp[(3) - (6)].node) = node_assign((yyvsp[(3) - (6)].node), NEW_GVAR(rb_intern("$!")));
			    (yyvsp[(5) - (6)].node) = block_append((yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].node));
			}
			(yyval.node) = NEW_RESBODY((yyvsp[(2) - (6)].node), (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (6)].node)?(yyvsp[(2) - (6)].node):(yyvsp[(5) - (6)].node));
		    }
    break;

  case 371:
#line 1909 "parse.y"
    {
			(yyval.node) = NEW_LIST((yyvsp[(1) - (1)].node));
		    }
    break;

  case 374:
#line 1917 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    }
    break;

  case 376:
#line 1924 "parse.y"
    {
			if ((yyvsp[(2) - (2)].node))
			    (yyval.node) = (yyvsp[(2) - (2)].node);
			else
			    /* place holder */
			    (yyval.node) = NEW_NIL();
		    }
    break;

  case 379:
#line 1936 "parse.y"
    {
			(yyval.node) = NEW_LIT(ID2SYM((yyvsp[(1) - (1)].id)));
		    }
    break;

  case 381:
#line 1943 "parse.y"
    {
			NODE *node = (yyvsp[(1) - (1)].node);
			if (!node) {
			    node = NEW_STR(rb_str_new(0, 0));
			}
			else {
			    node = evstr2dstr(node);
			}
			(yyval.node) = node;
		    }
    break;

  case 383:
#line 1957 "parse.y"
    {
			(yyval.node) = literal_concat((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 384:
#line 1963 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    }
    break;

  case 385:
#line 1969 "parse.y"
    {
			NODE *node = (yyvsp[(2) - (3)].node);
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
			(yyval.node) = node;
		    }
    break;

  case 386:
#line 1992 "parse.y"
    {
			int options = (yyvsp[(3) - (3)].num);
			NODE *node = (yyvsp[(2) - (3)].node);
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
			(yyval.node) = node;
		    }
    break;

  case 387:
#line 2025 "parse.y"
    {
			(yyval.node) = NEW_ZARRAY();
		    }
    break;

  case 388:
#line 2029 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    }
    break;

  case 389:
#line 2035 "parse.y"
    {
			(yyval.node) = 0;
		    }
    break;

  case 390:
#line 2039 "parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), evstr2dstr((yyvsp[(2) - (3)].node)));
		    }
    break;

  case 392:
#line 2046 "parse.y"
    {
			(yyval.node) = literal_concat((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 393:
#line 2052 "parse.y"
    {
			(yyval.node) = NEW_ZARRAY();
		    }
    break;

  case 394:
#line 2056 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    }
    break;

  case 395:
#line 2062 "parse.y"
    {
			(yyval.node) = 0;
		    }
    break;

  case 396:
#line 2066 "parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].node));
		    }
    break;

  case 397:
#line 2072 "parse.y"
    {
			(yyval.node) = 0;
		    }
    break;

  case 398:
#line 2076 "parse.y"
    {
			(yyval.node) = literal_concat((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 399:
#line 2082 "parse.y"
    {
			(yyval.node) = 0;
		    }
    break;

  case 400:
#line 2086 "parse.y"
    {
			(yyval.node) = literal_concat((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 402:
#line 2093 "parse.y"
    {
			(yyval.node) = lex_strterm;
			lex_strterm = 0;
			lex_state = EXPR_BEG;
		    }
    break;

  case 403:
#line 2099 "parse.y"
    {
			lex_strterm = (yyvsp[(2) - (3)].node);
		        (yyval.node) = NEW_EVSTR((yyvsp[(3) - (3)].node));
		    }
    break;

  case 404:
#line 2104 "parse.y"
    {
			(yyval.node) = lex_strterm;
			lex_strterm = 0;
			lex_state = EXPR_BEG;
			COND_PUSH(0);
			CMDARG_PUSH(0);
		    }
    break;

  case 405:
#line 2112 "parse.y"
    {
			lex_strterm = (yyvsp[(2) - (4)].node);
			COND_LEXPOP();
			CMDARG_LEXPOP();
			if (((yyval.node) = (yyvsp[(3) - (4)].node)) && nd_type((yyval.node)) == NODE_NEWLINE) {
			    (yyval.node) = (yyval.node)->nd_next;
			    rb_gc_force_recycle((VALUE)(yyvsp[(3) - (4)].node));
			}
			(yyval.node) = new_evstr((yyval.node));
		    }
    break;

  case 406:
#line 2124 "parse.y"
    {(yyval.node) = NEW_GVAR((yyvsp[(1) - (1)].id));}
    break;

  case 407:
#line 2125 "parse.y"
    {(yyval.node) = NEW_IVAR((yyvsp[(1) - (1)].id));}
    break;

  case 408:
#line 2126 "parse.y"
    {(yyval.node) = NEW_CVAR((yyvsp[(1) - (1)].id));}
    break;

  case 410:
#line 2131 "parse.y"
    {
		        lex_state = EXPR_END;
			(yyval.id) = (yyvsp[(2) - (2)].id);
		    }
    break;

  case 415:
#line 2144 "parse.y"
    {
		        lex_state = EXPR_END;
			if (!((yyval.node) = (yyvsp[(2) - (3)].node))) {
			    (yyval.node) = NEW_NIL();
			    yyerror("empty symbol literal");
			}
			else {
			    VALUE lit;

			    switch (nd_type((yyval.node))) {
			      case NODE_DSTR:
				nd_set_type((yyval.node), NODE_DSYM);
				break;
			      case NODE_STR:
				lit = (yyval.node)->nd_lit;
				if (RSTRING(lit)->len == 0) {
				    yyerror("empty symbol literal");
				    break;
				}
				if (strlen(RSTRING(lit)->ptr) == RSTRING(lit)->len) {
				    (yyval.node)->nd_lit = ID2SYM(rb_intern(RSTRING((yyval.node)->nd_lit)->ptr));
				    nd_set_type((yyval.node), NODE_LIT);
				    break;
				}
				/* fall through */
			      default:
				(yyval.node) = NEW_NODE(NODE_DSYM, rb_str_new(0, 0), 1, NEW_LIST((yyval.node)));
				break;
			    }
			}
		    }
    break;

  case 418:
#line 2180 "parse.y"
    {
			(yyval.node) = negate_lit((yyvsp[(2) - (2)].node));
		    }
    break;

  case 419:
#line 2184 "parse.y"
    {
			(yyval.node) = negate_lit((yyvsp[(2) - (2)].node));
		    }
    break;

  case 425:
#line 2194 "parse.y"
    {(yyval.id) = kNIL;}
    break;

  case 426:
#line 2195 "parse.y"
    {(yyval.id) = kSELF;}
    break;

  case 427:
#line 2196 "parse.y"
    {(yyval.id) = kTRUE;}
    break;

  case 428:
#line 2197 "parse.y"
    {(yyval.id) = kFALSE;}
    break;

  case 429:
#line 2198 "parse.y"
    {(yyval.id) = k__FILE__;}
    break;

  case 430:
#line 2199 "parse.y"
    {(yyval.id) = k__LINE__;}
    break;

  case 431:
#line 2203 "parse.y"
    {
			(yyval.node) = gettable((yyvsp[(1) - (1)].id));
		    }
    break;

  case 432:
#line 2209 "parse.y"
    {
			(yyval.node) = assignable((yyvsp[(1) - (1)].id), 0);
		    }
    break;

  case 435:
#line 2219 "parse.y"
    {
			(yyval.node) = 0;
		    }
    break;

  case 436:
#line 2223 "parse.y"
    {
			lex_state = EXPR_BEG;
		    }
    break;

  case 437:
#line 2227 "parse.y"
    {
			(yyval.node) = (yyvsp[(3) - (4)].node);
		    }
    break;

  case 438:
#line 2230 "parse.y"
    {yyerrok; (yyval.node) = 0;}
    break;

  case 439:
#line 2234 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (4)].node);
			lex_state = EXPR_BEG;
		        command_start = Qtrue;
		    }
    break;

  case 440:
#line 2240 "parse.y"
    {
			(yyval.node) = (yyvsp[(1) - (2)].node);
		    }
    break;

  case 441:
#line 2246 "parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS((yyvsp[(1) - (6)].num), (yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].node)), (yyvsp[(6) - (6)].node));
		    }
    break;

  case 442:
#line 2250 "parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS((yyvsp[(1) - (4)].num), (yyvsp[(3) - (4)].node), 0), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 443:
#line 2254 "parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS((yyvsp[(1) - (4)].num), 0, (yyvsp[(3) - (4)].node)), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 444:
#line 2258 "parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS((yyvsp[(1) - (2)].num), 0, 0), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 445:
#line 2262 "parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS(0, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node)), (yyvsp[(4) - (4)].node));
		    }
    break;

  case 446:
#line 2266 "parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS(0, (yyvsp[(1) - (2)].node), 0), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 447:
#line 2270 "parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS(0, 0, (yyvsp[(1) - (2)].node)), (yyvsp[(2) - (2)].node));
		    }
    break;

  case 448:
#line 2274 "parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS(0, 0, 0), (yyvsp[(1) - (1)].node));
		    }
    break;

  case 449:
#line 2278 "parse.y"
    {
			(yyval.node) = NEW_ARGS(0, 0, 0);
		    }
    break;

  case 450:
#line 2284 "parse.y"
    {
			yyerror("formal argument cannot be a constant");
		    }
    break;

  case 451:
#line 2288 "parse.y"
    {
                        yyerror("formal argument cannot be an instance variable");
		    }
    break;

  case 452:
#line 2292 "parse.y"
    {
                        yyerror("formal argument cannot be a global variable");
		    }
    break;

  case 453:
#line 2296 "parse.y"
    {
                        yyerror("formal argument cannot be a class variable");
		    }
    break;

  case 454:
#line 2300 "parse.y"
    {
			if (!is_local_id((yyvsp[(1) - (1)].id)))
			    yyerror("formal argument must be local variable");
			else if (local_id((yyvsp[(1) - (1)].id)))
			    yyerror("duplicate argument name");
			local_cnt((yyvsp[(1) - (1)].id));
			(yyval.num) = 1;
		    }
    break;

  case 456:
#line 2312 "parse.y"
    {
			(yyval.num) += 1;
		    }
    break;

  case 457:
#line 2318 "parse.y"
    {
			if (!is_local_id((yyvsp[(1) - (3)].id)))
			    yyerror("formal argument must be local variable");
			else if (local_id((yyvsp[(1) - (3)].id)))
			    yyerror("duplicate optional argument name");
			(yyval.node) = assignable((yyvsp[(1) - (3)].id), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 458:
#line 2328 "parse.y"
    {
			(yyval.node) = NEW_BLOCK((yyvsp[(1) - (1)].node));
			(yyval.node)->nd_end = (yyval.node);
		    }
    break;

  case 459:
#line 2333 "parse.y"
    {
			(yyval.node) = block_append((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 462:
#line 2343 "parse.y"
    {
			if (!is_local_id((yyvsp[(2) - (2)].id)))
			    yyerror("rest argument must be local variable");
			else if (local_id((yyvsp[(2) - (2)].id)))
			    yyerror("duplicate rest argument name");
			if (dyna_in_block()) {
			    rb_dvar_push((yyvsp[(2) - (2)].id), Qnil);
			}
			(yyval.node) = assignable((yyvsp[(2) - (2)].id), 0);
		    }
    break;

  case 463:
#line 2354 "parse.y"
    {
			if (dyna_in_block()) {
			    (yyval.node) = NEW_DASGN_CURR(internal_id(), 0);
			}
			else {
			    (yyval.node) = NEW_NODE(NODE_LASGN,0,0,local_append(0));
			}
		    }
    break;

  case 466:
#line 2369 "parse.y"
    {
			if (!is_local_id((yyvsp[(2) - (2)].id)))
			    yyerror("block argument must be local variable");
			else if (local_id((yyvsp[(2) - (2)].id)))
			    yyerror("duplicate block argument name");
			(yyval.node) = NEW_BLOCK_ARG((yyvsp[(2) - (2)].id));
		    }
    break;

  case 467:
#line 2379 "parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    }
    break;

  case 469:
#line 2386 "parse.y"
    {
			(yyval.node) = (yyvsp[(1) - (1)].node);
			value_expr((yyval.node));
		    }
    break;

  case 470:
#line 2390 "parse.y"
    {lex_state = EXPR_BEG;}
    break;

  case 471:
#line 2391 "parse.y"
    {
			if ((yyvsp[(3) - (5)].node) == 0) {
			    yyerror("can't define singleton method for ().");
			}
			else {
			    switch (nd_type((yyvsp[(3) - (5)].node))) {
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
				value_expr((yyvsp[(3) - (5)].node));
				break;
			    }
			}
			(yyval.node) = (yyvsp[(3) - (5)].node);
		    }
    break;

  case 473:
#line 2417 "parse.y"
    {
			(yyval.node) = (yyvsp[(1) - (2)].node);
		    }
    break;

  case 474:
#line 2421 "parse.y"
    {
			if ((yyvsp[(1) - (2)].node)->nd_alen%2 != 0) {
			    yyerror("odd number list for Hash");
			}
			(yyval.node) = (yyvsp[(1) - (2)].node);
		    }
    break;

  case 476:
#line 2431 "parse.y"
    {
			(yyval.node) = list_concat((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 477:
#line 2437 "parse.y"
    {
			(yyval.node) = list_append(NEW_LIST((yyvsp[(1) - (3)].node)), (yyvsp[(3) - (3)].node));
		    }
    break;

  case 497:
#line 2475 "parse.y"
    {yyerrok;}
    break;

  case 500:
#line 2480 "parse.y"
    {yyerrok;}
    break;

  case 501:
#line 2483 "parse.y"
    {(yyval.node) = 0;}
    break;


/* Line 1267 of yacc.c.  */
#line 7260 "parse.c"
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


#line 2485 "parse.y"

#ifdef yystacksize
#undef YYMALLOC
#endif

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
    const int max_line_margin = 30;
    const char *p, *pe;
    char *buf;
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
	char *p2;
	const char *pre = "", *post = "";

	if (len > max_line_margin * 2 + 10) {
	    int re_mbc_startpos _((const char *, int, int, int));
	    if ((len = lex_p - p) > max_line_margin) {
		p = p + re_mbc_startpos(p, len, len - max_line_margin, 0);
		pre = "...";
	    }
	    if ((len = pe - lex_p) > max_line_margin) {
		pe = lex_p + re_mbc_startpos(lex_p, len, max_line_margin, 1);
		post = "...";
	    }
	    len = pe - p;
	}
	buf = ALLOCA_N(char, len+2);
	MEMCPY(buf, p, char, len);
	buf[len] = '\0';
	rb_compile_error_append("%s%s%s", pre, buf, post);

	i = lex_p - p;
	p2 = buf; pe = buf + len;

	while (p2 < pe) {
	    if (*p2 != '\t') *p2 = ' ';
	    p2++;
	}
	buf[i] = '^';
	buf[i+1] = '\0';
	rb_compile_error_append("%s", buf);
    }

    return 0;
}

static int heredoc_end;

int ruby_in_compile = 0;
int ruby__end__seen;

static VALUE ruby_debug_lines;
#ifdef YYMALLOC
static NODE *parser_heap;
#endif

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
	    ruby_debug_lines = rb_ary_new();
	    rb_hash_aset(hash, fname, ruby_debug_lines);
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
    ruby_eval_tree_begin = 0;
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
    if (ruby_nerrs) ruby_eval_tree_begin = 0;
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
    xfree(RSTRING(str)->ptr);
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
    enum lex_state last_state;

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
		lex_strterm = NEW_STRTERM(str_dword, term, paren);
		do {c = nextc();} while (ISSPACE(c));
		pushback(c);
		return tWORDS_BEG;

	      case 'w':
		lex_strterm = NEW_STRTERM(str_sword, term, paren);
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
	last_state = lex_state;
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
	    if (is_identchar(c)) {
		tokadd(c);
	    }
	    else {
		pushback(c);
	    }
	  gvar:
	    tokfix();
	    yylval.id = rb_intern(tok());
	    /* xxx shouldn't check if valid option variable */
	    return tGVAR;

	  case '&':		/* $&: last match */
	  case '`':		/* $`: string before last match */
	  case '\'':		/* $': string after last match */
	  case '+':		/* $+: string matches last paren. */
	    if (last_state == EXPR_FNAME) {
		tokadd('$');
		tokadd(c);
		goto gvar;
	    }
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
	    if (last_state == EXPR_FNAME) goto gvar;
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
	    return 0;
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

	last_state = lex_state;
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
			return kw->id[0];
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
	    last_state != EXPR_DOT &&
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
	int line;
	if (nd_type(node) == NODE_NEWLINE) return node;
	line = nd_line(node);
	node = remove_begin(node);
	nl = NEW_NEWLINE(node);
	nd_set_line(nl, line);
	nl->nd_nth = line;
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
    rb_warning("%s", mesg);
    ruby_sourceline = line;
}

static void
parser_warn(node, mesg)
    NODE *node;
    const char *mesg;
{
    int line = ruby_sourceline;
    ruby_sourceline = nd_line(node);
    rb_warn("%s", mesg);
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

static VALUE dyna_var_lookup _((ID id));

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
	else if (dyna_var_lookup(id)) {
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
	void_expr0(node->nd_head);
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
	if (!lvtbl->nofree) xfree(lvtbl->tbl);
	else lvtbl->tbl[0] = lvtbl->cnt;
    }
    ruby_dyna_vars = lvtbl->dyna_vars;
    xfree(lvtbl);
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
               if (!(ruby_scope->flags & SCOPE_CLONE))
                   xfree(ruby_scope->local_tbl);
	    }
            ruby_scope->local_vars[-1] = 0; /* no reference needed */
	    ruby_scope->local_tbl = local_tbl();
	}
    }
    local_pop();
}

#define DVAR_USED FL_USER6

static VALUE
dyna_var_lookup(id)
    ID id;
{
    struct RVarmap *vars = ruby_dyna_vars;

    while (vars) {
	if (vars->id == id) {
	    FL_SET(vars, DVAR_USED);
	    return Qtrue;
	}
	vars = vars->next;
    }
    return Qfalse;
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
	if (FL_TEST(post, DVAR_USED)) {
	    var = NEW_DASGN_CURR(post->id, var);
	}
    }
    return block_append(var, node);
}

int
ruby_parser_stack_on_heap()
{
#if defined(YYMALLOC)
    return Qfalse;
#else
    return Qtrue;
#endif
}

void
rb_gc_mark_parser()
{
#if defined YYMALLOC
    rb_gc_mark((VALUE)parser_heap);
#elif defined yystacksize
    if (yyvsp) rb_gc_mark_locations((VALUE *)yyvs, (VALUE *)yyvsp);
#endif

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

static int
is_special_global_name(m)
    const char *m;
{
    switch (*m) {
      case '~': case '*': case '$': case '?': case '!': case '@':
      case '/': case '\\': case ';': case ',': case '.': case '=':
      case ':': case '<': case '>': case '\"':
      case '&': case '`': case '\'': case '+':
      case '0':
	++m;
	break;
      case '-':
	++m;
	if (is_identchar(*m)) m += mbclen(*m);
	break;
      default:
	if (!ISDIGIT(*m)) return 0;
	do ++m; while (ISDIGIT(*m));
    }
    return !*m;
}

int
rb_symname_p(name)
    const char *name;
{
    const char *m = name;
    int localid = Qfalse;

    if (!m) return Qfalse;
    switch (*m) {
      case '\0':
	return Qfalse;

      case '$':
	if (is_special_global_name(++m)) return Qtrue;
	goto id;

      case '@':
	if (*++m == '@') ++m;
	goto id;

      case '<':
	switch (*++m) {
	  case '<': ++m; break;
	  case '=': if (*++m == '>') ++m; break;
	  default: break;
	}
	break;

      case '>':
	switch (*++m) {
	  case '>': case '=': ++m; break;
	}
	break;

      case '=':
	switch (*++m) {
	  case '~': ++m; break;
	  case '=': if (*++m == '=') ++m; break;
	  default: return Qfalse;
	}
	break;

      case '*':
	if (*++m == '*') ++m;
	break;

      case '+': case '-':
	if (*++m == '@') ++m;
	break;

      case '|': case '^': case '&': case '/': case '%': case '~': case '`':
	++m;
	break;

      case '[':
	if (*++m != ']') return Qfalse;
	if (*++m == '=') ++m;
	break;

      default:
	localid = !ISUPPER(*m);
      id:
	if (*m != '_' && !ISALPHA(*m) && !ismbchar(*m)) return Qfalse;
	while (is_identchar(*m)) m += mbclen(*m);
	if (localid) {
	    switch (*m) {
	      case '!': case '?': case '=': ++m;
	    }
	}
	break;
    }
    return *m ? Qfalse : Qtrue;
}

int
rb_sym_interned_p(str)
    VALUE str;
{
    ID id;

    if (st_lookup(sym_tbl, (st_data_t)RSTRING(str)->ptr, (st_data_t *)&id))
	return Qtrue;
    return Qfalse;
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
	if (is_special_global_name(++m)) goto new_id;
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
	if (name[0] != '_' && ISASCII(name[0]) && !ISALNUM(name[0])) {
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
    if (!ISDIGIT(*m)) {
	while (m <= name + last && is_identchar(*m)) {
	    m += mbclen(*m);
	}
    }
    if (*m) id = ID_JUNK;
  new_id:
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
    st_data_t data;

    if (id < tLAST_TOKEN) {
	int i;

	for (i=0; op_tbl[i].token; i++) {
	    if (op_tbl[i].token == id)
		return op_tbl[i].name;
	}
    }

    if (st_lookup(sym_rev_tbl, id, &data))
	return (char *)data;

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

#ifdef YYMALLOC
#define HEAPCNT(n, size) ((n) * (size) / sizeof(YYSTYPE))
#define NEWHEAP() rb_node_newnode(NODE_ALLOCA, 0, (VALUE)parser_heap, 0)
#define ADD2HEAP(n, c, p) ((parser_heap = (n))->u1.node = (p), \
			   (n)->u3.cnt = (c), (p))

static void *
rb_parser_malloc(size)
    size_t size;
{
    size_t cnt = HEAPCNT(1, size);
    NODE *n = NEWHEAP();
    void *ptr = xmalloc(size);

    return ADD2HEAP(n, cnt, ptr);
}

static void *
rb_parser_calloc(nelem, size)
    size_t nelem, size;
{
    size_t cnt = HEAPCNT(nelem, size);
    NODE *n = NEWHEAP();
    void *ptr = xcalloc(nelem, size);

    return ADD2HEAP(n, cnt, ptr);
}

static void *
rb_parser_realloc(ptr, size)
    void *ptr;
    size_t size;
{
    NODE *n;
    size_t cnt = HEAPCNT(1, size);

    if (ptr && (n = parser_heap) != NULL) {
	do {
	    if (n->u1.node == ptr) {
		n->u1.node = ptr = xrealloc(ptr, size);
		if (n->u3.cnt) n->u3.cnt = cnt;
		return ptr;
	    }
	} while ((n = n->u2.node) != NULL);
    }
    n = NEWHEAP();
    ptr = xrealloc(ptr, size);
    return ADD2HEAP(n, cnt, ptr);
}

static void
rb_parser_free(ptr)
    void *ptr;
{
    NODE **prev = &parser_heap, *n;

    while ((n = *prev) != 0) {
	if (n->u1.node == ptr) {
	    *prev = n->u2.node;
	    rb_gc_force_recycle((VALUE)n);
	    break;
	}
	prev = &n->u2.node;
    }
    xfree(ptr);
}
#endif

