/* A Bison parser, made from parse.y, by GNU bison 1.75.  */

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
#define YYBISON	1

/* Pure parsers.  */
#define YYPURE	0

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
     tSTRING = 313,
     tXSTRING = 314,
     tREGEXP = 315,
     tDSTRING = 316,
     tDXSTRING = 317,
     tDREGEXP = 318,
     tNTH_REF = 319,
     tBACK_REF = 320,
     tQWORDS = 321,
     tUPLUS = 322,
     tUMINUS = 323,
     tPOW = 324,
     tCMP = 325,
     tEQ = 326,
     tEQQ = 327,
     tNEQ = 328,
     tGEQ = 329,
     tLEQ = 330,
     tANDOP = 331,
     tOROP = 332,
     tMATCH = 333,
     tNMATCH = 334,
     tDOT2 = 335,
     tDOT3 = 336,
     tAREF = 337,
     tASET = 338,
     tLSHFT = 339,
     tRSHFT = 340,
     tCOLON2 = 341,
     tCOLON3 = 342,
     tOP_ASGN = 343,
     tASSOC = 344,
     tLPAREN = 345,
     tLBRACK = 346,
     tLBRACE = 347,
     tSTAR = 348,
     tAMPER = 349,
     tSYMBEG = 350,
     LAST_TOKEN = 351
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
#define tSTRING 313
#define tXSTRING 314
#define tREGEXP 315
#define tDSTRING 316
#define tDXSTRING 317
#define tDREGEXP 318
#define tNTH_REF 319
#define tBACK_REF 320
#define tQWORDS 321
#define tUPLUS 322
#define tUMINUS 323
#define tPOW 324
#define tCMP 325
#define tEQ 326
#define tEQQ 327
#define tNEQ 328
#define tGEQ 329
#define tLEQ 330
#define tANDOP 331
#define tOROP 332
#define tMATCH 333
#define tNMATCH 334
#define tDOT2 335
#define tDOT3 336
#define tAREF 337
#define tASET 338
#define tLSHFT 339
#define tRSHFT 340
#define tCOLON2 341
#define tCOLON3 342
#define tOP_ASGN 343
#define tASSOC 344
#define tLPAREN 345
#define tLBRACK 346
#define tLBRACE 347
#define tSTAR 348
#define tAMPER 349
#define tSYMBEG 350
#define LAST_TOKEN 351




/* Copy the first part of user declarations.  */
#line 13 "parse.y"


#define YYDEBUG 1
#include "ruby.h"
#include "env.h"
#include "node.h"
#include "st.h"
#include <stdio.h>
#include <errno.h>

#define ID_SCOPE_SHIFT 3
#define ID_SCOPE_MASK 0x07
#define ID_LOCAL    0x01
#define ID_INSTANCE 0x02
#define ID_GLOBAL   0x03
#define ID_ATTRSET  0x04
#define ID_CONST    0x05
#define ID_CLASS    0x06

#define is_notop_id(id) ((id)>LAST_TOKEN)
#define is_local_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_LOCAL)
#define is_global_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_GLOBAL)
#define is_instance_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_INSTANCE)
#define is_attrset_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_ATTRSET)
#define is_const_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_CONST)
#define is_class_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_CLASS)

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
    EXPR_MID,			/* newline significant, +/- is a operator. */
    EXPR_FNAME,			/* ignore newline, no reserved words. */
    EXPR_DOT,			/* right after `.' or `::', no reserved words. */
    EXPR_CLASS,			/* immediate after `class', no here document. */
} lex_state;

#if SIZEOF_LONG_LONG > 0
typedef unsigned long long stack_type;
#elif SIZEOF___INT64 > 0
typedef unsigned __int64 stack_type;
#else
typedef unsigned long stack_type;
#endif

static int cond_nest = 0;
static stack_type cond_stack = 0;
#define COND_PUSH do {\
    cond_nest++;\
    cond_stack = (cond_stack<<1)|1;\
} while(0)
#define COND_POP do {\
    cond_nest--;\
    cond_stack >>= 1;\
} while (0)
#define COND_P() (cond_nest > 0 && (cond_stack&1))

static stack_type cmdarg_stack = 0;
#define CMDARG_PUSH do {\
    cmdarg_stack = (cmdarg_stack<<1)|1;\
} while(0)
#define CMDARG_POP do {\
    cmdarg_stack >>= 1;\
} while (0)
#define CMDARG_P() (cmdarg_stack && (cmdarg_stack&1))

static int class_nest = 0;
static int in_single = 0;
static int in_def = 0;
static int compile_for_eval = 0;
static ID cur_mid = 0;

static NODE *cond();
static NODE *logop();

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
static NODE *call_op();
static int in_defined = 0;

static NODE *arg_blk_pass();
static NODE *new_call();
static NODE *new_fcall();
static NODE *new_super();

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

static struct RVarmap *dyna_push();
static void dyna_pop();
static int dyna_in_block();

static void top_local_init();
static void top_local_setup();


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

#ifndef YYSTYPE
#line 142 "parse.y"
typedef union {
    NODE *node;
    VALUE val;
    ID id;
    int num;
    struct RVarmap *vars;
} yystype;
/* Line 193 of /usr/share/bison/yacc.c.  */
#line 402 "y.tab.c"
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif

#ifndef YYLTYPE
typedef struct yyltype
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} yyltype;
# define YYLTYPE yyltype
# define YYLTYPE_IS_TRIVIAL 1
#endif

/* Copy the second part of user declarations.  */


/* Line 213 of /usr/share/bison/yacc.c.  */
#line 423 "y.tab.c"

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
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)

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
	    (To)[yyi] = (From)[yyi];	\
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
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
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
#define YYFINAL  3
#define YYLAST   7722

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  123
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  110
/* YYNRULES -- Number of rules. */
#define YYNRULES  417
/* YYNRULES -- Number of states. */
#define YYNSTATES  737

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   351

#define YYTRANSLATE(X) \
  ((unsigned)(X) <= YYMAXUTOK ? yytranslate[X] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     121,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   109,     2,     2,     2,   108,   103,     2,
     120,   115,   106,   104,   116,   105,   114,   107,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    98,   122,
     100,    96,    99,    97,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   117,     2,   118,   102,     2,   119,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   112,   101,   113,   110,     2,     2,     2,
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
      95,   111
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned short yyprhs[] =
{
       0,     0,     3,     4,     7,    10,    12,    14,    18,    21,
      22,    27,    31,    35,    39,    42,    46,    50,    54,    58,
      62,    63,    69,    74,    78,    82,    86,    88,    92,    95,
      97,   101,   105,   108,   111,   113,   115,   117,   119,   124,
     129,   132,   137,   142,   145,   148,   150,   154,   156,   160,
     162,   165,   169,   172,   175,   177,   179,   183,   186,   190,
     192,   197,   201,   205,   209,   211,   213,   218,   222,   226,
     230,   232,   234,   236,   238,   240,   242,   244,   246,   248,
     250,   252,   253,   258,   260,   262,   264,   266,   268,   270,
     272,   274,   276,   278,   280,   282,   284,   286,   288,   290,
     292,   294,   296,   298,   300,   302,   304,   306,   308,   310,
     312,   314,   316,   318,   320,   322,   324,   326,   328,   330,
     332,   334,   336,   338,   340,   342,   344,   346,   348,   350,
     352,   354,   356,   358,   360,   362,   364,   366,   368,   370,
     372,   374,   376,   378,   380,   382,   384,   386,   388,   390,
     392,   396,   397,   402,   409,   415,   421,   427,   431,   435,
     439,   443,   447,   451,   455,   459,   463,   466,   469,   473,
     477,   481,   485,   489,   493,   497,   501,   505,   509,   513,
     517,   521,   524,   527,   531,   535,   539,   543,   544,   549,
     555,   557,   559,   562,   567,   570,   576,   579,   583,   587,
     592,   597,   604,   606,   608,   610,   614,   617,   623,   626,
     632,   637,   645,   649,   651,   652,   655,   658,   661,   663,
     665,   669,   671,   673,   677,   682,   685,   687,   689,   691,
     693,   695,   697,   699,   701,   703,   705,   712,   716,   720,
     723,   728,   732,   736,   741,   745,   747,   752,   756,   758,
     759,   766,   769,   771,   774,   781,   788,   789,   790,   798,
     799,   800,   808,   814,   819,   820,   821,   831,   832,   839,
     840,   841,   850,   851,   857,   858,   868,   869,   870,   883,
     885,   887,   889,   891,   893,   895,   898,   900,   902,   904,
     910,   912,   915,   917,   919,   921,   924,   926,   930,   931,
     937,   940,   945,   950,   953,   958,   963,   967,   970,   972,
     973,   979,   980,   986,   992,   994,   999,  1002,  1004,  1006,
    1008,  1010,  1013,  1015,  1022,  1024,  1026,  1029,  1031,  1033,
    1035,  1037,  1039,  1042,  1045,  1048,  1050,  1052,  1054,  1056,
    1058,  1060,  1062,  1064,  1066,  1068,  1070,  1072,  1074,  1076,
    1078,  1080,  1082,  1084,  1086,  1088,  1090,  1091,  1096,  1099,
    1104,  1107,  1114,  1119,  1124,  1127,  1132,  1135,  1138,  1140,
    1141,  1143,  1145,  1147,  1149,  1151,  1153,  1157,  1161,  1163,
    1167,  1170,  1172,  1175,  1178,  1180,  1182,  1183,  1189,  1191,
    1194,  1197,  1199,  1203,  1207,  1209,  1211,  1213,  1215,  1217,
    1219,  1221,  1223,  1225,  1227,  1229,  1231,  1232,  1234,  1235,
    1237,  1238,  1240,  1242,  1244,  1246,  1248,  1251
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const short yyrhs[] =
{
     124,     0,    -1,    -1,   125,   126,    -1,   127,   227,    -1,
     232,    -1,   128,    -1,   127,   231,   128,    -1,     1,   128,
      -1,    -1,    44,   144,   129,   144,    -1,    44,    52,    52,
      -1,    44,    52,    65,    -1,    44,    52,    64,    -1,     6,
     145,    -1,   128,    39,   131,    -1,   128,    40,   131,    -1,
     128,    41,   131,    -1,   128,    42,   131,    -1,   128,    43,
     128,    -1,    -1,    46,   130,   112,   126,   113,    -1,    47,
     112,   126,   113,    -1,   141,    96,   132,    -1,   135,    96,
     132,    -1,   141,    96,   162,    -1,   131,    -1,   135,    96,
     161,    -1,    29,   163,    -1,   132,    -1,   131,    36,   131,
      -1,   131,    37,   131,    -1,    38,   131,    -1,   109,   132,
      -1,   149,    -1,   134,    -1,   133,    -1,   187,    -1,   187,
     114,   224,   156,    -1,   187,    86,   224,   156,    -1,   223,
     156,    -1,   164,   114,   224,   156,    -1,   164,    86,   224,
     156,    -1,    31,   156,    -1,    30,   163,    -1,   137,    -1,
      90,   136,   115,    -1,   137,    -1,    90,   136,   115,    -1,
     139,    -1,   139,   138,    -1,   139,    93,   140,    -1,   139,
      93,    -1,    93,   140,    -1,    93,    -1,   140,    -1,    90,
     136,   115,    -1,   138,   116,    -1,   139,   138,   116,    -1,
     204,    -1,   164,   117,   152,   118,    -1,   164,   114,    50,
      -1,   164,    86,    50,    -1,   164,   114,    54,    -1,   206,
      -1,   204,    -1,   164,   117,   152,   118,    -1,   164,   114,
      50,    -1,   164,    86,    50,    -1,   164,   114,    54,    -1,
     206,    -1,    50,    -1,    54,    -1,    50,    -1,    54,    -1,
      51,    -1,   147,    -1,   148,    -1,   143,    -1,   201,    -1,
     144,    -1,    -1,   145,   116,   146,   144,    -1,   101,    -1,
     102,    -1,   103,    -1,    70,    -1,    71,    -1,    72,    -1,
      78,    -1,    99,    -1,    74,    -1,   100,    -1,    75,    -1,
      84,    -1,    85,    -1,   104,    -1,   105,    -1,   106,    -1,
      93,    -1,   107,    -1,   108,    -1,    69,    -1,   110,    -1,
      67,    -1,    68,    -1,    82,    -1,    83,    -1,   119,    -1,
      48,    -1,    49,    -1,    46,    -1,    47,    -1,    44,    -1,
      36,    -1,     7,    -1,    21,    -1,    16,    -1,     3,    -1,
       5,    -1,    45,    -1,    26,    -1,    15,    -1,    14,    -1,
      10,    -1,     9,    -1,    35,    -1,    20,    -1,    39,    -1,
      25,    -1,     4,    -1,    22,    -1,    33,    -1,    38,    -1,
      37,    -1,    23,    -1,     8,    -1,    24,    -1,    29,    -1,
      32,    -1,    31,    -1,    13,    -1,    34,    -1,     6,    -1,
      40,    -1,    42,    -1,    17,    -1,    41,    -1,    30,    -1,
      43,    -1,   141,    96,   149,    -1,    -1,   204,    88,   150,
     149,    -1,   164,   117,   152,   118,    88,   149,    -1,   164,
     114,    50,    88,   149,    -1,   164,   114,    54,    88,   149,
      -1,   164,    86,    50,    88,   149,    -1,   206,    88,   149,
      -1,   149,    80,   149,    -1,   149,    81,   149,    -1,   149,
     104,   149,    -1,   149,   105,   149,    -1,   149,   106,   149,
      -1,   149,   107,   149,    -1,   149,   108,   149,    -1,   149,
      69,   149,    -1,    67,   149,    -1,    68,   149,    -1,   149,
     101,   149,    -1,   149,   102,   149,    -1,   149,   103,   149,
      -1,   149,    70,   149,    -1,   149,    99,   149,    -1,   149,
      74,   149,    -1,   149,   100,   149,    -1,   149,    75,   149,
      -1,   149,    71,   149,    -1,   149,    72,   149,    -1,   149,
      73,   149,    -1,   149,    78,   149,    -1,   149,    79,   149,
      -1,   109,   149,    -1,   110,   149,    -1,   149,    84,   149,
      -1,   149,    85,   149,    -1,   149,    76,   149,    -1,   149,
      77,   149,    -1,    -1,    45,   228,   151,   149,    -1,   149,
      97,   149,    98,   149,    -1,   164,    -1,   232,    -1,   132,
     228,    -1,   160,   116,   132,   228,    -1,   160,   229,    -1,
     160,   116,    93,   149,   228,    -1,   221,   229,    -1,    93,
     149,   228,    -1,   120,   232,   115,    -1,   120,   155,   228,
     115,    -1,   120,   187,   228,   115,    -1,   120,   160,   116,
     187,   228,   115,    -1,   232,    -1,   153,    -1,   134,    -1,
     160,   116,   134,    -1,   160,   159,    -1,   160,   116,    93,
     149,   159,    -1,   221,   159,    -1,   221,   116,    93,   149,
     159,    -1,   160,   116,   221,   159,    -1,   160,   116,   221,
     116,    93,   149,   159,    -1,    93,   149,   159,    -1,   158,
      -1,    -1,   157,   155,    -1,    94,   149,    -1,   116,   158,
      -1,   232,    -1,   149,    -1,   160,   116,   149,    -1,   149,
      -1,   162,    -1,   160,   116,   149,    -1,   160,   116,    93,
     149,    -1,    93,   149,    -1,   155,    -1,   199,    -1,   200,
      -1,    59,    -1,    66,    -1,    62,    -1,    63,    -1,   205,
      -1,   206,    -1,    51,    -1,     7,   126,   197,   182,   198,
      10,    -1,    90,   126,   115,    -1,   164,    86,    54,    -1,
      87,   142,    -1,   164,   117,   152,   118,    -1,    91,   152,
     118,    -1,    92,   220,   113,    -1,    29,   120,   163,   115,
      -1,    29,   120,   115,    -1,    29,    -1,    30,   120,   163,
     115,    -1,    30,   120,   115,    -1,    30,    -1,    -1,    45,
     228,   120,   165,   131,   115,    -1,   223,   189,    -1,   188,
      -1,   188,   189,    -1,    11,   131,   179,   126,   181,    10,
      -1,    12,   131,   179,   126,   182,    10,    -1,    -1,    -1,
      18,   166,   131,   180,   167,   126,    10,    -1,    -1,    -1,
      19,   168,   131,   180,   169,   126,    10,    -1,    16,   131,
     227,   192,    10,    -1,    16,   227,   192,    10,    -1,    -1,
      -1,    20,   183,    25,   170,   131,   180,   171,   126,    10,
      -1,    -1,     3,   142,   207,   172,   126,    10,    -1,    -1,
      -1,     3,    84,   131,   173,   230,   174,   126,    10,    -1,
      -1,     4,   142,   175,   126,    10,    -1,    -1,     5,   143,
     176,   209,   126,   197,   182,   198,    10,    -1,    -1,    -1,
       5,   218,   226,   177,   143,   178,   209,   126,   197,   182,
     198,    10,    -1,    21,    -1,    22,    -1,    23,    -1,    24,
      -1,   230,    -1,    13,    -1,   230,    13,    -1,   230,    -1,
      27,    -1,   182,    -1,    14,   131,   179,   126,   181,    -1,
     232,    -1,    15,   126,    -1,   141,    -1,   135,    -1,   232,
      -1,   101,   101,    -1,    77,    -1,   101,   183,   101,    -1,
      -1,    28,   186,   184,   126,    10,    -1,   134,   185,    -1,
     187,   114,   224,   154,    -1,   187,    86,   224,   154,    -1,
     223,   153,    -1,   164,   114,   224,   154,    -1,   164,    86,
     224,   153,    -1,   164,    86,   225,    -1,    31,   153,    -1,
      31,    -1,    -1,   112,   190,   184,   126,   113,    -1,    -1,
      26,   191,   184,   126,    10,    -1,    17,   193,   179,   126,
     194,    -1,   160,    -1,   160,   116,    93,   149,    -1,    93,
     149,    -1,   182,    -1,   192,    -1,   232,    -1,   160,    -1,
      89,   141,    -1,   232,    -1,     8,   195,   196,   179,   126,
     197,    -1,   232,    -1,   232,    -1,     9,   126,    -1,   203,
      -1,   201,    -1,    60,    -1,    58,    -1,    61,    -1,   200,
      58,    -1,   200,    61,    -1,    95,   202,    -1,   143,    -1,
      53,    -1,    52,    -1,    55,    -1,    56,    -1,    57,    -1,
      50,    -1,    53,    -1,    52,    -1,    54,    -1,    55,    -1,
      33,    -1,    32,    -1,    34,    -1,    35,    -1,    49,    -1,
      48,    -1,   204,    -1,    64,    -1,    65,    -1,   230,    -1,
      -1,   100,   208,   131,   230,    -1,     1,   230,    -1,   120,
     210,   228,   115,    -1,   210,   230,    -1,   212,   116,   214,
     116,   215,   217,    -1,   212,   116,   214,   217,    -1,   212,
     116,   215,   217,    -1,   212,   217,    -1,   214,   116,   215,
     217,    -1,   214,   217,    -1,   215,   217,    -1,   216,    -1,
      -1,    54,    -1,    53,    -1,    52,    -1,    55,    -1,    50,
      -1,   211,    -1,   212,   116,   211,    -1,    50,    96,   149,
      -1,   213,    -1,   214,   116,   213,    -1,    93,    50,    -1,
      93,    -1,    94,    50,    -1,   116,   216,    -1,   232,    -1,
     205,    -1,    -1,   120,   219,   131,   228,   115,    -1,   232,
      -1,   221,   229,    -1,   160,   229,    -1,   222,    -1,   221,
     116,   222,    -1,   149,    89,   149,    -1,    50,    -1,    54,
      -1,    51,    -1,    50,    -1,    54,    -1,    51,    -1,   147,
      -1,    50,    -1,    51,    -1,   147,    -1,   114,    -1,    86,
      -1,    -1,   231,    -1,    -1,   121,    -1,    -1,   121,    -1,
     116,    -1,   122,    -1,   121,    -1,   230,    -1,   231,   122,
      -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short yyrline[] =
{
       0,   267,   267,   267,   294,   301,   302,   306,   310,   316,
     316,   322,   328,   337,   342,   348,   354,   360,   370,   380,
     385,   384,   398,   406,   410,   416,   420,   423,   429,   435,
     436,   440,   444,   449,   454,   457,   458,   461,   462,   467,
     474,   479,   485,   491,   498,   505,   506,   512,   513,   519,
     523,   527,   531,   535,   539,   545,   546,   552,   556,   562,
     566,   570,   574,   578,   582,   589,   593,   597,   601,   605,
     609,   616,   620,   623,   624,   625,   626,   631,   638,   639,
     642,   646,   646,   652,   653,   654,   655,   656,   657,   658,
     659,   660,   661,   662,   663,   664,   665,   666,   667,   668,
     669,   670,   671,   672,   673,   674,   675,   676,   677,   680,
     680,   680,   680,   681,   681,   681,   681,   681,   681,   681,
     682,   682,   682,   682,   682,   682,   682,   683,   683,   683,
     683,   683,   683,   683,   684,   684,   684,   684,   684,   684,
     684,   685,   685,   685,   685,   685,   685,   686,   686,   686,
     689,   693,   693,   718,   734,   746,   758,   770,   775,   781,
     787,   791,   795,   799,   803,   807,   830,   839,   851,   855,
     859,   863,   867,   871,   875,   879,   883,   887,   891,   895,
     899,   903,   908,   912,   916,   920,   924,   928,   928,   933,
     939,   945,   946,   950,   954,   958,   963,   967,   974,   978,
     982,   986,   992,   993,   996,  1000,  1004,  1008,  1014,  1019,
    1025,  1030,  1036,  1041,  1044,  1044,  1051,  1058,  1062,  1065,
    1070,  1077,  1082,  1085,  1090,  1095,  1102,  1117,  1121,  1122,
    1126,  1127,  1128,  1129,  1130,  1131,  1135,  1155,  1159,  1164,
    1168,  1173,  1181,  1185,  1192,  1198,  1204,  1209,  1213,  1217,
    1217,  1222,  1227,  1228,  1237,  1246,  1255,  1255,  1255,  1263,
    1263,  1263,  1271,  1279,  1283,  1283,  1283,  1292,  1291,  1308,
    1313,  1307,  1330,  1329,  1346,  1345,  1375,  1376,  1375,  1401,
    1405,  1409,  1413,  1419,  1420,  1421,  1424,  1425,  1428,  1429,
    1439,  1440,  1446,  1447,  1450,  1451,  1455,  1459,  1467,  1466,
    1480,  1489,  1494,  1501,  1506,  1512,  1518,  1523,  1530,  1540,
    1539,  1551,  1550,  1563,  1571,  1572,  1577,  1584,  1585,  1588,
    1589,  1592,  1596,  1599,  1610,  1613,  1614,  1624,  1625,  1629,
    1632,  1636,  1637,  1647,  1661,  1668,  1669,  1670,  1671,  1674,
    1675,  1678,  1679,  1680,  1681,  1682,  1683,  1684,  1685,  1686,
    1687,  1688,  1691,  1697,  1698,  1701,  1706,  1705,  1713,  1716,
    1721,  1727,  1731,  1735,  1739,  1743,  1747,  1751,  1755,  1759,
    1765,  1769,  1773,  1777,  1781,  1792,  1793,  1799,  1809,  1814,
    1820,  1828,  1834,  1844,  1848,  1851,  1860,  1860,  1885,  1886,
    1890,  1899,  1900,  1906,  1912,  1913,  1914,  1917,  1918,  1919,
    1920,  1923,  1924,  1925,  1928,  1929,  1932,  1933,  1936,  1937,
    1940,  1941,  1942,  1945,  1946,  1949,  1950,  1953
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
  "tCONSTANT", "tCVAR", "tINTEGER", "tFLOAT", "tSTRING", "tXSTRING", 
  "tREGEXP", "tDSTRING", "tDXSTRING", "tDREGEXP", "tNTH_REF", "tBACK_REF", 
  "tQWORDS", "tUPLUS", "tUMINUS", "tPOW", "tCMP", "tEQ", "tEQQ", "tNEQ", 
  "tGEQ", "tLEQ", "tANDOP", "tOROP", "tMATCH", "tNMATCH", "tDOT2", 
  "tDOT3", "tAREF", "tASET", "tLSHFT", "tRSHFT", "tCOLON2", "tCOLON3", 
  "tOP_ASGN", "tASSOC", "tLPAREN", "tLBRACK", "tLBRACE", "tSTAR", 
  "tAMPER", "tSYMBEG", "'='", "'?'", "':'", "'>'", "'<'", "'|'", "'^'", 
  "'&'", "'+'", "'-'", "'*'", "'/'", "'%'", "'!'", "'~'", "LAST_TOKEN", 
  "'{'", "'}'", "'.'", "')'", "','", "'['", "']'", "'`'", "'('", "'\\n'", 
  "';'", "$accept", "program", "@1", "compstmt", "stmts", "stmt", "@2", 
  "@3", "expr", "command_call", "block_command", "command", "mlhs", 
  "mlhs_entry", "mlhs_basic", "mlhs_item", "mlhs_head", "mlhs_node", 
  "lhs", "cname", "fname", "fitem", "undef_list", "@4", "op", "reswords", 
  "arg", "@5", "@6", "aref_args", "paren_args", "opt_paren_args", 
  "call_args", "command_args", "@7", "block_arg", "opt_block_arg", "args", 
  "mrhs", "mrhs_basic", "ret_args", "primary", "@8", "@9", "@10", "@11", 
  "@12", "@13", "@14", "@15", "@16", "@17", "@18", "@19", "@20", "@21", 
  "then", "do", "if_tail", "opt_else", "block_var", "opt_block_var", 
  "do_block", "@22", "block_call", "method_call", "brace_block", "@23", 
  "@24", "case_body", "when_args", "cases", "exc_list", "exc_var", 
  "rescue", "ensure", "literal", "string", "symbol", "sym", "numeric", 
  "variable", "var_ref", "backref", "superclass", "@25", "f_arglist", 
  "f_args", "f_norm_arg", "f_arg", "f_opt", "f_optarg", "f_rest_arg", 
  "f_block_arg", "opt_f_block_arg", "singleton", "@26", "assoc_list", 
  "assocs", "assoc", "operation", "operation2", "operation3", 
  "dot_or_colon", "opt_terms", "opt_nl", "trailer", "term", "terms", 
  "none", 0
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
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,    61,    63,    58,    62,
      60,   124,    94,    38,    43,    45,    42,    47,    37,    33,
     126,   351,   123,   125,    46,    41,    44,    91,    93,    96,
      40,    10,    59
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,   123,   125,   124,   126,   127,   127,   127,   127,   129,
     128,   128,   128,   128,   128,   128,   128,   128,   128,   128,
     130,   128,   128,   128,   128,   128,   128,   131,   131,   131,
     131,   131,   131,   131,   131,   132,   132,   133,   133,   133,
     134,   134,   134,   134,   134,   135,   135,   136,   136,   137,
     137,   137,   137,   137,   137,   138,   138,   139,   139,   140,
     140,   140,   140,   140,   140,   141,   141,   141,   141,   141,
     141,   142,   142,   143,   143,   143,   143,   143,   144,   144,
     145,   146,   145,   147,   147,   147,   147,   147,   147,   147,
     147,   147,   147,   147,   147,   147,   147,   147,   147,   147,
     147,   147,   147,   147,   147,   147,   147,   147,   147,   148,
     148,   148,   148,   148,   148,   148,   148,   148,   148,   148,
     148,   148,   148,   148,   148,   148,   148,   148,   148,   148,
     148,   148,   148,   148,   148,   148,   148,   148,   148,   148,
     148,   148,   148,   148,   148,   148,   148,   148,   148,   148,
     149,   150,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   151,   149,   149,
     149,   152,   152,   152,   152,   152,   152,   152,   153,   153,
     153,   153,   154,   154,   155,   155,   155,   155,   155,   155,
     155,   155,   155,   155,   157,   156,   158,   159,   159,   160,
     160,   161,   161,   162,   162,   162,   163,   164,   164,   164,
     164,   164,   164,   164,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   164,   164,   164,   164,   164,   164,   165,
     164,   164,   164,   164,   164,   164,   166,   167,   164,   168,
     169,   164,   164,   164,   170,   171,   164,   172,   164,   173,
     174,   164,   175,   164,   176,   164,   177,   178,   164,   164,
     164,   164,   164,   179,   179,   179,   180,   180,   181,   181,
     182,   182,   183,   183,   184,   184,   184,   184,   186,   185,
     187,   187,   187,   188,   188,   188,   188,   188,   188,   190,
     189,   191,   189,   192,   193,   193,   193,   194,   194,   195,
     195,   196,   196,   197,   197,   198,   198,   199,   199,   199,
     200,   200,   200,   200,   201,   202,   202,   202,   202,   203,
     203,   204,   204,   204,   204,   204,   204,   204,   204,   204,
     204,   204,   205,   206,   206,   207,   208,   207,   207,   209,
     209,   210,   210,   210,   210,   210,   210,   210,   210,   210,
     211,   211,   211,   211,   211,   212,   212,   213,   214,   214,
     215,   215,   216,   217,   217,   218,   219,   218,   220,   220,
     220,   221,   221,   222,   223,   223,   223,   224,   224,   224,
     224,   225,   225,   225,   226,   226,   227,   227,   228,   228,
     229,   229,   229,   230,   230,   231,   231,   232
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     0,     2,     2,     1,     1,     3,     2,     0,
       4,     3,     3,     3,     2,     3,     3,     3,     3,     3,
       0,     5,     4,     3,     3,     3,     1,     3,     2,     1,
       3,     3,     2,     2,     1,     1,     1,     1,     4,     4,
       2,     4,     4,     2,     2,     1,     3,     1,     3,     1,
       2,     3,     2,     2,     1,     1,     3,     2,     3,     1,
       4,     3,     3,     3,     1,     1,     4,     3,     3,     3,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     0,     4,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       3,     0,     4,     6,     5,     5,     5,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     2,     2,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     2,     2,     3,     3,     3,     3,     0,     4,     5,
       1,     1,     2,     4,     2,     5,     2,     3,     3,     4,
       4,     6,     1,     1,     1,     3,     2,     5,     2,     5,
       4,     7,     3,     1,     0,     2,     2,     2,     1,     1,
       3,     1,     1,     3,     4,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     6,     3,     3,     2,
       4,     3,     3,     4,     3,     1,     4,     3,     1,     0,
       6,     2,     1,     2,     6,     6,     0,     0,     7,     0,
       0,     7,     5,     4,     0,     0,     9,     0,     6,     0,
       0,     8,     0,     5,     0,     9,     0,     0,    12,     1,
       1,     1,     1,     1,     1,     2,     1,     1,     1,     5,
       1,     2,     1,     1,     1,     2,     1,     3,     0,     5,
       2,     4,     4,     2,     4,     4,     3,     2,     1,     0,
       5,     0,     5,     5,     1,     4,     2,     1,     1,     1,
       1,     2,     1,     6,     1,     1,     2,     1,     1,     1,
       1,     1,     2,     2,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     0,     4,     2,     4,
       2,     6,     4,     4,     2,     4,     2,     2,     1,     0,
       1,     1,     1,     1,     1,     1,     3,     3,     1,     3,
       2,     1,     2,     2,     1,     1,     0,     5,     1,     2,
       2,     1,     3,     3,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     0,     1,     0,     1,
       0,     1,     1,     1,     1,     1,     2,     0
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned short yydefact[] =
{
       2,     0,     0,     1,     0,     0,     0,     0,     0,     0,
       0,     0,   406,   256,   259,     0,   279,   280,   281,   282,
     245,   248,   308,   347,   346,   348,   349,     0,     0,   408,
      20,     0,   351,   350,   341,   396,   343,   342,   344,   345,
     339,   340,   330,   229,   329,   331,   231,   232,   353,   354,
     230,     0,     0,     0,     0,   417,   417,    54,     0,     0,
       0,     3,   406,     6,    26,    29,    36,    35,     0,    45,
       0,    49,    55,     0,    34,   190,    37,   252,   227,   228,
     328,   327,   352,   233,   234,   214,     5,     8,    71,    72,
       0,     0,   272,   118,   130,   119,   143,   115,   136,   125,
     124,   141,   123,   122,   117,   146,   127,   116,   131,   135,
     137,   129,   121,   138,   148,   140,   139,   132,   142,   126,
     114,   134,   133,   128,   144,   147,   145,   149,   113,   120,
     111,   112,   109,   110,    73,    75,    74,   104,   105,   102,
      86,    87,    88,    91,    93,    89,   106,   107,    94,    95,
      99,    90,    92,    83,    84,    85,    96,    97,    98,   100,
     101,   103,   108,   386,   274,    76,    77,   352,   385,     0,
     139,   132,   142,   126,   109,   110,    73,    74,    78,    80,
      14,    79,   417,     0,     0,     0,     0,   414,   413,   406,
       0,   415,   407,     0,     0,   245,   248,   308,   408,   293,
     292,     0,     0,   352,   234,     0,     0,     0,     0,     0,
       0,   204,   219,   226,   213,   417,    28,   190,   352,   234,
     417,   391,     0,    44,   417,   307,    43,     0,    32,     0,
       9,   409,   187,     0,     0,   166,   190,   167,   239,     0,
       0,     0,    45,     0,   408,     0,   410,   410,   191,   410,
       0,   410,   388,    53,     0,    59,    64,   337,   336,   338,
     335,   334,    33,   181,   182,     4,   407,     0,     0,     0,
       0,     0,     0,     0,   298,   300,     0,    57,     0,    52,
      50,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     417,     0,     0,   311,   309,   253,   332,   333,   151,     0,
     303,    40,   251,   269,     0,   356,   267,   355,     0,     0,
     369,   405,   404,   276,    81,   417,   417,   324,   284,     0,
     283,     0,     0,     0,     0,     0,     0,   416,     0,     0,
       0,     0,     0,   417,   264,   417,   216,   244,     0,     0,
       0,   206,   218,     0,     0,   417,     0,   208,   247,     0,
     204,   408,   417,   408,     0,   215,    11,    13,    12,     0,
     249,     0,     0,     0,     0,     0,     0,   237,    46,   408,
     192,   241,   412,   411,   194,   412,   196,   412,   390,   242,
     389,     0,     0,   417,     7,    15,    16,    17,    18,    19,
      30,    31,   417,     0,    24,   221,     0,    27,   222,     0,
      51,    58,    23,   150,    25,   165,   171,   176,   177,   178,
     173,   175,   185,   186,   179,   180,   158,   159,   183,   184,
       0,   172,   174,   168,   169,   170,   160,   161,   162,   163,
     164,   397,   402,   238,   403,   214,   306,   397,   399,   398,
     400,   417,     0,   397,   398,   214,   214,   417,   417,     0,
     157,     0,   358,     0,     0,     0,   408,   374,   372,   371,
     370,   373,   381,     0,   369,     0,     0,   375,   417,   378,
     417,   417,   368,     0,     0,   219,   320,   417,   319,     0,
     417,   290,   417,   285,   150,   417,     0,     0,   314,     0,
     263,   287,   257,   286,   260,   401,     0,   397,   398,   417,
       0,     0,     0,   212,   243,   393,     0,   205,   220,   217,
     417,   401,   397,   398,     0,     0,     0,   392,   246,     0,
       0,     0,     0,     0,   198,    10,     0,   188,     0,    22,
      46,   197,     0,   408,   220,    62,   397,   398,     0,   296,
       0,     0,   294,   225,     0,    56,     0,     0,   305,    42,
       0,     0,   203,   304,    41,   202,   240,   302,    39,   301,
      38,     0,     0,   152,   270,     0,     0,   273,     0,     0,
     380,   382,   408,   417,   360,     0,   364,   384,     0,   366,
       0,   367,   277,    82,     0,     0,     0,   322,   291,     0,
       0,   325,     0,     0,   288,     0,   262,   316,     0,     0,
       0,     0,   240,     0,   417,     0,   210,   240,   417,   199,
     205,   408,   417,   417,   200,     0,    21,   408,   193,    60,
     295,     0,     0,     0,   223,   189,   156,   154,   155,     0,
       0,     0,     0,   357,   268,   387,   377,     0,   417,   376,
     417,   417,   383,     0,   379,   417,   369,   321,     0,    65,
      70,     0,   326,   236,     0,   254,   255,     0,   417,     0,
       0,   265,   207,     0,   209,     0,   250,   195,   297,   299,
     224,   153,   312,   310,     0,   359,   417,     0,   362,   363,
     365,     0,     0,     0,   417,   417,     0,   315,   317,   318,
     313,   258,   261,     0,   417,   201,   271,     0,   417,   417,
     401,   397,   398,     0,   323,   417,     0,   211,   275,   361,
     417,    66,   289,   266,   417,     0,   278
};

/* YYDEFGOTO[NTERM-NUM]. */
static const short yydefgoto[] =
{
      -1,     1,     2,   240,    62,    63,   379,   233,    64,    65,
      66,    67,    68,   241,    69,    70,    71,    72,   185,    91,
     178,   179,   180,   494,   460,   166,    74,   469,   381,   245,
     572,   573,   213,   226,   227,   214,   361,   246,   417,   418,
     216,   236,   546,   193,   620,   194,   621,   521,   713,   474,
     471,   652,   328,   330,   493,   666,   339,   512,   613,   614,
     202,   561,   275,   412,    76,    77,   322,   468,   467,   346,
     509,   710,   497,   606,   336,   610,    78,    79,    80,   261,
      81,   218,    83,   219,   326,   473,   485,   486,   487,   488,
     489,   490,   491,   662,   596,   169,   329,   250,   220,   221,
     205,   516,   456,   333,   190,   232,   394,   340,   192,    86
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -608
static const short yypact[] =
{
    -608,   104,  1912,  -608,  4969,    37,   109,  3358,  4552,  2241,
    5064,  5064,  3248,  -608,  -608,  6394,  -608,  -608,  -608,  -608,
    3753,  3848,  3943,  -608,  -608,  -608,  -608,  5064,  4444,    10,
    -608,    95,  -608,  -608,  3468,  2000,  -608,  -608,  3563,  -608,
    -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,
    -608,  6204,  6204,   109,  2736,  5539,  6204,  6580,  4336,  6299,
    6204,  -608,   305,   434,   448,  -608,  -608,   124,    87,  -608,
      78,  6487,  -608,   127,  7494,   107,    82,    28,  -608,    36,
    -608,  -608,     2,  -608,   123,     0,  -608,   434,  -608,  -608,
    5064,    55,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,
    -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,
    -608,  -608,  -608,  -608,  -608,  -608,   103,   215,   245,   247,
    -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,
    -608,  -608,   270,   271,   336,  -608,   343,  -608,  -608,  -608,
    -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,
    -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,
    -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,   349,
    -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,
     113,  -608,   228,    45,   182,   196,    45,  -608,  -608,    43,
     282,  -608,   191,  5064,  5064,   197,   205,   227,    10,  -608,
    -608,   140,   286,    42,    74,     0,  2849,  6204,  6204,  6204,
    4769,  -608,  7414,  -608,  -608,   239,  -608,   166,    19,    46,
     252,  -608,  4869,  -608,  5159,  -608,  -608,  5159,  -608,    44,
    -608,  -608,   250,   264,  2962,   310,   189,   310,  -608,  2736,
     290,   296,   309,  6204,    10,   322,   139,   233,  -608,   287,
     334,   233,  -608,  -608,   198,   207,   295,  -608,  -608,  -608,
    -608,  -608,  -608,   310,   310,  -608,  3153,  5064,  5064,  5064,
    5064,  4969,  5064,  5064,  -608,  -608,  5634,  -608,  2736,  6580,
     333,  5634,  6204,  6204,  6204,  6204,  6204,  6204,  6204,  6204,
    6204,  6204,  6204,  6204,  6204,  6204,  6204,  6204,  6204,  6204,
    6204,  6204,  6204,  6204,  6204,  6204,  6204,  6204,  6609,  6670,
    5539,  6731,  6731,  -608,  -608,  -608,  -608,  -608,  -608,  6204,
    -608,  -608,  -608,   448,   305,  -608,  -608,  -608,  3058,  5064,
     473,  -608,  -608,  -608,  -608,  6204,   438,  -608,  -608,  2354,
     445,  5729,  6204,  2546,   282,  5824,   451,  -608,    11,    11,
     250,  6792,  6853,  5539,  -608,  7270,  7494,  -608,   355,  6204,
    5254,  -608,  -608,  6914,  6975,  5539,  5349,  -608,  -608,   363,
     124,    10,   348,    40,   365,  -608,  -608,  -608,  -608,  4552,
    -608,  6204,  2962,   374,  6914,  6975,   381,  -608,   386,  1555,
    -608,  -608,  5919,  -608,  -608,  6204,  -608,  6204,  -608,  -608,
    -608,  7036,  7097,  5539,   434,   448,   448,   448,   448,  -608,
    -608,  -608,    15,  6204,  -608,  7318,   388,  -608,  -608,   399,
    -608,  -608,  -608,  7318,  -608,   310,  2069,  2069,  2069,  2069,
     612,   612,  7614,  7574,  2069,  2069,  7534,  7534,   482,   482,
    7454,   612,   612,   552,   552,   238,   136,   136,   310,   310,
     310,  2119,  4038,  4133,  4228,   227,  -608,   222,  -608,   329,
    -608,  3943,   398,  -608,  -608,  1440,  1440,    15,    15,  6204,
    7494,   305,  -608,  5064,  3058,   520,    66,   436,  -608,  -608,
    -608,  -608,   487,   490,   481,  2241,   305,  -608,   429,  -608,
     437,   441,  -608,  4660,  4552,  7494,   446,   461,  -608,  2641,
     545,  -608,   498,  -608,  7494,   438,   555,  6204,   453,    50,
    -608,  -608,  -608,  -608,  -608,    68,   227,    85,    94,   227,
     454,  5064,   484,  -608,  -608,  7494,  6204,  -608,  7414,  -608,
     460,  3658,   150,   153,   459,  6204,  7414,  -608,  -608,   466,
    5254,  6731,  6731,   467,  -608,  -608,  5064,  7494,   470,  -608,
     405,  -608,  6204,    10,  7494,   177,   157,   366,   474,  -608,
    1736,  3058,  -608,  7494,  6014,  -608,  6204,  6204,  -608,  -608,
    6204,  6204,  -608,  -608,  -608,  -608,   332,  -608,  -608,  -608,
    -608,  3058,  2962,  7494,  -608,    43,   574,  -608,   480,  6204,
    -608,  -608,    10,   228,  -608,   481,  -608,  -608,    51,  -608,
     503,  -608,  -608,  -608,  6204,  6580,    50,  -608,  -608,  3058,
     588,  -608,  5064,   592,  -608,   594,  -608,  7494,  6109,  2450,
    3058,  3058,    98,    11,  7270,  5444,  -608,   208,  7270,  -608,
     124,    40,   227,   227,  -608,    41,  -608,  1555,  -608,   320,
    -608,   508,   604,  6204,  7366,  7494,  7494,  7494,  7494,  6204,
     606,   509,  3058,  -608,  -608,  -608,  7494,   523,   438,  -608,
     526,   441,  -608,   436,  -608,   441,   473,  -608,   327,   207,
     295,  2241,  -608,  -608,    45,  -608,  -608,  6204,    96,   614,
     629,  -608,  -608,  6204,  -608,   528,  -608,  -608,  -608,  -608,
    7494,  7494,  -608,  -608,   634,  -608,   545,    51,  -608,  -608,
    -608,  2241,  7158,  7219,  5539,   228,  2354,  7494,  -608,  -608,
    -608,  -608,  -608,  3058,  7270,  -608,  -608,   635,   441,   228,
      29,    52,    57,   530,  -608,   498,   636,  -608,  -608,  -608,
     438,   320,  -608,  -608,   545,   639,  -608
};

/* YYPGOTO[NTERM-NUM].  */
static const short yypgoto[] =
{
    -608,  -608,  -608,   603,  -608,    16,  -608,  -608,   495,   -25,
    -608,    23,   483,  -154,   -30,   580,  -608,   -45,   611,    53,
       8,    21,  -608,  -608,    67,  -608,  1049,  -608,  -608,  -289,
       1,  -420,    71,   -23,  -608,  -325,  -214,   550,  -608,   371,
      18,    -2,  -608,  -608,  -608,  -608,  -608,  -608,  -608,  -608,
    -608,  -608,  -608,  -608,  -608,  -608,  -175,  -333,   -71,  -219,
     102,    75,  -608,  -608,  -210,  -608,   586,  -608,  -608,  -343,
    -608,  -608,  -608,  -608,  -557,  -607,  -608,  -608,     9,  -608,
    -608,   362,   657,   537,  -608,  -608,     4,   183,    73,  -608,
    -558,    76,  -527,  -327,  -430,  -608,  -608,  -608,   -51,  -344,
     126,  -280,  -608,  -608,   -29,  -171,   241,    38,   613,  1180
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, parse error.  */
#define YYTABLE_NINF -418
static const short yytable[] =
{
      75,   506,    75,   492,   247,   251,   367,    75,    75,    75,
      75,   343,   253,   201,   373,   164,   514,   181,   217,   217,
      87,   462,   537,   225,   242,    75,   313,   350,   455,   461,
     244,   465,   466,   265,   262,   529,   658,   181,   511,   223,
     664,   529,   -68,   211,   211,   577,   579,   272,   273,   230,
     191,   537,    75,   217,   313,   254,   324,   217,   338,    92,
     599,   601,   321,   338,   520,   -67,   260,   -65,   661,   254,
     -69,   665,   519,   390,   165,   165,   534,   272,   273,   272,
     273,   272,   273,   455,   461,   386,   320,    88,    75,   717,
     318,    89,   559,   -68,   316,   165,   376,   317,   -65,   -70,
     191,   663,   272,   273,     3,   519,   238,   318,   377,   378,
     -67,   499,   314,   345,   558,   -65,   560,   500,   -59,   -69,
     224,    90,   519,   -66,   419,   165,   541,   735,    85,   327,
      85,   231,   187,   188,   319,    85,    85,    85,    85,   664,
     314,   523,   -70,   -65,   482,   483,    85,    85,   724,  -397,
     -68,   -68,   274,    85,   542,   325,   686,   492,   -59,    88,
     344,   231,   730,    89,   187,   188,   187,   188,   311,   -68,
     718,   187,   188,   -67,   -67,   -70,   187,   188,   -69,   -69,
      85,    85,   -61,   276,   -62,    85,   -67,   231,  -397,  -347,
     -64,    75,    75,   308,   277,   -69,   312,   529,   225,   -66,
     539,   -61,   543,  -401,    75,   282,   320,   234,   217,   242,
     -63,   319,   577,   579,   -60,   529,    85,  -347,   551,   -70,
     217,   309,   217,   281,   310,   217,   351,   191,   358,   334,
     698,   699,    75,   211,   420,   700,   335,    75,   570,   -64,
     369,   571,   305,   306,   307,   211,   -67,   370,   242,   -69,
     211,   414,   363,   -61,   352,   392,   422,   353,   -61,   247,
     393,   632,   633,  -401,    75,    75,    75,    75,    75,    75,
      75,    75,   -61,   -61,   217,   384,    75,   254,   341,   217,
     364,   537,   404,   365,   401,   244,   615,   409,   729,  -401,
     681,  -401,   342,  -352,  -401,   371,   649,  -397,   375,   345,
     529,  -346,   247,   385,   -66,   588,   365,   282,   217,   530,
     570,   354,   402,   347,   247,   403,   626,   210,   -67,    85,
      85,  -352,   295,   296,  -352,   222,    75,    75,   244,  -346,
     631,  -348,    85,  -349,   619,   709,    85,    75,   -61,   492,
     244,    75,   303,   304,   305,   306,   307,   224,    85,   395,
      85,   217,   247,    85,   393,   360,  -351,  -350,   217,  -348,
      85,  -349,   472,   217,    82,    85,    82,   553,   366,   167,
     380,    82,    82,    82,    82,   454,   382,   203,   244,   282,
      75,  -234,   638,   527,  -351,  -350,   513,   513,   181,    82,
     217,   -63,    85,    85,    85,    85,    85,    85,    85,    85,
     545,   217,    85,   397,    85,   387,  -240,    85,   393,  -234,
     682,   388,  -234,   702,   684,   723,    82,   571,   454,   255,
     649,   657,  -341,   519,   -47,   -69,   187,   188,   -66,  -344,
     454,   671,   569,   255,  -240,   331,    85,  -240,   574,   696,
     391,   703,   578,   580,   704,   -63,   165,   399,   -60,   421,
    -341,   454,    82,   499,    85,    85,   568,  -344,   503,   708,
     685,   510,   -63,   332,   540,    85,   687,   -63,   454,    85,
     524,    75,    75,   267,   268,   269,   270,   271,   538,    85,
     544,   -63,   -63,    75,   272,   273,    85,   549,   396,   530,
     398,    85,   400,   184,   184,   184,   550,    75,   199,   706,
     727,   602,   -56,   181,   564,   183,   186,   189,    85,   584,
     184,   734,   612,   499,   565,   603,   576,   568,    85,    75,
     -48,   -56,   228,   477,   594,   478,   479,   480,   481,    85,
     587,   477,   589,   478,   479,   480,   481,   590,   217,    84,
     591,    84,   581,   582,    75,   595,    84,    84,    84,    84,
     605,   282,   204,   598,   609,    82,    82,   600,   201,    75,
     165,   165,   604,   630,    84,   616,   482,   483,    82,   618,
     215,   215,   622,   184,   482,   483,   625,   627,   208,    75,
      75,   629,   634,   636,   654,   323,   303,   304,   305,   306,
     307,    84,   639,   484,   256,   655,    82,   483,   673,    85,
      85,    82,   675,   668,   676,    61,   249,    75,   256,   688,
      75,    85,   182,    73,   689,    73,   692,    75,    75,    75,
      73,   282,   693,   653,   711,    85,   200,    84,    82,    82,
      82,    82,    82,    82,    82,    82,   295,   296,   695,   712,
      82,   255,   697,   715,   716,   728,   733,    85,   731,   736,
      75,   280,   424,   247,   732,   302,   303,   304,   305,   306,
     307,   513,   641,   315,   168,    73,    85,   592,   659,    75,
     701,   660,    85,     0,     0,   266,   184,   184,     0,   244,
       0,   282,     0,     0,     0,     0,     0,    85,   348,   349,
      82,    82,     0,     0,     0,     0,   295,   296,     0,    75,
       0,    82,   217,     0,    75,    82,     0,    85,    85,     0,
       0,    75,     0,   300,   301,   302,   303,   304,   305,   306,
     307,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      84,    84,     0,     0,     0,    85,     0,     0,    85,     0,
       0,     0,     0,    84,    82,    85,    85,    85,     0,     0,
     184,   184,   184,   184,     0,   184,   184,     0,     0,     0,
     215,     0,   405,   406,   407,   408,     0,   410,   411,   454,
       0,    84,   215,     0,   372,     0,    84,   215,    85,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    85,     0,     0,
       0,     0,     0,    84,    84,    84,    84,    84,    84,    84,
      84,     0,   184,     0,     0,    84,   256,    73,     0,     0,
       0,     0,     0,     0,   476,     0,   416,    85,     0,     0,
      85,   416,    85,     0,     0,    82,    82,   383,     0,    85,
       0,     0,     0,     0,     0,    73,     0,    82,     0,     0,
      73,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    82,     0,     0,     0,    84,    84,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    84,    73,     0,     0,
      84,     0,    73,    82,     0,   496,     0,     0,     0,    73,
       0,   416,     0,     0,     0,   508,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    82,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    84,
       0,     0,   203,    82,     0,     0,     0,     0,     0,     0,
       0,   475,     0,     0,     0,     0,     0,     0,     0,    73,
       0,     0,   502,    82,    82,     0,   505,     0,     0,     0,
      73,     0,     0,     0,    73,     0,   184,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   669,   585,     0,
       0,    82,     0,     0,    82,     0,     0,     0,     0,     0,
       0,    82,    82,    82,     0,   548,     0,     0,     0,     0,
       0,     0,     0,    73,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   184,     0,     0,     0,     0,     0,
      84,    84,     0,     0,    82,     0,   623,     0,     0,     0,
       0,     0,    84,     0,     0,     0,     0,     0,     0,   184,
       0,     0,     0,    82,     0,     0,    84,     0,     0,     0,
       0,   635,     0,   199,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    84,     0,
       0,     0,     0,    82,     0,     0,     0,     0,    82,   212,
     212,     0,     0,     0,     0,    82,     0,   586,     0,     0,
       0,     0,     0,    84,     0,    73,     0,     0,   593,     0,
       0,     0,     0,     0,     0,   184,    73,   204,    84,     0,
     235,   237,   608,     0,   212,   212,     0,   674,   263,   264,
      73,     0,     0,     0,     0,     0,     0,     0,    84,    84,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   670,     0,     0,     0,    84,     0,     0,    84,
       0,     0,     0,     0,     0,     0,    84,    84,    84,     0,
       0,     0,     0,     0,   642,     0,     0,     0,     0,     0,
       0,   200,    73,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   650,   651,     0,     0,     0,    84,
       0,     0,    73,    73,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    84,     0,
       0,     0,   672,     0,     0,     0,   667,     0,     0,     0,
      73,     0,   678,   679,   680,     0,     0,     0,     0,     0,
      73,    73,    73,     0,     0,   248,   252,     0,    84,     0,
       0,     0,     0,    84,     0,     0,     0,     0,     0,     0,
      84,     0,     0,     0,     0,   694,   355,   356,   263,   212,
       0,     0,     0,    73,     0,     0,     0,     0,     0,     0,
       0,   212,     0,   212,   705,     0,   212,     0,     0,     0,
       0,     0,    73,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   389,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   719,     0,     0,     0,     0,   725,
       0,     0,    73,     0,     0,     0,   726,    73,     0,     0,
       0,     0,     0,     0,    73,   415,     0,     0,     0,     0,
     423,   425,   426,   427,   428,   429,   430,   431,   432,   433,
     434,   435,   436,   437,   438,   439,   440,   441,   442,   443,
     444,   445,   446,   447,   448,   449,   450,     0,     0,   212,
       0,     0,   337,     0,     0,     0,     0,     0,   470,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   495,     0,     0,     0,     0,     0,
     415,   504,     0,     0,   495,   362,     0,     0,     0,     0,
     362,     0,   212,     0,   374,     0,     0,     0,   525,   528,
       0,     0,     0,     0,   212,   536,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     547,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -417,   554,     0,     0,   536,     0,   554,     0,  -417,  -417,
    -417,     0,   212,  -417,  -417,  -417,     0,  -417,     0,     0,
       0,     0,   563,     0,     0,     0,     0,  -417,     0,     0,
       0,     0,     0,     0,     0,     0,  -417,  -417,     0,  -417,
    -417,  -417,  -417,  -417,     0,     0,     0,     0,     0,     0,
     248,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   498,   501,     0,   583,     0,
       0,     0,     0,     0,     0,     0,  -417,     0,     0,     0,
       0,     0,     0,   248,     0,   362,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   248,     0,     0,     0,     0,
       0,     0,   362,  -417,  -417,  -417,   617,     0,  -417,     0,
     224,  -417,  -417,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   624,     0,     0,     0,     0,
       0,     0,     0,   248,   628,     0,     0,     0,     0,   528,
       0,     0,   562,     0,     0,     0,     0,     0,     0,     0,
       0,   637,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   644,     0,   645,   646,     0,     0,   647,
     648,     0,     0,     0,   282,   283,   284,   285,   286,   287,
     288,   289,   290,   291,   292,   293,   294,     0,   656,   295,
     296,   575,     0,     0,     0,   575,   575,   562,   562,     0,
       0,     0,   297,   554,   298,   299,   300,   301,   302,   303,
     304,   305,   306,   307,     0,     0,     0,   554,   597,     0,
     597,   597,     0,     0,   536,     0,   231,   607,     0,     0,
     611,     0,   501,     0,     0,   501,     0,     0,     0,     0,
       0,     0,   690,     0,     0,     0,     0,     0,   691,   575,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     362,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   707,     0,     0,     0,
       0,     0,   714,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,     0,
       0,     0,    12,   212,    13,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,   195,   196,   197,    23,    24,
      25,    26,     0,   337,     0,     0,     0,     0,     0,     0,
       0,   198,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,     0,   362,     0,     0,     0,   362,     0,
       0,     0,   575,   575,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,     0,     0,    54,    55,    56,    57,
       0,    58,     0,     0,     0,     0,     0,   640,   501,     0,
     597,   597,     0,     0,     0,   597,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   501,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   611,     0,     0,     0,
       0,     0,     0,     0,   248,   337,     0,     0,     0,     0,
       0,     0,     0,     0,   362,     0,     0,     0,   597,   337,
       0,     0,     0,     0,     0,   501,     0,     0,     0,     0,
     501,     0,  -417,     4,   611,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
    -235,     0,    54,    55,    56,    57,     0,    58,  -235,  -235,
    -235,     0,     0,  -235,  -235,  -235,     0,  -235,     0,     0,
       0,    59,    60,     0,     0,     0,     0,  -235,  -235,     0,
       0,     0,     0,  -417,  -417,     0,  -235,  -235,     0,  -235,
    -235,  -235,  -235,  -235,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -235,
    -235,  -235,  -235,  -235,  -235,  -235,  -235,  -235,  -235,  -235,
    -235,  -235,     0,     0,  -235,  -235,  -235,     0,     0,  -235,
       0,     0,     0,     0,     0,     0,     0,  -235,  -235,  -235,
    -235,  -235,  -235,  -235,  -235,  -235,  -235,  -235,  -235,     0,
       0,     0,     0,  -235,  -235,  -235,  -235,  -235,  -235,  -401,
       0,  -235,  -235,     0,     0,     0,     0,  -401,  -401,  -401,
       0,     0,  -401,  -401,  -401,     0,  -401,     0,   282,  -418,
    -418,  -418,  -418,   287,   288,  -401,  -401,  -418,  -418,     0,
       0,     0,     0,   295,   296,  -401,  -401,     0,  -401,  -401,
    -401,  -401,  -401,     0,     0,     0,     0,     0,   298,   299,
     300,   301,   302,   303,   304,   305,   306,   307,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -401,  -401,
    -401,  -401,  -401,  -401,  -401,  -401,  -401,  -401,  -401,  -401,
    -401,     0,     0,  -401,  -401,  -401,     0,   567,     0,     0,
       0,     0,     0,     0,     0,   -68,  -401,     0,  -401,  -401,
    -401,  -401,  -401,  -401,  -401,  -401,  -401,  -401,     0,     0,
       0,  -401,  -401,  -401,  -401,   -62,  -401,     0,     0,     0,
    -401,  -401,     4,     0,     5,     6,     7,     8,     9,  -417,
    -417,  -417,    10,    11,     0,     0,  -417,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    53,     0,
       0,    54,    55,    56,    57,     0,    58,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      59,    60,     0,     0,     0,     4,     0,     5,     6,     7,
       8,     9,  -417,  -417,  -417,    10,    11,     0,  -417,  -417,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,     0,     0,    54,    55,    56,    57,     0,    58,
       0,     4,     0,     5,     6,     7,     8,     9,     0,     0,
    -417,    10,    11,    59,    60,  -417,    12,  -417,    13,    14,
      15,    16,    17,    18,    19,  -417,  -417,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    53,     0,     0,
      54,    55,    56,    57,     0,    58,     0,     4,     0,     5,
       6,     7,     8,     9,     0,     0,  -417,    10,    11,    59,
      60,  -417,    12,     0,    13,    14,    15,    16,    17,    18,
      19,  -417,  -417,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,     0,     0,    54,    55,    56,    57,
       0,    58,     4,     0,     5,     6,     7,     8,     9,     0,
    -417,  -417,    10,    11,     0,    59,    60,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,  -417,  -417,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    53,     0,
       0,    54,    55,    56,    57,     0,    58,     4,     0,     5,
       6,     7,     8,     9,     0,     0,     0,    10,    11,     0,
      59,    60,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,  -417,  -417,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    53,     0,     0,   239,    55,    56,    57,
       0,    58,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    59,    60,     0,     0,     0,
       4,  -417,     5,     6,     7,     8,     9,  -417,  -417,     0,
      10,    11,     0,     0,     0,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,    54,
      55,    56,    57,     0,    58,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    59,    60,
       0,     0,     0,     4,  -417,     5,     6,     7,     8,     9,
    -417,  -417,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    53,
       0,     0,    54,    55,    56,    57,     0,    58,     0,     4,
       0,     5,     6,     7,     8,     9,     0,     0,  -417,    10,
      11,    59,    60,     0,    12,  -417,    13,    14,    15,    16,
      17,    18,    19,  -417,  -417,     0,     0,    20,    21,    22,
      23,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,     0,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,     0,     0,    54,    55,
      56,    57,     0,    58,     0,     0,     5,     6,     7,     8,
       9,     0,     0,     0,    10,    11,     0,    59,    60,    12,
       0,    13,    14,    15,    16,    17,    18,    19,     0,  -417,
    -417,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,     0,     0,    54,    55,    56,    57,     0,    58,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,    59,    60,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,   347,     0,    20,    21,    22,
      23,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,     0,     0,    54,    55,
      56,    57,     0,    58,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    59,    60,     0,
       0,    93,    94,    95,    96,    97,    98,    99,   100,   187,
     188,   101,   102,   103,   104,   105,     0,     0,   106,   107,
     108,   109,   110,   111,   112,     0,     0,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
      36,    37,   136,    39,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   137,   138,   139,   140,   141,
     142,     0,   143,   144,     0,     0,   145,     0,     0,     0,
     146,   147,   148,   149,     0,     0,     0,     0,     0,     0,
       0,   150,     0,     0,     0,     0,     0,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,     0,   161,     0,
       0,  -394,  -394,  -394,     0,  -394,     0,   162,   163,  -394,
    -394,     0,     0,     0,  -394,     0,  -394,  -394,  -394,  -394,
    -394,  -394,  -394,     0,  -394,     0,     0,  -394,  -394,  -394,
    -394,  -394,  -394,  -394,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -394,     0,     0,  -394,  -394,  -394,  -394,
    -394,  -394,  -394,  -394,  -394,  -394,  -394,  -394,  -394,  -394,
    -394,  -394,  -394,  -394,  -394,  -394,  -394,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -394,     0,     0,  -394,  -394,
    -394,  -394,  -394,  -394,     0,     0,  -395,  -395,  -395,     0,
    -395,     0,     0,     0,  -395,  -395,     0,  -394,  -394,  -395,
    -394,  -395,  -395,  -395,  -395,  -395,  -395,  -395,  -394,  -395,
       0,     0,  -395,  -395,  -395,  -395,  -395,  -395,  -395,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -395,     0,
       0,  -395,  -395,  -395,  -395,  -395,  -395,  -395,  -395,  -395,
    -395,  -395,  -395,  -395,  -395,  -395,  -395,  -395,  -395,  -395,
    -395,  -395,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -395,     0,     0,  -395,  -395,  -395,  -395,  -395,  -395,     0,
       0,  -397,  -397,  -397,     0,  -397,     0,     0,     0,  -397,
    -397,     0,  -395,  -395,  -397,  -395,  -397,  -397,  -397,  -397,
    -397,  -397,  -397,  -395,     0,     0,     0,  -397,  -397,  -397,
    -397,  -397,  -397,  -397,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -397,     0,     0,  -397,  -397,  -397,  -397,
    -397,  -397,  -397,  -397,  -397,  -397,  -397,  -397,  -397,  -397,
    -397,  -397,  -397,  -397,  -397,  -397,  -397,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -397,   567,     0,  -397,  -397,
    -397,  -397,  -397,  -397,   -68,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,     0,  -397,  -397,    12,
       0,    13,    14,    15,    16,    17,    18,    19,  -397,     0,
       0,     0,   195,    21,    22,    23,    24,    25,    26,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      53,     0,     0,   206,    55,    56,   207,   208,    58,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,   209,    60,    12,     0,    13,    14,    15,    16,
      17,    18,    19,   210,     0,     0,     0,   195,    21,    22,
      23,    24,    25,    26,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,     0,     0,   206,    55,
      56,   207,   208,    58,     0,     0,  -214,  -214,  -214,     0,
    -214,     0,     0,     0,  -214,  -214,     0,   209,    60,  -214,
       0,  -214,  -214,  -214,  -214,  -214,  -214,  -214,   222,     0,
       0,     0,  -214,  -214,  -214,  -214,  -214,  -214,  -214,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -214,     0,
       0,  -214,  -214,  -214,  -214,  -214,  -214,  -214,  -214,  -214,
    -214,  -214,  -214,  -214,  -214,  -214,  -214,  -214,  -214,  -214,
    -214,  -214,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -214,     0,     0,  -214,  -214,  -214,  -214,  -214,  -214,     0,
       0,  -399,  -399,  -399,     0,  -399,     0,     0,     0,  -399,
    -399,     0,  -214,  -214,  -399,     0,  -399,  -399,  -399,  -399,
    -399,  -399,  -399,   224,     0,     0,     0,  -399,  -399,  -399,
    -399,  -399,  -399,  -399,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -399,     0,     0,  -399,  -399,  -399,  -399,
    -399,  -399,  -399,  -399,  -399,  -399,  -399,  -399,  -399,  -399,
    -399,  -399,  -399,  -399,  -399,  -399,  -399,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -399,     0,     0,  -399,  -399,
    -399,  -399,  -399,  -399,     0,     0,  -398,  -398,  -398,     0,
    -398,     0,     0,     0,  -398,  -398,     0,  -399,  -399,  -398,
       0,  -398,  -398,  -398,  -398,  -398,  -398,  -398,  -399,     0,
       0,     0,  -398,  -398,  -398,  -398,  -398,  -398,  -398,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -398,     0,
       0,  -398,  -398,  -398,  -398,  -398,  -398,  -398,  -398,  -398,
    -398,  -398,  -398,  -398,  -398,  -398,  -398,  -398,  -398,  -398,
    -398,  -398,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -398,     0,     0,  -398,  -398,  -398,  -398,  -398,  -398,     0,
       0,  -400,  -400,  -400,     0,  -400,     0,     0,     0,  -400,
    -400,     0,  -398,  -398,  -400,     0,  -400,  -400,  -400,  -400,
    -400,  -400,  -400,  -398,     0,     0,     0,  -400,  -400,  -400,
    -400,  -400,  -400,  -400,     0,     0,     0,     0,     0,     0,
       0,     0,     0,  -400,     0,     0,  -400,  -400,  -400,  -400,
    -400,  -400,  -400,  -400,  -400,  -400,  -400,  -400,  -400,  -400,
    -400,  -400,  -400,  -400,  -400,  -400,  -400,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -400,     0,     0,  -400,  -400,
    -400,  -400,  -400,  -400,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -400,  -400,    93,
      94,    95,    96,    97,    98,    99,   100,     0,  -400,   101,
     102,   103,   104,   105,     0,     0,   106,   107,   108,   109,
     110,   111,   112,     0,     0,   113,   114,   115,   170,   171,
     172,   173,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   174,   175,   176,   135,   257,   258,
     177,   259,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   137,   138,   139,   140,   141,   142,     0,
     143,   144,     0,     0,   145,     0,     0,     0,   146,   147,
     148,   149,     0,     0,     0,     0,     0,     0,     0,   150,
       0,     0,     0,     0,     0,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,     0,   161,    93,    94,    95,
      96,    97,    98,    99,   100,   162,     0,   101,   102,   103,
     104,   105,     0,     0,   106,   107,   108,   109,   110,   111,
     112,     0,     0,   113,   114,   115,   170,   171,   172,   173,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   174,   175,   176,   135,   229,     0,   177,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   137,   138,   139,   140,   141,   142,     0,   143,   144,
       0,     0,   145,     0,     0,     0,   146,   147,   148,   149,
       0,     0,     0,     0,     0,     0,     0,   150,     0,    58,
       0,     0,     0,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,     0,   161,    93,    94,    95,    96,    97,
      98,    99,   100,   162,     0,   101,   102,   103,   104,   105,
       0,     0,   106,   107,   108,   109,   110,   111,   112,     0,
       0,   113,   114,   115,   170,   171,   172,   173,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     174,   175,   176,   135,     0,     0,   177,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   137,
     138,   139,   140,   141,   142,     0,   143,   144,     0,     0,
     145,     0,     0,     0,   146,   147,   148,   149,     0,     0,
       0,     0,     0,     0,     0,   150,     0,    58,     0,     0,
       0,   151,   152,   153,   154,   155,   156,   157,   158,   159,
     160,     0,   161,    93,    94,    95,    96,    97,    98,    99,
     100,   162,     0,   101,   102,   103,   104,   105,     0,     0,
     106,   107,   108,   109,   110,   111,   112,     0,     0,   113,
     114,   115,   170,   171,   172,   173,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   174,   175,
     176,   135,     0,     0,   177,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   137,   138,   139,
     140,   141,   142,     0,   143,   144,     0,     0,   145,     0,
       0,     0,   146,   147,   148,   149,     0,     0,     0,     0,
       0,     0,     0,   150,     0,     0,     0,     0,     0,   151,
     152,   153,   154,   155,   156,   157,   158,   159,   160,     0,
     161,     0,     5,     6,     7,     0,     9,     0,     0,   162,
      10,    11,     0,     0,     0,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,   195,    21,
      22,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,   206,
      55,    56,   207,   208,    58,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,   209,    60,
      10,    11,     0,     0,   357,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,   195,    21,
      22,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,   206,
      55,    56,   207,   208,    58,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     8,     9,     0,   209,    60,
      10,    11,     0,     0,   368,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,    54,
      55,    56,    57,     0,    58,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,    59,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,     0,     0,    54,    55,    56,    57,     0,    58,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,    59,    60,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,   195,    21,
      22,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,   206,
      55,    56,   207,   208,    58,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,   209,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,   195,    21,    22,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,     0,     0,   206,    55,    56,   526,   208,    58,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,   209,    60,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,   195,   196,
     197,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,   206,
      55,    56,   535,   208,    58,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,   209,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,   195,   196,   197,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,     0,     0,   206,    55,    56,   683,   208,    58,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,   209,    60,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,   195,    21,
      22,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,   206,
      55,    56,   243,     0,    58,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,   209,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,   195,    21,    22,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,     0,     0,   206,    55,    56,   413,     0,    58,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,   209,    60,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,   195,   196,
     197,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,   206,
      55,    56,   413,     0,    58,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,   209,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,   195,   196,   197,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,     0,     0,   206,    55,    56,   507,     0,    58,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,   209,    60,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,   195,    21,
      22,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,   206,
      55,    56,   552,     0,    58,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,   209,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,   195,   196,   197,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,     0,     0,   206,    55,    56,   643,     0,    58,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,   209,    60,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,   195,   196,
     197,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,   206,
      55,    56,   677,     0,    58,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,   209,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,   195,   196,   197,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,     0,     0,   206,    55,    56,     0,     0,    58,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,   209,    60,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,   195,    21,
      22,    23,    24,    25,    26,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,     0,     0,   206,
      55,    56,     0,     0,    58,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,     0,   209,    60,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,   195,   196,   197,    23,    24,    25,    26,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   198,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    53,     0,     0,    54,    55,    56,    57,     0,    58,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,   195,   196,   197,    23,
      24,    25,    26,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   198,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    53,     0,     0,   278,    55,    56,
     279,     0,    58,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,   195,
     196,   197,    23,    24,    25,    26,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   198,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   451,
     452,     0,     0,   453,     0,     0,     0,    53,     0,     0,
     206,    55,    56,     0,     0,    58,   137,   138,   139,   140,
     141,   142,     0,   143,   144,     0,     0,   145,     0,     0,
       0,   146,   147,   148,   149,     0,     0,     0,     0,     0,
       0,     0,   150,     0,     0,     0,     0,     0,   151,   152,
     153,   154,   155,   156,   157,   158,   159,   160,     0,   161,
     457,   458,     0,     0,   459,     0,     0,     0,   162,     0,
       0,     0,     0,     0,     0,     0,     0,   137,   138,   139,
     140,   141,   142,     0,   143,   144,     0,     0,   145,     0,
       0,     0,   146,   147,   148,   149,     0,     0,     0,     0,
       0,     0,     0,   150,     0,     0,     0,     0,     0,   151,
     152,   153,   154,   155,   156,   157,   158,   159,   160,     0,
     161,   463,   458,     0,     0,   464,     0,     0,     0,   162,
       0,     0,     0,     0,     0,     0,     0,     0,   137,   138,
     139,   140,   141,   142,     0,   143,   144,     0,     0,   145,
       0,     0,     0,   146,   147,   148,   149,     0,     0,     0,
       0,     0,     0,     0,   150,     0,     0,     0,     0,     0,
     151,   152,   153,   154,   155,   156,   157,   158,   159,   160,
       0,   161,   515,   452,     0,     0,   453,     0,     0,     0,
     162,     0,     0,     0,     0,     0,     0,     0,     0,   137,
     138,   139,   140,   141,   142,     0,   143,   144,     0,     0,
     145,     0,     0,     0,   146,   147,   148,   149,     0,     0,
       0,     0,     0,     0,     0,   150,     0,     0,     0,     0,
       0,   151,   152,   153,   154,   155,   156,   157,   158,   159,
     160,     0,   161,   517,   458,     0,     0,   518,     0,     0,
       0,   162,     0,     0,     0,     0,     0,     0,     0,     0,
     137,   138,   139,   140,   141,   142,     0,   143,   144,     0,
       0,   145,     0,     0,     0,   146,   147,   148,   149,     0,
       0,     0,     0,     0,     0,     0,   150,     0,     0,     0,
       0,     0,   151,   152,   153,   154,   155,   156,   157,   158,
     159,   160,     0,   161,   531,   452,     0,     0,   453,     0,
       0,     0,   162,     0,     0,     0,     0,     0,     0,     0,
       0,   137,   138,   139,   140,   141,   142,     0,   143,   144,
       0,     0,   145,     0,     0,     0,   146,   147,   148,   149,
       0,     0,     0,     0,     0,     0,     0,   150,     0,     0,
       0,     0,     0,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,     0,   161,   532,   458,     0,     0,   533,
       0,     0,     0,   162,     0,     0,     0,     0,     0,     0,
       0,     0,   137,   138,   139,   140,   141,   142,     0,   143,
     144,     0,     0,   145,     0,     0,     0,   146,   147,   148,
     149,     0,     0,     0,     0,     0,     0,     0,   150,     0,
       0,     0,     0,     0,   151,   152,   153,   154,   155,   156,
     157,   158,   159,   160,     0,   161,   555,   452,     0,     0,
     453,     0,     0,     0,   162,     0,     0,     0,     0,     0,
       0,     0,     0,   137,   138,   139,   140,   141,   142,     0,
     143,   144,     0,     0,   145,     0,     0,     0,   146,   147,
     148,   149,     0,     0,     0,     0,     0,     0,     0,   150,
       0,     0,     0,     0,     0,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,     0,   161,   556,   458,     0,
       0,   557,     0,     0,     0,   162,     0,     0,     0,     0,
       0,     0,     0,     0,   137,   138,   139,   140,   141,   142,
       0,   143,   144,     0,     0,   145,     0,     0,     0,   146,
     147,   148,   149,     0,     0,     0,     0,     0,     0,     0,
     150,     0,     0,     0,     0,     0,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,     0,   161,   720,   452,
       0,     0,   453,     0,     0,     0,   162,     0,     0,     0,
       0,     0,     0,     0,     0,   137,   138,   139,   140,   141,
     142,     0,   143,   144,     0,     0,   145,     0,     0,     0,
     146,   147,   148,   149,     0,     0,     0,     0,     0,     0,
       0,   150,     0,     0,     0,     0,     0,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,     0,   161,   721,
     458,     0,     0,   722,     0,     0,     0,   162,     0,     0,
       0,     0,     0,     0,     0,     0,   137,   138,   139,   140,
     141,   142,     0,   143,   144,     0,     0,   145,     0,     0,
       0,   146,   147,   148,   149,     0,     0,     0,     0,     0,
       0,     0,   150,     0,     0,     0,     0,     0,   151,   152,
     153,   154,   155,   156,   157,   158,   159,   160,     0,   161,
       0,     0,     0,     0,     0,     0,     0,     0,   162,   282,
     283,   284,   285,   286,   287,   288,   289,   290,   291,   292,
     293,   294,     0,     0,   295,   296,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   297,     0,   298,
     299,   300,   301,   302,   303,   304,   305,   306,   307,     0,
       0,     0,     0,     0,     0,     0,   522,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
       0,     0,   295,   296,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   297,     0,   298,   299,   300,
     301,   302,   303,   304,   305,   306,   307,     0,     0,     0,
       0,     0,     0,     0,  -219,   282,   283,   284,   285,   286,
     287,   288,   289,   290,   291,   292,   293,   294,     0,     0,
     295,   296,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   297,     0,   298,   299,   300,   301,   302,
     303,   304,   305,   306,   307,     0,     0,     0,     0,     0,
       0,     0,  -220,   282,   283,   284,   285,   286,   287,   288,
     289,   290,   291,   292,   293,   294,     0,     0,   295,   296,
       0,     0,     0,   359,     0,     0,     0,     0,     0,     0,
       0,   297,     0,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   282,   283,   284,   285,   286,   287,   288,
     289,   290,   291,   292,   293,   294,     0,     0,   295,   296,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   297,   566,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   282,   283,   284,   285,   286,   287,   288,
     289,   290,   291,   292,   293,   294,     0,     0,   295,   296,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   297,     0,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   282,   283,   284,   285,   286,   287,   288,
     289,   290,   291,   292,  -418,  -418,     0,     0,   295,   296,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   282,   283,   284,   285,   286,   287,   288,
     289,     0,   291,   292,     0,     0,     0,     0,   295,   296,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   282,   283,   284,   285,   286,   287,   288,
       0,     0,   291,   292,     0,     0,     0,     0,   295,   296,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307
};

static const short yycheck[] =
{
       2,   344,     4,   330,    55,    56,   220,     9,    10,    11,
      12,   186,    57,    15,   224,     7,   349,     8,    20,    21,
       4,   310,   366,    22,    54,    27,    26,   198,   308,   309,
      55,   311,   312,    62,    59,   360,   593,    28,    27,    21,
     598,   366,    13,    20,    21,   465,   466,    36,    37,    28,
      12,   395,    54,    55,    26,    57,     1,    59,    13,     6,
     490,   491,    85,    13,   353,    13,    58,    25,   595,    71,
      13,   598,   352,   244,     7,     8,   365,    36,    37,    36,
      37,    36,    37,   363,   364,   239,    85,    50,    90,   696,
      88,    54,    77,    25,    58,    28,    52,    61,    96,    25,
      62,    50,    36,    37,     0,   385,    53,    88,    64,    65,
      25,    15,   112,    17,   403,    96,   101,   336,   116,    25,
     120,    84,   402,    25,   278,    58,    86,   734,     2,    91,
       4,   121,   121,   122,    88,     9,    10,    11,    12,   697,
     112,   355,    96,   101,    93,    94,    20,    21,   705,   120,
     121,   122,    28,    27,   114,   100,   115,   484,   116,    50,
     189,   121,   719,    54,   121,   122,   121,   122,    86,   101,
     697,   121,   122,   121,   122,   101,   121,   122,   121,   122,
      54,    55,    25,    96,   116,    59,   101,   121,   120,    86,
     116,   193,   194,    86,   116,   101,   114,   522,   197,   101,
     371,   116,   373,    26,   206,    69,   205,   112,   210,   239,
     116,    88,   632,   633,   116,   540,    90,   114,   389,    96,
     222,   114,   224,    96,   117,   227,    86,   189,   210,   116,
     660,   661,   234,   210,   279,   665,     8,   239,    88,   116,
     222,    88,   106,   107,   108,   222,    96,   224,   278,    96,
     227,   276,    86,    96,   114,   116,   281,   117,   101,   310,
     121,   541,   542,    86,   266,   267,   268,   269,   270,   271,
     272,   273,   115,   116,   276,    86,   278,   279,    96,   281,
     114,   625,   266,   117,    86,   310,   505,   271,   718,   112,
     623,   114,    96,    86,   117,   224,    88,   120,   227,    17,
     625,    86,   353,   114,    96,   476,   117,    69,   310,   360,
      88,    25,   114,   122,   365,   117,   530,   120,    96,   193,
     194,   114,    84,    85,   117,   120,   328,   329,   353,   114,
     540,    86,   206,    86,   509,   678,   210,   339,   116,   666,
     365,   343,   104,   105,   106,   107,   108,   120,   222,   116,
     224,   353,   403,   227,   121,   116,    86,    86,   360,   114,
     234,   114,   324,   365,     2,   239,     4,   392,   116,     7,
     120,     9,    10,    11,    12,   308,   112,    15,   403,    69,
     382,    86,   553,   360,   114,   114,   348,   349,   379,    27,
     392,    25,   266,   267,   268,   269,   270,   271,   272,   273,
     379,   403,   276,   116,   278,   115,    86,   281,   121,   114,
     624,   115,   117,    86,   628,   704,    54,    88,   351,    57,
      88,   592,    86,   703,   115,    96,   121,   122,    96,    86,
     363,   606,   455,    71,   114,    86,   310,   117,   461,   658,
     118,   114,   465,   466,   117,   116,   379,   113,   116,   116,
     114,   384,    90,    15,   328,   329,   455,   114,    13,   678,
     631,    10,    96,   114,   116,   339,   637,   101,   401,   343,
     115,   473,   474,    39,    40,    41,    42,    43,   115,   353,
     115,   115,   116,   485,    36,    37,   360,   113,   247,   540,
     249,   365,   251,    10,    11,    12,   115,   499,    15,   674,
     714,   493,   116,   494,   116,    10,    11,    12,   382,   471,
      27,   730,    14,    15,   115,   494,   118,   516,   392,   521,
     115,   116,    27,    50,   486,    52,    53,    54,    55,   403,
      10,    50,    96,    52,    53,    54,    55,    50,   540,     2,
      50,     4,   467,   468,   546,   116,     9,    10,    11,    12,
      89,    69,    15,   116,     9,   193,   194,   116,   560,   561,
     493,   494,   116,   540,    27,    10,    93,    94,   206,   116,
      20,    21,   118,    90,    93,    94,   116,   118,    94,   581,
     582,   115,   115,   113,    10,    90,   104,   105,   106,   107,
     108,    54,   118,   120,    57,   115,   234,    94,    10,   473,
     474,   239,    10,   605,    10,     2,    56,   609,    71,   101,
     612,   485,     9,     2,    10,     4,    10,   619,   620,   621,
       9,    69,   113,   585,    10,   499,    15,    90,   266,   267,
     268,   269,   270,   271,   272,   273,    84,    85,   115,    10,
     278,   279,   116,   115,    10,    10,    10,   521,   118,    10,
     652,    71,   281,   704,   725,   103,   104,   105,   106,   107,
     108,   623,   560,    77,     7,    54,   540,   484,   595,   671,
     666,   595,   546,    -1,    -1,    62,   193,   194,    -1,   704,
      -1,    69,    -1,    -1,    -1,    -1,    -1,   561,   193,   194,
     328,   329,    -1,    -1,    -1,    -1,    84,    85,    -1,   701,
      -1,   339,   704,    -1,   706,   343,    -1,   581,   582,    -1,
      -1,   713,    -1,   101,   102,   103,   104,   105,   106,   107,
     108,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     193,   194,    -1,    -1,    -1,   609,    -1,    -1,   612,    -1,
      -1,    -1,    -1,   206,   382,   619,   620,   621,    -1,    -1,
     267,   268,   269,   270,    -1,   272,   273,    -1,    -1,    -1,
     210,    -1,   267,   268,   269,   270,    -1,   272,   273,   702,
      -1,   234,   222,    -1,   224,    -1,   239,   227,   652,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   671,    -1,    -1,
      -1,    -1,    -1,   266,   267,   268,   269,   270,   271,   272,
     273,    -1,   329,    -1,    -1,   278,   279,   206,    -1,    -1,
      -1,    -1,    -1,    -1,   329,    -1,   276,   701,    -1,    -1,
     704,   281,   706,    -1,    -1,   473,   474,   234,    -1,   713,
      -1,    -1,    -1,    -1,    -1,   234,    -1,   485,    -1,    -1,
     239,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   499,    -1,    -1,    -1,   328,   329,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   339,   266,    -1,    -1,
     343,    -1,   271,   521,    -1,   335,    -1,    -1,    -1,   278,
      -1,   341,    -1,    -1,    -1,   345,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   546,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   382,
      -1,    -1,   560,   561,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   328,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   328,
      -1,    -1,   339,   581,   582,    -1,   343,    -1,    -1,    -1,
     339,    -1,    -1,    -1,   343,    -1,   473,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   605,   473,    -1,
      -1,   609,    -1,    -1,   612,    -1,    -1,    -1,    -1,    -1,
      -1,   619,   620,   621,    -1,   382,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   382,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   521,    -1,    -1,    -1,    -1,    -1,
     473,   474,    -1,    -1,   652,    -1,   521,    -1,    -1,    -1,
      -1,    -1,   485,    -1,    -1,    -1,    -1,    -1,    -1,   546,
      -1,    -1,    -1,   671,    -1,    -1,   499,    -1,    -1,    -1,
      -1,   546,    -1,   560,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   521,    -1,
      -1,    -1,    -1,   701,    -1,    -1,    -1,    -1,   706,    20,
      21,    -1,    -1,    -1,    -1,   713,    -1,   474,    -1,    -1,
      -1,    -1,    -1,   546,    -1,   474,    -1,    -1,   485,    -1,
      -1,    -1,    -1,    -1,    -1,   612,   485,   560,   561,    -1,
      51,    52,   499,    -1,    55,    56,    -1,   612,    59,    60,
     499,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   581,   582,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   605,    -1,    -1,    -1,   609,    -1,    -1,   612,
      -1,    -1,    -1,    -1,    -1,    -1,   619,   620,   621,    -1,
      -1,    -1,    -1,    -1,   561,    -1,    -1,    -1,    -1,    -1,
      -1,   560,   561,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   581,   582,    -1,    -1,    -1,   652,
      -1,    -1,   581,   582,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   671,    -1,
      -1,    -1,   609,    -1,    -1,    -1,   605,    -1,    -1,    -1,
     609,    -1,   619,   620,   621,    -1,    -1,    -1,    -1,    -1,
     619,   620,   621,    -1,    -1,    55,    56,    -1,   701,    -1,
      -1,    -1,    -1,   706,    -1,    -1,    -1,    -1,    -1,    -1,
     713,    -1,    -1,    -1,    -1,   652,   207,   208,   209,   210,
      -1,    -1,    -1,   652,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   222,    -1,   224,   671,    -1,   227,    -1,    -1,    -1,
      -1,    -1,   671,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   243,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   701,    -1,    -1,    -1,    -1,   706,
      -1,    -1,   701,    -1,    -1,    -1,   713,   706,    -1,    -1,
      -1,    -1,    -1,    -1,   713,   276,    -1,    -1,    -1,    -1,
     281,   282,   283,   284,   285,   286,   287,   288,   289,   290,
     291,   292,   293,   294,   295,   296,   297,   298,   299,   300,
     301,   302,   303,   304,   305,   306,   307,    -1,    -1,   310,
      -1,    -1,   182,    -1,    -1,    -1,    -1,    -1,   319,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   335,    -1,    -1,    -1,    -1,    -1,
     341,   342,    -1,    -1,   345,   215,    -1,    -1,    -1,    -1,
     220,    -1,   353,    -1,   224,    -1,    -1,    -1,   359,   360,
      -1,    -1,    -1,    -1,   365,   366,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     381,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       0,   392,    -1,    -1,   395,    -1,   397,    -1,     8,     9,
      10,    -1,   403,    13,    14,    15,    -1,    17,    -1,    -1,
      -1,    -1,   413,    -1,    -1,    -1,    -1,    27,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    36,    37,    -1,    39,
      40,    41,    42,    43,    -1,    -1,    -1,    -1,    -1,    -1,
     310,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   335,   336,    -1,   469,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,    -1,
      -1,    -1,    -1,   353,    -1,   355,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   365,    -1,    -1,    -1,    -1,
      -1,    -1,   372,   113,   114,   115,   507,    -1,   118,    -1,
     120,   121,   122,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   526,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   403,   535,    -1,    -1,    -1,    -1,   540,
      -1,    -1,   412,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   552,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   564,    -1,   566,   567,    -1,    -1,   570,
     571,    -1,    -1,    -1,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    -1,   589,    84,
      85,   461,    -1,    -1,    -1,   465,   466,   467,   468,    -1,
      -1,    -1,    97,   604,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,    -1,    -1,    -1,   618,   488,    -1,
     490,   491,    -1,    -1,   625,    -1,   121,   497,    -1,    -1,
     500,    -1,   502,    -1,    -1,   505,    -1,    -1,    -1,    -1,
      -1,    -1,   643,    -1,    -1,    -1,    -1,    -1,   649,   519,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     530,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   677,    -1,    -1,    -1,
      -1,    -1,   683,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,
      -1,    -1,    16,   704,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    -1,   593,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    45,    -1,    -1,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    -1,   624,    -1,    -1,    -1,   628,    -1,
      -1,    -1,   632,   633,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    92,    93,
      -1,    95,    -1,    -1,    -1,    -1,    -1,   101,   658,    -1,
     660,   661,    -1,    -1,    -1,   665,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   678,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   696,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   704,   705,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   714,    -1,    -1,    -1,   718,   719,
      -1,    -1,    -1,    -1,    -1,   725,    -1,    -1,    -1,    -1,
     730,    -1,     0,     1,   734,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      38,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
       0,    -1,    90,    91,    92,    93,    -1,    95,     8,     9,
      10,    -1,    -1,    13,    14,    15,    -1,    17,    -1,    -1,
      -1,   109,   110,    -1,    -1,    -1,    -1,    27,    28,    -1,
      -1,    -1,    -1,   121,   122,    -1,    36,    37,    -1,    39,
      40,    41,    42,    43,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    -1,    -1,    84,    85,    86,    -1,    -1,    89,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,    -1,
      -1,    -1,    -1,   113,   114,   115,   116,   117,   118,     0,
      -1,   121,   122,    -1,    -1,    -1,    -1,     8,     9,    10,
      -1,    -1,    13,    14,    15,    -1,    17,    -1,    69,    70,
      71,    72,    73,    74,    75,    26,    27,    78,    79,    -1,
      -1,    -1,    -1,    84,    85,    36,    37,    -1,    39,    40,
      41,    42,    43,    -1,    -1,    -1,    -1,    -1,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    -1,    -1,    84,    85,    86,    -1,    88,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    96,    97,    -1,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,    -1,    -1,
      -1,   112,   113,   114,   115,   116,   117,    -1,    -1,    -1,
     121,   122,     1,    -1,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    -1,    -1,    15,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    38,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,
      -1,    90,    91,    92,    93,    -1,    95,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     109,   110,    -1,    -1,    -1,     1,    -1,     3,     4,     5,
       6,     7,   121,   122,    10,    11,    12,    -1,    14,    15,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    92,    93,    -1,    95,
      -1,     1,    -1,     3,     4,     5,     6,     7,    -1,    -1,
      10,    11,    12,   109,   110,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,   121,   122,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    38,    -1,
      -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    92,    93,    -1,    95,    -1,     1,    -1,     3,
       4,     5,     6,     7,    -1,    -1,    10,    11,    12,   109,
     110,    15,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,   121,   122,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    92,    93,
      -1,    95,     1,    -1,     3,     4,     5,     6,     7,    -1,
       9,    10,    11,    12,    -1,   109,   110,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,   121,   122,    -1,
      29,    30,    31,    32,    33,    34,    35,    -1,    -1,    38,
      -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,
      -1,    90,    91,    92,    93,    -1,    95,     1,    -1,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    11,    12,    -1,
     109,   110,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,   121,   122,    -1,    29,    30,    31,    32,    33,
      34,    35,    -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    87,    -1,    -1,    90,    91,    92,    93,
      -1,    95,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   109,   110,    -1,    -1,    -1,
       1,   115,     3,     4,     5,     6,     7,   121,   122,    -1,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    38,    -1,    -1,
      -1,    -1,    -1,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    93,    -1,    95,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   109,   110,
      -1,    -1,    -1,     1,   115,     3,     4,     5,     6,     7,
     121,   122,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    -1,    -1,
      38,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    87,
      -1,    -1,    90,    91,    92,    93,    -1,    95,    -1,     1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    10,    11,
      12,   109,   110,    -1,    16,   113,    18,    19,    20,    21,
      22,    23,    24,   121,   122,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    38,    -1,    -1,    -1,
      -1,    -1,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      92,    93,    -1,    95,    -1,    -1,     3,     4,     5,     6,
       7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,   121,
     122,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    38,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    92,    93,    -1,    95,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,   109,   110,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,   122,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    38,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      92,    93,    -1,    95,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   109,   110,    -1,
      -1,     3,     4,     5,     6,     7,     8,     9,    10,   121,
     122,    13,    14,    15,    16,    17,    -1,    -1,    20,    21,
      22,    23,    24,    25,    26,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    67,    68,    69,    70,    71,
      72,    -1,    74,    75,    -1,    -1,    78,    -1,    -1,    -1,
      82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    93,    -1,    -1,    -1,    -1,    -1,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,    -1,   110,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,   119,   120,    11,
      12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    26,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      92,    93,    94,    95,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,    16,
     112,    18,    19,    20,    21,    22,    23,    24,   120,    26,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    92,    93,    94,    95,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,   109,   110,    16,   112,    18,    19,    20,    21,
      22,    23,    24,   120,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    88,    -1,    90,    91,
      92,    93,    94,    95,    96,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,   120,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    92,    93,    94,    95,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,   109,   110,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,   120,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      92,    93,    94,    95,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,   120,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    92,    93,    94,    95,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,   109,   110,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,   120,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      92,    93,    94,    95,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,   120,    -1,
      -1,    -1,    29,    30,    31,    32,    33,    34,    35,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,    -1,
      -1,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      87,    -1,    -1,    90,    91,    92,    93,    94,    95,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,   109,   110,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,   120,    -1,    -1,    -1,    29,    30,    31,
      32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,
      92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   109,   110,     3,
       4,     5,     6,     7,     8,     9,    10,    -1,   120,    13,
      14,    15,    16,    17,    -1,    -1,    20,    21,    22,    23,
      24,    25,    26,    -1,    -1,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    67,    68,    69,    70,    71,    72,    -1,
      74,    75,    -1,    -1,    78,    -1,    -1,    -1,    82,    83,
      84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    93,
      -1,    -1,    -1,    -1,    -1,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,    -1,   110,     3,     4,     5,
       6,     7,     8,     9,    10,   119,    -1,    13,    14,    15,
      16,    17,    -1,    -1,    20,    21,    22,    23,    24,    25,
      26,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    -1,    54,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    67,    68,    69,    70,    71,    72,    -1,    74,    75,
      -1,    -1,    78,    -1,    -1,    -1,    82,    83,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    93,    -1,    95,
      -1,    -1,    -1,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,    -1,   110,     3,     4,     5,     6,     7,
       8,     9,    10,   119,    -1,    13,    14,    15,    16,    17,
      -1,    -1,    20,    21,    22,    23,    24,    25,    26,    -1,
      -1,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    -1,    -1,    54,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,
      68,    69,    70,    71,    72,    -1,    74,    75,    -1,    -1,
      78,    -1,    -1,    -1,    82,    83,    84,    85,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    93,    -1,    95,    -1,    -1,
      -1,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,    -1,   110,     3,     4,     5,     6,     7,     8,     9,
      10,   119,    -1,    13,    14,    15,    16,    17,    -1,    -1,
      20,    21,    22,    23,    24,    25,    26,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    -1,    -1,    54,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,    68,    69,
      70,    71,    72,    -1,    74,    75,    -1,    -1,    78,    -1,
      -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    93,    -1,    -1,    -1,    -1,    -1,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,    -1,
     110,    -1,     3,     4,     5,    -1,     7,    -1,    -1,   119,
      11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,   109,   110,
      11,    12,    -1,    -1,   115,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    93,    94,    95,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,     6,     7,    -1,   109,   110,
      11,    12,    -1,    -1,   115,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    38,    -1,    -1,
      -1,    -1,    -1,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    93,    -1,    95,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    38,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    92,    93,    -1,    95,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,   109,   110,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    93,    94,    95,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    92,    93,    94,    95,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,   109,   110,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    93,    94,    95,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    92,    93,    94,    95,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,   109,   110,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    93,    -1,    95,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    92,    93,    -1,    95,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,   109,   110,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    93,    -1,    95,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    92,    93,    -1,    95,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,   109,   110,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    93,    -1,    95,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    92,    93,    -1,    95,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,   109,   110,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    93,    -1,    95,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    92,    -1,    -1,    95,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,   109,   110,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,    30,
      31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    90,
      91,    92,    -1,    -1,    95,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,    -1,   109,   110,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    29,    30,    31,    32,    33,    34,    35,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    90,    91,    92,    93,    -1,    95,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    29,    30,    31,    32,
      33,    34,    35,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    45,    -1,    -1,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    -1,    -1,    90,    91,    92,
      93,    -1,    95,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    29,
      30,    31,    32,    33,    34,    35,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    45,    -1,    -1,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    50,
      51,    -1,    -1,    54,    -1,    -1,    -1,    87,    -1,    -1,
      90,    91,    92,    -1,    -1,    95,    67,    68,    69,    70,
      71,    72,    -1,    74,    75,    -1,    -1,    78,    -1,    -1,
      -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    93,    -1,    -1,    -1,    -1,    -1,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,    -1,   110,
      50,    51,    -1,    -1,    54,    -1,    -1,    -1,   119,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,    68,    69,
      70,    71,    72,    -1,    74,    75,    -1,    -1,    78,    -1,
      -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    93,    -1,    -1,    -1,    -1,    -1,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,    -1,
     110,    50,    51,    -1,    -1,    54,    -1,    -1,    -1,   119,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,    68,
      69,    70,    71,    72,    -1,    74,    75,    -1,    -1,    78,
      -1,    -1,    -1,    82,    83,    84,    85,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    93,    -1,    -1,    -1,    -1,    -1,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
      -1,   110,    50,    51,    -1,    -1,    54,    -1,    -1,    -1,
     119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    67,
      68,    69,    70,    71,    72,    -1,    74,    75,    -1,    -1,
      78,    -1,    -1,    -1,    82,    83,    84,    85,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    93,    -1,    -1,    -1,    -1,
      -1,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,    -1,   110,    50,    51,    -1,    -1,    54,    -1,    -1,
      -1,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      67,    68,    69,    70,    71,    72,    -1,    74,    75,    -1,
      -1,    78,    -1,    -1,    -1,    82,    83,    84,    85,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    93,    -1,    -1,    -1,
      -1,    -1,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,    -1,   110,    50,    51,    -1,    -1,    54,    -1,
      -1,    -1,   119,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    67,    68,    69,    70,    71,    72,    -1,    74,    75,
      -1,    -1,    78,    -1,    -1,    -1,    82,    83,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    93,    -1,    -1,
      -1,    -1,    -1,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,    -1,   110,    50,    51,    -1,    -1,    54,
      -1,    -1,    -1,   119,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    67,    68,    69,    70,    71,    72,    -1,    74,
      75,    -1,    -1,    78,    -1,    -1,    -1,    82,    83,    84,
      85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    93,    -1,
      -1,    -1,    -1,    -1,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,    -1,   110,    50,    51,    -1,    -1,
      54,    -1,    -1,    -1,   119,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    67,    68,    69,    70,    71,    72,    -1,
      74,    75,    -1,    -1,    78,    -1,    -1,    -1,    82,    83,
      84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    93,
      -1,    -1,    -1,    -1,    -1,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,    -1,   110,    50,    51,    -1,
      -1,    54,    -1,    -1,    -1,   119,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    67,    68,    69,    70,    71,    72,
      -1,    74,    75,    -1,    -1,    78,    -1,    -1,    -1,    82,
      83,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      93,    -1,    -1,    -1,    -1,    -1,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,    -1,   110,    50,    51,
      -1,    -1,    54,    -1,    -1,    -1,   119,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    67,    68,    69,    70,    71,
      72,    -1,    74,    75,    -1,    -1,    78,    -1,    -1,    -1,
      82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    93,    -1,    -1,    -1,    -1,    -1,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,    -1,   110,    50,
      51,    -1,    -1,    54,    -1,    -1,    -1,   119,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    67,    68,    69,    70,
      71,    72,    -1,    74,    75,    -1,    -1,    78,    -1,    -1,
      -1,    82,    83,    84,    85,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    93,    -1,    -1,    -1,    -1,    -1,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,    -1,   110,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   119,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    -1,    -1,    84,    85,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    97,    -1,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   116,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      -1,    -1,    84,    85,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    97,    -1,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   116,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    -1,    -1,
      84,    85,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    97,    -1,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   116,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    -1,    -1,    84,    85,
      -1,    -1,    -1,    89,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    97,    -1,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    -1,    -1,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    -1,    -1,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    97,    -1,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    -1,    -1,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,    69,    70,    71,    72,    73,    74,    75,
      76,    -1,    78,    79,    -1,    -1,    -1,    -1,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,    69,    70,    71,    72,    73,    74,    75,
      -1,    -1,    78,    79,    -1,    -1,    -1,    -1,    84,    85,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,   124,   125,     0,     1,     3,     4,     5,     6,     7,
      11,    12,    16,    18,    19,    20,    21,    22,    23,    24,
      29,    30,    31,    32,    33,    34,    35,    38,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    87,    90,    91,    92,    93,    95,   109,
     110,   126,   127,   128,   131,   132,   133,   134,   135,   137,
     138,   139,   140,   141,   149,   164,   187,   188,   199,   200,
     201,   203,   204,   205,   206,   223,   232,   128,    50,    54,
      84,   142,   142,     3,     4,     5,     6,     7,     8,     9,
      10,    13,    14,    15,    16,    17,    20,    21,    22,    23,
      24,    25,    26,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    54,    67,    68,    69,
      70,    71,    72,    74,    75,    78,    82,    83,    84,    85,
      93,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   110,   119,   120,   143,   147,   148,   204,   205,   218,
      32,    33,    34,    35,    48,    49,    50,    54,   143,   144,
     145,   201,   126,   131,   135,   141,   131,   121,   122,   131,
     227,   230,   231,   166,   168,    29,    30,    31,    45,   135,
     141,   164,   183,   204,   206,   223,    90,    93,    94,   109,
     120,   134,   149,   155,   158,   160,   163,   164,   204,   206,
     221,   222,   120,   163,   120,   153,   156,   157,   131,    52,
     144,   121,   228,   130,   112,   149,   164,   149,   142,    90,
     126,   136,   137,    93,   132,   152,   160,   221,   232,   160,
     220,   221,   232,   140,   164,   204,   206,    52,    53,    55,
     143,   202,   132,   149,   149,   227,   231,    39,    40,    41,
      42,    43,    36,    37,    28,   185,    96,   116,    90,    93,
     138,    96,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    84,    85,    97,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,    86,   114,
     117,    86,   114,    26,   112,   189,    58,    61,    88,    88,
     153,   156,   189,   131,     1,   100,   207,   230,   175,   219,
     176,    86,   114,   226,   116,     8,   197,   232,    13,   179,
     230,    96,    96,   179,   227,    17,   192,   122,   131,   131,
     228,    86,   114,   117,    25,   149,   149,   115,   163,    89,
     116,   159,   232,    86,   114,   117,   116,   159,   115,   163,
     134,   155,   160,   187,   232,   155,    52,    64,    65,   129,
     120,   151,   112,   126,    86,   114,   136,   115,   115,   149,
     228,   118,   116,   121,   229,   116,   229,   116,   229,   113,
     229,    86,   114,   117,   128,   131,   131,   131,   131,   128,
     131,   131,   186,    93,   132,   149,   160,   161,   162,   136,
     140,   116,   132,   149,   162,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,   149,   149,   149,   149,   149,   149,   149,   149,   149,
     149,    50,    51,    54,   147,   224,   225,    50,    51,    54,
     147,   224,   152,    50,    54,   224,   224,   191,   190,   150,
     149,   173,   230,   208,   172,   126,   131,    50,    52,    53,
      54,    55,    93,    94,   120,   209,   210,   211,   212,   213,
     214,   215,   216,   177,   146,   149,   160,   195,   232,    15,
     182,   232,   126,    13,   149,   126,   192,    93,   160,   193,
      10,    27,   180,   230,   180,    50,   224,    50,    54,   224,
     152,   170,   116,   159,   115,   149,    93,   134,   149,   158,
     221,    50,    50,    54,   152,    93,   149,   222,   115,   228,
     116,    86,   114,   228,   115,   144,   165,   149,   126,   113,
     115,   228,    93,   132,   149,    50,    50,    54,   152,    77,
     101,   184,   232,   149,   116,   115,    98,    88,   153,   156,
      88,    88,   153,   154,   156,   232,   118,   154,   156,   154,
     156,   184,   184,   149,   230,   131,   126,    10,   228,    96,
      50,    50,   210,   126,   230,   116,   217,   232,   116,   217,
     116,   217,   143,   144,   116,    89,   196,   232,   126,     9,
     198,   232,    14,   181,   182,   182,    10,   149,   116,   179,
     167,   169,   118,   131,   149,   116,   159,   118,   149,   115,
     134,   187,   224,   224,   115,   131,   113,   149,   228,   118,
     101,   183,   126,    93,   149,   149,   149,   149,   149,    88,
     126,   126,   174,   230,    10,   115,   149,   228,   197,   211,
     214,   215,   216,    50,   213,   215,   178,   141,   164,   204,
     206,   179,   126,    10,   131,    10,    10,    93,   126,   126,
     126,   180,   159,    93,   159,   228,   115,   228,   101,    10,
     149,   149,    10,   113,   126,   115,   182,   116,   217,   217,
     217,   209,    86,   114,   117,   126,   179,   149,   182,   192,
     194,    10,    10,   171,   149,   115,    10,   198,   215,   126,
      50,    50,    54,   152,   197,   126,   126,   159,    10,   217,
     197,   118,   181,    10,   182,   198,    10
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
#define YYEMPTY		-2
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
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)           \
  Current.first_line   = Rhs[1].first_line;      \
  Current.first_column = Rhs[1].first_column;    \
  Current.last_line    = Rhs[N].last_line;       \
  Current.last_column  = Rhs[N].last_column;
#endif

/* YYLEX -- calling `yylex' with the right arguments.  */

#define YYLEX	yylex ()

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
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
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
/*-----------------------------.
| Print this symbol on YYOUT.  |
`-----------------------------*/

static void
#if defined (__STDC__) || defined (__cplusplus)
yysymprint (FILE* yyout, int yytype, YYSTYPE yyvalue)
#else
yysymprint (yyout, yytype, yyvalue)
    FILE* yyout;
    int yytype;
    YYSTYPE yyvalue;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvalue;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyout, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyout, yytoknum[yytype], yyvalue);
# endif
    }
  else
    YYFPRINTF (yyout, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyout, ")");
}
#endif /* YYDEBUG. */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
#if defined (__STDC__) || defined (__cplusplus)
yydestruct (int yytype, YYSTYPE yyvalue)
#else
yydestruct (yytype, yyvalue)
    int yytype;
    YYSTYPE yyvalue;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvalue;

  switch (yytype)
    {
      default:
        break;
    }
}



/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
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
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of parse errors so far.  */
int yynerrs;


int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yychar1 = 0;

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

  if (yyssp >= yyss + yystacksize - 1)
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
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
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

      if (yyssp >= yyss + yystacksize - 1)
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

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with.  */

  if (yychar <= 0)		/* This means end of input.  */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more.  */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

      /* We have to keep this `#if YYDEBUG', since we use variables
	 which are defined only if `YYDEBUG' is set.  */
      YYDPRINTF ((stderr, "Next token is "));
      YYDSYMPRINT ((stderr, yychar1, yylval));
      YYDPRINTF ((stderr, "\n"));
    }

  /* If the proper action on seeing token YYCHAR1 is to reduce or to
     detect an error, take that action.  */
  yyn += yychar1;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yychar1)
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
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

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



#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn - 1, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] >= 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif
  switch (yyn)
    {
        case 2:
#line 267 "parse.y"
    {
		        yyval.vars = ruby_dyna_vars;
			lex_state = EXPR_BEG;
                        top_local_init();
			if ((VALUE)ruby_class == rb_cObject) class_nest = 0;
			else class_nest = 1;
		    }
    break;

  case 3:
#line 275 "parse.y"
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
		        ruby_dyna_vars = yyvsp[-1].vars;
		    }
    break;

  case 4:
#line 295 "parse.y"
    {
			void_stmts(yyvsp[-1].node);
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 6:
#line 303 "parse.y"
    {
			yyval.node = newline_node(yyvsp[0].node);
		    }
    break;

  case 7:
#line 307 "parse.y"
    {
			yyval.node = block_append(yyvsp[-2].node, newline_node(yyvsp[0].node));
		    }
    break;

  case 8:
#line 311 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 9:
#line 316 "parse.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 10:
#line 317 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("alias within method");
		        yyval.node = NEW_ALIAS(yyvsp[-2].id, yyvsp[0].id);
		    }
    break;

  case 11:
#line 323 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("alias within method");
		        yyval.node = NEW_VALIAS(yyvsp[-1].id, yyvsp[0].id);
		    }
    break;

  case 12:
#line 329 "parse.y"
    {
			char buf[3];

			if (in_def || in_single)
			    yyerror("alias within method");
			sprintf(buf, "$%c", (int)yyvsp[0].node->nd_nth);
		        yyval.node = NEW_VALIAS(yyvsp[-1].id, rb_intern(buf));
		    }
    break;

  case 13:
#line 338 "parse.y"
    {
		        yyerror("can't make alias for the number variables");
		        yyval.node = 0;
		    }
    break;

  case 14:
#line 343 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("undef within method");
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 15:
#line 349 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = NEW_IF(cond(yyvsp[0].node), yyvsp[-2].node, 0);
		        fixpos(yyval.node, yyvsp[0].node);
		    }
    break;

  case 16:
#line 355 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = NEW_UNLESS(cond(yyvsp[0].node), yyvsp[-2].node, 0);
		        fixpos(yyval.node, yyvsp[0].node);
		    }
    break;

  case 17:
#line 361 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-2].node && nd_type(yyvsp[-2].node) == NODE_BEGIN) {
			    yyval.node = NEW_WHILE(cond(yyvsp[0].node), yyvsp[-2].node->nd_body, 0);
			}
			else {
			    yyval.node = NEW_WHILE(cond(yyvsp[0].node), yyvsp[-2].node, 1);
			}
		    }
    break;

  case 18:
#line 371 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-2].node && nd_type(yyvsp[-2].node) == NODE_BEGIN) {
			    yyval.node = NEW_UNTIL(cond(yyvsp[0].node), yyvsp[-2].node->nd_body, 0);
			}
			else {
			    yyval.node = NEW_UNTIL(cond(yyvsp[0].node), yyvsp[-2].node, 1);
			}
		    }
    break;

  case 19:
#line 381 "parse.y"
    {
			yyval.node = NEW_RESCUE(yyvsp[-2].node, NEW_RESBODY(0,yyvsp[0].node,0), 0);
		    }
    break;

  case 20:
#line 385 "parse.y"
    {
			if (in_def || in_single) {
			    yyerror("BEGIN in method");
			}
			local_push(0);
		    }
    break;

  case 21:
#line 392 "parse.y"
    {
			ruby_eval_tree_begin = block_append(ruby_eval_tree_begin,
						            NEW_PREEXE(yyvsp[-1].node));
		        local_pop();
		        yyval.node = 0;
		    }
    break;

  case 22:
#line 399 "parse.y"
    {
			if (compile_for_eval && (in_def || in_single)) {
			    yyerror("END in method; use at_exit");
			}

			yyval.node = NEW_ITER(0, NEW_POSTEXE(), yyvsp[-1].node);
		    }
    break;

  case 23:
#line 407 "parse.y"
    {
			yyval.node = node_assign(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 24:
#line 411 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyvsp[-2].node->nd_value = yyvsp[0].node;
			yyval.node = yyvsp[-2].node;
		    }
    break;

  case 25:
#line 417 "parse.y"
    {
			yyval.node = node_assign(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 27:
#line 424 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyvsp[-2].node->nd_value = yyvsp[0].node;
			yyval.node = yyvsp[-2].node;
		    }
    break;

  case 28:
#line 430 "parse.y"
    {
			if (!compile_for_eval && !in_def && !in_single)
			    yyerror("return appeared outside of method");
			yyval.node = NEW_RETURN(yyvsp[0].node);
		    }
    break;

  case 30:
#line 437 "parse.y"
    {
			yyval.node = logop(NODE_AND, yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 31:
#line 441 "parse.y"
    {
			yyval.node = logop(NODE_OR, yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 32:
#line 445 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = NEW_NOT(cond(yyvsp[0].node));
		    }
    break;

  case 33:
#line 450 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = NEW_NOT(cond(yyvsp[0].node));
		    }
    break;

  case 38:
#line 463 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		    }
    break;

  case 39:
#line 468 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		    }
    break;

  case 40:
#line 475 "parse.y"
    {
			yyval.node = new_fcall(yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[0].node);
		   }
    break;

  case 41:
#line 480 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 42:
#line 486 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 43:
#line 492 "parse.y"
    {
			if (!compile_for_eval && !in_def && !in_single)
			    yyerror("super called outside of method");
			yyval.node = new_super(yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[0].node);
		    }
    break;

  case 44:
#line 499 "parse.y"
    {
			yyval.node = NEW_YIELD(yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[0].node);
		    }
    break;

  case 46:
#line 507 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 48:
#line 514 "parse.y"
    {
			yyval.node = NEW_MASGN(NEW_LIST(yyvsp[-1].node), 0);
		    }
    break;

  case 49:
#line 520 "parse.y"
    {
			yyval.node = NEW_MASGN(yyvsp[0].node, 0);
		    }
    break;

  case 50:
#line 524 "parse.y"
    {
			yyval.node = NEW_MASGN(list_append(yyvsp[-1].node,yyvsp[0].node), 0);
		    }
    break;

  case 51:
#line 528 "parse.y"
    {
			yyval.node = NEW_MASGN(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 52:
#line 532 "parse.y"
    {
			yyval.node = NEW_MASGN(yyvsp[-1].node, -1);
		    }
    break;

  case 53:
#line 536 "parse.y"
    {
			yyval.node = NEW_MASGN(0, yyvsp[0].node);
		    }
    break;

  case 54:
#line 540 "parse.y"
    {
			yyval.node = NEW_MASGN(0, -1);
		    }
    break;

  case 56:
#line 547 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 57:
#line 553 "parse.y"
    {
			yyval.node = NEW_LIST(yyvsp[-1].node);
		    }
    break;

  case 58:
#line 557 "parse.y"
    {
			yyval.node = list_append(yyvsp[-2].node, yyvsp[-1].node);
		    }
    break;

  case 59:
#line 563 "parse.y"
    {
			yyval.node = assignable(yyvsp[0].id, 0);
		    }
    break;

  case 60:
#line 567 "parse.y"
    {
			yyval.node = aryset(yyvsp[-3].node, yyvsp[-1].node);
		    }
    break;

  case 61:
#line 571 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 62:
#line 575 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 63:
#line 579 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 64:
#line 583 "parse.y"
    {
		        rb_backref_error(yyvsp[0].node);
			yyval.node = 0;
		    }
    break;

  case 65:
#line 590 "parse.y"
    {
			yyval.node = assignable(yyvsp[0].id, 0);
		    }
    break;

  case 66:
#line 594 "parse.y"
    {
			yyval.node = aryset(yyvsp[-3].node, yyvsp[-1].node);
		    }
    break;

  case 67:
#line 598 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 68:
#line 602 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 69:
#line 606 "parse.y"
    {
			yyval.node = attrset(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 70:
#line 610 "parse.y"
    {
		        rb_backref_error(yyvsp[0].node);
			yyval.node = 0;
		    }
    break;

  case 71:
#line 617 "parse.y"
    {
			yyerror("class/module name must be CONSTANT");
		    }
    break;

  case 76:
#line 627 "parse.y"
    {
			lex_state = EXPR_END;
			yyval.id = yyvsp[0].id;
		    }
    break;

  case 77:
#line 632 "parse.y"
    {
			lex_state = EXPR_END;
			yyval.id = yyvsp[0].id;
		    }
    break;

  case 80:
#line 643 "parse.y"
    {
			yyval.node = NEW_UNDEF(yyvsp[0].id);
		    }
    break;

  case 81:
#line 646 "parse.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 82:
#line 647 "parse.y"
    {
			yyval.node = block_append(yyvsp[-3].node, NEW_UNDEF(yyvsp[0].id));
		    }
    break;

  case 83:
#line 652 "parse.y"
    { yyval.id = '|'; }
    break;

  case 84:
#line 653 "parse.y"
    { yyval.id = '^'; }
    break;

  case 85:
#line 654 "parse.y"
    { yyval.id = '&'; }
    break;

  case 86:
#line 655 "parse.y"
    { yyval.id = tCMP; }
    break;

  case 87:
#line 656 "parse.y"
    { yyval.id = tEQ; }
    break;

  case 88:
#line 657 "parse.y"
    { yyval.id = tEQQ; }
    break;

  case 89:
#line 658 "parse.y"
    { yyval.id = tMATCH; }
    break;

  case 90:
#line 659 "parse.y"
    { yyval.id = '>'; }
    break;

  case 91:
#line 660 "parse.y"
    { yyval.id = tGEQ; }
    break;

  case 92:
#line 661 "parse.y"
    { yyval.id = '<'; }
    break;

  case 93:
#line 662 "parse.y"
    { yyval.id = tLEQ; }
    break;

  case 94:
#line 663 "parse.y"
    { yyval.id = tLSHFT; }
    break;

  case 95:
#line 664 "parse.y"
    { yyval.id = tRSHFT; }
    break;

  case 96:
#line 665 "parse.y"
    { yyval.id = '+'; }
    break;

  case 97:
#line 666 "parse.y"
    { yyval.id = '-'; }
    break;

  case 98:
#line 667 "parse.y"
    { yyval.id = '*'; }
    break;

  case 99:
#line 668 "parse.y"
    { yyval.id = '*'; }
    break;

  case 100:
#line 669 "parse.y"
    { yyval.id = '/'; }
    break;

  case 101:
#line 670 "parse.y"
    { yyval.id = '%'; }
    break;

  case 102:
#line 671 "parse.y"
    { yyval.id = tPOW; }
    break;

  case 103:
#line 672 "parse.y"
    { yyval.id = '~'; }
    break;

  case 104:
#line 673 "parse.y"
    { yyval.id = tUPLUS; }
    break;

  case 105:
#line 674 "parse.y"
    { yyval.id = tUMINUS; }
    break;

  case 106:
#line 675 "parse.y"
    { yyval.id = tAREF; }
    break;

  case 107:
#line 676 "parse.y"
    { yyval.id = tASET; }
    break;

  case 108:
#line 677 "parse.y"
    { yyval.id = '`'; }
    break;

  case 150:
#line 690 "parse.y"
    {
			yyval.node = node_assign(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 151:
#line 693 "parse.y"
    {yyval.node = assignable(yyvsp[-1].id, 0);}
    break;

  case 152:
#line 694 "parse.y"
    {
			value_expr(yyvsp[0].node);
			if (yyvsp[-1].node) {
			    if (yyvsp[-2].id == tOROP) {
				yyvsp[-1].node->nd_value = yyvsp[0].node;
				yyval.node = NEW_OP_ASGN_OR(gettable(yyvsp[-3].id), yyvsp[-1].node);
				if (is_instance_id(yyvsp[-3].id)) {
				    yyval.node->nd_aid = yyvsp[-3].id;
				}
			    }
			    else if (yyvsp[-2].id == tANDOP) {
				yyvsp[-1].node->nd_value = yyvsp[0].node;
				yyval.node = NEW_OP_ASGN_AND(gettable(yyvsp[-3].id), yyvsp[-1].node);
			    }
			    else {
				yyval.node = yyvsp[-1].node;
				yyval.node->nd_value = call_op(gettable(yyvsp[-3].id),yyvsp[-2].id,1,yyvsp[0].node);
			    }
			    fixpos(yyval.node, yyvsp[0].node);
			}
			else {
			    yyval.node = 0;
			}
		    }
    break;

  case 153:
#line 719 "parse.y"
    {
                        NODE *args = NEW_LIST(yyvsp[0].node);

			value_expr(yyvsp[0].node);
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

  case 154:
#line 735 "parse.y"
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

  case 155:
#line 747 "parse.y"
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

  case 156:
#line 759 "parse.y"
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

  case 157:
#line 771 "parse.y"
    {
		        rb_backref_error(yyvsp[-2].node);
			yyval.node = 0;
		    }
    break;

  case 158:
#line 776 "parse.y"
    {
			value_expr(yyvsp[-2].node);
			value_expr(yyvsp[0].node);
			yyval.node = NEW_DOT2(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 159:
#line 782 "parse.y"
    {
			value_expr(yyvsp[-2].node);
			value_expr(yyvsp[0].node);
			yyval.node = NEW_DOT3(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 160:
#line 788 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '+', 1, yyvsp[0].node);
		    }
    break;

  case 161:
#line 792 "parse.y"
    {
		        yyval.node = call_op(yyvsp[-2].node, '-', 1, yyvsp[0].node);
		    }
    break;

  case 162:
#line 796 "parse.y"
    {
		        yyval.node = call_op(yyvsp[-2].node, '*', 1, yyvsp[0].node);
		    }
    break;

  case 163:
#line 800 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '/', 1, yyvsp[0].node);
		    }
    break;

  case 164:
#line 804 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '%', 1, yyvsp[0].node);
		    }
    break;

  case 165:
#line 808 "parse.y"
    {
			int need_negate = Qfalse;

			if (yyvsp[-2].node && nd_type(yyvsp[-2].node) == NODE_LIT) {

			    switch (TYPE(yyvsp[-2].node->nd_lit)) {
			      case T_FIXNUM:
			      case T_FLOAT:
			      case T_BIGNUM:
				if (RTEST(rb_funcall(yyvsp[-2].node->nd_lit,'<',1,INT2FIX(0)))) {
				    yyvsp[-2].node->nd_lit = rb_funcall(yyvsp[-2].node->nd_lit,rb_intern("-@"),0,0);
				    need_negate = Qtrue;
				}
			      default:
				break;
			    }
			}
			yyval.node = call_op(yyvsp[-2].node, tPOW, 1, yyvsp[0].node);
			if (need_negate) {
			    yyval.node = call_op(yyval.node, tUMINUS, 0, 0);
			}
		    }
    break;

  case 166:
#line 831 "parse.y"
    {
			if (yyvsp[0].node && nd_type(yyvsp[0].node) == NODE_LIT) {
			    yyval.node = yyvsp[0].node;
			}
			else {
			    yyval.node = call_op(yyvsp[0].node, tUPLUS, 0, 0);
			}
		    }
    break;

  case 167:
#line 840 "parse.y"
    {
			if (yyvsp[0].node && nd_type(yyvsp[0].node) == NODE_LIT && FIXNUM_P(yyvsp[0].node->nd_lit)) {
			    long i = FIX2LONG(yyvsp[0].node->nd_lit);

			    yyvsp[0].node->nd_lit = INT2NUM(-i);
			    yyval.node = yyvsp[0].node;
			}
			else {
			    yyval.node = call_op(yyvsp[0].node, tUMINUS, 0, 0);
			}
		    }
    break;

  case 168:
#line 852 "parse.y"
    {
		        yyval.node = call_op(yyvsp[-2].node, '|', 1, yyvsp[0].node);
		    }
    break;

  case 169:
#line 856 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '^', 1, yyvsp[0].node);
		    }
    break;

  case 170:
#line 860 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '&', 1, yyvsp[0].node);
		    }
    break;

  case 171:
#line 864 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tCMP, 1, yyvsp[0].node);
		    }
    break;

  case 172:
#line 868 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '>', 1, yyvsp[0].node);
		    }
    break;

  case 173:
#line 872 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tGEQ, 1, yyvsp[0].node);
		    }
    break;

  case 174:
#line 876 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, '<', 1, yyvsp[0].node);
		    }
    break;

  case 175:
#line 880 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tLEQ, 1, yyvsp[0].node);
		    }
    break;

  case 176:
#line 884 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tEQ, 1, yyvsp[0].node);
		    }
    break;

  case 177:
#line 888 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tEQQ, 1, yyvsp[0].node);
		    }
    break;

  case 178:
#line 892 "parse.y"
    {
			yyval.node = NEW_NOT(call_op(yyvsp[-2].node, tEQ, 1, yyvsp[0].node));
		    }
    break;

  case 179:
#line 896 "parse.y"
    {
			yyval.node = match_gen(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 180:
#line 900 "parse.y"
    {
			yyval.node = NEW_NOT(match_gen(yyvsp[-2].node, yyvsp[0].node));
		    }
    break;

  case 181:
#line 904 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = NEW_NOT(cond(yyvsp[0].node));
		    }
    break;

  case 182:
#line 909 "parse.y"
    {
			yyval.node = call_op(yyvsp[0].node, '~', 0, 0);
		    }
    break;

  case 183:
#line 913 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tLSHFT, 1, yyvsp[0].node);
		    }
    break;

  case 184:
#line 917 "parse.y"
    {
			yyval.node = call_op(yyvsp[-2].node, tRSHFT, 1, yyvsp[0].node);
		    }
    break;

  case 185:
#line 921 "parse.y"
    {
			yyval.node = logop(NODE_AND, yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 186:
#line 925 "parse.y"
    {
			yyval.node = logop(NODE_OR, yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 187:
#line 928 "parse.y"
    {in_defined = 1;}
    break;

  case 188:
#line 929 "parse.y"
    {
		        in_defined = 0;
			yyval.node = NEW_DEFINED(yyvsp[0].node);
		    }
    break;

  case 189:
#line 934 "parse.y"
    {
			value_expr(yyvsp[-4].node);
			yyval.node = NEW_IF(cond(yyvsp[-4].node), yyvsp[-2].node, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 190:
#line 940 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 192:
#line 947 "parse.y"
    {
			yyval.node = NEW_LIST(yyvsp[-1].node);
		    }
    break;

  case 193:
#line 951 "parse.y"
    {
			yyval.node = list_append(yyvsp[-3].node, yyvsp[-1].node);
		    }
    break;

  case 194:
#line 955 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 195:
#line 959 "parse.y"
    {
			value_expr(yyvsp[-1].node);
			yyval.node = arg_concat(yyvsp[-4].node, yyvsp[-1].node);
		    }
    break;

  case 196:
#line 964 "parse.y"
    {
			yyval.node = NEW_LIST(NEW_HASH(yyvsp[-1].node));
		    }
    break;

  case 197:
#line 968 "parse.y"
    {
			value_expr(yyvsp[-1].node);
			yyval.node = NEW_RESTARGS(yyvsp[-1].node);
		    }
    break;

  case 198:
#line 975 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 199:
#line 979 "parse.y"
    {
			yyval.node = yyvsp[-2].node;
		    }
    break;

  case 200:
#line 983 "parse.y"
    {
			yyval.node = NEW_LIST(yyvsp[-2].node);
		    }
    break;

  case 201:
#line 987 "parse.y"
    {
			yyval.node = list_append(yyvsp[-4].node, yyvsp[-2].node);
		    }
    break;

  case 204:
#line 997 "parse.y"
    {
			yyval.node = NEW_LIST(yyvsp[0].node);
		    }
    break;

  case 205:
#line 1001 "parse.y"
    {
			yyval.node = list_append(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 206:
#line 1005 "parse.y"
    {
			yyval.node = arg_blk_pass(yyvsp[-1].node, yyvsp[0].node);
		    }
    break;

  case 207:
#line 1009 "parse.y"
    {
			value_expr(yyvsp[-1].node);
			yyval.node = arg_concat(yyvsp[-4].node, yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 208:
#line 1015 "parse.y"
    {
			yyval.node = NEW_LIST(NEW_HASH(yyvsp[-1].node));
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 209:
#line 1020 "parse.y"
    {
			value_expr(yyvsp[-1].node);
			yyval.node = arg_concat(NEW_LIST(NEW_HASH(yyvsp[-4].node)), yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 210:
#line 1026 "parse.y"
    {
			yyval.node = list_append(yyvsp[-3].node, NEW_HASH(yyvsp[-1].node));
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 211:
#line 1031 "parse.y"
    {
			value_expr(yyvsp[-1].node);
			yyval.node = arg_concat(list_append(yyvsp[-6].node, NEW_HASH(yyvsp[-4].node)), yyvsp[-1].node);
			yyval.node = arg_blk_pass(yyval.node, yyvsp[0].node);
		    }
    break;

  case 212:
#line 1037 "parse.y"
    {
			value_expr(yyvsp[-1].node);
			yyval.node = arg_blk_pass(NEW_RESTARGS(yyvsp[-1].node), yyvsp[0].node);
		    }
    break;

  case 214:
#line 1044 "parse.y"
    {CMDARG_PUSH;}
    break;

  case 215:
#line 1045 "parse.y"
    {
		        CMDARG_POP;
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 216:
#line 1052 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = NEW_BLOCK_PASS(yyvsp[0].node);
		    }
    break;

  case 217:
#line 1059 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 219:
#line 1066 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = NEW_LIST(yyvsp[0].node);
		    }
    break;

  case 220:
#line 1071 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = list_append(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 221:
#line 1078 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 223:
#line 1086 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = list_append(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 224:
#line 1091 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = arg_concat(yyvsp[-3].node, yyvsp[0].node);
		    }
    break;

  case 225:
#line 1096 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 226:
#line 1103 "parse.y"
    {
			yyval.node = yyvsp[0].node;
			if (yyvsp[0].node) {
			    if (nd_type(yyvsp[0].node) == NODE_ARRAY &&
				yyvsp[0].node->nd_next == 0) {
				yyval.node = yyvsp[0].node->nd_head;
			    }
			    else if (nd_type(yyvsp[0].node) == NODE_BLOCK_PASS) {
				rb_compile_error("block argument should not be given");
			    }
			}
		    }
    break;

  case 227:
#line 1118 "parse.y"
    {
			yyval.node = NEW_LIT(yyvsp[0].val);
		    }
    break;

  case 229:
#line 1123 "parse.y"
    {
			yyval.node = NEW_XSTR(yyvsp[0].val);
		    }
    break;

  case 235:
#line 1132 "parse.y"
    {
			yyval.node = NEW_VCALL(yyvsp[0].id);
		    }
    break;

  case 236:
#line 1141 "parse.y"
    {
			if (!yyvsp[-3].node && !yyvsp[-2].node && !yyvsp[-1].node)
			    yyval.node = NEW_BEGIN(yyvsp[-4].node);
			else {
			    if (yyvsp[-3].node) yyvsp[-4].node = NEW_RESCUE(yyvsp[-4].node, yyvsp[-3].node, yyvsp[-2].node);
			    else if (yyvsp[-2].node) {
				rb_warn("else without rescue is useless");
				yyvsp[-4].node = block_append(yyvsp[-4].node, yyvsp[-2].node);
			    }
			    if (yyvsp[-1].node) yyvsp[-4].node = NEW_ENSURE(yyvsp[-4].node, yyvsp[-1].node);
			    yyval.node = yyvsp[-4].node;
			}
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 237:
#line 1156 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 238:
#line 1160 "parse.y"
    {
			value_expr(yyvsp[-2].node);
			yyval.node = NEW_COLON2(yyvsp[-2].node, yyvsp[0].id);
		    }
    break;

  case 239:
#line 1165 "parse.y"
    {
			yyval.node = NEW_COLON3(yyvsp[0].id);
		    }
    break;

  case 240:
#line 1169 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = NEW_CALL(yyvsp[-3].node, tAREF, yyvsp[-1].node);
		    }
    break;

  case 241:
#line 1174 "parse.y"
    {
			if (yyvsp[-1].node == 0)
			    yyval.node = NEW_ZARRAY(); /* zero length array*/
			else {
			    yyval.node = yyvsp[-1].node;
			}
		    }
    break;

  case 242:
#line 1182 "parse.y"
    {
			yyval.node = NEW_HASH(yyvsp[-1].node);
		    }
    break;

  case 243:
#line 1186 "parse.y"
    {
			if (!compile_for_eval && !in_def && !in_single)
			    yyerror("return appeared outside of method");
			value_expr(yyvsp[-1].node);
			yyval.node = NEW_RETURN(yyvsp[-1].node);
		    }
    break;

  case 244:
#line 1193 "parse.y"
    {
			if (!compile_for_eval && !in_def && !in_single)
			    yyerror("return appeared outside of method");
			yyval.node = NEW_RETURN(0);
		    }
    break;

  case 245:
#line 1199 "parse.y"
    {
			if (!compile_for_eval && !in_def && !in_single)
			    yyerror("return appeared outside of method");
			yyval.node = NEW_RETURN(0);
		    }
    break;

  case 246:
#line 1205 "parse.y"
    {
			value_expr(yyvsp[-1].node);
			yyval.node = NEW_YIELD(yyvsp[-1].node);
		    }
    break;

  case 247:
#line 1210 "parse.y"
    {
			yyval.node = NEW_YIELD(0);
		    }
    break;

  case 248:
#line 1214 "parse.y"
    {
			yyval.node = NEW_YIELD(0);
		    }
    break;

  case 249:
#line 1217 "parse.y"
    {in_defined = 1;}
    break;

  case 250:
#line 1218 "parse.y"
    {
		        in_defined = 0;
			yyval.node = NEW_DEFINED(yyvsp[-1].node);
		    }
    break;

  case 251:
#line 1223 "parse.y"
    {
			yyvsp[0].node->nd_iter = NEW_FCALL(yyvsp[-1].id, 0);
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 253:
#line 1229 "parse.y"
    {
			if (yyvsp[-1].node && nd_type(yyvsp[-1].node) == NODE_BLOCK_PASS) {
			    rb_compile_error("both block arg and actual block given");
			}
			yyvsp[0].node->nd_iter = yyvsp[-1].node;
			yyval.node = yyvsp[0].node;
		        fixpos(yyval.node, yyvsp[-1].node);
		    }
    break;

  case 254:
#line 1241 "parse.y"
    {
			value_expr(yyvsp[-4].node);
			yyval.node = NEW_IF(cond(yyvsp[-4].node), yyvsp[-2].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 255:
#line 1250 "parse.y"
    {
			value_expr(yyvsp[-4].node);
			yyval.node = NEW_UNLESS(cond(yyvsp[-4].node), yyvsp[-2].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 256:
#line 1255 "parse.y"
    {COND_PUSH;}
    break;

  case 257:
#line 1255 "parse.y"
    {COND_POP;}
    break;

  case 258:
#line 1258 "parse.y"
    {
			value_expr(yyvsp[-4].node);
			yyval.node = NEW_WHILE(cond(yyvsp[-4].node), yyvsp[-1].node, 1);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 259:
#line 1263 "parse.y"
    {COND_PUSH;}
    break;

  case 260:
#line 1263 "parse.y"
    {COND_POP;}
    break;

  case 261:
#line 1266 "parse.y"
    {
			value_expr(yyvsp[-4].node);
			yyval.node = NEW_UNTIL(cond(yyvsp[-4].node), yyvsp[-1].node, 1);
		        fixpos(yyval.node, yyvsp[-4].node);
		    }
    break;

  case 262:
#line 1274 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = NEW_CASE(yyvsp[-3].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 263:
#line 1280 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 264:
#line 1283 "parse.y"
    {COND_PUSH;}
    break;

  case 265:
#line 1283 "parse.y"
    {COND_POP;}
    break;

  case 266:
#line 1286 "parse.y"
    {
			value_expr(yyvsp[-4].node);
			yyval.node = NEW_FOR(yyvsp[-7].node, yyvsp[-4].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-7].node);
		    }
    break;

  case 267:
#line 1292 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("class definition in method body");
			class_nest++;
			local_push(0);
		        yyval.num = ruby_sourceline;
		    }
    break;

  case 268:
#line 1301 "parse.y"
    {
		        yyval.node = NEW_CLASS(yyvsp[-4].id, yyvsp[-1].node, yyvsp[-3].node);
		        nd_set_line(yyval.node, yyvsp[-2].num);
		        local_pop();
			class_nest--;
		    }
    break;

  case 269:
#line 1308 "parse.y"
    {
			yyval.num = in_def;
		        in_def = 0;
		    }
    break;

  case 270:
#line 1313 "parse.y"
    {
		        yyval.num = in_single;
		        in_single = 0;
			class_nest++;
			local_push(0);
		    }
    break;

  case 271:
#line 1321 "parse.y"
    {
		        yyval.node = NEW_SCLASS(yyvsp[-5].node, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-5].node);
		        local_pop();
			class_nest--;
		        in_def = yyvsp[-4].num;
		        in_single = yyvsp[-2].num;
		    }
    break;

  case 272:
#line 1330 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("module definition in method body");
			class_nest++;
			local_push(0);
		        yyval.num = ruby_sourceline;
		    }
    break;

  case 273:
#line 1339 "parse.y"
    {
		        yyval.node = NEW_MODULE(yyvsp[-3].id, yyvsp[-1].node);
		        nd_set_line(yyval.node, yyvsp[-2].num);
		        local_pop();
			class_nest--;
		    }
    break;

  case 274:
#line 1346 "parse.y"
    {
			if (in_def || in_single)
			    yyerror("nested method definition");
			yyval.id = cur_mid;
			cur_mid = yyvsp[0].id;
			in_def++;
			local_push(0);
		    }
    break;

  case 275:
#line 1360 "parse.y"
    {
		        if (yyvsp[-3].node) yyvsp[-4].node = NEW_RESCUE(yyvsp[-4].node, yyvsp[-3].node, yyvsp[-2].node);
			else if (yyvsp[-2].node) {
			    rb_warn("else without rescue is useless");
			    yyvsp[-4].node = block_append(yyvsp[-4].node, yyvsp[-2].node);
			}
			if (yyvsp[-1].node) yyvsp[-4].node = NEW_ENSURE(yyvsp[-4].node, yyvsp[-1].node);

			yyval.node = NEW_DEFN(yyvsp[-7].id, yyvsp[-5].node, yyvsp[-4].node, NOEX_PRIVATE);
			if (is_attrset_id(yyvsp[-7].id)) yyval.node->nd_noex = NOEX_PUBLIC;
		        fixpos(yyval.node, yyvsp[-5].node);
		        local_pop();
			in_def--;
			cur_mid = yyvsp[-6].id;
		    }
    break;

  case 276:
#line 1375 "parse.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 277:
#line 1376 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			in_single++;
			local_push(0);
		        lex_state = EXPR_END; /* force for args */
		    }
    break;

  case 278:
#line 1388 "parse.y"
    {
		        if (yyvsp[-3].node) yyvsp[-4].node = NEW_RESCUE(yyvsp[-4].node, yyvsp[-3].node, yyvsp[-2].node);
			else if (yyvsp[-2].node) {
			    rb_warn("else without rescue is useless");
			    yyvsp[-4].node = block_append(yyvsp[-4].node, yyvsp[-2].node);
			}
			if (yyvsp[-1].node) yyvsp[-4].node = NEW_ENSURE(yyvsp[-4].node, yyvsp[-1].node);

			yyval.node = NEW_DEFS(yyvsp[-10].node, yyvsp[-7].id, yyvsp[-5].node, yyvsp[-4].node);
		        fixpos(yyval.node, yyvsp[-10].node);
		        local_pop();
			in_single--;
		    }
    break;

  case 279:
#line 1402 "parse.y"
    {
			yyval.node = NEW_BREAK();
		    }
    break;

  case 280:
#line 1406 "parse.y"
    {
			yyval.node = NEW_NEXT();
		    }
    break;

  case 281:
#line 1410 "parse.y"
    {
			yyval.node = NEW_REDO();
		    }
    break;

  case 282:
#line 1414 "parse.y"
    {
			yyval.node = NEW_RETRY();
		    }
    break;

  case 289:
#line 1432 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = NEW_IF(cond(yyvsp[-3].node), yyvsp[-1].node, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 291:
#line 1441 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 295:
#line 1452 "parse.y"
    {
			yyval.node = (NODE*)1;
		    }
    break;

  case 296:
#line 1456 "parse.y"
    {
			yyval.node = (NODE*)1;
		    }
    break;

  case 297:
#line 1460 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 298:
#line 1467 "parse.y"
    {
		        yyval.vars = dyna_push();
		    }
    break;

  case 299:
#line 1473 "parse.y"
    {
			yyval.node = NEW_ITER(yyvsp[-2].node, 0, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-2].node?yyvsp[-2].node:yyvsp[-1].node);
			dyna_pop(yyvsp[-3].vars);
		    }
    break;

  case 300:
#line 1481 "parse.y"
    {
			if (yyvsp[-1].node && nd_type(yyvsp[-1].node) == NODE_BLOCK_PASS) {
			    rb_compile_error("both block arg and actual block given");
			}
			yyvsp[0].node->nd_iter = yyvsp[-1].node;
			yyval.node = yyvsp[0].node;
		        fixpos(yyval.node, yyvsp[0].node);
		    }
    break;

  case 301:
#line 1490 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		    }
    break;

  case 302:
#line 1495 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		    }
    break;

  case 303:
#line 1502 "parse.y"
    {
			yyval.node = new_fcall(yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[0].node);
		    }
    break;

  case 304:
#line 1507 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 305:
#line 1513 "parse.y"
    {
			value_expr(yyvsp[-3].node);
			yyval.node = new_call(yyvsp[-3].node, yyvsp[-1].id, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-3].node);
		    }
    break;

  case 306:
#line 1519 "parse.y"
    {
			value_expr(yyvsp[-2].node);
			yyval.node = new_call(yyvsp[-2].node, yyvsp[0].id, 0);
		    }
    break;

  case 307:
#line 1524 "parse.y"
    {
			if (!compile_for_eval && !in_def &&
		            !in_single && !in_defined)
			    yyerror("super called outside of method");
			yyval.node = new_super(yyvsp[0].node);
		    }
    break;

  case 308:
#line 1531 "parse.y"
    {
			if (!compile_for_eval && !in_def &&
		            !in_single && !in_defined)
			    yyerror("super called outside of method");
			yyval.node = NEW_ZSUPER();
		    }
    break;

  case 309:
#line 1540 "parse.y"
    {
		        yyval.vars = dyna_push();
		    }
    break;

  case 310:
#line 1545 "parse.y"
    {
			yyval.node = NEW_ITER(yyvsp[-2].node, 0, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-1].node);
			dyna_pop(yyvsp[-3].vars);
		    }
    break;

  case 311:
#line 1551 "parse.y"
    {
		        yyval.vars = dyna_push();
		    }
    break;

  case 312:
#line 1556 "parse.y"
    {
			yyval.node = NEW_ITER(yyvsp[-2].node, 0, yyvsp[-1].node);
		        fixpos(yyval.node, yyvsp[-1].node);
			dyna_pop(yyvsp[-3].vars);
		    }
    break;

  case 313:
#line 1566 "parse.y"
    {
			yyval.node = NEW_WHEN(yyvsp[-3].node, yyvsp[-1].node, yyvsp[0].node);
		    }
    break;

  case 315:
#line 1573 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = list_append(yyvsp[-3].node, NEW_WHEN(yyvsp[0].node, 0, 0));
		    }
    break;

  case 316:
#line 1578 "parse.y"
    {
			value_expr(yyvsp[0].node);
			yyval.node = NEW_LIST(NEW_WHEN(yyvsp[0].node, 0, 0));
		    }
    break;

  case 321:
#line 1593 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 323:
#line 1602 "parse.y"
    {
		        if (yyvsp[-3].node) {
		            yyvsp[-3].node = node_assign(yyvsp[-3].node, NEW_GVAR(rb_intern("$!")));
			    yyvsp[-1].node = block_append(yyvsp[-3].node, yyvsp[-1].node);
			}
			yyval.node = NEW_RESBODY(yyvsp[-4].node, yyvsp[-1].node, yyvsp[0].node);
		        fixpos(yyval.node, yyvsp[-4].node?yyvsp[-4].node:yyvsp[-1].node);
		    }
    break;

  case 326:
#line 1615 "parse.y"
    {
			if (yyvsp[0].node)
			    yyval.node = yyvsp[0].node;
			else
			    /* place holder */
			    yyval.node = NEW_NIL();
		    }
    break;

  case 328:
#line 1626 "parse.y"
    {
			yyval.val = ID2SYM(yyvsp[0].id);
		    }
    break;

  case 330:
#line 1633 "parse.y"
    {
			yyval.node = NEW_STR(yyvsp[0].val);
		    }
    break;

  case 332:
#line 1638 "parse.y"
    {
		        if (nd_type(yyvsp[-1].node) == NODE_DSTR) {
			    list_append(yyvsp[-1].node, NEW_STR(yyvsp[0].val));
			}
			else {
			    rb_str_concat(yyvsp[-1].node->nd_lit, yyvsp[0].val);
			}
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 333:
#line 1648 "parse.y"
    {
		        if (nd_type(yyvsp[-1].node) == NODE_STR) {
			    yyval.node = NEW_DSTR(yyvsp[-1].node->nd_lit);
			}
			else {
			    yyval.node = yyvsp[-1].node;
			}
			yyvsp[0].node->nd_head = NEW_STR(yyvsp[0].node->nd_lit);
			nd_set_type(yyvsp[0].node, NODE_ARRAY);
			list_concat(yyval.node, yyvsp[0].node);
		    }
    break;

  case 334:
#line 1662 "parse.y"
    {
		        lex_state = EXPR_END;
			yyval.id = yyvsp[0].id;
		    }
    break;

  case 346:
#line 1683 "parse.y"
    {yyval.id = kNIL;}
    break;

  case 347:
#line 1684 "parse.y"
    {yyval.id = kSELF;}
    break;

  case 348:
#line 1685 "parse.y"
    {yyval.id = kTRUE;}
    break;

  case 349:
#line 1686 "parse.y"
    {yyval.id = kFALSE;}
    break;

  case 350:
#line 1687 "parse.y"
    {yyval.id = k__FILE__;}
    break;

  case 351:
#line 1688 "parse.y"
    {yyval.id = k__LINE__;}
    break;

  case 352:
#line 1692 "parse.y"
    {
			yyval.node = gettable(yyvsp[0].id);
		    }
    break;

  case 355:
#line 1702 "parse.y"
    {
			yyval.node = 0;
		    }
    break;

  case 356:
#line 1706 "parse.y"
    {
			lex_state = EXPR_BEG;
		    }
    break;

  case 357:
#line 1710 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 358:
#line 1713 "parse.y"
    {yyerrok; yyval.node = 0;}
    break;

  case 359:
#line 1717 "parse.y"
    {
			yyval.node = yyvsp[-2].node;
			lex_state = EXPR_BEG;
		    }
    break;

  case 360:
#line 1722 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 361:
#line 1728 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(yyvsp[-5].num, yyvsp[-3].node, yyvsp[-1].id), yyvsp[0].node);
		    }
    break;

  case 362:
#line 1732 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(yyvsp[-3].num, yyvsp[-1].node, -1), yyvsp[0].node);
		    }
    break;

  case 363:
#line 1736 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(yyvsp[-3].num, 0, yyvsp[-1].id), yyvsp[0].node);
		    }
    break;

  case 364:
#line 1740 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(yyvsp[-1].num, 0, -1), yyvsp[0].node);
		    }
    break;

  case 365:
#line 1744 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(0, yyvsp[-3].node, yyvsp[-1].id), yyvsp[0].node);
		    }
    break;

  case 366:
#line 1748 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(0, yyvsp[-1].node, -1), yyvsp[0].node);
		    }
    break;

  case 367:
#line 1752 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(0, 0, yyvsp[-1].id), yyvsp[0].node);
		    }
    break;

  case 368:
#line 1756 "parse.y"
    {
			yyval.node = block_append(NEW_ARGS(0, 0, -1), yyvsp[0].node);
		    }
    break;

  case 369:
#line 1760 "parse.y"
    {
			yyval.node = NEW_ARGS(0, 0, -1);
		    }
    break;

  case 370:
#line 1766 "parse.y"
    {
			yyerror("formal argument cannot be a constant");
		    }
    break;

  case 371:
#line 1770 "parse.y"
    {
                        yyerror("formal argument cannot be an instance variable");
		    }
    break;

  case 372:
#line 1774 "parse.y"
    {
                        yyerror("formal argument cannot be a global variable");
		    }
    break;

  case 373:
#line 1778 "parse.y"
    {
                        yyerror("formal argument cannot be a class variable");
		    }
    break;

  case 374:
#line 1782 "parse.y"
    {
			if (!is_local_id(yyvsp[0].id))
			    yyerror("formal argument must be local variable");
			else if (local_id(yyvsp[0].id))
			    yyerror("duplicate argument name");
			local_cnt(yyvsp[0].id);
			yyval.num = 1;
		    }
    break;

  case 376:
#line 1794 "parse.y"
    {
			yyval.num += 1;
		    }
    break;

  case 377:
#line 1800 "parse.y"
    {
			if (!is_local_id(yyvsp[-2].id))
			    yyerror("formal argument must be local variable");
			else if (local_id(yyvsp[-2].id))
			    yyerror("duplicate optional argument name");
			yyval.node = assignable(yyvsp[-2].id, yyvsp[0].node);
		    }
    break;

  case 378:
#line 1810 "parse.y"
    {
			yyval.node = NEW_BLOCK(yyvsp[0].node);
			yyval.node->nd_end = yyval.node;
		    }
    break;

  case 379:
#line 1815 "parse.y"
    {
			yyval.node = block_append(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 380:
#line 1821 "parse.y"
    {
			if (!is_local_id(yyvsp[0].id))
			    yyerror("rest argument must be local variable");
			else if (local_id(yyvsp[0].id))
			    yyerror("duplicate rest argument name");
			yyval.id = local_cnt(yyvsp[0].id);
		    }
    break;

  case 381:
#line 1829 "parse.y"
    {
			yyval.id = -2;
		    }
    break;

  case 382:
#line 1835 "parse.y"
    {
			if (!is_local_id(yyvsp[0].id))
			    yyerror("block argument must be local variable");
			else if (local_id(yyvsp[0].id))
			    yyerror("duplicate block argument name");
			yyval.node = NEW_BLOCK_ARG(yyvsp[0].id);
		    }
    break;

  case 383:
#line 1845 "parse.y"
    {
			yyval.node = yyvsp[0].node;
		    }
    break;

  case 385:
#line 1852 "parse.y"
    {
			if (nd_type(yyvsp[0].node) == NODE_SELF) {
			    yyval.node = NEW_SELF();
			}
			else {
			    yyval.node = yyvsp[0].node;
			}
		    }
    break;

  case 386:
#line 1860 "parse.y"
    {lex_state = EXPR_BEG;}
    break;

  case 387:
#line 1861 "parse.y"
    {
					if (yyvsp[-2].node == 0) {
			    yyerror("can't define single method for ().");
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
				yyerror("can't define single method for literals");
			      default:
				value_expr(yyvsp[-2].node);
				break;
			    }
			}
			yyval.node = yyvsp[-2].node;
		    }
    break;

  case 389:
#line 1887 "parse.y"
    {
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 390:
#line 1891 "parse.y"
    {
			if (yyvsp[-1].node->nd_alen%2 != 0) {
			    yyerror("odd number list for Hash");
			}
			yyval.node = yyvsp[-1].node;
		    }
    break;

  case 392:
#line 1901 "parse.y"
    {
			yyval.node = list_concat(yyvsp[-2].node, yyvsp[0].node);
		    }
    break;

  case 393:
#line 1907 "parse.y"
    {
			yyval.node = list_append(NEW_LIST(yyvsp[-2].node), yyvsp[0].node);
		    }
    break;

  case 413:
#line 1945 "parse.y"
    {yyerrok;}
    break;

  case 416:
#line 1950 "parse.y"
    {yyerrok;}
    break;

  case 417:
#line 1954 "parse.y"
    {
			yyval.node = 0;
		    }
    break;


    }

/* Line 1016 of /usr/share/bison/yacc.c.  */
#line 5663 "y.tab.c"

  yyvsp -= yylen;
  yyssp -= yylen;


#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

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
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[yytype]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
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
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*----------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action.  |
`----------------------------------------------------*/
yyerrlab1:
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
	  while (yyssp > yyss)
	    {
	      YYDPRINTF ((stderr, "Error: popping "));
	      YYDSYMPRINT ((stderr,
			    yystos[*yyssp],
			    *yyvsp));
	      YYDPRINTF ((stderr, "\n"));
	      yydestruct (yystos[*yyssp], *yyvsp);
	      YYPOPSTACK;
	    }
	  YYABORT;
        }

      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yydestruct (yychar1, yylval);
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

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

      YYDPRINTF ((stderr, "Error: popping "));
      YYDSYMPRINT ((stderr,
		    yystos[*yyssp], *yyvsp));
      YYDPRINTF ((stderr, "\n"));

      yydestruct (yystos[yystate], *yyvsp);
      yyvsp--;
      yystate = *--yyssp;


#if YYDEBUG
      if (yydebug)
	{
	  short *yyssp1 = yyss - 1;
	  YYFPRINTF (stderr, "Error: state stack now");
	  while (yyssp1 != yyssp)
	    YYFPRINTF (stderr, " %d", *++yyssp1);
	  YYFPRINTF (stderr, "\n");
	}
#endif
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


#line 1958 "parse.y"

#include <ctype.h>
#include <sys/types.h>
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

static NODE *str_extend _((NODE*,int,int));

#define LEAVE_BS 1

static VALUE (*lex_gets)();	/* gets function */
static VALUE lex_input;		/* non-nil if File */
static VALUE lex_lastline;	/* gc protect */
static char *lex_pbeg;
static char *lex_p;
static char *lex_pend;

static int
yyerror(msg)
    char *msg;
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
    ruby_sourcefile = rb_source_filename(f);
    ruby_in_compile = 1;
    n = yyparse();
    ruby_debug_lines = 0;
    compile_for_eval = 0;
    ruby_in_compile = 0;
    cond_nest = 0;
    cond_stack = 0;
    cmdarg_stack = 0;
    class_nest = 0;
    in_single = 0;
    in_def = 0;
    cur_mid = 0;

    if (n == 0) node = ruby_eval_tree;
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
	    if (strncmp(lex_pbeg, "__END__", 7) == 0 &&
		(RSTRING(v)->len == 7 || lex_pbeg[7] == '\n' || lex_pbeg[7] == '\r')) {
		ruby__end__seen = 1;
		lex_lastline = 0;
		return -1;
	    }
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
	    char buf[3];
	    int i;

	    pushback(c);
	    for (i=0; i<3; i++) {
		c = nextc();
		if (c == -1) goto eof;
		if (c < '0' || '7' < c) {
		    pushback(c);
		    break;
		}
		buf[i] = c;
	    }
	    c = scan_oct(buf, i, &i);
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
	if (c != term)
	    tokadd('\\');
	tokadd(c);
    }
    return 0;
}

static int
parse_regx(term, paren)
    int term, paren;
{
    register int c;
    char kcode = 0;
    int once = 0;
    int nest = 0;
    int options = 0;
    int re_start = ruby_sourceline;
    NODE *list = 0;

    newtok();
    while ((c = nextc()) != -1) {
	if (c == term && nest == 0) {
	    goto regx_end;
	}

	switch (c) {
	  case '#':
	    list = str_extend(list, term, paren);
	    if (list == (NODE*)-1) goto unterminated;
	    continue;

	  case '\\':
	    if (tokadd_escape(term) < 0)
		return 0;
	    continue;

	  case -1:
	    goto unterminated;

	  default:
	    if (paren)  {
	      if (c == paren) nest++;
	      if (c == term) nest--;
	    }
	    if (ismbchar(c)) {
		int i, len = mbclen(c)-1;

		for (i = 0; i < len; i++) {
		    tokadd(c);
		    c = nextc();
		}
	    }
	    break;

	  regx_end:
	    for (;;) {
		switch (c = nextc()) {
		  case 'i':
		    options |= RE_OPTION_IGNORECASE;
		    break;
		  case 'x':
		    options |= RE_OPTION_EXTENDED;
		    break;
		  case 'p':	/* /p is obsolete */
		    rb_warn("/p option is obsolete; use /m\n\tnote: /m does not change ^, $ behavior");
		    options |= RE_OPTION_POSIXLINE;
		    break;
		  case 'm':
		    options |= RE_OPTION_MULTILINE;
		    break;
		  case 'o':
		    once = 1;
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
		    pushback(c);
		    goto end_options;
		}
	    }

	  end_options:
	    tokfix();
	    lex_state = EXPR_END;
	    if (list) {
		nd_set_line(list, re_start);
		if (toklen() > 0) {
		    VALUE ss = rb_str_new(tok(), toklen());
		    list_append(list, NEW_STR(ss));
		}
		nd_set_type(list, once?NODE_DREGX_ONCE:NODE_DREGX);
		list->nd_cflag = options | kcode;
		yylval.node = list;
		return tDREGEXP;
	    }
	    else {
		yylval.val = rb_reg_new(tok(), toklen(), options | kcode);
		return tREGEXP;
	    }
	}
	tokadd(c);
    }
  unterminated:
    ruby_sourceline = re_start;
    rb_compile_error("unterminated regexp meets end of file");
    return 0;
}

static int parse_qstring _((int,int));

static int
parse_string(func, term, paren)
    int func, term, paren;
{
    int c;
    NODE *list = 0;
    int strstart;
    int nest = 0;

    if (func == '\'') {
	return parse_qstring(term, paren);
    }
    if (func == 0) {		/* read 1 line for heredoc */
				/* -1 for chomp */
	yylval.val = rb_str_new(lex_pbeg, lex_pend - lex_pbeg - 1);
	lex_p = lex_pend;
	return tSTRING;
    }
    strstart = ruby_sourceline;
    newtok();
    while ((c = nextc()) != term || nest > 0) {
	if (c == -1) {
	  unterm_str:
	    ruby_sourceline = strstart;
	    rb_compile_error("unterminated string meets end of file");
	    return 0;
	}
	if (paren) {
	    if (c == paren) nest++;
	    if (c == term && nest-- == 0) break;
	}
	if (ismbchar(c)) {
	    int i, len = mbclen(c)-1;

	    for (i = 0; i < len; i++) {
		tokadd(c);
		c = nextc();
	    }
	}
	else if (c == '#') {
	    list = str_extend(list, term, paren);
	    if (list == (NODE*)-1) goto unterm_str;
	    continue;
	}
	else if (c == '\\') {
	    c = nextc();
	    if (c == '\n')
		continue;
	    if (c == term) {
		tokadd(c);
	    }
	    else {
                pushback(c);
                if (func != '"') tokadd('\\');
                tokadd(read_escape());
  	    }
	    continue;
	}
	tokadd(c);
    }

    tokfix();
    lex_state = EXPR_END;

    if (list) {
	nd_set_line(list, strstart);
	if (toklen() > 0) {
	    VALUE ss = rb_str_new(tok(), toklen());
	    list_append(list, NEW_STR(ss));
	}
	yylval.node = list;
	if (func == '`') {
	    nd_set_type(list, NODE_DXSTR);
	    return tDXSTRING;
	}
	else {
	    return tDSTRING;
	}
    }
    else {
	yylval.val = rb_str_new(tok(), toklen());
	return (func == '`') ? tXSTRING : tSTRING;
    }
}

static int
parse_qstring(term, paren)
    int term, paren;
{
    int strstart;
    int c;
    int nest = 0;

    strstart = ruby_sourceline;
    newtok();
    while ((c = nextc()) != term || nest > 0) {
	if (c == -1) {
	    ruby_sourceline = strstart;
	    rb_compile_error("unterminated string meets end of file");
	    return 0;
	}
	if (paren) {
	    if (c == paren) nest++;
	    if (c == term && nest-- == 0) break;
	}
	if (ismbchar(c)) {
	    int i, len = mbclen(c)-1;

	    for (i = 0; i < len; i++) {
		tokadd(c);
		c = nextc();
	    }
	}
	else if (c == '\\') {
	    c = nextc();
	    switch (c) {
	      case '\n':
		continue;

	      case '\\':
		c = '\\';
		break;

	      default:
		/* fall through */
		if (c == term || (paren && c == paren)) {
		    tokadd(c);
		    continue;
		}
		tokadd('\\');
	    }
	}
	tokadd(c);
    }

    tokfix();
    yylval.val = rb_str_new(tok(), toklen());
    lex_state = EXPR_END;
    return tSTRING;
}

static int
parse_quotedwords(term, paren)
    int term, paren;
{
    NODE *qwords = 0;
    int strstart;
    int c;
    int nest = 0;

    strstart = ruby_sourceline;
    newtok();

    while (c = nextc(),ISSPACE(c))
	;		/* skip preceding spaces */
    pushback(c);
    while ((c = nextc()) != term || nest > 0) {
	if (c == -1) {
	    ruby_sourceline = strstart;
	    rb_compile_error("unterminated string meets end of file");
	    return 0;
	}
	if (paren) {
	    if (c == paren) nest++;
	    if (c == term && nest-- == 0) break;
	}
	if (ismbchar(c)) {
	    int i, len = mbclen(c)-1;

	    for (i = 0; i < len; i++) {
		tokadd(c);
		c = nextc();
	    }
	}
	else if (c == '\\') {
	    c = nextc();
	    switch (c) {
	      case '\n':
		continue;
	      case '\\':
		c = '\\';
		break;
	      default:
		if (c == term || (paren && c == paren)) {
		    tokadd(c);
		    continue;
		}
		if (!ISSPACE(c))
		    tokadd('\\');
		break;
	    }
	}
	else if (ISSPACE(c)) {
	    NODE *str;

	    tokfix();
	    str = NEW_STR(rb_str_new(tok(), toklen()));
	    newtok();
	    if (!qwords) qwords = NEW_LIST(str);
	    else list_append(qwords, str);
	    while (c = nextc(),ISSPACE(c))
		;		/* skip continuous spaces */
	    pushback(c);
	    continue;
	}
	tokadd(c);
    }

    tokfix();
    if (toklen() > 0) {
	NODE *str;

	str = NEW_STR(rb_str_new(tok(), toklen()));
	if (!qwords) qwords = NEW_LIST(str);
	else list_append(qwords, str);
    }
    if (!qwords) qwords = NEW_ZARRAY();
    yylval.node = qwords;
    lex_state = EXPR_END;
    return tQWORDS;
}

static int
here_document(term, indent)
    char term;
    int indent;
{
    int c;
    char *eos, *p;
    int len;
    VALUE str;
    volatile VALUE line = 0;
    VALUE lastline_save;
    int offset_save;
    NODE *list = 0;
    int linesave = ruby_sourceline;
    int firstline;

    if (heredoc_end > 0) ruby_sourceline = heredoc_end;
    firstline = ruby_sourceline;

    newtok();
    switch (term) {
      case '\'':
      case '"':
      case '`':
	while ((c = nextc()) != term) {
	    switch (c) {
	      case -1:
		rb_compile_error("unterminated here document identifier meets end of file");
		return 0;
	      case '\n':
		rb_compile_error("unterminated here document identifier meets end of line");
		return 0;
	    }
	    tokadd(c);
	}
	if (term == '\'') term = 0;
	break;

      default:
	c = term;
	term = '"';
	if (!is_identchar(c)) {
	    rb_warn("use of bare << to mean <<\"\" is deprecated");
	    break;
	}
	while (is_identchar(c)) {
	    tokadd(c);
	    c = nextc();
	}
	pushback(c);
	break;
    }
    tokfix();
    lastline_save = lex_lastline;
    offset_save = lex_p - lex_pbeg;
    eos = strdup(tok());
    len = strlen(eos);

    str = rb_str_new(0,0);
    for (;;) {
	lex_lastline = line = lex_getline();
	if (NIL_P(line)) {
	  error:
	    ruby_sourceline = linesave;
	    rb_compile_error("can't find string \"%s\" anywhere before EOF", eos);
	    free(eos);
	    return 0;
	}
	ruby_sourceline++;
	p = RSTRING(line)->ptr;
	if (indent) {
	    while (*p && (*p == ' ' || *p == '\t')) {
		p++;
	    }
	}
	if (strncmp(eos, p, len) == 0) {
	    if (p[len] == '\n' || p[len] == '\r')
		break;
	    if (len == RSTRING(line)->len)
		break;
	}

	lex_pbeg = lex_p = RSTRING(line)->ptr;
	lex_pend = lex_p + RSTRING(line)->len;
#if 0
	if (indent) {
	    while (*lex_p && *lex_p == '\t') {
		lex_p++;
	    }
	}
#endif
      retry:
	switch (parse_string(term, '\n', 0)) {
	  case tSTRING:
	  case tXSTRING:
	    rb_str_cat2(yylval.val, "\n");
	    if (!list) {
	        rb_str_append(str, yylval.val);
	    }
	    else {
		list_append(list, NEW_STR(yylval.val));
	    }
	    break;
	  case tDSTRING:
	    if (!list) list = NEW_DSTR(str);
	    /* fall through */
	  case tDXSTRING:
	    if (!list) list = NEW_DXSTR(str);

	    list_append(yylval.node, NEW_STR(rb_str_new2("\n")));
	    nd_set_type(yylval.node, NODE_STR);
	    yylval.node = NEW_LIST(yylval.node);
	    yylval.node->nd_next = yylval.node->nd_head->nd_next;
	    list_concat(list, yylval.node);
	    break;

	  case 0:
	    goto error;
	}
	if (lex_p != lex_pend) {
	    goto retry;
	}
    }
    free(eos);
    lex_lastline = lastline_save;
    lex_pbeg = RSTRING(lex_lastline)->ptr;
    lex_pend = lex_pbeg + RSTRING(lex_lastline)->len;
    lex_p = lex_pbeg + offset_save;

    lex_state = EXPR_END;
    heredoc_end = ruby_sourceline;
    ruby_sourceline = linesave;

    if (list) {
	nd_set_line(list, firstline+1);
	yylval.node = list;
    }
    switch (term) {
      case '\0':
      case '\'':
      case '"':
	if (list) return tDSTRING;
	yylval.val = str;
	return tSTRING;
      case '`':
	if (list) return tDXSTRING;
	yylval.val = str;
	return tXSTRING;
    }
    return 0;
}

#include "lex.c"

static void
arg_ambiguous()
{
    rb_warning("ambiguous first argument; make sure");
}

static int
yylex()
{
    register int c;
    int space_seen = 0;
    struct kwtable *kw;

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
	    if (lex_state == EXPR_ARG && space_seen && !ISSPACE(c)){
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
	if (lex_p == lex_pbeg + 1) {
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
	    lex_state != EXPR_END && lex_state != EXPR_CLASS && lex_state != EXPR_DOT &&
	    (lex_state != EXPR_ARG || space_seen)) {
 	    int c2 = nextc();
	    int indent = 0;
	    if (c2 == '-') {
		indent = 1;
		c2 = nextc();
	    }
	    if (!ISSPACE(c2) && (strchr("\"'`", c2) || is_identchar(c2))) {
		return here_document(c2, indent);
	    }
	    pushback(c2);
	    if (indent) {
		pushback('-');
	    }
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
	return parse_string(c,c,0);
      case '`':
	if (lex_state == EXPR_FNAME) {
	    lex_state = EXPR_END;
	    return c;
	}
	if (lex_state == EXPR_DOT) {
	    lex_state = EXPR_ARG;
	    return c;
	}
	return parse_string(c,c,0);

      case '\'':
	return parse_qstring(c,0);

      case '?':
	if (lex_state == EXPR_END) {
	    lex_state = EXPR_BEG;
	    return '?';
	}
	c = nextc();
	if (c == -1) {
	    return '?';
	}
	if (ISSPACE(c)){
	    if (lex_state != EXPR_ARG){
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
		}
		if (c2) {
		    rb_warn("invalid character syntax; use ?\\%c", c);
		}
	    }
	    else {
		pushback(c);
		lex_state = EXPR_BEG;
		return '?';
	    }
	}
	if (c == '\\') {
	    c = read_escape();
	}
	c &= 0xff;
	yylval.val = INT2FIX(c);
	lex_state = EXPR_END;
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
	if (lex_state == EXPR_ARG && space_seen && !ISSPACE(c)){
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
	    (lex_state == EXPR_ARG && space_seen && !ISSPACE(c))) {
	    if (lex_state == EXPR_ARG) arg_ambiguous();
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
	    (lex_state == EXPR_ARG && space_seen && !ISSPACE(c))) {
	    if (lex_state == EXPR_ARG) arg_ambiguous();
	    lex_state = EXPR_BEG;
	    pushback(c);
	    if (ISDIGIT(c)) {
		c = '-';
		goto start_num;
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
	if (!ISDIGIT(c)) {
	    lex_state = EXPR_DOT;
	    return '.';
	}
	c = '.';
	/* fall through */

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
			} while (c = nextc());
		    }
		    pushback(c);
		    tokfix();
		    if (toklen() == start) {
			yyerror("hexadecimal number without hex-digits");
		    }
		    else if (nondigit) goto trailing_uc;
		    yylval.val = rb_cstr2inum(tok(), 16);
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
			} while (c = nextc());
		    }
		    pushback(c);
		    tokfix();
		    if (toklen() == start) {
			yyerror("numeric literal without digits");
		    }
		    else if (nondigit) goto trailing_uc;
		    yylval.val = rb_cstr2inum(tok(), 2);
		    return tINTEGER;
		}
		if (c >= '0' && c <= '7' || c == '_') {
		    /* octal */
	            do {
			if (c == '_') {
			    if (nondigit) break;
			    nondigit = c;
			    continue;
			}
			if (c < '0' || c > '7') break;
			nondigit = 0;
			tokadd(c);
		    } while (c = nextc());
		    if (toklen() > start) {
			pushback(c);
			tokfix();
			if (nondigit) goto trailing_uc;
			yylval.val = rb_cstr2inum(tok(), 8);
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
		    yylval.val = INT2FIX(0);
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
		yylval.val = rb_float_new(d);
		return tFLOAT;
	    }
	    yylval.val = rb_cstr2inum(tok(), 10);
	    return tINTEGER;
	}

      case ']':
      case '}':
	lex_state = EXPR_END;
	return c;

      case ')':
	if (cond_nest > 0) {
	    cond_stack >>= 1;
	}
	lex_state = EXPR_END;
	return c;

      case ':':
	c = nextc();
	if (c == ':') {
	    if (lex_state == EXPR_BEG ||  lex_state == EXPR_MID ||
		(lex_state == EXPR_ARG && space_seen)) {
		lex_state = EXPR_BEG;
		return tCOLON3;
	    }
	    lex_state = EXPR_DOT;
	    return tCOLON2;
	}
	pushback(c);
	if (lex_state == EXPR_END || ISSPACE(c)) {
	    lex_state = EXPR_BEG;
	    return ':';
	}
	lex_state = EXPR_FNAME;
	return tSYMBEG;

      case '/':
	if (lex_state == EXPR_BEG || lex_state == EXPR_MID) {
	    return parse_regx('/', '/');
	}
	if ((c = nextc()) == '=') {
	    yylval.id = '/';
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	pushback(c);
	if (lex_state == EXPR_ARG && space_seen) {
	    if (!ISSPACE(c)) {
		arg_ambiguous();
		return parse_regx('/', '/');
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

      case ',':
      case ';':
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
	if (cond_nest > 0) {
	    cond_stack = (cond_stack<<1)|0;
	}
	if (lex_state == EXPR_BEG || lex_state == EXPR_MID) {
	    c = tLPAREN;
	}
	else if (lex_state == EXPR_ARG && space_seen) {
	    rb_warning("%s (...) interpreted as method call", tok());
	}
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
	else if (lex_state == EXPR_ARG && space_seen) {
	    c = tLBRACK;
	}
	lex_state = EXPR_BEG;
	return c;

      case '{':
	if (lex_state != EXPR_END && lex_state != EXPR_ARG)
	    c = tLBRACE;
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
		return parse_string('"', term, paren);

	      case 'q':
		return parse_qstring(term, paren);

	      case 'w':
		return parse_quotedwords(term, paren);

	      case 'x':
		return parse_string('`', term, paren);

	      case 'r':
		return parse_regx(term, paren);

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
	if (lex_state == EXPR_ARG && space_seen && !ISSPACE(c)) {
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
	  case '*':		/* : argv */
	  case '$':		/* $$: pid */
	  case '?':		/* $?: last status */
	  case '!':		/* $!: error string */
	  case '@':		/* : error position */
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
	    while (ISDIGIT(c)) {
		tokadd(c);
		c = nextc();
	    }
	    if (is_identchar(c))
		break;
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
		rb_compile_error("`@%c' is not a valid instance variable name", c);
	    }
	    else {
		rb_compile_error("`@@%c' is not a valid class variable name", c);
	    }
	}
	if (!is_identchar(c)) {
	    pushback(c);
	    return '@';
	}
	break;

      default:
	if (!is_identchar(c) || ISDIGIT(c)) {
	    rb_compile_error("Invalid char `\\%03o' in expression", c);
	    goto retry;
	}

	newtok();
	break;
    }

    while (is_identchar(c)) {
	tokadd(c);
	if (ismbchar(c)) {
	    int i, len = mbclen(c)-1;

	    for (i = 0; i < len; i++) {
		c = nextc();
		tokadd(c);
	    }
	}
	c = nextc();
    }
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
	    if (lex_state != EXPR_DOT) {
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
			if (CMDARG_P()) return kDO_BLOCK;
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

	    if (toklast() == '!' || toklast() == '?') {
		result = tFID;
	    }
	    else {
		if (lex_state == EXPR_FNAME) {
#if 0
		    if ((c = nextc()) == '=' && !peek('=') && !peek('~') && !peek('>')) {
#else
		    if ((c = nextc()) == '=' && !peek('~') && !peek('>') &&
			(!peek('=') || lex_p + 1 < lex_pend && lex_p[1] == '>')) {
#endif
			result = tIDENTIFIER;
			tokadd(c);
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
	    if (lex_state == EXPR_BEG ||
		lex_state == EXPR_DOT ||
		lex_state == EXPR_ARG) {
		lex_state = EXPR_ARG;
	    }
	    else {
		lex_state = EXPR_END;
	    }
	}
	tokfix();
	yylval.id = rb_intern(tok());
	return result;
    }
}

static NODE*
str_extend(list, term, paren)
    NODE *list;
    int term, paren;
{
    int c;
    int brace = -1;
    VALUE ss;
    NODE *node;
    int brace_nest = 0;
    int paren_nest = 0;
    int start;

    c = nextc();
    switch (c) {
      case '$':
      case '@':
      case '{':
	break;
      default:
	tokadd('#');
	pushback(c);
	return list;
    }

    start = ruby_sourceline;
    ss = rb_str_new(tok(), toklen());
    if (list == 0) {
	list = NEW_DSTR(ss);
    }
    else if (toklen() > 0) {
	list_append(list, NEW_STR(ss));
    }
    newtok();

    switch (c) {
      case '$':
	tokadd('$');
	c = nextc();
	if (c == -1) return (NODE*)-1;
	switch (c) {
	  case '1': case '2': case '3':
	  case '4': case '5': case '6':
	  case '7': case '8': case '9':
	    while (ISDIGIT(c)) {
		tokadd(c);
		c = nextc();
	    }
	    pushback(c);
	    goto fetch_id;

	  case '&': case '+':
	  case '_': case '~':
	  case '*': case '$': case '?':
	  case '!': case '@': case ',':
	  case '.': case '=': case ':':
	  case '<': case '>': case '\\':
	  case ';':
	  refetch:
	    tokadd(c);
	    goto fetch_id;

	  case '-':
	    tokadd(c);
	    c = nextc();
	    if (!is_identchar(c)) {
		pushback(c);
		goto invalid_interporate;
	    }
	    tokadd(c);
	    goto fetch_id;

          default:
	    if (c == term) {
		list_append(list, NEW_STR(rb_str_new2("#$")));
		pushback(c);
		newtok();
		return list;
	    }
	    switch (c) {
	      case '\"':
	      case '/':
	      case '\'':
	      case '`':
		goto refetch;
	    }
	    if (!is_identchar(c)) {
	      pushback(c);
	      invalid_interporate:
		{
		    VALUE s = rb_str_new2("#");
		    rb_str_cat(s, tok(), toklen());
		    list_append(list, NEW_STR(s));
		    newtok();
		    return list;
		}
	    }
	}

	while (is_identchar(c)) {
	    tokadd(c);
	    if (ismbchar(c)) {
		int i, len = mbclen(c)-1;

		for (i = 0; i < len; i++) {
		    c = nextc();
		    tokadd(c);
		}
	    }
	    c = nextc();
	}
	pushback(c);
	break;

      case '@':
	tokadd(c);
	c = nextc();
        if (c == '@') {
	    tokadd(c);
	    c = nextc();
        }
	while (is_identchar(c)) {
	    tokadd(c);
	    if (ismbchar(c)) {
		int i, len = mbclen(c)-1;

		for (i = 0; i < len; i++) {
		    c = nextc();
		    tokadd(c);
		}
	    }
	    c = nextc();
	}
	pushback(c);
	if (toklen() == 1) {
	    goto invalid_interporate;
	}
	break;

      case '{':
	if (c == '{') brace = '}';
	brace_nest = 1;
	do {
	  loop_again:
	    c = nextc();
	    switch (c) {
	      case -1:
		if (brace_nest > 1) {
		    yyerror("bad substitution in string");
		    newtok();
		    return list;
		}
		return (NODE*)-1;
	      case '}':
		if (c == brace) {
		    brace_nest--;
		    if (brace_nest == 0) break;
		}
		tokadd(c);
		goto loop_again;
	      case '\\':
		c = nextc();
		if (c == -1) return (NODE*)-1;
		if (c == term) {
		    tokadd(c);
		}
		else {
		    tokadd('\\');
		    tokadd(c);
		}
		goto loop_again;
	      case '{':
		if (brace != -1) brace_nest++;
	      default:
		/* within brace */
		if (brace_nest > 0) {
		    if (ismbchar(c)) {
			int i, len = mbclen(c)-1;

			for (i = 0; i < len; i++) {
			    tokadd(c);
			    c = nextc();
			}
		    }
		    tokadd(c);
		    break;
		}
		/* out of brace */
		if (c == paren) paren_nest++;
		else if (c == term && (!paren || paren_nest-- == 0)) {
		    pushback(c);
		    list_append(list, NEW_STR(rb_str_new2("#{")));
		    rb_warning("bad substitution in string");
		    tokfix();
		    list_append(list, NEW_STR(rb_str_new(tok(), toklen())));
		    newtok();
		    return list;
		}
		break;
	      case '\n':
		tokadd(c);
		break;
	    }
	} while (c != brace);
    }

  fetch_id:
    tokfix();
    node = NEW_EVSTR(tok(),toklen());
    nd_set_line(node, start);
    list_append(list, node);
    newtok();

    return list;
}

NODE*
rb_node_newnode(type, a0, a1, a2)
    enum node_type type;
    NODE *a0, *a1, *a2;
{
    NODE *n = (NODE*)rb_newobj();

    n->flags |= T_NODE;
    nd_set_type(n, type);
    nd_set_line(n, ruby_sourceline);
    n->nd_file = ruby_sourcefile;

    n->u1.node = a0;
    n->u2.node = a1;
    n->u3.node = a2;

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

static NODE*
block_append(head, tail)
    NODE *head, *tail;
{
    NODE *end;

    if (tail == 0) return head;
    if (head == 0) return tail;

    if (nd_type(head) != NODE_BLOCK) {
	end = NEW_BLOCK(head);
	end->nd_end = end;
	fixpos(end, head);
	head = end;
    }
    else {
	end = head->nd_end;
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
	    rb_warning("statement not reached");
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
    head->nd_end = tail->nd_end;
    return head;
}

static NODE*
list_append(head, tail)
    NODE *head, *tail;
{
    NODE *last;

    if (head == 0) return NEW_LIST(tail);

    last = head;
    while (last->nd_next) {
	last = last->nd_next;
    }

    last->nd_next = NEW_LIST(tail);
    head->nd_alen += 1;
    return head;
}

static NODE*
list_concat(head, tail)
    NODE *head, *tail;
{
    NODE *last;

    last = head;
    while (last->nd_next) {
	last = last->nd_next;
    }

    last->nd_next = tail;
    head->nd_alen += tail->nd_alen;

    return head;
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
    }

    return NEW_CALL(recv, id, narg==1?NEW_LIST(arg1):0);
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
    rb_bug("invalid id for gettable");
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
	return NEW_CDECL(id, val);
    }
    else if (is_class_id(id)) {
	if (in_def || in_single) return NEW_CVASGN(id, val);
	return NEW_CVDECL(id, val);
    }
    else {
	rb_bug("bad id for variable");
    }
    return 0;
}

static NODE *
aryset(recv, idx)
    NODE *recv, *idx;
{
    value_expr(recv);
    return NEW_CALL(recv, tASET, idx);
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
    value_expr(recv);
    return NEW_CALL(recv, rb_id_attrset(id), 0);
}

static void
rb_backref_error(node)
    NODE *node;
{
    switch (nd_type(node)) {
      case NODE_NTH_REF:
	rb_compile_error("Can't set variable $%d", (int)node->nd_nth);
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

      case NODE_CALL:
	lhs->nd_args = arg_add(lhs->nd_args, rhs);
	break;

      default:
	/* should not happen */
	break;
    }

    if (rhs) fixpos(lhs, rhs);
    return lhs;
}

static int
value_expr0(node)
    NODE *node;
{
    int cond = 0;

    while (node) {
	switch (nd_type(node)) {
	  case NODE_RETURN:
	  case NODE_BREAK:
	  case NODE_NEXT:
	  case NODE_REDO:
	  case NODE_RETRY:
	  case NODE_WHILE:
	  case NODE_UNTIL:
	  case NODE_CLASS:
	  case NODE_MODULE:
	  case NODE_DEFN:
	  case NODE_DEFS:
	    if (!cond) yyerror("void value expression");
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
    if (!node) return;

  again:
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


static NODE *cond2 _((NODE*));

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
	rb_warn("found = in conditional, should be ==");
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
	rb_warning("assignment in condition");
    }
#endif
    return 1;
}

static NODE*
cond0(node)
    NODE *node;
{
    enum node_type type = nd_type(node);

    assign_in_cond(node);
    switch (type) {
      case NODE_DREGX:
      case NODE_DREGX_ONCE:
	local_cnt('_');
	local_cnt('~');
	return NEW_MATCH2(node, NEW_GVAR(rb_intern("$_")));

      case NODE_DOT2:
      case NODE_DOT3:
	node->nd_beg = cond2(node->nd_beg);
	node->nd_end = cond2(node->nd_end);
	if (type == NODE_DOT2) nd_set_type(node,NODE_FLIP2);
	else if (type == NODE_DOT3) nd_set_type(node, NODE_FLIP3);
	node->nd_cnt = local_append(0);
	return node;

      case NODE_LIT:
	if (TYPE(node->nd_lit) == T_REGEXP) {
	    local_cnt('_');
	    local_cnt('~');
	    return NEW_MATCH(node);
	}
	if (TYPE(node->nd_lit) == T_STRING) {
	    local_cnt('_');
	    local_cnt('~');
	    return NEW_MATCH(rb_reg_new(RSTRING(node)->ptr,RSTRING(node)->len,0));
	}
      default:
	return node;
    }
}

static NODE*
cond(node)
    NODE *node;
{
    if (node == 0) return 0;
    if (nd_type(node) == NODE_NEWLINE){
	node->nd_next = cond0(node->nd_next);
	return node;
    }
    return cond0(node);
}

static NODE*
cond2(node)
    NODE *node;
{
    enum node_type type;

    if (!node) return node;
    node = cond(node);
    type = nd_type(node);
    if (type == NODE_NEWLINE) node = node->nd_next;
    if (type == NODE_LIT && FIXNUM_P(node->nd_lit)) {
	return call_op(node,tEQ,1,NEW_GVAR(rb_intern("$.")));
    }
    return node;
}

static NODE*
logop(type, left, right)
    enum node_type type;
    NODE *left, *right;
{
    value_expr(left);
    return rb_node_newnode(type, cond(left), cond(right), 0);
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
	    if (i == 0 || (ruby_scope->flag & SCOPE_MALLOC) == 0) {
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
		ruby_scope->flag |= SCOPE_MALLOC;
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

    rb_gc_mark_maybe(yylval.val);
    rb_gc_mark(ruby_debug_lines);
    rb_gc_mark(lex_lastline);
    rb_gc_mark(lex_input);
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
    tDOT2,	"..",
    tDOT3,	"...",
    '+',	"+",
    '-',	"-",
    '+',	"+(binary)",
    '-',	"-(binary)",
    '*',	"*",
    '/',	"/",
    '%',	"%",
    tPOW,	"**",
    tUPLUS,	"+@",
    tUMINUS,	"-@",
    tUPLUS,	"+(unary)",
    tUMINUS,	"-(unary)",
    '|',	"|",
    '^',	"^",
    '&',	"&",
    tCMP,	"<=>",
    '>',	">",
    tGEQ,	">=",
    '<',	"<",
    tLEQ,	"<=",
    tEQ,	"==",
    tEQQ,	"===",
    tNEQ,	"!=",
    tMATCH,	"=~",
    tNMATCH,	"!~",
    '!',	"!",
    '~',	"~",
    '!',	"!(unary)",
    '~',	"~(unary)",
    '!',	"!@",
    '~',	"~@",
    tAREF,	"[]",
    tASET,	"[]=",
    tLSHFT,	"<<",
    tRSHFT,	">>",
    tCOLON2,	"::",
    tCOLON3,	"::",
    '`',	"`",
    0,		0,
};

static st_table *sym_tbl;
static st_table *sym_rev_tbl;

void
Init_sym()
{
    sym_tbl = st_init_strtable_with_size(200);
    sym_rev_tbl = st_init_numtable_with_size(200);
}

ID
rb_intern(name)
    const char *name;
{
    static ID last_id = LAST_TOKEN;
    ID id;
    int last;

    if (st_lookup(sym_tbl, name, &id))
	return id;

    id = 0;
    switch (name[0]) {
      case '$':
	id |= ID_GLOBAL;
	break;
      case '@':
	if (name[1] == '@')
	    id |= ID_CLASS;
	else
	    id |= ID_INSTANCE;
	break;
      default:
	if (name[0] != '_' && !ISALPHA(name[0]) && !ismbchar(name[0])) {
	    /* operator */
	    int i;

	    for (i=0; op_tbl[i].token; i++) {
		if (*op_tbl[i].name == *name &&
		    strcmp(op_tbl[i].name, name) == 0) {
		    id = op_tbl[i].token;
		    goto id_regist;
		}
	    }
	}

	last = strlen(name)-1;
	if (name[last] == '=') {
	    /* attribute assignment */
	    char *buf = ALLOCA_N(char,last+1);

	    strncpy(buf, name, last);
	    buf[last] = '\0';
	    id = rb_intern(buf);
	    if (id > LAST_TOKEN && !is_attrset_id(id)) {
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
    id |= ++last_id << ID_SCOPE_SHIFT;
  id_regist:
    name = strdup(name);
    st_add_direct(sym_tbl, name, id);
    st_add_direct(sym_rev_tbl, id, name);
    return id;
}

char *
rb_id2name(id)
    ID id;
{
    char *name;

    if (id < LAST_TOKEN) {
	int i = 0;

	for (i=0; op_tbl[i].token; i++) {
	    if (op_tbl[i].token == id)
		return op_tbl[i].name;
	}
    }

    if (st_lookup(sym_rev_tbl, id, &name))
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
    if (ruby_scope->local_vars) {
	return ruby_scope->local_vars[1];
    }
    return Qnil;
}

void
rb_backref_set(val)
    VALUE val;
{
    if (ruby_scope->local_vars) {
	ruby_scope->local_vars[1] = val;
    }
    else {
	special_local_set('~', val);
    }
}

VALUE
rb_lastline_get()
{
    if (ruby_scope->local_vars) {
	return ruby_scope->local_vars[0];
    }
    return Qnil;
}

void
rb_lastline_set(val)
    VALUE val;
{
    if (ruby_scope->local_vars) {
	ruby_scope->local_vars[0] = val;
    }
    else {
	special_local_set('_', val);
    }
}


