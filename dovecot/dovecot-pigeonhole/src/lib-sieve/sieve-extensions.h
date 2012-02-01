/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_EXTENSIONS_H
#define __SIEVE_EXTENSIONS_H

#include "lib.h"
#include "sieve-common.h"

/* 
 * Per-extension object registry 
 */

struct sieve_extension_objects {
	const void *objects;
	unsigned int count;
};

/* 
 * Extension definition
 */

struct sieve_extension_def {
	const char *name;

	/* Registration */		
	bool (*load)(const struct sieve_extension *ext, void **context);
	void (*unload)(const struct sieve_extension *ext);

	/* Compilation */
	bool (*validator_load)
		(const struct sieve_extension *ext, struct sieve_validator *validator);	
	bool (*generator_load)
		(const struct sieve_extension *ext, const struct sieve_codegen_env *cgenv);
	bool (*interpreter_load)
		(const struct sieve_extension *ext, const struct sieve_runtime_env *renv, 
			sieve_size_t *address);
	bool (*binary_load)
		(const struct sieve_extension *ext, struct sieve_binary *binary);
	
	/* Code dump */
	bool (*binary_dump)
		(const struct sieve_extension *ext, struct sieve_dumptime_env *denv);
	bool (*code_dump)
		(const struct sieve_extension *ext, const struct sieve_dumptime_env *denv, 
			sieve_size_t *address);

	/* Objects */
	struct sieve_extension_objects operations;
	struct sieve_extension_objects operands;
};

#define SIEVE_EXT_DEFINE_NO_OBJECTS \
	{ NULL, 0 }
#define SIEVE_EXT_DEFINE_OBJECT(OBJ) \
	{ &OBJ, 1 }
#define SIEVE_EXT_DEFINE_OBJECTS(OBJS) \
	{ OBJS, N_ELEMENTS(OBJS) }

#define SIEVE_EXT_GET_OBJECTS_COUNT(ext, field) \
	ext->field->count;

/*
 * Extension instance
 */

struct sieve_extension {
	const struct sieve_extension_def *def;
	int id;

	struct sieve_instance *svinst;
	void *context;	

	unsigned int required:1;
	unsigned int loaded:1;
	unsigned int enabled:1;
	unsigned int dummy:1;
};

#define sieve_extension_is(ext, definition) \
	( (ext)->def == &(definition) )
#define sieve_extension_name(ext) \
	(ext)->def->name
#define sieve_extension_name_is(ext, _name) \
	( strcmp((ext)->def->name, (_name)) == 0 )

/* 
 * Defining opcodes and operands 
 */

#define SIEVE_EXT_DEFINE_NO_OPERATIONS SIEVE_EXT_DEFINE_NO_OBJECTS
#define SIEVE_EXT_DEFINE_OPERATION(OP) SIEVE_EXT_DEFINE_OBJECT(OP)
#define SIEVE_EXT_DEFINE_OPERATIONS(OPS) SIEVE_EXT_DEFINE_OBJECTS(OPS)

#define SIEVE_EXT_DEFINE_NO_OPERANDS SIEVE_EXT_DEFINE_NO_OBJECTS
#define SIEVE_EXT_DEFINE_OPERAND(OP) SIEVE_EXT_DEFINE_OBJECT(OP)
#define SIEVE_EXT_DEFINE_OPERANDS(OPS) SIEVE_EXT_DEFINE_OBJECTS(OPS)

/*  
 * Extensions init/deinit 
 */

bool sieve_extensions_init(struct sieve_instance *svinst);
void sieve_extensions_deinit(struct sieve_instance *svinst);

/* 
 * Pre-loaded extensions 
 */

const struct sieve_extension *const *sieve_extensions_get_preloaded
	(struct sieve_instance *svinst, unsigned int *count_r);

/* 
 * Extension registry 
 */

const struct sieve_extension *sieve_extension_register
	(struct sieve_instance *svinst, const struct sieve_extension_def *extension, 
		bool load);
const struct sieve_extension *sieve_extension_require
	(struct sieve_instance *svinst, const struct sieve_extension_def *extension, 
		bool load);
bool sieve_extension_reload(const struct sieve_extension *ext);

void sieve_extension_unregister(const struct sieve_extension *ext);

int sieve_extensions_get_count(struct sieve_instance *svinst);

const struct sieve_extension *sieve_extension_get_by_id
	(struct sieve_instance *svinst, unsigned int ext_id);
const struct sieve_extension *sieve_extension_get_by_name
	(struct sieve_instance *svinst, const char *name);

const char *sieve_extensions_get_string
	(struct sieve_instance *svinst);
void sieve_extensions_set_string
	(struct sieve_instance *svinst, const char *ext_string);

const struct sieve_extension *sieve_get_match_type_extension
	(struct sieve_instance *svinst);
const struct sieve_extension *sieve_get_comparator_extension
	(struct sieve_instance *svinst);
const struct sieve_extension *sieve_get_address_part_extension
	(struct sieve_instance *svinst);

void sieve_enable_debug_extension(struct sieve_instance *svinst);

/*
 * Capability registries
 */

struct sieve_extension_capabilities {
	const char *name;

	const char *(*get_string)(const struct sieve_extension *ext);	
};

void sieve_extension_capabilities_register
	(const struct sieve_extension *ext, 
		const struct sieve_extension_capabilities *cap);
void sieve_extension_capabilities_unregister
	(const struct sieve_extension *ext);

const char *sieve_extension_capabilities_get_string
	(struct sieve_instance *svinst, const char *cap_name);

#endif /* __SIEVE_EXTENSIONS_H */
