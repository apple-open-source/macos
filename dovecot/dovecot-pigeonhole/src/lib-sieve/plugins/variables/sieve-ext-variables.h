/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* 
 * Public interface for other extensions to use 
 */
 
#ifndef __SIEVE_EXT_VARIABLES_H
#define __SIEVE_EXT_VARIABLES_H

#include "lib.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-objects.h"
#include "sieve-code.h"

/*
 * Limits
 */

unsigned int sieve_variables_get_max_scope_size(void);

/*
 * Variable extension
 */

/* FIXME: this is not suitable for future plugin support */

extern const struct sieve_extension_def variables_extension;

static inline const struct sieve_extension *sieve_ext_variables_get_extension
(struct sieve_instance *svinst)
{
	return sieve_extension_register(svinst, &variables_extension, FALSE);
}

/*
 * Variable name
 */

struct sieve_variable_name {
	string_t *identifier;
	int num_variable;
};

ARRAY_DEFINE_TYPE(sieve_variable_name, struct sieve_variable_name);

bool sieve_variable_identifier_is_valid(const char *identifier);

/*
 * Variable scope
 */

struct sieve_variable {
	const char *identifier;
	unsigned int index;

	const struct sieve_extension *ext;
	void *context;
};

struct sieve_variable_scope;

struct sieve_variable_scope *sieve_variable_scope_create
	(struct sieve_instance *svinst, const struct sieve_extension *ext);
void sieve_variable_scope_ref
	(struct sieve_variable_scope *scope);
void sieve_variable_scope_unref
	(struct sieve_variable_scope **scope);
pool_t sieve_variable_scope_pool
	(struct sieve_variable_scope *scope);

struct sieve_variable *sieve_variable_scope_declare
	(struct sieve_variable_scope *scope, const char *identifier);
struct sieve_variable *sieve_variable_scope_import
	(struct sieve_variable_scope *scope, struct sieve_variable *var);
struct sieve_variable *sieve_variable_scope_get_variable
	(struct sieve_variable_scope *scope, const char *identifier, bool create);
struct sieve_variable *sieve_variable_scope_get_indexed
	(struct sieve_variable_scope *scope, unsigned int index);

/* Binary */

struct sieve_variable_scope_binary *sieve_variable_scope_binary_create
	(struct sieve_variable_scope *scope);

void sieve_variable_scope_binary_ref
	(struct sieve_variable_scope_binary *scpbin);
void sieve_variable_scope_binary_unref
	(struct sieve_variable_scope_binary **scpbin);

struct sieve_variable_scope *sieve_variable_scope_binary_dump
	(struct sieve_instance *svinst, const struct sieve_extension *ext, 
		const struct sieve_dumptime_env *denv, sieve_size_t *address);
struct sieve_variable_scope_binary *sieve_variable_scope_binary_read
	(struct sieve_instance *svinst, const struct sieve_extension *ext,
		struct sieve_binary_block *sblock, sieve_size_t *address);

struct sieve_variable_scope *sieve_variable_scope_binary_get
	(struct sieve_variable_scope_binary *scpbin);
unsigned int sieve_variable_scope_binary_get_size
	(struct sieve_variable_scope_binary *scpbin);

/*
 * Variable namespaces
 */

struct sieve_variables_namespace;

struct sieve_variables_namespace_def {
	struct sieve_object_def obj_def;
  
	bool (*validate)
		(struct sieve_validator *valdtr, 
			const struct sieve_variables_namespace *nspc,
			struct sieve_ast_argument *arg, struct sieve_command *cmd, 
			ARRAY_TYPE(sieve_variable_name) *var_name, void **var_data, 
			bool assignment);
	bool (*generate)
		(const struct sieve_codegen_env *cgenv, 
			const struct sieve_variables_namespace *nspc,
			struct sieve_ast_argument *arg, struct sieve_command *cmd, 
			void *var_data);

	bool (*dump_variable)
		(const struct sieve_dumptime_env *denv, 
			const struct sieve_variables_namespace *nspc,
			const struct sieve_operand *oprnd, sieve_size_t *address);
	int (*read_variable)
		(const struct sieve_runtime_env *renv, 
			const struct sieve_variables_namespace *nspc, 
			const struct sieve_operand *oprnd, sieve_size_t *address, string_t **str);
};

#define SIEVE_VARIABLES_DEFINE_NAMESPACE(OP) SIEVE_EXT_DEFINE_OBJECT(OP)
#define SIEVE_VARIABLES_DEFINE_NAMESPACES(OPS) SIEVE_EXT_DEFINE_OBJECTS(OPS)

struct sieve_variables_namespace {
	struct sieve_object object;
		
	const struct sieve_variables_namespace_def *def;
};

void sieve_variables_namespace_register
(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
	const struct sieve_extension *ext,
	const struct sieve_variables_namespace_def *nspc_def);

extern const struct sieve_operand_class sieve_variables_namespace_operand_class;

void sieve_variables_opr_namespace_variable_emit
	(struct sieve_binary_block *sblock, const struct sieve_extension *var_ext,
    const struct sieve_extension *ext,
    const struct sieve_variables_namespace_def *nspc_def);

/* Iteration over all declared variables */

struct sieve_variable_scope_iter;

struct sieve_variable_scope_iter *sieve_variable_scope_iterate_init
	(struct sieve_variable_scope *scope);
bool sieve_variable_scope_iterate
	(struct sieve_variable_scope_iter *iter, struct sieve_variable **var_r);
void sieve_variable_scope_iterate_deinit
	(struct sieve_variable_scope_iter **iter);

/* Statistics */

unsigned int sieve_variable_scope_declarations
	(struct sieve_variable_scope *scope);
unsigned int sieve_variable_scope_size
	(struct sieve_variable_scope *scope);

/* Get all native variables */

struct sieve_variable * const *sieve_variable_scope_get_variables
	(struct sieve_variable_scope *scope, unsigned int *size_r);

/* 
 * Variable storage
 */	
	
struct sieve_variable_storage;

struct sieve_variable_storage *sieve_variable_storage_create
	(pool_t pool, struct sieve_variable_scope_binary *scpbin);
bool sieve_variable_get
	(struct sieve_variable_storage *storage, unsigned int index, 
		string_t **value);
bool sieve_variable_get_modifiable
	(struct sieve_variable_storage *storage, unsigned int index, 
		string_t **value);
bool sieve_variable_assign
	(struct sieve_variable_storage *storage, unsigned int index, 
		const string_t *value);
bool sieve_variable_get_identifier
	(struct sieve_variable_storage *storage, unsigned int index, 
		const char **identifier);
const char *sieve_variable_get_varid
	(struct sieve_variable_storage *storage, unsigned int index);

/*
 * Variables access
 */

bool sieve_ext_variables_is_active
	(const struct sieve_extension *var_ext, struct sieve_validator *valdtr);

struct sieve_variable_scope *sieve_ext_variables_get_local_scope
	(const struct sieve_extension *var_ext, struct sieve_validator *valdtr);

/* Runtime */

static inline const char *sieve_ext_variables_get_varid
(const struct sieve_extension *ext, unsigned int index)
{
	if ( ext == NULL )
		return t_strdup_printf("%ld", (long) index);

	return t_strdup_printf("%s:%ld", sieve_extension_name(ext), (long) index);
}
	
struct sieve_variable_storage *sieve_ext_variables_runtime_get_storage
	(const struct sieve_extension *var_ext, const struct sieve_runtime_env *renv,
		const struct sieve_extension *ext);
void sieve_ext_variables_runtime_set_storage
	(const struct sieve_extension *var_ext, const struct sieve_runtime_env *renv,
		const struct sieve_extension *ext, struct sieve_variable_storage *storage);

const char *sieve_ext_variables_runtime_get_identifier
(const struct sieve_extension *var_ext, const struct sieve_runtime_env *renv,
	const struct sieve_extension *ext, unsigned int index);
		
/* 
 * Variable arguments 
 */

bool sieve_variable_argument_activate
(const struct sieve_extension *this_ext, struct sieve_validator *valdtr,
	struct sieve_command *cmd, struct sieve_ast_argument *arg, bool assignment);
	
/* 
 * Variable operands 
 */

extern const struct sieve_operand_def variable_operand;

void sieve_variables_opr_variable_emit
	(struct sieve_binary_block *sblock, const struct sieve_extension *var_ext, 
		struct sieve_variable *var);
void sieve_variables_opr_match_value_emit
	(struct sieve_binary_block *sblock, const struct sieve_extension *var_ext, 
		unsigned int index);

int sieve_variable_operand_read_data
	(const struct sieve_runtime_env *renv, struct sieve_operand *operand, 
		sieve_size_t *address, const char *field_name,
		struct sieve_variable_storage **storage_r, unsigned int *var_index_r);
int sieve_variable_operand_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address,
		const char *field_name, struct sieve_variable_storage **storage_r,
		unsigned int *var_index_r);
		
static inline bool sieve_operand_is_variable
(const struct sieve_operand *operand)
{
	return ( operand != NULL && operand->def != NULL && 
		operand->def == &variable_operand );
}	

/* 
 * Modifiers 
 */

struct sieve_variables_modifier_def {
	struct sieve_object_def obj_def;
	
	unsigned int precedence;
	
	bool (*modify)(string_t *in, string_t **result);
};

struct sieve_variables_modifier {
	struct sieve_object object;
		
	const struct sieve_variables_modifier_def *def;
};

extern const struct sieve_operand_class sieve_variables_modifier_operand_class;

#define SIEVE_VARIABLES_DEFINE_MODIFIER(OP) SIEVE_EXT_DEFINE_OBJECT(OP)
#define SIEVE_VARIABLES_DEFINE_MODIFIERS(OPS) SIEVE_EXT_DEFINE_OBJECTS(OPS)

#define sieve_variables_modifier_name(smodf) \
	( (smodf)->object.def->identifier )

void sieve_variables_modifier_register
	(const struct sieve_extension *var_ext, struct sieve_validator *valdtr, 
		const struct sieve_extension *ext, 
		const struct sieve_variables_modifier_def *smodf);

/*
 * Code dumping
 */

void sieve_ext_variables_dump_set_scope
(const struct sieve_extension *var_ext, const struct sieve_dumptime_env *denv, 
	const struct sieve_extension *ext, struct sieve_variable_scope *scope);

#endif /* __SIEVE_EXT_VARIABLES_H */
