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

  unsigned int dyld_version;

  unsigned int nobject_images;
  unsigned int nlibrary_images;
  unsigned int object_image_size;
  unsigned int library_image_size;

  enum macosx_dyld_thread_state state;

  struct dyld_objfile_info current_info;
  struct dyld_path_info path_info;
  int uses_dyld;
};
typedef struct macosx_dyld_thread_status macosx_dyld_thread_status;

void dyld_debug (const char *fmt, ...);

const char *dyld_debug_error_string (enum dyld_debug_return ret);
void dyld_print_status_info (struct macosx_dyld_thread_status *s, unsigned int mask);

void macosx_init_dyld (struct macosx_dyld_thread_status *s, struct objfile *o);

void macosx_clear_start_breakpoint ();
void macosx_set_start_breakpoint (bfd *exec_bfd);

int macosx_try_start_dyld ();

void macosx_add_shared_symbol_files ();

void macosx_init_dyld_symfile (struct objfile *o);

int macosx_dyld_update (int dyldonly);

int dyld_lookup_and_bind_function (char *name);

#endif /* __GDB_MACOSX_NAT_DYLD_H__ */
