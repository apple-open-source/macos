/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"

#include "sieve-common.h"

#include "sieve-ast.h"
#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-match.h"

#include "ext-relational-common.h"

/*
 * Match-type objects
 */

const struct sieve_match_type_def value_match_type = {
	SIEVE_OBJECT("value", &rel_match_type_operand, RELATIONAL_VALUE), 
	mcht_relational_validate,
	NULL, NULL, NULL, NULL, NULL, NULL
};

#define VALUE_MATCH_TYPE(name, rel_match)                       \
const struct sieve_match_type_def rel_match_value_ ## name = {  \
	SIEVE_OBJECT(                                                 \
		"value-" #name, &rel_match_type_operand,                    \
		REL_MATCH_INDEX(RELATIONAL_VALUE, rel_match)),              \
	NULL, NULL, NULL, NULL, NULL,                                 \
	mcht_value_match_key,                                         \
	NULL                                                          \
}

VALUE_MATCH_TYPE(gt, REL_MATCH_GREATER);
VALUE_MATCH_TYPE(ge, REL_MATCH_GREATER_EQUAL); 
VALUE_MATCH_TYPE(lt, REL_MATCH_LESS);
VALUE_MATCH_TYPE(le, REL_MATCH_LESS_EQUAL); 
VALUE_MATCH_TYPE(eq, REL_MATCH_EQUAL);
VALUE_MATCH_TYPE(ne, REL_MATCH_NOT_EQUAL);

/* 
 * Match-type implementation 
 */

int mcht_value_match_key
(struct sieve_match_context *mctx, const char *val, size_t val_size, 
	const char *key, size_t key_size)
{
	const struct sieve_match_type *mtch = mctx->match_type;
	unsigned int rel_match = REL_MATCH(mtch->object.def->code);	
	int cmp_result;

	cmp_result = mctx->comparator->def->
		compare(mctx->comparator, val, val_size, key, key_size);

	switch ( rel_match ) {
	case REL_MATCH_GREATER:
		return ( cmp_result > 0 );
	case REL_MATCH_GREATER_EQUAL:
		return ( cmp_result >= 0 );
	case REL_MATCH_LESS:
		return ( cmp_result < 0 );
	case REL_MATCH_LESS_EQUAL:
		return ( cmp_result <= 0 );
	case REL_MATCH_EQUAL:
		return ( cmp_result == 0 );
	case REL_MATCH_NOT_EQUAL:
		return ( cmp_result != 0 );
	case REL_MATCH_INVALID:
 	default:
		break;
	}	
	
	return FALSE;
}


