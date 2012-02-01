/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_IHAVE_COMMON_H
#define __EXT_IHAVE_COMMON_H

#include "sieve-common.h"

/*
 * Extensions
 */

extern const struct sieve_extension_def ihave_extension;

/*
 * Tests
 */

extern const struct sieve_command_def ihave_test;

/*
 * Commands
 */

extern const struct sieve_command_def error_command;

/*
 * Operations
 */

extern const struct sieve_operation_def error_operation;

/*
 * AST context
 */

struct ext_ihave_ast_context {
  ARRAY_DEFINE(missing_extensions, const char *);
};

struct ext_ihave_ast_context *ext_ihave_get_ast_context
	(const struct sieve_extension *this_ext, struct sieve_ast *ast);

void ext_ihave_ast_add_missing_extension
	(const struct sieve_extension *this_ext, struct sieve_ast *ast,
		const char *ext_name);


#endif /* __EXT_IHAVE_COMMON_H */
