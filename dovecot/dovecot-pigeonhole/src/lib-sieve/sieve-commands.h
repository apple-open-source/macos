/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_COMMANDS_H
#define __SIEVE_COMMANDS_H

#include "lib.h"

#include "sieve-common.h"
#include "sieve-ast.h"

/* 
 * Argument definition
 */

struct sieve_argument_def {
	const char *identifier;
	
	bool (*is_instance_of)
		(struct sieve_validator *valdtr, struct sieve_command *cmd,
			const struct sieve_extension *ext, const char *identifier, void **data);
	
	bool (*validate)
		(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
			struct sieve_command *cmd);
	bool (*validate_context)
		(struct sieve_validator *valdtr, struct sieve_ast_argument *arg, 
			struct sieve_command *cmd);
	bool (*validate_persistent) 
		(struct sieve_validator *valdtr, struct sieve_command *cmd,
			const struct sieve_extension *ext);

	bool (*generate)
		(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
			struct sieve_command *cmd);
};

/*
 * Argument instance
 */

struct sieve_argument {
	const struct sieve_argument_def *def;
	const struct sieve_extension *ext;
	int id_code;

	/* Context data */
	void *data;
};

#define sieve_argument_is(ast_arg, definition) \
	( (ast_arg)->argument->def == &(definition) )
#define sieve_argument_ext(ast_arg) \
	( (ast_arg)->argument->ext )
#define sieve_argument_identifier(ast_arg) \
	( (ast_arg)->argument->def->identifier )

/* Utility macros */

#define sieve_argument_is_string_literal(arg) \
	( (arg)->argument->def == &string_argument )

/* Error handling */

#define sieve_argument_validate_error(validator, arg_node, ...) \
	sieve_validator_error(validator, (arg_node)->source_line, __VA_ARGS__)
#define sieve_argument_validate_warning(validator, arg_node, ...) \
	sieve_validator_warning(validator, (arg_node)->source_line, __VA_ARGS__)

/* Argument API */

struct sieve_argument *sieve_argument_create
	(struct sieve_ast *ast, const struct sieve_argument_def *def,
		const struct sieve_extension *ext, int id_code);

/* Literal arguments */

extern const struct sieve_argument_def number_argument;
extern const struct sieve_argument_def string_argument;
extern const struct sieve_argument_def string_list_argument;

/* Catenated string argument */

bool sieve_arg_catenated_string_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
		struct sieve_command *context);

struct sieve_arg_catenated_string;		

struct sieve_arg_catenated_string *sieve_arg_catenated_string_create
	(struct sieve_ast_argument *orig_arg);
void sieve_arg_catenated_string_add_element
	(struct sieve_arg_catenated_string *strdata, 
		struct sieve_ast_argument *element);

/* 
 * Command definition
 */

enum sieve_command_type {
	SCT_NONE,
	SCT_COMMAND,
	SCT_TEST,
	SCT_HYBRID
};

struct sieve_command_def {
	const char *identifier;
	enum sieve_command_type type;
	
	/* High-level command syntax */
	int positional_arguments;
	int subtests;
	bool block_allowed;
	bool block_required;
	
	bool (*registered)
		(struct sieve_validator *valdtr, const struct sieve_extension *ext,
			struct sieve_command_registration *cmd_reg); 
	bool (*pre_validate)
		(struct sieve_validator *valdtr, struct sieve_command *cmd); 
	bool (*validate)
		(struct sieve_validator *valdtr, struct sieve_command *cmd);
	bool (*validate_const)
		(struct sieve_validator *valdtr, struct sieve_command *cmd,
			int *const_current, int const_next); 
	bool (*generate) 
		(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);
	bool (*control_generate) 
		(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd,
		struct sieve_jumplist *jumps, bool jump_true);
};

/*
 * Command instance
 */

struct sieve_command {
	const struct sieve_command_def *def;
	const struct sieve_extension *ext;
	
	/* The registration of this command in the validator (sieve-validator.h) */
	struct sieve_command_registration *reg;

	/* The ast node of this command */
	struct sieve_ast_node *ast_node;
			
	/* First positional argument, found during argument validation */
	struct sieve_ast_argument *first_positional;

	/* The child ast node that unconditionally exits this command's block */
	struct sieve_command *block_exit_command;

	/* Context data*/
	void *data;
};

#define sieve_command_is(cmd, definition) \
	( (cmd)->def == &(definition) )
#define sieve_command_identifier(cmd) \
	( (cmd)->def->identifier )
#define sieve_command_type_name(cmd) \
	( sieve_command_def_type_name((cmd)->def) )	

#define sieve_commands_equal(cmd1, cmd2) \
	( (cmd1)->def == (cmd2)->def )

/* Context API */

struct sieve_command *sieve_command_create
	(struct sieve_ast_node *cmd_node, const struct sieve_extension *ext,
		const struct sieve_command_def *cmd_def,
		struct sieve_command_registration *cmd_reg);
		
const char *sieve_command_def_type_name
	(const struct sieve_command_def *cmd_def);		

struct sieve_command *sieve_command_prev
	(struct sieve_command *cmd); 
struct sieve_command *sieve_command_parent	
	(struct sieve_command *cmd);
	
struct sieve_ast_argument *sieve_command_add_dynamic_tag
	(struct sieve_command *cmd, const struct sieve_extension *ext,
		const struct sieve_argument_def *tag, int id_code);
struct sieve_ast_argument *sieve_command_find_argument
	(struct sieve_command *cmd, const struct sieve_argument_def *argument);	
	
void sieve_command_exit_block_unconditionally
	(struct sieve_command *cmd);
bool sieve_command_block_exits_unconditionally
	(struct sieve_command *cmd);
	
/* Error handling */
		
#define sieve_command_validate_error(validator, context, ...) \
	sieve_validator_error(validator, (context)->ast_node->source_line, __VA_ARGS__)
#define sieve_command_validate_warning(validator, context, ...) \
	sieve_validator_warning(validator, (context)->ast_node->source_line, __VA_ARGS__)

#define sieve_command_generate_error(gentr, context, ...) \
	sieve_generator_error(gentr, (context)->ast_node->source_line, __VA_ARGS__)

/* Utility macros */

#define sieve_command_pool(context) \
	sieve_ast_node_pool((context)->ast_node)

#define sieve_command_source_line(context) \
	(context)->ast_node->source_line

#define sieve_command_first_argument(context) \
	sieve_ast_argument_first((context)->ast_node)
	
#define sieve_command_is_toplevel(context) \
	( sieve_ast_node_type(sieve_ast_node_parent((context)->ast_node)) == SAT_ROOT )
#define sieve_command_is_first(context) \
	( sieve_ast_node_prev((context)->ast_node) == NULL )	

/*
 * Core commands
 */
 
extern const struct sieve_command_def cmd_require;
extern const struct sieve_command_def cmd_stop;
extern const struct sieve_command_def cmd_if;
extern const struct sieve_command_def cmd_elsif;
extern const struct sieve_command_def cmd_else;
extern const struct sieve_command_def cmd_redirect;
extern const struct sieve_command_def cmd_keep;
extern const struct sieve_command_def cmd_discard;

extern const struct sieve_command_def *sieve_core_commands[];
extern const unsigned int sieve_core_commands_count;

/* 
 * Core tests 
 */

extern const struct sieve_command_def tst_true;
extern const struct sieve_command_def tst_false;
extern const struct sieve_command_def tst_not;
extern const struct sieve_command_def tst_anyof;
extern const struct sieve_command_def tst_allof;
extern const struct sieve_command_def tst_address;
extern const struct sieve_command_def tst_header;
extern const struct sieve_command_def tst_exists;
extern const struct sieve_command_def tst_size;

extern const struct sieve_command_def *sieve_core_tests[];
extern const unsigned int sieve_core_tests_count;

/*
 * Command utility functions
 */

bool sieve_command_verify_headers_argument
(struct sieve_validator *valdtr, struct sieve_ast_argument *headers);

#endif /* __SIEVE_COMMANDS_H */
