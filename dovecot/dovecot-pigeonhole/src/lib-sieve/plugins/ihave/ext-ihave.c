/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension ihave
 * ---------------
 *
 * Authors: Stephan Bosch
 * Specification: RFC 5463
 * Implementation: full
 * Status: experimental
 *
 */

#include "lib.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-code.h"
#include "sieve-binary.h"
#include "sieve-validator.h"
#include "sieve-generator.h"

#include "ext-ihave-common.h"
#include "ext-ihave-binary.h"

/*
 * Extension
 */

static bool ext_ihave_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *validator);
static bool ext_ihave_generator_load
	(const struct sieve_extension *ext, const struct sieve_codegen_env *cgenv);

const struct sieve_extension_def ihave_extension = {
	"ihave",
	NULL, NULL,
	ext_ihave_validator_load,
	ext_ihave_generator_load,
	NULL, 
	ext_ihave_binary_load,
	ext_ihave_binary_dump,
	NULL,
	SIEVE_EXT_DEFINE_OPERATION(error_operation),
	SIEVE_EXT_DEFINE_NO_OPERANDS
};

static bool ext_ihave_validator_load
(const struct sieve_extension *ext, struct sieve_validator *validator)
{
	sieve_validator_register_command(validator, ext, &ihave_test);
	sieve_validator_register_command(validator, ext, &error_command);

	return TRUE;
}

static bool ext_ihave_generator_load
(const struct sieve_extension *ext, const struct sieve_codegen_env *cgenv)
{
	(void)ext_ihave_binary_init(ext, cgenv->sbin, cgenv->ast);

	return TRUE;
}

