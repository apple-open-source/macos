/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-ast.h"

#include "ext-ihave-common.h"

/*
 * AST context management
 */

struct ext_ihave_ast_context *ext_ihave_get_ast_context
(const struct sieve_extension *this_ext, struct sieve_ast *ast)
{
	struct ext_ihave_ast_context *actx = (struct ext_ihave_ast_context *)
		sieve_ast_extension_get_context(ast, this_ext);
	pool_t pool;

	if ( actx != NULL )
		return actx;

	pool = sieve_ast_pool(ast);
	actx = p_new(pool, struct ext_ihave_ast_context, 1);
	p_array_init(&actx->missing_extensions, pool, 64);

	sieve_ast_extension_set_context(ast, this_ext, (void *) actx);

	return actx;
}

void ext_ihave_ast_add_missing_extension
(const struct sieve_extension *this_ext, struct sieve_ast *ast,
	const char *ext_name) 
{
	struct ext_ihave_ast_context *actx = 
		ext_ihave_get_ast_context(this_ext, ast);
	const char *const *exts;
	unsigned int i, count;

	exts = array_get(&actx->missing_extensions, &count);
	for ( i = 0; i < count; i++ ) {
		if ( strcmp(exts[i], ext_name) == 0 )
			return;
	}

	array_append(&actx->missing_extensions, &ext_name, 1);
}

