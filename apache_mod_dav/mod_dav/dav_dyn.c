/*
** Copyright (C) 1998-2000 Greg Stein. All Rights Reserved.
**
** By using this file, you agree to the terms and conditions set forth in
** the LICENSE.html file which can be found at the top level of the mod_dav
** distribution or at http://www.webdav.org/mod_dav/license-1.html.
**
** Contact information:
**   Greg Stein, PO Box 760, Palo Alto, CA, 94302
**   gstein@lyra.org, http://www.webdav.org/mod_dav/
*/

/*
** DAV extension module for Apache 1.3.*
**  - handle dynamic DAV extensions
**
** Written by Greg Stein, gstein@lyra.org, http://www.lyra.org/
*/

#include "mod_dav.h"

#include "http_log.h"

/*
** Hold the runtime information associated with a module.
**
** One of these will exist per loaded module.
*/
typedef struct dav_dyn_runtime
{
    void *handle;		/* DSO handle */
    int index;			/* unique index for module */

    const dav_dyn_module *module;	/* dynamic module info */

    void *m_context;		/* module-level ctx (i.e. managed globals) */

    int num_providers;		/* number of providers in module */
    int **ns_maps;		/* map providers' URIs to global URIs */

    struct dav_dyn_runtime *next;
} dav_dyn_runtime;

/*
** Hold the runtime information associated with a provider.
**
** One per dir/loc per provider.
*/
typedef struct dav_dyn_prov_ctx
{
    int active;		/* is this provider active? */

    const dav_dyn_provider *provider;

} dav_dyn_prov_ctx;

/*
** Hold the runtime information associated with a directory/location and
** a module.
**
** One per dir/loc per module.
*/
typedef struct dav_dyn_mod_ctx
{
    dav_dyn_runtime *runtime;

    int state;				/* rolled up "active" state */
#define DAV_DYN_STATE_UNUSED	0
#define DAV_DYN_STATE_POTENTIAL	1	/* module's params seen */
#define DAV_DYN_STATE_HAS_CTX	2	/* per-dir context exists */
#define DAV_DYN_STATE_ACTIVE	3	/* a provider is active */

    void *d_context;			/* per-directory context */

    dav_dyn_prov_ctx *pc;		/* array of provider context */

    struct dav_dyn_mod_ctx *next;
} dav_dyn_mod_ctx;

typedef struct
{
    pool *p;

    const dav_dyn_module *mod;
    const dav_dyn_runtime *runtime;

    int index;
    const dav_dyn_provider *provider;

} dav_provider_scan_ctx;


/* ### needs thread protection */
static int dav_loaded_count = 0;
static dav_dyn_runtime *dav_loaded_modules = NULL;

/* record the namespaces found in all liveprop providers in all modules */
array_header *dav_liveprop_uris = NULL;


/* builtin, default module for filesystem-based access */
extern const dav_dyn_module dav_dyn_module_default;

typedef struct
{
    const char *name;
    const dav_dyn_module *mod;
} dav_dyn_module_spec;

static const dav_dyn_module_spec specs[] =
{
    { "filesystem", &dav_dyn_module_default },

    /* third party modules are inserted here */

    { NULL }
};


int dav_load_module(const char *name, const char *module_sym,
		    const char *filename)
{
    return 0;
}

const dav_dyn_module *dav_find_module(const char *name)
{
    const dav_dyn_module_spec *spec;

    /* name == NULL means the default. */
    if (name == NULL)
        return &dav_dyn_module_default;

    for (spec = specs; spec->name != NULL; ++spec)
        if (strcasecmp(name, spec->name) == 0)
            return spec->mod;

    /* wasn't found. */
    return NULL;
}

static void dav_cleanup_liveprops(void *ptr)
{
    /* DBG1("dav_cleanup_liveprops (0x%08ld)", (unsigned long)dav_liveprop_uris); */

    dav_liveprop_uris = NULL;
}

/* return a mapping from namespace_uris to dav_liveprop_uris */
int * dav_collect_liveprop_uris(pool *p, const dav_hooks_liveprop *hooks)
{
    int count = 0;
    const char * const * p_uri;
    int *ns_map;
    int *cur_map;

    for (p_uri = hooks->namespace_uris; *p_uri != NULL; ++p_uri)
	++count;
    ns_map = ap_palloc(p, count * sizeof(*ns_map));

    /* clean up this stuff if the pool goes away */
    ap_register_cleanup(p, NULL, dav_cleanup_liveprops, dav_cleanup_liveprops);

    if (dav_liveprop_uris == NULL) {
	dav_liveprop_uris = ap_make_array(p, 5, sizeof(const char *));
	(void) dav_insert_uri(dav_liveprop_uris, "DAV:");
    }

    for (cur_map = ns_map, p_uri = hooks->namespace_uris; *p_uri != NULL; ) {
	*cur_map++ = dav_insert_uri(dav_liveprop_uris, *p_uri++);

	/* DBG2("collect_liveprop: local %d  =>  global %d",
	     p_uri - hooks->namespace_uris - 1, *(cur_map - 1));
	*/
    }

    return ns_map;
}

static void dav_cleanup_module(void *ptr)
{
    const dav_dyn_runtime *ddr = ptr;
    dav_dyn_runtime *scan;

    /* DBG2("cleanup 0x%08lx  (mod=0x%08lx)", (unsigned long)ddr, (unsigned long)ddr->module); */

    --dav_loaded_count;
    /* DBG1("dav_loaded_count=%d", dav_loaded_count); */

    if (ddr == dav_loaded_modules) {
        dav_loaded_modules = dav_loaded_modules->next;
        return;
    }

    for (scan = dav_loaded_modules; scan->next == ddr; scan = scan->next)
        ;
    scan->next = scan->next->next;
}

void dav_process_module(pool *p, const dav_dyn_module *mod)
{
    dav_dyn_runtime *ddr = ap_pcalloc(p, sizeof(*ddr));
    int count = 0;
    const dav_dyn_provider *provider = mod->providers;
    int i;

    for (provider = mod->providers;
	 provider->type != DAV_DYN_TYPE_SENTINEL;
	 ++provider)
	++count;

    ddr->index = ++dav_loaded_count;
    ddr->module = mod;
    ddr->num_providers = count;
    ddr->ns_maps = ap_pcalloc(p, count * sizeof(*ddr->ns_maps));

    ddr->next = dav_loaded_modules;
    dav_loaded_modules = ddr;
    ap_register_cleanup(p, ddr, dav_cleanup_module, dav_cleanup_module);

    /* DBG2("alloc 0x%08lx  (mod=0x%08lx)", (unsigned long)ddr, (unsigned long)mod); */

    for (provider = mod->providers, i = 0;
	 provider->type != DAV_DYN_TYPE_SENTINEL;
	 ++provider, ++i) {

	/* ### null provider? */
	if (provider->hooks == NULL)
	    continue;

	/* process any LIVEPROP providers */
	if (provider->type == DAV_DYN_TYPE_LIVEPROP) {
	    ddr->ns_maps[i] = dav_collect_liveprop_uris(p, DAV_AS_HOOKS_LIVEPROP(provider));
	}
    }

    /* DBG1("dav_loaded_count=%d", dav_loaded_count); */
}

void dav_process_builtin_modules(pool *p)
{
    const dav_dyn_module_spec *spec;

    for (spec = specs; spec->name != NULL; ++spec)
        dav_process_module(p, spec->mod);
}

void *dav_prepare_scan(pool *p, const dav_dyn_module *mod)
{
    dav_provider_scan_ctx *dpsc = ap_pcalloc(p, sizeof(*dpsc));
    const dav_dyn_runtime *ddr;

    /*
    ** create_dir_config is called before init_handler, so we need to
    ** process the builtin modules right now.
    */
    if (dav_loaded_modules == NULL || dav_liveprop_uris == NULL) {
        /* DBG0("reloading during scan"); */
	dav_process_builtin_modules(p);
    }

    for (ddr = dav_loaded_modules; ddr != NULL; ddr = ddr->next) {
	if (ddr->module == mod)
	    break;
    }
    if (ddr == NULL) {
	/* ### we don't know about that module! */
	return NULL;
    }

    dpsc->p = p;
    dpsc->mod = mod;
    dpsc->provider = mod->providers;
    dpsc->runtime = ddr;

    return dpsc;
}

int dav_scan_providers(void *ctx,
		       const dav_dyn_provider **provider,
		       dav_dyn_hooks *output)
{
    dav_provider_scan_ctx *dpsc = ctx;
    int idx;

    *provider = dpsc->provider++;
    if ((*provider)->type == DAV_DYN_TYPE_SENTINEL) {
	return 1;	/* end of list */
    }

    idx = dpsc->index++;

    memset(output, 0, sizeof(*output));
    output->ctx.id = (*provider)->id;
    output->ctx.m_context = dpsc->runtime->m_context;
    output->ctx.ns_map = dpsc->runtime->ns_maps[idx];
    output->hooks = (*provider)->hooks;

    return 0;
}
