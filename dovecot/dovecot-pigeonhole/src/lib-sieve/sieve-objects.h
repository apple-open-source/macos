/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_OBJECTS_H
#define __SIEVE_OBJECTS_H

/*
 * Object definition
 */

struct sieve_object_def {
	const char *identifier;
	const struct sieve_operand_def *operand;
	unsigned int code;
};

#define SIEVE_OBJECT(identifier, operand, code) \
	{ identifier, operand, code }

/*
 * Object instance
 */

struct sieve_object {
	const struct sieve_object_def *def;
	const struct sieve_extension *ext;
};

#define SIEVE_OBJECT_DEFAULT(_obj) \
	{ &((_obj).obj_def), NULL }

#define SIEVE_OBJECT_EXTENSION(_obj) \
	(_obj->object.ext)

#define SIEVE_OBJECT_SET_DEF(_obj, def_value) \
	STMT_START { \
			(_obj)->def = def_value;	\
			(_obj)->object.def = &(_obj)->def->obj_def; \
	} STMT_END


/*
 * Object coding
 */
 
void sieve_opr_object_emit
	(struct sieve_binary_block *sblock, const struct sieve_extension *ext,
		const struct sieve_object_def *obj_def);

bool sieve_opr_object_read_data
	(struct sieve_binary_block *sblock, const struct sieve_operand *operand,
		const struct sieve_operand_class *opclass, sieve_size_t *address,
		struct sieve_object *obj);

bool sieve_opr_object_read
	(const struct sieve_runtime_env *renv, 
		const struct sieve_operand_class *opclass, sieve_size_t *address,
		struct sieve_object *obj);

bool sieve_opr_object_dump
	(const struct sieve_dumptime_env *denv, 
		const struct sieve_operand_class *opclass, sieve_size_t *address,
		struct sieve_object *obj);


#endif /* __SIEVE_OBJECTS_H */
