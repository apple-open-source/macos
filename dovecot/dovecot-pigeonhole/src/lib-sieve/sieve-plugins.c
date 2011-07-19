/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#include "lib.h"
#include "str.h"
#include "module-dir.h"

#include "sieve-settings.h"
#include "sieve-extensions.h"

#include "sieve-common.h"
#include "sieve-plugins.h"

/*
 * Types
 */

typedef void (*sieve_plugin_load_func_t)
	(struct sieve_instance *svinst, void **context);
typedef void (*sieve_plugin_unload_func_t)
	(struct sieve_instance *svinst, void *context);

struct sieve_plugin {
	struct module *module;
	
	void *context;

	struct sieve_plugin *next;
};

/*
 * Plugin support
 */

static struct module *sieve_modules = NULL;
static int sieve_modules_refcount = 0;

static struct module *sieve_plugin_module_find(const char *name)
{
	struct module *module;

	module = sieve_modules;
    while ( module != NULL ) {
		const char *mod_name;
		
		/* Strip module names */

		mod_name = module_get_plugin_name(module);
		
		if ( strcmp(mod_name, name) == 0 )
			return module;

		module = module->next;
    }

    return NULL;
}

void sieve_plugins_load(struct sieve_instance *svinst, const char *path, const char *plugins)
{
	struct module *new_modules, *module;
	struct module_dir_load_settings mod_set;
	const char **module_names;
	unsigned int i;

	/* Determine what to load */

	if ( path == NULL && plugins == NULL ) {
		path = sieve_setting_get(svinst, "sieve_plugin_dir");
		plugins = sieve_setting_get(svinst, "sieve_plugins");
	}

	if ( plugins == NULL || *plugins == '\0' )
		return;
	
	if ( path == NULL || *path == '\0' )
		path = MODULEDIR"/sieve";

	memset(&mod_set, 0, sizeof(mod_set));
	mod_set.version = PIGEONHOLE_VERSION;
	mod_set.require_init_funcs = TRUE;
	mod_set.debug = FALSE;

	/* Load missing plugin modules */

	new_modules = module_dir_load_missing
		(sieve_modules, path, plugins, &mod_set);

	if ( sieve_modules == NULL ) {
		/* No modules loaded yet */
		sieve_modules = new_modules;
	} else {
		/* Find the end of the list */
		module = sieve_modules;
		while ( module != NULL && module->next != NULL )
			module = module->next;

		/* Add newly loaded modules */
		module->next = new_modules;
	}

	/* Call plugin load functions for this Sieve instance */

	if ( svinst->plugins == NULL ) {
		sieve_modules_refcount++;
	}

	module_names = t_strsplit_spaces(plugins, ", ");

	for (i = 0; module_names[i] != NULL; i++) {
		/* Allow giving the module names also in non-base form. */
		module_names[i] = module_file_get_name(module_names[i]);
	}

 	for (i = 0; module_names[i] != NULL; i++) {
		struct sieve_plugin *plugin;
		const char *name = module_names[i];
		sieve_plugin_load_func_t load_func;

		/* Find the module */
		module = sieve_plugin_module_find(name);
		i_assert(module != NULL);

		/* Check whether the plugin is already loaded in this instance */
		plugin = svinst->plugins;
		while ( plugin != NULL ) {
			if ( plugin->module == module )
				break;
			plugin = plugin->next;
		}

		/* Skip it if it is loaded already */
		if ( plugin != NULL )
			continue;

		/* Create plugin list item */
		plugin = p_new(svinst->pool, struct sieve_plugin, 1);
		plugin->module = module;
	
		/* Call load function */
		load_func = (sieve_plugin_load_func_t) module_get_symbol
			(module, t_strdup_printf("%s_load", module->name));
		if ( load_func != NULL ) {
			load_func(svinst, &plugin->context);
		}

		/* Add plugin to the instance */
		if ( svinst->plugins == NULL )
			svinst->plugins = plugin;
		else {
			struct sieve_plugin *plugin_last;

			plugin_last = svinst->plugins;
			while ( plugin_last->next != NULL )
				plugin_last = plugin_last->next;

			plugin_last->next = plugin;
		}
	}
}

void sieve_plugins_unload(struct sieve_instance *svinst)
{
	struct sieve_plugin *plugin;

	if ( svinst->plugins == NULL )
		return;
	
	/* Call plugin unload functions for this instance */

	plugin = svinst->plugins;
	while ( plugin != NULL ) {
		struct module *module = plugin->module;
		sieve_plugin_unload_func_t unload_func;

		unload_func = (sieve_plugin_unload_func_t)module_get_symbol
			(module, t_strdup_printf("%s_unload", module->name));
		if ( unload_func != NULL ) {
			unload_func(svinst, plugin->context);
		}

		plugin = plugin->next;
	}

	/* Physically unload modules */

	i_assert(sieve_modules_refcount > 0);

	if ( --sieve_modules_refcount != 0 )
        return;

	module_dir_unload(&sieve_modules);
}

