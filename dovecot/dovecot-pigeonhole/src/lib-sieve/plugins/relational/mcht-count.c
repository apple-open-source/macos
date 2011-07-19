/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
/* Match-type ':count' 
 */

#include "lib.h"
#include "str.h"
#include "str-sanitize.h"

#include "sieve-common.h"
#include "sieve-ast.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-runtime-trace.h"
#include "sieve-match.h"

#include "ext-relational-common.h"

/* 
 * Forward declarations
 */

static int mcht_count_match
	(struct sieve_match_context *mctx, struct sieve_stringlist *value_list,
		struct sieve_stringlist *key_list);

/* 
 * Match-type objects
 */
 
const struct sieve_match_type_def count_match_type = {
	SIEVE_OBJECT("count", &rel_match_type_operand, RELATIONAL_COUNT),
	mcht_relational_validate,
	NULL, NULL, NULL, NULL, NULL, NULL
};

#define COUNT_MATCH_TYPE(name, rel_match)                      \
const struct sieve_match_type_def rel_match_count_ ## name = { \
	SIEVE_OBJECT(                                                \
		"count-" #name, &rel_match_type_operand,                   \
		REL_MATCH_INDEX(RELATIONAL_COUNT, rel_match)),             \
	NULL, NULL,                                                  \
	mcht_count_match,                                            \
	NULL, NULL, NULL, NULL                                       \
}
	
COUNT_MATCH_TYPE(gt, REL_MATCH_GREATER);
COUNT_MATCH_TYPE(ge, REL_MATCH_GREATER_EQUAL);
COUNT_MATCH_TYPE(lt, REL_MATCH_LESS);
COUNT_MATCH_TYPE(le, REL_MATCH_LESS_EQUAL);
COUNT_MATCH_TYPE(eq, REL_MATCH_EQUAL);
COUNT_MATCH_TYPE(ne, REL_MATCH_NOT_EQUAL);

/* 
 * Match-type implementation 
 */

static int mcht_count_match
(struct sieve_match_context *mctx, struct sieve_stringlist *value_list,
	struct sieve_stringlist *key_list)
{
	const struct sieve_runtime_env *renv = mctx->runenv;
	bool trace = sieve_runtime_trace_active(renv, SIEVE_TRLVL_MATCHING);
	int count;
	string_t *key_item;
	int match, ret;

	if ( (count=sieve_stringlist_get_length(value_list)) < 0 ) {
		mctx->exec_status = value_list->exec_status;
		return -1;
	}

	sieve_stringlist_reset(key_list);

	string_t *value = t_str_new(20);
	str_printfa(value, "%d", count);

	if ( trace ) {
		sieve_runtime_trace(renv, 0,
			"matching count value `%s'", str_sanitize(str_c(value), 80));
	}

	sieve_runtime_trace_descend(renv);

  /* Match to all key values */
  key_item = NULL;
	match = 0;
  while ( match == 0 && 
		(ret=sieve_stringlist_next_item(key_list, &key_item)) > 0 )
  {
		match = mcht_value_match_key
			(mctx, str_c(value), str_len(value), str_c(key_item), str_len(key_item));

		if ( trace ) {
			sieve_runtime_trace(renv, 0,
				"with key `%s' => %d", str_sanitize(str_c(key_item), 80), ret);
		}
	}

	sieve_runtime_trace_ascend(renv);

	if ( ret < 0 ) {
		mctx->exec_status = key_list->exec_status;
		match = -1;
	}

	return match;
}





