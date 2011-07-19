/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
 
#ifndef __SIEVE_ADDRESS_PARTS_H
#define __SIEVE_ADDRESS_PARTS_H

#include "message-address.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-objects.h"

/*
 * Address part definition
 */

struct sieve_address_part_def {
	struct sieve_object_def obj_def;		

	const char *(*extract_from)
		(const struct sieve_address_part *addrp, 
			const struct sieve_address *address);
};

/*
 * Address part instance
 */

struct sieve_address_part {
	struct sieve_object object;

	const struct sieve_address_part_def *def;
};

#define SIEVE_ADDRESS_PART_DEFAULT(definition) \
	{ SIEVE_OBJECT_DEFAULT(definition), &(definition) };

#define sieve_address_part_name(addrp) \
	( (addrp)->object.def->identifier )
#define sieve_address_part_is(addrp, definition) \
	( (addrp)->def == &(definition) )

/*
 * Core address parts
 */
 
enum sieve_address_part_code {
	SIEVE_ADDRESS_PART_ALL,
	SIEVE_ADDRESS_PART_LOCAL,
	SIEVE_ADDRESS_PART_DOMAIN,
	SIEVE_ADDRESS_PART_CUSTOM
};

extern const struct sieve_address_part_def all_address_part;
extern const struct sieve_address_part_def local_address_part;
extern const struct sieve_address_part_def domain_address_part;

/*
 * Address part tagged argument
 */
 
extern const struct sieve_argument_def address_part_tag;

void sieve_address_parts_link_tags
	(struct sieve_validator *valdtr, struct sieve_command_registration *cmd_reg,
		int id_code);

/*
 * Address part registry
 */
		
void sieve_address_part_register
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		const struct sieve_address_part_def *addrp);
		
/*
 * Address part operand
 */

extern const struct sieve_operand_def address_part_operand;
extern const struct sieve_operand_class sieve_address_part_operand_class;

#define SIEVE_EXT_DEFINE_ADDRESS_PART(OP) SIEVE_EXT_DEFINE_OBJECT(OP)
#define SIEVE_EXT_DEFINE_ADDRESS_PARTS(OPS) SIEVE_EXT_DEFINE_OBJECTS(OPS)

static inline void sieve_opr_address_part_emit
(struct sieve_binary_block *sblock, const struct sieve_address_part *addrp)
{ 
	sieve_opr_object_emit(sblock, addrp->object.ext, addrp->object.def);
}

static inline bool sieve_opr_address_part_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	return sieve_opr_object_dump
		(denv, &sieve_address_part_operand_class, address, NULL);
}

static inline int sieve_opr_address_part_read
(const struct sieve_runtime_env *renv, sieve_size_t *address, 
	struct sieve_address_part *addrp)
{
	if ( !sieve_opr_object_read
		(renv, &sieve_address_part_operand_class, address, &addrp->object) )
		return SIEVE_EXEC_BIN_CORRUPT;

	addrp->def = (const struct sieve_address_part_def *) addrp->object.def;
	return SIEVE_EXEC_OK;
}

/*
 * Address-part string list
 */

struct sieve_stringlist *sieve_address_part_stringlist_create
	(const struct sieve_runtime_env *renv, const struct sieve_address_part *addrp,
		struct sieve_address_list *addresses);

/* 
 * Match utility 
 */

enum sieve_addrmatch_opt_operand {
	SIEVE_AM_OPT_END,
	SIEVE_AM_OPT_COMPARATOR,
	SIEVE_AM_OPT_ADDRESS_PART,
	SIEVE_AM_OPT_MATCH_TYPE
};

int sieve_addrmatch_opr_optional_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address,
		signed int *opt_code);

int sieve_addrmatch_opr_optional_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address, 
		signed int *opt_code, int *exec_status, struct sieve_address_part *addrp,
		struct sieve_match_type *mtch, struct sieve_comparator *cmp);

#endif /* __SIEVE_ADDRESS_PARTS_H */
