/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_AST_H
#define __SIEVE_AST_H

#include "lib.h"
#include "str.h"

#include "sieve-common.h"
#include "sieve-error.h"

/*
	Abstract Syntax Tree (AST) structure:
	
	sieve_ast (root)
	[*command]
	 |
	 +-- command:
	 |   ....
	 +-- command:
	 |	 [identifier *argument                      *test *command]
	 |                +-- argument:                 |     \--> as from root
	 |                |   ....                      |
 	 |                +-- argument:                 V (continued below)
	 |                |   [number | tag | *string]
	 |                .
	 .
	
	 *test
	 +-- test:
	 |   ....
	 +-- test:
	 |   [identifier *argument                     *test]
	 |               +-- argument:                 \-->  as from the top 
	 .               |   ....                              of this tree
	                 +-- argument:
	                 |   [number | tag | *string]
	                 .
	                 
	 Tests and commands are defined using the same structure: sieve_ast_node. 
	 However, arguments and string-lists are described using sieve_ast_argument.  
*/

/* IMPORTANT NOTICE: Do not decorate the AST with objects other than those 
 * allocated on the ast's pool or static const objects. Otherwise it is possible 
 * that pointers in the tree become dangling which is highly undesirable.
 */

/*
 * Forward declarations
 */ 

struct sieve_ast_list;
struct sieve_ast_arg_list;

/*
 * Types
 */
 
enum sieve_ast_argument_type {
	SAAT_NONE,
	SAAT_NUMBER,
	SAAT_STRING,
	SAAT_STRING_LIST,
	SAAT_TAG,
};

enum sieve_ast_type {
	SAT_NONE,
	SAT_ROOT,
	SAT_COMMAND,
	SAT_TEST,
};

/*
 * AST Nodes
 */
 
/* Argument node */

struct sieve_ast_argument {
	enum sieve_ast_argument_type type;

	/* Back reference to the AST object */
	struct sieve_ast *ast;

	/* List related */
	struct sieve_ast_arg_list *list;
	struct sieve_ast_argument *next;
	struct sieve_ast_argument *prev;
  
	/* Parser-assigned data */
  
	union {	
		string_t *str;
		struct sieve_ast_arg_list *strlist;
		const char *tag;
		unsigned int number;
	} _value;
  
	unsigned int source_line;
  
	/* Assigned during validation */

	/* Argument associated with this ast element  */
	struct sieve_argument *argument;

	/* Parameters to this (tag) argument */
	struct sieve_ast_argument *parameters;
};

struct sieve_ast_node {
	enum sieve_ast_type type;

	/* Back reference to the AST object */
	struct sieve_ast *ast;
	
	/* Back reference to this node's parent */
	struct sieve_ast_node *parent;
	
	/* Linked list references */
	struct sieve_ast_list *list;
	struct sieve_ast_node *next;
	struct sieve_ast_node *prev;
	
	/* Commands (NULL if not allocated) */
	bool block;
	struct sieve_ast_list *commands;
	
	/* Tests (NULL if not allocated)*/
	bool test_list;
	struct sieve_ast_list *tests;

	/* Arguments (NULL if not allocated) */
	struct sieve_ast_arg_list *arguments;	

	/* Identifier of command or test */
	const char *identifier;		

	/* The location in the file where this command was started */
	unsigned int source_line;
		
	/* Assigned during validation */
		
	/* Context */
	struct sieve_command *command;	
};

/*
 * AST node lists
 */
 
struct sieve_ast_list {
	struct sieve_ast_node *head;		
	struct sieve_ast_node *tail;
	unsigned int len; 	
};

struct sieve_ast_arg_list {
	struct sieve_ast_argument *head;		
	struct sieve_ast_argument *tail;
	unsigned int len; 	
};

/*
 * AST object 
 */
 
struct sieve_ast; 
 
struct sieve_ast *sieve_ast_create(struct sieve_script *script);
void sieve_ast_ref(struct sieve_ast *ast);
void sieve_ast_unref(struct sieve_ast **ast);

struct sieve_ast_node *sieve_ast_root(struct sieve_ast *ast);
pool_t sieve_ast_pool(struct sieve_ast *ast);
struct sieve_script *sieve_ast_script(struct sieve_ast *ast);

/* Extension support */

struct sieve_ast_extension {
	const struct sieve_extension_def *ext;	

	void (*free)(const struct sieve_extension *ext, struct sieve_ast *ast, 
		void *context);
};

void sieve_ast_extension_link
	(struct sieve_ast *ast, const struct sieve_extension *ext);
const struct sieve_extension * const *sieve_ast_extensions_get
	(struct sieve_ast *ast, unsigned int *count_r);

void sieve_ast_extension_register
	(struct sieve_ast *ast, const struct sieve_extension *ext,
		const struct sieve_ast_extension *ast_ext, void *context);
void *sieve_ast_extension_get_context
	(struct sieve_ast *ast, const struct sieve_extension *ext);

/* 
 * AST node manipulation
 */
 
/* Command nodes */

struct sieve_ast_node *sieve_ast_test_create
	(struct sieve_ast_node *parent, const char *identifier, 
		unsigned int source_line);
struct sieve_ast_node *sieve_ast_command_create
	(struct sieve_ast_node *parent, const char *identifier, 
		unsigned int source_line);

struct sieve_ast_node *sieve_ast_node_detach
	(struct sieve_ast_node *first);

const char *sieve_ast_type_name(enum sieve_ast_type ast_type);
	
/* Argument nodes */

struct sieve_ast_argument *sieve_ast_argument_create
	(struct sieve_ast *ast, unsigned int source_line);

struct sieve_ast_arg_list *sieve_ast_arg_list_create(pool_t pool);	
bool sieve_ast_arg_list_add
	(struct sieve_ast_arg_list *list, struct sieve_ast_argument *argument);
bool sieve_ast_arg_list_insert
	(struct sieve_ast_arg_list *list, struct sieve_ast_argument *before,
		struct sieve_ast_argument *argument);
void sieve_ast_arg_list_substitute
	(struct sieve_ast_arg_list *list, struct sieve_ast_argument *argument, 
		struct sieve_ast_argument *replacement);

struct sieve_ast_argument *sieve_ast_argument_string_create_raw
	(struct sieve_ast *ast, string_t *str, unsigned int source_line);
struct sieve_ast_argument *sieve_ast_argument_string_create
	(struct sieve_ast_node *node, const string_t *str, unsigned int source_line);
struct sieve_ast_argument *sieve_ast_argument_cstring_create
	(struct sieve_ast_node *node, const char *str, unsigned int source_line);
	
struct sieve_ast_argument *sieve_ast_argument_tag_create
	(struct sieve_ast_node *node, const char *tag, unsigned int source_line);

struct sieve_ast_argument *sieve_ast_argument_number_create
	(struct sieve_ast_node *node, unsigned int number, unsigned int source_line);

void sieve_ast_argument_string_set
	(struct sieve_ast_argument *argument, string_t *newstr);
void sieve_ast_argument_string_setc
	(struct sieve_ast_argument *argument, const char *newstr);

void sieve_ast_argument_number_set
	(struct sieve_ast_argument *argument, unsigned int newnum);
void sieve_ast_argument_number_substitute
	(struct sieve_ast_argument *argument, unsigned int number);

struct sieve_ast_argument *sieve_ast_argument_tag_insert
(struct sieve_ast_argument *before, const char *tag, unsigned int source_line); 

struct sieve_ast_argument *sieve_ast_argument_stringlist_create
	(struct sieve_ast_node *node, unsigned int source_line);
struct sieve_ast_argument *sieve_ast_argument_stringlist_substitute
	(struct sieve_ast_node *node, struct sieve_ast_argument *arg);

struct sieve_ast_argument *sieve_ast_arguments_detach
	(struct sieve_ast_argument *first, unsigned int count);
bool sieve_ast_argument_attach
	(struct sieve_ast_node *node, struct sieve_ast_argument *argument);
	
const char *sieve_ast_argument_type_name(enum sieve_ast_argument_type arg_type);
#define sieve_ast_argument_name(argument) \
	sieve_ast_argument_type_name((argument)->type)

bool sieve_ast_stringlist_add
	(struct sieve_ast_argument *list, const string_t *str, 
		unsigned int source_line);
bool sieve_ast_stringlist_add_strc
	(struct sieve_ast_argument *list, const char *str, 
		unsigned int source_line);
		
/* 
 * Utility
 */

int sieve_ast_stringlist_map
	(struct sieve_ast_argument **listitem, void *context,
		int (*map_function)(void *context, struct sieve_ast_argument *arg));
struct sieve_ast_argument *sieve_ast_stringlist_join
	(struct sieve_ast_argument *list, struct sieve_ast_argument *items);
	
/* 
 * AST access macros 
 */

/* Generic list access macros */
#define __AST_LIST_FIRST(list) \
	((list) == NULL ? NULL : (list)->head)
#define __AST_LIST_LAST(list) \
	((list) == NULL ? NULL : (list)->tail)
#define __AST_LIST_COUNT(list) \
	((list) == NULL || (list)->head == NULL ? 0 : (list)->len)
#define __AST_LIST_NEXT(item) ((item)->next)
#define __AST_LIST_PREV(item) ((item)->prev)

#define __AST_NODE_LIST_FIRST(node, list) __AST_LIST_FIRST((node)->list)
#define __AST_NODE_LIST_LAST(node, list) __AST_LIST_LAST((node)->list)
#define __AST_NODE_LIST_COUNT(node, list) __AST_LIST_COUNT((node)->list)

/* AST macros */

/* AST node macros */
#define sieve_ast_node_pool(node) (sieve_ast_pool((node)->ast))
#define sieve_ast_node_parent(node) ((node)->parent)
#define sieve_ast_node_prev(node) __AST_LIST_PREV(node)
#define sieve_ast_node_next(node) __AST_LIST_NEXT(node)
#define sieve_ast_node_type(node) ((node) == NULL ? SAT_NONE : (node)->type)
#define sieve_ast_node_line(node) ((node) == NULL ? 0 : (node)->source_line)

/* AST command node macros */
#define sieve_ast_command_first(node) __AST_NODE_LIST_FIRST(node, commands)
#define sieve_ast_command_count(node) __AST_NODE_LIST_COUNT(node, commands)
#define sieve_ast_command_prev(command) __AST_LIST_PREV(command)
#define sieve_ast_command_next(command) __AST_LIST_NEXT(command)

/* Compare the identifier of the previous command */
#define sieve_ast_prev_cmd_is(cmd, id) \
	( (cmd)->prev == NULL ? FALSE : \
		strncasecmp((cmd)->prev->identifier, id, sizeof(id)-1) == 0 )
	
/* AST test macros */
#define sieve_ast_test_count(node) __AST_NODE_LIST_COUNT(node, tests)
#define sieve_ast_test_first(node) __AST_NODE_LIST_FIRST(node, tests)
#define sieve_ast_test_next(test) __AST_LIST_NEXT(test)

/* AST argument macros */
#define sieve_ast_argument_pool(node) (sieve_ast_pool((node)->ast))
#define sieve_ast_argument_first(node) __AST_NODE_LIST_FIRST(node, arguments)
#define sieve_ast_argument_last(node) __AST_NODE_LIST_LAST(node, arguments)
#define sieve_ast_argument_count(node) __AST_NODE_LIST_COUNT(node, arguments)
#define sieve_ast_argument_prev(argument) __AST_LIST_PREV(argument)
#define sieve_ast_argument_next(argument) __AST_LIST_NEXT(argument)
#define sieve_ast_argument_type(argument) \
	((argument) == NULL ? SAAT_NONE : (argument)->type)
#define sieve_ast_argument_line(argument) \
	((argument) == NULL ? 0 : (argument)->source_line)

#define sieve_ast_argument_str(argument) ((argument)->_value.str)
#define sieve_ast_argument_strc(argument) (str_c((argument)->_value.str))
#define sieve_ast_argument_tag(argument) ((argument)->_value.tag)
#define sieve_ast_argument_number(argument) ((argument)->_value.number)

/* AST string list macros */
// @UNSAFE: should check whether we are actually accessing a string list
#define sieve_ast_strlist_first(list) \
	__AST_NODE_LIST_FIRST(list, _value.strlist)
#define sieve_ast_strlist_last(list) \
	__AST_NODE_LIST_LAST(list, _value.strlist)
#define sieve_ast_strlist_count(list) \
	__AST_NODE_LIST_COUNT(list, _value.strlist)
#define sieve_ast_strlist_next(str) __AST_LIST_NEXT(str)
#define sieve_ast_strlist_prev(str) __AST_LIST_PREV(str)
#define sieve_ast_strlist_str(str) sieve_ast_argument_str(str)
#define sieve_ast_strlist_strc(str) sieve_ast_argument_strc(str)

/* 
 * Debug 
 */

void sieve_ast_unparse(struct sieve_ast *ast);

#endif /* __SIEVE_AST_H */
