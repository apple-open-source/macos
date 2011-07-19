/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "array.h"
#include "hostpid.h"
#include "var-expand.h"
#include "settings-parser.h"
#include "master-service.h"
#include "master-service-settings.h"
#include "master-service-settings-cache.h"

#include "sieve.h"

#include "managesieve-capabilities.h"

#include <stddef.h>
#include <unistd.h>

/*
 * Global plugin settings
 */

struct plugin_settings {
	ARRAY_DEFINE(plugin_envs, const char *);
};

static const struct setting_parser_info **plugin_set_roots;

#undef DEF
#define DEF(type, name) \
	{ type, #name, offsetof(struct plugin_settings, name), NULL }

static const struct setting_define plugin_setting_defines[] = {
	{ SET_STRLIST, "plugin", offsetof(struct plugin_settings, plugin_envs), NULL },

	SETTING_DEFINE_LIST_END
};

static const struct setting_parser_info plugin_setting_parser_info = {
	.module_name = "managesieve",
	.defines = plugin_setting_defines,

	.type_offset = (size_t)-1,
	.struct_size = sizeof(struct plugin_settings),

	.parent_offset = (size_t)-1,
};

static const struct setting_parser_info *default_plugin_set_roots[] = {
	&plugin_setting_parser_info,
	NULL
};

static const struct setting_parser_info **plugin_set_roots = 
	default_plugin_set_roots;

static struct plugin_settings *plugin_settings_read(void)
{
	const char *error;

	if (master_service_settings_read_simple(master_service, plugin_set_roots, &error) < 0)
		i_fatal("Error reading configuration: %s", error);

	return (struct plugin_settings *)
		master_service_settings_get_others(master_service)[0];
}

static const char *plugin_settings_get
	(const struct plugin_settings *set, const char *identifier)
{
	const char *const *envs;
	unsigned int i, count;

	if ( !array_is_created(&set->plugin_envs) )
		return NULL;

	envs = array_get(&set->plugin_envs, &count);
	for ( i = 0; i < count; i += 2 ) {
		if ( strcmp(envs[i], identifier) == 0 )
			return envs[i+1];
	}

	return NULL;
}

/*
 * Sieve environment
 */

static const char *sieve_get_homedir(void *context ATTR_UNUSED)
{
	return "/tmp";
}

static const char *sieve_get_setting
(void *context, const char *identifier)
{
	const struct plugin_settings *set = (const struct plugin_settings *) context;

  return plugin_settings_get(set, identifier);
}

static const struct sieve_environment sieve_env = {
	sieve_get_homedir,
	sieve_get_setting
};

/*
 * Capability dumping
 */

void managesieve_capabilities_dump(void)
{
	const struct plugin_settings *global_plugin_settings;
	struct sieve_instance *svinst;
	const char *notify_cap;
	
	/* Read plugin settings */

	global_plugin_settings = plugin_settings_read();

	/* Initialize Sieve engine */

	svinst = sieve_init(&sieve_env, (void *) global_plugin_settings, FALSE);

	/* Dump capabilities */

	notify_cap = sieve_get_capabilities(svinst, "notify");

	if ( notify_cap == NULL ) 
		printf("SIEVE: %s\n", sieve_get_capabilities(svinst, NULL));
	else
		printf("SIEVE: %s, NOTIFY: %s\n", sieve_get_capabilities(svinst, NULL),
			sieve_get_capabilities(svinst, "notify"));
}
