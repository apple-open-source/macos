/*
 * Internal definitions for states.
 * Copyright (c) 1997-1998 Markku Rossi.
 *
 * Author: Markku Rossi <mtr@iki.fi>
 */

/*
 * This file is part of GNU enscript.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef DEFS_H
#define DEFS_H

/*
 * Config stuffs.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>

#ifndef ___P
#if PROTOTYPES
#define ___P(protos) protos
#else /* no PROTOTYPES */
#define ___P(protos) ()
#endif /* no PROTOTYPES */
#endif

#if STDC_HEADERS

#include <stdlib.h>
#include <string.h>

#else /* no STDC_HEADERS */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#ifndef HAVE_STRCHR
#define strchr index
#define strrchr rindex
#endif
char *strchr ();
char *strrchr ();

#ifndef HAVE_STRERROR
extern char *strerror ___P ((int));
#endif

#ifndef HAVE_MEMMOVE
extern void *memmove ___P ((void *, void *, size_t));
#endif

#ifndef HAVE_MEMCPY
extern void *memcpy ___P ((void *, void *, size_t));
#endif

#endif /* no STDC_HEADERS */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#else
#define _(String) String
#endif

#if HAVE_LC_MESSAGES
#include <locale.h>
#endif

#include "regex.h"
#include "xalloc.h"
#include "strhash.h"

/*
 * Types and definitions.
 */

#define RULE_BEGIN	((void *) 0)
#define RULE_END	((void *) 1)

#define INBUFSIZE	(20 * 1024)

#define IS_TRUE(n) ((n)->type != nINTEGER || (n)->u.integer != 0)

#define REGEXP(regexp) \
  ((regexp)->u.re.compiled.fastmap_accurate			\
   ? (&(regexp)->u.re.compiled)					\
   : (compile_regexp (regexp), &(regexp)->u.re.compiled))

/* Flags for regular expressions. */
#define fRE_CASE_INSENSITIVE	1

/* Generic linked list. */

struct list_item_st
{
  struct list_item_st *next;
  void *data;
};

typedef struct list_item_st ListItem;

struct list_st
{
  ListItem *head;
  ListItem *tail;
};

typedef struct list_st List;

/* State. */

struct state_st
{
  char *name;
  char *super_name;
  struct state_st *super;
  List *rules;
};

typedef struct state_st State;


/* Node. */

typedef enum
{
  nVOID,
  nSTRING,
  nREGEXP,
  nINTEGER,
  nREAL,
  nSYMBOL,
  nARRAY
} NodeType;

struct node_st
{
  NodeType type;
  unsigned int refcount;
  unsigned int linenum;
  char *filename;

  union
  {
    struct
    {
      char *data;
      unsigned int len;
    } str;
    struct
    {
      char *data;
      unsigned int len;
      unsigned int flags;
      regex_t compiled;
      struct re_registers matches;
    } re;
    int integer;
    double real;
    char *sym;
    struct
    {
      struct node_st **array;
      unsigned int len;
      unsigned int allocated;
    } array;
  } u;
};

typedef struct node_st Node;

/* Cons cell. */
struct cons_st
{
  void *car;
  void *cdr;
};

typedef struct cons_st Cons;

/* Grammar types. */

typedef enum
{
  eSTRING,
  eREGEXP,
  eINTEGER,
  eREAL,
  eSYMBOL,
  eNOT,
  eAND,
  eOR,
  eFCALL,
  eASSIGN,
  eADDASSIGN,
  eSUBASSIGN,
  eMULASSIGN,
  eDIVASSIGN,
  ePOSTFIXADD,
  ePOSTFIXSUB,
  ePREFIXADD,
  ePREFIXSUB,
  eARRAYASSIGN,
  eARRAYREF,
  eQUESTCOLON,
  eMULT,
  eDIV,
  ePLUS,
  eMINUS,
  eLT,
  eGT,
  eEQ,
  eNE,
  eGE,
  eLE
} ExprType;

struct expr_st
{
  ExprType type;
  unsigned int linenum;
  char *filename;

  union
  {
    Node *node;
    struct expr_st *not;
    struct
    {
      Node *name;
      List *args;
    } fcall;
    struct
    {
      Node *sym;
      struct expr_st *expr;
    } assign;
    struct
    {
      struct expr_st *expr1;
      struct expr_st *expr2;
      struct expr_st *expr3;
    } arrayassign;
    struct
    {
      struct expr_st *expr1;
      struct expr_st *expr2;
    } arrayref;
    struct
    {
      struct expr_st *cond;
      struct expr_st *expr1;
      struct expr_st *expr2;
    } questcolon;
    struct
    {
      struct expr_st *left;
      struct expr_st *right;
    } op;
  } u;
};

typedef struct expr_st Expr;

typedef enum
{
  sRETURN,
  sDEFSUB,
  sBLOCK,
  sIF,
  sEXPR,
  sWHILE,
  sFOR
} StmtType;

struct stmt_st
{
  StmtType type;
  unsigned int linenum;
  char *filename;

  union
  {
    Expr *expr;
    struct
    {
      Node *name;
      Cons *closure;
    } defsub;
    struct
    {
      Expr *expr;
      struct stmt_st *then_stmt;
      struct stmt_st *else_stmt;
    } stmt_if;
    struct
    {
      Expr *expr;
      struct stmt_st *body;
    } stmt_while;
    struct
    {
      Expr *init;
      Expr *cond;
      Expr *incr;
      struct stmt_st *body;
    } stmt_for;
    List *block;
  } u;
};

typedef struct stmt_st Stmt;

struct environment_st
{
  struct environment_st *next;
  char *name;
  Node *val;
};

typedef struct environment_st Environment;

/* Primitive procedure. */
typedef Node *(*Primitive) ___P ((char *prim_name, List *args,
				  Environment *env, char *filename,
				  unsigned int linenum));

/* Variable definition chain. */
struct variable_definition_st
{
  struct variable_definition_st *next;
  char *sym;
  char *val;
};

typedef struct variable_definition_st VariableDef;

/* Grammar and execution warning levels. */
typedef enum
{
  WARN_LIGHT = 10,
  WARN_ALL = 100
} WarningLevel;


/*
 * Global variables.
 */

extern char *program;

extern FILE *yyin;
extern FILE *ofp;
extern char *defs_file;
extern unsigned int linenum;
extern char *yyin_name;
extern WarningLevel warning_level;
extern char *path;
extern unsigned int verbose;

/* Namespaces. */
extern StringHashPtr ns_prims;
extern StringHashPtr ns_vars;
extern StringHashPtr ns_subs;
extern StringHashPtr ns_states;

extern List *global_stmts;
extern List *start_stmts;
extern List *startrules;
extern List *namerules;

/* Void node value.  There is only nvoid instance. */
extern Node *nvoid;

extern FILE *ifp;
extern char *inbuf;
extern unsigned int data_in_buffer;
extern unsigned int bufpos;
extern int eof_seen;
extern char *current_fname;
extern unsigned int current_linenum;

extern struct re_registers *current_match;
extern char *current_match_buf;

/* Options. */

extern char *start_state_arg;
extern char *start_state;


/*
 * Prototypes for global functions.
 */

void init_primitives ();

/* Parser & lexer. */
int yyparse ();
int yylex ();
void yyerror ___P ((char *msg));

/* Generic linked list. */

/* Create a new linked list. */
List *list ();

/* Add a new element <data> to the beginning of list <list>. */
void list_prepend ___P ((List *list, void *data));

/* Add a new element <data> to the end of list <list>. */
void list_append ___P ((List *list, void *data));


/* Node manipulators. */

Node *node_alloc ___P ((NodeType type));

Node *node_copy ___P ((Node *node));

void node_reference ___P ((Node *node));

void node_free ___P ((Node *node));

void enter_system_variable ___P ((char *name, char *value));

void compile_regexp ___P ((Node *regexp));


/* Grammar constructors. */

Stmt *mk_stmt ___P ((StmtType type, void *arg1, void *arg2, void *arg3,
		     void *arg4));

Expr *mk_expr ___P ((ExprType type, void *arg1, void *arg2, void *arg3));

Cons *cons ___P ((void *car, void *cdr));

void define_state ___P ((Node *sym, Node *super, List *rules));

/* Execution. */

Node *eval_expr ___P ((Expr *expr, Environment *env));

Node *eval_statement ___P ((Stmt *stmt, Environment *env, int *return_seen));

Node *eval_statement_list ___P ((List *lst, Environment *env,
				 int *return_seen));

void process_file ___P ((char *fname));

Node *execute_state ___P ((char *name));

void load_states_file ___P ((char *name));

/*
 * Lookup state <name> and return its handle.  If the state is
 * undefined, the function tries to autoload it.
 */
State *lookup_state ___P ((char *name));

#endif /* not DEFS_H */
