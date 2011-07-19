/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension copy
 * --------------
 *
 * Authors: Stephan Bosch
 * Specification: RFC 3894
 * Implementation: full
 * Status: testing
 *
 */

#include "sieve-common.h"

#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-actions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-result.h"

#include "sieve-ext-copy.h"

/*
 * Forward declarations
 */

static const struct sieve_argument_def copy_tag;
static const struct sieve_operand_def copy_side_effect_operand;

/* 
 * Extension
 */

static bool ext_copy_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr);

const struct sieve_extension_def copy_extension = { 
	"copy", 
	NULL, NULL,
	ext_copy_validator_load, 
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_NO_OPERATIONS,
	SIEVE_EXT_DEFINE_OPERAND(copy_side_effect_operand)
};

static bool ext_copy_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	/* Register copy tag with redirect and fileinto commands and we don't care
	 * whether these commands are registered or even whether they will be
	 * registered at all. The validator handles either situation gracefully 
	 */
	sieve_validator_register_external_tag
		(valdtr, "redirect", ext, &copy_tag, SIEVE_OPT_SIDE_EFFECT);
	sieve_validator_register_external_tag
		(valdtr, "fileinto", ext, &copy_tag, SIEVE_OPT_SIDE_EFFECT);

	return TRUE;
}

/*
 * Side effect 
 */

static void seff_copy_print
	(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
		const struct sieve_result_print_env *rpenv, bool *keep);
static void seff_copy_post_commit
	(const struct sieve_side_effect *seffect, const struct sieve_action *action, 
		const struct sieve_action_exec_env *aenv, void *tr_context, bool *keep);

const struct sieve_side_effect_def copy_side_effect = {
	SIEVE_OBJECT("copy", &copy_side_effect_operand, 0),
	&act_store,
	NULL, NULL, NULL,
	seff_copy_print,
	NULL, NULL,
	seff_copy_post_commit, 
	NULL
};

/* 
 * Tagged argument 
 */

static bool tag_copy_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);
static bool tag_copy_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg,
    struct sieve_command *cmd);

static const struct sieve_argument_def copy_tag = { 
	"copy", 
	NULL,
	tag_copy_validate, 
	NULL, NULL,
	tag_copy_generate
};

/*
 * Operand
 */

static const struct sieve_extension_objects ext_side_effects =
	SIEVE_EXT_DEFINE_SIDE_EFFECT(copy_side_effect);

static const struct sieve_operand_def copy_side_effect_operand = {
	"copy operand",
	&copy_extension,
	0,
	&sieve_side_effect_operand_class,
	&ext_side_effects
};

/*
 * Tag registration
 */

void sieve_ext_copy_register_tag
(struct sieve_validator *valdtr, const struct sieve_extension *copy_ext,
	const char *command)
{
	if ( sieve_validator_extension_loaded(valdtr, copy_ext) ) {
		sieve_validator_register_external_tag
			(valdtr, command, copy_ext, &copy_tag, SIEVE_OPT_SIDE_EFFECT);
	}
}

/* 
 * Tag validation 
 */

static bool tag_copy_validate
	(struct sieve_validator *valdtr ATTR_UNUSED, 
	struct sieve_ast_argument **arg ATTR_UNUSED, 
	struct sieve_command *cmd ATTR_UNUSED)
{
	*arg = sieve_ast_argument_next(*arg);

	return TRUE;
}

/*
 * Code generation 
 */

static bool tag_copy_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg,
	struct sieve_command *cmd ATTR_UNUSED)
{
	if ( sieve_ast_argument_type(arg) != SAAT_TAG ) {
		return FALSE;
	}

	sieve_opr_side_effect_emit
		(cgenv->sblock, sieve_argument_ext(arg), &copy_side_effect);

	return TRUE;
}

/* 
 * Side effect implementation
 */

static void seff_copy_print
(const struct sieve_side_effect *seffect ATTR_UNUSED, 
	const struct sieve_action *action ATTR_UNUSED, 
	const struct sieve_result_print_env *rpenv, bool *keep)
{
	sieve_result_seffect_printf(rpenv, "preserve implicit keep");

	*keep = TRUE;
}

static void seff_copy_post_commit
(const struct sieve_side_effect *seffect ATTR_UNUSED, 
	const struct sieve_action *action ATTR_UNUSED, 
	const struct sieve_action_exec_env *aenv ATTR_UNUSED, 
		void *tr_context ATTR_UNUSED, bool *keep)
{	
	*keep = TRUE;
}


