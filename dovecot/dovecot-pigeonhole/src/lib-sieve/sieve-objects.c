/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-code.h"
#include "sieve-binary.h"
#include "sieve-dump.h"
#include "sieve-interpreter.h"

#include "sieve-objects.h"

/*
 * Object coding
 */

void sieve_opr_object_emit
(struct sieve_binary_block *sblock, const struct sieve_extension *ext,
	const struct sieve_object_def *obj_def)
{
	struct sieve_extension_objects *objs = 
		(struct sieve_extension_objects *) obj_def->operand->interface;
		 
	(void) sieve_operand_emit(sblock, ext, obj_def->operand);
	
	if ( objs->count > 1 ) {	
		(void) sieve_binary_emit_byte(sblock, obj_def->code);
	} 
}

bool sieve_opr_object_read_data
(struct sieve_binary_block *sblock, const struct sieve_operand *operand,
	const struct sieve_operand_class *opclass, sieve_size_t *address,
	struct sieve_object *obj)
{
	const struct sieve_extension_objects *objs;
	unsigned int obj_code; 

	if ( operand == NULL || operand->def->class != opclass )
		return FALSE;
	
	objs = (struct sieve_extension_objects *) operand->def->interface;
	if ( objs == NULL ) 
		return FALSE;
			
	if ( objs->count > 1 ) {
		if ( !sieve_binary_read_byte(sblock, address, &obj_code) ) 
			return FALSE;

		if ( obj_code < objs->count ) {
			const struct sieve_object_def *const *objects = 
				(const struct sieve_object_def *const *) objs->objects;

			obj->def = objects[obj_code];
			obj->ext = operand->ext;
			return TRUE; 
		}
	}
	
	obj->def = (const struct sieve_object_def *) objs->objects; 
	obj->ext = operand->ext;
	return TRUE;
}

bool sieve_opr_object_read
(const struct sieve_runtime_env *renv, 
	const struct sieve_operand_class *opclass, sieve_size_t *address,
	struct sieve_object *obj)
{
	struct sieve_operand operand; 

	if ( !sieve_operand_read(renv->sblock, address, NULL, &operand) ) {
		return FALSE;
	}
	
	return sieve_opr_object_read_data
		(renv->sblock, &operand, opclass, address, obj);
}

bool sieve_opr_object_dump
(const struct sieve_dumptime_env *denv, 
	const struct sieve_operand_class *opclass, sieve_size_t *address,
	struct sieve_object *obj)
{
	struct sieve_operand operand;
	struct sieve_object obj_i;
	const char *class;
	
	if ( obj == NULL )
		obj = &obj_i;

	sieve_code_mark(denv);
	
	if ( !sieve_operand_read(denv->sblock, address, NULL, &operand) ) {
		return FALSE;
	}

	if ( !sieve_opr_object_read_data
		(denv->sblock, &operand, opclass, address, obj) )
		return FALSE;
			
	if ( operand.def->class == NULL )
		class = "OBJECT";
	else
		class = operand.def->class->name;
			
	sieve_code_dumpf(denv, "%s: %s", class, obj->def->identifier);
		
	return TRUE;
}

