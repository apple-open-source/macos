/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-code.h"
#include "sieve-binary.h"
#include "sieve-dump.h"

#include "testsuite-common.h"
#include "testsuite-settings.h"

/*
 * Commands
 */

static bool cmd_test_config_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

/* Test_config_set command
 *
 * Syntax:   
 *   test_config_set <setting: string> <value: string>
 */

static bool cmd_test_config_set_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);

const struct sieve_command_def cmd_test_config_set = { 
	"test_config_set", 
	SCT_COMMAND, 
	2, 0, FALSE, FALSE,
	NULL, NULL,
	cmd_test_config_set_validate, 
	cmd_test_config_generate, 
	NULL 
};

/* Test_config_unset command
 *
 * Syntax:   
 *   test_config_unset <setting: string> 
 */

static bool cmd_test_config_unset_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);

const struct sieve_command_def cmd_test_config_unset = { 
	"test_config_unset", 
	SCT_COMMAND, 
	1, 0, FALSE, FALSE,
	NULL, NULL,
	cmd_test_config_unset_validate, 
	cmd_test_config_generate, 
	NULL 
};

/* Test_config_reload command
 *
 * Syntax:   
 *   test_config_reload [:extension <extension: string>]
 */

static bool cmd_test_config_reload_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	struct sieve_command_registration *cmd_reg); 

const struct sieve_command_def cmd_test_config_reload = { 
	"test_config_reload", 
	SCT_COMMAND, 
	0, 0, FALSE, FALSE,
	cmd_test_config_reload_registered, 
	NULL, NULL, 
	cmd_test_config_generate, 
	NULL 
};

/*
 * Command tags
 */

/* Forward declarations */

static bool cmd_test_config_reload_validate_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);

/* Argument objects */

static const struct sieve_argument_def test_config_reload_extension_tag = { 
	"extension", 
	NULL, 
	cmd_test_config_reload_validate_tag, 
	NULL, NULL, NULL, 
};

/* Codes for optional arguments */

enum cmd_test_config_optional {
	OPT_END,
	OPT_EXTENSION
};

/* 
 * Operations
 */ 
 
/* Test_config_set operation */

static bool cmd_test_config_set_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_test_config_set_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def test_config_set_operation = { 
	"TEST_CONFIG_SET",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_CONFIG_SET,
	cmd_test_config_set_operation_dump, 
	cmd_test_config_set_operation_execute 
};

/* Test_config_unset operation */

static bool cmd_test_config_unset_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_test_config_unset_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def test_config_unset_operation = { 
	"TEST_CONFIG_UNSET",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_CONFIG_UNSET,
	cmd_test_config_unset_operation_dump, 
	cmd_test_config_unset_operation_execute 
};

/* Test_config_read operation */

static bool cmd_test_config_reload_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_test_config_reload_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def test_config_reload_operation = { 
	"TEST_CONFIG_RELOAD",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_CONFIG_RELOAD,
	cmd_test_config_reload_operation_dump, 
	cmd_test_config_reload_operation_execute 
};

/*
 * Tag validation
 */

static bool cmd_test_config_reload_validate_tag
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd)
{
	struct sieve_ast_argument *tag = *arg;

	/* Detach the tag itself */
	*arg = sieve_ast_arguments_detach(*arg,1);

	/* Check syntax:
	 *   :extension <extension: string>
	 */
	if ( !sieve_validate_tag_parameter
		(valdtr, cmd, tag, *arg, NULL, 0, SAAT_STRING, TRUE) ) {
		return FALSE;
	}

	/* Skip parameter */
	*arg = sieve_ast_argument_next(*arg);
	
	return TRUE;
}

/* 
 * Command registration 
 */

static bool cmd_test_config_reload_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	struct sieve_command_registration *cmd_reg) 
{
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &test_config_reload_extension_tag, OPT_EXTENSION); 	

	return TRUE;
}

/* 
 * Command validation 
 */

static bool cmd_test_config_set_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd)
{
	struct sieve_ast_argument *arg = cmd->first_positional;

	/* Check syntax:
	 *   <setting: string> <value: string>
	 */

	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "setting", 1, SAAT_STRING) ) {
		return FALSE;
	}	

	if ( !sieve_validator_argument_activate(valdtr, cmd, arg, FALSE) )
		return FALSE;

	arg = sieve_ast_argument_next(arg);

	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "value", 2, SAAT_STRING) ) {
		return FALSE;
	}	

	return sieve_validator_argument_activate(valdtr, cmd, arg, FALSE);
}

static bool cmd_test_config_unset_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd)
{
	struct sieve_ast_argument *arg = cmd->first_positional;

	/* Check syntax:
	 *   <setting: string>
	 */

	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "setting", 1, SAAT_STRING) ) {
		return FALSE;
	}

	return sieve_validator_argument_activate(valdtr, cmd, arg, FALSE);
}

/* 
 * Code generation 
 */

static bool cmd_test_config_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd)
{	  	
	if ( sieve_command_is(cmd, cmd_test_config_set) )	
		sieve_operation_emit
			(cgenv->sblock, cmd->ext, &test_config_set_operation);
	else if ( sieve_command_is(cmd, cmd_test_config_unset) )	
		sieve_operation_emit
			(cgenv->sblock, cmd->ext, &test_config_unset_operation);
	else if ( sieve_command_is(cmd, cmd_test_config_reload) )	
		sieve_operation_emit
			(cgenv->sblock, cmd->ext, &test_config_reload_operation);
	else
		i_unreached();

 	/* Generate arguments */
	if ( !sieve_generate_arguments(cgenv, cmd, NULL) )
		return FALSE;

	return TRUE;
}

/* 
 * Code dump
 */
 
static bool cmd_test_config_set_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "TEST_CONFIG_SET:");
	
	sieve_code_descend(denv);
	
	return sieve_opr_string_dump(denv, address, "setting") &&
		sieve_opr_string_dump(denv, address, "value");
}

static bool cmd_test_config_unset_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "TEST_CONFIG_UNSET:");
	
	sieve_code_descend(denv);

	return 
		sieve_opr_string_dump(denv, address, "setting");
}

static bool cmd_test_config_reload_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	int opt_code = 0;
	
	sieve_code_dumpf(denv, "TEST_CONFIG_RELOAD:");	
	sieve_code_descend(denv);

	/* Dump optional operands */

	for (;;) {
		int opt;
		bool opok = TRUE;

		if ( (opt=sieve_opr_optional_dump(denv, address, &opt_code)) < 0 )
			return FALSE;

		if ( opt == 0 ) break;

		switch ( opt_code ) {
		case OPT_EXTENSION:
			opok = sieve_opr_string_dump(denv, address, "extensions");
			break;
		default:
			opok = FALSE;
			break;
		}

		if ( !opok ) return FALSE;
	}

	return TRUE;
}

/*
 * Intepretation
 */
 
static int cmd_test_config_set_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	string_t *setting;
	string_t *value;
	int ret;

	/* 
	 * Read operands 
	 */

	/* Setting */
	if ( (ret=sieve_opr_string_read(renv, address, "setting", &setting)) <= 0 )
		return ret;

	/* Value */
	if ( (ret=sieve_opr_string_read(renv, address, "value", &value)) <= 0 )
		return ret;

	/*
	 * Perform operation
	 */
		
	if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_COMMANDS) ) {
		sieve_runtime_trace(renv, 0,
			"testsuite: test_config_set command");
		sieve_runtime_trace_descend(renv);
		sieve_runtime_trace(renv, 0, "set config `%s' = `%s'", 
			str_c(setting), str_c(value));
	}

	testsuite_setting_set(str_c(setting), str_c(value));

	return SIEVE_EXEC_OK;
}

static int cmd_test_config_unset_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	string_t *setting;
	int ret;

	/* 
	 * Read operands 
	 */

	/* Setting */
	if ( (ret=sieve_opr_string_read(renv, address, "setting", &setting)) <= 0 )
		return ret;

	/*
	 * Perform operation
	 */
		
	if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_COMMANDS) ) {
		sieve_runtime_trace(renv, 0,
			"testsuite: test_config_unset command");
		sieve_runtime_trace_descend(renv);
		sieve_runtime_trace(renv, 0, "unset config `%s'", str_c(setting));
	}

	testsuite_setting_unset(str_c(setting));

	return SIEVE_EXEC_OK;
}

static int cmd_test_config_reload_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	const struct sieve_extension *ext;
	int opt_code = 0;
	string_t *extension = NULL;
	int ret;

	/* 
	 * Read operands 
	 */
	
	/* Optional operands */
	for (;;) {
		int opt;

		if ( (opt=sieve_opr_optional_read(renv, address, &opt_code)) < 0 )
			return SIEVE_EXEC_BIN_CORRUPT;

		if ( opt == 0 ) break;

		switch ( opt_code ) {
		case OPT_EXTENSION:
			ret = sieve_opr_string_read(renv, address, "extension", &extension);
			break;
		default:
			sieve_runtime_trace_error(renv, "unknown optional operand");
			ret = SIEVE_EXEC_BIN_CORRUPT;
		}

		if ( ret <= 0 ) return ret;
	}

	/*
	 * Perform operation
	 */

	if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_COMMANDS) ) {
		sieve_runtime_trace(renv, 0,
			"testsuite: test_config_reload command");
		sieve_runtime_trace_descend(renv);
	}

	if ( extension == NULL ) {
		testsuite_test_failf("test_config_reload: "
			":extension argument is currently mandatory");
		return SIEVE_EXEC_OK;
	}	

	if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_COMMANDS) ) {
		sieve_runtime_trace(renv, 0, "reload configuration for extension `%s'", 
			str_c(extension));
	}

	ext = sieve_extension_get_by_name(renv->svinst, str_c(extension));
	if ( ext == NULL ) {
		testsuite_test_failf("test_config_reload: "
			"unknown extension '%s'", str_c(extension));
		return SIEVE_EXEC_OK;
	}	

	sieve_extension_reload(ext);
	return SIEVE_EXEC_OK;
}





