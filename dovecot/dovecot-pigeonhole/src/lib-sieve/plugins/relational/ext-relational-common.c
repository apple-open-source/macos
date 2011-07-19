/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Syntax:
 *   MATCH-TYPE =/ COUNT / VALUE
 *   COUNT = ":count" relational-match
 *   VALUE = ":value" relational-match
 *   relational-match = DQUOTE ( "gt" / "ge" / "lt"
 *                             / "le" / "eq" / "ne" ) DQUOTE
 */ 

#include "lib.h"
#include "str.h"
#include "str-sanitize.h"

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

#include "ext-relational-common.h"

/*
 * Forward declarations
 */

const struct sieve_match_type_def *rel_match_types[];

/* 
 * Validation 
 */

bool mcht_relational_validate
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_match_type_context *ctx)
{	
	struct sieve_match_type *mcht;
	enum relational_match rel_match = REL_MATCH_INVALID;
	string_t *rel_match_ident;

	/* Check syntax:
	 *   relational-match = DQUOTE ( "gt" / "ge" / "lt"	
 	 *                             / "le" / "eq" / "ne" ) DQUOTE
 	 *
	 * So, actually this must be a constant string and it is implemented as such 
	 */
	 
	/* Did we get a string in the first place ? */ 
	if ( (*arg)->type != SAAT_STRING ) {
		sieve_argument_validate_error(valdtr, *arg, 
			"the :%s match-type requires a constant string argument being "
			"one of \"gt\", \"ge\", \"lt\", \"le\", \"eq\" or \"ne\", "
			"but %s was found", 
			sieve_match_type_name(ctx->match_type), sieve_ast_argument_name(*arg));
		return FALSE;
	}
	
	/* Check the relational match id */
	
	rel_match_ident = sieve_ast_argument_str(*arg);
	if ( str_len(rel_match_ident) == 2 ) {
		const char *rel_match_id = str_c(rel_match_ident);

		switch ( rel_match_id[0] ) {
		/* "gt" or "ge" */
		case 'g':
			switch ( rel_match_id[1] ) {
			case 't': 
				rel_match = REL_MATCH_GREATER; 
				break;
			case 'e': 
				rel_match = REL_MATCH_GREATER_EQUAL; 
				break;
			default: 
				rel_match = REL_MATCH_INVALID;
			}
			break;
		/* "lt" or "le" */
		case 'l':
			switch ( rel_match_id[1] ) {
			case 't': 
				rel_match = REL_MATCH_LESS; 
				break;
			case 'e': 
				rel_match = REL_MATCH_LESS_EQUAL; 
				break;
			default: 
				rel_match = REL_MATCH_INVALID;
			}
			break;
		/* "eq" */
		case 'e':
			if ( rel_match_id[1] == 'q' )
				rel_match = REL_MATCH_EQUAL;
			else	
				rel_match = REL_MATCH_INVALID;		
			break;
		/* "ne" */
		case 'n':
			if ( rel_match_id[1] == 'e' )
				rel_match = REL_MATCH_NOT_EQUAL;
			else	
				rel_match = REL_MATCH_INVALID;
			break;
		/* invalid */
		default:
			rel_match = REL_MATCH_INVALID;
		}
	}
	
	if ( rel_match >= REL_MATCH_INVALID ) {
		sieve_argument_validate_error(valdtr, *arg, 
			"the :%s match-type requires a constant string argument being "
			"one of \"gt\", \"ge\", \"lt\", \"le\", \"eq\" or \"ne\", "
			"but \"%s\" was found", 
			sieve_match_type_name(ctx->match_type), 
			str_sanitize(str_c(rel_match_ident), 32));
		return FALSE;
	}
	
	/* Delete argument */
	*arg = sieve_ast_arguments_detach(*arg, 1);

	/* Not used just yet */
	ctx->ctx_data = (void *) rel_match;

	/* Override the actual match type with a parameter-specific one 
	 * FIXME: ugly!
	 */
	mcht = p_new(sieve_ast_argument_pool(*arg), struct sieve_match_type, 1);
	mcht->object.ext = ctx->match_type->object.ext;
	SIEVE_OBJECT_SET_DEF(mcht, rel_match_types
		[REL_MATCH_INDEX(ctx->match_type->object.def->code, rel_match)]);
	ctx->match_type = mcht;

	return TRUE;
}

/*
 * Relational match-type operand
 */

const const struct sieve_match_type_def *rel_match_types[] = {
    &rel_match_value_gt, &rel_match_value_ge, &rel_match_value_lt,
    &rel_match_value_le, &rel_match_value_eq, &rel_match_value_ne,
    &rel_match_count_gt, &rel_match_count_ge, &rel_match_count_lt,
    &rel_match_count_le, &rel_match_count_eq, &rel_match_count_ne
};

static const struct sieve_extension_objects ext_match_types =
	SIEVE_EXT_DEFINE_MATCH_TYPES(rel_match_types);

const struct sieve_operand_def rel_match_type_operand = {
    "relational match",
    &relational_extension,
    0,
    &sieve_match_type_operand_class,
    &ext_match_types
};

