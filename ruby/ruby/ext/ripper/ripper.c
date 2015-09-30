/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

/* Apple Note: For the avoidance of doubt, Apple elects to distribute this file under the terms of the BSD license. */

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
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 268 of yacc.c  */
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
    stack_type cmdargs;
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
    VALUE parser_ruby_sourcefile_string;
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
#define ruby_sourcefile_string	(parser->parser_ruby_sourcefile_string)
#define current_enc		(parser->enc)
#define yydebug			(parser->parser_yydebug)
#ifdef RIPPER
#else
#define ruby_eval_tree		(parser->parser_eval_tree)
#define ruby_eval_tree_begin	(parser->parser_eval_tree_begin)
#define ruby_debug_lines	(parser->debug_lines)
#define ruby_coverage		(parser->coverage)
#endif

#if YYPURE
static int yylex(void*, void*);
#else
static int yylex(void*);
#endif

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


/* Line 268 of yacc.c  */
#line 747 "parse.c"

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



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 293 of yacc.c  */
#line 691 "ripper.y"

    VALUE val;
    NODE *node;
    ID id;
    int num;
    const struct vtable *vars;



/* Line 293 of yacc.c  */
#line 908 "parse.c"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 920 "parse.c"

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
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
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
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
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
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   10700

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  142
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  199
/* YYNRULES -- Number of rules.  */
#define YYNRULES  620
/* YYNRULES -- Number of states.  */
#define YYNSTATES  1057

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
     886,   887,   888,   894,   898,   902,   905,   909,   913,   915,
     920,   924,   926,   927,   934,   939,   943,   946,   948,   951,
     954,   961,   968,   969,   970,   978,   979,   980,   988,   994,
     999,  1000,  1001,  1011,  1012,  1019,  1020,  1021,  1030,  1031,
    1037,  1038,  1045,  1046,  1047,  1057,  1059,  1061,  1063,  1065,
    1067,  1069,  1071,  1073,  1075,  1077,  1079,  1081,  1083,  1085,
    1087,  1089,  1091,  1093,  1096,  1098,  1100,  1102,  1108,  1110,
    1113,  1115,  1117,  1119,  1123,  1125,  1129,  1131,  1136,  1143,
    1147,  1153,  1156,  1161,  1163,  1167,  1172,  1175,  1178,  1180,
    1183,  1184,  1191,  1200,  1205,  1212,  1217,  1220,  1227,  1230,
    1235,  1242,  1245,  1250,  1253,  1258,  1260,  1262,  1264,  1268,
    1270,  1275,  1277,  1282,  1284,  1288,  1290,  1292,  1293,  1294,
    1295,  1301,  1306,  1308,  1312,  1316,  1317,  1323,  1326,  1331,
    1337,  1343,  1346,  1347,  1353,  1354,  1360,  1364,  1365,  1370,
    1371,  1376,  1379,  1381,  1386,  1387,  1393,  1394,  1400,  1406,
    1408,  1410,  1417,  1419,  1421,  1423,  1425,  1428,  1430,  1433,
    1435,  1437,  1439,  1441,  1443,  1445,  1447,  1450,  1454,  1458,
    1462,  1466,  1470,  1471,  1475,  1477,  1480,  1484,  1488,  1489,
    1493,  1497,  1501,  1505,  1509,  1510,  1514,  1515,  1519,  1520,
    1523,  1524,  1527,  1528,  1531,  1533,  1534,  1538,  1539,  1540,
    1541,  1548,  1550,  1552,  1554,  1556,  1559,  1561,  1563,  1565,
    1567,  1571,  1573,  1575,  1578,  1581,  1583,  1585,  1587,  1589,
    1591,  1593,  1595,  1597,  1599,  1601,  1603,  1605,  1607,  1609,
    1611,  1613,  1615,  1617,  1619,  1620,  1625,  1628,  1632,  1635,
    1640,  1643,  1646,  1648,  1651,  1652,  1659,  1668,  1673,  1680,
    1685,  1692,  1695,  1700,  1707,  1710,  1715,  1718,  1723,  1725,
    1726,  1728,  1730,  1732,  1734,  1736,  1738,  1740,  1744,  1746,
    1750,  1753,  1756,  1758,  1762,  1764,  1768,  1770,  1772,  1775,
    1777,  1781,  1785,  1787,  1791,  1793,  1797,  1799,  1801,  1804,
    1806,  1808,  1810,  1813,  1816,  1818,  1820,  1821,  1826,  1828,
    1831,  1833,  1837,  1841,  1844,  1847,  1849,  1851,  1853,  1855,
    1857,  1859,  1861,  1863,  1865,  1867,  1869,  1871,  1872,  1874,
    1875,  1877,  1880,  1883,  1884,  1886,  1888,  1890,  1892,  1894,
    1897
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     143,     0,    -1,    -1,   144,   145,    -1,   146,   333,    -1,
     340,    -1,   147,    -1,   146,   339,   147,    -1,     1,   147,
      -1,   154,    -1,    -1,    47,   148,   130,   145,   131,    -1,
     150,   262,   230,   265,    -1,   151,   333,    -1,   340,    -1,
     152,    -1,   151,   339,   152,    -1,     1,   154,    -1,   154,
      -1,    -1,    47,   153,   130,   145,   131,    -1,    -1,    45,
     177,   155,   177,    -1,    45,    54,    54,    -1,    45,    54,
      64,    -1,    45,    54,    63,    -1,     6,   178,    -1,   154,
      40,   158,    -1,   154,    41,   158,    -1,   154,    42,   158,
      -1,   154,    43,   158,    -1,   154,    44,   154,    -1,    48,
     130,   150,   131,    -1,   156,    -1,   165,   113,   159,    -1,
     297,    87,   159,    -1,   215,   132,   188,   336,    87,   159,
      -1,   215,   133,    52,    87,   159,    -1,   215,   133,    56,
      87,   159,    -1,   215,    85,    56,    87,   159,    -1,   215,
      85,    52,    87,   159,    -1,   298,    87,   159,    -1,   172,
     113,   195,    -1,   165,   113,   184,    -1,   165,   113,   195,
      -1,   157,    -1,   172,   113,   159,    -1,   172,   113,   156,
      -1,   159,    -1,   157,    37,   157,    -1,   157,    38,   157,
      -1,    39,   334,   157,    -1,   127,   159,    -1,   182,    -1,
     157,    -1,   164,    -1,   160,    -1,   251,    -1,   251,   332,
     330,   190,    -1,    -1,    94,   162,   238,   150,   131,    -1,
     329,    -1,   163,   190,    -1,   163,   190,   161,    -1,   215,
     133,   330,   190,    -1,   215,   133,   330,   190,   161,    -1,
     215,    85,   330,   190,    -1,   215,    85,   330,   190,   161,
      -1,    32,   190,    -1,    31,   190,    -1,    30,   189,    -1,
      21,   189,    -1,    22,   189,    -1,   167,    -1,    89,   166,
     335,    -1,   167,    -1,    89,   166,   335,    -1,   169,    -1,
     169,   168,    -1,   169,    95,   171,    -1,   169,    95,   171,
     134,   170,    -1,   169,    95,    -1,   169,    95,   134,   170,
      -1,    95,   171,    -1,    95,   171,   134,   170,    -1,    95,
      -1,    95,   134,   170,    -1,   171,    -1,    89,   166,   335,
      -1,   168,   134,    -1,   169,   168,   134,    -1,   168,    -1,
     170,   134,   168,    -1,   294,    -1,   295,    -1,   215,   132,
     188,   336,    -1,   215,   133,    52,    -1,   215,    85,    52,
      -1,   215,   133,    56,    -1,   215,    85,    56,    -1,    86,
      56,    -1,   298,    -1,   294,    -1,   295,    -1,   215,   132,
     188,   336,    -1,   215,   133,    52,    -1,   215,    85,    52,
      -1,   215,   133,    56,    -1,   215,    85,    56,    -1,    86,
      56,    -1,   298,    -1,    52,    -1,    56,    -1,    86,   173,
      -1,   173,    -1,   215,    85,   173,    -1,    52,    -1,    56,
      -1,    53,    -1,   180,    -1,   181,    -1,   175,    -1,   290,
      -1,   176,    -1,   292,    -1,   177,    -1,    -1,   178,   134,
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
     113,   182,    44,   182,    -1,   297,    87,   182,    -1,   297,
      87,   182,    44,   182,    -1,   215,   132,   188,   336,    87,
     182,    -1,   215,   133,    52,    87,   182,    -1,   215,   133,
      56,    87,   182,    -1,   215,    85,    52,    87,   182,    -1,
     215,    85,    56,    87,   182,    -1,    86,    56,    87,   182,
      -1,   298,    87,   182,    -1,   182,    79,   182,    -1,   182,
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
      75,   182,    -1,   182,    76,   182,    -1,    -1,    46,   334,
     183,   182,    -1,   182,   114,   182,   334,   115,   182,    -1,
     196,    -1,   182,    -1,   340,    -1,   194,   337,    -1,   194,
     134,   327,   337,    -1,   327,   337,    -1,   136,   188,   335,
      -1,   340,    -1,   186,    -1,   340,    -1,   189,    -1,   194,
     134,    -1,   194,   134,   327,   134,    -1,   327,   134,    -1,
     164,    -1,   194,   193,    -1,   327,   193,    -1,   194,   134,
     327,   193,    -1,   192,    -1,    -1,   191,   189,    -1,    97,
     184,    -1,   134,   192,    -1,   340,    -1,   184,    -1,    95,
     184,    -1,   194,   134,   184,    -1,   194,   134,    95,   184,
      -1,   194,   134,   184,    -1,   194,   134,    95,   184,    -1,
      95,   184,    -1,   266,    -1,   267,    -1,   270,    -1,   271,
      -1,   272,    -1,   277,    -1,   275,    -1,   278,    -1,   296,
      -1,   298,    -1,    53,    -1,    -1,   216,   197,   149,   226,
      -1,    -1,    90,   198,   335,    -1,    -1,    -1,    90,   199,
     157,   200,   335,    -1,    89,   150,   137,    -1,   215,    85,
      56,    -1,    86,    56,    -1,    92,   185,   138,    -1,    93,
     326,   131,    -1,    30,    -1,    31,   136,   189,   335,    -1,
      31,   136,   335,    -1,    31,    -1,    -1,    46,   334,   136,
     201,   157,   335,    -1,    39,   136,   157,   335,    -1,    39,
     136,   335,    -1,   163,   257,    -1,   252,    -1,   252,   257,
      -1,    98,   243,    -1,   217,   158,   227,   150,   229,   226,
      -1,   218,   158,   227,   150,   230,   226,    -1,    -1,    -1,
     219,   202,   158,   228,   203,   150,   226,    -1,    -1,    -1,
     220,   204,   158,   228,   205,   150,   226,    -1,   221,   158,
     333,   260,   226,    -1,   221,   333,   260,   226,    -1,    -1,
      -1,   222,   231,    25,   206,   158,   228,   207,   150,   226,
      -1,    -1,   223,   174,   299,   208,   149,   226,    -1,    -1,
      -1,   223,    83,   157,   209,   338,   210,   149,   226,    -1,
      -1,   224,   174,   211,   149,   226,    -1,    -1,   225,   175,
     212,   301,   149,   226,    -1,    -1,    -1,   225,   324,   332,
     213,   175,   214,   301,   149,   226,    -1,    21,    -1,    22,
      -1,    23,    -1,    24,    -1,   196,    -1,     7,    -1,    11,
      -1,    12,    -1,    18,    -1,    19,    -1,    16,    -1,    20,
      -1,     3,    -1,     4,    -1,     5,    -1,    10,    -1,   338,
      -1,    13,    -1,   338,    13,    -1,   338,    -1,    27,    -1,
     230,    -1,    14,   158,   227,   150,   229,    -1,   340,    -1,
      15,   150,    -1,   172,    -1,   165,    -1,   306,    -1,    89,
     234,   335,    -1,   232,    -1,   233,   134,   232,    -1,   233,
      -1,   233,   134,    95,   306,    -1,   233,   134,    95,   306,
     134,   233,    -1,   233,   134,    95,    -1,   233,   134,    95,
     134,   233,    -1,    95,   306,    -1,    95,   306,   134,   233,
      -1,    95,    -1,    95,   134,   233,    -1,   311,   134,   314,
     323,    -1,   311,   323,    -1,   314,   323,    -1,   322,    -1,
     134,   235,    -1,    -1,   308,   134,   317,   134,   320,   236,
      -1,   308,   134,   317,   134,   320,   134,   308,   236,    -1,
     308,   134,   317,   236,    -1,   308,   134,   317,   134,   308,
     236,    -1,   308,   134,   320,   236,    -1,   308,   134,    -1,
     308,   134,   320,   134,   308,   236,    -1,   308,   236,    -1,
     317,   134,   320,   236,    -1,   317,   134,   320,   134,   308,
     236,    -1,   317,   236,    -1,   317,   134,   308,   236,    -1,
     320,   236,    -1,   320,   134,   308,   236,    -1,   235,    -1,
     340,    -1,   239,    -1,   118,   240,   118,    -1,    76,    -1,
     118,   237,   240,   118,    -1,   334,    -1,   334,   139,   241,
     334,    -1,   242,    -1,   241,   134,   242,    -1,    52,    -1,
     305,    -1,    -1,    -1,    -1,   244,   245,   247,   246,   248,
      -1,   136,   304,   240,   137,    -1,   304,    -1,   111,   150,
     131,    -1,    29,   150,    10,    -1,    -1,    28,   250,   238,
     150,    10,    -1,   164,   249,    -1,   251,   332,   330,   187,
      -1,   251,   332,   330,   187,   257,    -1,   251,   332,   330,
     190,   249,    -1,   163,   186,    -1,    -1,   215,   133,   330,
     253,   187,    -1,    -1,   215,    85,   330,   254,   186,    -1,
     215,    85,   331,    -1,    -1,   215,   133,   255,   186,    -1,
      -1,   215,    85,   256,   186,    -1,    32,   186,    -1,    32,
      -1,   215,   132,   188,   336,    -1,    -1,   130,   258,   238,
     150,   131,    -1,    -1,    26,   259,   238,   150,    10,    -1,
      17,   194,   227,   150,   261,    -1,   230,    -1,   260,    -1,
       8,   263,   264,   227,   150,   262,    -1,   340,    -1,   184,
      -1,   195,    -1,   340,    -1,    88,   172,    -1,   340,    -1,
       9,   150,    -1,   340,    -1,   293,    -1,   290,    -1,   292,
      -1,   268,    -1,    62,    -1,   269,    -1,   268,   269,    -1,
     100,   281,   110,    -1,   101,   282,   110,    -1,   102,   283,
      65,    -1,   103,   140,   110,    -1,   103,   273,   110,    -1,
      -1,   273,   274,   140,    -1,   284,    -1,   274,   284,    -1,
     105,   140,   110,    -1,   105,   276,   110,    -1,    -1,   276,
     274,   140,    -1,   104,   140,   110,    -1,   104,   279,   110,
      -1,   106,   140,   110,    -1,   106,   280,   110,    -1,    -1,
     279,    61,   140,    -1,    -1,   280,    61,   140,    -1,    -1,
     281,   284,    -1,    -1,   282,   284,    -1,    -1,   283,   284,
      -1,    61,    -1,    -1,   109,   285,   289,    -1,    -1,    -1,
      -1,   107,   286,   287,   288,   150,   108,    -1,    54,    -1,
      55,    -1,    57,    -1,   298,    -1,    99,   291,    -1,   175,
      -1,    55,    -1,    54,    -1,    57,    -1,    99,   282,   110,
      -1,    59,    -1,    60,    -1,   126,    59,    -1,   126,    60,
      -1,    52,    -1,    55,    -1,    54,    -1,    56,    -1,    57,
      -1,    34,    -1,    33,    -1,    35,    -1,    36,    -1,    50,
      -1,    49,    -1,    51,    -1,   294,    -1,   295,    -1,   294,
      -1,   295,    -1,    63,    -1,    64,    -1,   338,    -1,    -1,
     117,   300,   158,   338,    -1,     1,   338,    -1,   136,   304,
     335,    -1,   304,   338,    -1,   312,   134,   314,   323,    -1,
     312,   323,    -1,   314,   323,    -1,   322,    -1,   134,   302,
      -1,    -1,   308,   134,   318,   134,   320,   303,    -1,   308,
     134,   318,   134,   320,   134,   308,   303,    -1,   308,   134,
     318,   303,    -1,   308,   134,   318,   134,   308,   303,    -1,
     308,   134,   320,   303,    -1,   308,   134,   320,   134,   308,
     303,    -1,   308,   303,    -1,   318,   134,   320,   303,    -1,
     318,   134,   320,   134,   308,   303,    -1,   318,   303,    -1,
     318,   134,   308,   303,    -1,   320,   303,    -1,   320,   134,
     308,   303,    -1,   302,    -1,    -1,    56,    -1,    55,    -1,
      54,    -1,    57,    -1,   305,    -1,    52,    -1,   306,    -1,
      89,   234,   335,    -1,   307,    -1,   308,   134,   307,    -1,
      58,   184,    -1,    58,   215,    -1,   310,    -1,   311,   134,
     310,    -1,   309,    -1,   312,   134,   309,    -1,    68,    -1,
      96,    -1,   313,    52,    -1,   313,    -1,    52,   113,   184,
      -1,    52,   113,   215,    -1,   316,    -1,   317,   134,   316,
      -1,   315,    -1,   318,   134,   315,    -1,   123,    -1,    95,
      -1,   319,    52,    -1,   319,    -1,   120,    -1,    97,    -1,
     321,    52,    -1,   134,   322,    -1,   340,    -1,   296,    -1,
      -1,   136,   325,   157,   335,    -1,   340,    -1,   327,   337,
      -1,   328,    -1,   327,   134,   328,    -1,   184,    88,   184,
      -1,    58,   184,    -1,    96,   184,    -1,    52,    -1,    56,
      -1,    53,    -1,    52,    -1,    56,    -1,    53,    -1,   180,
      -1,    52,    -1,    53,    -1,   180,    -1,   133,    -1,    85,
      -1,    -1,   339,    -1,    -1,   141,    -1,   334,   137,    -1,
     334,   138,    -1,    -1,   141,    -1,   134,    -1,   139,    -1,
     141,    -1,   338,    -1,   339,   139,    -1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   855,   855,   855,   886,   897,   906,   914,   922,   928,
     930,   929,   950,   983,   994,  1003,  1011,  1019,  1025,  1030,
    1029,  1050,  1050,  1058,  1066,  1077,  1087,  1095,  1104,  1113,
    1126,  1139,  1148,  1160,  1161,  1171,  1176,  1197,  1202,  1207,
    1217,  1222,  1232,  1241,  1250,  1259,  1262,  1271,  1283,  1284,
    1292,  1300,  1308,  1316,  1319,  1331,  1332,  1335,  1336,  1348,
    1347,  1369,  1379,  1388,  1401,  1410,  1422,  1431,  1443,  1452,
    1461,  1469,  1477,  1487,  1488,  1498,  1499,  1509,  1517,  1525,
    1533,  1542,  1550,  1559,  1567,  1576,  1584,  1595,  1596,  1606,
    1614,  1624,  1632,  1642,  1646,  1650,  1658,  1666,  1674,  1682,
    1694,  1704,  1716,  1725,  1734,  1742,  1750,  1758,  1766,  1779,
    1792,  1803,  1811,  1814,  1822,  1830,  1840,  1841,  1842,  1843,
    1848,  1859,  1860,  1863,  1871,  1874,  1882,  1882,  1892,  1893,
    1894,  1895,  1896,  1897,  1898,  1899,  1900,  1901,  1902,  1903,
    1904,  1905,  1906,  1907,  1908,  1909,  1910,  1911,  1912,  1913,
    1914,  1915,  1916,  1917,  1918,  1919,  1920,  1921,  1924,  1924,
    1924,  1925,  1925,  1926,  1926,  1926,  1927,  1927,  1927,  1927,
    1928,  1928,  1928,  1928,  1929,  1929,  1929,  1930,  1930,  1930,
    1930,  1931,  1931,  1931,  1931,  1932,  1932,  1932,  1932,  1933,
    1933,  1933,  1933,  1934,  1934,  1934,  1934,  1935,  1935,  1938,
    1947,  1957,  1962,  1972,  1998,  2003,  2008,  2013,  2023,  2033,
    2044,  2058,  2072,  2080,  2088,  2096,  2104,  2112,  2120,  2129,
    2138,  2146,  2154,  2162,  2170,  2178,  2186,  2194,  2202,  2210,
    2218,  2226,  2234,  2242,  2253,  2261,  2269,  2277,  2285,  2293,
    2301,  2309,  2309,  2319,  2329,  2335,  2347,  2348,  2352,  2360,
    2370,  2380,  2381,  2384,  2385,  2386,  2390,  2398,  2408,  2417,
    2425,  2435,  2444,  2453,  2453,  2465,  2475,  2479,  2485,  2493,
    2501,  2515,  2531,  2545,  2560,  2570,  2571,  2572,  2573,  2574,
    2575,  2576,  2577,  2578,  2579,  2580,  2589,  2588,  2616,  2616,
    2625,  2629,  2624,  2638,  2646,  2654,  2662,  2675,  2683,  2691,
    2699,  2707,  2715,  2715,  2725,  2733,  2741,  2751,  2752,  2762,
    2766,  2778,  2790,  2790,  2790,  2801,  2801,  2801,  2812,  2823,
    2832,  2834,  2831,  2898,  2897,  2919,  2924,  2918,  2943,  2942,
    2964,  2963,  2986,  2987,  2986,  3007,  3015,  3023,  3031,  3041,
    3053,  3059,  3065,  3071,  3077,  3083,  3089,  3095,  3101,  3107,
    3117,  3123,  3128,  3129,  3136,  3141,  3144,  3145,  3158,  3159,
    3169,  3170,  3173,  3181,  3191,  3199,  3209,  3217,  3226,  3235,
    3243,  3251,  3260,  3272,  3280,  3291,  3295,  3299,  3303,  3309,
    3314,  3319,  3323,  3327,  3331,  3335,  3339,  3347,  3351,  3355,
    3359,  3363,  3367,  3371,  3375,  3379,  3385,  3386,  3392,  3401,
    3410,  3421,  3425,  3435,  3442,  3451,  3459,  3465,  3468,  3473,
    3465,  3489,  3497,  3507,  3511,  3518,  3517,  3538,  3554,  3563,
    3575,  3589,  3599,  3598,  3615,  3614,  3630,  3639,  3638,  3656,
    3655,  3672,  3680,  3688,  3703,  3702,  3722,  3721,  3742,  3754,
    3755,  3758,  3777,  3780,  3788,  3796,  3799,  3803,  3806,  3814,
    3817,  3818,  3826,  3829,  3846,  3847,  3848,  3858,  3868,  3895,
    3960,  3969,  3980,  3987,  3997,  4005,  4015,  4024,  4035,  4042,
    4060,  4069,  4079,  4088,  4099,  4106,  4117,  4124,  4139,  4146,
    4157,  4164,  4175,  4182,  4211,  4213,  4212,  4229,  4235,  4240,
    4228,  4259,  4267,  4275,  4283,  4286,  4297,  4298,  4299,  4300,
    4303,  4314,  4315,  4316,  4324,  4334,  4335,  4336,  4337,  4338,
    4341,  4342,  4343,  4344,  4345,  4346,  4347,  4350,  4363,  4373,
    4381,  4391,  4392,  4395,  4404,  4403,  4412,  4424,  4434,  4442,
    4446,  4450,  4454,  4460,  4465,  4470,  4474,  4478,  4482,  4486,
    4490,  4494,  4498,  4502,  4506,  4510,  4514,  4518,  4522,  4527,
    4533,  4542,  4551,  4560,  4571,  4572,  4579,  4588,  4607,  4614,
    4627,  4639,  4651,  4659,  4676,  4684,  4700,  4701,  4704,  4709,
    4715,  4727,  4739,  4747,  4763,  4771,  4787,  4788,  4791,  4804,
    4815,  4816,  4819,  4836,  4840,  4850,  4860,  4860,  4889,  4890,
    4900,  4907,  4917,  4925,  4933,  4945,  4946,  4947,  4950,  4951,
    4952,  4953,  4956,  4957,  4958,  4961,  4966,  4973,  4974,  4977,
    4978,  4981,  4984,  4987,  4988,  4989,  4992,  4993,  4996,  4997,
    5001
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
  "' '", "'\\n'", "$accept", "program", "$@1", "top_compstmt", "top_stmts",
  "top_stmt", "$@2", "bodystmt", "compstmt", "stmts", "stmt_or_begin",
  "$@3", "stmt", "$@4", "command_asgn", "expr", "expr_value",
  "command_call", "block_command", "cmd_brace_block", "@5", "fcall",
  "command", "mlhs", "mlhs_inner", "mlhs_basic", "mlhs_item", "mlhs_head",
  "mlhs_post", "mlhs_node", "lhs", "cname", "cpath", "fname", "fsym",
  "fitem", "undef_list", "$@6", "op", "reswords", "arg", "$@7",
  "arg_value", "aref_args", "paren_args", "opt_paren_args",
  "opt_call_args", "call_args", "command_args", "@8", "block_arg",
  "opt_block_arg", "args", "mrhs", "primary", "@9", "$@10", "$@11", "$@12",
  "$@13", "$@14", "$@15", "$@16", "$@17", "$@18", "$@19", "@20", "@21",
  "@22", "@23", "@24", "$@25", "$@26", "primary_value", "k_begin", "k_if",
  "k_unless", "k_while", "k_until", "k_case", "k_for", "k_class",
  "k_module", "k_def", "k_end", "then", "do", "if_tail", "opt_else",
  "for_var", "f_marg", "f_marg_list", "f_margs", "block_args_tail",
  "opt_block_args_tail", "block_param", "opt_block_param",
  "block_param_def", "opt_bv_decl", "bv_decls", "bvar", "lambda", "@27",
  "@28", "@29", "f_larglist", "lambda_body", "do_block", "@30",
  "block_call", "method_call", "@31", "@32", "@33", "@34", "brace_block",
  "@35", "@36", "case_body", "cases", "opt_rescue", "exc_list", "exc_var",
  "opt_ensure", "literal", "strings", "string", "string1", "xstring",
  "regexp", "words", "word_list", "word", "symbols", "symbol_list",
  "qwords", "qsymbols", "qword_list", "qsym_list", "string_contents",
  "xstring_contents", "regexp_contents", "string_content", "@37", "@38",
  "@39", "@40", "string_dvar", "symbol", "sym", "dsym", "numeric",
  "user_variable", "keyword_variable", "var_ref", "var_lhs", "backref",
  "superclass", "$@41", "f_arglist", "args_tail", "opt_args_tail",
  "f_args", "f_bad_arg", "f_norm_arg", "f_arg_item", "f_arg", "f_kw",
  "f_block_kw", "f_block_kwarg", "f_kwarg", "kwrest_mark", "f_kwrest",
  "f_opt", "f_block_opt", "f_block_optarg", "f_optarg", "restarg_mark",
  "f_rest_arg", "blkarg_mark", "f_block_arg", "opt_f_block_arg",
  "singleton", "$@42", "assoc_list", "assocs", "assoc", "operation",
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
     199,   200,   196,   196,   196,   196,   196,   196,   196,   196,
     196,   196,   201,   196,   196,   196,   196,   196,   196,   196,
     196,   196,   202,   203,   196,   204,   205,   196,   196,   196,
     206,   207,   196,   208,   196,   209,   210,   196,   211,   196,
     212,   196,   213,   214,   196,   196,   196,   196,   196,   215,
     216,   217,   218,   219,   220,   221,   222,   223,   224,   225,
     226,   227,   227,   227,   228,   228,   229,   229,   230,   230,
     231,   231,   232,   232,   233,   233,   234,   234,   234,   234,
     234,   234,   234,   234,   234,   235,   235,   235,   235,   236,
     236,   237,   237,   237,   237,   237,   237,   237,   237,   237,
     237,   237,   237,   237,   237,   237,   238,   238,   239,   239,
     239,   240,   240,   241,   241,   242,   242,   244,   245,   246,
     243,   247,   247,   248,   248,   250,   249,   251,   251,   251,
     251,   252,   253,   252,   254,   252,   252,   255,   252,   256,
     252,   252,   252,   252,   258,   257,   259,   257,   260,   261,
     261,   262,   262,   263,   263,   263,   264,   264,   265,   265,
     266,   266,   266,   267,   268,   268,   268,   269,   270,   271,
     272,   272,   273,   273,   274,   274,   275,   275,   276,   276,
     277,   277,   278,   278,   279,   279,   280,   280,   281,   281,
     282,   282,   283,   283,   284,   285,   284,   286,   287,   288,
     284,   289,   289,   289,   289,   290,   291,   291,   291,   291,
     292,   293,   293,   293,   293,   294,   294,   294,   294,   294,
     295,   295,   295,   295,   295,   295,   295,   296,   296,   297,
     297,   298,   298,   299,   300,   299,   299,   301,   301,   302,
     302,   302,   302,   303,   303,   304,   304,   304,   304,   304,
     304,   304,   304,   304,   304,   304,   304,   304,   304,   304,
     305,   305,   305,   305,   306,   306,   307,   307,   308,   308,
     309,   310,   311,   311,   312,   312,   313,   313,   314,   314,
     315,   316,   317,   317,   318,   318,   319,   319,   320,   320,
     321,   321,   322,   323,   323,   324,   325,   324,   326,   326,
     327,   327,   328,   328,   328,   329,   329,   329,   330,   330,
     330,   330,   331,   331,   331,   332,   332,   333,   333,   334,
     334,   335,   336,   337,   337,   337,   338,   338,   339,   339,
     340
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
       0,     0,     5,     3,     3,     2,     3,     3,     1,     4,
       3,     1,     0,     6,     4,     3,     2,     1,     2,     2,
       6,     6,     0,     0,     7,     0,     0,     7,     5,     4,
       0,     0,     9,     0,     6,     0,     0,     8,     0,     5,
       0,     6,     0,     0,     9,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     2,     1,     1,     1,     5,     1,     2,
       1,     1,     1,     3,     1,     3,     1,     4,     6,     3,
       5,     2,     4,     1,     3,     4,     2,     2,     1,     2,
       0,     6,     8,     4,     6,     4,     2,     6,     2,     4,
       6,     2,     4,     2,     4,     1,     1,     1,     3,     1,
       4,     1,     4,     1,     3,     1,     1,     0,     0,     0,
       5,     4,     1,     3,     3,     0,     5,     2,     4,     5,
       5,     2,     0,     5,     0,     5,     3,     0,     4,     0,
       4,     2,     1,     4,     0,     5,     0,     5,     5,     1,
       1,     6,     1,     1,     1,     1,     2,     1,     2,     1,
       1,     1,     1,     1,     1,     1,     2,     3,     3,     3,
       3,     3,     0,     3,     1,     2,     3,     3,     0,     3,
       3,     3,     3,     3,     0,     3,     0,     3,     0,     2,
       0,     2,     0,     2,     1,     0,     3,     0,     0,     0,
       6,     1,     1,     1,     1,     2,     1,     1,     1,     1,
       3,     1,     1,     2,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     0,     4,     2,     3,     2,     4,
       2,     2,     1,     2,     0,     6,     8,     4,     6,     4,
       6,     2,     4,     6,     2,     4,     2,     4,     1,     0,
       1,     1,     1,     1,     1,     1,     1,     3,     1,     3,
       2,     2,     1,     3,     1,     3,     1,     1,     2,     1,
       3,     3,     1,     3,     1,     3,     1,     1,     2,     1,
       1,     1,     2,     2,     1,     1,     0,     4,     1,     2,
       1,     3,     3,     2,     2,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     0,     1,     0,
       1,     2,     2,     0,     1,     1,     1,     1,     1,     2,
       0
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       2,     0,     0,     1,     0,   347,   348,   349,     0,   340,
     341,   342,   345,   343,   344,   346,   335,   336,   337,   338,
     298,   263,   263,   511,   510,   512,   513,   609,     0,   609,
      10,     0,   515,   514,   516,   595,   597,   507,   506,   596,
     509,   501,   502,   454,   521,   522,     0,     0,     0,     0,
     290,   620,   620,    85,   407,   480,   478,   480,   482,   462,
     474,   468,   476,     0,     0,     0,     3,   607,     6,     9,
      33,    45,    48,    56,   263,    55,     0,    73,     0,    77,
      87,     0,    53,   244,     0,   286,     0,     0,   312,   315,
     607,     0,     0,     0,     0,    57,   307,   275,   276,   453,
     455,   277,   278,   279,   281,   280,   282,   451,   452,   450,
     517,   518,   283,     0,   284,    61,     5,     8,   168,   179,
     169,   192,   165,   185,   175,   174,   195,   196,   190,   173,
     172,   167,   193,   197,   198,   177,   166,   180,   184,   186,
     178,   171,   187,   194,   189,   188,   181,   191,   176,   164,
     183,   182,   163,   170,   161,   162,   158,   159,   160,   116,
     118,   117,   153,   154,   149,   131,   132,   133,   140,   137,
     139,   134,   135,   155,   156,   141,   142,   146,   150,   136,
     138,   128,   129,   130,   143,   144,   145,   147,   148,   151,
     152,   157,   121,   123,   125,    26,   119,   120,   122,   124,
       0,     0,     0,     0,     0,     0,     0,     0,   258,     0,
     245,   268,    71,   262,   620,     0,   517,   518,     0,   284,
     620,   590,    72,    70,   609,    69,     0,   620,   431,    68,
     609,   610,     0,     0,    21,   241,     0,     0,   335,   336,
     298,   301,   432,     0,   220,     0,   221,   295,     0,    19,
       0,     0,   607,    15,    18,   609,    75,    14,   609,     0,
       0,   613,   613,   246,     0,     0,   613,   588,   609,     0,
       0,     0,    83,   339,     0,    93,    94,   101,   309,   408,
     498,   497,   499,   496,     0,   495,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   503,   504,    52,
     235,   236,   616,   617,     4,   618,   608,     0,     0,     0,
       0,     0,     0,     0,   436,   434,   421,    62,   306,   415,
     417,     0,    89,     0,    81,    78,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   429,   620,   427,     0,    54,     0,     0,
       0,     0,   607,     0,   608,     0,   361,   360,     0,     0,
     517,   518,   284,   111,   112,     0,     0,   114,     0,     0,
     517,   518,   284,   328,   188,   181,   191,   176,   158,   159,
     160,   116,   117,   586,   330,   585,     0,   606,   605,     0,
     308,   456,     0,     0,   126,   593,   295,   269,   594,   265,
       0,     0,     0,   259,   267,   429,   620,   427,     0,     0,
       0,   260,   609,     0,   300,   264,   609,   254,   620,   620,
     253,   609,   305,    51,    23,    25,    24,     0,   302,     0,
       0,     0,   429,   427,     0,    17,     0,   609,   293,    13,
     608,    74,   289,   291,   296,   615,   614,   247,   615,   249,
     297,   589,     0,   100,   503,   504,    91,    86,     0,   429,
     620,   427,   549,   484,   487,   485,   500,   481,   457,   479,
     458,   459,   483,   460,   461,     0,   464,   470,     0,   471,
     466,   467,     0,   472,     0,   473,     0,     0,   619,     7,
      27,    28,    29,    30,    31,    49,    50,   620,   620,    59,
      63,   620,     0,    34,    43,     0,    44,   609,     0,    79,
      90,    47,    46,     0,   199,   268,    42,   217,   225,   230,
     231,   232,   227,   229,   239,   240,   233,   234,   210,   211,
     237,   238,   609,   226,   228,   222,   223,   224,   212,   213,
     214,   215,   216,   598,   600,   599,   601,     0,   263,   426,
     609,   598,   600,   599,   601,     0,   263,     0,   620,   352,
       0,   351,     0,     0,     0,     0,     0,     0,   295,   429,
     620,   427,   320,   325,   111,   112,   113,     0,   524,   323,
     523,   429,   620,   427,     0,     0,   549,   332,   598,   599,
     263,    35,   201,    41,   209,     0,   199,   592,     0,   270,
     266,   620,   598,   599,   609,   598,   599,   591,   299,   611,
     250,   255,   257,   304,    22,     0,   242,     0,    32,   424,
     422,   208,     0,    76,    16,   609,   613,     0,    84,    97,
      99,   609,   598,   599,   555,   552,   551,   550,   553,     0,
     566,     0,   577,   567,   581,   580,   576,   549,   409,   548,
     412,   554,   556,   558,   534,   564,   620,   569,   620,   574,
     534,   579,   534,     0,   532,   488,     0,   463,   465,   475,
     469,   477,   218,   219,   399,   609,     0,   397,   396,     0,
     620,     0,   274,     0,    88,    82,     0,     0,     0,     0,
       0,     0,   430,    66,     0,     0,   433,     0,     0,   428,
      64,   620,   350,   287,   620,   620,   442,   620,   353,   620,
     355,   313,   354,   316,     0,     0,   319,   602,   294,   609,
     598,   599,     0,     0,   526,     0,     0,   111,   112,   115,
     609,     0,   609,   549,     0,     0,     0,   252,   418,    58,
     251,     0,   127,   271,   261,     0,     0,   433,     0,     0,
     620,   609,    11,     0,   292,   248,    92,    95,     0,   560,
     555,     0,   373,   364,   366,   609,   362,   609,     0,     0,
     541,     0,   530,   584,   568,     0,   531,     0,   544,   578,
       0,   546,   582,   489,   491,   492,   493,   486,   494,   555,
       0,   395,   609,     0,   380,   562,   620,   620,   572,   380,
     380,   378,   401,     0,     0,     0,     0,     0,   272,    80,
     200,     0,    40,   206,    39,   207,    67,   425,   612,     0,
      37,   204,    38,   205,    65,   423,   443,   444,   620,   445,
       0,   620,   358,     0,     0,   356,     0,     0,     0,   318,
       0,     0,   433,     0,   326,     0,     0,   433,   329,   587,
     609,     0,   528,   333,   419,   420,   202,     0,   256,   303,
      20,   570,   609,     0,   371,     0,   557,     0,     0,     0,
     410,   533,   559,   534,   534,   565,   620,   583,   534,   575,
     534,   534,     0,     0,     0,   561,     0,   398,   386,   388,
       0,   376,   377,     0,   391,     0,   393,     0,   437,   435,
       0,   416,   273,   243,    36,   203,     0,     0,   447,   359,
       0,    12,   449,     0,   310,   311,     0,     0,   270,   620,
     321,     0,   525,   324,   527,   331,   549,   363,   374,     0,
     369,   365,   411,     0,     0,     0,   537,     0,   539,   529,
       0,   545,     0,   542,   547,     0,   571,   295,   429,   400,
     379,   380,   380,   563,   620,   380,   573,   380,   380,   405,
     609,   403,   406,    60,     0,   446,     0,   102,   103,   110,
       0,   448,     0,   314,   317,   439,   440,   438,     0,     0,
       0,     0,   372,     0,   367,   414,   413,   534,   534,   534,
     534,   490,   602,   294,     0,   383,     0,   385,   375,     0,
     392,     0,   389,   394,     0,   402,   109,   429,   620,   427,
     620,   620,     0,   327,     0,   370,     0,   538,     0,   535,
     540,   543,   380,   380,   380,   380,   404,   602,   108,   609,
     598,   599,   441,   357,   322,   334,   368,   534,   384,     0,
     381,   387,   390,   433,   536,   380,   382
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    66,    67,    68,   236,   567,   568,   252,
     253,   446,   254,   437,    70,    71,   358,    72,    73,   510,
     690,   243,    75,    76,   255,    77,    78,    79,   467,    80,
     209,   377,   378,   192,   193,   194,   195,   605,   556,   197,
      82,   439,   211,   260,   228,   748,   426,   427,   225,   226,
     213,   413,   428,   516,    83,   356,   258,   259,   635,   625,
     360,   847,   361,   848,   732,   988,   736,   733,   931,   594,
     596,   746,   936,   245,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,   713,   570,   721,   844,   845,   369,
     773,   774,   775,   960,   899,   802,   686,   687,   803,   970,
     971,   278,   279,   472,   778,   658,   880,   320,   511,    95,
      96,   711,   704,   565,   557,   318,   508,   507,   577,   987,
     715,   838,   917,   921,    97,    98,    99,   100,   101,   102,
     103,   290,   485,   104,   294,   105,   106,   292,   296,   286,
     284,   288,   477,   676,   675,   793,   892,   797,   107,   285,
     108,   109,   216,   217,   112,   218,   219,   589,   735,   744,
     881,   780,   745,   661,   662,   663,   664,   665,   805,   806,
     666,   667,   668,   669,   808,   809,   670,   671,   672,   673,
     674,   782,   396,   595,   265,   429,   221,   115,   629,   559,
     399,   304,   423,   424,   706,   457,   571,   364,   257
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -808
static const yytype_int16 yypact[] =
{
    -808,   134,  2562,  -808,  7195,  -808,  -808,  -808,  6718,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  7308,  7308,  -808,  -808,
    7308,  3884,  3479,  -808,  -808,  -808,  -808,   455,  6585,    17,
    -808,    60,  -808,  -808,  -808,  2804,  3614,  -808,  -808,  2939,
    -808,  -808,  -808,  -808,  -808,  -808,  8664,  8664,   140,  4955,
     462,  7647,  7986,  6977,  -808,  6452,  -808,  -808,  -808,   103,
     115,   126,   206,   117,  8777,  8664,  -808,   -25,  -808,  1007,
    -808,   322,  -808,  -808,   141,   372,   352,  -808,   337,  9003,
    -808,   379,  2918,   247,   278,  -808,  8890,  8890,  -808,  -808,
    5842,  9112,  9221,  9330,  6318,    33,    65,  -808,  -808,   410,
    -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
     205,   409,  -808,   441,   433,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,   396,  -808,  -808,  -808,  -808,
     427,  8664,   519,  5094,  8664,  8664,  8664,  8664,  -808,   468,
    2918,   500,  -808,  -808,   463,   304,    22,   344,   517,   363,
     479,  -808,  -808,  -808,  5729,  -808,  7308,  7308,  -808,  -808,
    5955,  -808,  8890,   498,  -808,   482,   491,  5233,  -808,  -808,
    -808,   497,   499,   141,  -808,   361,   570,   458,  7421,  -808,
    4955,   505,   -25,  -808,  1007,    17,   538,  -808,    17,  8890,
     515,   188,   318,  -808,   500,   523,   318,  -808,    17,   617,
     338,  9439,   540,  -808,   474,   572,   613,   674,  -808,  -808,
    -808,  -808,  -808,  -808,   522,  -808,   601,   669,   284,   576,
     752,   577,    58,   582,   759,   584,    68,   609,   612,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  6068,  8890,  8890,  8890,
    8890,  7421,  8890,  8890,  -808,  -808,  -808,   588,  -808,  -808,
    -808,  8099,  -808,  4955,  7086,   561,  8099,  8664,  8664,  8664,
    8664,  8664,  8664,  8664,  8664,  8664,  8664,  8664,  8664,  8664,
    8664,  8664,  8664,  8664,  8664,  8664,  8664,  8664,  8664,  8664,
    8664,  8664,  8664,  2012,  7308,  9718,  4312,   322,   109,   109,
    8890,  8890,   -25,   689,   563,   651,  -808,  -808,   685,   691,
      53,    57,   100,   328,   393,  8890,   568,  -808,   199,   708,
    -808,  -808,  -808,  -808,    55,   223,   249,   279,   283,   301,
     316,   384,   417,  -808,  -808,  -808,    33,  -808,  -808,  9795,
    -808,  -808,  8777,  8777,  -808,  -808,   414,  -808,  -808,  -808,
    8664,  8664,  7534,  -808,  -808,  9872,  7308,  9949,  8664,  8664,
    7760,  -808,    17,   595,  -808,  -808,    17,  -808,   580,   599,
    -808,    61,  -808,  -808,  -808,  -808,  -808,  6718,  -808,  8664,
    5364,   604,  9872,  9949,  8664,  1007,   607,    17,  -808,  -808,
    6181,   618,  -808,   322,  -808,  7873,  -808,  -808,  7986,  -808,
    -808,  -808,   482,   769,  -808,  -808,  -808,   622,  9439, 10026,
    7308, 10103,  1417,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,   244,  -808,  -808,   611,  -808,
    -808,  -808,   347,  -808,   623,  -808,  8664,  8664,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  -808,    51,    51,  -808,
    -808,    51,  8664,  -808,   628,   634,  -808,    17,  9439,   655,
    -808,  -808,  -808,   678,  1866,  -808,  -808,   570,  2240,  2240,
    2240,  2240,   727,   727,  3323,  3188,  2240,  2240,  3053,  3053,
     415,   415,  1674,   727,   727,   907,   907,   660,    40,    40,
     570,   570,   570,  4019,  3074,  4154,  3209,   499,   661,  -808,
      17,   535,  -808,   688,  -808,   499,  3749,   737,   790,  -808,
    4451,   799,  4729,    66,    66,   689,  8212,   737,   147, 10180,
    7308, 10257,  -808,   322,  -808,   769,  -808,   -25,  -808,  -808,
    -808, 10334,  7308,  9795,  4312,  8890,  1538,  -808,  -808,  -808,
    4173,  -808,  2648,  -808,  2918,  6718,  2783,  -808,  8664,   500,
    -808,   479,  2669,  3344,    17,   420,   444,  -808,  -808,  -808,
    -808,  7534,  7760,  -808,  -808,  8890,  2918,   672,  -808,  -808,
    -808,  2918,  5364,     2,  -808,    17,   318,  9439,   622,   701,
     469,    17,   207,   224,   711,  -808,  -808,  -808,  -808,  8664,
    -808,   947,  -808,  -808,  -808,  -808,  -808,  1789,  -808,  -808,
    -808,  -808,  -808,  -808,   680,  -808,   687,   773,   694,  -808,
     698,   783,   704,   791,  -808,  -808,   834,  -808,  -808,  -808,
    -808,  -808,   570,   570,  -808,  1322,  5503,  -808,  -808,  5233,
      51,  5503,   721,  8325,  -808,   622,  9439,  8777,  8664,   741,
    8777,  8777,  -808,   588,   499,   722,   818,  8777,  8777,  -808,
     588,   499,  -808,  -808,  8438,   827,  -808,   597,  -808,   827,
    -808,  -808,  -808,  -808,   737,    70,  -808,    67,   136,    17,
     148,   149,  8890,   -25,  -808,  8890,  4312,   701,   469,  -808,
      17,   737,    61,  1789,  4312,   -25,  6851,  -808,    65,   372,
    -808,  8664,  -808,  -808,  -808,  8664,  8664,   466,  8664,  8664,
     731,    61,  -808,   736,  -808,  -808,  -808,   567,  8664,  -808,
    -808,   947,   350,  -808,   738,    17,  -808,    17,    72,  1789,
    -808,    42,  -808,  -808,  -808,   272,  -808,  1789,  -808,  -808,
     892,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,   762,
    9548,  -808,    17,   764,   746,  -808,   751,   694,  -808,   758,
     760,  -808,   761,   893,   775,  5233,   899,  8664,   776,   622,
    2918,  8664,  -808,  2918,  -808,  2918,  -808,  -808,  -808,  8777,
    -808,  2918,  -808,  2918,  -808,  -808,   628,  -808,   826,  -808,
    4842,   909,  -808,  8890,   737,  -808,   737,  5503,  5503,  -808,
    8551,  4590,   155,    66,  -808,   -25,   737,  -808,  -808,  -808,
      17,   737,  -808,  -808,  -808,  -808,  2918,  8664,  7760,  -808,
    -808,  -808,    17,  1011,   789,  1003,  -808,   793,  5503,  5233,
    -808,  -808,  -808,   798,   803,  -808,   694,  -808,   804,  -808,
     811,   804,  5616,  9548,   877,   792,   808,  -808,  1910,  -808,
     490,  -808,  -808,  1910,  -808,  1368,  -808,  1032,  -808,  -808,
     820,  -808,   819,  2918,  -808,  2918,  9657,   109,  -808,  -808,
    5503,  -808,  -808,   109,  -808,  -808,   737,   737,  -808,   130,
    -808,  4312,  -808,  -808,  -808,  -808,  1538,  -808,   821,  1011,
     434,  -808,  -808,   944,   832,  1789,  -808,   892,  -808,  -808,
     892,  -808,   892,  -808,  -808,   850,   792,  -808, 10411,  -808,
    -808,   830,   831,  -808,   694,   835,  -808,   837,   835,  -808,
     443,  -808,  -808,  -808,   910,  -808,   796,   572,   613,   674,
    4312,  -808,  4451,  -808,  -808,  -808,  -808,  -808,  5503,   737,
    4312,  1011,   821,  1011,   838,  -808,  -808,   804,   839,   804,
     804,  -808,   840,   841,  1910,  -808,  1368,  -808,  -808,  1368,
    -808,  1368,  -808,  -808,  1032,  -808,   769, 10488,  7308, 10565,
     790,   597,   737,  -808,   737,   821,  1011,  -808,   892,  -808,
    -808,  -808,   835,   844,   835,   835,  -808,    74,   469,    17,
     146,   158,  -808,  -808,  -808,  -808,   821,   804,  -808,  1368,
    -808,  -808,  -808,   176,  -808,   835,  -808
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -808,  -808,  -808,  -363,  -808,    27,  -808,  -550,   -21,  -808,
     518,  -808,    15,  -808,  -301,   -51,   -86,    41,  -808,  -268,
    -808,   413,    -7,   888,  -153,    -4,   -73,  -808,  -416,     7,
    1571,  -315,   889,   -53,  -808,    -5,  -808,  -808,    98,  -808,
     870,  -808,   735,  -808,   -44,   275,  -340,    87,   -11,  -808,
    -386,  -188,     4,  -311,   -20,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,    64,  -808,  -808,  -808,  -808,  -808,  -808,
    -808,  -808,  -808,  -808,   154,  -347,  -526,   -34,  -645,  -808,
    -785,  -779,   221,   309,   374,  -808,  -446,  -808,  -665,  -808,
     -18,  -808,  -808,  -808,  -808,  -808,  -808,   248,  -808,  -808,
    -808,  -808,  -808,  -808,  -808,   -93,  -808,  -808,  -535,  -808,
      -6,  -808,  -808,  -808,  -808,  -808,  -808,   918,  -808,  -808,
    -808,  -808,   712,  -808,  -808,  -808,  -808,  -808,  -808,  -808,
     963,  -808,  -142,  -808,  -808,  -808,  -808,  -808,    10,  -808,
      14,  -808,  1081,  1361,   928,  1531,  1552,  -808,  -808,    89,
    -464,  -372,  -415,  -783,  -518,  -723,   234,   242,   133,  -808,
    -808,  -808,   289,  -708,  -807,   128,   255,  -808,  -638,  -808,
     228,  -622,  -808,  -808,  -808,    69,  -383,  -808,  -348,  -808,
     639,   -26,     0,    48,  -598,  -208,    21,   -14,    -2
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -621
static const yytype_int16 yytable[] =
{
     116,   359,   283,   400,   362,   558,   325,   566,   659,   208,
     208,   229,   572,   208,   560,   526,   757,    69,   198,    69,
     214,   214,   199,   234,   214,   521,   610,   232,   251,   235,
     316,   117,   421,   273,   610,   357,   357,   617,   198,   357,
     724,   394,   199,   767,   741,   256,   786,   810,   723,   263,
     267,   600,   638,   306,   459,   261,   882,   660,   461,   273,
     272,   586,   689,   317,   363,   691,    84,   558,    84,   566,
     841,   273,   273,   273,   846,   617,   614,   627,  -102,   889,
     215,   215,  -103,   569,   215,   220,   220,  -106,   305,   220,
     941,   314,  -106,   720,   938,   630,   966,   447,   312,   313,
     649,   878,   695,   212,   222,   299,   196,   223,   327,  -519,
     650,   305,   877,    84,   302,   -74,   303,   274,   397,   488,
     262,   266,   569,   630,   972,  -110,   196,   684,   215,   494,
     641,   852,   659,   776,     3,  -102,   -88,   896,   653,   654,
    -511,   884,   857,   274,   479,   840,   482,   576,   486,   890,
     215,   215,   486,   196,   215,   368,   379,   379,   231,  -105,
     992,  -108,   655,   350,   351,   352,   398,   314,   489,   685,
     517,  -107,  -109,  -105,  -107,   882,   297,   298,   495,   431,
    -104,   433,   251,   879,   901,   902,   856,   -93,  -511,  -104,
     237,   -94,   196,   659,   861,   315,   247,   966,   466,   316,
     587,   -97,   231,  -598,   850,   302,   941,   303,   453,   302,
    -598,   303,   414,  -106,  1025,  -106,   441,   208,   414,   208,
     208,   500,   501,   502,   503,   430,   449,   882,   214,   251,
     214,   972,   -96,   630,  -101,   610,   610,   889,   450,   617,
     729,   421,   777,   289,   815,   630,   256,  1046,   302,   -98,
     303,   273,   740,   776,   874,   291,   357,   357,   357,   357,
     962,   505,   506,   445,   949,   967,   293,    84,   462,   763,
     -99,   315,  -599,   305,   573,   574,   739,   227,   432,   659,
     819,  -100,   -96,   -98,   985,  -105,   882,  -105,   215,   -95,
     215,   215,  -519,   220,   215,   220,   215,  -107,   788,  -107,
     791,    84,   251,   451,   273,   473,   452,   998,  -510,   357,
     357,   422,    84,   425,    84,  -104,   588,  -104,  -102,   256,
     -96,    69,   455,   215,   583,   515,   504,   930,   860,   456,
     515,   519,  -339,   499,  -512,   274,   575,   -98,   302,   -93,
     303,   -96,  1008,   678,   -96,   473,   295,   208,   -96,   481,
     678,   474,   430,   475,  -595,   776,  -510,   776,   -98,   312,
     313,   -98,   513,   353,  -513,   -98,  1033,   522,  -515,   654,
      84,   215,   215,   215,   215,    84,   215,   215,   851,  -339,
    -339,   989,  -512,   305,   677,   215,  -514,    84,   274,   415,
     215,   474,   655,   475,   986,   466,   521,   464,   465,   590,
     319,  -516,   770,   837,   645,   646,   647,   648,   473,   208,
     354,   355,  -513,  -505,   430,    74,  -515,    74,   215,  -596,
      84,   776,   994,   754,   215,   215,   414,   414,   765,    74,
      74,  -520,   624,    74,  -514,   826,   416,   417,   116,   215,
    1024,  1053,   834,   601,   603,   466,   442,   198,   273,  -516,
     419,   199,   458,   564,   474,    69,   475,  -103,  -595,   456,
    -505,  -505,    74,   208,  -595,   321,   215,   215,   430,  -505,
     618,   322,   659,   776,   620,   776,  -110,    74,  -508,   623,
     215,   611,   610,   327,   873,   617,   770,   680,   645,   646,
     647,   648,   326,   416,   443,   633,  -520,   564,   273,    74,
      74,   444,  -508,    74,    84,   688,   688,   758,   776,   688,
      56,   946,   948,   702,    84,   564,   951,  -505,   953,   954,
     403,   709,  -103,  -596,   636,  -508,  -508,  -109,   402,  -596,
     404,   759,   274,  -105,   215,   196,   348,   349,   350,   351,
     352,   564,   699,   -94,   742,   444,  -110,   703,   800,   717,
    -508,   719,   434,   867,  -294,   710,   747,  -107,   650,   469,
     705,   435,   436,   230,   766,   694,   716,  -101,   993,   564,
     980,  -109,   754,   208,   761,   406,   982,  1014,   430,  -104,
     725,   410,   274,   473,   231,   208,   653,   654,   411,   749,
     430,   230,  -100,  -433,   722,   722,   231,   412,   750,  -288,
     752,  -294,  -294,  -288,   418,  -599,   470,   471,   734,   414,
     655,   843,   840,   420,   705,   198,    74,   273,   438,   199,
     584,   440,   707,   466,   585,  1027,  1029,  1030,  1031,   474,
     116,   475,   476,   224,    84,   227,    84,    74,   327,    74,
      74,   705,   448,    74,   215,    74,   853,    69,  -105,   855,
      74,   -73,  -433,   454,   460,   864,   215,  -517,    84,   215,
     827,    74,   473,    74,   783,   813,   783,   747,   814,   -96,
     816,   630,    74,   463,   468,  1054,   273,   496,  1039,   564,
     497,   357,   509,   764,   357,   812,   483,   487,   688,   215,
     760,   564,   490,   863,   493,   520,    84,  -433,  -518,  -433,
    -433,   274,   498,   196,  -517,  -517,   576,   578,   474,   750,
     475,   478,   839,   842,   621,   842,   582,   842,   515,    74,
      74,    74,    74,    74,    74,    74,    74,  -602,   327,   705,
     473,   726,   619,   622,    74,   628,    74,   632,   522,    74,
     705,   822,   824,   340,   341,  -518,  -518,   712,   830,   832,
      84,   679,   -88,    84,   854,    84,   637,   923,   414,  -284,
     274,   215,  -268,   681,   215,   215,   862,    74,   693,    74,
     579,   215,   215,    74,    74,   708,   474,   812,   475,   480,
     273,   348,   349,   350,   351,   352,  -602,   264,    74,   696,
     859,   697,   357,   591,   910,   327,   215,  -424,   714,   215,
      84,  -107,   812,   762,   783,   783,  -284,  -284,    84,   869,
     340,   341,   718,   473,   779,    74,    74,   580,   581,   919,
     473,   781,   -98,   876,   768,   784,   926,   927,   785,    74,
     929,  -602,   787,  -602,  -602,   789,   918,  -598,   790,   922,
     592,   593,   840,   792,   196,   345,   346,   347,   348,   349,
     350,   351,   352,    74,  -295,  -269,   821,   943,   944,   474,
     828,   475,   484,    74,   895,   868,   474,   870,   475,   491,
     914,   955,   875,   273,   722,   893,   932,   958,   849,    84,
     898,  1017,   897,    74,   783,   900,   210,   210,   794,   795,
     210,   796,   903,   215,   905,   858,   273,    44,    45,   981,
     907,  -295,  -295,   908,    84,   829,   909,   215,   934,   911,
    -270,    84,    84,   811,   916,    84,   244,   246,   920,   804,
     937,   210,   210,   939,   592,   593,   959,   842,  1018,  1019,
     942,  -104,   945,   957,   300,   301,   405,   947,   950,   407,
     408,   409,    84,    84,   770,   952,   645,   646,   647,   648,
     649,   973,   -95,  -271,   995,   991,    84,   956,  1001,  1020,
     650,  1021,   783,   996,  1004,  1006,  1016,  1022,   634,  1009,
    1015,  1011,  1026,  1028,   807,   327,  -598,  -599,  1049,   366,
     976,   651,   383,    74,    84,    74,   835,  1043,   653,   654,
     340,   341,   872,    74,   801,    84,  1036,   865,   924,   770,
     925,   645,   646,   647,   648,    74,   492,    74,    74,   887,
     933,   208,   655,   887,  1042,   935,   430,   401,   716,   842,
     287,   888,   395,   885,   891,   990,   961,   347,   348,   349,
     350,   351,   352,   963,   883,   597,   771,     0,    74,   705,
       0,     0,   772,     0,    84,    74,    84,   307,   308,   309,
     310,   311,    84,     0,    84,   770,   514,   645,   646,   647,
     648,   525,     0,   770,     0,   645,   646,   647,   648,     0,
     886,   210,     0,     0,   210,   210,   210,   300,     0,     0,
     983,   984,   215,   110,   969,   110,   645,   646,   647,   648,
       0,     0,   771,     0,   210,     0,   210,   210,   940,    74,
     771,     0,    74,     0,    74,     0,     0,     0,     0,     0,
      74,     0,     0,    74,    74,     0,     0,   564,     0,     0,
      74,    74,     0,     0,     0,     0,   811,     0,   887,     0,
     110,   811,     0,   811,   275,     0,     0,   965,     0,   968,
       0,     0,     0,  1023,     0,    74,   607,   609,    74,    74,
       0,     0,     0,     0,     0,   264,     0,    74,     0,     0,
     275,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   370,   380,   380,   380,  1044,     0,  1045,   997,
       0,   999,     0,   904,   906,     0,  1000,   807,     0,   964,
     609,   210,   807,   264,   807,     0,   524,   527,   528,   529,
     530,   531,   532,   533,   534,   535,   536,   537,   538,   539,
     540,   541,   542,   543,   544,   545,   546,   547,   548,   549,
     550,   551,   552,     0,   210,     0,     0,     0,    74,     0,
       0,     0,   811,     0,   811,     0,     0,   811,  1032,   811,
    1034,     0,    74,     0,     0,  1035,     0,   692,     0,     0,
       0,     0,     0,    74,     0,     0,    74,     0,     0,     0,
      74,    74,  1047,     0,    74,     0,     0,     0,     0,     0,
       0,     0,   602,   604,     0,     0,     0,   811,     0,     0,
     606,   210,   210,  1055,   110,     0,   210,     0,   602,   604,
     210,    74,    74,   807,     0,   807,     0,     0,   807,     0,
     807,     0,     0,     0,     0,    74,     0,     0,     0,   626,
       0,   525,     0,     0,   631,     0,     0,     0,   110,     0,
       0,     0,     0,     0,     0,   210,     0,     0,   210,   110,
       0,   110,     0,    74,     0,  1005,  1007,     0,   807,  1010,
     210,  1012,  1013,   753,    74,     0,     0,     0,     0,     0,
       0,     0,   275,     0,     0,     0,   609,   264,     0,     0,
       0,     0,     0,   111,     0,   111,   682,   683,     0,     0,
       0,     0,     0,     0,   799,     0,   645,   646,   647,   648,
     800,     0,   210,     0,   769,     0,     0,   110,     0,     0,
     650,     0,   110,    74,     0,    74,     0,     0,     0,     0,
       0,    74,     0,    74,   110,   275,  1048,  1050,  1051,  1052,
     111,   651,     0,     0,   276,     0,     0,   652,   653,   654,
     770,     0,   645,   646,   647,   648,   800,     0,   818,  1056,
       0,    74,     0,     0,     0,     0,   650,   110,     0,     0,
     276,     0,   655,     0,     0,   656,   210,     0,     0,   836,
     210,     0,   371,   381,   381,   381,     0,   651,     0,     0,
       0,     0,   210,   231,   653,   654,     0,     0,     0,   644,
       0,   645,   646,   647,   648,   649,     0,     0,   210,     0,
       0,     0,     0,     0,     0,   650,     0,     0,   655,     0,
       0,   210,   210,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   871,     0,     0,   651,     0,     0,     0,
       0,     0,   652,   653,   654,     0,     0,     0,     0,   210,
       0,   110,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   110,     0,   113,     0,   113,     0,   655,     0,     0,
     656,     0,     0,     0,     0,     0,     0,     0,     0,   275,
       0,     0,   912,   657,   114,     0,   114,     0,     0,     0,
       0,     0,     0,   210,   111,     0,     0,   606,   820,     0,
     823,   825,     0,    81,     0,    81,     0,   831,   833,     0,
     113,     0,     0,     0,   210,   928,     0,     0,     0,     0,
     644,     0,   645,   646,   647,   648,   649,     0,   111,   275,
       0,   114,     0,   264,     0,   277,   650,     0,     0,   111,
       0,   111,     0,     0,     0,     0,     0,     0,     0,     0,
      81,   866,     0,     0,     0,   823,   825,   651,   831,   833,
       0,   277,   276,   652,   653,   654,     0,     0,   210,     0,
       0,     0,     0,   372,   382,   382,     0,     0,     0,     0,
       0,   110,     0,   110,     0,     0,     0,     0,   655,     0,
       0,   656,   367,     0,     0,     0,     0,   111,     0,     0,
       0,     0,   111,     0,   743,   110,     0,     0,     0,     0,
       0,     0,     0,     0,   111,   276,     0,   210,     0,     0,
       0,   913,     0,     0,     0,     0,     0,     0,     0,   915,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   110,     0,     0,     0,   111,   275,     0,
     210,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   113,     0,     0,   915,   210,     0,
       0,     0,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   114,     0,   340,   341,     0,
       0,     0,     0,     0,     0,     0,     0,   110,   113,     0,
     110,     0,   110,     0,    81,     0,     0,   275,     0,   113,
       0,   113,     0,     0,     0,     0,     0,     0,   342,   114,
     343,   344,   345,   346,   347,   348,   349,   350,   351,   352,
     114,   111,   114,     0,     0,     0,     0,     0,    81,     0,
       0,   111,     0,     0,     0,   231,     0,   110,     0,    81,
       0,    81,     0,   277,     0,   110,     0,     0,     0,   276,
       0,     0,     0,     0,     0,     0,     0,   113,     0,     0,
       0,   644,   113,   645,   646,   647,   648,   649,     0,     0,
       0,     0,     0,     0,   113,     0,     0,   650,   114,     0,
       0,     0,     0,   114,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   114,   277,    81,   651,   276,
       0,   380,    81,     0,   652,   653,   654,   113,   210,     0,
       0,     0,     0,     0,    81,     0,   110,   523,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   114,   655,
     698,     0,   656,     0,     0,     0,     0,     0,     0,     0,
       0,   110,     0,     0,     0,     0,     0,    81,   110,   110,
       0,   111,   110,   111,   327,   328,   329,   330,   331,   332,
     333,   334,   335,   336,   337,   338,   339,     0,     0,   340,
     341,     0,     0,     0,     0,   111,     0,     0,     0,   110,
     110,     0,   799,     0,   645,   646,   647,   648,   800,     0,
       0,   113,     0,   110,   380,     0,     0,     0,   650,     0,
     342,   113,   343,   344,   345,   346,   347,   348,   349,   350,
     351,   352,   114,   111,     0,     0,     0,   977,   276,   651,
    -245,   110,   114,     0,     0,   652,   653,   654,     0,     0,
       0,    81,   110,     0,     0,     0,     0,     0,     0,     0,
     277,    81,     0,     0,     0,     0,     0,     0,     0,     0,
     655,     0,     0,   656,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   111,     0,     0,
     111,     0,   111,     0,     0,     0,     0,   276,     0,     0,
       0,   110,     0,   110,   553,   554,     0,     0,   555,   110,
     277,   110,     0,     0,     0,     0,     0,     0,   162,   163,
     164,   165,   166,   167,   168,   169,   170,     0,     0,   171,
     172,     0,     0,   173,   174,   175,   176,   111,     0,     0,
       0,   113,     0,   113,     0,   111,     0,   177,   178,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   114,     0,   114,   113,     0,     0,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,     0,   189,
     190,    81,     0,    81,     0,     0,   114,   191,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   381,     0,   113,     0,    81,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   111,     0,     0,     0,
       0,     0,     0,     0,   114,     0,     0,     0,     0,   277,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   111,     0,    81,     0,     0,     0,     0,   111,   111,
       0,     0,   111,     0,     0,     0,     0,   113,     0,     0,
     113,     0,   113,     0,     0,     0,     0,     0,   798,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   114,   111,
     111,   114,     0,   114,     0,     0,     0,     0,   277,     0,
       0,     0,     0,   111,   381,     0,     0,    81,     0,     0,
      81,     0,    81,     0,     0,     0,     0,   113,   523,     0,
       0,     0,     0,     0,     0,   113,     0,   978,     0,     0,
       0,   111,     0,     0,     0,     0,     0,     0,   114,     0,
       0,     0,   111,     0,     0,     0,   114,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    81,   327,  -621,
    -621,  -621,  -621,   332,   333,    81,     0,  -621,  -621,     0,
       0,     0,     0,   340,   341,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   111,     0,   111,     0,     0,   113,     0,     0,   111,
       0,   111,   382,     0,     0,     0,   343,   344,   345,   346,
     347,   348,   349,   350,   351,   352,     0,   114,     0,     0,
       0,   113,     0,     0,     0,     0,     0,     0,   113,   113,
       0,     0,   113,     0,     0,     0,    81,     0,     0,     0,
       0,     0,   114,     0,     0,     0,     0,     0,     0,   114,
     114,     0,     0,   114,     0,     0,     0,     0,     0,   113,
     113,    81,     0,     0,     0,     0,     0,     0,    81,    81,
       0,     0,    81,   113,     0,     0,     0,     0,     0,     0,
     114,   114,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   114,   382,     0,     0,     0,    81,
      81,   113,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   113,    81,     0,     0,     0,     0,   979,     0,
       0,     0,   114,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   114,     0,     0,     0,   975,     0,     0,
       0,    81,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    81,     0,     0,     0,     0,     0,     0,     0,
       0,   113,     0,   113,     0,     0,     0,     0,     0,   113,
       0,   113,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   114,     0,   114,     0,     0,     0,     0,     0,
     114,     0,   114,     0,     0,     0,     0,     0,     0,     0,
       0,    81,     0,    81,     0,     0,     0,     0,     0,    81,
       0,    81,  -620,     4,     0,     5,     6,     7,     8,     9,
       0,     0,     0,    10,    11,     0,     0,     0,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,     0,     0,
       0,     0,    20,    21,    22,    23,    24,    25,    26,     0,
       0,    27,     0,     0,     0,     0,     0,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
       0,    41,    42,     0,    43,    44,    45,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    48,     0,
       0,    49,    50,     0,    51,    52,     0,    53,     0,     0,
      54,    55,    56,    57,    58,    59,    60,    61,    62,  -602,
       0,     0,     0,     0,     0,     0,     0,  -602,  -602,  -602,
       0,     0,  -602,  -602,  -602,     0,  -602,     0,    63,    64,
      65,     0,   751,     0,     0,  -602,  -602,  -602,  -602,     0,
       0,  -620,     0,  -620,     0,     0,  -602,  -602,     0,  -602,
    -602,  -602,  -602,  -602,     0,     0,   327,   328,   329,   330,
     331,   332,   333,   334,   335,   336,   337,   338,   339,     0,
       0,   340,   341,     0,     0,     0,     0,  -602,  -602,  -602,
    -602,  -602,  -602,  -602,  -602,  -602,  -602,  -602,  -602,  -602,
       0,     0,  -602,  -602,  -602,     0,   755,  -602,     0,     0,
       0,     0,   342,  -602,   343,   344,   345,   346,   347,   348,
     349,   350,   351,   352,     0,     0,     0,  -602,     0,     0,
    -602,     0,  -106,  -602,  -602,  -602,  -602,  -602,  -602,  -602,
    -602,  -602,  -602,  -602,  -602,     0,     0,     0,     0,  -602,
    -602,  -602,  -602,  -602,  -505,     0,  -602,  -602,  -602,     0,
    -602,     0,  -505,  -505,  -505,     0,     0,  -505,  -505,  -505,
       0,  -505,     0,     0,     0,     0,     0,   698,     0,  -505,
       0,  -505,  -505,  -505,     0,     0,     0,     0,     0,     0,
       0,  -505,  -505,     0,  -505,  -505,  -505,  -505,  -505,     0,
       0,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,   338,   339,     0,     0,   340,   341,     0,     0,
       0,     0,  -505,  -505,  -505,  -505,  -505,  -505,  -505,  -505,
    -505,  -505,  -505,  -505,  -505,     0,     0,  -505,  -505,  -505,
       0,  -505,  -505,     0,     0,     0,     0,   342,  -505,   343,
     344,   345,   346,   347,   348,   349,   350,   351,   352,     0,
       0,     0,  -505,     0,     0,  -505,     0,  -505,  -505,  -505,
    -505,  -505,  -505,  -505,  -505,  -505,  -505,  -505,  -505,  -505,
       0,     0,     0,     0,     0,  -505,  -505,  -505,  -505,  -508,
       0,  -505,  -505,  -505,     0,  -505,     0,  -508,  -508,  -508,
       0,     0,  -508,  -508,  -508,     0,  -508,     0,     0,     0,
       0,     0,     0,     0,  -508,     0,  -508,  -508,  -508,     0,
       0,     0,     0,     0,     0,     0,  -508,  -508,     0,  -508,
    -508,  -508,  -508,  -508,     0,     0,   327,   328,   329,   330,
     331,   332,   333,   334,   335,   336,   337,   338,   339,     0,
       0,   340,   341,     0,     0,     0,     0,  -508,  -508,  -508,
    -508,  -508,  -508,  -508,  -508,  -508,  -508,  -508,  -508,  -508,
       0,     0,  -508,  -508,  -508,     0,  -508,  -508,     0,     0,
       0,     0,   342,  -508,   343,   344,   345,   346,   347,   348,
     349,   350,   351,   352,     0,     0,     0,  -508,     0,     0,
    -508,     0,  -508,  -508,  -508,  -508,  -508,  -508,  -508,  -508,
    -508,  -508,  -508,  -508,  -508,     0,     0,     0,     0,     0,
    -508,  -508,  -508,  -508,  -603,     0,  -508,  -508,  -508,     0,
    -508,     0,  -603,  -603,  -603,     0,     0,  -603,  -603,  -603,
       0,  -603,     0,     0,     0,     0,     0,     0,     0,     0,
    -603,  -603,  -603,  -603,     0,     0,     0,     0,     0,     0,
       0,  -603,  -603,     0,  -603,  -603,  -603,  -603,  -603,     0,
       0,   327,   328,   329,   330,   331,   332,   333,   334,   335,
     336,   337,  -621,  -621,     0,     0,   340,   341,     0,     0,
       0,     0,  -603,  -603,  -603,  -603,  -603,  -603,  -603,  -603,
    -603,  -603,  -603,  -603,  -603,     0,     0,  -603,  -603,  -603,
       0,     0,  -603,     0,     0,     0,     0,     0,  -603,   343,
     344,   345,   346,   347,   348,   349,   350,   351,   352,     0,
       0,     0,  -603,     0,     0,  -603,     0,     0,  -603,  -603,
    -603,  -603,  -603,  -603,  -603,  -603,  -603,  -603,  -603,  -603,
       0,     0,     0,     0,  -603,  -603,  -603,  -603,  -603,  -604,
       0,  -603,  -603,  -603,     0,  -603,     0,  -604,  -604,  -604,
       0,     0,  -604,  -604,  -604,     0,  -604,     0,     0,     0,
       0,     0,     0,     0,     0,  -604,  -604,  -604,  -604,     0,
       0,     0,     0,     0,     0,     0,  -604,  -604,     0,  -604,
    -604,  -604,  -604,  -604,     0,     0,   327,   328,   329,   330,
     331,   332,   333,   334,     0,   336,   337,     0,     0,     0,
       0,   340,   341,     0,     0,     0,     0,  -604,  -604,  -604,
    -604,  -604,  -604,  -604,  -604,  -604,  -604,  -604,  -604,  -604,
       0,     0,  -604,  -604,  -604,     0,     0,  -604,     0,     0,
       0,     0,     0,  -604,   343,   344,   345,   346,   347,   348,
     349,   350,   351,   352,     0,     0,     0,  -604,     0,     0,
    -604,     0,     0,  -604,  -604,  -604,  -604,  -604,  -604,  -604,
    -604,  -604,  -604,  -604,  -604,     0,     0,     0,     0,  -604,
    -604,  -604,  -604,  -604,  -294,     0,  -604,  -604,  -604,     0,
    -604,     0,  -294,  -294,  -294,     0,     0,  -294,  -294,  -294,
       0,  -294,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -294,  -294,  -294,     0,     0,     0,     0,     0,     0,
       0,  -294,  -294,     0,  -294,  -294,  -294,  -294,  -294,     0,
       0,   327,   328,   329,   330,   331,   332,   333,     0,     0,
     336,   337,     0,     0,     0,     0,   340,   341,     0,     0,
       0,     0,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,     0,     0,  -294,  -294,  -294,
       0,   756,  -294,     0,     0,     0,     0,     0,  -294,   343,
     344,   345,   346,   347,   348,   349,   350,   351,   352,     0,
       0,     0,  -294,     0,     0,  -294,     0,  -108,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
       0,     0,     0,     0,     0,  -294,  -294,  -294,  -294,  -432,
       0,  -294,  -294,  -294,     0,  -294,     0,  -432,  -432,  -432,
       0,     0,  -432,  -432,  -432,     0,  -432,     0,     0,     0,
       0,     0,     0,     0,     0,  -432,  -432,  -432,     0,     0,
       0,     0,     0,     0,     0,     0,  -432,  -432,     0,  -432,
    -432,  -432,  -432,  -432,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -432,  -432,  -432,
    -432,  -432,  -432,  -432,  -432,  -432,  -432,  -432,  -432,  -432,
       0,     0,  -432,  -432,  -432,     0,     0,  -432,     0,     0,
       0,     0,     0,  -432,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -432,     0,     0,
       0,     0,     0,  -432,     0,  -432,  -432,  -432,  -432,  -432,
    -432,  -432,  -432,  -432,  -432,     0,     0,     0,     0,  -432,
    -432,  -432,  -432,  -432,  -285,   227,  -432,  -432,  -432,     0,
    -432,     0,  -285,  -285,  -285,     0,     0,  -285,  -285,  -285,
       0,  -285,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -285,  -285,  -285,     0,     0,     0,     0,     0,     0,
       0,  -285,  -285,     0,  -285,  -285,  -285,  -285,  -285,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -285,  -285,  -285,  -285,  -285,  -285,  -285,  -285,
    -285,  -285,  -285,  -285,  -285,     0,     0,  -285,  -285,  -285,
       0,     0,  -285,     0,     0,     0,     0,     0,  -285,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -285,     0,     0,  -285,     0,     0,  -285,  -285,
    -285,  -285,  -285,  -285,  -285,  -285,  -285,  -285,  -285,  -285,
       0,     0,     0,     0,     0,  -285,  -285,  -285,  -285,  -422,
       0,  -285,  -285,  -285,     0,  -285,     0,  -422,  -422,  -422,
       0,     0,  -422,  -422,  -422,     0,  -422,     0,     0,     0,
       0,     0,     0,     0,     0,  -422,  -422,  -422,     0,     0,
       0,     0,     0,     0,     0,     0,  -422,  -422,     0,  -422,
    -422,  -422,  -422,  -422,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -422,  -422,  -422,
    -422,  -422,  -422,  -422,  -422,  -422,  -422,  -422,  -422,  -422,
       0,     0,  -422,  -422,  -422,     0,     0,  -422,     0,     0,
       0,     0,     0,  -422,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -422,     0,     0,
       0,     0,     0,  -422,     0,  -422,  -422,  -422,  -422,  -422,
    -422,  -422,  -422,  -422,  -422,     0,     0,     0,     0,  -422,
    -422,  -422,  -422,  -422,  -301,  -422,  -422,  -422,  -422,     0,
    -422,     0,  -301,  -301,  -301,     0,     0,  -301,  -301,  -301,
       0,  -301,     0,     0,     0,     0,     0,     0,     0,     0,
       0,  -301,  -301,     0,     0,     0,     0,     0,     0,     0,
       0,  -301,  -301,     0,  -301,  -301,  -301,  -301,  -301,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -301,  -301,  -301,  -301,  -301,  -301,  -301,  -301,
    -301,  -301,  -301,  -301,  -301,     0,     0,  -301,  -301,  -301,
       0,     0,  -301,     0,     0,     0,     0,     0,  -301,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,  -301,     0,     0,     0,     0,     0,  -301,     0,
    -301,  -301,  -301,  -301,  -301,  -301,  -301,  -301,  -301,  -301,
       0,     0,     0,     0,     0,  -301,  -301,  -301,  -301,  -602,
     224,  -301,  -301,  -301,     0,  -301,     0,  -602,  -602,  -602,
       0,     0,     0,  -602,  -602,     0,  -602,     0,     0,     0,
       0,     0,     0,     0,     0,  -602,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,  -602,  -602,     0,  -602,
    -602,  -602,  -602,  -602,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -602,  -602,  -602,
    -602,  -602,  -602,  -602,  -602,  -602,  -602,  -602,  -602,  -602,
       0,     0,  -602,  -602,  -602,     0,   700,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,  -602,     0,     0,
       0,     0,  -106,  -602,     0,  -602,  -602,  -602,  -602,  -602,
    -602,  -602,  -602,  -602,  -602,     0,     0,     0,     0,  -602,
    -602,  -602,  -602,   -97,  -294,     0,  -602,     0,  -602,     0,
    -602,     0,  -294,  -294,  -294,     0,     0,     0,  -294,  -294,
       0,  -294,     0,  -620,     0,     0,     0,     0,     0,     0,
       0,  -620,  -620,  -620,     0,     0,  -620,  -620,  -620,     0,
    -620,  -294,  -294,     0,  -294,  -294,  -294,  -294,  -294,  -620,
    -620,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    -620,  -620,     0,  -620,  -620,  -620,  -620,  -620,     0,     0,
       0,     0,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
    -294,  -294,  -294,  -294,  -294,     0,     0,  -294,  -294,  -294,
       0,   701,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,  -620,     0,
       0,     0,  -294,     0,     0,     0,     0,  -108,  -294,     0,
    -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,  -294,
       0,  -620,     0,     0,     0,  -294,  -294,  -294,   -99,     0,
       0,  -294,     0,  -294,     0,  -294,     0,     0,     0,     0,
       0,     0,     0,  -620,  -620,     0,  -620,     0,     0,   227,
    -620,     0,  -620,   248,  -620,     5,     6,     7,     8,     9,
    -620,  -620,  -620,    10,    11,     0,     0,  -620,    12,     0,
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
       0,  -620,   248,  -620,     5,     6,     7,     8,     9,     0,
       0,  -620,    10,    11,     0,  -620,  -620,    12,     0,    13,
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
    -620,   248,  -620,     5,     6,     7,     8,     9,     0,     0,
    -620,    10,    11,     0,     0,  -620,    12,  -620,    13,    14,
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
       0,     0,     0,     0,     0,     0,     0,     0,     0,  -620,
     248,  -620,     5,     6,     7,     8,     9,     0,     0,  -620,
      10,    11,     0,     0,  -620,    12,     0,    13,    14,    15,
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
       0,  -620,  -620,    10,    11,    63,    64,    65,    12,     0,
      13,    14,    15,    16,    17,    18,    19,     0,  -620,     0,
    -620,     0,    20,    21,    22,    23,    24,    25,    26,     0,
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
       0,  -620,     0,  -620,     0,    20,    21,    22,    23,    24,
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
       0,     0,  -620,     0,  -620,   248,  -620,     5,     6,     7,
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
       0,  -620,     0,  -620,   248,  -620,     5,     6,     7,     8,
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
      64,    65,     0,     0,  -620,     4,     0,     5,     6,     7,
       8,     9,  -620,     0,  -620,    10,    11,     0,     0,     0,
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
      63,    64,    65,     0,     0,  -620,     0,     0,     0,     0,
       0,     0,     0,  -620,   248,  -620,     5,     6,     7,     8,
       9,     0,     0,  -620,    10,    11,     0,     0,     0,    12,
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
      19,     0,  -620,     0,  -620,     0,    20,    21,    22,    23,
      24,    25,    26,     0,     0,    27,     0,     0,     0,     0,
       0,    28,    29,   249,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,     0,    41,    42,     0,    43,    44,
      45,     0,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    48,     0,     0,    49,    50,     0,    51,    52,
       0,    53,     0,     0,    54,    55,    56,    57,    58,    59,
      60,    61,    62,     0,  -620,     0,     0,     0,     0,     0,
       0,     0,     5,     6,     7,     0,     9,     0,     0,     0,
      10,    11,    63,    64,    65,    12,     0,    13,    14,    15,
      16,    17,    18,    19,     0,  -620,     0,  -620,     0,    20,
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
     817,     0,     0,    54,    55,    56,    57,    58,    59,    60,
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
      23,    24,    25,    26,     0,     0,   200,     0,     0,     0,
       0,     0,     0,    29,     0,     0,    32,    33,    34,    35,
      36,    37,    38,    39,    40,     0,    41,    42,     0,    43,
      44,    45,     0,    46,    47,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   202,     0,     0,   203,    50,     0,    51,
      52,     0,     0,     0,     0,    54,    55,    56,    57,    58,
      59,    60,    61,    62,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     5,     6,     7,     0,     9,     0,     0,
       0,    10,    11,    63,   207,    65,    12,     0,    13,    14,
      15,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      20,    21,    22,    23,    24,    25,    26,     0,     0,    27,
       0,     0,     0,     0,     0,     0,    29,     0,     0,    32,
      33,    34,    35,    36,    37,    38,    39,    40,     0,    41,
      42,     0,    43,    44,    45,     0,    46,    47,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   202,     0,     0,   203,
      50,     0,    51,    52,     0,     0,     0,     0,    54,    55,
      56,    57,    58,    59,    60,    61,    62,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     5,     6,     7,     0,
       9,     0,     0,     0,    10,    11,    63,    64,    65,    12,
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
     561,   562,     0,     0,   563,     0,     0,     0,     0,     0,
       0,     0,     0,   270,   162,   163,   164,   165,   166,   167,
     168,   169,   170,     0,     0,   171,   172,     0,     0,   173,
     174,   175,   176,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   177,   178,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,     0,   189,   190,   598,   562,     0,
       0,   599,     0,   191,     0,     0,     0,     0,     0,     0,
       0,   162,   163,   164,   165,   166,   167,   168,   169,   170,
       0,     0,   171,   172,     0,     0,   173,   174,   175,   176,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     177,   178,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,     0,   189,   190,   612,   554,     0,     0,   613,     0,
     191,     0,     0,     0,     0,     0,     0,     0,   162,   163,
     164,   165,   166,   167,   168,   169,   170,     0,     0,   171,
     172,     0,     0,   173,   174,   175,   176,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   177,   178,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   179,   180,
     181,   182,   183,   184,   185,   186,   187,   188,     0,   189,
     190,   615,   562,     0,     0,   616,     0,   191,     0,     0,
       0,     0,     0,     0,     0,   162,   163,   164,   165,   166,
     167,   168,   169,   170,     0,     0,   171,   172,     0,     0,
     173,   174,   175,   176,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   177,   178,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   179,   180,   181,   182,   183,
     184,   185,   186,   187,   188,     0,   189,   190,   639,   554,
       0,     0,   640,     0,   191,     0,     0,     0,     0,     0,
       0,     0,   162,   163,   164,   165,   166,   167,   168,   169,
     170,     0,     0,   171,   172,     0,     0,   173,   174,   175,
     176,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   177,   178,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,     0,   189,   190,   642,   562,     0,     0,   643,
       0,   191,     0,     0,     0,     0,     0,     0,     0,   162,
     163,   164,   165,   166,   167,   168,   169,   170,     0,     0,
     171,   172,     0,     0,   173,   174,   175,   176,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   177,   178,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   179,
     180,   181,   182,   183,   184,   185,   186,   187,   188,     0,
     189,   190,   727,   554,     0,     0,   728,     0,   191,     0,
       0,     0,     0,     0,     0,     0,   162,   163,   164,   165,
     166,   167,   168,   169,   170,     0,     0,   171,   172,     0,
       0,   173,   174,   175,   176,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   177,   178,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   179,   180,   181,   182,
     183,   184,   185,   186,   187,   188,     0,   189,   190,   730,
     562,     0,     0,   731,     0,   191,     0,     0,     0,     0,
       0,     0,     0,   162,   163,   164,   165,   166,   167,   168,
     169,   170,     0,     0,   171,   172,     0,     0,   173,   174,
     175,   176,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   177,   178,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   179,   180,   181,   182,   183,   184,   185,
     186,   187,   188,     0,   189,   190,   737,   554,     0,     0,
     738,     0,   191,     0,     0,     0,     0,     0,     0,     0,
     162,   163,   164,   165,   166,   167,   168,   169,   170,     0,
       0,   171,   172,     0,     0,   173,   174,   175,   176,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   177,
     178,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     179,   180,   181,   182,   183,   184,   185,   186,   187,   188,
       0,   189,   190,  1002,   554,     0,     0,  1003,     0,   191,
       0,     0,     0,     0,     0,     0,     0,   162,   163,   164,
     165,   166,   167,   168,   169,   170,     0,     0,   171,   172,
       0,     0,   173,   174,   175,   176,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   177,   178,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   179,   180,   181,
     182,   183,   184,   185,   186,   187,   188,     0,   189,   190,
    1037,   554,     0,     0,  1038,     0,   191,     0,     0,     0,
       0,     0,     0,     0,   162,   163,   164,   165,   166,   167,
     168,   169,   170,     0,     0,   171,   172,     0,     0,   173,
     174,   175,   176,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   177,   178,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,     0,   189,   190,  1040,   562,     0,
       0,  1041,     0,   191,     0,     0,     0,     0,     0,     0,
       0,   162,   163,   164,   165,   166,   167,   168,   169,   170,
       0,     0,   171,   172,     0,     0,   173,   174,   175,   176,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     177,   178,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   179,   180,   181,   182,   183,   184,   185,   186,   187,
     188,     0,   189,   190,     0,     0,     0,     0,     0,     0,
     191
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-808))

#define yytable_value_is_error(yytable_value) \
  ((yytable_value) == (-621))

static const yytype_int16 yycheck[] =
{
       2,    87,    55,    96,    90,   353,    79,   355,   472,    16,
      17,    22,   359,    20,   354,   326,   614,     2,     8,     4,
      16,    17,     8,    28,    20,   326,   412,    27,    49,    29,
      74,     4,   220,    53,   420,    86,    87,   420,    28,    90,
     575,    94,    28,   641,   594,    49,   668,   685,   574,    51,
      52,   399,   468,    67,   262,    51,   779,   472,   266,    79,
      53,   376,   508,    74,    90,   511,     2,   415,     4,   417,
     715,    91,    92,    93,   719,   458,   416,   440,    25,   787,
      16,    17,    25,    13,    20,    16,    17,    13,    67,    20,
     875,    26,    25,    27,   873,   443,   903,   250,    37,    38,
      58,    29,   518,    16,    17,    64,     8,    20,    68,    87,
      68,    90,   777,    49,   139,   113,   141,    53,    85,    61,
      51,    52,    13,   471,   907,    25,    28,    76,    64,    61,
     470,   729,   596,   651,     0,   113,   134,   802,    96,    97,
      85,   779,   740,    79,   286,    15,   288,    17,   290,   787,
      86,    87,   294,    55,    90,    91,    92,    93,   141,    13,
     939,    25,   120,   123,   124,   125,   133,    26,   110,   118,
     323,    13,    25,    25,    25,   898,    59,    60,   110,   230,
      25,   232,   203,   111,   806,   807,   736,   134,   133,    13,
     130,   134,    94,   657,   744,   130,    56,  1004,   271,   243,
       1,   134,   141,   136,   134,   139,   991,   141,   259,   139,
     136,   141,   214,   139,   993,   141,   237,   224,   220,   226,
     227,   307,   308,   309,   310,   227,   252,   950,   224,   250,
     226,  1014,    25,   581,   134,   621,   622,   945,   252,   622,
     580,   429,   657,   140,   690,   593,   250,  1026,   139,    25,
     141,   271,   592,   771,   772,   140,   307,   308,   309,   310,
     898,   312,   313,   248,   886,   903,   140,   203,   268,   632,
     134,   130,   136,   252,   360,   361,   591,   136,   230,   743,
     696,   134,   134,   134,   929,   139,  1009,   141,   224,   134,
     226,   227,    87,   224,   230,   226,   232,   139,   670,   141,
     672,   237,   323,   255,   324,    61,   258,   945,    85,   360,
     361,   224,   248,   226,   250,   139,   117,   141,   113,   323,
     113,   306,   134,   259,   375,   321,   311,   853,   743,   141,
     326,   324,    85,   306,    85,   271,   362,   113,   139,   134,
     141,   134,   964,   485,   137,    61,   140,   354,   141,    65,
     492,   107,   354,   109,    26,   873,   133,   875,   134,    37,
      38,   137,   321,    85,    85,   141,  1004,   326,    85,    97,
     306,   307,   308,   309,   310,   311,   312,   313,   725,   132,
     133,   931,   133,   362,   140,   321,    85,   323,   324,    85,
     326,   107,   120,   109,   929,   468,   697,    59,    60,   378,
      28,    85,    52,   714,    54,    55,    56,    57,    61,   416,
     132,   133,   133,    85,   416,     2,   133,     4,   354,    26,
     356,   939,   940,   611,   360,   361,   428,   429,   636,    16,
      17,    87,   437,    20,   133,   703,   132,   133,   440,   375,
     990,  1039,   710,   402,   403,   518,    85,   437,   468,   133,
      87,   437,   134,   355,   107,   440,   109,   113,   130,   141,
     132,   133,    49,   470,   136,   113,   402,   403,   470,    85,
     422,   134,   936,   991,   426,   993,   113,    64,    85,   431,
     416,   412,   868,    68,   134,   868,    52,   140,    54,    55,
      56,    57,   113,   132,   133,   447,    87,   399,   518,    86,
      87,    87,    85,    90,   440,   507,   508,    87,  1026,   511,
     100,   883,   884,   557,   450,   417,   888,   133,   890,   891,
      87,   565,   113,   130,   455,   132,   133,   113,    87,   136,
     134,    87,   468,   113,   470,   437,   121,   122,   123,   124,
     125,   443,   542,   134,   595,    87,   113,   558,    58,   570,
     133,   572,    54,    87,    85,   566,   600,   113,    68,    85,
     560,    63,    64,   136,   637,   517,   568,   134,   134,   471,
     917,   113,   760,   580,   625,    56,   923,   134,   580,   113,
     576,   113,   518,    61,   141,   592,    96,    97,    88,   600,
     592,   136,   134,    26,   573,   574,   141,   134,   600,   137,
     605,   132,   133,   141,    87,   136,   132,   133,   587,   611,
     120,    14,    15,   134,   614,   605,   203,   637,   136,   605,
      52,   130,    87,   696,    56,   997,   998,   999,  1000,   107,
     632,   109,   110,   136,   570,   136,   572,   224,    68,   226,
     227,   641,   137,   230,   580,   232,   732,   632,   113,   735,
     237,   113,    85,   138,   131,   748,   592,    85,   594,   595,
     704,   248,    61,   250,   666,   686,   668,   711,   689,   134,
     691,  1019,   259,    56,   134,  1047,   696,    68,  1018,   581,
      68,   732,    94,   635,   735,   685,   110,   110,   690,   625,
     621,   593,   110,   746,   110,   134,   632,   130,    85,   132,
     133,   637,   139,   605,   132,   133,    17,    56,   107,   711,
     109,   110,   714,   715,   134,   717,    25,   719,   714,   306,
     307,   308,   309,   310,   311,   312,   313,    26,    68,   729,
      61,   577,   137,   134,   321,   131,   323,   130,   697,   326,
     740,   700,   701,    83,    84,   132,   133,    10,   707,   708,
     686,   140,   134,   689,   733,   691,   134,   843,   760,    85,
     696,   697,   134,   140,   700,   701,   745,   354,   134,   356,
      85,   707,   708,   360,   361,    87,   107,   777,   109,   110,
     800,   121,   122,   123,   124,   125,    85,    52,   375,   134,
     742,   113,   843,    85,   815,    68,   732,   136,     8,   735,
     736,   113,   802,   131,   806,   807,   132,   133,   744,   761,
      83,    84,    13,    61,   134,   402,   403,   132,   133,   840,
      61,   134,   134,   775,   113,    52,   847,   848,   134,   416,
     851,   130,   134,   132,   133,    52,   838,   136,   134,   841,
     132,   133,    15,    52,   746,   118,   119,   120,   121,   122,
     123,   124,   125,   440,    85,   134,   115,   878,   879,   107,
     138,   109,   110,   450,   800,   134,   107,   131,   109,   110,
     829,   892,   134,   893,   853,   113,   855,    85,   724,   815,
     134,    85,   118,   470,   886,   134,    16,    17,    54,    55,
      20,    57,   134,   829,   134,   741,   916,    63,    64,   920,
     139,   132,   133,    10,   840,    87,   131,   843,   860,    10,
     134,   847,   848,   685,    88,   851,    46,    47,     9,   685,
     872,    51,    52,   134,   132,   133,   118,   929,   132,   133,
     137,   113,   134,    56,    64,    65,   201,   134,   134,   204,
     205,   206,   878,   879,    52,   134,    54,    55,    56,    57,
      58,   131,   134,   134,    10,   134,   892,   893,   108,   980,
      68,   982,   964,   131,   134,   134,    56,   988,   450,   134,
     970,   134,   134,   134,   685,    68,   136,   136,   134,    91,
     916,    89,    93,   570,   920,   572,   711,  1021,    96,    97,
      83,    84,   771,   580,   685,   931,  1014,   749,   844,    52,
     846,    54,    55,    56,    57,   592,   294,   594,   595,   781,
     856,  1018,   120,   785,  1020,   861,  1018,    99,  1020,  1021,
      57,   787,    94,   781,   790,   936,   898,   120,   121,   122,
     123,   124,   125,   900,   779,   396,    89,    -1,   625,  1039,
      -1,    -1,    95,    -1,   980,   632,   982,    40,    41,    42,
      43,    44,   988,    -1,   990,    52,   321,    54,    55,    56,
      57,   326,    -1,    52,    -1,    54,    55,    56,    57,    -1,
     781,   201,    -1,    -1,   204,   205,   206,   207,    -1,    -1,
     926,   927,  1018,     2,    52,     4,    54,    55,    56,    57,
      -1,    -1,    89,    -1,   224,    -1,   226,   227,    95,   686,
      89,    -1,   689,    -1,   691,    -1,    -1,    -1,    -1,    -1,
     697,    -1,    -1,   700,   701,    -1,    -1,  1019,    -1,    -1,
     707,   708,    -1,    -1,    -1,    -1,   898,    -1,   900,    -1,
      49,   903,    -1,   905,    53,    -1,    -1,   903,    -1,   905,
      -1,    -1,    -1,   989,    -1,   732,   411,   412,   735,   736,
      -1,    -1,    -1,    -1,    -1,   420,    -1,   744,    -1,    -1,
      79,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    91,    92,    93,    94,  1022,    -1,  1024,   945,
      -1,   947,    -1,   809,   810,    -1,   952,   898,    -1,   900,
     455,   321,   903,   458,   905,    -1,   326,   327,   328,   329,
     330,   331,   332,   333,   334,   335,   336,   337,   338,   339,
     340,   341,   342,   343,   344,   345,   346,   347,   348,   349,
     350,   351,   352,    -1,   354,    -1,    -1,    -1,   815,    -1,
      -1,    -1,  1004,    -1,  1006,    -1,    -1,  1009,  1004,  1011,
    1006,    -1,   829,    -1,    -1,  1011,    -1,   512,    -1,    -1,
      -1,    -1,    -1,   840,    -1,    -1,   843,    -1,    -1,    -1,
     847,   848,  1028,    -1,   851,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   402,   403,    -1,    -1,    -1,  1049,    -1,    -1,
     410,   411,   412,  1049,   203,    -1,   416,    -1,   418,   419,
     420,   878,   879,  1004,    -1,  1006,    -1,    -1,  1009,    -1,
    1011,    -1,    -1,    -1,    -1,   892,    -1,    -1,    -1,   439,
      -1,   576,    -1,    -1,   444,    -1,    -1,    -1,   237,    -1,
      -1,    -1,    -1,    -1,    -1,   455,    -1,    -1,   458,   248,
      -1,   250,    -1,   920,    -1,   961,   962,    -1,  1049,   965,
     470,   967,   968,   608,   931,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   271,    -1,    -1,    -1,   621,   622,    -1,    -1,
      -1,    -1,    -1,     2,    -1,     4,   496,   497,    -1,    -1,
      -1,    -1,    -1,    -1,    52,    -1,    54,    55,    56,    57,
      58,    -1,   512,    -1,   649,    -1,    -1,   306,    -1,    -1,
      68,    -1,   311,   980,    -1,   982,    -1,    -1,    -1,    -1,
      -1,   988,    -1,   990,   323,   324,  1032,  1033,  1034,  1035,
      49,    89,    -1,    -1,    53,    -1,    -1,    95,    96,    97,
      52,    -1,    54,    55,    56,    57,    58,    -1,   693,  1055,
      -1,  1018,    -1,    -1,    -1,    -1,    68,   356,    -1,    -1,
      79,    -1,   120,    -1,    -1,   123,   576,    -1,    -1,   714,
     580,    -1,    91,    92,    93,    94,    -1,    89,    -1,    -1,
      -1,    -1,   592,   141,    96,    97,    -1,    -1,    -1,    52,
      -1,    54,    55,    56,    57,    58,    -1,    -1,   608,    -1,
      -1,    -1,    -1,    -1,    -1,    68,    -1,    -1,   120,    -1,
      -1,   621,   622,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   768,    -1,    -1,    89,    -1,    -1,    -1,
      -1,    -1,    95,    96,    97,    -1,    -1,    -1,    -1,   649,
      -1,   440,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   450,    -1,     2,    -1,     4,    -1,   120,    -1,    -1,
     123,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   468,
      -1,    -1,   817,   136,     2,    -1,     4,    -1,    -1,    -1,
      -1,    -1,    -1,   693,   203,    -1,    -1,   697,   698,    -1,
     700,   701,    -1,     2,    -1,     4,    -1,   707,   708,    -1,
      49,    -1,    -1,    -1,   714,   850,    -1,    -1,    -1,    -1,
      52,    -1,    54,    55,    56,    57,    58,    -1,   237,   518,
      -1,    49,    -1,   868,    -1,    53,    68,    -1,    -1,   248,
      -1,   250,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      49,   751,    -1,    -1,    -1,   755,   756,    89,   758,   759,
      -1,    79,   271,    95,    96,    97,    -1,    -1,   768,    -1,
      -1,    -1,    -1,    91,    92,    93,    -1,    -1,    -1,    -1,
      -1,   570,    -1,   572,    -1,    -1,    -1,    -1,   120,    -1,
      -1,   123,    91,    -1,    -1,    -1,    -1,   306,    -1,    -1,
      -1,    -1,   311,    -1,   136,   594,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   323,   324,    -1,   817,    -1,    -1,
      -1,   821,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   829,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   632,    -1,    -1,    -1,   356,   637,    -1,
     850,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   203,    -1,    -1,   867,   868,    -1,
      -1,    -1,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,   203,    -1,    83,    84,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   686,   237,    -1,
     689,    -1,   691,    -1,   203,    -1,    -1,   696,    -1,   248,
      -1,   250,    -1,    -1,    -1,    -1,    -1,    -1,   114,   237,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     248,   440,   250,    -1,    -1,    -1,    -1,    -1,   237,    -1,
      -1,   450,    -1,    -1,    -1,   141,    -1,   736,    -1,   248,
      -1,   250,    -1,   271,    -1,   744,    -1,    -1,    -1,   468,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   306,    -1,    -1,
      -1,    52,   311,    54,    55,    56,    57,    58,    -1,    -1,
      -1,    -1,    -1,    -1,   323,    -1,    -1,    68,   306,    -1,
      -1,    -1,    -1,   311,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   323,   324,   306,    89,   518,
      -1,   800,   311,    -1,    95,    96,    97,   356,  1018,    -1,
      -1,    -1,    -1,    -1,   323,    -1,   815,   326,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   356,   120,
      44,    -1,   123,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   840,    -1,    -1,    -1,    -1,    -1,   356,   847,   848,
      -1,   570,   851,   572,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    -1,    -1,    83,
      84,    -1,    -1,    -1,    -1,   594,    -1,    -1,    -1,   878,
     879,    -1,    52,    -1,    54,    55,    56,    57,    58,    -1,
      -1,   440,    -1,   892,   893,    -1,    -1,    -1,    68,    -1,
     114,   450,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   440,   632,    -1,    -1,    -1,   916,   637,    89,
     134,   920,   450,    -1,    -1,    95,    96,    97,    -1,    -1,
      -1,   440,   931,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     468,   450,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     120,    -1,    -1,   123,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   686,    -1,    -1,
     689,    -1,   691,    -1,    -1,    -1,    -1,   696,    -1,    -1,
      -1,   980,    -1,   982,    52,    53,    -1,    -1,    56,   988,
     518,   990,    -1,    -1,    -1,    -1,    -1,    -1,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    -1,    -1,    77,
      78,    -1,    -1,    81,    82,    83,    84,   736,    -1,    -1,
      -1,   570,    -1,   572,    -1,   744,    -1,    95,    96,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   570,    -1,   572,   594,    -1,    -1,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,    -1,   127,
     128,   570,    -1,   572,    -1,    -1,   594,   135,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   800,    -1,   632,    -1,   594,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   815,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   632,    -1,    -1,    -1,    -1,   637,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   840,    -1,   632,    -1,    -1,    -1,    -1,   847,   848,
      -1,    -1,   851,    -1,    -1,    -1,    -1,   686,    -1,    -1,
     689,    -1,   691,    -1,    -1,    -1,    -1,    -1,   676,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   686,   878,
     879,   689,    -1,   691,    -1,    -1,    -1,    -1,   696,    -1,
      -1,    -1,    -1,   892,   893,    -1,    -1,   686,    -1,    -1,
     689,    -1,   691,    -1,    -1,    -1,    -1,   736,   697,    -1,
      -1,    -1,    -1,    -1,    -1,   744,    -1,   916,    -1,    -1,
      -1,   920,    -1,    -1,    -1,    -1,    -1,    -1,   736,    -1,
      -1,    -1,   931,    -1,    -1,    -1,   744,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   736,    68,    69,
      70,    71,    72,    73,    74,   744,    -1,    77,    78,    -1,
      -1,    -1,    -1,    83,    84,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   980,    -1,   982,    -1,    -1,   815,    -1,    -1,   988,
      -1,   990,   800,    -1,    -1,    -1,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,    -1,   815,    -1,    -1,
      -1,   840,    -1,    -1,    -1,    -1,    -1,    -1,   847,   848,
      -1,    -1,   851,    -1,    -1,    -1,   815,    -1,    -1,    -1,
      -1,    -1,   840,    -1,    -1,    -1,    -1,    -1,    -1,   847,
     848,    -1,    -1,   851,    -1,    -1,    -1,    -1,    -1,   878,
     879,   840,    -1,    -1,    -1,    -1,    -1,    -1,   847,   848,
      -1,    -1,   851,   892,    -1,    -1,    -1,    -1,    -1,    -1,
     878,   879,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   892,   893,    -1,    -1,    -1,   878,
     879,   920,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   931,   892,    -1,    -1,    -1,    -1,   916,    -1,
      -1,    -1,   920,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   931,    -1,    -1,    -1,   916,    -1,    -1,
      -1,   920,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   931,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   980,    -1,   982,    -1,    -1,    -1,    -1,    -1,   988,
      -1,   990,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   980,    -1,   982,    -1,    -1,    -1,    -1,    -1,
     988,    -1,   990,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   980,    -1,   982,    -1,    -1,    -1,    -1,    -1,   988,
      -1,   990,     0,     1,    -1,     3,     4,     5,     6,     7,
      -1,    -1,    -1,    11,    12,    -1,    -1,    -1,    16,    -1,
      18,    19,    20,    21,    22,    23,    24,    -1,    -1,    -1,
      -1,    -1,    30,    31,    32,    33,    34,    35,    36,    -1,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      -1,    59,    60,    -1,    62,    63,    64,    -1,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    86,    -1,
      -1,    89,    90,    -1,    92,    93,    -1,    95,    -1,    -1,
      98,    99,   100,   101,   102,   103,   104,   105,   106,     0,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,     8,     9,    10,
      -1,    -1,    13,    14,    15,    -1,    17,    -1,   126,   127,
     128,    -1,    44,    -1,    -1,    26,    27,    28,    29,    -1,
      -1,   139,    -1,   141,    -1,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    -1,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    -1,
      -1,    83,    84,    -1,    -1,    -1,    -1,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      -1,    -1,    83,    84,    85,    -1,    87,    88,    -1,    -1,
      -1,    -1,   114,    94,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,    -1,    -1,   108,    -1,    -1,
     111,    -1,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,    -1,    -1,    -1,    -1,   130,
     131,   132,   133,   134,     0,    -1,   137,   138,   139,    -1,
     141,    -1,     8,     9,    10,    -1,    -1,    13,    14,    15,
      -1,    17,    -1,    -1,    -1,    -1,    -1,    44,    -1,    25,
      -1,    27,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    43,    44,    -1,
      -1,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    -1,    -1,    83,    84,    -1,    -1,
      -1,    -1,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    -1,    -1,    83,    84,    85,
      -1,    87,    88,    -1,    -1,    -1,    -1,   114,    94,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,    -1,
      -1,    -1,   108,    -1,    -1,   111,    -1,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
      -1,    -1,    -1,    -1,    -1,   131,   132,   133,   134,     0,
      -1,   137,   138,   139,    -1,   141,    -1,     8,     9,    10,
      -1,    -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    25,    -1,    27,    28,    29,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    -1,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    -1,
      -1,    83,    84,    -1,    -1,    -1,    -1,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      -1,    -1,    83,    84,    85,    -1,    87,    88,    -1,    -1,
      -1,    -1,   114,    94,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,    -1,    -1,   108,    -1,    -1,
     111,    -1,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,    -1,    -1,    -1,    -1,    -1,
     131,   132,   133,   134,     0,    -1,   137,   138,   139,    -1,
     141,    -1,     8,     9,    10,    -1,    -1,    13,    14,    15,
      -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      26,    27,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    43,    44,    -1,
      -1,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    -1,    -1,    83,    84,    -1,    -1,
      -1,    -1,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    -1,    -1,    83,    84,    85,
      -1,    -1,    88,    -1,    -1,    -1,    -1,    -1,    94,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,    -1,
      -1,    -1,   108,    -1,    -1,   111,    -1,    -1,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
      -1,    -1,    -1,    -1,   130,   131,   132,   133,   134,     0,
      -1,   137,   138,   139,    -1,   141,    -1,     8,     9,    10,
      -1,    -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    26,    27,    28,    29,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    -1,    -1,    68,    69,    70,    71,
      72,    73,    74,    75,    -1,    77,    78,    -1,    -1,    -1,
      -1,    83,    84,    -1,    -1,    -1,    -1,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      -1,    -1,    83,    84,    85,    -1,    -1,    88,    -1,    -1,
      -1,    -1,    -1,    94,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,    -1,    -1,    -1,   108,    -1,    -1,
     111,    -1,    -1,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,    -1,    -1,    -1,    -1,   130,
     131,   132,   133,   134,     0,    -1,   137,   138,   139,    -1,
     141,    -1,     8,     9,    10,    -1,    -1,    13,    14,    15,
      -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    27,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    43,    44,    -1,
      -1,    68,    69,    70,    71,    72,    73,    74,    -1,    -1,
      77,    78,    -1,    -1,    -1,    -1,    83,    84,    -1,    -1,
      -1,    -1,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    -1,    -1,    83,    84,    85,
      -1,    87,    88,    -1,    -1,    -1,    -1,    -1,    94,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,    -1,
      -1,    -1,   108,    -1,    -1,   111,    -1,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
      -1,    -1,    -1,    -1,    -1,   131,   132,   133,   134,     0,
      -1,   137,   138,   139,    -1,   141,    -1,     8,     9,    10,
      -1,    -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    26,    27,    28,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      -1,    -1,    83,    84,    85,    -1,    -1,    88,    -1,    -1,
      -1,    -1,    -1,    94,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,    -1,    -1,
      -1,    -1,    -1,   114,    -1,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,    -1,    -1,    -1,    -1,   130,
     131,   132,   133,   134,     0,   136,   137,   138,   139,    -1,
     141,    -1,     8,     9,    10,    -1,    -1,    13,    14,    15,
      -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    27,    28,    29,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    43,    44,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    -1,    -1,    83,    84,    85,
      -1,    -1,    88,    -1,    -1,    -1,    -1,    -1,    94,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   108,    -1,    -1,   111,    -1,    -1,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
      -1,    -1,    -1,    -1,    -1,   131,   132,   133,   134,     0,
      -1,   137,   138,   139,    -1,   141,    -1,     8,     9,    10,
      -1,    -1,    13,    14,    15,    -1,    17,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    26,    27,    28,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      -1,    -1,    83,    84,    85,    -1,    -1,    88,    -1,    -1,
      -1,    -1,    -1,    94,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,    -1,    -1,
      -1,    -1,    -1,   114,    -1,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,    -1,    -1,    -1,    -1,   130,
     131,   132,   133,   134,     0,   136,   137,   138,   139,    -1,
     141,    -1,     8,     9,    10,    -1,    -1,    13,    14,    15,
      -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    27,    28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    38,    -1,    40,    41,    42,    43,    44,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    -1,    -1,    83,    84,    85,
      -1,    -1,    88,    -1,    -1,    -1,    -1,    -1,    94,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   108,    -1,    -1,    -1,    -1,    -1,   114,    -1,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
      -1,    -1,    -1,    -1,    -1,   131,   132,   133,   134,     0,
     136,   137,   138,   139,    -1,   141,    -1,     8,     9,    10,
      -1,    -1,    -1,    14,    15,    -1,    17,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    26,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    37,    38,    -1,    40,
      41,    42,    43,    44,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      -1,    -1,    83,    84,    85,    -1,    87,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   108,    -1,    -1,
      -1,    -1,   113,   114,    -1,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,    -1,    -1,    -1,    -1,   130,
     131,   132,   133,   134,     0,    -1,   137,    -1,   139,    -1,
     141,    -1,     8,     9,    10,    -1,    -1,    -1,    14,    15,
      -1,    17,    -1,     0,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,     8,     9,    10,    -1,    -1,    13,    14,    15,    -1,
      17,    37,    38,    -1,    40,    41,    42,    43,    44,    26,
      27,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      37,    38,    -1,    40,    41,    42,    43,    44,    -1,    -1,
      -1,    -1,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    -1,    -1,    83,    84,    85,
      -1,    87,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    85,    -1,
      -1,    -1,   108,    -1,    -1,    -1,    -1,   113,   114,    -1,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
      -1,   108,    -1,    -1,    -1,   131,   132,   133,   134,    -1,
      -1,   137,    -1,   139,    -1,   141,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   130,   131,    -1,   133,    -1,    -1,   136,
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
     125,    -1,   127,   128,    -1,    -1,    -1,    -1,    -1,    -1,
     135
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
     171,   172,   182,   196,   215,   216,   217,   218,   219,   220,
     221,   222,   223,   224,   225,   251,   252,   266,   267,   268,
     269,   270,   271,   272,   275,   277,   278,   290,   292,   293,
     294,   295,   296,   297,   298,   329,   340,   147,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    56,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    77,    78,    81,    82,    83,    84,    95,    96,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   127,
     128,   135,   175,   176,   177,   178,   180,   181,   290,   292,
      39,    58,    86,    89,    95,    96,    97,   127,   164,   172,
     182,   184,   189,   192,   194,   215,   294,   295,   297,   298,
     327,   328,   189,   189,   136,   190,   191,   136,   186,   190,
     136,   141,   334,    54,   177,   334,   148,   130,    21,    22,
      30,    31,    32,   163,   182,   215,   182,    56,     1,    47,
      89,   150,   151,   152,   154,   166,   167,   340,   198,   199,
     185,   194,   327,   340,   184,   326,   327,   340,    46,    86,
     126,   134,   171,   196,   215,   294,   295,   298,   243,   244,
      54,    55,    57,   175,   282,   291,   281,   282,   283,   140,
     273,   140,   279,   140,   276,   140,   280,    59,    60,   159,
     182,   182,   139,   141,   333,   338,   339,    40,    41,    42,
      43,    44,    37,    38,    26,   130,   186,   190,   257,    28,
     249,   113,   134,    89,    95,   168,   113,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      83,    84,   114,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,    85,   132,   133,   197,   157,   158,   158,
     202,   204,   158,   333,   339,    86,   165,   172,   215,   231,
     294,   295,   298,    52,    56,    83,    86,   173,   174,   215,
     294,   295,   298,   174,    33,    34,    35,    36,    49,    50,
      51,    52,    56,   136,   175,   296,   324,    85,   133,   332,
     257,   269,    87,    87,   134,   184,    56,   184,   184,   184,
     113,    88,   134,   193,   340,    85,   132,   133,    87,    87,
     134,   193,   189,   334,   335,   189,   188,   189,   194,   327,
     340,   157,   335,   157,    54,    63,    64,   155,   136,   183,
     130,   150,    85,   133,    87,   154,   153,   166,   137,   333,
     339,   335,   335,   157,   138,   134,   141,   337,   134,   337,
     131,   337,   334,    56,    59,    60,   168,   170,   134,    85,
     132,   133,   245,    61,   107,   109,   110,   284,   110,   284,
     110,    65,   284,   110,   110,   274,   284,   110,    61,   110,
     110,   110,   274,   110,    61,   110,    68,    68,   139,   147,
     158,   158,   158,   158,   154,   157,   157,   259,   258,    94,
     161,   250,    95,   159,   184,   194,   195,   166,   134,   171,
     134,   156,   159,   172,   182,   184,   195,   182,   182,   182,
     182,   182,   182,   182,   182,   182,   182,   182,   182,   182,
     182,   182,   182,   182,   182,   182,   182,   182,   182,   182,
     182,   182,   182,    52,    53,    56,   180,   256,   330,   331,
     188,    52,    53,    56,   180,   255,   330,   149,   150,    13,
     227,   338,   227,   158,   158,   333,    17,   260,    56,    85,
     132,   133,    25,   157,    52,    56,   173,     1,   117,   299,
     338,    85,   132,   133,   211,   325,   212,   332,    52,    56,
     330,   159,   182,   159,   182,   179,   182,   184,    95,   184,
     192,   327,    52,    56,   188,    52,    56,   328,   335,   137,
     335,   134,   134,   335,   177,   201,   182,   145,   131,   330,
     330,   182,   130,   335,   152,   200,   327,   134,   170,    52,
      56,   188,    52,    56,    52,    54,    55,    56,    57,    58,
      68,    89,    95,    96,    97,   120,   123,   136,   247,   302,
     304,   305,   306,   307,   308,   309,   312,   313,   314,   315,
     318,   319,   320,   321,   322,   286,   285,   140,   284,   140,
     140,   140,   182,   182,    76,   118,   238,   239,   340,   238,
     162,   238,   184,   134,   335,   170,   134,   113,    44,   334,
      87,    87,   186,   190,   254,   334,   336,    87,    87,   186,
     190,   253,    10,   226,     8,   262,   340,   150,    13,   150,
      27,   228,   338,   228,   260,   194,   226,    52,    56,   188,
      52,    56,   206,   209,   338,   300,   208,    52,    56,   173,
     188,   149,   157,   136,   301,   304,   213,   186,   187,   190,
     340,    44,   177,   184,   193,    87,    87,   336,    87,    87,
     327,   157,   131,   145,   335,   337,   168,   336,   113,   184,
      52,    89,    95,   232,   233,   234,   306,   304,   246,   134,
     303,   134,   323,   340,    52,   134,   323,   134,   303,    52,
     134,   303,    52,   287,    54,    55,    57,   289,   298,    52,
      58,   235,   237,   240,   308,   310,   311,   314,   316,   317,
     320,   322,   334,   150,   150,   238,   150,    95,   184,   170,
     182,   115,   159,   182,   159,   182,   161,   186,   138,    87,
     159,   182,   159,   182,   161,   187,   184,   195,   263,   340,
      15,   230,   340,    14,   229,   230,   230,   203,   205,   226,
     134,   227,   336,   158,   338,   158,   149,   336,   226,   335,
     304,   149,   338,   175,   257,   249,   182,    87,   134,   335,
     131,   184,   234,   134,   306,   134,   335,   240,    29,   111,
     248,   302,   307,   318,   320,   309,   314,   322,   308,   315,
     320,   308,   288,   113,    86,   215,   240,   118,   134,   236,
     134,   323,   323,   134,   236,   134,   236,   139,    10,   131,
     150,    10,   184,   182,   159,   182,    88,   264,   340,   150,
       9,   265,   340,   158,   226,   226,   150,   150,   184,   150,
     228,   210,   338,   226,   335,   226,   214,   335,   233,   134,
      95,   232,   137,   150,   150,   134,   303,   134,   303,   323,
     134,   303,   134,   303,   303,   150,   215,    56,    85,   118,
     235,   317,   320,   310,   314,   308,   316,   320,   308,    52,
     241,   242,   305,   131,    86,   172,   215,   294,   295,   298,
     227,   150,   227,   226,   226,   230,   260,   261,   207,   149,
     301,   134,   233,   134,   306,    10,   131,   308,   320,   308,
     308,   108,    52,    56,   134,   236,   134,   236,   323,   134,
     236,   134,   236,   236,   134,   334,    56,    85,   132,   133,
     150,   150,   150,   226,   149,   233,   134,   303,   134,   303,
     303,   303,   308,   320,   308,   308,   242,    52,    56,   188,
      52,    56,   262,   229,   226,   226,   233,   308,   236,   134,
     236,   236,   236,   336,   303,   308,   236
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
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
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


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, parser)
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
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
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
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , parser);
      YYFPRINTF (stderr, "\n");
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

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
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
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

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
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
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

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
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
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
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

/* Line 1806 of yacc.c  */
#line 855 "ripper.y"
    {
			lex_state = EXPR_BEG;
#if 0
			local_push(compile_for_eval || rb_parse_in_main());
#endif
			local_push(0);

		    }
    break;

  case 3:

/* Line 1806 of yacc.c  */
#line 864 "ripper.y"
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
		    }
    break;

  case 4:

/* Line 1806 of yacc.c  */
#line 887 "ripper.y"
    {
#if 0
			void_stmts((yyvsp[(1) - (2)].val));
			fixup_nodes(&deferred_nodes);
#endif

			(yyval.val) = (yyvsp[(1) - (2)].val);
		    }
    break;

  case 5:

/* Line 1806 of yacc.c  */
#line 898 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch2(stmts_add, dispatch0(stmts_new),
						  dispatch0(void_stmt));

		    }
    break;

  case 6:

/* Line 1806 of yacc.c  */
#line 907 "ripper.y"
    {
#if 0
			(yyval.val) = newline_node((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = dispatch2(stmts_add, dispatch0(stmts_new), (yyvsp[(1) - (1)].val));

		    }
    break;

  case 7:

/* Line 1806 of yacc.c  */
#line 915 "ripper.y"
    {
#if 0
			(yyval.val) = block_append((yyvsp[(1) - (3)].val), newline_node((yyvsp[(3) - (3)].val)));
#endif
			(yyval.val) = dispatch2(stmts_add, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 8:

/* Line 1806 of yacc.c  */
#line 923 "ripper.y"
    {
			(yyval.val) = remove_begin((yyvsp[(2) - (2)].val));
		    }
    break;

  case 10:

/* Line 1806 of yacc.c  */
#line 930 "ripper.y"
    {
#if 0
			/* local_push(0); */
#endif

		    }
    break;

  case 11:

/* Line 1806 of yacc.c  */
#line 937 "ripper.y"
    {
#if 0
			ruby_eval_tree_begin = block_append(ruby_eval_tree_begin,
							    (yyvsp[(4) - (5)].val));
			/* NEW_PREEXE($4)); */
			/* local_pop(); */
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(BEGIN, (yyvsp[(4) - (5)].val));

		    }
    break;

  case 12:

/* Line 1806 of yacc.c  */
#line 954 "ripper.y"
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

		    }
    break;

  case 13:

/* Line 1806 of yacc.c  */
#line 984 "ripper.y"
    {
#if 0
			void_stmts((yyvsp[(1) - (2)].val));
			fixup_nodes(&deferred_nodes);
#endif

			(yyval.val) = (yyvsp[(1) - (2)].val);
		    }
    break;

  case 14:

/* Line 1806 of yacc.c  */
#line 995 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch2(stmts_add, dispatch0(stmts_new),
						  dispatch0(void_stmt));

		    }
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 1004 "ripper.y"
    {
#if 0
			(yyval.val) = newline_node((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = dispatch2(stmts_add, dispatch0(stmts_new), (yyvsp[(1) - (1)].val));

		    }
    break;

  case 16:

/* Line 1806 of yacc.c  */
#line 1012 "ripper.y"
    {
#if 0
			(yyval.val) = block_append((yyvsp[(1) - (3)].val), newline_node((yyvsp[(3) - (3)].val)));
#endif
			(yyval.val) = dispatch2(stmts_add, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 17:

/* Line 1806 of yacc.c  */
#line 1020 "ripper.y"
    {
			(yyval.val) = remove_begin((yyvsp[(2) - (2)].val));
		    }
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 1026 "ripper.y"
    {
			(yyval.val) = (yyvsp[(1) - (1)].val);
		    }
    break;

  case 19:

/* Line 1806 of yacc.c  */
#line 1030 "ripper.y"
    {
			yyerror("BEGIN is permitted only at toplevel");
#if 0
			/* local_push(0); */
#endif

		    }
    break;

  case 20:

/* Line 1806 of yacc.c  */
#line 1038 "ripper.y"
    {
#if 0
			ruby_eval_tree_begin = block_append(ruby_eval_tree_begin,
							    (yyvsp[(4) - (5)].val));
			/* NEW_PREEXE($4)); */
			/* local_pop(); */
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(BEGIN, (yyvsp[(4) - (5)].val));

		    }
    break;

  case 21:

/* Line 1806 of yacc.c  */
#line 1050 "ripper.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 22:

/* Line 1806 of yacc.c  */
#line 1051 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ALIAS((yyvsp[(2) - (4)].val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = dispatch2(alias, (yyvsp[(2) - (4)].val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 23:

/* Line 1806 of yacc.c  */
#line 1059 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_VALIAS((yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(var_alias, (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 24:

/* Line 1806 of yacc.c  */
#line 1067 "ripper.y"
    {
#if 0
			char buf[2];
			buf[0] = '$';
			buf[1] = (char)(yyvsp[(3) - (3)].val)->nd_nth;
			(yyval.val) = NEW_VALIAS((yyvsp[(2) - (3)].val), rb_intern2(buf, 2));
#endif
			(yyval.val) = dispatch2(var_alias, (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 25:

/* Line 1806 of yacc.c  */
#line 1078 "ripper.y"
    {
#if 0
			yyerror("can't make alias for the number variables");
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch2(var_alias, (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));
			(yyval.val) = dispatch1(alias_error, (yyval.val));

		    }
    break;

  case 26:

/* Line 1806 of yacc.c  */
#line 1088 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch1(undef, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 27:

/* Line 1806 of yacc.c  */
#line 1096 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_IF(cond((yyvsp[(3) - (3)].val)), remove_begin((yyvsp[(1) - (3)].val)), 0);
			fixpos((yyval.val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(if_mod, (yyvsp[(3) - (3)].val), (yyvsp[(1) - (3)].val));

		    }
    break;

  case 28:

/* Line 1806 of yacc.c  */
#line 1105 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_UNLESS(cond((yyvsp[(3) - (3)].val)), remove_begin((yyvsp[(1) - (3)].val)), 0);
			fixpos((yyval.val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(unless_mod, (yyvsp[(3) - (3)].val), (yyvsp[(1) - (3)].val));

		    }
    break;

  case 29:

/* Line 1806 of yacc.c  */
#line 1114 "ripper.y"
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

		    }
    break;

  case 30:

/* Line 1806 of yacc.c  */
#line 1127 "ripper.y"
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

		    }
    break;

  case 31:

/* Line 1806 of yacc.c  */
#line 1140 "ripper.y"
    {
#if 0
			NODE *resq = NEW_RESBODY(0, remove_begin((yyvsp[(3) - (3)].val)), 0);
			(yyval.val) = NEW_RESCUE(remove_begin((yyvsp[(1) - (3)].val)), resq, 0);
#endif
			(yyval.val) = dispatch2(rescue_mod, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 32:

/* Line 1806 of yacc.c  */
#line 1149 "ripper.y"
    {
			if (in_def || in_single) {
			    rb_warn0("END in method; use at_exit");
			}
#if 0
			(yyval.val) = NEW_POSTEXE(NEW_NODE(
			    NODE_SCOPE, 0 /* tbl */, (yyvsp[(3) - (4)].val) /* body */, 0 /* args */));
#endif
			(yyval.val) = dispatch1(END, (yyvsp[(3) - (4)].val));

		    }
    break;

  case 34:

/* Line 1806 of yacc.c  */
#line 1162 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (3)].val));
			(yyvsp[(1) - (3)].val)->nd_value = (yyvsp[(3) - (3)].val);
			(yyval.val) = (yyvsp[(1) - (3)].val);
#endif
			(yyval.val) = dispatch2(massign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 35:

/* Line 1806 of yacc.c  */
#line 1172 "ripper.y"
    {
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = new_op_assign((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));
		    }
    break;

  case 36:

/* Line 1806 of yacc.c  */
#line 1177 "ripper.y"
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

		    }
    break;

  case 37:

/* Line 1806 of yacc.c  */
#line 1198 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    }
    break;

  case 38:

/* Line 1806 of yacc.c  */
#line 1203 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    }
    break;

  case 39:

/* Line 1806 of yacc.c  */
#line 1208 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON2((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
			(yyval.val) = new_const_op_assign((yyval.val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
#endif
			(yyval.val) = dispatch2(const_path_field, (yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
			(yyval.val) = dispatch3(opassign, (yyval.val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));

		    }
    break;

  case 40:

/* Line 1806 of yacc.c  */
#line 1218 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_intern("::"), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    }
    break;

  case 41:

/* Line 1806 of yacc.c  */
#line 1223 "ripper.y"
    {
#if 0
			rb_backref_error((yyvsp[(1) - (3)].val));
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch2(assign, dispatch1(var_field, (yyvsp[(1) - (3)].val)), (yyvsp[(3) - (3)].val));
			(yyval.val) = dispatch1(assign_error, (yyval.val));

		    }
    break;

  case 42:

/* Line 1806 of yacc.c  */
#line 1233 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = node_assign((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(assign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 43:

/* Line 1806 of yacc.c  */
#line 1242 "ripper.y"
    {
#if 0
			(yyvsp[(1) - (3)].val)->nd_value = (yyvsp[(3) - (3)].val);
			(yyval.val) = (yyvsp[(1) - (3)].val);
#endif
			(yyval.val) = dispatch2(massign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 44:

/* Line 1806 of yacc.c  */
#line 1251 "ripper.y"
    {
#if 0
			(yyvsp[(1) - (3)].val)->nd_value = (yyvsp[(3) - (3)].val);
			(yyval.val) = (yyvsp[(1) - (3)].val);
#endif
			(yyval.val) = dispatch2(massign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 46:

/* Line 1806 of yacc.c  */
#line 1263 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = node_assign((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(assign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 47:

/* Line 1806 of yacc.c  */
#line 1272 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = node_assign((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(assign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 49:

/* Line 1806 of yacc.c  */
#line 1285 "ripper.y"
    {
#if 0
			(yyval.val) = logop(NODE_AND, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("and"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 50:

/* Line 1806 of yacc.c  */
#line 1293 "ripper.y"
    {
#if 0
			(yyval.val) = logop(NODE_OR, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("or"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 51:

/* Line 1806 of yacc.c  */
#line 1301 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op(cond((yyvsp[(3) - (3)].val)), '!');
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("not"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 52:

/* Line 1806 of yacc.c  */
#line 1309 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op(cond((yyvsp[(2) - (2)].val)), '!');
#endif
			(yyval.val) = dispatch2(unary, ripper_id2sym('!'), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 1320 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (1)].val));
			(yyval.val) = (yyvsp[(1) - (1)].val);
		        if (!(yyval.val)) (yyval.val) = NEW_NIL();
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    }
    break;

  case 58:

/* Line 1806 of yacc.c  */
#line 1337 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (4)].val), (yyvsp[(2) - (4)].val), (yyvsp[(3) - (4)].val));
			(yyval.val) = method_arg((yyval.val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 59:

/* Line 1806 of yacc.c  */
#line 1348 "ripper.y"
    {
			(yyvsp[(1) - (1)].vars) = dyna_push();
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    }
    break;

  case 60:

/* Line 1806 of yacc.c  */
#line 1358 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ITER((yyvsp[(3) - (5)].val),(yyvsp[(4) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(2) - (5)].num));
#endif
			(yyval.val) = dispatch2(brace_block, escape_Qundef((yyvsp[(3) - (5)].val)), (yyvsp[(4) - (5)].val));

			dyna_pop((yyvsp[(1) - (5)].vars));
		    }
    break;

  case 61:

/* Line 1806 of yacc.c  */
#line 1370 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_FCALL((yyvsp[(1) - (1)].val), 0);
			nd_set_line((yyval.val), tokline);
#endif

		    }
    break;

  case 62:

/* Line 1806 of yacc.c  */
#line 1380 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (2)].val);
			(yyval.val)->nd_args = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch2(command, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 63:

/* Line 1806 of yacc.c  */
#line 1389 "ripper.y"
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

		    }
    break;

  case 64:

/* Line 1806 of yacc.c  */
#line 1402 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
			fixpos((yyval.val), (yyvsp[(1) - (4)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (4)].val), ripper_id2sym('.'), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 65:

/* Line 1806 of yacc.c  */
#line 1411 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(4) - (5)].val),(yyvsp[(5) - (5)].val));
		        (yyvsp[(5) - (5)].val)->nd_iter = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = (yyvsp[(5) - (5)].val);
			fixpos((yyval.val), (yyvsp[(1) - (5)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(5) - (5)].val));

		   }
    break;

  case 66:

/* Line 1806 of yacc.c  */
#line 1423 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
			fixpos((yyval.val), (yyvsp[(1) - (4)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (4)].val), ripper_intern("::"), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 67:

/* Line 1806 of yacc.c  */
#line 1432 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(4) - (5)].val),(yyvsp[(5) - (5)].val));
		        (yyvsp[(5) - (5)].val)->nd_iter = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = (yyvsp[(5) - (5)].val);
			fixpos((yyval.val), (yyvsp[(1) - (5)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (5)].val), ripper_intern("::"), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(5) - (5)].val));

		   }
    break;

  case 68:

/* Line 1806 of yacc.c  */
#line 1444 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_SUPER((yyvsp[(2) - (2)].val));
			fixpos((yyval.val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(super, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 69:

/* Line 1806 of yacc.c  */
#line 1453 "ripper.y"
    {
#if 0
			(yyval.val) = new_yield((yyvsp[(2) - (2)].val));
			fixpos((yyval.val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(yield, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 70:

/* Line 1806 of yacc.c  */
#line 1462 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_RETURN(ret_args((yyvsp[(2) - (2)].val)));
#endif
			(yyval.val) = dispatch1(return, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 71:

/* Line 1806 of yacc.c  */
#line 1470 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_BREAK(ret_args((yyvsp[(2) - (2)].val)));
#endif
			(yyval.val) = dispatch1(break, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 72:

/* Line 1806 of yacc.c  */
#line 1478 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_NEXT(ret_args((yyvsp[(2) - (2)].val)));
#endif
			(yyval.val) = dispatch1(next, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 74:

/* Line 1806 of yacc.c  */
#line 1489 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 76:

/* Line 1806 of yacc.c  */
#line 1500 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(NEW_LIST((yyvsp[(2) - (3)].val)), 0);
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 77:

/* Line 1806 of yacc.c  */
#line 1510 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (1)].val), 0);
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    }
    break;

  case 78:

/* Line 1806 of yacc.c  */
#line 1518 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(list_append((yyvsp[(1) - (2)].val),(yyvsp[(2) - (2)].val)), 0);
#endif
			(yyval.val) = mlhs_add((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 79:

/* Line 1806 of yacc.c  */
#line 1526 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 80:

/* Line 1806 of yacc.c  */
#line 1534 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (5)].val), NEW_POSTARG((yyvsp[(3) - (5)].val),(yyvsp[(5) - (5)].val)));
#endif
			(yyvsp[(1) - (5)].val) = mlhs_add_star((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
			(yyval.val) = mlhs_add((yyvsp[(1) - (5)].val), (yyvsp[(5) - (5)].val));

		    }
    break;

  case 81:

/* Line 1806 of yacc.c  */
#line 1543 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (2)].val), -1);
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (2)].val), Qnil);

		    }
    break;

  case 82:

/* Line 1806 of yacc.c  */
#line 1551 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (4)].val), NEW_POSTARG(-1, (yyvsp[(4) - (4)].val)));
#endif
			(yyvsp[(1) - (4)].val) = mlhs_add_star((yyvsp[(1) - (4)].val), Qnil);
			(yyval.val) = mlhs_add((yyvsp[(1) - (4)].val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 83:

/* Line 1806 of yacc.c  */
#line 1560 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 84:

/* Line 1806 of yacc.c  */
#line 1568 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, NEW_POSTARG((yyvsp[(2) - (4)].val),(yyvsp[(4) - (4)].val)));
#endif
			(yyvsp[(2) - (4)].val) = mlhs_add_star(mlhs_new(), (yyvsp[(2) - (4)].val));
			(yyval.val) = mlhs_add((yyvsp[(2) - (4)].val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 85:

/* Line 1806 of yacc.c  */
#line 1577 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, -1);
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), Qnil);

		    }
    break;

  case 86:

/* Line 1806 of yacc.c  */
#line 1585 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, NEW_POSTARG(-1, (yyvsp[(3) - (3)].val)));
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), Qnil);
			(yyval.val) = mlhs_add((yyval.val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 88:

/* Line 1806 of yacc.c  */
#line 1597 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 89:

/* Line 1806 of yacc.c  */
#line 1607 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST((yyvsp[(1) - (2)].val));
#endif
			(yyval.val) = mlhs_add(mlhs_new(), (yyvsp[(1) - (2)].val));

		    }
    break;

  case 90:

/* Line 1806 of yacc.c  */
#line 1615 "ripper.y"
    {
#if 0
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = mlhs_add((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));

		    }
    break;

  case 91:

/* Line 1806 of yacc.c  */
#line 1625 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = mlhs_add(mlhs_new(), (yyvsp[(1) - (1)].val));

		    }
    break;

  case 92:

/* Line 1806 of yacc.c  */
#line 1633 "ripper.y"
    {
#if 0
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = mlhs_add((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 93:

/* Line 1806 of yacc.c  */
#line 1643 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
		    }
    break;

  case 94:

/* Line 1806 of yacc.c  */
#line 1647 "ripper.y"
    {
		        (yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
		    }
    break;

  case 95:

/* Line 1806 of yacc.c  */
#line 1651 "ripper.y"
    {
#if 0
			(yyval.val) = aryset((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val));
#endif
			(yyval.val) = dispatch2(aref_field, (yyvsp[(1) - (4)].val), escape_Qundef((yyvsp[(3) - (4)].val)));

		    }
    break;

  case 96:

/* Line 1806 of yacc.c  */
#line 1659 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(field, (yyvsp[(1) - (3)].val), ripper_id2sym('.'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 97:

/* Line 1806 of yacc.c  */
#line 1667 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(const_path_field, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 98:

/* Line 1806 of yacc.c  */
#line 1675 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(field, (yyvsp[(1) - (3)].val), ripper_id2sym('.'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 99:

/* Line 1806 of yacc.c  */
#line 1683 "ripper.y"
    {
#if 0
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.val) = NEW_CDECL(0, 0, NEW_COLON2((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val)));
#endif
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.val) = dispatch2(const_path_field, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 100:

/* Line 1806 of yacc.c  */
#line 1695 "ripper.y"
    {
#if 0
			if (in_def || in_single)
			    yyerror("dynamic constant assignment");
			(yyval.val) = NEW_CDECL(0, 0, NEW_COLON3((yyvsp[(2) - (2)].val)));
#endif
			(yyval.val) = dispatch1(top_const_field, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 101:

/* Line 1806 of yacc.c  */
#line 1705 "ripper.y"
    {
#if 0
			rb_backref_error((yyvsp[(1) - (1)].val));
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(var_field, (yyvsp[(1) - (1)].val));
			(yyval.val) = dispatch1(assign_error, (yyval.val));

		    }
    break;

  case 102:

/* Line 1806 of yacc.c  */
#line 1717 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
#if 0
			if (!(yyval.val)) (yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(var_field, (yyval.val));

		    }
    break;

  case 103:

/* Line 1806 of yacc.c  */
#line 1726 "ripper.y"
    {
		        (yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
#if 0
		        if (!(yyval.val)) (yyval.val) = NEW_BEGIN(0);
#endif
		        (yyval.val) = dispatch1(var_field, (yyval.val));

		    }
    break;

  case 104:

/* Line 1806 of yacc.c  */
#line 1735 "ripper.y"
    {
#if 0
			(yyval.val) = aryset((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val));
#endif
			(yyval.val) = dispatch2(aref_field, (yyvsp[(1) - (4)].val), escape_Qundef((yyvsp[(3) - (4)].val)));

		    }
    break;

  case 105:

/* Line 1806 of yacc.c  */
#line 1743 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(field, (yyvsp[(1) - (3)].val), ripper_id2sym('.'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 106:

/* Line 1806 of yacc.c  */
#line 1751 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(field, (yyvsp[(1) - (3)].val), ripper_intern("::"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 107:

/* Line 1806 of yacc.c  */
#line 1759 "ripper.y"
    {
#if 0
			(yyval.val) = attrset((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(field, (yyvsp[(1) - (3)].val), ripper_id2sym('.'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 108:

/* Line 1806 of yacc.c  */
#line 1767 "ripper.y"
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

		    }
    break;

  case 109:

/* Line 1806 of yacc.c  */
#line 1780 "ripper.y"
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

		    }
    break;

  case 110:

/* Line 1806 of yacc.c  */
#line 1793 "ripper.y"
    {
#if 0
			rb_backref_error((yyvsp[(1) - (1)].val));
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(assign_error, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 111:

/* Line 1806 of yacc.c  */
#line 1804 "ripper.y"
    {
#if 0
			yyerror("class/module name must be CONSTANT");
#endif
			(yyval.val) = dispatch1(class_name_error, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 113:

/* Line 1806 of yacc.c  */
#line 1815 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON3((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(top_const_ref, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 114:

/* Line 1806 of yacc.c  */
#line 1823 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON2(0, (yyval.val));
#endif
			(yyval.val) = dispatch1(const_ref, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 115:

/* Line 1806 of yacc.c  */
#line 1831 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON2((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(const_path_ref, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 119:

/* Line 1806 of yacc.c  */
#line 1844 "ripper.y"
    {
			lex_state = EXPR_ENDFN;
			(yyval.val) = (yyvsp[(1) - (1)].val);
		    }
    break;

  case 120:

/* Line 1806 of yacc.c  */
#line 1849 "ripper.y"
    {
			lex_state = EXPR_ENDFN;
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].id);
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    }
    break;

  case 123:

/* Line 1806 of yacc.c  */
#line 1864 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIT(ID2SYM((yyvsp[(1) - (1)].val)));
#endif
			(yyval.val) = dispatch1(symbol_literal, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 125:

/* Line 1806 of yacc.c  */
#line 1875 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_UNDEF((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 126:

/* Line 1806 of yacc.c  */
#line 1882 "ripper.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 127:

/* Line 1806 of yacc.c  */
#line 1883 "ripper.y"
    {
#if 0
			(yyval.val) = block_append((yyvsp[(1) - (4)].val), NEW_UNDEF((yyvsp[(4) - (4)].val)));
#endif
			rb_ary_push((yyvsp[(1) - (4)].val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 128:

/* Line 1806 of yacc.c  */
#line 1892 "ripper.y"
    { ifndef_ripper((yyval.val) = '|'); }
    break;

  case 129:

/* Line 1806 of yacc.c  */
#line 1893 "ripper.y"
    { ifndef_ripper((yyval.val) = '^'); }
    break;

  case 130:

/* Line 1806 of yacc.c  */
#line 1894 "ripper.y"
    { ifndef_ripper((yyval.val) = '&'); }
    break;

  case 131:

/* Line 1806 of yacc.c  */
#line 1895 "ripper.y"
    { ifndef_ripper((yyval.val) = tCMP); }
    break;

  case 132:

/* Line 1806 of yacc.c  */
#line 1896 "ripper.y"
    { ifndef_ripper((yyval.val) = tEQ); }
    break;

  case 133:

/* Line 1806 of yacc.c  */
#line 1897 "ripper.y"
    { ifndef_ripper((yyval.val) = tEQQ); }
    break;

  case 134:

/* Line 1806 of yacc.c  */
#line 1898 "ripper.y"
    { ifndef_ripper((yyval.val) = tMATCH); }
    break;

  case 135:

/* Line 1806 of yacc.c  */
#line 1899 "ripper.y"
    { ifndef_ripper((yyval.val) = tNMATCH); }
    break;

  case 136:

/* Line 1806 of yacc.c  */
#line 1900 "ripper.y"
    { ifndef_ripper((yyval.val) = '>'); }
    break;

  case 137:

/* Line 1806 of yacc.c  */
#line 1901 "ripper.y"
    { ifndef_ripper((yyval.val) = tGEQ); }
    break;

  case 138:

/* Line 1806 of yacc.c  */
#line 1902 "ripper.y"
    { ifndef_ripper((yyval.val) = '<'); }
    break;

  case 139:

/* Line 1806 of yacc.c  */
#line 1903 "ripper.y"
    { ifndef_ripper((yyval.val) = tLEQ); }
    break;

  case 140:

/* Line 1806 of yacc.c  */
#line 1904 "ripper.y"
    { ifndef_ripper((yyval.val) = tNEQ); }
    break;

  case 141:

/* Line 1806 of yacc.c  */
#line 1905 "ripper.y"
    { ifndef_ripper((yyval.val) = tLSHFT); }
    break;

  case 142:

/* Line 1806 of yacc.c  */
#line 1906 "ripper.y"
    { ifndef_ripper((yyval.val) = tRSHFT); }
    break;

  case 143:

/* Line 1806 of yacc.c  */
#line 1907 "ripper.y"
    { ifndef_ripper((yyval.val) = '+'); }
    break;

  case 144:

/* Line 1806 of yacc.c  */
#line 1908 "ripper.y"
    { ifndef_ripper((yyval.val) = '-'); }
    break;

  case 145:

/* Line 1806 of yacc.c  */
#line 1909 "ripper.y"
    { ifndef_ripper((yyval.val) = '*'); }
    break;

  case 146:

/* Line 1806 of yacc.c  */
#line 1910 "ripper.y"
    { ifndef_ripper((yyval.val) = '*'); }
    break;

  case 147:

/* Line 1806 of yacc.c  */
#line 1911 "ripper.y"
    { ifndef_ripper((yyval.val) = '/'); }
    break;

  case 148:

/* Line 1806 of yacc.c  */
#line 1912 "ripper.y"
    { ifndef_ripper((yyval.val) = '%'); }
    break;

  case 149:

/* Line 1806 of yacc.c  */
#line 1913 "ripper.y"
    { ifndef_ripper((yyval.val) = tPOW); }
    break;

  case 150:

/* Line 1806 of yacc.c  */
#line 1914 "ripper.y"
    { ifndef_ripper((yyval.val) = tDSTAR); }
    break;

  case 151:

/* Line 1806 of yacc.c  */
#line 1915 "ripper.y"
    { ifndef_ripper((yyval.val) = '!'); }
    break;

  case 152:

/* Line 1806 of yacc.c  */
#line 1916 "ripper.y"
    { ifndef_ripper((yyval.val) = '~'); }
    break;

  case 153:

/* Line 1806 of yacc.c  */
#line 1917 "ripper.y"
    { ifndef_ripper((yyval.val) = tUPLUS); }
    break;

  case 154:

/* Line 1806 of yacc.c  */
#line 1918 "ripper.y"
    { ifndef_ripper((yyval.val) = tUMINUS); }
    break;

  case 155:

/* Line 1806 of yacc.c  */
#line 1919 "ripper.y"
    { ifndef_ripper((yyval.val) = tAREF); }
    break;

  case 156:

/* Line 1806 of yacc.c  */
#line 1920 "ripper.y"
    { ifndef_ripper((yyval.val) = tASET); }
    break;

  case 157:

/* Line 1806 of yacc.c  */
#line 1921 "ripper.y"
    { ifndef_ripper((yyval.val) = '`'); }
    break;

  case 199:

/* Line 1806 of yacc.c  */
#line 1939 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = node_assign((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(assign, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 200:

/* Line 1806 of yacc.c  */
#line 1948 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (5)].val));
		        (yyvsp[(3) - (5)].val) = NEW_RESCUE((yyvsp[(3) - (5)].val), NEW_RESBODY(0,(yyvsp[(5) - (5)].val),0), 0);
			(yyval.val) = node_assign((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
#endif
			(yyval.val) = dispatch2(assign, (yyvsp[(1) - (5)].val), dispatch2(rescue_mod, (yyvsp[(3) - (5)].val), (yyvsp[(5) - (5)].val)));

		    }
    break;

  case 201:

/* Line 1806 of yacc.c  */
#line 1958 "ripper.y"
    {
			value_expr((yyvsp[(3) - (3)].val));
			(yyval.val) = new_op_assign((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));
		    }
    break;

  case 202:

/* Line 1806 of yacc.c  */
#line 1963 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(3) - (5)].val));
		        (yyvsp[(3) - (5)].val) = NEW_RESCUE((yyvsp[(3) - (5)].val), NEW_RESBODY(0,(yyvsp[(5) - (5)].val),0), 0);
#endif
			(yyvsp[(3) - (5)].val) = dispatch2(rescue_mod, (yyvsp[(3) - (5)].val), (yyvsp[(5) - (5)].val));

			(yyval.val) = new_op_assign((yyvsp[(1) - (5)].val), (yyvsp[(2) - (5)].val), (yyvsp[(3) - (5)].val));
		    }
    break;

  case 203:

/* Line 1806 of yacc.c  */
#line 1973 "ripper.y"
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

		    }
    break;

  case 204:

/* Line 1806 of yacc.c  */
#line 1999 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    }
    break;

  case 205:

/* Line 1806 of yacc.c  */
#line 2004 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    }
    break;

  case 206:

/* Line 1806 of yacc.c  */
#line 2009 "ripper.y"
    {
			value_expr((yyvsp[(5) - (5)].val));
			(yyval.val) = new_attr_op_assign((yyvsp[(1) - (5)].val), ripper_intern("::"), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
		    }
    break;

  case 207:

/* Line 1806 of yacc.c  */
#line 2014 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON2((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
			(yyval.val) = new_const_op_assign((yyval.val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
#endif
			(yyval.val) = dispatch2(const_path_field, (yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val));
			(yyval.val) = dispatch3(opassign, (yyval.val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));

		    }
    break;

  case 208:

/* Line 1806 of yacc.c  */
#line 2024 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON3((yyvsp[(2) - (4)].val));
			(yyval.val) = new_const_op_assign((yyval.val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = dispatch1(top_const_field, (yyvsp[(2) - (4)].val));
			(yyval.val) = dispatch3(opassign, (yyval.val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 209:

/* Line 1806 of yacc.c  */
#line 2034 "ripper.y"
    {
#if 0
			rb_backref_error((yyvsp[(1) - (3)].val));
			(yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(var_field, (yyvsp[(1) - (3)].val));
			(yyval.val) = dispatch3(opassign, (yyval.val), (yyvsp[(2) - (3)].val), (yyvsp[(3) - (3)].val));
			(yyval.val) = dispatch1(assign_error, (yyval.val));

		    }
    break;

  case 210:

/* Line 1806 of yacc.c  */
#line 2045 "ripper.y"
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

		    }
    break;

  case 211:

/* Line 1806 of yacc.c  */
#line 2059 "ripper.y"
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

		    }
    break;

  case 212:

/* Line 1806 of yacc.c  */
#line 2073 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '+', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('+'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 213:

/* Line 1806 of yacc.c  */
#line 2081 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '-', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('-'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 214:

/* Line 1806 of yacc.c  */
#line 2089 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '*', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('*'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 215:

/* Line 1806 of yacc.c  */
#line 2097 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '/', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('/'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 216:

/* Line 1806 of yacc.c  */
#line 2105 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '%', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('%'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 217:

/* Line 1806 of yacc.c  */
#line 2113 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tPOW, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("**"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 218:

/* Line 1806 of yacc.c  */
#line 2121 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL(call_bin_op((yyvsp[(2) - (4)].val), tPOW, (yyvsp[(4) - (4)].val)), tUMINUS, 0);
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(2) - (4)].val), ripper_intern("**"), (yyvsp[(4) - (4)].val));
			(yyval.val) = dispatch2(unary, ripper_intern("-@"), (yyval.val));

		    }
    break;

  case 219:

/* Line 1806 of yacc.c  */
#line 2130 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL(call_bin_op((yyvsp[(2) - (4)].val), tPOW, (yyvsp[(4) - (4)].val)), tUMINUS, 0);
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(2) - (4)].val), ripper_intern("**"), (yyvsp[(4) - (4)].val));
			(yyval.val) = dispatch2(unary, ripper_intern("-@"), (yyval.val));

		    }
    break;

  case 220:

/* Line 1806 of yacc.c  */
#line 2139 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op((yyvsp[(2) - (2)].val), tUPLUS);
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("+@"), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 221:

/* Line 1806 of yacc.c  */
#line 2147 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op((yyvsp[(2) - (2)].val), tUMINUS);
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("-@"), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 222:

/* Line 1806 of yacc.c  */
#line 2155 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '|', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('|'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 223:

/* Line 1806 of yacc.c  */
#line 2163 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '^', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('^'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 224:

/* Line 1806 of yacc.c  */
#line 2171 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '&', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('&'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 225:

/* Line 1806 of yacc.c  */
#line 2179 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tCMP, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("<=>"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 226:

/* Line 1806 of yacc.c  */
#line 2187 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '>', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('>'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 227:

/* Line 1806 of yacc.c  */
#line 2195 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tGEQ, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern(">="), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 228:

/* Line 1806 of yacc.c  */
#line 2203 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), '<', (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ID2SYM('<'), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 229:

/* Line 1806 of yacc.c  */
#line 2211 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tLEQ, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("<="), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 230:

/* Line 1806 of yacc.c  */
#line 2219 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tEQ, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("=="), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 231:

/* Line 1806 of yacc.c  */
#line 2227 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tEQQ, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("==="), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 232:

/* Line 1806 of yacc.c  */
#line 2235 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tNEQ, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("!="), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 233:

/* Line 1806 of yacc.c  */
#line 2243 "ripper.y"
    {
#if 0
			(yyval.val) = match_op((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
                        if (nd_type((yyvsp[(1) - (3)].val)) == NODE_LIT && RB_TYPE_P((yyvsp[(1) - (3)].val)->nd_lit, T_REGEXP)) {
                            (yyval.val) = reg_named_capture_assign((yyvsp[(1) - (3)].val)->nd_lit, (yyval.val));
                        }
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("=~"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 234:

/* Line 1806 of yacc.c  */
#line 2254 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tNMATCH, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("!~"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 235:

/* Line 1806 of yacc.c  */
#line 2262 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op(cond((yyvsp[(2) - (2)].val)), '!');
#endif
			(yyval.val) = dispatch2(unary, ID2SYM('!'), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 236:

/* Line 1806 of yacc.c  */
#line 2270 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op((yyvsp[(2) - (2)].val), '~');
#endif
			(yyval.val) = dispatch2(unary, ID2SYM('~'), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 237:

/* Line 1806 of yacc.c  */
#line 2278 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tLSHFT, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("<<"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 238:

/* Line 1806 of yacc.c  */
#line 2286 "ripper.y"
    {
#if 0
			(yyval.val) = call_bin_op((yyvsp[(1) - (3)].val), tRSHFT, (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern(">>"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 239:

/* Line 1806 of yacc.c  */
#line 2294 "ripper.y"
    {
#if 0
			(yyval.val) = logop(NODE_AND, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("&&"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 240:

/* Line 1806 of yacc.c  */
#line 2302 "ripper.y"
    {
#if 0
			(yyval.val) = logop(NODE_OR, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch3(binary, (yyvsp[(1) - (3)].val), ripper_intern("||"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 241:

/* Line 1806 of yacc.c  */
#line 2309 "ripper.y"
    {in_defined = 1;}
    break;

  case 242:

/* Line 1806 of yacc.c  */
#line 2310 "ripper.y"
    {
#if 0
			in_defined = 0;
			(yyval.val) = NEW_DEFINED((yyvsp[(4) - (4)].val));
#endif
			in_defined = 0;
			(yyval.val) = dispatch1(defined, (yyvsp[(4) - (4)].val));

		    }
    break;

  case 243:

/* Line 1806 of yacc.c  */
#line 2320 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (6)].val));
			(yyval.val) = NEW_IF(cond((yyvsp[(1) - (6)].val)), (yyvsp[(3) - (6)].val), (yyvsp[(6) - (6)].val));
			fixpos((yyval.val), (yyvsp[(1) - (6)].val));
#endif
			(yyval.val) = dispatch3(ifop, (yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(6) - (6)].val));

		    }
    break;

  case 244:

/* Line 1806 of yacc.c  */
#line 2330 "ripper.y"
    {
			(yyval.val) = (yyvsp[(1) - (1)].val);
		    }
    break;

  case 245:

/* Line 1806 of yacc.c  */
#line 2336 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (1)].val));
			(yyval.val) = (yyvsp[(1) - (1)].val);
		        if (!(yyval.val)) (yyval.val) = NEW_NIL();
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    }
    break;

  case 247:

/* Line 1806 of yacc.c  */
#line 2349 "ripper.y"
    {
			(yyval.val) = (yyvsp[(1) - (2)].val);
		    }
    break;

  case 248:

/* Line 1806 of yacc.c  */
#line 2353 "ripper.y"
    {
#if 0
			(yyval.val) = arg_append((yyvsp[(1) - (4)].val), NEW_HASH((yyvsp[(3) - (4)].val)));
#endif
			(yyval.val) = arg_add_assocs((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val));

		    }
    break;

  case 249:

/* Line 1806 of yacc.c  */
#line 2361 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].val)));
#endif
			(yyval.val) = arg_add_assocs(arg_new(), (yyvsp[(1) - (2)].val));

		    }
    break;

  case 250:

/* Line 1806 of yacc.c  */
#line 2371 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(arg_paren, escape_Qundef((yyvsp[(2) - (3)].val)));

		    }
    break;

  case 255:

/* Line 1806 of yacc.c  */
#line 2387 "ripper.y"
    {
		      (yyval.val) = (yyvsp[(1) - (2)].val);
		    }
    break;

  case 256:

/* Line 1806 of yacc.c  */
#line 2391 "ripper.y"
    {
#if 0
			(yyval.val) = arg_append((yyvsp[(1) - (4)].val), NEW_HASH((yyvsp[(3) - (4)].val)));
#endif
			(yyval.val) = arg_add_assocs((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val));

		    }
    break;

  case 257:

/* Line 1806 of yacc.c  */
#line 2399 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].val)));
#endif
			(yyval.val) = arg_add_assocs(arg_new(), (yyvsp[(1) - (2)].val));

		    }
    break;

  case 258:

/* Line 1806 of yacc.c  */
#line 2409 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (1)].val));
			(yyval.val) = NEW_LIST((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = arg_add(arg_new(), (yyvsp[(1) - (1)].val));

		    }
    break;

  case 259:

/* Line 1806 of yacc.c  */
#line 2418 "ripper.y"
    {
#if 0
			(yyval.val) = arg_blk_pass((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = arg_add_optblock((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 260:

/* Line 1806 of yacc.c  */
#line 2426 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST(NEW_HASH((yyvsp[(1) - (2)].val)));
			(yyval.val) = arg_blk_pass((yyval.val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = arg_add_assocs(arg_new(), (yyvsp[(1) - (2)].val));
			(yyval.val) = arg_add_optblock((yyval.val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 261:

/* Line 1806 of yacc.c  */
#line 2436 "ripper.y"
    {
#if 0
			(yyval.val) = arg_append((yyvsp[(1) - (4)].val), NEW_HASH((yyvsp[(3) - (4)].val)));
			(yyval.val) = arg_blk_pass((yyval.val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = arg_add_optblock(arg_add_assocs((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val)), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 262:

/* Line 1806 of yacc.c  */
#line 2447 "ripper.y"
    {
			(yyval.val) = arg_add_block(arg_new(), (yyvsp[(1) - (1)].val));
		    }
    break;

  case 263:

/* Line 1806 of yacc.c  */
#line 2453 "ripper.y"
    {
			(yyval.val) = cmdarg_stack;
			CMDARG_PUSH(1);
		    }
    break;

  case 264:

/* Line 1806 of yacc.c  */
#line 2458 "ripper.y"
    {
			/* CMDARG_POP() */
			cmdarg_stack = (yyvsp[(1) - (2)].val);
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    }
    break;

  case 265:

/* Line 1806 of yacc.c  */
#line 2466 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_BLOCK_PASS((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = (yyvsp[(2) - (2)].val);

		    }
    break;

  case 266:

/* Line 1806 of yacc.c  */
#line 2476 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    }
    break;

  case 267:

/* Line 1806 of yacc.c  */
#line 2480 "ripper.y"
    {
			(yyval.val) = 0;
		    }
    break;

  case 268:

/* Line 1806 of yacc.c  */
#line 2486 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = arg_add(arg_new(), (yyvsp[(1) - (1)].val));

		    }
    break;

  case 269:

/* Line 1806 of yacc.c  */
#line 2494 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_SPLAT((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = arg_add_star(arg_new(), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 270:

/* Line 1806 of yacc.c  */
#line 2502 "ripper.y"
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

		    }
    break;

  case 271:

/* Line 1806 of yacc.c  */
#line 2516 "ripper.y"
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

		    }
    break;

  case 272:

/* Line 1806 of yacc.c  */
#line 2532 "ripper.y"
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

		    }
    break;

  case 273:

/* Line 1806 of yacc.c  */
#line 2546 "ripper.y"
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

		    }
    break;

  case 274:

/* Line 1806 of yacc.c  */
#line 2561 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_SPLAT((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = mrhs_add_star(mrhs_new(), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 285:

/* Line 1806 of yacc.c  */
#line 2581 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_FCALL((yyvsp[(1) - (1)].val), 0);
#endif
			(yyval.val) = method_arg(dispatch1(fcall, (yyvsp[(1) - (1)].val)), arg_new());

		    }
    break;

  case 286:

/* Line 1806 of yacc.c  */
#line 2589 "ripper.y"
    {
			(yyvsp[(1) - (1)].val) = cmdarg_stack;
			cmdarg_stack = 0;
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    }
    break;

  case 287:

/* Line 1806 of yacc.c  */
#line 2599 "ripper.y"
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

		    }
    break;

  case 288:

/* Line 1806 of yacc.c  */
#line 2616 "ripper.y"
    {lex_state = EXPR_ENDARG;}
    break;

  case 289:

/* Line 1806 of yacc.c  */
#line 2617 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch1(paren, 0);

		    }
    break;

  case 290:

/* Line 1806 of yacc.c  */
#line 2625 "ripper.y"
    {
			(yyvsp[(1) - (1)].val) = cmdarg_stack;
			cmdarg_stack = 0;
		    }
    break;

  case 291:

/* Line 1806 of yacc.c  */
#line 2629 "ripper.y"
    {lex_state = EXPR_ENDARG;}
    break;

  case 292:

/* Line 1806 of yacc.c  */
#line 2630 "ripper.y"
    {
			cmdarg_stack = (yyvsp[(1) - (5)].val);
#if 0
			(yyval.val) = (yyvsp[(3) - (5)].val);
#endif
			(yyval.val) = dispatch1(paren, (yyvsp[(3) - (5)].val));

		    }
    break;

  case 293:

/* Line 1806 of yacc.c  */
#line 2639 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(paren, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 294:

/* Line 1806 of yacc.c  */
#line 2647 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON2((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(const_path_ref, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 295:

/* Line 1806 of yacc.c  */
#line 2655 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_COLON3((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(top_const_ref, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 296:

/* Line 1806 of yacc.c  */
#line 2663 "ripper.y"
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

		    }
    break;

  case 297:

/* Line 1806 of yacc.c  */
#line 2676 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_HASH((yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = dispatch1(hash, escape_Qundef((yyvsp[(2) - (3)].val)));

		    }
    break;

  case 298:

/* Line 1806 of yacc.c  */
#line 2684 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_RETURN(0);
#endif
			(yyval.val) = dispatch0(return0);

		    }
    break;

  case 299:

/* Line 1806 of yacc.c  */
#line 2692 "ripper.y"
    {
#if 0
			(yyval.val) = new_yield((yyvsp[(3) - (4)].val));
#endif
			(yyval.val) = dispatch1(yield, dispatch1(paren, (yyvsp[(3) - (4)].val)));

		    }
    break;

  case 300:

/* Line 1806 of yacc.c  */
#line 2700 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_YIELD(0);
#endif
			(yyval.val) = dispatch1(yield, dispatch1(paren, arg_new()));

		    }
    break;

  case 301:

/* Line 1806 of yacc.c  */
#line 2708 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_YIELD(0);
#endif
			(yyval.val) = dispatch0(yield0);

		    }
    break;

  case 302:

/* Line 1806 of yacc.c  */
#line 2715 "ripper.y"
    {in_defined = 1;}
    break;

  case 303:

/* Line 1806 of yacc.c  */
#line 2716 "ripper.y"
    {
#if 0
			in_defined = 0;
			(yyval.val) = NEW_DEFINED((yyvsp[(5) - (6)].val));
#endif
			in_defined = 0;
			(yyval.val) = dispatch1(defined, (yyvsp[(5) - (6)].val));

		    }
    break;

  case 304:

/* Line 1806 of yacc.c  */
#line 2726 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op(cond((yyvsp[(3) - (4)].val)), '!');
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("not"), (yyvsp[(3) - (4)].val));

		    }
    break;

  case 305:

/* Line 1806 of yacc.c  */
#line 2734 "ripper.y"
    {
#if 0
			(yyval.val) = call_uni_op(cond(NEW_NIL()), '!');
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("not"), Qnil);

		    }
    break;

  case 306:

/* Line 1806 of yacc.c  */
#line 2742 "ripper.y"
    {
#if 0
			(yyvsp[(2) - (2)].val)->nd_iter = (yyvsp[(1) - (2)].val);
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = method_arg(dispatch1(fcall, (yyvsp[(1) - (2)].val)), arg_new());
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 308:

/* Line 1806 of yacc.c  */
#line 2753 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(1) - (2)].val)->nd_args, (yyvsp[(2) - (2)].val));
			(yyvsp[(2) - (2)].val)->nd_iter = (yyvsp[(1) - (2)].val);
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = method_add_block((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 309:

/* Line 1806 of yacc.c  */
#line 2763 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    }
    break;

  case 310:

/* Line 1806 of yacc.c  */
#line 2770 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_IF(cond((yyvsp[(2) - (6)].val)), (yyvsp[(4) - (6)].val), (yyvsp[(5) - (6)].val));
			fixpos((yyval.val), (yyvsp[(2) - (6)].val));
#endif
			(yyval.val) = dispatch3(if, (yyvsp[(2) - (6)].val), (yyvsp[(4) - (6)].val), escape_Qundef((yyvsp[(5) - (6)].val)));

		    }
    break;

  case 311:

/* Line 1806 of yacc.c  */
#line 2782 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_UNLESS(cond((yyvsp[(2) - (6)].val)), (yyvsp[(4) - (6)].val), (yyvsp[(5) - (6)].val));
			fixpos((yyval.val), (yyvsp[(2) - (6)].val));
#endif
			(yyval.val) = dispatch3(unless, (yyvsp[(2) - (6)].val), (yyvsp[(4) - (6)].val), escape_Qundef((yyvsp[(5) - (6)].val)));

		    }
    break;

  case 312:

/* Line 1806 of yacc.c  */
#line 2790 "ripper.y"
    {COND_PUSH(1);}
    break;

  case 313:

/* Line 1806 of yacc.c  */
#line 2790 "ripper.y"
    {COND_POP();}
    break;

  case 314:

/* Line 1806 of yacc.c  */
#line 2793 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_WHILE(cond((yyvsp[(3) - (7)].val)), (yyvsp[(6) - (7)].val), 1);
			fixpos((yyval.val), (yyvsp[(3) - (7)].val));
#endif
			(yyval.val) = dispatch2(while, (yyvsp[(3) - (7)].val), (yyvsp[(6) - (7)].val));

		    }
    break;

  case 315:

/* Line 1806 of yacc.c  */
#line 2801 "ripper.y"
    {COND_PUSH(1);}
    break;

  case 316:

/* Line 1806 of yacc.c  */
#line 2801 "ripper.y"
    {COND_POP();}
    break;

  case 317:

/* Line 1806 of yacc.c  */
#line 2804 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_UNTIL(cond((yyvsp[(3) - (7)].val)), (yyvsp[(6) - (7)].val), 1);
			fixpos((yyval.val), (yyvsp[(3) - (7)].val));
#endif
			(yyval.val) = dispatch2(until, (yyvsp[(3) - (7)].val), (yyvsp[(6) - (7)].val));

		    }
    break;

  case 318:

/* Line 1806 of yacc.c  */
#line 2815 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CASE((yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val));
			fixpos((yyval.val), (yyvsp[(2) - (5)].val));
#endif
			(yyval.val) = dispatch2(case, (yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val));

		    }
    break;

  case 319:

/* Line 1806 of yacc.c  */
#line 2824 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CASE(0, (yyvsp[(3) - (4)].val));
#endif
			(yyval.val) = dispatch2(case, Qnil, (yyvsp[(3) - (4)].val));

		    }
    break;

  case 320:

/* Line 1806 of yacc.c  */
#line 2832 "ripper.y"
    {COND_PUSH(1);}
    break;

  case 321:

/* Line 1806 of yacc.c  */
#line 2834 "ripper.y"
    {COND_POP();}
    break;

  case 322:

/* Line 1806 of yacc.c  */
#line 2837 "ripper.y"
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

		    }
    break;

  case 323:

/* Line 1806 of yacc.c  */
#line 2898 "ripper.y"
    {
			if (in_def || in_single)
			    yyerror("class definition in method body");
			local_push(0);
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    }
    break;

  case 324:

/* Line 1806 of yacc.c  */
#line 2909 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CLASS((yyvsp[(2) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(3) - (6)].val));
			nd_set_line((yyval.val), (yyvsp[(4) - (6)].num));
#endif
			(yyval.val) = dispatch3(class, (yyvsp[(2) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val));

			local_pop();
		    }
    break;

  case 325:

/* Line 1806 of yacc.c  */
#line 2919 "ripper.y"
    {
			(yyval.num) = in_def;
			in_def = 0;
		    }
    break;

  case 326:

/* Line 1806 of yacc.c  */
#line 2924 "ripper.y"
    {
			(yyval.num) = in_single;
			in_single = 0;
			local_push(0);
		    }
    break;

  case 327:

/* Line 1806 of yacc.c  */
#line 2931 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_SCLASS((yyvsp[(3) - (8)].val), (yyvsp[(7) - (8)].val));
			fixpos((yyval.val), (yyvsp[(3) - (8)].val));
#endif
			(yyval.val) = dispatch2(sclass, (yyvsp[(3) - (8)].val), (yyvsp[(7) - (8)].val));

			local_pop();
			in_def = (yyvsp[(4) - (8)].num);
			in_single = (yyvsp[(6) - (8)].num);
		    }
    break;

  case 328:

/* Line 1806 of yacc.c  */
#line 2943 "ripper.y"
    {
			if (in_def || in_single)
			    yyerror("module definition in method body");
			local_push(0);
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    }
    break;

  case 329:

/* Line 1806 of yacc.c  */
#line 2954 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MODULE((yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(3) - (5)].num));
#endif
			(yyval.val) = dispatch2(module, (yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val));

			local_pop();
		    }
    break;

  case 330:

/* Line 1806 of yacc.c  */
#line 2964 "ripper.y"
    {
			(yyval.id) = cur_mid;
			cur_mid = (yyvsp[(2) - (2)].val);
			in_def++;
			local_push(0);
		    }
    break;

  case 331:

/* Line 1806 of yacc.c  */
#line 2973 "ripper.y"
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
		    }
    break;

  case 332:

/* Line 1806 of yacc.c  */
#line 2986 "ripper.y"
    {lex_state = EXPR_FNAME;}
    break;

  case 333:

/* Line 1806 of yacc.c  */
#line 2987 "ripper.y"
    {
			in_single++;
			lex_state = EXPR_ENDFN; /* force for args */
			local_push(0);
		    }
    break;

  case 334:

/* Line 1806 of yacc.c  */
#line 2995 "ripper.y"
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
		    }
    break;

  case 335:

/* Line 1806 of yacc.c  */
#line 3008 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_BREAK(0);
#endif
			(yyval.val) = dispatch1(break, arg_new());

		    }
    break;

  case 336:

/* Line 1806 of yacc.c  */
#line 3016 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_NEXT(0);
#endif
			(yyval.val) = dispatch1(next, arg_new());

		    }
    break;

  case 337:

/* Line 1806 of yacc.c  */
#line 3024 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_REDO();
#endif
			(yyval.val) = dispatch0(redo);

		    }
    break;

  case 338:

/* Line 1806 of yacc.c  */
#line 3032 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_RETRY();
#endif
			(yyval.val) = dispatch0(retry);

		    }
    break;

  case 339:

/* Line 1806 of yacc.c  */
#line 3042 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (1)].val));
			(yyval.val) = (yyvsp[(1) - (1)].val);
		        if (!(yyval.val)) (yyval.val) = NEW_NIL();
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    }
    break;

  case 340:

/* Line 1806 of yacc.c  */
#line 3054 "ripper.y"
    {
			token_info_push("begin");
		    }
    break;

  case 341:

/* Line 1806 of yacc.c  */
#line 3060 "ripper.y"
    {
			token_info_push("if");
		    }
    break;

  case 342:

/* Line 1806 of yacc.c  */
#line 3066 "ripper.y"
    {
			token_info_push("unless");
		    }
    break;

  case 343:

/* Line 1806 of yacc.c  */
#line 3072 "ripper.y"
    {
			token_info_push("while");
		    }
    break;

  case 344:

/* Line 1806 of yacc.c  */
#line 3078 "ripper.y"
    {
			token_info_push("until");
		    }
    break;

  case 345:

/* Line 1806 of yacc.c  */
#line 3084 "ripper.y"
    {
			token_info_push("case");
		    }
    break;

  case 346:

/* Line 1806 of yacc.c  */
#line 3090 "ripper.y"
    {
			token_info_push("for");
		    }
    break;

  case 347:

/* Line 1806 of yacc.c  */
#line 3096 "ripper.y"
    {
			token_info_push("class");
		    }
    break;

  case 348:

/* Line 1806 of yacc.c  */
#line 3102 "ripper.y"
    {
			token_info_push("module");
		    }
    break;

  case 349:

/* Line 1806 of yacc.c  */
#line 3108 "ripper.y"
    {
			token_info_push("def");
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    }
    break;

  case 350:

/* Line 1806 of yacc.c  */
#line 3118 "ripper.y"
    {
			token_info_pop("end");
		    }
    break;

  case 351:

/* Line 1806 of yacc.c  */
#line 3126 "ripper.y"
    { (yyval.val) = Qnil; }
    break;

  case 353:

/* Line 1806 of yacc.c  */
#line 3132 "ripper.y"
    { (yyval.val) = (yyvsp[(2) - (2)].val); }
    break;

  case 354:

/* Line 1806 of yacc.c  */
#line 3139 "ripper.y"
    { (yyval.val) = Qnil; }
    break;

  case 357:

/* Line 1806 of yacc.c  */
#line 3148 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_IF(cond((yyvsp[(2) - (5)].val)), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
			fixpos((yyval.val), (yyvsp[(2) - (5)].val));
#endif
			(yyval.val) = dispatch3(elsif, (yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val), escape_Qundef((yyvsp[(5) - (5)].val)));

		    }
    break;

  case 359:

/* Line 1806 of yacc.c  */
#line 3160 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch1(else, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 362:

/* Line 1806 of yacc.c  */
#line 3174 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
#if 0
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyval.val));

		    }
    break;

  case 363:

/* Line 1806 of yacc.c  */
#line 3182 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(mlhs_paren, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 364:

/* Line 1806 of yacc.c  */
#line 3192 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = mlhs_add(mlhs_new(), (yyvsp[(1) - (1)].val));

		    }
    break;

  case 365:

/* Line 1806 of yacc.c  */
#line 3200 "ripper.y"
    {
#if 0
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = mlhs_add((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 366:

/* Line 1806 of yacc.c  */
#line 3210 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (1)].val), 0);
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    }
    break;

  case 367:

/* Line 1806 of yacc.c  */
#line 3218 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(4) - (4)].val), 0);
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (4)].val), (yyval.val));
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (4)].val), (yyval.val));

		    }
    break;

  case 368:

/* Line 1806 of yacc.c  */
#line 3227 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(4) - (6)].val), 0);
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (6)].val), NEW_POSTARG((yyval.val), (yyvsp[(6) - (6)].val)));
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (6)].val), (yyval.val));

		    }
    break;

  case 369:

/* Line 1806 of yacc.c  */
#line 3236 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (3)].val), -1);
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (3)].val), Qnil);

		    }
    break;

  case 370:

/* Line 1806 of yacc.c  */
#line 3244 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN((yyvsp[(1) - (5)].val), NEW_POSTARG(-1, (yyvsp[(5) - (5)].val)));
#endif
			(yyval.val) = mlhs_add_star((yyvsp[(1) - (5)].val), (yyvsp[(5) - (5)].val));

		    }
    break;

  case 371:

/* Line 1806 of yacc.c  */
#line 3252 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(2) - (2)].val), 0);
#if 0
			(yyval.val) = NEW_MASGN(0, (yyval.val));
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), (yyval.val));

		    }
    break;

  case 372:

/* Line 1806 of yacc.c  */
#line 3261 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(2) - (4)].val), 0);
#if 0
			(yyval.val) = NEW_MASGN(0, NEW_POSTARG((yyval.val), (yyvsp[(4) - (4)].val)));
#endif
		      #if 0
		      TODO: Check me
		      #endif
			(yyval.val) = mlhs_add_star((yyval.val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 373:

/* Line 1806 of yacc.c  */
#line 3273 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, -1);
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), Qnil);

		    }
    break;

  case 374:

/* Line 1806 of yacc.c  */
#line 3281 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_MASGN(0, NEW_POSTARG(-1, (yyvsp[(3) - (3)].val)));
#endif
			(yyval.val) = mlhs_add_star(mlhs_new(), Qnil);

		    }
    break;

  case 375:

/* Line 1806 of yacc.c  */
#line 3292 "ripper.y"
    {
			(yyval.val) = new_args_tail((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    }
    break;

  case 376:

/* Line 1806 of yacc.c  */
#line 3296 "ripper.y"
    {
			(yyval.val) = new_args_tail((yyvsp[(1) - (2)].val), Qnone, (yyvsp[(2) - (2)].val));
		    }
    break;

  case 377:

/* Line 1806 of yacc.c  */
#line 3300 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
		    }
    break;

  case 378:

/* Line 1806 of yacc.c  */
#line 3304 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, Qnone, (yyvsp[(1) - (1)].val));
		    }
    break;

  case 379:

/* Line 1806 of yacc.c  */
#line 3310 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    }
    break;

  case 380:

/* Line 1806 of yacc.c  */
#line 3314 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, Qnone, Qnone);
		    }
    break;

  case 381:

/* Line 1806 of yacc.c  */
#line 3320 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), Qnone, (yyvsp[(6) - (6)].val));
		    }
    break;

  case 382:

/* Line 1806 of yacc.c  */
#line 3324 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (8)].val), (yyvsp[(3) - (8)].val), (yyvsp[(5) - (8)].val), (yyvsp[(7) - (8)].val), (yyvsp[(8) - (8)].val));
		    }
    break;

  case 383:

/* Line 1806 of yacc.c  */
#line 3328 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), Qnone, Qnone, (yyvsp[(4) - (4)].val));
		    }
    break;

  case 384:

/* Line 1806 of yacc.c  */
#line 3332 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), Qnone, (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    }
    break;

  case 385:

/* Line 1806 of yacc.c  */
#line 3336 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (4)].val), Qnone, (yyvsp[(3) - (4)].val), Qnone, (yyvsp[(4) - (4)].val));
		    }
    break;

  case 386:

/* Line 1806 of yacc.c  */
#line 3340 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (2)].val), Qnone, 1, Qnone, new_args_tail(Qnone, Qnone, Qnone));
#if 0
#endif
                        dispatch1(excessed_comma, (yyval.val));

		    }
    break;

  case 387:

/* Line 1806 of yacc.c  */
#line 3348 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), Qnone, (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    }
    break;

  case 388:

/* Line 1806 of yacc.c  */
#line 3352 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (2)].val), Qnone, Qnone, Qnone, (yyvsp[(2) - (2)].val));
		    }
    break;

  case 389:

/* Line 1806 of yacc.c  */
#line 3356 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), Qnone, (yyvsp[(4) - (4)].val));
		    }
    break;

  case 390:

/* Line 1806 of yacc.c  */
#line 3360 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    }
    break;

  case 391:

/* Line 1806 of yacc.c  */
#line 3364 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (2)].val), Qnone, Qnone, (yyvsp[(2) - (2)].val));
		    }
    break;

  case 392:

/* Line 1806 of yacc.c  */
#line 3368 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (4)].val), Qnone, (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    }
    break;

  case 393:

/* Line 1806 of yacc.c  */
#line 3372 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, (yyvsp[(1) - (2)].val), Qnone, (yyvsp[(2) - (2)].val));
		    }
    break;

  case 394:

/* Line 1806 of yacc.c  */
#line 3376 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, (yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    }
    break;

  case 395:

/* Line 1806 of yacc.c  */
#line 3380 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, Qnone, Qnone, (yyvsp[(1) - (1)].val));
		    }
    break;

  case 397:

/* Line 1806 of yacc.c  */
#line 3387 "ripper.y"
    {
			command_start = TRUE;
		    }
    break;

  case 398:

/* Line 1806 of yacc.c  */
#line 3393 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = blockvar_new(params_new(Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil),
                                          escape_Qundef((yyvsp[(2) - (3)].val)));

		    }
    break;

  case 399:

/* Line 1806 of yacc.c  */
#line 3402 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = blockvar_new(params_new(Qnil,Qnil,Qnil,Qnil,Qnil,Qnil,Qnil),
                                          Qnil);

		    }
    break;

  case 400:

/* Line 1806 of yacc.c  */
#line 3411 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (4)].val);
#endif
			(yyval.val) = blockvar_new(escape_Qundef((yyvsp[(2) - (4)].val)), escape_Qundef((yyvsp[(3) - (4)].val)));

		    }
    break;

  case 401:

/* Line 1806 of yacc.c  */
#line 3422 "ripper.y"
    {
		      (yyval.val) = 0;
		    }
    break;

  case 402:

/* Line 1806 of yacc.c  */
#line 3426 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = (yyvsp[(3) - (4)].val);

		    }
    break;

  case 403:

/* Line 1806 of yacc.c  */
#line 3438 "ripper.y"
    {
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));
		    }
    break;

  case 404:

/* Line 1806 of yacc.c  */
#line 3445 "ripper.y"
    {
			rb_ary_push((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
		    }
    break;

  case 405:

/* Line 1806 of yacc.c  */
#line 3452 "ripper.y"
    {
			new_bv(get_id((yyvsp[(1) - (1)].val)));
#if 0
#endif
			(yyval.val) = get_value((yyvsp[(1) - (1)].val));

		    }
    break;

  case 406:

/* Line 1806 of yacc.c  */
#line 3460 "ripper.y"
    {
			(yyval.val) = 0;
		    }
    break;

  case 407:

/* Line 1806 of yacc.c  */
#line 3465 "ripper.y"
    {
			(yyval.vars) = dyna_push();
		    }
    break;

  case 408:

/* Line 1806 of yacc.c  */
#line 3468 "ripper.y"
    {
			(yyval.num) = lpar_beg;
			lpar_beg = ++paren_nest;
		    }
    break;

  case 409:

/* Line 1806 of yacc.c  */
#line 3473 "ripper.y"
    {
			(yyval.num) = ruby_sourceline;
		    }
    break;

  case 410:

/* Line 1806 of yacc.c  */
#line 3477 "ripper.y"
    {
			lpar_beg = (yyvsp[(2) - (5)].num);
#if 0
			(yyval.val) = NEW_LAMBDA((yyvsp[(3) - (5)].val), (yyvsp[(5) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(4) - (5)].num));
#endif
			(yyval.val) = dispatch2(lambda, (yyvsp[(3) - (5)].val), (yyvsp[(5) - (5)].val));

			dyna_pop((yyvsp[(1) - (5)].vars));
		    }
    break;

  case 411:

/* Line 1806 of yacc.c  */
#line 3490 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (4)].val);
#endif
			(yyval.val) = dispatch1(paren, (yyvsp[(2) - (4)].val));

		    }
    break;

  case 412:

/* Line 1806 of yacc.c  */
#line 3498 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    }
    break;

  case 413:

/* Line 1806 of yacc.c  */
#line 3508 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (3)].val);
		    }
    break;

  case 414:

/* Line 1806 of yacc.c  */
#line 3512 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (3)].val);
		    }
    break;

  case 415:

/* Line 1806 of yacc.c  */
#line 3518 "ripper.y"
    {
			(yyvsp[(1) - (1)].vars) = dyna_push();
#if 0
			(yyval.num) = ruby_sourceline;
#endif
		    }
    break;

  case 416:

/* Line 1806 of yacc.c  */
#line 3527 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ITER((yyvsp[(3) - (5)].val),(yyvsp[(4) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(2) - (5)].num));
#endif
			(yyval.val) = dispatch2(do_block, escape_Qundef((yyvsp[(3) - (5)].val)), (yyvsp[(4) - (5)].val));

			dyna_pop((yyvsp[(1) - (5)].vars));
		    }
    break;

  case 417:

/* Line 1806 of yacc.c  */
#line 3539 "ripper.y"
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

		    }
    break;

  case 418:

/* Line 1806 of yacc.c  */
#line 3555 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (4)].val), (yyvsp[(2) - (4)].val), (yyvsp[(3) - (4)].val));
			(yyval.val) = method_optarg((yyval.val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 419:

/* Line 1806 of yacc.c  */
#line 3564 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
			(yyvsp[(5) - (5)].val)->nd_iter = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = (yyvsp[(5) - (5)].val);
			fixpos((yyval.val), (yyvsp[(1) - (5)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (5)].val), (yyvsp[(2) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(5) - (5)].val));

		    }
    break;

  case 420:

/* Line 1806 of yacc.c  */
#line 3576 "ripper.y"
    {
#if 0
			block_dup_check((yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
			(yyvsp[(5) - (5)].val)->nd_iter = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = (yyvsp[(5) - (5)].val);
			fixpos((yyval.val), (yyvsp[(1) - (5)].val));
#endif
			(yyval.val) = dispatch4(command_call, (yyvsp[(1) - (5)].val), (yyvsp[(2) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(4) - (5)].val));
			(yyval.val) = method_add_block((yyval.val), (yyvsp[(5) - (5)].val));

		    }
    break;

  case 421:

/* Line 1806 of yacc.c  */
#line 3590 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (2)].val);
			(yyval.val)->nd_args = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = method_arg(dispatch1(fcall, (yyvsp[(1) - (2)].val)), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 422:

/* Line 1806 of yacc.c  */
#line 3599 "ripper.y"
    {
#if 0
			(yyval.num) = ruby_sourceline;
#endif
		    }
    break;

  case 423:

/* Line 1806 of yacc.c  */
#line 3605 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(5) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(4) - (5)].num));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val));
			(yyval.val) = method_optarg((yyval.val), (yyvsp[(5) - (5)].val));

		    }
    break;

  case 424:

/* Line 1806 of yacc.c  */
#line 3615 "ripper.y"
    {
#if 0
			(yyval.num) = ruby_sourceline;
#endif
		    }
    break;

  case 425:

/* Line 1806 of yacc.c  */
#line 3621 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (5)].val), (yyvsp[(3) - (5)].val), (yyvsp[(5) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(4) - (5)].num));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (5)].val), ripper_id2sym('.'), (yyvsp[(3) - (5)].val));
			(yyval.val) = method_optarg((yyval.val), (yyvsp[(5) - (5)].val));

		    }
    break;

  case 426:

/* Line 1806 of yacc.c  */
#line 3631 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val), 0);
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (3)].val), ripper_intern("::"), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 427:

/* Line 1806 of yacc.c  */
#line 3639 "ripper.y"
    {
#if 0
			(yyval.num) = ruby_sourceline;
#endif
		    }
    break;

  case 428:

/* Line 1806 of yacc.c  */
#line 3645 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), rb_intern("call"), (yyvsp[(4) - (4)].val));
			nd_set_line((yyval.val), (yyvsp[(3) - (4)].num));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (4)].val), ripper_id2sym('.'),
				       ripper_intern("call"));
			(yyval.val) = method_optarg((yyval.val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 429:

/* Line 1806 of yacc.c  */
#line 3656 "ripper.y"
    {
#if 0
			(yyval.num) = ruby_sourceline;
#endif
		    }
    break;

  case 430:

/* Line 1806 of yacc.c  */
#line 3662 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), rb_intern("call"), (yyvsp[(4) - (4)].val));
			nd_set_line((yyval.val), (yyvsp[(3) - (4)].num));
#endif
			(yyval.val) = dispatch3(call, (yyvsp[(1) - (4)].val), ripper_intern("::"),
				       ripper_intern("call"));
			(yyval.val) = method_optarg((yyval.val), (yyvsp[(4) - (4)].val));

		    }
    break;

  case 431:

/* Line 1806 of yacc.c  */
#line 3673 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_SUPER((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(super, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 432:

/* Line 1806 of yacc.c  */
#line 3681 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ZSUPER();
#endif
			(yyval.val) = dispatch0(zsuper);

		    }
    break;

  case 433:

/* Line 1806 of yacc.c  */
#line 3689 "ripper.y"
    {
#if 0
			if ((yyvsp[(1) - (4)].val) && nd_type((yyvsp[(1) - (4)].val)) == NODE_SELF)
			    (yyval.val) = NEW_FCALL(tAREF, (yyvsp[(3) - (4)].val));
			else
			    (yyval.val) = NEW_CALL((yyvsp[(1) - (4)].val), tAREF, (yyvsp[(3) - (4)].val));
			fixpos((yyval.val), (yyvsp[(1) - (4)].val));
#endif
			(yyval.val) = dispatch2(aref, (yyvsp[(1) - (4)].val), escape_Qundef((yyvsp[(3) - (4)].val)));

		    }
    break;

  case 434:

/* Line 1806 of yacc.c  */
#line 3703 "ripper.y"
    {
			(yyvsp[(1) - (1)].vars) = dyna_push();
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    }
    break;

  case 435:

/* Line 1806 of yacc.c  */
#line 3712 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ITER((yyvsp[(3) - (5)].val),(yyvsp[(4) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(2) - (5)].num));
#endif
			(yyval.val) = dispatch2(brace_block, escape_Qundef((yyvsp[(3) - (5)].val)), (yyvsp[(4) - (5)].val));

			dyna_pop((yyvsp[(1) - (5)].vars));
		    }
    break;

  case 436:

/* Line 1806 of yacc.c  */
#line 3722 "ripper.y"
    {
			(yyvsp[(1) - (1)].vars) = dyna_push();
#if 0
			(yyval.num) = ruby_sourceline;
#endif

		    }
    break;

  case 437:

/* Line 1806 of yacc.c  */
#line 3731 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ITER((yyvsp[(3) - (5)].val),(yyvsp[(4) - (5)].val));
			nd_set_line((yyval.val), (yyvsp[(2) - (5)].num));
#endif
			(yyval.val) = dispatch2(do_block, escape_Qundef((yyvsp[(3) - (5)].val)), (yyvsp[(4) - (5)].val));

			dyna_pop((yyvsp[(1) - (5)].vars));
		    }
    break;

  case 438:

/* Line 1806 of yacc.c  */
#line 3745 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_WHEN((yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val), (yyvsp[(5) - (5)].val));
#endif
			(yyval.val) = dispatch3(when, (yyvsp[(2) - (5)].val), (yyvsp[(4) - (5)].val), escape_Qundef((yyvsp[(5) - (5)].val)));

		    }
    break;

  case 441:

/* Line 1806 of yacc.c  */
#line 3761 "ripper.y"
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

		    }
    break;

  case 443:

/* Line 1806 of yacc.c  */
#line 3781 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIST((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 444:

/* Line 1806 of yacc.c  */
#line 3789 "ripper.y"
    {
#if 0
			if (!((yyval.val) = splat_array((yyvsp[(1) - (1)].val)))) (yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    }
    break;

  case 446:

/* Line 1806 of yacc.c  */
#line 3800 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    }
    break;

  case 448:

/* Line 1806 of yacc.c  */
#line 3807 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch1(ensure, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 451:

/* Line 1806 of yacc.c  */
#line 3819 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_LIT(ID2SYM((yyvsp[(1) - (1)].val)));
#endif
			(yyval.val) = dispatch1(symbol_literal, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 453:

/* Line 1806 of yacc.c  */
#line 3830 "ripper.y"
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

		    }
    break;

  case 456:

/* Line 1806 of yacc.c  */
#line 3849 "ripper.y"
    {
#if 0
			(yyval.val) = literal_concat((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(string_concat, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 457:

/* Line 1806 of yacc.c  */
#line 3859 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(string_literal, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 458:

/* Line 1806 of yacc.c  */
#line 3869 "ripper.y"
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

		    }
    break;

  case 459:

/* Line 1806 of yacc.c  */
#line 3896 "ripper.y"
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

		    }
    break;

  case 460:

/* Line 1806 of yacc.c  */
#line 3961 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ZARRAY();
#endif
			(yyval.val) = dispatch0(words_new);
			(yyval.val) = dispatch1(array, (yyval.val));

		    }
    break;

  case 461:

/* Line 1806 of yacc.c  */
#line 3970 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(array, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 462:

/* Line 1806 of yacc.c  */
#line 3980 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(words_new);

		    }
    break;

  case 463:

/* Line 1806 of yacc.c  */
#line 3988 "ripper.y"
    {
#if 0
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), evstr2dstr((yyvsp[(2) - (3)].val)));
#endif
			(yyval.val) = dispatch2(words_add, (yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));

		    }
    break;

  case 464:

/* Line 1806 of yacc.c  */
#line 4000 "ripper.y"
    {
			(yyval.val) = dispatch0(word_new);
			(yyval.val) = dispatch2(word_add, (yyval.val), (yyvsp[(1) - (1)].val));
		    }
    break;

  case 465:

/* Line 1806 of yacc.c  */
#line 4006 "ripper.y"
    {
#if 0
			(yyval.val) = literal_concat((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(word_add, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 466:

/* Line 1806 of yacc.c  */
#line 4016 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ZARRAY();
#endif
			(yyval.val) = dispatch0(symbols_new);
			(yyval.val) = dispatch1(array, (yyval.val));

		    }
    break;

  case 467:

/* Line 1806 of yacc.c  */
#line 4025 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(array, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 468:

/* Line 1806 of yacc.c  */
#line 4035 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(symbols_new);

		    }
    break;

  case 469:

/* Line 1806 of yacc.c  */
#line 4043 "ripper.y"
    {
#if 0
			(yyvsp[(2) - (3)].val) = evstr2dstr((yyvsp[(2) - (3)].val));
			if (nd_type((yyvsp[(2) - (3)].val)) == NODE_DSTR) {
			    nd_set_type((yyvsp[(2) - (3)].val), NODE_DSYM);
			}
			else {
			    nd_set_type((yyvsp[(2) - (3)].val), NODE_LIT);
			    (yyvsp[(2) - (3)].val)->nd_lit = rb_str_intern((yyvsp[(2) - (3)].val)->nd_lit);
			}
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = dispatch2(symbols_add, (yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));

		    }
    break;

  case 470:

/* Line 1806 of yacc.c  */
#line 4061 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ZARRAY();
#endif
			(yyval.val) = dispatch0(qwords_new);
			(yyval.val) = dispatch1(array, (yyval.val));

		    }
    break;

  case 471:

/* Line 1806 of yacc.c  */
#line 4070 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(array, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 472:

/* Line 1806 of yacc.c  */
#line 4080 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_ZARRAY();
#endif
			(yyval.val) = dispatch0(qsymbols_new);
			(yyval.val) = dispatch1(array, (yyval.val));

		    }
    break;

  case 473:

/* Line 1806 of yacc.c  */
#line 4089 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(array, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 474:

/* Line 1806 of yacc.c  */
#line 4099 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(qwords_new);

		    }
    break;

  case 475:

/* Line 1806 of yacc.c  */
#line 4107 "ripper.y"
    {
#if 0
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = dispatch2(qwords_add, (yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));

		    }
    break;

  case 476:

/* Line 1806 of yacc.c  */
#line 4117 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(qsymbols_new);

		    }
    break;

  case 477:

/* Line 1806 of yacc.c  */
#line 4125 "ripper.y"
    {
#if 0
			VALUE lit;
			lit = (yyvsp[(2) - (3)].val)->nd_lit;
			(yyvsp[(2) - (3)].val)->nd_lit = ID2SYM(rb_intern_str(lit));
			nd_set_type((yyvsp[(2) - (3)].val), NODE_LIT);
			(yyval.val) = list_append((yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = dispatch2(qsymbols_add, (yyvsp[(1) - (3)].val), (yyvsp[(2) - (3)].val));

		    }
    break;

  case 478:

/* Line 1806 of yacc.c  */
#line 4139 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(string_content);

		    }
    break;

  case 479:

/* Line 1806 of yacc.c  */
#line 4147 "ripper.y"
    {
#if 0
			(yyval.val) = literal_concat((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(string_add, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 480:

/* Line 1806 of yacc.c  */
#line 4157 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(xstring_new);

		    }
    break;

  case 481:

/* Line 1806 of yacc.c  */
#line 4165 "ripper.y"
    {
#if 0
			(yyval.val) = literal_concat((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(xstring_add, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 482:

/* Line 1806 of yacc.c  */
#line 4175 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch0(regexp_new);

		    }
    break;

  case 483:

/* Line 1806 of yacc.c  */
#line 4183 "ripper.y"
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

		    }
    break;

  case 485:

/* Line 1806 of yacc.c  */
#line 4213 "ripper.y"
    {
			(yyval.node) = lex_strterm;
			lex_strterm = 0;
			lex_state = EXPR_BEG;
		    }
    break;

  case 486:

/* Line 1806 of yacc.c  */
#line 4219 "ripper.y"
    {
#if 0
			lex_strterm = (yyvsp[(2) - (3)].node);
			(yyval.val) = NEW_EVSTR((yyvsp[(3) - (3)].val));
#endif
			lex_strterm = (yyvsp[(2) - (3)].node);
			(yyval.val) = dispatch1(string_dvar, (yyvsp[(3) - (3)].val));

		    }
    break;

  case 487:

/* Line 1806 of yacc.c  */
#line 4229 "ripper.y"
    {
			(yyvsp[(1) - (1)].val) = cond_stack;
			(yyval.val) = cmdarg_stack;
			cond_stack = 0;
			cmdarg_stack = 0;
		    }
    break;

  case 488:

/* Line 1806 of yacc.c  */
#line 4235 "ripper.y"
    {
			(yyval.node) = lex_strterm;
			lex_strterm = 0;
			lex_state = EXPR_BEG;
		    }
    break;

  case 489:

/* Line 1806 of yacc.c  */
#line 4240 "ripper.y"
    {
			(yyval.num) = brace_nest;
			brace_nest = 0;
		    }
    break;

  case 490:

/* Line 1806 of yacc.c  */
#line 4245 "ripper.y"
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

		    }
    break;

  case 491:

/* Line 1806 of yacc.c  */
#line 4260 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_GVAR((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = dispatch1(var_ref, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 492:

/* Line 1806 of yacc.c  */
#line 4268 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_IVAR((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = dispatch1(var_ref, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 493:

/* Line 1806 of yacc.c  */
#line 4276 "ripper.y"
    {
#if 0
			(yyval.val) = NEW_CVAR((yyvsp[(1) - (1)].val));
#endif
			(yyval.val) = dispatch1(var_ref, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 495:

/* Line 1806 of yacc.c  */
#line 4287 "ripper.y"
    {
			lex_state = EXPR_END;
#if 0
			(yyval.val) = (yyvsp[(2) - (2)].val);
#endif
			(yyval.val) = dispatch1(symbol, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 500:

/* Line 1806 of yacc.c  */
#line 4304 "ripper.y"
    {
			lex_state = EXPR_END;
#if 0
			(yyval.val) = dsym_node((yyvsp[(2) - (3)].val));
#endif
			(yyval.val) = dispatch1(dyna_symbol, (yyvsp[(2) - (3)].val));

		    }
    break;

  case 503:

/* Line 1806 of yacc.c  */
#line 4317 "ripper.y"
    {
#if 0
			(yyval.val) = negate_lit((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("-@"), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 504:

/* Line 1806 of yacc.c  */
#line 4325 "ripper.y"
    {
#if 0
			(yyval.val) = negate_lit((yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(unary, ripper_intern("-@"), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 510:

/* Line 1806 of yacc.c  */
#line 4341 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword_nil);}
    break;

  case 511:

/* Line 1806 of yacc.c  */
#line 4342 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword_self);}
    break;

  case 512:

/* Line 1806 of yacc.c  */
#line 4343 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword_true);}
    break;

  case 513:

/* Line 1806 of yacc.c  */
#line 4344 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword_false);}
    break;

  case 514:

/* Line 1806 of yacc.c  */
#line 4345 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword__FILE__);}
    break;

  case 515:

/* Line 1806 of yacc.c  */
#line 4346 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword__LINE__);}
    break;

  case 516:

/* Line 1806 of yacc.c  */
#line 4347 "ripper.y"
    {ifndef_ripper((yyval.val) = keyword__ENCODING__);}
    break;

  case 517:

/* Line 1806 of yacc.c  */
#line 4351 "ripper.y"
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

		    }
    break;

  case 518:

/* Line 1806 of yacc.c  */
#line 4364 "ripper.y"
    {
#if 0
			if (!((yyval.val) = gettable((yyvsp[(1) - (1)].val)))) (yyval.val) = NEW_BEGIN(0);
#endif
			(yyval.val) = dispatch1(var_ref, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 519:

/* Line 1806 of yacc.c  */
#line 4374 "ripper.y"
    {
			(yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
#if 0
#endif
			(yyval.val) = dispatch1(var_field, (yyval.val));

		    }
    break;

  case 520:

/* Line 1806 of yacc.c  */
#line 4382 "ripper.y"
    {
		        (yyval.val) = assignable((yyvsp[(1) - (1)].val), 0);
#if 0
#endif
			(yyval.val) = dispatch1(var_field, (yyval.val));

		    }
    break;

  case 523:

/* Line 1806 of yacc.c  */
#line 4396 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = Qnil;

		    }
    break;

  case 524:

/* Line 1806 of yacc.c  */
#line 4404 "ripper.y"
    {
			lex_state = EXPR_BEG;
			command_start = TRUE;
		    }
    break;

  case 525:

/* Line 1806 of yacc.c  */
#line 4409 "ripper.y"
    {
			(yyval.val) = (yyvsp[(3) - (4)].val);
		    }
    break;

  case 526:

/* Line 1806 of yacc.c  */
#line 4413 "ripper.y"
    {
#if 0
			yyerrok;
			(yyval.val) = 0;
#endif
			yyerrok;
			(yyval.val) = Qnil;

		    }
    break;

  case 527:

/* Line 1806 of yacc.c  */
#line 4425 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(2) - (3)].val);
#endif
			(yyval.val) = dispatch1(paren, (yyvsp[(2) - (3)].val));

			lex_state = EXPR_BEG;
			command_start = TRUE;
		    }
    break;

  case 528:

/* Line 1806 of yacc.c  */
#line 4435 "ripper.y"
    {
			(yyval.val) = (yyvsp[(1) - (2)].val);
			lex_state = EXPR_BEG;
			command_start = TRUE;
		    }
    break;

  case 529:

/* Line 1806 of yacc.c  */
#line 4443 "ripper.y"
    {
			(yyval.val) = new_args_tail((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    }
    break;

  case 530:

/* Line 1806 of yacc.c  */
#line 4447 "ripper.y"
    {
			(yyval.val) = new_args_tail((yyvsp[(1) - (2)].val), Qnone, (yyvsp[(2) - (2)].val));
		    }
    break;

  case 531:

/* Line 1806 of yacc.c  */
#line 4451 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
		    }
    break;

  case 532:

/* Line 1806 of yacc.c  */
#line 4455 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, Qnone, (yyvsp[(1) - (1)].val));
		    }
    break;

  case 533:

/* Line 1806 of yacc.c  */
#line 4461 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    }
    break;

  case 534:

/* Line 1806 of yacc.c  */
#line 4465 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, Qnone, Qnone);
		    }
    break;

  case 535:

/* Line 1806 of yacc.c  */
#line 4471 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), Qnone, (yyvsp[(6) - (6)].val));
		    }
    break;

  case 536:

/* Line 1806 of yacc.c  */
#line 4475 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (8)].val), (yyvsp[(3) - (8)].val), (yyvsp[(5) - (8)].val), (yyvsp[(7) - (8)].val), (yyvsp[(8) - (8)].val));
		    }
    break;

  case 537:

/* Line 1806 of yacc.c  */
#line 4479 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), Qnone, Qnone, (yyvsp[(4) - (4)].val));
		    }
    break;

  case 538:

/* Line 1806 of yacc.c  */
#line 4483 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), Qnone, (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    }
    break;

  case 539:

/* Line 1806 of yacc.c  */
#line 4487 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (4)].val), Qnone, (yyvsp[(3) - (4)].val), Qnone, (yyvsp[(4) - (4)].val));
		    }
    break;

  case 540:

/* Line 1806 of yacc.c  */
#line 4491 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (6)].val), Qnone, (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    }
    break;

  case 541:

/* Line 1806 of yacc.c  */
#line 4495 "ripper.y"
    {
			(yyval.val) = new_args((yyvsp[(1) - (2)].val), Qnone, Qnone, Qnone, (yyvsp[(2) - (2)].val));
		    }
    break;

  case 542:

/* Line 1806 of yacc.c  */
#line 4499 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), Qnone, (yyvsp[(4) - (4)].val));
		    }
    break;

  case 543:

/* Line 1806 of yacc.c  */
#line 4503 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (6)].val), (yyvsp[(3) - (6)].val), (yyvsp[(5) - (6)].val), (yyvsp[(6) - (6)].val));
		    }
    break;

  case 544:

/* Line 1806 of yacc.c  */
#line 4507 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (2)].val), Qnone, Qnone, (yyvsp[(2) - (2)].val));
		    }
    break;

  case 545:

/* Line 1806 of yacc.c  */
#line 4511 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, (yyvsp[(1) - (4)].val), Qnone, (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    }
    break;

  case 546:

/* Line 1806 of yacc.c  */
#line 4515 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, (yyvsp[(1) - (2)].val), Qnone, (yyvsp[(2) - (2)].val));
		    }
    break;

  case 547:

/* Line 1806 of yacc.c  */
#line 4519 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, (yyvsp[(1) - (4)].val), (yyvsp[(3) - (4)].val), (yyvsp[(4) - (4)].val));
		    }
    break;

  case 548:

/* Line 1806 of yacc.c  */
#line 4523 "ripper.y"
    {
			(yyval.val) = new_args(Qnone, Qnone, Qnone, Qnone, (yyvsp[(1) - (1)].val));
		    }
    break;

  case 549:

/* Line 1806 of yacc.c  */
#line 4527 "ripper.y"
    {
			(yyval.val) = new_args_tail(Qnone, Qnone, Qnone);
			(yyval.val) = new_args(Qnone, Qnone, Qnone, Qnone, (yyval.val));
		    }
    break;

  case 550:

/* Line 1806 of yacc.c  */
#line 4534 "ripper.y"
    {
#if 0
			yyerror("formal argument cannot be a constant");
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch1(param_error, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 551:

/* Line 1806 of yacc.c  */
#line 4543 "ripper.y"
    {
#if 0
			yyerror("formal argument cannot be an instance variable");
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch1(param_error, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 552:

/* Line 1806 of yacc.c  */
#line 4552 "ripper.y"
    {
#if 0
			yyerror("formal argument cannot be a global variable");
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch1(param_error, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 553:

/* Line 1806 of yacc.c  */
#line 4561 "ripper.y"
    {
#if 0
			yyerror("formal argument cannot be a class variable");
			(yyval.val) = 0;
#endif
			(yyval.val) = dispatch1(param_error, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 555:

/* Line 1806 of yacc.c  */
#line 4573 "ripper.y"
    {
			formal_argument(get_id((yyvsp[(1) - (1)].val)));
			(yyval.val) = (yyvsp[(1) - (1)].val);
		    }
    break;

  case 556:

/* Line 1806 of yacc.c  */
#line 4580 "ripper.y"
    {
			arg_var(get_id((yyvsp[(1) - (1)].val)));
#if 0
			(yyval.val) = NEW_ARGS_AUX((yyvsp[(1) - (1)].val), 1);
#endif
			(yyval.val) = get_value((yyvsp[(1) - (1)].val));

		    }
    break;

  case 557:

/* Line 1806 of yacc.c  */
#line 4589 "ripper.y"
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

		    }
    break;

  case 558:

/* Line 1806 of yacc.c  */
#line 4610 "ripper.y"
    {
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));
		    }
    break;

  case 559:

/* Line 1806 of yacc.c  */
#line 4615 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (3)].val);
			(yyval.val)->nd_plen++;
			(yyval.val)->nd_next = block_append((yyval.val)->nd_next, (yyvsp[(3) - (3)].val)->nd_next);
			rb_gc_force_recycle((VALUE)(yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = rb_ary_push((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 560:

/* Line 1806 of yacc.c  */
#line 4628 "ripper.y"
    {
			arg_var(formal_argument(get_id((yyvsp[(1) - (2)].val))));
			(yyval.val) = assignable((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#if 0
			(yyval.val) = NEW_KW_ARG(0, (yyval.val));
#endif
			(yyval.val) = rb_assoc_new((yyval.val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 561:

/* Line 1806 of yacc.c  */
#line 4640 "ripper.y"
    {
			arg_var(formal_argument(get_id((yyvsp[(1) - (2)].val))));
			(yyval.val) = assignable((yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));
#if 0
			(yyval.val) = NEW_KW_ARG(0, (yyval.val));
#endif
			(yyval.val) = rb_assoc_new((yyval.val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 562:

/* Line 1806 of yacc.c  */
#line 4652 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 563:

/* Line 1806 of yacc.c  */
#line 4660 "ripper.y"
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

		    }
    break;

  case 564:

/* Line 1806 of yacc.c  */
#line 4677 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 565:

/* Line 1806 of yacc.c  */
#line 4685 "ripper.y"
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

		    }
    break;

  case 568:

/* Line 1806 of yacc.c  */
#line 4705 "ripper.y"
    {
			shadowing_lvar(get_id((yyvsp[(2) - (2)].val)));
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    }
    break;

  case 569:

/* Line 1806 of yacc.c  */
#line 4710 "ripper.y"
    {
			(yyval.val) = internal_id();
		    }
    break;

  case 570:

/* Line 1806 of yacc.c  */
#line 4716 "ripper.y"
    {
			arg_var(formal_argument(get_id((yyvsp[(1) - (3)].val))));
			(yyval.val) = assignable((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#if 0
			(yyval.val) = NEW_OPT_ARG(0, (yyval.val));
#endif
			(yyval.val) = rb_assoc_new((yyval.val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 571:

/* Line 1806 of yacc.c  */
#line 4728 "ripper.y"
    {
			arg_var(formal_argument(get_id((yyvsp[(1) - (3)].val))));
			(yyval.val) = assignable((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#if 0
			(yyval.val) = NEW_OPT_ARG(0, (yyval.val));
#endif
			(yyval.val) = rb_assoc_new((yyval.val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 572:

/* Line 1806 of yacc.c  */
#line 4740 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 573:

/* Line 1806 of yacc.c  */
#line 4748 "ripper.y"
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

		    }
    break;

  case 574:

/* Line 1806 of yacc.c  */
#line 4764 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (1)].val);
#endif
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));

		    }
    break;

  case 575:

/* Line 1806 of yacc.c  */
#line 4772 "ripper.y"
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

		    }
    break;

  case 578:

/* Line 1806 of yacc.c  */
#line 4792 "ripper.y"
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

		    }
    break;

  case 579:

/* Line 1806 of yacc.c  */
#line 4805 "ripper.y"
    {
#if 0
			(yyval.val) = internal_id();
			arg_var((yyval.val));
#endif
			(yyval.val) = dispatch1(rest_param, Qnil);

		    }
    break;

  case 582:

/* Line 1806 of yacc.c  */
#line 4820 "ripper.y"
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

		    }
    break;

  case 583:

/* Line 1806 of yacc.c  */
#line 4837 "ripper.y"
    {
			(yyval.val) = (yyvsp[(2) - (2)].val);
		    }
    break;

  case 584:

/* Line 1806 of yacc.c  */
#line 4841 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = Qundef;

		    }
    break;

  case 585:

/* Line 1806 of yacc.c  */
#line 4851 "ripper.y"
    {
#if 0
			value_expr((yyvsp[(1) - (1)].val));
			(yyval.val) = (yyvsp[(1) - (1)].val);
		        if (!(yyval.val)) (yyval.val) = NEW_NIL();
#endif
			(yyval.val) = (yyvsp[(1) - (1)].val);

		    }
    break;

  case 586:

/* Line 1806 of yacc.c  */
#line 4860 "ripper.y"
    {lex_state = EXPR_BEG;}
    break;

  case 587:

/* Line 1806 of yacc.c  */
#line 4861 "ripper.y"
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

		    }
    break;

  case 589:

/* Line 1806 of yacc.c  */
#line 4891 "ripper.y"
    {
#if 0
			(yyval.val) = (yyvsp[(1) - (2)].val);
#endif
			(yyval.val) = dispatch1(assoclist_from_args, (yyvsp[(1) - (2)].val));

		    }
    break;

  case 590:

/* Line 1806 of yacc.c  */
#line 4903 "ripper.y"
    {
			(yyval.val) = rb_ary_new3(1, (yyvsp[(1) - (1)].val));
		    }
    break;

  case 591:

/* Line 1806 of yacc.c  */
#line 4908 "ripper.y"
    {
#if 0
			(yyval.val) = list_concat((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = rb_ary_push((yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 592:

/* Line 1806 of yacc.c  */
#line 4918 "ripper.y"
    {
#if 0
			(yyval.val) = list_append(NEW_LIST((yyvsp[(1) - (3)].val)), (yyvsp[(3) - (3)].val));
#endif
			(yyval.val) = dispatch2(assoc_new, (yyvsp[(1) - (3)].val), (yyvsp[(3) - (3)].val));

		    }
    break;

  case 593:

/* Line 1806 of yacc.c  */
#line 4926 "ripper.y"
    {
#if 0
			(yyval.val) = list_append(NEW_LIST(NEW_LIT(ID2SYM((yyvsp[(1) - (2)].val)))), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch2(assoc_new, (yyvsp[(1) - (2)].val), (yyvsp[(2) - (2)].val));

		    }
    break;

  case 594:

/* Line 1806 of yacc.c  */
#line 4934 "ripper.y"
    {
#if 0
			(yyval.val) = list_append(NEW_LIST(0), (yyvsp[(2) - (2)].val));
#endif
			(yyval.val) = dispatch1(assoc_splat, (yyvsp[(2) - (2)].val));

		    }
    break;

  case 605:

/* Line 1806 of yacc.c  */
#line 4964 "ripper.y"
    { (yyval.val) = (yyvsp[(1) - (1)].val); }
    break;

  case 606:

/* Line 1806 of yacc.c  */
#line 4969 "ripper.y"
    { (yyval.val) = (yyvsp[(1) - (1)].val); }
    break;

  case 616:

/* Line 1806 of yacc.c  */
#line 4992 "ripper.y"
    {yyerrok;}
    break;

  case 619:

/* Line 1806 of yacc.c  */
#line 4997 "ripper.y"
    {yyerrok;}
    break;

  case 620:

/* Line 1806 of yacc.c  */
#line 5001 "ripper.y"
    {
#if 0
			(yyval.val) = 0;
#endif
			(yyval.val) = Qundef;

		    }
    break;



/* Line 1806 of yacc.c  */
#line 11293 "parse.c"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
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
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      parser_yyerror (parser, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        parser_yyerror (parser, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
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

  /* Else will try to reuse lookahead token after shifting the error
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
      if (!yypact_value_is_default (yyn))
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

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  parser_yyerror (parser, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, parser);
    }
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



/* Line 2067 of yacc.c  */
#line 5009 "ripper.y"

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
	rb_compile_error_with_enc(NULL, 0, (void *)current_enc, "%s%s%s", pre, buf, post);

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
debug_lines(VALUE fname)
{
    ID script_lines;
    CONST_ID(script_lines, "SCRIPT_LINES__");
    if (rb_const_defined_at(rb_cObject, script_lines)) {
	VALUE hash = rb_const_get_at(rb_cObject, script_lines);
	if (RB_TYPE_P(hash, T_HASH)) {
	    VALUE lines = rb_ary_new();
	    rb_hash_aset(hash, fname, lines);
	    return lines;
	}
    }
    return 0;
}

static VALUE
coverage(VALUE fname, int n)
{
    VALUE coverages = rb_get_coverages();
    if (RTEST(coverages) && RBASIC(coverages)->klass == 0) {
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
	ruby_debug_lines = debug_lines(ruby_sourcefile_string);
	if (ruby_debug_lines && ruby_sourceline > 0) {
	    VALUE str = STR_NEW0();
	    n = ruby_sourceline;
	    do {
		rb_ary_push(ruby_debug_lines, str);
	    } while (--n);
	}

	if (!e_option_supplied(parser)) {
	    ruby_coverage = coverage(ruby_sourcefile_string, ruby_sourceline);
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
yycompile(struct parser_params *parser, VALUE fname, int line)
{
    ruby_sourcefile_string = rb_str_new_frozen(fname);
    ruby_sourcefile = RSTRING_PTR(fname);
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
parser_compile_string(volatile VALUE vparser, VALUE fname, VALUE s, int line)
{
    struct parser_params *parser;
    NODE *node;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, parser);
    lex_gets = lex_get_str;
    lex_gets_ptr = 0;
    lex_input = s;
    lex_pbeg = lex_p = lex_pend = 0;
    compile_for_eval = rb_parse_in_eval();

    node = yycompile(parser, fname, line);
    RB_GC_GUARD(vparser); /* prohibit tail call optimization */

    return node;
}

NODE*
rb_compile_string(const char *f, VALUE s, int line)
{
    must_be_ascii_compatible(s);
    return parser_compile_string(rb_parser_new(), rb_filesystem_str_new_cstr(f), s, line);
}

NODE*
rb_parser_compile_string(volatile VALUE vparser, const char *f, VALUE s, int line)
{
    return rb_parser_compile_string_path(vparser, rb_filesystem_str_new_cstr(f), s, line);
}

NODE*
rb_parser_compile_string_path(volatile VALUE vparser, VALUE f, VALUE s, int line)
{
    must_be_ascii_compatible(s);
    return parser_compile_string(vparser, f, s, line);
}

NODE*
rb_compile_cstr(const char *f, const char *s, int len, int line)
{
    VALUE str = rb_str_new(s, len);
    return parser_compile_string(rb_parser_new(), rb_filesystem_str_new_cstr(f), str, line);
}

NODE*
rb_parser_compile_cstr(volatile VALUE vparser, const char *f, const char *s, int len, int line)
{
    VALUE str = rb_str_new(s, len);
    return parser_compile_string(vparser, rb_filesystem_str_new_cstr(f), str, line);
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
    return rb_parser_compile_file_path(vparser, rb_filesystem_str_new_cstr(f), file, start);
}

NODE*
rb_parser_compile_file_path(volatile VALUE vparser, VALUE fname, VALUE file, int start)
{
    struct parser_params *parser;
    NODE *node;

    TypedData_Get_Struct(vparser, struct parser_params, &parser_data_type, parser);
    lex_gets = lex_io_gets;
    lex_input = file;
    lex_pbeg = lex_p = lex_pend = 0;
    compile_for_eval = rb_parse_in_eval();

    node = yycompile(parser, fname, start);
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
		if (str) {
		    dispose_string(str);
		    str = 0;
		}
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
	    compile_error(PARSER_ARG  "Invalid char `\\x%02X' in expression", c);
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
	return NEW_STR(rb_str_dup(ruby_sourcefile_string));
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

static const char id_type_names[][9] = {
    "LOCAL",
    "INSTANCE",
    "",				/* INSTANCE2 */
    "GLOBAL",
    "ATTRSET",
    "CONST",
    "CLASS",
    "JUNK",
};

ID
rb_id_attrset(ID id)
{
    if (!is_notop_id(id)) {
	switch (id) {
	  case tAREF: case tASET:
	    return tASET;	/* only exception */
	}
	rb_name_error(id, "cannot make operator ID :%s attrset", rb_id2name(id));
    }
    else {
	int scope = (int)(id & ID_SCOPE_MASK);
	switch (scope) {
	  case ID_LOCAL: case ID_INSTANCE: case ID_GLOBAL:
	  case ID_CONST: case ID_CLASS: case ID_JUNK:
	    break;
	  case ID_ATTRSET:
	    return id;
	  default:
	    rb_name_error(id, "cannot make %s ID %+"PRIsVALUE" attrset",
			  id_type_names[scope], ID2SYM(id));

	}
    }
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
    local->cmdargs = cmdarg_stack;
    cmdarg_stack = 0;
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
    cmdarg_stack = lvtbl->cmdargs;
    xfree(lvtbl);
    lvtbl = local;
}

#ifndef RIPPER
static ID*
local_tbl_gen(struct parser_params *parser)
{
    int cnt_args = vtable_size(lvtbl->args);
    int cnt_vars = vtable_size(lvtbl->vars);
    int cnt = cnt_args + cnt_vars;
    int i, j;
    ID *buf;

    if (cnt <= 0) return 0;
    buf = ALLOC_N(ID, cnt + 1);
    MEMCPY(buf+1, lvtbl->args->tbl, ID, cnt_args);
    /* remove IDs duplicated to warn shadowing */
    for (i = 0, j = cnt_args+1; i < cnt_vars; ++i) {
	ID id = lvtbl->vars->tbl[i];
	if (!vtable_included(lvtbl->args, id)) {
	    buf[j++] = id;
	}
    }
    if (--j < cnt) REALLOC_N(buf, ID, (cnt = j) + 1);
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

#define IDSET_ATTRSET_FOR_SYNTAX ((1U<<ID_LOCAL)|(1U<<ID_CONST))
#define IDSET_ATTRSET_FOR_INTERN (~(~0U<<(1<<ID_SCOPE_SHIFT)) & ~(1U<<ID_ATTRSET))

static int
rb_enc_symname_type(const char *name, long len, rb_encoding *enc, unsigned int allowed_atttset)
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
	if (m >= e) break;
	switch (*m) {
	  case '!': case '?':
	    if (type == ID_GLOBAL || type == ID_CLASS || type == ID_INSTANCE) return -1;
	    type = ID_JUNK;
	    ++m;
	    if (m + 1 < e || *m != '=') break;
	    /* fall through */
	  case '=':
	    if (!(allowed_atttset & (1U << type))) return -1;
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
    return rb_enc_symname_type(name, len, enc, IDSET_ATTRSET_FOR_SYNTAX) != -1;
}

static int
rb_str_symname_type(VALUE name, unsigned int allowed_atttset)
{
    const char *ptr = StringValuePtr(name);
    long len = RSTRING_LEN(name);
    int type = rb_enc_symname_type(ptr, len, rb_enc_get(name), allowed_atttset);
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

    if (!len || (rb_cString && !rb_enc_asciicompat(enc))) {
      junk:
	id = ID_JUNK;
	goto new_id;
    }
    last = len-1;
    id = 0;
    switch (*m) {
      case '$':
	if (len < 2) goto junk;
	id |= ID_GLOBAL;
	if ((mb = is_special_global_name(++m, e, enc)) != 0) {
	    if (!--mb) symenc = rb_usascii_encoding();
	    goto new_id;
	}
	break;
      case '@':
	if (m[1] == '@') {
	    if (len < 3) goto junk;
	    m++;
	    id |= ID_CLASS;
	}
	else {
	    if (len < 2) goto junk;
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
	break;
    }
    if (name[last] == '=') {
	/* attribute assignment */
	if (last > 1 && name[last-1] == '=')
	    goto junk;
	id = rb_intern3(name, last, enc);
	if (id > tLAST_OP_ID && !is_attrset_id(id)) {
	    enc = rb_enc_get(rb_id2str(id));
	    id = rb_id_attrset(id);
	    goto id_register;
	}
	id = ID_ATTRSET;
    }
    else if (id == 0) {
	if (rb_enc_isupper(m[0], enc)) {
	    id = ID_CONST;
	}
	else {
	    id = ID_LOCAL;
	}
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
    if (id != ID_ATTRSET && m - name < len) id = ID_JUNK;
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
	ID id_stem = (id & ~ID_SCOPE_MASK);
	VALUE str;

	do {
	    if (!!(str = rb_id2str(id_stem | ID_LOCAL))) break;
	    if (!!(str = rb_id2str(id_stem | ID_CONST))) break;
	    if (!!(str = rb_id2str(id_stem | ID_INSTANCE))) break;
	    if (!!(str = rb_id2str(id_stem | ID_GLOBAL))) break;
	    if (!!(str = rb_id2str(id_stem | ID_CLASS))) break;
	    if (!!(str = rb_id2str(id_stem | ID_JUNK))) break;
	    return 0;
	} while (0);
	str = rb_str_dup(str);
	rb_str_cat(str, "=", 1);
	register_symid_str(id, str);
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
    return rb_str_symname_type(name, 0) == ID_CONST;
}

int
rb_is_class_name(VALUE name)
{
    return rb_str_symname_type(name, 0) == ID_CLASS;
}

int
rb_is_global_name(VALUE name)
{
    return rb_str_symname_type(name, 0) == ID_GLOBAL;
}

int
rb_is_instance_name(VALUE name)
{
    return rb_str_symname_type(name, 0) == ID_INSTANCE;
}

int
rb_is_attrset_name(VALUE name)
{
    return rb_str_symname_type(name, IDSET_ATTRSET_FOR_INTERN) == ID_ATTRSET;
}

int
rb_is_local_name(VALUE name)
{
    return rb_str_symname_type(name, 0) == ID_LOCAL;
}

int
rb_is_method_name(VALUE name)
{
    switch (rb_str_symname_type(name, 0)) {
      case ID_LOCAL: case ID_ATTRSET: case ID_JUNK:
	return TRUE;
    }
    return FALSE;
}

int
rb_is_junk_name(VALUE name)
{
    return rb_str_symname_type(name, IDSET_ATTRSET_FOR_SYNTAX) == -1;
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
    parser->parser_ruby_sourcefile_string = Qnil;
#ifndef RIPPER
    parser->is_ripper = 0;
    parser->parser_eval_tree_begin = 0;
    parser->parser_eval_tree = 0;
#else
    parser->is_ripper = 1;
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
    rb_gc_mark(p->parser_ruby_sourcefile_string);
#ifndef RIPPER
    rb_gc_mark((VALUE)p->parser_eval_tree_begin) ;
    rb_gc_mark((VALUE)p->parser_eval_tree) ;
    rb_gc_mark(p->debug_lines);
#else
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

