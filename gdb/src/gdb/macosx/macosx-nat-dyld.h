#ifndef __GDB_MACOSX_NAT_DYLD_H__
#define __GDB_MACOSX_NAT_DYLD_H__

#include "defs.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-threads.h"

#include <mach-o/dyld_debug.h>

struct objfile;
struct target_waitstatus;

struct macosx_inferior_status;

struct dyld_objfile_entry;

#include "macosx-nat-dyld-info.h"
#include "macosx-nat-dyld-path.h"

enum macosx_dyld_thread_state
{ 
  dyld_clear,
  dyld_initialized,
  dyld_started
};
typedef enum macosx_dyld_thread_state macosx_dyld_thread_state;

struct macosx_dyld_thread_status {

  CORE_ADDR object_images;
  CORE_ADDR library_images;
  CORE_ADDR state_changed_hook;

  unsigned long nobject_images;
  unsigned long nlibrary_images;
  unsigned long object_image_size;
  unsigned long library_image_size;

  unsigned long dyld_version;
  CORE_ADDR dyld_addr;
  CORE_ADDR dyld_slide;
  const char *dyld_name;

  enum macosx_dyld_thread_state state;

  struct dyld_objfile_info current_info;
  struct dyld_path_info path_info;

  struct breakpoint *dyld_breakpoint;
  struct breakpoint *dyld_event_breakpoint;
  unsigned int dyld_event_counter;
};
typedef struct macosx_dyld_thread_status macosx_dyld_thread_status;

void dyld_debug (const char *fmt, ...);

const char *dyld_debug_error_string (enum dyld_debug_return ret);
const char *dyld_debug_event_string (enum dyld_event_type type);
void dyld_print_status_info (macosx_dyld_thread_status *s, unsigned int mask, const char *args);
void macosx_clear_start_breakpoint ();
void macosx_set_start_breakpoint (macosx_dyld_thread_status *s, bfd *exec_bfd);
void macosx_init_addresses (macosx_dyld_thread_status *s);
void macosx_dyld_init (macosx_dyld_thread_status *s, bfd *exec_bfd);
int macosx_solib_add (const char *filename, int from_tty,
		      struct target_ops *targ, int loadsyms);
void macosx_dyld_thread_init (macosx_dyld_thread_status *s);
void macosx_add_shared_symbol_files ();
void macosx_init_dyld (struct macosx_dyld_thread_status *s,
		       struct objfile *o, bfd *abfd);
void macosx_init_dyld_symfile (struct objfile *o, bfd *abfd);
int macosx_dyld_update (int dyldonly);
int dyld_lookup_and_bind_function (char *name);
void update_section_tables (struct target_ops *target, struct dyld_objfile_info *info);

#endif /* __GDB_MACOSX_NAT_DYLD_H__ */
