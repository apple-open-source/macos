#ifndef __LOAD_H__
#define __LOAD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "dgraph.h"
#include "KXKext.h"


void set_log_level(SInt32 level);
void set_log_function(void (*)(const char * format, ...));
void set_error_log_function(void (*)(const char * format, ...));
void set_user_approve_function(int (*)(int default_answer,
    const char * format, ...));
void set_user_veto_function(int (*)(int default_answer,
    const char * format, ...));
void set_user_input_function(const char * (*)(const char * format, ...));


KXKextManagerError load_with_arglist(
    int argc, char **argv,
    const char * kernel_file,
    const char * patch_file, const char * patch_dir,
    const char * symbol_file, const char * symbol_dir,
    int do_load, int do_start_kmod,
    int interactive_level,
    int ask_overwrite_symbols, int overwrite_symbols);
KXKextManagerError load_dgraph(dgraph_t * dgraph,
    const char * kernel_file,
    const char * patch_file, const char * patch_dir,
    const char * symbol_file, const char * symbol_dir,
    int do_load, int do_start_kmod,
    int interactive_level,
    int ask_overwrite_symbols, int overwrite_symbols);

KXKextManagerError loader_map_dgraph(dgraph_t * dgraph, const char * kernel_file);
KXKextManagerError map_entry(dgraph_entry_t * entry);
void clean_up_entry(dgraph_entry_t * entry);

KXKextManagerError loader_request_load_addresses(
    dgraph_t * dgraph,
    const char * kernel_file);
KXKextManagerError loader_set_load_addresses_from_args(
    dgraph_t * dgraph,
    const char * kernel_file,
    char ** addresses);
KXKextManagerError loader_set_load_addresses_from_kernel(
    dgraph_t * dgraph,
    const char * kernel_file,
    int do_load);

KXKextManagerError check_module_loaded(
    dgraph_t * dgraph,
    dgraph_entry_t * entry,
    kmod_info_t * kmod_list,
    int log_if_already);
KXKextManagerError load_modules(dgraph_t * dgraph,
    const char * kernel_file,
    const char * patch_file, const char * patch_dir,
    const char * symbol_file, const char * symbol_dir,
    int do_load, int do_start_kmod,
    int interactive_level,
    int ask_overwrite_symbols, int overwrite_symbols);
KXKextManagerError load_module(dgraph_entry_t * entry,
    int is_root,
    const char * symbol_file,
    const char * symbol_dir,
    int do_load,
    int interactive_level,
    int ask_overwrite_symbols,
    int overwrite_symbols);
KXKextManagerError set_module_dependencies(dgraph_entry_t * entry);
KXKextManagerError start_module(dgraph_entry_t * entry);

#endif __LOAD_H__

#ifdef __cplusplus
}
#endif

