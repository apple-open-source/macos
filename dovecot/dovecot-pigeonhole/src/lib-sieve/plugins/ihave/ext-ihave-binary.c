/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-error.h"
#include "sieve-script.h"
#include "sieve-binary.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "ext-ihave-common.h"
#include "ext-ihave-binary.h"

/*
 * Forward declarations
 */
 
static bool ext_ihave_binary_save
	(const struct sieve_extension *ext, struct sieve_binary *sbin, void *context);
static bool ext_ihave_binary_open
	(const struct sieve_extension *ext, struct sieve_binary *sbin, void *context);
static bool ext_ihave_binary_up_to_date
	(const struct sieve_extension *ext, struct sieve_binary *sbin, void *context);

/* 
 * Binary include extension
 */
 
const struct sieve_binary_extension ihave_binary_ext = {
	&ihave_extension,
	ext_ihave_binary_save,
	ext_ihave_binary_open,
	NULL,
	ext_ihave_binary_up_to_date
};

/*
 * Binary context management
 */
 
struct ext_ihave_binary_context {
	struct sieve_binary *binary;
	struct sieve_binary_block *block;
	
	ARRAY_DEFINE(missing_extensions, const char *);
};

static struct ext_ihave_binary_context *ext_ihave_binary_create_context
(const struct sieve_extension *this_ext, struct sieve_binary *sbin)
{
	pool_t pool = sieve_binary_pool(sbin);
	
	struct ext_ihave_binary_context *ctx = 
		p_new(pool, struct ext_ihave_binary_context, 1);
	
	ctx->binary = sbin;			
	p_array_init(&ctx->missing_extensions, pool, 64);

	sieve_binary_extension_set(sbin, this_ext, &ihave_binary_ext, ctx);
	return ctx;
}

struct ext_ihave_binary_context *ext_ihave_binary_get_context
(const struct sieve_extension *this_ext, struct sieve_binary *sbin)
{	
	struct ext_ihave_binary_context *ctx = (struct ext_ihave_binary_context *)
		sieve_binary_extension_get_context(sbin, this_ext);
	
	if ( ctx == NULL )
		ctx = ext_ihave_binary_create_context(this_ext, sbin);
	
	return ctx;
}
 
struct ext_ihave_binary_context *ext_ihave_binary_init
(const struct sieve_extension *this_ext, struct sieve_binary *sbin, 
	struct sieve_ast *ast)
{
	struct ext_ihave_ast_context *ast_ctx =
		ext_ihave_get_ast_context(this_ext, ast);
	struct ext_ihave_binary_context *binctx;
	const char *const *exts;
	unsigned int i, count;
	
	binctx = ext_ihave_binary_get_context(this_ext, sbin);

	exts = array_get(&ast_ctx->missing_extensions, &count);

	if ( count > 0 ) {
		pool_t pool = sieve_binary_pool(sbin);

		if ( binctx->block == NULL )
			binctx->block = sieve_binary_extension_create_block(sbin, this_ext);
	
		for ( i = 0; i < count; i++ ) {
			const char *ext_name = p_strdup(pool, exts[i]);

			array_append(&binctx->missing_extensions, &ext_name, 1);
		}
	}	
				
	return binctx;
}

/*
 * Binary extension
 */

static bool ext_ihave_binary_save
(const struct sieve_extension *ext, struct sieve_binary *sbin, void *context)
{
	struct ext_ihave_binary_context *binctx = 
		(struct ext_ihave_binary_context *) context;
	const char *const *exts;
	unsigned int count, i;

	exts = array_get(&binctx->missing_extensions, &count);

	if ( binctx->block != NULL )
		sieve_binary_block_clear(binctx->block);
	
	if ( count > 0 ) {
		if ( binctx->block == NULL )
			binctx->block = sieve_binary_extension_create_block(sbin, ext);

		sieve_binary_emit_unsigned(binctx->block, count);

		for ( i = 0; i < count; i++ ) {
			sieve_binary_emit_cstring(binctx->block, exts[i]);
		}
	}
	
	return TRUE;
}

static bool ext_ihave_binary_open
(const struct sieve_extension *ext, struct sieve_binary *sbin, void *context)
{
	struct sieve_instance *svinst = ext->svinst;
	struct ext_ihave_binary_context *binctx = 
		(struct ext_ihave_binary_context *) context;
	struct sieve_binary_block *sblock;
	unsigned int i, count, block_id;
	sieve_size_t offset;
	
	sblock = sieve_binary_extension_get_block(sbin, ext);

	if ( sblock != NULL ) {
		binctx->block = sblock;
		block_id = sieve_binary_block_get_id(sblock);
			
		offset = 0;	
		
		/* Read number of missing extensions to read subsequently */
		if ( !sieve_binary_read_unsigned(sblock, &offset, &count) ) {
			sieve_sys_error(svinst, 
				"ihave: failed to read missing extension count "
				"from block %d of binary %s", block_id, sieve_binary_path(sbin));
			return FALSE;
		}
		
		/* Read dependencies */
		for ( i = 0; i < count; i++ ) {
			string_t *ext_name;
			const char *name;
		
			if ( !sieve_binary_read_string(sblock, &offset, &ext_name) ) {
				/* Binary is corrupt, recompile */
				sieve_sys_error(svinst,
					"ihave: failed to read missing extension name "
					"from block %d of binary %s", block_id, sieve_binary_path(sbin)); 
				return FALSE;
			}

			name = str_c(ext_name);
			array_append(&binctx->missing_extensions, &name, 1);
		}
	}
	
	return TRUE;	
}

static bool ext_ihave_binary_up_to_date
(const struct sieve_extension *ext, struct sieve_binary *sbin ATTR_UNUSED, 
	void *context)
{
	struct ext_ihave_binary_context *binctx = 
		(struct ext_ihave_binary_context *) context;
	const char *const *exts;
	unsigned int count, i;
	
	exts = array_get(&binctx->missing_extensions, &count);
	for ( i = 0; i < count; i++ ) {
		if ( sieve_extension_get_by_name(ext->svinst, exts[i]) != NULL )
			return FALSE;
	}
	
	return TRUE;
}

/*
 * Main extension interface
 */

bool ext_ihave_binary_load
(const struct sieve_extension *ext, struct sieve_binary *sbin)
{
	(void)ext_ihave_binary_get_context(ext, sbin);

	return TRUE;
}

bool ext_ihave_binary_dump
(const struct sieve_extension *ext, struct sieve_dumptime_env *denv)
{
	struct sieve_binary *sbin = denv->sbin;
	struct ext_ihave_binary_context *binctx = 
		ext_ihave_binary_get_context(ext, sbin);
	const char *const *exts;
	unsigned int count, i;
	
	exts = array_get(&binctx->missing_extensions, &count);

	if ( count > 0 ) {
		sieve_binary_dump_sectionf(denv,
			"Extensions missing at compile (block: %d)", 
			sieve_binary_block_get_id(binctx->block));

		for ( i = 0; i < count; i++ ) {		
			sieve_binary_dumpf(denv, "  -  %s\n", exts[i]);
		}
	}	
	
	return TRUE;
}


