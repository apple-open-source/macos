/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <mach/mach_types.h>
#include <mach/vm_types.h>
#include <mach/vm_region.h>
#include <mach/machine/vm_param.h>

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcmd.h"
#include "annotate.h"
#include "mach-o.h"
#include "gdbcore.h"                /* for core_ops */
#include "symfile.h"
#include "objfiles.h"
#include "gdb_assert.h"
#include "gdb_stat.h"
#include "regcache.h"

#if USE_MMALLOC
#include <mmalloc.h>
#endif

#include "macosx-nat-dyld.h"
#include "macosx-nat-dyld-path.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-inferior-debug.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-dyld.h"
#include "macosx-nat-dyld-info.h"
#include "macosx-nat-dyld-path.h"
#include "macosx-nat-dyld-process.h"
#include "cached-symfile.h"

#if WITH_CFM
#include "macosx-nat-cfm.h"
#endif /* WITH_CFM */

#define MAPPED_SYMFILES (USE_MMALLOC && HAVE_MMAP)

#define INVALID_ADDRESS ((CORE_ADDR) (-1))

extern macosx_inferior_status *macosx_status;

int dyld_preload_libraries_flag = 1;
int dyld_filter_events_flag = 1;
int dyld_always_read_from_memory_flag = 0;
char *dyld_symbols_prefix = "__dyld_";
int dyld_load_dyld_symbols_flag = 1;
int dyld_load_dyld_shlib_symbols_flag = 1;
int dyld_load_cfm_shlib_symbols_flag = 1;
int dyld_print_basenames_flag = 0;
char *dyld_load_rules = NULL;
char *dyld_minimal_load_rules = NULL;

FILE *dyld_stderr = NULL;
int dyld_debug_flag = 0;

extern int inferior_auto_start_cfm_flag;

int dyld_stop_on_shlibs_added = 1;
int dyld_stop_on_shlibs_updated = 1;
int dyld_combine_shlibs_added = 1;

struct dyld_raw_info
{
  CORE_ADDR name;
  CORE_ADDR addr;
  CORE_ADDR slide;
};

void clear_gdbarch_swap (struct gdbarch *);
void swapout_gdbarch_swap (struct gdbarch *);
void swapin_gdbarch_swap (struct gdbarch *);

static
void dyld_info_read_raw
(struct macosx_dyld_thread_status *status,
 struct dyld_raw_info **rinfo, unsigned int *rninfo);

static int dyld_starts_here_p (vm_address_t addr);

static
int dyld_info_process_raw
(struct dyld_objfile_entry *entry, CORE_ADDR name, CORE_ADDR slide, CORE_ADDR header);

void dyld_debug (const char *fmt, ...)
{
  va_list ap;
  if (dyld_debug_flag >= 1) {
    va_start (ap, fmt);
    fprintf (dyld_stderr, "[%d dyld]: ", getpid ());
    vfprintf (dyld_stderr, fmt, ap);
    va_end (ap);
    fflush (dyld_stderr);
  }
}

const char *
dyld_debug_error_string (enum dyld_debug_return ret)
{
  switch (ret) {
  case DYLD_SUCCESS: return "DYLD_SUCCESS";
  case DYLD_INCONSISTENT_DATA: return "DYLD_INCONSISTENT_DATA";
  case DYLD_INVALID_ARGUMENTS: return "DYLD_INVALID_ARGUMENTS";
  case DYLD_FAILURE: return "DYLD_FAILURE";
  default: return "[UNKNOWN]";
  }  
}

const char *
dyld_debug_event_string (enum dyld_event_type type)
{
  switch (type) {
  case DYLD_IMAGE_ADDED: return "DYLD_IMAGE_ADDED";
  case DYLD_MODULE_BOUND: return "DYLD_MODULE_BOUND";
  case DYLD_MODULE_REMOVED: return "DYLD_MODULE_REMOVED";
  case DYLD_MODULE_REPLACED: return "DYLD_MODULE_REPLACED";
  case DYLD_PAST_EVENTS_END: return "DYLD_PAST_EVENTS_END";
  case DYLD_IMAGE_REMOVED: return "DYLD_IMAGE_REMOVED";
  default: return "[UNKNOWN]";
  }
}

void
dyld_print_status_info (struct macosx_dyld_thread_status *s, unsigned int mask, const char *args)
{
  switch (s->state) {
  case dyld_clear:
    ui_out_text (uiout,
		 "The DYLD shared library state has not yet been initialized.\n");
    break;
  case dyld_initialized:
    ui_out_text (uiout,
		 "The DYLD shared library state has been initialized from the "
		 "executable's shared library information.  All symbols should be "
		 "present, but the addresses of some symbols may move when the program "
		 "is executed, as DYLD may relocate library load addresses if "
		 "necessary.\n");
    break;
  case dyld_started:
    ui_out_text (uiout,
		 "DYLD shared library information has been read from "
		 "the DYLD debugging thread.\n");
    break;
  default:
    internal_error (__FILE__, __LINE__, "invalid value for s->dyld_state");
    break;
  }

  dyld_print_shlib_info (&s->current_info, mask, 1, args);
}

void
macosx_clear_start_breakpoint ()
{
  remove_solib_event_breakpoints ();
}

static
CORE_ADDR lookup_dyld_address
(macosx_dyld_thread_status *status, const char *s)
{
  struct minimal_symbol *msym = NULL;
  CORE_ADDR sym_addr;
  char *ns = NULL;

  xasprintf (&ns, "%s%s", dyld_symbols_prefix, s);
  msym = lookup_minimal_symbol (ns, NULL, NULL);
  xfree (ns);

  if (msym == NULL)
    return (CORE_ADDR) -1;
  
  sym_addr = SYMBOL_VALUE_ADDRESS (msym);
  return (sym_addr + status->dyld_slide);
}

static
unsigned int lookup_dyld_value
(macosx_dyld_thread_status *status, const char *s)
{
  CORE_ADDR addr = lookup_dyld_address (status, s);
  if (addr == (CORE_ADDR) -1)
    return -1;
  return read_memory_unsigned_integer (addr, 4);
}

void
macosx_init_addresses (macosx_dyld_thread_status *s)
{
  s->object_images = lookup_dyld_address (s, "object_images");
  s->library_images = lookup_dyld_address (s, "library_images");
  s->state_changed_hook = lookup_dyld_address (s, "gdb_dyld_state_changed");

  s->dyld_version = lookup_dyld_value (s, "gdb_dyld_version");

  s->nobject_images = lookup_dyld_value (s, "gdb_nobject_images");
  s->nlibrary_images = lookup_dyld_value (s, "gdb_nlibrary_images");
  s->object_image_size = lookup_dyld_value (s, "gdb_object_image_size");
  s->library_image_size = lookup_dyld_value (s, "gdb_library_image_size");
}

static int
dyld_starts_here_p (vm_address_t addr)
{
  vm_address_t address =  addr;
  vm_size_t size = 0;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t info_cnt = sizeof (vm_region_basic_info_data_64_t);
  kern_return_t ret;
  mach_port_t object_name;
  vm_address_t data;
  vm_size_t data_count;

  struct mach_header *mh;

  info_cnt = VM_REGION_BASIC_INFO_COUNT;
  ret = vm_region (macosx_status->task, &address, &size, VM_REGION_BASIC_INFO,
		  (vm_region_info_t) &info, &info_cnt, &object_name);

  if (ret != KERN_SUCCESS)
    return 0;

  /* If it is not readable, it is not dyld. */
  
  if ((info.protection & VM_PROT_READ) == 0)
    return 0; 

  ret = vm_read (macosx_status->task, address, size, &data, &data_count);

  if (ret != KERN_SUCCESS)
    {
      /* Don't vm_deallocate the memory here, you didn't successfully get
	 it, and deallocating it will cause a crash. */
      return 0; 
    }

  /* If the vm region is too small to contain a mach_header, it also can't be
     where dyld is loaded */

  if (data_count < sizeof (struct mach_header))
    {
      ret = vm_deallocate (mach_task_self (), data, data_count);
      return 0;
    }

  mh = (struct mach_header *) data;

  /* If the magic number is right and the size of this region is big
     enough to cover the mach header and load commands, assume it is
     correct. */

  if (mh->magic != MH_MAGIC ||
      mh->filetype != MH_DYLINKER ||
      data_count < sizeof (struct mach_header) +
      mh->sizeofcmds) {
    ret = vm_deallocate (mach_task_self (), data, data_count);
    return 0;
  }

  /* Looks like dyld! */
  ret = vm_deallocate (mach_task_self(), data, data_count);

  return 1;
}

/* Tries to find the name string for the dynamic linker passed as
   `abfd'.  Returns 1 if it found a name (and returns name in
   *name). Return 0 if no name was found; -1 if there was an error. */

static int
macosx_lookup_dyld_name (bfd *abfd, const char **rname)
{
  struct mach_o_data_struct *mdata = NULL;
  char *name = NULL;
  unsigned int i;

  if (abfd == NULL)
    return -1;
  if (! bfd_mach_o_valid (abfd))
    return -1;

  mdata = abfd->tdata.mach_o_data;

  CHECK_FATAL (mdata != NULL);
  CHECK_FATAL (rname != NULL);
      
  for (i = 0; i < mdata->header.ncmds; i++) 
    {
      struct bfd_mach_o_load_command *cmd = &mdata->commands[i];
	  
      if (cmd->type == BFD_MACH_O_LC_LOAD_DYLINKER)
	{
	  bfd_mach_o_dylinker_command *dcmd = &cmd->command.dylinker;
	      
	  name = xmalloc (dcmd->name_len + 1);
	      
	  bfd_seek (abfd, dcmd->name_offset, SEEK_SET);
	  if (bfd_bread (name, dcmd->name_len, abfd) != dcmd->name_len) 
	    {
	      warning ("Unable to find library name for LC_LOAD_DYLINKER "
		       "or LD_ID_DYLINKER command; ignoring");
	      xfree (name);
	      continue;
	    }
	  else
	    {
	      break;
	    }
	}
    }

  *rname = name;
  return (name == NULL) ? 0 : 1;
}

/* Searches the target address space for dyld itself, returning it in
   'value'.  Returns 1 if it found dyld, 0 if it didn't, -1 if there
   was an error.  If 'hint' is not INVALID_ADDRESS, check there first
   as a hint for where to find dyld. */

static int
macosx_locate_dyld (CORE_ADDR *value, CORE_ADDR hint)
{
  if ((hint != INVALID_ADDRESS) && dyld_starts_here_p (hint))
    {
     *value = hint;
     return 1;
    }
  else
    {
      kern_return_t kret = KERN_SUCCESS;
      vm_region_basic_info_data_t info;
      int info_cnt = sizeof (vm_region_basic_info_data_t);
      vm_address_t test_addr = VM_MIN_ADDRESS;
      vm_size_t size = 0;
      mach_port_t object_name = PORT_NULL;
      task_t target_task = macosx_status->task;
      
      do 
	{
	  kret = vm_region (target_task, &test_addr, 
			    &size, VM_REGION_BASIC_INFO,
			    (vm_region_info_t) &info, &info_cnt, 
			    &object_name);
	  
	  if (kret != KERN_SUCCESS)
	    return -1;
	  
	  if (dyld_starts_here_p (test_addr))
	    {
	      *value = test_addr;
	      return 1;
	    }
	  
	  test_addr += size;
	  
	} while (size != 0);
    }

  return 0;
}
  
/* Determine the address where the current target expected DYLD to be
   loaded.  This is used when first guessing the address of dyld for
   symbol loading on dynamic executables, and also to compute the
   proper slide to be applied to values found in the dyld memory
   itself.  Returns 1 if it found the address, 0 if it did not. */

int macosx_locate_dyld_static (macosx_dyld_thread_status *s, CORE_ADDR *value)
{
  struct objfile *objfile = NULL;
  const char *dyld_name = s->dyld_name;

  if (dyld_name == NULL)
    dyld_name = "/usr/lib/dyld";
  
  ALL_OBJFILES (objfile)
    {
      if (strcmp (dyld_name, bfd_get_filename (objfile->obfd)) == 0) 
	{
	  asection *text_section
	    = bfd_get_section_by_name (objfile->obfd, "LC_SEGMENT.__TEXT");
	  *value = bfd_section_vma (objfile->obfd, text_section);
	  return 1;
	}
    }
  return 0;
}

/* Locates the dylinker in the executable, and updates the dyld part
   of our data structures.

   This function should be able to be called at any time, and should
   always do as much of the right thing as possible based on the
   information available at the time. */

void
macosx_dyld_init (macosx_dyld_thread_status *s, bfd *exec_bfd)
{
  CORE_ADDR static_dyld_address = INVALID_ADDRESS;
  CORE_ADDR dyld_address = INVALID_ADDRESS;
  CORE_ADDR prev_dyld_address = INVALID_ADDRESS;
  int ret = 0;

  /* Once dyld_slide is set, we have already done everything that
     needs to be done.  At least for the moment, it's not possible for dyld to
     be moved once the program is executing.  */

  if (s->dyld_slide != INVALID_ADDRESS)
    return;

  /* Now find dyld's name.  Get it from the exec_bfd if we have
     it, if we don't find it here, then we will look again in
     memory once we have found dyld. */

  if (s->dyld_name == NULL)
    macosx_lookup_dyld_name (exec_bfd, &s->dyld_name);

  /* Find the location of the dyld to be used, if it hasn't been found already. */
  /* Store the old value, so we'll know if it was changed. */

  prev_dyld_address = s->dyld_addr;

  /* Take our best guess as to where to look for dyld, just as an
     optimization. */

  macosx_locate_dyld_static (s, &static_dyld_address);
  if (s->dyld_addr != INVALID_ADDRESS)
    ret = macosx_locate_dyld (&dyld_address, s->dyld_addr);
  else if (static_dyld_address != INVALID_ADDRESS)
    ret = macosx_locate_dyld (&dyld_address, s->dyld_addr);
  else
    ret = macosx_locate_dyld (&dyld_address, s->dyld_addr);

  /* If we didn't find dyld, there's no point continuing. */

  if (ret != 1)
    return;
  CHECK_FATAL (dyld_address != INVALID_ADDRESS);

  /* Now grub for dyld's name in memory if we haven't found it already. */
  if ((s->dyld_name == NULL) && (dyld_address != INVALID_ADDRESS))
    {
      struct mach_header header;
      target_read_memory (dyld_address, (char *) &header, sizeof (struct mach_header));
      s->dyld_name = dyld_find_dylib_name (dyld_address, header.ncmds);
    }

  /* Once we know the address at which dyld was loaded, we can try to
     read in the symbols for it, and then hopefully find the static
     address and use it to compute the slide. */

  if (dyld_address != prev_dyld_address)
    {
      s->dyld_addr = dyld_address;
      macosx_dyld_update (1);
      macosx_locate_dyld_static (s, &static_dyld_address);
    }

  
  /* If we were able to find the slide, finish the dyld
     initialization, by setting up the data structures and inserting
     the dyld_gdb_state_changed breakpoint. */

  if (static_dyld_address != INVALID_ADDRESS)
    {
      s->dyld_addr = dyld_address;
      s->dyld_slide = dyld_address - static_dyld_address;

      macosx_init_addresses (s);
      macosx_set_start_breakpoint (s, exec_bfd);

      breakpoints_changed ();
    }
}

void
macosx_set_start_breakpoint (macosx_dyld_thread_status *s, bfd *exec_bfd)
{
  struct macosx_dyld_thread_status *status = &macosx_status->dyld_status;

  if (status->dyld_breakpoint == NULL) {
    
    struct symtab_and_line sal;
    char *ns = NULL;

    xasprintf (&ns, "%s%s", dyld_symbols_prefix, "gdb_dyld_state_changed");
    
    init_sal (&sal);
    sal.pc = status->state_changed_hook;
    status->dyld_breakpoint = set_momentary_breakpoint
      (sal, null_frame_id, bp_shlib_event);
    status->dyld_breakpoint->disposition = disp_donttouch;
    status->dyld_breakpoint->thread = -1;
    status->dyld_breakpoint->addr_string = ns;

    breakpoints_changed ();
  }
}

#if defined (__powerpc__) || defined (__ppc__)
static ULONGEST FETCH_ARGUMENT (int i)
{
  return read_register (3 + i);
}
#elif defined (__i386__)
static ULONGEST FETCH_ARGUMENT (int i)
{
  CORE_ADDR stack = read_register (SP_REGNUM);
  return read_memory_unsigned_integer (stack + (4 * (i + 1)), 4);
}
#elif defined (__sparc__)
static ULONGEST FETCH_ARGUMENT (int i)
{
  return read_register (O0_REGNUM + i);
}
#elif defined (__hppa__) || defined (__hppa)
static ULONGEST FETCH_ARGUMENT (int i)
{
  return read_register (R0_REGNUM + 26 - i);
}
#else
#error unknown architecture
#endif

static CORE_ADDR extend_slide (CORE_ADDR addr, CORE_ADDR slide)
{
  CORE_ADDR orig = (addr - slide) & 0xffffffff;
  return (addr - orig);
}

int
macosx_solib_add (const char *filename, int from_tty,
		  struct target_ops *targ, int loadsyms)
{
  struct macosx_dyld_thread_status *dyld_status = NULL;
  struct macosx_cfm_thread_status *cfm_status = NULL;
  int libraries_changed = 0;
  int notify = 0;

  CHECK_FATAL (macosx_status != NULL);
  dyld_status = &macosx_status->dyld_status;
  cfm_status = &macosx_status->cfm_status;

  macosx_dyld_init (dyld_status, exec_bfd);

  if ((dyld_status->dyld_event_breakpoint != NULL) 
      && (dyld_status->dyld_event_breakpoint->address == read_pc ()))
    {
      CORE_ADDR event_addr = FETCH_ARGUMENT (0);
      int type = read_memory_unsigned_integer (event_addr, 4);
      CORE_ADDR addr_1 = read_memory_unsigned_integer (event_addr + 4, 4);
      CORE_ADDR slide_1 = read_memory_unsigned_integer (event_addr + 8, 4);
      slide_1 = extend_slide (addr_1, slide_1);
      CORE_ADDR name_1 = INVALID_ADDRESS;

      switch (type) {
      case DYLD_IMAGE_ADDED:
	{
	  struct dyld_objfile_entry tentry;
	  struct dyld_raw_info *rinfo = NULL;
	  unsigned int nrinfo = 0;
	  unsigned int i = 0;

	  dyld_info_read_raw (dyld_status, &rinfo, &nrinfo);
	  for (i = 0; i < nrinfo; i++) {
	    if (addr_1 == rinfo[i].addr) {
	      CHECK_FATAL (slide_1 == rinfo[i].slide);
	      name_1 = rinfo[i].name;
	    }
	  }
	  xfree (rinfo);

	  dyld_objfile_entry_clear (&tentry);
	  tentry.allocated = 1;
	  tentry.reason = dyld_reason_dyld;
	  tentry.dyld_valid = 1;
	  
	  if (dyld_info_process_raw (&tentry, name_1, slide_1, addr_1) == 1)
	    {
	      struct dyld_objfile_entry *pentry = NULL;
	      struct objfile *objfile = NULL;
	      unsigned int shlibnum = 0;

	      dyld_merge_shlib (dyld_status, &dyld_status->path_info,
				&dyld_status->current_info, &tentry);
	      dyld_prune_shlib (dyld_status, &dyld_status->path_info,
				&dyld_status->current_info, &tentry);

	      pentry = dyld_objfile_entry_alloc (&dyld_status->current_info);
	      *pentry = tentry;
	      CHECK_FATAL (dyld_entry_shlib_num (&dyld_status->current_info, pentry, &shlibnum) == 0);

	      dyld_update_shlibs (dyld_status, &dyld_status->path_info,
				  &dyld_status->current_info);
	      CHECK_FATAL (dyld_resolve_shlib_num (&dyld_status->current_info, shlibnum, &pentry, &objfile) == 0);

	      if (dyld_combine_shlibs_added)
		dyld_status->dyld_event_counter++;
	      else
		notify = 1;
	      
	      if (ui_out_is_mi_like_p (uiout))
		{
		  ui_out_notify_begin (uiout, "shlibs-added");
		  dyld_print_entry_info (&tentry, shlibnum, 0);
		  ui_out_notify_end (uiout);
		}
	    }
	}
	
      case DYLD_MODULE_BOUND:
      case DYLD_MODULE_REMOVED:
      case DYLD_MODULE_REPLACED:
      case DYLD_PAST_EVENTS_END:
	break;
	
      case DYLD_IMAGE_REMOVED:
	libraries_changed = macosx_dyld_update (0);
	notify = libraries_changed && dyld_stop_on_shlibs_updated;
	break;
	
      default:
	warning ("unknown dyld event type %s (%x); updating dyld state",
		 dyld_debug_event_string (type), type);
	libraries_changed = macosx_dyld_update (0);
	notify = libraries_changed && dyld_stop_on_shlibs_updated;
	break;
      }
    }
  else if ((dyld_status->dyld_breakpoint != NULL) 
	   && (dyld_status->dyld_breakpoint->address == read_pc ()))
    {
#if 0
      /* For now, just leave the breakpoint enabled, until we get around
	 to adding code to re-enable the breakpoint if the user sets
	 the flag to combine shared library messages. */

      /* Disable this breakpoint, unless we need it set in order to
	 allow GDB to notify the user when a complete batch of shared
	 library events has been received.  We can't just delete the
	 breakpoint, as then stop_bpstat() would have no idea why we
	 stopped here. */

      if (! dyld_combine_shlibs_added)
	disable_breakpoint (dyld_status->dyld_breakpoint);
#endif

      /* Only do a full update if the event breakpoint isn't being used. */

      if (dyld_status->dyld_event_breakpoint == NULL)
	{
	  libraries_changed = macosx_dyld_update (0);
	  notify = libraries_changed && dyld_stop_on_shlibs_updated;
	}
      else
	{
	  if (dyld_status->dyld_event_counter > 0)
	    libraries_changed = 1;
	  notify = libraries_changed && dyld_stop_on_shlibs_added;
	  dyld_status->dyld_event_counter = 0;
	}	  


      if (dyld_status->dyld_event_breakpoint == NULL)
	{
	  struct symtab_and_line sal;
	  char *ns = NULL;

	  xasprintf (&ns, "%s%s", dyld_symbols_prefix, "send_event");
    
	  init_sal (&sal);
	  sal.pc = lookup_dyld_address (dyld_status, "send_event");
	  dyld_status->dyld_event_breakpoint = set_momentary_breakpoint
	    (sal, null_frame_id, bp_shlib_event);
	  dyld_status->dyld_event_breakpoint->disposition = disp_donttouch;
	  dyld_status->dyld_event_breakpoint->thread = -1;
	  dyld_status->dyld_event_breakpoint->addr_string = ns;

	  breakpoints_changed ();
	}
    }
  else if ((cfm_status->cfm_breakpoint != NULL) 
	   && (cfm_status->cfm_breakpoint->address == read_pc ()))
    {
      /* no cfm support for incremental update yet */
      libraries_changed = macosx_dyld_update (0);
      notify = libraries_changed && dyld_stop_on_shlibs_updated;
    }
  else
    {
      /* initial update */
      libraries_changed = macosx_dyld_update (0);
      notify = libraries_changed && dyld_stop_on_shlibs_updated;
    }

  return notify;
}

void macosx_dyld_thread_init (macosx_dyld_thread_status *s)
{
  s->dyld_name = NULL;
  s->dyld_addr = INVALID_ADDRESS;
  s->dyld_slide = INVALID_ADDRESS;

  s->dyld_version = 0;
  s->dyld_breakpoint = NULL;
  s->dyld_event_breakpoint = NULL;
  s->dyld_event_counter = 0;
}

void
macosx_add_shared_symbol_files ()
{
  CHECK_FATAL (macosx_status != NULL);
  macosx_dyld_update (0);
}

void
macosx_init_dyld (struct macosx_dyld_thread_status *s,
		  struct objfile *o, bfd *abfd)
{
  struct dyld_objfile_entry *e;
  unsigned int i;
  struct dyld_objfile_info previous_info;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (&s->current_info, e, i)
    if (e->reason & dyld_reason_executable_mask) {
      dyld_objfile_entry_clear (e);
    }

  dyld_init_paths (&s->path_info);

  dyld_objfile_info_init (&previous_info);

  dyld_objfile_info_copy (&previous_info, &s->current_info);
  dyld_objfile_info_free (&s->current_info);

  if (o != NULL) {
    struct dyld_objfile_entry *e;
    e = dyld_objfile_entry_alloc (&s->current_info);
    e->text_name_valid = 1;
    e->reason = dyld_reason_executable;
    if (o != NULL)
      {
	e->abfd = o->obfd;
        e->objfile = o;
        e->load_flag = o->symflags;
      }
    if (e->abfd != NULL)
      {
	e->text_name = xstrdup (bfd_get_filename (e->abfd));
      }
    e->loaded_from_memory = 0;
    e->loaded_name = e->text_name;
    e->loaded_addr = 0;
    e->loaded_addrisoffset = 1;
  }

  if (dyld_preload_libraries_flag) {
    dyld_add_inserted_libraries (&s->current_info, &s->path_info);
    if (abfd != NULL) {
      dyld_add_image_libraries (&s->current_info, abfd);
    }
  }

  dyld_merge_shlibs (s, &s->path_info, &previous_info, &s->current_info);
  dyld_update_shlibs (s, &s->path_info, &s->current_info);

  dyld_objfile_info_free (&previous_info);
 
  s->state = dyld_initialized;
}

void
macosx_init_dyld_symfile (struct objfile *o, bfd *abfd)
{
  CHECK_FATAL (macosx_status != NULL);
  macosx_init_dyld (&macosx_status->dyld_status, o, abfd);
}

static
void dyld_cache_purge_command (char *exp, int from_tty)
{
  CHECK_FATAL (macosx_status != NULL);
  dyld_purge_cached_libraries (&macosx_status->dyld_status.current_info);
}

static
int dyld_info_process_raw
(struct dyld_objfile_entry *entry, CORE_ADDR name, CORE_ADDR slide, CORE_ADDR header)
{
  struct mach_header headerbuf;
  char *namebuf = NULL;
  int errno;

  target_read_memory (header, (char *) &headerbuf, sizeof (struct mach_header));

  switch (headerbuf.filetype) {
  case 0:
    return 0;
  case MH_EXECUTE:
  case MH_DYLIB:
  case MH_DYLINKER:
  case MH_BUNDLE:
    break;
  case MH_FVMLIB:
  case MH_PRELOAD:
    return 0;
  default:
    warning ("Ignored unknown object module at 0x%lx (offset 0x%lx) with type 0x%lx\n",
             (unsigned long) header, (unsigned long) slide, (unsigned long) headerbuf.filetype);
    return 0;
  }

  target_read_string (name, &namebuf, 1024, &errno);

  if (errno != 0) {
    xfree (namebuf);
    namebuf = NULL;
  }

  if (namebuf != NULL) {

    char *s = strchr (namebuf, ':');
    if (s != NULL) {
      *s = '\0';
    }
  }

  if (namebuf == NULL) {

    namebuf = xmalloc (256);
    CORE_ADDR curpos = header + sizeof (struct mach_header);
    struct load_command cmd;
    struct dylib_command dcmd;
    int i;

    for (i = 0; i < headerbuf.ncmds; i++) {
      target_read_memory (curpos, (char *) &cmd, sizeof (struct load_command));
      if (cmd.cmd == LC_ID_DYLIB) {
	target_read_memory (curpos, (char *) &dcmd, sizeof (struct dylib_command));
	target_read_memory (curpos + dcmd.dylib.name.offset, namebuf, 256);
	break;
      }
      curpos += cmd.cmdsize;
    }
  }

  if (namebuf != NULL) {
    entry->dyld_name = xstrdup (namebuf);
    entry->dyld_name_valid = 1;
  }

  entry->dyld_addr = header;
  entry->dyld_slide = slide;
  entry->dyld_index = header;
  entry->dyld_valid = 1;

  switch (headerbuf.filetype) {
  case MH_EXECUTE: {
    entry->reason = dyld_reason_executable;
    if (symfile_objfile != NULL) {
      entry->objfile = symfile_objfile;
      entry->abfd = symfile_objfile->obfd;
      entry->loaded_from_memory = 0;
      entry->loaded_name = entry->dyld_name;
      entry->loaded_addr = 0;
      entry->loaded_addrisoffset = 1;
    }
    break;
  }
  case MH_DYLIB:
    entry->reason = dyld_reason_dyld;
    break;
  case MH_DYLINKER:
  case MH_BUNDLE:
    entry->reason = dyld_reason_dyld;
    break;
  default:
    internal_error (__FILE__, __LINE__, "Unknown object module at 0x%lx (offset 0x%lx) with type 0x%lx\n",
                    (unsigned long) header, (unsigned long) slide, (unsigned long) headerbuf.filetype);
  }

  return 1;
}

static
void dyld_info_read_raw
(struct macosx_dyld_thread_status *status,
 struct dyld_raw_info **rinfo, unsigned int *rninfo)
{
  struct dyld_raw_info *info = NULL;
  unsigned int ninfo = 0;
  unsigned int nsize = 0;

  CORE_ADDR library_images_addr;
  CORE_ADDR object_images_addr;
  unsigned char *buf = NULL;

  unsigned long library_size = status->nlibrary_images * status->library_image_size;
  unsigned long object_size = status->nobject_images * status->object_image_size;
  unsigned int i;

  *rinfo = NULL;
  *rninfo = 0;

  if (status->dyld_slide == INVALID_ADDRESS)
    return;

  if ((status->library_images == INVALID_ADDRESS)
      || (status->library_image_size == 0)
      || (status->nlibrary_images > 8192)
      || (status->library_image_size > 8192))
    {
      warning ("error parsing dyld data");
      return;
    }

  if ((status->object_images == INVALID_ADDRESS)
      || (status->object_image_size == 0)
      || (status->nobject_images > 8192)
      || (status->object_image_size > 8192))
    {
      warning ("error parsing dyld data");
      return;
    }

  if (library_size > object_size)
      buf = xmalloc (library_size + 8);
  else
      buf = xmalloc (object_size + 8);

  info = xmalloc (sizeof (struct dyld_raw_info) * 4);
  nsize = 4;

  library_images_addr = status->library_images;
  while (library_images_addr != NULL) {

    unsigned long nimages = 0;
    target_read_memory (library_images_addr, buf, library_size + 8);

    nimages = extract_unsigned_integer (buf + library_size, 4);
    library_images_addr = extract_unsigned_integer (buf + library_size + 4, 4);
   
    if (nimages > status->nlibrary_images) {
      warning ("image specifies an invalid number of libraries (%lu > %lu)",
	       nimages, status->nlibrary_images);
      break;
    }

    for (i = 0; i < nimages; i++) {
      char *ebuf = buf + (i * status->library_image_size);
      while (nsize <= ninfo) {
	nsize *= 2;
	info = xrealloc (info, sizeof (struct dyld_raw_info) * nsize);
      }
      info[ninfo].name = extract_unsigned_integer (ebuf, 4);
      info[ninfo].slide = extract_unsigned_integer (ebuf + 4, 4);
      info[ninfo].addr = extract_unsigned_integer (ebuf + 8, 4);
      info[ninfo].slide = extend_slide (info[ninfo].addr, info[ninfo].slide);
      ninfo++;
    }
  }

  object_images_addr = status->object_images;
  while (object_images_addr != NULL) {

    unsigned long nimages = 0;
    target_read_memory (object_images_addr, buf, object_size + 8);

    nimages = extract_unsigned_integer (buf + object_size, 4);
    object_images_addr = extract_unsigned_integer (buf + object_size + 4, 4);
   
    if (nimages > status->nobject_images) {
      warning ("image specifies an invalid number of libraries (%lu > %lu)",
	       nimages, status->nobject_images);
      break;
    }

    for (i = 0; i < nimages; i++) {
      char *ebuf = buf + (i * status->object_image_size);
      while (nsize <= ninfo) {
	nsize *= 2;
	info = xrealloc (info, sizeof (struct dyld_raw_info) * nsize);
      }
      info[ninfo].name = extract_unsigned_integer (ebuf, 4);
      info[ninfo].slide = extract_unsigned_integer (ebuf + 4, 4);
      info[ninfo].addr = extract_unsigned_integer (ebuf + 8, 4);
      info[ninfo].slide = extend_slide (info[ninfo].addr, info[ninfo].slide);
      ninfo++;
    }
  }

  *rinfo = info;
  *rninfo = ninfo;
}

static
void dyld_info_read
(struct macosx_dyld_thread_status *status, struct dyld_objfile_info *info, int dyldonly)
{
  struct dyld_raw_info *rinfo = NULL;
  int reserved = -1;
  unsigned int nrinfo = 0;
  unsigned int i = 0;
  
  if (! dyldonly)
    {
      struct dyld_objfile_entry *entry = dyld_objfile_entry_alloc (info);
      reserved = entry - info->entries;
    }

  if (status->dyld_addr != INVALID_ADDRESS)
    {
      const char *dyld_name = NULL;
      struct dyld_objfile_entry *entry = NULL;

      entry = dyld_objfile_entry_alloc (info);
      dyld_name = (status->dyld_name != NULL) ? status->dyld_name : "/usr/lib/dyld";

      entry->dyld_name = xstrdup (dyld_name);
      entry->dyld_name_valid = 1;
      entry->prefix = dyld_symbols_prefix;

      entry->dyld_addr = status->dyld_addr;
      if (status->dyld_slide != INVALID_ADDRESS)
	entry->dyld_slide = status->dyld_slide;
      else
	entry->dyld_slide = 0;

      entry->dyld_index = 0;
      entry->dyld_valid = 1;
      entry->reason = dyld_reason_dyld;
    }

  if (dyldonly)
    return;

  dyld_info_read_raw (status, &rinfo, &nrinfo);

  for (i = 0; i < nrinfo; i++) {
    struct dyld_objfile_entry tentry;
    struct dyld_objfile_entry *pentry = NULL;
    dyld_objfile_entry_clear (&tentry);
    tentry.allocated = 1;
    tentry.reason = dyld_reason_dyld;
    if (dyld_info_process_raw (&tentry, rinfo[i].name, rinfo[i].slide, rinfo[i].addr) == 1) {
      if ((tentry.reason & dyld_reason_executable_mask) && (reserved >= 0)) {
	pentry = &info->entries[reserved];
	reserved = -1;
      } else {
	pentry = dyld_objfile_entry_alloc (info);
      }
      *pentry = tentry;
    }
  }

  xfree (rinfo);

  if (reserved >= 0)
    dyld_objfile_entry_clear (&info->entries[reserved]);
}

int
macosx_dyld_update (int dyldonly)
{
  int ret;
  int libraries_changed;

  struct dyld_objfile_info previous_info, saved_info, *current_info;
  struct macosx_dyld_thread_status *status;

  CHECK_FATAL (macosx_status != NULL);
  status = &macosx_status->dyld_status;
  current_info = &status->current_info;

  dyld_objfile_info_init (&previous_info);
  dyld_objfile_info_init (&saved_info);

  dyld_objfile_info_copy (&previous_info, current_info);
  dyld_objfile_info_copy (&saved_info, &previous_info);

  dyld_objfile_info_free (current_info);
  dyld_objfile_info_init (current_info);

  dyld_info_read (status, current_info, dyldonly);
  if (inferior_auto_start_cfm_flag)
    ret = cfm_update (macosx_status->task, current_info);

  dyld_merge_shlibs (status, &status->path_info, &previous_info, current_info);
  dyld_update_shlibs (status, &status->path_info, current_info);

  if (dyld_filter_events_flag) {
    libraries_changed = dyld_objfile_info_compare (&saved_info, current_info);
  } else {
    libraries_changed = 1;
  }

  dyld_objfile_info_free (&saved_info);
  dyld_objfile_info_free (&previous_info);

  if (ui_out_is_mi_like_p (uiout) && libraries_changed)
    {
      ui_out_notify_begin (uiout, "shlibs-updated");
      ui_out_notify_end (uiout);
    }

  return libraries_changed;
}

static void
macosx_dyld_update_command (char *args, int from_tty)
{
  macosx_dyld_update (0);
  macosx_init_dyld (&macosx_status->dyld_status, symfile_objfile, exec_bfd);
}

static void
map_shlib_numbers
(char *args, void (*function) (struct dyld_path_info *, struct dyld_objfile_entry *, struct objfile *, const char *param), 
 struct dyld_path_info *d, struct dyld_objfile_info *info)
{
  char *p, *p1, *val;
  int num, match;

  if (args == 0)
    error_no_arg ("one or more shlib numbers");

  p = args;
  while (isspace (*p) && (*p != '\0'))
    p++;

  if (strncmp (p, "all ", 4) == 0)
    p += 4;
  else
    {
      for (;;) {
	while (isspace (*p) && (*p != '\0'))
	  p++;
	if (! isdigit (*p))
	  break;
	while ((!isspace (*p)) && (*p != '\0'))
	  p++;
      }
    }
  val = p;

  if ((*p != '\0') && (p > args)) {
    p[-1] = '\0';
  }

  p = args;

  if (strcmp (p, "all") == 0)
    {
      struct dyld_objfile_entry *e;
      unsigned int n;

      DYLD_ALL_OBJFILE_INFO_ENTRIES (info, e, n)
	(* function) (d, e, e->objfile, val);

      return;
    }

  while (*p)
    {
      struct dyld_objfile_entry *e;
      struct objfile *o;
      int ret;

      match = 0;
      p1 = p;
      num = get_number_or_range (&p);

      if (num == 0) {
	warning ("bad shlib number at or near '%s'", p1);
	return;
      }

      ret = dyld_resolve_shlib_num (info, num, &e, &o);

      if (ret < 0) {
	warning ("no shlib %d", num);
	return;
      }

      (* function) (d, e, o, val);
    }
}

static void
add_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e, struct objfile *o, const char *arg)
{
  if (e != NULL)
    e->load_flag = OBJF_SYM_ALL;
}

static void
dyld_add_symbol_file_command (char *args, int from_tty)
{
  struct dyld_objfile_info original_info, modified_info;

  dyld_objfile_info_init (&original_info);
  dyld_objfile_info_init (&modified_info);

  dyld_objfile_info_copy (&original_info, &macosx_status->dyld_status.current_info);
  dyld_objfile_info_copy (&modified_info, &macosx_status->dyld_status.current_info);

  map_shlib_numbers (args, add_helper, &macosx_status->dyld_status.path_info, &modified_info);

  dyld_merge_shlibs (&macosx_status->dyld_status, &macosx_status->dyld_status.path_info,
		     &original_info, &modified_info);
  dyld_update_shlibs (&macosx_status->dyld_status, &macosx_status->dyld_status.path_info,
		      &modified_info);
  
  dyld_objfile_info_copy (&macosx_status->dyld_status.current_info, &modified_info);

  dyld_objfile_info_free (&original_info);
  dyld_objfile_info_free (&modified_info);
}

static void
remove_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e, struct objfile *o, const char *arg)
{
  if (e != NULL)
    e->load_flag = OBJF_SYM_NONE | dyld_minimal_load_flag (d, e);
}

static void
specify_symfile_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e, struct objfile *o, const char *arg)
{
  e->user_name = xstrdup (arg);
}

static void dyld_specify_symbol_file_command (char *args, int from_tty)
{
  struct dyld_objfile_info original_info, modified_info;

  dyld_objfile_info_init (&original_info);
  dyld_objfile_info_init (&modified_info);

  dyld_objfile_info_copy (&original_info, &macosx_status->dyld_status.current_info);
  dyld_objfile_info_copy (&modified_info, &macosx_status->dyld_status.current_info);

  map_shlib_numbers (args, specify_symfile_helper, &macosx_status->dyld_status.path_info, &modified_info);

  dyld_merge_shlibs (&macosx_status->dyld_status, &macosx_status->dyld_status.path_info,
		     &original_info, &modified_info);
  dyld_update_shlibs (&macosx_status->dyld_status, &macosx_status->dyld_status.path_info,
                      &modified_info);
  
  dyld_objfile_info_copy (&macosx_status->dyld_status.current_info, &modified_info);

  dyld_objfile_info_free (&original_info);
  dyld_objfile_info_free (&modified_info);
}

static
void dyld_remove_symbol_file_command (char *args, int from_tty)
{
  struct dyld_objfile_info original_info, modified_info;

  dyld_objfile_info_init (&original_info);
  dyld_objfile_info_init (&modified_info);

  dyld_objfile_info_copy (&original_info, &macosx_status->dyld_status.current_info);
  dyld_objfile_info_copy (&modified_info, &macosx_status->dyld_status.current_info);

  map_shlib_numbers (args, remove_helper, &macosx_status->dyld_status.path_info, &modified_info);

  dyld_merge_shlibs (&macosx_status->dyld_status, &macosx_status->dyld_status.path_info,
		     &original_info, &modified_info);
  dyld_update_shlibs (&macosx_status->dyld_status, &macosx_status->dyld_status.path_info,
                      &modified_info);
  
  dyld_objfile_info_copy (&macosx_status->dyld_status.current_info, &modified_info);

  dyld_objfile_info_free (&original_info);
  dyld_objfile_info_free (&modified_info);
}
        
static void
set_load_state_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e, struct objfile *o, const char *arg)
{
  if (e == NULL)
    return;

  e->load_flag = dyld_parse_load_level (arg);
  e->load_flag |= dyld_minimal_load_flag (d, e);
}

static void
dyld_set_load_state_command (char *args, int from_tty)
{
  struct dyld_objfile_info original_info, modified_info;

  dyld_objfile_info_init (&original_info);
  dyld_objfile_info_init (&modified_info);

  dyld_objfile_info_copy (&original_info, &macosx_status->dyld_status.current_info);
  dyld_objfile_info_copy (&modified_info, &macosx_status->dyld_status.current_info);

  map_shlib_numbers (args, set_load_state_helper, &macosx_status->dyld_status.path_info, &modified_info);

  dyld_merge_shlibs (&macosx_status->dyld_status, &macosx_status->dyld_status.path_info,
                      &original_info, &modified_info);
  dyld_update_shlibs (&macosx_status->dyld_status, &macosx_status->dyld_status.path_info,
                      &modified_info);
  
  dyld_objfile_info_copy (&macosx_status->dyld_status.current_info, &modified_info);

  dyld_objfile_info_free (&original_info);
  dyld_objfile_info_free (&modified_info);
}
        
static void
section_info_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e,
		     struct objfile *o, const char *arg)
{
  int ret;

  if (o != NULL) {
    print_section_info_objfile (o);
  } else {
    ui_out_list_begin (uiout, "section-info");
    ui_out_list_end (uiout);
  }

  if (e != NULL) {
#if WITH_CFM
    if (e->cfm_container != 0) {

      NCFragContainerInfo container;
      struct cfm_parser *parser;
      unsigned long section_index;
      
      parser = &macosx_status->cfm_status.parser;

      ret = cfm_fetch_container_info (parser, e->cfm_container, &container);
      if (ret != 0)
        return;

      ui_out_list_begin (uiout, "cfm-section-info");

      for (section_index = 0; section_index < container.sectionCount; section_index++)
        {
          NCFragSectionInfo section;
          NCFragInstanceInfo instance;

          ret = cfm_fetch_container_section_info (parser, e->cfm_container, section_index, &section);
          if (ret != 0)
            break;

          ui_out_list_begin (uiout, "section");

          ui_out_text (uiout, "\t");
          ui_out_field_core_addr (uiout, "addr", instance.address);
          ui_out_text (uiout, " - ");
          ui_out_field_core_addr (uiout, "endaddr", instance.address + section.length);
          if (info_verbose)
            {
              ui_out_text (uiout, " @ ");
              ui_out_field_core_addr (uiout, "filepos", 0);
            }
          ui_out_text (uiout, " is ");
          ui_out_field_string (uiout, "name", "unknown");
#if 0
          if (p->objfile->obfd != abfd)
            {
              ui_out_text (uiout, " in ");
              ui_out_field_string (uiout, "filename", bfd_get_filename (p->bfd));
            }
#endif
          ui_out_text (uiout, "\n");

          ui_out_list_end (uiout); /* "section" */
        }

      ui_out_list_end (uiout); /* "cfm-section-info" */
    }
#endif /* WITH_CFM */
  }
}

static void
dyld_section_info_command (char *args, int from_tty)
{
  map_shlib_numbers (args, section_info_helper,
		     &macosx_status->dyld_status.path_info,
		     &macosx_status->dyld_status.current_info);
}
        
static
void info_sharedlibrary_command (char *args, int from_tty)
{
  CHECK_FATAL (macosx_status != NULL);
  dyld_print_status_info (&macosx_status->dyld_status, dyld_reason_all_mask | dyld_reason_user, args);
}

static
void info_sharedlibrary_all_command (char *args, int from_tty)
{
  CHECK_FATAL (macosx_status != NULL);
  dyld_print_shlib_info (&macosx_status->dyld_status.current_info, dyld_reason_all_mask | dyld_reason_user, 1, args);
}

static
void info_sharedlibrary_dyld_command (char *args, int from_tty)
{
  CHECK_FATAL (macosx_status != NULL);
  dyld_print_shlib_info (&macosx_status->dyld_status.current_info, dyld_reason_dyld, 1, args);
}

static void
info_sharedlibrary_cfm_command (char *args, int from_tty)
{
  CHECK_FATAL (macosx_status != NULL);
  dyld_print_shlib_info (&macosx_status->dyld_status.current_info, dyld_reason_cfm, 1, args);
}

static void
info_sharedlibrary_raw_cfm_command (char *args, int from_tty)
{
  task_t task = macosx_status->task;
  struct dyld_objfile_info info;

  dyld_objfile_info_init (&info);
  cfm_update (task, &info);

  dyld_print_shlib_info (&info, dyld_reason_cfm, 1, args);
}

int
dyld_lookup_and_bind_function (char *name)
{
  static struct cached_value *lookup_function = NULL;
  struct value *name_val;
  char *real_name;
  int name_len;
  CORE_ADDR ret_addr;

  if (! target_has_execution)
    return 0;

  /* Chicken and egg problem, need NSLookupAndBindSymbol already...
     Also, we need to have found malloc already to call
     NSLookupAndBindSymbol... */

  if (strcmp (name, "NSLookupAndBindSymbol") == 0)
    return 1;
  if (strcmp (name, "malloc") == 0)
    return 1;

  if (lookup_function == NULL)
    {
      if (lookup_minimal_symbol ("NSLookupAndBindSymbol", 0, 0))
	lookup_function = create_cached_function ("NSLookupAndBindSymbol", 
						  builtin_type_voidptrfuncptr);
      else
	error ("Couldn't find NSLookupAndBindSymbol.");
    }

  name_len = strlen (name);
  real_name = (char *) alloca (name_len + 2);
  real_name[0] = '_';
  memcpy (real_name + 1, name, name_len + 1);

  name_val = value_string (real_name, name_len + 2);
  name_val = value_coerce_array (name_val);

  ret_addr = value_as_long (call_function_by_hand (lookup_cached_function (lookup_function), 1, &name_val));

  if (ret_addr != 0x0)
    return 1;
  else
    return 0;
}

#if MAPPED_SYMFILES

static void
cache_symfiles_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e, struct objfile *o, const char *arg)
{
  struct objfile *old = NULL;
  struct objfile *new = NULL;
  bfd *abfd;

  if (e != NULL)
    {
      old = e->objfile;
      abfd =e->abfd;
      new = cache_bfd (e->abfd, e->prefix, e->load_flag, 0, 0, arg);
    }
  else
    {
      CHECK_FATAL (o != NULL);
      old = o;
      abfd =o->obfd;
      new = cache_bfd (o->obfd, o->prefix, o->symflags, 0, 0, arg);
    }

  if (new == NULL) {
    warning ("unable to move objfile");
    return;
  }

  if (e != NULL)
    {
      if (e->reason & dyld_reason_executable_mask) {
	CHECK_FATAL (e->objfile == symfile_objfile);
	symfile_objfile = new;
      }
      e->objfile = new;
    }
      
  if (old != NULL)
    unlink_objfile (old);
}

#endif /* MAPPED_SYMFILES */

static void
dyld_cache_symfiles_command (char *args, int from_tty)
{
#if MAPPED_SYMFILES
  map_shlib_numbers (args, cache_symfiles_helper, 
		     &macosx_status->dyld_status.path_info,
		     &macosx_status->dyld_status.current_info);
#else /* ! MAPPED_SYMFILES */
  error ("Cached symfiles not supported on this configuration of GDB.");
#endif /* MAPPED_SYMFILES */
}

static void
dyld_cache_symfile_command (char *args, int from_tty)
{
#if MAPPED_SYMFILES
  bfd *abfd = NULL;
  struct objfile *objfile = NULL;
  const char *filename = NULL;
  const char *dest = NULL;
  char **argv = NULL;
  struct cleanup *cleanups;
  const char *prefix;

  argv = buildargv (args);
  if (argv == NULL)
    nomem (0);
  cleanups = make_cleanup_freeargv (argv);
  if ((argv[0] == NULL) || (argv[1] == NULL) || ((argv[2] != NULL) && (argv[3] != NULL)))
    error ("usage: cache-symfile <source> <target> [prefix]");
  filename = argv[0];
  dest = argv[1];
  prefix = argv[2];

  abfd = symfile_bfd_open (filename, 0);
  if (abfd == NULL)
    error ("unable to open BFD for \"%s\"", filename);
  objfile = cache_bfd (abfd, prefix, OBJF_SYM_ALL, 0, 0, dest);
  free_objfile (objfile);
#else /* ! MAPPED_SYMFILES */
  error ("Cached symfiles not supported on this configuration of GDB.");
#endif /* MAPPED_SYMFILES */
}

void
update_section_tables (struct target_ops *target, struct dyld_objfile_info *info)
{
  struct dyld_objfile_entry *e;
  struct obj_section *osection;
  int nsections, csection, osections;
  unsigned int i;

  nsections = 0;
  DYLD_ALL_OBJFILE_INFO_ENTRIES (info, e, i) {
    if (e->objfile == NULL)
      continue;
    ALL_OBJFILE_OSECTIONS (e->objfile, osection)
      nsections++;
  }

  osections = target->to_sections_end - target->to_sections;
  target_resize_to_sections (target, nsections - osections);
  gdb_assert ((target->to_sections + nsections) == target->to_sections_end);

  csection = 0;
  DYLD_ALL_OBJFILE_INFO_ENTRIES (info, e, i) {
    if (e->objfile == NULL)
      continue;
    ALL_OBJFILE_OSECTIONS (e->objfile, osection) {
      gdb_assert (osection != NULL);
      gdb_assert (osection->objfile != NULL);
      target->to_sections[csection].addr = osection->addr;
      target->to_sections[csection].endaddr = osection->endaddr;
      target->to_sections[csection].the_bfd_section = osection->the_bfd_section;
      target->to_sections[csection].bfd = osection->objfile->obfd;
      csection++;
    }
  }

  gdb_assert ((target->to_sections + csection) == target->to_sections_end);
}

struct cmd_list_element *dyldlist = NULL;
struct cmd_list_element *setshliblist = NULL;
struct cmd_list_element *showshliblist = NULL;
struct cmd_list_element *infoshliblist = NULL;
struct cmd_list_element *shliblist = NULL;
struct cmd_list_element *maintenanceshliblist = NULL;

static void
maintenance_sharedlibrary_command (char *arg, int from_tty)
{
  printf_unfiltered ("\"maintenance sharedlibrary\" must be followed by the name of a sharedlibrary command.\n");
  help_list (maintenanceshliblist, "maintenance sharedlibrary ", -1, gdb_stdout);
}

static void
sharedlibrary_command (char *arg, int from_tty)
{
  printf_unfiltered ("\"sharedlibrary\" must be followed by the name of a sharedlibrary command.\n");
  help_list (shliblist, "sharedlibrary ", -1, gdb_stdout);
}

void
_initialize_macosx_nat_dyld ()
{
  struct cmd_list_element *cmd = NULL;

  dyld_stderr = fdopen (fileno (stderr), "w+");

  add_prefix_cmd ("sharedlibrary", class_run, sharedlibrary_command,
                  "Commands for shared library manipulation.",
                  &shliblist, "sharedlibrary ", 0, &cmdlist);

  add_prefix_cmd ("sharedlibrary", class_maintenance, maintenance_sharedlibrary_command,
		  "Commands for internal sharedlibrary manipulation.",
		  &maintenanceshliblist, "maintenance sharedlibrary ", 0, &maintenancelist);

  add_cmd ("add-symbol-file", class_run, dyld_add_symbol_file_command,
           "Add a symbol file.", &shliblist);

  add_cmd ("remove-symbol-file", class_run, dyld_remove_symbol_file_command,
           "Remove a symbol file.", &shliblist);

  add_cmd ("specify-symbol-file", class_run, dyld_specify_symbol_file_command,
           "Specify the symbol file for a sharedlibrary entry.", &shliblist);

  add_cmd ("set-load-state", class_run, dyld_set_load_state_command,
           "Set the load level of a library (given by the index from \"info sharedlibrary\").", &shliblist);

  add_cmd ("section-info", class_run, dyld_section_info_command,
           "Get the section info for a library (given by the index from \"info sharedlibrary\").", &shliblist);

  add_cmd ("cache-purge", class_obscure, dyld_cache_purge_command,
           "Purge all symbols for DYLD images cached by GDB.", &maintenanceshliblist);

  add_cmd ("update", class_run, macosx_dyld_update_command,
           "Process all pending DYLD events.", &shliblist);

  add_prefix_cmd ("sharedlibrary", no_class, info_sharedlibrary_command,
                  "Generic command for shlib information.",
                  &infoshliblist, "info sharedlibrary ", 0, &infolist);

  add_cmd ("all", no_class, info_sharedlibrary_all_command, "Show current DYLD state.", &infoshliblist);  
  add_cmd ("dyld", no_class, info_sharedlibrary_dyld_command, "Show current DYLD state.", &infoshliblist);  
  add_cmd ("cfm", no_class, info_sharedlibrary_cfm_command, "Show current CFM state.", &infoshliblist);
  add_cmd ("raw-cfm", no_class, info_sharedlibrary_raw_cfm_command, "Show current CFM state.", &infoshliblist);

  add_prefix_cmd ("sharedlibrary", no_class, not_just_help_class_command,
                  "Generic command for setting shlib settings.",
                  &setshliblist, "set sharedlibrary ", 0, &setlist);

  add_prefix_cmd ("sharedlibrary", no_class, not_just_help_class_command,
                  "Generic command for showing shlib settings.",
                  &showshliblist, "show sharedlibrary ", 0, &showlist);

  cmd = add_set_cmd ("filter-events", class_obscure, var_boolean,
                     (char *) &dyld_filter_events_flag,
                     "Set if GDB should filter shared library events to a minimal set.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);                

  cmd = add_set_cmd ("preload-libraries", class_obscure, var_boolean, 
                     (char *) &dyld_preload_libraries_flag,
                     "Set if GDB should pre-load symbols for DYLD libraries.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);                

  cmd = add_set_cmd ("load-dyld-symbols", class_obscure, var_boolean, 
                     (char *) &dyld_load_dyld_symbols_flag,
                     "Set if GDB should load symbol information for the dynamic linker.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("load-dyld-shlib-symbols", class_obscure, var_boolean, 
                     (char *) &dyld_load_dyld_shlib_symbols_flag,
                     "Set if GDB should load symbol information for DYLD-based shared libraries.",
                     &setshliblist);
  deprecate_cmd (cmd, "set sharedlibrary load-rules");
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("load-cfm-shlib-symbols", class_obscure, var_boolean, 
                     (char *) &dyld_load_cfm_shlib_symbols_flag,
                     "Set if GDB should load symbol information for CFM-based shared libraries.",
                     &setshliblist);
  deprecate_cmd (cmd, "set sharedlibrary load-rules");
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("dyld-symbols-prefix", class_obscure, var_string, 
                     (char *) &dyld_symbols_prefix,
                     "Set the prefix that GDB should prepend to all symbols for the dynamic linker.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);
  dyld_symbols_prefix = xstrdup (dyld_symbols_prefix);

  cmd = add_set_cmd ("always-read-from-memory", class_obscure, var_boolean, 
                     (char *) &dyld_always_read_from_memory_flag,
                     "Set if GDB should always read loaded images from the inferior's memory.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("print-basenames", class_obscure, var_boolean,
                     (char *) &dyld_print_basenames_flag,
                     "Set if GDB should print the basenames of loaded files when printing progress messages.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("load-rules", class_support, var_string,
                     (char *) &dyld_load_rules,
                     "Set the rules governing the level of symbol loading for shared libraries.\n\
 * Each load rule is a triple.\n\
 * The command takes a flattened list of triples.\n\
 * The first two elements of the triple specify the library, by giving \n\
      - \"who loaded it\" (i.e. dyld, cfm or all) in the first element, \n\
      - and a regexp to match the library name in the second. \n\
 * The last element specifies the level of loading for that library\n\
      - The options are:  all, extern, container or none.\n\
\n\
Example: To load only external symbols from all dyld-based system libraries, use: \n\
    set sharedlibrary load-rules dyld ^/System/Library.* extern\n",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("minimal-load-rules", class_support, var_string,
                     (char *) &dyld_minimal_load_rules,
                     "Set the minimal DYLD load rules.  These prime the main list.\n\
gdb relies on some of these for proper functioning, so don't remove elements from it\n\
unless you know what you are doing.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("dyld", class_obscure, var_zinteger, 
                     (char *) &dyld_debug_flag,
                     "Set if printing dyld communication debugging statements.",
                     &setdebuglist);
  add_show_from_set (cmd, &showdebuglist);

  dyld_minimal_load_rules = xstrdup ("\"dyld\" \"CarbonCore$\\\\|CarbonCore_[^/]*$\" all \".*\" \"dyld$\" extern \".*\" \".*\" none");
  dyld_load_rules = xstrdup ("\".*\" \".*\" all");

  cmd = add_set_cmd ("stop-on-shlibs-added", class_support, var_zinteger,
		     (char *) &dyld_stop_on_shlibs_added,
		     "Set if a shlib event should be reported on a shlibs-added event.", &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("stop-on-shlibs-updated", class_support, var_zinteger,
		     (char *) &dyld_stop_on_shlibs_updated,
		     "Set if a shlib event should be reported on a shlibs-updated event.", &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("combine-shlibs-added", class_support, var_zinteger,
		     (char *) &dyld_combine_shlibs_added,
		     "Set if GDB should combine shlibs-added events from the same image into a single event.", &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  add_cmd ("cache-symfiles", class_run, dyld_cache_symfiles_command,
           "Generate persistent caches of symbol files for the current executable state", &shliblist);

  add_cmd ("cache-symfile", class_run, dyld_cache_symfile_command,
           "Generate persistent caches of symbol files for the current executable state", &shliblist);
}
