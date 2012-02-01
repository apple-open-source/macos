/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_IHAVE_BINARY_H
#define __EXT_IHAVE_BINARY_H

/*
 * Binary context management
 */
 
struct ext_ihave_binary_context;

struct ext_ihave_binary_context *ext_ihave_binary_get_context
	(const struct sieve_extension *this_ext, struct sieve_binary *sbin);
struct ext_ihave_binary_context *ext_ihave_binary_init
	(const struct sieve_extension *this_ext, struct sieve_binary *sbin, 
		struct sieve_ast *ast);

/*
 * Registering missing extension
 */

void ext_ihave_binary_add_missing_extension
	(struct ext_ihave_binary_context *binctx, const char *ext_name);

/*
 * Main extension interface
 */

bool ext_ihave_binary_load
	(const struct sieve_extension *ext, struct sieve_binary *sbin);
bool ext_ihave_binary_dump
	(const struct sieve_extension *ext, struct sieve_dumptime_env *denv);

#endif /* __EXT_IHAVE_BINARY_H */

