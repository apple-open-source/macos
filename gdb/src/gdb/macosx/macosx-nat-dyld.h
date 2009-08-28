#ifndef __GDB_MACOSX_NAT_DYLD_H__
#define __GDB_MACOSX_NAT_DYLD_H__

#include "defs.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-threads.h"

#include <mach/machine.h>       /* cpu_type_t, cpu_subtype_t */
#include <mach-o/loader.h>      /* struct mach_header, struct load_command */

struct objfile;
struct target_waitstatus;
struct target_ops;

struct macosx_inferior_status;

struct dyld_objfile_entry;

#include "macosx-nat-dyld-info.h"
#include "macosx-nat-dyld-path.h"

/* A representation of the address space of the inferior.
   We use this to slide libraries so nothing overlaps before execution
   starts.  Once we've started execution we can rely on dyld to keep everything
   separate.  */

struct pre_run_memory_map {
  int number_of_buckets;
  CORE_ADDR bucket_size;
  int *buckets;
};

/* Imported definitions from <mach/machine.h> which may not be available on
   older systems.  */
typedef int                     gdb_integer_t;
typedef gdb_integer_t       gdb_cpu_type_t;
#define GDB_CPU_ARCH_ABI64  0x01000000
#define GDB_CPU_TYPE_X86            ((gdb_cpu_type_t) 7)
#define GDB_CPU_TYPE_X86_64         (GDB_CPU_TYPE_X86 | GDB_CPU_ARCH_ABI64)

enum macosx_dyld_thread_state
{
  dyld_clear,
  dyld_initialized,
  dyld_started
};
typedef enum macosx_dyld_thread_state macosx_dyld_thread_state;

struct dyld_cache_range
{
  CORE_ADDR start;
  CORE_ADDR length;
};

struct macosx_dyld_thread_status
{

  CORE_ADDR dyld_image_infos;
  unsigned long dyld_version;
  CORE_ADDR dyld_notify;

  struct breakpoint *dyld_breakpoint;

  /* There are two ways to tell if libSystem's version of
     malloc is initialized.  If dyld_version is 1, then 
     you have to break on malloc_inited_breakpoint, and 
     if you hit it, then everything is kosher.  */
  struct breakpoint *malloc_inited_breakpoint;
  /* if dyld_version is 2 or greater, there's a bool
     in the dyld all_image_infos structure that tells us
     this directly.  */

  int libsystem_initialized;

  CORE_ADDR dyld_addr;
  CORE_ADDR dyld_slide;
  const char *dyld_name;

  /* When we're attaching to a process and dyld has slid (e.g. when attaching
     to something running under Rosetta translation, there is a native dyld
     which we don't see and a ppc dyld that we do see, the ppc dyld has been
     slid to a new address range), at the very early startup the minsyms have
     their unslid addresses and we need to apply dyld_slide to them.  But once
     we slide the dyld objfile to its actual load address, we need to stop that
     by-hand address translation in lookup_dyld_address() */
  int dyld_minsyms_have_been_relocated;

  enum macosx_dyld_thread_state state;

  struct dyld_objfile_info current_info;
  struct dyld_path_info path_info;
  
  /* This supports the Leopard "shared cache".  If a dylib is 
     in any of the "shared cache ranges" then it will have been
     prebound into a cache.  */
  CORE_ADDR dyld_shared_cache_ranges;
  /* The number of cache ranges.  -1 means the cache
     data hasn't been read yet.  */
  int dyld_num_shared_cache_ranges;
  struct dyld_cache_range *dyld_shared_cache_array;

  struct pre_run_memory_map *pre_run_memory_map;
};
typedef struct macosx_dyld_thread_status macosx_dyld_thread_status;

void dyld_debug (const char *fmt, ...);

void dyld_print_status_info (macosx_dyld_thread_status *s, unsigned int mask,
                             char *args);
int dyld_objfile_set_load_state (struct objfile *o, int load_state);
void macosx_clear_start_breakpoint ();
void macosx_set_start_breakpoint (macosx_dyld_thread_status *s,
                                  bfd *exec_bfd);
void macosx_init_addresses (macosx_dyld_thread_status *s);
int macosx_dyld_init (macosx_dyld_thread_status *s, bfd *exec_bfd);
int macosx_solib_add (const char *filename, int from_tty,
                      struct target_ops *targ, int loadsyms);
void macosx_dyld_thread_init (macosx_dyld_thread_status *s);
void macosx_add_shared_symbol_files ();
void macosx_init_dyld_from_core ();
void macosx_dyld_create_inferior_hook ();

void macosx_init_dyld (struct macosx_dyld_thread_status *s,
                       struct objfile *o, bfd *abfd);
void macosx_init_dyld_symfile (struct objfile *o, bfd *abfd);
void macosx_dyld_mourn_inferior (void);

int target_is_remote ();
int macosx_dyld_update (int dyldonly);
int dyld_lookup_and_bind_function (char *name);
void update_section_tables_dyld (struct dyld_objfile_info *s);
void update_section_tables ();
char *dyld_fix_path (char *path);

void macosx_set_malloc_inited (int new_val);
int macosx_get_malloc_inited (void);
struct section_offsets *get_sectoffs_for_shared_cache_dylib (struct dyld_objfile_entry *, CORE_ADDR);
int target_read_mach_header (CORE_ADDR addr,
                            struct mach_header *mh);
int target_get_mach_header_size (struct mach_header *mh);
int target_read_load_command (CORE_ADDR addr,
                                    struct load_command *load_cmd);
int target_read_uuid (CORE_ADDR addr, unsigned char *uuid);

/* From macosx-nat-dyld.c.  */
extern struct cmd_list_element *setshliblist;
extern struct cmd_list_element *showshliblist;



#endif /* __GDB_MACOSX_NAT_DYLD_H__ */
