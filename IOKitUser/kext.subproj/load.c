#include <CoreFoundation/CoreFoundation.h>

#include <libc.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/mach_error.h>
#include <mach/mach_host.h>
#include <mach/mach_port.h>
#include <mach-o/kld.h>
#include <mach-o/arch.h>
#include <mach-o/fat.h>

#include "load.h"
#include "dgraph.h"
#include "kld_patch.h"
#include "vers_rsrc.h"

static mach_port_t G_kernel_port = PORT_NULL;
static mach_port_t G_kernel_priv_port = PORT_NULL;
static int G_syms_only;


static void null_log(const char * format, ...);
static void null_err_log(const char * format, ...);
static int null_approve(int default_answer, const char * format, ...);
static int null_veto(int default_answer, const char * format, ...);
static const char * null_input(const char * format, ...);

// used by dgraph.c so can't be static
SInt32 log_level = 0;
void (*kload_log_func)(const char * format, ...) =
    &null_log;
void (*kload_err_log_func)(const char * format, ...) = &null_err_log;
int (*kload_approve_func)(int default_answer,
    const char * format, ...) = &null_approve;
int (*kload_veto_func)(int default_answer,
    const char * format, ...) = &null_veto;
const char * (*kload_input_func)(const char * format, ...) = &null_input;

// Used to pass info between kld library and callbacks
static dgraph_entry_t * G_current_load_entry = NULL;

/*******************************************************************************
*******************************************************************************/

static unsigned long linkedit_address(
    unsigned long size,
    unsigned long headers_size);
static void clear_kld_globals(void);
static KXKextManagerError patch_dgraph(dgraph_t * dgraph, const char * kernel_file);
static KXKextManagerError output_patches(
    dgraph_t * dgraph,
    const char * patch_file,
    const char * patch_dir,
    int ask_overwrite_symbols,
    int overwrite_symbols);

static int file_exists(const char * path);

/*******************************************************************************
*******************************************************************************/

/*******************************************************************************
*
*******************************************************************************/
void set_log_level(SInt32 level)
{
    log_level = level;
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void set_log_function(
    void (*func)(const char * format, ...))
{
    if (!func) {
        kload_log_func = &null_log;
    } else {
        kload_log_func = func;
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void set_error_log_function(void (*func)(const char * format, ...))
{
    if (!func) {
        kload_err_log_func = &null_err_log;
    } else {
        kload_err_log_func = func;
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void set_user_approve_function(int (*func)(int default_answer,
    const char * format, ...))
{
    if (!func) {
        kload_approve_func = &null_approve;
    } else {
        kload_approve_func = func;
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void set_user_veto_function(int (*func)(int default_answer,
    const char * format, ...))
{
    if (!func) {
        kload_veto_func = &null_veto;
    } else {
        kload_veto_func = func;
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void set_user_input_function(const char * (*func)(const char * format, ...))
{
    if (!func) {
        kload_input_func = &null_input;
    } else {
        kload_input_func = func;
    }
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void null_log(const char * format, ...)
{
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void null_err_log(const char * format, ...)
{
    return;
}

/*******************************************************************************
*
*******************************************************************************/
int null_approve(int default_answer, const char * format, ...)
{
    return 0;
}

/*******************************************************************************
*
*******************************************************************************/
int null_veto(int default_answer, const char * format, ...)
{
    return 1;
}

/*******************************************************************************
*
*******************************************************************************/
const char * null_input(const char * format, ...)
{
    return NULL;
}

/*******************************************************************************
* This function claims the option flags d and D for object file dependencies
* and in-kernel dependencies, respectively.
*******************************************************************************/
KXKextManagerError load_with_arglist(
    int argc, char **argv,
    const char * kernel_file,
    const char * patch_file, const char * patch_dir,
    const char * symbol_file, const char * symbol_dir,
    int do_load, int do_start_kmod,
    int interactive_level,
    int ask_overwrite_symbols, int overwrite_symbols)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    dgraph_error_t dgraph_result;
    int syms_only = (!do_load) && (symbol_file || symbol_dir);

    static dgraph_t dependency_graph;

   /* Zero out fields in dependency graph for proper error handling later.
    */
    bzero(&dependency_graph, sizeof(dependency_graph));

    dgraph_result = dgraph_init_with_arglist(&dependency_graph,
        syms_only, "-d", "-D", argc, argv);
    if (dgraph_result == dgraph_error) {
        (*kload_err_log_func)("error processing dependency list");
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    } else if (dgraph_result == dgraph_invalid) {
        // anything to print here, or did init call print something?
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    result = load_dgraph(&dependency_graph, kernel_file,
        patch_file, patch_dir, symbol_file, symbol_dir,
        do_load, do_start_kmod, interactive_level,
        ask_overwrite_symbols, overwrite_symbols);

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError load_dgraph(dgraph_t * dgraph,
    const char * kernel_file,
    const char * patch_file, const char * patch_dir,
    const char * symbol_file, const char * symbol_dir,
    int do_load, int do_start_kmod,
    int interactive_level,
    int ask_overwrite_symbols, int overwrite_symbols)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    int one_has_address = 0;
    int one_lacks_address = 0;
    int syms_only;
    unsigned int i;

    syms_only = (!do_load) && (symbol_dir || symbol_file);

    if (log_level >= kKXKextManagerLogLevelLoadDetails) {
        kload_log_func("loading dependency graph:");
        dgraph_log(dgraph);
    }

    if (syms_only && log_level >= kKXKextManagerLogLevelLoadDetails) {
        kload_log_func("loading for symbol generation only");
    }

   /*****
    * If we're not loading and have no request to emit a symbol
    * or patch file, there's nothing to do!
    */
    if (!do_load && !symbol_dir && !symbol_file &&
        !patch_dir && !patch_file) {

        if (syms_only && log_level >= kKXKextManagerLogLevelLoadDetails) {
            kload_log_func("loader has no work to do");
        }

        result = kKXKextManagerErrorNone;  // fixme: should this be USAGE error?
        goto finish;
    }

   /*****
    * If we're doing symbols only, then all entries in the dgraph must
    * have addresses assigned, or none must.
    */
    if (syms_only) {
        if (log_level >= kKXKextManagerLogLevelLoadDetails) {
            kload_log_func("checking whether modules have addresses assigned");
        }
        for (i = 0; i < dgraph->length; i++) {
            struct dgraph_entry_t * entry = dgraph->load_order[i];
            if (entry->is_kernel_component) {
                continue;
            }
            if (entry->loaded_address != 0) {
                one_has_address = 1;
            } else {
                one_lacks_address = 1;
            }
        }
    }

    if (one_has_address && one_lacks_address) {
        (*kload_err_log_func)(
            "either all modules must have addresses set nonzero values or "
            "none must");
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

   /* we need the priv port to check/load modules in the kernel.
    */
    if (PORT_NULL == G_kernel_priv_port) {
        G_kernel_priv_port = mach_host_self();  /* if we are privileged */
    }

   /*****
    * If we don't have addresses, then get them from the kernel.
    */
    if (!one_has_address && (do_load || symbol_file || symbol_dir)) {
        if (log_level >= kKXKextManagerLogLevelLoadDetails) {
            kload_log_func("getting module addresses from kernel");
        }
        result = loader_set_load_addresses_from_kernel(dgraph, kernel_file,
            do_load);
        if (result == kKXKextManagerErrorAlreadyLoaded) {
            if (do_load) {
                goto finish;
            }
        } else if (result != kKXKextManagerErrorNone) {
            (*kload_err_log_func)("can't check load addresses of modules");
            goto finish;
        }
    }

   /*****
    * At this point, if we're doing symbols only, it's an error to not
    * have a load address for every module.
    */
    if (syms_only) {
        if (log_level >= kKXKextManagerLogLevelLoadDetails) {
            kload_log_func("checking that all modules have addresses assigned");
        }
        for (i = 0; i < dgraph->length; i++) {
            struct dgraph_entry_t * entry = dgraph->load_order[i];
            if (entry->is_kernel_component) {
                continue;
            }
            if (!entry->loaded_address) {
                (*kload_err_log_func)(
                    "missing load address during symbol generation: %s",
                    entry->filename);
                result = kKXKextManagerErrorUnspecified;
                goto finish;
            }
       }
    }

    result = load_modules(dgraph, kernel_file,
        patch_file, patch_dir, symbol_file, symbol_dir,
        do_load, do_start_kmod, interactive_level,
        ask_overwrite_symbols, overwrite_symbols);

finish:

   /* Dispose of the host port to prevent security breaches and port
    * leaks. We don't care about the kern_return_t value of this
    * call for now as there's nothing we can do if it fails.
    */
    if (PORT_NULL != G_kernel_priv_port) {
        mach_port_deallocate(mach_task_self(), G_kernel_priv_port);
        G_kernel_priv_port = PORT_NULL;
    }

    for (i = 0; i < dgraph->length; i++) {
        dgraph_entry_t * current_entry = dgraph->graph[i];
        clean_up_entry(current_entry);
    }

    return result;
}

/*******************************************************************************
*******************************************************************************/

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError loader_map_dgraph(
    dgraph_t * dgraph,
    const char * kernel_file)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    int i;

    if (log_level >= kKXKextManagerLogLevelLoadDetails) {
        kload_log_func("mapping the kernel file %s", kernel_file);
    }

    if (!kld_file_map(kernel_file)) {
        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    for (i = 0; i < dgraph->length; i++) {
        dgraph_entry_t * entry = dgraph->load_order[i];

        if (entry->is_kernel_component) {
            continue;
        }

        result = map_entry(entry);
        if (result != kKXKextManagerErrorNone) {
            goto finish;
        }
    }

finish:
    return result;

}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError map_entry(dgraph_entry_t * entry)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    char version_string[KMOD_MAX_NAME];

    if (entry->is_kernel_component) {
        result = kKXKextManagerErrorInvalidArgument;
        goto finish;
    }

    if (log_level >= kKXKextManagerLogLevelLoadDetails) {
        kload_log_func("mapping module file %s", entry->filename);
    }

    if (entry->kmod_info) {
        if (log_level >= kKXKextManagerLogLevelLoadDetails) {
            kload_log_func("module file %s is already mapped", entry->filename);
        }
        result = kKXKextManagerErrorNone;
        goto finish;
    }

    if (!kld_file_map(entry->filename)) {
        kload_err_log_func("error mapping module file %s", entry->filename);

        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    // FIXME: Stop using this symbol; have the info passed in by
    // FIXME: ...the kext management library.
    entry->kmod_info = kld_file_lookupsymbol(entry->filename,
        "_kmod_info");
    if (!entry->kmod_info) {
        kload_err_log_func("%s does not not contain kernel extension code",
            entry->filename);
        result = kKXKextManagerErrorLoadExecutableBad;
        goto finish;
    }

    bzero(entry->kmod_info->name, sizeof(entry->kmod_info->name));
    strcpy(entry->kmod_info->name, entry->expected_kmod_name);

    if (!VERS_string(version_string, sizeof(version_string),
        entry->expected_kmod_vers)) {

        (*kload_err_log_func)(
            "can't render version string for module %s",
                entry->filename);
        result = kKXKextManagerErrorUnspecified;
        goto finish;
    }

    bzero(entry->kmod_info->version, sizeof(entry->kmod_info->version));
    strcpy(entry->kmod_info->version, version_string);

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
void clean_up_entry(dgraph_entry_t * entry) {
    int mach_result;

    if (entry->need_cleanup && entry->kernel_alloc_address) {
        mach_result = vm_deallocate(G_kernel_port, entry->kernel_alloc_address,
            entry->kernel_alloc_size);
        entry->kernel_alloc_address = 0;
    }

    return;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError loader_request_load_addresses(
    dgraph_t * dgraph,
    const char * kernel_file)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    int i;
    const char * user_response = NULL;  // must free
    int scan_result;
    unsigned int address;

   /* We have to map all object files to get their CFBundleIdentifier
    * names.
    */
    result = loader_map_dgraph(dgraph, kernel_file);
    if (result != kKXKextManagerErrorNone) {
        fprintf(stderr, "error mapping object files\n");
        goto finish;
    }

    printf("enter the hexadecimal load addresses for these modules:\n");

    for (i = 0; i < dgraph->length; i++) {
        dgraph_entry_t * entry = dgraph->load_order[i];

        if (!entry) {
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        }

        if (entry->is_kernel_component) {
            continue;
        }

        if (!entry->kmod_info) {
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        }

        user_response = kload_input_func("%s:",
            entry->kmod_info->name);
        if (!user_response) {
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        }
        scan_result = sscanf(user_response, "%x", &address);
        if (scan_result < 1 || scan_result == EOF) {
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        }
        entry->loaded_address = address;
    }

finish:
    return result;

}

/*******************************************************************************
* addresses is a NULL-terminated list of string of the form "module_id@address"
*******************************************************************************/
KXKextManagerError loader_set_load_addresses_from_args(
    dgraph_t * dgraph,
    const char * kernel_file,
    char ** addresses)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    int i, j;


   /* We have to map all object files to get their CFBundleIdentifier
    * names.
    */
    result = loader_map_dgraph(dgraph, kernel_file);
    if (result != kKXKextManagerErrorNone) {
        kload_err_log_func("error mapping object files");
        goto finish;
    }

   /*****
    * Run through and assign all addresses to their relevant module
    * entries.
    */
    for (i = 0; i < dgraph->length; i++) {
        dgraph_entry_t * entry = dgraph->load_order[i];

        if (!entry) {
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        }

        if (entry->is_kernel_component) {
            continue;
        }

        if (!entry->kmod_info) {
            result = kKXKextManagerErrorUnspecified;
            goto finish;
        }

        for (j = 0; addresses[j]; j++) {
            char * this_addr = addresses[j];
            char * address_string = NULL;
            unsigned int address;
            unsigned int module_namelen = strlen(entry->kmod_info->name);

            if (!this_addr) {
                result = kKXKextManagerErrorUnspecified;
                goto finish;
            }

            if (strncmp(this_addr, entry->kmod_info->name, module_namelen)) {
                continue;
            }
            if (this_addr[module_namelen] != '@') {
                continue;
            }

            address_string = index(this_addr, '@');
            if (!address_string) {
                result = kKXKextManagerErrorUnspecified;
                goto finish;
            }
            address_string++;
            address = strtoul(address_string, NULL, 16);
            entry->loaded_address = address;
        }
    }

   /*****
    * Now that we've done that see that all non-kernel modules do have
    * addresses set. If even one doesn't, we can't complete the link
    * relocation of symbols, so return a usage error.
    */
    for (i = 0; i < dgraph->length; i++) {
        dgraph_entry_t * entry = dgraph->load_order[i];

        if (entry->is_kernel_component) {
            continue;
        }

        if (!entry->loaded_address) {
            result = kKXKextManagerErrorInvalidArgument;
            goto finish;
        }
    }

finish:
    return result;

}

/*******************************************************************************
* This function requires G_kernel_priv_port to be set before it will work.
*******************************************************************************/
KXKextManagerError loader_set_load_addresses_from_kernel(
    dgraph_t * dgraph,
    const char * kernel_file,
    int do_load)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    int mach_result;
    kmod_info_t * loaded_modules = NULL;
    int           loaded_bytecount = 0;
    unsigned int i;


   /*****
    * We have to map the dgraph's modules before checking whether they've
    * been loaded.
    */
    result = loader_map_dgraph(dgraph, kernel_file);
    if (result != kKXKextManagerErrorNone) {
        (*kload_err_log_func)("can't map module files");
        goto finish;
    }


   /* First clear all the load addresses.
    */
    for (i = 0; i < dgraph->length; i++) {
        struct dgraph_entry_t * entry = dgraph->load_order[i];
        entry->loaded_address = 0;
    }

    mach_result = kmod_get_info(G_kernel_priv_port,
        (void *)&loaded_modules, &loaded_bytecount);
    if (mach_result != KERN_SUCCESS) {
        (*kload_err_log_func)("kmod_get_info() failed");
        result = kKXKextManagerErrorKernelError;
        goto finish;
    }

   /*****
    * Find out which modules have already been loaded & verify
    * that loaded versions are same as requested.
    */
    for (i = 0; i < dgraph->length; i++) {
        KXKextManagerError cresult;
        dgraph_entry_t * current_entry = dgraph->load_order[i];

       /* If necessary, check whether the current module is already loaded.
        * (We already did the root module above.)
        */
        cresult = check_module_loaded(dgraph, current_entry,
            loaded_modules, do_load);
        if ( ! (cresult == kKXKextManagerErrorNone ||
                cresult == kKXKextManagerErrorAlreadyLoaded) ) {
            goto finish;
        }
        if (current_entry == dgraph->root &&
            cresult == kKXKextManagerErrorAlreadyLoaded) {

            result = cresult;
        }
    }

finish:

    if (loaded_modules) {
        vm_deallocate(mach_task_self(), (vm_address_t)loaded_modules,
            loaded_bytecount);
        loaded_modules = 0;
    }

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError check_module_loaded(
    dgraph_t * dgraph,
    dgraph_entry_t * entry,
    kmod_info_t * kmod_list,
    int log_if_already)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    const char * kmod_name;
    unsigned int i;

    if (entry->is_kernel_component) {
        kmod_name = entry->filename;
    } else {
        kmod_name = entry->kmod_info->name;
        if (log_level >= kKXKextManagerLogLevelLoadDetails) {
            kload_log_func("checking whether module file %s is already loaded",
                kmod_name);
        }
    }

    for (i = 0; /* see bottom of loop */; i++) {
        kmod_info_t * current_kmod = &(kmod_list[i]);

        if (0 == strcmp(current_kmod->name, kmod_name)) {
            UInt32 entry_vers;
            UInt32 loaded_vers;

            entry->do_load = 0;
            entry->kmod_id = kmod_list[i].id;
            entry->loaded_address = current_kmod->address;

            if (entry->is_kernel_component) {
                goto finish;
            }

            if (log_level >= kKXKextManagerLogLevelLoadDetails) {
                kload_log_func("module file %s is loaded; checking status",
                    kmod_name);
            }

            // We really want to move away from having this info in a kmod....
            //
            if (!VERS_parse_string(current_kmod->version, &loaded_vers)) {
                (*kload_err_log_func)(
                    "can't parse version string \"%s\" of loaded module %s",
                    current_kmod->version,
                    current_kmod->name);
                result = kKXKextManagerErrorUnspecified;
                goto finish;
            }

            if (!VERS_parse_string(entry->kmod_info->version, &entry_vers)) {
                (*kload_err_log_func)(
                    "can't parse version string \"%s\" of module file %s",
                    entry->kmod_info->version,
                    kmod_name);
                result = kKXKextManagerErrorUnspecified;
                goto finish;
            }

            if (loaded_vers != entry_vers) {
                (*kload_err_log_func)(
                    "loaded version %s of module %s differs from "
                    "requested version %s",
                    current_kmod->version,
                    current_kmod->name,
                    entry->kmod_info->version);
                result = kKXKextManagerErrorLoadedVersionDiffers;
                goto finish;
            } else {

                if (log_if_already && log_level >=
                        kKXKextManagerLogLevelLoadBasic) {

                    (*kload_log_func)(
                        "module %s (identifier %s) is already loaded",
                        entry->filename, kmod_name);
                }
                result = kKXKextManagerErrorAlreadyLoaded;
                goto finish;
            }

            goto finish;
        }

        if (kmod_list[i].next == 0) {
            break;
        }

    } /* for loop */

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError load_modules(dgraph_t * dgraph,
    const char * kernel_file,
    const char * patch_file,
    const char * patch_dir,
    const char * symbol_file,
    const char * symbol_dir,
    int do_load,
    int do_start_kmod,
    int interactive_level,
    int ask_overwrite_symbols,
    int overwrite_symbols)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    char * kernel_base_addr = 0;
    long int kernel_size = 0;
    kern_return_t mach_result = KERN_SUCCESS;
    int kld_result;
    Boolean cleanup_kld_loader = false;
    unsigned int i;

   /* We have to map all object files to get their CFBundleIdentifier
    * names.
    */
    result = loader_map_dgraph(dgraph, kernel_file);
    if (result != kKXKextManagerErrorNone) {
        fprintf(stderr, "error mapping object files\n");
        goto finish;
    }

    result = patch_dgraph(dgraph, kernel_file);
    if (result != kKXKextManagerErrorNone) {
        // FIXME: print an error message here?
        goto finish;
    }

    // FIXME: check error return
    output_patches(dgraph, patch_file, patch_dir,
        ask_overwrite_symbols, overwrite_symbols);

   /*****
    * If we're not loading or writing symbols, we're done.
    */
    if (!do_load && !symbol_file && !symbol_dir) {
        goto finish;
    }

    if (do_load && PORT_NULL == G_kernel_port) {
        mach_result = task_for_pid(mach_task_self(), 0, &G_kernel_port);
        if (mach_result != KERN_SUCCESS) {
            (*kload_err_log_func)("unable to get kernel task port: %s",
                mach_error_string(mach_result));
            (*kload_err_log_func)("you must be running as root to load "
                "modules into the kernel");
            result = kKXKextManagerErrorKernelPermission;
            goto finish;
        }
    }

    kld_address_func(&linkedit_address);
    G_syms_only = (!do_load) && (symbol_file || symbol_dir);

    kernel_base_addr = kld_file_getaddr(kernel_file, &kernel_size);
    if (!kernel_base_addr) {
        (*kload_err_log_func)(
            "can't get load address for kernel %s", kernel_file);
        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    kld_result = kld_load_basefile_from_memory(kernel_file, kernel_base_addr,
        kernel_size);
    fflush(stdout);
    fflush(stderr);
    if (!kld_result) {
        (*kload_err_log_func)("can't read kernel %s", kernel_file);
        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    cleanup_kld_loader = true;

    for (i = 0; i < dgraph->length; i++) {
        dgraph_entry_t * current_entry = dgraph->load_order[i];

        result = load_module(current_entry, (current_entry == dgraph->root),
            symbol_file, symbol_dir, do_load,
            interactive_level, ask_overwrite_symbols, overwrite_symbols);
        if (result != kKXKextManagerErrorNone) {
            goto finish;
        }

        if (do_load && current_entry->do_load) {
            result = set_module_dependencies(current_entry);
            if ( ! (result == kKXKextManagerErrorNone ||
                    result == kKXKextManagerErrorAlreadyLoaded) ) {
                goto finish;
            }

            if ( (interactive_level == 1 && current_entry == dgraph->root) ||
                 (interactive_level == 2) ) {

                int approve = (*kload_approve_func)(1,
                    "\nStart module %s (ansering no will abort the load)",
                    current_entry->filename);

                if (approve > 0) {
                    do_start_kmod = true; // override 'cause user said so
                } else {
                    kern_return_t mach_result;
                    if (approve < 0) {
                         kload_log_func("error reading user response; "
                            "destroying loaded module");
                    } else {
                         kload_log_func("user canceled module start; "
                            "destroying loaded module");
                    }
                    mach_result = kmod_destroy(G_kernel_priv_port,
                        current_entry->kmod_id);
                    if (mach_result != KERN_SUCCESS) {
                        (*kload_err_log_func)("kmod_destroy() failed");
                    }
                    if (approve < 0) {
                        result = kKXKextManagerErrorUnspecified;
                        goto finish;
                    } else {
                        result = kKXKextManagerErrorUserAbort;
                        goto finish;
                    }
                }
            }

            if (current_entry != dgraph->root ||
                (current_entry == dgraph->root && do_start_kmod)) {

                result = start_module(current_entry);
                if ( ! (result == kKXKextManagerErrorNone ||
                        result == kKXKextManagerErrorAlreadyLoaded) ) {
                    goto finish;
                } else if (interactive_level ||
                           log_level >= kKXKextManagerLogLevelLoadDetails) {

                     kload_log_func("started module %s",
                        current_entry->filename);
                }
            }
        }
    }

finish:

   /* Dispose of the kernel port to prevent security breaches and port
    * leaks. We don't care about the kern_return_t value of this
    * call for now as there's nothing we can do if it fails.
    */
    if (PORT_NULL != G_kernel_port) {
        mach_port_deallocate(mach_task_self(), G_kernel_port);
        G_kernel_port = PORT_NULL;
    }

    if (cleanup_kld_loader) {
        kld_unload_all(1);
    }
    kld_file_cleanup_all_resources();

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError patch_dgraph(dgraph_t * dgraph, const char * kernel_file)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    unsigned int i;

    if (!kld_file_merge_OSObjects(kernel_file)) {
        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    for (i = 0; i < dgraph->length; i++) {
        dgraph_entry_t * current_entry = dgraph->load_order[i];

       /* The kernel has already been patched.
        */
        if (current_entry->is_kernel_component) {
            continue;
        }

        if (log_level >= kKXKextManagerLogLevelLoadDetails) {
            kload_log_func("patching C++ code in module %s",
                current_entry->filename);
        }

        if (!kld_file_patch_OSObjects(current_entry->filename)) {
            result = kKXKextManagerErrorLinkLoad;   // FIXME: need a "patch" error?
            goto finish;
        }

    }

    if (!kld_file_prepare_for_link()) {
        result = kKXKextManagerErrorLinkLoad;   // FIXME: need more specific error?
        goto finish;
    }

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
#define PATCH_EXTENSION ".patch"

KXKextManagerError output_patches(
    dgraph_t * dgraph,
    const char * patch_file,
    const char * patch_dir,
    int ask_overwrite_symbols,
    int overwrite_symbols)
{
    KXKextManagerError result = kKXKextManagerErrorNone;
    unsigned int i;
    char * allocated_filename = NULL;
    char * patch_filename = NULL;
    int file_check;
    int output_patch;

    if (patch_dir) {

        for (i = 0; i < dgraph->length; i++) {

            struct dgraph_entry_t * entry = dgraph->load_order[i];
            unsigned long length;

            if (entry->is_kernel_component) {
                continue;
            }

            length = strlen(patch_dir) +
                strlen(entry->kmod_info->name) + strlen(PATCH_EXTENSION) + 1;
            if (length >= MAXPATHLEN) {
                (*kload_err_log_func)(
                    "output filename \"%s/%s%s\" would be too long",
                    patch_dir, entry->kmod_info->name, PATCH_EXTENSION);
                result = kKXKextManagerErrorInvalidArgument;
                goto finish;
            }

            allocated_filename = (char *)malloc(length);
            if (! allocated_filename) {
                (*kload_err_log_func)("malloc failure");
                result = kKXKextManagerErrorNoMemory;
                goto finish;
            }

            patch_filename = allocated_filename;
            strcpy(patch_filename, patch_dir);
            strcat(patch_filename, "/");
            strcat(patch_filename, entry->kmod_info->name);
            strcat(patch_filename, PATCH_EXTENSION);

            output_patch = 1;
            file_check = file_exists(patch_filename);

            if (file_check < 0) {
                (*kload_err_log_func)("error checking existence of file %s",
                    patch_filename);
            } else if (file_check > 0 && !overwrite_symbols) {
                if (!ask_overwrite_symbols) {
                    (*kload_err_log_func)(
                        "patch file %s exists; not overwriting",
                        patch_filename);
                    output_patch = 0;
                } else {
                    int approve = (*kload_approve_func)(1,
                        "\nPatch file %s exists; overwrite", patch_filename);

                    if (approve < 0) {
                        result = kKXKextManagerErrorUnspecified;
                        goto finish;
                    } else {
                        output_patch = approve;
                    }
                }
            }

            if (output_patch) {
                if (log_level >= kKXKextManagerLogLevelBasic) {
                    (*kload_log_func)("writing patch file %s", patch_filename);
                }
                kld_file_debug_dump(entry->filename, patch_filename);
            }

            if (allocated_filename) free(allocated_filename);
            allocated_filename = NULL;
        }

    } else if (patch_file) {
        output_patch = 1;
        file_check = file_exists(patch_file);

        if (file_check < 0) {
            (*kload_err_log_func)("error checking existence of file %s",
                patch_file);
        } else if (file_check > 0 && !overwrite_symbols) {
            if (!ask_overwrite_symbols) {
                (*kload_err_log_func)("patch file %s exists; not overwriting",
                    patch_filename);
                output_patch = 0;
            } else {
                int approve = (*kload_approve_func)(1,
                    "\nPatch file %s exists; overwrite", patch_filename);

                if (approve < 0) {
                    result = kKXKextManagerErrorUnspecified;
                    goto finish;
                } else {
                    output_patch = approve;
                }
            }
        }

        if (output_patch) {
            if (log_level >= kKXKextManagerLogLevelBasic) {
                (*kload_log_func)("writing patch file %s", patch_filename);
            }
            kld_file_debug_dump(dgraph->root->filename, patch_file);
        }
    }

finish:
    if (allocated_filename) free(allocated_filename);

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
#define SYMBOL_EXTENSION   ".sym"

KXKextManagerError load_module(dgraph_entry_t * entry,
    int is_root,
    const char * symbol_file,
    const char * symbol_dir,
    int do_load,
    int interactive_level,
    int ask_overwrite_symbols,
    int overwrite_symbols)
{
    KXKextManagerError result = kKXKextManagerErrorNone;

    int kld_result;
    struct mach_header * kld_header;
    char * object_addr = 0;
    long int object_size = 0;
    char * allocated_filename = NULL;
    char * symbol_filename = NULL;
    const char * kmod_symbol = "_kmod_info";
    unsigned long kernel_kmod_info;
    kmod_info_t * local_kmod_info = NULL;
    vm_address_t vm_buffer = 0;
    int file_check;

   /* A kernel component is by nature already linked and loaded and has
    * no work to be done upon it.
    */
    if (entry->is_kernel_component) {
        result = kKXKextManagerErrorNone;
        goto finish;
    }

    G_current_load_entry = entry;

    if (log_level >= kKXKextManagerLogLevelLoadBasic) {
        if (do_load) {
            (*kload_log_func)("link/loading file %s", entry->filename);
        } else {
            (*kload_log_func)("linking file %s", entry->filename);
        }
    }

    if (symbol_dir) {
        unsigned long length = strlen(symbol_dir) +
            strlen(entry->kmod_info->name) + strlen(SYMBOL_EXTENSION) + 1;
        if (length >= MAXPATHLEN) {
            (*kload_err_log_func)(
                "output symbol filename \"%s/%s%s\" would be too long",
                symbol_dir, entry->kmod_info->name, SYMBOL_EXTENSION);
            result = kKXKextManagerErrorInvalidArgument;
            goto finish;
        }

        allocated_filename = (char *)malloc(length);
        if (!allocated_filename) {
            (*kload_err_log_func)("memory allocation failure");
            result = kKXKextManagerErrorNoMemory;
            goto finish;
        }

        symbol_filename = allocated_filename;
        strcpy(symbol_filename, symbol_dir);
        strcat(symbol_filename, "/");
        strcat(symbol_filename, entry->kmod_info->name);
        strcat(symbol_filename, SYMBOL_EXTENSION);

    } else if (symbol_file && (is_root)) {
        symbol_filename = (char *)symbol_file;
    }

    if (symbol_filename) {
        file_check = file_exists(symbol_filename);
        if (file_check < 0) {
            (*kload_err_log_func)("error checking existence of file %s",
                symbol_filename);
        } else if (file_check > 0 && !overwrite_symbols) {
            if (!ask_overwrite_symbols) {
                (*kload_err_log_func)("symbol file %s exists; not overwriting",
                    symbol_filename);
                symbol_filename = NULL;
            } else {
                int approve = (*kload_approve_func)(1,
                    "\nSymbol file %s exists; overwrite", symbol_filename);

                if (approve < 0) {
                    result = kKXKextManagerErrorUnspecified;
                    goto finish;
                } else if (approve == 0) {
                    if (allocated_filename) free(allocated_filename);
                    allocated_filename = NULL;
                    symbol_filename = NULL;
                }
            }
        }
    }

    if (symbol_filename &&
        (interactive_level ||
         log_level >= kKXKextManagerLogLevelBasic) ) {

        (*kload_log_func)("writing symbol file %s", symbol_filename);
    }

    if (do_load) {
        if (interactive_level && entry->loaded_address) {
            (*kload_log_func)(
                "module %s is already loaded as %s at address 0x%08x",
                entry->filename, entry->kmod_info->name,
                entry->loaded_address);
        } else if ( (interactive_level == 1 && is_root) ||
             (interactive_level == 2) ) {

            int approve = (*kload_approve_func)(1,
                "\nLoad module %s", entry->filename);

            if (approve < 0) {
                result = kKXKextManagerErrorUnspecified;
                goto finish;
            } else if (approve == 0) {
                result = kKXKextManagerErrorUserAbort;
                goto finish;
            }
        }
    }

    object_addr = kld_file_getaddr(entry->filename, &object_size);
    if (!object_addr) {
        (*kload_err_log_func)("kld_file_getaddr() failed for module %s",
            entry->filename);
        clear_kld_globals();
        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    kld_result = kld_load_from_memory(&kld_header, entry->filename,
        object_addr, object_size, symbol_filename);
    fflush(stdout);
    fflush(stderr);

    if (!kld_result || !entry->kernel_load_address) {
        (*kload_err_log_func)("kld_load_from_memory() failed for module %s",
            entry->filename);
        clear_kld_globals();
        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    kld_result = kld_lookup(kmod_symbol, &kernel_kmod_info);
    if (!kld_result) {
        (*kload_err_log_func)("kld_lookup(\"%s\") failed for module %s",
            kmod_symbol, entry->filename);
        entry->need_cleanup = 1;
        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    kld_result = kld_forget_symbol(kmod_symbol);
    fflush(stdout);
    if (!kld_result) {
        (*kload_err_log_func)("kld_forget_symbol(\"%s\") failed for module %s",
            kmod_symbol, entry->filename);
        entry->need_cleanup = 1;
        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

   /* Get the kmod_info by translating from the kernel-space address
    * at kernel_kmod_info.
    */
    local_kmod_info = (kmod_info_t *)(kernel_kmod_info -
        (unsigned long)G_current_load_entry->kernel_load_address +
        (unsigned long)kld_header);
    if (log_level >= kKXKextManagerLogLevelLoadBasic) {
        (*kload_log_func)("kmod name: %s", local_kmod_info->name);
        (*kload_log_func)("kmod start @ 0x%x",
           (vm_address_t)local_kmod_info->start);
        (*kload_log_func)("kmod stop @ 0x%x",
           (vm_address_t)local_kmod_info->stop);
    }

    if (!local_kmod_info->start || !local_kmod_info->start) {
        (*kload_err_log_func)(
            "error for module file %s; start or stop address is zero",
            entry->filename);
        entry->need_cleanup = 1;
        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

   /* Record link info into kmod_info struct, rounding the hdr_size
    * to fit the adjustment that was made in linkedit_address().
    */
    local_kmod_info->address = entry->kernel_alloc_address;
    local_kmod_info->size = entry->kernel_alloc_size;
    local_kmod_info->hdr_size = round_page(entry->kernel_hdr_size);

    if (do_load && entry->do_load) {
        int mach_result;

        mach_result = vm_allocate(mach_task_self(), &vm_buffer,
            entry->kernel_alloc_size, TRUE);
        if (mach_result != KERN_SUCCESS) {
            (*kload_err_log_func)("unable to vm_allocate() copy buffer");
            entry->need_cleanup = 1;
            result = kKXKextManagerErrorNoMemory;  // FIXME: kernel error?
            goto finish;
        }

        memcpy((void *)vm_buffer, kld_header, entry->kernel_hdr_size);
        memcpy((void *)vm_buffer + round_page(entry->kernel_hdr_size),
               (void *)((unsigned long)kld_header + entry->kernel_hdr_size),
               entry->kernel_load_size - entry->kernel_hdr_size);

        mach_result = vm_write(G_kernel_port, entry->kernel_alloc_address,
            vm_buffer, entry->kernel_alloc_size);
        if (mach_result != KERN_SUCCESS) {
            (*kload_err_log_func)("unable to write module to kernel memory");
            entry->need_cleanup = 1;
            result = kKXKextManagerErrorKernelError;
            goto finish;
        }

        mach_result = kmod_create(G_kernel_priv_port,
            (vm_address_t)kernel_kmod_info, &(entry->kmod_id));
        if (mach_result != KERN_SUCCESS) {
            (*kload_err_log_func)("unable to register module with kernel");
            entry->need_cleanup = 1;
            result = kKXKextManagerErrorKernelError;
            goto finish;
        }

        if (interactive_level || log_level >= kKXKextManagerLogLevelLoadBasic) {
            (*kload_log_func)(
                "module %s created as # %d at address 0x%x, size %ld",
                entry->kmod_info->name, entry->kmod_id,
                entry->kernel_alloc_address,
                entry->kernel_alloc_size);
        }
        if (interactive_level) {
            (*kload_log_func)(
                "You can now break to the debugger and set breakpoints "
                " for this extension.");
        }
    }

finish:

    if (allocated_filename) {
        free(allocated_filename);
    }
    if (vm_buffer) {
        vm_deallocate(mach_task_self(), vm_buffer, entry->kernel_alloc_size);
    }
    clear_kld_globals();

    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError set_module_dependencies(dgraph_entry_t * entry) {
    KXKextManagerError result = kKXKextManagerErrorNone;
    int mach_result;
    void * kmod_control_args = 0;
    int num_args = 0;
    unsigned int i;
    dgraph_entry_t * current_dep = NULL;

    if (!entry->do_load) {
        result = kKXKextManagerErrorAlreadyLoaded;
        goto finish;
    }

    for (i = 0; i < entry->num_dependencies; i++) {
        current_dep = entry->dependencies[i];

        if (log_level >= kKXKextManagerLogLevelLoadDetails) {
            (*kload_log_func)("adding reference from %s (%d) to %s (%d)",
                entry->expected_kmod_name, entry->kmod_id,
                current_dep->expected_kmod_name, current_dep->kmod_id);
        }
        mach_result = kmod_control(G_kernel_priv_port,
            KMOD_PACK_IDS(entry->kmod_id, current_dep->kmod_id),
            KMOD_CNTL_RETAIN, &kmod_control_args, &num_args);
        if (mach_result != KERN_SUCCESS) {
            (*kload_err_log_func)(
                "kmod_control/retain failed for %s; destroying kmod",
                entry->kmod_info->name);
            mach_result = kmod_destroy(G_kernel_priv_port, entry->kmod_id);
            if (mach_result != KERN_SUCCESS) {
                (*kload_err_log_func)("kmod_destroy() failed");
            }
            result = kKXKextManagerErrorLinkLoad;
            goto finish;
        }
    }

    if (log_level >= kKXKextManagerLogLevelLoadBasic) {
        (*kload_log_func)("module # %d reference counts incremented",
            entry->kmod_id);
    }

finish:
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
KXKextManagerError start_module(dgraph_entry_t * entry) {
    KXKextManagerError result = kKXKextManagerErrorNone;
    int mach_result;
    void * kmod_control_args = 0;
    int num_args = 0;

    if (!entry->do_load) {
        result = kKXKextManagerErrorAlreadyLoaded;
        goto finish;
    }

    mach_result = kmod_control(G_kernel_priv_port,
        entry->kmod_id, KMOD_CNTL_START, &kmod_control_args, &num_args);
    if (mach_result != KERN_SUCCESS) {
        (*kload_err_log_func)(
            "kmod_control/start failed for %s; destroying kmod",
            entry->kmod_info->name);
        mach_result = kmod_destroy(G_kernel_priv_port, entry->kmod_id);
        if (mach_result != KERN_SUCCESS) {
            (*kload_err_log_func)("kmod_destroy() failed");
        }
        result = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    if (log_level >= kKXKextManagerLogLevelLoadBasic) {
        (*kload_log_func)("module # %d started",
           entry->kmod_id);
    }

finish:
    return result;
}

/*******************************************************************************
*******************************************************************************/

/*******************************************************************************
*
*******************************************************************************/
static unsigned long linkedit_address(
    unsigned long size,
    unsigned long headers_size)
{
    unsigned long round_segments_size;
    unsigned long round_headers_size;
    unsigned long round_size;
    int mach_result;

    if (!G_current_load_entry) {
        return 0;
    }

    // the actual size allocated by kld_load_from_memory()
    G_current_load_entry->kernel_load_size = size;

    round_headers_size = round_page(headers_size);
    round_segments_size = round_page(size - headers_size);
    round_size = round_headers_size + round_segments_size;

    G_current_load_entry->kernel_alloc_size = round_size;

    // will need to be rounded *after* load/link
    G_current_load_entry->kernel_hdr_size = headers_size;
    G_current_load_entry->kernel_hdr_pad = round_headers_size - headers_size;

    if (G_current_load_entry->loaded_address) {
        G_current_load_entry->kernel_load_address =
            G_current_load_entry->loaded_address +
            G_current_load_entry->kernel_hdr_pad;
        if (log_level >= kKXKextManagerLogLevelLoadBasic) {
            (*kload_log_func)(
                "using %s load address 0x%x (0x%x with header pad)",
                G_current_load_entry->kmod_id ? "existing" : "provided",
                G_current_load_entry->loaded_address,
                G_current_load_entry->kernel_load_address);
        }
        return G_current_load_entry->kernel_load_address;
    }
    if (G_syms_only) {
        (*kload_err_log_func)(
            "internal error; asked to allocate kernel memory");
        // FIXME: no provision for cleanup here
        return kKXKextManagerErrorUnspecified;
    }

    mach_result = vm_allocate(G_kernel_port,
        &G_current_load_entry->kernel_alloc_address,
        G_current_load_entry->kernel_alloc_size, TRUE);
    if (mach_result != KERN_SUCCESS) {
        (*kload_err_log_func)("can't allocate kernel memory");
        // FIXME: no provision for cleanup here
        return kKXKextManagerErrorKernelError;
    }

    if (log_level >= kKXKextManagerLogLevelLoadBasic) {
        (*kload_log_func)("allocated %ld bytes in kernel space at 0x%x",
            G_current_load_entry->kernel_alloc_size,
            G_current_load_entry->kernel_alloc_address);
    }

    G_current_load_entry->kernel_load_address =
        G_current_load_entry->kernel_alloc_address +
        G_current_load_entry->kernel_hdr_pad;

    if (log_level >= kKXKextManagerLogLevelLoadBasic) {
        (*kload_log_func)(
            "using load address of 0x%x (0x%x with header pad)",
            G_current_load_entry->kernel_alloc_address,
            G_current_load_entry->kernel_load_address);
    }

    return G_current_load_entry->kernel_load_address;
}

/*******************************************************************************
*
*******************************************************************************/
void clear_kld_globals(void) {
    G_current_load_entry = NULL;
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void kld_error_vprintf(const char * format, va_list ap) {
    vfprintf(stderr, format, ap);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
static int file_exists(const char * path)
{
    int result = 0;  // assume it doesn't exist
    struct stat stat_buf;

    if (stat(path, &stat_buf) == 0) {
        result = 1;  // the file does exist; we don't care beyond that
        goto finish;
    }

    switch (errno) {
      case ENOENT:
        result = 0;  // the file doesn't exist
        goto finish;
        break;
      default:
        result = -1;  // unknown error
        goto finish;
        break;
    }

finish:
    return result;
}
