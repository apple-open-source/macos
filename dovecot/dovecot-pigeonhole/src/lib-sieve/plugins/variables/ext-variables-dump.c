/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#include "lib.h"
#include "str.h"
 
#include "sieve-common.h"
#include "sieve-dump.h"
#include "sieve-binary.h"
#include "sieve-code.h"

#include "ext-variables-common.h"
#include "ext-variables-dump.h"

/*
 * Code dumper extension
 */

static void ext_variables_code_dumper_free
	(struct sieve_code_dumper *dumper, void *context);

const struct sieve_code_dumper_extension variables_dump_extension = {
	&variables_extension,
	ext_variables_code_dumper_free
};

/*
 * Code dump context
 */
 
struct ext_variables_dump_context {
	struct sieve_variable_scope *local_scope;
	ARRAY_DEFINE(ext_scopes, struct sieve_variable_scope *);
};

static void ext_variables_code_dumper_free
(struct sieve_code_dumper *dumper ATTR_UNUSED, void *context)
{
	struct ext_variables_dump_context *dctx = 
		(struct ext_variables_dump_context *) context;

	if ( dctx == NULL || dctx->local_scope == NULL )
		return;

	sieve_variable_scope_unref(&dctx->local_scope);
}

static struct ext_variables_dump_context *ext_variables_dump_get_context
(const struct sieve_extension *this_ext, const struct sieve_dumptime_env *denv)
{
	struct sieve_code_dumper *dumper = denv->cdumper;
	struct ext_variables_dump_context *dctx = sieve_dump_extension_get_context
		(dumper, this_ext);
	pool_t pool;

	if ( dctx == NULL ) {
		/* Create dumper context */
		pool = sieve_code_dumper_pool(dumper);
		dctx = p_new(pool, struct ext_variables_dump_context, 1);
		p_array_init(&dctx->ext_scopes, pool, 
			sieve_extensions_get_count(this_ext->svinst));
	
		sieve_dump_extension_set_context(dumper, this_ext, dctx);
	}

	return dctx;
} 
 
bool ext_variables_code_dump
(const struct sieve_extension *ext, 
	const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	struct ext_variables_dump_context *dctx;
	struct sieve_variable_scope *local_scope;

	local_scope = sieve_variable_scope_binary_dump
		(ext->svinst, NULL, denv, address);
		
	dctx = ext_variables_dump_get_context(ext, denv);
	dctx->local_scope = local_scope;
	
	return TRUE;
}

/*
 * Scope registry
 */

void sieve_ext_variables_dump_set_scope
(const struct sieve_extension *var_ext, const struct sieve_dumptime_env *denv,
	const struct sieve_extension *ext, struct sieve_variable_scope *scope)
{
	struct ext_variables_dump_context *dctx = 
		ext_variables_dump_get_context(var_ext, denv);

	if ( ext->id < 0 ) return;

	array_idx_set(&dctx->ext_scopes, (unsigned int) ext->id, &scope);	
}

/*
 * Variable identifier dump
 */

const char *ext_variables_dump_get_identifier
(const struct sieve_extension *var_ext, const struct sieve_dumptime_env *denv,
	const struct sieve_extension *ext, unsigned int index)
{
	struct ext_variables_dump_context *dctx =
		ext_variables_dump_get_context(var_ext, denv);	
	struct sieve_variable_scope *scope;
	struct sieve_variable *var;

	if ( ext == NULL )
		scope = dctx->local_scope;
	else {
		struct sieve_variable_scope *const *ext_scope;

		if  ( ext->id < 0 || ext->id >= (int) array_count(&dctx->ext_scopes) )
			return NULL;
	
		ext_scope = array_idx(&dctx->ext_scopes, (unsigned int) ext->id);
		scope = *ext_scope;			
	}

	if ( scope == NULL )
		return NULL;
			
	var = sieve_variable_scope_get_indexed(scope, index);
	
	return var->identifier;
}

