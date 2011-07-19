/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#include "lib.h"
#include "compat.h"
#include "mempool.h"
#include "hash.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-limits.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-binary.h"
#include "sieve-comparators.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "sieve-match-types.h"

#include <string.h>

/*
 * Types
 */
 
struct sieve_match_values {
	pool_t pool;
	ARRAY_DEFINE(values, string_t *);
	unsigned count;
};

/* 
 * Default match types
 */ 

const struct sieve_match_type_def *sieve_core_match_types[] = {
	&is_match_type, &contains_match_type, &matches_match_type
};

const unsigned int sieve_core_match_types_count = 
	N_ELEMENTS(sieve_core_match_types);

/* 
 * Match-type 'extension' 
 */

static bool mtch_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr);

const struct sieve_extension_def match_type_extension = {
	"@match-types",
	NULL, NULL,
	mtch_validator_load,
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_NO_OPERATIONS,
	SIEVE_EXT_DEFINE_NO_OPERANDS
};
	
/* 
 * Validator context:
 *   name-based match-type registry. 
 */

static struct sieve_validator_object_registry *_get_object_registry
(struct sieve_validator *valdtr)
{
	struct sieve_instance *svinst;
	const struct sieve_extension *mcht_ext;

	svinst = sieve_validator_svinst(valdtr);
	mcht_ext = sieve_get_match_type_extension(svinst);
	return sieve_validator_object_registry_get(valdtr, mcht_ext);
}
 
void sieve_match_type_register
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	const struct sieve_match_type_def *mcht_def) 
{
	struct sieve_validator_object_registry *regs = _get_object_registry(valdtr);
	
	sieve_validator_object_registry_add(regs, ext, &mcht_def->obj_def);
}

static bool sieve_match_type_exists
(struct sieve_validator *valdtr, const char *identifier) 
{
	struct sieve_validator_object_registry *regs = _get_object_registry(valdtr);

	return sieve_validator_object_registry_find(regs, identifier, NULL);
}

static const struct sieve_match_type *sieve_match_type_create_instance
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	const char *identifier) 
{
	struct sieve_validator_object_registry *regs = _get_object_registry(valdtr);
	struct sieve_object object;
	struct sieve_match_type *mcht;

	if ( !sieve_validator_object_registry_find(regs, identifier, &object) )
		return NULL;

	mcht = p_new(sieve_command_pool(cmd), struct sieve_match_type, 1);
	mcht->object = object;
	mcht->def = (const struct sieve_match_type_def *) object.def;

  return mcht;
}

bool mtch_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	struct sieve_validator_object_registry *regs = 
		sieve_validator_object_registry_init(valdtr, ext);
	unsigned int i;

	/* Register core match-types */
	for ( i = 0; i < sieve_core_match_types_count; i++ ) {
		sieve_validator_object_registry_add
			(regs, NULL, &(sieve_core_match_types[i]->obj_def));
	}

	return TRUE;
}

/* 
 * Interpreter context
 */

struct mtch_interpreter_context {
	struct sieve_match_values *match_values;
	bool match_values_enabled;
};

static void mtch_interpreter_free
(const struct sieve_extension *ext ATTR_UNUSED, 
	struct sieve_interpreter *interp ATTR_UNUSED, void *context)
{
	struct mtch_interpreter_context *mctx = 
		(struct mtch_interpreter_context *) context;
	
	if ( mctx->match_values != NULL ) {
		pool_unref(&mctx->match_values->pool);
	}
}

struct sieve_interpreter_extension mtch_interpreter_extension = {
	&match_type_extension,
	NULL,
	mtch_interpreter_free
};

static inline struct mtch_interpreter_context *get_interpreter_context
(struct sieve_interpreter *interp, bool create)
{
	struct sieve_instance *svinst;
	const struct sieve_extension *mcht_ext;
	struct mtch_interpreter_context *ctx;

	svinst = sieve_interpreter_svinst(interp);
	mcht_ext = sieve_get_match_type_extension(svinst);

	ctx = (struct mtch_interpreter_context *)
		sieve_interpreter_extension_get_context(interp, mcht_ext);

	if ( ctx == NULL && create ) {
		pool_t pool = sieve_interpreter_pool(interp);	
		ctx = p_new(pool, struct mtch_interpreter_context, 1);

		sieve_interpreter_extension_register
			(interp, mcht_ext, &mtch_interpreter_extension, (void *) ctx);
	}

	return ctx;
}

/*
 * Match values
 */

bool sieve_match_values_set_enabled
(const struct sieve_runtime_env *renv, bool enable)
{
	struct mtch_interpreter_context *ctx = 
		get_interpreter_context(renv->interp, enable);
		
	if ( ctx != NULL ) {
		bool previous = ctx->match_values_enabled;
		
		ctx->match_values_enabled = enable;
		return previous;
	}
	
	return FALSE;
}

bool sieve_match_values_are_enabled
(const struct sieve_runtime_env *renv)
{
	struct mtch_interpreter_context *ctx = 
		get_interpreter_context(renv->interp, FALSE);
		
	return ( ctx == NULL ? FALSE : ctx->match_values_enabled );
}

struct sieve_match_values *sieve_match_values_start
(const struct sieve_runtime_env *renv)
{
	struct mtch_interpreter_context *ctx = 
		get_interpreter_context(renv->interp, FALSE);
	struct sieve_match_values *match_values;
	
	if ( ctx == NULL || !ctx->match_values_enabled )
		return NULL;
	
	pool_t pool = pool_alloconly_create("sieve_match_values", 1024);
		
	match_values = p_new(pool, struct sieve_match_values, 1);
	match_values->pool = pool;
	match_values->count = 0;
	
	p_array_init(&match_values->values, pool, 4);

	return match_values;
}

static string_t *sieve_match_values_add_entry
(struct sieve_match_values *mvalues) 
{
	string_t *entry;
	
	if ( mvalues == NULL ) return NULL;	

	if ( mvalues->count >= SIEVE_MAX_MATCH_VALUES ) return NULL;
		
	if ( mvalues->count >= array_count(&mvalues->values) ) {
		entry = str_new(mvalues->pool, 64);
		array_append(&mvalues->values, &entry, 1);	} else {
		string_t * const *ep = array_idx(&mvalues->values, mvalues->count);
		entry = *ep;
		str_truncate(entry, 0);
	}
	
	mvalues->count++;

	return entry;
}

void sieve_match_values_set
(struct sieve_match_values *mvalues, unsigned int index, string_t *value)
{
	if ( mvalues != NULL && index < array_count(&mvalues->values) ) {
		string_t * const *ep = array_idx(&mvalues->values, index);
    	string_t *entry = *ep;

	    if ( entry != NULL && value != NULL ) {
			str_truncate(entry, 0);
        	str_append_str(entry, value);
		}
	}
}
	
void sieve_match_values_add
(struct sieve_match_values *mvalues, string_t *value) 
{
	string_t *entry = sieve_match_values_add_entry(mvalues); 

	if ( entry != NULL && value != NULL )
		str_append_str(entry, value);
}

void sieve_match_values_add_char
(struct sieve_match_values *mvalues, char c) 
{
	string_t *entry = sieve_match_values_add_entry(mvalues); 

	if ( entry != NULL )
		str_append_c(entry, c);
}

void sieve_match_values_skip
(struct sieve_match_values *mvalues, int num) 
{
	int i;
	
	for ( i = 0; i < num; i++ )
		(void) sieve_match_values_add_entry(mvalues); 
}

void sieve_match_values_commit
(const struct sieve_runtime_env *renv, struct sieve_match_values **mvalues)
{
	struct mtch_interpreter_context *ctx;
	
	if ( (*mvalues) == NULL ) return;
	
	ctx = get_interpreter_context(renv->interp, FALSE);
	if ( ctx == NULL || !ctx->match_values_enabled )
		return;	
		
	if ( ctx->match_values != NULL ) {
		pool_unref(&ctx->match_values->pool);
		ctx->match_values = NULL;
	}

	ctx->match_values = *mvalues;
	*mvalues = NULL;
}

void sieve_match_values_abort
(struct sieve_match_values **mvalues)
{		
	if ( (*mvalues) == NULL ) return;
	
	pool_unref(&(*mvalues)->pool);
	*mvalues = NULL;
}

void sieve_match_values_get
(const struct sieve_runtime_env *renv, unsigned int index, string_t **value_r) 
{
	struct mtch_interpreter_context *ctx = 
		get_interpreter_context(renv->interp, FALSE);
	struct sieve_match_values *mvalues;

	if ( ctx == NULL || ctx->match_values == NULL ) {
		*value_r = NULL;
		return;
	}
	
	mvalues = ctx->match_values;
	if ( index < array_count(&mvalues->values) && index < mvalues->count ) {
		string_t * const *entry = array_idx(&mvalues->values, index);
		
		*value_r = *entry;
		return;
	}

	*value_r = NULL;	
}

/* 
 * Match-type tagged argument 
 */
 
/* Forward declarations */

static bool tag_match_type_is_instance_of
	(struct sieve_validator *valdtr, struct sieve_command *cmd, 
		const struct sieve_extension *ext, const char *identifier, void **data);
static bool tag_match_type_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);
static bool tag_match_type_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
		struct sieve_command *cmd);

/* Argument object */
 
const struct sieve_argument_def match_type_tag = { 
	"MATCH-TYPE",
	tag_match_type_is_instance_of,
	tag_match_type_validate, 
	NULL,	NULL,
	tag_match_type_generate 
};

/* Argument implementation */

static bool tag_match_type_is_instance_of
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	const struct sieve_extension *ext ATTR_UNUSED, const char *identifier, 
	void **data)
{
	const struct sieve_match_type *mcht;

	if ( data == NULL )
		return sieve_match_type_exists(valdtr, identifier);

	if ( (mcht=sieve_match_type_create_instance
		(valdtr, cmd, identifier)) == NULL )
		return FALSE;
	
	*data = (void *) mcht;
	return TRUE;
}
 
static bool tag_match_type_validate
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd ATTR_UNUSED)
{
	const struct sieve_match_type *mcht = 
		(const struct sieve_match_type *) (*arg)->argument->data;
	struct sieve_match_type_context *mtctx;

	mtctx = p_new(sieve_command_pool(cmd), struct sieve_match_type_context, 1);
	mtctx->match_type = mcht;
	mtctx->argument = *arg;
	mtctx->comparator = NULL; /* Can be filled in later */

	(*arg)->argument->data = mtctx;

	/* Syntax:   
	 *   ":is" / ":contains" / ":matches" (subject to extension)
	 */
		
	/* Skip tag */
	*arg = sieve_ast_argument_next(*arg);
	
	/* Check whether this match type requires additional validation. 
	 * Additional validation can override the match type recorded in the context 
	 * for later code generation. 
	 */
	if ( mcht->def != NULL && mcht->def->validate != NULL ) {
		return mcht->def->validate(valdtr, arg, mtctx);
	}
	
	return TRUE;
}

static bool tag_match_type_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *cmd ATTR_UNUSED)
{
	struct sieve_match_type_context *mtctx =
		(struct sieve_match_type_context *) arg->argument->data;
	
	(void) sieve_opr_match_type_emit(cgenv->sblock, mtctx->match_type);
			
	return TRUE;
}

void sieve_match_types_link_tags
(struct sieve_validator *valdtr, 
	struct sieve_command_registration *cmd_reg, int id_code) 
{	
	struct sieve_instance *svinst;
	const struct sieve_extension *mcht_ext;

	svinst = sieve_validator_svinst(valdtr);
	mcht_ext = sieve_get_comparator_extension(svinst);

	sieve_validator_register_tag
		(valdtr, cmd_reg, mcht_ext, &match_type_tag, id_code); 	
}

/*
 * Validation
 */

bool sieve_match_type_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd,
	struct sieve_ast_argument *key_arg, 
	const struct sieve_match_type *mcht_default, 
	const struct sieve_comparator *cmp_default)
{
	struct sieve_ast_argument *arg = sieve_command_first_argument(cmd);
	struct sieve_ast_argument *mt_arg = NULL;
	struct sieve_match_type_context *mtctx;
	const struct sieve_match_type *mcht = NULL;
	const struct sieve_comparator *cmp = NULL;

	/* Find match type and comparator among the arguments */
	while ( arg != NULL && arg != cmd->first_positional ) {
		if ( sieve_argument_is_comparator(arg) ) {
			cmp = sieve_comparator_tag_get(arg);
			if ( mt_arg != NULL ) break;
		}

		if ( sieve_argument_is_match_type(arg) ) {
			mt_arg = arg;
			if ( cmp != NULL ) break;
		}
		arg = sieve_ast_argument_next(arg);
	}
	
	/* Verify using the default comparator if none is specified explicitly */
	if ( cmp == NULL ) {
		cmp = sieve_comparator_copy(sieve_command_pool(cmd), cmp_default);
	}
	
	/* Verify the default match type if none is specified explicitly */
	if ( mt_arg == NULL || mt_arg->argument == NULL || 
		mt_arg->argument->data == NULL ) {
		mtctx = NULL;
		mcht = sieve_match_type_copy(sieve_command_pool(cmd), mcht_default);
	} else {
		mtctx = (struct sieve_match_type_context *) mt_arg->argument->data;
		mcht = mtctx->match_type;
		mtctx->comparator = cmp;
	}

	/* Check whether this match type requires additional validation. 
	 * Additional validation can override the match type recorded in the context 
	 * for later code generation. 
	 */
	if ( mcht != NULL && mcht->def != NULL &&
		mcht->def->validate_context != NULL ) {
		return mcht->def->validate_context(valdtr, mt_arg, mtctx, key_arg);
	}
	
	return TRUE;	
}

/*
 * Match-type operand
 */
 
const struct sieve_operand_class sieve_match_type_operand_class = 
	{ "match type" };
	
static const struct sieve_extension_objects core_match_types =
	SIEVE_EXT_DEFINE_MATCH_TYPES(sieve_core_match_types);

const struct sieve_operand_def match_type_operand = { 
	"match-type", 
	NULL,
	SIEVE_OPERAND_MATCH_TYPE,
	&sieve_match_type_operand_class,
	&core_match_types
};

/*
 * Common validation implementation
 */

bool sieve_match_substring_validate_context
(struct sieve_validator *valdtr, struct sieve_ast_argument *arg,
	struct sieve_match_type_context *ctx,
	struct sieve_ast_argument *key_arg ATTR_UNUSED)
{
	const struct sieve_comparator *cmp = ctx->comparator;
		
	if ( cmp == NULL || cmp->def == NULL )
		return TRUE;
			
	if ( (cmp->def->flags & SIEVE_COMPARATOR_FLAG_SUBSTRING_MATCH) == 0 ) {
		sieve_argument_validate_error(valdtr, arg,
			"the specified %s comparator does not support "
			"sub-string matching as required by the :%s match type",
			cmp->object.def->identifier, ctx->match_type->object.def->identifier );

		return FALSE;
	}
	
	return TRUE;
} 
