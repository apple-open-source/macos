/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str-sanitize.h"

#include "sieve-common.h"
#include "sieve-error.h"
#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-actions.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-result.h"

#include "ext-enotify-common.h"

/* 
 * Forward declarations 
 */
 
static const struct sieve_argument_def notify_importance_tag;
static const struct sieve_argument_def notify_from_tag;
static const struct sieve_argument_def notify_options_tag;
static const struct sieve_argument_def notify_message_tag;

/* 
 * Notify command 
 *	
 * Syntax: 
 *    notify [":from" string]
 *           [":importance" <"1" / "2" / "3">]
 *           [":options" string-list]
 *           [":message" string]
 *           <method: string>
 */

static bool cmd_notify_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		struct sieve_command_registration *cmd_reg);
static bool cmd_notify_pre_validate
	(struct sieve_validator *validator, struct sieve_command *cmd);
static bool cmd_notify_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_notify_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

const struct sieve_command_def notify_command = { 
	"notify",
	SCT_COMMAND, 
	1, 0, FALSE, FALSE, 
	cmd_notify_registered,
	cmd_notify_pre_validate,
	cmd_notify_validate,
	NULL, 
	cmd_notify_generate, 
	NULL 
};

/*
 * Notify command tags
 */

/* Forward declarations */

static bool cmd_notify_validate_string_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);
static bool cmd_notify_validate_stringlist_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);
static bool cmd_notify_validate_importance_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);

/* Argument objects */

static const struct sieve_argument_def notify_from_tag = { 
	"from", 
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

static const struct sieve_argument_def notify_message_tag = { 
	"message", 
	NULL, 
	cmd_notify_validate_string_tag, 
	NULL, NULL, NULL 
};

static const struct sieve_argument_def notify_importance_tag = { 
	"importance", 
	NULL,
	cmd_notify_validate_importance_tag, 
	NULL, NULL, NULL 
};

/* 
 * Notify operation 
 */

static bool cmd_notify_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_notify_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def notify_operation = { 
	"NOTIFY",
	&enotify_extension,
	EXT_ENOTIFY_OPERATION_NOTIFY,
	cmd_notify_operation_dump, 
	cmd_notify_operation_execute
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

const struct sieve_action_def act_notify = {
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
	struct sieve_ast_argument *from;
	struct sieve_ast_argument *message;
	struct sieve_ast_argument *options;
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
	*arg = sieve_ast_arguments_detach(*arg,1);
	
	/* Check syntax:
	 *   :from <string>
	 *   :message <string>
	 */
	if ( !sieve_validate_tag_parameter
		(valdtr, cmd, tag, *arg, NULL, 0, SAAT_STRING, FALSE) )
		return FALSE;

	if ( sieve_argument_is(tag, notify_from_tag) ) {
		ctx_data->from = *arg;
		
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

static bool cmd_notify_validate_importance_tag
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd ATTR_UNUSED)
{
	const struct sieve_ast_argument *tag = *arg;
	const char *impstr;

	/* Detach the tag itself */
	*arg = sieve_ast_arguments_detach(*arg,1);

	/* Check syntax: 
	 *   :importance <"1" / "2" / "3">
	 */

	if ( sieve_ast_argument_type(*arg) != SAAT_STRING ) {
		/* Not a string */
		sieve_argument_validate_error(valdtr, *arg, 
			"the :importance tag for the notify command requires a string parameter, "
			"but %s was found", sieve_ast_argument_name(*arg));
		return FALSE;
	}

	impstr = sieve_ast_argument_strc(*arg);

	if ( impstr[0] < '1' || impstr[0]  > '3' || impstr[1] != '\0' ) {
		/* Invalid importance */
		sieve_argument_validate_error(valdtr, *arg, 
			"invalid :importance value for notify command: %s", impstr);
		return FALSE;
	} 

	sieve_ast_argument_number_substitute(*arg, impstr[0] - '0');
	(*arg)->argument = sieve_argument_create
		((*arg)->ast, &number_argument, tag->argument->ext, tag->argument->id_code);

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
		(valdtr, cmd_reg, ext, &notify_importance_tag, CMD_NOTIFY_OPT_IMPORTANCE); 	
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &notify_from_tag, CMD_NOTIFY_OPT_FROM); 	
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &notify_options_tag, CMD_NOTIFY_OPT_OPTIONS); 	
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &notify_message_tag, CMD_NOTIFY_OPT_MESSAGE); 	

	return TRUE;
}

/* 
 * Command validation 
 */

static bool cmd_notify_pre_validate
(struct sieve_validator *validator ATTR_UNUSED, 
	struct sieve_command *cmd) 
{
	struct cmd_notify_context_data *ctx_data;
	
	/* Assign context */
	ctx_data = p_new(sieve_command_pool(cmd), 
		struct cmd_notify_context_data, 1);
	cmd->data = ctx_data;

	return TRUE;
}
 
static bool cmd_notify_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd) 
{ 	
	struct sieve_ast_argument *arg = cmd->first_positional;
	struct cmd_notify_context_data *ctx_data = 
		(struct cmd_notify_context_data *) cmd->data; 

	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "method", 1, SAAT_STRING) ) {
		return FALSE;
	}
	
	if ( !sieve_validator_argument_activate(valdtr, cmd, arg, FALSE) )
		return FALSE;
		
	return ext_enotify_compile_check_arguments
		(valdtr, cmd, arg, ctx_data->message, ctx_data->from, ctx_data->options);
}

/*
 * Code generation
 */
 
static bool cmd_notify_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{		 
	sieve_operation_emit(cgenv->sblock, cmd->ext, &notify_operation);

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
		case CMD_NOTIFY_OPT_IMPORTANCE:
			opok = sieve_opr_number_dump(denv, address, "importance");
			break;
		case CMD_NOTIFY_OPT_FROM:
			opok = sieve_opr_string_dump(denv, address, "from");
			break;
		case CMD_NOTIFY_OPT_OPTIONS:
			opok = sieve_opr_stringlist_dump(denv, address, "options");
			break;
		case CMD_NOTIFY_OPT_MESSAGE:
			opok = sieve_opr_string_dump(denv, address, "message");
			break;
		default:
			return FALSE;
		}

		if ( !opok ) return FALSE;
	}
	
	/* Dump method operand */
	return sieve_opr_string_dump(denv, address, "method");
}

/* 
 * Code execution
 */
 
static int cmd_notify_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{	
	const struct sieve_extension *this_ext = renv->oprtn->ext;
	struct sieve_side_effects_list *slist = NULL;
	struct sieve_enotify_action *act;
	void *method_context;
	pool_t pool;
	int opt_code = 0;
	sieve_number_t importance = 2;
	struct sieve_stringlist *options = NULL;
	const struct sieve_enotify_method *method;
	string_t *method_uri, *message = NULL, *from = NULL; 
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
		case CMD_NOTIFY_OPT_IMPORTANCE:
			ret = sieve_opr_number_read(renv, address, "importance", &importance);	
			break;
		case CMD_NOTIFY_OPT_FROM:
			ret = sieve_opr_string_read(renv, address, "from", &from);
			break;
		case CMD_NOTIFY_OPT_MESSAGE:
			ret = sieve_opr_string_read(renv, address, "message", &message);
			break;
		case CMD_NOTIFY_OPT_OPTIONS:
			ret = sieve_opr_stringlist_read(renv, address, "options", &options);
			break;
		default:
			sieve_runtime_trace_error(renv, "unknown optional operand");
			return SIEVE_EXEC_BIN_CORRUPT;
		}

		if ( ret <= 0 ) return ret;
	}
	
	/* Method operand */

	if ( (ret=sieve_opr_string_read(renv, address, "method", &method_uri)) <= 0 )
		return ret;
		
	/*
	 * Perform operation
	 */

	/* Enforce 0 < importance < 4 (just to be sure) */

	if ( importance < 1 ) 
		importance = 1;
	else if ( importance > 3 )
		importance = 3;

	/* Trace */

	if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_ACTIONS) ) {
		sieve_runtime_trace(renv, 0, "notify action");	
		sieve_runtime_trace_descend(renv);
		sieve_runtime_trace(renv, 0, "notify with uri `%s'",
			str_sanitize(str_c(method_uri), 80));	
	}

	/* Check operands */

	if ( (ret=ext_enotify_runtime_check_operands
		(renv, method_uri, message, from, options, &method, 
			&method_context)) > 0 ) 
	{
		/* Add notify action to the result */

		pool = sieve_result_pool(renv->result);
		act = p_new(pool, struct sieve_enotify_action, 1);
		act->method = method;
		act->method_context = method_context;
		act->importance = importance;
		if ( message != NULL )
			act->message = p_strdup(pool, str_c(message));
		if ( from != NULL )
			act->from = p_strdup(pool, str_c(from));

		if ( sieve_result_add_action
			(renv, this_ext, &act_notify, slist, (void *) act, 0) < 0 )
			return SIEVE_EXEC_FAILURE;

		return SIEVE_EXEC_OK;
	}

	return ret;
}

/*
 * Action
 */

/* Runtime verification */

static int act_notify_check_duplicate
(const struct sieve_runtime_env *renv, const struct sieve_action *act,
	const struct sieve_action *act_other)
{
	const struct sieve_enotify_action *nact, *nact_other;
	const struct sieve_enotify_method_def *nmth_def;
	struct sieve_enotify_env nenv;
	bool result = TRUE;
		
	if ( act->context == NULL || act_other->context == NULL )
		return 0;

	nact = (const struct sieve_enotify_action *) act->context;
	nact_other = (const struct sieve_enotify_action *) act_other->context;

	if ( nact->method == NULL || nact->method->def == NULL ) 
		return 0;

	nmth_def = nact->method->def;
	if ( nmth_def->action_check_duplicates == NULL )
		return 0;

	memset(&nenv, 0, sizeof(nenv));
	nenv.svinst = renv->svinst;
	nenv.method = nact->method;	
	nenv.ehandler = sieve_prefix_ehandler_create
		(sieve_result_get_error_handler(renv->result), act->location, "notify");

	result = nmth_def->action_check_duplicates(&nenv, nact, nact_other);

	sieve_error_handler_unref(&nenv.ehandler);
	
	return result;
}

/* Result printing */
 
static void act_notify_print
(const struct sieve_action *action, const struct sieve_result_print_env *rpenv, 
	bool *keep ATTR_UNUSED)	
{
	const struct sieve_enotify_action *act = 
		(const struct sieve_enotify_action *) action->context;
	const struct sieve_enotify_method *method;

	method = act->method;

	if ( method->def != NULL ) {
		sieve_result_action_printf
			( rpenv, "send notification with method '%s:':", method->def->identifier);

		if ( method->def->action_print != NULL ) {
			struct sieve_enotify_print_env penv;

			memset(&penv, 0, sizeof(penv));
			penv.result_penv = rpenv;

			method->def->action_print(&penv, act);
		}
	}
}

/* Result execution */

static bool act_notify_commit
(const struct sieve_action *action,	const struct sieve_action_exec_env *aenv,
	void *tr_context ATTR_UNUSED, bool *keep ATTR_UNUSED)
{
	const struct sieve_enotify_action *act = 
		(const struct sieve_enotify_action *) action->context;
	const struct sieve_enotify_method *method = act->method;
	struct sieve_enotify_exec_env nenv;
	bool result = TRUE;

	if ( method->def != NULL && method->def->action_execute != NULL )	{	
		/* Compose log structure */
		memset(&nenv, 0, sizeof(nenv));
		nenv.svinst = aenv->svinst;
		nenv.method = method;	
		nenv.scriptenv = aenv->scriptenv;
		nenv.msgdata = aenv->msgdata;
		nenv.msgctx = aenv->msgctx;

		nenv.ehandler = sieve_prefix_ehandler_create
			(aenv->ehandler, NULL, "notify action");

		result = method->def->action_execute(&nenv, act);

		sieve_error_handler_unref(&nenv.ehandler);
	}
			
	return result;
}




