/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "mempool.h"
#include "hash.h"
#include "array.h"
#include "str-sanitize.h"

#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-binary.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-runtime-trace.h"

#include "sieve-match.h"

/*
 * Matching implementation
 */

struct sieve_match_context *sieve_match_begin
(const struct sieve_runtime_env *renv,
	const struct sieve_match_type *mcht, 
	const struct sieve_comparator *cmp)
{
	struct sieve_match_context *mctx;
	pool_t pool;

	/* Reject unimplemented match-type */
	if ( mcht->def == NULL || (mcht->def->match == NULL && 
			mcht->def->match_keys == NULL && mcht->def->match_key == NULL) )
			return NULL;

	/* Create match context */
	pool = pool_alloconly_create("sieve_match_context", 1024);
	mctx = p_new(pool, struct sieve_match_context, 1);  
	mctx->pool = pool;
	mctx->runenv = renv;
	mctx->match_type = mcht;
	mctx->comparator = cmp;
	mctx->exec_status = SIEVE_EXEC_OK;
	mctx->trace = sieve_runtime_trace_active(renv, SIEVE_TRLVL_MATCHING);

	/* Trace */
	if ( mctx->trace ) {
		sieve_runtime_trace_descend(renv);
		sieve_runtime_trace(renv, 0,
			"starting `:%s' match with `%s' comparator:",
			sieve_match_type_name(mcht), sieve_comparator_name(cmp));
	}

	/* Initialize match type */
	if ( mcht->def != NULL && mcht->def->match_init != NULL ) {
		mcht->def->match_init(mctx);
	}

	return mctx;
}

int sieve_match_value
(struct sieve_match_context *mctx, const char *value, size_t value_size,
	struct sieve_stringlist *key_list)
{
	const struct sieve_match_type *mcht = mctx->match_type;
	const struct sieve_runtime_env *renv = mctx->runenv;
	int match, ret;

	if ( mctx->trace ) {
		sieve_runtime_trace(renv, 0,
			"matching value `%s'", str_sanitize(value, 80));
	}

	/* Match to key values */
	
	sieve_stringlist_reset(key_list);

	if ( mctx->trace )
		sieve_stringlist_set_trace(key_list, TRUE);

	sieve_runtime_trace_descend(renv);

	if ( mcht->def->match_keys != NULL ) {
		/* Call match-type's own key match handler */
		match = mcht->def->match_keys(mctx, value, value_size, key_list);
	} else {
		string_t *key_item = NULL;

		/* Default key match loop */
		match = 0;
		while ( match == 0 && 
			(ret=sieve_stringlist_next_item(key_list, &key_item)) > 0 ) {				
			T_BEGIN {
				match = mcht->def->match_key
					(mctx, value, value_size, str_c(key_item), str_len(key_item));

				if ( mctx->trace ) {
					sieve_runtime_trace(renv, 0,
						"with key `%s' => %d", str_sanitize(str_c(key_item), 80),
						match);
				}
			} T_END;
		}

		if ( ret < 0 ) {
			mctx->exec_status = key_list->exec_status;
			match = -1;
		}
	}

	sieve_runtime_trace_ascend(renv);

	if ( mctx->match_status < 0 || match < 0 )
		mctx->match_status = -1;
	else 
		mctx->match_status = 
			( mctx->match_status > match ? mctx->match_status : match );
	return match;
}

int sieve_match_end(struct sieve_match_context **mctx, int *exec_status)
{
	const struct sieve_match_type *mcht = (*mctx)->match_type;
	const struct sieve_runtime_env *renv = (*mctx)->runenv;
	int match = (*mctx)->match_status;

	if ( mcht->def != NULL && mcht->def->match_deinit != NULL )
		mcht->def->match_deinit(*mctx);

	if ( exec_status != NULL )
		*exec_status = (*mctx)->exec_status;

	pool_unref(&(*mctx)->pool);

	sieve_runtime_trace(renv, SIEVE_TRLVL_MATCHING,
		"finishing match with result: %s", 
		( match > 0 ? "matched" : ( match < 0 ? "error" : "not matched" ) ));
	sieve_runtime_trace_ascend(renv);

	return match;
}

int sieve_match
(const struct sieve_runtime_env *renv,
	const struct sieve_match_type *mcht, 
	const struct sieve_comparator *cmp, 
	struct sieve_stringlist *value_list,
	struct sieve_stringlist *key_list, 
	int *exec_status)
{
	struct sieve_match_context *mctx;
	string_t *value_item = NULL;
	int match, ret;

	if ( (mctx=sieve_match_begin(renv, mcht, cmp)) == NULL )
		return 0;

	/* Match value to keys */

	sieve_stringlist_reset(value_list);

	if ( mctx->trace )
		sieve_stringlist_set_trace(value_list, TRUE);

	if ( mcht->def->match != NULL ) {
		/* Call match-type's match handler */
		match = mctx->match_status = 
			mcht->def->match(mctx, value_list, key_list); 

	} else {
		/* Default value match loop */

		match = 0;
		while ( match == 0 && 
			(ret=sieve_stringlist_next_item(value_list, &value_item)) > 0 ) {

			match = sieve_match_value
				(mctx, str_c(value_item), str_len(value_item), key_list);
		}

		if ( ret < 0 ) {
			mctx->exec_status = value_list->exec_status;
			match = -1;
		}
	}

	(void)sieve_match_end(&mctx, exec_status);
	return match;
}

/*
 * Reading match operands
 */
 
int sieve_match_opr_optional_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address, int *opt_code)
{
	int _opt_code = 0;
	bool final = FALSE, opok = TRUE;

	if ( opt_code == NULL ) {
		opt_code = &_opt_code;
		final = TRUE;
	}

	while ( opok ) {
		int opt;

		if ( (opt=sieve_opr_optional_dump(denv, address, opt_code)) <= 0 )
			return opt;

		switch ( *opt_code ) {
		case SIEVE_MATCH_OPT_COMPARATOR:
			opok = sieve_opr_comparator_dump(denv, address);
			break;
		case SIEVE_MATCH_OPT_MATCH_TYPE:
			opok = sieve_opr_match_type_dump(denv, address);
			break;
		default:
			return ( final ? -1 : 1 );
		}
	}

	return -1;
}

int sieve_match_opr_optional_read
(const struct sieve_runtime_env *renv, sieve_size_t *address, int *opt_code,
	int *exec_status, struct sieve_comparator *cmp, struct sieve_match_type *mcht)
{
	int _opt_code = 0;
	bool final = FALSE;
	int status = SIEVE_EXEC_OK;

	if ( opt_code == NULL ) {
		opt_code = &_opt_code;
		final = TRUE;
	}

	if ( exec_status != NULL )
		*exec_status = SIEVE_EXEC_OK;			

	while ( status == SIEVE_EXEC_OK ) {
		int opt;

		if ( (opt=sieve_opr_optional_read(renv, address, opt_code)) <= 0 ){
			if ( opt < 0 && exec_status != NULL )
				*exec_status = SIEVE_EXEC_BIN_CORRUPT;				
			return opt;
		}

		switch ( *opt_code ) {
		case SIEVE_MATCH_OPT_COMPARATOR:
			status = sieve_opr_comparator_read(renv, address, cmp);
			break;
		case SIEVE_MATCH_OPT_MATCH_TYPE:
			status = sieve_opr_match_type_read(renv, address, mcht);
			break;
		default:
			if ( final ) {
				sieve_runtime_trace_error(renv, "invalid optional operand");
				if ( exec_status != NULL )
					*exec_status = SIEVE_EXEC_BIN_CORRUPT;
				return -1;
			}
			return 1;
		}
	}

	if ( exec_status != NULL )
		*exec_status = status;	
	return -1;
}

