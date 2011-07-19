/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "str-sanitize.h"

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-stringlist.h"
#include "sieve-actions.h"
#include "sieve-validator.h" 
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-result.h"
#include "sieve-dump.h"

#include "sieve-ext-variables.h"

#include "ext-imap4flags-common.h"

/*
 * Forward declarations
 */

static bool flag_is_valid(const char *flag);

/* 
 * Tagged arguments 
 */

extern const struct sieve_argument_def tag_flags;
extern const struct sieve_argument_def tag_flags_implicit;

/* 
 * Common command functions 
 */

bool ext_imap4flags_command_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd)
{
	struct sieve_ast_argument *arg = cmd->first_positional;
	struct sieve_ast_argument *arg2;
	const struct sieve_extension *var_ext;
	
	/* Check arguments */
	
	if ( arg == NULL ) {
		sieve_command_validate_error(valdtr, cmd, 
			"the %s %s expects at least one argument, but none was found", 
			sieve_command_identifier(cmd), sieve_command_type_name(cmd));
		return FALSE;
	}
	
	if ( sieve_ast_argument_type(arg) != SAAT_STRING && 
		sieve_ast_argument_type(arg) != SAAT_STRING_LIST ) 
	{
		sieve_argument_validate_error(valdtr, arg, 
			"the %s %s expects either a string (variable name) or "
			"a string-list (list of flags) as first argument, but %s was found", 
			sieve_command_identifier(cmd), sieve_command_type_name(cmd),
			sieve_ast_argument_name(arg));
		return FALSE; 
	}

	arg2 = sieve_ast_argument_next(arg);
	if ( arg2 != NULL ) {		
		/* First, check syntax sanity */
				
		if ( sieve_ast_argument_type(arg) != SAAT_STRING ) 
		{
			if ( sieve_command_is(cmd, tst_hasflag) ) {
				if ( sieve_ast_argument_type(arg) != SAAT_STRING_LIST ) {
					sieve_argument_validate_error(valdtr, arg, 
						"if a second argument is specified for the hasflag, the first "
						"must be a string-list (variable-list), but %s was found",
						sieve_ast_argument_name(arg));
					return FALSE;
				}
			} else {
				sieve_argument_validate_error(valdtr, arg, 
					"if a second argument is specified for the %s %s, the first "
					"must be a string (variable name), but %s was found",
					sieve_command_identifier(cmd), sieve_command_type_name(cmd), 
					sieve_ast_argument_name(arg));
				return FALSE; 
			}
		}
		
		/* Then, check whether the second argument is permitted */
		
		var_ext = sieve_ext_variables_get_extension(cmd->ext->svinst);

		if ( var_ext == NULL || !sieve_ext_variables_is_active(var_ext, valdtr) )	
			{
			sieve_argument_validate_error(valdtr,arg, 
				"the %s %s only allows for the specification of a "
				"variable name when the variables extension is active",
				sieve_command_identifier(cmd), sieve_command_type_name(cmd));
			return FALSE;
		}		
		
		if ( !sieve_variable_argument_activate
			(var_ext, valdtr, cmd, arg, !sieve_command_is(cmd, tst_hasflag) ) )
			return FALSE;
		
		if ( sieve_ast_argument_type(arg2) != SAAT_STRING && 
			sieve_ast_argument_type(arg2) != SAAT_STRING_LIST ) 
		{
			sieve_argument_validate_error(valdtr, arg2, 
				"the %s %s expects a string list (list of flags) as "
				"second argument when two arguments are specified, "
				"but %s was found",
				sieve_command_identifier(cmd), sieve_command_type_name(cmd),
				sieve_ast_argument_name(arg2));
			return FALSE; 
		}
	} else
		arg2 = arg;

	if ( !sieve_validator_argument_activate(valdtr, cmd, arg2, FALSE) )
		return FALSE;

	if ( !sieve_command_is(cmd, tst_hasflag) && 
		sieve_argument_is_string_literal(arg2) ) {
		struct ext_imap4flags_iter fiter;
		const char *flag;
		
		/* Warn the user about validity of verifiable flags */
		ext_imap4flags_iter_init(&fiter, sieve_ast_argument_str(arg));

		while ( (flag=ext_imap4flags_iter_get_flag(&fiter)) != NULL ) {
			if ( !flag_is_valid(flag) ) {
				sieve_argument_validate_warning(valdtr, arg,
                	"IMAP flag '%s' specified for the %s command is invalid "
					"and will be ignored (only first invalid is reported)",					
					str_sanitize(flag, 64), sieve_command_identifier(cmd));
				break;
			}
		}
	}

	return TRUE;
}

/* 
 * Flags tag registration 
 */

void ext_imap4flags_attach_flags_tag
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	const char *command, bool implicit)
{
	/* Register :flags tag with the command and we don't care whether it is 
	 * registered or even whether it will be registered at all. The validator 
	 * handles either situation gracefully 
	 */
	 
	if ( !implicit ) {
		/* Tag specified by user */
		sieve_validator_register_external_tag
			(valdtr, command, ext, &tag_flags, SIEVE_OPT_SIDE_EFFECT);
	}

    /* Implicit tag if none is specified */
	sieve_validator_register_persistent_tag
		(valdtr, command, ext, &tag_flags_implicit);
}

/* 
 * Result context 
 */

struct ext_imap4flags_result_context {
    string_t *internal_flags;
};

static void _get_initial_flags
(struct sieve_result *result, string_t *flags)
{
	const struct sieve_message_data *msgdata = 
		sieve_result_get_message_data(result);
	enum mail_flags mail_flags;
	const char *const *mail_keywords;

	mail_flags = mail_get_flags(msgdata->mail);
	mail_keywords = mail_get_keywords(msgdata->mail);	

	if ( (mail_flags & MAIL_FLAGGED) > 0 )
		str_printfa(flags, " \\flagged");

	if ( (mail_flags & MAIL_ANSWERED) > 0 )
		str_printfa(flags, " \\answered");

	if ( (mail_flags & MAIL_DELETED) > 0 )
		str_printfa(flags, " \\deleted");

	if ( (mail_flags & MAIL_SEEN) > 0 )
		str_printfa(flags, " \\seen");

	if ( (mail_flags & MAIL_DRAFT) > 0 )
		str_printfa(flags, " \\draft");

	while ( *mail_keywords != NULL ) {
		str_printfa(flags, " %s", *mail_keywords);
		mail_keywords++;
	}	
}

static inline struct ext_imap4flags_result_context *_get_result_context
(const struct sieve_extension *this_ext, struct sieve_result *result)
{
	struct ext_imap4flags_result_context *rctx =
		(struct ext_imap4flags_result_context *) 
		sieve_result_extension_get_context(result, this_ext);

	if ( rctx == NULL ) {
		pool_t pool = sieve_result_pool(result);

		rctx =p_new(pool, struct ext_imap4flags_result_context, 1);
		rctx->internal_flags = str_new(pool, 32);
		_get_initial_flags(result, rctx->internal_flags);

		sieve_result_extension_set_context
			(result, this_ext, rctx);
	}

	return rctx;
}

static string_t *_get_flags_string
(const struct sieve_extension *this_ext, struct sieve_result *result)
{
	struct ext_imap4flags_result_context *ctx = 
		_get_result_context(this_ext, result);
		
	return ctx->internal_flags;
}

/* 
 * Runtime initialization 
 */

static void ext_imap4flags_runtime_init
(const struct sieve_extension *ext, const struct sieve_runtime_env *renv, 
	void *context ATTR_UNUSED)
{	
	sieve_result_add_implicit_side_effect
		(renv->result, NULL, TRUE, ext, &flags_side_effect, NULL);
}

const struct sieve_interpreter_extension imap4flags_interpreter_extension = {
	&imap4flags_extension,
	ext_imap4flags_runtime_init,
	NULL,
};

/* 
 * Flag handling
 */

/* FIXME: This currently accepts a potentially unlimited number of 
 * flags, making the internal or variable flag list indefinitely long
 */

static bool flag_is_valid(const char *flag)
{
	if (*flag == '\\') {
		/* System flag */
		const char *atom = t_str_ucase(flag);

		if (
			(strcmp(atom, "\\ANSWERED") != 0) &&
			(strcmp(atom, "\\FLAGGED") != 0) &&
			(strcmp(atom, "\\DELETED") != 0) &&
			(strcmp(atom, "\\SEEN") != 0) &&
			(strcmp(atom, "\\DRAFT") != 0) )
		{
			return FALSE;
		}
	} else {
		const char *p;

		/* Custom keyword:
		 *
		 * Syntax (IMAP4rev1, RFC 3501, Section 9. Formal Syntax) :
		 *  flag-keyword    = atom
		 *  atom            = 1*ATOM-CHAR
		 *  ATOM-CHAR       = <any CHAR except atom-specials>
		 *  atom-specials   = "(" / ")" / "{" / SP / CTL / list-wildcards /
		 *                    quoted-specials / resp-specials
		 *  CTL             =  %x00-1F / %x7F
		 *  list-wildcards  = "%" / "*"
		 *  quoted-specials = DQUOTE / "\"
		 *  resp-specials   = "]"
		 */

		p = flag;
		while ( *p != '\0' ) {
			if ( *p == '(' || *p == ')' || *p == '{' || *p == ' ' ||
				*p <= 0x1F || *p == 0x7F || *p == '%' || *p ==  '*' ||
				*p == '"' || *p == '\\' || *p == ']' )
				return FALSE;
			p++;
		}
	}

	return TRUE;
}

/* Flag iterator */

static void ext_imap4flags_iter_clear
(struct ext_imap4flags_iter *iter)
{
	memset(iter, 0, sizeof(*iter));
} 

void ext_imap4flags_iter_init
(struct ext_imap4flags_iter *iter, string_t *flags_list) 
{
	ext_imap4flags_iter_clear(iter);
	iter->flags_list = flags_list;
}

static string_t *ext_imap4flags_iter_get_flag_str
(struct ext_imap4flags_iter *iter) 
{
	unsigned int len;
	const unsigned char *fp;
	const unsigned char *fbegin;
	const unsigned char *fstart;
	const unsigned char *fend;

	/* Return if not initialized */
	if ( iter->flags_list == NULL ) return NULL;

	/* Return if no more flags are available */	
	len = str_len(iter->flags_list);
	if ( iter->offset >= len ) return NULL;
	
	/* Mark string boundries */
	fbegin = str_data(iter->flags_list);
	fend = fbegin + len;

	/* Start of this flag */
	fstart = fbegin + iter->offset;

	/* Scan for next flag */
	fp = fstart;
	for (;;) {
		/* Have we reached the end or a flag boundary? */
		if ( fp >= fend || *fp == ' ' ) {
			/* Did we scan more than nothing ? */
			if ( fp > fstart ) {
				/* Return flag */
				string_t *flag = t_str_new(fp-fstart+1);
				str_append_n(flag, fstart, fp-fstart);
				
				iter->last = fstart - fbegin;
				iter->offset = fp - fbegin;

				return flag;
			} 	
			
			fstart = fp + 1;
		}
		
		if ( fp >= fend ) break;
				
		fp++;
	}
	
	iter->last = fstart - fbegin;
	iter->offset = fp - fbegin;
	return NULL;
}

const char *ext_imap4flags_iter_get_flag
(struct ext_imap4flags_iter *iter)
{
	string_t *flag = ext_imap4flags_iter_get_flag_str(iter);

	if ( flag == NULL ) return NULL;

	return str_c(flag);
}

static void ext_imap4flags_iter_delete_last
(struct ext_imap4flags_iter *iter) 
{
	iter->offset++;
	if ( iter->offset > str_len(iter->flags_list) )
		iter->offset = str_len(iter->flags_list);
	if ( iter->offset == str_len(iter->flags_list) && iter->last > 0 )
		iter->last--;

	str_delete(iter->flags_list, iter->last, iter->offset - iter->last);	
	
	iter->offset = iter->last;
}

/* Flag operations */

static bool flags_list_flag_exists
(string_t *flags_list, const char *flag)
{
	const char *flg;
	struct ext_imap4flags_iter flit;
		
	ext_imap4flags_iter_init(&flit, flags_list);
	
	while ( (flg=ext_imap4flags_iter_get_flag(&flit)) != NULL ) {
		if ( strcasecmp(flg, flag) == 0 ) 
			return TRUE; 	
	}
	
	return FALSE;
}

static void flags_list_flag_delete
(string_t *flags_list, const char *flag)
{
	const char *flg;
	struct ext_imap4flags_iter flit;
		
	ext_imap4flags_iter_init(&flit, flags_list);
	
	while ( (flg=ext_imap4flags_iter_get_flag(&flit)) != NULL ) {
		if ( strcasecmp(flg, flag) == 0 ) {
			ext_imap4flags_iter_delete_last(&flit);
		} 	
	}
}
 			
static void flags_list_add_flags
(string_t *flags_list, string_t *flags)
{	
	const char *flg;
	struct ext_imap4flags_iter flit;
		
	ext_imap4flags_iter_init(&flit, flags);
	
	while ( (flg=ext_imap4flags_iter_get_flag(&flit)) != NULL ) {
		if ( flag_is_valid(flg) && !flags_list_flag_exists(flags_list, flg) ) {
			if ( str_len(flags_list) != 0 ) 
				str_append_c(flags_list, ' '); 
			str_append(flags_list, flg);
		} 	
	}
}

static void flags_list_remove_flags
(string_t *flags_list, string_t *flags)
{	
	const char *flg;
	struct ext_imap4flags_iter flit;
		
	ext_imap4flags_iter_init(&flit, flags);
	
	while ( (flg=ext_imap4flags_iter_get_flag(&flit)) != NULL ) {
		flags_list_flag_delete(flags_list, flg); 	
	}
}

static void flags_list_set_flags
(string_t *flags_list, string_t *flags)
{
	str_truncate(flags_list, 0);
	flags_list_add_flags(flags_list, flags);
}

static void flags_list_clear_flags
(string_t *flags_list)
{
	str_truncate(flags_list, 0);
}

static string_t *ext_imap4flags_get_flag_variable
(const struct sieve_runtime_env *renv, struct sieve_variable_storage *storage,
	unsigned int var_index)
{
	string_t *flags;
	
	if ( storage != NULL ) {
		if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_COMMANDS) ) {
			const char *var_name, *var_id;
	
			(void)sieve_variable_get_identifier(storage, var_index, &var_name);
			var_id = sieve_variable_get_varid(storage, var_index);

			sieve_runtime_trace(renv, 0, "update variable `%s' [%s]", 
				var_name, var_id);
		}

		if ( !sieve_variable_get_modifiable(storage, var_index, &flags) )
			return NULL;
	} else {
		flags = _get_flags_string(renv->oprtn->ext, renv->result);
	}

	return flags;
}

int ext_imap4flags_set_flags
(const struct sieve_runtime_env *renv, struct sieve_variable_storage *storage,
	unsigned int var_index, struct sieve_stringlist *flags)
{
	string_t *cur_flags = ext_imap4flags_get_flag_variable
		(renv, storage, var_index);
		
	if ( cur_flags != NULL ) {
		string_t *flags_item;
		int ret;

		flags_list_clear_flags(cur_flags);
		while ( (ret=sieve_stringlist_next_item(flags, &flags_item)) > 0 ) {
			sieve_runtime_trace(renv, SIEVE_TRLVL_COMMANDS, 
				"set flags `%s'", str_c(flags_item)); 

			flags_list_add_flags(cur_flags, flags_item);
		}

		if ( ret < 0 ) return SIEVE_EXEC_BIN_CORRUPT;
	
		return SIEVE_EXEC_OK;
	}

	return SIEVE_EXEC_BIN_CORRUPT;
}

int ext_imap4flags_add_flags
(const struct sieve_runtime_env *renv, struct sieve_variable_storage *storage, 
	unsigned int var_index, struct sieve_stringlist *flags)
{
	string_t *cur_flags = ext_imap4flags_get_flag_variable
		(renv, storage, var_index);
		
	if ( cur_flags != NULL ) {
		string_t *flags_item;
		int ret;

		while ( (ret=sieve_stringlist_next_item(flags, &flags_item)) > 0 ) {
			sieve_runtime_trace(renv, SIEVE_TRLVL_COMMANDS, 
				"add flags `%s'", str_c(flags_item)); 

			flags_list_add_flags(cur_flags, flags_item);
		}

		if ( ret < 0 ) return SIEVE_EXEC_BIN_CORRUPT;
	
		return SIEVE_EXEC_OK;
	}

	return SIEVE_EXEC_BIN_CORRUPT;
}

int ext_imap4flags_remove_flags
(const struct sieve_runtime_env *renv, struct sieve_variable_storage *storage, 
	unsigned int var_index, struct sieve_stringlist *flags)
{
	string_t *cur_flags = ext_imap4flags_get_flag_variable
		(renv, storage, var_index);
		
	if ( cur_flags != NULL ) {
		string_t *flags_item;
		int ret;

		while ( (ret=sieve_stringlist_next_item(flags, &flags_item)) > 0 ) {
			sieve_runtime_trace(renv, SIEVE_TRLVL_COMMANDS, 
				"remove flags `%s'", str_c(flags_item)); 

			flags_list_remove_flags(cur_flags, flags_item);
		}

		if ( ret < 0 ) return SIEVE_EXEC_BIN_CORRUPT;
	
		return SIEVE_EXEC_OK;
	}

	return SIEVE_EXEC_BIN_CORRUPT;
}

/* Flag stringlist */

static int ext_imap4flags_stringlist_next_item
	(struct sieve_stringlist *_strlist, string_t **str_r);
static void ext_imap4flags_stringlist_reset
	(struct sieve_stringlist *_strlist);

struct ext_imap4flags_stringlist {
	struct sieve_stringlist strlist;

	struct sieve_stringlist *flags_list;
	string_t *flags_string;
	struct ext_imap4flags_iter flit;

	unsigned int normalize:1;
};

static struct sieve_stringlist *ext_imap4flags_stringlist_create
(const struct sieve_runtime_env *renv, struct sieve_stringlist *flags_list,
	bool normalize)
{
	struct ext_imap4flags_stringlist *strlist;

	strlist = t_new(struct ext_imap4flags_stringlist, 1);
	strlist->strlist.exec_status = SIEVE_EXEC_OK;
	strlist->strlist.runenv = renv;
	strlist->strlist.next_item = ext_imap4flags_stringlist_next_item;
	strlist->strlist.reset = ext_imap4flags_stringlist_reset;
	strlist->normalize = normalize;

	strlist->flags_list = flags_list;

	return &strlist->strlist;
}

static struct sieve_stringlist *ext_imap4flags_stringlist_create_single
(const struct sieve_runtime_env *renv, string_t *flags_string, bool normalize)
{
	struct ext_imap4flags_stringlist *strlist;

	strlist = t_new(struct ext_imap4flags_stringlist, 1);
	strlist->strlist.exec_status = SIEVE_EXEC_OK;
	strlist->strlist.runenv = renv;
	strlist->strlist.next_item = ext_imap4flags_stringlist_next_item;
	strlist->strlist.reset = ext_imap4flags_stringlist_reset;
	strlist->normalize = normalize;

	if ( normalize ) {
		strlist->flags_string = t_str_new(256);
		flags_list_set_flags(strlist->flags_string, flags_string);
	} else {
		strlist->flags_string = flags_string;
	}

	ext_imap4flags_iter_init(&strlist->flit, strlist->flags_string);

	return &strlist->strlist;
}

static int ext_imap4flags_stringlist_next_item
(struct sieve_stringlist *_strlist, string_t **str_r)
{
	struct ext_imap4flags_stringlist *strlist = 
		(struct ext_imap4flags_stringlist *)_strlist;
	
	while ( (*str_r=ext_imap4flags_iter_get_flag_str(&strlist->flit)) == NULL ) {
		int ret;

		if ( strlist->flags_list == NULL )
			return 0;

		if ( (ret=sieve_stringlist_next_item
			(strlist->flags_list, &strlist->flags_string)) <= 0 )
			return ret;

		if ( strlist->flags_string == NULL )
			return -1; 

		if ( strlist->normalize ) {
			string_t *flags_string = t_str_new(256);

			flags_list_set_flags(flags_string, strlist->flags_string);
			strlist->flags_string = flags_string;
		}

		ext_imap4flags_iter_init(&strlist->flit, strlist->flags_string);
	}

	return 1;
}

static void ext_imap4flags_stringlist_reset
(struct sieve_stringlist *_strlist)
{
	struct ext_imap4flags_stringlist *strlist = 
		(struct ext_imap4flags_stringlist *)_strlist;

	if ( strlist->flags_list != NULL ) {
		sieve_stringlist_reset(strlist->flags_list);
		ext_imap4flags_iter_clear(&strlist->flit);
	} else {
		ext_imap4flags_iter_init(&strlist->flit, strlist->flags_string);
	}
}

/* Flag access */

struct sieve_stringlist *ext_imap4flags_get_flags
(const struct sieve_runtime_env *renv, struct sieve_stringlist *flags_list)
{
	if ( flags_list == NULL )
		return ext_imap4flags_stringlist_create_single
			(renv, _get_flags_string(renv->oprtn->ext, renv->result), FALSE);

	return ext_imap4flags_stringlist_create(renv, flags_list, TRUE);
}

void ext_imap4flags_get_implicit_flags_init
(struct ext_imap4flags_iter *iter, const struct sieve_extension *this_ext,
	struct sieve_result *result)
{
	string_t *cur_flags = _get_flags_string(this_ext, result);
	
	ext_imap4flags_iter_init(iter, cur_flags);		
}


	
	

