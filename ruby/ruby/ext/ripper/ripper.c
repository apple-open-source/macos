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



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     END_OF_INPUT = 0,
     keyword_class = 258,
     keyword_module = 259,
     keyword_def = 260,
     keyword_undef = 261,
     keyword_begin = 262,
     keyword_rescue = 263,
     keyword_ensure = 264,
     keyword_end = 265,
     keyword_if = 266,
     keyword_unless = 267,
     keyword_then = 268,
     keyword_elsif = 269,
     keyword_else = 270,
     keyword_case = 271,
     keyword_when = 272,
     keyword_while = 273,
     keyword_until = 274,
     keyword_for = 275,
     keyword_break = 276,
     keyword_next = 277,
     keyword_redo = 278,
     keyword_retry = 279,
     keyword_in = 280,
     keyword_do = 281,
     keyword_do_cond = 282,
     keyword_do_block = 283,
     keyword_do_LAMBDA = 284,
     keyword_return = 285,
     keyword_yield = 286,
     keyword_super = 287,
     keyword_self = 288,
     keyword_nil = 289,
     keyword_true = 290,
     keyword_false = 291,
     keyword_and = 292,
     keyword_or = 293,
     keyword_not = 294,
     modifier_if = 295,
     modifier_unless = 296,
     modifier_while = 297,
     modifier_until = 298,
     modifier_rescue = 299,
     keyword_alias = 300,
     keyword_defined = 301,
     keyword_BEGIN = 302,
     keyword_END = 303,
     keyword__LINE__ = 304,
     keyword__FILE__ = 305,
     keyword__ENCODING__ = 306,
     tIDENTIFIER = 307,
     tFID = 308,
     tGVAR = 309,
     tIVAR = 310,
     tCONSTANT = 311,
     tCVAR = 312,
     tLABEL = 313,
     tINTEGER = 314,
     tFLOAT = 315,
     tSTRING_CONTENT = 316,
     tCHAR = 317,
     tNTH_REF = 318,
     tBACK_REF = 319,
     tREGEXP_END = 320,
     tUPLUS = 130,
     tUMINUS = 131,
     tPOW = 132,
     tCMP = 134,
     tEQ = 139,
     tEQQ = 140,
     tNEQ = 141,
     tGEQ = 138,
     tLEQ = 137,
     tANDOP = 321,
     tOROP = 322,
     tMATCH = 142,
     tNMATCH = 143,
     tDOT2 = 128,
     tDOT3 = 129,
     tAREF = 144,
     tASET = 145,
     tLSHFT = 135,
     tRSHFT = 136,
     tCOLON2 = 323,
     tCOLON3 = 324,
     tOP_ASGN = 325,
     tASSOC = 326,
     tLPAREN = 327,
     tLPAREN_ARG = 328,
     tRPAREN = 329,
     tLBRACK = 330,
     tLBRACE = 331,
     tLBRACE_ARG = 332,
     tSTAR = 333,
     tDSTAR = 334,
     tAMPER = 335,
     tLAMBDA = 336,
     tSYMBEG = 337,
     tSTRING_BEG = 338,
     tXSTRING_BEG = 339,
     tREGEXP_BEG = 340,
     tWORDS_BEG = 341,
     tQWORDS_BEG = 342,
     tSYMBOLS_BEG = 343,
     tQSYMBOLS_BEG = 344,
     tSTRING_DBEG = 345,
     tSTRING_DEND = 346,
     tSTRING_DVAR = 347,
     tSTRING_END = 348,
     tLAMBEG = 349,
     tLOWEST = 350,
     tUMINUS_NUM = 351,
     tLAST_TOKEN = 352
   };
#endif
/* Tokens.  */
#define END_OF_INPUT 0
#define keyword_class 258
#define keyword_module 259
#define keyword_def 260
#define keyword_undef 261
#define keyword_begin 262
#define keyword_rescue 263
#define keyword_ensure 264
#define keyword_end 265
#define keyword_if 266
#define keyword_unless 267
#define keyword_then 268
#define keyword_elsif 269
#define keyword_else 270
#define keyword_case 271
#define keyword_when 272
#define keyword_while 273
#define keyword_until 274
#define keyword_for 275
#define keyword_break 276
#define keyword_next 277
#define keyword_redo 278
#define keyword_retry 279
#define keyword_in 280
#define keyword_do 281
#define keyword_do_cond 282
#define keyword_do_block 283
#define keyword_do_LAMBDA 284
#define keyword_return 285
#define keyword_yield 286
#define keyword_super 287
#define keyword_self 288
#define keyword_nil 289
#define keyword_true 290
#define keyword_false 291
#define keyword_and 292
#define keyword_or 293
#define keyword_not 294
#define modifier_if 295
#define modifier_unless 296
#define modifier_while 297
#define modifier_until 298
#define modifier_rescue 299
#define keyword_alias 300
#define keyword_defined 301
#define keyword_BEGIN 302
#define keyword_END 303
#define keyword__LINE__ 304
#define keyword__FILE__ 305
#define keyword__ENCODING__ 306
#define tIDENTIFIER 307
#define tFID 308
#define tGVAR 309
#define tIVAR 310
#define tCONSTANT 311
#define tCVAR 312
#define tLABEL 313
#define tINTEGER 314
#define tFLOAT 315
#define tSTRING_CONTENT 316
#define tCHAR 317
#define tNTH_REF 318
#define tBACK_REF 319
#define tREGEXP_END 320
#define tUPLUS 130
#define tUMINUS 131
#define tPOW 132
#define tCMP 134
#define tEQ 139
#define tEQQ 140
#define tNEQ 141
#define tGEQ 138
#define tLEQ 137
#define tANDOP 321
#define tOROP 322
#define tMATCH 142
#define tNMATCH 143
#define tDOT2 128
#define tDOT3 129
#define tAREF 144
#define tASET 145
#define tLSHFT 135
#define tRSHFT 136
#define tCOLON2 323
#define tCOLON3 324
#define tOP_ASGN 325
#define tASSOC 326
#define tLPAREN 327
#define tLPAREN_ARG 328
#define tRPAREN 329
#define tLBRACK 330
#define tLBRACE 331
#define tLBRACE_ARG 332
#define tSTAR 333
#define tDSTAR 334
#define tAMPER 335
#define tLAMBDA 336
#define tSYMBEG 337
#define tSTRING_BEG 338
#define tXSTRING_BEG 339
#define tREGEXP_BEG 340
#define tWORDS_BEG 341
#define tQWORDS_BEG 342
#define tSYMBOLS_BEG 343
#define tQSYMBOLS_BEG 344
#define tSTRING_DBEG 345
#define tSTRING_DEND 346
#define tSTRING_DVAR 347
#define tSTRING_END 348
#define tLAMBEG 349
#define tLOWEST 350
#define tUMINUS_NUM 351
#define tLAST_TOKEN 352




/* Copy the first part of user declarations.  */
#line 12 "ripper.y"


#ifndef PARSER_DEBUG
#define PARSER_DEBUG 0
#endif
#define YYDEBUG 1
#define YYERROR_VERBOSE 1
#define YYSTACK_USE_ALLOCA 0

#include "ruby/ruby.h"
#include "ruby/st.h"
#include "ruby/encoding.h"
#include "internal.h"
#include "node.h"
#include "parse.h"
#include "id.h"
#include "regenc.h"
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include "probes.h"

#define numberof(array) (int)(sizeof(array) / sizeof((array)[0]))

#define YYMALLOC(size)		rb_parser_malloc(parser, (size))
#define YYREALLOC(ptr, size)	rb_parser_realloc(parser, (ptr), (size))
#define YYCALLOC(nelem, size)	rb_parser_calloc(parser, (nelem), (size))
#define YYFREE(ptr)		rb_parser_free(parser, (ptr))
#define malloc	YYMALLOC
#define realloc	YYREALLOC
#define calloc	YYCALLOC
#define free	YYFREE

#ifndef RIPPER
static ID register_symid(ID, const char *, long, rb_encoding *);
static ID register_symid_str(ID, VALUE);
#define REGISTER_SYMID(id, name) register_symid((id), (name), strlen(name), enc)
#include "id.c"
#endif

#define is_notop_id(id) ((id)>tLAST_OP_ID)
#define is_local_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_LOCAL)
#define is_global_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_GLOBAL)
#define is_instance_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_INSTANCE)
#define is_attrset_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_ATTRSET)
#define is_const_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_CONST)
#define is_class_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_CLASS)
#define is_junk_id(id) (is_notop_id(id)&&((id)&ID_SCOPE_MASK)==ID_JUNK)
#define id_type(id) (is_notop_id(id) ? (int)((id)&ID_SCOPE_MASK) : -1)

#define is_asgn_or_id(id) ((is_notop_id(id)) && \
	(((id)&ID_SCOPE_MASK) == ID_GLOBAL || \
	 ((id)&ID_SCOPE_MASK) == ID_INSTANCE || \
	 ((id)&ID_SCOPE_MASK) == ID_CLASS))

enum lex_state_bits {
    EXPR_BEG_bit,		/* ignore newline, +/- is a sign. */
    EXPR_END_bit,		/* newline significant, +/- is an operator. */
    EXPR_ENDARG_bit,		/* ditto, and unbound braces. */
    EXPR_ENDFN_bit,		/* ditto, and unbound braces. */
    EXPR_ARG_bit,		/* newline significant, +/- is an operator. */
    EXPR_CMDARG_bit,		/* newline significant, +/- is an operator. */
    EXPR_MID_bit,		/* newline significant, +/- is an operator. */
    EXPR_FNAME_bit,		/* ignore newline, no reserved words. */
    EXPR_DOT_bit,		/* right after `.' or `::', no reserved words. */
    EXPR_CLASS_bit,		/* immediate after `class', no here document. */
    EXPR_VALUE_bit,		/* alike EXPR_BEG but label is disallowed. */
    EXPR_MAX_STATE
};
/* examine combinations */
enum lex_state_e {
#define DEF_EXPR(n) EXPR_##n = (1 << EXPR_##n##_bit)
    DEF_EXPR(BEG),
    DEF_EXPR(END),
    DEF_EXPR(ENDARG),
    DEF_EXPR(ENDFN),
    DEF_EXPR(ARG),
    DEF_EXPR(CMDARG),
    DEF_EXPR(MID),
    DEF_EXPR(FNAME),
    DEF_EXPR(DOT),
    DEF_EXPR(CLASS),
    DEF_EXPR(VALUE),
    EXPR_BEG_ANY  =  (EXPR_BEG | EXPR_VALUE | EXPR_MID | EXPR_CLASS),
    EXPR_ARG_ANY  =  (EXPR_ARG | EXPR_CMDARG),
    EXPR_END_ANY  =  (EXPR_END | EXPR_ENDARG | EXPR_ENDFN)
};
#define IS_lex_state_for(x, ls)	((x) & (ls))
#define IS_lex_state(ls)	IS_lex_state_for(lex_state, (ls))

#if PARSER_DEBUG
static const char *lex_state_name(enum lex_state_e state);
#endif

typedef VALUE stack_type;

# define BITSTACK_PUSH(stack, n)	((stack) = ((stack)<<1)|((n)&1))
# define BITSTACK_POP(stack)	((stack) = (stack) >> 1)
# define BITSTACK_LEXPOP(stack)	((stack) = ((stack) >> 1) | ((stack) & 1))
# define BITSTACK_SET_P(stack)	((stack)&1)

#define COND_PUSH(n)	BITSTACK_PUSH(cond_stack, (n))
#define COND_POP()	BITSTACK_POP(cond_stack)
#define COND_LEXPOP()	BITSTACK_LEXPOP(cond_stack)
#define COND_P()	BITSTACK_SET_P(cond_stack)

#define CMDARG_PUSH(n)	BITSTACK_PUSH(cmdarg_stack, (n))
#define CMDARG_POP()	BITSTACK_POP(cmdarg_stack)
#define CMDARG_LEXPOP()	BITSTACK_LEXPOP(cmdarg_stack)
#define CMDARG_P()	BITSTACK_SET_P(cmdarg_stack)

struct vtable {
    ID *tbl;
    int pos;
    int capa;
    struct vtable *prev;
};

struct local_vars {
    struct vtable *args;
    struct vtable *vars;
    struct vtable *used;
    struct local_vars *prev;
};

#define DVARS_INHERIT ((void*)1)
#define DVARS_TOPSCOPE NULL
#define DVARS_SPECIAL_P(tbl) (!POINTER_P(tbl))
#define POINTER_P(val) ((VALUE)(val) & ~(VALUE)3)

static int
vtable_size(const struct vtable *tbl)
{
    if (POINTER_P(tbl)) {
        return tbl->pos;
    }
    else {
        return 0;
    }
}

#define VTBL_DEBUG 0

static struct vtable *
vtable_alloc(struct vtable *prev)
{
    struct vtable *tbl = ALLOC(struct vtable);
    tbl->pos = 0;
    tbl->capa = 8;
    tbl->tbl = ALLOC_N(ID, tbl->capa);
    tbl->prev = prev;
    if (VTBL_DEBUG) printf("vtable_alloc: %p\n", (void *)tbl);
    return tbl;
}

static void
vtable_free(struct vtable *tbl)
{
    if (VTBL_DEBUG)printf("vtable_free: %p\n", (void *)tbl);
    if (POINTER_P(tbl)) {
        if (tbl->tbl) {
            xfree(tbl->tbl);
        }
        xfree(tbl);
    }
}

static void
vtable_add(struct vtable *tbl, ID id)
{
    if (!POINTER_P(tbl)) {
        rb_bug("vtable_add: vtable is not allocated (%p)", (void *)tbl);
    }
    if (VTBL_DEBUG) printf("vtable_add: %p, %s\n", (void *)tbl, rb_id2name(id));

    if (tbl->pos == tbl->capa) {
        tbl->capa = tbl->capa * 2;
        REALLOC_N(tbl->tbl, ID, tbl->capa);
    }
    tbl->tbl[tbl->pos++] = id;
}

static int
vtable_included(const struct vtable * tbl, ID id)
{
    int i;

    if (POINTER_P(tbl)) {
        for (i = 0; i < tbl->pos; i++) {
            if (tbl->tbl[i] == id) {
                return i+1;
            }
        }
    }
    return 0;
}


#ifndef RIPPER
typedef struct token_info {
    const char *token;
    int linenum;
    int column;
    int nonspc;
    struct token_info *next;
} token_info;
#endif

/*
    Structure of Lexer Buffer:

 lex_pbeg      tokp         lex_p        lex_pend
    |           |              |            |
    |-----------+--------------+------------|
                |<------------>|
                     token
*/
struct parser_params {
    int is_ripper;
    NODE *heap;

    YYSTYPE *parser_yylval;
    VALUE eofp;

    NODE *parser_lex_strterm;
    enum lex_state_e parser_lex_state;
    stack_type parser_cond_stack;
    stack_type parser_cmdarg_stack;
    int parser_class_nest;
    int parser_paren_nest;
    int parser_lpar_beg;
    int parser_in_single;
    int parser_in_def;
    int parser_brace_nest;
    int parser_compile_for_eval;
    VALUE parser_cur_mid;
    int parser_in_defined;
    char *parser_tokenbuf;
    int parser_tokidx;
    int parser_toksiz;
    int parser_tokline;
    VALUE parser_lex_input;
    VALUE parser_lex_lastline;
    VALUE parser_lex_nextline;
    const char *parser_lex_pbeg;
    const char *parser_lex_p;
    const char *parser_lex_pend;
    int parser_heredoc_end;
    int parser_command_start;
    NODE *parser_deferred_nodes;
    long parser_lex_gets_ptr;
    VALUE (*parser_lex_gets)(struct parser_params*,VALUE);
    struct local_vars *parser_lvtbl;
    int parser_ruby__end__seen;
    int line_count;
    int has_shebang;
    char *parser_ruby_sourcefile; /* current source file */
    int parser_ruby_sourceline;	/* current line no. */
    rb_encoding *enc;

    int parser_yydebug;

#ifndef RIPPER
    /* Ruby core only */
    NODE *parser_eval_tree_begin;
    NODE *parser_eval_tree;
    VALUE debug_lines;
    VALUE coverage;
    int nerr;

    int parser_token_info_enabled;
    token_info *parser_token_info;
#else
    /* Ripper only */
    VALUE parser_ruby_sourcefile_string;
    const char *tokp;
    VALUE delayed;
    int delayed_line;
    int delayed_col;

    VALUE value;
    VALUE result;
    VALUE parsing_thread;
    int toplevel_p;
#endif
};

#define STR_NEW(p,n) rb_enc_str_new((p),(n),current_enc)
#define STR_NEW0() rb_enc_str_new(0,0,current_enc)
#define STR_NEW2(p) rb_enc_str_new((p),strlen(p),current_enc)
#define STR_NEW3(p,n,e,func) parser_str_new((p),(n),(e),(func),current_enc)
#define ENC_SINGLE(cr) ((cr)==ENC_CODERANGE_7BIT)
#define TOK_INTERN(mb) rb_intern3(tok(), toklen(), current_enc)

static int parser_yyerror(struct parser_params*, const char*);
#define yyerror(msg) parser_yyerror(parser, (msg))

#define YYLEX_PARAM parser

#define lex_strterm		(parser->parser_lex_strterm)
#define lex_state		(parser->parser_lex_state)
#define cond_stack		(parser->parser_cond_stack)
#define cmdarg_stack		(parser->parser_cmdarg_stack)
#define class_nest		(parser->parser_class_nest)
#define paren_nest		(parser->parser_paren_nest)
#define lpar_beg		(parser->parser_lpar_beg)
#define brace_nest		(parser->parser_brace_nest)
#define in_single		(parser->parser_in_single)
#define in_def			(parser->parser_in_def)
#define compile_for_eval	(parser->parser_compile_for_eval)
#define cur_mid			(parser->parser_cur_mid)
#define in_defined		(parser->parser_in_defined)
#define tokenbuf		(parser->parser_tokenbuf)
#define tokidx			(parser->parser_tokidx)
#define toksiz			(parser->parser_toksiz)
#define tokline			(parser->parser_tokline)
#define lex_input		(parser->parser_lex_input)
#define lex_lastline		(parser->parser_lex_lastline)
#define lex_nextline		(parser->parser_lex_nextline)
#define lex_pbeg		(parser->parser_lex_pbeg)
#define lex_p			(parser->parser_lex_p)
#define lex_pend		(parser->parser_lex_pend)
#define heredoc_end		(parser->parser_heredoc_end)
#define command_start		(parser->parser_command_start)
#define deferred_nodes		(parser->parser_deferred_nodes)
#define lex_gets_ptr		(parser->parser_lex_gets_ptr)
#define lex_gets		(parser->parser_lex_gets)
#define lvtbl			(parser->parser_lvtbl)
#define ruby__end__seen		(parser->parser_ruby__end__seen)
#define ruby_sourceline		(parser->parser_ruby_sourceline)
#define ruby_sourcefile		(parser->parser_ruby_sourcefile)
#define current_enc		(parser->enc)
#define yydebug			(parser->parser_yydebug)
#ifdef RIPPER
#else
#define ruby_eval_tree		(parser->parser_eval_tree)
#define ruby_eval_tree_begin	(parser->parser_eval_tree_begin)
#define ruby_debug_lines	(parser->debug_lines)
#define ruby_coverage		(parser->coverage)
#endif

static int yylex(void*, void*);

#ifndef RIPPER
#define yyparse ruby_yyparse

static NODE* node_newnode(struct parser_params *, enum node_type, VALUE, VALUE, VALUE);
#define rb_node_newnode(type, a1, a2, a3) node_newnode(parser, (type), (a1), (a2), (a3))

static NODE *cond_gen(struct parser_params*,NODE*);
#define cond(node) cond_gen(parser, (node))
static NODE *logop_gen(struct parser_params*,enum node_type,NODE*,NODE*);
#define logop(type,node1,node2) logop_gen(parser, (type), (node1), (node2))

static NODE *newline_node(NODE*);
static void fixpos(NODE*,NODE*);

static int value_expr_gen(struct parser_params*,NODE*);
static void void_expr_gen(struct parser_params*,NODE*);
static NODE *remove_begin(NODE*);
#define value_expr(node) value_expr_gen(parser, (node) = remove_begin(node))
#define void_expr0(node) void_expr_gen(parser, (node))
#define void_expr(node) void_expr0((node) = remove_begin(node))
static void void_stmts_gen(struct parser_params*,NODE*);
#define void_stmts(node) void_stmts_gen(parser, (node))
static void reduce_nodes_gen(struct parser_params*,NODE**);
#define reduce_nodes(n) reduce_nodes_gen(parser,(n))
static void block_dup_check_gen(struct parser_params*,NODE*,NODE*);
#define block_dup_check(n1,n2) block_dup_check_gen(parser,(n1),(n2))

static NODE *block_append_gen(struct parser_params*,NODE*,NODE*);
#define block_append(h,t) block_append_gen(parser,(h),(t))
static NODE *list_append_gen(struct parser_params*,NODE*,NODE*);
#define list_append(l,i) list_append_gen(parser,(l),(i))
static NODE *list_concat_gen(struct parser_params*,NODE*,NODE*);
#define list_concat(h,t) list_concat_gen(parser,(h),(t))
static NODE *arg_append_gen(struct parser_params*,NODE*,NODE*);
#define arg_append(h,t) arg_append_gen(parser,(h),(t))
static NODE *arg_concat_gen(struct parser_params*,NODE*,NODE*);
#define arg_concat(h,t) arg_concat_gen(parser,(h),(t))
static NODE *literal_concat_gen(struct parser_params*,NODE*,NODE*);
#define literal_concat(h,t) literal_concat_gen(parser,(h),(t))
static int literal_concat0(struct parser_params *, VALUE, VALUE);
static NODE *new_evstr_gen(struct parser_params*,NODE*);
#define new_evstr(n) new_evstr_gen(parser,(n))
static NODE *evstr2dstr_gen(struct parser_params*,NODE*);
#define evstr2dstr(n) evstr2dstr_gen(parser,(n))
static NODE *splat_array(NODE*);

static NODE *call_bin_op_gen(struct parser_params*,NODE*,ID,NODE*);
#define call_bin_op(recv,id,arg1) call_bin_op_gen(parser, (recv),(id),(arg1))
static NODE *call_uni_op_gen(struct parser_params*,NODE*,ID);
#define call_uni_op(recv,id) call_uni_op_gen(parser, (recv),(id))

static NODE *new_args_gen(struct parser_params*,NODE*,NODE*,ID,NODE*,NODE*);
#define new_args(f,o,r,p,t) new_args_gen(parser, (f),(o),(r),(p),(t))
static NODE *new_args_tail_gen(struct parser_params*,NODE*,ID,ID);
#define new_args_tail(k,kr,b) new_args_tail_gen(parser, (k),(kr),(b))

static NODE *negate_lit(NODE*);
static NODE *ret_args_gen(struct parser_params*,NODE*);
#define ret_args(node) ret_args_gen(parser, (node))
static NODE *arg_blk_pass(NODE*,NODE*);
static NODE *new_yield_gen(struct parser_params*,NODE*);
#define new_yield(node) new_yield_gen(parser, (node))
static NODE *dsym_node_gen(struct parser_params*,NODE*);
#define dsym_node(node) dsym_node_gen(parser, (node))

static NODE *gettable_gen(struct parser_params*,ID);
#define gettable(id) gettable_gen(parser,(id))
static NODE *assignable_gen(struct parser_params*,ID,NODE*);
#define assignable(id,node) assignable_gen(parser, (id), (node))

static NODE *aryset_gen(struct parser_params*,NODE*,NODE*);
#define aryset(node1,node2) aryset_gen(parser, (node1), (node2))
static NODE *attrset_gen(struct parser_params*,NODE*,ID);
#define attrset(node,id) attrset_gen(parser, (node), (id))

static void rb_backref_error_gen(struct parser_params*,NODE*);
#define rb_backref_error(n) rb_backref_error_gen(parser,(n))
static NODE *node_assign_gen(struct parser_params*,NODE*,NODE*);
#define node_assign(node1, node2) node_assign_gen(parser, (node1), (node2))

static NODE *new_op_assign_gen(struct parser_params *parser, NODE *lhs, ID op, NODE *rhs);
static NODE *new_attr_op_assign_gen(struct parser_params *parser, NODE *lhs, ID attr, ID op, NODE *rhs);
#define new_attr_op_assign(lhs, type, attr, op, rhs) new_attr_op_assign_gen(parser, (lhs), (attr), (op), (rhs))
static NODE *new_const_op_assign_gen(struct parser_params *parser, NODE *lhs, ID op, NODE *rhs);
#define new_const_op_assign(lhs, op, rhs) new_const_op_assign_gen(parser, (lhs), (op), (rhs))

static NODE *match_op_gen(struct parser_params*,NODE*,NODE*);
#define match_op(node1,node2) match_op_gen(parser, (node1), (node2))

static ID  *local_tbl_gen(struct parser_params*);
#define local_tbl() local_tbl_gen(parser)

static void fixup_nodes(NODE **);

static VALUE reg_compile_gen(struct parser_params*, VALUE, int);
#define reg_compile(str,options) reg_compile_gen(parser, (str), (options))
static void reg_fragment_setenc_gen(struct parser_params*, VALUE, int);
#define reg_fragment_setenc(str,options) reg_fragment_setenc_gen(parser, (str), (options))
static int reg_fragment_check_gen(struct parser_params*, VALUE, int);
#define reg_fragment_check(str,options) reg_fragment_check_gen(parser, (str), (options))
static NODE *reg_named_capture_assign_gen(struct parser_params* parser, VALUE regexp, NODE *match);
#define reg_named_capture_assign(regexp,match) reg_named_capture_assign_gen(parser,(regexp),(match))

#define get_id(id) (id)
#define get_value(val) (val)
#else
#define value_expr(node) ((void)(node))
#define remove_begin(node) (node)
#define rb_dvar_defined(id) 0
#define rb_local_defined(id) 0
static ID ripper_get_id(VALUE);
#define get_id(id) ripper_get_id(id)
static VALUE ripper_get_value(VALUE);
#define get_value(val) ripper_get_value(val)
static VALUE assignable_gen(struct parser_params*,VALUE);
#define assignable(lhs,node) assignable_gen(parser, (lhs))
static int id_is_var_gen(struct parser_params *parser, ID id);
#define id_is_var(id) id_is_var_gen(parser, (id))

#define node_assign(node1, node2) dispatch2(assign, (node1), (node2))

static VALUE new_op_assign_gen(struct parser_params *parser, VALUE lhs, VALUE op, VALUE rhs);
static VALUE new_attr_op_assign_gen(struct parser_params *parser, VALUE lhs, VALUE type, VALUE attr, VALUE op, VALUE rhs);
#define new_attr_op_assign(lhs, type, attr, op, rhs) new_attr_op_assign_gen(parser, (lhs), (type), (attr), (op), (rhs))

#endif /* !RIPPER */

#define new_op_assign(lhs, op, rhs) new_op_assign_gen(parser, (lhs), (op), (rhs))

static ID formal_argument_gen(struct parser_params*, ID);
#define formal_argument(id) formal_argument_gen(parser, (id))
static ID shadowing_lvar_gen(struct parser_params*,ID);
#define shadowing_lvar(name) shadowing_lvar_gen(parser, (name))
static void new_bv_gen(struct parser_params*,ID);
#define new_bv(id) new_bv_gen(parser, (id))

static void local_push_gen(struct parser_params*,int);
#define local_push(top) local_push_gen(parser,(top))
static void local_pop_gen(struct parser_params*);
#define local_pop() local_pop_gen(parser)
static int local_var_gen(struct parser_params*, ID);
#define local_var(id) local_var_gen(parser, (id))
static int arg_var_gen(struct parser_params*, ID);
#define arg_var(id) arg_var_gen(parser, (id))
static int  local_id_gen(struct parser_params*, ID);
#define local_id(id) local_id_gen(parser, (id))
static ID   internal_id_gen(struct parser_params*);
#define internal_id() internal_id_gen(parser)

static const struct vtable *dyna_push_gen(struct parser_params *);
#define dyna_push() dyna_push_gen(parser)
static void dyna_pop_gen(struct parser_params*, const struct vtable *);
#define dyna_pop(node) dyna_pop_gen(parser, (node))
static int dyna_in_block_gen(struct parser_params*);
#define dyna_in_block() dyna_in_block_gen(parser)
#define dyna_var(id) local_var(id)
static int dvar_defined_gen(struct parser_params*,ID,int);
#define dvar_defined(id) dvar_defined_gen(parser, (id), 0)
#define dvar_defined_get(id) dvar_defined_gen(parser, (id), 1)
static int dvar_curr_gen(struct parser_params*,ID);
#define dvar_curr(id) dvar_curr_gen(parser, (id))

static int lvar_defined_gen(struct parser_params*, ID);
#define lvar_defined(id) lvar_defined_gen(parser, (id))

#define RE_OPTION_ONCE (1<<16)
#define RE_OPTION_ENCODING_SHIFT 8
#define RE_OPTION_ENCODING(e) (((e)&0xff)<<RE_OPTION_ENCODING_SHIFT)
#define RE_OPTION_ENCODING_IDX(o) (((o)>>RE_OPTION_ENCODING_SHIFT)&0xff)
#define RE_OPTION_ENCODING_NONE(o) ((o)&RE_OPTION_ARG_ENCODING_NONE)
#define RE_OPTION_MASK  0xff
#define RE_OPTION_ARG_ENCODING_NONE 32

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
#define nd_nest u3.cnt

/****** Ripper *******/

#ifdef RIPPER
#define RIPPER_VERSION "0.1.0"

#include "eventids1.c"
#include "eventids2.c"

static VALUE ripper_dispatch0(struct parser_params*,ID);
static VALUE ripper_dispatch1(struct parser_params*,ID,VALUE);
static VALUE ripper_dispatch2(struct parser_params*,ID,VALUE,VALUE);
static VALUE ripper_dispatch3(struct parser_params*,ID,VALUE,VALUE,VALUE);
static VALUE ripper_dispatch4(struct parser_params*,ID,VALUE,VALUE,VALUE,VALUE);
static VALUE ripper_dispatch5(struct parser_params*,ID,VALUE,VALUE,VALUE,VALUE,VALUE);
static VALUE ripper_dispatch7(struct parser_params*,ID,VALUE,VALUE,VALUE,VALUE,VALUE,VALUE,VALUE);

#define dispatch0(n)            ripper_dispatch0(parser, TOKEN_PASTE(ripper_id_, n))
#define dispatch1(n,a)          ripper_dispatch1(parser, TOKEN_PASTE(ripper_id_, n), (a))
#define dispatch2(n,a,b)        ripper_dispatch2(parser, TOKEN_PASTE(ripper_id_, n), (a), (b))
#define dispatch3(n,a,b,c)      ripper_dispatch3(parser, TOKEN_PASTE(ripper_id_, n), (a), (b), (c))
#define dispatch4(n,a,b,c,d)    ripper_dispatch4(parser, TOKEN_PASTE(ripper_id_, n), (a), (b), (c), (d))
#define dispatch5(n,a,b,c,d,e)  ripper_dispatch5(parser, TOKEN_PASTE(ripper_id_, n), (a), (b), (c), (d), (e))
#define dispatch7(n,a,b,c,d,e,f,g) ripper_dispatch7(parser, TOKEN_PASTE(ripper_id_, n), (a), (b), (c), (d), (e), (f), (g))

#define yyparse ripper_yyparse

#define ripper_intern(s) ID2SYM(rb_intern(s))
static VALUE ripper_id2sym(ID);
#ifdef __GNUC__
#define ripper_id2sym(id) ((id) < 256 && rb_ispunct(id) ? \
			   ID2SYM(id) : ripper_id2sym(id))
#endif

#define arg_new() dispatch0(args_new)
#define arg_add(l,a) dispatch2(args_add, (l), (a))
#define arg_add_star(l,a) dispatch2(args_add_star, (l), (a))
#define arg_add_block(l,b) dispatch2(args_add_block, (l), (b))
#define arg_add_optblock(l,b) ((b)==Qundef? (l) : dispatch2(args_add_block, (l), (b)))
#define bare_assoc(v) dispatch1(bare_assoc_hash, (v))
#define arg_add_assocs(l,b) arg_add((l), bare_assoc(b))

#define args2mrhs(a) dispatch1(mrhs_new_from_args, (a))
#define mrhs_new() dispatch0(mrhs_new)
#define mrhs_add(l,a) dispatch2(mrhs_add, (l), (a))
#define mrhs_add_star(l,a) dispatch2(mrhs_add_star, (l), (a))

#define mlhs_new() dispatch0(mlhs_new)
#define mlhs_add(l,a) dispatch2(mlhs_add, (l), (a))
#define mlhs_add_star(l,a) dispatch2(mlhs_add_star, (l), (a))

#define params_new(pars, opts, rest, pars2, kws, kwrest, blk) \
        dispatch7(params, (pars), (opts), (rest), (pars2), (kws), (kwrest), (blk))

#define blockvar_new(p,v) dispatch2(block_var, (p), (v))
#define blockvar_add_star(l,a) dispatch2(block_var_add_star, (l), (a))
#define blockvar_add_block(l,a) dispatch2(block_var_add_block, (l), (a))

#define method_optarg(m,a) ((a)==Qundef ? (m) : dispatch2(method_add_arg,(m),(a)))
#define method_arg(m,a) dispatch2(method_add_arg,(m),(a))
#define method_add_block(m,b) dispatch2(method_add_block, (m), (b))

#define escape_Qundef(x) ((x)==Qundef ? Qnil : (x))

static inline VALUE
new_args_gen(struct parser_params *parser, VALUE f, VALUE o, VALUE r, VALUE p, VALUE tail)
{
    NODE *t = (NODE *)tail;
    VALUE k = t->u1.value, kr = t->u2.value, b = t->u3.value;
    return params_new(f, o, r, p, k, kr, escape_Qundef(b));
}
#define new_args(f,o,r,p,t) new_args_gen(parser, (f),(o),(r),(p),(t))

static inline VALUE
new_args_tail_gen(struct parser_params *parser, VALUE k, VALUE kr, VALUE b)
{
    return (VALUE)rb_node_newnode(NODE_MEMO, k, kr, b);
}
#define new_args_tail(k,kr,b) new_args_tail_gen(parser, (k),(kr),(b))

#define FIXME 0

#endif /* RIPPER */

#ifndef RIPPER
# define Qnone 0
# define ifndef_ripper(x) (x)
#else
# define Qnone Qnil
# define ifndef_ripper(x)
#endif

#ifndef RIPPER
# define rb_warn0(fmt)    rb_compile_warn(ruby_sourcefile, ruby_sourceline, (fmt))
# define rb_warnI(fmt,a)  rb_compile_warn(ruby_sourcefile, ruby_sourceline, (fmt), (a))
# define rb_warnS(fmt,a)  rb_compile_warn(ruby_sourcefile, ruby_sourceline, (fmt), (a))
# define rb_warn4S(file,line,fmt,a)  rb_compile_warn((file), (line), (fmt), (a))
# define rb_warning0(fmt) rb_compile_warning(ruby_sourcefile, ruby_sourceline, (fmt))
# define rb_warningS(fmt,a) rb_compile_warning(ruby_sourcefile, ruby_sourceline, (fmt), (a))
#else
# define rb_warn0(fmt)    ripper_warn0(parser, (fmt))
# define rb_warnI(fmt,a)  ripper_warnI(parser, (fmt), (a))
# define rb_warnS(fmt,a)  ripper_warnS(parser, (fmt), (a))
# define rb_warn4S(file,line,fmt,a)  ripper_warnS(parser, (fmt), (a))
# define rb_warning0(fmt) ripper_warning0(parser, (fmt))
# define rb_warningS(fmt,a) ripper_warningS(parser, (fmt), (a))
static void ripper_warn0(struct parser_params*, const char*);
static void ripper_warnI(struct parser_params*, const char*, int);
static void ripper_warnS(struct parser_params*, const char*, const char*);
static void ripper_warning0(struct parser_params*, const char*);
static void ripper_warningS(struct parser_params*, const char*, const char*);
#endif

#ifdef RIPPER
static void ripper_compile_error(struct parser_params*, const char *fmt, ...);
# define rb_compile_error ripper_compile_error
# define compile_error ripper_compile_error
# define PARSER_ARG parser,
#else
# define rb_compile_error rb_compile_error_with_enc
# define compile_error parser->nerr++,rb_compile_error_with_enc
# define PARSER_ARG ruby_sourcefile, ruby_sourceline, current_enc,
#endif

/* Older versions of Yacc set YYMAXDEPTH to a very low value by default (150,
   for instance).  This is too low for Ruby to parse some files, such as
   date/format.rb, therefore bump the value up to at least Bison's default. */
#ifdef OLD_YACC
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif
#endif

#ifndef RIPPER
static void token_info_push(struct parser_params*, const char *token);
static void token_info_pop(struct parser_params*, const char *token);
#define token_info_push(token) (RTEST(ruby_verbose) ? token_info_push(parser, (token)) : (void)0)
#define token_info_pop(token) (RTEST(ruby_verbose) ? token_info_pop(parser, (token)) : (void)0)
#else
#define token_info_push(token) /* nothing */
#define token_info_pop(token) /* nothing */
#endif


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
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
#line 686 "ripper.y"
{
    VALUE val;
    NODE *node;
    ID id;
    int num;
    const struct vtable *vars;
}
/* Line 187 of yacc.c.  */
#line 1005 "parse.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 1018 "parse.c"

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
#define YYLAST   10982

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  142
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  197
/* YYNRULES -- Number of rules.  */
#define YYNRULES  618
/* YYNRULES -- Number of states.  */
#define YYNSTATES  1055

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   352

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     141,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,   140,   127,     2,     2,     2,   125,   120,     2,
     136,   137,   123,   121,   134,   122,   133,   124,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   115,   139,
     117,   113,   116,   114,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   132,     2,   138,   119,     2,   135,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   130,   118,   131,   128,     2,    79,    80,
      66,    67,    68,     2,    69,    83,    84,    74,    73,    70,
      71,    72,    77,    78,    81,    82,     2,     2,     2,     2,
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
      65,    75,    76,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   126,   129
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     4,     7,    10,    12,    14,    18,    21,
      23,    24,    30,    35,    38,    40,    42,    46,    49,    51,
      52,    58,    59,    64,    68,    72,    76,    79,    83,    87,
      91,    95,    99,   104,   106,   110,   114,   121,   127,   133,
     139,   145,   149,   153,   157,   161,   163,   167,   171,   173,
     177,   181,   185,   188,   190,   192,   194,   196,   198,   203,
     204,   210,   212,   215,   219,   224,   230,   235,   241,   244,
     247,   250,   253,   256,   258,   262,   264,   268,   270,   273,
     277,   283,   286,   291,   294,   299,   301,   305,   307,   311,
     314,   318,   320,   324,   326,   328,   333,   337,   341,   345,
     349,   352,   354,   356,   358,   363,   367,   371,   375,   379,
     382,   384,   386,   388,   391,   393,   397,   399,   401,   403,
     405,   407,   409,   411,   413,   415,   417,   418,   423,   425,
     427,   429,   431,   433,   435,   437,   439,   441,   443,   445,
     447,   449,   451,   453,   455,   457,   459,   461,   463,   465,
     467,   469,   471,   473,   475,   477,   479,   481,   483,   485,
     487,   489,   491,   493,   495,   497,   499,   501,   503,   505,
     507,   509,   511,   513,   515,   517,   519,   521,   523,   525,
     527,   529,   531,   533,   535,   537,   539,   541,   543,   545,
     547,   549,   551,   553,   555,   557,   559,   561,   563,   565,
     569,   575,   579,   585,   592,   598,   604,   610,   616,   621,
     625,   629,   633,   637,   641,   645,   649,   653,   657,   662,
     667,   670,   673,   677,   681,   685,   689,   693,   697,   701,
     705,   709,   713,   717,   721,   725,   728,   731,   735,   739,
     743,   747,   748,   753,   760,   762,   764,   766,   769,   774,
     777,   781,   783,   785,   787,   789,   792,   797,   800,   802,
     805,   808,   813,   815,   816,   819,   822,   825,   827,   829,
     832,   836,   841,   845,   850,   853,   855,   857,   859,   861,
     863,   865,   867,   869,   871,   873,   875,   876,   881,   882,
     886,   887,   892,   896,   900,   903,   907,   911,   913,   918,
     922,   924,   925,   932,   937,   941,   944,   946,   949,   952,
     959,   966,   967,   968,   976,   977,   978,   986,   992,   997,
     998,   999,  1009,  1010,  1017,  1018,  1019,  1028,  1029,  1035,
    1036,  1043,  1044,  1045,  1055,  1057,  1059,  1061,  1063,  1065,
    1067,  1069,  1071,  1073,  1075,  1077,  1079,  1081,  1083,  1085,
    1087,  1089,  1091,  1094,  1096,  1098,  1100,  1106,  1108,  1111,
    1113,  1115,  1117,  1121,  1123,  1127,  1129,  1134,  1141,  1145,
    1151,  1154,  1159,  1161,  1165,  1170,  1173,  1176,  1178,  1181,
    1182,  1189,  1198,  1203,  1210,  1215,  1218,  1225,  1228,  1233,
    1240,  1243,  1248,  1251,  1256,  1258,  1260,  1262,  1266,  1268,
    1273,  1275,  1280,  1282,  1286,  1288,  1290,  1291,  1292,  1297,
    1302,  1304,  1308,  1312,  1313,  1319,  1322,  1327,  1333,  1339,
    1342,  1343,  1349,  1350,  1356,  1360,  1361,  1366,  1367,  1372,
    1375,  1377,  1382,  1383,  1389,  1390,  1396,  1402,  1404,  1406,
    1413,  1415,  1417,  1419,  1421,  1424,  1426,  1429,  1431,  1433,
    1435,  1437,  1439,  1441,  1443,  1446,  1450,  1454,  1458,  1462,
    1466,  1467,  1471,  1473,  1476,  1480,  1484,  1485,  1489,  1493,
    1497,  1501,  1505,  1506,  1510,  1511,  1515,  1516,  1519,  1520,
    1523,  1524,  1527,  1529,  1530,  1534,  1535,  1536,  1537,  1544,
    1546,  1548,  1550,  1552,  1555,  1557,  1559,  1561,  1563,  1567,
    1569,  1571,  1574,  1577,  1579,  1581,  1583,  1585,  1587,  1589,
    1591,  1593,  1595,  1597,  1599,  1601,  1603,  1605,  1607,  1609,
    1611,  1613,  1615,  1616,  1621,  1624,  1628,  1631,  1636,  1639,
    1642,  1644,  1647,  1648,  1655,  1664,  1669,  1676,  1681,  1688,
    1691,  1696,  1703,  1706,  1711,  1714,  1719,  1721,  1722,  1724,
    1726,  1728,  1730,  1732,  1734,  1736,  1740,  1742,  1746,  1749,
    1752,  1754,  1758,  1760,  1764,  1766,  1768,  1771,  1773,  1777,
    1781,  1783,  1787,  1789,  1793,  1795,  1797,  1800,  1802,  1804,
    1806,  1809,  1812,  1814,  1816,  1817,  1822,  1824,  1827,  1829,
    1833,  1837,  1840,  1843,  1845,  1847,  1849,  1851,  1853,  1855,
    1857,  1859,  1861,  1863,  1865,  1867,  1868,  1870,  1871,  1873,
    1876,  1879,  1880,  1882,  1884,  1886,  1888,  1890,  1893
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     143,     0,    -1,    -1,   144,   145,    -1,   146,   331,    -1,
     338,    -1,   147,    -1,   146,   337,   147,    -1,     1,   147,
      -1,   154,    -1,    -1,    47,   148,   130,   145,   131,    -1,
     150,   260,   229,   263,    -1,   151,   331,    -1,   338,    -1,
     152,    -1,   151,   337,   152,    -1,     1,   154,    -1,   154,
      -1,    -1,    47,   153,   130,   145,   131,    -1,    -1,    45,
     177,   155,   177,    -1,    45,    54,    54,    -1,    45,    54,
      64,    -1,    45,    54,    63,    -1,     6,   178,    -1,   154,
      40,   158,    -1,   154,    41,   158,    -1,   154,    42,   158,
      -1,   154,    43,   158,    -1,   154,    44,   154,    -1,    48,
     130,   150,   131,    -1,   156,    -1,   165,   113,   159,    -1,
     295,    87,   159,    -1,   214,   132,   188,   334,    87,   159,
      -1,   214,   133,    52,    87,   159,    -1,   214,   133,    56,
      87,   159,    -1,   214,    85,    56,    87,   159,    -1,   214,
      85,    52,    87,   159,    -1,   296,    87,   159,    -1,   172,
     113,   195,    -1,   165,   113,   184,    -1,   165,   113,   195,
      -1,   157,    -1,   172,   113,   159,    -1,   172,   113,   156,
      -1,   159,    -1,   157,    37,   157,    -1,   157,    38,   157,
      -1,    39,   332,   157,    -1,   127,   159,    -1,   182,    -1,
     157,    -1,   164,    -1,   160,    -1,   249,    -1,   249,   330,
     328,   190,    -1,    -1,    94,   162,   237,   150,   131,    -1,
     327,    -1,   163,   190,    -1,   163,   190,   161,    -1,   214,
     133,   328,   190,    -1,   214,   133,   328,   190,   161,    -1,
     214,    85,   328,   190,    -1,   214,    85,   328,   190,   161,
      -1,    32,   190,    -1,    31,   190,    -1,    30,   189,    -1,
      21,   189,    -1,    22,   189,    -1,   167,    -1,    89,   166,
     333,    -1,   167,    -1,    89,   166,   333,    -1,   169,    -1,
     169,   168,    -1,   169,    95,   171,    -1,   169,    95,   171,
     134,   170,    -1,   169,    95,    -1,   169,    95,   134,   170,
      -1,    95,   171,    -1,    95,   171,   134,   170,    -1,    95,
      -1,    95,   134,   170,    -1,   171,    -1,    89,   166,   333,
      -1,   168,   134,    -1,   169,   168,   134,    -1,   168,    -1,
     170,   134,   168,    -1,   292,    -1,   293,    -1,   214,   132,
     188,   334,    -1,   214,   133,    52,    -1,   214,    85,    52,
      -1,   214,   133,    56,    -1,   214,    85,    56,    -1,    86,
      56,    -1,   296,    -1,   292,    -1,   293,    -1,   214,   132,
     188,   334,    -1,   214,   133,    52,    -1,   214,    85,    52,
      -1,   214,   133,    56,    -1,   214,    85,    56,    -1,    86,
      56,    -1,   296,    -1,    52,    -1,    56,    -1,    86,   173,
      -1,   173,    -1,   214,    85,   173,    -1,    52,    -1,    56,
      -1,    53,    -1,   180,    -1,   181,    -1,   175,    -1,   288,
      -1,   176,    -1,   290,    -1,   177,    -1,    -1,   178,   134,
     179,   177,    -1,   118,    -1,   119,    -1,   120,    -1,    69,
      -1,    70,    -1,    71,    -1,    77,    -1,    78,    -1,   116,
      -1,    73,    -1,   117,    -1,    74,    -1,    72,    -1,    83,
      -1,    84,    -1,   121,    -1,   122,    -1,   123,    -1,    95,
      -1,   124,    -1,   125,    -1,    68,    -1,    96,    -1,   127,
      -1,   128,    -1,    66,    -1,    67,    -1,    81,    -1,    82,
      -1,   135,    -1,    49,    -1,    50,    -1,    51,    -1,    47,
      -1,    48,    -1,    45,    -1,    37,    -1,     7,    -1,    21,
      -1,    16,    -1,     3,    -1,     5,    -1,    46,    -1,    26,
      -1,    15,    -1,    14,    -1,    10,    -1,     9,    -1,    36,
      -1,    20,    -1,    25,    -1,     4,    -1,    22,    -1,    34,
      -1,    39,    -1,    38,    -1,    23,    -1,     8,    -1,    24,
      -1,    30,    -1,    33,    -1,    32,    -1,    13,    -1,    35,
      -1,     6,    -1,    17,    -1,    31,    -1,    11,    -1,    12,
      -1,    18,    -1,    19,    -1,   172,   113,   182,    -1,   172,
     113,   182,    44,   182,    -1,   295,    87,   182,    -1,   295,
      87,   182,    44,   182,    -1,   214,   132,   188,   334,    87,
     182,    -1,   214,   133,    52,    87,   182,    -1,   214,   133,
      56,    87,   182,    -1,   214,    85,    52,    87,   182,    -1,
     214,    85,    56,    87,   182,    -1,    86,    56,    87,   182,
      -1,   296,    87,   182,    -1,   182,    79,   182,    -1,   182,
      80,   182,    -1,   182,   121,   182,    -1,   182,   122,   182,
      -1,   182,   123,   182,    -1,   182,   124,   182,    -1,   182,
     125,   182,    -1,   182,    68,   182,    -1,   126,    59,    68,
     182,    -1,   126,    60,    68,   182,    -1,    66,   182,    -1,
      67,   182,    -1,   182,   118,   182,    -1,   182,   119,   182,
      -1,   182,   120,   182,    -1,   182,    69,   182,    -1,   182,
     116,   182,    -1,   182,    73,   182,    -1,   182,   117,   182,
      -1,   182,    74,   182,    -1,   182,    70,   182,    -1,   182,
      71,   182,    -1,   182,    72,   182,    -1,   182,    77,   182,
      -1,   182,    78,   182,    -1,   127,   182,    -1,   128,   182,
      -1,   182,    83,   182,    -1,   182,    84,   182,    -1,   182,
      75,   182,    -1,   182,    76,   182,    -1,    -1,    46,   332,
     183,   182,    -1,   182,   114,   182,   332,   115,   182,    -1,
     196,    -1,   182,    -1,   338,    -1,   194,   335,    -1,   194,
     134,   325,   335,    -1,   325,   335,    -1,   136,   188,   333,
      -1,   338,    -1,   186,    -1,   338,    -1,   189,    -1,   194,
     134,    -1,   194,   134,   325,   134,    -1,   325,   134,    -1,
     164,    -1,   194,   193,    -1,   325,   193,    -1,   194,   134,
     325,   193,    -1,   192,    -1,    -1,   191,   189,    -1,    97,
     184,    -1,   134,   192,    -1,   338,    -1,   184,    -1,    95,
     184,    -1,   194,   134,   184,    -1,   194,   134,    95,   184,
      -1,   194,   134,   184,    -1,   194,   134,    95,   184,    -1,
      95,   184,    -1,   264,    -1,   265,    -1,   268,    -1,   269,
      -1,   270,    -1,   275,    -1,   273,    -1,   276,    -1,   294,
      -1,   296,    -1,    53,    -1,    -1,   215,   197,   149,   225,
      -1,    -1,    90,   198,   333,    -1,    -1,    90,   157,   199,
     333,    -1,    89,   150,   137,    -1,   214,    85,    56,    -1,
      86,    56,    -1,    92,   185,   138,    -1,    93,   324,   131,
      -1,    30,    -1,    31,   136,   189,   333,    -1,    31,   136,
     333,    -1,    31,    -1,    -1,    46,   332,   136,   200,   157,
     333,    -1,    39,   136,   157,   333,    -1,    39,   136,   333,
      -1,   163,   255,    -1,   250,    -1,   250,   255,    -1,    98,
     242,    -1,   216,   158,   226,   150,   228,   225,    -1,   217,
     158,   226,   150,   229,   225,    -1,    -1,    -1,   218,   201,
     158,   227,   202,   150,   225,    -1,    -1,    -1,   219,   203,
     158,   227,   204,   150,   225,    -1,   220,   158,   331,   258,
     225,    -1,   220,   331,   258,   225,    -1,    -1,    -1,   221,
     230,    25,   205,   158,   227,   206,   150,   225,    -1,    -1,
     222,   174,   297,   207,   149,   225,    -1,    -1,    -1,   222,
      83,   157,   208,   336,   209,   149,   225,    -1,    -1,   223,
     174,   210,   149,   225,    -1,    -1,   224,   175,   211,   299,
     149,   225,    -1,    -1,    -1,   224,   322,   330,   212,   175,
     213,   299,   149,   225,    -1,    21,    -1,    22,    -1,    23,
      -1,    24,    -1,   196,    -1,     7,    -1,    11,    -1,    12,
      -1,    18,    -1,    19,    -1,    16,    -1,    20,    -1,     3,
      -1,     4,    -1,     5,    -1,    10,    -1,   336,    -1,    13,
      -1,   336,    13,    -1,   336,    -1,    27,    -1,   229,    -1,
      14,   158,   226,   150,   228,    -1,   338,    -1,    15,   150,
      -1,   172,    -1,   165,    -1,   304,    -1,    89,   233,   333,
      -1,   231,    -1,   232,   134,   231,    -1,   232,    -1,   232,
     134,    95,   304,    -1,   232,   134,    95,   304,   134,   232,
      -1,   232,   134,    95,    -1,   232,   134,    95,   134,   232,
      -1,    95,   304,    -1,    95,   304,   134,   232,    -1,    95,
      -1,    95,   134,   232,    -1,   309,   134,   312,   321,    -1,
     309,   321,    -1,   312,   321,    -1,   320,    -1,   134,   234,
      -1,    -1,   306,   134,   315,   134,   318,   235,    -1,   306,
     134,   315,   134,   318,   134,   306,   235,    -1,   306,   134,
     315,   235,    -1,   306,   134,   315,   134,   306,   235,    -1,
     306,   134,   318,   235,    -1,   306,   134,    -1,   306,   134,
     318,   134,   306,   235,    -1,   306,   235,    -1,   315,   134,
     318,   235,    -1,   315,   134,   318,   134,   306,   235,    -1,
     315,   235,    -1,   315,   134,   306,   235,    -1,   318,   235,
      -1,   318,   134,   306,   235,    -1,   234,    -1,   338,    -1,
     238,    -1,   118,   239,   118,    -1,    76,    -1,   118,   236,
     239,   118,    -1,   332,    -1,   332,   139,   240,   332,    -1,
     241,    -1,   240,   134,   241,    -1,    52,    -1,   303,    -1,
      -1,    -1,   243,   244,   245,   246,    -1,   136,   302,   239,
     137,    -1,   302,    -1,   111,   150,   131,    -1,    29,   150,
      10,    -1,    -1,    28,   248,   237,   150,    10,    -1,   164,
     247,    -1,   249,   330,   328,   187,    -1,   249,   330,   328,
     187,   255,    -1,   249,   330,   328,   190,   247,    -1,   163,
     186,    -1,    -1,   214,   133,   328,   251,   187,    -1,    -1,
     214,    85,   328,   252,   186,    -1,   214,    85,   329,    -1,
      -1,   214,   133,   253,   186,    -1,    -1,   214,    85,   254,
     186,    -1,    32,   186,    -1,    32,    -1,   214,   132,   188,
     334,    -1,    -1,   130,   256,   237,   150,   131,    -1,    -1,
      26,   257,   237,   150,    10,    -1,    17,   194,   226,   150,
     259,    -1,   229,    -1,   258,    -1,     8,   261,   262,   226,
     150,   260,    -1,   338,    -1,   184,    -1,   195,    -1,   338,
      -1,    88,   172,    -1,   338,    -1,     9,   150,    -1,   338,
      -1,   291,    -1,   288,    -1,   290,    -1,   266,    -1,    62,
      -1,   267,    -1,   266,   267,    -1,   100,   279,   110,    -1,
     101,   280,   110,    -1,   102,   281,    65,    -1,   103,   140,
     110,    -1,   103,   271,   110,    -1,    -1,   271,   272,   140,
      -1,   282,    -1,   272,   282,    -1,   105,   140,   110,    -1,
     105,   274,   110,    -1,    -1,   274,   272,   140,    -1,   104,
     140,   110,    -1,   104,   277,   110,    -1,   106,   140,   110,
      -1,   106,   278,   110,    -1,    -1,   277,    61,   140,    -1,
      -1,   278,    61,   140,    -1,    -1,   279,   282,    -1,    -1,
     280,   282,    -1,    -1,   281,   282,    -1,    61,    -1,    -1,
     109,   283,   287,    -1,    -1,    -1,    -1,   107,   284,   285,
     286,   150,   108,    -1,    54,    -1,    55,    -1,    57,    -1,
     296,    -1,    99,   289,    -1,   175,    -1,    55,    -1,    54,
      -1,    57,    -1,    99,   280,   110,    -1,    59,    -1,    60,
      -1,   126,    59,    -1,   126,    60,    -1,    52,    -1,    55,
      -1,    54,    -1,    56,    -1,    57,    -1,    34,    -1,    33,
      -1,    35,    -1,    36,    -1,    50,    -1,    49,    -1,    51,
      -1,   292,    -1,   293,    -1,   292,    -1,   293,    -1,    63,
      -1,    64,    -1,   336,    -1,    -1,   117,   298,   158,   336,
      -1,     1,   336,    -1,   136,   302,   333,    -1,   302,   336,
      -1,   310,   134,   312,   321,    -1,   310,   321,    -1,   312,
     321,    -1,   320,    -1,   134,   300,    -1,    -1,   306,   134,
     316,   134,   318,   301,    -1,   306,   134,   316,   134,   318,
     134,   306,   301,    -1,   306,   134,   316,   301,    -1,   306,
     134,   316,   134,   306,   301,    -1,   306,   134,   318,   301,
      -1,   306,   134,   318,   134,   306,   301,    -1,   306,   301,
      -1,   316,   134,   318,   301,    -1,   316,   134,   318,   134,
     306,   301,    -1,   316,   301,    -1,   316,   134,   306,   301,
      -1,   318,   301,    -1,   318,   134,   306,   301,    -1,   300,
      -1,    -1,    56,    -1,    55,    -1,    54,    -1,    57,    -1,
     303,    -1,    52,    -1,   304,    -1,    89,   233,   333,    -1,
     305,    -1,   306,   134,   305,    -1,    58,   184,    -1,    58,
     214,    -1,   308,    -1,   309,   134,   308,    -1,   307,    -1,
     310,   134,   307,    -1,    68,    -1,    96,    -1,   311,    52,
      -1,   311,    -1,    52,   113,   184,    -1,    52,   113,   214,
      -1,   314,    -1,   315,   134,   314,    -1,   313,    -1,   316,
     134,   313,    -1,   123,    -1,    95,    -1,   317,    52,    -1,
     317,    -1,   120,    -1,    97,    -1,   319,    52,    -1,   134,
     320,    -1,   338,    -1,   294,    -1,    -1,   136,   323,   157,
     333,    -1,   338,    -1,   325,   335,    -1,   326,    -1,   325,
     134,   326,    -1,   184,    88,   184,    -1,    58,   184,    -1,
      96,   184,    -1,    52,    -1,    56,    -1,    53,    -1,    52,
      -1,    56,    -1,    53,    -1,   180,    -1,    52,    -1,    53,
      -1,   180,    -1,   133,    -1,    85,    -1,    -1,   337,    -1,
      -1,   141,    -1,   332,   137,    -1,   332,   138,    -1,    -1,
     141,    -1,   134,    -1,   139,    -1,   141,    -1,   336,    -1,
     337,   139,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   850,   850,   850,   881,   892,   901,   909,   917,   923,
     925,   924,   945,   978,   989,   998,  1006,  1014,  1020,  1025,
    1024,  1045,  1045,  1053,  1061,  1072,  1082,  1090,  1099,  1108,
    1121,  1134,  1143,  1155,  1156,  1166,  1171,  1192,  1197,  1202,
    1212,  1217,  1227,  1236,  1245,  1254,  1257,  1266,  1278,  1279,
    1287,  1295,  1303,  1311,  1314,  1326,  1327,  1330,  1331,  1343,
    1342,  1364,  1374,  1383,  1396,  1405,  1417,  1426,  1438,  1447,
    1456,  1464,  1472,  1482,  1483,  1493,  1494,  1504,  1512,  1520,
    1528,  1537,  1545,  1554,  1562,  1571,  1579,  1590,  1591,  1601,
    1609,  1619,  1627,  1637,  1641,  1645,  1653,  1661,  1669,  1677,
    1689,  1699,  1711,  1720,  1729,  1737,  1745,  1753,  1761,  1774,
    1787,  1798,  1806,  1809,  1817,  1825,  1835,  1836,  1837,  1838,
    1843,  1854,  1855,  1858,  1866,  1869,  1877,  1877,  1887,  1888,
    1889,  1890,  1891,  1892,  1893,  1894,  1895,  1896,  1897,  1898,
    1899,  1900,  1901,  1902,  1903,  1904,  1905,  1906,  1907,  1908,
    1909,  1910,  1911,  1912,  1913,  1914,  1915,  1916,  1919,  1919,
    1919,  1920,  1920,  1921,  1921,  1921,  1922,  1922,  1922,  1922,
    1923,  1923,  1923,  1923,  1924,  1924,  1924,  1925,  1925,  1925,
    1925,  1926,  1926,  1926,  1926,  1927,  1927,  1927,  1927,  1928,
    1928,  1928,  1928,  1929,  1929,  1929,  1929,  1930,  1930,  1933,
    1942,  1952,  1957,  1967,  1993,  1998,  2003,  2008,  2018,  2028,
    2039,  2053,  2067,  2075,  2083,  2091,  2099,  2107,  2115,  2124,
    2133,  2141,  2149,  2157,  2165,  2173,  2181,  2189,  2197,  2205,
    2213,  2221,  2229,  2237,  2248,  2256,  2264,  2272,  2280,  2288,
    2296,  2304,  2304,  2314,  2324,  2330,  2342,  2343,  2347,  2355,
    2365,  2375,  2376,  2379,  2380,  2381,  2385,  2393,  2403,  2412,
    2420,  2430,  2439,  2448,  2448,  2460,  2470,  2474,  2480,  2488,
    2496,  2510,  2526,  2540,  2555,  2565,  2566,  2567,  2568,  2569,
    2570,  2571,  2572,  2573,  2574,  2575,  2584,  2583,  2611,  2611,
    2619,  2619,  2627,  2635,  2643,  2651,  2664,  2672,  2680,  2688,
    2696,  2704,  2704,  2714,  2722,  2730,  2740,  2741,  2751,  2755,
    2767,  2779,  2779,  2779,  2790,  2790,  2790,  2801,  2812,  2821,
    2823,  2820,  2887,  2886,  2908,  2913,  2907,  2932,  2931,  2953,
    2952,  2975,  2976,  2975,  2996,  3004,  3012,  3020,  3030,  3042,
    3048,  3054,  3060,  3066,  3072,  3078,  3084,  3090,  3096,  3106,
    3112,  3117,  3118,  3125,  3130,  3133,  3134,  3147,  3148,  3158,
    3159,  3162,  3170,  3180,  3188,  3198,  3206,  3215,  3224,  3232,
    3240,  3249,  3261,  3269,  3280,  3284,  3288,  3292,  3298,  3303,
    3308,  3312,  3316,  3320,  3324,  3328,  3336,  3340,  3344,  3348,
    3352,  3356,  3360,  3364,  3368,  3374,  3375,  3381,  3390,  3399,
    3410,  3414,  3424,  3431,  3440,  3448,  3454,  3457,  3454,  3474,
    3482,  3492,  3496,  3503,  3502,  3523,  3539,  3548,  3560,  3574,
    3584,  3583,  3600,  3599,  3615,  3624,  3623,  3641,  3640,  3657,
    3665,  3673,  3688,  3687,  3707,  3706,  3727,  3739,  3740,  3743,
    3762,  3765,  3773,  3781,  3784,  3788,  3791,  3799,  3802,  3803,
    3811,  3814,  3831,  3832,  3833,  3843,  3853,  3880,  3945,  3954,
    3965,  3972,  3982,  3990,  4000,  4009,  4020,  4027,  4039,  4048,
    4058,  4067,  4078,  4085,  4096,  4103,  4118,  4125,  4136,  4143,
    4154,  4161,  4190,  4192,  4191,  4208,  4214,  4219,  4207,  4238,
    4246,  4254,  4262,  4265,  4276,  4277,  4278,  4279,  4282,  4293,
    4294,  4295,  4303,  4313,  4314,  4315,  4316,  4317,  4320,  4321,
    4322,  4323,  4324,  4325,  4326,  4329,  4342,  4352,  4360,  4370,
    4371,  4374,  4383,  4382,  4391,  4403,  4413,  4421,  4425,  4429,
    4433,  4439,  4444,  4449,  4453,  4457,  4461,  4465,  4469,  4473,
    4477,  4481,  4485,  4489,  4493,  4497,  4501,  4506,  4512,  4521,
    4530,  4539,  4550,  4551,  4558,  4567,  4586,  4593,  4606,  4618,
    4630,  4638,  4655,  4663,  4679,  4680,  4683,  4688,  4694,  4706,
    4718,  4726,  4742,  4750,  4766,  4767,  4770,  4783,  4794,  4795,
    4798,  4815,  4819,  4829,  4839,  4839,  4868,  4869,  4879,  4886,
    4896,  4904,  4912,  4924,  4925,  4926,  4929,  4930,  4931,  4932,
    4935,  4936,  4937,  4940,  4945,  4952,  4953,  4956,  4957,  4960,
    4963,  4966,  4967,  4968,  4971,  4972,  4975,  4976,  4980
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end-of-input\"", "error", "$undefined", "keyword_class",
  "keyword_module", "keyword_def", "keyword_undef", "keyword_begin",
  "keyword_rescue", "keyword_ensure", "keyword_end", "keyword_if",
  "keyword_unless", "keyword_then", "keyword_elsif", "keyword_else",
  "keyword_case", "keyword_when", "keyword_while", "keyword_until",
  "keyword_for", "keyword_break", "keyword_next", "keyword_redo",
  "keyword_retry", "keyword_in", "keyword_do", "keyword_do_cond",
  "keyword_do_block", "keyword_do_LAMBDA", "keyword_return",
  "keyword_yield", "keyword_super", "keyword_self", "keyword_nil",
  "keyword_true", "keyword_false", "keyword_and", "keyword_or",
  "keyword_not", "modifier_if", "modifier_unless", "modifier_while",
  "modifier_until", "modifier_rescue", "keyword_alias", "keyword_defined",
  "keyword_BEGIN", "keyword_END", "keyword__LINE__", "keyword__FILE__",
  "keyword__ENCODING__", "tIDENTIFIER", "tFID", "tGVAR", "tIVAR",
  "tCONSTANT", "tCVAR", "tLABEL", "tINTEGER", "tFLOAT", "tSTRING_CONTENT",
  "tCHAR", "tNTH_REF", "tBACK_REF", "tREGEXP_END", "\"unary+\"",
  "\"unary-\"", "\"**\"", "\"<=>\"", "\"==\"", "\"===\"", "\"!=\"",
  "\">=\"", "\"<=\"", "\"&&\"", "\"||\"", "\"=~\"", "\"!~\"", "\"..\"",
  "\"...\"", "\"[]\"", "\"[]=\"", "\"<<\"", "\">>\"", "\"::\"",
  "\":: at EXPR_BEG\"", "tOP_ASGN", "\"=>\"", "\"(\"", "\"( arg\"",
  "\")\"", "\"[\"", "\"{\"", "\"{ arg\"", "\"*\"", "\"**arg\"", "\"&\"",
  "\"->\"", "tSYMBEG", "tSTRING_BEG", "tXSTRING_BEG", "tREGEXP_BEG",
  "tWORDS_BEG", "tQWORDS_BEG", "tSYMBOLS_BEG", "tQSYMBOLS_BEG",
  "tSTRING_DBEG", "tSTRING_DEND", "tSTRING_DVAR", "tSTRING_END", "tLAMBEG",
  "tLOWEST", "'='", "'?'", "':'", "'>'", "'<'", "'|'", "'^'", "'&'", "'+'",
  "'-'", "'*'", "'/'", "'%'", "tUMINUS_NUM", "'!'", "'~'", "tLAST_TOKEN",
  "'{'", "'}'", "'['", "'.'", "','", "'`'", "'('", "')'", "']'", "';'",
  "' '", "'\\n'", "$accept", "program", "@1", "top_compstmt", "top_stmts",
  "top_stmt", "@2", "bodystmt", "compstmt", "stmts", "stmt_or_begin", "@3",
  "stmt", "@4", "command_asgn", "expr", "expr_value", "command_call",
  "block_command", "cmd_brace_block", "@5", "fcall", "command", "mlhs",
  "mlhs_inner", "mlhs_basic", "mlhs_item", "mlhs_head", "mlhs_post",
  "mlhs_node", "lhs", "cname", "cpath", "fname", "fsym", "fitem",
  "undef_list", "@6", "op", "reswords", "arg", "@7", "arg_value",
  "aref_args", "paren_args", "opt_paren_args", "opt_call_args",
  "call_args", "command_args", "@8", "block_arg", "opt_block_arg", "args",
  "mrhs", "primary", "@9", "@10", "@11", "@12", "@13", "@14", "@15", "@16",
  "@17", "@18", "@19", "@20", "@21", "@22", "@23", "@24", "@25",
  "primary_value", "k_begin", "k_if", "k_unless", "k_while", "k_until",
  "k_case", "k_for", "k_class", "k_module", "k_def", "k_end", "then", "do",
  "if_tail", "opt_else", "for_var", "f_marg", "f_marg_list", "f_margs",
  "block_args_tail", "opt_block_args_tail", "block_param",
  "opt_block_param", "block_param_def", "opt_bv_decl", "bv_decls", "bvar",
  "lambda", "@26", "@27", "f_larglist", "lambda_body", "do_block", "@28",
  "block_call", "method_call", "@29", "@30", "@31", "@32", "brace_block",
  "@33", "@34", "case_body", "cases", "opt_rescue", "exc_list", "exc_var",
  "opt_ensure", "literal", "strings", "string", "string1", "xstring",
  "regexp", "words", "word_list", "word", "symbols", "symbol_list",
  "qwords", "qsymbols", "qword_list", "qsym_list", "string_contents",
  "xstring_contents", "regexp_contents", "string_content", "@35", "@36",
  "@37", "@38", "string_dvar", "symbol", "sym", "dsym", "numeric",
  "user_variable", "keyword_variable", "var_ref", "var_lhs", "backref",
  "superclass", "@39", "f_arglist", "args_tail", "opt_args_tail", "f_args",
  "f_bad_arg", "f_norm_arg", "f_arg_item", "f_arg", "f_kw", "f_block_kw",
  "f_block_kwarg", "f_kwarg", "kwrest_mark", "f_kwrest", "f_opt",
  "f_block_opt", "f_block_optarg", "f_optarg", "restarg_mark",
  "f_rest_arg", "blkarg_mark", "f_block_arg", "opt_f_block_arg",
  "singleton", "@40", "assoc_list", "assocs", "assoc", "operation",
  "operation2", "operation3", "dot_or_colon", "opt_terms", "opt_nl",
  "rparen", "rbracket", "trailer", "term", "terms", "none", 0
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
     315,   316,   317,   318,   319,   320,   130,   131,   132,   134,
     139,   140,   141,   138,   137,   321,   322,   142,   143,   128,
     129,   144,   145,   135,   136,   323,   324,   325,   326,   327,
     328,   329,   330,   331,   332,   333,   334,   335,   336,   337,
     338,   339,   340,   341,   342,   343,   344,   345,   346,   347,
     348,   349,   350,    61,    63,    58,    62,    60,   124,    94,
      38,    43,    45,    42,    47,    37,   351,    33,   126,   352,
     123,   125,    91,    46,    44,    96,    40,    41,    93,    59,
      32,    10
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   142,   144,   143,   145,   146,   146,   146,   146,   147,
     148,   147,   149,   150,   151,   151,   151,   151,   152,   153,
     152,   155,   154,   154,   154,   154,   154,   154,   154,   154,
     154,   154,   154,   154,   154,   154,   154,   154,   154,   154,
     154,   154,   154,   154,   154,   154,   156,   156,   157,   157,
     157,   157,   157,   157,   158,   159,   159,   160,   160,   162,
     161,   163,   164,   164,   164,   164,   164,   164,   164,   164,
     164,   164,   164,   165,   165,   166,   166,   167,   167,   167,
     167,   167,   167,   167,   167,   167,   167,   168,   168,   169,
     169,   170,   170,   171,   171,   171,   171,   171,   171,   171,
     171,   171,   172,   172,   172,   172,   172,   172,   172,   172,
     172,   173,   173,   174,   174,   174,   175,   175,   175,   175,
     175,   176,   176,   177,   177,   178,   179,   178,   180,   180,
     180,   180,   180,   180,   180,   180,   180,   180,   180,   180,
     180,   180,   180,   180,   180,   180,   180,   180,   180,   180,
     180,   180,   180,   180,   180,   180,   180,   180,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   181,
     181,   181,   181,   181,   181,   181,   181,   181,   181,   182,
     182,   182,   182,   182,   182,   182,   182,   182,   182,   182,
     182,   182,   182,   182,   182,   182,   182,   182,   182,   182,
     182,   182,   182,   182,   182,   182,   182,   182,   182,   182,
     182,   182,   182,   182,   182,   182,   182,   182,   182,   182,
     182,   183,   182,   182,   182,   184,   185,   185,   185,   185,
     186,   187,   187,   188,   188,   188,   188,   188,   189,   189,
     189,   189,   189,   191,   190,   192,   193,   193,   194,   194,
     194,   194,   195,   195,   195,   196,   196,   196,   196,   196,
     196,   196,   196,   196,   196,   196,   197,   196,   198,   196,
     199,   196,   196,   196,   196,   196,   196,   196,   196,   196,
     196,   200,   196,   196,   196,   196,   196,   196,   196,   196,
     196,   201,   202,   196,   203,   204,   196,   196,   196,   205,
     206,   196,   207,   196,   208,   209,   196,   210,   196,   211,
     196,   212,   213,   196,   196,   196,   196,   196,   214,   215,
     216,   217,   218,   219,   220,   221,   222,   223,   224,   225,
     226,   226,   226,   227,   227,   228,   228,   229,   229,   230,
     230,   231,   231,   232,   232,   233,   233,   233,   233,   233,
     233,   233,   233,   233,   234,   234,   234,   234,   235,   235,
     236,   236,   236,   236,   236,   236,   236,   236,   236,   236,
     236,   236,   236,   236,   236,   237,   237,   238,   238,   238,
     239,   239,   240,   240,   241,   241,   243,   244,   242,   245,
     245,   246,   246,   248,   247,   249,   249,   249,   249,   250,
     251,   250,   252,   250,   250,   253,   250,   254,   250,   250,
     250,   250,   256,   255,   257,   255,   258,   259,   259,   260,
     260,   261,   261,   261,   262,   262,   263,   263,   264,   264,
     264,   265,   266,   266,   266,   267,   268,   269,   270,   270,
     271,   271,   272,   272,   273,   273,   274,   274,   275,   275,
     276,   276,   277,   277,   278,   278,   279,   279,   280,   280,
     281,   281,   282,   283,   282,   284,   285,   286,   282,   287,
     287,   287,   287,   288,   289,   289,   289,   289,   290,   291,
     291,   291,   291,   292,   292,   292,   292,   292,   293,   293,
     293,   293,   293,   293,   293,   294,   294,   295,   295,   296,
     296,   297,   298,   297,   297,   299,   299,   300,   300,   300,
     300,   301,   301,   302,   302,   302,   302,   302,   302,   302,
     302,   302,   302,   302,   302,   302,   302,   302,   303,   303,
     303,   303,   304,   304,   305,   305,   306,   306,   307,   308,
     309,   309,   310,   310,   311,   311,   312,   312,   313,   314,
     315,   315,   316,   316,   317,   317,   318,   318,   319,   319,
     320,   321,   321,   322,   323,   322,   324,   324,   325,   325,
     326,   326,   326,   327,   327,   327,   328,   328,   328,   328,
     329,   329,   329,   330,   330,   331,   331,   332,   332,   333,
     334,   335,   335,   335,   336,   336,   337,   337,   338
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     2,     1,     1,     3,     2,     1,
       0,     5,     4,     2,     1,     1,     3,     2,     1,     0,
       5,     0,     4,     3,     3,     3,     2,     3,     3,     3,
       3,     3,     4,     1,     3,     3,     6,     5,     5,     5,
       5,     3,     3,     3,     3,     1,     3,     3,     1,     3,
       3,     3,     2,     1,     1,     1,     1,     1,     4,     0,
       5,     1,     2,     3,     4,     5,     4,     5,     2,     2,
       2,     2,     2,     1,     3,     1,     3,     1,     2,     3,
       5,     2,     4,     2,     4,     1,     3,     1,     3,     2,
       3,     1,     3,     1,     1,     4,     3,     3,     3,     3,
       2,     1,     1,     1,     4,     3,     3,     3,     3,     2,
       1,     1,     1,     2,     1,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     0,     4,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     3,
       5,     3,     5,     6,     5,     5,     5,     5,     4,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     4,     4,
       2,     2,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     2,     2,     3,     3,     3,
       3,     0,     4,     6,     1,     1,     1,     2,     4,     2,
       3,     1,     1,     1,     1,     2,     4,     2,     1,     2,
       2,     4,     1,     0,     2,     2,     2,     1,     1,     2,
       3,     4,     3,     4,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     0,     4,     0,     3,
       0,     4,     3,     3,     2,     3,     3,     1,     4,     3,
       1,     0,     6,     4,     3,     2,     1,     2,     2,     6,
       6,     0,     0,     7,     0,     0,     7,     5,     4,     0,
       0,     9,     0,     6,     0,     0,     8,     0,     5,     0,
       6,     0,     0,     9,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     1,     1,     1,     5,     1,     2,     1,
       1,     1,     3,     1,     3,     1,     4,     6,     3,     5,
       2,     4,     1,     3,     4,     2,     2,     1,     2,     0,
       6,     8,     4,     6,     4,     2,     6,     2,     4,     6,
       2,     4,     2,     4,     1,     1,     1,     3,     1,     4,
       1,     4,     1,     3,     1,     1,     0,     0,     4,     4,
       1,     3,     3,     0,     5,     2,     4,     5,     5,     2,
       0,     5,     0,     5,     3,     0,     4,     0,     4,     2,
       1,     4,     0,     5,     0,     5,     5,     1,     1,     6,
       1,     1,     1,     1,     2,     1,     2,     1,     1,     1,
       1,     1,     1,     1,     2,     3,     3,     3,     3,     3,
       0,     3,     1,     2,     3,     3,     0,     3,     3,     3,
       3,     3,     0,     3,     0,     3,     0,     2,     0,     2,
       0,     2,     1,     0,     3,     0,     0,     0,     6,     1,
       1,     1,     1,     2,     1,     1,     1,     1,     3,     1,
       1,     2,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     0,     4,     2,     3,     2,     4,     2,     2,
       1,     2,     0,     6,     8,     4,     6,     4,     6,     2,
       4,     6,     2,     4,     2,     4,     1,     0,     1,     1,
       1,     1,     1,     1,     1,     3,     1,     3,     2,     2,
       1,     3,     1,     3,     1,     1,     2,     1,     3,     3,
       1,     3,     1,     3,     1,     1,     2,     1,     1,     1,
       2,     2,     1,     1,     0,     4,     1,     2,     1,     3,
       3,     2,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     0,     1,     0,     1,     2,
       2,     0,     1,     1,     1,     1,     1,     2,     0
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,     0,   346,   347,   348,     0,   339,
     340,   341,   344,   342,   343,   345,   334,   335,   336,   337,
     297,   263,   263,   509,   508,   510,   511,   607,     0,   607,
      10,     0,   513,   512,   514,   593,   595,   505,   504,   594,
     507,   499,   500,   452,   519,   520,     0,     0,     0,     0,
     288,   618,   618,    85,   406,   478,   476,   478,   480,   460,
     472,   466,   474,     0,     0,     0,     3,   605,     6,     9,
      33,    45,    48,    56,   263,    55,     0,    73,     0,    77,
      87,     0,    53,   244,     0,   286,     0,     0,   311,   314,
     605,     0,     0,     0,     0,    57,   306,   275,   276,   451,
     453,   277,   278,   279,   281,   280,   282,   449,   450,   448,
     515,   516,   283,     0,   284,    61,     5,     8,   168,   179,
     169,   192,   165,   185,   175,   174,   195,   196,   190,   173,
     172,   167,   193,   197,   198,   177,   166,   180,   184,   186,
     178,   171,   187,   194,   189,   188,   181,   191,   176,   164,
     183,   182,   163,   170,   161,   162,   158,   159,   160,   116,
     118,   117,   153,   154,   149,   131,   132,   133,   140,   137,
     139,   134,   135,   155,   156,   141,   142,   146,   150,   136,
     138,   128,   129,   130,   143,   144,   145,   147,   148,   151,
     152,   157,   121,   123,   125,    26,   119,   120,   122,   124,
       0,     0,     0,     0,     0,     0,     0,     0,   258,     0,
     245,   268,    71,   262,   618,     0,   515,   516,     0,   284,
     618,   588,    72,    70,   607,    69,     0,   618,   429,    68,
     607,   608,     0,     0,    21,   241,     0,     0,   334,   335,
     297,   300,   430,     0,   220,     0,   221,   294,     0,    19,
       0,     0,   605,    15,    18,   607,    75,    14,   290,   607,
       0,   611,   611,   246,     0,     0,   611,   586,   607,     0,
       0,     0,    83,   338,     0,    93,    94,   101,   308,   407,
     496,   495,   497,   494,     0,   493,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   501,   502,    52,
     235,   236,   614,   615,     4,   616,   606,     0,     0,     0,
       0,     0,     0,     0,   434,   432,   419,    62,   305,   413,
     415,     0,    89,     0,    81,    78,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   427,   618,   425,     0,    54,     0,     0,
       0,     0,   605,     0,   606,     0,   360,   359,     0,     0,
     515,   516,   284,   111,   112,     0,     0,   114,     0,     0,
     515,   516,   284,   327,   188,   181,   191,   176,   158,   159,
     160,   116,   117,   584,   329,   583,     0,   604,   603,     0,
     307,   454,     0,     0,   126,   591,   294,   269,   592,   265,
       0,     0,     0,   259,   267,   427,   618,   425,     0,     0,
       0,   260,   607,     0,   299,   264,   607,   254,   618,   618,
     253,   607,   304,    51,    23,    25,    24,     0,   301,     0,
       0,     0,   427,   425,     0,    17,     0,   607,   292,    13,
     606,    74,   607,   289,   295,   613,   612,   247,   613,   249,
     296,   587,     0,   100,   501,   502,    91,    86,     0,   427,
     618,   425,   547,   482,   485,   483,   498,   479,   455,   477,
     456,   457,   481,   458,   459,     0,   462,   468,     0,   469,
     464,   465,     0,   470,     0,   471,     0,     0,   617,     7,
      27,    28,    29,    30,    31,    49,    50,   618,   618,    59,
      63,   618,     0,    34,    43,     0,    44,   607,     0,    79,
      90,    47,    46,     0,   199,   268,    42,   217,   225,   230,
     231,   232,   227,   229,   239,   240,   233,   234,   210,   211,
     237,   238,   607,   226,   228,   222,   223,   224,   212,   213,
     214,   215,   216,   596,   598,   597,   599,     0,   263,   424,
     607,   596,   598,   597,   599,     0,   263,     0,   618,   351,
       0,   350,     0,     0,     0,     0,     0,     0,   294,   427,
     618,   425,   319,   324,   111,   112,   113,     0,   522,   322,
     521,   427,   618,   425,     0,     0,   547,   331,   596,   597,
     263,    35,   201,    41,   209,     0,   199,   590,     0,   270,
     266,   618,   596,   597,   607,   596,   597,   589,   298,   609,
     250,   255,   257,   303,    22,     0,   242,     0,    32,   422,
     420,   208,     0,    76,    16,   291,   611,     0,    84,    97,
      99,   607,   596,   597,   553,   550,   549,   548,   551,     0,
     564,     0,   575,   565,   579,   578,   574,   547,     0,   546,
     410,   552,   554,   556,   532,   562,   618,   567,   618,   572,
     532,   577,   532,     0,   530,   486,     0,   461,   463,   473,
     467,   475,   218,   219,   398,   607,     0,   396,   395,     0,
     618,     0,   274,     0,    88,    82,     0,     0,     0,     0,
       0,     0,   428,    66,     0,     0,   431,     0,     0,   426,
      64,   618,   349,   287,   618,   618,   440,   618,   352,   618,
     354,   312,   353,   315,     0,     0,   318,   600,   293,   607,
     596,   597,     0,     0,   524,     0,     0,   111,   112,   115,
     607,     0,   607,   547,     0,     0,     0,   252,   416,    58,
     251,     0,   127,   271,   261,     0,     0,   431,     0,     0,
     618,   607,    11,     0,   248,    92,    95,     0,   558,   553,
       0,   372,   363,   365,   607,   361,   607,     0,     0,   408,
       0,   539,     0,   528,   582,   566,     0,   529,     0,   542,
     576,     0,   544,   580,   487,   489,   490,   491,   484,   492,
     553,     0,   394,   607,     0,   379,   560,   618,   618,   570,
     379,   379,   377,   400,     0,     0,     0,     0,     0,   272,
      80,   200,     0,    40,   206,    39,   207,    67,   423,   610,
       0,    37,   204,    38,   205,    65,   421,   441,   442,   618,
     443,     0,   618,   357,     0,     0,   355,     0,     0,     0,
     317,     0,     0,   431,     0,   325,     0,     0,   431,   328,
     585,   607,     0,   526,   332,   417,   418,   202,     0,   256,
     302,    20,   568,   607,     0,   370,     0,   555,     0,     0,
       0,   531,   557,   532,   532,   563,   618,   581,   532,   573,
     532,   532,     0,     0,     0,   559,     0,   397,   385,   387,
       0,   375,   376,     0,   390,     0,   392,     0,   435,   433,
       0,   414,   273,   243,    36,   203,     0,     0,   445,   358,
       0,    12,   447,     0,   309,   310,     0,     0,   270,   618,
     320,     0,   523,   323,   525,   330,   547,   362,   373,     0,
     368,   364,   409,   412,   411,     0,   535,     0,   537,   527,
       0,   543,     0,   540,   545,     0,   569,   294,   427,   399,
     378,   379,   379,   561,   618,   379,   571,   379,   379,   404,
     607,   402,   405,    60,     0,   444,     0,   102,   103,   110,
       0,   446,     0,   313,   316,   437,   438,   436,     0,     0,
       0,     0,   371,     0,   366,   532,   532,   532,   532,   488,
     600,   293,     0,   382,     0,   384,   374,     0,   391,     0,
     388,   393,     0,   401,   109,   427,   618,   425,   618,   618,
       0,   326,     0,   369,     0,   536,     0,   533,   538,   541,
     379,   379,   379,   379,   403,   600,   108,   607,   596,   597,
     439,   356,   321,   333,   367,   532,   383,     0,   380,   386,
     389,   431,   534,   379,   381
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    66,    67,    68,   236,   567,   568,   252,
     253,   446,   254,   437,    70,    71,   358,    72,    73,   510,
     690,   243,    75,    76,   255,    77,    78,    79,   467,    80,
     209,   377,   378,   192,   193,   194,   195,   605,   556,   197,
      82,   439,   211,   260,   228,   748,   426,   427,   225,   226,
     213,   413,   428,   516,    83,   356,   259,   452,   625,   360,
     848,   361,   849,   732,   988,   736,   733,   931,   594,   596,
     746,   936,   245,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,   713,   570,   721,   845,   846,   369,   772,
     773,   774,   960,   899,   803,   686,   687,   804,   970,   971,
     278,   279,   472,   658,   779,   320,   511,    95,    96,   711,
     704,   565,   557,   318,   508,   507,   577,   987,   715,   839,
     917,   921,    97,    98,    99,   100,   101,   102,   103,   290,
     485,   104,   294,   105,   106,   292,   296,   286,   284,   288,
     477,   676,   675,   794,   892,   798,   107,   285,   108,   109,
     216,   217,   112,   218,   219,   589,   735,   744,   881,   781,
     745,   661,   662,   663,   664,   665,   806,   807,   666,   667,
     668,   669,   809,   810,   670,   671,   672,   673,   674,   783,
     396,   595,   265,   429,   221,   115,   629,   559,   399,   304,
     423,   424,   706,   457,   571,   364,   257
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -804
static const yytype_int16 yypact[] =
{
    -804,   112,  2751,  -804,  7365,  -804,  -804,  -804,  6888,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  7478,  7478,  -804,  -804,
    7478,  4073,  3668,  -804,  -804,  -804,  -804,   335,  6755,   -10,
    -804,    15,  -804,  -804,  -804,  2993,  3803,  -804,  -804,  3128,
    -804,  -804,  -804,  -804,  -804,  -804,  8834,  8834,    92,  5125,
    8947,  7817,  8156,  7147,  -804,  6622,  -804,  -804,  -804,    44,
      56,   182,   209,   544,  9060,  8834,  -804,    -9,  -804,   845,
    -804,   130,  -804,  -804,   138,   266,   238,  -804,   219,  9173,
    -804,   295,  2612,   398,   405,  -804,  8947,  8947,  -804,  -804,
    6012,  9282,  9391,  9500,  6488,    30,    62,  -804,  -804,   316,
    -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
     564,   574,  -804,   356,   634,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,   315,  -804,  -804,  -804,  -804,
     360,  8834,   402,  5264,  8834,  8834,  8834,  8834,  -804,   388,
    2612,   430,  -804,  -804,   392,   422,   174,   185,   447,   291,
     408,  -804,  -804,  -804,  5899,  -804,  7478,  7478,  -804,  -804,
    6125,  -804,  8947,   596,  -804,   400,   420,  5403,  -804,  -804,
    -804,   423,   432,   138,  -804,   546,   515,   720,  7591,  -804,
    5125,   478,    -9,  -804,   845,   -10,   487,  -804,   130,   -10,
     479,   180,   247,  -804,   430,   491,   247,  -804,   -10,   582,
     577,  9609,   499,  -804,   573,   578,   600,   654,  -804,  -804,
    -804,  -804,  -804,  -804,   322,  -804,   463,   486,   401,   532,
     504,   545,    55,   563,   518,   566,    63,   625,   650,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  6238,  8947,  8947,  8947,
    8947,  7591,  8947,  8947,  -804,  -804,  -804,   608,  -804,  -804,
    -804,  8269,  -804,  5125,  7256,   588,  8269,  8834,  8834,  8834,
    8834,  8834,  8834,  8834,  8834,  8834,  8834,  8834,  8834,  8834,
    8834,  8834,  8834,  8834,  8834,  8834,  8834,  8834,  8834,  8834,
    8834,  8834,  8834,  9888,  7478,  9965,  4482,   130,   116,   116,
    8947,  8947,    -9,   707,   592,   678,  -804,  -804,   657,   711,
      65,    66,    69,   444,   571,  8947,   129,  -804,   257,   670,
    -804,  -804,  -804,  -804,   253,   334,   349,   370,   387,   458,
     477,   535,   539,  -804,  -804,  -804,    30,  -804,  -804, 10042,
    -804,  -804,  9060,  9060,  -804,  -804,   304,  -804,  -804,  -804,
    8834,  8834,  7704,  -804,  -804, 10119,  7478, 10196,  8834,  8834,
    7930,  -804,   -10,   604,  -804,  -804,   -10,  -804,   603,   609,
    -804,    89,  -804,  -804,  -804,  -804,  -804,  6888,  -804,  8834,
    5534,   617, 10119, 10196,  8834,   845,   621,   -10,  -804,  -804,
    6351,   618,   -10,  -804,  -804,  8043,  -804,  -804,  8156,  -804,
    -804,  -804,   400,   692,  -804,  -804,  -804,   619,  9609, 10273,
    7478, 10350,  1377,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,   283,  -804,  -804,   614,  -804,
    -804,  -804,   293,  -804,   638,  -804,  8834,  8834,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  -804,    29,    29,  -804,
    -804,    29,  8834,  -804,   649,   662,  -804,   -10,  9609,   674,
    -804,  -804,  -804,   691,  2023,  -804,  -804,   515,  2149,  2149,
    2149,  2149,  1118,  1118,  2550,  1796,  2149,  2149,  2837,  2837,
     524,   524, 10841,  1118,  1118,  1095,  1095,  1178,    52,    52,
     515,   515,   515,  4208,  3263,  4343,  3398,   432,   685,  -804,
     -10,   722,  -804,   732,  -804,   432,  3938,   800,   804,  -804,
    4621,   801,  4899,    70,    70,   707,  8382,   800,    85, 10427,
    7478, 10504,  -804,   130,  -804,   692,  -804,    -9,  -804,  -804,
    -804, 10581,  7478, 10042,  4482,  8947,  1481,  -804,  -804,  -804,
    1147,  -804,  2365,  -804,  2612,  6888,  2484,  -804,  8834,   430,
    -804,   408,  2858,  3533,   -10,   328,   333,  -804,  -804,  -804,
    -804,  7704,  7930,  -804,  -804,  8947,  2612,   700,  -804,  -804,
    -804,  2612,  5534,   364,  -804,  -804,   247,  9609,   619,   643,
      23,   -10,   202,   323,   703,  -804,  -804,  -804,  -804,  8834,
    -804,   807,  -804,  -804,  -804,  -804,  -804,  1551,    75,  -804,
    -804,  -804,  -804,  -804,   689,  -804,   694,   780,   702,  -804,
     717,   806,   733,   817,  -804,  -804,   789,  -804,  -804,  -804,
    -804,  -804,   515,   515,  -804,  1086,  5673,  -804,  -804,  5403,
      29,  5673,   737,  8495,  -804,   619,  9609,  9060,  8834,   726,
    9060,  9060,  -804,   608,   432,   735,   747,  9060,  9060,  -804,
     608,   432,  -804,  -804,  8608,   859,  -804,   676,  -804,   859,
    -804,  -804,  -804,  -804,   800,    74,  -804,    57,    61,   -10,
     100,   108,  8947,    -9,  -804,  8947,  4482,   643,    23,  -804,
     -10,   800,    89,  1551,  4482,    -9,  7021,  -804,    62,   266,
    -804,  8834,  -804,  -804,  -804,  8834,  8834,   374,  8834,  8834,
     742,    89,  -804,   749,  -804,  -804,   428,  8834,  -804,  -804,
     807,   708,  -804,   744,   -10,  -804,   -10,  5673,  5403,  -804,
    1551,  -804,   395,  -804,  -804,  -804,    40,  -804,  1551,  -804,
    -804,   992,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
     769,  9718,  -804,   -10,   773,   760,  -804,   765,   702,  -804,
     766,   771,  -804,   764,   898,   778,  5403,   901,  8834,   781,
     619,  2612,  8834,  -804,  2612,  -804,  2612,  -804,  -804,  -804,
    9060,  -804,  2612,  -804,  2612,  -804,  -804,   649,  -804,   826,
    -804,  5012,   907,  -804,  8947,   800,  -804,   800,  5673,  5673,
    -804,  8721,  4760,   154,    70,  -804,    -9,   800,  -804,  -804,
    -804,   -10,   800,  -804,  -804,  -804,  -804,  2612,  8834,  7930,
    -804,  -804,  -804,   -10,   892,   784,   877,  -804,   783,   912,
     792,  -804,  -804,   791,   802,  -804,   702,  -804,   809,  -804,
     811,   809,  5786,  9718,   894,   697,   833,  -804,  1627,  -804,
     436,  -804,  -804,  1627,  -804,  1699,  -804,   883,  -804,  -804,
     821,  -804,   819,  2612,  -804,  2612,  9827,   116,  -804,  -804,
    5673,  -804,  -804,   116,  -804,  -804,   800,   800,  -804,   289,
    -804,  4482,  -804,  -804,  -804,  -804,  1481,  -804,   820,   892,
     743,  -804,  -804,  -804,  -804,  1551,  -804,   992,  -804,  -804,
     992,  -804,   992,  -804,  -804,   848,   697,  -804, 10658,  -804,
    -804,   823,   824,  -804,   702,   825,  -804,   829,   825,  -804,
     277,  -804,  -804,  -804,   904,  -804,   706,   578,   600,   654,
    4482,  -804,  4621,  -804,  -804,  -804,  -804,  -804,  5673,   800,
    4482,   892,   820,   892,   834,   809,   836,   809,   809,  -804,
     831,   837,  1627,  -804,  1699,  -804,  -804,  1699,  -804,  1699,
    -804,  -804,   883,  -804,   692, 10735,  7478, 10812,   804,   676,
     800,  -804,   800,   820,   892,  -804,   992,  -804,  -804,  -804,
     825,   841,   825,   825,  -804,   169,    23,   -10,   139,   170,
    -804,  -804,  -804,  -804,   820,   809,  -804,  1699,  -804,  -804,
    -804,   241,  -804,   825,  -804
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -804,  -804,  -804,  -386,  -804,    41,  -804,  -542,   285,  -804,
     527,  -804,    35,  -804,  -310,   -43,   -70,    19,  -804,  -187,
    -804,   680,     9,   887,  -154,    27,   -73,  -804,  -404,     6,
    1733,  -328,   886,   -52,  -804,   -24,  -804,  -804,    13,  -804,
    1007,  -804,   909,  -804,   -72,   271,  -336,   141,     5,  -804,
    -320,  -212,    58,  -313,   -21,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,    49,  -804,  -804,  -804,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,  -520,  -348,  -519,   -36,  -634,  -804,  -803,
    -773,   214,   300,    37,  -804,  -405,  -804,  -653,  -804,   -18,
    -804,  -804,  -804,  -804,  -804,   246,  -804,  -804,  -804,  -804,
    -804,  -804,  -804,   -95,  -804,  -804,  -535,  -804,   -22,  -804,
    -804,  -804,  -804,  -804,  -804,   908,  -804,  -804,  -804,  -804,
     714,  -804,  -804,  -804,  -804,  -804,  -804,  -804,   940,  -804,
    -116,  -804,  -804,  -804,  -804,  -804,     2,  -804,     7,  -804,
    1388,  1539,   906,  1901,  1724,  -804,  -804,    73,  -450,  -410,
    -412,  -769,  -627,  -718,  -134,   228,   111,  -804,  -804,  -804,
     -83,  -721,  -786,   114,   233,  -804,  -574,  -804,  -463,  -579,
    -804,  -804,  -804,   102,  -374,  -804,  -322,  -804,   622,   -29,
     -15,  -221,  -578,  -243,   -62,   -11,    -2
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -619
static const yytype_int16 yytable[] =
{
     116,   400,   316,   283,   234,   305,   325,   258,   421,   432,
     198,   572,   232,   526,   235,   199,   521,   359,   560,   459,
     362,   196,   659,   461,   775,   208,   208,   229,   305,   208,
     198,   558,   273,   566,   451,   199,   757,    69,   453,    69,
     724,   196,   394,   357,   357,   117,   617,   357,   586,   263,
     267,    84,   741,    84,   627,   723,   306,   726,   273,   272,
     660,   363,   882,   766,   638,   215,   215,   889,   196,   215,
     273,   273,   273,   941,   214,   214,   256,   600,   214,   317,
     614,   842,  -106,   299,   617,   847,  -108,   569,   314,   787,
    -102,  -103,   610,   558,  -110,   566,   447,   720,    84,   215,
     610,   938,   274,   689,   777,   684,   691,   196,  -293,   261,
    -109,   811,     3,   215,   695,   397,   488,   966,   220,   220,
     327,   630,   220,   878,   494,  -105,   312,   313,   274,   569,
     302,   231,   303,  -107,   641,   215,   215,   654,   972,   215,
     368,   379,   379,   775,   875,   237,   659,   685,   247,   630,
     896,   853,  -105,   262,   266,  -293,  -293,   212,   222,  -597,
     655,   223,   858,   398,   314,   489,   992,   312,   313,   517,
     479,   316,   482,   495,   486,   350,   351,   352,   486,  -104,
     882,   584,  -106,  -107,   289,   585,   778,   431,   941,   433,
     305,   -97,   315,  -596,   857,   -99,   291,  -597,   466,   -93,
     -94,   618,   862,  -101,   850,   620,   884,   659,   851,   302,
     623,   303,   414,   302,   890,   303,   966,   421,   414,  -100,
    1023,   859,   812,   449,   889,   430,   633,   -96,   901,   902,
     231,   635,   882,   208,   -96,   208,   208,   500,   501,   502,
     503,   450,   -98,   972,   729,   776,   763,   775,   617,   775,
     273,  1044,    84,   462,  -104,   302,   740,   303,   587,   630,
     789,  -517,   792,   739,   357,   357,   357,   357,   315,   505,
     506,   630,  -518,   215,   227,   215,   215,   256,  -105,   215,
    -105,   215,   214,   445,   214,   816,    84,  -102,   -95,   882,
     573,   574,   820,   659,   319,   985,   694,    84,  -103,    84,
     305,   610,   610,   273,   841,  -596,   576,   949,  -106,  -107,
    -106,  -107,   775,   994,   455,   -96,   590,   357,   357,   887,
     274,   456,   293,   887,   962,   924,   220,   925,   220,   967,
     519,   861,   583,   575,   251,   930,   -96,   933,  -509,   -96,
     513,    69,   935,   -96,   473,   522,   504,   499,   -98,   295,
     256,   321,   430,   322,   473,    84,   215,   215,   215,   215,
      84,   215,   215,   208,   775,   422,   775,   425,   564,   678,
     215,   996,    84,   274,   588,   215,   678,   852,   419,   515,
    -104,   458,  -104,   473,   515,  1006,  -509,   521,   456,   989,
     474,   444,   475,   764,   986,   466,   302,   775,   303,   754,
     474,   838,   475,   215,  -110,    84,   983,   984,   326,   215,
     215,  1012,   564,   624,   430,   758,    56,  -109,   231,  -508,
     759,   601,   603,   677,   215,   208,   414,   414,  1031,   474,
     564,   475,   476,   680,  -510,   812,   -98,   887,   116,   198,
     812,  -105,   812,   402,   199,   466,  -107,   273,  1022,   404,
     196,   215,   215,   649,  -431,  -511,   564,   -98,   406,  1051,
     -98,   868,   473,   650,   -98,   215,   481,  -508,   430,  1021,
    -593,   230,  -513,   946,   948,    69,   231,   -74,   951,   208,
     953,   954,  -510,  -338,   564,   702,   659,  -104,   251,    84,
     353,   653,   654,   709,   801,   617,   230,   273,   -88,    84,
    1042,   410,  1043,  -511,   650,   688,   688,   415,   474,   688,
     475,   722,   722,  -431,   611,   655,   827,   274,   411,   215,
    -513,   860,   441,   835,   473,   734,   412,   699,   747,  -503,
    -338,  -338,   653,   654,   418,   251,   438,   354,   355,   812,
     870,   812,   420,  -512,   812,   705,   812,   473,   754,   610,
     440,   805,   742,   877,   416,   417,   655,   636,  -431,   224,
    -431,  -431,  -514,   703,   765,   473,   716,   274,   227,   980,
     474,   710,   475,   478,  -593,   982,  -503,  -503,   430,   473,
    -593,   752,   761,   327,   812,  1025,  1027,  1028,  1029,   208,
     430,  -512,   327,   474,   564,   475,   480,  -594,   750,   705,
     -73,   208,   808,   297,   298,   749,   564,   198,   251,   414,
    -514,   474,   199,   475,   484,   448,   273,   454,   196,    84,
    -503,    84,   460,   466,  -506,   474,   705,   475,   491,   215,
     116,   442,   828,   468,   725,  1052,   464,   465,   463,   747,
     934,   215,   483,    84,   215,   348,   349,   350,   351,   352,
     434,  -517,   937,   865,   888,   487,  -506,   891,   469,   435,
     436,  -518,   854,  -515,   784,   856,   784,    69,  -503,  -600,
     813,   855,  -506,   490,   215,   273,   493,  -102,   416,   443,
    1037,    84,    74,   863,    74,  -516,   274,  -103,   688,   357,
     844,   841,   357,   496,   864,   630,    74,    74,   -93,   886,
      74,  -594,   509,  -506,  -506,   470,   471,  -594,   -94,   750,
    -515,  -515,   840,   843,   705,   843,   522,   843,   497,   823,
     825,   403,   520,   760,   576,   705,   831,   833,  -600,    74,
      74,   498,  -516,  -516,   578,    84,   582,   621,    84,  -284,
      84,   619,   579,   622,    74,   274,   215,  -110,   628,   215,
     215,   632,   -88,   637,   679,   591,   215,   215,   414,   196,
     769,   813,   645,   646,   647,   648,    74,    74,  -101,   965,
      74,   968,   515,  -600,   923,  -600,  -600,  -294,   681,  -596,
     273,   215,   958,  -268,   215,    84,  -284,  -284,   813,   580,
     581,  1015,   722,    84,   932,   769,   693,   645,   646,   647,
     648,   357,   592,   593,   697,   784,   784,   444,   696,   707,
     712,   995,   714,   997,   718,   808,   767,   964,   998,   708,
     808,  -422,   808,   780,  -294,  -294,    84,    84,   782,   592,
     593,   762,   785,  -109,   830,  -105,   786,   918,  1016,  1017,
     922,   822,   874,   795,   796,  -107,   797,   904,   906,   914,
     895,   788,    44,    45,  -100,   717,   -96,   719,   790,   769,
    -104,   645,   646,   647,   648,    84,   -98,   791,  1030,   793,
    1032,  -269,   273,   829,   841,  1033,   869,   993,   876,   215,
     871,   -95,   893,    74,   784,   307,   308,   309,   310,   311,
      84,   897,  1045,   215,   898,   273,   770,    84,    84,   900,
     903,    84,   771,   907,    74,   905,    74,    74,   908,   909,
      74,   911,    74,  1053,   916,  -270,   920,    74,   939,   808,
     942,   808,   943,   944,   808,   945,   808,   843,    74,   769,
      74,   645,   646,   647,   648,   969,   947,   645,   646,   647,
     648,    84,   956,   950,   769,   952,   645,   646,   647,   648,
     957,   959,   973,  -271,   991,  1013,   999,  1002,  1004,  1007,
    1014,   264,   784,  1009,   808,   976,   770,  -596,  1024,    84,
    1026,   814,   940,  -597,   815,  1047,   817,   634,   366,   383,
      84,   770,   836,  1041,   873,   802,    74,    74,    74,    74,
      74,    74,    74,    74,  1034,   866,  1040,   287,  1003,  1005,
     395,    74,  1008,    74,  1010,  1011,    74,   401,   492,   990,
     885,   963,   961,   883,   430,     0,   716,   843,   597,     0,
       0,     0,   705,   210,   210,   208,     0,   210,     0,    84,
     564,    84,     0,     0,    74,     0,    74,    84,     0,    84,
      74,    74,     0,     0,   769,     0,   645,   646,   647,   648,
     649,     0,     0,   244,   246,    74,     0,     0,   210,   210,
     650,     0,   879,   880,     0,   215,     0,  1046,  1048,  1049,
    1050,   300,   301,     0,     0,     0,     0,     0,     0,     0,
       0,   651,    74,    74,     0,     0,     0,     0,   653,   654,
    1054,     0,     0,     0,     0,     0,    74,     0,     0,     0,
       0,   910,     0,     0,     0,     0,     0,     0,     0,     0,
     405,     0,   655,   407,   408,   409,     0,     0,     0,     0,
      74,     0,     0,     0,     0,     0,   919,     0,     0,     0,
      74,     0,     0,   926,   927,     0,     0,   929,   800,     0,
     645,   646,   647,   648,   801,     0,     0,  -618,     0,     0,
      74,     0,     0,     0,   650,  -618,  -618,  -618,     0,     0,
    -618,  -618,  -618,   327,  -618,     0,     0,     0,     0,     0,
       0,     0,     0,  -618,  -618,   651,     0,   955,   340,   341,
       0,   652,   653,   654,  -618,  -618,   327,  -618,  -618,  -618,
    -618,  -618,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   340,   341,     0,     0,   981,   655,     0,   210,   656,
       0,   210,   210,   210,   300,   347,   348,   349,   350,   351,
     352,     0,     0,     0,     0,     0,     0,   231,     0,     0,
     514,   210,  -618,   210,   210,   525,   345,   346,   347,   348,
     349,   350,   351,   352,     0,     0,   327,     0,     0,     0,
      74,     0,    74,     0,     0,  -618,     0,     0,     0,     0,
      74,   340,   341,     0,     0,  1018,     0,  1019,     0,     0,
       0,     0,    74,  1020,    74,    74,     0,  -618,  -618,     0,
    -618,     0,     0,   227,  -618,     0,  -618,     0,  -618,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   348,
     349,   350,   351,   352,     0,    74,     0,     0,     0,     0,
       0,     0,    74,     0,     0,     0,     0,     0,     0,     0,
     607,   609,     0,     0,     0,     0,     0,     0,   210,   264,
       0,     0,     0,   524,   527,   528,   529,   530,   531,   532,
     533,   534,   535,   536,   537,   538,   539,   540,   541,   542,
     543,   544,   545,   546,   547,   548,   549,   550,   551,   552,
       0,   210,     0,     0,   609,     0,    74,   264,     0,    74,
       0,    74,     0,     0,     0,     0,     0,    74,     0,     0,
      74,    74,     0,     0,     0,     0,     0,    74,    74,     0,
     110,     0,   110,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   602,
     604,     0,    74,     0,     0,    74,    74,   606,   210,   210,
       0,   692,     0,   210,    74,   602,   604,   210,     0,   644,
       0,   645,   646,   647,   648,   649,     0,   110,     0,     0,
       0,   275,     0,     0,     0,   650,   626,     0,     0,     0,
       0,   631,     0,     0,     0,     0,     0,    74,    74,     0,
       0,     0,   210,     0,     0,   210,   651,   275,     0,     0,
       0,     0,   652,   653,   654,     0,     0,   210,     0,   370,
     380,   380,   380,     0,     0,   525,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    74,   655,     0,     0,
     656,     0,     0,   682,   683,     0,     0,     0,     0,     0,
      74,     0,     0,   657,     0,     0,     0,   753,     0,   210,
       0,    74,     0,     0,    74,     0,     0,     0,    74,    74,
     609,   264,    74,   644,     0,   645,   646,   647,   648,   649,
       0,   111,     0,   111,     0,     0,     0,     0,     0,   650,
       0,     0,     0,     0,     0,     0,     0,     0,   768,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     651,     0,    74,     0,     0,     0,   652,   653,   654,     0,
       0,     0,     0,   210,     0,     0,     0,   210,   111,     0,
       0,   110,   276,     0,     0,     0,     0,     0,     0,   210,
      74,   655,   819,   644,   656,   645,   646,   647,   648,   649,
       0,    74,     0,     0,     0,   210,     0,   743,   276,   650,
       0,     0,     0,   837,     0,   110,     0,     0,   210,   210,
     371,   381,   381,   381,     0,     0,   110,     0,   110,     0,
     651,     0,     0,     0,     0,     0,   652,   653,   654,     0,
       0,     0,     0,     0,     0,     0,   210,     0,     0,   275,
      74,     0,    74,     0,     0,     0,     0,     0,    74,     0,
      74,   655,     0,     0,   656,     0,   872,     0,     0,   800,
       0,   645,   646,   647,   648,   801,     0,     0,     0,     0,
       0,     0,     0,     0,   110,   650,    74,     0,     0,   110,
     210,     0,     0,     0,   606,   821,     0,   824,   826,     0,
       0,   110,   275,     0,   832,   834,   651,     0,     0,     0,
       0,   210,   652,   653,   654,     0,   114,   912,   114,     0,
       0,     0,     0,     0,     0,    81,     0,    81,     0,     0,
       0,     0,   111,     0,   110,     0,     0,   655,     0,     0,
     656,   769,     0,   645,   646,   647,   648,   801,   867,     0,
     928,     0,   824,   826,     0,   832,   834,   650,     0,     0,
       0,     0,     0,   114,   210,     0,   111,   277,   264,     0,
       0,     0,    81,     0,     0,     0,     0,   111,   651,   111,
       0,     0,     0,     0,     0,   653,   654,     0,     0,     0,
       0,     0,     0,   277,     0,     0,     0,     0,     0,     0,
     276,     0,     0,     0,     0,   372,   382,   382,     0,   655,
       0,     0,     0,     0,   367,   210,     0,     0,   110,   913,
       0,     0,     0,     0,     0,     0,     0,   915,   110,     0,
       0,     0,     0,     0,     0,   111,     0,     0,     0,     0,
     111,     0,     0,     0,     0,     0,   275,     0,   210,     0,
       0,     0,   111,   276,   327,   328,   329,   330,   331,   332,
     333,   334,     0,   336,   337,   915,   210,     0,     0,   340,
     341,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   111,     0,     0,     0,     0,
       0,     0,     0,   113,     0,   113,   275,     0,     0,     0,
       0,     0,   343,   344,   345,   346,   347,   348,   349,   350,
     351,   352,     0,     0,     0,     0,     0,   114,     0,     0,
       0,     0,     0,     0,     0,     0,    81,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     113,     0,     0,     0,     0,     0,     0,     0,   110,     0,
     110,   114,     0,     0,     0,     0,     0,     0,     0,     0,
      81,     0,   114,     0,   114,     0,     0,     0,     0,   111,
       0,    81,   110,    81,     0,     0,     0,     0,     0,   111,
       0,     0,     0,     0,     0,   277,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   276,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     110,     0,     0,   210,     0,   275,     0,     0,     0,     0,
     114,     0,     0,     0,     0,   114,     0,     0,     0,    81,
       0,     0,     0,     0,    81,     0,     0,   114,   277,     0,
       0,     0,     0,     0,     0,     0,    81,   276,     0,   523,
       0,     0,     0,     0,     0,     0,     0,   698,     0,     0,
       0,     0,     0,     0,   110,     0,     0,   110,     0,   110,
     114,     0,     0,     0,   275,     0,     0,     0,     0,    81,
       0,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,   338,   339,   113,     0,   340,   341,     0,   111,
       0,   111,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   110,     0,     0,     0,     0,     0,
       0,     0,   110,   111,     0,     0,     0,   342,   113,   343,
     344,   345,   346,   347,   348,   349,   350,   351,   352,   113,
       0,   113,     0,     0,     0,     0,     0,  -245,     0,     0,
       0,     0,     0,     0,   114,   110,   110,     0,     0,     0,
       0,   111,     0,    81,   114,     0,   276,     0,     0,     0,
       0,     0,     0,    81,     0,     0,     0,     0,     0,   380,
       0,     0,   277,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   110,     0,     0,   113,     0,     0,
       0,     0,   113,     0,     0,     0,     0,   327,  -619,  -619,
    -619,  -619,   332,   333,   113,   111,  -619,  -619,   111,   110,
     111,     0,   340,   341,     0,   276,   110,   110,     0,     0,
     110,     0,   277,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   113,     0,     0,
       0,     0,     0,     0,     0,   343,   344,   345,   346,   347,
     348,   349,   350,   351,   352,   111,     0,     0,     0,     0,
     110,   380,     0,   111,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   114,     0,   114,     0,     0,     0,
       0,     0,     0,    81,   977,    81,     0,     0,   110,     0,
       0,     0,     0,     0,     0,     0,   111,   111,   114,   110,
       0,     0,     0,     0,     0,     0,     0,    81,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     381,   113,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   113,     0,     0,     0,   111,   114,     0,     0,     0,
       0,   277,     0,     0,     0,    81,     0,     0,   110,     0,
     110,     0,     0,     0,     0,     0,   110,     0,   110,     0,
     111,     0,     0,     0,     0,     0,     0,   111,   111,     0,
       0,   111,     0,     0,     0,     0,     0,     0,     0,     0,
     799,     0,     0,     0,     0,     0,     0,     0,     0,   751,
     114,     0,     0,   114,     0,   114,     0,     0,     0,    81,
     277,     0,    81,     0,    81,     0,     0,     0,     0,     0,
     523,   111,   381,   327,   328,   329,   330,   331,   332,   333,
     334,   335,   336,   337,   338,   339,     0,     0,   340,   341,
       0,     0,     0,     0,     0,   978,     0,     0,     0,   111,
     114,     0,     0,     0,     0,     0,     0,     0,   114,    81,
     111,   113,     0,   113,     0,     0,     0,    81,     0,   342,
       0,   343,   344,   345,   346,   347,   348,   349,   350,   351,
     352,     0,     0,     0,     0,   113,     0,     0,     0,     0,
       0,   114,   114,     0,     0,     0,     0,     0,     0,     0,
      81,    81,     0,     0,     0,     0,     0,     0,     0,   111,
       0,   111,     0,     0,     0,   382,     0,   111,   698,   111,
       0,     0,     0,   113,     0,     0,     0,     0,     0,     0,
     114,     0,     0,     0,     0,     0,     0,     0,     0,    81,
       0,     0,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   114,     0,   340,   341,     0,
       0,     0,   114,   114,    81,     0,   114,     0,     0,     0,
       0,    81,    81,     0,     0,    81,     0,   113,     0,     0,
     113,     0,   113,     0,     0,     0,     0,     0,   342,     0,
     343,   344,   345,   346,   347,   348,   349,   350,   351,   352,
       0,     0,     0,     0,     0,     0,   114,   382,   327,   328,
     329,   330,   331,   332,   333,    81,     0,   336,   337,     0,
       0,     0,     0,   340,   341,     0,     0,   113,     0,     0,
     979,     0,     0,     0,   114,   113,     0,     0,     0,   975,
       0,     0,     0,    81,     0,   114,     0,     0,     0,     0,
       0,     0,     0,     0,    81,     0,   343,   344,   345,   346,
     347,   348,   349,   350,   351,   352,     0,     0,   113,   113,
     327,   328,   329,   330,   331,   332,   333,   334,   335,   336,
     337,   338,   339,     0,     0,   340,   341,     0,     0,     0,
       0,     0,     0,     0,   114,     0,   114,     0,     0,     0,
       0,     0,   114,    81,   114,    81,     0,   113,     0,     0,
       0,    81,     0,    81,     0,     0,   342,     0,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,     0,     0,
       0,     0,   113,     0,     0,     0,     0,     0,     0,   113,
     113,  -618,     4,   113,     5,     6,     7,     8,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,   113,     0,     0,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,     0,
      41,    42,     0,    43,    44,    45,     0,    46,    47,     0,
       0,   113,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   113,     0,     0,     0,     0,    48,     0,     0,
      49,    50,     0,    51,    52,     0,    53,     0,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,  -600,     0,
       0,     0,     0,     0,     0,     0,  -600,  -600,  -600,     0,
       0,  -600,  -600,  -600,     0,  -600,     0,    63,    64,    65,
       0,   113,     0,   113,  -600,  -600,  -600,  -600,     0,   113,
    -618,   113,  -618,     0,     0,  -600,  -600,     0,  -600,  -600,
    -600,  -600,  -600,     0,     0,   327,   328,   329,   330,   331,
     332,   333,   334,   335,   336,   337,  -619,  -619,     0,     0,
     340,   341,     0,     0,     0,     0,  -600,  -600,  -600,  -600,
    -600,  -600,  -600,  -600,  -600,  -600,  -600,  -600,  -600,     0,
       0,  -600,  -600,  -600,     0,   755,  -600,     0,     0,     0,
       0,     0,  -600,   343,   344,   345,   346,   347,   348,   349,
     350,   351,   352,     0,     0,     0,  -600,     0,     0,  -600,
       0,  -106,  -600,  -600,  -600,  -600,  -600,  -600,  -600,  -600,
    -600,  -600,  -600,  -600,     0,     0,     0,     0,  -600,  -600,
    -600,  -600,  -600,  -503,     0,  -600,  -600,  -600,     0,  -600,
       0,  -503,  -503,  -503,     0,     0,  -503,  -503,  -503,     0,
    -503,     0,     0,     0,     0,     0,     0,     0,  -503,     0,
    -503,  -503,  -503,     0,     0,     0,     0,     0,     0,     0,
    -503,  -503,     0,  -503,  -503,  -503,  -503,  -503,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -503,  -503,  -503,  -503,  -503,  -503,  -503,  -503,  -503,
    -503,  -503,  -503,  -503,     0,     0,  -503,  -503,  -503,     0,
    -503,  -503,     0,     0,     0,     0,     0,  -503,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -503,     0,     0,  -503,     0,  -503,  -503,  -503,  -503,
    -503,  -503,  -503,  -503,  -503,  -503,  -503,  -503,  -503,     0,
       0,     0,     0,     0,  -503,  -503,  -503,  -503,  -506,     0,
    -503,  -503,  -503,     0,  -503,     0,  -506,  -506,  -506,     0,
       0,  -506,  -506,  -506,     0,  -506,     0,     0,     0,     0,
       0,     0,     0,  -506,     0,  -506,  -506,  -506,     0,     0,
       0,     0,     0,     0,     0,  -506,  -506,     0,  -506,  -506,
    -506,  -506,  -506,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -506,  -506,  -506,  -506,
    -506,  -506,  -506,  -506,  -506,  -506,  -506,  -506,  -506,     0,
       0,  -506,  -506,  -506,     0,  -506,  -506,     0,     0,     0,
       0,     0,  -506,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -506,     0,     0,  -506,
       0,  -506,  -506,  -506,  -506,  -506,  -506,  -506,  -506,  -506,
    -506,  -506,  -506,  -506,     0,     0,     0,     0,     0,  -506,
    -506,  -506,  -506,  -601,     0,  -506,  -506,  -506,     0,  -506,
       0,  -601,  -601,  -601,     0,     0,  -601,  -601,  -601,     0,
    -601,     0,     0,     0,     0,     0,     0,     0,     0,  -601,
    -601,  -601,  -601,     0,     0,     0,     0,     0,     0,     0,
    -601,  -601,     0,  -601,  -601,  -601,  -601,  -601,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,
    -601,  -601,  -601,  -601,     0,     0,  -601,  -601,  -601,     0,
       0,  -601,     0,     0,     0,     0,     0,  -601,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -601,     0,     0,  -601,     0,     0,  -601,  -601,  -601,
    -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,  -601,     0,
       0,     0,     0,  -601,  -601,  -601,  -601,  -601,  -602,     0,
    -601,  -601,  -601,     0,  -601,     0,  -602,  -602,  -602,     0,
       0,  -602,  -602,  -602,     0,  -602,     0,     0,     0,     0,
       0,     0,     0,     0,  -602,  -602,  -602,  -602,     0,     0,
       0,     0,     0,     0,     0,  -602,  -602,     0,  -602,  -602,
    -602,  -602,  -602,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -602,  -602,  -602,  -602,
    -602,  -602,  -602,  -602,  -602,  -602,  -602,  -602,  -602,     0,
       0,  -602,  -602,  -602,     0,     0,  -602,     0,     0,     0,
       0,     0,  -602,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -602,     0,     0,  -602,
       0,     0,  -602,  -602,  -602,  -602,  -602,  -602,  -602,  -602,
    -602,  -602,  -602,  -602,     0,     0,     0,     0,  -602,  -602,
    -602,  -602,  -602,  -293,     0,  -602,  -602,  -602,     0,  -602,
       0,  -293,  -293,  -293,     0,     0,  -293,  -293,  -293,     0,
    -293,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -293,  -293,  -293,     0,     0,     0,     0,     0,     0,     0,
    -293,  -293,     0,  -293,  -293,  -293,  -293,  -293,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,     0,     0,  -293,  -293,  -293,     0,
     756,  -293,     0,     0,     0,     0,     0,  -293,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -293,     0,     0,  -293,     0,  -108,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,     0,
       0,     0,     0,     0,  -293,  -293,  -293,  -293,  -430,     0,
    -293,  -293,  -293,     0,  -293,     0,  -430,  -430,  -430,     0,
       0,  -430,  -430,  -430,     0,  -430,     0,     0,     0,     0,
       0,     0,     0,     0,  -430,  -430,  -430,     0,     0,     0,
       0,     0,     0,     0,     0,  -430,  -430,     0,  -430,  -430,
    -430,  -430,  -430,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -430,  -430,  -430,  -430,
    -430,  -430,  -430,  -430,  -430,  -430,  -430,  -430,  -430,     0,
       0,  -430,  -430,  -430,     0,     0,  -430,     0,     0,     0,
       0,     0,  -430,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -430,     0,     0,     0,
       0,     0,  -430,     0,  -430,  -430,  -430,  -430,  -430,  -430,
    -430,  -430,  -430,  -430,     0,     0,     0,     0,  -430,  -430,
    -430,  -430,  -430,  -285,   227,  -430,  -430,  -430,     0,  -430,
       0,  -285,  -285,  -285,     0,     0,  -285,  -285,  -285,     0,
    -285,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -285,  -285,  -285,     0,     0,     0,     0,     0,     0,     0,
    -285,  -285,     0,  -285,  -285,  -285,  -285,  -285,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -285,  -285,  -285,  -285,  -285,  -285,  -285,  -285,  -285,
    -285,  -285,  -285,  -285,     0,     0,  -285,  -285,  -285,     0,
       0,  -285,     0,     0,     0,     0,     0,  -285,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -285,     0,     0,  -285,     0,     0,  -285,  -285,  -285,
    -285,  -285,  -285,  -285,  -285,  -285,  -285,  -285,  -285,     0,
       0,     0,     0,     0,  -285,  -285,  -285,  -285,  -420,     0,
    -285,  -285,  -285,     0,  -285,     0,  -420,  -420,  -420,     0,
       0,  -420,  -420,  -420,     0,  -420,     0,     0,     0,     0,
       0,     0,     0,     0,  -420,  -420,  -420,     0,     0,     0,
       0,     0,     0,     0,     0,  -420,  -420,     0,  -420,  -420,
    -420,  -420,  -420,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -420,  -420,  -420,  -420,
    -420,  -420,  -420,  -420,  -420,  -420,  -420,  -420,  -420,     0,
       0,  -420,  -420,  -420,     0,     0,  -420,     0,     0,     0,
       0,     0,  -420,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -420,     0,     0,     0,
       0,     0,  -420,     0,  -420,  -420,  -420,  -420,  -420,  -420,
    -420,  -420,  -420,  -420,     0,     0,     0,     0,  -420,  -420,
    -420,  -420,  -420,  -300,  -420,  -420,  -420,  -420,     0,  -420,
       0,  -300,  -300,  -300,     0,     0,  -300,  -300,  -300,     0,
    -300,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -300,  -300,     0,     0,     0,     0,     0,     0,     0,     0,
    -300,  -300,     0,  -300,  -300,  -300,  -300,  -300,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -300,  -300,  -300,  -300,  -300,  -300,  -300,  -300,  -300,
    -300,  -300,  -300,  -300,     0,     0,  -300,  -300,  -300,     0,
       0,  -300,     0,     0,     0,     0,     0,  -300,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -300,     0,     0,     0,     0,     0,  -300,     0,  -300,
    -300,  -300,  -300,  -300,  -300,  -300,  -300,  -300,  -300,     0,
       0,     0,     0,     0,  -300,  -300,  -300,  -300,  -600,   224,
    -300,  -300,  -300,     0,  -300,     0,  -600,  -600,  -600,     0,
       0,     0,  -600,  -600,     0,  -600,     0,     0,     0,     0,
       0,     0,     0,     0,  -600,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,  -600,  -600,     0,  -600,  -600,
    -600,  -600,  -600,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -600,  -600,  -600,  -600,
    -600,  -600,  -600,  -600,  -600,  -600,  -600,  -600,  -600,     0,
       0,  -600,  -600,  -600,     0,   700,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -600,     0,     0,     0,
       0,  -106,  -600,     0,  -600,  -600,  -600,  -600,  -600,  -600,
    -600,  -600,  -600,  -600,     0,     0,     0,     0,  -600,  -600,
    -600,  -600,   -97,  -293,     0,  -600,     0,  -600,     0,  -600,
       0,  -293,  -293,  -293,     0,     0,     0,  -293,  -293,     0,
    -293,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -293,  -293,     0,  -293,  -293,  -293,  -293,  -293,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,
    -293,  -293,  -293,  -293,     0,     0,  -293,  -293,  -293,     0,
     701,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -293,     0,     0,     0,     0,  -108,  -293,     0,  -293,
    -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,  -293,     0,
       0,     0,     0,     0,  -293,  -293,  -293,   -99,     0,     0,
    -293,     0,  -293,   248,  -293,     5,     6,     7,     8,     9,
    -618,  -618,  -618,    10,    11,     0,     0,  -618,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,    28,    29,   249,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,     0,    43,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,     0,
       0,    49,    50,     0,    51,    52,     0,    53,     0,     0,
      54,    55,    56,    57,    58,    59,    60,    61,    62,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    63,    64,
      65,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -618,   248,  -618,     5,     6,     7,     8,     9,     0,
       0,  -618,    10,    11,     0,  -618,  -618,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,    29,   249,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,     0,
      41,    42,     0,    43,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,     0,     0,
      49,    50,     0,    51,    52,     0,    53,     0,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    63,    64,    65,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -618,   248,  -618,     5,     6,     7,     8,     9,     0,     0,
    -618,    10,    11,     0,     0,  -618,    12,  -618,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,    28,    29,   249,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,     0,    41,
      42,     0,    43,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    48,     0,     0,    49,
      50,     0,    51,    52,     0,    53,     0,     0,    54,    55,
      56,    57,    58,    59,    60,    61,    62,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    63,    64,    65,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -618,
     248,  -618,     5,     6,     7,     8,     9,     0,     0,  -618,
      10,    11,     0,     0,  -618,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,    27,     0,
       0,     0,     0,     0,    28,    29,   249,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,     0,    41,    42,
       0,    43,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    48,     0,     0,    49,    50,
       0,    51,    52,     0,    53,     0,     0,    54,    55,    56,
      57,    58,    59,    60,    61,    62,     0,     0,     0,     0,
       0,     0,     0,   248,     0,     5,     6,     7,     8,     9,
       0,  -618,  -618,    10,    11,    63,    64,    65,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,  -618,     0,
    -618,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,    28,    29,   249,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,     0,    43,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,     0,
       0,    49,    50,     0,    51,    52,     0,    53,     0,     0,
      54,    55,    56,    57,    58,    59,    60,    61,    62,     0,
       0,     0,     0,     0,     0,     0,   248,     0,     5,     6,
       7,     8,     9,     0,     0,     0,    10,    11,    63,    64,
      65,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,  -618,     0,  -618,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
      28,    29,   249,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,     0,    41,    42,     0,    43,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    48,     0,     0,   250,    50,     0,    51,    52,     0,
      53,     0,     0,    54,    55,    56,    57,    58,    59,    60,
      61,    62,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    63,    64,    65,     0,     0,     0,     0,     0,     0,
       0,     0,  -618,     0,  -618,   248,  -618,     5,     6,     7,
       8,     9,     0,     0,     0,    10,    11,     0,     0,     0,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,     0,    28,
      29,   249,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,     0,    41,    42,     0,    43,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      48,     0,     0,    49,    50,     0,    51,    52,     0,    53,
       0,     0,    54,    55,    56,    57,    58,    59,    60,    61,
      62,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      63,    64,    65,     0,     0,     0,     0,     0,     0,     0,
       0,  -618,     0,  -618,   248,  -618,     5,     6,     7,     8,
       9,     0,     0,     0,    10,    11,     0,     0,     0,    12,
       0,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
     249,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,     0,    41,    42,     0,    43,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    48,
       0,     0,    49,    50,     0,    51,    52,     0,    53,     0,
       0,    54,    55,    56,    57,    58,    59,    60,    61,    62,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    63,
      64,    65,     0,     0,  -618,     4,     0,     5,     6,     7,
       8,     9,  -618,     0,  -618,    10,    11,     0,     0,     0,
      12,     0,    13,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,     0,    20,    21,    22,    23,    24,    25,
      26,     0,     0,    27,     0,     0,     0,     0,     0,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,     0,    41,    42,     0,    43,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      48,     0,     0,    49,    50,     0,    51,    52,     0,    53,
       0,     0,    54,    55,    56,    57,    58,    59,    60,    61,
      62,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      63,    64,    65,     0,     0,  -618,     0,     0,     0,     0,
       0,     0,     0,  -618,   248,  -618,     5,     6,     7,     8,
       9,     0,     0,  -618,    10,    11,     0,     0,     0,    12,
       0,    13,    14,    15,    16,    17,    18,    19,     0,     0,
       0,     0,     0,    20,    21,    22,    23,    24,    25,    26,
       0,     0,    27,     0,     0,     0,     0,     0,    28,    29,
     249,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,     0,    41,    42,     0,    43,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    48,
       0,     0,    49,    50,     0,    51,    52,     0,    53,     0,
       0,    54,    55,    56,    57,    58,    59,    60,    61,    62,
       0,     0,     0,     0,     0,     0,     0,   248,     0,     5,
       6,     7,     8,     9,     0,     0,     0,    10,    11,    63,
      64,    65,    12,     0,    13,    14,    15,    16,    17,    18,
      19,     0,  -618,     0,  -618,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,    29,   249,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,     0,    41,    42,     0,    43,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    48,     0,     0,    49,    50,     0,    51,    52,
       0,    53,     0,     0,    54,    55,    56,    57,    58,    59,
      60,    61,    62,     0,  -618,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    63,    64,    65,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,  -618,     0,  -618,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,   200,     0,
       0,     0,     0,     0,     0,    29,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,   201,    41,    42,
       0,    43,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   202,     0,     0,   203,    50,
       0,    51,    52,     0,   204,   205,   206,    54,    55,    56,
      57,    58,    59,    60,    61,    62,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    63,   207,    65,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
     231,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,     0,    43,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   202,     0,
       0,   203,    50,     0,    51,    52,     0,     0,     0,     0,
      54,    55,    56,    57,    58,    59,    60,    61,    62,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    63,    64,
      65,    12,     0,    13,    14,    15,    16,    17,    18,    19,
       0,   302,     0,   303,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,     0,    41,    42,     0,    43,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   202,     0,     0,   203,    50,     0,    51,    52,     0,
       0,     0,     0,    54,    55,    56,    57,    58,    59,    60,
      61,    62,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     8,     9,     0,     0,     0,    10,
      11,    63,    64,    65,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,   231,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,    27,     0,     0,
       0,     0,     0,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,     0,    41,    42,     0,
      43,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    48,     0,     0,    49,    50,     0,
      51,    52,     0,    53,     0,     0,    54,    55,    56,    57,
      58,    59,    60,    61,    62,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     8,     9,     0,
       0,     0,    10,    11,    63,    64,    65,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,   498,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,    29,   249,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,     0,
      41,    42,     0,    43,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,     0,     0,
      49,    50,     0,    51,    52,     0,    53,     0,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    63,    64,    65,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     498,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,     0,     0,     0,   142,   143,
     144,   384,   385,   386,   387,   149,   150,   151,     0,     0,
       0,     0,     0,   152,   153,   154,   155,   388,   389,   390,
     391,   160,    37,    38,   392,    40,     0,     0,     0,     0,
       0,     0,     0,     0,   162,   163,   164,   165,   166,   167,
     168,   169,   170,     0,     0,   171,   172,     0,     0,   173,
     174,   175,   176,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   177,   178,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,     0,   189,   190,     0,     0,     0,
       0,     0,     0,   191,   393,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,     0,
       0,     0,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,     0,     0,     0,     0,     0,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   280,   281,   161,   282,
       0,     0,     0,     0,     0,     0,     0,     0,   162,   163,
     164,   165,   166,   167,   168,   169,   170,     0,     0,   171,
     172,     0,     0,   173,   174,   175,   176,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   177,   178,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,     0,   189,
     190,     0,     0,     0,     0,     0,     0,   191,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,     0,     0,     0,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,     0,     0,     0,     0,     0,
     152,   153,   154,   155,   156,   157,   158,   159,   160,   233,
       0,   161,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   162,   163,   164,   165,   166,   167,   168,   169,   170,
       0,     0,   171,   172,     0,     0,   173,   174,   175,   176,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     177,   178,     0,     0,    55,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,     0,   189,   190,     0,     0,     0,     0,     0,     0,
     191,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,     0,     0,     0,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,     0,     0,
       0,     0,     0,   152,   153,   154,   155,   156,   157,   158,
     159,   160,     0,     0,   161,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   162,   163,   164,   165,   166,   167,
     168,   169,   170,     0,     0,   171,   172,     0,     0,   173,
     174,   175,   176,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   177,   178,     0,     0,    55,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,     0,   189,   190,     0,     0,     0,
       0,     0,     0,   191,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,     0,     0,
       0,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,     0,     0,     0,     0,     0,   152,   153,   154,   155,
     156,   157,   158,   159,   160,     0,     0,   161,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   162,   163,   164,
     165,   166,   167,   168,   169,   170,     0,     0,   171,   172,
       0,     0,   173,   174,   175,   176,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   177,   178,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,     0,   189,   190,
       5,     6,     7,     0,     9,     0,   191,     0,    10,    11,
       0,     0,     0,    12,     0,    13,    14,    15,   238,   239,
      18,    19,     0,     0,     0,     0,     0,   240,   241,   242,
      23,    24,    25,    26,     0,     0,   200,     0,     0,     0,
       0,     0,     0,   268,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,     0,    41,    42,     0,    43,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   269,     0,     0,   203,    50,     0,    51,
      52,     0,     0,     0,     0,    54,    55,    56,    57,    58,
      59,    60,    61,    62,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,     0,
       0,     0,    12,   270,    13,    14,    15,   238,   239,    18,
      19,   271,     0,     0,     0,     0,   240,   241,   242,    23,
      24,    25,    26,     0,     0,   200,     0,     0,     0,     0,
       0,     0,   268,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,     0,    41,    42,     0,    43,    44,
      45,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   269,     0,     0,   203,    50,     0,    51,    52,
       0,     0,     0,     0,    54,    55,    56,    57,    58,    59,
      60,    61,    62,     0,     0,     0,     0,     0,     5,     6,
       7,     8,     9,     0,     0,     0,    10,    11,     0,     0,
       0,    12,   270,    13,    14,    15,    16,    17,    18,    19,
     518,     0,     0,     0,     0,    20,    21,    22,    23,    24,
      25,    26,     0,     0,    27,     0,     0,     0,     0,     0,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,     0,    41,    42,     0,    43,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    48,     0,     0,    49,    50,     0,    51,    52,     0,
      53,     0,     0,    54,    55,    56,    57,    58,    59,    60,
      61,    62,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    63,    64,    65,    12,     0,    13,    14,    15,    16,
      17,    18,    19,     0,     0,     0,     0,     0,    20,    21,
      22,    23,    24,    25,    26,     0,     0,   200,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,   201,    41,    42,     0,
      43,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   202,     0,     0,   203,    50,     0,
      51,    52,     0,   204,   205,   206,    54,    55,    56,    57,
      58,    59,    60,    61,    62,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     8,     9,     0,
       0,     0,    10,    11,    63,   207,    65,    12,     0,    13,
      14,    15,    16,    17,    18,    19,     0,     0,     0,     0,
       0,    20,    21,    22,    23,    24,    25,    26,     0,     0,
      27,     0,     0,     0,     0,     0,    28,    29,     0,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,     0,
      41,    42,     0,    43,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    48,     0,     0,
      49,    50,     0,    51,    52,     0,    53,     0,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    63,    64,    65,
      12,     0,    13,    14,    15,   238,   239,    18,    19,     0,
       0,     0,     0,     0,   240,   241,   242,    23,    24,    25,
      26,     0,     0,   200,     0,     0,     0,     0,     0,     0,
      29,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,   201,    41,    42,     0,    43,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     202,     0,     0,   203,    50,     0,    51,    52,     0,   608,
     205,   206,    54,    55,    56,    57,    58,    59,    60,    61,
      62,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      63,   207,    65,    12,     0,    13,    14,    15,   238,   239,
      18,    19,     0,     0,     0,     0,     0,   240,   241,   242,
      23,    24,    25,    26,     0,     0,   200,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,   201,    41,    42,     0,    43,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   202,     0,     0,   203,    50,     0,    51,
      52,     0,   204,   205,     0,    54,    55,    56,    57,    58,
      59,    60,    61,    62,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    63,   207,    65,    12,     0,    13,    14,
      15,   238,   239,    18,    19,     0,     0,     0,     0,     0,
     240,   241,   242,    23,    24,    25,    26,     0,     0,   200,
       0,     0,     0,     0,     0,     0,    29,     0,     0,    32,
      33,    34,    35,    36,    37,    38,    39,    40,   201,    41,
      42,     0,    43,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   202,     0,     0,   203,
      50,     0,    51,    52,     0,     0,   205,   206,    54,    55,
      56,    57,    58,    59,    60,    61,    62,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    63,   207,    65,    12,
       0,    13,    14,    15,   238,   239,    18,    19,     0,     0,
       0,     0,     0,   240,   241,   242,    23,    24,    25,    26,
       0,     0,   200,     0,     0,     0,     0,     0,     0,    29,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,   201,    41,    42,     0,    43,    44,    45,     0,    46,
      47,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   202,
       0,     0,   203,    50,     0,    51,    52,     0,   608,   205,
       0,    54,    55,    56,    57,    58,    59,    60,    61,    62,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     5,
       6,     7,     0,     9,     0,     0,     0,    10,    11,    63,
     207,    65,    12,     0,    13,    14,    15,   238,   239,    18,
      19,     0,     0,     0,     0,     0,   240,   241,   242,    23,
      24,    25,    26,     0,     0,   200,     0,     0,     0,     0,
       0,     0,    29,     0,     0,    32,    33,    34,    35,    36,
      37,    38,    39,    40,   201,    41,    42,     0,    43,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   202,     0,     0,   203,    50,     0,    51,    52,
       0,     0,   205,     0,    54,    55,    56,    57,    58,    59,
      60,    61,    62,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    63,   207,    65,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,     0,     0,     0,     0,    20,
      21,    22,    23,    24,    25,    26,     0,     0,   200,     0,
       0,     0,     0,     0,     0,    29,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,     0,    41,    42,
       0,    43,    44,    45,     0,    46,    47,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   202,     0,     0,   203,    50,
       0,    51,    52,     0,   512,     0,     0,    54,    55,    56,
      57,    58,    59,    60,    61,    62,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,    63,   207,    65,    12,     0,
      13,    14,    15,   238,   239,    18,    19,     0,     0,     0,
       0,     0,   240,   241,   242,    23,    24,    25,    26,     0,
       0,   200,     0,     0,     0,     0,     0,     0,    29,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,     0,    43,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   202,     0,
       0,   203,    50,     0,    51,    52,     0,   204,     0,     0,
      54,    55,    56,    57,    58,    59,    60,    61,    62,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     5,     6,
       7,     0,     9,     0,     0,     0,    10,    11,    63,   207,
      65,    12,     0,    13,    14,    15,   238,   239,    18,    19,
       0,     0,     0,     0,     0,   240,   241,   242,    23,    24,
      25,    26,     0,     0,   200,     0,     0,     0,     0,     0,
       0,    29,     0,     0,    32,    33,    34,    35,    36,    37,
      38,    39,    40,     0,    41,    42,     0,    43,    44,    45,
       0,    46,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   202,     0,     0,   203,    50,     0,    51,    52,     0,
     818,     0,     0,    54,    55,    56,    57,    58,    59,    60,
      61,    62,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,    63,   207,    65,    12,     0,    13,    14,    15,   238,
     239,    18,    19,     0,     0,     0,     0,     0,   240,   241,
     242,    23,    24,    25,    26,     0,     0,   200,     0,     0,
       0,     0,     0,     0,    29,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,     0,    41,    42,     0,
      43,    44,    45,     0,    46,    47,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   202,     0,     0,   203,    50,     0,
      51,    52,     0,   512,     0,     0,    54,    55,    56,    57,
      58,    59,    60,    61,    62,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,    63,   207,    65,    12,     0,    13,
      14,    15,   238,   239,    18,    19,     0,     0,     0,     0,
       0,   240,   241,   242,    23,    24,    25,    26,     0,     0,
     200,     0,     0,     0,     0,     0,     0,    29,     0,     0,
      32,    33,    34,    35,    36,    37,    38,    39,    40,     0,
      41,    42,     0,    43,    44,    45,     0,    46,    47,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   202,     0,     0,
     203,    50,     0,    51,    52,     0,   608,     0,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     5,     6,     7,
       0,     9,     0,     0,     0,    10,    11,    63,   207,    65,
      12,     0,    13,    14,    15,   238,   239,    18,    19,     0,
       0,     0,     0,     0,   240,   241,   242,    23,    24,    25,
      26,     0,     0,   200,     0,     0,     0,     0,     0,     0,
      29,     0,     0,    32,    33,    34,    35,    36,    37,    38,
      39,    40,     0,    41,    42,     0,    43,    44,    45,     0,
      46,    47,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     202,     0,     0,   203,    50,     0,    51,    52,     0,     0,
       0,     0,    54,    55,    56,    57,    58,    59,    60,    61,
      62,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
      63,   207,    65,    12,     0,    13,    14,    15,    16,    17,
      18,    19,     0,     0,     0,     0,     0,    20,    21,    22,
      23,    24,    25,    26,     0,     0,    27,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,     0,    41,    42,     0,    43,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   202,     0,     0,   203,    50,     0,    51,
      52,     0,     0,     0,     0,    54,    55,    56,    57,    58,
      59,    60,    61,    62,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    63,    64,    65,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,   200,
       0,     0,     0,     0,     0,     0,    29,     0,     0,    32,
      33,    34,    35,    36,    37,    38,    39,    40,     0,    41,
      42,     0,    43,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   202,     0,     0,   203,
      50,     0,    51,    52,     0,     0,     0,     0,    54,    55,
      56,    57,    58,    59,    60,    61,    62,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    63,   207,    65,    12,
       0,    13,    14,    15,   238,   239,    18,    19,     0,     0,
       0,     0,     0,   240,   241,   242,    23,    24,    25,    26,
       0,     0,   200,     0,     0,     0,     0,     0,     0,   268,
       0,     0,    32,    33,    34,    35,    36,    37,    38,    39,
      40,     0,    41,    42,     0,    43,    44,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   269,
       0,     0,   323,    50,     0,    51,    52,     0,   324,     0,
       0,    54,    55,    56,    57,    58,    59,    60,    61,    62,
       0,     0,     0,     0,     0,     5,     6,     7,     0,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,   270,
      13,    14,    15,   238,   239,    18,    19,     0,     0,     0,
       0,     0,   240,   241,   242,    23,    24,    25,    26,     0,
       0,   200,     0,     0,     0,     0,     0,     0,   268,     0,
       0,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,     0,    43,    44,    45,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   365,     0,
       0,    49,    50,     0,    51,    52,     0,    53,     0,     0,
      54,    55,    56,    57,    58,    59,    60,    61,    62,     0,
       0,     0,     0,     0,     5,     6,     7,     0,     9,     0,
       0,     0,    10,    11,     0,     0,     0,    12,   270,    13,
      14,    15,   238,   239,    18,    19,     0,     0,     0,     0,
       0,   240,   241,   242,    23,    24,    25,    26,     0,     0,
     200,     0,     0,     0,     0,     0,     0,   268,     0,     0,
      32,    33,    34,   373,    36,    37,    38,   374,    40,     0,
      41,    42,     0,    43,    44,    45,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   375,     0,     0,   376,     0,     0,
     203,    50,     0,    51,    52,     0,     0,     0,     0,    54,
      55,    56,    57,    58,    59,    60,    61,    62,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,     0,     0,     0,    12,   270,    13,    14,
      15,   238,   239,    18,    19,     0,     0,     0,     0,     0,
     240,   241,   242,    23,    24,    25,    26,     0,     0,   200,
       0,     0,     0,     0,     0,     0,   268,     0,     0,    32,
      33,    34,   373,    36,    37,    38,   374,    40,     0,    41,
      42,     0,    43,    44,    45,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   376,     0,     0,   203,
      50,     0,    51,    52,     0,     0,     0,     0,    54,    55,
      56,    57,    58,    59,    60,    61,    62,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,     0,     0,     0,    12,   270,    13,    14,    15,
     238,   239,    18,    19,     0,     0,     0,     0,     0,   240,
     241,   242,    23,    24,    25,    26,     0,     0,   200,     0,
       0,     0,     0,     0,     0,   268,     0,     0,    32,    33,
      34,    35,    36,    37,    38,    39,    40,     0,    41,    42,
       0,    43,    44,    45,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   269,     0,     0,   323,    50,
       0,    51,    52,     0,     0,     0,     0,    54,    55,    56,
      57,    58,    59,    60,    61,    62,     0,     0,     0,     0,
       0,     5,     6,     7,     0,     9,     0,     0,     0,    10,
      11,     0,     0,     0,    12,   270,    13,    14,    15,   238,
     239,    18,    19,     0,     0,     0,     0,     0,   240,   241,
     242,    23,    24,    25,    26,     0,     0,   200,     0,     0,
       0,     0,     0,     0,   268,     0,     0,    32,    33,    34,
      35,    36,    37,    38,    39,    40,     0,    41,    42,     0,
      43,    44,    45,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   894,     0,     0,   203,    50,     0,
      51,    52,     0,     0,     0,     0,    54,    55,    56,    57,
      58,    59,    60,    61,    62,     0,     0,     0,     0,     0,
       5,     6,     7,     0,     9,     0,     0,     0,    10,    11,
       0,     0,     0,    12,   270,    13,    14,    15,   238,   239,
      18,    19,     0,     0,     0,     0,     0,   240,   241,   242,
      23,    24,    25,    26,     0,     0,   200,     0,     0,     0,
       0,     0,     0,   268,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,     0,    41,    42,     0,    43,
      44,    45,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   974,     0,     0,   203,    50,     0,    51,
      52,     0,     0,     0,     0,    54,    55,    56,    57,    58,
      59,    60,    61,    62,     0,     0,     0,     0,     0,     0,
     553,   554,     0,     0,   555,     0,     0,     0,     0,     0,
       0,     0,     0,   270,   162,   163,   164,   165,   166,   167,
     168,   169,   170,     0,     0,   171,   172,     0,     0,   173,
     174,   175,   176,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   177,   178,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,     0,   189,   190,   561,   562,     0,
       0,   563,     0,   191,     0,     0,     0,     0,     0,     0,
       0,   162,   163,   164,   165,   166,   167,   168,   169,   170,
       0,     0,   171,   172,     0,     0,   173,   174,   175,   176,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     177,   178,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,     0,   189,   190,   598,   562,     0,     0,   599,     0,
     191,     0,     0,     0,     0,     0,     0,     0,   162,   163,
     164,   165,   166,   167,   168,   169,   170,     0,     0,   171,
     172,     0,     0,   173,   174,   175,   176,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   177,   178,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,     0,   189,
     190,   612,   554,     0,     0,   613,     0,   191,     0,     0,
       0,     0,     0,     0,     0,   162,   163,   164,   165,   166,
     167,   168,   169,   170,     0,     0,   171,   172,     0,     0,
     173,   174,   175,   176,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   177,   178,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,     0,   189,   190,   615,   562,
       0,     0,   616,     0,   191,     0,     0,     0,     0,     0,
       0,     0,   162,   163,   164,   165,   166,   167,   168,   169,
     170,     0,     0,   171,   172,     0,     0,   173,   174,   175,
     176,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   177,   178,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,     0,   189,   190,   639,   554,     0,     0,   640,
       0,   191,     0,     0,     0,     0,     0,     0,     0,   162,
     163,   164,   165,   166,   167,   168,   169,   170,     0,     0,
     171,   172,     0,     0,   173,   174,   175,   176,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   177,   178,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,     0,
     189,   190,   642,   562,     0,     0,   643,     0,   191,     0,
       0,     0,     0,     0,     0,     0,   162,   163,   164,   165,
     166,   167,   168,   169,   170,     0,     0,   171,   172,     0,
       0,   173,   174,   175,   176,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   177,   178,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   179,   180,   181,   182,
     183,   184,   185,   186,   187,   188,     0,   189,   190,   727,
     554,     0,     0,   728,     0,   191,     0,     0,     0,     0,
       0,     0,     0,   162,   163,   164,   165,   166,   167,   168,
     169,   170,     0,     0,   171,   172,     0,     0,   173,   174,
     175,   176,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   177,   178,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,     0,   189,   190,   730,   562,     0,     0,
     731,     0,   191,     0,     0,     0,     0,     0,     0,     0,
     162,   163,   164,   165,   166,   167,   168,   169,   170,     0,
       0,   171,   172,     0,     0,   173,   174,   175,   176,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   177,
     178,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     179,   180,   181,   182,   183,   184,   185,   186,   187,   188,
       0,   189,   190,   737,   554,     0,     0,   738,     0,   191,
       0,     0,     0,     0,     0,     0,     0,   162,   163,   164,
     165,   166,   167,   168,   169,   170,     0,     0,   171,   172,
       0,     0,   173,   174,   175,   176,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   177,   178,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,     0,   189,   190,
    1000,   554,     0,     0,  1001,     0,   191,     0,     0,     0,
       0,     0,     0,     0,   162,   163,   164,   165,   166,   167,
     168,   169,   170,     0,     0,   171,   172,     0,     0,   173,
     174,   175,   176,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   177,   178,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,     0,   189,   190,  1035,   554,     0,
       0,  1036,     0,   191,     0,     0,     0,     0,     0,     0,
       0,   162,   163,   164,   165,   166,   167,   168,   169,   170,
       0,     0,   171,   172,     0,     0,   173,   174,   175,   176,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     177,   178,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,     0,   189,   190,  1038,   562,     0,     0,  1039,     0,
     191,     0,     0,     0,     0,     0,     0,     0,   162,   163,
     164,   165,   166,   167,   168,   169,   170,     0,     0,   171,
     172,     0,     0,   173,   174,   175,   176,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   177,   178,   327,
     328,   329,   330,   331,   332,   333,   334,   335,   336,   337,
     338,   339,     0,     0,   340,   341,     0,     0,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,     0,   189,
     190,     0,     0,     0,     0,     0,     0,   191,     0,     0,
       0,     0,     0,     0,     0,   342,     0,   343,   344,   345,
     346,   347,   348,   349,   350,   351,   352,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   231
};

static const yytype_int16 yycheck[] =
{
       2,    96,    74,    55,    28,    67,    79,    50,   220,   230,
       8,   359,    27,   326,    29,     8,   326,    87,   354,   262,
      90,     8,   472,   266,   651,    16,    17,    22,    90,    20,
      28,   353,    53,   355,   255,    28,   614,     2,   259,     4,
     575,    28,    94,    86,    87,     4,   420,    90,   376,    51,
      52,     2,   594,     4,   440,   574,    67,   577,    79,    53,
     472,    90,   780,   641,   468,    16,    17,   788,    55,    20,
      91,    92,    93,   876,    16,    17,    49,   399,    20,    74,
     416,   715,    25,    64,   458,   719,    25,    13,    26,   668,
      25,    25,   412,   415,    25,   417,   250,    27,    49,    50,
     420,   874,    53,   508,    29,    76,   511,    94,    85,    51,
      25,   685,     0,    64,   518,    85,    61,   903,    16,    17,
      68,   443,    20,   776,    61,    25,    37,    38,    79,    13,
     139,   141,   141,    25,   470,    86,    87,    97,   907,    90,
      91,    92,    93,   770,   771,   130,   596,   118,    56,   471,
     803,   729,    13,    51,    52,   132,   133,    16,    17,   136,
     120,    20,   740,   133,    26,   110,   939,    37,    38,   323,
     286,   243,   288,   110,   290,   123,   124,   125,   294,    25,
     898,    52,    13,    13,   140,    56,   111,   230,   991,   232,
     252,   134,   130,   136,   736,   134,   140,   136,   271,   134,
     134,   422,   744,   134,   724,   426,   780,   657,   134,   139,
     431,   141,   214,   139,   788,   141,  1002,   429,   220,   134,
     993,   741,   685,   252,   945,   227,   447,    25,   807,   808,
     141,   452,   950,   224,   134,   226,   227,   307,   308,   309,
     310,   252,   134,  1012,   580,   657,   632,   874,   622,   876,
     271,  1024,   203,   268,    13,   139,   592,   141,     1,   581,
     670,    87,   672,   591,   307,   308,   309,   310,   130,   312,
     313,   593,    87,   224,   136,   226,   227,   250,   139,   230,
     141,   232,   224,   248,   226,   690,   237,   113,   134,  1007,
     360,   361,   696,   743,    28,   929,   517,   248,   113,   250,
     362,   621,   622,   324,    15,   136,    17,   886,   139,   139,
     141,   141,   939,   940,   134,   113,   378,   360,   361,   782,
     271,   141,   140,   786,   898,   845,   224,   847,   226,   903,
     324,   743,   375,   362,    49,   854,   134,   857,    85,   137,
     321,   306,   862,   141,    61,   326,   311,   306,    25,   140,
     323,   113,   354,   134,    61,   306,   307,   308,   309,   310,
     311,   312,   313,   354,   991,   224,   993,   226,   355,   485,
     321,   945,   323,   324,   117,   326,   492,   725,    87,   321,
     139,   134,   141,    61,   326,   964,   133,   697,   141,   931,
     107,    87,   109,   636,   929,   468,   139,  1024,   141,   611,
     107,   714,   109,   354,   113,   356,   926,   927,   113,   360,
     361,   134,   399,   437,   416,    87,   100,   113,   141,    85,
      87,   402,   403,   140,   375,   416,   428,   429,  1002,   107,
     417,   109,   110,   140,    85,   898,   113,   900,   440,   437,
     903,   113,   905,    87,   437,   518,   113,   468,   990,   134,
     437,   402,   403,    58,    26,    85,   443,   134,    56,  1037,
     137,    87,    61,    68,   141,   416,    65,   133,   470,   989,
      26,   136,    85,   883,   884,   440,   141,   113,   888,   470,
     890,   891,   133,    85,   471,   557,   936,   113,   203,   440,
      85,    96,    97,   565,    58,   869,   136,   518,   134,   450,
    1020,   113,  1022,   133,    68,   507,   508,    85,   107,   511,
     109,   573,   574,    85,   412,   120,   703,   468,    88,   470,
     133,   742,   237,   710,    61,   587,   134,   542,   600,    85,
     132,   133,    96,    97,    87,   250,   136,   132,   133,  1002,
     761,  1004,   134,    85,  1007,   560,  1009,    61,   760,   869,
     130,   685,   595,   774,   132,   133,   120,   455,   130,   136,
     132,   133,    85,   558,   637,    61,   568,   518,   136,   917,
     107,   566,   109,   110,   130,   923,   132,   133,   580,    61,
     136,   605,   625,    68,  1047,   995,   996,   997,   998,   580,
     592,   133,    68,   107,   581,   109,   110,    26,   600,   614,
     113,   592,   685,    59,    60,   600,   593,   605,   323,   611,
     133,   107,   605,   109,   110,   137,   637,   138,   605,   570,
      85,   572,   131,   696,    85,   107,   641,   109,   110,   580,
     632,    85,   704,   134,   576,  1045,    59,    60,    56,   711,
     861,   592,   110,   594,   595,   121,   122,   123,   124,   125,
      54,    87,   873,   748,   788,   110,    85,   791,    85,    63,
      64,    87,   732,    85,   666,   735,   668,   632,   133,    26,
     685,   733,   133,   110,   625,   696,   110,   113,   132,   133,
    1016,   632,     2,   745,     4,    85,   637,   113,   690,   732,
      14,    15,   735,    68,   746,  1017,    16,    17,   134,   782,
      20,   130,    94,   132,   133,   132,   133,   136,   134,   711,
     132,   133,   714,   715,   729,   717,   697,   719,    68,   700,
     701,    87,   134,   621,    17,   740,   707,   708,    85,    49,
      50,   139,   132,   133,    56,   686,    25,   134,   689,    85,
     691,   137,    85,   134,    64,   696,   697,   113,   131,   700,
     701,   130,   134,   134,   140,    85,   707,   708,   760,   746,
      52,   776,    54,    55,    56,    57,    86,    87,   134,   903,
      90,   905,   714,   130,   844,   132,   133,    85,   140,   136,
     801,   732,    85,   134,   735,   736,   132,   133,   803,   132,
     133,    85,   854,   744,   856,    52,   134,    54,    55,    56,
      57,   844,   132,   133,   113,   807,   808,    87,   134,    87,
      10,   945,     8,   947,    13,   898,   113,   900,   952,    87,
     903,   136,   905,   134,   132,   133,   777,   778,   134,   132,
     133,   131,    52,   113,    87,   113,   134,   839,   132,   133,
     842,   115,   134,    54,    55,   113,    57,   810,   811,   830,
     801,   134,    63,    64,   134,   570,   134,   572,    52,    52,
     113,    54,    55,    56,    57,   816,   134,   134,  1002,    52,
    1004,   134,   893,   138,    15,  1009,   134,   134,   134,   830,
     131,   134,   113,   203,   886,    40,    41,    42,    43,    44,
     841,   118,  1026,   844,   134,   916,    89,   848,   849,   134,
     134,   852,    95,   139,   224,   134,   226,   227,    10,   131,
     230,    10,   232,  1047,    88,   134,     9,   237,   134,  1002,
     137,  1004,    10,   131,  1007,   134,  1009,   929,   248,    52,
     250,    54,    55,    56,    57,    52,   134,    54,    55,    56,
      57,   892,   893,   134,    52,   134,    54,    55,    56,    57,
      56,   118,   131,   134,   134,   970,   108,   134,   134,   134,
      56,    52,   964,   134,  1047,   916,    89,   136,   134,   920,
     134,   686,    95,   136,   689,   134,   691,   450,    91,    93,
     931,    89,   711,  1019,   770,   685,   306,   307,   308,   309,
     310,   311,   312,   313,  1012,   749,  1018,    57,   961,   962,
      94,   321,   965,   323,   967,   968,   326,    99,   294,   936,
     782,   900,   898,   780,  1016,    -1,  1018,  1019,   396,    -1,
      -1,    -1,  1037,    16,    17,  1016,    -1,    20,    -1,   980,
    1017,   982,    -1,    -1,   354,    -1,   356,   988,    -1,   990,
     360,   361,    -1,    -1,    52,    -1,    54,    55,    56,    57,
      58,    -1,    -1,    46,    47,   375,    -1,    -1,    51,    52,
      68,    -1,   777,   778,    -1,  1016,    -1,  1030,  1031,  1032,
    1033,    64,    65,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    89,   402,   403,    -1,    -1,    -1,    -1,    96,    97,
    1053,    -1,    -1,    -1,    -1,    -1,   416,    -1,    -1,    -1,
      -1,   816,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     201,    -1,   120,   204,   205,   206,    -1,    -1,    -1,    -1,
     440,    -1,    -1,    -1,    -1,    -1,   841,    -1,    -1,    -1,
     450,    -1,    -1,   848,   849,    -1,    -1,   852,    52,    -1,
      54,    55,    56,    57,    58,    -1,    -1,     0,    -1,    -1,
     470,    -1,    -1,    -1,    68,     8,     9,    10,    -1,    -1,
      13,    14,    15,    68,    17,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    26,    27,    89,    -1,   892,    83,    84,
      -1,    95,    96,    97,    37,    38,    68,    40,    41,    42,
      43,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    83,    84,    -1,    -1,   920,   120,    -1,   201,   123,
      -1,   204,   205,   206,   207,   120,   121,   122,   123,   124,
     125,    -1,    -1,    -1,    -1,    -1,    -1,   141,    -1,    -1,
     321,   224,    85,   226,   227,   326,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,    -1,    68,    -1,    -1,    -1,
     570,    -1,   572,    -1,    -1,   108,    -1,    -1,    -1,    -1,
     580,    83,    84,    -1,    -1,   980,    -1,   982,    -1,    -1,
      -1,    -1,   592,   988,   594,   595,    -1,   130,   131,    -1,
     133,    -1,    -1,   136,   137,    -1,   139,    -1,   141,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   121,
     122,   123,   124,   125,    -1,   625,    -1,    -1,    -1,    -1,
      -1,    -1,   632,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     411,   412,    -1,    -1,    -1,    -1,    -1,    -1,   321,   420,
      -1,    -1,    -1,   326,   327,   328,   329,   330,   331,   332,
     333,   334,   335,   336,   337,   338,   339,   340,   341,   342,
     343,   344,   345,   346,   347,   348,   349,   350,   351,   352,
      -1,   354,    -1,    -1,   455,    -1,   686,   458,    -1,   689,
      -1,   691,    -1,    -1,    -1,    -1,    -1,   697,    -1,    -1,
     700,   701,    -1,    -1,    -1,    -1,    -1,   707,   708,    -1,
       2,    -1,     4,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   402,
     403,    -1,   732,    -1,    -1,   735,   736,   410,   411,   412,
      -1,   512,    -1,   416,   744,   418,   419,   420,    -1,    52,
      -1,    54,    55,    56,    57,    58,    -1,    49,    -1,    -1,
      -1,    53,    -1,    -1,    -1,    68,   439,    -1,    -1,    -1,
      -1,   444,    -1,    -1,    -1,    -1,    -1,   777,   778,    -1,
      -1,    -1,   455,    -1,    -1,   458,    89,    79,    -1,    -1,
      -1,    -1,    95,    96,    97,    -1,    -1,   470,    -1,    91,
      92,    93,    94,    -1,    -1,   576,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   816,   120,    -1,    -1,
     123,    -1,    -1,   496,   497,    -1,    -1,    -1,    -1,    -1,
     830,    -1,    -1,   136,    -1,    -1,    -1,   608,    -1,   512,
      -1,   841,    -1,    -1,   844,    -1,    -1,    -1,   848,   849,
     621,   622,   852,    52,    -1,    54,    55,    56,    57,    58,
      -1,     2,    -1,     4,    -1,    -1,    -1,    -1,    -1,    68,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   649,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      89,    -1,   892,    -1,    -1,    -1,    95,    96,    97,    -1,
      -1,    -1,    -1,   576,    -1,    -1,    -1,   580,    49,    -1,
      -1,   203,    53,    -1,    -1,    -1,    -1,    -1,    -1,   592,
     920,   120,   693,    52,   123,    54,    55,    56,    57,    58,
      -1,   931,    -1,    -1,    -1,   608,    -1,   136,    79,    68,
      -1,    -1,    -1,   714,    -1,   237,    -1,    -1,   621,   622,
      91,    92,    93,    94,    -1,    -1,   248,    -1,   250,    -1,
      89,    -1,    -1,    -1,    -1,    -1,    95,    96,    97,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   649,    -1,    -1,   271,
     980,    -1,   982,    -1,    -1,    -1,    -1,    -1,   988,    -1,
     990,   120,    -1,    -1,   123,    -1,   767,    -1,    -1,    52,
      -1,    54,    55,    56,    57,    58,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   306,    68,  1016,    -1,    -1,   311,
     693,    -1,    -1,    -1,   697,   698,    -1,   700,   701,    -1,
      -1,   323,   324,    -1,   707,   708,    89,    -1,    -1,    -1,
      -1,   714,    95,    96,    97,    -1,     2,   818,     4,    -1,
      -1,    -1,    -1,    -1,    -1,     2,    -1,     4,    -1,    -1,
      -1,    -1,   203,    -1,   356,    -1,    -1,   120,    -1,    -1,
     123,    52,    -1,    54,    55,    56,    57,    58,   751,    -1,
     851,    -1,   755,   756,    -1,   758,   759,    68,    -1,    -1,
      -1,    -1,    -1,    49,   767,    -1,   237,    53,   869,    -1,
      -1,    -1,    49,    -1,    -1,    -1,    -1,   248,    89,   250,
      -1,    -1,    -1,    -1,    -1,    96,    97,    -1,    -1,    -1,
      -1,    -1,    -1,    79,    -1,    -1,    -1,    -1,    -1,    -1,
     271,    -1,    -1,    -1,    -1,    91,    92,    93,    -1,   120,
      -1,    -1,    -1,    -1,    91,   818,    -1,    -1,   440,   822,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   830,   450,    -1,
      -1,    -1,    -1,    -1,    -1,   306,    -1,    -1,    -1,    -1,
     311,    -1,    -1,    -1,    -1,    -1,   468,    -1,   851,    -1,
      -1,    -1,   323,   324,    68,    69,    70,    71,    72,    73,
      74,    75,    -1,    77,    78,   868,   869,    -1,    -1,    83,
      84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   356,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     2,    -1,     4,   518,    -1,    -1,    -1,
      -1,    -1,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,    -1,    -1,    -1,    -1,    -1,   203,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   203,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      49,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   570,    -1,
     572,   237,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     237,    -1,   248,    -1,   250,    -1,    -1,    -1,    -1,   440,
      -1,   248,   594,   250,    -1,    -1,    -1,    -1,    -1,   450,
      -1,    -1,    -1,    -1,    -1,   271,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   468,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     632,    -1,    -1,  1016,    -1,   637,    -1,    -1,    -1,    -1,
     306,    -1,    -1,    -1,    -1,   311,    -1,    -1,    -1,   306,
      -1,    -1,    -1,    -1,   311,    -1,    -1,   323,   324,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   323,   518,    -1,   326,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,    -1,    -1,
      -1,    -1,    -1,    -1,   686,    -1,    -1,   689,    -1,   691,
     356,    -1,    -1,    -1,   696,    -1,    -1,    -1,    -1,   356,
      -1,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,   203,    -1,    83,    84,    -1,   570,
      -1,   572,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   736,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   744,   594,    -1,    -1,    -1,   114,   237,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   248,
      -1,   250,    -1,    -1,    -1,    -1,    -1,   134,    -1,    -1,
      -1,    -1,    -1,    -1,   440,   777,   778,    -1,    -1,    -1,
      -1,   632,    -1,   440,   450,    -1,   637,    -1,    -1,    -1,
      -1,    -1,    -1,   450,    -1,    -1,    -1,    -1,    -1,   801,
      -1,    -1,   468,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   816,    -1,    -1,   306,    -1,    -1,
      -1,    -1,   311,    -1,    -1,    -1,    -1,    68,    69,    70,
      71,    72,    73,    74,   323,   686,    77,    78,   689,   841,
     691,    -1,    83,    84,    -1,   696,   848,   849,    -1,    -1,
     852,    -1,   518,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   356,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   736,    -1,    -1,    -1,    -1,
     892,   893,    -1,   744,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   570,    -1,   572,    -1,    -1,    -1,
      -1,    -1,    -1,   570,   916,   572,    -1,    -1,   920,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   777,   778,   594,   931,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   594,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     801,   440,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   450,    -1,    -1,    -1,   816,   632,    -1,    -1,    -1,
      -1,   637,    -1,    -1,    -1,   632,    -1,    -1,   980,    -1,
     982,    -1,    -1,    -1,    -1,    -1,   988,    -1,   990,    -1,
     841,    -1,    -1,    -1,    -1,    -1,    -1,   848,   849,    -1,
      -1,   852,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     676,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    44,
     686,    -1,    -1,   689,    -1,   691,    -1,    -1,    -1,   686,
     696,    -1,   689,    -1,   691,    -1,    -1,    -1,    -1,    -1,
     697,   892,   893,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    -1,    -1,    83,    84,
      -1,    -1,    -1,    -1,    -1,   916,    -1,    -1,    -1,   920,
     736,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   744,   736,
     931,   570,    -1,   572,    -1,    -1,    -1,   744,    -1,   114,
      -1,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,    -1,    -1,    -1,    -1,   594,    -1,    -1,    -1,    -1,
      -1,   777,   778,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     777,   778,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   980,
      -1,   982,    -1,    -1,    -1,   801,    -1,   988,    44,   990,
      -1,    -1,    -1,   632,    -1,    -1,    -1,    -1,    -1,    -1,
     816,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   816,
      -1,    -1,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,   841,    -1,    83,    84,    -1,
      -1,    -1,   848,   849,   841,    -1,   852,    -1,    -1,    -1,
      -1,   848,   849,    -1,    -1,   852,    -1,   686,    -1,    -1,
     689,    -1,   691,    -1,    -1,    -1,    -1,    -1,   114,    -1,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
      -1,    -1,    -1,    -1,    -1,    -1,   892,   893,    68,    69,
      70,    71,    72,    73,    74,   892,    -1,    77,    78,    -1,
      -1,    -1,    -1,    83,    84,    -1,    -1,   736,    -1,    -1,
     916,    -1,    -1,    -1,   920,   744,    -1,    -1,    -1,   916,
      -1,    -1,    -1,   920,    -1,   931,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   931,    -1,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,    -1,    -1,   777,   778,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    -1,    -1,    83,    84,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   980,    -1,   982,    -1,    -1,    -1,
      -1,    -1,   988,   980,   990,   982,    -1,   816,    -1,    -1,
      -1,   988,    -1,   990,    -1,    -1,   114,    -1,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,    -1,    -1,
      -1,    -1,   841,    -1,    -1,    -1,    -1,    -1,    -1,   848,
     849,     0,     1,   852,     3,     4,     5,     6,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,   892,    -1,    -1,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    64,    -1,    66,    67,    -1,
      -1,   920,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   931,    -1,    -1,    -1,    -1,    86,    -1,    -1,
      89,    90,    -1,    92,    93,    -1,    95,    -1,    -1,    98,
      99,   100,   101,   102,   103,   104,   105,   106,     0,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     8,     9,    10,    -1,
      -1,    13,    14,    15,    -1,    17,    -1,   126,   127,   128,
      -1,   980,    -1,   982,    26,    27,    28,    29,    -1,   988,
     139,   990,   141,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    43,    44,    -1,    -1,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    -1,    -1,
      83,    84,    -1,    -1,    -1,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    -1,
      -1,    83,    84,    85,    -1,    87,    88,    -1,    -1,    -1,
      -1,    -1,    94,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,    -1,    -1,    -1,   108,    -1,    -1,   111,
      -1,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,    -1,    -1,    -1,   130,   131,
     132,   133,   134,     0,    -1,   137,   138,   139,    -1,   141,
      -1,     8,     9,    10,    -1,    -1,    13,    14,    15,    -1,
      17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,    -1,
      27,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    -1,    40,    41,    42,    43,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    -1,    -1,    83,    84,    85,    -1,
      87,    88,    -1,    -1,    -1,    -1,    -1,    94,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   108,    -1,    -1,   111,    -1,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,    -1,
      -1,    -1,    -1,    -1,   131,   132,   133,   134,     0,    -1,
     137,   138,   139,    -1,   141,    -1,     8,     9,    10,    -1,
      -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    25,    -1,    27,    28,    29,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    -1,
      -1,    83,    84,    85,    -1,    87,    88,    -1,    -1,    -1,
      -1,    -1,    94,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   108,    -1,    -1,   111,
      -1,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,    -1,    -1,    -1,    -1,   131,
     132,   133,   134,     0,    -1,   137,   138,   139,    -1,   141,
      -1,     8,     9,    10,    -1,    -1,    13,    14,    15,    -1,
      17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    26,
      27,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    -1,    40,    41,    42,    43,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    -1,    -1,    83,    84,    85,    -1,
      -1,    88,    -1,    -1,    -1,    -1,    -1,    94,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   108,    -1,    -1,   111,    -1,    -1,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,    -1,
      -1,    -1,    -1,   130,   131,   132,   133,   134,     0,    -1,
     137,   138,   139,    -1,   141,    -1,     8,     9,    10,    -1,
      -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    26,    27,    28,    29,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    -1,
      -1,    83,    84,    85,    -1,    -1,    88,    -1,    -1,    -1,
      -1,    -1,    94,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   108,    -1,    -1,   111,
      -1,    -1,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,    -1,    -1,    -1,   130,   131,
     132,   133,   134,     0,    -1,   137,   138,   139,    -1,   141,
      -1,     8,     9,    10,    -1,    -1,    13,    14,    15,    -1,
      17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      27,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    -1,    40,    41,    42,    43,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    -1,    -1,    83,    84,    85,    -1,
      87,    88,    -1,    -1,    -1,    -1,    -1,    94,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   108,    -1,    -1,   111,    -1,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,    -1,
      -1,    -1,    -1,    -1,   131,   132,   133,   134,     0,    -1,
     137,   138,   139,    -1,   141,    -1,     8,     9,    10,    -1,
      -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    26,    27,    28,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    -1,
      -1,    83,    84,    85,    -1,    -1,    88,    -1,    -1,    -1,
      -1,    -1,    94,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   108,    -1,    -1,    -1,
      -1,    -1,   114,    -1,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,    -1,    -1,    -1,   130,   131,
     132,   133,   134,     0,   136,   137,   138,   139,    -1,   141,
      -1,     8,     9,    10,    -1,    -1,    13,    14,    15,    -1,
      17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      27,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    -1,    40,    41,    42,    43,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    -1,    -1,    83,    84,    85,    -1,
      -1,    88,    -1,    -1,    -1,    -1,    -1,    94,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   108,    -1,    -1,   111,    -1,    -1,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,    -1,
      -1,    -1,    -1,    -1,   131,   132,   133,   134,     0,    -1,
     137,   138,   139,    -1,   141,    -1,     8,     9,    10,    -1,
      -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    26,    27,    28,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    -1,
      -1,    83,    84,    85,    -1,    -1,    88,    -1,    -1,    -1,
      -1,    -1,    94,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   108,    -1,    -1,    -1,
      -1,    -1,   114,    -1,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,    -1,    -1,    -1,   130,   131,
     132,   133,   134,     0,   136,   137,   138,   139,    -1,   141,
      -1,     8,     9,    10,    -1,    -1,    13,    14,    15,    -1,
      17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      27,    28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    -1,    40,    41,    42,    43,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    -1,    -1,    83,    84,    85,    -1,
      -1,    88,    -1,    -1,    -1,    -1,    -1,    94,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   108,    -1,    -1,    -1,    -1,    -1,   114,    -1,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,    -1,
      -1,    -1,    -1,    -1,   131,   132,   133,   134,     0,   136,
     137,   138,   139,    -1,   141,    -1,     8,     9,    10,    -1,
      -1,    -1,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    26,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,    41,
      42,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    -1,
      -1,    83,    84,    85,    -1,    87,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   108,    -1,    -1,    -1,
      -1,   113,   114,    -1,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,    -1,    -1,    -1,   130,   131,
     132,   133,   134,     0,    -1,   137,    -1,   139,    -1,   141,
      -1,     8,     9,    10,    -1,    -1,    -1,    14,    15,    -1,
      17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    -1,    40,    41,    42,    43,    44,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    -1,    -1,    83,    84,    85,    -1,
      87,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   108,    -1,    -1,    -1,    -1,   113,   114,    -1,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,    -1,
      -1,    -1,    -1,    -1,   131,   132,   133,   134,    -1,    -1,
     137,    -1,   139,     1,   141,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    -1,    -1,    15,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    64,    -1,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,
      -1,    89,    90,    -1,    92,    93,    -1,    95,    -1,    -1,
      98,    99,   100,   101,   102,   103,   104,   105,   106,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   126,   127,
     128,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   139,     1,   141,     3,     4,     5,     6,     7,    -1,
      -1,    10,    11,    12,    -1,    14,    15,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    64,    -1,    66,    67,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,
      89,    90,    -1,    92,    93,    -1,    95,    -1,    -1,    98,
      99,   100,   101,   102,   103,   104,   105,   106,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   126,   127,   128,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     139,     1,   141,     3,     4,     5,     6,     7,    -1,    -1,
      10,    11,    12,    -1,    -1,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    64,    -1,    66,    67,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,    89,
      90,    -1,    92,    93,    -1,    95,    -1,    -1,    98,    99,
     100,   101,   102,   103,   104,   105,   106,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   126,   127,   128,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   139,
       1,   141,     3,     4,     5,     6,     7,    -1,    -1,    10,
      11,    12,    -1,    -1,    15,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    62,    63,    64,    -1,    66,    67,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,
      -1,    92,    93,    -1,    95,    -1,    -1,    98,    99,   100,
     101,   102,   103,   104,   105,   106,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     1,    -1,     3,     4,     5,     6,     7,
      -1,     9,    10,    11,    12,   126,   127,   128,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,   139,    -1,
     141,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    64,    -1,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,
      -1,    89,    90,    -1,    92,    93,    -1,    95,    -1,    -1,
      98,    99,   100,   101,   102,   103,   104,   105,   106,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    -1,    11,    12,   126,   127,
     128,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,   139,    -1,   141,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    62,    63,    64,
      -1,    66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    86,    -1,    -1,    89,    90,    -1,    92,    93,    -1,
      95,    -1,    -1,    98,    99,   100,   101,   102,   103,   104,
     105,   106,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   126,   127,   128,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   137,    -1,   139,     1,   141,     3,     4,     5,
       6,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    64,    -1,
      66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      86,    -1,    -1,    89,    90,    -1,    92,    93,    -1,    95,
      -1,    -1,    98,    99,   100,   101,   102,   103,   104,   105,
     106,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     126,   127,   128,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   137,    -1,   139,     1,   141,     3,     4,     5,     6,
       7,    -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    62,    63,    64,    -1,    66,
      67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,
      -1,    -1,    89,    90,    -1,    92,    93,    -1,    95,    -1,
      -1,    98,    99,   100,   101,   102,   103,   104,   105,   106,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   126,
     127,   128,    -1,    -1,   131,     1,    -1,     3,     4,     5,
       6,     7,   139,    -1,   141,    11,    12,    -1,    -1,    -1,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    64,    -1,
      66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      86,    -1,    -1,    89,    90,    -1,    92,    93,    -1,    95,
      -1,    -1,    98,    99,   100,   101,   102,   103,   104,   105,
     106,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     126,   127,   128,    -1,    -1,   131,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   139,     1,   141,     3,     4,     5,     6,
       7,    -1,    -1,    10,    11,    12,    -1,    -1,    -1,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    62,    63,    64,    -1,    66,
      67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,
      -1,    -1,    89,    90,    -1,    92,    93,    -1,    95,    -1,
      -1,    98,    99,   100,   101,   102,   103,   104,   105,   106,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     1,    -1,     3,
       4,     5,     6,     7,    -1,    -1,    -1,    11,    12,   126,
     127,   128,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,   139,    -1,   141,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    -1,    62,    63,
      64,    -1,    66,    67,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    86,    -1,    -1,    89,    90,    -1,    92,    93,
      -1,    95,    -1,    -1,    98,    99,   100,   101,   102,   103,
     104,   105,   106,    -1,   108,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,   126,   127,   128,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,   139,    -1,   141,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      -1,    62,    63,    64,    -1,    66,    67,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,
      -1,    92,    93,    -1,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,   126,   127,   128,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
     141,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,
      -1,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    64,    -1,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,
      -1,    89,    90,    -1,    92,    93,    -1,    -1,    -1,    -1,
      98,    99,   100,   101,   102,   103,   104,   105,   106,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,   126,   127,
     128,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,   139,    -1,   141,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    46,    -1,    -1,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    62,    63,    64,
      -1,    66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    86,    -1,    -1,    89,    90,    -1,    92,    93,    -1,
      -1,    -1,    -1,    98,    99,   100,   101,   102,   103,   104,
     105,   106,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,     6,     7,    -1,    -1,    -1,    11,
      12,   126,   127,   128,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,   141,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    -1,
      62,    63,    64,    -1,    66,    67,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,
      92,    93,    -1,    95,    -1,    -1,    98,    99,   100,   101,
     102,   103,   104,   105,   106,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,    -1,
      -1,    -1,    11,    12,   126,   127,   128,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,   139,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    64,    -1,    66,    67,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,
      89,    90,    -1,    92,    93,    -1,    95,    -1,    -1,    98,
      99,   100,   101,   102,   103,   104,   105,   106,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   126,   127,   128,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     139,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    -1,    -1,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    -1,    -1,    77,    78,    -1,    -1,    81,
      82,    83,    84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    96,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,   127,   128,    -1,    -1,    -1,
      -1,    -1,    -1,   135,   136,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    -1,    -1,    77,
      78,    -1,    -1,    81,    82,    83,    84,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,    96,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,    -1,   127,
     128,    -1,    -1,    -1,    -1,    -1,    -1,   135,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      -1,    56,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      -1,    -1,    77,    78,    -1,    -1,    81,    82,    83,    84,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      95,    96,    -1,    -1,    99,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,    -1,   127,   128,    -1,    -1,    -1,    -1,    -1,    -1,
     135,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    -1,    -1,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    -1,    -1,    77,    78,    -1,    -1,    81,
      82,    83,    84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    96,    -1,    -1,    99,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,   127,   128,    -1,    -1,    -1,
      -1,    -1,    -1,   135,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    -1,    -1,    56,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    -1,    -1,    77,    78,
      -1,    -1,    81,    82,    83,    84,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    95,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,    -1,   127,   128,
       3,     4,     5,    -1,     7,    -1,   135,    -1,    11,    12,
      -1,    -1,    -1,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,    92,
      93,    -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,
     103,   104,   105,   106,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,    -1,
      -1,    -1,    16,   126,    18,    19,    20,    21,    22,    23,
      24,   134,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    -1,    59,    60,    -1,    62,    63,
      64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    86,    -1,    -1,    89,    90,    -1,    92,    93,
      -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,   103,
     104,   105,   106,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,    -1,    -1,    -1,    11,    12,    -1,    -1,
      -1,    16,   126,    18,    19,    20,    21,    22,    23,    24,
     134,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    62,    63,    64,
      -1,    66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    86,    -1,    -1,    89,    90,    -1,    92,    93,    -1,
      95,    -1,    -1,    98,    99,   100,   101,   102,   103,   104,
     105,   106,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,   126,   127,   128,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    -1,
      62,    63,    64,    -1,    66,    67,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,
      92,    93,    -1,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,    -1,
      -1,    -1,    11,    12,   126,   127,   128,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    45,    46,    -1,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    64,    -1,    66,    67,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,
      89,    90,    -1,    92,    93,    -1,    95,    -1,    -1,    98,
      99,   100,   101,   102,   103,   104,   105,   106,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,   126,   127,   128,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    -1,    -1,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    -1,    62,    63,    64,    -1,
      66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      86,    -1,    -1,    89,    90,    -1,    92,    93,    -1,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
     126,   127,   128,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    -1,    62,
      63,    64,    -1,    66,    67,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,    92,
      93,    -1,    95,    96,    -1,    98,    99,   100,   101,   102,
     103,   104,   105,   106,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,   126,   127,   128,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    -1,    62,    63,    64,    -1,    66,    67,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,    89,
      90,    -1,    92,    93,    -1,    -1,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,   126,   127,   128,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      -1,    -1,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    -1,    62,    63,    64,    -1,    66,
      67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,
      -1,    -1,    89,    90,    -1,    92,    93,    -1,    95,    96,
      -1,    98,    99,   100,   101,   102,   103,   104,   105,   106,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,
       4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,   126,
     127,   128,    16,    -1,    18,    19,    20,    21,    22,    23,
      24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,
      34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    -1,    62,    63,
      64,    -1,    66,    67,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    86,    -1,    -1,    89,    90,    -1,    92,    93,
      -1,    -1,    96,    -1,    98,    99,   100,   101,   102,   103,
     104,   105,   106,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,   126,   127,   128,    16,    -1,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    62,    63,    64,    -1,    66,    67,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,
      -1,    92,    93,    -1,    95,    -1,    -1,    98,    99,   100,
     101,   102,   103,   104,   105,   106,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,   126,   127,   128,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,
      -1,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    64,    -1,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,
      -1,    89,    90,    -1,    92,    93,    -1,    95,    -1,    -1,
      98,    99,   100,   101,   102,   103,   104,   105,   106,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,
       5,    -1,     7,    -1,    -1,    -1,    11,    12,   126,   127,
     128,    16,    -1,    18,    19,    20,    21,    22,    23,    24,
      -1,    -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,
      35,    36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    46,    -1,    -1,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    -1,    59,    60,    -1,    62,    63,    64,
      -1,    66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    86,    -1,    -1,    89,    90,    -1,    92,    93,    -1,
      95,    -1,    -1,    98,    99,   100,   101,   102,   103,   104,
     105,   106,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,   126,   127,   128,    16,    -1,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    -1,
      62,    63,    64,    -1,    66,    67,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,
      92,    93,    -1,    95,    -1,    -1,    98,    99,   100,   101,
     102,   103,   104,   105,   106,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,   126,   127,   128,    16,    -1,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    64,    -1,    66,    67,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,
      89,    90,    -1,    92,    93,    -1,    95,    -1,    -1,    98,
      99,   100,   101,   102,   103,   104,   105,   106,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,
      -1,     7,    -1,    -1,    -1,    11,    12,   126,   127,   128,
      16,    -1,    18,    19,    20,    21,    22,    23,    24,    -1,
      -1,    -1,    -1,    -1,    30,    31,    32,    33,    34,    35,
      36,    -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      46,    -1,    -1,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    -1,    59,    60,    -1,    62,    63,    64,    -1,
      66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      86,    -1,    -1,    89,    90,    -1,    92,    93,    -1,    -1,
      -1,    -1,    98,    99,   100,   101,   102,   103,   104,   105,
     106,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
     126,   127,   128,    16,    -1,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    64,    -1,    66,    67,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,    92,
      93,    -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,
     103,   104,   105,   106,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,   126,   127,   128,    16,    -1,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    64,    -1,    66,    67,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,    89,
      90,    -1,    92,    93,    -1,    -1,    -1,    -1,    98,    99,
     100,   101,   102,   103,   104,   105,   106,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,    -1,    -1,    11,    12,   126,   127,   128,    16,
      -1,    18,    19,    20,    21,    22,    23,    24,    -1,    -1,
      -1,    -1,    -1,    30,    31,    32,    33,    34,    35,    36,
      -1,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    46,
      -1,    -1,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    -1,    59,    60,    -1,    62,    63,    64,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,
      -1,    -1,    89,    90,    -1,    92,    93,    -1,    95,    -1,
      -1,    98,    99,   100,   101,   102,   103,   104,   105,   106,
      -1,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,   126,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,
      -1,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    64,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,
      -1,    89,    90,    -1,    92,    93,    -1,    95,    -1,    -1,
      98,    99,   100,   101,   102,   103,   104,   105,   106,    -1,
      -1,    -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,
      -1,    -1,    11,    12,    -1,    -1,    -1,    16,   126,    18,
      19,    20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,
      -1,    30,    31,    32,    33,    34,    35,    36,    -1,    -1,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    -1,
      59,    60,    -1,    62,    63,    64,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    83,    -1,    -1,    86,    -1,    -1,
      89,    90,    -1,    92,    93,    -1,    -1,    -1,    -1,    98,
      99,   100,   101,   102,   103,   104,   105,   106,    -1,    -1,
      -1,    -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,
      -1,    11,    12,    -1,    -1,    -1,    16,   126,    18,    19,
      20,    21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,
      30,    31,    32,    33,    34,    35,    36,    -1,    -1,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    -1,    59,
      60,    -1,    62,    63,    64,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,    89,
      90,    -1,    92,    93,    -1,    -1,    -1,    -1,    98,    99,
     100,   101,   102,   103,   104,   105,   106,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,
      11,    12,    -1,    -1,    -1,    16,   126,    18,    19,    20,
      21,    22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,
      31,    32,    33,    34,    35,    36,    -1,    -1,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    -1,    59,    60,
      -1,    62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,
      -1,    92,    93,    -1,    -1,    -1,    -1,    98,    99,   100,
     101,   102,   103,   104,   105,   106,    -1,    -1,    -1,    -1,
      -1,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,
      12,    -1,    -1,    -1,    16,   126,    18,    19,    20,    21,
      22,    23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,
      32,    33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    -1,    59,    60,    -1,
      62,    63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,
      92,    93,    -1,    -1,    -1,    -1,    98,    99,   100,   101,
     102,   103,   104,   105,   106,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    11,    12,
      -1,    -1,    -1,    16,   126,    18,    19,    20,    21,    22,
      23,    24,    -1,    -1,    -1,    -1,    -1,    30,    31,    32,
      33,    34,    35,    36,    -1,    -1,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    46,    -1,    -1,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    -1,    59,    60,    -1,    62,
      63,    64,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    86,    -1,    -1,    89,    90,    -1,    92,
      93,    -1,    -1,    -1,    -1,    98,    99,   100,   101,   102,
     103,   104,   105,   106,    -1,    -1,    -1,    -1,    -1,    -1,
      52,    53,    -1,    -1,    56,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   126,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    -1,    -1,    77,    78,    -1,    -1,    81,
      82,    83,    84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    96,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,   127,   128,    52,    53,    -1,
      -1,    56,    -1,   135,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      -1,    -1,    77,    78,    -1,    -1,    81,    82,    83,    84,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      95,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,    -1,   127,   128,    52,    53,    -1,    -1,    56,    -1,
     135,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    -1,    -1,    77,
      78,    -1,    -1,    81,    82,    83,    84,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,    96,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,    -1,   127,
     128,    52,    53,    -1,    -1,    56,    -1,   135,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    -1,    -1,    77,    78,    -1,    -1,
      81,    82,    83,    84,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    95,    96,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,    -1,   127,   128,    52,    53,
      -1,    -1,    56,    -1,   135,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    -1,    -1,    77,    78,    -1,    -1,    81,    82,    83,
      84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    95,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,    -1,   127,   128,    52,    53,    -1,    -1,    56,
      -1,   135,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    -1,    -1,
      77,    78,    -1,    -1,    81,    82,    83,    84,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,    96,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,    -1,
     127,   128,    52,    53,    -1,    -1,    56,    -1,   135,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    -1,    -1,    77,    78,    -1,
      -1,    81,    82,    83,    84,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    95,    96,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,    -1,   127,   128,    52,
      53,    -1,    -1,    56,    -1,   135,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    -1,    -1,    77,    78,    -1,    -1,    81,    82,
      83,    84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    95,    96,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,    -1,   127,   128,    52,    53,    -1,    -1,
      56,    -1,   135,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    -1,
      -1,    77,    78,    -1,    -1,    81,    82,    83,    84,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,
      96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
      -1,   127,   128,    52,    53,    -1,    -1,    56,    -1,   135,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    -1,    -1,    77,    78,
      -1,    -1,    81,    82,    83,    84,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    95,    96,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,    -1,   127,   128,
      52,    53,    -1,    -1,    56,    -1,   135,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    -1,    -1,    77,    78,    -1,    -1,    81,
      82,    83,    84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    95,    96,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,   127,   128,    52,    53,    -1,
      -1,    56,    -1,   135,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      -1,    -1,    77,    78,    -1,    -1,    81,    82,    83,    84,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      95,    96,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,    -1,   127,   128,    52,    53,    -1,    -1,    56,    -1,
     135,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    -1,    -1,    77,
      78,    -1,    -1,    81,    82,    83,    84,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,    96,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    -1,    -1,    83,    84,    -1,    -1,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,    -1,   127,
     128,    -1,    -1,    -1,    -1,    -1,    -1,   135,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   114,    -1,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   141
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,   143,   144,     0,     1,     3,     4,     5,     6,     7,
      11,    12,    16,    18,    19,    20,    21,    22,    23,    24,
      30,    31,    32,    33,    34,    35,    36,    39,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    59,    60,    62,    63,    64,    66,    67,    86,    89,
      90,    92,    93,    95,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   126,   127,   128,   145,   146,   147,   154,
     156,   157,   159,   160,   163,   164,   165,   167,   168,   169,
     171,   172,   182,   196,   214,   215,   216,   217,   218,   219,
     220,   221,   222,   223,   224,   249,   250,   264,   265,   266,
     267,   268,   269,   270,   273,   275,   276,   288,   290,   291,
     292,   293,   294,   295,   296,   327,   338,   147,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    56,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    77,    78,    81,    82,    83,    84,    95,    96,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   127,
     128,   135,   175,   176,   177,   178,   180,   181,   288,   290,
      39,    58,    86,    89,    95,    96,    97,   127,   164,   172,
     182,   184,   189,   192,   194,   214,   292,   293,   295,   296,
     325,   326,   189,   189,   136,   190,   191,   136,   186,   190,
     136,   141,   332,    54,   177,   332,   148,   130,    21,    22,
      30,    31,    32,   163,   182,   214,   182,    56,     1,    47,
      89,   150,   151,   152,   154,   166,   167,   338,   157,   198,
     185,   194,   325,   338,   184,   324,   325,   338,    46,    86,
     126,   134,   171,   196,   214,   292,   293,   296,   242,   243,
      54,    55,    57,   175,   280,   289,   279,   280,   281,   140,
     271,   140,   277,   140,   274,   140,   278,    59,    60,   159,
     182,   182,   139,   141,   331,   336,   337,    40,    41,    42,
      43,    44,    37,    38,    26,   130,   186,   190,   255,    28,
     247,   113,   134,    89,    95,   168,   113,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      83,    84,   114,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,    85,   132,   133,   197,   157,   158,   158,
     201,   203,   158,   331,   337,    86,   165,   172,   214,   230,
     292,   293,   296,    52,    56,    83,    86,   173,   174,   214,
     292,   293,   296,   174,    33,    34,    35,    36,    49,    50,
      51,    52,    56,   136,   175,   294,   322,    85,   133,   330,
     255,   267,    87,    87,   134,   184,    56,   184,   184,   184,
     113,    88,   134,   193,   338,    85,   132,   133,    87,    87,
     134,   193,   189,   332,   333,   189,   188,   189,   194,   325,
     338,   157,   333,   157,    54,    63,    64,   155,   136,   183,
     130,   150,    85,   133,    87,   154,   153,   166,   137,   331,
     337,   333,   199,   333,   138,   134,   141,   335,   134,   335,
     131,   335,   332,    56,    59,    60,   168,   170,   134,    85,
     132,   133,   244,    61,   107,   109,   110,   282,   110,   282,
     110,    65,   282,   110,   110,   272,   282,   110,    61,   110,
     110,   110,   272,   110,    61,   110,    68,    68,   139,   147,
     158,   158,   158,   158,   154,   157,   157,   257,   256,    94,
     161,   248,    95,   159,   184,   194,   195,   166,   134,   171,
     134,   156,   159,   172,   182,   184,   195,   182,   182,   182,
     182,   182,   182,   182,   182,   182,   182,   182,   182,   182,
     182,   182,   182,   182,   182,   182,   182,   182,   182,   182,
     182,   182,   182,    52,    53,    56,   180,   254,   328,   329,
     188,    52,    53,    56,   180,   253,   328,   149,   150,    13,
     226,   336,   226,   158,   158,   331,    17,   258,    56,    85,
     132,   133,    25,   157,    52,    56,   173,     1,   117,   297,
     336,    85,   132,   133,   210,   323,   211,   330,    52,    56,
     328,   159,   182,   159,   182,   179,   182,   184,    95,   184,
     192,   325,    52,    56,   188,    52,    56,   326,   333,   137,
     333,   134,   134,   333,   177,   200,   182,   145,   131,   328,
     328,   182,   130,   333,   152,   333,   325,   134,   170,    52,
      56,   188,    52,    56,    52,    54,    55,    56,    57,    58,
      68,    89,    95,    96,    97,   120,   123,   136,   245,   300,
     302,   303,   304,   305,   306,   307,   310,   311,   312,   313,
     316,   317,   318,   319,   320,   284,   283,   140,   282,   140,
     140,   140,   182,   182,    76,   118,   237,   238,   338,   237,
     162,   237,   184,   134,   333,   170,   134,   113,    44,   332,
      87,    87,   186,   190,   252,   332,   334,    87,    87,   186,
     190,   251,    10,   225,     8,   260,   338,   150,    13,   150,
      27,   227,   336,   227,   258,   194,   225,    52,    56,   188,
      52,    56,   205,   208,   336,   298,   207,    52,    56,   173,
     188,   149,   157,   136,   299,   302,   212,   186,   187,   190,
     338,    44,   177,   184,   193,    87,    87,   334,    87,    87,
     325,   157,   131,   145,   335,   168,   334,   113,   184,    52,
      89,    95,   231,   232,   233,   304,   302,    29,   111,   246,
     134,   301,   134,   321,   338,    52,   134,   321,   134,   301,
      52,   134,   301,    52,   285,    54,    55,    57,   287,   296,
      52,    58,   234,   236,   239,   306,   308,   309,   312,   314,
     315,   318,   320,   332,   150,   150,   237,   150,    95,   184,
     170,   182,   115,   159,   182,   159,   182,   161,   186,   138,
      87,   159,   182,   159,   182,   161,   187,   184,   195,   261,
     338,    15,   229,   338,    14,   228,   229,   229,   202,   204,
     225,   134,   226,   334,   158,   336,   158,   149,   334,   225,
     333,   302,   149,   336,   175,   255,   247,   182,    87,   134,
     333,   131,   184,   233,   134,   304,   134,   333,   239,   150,
     150,   300,   305,   316,   318,   307,   312,   320,   306,   313,
     318,   306,   286,   113,    86,   214,   239,   118,   134,   235,
     134,   321,   321,   134,   235,   134,   235,   139,    10,   131,
     150,    10,   184,   182,   159,   182,    88,   262,   338,   150,
       9,   263,   338,   158,   225,   225,   150,   150,   184,   150,
     227,   209,   336,   225,   333,   225,   213,   333,   232,   134,
      95,   231,   137,    10,   131,   134,   301,   134,   301,   321,
     134,   301,   134,   301,   301,   150,   214,    56,    85,   118,
     234,   315,   318,   308,   312,   306,   314,   318,   306,    52,
     240,   241,   303,   131,    86,   172,   214,   292,   293,   296,
     226,   150,   226,   225,   225,   229,   258,   259,   206,   149,
     299,   134,   232,   134,   304,   306,   318,   306,   306,   108,
      52,    56,   134,   235,   134,   235,   321,   134,   235,   134,
     235,   235,   134,   332,    56,    85,   132,   133,   150,   150,
     150,   225,   149,   232,   134,   301,   134,   301,   301,   301,
     306,   318,   306,   306,   241,    52,    56,   188,    52,    56,
     260,   228,   225,   225,   232,   306,   235,   134,   235,   235,
     235,   334,   301,   306,   235
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
      parser_yyerror (parser, YY_("syntax error: cannot back up")); \
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
		  Type, Value, parser); \
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
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, struct parser_params *parser)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, parser)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    struct parser_params *parser;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (parser);
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
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, struct parser_params *parser)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, parser)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    struct parser_params *parser;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, parser);
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
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, struct parser_params *parser)
#else
static void
yy_reduce_print (yyvsp, yyrule, parser)
    YYSTYPE *yyvsp;
    int yyrule;
    struct parser_params *parser;
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
		       		       , parser);
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, parser); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
#ifndef yydebug
int yydebug;
#endif
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
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, struct parser_params *parser)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, parser)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    struct parser_params *parser;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (parser);

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
int yyparse (struct parser_params *parser);
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
yyparse (struct parser_params *parser)
#else
int
yyparse (parser)
    struct parser_params *parser;
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
        case 2:
#line 850 "ripper.y"
    {
			lex_state = EXPR_BEG;
#if 0
			local_push(compile_for_eval || rb_parse_in_main());
#endif
			local_push(0);

		    ;}
    break;

  case 3:
#line 859 "ripper.y"
    {
#if 0
			if ((yyvsp[(2) - (2)].val) && !compile_for_eval) {
			    /* last expression should not be void */
			    if (nd_type((yyvsp[(2) - (2)].val)) != NODE_BLOCK) void_expr((yyvsp[(2) - (2)].val));
			    else {
				NODE *node = (yyvsp[(2) - (2)].val);
				while (node->nd_next) {
				    node = node->nd_next;
				}
				void_expr(node->nd_head);
			    }
			}
			ruby_eval_tree = NEW_SCOPE(0, block_append(ruby_eval_tree, (yyvsp[(2) - (2)].val)));
#endif
			(yyval.val) = (yyvsp[(2) - (2)].val);
			parser->result = dispatch1(program, (yyval.val));

			local_pop();
		    ;}
    break;

  case 4:
#line 882 "ripper.y"
    {
#if 0
			void_stmts((yyvsp[(1) - (2)].val));
			fixup_nodes(&deferred_nodes);
#endif

			(yyval.val) = (yyvsp[(1) - (2)].val);
		    ;}
    break;

  case 5:
#line 893 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch2(stmts_add, dispatch0(stmts_new),
						  dispatch0(void_stmt));

		    ;}
    break;

  case 6:
#line 902 "ripper.y"
    {
#if 0
			(yyval.val) = newline_node((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = dispatch2(stmts_add, dispatch0(stmts_new), (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 7:
#line 910 "ripper.y"
    {
#if 0
			(yyval.val) = block_append((yyvsp[(1) - (3)].val), newline_node((yyvsp[(3) - (3)].val)));
#endif
			(yyval.val) = dispatch2(stmts_add, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 8:
#line 918 "ripper.y"
    {
			(yyval.val) = remove_begin((yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 10:
#line 925 "ripper.y"
    {
#if 0
			/* local_push(0); */
#endif

		    ;}
    break;

  case 11:
#line 932 "ripper.y"
    {
#if 0
			ruby_eval_tree_begin = block_append(ruby_eval_tree_begin,
							    (yyvsp[(4) - (5)].val));
			/* NEW_PREEXE($4)); */
			/* local_pop(); */
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(BEGIN, (yyvsp[(4) - (5)].val));

		    ;}
    break;

  case 12:
#line 949 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (4)].val);
			if ((yyvsp[(2) - (4)].val)) {
			    (yyval.val) = NEW_RESCUE((yyvsp[(1) - (4)].val), (yyvsp[(2) - (4)].val), (yyvsp[(3) - (4)].val));
			}
			else if ((yyvsp[(3) - (4)].val)) {
			    rb_warn0("else without rescue is useless");
			    (yyval.val) = block_append((yyval.val), (yyvsp[(3) - (4)].val));
			}
			if ((yyvsp[(4) - (4)].val)) {
			    if ((yyval.val)) {
				(yyval.val) = NEW_ENSURE((yyval.val), (yyvsp[(4) - (4)].val));
			    }
			    else {
				(yyval.val) = block_append((yyvsp[(4) - (4)].val), NEW_NIL());
			    }
			}
			fixpos((yyval.val), (yyvsp[(1) - (4)].val));
#endif
			(yyval.val) = dispatch4(bodystmt,
				       escape_Qundef((yyvsp[(1) - (4)].val)),
				       escape_Qundef((yyvsp[(2) - (4)].val)),
				       escape_Qundef((yyvsp[(3) - (4)].val)),
				       escape_Qundef((yyvsp[(4) - (4)].val)));

		    ;}
    break;

  case 13:
#line 979 "ripper.y"
    {
#if 0
			void_stmts((yyvsp[(1) - (2)].val));
			fixup_nodes(&deferred_nodes);
#endif

			(yyval.val) = (yyvsp[(1) - (2)].val);
		    ;}
    break;

  case 14:
#line 990 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch2(stmts_add, dispatch0(stmts_new),
						  dispatch0(void_stmt));

		    ;}
    break;

  case 15:
#line 999 "ripper.y"
    {
#if 0
			(yyval.val) = newline_node((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = dispatch2(stmts_add, dispatch0(stmts_new), (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 16:
#line 1007 "ripper.y"
    {
#if 0
			(yyval.val) = block_append((yyvsp[(1) - (3)].val), newline_node((yyvsp[(3) - (3)].val)));
#endif
			(yyval.val) = dispatch2(stmts_add, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 17:
#line 1015 "ripper.y"
    {
			(yyval.val) = remove_begin((yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 18:
#line 1021 "ripper.y"
    {
			(yyval.val) = (yyvsp[(1) - (1)].val);
		    ;}
    break;

  case 19:
#line 1025 "ripper.y"
    {
			yyerror("BEGIN is permitted only at toplevel");
#if 0
			/* local_push(0); */
#endif

		    ;}
    break;

  case 20:
#line 1033 "ripper.y"
    {
#if 0
			ruby_eval_tree_begin = block_append(ruby_eval_tree_begin,
							    (yyvsp[(4) - (5)].val));
			/* NEW_PREEXE($4)); */
			/* local_pop(); */
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(BEGIN, (yyvsp[(4) - (5)].val));

		    ;}
    break;

  case 21:
#line 1045 "ripper.y"
    {lex_state = EXPR_FNAME;;}
    break;

  case 22:
#line 1046 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ALIAS((yyvsp[(2) - (4)].val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = dispatch2(alias, (yyvsp[(2) - (4)].val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 23:
#line 1054 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_VALIAS((yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(var_alias, (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 24:
#line 1062 "ripper.y"
    {
#if 0
			char buf[2];
			buf[0] = '$';
			buf[1] = (char)(yyvsp[(3) - (3)].val)->nd_nth;
			(yyval.val) = NEW_VALIAS((yyvsp[(2) - (3)].val), rb_intern2(buf, 2));
#endif
			(yyval.val) = dispatch2(var_alias, (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 25:
#line 1073 "ripper.y"
    {
#if 0
			yyerror("can't make alias for the number variables");
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch2(var_alias, (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));
			(yyval.val) = dispatch1(alias_error, (yyval.val));

		    ;}
    break;

  case 26:
#line 1083 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch1(undef, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 27:
#line 1091 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_IF(cond((yyvsp[(3) - (3)].val)), remove_begin((yyvsp[(1) - (3)].val)), 0);
			fixpos((yyval.val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(if_mod, (yyvsp[(3) - (3)].val), (yyvsp[(1) - (3)].val));

		    ;}
    break;

  case 28:
#line 1100 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_UNLESS(cond((yyvsp[(3) - (3)].val)), remove_begin((yyvsp[(1) - (3)].val)), 0);
			fixpos((yyval.val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(unless_mod, (yyvsp[(3) - (3)].val), (yyvsp[(1) - (3)].val));

		    ;}
    break;

  case 29:
#line 1109 "ripper.y"
    {
#if 0
			if ((yyvsp[(1) - (3)].val) && nd_type((yyvsp[(1) - (3)].val)) == NODE_BEGIN) {
			    (yyval.val) = NEW_WHILE(cond((yyvsp[(3) - (3)].val)), (yyvsp[(1) - (3)].val)->nd_body, 0);
			}
			else {
			    (yyval.val) = NEW_WHILE(cond((yyvsp[(3) - (3)].val)), (yyvsp[(1) - (3)].val), 1);
			}
#endif
			(yyval.val) = dispatch2(while_mod, (yyvsp[(3) - (3)].val), (yyvsp[(1) - (3)].val));

		    ;}
    break;

  case 30:
#line 1122 "ripper.y"
    {
#if 0
			if ((yyvsp[(1) - (3)].val) && nd_type((yyvsp[(1) - (3)].val)) == NODE_BEGIN) {
			    (yyval.val) = NEW_UNTIL(cond((yyvsp[(3) - (3)].val)), (yyvsp[(1) - (3)].val)->nd_body, 0);
			}
			else {
			    (yyval.val) = NEW_UNTIL(cond((yyvsp[(3) - (3)].val)), (yyvsp[(1) - (3)].val), 1);
			}
#endif
			(yyval.val) = dispatch2(until_mod, (yyvsp[(3) - (3)].val), (yyvsp[(1) - (3)].val));

		    ;}
    break;

  case 31:
#line 1135 "ripper.y"
    {
#if 0
			NODE *resq = NEW_RESBODY(0, remove_begin((yyvsp[(3) - (3)].val)), 0);
			(yyval.val) = NEW_RESCUE(remove_begin((yyvsp[(1) - (3)].val)), resq, 0);
#endif
			(yyval.val) = dispatch2(rescue_mod, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 32:
#line 1144 "ripper.y"
    {
			if (in_def || in_single) {
			    rb_warn0("END in method; use at_exit");
			}
#if 0
			(yyval.val) = NEW_POSTEXE(NEW_NODE(
			    NODE_SCOPE, 0 /* tbl */, (yyvsp[(3) - (4)].val) /* body */, 0 /* args */));
#endif
			(yyval.val) = dispatch1(END, (yyvsp[(3) - (4)].val));

		    ;}
    break;

  case 34:
#line 1157 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (3)].val));
			(yyvsp[(1) - (3)].val)->nd_value = (yyvsp[(3) - (3)].val);
			(yyval.val) = (yyvsp[(1) - (3)].val);
#endif
			(yyval.val) = dispatch2(massign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 35:
#line 1167 "ripper.y"
    {
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = new_op_assign((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));
		    ;}
    break;

  case 36:
#line 1172 "ripper.y"
    {
#if 0
			NODE *args;

			value_expr((yyvsp[(6) - (6)].val));
			if (!(yyvsp[(3) - (6)].val)) (yyvsp[(3) - (6)].val) = NEW_ZARRAY();
			args = arg_concat((yyvsp[(3) - (6)].val), (yyvsp[(6) - (6)].val));
			if ((yyvsp[(5) - (6)].val) == tOROP) {
			    (yyvsp[(5) - (6)].val) = 0;
			}
			else if ((yyvsp[(5) - (6)].val) == tANDOP) {
			    (yyvsp[(5) - (6)].val) = 1;
			}
			(yyval.val) = NEW_OP_ASGN1((yyvsp[(1) - (6)].val), (yyvsp[(5) - (6)].val), args);
			fixpos((yyval.val), (yyvsp[(1) - (6)].val));
#endif
			(yyval.val) = dispatch2(aref_field, (yyvsp[(1) - (6)].val), escape_Qundef((yyvsp[(3) - (6)].val)));
			(yyval.val) = dispatch3(opassign, (yyval.val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));

		    ;}
    break;

  case 37:
#line 1193 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    ;}
    break;

  case 38:
#line 1198 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    ;}
    break;

  case 39:
#line 1203 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON2((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
			(yyval.val) = new_const_op_assign((yyval.val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
#endif
			(yyval.val) = dispatch2(const_path_field, (yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
			(yyval.val) = dispatch3(opassign, (yyval.val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));

		    ;}
    break;

  case 40:
#line 1213 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_intern("::"), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    ;}
    break;

  case 41:
#line 1218 "ripper.y"
    {
#if 0
			rb_backref_error((yyvsp[(1) - (3)].val));
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch2(assign, dispatch1(var_field, (yyvsp[(1) - (3)].val)), (yyvsp[(3) - (3)].val));
			(yyval.val) = dispatch1(assign_error, (yyval.val));

		    ;}
    break;

  case 42:
#line 1228 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = node_assign((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(assign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 43:
#line 1237 "ripper.y"
    {
#if 0
			(yyvsp[(1) - (3)].val)->nd_value = (yyvsp[(3) - (3)].val);
			(yyval.val) = (yyvsp[(1) - (3)].val);
#endif
			(yyval.val) = dispatch2(massign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 44:
#line 1246 "ripper.y"
    {
#if 0
			(yyvsp[(1) - (3)].val)->nd_value = (yyvsp[(3) - (3)].val);
			(yyval.val) = (yyvsp[(1) - (3)].val);
#endif
			(yyval.val) = dispatch2(massign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 46:
#line 1258 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = node_assign((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(assign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 47:
#line 1267 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = node_assign((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(assign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 49:
#line 1280 "ripper.y"
    {
#if 0
			(yyval.val) = logop(NODE_AND, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("and"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 50:
#line 1288 "ripper.y"
    {
#if 0
			(yyval.val) = logop(NODE_OR, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("or"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 51:
#line 1296 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op(cond((yyvsp[(3) - (3)].val)), '!');
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("not"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 52:
#line 1304 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op(cond((yyvsp[(2) - (2)].val)), '!');
#endif
			(yyval.val) = dispatch2(unary, ripper_id2sym('!'), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 54:
#line 1315 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (1)].val));
			(yyval.val) = (yyvsp[(1) - (1)].val);
		        if (!(yyval.val)) (yyval.val) = NEW_NIL();
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    ;}
    break;

  case 58:
#line 1332 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (4)].val), (yyvsp[(2) - (4)].val), (yyvsp[(3) - (4)].val));
			(yyval.val) = method_arg((yyval.val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 59:
#line 1343 "ripper.y"
    {
			(yyvsp[(1) - (1)].vars) = dyna_push();
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    ;}
    break;

  case 60:
#line 1353 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ITER((yyvsp[(3) - (5)].val),(yyvsp[(4) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(2) - (5)].num));
#endif
			(yyval.val) = dispatch2(brace_block, escape_Qundef((yyvsp[(3) - (5)].val)), (yyvsp[(4) - (5)].val));

			dyna_pop((yyvsp[(1) - (5)].vars));
		    ;}
    break;

  case 61:
#line 1365 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_FCALL((yyvsp[(1) - (1)].val), 0);
			nd_set_line((yyval.val), tokline);
#endif

		    ;}
    break;

  case 62:
#line 1375 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (2)].val);
			(yyval.val)->nd_args = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch2(command, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 63:
#line 1384 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(2) - (3)].val),(yyvsp[(3) - (3)].val));
			(yyvsp[(1) - (3)].val)->nd_args = (yyvsp[(2) - (3)].val);
		        (yyvsp[(3) - (3)].val)->nd_iter = (yyvsp[(1) - (3)].val);
			(yyval.val) = (yyvsp[(3) - (3)].val);
			fixpos((yyval.val), (yyvsp[(1) - (3)].val));
#endif
			(yyval.val) = dispatch2(command, (yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 64:
#line 1397 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
			fixpos((yyval.val), (yyvsp[(1) - (4)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (4)].val), ripper_id2sym('.'), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 65:
#line 1406 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(4) - (5)].val),(yyvsp[(5) - (5)].val));
		        (yyvsp[(5) - (5)].val)->nd_iter = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = (yyvsp[(5) - (5)].val);
			fixpos((yyval.val), (yyvsp[(1) - (5)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(5) - (5)].val));

		   ;}
    break;

  case 66:
#line 1418 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
			fixpos((yyval.val), (yyvsp[(1) - (4)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (4)].val), ripper_intern("::"), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 67:
#line 1427 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(4) - (5)].val),(yyvsp[(5) - (5)].val));
		        (yyvsp[(5) - (5)].val)->nd_iter = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = (yyvsp[(5) - (5)].val);
			fixpos((yyval.val), (yyvsp[(1) - (5)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (5)].val), ripper_intern("::"), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(5) - (5)].val));

		   ;}
    break;

  case 68:
#line 1439 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_SUPER((yyvsp[(2) - (2)].val));
			fixpos((yyval.val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(super, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 69:
#line 1448 "ripper.y"
    {
#if 0
			(yyval.val) = new_yield((yyvsp[(2) - (2)].val));
			fixpos((yyval.val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(yield, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 70:
#line 1457 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_RETURN(ret_args((yyvsp[(2) - (2)].val)));
#endif
			(yyval.val) = dispatch1(return, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 71:
#line 1465 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_BREAK(ret_args((yyvsp[(2) - (2)].val)));
#endif
			(yyval.val) = dispatch1(break, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 72:
#line 1473 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_NEXT(ret_args((yyvsp[(2) - (2)].val)));
#endif
			(yyval.val) = dispatch1(next, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 74:
#line 1484 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 76:
#line 1495 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(NEW_LIST((yyvsp[(2) - (3)].val)), 0);
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 77:
#line 1505 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (1)].val), 0);
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    ;}
    break;

  case 78:
#line 1513 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(list_append((yyvsp[(1) - (2)].val),(yyvsp[(2) - (2)].val)), 0);
#endif
			(yyval.val) = mlhs_add((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 79:
#line 1521 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 80:
#line 1529 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (5)].val), NEW_POSTARG((yyvsp[(3) - (5)].val),(yyvsp[(5) - (5)].val)));
#endif
			(yyvsp[(1) - (5)].val) = mlhs_add_star((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
			(yyval.val) = mlhs_add((yyvsp[(1) - (5)].val), (yyvsp[(5) - (5)].val));

		    ;}
    break;

  case 81:
#line 1538 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (2)].val), -1);
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (2)].val), Qnil);

		    ;}
    break;

  case 82:
#line 1546 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (4)].val), NEW_POSTARG(-1, (yyvsp[(4) - (4)].val)));
#endif
			(yyvsp[(1) - (4)].val) = mlhs_add_star((yyvsp[(1) - (4)].val), Qnil);
			(yyval.val) = mlhs_add((yyvsp[(1) - (4)].val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 83:
#line 1555 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 84:
#line 1563 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, NEW_POSTARG((yyvsp[(2) - (4)].val),(yyvsp[(4) - (4)].val)));
#endif
			(yyvsp[(2) - (4)].val) = mlhs_add_star(mlhs_new(), (yyvsp[(2) - (4)].val));
			(yyval.val) = mlhs_add((yyvsp[(2) - (4)].val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 85:
#line 1572 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, -1);
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), Qnil);

		    ;}
    break;

  case 86:
#line 1580 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, NEW_POSTARG(-1, (yyvsp[(3) - (3)].val)));
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), Qnil);
			(yyval.val) = mlhs_add((yyval.val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 88:
#line 1592 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 89:
#line 1602 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST((yyvsp[(1) - (2)].val));
#endif
			(yyval.val) = mlhs_add(mlhs_new(), (yyvsp[(1) - (2)].val));

		    ;}
    break;

  case 90:
#line 1610 "ripper.y"
    {
#if 0
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = mlhs_add((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 91:
#line 1620 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = mlhs_add(mlhs_new(), (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 92:
#line 1628 "ripper.y"
    {
#if 0
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = mlhs_add((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 93:
#line 1638 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
		    ;}
    break;

  case 94:
#line 1642 "ripper.y"
    {
		        (yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
		    ;}
    break;

  case 95:
#line 1646 "ripper.y"
    {
#if 0
			(yyval.val) = aryset((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val));
#endif
			(yyval.val) = dispatch2(aref_field, (yyvsp[(1) - (4)].val), escape_Qundef((yyvsp[(3) - (4)].val)));

		    ;}
    break;

  case 96:
#line 1654 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(field, (yyvsp[(1) - (3)].val), ripper_id2sym('.'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 97:
#line 1662 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(const_path_field, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 98:
#line 1670 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(field, (yyvsp[(1) - (3)].val), ripper_id2sym('.'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 99:
#line 1678 "ripper.y"
    {
#if 0
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.val) = NEW_CDECL(0, 0, NEW_COLON2((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val)));
#endif
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.val) = dispatch2(const_path_field, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 100:
#line 1690 "ripper.y"
    {
#if 0
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.val) = NEW_CDECL(0, 0, NEW_COLON3((yyvsp[(2) - (2)].val)));
#endif
			(yyval.val) = dispatch1(top_const_field, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 101:
#line 1700 "ripper.y"
    {
#if 0
			rb_backref_error((yyvsp[(1) - (1)].val));
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(var_field, (yyvsp[(1) - (1)].val));
			(yyval.val) = dispatch1(assign_error, (yyval.val));

		    ;}
    break;

  case 102:
#line 1712 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
#if 0
			if (!(yyval.val)) (yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(var_field, (yyval.val));

		    ;}
    break;

  case 103:
#line 1721 "ripper.y"
    {
		        (yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
#if 0
		        if (!(yyval.val)) (yyval.val) = NEW_BEGIN(0);
#endif
		        (yyval.val) = dispatch1(var_field, (yyval.val));

		    ;}
    break;

  case 104:
#line 1730 "ripper.y"
    {
#if 0
			(yyval.val) = aryset((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val));
#endif
			(yyval.val) = dispatch2(aref_field, (yyvsp[(1) - (4)].val), escape_Qundef((yyvsp[(3) - (4)].val)));

		    ;}
    break;

  case 105:
#line 1738 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(field, (yyvsp[(1) - (3)].val), ripper_id2sym('.'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 106:
#line 1746 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(field, (yyvsp[(1) - (3)].val), ripper_intern("::"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 107:
#line 1754 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(field, (yyvsp[(1) - (3)].val), ripper_id2sym('.'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 108:
#line 1762 "ripper.y"
    {
#if 0
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.val) = NEW_CDECL(0, 0, NEW_COLON2((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val)));
#endif
			(yyval.val) = dispatch2(const_path_field, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
			if (in_def || in_single) {
			    (yyval.val) = dispatch1(assign_error, (yyval.val));
			}

		    ;}
    break;

  case 109:
#line 1775 "ripper.y"
    {
#if 0
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.val) = NEW_CDECL(0, 0, NEW_COLON3((yyvsp[(2) - (2)].val)));
#endif
			(yyval.val) = dispatch1(top_const_field, (yyvsp[(2) - (2)].val));
			if (in_def || in_single) {
			    (yyval.val) = dispatch1(assign_error, (yyval.val));
			}

		    ;}
    break;

  case 110:
#line 1788 "ripper.y"
    {
#if 0
			rb_backref_error((yyvsp[(1) - (1)].val));
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(assign_error, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 111:
#line 1799 "ripper.y"
    {
#if 0
			yyerror("class/module name must be CONSTANT");
#endif
			(yyval.val) = dispatch1(class_name_error, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 113:
#line 1810 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON3((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(top_const_ref, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 114:
#line 1818 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON2(0, (yyval.val));
#endif
			(yyval.val) = dispatch1(const_ref, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 115:
#line 1826 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON2((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(const_path_ref, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 119:
#line 1839 "ripper.y"
    {
			lex_state = EXPR_ENDFN;
			(yyval.val) = (yyvsp[(1) - (1)].val);
		    ;}
    break;

  case 120:
#line 1844 "ripper.y"
    {
			lex_state = EXPR_ENDFN;
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].id);
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    ;}
    break;

  case 123:
#line 1859 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIT(ID2SYM((yyvsp[(1) - (1)].val)));
#endif
			(yyval.val) = dispatch1(symbol_literal, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 125:
#line 1870 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_UNDEF((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 126:
#line 1877 "ripper.y"
    {lex_state = EXPR_FNAME;;}
    break;

  case 127:
#line 1878 "ripper.y"
    {
#if 0
			(yyval.val) = block_append((yyvsp[(1) - (4)].val), NEW_UNDEF((yyvsp[(4) - (4)].val)));
#endif
			rb_ary_push((yyvsp[(1) - (4)].val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 128:
#line 1887 "ripper.y"
    { ifndef_ripper((yyval.val) = '|'); ;}
    break;

  case 129:
#line 1888 "ripper.y"
    { ifndef_ripper((yyval.val) = '^'); ;}
    break;

  case 130:
#line 1889 "ripper.y"
    { ifndef_ripper((yyval.val) = '&'); ;}
    break;

  case 131:
#line 1890 "ripper.y"
    { ifndef_ripper((yyval.val) = tCMP); ;}
    break;

  case 132:
#line 1891 "ripper.y"
    { ifndef_ripper((yyval.val) = tEQ); ;}
    break;

  case 133:
#line 1892 "ripper.y"
    { ifndef_ripper((yyval.val) = tEQQ); ;}
    break;

  case 134:
#line 1893 "ripper.y"
    { ifndef_ripper((yyval.val) = tMATCH); ;}
    break;

  case 135:
#line 1894 "ripper.y"
    { ifndef_ripper((yyval.val) = tNMATCH); ;}
    break;

  case 136:
#line 1895 "ripper.y"
    { ifndef_ripper((yyval.val) = '>'); ;}
    break;

  case 137:
#line 1896 "ripper.y"
    { ifndef_ripper((yyval.val) = tGEQ); ;}
    break;

  case 138:
#line 1897 "ripper.y"
    { ifndef_ripper((yyval.val) = '<'); ;}
    break;

  case 139:
#line 1898 "ripper.y"
    { ifndef_ripper((yyval.val) = tLEQ); ;}
    break;

  case 140:
#line 1899 "ripper.y"
    { ifndef_ripper((yyval.val) = tNEQ); ;}
    break;

  case 141:
#line 1900 "ripper.y"
    { ifndef_ripper((yyval.val) = tLSHFT); ;}
    break;

  case 142:
#line 1901 "ripper.y"
    { ifndef_ripper((yyval.val) = tRSHFT); ;}
    break;

  case 143:
#line 1902 "ripper.y"
    { ifndef_ripper((yyval.val) = '+'); ;}
    break;

  case 144:
#line 1903 "ripper.y"
    { ifndef_ripper((yyval.val) = '-'); ;}
    break;

  case 145:
#line 1904 "ripper.y"
    { ifndef_ripper((yyval.val) = '*'); ;}
    break;

  case 146:
#line 1905 "ripper.y"
    { ifndef_ripper((yyval.val) = '*'); ;}
    break;

  case 147:
#line 1906 "ripper.y"
    { ifndef_ripper((yyval.val) = '/'); ;}
    break;

  case 148:
#line 1907 "ripper.y"
    { ifndef_ripper((yyval.val) = '%'); ;}
    break;

  case 149:
#line 1908 "ripper.y"
    { ifndef_ripper((yyval.val) = tPOW); ;}
    break;

  case 150:
#line 1909 "ripper.y"
    { ifndef_ripper((yyval.val) = tDSTAR); ;}
    break;

  case 151:
#line 1910 "ripper.y"
    { ifndef_ripper((yyval.val) = '!'); ;}
    break;

  case 152:
#line 1911 "ripper.y"
    { ifndef_ripper((yyval.val) = '~'); ;}
    break;

  case 153:
#line 1912 "ripper.y"
    { ifndef_ripper((yyval.val) = tUPLUS); ;}
    break;

  case 154:
#line 1913 "ripper.y"
    { ifndef_ripper((yyval.val) = tUMINUS); ;}
    break;

  case 155:
#line 1914 "ripper.y"
    { ifndef_ripper((yyval.val) = tAREF); ;}
    break;

  case 156:
#line 1915 "ripper.y"
    { ifndef_ripper((yyval.val) = tASET); ;}
    break;

  case 157:
#line 1916 "ripper.y"
    { ifndef_ripper((yyval.val) = '`'); ;}
    break;

  case 199:
#line 1934 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = node_assign((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(assign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 200:
#line 1943 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (5)].val));
		        (yyvsp[(3) - (5)].val) = NEW_RESCUE((yyvsp[(3) - (5)].val), NEW_RESBODY(0,(yyvsp[(5) - (5)].val),0), 0);
			(yyval.val) = node_assign((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
#endif
			(yyval.val) = dispatch2(assign, (yyvsp[(1) - (5)].val), dispatch2(rescue_mod, (yyvsp[(3) - (5)].val), (yyvsp[(5) - (5)].val)));

		    ;}
    break;

  case 201:
#line 1953 "ripper.y"
    {
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = new_op_assign((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));
		    ;}
    break;

  case 202:
#line 1958 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (5)].val));
		        (yyvsp[(3) - (5)].val) = NEW_RESCUE((yyvsp[(3) - (5)].val), NEW_RESBODY(0,(yyvsp[(5) - (5)].val),0), 0);
#endif
			(yyvsp[(3) - (5)].val) = dispatch2(rescue_mod, (yyvsp[(3) - (5)].val), (yyvsp[(5) - (5)].val));

			(yyval.val) = new_op_assign((yyvsp[(1) - (5)].val), (yyvsp[(2) - (5)].val), (yyvsp[(3) - (5)].val));
		    ;}
    break;

  case 203:
#line 1968 "ripper.y"
    {
#if 0
			NODE *args;

			value_expr((yyvsp[(6) - (6)].val));
			if (!(yyvsp[(3) - (6)].val)) (yyvsp[(3) - (6)].val) = NEW_ZARRAY();
			if (nd_type((yyvsp[(3) - (6)].val)) == NODE_BLOCK_PASS) {
			    args = NEW_ARGSCAT((yyvsp[(3) - (6)].val), (yyvsp[(6) - (6)].val));
			}
		        else {
			    args = arg_concat((yyvsp[(3) - (6)].val), (yyvsp[(6) - (6)].val));
		        }
			if ((yyvsp[(5) - (6)].val) == tOROP) {
			    (yyvsp[(5) - (6)].val) = 0;
			}
			else if ((yyvsp[(5) - (6)].val) == tANDOP) {
			    (yyvsp[(5) - (6)].val) = 1;
			}
			(yyval.val) = NEW_OP_ASGN1((yyvsp[(1) - (6)].val), (yyvsp[(5) - (6)].val), args);
			fixpos((yyval.val), (yyvsp[(1) - (6)].val));
#endif
			(yyvsp[(1) - (6)].val) = dispatch2(aref_field, (yyvsp[(1) - (6)].val), escape_Qundef((yyvsp[(3) - (6)].val)));
			(yyval.val) = dispatch3(opassign, (yyvsp[(1) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));

		    ;}
    break;

  case 204:
#line 1994 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    ;}
    break;

  case 205:
#line 1999 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    ;}
    break;

  case 206:
#line 2004 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_intern("::"), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    ;}
    break;

  case 207:
#line 2009 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON2((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
			(yyval.val) = new_const_op_assign((yyval.val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
#endif
			(yyval.val) = dispatch2(const_path_field, (yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
			(yyval.val) = dispatch3(opassign, (yyval.val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));

		    ;}
    break;

  case 208:
#line 2019 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON3((yyvsp[(2) - (4)].val));
			(yyval.val) = new_const_op_assign((yyval.val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = dispatch1(top_const_field, (yyvsp[(2) - (4)].val));
			(yyval.val) = dispatch3(opassign, (yyval.val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 209:
#line 2029 "ripper.y"
    {
#if 0
			rb_backref_error((yyvsp[(1) - (3)].val));
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(var_field, (yyvsp[(1) - (3)].val));
			(yyval.val) = dispatch3(opassign, (yyval.val), (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));
			(yyval.val) = dispatch1(assign_error, (yyval.val));

		    ;}
    break;

  case 210:
#line 2040 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (3)].val));
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = NEW_DOT2((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
			if (nd_type((yyvsp[(1) - (3)].val)) == NODE_LIT && FIXNUM_P((yyvsp[(1) - (3)].val)->nd_lit) &&
			    nd_type((yyvsp[(3) - (3)].val)) == NODE_LIT && FIXNUM_P((yyvsp[(3) - (3)].val)->nd_lit)) {
			    deferred_nodes = list_append(deferred_nodes, (yyval.val));
			}
#endif
			(yyval.val) = dispatch2(dot2, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 211:
#line 2054 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (3)].val));
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = NEW_DOT3((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
			if (nd_type((yyvsp[(1) - (3)].val)) == NODE_LIT && FIXNUM_P((yyvsp[(1) - (3)].val)->nd_lit) &&
			    nd_type((yyvsp[(3) - (3)].val)) == NODE_LIT && FIXNUM_P((yyvsp[(3) - (3)].val)->nd_lit)) {
			    deferred_nodes = list_append(deferred_nodes, (yyval.val));
			}
#endif
			(yyval.val) = dispatch2(dot3, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 212:
#line 2068 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '+', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('+'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 213:
#line 2076 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '-', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('-'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 214:
#line 2084 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '*', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('*'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 215:
#line 2092 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '/', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('/'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 216:
#line 2100 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '%', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('%'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 217:
#line 2108 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tPOW, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("**"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 218:
#line 2116 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL(call_bin_op((yyvsp[(2) - (4)].val), tPOW, (yyvsp[(4) - (4)].val)), tUMINUS, 0);
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(2) - (4)].val), ripper_intern("**"), (yyvsp[(4) - (4)].val));
			(yyval.val) = dispatch2(unary, ripper_intern("-@"), (yyval.val));

		    ;}
    break;

  case 219:
#line 2125 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL(call_bin_op((yyvsp[(2) - (4)].val), tPOW, (yyvsp[(4) - (4)].val)), tUMINUS, 0);
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(2) - (4)].val), ripper_intern("**"), (yyvsp[(4) - (4)].val));
			(yyval.val) = dispatch2(unary, ripper_intern("-@"), (yyval.val));

		    ;}
    break;

  case 220:
#line 2134 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op((yyvsp[(2) - (2)].val), tUPLUS);
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("+@"), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 221:
#line 2142 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op((yyvsp[(2) - (2)].val), tUMINUS);
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("-@"), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 222:
#line 2150 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '|', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('|'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 223:
#line 2158 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '^', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('^'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 224:
#line 2166 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '&', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('&'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 225:
#line 2174 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tCMP, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("<=>"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 226:
#line 2182 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '>', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('>'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 227:
#line 2190 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tGEQ, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern(">="), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 228:
#line 2198 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '<', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('<'), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 229:
#line 2206 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tLEQ, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("<="), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 230:
#line 2214 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tEQ, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("=="), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 231:
#line 2222 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tEQQ, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("==="), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 232:
#line 2230 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tNEQ, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("!="), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 233:
#line 2238 "ripper.y"
    {
#if 0
			(yyval.val) = match_op((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
                        if (nd_type((yyvsp[(1) - (3)].val)) == NODE_LIT && RB_TYPE_P((yyvsp[(1) - (3)].val)->nd_lit, T_REGEXP)) {
                            (yyval.val) = reg_named_capture_assign((yyvsp[(1) - (3)].val)->nd_lit, (yyval.val));
                        }
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("=~"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 234:
#line 2249 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tNMATCH, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("!~"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 235:
#line 2257 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op(cond((yyvsp[(2) - (2)].val)), '!');
#endif
			(yyval.val) = dispatch2(unary, ID2SYM('!'), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 236:
#line 2265 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op((yyvsp[(2) - (2)].val), '~');
#endif
			(yyval.val) = dispatch2(unary, ID2SYM('~'), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 237:
#line 2273 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tLSHFT, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("<<"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 238:
#line 2281 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tRSHFT, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern(">>"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 239:
#line 2289 "ripper.y"
    {
#if 0
			(yyval.val) = logop(NODE_AND, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("&&"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 240:
#line 2297 "ripper.y"
    {
#if 0
			(yyval.val) = logop(NODE_OR, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("||"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 241:
#line 2304 "ripper.y"
    {in_defined = 1;;}
    break;

  case 242:
#line 2305 "ripper.y"
    {
#if 0
			in_defined = 0;
			(yyval.val) = NEW_DEFINED((yyvsp[(4) - (4)].val));
#endif
			in_defined = 0;
			(yyval.val) = dispatch1(defined, (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 243:
#line 2315 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (6)].val));
			(yyval.val) = NEW_IF(cond((yyvsp[(1) - (6)].val)), (yyvsp[(3) - (6)].val), (yyvsp[(6) - (6)].val));
			fixpos((yyval.val), (yyvsp[(1) - (6)].val));
#endif
			(yyval.val) = dispatch3(ifop, (yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(6) - (6)].val));

		    ;}
    break;

  case 244:
#line 2325 "ripper.y"
    {
			(yyval.val) = (yyvsp[(1) - (1)].val);
		    ;}
    break;

  case 245:
#line 2331 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (1)].val));
			(yyval.val) = (yyvsp[(1) - (1)].val);
		        if (!(yyval.val)) (yyval.val) = NEW_NIL();
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    ;}
    break;

  case 247:
#line 2344 "ripper.y"
    {
			(yyval.val) = (yyvsp[(1) - (2)].val);
		    ;}
    break;

  case 248:
#line 2348 "ripper.y"
    {
#if 0
			(yyval.val) = arg_append((yyvsp[(1) - (4)].val), NEW_HASH((yyvsp[(3) - (4)].val)));
#endif
			(yyval.val) = arg_add_assocs((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val));

		    ;}
    break;

  case 249:
#line 2356 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].val)));
#endif
			(yyval.val) = arg_add_assocs(arg_new(), (yyvsp[(1) - (2)].val));

		    ;}
    break;

  case 250:
#line 2366 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(arg_paren, escape_Qundef((yyvsp[(2) - (3)].val)));

		    ;}
    break;

  case 255:
#line 2382 "ripper.y"
    {
		      (yyval.val) = (yyvsp[(1) - (2)].val);
		    ;}
    break;

  case 256:
#line 2386 "ripper.y"
    {
#if 0
			(yyval.val) = arg_append((yyvsp[(1) - (4)].val), NEW_HASH((yyvsp[(3) - (4)].val)));
#endif
			(yyval.val) = arg_add_assocs((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val));

		    ;}
    break;

  case 257:
#line 2394 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].val)));
#endif
			(yyval.val) = arg_add_assocs(arg_new(), (yyvsp[(1) - (2)].val));

		    ;}
    break;

  case 258:
#line 2404 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (1)].val));
			(yyval.val) = NEW_LIST((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = arg_add(arg_new(), (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 259:
#line 2413 "ripper.y"
    {
#if 0
			(yyval.val) = arg_blk_pass((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = arg_add_optblock((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 260:
#line 2421 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].val)));
			(yyval.val) = arg_blk_pass((yyval.val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = arg_add_assocs(arg_new(), (yyvsp[(1) - (2)].val));
			(yyval.val) = arg_add_optblock((yyval.val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 261:
#line 2431 "ripper.y"
    {
#if 0
			(yyval.val) = arg_append((yyvsp[(1) - (4)].val), NEW_HASH((yyvsp[(3) - (4)].val)));
			(yyval.val) = arg_blk_pass((yyval.val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = arg_add_optblock(arg_add_assocs((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val)), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 262:
#line 2442 "ripper.y"
    {
			(yyval.val) = arg_add_block(arg_new(), (yyvsp[(1) - (1)].val));
		    ;}
    break;

  case 263:
#line 2448 "ripper.y"
    {
			(yyval.val) = cmdarg_stack;
			CMDARG_PUSH(1);
		    ;}
    break;

  case 264:
#line 2453 "ripper.y"
    {
			/* CMDARG_POP() */
			cmdarg_stack = (yyvsp[(1) - (2)].val);
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    ;}
    break;

  case 265:
#line 2461 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_BLOCK_PASS((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = (yyvsp[(2) - (2)].val);

		    ;}
    break;

  case 266:
#line 2471 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    ;}
    break;

  case 267:
#line 2475 "ripper.y"
    {
			(yyval.val) = 0;
		    ;}
    break;

  case 268:
#line 2481 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = arg_add(arg_new(), (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 269:
#line 2489 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_SPLAT((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = arg_add_star(arg_new(), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 270:
#line 2497 "ripper.y"
    {
#if 0
			NODE *n1;
			if ((n1 = splat_array((yyvsp[(1) - (3)].val))) != 0) {
			    (yyval.val) = list_append(n1, (yyvsp[(3) - (3)].val));
			}
			else {
			    (yyval.val) = arg_append((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
			}
#endif
			(yyval.val) = arg_add((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 271:
#line 2511 "ripper.y"
    {
#if 0
			NODE *n1;
			if ((nd_type((yyvsp[(4) - (4)].val)) == NODE_ARRAY) && (n1 = splat_array((yyvsp[(1) - (4)].val))) != 0) {
			    (yyval.val) = list_concat(n1, (yyvsp[(4) - (4)].val));
			}
			else {
			    (yyval.val) = arg_concat((yyvsp[(1) - (4)].val), (yyvsp[(4) - (4)].val));
			}
#endif
			(yyval.val) = arg_add_star((yyvsp[(1) - (4)].val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 272:
#line 2527 "ripper.y"
    {
#if 0
			NODE *n1;
			if ((n1 = splat_array((yyvsp[(1) - (3)].val))) != 0) {
			    (yyval.val) = list_append(n1, (yyvsp[(3) - (3)].val));
			}
			else {
			    (yyval.val) = arg_append((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
			}
#endif
			(yyval.val) = mrhs_add(args2mrhs((yyvsp[(1) - (3)].val)), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 273:
#line 2541 "ripper.y"
    {
#if 0
			NODE *n1;
			if (nd_type((yyvsp[(4) - (4)].val)) == NODE_ARRAY &&
			    (n1 = splat_array((yyvsp[(1) - (4)].val))) != 0) {
			    (yyval.val) = list_concat(n1, (yyvsp[(4) - (4)].val));
			}
			else {
			    (yyval.val) = arg_concat((yyvsp[(1) - (4)].val), (yyvsp[(4) - (4)].val));
			}
#endif
			(yyval.val) = mrhs_add_star(args2mrhs((yyvsp[(1) - (4)].val)), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 274:
#line 2556 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_SPLAT((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = mrhs_add_star(mrhs_new(), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 285:
#line 2576 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_FCALL((yyvsp[(1) - (1)].val), 0);
#endif
			(yyval.val) = method_arg(dispatch1(fcall, (yyvsp[(1) - (1)].val)), arg_new());

		    ;}
    break;

  case 286:
#line 2584 "ripper.y"
    {
			(yyvsp[(1) - (1)].val) = cmdarg_stack;
			cmdarg_stack = 0;
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    ;}
    break;

  case 287:
#line 2594 "ripper.y"
    {
			cmdarg_stack = (yyvsp[(1) - (4)].val);
#if 0
			if ((yyvsp[(3) - (4)].val) == NULL) {
			    (yyval.val) = NEW_NIL();
			}
			else {
			    if (nd_type((yyvsp[(3) - (4)].val)) == NODE_RESCUE ||
				nd_type((yyvsp[(3) - (4)].val)) == NODE_ENSURE)
				nd_set_line((yyvsp[(3) - (4)].val), (yyvsp[(2) - (4)].num));
			    (yyval.val) = NEW_BEGIN((yyvsp[(3) - (4)].val));
			}
			nd_set_line((yyval.val), (yyvsp[(2) - (4)].num));
#endif
			(yyval.val) = dispatch1(begin, (yyvsp[(3) - (4)].val));

		    ;}
    break;

  case 288:
#line 2611 "ripper.y"
    {lex_state = EXPR_ENDARG;;}
    break;

  case 289:
#line 2612 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch1(paren, 0);

		    ;}
    break;

  case 290:
#line 2619 "ripper.y"
    {lex_state = EXPR_ENDARG;;}
    break;

  case 291:
#line 2620 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (4)].val);
#endif
			(yyval.val) = dispatch1(paren, (yyvsp[(2) - (4)].val));

		    ;}
    break;

  case 292:
#line 2628 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(paren, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 293:
#line 2636 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON2((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(const_path_ref, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 294:
#line 2644 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON3((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(top_const_ref, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 295:
#line 2652 "ripper.y"
    {
#if 0
			if ((yyvsp[(2) - (3)].val) == 0) {
			    (yyval.val) = NEW_ZARRAY(); /* zero length array*/
			}
			else {
			    (yyval.val) = (yyvsp[(2) - (3)].val);
			}
#endif
			(yyval.val) = dispatch1(array, escape_Qundef((yyvsp[(2) - (3)].val)));

		    ;}
    break;

  case 296:
#line 2665 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_HASH((yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = dispatch1(hash, escape_Qundef((yyvsp[(2) - (3)].val)));

		    ;}
    break;

  case 297:
#line 2673 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_RETURN(0);
#endif
			(yyval.val) = dispatch0(return0);

		    ;}
    break;

  case 298:
#line 2681 "ripper.y"
    {
#if 0
			(yyval.val) = new_yield((yyvsp[(3) - (4)].val));
#endif
			(yyval.val) = dispatch1(yield, dispatch1(paren, (yyvsp[(3) - (4)].val)));

		    ;}
    break;

  case 299:
#line 2689 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_YIELD(0);
#endif
			(yyval.val) = dispatch1(yield, dispatch1(paren, arg_new()));

		    ;}
    break;

  case 300:
#line 2697 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_YIELD(0);
#endif
			(yyval.val) = dispatch0(yield0);

		    ;}
    break;

  case 301:
#line 2704 "ripper.y"
    {in_defined = 1;;}
    break;

  case 302:
#line 2705 "ripper.y"
    {
#if 0
			in_defined = 0;
			(yyval.val) = NEW_DEFINED((yyvsp[(5) - (6)].val));
#endif
			in_defined = 0;
			(yyval.val) = dispatch1(defined, (yyvsp[(5) - (6)].val));

		    ;}
    break;

  case 303:
#line 2715 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op(cond((yyvsp[(3) - (4)].val)), '!');
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("not"), (yyvsp[(3) - (4)].val));

		    ;}
    break;

  case 304:
#line 2723 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op(cond(NEW_NIL()), '!');
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("not"), Qnil);

		    ;}
    break;

  case 305:
#line 2731 "ripper.y"
    {
#if 0
			(yyvsp[(2) - (2)].val)->nd_iter = (yyvsp[(1) - (2)].val);
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = method_arg(dispatch1(fcall, (yyvsp[(1) - (2)].val)), arg_new());
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 307:
#line 2742 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(1) - (2)].val)->nd_args, (yyvsp[(2) - (2)].val));
			(yyvsp[(2) - (2)].val)->nd_iter = (yyvsp[(1) - (2)].val);
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = method_add_block((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 308:
#line 2752 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    ;}
    break;

  case 309:
#line 2759 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_IF(cond((yyvsp[(2) - (6)].val)), (yyvsp[(4) - (6)].val), (yyvsp[(5) - (6)].val));
			fixpos((yyval.val), (yyvsp[(2) - (6)].val));
#endif
			(yyval.val) = dispatch3(if, (yyvsp[(2) - (6)].val), (yyvsp[(4) - (6)].val), escape_Qundef((yyvsp[(5) - (6)].val)));

		    ;}
    break;

  case 310:
#line 2771 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_UNLESS(cond((yyvsp[(2) - (6)].val)), (yyvsp[(4) - (6)].val), (yyvsp[(5) - (6)].val));
			fixpos((yyval.val), (yyvsp[(2) - (6)].val));
#endif
			(yyval.val) = dispatch3(unless, (yyvsp[(2) - (6)].val), (yyvsp[(4) - (6)].val), escape_Qundef((yyvsp[(5) - (6)].val)));

		    ;}
    break;

  case 311:
#line 2779 "ripper.y"
    {COND_PUSH(1);;}
    break;

  case 312:
#line 2779 "ripper.y"
    {COND_POP();;}
    break;

  case 313:
#line 2782 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_WHILE(cond((yyvsp[(3) - (7)].val)), (yyvsp[(6) - (7)].val), 1);
			fixpos((yyval.val), (yyvsp[(3) - (7)].val));
#endif
			(yyval.val) = dispatch2(while, (yyvsp[(3) - (7)].val), (yyvsp[(6) - (7)].val));

		    ;}
    break;

  case 314:
#line 2790 "ripper.y"
    {COND_PUSH(1);;}
    break;

  case 315:
#line 2790 "ripper.y"
    {COND_POP();;}
    break;

  case 316:
#line 2793 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_UNTIL(cond((yyvsp[(3) - (7)].val)), (yyvsp[(6) - (7)].val), 1);
			fixpos((yyval.val), (yyvsp[(3) - (7)].val));
#endif
			(yyval.val) = dispatch2(until, (yyvsp[(3) - (7)].val), (yyvsp[(6) - (7)].val));

		    ;}
    break;

  case 317:
#line 2804 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CASE((yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val));
			fixpos((yyval.val), (yyvsp[(2) - (5)].val));
#endif
			(yyval.val) = dispatch2(case, (yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val));

		    ;}
    break;

  case 318:
#line 2813 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CASE(0, (yyvsp[(3) - (4)].val));
#endif
			(yyval.val) = dispatch2(case, Qnil, (yyvsp[(3) - (4)].val));

		    ;}
    break;

  case 319:
#line 2821 "ripper.y"
    {COND_PUSH(1);;}
    break;

  case 320:
#line 2823 "ripper.y"
    {COND_POP();;}
    break;

  case 321:
#line 2826 "ripper.y"
    {
#if 0
			/*
			 *  for a, b, c in e
			 *  #=>
			 *  e.each{|*x| a, b, c = x
			 *
			 *  for a in e
			 *  #=>
			 *  e.each{|x| a, = x}
			 */
			ID id = internal_id();
			ID *tbl = ALLOC_N(ID, 2);
			NODE *m = NEW_ARGS_AUX(0, 0);
			NODE *args, *scope;

			if (nd_type((yyvsp[(2) - (9)].val)) == NODE_MASGN) {
			    /* if args.length == 1 && args[0].kind_of?(Array)
			     *   args = args[0]
			     * end
			     */
			    NODE *one = NEW_LIST(NEW_LIT(INT2FIX(1)));
			    NODE *zero = NEW_LIST(NEW_LIT(INT2FIX(0)));
			    m->nd_next = block_append(
				NEW_IF(
				    NEW_NODE(NODE_AND,
					     NEW_CALL(NEW_CALL(NEW_DVAR(id), idLength, 0),
						      idEq, one),
					     NEW_CALL(NEW_CALL(NEW_DVAR(id), idAREF, zero),
						      rb_intern("kind_of?"), NEW_LIST(NEW_LIT(rb_cArray))),
					     0),
				    NEW_DASGN_CURR(id,
						   NEW_CALL(NEW_DVAR(id), idAREF, zero)),
				    0),
				node_assign((yyvsp[(2) - (9)].val), NEW_DVAR(id)));

			    args = new_args(m, 0, id, 0, new_args_tail(0, 0, 0));
			}
			else {
			    if (nd_type((yyvsp[(2) - (9)].val)) == NODE_LASGN ||
				nd_type((yyvsp[(2) - (9)].val)) == NODE_DASGN ||
				nd_type((yyvsp[(2) - (9)].val)) == NODE_DASGN_CURR) {
				(yyvsp[(2) - (9)].val)->nd_value = NEW_DVAR(id);
				m->nd_plen = 1;
				m->nd_next = (yyvsp[(2) - (9)].val);
				args = new_args(m, 0, 0, 0, new_args_tail(0, 0, 0));
			    }
			    else {
				m->nd_next = node_assign(NEW_MASGN(NEW_LIST((yyvsp[(2) - (9)].val)), 0), NEW_DVAR(id));
				args = new_args(m, 0, id, 0, new_args_tail(0, 0, 0));
			    }
			}
			scope = NEW_NODE(NODE_SCOPE, tbl, (yyvsp[(8) - (9)].val), args);
			tbl[0] = 1; tbl[1] = id;
			(yyval.val) = NEW_FOR(0, (yyvsp[(5) - (9)].val), scope);
			fixpos((yyval.val), (yyvsp[(2) - (9)].val));
#endif
			(yyval.val) = dispatch3(for, (yyvsp[(2) - (9)].val), (yyvsp[(5) - (9)].val), (yyvsp[(8) - (9)].val));

		    ;}
    break;

  case 322:
#line 2887 "ripper.y"
    {
			if (in_def || in_single)
			    yyerror("class definition in method body");
			local_push(0);
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    ;}
    break;

  case 323:
#line 2898 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CLASS((yyvsp[(2) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(3) - (6)].val));
			nd_set_line((yyval.val), (yyvsp[(4) - (6)].num));
#endif
			(yyval.val) = dispatch3(class, (yyvsp[(2) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val));

			local_pop();
		    ;}
    break;

  case 324:
#line 2908 "ripper.y"
    {
			(yyval.num) = in_def;
			in_def = 0;
		    ;}
    break;

  case 325:
#line 2913 "ripper.y"
    {
			(yyval.num) = in_single;
			in_single = 0;
			local_push(0);
		    ;}
    break;

  case 326:
#line 2920 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_SCLASS((yyvsp[(3) - (8)].val), (yyvsp[(7) - (8)].val));
			fixpos((yyval.val), (yyvsp[(3) - (8)].val));
#endif
			(yyval.val) = dispatch2(sclass, (yyvsp[(3) - (8)].val), (yyvsp[(7) - (8)].val));

			local_pop();
			in_def = (yyvsp[(4) - (8)].num);
			in_single = (yyvsp[(6) - (8)].num);
		    ;}
    break;

  case 327:
#line 2932 "ripper.y"
    {
			if (in_def || in_single)
			    yyerror("module definition in method body");
			local_push(0);
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    ;}
    break;

  case 328:
#line 2943 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MODULE((yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(3) - (5)].num));
#endif
			(yyval.val) = dispatch2(module, (yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val));

			local_pop();
		    ;}
    break;

  case 329:
#line 2953 "ripper.y"
    {
			(yyval.id) = cur_mid;
			cur_mid = (yyvsp[(2) - (2)].val);
			in_def++;
			local_push(0);
		    ;}
    break;

  case 330:
#line 2962 "ripper.y"
    {
#if 0
			NODE *body = remove_begin((yyvsp[(5) - (6)].val));
			reduce_nodes(&body);
			(yyval.val) = NEW_DEFN((yyvsp[(2) - (6)].val), (yyvsp[(4) - (6)].val), body, NOEX_PRIVATE);
			nd_set_line((yyval.val), (yyvsp[(1) - (6)].num));
#endif
			(yyval.val) = dispatch3(def, (yyvsp[(2) - (6)].val), (yyvsp[(4) - (6)].val), (yyvsp[(5) - (6)].val));

			local_pop();
			in_def--;
			cur_mid = (yyvsp[(3) - (6)].id);
		    ;}
    break;

  case 331:
#line 2975 "ripper.y"
    {lex_state = EXPR_FNAME;;}
    break;

  case 332:
#line 2976 "ripper.y"
    {
			in_single++;
			lex_state = EXPR_ENDFN; /* force for args */
			local_push(0);
		    ;}
    break;

  case 333:
#line 2984 "ripper.y"
    {
#if 0
			NODE *body = remove_begin((yyvsp[(8) - (9)].val));
			reduce_nodes(&body);
			(yyval.val) = NEW_DEFS((yyvsp[(2) - (9)].val), (yyvsp[(5) - (9)].val), (yyvsp[(7) - (9)].val), body);
			nd_set_line((yyval.val), (yyvsp[(1) - (9)].num));
#endif
			(yyval.val) = dispatch5(defs, (yyvsp[(2) - (9)].val), (yyvsp[(3) - (9)].val), (yyvsp[(5) - (9)].val), (yyvsp[(7) - (9)].val), (yyvsp[(8) - (9)].val));

			local_pop();
			in_single--;
		    ;}
    break;

  case 334:
#line 2997 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_BREAK(0);
#endif
			(yyval.val) = dispatch1(break, arg_new());

		    ;}
    break;

  case 335:
#line 3005 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_NEXT(0);
#endif
			(yyval.val) = dispatch1(next, arg_new());

		    ;}
    break;

  case 336:
#line 3013 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_REDO();
#endif
			(yyval.val) = dispatch0(redo);

		    ;}
    break;

  case 337:
#line 3021 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_RETRY();
#endif
			(yyval.val) = dispatch0(retry);

		    ;}
    break;

  case 338:
#line 3031 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (1)].val));
			(yyval.val) = (yyvsp[(1) - (1)].val);
		        if (!(yyval.val)) (yyval.val) = NEW_NIL();
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    ;}
    break;

  case 339:
#line 3043 "ripper.y"
    {
			token_info_push("begin");
		    ;}
    break;

  case 340:
#line 3049 "ripper.y"
    {
			token_info_push("if");
		    ;}
    break;

  case 341:
#line 3055 "ripper.y"
    {
			token_info_push("unless");
		    ;}
    break;

  case 342:
#line 3061 "ripper.y"
    {
			token_info_push("while");
		    ;}
    break;

  case 343:
#line 3067 "ripper.y"
    {
			token_info_push("until");
		    ;}
    break;

  case 344:
#line 3073 "ripper.y"
    {
			token_info_push("case");
		    ;}
    break;

  case 345:
#line 3079 "ripper.y"
    {
			token_info_push("for");
		    ;}
    break;

  case 346:
#line 3085 "ripper.y"
    {
			token_info_push("class");
		    ;}
    break;

  case 347:
#line 3091 "ripper.y"
    {
			token_info_push("module");
		    ;}
    break;

  case 348:
#line 3097 "ripper.y"
    {
			token_info_push("def");
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    ;}
    break;

  case 349:
#line 3107 "ripper.y"
    {
			token_info_pop("end");
		    ;}
    break;

  case 350:
#line 3115 "ripper.y"
    { (yyval.val) = Qnil; ;}
    break;

  case 352:
#line 3121 "ripper.y"
    { (yyval.val) = (yyvsp[(2) - (2)].val); ;}
    break;

  case 353:
#line 3128 "ripper.y"
    { (yyval.val) = Qnil; ;}
    break;

  case 356:
#line 3137 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_IF(cond((yyvsp[(2) - (5)].val)), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
			fixpos((yyval.val), (yyvsp[(2) - (5)].val));
#endif
			(yyval.val) = dispatch3(elsif, (yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val), escape_Qundef((yyvsp[(5) - (5)].val)));

		    ;}
    break;

  case 358:
#line 3149 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch1(else, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 361:
#line 3163 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
#if 0
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyval.val));

		    ;}
    break;

  case 362:
#line 3171 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 363:
#line 3181 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = mlhs_add(mlhs_new(), (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 364:
#line 3189 "ripper.y"
    {
#if 0
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = mlhs_add((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 365:
#line 3199 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (1)].val), 0);
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    ;}
    break;

  case 366:
#line 3207 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(4) - (4)].val), 0);
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (4)].val), (yyval.val));
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (4)].val), (yyval.val));

		    ;}
    break;

  case 367:
#line 3216 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(4) - (6)].val), 0);
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (6)].val), NEW_POSTARG((yyval.val), (yyvsp[(6) - (6)].val)));
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (6)].val), (yyval.val));

		    ;}
    break;

  case 368:
#line 3225 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (3)].val), -1);
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (3)].val), Qnil);

		    ;}
    break;

  case 369:
#line 3233 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (5)].val), NEW_POSTARG(-1, (yyvsp[(5) - (5)].val)));
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (5)].val), (yyvsp[(5) - (5)].val));

		    ;}
    break;

  case 370:
#line 3241 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(2) - (2)].val), 0);
#if 0
			(yyval.val) = NEW_MASGN(0, (yyval.val));
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), (yyval.val));

		    ;}
    break;

  case 371:
#line 3250 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(2) - (4)].val), 0);
#if 0
			(yyval.val) = NEW_MASGN(0, NEW_POSTARG((yyval.val), (yyvsp[(4) - (4)].val)));
#endif
		      #if 0
		      TODO: Check me
		      #endif
			(yyval.val) = mlhs_add_star((yyval.val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 372:
#line 3262 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, -1);
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), Qnil);

		    ;}
    break;

  case 373:
#line 3270 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, NEW_POSTARG(-1, (yyvsp[(3) - (3)].val)));
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), Qnil);

		    ;}
    break;

  case 374:
#line 3281 "ripper.y"
    {
			(yyval.val) = new_args_tail((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 375:
#line 3285 "ripper.y"
    {
			(yyval.val) = new_args_tail((yyvsp[(1) - (2)].val), Qnone, (yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 376:
#line 3289 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 377:
#line 3293 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, Qnone, (yyvsp[(1) - (1)].val));
		    ;}
    break;

  case 378:
#line 3299 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    ;}
    break;

  case 379:
#line 3303 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, Qnone, Qnone);
		    ;}
    break;

  case 380:
#line 3309 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), Qnone, (yyvsp[(6) - (6)].val));
		    ;}
    break;

  case 381:
#line 3313 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (8)].val), (yyvsp[(3) - (8)].val), (yyvsp[(5) - (8)].val), (yyvsp[(7) - (8)].val), (yyvsp[(8) - (8)].val));
		    ;}
    break;

  case 382:
#line 3317 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), Qnone, Qnone, (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 383:
#line 3321 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), Qnone, (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    ;}
    break;

  case 384:
#line 3325 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (4)].val), Qnone, (yyvsp[(3) - (4)].val), Qnone, (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 385:
#line 3329 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (2)].val), Qnone, 1, Qnone, new_args_tail(Qnone, Qnone, Qnone));
#if 0
#endif
                        dispatch1(excessed_comma, (yyval.val));

		    ;}
    break;

  case 386:
#line 3337 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), Qnone, (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    ;}
    break;

  case 387:
#line 3341 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (2)].val), Qnone, Qnone, Qnone, (yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 388:
#line 3345 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), Qnone, (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 389:
#line 3349 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    ;}
    break;

  case 390:
#line 3353 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (2)].val), Qnone, Qnone, (yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 391:
#line 3357 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (4)].val), Qnone, (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 392:
#line 3361 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, (yyvsp[(1) - (2)].val), Qnone, (yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 393:
#line 3365 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, (yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 394:
#line 3369 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, Qnone, Qnone, (yyvsp[(1) - (1)].val));
		    ;}
    break;

  case 396:
#line 3376 "ripper.y"
    {
			command_start = TRUE;
		    ;}
    break;

  case 397:
#line 3382 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = blockvar_new(params_new(Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil),
                                          escape_Qundef((yyvsp[(2) - (3)].val)));

		    ;}
    break;

  case 398:
#line 3391 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = blockvar_new(params_new(Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil),
                                          Qnil);

		    ;}
    break;

  case 399:
#line 3400 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (4)].val);
#endif
			(yyval.val) = blockvar_new(escape_Qundef((yyvsp[(2) - (4)].val)), escape_Qundef((yyvsp[(3) - (4)].val)));

		    ;}
    break;

  case 400:
#line 3411 "ripper.y"
    {
		      (yyval.val) = 0;
		    ;}
    break;

  case 401:
#line 3415 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = (yyvsp[(3) - (4)].val);

		    ;}
    break;

  case 402:
#line 3427 "ripper.y"
    {
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));
		    ;}
    break;

  case 403:
#line 3434 "ripper.y"
    {
			rb_ary_push((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
		    ;}
    break;

  case 404:
#line 3441 "ripper.y"
    {
			new_bv(get_id((yyvsp[(1) - (1)].val)));
#if 0
#endif
			(yyval.val) = get_value((yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 405:
#line 3449 "ripper.y"
    {
			(yyval.val) = 0;
		    ;}
    break;

  case 406:
#line 3454 "ripper.y"
    {
			(yyval.vars) = dyna_push();
		    ;}
    break;

  case 407:
#line 3457 "ripper.y"
    {
			(yyval.num) = lpar_beg;
			lpar_beg = ++paren_nest;
		    ;}
    break;

  case 408:
#line 3463 "ripper.y"
    {
			lpar_beg = (yyvsp[(2) - (4)].num);
#if 0
			(yyval.val) = NEW_LAMBDA((yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = dispatch2(lambda, (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));

			dyna_pop((yyvsp[(1) - (4)].vars));
		    ;}
    break;

  case 409:
#line 3475 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (4)].val);
#endif
			(yyval.val) = dispatch1(paren, (yyvsp[(2) - (4)].val));

		    ;}
    break;

  case 410:
#line 3483 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    ;}
    break;

  case 411:
#line 3493 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (3)].val);
		    ;}
    break;

  case 412:
#line 3497 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (3)].val);
		    ;}
    break;

  case 413:
#line 3503 "ripper.y"
    {
			(yyvsp[(1) - (1)].vars) = dyna_push();
#if 0
			(yyval.num) = ruby_sourceline;
#endif
		    ;}
    break;

  case 414:
#line 3512 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ITER((yyvsp[(3) - (5)].val),(yyvsp[(4) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(2) - (5)].num));
#endif
			(yyval.val) = dispatch2(do_block, escape_Qundef((yyvsp[(3) - (5)].val)), (yyvsp[(4) - (5)].val));

			dyna_pop((yyvsp[(1) - (5)].vars));
		    ;}
    break;

  case 415:
#line 3524 "ripper.y"
    {
#if 0
			if (nd_type((yyvsp[(1) - (2)].val)) == NODE_YIELD) {
			    compile_error(PARSER_ARG "block given to yield");
			}
			else {
			    block_dup_check((yyvsp[(1) - (2)].val)->nd_args, (yyvsp[(2) - (2)].val));
			}
			(yyvsp[(2) - (2)].val)->nd_iter = (yyvsp[(1) - (2)].val);
			(yyval.val) = (yyvsp[(2) - (2)].val);
			fixpos((yyval.val), (yyvsp[(1) - (2)].val));
#endif
			(yyval.val) = method_add_block((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 416:
#line 3540 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (4)].val), (yyvsp[(2) - (4)].val), (yyvsp[(3) - (4)].val));
			(yyval.val) = method_optarg((yyval.val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 417:
#line 3549 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
			(yyvsp[(5) - (5)].val)->nd_iter = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = (yyvsp[(5) - (5)].val);
			fixpos((yyval.val), (yyvsp[(1) - (5)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (5)].val), (yyvsp[(2) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(5) - (5)].val));

		    ;}
    break;

  case 418:
#line 3561 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
			(yyvsp[(5) - (5)].val)->nd_iter = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = (yyvsp[(5) - (5)].val);
			fixpos((yyval.val), (yyvsp[(1) - (5)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (5)].val), (yyvsp[(2) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(5) - (5)].val));

		    ;}
    break;

  case 419:
#line 3575 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (2)].val);
			(yyval.val)->nd_args = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = method_arg(dispatch1(fcall, (yyvsp[(1) - (2)].val)), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 420:
#line 3584 "ripper.y"
    {
#if 0
			(yyval.num) = ruby_sourceline;
#endif
		    ;}
    break;

  case 421:
#line 3590 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(5) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(4) - (5)].num));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val));
			(yyval.val) = method_optarg((yyval.val), (yyvsp[(5) - (5)].val));

		    ;}
    break;

  case 422:
#line 3600 "ripper.y"
    {
#if 0
			(yyval.num) = ruby_sourceline;
#endif
		    ;}
    break;

  case 423:
#line 3606 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(5) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(4) - (5)].num));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val));
			(yyval.val) = method_optarg((yyval.val), (yyvsp[(5) - (5)].val));

		    ;}
    break;

  case 424:
#line 3616 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val), 0);
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (3)].val), ripper_intern("::"), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 425:
#line 3624 "ripper.y"
    {
#if 0
			(yyval.num) = ruby_sourceline;
#endif
		    ;}
    break;

  case 426:
#line 3630 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), rb_intern("call"), (yyvsp[(4) - (4)].val));
			nd_set_line((yyval.val), (yyvsp[(3) - (4)].num));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (4)].val), ripper_id2sym('.'),
				       ripper_intern("call"));
			(yyval.val) = method_optarg((yyval.val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 427:
#line 3641 "ripper.y"
    {
#if 0
			(yyval.num) = ruby_sourceline;
#endif
		    ;}
    break;

  case 428:
#line 3647 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), rb_intern("call"), (yyvsp[(4) - (4)].val));
			nd_set_line((yyval.val), (yyvsp[(3) - (4)].num));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (4)].val), ripper_intern("::"),
				       ripper_intern("call"));
			(yyval.val) = method_optarg((yyval.val), (yyvsp[(4) - (4)].val));

		    ;}
    break;

  case 429:
#line 3658 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_SUPER((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(super, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 430:
#line 3666 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ZSUPER();
#endif
			(yyval.val) = dispatch0(zsuper);

		    ;}
    break;

  case 431:
#line 3674 "ripper.y"
    {
#if 0
			if ((yyvsp[(1) - (4)].val) && nd_type((yyvsp[(1) - (4)].val)) == NODE_SELF)
			    (yyval.val) = NEW_FCALL(tAREF, (yyvsp[(3) - (4)].val));
			else
			    (yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), tAREF, (yyvsp[(3) - (4)].val));
			fixpos((yyval.val), (yyvsp[(1) - (4)].val));
#endif
			(yyval.val) = dispatch2(aref, (yyvsp[(1) - (4)].val), escape_Qundef((yyvsp[(3) - (4)].val)));

		    ;}
    break;

  case 432:
#line 3688 "ripper.y"
    {
			(yyvsp[(1) - (1)].vars) = dyna_push();
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    ;}
    break;

  case 433:
#line 3697 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ITER((yyvsp[(3) - (5)].val),(yyvsp[(4) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(2) - (5)].num));
#endif
			(yyval.val) = dispatch2(brace_block, escape_Qundef((yyvsp[(3) - (5)].val)), (yyvsp[(4) - (5)].val));

			dyna_pop((yyvsp[(1) - (5)].vars));
		    ;}
    break;

  case 434:
#line 3707 "ripper.y"
    {
			(yyvsp[(1) - (1)].vars) = dyna_push();
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    ;}
    break;

  case 435:
#line 3716 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ITER((yyvsp[(3) - (5)].val),(yyvsp[(4) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(2) - (5)].num));
#endif
			(yyval.val) = dispatch2(do_block, escape_Qundef((yyvsp[(3) - (5)].val)), (yyvsp[(4) - (5)].val));

			dyna_pop((yyvsp[(1) - (5)].vars));
		    ;}
    break;

  case 436:
#line 3730 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_WHEN((yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
#endif
			(yyval.val) = dispatch3(when, (yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val), escape_Qundef((yyvsp[(5) - (5)].val)));

		    ;}
    break;

  case 439:
#line 3746 "ripper.y"
    {
#if 0
			if ((yyvsp[(3) - (6)].val)) {
			    (yyvsp[(3) - (6)].val) = node_assign((yyvsp[(3) - (6)].val), NEW_ERRINFO());
			    (yyvsp[(5) - (6)].val) = block_append((yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val));
			}
			(yyval.val) = NEW_RESBODY((yyvsp[(2) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
			fixpos((yyval.val), (yyvsp[(2) - (6)].val)?(yyvsp[(2) - (6)].val):(yyvsp[(5) - (6)].val));
#endif
			(yyval.val) = dispatch4(rescue,
				       escape_Qundef((yyvsp[(2) - (6)].val)),
				       escape_Qundef((yyvsp[(3) - (6)].val)),
				       escape_Qundef((yyvsp[(5) - (6)].val)),
				       escape_Qundef((yyvsp[(6) - (6)].val)));

		    ;}
    break;

  case 441:
#line 3766 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 442:
#line 3774 "ripper.y"
    {
#if 0
			if (!((yyval.val) = splat_array((yyvsp[(1) - (1)].val)))) (yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    ;}
    break;

  case 444:
#line 3785 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    ;}
    break;

  case 446:
#line 3792 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch1(ensure, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 449:
#line 3804 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIT(ID2SYM((yyvsp[(1) - (1)].val)));
#endif
			(yyval.val) = dispatch1(symbol_literal, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 451:
#line 3815 "ripper.y"
    {
#if 0
			NODE *node = (yyvsp[(1) - (1)].val);
			if (!node) {
			    node = NEW_STR(STR_NEW0());
			}
			else {
			    node = evstr2dstr(node);
			}
			(yyval.val) = node;
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    ;}
    break;

  case 454:
#line 3834 "ripper.y"
    {
#if 0
			(yyval.val) = literal_concat((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(string_concat, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 455:
#line 3844 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(string_literal, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 456:
#line 3854 "ripper.y"
    {
#if 0
			NODE *node = (yyvsp[(2) - (3)].val);
			if (!node) {
			    node = NEW_XSTR(STR_NEW0());
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
				node = NEW_NODE(NODE_DXSTR, Qnil, 1, NEW_LIST(node));
				break;
			    }
			}
			(yyval.val) = node;
#endif
			(yyval.val) = dispatch1(xstring_literal, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 457:
#line 3881 "ripper.y"
    {
#if 0
			int options = (yyvsp[(3) - (3)].val);
			NODE *node = (yyvsp[(2) - (3)].val);
			NODE *list, *prev;
			if (!node) {
			    node = NEW_LIT(reg_compile(STR_NEW0(), options));
			}
			else switch (nd_type(node)) {
			  case NODE_STR:
			    {
				VALUE src = node->nd_lit;
				nd_set_type(node, NODE_LIT);
				node->nd_lit = reg_compile(src, options);
			    }
			    break;
			  default:
			    node = NEW_NODE(NODE_DSTR, STR_NEW0(), 1, NEW_LIST(node));
			  case NODE_DSTR:
			    if (options & RE_OPTION_ONCE) {
				nd_set_type(node, NODE_DREGX_ONCE);
			    }
			    else {
				nd_set_type(node, NODE_DREGX);
			    }
			    node->nd_cflag = options & RE_OPTION_MASK;
			    if (!NIL_P(node->nd_lit)) reg_fragment_check(node->nd_lit, options);
			    for (list = (prev = node)->nd_next; list; list = list->nd_next) {
				if (nd_type(list->nd_head) == NODE_STR) {
				    VALUE tail = list->nd_head->nd_lit;
				    if (reg_fragment_check(tail, options) && prev && !NIL_P(prev->nd_lit)) {
					VALUE lit = prev == node ? prev->nd_lit : prev->nd_head->nd_lit;
					if (!literal_concat0(parser, lit, tail)) {
					    node = 0;
					    break;
					}
					rb_str_resize(tail, 0);
					prev->nd_next = list->nd_next;
					rb_gc_force_recycle((VALUE)list->nd_head);
					rb_gc_force_recycle((VALUE)list);
					list = prev;
				    }
				    else {
					prev = list;
				    }
                                }
				else {
				    prev = 0;
				}
                            }
			    if (!node->nd_next) {
				VALUE src = node->nd_lit;
				nd_set_type(node, NODE_LIT);
				node->nd_lit = reg_compile(src, options);
			    }
			    break;
			}
			(yyval.val) = node;
#endif
			(yyval.val) = dispatch2(regexp_literal, (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 458:
#line 3946 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ZARRAY();
#endif
			(yyval.val) = dispatch0(words_new);
			(yyval.val) = dispatch1(array, (yyval.val));

		    ;}
    break;

  case 459:
#line 3955 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(array, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 460:
#line 3965 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(words_new);

		    ;}
    break;

  case 461:
#line 3973 "ripper.y"
    {
#if 0
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), evstr2dstr((yyvsp[(2) - (3)].val)));
#endif
			(yyval.val) = dispatch2(words_add, (yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 462:
#line 3985 "ripper.y"
    {
			(yyval.val) = dispatch0(word_new);
			(yyval.val) = dispatch2(word_add, (yyval.val), (yyvsp[(1) - (1)].val));
		    ;}
    break;

  case 463:
#line 3991 "ripper.y"
    {
#if 0
			(yyval.val) = literal_concat((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(word_add, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 464:
#line 4001 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ZARRAY();
#endif
			(yyval.val) = dispatch0(symbols_new);
			(yyval.val) = dispatch1(array, (yyval.val));

		    ;}
    break;

  case 465:
#line 4010 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(array, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 466:
#line 4020 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(symbols_new);

		    ;}
    break;

  case 467:
#line 4028 "ripper.y"
    {
#if 0
			(yyvsp[(2) - (3)].val) = evstr2dstr((yyvsp[(2) - (3)].val));
			nd_set_type((yyvsp[(2) - (3)].val), NODE_DSYM);
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = dispatch2(symbols_add, (yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 468:
#line 4040 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ZARRAY();
#endif
			(yyval.val) = dispatch0(qwords_new);
			(yyval.val) = dispatch1(array, (yyval.val));

		    ;}
    break;

  case 469:
#line 4049 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(array, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 470:
#line 4059 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ZARRAY();
#endif
			(yyval.val) = dispatch0(qsymbols_new);
			(yyval.val) = dispatch1(array, (yyval.val));

		    ;}
    break;

  case 471:
#line 4068 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(array, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 472:
#line 4078 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(qwords_new);

		    ;}
    break;

  case 473:
#line 4086 "ripper.y"
    {
#if 0
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = dispatch2(qwords_add, (yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 474:
#line 4096 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(qsymbols_new);

		    ;}
    break;

  case 475:
#line 4104 "ripper.y"
    {
#if 0
			VALUE lit;
			lit = (yyvsp[(2) - (3)].val)->nd_lit;
			(yyvsp[(2) - (3)].val)->nd_lit = ID2SYM(rb_intern_str(lit));
			nd_set_type((yyvsp[(2) - (3)].val), NODE_LIT);
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = dispatch2(qsymbols_add, (yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 476:
#line 4118 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(string_content);

		    ;}
    break;

  case 477:
#line 4126 "ripper.y"
    {
#if 0
			(yyval.val) = literal_concat((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(string_add, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 478:
#line 4136 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(xstring_new);

		    ;}
    break;

  case 479:
#line 4144 "ripper.y"
    {
#if 0
			(yyval.val) = literal_concat((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(xstring_add, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 480:
#line 4154 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(regexp_new);

		    ;}
    break;

  case 481:
#line 4162 "ripper.y"
    {
#if 0
			NODE *head = (yyvsp[(1) - (2)].val), *tail = (yyvsp[(2) - (2)].val);
			if (!head) {
			    (yyval.val) = tail;
			}
			else if (!tail) {
			    (yyval.val) = head;
			}
			else {
			    switch (nd_type(head)) {
			      case NODE_STR:
				nd_set_type(head, NODE_DSTR);
				break;
			      case NODE_DSTR:
				break;
			      default:
				head = list_append(NEW_DSTR(Qnil), head);
				break;
			    }
			    (yyval.val) = list_append(head, tail);
			}
#endif
			(yyval.val) = dispatch2(regexp_add, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 483:
#line 4192 "ripper.y"
    {
			(yyval.node) = lex_strterm;
			lex_strterm = 0;
			lex_state = EXPR_BEG;
		    ;}
    break;

  case 484:
#line 4198 "ripper.y"
    {
#if 0
			lex_strterm = (yyvsp[(2) - (3)].node);
			(yyval.val) = NEW_EVSTR((yyvsp[(3) - (3)].val));
#endif
			lex_strterm = (yyvsp[(2) - (3)].node);
			(yyval.val) = dispatch1(string_dvar, (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 485:
#line 4208 "ripper.y"
    {
			(yyvsp[(1) - (1)].val) = cond_stack;
			(yyval.val) = cmdarg_stack;
			cond_stack = 0;
			cmdarg_stack = 0;
		    ;}
    break;

  case 486:
#line 4214 "ripper.y"
    {
			(yyval.node) = lex_strterm;
			lex_strterm = 0;
			lex_state = EXPR_BEG;
		    ;}
    break;

  case 487:
#line 4219 "ripper.y"
    {
			(yyval.num) = brace_nest;
			brace_nest = 0;
		    ;}
    break;

  case 488:
#line 4224 "ripper.y"
    {
			cond_stack = (yyvsp[(1) - (6)].val);
			cmdarg_stack = (yyvsp[(2) - (6)].val);
			lex_strterm = (yyvsp[(3) - (6)].node);
			brace_nest = (yyvsp[(4) - (6)].num);
#if 0
			if ((yyvsp[(5) - (6)].val)) (yyvsp[(5) - (6)].val)->flags &= ~NODE_FL_NEWLINE;
			(yyval.val) = new_evstr((yyvsp[(5) - (6)].val));
#endif
			(yyval.val) = dispatch1(string_embexpr, (yyvsp[(5) - (6)].val));

		    ;}
    break;

  case 489:
#line 4239 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_GVAR((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = dispatch1(var_ref, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 490:
#line 4247 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_IVAR((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = dispatch1(var_ref, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 491:
#line 4255 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CVAR((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = dispatch1(var_ref, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 493:
#line 4266 "ripper.y"
    {
			lex_state = EXPR_END;
#if 0
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch1(symbol, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 498:
#line 4283 "ripper.y"
    {
			lex_state = EXPR_END;
#if 0
			(yyval.val) = dsym_node((yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = dispatch1(dyna_symbol, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 501:
#line 4296 "ripper.y"
    {
#if 0
			(yyval.val) = negate_lit((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("-@"), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 502:
#line 4304 "ripper.y"
    {
#if 0
			(yyval.val) = negate_lit((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("-@"), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 508:
#line 4320 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword_nil);;}
    break;

  case 509:
#line 4321 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword_self);;}
    break;

  case 510:
#line 4322 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword_true);;}
    break;

  case 511:
#line 4323 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword_false);;}
    break;

  case 512:
#line 4324 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword__FILE__);;}
    break;

  case 513:
#line 4325 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword__LINE__);;}
    break;

  case 514:
#line 4326 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword__ENCODING__);;}
    break;

  case 515:
#line 4330 "ripper.y"
    {
#if 0
			if (!((yyval.val) = gettable((yyvsp[(1) - (1)].val)))) (yyval.val) = NEW_BEGIN(0);
#endif
			if (id_is_var(get_id((yyvsp[(1) - (1)].val)))) {
			    (yyval.val) = dispatch1(var_ref, (yyvsp[(1) - (1)].val));
			}
			else {
			    (yyval.val) = dispatch1(vcall, (yyvsp[(1) - (1)].val));
			}

		    ;}
    break;

  case 516:
#line 4343 "ripper.y"
    {
#if 0
			if (!((yyval.val) = gettable((yyvsp[(1) - (1)].val)))) (yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(var_ref, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 517:
#line 4353 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
#if 0
#endif
			(yyval.val) = dispatch1(var_field, (yyval.val));

		    ;}
    break;

  case 518:
#line 4361 "ripper.y"
    {
		        (yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
#if 0
#endif
			(yyval.val) = dispatch1(var_field, (yyval.val));

		    ;}
    break;

  case 521:
#line 4375 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = Qnil;

		    ;}
    break;

  case 522:
#line 4383 "ripper.y"
    {
			lex_state = EXPR_BEG;
			command_start = TRUE;
		    ;}
    break;

  case 523:
#line 4388 "ripper.y"
    {
			(yyval.val) = (yyvsp[(3) - (4)].val);
		    ;}
    break;

  case 524:
#line 4392 "ripper.y"
    {
#if 0
			yyerrok;
			(yyval.val) = 0;
#endif
			yyerrok;
			(yyval.val) = Qnil;

		    ;}
    break;

  case 525:
#line 4404 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(paren, (yyvsp[(2) - (3)].val));

			lex_state = EXPR_BEG;
			command_start = TRUE;
		    ;}
    break;

  case 526:
#line 4414 "ripper.y"
    {
			(yyval.val) = (yyvsp[(1) - (2)].val);
			lex_state = EXPR_BEG;
			command_start = TRUE;
		    ;}
    break;

  case 527:
#line 4422 "ripper.y"
    {
			(yyval.val) = new_args_tail((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 528:
#line 4426 "ripper.y"
    {
			(yyval.val) = new_args_tail((yyvsp[(1) - (2)].val), Qnone, (yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 529:
#line 4430 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 530:
#line 4434 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, Qnone, (yyvsp[(1) - (1)].val));
		    ;}
    break;

  case 531:
#line 4440 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    ;}
    break;

  case 532:
#line 4444 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, Qnone, Qnone);
		    ;}
    break;

  case 533:
#line 4450 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), Qnone, (yyvsp[(6) - (6)].val));
		    ;}
    break;

  case 534:
#line 4454 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (8)].val), (yyvsp[(3) - (8)].val), (yyvsp[(5) - (8)].val), (yyvsp[(7) - (8)].val), (yyvsp[(8) - (8)].val));
		    ;}
    break;

  case 535:
#line 4458 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), Qnone, Qnone, (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 536:
#line 4462 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), Qnone, (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    ;}
    break;

  case 537:
#line 4466 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (4)].val), Qnone, (yyvsp[(3) - (4)].val), Qnone, (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 538:
#line 4470 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), Qnone, (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    ;}
    break;

  case 539:
#line 4474 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (2)].val), Qnone, Qnone, Qnone, (yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 540:
#line 4478 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), Qnone, (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 541:
#line 4482 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    ;}
    break;

  case 542:
#line 4486 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (2)].val), Qnone, Qnone, (yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 543:
#line 4490 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (4)].val), Qnone, (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 544:
#line 4494 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, (yyvsp[(1) - (2)].val), Qnone, (yyvsp[(2) - (2)].val));
		    ;}
    break;

  case 545:
#line 4498 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, (yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    ;}
    break;

  case 546:
#line 4502 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, Qnone, Qnone, (yyvsp[(1) - (1)].val));
		    ;}
    break;

  case 547:
#line 4506 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, Qnone, Qnone);
			(yyval.val) = new_args(Qnone, Qnone, Qnone, Qnone, (yyval.val));
		    ;}
    break;

  case 548:
#line 4513 "ripper.y"
    {
#if 0
			yyerror("formal argument cannot be a constant");
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch1(param_error, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 549:
#line 4522 "ripper.y"
    {
#if 0
			yyerror("formal argument cannot be an instance variable");
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch1(param_error, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 550:
#line 4531 "ripper.y"
    {
#if 0
			yyerror("formal argument cannot be a global variable");
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch1(param_error, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 551:
#line 4540 "ripper.y"
    {
#if 0
			yyerror("formal argument cannot be a class variable");
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch1(param_error, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 553:
#line 4552 "ripper.y"
    {
			formal_argument(get_id((yyvsp[(1) - (1)].val)));
			(yyval.val) = (yyvsp[(1) - (1)].val);
		    ;}
    break;

  case 554:
#line 4559 "ripper.y"
    {
			arg_var(get_id((yyvsp[(1) - (1)].val)));
#if 0
			(yyval.val) = NEW_ARGS_AUX((yyvsp[(1) - (1)].val), 1);
#endif
			(yyval.val) = get_value((yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 555:
#line 4568 "ripper.y"
    {
			ID tid = internal_id();
			arg_var(tid);
#if 0
			if (dyna_in_block()) {
			    (yyvsp[(2) - (3)].val)->nd_value = NEW_DVAR(tid);
			}
			else {
			    (yyvsp[(2) - (3)].val)->nd_value = NEW_LVAR(tid);
			}
			(yyval.val) = NEW_ARGS_AUX(tid, 1);
			(yyval.val)->nd_next = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyvsp[(2) - (3)].val));

		    ;}
    break;

  case 556:
#line 4589 "ripper.y"
    {
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));
		    ;}
    break;

  case 557:
#line 4594 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (3)].val);
			(yyval.val)->nd_plen++;
			(yyval.val)->nd_next = block_append((yyval.val)->nd_next, (yyvsp[(3) - (3)].val)->nd_next);
			rb_gc_force_recycle((VALUE)(yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = rb_ary_push((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 558:
#line 4607 "ripper.y"
    {
			arg_var(formal_argument(get_id((yyvsp[(1) - (2)].val))));
			(yyval.val) = assignable((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#if 0
			(yyval.val) = NEW_KW_ARG(0, (yyval.val));
#endif
			(yyval.val) = rb_assoc_new((yyval.val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 559:
#line 4619 "ripper.y"
    {
			arg_var(formal_argument(get_id((yyvsp[(1) - (2)].val))));
			(yyval.val) = assignable((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#if 0
			(yyval.val) = NEW_KW_ARG(0, (yyval.val));
#endif
			(yyval.val) = rb_assoc_new((yyval.val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 560:
#line 4631 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 561:
#line 4639 "ripper.y"
    {
#if 0
			NODE *kws = (yyvsp[(1) - (3)].val);

			while (kws->nd_next) {
			    kws = kws->nd_next;
			}
			kws->nd_next = (yyvsp[(3) - (3)].val);
			(yyval.val) = (yyvsp[(1) - (3)].val);
#endif
			(yyval.val) = rb_ary_push((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 562:
#line 4656 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 563:
#line 4664 "ripper.y"
    {
#if 0
			NODE *kws = (yyvsp[(1) - (3)].val);

			while (kws->nd_next) {
			    kws = kws->nd_next;
			}
			kws->nd_next = (yyvsp[(3) - (3)].val);
			(yyval.val) = (yyvsp[(1) - (3)].val);
#endif
			(yyval.val) = rb_ary_push((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 566:
#line 4684 "ripper.y"
    {
			shadowing_lvar(get_id((yyvsp[(2) - (2)].val)));
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    ;}
    break;

  case 567:
#line 4689 "ripper.y"
    {
			(yyval.val) = internal_id();
		    ;}
    break;

  case 568:
#line 4695 "ripper.y"
    {
			arg_var(formal_argument(get_id((yyvsp[(1) - (3)].val))));
			(yyval.val) = assignable((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#if 0
			(yyval.val) = NEW_OPT_ARG(0, (yyval.val));
#endif
			(yyval.val) = rb_assoc_new((yyval.val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 569:
#line 4707 "ripper.y"
    {
			arg_var(formal_argument(get_id((yyvsp[(1) - (3)].val))));
			(yyval.val) = assignable((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#if 0
			(yyval.val) = NEW_OPT_ARG(0, (yyval.val));
#endif
			(yyval.val) = rb_assoc_new((yyval.val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 570:
#line 4719 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 571:
#line 4727 "ripper.y"
    {
#if 0
			NODE *opts = (yyvsp[(1) - (3)].val);

			while (opts->nd_next) {
			    opts = opts->nd_next;
			}
			opts->nd_next = (yyvsp[(3) - (3)].val);
			(yyval.val) = (yyvsp[(1) - (3)].val);
#endif
			(yyval.val) = rb_ary_push((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 572:
#line 4743 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    ;}
    break;

  case 573:
#line 4751 "ripper.y"
    {
#if 0
			NODE *opts = (yyvsp[(1) - (3)].val);

			while (opts->nd_next) {
			    opts = opts->nd_next;
			}
			opts->nd_next = (yyvsp[(3) - (3)].val);
			(yyval.val) = (yyvsp[(1) - (3)].val);
#endif
			(yyval.val) = rb_ary_push((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 576:
#line 4771 "ripper.y"
    {
#if 0
			if (!is_local_id((yyvsp[(2) - (2)].val)))
			    yyerror("rest argument must be local variable");
#endif
			arg_var(shadowing_lvar(get_id((yyvsp[(2) - (2)].val))));
#if 0
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch1(rest_param, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 577:
#line 4784 "ripper.y"
    {
#if 0
			(yyval.val) = internal_id();
			arg_var((yyval.val));
#endif
			(yyval.val) = dispatch1(rest_param, Qnil);

		    ;}
    break;

  case 580:
#line 4799 "ripper.y"
    {
#if 0
			if (!is_local_id((yyvsp[(2) - (2)].val)))
			    yyerror("block argument must be local variable");
			else if (!dyna_in_block() && local_id((yyvsp[(2) - (2)].val)))
			    yyerror("duplicated block argument name");
#endif
			arg_var(shadowing_lvar(get_id((yyvsp[(2) - (2)].val))));
#if 0
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch1(blockarg, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 581:
#line 4816 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    ;}
    break;

  case 582:
#line 4820 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = Qundef;

		    ;}
    break;

  case 583:
#line 4830 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (1)].val));
			(yyval.val) = (yyvsp[(1) - (1)].val);
		        if (!(yyval.val)) (yyval.val) = NEW_NIL();
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    ;}
    break;

  case 584:
#line 4839 "ripper.y"
    {lex_state = EXPR_BEG;;}
    break;

  case 585:
#line 4840 "ripper.y"
    {
#if 0
			if ((yyvsp[(3) - (4)].val) == 0) {
			    yyerror("can't define singleton method for ().");
			}
			else {
			    switch (nd_type((yyvsp[(3) - (4)].val))) {
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
				value_expr((yyvsp[(3) - (4)].val));
				break;
			    }
			}
			(yyval.val) = (yyvsp[(3) - (4)].val);
#endif
			(yyval.val) = dispatch1(paren, (yyvsp[(3) - (4)].val));

		    ;}
    break;

  case 587:
#line 4870 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (2)].val);
#endif
			(yyval.val) = dispatch1(assoclist_from_args, (yyvsp[(1) - (2)].val));

		    ;}
    break;

  case 588:
#line 4882 "ripper.y"
    {
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));
		    ;}
    break;

  case 589:
#line 4887 "ripper.y"
    {
#if 0
			(yyval.val) = list_concat((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = rb_ary_push((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 590:
#line 4897 "ripper.y"
    {
#if 0
			(yyval.val) = list_append(NEW_LIST((yyvsp[(1) - (3)].val)), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(assoc_new, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    ;}
    break;

  case 591:
#line 4905 "ripper.y"
    {
#if 0
			(yyval.val) = list_append(NEW_LIST(NEW_LIT(ID2SYM((yyvsp[(1) - (2)].val)))), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(assoc_new, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 592:
#line 4913 "ripper.y"
    {
#if 0
			(yyval.val) = list_append(NEW_LIST(0), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(assoc_splat, (yyvsp[(2) - (2)].val));

		    ;}
    break;

  case 603:
#line 4943 "ripper.y"
    { (yyval.val) = (yyvsp[(1) - (1)].val); ;}
    break;

  case 604:
#line 4948 "ripper.y"
    { (yyval.val) = (yyvsp[(1) - (1)].val); ;}
    break;

  case 614:
#line 4971 "ripper.y"
    {yyerrok;;}
    break;

  case 617:
#line 4976 "ripper.y"
    {yyerrok;;}
    break;

  case 618:
#line 4980 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = Qundef;

		    ;}
    break;


/* Line 1267 of yacc.c.  */
#line 10424 "parse.c"
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
      parser_yyerror (parser, YY_("syntax error"));
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
	    parser_yyerror (parser, yymsg);
	  }
	else
	  {
	    parser_yyerror (parser, YY_("syntax error"));
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
		      yytoken, &yylval, parser);
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
		  yystos[yystate], yyvsp, parser);
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
  parser_yyerror (parser, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, parser);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, parser);
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


#line 4988 "ripper.y"

# undef parser
# undef yylex
# undef yylval
# define yylval  (*((YYSTYPE*)(parser->parser_yylval)))

static int parser_regx_options(struct parser_params*);
static int parser_tokadd_string(struct parser_params*,int,int,int,long*,rb_encoding**);
static void parser_tokaddmbc(struct parser_params *parser, int c, rb_encoding *enc);
static int parser_parse_string(struct parser_params*,NODE*);
static int parser_here_document(struct parser_params*,NODE*);


# define nextc()                   parser_nextc(parser)
# define pushback(c)               parser_pushback(parser, (c))
# define newtok()                  parser_newtok(parser)
# define tokspace(n)               parser_tokspace(parser, (n))
# define tokadd(c)                 parser_tokadd(parser, (c))
# define tok_hex(numlen)           parser_tok_hex(parser, (numlen))
# define read_escape(flags,e)      parser_read_escape(parser, (flags), (e))
# define tokadd_escape(e)          parser_tokadd_escape(parser, (e))
# define regx_options()            parser_regx_options(parser)
# define tokadd_string(f,t,p,n,e)  parser_tokadd_string(parser,(f),(t),(p),(n),(e))
# define parse_string(n)           parser_parse_string(parser,(n))
# define tokaddmbc(c, enc)         parser_tokaddmbc(parser, (c), (enc))
# define here_document(n)          parser_here_document(parser,(n))
# define heredoc_identifier()      parser_heredoc_identifier(parser)
# define heredoc_restore(n)        parser_heredoc_restore(parser,(n))
# define whole_match_p(e,l,i)      parser_whole_match_p(parser,(e),(l),(i))

#ifndef RIPPER
# define set_yylval_str(x) (yylval.node = NEW_STR(x))
# define set_yylval_num(x) (yylval.num = (x))
# define set_yylval_id(x)  (yylval.id = (x))
# define set_yylval_name(x)  (yylval.id = (x))
# define set_yylval_literal(x) (yylval.node = NEW_LIT(x))
# define set_yylval_node(x) (yylval.node = (x))
# define yylval_id() (yylval.id)
#else
static inline VALUE
ripper_yylval_id(ID x)
{
    return (VALUE)NEW_LASGN(x, ID2SYM(x));
}
# define set_yylval_str(x) (void)(x)
# define set_yylval_num(x) (void)(x)
# define set_yylval_id(x)  (void)(x)
# define set_yylval_name(x) (void)(yylval.val = ripper_yylval_id(x))
# define set_yylval_literal(x) (void)(x)
# define set_yylval_node(x) (void)(x)
# define yylval_id() yylval.id
#endif

#ifndef RIPPER
#define ripper_flush(p) (void)(p)
#else
#define ripper_flush(p) ((p)->tokp = (p)->parser_lex_p)

#define yylval_rval (*(RB_TYPE_P(yylval.val, T_NODE) ? &yylval.node->nd_rval : &yylval.val))

static int
ripper_has_scan_event(struct parser_params *parser)
{

    if (lex_p < parser->tokp) rb_raise(rb_eRuntimeError, "lex_p < tokp");
    return lex_p > parser->tokp;
}

static VALUE
ripper_scan_event_val(struct parser_params *parser, int t)
{
    VALUE str = STR_NEW(parser->tokp, lex_p - parser->tokp);
    VALUE rval = ripper_dispatch1(parser, ripper_token2eventid(t), str);
    ripper_flush(parser);
    return rval;
}

static void
ripper_dispatch_scan_event(struct parser_params *parser, int t)
{
    if (!ripper_has_scan_event(parser)) return;
    yylval_rval = ripper_scan_event_val(parser, t);
}

static void
ripper_dispatch_ignored_scan_event(struct parser_params *parser, int t)
{
    if (!ripper_has_scan_event(parser)) return;
    (void)ripper_scan_event_val(parser, t);
}

static void
ripper_dispatch_delayed_token(struct parser_params *parser, int t)
{
    int saved_line = ruby_sourceline;
    const char *saved_tokp = parser->tokp;

    ruby_sourceline = parser->delayed_line;
    parser->tokp = lex_pbeg + parser->delayed_col;
    yylval_rval = ripper_dispatch1(parser, ripper_token2eventid(t), parser->delayed);
    parser->delayed = Qnil;
    ruby_sourceline = saved_line;
    parser->tokp = saved_tokp;
}
#endif /* RIPPER */

#include "ruby/regex.h"
#include "ruby/util.h"

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

#define parser_encoding_name()  (current_enc->name)
#define parser_mbclen()  mbclen((lex_p-1),lex_pend,current_enc)
#define parser_precise_mbclen()  rb_enc_precise_mbclen((lex_p-1),lex_pend,current_enc)
#define is_identchar(p,e,enc) (rb_enc_isalnum(*(p),(enc)) || (*(p)) == '_' || !ISASCII(*(p)))
#define parser_is_identchar() (!parser->eofp && is_identchar((lex_p-1),lex_pend,current_enc))

#define parser_isascii() ISASCII(*(lex_p-1))

#ifndef RIPPER
static int
token_info_get_column(struct parser_params *parser, const char *token)
{
    int column = 1;
    const char *p, *pend = lex_p - strlen(token);
    for (p = lex_pbeg; p < pend; p++) {
	if (*p == '\t') {
	    column = (((column - 1) / 8) + 1) * 8;
	}
	column++;
    }
    return column;
}

static int
token_info_has_nonspaces(struct parser_params *parser, const char *token)
{
    const char *p, *pend = lex_p - strlen(token);
    for (p = lex_pbeg; p < pend; p++) {
	if (*p != ' ' && *p != '\t') {
	    return 1;
	}
    }
    return 0;
}

#undef token_info_push
static void
token_info_push(struct parser_params *parser, const char *token)
{
    token_info *ptinfo;

    if (!parser->parser_token_info_enabled) return;
    ptinfo = ALLOC(token_info);
    ptinfo->token = token;
    ptinfo->linenum = ruby_sourceline;
    ptinfo->column = token_info_get_column(parser, token);
    ptinfo->nonspc = token_info_has_nonspaces(parser, token);
    ptinfo->next = parser->parser_token_info;

    parser->parser_token_info = ptinfo;
}

#undef token_info_pop
static void
token_info_pop(struct parser_params *parser, const char *token)
{
    int linenum;
    token_info *ptinfo = parser->parser_token_info;

    if (!ptinfo) return;
    parser->parser_token_info = ptinfo->next;
    if (token_info_get_column(parser, token) == ptinfo->column) { /* OK */
	goto finish;
    }
    linenum = ruby_sourceline;
    if (linenum == ptinfo->linenum) { /* SKIP */
	goto finish;
    }
    if (token_info_has_nonspaces(parser, token) || ptinfo->nonspc) { /* SKIP */
	goto finish;
    }
    if (parser->parser_token_info_enabled) {
	rb_compile_warn(ruby_sourcefile, linenum,
			"mismatched indentations at '%s' with '%s' at %d",
			token, ptinfo->token, ptinfo->linenum);
    }

  finish:
    xfree(ptinfo);
}
#endif	/* RIPPER */

static int
parser_yyerror(struct parser_params *parser, const char *msg)
{
#ifndef RIPPER
    const int max_line_margin = 30;
    const char *p, *pe;
    char *buf;
    long len;
    int i;

    compile_error(PARSER_ARG "%s", msg);
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
	    if (lex_p - p > max_line_margin) {
		p = rb_enc_prev_char(p, lex_p - max_line_margin, pe, rb_enc_get(lex_lastline));
		pre = "...";
	    }
	    if (pe - lex_p > max_line_margin) {
		pe = rb_enc_prev_char(lex_p, lex_p + max_line_margin, pe, rb_enc_get(lex_lastline));
		post = "...";
	    }
	    len = pe - p;
	}
	buf = ALLOCA_N(char, len+2);
	MEMCPY(buf, p, char, len);
	buf[len] = '\0';
	rb_compile_error_append("%s%s%s", pre, buf, post);

	i = (int)(lex_p - p);
	p2 = buf; pe = buf + len;

	while (p2 < pe) {
	    if (*p2 != '\t') *p2 = ' ';
	    p2++;
	}
	buf[i] = '^';
	buf[i+1] = '\0';
	rb_compile_error_append("%s%s", pre, buf);
    }
#else
    dispatch1(parse_error, STR_NEW2(msg));
#endif /* !RIPPER */
    return 0;
}

static void parser_prepare(struct parser_params *parser);

#ifndef RIPPER
static VALUE
debug_lines(const char *f)
{
    ID script_lines;
    CONST_ID(script_lines, "SCRIPT_LINES__");
    if (rb_const_defined_at(rb_cObject, script_lines)) {
	VALUE hash = rb_const_get_at(rb_cObject, script_lines);
	if (RB_TYPE_P(hash, T_HASH)) {
	    VALUE fname = rb_external_str_new_with_enc(f, strlen(f), rb_filesystem_encoding());
	    VALUE lines = rb_ary_new();
	    rb_hash_aset(hash, fname, lines);
	    return lines;
	}
    }
    return 0;
}

static VALUE
coverage(const char *f, int n)
{
    VALUE coverages = rb_get_coverages();
    if (RTEST(coverages) && RBASIC(coverages)->klass == 0) {
	VALUE fname = rb_external_str_new_with_enc(f, strlen(f), rb_filesystem_encoding());
	VALUE lines = rb_ary_new2(n);
	int i;
	RBASIC(lines)->klass = 0;
	for (i = 0; i < n; i++) RARRAY_PTR(lines)[i] = Qnil;
	RARRAY(lines)->as.heap.len = n;
	rb_hash_aset(coverages, fname, lines);
	return lines;
    }
    return 0;
}

static int
e_option_supplied(struct parser_params *parser)
{
    return strcmp(ruby_sourcefile, "-e") == 0;
}

static VALUE
yycompile0(VALUE arg)
{
    int n;
    NODE *tree;
    struct parser_params *parser = (struct parser_params *)arg;

    if (!compile_for_eval && rb_safe_level() == 0) {
	ruby_debug_lines = debug_lines(ruby_sourcefile);
	if (ruby_debug_lines && ruby_sourceline > 0) {
	    VALUE str = STR_NEW0();
	    n = ruby_sourceline;
	    do {
		rb_ary_push(ruby_debug_lines, str);
	    } while (--n);
	}

	if (!e_option_supplied(parser)) {
	    ruby_coverage = coverage(ruby_sourcefile, ruby_sourceline);
	}
    }

    parser_prepare(parser);
    deferred_nodes = 0;
#ifndef RIPPER
    parser->parser_token_info_enabled = !compile_for_eval && RTEST(ruby_verbose);
#endif
#ifndef RIPPER
    if (RUBY_DTRACE_PARSE_BEGIN_ENABLED()) {
	RUBY_DTRACE_PARSE_BEGIN(parser->parser_ruby_sourcefile,
				parser->parser_ruby_sourceline);
    }
#endif
    n = yyparse((void*)parser);
#ifndef RIPPER
    if (RUBY_DTRACE_PARSE_END_ENABLED()) {
	RUBY_DTRACE_PARSE_END(parser->parser_ruby_sourcefile,
			      parser->parser_ruby_sourceline);
    }
#endif
    ruby_debug_lines = 0;
    ruby_coverage = 0;
    compile_for_eval = 0;

    lex_strterm = 0;
    lex_p = lex_pbeg = lex_pend = 0;
    lex_lastline = lex_nextline = 0;
    if (parser->nerr) {
	return 0;
    }
    tree = ruby_eval_tree;
    if (!tree) {
	tree = NEW_NIL();
    }
    else if (ruby_eval_tree_begin) {
	tree->nd_body = NEW_PRELUDE(ruby_eval_tree_begin, tree->nd_body);
    }
    return (VALUE)tree;
}

static NODE*
yycompile(struct parser_params *parser, const char *f, int line)
{
    ruby_sourcefile = ruby_strdup(f);
    ruby_sourceline = line - 1;
    return (NODE *)rb_suppress_tracing(yycompile0, (VALUE)parser);
}
#endif /* !RIPPER */

static rb_encoding *
must_be_ascii_compatible(VALUE s)
{
    rb_encoding *enc = rb_enc_get(s);
    if (!rb_enc_asciicompat(enc)) {
	rb_raise(rb_eArgError, "invalid source encoding");
    }
    return enc;
}

static VALUE
lex_get_str(struct parser_params *parser, VALUE s)
{
    char *beg, *end, *pend;
    rb_encoding *enc = must_be_ascii_compatible(s);

    beg = RSTRING_PTR(s);
    if (lex_gets_ptr) {
	if (RSTRING_LEN(s) == lex_gets_ptr) return Qnil;
	beg += lex_gets_ptr;
    }
    pend = RSTRING_PTR(s) + RSTRING_LEN(s);
    end = beg;
    while (end < pend) {
	if (*end++ == '\n') break;
    }
    lex_gets_ptr = end - RSTRING_PTR(s);
    return rb_enc_str_new(beg, end - beg, enc);
}

static VALUE
lex_getline(struct parser_params *parser)
{
    VALUE line = (*parser->parser_lex_gets)(parser, parser->parser_lex_input);
    if (NIL_P(line)) return line;
    must_be_ascii_compatible(line);
#ifndef RIPPER
    if (ruby_debug_lines) {
	rb_enc_associate(line, current_enc);
	rb_ary_push(ruby_debug_lines, line);
    }
    if (ruby_coverage) {
	rb_ary_push(ruby_coverage, Qnil);
    }
#endif
    return line;
}

#ifdef RIPPER
static rb_data_type_t parser_data_type;
#else
static const rb_data_type_t parser_data_type;

static NODE*
parser_compile_string(volatile VALUE vparser, const char *f, VALUE s, int line)
{
    struct parser_params *parser;
    NODE *node;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, parser);
    lex_gets = lex_get_str;
    lex_gets_ptr = 0;
    lex_input = s;
    lex_pbeg = lex_p = lex_pend = 0;
    compile_for_eval = rb_parse_in_eval();

    node = yycompile(parser, f, line);
    RB_GC_GUARD(vparser); /* prohibit tail call optimization */

    return node;
}

NODE*
rb_compile_string(const char *f, VALUE s, int line)
{
    must_be_ascii_compatible(s);
    return parser_compile_string(rb_parser_new(), f, s, line);
}

NODE*
rb_parser_compile_string(volatile VALUE vparser, const char *f, VALUE s, int line)
{
    must_be_ascii_compatible(s);
    return parser_compile_string(vparser, f, s, line);
}

NODE*
rb_compile_cstr(const char *f, const char *s, int len, int line)
{
    VALUE str = rb_str_new(s, len);
    return parser_compile_string(rb_parser_new(), f, str, line);
}

NODE*
rb_parser_compile_cstr(volatile VALUE vparser, const char *f, const char *s, int len, int line)
{
    VALUE str = rb_str_new(s, len);
    return parser_compile_string(vparser, f, str, line);
}

static VALUE
lex_io_gets(struct parser_params *parser, VALUE io)
{
    return rb_io_gets(io);
}

NODE*
rb_compile_file(const char *f, VALUE file, int start)
{
    VALUE volatile vparser = rb_parser_new();

    return rb_parser_compile_file(vparser, f, file, start);
}

NODE*
rb_parser_compile_file(volatile VALUE vparser, const char *f, VALUE file, int start)
{
    struct parser_params *parser;
    NODE *node;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, parser);
    lex_gets = lex_io_gets;
    lex_input = file;
    lex_pbeg = lex_p = lex_pend = 0;
    compile_for_eval = rb_parse_in_eval();

    node = yycompile(parser, f, start);
    RB_GC_GUARD(vparser); /* prohibit tail call optimization */

    return node;
}
#endif  /* !RIPPER */

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
    str_dsym   = (STR_FUNC_SYMBOL|STR_FUNC_EXPAND)
};

static VALUE
parser_str_new(const char *p, long n, rb_encoding *enc, int func, rb_encoding *enc0)
{
    VALUE str;

    str = rb_enc_str_new(p, n, enc);
    if (!(func & STR_FUNC_REGEXP) && rb_enc_asciicompat(enc)) {
	if (rb_enc_str_coderange(str) == ENC_CODERANGE_7BIT) {
	}
	else if (enc0 == rb_usascii_encoding() && enc != rb_utf8_encoding()) {
	    rb_enc_associate(str, rb_ascii8bit_encoding());
	}
    }

    return str;
}

#define lex_goto_eol(parser) ((parser)->parser_lex_p = (parser)->parser_lex_pend)
#define lex_eol_p() (lex_p >= lex_pend)
#define peek(c) peek_n((c), 0)
#define peek_n(c,n) (lex_p+(n) < lex_pend && (c) == (unsigned char)lex_p[n])

static inline int
parser_nextc(struct parser_params *parser)
{
    int c;

    if (lex_p == lex_pend) {
	VALUE v = lex_nextline;
	lex_nextline = 0;
	if (!v) {
	    if (parser->eofp)
		return -1;

	    if (!lex_input || NIL_P(v = lex_getline(parser))) {
		parser->eofp = Qtrue;
		lex_goto_eol(parser);
		return -1;
	    }
	}
	{
#ifdef RIPPER
	    if (parser->tokp < lex_pend) {
		if (NIL_P(parser->delayed)) {
		    parser->delayed = rb_str_buf_new(1024);
		    rb_enc_associate(parser->delayed, current_enc);
		    rb_str_buf_cat(parser->delayed,
				   parser->tokp, lex_pend - parser->tokp);
		    parser->delayed_line = ruby_sourceline;
		    parser->delayed_col = (int)(parser->tokp - lex_pbeg);
		}
		else {
		    rb_str_buf_cat(parser->delayed,
				   parser->tokp, lex_pend - parser->tokp);
		}
	    }
#endif
	    if (heredoc_end > 0) {
		ruby_sourceline = heredoc_end;
		heredoc_end = 0;
	    }
	    ruby_sourceline++;
	    parser->line_count++;
	    lex_pbeg = lex_p = RSTRING_PTR(v);
	    lex_pend = lex_p + RSTRING_LEN(v);
	    ripper_flush(parser);
	    lex_lastline = v;
	}
    }
    c = (unsigned char)*lex_p++;
    if (c == '\r' && peek('\n')) {
	lex_p++;
	c = '\n';
    }

    return c;
}

static void
parser_pushback(struct parser_params *parser, int c)
{
    if (c == -1) return;
    lex_p--;
    if (lex_p > lex_pbeg && lex_p[0] == '\n' && lex_p[-1] == '\r') {
	lex_p--;
    }
}

#define was_bol() (lex_p == lex_pbeg + 1)

#define tokfix() (tokenbuf[tokidx]='\0')
#define tok() tokenbuf
#define toklen() tokidx
#define toklast() (tokidx>0?tokenbuf[tokidx-1]:0)

static char*
parser_newtok(struct parser_params *parser)
{
    tokidx = 0;
    tokline = ruby_sourceline;
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

static char *
parser_tokspace(struct parser_params *parser, int n)
{
    tokidx += n;

    if (tokidx >= toksiz) {
	do {toksiz *= 2;} while (toksiz < tokidx);
	REALLOC_N(tokenbuf, char, toksiz);
    }
    return &tokenbuf[tokidx-n];
}

static void
parser_tokadd(struct parser_params *parser, int c)
{
    tokenbuf[tokidx++] = (char)c;
    if (tokidx >= toksiz) {
	toksiz *= 2;
	REALLOC_N(tokenbuf, char, toksiz);
    }
}

static int
parser_tok_hex(struct parser_params *parser, size_t *numlen)
{
    int c;

    c = scan_hex(lex_p, 2, numlen);
    if (!*numlen) {
	yyerror("invalid hex escape");
	return 0;
    }
    lex_p += *numlen;
    return c;
}

#define tokcopy(n) memcpy(tokspace(n), lex_p - (n), (n))

/* return value is for ?\u3042 */
static int
parser_tokadd_utf8(struct parser_params *parser, rb_encoding **encp,
                   int string_literal, int symbol_literal, int regexp_literal)
{
    /*
     * If string_literal is true, then we allow multiple codepoints
     * in \u{}, and add the codepoints to the current token.
     * Otherwise we're parsing a character literal and return a single
     * codepoint without adding it
     */

    int codepoint;
    size_t numlen;

    if (regexp_literal) { tokadd('\\'); tokadd('u'); }

    if (peek('{')) {  /* handle \u{...} form */
	do {
            if (regexp_literal) { tokadd(*lex_p); }
	    nextc();
	    codepoint = scan_hex(lex_p, 6, &numlen);
	    if (numlen == 0)  {
		yyerror("invalid Unicode escape");
		return 0;
	    }
	    if (codepoint > 0x10ffff) {
		yyerror("invalid Unicode codepoint (too large)");
		return 0;
	    }
	    lex_p += numlen;
            if (regexp_literal) {
                tokcopy((int)numlen);
            }
            else if (codepoint >= 0x80) {
		*encp = rb_utf8_encoding();
		if (string_literal) tokaddmbc(codepoint, *encp);
	    }
	    else if (string_literal) {
		tokadd(codepoint);
	    }
	} while (string_literal && (peek(' ') || peek('\t')));

	if (!peek('}')) {
	    yyerror("unterminated Unicode escape");
	    return 0;
	}

        if (regexp_literal) { tokadd('}'); }
	nextc();
    }
    else {			/* handle \uxxxx form */
	codepoint = scan_hex(lex_p, 4, &numlen);
	if (numlen < 4) {
	    yyerror("invalid Unicode escape");
	    return 0;
	}
	lex_p += 4;
        if (regexp_literal) {
            tokcopy(4);
        }
	else if (codepoint >= 0x80) {
	    *encp = rb_utf8_encoding();
	    if (string_literal) tokaddmbc(codepoint, *encp);
	}
	else if (string_literal) {
	    tokadd(codepoint);
	}
    }

    return codepoint;
}

#define ESCAPE_CONTROL 1
#define ESCAPE_META    2

static int
parser_read_escape(struct parser_params *parser, int flags,
		   rb_encoding **encp)
{
    int c;
    size_t numlen;

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
	pushback(c);
	c = scan_oct(lex_p, 3, &numlen);
	lex_p += numlen;
	return c;

      case 'x':	/* hex constant */
	c = tok_hex(&numlen);
	if (numlen == 0) return 0;
	return c;

      case 'b':	/* backspace */
	return '\010';

      case 's':	/* space */
	return ' ';

      case 'M':
	if (flags & ESCAPE_META) goto eof;
	if ((c = nextc()) != '-') {
	    pushback(c);
	    goto eof;
	}
	if ((c = nextc()) == '\\') {
	    if (peek('u')) goto eof;
	    return read_escape(flags|ESCAPE_META, encp) | 0x80;
	}
	else if (c == -1 || !ISASCII(c)) goto eof;
	else {
	    return ((c & 0xff) | 0x80);
	}

      case 'C':
	if ((c = nextc()) != '-') {
	    pushback(c);
	    goto eof;
	}
      case 'c':
	if (flags & ESCAPE_CONTROL) goto eof;
	if ((c = nextc())== '\\') {
	    if (peek('u')) goto eof;
	    c = read_escape(flags|ESCAPE_CONTROL, encp);
	}
	else if (c == '?')
	    return 0177;
	else if (c == -1 || !ISASCII(c)) goto eof;
	return c & 0x9f;

      eof:
      case -1:
        yyerror("Invalid escape character syntax");
	return '\0';

      default:
	return c;
    }
}

static void
parser_tokaddmbc(struct parser_params *parser, int c, rb_encoding *enc)
{
    int len = rb_enc_codelen(c, enc);
    rb_enc_mbcput(c, tokspace(len), enc);
}

static int
parser_tokadd_escape(struct parser_params *parser, rb_encoding **encp)
{
    int c;
    int flags = 0;
    size_t numlen;

  first:
    switch (c = nextc()) {
      case '\n':
	return 0;		/* just ignore */

      case '0': case '1': case '2': case '3': /* octal constant */
      case '4': case '5': case '6': case '7':
	{
	    ruby_scan_oct(--lex_p, 3, &numlen);
	    if (numlen == 0) goto eof;
	    lex_p += numlen;
	    tokcopy((int)numlen + 1);
	}
	return 0;

      case 'x':	/* hex constant */
	{
	    tok_hex(&numlen);
	    if (numlen == 0) return -1;
	    tokcopy((int)numlen + 2);
	}
	return 0;

      case 'M':
	if (flags & ESCAPE_META) goto eof;
	if ((c = nextc()) != '-') {
	    pushback(c);
	    goto eof;
	}
	tokcopy(3);
	flags |= ESCAPE_META;
	goto escaped;

      case 'C':
	if (flags & ESCAPE_CONTROL) goto eof;
	if ((c = nextc()) != '-') {
	    pushback(c);
	    goto eof;
	}
	tokcopy(3);
	goto escaped;

      case 'c':
	if (flags & ESCAPE_CONTROL) goto eof;
	tokcopy(2);
	flags |= ESCAPE_CONTROL;
      escaped:
	if ((c = nextc()) == '\\') {
	    goto first;
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
parser_regx_options(struct parser_params *parser)
{
    int kcode = 0;
    int kopt = 0;
    int options = 0;
    int c, opt, kc;

    newtok();
    while (c = nextc(), ISALPHA(c)) {
        if (c == 'o') {
            options |= RE_OPTION_ONCE;
        }
        else if (rb_char_to_option_kcode(c, &opt, &kc)) {
	    if (kc >= 0) {
		if (kc != rb_ascii8bit_encindex()) kcode = c;
		kopt = opt;
	    }
	    else {
		options |= opt;
	    }
        }
        else {
	    tokadd(c);
        }
    }
    options |= kopt;
    pushback(c);
    if (toklen()) {
	tokfix();
	compile_error(PARSER_ARG "unknown regexp option%s - %s",
		      toklen() > 1 ? "s" : "", tok());
    }
    return options | RE_OPTION_ENCODING(kcode);
}

static void
dispose_string(VALUE str)
{
    rb_str_free(str);
    rb_gc_force_recycle(str);
}

static int
parser_tokadd_mbchar(struct parser_params *parser, int c)
{
    int len = parser_precise_mbclen();
    if (!MBCLEN_CHARFOUND_P(len)) {
	compile_error(PARSER_ARG "invalid multibyte char (%s)", parser_encoding_name());
	return -1;
    }
    tokadd(c);
    lex_p += --len;
    if (len > 0) tokcopy(len);
    return c;
}

#define tokadd_mbchar(c) parser_tokadd_mbchar(parser, (c))

static inline int
simple_re_meta(int c)
{
    switch (c) {
      case '$': case '*': case '+': case '.':
      case '?': case '^': case '|':
      case ')': case ']': case '}': case '>':
	return TRUE;
      default:
	return FALSE;
    }
}

static int
parser_tokadd_string(struct parser_params *parser,
		     int func, int term, int paren, long *nest,
		     rb_encoding **encp)
{
    int c;
    int has_nonascii = 0;
    rb_encoding *enc = *encp;
    char *errbuf = 0;
    static const char mixed_msg[] = "%s mixed within %s source";

#define mixed_error(enc1, enc2) if (!errbuf) {	\
	size_t len = sizeof(mixed_msg) - 4;	\
	len += strlen(rb_enc_name(enc1));	\
	len += strlen(rb_enc_name(enc2));	\
	errbuf = ALLOCA_N(char, len);		\
	snprintf(errbuf, len, mixed_msg,	\
		 rb_enc_name(enc1),		\
		 rb_enc_name(enc2));		\
	yyerror(errbuf);			\
    }
#define mixed_escape(beg, enc1, enc2) do {	\
	const char *pos = lex_p;		\
	lex_p = (beg);				\
	mixed_error((enc1), (enc2));		\
	lex_p = pos;				\
    } while (0)

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
	    const char *beg = lex_p - 1;
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

	      case 'u':
		if ((func & STR_FUNC_EXPAND) == 0) {
		    tokadd('\\');
		    break;
		}
		parser_tokadd_utf8(parser, &enc, 1,
				   func & STR_FUNC_SYMBOL,
                                   func & STR_FUNC_REGEXP);
		if (has_nonascii && enc != *encp) {
		    mixed_escape(beg, enc, *encp);
		}
		continue;

	      default:
		if (c == -1) return -1;
		if (!ISASCII(c)) {
		    if ((func & STR_FUNC_EXPAND) == 0) tokadd('\\');
		    goto non_ascii;
		}
		if (func & STR_FUNC_REGEXP) {
		    if (c == term && !simple_re_meta(c)) {
			tokadd(c);
			continue;
		    }
		    pushback(c);
		    if ((c = tokadd_escape(&enc)) < 0)
			return -1;
		    if (has_nonascii && enc != *encp) {
			mixed_escape(beg, enc, *encp);
		    }
		    continue;
		}
		else if (func & STR_FUNC_EXPAND) {
		    pushback(c);
		    if (func & STR_FUNC_ESCAPE) tokadd('\\');
		    c = read_escape(0, &enc);
		}
		else if ((func & STR_FUNC_QWORDS) && ISSPACE(c)) {
		    /* ignore backslashed spaces in %w */
		}
		else if (c != term && !(paren && c == paren)) {
		    tokadd('\\');
		    pushback(c);
		    continue;
		}
	    }
	}
	else if (!parser_isascii()) {
	  non_ascii:
	    has_nonascii = 1;
	    if (enc != *encp) {
		mixed_error(enc, *encp);
		continue;
	    }
	    if (tokadd_mbchar(c) == -1) return -1;
	    continue;
	}
	else if ((func & STR_FUNC_QWORDS) && ISSPACE(c)) {
	    pushback(c);
	    break;
	}
        if (c & 0x80) {
	    has_nonascii = 1;
	    if (enc != *encp) {
		mixed_error(enc, *encp);
		continue;
	    }
        }
	tokadd(c);
    }
    *encp = enc;
    return c;
}

#define NEW_STRTERM(func, term, paren) \
	rb_node_newnode(NODE_STRTERM, (func), (term) | ((paren) << (CHAR_BIT * 2)), 0)

#ifdef RIPPER
static void
ripper_flush_string_content(struct parser_params *parser, rb_encoding *enc)
{
    if (!NIL_P(parser->delayed)) {
	ptrdiff_t len = lex_p - parser->tokp;
	if (len > 0) {
	    rb_enc_str_buf_cat(parser->delayed, parser->tokp, len, enc);
	}
	ripper_dispatch_delayed_token(parser, tSTRING_CONTENT);
	parser->tokp = lex_p;
    }
}

#define flush_string_content(enc) ripper_flush_string_content(parser, (enc))
#else
#define flush_string_content(enc) ((void)(enc))
#endif

RUBY_FUNC_EXPORTED const unsigned int ruby_global_name_punct_bits[(0x7e - 0x20 + 31) / 32];
/* this can be shared with ripper, since it's independent from struct
 * parser_params. */
#ifndef RIPPER
#define BIT(c, idx) (((c) / 32 - 1 == idx) ? (1U << ((c) % 32)) : 0)
#define SPECIAL_PUNCT(idx) ( \
	BIT('~', idx) | BIT('*', idx) | BIT('$', idx) | BIT('?', idx) | \
	BIT('!', idx) | BIT('@', idx) | BIT('/', idx) | BIT('\\', idx) | \
	BIT(';', idx) | BIT(',', idx) | BIT('.', idx) | BIT('=', idx) | \
	BIT(':', idx) | BIT('<', idx) | BIT('>', idx) | BIT('\"', idx) | \
	BIT('&', idx) | BIT('`', idx) | BIT('\'', idx) | BIT('+', idx) | \
	BIT('0', idx))
const unsigned int ruby_global_name_punct_bits[] = {
    SPECIAL_PUNCT(0),
    SPECIAL_PUNCT(1),
    SPECIAL_PUNCT(2),
};
#undef BIT
#undef SPECIAL_PUNCT
#endif

static inline int
is_global_name_punct(const char c)
{
    if (c <= 0x20 || 0x7e < c) return 0;
    return (ruby_global_name_punct_bits[(c - 0x20) / 32] >> (c % 32)) & 1;
}

static int
parser_peek_variable_name(struct parser_params *parser)
{
    int c;
    const char *p = lex_p;

    if (p + 1 >= lex_pend) return 0;
    c = *p++;
    switch (c) {
      case '$':
	if ((c = *p) == '-') {
	    if (++p >= lex_pend) return 0;
	    c = *p;
	}
	else if (is_global_name_punct(c) || ISDIGIT(c)) {
	    return tSTRING_DVAR;
	}
	break;
      case '@':
	if ((c = *p) == '@') {
	    if (++p >= lex_pend) return 0;
	    c = *p;
	}
	break;
      case '{':
	lex_p = p;
	command_start = TRUE;
	return tSTRING_DBEG;
      default:
	return 0;
    }
    if (!ISASCII(c) || c == '_' || ISALPHA(c))
	return tSTRING_DVAR;
    return 0;
}

static int
parser_parse_string(struct parser_params *parser, NODE *quote)
{
    int func = (int)quote->nd_func;
    int term = nd_term(quote);
    int paren = nd_paren(quote);
    int c, space = 0;
    rb_encoding *enc = current_enc;

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
        set_yylval_num(regx_options());
	return tREGEXP_END;
    }
    if (space) {
	pushback(c);
	return ' ';
    }
    newtok();
    if ((func & STR_FUNC_EXPAND) && c == '#') {
	int t = parser_peek_variable_name(parser);
	if (t) return t;
	tokadd('#');
	c = nextc();
    }
    pushback(c);
    if (tokadd_string(func, term, paren, &quote->nd_nest,
		      &enc) == -1) {
	ruby_sourceline = nd_line(quote);
	if (func & STR_FUNC_REGEXP) {
	    if (parser->eofp)
		compile_error(PARSER_ARG "unterminated regexp meets end of file");
	    return tREGEXP_END;
	}
	else {
	    if (parser->eofp)
		compile_error(PARSER_ARG "unterminated string meets end of file");
	    return tSTRING_END;
	}
    }

    tokfix();
    set_yylval_str(STR_NEW3(tok(), toklen(), enc, func));
    flush_string_content(enc);

    return tSTRING_CONTENT;
}

static int
parser_heredoc_identifier(struct parser_params *parser)
{
    int c = nextc(), term, func = 0;
    long len;

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
	    if (tokadd_mbchar(c) == -1) return 0;
	}
	if (c == -1) {
	    compile_error(PARSER_ARG "unterminated here document identifier");
	    return 0;
	}
	break;

      default:
	if (!parser_is_identchar()) {
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
	    if (tokadd_mbchar(c) == -1) return 0;
	} while ((c = nextc()) != -1 && parser_is_identchar());
	pushback(c);
	break;
    }

    tokfix();
#ifdef RIPPER
    ripper_dispatch_scan_event(parser, tHEREDOC_BEG);
#endif
    len = lex_p - lex_pbeg;
    lex_goto_eol(parser);
    lex_strterm = rb_node_newnode(NODE_HEREDOC,
				  STR_NEW(tok(), toklen()),	/* nd_lit */
				  len,				/* nd_nth */
				  lex_lastline);		/* nd_orig */
    nd_set_line(lex_strterm, ruby_sourceline);
    ripper_flush(parser);
    return term == '`' ? tXSTRING_BEG : tSTRING_BEG;
}

static void
parser_heredoc_restore(struct parser_params *parser, NODE *here)
{
    VALUE line;

    line = here->nd_orig;
    lex_lastline = line;
    lex_pbeg = RSTRING_PTR(line);
    lex_pend = lex_pbeg + RSTRING_LEN(line);
    lex_p = lex_pbeg + here->nd_nth;
    heredoc_end = ruby_sourceline;
    ruby_sourceline = nd_line(here);
    dispose_string(here->nd_lit);
    rb_gc_force_recycle((VALUE)here);
    ripper_flush(parser);
}

static int
parser_whole_match_p(struct parser_params *parser,
    const char *eos, long len, int indent)
{
    const char *p = lex_pbeg;
    long n;

    if (indent) {
	while (*p && ISSPACE(*p)) p++;
    }
    n = lex_pend - (p + len);
    if (n < 0 || (n > 0 && p[len] != '\n' && p[len] != '\r')) return FALSE;
    return strncmp(eos, p, len) == 0;
}

#ifdef RIPPER
static void
ripper_dispatch_heredoc_end(struct parser_params *parser)
{
    if (!NIL_P(parser->delayed))
	ripper_dispatch_delayed_token(parser, tSTRING_CONTENT);
    lex_goto_eol(parser);
    ripper_dispatch_ignored_scan_event(parser, tHEREDOC_END);
}

#define dispatch_heredoc_end() ripper_dispatch_heredoc_end(parser)
#else
#define dispatch_heredoc_end() ((void)0)
#endif

static int
parser_here_document(struct parser_params *parser, NODE *here)
{
    int c, func, indent = 0;
    const char *eos, *p, *pend;
    long len;
    VALUE str = 0;
    rb_encoding *enc = current_enc;

    eos = RSTRING_PTR(here->nd_lit);
    len = RSTRING_LEN(here->nd_lit) - 1;
    indent = (func = *eos++) & STR_FUNC_INDENT;

    if ((c = nextc()) == -1) {
      error:
	compile_error(PARSER_ARG "can't find string \"%s\" anywhere before EOF", eos);
#ifdef RIPPER
	if (NIL_P(parser->delayed)) {
	    ripper_dispatch_scan_event(parser, tSTRING_CONTENT);
	}
	else {
	    if (str ||
		((len = lex_p - parser->tokp) > 0 &&
		 (str = STR_NEW3(parser->tokp, len, enc, func), 1))) {
		rb_str_append(parser->delayed, str);
	    }
	    ripper_dispatch_delayed_token(parser, tSTRING_CONTENT);
	}
	lex_goto_eol(parser);
#endif
      restore:
	heredoc_restore(lex_strterm);
	lex_strterm = 0;
	return 0;
    }
    if (was_bol() && whole_match_p(eos, len, indent)) {
	dispatch_heredoc_end();
	heredoc_restore(lex_strterm);
	return tSTRING_END;
    }

    if (!(func & STR_FUNC_EXPAND)) {
	do {
	    p = RSTRING_PTR(lex_lastline);
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
		str = STR_NEW(p, pend - p);
	    if (pend < lex_pend) rb_str_cat(str, "\n", 1);
	    lex_goto_eol(parser);
	    if (nextc() == -1) {
		if (str) dispose_string(str);
		goto error;
	    }
	} while (!whole_match_p(eos, len, indent));
    }
    else {
	/*	int mb = ENC_CODERANGE_7BIT, *mbp = &mb;*/
	newtok();
	if (c == '#') {
	    int t = parser_peek_variable_name(parser);
	    if (t) return t;
	    tokadd('#');
	    c = nextc();
	}
	do {
	    pushback(c);
	    if ((c = tokadd_string(func, '\n', 0, NULL, &enc)) == -1) {
		if (parser->eofp) goto error;
		goto restore;
	    }
	    if (c != '\n') {
		set_yylval_str(STR_NEW3(tok(), toklen(), enc, func));
		flush_string_content(enc);
		return tSTRING_CONTENT;
	    }
	    tokadd(nextc());
	    /*	    if (mbp && mb == ENC_CODERANGE_UNKNOWN) mbp = 0;*/
	    if ((c = nextc()) == -1) goto error;
	} while (!whole_match_p(eos, len, indent));
	str = STR_NEW3(tok(), toklen(), enc, func);
    }
    dispatch_heredoc_end();
    heredoc_restore(lex_strterm);
    lex_strterm = NEW_STRTERM(-1, 0, 0);
    set_yylval_str(str);
    return tSTRING_CONTENT;
}

#include "lex.c"

static void
arg_ambiguous_gen(struct parser_params *parser)
{
#ifndef RIPPER
    rb_warning0("ambiguous first argument; put parentheses or even spaces");
#else
    dispatch0(arg_ambiguous);
#endif
}
#define arg_ambiguous() (arg_ambiguous_gen(parser), 1)

static ID
formal_argument_gen(struct parser_params *parser, ID lhs)
{
#ifndef RIPPER
    if (!is_local_id(lhs))
	yyerror("formal argument must be local variable");
#endif
    shadowing_lvar(lhs);
    return lhs;
}

static int
lvar_defined_gen(struct parser_params *parser, ID id)
{
    return (dyna_in_block() && dvar_defined_get(id)) || local_id(id);
}

/* emacsen -*- hack */
static long
parser_encode_length(struct parser_params *parser, const char *name, long len)
{
    long nlen;

    if (len > 5 && name[nlen = len - 5] == '-') {
	if (rb_memcicmp(name + nlen + 1, "unix", 4) == 0)
	    return nlen;
    }
    if (len > 4 && name[nlen = len - 4] == '-') {
	if (rb_memcicmp(name + nlen + 1, "dos", 3) == 0)
	    return nlen;
	if (rb_memcicmp(name + nlen + 1, "mac", 3) == 0 &&
	    !(len == 8 && rb_memcicmp(name, "utf8-mac", len) == 0))
	    /* exclude UTF8-MAC because the encoding named "UTF8" doesn't exist in Ruby */
	    return nlen;
    }
    return len;
}

static void
parser_set_encode(struct parser_params *parser, const char *name)
{
    int idx = rb_enc_find_index(name);
    rb_encoding *enc;
    VALUE excargs[3];

    if (idx < 0) {
	excargs[1] = rb_sprintf("unknown encoding name: %s", name);
      error:
	excargs[0] = rb_eArgError;
	excargs[2] = rb_make_backtrace();
	rb_ary_unshift(excargs[2], rb_sprintf("%s:%d", ruby_sourcefile, ruby_sourceline));
	rb_exc_raise(rb_make_exception(3, excargs));
    }
    enc = rb_enc_from_index(idx);
    if (!rb_enc_asciicompat(enc)) {
	excargs[1] = rb_sprintf("%s is not ASCII compatible", rb_enc_name(enc));
	goto error;
    }
    parser->enc = enc;
#ifndef RIPPER
    if (ruby_debug_lines) {
	long i, n = RARRAY_LEN(ruby_debug_lines);
	const VALUE *p = RARRAY_PTR(ruby_debug_lines);
	for (i = 0; i < n; ++i) {
	    rb_enc_associate_index(*p, idx);
	}
    }
#endif
}

static int
comment_at_top(struct parser_params *parser)
{
    const char *p = lex_pbeg, *pend = lex_p - 1;
    if (parser->line_count != (parser->has_shebang ? 2 : 1)) return 0;
    while (p < pend) {
	if (!ISSPACE(*p)) return 0;
	p++;
    }
    return 1;
}

#ifndef RIPPER
typedef long (*rb_magic_comment_length_t)(struct parser_params *parser, const char *name, long len);
typedef void (*rb_magic_comment_setter_t)(struct parser_params *parser, const char *name, const char *val);

static void
magic_comment_encoding(struct parser_params *parser, const char *name, const char *val)
{
    if (!comment_at_top(parser)) {
	return;
    }
    parser_set_encode(parser, val);
}

static void
parser_set_token_info(struct parser_params *parser, const char *name, const char *val)
{
    int *p = &parser->parser_token_info_enabled;

    switch (*val) {
      case 't': case 'T':
	if (strcasecmp(val, "true") == 0) {
	    *p = TRUE;
	    return;
	}
	break;
      case 'f': case 'F':
	if (strcasecmp(val, "false") == 0) {
	    *p = FALSE;
	    return;
	}
	break;
    }
    rb_compile_warning(ruby_sourcefile, ruby_sourceline, "invalid value for %s: %s", name, val);
}

struct magic_comment {
    const char *name;
    rb_magic_comment_setter_t func;
    rb_magic_comment_length_t length;
};

static const struct magic_comment magic_comments[] = {
    {"coding", magic_comment_encoding, parser_encode_length},
    {"encoding", magic_comment_encoding, parser_encode_length},
    {"warn_indent", parser_set_token_info},
};
#endif

static const char *
magic_comment_marker(const char *str, long len)
{
    long i = 2;

    while (i < len) {
	switch (str[i]) {
	  case '-':
	    if (str[i-1] == '*' && str[i-2] == '-') {
		return str + i + 1;
	    }
	    i += 2;
	    break;
	  case '*':
	    if (i + 1 >= len) return 0;
	    if (str[i+1] != '-') {
		i += 4;
	    }
	    else if (str[i-1] != '-') {
		i += 2;
	    }
	    else {
		return str + i + 2;
	    }
	    break;
	  default:
	    i += 3;
	    break;
	}
    }
    return 0;
}

static int
parser_magic_comment(struct parser_params *parser, const char *str, long len)
{
    VALUE name = 0, val = 0;
    const char *beg, *end, *vbeg, *vend;
#define str_copy(_s, _p, _n) ((_s) \
	? (void)(rb_str_resize((_s), (_n)), \
	   MEMCPY(RSTRING_PTR(_s), (_p), char, (_n)), (_s)) \
	: (void)((_s) = STR_NEW((_p), (_n))))

    if (len <= 7) return FALSE;
    if (!(beg = magic_comment_marker(str, len))) return FALSE;
    if (!(end = magic_comment_marker(beg, str + len - beg))) return FALSE;
    str = beg;
    len = end - beg - 3;

    /* %r"([^\\s\'\":;]+)\\s*:\\s*(\"(?:\\\\.|[^\"])*\"|[^\"\\s;]+)[\\s;]*" */
    while (len > 0) {
#ifndef RIPPER
	const struct magic_comment *p = magic_comments;
#endif
	char *s;
	int i;
	long n = 0;

	for (; len > 0 && *str; str++, --len) {
	    switch (*str) {
	      case '\'': case '"': case ':': case ';':
		continue;
	    }
	    if (!ISSPACE(*str)) break;
	}
	for (beg = str; len > 0; str++, --len) {
	    switch (*str) {
	      case '\'': case '"': case ':': case ';':
		break;
	      default:
		if (ISSPACE(*str)) break;
		continue;
	    }
	    break;
	}
	for (end = str; len > 0 && ISSPACE(*str); str++, --len);
	if (!len) break;
	if (*str != ':') continue;

	do str++; while (--len > 0 && ISSPACE(*str));
	if (!len) break;
	if (*str == '"') {
	    for (vbeg = ++str; --len > 0 && *str != '"'; str++) {
		if (*str == '\\') {
		    --len;
		    ++str;
		}
	    }
	    vend = str;
	    if (len) {
		--len;
		++str;
	    }
	}
	else {
	    for (vbeg = str; len > 0 && *str != '"' && *str != ';' && !ISSPACE(*str); --len, str++);
	    vend = str;
	}
	while (len > 0 && (*str == ';' || ISSPACE(*str))) --len, str++;

	n = end - beg;
	str_copy(name, beg, n);
	s = RSTRING_PTR(name);
	for (i = 0; i < n; ++i) {
	    if (s[i] == '-') s[i] = '_';
	}
#ifndef RIPPER
	do {
	    if (STRNCASECMP(p->name, s, n) == 0) {
		n = vend - vbeg;
		if (p->length) {
		    n = (*p->length)(parser, vbeg, n);
		}
		str_copy(val, vbeg, n);
		(*p->func)(parser, s, RSTRING_PTR(val));
		break;
	    }
	} while (++p < magic_comments + numberof(magic_comments));
#else
	str_copy(val, vbeg, vend - vbeg);
	dispatch2(magic_comment, name, val);
#endif
    }

    return TRUE;
}

static void
set_file_encoding(struct parser_params *parser, const char *str, const char *send)
{
    int sep = 0;
    const char *beg = str;
    VALUE s;

    for (;;) {
	if (send - str <= 6) return;
	switch (str[6]) {
	  case 'C': case 'c': str += 6; continue;
	  case 'O': case 'o': str += 5; continue;
	  case 'D': case 'd': str += 4; continue;
	  case 'I': case 'i': str += 3; continue;
	  case 'N': case 'n': str += 2; continue;
	  case 'G': case 'g': str += 1; continue;
	  case '=': case ':':
	    sep = 1;
	    str += 6;
	    break;
	  default:
	    str += 6;
	    if (ISSPACE(*str)) break;
	    continue;
	}
	if (STRNCASECMP(str-6, "coding", 6) == 0) break;
    }
    for (;;) {
	do {
	    if (++str >= send) return;
	} while (ISSPACE(*str));
	if (sep) break;
	if (*str != '=' && *str != ':') return;
	sep = 1;
	str++;
    }
    beg = str;
    while ((*str == '-' || *str == '_' || ISALNUM(*str)) && ++str < send);
    s = rb_str_new(beg, parser_encode_length(parser, beg, str - beg));
    parser_set_encode(parser, RSTRING_PTR(s));
    rb_str_resize(s, 0);
}

static void
parser_prepare(struct parser_params *parser)
{
    int c = nextc();
    switch (c) {
      case '#':
	if (peek('!')) parser->has_shebang = 1;
	break;
      case 0xef:		/* UTF-8 BOM marker */
	if (lex_pend - lex_p >= 2 &&
	    (unsigned char)lex_p[0] == 0xbb &&
	    (unsigned char)lex_p[1] == 0xbf) {
	    parser->enc = rb_utf8_encoding();
	    lex_p += 2;
	    lex_pbeg = lex_p;
	    return;
	}
	break;
      case EOF:
	return;
    }
    pushback(c);
    parser->enc = rb_enc_get(lex_lastline);
}

#define IS_ARG() IS_lex_state(EXPR_ARG_ANY)
#define IS_END() IS_lex_state(EXPR_END_ANY)
#define IS_BEG() IS_lex_state(EXPR_BEG_ANY)
#define IS_SPCARG(c) (IS_ARG() && space_seen && !ISSPACE(c))
#define IS_LABEL_POSSIBLE() ((IS_lex_state(EXPR_BEG | EXPR_ENDFN) && !cmd_state) || IS_ARG())
#define IS_LABEL_SUFFIX(n) (peek_n(':',(n)) && !peek_n(':', (n)+1))
#define IS_AFTER_OPERATOR() IS_lex_state(EXPR_FNAME | EXPR_DOT)

#ifndef RIPPER
#define ambiguous_operator(op, syn) ( \
    rb_warning0("`"op"' after local variable is interpreted as binary operator"), \
    rb_warning0("even though it seems like "syn""))
#else
#define ambiguous_operator(op, syn) dispatch2(operator_ambiguous, ripper_intern(op), rb_str_new_cstr(syn))
#endif
#define warn_balanced(op, syn) ((void) \
    (!IS_lex_state_for(last_state, EXPR_CLASS|EXPR_DOT|EXPR_FNAME|EXPR_ENDFN|EXPR_ENDARG) && \
     space_seen && !ISSPACE(c) && \
     (ambiguous_operator(op, syn), 0)))

static int
parser_yylex(struct parser_params *parser)
{
    register int c;
    int space_seen = 0;
    int cmd_state;
    enum lex_state_e last_state;
    rb_encoding *enc;
    int mb;
#ifdef RIPPER
    int fallthru = FALSE;
#endif

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
    command_start = FALSE;
  retry:
    last_state = lex_state;
    switch (c = nextc()) {
      case '\0':		/* NUL */
      case '\004':		/* ^D */
      case '\032':		/* ^Z */
      case -1:			/* end of script. */
	return 0;

	/* white spaces */
      case ' ': case '\t': case '\f': case '\r':
      case '\13': /* '\v' */
	space_seen = 1;
#ifdef RIPPER
	while ((c = nextc())) {
	    switch (c) {
	      case ' ': case '\t': case '\f': case '\r':
	      case '\13': /* '\v' */
		break;
	      default:
		goto outofloop;
	    }
	}
      outofloop:
	pushback(c);
	ripper_dispatch_scan_event(parser, tSP);
#endif
	goto retry;

      case '#':		/* it's a comment */
	/* no magic_comment in shebang line */
	if (!parser_magic_comment(parser, lex_p, lex_pend - lex_p)) {
	    if (comment_at_top(parser)) {
		set_file_encoding(parser, lex_p, lex_pend);
	    }
	}
	lex_p = lex_pend;
#ifdef RIPPER
        ripper_dispatch_scan_event(parser, tCOMMENT);
        fallthru = TRUE;
#endif
	/* fall through */
      case '\n':
	if (IS_lex_state(EXPR_BEG | EXPR_VALUE | EXPR_CLASS | EXPR_FNAME | EXPR_DOT)) {
#ifdef RIPPER
            if (!fallthru) {
                ripper_dispatch_scan_event(parser, tIGNORED_NL);
            }
            fallthru = FALSE;
#endif
	    goto retry;
	}
	while ((c = nextc())) {
	    switch (c) {
	      case ' ': case '\t': case '\f': case '\r':
	      case '\13': /* '\v' */
		space_seen = 1;
		break;
	      case '.': {
		  if ((c = nextc()) != '.') {
		      pushback(c);
		      pushback('.');
		      goto retry;
		  }
	      }
	      default:
		--ruby_sourceline;
		lex_nextline = lex_lastline;
	      case -1:		/* EOF no decrement*/
		lex_goto_eol(parser);
#ifdef RIPPER
		if (c != -1) {
		    parser->tokp = lex_p;
		}
#endif
		goto normal_newline;
	    }
	}
      normal_newline:
	command_start = TRUE;
	lex_state = EXPR_BEG;
	return '\n';

      case '*':
	if ((c = nextc()) == '*') {
	    if ((c = nextc()) == '=') {
                set_yylval_id(tPOW);
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    if (IS_SPCARG(c)) {
		rb_warning0("`**' interpreted as argument prefix");
		c = tDSTAR;
	    }
	    else if (IS_BEG()) {
		c = tDSTAR;
	    }
	    else {
		warn_balanced("**", "argument prefix");
		c = tPOW;
	    }
	}
	else {
	    if (c == '=') {
                set_yylval_id('*');
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    if (IS_SPCARG(c)) {
		rb_warning0("`*' interpreted as argument prefix");
		c = tSTAR;
	    }
	    else if (IS_BEG()) {
		c = tSTAR;
	    }
	    else {
		warn_balanced("*", "argument prefix");
		c = '*';
	    }
	}
	lex_state = IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG;
	return c;

      case '!':
	c = nextc();
	if (IS_AFTER_OPERATOR()) {
	    lex_state = EXPR_ARG;
	    if (c == '@') {
		return '!';
	    }
	}
	else {
	    lex_state = EXPR_BEG;
	}
	if (c == '=') {
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
#ifdef RIPPER
                int first_p = TRUE;

                lex_goto_eol(parser);
                ripper_dispatch_scan_event(parser, tEMBDOC_BEG);
#endif
		for (;;) {
		    lex_goto_eol(parser);
#ifdef RIPPER
                    if (!first_p) {
                        ripper_dispatch_scan_event(parser, tEMBDOC);
                    }
                    first_p = FALSE;
#endif
		    c = nextc();
		    if (c == -1) {
			compile_error(PARSER_ARG "embedded document meets end of file");
			return 0;
		    }
		    if (c != '=') continue;
		    if (strncmp(lex_p, "end", 3) == 0 &&
			(lex_p + 3 == lex_pend || ISSPACE(lex_p[3]))) {
			break;
		    }
		}
		lex_goto_eol(parser);
#ifdef RIPPER
                ripper_dispatch_scan_event(parser, tEMBDOC_END);
#endif
		goto retry;
	    }
	}

	lex_state = IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG;
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
	last_state = lex_state;
	c = nextc();
	if (c == '<' &&
	    !IS_lex_state(EXPR_DOT | EXPR_CLASS) &&
	    !IS_END() &&
	    (!IS_ARG() || space_seen)) {
	    int token = heredoc_identifier();
	    if (token) return token;
	}
	if (IS_AFTER_OPERATOR()) {
	    lex_state = EXPR_ARG;
	}
	else {
	    if (IS_lex_state(EXPR_CLASS))
		command_start = TRUE;
	    lex_state = EXPR_BEG;
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
                set_yylval_id(tLSHFT);
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    warn_balanced("<<", "here document");
	    return tLSHFT;
	}
	pushback(c);
	return '<';

      case '>':
	lex_state = IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG;
	if ((c = nextc()) == '=') {
	    return tGEQ;
	}
	if (c == '>') {
	    if ((c = nextc()) == '=') {
                set_yylval_id(tRSHFT);
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
	if (IS_lex_state(EXPR_FNAME)) {
	    lex_state = EXPR_ENDFN;
	    return c;
	}
	if (IS_lex_state(EXPR_DOT)) {
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
	if (IS_END()) {
	    lex_state = EXPR_VALUE;
	    return '?';
	}
	c = nextc();
	if (c == -1) {
	    compile_error(PARSER_ARG "incomplete character syntax");
	    return 0;
	}
	if (rb_enc_isspace(c, current_enc)) {
	    if (!IS_ARG()) {
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
		    rb_warnI("invalid character syntax; use ?\\%c", c2);
		}
	    }
	  ternary:
	    pushback(c);
	    lex_state = EXPR_VALUE;
	    return '?';
	}
	newtok();
	enc = current_enc;
	if (!parser_isascii()) {
	    if (tokadd_mbchar(c) == -1) return 0;
	}
	else if ((rb_enc_isalnum(c, current_enc) || c == '_') &&
		 lex_p < lex_pend && is_identchar(lex_p, lex_pend, current_enc)) {
	    goto ternary;
	}
        else if (c == '\\') {
            if (peek('u')) {
                nextc();
                c = parser_tokadd_utf8(parser, &enc, 0, 0, 0);
                if (0x80 <= c) {
                    tokaddmbc(c, enc);
                }
                else {
                    tokadd(c);
                }
            }
            else if (!lex_eol_p() && !(c = *lex_p, ISASCII(c))) {
                nextc();
                if (tokadd_mbchar(c) == -1) return 0;
            }
            else {
                c = read_escape(0, &enc);
                tokadd(c);
            }
        }
        else {
	    tokadd(c);
        }
	tokfix();
	set_yylval_str(STR_NEW3(tok(), toklen(), enc, 0));
	lex_state = EXPR_END;
	return tCHAR;

      case '&':
	if ((c = nextc()) == '&') {
	    lex_state = EXPR_BEG;
	    if ((c = nextc()) == '=') {
                set_yylval_id(tANDOP);
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    return tANDOP;
	}
	else if (c == '=') {
            set_yylval_id('&');
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	pushback(c);
	if (IS_SPCARG(c)) {
	    rb_warning0("`&' interpreted as argument prefix");
	    c = tAMPER;
	}
	else if (IS_BEG()) {
	    c = tAMPER;
	}
	else {
	    warn_balanced("&", "argument prefix");
	    c = '&';
	}
	lex_state = IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG;
	return c;

      case '|':
	if ((c = nextc()) == '|') {
	    lex_state = EXPR_BEG;
	    if ((c = nextc()) == '=') {
                set_yylval_id(tOROP);
		lex_state = EXPR_BEG;
		return tOP_ASGN;
	    }
	    pushback(c);
	    return tOROP;
	}
	if (c == '=') {
            set_yylval_id('|');
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	lex_state = IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG;
	pushback(c);
	return '|';

      case '+':
	c = nextc();
	if (IS_AFTER_OPERATOR()) {
	    lex_state = EXPR_ARG;
	    if (c == '@') {
		return tUPLUS;
	    }
	    pushback(c);
	    return '+';
	}
	if (c == '=') {
            set_yylval_id('+');
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	if (IS_BEG() || (IS_SPCARG(c) && arg_ambiguous())) {
	    lex_state = EXPR_BEG;
	    pushback(c);
	    if (c != -1 && ISDIGIT(c)) {
		c = '+';
		goto start_num;
	    }
	    return tUPLUS;
	}
	lex_state = EXPR_BEG;
	pushback(c);
	warn_balanced("+", "unary operator");
	return '+';

      case '-':
	c = nextc();
	if (IS_AFTER_OPERATOR()) {
	    lex_state = EXPR_ARG;
	    if (c == '@') {
		return tUMINUS;
	    }
	    pushback(c);
	    return '-';
	}
	if (c == '=') {
            set_yylval_id('-');
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	if (c == '>') {
	    lex_state = EXPR_ENDFN;
	    return tLAMBDA;
	}
	if (IS_BEG() || (IS_SPCARG(c) && arg_ambiguous())) {
	    lex_state = EXPR_BEG;
	    pushback(c);
	    if (c != -1 && ISDIGIT(c)) {
		return tUMINUS_NUM;
	    }
	    return tUMINUS;
	}
	lex_state = EXPR_BEG;
	pushback(c);
	warn_balanced("-", "unary operator");
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
	if (c != -1 && ISDIGIT(c)) {
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
#define no_digits() do {yyerror("numeric literal without digits"); return 0;} while (0)
		int start = toklen();
		c = nextc();
		if (c == 'x' || c == 'X') {
		    /* hexadecimal */
		    c = nextc();
		    if (c != -1 && ISXDIGIT(c)) {
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
			no_digits();
		    }
		    else if (nondigit) goto trailing_uc;
		    set_yylval_literal(rb_cstr_to_inum(tok(), 16, FALSE));
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
			no_digits();
		    }
		    else if (nondigit) goto trailing_uc;
		    set_yylval_literal(rb_cstr_to_inum(tok(), 2, FALSE));
		    return tINTEGER;
		}
		if (c == 'd' || c == 'D') {
		    /* decimal */
		    c = nextc();
		    if (c != -1 && ISDIGIT(c)) {
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
			no_digits();
		    }
		    else if (nondigit) goto trailing_uc;
		    set_yylval_literal(rb_cstr_to_inum(tok(), 10, FALSE));
		    return tINTEGER;
		}
		if (c == '_') {
		    /* 0_0 */
		    goto octal_number;
		}
		if (c == 'o' || c == 'O') {
		    /* prefixed octal */
		    c = nextc();
		    if (c == -1 || c == '_' || !ISDIGIT(c)) {
			no_digits();
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
			set_yylval_literal(rb_cstr_to_inum(tok(), 8, FALSE));
			return tINTEGER;
		    }
		    if (nondigit) {
			pushback(c);
			goto trailing_uc;
		    }
		}
		if (c > '7' && c <= '9') {
		  invalid_octal:
		    yyerror("Invalid octal digit");
		}
		else if (c == '.' || c == 'e' || c == 'E') {
		    tokadd('0');
		}
		else {
		    pushback(c);
                    set_yylval_literal(INT2FIX(0));
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
			if (c0 == -1 || !ISDIGIT(c0)) {
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
	    if (nondigit) {
		char tmp[30];
	      trailing_uc:
		snprintf(tmp, sizeof(tmp), "trailing `%c' in number", nondigit);
		yyerror(tmp);
	    }
	    tokfix();
	    if (is_float) {
		double d = strtod(tok(), 0);
		if (errno == ERANGE) {
		    rb_warningS("Float %s out of range", tok());
		    errno = 0;
		}
                set_yylval_literal(DBL2NUM(d));
		return tFLOAT;
	    }
	    set_yylval_literal(rb_cstr_to_inum(tok(), 10, FALSE));
	    return tINTEGER;
	}

      case ')':
      case ']':
	paren_nest--;
      case '}':
	COND_LEXPOP();
	CMDARG_LEXPOP();
	if (c == ')')
	    lex_state = EXPR_ENDFN;
	else
	    lex_state = EXPR_ENDARG;
	if (c == '}') {
	    if (!brace_nest--) c = tSTRING_DEND;
	}
	return c;

      case ':':
	c = nextc();
	if (c == ':') {
	    if (IS_BEG() || IS_lex_state(EXPR_CLASS) || IS_SPCARG(-1)) {
		lex_state = EXPR_BEG;
		return tCOLON3;
	    }
	    lex_state = EXPR_DOT;
	    return tCOLON2;
	}
	if (IS_END() || ISSPACE(c)) {
	    pushback(c);
	    warn_balanced(":", "symbol literal");
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
	if (IS_lex_state(EXPR_BEG_ANY)) {
	    lex_strterm = NEW_STRTERM(str_regexp, '/', 0);
	    return tREGEXP_BEG;
	}
	if ((c = nextc()) == '=') {
            set_yylval_id('/');
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	pushback(c);
	if (IS_SPCARG(c)) {
	    (void)arg_ambiguous();
	    lex_strterm = NEW_STRTERM(str_regexp, '/', 0);
	    return tREGEXP_BEG;
	}
	lex_state = IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG;
	warn_balanced("/", "regexp literal");
	return '/';

      case '^':
	if ((c = nextc()) == '=') {
            set_yylval_id('^');
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	lex_state = IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG;
	pushback(c);
	return '^';

      case ';':
	lex_state = EXPR_BEG;
	command_start = TRUE;
	return ';';

      case ',':
	lex_state = EXPR_BEG;
	return ',';

      case '~':
	if (IS_AFTER_OPERATOR()) {
	    if ((c = nextc()) != '@') {
		pushback(c);
	    }
	    lex_state = EXPR_ARG;
	}
	else {
	    lex_state = EXPR_BEG;
	}
	return '~';

      case '(':
	if (IS_BEG()) {
	    c = tLPAREN;
	}
	else if (IS_SPCARG(-1)) {
	    c = tLPAREN_ARG;
	}
	paren_nest++;
	COND_PUSH(0);
	CMDARG_PUSH(0);
	lex_state = EXPR_BEG;
	return c;

      case '[':
	paren_nest++;
	if (IS_AFTER_OPERATOR()) {
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
	++brace_nest;
	if (lpar_beg && lpar_beg == paren_nest) {
	    lex_state = EXPR_BEG;
	    lpar_beg = 0;
	    --paren_nest;
	    COND_PUSH(0);
	    CMDARG_PUSH(0);
	    return tLAMBEG;
	}
	if (IS_ARG() || IS_lex_state(EXPR_END | EXPR_ENDFN))
	    c = '{';          /* block (primary) */
	else if (IS_lex_state(EXPR_ENDARG))
	    c = tLBRACE_ARG;  /* block (expr) */
	else
	    c = tLBRACE;      /* hash */
	COND_PUSH(0);
	CMDARG_PUSH(0);
	lex_state = EXPR_BEG;
	if (c != tLBRACE) command_start = TRUE;
	return c;

      case '\\':
	c = nextc();
	if (c == '\n') {
	    space_seen = 1;
#ifdef RIPPER
	    ripper_dispatch_scan_event(parser, tSP);
#endif
	    goto retry; /* skip \\n */
	}
	pushback(c);
	return '\\';

      case '%':
	if (IS_lex_state(EXPR_BEG_ANY)) {
	    int term;
	    int paren;

	    c = nextc();
	  quotation:
	    if (c == -1 || !ISALNUM(c)) {
		term = c;
		c = 'Q';
	    }
	    else {
		term = nextc();
		if (rb_enc_isalnum(term, current_enc) || !parser_isascii()) {
		    yyerror("unknown type of %string");
		    return 0;
		}
	    }
	    if (c == -1 || term == -1) {
		compile_error(PARSER_ARG "unterminated quoted string meets end of file");
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

	      case 'I':
		lex_strterm = NEW_STRTERM(str_dword, term, paren);
		do {c = nextc();} while (ISSPACE(c));
		pushback(c);
		return tSYMBOLS_BEG;

	      case 'i':
		lex_strterm = NEW_STRTERM(str_sword, term, paren);
		do {c = nextc();} while (ISSPACE(c));
		pushback(c);
		return tQSYMBOLS_BEG;

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
            set_yylval_id('%');
	    lex_state = EXPR_BEG;
	    return tOP_ASGN;
	}
	if (IS_SPCARG(c)) {
	    goto quotation;
	}
	lex_state = IS_AFTER_OPERATOR() ? EXPR_ARG : EXPR_BEG;
	pushback(c);
	warn_balanced("%%", "string literal");
	return '%';

      case '$':
	lex_state = EXPR_END;
	newtok();
	c = nextc();
	switch (c) {
	  case '_':		/* $_: last read line string */
	    c = nextc();
	    if (parser_is_identchar()) {
		tokadd('$');
		tokadd('_');
		break;
	    }
	    pushback(c);
	    c = '_';
	    /* fall through */
	  case '~':		/* $~: match-data */
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
	    set_yylval_name(rb_intern(tok()));
	    return tGVAR;

	  case '-':
	    tokadd('$');
	    tokadd(c);
	    c = nextc();
	    if (parser_is_identchar()) {
		if (tokadd_mbchar(c) == -1) return 0;
	    }
	    else {
		pushback(c);
	    }
	  gvar:
	    tokfix();
	    set_yylval_name(rb_intern(tok()));
	    return tGVAR;

	  case '&':		/* $&: last match */
	  case '`':		/* $`: string before last match */
	  case '\'':		/* $': string after last match */
	  case '+':		/* $+: string matches last paren. */
	    if (IS_lex_state_for(last_state, EXPR_FNAME)) {
		tokadd('$');
		tokadd(c);
		goto gvar;
	    }
	    set_yylval_node(NEW_BACK_REF(c));
	    return tBACK_REF;

	  case '1': case '2': case '3':
	  case '4': case '5': case '6':
	  case '7': case '8': case '9':
	    tokadd('$');
	    do {
		tokadd(c);
		c = nextc();
	    } while (c != -1 && ISDIGIT(c));
	    pushback(c);
	    if (IS_lex_state_for(last_state, EXPR_FNAME)) goto gvar;
	    tokfix();
	    set_yylval_node(NEW_NTH_REF(atoi(tok()+1)));
	    return tNTH_REF;

	  default:
	    if (!parser_is_identchar()) {
		pushback(c);
		compile_error(PARSER_ARG "`$%c' is not allowed as a global variable name", c);
		return 0;
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
	if (c != -1 && (ISDIGIT(c) || !parser_is_identchar())) {
	    pushback(c);
	    if (tokidx == 1) {
		compile_error(PARSER_ARG "`@%c' is not allowed as an instance variable name", c);
	    }
	    else {
		compile_error(PARSER_ARG "`@@%c' is not allowed as a class variable name", c);
	    }
	    return 0;
	}
	break;

      case '_':
	if (was_bol() && whole_match_p("__END__", 7, 0)) {
	    ruby__end__seen = 1;
	    parser->eofp = Qtrue;
#ifndef RIPPER
	    return -1;
#else
            lex_goto_eol(parser);
            ripper_dispatch_scan_event(parser, k__END__);
            return 0;
#endif
	}
	newtok();
	break;

      default:
	if (!parser_is_identchar()) {
	    rb_compile_error(PARSER_ARG  "Invalid char `\\x%02X' in expression", c);
	    goto retry;
	}

	newtok();
	break;
    }

    mb = ENC_CODERANGE_7BIT;
    do {
	if (!ISASCII(c)) mb = ENC_CODERANGE_UNKNOWN;
	if (tokadd_mbchar(c) == -1) return 0;
	c = nextc();
    } while (parser_is_identchar());
    switch (tok()[0]) {
      case '@': case '$':
	pushback(c);
	break;
      default:
	if ((c == '!' || c == '?') && !peek('=')) {
	    tokadd(c);
	}
	else {
	    pushback(c);
	}
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
		if (IS_lex_state(EXPR_FNAME)) {
		    if ((c = nextc()) == '=' && !peek('~') && !peek('>') &&
			(!peek('=') || (peek_n('>', 1)))) {
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

	    if (IS_LABEL_POSSIBLE()) {
		if (IS_LABEL_SUFFIX(0)) {
		    lex_state = EXPR_BEG;
		    nextc();
		    set_yylval_name(TOK_INTERN(!ENC_SINGLE(mb)));
		    return tLABEL;
		}
	    }
	    if (mb == ENC_CODERANGE_7BIT && !IS_lex_state(EXPR_DOT)) {
		const struct kwtable *kw;

		/* See if it is a reserved word.  */
		kw = rb_reserved_word(tok(), toklen());
		if (kw) {
		    enum lex_state_e state = lex_state;
		    lex_state = kw->state;
		    if (state == EXPR_FNAME) {
			set_yylval_name(rb_intern(kw->name));
			return kw->id[0];
		    }
		    if (lex_state == EXPR_BEG) {
			command_start = TRUE;
		    }
		    if (kw->id[0] == keyword_do) {
			if (lpar_beg && lpar_beg == paren_nest) {
			    lpar_beg = 0;
			    --paren_nest;
			    return keyword_do_LAMBDA;
			}
			if (COND_P()) return keyword_do_cond;
			if (CMDARG_P() && state != EXPR_CMDARG)
			    return keyword_do_block;
			if (state & (EXPR_BEG | EXPR_ENDARG))
			    return keyword_do_block;
			return keyword_do;
		    }
		    if (state & (EXPR_BEG | EXPR_VALUE))
			return kw->id[0];
		    else {
			if (kw->id[0] != kw->id[1])
			    lex_state = EXPR_BEG;
			return kw->id[1];
		    }
		}
	    }

	    if (IS_lex_state(EXPR_BEG_ANY | EXPR_ARG_ANY | EXPR_DOT)) {
		if (cmd_state) {
		    lex_state = EXPR_CMDARG;
		}
		else {
		    lex_state = EXPR_ARG;
		}
	    }
	    else if (lex_state == EXPR_FNAME) {
		lex_state = EXPR_ENDFN;
	    }
	    else {
		lex_state = EXPR_END;
	    }
	}
        {
            ID ident = TOK_INTERN(!ENC_SINGLE(mb));

            set_yylval_name(ident);
            if (!IS_lex_state_for(last_state, EXPR_DOT|EXPR_FNAME) &&
		is_local_id(ident) && lvar_defined(ident)) {
                lex_state = EXPR_END;
            }
        }
	return result;
    }
}

#if YYPURE
static int
yylex(void *lval, void *p)
#else
yylex(void *p)
#endif
{
    struct parser_params *parser = (struct parser_params*)p;
    int t;

#if YYPURE
    parser->parser_yylval = lval;
    parser->parser_yylval->val = Qundef;
#endif
    t = parser_yylex(parser);
#ifdef RIPPER
    if (!NIL_P(parser->delayed)) {
	ripper_dispatch_delayed_token(parser, t);
	return t;
    }
    if (t != 0)
	ripper_dispatch_scan_event(parser, t);
#endif

    return t;
}

#ifndef RIPPER
static NODE*
node_newnode(struct parser_params *parser, enum node_type type, VALUE a0, VALUE a1, VALUE a2)
{
    NODE *n = (rb_node_newnode)(type, a0, a1, a2);
    nd_set_line(n, ruby_sourceline);
    return n;
}

static enum node_type
nodetype(NODE *node)			/* for debug */
{
    return (enum node_type)nd_type(node);
}

static int
nodeline(NODE *node)
{
    return nd_line(node);
}

static NODE*
newline_node(NODE *node)
{
    if (node) {
	node = remove_begin(node);
	node->flags |= NODE_FL_NEWLINE;
    }
    return node;
}

static void
fixpos(NODE *node, NODE *orig)
{
    if (!node) return;
    if (!orig) return;
    if (orig == (NODE*)1) return;
    nd_set_line(node, nd_line(orig));
}

static void
parser_warning(struct parser_params *parser, NODE *node, const char *mesg)
{
    rb_compile_warning(ruby_sourcefile, nd_line(node), "%s", mesg);
}
#define parser_warning(node, mesg) parser_warning(parser, (node), (mesg))

static void
parser_warn(struct parser_params *parser, NODE *node, const char *mesg)
{
    rb_compile_warn(ruby_sourcefile, nd_line(node), "%s", mesg);
}
#define parser_warn(node, mesg) parser_warn(parser, (node), (mesg))

static NODE*
block_append_gen(struct parser_params *parser, NODE *head, NODE *tail)
{
    NODE *end, *h = head, *nd;

    if (tail == 0) return head;

    if (h == 0) return tail;
    switch (nd_type(h)) {
      case NODE_LIT:
      case NODE_STR:
      case NODE_SELF:
      case NODE_TRUE:
      case NODE_FALSE:
      case NODE_NIL:
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

    nd = end->nd_head;
    switch (nd_type(nd)) {
      case NODE_RETURN:
      case NODE_BREAK:
      case NODE_NEXT:
      case NODE_REDO:
      case NODE_RETRY:
	if (RTEST(ruby_verbose)) {
	    parser_warning(tail, "statement not reached");
	}
	break;

      default:
	break;
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
list_append_gen(struct parser_params *parser, NODE *list, NODE *item)
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
list_concat_gen(struct parser_params *parser, NODE *head, NODE *tail)
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

static int
literal_concat0(struct parser_params *parser, VALUE head, VALUE tail)
{
    if (NIL_P(tail)) return 1;
    if (!rb_enc_compatible(head, tail)) {
	compile_error(PARSER_ARG "string literal encodings differ (%s / %s)",
		      rb_enc_name(rb_enc_get(head)),
		      rb_enc_name(rb_enc_get(tail)));
	rb_str_resize(head, 0);
	rb_str_resize(tail, 0);
	return 0;
    }
    rb_str_buf_append(head, tail);
    return 1;
}

/* concat two string literals */
static NODE *
literal_concat_gen(struct parser_params *parser, NODE *head, NODE *tail)
{
    enum node_type htype;
    NODE *headlast;
    VALUE lit;

    if (!head) return tail;
    if (!tail) return head;

    htype = nd_type(head);
    if (htype == NODE_EVSTR) {
	NODE *node = NEW_DSTR(Qnil);
	head = list_append(node, head);
	htype = NODE_DSTR;
    }
    switch (nd_type(tail)) {
      case NODE_STR:
	if (htype == NODE_DSTR && (headlast = head->nd_next->nd_end->nd_head) &&
	    nd_type(headlast) == NODE_STR) {
	    htype = NODE_STR;
	    lit = headlast->nd_lit;
	}
	else {
	    lit = head->nd_lit;
	}
	if (htype == NODE_STR) {
	    if (!literal_concat0(parser, lit, tail->nd_lit)) {
	      error:
		rb_gc_force_recycle((VALUE)head);
		rb_gc_force_recycle((VALUE)tail);
		return 0;
	    }
	    rb_gc_force_recycle((VALUE)tail);
	}
	else {
	    list_append(head, tail);
	}
	break;

      case NODE_DSTR:
	if (htype == NODE_STR) {
	    if (!literal_concat0(parser, head->nd_lit, tail->nd_lit))
		goto error;
	    tail->nd_lit = head->nd_lit;
	    rb_gc_force_recycle((VALUE)head);
	    head = tail;
	}
	else if (NIL_P(tail->nd_lit)) {
	  append:
	    head->nd_alen += tail->nd_alen - 1;
	    head->nd_next->nd_end->nd_next = tail->nd_next;
	    head->nd_next->nd_end = tail->nd_next->nd_end;
	    rb_gc_force_recycle((VALUE)tail);
	}
	else if (htype == NODE_DSTR && (headlast = head->nd_next->nd_end->nd_head) &&
		 nd_type(headlast) == NODE_STR) {
	    lit = headlast->nd_lit;
	    if (!literal_concat0(parser, lit, tail->nd_lit))
		goto error;
	    tail->nd_lit = Qnil;
	    goto append;
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
evstr2dstr_gen(struct parser_params *parser, NODE *node)
{
    if (nd_type(node) == NODE_EVSTR) {
	node = list_append(NEW_DSTR(Qnil), node);
    }
    return node;
}

static NODE *
new_evstr_gen(struct parser_params *parser, NODE *node)
{
    NODE *head = node;

    if (node) {
	switch (nd_type(node)) {
	  case NODE_STR: case NODE_DSTR: case NODE_EVSTR:
	    return node;
	}
    }
    return NEW_EVSTR(head);
}

static NODE *
call_bin_op_gen(struct parser_params *parser, NODE *recv, ID id, NODE *arg1)
{
    value_expr(recv);
    value_expr(arg1);
    return NEW_CALL(recv, id, NEW_LIST(arg1));
}

static NODE *
call_uni_op_gen(struct parser_params *parser, NODE *recv, ID id)
{
    value_expr(recv);
    return NEW_CALL(recv, id, 0);
}

static NODE*
match_op_gen(struct parser_params *parser, NODE *node1, NODE *node2)
{
    value_expr(node1);
    value_expr(node2);
    if (node1) {
	switch (nd_type(node1)) {
	  case NODE_DREGX:
	  case NODE_DREGX_ONCE:
	    return NEW_MATCH2(node1, node2);

	  case NODE_LIT:
	    if (RB_TYPE_P(node1->nd_lit, T_REGEXP)) {
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
	    if (RB_TYPE_P(node2->nd_lit, T_REGEXP)) {
		return NEW_MATCH3(node2, node1);
	    }
	}
    }

    return NEW_CALL(node1, tMATCH, NEW_LIST(node2));
}

static NODE*
gettable_gen(struct parser_params *parser, ID id)
{
    switch (id) {
      case keyword_self:
	return NEW_SELF();
      case keyword_nil:
	return NEW_NIL();
      case keyword_true:
	return NEW_TRUE();
      case keyword_false:
	return NEW_FALSE();
      case keyword__FILE__:
	return NEW_STR(rb_external_str_new_with_enc(ruby_sourcefile, strlen(ruby_sourcefile),
						    rb_filesystem_encoding()));
      case keyword__LINE__:
	return NEW_LIT(INT2FIX(tokline));
      case keyword__ENCODING__:
	return NEW_LIT(rb_enc_from_encoding(current_enc));
    }
    switch (id_type(id)) {
      case ID_LOCAL:
	if (dyna_in_block() && dvar_defined(id)) return NEW_DVAR(id);
	if (local_id(id)) return NEW_LVAR(id);
	/* method call without arguments */
	return NEW_VCALL(id);
      case ID_GLOBAL:
	return NEW_GVAR(id);
      case ID_INSTANCE:
	return NEW_IVAR(id);
      case ID_CONST:
	return NEW_CONST(id);
      case ID_CLASS:
	return NEW_CVAR(id);
    }
    compile_error(PARSER_ARG "identifier %s is not valid to get", rb_id2name(id));
    return 0;
}
#else  /* !RIPPER */
static int
id_is_var_gen(struct parser_params *parser, ID id)
{
    if (is_notop_id(id)) {
	switch (id & ID_SCOPE_MASK) {
	  case ID_GLOBAL: case ID_INSTANCE: case ID_CONST: case ID_CLASS:
	    return 1;
	  case ID_LOCAL:
	    if (dyna_in_block() && dvar_defined(id)) return 1;
	    if (local_id(id)) return 1;
	    /* method call without arguments */
	    return 0;
	}
    }
    compile_error(PARSER_ARG "identifier %s is not valid to get", rb_id2name(id));
    return 0;
}
#endif /* !RIPPER */

#if PARSER_DEBUG
static const char *
lex_state_name(enum lex_state_e state)
{
    static const char names[][12] = {
	"EXPR_BEG",    "EXPR_END",    "EXPR_ENDARG", "EXPR_ENDFN",  "EXPR_ARG",
	"EXPR_CMDARG", "EXPR_MID",    "EXPR_FNAME",  "EXPR_DOT",    "EXPR_CLASS",
	"EXPR_VALUE",
    };

    if ((unsigned)state & ~(~0u << EXPR_MAX_STATE))
	return names[ffs(state)];
    return NULL;
}
#endif

#ifdef RIPPER
static VALUE
assignable_gen(struct parser_params *parser, VALUE lhs)
#else
static NODE*
assignable_gen(struct parser_params *parser, ID id, NODE *val)
#endif
{
#ifdef RIPPER
    ID id = get_id(lhs);
# define assignable_result(x) get_value(lhs)
# define parser_yyerror(parser, x) dispatch1(assign_error, lhs)
#else
# define assignable_result(x) (x)
#endif
    if (!id) return assignable_result(0);
    switch (id) {
      case keyword_self:
	yyerror("Can't change the value of self");
	goto error;
      case keyword_nil:
	yyerror("Can't assign to nil");
	goto error;
      case keyword_true:
	yyerror("Can't assign to true");
	goto error;
      case keyword_false:
	yyerror("Can't assign to false");
	goto error;
      case keyword__FILE__:
	yyerror("Can't assign to __FILE__");
	goto error;
      case keyword__LINE__:
	yyerror("Can't assign to __LINE__");
	goto error;
      case keyword__ENCODING__:
	yyerror("Can't assign to __ENCODING__");
	goto error;
    }
    switch (id_type(id)) {
      case ID_LOCAL:
	if (dyna_in_block()) {
	    if (dvar_curr(id)) {
		return assignable_result(NEW_DASGN_CURR(id, val));
	    }
	    else if (dvar_defined(id)) {
		return assignable_result(NEW_DASGN(id, val));
	    }
	    else if (local_id(id)) {
		return assignable_result(NEW_LASGN(id, val));
	    }
	    else {
		dyna_var(id);
		return assignable_result(NEW_DASGN_CURR(id, val));
	    }
	}
	else {
	    if (!local_id(id)) {
		local_var(id);
	    }
	    return assignable_result(NEW_LASGN(id, val));
	}
	break;
      case ID_GLOBAL:
	return assignable_result(NEW_GASGN(id, val));
      case ID_INSTANCE:
	return assignable_result(NEW_IASGN(id, val));
      case ID_CONST:
	if (!in_def && !in_single)
	    return assignable_result(NEW_CDECL(id, val, 0));
	yyerror("dynamic constant assignment");
	break;
      case ID_CLASS:
	return assignable_result(NEW_CVASGN(id, val));
      default:
	compile_error(PARSER_ARG "identifier %s is not valid to set", rb_id2name(id));
    }
  error:
    return assignable_result(0);
#undef assignable_result
#undef parser_yyerror
}

static int
is_private_local_id(ID name)
{
    VALUE s;
    if (name == idUScore) return 1;
    if (!is_local_id(name)) return 0;
    s = rb_id2str(name);
    if (!s) return 0;
    return RSTRING_PTR(s)[0] == '_';
}

#define LVAR_USED ((ID)1 << (sizeof(ID) * CHAR_BIT - 1))

static ID
shadowing_lvar_gen(struct parser_params *parser, ID name)
{
    if (is_private_local_id(name)) return name;
    if (dyna_in_block()) {
	if (dvar_curr(name)) {
	    yyerror("duplicated argument name");
	}
	else if (dvar_defined_get(name) || local_id(name)) {
	    rb_warningS("shadowing outer local variable - %s", rb_id2name(name));
	    vtable_add(lvtbl->vars, name);
	    if (lvtbl->used) {
		vtable_add(lvtbl->used, (ID)ruby_sourceline | LVAR_USED);
	    }
	}
    }
    else {
	if (local_id(name)) {
	    yyerror("duplicated argument name");
	}
    }
    return name;
}

static void
new_bv_gen(struct parser_params *parser, ID name)
{
    if (!name) return;
    if (!is_local_id(name)) {
	compile_error(PARSER_ARG "invalid local variable - %s",
		      rb_id2name(name));
	return;
    }
    shadowing_lvar(name);
    dyna_var(name);
}

#ifndef RIPPER
static NODE *
aryset_gen(struct parser_params *parser, NODE *recv, NODE *idx)
{
    if (recv && nd_type(recv) == NODE_SELF)
	recv = (NODE *)1;
    return NEW_ATTRASGN(recv, tASET, idx);
}

static void
block_dup_check_gen(struct parser_params *parser, NODE *node1, NODE *node2)
{
    if (node2 && node1 && nd_type(node1) == NODE_BLOCK_PASS) {
	compile_error(PARSER_ARG "both block arg and actual block given");
    }
}

ID
rb_id_attrset(ID id)
{
    id &= ~ID_SCOPE_MASK;
    id |= ID_ATTRSET;
    return id;
}

static NODE *
attrset_gen(struct parser_params *parser, NODE *recv, ID id)
{
    if (recv && nd_type(recv) == NODE_SELF)
	recv = (NODE *)1;
    return NEW_ATTRASGN(recv, rb_id_attrset(id), 0);
}

static void
rb_backref_error_gen(struct parser_params *parser, NODE *node)
{
    switch (nd_type(node)) {
      case NODE_NTH_REF:
	compile_error(PARSER_ARG "Can't set variable $%ld", node->nd_nth);
	break;
      case NODE_BACK_REF:
	compile_error(PARSER_ARG "Can't set variable $%c", (int)node->nd_nth);
	break;
    }
}

static NODE *
arg_concat_gen(struct parser_params *parser, NODE *node1, NODE *node2)
{
    if (!node2) return node1;
    switch (nd_type(node1)) {
      case NODE_BLOCK_PASS:
	if (node1->nd_head)
	    node1->nd_head = arg_concat(node1->nd_head, node2);
	else
	    node1->nd_head = NEW_LIST(node2);
	return node1;
      case NODE_ARGSPUSH:
	if (nd_type(node2) != NODE_ARRAY) break;
	node1->nd_body = list_concat(NEW_LIST(node1->nd_body), node2);
	nd_set_type(node1, NODE_ARGSCAT);
	return node1;
      case NODE_ARGSCAT:
	if (nd_type(node2) != NODE_ARRAY ||
	    nd_type(node1->nd_body) != NODE_ARRAY) break;
	node1->nd_body = list_concat(node1->nd_body, node2);
	return node1;
    }
    return NEW_ARGSCAT(node1, node2);
}

static NODE *
arg_append_gen(struct parser_params *parser, NODE *node1, NODE *node2)
{
    if (!node1) return NEW_LIST(node2);
    switch (nd_type(node1))  {
      case NODE_ARRAY:
	return list_append(node1, node2);
      case NODE_BLOCK_PASS:
	node1->nd_head = arg_append(node1->nd_head, node2);
	return node1;
      case NODE_ARGSPUSH:
	node1->nd_body = list_append(NEW_LIST(node1->nd_body), node2);
	nd_set_type(node1, NODE_ARGSCAT);
	return node1;
    }
    return NEW_ARGSPUSH(node1, node2);
}

static NODE *
splat_array(NODE* node)
{
    if (nd_type(node) == NODE_SPLAT) node = node->nd_head;
    if (nd_type(node) == NODE_ARRAY) return node;
    return 0;
}

static NODE *
node_assign_gen(struct parser_params *parser, NODE *lhs, NODE *rhs)
{
    if (!lhs) return 0;

    switch (nd_type(lhs)) {
      case NODE_GASGN:
      case NODE_IASGN:
      case NODE_IASGN2:
      case NODE_LASGN:
      case NODE_DASGN:
      case NODE_DASGN_CURR:
      case NODE_MASGN:
      case NODE_CDECL:
      case NODE_CVASGN:
	lhs->nd_value = rhs;
	break;

      case NODE_ATTRASGN:
      case NODE_CALL:
	lhs->nd_args = arg_append(lhs->nd_args, rhs);
	break;

      default:
	/* should not happen */
	break;
    }

    return lhs;
}

static int
value_expr_gen(struct parser_params *parser, NODE *node)
{
    int cond = 0;

    if (!node) {
	rb_warning0("empty expression");
    }
    while (node) {
	switch (nd_type(node)) {
	  case NODE_DEFN:
	  case NODE_DEFS:
	    parser_warning(node, "void value expression");
	    return FALSE;

	  case NODE_RETURN:
	  case NODE_BREAK:
	  case NODE_NEXT:
	  case NODE_REDO:
	  case NODE_RETRY:
	    if (!cond) yyerror("void value expression");
	    /* or "control never reach"? */
	    return FALSE;

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
	    if (!node->nd_body) {
		node = node->nd_else;
		break;
	    }
	    else if (!node->nd_else) {
		node = node->nd_body;
		break;
	    }
	    if (!value_expr(node->nd_body)) return FALSE;
	    node = node->nd_else;
	    break;

	  case NODE_AND:
	  case NODE_OR:
	    cond = 1;
	    node = node->nd_2nd;
	    break;

	  default:
	    return TRUE;
	}
    }

    return TRUE;
}

static void
void_expr_gen(struct parser_params *parser, NODE *node)
{
    const char *useless = 0;

    if (!RTEST(ruby_verbose)) return;

    if (!node) return;
    switch (nd_type(node)) {
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
	rb_warnS("possibly useless use of %s in void context", useless);
	ruby_sourceline = line;
    }
}

static void
void_stmts_gen(struct parser_params *parser, NODE *node)
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
remove_begin(NODE *node)
{
    NODE **n = &node, *n1 = node;
    while (n1 && nd_type(n1) == NODE_BEGIN && n1->nd_body) {
	*n = n1 = n1->nd_body;
    }
    return node;
}

static void
reduce_nodes_gen(struct parser_params *parser, NODE **body)
{
    NODE *node = *body;

    if (!node) {
	*body = NEW_NIL();
	return;
    }
#define subnodes(n1, n2) \
    ((!node->n1) ? (node->n2 ? (body = &node->n2, 1) : 0) : \
     (!node->n2) ? (body = &node->n1, 1) : \
     (reduce_nodes(&node->n1), body = &node->n2, 1))

    while (node) {
	int newline = (int)(node->flags & NODE_FL_NEWLINE);
	switch (nd_type(node)) {
	  end:
	  case NODE_NIL:
	    *body = 0;
	    return;
	  case NODE_RETURN:
	    *body = node = node->nd_stts;
	    if (newline && node) node->flags |= NODE_FL_NEWLINE;
	    continue;
	  case NODE_BEGIN:
	    *body = node = node->nd_body;
	    if (newline && node) node->flags |= NODE_FL_NEWLINE;
	    continue;
	  case NODE_BLOCK:
	    body = &node->nd_end->nd_head;
	    break;
	  case NODE_IF:
	    if (subnodes(nd_body, nd_else)) break;
	    return;
	  case NODE_CASE:
	    body = &node->nd_body;
	    break;
	  case NODE_WHEN:
	    if (!subnodes(nd_body, nd_next)) goto end;
	    break;
	  case NODE_ENSURE:
	    if (!subnodes(nd_head, nd_resq)) goto end;
	    break;
	  case NODE_RESCUE:
	    if (node->nd_else) {
		body = &node->nd_resq;
		break;
	    }
	    if (!subnodes(nd_head, nd_resq)) goto end;
	    break;
	  default:
	    return;
	}
	node = *body;
	if (newline && node) node->flags |= NODE_FL_NEWLINE;
    }

#undef subnodes
}

static int
is_static_content(NODE *node)
{
    if (!node) return 1;
    switch (nd_type(node)) {
      case NODE_HASH:
	if (!(node = node->nd_head)) break;
      case NODE_ARRAY:
	do {
	    if (!is_static_content(node->nd_head)) return 0;
	} while ((node = node->nd_next) != 0);
      case NODE_LIT:
      case NODE_STR:
      case NODE_NIL:
      case NODE_TRUE:
      case NODE_FALSE:
      case NODE_ZARRAY:
	break;
      default:
	return 0;
    }
    return 1;
}

static int
assign_in_cond(struct parser_params *parser, NODE *node)
{
    switch (nd_type(node)) {
      case NODE_MASGN:
	yyerror("multiple assignment in conditional");
	return 1;

      case NODE_LASGN:
      case NODE_DASGN:
      case NODE_DASGN_CURR:
      case NODE_GASGN:
      case NODE_IASGN:
	break;

      default:
	return 0;
    }

    if (!node->nd_value) return 1;
    if (is_static_content(node->nd_value)) {
	/* reports always */
	parser_warn(node->nd_value, "found = in conditional, should be ==");
    }
    return 1;
}

static void
warn_unless_e_option(struct parser_params *parser, NODE *node, const char *str)
{
    if (!e_option_supplied(parser)) parser_warn(node, str);
}

static void
warning_unless_e_option(struct parser_params *parser, NODE *node, const char *str)
{
    if (!e_option_supplied(parser)) parser_warning(node, str);
}

static void
fixup_nodes(NODE **rootnode)
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
			       type == NODE_DOT3);
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

static NODE *cond0(struct parser_params*,NODE*);

static NODE*
range_op(struct parser_params *parser, NODE *node)
{
    enum node_type type;

    if (node == 0) return 0;

    type = nd_type(node);
    value_expr(node);
    if (type == NODE_LIT && FIXNUM_P(node->nd_lit)) {
	warn_unless_e_option(parser, node, "integer literal in conditional range");
	return NEW_CALL(node, tEQ, NEW_LIST(NEW_GVAR(rb_intern("$."))));
    }
    return cond0(parser, node);
}

static int
literal_node(NODE *node)
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
cond0(struct parser_params *parser, NODE *node)
{
    if (node == 0) return 0;
    assign_in_cond(parser, node);

    switch (nd_type(node)) {
      case NODE_DSTR:
      case NODE_EVSTR:
      case NODE_STR:
	rb_warn0("string literal in condition");
	break;

      case NODE_DREGX:
      case NODE_DREGX_ONCE:
	warning_unless_e_option(parser, node, "regex literal in condition");
	return NEW_MATCH2(node, NEW_GVAR(rb_intern("$_")));

      case NODE_AND:
      case NODE_OR:
	node->nd_1st = cond0(parser, node->nd_1st);
	node->nd_2nd = cond0(parser, node->nd_2nd);
	break;

      case NODE_DOT2:
      case NODE_DOT3:
	node->nd_beg = range_op(parser, node->nd_beg);
	node->nd_end = range_op(parser, node->nd_end);
	if (nd_type(node) == NODE_DOT2) nd_set_type(node,NODE_FLIP2);
	else if (nd_type(node) == NODE_DOT3) nd_set_type(node, NODE_FLIP3);
	if (!e_option_supplied(parser)) {
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
	if (RB_TYPE_P(node->nd_lit, T_REGEXP)) {
	    warn_unless_e_option(parser, node, "regex literal in condition");
	    nd_set_type(node, NODE_MATCH);
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
cond_gen(struct parser_params *parser, NODE *node)
{
    if (node == 0) return 0;
    return cond0(parser, node);
}

static NODE*
logop_gen(struct parser_params *parser, enum node_type type, NODE *left, NODE *right)
{
    value_expr(left);
    if (left && (enum node_type)nd_type(left) == type) {
	NODE *node = left, *second;
	while ((second = node->nd_2nd) != 0 && (enum node_type)nd_type(second) == type) {
	    node = second;
	}
	node->nd_2nd = NEW_NODE(type, second, right, 0);
	return left;
    }
    return NEW_NODE(type, left, right, 0);
}

static void
no_blockarg(struct parser_params *parser, NODE *node)
{
    if (node && nd_type(node) == NODE_BLOCK_PASS) {
	compile_error(PARSER_ARG "block argument should not be given");
    }
}

static NODE *
ret_args_gen(struct parser_params *parser, NODE *node)
{
    if (node) {
	no_blockarg(parser, node);
	if (nd_type(node) == NODE_ARRAY) {
	    if (node->nd_next == 0) {
		node = node->nd_head;
	    }
	    else {
		nd_set_type(node, NODE_VALUES);
	    }
	}
    }
    return node;
}

static NODE *
new_yield_gen(struct parser_params *parser, NODE *node)
{
    if (node) no_blockarg(parser, node);

    return NEW_YIELD(node);
}

static NODE*
negate_lit(NODE *node)
{
    switch (TYPE(node->nd_lit)) {
      case T_FIXNUM:
	node->nd_lit = LONG2FIX(-FIX2LONG(node->nd_lit));
	break;
      case T_BIGNUM:
	node->nd_lit = rb_funcall(node->nd_lit,tUMINUS,0,0);
	break;
      case T_FLOAT:
#if USE_FLONUM
	if (FLONUM_P(node->nd_lit)) {
	    node->nd_lit = DBL2NUM(-RFLOAT_VALUE(node->nd_lit));
	}
	else {
	    RFLOAT(node->nd_lit)->float_value = -RFLOAT_VALUE(node->nd_lit);
	}
#else
	RFLOAT(node->nd_lit)->float_value = -RFLOAT_VALUE(node->nd_lit);
#endif
	break;
      default:
	break;
    }
    return node;
}

static NODE *
arg_blk_pass(NODE *node1, NODE *node2)
{
    if (node2) {
	node2->nd_head = node1;
	return node2;
    }
    return node1;
}


static NODE*
new_args_gen(struct parser_params *parser, NODE *m, NODE *o, ID r, NODE *p, NODE *tail)
{
    int saved_line = ruby_sourceline;
    struct rb_args_info *args = tail->nd_ainfo;

    args->pre_args_num   = m ? rb_long2int(m->nd_plen) : 0;
    args->pre_init       = m ? m->nd_next : 0;

    args->post_args_num  = p ? rb_long2int(p->nd_plen) : 0;
    args->post_init      = p ? p->nd_next : 0;
    args->first_post_arg = p ? p->nd_pid : 0;

    args->rest_arg       = r;

    args->opt_args       = o;

    ruby_sourceline = saved_line;

    return tail;
}

static NODE*
new_args_tail_gen(struct parser_params *parser, NODE *k, ID kr, ID b)
{
    int saved_line = ruby_sourceline;
    struct rb_args_info *args;
    NODE *kw_rest_arg = 0;
    NODE *node;

    args = ALLOC(struct rb_args_info);
    MEMZERO(args, struct rb_args_info, 1);
    node = NEW_NODE(NODE_ARGS, 0, 0, args);

    args->block_arg      = b;
    args->kw_args        = k;
    if (k && !kr) kr = internal_id();
    if (kr) {
	arg_var(kr);
	kw_rest_arg  = NEW_DVAR(kr);
    }
    args->kw_rest_arg    = kw_rest_arg;

    ruby_sourceline = saved_line;
    return node;
}

static NODE*
dsym_node_gen(struct parser_params *parser, NODE *node)
{
    VALUE lit;

    if (!node) {
	return NEW_LIT(ID2SYM(idNULL));
    }

    switch (nd_type(node)) {
      case NODE_DSTR:
	nd_set_type(node, NODE_DSYM);
	break;
      case NODE_STR:
	lit = node->nd_lit;
	node->nd_lit = ID2SYM(rb_intern_str(lit));
	nd_set_type(node, NODE_LIT);
	break;
      default:
	node = NEW_NODE(NODE_DSYM, Qnil, 1, NEW_LIST(node));
	break;
    }
    return node;
}
#endif /* !RIPPER */

#ifndef RIPPER
static NODE *
new_op_assign_gen(struct parser_params *parser, NODE *lhs, ID op, NODE *rhs)
{
    NODE *asgn;

    if (lhs) {
	ID vid = lhs->nd_vid;
	if (op == tOROP) {
	    lhs->nd_value = rhs;
	    asgn = NEW_OP_ASGN_OR(gettable(vid), lhs);
	    if (is_asgn_or_id(vid)) {
		asgn->nd_aid = vid;
	    }
	}
	else if (op == tANDOP) {
	    lhs->nd_value = rhs;
	    asgn = NEW_OP_ASGN_AND(gettable(vid), lhs);
	}
	else {
	    asgn = lhs;
	    asgn->nd_value = NEW_CALL(gettable(vid), op, NEW_LIST(rhs));
	}
    }
    else {
	asgn = NEW_BEGIN(0);
    }
    return asgn;
}

static NODE *
new_attr_op_assign_gen(struct parser_params *parser, NODE *lhs, ID attr, ID op, NODE *rhs)
{
    NODE *asgn;

    if (op == tOROP) {
	op = 0;
    }
    else if (op == tANDOP) {
	op = 1;
    }
    asgn = NEW_OP_ASGN2(lhs, attr, op, rhs);
    fixpos(asgn, lhs);
    return asgn;
}

static NODE *
new_const_op_assign_gen(struct parser_params *parser, NODE *lhs, ID op, NODE *rhs)
{
    NODE *asgn;

    if (op == tOROP) {
	op = 0;
    }
    else if (op == tANDOP) {
	op = 1;
    }
    if (lhs) {
	asgn = NEW_OP_CDECL(lhs, op, rhs);
    }
    else {
	asgn = NEW_BEGIN(0);
    }
    fixpos(asgn, lhs);
    return asgn;
}
#else
static VALUE
new_op_assign_gen(struct parser_params *parser, VALUE lhs, VALUE op, VALUE rhs)
{
    return dispatch3(opassign, lhs, op, rhs);
}

static VALUE
new_attr_op_assign_gen(struct parser_params *parser, VALUE lhs, VALUE type, VALUE attr, VALUE op, VALUE rhs)
{
    VALUE recv = dispatch3(field, lhs, type, attr);
    return dispatch3(opassign, recv, op, rhs);
}
#endif

static void
warn_unused_var(struct parser_params *parser, struct local_vars *local)
{
    int i, cnt;
    ID *v, *u;

    if (!local->used) return;
    v = local->vars->tbl;
    u = local->used->tbl;
    cnt = local->used->pos;
    if (cnt != local->vars->pos) {
	rb_bug("local->used->pos != local->vars->pos");
    }
    for (i = 0; i < cnt; ++i) {
	if (!v[i] || (u[i] & LVAR_USED)) continue;
	if (is_private_local_id(v[i])) continue;
	rb_warn4S(ruby_sourcefile, (int)u[i], "assigned but unused variable - %s", rb_id2name(v[i]));
    }
}

static void
local_push_gen(struct parser_params *parser, int inherit_dvars)
{
    struct local_vars *local;

    local = ALLOC(struct local_vars);
    local->prev = lvtbl;
    local->args = vtable_alloc(0);
    local->vars = vtable_alloc(inherit_dvars ? DVARS_INHERIT : DVARS_TOPSCOPE);
    local->used = !(inherit_dvars &&
		    (ifndef_ripper(compile_for_eval || e_option_supplied(parser))+0)) &&
	RTEST(ruby_verbose) ? vtable_alloc(0) : 0;
    lvtbl = local;
}

static void
local_pop_gen(struct parser_params *parser)
{
    struct local_vars *local = lvtbl->prev;
    if (lvtbl->used) {
	warn_unused_var(parser, lvtbl);
	vtable_free(lvtbl->used);
    }
    vtable_free(lvtbl->args);
    vtable_free(lvtbl->vars);
    xfree(lvtbl);
    lvtbl = local;
}

#ifndef RIPPER
static ID*
vtable_tblcpy(ID *buf, const struct vtable *src)
{
    int i, cnt = vtable_size(src);

    if (cnt > 0) {
        buf[0] = cnt;
        for (i = 0; i < cnt; i++) {
            buf[i] = src->tbl[i];
        }
        return buf;
    }
    return 0;
}

static ID*
local_tbl_gen(struct parser_params *parser)
{
    int cnt = vtable_size(lvtbl->args) + vtable_size(lvtbl->vars);
    ID *buf;

    if (cnt <= 0) return 0;
    buf = ALLOC_N(ID, cnt + 1);
    vtable_tblcpy(buf+1, lvtbl->args);
    vtable_tblcpy(buf+vtable_size(lvtbl->args)+1, lvtbl->vars);
    buf[0] = cnt;
    return buf;
}
#endif

static int
arg_var_gen(struct parser_params *parser, ID id)
{
    vtable_add(lvtbl->args, id);
    return vtable_size(lvtbl->args) - 1;
}

static int
local_var_gen(struct parser_params *parser, ID id)
{
    vtable_add(lvtbl->vars, id);
    if (lvtbl->used) {
	vtable_add(lvtbl->used, (ID)ruby_sourceline);
    }
    return vtable_size(lvtbl->vars) - 1;
}

static int
local_id_gen(struct parser_params *parser, ID id)
{
    struct vtable *vars, *args, *used;

    vars = lvtbl->vars;
    args = lvtbl->args;
    used = lvtbl->used;

    while (vars && POINTER_P(vars->prev)) {
	vars = vars->prev;
	args = args->prev;
	if (used) used = used->prev;
    }

    if (vars && vars->prev == DVARS_INHERIT) {
	return rb_local_defined(id);
    }
    else if (vtable_included(args, id)) {
	return 1;
    }
    else {
	int i = vtable_included(vars, id);
	if (i && used) used->tbl[i-1] |= LVAR_USED;
	return i != 0;
    }
}

static const struct vtable *
dyna_push_gen(struct parser_params *parser)
{
    lvtbl->args = vtable_alloc(lvtbl->args);
    lvtbl->vars = vtable_alloc(lvtbl->vars);
    if (lvtbl->used) {
	lvtbl->used = vtable_alloc(lvtbl->used);
    }
    return lvtbl->args;
}

static void
dyna_pop_1(struct parser_params *parser)
{
    struct vtable *tmp;

    if ((tmp = lvtbl->used) != 0) {
	warn_unused_var(parser, lvtbl);
	lvtbl->used = lvtbl->used->prev;
	vtable_free(tmp);
    }
    tmp = lvtbl->args;
    lvtbl->args = lvtbl->args->prev;
    vtable_free(tmp);
    tmp = lvtbl->vars;
    lvtbl->vars = lvtbl->vars->prev;
    vtable_free(tmp);
}

static void
dyna_pop_gen(struct parser_params *parser, const struct vtable *lvargs)
{
    while (lvtbl->args != lvargs) {
	dyna_pop_1(parser);
	if (!lvtbl->args) {
	    struct local_vars *local = lvtbl->prev;
	    xfree(lvtbl);
	    lvtbl = local;
	}
    }
    dyna_pop_1(parser);
}

static int
dyna_in_block_gen(struct parser_params *parser)
{
    return POINTER_P(lvtbl->vars) && lvtbl->vars->prev != DVARS_TOPSCOPE;
}

static int
dvar_defined_gen(struct parser_params *parser, ID id, int get)
{
    struct vtable *vars, *args, *used;
    int i;

    args = lvtbl->args;
    vars = lvtbl->vars;
    used = lvtbl->used;

    while (POINTER_P(vars)) {
	if (vtable_included(args, id)) {
	    return 1;
	}
	if ((i = vtable_included(vars, id)) != 0) {
	    if (used) used->tbl[i-1] |= LVAR_USED;
	    return 1;
	}
	args = args->prev;
	vars = vars->prev;
	if (get) used = 0;
	if (used) used = used->prev;
    }

    if (vars == DVARS_INHERIT) {
        return rb_dvar_defined(id);
    }

    return 0;
}

static int
dvar_curr_gen(struct parser_params *parser, ID id)
{
    return (vtable_included(lvtbl->args, id) ||
	    vtable_included(lvtbl->vars, id));
}

#ifndef RIPPER
static void
reg_fragment_setenc_gen(struct parser_params* parser, VALUE str, int options)
{
    int c = RE_OPTION_ENCODING_IDX(options);

    if (c) {
	int opt, idx;
	rb_char_to_option_kcode(c, &opt, &idx);
	if (idx != ENCODING_GET(str) &&
	    rb_enc_str_coderange(str) != ENC_CODERANGE_7BIT) {
            goto error;
	}
	ENCODING_SET(str, idx);
    }
    else if (RE_OPTION_ENCODING_NONE(options)) {
        if (!ENCODING_IS_ASCII8BIT(str) &&
            rb_enc_str_coderange(str) != ENC_CODERANGE_7BIT) {
            c = 'n';
            goto error;
        }
	rb_enc_associate(str, rb_ascii8bit_encoding());
    }
    else if (current_enc == rb_usascii_encoding()) {
	if (rb_enc_str_coderange(str) != ENC_CODERANGE_7BIT) {
	    /* raise in re.c */
	    rb_enc_associate(str, rb_usascii_encoding());
	}
	else {
	    rb_enc_associate(str, rb_ascii8bit_encoding());
	}
    }
    return;

  error:
    compile_error(PARSER_ARG
        "regexp encoding option '%c' differs from source encoding '%s'",
        c, rb_enc_name(rb_enc_get(str)));
}

static int
reg_fragment_check_gen(struct parser_params* parser, VALUE str, int options)
{
    VALUE err;
    reg_fragment_setenc(str, options);
    err = rb_reg_check_preprocess(str);
    if (err != Qnil) {
        err = rb_obj_as_string(err);
        compile_error(PARSER_ARG "%s", RSTRING_PTR(err));
	RB_GC_GUARD(err);
	return 0;
    }
    return 1;
}

typedef struct {
    struct parser_params* parser;
    rb_encoding *enc;
    NODE *succ_block;
    NODE *fail_block;
    int num;
} reg_named_capture_assign_t;

static int
reg_named_capture_assign_iter(const OnigUChar *name, const OnigUChar *name_end,
          int back_num, int *back_refs, OnigRegex regex, void *arg0)
{
    reg_named_capture_assign_t *arg = (reg_named_capture_assign_t*)arg0;
    struct parser_params* parser = arg->parser;
    rb_encoding *enc = arg->enc;
    long len = name_end - name;
    const char *s = (const char *)name;
    ID var;

    arg->num++;

    if (arg->succ_block == 0) {
        arg->succ_block = NEW_BEGIN(0);
        arg->fail_block = NEW_BEGIN(0);
    }

    if (!len || (*name != '_' && ISASCII(*name) && !rb_enc_islower(*name, enc)) ||
	(len < MAX_WORD_LENGTH && rb_reserved_word(s, (int)len)) ||
	!rb_enc_symname2_p(s, len, enc)) {
        return ST_CONTINUE;
    }
    var = rb_intern3(s, len, enc);
    if (dvar_defined(var) || local_id(var)) {
        rb_warningS("named capture conflicts a local variable - %s",
                    rb_id2name(var));
    }
    arg->succ_block = block_append(arg->succ_block,
        newline_node(node_assign(assignable(var,0),
            NEW_CALL(
              gettable(rb_intern("$~")),
              idAREF,
              NEW_LIST(NEW_LIT(ID2SYM(var))))
            )));
    arg->fail_block = block_append(arg->fail_block,
        newline_node(node_assign(assignable(var,0), NEW_LIT(Qnil))));
    return ST_CONTINUE;
}

static NODE *
reg_named_capture_assign_gen(struct parser_params* parser, VALUE regexp, NODE *match)
{
    reg_named_capture_assign_t arg;

    arg.parser = parser;
    arg.enc = rb_enc_get(regexp);
    arg.succ_block = 0;
    arg.fail_block = 0;
    arg.num = 0;
    onig_foreach_name(RREGEXP(regexp)->ptr, reg_named_capture_assign_iter, (void*)&arg);

    if (arg.num == 0)
        return match;

    return
        block_append(
            newline_node(match),
            NEW_IF(gettable(rb_intern("$~")),
                block_append(
                    newline_node(arg.succ_block),
                    newline_node(
                        NEW_CALL(
                          gettable(rb_intern("$~")),
                          rb_intern("begin"),
                          NEW_LIST(NEW_LIT(INT2FIX(0)))))),
                block_append(
                    newline_node(arg.fail_block),
                    newline_node(
                        NEW_LIT(Qnil)))));
}

static VALUE
reg_compile_gen(struct parser_params* parser, VALUE str, int options)
{
    VALUE re;
    VALUE err;

    reg_fragment_setenc(str, options);
    err = rb_errinfo();
    re = rb_reg_compile(str, options & RE_OPTION_MASK, ruby_sourcefile, ruby_sourceline);
    if (NIL_P(re)) {
	ID mesg = rb_intern("mesg");
	VALUE m = rb_attr_get(rb_errinfo(), mesg);
	rb_set_errinfo(err);
	if (!NIL_P(err)) {
	    rb_str_append(rb_str_cat(rb_attr_get(err, mesg), "\n", 1), m);
	}
	else {
	    compile_error(PARSER_ARG "%s", RSTRING_PTR(m));
	}
	return Qnil;
    }
    return re;
}

void
rb_gc_mark_parser(void)
{
}

NODE*
rb_parser_append_print(VALUE vparser, NODE *node)
{
    NODE *prelude = 0;
    NODE *scope = node;
    struct parser_params *parser;

    if (!node) return node;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, parser);

    node = node->nd_body;

    if (nd_type(node) == NODE_PRELUDE) {
	prelude = node;
	node = node->nd_body;
    }

    node = block_append(node,
			NEW_FCALL(rb_intern("print"),
				  NEW_ARRAY(NEW_GVAR(rb_intern("$_")))));
    if (prelude) {
	prelude->nd_body = node;
	scope->nd_body = prelude;
    }
    else {
	scope->nd_body = node;
    }

    return scope;
}

NODE *
rb_parser_while_loop(VALUE vparser, NODE *node, int chop, int split)
{
    NODE *prelude = 0;
    NODE *scope = node;
    struct parser_params *parser;

    if (!node) return node;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, parser);

    node = node->nd_body;

    if (nd_type(node) == NODE_PRELUDE) {
	prelude = node;
	node = node->nd_body;
    }
    if (split) {
	node = block_append(NEW_GASGN(rb_intern("$F"),
				      NEW_CALL(NEW_GVAR(rb_intern("$_")),
					       rb_intern("split"), 0)),
			    node);
    }
    if (chop) {
	node = block_append(NEW_CALL(NEW_GVAR(rb_intern("$_")),
				     rb_intern("chop!"), 0), node);
    }

    node = NEW_OPT_N(node);

    if (prelude) {
	prelude->nd_body = node;
	scope->nd_body = prelude;
    }
    else {
	scope->nd_body = node;
    }

    return scope;
}

static const struct {
    ID token;
    const char *name;
} op_tbl[] = {
    {tDOT2,	".."},
    {tDOT3,	"..."},
    {'+',	"+(binary)"},
    {'-',	"-(binary)"},
    {tPOW,	"**"},
    {tDSTAR,	"**"},
    {tUPLUS,	"+@"},
    {tUMINUS,	"-@"},
    {tCMP,	"<=>"},
    {tGEQ,	">="},
    {tLEQ,	"<="},
    {tEQ,	"=="},
    {tEQQ,	"==="},
    {tNEQ,	"!="},
    {tMATCH,	"=~"},
    {tNMATCH,	"!~"},
    {tAREF,	"[]"},
    {tASET,	"[]="},
    {tLSHFT,	"<<"},
    {tRSHFT,	">>"},
    {tCOLON2,	"::"},
};

#define op_tbl_count numberof(op_tbl)

#ifndef ENABLE_SELECTOR_NAMESPACE
#define ENABLE_SELECTOR_NAMESPACE 0
#endif

static struct symbols {
    ID last_id;
    st_table *sym_id;
    st_table *id_str;
#if ENABLE_SELECTOR_NAMESPACE
    st_table *ivar2_id;
    st_table *id_ivar2;
#endif
    VALUE op_sym[tLAST_OP_ID];
} global_symbols = {tLAST_TOKEN};

static const struct st_hash_type symhash = {
    rb_str_hash_cmp,
    rb_str_hash,
};

#if ENABLE_SELECTOR_NAMESPACE
struct ivar2_key {
    ID id;
    VALUE klass;
};

static int
ivar2_cmp(struct ivar2_key *key1, struct ivar2_key *key2)
{
    if (key1->id == key2->id && key1->klass == key2->klass) {
	return 0;
    }
    return 1;
}

static int
ivar2_hash(struct ivar2_key *key)
{
    return (key->id << 8) ^ (key->klass >> 2);
}

static const struct st_hash_type ivar2_hash_type = {
    ivar2_cmp,
    ivar2_hash,
};
#endif

void
Init_sym(void)
{
    global_symbols.sym_id = st_init_table_with_size(&symhash, 1000);
    global_symbols.id_str = st_init_numtable_with_size(1000);
#if ENABLE_SELECTOR_NAMESPACE
    global_symbols.ivar2_id = st_init_table_with_size(&ivar2_hash_type, 1000);
    global_symbols.id_ivar2 = st_init_numtable_with_size(1000);
#endif

    (void)nodetype;
    (void)nodeline;
#if PARSER_DEBUG
    (void)lex_state_name(-1);
#endif

    Init_id();
}

void
rb_gc_mark_symbols(void)
{
    rb_mark_tbl(global_symbols.id_str);
    rb_gc_mark_locations(global_symbols.op_sym,
			 global_symbols.op_sym + numberof(global_symbols.op_sym));
}
#endif /* !RIPPER */

static ID
internal_id_gen(struct parser_params *parser)
{
    ID id = (ID)vtable_size(lvtbl->args) + (ID)vtable_size(lvtbl->vars);
    id += ((tLAST_TOKEN - ID_INTERNAL) >> ID_SCOPE_SHIFT) + 1;
    return ID_INTERNAL | (id << ID_SCOPE_SHIFT);
}

#ifndef RIPPER
static int
is_special_global_name(const char *m, const char *e, rb_encoding *enc)
{
    int mb = 0;

    if (m >= e) return 0;
    if (is_global_name_punct(*m)) {
	++m;
    }
    else if (*m == '-') {
	++m;
	if (m < e && is_identchar(m, e, enc)) {
	    if (!ISASCII(*m)) mb = 1;
	    m += rb_enc_mbclen(m, e, enc);
	}
    }
    else {
	if (!rb_enc_isdigit(*m, enc)) return 0;
	do {
	    if (!ISASCII(*m)) mb = 1;
	    ++m;
	} while (m < e && rb_enc_isdigit(*m, enc));
    }
    return m == e ? mb + 1 : 0;
}

int
rb_symname_p(const char *name)
{
    return rb_enc_symname_p(name, rb_ascii8bit_encoding());
}

int
rb_enc_symname_p(const char *name, rb_encoding *enc)
{
    return rb_enc_symname2_p(name, strlen(name), enc);
}

static int
rb_enc_symname_type(const char *name, long len, rb_encoding *enc)
{
    const char *m = name;
    const char *e = m + len;
    int type = ID_JUNK;

    if (!m || len <= 0) return -1;
    switch (*m) {
      case '\0':
	return -1;

      case '$':
	type = ID_GLOBAL;
	if (is_special_global_name(++m, e, enc)) return type;
	goto id;

      case '@':
	type = ID_INSTANCE;
	if (*++m == '@') {
	    ++m;
	    type = ID_CLASS;
	}
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
	  default: return -1;
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
	if (*++m != ']') return -1;
	if (*++m == '=') ++m;
	break;

      case '!':
	if (len == 1) return ID_JUNK;
	switch (*++m) {
	  case '=': case '~': ++m; break;
	  default: return -1;
	}
	break;

      default:
	type = rb_enc_isupper(*m, enc) ? ID_CONST : ID_LOCAL;
      id:
	if (m >= e || (*m != '_' && !rb_enc_isalpha(*m, enc) && ISASCII(*m)))
	    return -1;
	while (m < e && is_identchar(m, e, enc)) m += rb_enc_mbclen(m, e, enc);
	switch (*m) {
	  case '!': case '?':
	    if (type == ID_GLOBAL || type == ID_CLASS || type == ID_INSTANCE) return -1;
	    type = ID_JUNK;
	    ++m;
	    break;
	  case '=':
	    if (type != ID_CONST && type != ID_LOCAL) return -1;
	    type = ID_ATTRSET;
	    ++m;
	    break;
	}
	break;
    }
    return m == e ? type : -1;
}

int
rb_enc_symname2_p(const char *name, long len, rb_encoding *enc)
{
    return rb_enc_symname_type(name, len, enc) != -1;
}

static int
rb_str_symname_type(VALUE name)
{
    const char *ptr = StringValuePtr(name);
    long len = RSTRING_LEN(name);
    int type = rb_enc_symname_type(ptr, len, rb_enc_get(name));
    RB_GC_GUARD(name);
    return type;
}

static ID
register_symid(ID id, const char *name, long len, rb_encoding *enc)
{
    VALUE str = rb_enc_str_new(name, len, enc);
    return register_symid_str(id, str);
}

static ID
register_symid_str(ID id, VALUE str)
{
    OBJ_FREEZE(str);
    st_add_direct(global_symbols.sym_id, (st_data_t)str, id);
    st_add_direct(global_symbols.id_str, id, (st_data_t)str);
    return id;
}

static int
sym_check_asciionly(VALUE str)
{
    if (!rb_enc_asciicompat(rb_enc_get(str))) return FALSE;
    switch (rb_enc_str_coderange(str)) {
      case ENC_CODERANGE_BROKEN:
	rb_raise(rb_eEncodingError, "invalid encoding symbol");
      case ENC_CODERANGE_7BIT:
	return TRUE;
    }
    return FALSE;
}

/*
 * _str_ itself will be registered at the global symbol table.  _str_
 * can be modified before the registration, since the encoding will be
 * set to ASCII-8BIT if it is a special global name.
 */
static ID intern_str(VALUE str);

ID
rb_intern3(const char *name, long len, rb_encoding *enc)
{
    VALUE str;
    st_data_t data;
    struct RString fake_str;
    fake_str.basic.flags = T_STRING|RSTRING_NOEMBED;
    fake_str.basic.klass = rb_cString;
    fake_str.as.heap.len = len;
    fake_str.as.heap.ptr = (char *)name;
    fake_str.as.heap.aux.capa = len;
    str = (VALUE)&fake_str;
    rb_enc_associate(str, enc);
    OBJ_FREEZE(str);

    if (st_lookup(global_symbols.sym_id, str, &data))
	return (ID)data;

    str = rb_enc_str_new(name, len, enc); /* make true string */
    return intern_str(str);
}

static ID
intern_str(VALUE str)
{
    const char *name, *m, *e;
    long len, last;
    rb_encoding *enc, *symenc;
    unsigned char c;
    ID id;
    int mb;

    RSTRING_GETMEM(str, name, len);
    m = name;
    e = m + len;
    enc = rb_enc_get(str);
    symenc = enc;

    if (rb_cString && !rb_enc_asciicompat(enc)) {
	id = ID_JUNK;
	goto new_id;
    }
    last = len-1;
    id = 0;
    switch (*m) {
      case '$':
	id |= ID_GLOBAL;
	if ((mb = is_special_global_name(++m, e, enc)) != 0) {
	    if (!--mb) symenc = rb_usascii_encoding();
	    goto new_id;
	}
	break;
      case '@':
	if (m[1] == '@') {
	    m++;
	    id |= ID_CLASS;
	}
	else {
	    id |= ID_INSTANCE;
	}
	m++;
	break;
      default:
	c = m[0];
	if (c != '_' && rb_enc_isascii(c, enc) && rb_enc_ispunct(c, enc)) {
	    /* operators */
	    int i;

	    if (len == 1) {
		id = c;
		goto id_register;
	    }
	    for (i = 0; i < op_tbl_count; i++) {
		if (*op_tbl[i].name == *m &&
		    strcmp(op_tbl[i].name, m) == 0) {
		    id = op_tbl[i].token;
		    goto id_register;
		}
	    }
	}

	if (m[last] == '=') {
	    /* attribute assignment */
	    id = rb_intern3(name, last, enc);
	    if (id > tLAST_OP_ID && !is_attrset_id(id)) {
		enc = rb_enc_get(rb_id2str(id));
		id = rb_id_attrset(id);
		goto id_register;
	    }
	    id = ID_ATTRSET;
	}
	else if (rb_enc_isupper(m[0], enc)) {
	    id = ID_CONST;
        }
	else {
	    id = ID_LOCAL;
	}
	break;
    }
    if (!rb_enc_isdigit(*m, enc)) {
	while (m <= name + last && is_identchar(m, e, enc)) {
	    if (ISASCII(*m)) {
		m++;
	    }
	    else {
		m += rb_enc_mbclen(m, e, enc);
	    }
	}
    }
    if (m - name < len) id = ID_JUNK;
    if (sym_check_asciionly(str)) symenc = rb_usascii_encoding();
  new_id:
    if (symenc != enc) rb_enc_associate(str, symenc);
    if (global_symbols.last_id >= ~(ID)0 >> (ID_SCOPE_SHIFT+RUBY_SPECIAL_SHIFT)) {
	if (len > 20) {
	    rb_raise(rb_eRuntimeError, "symbol table overflow (symbol %.20s...)",
		     name);
	}
	else {
	    rb_raise(rb_eRuntimeError, "symbol table overflow (symbol %.*s)",
		     (int)len, name);
	}
    }
    id |= ++global_symbols.last_id << ID_SCOPE_SHIFT;
  id_register:
    return register_symid_str(id, str);
}

ID
rb_intern2(const char *name, long len)
{
    return rb_intern3(name, len, rb_usascii_encoding());
}

#undef rb_intern
ID
rb_intern(const char *name)
{
    return rb_intern2(name, strlen(name));
}

ID
rb_intern_str(VALUE str)
{
    st_data_t id;

    if (st_lookup(global_symbols.sym_id, str, &id))
	return (ID)id;
    return intern_str(rb_str_dup(str));
}

VALUE
rb_id2str(ID id)
{
    st_data_t data;

    if (id < tLAST_TOKEN) {
	int i = 0;

	if (id < INT_MAX && rb_ispunct((int)id)) {
	    VALUE str = global_symbols.op_sym[i = (int)id];
	    if (!str) {
		char name[2];
		name[0] = (char)id;
		name[1] = 0;
		str = rb_usascii_str_new(name, 1);
		OBJ_FREEZE(str);
		global_symbols.op_sym[i] = str;
	    }
	    return str;
	}
	for (i = 0; i < op_tbl_count; i++) {
	    if (op_tbl[i].token == id) {
		VALUE str = global_symbols.op_sym[i];
		if (!str) {
		    str = rb_usascii_str_new2(op_tbl[i].name);
		    OBJ_FREEZE(str);
		    global_symbols.op_sym[i] = str;
		}
		return str;
	    }
	}
    }

    if (st_lookup(global_symbols.id_str, id, &data)) {
        VALUE str = (VALUE)data;
        if (RBASIC(str)->klass == 0)
            RBASIC(str)->klass = rb_cString;
	return str;
    }

    if (is_attrset_id(id)) {
	ID id2 = (id & ~ID_SCOPE_MASK) | ID_LOCAL;
	VALUE str;

	while (!(str = rb_id2str(id2))) {
	    if (!is_local_id(id2)) return 0;
	    id2 = (id & ~ID_SCOPE_MASK) | ID_CONST;
	}
	str = rb_str_dup(str);
	rb_str_cat(str, "=", 1);
	rb_intern_str(str);
	if (st_lookup(global_symbols.id_str, id, &data)) {
            VALUE str = (VALUE)data;
            if (RBASIC(str)->klass == 0)
                RBASIC(str)->klass = rb_cString;
            return str;
        }
    }
    return 0;
}

const char *
rb_id2name(ID id)
{
    VALUE str = rb_id2str(id);

    if (!str) return 0;
    return RSTRING_PTR(str);
}

static int
symbols_i(VALUE sym, ID value, VALUE ary)
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
rb_sym_all_symbols(void)
{
    VALUE ary = rb_ary_new2(global_symbols.sym_id->num_entries);

    st_foreach(global_symbols.sym_id, symbols_i, ary);
    return ary;
}

int
rb_is_const_id(ID id)
{
    return is_const_id(id);
}

int
rb_is_class_id(ID id)
{
    return is_class_id(id);
}

int
rb_is_global_id(ID id)
{
    return is_global_id(id);
}

int
rb_is_instance_id(ID id)
{
    return is_instance_id(id);
}

int
rb_is_attrset_id(ID id)
{
    return is_attrset_id(id);
}

int
rb_is_local_id(ID id)
{
    return is_local_id(id);
}

int
rb_is_junk_id(ID id)
{
    return is_junk_id(id);
}

/**
 * Returns ID for the given name if it is interned already, or 0.
 *
 * \param namep   the pointer to the name object
 * \return        the ID for *namep
 * \pre           the object referred by \p namep must be a Symbol or
 *                a String, or possible to convert with to_str method.
 * \post          the object referred by \p namep is a Symbol or a
 *                String if non-zero value is returned, or is a String
 *                if 0 is returned.
 */
ID
rb_check_id(volatile VALUE *namep)
{
    st_data_t id;
    VALUE tmp;
    VALUE name = *namep;

    if (SYMBOL_P(name)) {
	return SYM2ID(name);
    }
    else if (!RB_TYPE_P(name, T_STRING)) {
	tmp = rb_check_string_type(name);
	if (NIL_P(tmp)) {
	    tmp = rb_inspect(name);
	    rb_raise(rb_eTypeError, "%s is not a symbol",
		     RSTRING_PTR(tmp));
	}
	name = tmp;
	*namep = name;
    }

    sym_check_asciionly(name);

    if (st_lookup(global_symbols.sym_id, (st_data_t)name, &id))
	return (ID)id;

    if (rb_is_attrset_name(name)) {
	struct RString fake_str;
	const VALUE localname = (VALUE)&fake_str;
	/* make local name by chopping '=' */
	fake_str.basic.flags = T_STRING|RSTRING_NOEMBED;
	fake_str.basic.klass = rb_cString;
	fake_str.as.heap.len = RSTRING_LEN(name) - 1;
	fake_str.as.heap.ptr = RSTRING_PTR(name);
	fake_str.as.heap.aux.capa = fake_str.as.heap.len;
	rb_enc_copy(localname, name);
	OBJ_FREEZE(localname);

	if (st_lookup(global_symbols.sym_id, (st_data_t)localname, &id)) {
	    return rb_id_attrset((ID)id);
	}
	RB_GC_GUARD(name);
    }

    return (ID)0;
}

ID
rb_check_id_cstr(const char *ptr, long len, rb_encoding *enc)
{
    st_data_t id;
    struct RString fake_str;
    const VALUE name = (VALUE)&fake_str;
    fake_str.basic.flags = T_STRING|RSTRING_NOEMBED;
    fake_str.basic.klass = rb_cString;
    fake_str.as.heap.len = len;
    fake_str.as.heap.ptr = (char *)ptr;
    fake_str.as.heap.aux.capa = len;
    rb_enc_associate(name, enc);

    sym_check_asciionly(name);

    if (st_lookup(global_symbols.sym_id, (st_data_t)name, &id))
	return (ID)id;

    if (rb_is_attrset_name(name)) {
	fake_str.as.heap.len = len - 1;
	if (st_lookup(global_symbols.sym_id, (st_data_t)name, &id)) {
	    return rb_id_attrset((ID)id);
	}
    }

    return (ID)0;
}

int
rb_is_const_name(VALUE name)
{
    return rb_str_symname_type(name) == ID_CONST;
}

int
rb_is_class_name(VALUE name)
{
    return rb_str_symname_type(name) == ID_CLASS;
}

int
rb_is_global_name(VALUE name)
{
    return rb_str_symname_type(name) == ID_GLOBAL;
}

int
rb_is_instance_name(VALUE name)
{
    return rb_str_symname_type(name) == ID_INSTANCE;
}

int
rb_is_attrset_name(VALUE name)
{
    return rb_str_symname_type(name) == ID_ATTRSET;
}

int
rb_is_local_name(VALUE name)
{
    return rb_str_symname_type(name) == ID_LOCAL;
}

int
rb_is_method_name(VALUE name)
{
    switch (rb_str_symname_type(name)) {
      case ID_LOCAL: case ID_ATTRSET: case ID_JUNK:
	return TRUE;
    }
    return FALSE;
}

int
rb_is_junk_name(VALUE name)
{
    return rb_str_symname_type(name) == -1;
}

#endif /* !RIPPER */

static void
parser_initialize(struct parser_params *parser)
{
    parser->eofp = Qfalse;

    parser->parser_lex_strterm = 0;
    parser->parser_cond_stack = 0;
    parser->parser_cmdarg_stack = 0;
    parser->parser_class_nest = 0;
    parser->parser_paren_nest = 0;
    parser->parser_lpar_beg = 0;
    parser->parser_brace_nest = 0;
    parser->parser_in_single = 0;
    parser->parser_in_def = 0;
    parser->parser_in_defined = 0;
    parser->parser_compile_for_eval = 0;
    parser->parser_cur_mid = 0;
    parser->parser_tokenbuf = NULL;
    parser->parser_tokidx = 0;
    parser->parser_toksiz = 0;
    parser->parser_heredoc_end = 0;
    parser->parser_command_start = TRUE;
    parser->parser_deferred_nodes = 0;
    parser->parser_lex_pbeg = 0;
    parser->parser_lex_p = 0;
    parser->parser_lex_pend = 0;
    parser->parser_lvtbl = 0;
    parser->parser_ruby__end__seen = 0;
    parser->parser_ruby_sourcefile = 0;
#ifndef RIPPER
    parser->is_ripper = 0;
    parser->parser_eval_tree_begin = 0;
    parser->parser_eval_tree = 0;
#else
    parser->is_ripper = 1;
    parser->parser_ruby_sourcefile_string = Qnil;
    parser->delayed = Qnil;

    parser->result = Qnil;
    parser->parsing_thread = Qnil;
    parser->toplevel_p = TRUE;
#endif
#ifdef YYMALLOC
    parser->heap = NULL;
#endif
    parser->enc = rb_utf8_encoding();
}

#ifdef RIPPER
#define parser_mark ripper_parser_mark
#define parser_free ripper_parser_free
#endif

static void
parser_mark(void *ptr)
{
    struct parser_params *p = (struct parser_params*)ptr;

    rb_gc_mark((VALUE)p->parser_lex_strterm);
    rb_gc_mark((VALUE)p->parser_deferred_nodes);
    rb_gc_mark(p->parser_lex_input);
    rb_gc_mark(p->parser_lex_lastline);
    rb_gc_mark(p->parser_lex_nextline);
#ifndef RIPPER
    rb_gc_mark((VALUE)p->parser_eval_tree_begin) ;
    rb_gc_mark((VALUE)p->parser_eval_tree) ;
    rb_gc_mark(p->debug_lines);
#else
    rb_gc_mark(p->parser_ruby_sourcefile_string);
    rb_gc_mark(p->delayed);
    rb_gc_mark(p->value);
    rb_gc_mark(p->result);
    rb_gc_mark(p->parsing_thread);
#endif
#ifdef YYMALLOC
    rb_gc_mark((VALUE)p->heap);
#endif
}

static void
parser_free(void *ptr)
{
    struct parser_params *p = (struct parser_params*)ptr;
    struct local_vars *local, *prev;

    if (p->parser_tokenbuf) {
        xfree(p->parser_tokenbuf);
    }
    for (local = p->parser_lvtbl; local; local = prev) {
	if (local->vars) xfree(local->vars);
	prev = local->prev;
	xfree(local);
    }
#ifndef RIPPER
    xfree(p->parser_ruby_sourcefile);
#endif
    xfree(p);
}

static size_t
parser_memsize(const void *ptr)
{
    struct parser_params *p = (struct parser_params*)ptr;
    struct local_vars *local;
    size_t size = sizeof(*p);

    if (!ptr) return 0;
    size += p->parser_toksiz;
    for (local = p->parser_lvtbl; local; local = local->prev) {
	size += sizeof(*local);
	if (local->vars) size += local->vars->capa * sizeof(ID);
    }
#ifndef RIPPER
    if (p->parser_ruby_sourcefile) {
	size += strlen(p->parser_ruby_sourcefile) + 1;
    }
#endif
    return size;
}

static
#ifndef RIPPER
const
#endif
rb_data_type_t parser_data_type = {
    "parser",
    {
	parser_mark,
	parser_free,
	parser_memsize,
    },
};

#ifndef RIPPER
#undef rb_reserved_word

const struct kwtable *
rb_reserved_word(const char *str, unsigned int len)
{
    return reserved_word(str, len);
}

static struct parser_params *
parser_new(void)
{
    struct parser_params *p;

    p = ALLOC_N(struct parser_params, 1);
    MEMZERO(p, struct parser_params, 1);
    parser_initialize(p);
    return p;
}

VALUE
rb_parser_new(void)
{
    struct parser_params *p = parser_new();

    return TypedData_Wrap_Struct(0, &parser_data_type, p);
}

/*
 *  call-seq:
 *    ripper#end_seen?   -> Boolean
 *
 *  Return true if parsed source ended by +\_\_END\_\_+.
 */
VALUE
rb_parser_end_seen_p(VALUE vparser)
{
    struct parser_params *parser;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, parser);
    return ruby__end__seen ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *    ripper#encoding   -> encoding
 *
 *  Return encoding of the source.
 */
VALUE
rb_parser_encoding(VALUE vparser)
{
    struct parser_params *parser;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, parser);
    return rb_enc_from_encoding(current_enc);
}

/*
 *  call-seq:
 *    ripper.yydebug   -> true or false
 *
 *  Get yydebug.
 */
VALUE
rb_parser_get_yydebug(VALUE self)
{
    struct parser_params *parser;

    TypedData_Get_Struct(self, struct parser_params, &parser_data_type, parser);
    return yydebug ? Qtrue : Qfalse;
}

/*
 *  call-seq:
 *    ripper.yydebug = flag
 *
 *  Set yydebug.
 */
VALUE
rb_parser_set_yydebug(VALUE self, VALUE flag)
{
    struct parser_params *parser;

    TypedData_Get_Struct(self, struct parser_params, &parser_data_type, parser);
    yydebug = RTEST(flag);
    return flag;
}

#ifdef YYMALLOC
#define HEAPCNT(n, size) ((n) * (size) / sizeof(YYSTYPE))
#define NEWHEAP() rb_node_newnode(NODE_ALLOCA, 0, (VALUE)parser->heap, 0)
#define ADD2HEAP(n, c, p) ((parser->heap = (n))->u1.node = (p), \
			   (n)->u3.cnt = (c), (p))

void *
rb_parser_malloc(struct parser_params *parser, size_t size)
{
    size_t cnt = HEAPCNT(1, size);
    NODE *n = NEWHEAP();
    void *ptr = xmalloc(size);

    return ADD2HEAP(n, cnt, ptr);
}

void *
rb_parser_calloc(struct parser_params *parser, size_t nelem, size_t size)
{
    size_t cnt = HEAPCNT(nelem, size);
    NODE *n = NEWHEAP();
    void *ptr = xcalloc(nelem, size);

    return ADD2HEAP(n, cnt, ptr);
}

void *
rb_parser_realloc(struct parser_params *parser, void *ptr, size_t size)
{
    NODE *n;
    size_t cnt = HEAPCNT(1, size);

    if (ptr && (n = parser->heap) != NULL) {
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

void
rb_parser_free(struct parser_params *parser, void *ptr)
{
    NODE **prev = &parser->heap, *n;

    while ((n = *prev) != NULL) {
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
#endif

#ifdef RIPPER
#ifdef RIPPER_DEBUG
extern int rb_is_pointer_to_heap(VALUE);

/* :nodoc: */
static VALUE
ripper_validate_object(VALUE self, VALUE x)
{
    if (x == Qfalse) return x;
    if (x == Qtrue) return x;
    if (x == Qnil) return x;
    if (x == Qundef)
        rb_raise(rb_eArgError, "Qundef given");
    if (FIXNUM_P(x)) return x;
    if (SYMBOL_P(x)) return x;
    if (!rb_is_pointer_to_heap(x))
        rb_raise(rb_eArgError, "invalid pointer: %p", x);
    switch (TYPE(x)) {
      case T_STRING:
      case T_OBJECT:
      case T_ARRAY:
      case T_BIGNUM:
      case T_FLOAT:
        return x;
      case T_NODE:
	if (nd_type(x) != NODE_LASGN) {
	    rb_raise(rb_eArgError, "NODE given: %p", x);
	}
	return ((NODE *)x)->nd_rval;
      default:
        rb_raise(rb_eArgError, "wrong type of ruby object: %p (%s)",
                 x, rb_obj_classname(x));
    }
    return x;
}
#endif

#define validate(x) ((x) = get_value(x))

static VALUE
ripper_dispatch0(struct parser_params *parser, ID mid)
{
    return rb_funcall(parser->value, mid, 0);
}

static VALUE
ripper_dispatch1(struct parser_params *parser, ID mid, VALUE a)
{
    validate(a);
    return rb_funcall(parser->value, mid, 1, a);
}

static VALUE
ripper_dispatch2(struct parser_params *parser, ID mid, VALUE a, VALUE b)
{
    validate(a);
    validate(b);
    return rb_funcall(parser->value, mid, 2, a, b);
}

static VALUE
ripper_dispatch3(struct parser_params *parser, ID mid, VALUE a, VALUE b, VALUE c)
{
    validate(a);
    validate(b);
    validate(c);
    return rb_funcall(parser->value, mid, 3, a, b, c);
}

static VALUE
ripper_dispatch4(struct parser_params *parser, ID mid, VALUE a, VALUE b, VALUE c, VALUE d)
{
    validate(a);
    validate(b);
    validate(c);
    validate(d);
    return rb_funcall(parser->value, mid, 4, a, b, c, d);
}

static VALUE
ripper_dispatch5(struct parser_params *parser, ID mid, VALUE a, VALUE b, VALUE c, VALUE d, VALUE e)
{
    validate(a);
    validate(b);
    validate(c);
    validate(d);
    validate(e);
    return rb_funcall(parser->value, mid, 5, a, b, c, d, e);
}

static VALUE
ripper_dispatch7(struct parser_params *parser, ID mid, VALUE a, VALUE b, VALUE c, VALUE d, VALUE e, VALUE f, VALUE g)
{
    validate(a);
    validate(b);
    validate(c);
    validate(d);
    validate(e);
    validate(f);
    validate(g);
    return rb_funcall(parser->value, mid, 7, a, b, c, d, e, f, g);
}

static const struct kw_assoc {
    ID id;
    const char *name;
} keyword_to_name[] = {
    {keyword_class,	"class"},
    {keyword_module,	"module"},
    {keyword_def,	"def"},
    {keyword_undef,	"undef"},
    {keyword_begin,	"begin"},
    {keyword_rescue,	"rescue"},
    {keyword_ensure,	"ensure"},
    {keyword_end,	"end"},
    {keyword_if,	"if"},
    {keyword_unless,	"unless"},
    {keyword_then,	"then"},
    {keyword_elsif,	"elsif"},
    {keyword_else,	"else"},
    {keyword_case,	"case"},
    {keyword_when,	"when"},
    {keyword_while,	"while"},
    {keyword_until,	"until"},
    {keyword_for,	"for"},
    {keyword_break,	"break"},
    {keyword_next,	"next"},
    {keyword_redo,	"redo"},
    {keyword_retry,	"retry"},
    {keyword_in,	"in"},
    {keyword_do,	"do"},
    {keyword_do_cond,	"do"},
    {keyword_do_block,	"do"},
    {keyword_return,	"return"},
    {keyword_yield,	"yield"},
    {keyword_super,	"super"},
    {keyword_self,	"self"},
    {keyword_nil,	"nil"},
    {keyword_true,	"true"},
    {keyword_false,	"false"},
    {keyword_and,	"and"},
    {keyword_or,	"or"},
    {keyword_not,	"not"},
    {modifier_if,	"if"},
    {modifier_unless,	"unless"},
    {modifier_while,	"while"},
    {modifier_until,	"until"},
    {modifier_rescue,	"rescue"},
    {keyword_alias,	"alias"},
    {keyword_defined,	"defined?"},
    {keyword_BEGIN,	"BEGIN"},
    {keyword_END,	"END"},
    {keyword__LINE__,	"__LINE__"},
    {keyword__FILE__,	"__FILE__"},
    {keyword__ENCODING__, "__ENCODING__"},
    {0, NULL}
};

static const char*
keyword_id_to_str(ID id)
{
    const struct kw_assoc *a;

    for (a = keyword_to_name; a->id; a++) {
        if (a->id == id)
            return a->name;
    }
    return NULL;
}

#undef ripper_id2sym
static VALUE
ripper_id2sym(ID id)
{
    const char *name;
    char buf[8];

    if (id <= 256) {
        buf[0] = (char)id;
        buf[1] = '\0';
        return ID2SYM(rb_intern2(buf, 1));
    }
    if ((name = keyword_id_to_str(id))) {
        return ID2SYM(rb_intern(name));
    }
    switch (id) {
      case tOROP:
        name = "||";
        break;
      case tANDOP:
        name = "&&";
        break;
      default:
        name = rb_id2name(id);
        if (!name) {
            rb_bug("cannot convert ID to string: %ld", (unsigned long)id);
        }
        return ID2SYM(id);
    }
    return ID2SYM(rb_intern(name));
}

static ID
ripper_get_id(VALUE v)
{
    NODE *nd;
    if (!RB_TYPE_P(v, T_NODE)) return 0;
    nd = (NODE *)v;
    if (nd_type(nd) != NODE_LASGN) return 0;
    return nd->nd_vid;
}

static VALUE
ripper_get_value(VALUE v)
{
    NODE *nd;
    if (v == Qundef) return Qnil;
    if (!RB_TYPE_P(v, T_NODE)) return v;
    nd = (NODE *)v;
    if (nd_type(nd) != NODE_LASGN) return Qnil;
    return nd->nd_rval;
}

static void
ripper_compile_error(struct parser_params *parser, const char *fmt, ...)
{
    VALUE str;
    va_list args;

    va_start(args, fmt);
    str = rb_vsprintf(fmt, args);
    va_end(args);
    rb_funcall(parser->value, rb_intern("compile_error"), 1, str);
}

static void
ripper_warn0(struct parser_params *parser, const char *fmt)
{
    rb_funcall(parser->value, rb_intern("warn"), 1, STR_NEW2(fmt));
}

static void
ripper_warnI(struct parser_params *parser, const char *fmt, int a)
{
    rb_funcall(parser->value, rb_intern("warn"), 2,
               STR_NEW2(fmt), INT2NUM(a));
}

static void
ripper_warnS(struct parser_params *parser, const char *fmt, const char *str)
{
    rb_funcall(parser->value, rb_intern("warn"), 2,
               STR_NEW2(fmt), STR_NEW2(str));
}

static void
ripper_warning0(struct parser_params *parser, const char *fmt)
{
    rb_funcall(parser->value, rb_intern("warning"), 1, STR_NEW2(fmt));
}

static void
ripper_warningS(struct parser_params *parser, const char *fmt, const char *str)
{
    rb_funcall(parser->value, rb_intern("warning"), 2,
               STR_NEW2(fmt), STR_NEW2(str));
}

static VALUE
ripper_lex_get_generic(struct parser_params *parser, VALUE src)
{
    return rb_io_gets(src);
}

static VALUE
ripper_s_allocate(VALUE klass)
{
    struct parser_params *p;
    VALUE self;

    p = ALLOC_N(struct parser_params, 1);
    MEMZERO(p, struct parser_params, 1);
    self = TypedData_Wrap_Struct(klass, &parser_data_type, p);
    p->value = self;
    return self;
}

#define ripper_initialized_p(r) ((r)->parser_lex_input != 0)

/*
 *  call-seq:
 *    Ripper.new(src, filename="(ripper)", lineno=1) -> ripper
 *
 *  Create a new Ripper object.
 *  _src_ must be a String, an IO, or an Object which has #gets method.
 *
 *  This method does not starts parsing.
 *  See also Ripper#parse and Ripper.parse.
 */
static VALUE
ripper_initialize(int argc, VALUE *argv, VALUE self)
{
    struct parser_params *parser;
    VALUE src, fname, lineno;

    TypedData_Get_Struct(self, struct parser_params, &parser_data_type, parser);
    rb_scan_args(argc, argv, "12", &src, &fname, &lineno);
    if (RB_TYPE_P(src, T_FILE)) {
        parser->parser_lex_gets = ripper_lex_get_generic;
    }
    else {
        StringValue(src);
        parser->parser_lex_gets = lex_get_str;
    }
    parser->parser_lex_input = src;
    parser->eofp = Qfalse;
    if (NIL_P(fname)) {
        fname = STR_NEW2("(ripper)");
    }
    else {
        StringValue(fname);
    }
    parser_initialize(parser);

    parser->parser_ruby_sourcefile_string = fname;
    parser->parser_ruby_sourcefile = RSTRING_PTR(fname);
    parser->parser_ruby_sourceline = NIL_P(lineno) ? 0 : NUM2INT(lineno) - 1;

    return Qnil;
}

struct ripper_args {
    struct parser_params *parser;
    int argc;
    VALUE *argv;
};

static VALUE
ripper_parse0(VALUE parser_v)
{
    struct parser_params *parser;

    TypedData_Get_Struct(parser_v, struct parser_params, &parser_data_type, parser);
    parser_prepare(parser);
    ripper_yyparse((void*)parser);
    return parser->result;
}

static VALUE
ripper_ensure(VALUE parser_v)
{
    struct parser_params *parser;

    TypedData_Get_Struct(parser_v, struct parser_params, &parser_data_type, parser);
    parser->parsing_thread = Qnil;
    return Qnil;
}

/*
 *  call-seq:
 *    ripper#parse
 *
 *  Start parsing and returns the value of the root action.
 */
static VALUE
ripper_parse(VALUE self)
{
    struct parser_params *parser;

    TypedData_Get_Struct(self, struct parser_params, &parser_data_type, parser);
    if (!ripper_initialized_p(parser)) {
        rb_raise(rb_eArgError, "method called for uninitialized object");
    }
    if (!NIL_P(parser->parsing_thread)) {
        if (parser->parsing_thread == rb_thread_current())
            rb_raise(rb_eArgError, "Ripper#parse is not reentrant");
        else
            rb_raise(rb_eArgError, "Ripper#parse is not multithread-safe");
    }
    parser->parsing_thread = rb_thread_current();
    rb_ensure(ripper_parse0, self, ripper_ensure, self);

    return parser->result;
}

/*
 *  call-seq:
 *    ripper#column   -> Integer
 *
 *  Return column number of current parsing line.
 *  This number starts from 0.
 */
static VALUE
ripper_column(VALUE self)
{
    struct parser_params *parser;
    long col;

    TypedData_Get_Struct(self, struct parser_params, &parser_data_type, parser);
    if (!ripper_initialized_p(parser)) {
        rb_raise(rb_eArgError, "method called for uninitialized object");
    }
    if (NIL_P(parser->parsing_thread)) return Qnil;
    col = parser->tokp - parser->parser_lex_pbeg;
    return LONG2NUM(col);
}

/*
 *  call-seq:
 *    ripper#filename   -> String
 *
 *  Return current parsing filename.
 */
static VALUE
ripper_filename(VALUE self)
{
    struct parser_params *parser;

    TypedData_Get_Struct(self, struct parser_params, &parser_data_type, parser);
    if (!ripper_initialized_p(parser)) {
        rb_raise(rb_eArgError, "method called for uninitialized object");
    }
    return parser->parser_ruby_sourcefile_string;
}

/*
 *  call-seq:
 *    ripper#lineno   -> Integer
 *
 *  Return line number of current parsing line.
 *  This number starts from 1.
 */
static VALUE
ripper_lineno(VALUE self)
{
    struct parser_params *parser;

    TypedData_Get_Struct(self, struct parser_params, &parser_data_type, parser);
    if (!ripper_initialized_p(parser)) {
        rb_raise(rb_eArgError, "method called for uninitialized object");
    }
    if (NIL_P(parser->parsing_thread)) return Qnil;
    return INT2NUM(parser->parser_ruby_sourceline);
}

#ifdef RIPPER_DEBUG
/* :nodoc: */
static VALUE
ripper_assert_Qundef(VALUE self, VALUE obj, VALUE msg)
{
    StringValue(msg);
    if (obj == Qundef) {
        rb_raise(rb_eArgError, "%s", RSTRING_PTR(msg));
    }
    return Qnil;
}

/* :nodoc: */
static VALUE
ripper_value(VALUE self, VALUE obj)
{
    return ULONG2NUM(obj);
}
#endif


void
Init_ripper(void)
{
    parser_data_type.parent = RTYPEDDATA_TYPE(rb_parser_new());

    ripper_init_eventids1();
    ripper_init_eventids2();
    /* ensure existing in symbol table */
    (void)rb_intern("||");
    (void)rb_intern("&&");

    InitVM(ripper);
}

void
InitVM_ripper(void)
{
    VALUE Ripper;

    Ripper = rb_define_class("Ripper", rb_cObject);
    rb_define_const(Ripper, "Version", rb_usascii_str_new2(RIPPER_VERSION));
    rb_define_alloc_func(Ripper, ripper_s_allocate);
    rb_define_method(Ripper, "initialize", ripper_initialize, -1);
    rb_define_method(Ripper, "parse", ripper_parse, 0);
    rb_define_method(Ripper, "column", ripper_column, 0);
    rb_define_method(Ripper, "filename", ripper_filename, 0);
    rb_define_method(Ripper, "lineno", ripper_lineno, 0);
    rb_define_method(Ripper, "end_seen?", rb_parser_end_seen_p, 0);
    rb_define_method(Ripper, "encoding", rb_parser_encoding, 0);
    rb_define_method(Ripper, "yydebug", rb_parser_get_yydebug, 0);
    rb_define_method(Ripper, "yydebug=", rb_parser_set_yydebug, 1);
#ifdef RIPPER_DEBUG
    rb_define_method(rb_mKernel, "assert_Qundef", ripper_assert_Qundef, 2);
    rb_define_method(rb_mKernel, "rawVALUE", ripper_value, 1);
    rb_define_method(rb_mKernel, "validate_object", ripper_validate_object, 1);
#endif

    ripper_init_eventids1_table(Ripper);
    ripper_init_eventids2_table(Ripper);

# if 0
    /* Hack to let RDoc document SCRIPT_LINES__ */

    /*
     * When a Hash is assigned to +SCRIPT_LINES__+ the contents of files loaded
     * after the assignment will be added as an Array of lines with the file
     * name as the key.
     */
    rb_define_global_const("SCRIPT_LINES__", Qnil);
#endif

}
#endif /* RIPPER */

