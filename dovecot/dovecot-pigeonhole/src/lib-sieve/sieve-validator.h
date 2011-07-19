/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
 
#ifndef __SIEVE_VALIDATOR_H
#define __SIEVE_VALIDATOR_H

#include "lib.h"

#include "sieve-common.h"

/*
 * Types
 */

enum sieve_argument_type {
	SAT_NUMBER,
	SAT_CONST_STRING,
	SAT_VAR_STRING,
	SAT_STRING_LIST,
	
	SAT_COUNT
};

struct sieve_command_registration;

/*
 * Validator
 */
 
struct sieve_validator;

struct sieve_validator *sieve_validator_create
	(struct sieve_ast *ast, struct sieve_error_handler *ehandler);
void sieve_validator_free(struct sieve_validator **valdtr);
pool_t sieve_validator_pool(struct sieve_validator *valdtr);

bool sieve_validator_run(struct sieve_validator *valdtr);

/*
 * Accessors
 */
 
struct sieve_error_handler *sieve_validator_error_handler
	(struct sieve_validator *valdtr);
struct sieve_ast *sieve_validator_ast
	(struct sieve_validator *valdtr);
struct sieve_script *sieve_validator_script
	(struct sieve_validator *valdtr);
struct sieve_instance *sieve_validator_svinst
	(struct sieve_validator *valdtr);

/*
 * Error handling
 */

void sieve_validator_warning
	(struct sieve_validator *valdtr, unsigned int source_line, 
		const char *fmt, ...) ATTR_FORMAT(3, 4);
void sieve_validator_error
	(struct sieve_validator *valdtr, unsigned int source_line, 
		const char *fmt, ...) ATTR_FORMAT(3, 4);
void sieve_validator_critical
	(struct sieve_validator *valdtr, unsigned int source_line, 
		const char *fmt, ...) ATTR_FORMAT(3, 4);
		
/* 
 * Command/Test registry
 */
 
void sieve_validator_register_command
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		const struct sieve_command_def *command);
	
/* 
 * Per-command tagged argument registry
 */

void sieve_validator_register_tag
	(struct sieve_validator *valdtr, struct sieve_command_registration *cmd_reg,
		const struct sieve_extension *ext, const struct sieve_argument_def *tag_def, 
		int id_code);
void sieve_validator_register_external_tag
	(struct sieve_validator *valdtr, const char *command, 
		const struct sieve_extension *ext, const struct sieve_argument_def *tag_def,
		int id_code);
void sieve_validator_register_persistent_tag
	(struct sieve_validator *valdtr, const char *command,
		const struct sieve_extension *ext, 
		const struct sieve_argument_def *tag_def);
	
/*
 * Overriding the default literal arguments
 */	
 
void sieve_validator_argument_override
(struct sieve_validator *valdtr, enum sieve_argument_type type, 
	const struct sieve_extension *ext, const struct sieve_argument_def *arg_def);
bool sieve_validator_argument_activate_super
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	struct sieve_ast_argument *arg, bool constant);
		
/* 
 * Argument validation API
 */

bool sieve_validate_positional_argument
	(struct sieve_validator *valdtr, struct sieve_command *cmd,
		struct sieve_ast_argument *arg, const char *arg_name, unsigned int arg_pos,
		enum sieve_ast_argument_type req_type);
bool sieve_validator_argument_activate
	(struct sieve_validator *valdtr, struct sieve_command *cmd,
		struct sieve_ast_argument *arg, bool constant);
		
bool sieve_validate_tag_parameter
	(struct sieve_validator *valdtr, struct sieve_command *cmd,
		struct sieve_ast_argument *tag, struct sieve_ast_argument *param,
		const char *arg_name, unsigned int arg_pos,
		enum sieve_ast_argument_type req_type, bool constant);
	
/* 
 * Extension support
 */

struct sieve_validator_extension {
	const struct sieve_extension_def *ext;	

	bool (*validate)
		(const struct sieve_extension *ext, struct sieve_validator *valdtr, 
			void *context, struct sieve_ast_argument *require_arg);

	void (*free)
		(const struct sieve_extension *ext, struct sieve_validator *valdtr, 
			void *context);
};

const struct sieve_extension *sieve_validator_extension_load
	(struct sieve_validator *valdtr, struct sieve_command *cmd,
		struct sieve_ast_argument *ext_arg, string_t *ext_name);
const struct sieve_extension *sieve_validator_extension_load_implicit
	(struct sieve_validator *valdtr, const char *ext_name);

void sieve_validator_extension_register
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		const struct sieve_validator_extension *valext, void *context);
bool sieve_validator_extension_loaded
    (struct sieve_validator *valdtr, const struct sieve_extension *ext);

void sieve_validator_extension_set_context
(struct sieve_validator *valdtr, const struct sieve_extension *ext, 
	void *context);
void *sieve_validator_extension_get_context
(struct sieve_validator *valdtr, const struct sieve_extension *ext);

/*
 * Validator object registry
 */

struct sieve_validator_object_registry;

struct sieve_validator_object_registry *sieve_validator_object_registry_get
	(struct sieve_validator *valdtr, const struct sieve_extension *ext);
void sieve_validator_object_registry_add
	(struct sieve_validator_object_registry *regs,
		const struct sieve_extension *ext, const struct sieve_object_def *obj_def);
bool sieve_validator_object_registry_find
	(struct sieve_validator_object_registry *regs, const char *identifier,
		struct sieve_object *obj);
struct sieve_validator_object_registry *sieve_validator_object_registry_create
	(struct sieve_validator *valdtr);
struct sieve_validator_object_registry *sieve_validator_object_registry_init
	(struct sieve_validator *valdtr, const struct sieve_extension *ext);

#endif /* __SIEVE_VALIDATOR_H */
