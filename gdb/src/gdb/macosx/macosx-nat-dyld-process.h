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

struct dyld_objfile_entry *dyld_lookup_objfile_entry
PARAMS ((struct dyld_objfile_info *info, struct objfile *o));

void dyld_add_inserted_libraries
PARAMS ((struct dyld_objfile_info *info, const struct dyld_path_info *d));

void dyld_add_image_libraries
PARAMS ((struct dyld_objfile_info *info, bfd *abfd));

void dyld_resolve_shlibs_internal
PARAMS ((const struct macosx_inferior_status *s, struct dyld_objfile_info *new));

void dyld_resolve_shlibs_dyld
PARAMS ((const struct macosx_inferior_status *s, struct dyld_objfile_info *new));

void dyld_load_libraries
PARAMS ((const struct dyld_path_info *d, struct dyld_objfile_info *result));

void dyld_merge_libraries
PARAMS ((struct dyld_objfile_info *old,
	 struct dyld_objfile_info *new,
	 struct dyld_objfile_info *result));

void dyld_prune_shlib
PARAMS ((const struct macosx_dyld_thread_status *s,
	 struct dyld_path_info *d,
	 struct dyld_objfile_info *old, 
	 struct dyld_objfile_entry *n));

void dyld_merge_shlibs
PARAMS ((const struct macosx_dyld_thread_status *s,
	 struct dyld_path_info *d,
	 struct dyld_objfile_info *old, 
	 struct dyld_objfile_info *new)); 

void remove_objfile_from_dyld_records (struct objfile *);

void dyld_remove_objfile (struct dyld_objfile_entry *);

void dyld_remove_objfiles
PARAMS ((const struct dyld_path_info *d, struct dyld_objfile_info *result));

void dyld_remove_duplicates
PARAMS ((struct dyld_path_info *d, struct dyld_objfile_info *result));

void dyld_check_discarded
PARAMS ((struct dyld_objfile_info *info));

void dyld_purge_cached_libraries
PARAMS ((struct dyld_objfile_info *info));

void dyld_update_shlibs
PARAMS ((const struct macosx_dyld_thread_status *s,
	 struct dyld_path_info *d,
	 struct dyld_objfile_info *result));

void dyld_merge_shlib
PARAMS ((const struct macosx_dyld_thread_status *s,
	 struct dyld_path_info *d,
	 struct dyld_objfile_info *old, 
	 struct dyld_objfile_entry *n));

int dyld_objfile_allocated PARAMS ((struct objfile *o));

unsigned int dyld_parse_load_level PARAMS ((const char *s));

int dyld_minimal_load_flag
PARAMS ((const struct dyld_path_info *d,
	 struct dyld_objfile_entry *e));

char *dyld_find_dylib_name (CORE_ADDR addr, unsigned int ncmds);

#endif /* __GDB_MACOSX_NAT_DYLD_PROCESS_H__ */
