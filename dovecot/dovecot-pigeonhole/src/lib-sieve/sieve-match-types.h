/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#ifndef __SIEVE_MATCH_TYPES_H
#define __SIEVE_MATCH_TYPES_H

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-objects.h"

/*
 * Types
 */

struct sieve_match_type_context;

/*
 * Core match types 
 */
 
enum sieve_match_type_code {
	SIEVE_MATCH_TYPE_IS,
	SIEVE_MATCH_TYPE_CONTAINS,
	SIEVE_MATCH_TYPE_MATCHES,
	SIEVE_MATCH_TYPE_CUSTOM
};

extern const struct sieve_match_type_def is_match_type;
extern const struct sieve_match_type_def contains_match_type;
extern const struct sieve_match_type_def matches_match_type;

/*
 * Match type definition
 */
 
struct sieve_match_type_def {
	struct sieve_object_def obj_def;
		
	bool (*validate)
		(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
			struct sieve_match_type_context *ctx);
	bool (*validate_context)
		(struct sieve_validator *valdtr, struct sieve_ast_argument *arg, 
			struct sieve_match_type_context *ctx, struct sieve_ast_argument *key_arg);
			
	/*
	 * Matching
 	 */

	/* Custom implementation */

	int (*match)
		(struct sieve_match_context *mctx, struct sieve_stringlist *value_list,
			struct sieve_stringlist *key_list);

	/* Default match loop */ 

	void (*match_init)(struct sieve_match_context *mctx);

	int (*match_keys)
		(struct sieve_match_context *mctx, const char *val, size_t val_size, 
			struct sieve_stringlist *key_list);
	int (*match_key)
		(struct sieve_match_context *mctx, const char *val, size_t val_size, 
			const char *key, size_t key_size);

	void (*match_deinit)(struct sieve_match_context *mctx);
};

/* 
 * Match type instance
 */

struct sieve_match_type {
	struct sieve_object object;

	const struct sieve_match_type_def *def; 
};

#define SIEVE_MATCH_TYPE_DEFAULT(definition) \
	{ SIEVE_OBJECT_DEFAULT(definition), &(definition) }

#define sieve_match_type_name(mcht) \
	( (mcht)->object.def->identifier )
#define sieve_match_type_is(mcht, definition) \
	( (mcht)->def == &(definition) ) 

static inline const struct sieve_match_type *sieve_match_type_copy
(pool_t pool, const struct sieve_match_type *cmp_orig)
{
	struct sieve_match_type *cmp = p_new(pool, struct sieve_match_type, 1);

	*cmp = *cmp_orig;

	return cmp;
}

/*
 * Match type context
 */

struct sieve_match_type_context {
	struct sieve_command *command;
	struct sieve_ast_argument *argument;

	const struct sieve_match_type *match_type;
	
	/* Only filled in when match_type->validate_context() is called */
	const struct sieve_comparator *comparator;
	
	/* Context data could be used in the future to pass data between validator and
	 * generator in match types that use extra parameters. Currently not 
	 * necessary, not even for the relational extension.
	 */
	void *ctx_data;
};

/*
 * Match type registration
 */

void sieve_match_type_register
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		const struct sieve_match_type_def *mcht);

/* 
 * Match values 
 */

struct sieve_match_values;

bool sieve_match_values_set_enabled
	(const struct sieve_runtime_env *renv, bool enable);
bool sieve_match_values_are_enabled
	(const struct sieve_runtime_env *renv);	
	
struct sieve_match_values *sieve_match_values_start
	(const struct sieve_runtime_env *renv);
void sieve_match_values_set
	(struct sieve_match_values *mvalues, unsigned int index, string_t *value);
void sieve_match_values_add
	(struct sieve_match_values *mvalues, string_t *value);
void sieve_match_values_add_char
	(struct sieve_match_values *mvalues, char c);	
void sieve_match_values_skip
	(struct sieve_match_values *mvalues, int num);
	
void sieve_match_values_commit
	(const struct sieve_runtime_env *renv, struct sieve_match_values **mvalues);
void sieve_match_values_abort
	(struct sieve_match_values **mvalues);
	
void sieve_match_values_get
	(const struct sieve_runtime_env *renv, unsigned int index, string_t **value_r);

/*
 * Match type tagged argument 
 */

extern const struct sieve_argument_def match_type_tag;

static inline bool sieve_argument_is_match_type
	(struct sieve_ast_argument *arg)
{
	return ( arg->argument->def == &match_type_tag );
}

void sieve_match_types_link_tags
	(struct sieve_validator *valdtr, 
		struct sieve_command_registration *cmd_reg, int id_code);

/*
 * Validation
 */

bool sieve_match_type_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd,
		struct sieve_ast_argument *key_arg, 
		const struct sieve_match_type *mcht_default, 
		const struct sieve_comparator *cmp_default);

/*
 * Match type operand
 */
 
extern const struct sieve_operand_def match_type_operand;
extern const struct sieve_operand_class sieve_match_type_operand_class;

#define SIEVE_EXT_DEFINE_MATCH_TYPE(OP) SIEVE_EXT_DEFINE_OBJECT(OP)
#define SIEVE_EXT_DEFINE_MATCH_TYPES(OPS) SIEVE_EXT_DEFINE_OBJECTS(OPS)

static inline bool sieve_operand_is_match_type
(const struct sieve_operand *operand)
{
	return ( operand != NULL && operand->def != NULL &&
		operand->def->class == &sieve_match_type_operand_class );
}

static inline void sieve_opr_match_type_emit
(struct sieve_binary_block *sblock, const struct sieve_match_type *mcht)
{ 
	sieve_opr_object_emit(sblock, mcht->object.ext, mcht->object.def);
}

static inline bool sieve_opr_match_type_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	return sieve_opr_object_dump
		(denv, &sieve_match_type_operand_class, address, NULL);
}

static inline int sieve_opr_match_type_read
(const struct sieve_runtime_env *renv, sieve_size_t *address,
	struct sieve_match_type *mcht)
{
	if ( !sieve_opr_object_read
		(renv, &sieve_match_type_operand_class, address, &mcht->object) )
		return SIEVE_EXEC_BIN_CORRUPT;

	mcht->def = (const struct sieve_match_type_def *) mcht->object.def;
	return SIEVE_EXEC_OK;
}

/* Common validation implementation */

bool sieve_match_substring_validate_context
	(struct sieve_validator *valdtr, struct sieve_ast_argument *arg,
		struct sieve_match_type_context *ctx, 
		struct sieve_ast_argument *key_arg);

#endif /* __SIEVE_MATCH_TYPES_H */
