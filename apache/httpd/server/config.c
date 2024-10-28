/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * http_config.c: once was auxiliary functions for reading httpd's config
 * file and converting filenames into a namespace
 *
 * Rob McCool
 *
 * Wall-to-wall rewrite for Apache... commands which are part of the
 * server core can now be found next door in "http_core.c".  Now contains
 * general command loop, and functions which do bookkeeping for the new
 * Apache config stuff (modules and configuration vectors).
 *
 * rst
 *
 */

#include "apr.h"
#include "apr_strings.h"
#include "apr_portable.h"
#include "apr_file_io.h"
#include "apr_fnmatch.h"

#define APR_WANT_STDIO
#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_core.h"
#include "http_log.h"      /* for errors in parse_htaccess */
#include "http_request.h"  /* for default_handler (see invoke_handler) */
#include "http_main.h"
#include "http_vhost.h"
#include "util_cfgtree.h"
#include "util_varbuf.h"
#include "mpm_common.h"

#define APLOG_UNSET   (APLOG_NO_MODULE - 1)
/* we know core's module_index is 0 */
#undef APLOG_MODULE_INDEX
#define APLOG_MODULE_INDEX AP_CORE_MODULE_INDEX

AP_DECLARE_DATA const char *ap_server_argv0 = NULL;
AP_DECLARE_DATA const char *ap_server_root = NULL;
AP_DECLARE_DATA server_rec *ap_server_conf = NULL;
AP_DECLARE_DATA apr_pool_t *ap_pglobal = NULL;

AP_DECLARE_DATA apr_array_header_t *ap_server_pre_read_config = NULL;
AP_DECLARE_DATA apr_array_header_t *ap_server_post_read_config = NULL;
AP_DECLARE_DATA apr_array_header_t *ap_server_config_defines = NULL;

AP_DECLARE_DATA ap_directive_t *ap_conftree = NULL;

APR_HOOK_STRUCT(
           APR_HOOK_LINK(header_parser)
           APR_HOOK_LINK(pre_config)
           APR_HOOK_LINK(check_config)
           APR_HOOK_LINK(post_config)
           APR_HOOK_LINK(open_logs)
           APR_HOOK_LINK(child_init)
           APR_HOOK_LINK(handler)
           APR_HOOK_LINK(quick_handler)
           APR_HOOK_LINK(optional_fn_retrieve)
           APR_HOOK_LINK(test_config)
           APR_HOOK_LINK(open_htaccess)
)

AP_IMPLEMENT_HOOK_RUN_ALL(int, header_parser,
                          (request_rec *r), (r), OK, DECLINED)

AP_IMPLEMENT_HOOK_RUN_ALL(int, pre_config,
                          (apr_pool_t *pconf, apr_pool_t *plog,
                           apr_pool_t *ptemp),
                          (pconf, plog, ptemp), OK, DECLINED)

AP_IMPLEMENT_HOOK_RUN_ALL(int, check_config,
                          (apr_pool_t *pconf, apr_pool_t *plog,
                           apr_pool_t *ptemp, server_rec *s),
                          (pconf, plog, ptemp, s), OK, DECLINED)

AP_IMPLEMENT_HOOK_VOID(test_config,
                       (apr_pool_t *pconf, server_rec *s),
                       (pconf, s))

AP_IMPLEMENT_HOOK_RUN_ALL(int, post_config,
                          (apr_pool_t *pconf, apr_pool_t *plog,
                           apr_pool_t *ptemp, server_rec *s),
                          (pconf, plog, ptemp, s), OK, DECLINED)

/* During the course of debugging I expanded this macro out, so
 * rather than remove all the useful information there is in the
 * following lines, I'm going to leave it here in case anyone
 * else finds it useful.
 *
 * Ben has looked at it and thinks it correct :)
 *
AP_DECLARE(int) ap_hook_post_config(ap_HOOK_post_config_t *pf,
                                    const char * const *aszPre,
                                    const char * const *aszSucc,
                                    int nOrder)
{
    ap_LINK_post_config_t *pHook;

    if (!_hooks.link_post_config) {
        _hooks.link_post_config = apr_array_make(apr_hook_global_pool, 1,
                                                 sizeof(ap_LINK_post_config_t));
        apr_hook_sort_register("post_config", &_hooks.link_post_config);
    }

    pHook = apr_array_push(_hooks.link_post_config);
    pHook->pFunc = pf;
    pHook->aszPredecessors = aszPre;
    pHook->aszSuccessors = aszSucc;
    pHook->nOrder = nOrder;
    pHook->szName = apr_hook_debug_current;

    if (apr_hook_debug_enabled)
        apr_hook_debug_show("post_config", aszPre, aszSucc);
}

AP_DECLARE(apr_array_header_t *) ap_hook_get_post_config(void)
{
    return _hooks.link_post_config;
}

AP_DECLARE(int) ap_run_post_config(apr_pool_t *pconf,
                                   apr_pool_t *plog,
                                   apr_pool_t *ptemp,
                                   server_rec *s)
{
    ap_LINK_post_config_t *pHook;
    int n;

    if (!_hooks.link_post_config)
        return;

    pHook = (ap_LINK_post_config_t *)_hooks.link_post_config->elts;
    for (n = 0; n < _hooks.link_post_config->nelts; ++n)
        pHook[n].pFunc (pconf, plog, ptemp, s);
}
 */

AP_IMPLEMENT_HOOK_RUN_ALL(int, open_logs,
                          (apr_pool_t *pconf, apr_pool_t *plog,
                           apr_pool_t *ptemp, server_rec *s),
                          (pconf, plog, ptemp, s), OK, DECLINED)

AP_IMPLEMENT_HOOK_VOID(child_init,
                       (apr_pool_t *pchild, server_rec *s),
                       (pchild, s))

AP_IMPLEMENT_HOOK_RUN_FIRST(int, handler, (request_rec *r),
                            (r), DECLINED)

AP_IMPLEMENT_HOOK_RUN_FIRST(int, quick_handler, (request_rec *r, int lookup),
                            (r, lookup), DECLINED)

AP_IMPLEMENT_HOOK_RUN_FIRST(apr_status_t, open_htaccess,
                            (request_rec *r, const char *dir_name, const char *access_name,
                             ap_configfile_t **conffile, const char **full_name),
                            (r, dir_name, access_name, conffile, full_name),
                            AP_DECLINED)

/* hooks with no args are implemented last, after disabling APR hook probes */
#if defined(APR_HOOK_PROBES_ENABLED)
#undef APR_HOOK_PROBES_ENABLED
#undef APR_HOOK_PROBE_ENTRY
#define APR_HOOK_PROBE_ENTRY(ud,ns,name,args)
#undef APR_HOOK_PROBE_RETURN
#define APR_HOOK_PROBE_RETURN(ud,ns,name,rv,args)
#undef APR_HOOK_PROBE_INVOKE
#define APR_HOOK_PROBE_INVOKE(ud,ns,name,src,args)
#undef APR_HOOK_PROBE_COMPLETE
#define APR_HOOK_PROBE_COMPLETE(ud,ns,name,src,rv,args)
#undef APR_HOOK_INT_DCL_UD
#define APR_HOOK_INT_DCL_UD
#endif
AP_IMPLEMENT_HOOK_VOID(optional_fn_retrieve, (void), ())

/****************************************************************
 *
 * We begin with the functions which deal with the linked list
 * of modules which control just about all of the server operation.
 */

/* total_modules is the number of modules that have been linked
 * into the server.
 */
static int total_modules = 0;

/* dynamic_modules is the number of modules that have been added
 * after the pre-loaded ones have been set up. It shouldn't be larger
 * than DYNAMIC_MODULE_LIMIT.
 */
static int dynamic_modules = 0;

/* The maximum possible value for total_modules, i.e. number of static
 * modules plus DYNAMIC_MODULE_LIMIT.
 */
static int max_modules = 0;

/* The number of elements we need to alloc for config vectors. Before loading
 * of dynamic modules, we must be liberal and set this to max_modules. After
 * loading of dynamic modules, we can trim it down to total_modules. On
 * restart, reset to max_modules.
 */
static int conf_vector_length = 0;

static int reserved_module_slots = 0;

AP_DECLARE_DATA module *ap_top_module = NULL;
AP_DECLARE_DATA module **ap_loaded_modules=NULL;

static apr_hash_t *ap_config_hash = NULL;

/* a list of the module symbol names with the trailing "_module"removed */
static char **ap_module_short_names = NULL;

typedef int (*handler_func)(request_rec *);
typedef void *(*dir_maker_func)(apr_pool_t *, char *);
typedef void *(*merger_func)(apr_pool_t *, void *, void *);

/* A list of the merge_dir_config functions of all loaded modules, sorted
 * by module_index.
 * Using this list in ap_merge_per_dir_configs() is faster than following
 * the module->next linked list because of better memory locality (resulting
 * in better cache usage).
 */
static merger_func *merger_func_cache;

/* maximum nesting level for config directories */
#ifndef AP_MAX_INCLUDE_DIR_DEPTH
#define AP_MAX_INCLUDE_DIR_DEPTH (128)
#endif

/* Dealing with config vectors.  These are associated with per-directory,
 * per-server, and per-request configuration, and have a void* pointer for
 * each modules.  The nature of the structure pointed to is private to the
 * module in question... the core doesn't (and can't) know.  However, there
 * are defined interfaces which allow it to create instances of its private
 * per-directory and per-server structures, and to merge the per-directory
 * structures of a directory and its subdirectory (producing a new one in
 * which the defaults applying to the base directory have been properly
 * overridden).
 */

static ap_conf_vector_t *create_empty_config(apr_pool_t *p)
{
    void *conf_vector = apr_pcalloc(p, sizeof(void *) * conf_vector_length);
    return conf_vector;
}

static ap_conf_vector_t *create_default_per_dir_config(apr_pool_t *p)
{
    void **conf_vector = apr_pcalloc(p, sizeof(void *) * conf_vector_length);
    module *modp;

    for (modp = ap_top_module; modp; modp = modp->next) {
        dir_maker_func df = modp->create_dir_config;

        if (df)
            conf_vector[modp->module_index] = (*df)(p, NULL);
    }

    return (ap_conf_vector_t *)conf_vector;
}

AP_CORE_DECLARE(ap_conf_vector_t *) ap_merge_per_dir_configs(apr_pool_t *p,
                                           ap_conf_vector_t *base,
                                           ap_conf_vector_t *new_conf)
{
    void **conf_vector = apr_palloc(p, sizeof(void *) * conf_vector_length);
    void **base_vector = (void **)base;
    void **new_vector = (void **)new_conf;
    int i;

    for (i = 0; i < total_modules; i++) {
        if (!new_vector[i]) {
            conf_vector[i] = base_vector[i];
        }
        else {
            const merger_func df = merger_func_cache[i];
            if (df && base_vector[i]) {
                conf_vector[i] = (*df)(p, base_vector[i], new_vector[i]);
            }
            else
                conf_vector[i] = new_vector[i];
        }
    }

    return (ap_conf_vector_t *)conf_vector;
}

static ap_conf_vector_t *create_server_config(apr_pool_t *p, server_rec *s)
{
    void **conf_vector = apr_pcalloc(p, sizeof(void *) * conf_vector_length);
    module *modp;

    for (modp = ap_top_module; modp; modp = modp->next) {
        if (modp->create_server_config)
            conf_vector[modp->module_index] = (*modp->create_server_config)(p, s);
    }

    return (ap_conf_vector_t *)conf_vector;
}

static void merge_server_configs(apr_pool_t *p, ap_conf_vector_t *base,
                                 server_rec *virt)
{
    /* Can reuse the 'virt' vector for the spine of it, since we don't
     * have to deal with the moral equivalent of .htaccess files here...
     */

    void **base_vector = (void **)base;
    void **virt_vector = (void **)virt->module_config;
    module *modp;

    for (modp = ap_top_module; modp; modp = modp->next) {
        merger_func df = modp->merge_server_config;
        int i = modp->module_index;

        if (!virt_vector[i]) {
            if (df && modp->create_server_config
                   && (ap_get_module_flags(modp) &
                       AP_MODULE_FLAG_ALWAYS_MERGE)) {
                virt_vector[i] = (*modp->create_server_config)(p, virt);
            }
            else {
                virt_vector[i] = base_vector[i];
                df = NULL;
            }
        }
        if (df) {
            virt_vector[i] = (*df)(p, base_vector[i], virt_vector[i]);
        }
    }
}

AP_CORE_DECLARE(ap_conf_vector_t *) ap_create_request_config(apr_pool_t *p)
{
    return create_empty_config(p);
}

AP_CORE_DECLARE(ap_conf_vector_t *) ap_create_conn_config(apr_pool_t *p)
{
    return create_empty_config(p);
}

AP_CORE_DECLARE(ap_conf_vector_t *) ap_create_per_dir_config(apr_pool_t *p)
{
    return create_empty_config(p);
}

/* Invoke the filter_init_func for all filters with FILTERS where f->r
 * matches R.  Restricting to a matching R avoids re-running init
 * functions for filters configured for r->main where r is a
 * subrequest.  */
static int invoke_filter_init(request_rec *r, ap_filter_t *filters)
{
    while (filters) {
        if (filters->frec->filter_init_func && filters->r == r) {
            int result = filters->frec->filter_init_func(filters);
            if (result != OK) {
                return result;
            }
        }
        filters = filters->next;
    }
    return OK;
}

AP_CORE_DECLARE(int) ap_invoke_handler(request_rec *r)
{
    const char *handler;
    const char *p;
    int result;
    const char *old_handler = r->handler;
    const char *ignore;

    /*
     * The new insert_filter stage makes the most sense here.  We only use
     * it when we are going to run the request, so we must insert filters
     * if any are available.  Since the goal of this phase is to allow all
     * modules to insert a filter if they want to, this filter returns
     * void.  I just can't see any way that this filter can reasonably
     * fail, either your modules inserts something or it doesn't.  rbb
     */
    ap_run_insert_filter(r);

    /* Before continuing, allow each filter that is in the two chains to
     * run their init function to let them do any magic before we could
     * start generating data.
     */
    result = invoke_filter_init(r, r->input_filters);
    if (result != OK) {
        return result;
    }
    result = invoke_filter_init(r, r->output_filters);
    if (result != OK) {
        return result;
    }

    if (!r->handler) {
        if (r->content_type && AP_REQUEST_IS_TRUSTED_CT(r)) {
            handler = r->content_type;
            if ((p=ap_strchr_c(handler, ';')) != NULL) {
                char *new_handler = (char *)apr_pmemdup(r->pool, handler,
                                                        p - handler + 1);
                char *p2 = new_handler + (p - handler);
                handler = new_handler;

                /* exclude media type arguments */
                while (p2 > handler && p2[-1] == ' ')
                    --p2; /* strip trailing spaces */

                *p2='\0';
            }
        }
        else {
            handler = AP_DEFAULT_HANDLER_NAME;
        }

        r->handler = handler;
    }

    result = ap_run_handler(r);

    r->handler = old_handler;

    if (result == DECLINED && r->handler && r->filename) {
        ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, APLOGNO(00523)
            "handler \"%s\" not found for: %s", r->handler, r->filename);
    }
    if ((result != OK) && (result != DONE) && (result != DECLINED) && (result != SUSPENDED)
        && (result != AP_FILTER_ERROR) /* ap_die() knows about this specifically */
        && !ap_is_HTTP_VALID_RESPONSE(result)) {
        /* If a module is deliberately returning something else
         * (request_rec in non-HTTP or proprietary extension?)
         * let it set a note to allow it explicitly.
         * Otherwise, a return code that is neither reserved nor HTTP
         * is a bug, as in PR#31759.
         */
        ignore = apr_table_get(r->notes, "HTTP_IGNORE_RANGE");
        if (!ignore) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, APLOGNO(00524)
                          "Handler for %s returned invalid result code %d",
                          r->handler, result);
            result = HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    return result == DECLINED ? HTTP_INTERNAL_SERVER_ERROR : result;
}

AP_DECLARE(int) ap_method_is_limited(cmd_parms *cmd, const char *method)
{
    int methnum;

    methnum = ap_method_number_of(method);

    /*
     * A method number either hardcoded into apache or
     * added by a module and registered.
     */
    if (methnum != M_INVALID) {
        return (cmd->limited & (AP_METHOD_BIT << methnum)) ? 1 : 0;
    }

    return 0; /* not found */
}

AP_DECLARE(void) ap_register_hooks(module *m, apr_pool_t *p)
{
    if (m->register_hooks) {
        if (getenv("SHOW_HOOKS")) {
            printf("Registering hooks for %s\n", m->name);
            apr_hook_debug_enabled = 1;
        }

        apr_hook_debug_current = m->name;
        m->register_hooks(p);
    }
}

static void ap_add_module_commands(module *m, apr_pool_t *p);

typedef struct ap_mod_list_struct ap_mod_list;
struct ap_mod_list_struct {
    struct ap_mod_list_struct *next;
    module *m;
    const command_rec *cmd;
};

static void rebuild_conf_hash(apr_pool_t *p, int add_prelinked)
{
    module **m;

    ap_config_hash = apr_hash_make(p);

    apr_pool_cleanup_register(p, &ap_config_hash, ap_pool_cleanup_set_null,
                              apr_pool_cleanup_null);
    if (add_prelinked) {
        for (m = ap_prelinked_modules; *m != NULL; m++) {
            ap_add_module_commands(*m, p);
        }
    }
}

static void ap_add_module_commands(module *m, apr_pool_t *p)
{
    apr_pool_t *tpool;
    ap_mod_list *mln;
    const command_rec *cmd;
    char *dir;

    cmd = m->cmds;

    if (ap_config_hash == NULL) {
        rebuild_conf_hash(p, 0);
    }

    tpool = apr_hash_pool_get(ap_config_hash);

    while (cmd && cmd->name) {
        mln = apr_palloc(tpool, sizeof(ap_mod_list));
        mln->cmd = cmd;
        mln->m = m;
        dir = apr_pstrdup(tpool, cmd->name);

        ap_str_tolower(dir);

        mln->next = apr_hash_get(ap_config_hash, dir, APR_HASH_KEY_STRING);
        apr_hash_set(ap_config_hash, dir, APR_HASH_KEY_STRING, mln);
        ++cmd;
    }
}


/* One-time setup for precompiled modules --- NOT to be done on restart */

AP_DECLARE(const char *) ap_add_module(module *m, apr_pool_t *p,
                                       const char *sym_name)
{
    ap_module_symbol_t *sym = ap_prelinked_module_symbols;

    /* This could be called from a LoadModule httpd.conf command,
     * after the file has been linked and the module structure within it
     * teased out...
     */

    if (m->version != MODULE_MAGIC_NUMBER_MAJOR) {
        return apr_psprintf(p, "Module \"%s\" is not compatible with this "
                            "version of Apache (found %d, need %d). Please "
                            "contact the vendor for the correct version.",
                            m->name, m->version, MODULE_MAGIC_NUMBER_MAJOR);
    }

    if (m->module_index == -1) {
        if (dynamic_modules >= DYNAMIC_MODULE_LIMIT) {
            return apr_psprintf(p, "Module \"%s\" could not be loaded, "
                                "because the dynamic module limit was "
                                "reached. Please increase "
                                "DYNAMIC_MODULE_LIMIT and recompile.", m->name);
        }
        /*
         * If this fails some module forgot to call ap_reserve_module_slots*.
         */
        ap_assert(total_modules < conf_vector_length);

        m->module_index = total_modules++;
        dynamic_modules++;

    }
    else if (!sym_name) {
        while (sym->modp != NULL) {
            if (sym->modp == m) {
                sym_name = sym->name;
                break;
            }
            sym++;
        }
    }

    if (m->next == NULL) {
        m->next = ap_top_module;
        ap_top_module = m;
    }

    if (sym_name) {
        int len = strlen(sym_name);
        int slen = strlen("_module");
        if (len > slen && !strcmp(sym_name + len - slen, "_module")) {
            len -= slen;
        }

        ap_module_short_names[m->module_index] = ap_malloc(len + 1);
        memcpy(ap_module_short_names[m->module_index], sym_name, len);
        ap_module_short_names[m->module_index][len] = '\0';
        merger_func_cache[m->module_index] = m->merge_dir_config;
    }


    /* Some C compilers put a complete path into __FILE__, but we want
     * only the filename (e.g. mod_includes.c). So check for path
     * components (Unix and DOS), and remove them.
     */

    if (ap_strrchr_c(m->name, '/'))
        m->name = 1 + ap_strrchr_c(m->name, '/');

    if (ap_strrchr_c(m->name, '\\'))
        m->name = 1 + ap_strrchr_c(m->name, '\\');

#ifdef _OSD_POSIX
    /* __FILE__ =
     * "*POSIX(/home/martin/apache/src/modules/standard/mod_info.c)"
     */

    /* We cannot fix the string in-place, because it's const */
    if (m->name[strlen(m->name)-1] == ')') {
        char *tmp = ap_malloc(strlen(m->name)); /* FIXME: memory leak, albeit a small one */
        memcpy(tmp, m->name, strlen(m->name)-1);
        tmp[strlen(m->name)-1] = '\0';
        m->name = tmp;
    }
#endif /*_OSD_POSIX*/

    ap_add_module_commands(m, p);
    /*  FIXME: is this the right place to call this?
     *  It doesn't appear to be
     */
    ap_register_hooks(m, p);

    return NULL;
}

/*
 * remove_module undoes what add_module did. There are some caveats:
 * when the module is removed, its slot is lost so all the current
 * per-dir and per-server configurations are invalid. So we should
 * only ever call this function when you are invalidating almost
 * all our current data. I.e. when doing a restart.
 */

AP_DECLARE(void) ap_remove_module(module *m)
{
    module *modp;

    modp = ap_top_module;
    if (modp == m) {
        /* We are the top module, special case */
        ap_top_module = modp->next;
        m->next = NULL;
    }
    else {
        /* Not the top module, find use. When found modp will
         * point to the module _before_ us in the list
         */

        while (modp && modp->next != m) {
            modp = modp->next;
        }

        if (!modp) {
            /* Uh-oh, this module doesn't exist */
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL, APLOGNO(00525)
                         "Cannot remove module %s: not found in module list",
                         m->name);
            return;
        }

        /* Eliminate us from the module list */
        modp->next = modp->next->next;
    }

    free(ap_module_short_names[m->module_index]);
    ap_module_short_names[m->module_index] = NULL;
    merger_func_cache[m->module_index] = NULL;

    m->module_index = -1; /* simulate being unloaded, should
                           * be unnecessary */
    dynamic_modules--;
    total_modules--;
}

AP_DECLARE(const char *) ap_add_loaded_module(module *mod, apr_pool_t *p,
                                              const char *short_name)
{
    module **m;
    const char *error;

    /*
     *  Add module pointer to top of chained module list
     */
    error = ap_add_module(mod, p, short_name);
    if (error) {
        return error;
    }

    /*
     *  And module pointer to list of loaded modules
     *
     *  Notes: 1. ap_add_module() would already complain if no more space
     *            exists for adding a dynamically loaded module
     *         2. ap_add_module() accepts double inclusion, so we have
     *            to accept this, too.
     */
    for (m = ap_loaded_modules; *m != NULL; m++)
        ;
    *m++ = mod;
    *m = NULL;

    return NULL;
}

AP_DECLARE(void) ap_remove_loaded_module(module *mod)
{
    module **m;
    module **m2;
    int done;

    /*
     *  Remove module pointer from chained module list
     */
    ap_remove_module(mod);

    /*
     *  Remove module pointer from list of loaded modules
     *
     *  Note: 1. We cannot determine if the module was successfully
     *           removed by ap_remove_module().
     *        2. We have not to complain explicitly when the module
     *           is not found because ap_remove_module() did it
     *           for us already.
     */
    for (m = m2 = ap_loaded_modules, done = 0; *m2 != NULL; m2++) {
        if (*m2 == mod && done == 0)
            done = 1;
        else
            *m++ = *m2;
    }

    *m = NULL;
}

AP_DECLARE(const char *) ap_setup_prelinked_modules(process_rec *process)
{
    module **m;
    module **m2;
    const char *error;

    apr_hook_global_pool=process->pconf;

    rebuild_conf_hash(process->pconf, 0);

    /*
     *  Initialise total_modules variable and module indices
     */
    total_modules = 0;
    for (m = ap_preloaded_modules; *m != NULL; m++)
        (*m)->module_index = total_modules++;

    max_modules = total_modules + DYNAMIC_MODULE_LIMIT + 1;
    conf_vector_length = max_modules;

    /*
     *  Initialise list of loaded modules and short names
     */
    ap_loaded_modules = (module **)apr_palloc(process->pool,
        sizeof(module *) * conf_vector_length);
    if (!ap_module_short_names)
        ap_module_short_names = ap_calloc(sizeof(char *), conf_vector_length);

    if (!merger_func_cache)
        merger_func_cache = ap_calloc(sizeof(merger_func), conf_vector_length);

    if (ap_loaded_modules == NULL || ap_module_short_names == NULL
        || merger_func_cache == NULL)
        return "Ouch! Out of memory in ap_setup_prelinked_modules()!";

    for (m = ap_preloaded_modules, m2 = ap_loaded_modules; *m != NULL; )
        *m2++ = *m++;

    *m2 = NULL;

    /*
     *   Initialize chain of linked (=activate) modules
     */
    for (m = ap_prelinked_modules; *m != NULL; m++) {
        error = ap_add_module(*m, process->pconf, NULL);
        if (error) {
            return error;
        }
    }

    apr_hook_sort_all();

    return NULL;
}

AP_DECLARE(const char *) ap_find_module_name(module *m)
{
    return m->name;
}

AP_DECLARE(const char *) ap_find_module_short_name(int module_index)
{
        if (module_index < 0 || module_index >= conf_vector_length)
                return NULL;
        return ap_module_short_names[module_index];
}

AP_DECLARE(module *) ap_find_linked_module(const char *name)
{
    module *modp;

    for (modp = ap_top_module; modp; modp = modp->next) {
        if (strcmp(modp->name, name) == 0)
            return modp;
    }

    return NULL;
}

/*****************************************************************
 *
 * Resource, access, and .htaccess config files now parsed by a common
 * command loop.
 *
 * Let's begin with the basics; parsing the line and
 * invoking the function...
 */

#define AP_MAX_ARGC 64

static const char *invoke_cmd(const command_rec *cmd, cmd_parms *parms,
                              void *mconfig, const char *args)
{
    int override_list_ok = 0;
    char *w, *w2, *w3;
    const char *errmsg = NULL;

    /* Have we been provided a list of acceptable directives? */
    if (parms->override_list != NULL) { 
         if (apr_table_get(parms->override_list, cmd->name) != NULL) { 
              override_list_ok = 1;
         }
    }

    if ((parms->override & cmd->req_override) == 0 && !override_list_ok) {
        if (parms->override & NONFATAL_OVERRIDE) {
            ap_log_perror(APLOG_MARK, APLOG_WARNING, 0, parms->temp_pool,
                          APLOGNO(02295)
                          "%s in .htaccess forbidden by AllowOverride",
                          cmd->name);
            return NULL;
        }
        else if (parms->directive && parms->directive->parent) {
            return apr_pstrcat(parms->pool, cmd->name, " not allowed in ",
                               parms->directive->parent->directive, ">",
                               " context", NULL);
        }
        else {
            return apr_pstrcat(parms->pool, cmd->name,
                               " not allowed here", NULL);
        }
    }

    parms->info = cmd->cmd_data;
    parms->cmd = cmd;

    switch (cmd->args_how) {
    case RAW_ARGS:
#ifdef RESOLVE_ENV_PER_TOKEN
        args = ap_resolve_env(parms->pool,args);
#endif
        return cmd->AP_RAW_ARGS(parms, mconfig, args);

    case TAKE_ARGV:
        {
            char *argv[AP_MAX_ARGC];
            int argc = 0;

            do {
                w = ap_getword_conf(parms->pool, &args);
                if (*w == '\0' && *args == '\0') {
                    break;
                }
                argv[argc] = w;
                argc++;
            } while (argc < AP_MAX_ARGC && *args != '\0');

            return cmd->AP_TAKE_ARGV(parms, mconfig, argc, argv);
        }

    case NO_ARGS:
        if (*args != 0)
            return apr_pstrcat(parms->pool, cmd->name, " takes no arguments",
                               NULL);

        return cmd->AP_NO_ARGS(parms, mconfig);

    case TAKE1:
        w = ap_getword_conf(parms->pool, &args);

        if (*w == '\0' || *args != 0)
            return apr_pstrcat(parms->pool, cmd->name, " takes one argument",
                               cmd->errmsg ? ", " : NULL, cmd->errmsg, NULL);

        return cmd->AP_TAKE1(parms, mconfig, w);

    case TAKE2:
        w = ap_getword_conf(parms->pool, &args);
        w2 = ap_getword_conf(parms->pool, &args);

        if (*w == '\0' || *w2 == '\0' || *args != 0)
            return apr_pstrcat(parms->pool, cmd->name, " takes two arguments",
                               cmd->errmsg ? ", " : NULL, cmd->errmsg, NULL);

        return cmd->AP_TAKE2(parms, mconfig, w, w2);

    case TAKE12:
        w = ap_getword_conf(parms->pool, &args);
        w2 = ap_getword_conf(parms->pool, &args);

        if (*w == '\0' || *args != 0)
            return apr_pstrcat(parms->pool, cmd->name, " takes 1-2 arguments",
                               cmd->errmsg ? ", " : NULL, cmd->errmsg, NULL);

        return cmd->AP_TAKE2(parms, mconfig, w, *w2 ? w2 : NULL);

    case TAKE3:
        w = ap_getword_conf(parms->pool, &args);
        w2 = ap_getword_conf(parms->pool, &args);
        w3 = ap_getword_conf(parms->pool, &args);

        if (*w == '\0' || *w2 == '\0' || *w3 == '\0' || *args != 0)
            return apr_pstrcat(parms->pool, cmd->name, " takes three arguments",
                               cmd->errmsg ? ", " : NULL, cmd->errmsg, NULL);

        return cmd->AP_TAKE3(parms, mconfig, w, w2, w3);

    case TAKE23:
        w = ap_getword_conf(parms->pool, &args);
        w2 = ap_getword_conf(parms->pool, &args);
        w3 = *args ? ap_getword_conf(parms->pool, &args) : NULL;

        if (*w == '\0' || *w2 == '\0' || *args != 0)
            return apr_pstrcat(parms->pool, cmd->name,
                               " takes two or three arguments",
                               cmd->errmsg ? ", " : NULL, cmd->errmsg, NULL);

        return cmd->AP_TAKE3(parms, mconfig, w, w2, w3);

    case TAKE123:
        w = ap_getword_conf(parms->pool, &args);
        w2 = *args ? ap_getword_conf(parms->pool, &args) : NULL;
        w3 = *args ? ap_getword_conf(parms->pool, &args) : NULL;

        if (*w == '\0' || *args != 0)
            return apr_pstrcat(parms->pool, cmd->name,
                               " takes one, two or three arguments",
                               cmd->errmsg ? ", " : NULL, cmd->errmsg, NULL);

        return cmd->AP_TAKE3(parms, mconfig, w, w2, w3);

    case TAKE13:
        w = ap_getword_conf(parms->pool, &args);
        w2 = *args ? ap_getword_conf(parms->pool, &args) : NULL;
        w3 = *args ? ap_getword_conf(parms->pool, &args) : NULL;

        if (*w == '\0' || (w2 && *w2 && !w3) || *args != 0)
            return apr_pstrcat(parms->pool, cmd->name,
                               " takes one or three arguments",
                               cmd->errmsg ? ", " : NULL, cmd->errmsg, NULL);

        return cmd->AP_TAKE3(parms, mconfig, w, w2, w3);

    case ITERATE:
        w = ap_getword_conf(parms->pool, &args);
        
        if (*w == '\0')
            return apr_pstrcat(parms->pool, cmd->name,
                               " requires at least one argument",
                               cmd->errmsg ? ", " : NULL, cmd->errmsg, NULL);

        while (*w != '\0') {
            errmsg = cmd->AP_TAKE1(parms, mconfig, w);

            if (errmsg && strcmp(errmsg, DECLINE_CMD) != 0)
                return errmsg;

            w = ap_getword_conf(parms->pool, &args);
        }

        return errmsg;

    case ITERATE2:
        w = ap_getword_conf(parms->pool, &args);

        if (*w == '\0' || *args == 0)
            return apr_pstrcat(parms->pool, cmd->name,
                               " requires at least two arguments",
                               cmd->errmsg ? ", " : NULL, cmd->errmsg, NULL);

        while (*(w2 = ap_getword_conf(parms->pool, &args)) != '\0') {

            errmsg = cmd->AP_TAKE2(parms, mconfig, w, w2);

            if (errmsg && strcmp(errmsg, DECLINE_CMD) != 0)
                return errmsg;
        }

        return errmsg;

    case FLAG:
        /*
         * This is safe to use temp_pool here, because the 'flag' itself is not
         * forwarded as-is
         */
        w = ap_getword_conf(parms->temp_pool, &args);

        if (*w == '\0' || (ap_cstr_casecmp(w, "on") && ap_cstr_casecmp(w, "off")))
            return apr_pstrcat(parms->pool, cmd->name, " must be On or Off",
                               NULL);

        return cmd->AP_FLAG(parms, mconfig, ap_cstr_casecmp(w, "off") != 0);

    default:
        return apr_pstrcat(parms->pool, cmd->name,
                           " is improperly configured internally (server bug)",
                           NULL);
    }
}

AP_CORE_DECLARE(const command_rec *) ap_find_command(const char *name,
                                                     const command_rec *cmds)
{
    while (cmds->name) {
        if (!ap_cstr_casecmp(name, cmds->name))
            return cmds;

        ++cmds;
    }

    return NULL;
}

AP_CORE_DECLARE(const command_rec *) ap_find_command_in_modules(
                                          const char *cmd_name, module **mod)
{
    const command_rec *cmdp;
    module *modp;

    for (modp = *mod; modp; modp = modp->next) {
        if (modp->cmds && (cmdp = ap_find_command(cmd_name, modp->cmds))) {
            *mod = modp;
            return cmdp;
        }
    }

    return NULL;
}

AP_CORE_DECLARE(void *) ap_set_config_vectors(server_rec *server,
                                              ap_conf_vector_t *section_vector,
                                              const char *section,
                                              module *mod, apr_pool_t *pconf)
{
    void *section_config = ap_get_module_config(section_vector, mod);
    void *server_config = ap_get_module_config(server->module_config, mod);

    if (!section_config && mod->create_dir_config) {
        /* ### need to fix the create_dir_config functions' prototype... */
        section_config = (*mod->create_dir_config)(pconf, (char *)section);
        ap_set_module_config(section_vector, mod, section_config);
    }

    if (!server_config && mod->create_server_config) {
        server_config = (*mod->create_server_config)(pconf, server);
        ap_set_module_config(server->module_config, mod, server_config);
    }

    return section_config;
}

static const char *execute_now(char *cmd_line, const char *args,
                               cmd_parms *parms,
                               apr_pool_t *p, apr_pool_t *ptemp,
                               ap_directive_t **sub_tree,
                               ap_directive_t *parent);

static const char *ap_build_config_sub(apr_pool_t *p, apr_pool_t *temp_pool,
                                       const char *l, cmd_parms *parms,
                                       ap_directive_t **current,
                                       ap_directive_t **curr_parent,
                                       ap_directive_t **conftree)
{
    const char *retval = NULL;
    const char *args;
    char *cmd_name;
    ap_directive_t *newdir;
    const command_rec *cmd;
    ap_mod_list *ml;
    char *lname;

    if (*l == '#' || *l == '\0')
        return NULL;

#if RESOLVE_ENV_PER_TOKEN
    args = l;
#else
    args = ap_resolve_env(temp_pool, l);
#endif

    /* The first word is the name of a directive.  We can safely use the
     * 'temp_pool' for it.  If it matches the name of a known directive, we
     * can reference the string within the module if needed.  Otherwise, we
     * can still make a copy in the 'p' pool. */
    cmd_name = ap_getword_conf(temp_pool, &args);
    if (*cmd_name == '\0') {
        /* Note: this branch should not occur. An empty line should have
         * triggered the exit further above.
         */
        return NULL;
    }

    if (cmd_name[1] != '/') {
        char *lastc = cmd_name + strlen(cmd_name) - 1;
        if (*lastc == '>') {
            *lastc = '\0' ;
        }
        if (cmd_name[0] == '<' && *args == '\0') {
            args = ">";
        }
    }

    newdir = apr_pcalloc(p, sizeof(ap_directive_t));
    newdir->filename = parms->config_file->name;
    newdir->line_num = parms->config_file->line_number;
    newdir->args = apr_pstrdup(p, args);

    lname = apr_pstrdup(temp_pool, cmd_name);
    ap_str_tolower(lname);
    ml = apr_hash_get(ap_config_hash, lname, APR_HASH_KEY_STRING);

    if (ml && (cmd = ml->cmd) != NULL) {
        newdir->directive = cmd->name;
        if (cmd->req_override & EXEC_ON_READ) {
            ap_directive_t *sub_tree = NULL;

            parms->err_directive = newdir;
            retval = execute_now(cmd_name, args, parms, p, temp_pool,
                                 &sub_tree, *curr_parent);
            if (*current) {
                (*current)->next = sub_tree;
            }
            else {
                *current = sub_tree;
                if (*curr_parent) {
                    (*curr_parent)->first_child = (*current);
                }
                if (*current) {
                    (*current)->parent = (*curr_parent);
                }
            }
            if (*current) {
                if (!*conftree) {
                    /* Before walking *current to the end of the list,
                     * set the head to *current.
                     */
                    *conftree = *current;
                }
                while ((*current)->next != NULL) {
                    (*current) = (*current)->next;
                    (*current)->parent = (*curr_parent);
                }
            }
            return retval;
        }
    }
    else {
        /* No known directive found?  Make a copy of what we have parsed. */
        newdir->directive = apr_pstrdup(p, cmd_name);
    }


    if (cmd_name[0] == '<') {
        if (cmd_name[1] != '/') {
            (*current) = ap_add_node(curr_parent, *current, newdir, 1);
        }
        else if (*curr_parent == NULL) {
            parms->err_directive = newdir;
            return apr_pstrcat(p, cmd_name,
                               " without matching <", cmd_name + 2,
                               " section", NULL);
        }
        else {
            char *bracket = cmd_name + strlen(cmd_name) - 1;

            if (*bracket != '>') {
                parms->err_directive = newdir;
                return apr_pstrcat(p, cmd_name,
                                   "> directive missing closing '>'", NULL);
            }

            *bracket = '\0';

            if (ap_cstr_casecmp(cmd_name + 2,
                           (*curr_parent)->directive + 1) != 0) {
                parms->err_directive = newdir;
                return apr_pstrcat(p, "Expected </",
                                   (*curr_parent)->directive + 1, "> but saw ",
                                   cmd_name, ">", NULL);
            }

            *bracket = '>';

            /* done with this section; move up a level */
            *current = *curr_parent;
            *curr_parent = (*current)->parent;
        }
    }
    else {
        *current = ap_add_node(curr_parent, *current, newdir, 0);
    }

    return retval;
}

#define VARBUF_INIT_LEN 200
#define VARBUF_MAX_LEN  (16*1024*1024)

AP_DECLARE(const char *) ap_build_cont_config(apr_pool_t *p,
                                              apr_pool_t *temp_pool,
                                              cmd_parms *parms,
                                              ap_directive_t **current,
                                              ap_directive_t **curr_parent,
                                              char *orig_directive)
{
    char *bracket;
    const char *retval;
    ap_directive_t *sub_tree = NULL;
    apr_status_t rc;
    struct ap_varbuf vb;
    apr_size_t max_len = VARBUF_MAX_LEN;
    if (p == temp_pool)
        max_len = HUGE_STRING_LEN; /* lower limit for .htaccess */

    bracket = apr_pstrcat(temp_pool, orig_directive + 1, ">", NULL);
    ap_varbuf_init(temp_pool, &vb, VARBUF_INIT_LEN);

    while ((rc = ap_varbuf_cfg_getline(&vb, parms->config_file, max_len))
           == APR_SUCCESS) {
        if (!memcmp(vb.buf, "</", 2)
            && (ap_cstr_casecmp(vb.buf + 2, bracket) == 0)
            && (*curr_parent == NULL)) {
            break;
        }
        retval = ap_build_config_sub(p, temp_pool, vb.buf, parms, current,
                                     curr_parent, &sub_tree);
        if (retval != NULL)
            return retval;

        if (sub_tree == NULL) {
            sub_tree = *curr_parent;
        }

        if (sub_tree == NULL) {
            sub_tree = *current;
        }
    }
    ap_varbuf_free(&vb);
    if (rc != APR_EOF && rc != APR_SUCCESS)
        return ap_pcfg_strerror(temp_pool, parms->config_file, rc);

    *current = sub_tree;
    return NULL;
}

static const char *ap_walk_config_sub(const ap_directive_t *current,
                                      cmd_parms *parms,
                                      ap_conf_vector_t *section_vector)
{
    const command_rec *cmd;
    ap_mod_list *ml;
    char *dir = apr_pstrdup(parms->temp_pool, current->directive);

    ap_str_tolower(dir);

    ml = apr_hash_get(ap_config_hash, dir, APR_HASH_KEY_STRING);

    if (ml == NULL) {
        parms->err_directive = current;
        if (parms->override & NONFATAL_UNKNOWN) {
            ap_log_perror(APLOG_MARK, APLOG_WARNING, 0, parms->temp_pool,
                          APLOGNO(02296) "Unknown directive %s "
                          "perhaps misspelled or defined by a module "
                          "not included in the server configuration", dir);
            return NULL;
        }
        else {
            return apr_pstrcat(parms->pool, "Invalid command '",
                               current->directive,
                               "', perhaps misspelled or defined by a module "
                               "not included in the server configuration",
                               NULL);
        }
    }

    for ( ; ml != NULL; ml = ml->next) {
        void *dir_config = ap_set_config_vectors(parms->server,
                                                 section_vector,
                                                 parms->path,
                                                 ml->m,
                                                 parms->pool);
        const char *retval;
        cmd = ml->cmd;

        /* Once was enough? */
        if (cmd->req_override & EXEC_ON_READ) {
            continue;
        }

        retval = invoke_cmd(cmd, parms, dir_config, current->args);

        if (retval != NULL && strcmp(retval, DECLINE_CMD) != 0) {
            /* If the directive in error has already been set, don't
             * replace it.  Otherwise, an error inside a container
             * will be reported as occurring on the first line of the
             * container.
             */
            if (!parms->err_directive) {
                parms->err_directive = current;
            }
            return retval;
        }
    }

    return NULL;
}

AP_DECLARE(const char *) ap_walk_config(ap_directive_t *current,
                                        cmd_parms *parms,
                                        ap_conf_vector_t *section_vector)
{
    ap_conf_vector_t *oldconfig = parms->context;

    parms->context = section_vector;

    /* scan through all directives, executing each one */
    for (; current != NULL; current = current->next) {
        const char *errmsg;

        parms->directive = current;

        /* actually parse the command and execute the correct function */
        errmsg = ap_walk_config_sub(current, parms, section_vector);
        if (errmsg != NULL) {
            /* restore the context (just in case) */
            parms->context = oldconfig;
            return errmsg;
        }
    }

    parms->context = oldconfig;
    return NULL;
}

AP_DECLARE(const char *) ap_build_config(cmd_parms *parms,
                                         apr_pool_t *p, apr_pool_t *temp_pool,
                                         ap_directive_t **conftree)
{
    ap_directive_t *current = *conftree;
    ap_directive_t *curr_parent = NULL;
    const char *errmsg;
    ap_directive_t **last_ptr = NULL;
    apr_status_t rc;
    struct ap_varbuf vb;
    apr_size_t max_len = VARBUF_MAX_LEN;
    if (p == temp_pool)
        max_len = HUGE_STRING_LEN; /* lower limit for .htaccess */

    ap_varbuf_init(temp_pool, &vb, VARBUF_INIT_LEN);

    if (current != NULL) {
        /* If we have to traverse the whole tree again for every included
         * config file, the required time grows as O(n^2) with the number of
         * files. This can be a significant delay for large configurations.
         * Therefore we cache a pointer to the last node.
         */
        last_ptr = &(current->last);

        if (last_ptr && *last_ptr) {
            current = *last_ptr;
        }

        while (current->next) {
            current = current->next;
        }

        if (last_ptr) {
            /* update cached pointer to last node */
            *last_ptr = current;
        }
    }

    while ((rc = ap_varbuf_cfg_getline(&vb, parms->config_file, max_len))
           == APR_SUCCESS) {
        errmsg = ap_build_config_sub(p, temp_pool, vb.buf, parms,
                                     &current, &curr_parent, conftree);
        if (errmsg != NULL)
            return errmsg;

        if (*conftree == NULL && curr_parent != NULL) {
            *conftree = curr_parent;
        }

        if (*conftree == NULL && current != NULL) {
            *conftree = current;
        }
    }
    ap_varbuf_free(&vb);
    if (rc != APR_EOF && rc != APR_SUCCESS)
        return ap_pcfg_strerror(temp_pool, parms->config_file, rc);

    if (curr_parent != NULL) {
        errmsg = "";

        while (curr_parent != NULL) {
            errmsg = apr_psprintf(p, "%s%s%s:%u: %s> was not closed.",
                                  errmsg,
                                  *errmsg == '\0' ? "" : APR_EOL_STR,
                                  curr_parent->filename,
                                  curr_parent->line_num,
                                  curr_parent->directive);

            parms->err_directive = curr_parent;
            curr_parent = curr_parent->parent;
        }

        return errmsg;
    }

    return NULL;
}

/*
 * Generic command functions...
 */

AP_DECLARE_NONSTD(const char *) ap_set_string_slot(cmd_parms *cmd,
                                                   void *struct_ptr,
                                                   const char *arg)
{
    int offset = (int)(long)cmd->info;

    *(const char **)((char *)struct_ptr + offset) = arg;

    return NULL;
}

AP_DECLARE_NONSTD(const char *) ap_set_int_slot(cmd_parms *cmd,
                                                void *struct_ptr,
                                                const char *arg)
{
    char *endptr;
    char *error_str = NULL;
    int offset = (int)(long)cmd->info;

    *(int *)((char*)struct_ptr + offset) = strtol(arg, &endptr, 10);

    if ((*arg == '\0') || (*endptr != '\0')) {
        error_str = apr_psprintf(cmd->pool,
                     "Invalid value for directive %s, expected integer",
                     cmd->directive->directive);
    }

    return error_str;
}

AP_DECLARE_NONSTD(const char *) ap_set_string_slot_lower(cmd_parms *cmd,
                                                         void *struct_ptr,
                                                         const char *arg_)
{
    char *arg = apr_pstrdup(cmd->pool,arg_);
    int offset = (int)(long)cmd->info;

    ap_str_tolower(arg);
    *(char **)((char *)struct_ptr + offset) = arg;

    return NULL;
}

AP_DECLARE_NONSTD(const char *) ap_set_flag_slot(cmd_parms *cmd,
                                                 void *struct_ptr_v, int arg)
{
    int offset = (int)(long)cmd->info;
    char *struct_ptr = (char *)struct_ptr_v;

    *(int *)(struct_ptr + offset) = arg ? 1 : 0;

    return NULL;
}

AP_DECLARE_NONSTD(const char *) ap_set_flag_slot_char(cmd_parms *cmd,
                                                      void *struct_ptr_v, int arg)
{
    int offset = (int)(long)cmd->info;
    char *struct_ptr = (char *)struct_ptr_v;

    *(struct_ptr + offset) = arg ? 1 : 0;

    return NULL;
}


AP_DECLARE_NONSTD(const char *) ap_set_file_slot(cmd_parms *cmd, void *struct_ptr,
                                                 const char *arg)
{
    /* Prepend server_root to relative arg.
     * This allows most args to be independent of server_root,
     * so the server can be moved or mirrored with less pain.
     */
    const char *path;
    int offset = (int)(long)cmd->info;

    path = ap_server_root_relative(cmd->pool, arg);

    if (!path) {
        return apr_pstrcat(cmd->pool, cmd->cmd->name, ": Invalid file path '",
                           arg, "'", NULL);
    }

    *(const char **) ((char*)struct_ptr + offset) = path;

    return NULL;
}

AP_DECLARE_NONSTD(const char *) ap_set_deprecated(cmd_parms *cmd,
                                                  void *struct_ptr,
                                                  const char *arg)
{
    return cmd->cmd->errmsg;
}

AP_DECLARE(void) ap_reset_module_loglevels(struct ap_logconf *l, int val)
{
    if (l->module_levels)
        memset(l->module_levels, val, conf_vector_length);
}

AP_DECLARE(void) ap_set_module_loglevel(apr_pool_t *pool, struct ap_logconf *l,
                                        int index, int level)
{
    if (!l->module_levels) {
        l->module_levels = apr_palloc(pool, conf_vector_length);
        if (l->level == APLOG_UNSET) {
                ap_reset_module_loglevels(l, APLOG_UNSET);
        }
        else {
                ap_reset_module_loglevels(l, APLOG_NO_MODULE);
        }
    }

    l->module_levels[index] = level;
}

/*****************************************************************
 *
 * Reading whole config files...
 */

static cmd_parms default_parms =
{NULL, 0, 0, NULL, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

AP_DECLARE(char *) ap_server_root_relative(apr_pool_t *p, const char *file)
{
    char *newpath = NULL;
    apr_status_t rv;
    rv = apr_filepath_merge(&newpath, ap_server_root, file,
                            APR_FILEPATH_TRUENAME, p);
    if (newpath && (rv == APR_SUCCESS || APR_STATUS_IS_EPATHWILD(rv)
                                      || APR_STATUS_IS_ENOENT(rv)
                                      || APR_STATUS_IS_ENOTDIR(rv))) {
        return newpath;
    }
    else {
        return NULL;
    }
}

AP_DECLARE(char *) ap_runtime_dir_relative(apr_pool_t *p, const char *file)
{
    char *newpath = NULL;
    apr_status_t rv;
    const char *runtime_dir = ap_runtime_dir ? ap_runtime_dir : ap_server_root_relative(p, DEFAULT_REL_RUNTIMEDIR);

    rv = apr_filepath_merge(&newpath, runtime_dir, file,
                            APR_FILEPATH_TRUENAME, p);
    if (newpath && (rv == APR_SUCCESS || APR_STATUS_IS_EPATHWILD(rv)
                                      || APR_STATUS_IS_ENOENT(rv)
                                      || APR_STATUS_IS_ENOTDIR(rv))) {
        return newpath;
    }
    else {
        return NULL;
    }
}


AP_DECLARE(const char *) ap_soak_end_container(cmd_parms *cmd, char *directive)
{
    struct ap_varbuf vb;
    const char *args;
    char *cmd_name;
    apr_status_t rc;
    apr_size_t max_len = VARBUF_MAX_LEN;
    if (cmd->pool == cmd->temp_pool)
        max_len = HUGE_STRING_LEN; /* lower limit for .htaccess */

    ap_varbuf_init(cmd->temp_pool, &vb, VARBUF_INIT_LEN);

    while ((rc = ap_varbuf_cfg_getline(&vb, cmd->config_file, max_len))
           == APR_SUCCESS) {
        args = vb.buf;

        cmd_name = ap_getword_conf(cmd->temp_pool, &args);
        if (cmd_name[0] == '<') {
            if (cmd_name[1] == '/') {
                cmd_name[strlen(cmd_name) - 1] = '\0';

                if (ap_cstr_casecmp(cmd_name + 2, directive + 1) != 0) {
                    return apr_pstrcat(cmd->pool, "Expected </",
                                       directive + 1, "> but saw ",
                                       cmd_name, ">", NULL);
                }

                ap_varbuf_free(&vb);
                return NULL; /* found end of container */
            }
            else {
                const char *msg;

                if (*args == '\0' && cmd_name[strlen(cmd_name) - 1] == '>') {
                    cmd_name[strlen(cmd_name) - 1] = '\0';
                }

                if ((msg = ap_soak_end_container(cmd, cmd_name)) != NULL) {
                    return msg;
                }
            }
        }
    }
    if (rc != APR_EOF && rc != APR_SUCCESS)
        return ap_pcfg_strerror(cmd->temp_pool, cmd->config_file, rc);

    return apr_pstrcat(cmd->pool, "Expected </",
                       directive + 1, "> before end of configuration",
                       NULL);
}

static const char *execute_now(char *cmd_line, const char *args,
                               cmd_parms *parms,
                               apr_pool_t *p, apr_pool_t *ptemp,
                               ap_directive_t **sub_tree,
                               ap_directive_t *parent)
{
    const command_rec *cmd;
    ap_mod_list *ml;
    char *dir = apr_pstrdup(parms->temp_pool, cmd_line);

    ap_str_tolower(dir);

    ml = apr_hash_get(ap_config_hash, dir, APR_HASH_KEY_STRING);

    if (ml == NULL) {
        return apr_pstrcat(parms->pool, "Invalid command '",
                           cmd_line,
                           "', perhaps misspelled or defined by a module "
                           "not included in the server configuration",
                           NULL);
    }

    for ( ; ml != NULL; ml = ml->next) {
        const char *retval;
        cmd = ml->cmd;

        retval = invoke_cmd(cmd, parms, sub_tree, args);

        if (retval != NULL) {
            return retval;
        }
    }

    return NULL;
}

/* This structure and the following functions are needed for the
 * table-based config file reading. They are passed to the
 * cfg_open_custom() routine.
 */

/* Structure to be passed to cfg_open_custom(): it contains an
 * index which is incremented from 0 to nelts on each call to
 * cfg_getline() (which in turn calls arr_elts_getstr())
 * and an apr_array_header_t pointer for the string array.
 */
typedef struct {
    apr_array_header_t *array;
    int curr_idx;
} arr_elts_param_t;


/* arr_elts_getstr() returns the next line from the string array. */
static apr_status_t arr_elts_getstr(void *buf, apr_size_t bufsiz, void *param)
{
    arr_elts_param_t *arr_param = (arr_elts_param_t *)param;
    const char *elt;

    /* End of array reached? */
    if (++arr_param->curr_idx > arr_param->array->nelts)
        return APR_EOF;

    /* return the line */
    elt = ((const char **)arr_param->array->elts)[arr_param->curr_idx - 1];
    if (apr_cpystrn(buf, elt, bufsiz) - (char *)buf >= bufsiz - 1)
        return APR_ENOSPC;
    return APR_SUCCESS;
}


/* arr_elts_close(): dummy close routine (makes sure no more lines can be read) */
static apr_status_t arr_elts_close(void *param)
{
    arr_elts_param_t *arr_param = (arr_elts_param_t *)param;

    arr_param->curr_idx = arr_param->array->nelts;

    return APR_SUCCESS;
}

static const char *process_command_config(server_rec *s,
                                          apr_array_header_t *arr,
                                          ap_directive_t **conftree,
                                          apr_pool_t *p,
                                          apr_pool_t *ptemp)
{
    const char *errmsg;
    cmd_parms parms;
    arr_elts_param_t arr_parms;

    arr_parms.curr_idx = 0;
    arr_parms.array = arr;

    if (ap_config_hash == NULL) {
        rebuild_conf_hash(s->process->pconf, 1);
    }

    parms = default_parms;
    parms.pool = p;
    parms.temp_pool = ptemp;
    parms.server = s;
    parms.override = (RSRC_CONF | OR_ALL) & ~(OR_AUTHCFG | OR_LIMIT);
    parms.override_opts = OPT_ALL | OPT_SYM_OWNER | OPT_MULTI;

    parms.config_file = ap_pcfg_open_custom(p, "-c/-C directives",
                                            &arr_parms, NULL,
                                            arr_elts_getstr, arr_elts_close);

    errmsg = ap_build_config(&parms, p, ptemp, conftree);
    ap_cfg_closefile(parms.config_file);

    if (errmsg) {
        return apr_pstrcat(p, "Syntax error in -C/-c directive: ", errmsg,
                           NULL);
    }

    return NULL;
}

/**
 * Used by -D DUMP_INCLUDES to output the config file "tree".
 */
static void dump_config_name(const char *fname, apr_pool_t *p)
{
    unsigned i, recursion, line_number;
    void *data;
    apr_file_t *out = NULL;

    apr_file_open_stdout(&out, p);

    /* ap_include_sentinel is defined by the core Include directive; use it to
     * figure out how deep in the stack we are.
     */
    apr_pool_userdata_get(&data, "ap_include_sentinel", p);

    if (data) {
        recursion = *(unsigned *)data;
    } else {
        recursion = 0;
    }

    /* Indent once for each level. */
    for (i = 0; i < (recursion + 1); ++i) {
        apr_file_printf(out, "  ");
    }

    /* ap_include_lineno is similarly defined to tell us where in the last
     * config file we were.
     */
    apr_pool_userdata_get(&data, "ap_include_lineno", p);

    if (data) {
        line_number = *(unsigned *)data;
    } else {
        line_number = 0;
    }

    /* Print the line number and the name of the parsed file. */
    if (line_number > 0) {
        apr_file_printf(out, "(%u)", line_number);
    } else {
        apr_file_printf(out, "(*)");
    }

    apr_file_printf(out, " %s\n", fname);
}

AP_DECLARE(const char *) ap_process_resource_config(server_rec *s,
                                                    const char *fname,
                                                    ap_directive_t **conftree,
                                                    apr_pool_t *p,
                                                    apr_pool_t *ptemp)
{
    ap_configfile_t *cfp;
    cmd_parms parms;
    apr_status_t rv;
    const char *error;

    parms = default_parms;
    parms.pool = p;
    parms.temp_pool = ptemp;
    parms.server = s;
    parms.override = (RSRC_CONF | OR_ALL) & ~(OR_AUTHCFG | OR_LIMIT);
    parms.override_opts = OPT_ALL | OPT_SYM_OWNER | OPT_MULTI;

    rv = ap_pcfg_openfile(&cfp, p, fname);
    if (rv != APR_SUCCESS) {
        return apr_psprintf(p, "Could not open configuration file %s: %pm",
                            fname, &rv);
    }

    if (ap_exists_config_define("DUMP_INCLUDES")) {
        dump_config_name(fname, p);
    }

    parms.config_file = cfp;
    error = ap_build_config(&parms, p, ptemp, conftree);
    ap_cfg_closefile(cfp);

    if (error) {
        if (parms.err_directive)
            return apr_psprintf(p, "Syntax error on line %d of %s: %s",
                                parms.err_directive->line_num,
                                parms.err_directive->filename, error);
        else
            return error;
    }

    return NULL;
}

typedef struct {
    server_rec *s;
    ap_directive_t **conftree;
} configs;

static const char *process_resource_config_cb(ap_dir_match_t *w, const char *fname)
{
    configs *cfgs = w->ctx;
    return ap_process_resource_config(cfgs->s, fname, cfgs->conftree, w->p, w->ptemp);
}

AP_DECLARE(const char *) ap_process_fnmatch_configs(server_rec *s,
                                                    const char *fname,
                                                    ap_directive_t **conftree,
                                                    apr_pool_t *p,
                                                    apr_pool_t *ptemp,
                                                    int optional)
{
    configs cfgs;
    ap_dir_match_t w;

    cfgs.s = s;
    cfgs.conftree = conftree;

    w.prefix = "Include/IncludeOptional: ";
    w.p = p;
    w.ptemp = ptemp;
    w.flags = (optional ? AP_DIR_FLAG_OPTIONAL : AP_DIR_FLAG_NONE) | AP_DIR_FLAG_RECURSIVE;
    w.cb = process_resource_config_cb;
    w.ctx = &cfgs;
    w.depth = 0;

    /* don't require conf/httpd.conf if we have a -C or -c switch */
    if ((ap_server_pre_read_config->nelts
        || ap_server_post_read_config->nelts)
        && !(strcmp(fname, ap_server_root_relative(ptemp, SERVER_CONFIG_FILE)))) {
        apr_finfo_t finfo;

        if (apr_stat(&finfo, fname, APR_FINFO_LINK | APR_FINFO_TYPE, ptemp) != APR_SUCCESS)
            return NULL;
    }

    if (!apr_fnmatch_test(fname)) {
        return ap_dir_nofnmatch(&w, fname);
    }
    else {
        apr_status_t status;
        const char *rootpath, *filepath = fname;

        /* locate the start of the directories proper */
        status = apr_filepath_root(&rootpath, &filepath, APR_FILEPATH_TRUENAME, ptemp);

        /* we allow APR_SUCCESS and APR_EINCOMPLETE */
        if (APR_ERELATIVE == status) {
            return apr_pstrcat(p, "Include must have an absolute path, ", fname, NULL);
        }
        else if (APR_EBADPATH == status) {
            return apr_pstrcat(p, "Include has a bad path, ", fname, NULL);
        }

        /* walk the filepath */
        return ap_dir_fnmatch(&w, rootpath, filepath);
    }
}

AP_DECLARE(int) ap_process_config_tree(server_rec *s,
                                       ap_directive_t *conftree,
                                       apr_pool_t *p,
                                       apr_pool_t *ptemp)
{
    const char *errmsg;
    cmd_parms parms;

    parms = default_parms;
    parms.pool = p;
    parms.temp_pool = ptemp;
    parms.server = s;
    parms.override = (RSRC_CONF | OR_ALL) & ~(OR_AUTHCFG | OR_LIMIT);
    parms.override_opts = OPT_ALL | OPT_SYM_OWNER | OPT_MULTI;
    parms.limited = -1;

    errmsg = ap_walk_config(conftree, &parms, s->lookup_defaults);
    if (errmsg) {
        if (parms.err_directive)
            ap_log_perror(APLOG_MARK, APLOG_STARTUP, 0, p, APLOGNO(00526)
                          "Syntax error on line %d of %s:",
                          parms.err_directive->line_num,
                          parms.err_directive->filename);
        ap_log_perror(APLOG_MARK, APLOG_STARTUP, 0, p, "%s", errmsg);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    return OK;
}

apr_status_t ap_open_htaccess(request_rec *r, const char *dir_name,
                              const char *access_name,
                              ap_configfile_t **conffile,
                              const char **full_name)
{
    *full_name = ap_make_full_path(r->pool, dir_name, access_name);
    return ap_pcfg_openfile(conffile, r->pool, *full_name);
}

AP_CORE_DECLARE(int) ap_parse_htaccess(ap_conf_vector_t **result,
                                       request_rec *r, int override,
                                       int override_opts, apr_table_t *override_list,
                                       const char *d, const char *access_names)
{
    ap_configfile_t *f = NULL;
    cmd_parms parms;
    const char *filename;
    const struct htaccess_result *cache;
    struct htaccess_result *new;
    ap_conf_vector_t *dc = NULL;
    apr_status_t status;

    /* firstly, search cache */
    for (cache = r->htaccess; cache != NULL; cache = cache->next) {
        if (cache->override == override && strcmp(cache->dir, d) == 0) {
            *result = cache->htaccess;
            return OK;
        }
    }

    parms = default_parms;
    parms.override = override;
    parms.override_opts = override_opts;
    parms.override_list = override_list;
    parms.pool = r->pool;
    parms.temp_pool = r->pool;
    parms.server = r->server;
    parms.path = apr_pstrdup(r->pool, d);

    /* loop through the access names and find the first one */
    while (access_names[0]) {
        const char *access_name = ap_getword_conf(r->pool, &access_names);

        filename = NULL;
        status = ap_run_open_htaccess(r, d, access_name, &f, &filename);
        if (status == APR_SUCCESS) {
            const char *errmsg;
            ap_directive_t *temptree = NULL;

            dc = ap_create_per_dir_config(r->pool);

            parms.config_file = f;
            errmsg = ap_build_config(&parms, r->pool, r->pool, &temptree);
            if (errmsg == NULL)
                errmsg = ap_walk_config(temptree, &parms, dc);

            ap_cfg_closefile(f);

            if (errmsg) {
                ap_log_rerror(APLOG_MARK, APLOG_ALERT, 0, r,
                              "%s: %s", filename, errmsg);
                return HTTP_INTERNAL_SERVER_ERROR;
            }

            *result = dc;
            break;
        }
        else {
            if (!APR_STATUS_IS_ENOENT(status)
                && !APR_STATUS_IS_ENOTDIR(status)) {
                ap_log_rerror(APLOG_MARK, APLOG_CRIT, status, r, APLOGNO(00529)
                              "%s pcfg_openfile: unable to check htaccess file, "
                              "ensure it is readable and that '%s' "
                              "is executable",
                              filename, d);
                apr_table_setn(r->notes, "error-notes",
                               "Server unable to read htaccess file, denying "
                               "access to be safe");
                return HTTP_FORBIDDEN;
            }
        }
    }

    /* cache it */
    new = apr_palloc(r->pool, sizeof(struct htaccess_result));
    new->dir = parms.path;
    new->override = override;
    new->override_opts = override_opts;
    new->htaccess = dc;

    /* add to head of list */
    new->next = r->htaccess;
    r->htaccess = new;

    return OK;
}

AP_CORE_DECLARE(const char *) ap_init_virtual_host(apr_pool_t *p,
                                                   const char *hostname,
                                                   server_rec *main_server,
                                                   server_rec **ps)
{
    server_rec *s = (server_rec *) apr_pcalloc(p, sizeof(server_rec));

    /* TODO: this crap belongs in http_core */
    s->process = main_server->process;
    s->server_admin = NULL;
    s->server_hostname = NULL;
    s->server_scheme = NULL;
    s->error_fname = NULL;
    s->timeout = 0;
    s->keep_alive_timeout = 0;
    s->keep_alive = -1;
    s->keep_alive_max = -1;
    s->error_log = main_server->error_log;
    s->log.level = APLOG_UNSET;
    s->log.module_levels = NULL;
    /* useful default, otherwise we get a port of 0 on redirects */
    s->port = main_server->port;
    s->next = NULL;

    s->is_virtual = 1;
    s->names = apr_array_make(p, 4, sizeof(char **));
    s->wild_names = apr_array_make(p, 4, sizeof(char **));

    s->module_config = create_empty_config(p);
    s->lookup_defaults = ap_create_per_dir_config(p);

    s->limit_req_line = main_server->limit_req_line;
    s->limit_req_fieldsize = main_server->limit_req_fieldsize;
    s->limit_req_fields = main_server->limit_req_fields;

    *ps = s;

    return ap_parse_vhost_addrs(p, hostname, s);
}

AP_DECLARE(struct ap_logconf *) ap_new_log_config(apr_pool_t *p,
                                                  const struct ap_logconf *old)
{
    struct ap_logconf *l = apr_pcalloc(p, sizeof(struct ap_logconf));
    if (old) {
        l->level = old->level;
        if (old->module_levels) {
            l->module_levels =
                apr_pmemdup(p, old->module_levels, conf_vector_length);
        }
    }
    else {
        l->level = APLOG_UNSET;
    }
    return l;
}

AP_DECLARE(void) ap_merge_log_config(const struct ap_logconf *old_conf,
                                     struct ap_logconf *new_conf)
{
    if (new_conf->level != APLOG_UNSET) {
        /* Setting the main loglevel resets all per-module log levels.
         * I.e. if new->level has been set, we must ignore old->module_levels.
         */
        return;
    }

    new_conf->level = old_conf->level;
    if (new_conf->module_levels == NULL) {
        new_conf->module_levels = old_conf->module_levels;
    }
    else if (old_conf->module_levels != NULL) {
        int i;
        for (i = 0; i < conf_vector_length; i++) {
            if (new_conf->module_levels[i] == APLOG_UNSET)
                new_conf->module_levels[i] = old_conf->module_levels[i];
        }
    }
}

AP_DECLARE(void) ap_fixup_virtual_hosts(apr_pool_t *p, server_rec *main_server)
{
    server_rec *virt;
    core_dir_config *dconf =
        ap_get_core_module_config(main_server->lookup_defaults);
    dconf->log = &main_server->log;

    for (virt = main_server->next; virt; virt = virt->next) {
        merge_server_configs(p, main_server->module_config, virt);

        virt->lookup_defaults =
            ap_merge_per_dir_configs(p, main_server->lookup_defaults,
                                     virt->lookup_defaults);

        if (virt->server_admin == NULL)
            virt->server_admin = main_server->server_admin;

        if (virt->timeout == 0)
            virt->timeout = main_server->timeout;

        if (virt->keep_alive_timeout == 0)
            virt->keep_alive_timeout = main_server->keep_alive_timeout;

        if (virt->keep_alive == -1)
            virt->keep_alive = main_server->keep_alive;

        if (virt->keep_alive_max == -1)
            virt->keep_alive_max = main_server->keep_alive_max;

        ap_merge_log_config(&main_server->log, &virt->log);

        dconf = ap_get_core_module_config(virt->lookup_defaults);
        dconf->log = &virt->log;

        /* XXX: this is really something that should be dealt with by a
         * post-config api phase
         */
        ap_core_reorder_directories(p, virt);
    }

    ap_core_reorder_directories(p, main_server);
}

/*****************************************************************
 *
 * Getting *everything* configured...
 */

static void init_config_globals(apr_pool_t *p)
{
    /* Global virtual host hash bucket pointers.  Init to null. */
    ap_init_vhost_config(p);
}

static server_rec *init_server_config(process_rec *process, apr_pool_t *p)
{
    apr_status_t rv;
    server_rec *s = (server_rec *) apr_pcalloc(p, sizeof(server_rec));

    apr_file_open_stderr(&s->error_log, p);
    s->process = process;
    s->port = 0;
    s->server_admin = DEFAULT_ADMIN;
    s->server_hostname = NULL;
    s->server_scheme = NULL;
    s->error_fname = DEFAULT_ERRORLOG;
    s->log.level = DEFAULT_LOGLEVEL;
    s->log.module_levels = NULL;
    s->limit_req_line = DEFAULT_LIMIT_REQUEST_LINE;
    s->limit_req_fieldsize = DEFAULT_LIMIT_REQUEST_FIELDSIZE;
    s->limit_req_fields = DEFAULT_LIMIT_REQUEST_FIELDS;
    s->timeout = apr_time_from_sec(DEFAULT_TIMEOUT);
    s->keep_alive_timeout = apr_time_from_sec(DEFAULT_KEEPALIVE_TIMEOUT);
    s->keep_alive_max = DEFAULT_KEEPALIVE;
    s->keep_alive = 1;
    s->next = NULL;
    s->addrs = apr_pcalloc(p, sizeof(server_addr_rec));

    /* NOT virtual host; don't match any real network interface */
    rv = apr_sockaddr_info_get(&s->addrs->host_addr,
                               NULL, APR_UNSPEC, 0, 0, p);
    if (rv != APR_SUCCESS) {
        /* should we test here for rv being an EAIERR? */
        ap_log_error(APLOG_MARK, APLOG_STARTUP|APLOG_CRIT, rv, NULL, APLOGNO(00530)
                     "initialisation: bug or getaddrinfo fail");
        return NULL;
    }

    s->addrs->host_port = 0; /* matches any port */
    s->addrs->virthost = ""; /* must be non-NULL */
    s->names = s->wild_names = NULL;

    s->module_config = create_server_config(p, s);
    s->lookup_defaults = create_default_per_dir_config(p);

    return s;
}


static apr_status_t reset_conf_vector_length(void *dummy)
{
    reserved_module_slots = 0;
    conf_vector_length = max_modules;
    return APR_SUCCESS;
}

static int conf_vector_length_pre_config(apr_pool_t *pconf, apr_pool_t *plog,
                                         apr_pool_t *ptemp)
{
    /*
     * We have loaded all modules that are loaded by EXEC_ON_READ directives.
     * From now on we reduce the size of the config vectors to what we need,
     * plus what has been reserved (e.g. by mod_perl) for additional modules
     * loaded later on.
     * If max_modules is too small, ap_add_module() will abort.
     */
    if (total_modules + reserved_module_slots < max_modules) {
        conf_vector_length = total_modules + reserved_module_slots;
    }
    apr_pool_cleanup_register(pconf, NULL, reset_conf_vector_length,
                              apr_pool_cleanup_null);
    return OK;
}


AP_CORE_DECLARE(void) ap_register_config_hooks(apr_pool_t *p)
{
    ap_hook_pre_config(conf_vector_length_pre_config, NULL, NULL,
                       APR_HOOK_REALLY_LAST);
}

AP_DECLARE(server_rec*) ap_read_config(process_rec *process, apr_pool_t *ptemp,
                                       const char *filename,
                                       ap_directive_t **conftree)
{
    const char *confname, *error;
    apr_pool_t *p = process->pconf;
    server_rec *s = init_server_config(process, p);
    if (s == NULL) {
        return s;
    }
    if (ap_server_conf == NULL) {
        ap_server_conf = s;
    }

    init_config_globals(p);

    if (ap_exists_config_define("DUMP_INCLUDES")) {
        apr_file_t *out = NULL;
        apr_file_open_stdout(&out, p);

        /* Included files will be dumped as the config is walked; print a
         * header.
         */
        apr_file_printf(out, "Included configuration files:\n");
    }

    /* All server-wide config files now have the SAME syntax... */
    error = process_command_config(s, ap_server_pre_read_config, conftree,
                                   p, ptemp);
    if (error) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP|APLOG_CRIT, 0, NULL, "%s: %s",
                     ap_server_argv0, error);
        return NULL;
    }

    /* process_command_config may change the ServerRoot so
     * compute this config file name afterwards.
     */
    confname = ap_server_root_relative(p, filename);

    if (!confname) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP|APLOG_CRIT,
                     APR_EBADPATH, NULL, APLOGNO(00532) "Invalid config file path %s",
                     filename);
        return NULL;
    }

    error = ap_process_resource_config(s, confname, conftree, p, ptemp);
    if (error) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP|APLOG_CRIT, 0, NULL,
                     "%s: %s", ap_server_argv0, error);
        return NULL;
    }

    error = ap_check_mpm();
    if (error) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP|APLOG_CRIT, 0, NULL, APLOGNO(00534)
                     "%s: Configuration error: %s", ap_server_argv0, error);
        return NULL;
    }

    error = process_command_config(s, ap_server_post_read_config, conftree,
                                   p, ptemp);

    if (error) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP|APLOG_CRIT, 0, NULL, "%s: %s",
                     ap_server_argv0, error);
        return NULL;
    }

    return s;
}

AP_DECLARE(void) ap_single_module_configure(apr_pool_t *p, server_rec *s,
                                            module *m)
{
    if (m->create_server_config)
        ap_set_module_config(s->module_config, m,
                             (*m->create_server_config)(p, s));

    if (m->create_dir_config)
        ap_set_module_config(s->lookup_defaults, m,
                             (*m->create_dir_config)(p, NULL));
}

AP_DECLARE(void) ap_run_rewrite_args(process_rec *process)
{
    module *m;

    for (m = ap_top_module; m; m = m->next) {
        if (m->rewrite_args) {
            (*m->rewrite_args)(process);
        }
    }
}

/********************************************************************
 * Configuration directives are restricted in terms of where they may
 * appear in the main configuration files and/or .htaccess files according
 * to the bitmask req_override in the command_rec structure.
 * If any of the overrides set in req_override are also allowed in the
 * context in which the command is read, then the command is allowed.
 * The context is determined as follows:
 *
 *    inside *.conf --> override = (RSRC_CONF|OR_ALL)&~(OR_AUTHCFG|OR_LIMIT);
 *    within <Directory> or <Location> --> override = OR_ALL|ACCESS_CONF;
 *    within .htaccess --> override = AllowOverride for current directory;
 *
 * the result is, well, a rather confusing set of possibilities for when
 * a particular directive is allowed to be used.  This procedure prints
 * in English where the given (pc) directive can be used.
 */
static void show_overrides(const command_rec *pc, module *pm)
{
    int n = 0;

    printf("\tAllowed in *.conf ");
    if ((pc->req_override & (OR_OPTIONS | OR_FILEINFO | OR_INDEXES))
        || ((pc->req_override & RSRC_CONF)
        && ((pc->req_override & (ACCESS_CONF | OR_AUTHCFG | OR_LIMIT))))) {
        printf("anywhere");
    }
    else if (pc->req_override & RSRC_CONF) {
        printf("only outside <Directory>, <Files>, <Location>, or <If>");
    }
    else {
        printf("only inside <Directory>, <Files>, <Location>, or <If>");
    }

    /* Warn if the directive is allowed inside <Directory> or .htaccess
     * but module doesn't support per-dir configuration
     */
    if ((pc->req_override & (OR_ALL | ACCESS_CONF)) && !pm->create_dir_config)
        printf(" [no per-dir config]");

    if (pc->req_override & OR_ALL) {
        printf(" and in .htaccess\n\twhen AllowOverride");

        if ((pc->req_override & OR_ALL) == OR_ALL) {
            printf(" isn't None");
        }
        else {
            printf(" includes ");

            if (pc->req_override & OR_AUTHCFG) {
                if (n++)
                    printf(" or ");

                printf("AuthConfig");
            }

            if (pc->req_override & OR_LIMIT) {
                if (n++)
                    printf(" or ");

                printf("Limit");
            }

            if (pc->req_override & OR_OPTIONS) {
                if (n++)
                    printf(" or ");

                printf("Options");
            }

            if (pc->req_override & OR_FILEINFO) {
                if (n++)
                    printf(" or ");

                printf("FileInfo");
            }

            if (pc->req_override & OR_INDEXES) {
                if (n++)
                    printf(" or ");

                printf("Indexes");
            }
        }
    }

    printf("\n");
}

/* Show the preloaded configuration directives, the help string explaining
 * the directive arguments, in what module they are handled, and in
 * what parts of the configuration they are allowed.  Used for httpd -L.
 */
AP_DECLARE(void) ap_show_directives(void)
{
    const command_rec *pc;
    int n;

    for (n = 0; ap_loaded_modules[n]; ++n) {
        for (pc = ap_loaded_modules[n]->cmds; pc && pc->name; ++pc) {
            printf("%s (%s)\n", pc->name, ap_loaded_modules[n]->name);

            if (pc->errmsg)
                printf("\t%s\n", pc->errmsg);

            show_overrides(pc, ap_loaded_modules[n]);
        }
    }
}

/* Show the preloaded module names.  Used for httpd -l. */
AP_DECLARE(void) ap_show_modules(void)
{
    int n;

    printf("Compiled in modules:\n");
    for (n = 0; ap_loaded_modules[n]; ++n)
        printf("  %s\n", ap_loaded_modules[n]->name);
}

AP_DECLARE(int) ap_exists_directive(apr_pool_t *p, const char *name)
{
    char *lname = apr_pstrdup(p, name);

    ap_str_tolower(lname);
    
    return ap_config_hash &&
        apr_hash_get(ap_config_hash, lname, APR_HASH_KEY_STRING) != NULL;
}

AP_DECLARE(void *) ap_retained_data_get(const char *key)
{
    void *retained;

    apr_pool_userdata_get((void *)&retained, key, ap_pglobal);
    return retained;
}

AP_DECLARE(void *) ap_retained_data_create(const char *key, apr_size_t size)
{
    void *retained;

    retained = apr_pcalloc(ap_pglobal, size);
    apr_pool_userdata_set((const void *)retained, key, apr_pool_cleanup_null, ap_pglobal);
    return retained;
}

static int count_directives_sub(const char *directive, ap_directive_t *current)
{
    int count = 0;
    while (current != NULL) {
        if (current->first_child != NULL)
            count += count_directives_sub(directive, current->first_child);
        if (ap_cstr_casecmp(current->directive, directive) == 0)
            count++;
        current = current->next;
    }
    return count;
}

AP_DECLARE(void) ap_reserve_module_slots(int count)
{
    reserved_module_slots += count;
}

AP_DECLARE(void) ap_reserve_module_slots_directive(const char *directive)
{
    ap_reserve_module_slots(count_directives_sub(directive, ap_conftree));
}
