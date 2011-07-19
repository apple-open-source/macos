/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension imapflags
 * --------------------
 *
 * Authors: Stephan Bosch
 * Specification: draft-melnikov-sieve-imapflags-03.txt
 * Implementation: full, but deprecated; provided for backwards compatibility
 * Status: testing
 *
 */

#include "lib.h"
#include "mempool.h"
#include "str.h"

#include "sieve-common.h"

#include "sieve-ast.h"
#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-actions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"

#include "ext-imap4flags-common.h"

/*
 * Commands
 */

static bool cmd_mark_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);

/* Mark command
 *
 * Syntax:
 *   mark
 */

static const struct sieve_command_def cmd_mark = {
    "mark",
    SCT_COMMAND,
    0, 0, FALSE, FALSE,
    NULL, NULL,
    cmd_mark_validate,
    NULL, NULL,
};

/* Unmark command
 *
 * Syntax:
 *   unmark
 */
static const struct sieve_command_def cmd_unmark = {
    "unmark",
    SCT_COMMAND,
    0, 0, FALSE, FALSE,
    NULL, NULL,
    cmd_mark_validate,
    NULL, NULL,
};

/* 
 * Extension
 */

static bool ext_imapflags_load
	(const struct sieve_extension *ext, void **context);
static bool ext_imapflags_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *valdtr);
static bool ext_imapflags_interpreter_load
	(const struct sieve_extension *ext, const struct sieve_runtime_env *renv,
		sieve_size_t *address);

const struct sieve_extension_def imapflags_extension = { 
	"imapflags", 
	ext_imapflags_load, 
	NULL,
	ext_imapflags_validator_load, 
	NULL,
	ext_imapflags_interpreter_load, 
	NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_NO_OPERATIONS, 
	SIEVE_EXT_DEFINE_NO_OPERANDS
};

static bool ext_imapflags_load
(const struct sieve_extension *ext, void **context)
{
	if ( *context == NULL ) {	
		/* Make sure real extension is registered, it is needed by the binary */
		*context = (void *)	
			sieve_extension_require(ext->svinst, &imap4flags_extension);
	}

	return TRUE;
}

/*
 * Validator
 */

static bool ext_imapflags_validator_extension_validate
	(const struct sieve_extension *ext, struct sieve_validator *valdtr, 
		void *context, struct sieve_ast_argument *require_arg);

const struct sieve_validator_extension imapflags_validator_extension = {
	&imapflags_extension,
	ext_imapflags_validator_extension_validate,
	NULL
};

static bool ext_imapflags_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	const struct sieve_extension *master_ext = 
		(const struct sieve_extension *) ext->context;

	sieve_validator_extension_register
		(valdtr, ext, &imapflags_validator_extension, NULL);

	/* Register commands */
	sieve_validator_register_command(valdtr, master_ext, &cmd_setflag);
	sieve_validator_register_command(valdtr, master_ext, &cmd_addflag);
	sieve_validator_register_command(valdtr, master_ext, &cmd_removeflag);

	sieve_validator_register_command(valdtr, master_ext, &cmd_mark);
	sieve_validator_register_command(valdtr, master_ext, &cmd_unmark);	

    /* Attach implicit flags tag to keep and fileinto commands */
    ext_imap4flags_attach_flags_tag(valdtr, master_ext, "keep", TRUE);
    ext_imap4flags_attach_flags_tag(valdtr, master_ext, "fileinto", TRUE);
	
	return TRUE;
}

static bool ext_imapflags_validator_extension_validate
(const struct sieve_extension *ext, struct sieve_validator *valdtr,
	void *context ATTR_UNUSED, struct sieve_ast_argument *require_arg)
{
	const struct sieve_extension *master_ext = 
		(const struct sieve_extension *) ext->context;

	if ( sieve_validator_extension_loaded(valdtr, master_ext) ) {
		sieve_argument_validate_error(valdtr, require_arg,
			"the (deprecated) imapflags extension cannot be used "
			"together with the imap4flags extension");
		return FALSE;
	}

	return TRUE;
}

/*
 * Interpreter
 */

static bool ext_imapflags_interpreter_load
(const struct sieve_extension *ext, const struct sieve_runtime_env *renv,
	sieve_size_t *address ATTR_UNUSED)
{
	const struct sieve_extension *master_ext = 
		(const struct sieve_extension *) ext->context;

	sieve_interpreter_extension_register
		(renv->interp, master_ext, &imap4flags_interpreter_extension, NULL);

	return TRUE;
}

/*
 * Command validation
 */ 

static bool cmd_mark_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd)
{
	if ( sieve_command_is(cmd, cmd_mark) )
		cmd->def = &cmd_addflag;
	else
		cmd->def = &cmd_removeflag;

	cmd->first_positional = sieve_ast_argument_cstring_create
		(cmd->ast_node, "\\flagged", cmd->ast_node->source_line);

	if ( !sieve_validator_argument_activate
		(valdtr, cmd, cmd->first_positional, FALSE) )
		return FALSE;	
		
	return TRUE;
}
