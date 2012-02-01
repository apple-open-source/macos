/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "ioloop.h"
#include "str-sanitize.h"
#include "istream.h"
#include "istream-crlf.h"
#include "istream-header-filter.h"

#include "rfc2822.h"

#include "sieve-common.h"
#include "sieve-limits.h"
#include "sieve-address.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-actions.h"
#include "sieve-validator.h" 
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-code-dumper.h"
#include "sieve-result.h"
#include "sieve-smtp.h"
#include "sieve-message.h"

#include <stdio.h>

/* 
 * Configuration 
 */

#define CMD_REDIRECT_DUPLICATE_KEEP (3600 * 24)

/* 
 * Redirect command 
 * 
 * Syntax
 *   redirect <address: string>
 */

static bool cmd_redirect_validate
	(struct sieve_validator *validator, struct sieve_command *cmd);
static bool cmd_redirect_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);

const struct sieve_command_def cmd_redirect = { 
	"redirect", 
	SCT_COMMAND,
	1, 0, FALSE, FALSE, 
	NULL, NULL,
	cmd_redirect_validate,
	NULL,
	cmd_redirect_generate, 
	NULL 
};

/* 
 * Redirect operation 
 */

static bool cmd_redirect_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_redirect_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def cmd_redirect_operation = { 
	"REDIRECT",
	NULL, 
	SIEVE_OPERATION_REDIRECT,
	cmd_redirect_operation_dump, 
	cmd_redirect_operation_execute 
};

/* 
 * Redirect action 
 */

static bool act_redirect_equals
	(const struct sieve_script_env *senv, const struct sieve_action *act1, 
		const struct sieve_action *act2);
static int act_redirect_check_duplicate
	(const struct sieve_runtime_env *renv,
		const struct sieve_action *act, 
		const struct sieve_action *act_other);
static void act_redirect_print
	(const struct sieve_action *action, const struct sieve_result_print_env *rpenv,
		bool *keep);	
static bool act_redirect_commit
	(const struct sieve_action *action, const struct sieve_action_exec_env *aenv,
		void *tr_context, bool *keep);
		
const struct sieve_action_def act_redirect = {
	"redirect",
	SIEVE_ACTFLAG_TRIES_DELIVER,
	act_redirect_equals,
	act_redirect_check_duplicate, 
	NULL,
	act_redirect_print,
	NULL, NULL,
	act_redirect_commit,
	NULL
};

struct act_redirect_context {
	const char *to_address;
};

/* 
 * Validation 
 */

static bool cmd_redirect_validate
(struct sieve_validator *validator, struct sieve_command *cmd) 
{
	struct sieve_ast_argument *arg = cmd->first_positional;

	/* Check and activate address argument */

	if ( !sieve_validate_positional_argument
		(validator, cmd, arg, "address", 1, SAAT_STRING) ) {
		return FALSE;
	}

	if ( !sieve_validator_argument_activate(validator, cmd, arg, FALSE) )
		return FALSE;

	/* We can only assess the validity of the outgoing address when it is
	 * a string literal. For runtime-generated strings this needs to be
	 * done at runtime.
	 */
	if ( sieve_argument_is_string_literal(arg) ) {
		string_t *address = sieve_ast_argument_str(arg);
		const char *error;
		const char *norm_address;

		T_BEGIN {
			/* Verify and normalize the address to 'local_part@domain' */
			norm_address = sieve_address_normalize(address, &error);

			if ( norm_address == NULL ) {
				sieve_argument_validate_error(validator, arg,
					"specified redirect address '%s' is invalid: %s",
					str_sanitize(str_c(address),128), error);
			} else {
				/* Replace string literal in AST */
				sieve_ast_argument_string_setc(arg, norm_address);
			}
		} T_END;

		return ( norm_address != NULL );
	}

	return TRUE;
}

/*
 * Code generation
 */
 
static bool cmd_redirect_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	sieve_operation_emit(cgenv->sblock, NULL,  &cmd_redirect_operation);

	/* Generate arguments */
	return sieve_generate_arguments(cgenv, cmd, NULL);
}

/* 
 * Code dump
 */
 
static bool cmd_redirect_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "REDIRECT");
	sieve_code_descend(denv);

	if ( sieve_action_opr_optional_dump(denv, address, NULL) != 0 )
		return FALSE;

	return sieve_opr_string_dump(denv, address, "address");
}

/*
 * Code execution
 */

static int cmd_redirect_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	struct sieve_instance *svinst = renv->svinst;
	struct sieve_side_effects_list *slist = NULL;
	struct act_redirect_context *act;
	string_t *redirect;
	bool literal_address;
	const char *norm_address;
	pool_t pool;
	int ret;

	/*
	 * Read data
	 */

	/* Optional operands (side effects only) */
	if ( sieve_action_opr_optional_read(renv, address, NULL, &ret, &slist) != 0 ) 
		return ret;

	/* Read the address */
	if ( (ret=sieve_opr_string_read_ex(renv, address, "address", &redirect, 
		&literal_address)) <= 0 )
		return ret;

	/*
	 * Perform operation
	 */

	if ( !literal_address ) {
		const char *error;

		/* Verify and normalize the address to 'local_part@domain' */
		norm_address = sieve_address_normalize(redirect, &error);

		if ( norm_address == NULL ) {
			sieve_runtime_error(renv, NULL,
				"specified redirect address '%s' is invalid: %s",
				str_sanitize(str_c(redirect),128), error);
			return SIEVE_EXEC_FAILURE;
		}
	} else {
		norm_address = str_c(redirect);
	}

	if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_ACTIONS) ) {
		sieve_runtime_trace(renv, 0, "redirect action");
		sieve_runtime_trace_descend(renv);
		sieve_runtime_trace(renv, 0, "forward message to address `%s'",
			str_sanitize(norm_address, 80));
	}

	/* Add redirect action to the result */

	pool = sieve_result_pool(renv->result);
	act = p_new(pool, struct act_redirect_context, 1);
	act->to_address = p_strdup(pool, norm_address);

	if ( sieve_result_add_action
		(renv, NULL, &act_redirect, slist, (void *) act,
			svinst->max_redirects) < 0 )
		return SIEVE_EXEC_FAILURE;

	return SIEVE_EXEC_OK;
}

/*
 * Action implementation
 */

static bool act_redirect_equals
(const struct sieve_script_env *senv ATTR_UNUSED, 
	const struct sieve_action *act1, const struct sieve_action *act2)
{
	struct act_redirect_context *rd_ctx1 =
		(struct act_redirect_context *) act1->context;
	struct act_redirect_context *rd_ctx2 = 
		(struct act_redirect_context *) act2->context;

	/* Address is already normalized */
	return ( sieve_address_compare
		(rd_ctx1->to_address, rd_ctx2->to_address, TRUE) == 0 );
}
 
static int act_redirect_check_duplicate
(const struct sieve_runtime_env *renv ATTR_UNUSED,
	const struct sieve_action *act, 
	const struct sieve_action *act_other)
{
	return ( act_redirect_equals(renv->scriptenv, act, act_other) ? 1 : 0 );
}

static void act_redirect_print
(const struct sieve_action *action, 
	const struct sieve_result_print_env *rpenv, bool *keep)	
{
	struct act_redirect_context *ctx = 
		(struct act_redirect_context *) action->context;
	
	sieve_result_action_printf(rpenv, "redirect message to: %s", 
		str_sanitize(ctx->to_address, 128));
	
	*keep = FALSE;
}

static bool act_redirect_send	
(const struct sieve_action_exec_env *aenv, struct act_redirect_context *ctx)
{
	static const char *hide_headers[] = 
		{ "Return-Path", "X-Sieve", "X-Sieve-Redirected-From" };

	const struct sieve_message_data *msgdata = aenv->msgdata;
	const struct sieve_script_env *senv = aenv->scriptenv;
	const char *sender = sieve_message_get_sender(aenv->msgctx);
	const char *recipient = sieve_message_get_final_recipient(aenv->msgctx);
	struct istream *input, *crlf_input;
	void *smtp_handle;
	FILE *f;
	const unsigned char *data;
	size_t size;
	int ret;
	
	/* Just to be sure */
	if ( !sieve_smtp_available(senv) ) {
		sieve_result_global_warning
			(aenv, "redirect action has no means to send mail.");
		return TRUE;
	}
	
	if (mail_get_stream(msgdata->mail, NULL, NULL, &input) < 0)
		return FALSE;
		
	/* Open SMTP transport */
	smtp_handle = sieve_smtp_open(senv, ctx->to_address, sender, &f);

	/* Remove unwanted headers */
	input = i_stream_create_header_filter
		(input, HEADER_FILTER_EXCLUDE, hide_headers,
			N_ELEMENTS(hide_headers), null_header_filter_callback, NULL);
	
	/* Make sure the message contains CRLF consistently */
	crlf_input = i_stream_create_crlf(input);

	/* Prepend sieve headers (should not affect signatures) */
	rfc2822_header_field_write(f, "X-Sieve", SIEVE_IMPLEMENTATION);
	if ( recipient != NULL )
		rfc2822_header_field_write(f, "X-Sieve-Redirected-From", recipient);

	/* Pipe the message to the outgoing SMTP transport */
	while ((ret = i_stream_read_data(crlf_input, &data, &size, 0)) > 0) {	
		if (fwrite(data, size, 1, f) == 0)
			break;
		i_stream_skip(crlf_input, size);
	}

	i_stream_unref(&crlf_input);
	i_stream_unref(&input);

	/* Close SMTP transport */
	if ( !sieve_smtp_close(senv, smtp_handle) ) {
		sieve_result_global_error(aenv, 
			"failed to redirect message to <%s> "
			"(refer to server log for more information)",
			str_sanitize(ctx->to_address, 80));
		return FALSE;
	}
	
	return TRUE;
}

static bool act_redirect_commit
(const struct sieve_action *action, 
	const struct sieve_action_exec_env *aenv, void *tr_context ATTR_UNUSED,
	bool *keep)
{
	struct act_redirect_context *ctx =
		(struct act_redirect_context *) action->context;
	const struct sieve_message_data *msgdata = aenv->msgdata;
	const struct sieve_script_env *senv = aenv->scriptenv;
	const char *dupeid;

	/* Prevent mail loops if possible */
	dupeid = msgdata->id == NULL ? 
		NULL : t_strdup_printf("%s-%s", msgdata->id, ctx->to_address);
	if (dupeid != NULL) {
		/* Check whether we've seen this message before */
		if (sieve_action_duplicate_check(senv, dupeid, strlen(dupeid))) {
			sieve_result_global_log(aenv, "discarded duplicate forward to <%s>",
				str_sanitize(ctx->to_address, 128));
			return TRUE;
		}
	}

	/* Try to forward the message */
	if ( act_redirect_send(aenv, ctx) ) {

		/* Mark this message id as forwarded to the specified destination */
		if (dupeid != NULL) {
			sieve_action_duplicate_mark(senv, dupeid, strlen(dupeid),
				ioloop_time + CMD_REDIRECT_DUPLICATE_KEEP);
		}

		sieve_result_global_log(aenv, "forwarded to <%s>", 
			str_sanitize(ctx->to_address, 128));	

		/* Indicate that message was successfully forwarded */
		aenv->exec_status->message_forwarded = TRUE;

		/* Cancel implicit keep */
		*keep = FALSE;

		return TRUE;
	}
 
	return FALSE;
}


