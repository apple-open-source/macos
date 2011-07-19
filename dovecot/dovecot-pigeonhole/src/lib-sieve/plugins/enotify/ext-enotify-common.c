/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
 
#include "lib.h"
#include "str.h"
#include "str-sanitize.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-ast.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-interpreter.h"
#include "sieve-result.h"

#include "ext-enotify-limits.h"
#include "ext-enotify-common.h"

#include <ctype.h>

/* FIXME: (from draft RFC)
 *
 * Header/envelope tests [Sieve] together with Sieve variables can be
 * used to extract the list of users to receive notifications from the
 * incoming email message or its envelope.  This is potentially quite
 * dangerous, as this can be used for Deny Of Service attacks on
 * recipients controlled by the message sender.  For this reason
 * implementations SHOULD NOT allow use of variables containing values
 * extracted from the email message in the method parameter to the
 * notify action.  Note that violation of this SHOULD NOT may result in
 * the creation of an open relay, i.e. any sender would be able to
 * create specially crafted email messages that would result in
 * notifications delivered to recipients under the control of the
 * sender.  In worst case this might result in financial loss by user
 * controlling the Sieve script and/or by recipients of notifications
 * (e.g. if a notification is an SMS message).
 *
 * --> This is currently not possible to check.
 */

/*
 * Notify capability
 */

static const char *ext_notify_get_methods_string
	(const struct sieve_extension *ntfy_ext);

const struct sieve_extension_capabilities notify_capabilities = {
	"notify",
	ext_notify_get_methods_string
};

/*
 * Core notification methods
 */

extern const struct sieve_enotify_method_def mailto_notify;

/*
 * Notify method registry
 */
 
static const struct sieve_enotify_method *ext_enotify_method_register
(struct sieve_instance *svinst, struct ext_enotify_context *ectx, 
	const struct sieve_enotify_method_def *nmth_def) 
{
	struct sieve_enotify_method *nmth;
	int nmth_id = (int) array_count(&ectx->notify_methods);

	nmth = array_append_space(&ectx->notify_methods);
	nmth->def = nmth_def;
	nmth->id = nmth_id;
	nmth->svinst = svinst;

	if ( nmth_def->load != NULL )
		nmth_def->load(nmth, &nmth->context);

	return nmth;
} 

void ext_enotify_methods_init
(struct sieve_instance *svinst, struct ext_enotify_context *ectx)
{
	p_array_init(&ectx->notify_methods, default_pool, 4);

	ext_enotify_method_register(svinst, ectx, &mailto_notify);
}

void ext_enotify_methods_deinit(struct ext_enotify_context *ectx)
{
	const struct sieve_enotify_method *methods;
	unsigned int meth_count, i;
	 
	methods = array_get(&ectx->notify_methods, &meth_count);
	for ( i = 0; i < meth_count; i++ ) {
		if ( methods[i].def != NULL && methods[i].def->unload != NULL )
			methods[i].def->unload(&methods[i]);
	}

	array_free(&ectx->notify_methods);
}

const struct sieve_enotify_method *sieve_enotify_method_register
(struct sieve_instance *svinst, 
	const struct sieve_enotify_method_def *nmth_def)
{
	const struct sieve_extension *ntfy_ext =
		sieve_extension_get_by_name(svinst, "enotify");

	if ( ntfy_ext != NULL ) {
		struct ext_enotify_context *ectx = 
			(struct ext_enotify_context *) ntfy_ext->context;

		return ext_enotify_method_register(svinst, ectx, nmth_def);
	}

	return NULL;
}

void sieve_enotify_method_unregister
(const struct sieve_enotify_method *nmth)
{
	struct sieve_instance *svinst = nmth->svinst;
	const struct sieve_extension *ntfy_ext =
		sieve_extension_get_by_name(svinst, "enotify");

	if ( ntfy_ext != NULL ) {
		struct ext_enotify_context *ectx = 
			(struct ext_enotify_context *) ntfy_ext->context;
		int nmth_id = nmth->id;

		if ( nmth_id >= 0 && nmth_id < (int)array_count(&ectx->notify_methods) ) {
			struct sieve_enotify_method *nmth_mod =
				array_idx_modifiable(&ectx->notify_methods, nmth_id);
			
			nmth_mod->def = NULL;
		}
	}
}

const struct sieve_enotify_method *ext_enotify_method_find
(const struct sieve_extension *ntfy_ext, const char *identifier) 
{
	struct ext_enotify_context *ectx = 
		(struct ext_enotify_context *) ntfy_ext->context;
	unsigned int meth_count, i;
	const struct sieve_enotify_method *methods;
	 
	methods = array_get(&ectx->notify_methods, &meth_count);
		
	for ( i = 0; i < meth_count; i++ ) {
		if ( methods[i].def == NULL ) continue;

		if ( strcasecmp(methods[i].def->identifier, identifier) == 0 ) {
			return &methods[i];
		}
	}
	
	return NULL;
}

static const char *ext_notify_get_methods_string
(const struct sieve_extension *ntfy_ext)
{
	struct ext_enotify_context *ectx = 
		(struct ext_enotify_context *) ntfy_ext->context;
	unsigned int meth_count, i;
	const struct sieve_enotify_method *methods;
	string_t *result = t_str_new(128);
	 
	methods = array_get(&ectx->notify_methods, &meth_count);
	
	if ( meth_count > 0 ) {
		for ( i = 0; i < meth_count; i++ ) {
			if ( str_len(result) > 0 )
				str_append_c(result, ' ');

			if ( methods[i].def != NULL )
				str_append(result, methods[i].def->identifier);
		}
		
		return str_c(result);
	}
	
	return NULL;
}

/*
 * Compile-time argument validation
 */
 
static const char *ext_enotify_uri_scheme_parse(const char **uri_p)
{
	string_t *scheme = t_str_new(EXT_ENOTIFY_MAX_SCHEME_LEN);
	const char *p = *uri_p;
	unsigned int len = 0;
	
	/* RFC 3968:
	 *
	 *   scheme  = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
	 *
	 * FIXME: we do not allow '%' in schemes. Is this correct?
	 */
	 
	if ( !i_isalpha(*p) )
		return NULL;
		
	str_append_c(scheme, *p);
	p++;
		
	while ( *p != '\0' && len < EXT_ENOTIFY_MAX_SCHEME_LEN ) {
			
		if ( !i_isalnum(*p) && *p != '+' && *p != '-' && *p != '.' )
			break;
	
		str_append_c(scheme, *p);
		p++;
		len++;
	}
	
	if ( *p != ':' )
		return NULL;
	p++;
	
	*uri_p = p;
	return str_c(scheme);
}

static bool ext_enotify_option_parse
(struct sieve_enotify_env *nenv, const char *option, bool name_only,
	const char **opt_name_r, const char **opt_value_r)
{
	const char *p = option;
	
	/* "<optionname>=<value>".
	 * 
	 * l-d = ALPHA / DIGIT
	 * l-d-p = l-d / "." / "-" / "_"
	 * optionname = l-d *l-d-p
	 * value = *(%x01-09 / %x0B-0C / %x0E-FF)
	 */
				
	/* 
	 * Parse option name 
	 *
	 * optionname = l-d *l-d-p
	 */
	
	/* Explicitly report empty option as such */
	if ( *p == '\0' ) {
		sieve_enotify_error(nenv, "empty option specified");
		return FALSE;
	}

	/* l-d = ALPHA / DIGIT */
	if ( i_isalnum(*p) ) {
		p++;
	
		/* l-d-p = l-d / "." / "-" / "_" */
		while ( i_isalnum(*p) || *p == '.' || *p == '-' || *p == '_' )
			p++;
	}
	
	/* Parsing must end at '=' and we must parse at least one character */
	if ( *p != '=' || p == option ) {
		sieve_enotify_error(nenv, "invalid option name specified in option '%s'",
				str_sanitize(option, 80));
		return FALSE;
	}
	
	/* Assign option name */
	if ( opt_name_r != NULL ) 
		*opt_name_r = t_strdup_until(option, p);
	
	/* Skip '=' */
	p++;
	
	/* Exit now if only the option name is of interest */
	if ( name_only )
		return TRUE;
			
	/* 
	 * Parse option value
	 */
	 
	/* value = *(%x01-09 / %x0B-0C / %x0E-FF) */
	while ( *p != '\0' && *p != 0x0A && *p != 0x0D )
		p++;
		
	/* Parse must end at end of string */
	if ( *p != '\0' ) {
		sieve_enotify_error(nenv, 
			"notify command: invalid option value specified in option '%s'",
				str_sanitize(option, 80));
		return FALSE;
	}
	
	/* Assign option value */
	if ( opt_value_r != NULL )
		*opt_value_r = p;
		
	return TRUE;
} 

struct _ext_enotify_option_check_context {
	struct sieve_instance *svinst;
	struct sieve_validator *valdtr;
	const struct sieve_enotify_method *method;
};

static int _ext_enotify_option_check
(void *context, struct sieve_ast_argument *arg)
{
	struct _ext_enotify_option_check_context *optn_context = 
		(struct _ext_enotify_option_check_context *) context;
	struct sieve_validator *valdtr = optn_context->valdtr;
	const struct sieve_enotify_method *method = optn_context->method;
	struct sieve_enotify_env nenv;
	const char *option = sieve_ast_argument_strc(arg);
	const char *opt_name = NULL, *opt_value = NULL;
	bool result = TRUE, check = TRUE;
	
	/* Compose log structure */
	memset(&nenv, 0, sizeof(nenv));
	nenv.svinst = optn_context->svinst;
	nenv.method = method;	
	nenv.ehandler = sieve_prefix_ehandler_create
		(sieve_validator_error_handler(valdtr), 
			sieve_error_script_location
				(sieve_validator_script(valdtr), arg->source_line), 
			"notify command");
		
	/* Parse option */
	if ( !sieve_argument_is_string_literal(arg) ) {
		/* Variable string: partial option parse
		 * 
		 * If the string item is not a string literal, it cannot be validated fully
		 * at compile time. We can however check whether the '=' is in the string
		 * specification and whether the part before the '=' is a valid option name.
		 * In that case, the method option check function is called with the value
		 * parameter equal to NULL, meaning that it should only check the validity
		 * of the option itself and not the assigned value.
		 */ 
		if ( !ext_enotify_option_parse(NULL, option, TRUE, &opt_name, &opt_value) )
			check = FALSE;
	} else {
		/* Literal string: full option parse */
		if ( !ext_enotify_option_parse
			(&nenv, option, FALSE, &opt_name, &opt_value) )
			result = FALSE;
	}
	
	/* Call method's option check function */
	if ( result && check && method->def != NULL && 
		method->def->compile_check_option != NULL ) 
		result = method->def->compile_check_option(&nenv, opt_name, opt_value); 
	
	sieve_error_handler_unref(&nenv.ehandler);

	return result;
}

bool ext_enotify_compile_check_arguments
(struct sieve_validator *valdtr, struct sieve_command *cmd,
	struct sieve_ast_argument *uri_arg, struct sieve_ast_argument *msg_arg, 
	struct sieve_ast_argument *from_arg, struct sieve_ast_argument *options_arg)
{
	const struct sieve_extension *this_ext = cmd->ext;
	struct sieve_instance *svinst = this_ext->svinst;
	const char *uri = sieve_ast_argument_strc(uri_arg);
	const char *scheme;
	const struct sieve_enotify_method *method;
	struct sieve_enotify_env nenv;
	bool result = TRUE;

	/* If the uri string is not a constant literal, we cannot determine which
	 * method is used, so we bail out successfully and defer checking to runtime.
	 */
	if ( !sieve_argument_is_string_literal(uri_arg) )
		return TRUE;
	
	/* Parse scheme part of URI */
	if ( (scheme=ext_enotify_uri_scheme_parse(&uri)) == NULL ) {
		sieve_argument_validate_error(valdtr, uri_arg, 
			"notify command: invalid scheme part for method URI '%s'", 
			str_sanitize(sieve_ast_argument_strc(uri_arg), 80));
		return FALSE;
	}
	
	/* Find used method with the parsed scheme identifier */
	if ( (method=ext_enotify_method_find(this_ext, scheme)) == NULL ) {
		sieve_argument_validate_error(valdtr, uri_arg, 
			"notify command: invalid method '%s'", scheme);
		return FALSE;
	}

	if ( method->def == NULL ) return TRUE;

	/* Compose log structure */
	memset(&nenv, 0, sizeof(nenv));
	nenv.svinst = svinst;
	nenv.method = method;	
	
	/* Check URI itself */
	if ( result && method->def->compile_check_uri != NULL ) {
		/* Set log location to location of URI argument */
		nenv.ehandler = sieve_prefix_ehandler_create
		(sieve_validator_error_handler(valdtr), 
			sieve_error_script_location
				(sieve_validator_script(valdtr), uri_arg->source_line), 
			"notify command");

		/* Execute method check function */
		result = method->def->compile_check_uri
			(&nenv, sieve_ast_argument_strc(uri_arg), uri);
	}

	/* Check :message argument */
	if ( result && msg_arg != NULL && sieve_argument_is_string_literal(msg_arg)
		&& method->def->compile_check_message != NULL ) {
		/* Set log location to location of :message argument */
		sieve_error_handler_unref(&nenv.ehandler);
		nenv.ehandler = sieve_prefix_ehandler_create
		(sieve_validator_error_handler(valdtr), 
			sieve_error_script_location
				(sieve_validator_script(valdtr), msg_arg->source_line), 
			"notify command");

		/* Execute method check function */
		result = method->def->compile_check_message
			(&nenv, sieve_ast_argument_str(msg_arg));
	}

	/* Check :from argument */
	if ( result && from_arg != NULL && sieve_argument_is_string_literal(from_arg)
		&& method->def->compile_check_from != NULL ) {
		/* Set log location to location of :from argument */
		sieve_error_handler_unref(&nenv.ehandler);
		nenv.ehandler = sieve_prefix_ehandler_create
		(sieve_validator_error_handler(valdtr), 
			sieve_error_script_location
				(sieve_validator_script(valdtr), from_arg->source_line), 
				"notify command");

		/* Execute method check function */
		result = method->def->compile_check_from
			(&nenv, sieve_ast_argument_str(from_arg));
	}

	sieve_error_handler_unref(&nenv.ehandler);
	
	/* Check :options argument */
	if ( result && options_arg != NULL ) {
		struct sieve_ast_argument *option = options_arg;
		struct _ext_enotify_option_check_context optn_context = 
			{ svinst, valdtr, method };
		
		/* Parse and check options */
		result = ( sieve_ast_stringlist_map
			(&option, (void *) &optn_context, _ext_enotify_option_check) > 0 );
			
		/* Discard argument if options are not accepted by method */
		if ( result && method->def->compile_check_option == NULL ) {
			sieve_argument_validate_warning(valdtr, options_arg, 
				"notify command: method '%s' accepts no options", scheme);
			(void)sieve_ast_arguments_detach(options_arg,1);
		}
	}
	
	return result;
}

/*
 * Runtime operand checking
 */
 
bool ext_enotify_runtime_method_validate
(const struct sieve_runtime_env *renv, string_t *method_uri)
{
	const struct sieve_extension *this_ext = renv->oprtn->ext;
	const struct sieve_enotify_method *method;
	const char *uri = str_c(method_uri);
	const char *scheme;
	bool result = TRUE;

	/* Get the method */

	if ( (scheme=ext_enotify_uri_scheme_parse(&uri)) == NULL )
		return FALSE;

	if ( (method=ext_enotify_method_find(this_ext, scheme)) == NULL )
		return FALSE;

	/* Validate the provided URI */

	if ( method->def != NULL && method->def->runtime_check_uri != NULL ) {
		struct sieve_enotify_env nenv;

		memset(&nenv, 0, sizeof(nenv));
		nenv.svinst = renv->svinst;
		nenv.method = method;
		nenv.ehandler = sieve_prefix_ehandler_create
			(sieve_interpreter_get_error_handler(renv->interp),
				sieve_runtime_get_full_command_location(renv),
				"valid_notify_method test");

		/* Use the method check function to validate the URI */
		result = method->def->runtime_check_uri(&nenv, str_c(method_uri), uri);

		sieve_error_handler_unref(&nenv.ehandler);
	}

	return result;
}

static const struct sieve_enotify_method *ext_enotify_get_method
(const struct sieve_runtime_env *renv, string_t *method_uri,
	const char **uri_body_r)
{
	const struct sieve_extension *this_ext = renv->oprtn->ext;
	const struct sieve_enotify_method *method;
	const char *uri = str_c(method_uri);
	const char *scheme;

	/* Parse part before ':' of the uri (the scheme) and use it to identify
	 * notify method.
	 */
	if ( (scheme=ext_enotify_uri_scheme_parse(&uri)) == NULL ) {
		sieve_runtime_error(renv, NULL,
			"invalid scheme part for method URI '%s'", 
			str_sanitize(str_c(method_uri), 80));
		return NULL;
	}

	/* Find the notify method */
	if ( (method=ext_enotify_method_find(this_ext, scheme)) == NULL ) {
		sieve_runtime_error(renv, NULL,
			"invalid notify method '%s'", scheme);
		return NULL;
	}

	/* Return the parse pointer and the found method */
	*uri_body_r = uri;
	return method;
}

const char *ext_enotify_runtime_get_method_capability
(const struct sieve_runtime_env *renv,
	string_t *method_uri, const char *capability)
{
	const struct sieve_enotify_method *method;
	const char *uri_body;
	const char *result = NULL;

	/* Get method */
	method = ext_enotify_get_method(renv, method_uri, &uri_body);
	if ( method == NULL ) return NULL;

	/* Get requested capability */
	if ( method->def != NULL && 
		method->def->runtime_get_method_capability != NULL ) {
		struct sieve_enotify_env nenv; 

		memset(&nenv, 0, sizeof(nenv));
		nenv.svinst = renv->svinst;
		nenv.method = method;
		nenv.ehandler = sieve_prefix_ehandler_create
			(sieve_interpreter_get_error_handler(renv->interp),
				sieve_runtime_get_full_command_location(renv),
				"notify_method_capability test");

		/* Execute method function to acquire capability value */
		result = method->def->runtime_get_method_capability
			(&nenv, str_c(method_uri), uri_body, capability);
		sieve_error_handler_unref(&nenv.ehandler);
	}

	return result;
}

int ext_enotify_runtime_check_operands
(const struct sieve_runtime_env *renv,
	string_t *method_uri, string_t *message, string_t *from, 
	struct sieve_stringlist *options, 
	const struct sieve_enotify_method **method_r, void **method_context)
{
	const struct sieve_enotify_method *method;
	const char *uri_body;

	/* Get method */
	method = ext_enotify_get_method(renv, method_uri, &uri_body);
	if ( method == NULL ) return SIEVE_EXEC_FAILURE;

	/* Check provided operands */
	if ( method->def != NULL && method->def->runtime_check_operands != NULL ) {
		struct sieve_enotify_env nenv; 
		int result = SIEVE_EXEC_OK;

		memset(&nenv, 0, sizeof(nenv));
		nenv.svinst = renv->svinst;
		nenv.method = method;
		nenv.ehandler = sieve_prefix_ehandler_create
			(sieve_interpreter_get_error_handler(renv->interp),
				sieve_runtime_get_full_command_location(renv),
				"notify action");

		/* Execute check function */
		if ( method->def->runtime_check_operands
			(&nenv, str_c(method_uri), uri_body, message, from, 
				sieve_result_pool(renv->result), method_context) ) {

			/* Check any provided options */
			if ( options != NULL ) {			
				string_t *option = NULL;
				int ret;
			
				/* Iterate through all provided options */
				while ( (ret=sieve_stringlist_next_item(options, &option)) > 0 ) {
					const char *opt_name = NULL, *opt_value = NULL;
				
					/* Parse option into <optionname> and <value> */
					if ( ext_enotify_option_parse
						(&nenv, str_c(option), FALSE, &opt_name, &opt_value) ) {
					
						/* Set option */
						if ( method->def->runtime_set_option != NULL ) {
							(void) method->def->runtime_set_option
								(&nenv, *method_context, opt_name, opt_value);
						}
					}
				}
			
				/* Check for binary corruptions encountered during string list iteration
				 */
				if ( ret >= 0 ) {
					*method_r = method;
				} else {
					/* Binary corrupt */
					sieve_runtime_trace_error
						(renv, "invalid item in options string list");
					result = SIEVE_EXEC_BIN_CORRUPT;
				}

			} else {
				/* No options */			
				*method_r = method;
			}

		} else { 	
			/* Operand check failed */
			result = SIEVE_EXEC_FAILURE;
		}

		sieve_error_handler_unref(&nenv.ehandler);
		return result;
	}

	/* No check function defined: a most unlikely situation */
	*method_context = NULL;	
	*method_r = method;
	return SIEVE_EXEC_OK;
}

/*
 * Notify method printing
 */

void sieve_enotify_method_printf
(const struct sieve_enotify_print_env *penv, const char *fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);	
	sieve_result_vprintf(penv->result_penv, fmt, args);
	va_end(args);	 
}

