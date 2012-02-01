/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_INCLUDE_COMMON_H
#define __EXT_INCLUDE_COMMON_H

#include "lib.h"
#include "hash.h"

#include "sieve-common.h"
#include "sieve-extensions.h"

/* 
 * Forward declarations
 */

struct ext_include_script_info;
struct ext_include_binary_context;

/* 
 * Types 
 */

enum ext_include_script_location { 
	EXT_INCLUDE_LOCATION_PERSONAL, 
	EXT_INCLUDE_LOCATION_GLOBAL,
	EXT_INCLUDE_LOCATION_INVALID 
};

static inline const char *ext_include_script_location_name
(enum ext_include_script_location location)
{
	switch ( location ) {
	case EXT_INCLUDE_LOCATION_PERSONAL:
		return "personal";

	case EXT_INCLUDE_LOCATION_GLOBAL:
		return "global";

	default:
		break;
	}

	return "[INVALID LOCATION]";
}


/* 
 * Extension 
 */

extern const struct sieve_extension_def include_extension;
extern const struct sieve_binary_extension include_binary_ext;

bool ext_include_load
	(const struct sieve_extension *ext, void **context);
void ext_include_unload
	(const struct sieve_extension *ext);

/* 
 * Commands 
 */

extern const struct sieve_command_def cmd_include;
extern const struct sieve_command_def cmd_return;
extern const struct sieve_command_def cmd_global;

/* DEPRICATED */ 
extern const struct sieve_command_def cmd_import;
extern const struct sieve_command_def cmd_export;

/*
 * Operations
 */
 
enum ext_include_opcode {
	EXT_INCLUDE_OPERATION_INCLUDE,
	EXT_INCLUDE_OPERATION_RETURN,
	EXT_INCLUDE_OPERATION_GLOBAL
};
 
extern const struct sieve_operation_def include_operation;
extern const struct sieve_operation_def return_operation;
extern const struct sieve_operation_def global_operation;

/* 
 * Script access 
 */

const char *ext_include_get_script_directory
	(const struct sieve_extension *ext,
		enum ext_include_script_location location, const char *script_name);

/* 
 * Context 
 */
 
/* Extension context */

struct ext_include_context {
	/* Extension dependencies */
	const struct sieve_extension *var_ext;

	/* Configuration */
 	char *global_dir;
	char *personal_dir;

	unsigned int max_nesting_depth;
	unsigned int max_includes;
};

static inline struct ext_include_context *ext_include_get_context
(const struct sieve_extension *ext)
{
	return (struct ext_include_context *) ext->context;
}

/* AST Context */

struct ext_include_ast_context {
  struct sieve_variable_scope *global_vars;

  ARRAY_DEFINE(included_scripts, struct sieve_script *);
};

struct ext_include_ast_context *ext_include_create_ast_context
	(const struct sieve_extension *this_ext, struct sieve_ast *ast, 
		struct sieve_ast *parent);
struct ext_include_ast_context *ext_include_get_ast_context
	(const struct sieve_extension *this_ext, struct sieve_ast *ast);

void ext_include_ast_link_included_script
	(const struct sieve_extension *this_ext, struct sieve_ast *ast, 
		struct sieve_script *script);

bool ext_include_validator_have_variables
	(const struct sieve_extension *this_ext, struct sieve_validator *valdtr);

/* Generator context */

void ext_include_register_generator_context
	(const struct sieve_extension *this_ext, 
		const struct sieve_codegen_env *cgenv);

bool ext_include_generate_include
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd,
		enum ext_include_script_location location, struct sieve_script *script, 
		const struct ext_include_script_info **included_r, bool once);

/* Interpreter context */

void ext_include_interpreter_context_init
	(const struct sieve_extension *this_ext, struct sieve_interpreter *interp);

int ext_include_execute_include
	(const struct sieve_runtime_env *renv, unsigned int block_id, bool once);
void ext_include_execute_return(const struct sieve_runtime_env *renv);

struct sieve_variable_storage *ext_include_interpreter_get_global_variables
	(const struct sieve_extension *this_ext, struct sieve_interpreter *interp);

#endif /* __EXT_INCLUDE_COMMON_H */
