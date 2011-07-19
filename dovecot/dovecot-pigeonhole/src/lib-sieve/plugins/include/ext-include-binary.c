/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"

#include "sieve-common.h"
#include "sieve-error.h"
#include "sieve-script.h"
#include "sieve-binary.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "sieve-ext-variables.h"

#include "ext-include-common.h"
#include "ext-include-limits.h"
#include "ext-include-variables.h"
#include "ext-include-binary.h"

/*
 * Forward declarations
 */
 
static bool ext_include_binary_save
	(const struct sieve_extension *ext, struct sieve_binary *sbin, void *context);
static bool ext_include_binary_open
	(const struct sieve_extension *ext, struct sieve_binary *sbin, void *context);
static bool ext_include_binary_up_to_date
	(const struct sieve_extension *ext, struct sieve_binary *sbin, void *context);
static void ext_include_binary_free
	(const struct sieve_extension *ext, struct sieve_binary *sbin, void *context);

/* 
 * Binary include extension
 */
 
const struct sieve_binary_extension include_binary_ext = {
	&include_extension,
	ext_include_binary_save,
	ext_include_binary_open,
	ext_include_binary_free,
	ext_include_binary_up_to_date
};

/*
 * Binary context management
 */
 
struct ext_include_binary_context {
	struct sieve_binary *binary;
	struct sieve_binary_block *dependency_block;
	
	struct hash_table *included_scripts;
	ARRAY_DEFINE(include_index, struct ext_include_script_info *);

	struct sieve_variable_scope_binary *global_vars;
};

 
static struct ext_include_binary_context *ext_include_binary_create_context
(const struct sieve_extension *this_ext, struct sieve_binary *sbin)
{
	pool_t pool = sieve_binary_pool(sbin);
	
	struct ext_include_binary_context *ctx = 
		p_new(pool, struct ext_include_binary_context, 1);
	
	ctx->binary = sbin;			
	ctx->included_scripts = hash_table_create(default_pool, pool, 0, 
		(hash_callback_t *) sieve_script_hash, 
		(hash_cmp_callback_t *) sieve_script_cmp);
	p_array_init(&ctx->include_index, pool, 128);

	sieve_binary_extension_set(sbin, this_ext, &include_binary_ext, ctx);

	return ctx;
}

struct ext_include_binary_context *ext_include_binary_get_context
(const struct sieve_extension *this_ext, struct sieve_binary *sbin)
{	
	struct ext_include_binary_context *ctx = (struct ext_include_binary_context *)
		sieve_binary_extension_get_context(sbin, this_ext);
	
	if ( ctx == NULL )
		ctx = ext_include_binary_create_context(this_ext, sbin);
	
	return ctx;
}
 
struct ext_include_binary_context *ext_include_binary_init
(const struct sieve_extension *this_ext, struct sieve_binary *sbin, 
	struct sieve_ast *ast)
{
	struct ext_include_ast_context *ast_ctx =
		ext_include_get_ast_context(this_ext, ast);
	struct ext_include_binary_context *ctx;
	
	/* Get/create our context from the binary we are working on */
	ctx = ext_include_binary_get_context(this_ext, sbin);
	
	/* Create dependency block */
	if ( ctx->dependency_block == 0 )
		ctx->dependency_block = 
			sieve_binary_extension_create_block(sbin, this_ext);

	if ( ctx->global_vars == NULL ) {
		ctx->global_vars = 
			sieve_variable_scope_binary_create(ast_ctx->global_vars);
		sieve_variable_scope_binary_ref(ctx->global_vars);
	}
			
	return ctx;
}

/*
 * Script inclusion
 */

const struct ext_include_script_info *ext_include_binary_script_include
(struct ext_include_binary_context *binctx, struct sieve_script *script,
	enum ext_include_script_location location, struct sieve_binary_block *inc_block)
{
	pool_t pool = sieve_binary_pool(binctx->binary);
	struct ext_include_script_info *incscript;

	incscript = p_new(pool, struct ext_include_script_info, 1);
	incscript->id = array_count(&binctx->include_index)+1;
	incscript->script = script;
	incscript->location = location;
	incscript->block = inc_block;
	
	/* Unreferenced on binary_free */
	sieve_script_ref(script);
	
	hash_table_insert
		(binctx->included_scripts, (void *) script, (void *) incscript);
	array_append(&binctx->include_index, &incscript, 1);

	return incscript;
}

bool ext_include_binary_script_is_included
(struct ext_include_binary_context *binctx, struct sieve_script *script,
	const struct ext_include_script_info **script_info_r)
{
	struct ext_include_script_info *incscript = (struct ext_include_script_info *)
		hash_table_lookup(binctx->included_scripts, script);
		
	if ( incscript == NULL )
		return FALSE;
				
	*script_info_r = incscript;
	return TRUE;
}

const struct ext_include_script_info *ext_include_binary_script_get_included
(struct ext_include_binary_context *binctx, unsigned int include_id)
{		
	if ( include_id > 0 && 
		(include_id - 1) < array_count(&binctx->include_index) ) {
		struct ext_include_script_info *const *sinfo =
			array_idx(&binctx->include_index, include_id - 1);

		return *sinfo;
	}

	return NULL;
}

const struct ext_include_script_info *ext_include_binary_script_get
(struct ext_include_binary_context *binctx, struct sieve_script *script)
{
	return (struct ext_include_script_info *)
		hash_table_lookup(binctx->included_scripts, script);
}

unsigned int ext_include_binary_script_get_count
(struct ext_include_binary_context *binctx)
{
	return array_count(&binctx->include_index);
}

/*
 * Variables 
 */

struct sieve_variable_scope_binary *ext_include_binary_get_global_scope
(const struct sieve_extension *this_ext, struct sieve_binary *sbin)
{
	struct ext_include_binary_context *binctx = 
		ext_include_binary_get_context(this_ext, sbin);

	return binctx->global_vars;
}

/*
 * Binary extension
 */

static bool ext_include_binary_save
(const struct sieve_extension *ext ATTR_UNUSED, 
	struct sieve_binary *sbin ATTR_UNUSED, void *context)
{
	struct ext_include_binary_context *binctx = 
		(struct ext_include_binary_context *) context;
	struct ext_include_script_info *const *scripts;
	struct sieve_binary_block *sblock = binctx->dependency_block;
	unsigned int script_count, i;
	bool result = TRUE;
	
	sieve_binary_block_clear(sblock);

	scripts = array_get(&binctx->include_index, &script_count);

	sieve_binary_emit_unsigned(sblock, script_count);

	for ( i = 0; i < script_count; i++ ) {
		struct ext_include_script_info *incscript = scripts[i];

		sieve_binary_emit_unsigned(sblock, sieve_binary_block_get_id(incscript->block));
		sieve_binary_emit_byte(sblock, incscript->location);
		sieve_binary_emit_cstring(sblock, sieve_script_name(incscript->script));
	}

	result = ext_include_variables_save(sblock, binctx->global_vars);
	
	return result;
}

static bool ext_include_binary_open
(const struct sieve_extension *ext, struct sieve_binary *sbin, void *context)
{
	struct sieve_instance *svinst = ext->svinst;
	struct ext_include_binary_context *binctx = 
		(struct ext_include_binary_context *) context;
	struct sieve_binary_block *sblock;
	unsigned int depcount, i, block_id;
	sieve_size_t offset;
	
	sblock = sieve_binary_extension_get_block(sbin, ext);
	block_id = sieve_binary_block_get_id(sblock);
			
	offset = 0;	
		
	if ( !sieve_binary_read_unsigned(sblock, &offset, &depcount) ) {
		sieve_sys_error(svinst, 
			"include: failed to read include count "
			"for dependency block %d of binary %s", block_id, 
			sieve_binary_path(sbin));
		return FALSE;
	}
	
	/* Check include limit */	
	if ( depcount > EXT_INCLUDE_MAX_INCLUDES ) {
		sieve_sys_error(svinst,
			"include: binary %s includes too many scripts (%u > %u)",
			sieve_binary_path(sbin), depcount, EXT_INCLUDE_MAX_INCLUDES); 
		return FALSE;
	}
	
	/* Read dependencies */
	for ( i = 0; i < depcount; i++ ) {
		unsigned int inc_block_id;
		struct sieve_binary_block *inc_block;
		unsigned int location;
		string_t *script_name;
		const char *script_dir;
		struct sieve_script *script;
		
		if ( 
			!sieve_binary_read_unsigned(sblock, &offset, &inc_block_id) ||
			!sieve_binary_read_byte(sblock, &offset, &location) ||
			!sieve_binary_read_string(sblock, &offset, &script_name) ) {
			/* Binary is corrupt, recompile */
			sieve_sys_error(svinst,
				"include: failed to read included script "
				"from dependency block %d of binary %s", block_id, 
				sieve_binary_path(sbin)); 
			return FALSE;
		}

		if ( (inc_block=sieve_binary_block_get(sbin, inc_block_id)) == NULL ) {
			sieve_sys_error(svinst,
				"include: failed to find block %d for included script "
				"from dependency block %d of binary %s", inc_block_id, block_id, 
				sieve_binary_path(sbin)); 

			return FALSE;
		}
		
		if ( location >= EXT_INCLUDE_LOCATION_INVALID ) {
			/* Binary is corrupt, recompile */
			sieve_sys_error(svinst,
				"include: dependency block %d of binary %s "
				"reports invalid script location (id %d)", 
				block_id, sieve_binary_path(sbin), location); 
			return FALSE;
		}		
		
		/* Can we find/open the script dependency ? */
		script_dir = ext_include_get_script_directory
			(ext, location, str_c(script_name));		
		if ( script_dir == NULL || 
			!(script=sieve_script_create_in_directory
				(ext->svinst, script_dir, str_c(script_name), NULL, NULL)) ) {
			/* No, recompile */
			return FALSE;
		}
		
		(void)ext_include_binary_script_include
			(binctx, script, location, inc_block);
				
		sieve_script_unref(&script);
	}

	if ( !ext_include_variables_load
		(ext, sblock, &offset, &binctx->global_vars) )
		return FALSE;
	
	return TRUE;	
}

static bool ext_include_binary_up_to_date
(const struct sieve_extension *ext ATTR_UNUSED, struct sieve_binary *sbin, 
	void *context)
{
	struct ext_include_binary_context *binctx = 
		(struct ext_include_binary_context *) context;
	struct hash_iterate_context *hctx;
	void *key, *value;
		
	/* Check all included scripts for changes */
	hctx = hash_table_iterate_init(binctx->included_scripts);
	while ( hash_table_iterate(hctx, &key, &value) ) {
		struct ext_include_script_info *incscript = 
			(struct ext_include_script_info *) value;
		
		/* Is the binary old than this dependency? */
		if ( sieve_binary_script_newer(sbin, incscript->script) ) {
			/* No, recompile */
			return FALSE;
		}
	}
	hash_table_iterate_deinit(&hctx);

	return TRUE;
}

static void ext_include_binary_free
(const struct sieve_extension *ext ATTR_UNUSED, 
	struct sieve_binary *sbin ATTR_UNUSED, void *context)
{
	struct ext_include_binary_context *binctx = 
		(struct ext_include_binary_context *) context;
	struct hash_iterate_context *hctx;
	void *key, *value;
		
	/* Release references to all included script objects */
	hctx = hash_table_iterate_init(binctx->included_scripts);
	while ( hash_table_iterate(hctx, &key, &value) ) {
		struct ext_include_script_info *incscript = 
			(struct ext_include_script_info *) value;
		
		sieve_script_unref(&incscript->script);
	}
	hash_table_iterate_deinit(&hctx);

	hash_table_destroy(&binctx->included_scripts);

	if ( binctx->global_vars != NULL ) 
		sieve_variable_scope_binary_unref(&binctx->global_vars);
}

/*
 * Dumping the binary 
 */

bool ext_include_binary_dump
(const struct sieve_extension *ext, struct sieve_dumptime_env *denv)
{
	struct sieve_binary *sbin = denv->sbin;
	struct ext_include_binary_context *binctx = 
		ext_include_binary_get_context(ext, sbin);
	struct hash_iterate_context *hctx;
	void *key, *value;

	if ( !ext_include_variables_dump(denv, binctx->global_vars) )
		return FALSE;

	hctx = hash_table_iterate_init(binctx->included_scripts);		
	while ( hash_table_iterate(hctx, &key, &value) ) {
		struct ext_include_script_info *incscript = 
			(struct ext_include_script_info *) value;
		unsigned int block_id = sieve_binary_block_get_id(incscript->block);
		
		sieve_binary_dump_sectionf(denv, "Included %s script '%s' (block: %d)", 
			ext_include_script_location_name(incscript->location), 
			sieve_script_name(incscript->script), block_id);
							
		denv->sblock = incscript->block;
		denv->cdumper = sieve_code_dumper_create(denv);

		if ( denv->cdumper == NULL )
			return FALSE;

		sieve_code_dumper_run(denv->cdumper);
		sieve_code_dumper_free(&(denv->cdumper));
	}
		
	hash_table_iterate_deinit(&hctx);
	
	return TRUE;
}

bool ext_include_code_dump
(const struct sieve_extension *ext, const struct sieve_dumptime_env *denv, 
	sieve_size_t *address ATTR_UNUSED)
{
	struct sieve_binary *sbin = denv->sbin;
	struct ext_include_binary_context *binctx = 
		ext_include_binary_get_context(ext, sbin);
	struct ext_include_context *ectx = ext_include_get_context(ext);
	
	sieve_ext_variables_dump_set_scope
		(ectx->var_ext, denv, ext, 
			sieve_variable_scope_binary_get(binctx->global_vars));

	return TRUE;
}


