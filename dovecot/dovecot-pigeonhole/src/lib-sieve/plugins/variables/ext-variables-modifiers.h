/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_VARIABLES_MODIFIERS_H
#define __EXT_VARIABLES_MODIFIERS_H

#include "sieve-common.h"
#include "sieve-runtime-trace.h"

#include "ext-variables-common.h"

#define ext_variables_namespace_name(nspc) \
	(nspc)->object->def->name
#define ext_variables_namespaces_equal(nspc1, nspc2) \
	( (nspc1)->def == (nspc2)->def ))

/*
 * Modifier registry
 */

bool ext_variables_modifier_exists
	(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
		const char *identifier);
const struct sieve_variables_modifier *ext_variables_modifier_create_instance
	(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
		struct sieve_command *cmd, const char *identifier);
	
void ext_variables_register_core_modifiers
	(const struct sieve_extension *var_ext,
		struct ext_variables_validator_context *ctx);
	
/*
 * Modifier operand
 */

extern const struct sieve_operand_def modifier_operand;

static inline void ext_variables_opr_modifier_emit
(struct sieve_binary_block *sblock, const struct sieve_extension *ext,
	const struct sieve_variables_modifier_def *modf_def)
{ 
	sieve_opr_object_emit(sblock, ext, &modf_def->obj_def);
}

static inline bool ext_variables_opr_modifier_read
(const struct sieve_runtime_env *renv, sieve_size_t *address,
	struct sieve_variables_modifier *modf)
{
	if ( !sieve_opr_object_read
		(renv, &sieve_variables_modifier_operand_class, address, &modf->object) ) {
		sieve_runtime_trace_error(renv, "invalid modifier operand");
		return FALSE;
	}

	modf->def = (const struct sieve_variables_modifier_def *) modf->object.def;
	return TRUE;
}

static inline bool ext_variables_opr_modifier_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	return sieve_opr_object_dump
		(denv, &sieve_variables_modifier_operand_class, address, NULL);
}
	
#endif /* __EXT_VARIABLES_MODIFIERS_H */
