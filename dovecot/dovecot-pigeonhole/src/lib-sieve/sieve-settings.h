/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_SETTINGS_H
#define __SIEVE_SETTINGS_H

#include "sieve-common.h"

/*
 * Settings
 */

static inline const char *sieve_setting_get
(struct sieve_instance *svinst, const char *identifier)
{
	const struct sieve_environment *env = svinst->env;

	if ( env == NULL || env->get_setting == NULL )
		return NULL;

	return env->get_setting(svinst->context, identifier);
}

bool sieve_setting_get_uint_value
	(struct sieve_instance *svinst, const char *setting,
		unsigned long long int *value_r);
bool sieve_setting_get_int_value
	(struct sieve_instance *svinst, const char *setting,
		long long int *value_r);
bool sieve_setting_get_size_value
	(struct sieve_instance *svinst, const char *setting,
		size_t *value_r);
bool sieve_setting_get_bool_value
	(struct sieve_instance *svinst, const char *setting,
		bool *value_r);
bool sieve_setting_get_duration_value
	(struct sieve_instance *svinst, const char *setting,
		sieve_number_t *value_r);

/*
 * Home directory
 */

static inline const char *sieve_environment_get_homedir
(struct sieve_instance *svinst)
{
	const struct sieve_environment *env = svinst->env;

	if ( env == NULL || env->get_homedir == NULL )
		return NULL;

	return env->get_homedir(svinst->context);
}

#endif /* __SIEVE_SETTINGS_H */
