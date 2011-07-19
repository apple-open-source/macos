/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_INCLUDE_BINARY_H
#define __EXT_INCLUDE_BINARY_H

#include "sieve-common.h"

/*
 * Binary context management
 */

struct ext_include_binary_context;

struct ext_include_binary_context *ext_include_binary_init
	(const struct sieve_extension *this_ext, struct sieve_binary *sbin, 
		struct sieve_ast *ast);
struct ext_include_binary_context *ext_include_binary_get_context
	(const struct sieve_extension *this_ext, struct sieve_binary *sbin);

/*
 * Variables
 */

struct sieve_variable_scope_binary *ext_include_binary_get_global_scope
	(const struct sieve_extension *this_ext, struct sieve_binary *sbin);

/*
 * Including scripts
 */

struct ext_include_script_info {
    unsigned int id;

    struct sieve_script *script;
    enum ext_include_script_location location;

    struct sieve_binary_block *block;
};

const struct ext_include_script_info *ext_include_binary_script_include
	(struct ext_include_binary_context *binctx, struct sieve_script *script,
		enum ext_include_script_location location, 
		struct sieve_binary_block *block);
bool ext_include_binary_script_is_included
	(struct ext_include_binary_context *binctx, struct sieve_script *script,
		const struct ext_include_script_info **script_info_r);

const struct ext_include_script_info *ext_include_binary_script_get_included
	(struct ext_include_binary_context *binctx, unsigned int include_id);
const struct ext_include_script_info *ext_include_binary_script_get
	(struct ext_include_binary_context *binctx, struct sieve_script *script);
unsigned int ext_include_binary_script_get_count
	(struct ext_include_binary_context *binctx);

/*
 * Dumping the binary
 */

bool ext_include_binary_dump
	(const struct sieve_extension *ext, struct sieve_dumptime_env *denv);
bool ext_include_code_dump
	(const struct sieve_extension *ext, const struct sieve_dumptime_env *denv, 
		sieve_size_t *address ATTR_UNUSED);
		
#endif /* __EXT_INCLUDE_BINARY_H */

