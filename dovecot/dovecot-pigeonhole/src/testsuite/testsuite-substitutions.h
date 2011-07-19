/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __TESTSUITE_SUBSTITUTIONS_H
#define __TESTSUITE_SUBSTITUTIONS_H

#include "sieve-common.h"
#include "sieve-objects.h"

struct testsuite_substitution_def {
	struct sieve_object_def obj_def;
	
	bool (*get_value)(const char *param, string_t **result);
};

struct testsuite_substitution {
	struct sieve_object object;

	const struct testsuite_substitution_def *def;
};

struct sieve_ast_argument *testsuite_substitution_argument_create
	(struct sieve_validator *valdtr, struct sieve_ast *ast, 
		unsigned int source_line, const char *substitution, const char *param);

#endif /* __TESTSUITE_SUBSTITUTIONS_H */
