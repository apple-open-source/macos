/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension fileinto
 * ------------------
 *
 * Authors: Stephan Bosch 
 * Specification: RFC 5228
 * Implementation: full
 * Status: testing
 *
 */

#include "lib.h"
#include "str-sanitize.h"
#include "unichar.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-binary.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-actions.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-result.h"

/* 
 * Forward declarations 
 */

static const struct sieve_command_def fileinto_command;
const struct sieve_operation_def fileinto_operation;
const struct sieve_extension_def fileinto_extension; 

/* 
 * Extension
 */

static bool ext_fileinto_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr);

const struct sieve_extension_def fileinto_extension = { 
	"fileinto", 
	NULL, NULL,
	ext_fileinto_validator_load, 
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_OPERATION(fileinto_operation), 
	SIEVE_EXT_DEFINE_NO_OPERANDS	
};

static bool ext_fileinto_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	/* Register new command */
	sieve_validator_register_command(valdtr, ext, &fileinto_command);

	return TRUE;
}

/* 
 * Fileinto command
 *
 * Syntax: 
 *   fileinto <folder: string>
 */

static bool cmd_fileinto_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_fileinto_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

static const struct sieve_command_def fileinto_command = { 
	"fileinto", 
	SCT_COMMAND,
	1, 0, FALSE, FALSE, 
	NULL, NULL,
	cmd_fileinto_validate,
	NULL,
	cmd_fileinto_generate, 
	NULL 
};

/* 
 * Fileinto operation 
 */

static bool ext_fileinto_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int ext_fileinto_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address); 

const struct sieve_operation_def fileinto_operation = { 
	"FILEINTO",
	&fileinto_extension,
	0,
	ext_fileinto_operation_dump, 
	ext_fileinto_operation_execute 
};

/* 
 * Validation 
 */

static bool cmd_fileinto_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd) 
{ 	
	struct sieve_ast_argument *arg = cmd->first_positional;
	
	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "folder", 1, SAAT_STRING) ) {
		return FALSE;
	}
	
	if ( !sieve_validator_argument_activate(valdtr, cmd, arg, FALSE) )
		return FALSE;

	/* Check name validity when folder argument is not a variable */
	if ( sieve_argument_is_string_literal(arg) ) {
		const char *folder = sieve_ast_argument_strc(arg);

		if ( !uni_utf8_str_is_valid(folder) ) {
			sieve_command_validate_error(valdtr, cmd, 
				"folder name specified for fileinto command is not utf-8: %s", folder);
			return FALSE;
		}		
	}

	return TRUE;
}

/*
 * Code generation
 */
 
static bool cmd_fileinto_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	sieve_operation_emit(cgenv->sblock, cmd->ext, &fileinto_operation);

	/* Generate arguments */
	return sieve_generate_arguments(cgenv, cmd, NULL);
}

/* 
 * Code dump
 */
 
static bool ext_fileinto_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "FILEINTO");
	sieve_code_descend(denv);

	if ( sieve_action_opr_optional_dump(denv, address, NULL) != 0 )
		return FALSE;

	return sieve_opr_string_dump(denv, address, "folder");
}

/*
 * Execution
 */

static int ext_fileinto_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	struct sieve_side_effects_list *slist = NULL; 
	string_t *folder;
	bool folder_literal;
	bool trace = sieve_runtime_trace_active(renv, SIEVE_TRLVL_ACTIONS);
	int ret = 0;

	/*
	 * Read operands
	 */

	/* Optional operands (side effects only) */
	if ( sieve_action_opr_optional_read(renv, address, NULL, &ret, &slist) != 0 ) 
		return ret;

	/* Folder operand */
	if ( (ret=sieve_opr_string_read_ex(renv, address, "folder", &folder, 
		&folder_literal)) <= 0 )
		return ret;

	/*
	 * Perform operation
	 */

	if ( trace ) {
		sieve_runtime_trace(renv, 0, "fileinto action");
		sieve_runtime_trace_descend(renv);
	}

	if ( !folder_literal && !uni_utf8_str_is_valid(str_c(folder)) ) {
		sieve_runtime_error(renv, NULL, 
			"folder name specified for fileinto command is not utf-8: %s", 
			str_c(folder));
		return SIEVE_EXEC_FAILURE;		
	}
	

	if ( trace ) {
		sieve_runtime_trace(renv, 0, "store message in mailbox `%s'", 
			str_sanitize(str_c(folder), 80));
	}

	/* Add action to result */
	if ( sieve_act_store_add_to_result
		(renv, slist, str_c(folder)) < 0 )
		return SIEVE_EXEC_FAILURE;

	return SIEVE_EXEC_OK;
}






