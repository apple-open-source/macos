/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#include "lib.h"
#include "str.h"
#include "mempool.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-script.h"
#include "sieve-extensions.h"

#include "sieve-ast.h"

#include <stdio.h>

/* 
 * Forward declarations 
 */

static struct sieve_ast_node *sieve_ast_node_create
	(struct sieve_ast *ast, struct sieve_ast_node *parent, 
		enum sieve_ast_type type, unsigned int source_line);

/*
 * Types
 */

/* Extensions to the AST */

struct sieve_ast_extension_reg {
	const struct sieve_extension *ext;
	const struct sieve_ast_extension *ast_ext;
	void *context;
};

/* 
 * AST object 
 */

struct sieve_ast {
	pool_t pool;
	int refcount;

	struct sieve_instance *svinst;
		
	struct sieve_script *script;
		
	struct sieve_ast_node *root;
	
	ARRAY_DEFINE(linked_extensions, const struct sieve_extension *);
	ARRAY_DEFINE(extensions, struct sieve_ast_extension_reg);
};

struct sieve_ast *sieve_ast_create
(struct sieve_script *script) 
{
	pool_t pool;
	struct sieve_ast *ast;
	unsigned int ext_count;
	
	pool = pool_alloconly_create("sieve_ast", 16384);	
	ast = p_new(pool, struct sieve_ast, 1);
	ast->pool = pool;
	ast->refcount = 1;
	
	ast->script = script;
	sieve_script_ref(script);
	ast->svinst = sieve_script_svinst(script);
		
	ast->root = sieve_ast_node_create(ast, NULL, SAT_ROOT, 0);
	ast->root->identifier = "ROOT";
	
	ext_count = sieve_extensions_get_count(ast->svinst);
	p_array_init(&ast->linked_extensions, pool, ext_count);
	p_array_init(&ast->extensions, pool, ext_count);
	
	return ast;
}

void sieve_ast_ref(struct sieve_ast *ast) 
{
	ast->refcount++;
}

void sieve_ast_unref(struct sieve_ast **ast) 
{
	unsigned int i, ext_count;
	const struct sieve_ast_extension_reg *extrs;
	
	i_assert((*ast)->refcount > 0);

	if (--(*ast)->refcount != 0)
		return;
	
	/* Release script reference */
	sieve_script_unref(&(*ast)->script);
	
	/* Signal registered extensions that the AST is being destroyed */
	extrs = array_get(&(*ast)->extensions, &ext_count);
	for ( i = 0; i < ext_count; i++ ) {
		if ( extrs[i].ast_ext != NULL && 
			extrs[i].ast_ext->free != NULL )
			extrs[i].ast_ext->free(extrs[i].ext, *ast, extrs[i].context);
	}

	/* Destroy AST */
	pool_unref(&(*ast)->pool);
	
	*ast = NULL;
}

struct sieve_ast_node *sieve_ast_root(struct sieve_ast *ast)
{
	return ast->root;
}

pool_t sieve_ast_pool(struct sieve_ast *ast)
{
	return ast->pool;
}

struct sieve_script *sieve_ast_script(struct sieve_ast *ast)
{
	return ast->script;
}

/* 
 * Extension support 
 */

bool sieve_ast_extension_link
(struct sieve_ast *ast, const struct sieve_extension *ext)
{
	unsigned int i, ext_count;
	const struct sieve_extension *const *extensions;
	
	if ( ext->id < 0 ) return TRUE;
	 
	/* Prevent duplicates */
	extensions = array_get(&ast->linked_extensions, &ext_count);
	for ( i = 0; i < ext_count; i++ ) {
		if ( extensions[i] == ext )
			return FALSE;
	}

	/* Add extension */
	array_append(&ast->linked_extensions, &ext, 1);	
	return TRUE;
}

const struct sieve_extension * const *sieve_ast_extensions_get
(struct sieve_ast *ast, unsigned int *count_r)
{
	return array_get(&ast->linked_extensions, count_r);
}

void sieve_ast_extension_register
(struct sieve_ast *ast, const struct sieve_extension *ext,
	const struct sieve_ast_extension *ast_ext, void *context)
{
	struct sieve_ast_extension_reg *reg;

	if ( ext->id < 0 ) return;

	/* Initialize registration */
	reg = array_idx_modifiable(&ast->extensions, (unsigned int) ext->id);
	reg->ast_ext = ast_ext;
	reg->ext = ext;
	reg->context = context;	
}

void sieve_ast_extension_set_context
(struct sieve_ast *ast, const struct sieve_extension *ext, void *context)
{
	struct sieve_ast_extension_reg *reg;

	if ( ext->id < 0 ) return;
	
	reg = array_idx_modifiable(&ast->extensions, (unsigned int) ext->id);
	reg->context = context;
}

void *sieve_ast_extension_get_context
(struct sieve_ast *ast, const struct sieve_extension *ext) 
{
	const struct sieve_ast_extension_reg *reg;

	if  ( ext->id < 0 || ext->id >= (int) array_count(&ast->extensions) )
		return NULL;
	
	reg = array_idx(&ast->extensions, (unsigned int) ext->id);		

	return reg->context;
}

/*
 * AST list implementations
 */
 
/* Very simplistic linked list implementation
 * FIXME: Move to separate file
 */
#define __LIST_CREATE(pool, type) { \
		type *list = p_new(pool, type, 1); \
		list->head = NULL; \
		list->tail = NULL; \
		list->len = 0;		\
		return list; \
	}

#define __LIST_ADD(list, node) { \
		if ( list->len + 1 < list->len ) \
			return FALSE; \
		\
		node->next = NULL; \
		if ( list->head == NULL ) { \
			node->prev = NULL; \
			list->head = node; \
			list->tail = node; \
		} else { \
			list->tail->next = node; \
			node->prev = list->tail; \
			list->tail = node; \
		} \
		list->len++; \
		node->list = list; \
		return TRUE; \
	}	 
	
#define __LIST_INSERT(list, before, node) { \
		if ( list->len + 1 < list->len ) \
			return FALSE; \
		\
		node->next = before; \
		if ( list->head == before ) { \
			node->prev = NULL; \
			list->head = node; \
		} else { \
			before->prev->next = node; \
		} \
		node->prev = before->prev; \
		before->prev = node; \
		list->len++; \
		node->list = list; \
		\
		return TRUE; \
	}

#define __LIST_JOIN(list, node_type, items) { \
		node_type *node; \
		\
		if ( list->len + items->len < list->len ) \
			return FALSE; \
		\
		if ( items->len == 0 ) return TRUE; \
		\
		if ( list->head == NULL ) { \
			list->head = items->head; \
			list->tail = items->tail; \
		} else { \
			list->tail->next = items->head; \
			items->head->prev = list->tail; \
			list->tail = items->tail; \
		} \
		list->len += items->len; \
		\
		node = items->head; \
		while ( node != NULL ) { \
			node->list = list; \
			node = node->next; \
		} \
		return TRUE; \
	}	 

#define __LIST_DETACH(first, node_type, count) { \
		node_type *last, *result; \
		unsigned int left; \
		\
		i_assert(first->list != NULL); \
		\
		left = count - 1; \
		last = first; \
		while ( left > 0 && last->next != NULL ) { \
			left--; \
			last = last->next; \
		} \
		\
		if ( first->list->head == first ) \
			first->list->head = last->next; \
		if ( first->list->tail == last ) \
			first->list->tail = first->prev; \
		\
		if ( first->prev != NULL ) \
			first->prev->next = last->next;	\
		if ( last->next != NULL ) \
			last->next->prev = first->prev; \
		\
		first->list->len -= count - left; \
		\
		result = last->next; \
		first->prev = NULL; \
		last->next = NULL; \
		\
		return result; \
	}

/* List of AST nodes */

static struct sieve_ast_list *sieve_ast_list_create(pool_t pool) 
	__LIST_CREATE(pool, struct sieve_ast_list)

static bool sieve_ast_list_add
(struct sieve_ast_list *list, struct sieve_ast_node *node) 
	__LIST_ADD(list, node)

static struct sieve_ast_node *sieve_ast_list_detach
(struct sieve_ast_node *first, unsigned int count) 
	__LIST_DETACH(first, struct sieve_ast_node, count)

/* List of argument AST nodes */

struct sieve_ast_arg_list *sieve_ast_arg_list_create(pool_t pool) 
	__LIST_CREATE(pool, struct sieve_ast_arg_list)
	
bool sieve_ast_arg_list_add
(struct sieve_ast_arg_list *list, struct sieve_ast_argument *argument)
	__LIST_ADD(list, argument)

bool sieve_ast_arg_list_insert
(struct sieve_ast_arg_list *list, struct sieve_ast_argument *before,
	struct sieve_ast_argument *argument)
	__LIST_INSERT(list, before, argument)

static bool sieve_ast_arg_list_join
(struct sieve_ast_arg_list *list, struct sieve_ast_arg_list *items)
	__LIST_JOIN(list, struct sieve_ast_argument, items)

static struct sieve_ast_argument *sieve_ast_arg_list_detach
(struct sieve_ast_argument *first, const unsigned int count)
	__LIST_DETACH(first, struct sieve_ast_argument, count)

void sieve_ast_arg_list_substitute
(struct sieve_ast_arg_list *list, struct sieve_ast_argument *argument, 
	struct sieve_ast_argument *replacement)
{
	if ( list->head == argument )
		list->head = replacement;
	if ( list->tail == argument )
		list->tail = replacement;
		
	if ( argument->prev != NULL )
		argument->prev->next = replacement;
	if ( argument->next != NULL )
		argument->next->prev = replacement;
	
	replacement->prev = argument->prev;
	replacement->next = argument->next;
	replacement->list = argument->list;
	
	argument->next = NULL;
	argument->prev = NULL;
}
	
/* 
 * AST node 
 */

static struct sieve_ast_node *sieve_ast_node_create
(struct sieve_ast *ast, struct sieve_ast_node *parent, enum sieve_ast_type type, 
	unsigned int source_line) 
{
	struct sieve_ast_node *node = p_new(ast->pool, struct sieve_ast_node, 1);
	
	node->ast = ast;
	node->parent = parent;
	node->type = type;
	
	node->prev = NULL;
	node->next = NULL;
	
	node->arguments = NULL;
	node->tests = NULL;
	node->commands = NULL;		
	
	node->test_list = FALSE;
	node->block = FALSE;
	
	node->source_line = source_line;
	
	return node;
}

static bool sieve_ast_node_add_command
(struct sieve_ast_node *node, struct sieve_ast_node *command) 
{
	i_assert( command->type == SAT_COMMAND && 
		(node->type == SAT_ROOT || node->type == SAT_COMMAND) );
	
	if (node->commands == NULL) 
		node->commands = sieve_ast_list_create(node->ast->pool);
	
	return sieve_ast_list_add(node->commands, command);
}

static bool sieve_ast_node_add_test
(struct sieve_ast_node *node, struct sieve_ast_node *test) 
{
	i_assert( test->type == SAT_TEST && 
		(node->type == SAT_TEST || node->type == SAT_COMMAND) );
	
	if (node->tests == NULL) 
		node->tests = sieve_ast_list_create(node->ast->pool);
	
	return sieve_ast_list_add(node->tests, test);
}

static bool sieve_ast_node_add_argument
(struct sieve_ast_node *node, struct sieve_ast_argument *argument) 
{
	i_assert( node->type == SAT_TEST || node->type == SAT_COMMAND );
	
	if (node->arguments == NULL) 
		node->arguments = sieve_ast_arg_list_create(node->ast->pool);
	
	return sieve_ast_arg_list_add(node->arguments, argument);
}

struct sieve_ast_node *sieve_ast_node_detach
(struct sieve_ast_node *first) 
{	
	return sieve_ast_list_detach(first, 1);
}

const char *sieve_ast_type_name
(enum sieve_ast_type ast_type) 
{
	switch ( ast_type ) {
	
	case SAT_NONE: return "none";
	case SAT_ROOT: return "ast root node";
	case SAT_COMMAND: return "command";
	case SAT_TEST: return "test";
	
	default: return "??AST NODE??";
	}
}

/* 
 * Argument AST node 
 */

struct sieve_ast_argument *sieve_ast_argument_create
(struct sieve_ast *ast, unsigned int source_line) 
{	
	struct sieve_ast_argument *arg = 
		p_new(ast->pool, struct sieve_ast_argument, 1);
	
	arg->ast = ast;
	
	arg->prev = NULL;
	arg->next = NULL;
	
	arg->source_line = source_line;
	
	arg->argument = NULL;
			
	return arg;
}

static void sieve_ast_argument_substitute
(struct sieve_ast_argument *argument, struct sieve_ast_argument *replacement) 
{
	sieve_ast_arg_list_substitute(argument->list, argument, replacement);
}

struct sieve_ast_argument *sieve_ast_argument_string_create_raw
(struct sieve_ast *ast, string_t *str, unsigned int source_line) 
{
	struct sieve_ast_argument *argument = sieve_ast_argument_create
		(ast, source_line);
		
	argument->type = SAAT_STRING;
	argument->_value.str = str;

	return argument;
}

struct sieve_ast_argument *sieve_ast_argument_string_create
(struct sieve_ast_node *node, const string_t *str, unsigned int source_line) 
{	
	struct sieve_ast_argument *argument;
	string_t *newstr;
	
	/* Allocate new internal string buffer */
	newstr = str_new(node->ast->pool, str_len(str));
	
	/* Clone string */
	str_append_str(newstr, str);
	 
	/* Create string argument */
	argument = sieve_ast_argument_string_create_raw
		(node->ast, newstr, source_line);

	/* Add argument to command/test node */
	sieve_ast_node_add_argument(node, argument);

	return argument;
}

struct sieve_ast_argument *sieve_ast_argument_cstring_create
(struct sieve_ast_node *node, const char *str, unsigned int source_line) 
{	
	struct sieve_ast_argument *argument;
	string_t *newstr;
	
	/* Allocate new internal string buffer */
	newstr = str_new(node->ast->pool, strlen(str));
	
	/* Clone string */
	str_append(newstr, str);
	 
	/* Create string argument */
	argument = sieve_ast_argument_string_create_raw
		(node->ast, newstr, source_line);

	/* Add argument to command/test node */
	sieve_ast_node_add_argument(node, argument);

	return argument;
}

void sieve_ast_argument_string_set
(struct sieve_ast_argument *argument, string_t *newstr)
{
	i_assert( argument->type == SAAT_STRING);
	argument->_value.str = newstr;
}

void sieve_ast_argument_string_setc
(struct sieve_ast_argument *argument, const char *newstr)
{
	i_assert( argument->type == SAAT_STRING);
	
	str_truncate(argument->_value.str, 0);
	str_append(argument->_value.str, newstr);
}

void sieve_ast_argument_number_substitute
(struct sieve_ast_argument *argument, unsigned int number)
{
	argument->type = SAAT_NUMBER;
	argument->_value.number = number;
}

struct sieve_ast_argument *sieve_ast_argument_stringlist_create
(struct sieve_ast_node *node, unsigned int source_line) 
{
	struct sieve_ast_argument *argument = 
		sieve_ast_argument_create(node->ast, source_line);
	
	argument->type = SAAT_STRING_LIST;
	argument->_value.strlist = NULL;
	
	sieve_ast_node_add_argument(node, argument);

	return argument;
}

struct sieve_ast_argument *sieve_ast_argument_stringlist_substitute
(struct sieve_ast_node *node, struct sieve_ast_argument *arg) 
{
	struct sieve_ast_argument *argument = 
		sieve_ast_argument_create(node->ast, arg->source_line);
	
	argument->type = SAAT_STRING_LIST;
	argument->_value.strlist = NULL;
	
	sieve_ast_argument_substitute(arg, argument);

	return argument;
}

static inline bool _sieve_ast_stringlist_add_item
(struct sieve_ast_argument *list, struct sieve_ast_argument *item) 
{
	i_assert( list->type == SAAT_STRING_LIST );
	
	if ( list->_value.strlist == NULL ) 
		list->_value.strlist = sieve_ast_arg_list_create(list->ast->pool);
	
	return sieve_ast_arg_list_add(list->_value.strlist, item);
}

static bool sieve_ast_stringlist_add_stringlist
(struct sieve_ast_argument *list, struct sieve_ast_argument *items) 
{
	i_assert( list->type == SAAT_STRING_LIST );
	i_assert( items->type == SAAT_STRING_LIST );

	if ( list->_value.strlist == NULL ) 
		list->_value.strlist = sieve_ast_arg_list_create(list->ast->pool);
	
	return sieve_ast_arg_list_join(list->_value.strlist, items->_value.strlist);
}

static bool _sieve_ast_stringlist_add_str
(struct sieve_ast_argument *list, string_t *str, unsigned int source_line) 
{
	struct sieve_ast_argument *stritem;
	
	stritem = sieve_ast_argument_create(list->ast, source_line);		
	stritem->type = SAAT_STRING;
	stritem->_value.str = str;

	return _sieve_ast_stringlist_add_item(list, stritem);
}

bool sieve_ast_stringlist_add
(struct sieve_ast_argument *list, const string_t *str, unsigned int source_line) 
{
	string_t *copied_str = str_new(list->ast->pool, str_len(str));
	str_append_str(copied_str, str);

	return _sieve_ast_stringlist_add_str(list, copied_str, source_line);
}

bool sieve_ast_stringlist_add_strc
(struct sieve_ast_argument *list, const char *str, unsigned int source_line) 
{
	string_t *copied_str = str_new(list->ast->pool, strlen(str));
	str_append(copied_str, str);
	
	return _sieve_ast_stringlist_add_str(list, copied_str, source_line);
}

struct sieve_ast_argument *sieve_ast_argument_tag_create
(struct sieve_ast_node *node, const char *tag, unsigned int source_line) 
{	
	struct sieve_ast_argument *argument = 
		sieve_ast_argument_create(node->ast, source_line);
	
	argument->type = SAAT_TAG;
	argument->_value.tag = p_strdup(node->ast->pool, tag);

	if ( !sieve_ast_node_add_argument(node, argument) )
		return NULL;

	return argument;
}

struct sieve_ast_argument *sieve_ast_argument_tag_insert
(struct sieve_ast_argument *before, const char *tag, unsigned int source_line) 
{	
	struct sieve_ast_argument *argument = 
		sieve_ast_argument_create(before->ast, source_line);
	
	argument->type = SAAT_TAG;
	argument->_value.tag = p_strdup(before->ast->pool, tag);

	if ( !sieve_ast_arg_list_insert(before->list, before, argument) )
		return NULL;
	
	return argument;
}

struct sieve_ast_argument *sieve_ast_argument_number_create
(struct sieve_ast_node *node, unsigned int number, unsigned int source_line) 
{
	
	struct sieve_ast_argument *argument = 
		sieve_ast_argument_create(node->ast, source_line);
		
	argument->type = SAAT_NUMBER;
	argument->_value.number = number;
	
	if ( !sieve_ast_node_add_argument(node, argument) )
		return NULL;
	
	return argument;
}

void sieve_ast_argument_number_set
(struct sieve_ast_argument *argument, unsigned int newnum)
{
	i_assert( argument->type == SAAT_NUMBER );
	argument->_value.number = newnum;
}


struct sieve_ast_argument *sieve_ast_arguments_detach
(struct sieve_ast_argument *first, unsigned int count) 
{	
	return sieve_ast_arg_list_detach(first, count);
}

bool sieve_ast_argument_attach
(struct sieve_ast_node *node, struct sieve_ast_argument *argument)
{
	return sieve_ast_node_add_argument(node, argument);
}

const char *sieve_ast_argument_type_name
(enum sieve_ast_argument_type arg_type) 
{
	switch ( arg_type ) {
	
	case SAAT_NONE: return "none";
	case SAAT_STRING_LIST: return "a string list";
	case SAAT_STRING: return "a string";
	case SAAT_NUMBER: return "a number";
	case SAAT_TAG: return "a tag";
	
	default: return "??ARGUMENT??";
	}
}

/* Test AST node */

struct sieve_ast_node *sieve_ast_test_create
(struct sieve_ast_node *parent, const char *identifier, 
	unsigned int source_line) 
{	
	struct sieve_ast_node *test = sieve_ast_node_create
		(parent->ast, parent, SAT_TEST, source_line);
		
	test->identifier = p_strdup(parent->ast->pool, identifier);
	
	if ( !sieve_ast_node_add_test(parent, test) )
		return NULL;
	
	return test;
}

/* Command AST node */

struct sieve_ast_node *sieve_ast_command_create
(struct sieve_ast_node *parent, const char *identifier, 
	unsigned int source_line) 
{

	struct sieve_ast_node *command = sieve_ast_node_create
		(parent->ast, parent, SAT_COMMAND, source_line);
	
	command->identifier = p_strdup(parent->ast->pool, identifier);
	
	if ( !sieve_ast_node_add_command(parent, command) )
		return NULL;
	
	return command;
}

/*
 * Utility
 */

int sieve_ast_stringlist_map
(struct sieve_ast_argument **listitem, void *context,
	int (*map_function)(void *context, struct sieve_ast_argument *arg))
{
	if ( sieve_ast_argument_type(*listitem) == SAAT_STRING ) {
		/* Single string */
		return map_function(context, *listitem);
	} else if ( sieve_ast_argument_type(*listitem) == SAAT_STRING_LIST ) {
		int ret = 0; 
		
		/* String list */
		*listitem = sieve_ast_strlist_first(*listitem);
		
		while ( *listitem != NULL ) {
			
			if ( (ret=map_function(context, *listitem)) <= 0 )
				return ret;
			
			*listitem = sieve_ast_strlist_next(*listitem);
		}
		
		return ret;
	} 
	
	i_unreached();
	return -1;
}

struct sieve_ast_argument *sieve_ast_stringlist_join
(struct sieve_ast_argument *list, struct sieve_ast_argument *items)
{
	enum sieve_ast_argument_type list_type, items_type;
	struct sieve_ast_argument *newlist;
	
	list_type = sieve_ast_argument_type(list);
	items_type = sieve_ast_argument_type(items);
	
	switch ( list_type ) {
	
	case SAAT_STRING:
		switch ( items_type ) {
		
		case SAAT_STRING:
			newlist = 
				sieve_ast_argument_create(list->ast, list->source_line);
			newlist->type = SAAT_STRING_LIST;
			newlist->_value.strlist = NULL;
			
			sieve_ast_argument_substitute(list, newlist);
			sieve_ast_arguments_detach(items, 1);
			
			if ( !_sieve_ast_stringlist_add_item(newlist, list) ||
				!_sieve_ast_stringlist_add_item(newlist, items) ) {
				return NULL;
			}
			
			return newlist;
			
		case SAAT_STRING_LIST:
			/* Adding stringlist to string; make them swith places and add one to the
			 * other.
			 */
			sieve_ast_arguments_detach(items, 1);
			sieve_ast_argument_substitute(list, items);
			if ( !_sieve_ast_stringlist_add_item(items, list) ) 
				return NULL;
			
			return list;
			
		default:
			i_unreached();
		}
		break;
		
	case SAAT_STRING_LIST:
		switch ( items_type ) {
		
		case SAAT_STRING:
			/* Adding string to stringlist; straightforward add */
			sieve_ast_arguments_detach(items, 1);
			if ( !_sieve_ast_stringlist_add_item(list, items) )
				return NULL;
			
			return list;
			
		case SAAT_STRING_LIST:
			/* Adding stringlist to stringlist; perform actual join */
			sieve_ast_arguments_detach(items, 1);
			if ( !sieve_ast_stringlist_add_stringlist(list, items) )
				return NULL;
			
			return list;
			
		default:
			i_unreached();
		}
		
		break;
	default:
		i_unreached();
	}
	
	return NULL;
}


/* Debug */

/* Unparsing, currently implemented using plain printf()s */

static void sieve_ast_unparse_string(const string_t *strval) 
{
	char *str = t_strdup_noconst(str_c((string_t *) strval));

	if ( strchr(str, '\n') != NULL && str[strlen(str)-1] == '\n' ) {
		/* Print it as a multi-line string and do required dotstuffing */
		char *spos = str;
		char *epos = strchr(str, '\n');
		printf("text:\n");
		
		while ( epos != NULL ) {
			*epos = '\0';
			if ( *spos == '.' ) 
				printf(".");
			
			printf("%s\n", spos);
			
			spos = epos+1;
			epos = strchr(spos, '\n');
		}
		if ( *spos == '.' ) 
				printf(".");
		
		printf("%s\n.\n", spos);	
	} else {
		/* Print it as a quoted string and escape " */
		char *spos = str;
		char *epos = strchr(str, '"');
		printf("\"");
		
		while ( epos != NULL ) {
			*epos = '\0';
			printf("%s\\\"", spos);
			
			spos = epos+1;
			epos = strchr(spos, '"');
		}
		
		printf("%s\"", spos);
	}
}

static void sieve_ast_unparse_argument
	(struct sieve_ast_argument *argument, int level);

static void sieve_ast_unparse_stringlist
(struct sieve_ast_argument *strlist, int level) 
{
	struct sieve_ast_argument *stritem;
	
	if ( sieve_ast_strlist_count(strlist) > 1 ) { 
		int i;
		
		printf("[\n");
	
		/* Create indent */
		for ( i = 0; i < level+2; i++ ) 
			printf("  ");	

		stritem = sieve_ast_strlist_first(strlist);
		sieve_ast_unparse_string(sieve_ast_strlist_str(stritem));
		
		stritem = sieve_ast_strlist_next(stritem);
		while ( stritem != NULL ) {
			printf(",\n");
			for ( i = 0; i < level+2; i++ ) 
				printf("  ");
			sieve_ast_unparse_string(sieve_ast_strlist_str(stritem));
		  stritem = sieve_ast_strlist_next(stritem);
	  }
 
		printf(" ]");
	} else {
		stritem = sieve_ast_strlist_first(strlist);
		if ( stritem != NULL ) 
			sieve_ast_unparse_string(sieve_ast_strlist_str(stritem));
	}
}

static void sieve_ast_unparse_argument
(struct sieve_ast_argument *argument, int level) 
{
	switch ( argument->type ) {
	case SAAT_STRING:
		sieve_ast_unparse_string(sieve_ast_argument_str(argument));
		break;
	case SAAT_STRING_LIST:
		sieve_ast_unparse_stringlist(argument, level+1);
		break;
	case SAAT_NUMBER:
		printf("%d", sieve_ast_argument_number(argument));
		break;
	case SAAT_TAG:
		printf(":%s", sieve_ast_argument_tag(argument));
		break;
	default:
		printf("??ARGUMENT??");
		break;
	}
}

static void sieve_ast_unparse_test
	(struct sieve_ast_node *node, int level);

static void sieve_ast_unparse_tests
(struct sieve_ast_node *node, int level) 
{
	struct sieve_ast_node *test;
	
	if ( sieve_ast_test_count(node) > 1 ) { 
		int i;
		
		printf(" (\n");
	
		/* Create indent */
		for ( i = 0; i < level+2; i++ ) 
			printf("  ");	

		test = sieve_ast_test_first(node);
		sieve_ast_unparse_test(test, level+1);
		
		test = sieve_ast_test_next(test);
		while ( test != NULL ) {
			printf(", \n");
			for ( i = 0; i < level+2; i++ ) 
				printf("  ");
			sieve_ast_unparse_test(test, level+1);
		  test = sieve_ast_test_next(test);
	  }
 
		printf(" )");
	} else {
		test = sieve_ast_test_first(node);
		if ( test != NULL ) 
			sieve_ast_unparse_test(test, level);
	}
}

static void sieve_ast_unparse_test
(struct sieve_ast_node *node, int level) 
{
	struct sieve_ast_argument *argument;
		
	printf(" %s", node->identifier);
	
	argument = sieve_ast_argument_first(node);
	while ( argument != NULL ) {
		printf(" ");
		sieve_ast_unparse_argument(argument, level);
		argument = sieve_ast_argument_next(argument);
	}
	
	sieve_ast_unparse_tests(node, level);
}

static void sieve_ast_unparse_command
(struct sieve_ast_node *node, int level) 
{
	struct sieve_ast_node *command;
	struct sieve_ast_argument *argument;
	
	int i;
	
	/* Create indent */
	for ( i = 0; i < level; i++ ) 
		printf("  ");
		
	printf("%s", node->identifier);
	
	argument = sieve_ast_argument_first(node);
	while ( argument != NULL ) {
		printf(" ");
		sieve_ast_unparse_argument(argument, level);
		argument = sieve_ast_argument_next(argument);
	}
	
	sieve_ast_unparse_tests(node, level);
	
	command = sieve_ast_command_first(node);
	if ( command != NULL ) {
		printf(" {\n");
		
		while ( command != NULL) {	
			sieve_ast_unparse_command(command, level+1);
			command = sieve_ast_command_next(command);
		}
		
		for ( i = 0; i < level; i++ ) 
			printf("  ");
		printf("}\n");
	} else 
		printf(";\n");
}

void sieve_ast_unparse(struct sieve_ast *ast) 
{
	struct sieve_ast_node *command;

	printf("Unparsing Abstract Syntax Tree:\n");

	T_BEGIN {	
		command = sieve_ast_command_first(sieve_ast_root(ast));
		while ( command != NULL ) {	
			sieve_ast_unparse_command(command, 0);
			command = sieve_ast_command_next(command);
		}		
	} T_END;
}


