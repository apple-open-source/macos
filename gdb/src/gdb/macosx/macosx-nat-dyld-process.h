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
	 struct dyld_objfile_info *old,
	 struct dyld_objfile_info *new,
	 struct dyld_objfile_info *result));

unsigned int dyld_parse_load_level (const char *s);

int dyld_minimal_load_flag (const struct dyld_path_info *d, 
			    struct dyld_objfile_entry *e);

#endif /* __GDB_MACOSX_NAT_DYLD_PROCESS_H__ */
