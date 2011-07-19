/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __TESTSUITE_OBJECTS_H
#define __TESTSUITE_OBJECTS_H

#include "sieve-common.h"
#include "sieve-objects.h"

#include "testsuite-common.h"

/* 
 * Testsuite object operand 
 */

struct testsuite_object_operand_interface {
	struct sieve_extension_objects testsuite_objects;
};

extern const struct sieve_operand_class testsuite_object_oprclass;

/* 
 * Testsuite object access 
 */

struct testsuite_object_def {
	struct sieve_object_def obj_def;
	
	int (*get_member_id)(const char *identifier);
	const char *(*get_member_name)(int id);

	bool (*set_member)
		(const struct sieve_runtime_env *renv, int id, string_t *value);
	string_t *(*get_member)
		(const struct sieve_runtime_env *renv, int id);
};

struct testsuite_object {
	struct sieve_object object;

	const struct testsuite_object_def *def;
};

/* 
 * Testsuite object registration 
 */

void testsuite_register_core_objects
	(struct testsuite_validator_context *ctx);
void testsuite_object_register
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		const struct testsuite_object_def *tobj_def);		
		
/* 
 * Testsuite object argument 
 */		
	
bool testsuite_object_argument_activate
	(struct sieve_validator *valdtr, struct sieve_ast_argument *arg,
		struct sieve_command *cmd);		
		
/* 
 * Testsuite object code 
 */

bool testsuite_object_read
  (struct sieve_binary_block *sblock, sieve_size_t *address, 
		struct testsuite_object *tobj);
bool testsuite_object_read_member
  (struct sieve_binary_block *sblock, sieve_size_t *address,
		struct testsuite_object *tobj, int *member_id_r);

bool testsuite_object_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);

const char *testsuite_object_member_name
	(const struct testsuite_object *object, int member_id);

/* 
 * Testsuite core objects 
 */

extern const struct testsuite_object_def message_testsuite_object;
extern const struct testsuite_object_def envelope_testsuite_object;

#endif /* __TESTSUITE_OBJECTS_H */
