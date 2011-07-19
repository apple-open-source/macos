/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str-sanitize.h"
#include "array.h"
#include "mail-storage.h"

#include "sieve-code.h"
#include "sieve-stringlist.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-result.h"
#include "sieve-validator.h" 
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-actions.h"
#include "sieve-dump.h"

#include "ext-imap4flags-common.h"

#include <ctype.h>

/* 
 * Flags tagged argument
 */

static bool tag_flags_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);
static bool tag_flags_validate_persistent
	(struct sieve_validator *valdtr, struct sieve_command *cmd,
		const struct sieve_extension *ext);
static bool tag_flags_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg,
		struct sieve_command *cmd);

const struct sieve_argument_def tag_flags = { 
	"flags", 
	NULL, 
	tag_flags_validate, 
	NULL, NULL,
	tag_flags_generate 
};

const struct sieve_argument_def tag_flags_implicit = { 
	"flags-implicit", 
	NULL,	NULL, NULL, 
	tag_flags_validate_persistent, 
	tag_flags_generate
};

/* 
 * Side effect 
 */

static bool seff_flags_dump_context
	(const struct sieve_side_effect *seffect,
    	const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int seff_flags_read_context
	(const struct sieve_side_effect *seffect, 
		const struct sieve_runtime_env *renv, sieve_size_t *address,
		void **context);

static int seff_flags_merge
	(const struct sieve_runtime_env *renv, const struct sieve_action *action, 
		const struct sieve_side_effect *old_seffect, 	
		const struct sieve_side_effect *new_seffect, void **old_context);

static void seff_flags_print
	(const struct sieve_side_effect *seffect, const struct sieve_action *action,
		const struct sieve_result_print_env *rpenv, bool *keep);
static bool seff_flags_pre_execute
	(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
		const struct sieve_action_exec_env *aenv, void **context, void *tr_context);

const struct sieve_side_effect_def flags_side_effect = {
	SIEVE_OBJECT("flags", &flags_side_effect_operand, 0),
	&act_store,

	seff_flags_dump_context,
	seff_flags_read_context,
	seff_flags_merge,
	seff_flags_print,
	seff_flags_pre_execute, 
	NULL, NULL, NULL
};

/*
 * Operand
 */

static const struct sieve_extension_objects ext_side_effects =
	SIEVE_EXT_DEFINE_SIDE_EFFECT(flags_side_effect);

const struct sieve_operand_def flags_side_effect_operand = { 
	"flags operand", 
	&imap4flags_extension,
	0, 
	&sieve_side_effect_operand_class,
	&ext_side_effects
};

/* 
 * Tag validation 
 */

static bool tag_flags_validate_persistent
(struct sieve_validator *valdtr ATTR_UNUSED, struct sieve_command *cmd,
	const struct sieve_extension *ext)
{
	if ( sieve_command_find_argument(cmd, &tag_flags) == NULL ) {
		sieve_command_add_dynamic_tag(cmd, ext, &tag_flags_implicit, -1);
	}
	
	return TRUE;
}

static bool tag_flags_validate
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd)
{
	struct sieve_ast_argument *tag = *arg;

	/* Detach the tag itself */
	*arg = sieve_ast_argument_next(*arg);
	
	/* Check syntax:
	 *   :flags <list-of-flags: string-list>
	 */
	if ( !sieve_validate_tag_parameter
		(valdtr, cmd, tag, *arg, NULL, 0, SAAT_STRING_LIST, FALSE) ) {
		return FALSE;
	}
	
	tag->parameters = *arg;
	
	/* Detach parameter */
	*arg = sieve_ast_arguments_detach(*arg,1);

	return TRUE;
}

/* 
 * Code generation 
 */

static bool tag_flags_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg,
	struct sieve_command *cmd)
{
	struct sieve_ast_argument *param;

	if ( sieve_ast_argument_type(arg) != SAAT_TAG ) {
		return FALSE;
	}

	sieve_opr_side_effect_emit
		(cgenv->sblock, arg->argument->ext, &flags_side_effect);

	if ( sieve_argument_is(arg, tag_flags) ) {
		/* Explicit :flags tag */
		param = arg->parameters;

		/* Call the generation function for the argument */ 
		if ( param->argument != NULL && param->argument->def != NULL && 
			param->argument->def->generate != NULL && 
			!param->argument->def->generate(cgenv, param, cmd) ) 
			return FALSE;

	} else if ( sieve_argument_is(arg, tag_flags_implicit) ) {
		/* Implicit flags */
		sieve_opr_omitted_emit(cgenv->sblock);
	
	} else {
		/* Something else?! */
		i_unreached();
	}
	
	return TRUE;
}

/* 
 * Side effect implementation
 */
 
/* Context data */

struct seff_flags_context {
	ARRAY_DEFINE(keywords, const char *);
	enum mail_flags flags;
};

/* Context coding */

static bool seff_flags_dump_context
(const struct sieve_side_effect *seffect ATTR_UNUSED, 
	const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
  struct sieve_operand oprnd;

  if ( !sieve_operand_read(denv->sblock, address, "flags", &oprnd) ) {
		sieve_code_dumpf(denv, "ERROR: INVALID OPERAND");
		return FALSE;
	}

	if ( sieve_operand_is_omitted(&oprnd) ) {
		sieve_code_dumpf(denv, "flags: INTERNAL");
		return TRUE;
	}

	return sieve_opr_stringlist_dump_data(denv, &oprnd, address, "flags");
}

static struct seff_flags_context *seff_flags_get_implicit_context
(const struct sieve_extension *this_ext, struct sieve_result *result)
{
	pool_t pool = sieve_result_pool(result);
	struct seff_flags_context *ctx;
	const char *flag;
	struct ext_imap4flags_iter flit;
	
	ctx = p_new(pool, struct seff_flags_context, 1);
	p_array_init(&ctx->keywords, pool, 2);
	
	T_BEGIN {
		
		/* Unpack */
		ext_imap4flags_get_implicit_flags_init(&flit, this_ext, result);
		while ( (flag=ext_imap4flags_iter_get_flag(&flit)) != NULL ) {		
			if (flag != NULL && *flag != '\\') {
				/* keyword */
				const char *keyword = p_strdup(pool, flag);
				array_append(&ctx->keywords, &keyword, 1);
			} else {
				/* system flag */
				if (flag == NULL || strcasecmp(flag, "\\flagged") == 0)
					ctx->flags |= MAIL_FLAGGED;
				else if (strcasecmp(flag, "\\answered") == 0)
					ctx->flags |= MAIL_ANSWERED;
				else if (strcasecmp(flag, "\\deleted") == 0)
					ctx->flags |= MAIL_DELETED;
				else if (strcasecmp(flag, "\\seen") == 0)
					ctx->flags |= MAIL_SEEN;
				else if (strcasecmp(flag, "\\draft") == 0)
					ctx->flags |= MAIL_DRAFT;
			}
		}

	} T_END;
	
	return ctx;
}

static int seff_flags_read_context
(const struct sieve_side_effect *seffect, 
	const struct sieve_runtime_env *renv, sieve_size_t *address,
	void **se_context)
{
	struct sieve_operand oprnd;
	pool_t pool = sieve_result_pool(renv->result);
	struct seff_flags_context *ctx;
	string_t *flags_item;
	struct sieve_stringlist *flag_list;
	int ret;
	
	t_push();

	/* Check whether explicit flag list operand is present */
	if ( (ret=sieve_operand_runtime_read(renv, address, "flags", &oprnd)) <= 0 ) {
 		t_pop();
		return ret;
	}
	
	if ( sieve_operand_is_omitted(&oprnd) ) {
		/* Flag list is omitted, use current value of internal 
		 * variable to construct side effect context.
		 */
		*se_context = seff_flags_get_implicit_context
			(SIEVE_OBJECT_EXTENSION(seffect), renv->result);
		t_pop();
		return SIEVE_EXEC_OK;
	}
	
	/* Read flag-list */
	if ( (ret=sieve_opr_stringlist_read_data
		(renv, &oprnd, address, NULL, &flag_list)) <= 0 ) {
		t_pop();
		return ret;
	}
	
	ctx = p_new(pool, struct seff_flags_context, 1);
	p_array_init(&ctx->keywords, pool, 2);

	/* Unpack */
	flags_item = NULL;
	while ( (ret=sieve_stringlist_next_item(flag_list, &flags_item)) > 0 ) {
		const char *flag;
		struct ext_imap4flags_iter flit;

		ext_imap4flags_iter_init(&flit, flags_item);
	
		while ( (flag=ext_imap4flags_iter_get_flag(&flit)) != NULL ) {		
			if (flag != NULL && *flag != '\\') {
				/* keyword */
				const char *keyword = p_strdup(pool, flag);

				/* FIXME: should check for duplicates (cannot trust variables) */
				array_append(&ctx->keywords, &keyword, 1);

			} else {
				/* system flag */
				if (flag == NULL || strcasecmp(flag, "\\flagged") == 0)
					ctx->flags |= MAIL_FLAGGED;
				else if (strcasecmp(flag, "\\answered") == 0)
					ctx->flags |= MAIL_ANSWERED;
				else if (strcasecmp(flag, "\\deleted") == 0)
					ctx->flags |= MAIL_DELETED;
				else if (strcasecmp(flag, "\\seen") == 0)
					ctx->flags |= MAIL_SEEN;
				else if (strcasecmp(flag, "\\draft") == 0)
					ctx->flags |= MAIL_DRAFT;
			}
		}
	}
	
	*se_context = (void *) ctx;

	t_pop();
	
	return SIEVE_EXEC_OK;
}

/* Result verification */

static int seff_flags_merge
(const struct sieve_runtime_env *renv ATTR_UNUSED, 
	const struct sieve_action *action ATTR_UNUSED, 
	const struct sieve_side_effect *old_seffect ATTR_UNUSED, 	
	const struct sieve_side_effect *new_seffect, 	
	void **old_context)
{
	if ( new_seffect != NULL )
		*old_context = new_seffect->context;
	
	return 1;
}

/* Result printing */

static void seff_flags_print
(const struct sieve_side_effect *seffect, 
	const struct sieve_action *action ATTR_UNUSED, 
	const struct sieve_result_print_env *rpenv,bool *keep ATTR_UNUSED)
{
	struct sieve_result *result = rpenv->result;
	struct seff_flags_context *ctx = 
		(struct seff_flags_context *) seffect->context;
	unsigned int i;
	
	if ( ctx == NULL )
		ctx = seff_flags_get_implicit_context
			(SIEVE_OBJECT_EXTENSION(seffect), result);
	
	if ( ctx->flags != 0 || array_count(&ctx->keywords) > 0 ) {
		T_BEGIN {
			string_t *flags = t_str_new(128);
 
			if ( (ctx->flags & MAIL_FLAGGED) > 0 )
				str_printfa(flags, " \\flagged");

			if ( (ctx->flags & MAIL_ANSWERED) > 0 )
				str_printfa(flags, " \\answered");
		
			if ( (ctx->flags & MAIL_DELETED) > 0 )
				str_printfa(flags, " \\deleted");
					
			if ( (ctx->flags & MAIL_SEEN) > 0 )
				str_printfa(flags, " \\seen");
			
			if ( (ctx->flags & MAIL_DRAFT) > 0 )
				str_printfa(flags, " \\draft");

			for ( i = 0; i < array_count(&ctx->keywords); i++ ) {
				const char *const *keyword = array_idx(&ctx->keywords, i);
				str_printfa(flags, " %s", str_sanitize(*keyword, 64));
			}

			sieve_result_seffect_printf(rpenv, "add IMAP flags:%s", str_c(flags));
		} T_END;
	}
}

/* Result execution */

static bool seff_flags_pre_execute
(const struct sieve_side_effect *seffect, 
	const struct sieve_action *action ATTR_UNUSED,
	const struct sieve_action_exec_env *aenv, void **context, void *tr_context)
{	
	struct seff_flags_context *ctx = (struct seff_flags_context *) *context;
	const char *const *keywords;
		
	if ( ctx == NULL ) {
		ctx = seff_flags_get_implicit_context
			(SIEVE_OBJECT_EXTENSION(seffect), aenv->result);
		*context = (void *) ctx;
	}
		
	(void)array_append_space(&ctx->keywords);
	keywords = array_idx(&ctx->keywords, 0);

	sieve_act_store_add_flags(aenv, tr_context, keywords, ctx->flags);
	
	return TRUE;
}


