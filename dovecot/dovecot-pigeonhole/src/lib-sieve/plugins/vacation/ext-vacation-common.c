/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-common.h"
#include "sieve-error.h"
#include "sieve-settings.h"
#include "sieve-extensions.h"

#include "ext-vacation-common.h"

bool ext_vacation_load
(const struct sieve_extension *ext, void **context)
{
	struct sieve_instance *svinst = ext->svinst;
	struct ext_vacation_config *config;
	sieve_number_t min_period, max_period, default_period;

	if ( *context != NULL ) {
		ext_vacation_unload(ext);
	}

	if ( !sieve_setting_get_duration_value
		(svinst, "sieve_vacation_min_period", &min_period) ) {
		min_period = EXT_VACATION_DEFAULT_MIN_PERIOD;
	}

	if ( !sieve_setting_get_duration_value
		(svinst, "sieve_vacation_max_period", &max_period) ) {
		max_period = EXT_VACATION_DEFAULT_MAX_PERIOD;
	}

	if ( !sieve_setting_get_duration_value
		(svinst, "sieve_vacation_default_period", &default_period) ) {
		default_period = EXT_VACATION_DEFAULT_PERIOD;
	}

	if ( max_period > 0 
		&& (min_period > max_period || default_period < min_period
			|| default_period > max_period) ) {
		min_period = EXT_VACATION_DEFAULT_MIN_PERIOD;
		max_period = EXT_VACATION_DEFAULT_MAX_PERIOD;
		default_period = EXT_VACATION_DEFAULT_PERIOD;

		sieve_sys_warning(svinst,
			"vacation extension: invalid settings: violated "
			"sieve_vacation_min_period < sieve_vacation_default_period < "
			"sieve_vacation_max_period");
	}
	
	config = i_new(struct ext_vacation_config, 1);
	config->min_period = min_period;
	config->max_period = max_period;
	config->default_period = default_period;

	*context = (void *) config;

	return TRUE;
}

void ext_vacation_unload
(const struct sieve_extension *ext)
{
	struct ext_vacation_config *config =
		(struct ext_vacation_config *) ext->context;

	i_free(config);
}
