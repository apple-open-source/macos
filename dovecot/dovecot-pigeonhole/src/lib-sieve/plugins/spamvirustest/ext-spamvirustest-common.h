/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#ifndef __EXT_SPAMVIRUSTEST_COMMON_H
#define __EXT_SPAMVIRUSTEST_COMMON_H

#include "sieve-common.h"

/*
 * Extensions
 */
 
extern const struct sieve_extension_def spamtest_extension;
extern const struct sieve_extension_def spamtestplus_extension;
extern const struct sieve_extension_def virustest_extension;

bool ext_spamvirustest_load(const struct sieve_extension *ext, void **context);
void ext_spamvirustest_unload(const struct sieve_extension *ext);

/* 
 * Tests
 */

extern const struct sieve_command_def spamtest_test;
extern const struct sieve_command_def virustest_test;
 
const char *ext_spamvirustest_get_value
(const struct sieve_runtime_env *renv, const struct sieve_extension *ext,
	 bool percent);

/*
 * Operations
 */

extern const struct sieve_operation_def spamtest_operation;
extern const struct sieve_operation_def virustest_operation;

#endif /* __EXT_SPAMVIRUSTEST_COMMON_H */
