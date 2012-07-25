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
#line 13 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"


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

static NODE *deferred_nodes;

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

static void fixup_nodes();

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

#define NEW_BLOCK_VAR(b, v) NEW_NODE(NODE_BLOCK_PASS, 0, b, v)

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
#line 215 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
{
    NODE *node;
    ID id;
    int num;
    struct RVarmap *vars;
}
/* Line 193 of yacc.c.  */
#line 513 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 526 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.c"

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
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   10042

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  132
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  146
/* YYNRULES -- Number of rules.  */
#define YYNRULES  515
/* YYNRULES -- Number of states.  */
#define YYNSTATES  919

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
    1113,  1115,  1117,  1121,  1123,  1126,  1131,  1139,  1146,  1151,
    1155,  1161,  1166,  1169,  1171,  1174,  1176,  1179,  1181,  1185,
    1186,  1187,  1194,  1197,  1202,  1207,  1210,  1215,  1220,  1224,
    1227,  1229,  1230,  1231,  1238,  1239,  1240,  1247,  1253,  1255,
    1260,  1263,  1265,  1267,  1274,  1276,  1278,  1280,  1282,  1285,
    1287,  1290,  1292,  1294,  1296,  1298,  1300,  1302,  1305,  1309,
    1313,  1317,  1321,  1325,  1326,  1330,  1332,  1335,  1339,  1343,
    1344,  1348,  1349,  1352,  1353,  1356,  1358,  1359,  1363,  1364,
    1369,  1371,  1373,  1375,  1377,  1380,  1382,  1384,  1386,  1388,
    1392,  1394,  1396,  1399,  1402,  1404,  1406,  1408,  1410,  1412,
    1414,  1416,  1418,  1420,  1422,  1424,  1426,  1428,  1430,  1432,
    1434,  1435,  1440,  1443,  1448,  1451,  1458,  1463,  1468,  1471,
    1476,  1479,  1482,  1484,  1485,  1487,  1489,  1491,  1493,  1495,
    1497,  1501,  1505,  1507,  1511,  1513,  1515,  1518,  1520,  1522,
    1524,  1527,  1530,  1532,  1534,  1535,  1541,  1543,  1546,  1549,
    1551,  1555,  1559,  1561,  1563,  1565,  1567,  1569,  1571,  1573,
    1575,  1577,  1579,  1581,  1583,  1584,  1586,  1587,  1589,  1590,
    1592,  1594,  1596,  1598,  1600,  1603
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     133,     0,    -1,    -1,   134,   136,    -1,   136,   221,   203,
     224,    -1,   137,   272,    -1,   277,    -1,   138,    -1,   137,
     276,   138,    -1,     1,   138,    -1,    -1,    44,   160,   139,
     160,    -1,    44,    52,    52,    -1,    44,    52,    60,    -1,
      44,    52,    59,    -1,     6,   161,    -1,   138,    39,   142,
      -1,   138,    40,   142,    -1,   138,    41,   142,    -1,   138,
      42,   142,    -1,   138,    43,   138,    -1,    -1,    46,   140,
     120,   136,   121,    -1,    47,   120,   136,   121,    -1,   155,
     103,   143,    -1,   149,   103,   143,    -1,   248,    83,   143,
      -1,   199,   122,   168,   123,    83,   143,    -1,   199,   124,
      50,    83,   143,    -1,   199,   124,    54,    83,   143,    -1,
     199,    81,    50,    83,   143,    -1,   249,    83,   143,    -1,
     155,   103,   181,    -1,   149,   103,   167,    -1,   149,   103,
     181,    -1,   141,    -1,   143,    -1,   141,    36,   141,    -1,
     141,    37,   141,    -1,    38,   141,    -1,   117,   143,    -1,
     165,    -1,   141,    -1,   148,    -1,   144,    -1,    29,   171,
      -1,    21,   171,    -1,    22,   171,    -1,   211,    -1,   211,
     124,   269,   173,    -1,   211,    81,   269,   173,    -1,    -1,
      -1,    90,   146,   207,   147,   136,   121,    -1,   268,   173,
      -1,   268,   173,   145,    -1,   199,   124,   269,   173,    -1,
     199,   124,   269,   173,   145,    -1,   199,    81,   269,   173,
      -1,   199,    81,   269,   173,   145,    -1,    31,   173,    -1,
      30,   173,    -1,   151,    -1,    85,   150,   125,    -1,   151,
      -1,    85,   150,   125,    -1,   153,    -1,   153,   152,    -1,
     153,    91,   154,    -1,   153,    91,    -1,    91,   154,    -1,
      91,    -1,   154,    -1,    85,   150,   125,    -1,   152,   126,
      -1,   153,   152,   126,    -1,   246,    -1,   199,   122,   168,
     123,    -1,   199,   124,    50,    -1,   199,    81,    50,    -1,
     199,   124,    54,    -1,   199,    81,    54,    -1,    82,    54,
      -1,   249,    -1,   246,    -1,   199,   122,   168,   123,    -1,
     199,   124,    50,    -1,   199,    81,    50,    -1,   199,   124,
      54,    -1,   199,    81,    54,    -1,    82,    54,    -1,   249,
      -1,    50,    -1,    54,    -1,    82,   156,    -1,   156,    -1,
     199,    81,   156,    -1,    50,    -1,    54,    -1,    51,    -1,
     163,    -1,   164,    -1,   158,    -1,   242,    -1,   159,    -1,
     244,    -1,   160,    -1,    -1,   161,   126,   162,   160,    -1,
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
     165,    -1,   248,    83,   165,    -1,   199,   122,   168,   123,
      83,   165,    -1,   199,   124,    50,    83,   165,    -1,   199,
     124,    54,    83,   165,    -1,   199,    81,    50,    83,   165,
      -1,   199,    81,    54,    83,   165,    -1,    82,    54,    83,
     165,    -1,   249,    83,   165,    -1,   165,    75,   165,    -1,
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
     273,   166,   165,    -1,   165,   104,   165,   105,   165,    -1,
     182,    -1,   165,    -1,   277,    -1,   148,   273,    -1,   180,
     274,    -1,   180,   126,    91,   165,   273,    -1,   266,   274,
      -1,    91,   165,   273,    -1,   128,   277,   125,    -1,   128,
     171,   273,   125,    -1,   128,   211,   273,   125,    -1,   128,
     180,   126,   211,   273,   125,    -1,   277,    -1,   169,    -1,
     148,    -1,   180,   179,    -1,   180,   126,    91,   167,   179,
      -1,   266,   179,    -1,   266,   126,    91,   167,   179,    -1,
     180,   126,   266,   179,    -1,   180,   126,   266,   126,    91,
     165,   179,    -1,    91,   167,   179,    -1,   178,    -1,   167,
     126,   180,   179,    -1,   167,   126,   178,    -1,   167,   126,
      91,   167,   179,    -1,   167,   126,   180,   126,    91,   167,
     179,    -1,   266,   179,    -1,   266,   126,    91,   167,   179,
      -1,   167,   126,   266,   179,    -1,   167,   126,   180,   126,
     266,   179,    -1,   167,   126,   266,   126,    91,   167,   179,
      -1,   167,   126,   180,   126,   266,   126,    91,   167,   179,
      -1,    91,   167,   179,    -1,   178,    -1,    -1,   174,   175,
      -1,   171,    -1,    -1,    86,   176,   125,    -1,    -1,    86,
     172,   177,   125,    -1,    92,   167,    -1,   126,   178,    -1,
     277,    -1,   167,    -1,   180,   126,   167,    -1,   180,   126,
     167,    -1,   180,   126,    91,   167,    -1,    91,   167,    -1,
     225,    -1,   226,    -1,   229,    -1,   230,    -1,   231,    -1,
     234,    -1,   247,    -1,   249,    -1,    51,    -1,    -1,     7,
     183,   135,    10,    -1,    -1,    86,   141,   184,   273,   125,
      -1,    85,   136,   125,    -1,   199,    81,    54,    -1,    82,
      54,    -1,   199,   122,   168,   123,    -1,    88,   168,   123,
      -1,    89,   265,   121,    -1,    29,    -1,    30,   128,   171,
     125,    -1,    30,   128,   125,    -1,    30,    -1,    -1,    45,
     273,   128,   185,   141,   125,    -1,   268,   213,    -1,   212,
      -1,   212,   213,    -1,    11,   142,   200,   136,   202,    10,
      -1,    12,   142,   200,   136,   203,    10,    -1,    -1,    -1,
      18,   186,   142,   201,   187,   136,    10,    -1,    -1,    -1,
      19,   188,   142,   201,   189,   136,    10,    -1,    16,   142,
     272,   218,    10,    -1,    16,   272,   218,    10,    -1,    16,
     272,    15,   136,    10,    -1,    -1,    -1,    20,   204,    25,
     190,   142,   201,   191,   136,    10,    -1,    -1,     3,   157,
     250,   192,   135,    10,    -1,    -1,    -1,     3,    79,   141,
     193,   275,   194,   135,    10,    -1,    -1,     4,   157,   195,
     135,    10,    -1,    -1,     5,   158,   196,   252,   135,    10,
      -1,    -1,    -1,     5,   263,   271,   197,   158,   198,   252,
     135,    10,    -1,    21,    -1,    22,    -1,    23,    -1,    24,
      -1,   182,    -1,   275,    -1,   105,    -1,    13,    -1,   275,
      13,    -1,   275,    -1,   105,    -1,    27,    -1,   203,    -1,
      14,   142,   200,   136,   202,    -1,   277,    -1,    15,   136,
      -1,   155,    -1,   149,    -1,   152,    -1,   205,   126,   152,
      -1,   205,    -1,   205,   126,    -1,   205,   126,    92,   155,
      -1,   205,   126,    91,   155,   126,    92,   155,    -1,   205,
     126,    91,   126,    92,   155,    -1,   205,   126,    91,   155,
      -1,   205,   126,    91,    -1,    91,   155,   126,    92,   155,
      -1,    91,   126,    92,   155,    -1,    91,   155,    -1,    91,
      -1,    92,   155,    -1,   277,    -1,   108,   108,    -1,    72,
      -1,   108,   206,   108,    -1,    -1,    -1,    28,   209,   207,
     210,   136,    10,    -1,   148,   208,    -1,   211,   124,   269,
     170,    -1,   211,    81,   269,   170,    -1,   268,   169,    -1,
     199,   124,   269,   170,    -1,   199,    81,   269,   169,    -1,
     199,    81,   270,    -1,    31,   169,    -1,    31,    -1,    -1,
      -1,   120,   214,   207,   215,   136,   121,    -1,    -1,    -1,
      26,   216,   207,   217,   136,    10,    -1,    17,   219,   200,
     136,   220,    -1,   180,    -1,   180,   126,    91,   167,    -1,
      91,   167,    -1,   203,    -1,   218,    -1,     8,   222,   223,
     200,   136,   221,    -1,   277,    -1,   167,    -1,   181,    -1,
     277,    -1,    84,   155,    -1,   277,    -1,     9,   136,    -1,
     277,    -1,   245,    -1,   242,    -1,   244,    -1,   227,    -1,
     228,    -1,   227,   228,    -1,    94,   236,   101,    -1,    95,
     237,   101,    -1,    96,   237,    61,    -1,    97,   129,   101,
      -1,    97,   232,   101,    -1,    -1,   232,   233,   129,    -1,
     238,    -1,   233,   238,    -1,    98,   129,   101,    -1,    98,
     235,   101,    -1,    -1,   235,    58,   129,    -1,    -1,   236,
     238,    -1,    -1,   237,   238,    -1,    58,    -1,    -1,   100,
     239,   241,    -1,    -1,    99,   240,   136,   121,    -1,    52,
      -1,    53,    -1,    55,    -1,   249,    -1,    93,   243,    -1,
     158,    -1,    53,    -1,    52,    -1,    55,    -1,    93,   237,
     101,    -1,    56,    -1,    57,    -1,   116,    56,    -1,   116,
      57,    -1,    50,    -1,    53,    -1,    52,    -1,    54,    -1,
      55,    -1,    33,    -1,    32,    -1,    34,    -1,    35,    -1,
      49,    -1,    48,    -1,   246,    -1,   246,    -1,    59,    -1,
      60,    -1,   275,    -1,    -1,   107,   251,   142,   275,    -1,
       1,   275,    -1,   128,   253,   273,   125,    -1,   253,   275,
      -1,   255,   126,   257,   126,   259,   262,    -1,   255,   126,
     257,   262,    -1,   255,   126,   259,   262,    -1,   255,   262,
      -1,   257,   126,   259,   262,    -1,   257,   262,    -1,   259,
     262,    -1,   261,    -1,    -1,    54,    -1,    53,    -1,    52,
      -1,    55,    -1,    50,    -1,   254,    -1,   255,   126,   254,
      -1,    50,   103,   167,    -1,   256,    -1,   257,   126,   256,
      -1,   113,    -1,    91,    -1,   258,    50,    -1,   258,    -1,
     110,    -1,    92,    -1,   260,    50,    -1,   126,   261,    -1,
     277,    -1,   247,    -1,    -1,   128,   264,   141,   273,   125,
      -1,   277,    -1,   266,   274,    -1,   180,   274,    -1,   267,
      -1,   266,   126,   267,    -1,   167,    84,   167,    -1,    50,
      -1,    54,    -1,    51,    -1,    50,    -1,    54,    -1,    51,
      -1,   163,    -1,    50,    -1,    51,    -1,   163,    -1,   124,
      -1,    81,    -1,    -1,   276,    -1,    -1,   130,    -1,    -1,
     130,    -1,   126,    -1,   131,    -1,   130,    -1,   275,    -1,
     276,   131,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   353,   353,   353,   378,   398,   406,   407,   411,   415,
     421,   421,   425,   429,   436,   441,   445,   454,   463,   475,
     487,   493,   492,   506,   514,   518,   524,   549,   565,   577,
     589,   601,   606,   610,   615,   620,   623,   624,   628,   632,
     636,   640,   643,   650,   651,   652,   656,   660,   666,   667,
     671,   678,   682,   677,   692,   697,   709,   714,   726,   731,
     743,   748,   755,   756,   762,   763,   769,   773,   777,   781,
     785,   789,   795,   796,   802,   806,   812,   816,   820,   824,
     828,   832,   838,   844,   851,   855,   859,   863,   867,   871,
     877,   883,   890,   894,   897,   901,   905,   911,   912,   913,
     914,   919,   926,   927,   930,   934,   937,   941,   941,   947,
     948,   949,   950,   951,   952,   953,   954,   955,   956,   957,
     958,   959,   960,   961,   962,   963,   964,   965,   966,   967,
     968,   969,   970,   971,   972,   975,   975,   975,   975,   976,
     976,   976,   976,   976,   976,   976,   977,   977,   977,   977,
     977,   977,   977,   978,   978,   978,   978,   978,   978,   979,
     979,   979,   979,   979,   979,   979,   980,   980,   980,   980,
     980,   981,   981,   981,   981,   984,   988,   992,  1017,  1033,
    1045,  1057,  1069,  1074,  1079,  1084,  1094,  1104,  1108,  1112,
    1116,  1120,  1124,  1128,  1132,  1136,  1145,  1149,  1153,  1157,
    1161,  1165,  1169,  1173,  1177,  1181,  1185,  1189,  1193,  1197,
    1201,  1205,  1209,  1213,  1217,  1221,  1225,  1225,  1230,  1235,
    1241,  1248,  1249,  1253,  1257,  1262,  1266,  1273,  1277,  1281,
    1285,  1291,  1292,  1295,  1299,  1303,  1308,  1313,  1318,  1323,
    1329,  1333,  1336,  1340,  1344,  1349,  1354,  1359,  1364,  1369,
    1374,  1379,  1384,  1388,  1391,  1391,  1403,  1404,  1404,  1409,
    1409,  1416,  1422,  1426,  1429,  1433,  1439,  1443,  1447,  1453,
    1454,  1455,  1456,  1457,  1458,  1459,  1460,  1461,  1466,  1465,
    1478,  1478,  1483,  1488,  1492,  1496,  1504,  1513,  1517,  1521,
    1525,  1529,  1533,  1533,  1538,  1544,  1545,  1554,  1567,  1580,
    1580,  1580,  1590,  1590,  1590,  1600,  1607,  1611,  1615,  1615,
    1615,  1623,  1622,  1639,  1644,  1638,  1661,  1660,  1677,  1676,
    1694,  1695,  1694,  1709,  1713,  1717,  1721,  1727,  1734,  1735,
    1736,  1737,  1740,  1741,  1742,  1745,  1746,  1755,  1756,  1762,
    1763,  1766,  1770,  1776,  1786,  1790,  1794,  1798,  1802,  1806,
    1810,  1814,  1818,  1822,  1826,  1832,  1833,  1838,  1843,  1851,
    1855,  1850,  1865,  1874,  1878,  1884,  1889,  1894,  1899,  1903,
    1907,  1914,  1918,  1913,  1926,  1930,  1925,  1939,  1946,  1947,
    1951,  1957,  1958,  1961,  1972,  1975,  1979,  1980,  1983,  1987,
    1990,  1998,  2001,  2002,  2006,  2009,  2022,  2023,  2029,  2035,
    2058,  2091,  2095,  2102,  2105,  2111,  2112,  2118,  2122,  2129,
    2132,  2139,  2142,  2149,  2152,  2158,  2160,  2159,  2171,  2170,
    2191,  2192,  2193,  2194,  2197,  2204,  2205,  2206,  2207,  2210,
    2244,  2245,  2246,  2250,  2256,  2257,  2258,  2259,  2260,  2261,
    2262,  2263,  2264,  2265,  2266,  2269,  2275,  2281,  2282,  2285,
    2290,  2289,  2297,  2300,  2306,  2312,  2316,  2320,  2324,  2328,
    2332,  2336,  2340,  2345,  2350,  2354,  2358,  2362,  2366,  2377,
    2378,  2384,  2394,  2399,  2405,  2406,  2409,  2420,  2431,  2432,
    2435,  2445,  2449,  2452,  2457,  2457,  2482,  2483,  2487,  2496,
    2497,  2503,  2509,  2510,  2511,  2514,  2515,  2516,  2517,  2520,
    2521,  2522,  2525,  2526,  2529,  2530,  2533,  2534,  2537,  2538,
    2539,  2542,  2543,  2546,  2547,  2550
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
  "for_var", "block_par", "block_var", "opt_block_var", "do_block", "@27",
  "@28", "block_call", "method_call", "brace_block", "@29", "@30", "@31",
  "@32", "case_body", "when_args", "cases", "opt_rescue", "exc_list",
  "exc_var", "opt_ensure", "literal", "strings", "string", "string1",
  "xstring", "regexp", "words", "word_list", "word", "qwords",
  "qword_list", "string_contents", "xstring_contents", "string_content",
  "@33", "@34", "string_dvar", "symbol", "sym", "dsym", "numeric",
  "variable", "var_ref", "var_lhs", "backref", "superclass", "@35",
  "f_arglist", "f_args", "f_norm_arg", "f_arg", "f_opt", "f_optarg",
  "restarg_mark", "f_rest_arg", "blkarg_mark", "f_block_arg",
  "opt_f_block_arg", "singleton", "@36", "assoc_list", "assocs", "assoc",
  "operation", "operation2", "operation3", "dot_or_colon", "opt_terms",
  "opt_nl", "trailer", "term", "terms", "none", 0
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
     204,   205,   205,   206,   206,   206,   206,   206,   206,   206,
     206,   206,   206,   206,   206,   207,   207,   207,   207,   209,
     210,   208,   211,   211,   211,   212,   212,   212,   212,   212,
     212,   214,   215,   213,   216,   217,   213,   218,   219,   219,
     219,   220,   220,   221,   221,   222,   222,   222,   223,   223,
     224,   224,   225,   225,   225,   226,   227,   227,   228,   229,
     230,   231,   231,   232,   232,   233,   233,   234,   234,   235,
     235,   236,   236,   237,   237,   238,   239,   238,   240,   238,
     241,   241,   241,   241,   242,   243,   243,   243,   243,   244,
     245,   245,   245,   245,   246,   246,   246,   246,   246,   246,
     246,   246,   246,   246,   246,   247,   248,   249,   249,   250,
     251,   250,   250,   252,   252,   253,   253,   253,   253,   253,
     253,   253,   253,   253,   254,   254,   254,   254,   254,   255,
     255,   256,   257,   257,   258,   258,   259,   259,   260,   260,
     261,   262,   262,   263,   264,   263,   265,   265,   265,   266,
     266,   267,   268,   268,   268,   269,   269,   269,   269,   270,
     270,   270,   271,   271,   272,   272,   273,   273,   274,   274,
     274,   275,   275,   276,   276,   277
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
       1,     1,     3,     1,     2,     4,     7,     6,     4,     3,
       5,     4,     2,     1,     2,     1,     2,     1,     3,     0,
       0,     6,     2,     4,     4,     2,     4,     4,     3,     2,
       1,     0,     0,     6,     0,     0,     6,     5,     1,     4,
       2,     1,     1,     6,     1,     1,     1,     1,     2,     1,
       2,     1,     1,     1,     1,     1,     1,     2,     3,     3,
       3,     3,     3,     0,     3,     1,     2,     3,     3,     0,
       3,     0,     2,     0,     2,     1,     0,     3,     0,     4,
       1,     1,     1,     1,     2,     1,     1,     1,     1,     3,
       1,     1,     2,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       0,     4,     2,     4,     2,     6,     4,     4,     2,     4,
       2,     2,     1,     0,     1,     1,     1,     1,     1,     1,
       3,     3,     1,     3,     1,     1,     2,     1,     1,     1,
       2,     2,     1,     1,     0,     5,     1,     2,     2,     1,
       3,     3,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     0,     1,     0,     1,     0,     1,
       1,     1,     1,     1,     2,     0
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,     0,     0,     0,     0,     0,   278,
       0,     0,   504,   299,   302,     0,   323,   324,   325,   326,
     288,   291,   370,   440,   439,   441,   442,     0,     0,   506,
      21,     0,   444,   443,   434,   277,   436,   435,   437,   438,
     430,   431,   447,   448,     0,     0,     0,     0,     0,   515,
     515,    71,   413,   411,   413,   413,   403,   409,     0,     0,
       0,     3,   504,     7,    35,    36,    44,    43,     0,    62,
       0,    66,    72,     0,    41,   219,     0,    48,   295,   269,
     270,   395,   396,   271,   272,   273,   274,   393,   394,   392,
     445,   275,     0,   276,   254,     6,     9,   323,   324,   288,
     291,   370,   506,    92,    93,     0,     0,     0,     0,    95,
       0,   327,     0,   445,   276,     0,   316,   144,   155,   145,
     168,   141,   161,   151,   150,   171,   172,   166,   149,   148,
     143,   169,   173,   174,   153,   142,   156,   160,   162,   154,
     147,   163,   170,   165,   164,   157,   167,   152,   140,   159,
     158,   139,   146,   137,   138,   135,   136,    97,    99,    98,
     130,   131,   128,   112,   113,   114,   117,   119,   115,   132,
     133,   120,   121,   125,   116,   118,   109,   110,   111,   122,
     123,   124,   126,   127,   129,   134,   484,   318,   100,   101,
     483,     0,   164,   157,   167,   152,   135,   136,    97,    98,
     102,   104,   106,    15,   103,   105,     0,     0,    42,     0,
       0,     0,   445,     0,   276,     0,   512,   511,   504,     0,
     513,   505,     0,     0,     0,   340,   339,     0,     0,   445,
     276,     0,     0,     0,   233,   220,   264,    46,   241,   515,
     515,   489,    47,    45,     0,    61,     0,   515,   369,    60,
      39,     0,    10,   507,   216,     0,     0,   195,     0,   196,
     284,     0,     0,     0,    62,   280,     0,   506,     0,   508,
     508,   221,   508,     0,   508,   486,     0,    70,     0,    76,
      83,   427,   426,   428,   425,     0,   424,     0,     0,     0,
       0,     0,     0,     0,   432,   433,    40,   210,   211,     5,
     505,     0,     0,     0,     0,     0,     0,     0,   359,   362,
       0,    74,     0,    69,    67,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   515,     0,     0,     0,   374,   371,   296,
     397,     0,     0,   365,    54,   294,     0,   313,    92,    93,
      94,   432,   433,     0,   450,   311,   449,     0,   515,     0,
       0,     0,   463,   503,   502,   320,   107,     0,   515,   284,
     330,   329,     0,   328,     0,     0,   515,     0,     0,     0,
       0,     0,     0,     0,     0,   514,     0,     0,   284,     0,
     515,     0,   308,   515,   261,     0,     0,   234,   263,     0,
     236,   290,     0,   257,   256,   255,   233,   506,   515,   506,
       0,    12,    14,    13,     0,   292,     0,     0,     0,     0,
       0,     0,     0,   282,    63,   506,   506,   222,   286,   510,
     509,   223,   510,   225,   510,   488,   287,   487,    82,     0,
     515,     0,   415,   418,   416,   429,   414,   398,   412,   399,
     400,   401,   402,     0,   405,   407,     0,   408,     0,     0,
       8,    16,    17,    18,    19,    20,    37,    38,   515,     0,
      25,    33,     0,    34,     0,    68,    75,    24,   175,   264,
      32,   192,   200,   205,   206,   207,   202,   204,   214,   215,
     208,   209,   185,   186,   212,   213,     0,   201,   203,   197,
     198,   199,   187,   188,   189,   190,   191,   495,   500,   496,
     501,   254,   368,     0,   495,   497,   496,   498,   515,   495,
     496,   254,   254,   515,   515,    26,   177,    31,   184,    51,
      55,     0,   452,     0,     0,    92,    93,    96,     0,     0,
     515,     0,   506,   468,   466,   465,   464,   467,   475,   479,
     478,   474,   463,     0,     0,   469,   515,   472,   515,   477,
     515,     0,   462,     0,     0,   279,   515,   515,   384,   515,
     331,   175,   499,   283,     0,   495,   496,   515,     0,     0,
       0,   378,     0,   306,   334,   333,   300,   332,   303,   499,
     283,     0,   495,   496,     0,     0,   240,   491,     0,   265,
     262,   515,     0,     0,   490,   289,     0,    41,     0,   259,
       0,   253,   515,     0,     0,     0,     0,     0,   227,    11,
       0,   217,     0,    23,   183,    63,     0,   226,     0,   265,
      79,    81,     0,   495,   496,     0,     0,   404,   406,   410,
     193,   194,   357,     0,   360,   355,   268,     0,    73,     0,
       0,     0,     0,   367,    58,   285,     0,     0,   232,   366,
      56,   231,   364,    50,   363,    49,   375,   372,   515,   314,
       0,     0,   285,   317,     0,     0,   506,     0,   454,     0,
     458,   482,     0,   460,   476,     0,   461,   480,   321,   108,
     385,   386,   515,   387,     0,   515,   337,     0,     0,   335,
       0,   285,     0,     0,     0,   305,   307,   380,     0,     0,
       0,     0,   285,     0,   515,     0,   238,   515,   515,     0,
       0,   258,     0,   246,   228,     0,   506,   515,   515,   229,
       0,    22,   281,   506,    77,     0,   420,   421,   422,   417,
     423,   353,     0,   356,   341,   343,     0,     0,     0,   266,
     176,   218,    30,   181,   182,    59,     0,    28,   179,    29,
     180,    57,     0,     0,    52,     0,   451,   312,   485,   471,
       0,   319,   470,   515,   515,   481,     0,   473,   515,   463,
       0,     0,   389,   338,     0,     4,   391,     0,   297,     0,
     298,     0,   515,     0,     0,   309,   235,     0,   237,   252,
       0,   243,   515,   515,   260,     0,     0,   293,   224,   419,
       0,     0,   352,     0,    84,    91,   354,   344,   358,     0,
     267,    27,   178,     0,     0,     0,     0,   453,     0,   456,
     457,   459,     0,   388,     0,   390,     0,   379,   381,   382,
     377,   301,   304,     0,   515,   515,     0,   242,     0,   248,
     515,   230,    90,     0,     0,     0,   515,     0,   349,     0,
     342,   361,   376,   373,     0,   315,   515,     0,   515,   515,
       0,   239,   244,     0,   515,     0,   247,   351,     0,    87,
      89,     0,    86,    88,     0,   348,   345,    53,   455,   322,
     383,   336,   310,   515,     0,   249,   515,   350,    85,     0,
       0,   245,     0,   250,   347,     0,   515,   346,   251
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,   377,   378,    62,    63,   424,   255,    64,
     209,    65,    66,   540,   678,   835,    67,    68,   263,    69,
      70,    71,    72,   210,   109,   110,   200,   201,   202,   203,
     574,   527,   189,    74,   426,   236,   268,   668,   669,   237,
     619,   245,   246,   415,   620,   730,   610,   407,   269,   483,
      75,   206,   435,   630,   222,   720,   223,   721,   604,   853,
     544,   541,   775,   370,   372,   573,   789,   258,   382,   596,
     708,   709,   228,   755,   756,   654,   309,   478,   757,    77,
      78,   355,   534,   773,   533,   772,   394,   592,   850,   577,
     702,   791,   795,    79,    80,    81,    82,    83,    84,    85,
     291,   463,    86,   293,   287,   285,   456,   646,   645,   749,
      87,   286,    88,    89,   212,    91,   213,   214,   365,   543,
     563,   564,   565,   566,   567,   568,   569,   570,   571,   785,
     690,   191,   371,   273,   270,   241,   115,   548,   522,   375,
     219,   254,   441,   383,   221,    95
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -639
static const yytype_int16 yypact[] =
{
    -639,   149,  2672,  -639,  5966,  8434,  8731,  4199,  5517,  -639,
    7305,  7305,  4070,  -639,  -639,  8533,  6172,  6172,  -639,  -639,
    6172,  4832,  4935,  -639,  -639,  -639,  -639,  7305,  5392,   -42,
    -639,    -9,  -639,  -639,  4317,  4420,  -639,  -639,  4523,  -639,
    -639,  -639,  -639,  -639,  8129,  8129,   123,  3480,  7305,  7408,
    8129,  8830,  5267,  -639,  -639,  -639,    94,   129,    70,  8232,
    8129,  -639,   -34,   795,   403,  -639,  -639,   185,   173,  -639,
     158,  8632,  -639,   183,  9858,    20,   206,    47,    58,  -639,
    -639,   199,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,
     136,  -639,   240,   298,    69,  -639,   795,  -639,  -639,  -639,
     210,   229,   -42,   112,   339,  7305,   108,  3611,   429,  -639,
      75,  -639,   211,  -639,  -639,    69,  -639,  -639,  -639,  -639,
    -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,
    -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,
    -639,  -639,  -639,  -639,    56,    98,   111,   228,  -639,  -639,
    -639,  -639,  -639,  -639,  -639,   243,   250,   261,  -639,   262,
    -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,
    -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,
    -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,
    -639,   267,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,
    -639,  -639,  -639,   244,  -639,  -639,  2908,   319,   403,    43,
     275,   340,    73,   309,   145,    43,  -639,  -639,   -34,   236,
    -639,   268,  7305,  7305,   358,  -639,  -639,   457,   427,    77,
      82,  8129,  8129,  8129,  -639,  9858,   344,  -639,  -639,   331,
     353,  -639,  -639,  -639,  5858,  -639,  6275,  6172,  -639,  -639,
    -639,   354,  -639,  -639,   338,   350,  3742,  -639,   503,   417,
     399,  3480,   363,   366,   387,   403,  8129,   -42,   372,   142,
     159,  -639,   323,   383,   159,  -639,   456,  -639,   536,   545,
     571,  -639,  -639,  -639,  -639,   373,  -639,   377,   450,   499,
     433,   475,   451,    83,   467,   495,  -639,  -639,  -639,  -639,
    3967,  7305,  7305,  7305,  7305,  5966,  7305,  7305,  -639,  -639,
    7511,  -639,  3480,  8830,   438,  7511,  8129,  8129,  8129,  8129,
    8129,  8129,  8129,  8129,  8129,  8129,  8129,  8129,  8129,  8129,
    8129,  8129,  8129,  8129,  8129,  8129,  8129,  8129,  8129,  8129,
    8129,  8129,  8984,  7408,  9050,  9116,  9116,  -639,  -639,  -639,
    -639,  8232,  8232,  -639,   479,  -639,   338,   403,  -639,   590,
    -639,  -639,  -639,   -34,  -639,  -639,  -639,  9182,  7408,  9116,
    2908,  7305,   553,  -639,  -639,  -639,  -639,   570,   577,   154,
    -639,  -639,  3030,   573,  8129,  9248,  7408,  9314,  8129,  8129,
    3274,   572,  3864,  7614,   581,  -639,    60,    60,    84,  9380,
    7408,  9446,  -639,   466,  -639,  8129,  6378,  -639,  -639,  6481,
    -639,  -639,   472,  6069,  -639,  -639,   185,   -42,   478,    37,
     476,  -639,  -639,  -639,  5517,  -639,  8129,  3742,   494,  9248,
    9314,  8129,   496,  -639,   505,   -42,   891,  -639,  -639,  7717,
    -639,  -639,  8129,  -639,  8129,  -639,  -639,  -639,   590,  9512,
    7408,  9578,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,
    -639,  -639,  -639,   127,  -639,  -639,   504,  -639,  8129,  8129,
     795,  -639,  -639,  -639,  -639,  -639,  -639,  -639,    51,  8129,
    -639,   506,   516,  -639,   512,  -639,  -639,  -639,  2073,  -639,
    -639,   417,  1140,  1140,  1140,  1140,   865,   865,  9927,  2348,
    1140,  1140,  9875,  9875,   498,   498,  2757,   865,   865,   404,
     404,   611,    40,    40,   417,   417,   417,  1581,  5038,  2777,
    5141,   229,  -639,   497,   437,  -639,   490,  -639,  4935,  -639,
    -639,  1277,  1277,    51,    51,  -639,  9858,  -639,  9858,  -639,
    -639,   -34,  -639,  7305,  2908,   370,    44,  -639,   229,   526,
     229,   643,    95,   551,  -639,  -639,  -639,  -639,  -639,  -639,
    -639,  -639,   586,  2908,   -34,  -639,   535,  -639,   544,   623,
     559,   629,  -639,  5642,  5517,  -639,  7820,   673,  -639,   491,
    -639,  2329,  4626,  4729,   574,   242,   292,   673,   691,   698,
    8129,   583,    43,  -639,  -639,  -639,  -639,  -639,  -639,    68,
      81,   592,    88,    89,  7305,   624,  -639,  -639,  8129,   344,
    -639,   594,  8129,   344,  -639,  -639,  8129,  9774,    38,  -639,
     596,  -639,   601,   609,  6584,  9116,  9116,   617,  -639,  -639,
    7305,  9858,   615,  -639,  9858,   430,   620,  -639,  8129,  -639,
     370,    44,   628,   135,   138,  3742,   651,  -639,  -639,  -639,
     417,   417,  -639,  2550,  -639,  -639,  -639,  7923,  -639,  8129,
    8129,  8232,  8129,  -639,   479,   604,  8232,  8232,  -639,  -639,
     479,  -639,  -639,  -639,  -639,  -639,  -639,  -639,    51,  -639,
     -34,   728,  -639,  -639,   627,  8129,   -42,   743,  -639,   586,
    -639,  -639,   269,  -639,  -639,    91,  -639,  -639,  -639,  -639,
     506,  -639,   670,  -639,  3377,   747,  -639,  7305,   748,  -639,
    8129,   304,  8129,  8129,   750,  -639,  -639,  -639,  8026,  3152,
    3864,  3864,    90,    60,   466,  6687,  -639,   466,   466,  6790,
     632,  -639,  6893,  -639,  -639,   185,    37,   229,   229,  -639,
      99,  -639,  -639,   891,   613,   641,  -639,  -639,  -639,  -639,
    -639,  2233,  8929,  -639,  -639,   639,   658,  3864,  8129,   648,
    9858,  9858,  -639,  9858,  9858,  -639,  8232,  -639,  9858,  -639,
    9858,  -639,  3864,  3742,  -639,  2908,  -639,  -639,  -639,  -639,
     644,  -639,  -639,   649,   559,  -639,   551,  -639,   559,   553,
    8929,    43,  -639,  -639,  3864,  -639,  -639,    43,  -639,  8129,
    -639,  8129,   385,   766,   770,  -639,  -639,  8129,  -639,  -639,
    8129,  -639,   655,   657,  -639,  8129,   660,  -639,  -639,  -639,
     732,   695,   663,   619,   545,   571,  -639,  8335,  -639,   780,
    -639,  -639,  9858,   781,   672,  3742,   784,  -639,   269,  -639,
    -639,  -639,  2908,  -639,  2908,  -639,  3030,  -639,  -639,  -639,
    -639,  -639,  -639,  3864,  9795,   466,  6996,  -639,  7099,  -639,
     466,  -639,   590,  8929,   703,  9644,  7408,  9710,  5759,  8929,
    -639,  -639,  -639,  -639,   676,  -639,   559,   791,   577,   491,
     794,  -639,  -639,  8129,   679,  8129,  -639,  -639,  8929,   370,
      44,   683,   379,   408,   716,   685,  -639,  -639,  -639,  -639,
    -639,  -639,  -639,   466,  7202,  -639,   466,  -639,   613,  8929,
     721,  -639,  8129,  -639,  -639,  8929,   466,  -639,  -639
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -639,  -639,  -639,  -346,   511,  -639,    17,  -639,  -639,    52,
     140,   -37,  -639,  -483,  -639,  -639,    50,   799,  -172,    -6,
     -66,  -639,    21,   507,   -41,   809,    -1,  -639,   -21,  -639,
    -639,     9,  -639,  1134,  -639,  1619,  -342,    18,  -472,    35,
    -639,    16,  -639,  -639,  -639,  -639,     3,   283,   100,  -247,
     124,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,  -639,
    -639,  -639,  -639,  -639,  -639,  -639,  -639,    -2,  -203,  -386,
     -63,  -495,  -639,  -639,  -639,  -448,  -639,  -639,  -639,  -220,
    -639,   744,  -639,  -639,  -639,  -639,  -355,  -639,  -639,   -54,
    -639,  -639,  -639,  -639,  -639,  -639,   745,  -639,  -639,  -639,
    -639,  -639,  -639,  -639,  -639,   513,  -252,  -639,  -639,  -639,
      22,  -639,    45,  -639,    27,   820,   428,   265,  -639,  -639,
      41,   270,   150,  -639,  -638,   152,  -639,  -609,  -639,  -344,
    -493,  -639,  -639,  -639,    74,  -361,  1821,  -199,  -639,  -639,
     -19,   110,   376,    59,   769,  1194
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -516
static const yytype_int16 yytable[] =
{
      76,   523,    76,   112,   112,   314,   187,   252,   211,   211,
     211,   598,   390,   227,   211,   211,   188,   188,   211,   238,
     238,    96,   296,   238,   551,   211,   549,   419,   572,    90,
     204,    90,   113,   113,   113,   458,   588,   188,   249,   464,
     248,   264,   229,   299,   584,    76,   211,   211,   614,   278,
     204,   284,   242,   205,   787,   243,   380,   211,   601,   672,
     674,   188,   208,   208,   208,   360,   234,   234,   490,   278,
     234,   220,   277,   205,    90,   693,   363,   696,   279,   250,
     784,   614,   705,   788,   347,   676,   677,   594,   253,   432,
     240,   240,   714,   -87,   240,   347,   216,   217,   279,   267,
     265,  -327,   -84,   211,   316,    76,   -89,   -91,   642,   -90,
     354,   256,   353,   -86,   -88,   -85,   239,   239,   625,   248,
     239,   220,   405,   652,   274,  -283,   294,   295,   345,   111,
     111,   306,   307,   353,    90,   306,   307,  -440,  -492,   111,
     484,   466,  -327,   521,  -327,   528,   531,   532,   381,     3,
     272,   215,   218,   339,   340,   341,  -446,   357,   358,   653,
     -78,   626,   359,   -80,   729,   595,  -283,   253,  -283,   366,
     550,   346,  -496,   216,   217,   111,   -84,   260,   348,  -439,
    -440,   765,   364,   559,   467,   452,   521,   771,   528,   348,
     216,   217,  -441,  -434,   -79,   111,  -495,   247,   681,   391,
     787,   560,   550,   -76,    76,   216,   217,   -81,   -83,  -496,
     -82,   648,   356,   308,   -78,   -80,   -77,   687,   572,  -446,
     211,   211,  -439,   290,   817,   253,   453,   454,   389,   876,
     774,   550,  -492,    90,  -434,  -441,  -434,   431,   -78,   -84,
    -492,   -80,   211,   -78,   211,   211,   -80,   238,   -91,   238,
     238,   392,   550,   393,    76,   264,   647,   -90,   292,    76,
     -78,   -78,   -76,   -80,   -80,   672,   674,    93,   439,    93,
     114,   114,   440,   480,   208,   208,   310,   220,   487,   412,
     230,   414,   417,    90,   311,   442,   315,   342,    90,   440,
     839,   840,   367,    53,   234,   841,   234,   416,    76,   211,
     211,   211,   211,    76,   211,   211,   264,   848,   211,  -442,
      76,   278,    93,   211,   535,   537,   280,   470,   240,   786,
     240,   240,   475,   351,  -444,   712,   547,    90,   343,   701,
     344,  -443,    90,   368,   485,   369,   280,   805,   244,    90,
     279,   211,  -434,  -437,   239,   -86,   239,   418,   373,   211,
     211,   520,  -442,   208,   208,   208,   208,   247,   476,   477,
     558,   559,   396,   397,   614,  -493,   211,  -444,    76,   211,
     376,   614,    93,   379,  -443,   713,   520,   437,   384,   560,
      76,   352,   561,   898,   211,  -434,  -437,   799,    76,   719,
      76,   374,   388,   267,   520,   -88,  -499,    90,   211,   395,
     704,   -91,   393,   629,   736,  -495,   421,   -85,   520,    90,
     482,   211,   398,   422,   423,   482,   621,    90,   267,    90,
    -437,   385,   542,   552,   -83,    76,   737,   738,   405,   836,
      92,   452,    92,   188,  -496,   452,   267,   111,   520,   306,
     307,   471,   472,   473,   474,   572,   204,   849,   211,   444,
     267,  -499,   402,   440,    90,   597,   597,   406,   520,  -493,
    -495,  -437,   386,  -437,   387,   265,   425,  -493,   316,   205,
     427,    93,   453,   454,   455,    92,   453,   454,   457,   409,
     611,   316,   431,   329,   330,   361,   362,   622,   433,  -496,
    -499,   434,  -499,   591,  -499,   438,   877,   614,  -495,  -495,
     267,  -495,   -90,  -495,   446,   707,   704,  -495,   452,    73,
     448,    73,   -64,    61,   336,   337,   338,   339,   340,   341,
     666,    93,   226,   410,   891,   -82,    93,   623,  -496,   627,
    -496,   468,  -496,   452,   461,    92,  -496,   664,   399,   663,
     -86,   211,    76,   614,   670,   636,   637,   673,   675,   453,
     454,   459,   465,   699,    73,   -65,   -73,   452,   262,   469,
     460,    76,   316,   -78,   486,    93,   663,   288,   289,   539,
      93,    90,   698,   667,   453,   454,   462,    93,   280,   400,
     575,   401,   188,   188,   429,   576,   580,   754,   844,   393,
      90,   593,   605,   -88,   846,   208,   204,   615,   453,   454,
     679,   628,   211,   553,   624,   554,   555,   556,   557,   337,
     338,   339,   340,   341,    73,   633,   -80,   449,   262,   205,
     665,   635,   211,   688,   762,   386,  -445,   430,   211,   767,
     769,   -73,  -264,   649,    92,    93,   553,   658,   554,   555,
     556,   557,   657,    76,   558,   559,   443,    93,   445,   682,
     447,   278,  -276,   683,   685,    93,   208,    93,   450,   211,
     451,   689,   684,   560,   211,   211,   561,  -445,   550,  -445,
     692,  -284,    90,   694,   735,   316,   482,   558,   559,   697,
     279,   562,   740,   680,    92,   695,   606,   766,   704,    92,
     329,   330,    93,  -276,  -285,  -276,   560,   711,   611,   561,
     865,   715,    76,   746,   747,   211,   748,   -85,   716,   718,
      42,    43,  -284,    73,  -284,   722,   232,    76,    76,    76,
     725,   731,   337,   338,   339,   340,   341,   732,    92,   831,
     -77,    90,   811,    92,   734,  -285,   741,  -285,   777,   776,
      92,   866,   739,   867,   723,   742,    90,    90,    90,   823,
     823,   744,   778,   781,   790,    76,   794,   814,   798,   208,
     800,   870,   819,    73,   211,   827,   828,   428,    73,   837,
      76,    76,   262,    76,  -265,   838,   851,   111,   824,   824,
     852,   856,   597,   858,    90,   861,   862,   863,   823,   864,
     871,   872,    76,   873,   875,   888,   780,   897,    92,    90,
      90,   899,    90,   813,   902,   904,   908,    73,   909,    93,
      92,   910,    73,   915,   225,   116,   901,   824,    92,    73,
      92,    90,   349,   262,   900,   278,   350,   190,    93,   812,
     842,   300,   686,    76,   301,   302,   303,   304,   305,   782,
      76,   783,    76,     0,    76,     0,   816,   797,     0,     0,
       0,    76,     0,   818,   279,    92,     0,     0,     0,     0,
       0,   823,    90,     0,   211,     0,   823,   823,     0,    90,
       0,    90,     0,    90,   520,   111,   111,    73,     0,     0,
      90,     0,     0,     0,     0,     0,   823,     0,     0,    73,
     824,     0,     0,   579,   726,   824,   824,    73,     0,    73,
       0,   587,     0,   589,     0,   733,     0,   823,     0,     0,
      93,   750,     0,   823,   111,   824,   267,     0,   280,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   316,
     884,     0,     0,     0,    73,     0,   824,     0,   632,     0,
       0,     0,   824,     0,   329,   330,     0,     0,     0,     0,
       0,   111,     0,     0,     0,   316,   317,   318,   319,   320,
     321,   322,   323,   324,   325,   326,   327,   328,     0,    93,
     329,   330,    92,   334,   335,   336,   337,   338,   339,   340,
     341,     0,     0,     0,    93,    93,    93,   111,     0,     0,
       0,    92,   111,   111,     0,   331,     0,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   806,     0,     0,
     808,   809,   111,     0,     0,     0,   825,   825,     0,     0,
       0,   253,    93,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   111,     0,     0,     0,    93,    93,   111,
      93,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    73,     0,     0,     0,   825,     0,     0,     0,    93,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      73,     0,     0,    92,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   280,     0,     0,   857,   859,     0,     0,     0,
      93,     0,     0,     0,     0,     0,     0,    93,     0,    93,
       0,    93,     0,     0,     0,     0,     0,     0,    93,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   825,     0,
       0,     0,    92,   825,   825,     0,     0,   881,   882,     0,
       0,     0,     0,   886,     0,     0,     0,    92,    92,    92,
     235,   235,    73,   825,   235,     0,   745,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   905,     0,     0,
       0,     0,     0,     0,   825,     0,     0,     0,   257,   259,
     825,     0,     0,   235,   235,    92,   911,     0,     0,   913,
       0,     0,     0,   297,   298,     0,     0,     0,     0,   918,
      92,    92,     0,    92,   316,  -516,  -516,  -516,  -516,   321,
     322,    73,     0,  -516,  -516,   793,     0,     0,     0,   329,
     330,     0,    92,     0,     0,     0,    73,    73,    73,     0,
     802,   803,   804,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   271,   275,     0,   332,   333,   334,   335,
     336,   337,   338,   339,   340,   341,     0,     0,   822,   826,
       0,     0,     0,    92,    73,     0,     0,     0,   829,     0,
      92,     0,    92,     0,    92,     0,     0,  -515,     0,    73,
      73,    92,    73,   833,   834,  -515,  -515,  -515,     0,     0,
    -515,  -515,  -515,     0,  -515,     0,     0,   843,     0,     0,
       0,    73,     0,     0,  -515,   845,     0,     0,     0,     0,
       0,     0,     0,  -515,  -515,     0,  -515,  -515,  -515,  -515,
    -515,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    73,     0,     0,     0,   874,     0,     0,    73,
       0,    73,     0,    73,     0,   878,     0,   879,  -515,     0,
      73,     0,     0,     0,   880,   235,   235,   297,     0,     0,
     887,     0,     0,     0,     0,   895,   896,     0,   235,     0,
     235,   235,  -515,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   907,     0,     0,  -515,     0,
     436,  -515,  -515,     0,     0,   247,     0,  -515,  -515,     0,
       0,     0,     0,     0,     0,     0,   914,     0,     0,     0,
       0,     0,   917,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   408,   408,     0,     0,     0,     0,     0,
       0,   420,     0,     0,   235,     0,     0,     0,     0,   488,
     491,   492,   493,   494,   495,   496,   497,   498,   499,   500,
     501,   502,   503,   504,   505,   506,   507,   508,   509,   510,
     511,   512,   513,   514,   515,   516,     0,   235,     0,     0,
       0,     0,     0,     0,     0,   536,   538,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   235,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   581,     0,
     235,     0,   536,   538,     0,     0,     0,   235,     0,     0,
       0,     0,     0,     0,   235,     0,     0,   271,     0,   235,
     235,     0,     0,   235,     0,     0,     0,   617,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     631,     0,   271,     0,     0,   634,     0,     0,     0,     0,
       0,     0,   578,   235,     0,     0,   235,     0,   235,     0,
     271,  -499,     0,     0,   235,     0,     0,     0,     0,  -499,
    -499,  -499,     0,     0,   271,  -499,  -499,   408,  -499,     0,
       0,     0,   650,   651,     0,     0,     0,  -499,     0,     0,
       0,     0,   408,   235,     0,     0,     0,  -499,  -499,     0,
    -499,  -499,  -499,  -499,  -499,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   271,  -499,  -499,  -499,  -499,  -499,
    -499,  -499,  -499,  -499,  -499,  -499,  -499,  -499,     0,     0,
    -499,  -499,  -499,     0,   661,     0,     0,     0,     0,     0,
       0,     0,   655,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   -87,  -499,     0,  -499,  -499,  -499,
    -499,  -499,  -499,  -499,  -499,  -499,  -499,     0,     0,     0,
       0,  -499,  -499,  -499,     0,  -499,  -499,   -79,     0,     0,
     235,  -499,  -499,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   671,     0,   235,   671,   671,   655,   655,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   235,     0,   671,     0,   235,     0,     0,     0,
     235,     0,     0,     0,     0,     0,     0,     0,   235,     0,
     691,     0,   691,     0,   691,     0,     0,     0,     0,     0,
     703,   706,   743,   706,     0,     0,     0,     0,     0,     0,
       0,   706,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   235,     0,   760,   761,   763,   764,     0,     0,     0,
     768,   770,     0,     0,     0,   408,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   408,     0,     0,   235,
       0,     0,     0,    94,     0,    94,     0,     0,     0,     0,
       0,    94,    94,    94,     0,     0,     0,    94,    94,     0,
       0,    94,     0,     0,   763,     0,   768,   770,    94,     0,
     403,   404,   235,     0,     0,     0,     0,     0,     0,   235,
       0,     0,     0,   235,     0,     0,   235,     0,    94,    94,
      94,     0,   655,     0,     0,     0,     0,     0,     0,     0,
      94,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   235,     0,     0,     0,   792,     0,     0,   796,
     832,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   408,     0,
       0,   408,   408,     0,     0,     0,    94,     0,    94,   481,
       0,   671,   671,   832,   489,   235,     0,     0,     0,     0,
       0,   854,     0,     0,   235,     0,     0,     0,     0,   235,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   691,   691,     0,
       0,     0,   691,     0,     0,     0,     0,     0,     0,     0,
     235,     0,   235,     0,     0,     0,   706,     0,     0,     0,
     235,     0,     0,     0,     0,     0,   408,   408,     0,     0,
       0,     0,   489,     0,     0,     0,     0,   235,     0,   235,
       0,     0,     0,     0,   607,   609,     0,    94,   613,     0,
       0,     0,   618,     0,     0,     0,     0,     0,   235,     0,
       0,     0,     0,    94,    94,     0,   235,     0,   408,   408,
       0,     0,     0,     0,   408,     0,     0,     0,   639,     0,
     271,   613,     0,   639,     0,    94,     0,    94,    94,     0,
     691,     0,   578,   706,     0,     0,     0,    94,   408,     0,
       0,     0,    94,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   408,   656,     0,
     408,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     408,     0,     0,     0,     0,     0,   659,     0,     0,     0,
       0,    94,    94,    94,    94,    94,    94,    94,    94,     0,
       0,    94,     0,    94,     0,     0,    94,   316,   317,   318,
     319,   320,   321,   322,   323,   324,   325,   326,   327,   328,
       0,     0,   329,   330,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    94,     0,     0,     0,     0,     0,
       0,     0,    94,    94,     0,     0,     0,   331,     0,   332,
     333,   334,   335,   336,   337,   338,   339,   340,   341,    94,
       0,    94,    94,     0,     0,   700,     0,     0,     0,  -220,
       0,     0,     0,    94,     0,     0,     0,    94,     0,   717,
       0,    94,     0,    94,     0,     0,     0,     0,     0,     0,
       0,    94,     0,     0,     0,     0,     0,   724,     0,     0,
       0,   727,     0,     0,    94,   728,     5,     6,     7,     0,
       9,     0,     0,   609,    10,    11,     0,     0,    94,    12,
       0,    13,    14,    15,    97,    98,    18,    19,     0,     0,
       0,     0,    99,   100,   101,    23,    24,    25,    26,     0,
       0,    94,     0,     0,     0,     0,   759,     0,   102,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   779,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   820,     0,     0,   107,    48,
       0,    49,    50,     0,     0,     0,    52,    53,    54,    55,
      56,    57,     0,     0,     0,     0,     0,   639,     0,     0,
       0,     0,     0,     0,   613,     0,     0,     0,     0,   108,
       0,   613,     0,     0,     0,     0,     0,     0,     0,   821,
       0,     0,     0,     0,    94,    94,     0,     0,     0,     0,
       0,     0,   659,     0,     0,     0,     0,   830,     0,     0,
       0,     0,     0,     0,    94,     0,     0,     0,     0,     0,
       0,     0,     0,   316,   317,   318,   319,   320,   321,   322,
     323,   324,   325,   326,   327,   328,     0,     0,   329,   330,
       0,     0,   316,   317,   318,   319,   320,   321,   322,   323,
     847,   325,   326,     0,     0,    94,     0,   329,   330,   855,
       0,     0,     0,   331,   860,   332,   333,   334,   335,   336,
     337,   338,   339,   340,   341,    94,     0,     0,     0,     0,
       0,    94,     0,     0,   332,   333,   334,   335,   336,   337,
     338,   339,   340,   341,     0,     0,    94,     0,     0,     0,
       0,     0,     0,     0,     0,   609,     0,   613,     0,     0,
       0,     0,    94,     0,     0,     0,     0,    94,    94,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   903,     0,   906,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   613,     0,    94,     0,     0,    94,     0,
       0,   916,     0,     0,     0,     0,     0,     0,     0,     0,
      94,    94,    94,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,     0,    13,    14,
      15,    97,    98,    18,    19,     0,     0,     0,    94,    99,
     100,   101,    23,    24,    25,    26,     0,    94,     0,     0,
       0,     0,     0,    94,    94,   102,    94,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,     0,     0,     0,    94,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   276,     0,     0,   312,    48,     0,    49,    50,
       0,   751,   752,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,    94,     0,   753,     0,
       0,     0,     0,    94,     0,    94,   108,    94,     0,     0,
       0,     0,  -515,     4,    94,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,    94,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    46,     0,     0,    47,    48,     0,
      49,    50,     0,    51,     0,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     0,     0,     0,  -283,     0,     0,
       0,     0,     0,     0,     0,  -283,  -283,  -283,    58,    59,
      60,  -283,  -283,     0,  -283,     0,     0,     0,     0,     0,
       0,     0,  -515,  -515,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -283,  -283,     0,  -283,  -283,  -283,  -283,
    -283,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,     0,     0,   329,   330,     0,     0,
       0,  -283,  -283,  -283,  -283,  -283,  -283,  -283,  -283,  -283,
    -283,  -283,  -283,  -283,     0,     0,  -283,  -283,  -283,     0,
     662,   331,   660,   332,   333,   334,   335,   336,   337,   338,
     339,   340,   341,     0,     0,     0,     0,     0,     0,     0,
     -89,  -283,     0,  -283,  -283,  -283,  -283,  -283,  -283,  -283,
    -283,  -283,  -283,     0,     0,     0,     0,     0,  -283,  -283,
       0,  -283,  -283,   -81,     0,     0,     0,  -283,  -283,     4,
       0,     5,     6,     7,     8,     9,  -515,  -515,  -515,    10,
      11,     0,     0,  -515,    12,     0,    13,    14,    15,    16,
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
       0,     4,     0,     5,     6,     7,     8,     9,  -515,  -515,
    -515,    10,    11,     0,  -515,  -515,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    46,     0,     0,    47,    48,     0,    49,    50,
       0,    51,     0,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    58,    59,    60,     0,
       0,     0,     0,     4,     0,     5,     6,     7,     8,     9,
    -515,  -515,  -515,    10,    11,     0,     0,  -515,    12,  -515,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    46,     0,     0,    47,    48,     0,
      49,    50,     0,    51,     0,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    58,    59,
      60,     0,     0,     0,     0,     4,     0,     5,     6,     7,
       8,     9,  -515,  -515,  -515,    10,    11,     0,     0,  -515,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    46,     0,     0,    47,
      48,     0,    49,    50,     0,    51,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     4,     0,
       5,     6,     7,     8,     9,     0,  -515,  -515,    10,    11,
      58,    59,    60,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,  -515,  -515,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    46,
       0,     0,    47,    48,     0,    49,    50,     0,    51,     0,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     4,     0,     5,     6,     7,     8,     9,     0,     0,
       0,    10,    11,    58,    59,    60,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,  -515,  -515,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    46,     0,     0,   261,    48,     0,    49,    50,
       0,    51,     0,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    58,    59,    60,     0,
       0,     0,     0,     0,     0,  -515,     0,     0,     0,     0,
    -515,  -515,     4,     0,     5,     6,     7,     8,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,     0,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    46,     0,     0,    47,    48,     0,    49,
      50,     0,    51,     0,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    58,    59,    60,
       0,     0,     0,     0,     0,     0,  -515,     0,     0,     0,
       0,  -515,  -515,     4,     0,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    46,     0,     0,    47,    48,     0,
      49,    50,     0,    51,     0,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    58,    59,
      60,     0,     0,  -515,     0,     4,     0,     5,     6,     7,
       8,     9,  -515,  -515,  -515,    10,    11,     0,     0,     0,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    46,     0,     0,    47,
      48,     0,    49,    50,     0,    51,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     8,     9,     0,     0,     0,    10,    11,
      58,    59,    60,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,  -515,  -515,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    46,
       0,     0,    47,    48,     0,    49,    50,     0,    51,     0,
      52,    53,    54,    55,    56,    57,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    58,    59,    60,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,   395,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,     0,    29,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   207,     0,     0,   107,    48,     0,    49,    50,
       0,     0,     0,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    58,    59,    60,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     216,   217,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,     0,     0,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,     0,     0,
       0,     0,     0,   151,   152,   153,   154,   155,   156,   157,
     158,    36,    37,   159,    39,     0,     0,     0,     0,     0,
       0,   160,   161,   162,   163,   164,   165,     0,   166,   167,
       0,     0,   168,     0,     0,     0,   169,   170,   171,   172,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     173,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   174,   175,   176,   177,   178,
     179,   180,   181,   182,   183,     0,     0,   184,     0,     0,
    -492,  -492,  -492,     0,  -492,     0,   185,   186,  -492,  -492,
       0,     0,     0,  -492,     0,  -492,  -492,  -492,  -492,  -492,
    -492,  -492,     0,  -492,     0,     0,  -492,  -492,  -492,  -492,
    -492,  -492,  -492,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -492,     0,     0,  -492,  -492,  -492,  -492,  -492,
    -492,  -492,  -492,  -492,  -492,     0,  -492,  -492,     0,  -492,
    -492,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -492,
       0,     0,  -492,  -492,     0,  -492,  -492,     0,  -492,  -492,
    -492,  -492,  -492,  -492,  -492,  -492,     0,     0,     0,     0,
       0,     0,     0,  -494,  -494,  -494,     0,  -494,     0,     0,
       0,  -494,  -494,  -492,  -492,  -492,  -494,  -492,  -494,  -494,
    -494,  -494,  -494,  -494,  -494,  -492,  -494,     0,     0,  -494,
    -494,  -494,  -494,  -494,  -494,  -494,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -494,     0,     0,  -494,  -494,
    -494,  -494,  -494,  -494,  -494,  -494,  -494,  -494,     0,  -494,
    -494,     0,  -494,  -494,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -494,     0,     0,  -494,  -494,     0,  -494,  -494,
       0,  -494,  -494,  -494,  -494,  -494,  -494,  -494,  -494,     0,
       0,     0,     0,     0,     0,     0,  -493,  -493,  -493,     0,
    -493,     0,     0,     0,  -493,  -493,  -494,  -494,  -494,  -493,
    -494,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -494,  -493,
       0,     0,  -493,  -493,  -493,  -493,  -493,  -493,  -493,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -493,     0,
       0,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,  -493,
    -493,     0,  -493,  -493,     0,  -493,  -493,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -493,     0,     0,  -493,  -493,
       0,  -493,  -493,     0,  -493,  -493,  -493,  -493,  -493,  -493,
    -493,  -493,     0,     0,     0,     0,     0,     0,     0,  -495,
    -495,  -495,     0,  -495,     0,     0,     0,  -495,  -495,  -493,
    -493,  -493,  -495,  -493,  -495,  -495,  -495,  -495,  -495,  -495,
    -495,  -493,     0,     0,     0,  -495,  -495,  -495,  -495,  -495,
    -495,  -495,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -495,     0,     0,  -495,  -495,  -495,  -495,  -495,  -495,
    -495,  -495,  -495,  -495,     0,  -495,  -495,     0,  -495,  -495,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -495,   710,
       0,  -495,  -495,     0,  -495,  -495,     0,  -495,  -495,  -495,
    -495,  -495,  -495,  -495,  -495,     0,     0,     0,     0,   -87,
       0,     0,  -496,  -496,  -496,     0,  -496,     0,     0,     0,
    -496,  -496,  -495,  -495,  -495,  -496,     0,  -496,  -496,  -496,
    -496,  -496,  -496,  -496,  -495,     0,     0,     0,  -496,  -496,
    -496,  -496,  -496,  -496,  -496,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -496,     0,     0,  -496,  -496,  -496,
    -496,  -496,  -496,  -496,  -496,  -496,  -496,     0,  -496,  -496,
       0,  -496,  -496,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -496,   662,     0,  -496,  -496,     0,  -496,  -496,     0,
    -496,  -496,  -496,  -496,  -496,  -496,  -496,  -496,     0,     0,
       0,     0,   -89,     0,     0,  -254,  -254,  -254,     0,  -254,
       0,     0,     0,  -254,  -254,  -496,  -496,  -496,  -254,     0,
    -254,  -254,  -254,  -254,  -254,  -254,  -254,  -496,     0,     0,
       0,  -254,  -254,  -254,  -254,  -254,  -254,  -254,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -254,     0,     0,
    -254,  -254,  -254,  -254,  -254,  -254,  -254,  -254,  -254,  -254,
       0,  -254,  -254,     0,  -254,  -254,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,  -254,     0,     0,  -254,  -254,     0,
    -254,  -254,     0,  -254,  -254,  -254,  -254,  -254,  -254,  -254,
    -254,     0,     0,     0,     0,     0,     0,     0,  -254,  -254,
    -254,     0,  -254,     0,     0,     0,  -254,  -254,  -254,  -254,
    -254,  -254,     0,  -254,  -254,  -254,  -254,  -254,  -254,  -254,
     244,     0,     0,     0,  -254,  -254,  -254,  -254,  -254,  -254,
    -254,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -254,     0,     0,  -254,  -254,  -254,  -254,  -254,  -254,  -254,
    -254,  -254,  -254,     0,  -254,  -254,     0,  -254,  -254,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -254,     0,     0,
    -254,  -254,     0,  -254,  -254,     0,  -254,  -254,  -254,  -254,
    -254,  -254,  -254,  -254,     0,     0,     0,     0,     0,     0,
       0,  -497,  -497,  -497,     0,  -497,     0,     0,     0,  -497,
    -497,  -254,  -254,  -254,  -497,     0,  -497,  -497,  -497,  -497,
    -497,  -497,  -497,   247,     0,     0,     0,  -497,  -497,  -497,
    -497,  -497,  -497,  -497,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -497,     0,     0,  -497,  -497,  -497,  -497,
    -497,  -497,  -497,  -497,  -497,  -497,     0,  -497,  -497,     0,
    -497,  -497,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -497,     0,     0,  -497,  -497,     0,  -497,  -497,     0,  -497,
    -497,  -497,  -497,  -497,  -497,  -497,  -497,     0,     0,     0,
       0,     0,     0,     0,  -498,  -498,  -498,     0,  -498,     0,
       0,     0,  -498,  -498,  -497,  -497,  -497,  -498,     0,  -498,
    -498,  -498,  -498,  -498,  -498,  -498,  -497,     0,     0,     0,
    -498,  -498,  -498,  -498,  -498,  -498,  -498,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -498,     0,     0,  -498,
    -498,  -498,  -498,  -498,  -498,  -498,  -498,  -498,  -498,     0,
    -498,  -498,     0,  -498,  -498,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -498,     0,     0,  -498,  -498,     0,  -498,
    -498,     0,  -498,  -498,  -498,  -498,  -498,  -498,  -498,  -498,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -498,  -498,  -498,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -498,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,     0,     0,   141,   142,   143,   192,
     193,   194,   195,   148,   149,   150,     0,     0,     0,     0,
       0,   151,   152,   153,   154,   196,   197,   198,   158,   281,
     282,   199,   283,     0,     0,     0,     0,     0,     0,   160,
     161,   162,   163,   164,   165,     0,   166,   167,     0,     0,
     168,     0,     0,     0,   169,   170,   171,   172,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   173,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   174,   175,   176,   177,   178,   179,   180,
     181,   182,   183,     0,     0,   184,     0,     0,     0,     0,
       0,     0,     0,     0,   185,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,     0,
       0,   141,   142,   143,   192,   193,   194,   195,   148,   149,
     150,     0,     0,     0,     0,     0,   151,   152,   153,   154,
     196,   197,   198,   158,   251,     0,   199,     0,     0,     0,
       0,     0,     0,     0,   160,   161,   162,   163,   164,   165,
       0,   166,   167,     0,     0,   168,     0,     0,     0,   169,
     170,   171,   172,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   173,     0,    52,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,     0,     0,
     184,     0,     0,     0,     0,     0,     0,     0,     0,   185,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,     0,     0,   141,   142,   143,   192,
     193,   194,   195,   148,   149,   150,     0,     0,     0,     0,
       0,   151,   152,   153,   154,   196,   197,   198,   158,     0,
       0,   199,     0,     0,     0,     0,     0,     0,     0,   160,
     161,   162,   163,   164,   165,     0,   166,   167,     0,     0,
     168,     0,     0,     0,   169,   170,   171,   172,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   173,     0,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   174,   175,   176,   177,   178,   179,   180,
     181,   182,   183,     0,     0,   184,     0,     0,     0,     0,
       0,     0,     0,     0,   185,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,     0,
       0,   141,   142,   143,   192,   193,   194,   195,   148,   149,
     150,     0,     0,     0,     0,     0,   151,   152,   153,   154,
     196,   197,   198,   158,     0,     0,   199,     0,     0,     0,
       0,     0,     0,     0,   160,   161,   162,   163,   164,   165,
       0,   166,   167,     0,     0,   168,     0,     0,     0,   169,
     170,   171,   172,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   173,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,     0,     0,
     184,     0,     5,     6,     7,     0,     9,     0,     0,   185,
      10,    11,     0,     0,     0,    12,     0,    13,    14,    15,
      97,    98,    18,    19,     0,     0,     0,     0,    99,   100,
     101,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   102,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,     0,    42,    43,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   820,     0,     0,   107,    48,     0,    49,    50,     0,
       0,     0,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,   108,    13,    14,    15,    97,
      98,    18,    19,     0,     0,   894,     0,    99,    21,    22,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,     0,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     207,     0,     0,   107,    48,     0,    49,    50,     0,   231,
     232,    52,    53,    54,    55,    56,    57,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     8,     9,    58,   233,    60,    10,    11,     0,
       0,     0,    12,   411,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     0,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    46,     0,
       0,    47,    48,     0,    49,    50,     0,    51,     0,    52,
      53,    54,    55,    56,    57,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    58,    59,    60,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,     0,    42,    43,
       0,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   207,     0,     0,   107,    48,     0,    49,    50,     0,
     616,   232,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    58,    59,    60,    12,     0,
      13,    14,    15,    97,    98,    18,    19,     0,     0,     0,
       0,    99,    21,    22,    23,    24,    25,    26,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   207,     0,     0,   107,    48,     0,
      49,    50,     0,   231,   232,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    58,   233,
      60,    12,     0,    13,    14,    15,    97,    98,    18,    19,
       0,     0,     0,     0,    99,    21,    22,    23,    24,    25,
      26,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      29,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,     0,    42,    43,     0,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   207,     0,     0,
     107,   413,     0,    49,    50,     0,   231,   232,    52,    53,
      54,    55,    56,    57,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    58,   233,    60,    12,     0,    13,    14,    15,    97,
      98,    18,    19,     0,     0,     0,     0,    99,   100,   101,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,     0,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     207,     0,     0,   107,    48,     0,    49,    50,     0,   608,
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
      50,     0,   612,   232,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    58,   233,    60,
      12,     0,    13,    14,    15,    97,    98,    18,    19,     0,
       0,     0,     0,    99,    21,    22,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   207,     0,     0,   107,
      48,     0,    49,    50,     0,   608,   232,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      58,   233,    60,    12,     0,    13,    14,    15,    97,    98,
      18,    19,     0,     0,     0,     0,    99,   100,   101,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   107,    48,     0,    49,    50,     0,   807,   232,
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
       0,   810,   232,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    58,   233,    60,    12,
       0,    13,    14,    15,    97,    98,    18,    19,     0,     0,
       0,     0,    99,   100,   101,    23,    24,    25,    26,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,     0,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   207,     0,     0,   107,    48,
       0,    49,    50,     0,   815,   232,    52,    53,    54,    55,
      56,    57,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    58,
     233,    60,    12,     0,    13,    14,    15,    97,    98,    18,
      19,     0,     0,     0,     0,    99,   100,   101,    23,    24,
      25,    26,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     0,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   207,     0,
       0,   107,    48,     0,    49,    50,     0,   883,   232,    52,
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
     885,   232,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    58,   233,    60,    12,     0,
      13,    14,    15,    97,    98,    18,    19,     0,     0,     0,
       0,    99,   100,   101,    23,    24,    25,    26,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   207,     0,     0,   107,    48,     0,
      49,    50,     0,   912,   232,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    58,   233,
      60,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,     0,     0,
      29,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,     0,    42,    43,     0,    44,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   207,     0,     0,
     107,    48,     0,    49,    50,     0,     0,     0,    52,    53,
      54,    55,    56,    57,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    58,    59,    60,    12,     0,    13,    14,    15,    97,
      98,    18,    19,     0,     0,     0,     0,    99,    21,    22,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,     0,    42,    43,     0,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     207,     0,     0,   107,    48,     0,    49,    50,     0,   266,
       0,    52,    53,    54,    55,    56,    57,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    58,   233,    60,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    29,     0,     0,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,     0,
      42,    43,     0,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   207,     0,     0,   107,    48,     0,    49,
      50,     0,   479,     0,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    58,   233,    60,
      12,     0,    13,    14,    15,    97,    98,    18,    19,     0,
       0,     0,     0,    99,   100,   101,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,     0,    42,    43,     0,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   207,     0,     0,   107,
      48,     0,    49,    50,     0,   590,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      58,   233,    60,    12,     0,    13,    14,    15,    97,    98,
      18,    19,     0,     0,     0,     0,    99,   100,   101,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,     0,    42,    43,     0,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   207,
       0,     0,   107,    48,     0,    49,    50,     0,   638,     0,
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
       0,   479,     0,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    58,   233,    60,    12,
       0,    13,    14,    15,    97,    98,    18,    19,     0,     0,
       0,     0,    99,   100,   101,    23,    24,    25,    26,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,     0,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   207,     0,     0,   107,    48,
       0,    49,    50,     0,   758,     0,    52,    53,    54,    55,
      56,    57,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    58,
     233,    60,    12,     0,    13,    14,    15,    97,    98,    18,
      19,     0,     0,     0,     0,    99,   100,   101,    23,    24,
      25,    26,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,     0,    42,    43,     0,    44,    45,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   207,     0,
       0,   107,    48,     0,    49,    50,     0,   801,     0,    52,
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
       0,     0,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    58,   233,    60,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   207,     0,     0,   107,    48,     0,
      49,    50,     0,     0,     0,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    58,   233,
      60,    12,     0,    13,    14,    15,    97,    98,    18,    19,
       0,     0,     0,     0,    99,   100,   101,    23,    24,    25,
      26,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     102,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,     0,    42,    43,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   276,     0,     0,
     312,    48,     0,    49,    50,     0,   868,   869,    52,    53,
      54,    55,    56,    57,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,     0,     0,
      12,   108,    13,    14,    15,    97,    98,    18,    19,     0,
       0,     0,     0,    99,   100,   101,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   102,
       0,     0,    32,    33,   103,    35,    36,    37,   104,    39,
      40,    41,     0,    42,    43,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   105,     0,     0,   106,     0,     0,   107,
      48,     0,    49,    50,     0,     0,     0,    52,    53,    54,
      55,    56,    57,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,     0,     0,     0,    12,
     108,    13,    14,    15,    97,    98,    18,    19,     0,     0,
       0,     0,    99,   100,   101,    23,    24,    25,    26,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   102,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,     0,    42,    43,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   224,     0,     0,    47,    48,
       0,    49,    50,     0,    51,     0,    52,    53,    54,    55,
      56,    57,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,   108,
      13,    14,    15,    97,    98,    18,    19,     0,     0,     0,
       0,    99,   100,   101,    23,    24,    25,    26,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   102,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
       0,    42,    43,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   276,     0,     0,   312,    48,     0,
      49,    50,     0,   313,     0,    52,    53,    54,    55,    56,
      57,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,   108,    13,
      14,    15,    97,    98,    18,    19,     0,     0,     0,     0,
      99,   100,   101,    23,    24,    25,    26,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   102,     0,     0,    32,
      33,   103,    35,    36,    37,   104,    39,    40,    41,     0,
      42,    43,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   106,     0,     0,   107,    48,     0,    49,
      50,     0,     0,     0,    52,    53,    54,    55,    56,    57,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,   108,    13,    14,
      15,    97,    98,    18,    19,     0,     0,     0,     0,    99,
     100,   101,    23,    24,    25,    26,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   102,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,     0,    42,
      43,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   276,     0,     0,   107,    48,     0,    49,    50,
       0,     0,     0,    52,    53,    54,    55,    56,    57,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,   108,    13,    14,    15,
      97,    98,    18,    19,     0,     0,     0,     0,    99,   100,
     101,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   102,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,     0,    42,    43,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   820,     0,     0,   107,    48,     0,    49,    50,     0,
       0,     0,    52,    53,    54,    55,    56,    57,     0,     0,
       0,     0,     0,     0,   517,   518,     0,     0,   519,     0,
       0,     0,     0,     0,     0,   108,   160,   161,   162,   163,
     164,   165,     0,   166,   167,     0,     0,   168,     0,     0,
       0,   169,   170,   171,   172,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   173,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     524,   525,   184,     0,   526,     0,     0,     0,     0,     0,
       0,   185,   160,   161,   162,   163,   164,   165,     0,   166,
     167,     0,     0,   168,     0,     0,     0,   169,   170,   171,
     172,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   173,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   529,   525,   184,     0,
     530,     0,     0,     0,     0,     0,     0,   185,   160,   161,
     162,   163,   164,   165,     0,   166,   167,     0,     0,   168,
       0,     0,     0,   169,   170,   171,   172,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   173,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   545,   518,   184,     0,   546,     0,     0,     0,
       0,     0,     0,   185,   160,   161,   162,   163,   164,   165,
       0,   166,   167,     0,     0,   168,     0,     0,     0,   169,
     170,   171,   172,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   173,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   582,   518,
     184,     0,   583,     0,     0,     0,     0,     0,     0,   185,
     160,   161,   162,   163,   164,   165,     0,   166,   167,     0,
       0,   168,     0,     0,     0,   169,   170,   171,   172,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   173,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   585,   525,   184,     0,   586,     0,
       0,     0,     0,     0,     0,   185,   160,   161,   162,   163,
     164,   165,     0,   166,   167,     0,     0,   168,     0,     0,
       0,   169,   170,   171,   172,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   173,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     599,   518,   184,     0,   600,     0,     0,     0,     0,     0,
       0,   185,   160,   161,   162,   163,   164,   165,     0,   166,
     167,     0,     0,   168,     0,     0,     0,   169,   170,   171,
     172,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   173,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,   602,   525,   184,     0,
     603,     0,     0,     0,     0,     0,     0,   185,   160,   161,
     162,   163,   164,   165,     0,   166,   167,     0,     0,   168,
       0,     0,     0,   169,   170,   171,   172,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   173,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   174,   175,   176,   177,   178,   179,   180,   181,
     182,   183,   640,   518,   184,     0,   641,     0,     0,     0,
       0,     0,     0,   185,   160,   161,   162,   163,   164,   165,
       0,   166,   167,     0,     0,   168,     0,     0,     0,   169,
     170,   171,   172,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   173,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   174,   175,
     176,   177,   178,   179,   180,   181,   182,   183,   643,   525,
     184,     0,   644,     0,     0,     0,     0,     0,     0,   185,
     160,   161,   162,   163,   164,   165,     0,   166,   167,     0,
       0,   168,     0,     0,     0,   169,   170,   171,   172,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   173,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   174,   175,   176,   177,   178,   179,
     180,   181,   182,   183,   889,   518,   184,     0,   890,     0,
       0,     0,     0,     0,     0,   185,   160,   161,   162,   163,
     164,   165,     0,   166,   167,     0,     0,   168,     0,     0,
       0,   169,   170,   171,   172,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   173,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     174,   175,   176,   177,   178,   179,   180,   181,   182,   183,
     892,   525,   184,     0,   893,     0,     0,     0,     0,     0,
       0,   185,   160,   161,   162,   163,   164,   165,     0,   166,
     167,     0,     0,   168,     0,     0,     0,   169,   170,   171,
     172,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   173,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   174,   175,   176,   177,
     178,   179,   180,   181,   182,   183,     0,     0,   184,     0,
       0,     0,     0,     0,     0,     0,     0,   185,   316,   317,
     318,   319,   320,   321,   322,   323,   324,   325,   326,   327,
     328,     0,     0,   329,   330,     0,     0,     0,  -220,   316,
     317,   318,   319,   320,   321,   322,   323,   324,   325,   326,
     327,   328,     0,     0,   329,   330,     0,     0,   331,     0,
     332,   333,   334,   335,   336,   337,   338,   339,   340,   341,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   331,
    -220,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   605,   316,   317,   318,   319,   320,   321,   322,   323,
     324,   325,   326,   327,   328,     0,     0,   329,   330,   316,
     317,   318,   319,   320,   321,   322,   323,   324,   325,   326,
    -516,  -516,     0,     0,   329,   330,     0,     0,     0,     0,
       0,     0,   331,     0,   332,   333,   334,   335,   336,   337,
     338,   339,   340,   341,     0,     0,     0,     0,     0,     0,
       0,   332,   333,   334,   335,   336,   337,   338,   339,   340,
     341,   316,   317,   318,   319,   320,   321,   322,     0,     0,
     325,   326,     0,     0,     0,     0,   329,   330,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   332,   333,   334,   335,   336,   337,   338,
     339,   340,   341
};

static const yytype_int16 yycheck[] =
{
       2,   343,     4,     5,     6,    71,     7,    28,    10,    11,
      12,   397,   215,    15,    16,    17,     7,     8,    20,    16,
      17,     4,    59,    20,   370,    27,   368,   247,   372,     2,
       8,     4,     5,     6,     7,   287,   391,    28,    22,   291,
      22,    47,    15,    62,   386,    47,    48,    49,   409,    51,
      28,    52,    17,     8,   692,    20,    13,    59,   400,   531,
     532,    52,    10,    11,    12,   106,    16,    17,   315,    71,
      20,    12,    51,    28,    47,   568,     1,   570,    51,    27,
     689,   442,   577,   692,    26,   533,   534,    27,   130,   261,
      16,    17,   587,    25,    20,    26,   130,   131,    71,    49,
      48,    81,    25,   105,    64,   107,    25,    25,   450,    25,
      94,   120,    94,    25,    25,    25,    16,    17,    81,   101,
      20,    62,    84,    72,    50,    81,    56,    57,    81,     5,
       6,    36,    37,   115,   107,    36,    37,    81,    26,    15,
     312,    58,   122,   342,   124,   344,   345,   346,   105,     0,
      50,    11,    12,   113,   114,   115,    83,   105,    50,   108,
      25,   124,    54,    25,   126,   105,   122,   130,   124,   110,
     369,   124,   128,   130,   131,    51,   103,    54,   120,    81,
     124,   664,   107,    92,   101,    58,   385,   670,   387,   120,
     130,   131,    81,    81,   126,    71,   128,   128,   544,   218,
     838,   110,   401,   126,   206,   130,   131,   126,   126,   128,
     126,   463,   102,    28,   126,   126,   126,   563,   562,    83,
     222,   223,   124,   129,   125,   130,    99,   100,    83,   838,
     678,   430,   120,   206,   122,   124,   124,    83,   103,   103,
     128,   103,   244,   108,   246,   247,   108,   244,   103,   246,
     247,    15,   451,    17,   256,   261,   129,   103,   129,   261,
     125,   126,   126,   125,   126,   737,   738,     2,   126,     4,
       5,     6,   130,   310,   222,   223,   103,   218,   315,   244,
      15,   246,   247,   256,   126,   126,   103,    81,   261,   130,
     783,   784,    81,    94,   244,   788,   246,   247,   300,   301,
     302,   303,   304,   305,   306,   307,   312,   802,   310,    81,
     312,   313,    47,   315,   351,   352,    51,   300,   244,    50,
     246,   247,   305,    83,    81,    83,   367,   300,   122,   576,
     124,    81,   305,   122,   313,   124,    71,   723,   128,   312,
     313,   343,    81,    81,   244,   103,   246,   247,    81,   351,
     352,   342,   124,   301,   302,   303,   304,   128,   306,   307,
      91,    92,   222,   223,   725,    26,   368,   124,   370,   371,
     126,   732,   107,    54,   124,    83,   367,   267,   103,   110,
     382,    83,   113,   876,   386,   124,   124,    83,   390,   592,
     392,   124,    83,   343,   385,   103,    26,   370,   400,   131,
      15,   103,    17,   424,   624,    26,    52,   103,   399,   382,
     310,   413,    54,    59,    60,   315,   413,   390,   368,   392,
      81,    81,   363,   371,   126,   427,   625,   626,    84,   775,
       2,    58,     4,   424,    26,    58,   386,   313,   429,    36,
      37,   301,   302,   303,   304,   789,   424,   802,   450,   126,
     400,    81,    25,   130,   427,   396,   397,   126,   449,   120,
      81,   122,   122,   124,   124,   413,   128,   128,    64,   424,
     120,   206,    99,   100,   101,    47,    99,   100,   101,   126,
     406,    64,    83,    79,    80,    56,    57,   413,   125,    81,
     120,   125,   122,   393,   124,   123,   842,   858,   128,   120,
     450,   122,   103,   124,   121,    14,    15,   128,    58,     2,
      54,     4,   125,     2,   110,   111,   112,   113,   114,   115,
      83,   256,    15,   240,   866,   126,   261,   417,   120,   419,
     122,    64,   124,    58,   101,   107,   128,   521,    81,   521,
     103,   543,   544,   904,   528,   435,   436,   531,   532,    99,
     100,   101,   101,   574,    47,   125,   126,    58,    47,    64,
      61,   563,    64,   126,   126,   300,   548,    54,    55,    90,
     305,   544,   573,    83,    99,   100,   101,   312,   313,   122,
      10,   124,   573,   574,    81,     8,    13,   653,   791,    17,
     563,    10,   126,   103,   797,   543,   574,   125,    99,   100,
     541,   125,   604,    50,   126,    52,    53,    54,    55,   111,
     112,   113,   114,   115,   107,   121,   126,    81,   107,   574,
     123,   125,   624,   564,   661,   122,    81,   124,   630,   666,
     667,   126,   126,   129,   206,   370,    50,   125,    52,    53,
      54,    55,   126,   645,    91,    92,   270,   382,   272,   123,
     274,   653,    81,    10,   103,   390,   604,   392,   122,   661,
     124,   126,   552,   110,   666,   667,   113,   122,   867,   124,
     126,    81,   645,    50,   624,    64,   576,    91,    92,    50,
     653,   128,   630,   543,   256,   126,   403,    83,    15,   261,
      79,    80,   427,   122,    81,   124,   110,   123,   624,   113,
      81,    10,   704,    52,    53,   707,    55,   103,    10,   126,
      59,    60,   122,   206,   124,   123,    92,   719,   720,   721,
     126,   125,   111,   112,   113,   114,   115,   126,   300,   766,
     126,   704,   729,   305,   125,   122,   121,   124,    10,   680,
     312,   122,   125,   124,   604,   125,   719,   720,   721,   751,
     752,   123,   125,    10,    84,   757,     9,   125,    10,   707,
      10,   827,   121,   256,   766,   126,   108,   256,   261,   125,
     772,   773,   261,   775,   126,   126,    10,   653,   751,   752,
      10,   126,   723,   126,   757,   125,    54,    92,   790,   126,
      10,    10,   794,   121,    10,    92,   686,   121,   370,   772,
     773,    10,   775,   729,    10,   126,   123,   300,    92,   544,
     382,   126,   305,    92,    15,     6,   879,   790,   390,   312,
     392,   794,    78,   312,   878,   827,    81,     7,   563,   729,
     789,    62,   562,   835,    39,    40,    41,    42,    43,   689,
     842,   689,   844,    -1,   846,    -1,   736,   707,    -1,    -1,
      -1,   853,    -1,   743,   827,   427,    -1,    -1,    -1,    -1,
      -1,   863,   835,    -1,   866,    -1,   868,   869,    -1,   842,
      -1,   844,    -1,   846,   865,   751,   752,   370,    -1,    -1,
     853,    -1,    -1,    -1,    -1,    -1,   888,    -1,    -1,   382,
     863,    -1,    -1,   382,   611,   868,   869,   390,    -1,   392,
      -1,   390,    -1,   392,    -1,   622,    -1,   909,    -1,    -1,
     645,   646,    -1,   915,   790,   888,   866,    -1,   653,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    64,
     856,    -1,    -1,    -1,   427,    -1,   909,    -1,   427,    -1,
      -1,    -1,   915,    -1,    79,    80,    -1,    -1,    -1,    -1,
      -1,   827,    -1,    -1,    -1,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    -1,   704,
      79,    80,   544,   108,   109,   110,   111,   112,   113,   114,
     115,    -1,    -1,    -1,   719,   720,   721,   863,    -1,    -1,
      -1,   563,   868,   869,    -1,   104,    -1,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   724,    -1,    -1,
     727,   728,   888,    -1,    -1,    -1,   751,   752,    -1,    -1,
      -1,   130,   757,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   909,    -1,    -1,    -1,   772,   773,   915,
     775,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   544,    -1,    -1,    -1,   790,    -1,    -1,    -1,   794,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     563,    -1,    -1,   645,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   827,    -1,    -1,   812,   813,    -1,    -1,    -1,
     835,    -1,    -1,    -1,    -1,    -1,    -1,   842,    -1,   844,
      -1,   846,    -1,    -1,    -1,    -1,    -1,    -1,   853,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   863,    -1,
      -1,    -1,   704,   868,   869,    -1,    -1,   854,   855,    -1,
      -1,    -1,    -1,   860,    -1,    -1,    -1,   719,   720,   721,
      16,    17,   645,   888,    20,    -1,   645,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   884,    -1,    -1,
      -1,    -1,    -1,    -1,   909,    -1,    -1,    -1,    44,    45,
     915,    -1,    -1,    49,    50,   757,   903,    -1,    -1,   906,
      -1,    -1,    -1,    59,    60,    -1,    -1,    -1,    -1,   916,
     772,   773,    -1,   775,    64,    65,    66,    67,    68,    69,
      70,   704,    -1,    73,    74,   704,    -1,    -1,    -1,    79,
      80,    -1,   794,    -1,    -1,    -1,   719,   720,   721,    -1,
     719,   720,   721,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    49,    50,    -1,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,    -1,    -1,   751,   752,
      -1,    -1,    -1,   835,   757,    -1,    -1,    -1,   757,    -1,
     842,    -1,   844,    -1,   846,    -1,    -1,     0,    -1,   772,
     773,   853,   775,   772,   773,     8,     9,    10,    -1,    -1,
      13,    14,    15,    -1,    17,    -1,    -1,   790,    -1,    -1,
      -1,   794,    -1,    -1,    27,   794,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    36,    37,    -1,    39,    40,    41,    42,
      43,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   835,    -1,    -1,    -1,   835,    -1,    -1,   842,
      -1,   844,    -1,   846,    -1,   844,    -1,   846,    81,    -1,
     853,    -1,    -1,    -1,   853,   231,   232,   233,    -1,    -1,
     863,    -1,    -1,    -1,    -1,   868,   869,    -1,   244,    -1,
     246,   247,   105,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   888,    -1,    -1,   121,    -1,
     266,   124,   125,    -1,    -1,   128,    -1,   130,   131,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   909,    -1,    -1,    -1,
      -1,    -1,   915,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   239,   240,    -1,    -1,    -1,    -1,    -1,
      -1,   247,    -1,    -1,   310,    -1,    -1,    -1,    -1,   315,
     316,   317,   318,   319,   320,   321,   322,   323,   324,   325,
     326,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,   338,   339,   340,   341,    -1,   343,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   351,   352,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   368,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   384,    -1,
     386,    -1,   388,   389,    -1,    -1,    -1,   393,    -1,    -1,
      -1,    -1,    -1,    -1,   400,    -1,    -1,   343,    -1,   405,
     406,    -1,    -1,   409,    -1,    -1,    -1,   413,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     426,    -1,   368,    -1,    -1,   431,    -1,    -1,    -1,    -1,
      -1,    -1,   378,   439,    -1,    -1,   442,    -1,   444,    -1,
     386,     0,    -1,    -1,   450,    -1,    -1,    -1,    -1,     8,
       9,    10,    -1,    -1,   400,    14,    15,   403,    17,    -1,
      -1,    -1,   468,   469,    -1,    -1,    -1,    26,    -1,    -1,
      -1,    -1,   418,   479,    -1,    -1,    -1,    36,    37,    -1,
      39,    40,    41,    42,    43,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   450,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    -1,    -1,
      79,    80,    81,    -1,    83,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   478,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   103,   104,    -1,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,    -1,    -1,    -1,
      -1,   120,   121,   122,    -1,   124,   125,   126,    -1,    -1,
     576,   130,   131,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   528,    -1,   590,   531,   532,   533,   534,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   608,    -1,   550,    -1,   612,    -1,    -1,    -1,
     616,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   624,    -1,
     566,    -1,   568,    -1,   570,    -1,    -1,    -1,    -1,    -1,
     576,   577,   638,   579,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   587,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   657,    -1,   659,   660,   661,   662,    -1,    -1,    -1,
     666,   667,    -1,    -1,    -1,   611,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   622,    -1,    -1,   685,
      -1,    -1,    -1,     2,    -1,     4,    -1,    -1,    -1,    -1,
      -1,    10,    11,    12,    -1,    -1,    -1,    16,    17,    -1,
      -1,    20,    -1,    -1,   710,    -1,   712,   713,    27,    -1,
     231,   232,   718,    -1,    -1,    -1,    -1,    -1,    -1,   725,
      -1,    -1,    -1,   729,    -1,    -1,   732,    -1,    47,    48,
      49,    -1,   678,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      59,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   758,    -1,    -1,    -1,   702,    -1,    -1,   705,
     766,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   724,    -1,
      -1,   727,   728,    -1,    -1,    -1,   105,    -1,   107,   310,
      -1,   737,   738,   799,   315,   801,    -1,    -1,    -1,    -1,
      -1,   807,    -1,    -1,   810,    -1,    -1,    -1,    -1,   815,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   783,   784,    -1,
      -1,    -1,   788,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     856,    -1,   858,    -1,    -1,    -1,   802,    -1,    -1,    -1,
     866,    -1,    -1,    -1,    -1,    -1,   812,   813,    -1,    -1,
      -1,    -1,   393,    -1,    -1,    -1,    -1,   883,    -1,   885,
      -1,    -1,    -1,    -1,   405,   406,    -1,   206,   409,    -1,
      -1,    -1,   413,    -1,    -1,    -1,    -1,    -1,   904,    -1,
      -1,    -1,    -1,   222,   223,    -1,   912,    -1,   854,   855,
      -1,    -1,    -1,    -1,   860,    -1,    -1,    -1,   439,    -1,
     866,   442,    -1,   444,    -1,   244,    -1,   246,   247,    -1,
     876,    -1,   878,   879,    -1,    -1,    -1,   256,   884,    -1,
      -1,    -1,   261,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   903,   479,    -1,
     906,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     916,    -1,    -1,    -1,    -1,    -1,    43,    -1,    -1,    -1,
      -1,   300,   301,   302,   303,   304,   305,   306,   307,    -1,
      -1,   310,    -1,   312,    -1,    -1,   315,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      -1,    -1,    79,    80,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   343,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   351,   352,    -1,    -1,    -1,   104,    -1,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   368,
      -1,   370,   371,    -1,    -1,   576,    -1,    -1,    -1,   126,
      -1,    -1,    -1,   382,    -1,    -1,    -1,   386,    -1,   590,
      -1,   390,    -1,   392,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   400,    -1,    -1,    -1,    -1,    -1,   608,    -1,    -1,
      -1,   612,    -1,    -1,   413,   616,     3,     4,     5,    -1,
       7,    -1,    -1,   624,    11,    12,    -1,    -1,   427,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,   450,    -1,    -1,    -1,    -1,   657,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   685,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
      -1,    88,    89,    -1,    -1,    -1,    93,    94,    95,    96,
      97,    98,    -1,    -1,    -1,    -1,    -1,   718,    -1,    -1,
      -1,    -1,    -1,    -1,   725,    -1,    -1,    -1,    -1,   116,
      -1,   732,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   126,
      -1,    -1,    -1,    -1,   543,   544,    -1,    -1,    -1,    -1,
      -1,    -1,    43,    -1,    -1,    -1,    -1,   758,    -1,    -1,
      -1,    -1,    -1,    -1,   563,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    -1,    -1,    79,    80,
      -1,    -1,    64,    65,    66,    67,    68,    69,    70,    71,
     801,    73,    74,    -1,    -1,   604,    -1,    79,    80,   810,
      -1,    -1,    -1,   104,   815,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   624,    -1,    -1,    -1,    -1,
      -1,   630,    -1,    -1,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,    -1,    -1,   645,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   856,    -1,   858,    -1,    -1,
      -1,    -1,   661,    -1,    -1,    -1,    -1,   666,   667,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   883,    -1,   885,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   904,    -1,   704,    -1,    -1,   707,    -1,
      -1,   912,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     719,   720,   721,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,   757,    29,
      30,    31,    32,    33,    34,    35,    -1,   766,    -1,    -1,
      -1,    -1,    -1,   772,   773,    45,   775,    -1,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    -1,    -1,    -1,   794,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    91,    92,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   835,    -1,   108,    -1,
      -1,    -1,    -1,   842,    -1,   844,   116,   846,    -1,    -1,
      -1,    -1,     0,     1,   853,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,   866,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      38,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
      88,    89,    -1,    91,    -1,    93,    94,    95,    96,    97,
      98,    -1,    -1,    -1,    -1,    -1,    -1,     0,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     8,     9,    10,   116,   117,
     118,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   130,   131,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    36,    37,    -1,    39,    40,    41,    42,
      43,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    -1,    -1,    79,    80,    -1,    -1,
      -1,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    -1,    -1,    79,    80,    81,    -1,
      83,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     103,   104,    -1,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,    -1,    -1,    -1,    -1,    -1,   121,   122,
      -1,   124,   125,   126,    -1,    -1,    -1,   130,   131,     1,
      -1,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    -1,    -1,    15,    16,    -1,    18,    19,    20,    21,
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
      -1,     1,    -1,     3,     4,     5,     6,     7,   130,   131,
      10,    11,    12,    -1,    14,    15,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    38,    -1,
      -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    91,    -1,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   116,   117,   118,    -1,
      -1,    -1,    -1,     1,    -1,     3,     4,     5,     6,     7,
     130,   131,    10,    11,    12,    -1,    -1,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      38,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
      88,    89,    -1,    91,    -1,    93,    94,    95,    96,    97,
      98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,   117,
     118,    -1,    -1,    -1,    -1,     1,    -1,     3,     4,     5,
       6,     7,   130,   131,    10,    11,    12,    -1,    -1,    15,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    91,    -1,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,    -1,    -1,     1,    -1,
       3,     4,     5,     6,     7,    -1,     9,    10,    11,    12,
     116,   117,   118,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,   130,   131,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    38,    -1,    -1,    -1,    -1,
      -1,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,    -1,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,     1,    -1,     3,     4,     5,     6,     7,    -1,    -1,
      -1,    11,    12,   116,   117,   118,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,   130,   131,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    38,    -1,
      -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    91,    -1,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   116,   117,   118,    -1,
      -1,    -1,    -1,    -1,    -1,   125,    -1,    -1,    -1,    -1,
     130,   131,     1,    -1,     3,     4,     5,     6,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    38,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,
      89,    -1,    91,    -1,    93,    94,    95,    96,    97,    98,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,   117,   118,
      -1,    -1,    -1,    -1,    -1,    -1,   125,    -1,    -1,    -1,
      -1,   130,   131,     1,    -1,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      38,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
      88,    89,    -1,    91,    -1,    93,    94,    95,    96,    97,
      98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,   117,
     118,    -1,    -1,   121,    -1,     1,    -1,     3,     4,     5,
       6,     7,   130,   131,    10,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    91,    -1,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
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
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,   116,   117,   118,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,   131,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    38,    -1,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    -1,    -1,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   116,   117,   118,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     130,   131,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    -1,    -1,
      -1,    -1,    -1,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    -1,    -1,    -1,    -1,    -1,
      -1,    62,    63,    64,    65,    66,    67,    -1,    69,    70,
      -1,    -1,    73,    -1,    -1,    -1,    77,    78,    79,    80,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      91,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,    -1,    -1,   118,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,   127,   128,    11,    12,
      -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    26,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,    92,
      93,    94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,   116,   117,   118,    16,   120,    18,    19,
      20,    21,    22,    23,    24,   128,    26,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    91,    92,    93,    94,    95,    96,    97,    98,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,   116,   117,   118,    16,
     120,    18,    19,    20,    21,    22,    23,    24,   128,    26,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
      -1,    88,    89,    -1,    91,    92,    93,    94,    95,    96,
      97,    98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,   116,
     117,   118,    16,   120,    18,    19,    20,    21,    22,    23,
      24,   128,    -1,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    45,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    -1,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    83,
      -1,    85,    86,    -1,    88,    89,    -1,    91,    92,    93,
      94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,   103,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,   116,   117,   118,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,   128,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    82,    83,    -1,    85,    86,    -1,    88,    89,    -1,
      91,    92,    93,    94,    95,    96,    97,    98,    -1,    -1,
      -1,    -1,   103,    -1,    -1,     3,     4,     5,    -1,     7,
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
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,   116,   117,   118,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,   128,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    -1,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,
      92,    93,    94,    95,    96,    97,    98,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,   116,   117,   118,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,   128,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,
      89,    -1,    91,    92,    93,    94,    95,    96,    97,    98,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,   117,   118,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   128,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,
      -1,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    -1,    -1,    -1,    -1,    -1,    -1,    62,
      63,    64,    65,    66,    67,    -1,    69,    70,    -1,    -1,
      73,    -1,    -1,    -1,    77,    78,    79,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,    -1,    -1,   118,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   127,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    -1,    54,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    62,    63,    64,    65,    66,    67,
      -1,    69,    70,    -1,    -1,    73,    -1,    -1,    -1,    77,
      78,    79,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    -1,    93,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,    -1,    -1,
     118,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   127,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,
      -1,    44,    45,    46,    47,    48,    49,    50,    51,    -1,
      -1,    54,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    62,
      63,    64,    65,    66,    67,    -1,    69,    70,    -1,    -1,
      73,    -1,    -1,    -1,    77,    78,    79,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,
      93,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,    -1,    -1,   118,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   127,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,
      48,    49,    50,    51,    -1,    -1,    54,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    62,    63,    64,    65,    66,    67,
      -1,    69,    70,    -1,    -1,    73,    -1,    -1,    -1,    77,
      78,    79,    80,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,    -1,    -1,
     118,    -1,     3,     4,     5,    -1,     7,    -1,    -1,   127,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
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
      22,    23,    24,    -1,    -1,   126,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    -1,
      62,    63,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      82,    -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,
      92,    93,    94,    95,    96,    97,    98,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,     6,     7,   116,   117,   118,    11,    12,    -1,
      -1,    -1,    16,   125,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    -1,    62,    63,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,
      -1,    85,    86,    -1,    88,    89,    -1,    91,    -1,    93,
      94,    95,    96,    97,    98,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,   116,   117,   118,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    38,    -1,    -1,
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
      35,    -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    -1,
      45,    -1,    -1,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    62,    63,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
      85,    86,    -1,    88,    89,    -1,    -1,    -1,    93,    94,
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
      -1,    -1,    85,    86,    -1,    88,    89,    -1,    91,    -1,
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
      -1,    91,    -1,    93,    94,    95,    96,    97,    98,    -1,
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
      -1,    -1,    93,    94,    95,    96,    97,    98,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,   116,   117,   118,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
      88,    89,    -1,    -1,    -1,    93,    94,    95,    96,    97,
      98,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,   116,   117,
     118,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,    34,
      35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      45,    -1,    -1,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,
      85,    86,    -1,    88,    89,    -1,    91,    92,    93,    94,
      95,    96,    97,    98,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,
      16,   116,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    79,    -1,    -1,    82,    -1,    -1,    85,
      86,    -1,    88,    89,    -1,    -1,    -1,    93,    94,    95,
      96,    97,    98,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,
     116,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,
      -1,    88,    89,    -1,    91,    -1,    93,    94,    95,    96,
      97,    98,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,   116,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,
      88,    89,    -1,    91,    -1,    93,    94,    95,    96,    97,
      98,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,   116,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,
      89,    -1,    -1,    -1,    93,    94,    95,    96,    97,    98,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,   116,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    82,    -1,    -1,    85,    86,    -1,    88,    89,
      -1,    -1,    -1,    93,    94,    95,    96,    97,    98,    -1,
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
      -1,    -1,    -1,    -1,    50,    51,    -1,    -1,    54,    -1,
      -1,    -1,    -1,    -1,    -1,   116,    62,    63,    64,    65,
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
     110,   111,   112,   113,   114,   115,    -1,    -1,   118,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   127,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    -1,    -1,    79,    80,    -1,    -1,    -1,    84,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    -1,    -1,    79,    80,    -1,    -1,   104,    -1,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   104,
     126,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   126,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    -1,    -1,    79,    80,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    -1,    -1,    79,    80,    -1,    -1,    -1,    -1,
      -1,    -1,   104,    -1,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,    64,    65,    66,    67,    68,    69,    70,    -1,    -1,
      73,    74,    -1,    -1,    -1,    -1,    79,    80,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115
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
     152,   153,   154,   155,   165,   182,   199,   211,   212,   225,
     226,   227,   228,   229,   230,   231,   234,   242,   244,   245,
     246,   247,   248,   249,   268,   277,   138,    21,    22,    29,
      30,    31,    45,    50,    54,    79,    82,    85,   116,   156,
     157,   182,   199,   246,   249,   268,   157,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    44,    45,    46,    47,    48,    49,    50,    51,    54,
      62,    63,    64,    65,    66,    67,    69,    70,    73,    77,
      78,    79,    80,    91,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   118,   127,   128,   158,   163,   164,
     247,   263,    32,    33,    34,    35,    48,    49,    50,    54,
     158,   159,   160,   161,   242,   244,   183,    82,   141,   142,
     155,   199,   246,   248,   249,   142,   130,   131,   142,   272,
     275,   276,   186,   188,    82,   149,   155,   199,   204,   246,
     249,    91,    92,   117,   148,   165,   167,   171,   178,   180,
     266,   267,   171,   171,   128,   173,   174,   128,   169,   173,
     141,    52,   160,   130,   273,   140,   120,   165,   199,   165,
      54,    85,   136,   150,   151,   141,    91,   148,   168,   180,
     266,   277,   180,   265,   266,   277,    82,   154,   199,   246,
     249,    52,    53,    55,   158,   237,   243,   236,   237,   237,
     129,   232,   129,   235,    56,    57,   143,   165,   165,   272,
     276,    39,    40,    41,    42,    43,    36,    37,    28,   208,
     103,   126,    85,    91,   152,   103,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    79,
      80,   104,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,    81,   122,   124,    81,   124,    26,   120,   213,
     228,    83,    83,   169,   173,   213,   273,   141,    50,    54,
     156,    56,    57,     1,   107,   250,   275,    81,   122,   124,
     195,   264,   196,    81,   124,   271,   126,   135,   136,    54,
      13,   105,   200,   275,   103,    81,   122,   124,    83,    83,
     200,   272,    15,    17,   218,   131,   142,   142,    54,    81,
     122,   124,    25,   167,   167,    84,   126,   179,   277,   126,
     179,   125,   171,    86,   171,   175,   148,   171,   180,   211,
     277,    52,    59,    60,   139,   128,   166,   120,   136,    81,
     124,    83,   150,   125,   125,   184,   165,   273,   123,   126,
     130,   274,   126,   274,   126,   274,   121,   274,    54,    81,
     122,   124,    58,    99,   100,   101,   238,   101,   238,   101,
      61,   101,   101,   233,   238,   101,    58,   101,    64,    64,
     138,   142,   142,   142,   142,   138,   141,   141,   209,    91,
     143,   167,   180,   181,   150,   154,   126,   143,   165,   167,
     181,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,   165,   165,   165,
     165,   165,   165,   165,   165,   165,   165,    50,    51,    54,
     163,   269,   270,   168,    50,    51,    54,   163,   269,    50,
      54,   269,   269,   216,   214,   143,   165,   143,   165,    90,
     145,   193,   275,   251,   192,    50,    54,   156,   269,   168,
     269,   135,   141,    50,    52,    53,    54,    55,    91,    92,
     110,   113,   128,   252,   253,   254,   255,   256,   257,   258,
     259,   260,   261,   197,   162,    10,     8,   221,   277,   136,
      13,   165,    50,    54,   168,    50,    54,   136,   218,   136,
      91,   180,   219,    10,    27,   105,   201,   275,   201,    50,
      54,   168,    50,    54,   190,   126,   179,   167,    91,   167,
     178,   266,    91,   167,   267,   125,    91,   165,   167,   172,
     176,   178,   266,   273,   126,    81,   124,   273,   125,   160,
     185,   165,   136,   121,   165,   125,   273,   273,    91,   167,
      50,    54,   168,    50,    54,   240,   239,   129,   238,   129,
     165,   165,    72,   108,   207,   277,   167,   126,   125,    43,
     105,    83,    83,   169,   173,   123,    83,    83,   169,   170,
     173,   277,   170,   173,   170,   173,   207,   207,   146,   275,
     142,   135,   123,    10,   273,   103,   253,   135,   275,   126,
     262,   277,   126,   262,    50,   126,   262,    50,   158,   160,
     167,   181,   222,   277,    15,   203,   277,    14,   202,   203,
      83,   123,    83,    83,   203,    10,    10,   167,   126,   200,
     187,   189,   123,   142,   167,   126,   179,   167,   167,   126,
     177,   125,   126,   179,   125,   148,   211,   269,   269,   125,
     141,   121,   125,   165,   123,   136,    52,    53,    55,   241,
     249,    91,    92,   108,   152,   205,   206,   210,    91,   167,
     165,   165,   143,   165,   165,   145,    83,   143,   165,   143,
     165,   145,   217,   215,   207,   194,   275,    10,   125,   167,
     273,    10,   254,   257,   259,   261,    50,   256,   259,   198,
      84,   223,   277,   136,     9,   224,   277,   142,    10,    83,
      10,    91,   136,   136,   136,   201,   179,    91,   179,   179,
      91,   178,   180,   266,   125,    91,   273,   125,   273,   121,
      82,   126,   155,   199,   246,   249,   155,   126,   108,   136,
     167,   143,   165,   136,   136,   147,   135,   125,   126,   262,
     262,   262,   252,   155,   200,   136,   200,   167,   203,   218,
     220,    10,    10,   191,   165,   167,   126,   179,   126,   179,
     167,   125,    54,    92,   126,    81,   122,   124,    91,    92,
     152,    10,    10,   121,   136,    10,   259,   135,   136,   136,
     136,   179,   179,    91,   266,    91,   179,   155,    92,    50,
      54,   168,    50,    54,   126,   155,   155,   121,   262,    10,
     221,   202,    10,   167,   126,   179,   167,   155,   123,    92,
     126,   179,    91,   179,   155,    92,   167,   155,   179
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
#line 353 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			lex_state = EXPR_BEG;
                        top_local_init();
			if (ruby_class == rb_cObject) class_nest = 0;
			else class_nest = 1;
		    ;}
    break;

  case 3:
#line 360 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 4:
#line 382 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 5:
#line 399 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			void_stmts((yyvsp[(1) - (2)].node));
			fixup_nodes(&deferred_nodes);
		        (yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 7:
#line 408 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = newline_node((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 8:
#line 412 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append((yyvsp[(1) - (3)].node), newline_node((yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 9:
#line 416 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = remove_begin((yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 10:
#line 421 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {lex_state = EXPR_FNAME;;}
    break;

  case 11:
#line 422 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.node) = NEW_ALIAS((yyvsp[(2) - (4)].node), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 12:
#line 426 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.node) = NEW_VALIAS((yyvsp[(2) - (3)].id), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 13:
#line 430 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			char buf[3];

			sprintf(buf, "$%c", (char)(yyvsp[(3) - (3)].node)->nd_nth);
		        (yyval.node) = NEW_VALIAS((yyvsp[(2) - (3)].id), rb_intern(buf));
		    ;}
    break;

  case 14:
#line 437 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        yyerror("can't make alias for the number variables");
		        (yyval.node) = 0;
		    ;}
    break;

  case 15:
#line 442 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 16:
#line 446 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_IF(cond((yyvsp[(3) - (3)].node)), remove_begin((yyvsp[(1) - (3)].node)), 0);
		        fixpos((yyval.node), (yyvsp[(3) - (3)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
		            (yyval.node)->nd_else = (yyval.node)->nd_body;
		            (yyval.node)->nd_body = 0;
			}
		    ;}
    break;

  case 17:
#line 455 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_UNLESS(cond((yyvsp[(3) - (3)].node)), remove_begin((yyvsp[(1) - (3)].node)), 0);
		        fixpos((yyval.node), (yyvsp[(3) - (3)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
		            (yyval.node)->nd_body = (yyval.node)->nd_else;
		            (yyval.node)->nd_else = 0;
			}
		    ;}
    break;

  case 18:
#line 464 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 19:
#line 476 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 20:
#line 488 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			NODE *resq = NEW_RESBODY(0, remove_begin((yyvsp[(3) - (3)].node)), 0);
			(yyval.node) = NEW_RESCUE(remove_begin((yyvsp[(1) - (3)].node)), resq, 0);
		    ;}
    break;

  case 21:
#line 493 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (in_def || in_single) {
			    yyerror("BEGIN in method");
			}
			local_push(0);
		    ;}
    break;

  case 22:
#line 500 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			ruby_eval_tree_begin = block_append(ruby_eval_tree_begin,
						            NEW_PREEXE((yyvsp[(4) - (5)].node)));
		        local_pop();
		        (yyval.node) = 0;
		    ;}
    break;

  case 23:
#line 507 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (in_def || in_single) {
			    rb_warn("END in method; use at_exit");
			}

			(yyval.node) = NEW_ITER(0, NEW_POSTEXE(), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 24:
#line 515 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = node_assign((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 25:
#line 519 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			value_expr((yyvsp[(3) - (3)].node));
			(yyvsp[(1) - (3)].node)->nd_value = ((yyvsp[(1) - (3)].node)->nd_head) ? NEW_TO_ARY((yyvsp[(3) - (3)].node)) : NEW_ARRAY((yyvsp[(3) - (3)].node));
			(yyval.node) = (yyvsp[(1) - (3)].node);
		    ;}
    break;

  case 26:
#line 525 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 27:
#line 550 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 28:
#line 566 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 29:
#line 578 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 30:
#line 590 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 31:
#line 602 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        rb_backref_error((yyvsp[(1) - (3)].node));
			(yyval.node) = 0;
		    ;}
    break;

  case 32:
#line 607 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = node_assign((yyvsp[(1) - (3)].node), NEW_SVALUE((yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 33:
#line 611 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyvsp[(1) - (3)].node)->nd_value = ((yyvsp[(1) - (3)].node)->nd_head) ? NEW_TO_ARY((yyvsp[(3) - (3)].node)) : NEW_ARRAY((yyvsp[(3) - (3)].node));
			(yyval.node) = (yyvsp[(1) - (3)].node);
		    ;}
    break;

  case 34:
#line 616 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyvsp[(1) - (3)].node)->nd_value = (yyvsp[(3) - (3)].node);
			(yyval.node) = (yyvsp[(1) - (3)].node);
		    ;}
    break;

  case 37:
#line 625 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = logop(NODE_AND, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 38:
#line 629 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = logop(NODE_OR, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 39:
#line 633 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_NOT(cond((yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 40:
#line 637 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_NOT(cond((yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 42:
#line 644 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			value_expr((yyval.node));
			(yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 45:
#line 653 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_RETURN(ret_args((yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 46:
#line 657 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_BREAK(ret_args((yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 47:
#line 661 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_NEXT(ret_args((yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 49:
#line 668 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 50:
#line 672 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 51:
#line 678 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.vars) = dyna_push();
			(yyvsp[(1) - (1)].num) = ruby_sourceline;
		    ;}
    break;

  case 52:
#line 682 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.vars) = ruby_dyna_vars;;}
    break;

  case 53:
#line 685 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_ITER((yyvsp[(3) - (6)].node), 0, dyna_init((yyvsp[(5) - (6)].node), (yyvsp[(4) - (6)].vars)));
			nd_set_line((yyval.node), (yyvsp[(1) - (6)].num));
			dyna_pop((yyvsp[(2) - (6)].vars));
		    ;}
    break;

  case 54:
#line 693 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_fcall((yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (2)].node));
		   ;}
    break;

  case 55:
#line 698 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		   ;}
    break;

  case 56:
#line 710 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    ;}
    break;

  case 57:
#line 715 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		   ;}
    break;

  case 58:
#line 727 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    ;}
    break;

  case 59:
#line 732 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		   ;}
    break;

  case 60:
#line 744 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_super((yyvsp[(2) - (2)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 61:
#line 749 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_yield((yyvsp[(2) - (2)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 63:
#line 757 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 65:
#line 764 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN(NEW_LIST((yyvsp[(2) - (3)].node)), 0);
		    ;}
    break;

  case 66:
#line 770 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN((yyvsp[(1) - (1)].node), 0);
		    ;}
    break;

  case 67:
#line 774 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN(list_append((yyvsp[(1) - (2)].node),(yyvsp[(2) - (2)].node)), 0);
		    ;}
    break;

  case 68:
#line 778 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 69:
#line 782 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN((yyvsp[(1) - (2)].node), -1);
		    ;}
    break;

  case 70:
#line 786 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN(0, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 71:
#line 790 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN(0, -1);
		    ;}
    break;

  case 73:
#line 797 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 74:
#line 803 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST((yyvsp[(1) - (2)].node));
		    ;}
    break;

  case 75:
#line 807 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 76:
#line 813 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = assignable((yyvsp[(1) - (1)].id), 0);
		    ;}
    break;

  case 77:
#line 817 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = aryset((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 78:
#line 821 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 79:
#line 825 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 80:
#line 829 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 81:
#line 833 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.node) = NEW_CDECL(0, 0, NEW_COLON2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id)));
		    ;}
    break;

  case 82:
#line 839 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.node) = NEW_CDECL(0, 0, NEW_COLON3((yyvsp[(2) - (2)].id)));
		    ;}
    break;

  case 83:
#line 845 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        rb_backref_error((yyvsp[(1) - (1)].node));
			(yyval.node) = 0;
		    ;}
    break;

  case 84:
#line 852 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = assignable((yyvsp[(1) - (1)].id), 0);
		    ;}
    break;

  case 85:
#line 856 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = aryset((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 86:
#line 860 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 87:
#line 864 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 88:
#line 868 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = attrset((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 89:
#line 872 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.node) = NEW_CDECL(0, 0, NEW_COLON2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id)));
		    ;}
    break;

  case 90:
#line 878 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.node) = NEW_CDECL(0, 0, NEW_COLON3((yyvsp[(2) - (2)].id)));
		    ;}
    break;

  case 91:
#line 884 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        rb_backref_error((yyvsp[(1) - (1)].node));
			(yyval.node) = 0;
		    ;}
    break;

  case 92:
#line 891 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			yyerror("class/module name must be CONSTANT");
		    ;}
    break;

  case 94:
#line 898 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_COLON3((yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 95:
#line 902 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_COLON2(0, (yyval.node));
		    ;}
    break;

  case 96:
#line 906 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_COLON2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 100:
#line 915 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			lex_state = EXPR_END;
			(yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 101:
#line 920 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			lex_state = EXPR_END;
			(yyval.id) = (yyvsp[(1) - (1)].id);
		    ;}
    break;

  case 104:
#line 931 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIT(ID2SYM((yyvsp[(1) - (1)].id)));
		    ;}
    break;

  case 106:
#line 938 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_UNDEF((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 107:
#line 941 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {lex_state = EXPR_FNAME;;}
    break;

  case 108:
#line 942 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append((yyvsp[(1) - (4)].node), NEW_UNDEF((yyvsp[(4) - (4)].node)));
		    ;}
    break;

  case 109:
#line 947 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '|'; ;}
    break;

  case 110:
#line 948 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '^'; ;}
    break;

  case 111:
#line 949 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '&'; ;}
    break;

  case 112:
#line 950 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tCMP; ;}
    break;

  case 113:
#line 951 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tEQ; ;}
    break;

  case 114:
#line 952 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tEQQ; ;}
    break;

  case 115:
#line 953 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tMATCH; ;}
    break;

  case 116:
#line 954 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '>'; ;}
    break;

  case 117:
#line 955 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tGEQ; ;}
    break;

  case 118:
#line 956 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '<'; ;}
    break;

  case 119:
#line 957 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tLEQ; ;}
    break;

  case 120:
#line 958 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tLSHFT; ;}
    break;

  case 121:
#line 959 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tRSHFT; ;}
    break;

  case 122:
#line 960 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '+'; ;}
    break;

  case 123:
#line 961 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '-'; ;}
    break;

  case 124:
#line 962 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '*'; ;}
    break;

  case 125:
#line 963 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '*'; ;}
    break;

  case 126:
#line 964 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '/'; ;}
    break;

  case 127:
#line 965 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '%'; ;}
    break;

  case 128:
#line 966 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tPOW; ;}
    break;

  case 129:
#line 967 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '~'; ;}
    break;

  case 130:
#line 968 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tUPLUS; ;}
    break;

  case 131:
#line 969 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tUMINUS; ;}
    break;

  case 132:
#line 970 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tAREF; ;}
    break;

  case 133:
#line 971 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = tASET; ;}
    break;

  case 134:
#line 972 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    { (yyval.id) = '`'; ;}
    break;

  case 175:
#line 985 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = node_assign((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 176:
#line 989 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = node_assign((yyvsp[(1) - (5)].node), NEW_RESCUE((yyvsp[(3) - (5)].node), NEW_RESBODY(0,(yyvsp[(5) - (5)].node),0), 0));
		    ;}
    break;

  case 177:
#line 993 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 178:
#line 1018 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 179:
#line 1034 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 180:
#line 1046 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 181:
#line 1058 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 182:
#line 1070 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			yyerror("constant re-assignment");
			(yyval.node) = 0;
		    ;}
    break;

  case 183:
#line 1075 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			yyerror("constant re-assignment");
			(yyval.node) = 0;
		    ;}
    break;

  case 184:
#line 1080 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        rb_backref_error((yyvsp[(1) - (3)].node));
			(yyval.node) = 0;
		    ;}
    break;

  case 185:
#line 1085 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			value_expr((yyvsp[(1) - (3)].node));
			value_expr((yyvsp[(3) - (3)].node));
			(yyval.node) = NEW_DOT2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
			if (nd_type((yyvsp[(1) - (3)].node)) == NODE_LIT && FIXNUM_P((yyvsp[(1) - (3)].node)->nd_lit) &&
			    nd_type((yyvsp[(3) - (3)].node)) == NODE_LIT && FIXNUM_P((yyvsp[(3) - (3)].node)->nd_lit)) {
			    deferred_nodes = list_append(deferred_nodes, (yyval.node));
			}
		    ;}
    break;

  case 186:
#line 1095 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			value_expr((yyvsp[(1) - (3)].node));
			value_expr((yyvsp[(3) - (3)].node));
			(yyval.node) = NEW_DOT3((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
			if (nd_type((yyvsp[(1) - (3)].node)) == NODE_LIT && FIXNUM_P((yyvsp[(1) - (3)].node)->nd_lit) &&
			    nd_type((yyvsp[(3) - (3)].node)) == NODE_LIT && FIXNUM_P((yyvsp[(3) - (3)].node)->nd_lit)) {
			    deferred_nodes = list_append(deferred_nodes, (yyval.node));
			}
		    ;}
    break;

  case 187:
#line 1105 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '+', 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 188:
#line 1109 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.node) = call_op((yyvsp[(1) - (3)].node), '-', 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 189:
#line 1113 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.node) = call_op((yyvsp[(1) - (3)].node), '*', 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 190:
#line 1117 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '/', 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 191:
#line 1121 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '%', 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 192:
#line 1125 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tPOW, 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 193:
#line 1129 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op(call_op((yyvsp[(2) - (4)].node), tPOW, 1, (yyvsp[(4) - (4)].node)), tUMINUS, 0, 0);
		    ;}
    break;

  case 194:
#line 1133 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op(call_op((yyvsp[(2) - (4)].node), tPOW, 1, (yyvsp[(4) - (4)].node)), tUMINUS, 0, 0);
		    ;}
    break;

  case 195:
#line 1137 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if ((yyvsp[(2) - (2)].node) && nd_type((yyvsp[(2) - (2)].node)) == NODE_LIT) {
			    (yyval.node) = (yyvsp[(2) - (2)].node);
			}
			else {
			    (yyval.node) = call_op((yyvsp[(2) - (2)].node), tUPLUS, 0, 0);
			}
		    ;}
    break;

  case 196:
#line 1146 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(2) - (2)].node), tUMINUS, 0, 0);
		    ;}
    break;

  case 197:
#line 1150 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.node) = call_op((yyvsp[(1) - (3)].node), '|', 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 198:
#line 1154 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '^', 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 199:
#line 1158 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '&', 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 200:
#line 1162 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tCMP, 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 201:
#line 1166 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '>', 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 202:
#line 1170 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tGEQ, 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 203:
#line 1174 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), '<', 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 204:
#line 1178 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tLEQ, 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 205:
#line 1182 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tEQ, 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 206:
#line 1186 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tEQQ, 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 207:
#line 1190 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_NOT(call_op((yyvsp[(1) - (3)].node), tEQ, 1, (yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 208:
#line 1194 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = match_gen((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 209:
#line 1198 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_NOT(match_gen((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node)));
		    ;}
    break;

  case 210:
#line 1202 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_NOT(cond((yyvsp[(2) - (2)].node)));
		    ;}
    break;

  case 211:
#line 1206 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(2) - (2)].node), '~', 0, 0);
		    ;}
    break;

  case 212:
#line 1210 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tLSHFT, 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 213:
#line 1214 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = call_op((yyvsp[(1) - (3)].node), tRSHFT, 1, (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 214:
#line 1218 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = logop(NODE_AND, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 215:
#line 1222 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = logop(NODE_OR, (yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 216:
#line 1225 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {in_defined = 1;;}
    break;

  case 217:
#line 1226 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        in_defined = 0;
			(yyval.node) = NEW_DEFINED((yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 218:
#line 1231 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_IF(cond((yyvsp[(1) - (5)].node)), (yyvsp[(3) - (5)].node), (yyvsp[(5) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (5)].node));
		    ;}
    break;

  case 219:
#line 1236 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 220:
#line 1242 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			value_expr((yyvsp[(1) - (1)].node));
			(yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 222:
#line 1250 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST((yyvsp[(1) - (2)].node));
		    ;}
    break;

  case 223:
#line 1254 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 224:
#line 1258 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			value_expr((yyvsp[(4) - (5)].node));
			(yyval.node) = arg_concat((yyvsp[(1) - (5)].node), (yyvsp[(4) - (5)].node));
		    ;}
    break;

  case 225:
#line 1263 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].node)));
		    ;}
    break;

  case 226:
#line 1267 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			value_expr((yyvsp[(2) - (3)].node));
			(yyval.node) = NEW_NEWLINE(NEW_SPLAT((yyvsp[(2) - (3)].node)));
		    ;}
    break;

  case 227:
#line 1274 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 228:
#line 1278 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (4)].node);
		    ;}
    break;

  case 229:
#line 1282 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST((yyvsp[(2) - (4)].node));
		    ;}
    break;

  case 230:
#line 1286 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append((yyvsp[(2) - (6)].node), (yyvsp[(4) - (6)].node));
		    ;}
    break;

  case 233:
#line 1296 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 234:
#line 1300 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_blk_pass((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 235:
#line 1304 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_concat((yyvsp[(1) - (5)].node), (yyvsp[(4) - (5)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 236:
#line 1309 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].node)));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 237:
#line 1314 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_concat(NEW_LIST(NEW_HASH((yyvsp[(1) - (5)].node))), (yyvsp[(4) - (5)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 238:
#line 1319 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (4)].node), NEW_HASH((yyvsp[(3) - (4)].node)));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 239:
#line 1324 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			value_expr((yyvsp[(6) - (7)].node));
			(yyval.node) = arg_concat(list_append((yyvsp[(1) - (7)].node), NEW_HASH((yyvsp[(3) - (7)].node))), (yyvsp[(6) - (7)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(7) - (7)].node));
		    ;}
    break;

  case 240:
#line 1330 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_blk_pass(NEW_SPLAT((yyvsp[(2) - (3)].node)), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 242:
#line 1337 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_blk_pass(list_concat(NEW_LIST((yyvsp[(1) - (4)].node)),(yyvsp[(3) - (4)].node)), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 243:
#line 1341 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
                        (yyval.node) = arg_blk_pass((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
                    ;}
    break;

  case 244:
#line 1345 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_concat(NEW_LIST((yyvsp[(1) - (5)].node)), (yyvsp[(4) - (5)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 245:
#line 1350 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
                       (yyval.node) = arg_concat(list_concat(NEW_LIST((yyvsp[(1) - (7)].node)),(yyvsp[(3) - (7)].node)), (yyvsp[(6) - (7)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(7) - (7)].node));
		    ;}
    break;

  case 246:
#line 1355 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].node)));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 247:
#line 1360 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_concat(NEW_LIST(NEW_HASH((yyvsp[(1) - (5)].node))), (yyvsp[(4) - (5)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 248:
#line 1365 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append(NEW_LIST((yyvsp[(1) - (4)].node)), NEW_HASH((yyvsp[(3) - (4)].node)));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 249:
#line 1370 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append(list_concat(NEW_LIST((yyvsp[(1) - (6)].node)),(yyvsp[(3) - (6)].node)), NEW_HASH((yyvsp[(5) - (6)].node)));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 250:
#line 1375 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_concat(list_append(NEW_LIST((yyvsp[(1) - (7)].node)), NEW_HASH((yyvsp[(3) - (7)].node))), (yyvsp[(6) - (7)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(7) - (7)].node));
		    ;}
    break;

  case 251:
#line 1380 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_concat(list_append(list_concat(NEW_LIST((yyvsp[(1) - (9)].node)), (yyvsp[(3) - (9)].node)), NEW_HASH((yyvsp[(5) - (9)].node))), (yyvsp[(8) - (9)].node));
			(yyval.node) = arg_blk_pass((yyval.node), (yyvsp[(9) - (9)].node));
		    ;}
    break;

  case 252:
#line 1385 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_blk_pass(NEW_SPLAT((yyvsp[(2) - (3)].node)), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 254:
#line 1391 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.num) = cmdarg_stack;
			CMDARG_PUSH(1);
		    ;}
    break;

  case 255:
#line 1396 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			/* CMDARG_POP() */
		        cmdarg_stack = (yyvsp[(1) - (2)].num);
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 257:
#line 1404 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {lex_state = EXPR_ENDARG;;}
    break;

  case 258:
#line 1405 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        rb_warn("don't put space before argument parentheses");
			(yyval.node) = 0;
		    ;}
    break;

  case 259:
#line 1409 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {lex_state = EXPR_ENDARG;;}
    break;

  case 260:
#line 1410 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        rb_warn("don't put space before argument parentheses");
			(yyval.node) = (yyvsp[(2) - (4)].node);
		    ;}
    break;

  case 261:
#line 1417 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_BLOCK_PASS((yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 262:
#line 1423 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 264:
#line 1430 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 265:
#line 1434 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 266:
#line 1440 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 267:
#line 1444 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = arg_concat((yyvsp[(1) - (4)].node), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 268:
#line 1448 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_SPLAT((yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 277:
#line 1462 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_FCALL((yyvsp[(1) - (1)].id), 0);
		    ;}
    break;

  case 278:
#line 1466 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyvsp[(1) - (1)].num) = ruby_sourceline;
		    ;}
    break;

  case 279:
#line 1471 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if ((yyvsp[(3) - (4)].node) == NULL)
			    (yyval.node) = NEW_NIL();
			else
			    (yyval.node) = NEW_BEGIN((yyvsp[(3) - (4)].node));
			nd_set_line((yyval.node), (yyvsp[(1) - (4)].num));
		    ;}
    break;

  case 280:
#line 1478 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {lex_state = EXPR_ENDARG;;}
    break;

  case 281:
#line 1479 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        rb_warning("(...) interpreted as grouped expression");
			(yyval.node) = (yyvsp[(2) - (5)].node);
		    ;}
    break;

  case 282:
#line 1484 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (!(yyvsp[(2) - (3)].node)) (yyval.node) = NEW_NIL();
			else (yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 283:
#line 1489 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_COLON2((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id));
		    ;}
    break;

  case 284:
#line 1493 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_COLON3((yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 285:
#line 1497 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if ((yyvsp[(1) - (4)].node) && nd_type((yyvsp[(1) - (4)].node)) == NODE_SELF)
			    (yyval.node) = NEW_FCALL(tAREF, (yyvsp[(3) - (4)].node));
			else
			    (yyval.node) = NEW_CALL((yyvsp[(1) - (4)].node), tAREF, (yyvsp[(3) - (4)].node));
			fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    ;}
    break;

  case 286:
#line 1505 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        if ((yyvsp[(2) - (3)].node) == 0) {
			    (yyval.node) = NEW_ZARRAY(); /* zero length array*/
			}
			else {
			    (yyval.node) = (yyvsp[(2) - (3)].node);
			}
		    ;}
    break;

  case 287:
#line 1514 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_HASH((yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 288:
#line 1518 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_RETURN(0);
		    ;}
    break;

  case 289:
#line 1522 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_yield((yyvsp[(3) - (4)].node));
		    ;}
    break;

  case 290:
#line 1526 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_YIELD(0, Qfalse);
		    ;}
    break;

  case 291:
#line 1530 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_YIELD(0, Qfalse);
		    ;}
    break;

  case 292:
#line 1533 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {in_defined = 1;;}
    break;

  case 293:
#line 1534 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        in_defined = 0;
			(yyval.node) = NEW_DEFINED((yyvsp[(5) - (6)].node));
		    ;}
    break;

  case 294:
#line 1539 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyvsp[(2) - (2)].node)->nd_iter = NEW_FCALL((yyvsp[(1) - (2)].id), 0);
			(yyval.node) = (yyvsp[(2) - (2)].node);
			fixpos((yyvsp[(2) - (2)].node)->nd_iter, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 296:
#line 1546 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if ((yyvsp[(1) - (2)].node) && nd_type((yyvsp[(1) - (2)].node)) == NODE_BLOCK_PASS) {
			    rb_compile_error("both block arg and actual block given");
			}
			(yyvsp[(2) - (2)].node)->nd_iter = (yyvsp[(1) - (2)].node);
			(yyval.node) = (yyvsp[(2) - (2)].node);
		        fixpos((yyval.node), (yyvsp[(1) - (2)].node));
		    ;}
    break;

  case 297:
#line 1558 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_IF(cond((yyvsp[(2) - (6)].node)), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (6)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
		            NODE *tmp = (yyval.node)->nd_body;
		            (yyval.node)->nd_body = (yyval.node)->nd_else;
		            (yyval.node)->nd_else = tmp;
			}
		    ;}
    break;

  case 298:
#line 1571 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_UNLESS(cond((yyvsp[(2) - (6)].node)), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (6)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
		            NODE *tmp = (yyval.node)->nd_body;
		            (yyval.node)->nd_body = (yyval.node)->nd_else;
		            (yyval.node)->nd_else = tmp;
			}
		    ;}
    break;

  case 299:
#line 1580 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {COND_PUSH(1);;}
    break;

  case 300:
#line 1580 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {COND_POP();;}
    break;

  case 301:
#line 1583 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_WHILE(cond((yyvsp[(3) - (7)].node)), (yyvsp[(6) - (7)].node), 1);
		        fixpos((yyval.node), (yyvsp[(3) - (7)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
			    nd_set_type((yyval.node), NODE_UNTIL);
			}
		    ;}
    break;

  case 302:
#line 1590 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {COND_PUSH(1);;}
    break;

  case 303:
#line 1590 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {COND_POP();;}
    break;

  case 304:
#line 1593 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_UNTIL(cond((yyvsp[(3) - (7)].node)), (yyvsp[(6) - (7)].node), 1);
		        fixpos((yyval.node), (yyvsp[(3) - (7)].node));
			if (cond_negative(&(yyval.node)->nd_cond)) {
			    nd_set_type((yyval.node), NODE_WHILE);
			}
		    ;}
    break;

  case 305:
#line 1603 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_CASE((yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (5)].node));
		    ;}
    break;

  case 306:
#line 1608 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(3) - (4)].node);
		    ;}
    break;

  case 307:
#line 1612 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(4) - (5)].node);
		    ;}
    break;

  case 308:
#line 1615 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {COND_PUSH(1);;}
    break;

  case 309:
#line 1615 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {COND_POP();;}
    break;

  case 310:
#line 1618 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_FOR((yyvsp[(2) - (9)].node), (yyvsp[(5) - (9)].node), (yyvsp[(8) - (9)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (9)].node));
		    ;}
    break;

  case 311:
#line 1623 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (in_def || in_single)
			    yyerror("class definition in method body");
			class_nest++;
			local_push(0);
		        (yyval.num) = ruby_sourceline;
		    ;}
    break;

  case 312:
#line 1632 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.node) = NEW_CLASS((yyvsp[(2) - (6)].node), (yyvsp[(5) - (6)].node), (yyvsp[(3) - (6)].node));
		        nd_set_line((yyval.node), (yyvsp[(4) - (6)].num));
		        local_pop();
			class_nest--;
		    ;}
    break;

  case 313:
#line 1639 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.num) = in_def;
		        in_def = 0;
		    ;}
    break;

  case 314:
#line 1644 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.num) = in_single;
		        in_single = 0;
			class_nest++;
			local_push(0);
		    ;}
    break;

  case 315:
#line 1652 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.node) = NEW_SCLASS((yyvsp[(3) - (8)].node), (yyvsp[(7) - (8)].node));
		        fixpos((yyval.node), (yyvsp[(3) - (8)].node));
		        local_pop();
			class_nest--;
		        in_def = (yyvsp[(4) - (8)].num);
		        in_single = (yyvsp[(6) - (8)].num);
		    ;}
    break;

  case 316:
#line 1661 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (in_def || in_single)
			    yyerror("module definition in method body");
			class_nest++;
			local_push(0);
		        (yyval.num) = ruby_sourceline;
		    ;}
    break;

  case 317:
#line 1670 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.node) = NEW_MODULE((yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node));
		        nd_set_line((yyval.node), (yyvsp[(3) - (5)].num));
		        local_pop();
			class_nest--;
		    ;}
    break;

  case 318:
#line 1677 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.id) = cur_mid;
			cur_mid = (yyvsp[(2) - (2)].id);
			in_def++;
			local_push(0);
		    ;}
    break;

  case 319:
#line 1686 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (!(yyvsp[(5) - (6)].node)) (yyvsp[(5) - (6)].node) = NEW_NIL();
			(yyval.node) = NEW_DEFN((yyvsp[(2) - (6)].id), (yyvsp[(4) - (6)].node), (yyvsp[(5) - (6)].node), NOEX_PRIVATE);
		        fixpos((yyval.node), (yyvsp[(4) - (6)].node));
		        local_pop();
			in_def--;
			cur_mid = (yyvsp[(3) - (6)].id);
		    ;}
    break;

  case 320:
#line 1694 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {lex_state = EXPR_FNAME;;}
    break;

  case 321:
#line 1695 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			in_single++;
			local_push(0);
		        lex_state = EXPR_END; /* force for args */
		    ;}
    break;

  case 322:
#line 1703 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_DEFS((yyvsp[(2) - (9)].node), (yyvsp[(5) - (9)].id), (yyvsp[(7) - (9)].node), (yyvsp[(8) - (9)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (9)].node));
		        local_pop();
			in_single--;
		    ;}
    break;

  case 323:
#line 1710 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_BREAK(0);
		    ;}
    break;

  case 324:
#line 1714 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_NEXT(0);
		    ;}
    break;

  case 325:
#line 1718 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_REDO();
		    ;}
    break;

  case 326:
#line 1722 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_RETRY();
		    ;}
    break;

  case 327:
#line 1728 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			value_expr((yyvsp[(1) - (1)].node));
			(yyval.node) = (yyvsp[(1) - (1)].node);
		    ;}
    break;

  case 336:
#line 1749 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_IF(cond((yyvsp[(2) - (5)].node)), (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (5)].node));
		    ;}
    break;

  case 338:
#line 1757 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 341:
#line 1767 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 342:
#line 1771 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 343:
#line 1777 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if ((yyvsp[(1) - (1)].node)->nd_alen == 1) {
			    (yyval.node) = (yyvsp[(1) - (1)].node)->nd_head;
			    rb_gc_force_recycle((VALUE)(yyvsp[(1) - (1)].node));
			}
			else {
			    (yyval.node) = NEW_MASGN((yyvsp[(1) - (1)].node), 0);
			}
		    ;}
    break;

  case 344:
#line 1787 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN((yyvsp[(1) - (2)].node), 0);
		    ;}
    break;

  case 345:
#line 1791 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_BLOCK_VAR((yyvsp[(4) - (4)].node), NEW_MASGN((yyvsp[(1) - (4)].node), 0));
		    ;}
    break;

  case 346:
#line 1795 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_BLOCK_VAR((yyvsp[(7) - (7)].node), NEW_MASGN((yyvsp[(1) - (7)].node), (yyvsp[(4) - (7)].node)));
		    ;}
    break;

  case 347:
#line 1799 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_BLOCK_VAR((yyvsp[(6) - (6)].node), NEW_MASGN((yyvsp[(1) - (6)].node), -1));
		    ;}
    break;

  case 348:
#line 1803 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN((yyvsp[(1) - (4)].node), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 349:
#line 1807 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN((yyvsp[(1) - (3)].node), -1);
		    ;}
    break;

  case 350:
#line 1811 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_BLOCK_VAR((yyvsp[(5) - (5)].node), NEW_MASGN(0, (yyvsp[(2) - (5)].node)));
		    ;}
    break;

  case 351:
#line 1815 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_BLOCK_VAR((yyvsp[(4) - (4)].node), NEW_MASGN(0, -1));
		    ;}
    break;

  case 352:
#line 1819 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN(0, (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 353:
#line 1823 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_MASGN(0, -1);
		    ;}
    break;

  case 354:
#line 1827 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_BLOCK_VAR((yyvsp[(2) - (2)].node), (NODE*)1);
		    ;}
    break;

  case 356:
#line 1834 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (NODE*)1;
			command_start = Qtrue;
		    ;}
    break;

  case 357:
#line 1839 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (NODE*)1;
			command_start = Qtrue;
		    ;}
    break;

  case 358:
#line 1844 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
			command_start = Qtrue;
		    ;}
    break;

  case 359:
#line 1851 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.vars) = dyna_push();
			(yyvsp[(1) - (1)].num) = ruby_sourceline;
		    ;}
    break;

  case 360:
#line 1855 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.vars) = ruby_dyna_vars;;}
    break;

  case 361:
#line 1858 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_ITER((yyvsp[(3) - (6)].node), 0, dyna_init((yyvsp[(5) - (6)].node), (yyvsp[(4) - (6)].vars)));
			nd_set_line((yyval.node), (yyvsp[(1) - (6)].num));
			dyna_pop((yyvsp[(2) - (6)].vars));
		    ;}
    break;

  case 362:
#line 1866 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if ((yyvsp[(1) - (2)].node) && nd_type((yyvsp[(1) - (2)].node)) == NODE_BLOCK_PASS) {
			    rb_compile_error("both block arg and actual block given");
			}
			(yyvsp[(2) - (2)].node)->nd_iter = (yyvsp[(1) - (2)].node);
			(yyval.node) = (yyvsp[(2) - (2)].node);
		        fixpos((yyval.node), (yyvsp[(1) - (2)].node));
		    ;}
    break;

  case 363:
#line 1875 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 364:
#line 1879 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 365:
#line 1885 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_fcall((yyvsp[(1) - (2)].id), (yyvsp[(2) - (2)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 366:
#line 1890 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    ;}
    break;

  case 367:
#line 1895 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].id), (yyvsp[(4) - (4)].node));
		        fixpos((yyval.node), (yyvsp[(1) - (4)].node));
		    ;}
    break;

  case 368:
#line 1900 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_call((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].id), 0);
		    ;}
    break;

  case 369:
#line 1904 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = new_super((yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 370:
#line 1908 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_ZSUPER();
		    ;}
    break;

  case 371:
#line 1914 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.vars) = dyna_push();
			(yyvsp[(1) - (1)].num) = ruby_sourceline;
		    ;}
    break;

  case 372:
#line 1918 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.vars) = ruby_dyna_vars;;}
    break;

  case 373:
#line 1920 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_ITER((yyvsp[(3) - (6)].node), 0, dyna_init((yyvsp[(5) - (6)].node), (yyvsp[(4) - (6)].vars)));
			nd_set_line((yyval.node), (yyvsp[(1) - (6)].num));
			dyna_pop((yyvsp[(2) - (6)].vars));
		    ;}
    break;

  case 374:
#line 1926 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        (yyval.vars) = dyna_push();
			(yyvsp[(1) - (1)].num) = ruby_sourceline;
		    ;}
    break;

  case 375:
#line 1930 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.vars) = ruby_dyna_vars;;}
    break;

  case 376:
#line 1932 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_ITER((yyvsp[(3) - (6)].node), 0, dyna_init((yyvsp[(5) - (6)].node), (yyvsp[(4) - (6)].vars)));
			nd_set_line((yyval.node), (yyvsp[(1) - (6)].num));
			dyna_pop((yyvsp[(2) - (6)].vars));
		    ;}
    break;

  case 377:
#line 1942 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_WHEN((yyvsp[(2) - (5)].node), (yyvsp[(4) - (5)].node), (yyvsp[(5) - (5)].node));
		    ;}
    break;

  case 379:
#line 1948 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (4)].node), NEW_WHEN((yyvsp[(4) - (4)].node), 0, 0));
		    ;}
    break;

  case 380:
#line 1952 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST(NEW_WHEN((yyvsp[(2) - (2)].node), 0, 0));
		    ;}
    break;

  case 383:
#line 1964 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        if ((yyvsp[(3) - (6)].node)) {
		            (yyvsp[(3) - (6)].node) = node_assign((yyvsp[(3) - (6)].node), NEW_GVAR(rb_intern("$!")));
			    (yyvsp[(5) - (6)].node) = block_append((yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].node));
			}
			(yyval.node) = NEW_RESBODY((yyvsp[(2) - (6)].node), (yyvsp[(5) - (6)].node), (yyvsp[(6) - (6)].node));
		        fixpos((yyval.node), (yyvsp[(2) - (6)].node)?(yyvsp[(2) - (6)].node):(yyvsp[(5) - (6)].node));
		    ;}
    break;

  case 385:
#line 1976 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIST((yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 388:
#line 1984 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 390:
#line 1991 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if ((yyvsp[(2) - (2)].node))
			    (yyval.node) = (yyvsp[(2) - (2)].node);
			else
			    /* place holder */
			    (yyval.node) = NEW_NIL();
		    ;}
    break;

  case 393:
#line 2003 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_LIT(ID2SYM((yyvsp[(1) - (1)].id)));
		    ;}
    break;

  case 395:
#line 2010 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			NODE *node = (yyvsp[(1) - (1)].node);
			if (!node) {
			    node = NEW_STR(rb_str_new(0, 0));
			}
			else {
			    node = evstr2dstr(node);
			}
			(yyval.node) = node;
		    ;}
    break;

  case 397:
#line 2024 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = literal_concat((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 398:
#line 2030 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 399:
#line 2036 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 400:
#line 2059 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 401:
#line 2092 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_ZARRAY();
		    ;}
    break;

  case 402:
#line 2096 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 403:
#line 2102 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = 0;
		    ;}
    break;

  case 404:
#line 2106 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), evstr2dstr((yyvsp[(2) - (3)].node)));
		    ;}
    break;

  case 406:
#line 2113 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = literal_concat((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 407:
#line 2119 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_ZARRAY();
		    ;}
    break;

  case 408:
#line 2123 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (3)].node);
		    ;}
    break;

  case 409:
#line 2129 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = 0;
		    ;}
    break;

  case 410:
#line 2133 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append((yyvsp[(1) - (3)].node), (yyvsp[(2) - (3)].node));
		    ;}
    break;

  case 411:
#line 2139 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = 0;
		    ;}
    break;

  case 412:
#line 2143 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = literal_concat((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 413:
#line 2149 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = 0;
		    ;}
    break;

  case 414:
#line 2153 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = literal_concat((yyvsp[(1) - (2)].node), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 416:
#line 2160 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = lex_strterm;
			lex_strterm = 0;
			lex_state = EXPR_BEG;
		    ;}
    break;

  case 417:
#line 2166 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			lex_strterm = (yyvsp[(2) - (3)].node);
		        (yyval.node) = NEW_EVSTR((yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 418:
#line 2171 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = lex_strterm;
			lex_strterm = 0;
			lex_state = EXPR_BEG;
			COND_PUSH(0);
			CMDARG_PUSH(0);
		    ;}
    break;

  case 419:
#line 2179 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			lex_strterm = (yyvsp[(2) - (4)].node);
			COND_LEXPOP();
			CMDARG_LEXPOP();
			if (((yyval.node) = (yyvsp[(3) - (4)].node)) && nd_type((yyval.node)) == NODE_NEWLINE) {
			    (yyval.node) = (yyval.node)->nd_next;
			    rb_gc_force_recycle((VALUE)(yyvsp[(3) - (4)].node));
			}
			(yyval.node) = new_evstr((yyval.node));
		    ;}
    break;

  case 420:
#line 2191 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.node) = NEW_GVAR((yyvsp[(1) - (1)].id));;}
    break;

  case 421:
#line 2192 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.node) = NEW_IVAR((yyvsp[(1) - (1)].id));;}
    break;

  case 422:
#line 2193 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.node) = NEW_CVAR((yyvsp[(1) - (1)].id));;}
    break;

  case 424:
#line 2198 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
		        lex_state = EXPR_END;
			(yyval.id) = (yyvsp[(2) - (2)].id);
		    ;}
    break;

  case 429:
#line 2211 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 432:
#line 2247 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = negate_lit((yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 433:
#line 2251 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = negate_lit((yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 439:
#line 2261 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.id) = kNIL;;}
    break;

  case 440:
#line 2262 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.id) = kSELF;;}
    break;

  case 441:
#line 2263 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.id) = kTRUE;;}
    break;

  case 442:
#line 2264 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.id) = kFALSE;;}
    break;

  case 443:
#line 2265 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.id) = k__FILE__;;}
    break;

  case 444:
#line 2266 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.id) = k__LINE__;;}
    break;

  case 445:
#line 2270 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = gettable((yyvsp[(1) - (1)].id));
		    ;}
    break;

  case 446:
#line 2276 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = assignable((yyvsp[(1) - (1)].id), 0);
		    ;}
    break;

  case 449:
#line 2286 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = 0;
		    ;}
    break;

  case 450:
#line 2290 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			lex_state = EXPR_BEG;
		    ;}
    break;

  case 451:
#line 2294 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(3) - (4)].node);
		    ;}
    break;

  case 452:
#line 2297 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {yyerrok; (yyval.node) = 0;;}
    break;

  case 453:
#line 2301 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (4)].node);
			lex_state = EXPR_BEG;
		        command_start = Qtrue;
		    ;}
    break;

  case 454:
#line 2307 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 455:
#line 2313 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS((yyvsp[(1) - (6)].num), (yyvsp[(3) - (6)].node), (yyvsp[(5) - (6)].node)), (yyvsp[(6) - (6)].node));
		    ;}
    break;

  case 456:
#line 2317 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS((yyvsp[(1) - (4)].num), (yyvsp[(3) - (4)].node), 0), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 457:
#line 2321 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS((yyvsp[(1) - (4)].num), 0, (yyvsp[(3) - (4)].node)), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 458:
#line 2325 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS((yyvsp[(1) - (2)].num), 0, 0), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 459:
#line 2329 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS(0, (yyvsp[(1) - (4)].node), (yyvsp[(3) - (4)].node)), (yyvsp[(4) - (4)].node));
		    ;}
    break;

  case 460:
#line 2333 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS(0, (yyvsp[(1) - (2)].node), 0), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 461:
#line 2337 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS(0, 0, (yyvsp[(1) - (2)].node)), (yyvsp[(2) - (2)].node));
		    ;}
    break;

  case 462:
#line 2341 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append(NEW_ARGS(0, 0, 0), (yyvsp[(1) - (1)].node));
		    ;}
    break;

  case 463:
#line 2345 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_ARGS(0, 0, 0);
		    ;}
    break;

  case 464:
#line 2351 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			yyerror("formal argument cannot be a constant");
		    ;}
    break;

  case 465:
#line 2355 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
                        yyerror("formal argument cannot be an instance variable");
		    ;}
    break;

  case 466:
#line 2359 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
                        yyerror("formal argument cannot be a global variable");
		    ;}
    break;

  case 467:
#line 2363 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
                        yyerror("formal argument cannot be a class variable");
		    ;}
    break;

  case 468:
#line 2367 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (!is_local_id((yyvsp[(1) - (1)].id)))
			    yyerror("formal argument must be local variable");
			else if (local_id((yyvsp[(1) - (1)].id)))
			    yyerror("duplicate argument name");
			local_cnt((yyvsp[(1) - (1)].id));
			(yyval.num) = 1;
		    ;}
    break;

  case 470:
#line 2379 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.num) += 1;
		    ;}
    break;

  case 471:
#line 2385 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (!is_local_id((yyvsp[(1) - (3)].id)))
			    yyerror("formal argument must be local variable");
			else if (local_id((yyvsp[(1) - (3)].id)))
			    yyerror("duplicate optional argument name");
			(yyval.node) = assignable((yyvsp[(1) - (3)].id), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 472:
#line 2395 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = NEW_BLOCK((yyvsp[(1) - (1)].node));
			(yyval.node)->nd_end = (yyval.node);
		    ;}
    break;

  case 473:
#line 2400 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = block_append((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 476:
#line 2410 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (!is_local_id((yyvsp[(2) - (2)].id)))
			    yyerror("rest argument must be local variable");
			else if (local_id((yyvsp[(2) - (2)].id)))
			    yyerror("duplicate rest argument name");
			if (dyna_in_block()) {
			    rb_dvar_push((yyvsp[(2) - (2)].id), Qnil);
			}
			(yyval.node) = assignable((yyvsp[(2) - (2)].id), 0);
		    ;}
    break;

  case 477:
#line 2421 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (dyna_in_block()) {
			    (yyval.node) = NEW_DASGN_CURR(internal_id(), 0);
			}
			else {
			    (yyval.node) = NEW_NODE(NODE_LASGN,0,0,local_append(0));
			}
		    ;}
    break;

  case 480:
#line 2436 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if (!is_local_id((yyvsp[(2) - (2)].id)))
			    yyerror("block argument must be local variable");
			else if (local_id((yyvsp[(2) - (2)].id)))
			    yyerror("duplicate block argument name");
			(yyval.node) = NEW_BLOCK_ARG((yyvsp[(2) - (2)].id));
		    ;}
    break;

  case 481:
#line 2446 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(2) - (2)].node);
		    ;}
    break;

  case 483:
#line 2453 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(1) - (1)].node);
			value_expr((yyval.node));
		    ;}
    break;

  case 484:
#line 2457 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {lex_state = EXPR_BEG;;}
    break;

  case 485:
#line 2458 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
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
		    ;}
    break;

  case 487:
#line 2484 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 488:
#line 2488 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			if ((yyvsp[(1) - (2)].node)->nd_alen%2 != 0) {
			    yyerror("odd number list for Hash");
			}
			(yyval.node) = (yyvsp[(1) - (2)].node);
		    ;}
    break;

  case 490:
#line 2498 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_concat((yyvsp[(1) - (3)].node), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 491:
#line 2504 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {
			(yyval.node) = list_append(NEW_LIST((yyvsp[(1) - (3)].node)), (yyvsp[(3) - (3)].node));
		    ;}
    break;

  case 511:
#line 2542 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {yyerrok;;}
    break;

  case 514:
#line 2547 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {yyerrok;;}
    break;

  case 515:
#line 2550 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"
    {(yyval.node) = 0;;}
    break;


/* Line 1267 of yacc.c.  */
#line 7478 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.c"
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


#line 2552 "/var/folders/wb/wrpc1dw48xx2shsvts3nwzq80004zq/T/x6gUxurDHN/70064/ruby/ruby/parse.y"

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
    deferred_nodes = 0;
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
    deferred_nodes = 0;

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
tokadd_escape()
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
	    return tokadd_escape();
	}
	else if (c == -1) goto eof;
	tokadd(c);
	return 0;

      eof:
      case -1:
        yyerror("Invalid escape character syntax");
	return -1;

      default:
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
		    if (tokadd_escape() < 0)
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
#define IS_BEG() (lex_state == EXPR_BEG || lex_state == EXPR_MID || lex_state == EXPR_CLASS)

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
	    else if (IS_BEG()) {
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
	else if (IS_BEG()) {
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
	if (IS_BEG() ||
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
	if (IS_BEG() ||
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
			if (c < '0' || c > '9') break;
			if (c > '7') goto invalid_octal;
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
		  invalid_octal:
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
	    if (IS_BEG() || (IS_ARG() && space_seen)) {
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
	if (IS_BEG()) {
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
	if (IS_BEG()) {
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
	else if (IS_BEG()) {
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
	if (c != tLBRACE) command_start = Qtrue;
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
	if (IS_BEG()) {
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
		const struct kwtable *kw;

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
			command_start = Qtrue;
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
		lex_state == EXPR_CLASS ||
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
    const char *useless = 0;

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

static void
fixup_nodes(rootnode)
    NODE **rootnode;
{
    NODE *node, *next, *head;

    for (node = *rootnode; node; node = next) {
	enum node_type type;
	VALUE val;

	next = node->nd_next;
	head = node->nd_head;
	rb_gc_force_recycle((VALUE)node);
	*rootnode = next;
	switch (type = nd_type(head)) {
	  case NODE_DOT2:
	  case NODE_DOT3:
	    val = rb_range_new(head->nd_beg->nd_lit, head->nd_end->nd_lit,
			       type == NODE_DOT3 ? Qtrue : Qfalse);
	    rb_gc_force_recycle((VALUE)head->nd_beg);
	    rb_gc_force_recycle((VALUE)head->nd_end);
	    nd_set_type(head, NODE_LIT);
	    head->nd_lit = val;
	    break;
	  default:
	    break;
	}
    }
}

static NODE *cond0();

static NODE*
range_op(node)
    NODE *node;
{
    enum node_type type;

    if (node == 0) return 0;

    type = nd_type(node);
    if (type == NODE_NEWLINE) {
	node = node->nd_next;
	type = nd_type(node);
    }
    value_expr(node);
    if (type == NODE_LIT && FIXNUM_P(node->nd_lit)) {
	warn_unless_e_option(node, "integer literal in conditional range");
	return call_op(node,tEQ,1,NEW_GVAR(rb_intern("$.")));
    }
    return cond0(node);
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
    (void)rb_parser_realloc;
    (void)rb_parser_calloc;
    (void)nodetype;
    (void)nodeline;
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
    rb_gc_mark((VALUE)deferred_nodes);
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
    const char *name;
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
    if (last_id >= SYM2ID(~(VALUE)0) >> ID_SCOPE_SHIFT) {
	if (last > 20) {
	    rb_raise(rb_eRuntimeError, "symbol table overflow (symbol %.20s...)",
		     name);
	}
	else {
	    rb_raise(rb_eRuntimeError, "symbol table overflow (symbol %.*s)",
		     last, name);
	}
    }
    id |= ++last_id << ID_SCOPE_SHIFT;
  id_regist:
    name = strdup(name);
    st_add_direct(sym_tbl, (st_data_t)name, id);
    st_add_direct(sym_rev_tbl, id, (st_data_t)name);
    return id;
}

const char *
rb_id2name(id)
    ID id;
{
    const char *name;
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

