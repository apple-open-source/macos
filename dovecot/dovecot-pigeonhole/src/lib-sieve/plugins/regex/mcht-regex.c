/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Match-type ':regex'
 */

#include "lib.h"
#include "mempool.h"
#include "buffer.h"
#include "array.h"
#include "str.h"
#include "str-sanitize.h"

#include "sieve-common.h"
#include "sieve-limits.h"
#include "sieve-ast.h"
#include "sieve-stringlist.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-interpreter.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-match.h"

#include "ext-regex-common.h"

#include <sys/types.h>
#include <regex.h>
#include <ctype.h>

/*
 * Configuration
 */

#define MCHT_REGEX_MAX_SUBSTITUTIONS SIEVE_MAX_MATCH_VALUES

/* 
 * Match type
 */
 
static bool mcht_regex_validate_context
(struct sieve_validator *valdtr, struct sieve_ast_argument *arg,
    struct sieve_match_type_context *ctx, struct sieve_ast_argument *key_arg);

static void mcht_regex_match_init(struct sieve_match_context *mctx);
static int mcht_regex_match_keys
	(struct sieve_match_context *mctx, const char *val, size_t val_size,
    struct sieve_stringlist *key_list);
static void mcht_regex_match_deinit(struct sieve_match_context *mctx);

const struct sieve_match_type_def regex_match_type = {
	SIEVE_OBJECT("regex", &regex_match_type_operand, 0),
	NULL,
	mcht_regex_validate_context,
	NULL,
	mcht_regex_match_init,
	mcht_regex_match_keys,
	NULL, 
	mcht_regex_match_deinit
};

/* 
 * Match type validation 
 */

/* Wrapper around the regerror function for easy access */
static const char *_regexp_error(regex_t *regexp, int errorcode)
{
	size_t errsize = regerror(errorcode, regexp, NULL, 0); 

	if ( errsize > 0 ) {
		char *errbuf;

		buffer_t *error_buf = 
			buffer_create_dynamic(pool_datastack_create(), errsize);
		errbuf = buffer_get_space_unsafe(error_buf, 0, errsize);

		errsize = regerror(errorcode, regexp, errbuf, errsize);
	 
		/* We don't want the error to start with a capital letter */
		errbuf[0] = i_tolower(errbuf[0]);

		buffer_append_space_unsafe(error_buf, errsize);

		return str_c(error_buf);
	}

	return "";
}

static int mcht_regex_validate_regexp
(struct sieve_validator *valdtr, 
	struct sieve_match_type_context *mtctx ATTR_UNUSED,
	struct sieve_ast_argument *key, int cflags) 
{
	int ret;
	regex_t regexp;
	const char *regex_str = sieve_ast_argument_strc(key);

	if ( (ret=regcomp(&regexp, regex_str, cflags)) != 0 ) {
		sieve_argument_validate_error(valdtr, key,
			"invalid regular expression '%s' for regex match: %s", 
			str_sanitize(regex_str, 128), _regexp_error(&regexp, ret));

		regfree(&regexp);	
		return FALSE;
	}

	regfree(&regexp);
	return TRUE;
}

struct _regex_key_context {
	struct sieve_validator *valdtr;
	struct sieve_match_type_context *mtctx;
	int cflags;
};

static int mcht_regex_validate_key_argument
(void *context, struct sieve_ast_argument *key)
{
	struct _regex_key_context *keyctx = (struct _regex_key_context *) context;

	/* FIXME: We can currently only handle string literal argument, so
	 * variables are not allowed.
	 */
	if ( sieve_argument_is_string_literal(key) ) {
		return mcht_regex_validate_regexp
			(keyctx->valdtr, keyctx->mtctx, key, keyctx->cflags);
	}

	return TRUE;
}
	
static bool mcht_regex_validate_context
(struct sieve_validator *valdtr, struct sieve_ast_argument *arg ATTR_UNUSED,
	struct sieve_match_type_context *mtctx, struct sieve_ast_argument *key_arg)
{
	const struct sieve_comparator *cmp = mtctx->comparator;
	int cflags = REG_EXTENDED | REG_NOSUB;
	struct _regex_key_context keyctx;
	struct sieve_ast_argument *kitem;

	if ( cmp != NULL ) { 
		if ( sieve_comparator_is(cmp, i_ascii_casemap_comparator) )
			cflags =  REG_EXTENDED | REG_NOSUB | REG_ICASE;
		else if ( sieve_comparator_is(cmp, i_octet_comparator) )
			cflags =  REG_EXTENDED | REG_NOSUB;
		else {
			sieve_argument_validate_error(valdtr, mtctx->argument, 
				"regex match type only supports "
				"i;octet and i;ascii-casemap comparators" );
			return FALSE;	
		}
	}

	/* Validate regular expression keys */

	keyctx.valdtr = valdtr;
	keyctx.mtctx = mtctx;
	keyctx.cflags = cflags;

	kitem = key_arg;
	if ( !sieve_ast_stringlist_map(&kitem, (void *) &keyctx,
		mcht_regex_validate_key_argument) )
		return FALSE;

	return TRUE;
}

/* 
 * Match type implementation 
 */

struct mcht_regex_key {
	regex_t regexp;
	int status;
};

struct mcht_regex_context {
	ARRAY_DEFINE(reg_expressions, struct mcht_regex_key);
	regmatch_t *pmatch;
	size_t nmatch;
	unsigned int all_compiled:1;
};

static void mcht_regex_match_init
(struct sieve_match_context *mctx)
{
	pool_t pool = mctx->pool;
	struct mcht_regex_context *ctx;

	/* Create context */	
	ctx = p_new(pool, struct mcht_regex_context, 1);

	/* Create storage for match values if match values are requested */
	if ( sieve_match_values_are_enabled(mctx->runenv) ) {
		ctx->pmatch = p_new(pool, regmatch_t, MCHT_REGEX_MAX_SUBSTITUTIONS);
		ctx->nmatch = MCHT_REGEX_MAX_SUBSTITUTIONS;
	} else {
		ctx->pmatch = NULL;
		ctx->nmatch = 0;
	}
	
	/* Assign context */
	mctx->data = (void *) ctx;
}

static int mcht_regex_match_key
(struct sieve_match_context *mctx, const char *val,
	const regex_t *regexp)
{
	struct mcht_regex_context *ctx = (struct mcht_regex_context *) mctx->data;
	int ret;

	/* Execute regex */

	ret = regexec(regexp, val, ctx->nmatch, ctx->pmatch, 0);

	/* Handle match values if necessary */

	if ( ret == 0 ) {
		if ( ctx->nmatch > 0 ) {
			struct sieve_match_values *mvalues;
			size_t i;
			int skipped = 0;
			string_t *subst = t_str_new(32);

			/* Start new list of match values */
			mvalues = sieve_match_values_start(mctx->runenv);

			i_assert( mvalues != NULL );

			/* Add match values from regular expression */
			for ( i = 0; i < ctx->nmatch; i++ ) {
				str_truncate(subst, 0);
	
				if ( ctx->pmatch[i].rm_so != -1 ) {
					if ( skipped > 0 ) {
						sieve_match_values_skip(mvalues, skipped);
						skipped = 0;
					}
			
					str_append_n(subst, val + ctx->pmatch[i].rm_so, 
						ctx->pmatch[i].rm_eo - ctx->pmatch[i].rm_so);
					sieve_match_values_add(mvalues, subst);
				} else 
					skipped++;
			}

			/* Substitute the new match values */
			sieve_match_values_commit(mctx->runenv, &mvalues);
		}

		return 1;
	}

	return 0;
}

static int mcht_regex_match_keys
(struct sieve_match_context *mctx, const char *val, size_t val_size ATTR_UNUSED, 
	struct sieve_stringlist *key_list)
{
	const struct sieve_runtime_env *renv = mctx->runenv;
	bool trace = sieve_runtime_trace_active(renv, SIEVE_TRLVL_MATCHING);
	struct mcht_regex_context *ctx = (struct mcht_regex_context *) mctx->data;
	const struct sieve_comparator *cmp = mctx->comparator;
	int match;

	if ( !ctx->all_compiled ) {
		string_t *key_item = NULL;
		unsigned int i;
		int ret;

		/* Regular expressions still need to be compiled */

		if ( !array_is_created(&ctx->reg_expressions) )
			p_array_init(&ctx->reg_expressions, mctx->pool, 16);

		i = 0;
		match = 0;
		while ( match == 0 &&
			(ret=sieve_stringlist_next_item(key_list, &key_item)) > 0 ) {				

			T_BEGIN {
				struct mcht_regex_key *rkey;

				if ( i >= array_count(&ctx->reg_expressions) ) {
					int cflags;

					rkey = array_append_space(&ctx->reg_expressions);

					/* Configure case-sensitivity according to comparator */
					if ( sieve_comparator_is(cmp, i_octet_comparator) ) 
						cflags =  REG_EXTENDED;
					else if ( sieve_comparator_is(cmp, i_ascii_casemap_comparator) )
						cflags =  REG_EXTENDED | REG_ICASE;
					else
						rkey->status = -1; /* Not supported */
		
					if ( rkey->status >= 0 ) {
						const char *regex_str = str_c(key_item);
						int rxret;

						/* Indicate whether match values need to be produced */
						if ( ctx->nmatch == 0 ) cflags |= REG_NOSUB;

						/* Compile regular expression */
						if ( (rxret=regcomp(&rkey->regexp, regex_str, cflags)) != 0 ) {
							sieve_runtime_error(renv, NULL,
								"invalid regular expression '%s' for regex match: %s", 
								str_sanitize(regex_str, 128),
								_regexp_error(&rkey->regexp, rxret));
							rkey->status = -1;
						} else {
							rkey->status = 1;
						}
					}
				} else {
					rkey = array_idx_modifiable(&ctx->reg_expressions, 1);
				} 

				if ( rkey->status > 0 ) {
					match = mcht_regex_match_key(mctx, val, &rkey->regexp);

					if ( trace ) {
						sieve_runtime_trace(renv, 0,
							"with regex `%s' [id=%d] => %d", 
							str_sanitize(str_c(key_item), 80),
							array_count(&ctx->reg_expressions)-1, match);
					}
				}
			} T_END;

			i++;
		}

		if ( ret == 0 ) {
			ctx->all_compiled = TRUE;
		} else if ( ret < 0 ) {
			mctx->exec_status = key_list->exec_status;
			match = -1;
		}

	} else {
		const struct mcht_regex_key *rkeys;
		unsigned int i, count;

		/* Regular expressions are compiled */

		rkeys = array_get(&ctx->reg_expressions, &count);

		i = 0;
		match = 0;
		while ( match == 0 && i < count ) {
			if ( rkeys[i].status > 0 ) {
				match = mcht_regex_match_key(mctx, val, &rkeys[i].regexp);

				if ( trace ) {
					sieve_runtime_trace(renv, 0,
						"with compiled regex [id=%d] => %d", i, match);
				}
			}

			i++;
		}
	}

	return match;
}

void mcht_regex_match_deinit
(struct sieve_match_context *mctx)
{
	struct mcht_regex_context *ctx = (struct mcht_regex_context *) mctx->data;
	struct mcht_regex_key *rkeys;
	unsigned int count, i;

	/* Clean up compiled regular expressions */
	if ( array_is_created(&ctx->reg_expressions) ) {
		rkeys = array_get_modifiable(&ctx->reg_expressions, &count);
		for ( i = 0; i < count; i++ ) {
			regfree(&rkeys[i].regexp);
		}
	}
}

