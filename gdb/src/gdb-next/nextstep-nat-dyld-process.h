#ifndef _NEXTSTEP_NAT_DYLD_PROCESS_H_
#define _NEXTSTEP_NAT_DYLD_PROCESS_H_

#include "defs.h"

struct next_inferior_status;
struct next_dyld_thread_status;

struct dyld_objfile_info;
struct dyld_path_info;

struct dyld_event;

struct target_ops;

void dyld_add_inserted_libraries
PARAMS ((struct dyld_objfile_info *info, const struct dyld_path_info *d));

void dyld_add_image_libraries
PARAMS ((struct dyld_objfile_info *info, bfd *abfd));

void dyld_resolve_shlibs_internal
PARAMS ((const struct next_inferior_status *s, struct dyld_objfile_info *new));

void dyld_resolve_shlibs_dyld
PARAMS ((const struct next_inferior_status *s, struct dyld_objfile_info *new));

void dyld_load_libraries
PARAMS ((const struct dyld_path_info *d, struct dyld_objfile_info *result));

void dyld_merge_libraries
PARAMS ((struct dyld_objfile_info *old,
	 struct dyld_objfile_info *new,
	 struct dyld_objfile_info *result));

void dyld_remove_objfiles
PARAMS ((struct dyld_objfile_info *result));

void dyld_remove_duplicates
PARAMS ((struct dyld_path_info *d, struct dyld_objfile_info *result));

void dyld_check_discarded
PARAMS ((struct dyld_objfile_info *info));

void dyld_purge_cached_libraries
PARAMS ((struct dyld_objfile_info *info));

void dyld_update_shlibs
PARAMS ((const struct next_dyld_thread_status *s,
	 struct dyld_path_info *d,
	 struct dyld_objfile_info *old,
	 struct dyld_objfile_info *new,
	 struct dyld_objfile_info *result));

void dyld_process_event
PARAMS ((struct dyld_objfile_info *info,
	 const struct dyld_event *event));

#endif /* _NEXTSTEP_NAT_DYLD_PROCESS_H_ */
