/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "strfuncs.h"

#include "sieve-common.h"
#include "sieve-settings.h"
#include "sieve-error.h"
#include "sieve-extensions.h"
#include "sieve-message.h"
#include "sieve-interpreter.h"
#include "sieve-runtime-trace.h"

#include "ext-spamvirustest-common.h"

#include <sys/types.h>
#include <regex.h>
#include <ctype.h>

/*
 * Extension data
 */

enum ext_spamvirustest_status_type {
	EXT_SPAMVIRUSTEST_STATUS_TYPE_SCORE,
	EXT_SPAMVIRUSTEST_STATUS_TYPE_STRLEN,
	EXT_SPAMVIRUSTEST_STATUS_TYPE_TEXT,
};

struct ext_spamvirustest_header_spec {
	const char *header_name;
	regex_t regexp;
	bool regexp_match;
};

struct ext_spamvirustest_data {
	pool_t pool;

	int reload;

	struct ext_spamvirustest_header_spec status_header;
	struct ext_spamvirustest_header_spec max_header;

	enum ext_spamvirustest_status_type status_type;

	float max_value;

	const char *text_values[11];
};

/*
 * Regexp utility
 */

static bool _regexp_compile
(regex_t *regexp, const char *data, const char **error_r)
{
	size_t errsize;
	int ret;

	*error_r = "";

	if ( (ret=regcomp(regexp, data, REG_EXTENDED)) == 0 ) {
		return TRUE;
	}

	errsize = regerror(ret, regexp, NULL, 0); 

	if ( errsize > 0 ) {
		char *errbuf = t_malloc(errsize);

		(void)regerror(ret, regexp, errbuf, errsize);
	 
		/* We don't want the error to start with a capital letter */
		errbuf[0] = i_tolower(errbuf[0]);

		*error_r = errbuf;
	}

	return FALSE;
}

static const char *_regexp_match_get_value
(const char *string, int index, regmatch_t pmatch[], int nmatch)
{
	if ( index > -1 && index < nmatch && pmatch[index].rm_so != -1 ) {
		return t_strndup(string + pmatch[index].rm_so, 
						pmatch[index].rm_eo - pmatch[index].rm_so);
	}
	return NULL;
}

/*
 * Configuration parser
 */

static bool ext_spamvirustest_header_spec_parse
(struct ext_spamvirustest_header_spec *spec, pool_t pool, const char *data, 
	const char **error_r)
{
	const char *p;
	const char *regexp_error;

	if ( *data == '\0' ) {
		*error_r = "empty header specification";
		return FALSE;
	}

	/* Parse header name */

	p = data;

	while ( *p == ' ' || *p == '\t' ) p++;
	while ( *p != ':' && *p != '\0' && *p != ' ' && *p != '\t' ) p++;

	if ( *p == '\0' ) {
		spec->header_name = p_strdup(pool, data);
		return TRUE;
	}

	spec->header_name = p_strdup_until(pool, data, p);
	while ( *p == ' ' || *p == '\t' ) p++;

	if ( p == '\0' ) {
		spec->regexp_match = FALSE;
		return TRUE;
	}

	/* Parse and compile regular expression */

	if ( *p != ':' ) {
		*error_r = t_strdup_printf("expecting ':', but found '%c'", *p);
		return FALSE;
	}
	p++;
	while ( *p == ' ' || *p == '\t' ) p++;

	spec->regexp_match = TRUE;
	if ( !_regexp_compile(&spec->regexp, p, &regexp_error) ) {
		*error_r = t_strdup_printf("failed to compile regular expression '%s': " 
			"%s", p, regexp_error);
		return FALSE;
	}

	return TRUE;
}

static void ext_spamvirustest_header_spec_free
(struct ext_spamvirustest_header_spec *spec)
{
	regfree(&spec->regexp);
}

static bool ext_spamvirustest_parse_strlen_value
(const char *str_value, float *value_r, const char **error_r)
{
	const char *p = str_value;
	char ch = *p;

	if ( *str_value == '\0' ) {
		*value_r = 0;
		return TRUE;
	}
	
	while ( *p == ch ) p++;

	if ( *p != '\0' ) {
		*error_r = t_strdup_printf(
			"different character '%c' encountered in strlen value",
			*p);
		return FALSE;
	}

	*value_r = ( p - str_value );

	return TRUE;
}

static bool ext_spamvirustest_parse_decimal_value
(const char *str_value, float *value_r, const char **error_r)
{
	const char *p = str_value;
	float value;
	float sign = 1;
	int digits;

	if ( *p == '\0' ) {
		*error_r = "empty value";		
		return FALSE;
	}

	if ( *p == '+' || *p == '-' ) {
		if ( *p == '-' )
			sign = -1;
		
		p++;
	}

	value = 0;
	digits = 0;
	while ( i_isdigit(*p) ) {
		value = value*10 + (*p-'0');
		if ( digits++ > 4 ) {
			*error_r = t_strdup_printf
				("decimal value has too many digits before radix point: %s", 
					str_value);
			return FALSE;	
		}
		p++;
	}

	if ( *p == '.' || *p == ',' ) {
		float radix = .1;
		p++;

		digits = 0;
		while ( i_isdigit(*p) ) {
			value = value + (*p-'0')*radix;

			if ( digits++ > 4 ) {
				*error_r = t_strdup_printf
					("decimal value has too many digits after radix point: %s", 
						str_value);
				return FALSE;
			}
			radix /= 10;
			p++;
		}
	}

	if ( *p != '\0' ) {
		*error_r = t_strdup_printf
			("invalid decimal point value: %s", str_value);
		return FALSE;
	}

	*value_r = value * sign;

	return TRUE;
}

/*
 * Extension initialization
 */

bool ext_spamvirustest_load
(const struct sieve_extension *ext, void **context)
{
	struct ext_spamvirustest_data *ext_data =
		(struct ext_spamvirustest_data *) *context;
	struct sieve_instance *svinst = ext->svinst;
	const char *ext_name, *status_header, *max_header, *status_type, 
		*max_value;
	enum ext_spamvirustest_status_type type;
	const char *error;
	pool_t pool;
	bool result = TRUE;
	int reload = 0;

	if ( *context != NULL ) {
		reload = ext_data->reload + 1;
		ext_spamvirustest_unload(ext);
		*context = NULL;
	}

	/* FIXME: 
	 *   Prevent loading of both spamtest and spamtestplus: let these share 
	 *   contexts.
	 */

	if ( sieve_extension_is(ext, spamtest_extension) || 
		sieve_extension_is(ext, spamtestplus_extension) ) {
		ext_name = spamtest_extension.name;
	} else {
		ext_name = sieve_extension_name(ext);
	}

	/* Get settings */

	status_header = sieve_setting_get
		(svinst, t_strconcat("sieve_", ext_name, "_status_header", NULL));
	status_type = sieve_setting_get
		(svinst, t_strconcat("sieve_", ext_name, "_status_type", NULL));
	max_header = sieve_setting_get
		(svinst, t_strconcat("sieve_", ext_name, "_max_header", NULL));
	max_value = sieve_setting_get
		(svinst, t_strconcat("sieve_", ext_name, "_max_value", NULL));

	/* Base configuration */

	if ( status_header == NULL ) {
		return TRUE;
	}

	if ( status_type == NULL || strcmp(status_type, "score") == 0 ) {
		type = EXT_SPAMVIRUSTEST_STATUS_TYPE_SCORE;
	} else if ( strcmp(status_type, "strlen") == 0 ) {
		type = EXT_SPAMVIRUSTEST_STATUS_TYPE_STRLEN;
	} else if ( strcmp(status_type, "text") == 0 ) {
		type = EXT_SPAMVIRUSTEST_STATUS_TYPE_TEXT;
	} else {
		sieve_sys_error(svinst, 
			"%s: invalid status type '%s'", ext_name, status_type);
		return FALSE;
	}

	/* Verify settings */

	if ( type != EXT_SPAMVIRUSTEST_STATUS_TYPE_TEXT ) {

		if ( max_header != NULL && max_value != NULL ) {
			sieve_sys_error(svinst,
				"%s: sieve_%s_max_header and sieve_%s_max_value "
				"cannot both be configured", ext_name, ext_name, ext_name);
			return TRUE;
		}

		if ( max_header == NULL && max_value == NULL ) {
			sieve_sys_error(svinst,
				"%s: none of sieve_%s_max_header or sieve_%s_max_value "
				"is configured", ext_name, ext_name, ext_name);
			return TRUE;
		}
	} else {
		if ( max_header != NULL ) {
			sieve_sys_warning(svinst,
				"%s: setting sieve_%s_max_header has no meaning "
				"for sieve_%s_status_type=text", ext_name, ext_name, ext_name);
		}

		if ( max_value != NULL ) {
			sieve_sys_warning(svinst,
				"%s: setting sieve_%s_max_value has no meaning "
				"for sieve_%s_status_type=text", ext_name, ext_name, ext_name);
		}	
	}

	pool = pool_alloconly_create("spamvirustest_data", 512);
	ext_data = p_new(pool, struct ext_spamvirustest_data, 1);
	ext_data->pool = pool;
	ext_data->reload = reload;
	ext_data->status_type = type;

	if ( !ext_spamvirustest_header_spec_parse
		(&ext_data->status_header, ext_data->pool, status_header, &error) ) {
		sieve_sys_error(svinst,
			"%s: invalid status header specification "
			"'%s': %s", ext_name, status_header, error);
		result = FALSE;
	}

	if ( result ) {
		if ( type != EXT_SPAMVIRUSTEST_STATUS_TYPE_TEXT ) {
			/* Parse max header */

			if ( max_header != NULL && !ext_spamvirustest_header_spec_parse
				(&ext_data->max_header, ext_data->pool, max_header, &error) ) {
				sieve_sys_error(svinst,
					"%s: invalid max header specification "
					"'%s': %s", ext_name, max_header, error);
				result = FALSE;
			}
	
			/* Parse max value */

			if ( result && max_value != NULL ) {
				if ( !ext_spamvirustest_parse_decimal_value
					(max_value, &ext_data->max_value, &error) ) {
					sieve_sys_error(svinst,
						"%s: invalid max value specification "
						"'%s': %s", ext_name, max_value, error);
					result = FALSE;
				}
			}

		} else {
			unsigned int i, max_text;
		
			max_text = ( sieve_extension_is(ext, virustest_extension) ? 5 : 10 );

			/* Get text values */
			for ( i = 0; i <= max_text; i++ ) {
				const char *value = sieve_setting_get
					(svinst, t_strdup_printf("sieve_%s_text_value%d", ext_name, i));			

				if ( value != NULL && *value != '\0' )
					ext_data->text_values[i] = p_strdup(ext_data->pool, value);
			}

			ext_data->max_value = 1;
		}
	}

	if ( result ) {
		*context = (void *) ext_data;
	} else {
		sieve_sys_warning(svinst, 
			"%s: extension not configured, tests will always match against \"0\"",
			ext_name);
		ext_spamvirustest_unload(ext);
		*context = NULL;
	}

	return result;
}

void ext_spamvirustest_unload(const struct sieve_extension *ext)
{
	struct ext_spamvirustest_data *ext_data = 
		(struct ext_spamvirustest_data *) ext->context;
	
	if ( ext_data != NULL ) {
		ext_spamvirustest_header_spec_free(&ext_data->status_header);
		ext_spamvirustest_header_spec_free(&ext_data->max_header);
		pool_unref(&ext_data->pool);
	}
}

/*
 * Runtime
 */

struct ext_spamvirustest_message_context {
	int reload;
	float score_ratio;
};

static const char *ext_spamvirustest_get_score
(const struct sieve_extension *ext, float score_ratio, bool percent)
{
	int score;

	if ( score_ratio < 0 )
		return "0";

	if ( score_ratio > 1 )
		score_ratio = 1;

	if ( percent )
		score = score_ratio * 100 + 0.001;
	else if ( sieve_extension_is(ext, virustest_extension) )
		score = score_ratio * 4 + 1.001;
	else
		score = score_ratio * 9 + 1.001;

	return t_strdup_printf("%d", score); 
}

const char *ext_spamvirustest_get_value
(const struct sieve_runtime_env *renv, const struct sieve_extension *ext,
	 bool percent)
{
	struct ext_spamvirustest_data *ext_data = 
		(struct ext_spamvirustest_data *) ext->context;	
	struct ext_spamvirustest_header_spec *status_header, *max_header;
	const struct sieve_message_data *msgdata = renv->msgdata;
	struct sieve_message_context *msgctx = renv->msgctx;
	struct ext_spamvirustest_message_context *mctx;
	const char *ext_name = sieve_extension_name(ext);
	regmatch_t match_values[2];
	const char *header_value, *error;
	const char *status = NULL, *max = NULL;
	float status_value, max_value;
	unsigned int i, max_text;
	pool_t pool = sieve_interpreter_pool(renv->interp);

	/* 
	 * Check whether extension is properly configured 
	 */
	if ( ext_data == NULL ) {
		sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, 
			"%s: extension not configured", ext_name);
		return "0";
	}

	/*
	 * Check wether a cached result is available
	 */
	
	mctx = 	(struct ext_spamvirustest_message_context *)
		sieve_message_context_extension_get(msgctx, ext);

	if ( mctx == NULL ) {
		mctx = p_new(pool, struct ext_spamvirustest_message_context, 1);
		sieve_message_context_extension_set(msgctx, ext, (void *)mctx);
	} else if ( mctx->reload == ext_data->reload ) {
		return ext_spamvirustest_get_score(ext, mctx->score_ratio, percent);		
	}

	mctx->reload = ext_data->reload;

	/*
	 * Get max status value
	 */		

	status_header = &ext_data->status_header;
	max_header = &ext_data->max_header;

	if ( ext_data->status_type != EXT_SPAMVIRUSTEST_STATUS_TYPE_TEXT ) {
		if ( max_header->header_name != NULL ) {
			/* Get header from message */
			if ( mail_get_first_header_utf8
				(msgdata->mail, max_header->header_name, &header_value) < 0 ||
				header_value == NULL ) {
				sieve_runtime_trace(renv,  SIEVE_TRLVL_TESTS, 
					"%s: header '%s' not found in message", 
					ext_name, max_header->header_name);
				goto failed;
			} 

			if ( max_header->regexp_match ) {
				/* Execute regex */
				if ( regexec(&max_header->regexp, header_value, 2, match_values, 0) 
					!= 0 ) {
					sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS,
						"%s: regexp for header '%s' did not match "
						"on value '%s'", ext_name, max_header->header_name, header_value);
					goto failed;
				}

				max = _regexp_match_get_value(header_value, 1, match_values, 2);
				if ( max == NULL ) {
					sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS,
						"%s: regexp did not return match value "
						"for string '%s'", ext_name, header_value);
					goto failed;
				}
			} else {
				max = header_value;
			}

			if ( !ext_spamvirustest_parse_decimal_value(max, &max_value, &error) ) {
				sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS,
					"%s: failed to parse maximum value: %s", ext_name, error);
				goto failed;
			}
		} else {
			max_value = ext_data->max_value;
		}

		if ( max_value == 0 ) {
			sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, 
				"%s: max value is 0", ext_name);
			goto failed;
		}
	} else {
		max_value = ( sieve_extension_is(ext, virustest_extension) ? 5 : 10 );
	}

	/*
	 * Get status value
	 */

	/* Get header from message */
	if ( mail_get_first_header_utf8
		(msgdata->mail, status_header->header_name, &header_value) < 0 ||
		header_value == NULL ) {
		sieve_runtime_trace(renv,  SIEVE_TRLVL_TESTS,
			"%s: header '%s' not found in message", 
			ext_name, status_header->header_name);
		goto failed;
	}

	/* Execute regex */
	if ( status_header->regexp_match ) {
		if ( regexec(&status_header->regexp, header_value, 2, match_values, 0) 
			!= 0 ) {
			sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS,
				"%s: regexp for header '%s' did not match on value '%s'",
			ext_name, status_header->header_name, header_value);
			goto failed;
		} 
		
		status = _regexp_match_get_value(header_value, 1, match_values, 2);
		if ( status == NULL ) {
			sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, 
				"%s: regexp did not return match value for string '%s'", 
				ext_name, header_value);
			goto failed;
		}
	} else {
		status = header_value;
	}

	switch ( ext_data->status_type ) {
	case EXT_SPAMVIRUSTEST_STATUS_TYPE_SCORE:
		if ( !ext_spamvirustest_parse_decimal_value
			(status, &status_value, &error) ) {
			sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS,
				"%s: failed to parse status value '%s': %s", 
				ext_name, status, error);
			goto failed;
		}
		break;
	case EXT_SPAMVIRUSTEST_STATUS_TYPE_STRLEN:
		if ( !ext_spamvirustest_parse_strlen_value
			(status, &status_value, &error) ) {
			sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, 
				"%s: failed to parse status value '%s': %s", 
				ext_name, status, error);
			goto failed;
		}
		break;
	case EXT_SPAMVIRUSTEST_STATUS_TYPE_TEXT:
		max_text = ( sieve_extension_is(ext, virustest_extension) ? 5 : 10 );
		status_value = 0;

		i = 0; 
		while ( i <= max_text ) {
			if ( ext_data->text_values[i] != NULL &&
				strcmp(status, ext_data->text_values[i]) == 0 ) {
				status_value = (float) i;
				break;
			}
			i++;
		}

		if ( i > max_text ) {
			sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, 
				"%s: failed to match textstatus value '%s'", 
				ext_name, status);
			goto failed;	
		}		
		break;
	default:
		i_unreached();
		break;
	}
	
	/* Calculate value */
	if ( status_value < 0 )
		mctx->score_ratio = 0;
	else if ( status_value > max_value )
		mctx->score_ratio = 1;
	else
		mctx->score_ratio = (status_value / max_value);

	return ext_spamvirustest_get_score(ext, mctx->score_ratio, percent);

failed:
	mctx->score_ratio = -1;
	return "0";
}


