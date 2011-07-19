/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-common.h"
#include "sieve-error.h"
#include "sieve-settings.h"

#include <stdlib.h>
#include <ctype.h>

static bool sieve_setting_parse_uint
(struct sieve_instance *svinst, const char *setting, const char *str_value,
	char **endptr, unsigned long long int *value_r)
{
	if ( (*value_r = strtoull(str_value, endptr, 10)) == ULLONG_MAX
		&& errno == ERANGE ) {
		sieve_sys_warning(svinst,
			"overflowing unsigned integer value for setting '%s': '%s'",
			setting, str_value);
		return FALSE;
	}
	
	return TRUE;
}

static bool sieve_setting_parse_int
(struct sieve_instance *svinst, const char *setting, const char *str_value,
	char **endptr, long long int *value_r)
{
	*value_r = strtoll(str_value, endptr, 10);

	if ( *value_r == LLONG_MIN && errno == ERANGE ) {
		sieve_sys_warning(svinst,
			"underflowing integer value for setting '%s': '%s'",
			setting, str_value);
		return FALSE;
	}

	if ( *value_r == LLONG_MAX && errno == ERANGE ) {
		sieve_sys_warning(svinst,
			"overflowing integer value for setting '%s': '%s'",
			setting, str_value);
		return FALSE;
	}
	
	return TRUE;
}

bool sieve_setting_get_uint_value
(struct sieve_instance *svinst, const char *setting,
	unsigned long long int *value_r)
{
	const char *str_value;
	char *endp;

	str_value = sieve_setting_get(svinst, setting);

	if ( str_value == NULL || *str_value == '\0' )
		return FALSE;

	if ( !sieve_setting_parse_uint(svinst, setting, str_value, &endp, value_r) )
		return FALSE;

	if ( *endp != '\0' ) {
		sieve_sys_warning(svinst,
			"invalid unsigned integer value for setting '%s': '%s'",
			setting, str_value);
		return FALSE;
	}
	
	return TRUE;	
}

bool sieve_setting_get_int_value
(struct sieve_instance *svinst, const char *setting,
	long long int *value_r)
{
	const char *str_value;
	char *endp;

	str_value = sieve_setting_get(svinst, setting);

	if ( str_value == NULL || *str_value == '\0' )
		return FALSE;

	if ( !sieve_setting_parse_int(svinst, setting, str_value, &endp, value_r) )
		return FALSE;

	if ( *endp != '\0' ) {
		sieve_sys_warning(svinst, "invalid integer value for setting '%s': '%s'",
			setting, str_value);

		return FALSE;
	}
	
	return TRUE;	
}

bool sieve_setting_get_size_value
(struct sieve_instance *svinst, const char *setting,
	size_t *value_r)
{
	const char *str_value;
	unsigned long long int value, multiply = 1;
	char *endp;

	str_value = sieve_setting_get(svinst, setting);

	if ( str_value == NULL || *str_value == '\0' )
		return FALSE;

	if ( !sieve_setting_parse_uint(svinst, setting, str_value, &endp, &value) )
		return FALSE;

	switch (i_toupper(*endp)) {
	case '\0': /* default */
	case 'B': /* byte (useless) */
		multiply = 1;
		break;
	case 'K': /* kilobyte */
		multiply = 1024;
		break;
	case 'M': /* megabyte */
		multiply = 1024*1024;
		break;
	case 'G': /* gigabyte */
		multiply = 1024*1024*1024;
		break;
	case 'T': /* terabyte */
		multiply = 1024ULL*1024*1024*1024;
		break;
	default:
		sieve_sys_warning(svinst,
			"invalid size value for setting '%s': '%s'",
			setting, str_value);
		return FALSE;
	}

	if ( value > SSIZE_T_MAX / multiply ) {
		sieve_sys_warning(svinst,
			"overflowing size value for setting '%s': '%s'",
			setting, str_value);
		return FALSE;
	}

	*value_r = (size_t) (value * multiply);
	
	return TRUE;
}

bool sieve_setting_get_bool_value
(struct sieve_instance *svinst, const char *setting,
	bool *value_r)
{
	const char *str_value;

	str_value = sieve_setting_get(svinst, setting);

	if ( str_value == NULL || *str_value == '\0' )
		return FALSE;

 	if ( strcasecmp(str_value, "yes" ) == 0) {
        *value_r = TRUE;
		return TRUE;
	}

 	if ( strcasecmp(str_value, "no" ) == 0) {
        *value_r = FALSE;
		return TRUE;
	}

	sieve_sys_warning(svinst, "invalid boolean value for setting '%s': '%s'",
		setting, str_value);
	return FALSE;
}

bool sieve_setting_get_duration_value
(struct sieve_instance *svinst, const char *setting,
	sieve_number_t *value_r)
{
	const char *str_value;
	unsigned long long int value, multiply = 1;
	char *endp;

	str_value = sieve_setting_get(svinst, setting);

	if ( str_value == NULL || *str_value == '\0' )
		return FALSE;

	if ( !sieve_setting_parse_uint(svinst, setting, str_value, &endp, &value) )
		return FALSE;

	switch (i_tolower(*endp)) {
	case '\0': /* default */
	case 's': /* seconds */
		multiply = 1;
		break;
	case 'm': /* minutes */	
		multiply = 60;
		break;
	case 'h': /* hours */
		multiply = 60*60;
		break;
	case 'd': /* days */
		multiply = 24*60*60;
		break;
	default:
		sieve_sys_warning(svinst,
			"invalid duration value for setting '%s': '%s'",
			setting, str_value);
		return FALSE;
	}

	if ( value > SIEVE_MAX_NUMBER / multiply ) {
		sieve_sys_warning(svinst,
			"overflowing duration value for setting '%s': '%s'",
			setting, str_value);
		return FALSE;
	}

	*value_r = (unsigned int) (value * multiply);
	
	return TRUE;
}

