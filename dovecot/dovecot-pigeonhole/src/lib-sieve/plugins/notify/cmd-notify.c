/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "array.h"
#include "str.h"
#include "ioloop.h"
#include "str-sanitize.h"
#include "message-date.h"
#include "mail-storage.h"

#include "rfc2822.h"

#include "sieve-common.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-actions.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-result.h"
#include "sieve-address.h"
#include "sieve-message.h"
#include "sieve-smtp.h"

#include "ext-notify-common.h"
#include "ext-notify-limits.h"

#include <ctype.h>

/* Notify command (DEPRECATED)
 *
 * Syntax:
 *   notify [":method" string] [":id" string] [":options" string-list]
 *          [<":low" / ":normal" / ":high">] ["message:" string]
 *
 */

static bool cmd_notify_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		struct sieve_command_registration *cmd_reg);
static bool cmd_notify_pre_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_notify_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_notify_generate
	(const struct sieve_codegen_env *cgenv, 
		struct sieve_command *ctx);

const struct sieve_command_def cmd_notify_old = {
	"notify",
	SCT_COMMAND,
	0, 0, FALSE, FALSE,
	cmd_notify_registered,
	cmd_notify_pre_validate,
	cmd_notify_validate,
	NULL,
	cmd_notify_generate, 
	NULL,
};

/*
 * Tagged arguments
 */

/* Forward declarations */

static bool cmd_notify_validate_string_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg,
		struct sieve_command *cmd);
static bool cmd_notify_validate_stringlist_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);

/* Argument objects */

static const struct sieve_argument_def notify_method_tag = {
	"method",
	NULL,
	cmd_notify_validate_string_tag,
	NULL, NULL, NULL
};

static const struct sieve_argument_def notify_options_tag = { 
	"options", 
	NULL, 
	cmd_notify_validate_stringlist_tag, 
	NULL, NULL, NULL 
};

static const struct sieve_argument_def notify_id_tag = {
	"id",
	NULL,
	cmd_notify_validate_string_tag,
	NULL, NULL, NULL
};

static const struct sieve_argument_def notify_message_tag = {
	"message",
	NULL,
	cmd_notify_validate_string_tag,
	NULL, NULL, NULL
};

/* 
 * Notify operation 
 */

static bool cmd_notify_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_notify_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def notify_old_operation = { 
	"NOTIFY",
	&notify_extension,
	EXT_NOTIFY_OPERATION_NOTIFY,
	cmd_notify_operation_dump, 
	cmd_notify_operation_execute
};

/* Codes for optional operands */

enum cmd_notify_optional {
  OPT_END,
  OPT_MESSAGE,
  OPT_IMPORTANCE,
  OPT_OPTIONS,
  OPT_ID
};

/* 
 * Notify action 
 */

/* Forward declarations */

static int act_notify_check_duplicate
	(const struct sieve_runtime_env *renv, 
		const struct sieve_action *act,
		const struct sieve_action *act_other);
static void act_notify_print
	(const struct sieve_action *action, const struct sieve_result_print_env *rpenv,
		bool *keep);	
static bool act_notify_commit
	(const struct sieve_action *action,	const struct sieve_action_exec_env *aenv, 
		void *tr_context, bool *keep);

/* Action object */

const struct sieve_action_def act_notify_old = {
	"notify",
	0,
	NULL,
	act_notify_check_duplicate, 
	NULL,
	act_notify_print,
	NULL, NULL,
	act_notify_commit,
	NULL
};

/*
 * Command validation context
 */

struct cmd_notify_context_data {
	struct sieve_ast_argument *id;
	struct sieve_ast_argument *method;
	struct sieve_ast_argument *options;
	struct sieve_ast_argument *message;
};

/*
 * Tag validation
 */

static bool cmd_notify_validate_string_tag
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg,
    struct sieve_command *cmd)
{
	struct sieve_ast_argument *tag = *arg;
	struct cmd_notify_context_data *ctx_data =
		(struct cmd_notify_context_data *) cmd->data;

	/* Detach the tag itself */
	*arg = sieve_ast_arguments_detach(*arg, 1);

	/* Check syntax:
	 *   :id <string>
	 *   :method <string>
	 *   :message <string>
	 */
	if ( !sieve_validate_tag_parameter
		(valdtr, cmd, tag, *arg, NULL, 0, SAAT_STRING, FALSE) )
		return FALSE;

	if ( sieve_argument_is(tag, notify_method_tag) ) {
		ctx_data->method = *arg;
	
		/* Removed */
		*arg = sieve_ast_arguments_detach(*arg, 1);

	} else if ( sieve_argument_is(tag, notify_id_tag) ) {
		ctx_data->id = *arg;

		/* Skip parameter */
		*arg = sieve_ast_argument_next(*arg);

	} else if ( sieve_argument_is(tag, notify_message_tag) ) {
		ctx_data->message = *arg;

		/* Skip parameter */
		*arg = sieve_ast_argument_next(*arg);
	}

	return TRUE;
}

static bool cmd_notify_validate_stringlist_tag
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd)
{
	struct sieve_ast_argument *tag = *arg;
	struct cmd_notify_context_data *ctx_data = 
		(struct cmd_notify_context_data *) cmd->data; 

	/* Detach the tag itself */
	*arg = sieve_ast_arguments_detach(*arg,1);
	
	/* Check syntax:
	 *   :options string-list
	 */
	if ( !sieve_validate_tag_parameter
		(valdtr, cmd, tag, *arg, NULL, 0, SAAT_STRING_LIST, FALSE) ) 
		return FALSE;
		
	/* Assign context */
	ctx_data->options = *arg;	
	
	/* Skip parameter */
	*arg = sieve_ast_argument_next(*arg);

	return TRUE;
}

/*
 * Command registration
 */

static bool cmd_notify_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	struct sieve_command_registration *cmd_reg)
{
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &notify_method_tag, 0);
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &notify_id_tag, OPT_ID);
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &notify_message_tag, OPT_MESSAGE);
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &notify_options_tag, OPT_OPTIONS);

	ext_notify_register_importance_tags(valdtr, cmd_reg, ext, OPT_IMPORTANCE);

	return TRUE;
}

/*
 * Command validation
 */

static bool cmd_notify_pre_validate
(struct sieve_validator *valdtr ATTR_UNUSED,
	struct sieve_command *cmd)
{
	struct cmd_notify_context_data *ctx_data;
	
	/* Create context */
	ctx_data = p_new(sieve_command_pool(cmd),	struct cmd_notify_context_data, 1);
	cmd->data = ctx_data;

	return TRUE;
}

static int cmd_notify_address_validate
(void *context, struct sieve_ast_argument *arg)
{
	struct sieve_validator *valdtr = (struct sieve_validator *) context;

	if ( sieve_argument_is_string_literal(arg) ) {
		string_t *address = sieve_ast_argument_str(arg);
		const char *error;
		bool result = FALSE;

		T_BEGIN {
			result = sieve_address_validate(address, &error);

			if ( !result ) {
				sieve_argument_validate_error(valdtr, arg,
					"specified :options address '%s' is invalid for "
					"the mailto notify method: %s",
					str_sanitize(str_c(address), 128), error);
			}
		} T_END;

		return result;
	}

	return TRUE;
}

static bool cmd_notify_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd)
{
	struct cmd_notify_context_data *ctx_data =
		(struct cmd_notify_context_data *) cmd->data;

	/* Check :method argument */
	if ( ctx_data->method != NULL )	{
		const char *method = sieve_ast_argument_strc(ctx_data->method);
		
		if ( strcasecmp(method, "mailto") != 0 ) {
			sieve_command_validate_error(valdtr, cmd,
				"the notify command of the deprecated notify extension "
				"only supports the 'mailto' notification method");
			return FALSE;
		}
	}

	/* Check :options argument */
	if ( ctx_data->options != NULL ) {
		struct sieve_ast_argument *option = ctx_data->options;
		
		/* Parse and check options */
		if ( sieve_ast_stringlist_map
			(&option, (void *) valdtr, cmd_notify_address_validate) <= 0 ) {
			return FALSE;
		}
	} else {
		sieve_command_validate_warning(valdtr, cmd,
			"no :options (and hence recipients) specified for the notify command");
	}

	return TRUE;
}

/*
 * Code generation
 */

static bool cmd_notify_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd)
{
	sieve_operation_emit(cgenv->sblock, cmd->ext, &notify_old_operation);

	/* Generate arguments */
	return sieve_generate_arguments(cgenv, cmd, NULL);
}

/* 
 * Code dump
 */
 
static bool cmd_notify_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{	
	int opt_code = 0;
	
	sieve_code_dumpf(denv, "NOTIFY");
	sieve_code_descend(denv);	

	/* Dump optional operands */
	for (;;) {
		int opt;
		bool opok = TRUE;

		if ( (opt=sieve_opr_optional_dump(denv, address, &opt_code)) < 0 )
			return FALSE;

		if ( opt == 0 ) break;

		switch ( opt_code ) {
		case OPT_IMPORTANCE:
			opok = sieve_opr_number_dump(denv, address, "importance");
			break;
		case OPT_ID:
			opok = sieve_opr_string_dump(denv, address, "id");
			break;
		case OPT_OPTIONS:
			opok = sieve_opr_stringlist_dump(denv, address, "options");
			break;
		case OPT_MESSAGE:
			opok = sieve_opr_string_dump(denv, address, "message");
			break;
		default:
			return FALSE;
		}

		if ( !opok ) return FALSE;
	}
	
	return TRUE;
}

/* 
 * Code execution
 */

 
static int cmd_notify_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{	
	const struct sieve_extension *this_ext = renv->oprtn->ext;
	struct ext_notify_action *act;
	pool_t pool;
	int opt_code = 0;
	sieve_number_t importance = 1;
	struct sieve_stringlist *options = NULL;
	string_t *message = NULL, *id = NULL; 
	int ret = 0;

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
		case OPT_IMPORTANCE:
			ret = sieve_opr_number_read(renv, address, "importance", &importance);
			break;
		case OPT_ID:
			ret = sieve_opr_string_read(renv, address, "id", &id);
			break;
		case OPT_MESSAGE:
			ret = sieve_opr_string_read(renv, address, "from", &message);
			break;
		case OPT_OPTIONS:
			ret = sieve_opr_stringlist_read(renv, address, "options", &options);
			break;
		default:
			sieve_runtime_trace_error(renv, "unknown optional operand");
			return SIEVE_EXEC_BIN_CORRUPT;
		}

		if ( ret <= 0 ) return ret;
	}
		
	/*
	 * Perform operation
	 */

	/* Enforce 0 < importance < 4 (just to be sure) */

	if ( importance < 1 ) 
		importance = 1;
	else if ( importance > 3 )
		importance = 3;

	/* Trace */

	sieve_runtime_trace(renv, SIEVE_TRLVL_ACTIONS, "notify action");	

	/* Compose action */
	if ( options != NULL ) {
		string_t *raw_address;
		string_t *out_message;

		pool = sieve_result_pool(renv->result);
		act = p_new(pool, struct ext_notify_action, 1);
		if ( id != NULL )
				act->id = p_strdup(pool, str_c(id));
		act->importance = importance;		
	
		/* Process message */

		out_message = t_str_new(1024);
		ext_notify_construct_message
			(renv, (message == NULL ? NULL : str_c(message)), out_message);
		act->message = p_strdup(pool, str_c(out_message));
		
		/* Normalize and verify all :options addresses */					

		sieve_stringlist_reset(options);
			
		p_array_init(&act->recipients, pool, 4);
		
		raw_address = NULL;
		while ( (ret=sieve_stringlist_next_item(options, &raw_address)) > 0 ) {
			const char *error = NULL;
			const char *addr_norm = sieve_address_normalize(raw_address, &error);
			
			/* Add if valid address */
			if ( addr_norm != NULL ) {
				const struct ext_notify_recipient *rcpts;
				unsigned int rcpt_count, i;

				/* Prevent duplicates */
				rcpts = array_get(&act->recipients, &rcpt_count);
				
				for ( i = 0; i < rcpt_count; i++ ) {
					if ( sieve_address_compare
						(rcpts[i].normalized, addr_norm, TRUE) == 0 )
						break;
				}
	
				/* Add only if unique */
				if ( i != rcpt_count ) {
					sieve_runtime_warning(renv, NULL,
						"duplicate recipient '%s' specified in the :options argument of "
						"the deprecated notify command", 
						str_sanitize(str_c(raw_address), 128));

				}	else if 
					( array_count(&act->recipients) >= EXT_NOTIFY_MAX_RECIPIENTS ) {
					sieve_runtime_warning(renv, NULL,
						"more than the maximum %u recipients are specified "
						"for the deprecated notify command; "
						"the rest is discarded", EXT_NOTIFY_MAX_RECIPIENTS);
					break;

				} else {						
					struct ext_notify_recipient recipient;			

					recipient.full = p_strdup(pool, str_c(raw_address));
					recipient.normalized = p_strdup(pool, addr_norm);
		
					array_append(&act->recipients, &recipient, 1);
				}
			} else {
				sieve_runtime_error(renv, NULL,
					"specified :options address '%s' is invalid for "
					"the deprecated notify command: %s", 
					str_sanitize(str_c(raw_address), 128), error);
				return SIEVE_EXEC_FAILURE;
			}
		}

		if ( ret < 0 ) {
			sieve_runtime_trace_error(renv, "invalid options stringlist");
			return SIEVE_EXEC_BIN_CORRUPT;
		}

		if ( sieve_result_add_action
			(renv, this_ext, &act_notify_old, NULL, (void *) act, 0) < 0 )
			return SIEVE_EXEC_FAILURE;
	}

	return SIEVE_EXEC_OK;
}

/*
 * Action
 */

/* Runtime verification */

static int act_notify_check_duplicate
(const struct sieve_runtime_env *renv ATTR_UNUSED, 
	const struct sieve_action *act ATTR_UNUSED,
	const struct sieve_action *act_other ATTR_UNUSED)
{
	struct ext_notify_action *new_nact, *old_nact;
	const struct ext_notify_recipient *new_rcpts;
	const struct ext_notify_recipient *old_rcpts;
	unsigned int new_count, old_count, i, j;
	unsigned int del_start = 0, del_len = 0;
		
	if ( act->context == NULL || act_other->context == NULL )
		return 0;

	new_nact = (struct ext_notify_action *) act->context;
	old_nact = (struct ext_notify_action *) act_other->context;

	new_rcpts = array_get(&new_nact->recipients, &new_count);
	old_rcpts = array_get(&old_nact->recipients, &old_count);

	for ( i = 0; i < new_count; i++ ) {
		for ( j = 0; j < old_count; j++ ) {
			if ( sieve_address_compare
				(new_rcpts[i].normalized, old_rcpts[j].normalized, TRUE) == 0 )
				break;				
		}

		if ( j == old_count ) {
			/* Not duplicate */
			if ( del_len > 0 ) {
				/* Perform pending deletion */
				array_delete(&new_nact->recipients, del_start, del_len);

				/* Make sure the loop integrity is maintained */
				i -= del_len;
				new_rcpts = array_get(&new_nact->recipients, &new_count);
			}

			del_len = 0;		
		} else {
			/* Mark deletion */
			if ( del_len == 0 )
				del_start = i;
			del_len++;
		}
	}

	/* Perform pending deletion */
	if ( del_len > 0 ) {
		array_delete(&new_nact->recipients, del_start, del_len);			
	}

	return ( array_count(&new_nact->recipients) > 0 ? 0 : 1 );
}

/* Result printing */
 
static void act_notify_print
(const struct sieve_action *action,	const struct sieve_result_print_env *rpenv, 
	bool *keep ATTR_UNUSED)	
{
	const struct ext_notify_action *act = 
		(const struct ext_notify_action *) action->context;
	const struct ext_notify_recipient *recipients;
	unsigned int count, i;

	sieve_result_action_printf
		( rpenv, "send (depricated) notification with method 'mailto':");
	
	/* Print main method parameters */

	sieve_result_printf
		( rpenv, "    => importance    : %d\n", act->importance);

	if ( act->message != NULL )
		sieve_result_printf
			( rpenv, "    => message       : %s\n", act->message);

	if ( act->id != NULL )
		sieve_result_printf
			( rpenv, "    => id            : %s \n", act->id);

	/* Print mailto: recipients */

	sieve_result_printf
		( rpenv, "    => recipients    :\n" );

	recipients = array_get(&act->recipients, &count);
	if ( count == 0 ) {
		sieve_result_printf(rpenv, "       NONE, action has no effect\n");
	} else {
		for ( i = 0; i < count; i++ ) {
			sieve_result_printf
				( rpenv, "       + To: %s\n", recipients[i].full);
		}
	}

	/* Finish output with an empty line */

	sieve_result_printf(rpenv, "\n");
}

/* Result execution */

static bool contains_8bit(const char *msg)
{
	const unsigned char *s = (const unsigned char *)msg;

	for (; *s != '\0'; s++) {
		if ((*s & 0x80) != 0)
			return TRUE;
	}
	return FALSE;
}

static bool act_notify_send
(const struct sieve_action_exec_env *aenv, 
	const struct ext_notify_action *act)
{ 
	const struct sieve_script_env *senv = aenv->scriptenv;
	const struct ext_notify_recipient *recipients;
	void *smtp_handle;
	unsigned int count, i;
	FILE *f;
	const char *outmsgid;

	/* Get recipients */
	recipients = array_get(&act->recipients, &count);
	if ( count == 0  ) {
		sieve_result_warning(aenv, 
			"notify action specifies no recipients; action has no effect");
		return TRUE;
	}

	/* Just to be sure */
	if ( senv->smtp_open == NULL || senv->smtp_close == NULL ) {
		sieve_result_global_warning(aenv, 
			"notify action has no means to send mail");
		return TRUE;
	}
	
	/* Send message to all recipients */
	for ( i = 0; i < count; i++ ) {

		if ( sieve_message_get_sender(aenv->msgctx) != NULL )
			smtp_handle = sieve_smtp_open
				(senv, recipients[i].normalized, senv->postmaster_address, &f);
		else		
			smtp_handle = sieve_smtp_open
				(senv, recipients[i].normalized, NULL, &f);

		outmsgid = sieve_message_get_new_id(senv);
	
		rfc2822_header_field_write(f, "X-Sieve", SIEVE_IMPLEMENTATION);
		rfc2822_header_field_write(f, "Message-ID", outmsgid);
		rfc2822_header_field_write(f, "Date", message_date_create(ioloop_time));

		/* Set importance */
		switch ( act->importance ) {
		case 1:
			rfc2822_header_field_write(f, "X-Priority", "1 (Highest)");
			rfc2822_header_field_write(f, "Importance", "High");
			break;
		case 3:
			rfc2822_header_field_write(f, "X-Priority", "5 (Lowest)");
			rfc2822_header_field_write(f, "Importance", "Low");
			break;
		case 2:
		default:
			rfc2822_header_field_write(f, "X-Priority", "3 (Normal)");
			rfc2822_header_field_write(f, "Importance", "Normal");
			break;
		}

		rfc2822_header_field_printf(f, "From", "%s", 
			t_strdup_printf("Postmaster <%s>", senv->postmaster_address));

		rfc2822_header_field_printf(f, "To", "%s", recipients[i].full);

		rfc2822_header_field_write(f, "Subject", "[SIEVE] New mail notification");

		rfc2822_header_field_write(f, "Auto-Submitted", "auto-generated (notify)");
		rfc2822_header_field_write(f, "Precedence", "bulk");

		if (contains_8bit(act->message)) {
			rfc2822_header_field_write(f, "MIME-Version", "1.0");
			rfc2822_header_field_write(f, 
				"Content-Type", "text/plain; charset=UTF-8");
			rfc2822_header_field_write(f, "Content-Transfer-Encoding", "8bit");
		}

		/* Generate message body */
		fprintf(f, "\r\n");
		fprintf(f, "%s\r\n", act->message);
			
		if ( sieve_smtp_close(senv, smtp_handle) ) {
			sieve_result_global_log(aenv, 
				"sent mail notification to <%s>", 
				str_sanitize(recipients[i].normalized, 80));
		} else {
			sieve_result_global_error(aenv,
				"failed to send mail notification to <%s> "
				"(refer to system log for more information)", 
				str_sanitize(recipients[i].normalized, 80));
		}
	}

	return TRUE;
}

static bool act_notify_commit
(const struct sieve_action *action, const struct sieve_action_exec_env *aenv, 
	void *tr_context ATTR_UNUSED, bool *keep ATTR_UNUSED)
{
	const struct ext_notify_action *act = 
		(const struct ext_notify_action *) action->context;
	const struct sieve_message_data *msgdata = aenv->msgdata;
	const char *const *headers;

	/* Is the message an automatic reply ? */
	if ( mail_get_headers
		(msgdata->mail, "auto-submitted", &headers) >= 0 ) {
		const char *const *hdsp = headers;

		/* Theoretically multiple headers could exist, so lets make sure */
		while ( *hdsp != NULL ) {
			if ( strcasecmp(*hdsp, "no") != 0 ) {
				sieve_result_global_log(aenv, 
					"not sending notification for auto-submitted message from <%s>", 
					str_sanitize(msgdata->return_path, 128));	
					return TRUE;				 
			}
			hdsp++;
		}
	}

	return act_notify_send(aenv, act);
}







