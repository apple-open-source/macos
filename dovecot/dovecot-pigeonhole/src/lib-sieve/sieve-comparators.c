/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#include "lib.h"
#include "str-sanitize.h"
#include "hash.h"
#include "array.h"

#include "sieve-extensions.h"
#include "sieve-code.h"
#include "sieve-commands.h"
#include "sieve-binary.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "sieve-comparators.h"

#include <string.h>
#include <stdio.h>

/* 
 * Core comparators
 */
 
const struct sieve_comparator_def *sieve_core_comparators[] = {
	&i_octet_comparator, &i_ascii_casemap_comparator
};

const unsigned int sieve_core_comparators_count =
	N_ELEMENTS(sieve_core_comparators);

/* 
 * Comparator 'extension' 
 */

static bool cmp_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *valdtr);

const struct sieve_extension_def comparator_extension = {
	"@comparators",
	NULL, NULL,
	cmp_validator_load,
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_NO_OPERATIONS,
	SIEVE_EXT_DEFINE_NO_OPERANDS    /* Defined as core operand */
};

/* 
 * Validator context:
 *   name-based comparator registry. 
 */
 
static struct sieve_validator_object_registry *_get_object_registry
(struct sieve_validator *valdtr)
{
	struct sieve_instance *svinst;
	const struct sieve_extension *mcht_ext;

	svinst = sieve_validator_svinst(valdtr);
	mcht_ext = sieve_get_comparator_extension(svinst);
	return sieve_validator_object_registry_get(valdtr, mcht_ext);
}

void sieve_comparator_register
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	const struct sieve_comparator_def *cmp) 
{
	struct sieve_validator_object_registry *regs = _get_object_registry(valdtr);
	
	sieve_validator_object_registry_add(regs, ext, &cmp->obj_def);
}

static struct sieve_comparator *sieve_comparator_create
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	const char *identifier) 
{
	struct sieve_validator_object_registry *regs = _get_object_registry(valdtr);
	struct sieve_object object;
	struct sieve_comparator *cmp;

	if ( !sieve_validator_object_registry_find(regs, identifier, &object) )
		return NULL;

	cmp = p_new(sieve_command_pool(cmd), struct sieve_comparator, 1);
	cmp->object = object;
	cmp->def = (const struct sieve_comparator_def *) object.def;

  return cmp;
}

bool cmp_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	struct sieve_validator_object_registry *regs = 
		sieve_validator_object_registry_init(valdtr, ext);
	unsigned int i;
		
	/* Register core comparators */
	for ( i = 0; i < sieve_core_comparators_count; i++ ) {
		sieve_validator_object_registry_add
			(regs, NULL, &(sieve_core_comparators[i]->obj_def));
	}

	return TRUE;
}

/* 
 * Comparator tagged argument 
 */
  
/* Forward declarations */

static bool tag_comparator_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);
static bool tag_comparator_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
		struct sieve_command *cmd);

/* Argument object */

const struct sieve_argument_def comparator_tag = { 
	"comparator", 
	NULL, 
	tag_comparator_validate, 
	NULL, NULL,
	tag_comparator_generate 
};

/* Argument implementation */

static bool tag_comparator_validate
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd)
{
	struct sieve_ast_argument *tag = *arg;
	const struct sieve_comparator *cmp;
	
	/* Skip tag */
	*arg = sieve_ast_argument_next(*arg);
	
	/* Check syntax:
	 *   ":comparator" <comparator-name: string>
	 */
	if ( (*arg)->type != SAAT_STRING ) {
		sieve_argument_validate_error(valdtr, *arg, 
			":comparator tag requires one string argument, but %s was found", 
			sieve_ast_argument_name(*arg) );
		return FALSE;
	}

	if ( !sieve_validator_argument_activate(valdtr, cmd, *arg, FALSE) )
		return FALSE;

	/* FIXME: We can currently only handle string literal argument, so
	 * variables are not allowed.
	 */
	if ( !sieve_argument_is_string_literal(*arg) ) {
		sieve_argument_validate_error(valdtr, *arg, 
			"this Sieve implementation currently only supports "
			"a literal string argument for the :comparator tag");
		return FALSE;
	}
	
	/* Get comparator from registry */
	cmp = sieve_comparator_create(valdtr, cmd, sieve_ast_argument_strc(*arg));
	
	if ( cmp == NULL ) {
		sieve_argument_validate_error(valdtr, *arg, 
			"unknown comparator '%s'", 
			str_sanitize(sieve_ast_argument_strc(*arg),80));

		return FALSE;
	}
	
	/* String argument not needed during code generation, so detach it from 
	 * argument list 
	 */
	*arg = sieve_ast_arguments_detach(*arg, 1);

	/* Store comparator in context */
	tag->argument->data = (void *) cmp;
	
	return TRUE;
}

static bool tag_comparator_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *cmd ATTR_UNUSED)
{
	const struct sieve_comparator *cmp = 
		(const struct sieve_comparator *) arg->argument->data;
	
	sieve_opr_comparator_emit(cgenv->sblock, cmp);
		
	return TRUE;
}

/* Functions to enable and evaluate comparator tag for commands */

void sieve_comparators_link_tag
(struct sieve_validator *valdtr, struct sieve_command_registration *cmd_reg,	
	int id_code) 
{
	struct sieve_instance *svinst;
	const struct sieve_extension *mcht_ext;

	svinst = sieve_validator_svinst(valdtr);
	mcht_ext = sieve_get_comparator_extension(svinst);

	sieve_validator_register_tag
		(valdtr, cmd_reg, mcht_ext, &comparator_tag, id_code); 	
}

bool sieve_comparator_tag_is
(struct sieve_ast_argument *tag, const struct sieve_comparator_def *cmp_def)
{
	const struct sieve_comparator *cmp;

	if ( !sieve_argument_is(tag, comparator_tag) )
		return FALSE;

	cmp = (const struct sieve_comparator *) tag->argument->data;
	
	return ( cmp->def == cmp_def );
}

const struct sieve_comparator *sieve_comparator_tag_get
(struct sieve_ast_argument *tag)
{
	if ( !sieve_argument_is(tag, comparator_tag) )
		return NULL;

		
	return (const struct sieve_comparator *) tag->argument->data;
}

/*
 * Comparator coding
 */
 
const struct sieve_operand_class sieve_comparator_operand_class = 
	{ "comparator" };
	
static const struct sieve_extension_objects core_comparators =
	SIEVE_EXT_DEFINE_COMPARATORS(sieve_core_comparators);

const struct sieve_operand_def comparator_operand = { 
	"comparator", 
	NULL,
	SIEVE_OPERAND_COMPARATOR, 
	&sieve_comparator_operand_class,
	&core_comparators
};

/*
 * Trivial/Common comparator method implementations
 */

bool sieve_comparator_octet_skip
	(const struct sieve_comparator *cmp ATTR_UNUSED, 
		const char **val, const char *val_end)
{
	if ( *val < val_end ) {
		(*val)++;
		return TRUE;
	}
	
	return FALSE;
}
