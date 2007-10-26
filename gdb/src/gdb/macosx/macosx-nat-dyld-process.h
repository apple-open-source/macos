#ifndef __GDB_MACOSX_NAT_DYLD_PROCESS_H__
#define __GDB_MACOSX_NAT_DYLD_PROCESS_H__

#include "defs.h"

struct macosx_inferior_status;
struct macosx_dyld_thread_status;

struct dyld_objfile_info;
struct dyld_path_info;
struct dyld_objfile_entry;

struct dyld_event;

struct target_ops;

struct dyld_objfile_entry *dyld_lookup_objfile_entry (struct dyld_objfile_info * info, struct objfile * o);

enum dyld_reload_result
{
  DYLD_NO_CHANGE = 0,
  DYLD_UPGRADE,
  DYLD_DOWNGRADE
};

enum dyld_reload_result dyld_should_reload_objfile_for_flags (struct dyld_objfile_entry * e);

void dyld_add_inserted_libraries (struct dyld_objfile_info * info,
                                  const struct dyld_path_info * d);

void dyld_add_image_libraries (struct dyld_objfile_info * info, bfd *abfd);

void dyld_resolve_shlibs_internal (const struct macosx_inferior_status *s,
                                   struct dyld_objfile_info * new);

void dyld_resolve_shlibs_dyld (const struct macosx_inferior_status *s,
                               struct dyld_objfile_info * new);

void dyld_load_library (const struct dyld_path_info * d,
                        struct dyld_objfile_entry * e);

void dyld_load_libraries (const struct dyld_path_info * d,
                          struct dyld_objfile_info * result);

void dyld_merge_libraries (struct dyld_objfile_info * old,
                           struct dyld_objfile_info * new,
                           struct dyld_objfile_info * result);

void dyld_prune_shlib (struct dyld_path_info * d,
		       struct dyld_objfile_info * old,
                       struct dyld_objfile_entry * n);

void dyld_merge_shlibs (const struct macosx_dyld_thread_status *s,
                        struct dyld_path_info * d,
                        struct dyld_objfile_info * old,
                        struct dyld_objfile_info * new);

void remove_objfile_from_dyld_records (struct objfile *);

int dyld_is_objfile_loaded (struct objfile *obj);

void dyld_remove_objfile (struct dyld_objfile_entry *);

void dyld_clear_objfile (struct dyld_objfile_entry *);

void dyld_remove_objfiles (const struct dyld_path_info * d,
                           struct dyld_objfile_info * result);

void dyld_remove_duplicates (struct dyld_path_info * d,
                             struct dyld_objfile_info * result);

void dyld_check_discarded (struct dyld_objfile_info * info);

void dyld_purge_cached_libraries (struct dyld_objfile_info * info);

void dyld_update_shlibs (struct dyld_path_info * d,
                         struct dyld_objfile_info * result);

void dyld_merge_shlib (const struct macosx_dyld_thread_status *s,
                       struct dyld_path_info * d,
                       struct dyld_objfile_info * old,
                       struct dyld_objfile_entry * n);

int dyld_libraries_compatible (struct dyld_path_info *d,
                           struct dyld_objfile_entry *f,
                           struct dyld_objfile_entry *l);

int dyld_objfile_allocated (struct objfile * o);

int dyld_parse_load_level (const char *s);

int dyld_minimal_load_flag (const struct dyld_path_info * d,
                            struct dyld_objfile_entry * e);

int dyld_default_load_flag (const struct dyld_path_info * d,
                            struct dyld_objfile_entry * e);

char *dyld_find_dylib_name (CORE_ADDR addr, int ncmds);

void dyld_load_symfile (struct dyld_objfile_entry *e);

void dyld_load_symfile_preserving_objfile (struct dyld_objfile_entry *e);

struct pre_run_memory_map *create_pre_run_memory_map (struct bfd *abfd);

void free_pre_run_memory_map (struct pre_run_memory_map *map);


#endif /* __GDB_MACOSX_NAT_DYLD_PROCESS_H__ */
