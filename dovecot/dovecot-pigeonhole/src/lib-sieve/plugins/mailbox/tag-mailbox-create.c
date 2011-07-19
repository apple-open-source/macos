/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "mail-storage.h"
#include "mail-namespace.h"

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-actions.h"
#include "sieve-code.h"
#include "sieve-actions.h"
#include "sieve-result.h"
#include "sieve-generator.h"

#include "ext-mailbox-common.h"

/* 
 * Tagged argument 
 */

static bool tag_mailbox_create_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);
static bool tag_mailbox_create_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg,
    struct sieve_command *context);

const struct sieve_argument_def mailbox_create_tag = { 
	"create", 
	NULL,
	tag_mailbox_create_validate, 
	NULL, NULL,
	tag_mailbox_create_generate
};

/*
 * Side effect 
 */

static void seff_mailbox_create_print
	(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
		const struct sieve_result_print_env *rpenv, bool *keep);
static bool seff_mailbox_create_pre_execute
	(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
		const struct sieve_action_exec_env *aenv, void **se_context, 
		void *tr_context);

const struct sieve_side_effect_def mailbox_create_side_effect = {
	SIEVE_OBJECT("create", &mailbox_create_operand, 0),
	&act_store,
	NULL, NULL, NULL,
	seff_mailbox_create_print,
	seff_mailbox_create_pre_execute, 
	NULL, NULL, NULL
};

/*
 * Operand
 */

static const struct sieve_extension_objects ext_side_effects =
	SIEVE_EXT_DEFINE_SIDE_EFFECT(mailbox_create_side_effect);

const struct sieve_operand_def mailbox_create_operand = {
	"create operand",
	&mailbox_extension,
	0,
	&sieve_side_effect_operand_class,
	&ext_side_effects
};

/* 
 * Tag validation 
 */

static bool tag_mailbox_create_validate
(struct sieve_validator *valdtr ATTR_UNUSED, 
	struct sieve_ast_argument **arg, struct sieve_command *cmd ATTR_UNUSED)
{
	*arg = sieve_ast_argument_next(*arg);

	return TRUE;
}

/*
 * Code generation 
 */

static bool tag_mailbox_create_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg,
	struct sieve_command *context ATTR_UNUSED)
{
	if ( sieve_ast_argument_type(arg) != SAAT_TAG ) {
		return FALSE;
	}

	sieve_opr_side_effect_emit
		(cgenv->sblock, arg->argument->ext, &mailbox_create_side_effect);

	return TRUE;
}

/* 
 * Side effect implementation
 */

static void seff_mailbox_create_print
(const struct sieve_side_effect *seffect ATTR_UNUSED, 
	const struct sieve_action *action ATTR_UNUSED, 
	const struct sieve_result_print_env *rpenv, bool *keep ATTR_UNUSED)
{
	sieve_result_seffect_printf(rpenv, "create mailbox if it does not exist");
}

static bool seff_mailbox_create_pre_execute
(const struct sieve_side_effect *seffect ATTR_UNUSED, 
	const struct sieve_action *action ATTR_UNUSED, 
	const struct sieve_action_exec_env *aenv ATTR_UNUSED,
	void **se_context ATTR_UNUSED, void *tr_context ATTR_UNUSED)
{	
	struct act_store_transaction *trans = 
		(struct act_store_transaction *) tr_context;
	struct mail_storage **storage = &(aenv->exec_status->last_storage);
	enum mail_error error;
	
	/* Check whether creation is necessary */
	if ( trans->box == NULL || trans->disabled )
		return TRUE;

	/* Check whether creation has a chance of working */
	if ( trans->error_code != MAIL_ERROR_NONE && 
		trans->error_code != MAIL_ERROR_NOTFOUND )
		return FALSE;

	trans->error = NULL;
	trans->error_code = MAIL_ERROR_NONE;
	

	*storage = mailbox_get_storage(trans->box);

	/* Create mailbox */
	if ( mailbox_create(trans->box, NULL, FALSE) < 0 ) {
		(void)mail_storage_get_last_error(*storage, &error);
		if ( error != MAIL_ERROR_EXISTS ) {
			sieve_act_store_get_storage_error(aenv, trans);
			return FALSE;
		}
	}

	/* Subscribe to it if necessary */
	if ( aenv->scriptenv->mailbox_autosubscribe ) {
		(void)mailbox_list_set_subscribed
			(mailbox_get_namespace(trans->box)->list,
			 mailbox_get_name(trans->box), TRUE);
	}

	/* Try opening again */
	if ( mailbox_sync(trans->box, 0) < 0 ) {
		/* Failed definitively */
		sieve_act_store_get_storage_error(aenv, trans);
		return FALSE;
	}

	return TRUE;
}



