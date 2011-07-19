/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_ENOTIFY_COMMON_H
#define __EXT_ENOTIFY_COMMON_H

#include "lib.h"
#include "array.h"

#include "sieve-common.h"

#include "sieve-ext-variables.h"

#include "sieve-ext-enotify.h"

/*
 * Extension
 */

extern const struct sieve_extension_def enotify_extension;
extern const struct sieve_extension_capabilities notify_capabilities;

struct ext_enotify_context {
	const struct sieve_extension *var_ext;
	ARRAY_DEFINE(notify_methods, struct sieve_enotify_method);
};


/*
 * Commands
 */

extern const struct sieve_command_def notify_command;

/* Codes for optional arguments */

enum cmd_notify_optional {
    CMD_NOTIFY_OPT_END,
    CMD_NOTIFY_OPT_FROM,
    CMD_NOTIFY_OPT_OPTIONS,
    CMD_NOTIFY_OPT_MESSAGE,
    CMD_NOTIFY_OPT_IMPORTANCE
};

/*
 * Tests
 */

extern const struct sieve_command_def valid_notify_method_test;
extern const struct sieve_command_def notify_method_capability_test;

/*
 * Operations
 */

extern const struct sieve_operation_def notify_operation;
extern const struct sieve_operation_def valid_notify_method_operation;
extern const struct sieve_operation_def notify_method_capability_operation;

enum ext_variables_opcode {
	EXT_ENOTIFY_OPERATION_NOTIFY,
	EXT_ENOTIFY_OPERATION_VALID_NOTIFY_METHOD,
	EXT_ENOTIFY_OPERATION_NOTIFY_METHOD_CAPABILITY
};

/*
 * Operands
 */
 
extern const struct sieve_operand_def encodeurl_operand;

/*
 * Modifiers
 */

extern const struct sieve_variables_modifier_def encodeurl_modifier;

/*
 * Notify methods
 */
  
void ext_enotify_methods_init
	(struct sieve_instance *svinst, struct ext_enotify_context *ectx);
void ext_enotify_methods_deinit
	(struct ext_enotify_context *ectx);

const struct sieve_enotify_method *ext_enotify_method_find
	(const struct sieve_extension *ntfy_ext, const char *identifier);
	
/*
 * Validation
 */
 
bool ext_enotify_compile_check_arguments
	(struct sieve_validator *valdtr, struct sieve_command *cmd,
		struct sieve_ast_argument *uri_arg, struct sieve_ast_argument *msg_arg,
		struct sieve_ast_argument *from_arg, 	
		struct sieve_ast_argument *options_arg);

/*
 * Runtime
 */
 
bool ext_enotify_runtime_method_validate
	(const struct sieve_runtime_env *renv,
		string_t *method_uri);
 
const char *ext_enotify_runtime_get_method_capability
	(const struct sieve_runtime_env *renv,
		string_t *method_uri, const char *capability);

int ext_enotify_runtime_check_operands
	(const struct sieve_runtime_env *renv,
		string_t *method_uri, string_t *message, string_t *from, 
		struct sieve_stringlist *options, 
		const struct sieve_enotify_method **method_r, void **method_context);
		
/*
 * Method printing
 */

struct sieve_enotify_print_env {
	const struct sieve_result_print_env *result_penv;
};

#endif /* __EXT_ENOTIFY_COMMON_H */
