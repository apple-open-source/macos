/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
 
#ifndef __SIEVE_COMPARATORS_H
#define __SIEVE_COMPARATORS_H

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-objects.h"
#include "sieve-code.h"

/* 
 * Core comparators 
 */
 
enum sieve_comparator_code {
	SIEVE_COMPARATOR_I_OCTET,
	SIEVE_COMPARATOR_I_ASCII_CASEMAP,
	SIEVE_COMPARATOR_CUSTOM
};

extern const struct sieve_comparator_def i_octet_comparator;
extern const struct sieve_comparator_def i_ascii_casemap_comparator;

/*
 * Comparator flags
 */

enum sieve_comparator_flags {
	SIEVE_COMPARATOR_FLAG_ORDERING = (1 << 0),
	SIEVE_COMPARATOR_FLAG_EQUALITY = (1 << 1),
	SIEVE_COMPARATOR_FLAG_PREFIX_MATCH = (1 << 2),
	SIEVE_COMPARATOR_FLAG_SUBSTRING_MATCH = (1 << 3),	
};

/*
 * Comparator definition
 */

struct sieve_comparator_def {
	struct sieve_object_def obj_def;	
		
	unsigned int flags;
	
	/* Equality and ordering */

	int (*compare)(const struct sieve_comparator *cmp, 
		const char *val1, size_t val1_size, 
		const char *val2, size_t val2_size);
	
	/* Prefix and substring match */
	
	bool (*char_match)(const struct sieve_comparator *cmp, 
		const char **val, const char *val_end,
		const char **key, const char *key_end);
	bool (*char_skip)(const struct sieve_comparator *cmp, 
		const char **val, const char *val_end);
};

/*
 * Comparator instance
 */

struct sieve_comparator {
	struct sieve_object object;
	
	const struct sieve_comparator_def *def;
};	

#define SIEVE_COMPARATOR_DEFAULT(definition) \
	{ SIEVE_OBJECT_DEFAULT(definition), &(definition) }

#define sieve_comparator_name(cmp) \
	( (cmp)->object.def->identifier )
#define sieve_comparator_is(cmp, definition) \
	( (cmp)->def == &(definition) ) 

static inline const struct sieve_comparator *sieve_comparator_copy
(pool_t pool, const struct sieve_comparator *cmp_orig)
{
	struct sieve_comparator *cmp = p_new(pool, struct sieve_comparator, 1);

	*cmp = *cmp_orig;

	return cmp;
}

/*
 * Comparator tagged argument
 */
 
extern const struct sieve_argument_def comparator_tag;

static inline bool sieve_argument_is_comparator
(struct sieve_ast_argument *arg) 
{
	return ( arg->argument != NULL && 
		(arg->argument->def == &comparator_tag) );
}

void sieve_comparators_link_tag
	(struct sieve_validator *validator, 
		struct sieve_command_registration *cmd_reg,	int id_code);
bool sieve_comparator_tag_is
	(struct sieve_ast_argument *tag, const struct sieve_comparator_def *cmp);
const struct sieve_comparator *sieve_comparator_tag_get
	(struct sieve_ast_argument *tag);

void sieve_comparator_register
	(struct sieve_validator *validator, const struct sieve_extension *ext,
		const struct sieve_comparator_def *cmp); 
		
/*
 * Comparator operand
 */

#define SIEVE_EXT_DEFINE_COMPARATOR(OP) SIEVE_EXT_DEFINE_OBJECT(OP)
#define SIEVE_EXT_DEFINE_COMPARATORS(OPS) SIEVE_EXT_DEFINE_OBJECTS(OPS)

extern const struct sieve_operand_class sieve_comparator_operand_class;
extern const struct sieve_operand_def comparator_operand;

static inline void sieve_opr_comparator_emit
(struct sieve_binary_block *sblock, const struct sieve_comparator *cmp)
{ 
	sieve_opr_object_emit(sblock, cmp->object.ext, cmp->object.def);
}
static inline bool sieve_opr_comparator_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	return sieve_opr_object_dump
		(denv, &sieve_comparator_operand_class, address, NULL);
}

static inline int sieve_opr_comparator_read
(const struct sieve_runtime_env *renv, sieve_size_t *address,
	struct sieve_comparator *cmp)
{
	if ( !sieve_opr_object_read
		(renv, &sieve_comparator_operand_class, address, &cmp->object) )
		return SIEVE_EXEC_BIN_CORRUPT;

	cmp->def = (const struct sieve_comparator_def *) cmp->object.def;
	return SIEVE_EXEC_OK;
}
	
/*
 * Trivial/Common comparator method implementations
 */

bool sieve_comparator_octet_skip
	(const struct sieve_comparator *cmp ATTR_UNUSED, 
		const char **val, const char *val_end);

#endif /* __SIEVE_COMPARATORS_H */
