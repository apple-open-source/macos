/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#ifndef __SIEVE_CODE_H
#define __SIEVE_CODE_H

#include "lib.h"
#include "buffer.h"
#include "mempool.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-runtime.h"
#include "sieve-runtime-trace.h"
#include "sieve-dump.h"

/* 
 * Operand object
 */

struct sieve_operand_class {
	const char *name;
};

struct sieve_operand_def {
	const char *name;
	
	const struct sieve_extension_def *ext_def;
	unsigned int code;
	
	const struct sieve_operand_class *class;
	const void *interface;
};

struct sieve_operand {
	const struct sieve_operand_def *def;
	const struct sieve_extension *ext;
	sieve_size_t address;
	const char *field_name;
};

#define sieve_operand_name(opr) \
	( (opr)->def == NULL ? "(NULL)" : (opr)->def->name )
#define sieve_operand_is(opr, definition) \
	( (opr)->def == &(definition) )

sieve_size_t sieve_operand_emit
	(struct sieve_binary_block *sblock, const struct sieve_extension *ext,
		const struct sieve_operand_def *oprnd);
bool sieve_operand_read
	(struct sieve_binary_block *sblock, sieve_size_t *address, 
		const char *field_name, struct sieve_operand *oprnd);

static inline int sieve_operand_runtime_read
(const struct sieve_runtime_env *renv, sieve_size_t *address,
	const char *field_name, struct sieve_operand *operand)
{
	if ( !sieve_operand_read(renv->sblock, address, field_name, operand) ) {
		sieve_runtime_trace_operand_error(renv, operand, "invalid operand");
		return SIEVE_EXEC_BIN_CORRUPT;
	}

	return SIEVE_EXEC_OK;
}

/*
 * Optional operands
 */

int sieve_opr_optional_next
(struct sieve_binary_block *sblock, sieve_size_t *address, 
	signed int *opt_code);

static inline int sieve_opr_optional_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address, 
	signed int *opt_code)
{
	sieve_size_t pc = *address;
	int ret;

	if ( (ret=sieve_opr_optional_next(denv->sblock, address, opt_code)) <= 0 )
		return ret;

	sieve_code_mark_specific(denv, pc);
	return ret; 
}

static inline int sieve_opr_optional_read
(const struct sieve_runtime_env *renv, sieve_size_t *address, 
	signed int *opt_code)
{
	int ret;

	if ( (ret=sieve_opr_optional_next(renv->sblock, address, opt_code)) < 0 )
		sieve_runtime_trace_error(renv, "invalid optional operand code");

	return ret;
}

/*
 * Core operands
 */
 
/* Operand codes */

enum sieve_core_operand {
	SIEVE_OPERAND_OPTIONAL = 0x00,
	SIEVE_OPERAND_NUMBER,
	SIEVE_OPERAND_STRING,
	SIEVE_OPERAND_STRING_LIST,
	SIEVE_OPERAND_COMPARATOR,
	SIEVE_OPERAND_MATCH_TYPE,
	SIEVE_OPERAND_ADDRESS_PART,
	SIEVE_OPERAND_CATENATED_STRING,

	SIEVE_OPERAND_CUSTOM
};

/* Operand classes */

extern const struct sieve_operand_class number_class;
extern const struct sieve_operand_class string_class;
extern const struct sieve_operand_class stringlist_class;

/* Operand objects */

extern const struct sieve_operand_def omitted_operand;
extern const struct sieve_operand_def number_operand;
extern const struct sieve_operand_def string_operand;
extern const struct sieve_operand_def stringlist_operand;
extern const struct sieve_operand_def catenated_string_operand;

extern const struct sieve_operand_def *sieve_operands[];
extern const unsigned int sieve_operand_count;

/* Operand object interfaces */

struct sieve_opr_number_interface {
	bool (*dump)	
		(const struct sieve_dumptime_env *denv, const struct sieve_operand *oprnd,
			sieve_size_t *address);
	int (*read)
	  (const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd,
			sieve_size_t *address, sieve_number_t *number_r);
};

struct sieve_opr_string_interface {
	bool (*dump)
		(const struct sieve_dumptime_env *denv, const struct sieve_operand *oprnd, 
			sieve_size_t *address);
	int (*read)
		(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd, 
		 	sieve_size_t *address, string_t **str_r);
};

struct sieve_opr_stringlist_interface {
	bool (*dump)
		(const struct sieve_dumptime_env *denv, const struct sieve_operand *oprnd,
			sieve_size_t *address);
	int (*read)
		(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd,
			sieve_size_t *address, struct sieve_stringlist **strlist_r);
};

/* 
 * Core operand functions 
 */

/* Omitted */

void sieve_opr_omitted_emit(struct sieve_binary_block *sblock);

static inline bool sieve_operand_is_omitted
(const struct sieve_operand *operand)
{
	return ( operand != NULL && operand->def != NULL &&
		operand->def == &omitted_operand );
}

/* Number */

void sieve_opr_number_emit
	(struct sieve_binary_block *sblock, sieve_number_t number);
bool sieve_opr_number_dump_data	
	(const struct sieve_dumptime_env *denv, struct sieve_operand *operand,
		sieve_size_t *address, const char *field_name); 
bool sieve_opr_number_dump	
	(const struct sieve_dumptime_env *denv, sieve_size_t *address,
		const char *field_name); 
int sieve_opr_number_read_data
	(const struct sieve_runtime_env *renv, struct sieve_operand *operand,
		sieve_size_t *address, const char *field_name, sieve_number_t *number_r);
int sieve_opr_number_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address, 
		const char *field_name, sieve_number_t *number_r);

static inline bool sieve_operand_is_number
(const struct sieve_operand *operand)
{
	return ( operand != NULL && operand->def != NULL && 
		operand->def->class == &number_class );
}

/* String */

void sieve_opr_string_emit
	(struct sieve_binary_block *sblock, string_t *str);
bool sieve_opr_string_dump_data
	(const struct sieve_dumptime_env *denv, struct sieve_operand *operand,
		sieve_size_t *address, const char *field_name); 
bool sieve_opr_string_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address,
		const char *field_name); 
bool sieve_opr_string_dump_ex
	(const struct sieve_dumptime_env *denv, sieve_size_t *address, 
		const char *field_name, bool *literal_r); 
int sieve_opr_string_read_data
	(const struct sieve_runtime_env *renv, struct sieve_operand *operand,
		sieve_size_t *address, const char *field_name, string_t **str_r);
int sieve_opr_string_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address, 
		const char *field_name, string_t **str_r);
int sieve_opr_string_read_ex
	(const struct sieve_runtime_env *renv, sieve_size_t *address, 
		const char *field_name, string_t **str_r, bool *literal_r);

static inline bool sieve_operand_is_string
(const struct sieve_operand *operand)
{
	return ( operand != NULL && operand->def != NULL &&
		operand->def->class == &string_class );
}

static inline bool sieve_operand_is_string_literal
(const struct sieve_operand *operand)
{
	return ( operand != NULL && sieve_operand_is(operand, string_operand) );
}

/* String list */

void sieve_opr_stringlist_emit_start
	(struct sieve_binary_block *sblock, unsigned int listlen, void **context);
void sieve_opr_stringlist_emit_item
	(struct sieve_binary_block *sblock, void *context ATTR_UNUSED,
		string_t *item);
void sieve_opr_stringlist_emit_end
	(struct sieve_binary_block *sblock, void *context);
bool sieve_opr_stringlist_dump_data
	(const struct sieve_dumptime_env *denv, struct sieve_operand *operand, 
		sieve_size_t *address, const char *field_name);
bool sieve_opr_stringlist_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address,
		const char *field_name);
int sieve_opr_stringlist_read_data
	(const struct sieve_runtime_env *renv, struct sieve_operand *operand, 
		sieve_size_t *address, const char *field_name, 
		struct sieve_stringlist **strlist_r);
int sieve_opr_stringlist_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address,
		const char *field_name, struct sieve_stringlist **strlist_r);

static inline bool sieve_operand_is_stringlist
(const struct sieve_operand *operand)
{
	return ( operand != NULL && operand->def != NULL &&
		(operand->def->class == &stringlist_class || 
			operand->def->class == &string_class) );
}

/* Catenated string */

void sieve_opr_catenated_string_emit
	(struct sieve_binary_block *sblock, unsigned int elements);
	
/*
 * Operation object
 */
 
struct sieve_operation_def {
	const char *mnemonic;
	
	const struct sieve_extension_def *ext_def;
	unsigned int code;
	
	bool (*dump)
		(const struct sieve_dumptime_env *denv, sieve_size_t *address);
	int (*execute)
		(const struct sieve_runtime_env *renv, sieve_size_t *address);
};

struct sieve_operation {
	const struct sieve_operation_def *def;
	const struct sieve_extension *ext;

	sieve_size_t address;
};

#define sieve_operation_is(oprtn, definition) \
	( (oprtn)->def == &(definition) )
#define sieve_operation_mnemonic(oprtn) \
	( (oprtn)->def == NULL ? "(NULL)" : (oprtn)->def->mnemonic )

sieve_size_t sieve_operation_emit
	(struct sieve_binary_block *sblock, const struct sieve_extension *ext,
		const struct sieve_operation_def *op_def);	
bool sieve_operation_read
	(struct sieve_binary_block *sblock, sieve_size_t *address,
		struct sieve_operation *oprtn);
const char *sieve_operation_read_string
	(struct sieve_binary_block *sblock, sieve_size_t *address);

/* 
 * Core operations 
 */

/* Opcodes */

enum sieve_operation_code {
	SIEVE_OPERATION_INVALID,
	SIEVE_OPERATION_JMP,
	SIEVE_OPERATION_JMPTRUE,
	SIEVE_OPERATION_JMPFALSE,
	
	SIEVE_OPERATION_STOP,
	SIEVE_OPERATION_KEEP,
	SIEVE_OPERATION_DISCARD,
	SIEVE_OPERATION_REDIRECT,
	
	SIEVE_OPERATION_ADDRESS,
	SIEVE_OPERATION_HEADER, 
	SIEVE_OPERATION_EXISTS, 
	SIEVE_OPERATION_SIZE_OVER,
	SIEVE_OPERATION_SIZE_UNDER,
	
	SIEVE_OPERATION_CUSTOM
};

/* Operation objects */

extern const struct sieve_operation_def sieve_jmp_operation;
extern const struct sieve_operation_def sieve_jmptrue_operation;
extern const struct sieve_operation_def sieve_jmpfalse_operation; 

extern const struct sieve_operation_def *sieve_operations[];
extern const unsigned int sieve_operations_count;

#endif
