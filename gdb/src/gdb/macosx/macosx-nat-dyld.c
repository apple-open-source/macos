/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002, 2004, 2005
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
#include <mach-o/loader.h>

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcmd.h"
#include "annotate.h"
#include "mach-o.h"
#include "gdbcore.h"            /* for core_ops */
#include "symfile.h"
#include "objfiles.h"
#include "gdb_assert.h"
#include "gdb_stat.h"
#include "regcache.h"
#include "bfd.h"
#include "gdb-stabs.h"
#include "objc-lang.h"
#include "gdbarch.h"

#ifdef USE_MMALLOC
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

#include <AvailabilityMacros.h>

#define MACH64 (MAC_OS_X_VERSION_MAX_ALLOWED >= 1040)

#if MACH64

#include <mach/mach_vm.h>

#else /* ! MACH64 */

#define mach_vm_size_t vm_size_t
#define mach_vm_address_t vm_address_t
#define mach_vm_read vm_read
#define mach_vm_write vm_write
#define mach_vm_region vm_region
#define mach_vm_protect vm_protect
#define VM_REGION_BASIC_INFO_COUNT_64 VM_REGION_BASIC_INFO_COUNT
#define VM_REGION_BASIC_INFO_64 VM_REGION_BASIC_INFO

#endif /* MACH64 */

/* For the gdbarch_tdep structure so we can get the wordsize. */

#if defined (__powerpc__) || defined (__ppc__) || defined (__ppc64__)
#include "ppc-tdep.h"
#elif defined (__i386__)
#include "i386-tdep.h"
#else
#error "Unrecognized target architecture."
#endif

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
int dyld_reload_on_downgrade_flag = 0;
char *dyld_load_rules = NULL;
char *dyld_minimal_load_rules = NULL;

FILE *dyld_stderr = NULL;
int dyld_debug_flag = 0;

extern int inferior_auto_start_cfm_flag;

int dyld_stop_on_shlibs_added = 1;
int dyld_stop_on_shlibs_updated = 1;
int dyld_combine_shlibs_added = 1;

/* A structure filled in by dyld in the inferior process.
   There is only one of these in the inferior.  */

struct dyld_raw_infos
{
  uint32_t version;             /* MacOS X 10.4 == 1 */
  uint32_t num_info;            /* Number of elements in the following array */

  /* Array of images (struct dyld_raw_info here in gdb) that are loaded
     in the inferior process.
     Note that this address may change over the lifetime of a process;
     as the array grows, dyld may need to realloc () the array.  So don't
     cache the value of info_array except while the inferior is stopped.
     This is either 4 or 8 bytes in the inferior, depending on wordsize.
     This value can be 0 (NULL) if dyld is in the middle of updating the
     array.  Currently, we'll just fail in that (unlikely) circumstance.  */

  CORE_ADDR info_array;

  /* Function called by dyld after a new dylib/bundle (or group of
     dylib/bundles) has been loaded, but before those images have had
     their initializer functions run.  This function has a prototype of

     void dyld_image_notifier (enum dyld_image_mode mode, uint32_t infoCount,
     const struct dyld_image_info info[]);

     Where mode is either dyld_image_adding (0) or dyld_image_removing (1).
     This is either 4 or 8 bytes in the inferior, depending on wordsize. */

  CORE_ADDR dyld_notify;
};

/* A structure filled in by dyld in the inferior process.
   Each dylib/bundle loaded has one of these structures allocated
   for it.
   Each field is either 4 or 8 bytes, depending on the wordsize of
   the inferior process.  (including the modtime field - size_t goes to
   64 bits in the 64 bit ABIs).  */

struct dyld_raw_info
{
  CORE_ADDR addr;               /* struct mach_header *imageLoadAddress */
  CORE_ADDR name;               /* const char *imageFilePath */
  uint64_t modtime;             /* time_t imageFileModDate */
};

void clear_gdbarch_swap (struct gdbarch *);
void swapout_gdbarch_swap (struct gdbarch *);
void swapin_gdbarch_swap (struct gdbarch *);

static void info_sharedlibrary_address (CORE_ADDR);
static void set_load_state_1 (struct dyld_objfile_entry *e,
                              const struct dyld_path_info *d, int load_state);

static void dyld_read_raw_infos (CORE_ADDR addr, struct dyld_raw_infos *info);

static void dyld_info_read_raw (struct macosx_dyld_thread_status *status,
                                struct dyld_raw_info **rinfo,
                                int *rninfo);

static void dyld_info_read_raw_data (CORE_ADDR addr, int num,
                                     struct dyld_raw_info *rinfo);

static int dyld_starts_here_p (mach_vm_address_t addr);

static int dyld_info_process_raw (struct dyld_objfile_entry *entry,
                                  CORE_ADDR name, uint64_t modtime,
                                  CORE_ADDR header);

void
dyld_debug (const char *fmt, ...)
{
  va_list ap;
  if (dyld_debug_flag >= 1)
    {
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
  switch (ret)
    {
    case DYLD_SUCCESS:
      return "DYLD_SUCCESS";
    case DYLD_INCONSISTENT_DATA:
      return "DYLD_INCONSISTENT_DATA";
    case DYLD_INVALID_ARGUMENTS:
      return "DYLD_INVALID_ARGUMENTS";
    case DYLD_FAILURE:
      return "DYLD_FAILURE";
    default:
      return "[UNKNOWN]";
    }
}

const char *
dyld_debug_event_string (enum dyld_event_type type)
{
  switch (type)
    {
    case DYLD_IMAGE_ADDED:
      return "DYLD_IMAGE_ADDED";
    case DYLD_MODULE_BOUND:
      return "DYLD_MODULE_BOUND";
    case DYLD_MODULE_REMOVED:
      return "DYLD_MODULE_REMOVED";
    case DYLD_MODULE_REPLACED:
      return "DYLD_MODULE_REPLACED";
    case DYLD_PAST_EVENTS_END:
      return "DYLD_PAST_EVENTS_END";
    case DYLD_IMAGE_REMOVED:
      return "DYLD_IMAGE_REMOVED";
    default:
      return "[UNKNOWN]";
    }
}

void
dyld_print_status_info (struct macosx_dyld_thread_status *s,
                        unsigned int mask, char *args)
{
  switch (s->state)
    {
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
macosx_clear_start_breakpoint (void)
{
  remove_solib_event_breakpoints ();
}

static CORE_ADDR
lookup_dyld_address (macosx_dyld_thread_status *status, const char *s)
{
  struct minimal_symbol *msym = NULL;
  CORE_ADDR sym_addr;
  char *ns = NULL;

  xasprintf (&ns, "%s%s", dyld_symbols_prefix, s);
  msym = lookup_minimal_symbol (ns, NULL, NULL);
  xfree (ns);

  if (msym == NULL)
    return (CORE_ADDR) - 1;

  sym_addr = SYMBOL_VALUE_ADDRESS (msym);
  return (sym_addr + status->dyld_slide);
}

/* Find the dyld_all_image_infos structure in the inferior's dyld.
   This is the top-level data structure that we use to find all the
   loaded dylibs in the inferior and the hook to get notifications
   about new ones.  */

void
macosx_init_addresses (macosx_dyld_thread_status *s)
{
  struct dyld_raw_infos infos;

  s->image_infos = lookup_dyld_address (s, "dyld_all_image_infos");

  dyld_read_raw_infos (s->image_infos, &infos);

  s->dyld_version = infos.version;
  s->dyld_notify = infos.dyld_notify + s->dyld_slide;
}

static int
dyld_starts_here_p (mach_vm_address_t addr)
{
  mach_vm_address_t address = addr;
  mach_vm_size_t size = 0;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t info_cnt = sizeof (vm_region_basic_info_data_64_t);
  kern_return_t ret;
  mach_port_t object_name;
  vm_address_t data;
  vm_size_t data_count;

  struct mach_header *mh;

  info_cnt = VM_REGION_BASIC_INFO_COUNT_64;
  ret = mach_vm_region (macosx_status->task, &address, &size, VM_REGION_BASIC_INFO_64,
                   (vm_region_info_t) & info, &info_cnt, &object_name);

  if (ret != KERN_SUCCESS)
    return 0;

  /* If it is not readable, it is not dyld. */

  if ((info.protection & VM_PROT_READ) == 0)
    return 0;

  ret = mach_vm_read (macosx_status->task, address, size, &data, &data_count);

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

  if ((mh->magic != MH_MAGIC && mh->magic != MH_CIGAM &&
       mh->magic != MH_MAGIC_64 && mh->magic != MH_CIGAM_64) ||
      mh->filetype != MH_DYLINKER ||
      data_count < sizeof (struct mach_header) + mh->sizeofcmds)
    {
      ret = vm_deallocate (mach_task_self (), data, data_count);
      return 0;
    }

  /* Looks like dyld, smells like dyld -- must be dyld! */
  ret = vm_deallocate (mach_task_self (), data, data_count);

  return 1;
}

/* Tries to find the name string for the dynamic linker passed as
   ABFD.  Returns 1 if it found a name (and returns xmalloc'ed name in *RNAME).
   Return 0 if no name was found; -1 if there was an error. */

static int
macosx_lookup_dyld_name (bfd *abfd, const char **rname)
{
  struct mach_o_data_struct *mdata = NULL;
  char *name = NULL;
  int i;

  if (abfd == NULL)
    return -1;
  if (!bfd_mach_o_valid (abfd))
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
              name = NULL;
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
   VALUE.  Returns 1 if it found dyld, 0 if it didn't, -1 if there
   was an error.  If HINT is not INVALID_ADDRESS, check there first
   as a hint for where to find dyld. */

static int
macosx_locate_dyld (CORE_ADDR *value, CORE_ADDR hint)
{
  if (hint != INVALID_ADDRESS && dyld_starts_here_p (hint))
    {
      *value = hint;
      return 1;
    }
  else
    {
      kern_return_t kret = KERN_SUCCESS;
      vm_region_basic_info_data_64_t info;
      int info_cnt = sizeof (vm_region_basic_info_data_64_t);
      mach_vm_address_t test_addr = VM_MIN_ADDRESS;
      mach_vm_size_t size = 0;
      mach_port_t object_name = PORT_NULL;
      task_t target_task = macosx_status->task;

  info_cnt = VM_REGION_BASIC_INFO_COUNT_64;

      do
        {
          kret = mach_vm_region (target_task, &test_addr,
                            &size, VM_REGION_BASIC_INFO_64,
                            (vm_region_info_t) & info, &info_cnt,
                            &object_name);

          if (kret != KERN_SUCCESS)
            return -1;

          if (dyld_starts_here_p (test_addr))
            {
              *value = test_addr;
              return 1;
            }

          test_addr += size;

        }
      while (size != 0);
    }

  return 0;
}

/* Determine the address where the current target expected dyld to be
   loaded.  This is used when first guessing the address of dyld for
   symbol loading on dynamic executables, and also to compute the
   proper slide to be applied to values found in the dyld memory
   itself.  Returns 1 if it found the address, 0 if it did not. */

int
macosx_locate_dyld_static (macosx_dyld_thread_status *s, CORE_ADDR *value)
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

  /* Find the location of the dyld, if it hasn't been found already.*/
  /* Store the old value, so we'll know if it was changed. */

  prev_dyld_address = s->dyld_addr;

  /* Take our best guess as to where to look for dyld, just as an
     optimization. */

  macosx_locate_dyld_static (s, &static_dyld_address);
  if (s->dyld_addr != INVALID_ADDRESS)
    ret = macosx_locate_dyld (&dyld_address, s->dyld_addr);
  else if (static_dyld_address != INVALID_ADDRESS)
    ret = macosx_locate_dyld (&dyld_address, static_dyld_address);
  else
    ret = macosx_locate_dyld (&dyld_address, INVALID_ADDRESS);

  /* If we didn't find dyld, there's no point continuing. */

  if (ret != 1)
    return;
  CHECK_FATAL (dyld_address != INVALID_ADDRESS);

  /* Now grub for dyld's name in memory if we haven't found it already. */
  if (dyld_address != INVALID_ADDRESS)
    {
      struct mach_header header;
      target_read_memory (dyld_address, (char *) &header,
                          sizeof (struct mach_header));
      if (s->dyld_name == NULL)
        s->dyld_name = dyld_find_dylib_name (dyld_address, header.ncmds);

      /* Store the osabi of the found-dyld in these global variables because
         we need to use this over in the tdep.c file where we don't always
         have access to the macosx_dyld_thread_status or macosx_status
         structures.  */
      if (header.cputype == CPU_TYPE_POWERPC)
        osabi_seen_in_attached_dyld = GDB_OSABI_DARWIN;
      if (header.cputype == CPU_TYPE_POWERPC64)
        osabi_seen_in_attached_dyld = GDB_OSABI_DARWIN64;
    }

  /* Once we know the address at which dyld was loaded, we can try to
     read in the symbols for it, and then hopefully find the static
     address and use it to compute the slide. */

  s->dyld_addr = dyld_address;
  macosx_dyld_update (1);
  macosx_locate_dyld_static (s, &static_dyld_address);

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

/* Put a breakpoint in the dyld function (in the inferior) which is
   called every time an image/a group of images (bundles, dylibs, etc.) are
   added/removed from the inferior process.  */

void
macosx_set_start_breakpoint (macosx_dyld_thread_status *s, bfd *exec_bfd)
{
  struct macosx_dyld_thread_status *status = &macosx_status->dyld_status;

  if (status->dyld_breakpoint == NULL)
    {
      status->dyld_breakpoint = create_solib_event_breakpoint 
                                         (status->dyld_notify);
      breakpoints_changed ();
    }
}

#if defined (__powerpc__) || defined (__ppc__)
static ULONGEST
FETCH_ARGUMENT (int i)
{
  return read_register (3 + i);
}
#elif defined (__i386__)
static ULONGEST
FETCH_ARGUMENT (int i)
{
  CORE_ADDR stack = read_register (SP_REGNUM);
  return read_memory_unsigned_integer (stack + (4 * (i + 1)), 4);
}
#else
#error unknown architecture
#endif

/* Add the dyld_objfile_entry ENTRIES (an array of N of them) to the
   inferior process' DYLD_OBJFILE_INFO list of known images.

   We may already have dyld_objfile_entry records for some of the ENTRIES,
   e.g. because we looked at the program's load commands before starting it
   and now we're getting dylib-loaded notifications from dyld.  So we want
   to replace the old entries with these newer, shinier ones.  */

void
macosx_dyld_add_libraries (struct macosx_dyld_thread_status *dyld_status,
                           struct dyld_objfile_entry *entries,
                           int num)
{
  int i;
  int shlibnum;

  for (i = 0; i < num; i++)
    {
      struct dyld_objfile_entry *pentry;
      dyld_merge_shlib (dyld_status, &dyld_status->path_info,
                        &dyld_status->current_info, &entries[i]);
      dyld_prune_shlib (&dyld_status->path_info, 
			&dyld_status->current_info, &entries[i]);

      pentry = dyld_objfile_entry_alloc (&dyld_status->current_info);
      *pentry = entries[i];
    }

  dyld_update_shlibs (&dyld_status->path_info, &dyld_status->current_info);

  if (! ui_out_is_mi_like_p (uiout))
    return;

  for (i = 0; i < num; i++)
    {
      struct dyld_objfile_entry *entry;
      int j;
      DYLD_ALL_OBJFILE_INFO_ENTRIES (&dyld_status->current_info, entry, j)
        if (dyld_libraries_compatible (&dyld_status->path_info, entry,
                                       &entries[i]))
          {
            /* Get the shlib number of this entry */
            CHECK_FATAL (dyld_entry_shlib_num (&dyld_status->current_info,
                         entry, &shlibnum) == 0);
            struct cleanup *notify_cleanup;
            notify_cleanup = make_cleanup_ui_out_notify_begin_end (uiout,
                                                              "shlibs-added");
            dyld_print_entry_info (entry, shlibnum, 0);
            do_cleanups (notify_cleanup);
	    /* We have to add objfiles to the list of objfiles which
	       should be scanned for new breakpoints when we see them
	       get loaded.
  
	       This is necessary because if we try to set a breakpoint
	       before the objfile gets loaded into memory we might get
	       it wrong - mostly because we try to move past the
	       prologue, but the memory for the breakpoint's not
	       mapped yet, so we can't read the instructions.  When
	       that happens - for now just in breakpoint_re_set - we
	       mark the breakpoint as unset.  So we need to try it
	       again here.  */
	    if (entry->objfile != NULL)
	      breakpoint_re_set (entry->objfile);
          }
    }
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

  /* macosx_dyld_init () can fail to set up the dyld addresses when
     we're re-setting the inferior process and this func is called
     from exec.c before the new process is up and running.  e.g. this
     sequence of commands:
        b main
        run
        file /path/to/some/file
        y
     will result in this function being called from solib_add_stub ()
     without an inferior process yet initialized -- in which case, none of
     the below checks are going to accomplish anything.  So we'll just close
     our eyes for the nonce and wait to be called again when there's an
     inferior to inspect.  */

  if (dyld_status->dyld_addr == INVALID_ADDRESS)
    return 0;


  /* If the inferior stopped at the dyld notification function,
     some file images have been loaded or removed.  */

  if (dyld_status->dyld_breakpoint != NULL
      && dyld_status->dyld_breakpoint->loc->address == read_pc ())
    {
      static struct dyld_raw_info *rinfo = NULL;
      static struct dyld_objfile_entry *tinfo = NULL;
      static unsigned int maxent = 0;
      unsigned int i;
      unsigned int j;

      int mode = FETCH_ARGUMENT (0);             /* Adding or removing */
      int count = FETCH_ARGUMENT (1);            /* How many */
      CORE_ADDR addr = FETCH_ARGUMENT (2);       /* ptr to array of structs */

      /* FIXME: Image files are being removed.  Which we just ignore for
         now.  */
      if (mode == 1)
        return 0;     /* Nothing to notify */

      if (count > maxent)
        {
          maxent = (count >= 16) ? count : 16;
          rinfo = xrealloc (rinfo, maxent * sizeof (struct dyld_raw_info));
          tinfo =
            xrealloc (tinfo, maxent * sizeof (struct dyld_objfile_entry));
        }

      dyld_info_read_raw_data (addr, count, rinfo);

      j = 0;
      for (i = 0; i < count; i++)
        {
          dyld_objfile_entry_clear (&tinfo[i]);
          tinfo[i].allocated = 1;
          tinfo[i].reason = dyld_reason_dyld;
          tinfo[i].dyld_valid = 1;
          if (dyld_info_process_raw
              (&tinfo[j], rinfo[i].name, rinfo[i].modtime, rinfo[i].addr) == 1)
            j++;
        }

      macosx_dyld_add_libraries (&macosx_status->dyld_status, tinfo, j);

      libraries_changed = 1;

      /* Since we want to re-check breakpoints in libraries that get
	 loaded (to catch cases where we originally didn't insert
	 a breakpoint because it wasn't loaded and we couldn't read
	 the memory for the breakpoint) we need to update all the
	 breakpoints here after we've added all the libraries.  */

      breakpoint_update ();

      notify = libraries_changed && dyld_stop_on_shlibs_updated;
    }
  else if (cfm_status->cfm_breakpoint != NULL
           && cfm_status->cfm_breakpoint->loc->address == read_pc ())
    {
      /* no cfm support for incremental update yet */
      libraries_changed = macosx_dyld_update (0);
      notify = libraries_changed && dyld_stop_on_shlibs_updated;
    }
  else if (read_pc () == 0)
    {
      /* initial update -- $pc == 0x0 means we're not executing */
      libraries_changed = macosx_dyld_update (0);
      notify = libraries_changed && dyld_stop_on_shlibs_updated;
    }
  else if (dyld_status->dyld_breakpoint != NULL)
    {
      /* looks like an attach -- we're not at the dyld_breakpoint, but
         the dyld_breakpoint has already been set.  */
      libraries_changed = macosx_dyld_update (0);
      notify = libraries_changed && dyld_stop_on_shlibs_updated;
    }
  else
    {
      internal_error (__FILE__, __LINE__,
                      "unrecognized shared library breakpoint");
      notify = 0;
    }

  return notify;
}

void
macosx_dyld_thread_init (macosx_dyld_thread_status *s)
{
  s->dyld_name = NULL;
  s->dyld_addr = INVALID_ADDRESS;
  s->dyld_slide = INVALID_ADDRESS;
  s->dyld_version = 0;
  s->dyld_breakpoint = NULL;
  dyld_zero_path_info (&s->path_info);
}

void
macosx_add_shared_symbol_files (void)
{
  CHECK_FATAL (macosx_status != NULL);
  macosx_dyld_update (0);
}

void
macosx_init_dyld (struct macosx_dyld_thread_status *s,
                  struct objfile *o, bfd *abfd)
{
  struct dyld_objfile_entry *e;
  int i;
  struct dyld_objfile_info previous_info;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (&s->current_info, e, i)
    if (e->reason & dyld_reason_executable_mask)
      dyld_objfile_entry_clear (e);

  dyld_init_paths (&s->path_info);

  dyld_objfile_info_init (&previous_info);

  dyld_objfile_info_copy (&previous_info, &s->current_info);
  dyld_objfile_info_free (&s->current_info);

  if (o != NULL)
    {
      char *objfile_name = NULL;
      struct dyld_objfile_entry *e;

      /* Canonicalize the name */
      if (bfd_get_filename (o->obfd) != NULL)
        {
          char buf[PATH_MAX];
          if (realpath (bfd_get_filename (o->obfd), buf) != NULL)
            {
              objfile_name = xstrdup (buf);
            }
          else
            objfile_name = bfd_get_filename (o->obfd);
        }

      e = dyld_objfile_entry_alloc (&s->current_info);
      e->text_name_valid = 1;
      e->reason = dyld_reason_executable;
      if (o != NULL)
        {
          e->objfile = o;
          /* No need to set e->abfd, since e->objfile is present. */
          e->load_flag = o->symflags;
          e->text_name = objfile_name;
        }
      e->loaded_from_memory = 0;
      e->loaded_name = e->text_name;
      e->loaded_addr = 0;
      e->loaded_addrisoffset = 1;
    }

  if (dyld_preload_libraries_flag)
    {
      dyld_add_inserted_libraries (&s->current_info, &s->path_info);
      if (abfd != NULL)
        {
          dyld_add_image_libraries (&s->current_info, abfd);
        }
    }

  dyld_merge_shlibs (s, &s->path_info, &previous_info, &s->current_info);
  dyld_update_shlibs (&s->path_info, &s->current_info);

  dyld_objfile_info_free (&previous_info);

  s->state = dyld_initialized;
}

void
macosx_init_dyld_symfile (struct objfile *o, bfd *abfd)
{
  CHECK_FATAL (macosx_status != NULL);
  macosx_init_dyld (&macosx_status->dyld_status, o, abfd);
}

static void
dyld_cache_purge_command (char *exp, int from_tty)
{
  CHECK_FATAL (macosx_status != NULL);
  dyld_purge_cached_libraries (&macosx_status->dyld_status.current_info);
}

/* Process the information about a just-discovered file image from dyld
   and fill in a dyld_objfile_entry with the preliminary information.  */

static int
dyld_info_process_raw (struct dyld_objfile_entry *entry,
                       CORE_ADDR name, uint64_t modtime, CORE_ADDR header)
{
  struct mach_header headerbuf;
  char *namebuf = NULL;
  int errno, i;
  CORE_ADDR curpos;
  CORE_ADDR intended_loadaddr;
  struct load_command cmd;
  struct dylib_command dcmd;

  target_read_memory (header, (char *) &headerbuf, sizeof (struct mach_header));

  switch (headerbuf.filetype)
    {
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
      warning
        ("Ignored unknown object module at 0x%s with type 0x%x\n",
         paddr_nz (header), headerbuf.filetype);
      return 0;
    }

  target_read_string (name, &namebuf, 1024, &errno);

  if (errno != 0)
    {
      xfree (namebuf);
      namebuf = NULL;
    }

  if (namebuf != NULL)
    {
      char *s = strchr (namebuf, ':');
      if (s != NULL)
        {
          *s = '\0';
        }
    }

  /* Look through the load commands for the image filename.  */

  if (namebuf == NULL)
    {
      namebuf = xmalloc (256);
      curpos = header + sizeof (struct mach_header);

      for (i = 0; i < headerbuf.ncmds; i++)
        {
          target_read_memory (curpos, (char *) &cmd,
                              sizeof (struct load_command));
          if (cmd.cmd == LC_ID_DYLIB)
            {
              target_read_memory (curpos, (char *) &dcmd,
                                  sizeof (struct dylib_command));
              target_read_memory (curpos + dcmd.dylib.name.offset, namebuf,
                                  256);
              break;
            }
          curpos += cmd.cmdsize;
        }
    }

  /* realpath the image name -- we can get many different paths to the
     same file handed to us, so try to canonicalize them via realpath.  */

  if (namebuf != NULL)
    {
      char buf[PATH_MAX];
      char *resolved = NULL;

      resolved = namebuf;

      resolved = realpath (namebuf, buf);
      if (resolved == NULL)
        resolved = namebuf;

      entry->dyld_name = xstrdup (resolved);
      entry->dyld_name_valid = 1;
    }

  /* Find what address the image intended to load at, and compute the slide
     (the difference between the intended load addr and the actual load addr.)
     To get the address, grab the vmaddr out of the __TEXT LC_SEGMENT load
     command.  It's dumb to do this all by hand instead of using BFD, but all
     we have is the mach_header sitting in the inferior...
   */

  intended_loadaddr = INVALID_ADDRESS;

  if (gdbarch_tdep (current_gdbarch)->wordsize == 4)
    curpos = header + sizeof (struct mach_header);
  else
    curpos = header + sizeof (struct mach_header_64);

  for (i = 0; i < headerbuf.ncmds; i++)
    {
      target_read_memory (curpos, (char *) &cmd, sizeof (struct load_command));
      if (cmd.cmd == LC_SEGMENT)
        {
          struct segment_command segcmd;
          target_read_memory (curpos, (char *) &segcmd,
                              sizeof (struct segment_command));
          if (strcmp (segcmd.segname, "__TEXT") == 0)
            {
              intended_loadaddr = segcmd.vmaddr;
              break;
            }
        }
      else if (cmd.cmd == LC_SEGMENT_64)
        {
          struct segment_command_64 segcmd;
          target_read_memory (curpos, (char *) &segcmd,
                              sizeof (struct segment_command_64));
          if (strcmp (segcmd.segname, "__TEXT") == 0)
            {
              intended_loadaddr = segcmd.vmaddr;
              break;
            }
        }
      curpos = curpos + cmd.cmdsize;
    }

  /* The slide is the difference between the actual load address and
     the intended load address.  So if a library was supposed to load
     at 0x1f00 and it actually loads at 0x2f00 because another library
     was there, we have a slide of 0x1000.

     If it was supposed to load at 0x2f00 but it slides *down* to
     0x1f00, the slide will be 0xfffffffffffff000 in a 64-bit CORE_ADDR or
     0xfffff000 in a 32-bit CORE_ADDR.
     In short the slide is added to the intended load address and it "wraps
     around" to give you the actual load address. if you have a so-called
     "negative slide".
     e.g  0x2f00 + 0xfffffffffffff000 == 0x1f00 in 64-bit unsigned math.  */

  entry->dyld_addr = header;
  entry->dyld_slide = header - intended_loadaddr;
  entry->dyld_valid = 1;

  switch (headerbuf.filetype)
    {
    case MH_EXECUTE:
      {
        entry->reason = dyld_reason_executable;
        if (symfile_objfile != NULL)
          {
            entry->objfile = symfile_objfile;
            /* No need to set e->abfd, since e->objfile is present. */
            entry->loaded_from_memory = 0;
            entry->loaded_name = symfile_objfile->name;
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
      internal_error (__FILE__, __LINE__,
                      "Unknown object module at 0x%s (offset 0x%s) with type 0x%x\n",
                      paddr_nz (header), paddr_nz (entry->dyld_slide),
                      headerbuf.filetype);
    }

  return 1;
}

/* Given the ADDR of the struct dyld_all_image_infos in the inferior,
   copy it from the inferior's memory into INFO.

   NB: the value for info->info_array may change as dyld grows the array
   of image files that are loaded (it'll realloc() the array), so we can't
   assume this value is constant across inferior running.
*/

static void
dyld_read_raw_infos (CORE_ADDR addr, struct dyld_raw_infos *info)
{
  char *buf = (char *) alloca (sizeof (struct dyld_raw_infos));
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;

  /* if ADDR is zero, we're looking at a not-yet-running process,
     e.g. the user did "file such-and-so" from the CLI.  Why are we
     down in this function then?  macosx-nat-dyld is mysterious and mere
     mortals are not meant to understand all its motivations.  So we
     assign 0's to everything and pretend everything is all right.  */

  if (addr == 0)
    {
      info->version = info->num_info = info->info_array = info->dyld_notify = 0;
      return;
    }

  /* The struct dyld_raw_infos in the inferior consists of two 4-byte
     words and two (inferior) wordsize words.  */
  target_read_memory (addr, buf, 8 + wordsize * 2);

  info->version = extract_unsigned_integer (buf, 4);
  info->num_info = extract_unsigned_integer (buf + 4, 4);
  info->info_array = extract_unsigned_integer (buf + 8, wordsize);
  info->dyld_notify = extract_unsigned_integer (buf + 8 + wordsize, wordsize);
}

/* Read an array of NUM dyld_raw_info structures out of the inferior's
   dyld at ADDR and store the data in RINFO.
   In gdb, struct dyld_raw_info is the maximal size of the fields.
   In the inferior, some of the fields will be 4 or 8 bytes depending on
   the wordsize of the inferior.  */

static void
dyld_info_read_raw_data (CORE_ADDR addr, int num, struct dyld_raw_info *rinfo)
{
  char *buf;
  int i;
  int size_of_dyld_raw_info_in_inferior;
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;

  size_of_dyld_raw_info_in_inferior = wordsize * 3;  /* three fields */

  if (num == 0)
    return;

  buf = xmalloc (sizeof (struct dyld_raw_info) * num);

  target_read_memory (addr, buf, num * size_of_dyld_raw_info_in_inferior);

  for (i = 0; i < num; i++)
    {
      char *ebuf = buf + (i * size_of_dyld_raw_info_in_inferior);
      rinfo[i].addr = extract_unsigned_integer (ebuf, wordsize);
      ebuf += wordsize;
      rinfo[i].name = extract_unsigned_integer (ebuf, wordsize);
      ebuf += wordsize;
      rinfo[i].modtime = extract_unsigned_integer (ebuf, wordsize);
    }
  xfree (buf);
}

/* Given the address of the struct dyld_all_image_infos in the
   inferior, read the struct dyld_all_image_infos and the entire
   array of struct dyld_image_info's out of the inferior's dyld.
   Sets RINFO to point to this (xmalloc'ed) array of dyld_raw_info's
   and RNINFO to the number of elements in the array. */

static void
dyld_info_read_raw (struct macosx_dyld_thread_status *status,
                    struct dyld_raw_info **rinfo, int *rninfo)
{
  struct dyld_raw_infos info;
  struct dyld_raw_info *ninfo;

  dyld_read_raw_infos (status->image_infos, &info);

  ninfo = xmalloc (info.num_info * sizeof (struct dyld_raw_info));

  dyld_info_read_raw_data (info.info_array, info.num_info, ninfo);

  *rinfo = ninfo;
  *rninfo = info.num_info;
}

static void
dyld_info_read (struct macosx_dyld_thread_status *status,
                struct dyld_objfile_info *info, int dyldonly)
{
  struct dyld_raw_info *rinfo = NULL;
  int reserved = -1;
  int nrinfo = 0;
  int i = 0;

  if (!dyldonly)
    {
      struct dyld_objfile_entry *entry = dyld_objfile_entry_alloc (info);
      reserved = entry - info->entries;
    }

  if (status->dyld_addr != INVALID_ADDRESS)
    {
      const char *dyld_name = NULL;
      struct dyld_objfile_entry *entry = NULL;

      entry = dyld_objfile_entry_alloc (info);
      dyld_name =
        (status->dyld_name != NULL) ? status->dyld_name : "/usr/lib/dyld";

      entry->dyld_name = xstrdup (dyld_name);
      entry->dyld_name_valid = 1;
      entry->prefix = dyld_symbols_prefix;

      entry->dyld_addr = status->dyld_addr;
      if (status->dyld_slide != INVALID_ADDRESS)
        entry->dyld_slide = status->dyld_slide;
      else
        entry->dyld_slide = 0;

      entry->dyld_valid = 1;
      entry->reason = dyld_reason_dyld;
    }

  if (dyldonly)
    return;

  dyld_info_read_raw (status, &rinfo, &nrinfo);

  for (i = 0; i < nrinfo; i++)
    {
      struct dyld_objfile_entry tentry;
      struct dyld_objfile_entry *pentry = NULL;
      dyld_objfile_entry_clear (&tentry);
      tentry.allocated = 1;
      tentry.reason = dyld_reason_dyld;
      if (dyld_info_process_raw
          (&tentry, rinfo[i].name, rinfo[i].modtime, rinfo[i].addr) == 1)
        {
          if ((tentry.reason & dyld_reason_executable_mask) && reserved >= 0)
            {
              pentry = &info->entries[reserved];
              reserved = -1;
            }
          else
            {
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
  dyld_update_shlibs (&status->path_info, current_info);

  if (dyld_filter_events_flag)
    {
      libraries_changed = dyld_objfile_info_compare (&saved_info, current_info);
    }
  else
    {
      libraries_changed = 1;
    }

  dyld_objfile_info_free (&saved_info);
  dyld_objfile_info_free (&previous_info);

  if (ui_out_is_mi_like_p (uiout) && libraries_changed)
    {
      struct cleanup *notify_cleanup;
      notify_cleanup =
        make_cleanup_ui_out_notify_begin_end (uiout, "shlibs-updated");
      do_cleanups (notify_cleanup);
    }

  return libraries_changed;
}

/* The inferior has gone away.  Let's keep some state around so that
   we can still respond usefully to a "info shared", but none of the
   images, or their data, are actually in memory any longer.  */

void
macosx_dyld_mourn_inferior (void)
{
  struct dyld_objfile_entry *e;
  int i;
  struct macosx_dyld_thread_status *status = &macosx_status->dyld_status;
  DYLD_ALL_OBJFILE_INFO_ENTRIES (&status->current_info, e, i)
    {
      e->dyld_addr = 0;
      e->dyld_slide = 0;
      e->dyld_length = 0;
      e->dyld_valid = 0;
      e->cfm_container = 0;
      /* This isn't really right - the executable is actually
	 dyld_reason_executable - but I don't think it'll actually cause
	 any problems.  */
      if (e->reason == dyld_reason_dyld)
	e->reason = dyld_reason_init;
      
      /* God as my witness I don't know what all these names are doing in
	 a dyld_objfile_entry.  */
      /* FIXME: xfree dyld_name if I'm not assigning it to text_name? */
      if (e->text_name == NULL && e->dyld_name != NULL)
	e->text_name = e->dyld_name;
      e->dyld_name = NULL;
    }
}

static void
macosx_dyld_update_command (char *args, int from_tty)
{
  macosx_dyld_update (0);
  macosx_init_dyld (&macosx_status->dyld_status, symfile_objfile, exec_bfd);
}

static void
map_shlib_numbers (char *args,
                   void (*function) (struct dyld_path_info *,
                                     struct dyld_objfile_entry *,
                                     struct objfile *,
                                     const char *param),
                   struct dyld_path_info *d, struct dyld_objfile_info *info)
{
  char *p, *p1, *val;
  char **argv;
  int num, match;
  struct cleanup *cleanups;

  if (args == 0)
    error_no_arg ("one or more shlib numbers");

  p = args;
  while (isspace (*p) && (*p != '\0'))
    p++;

  if (strncmp (p, "all ", 4) == 0)
    p += 4;
  else
    {
      for (;;)
        {
          while (isspace (*p) && (*p != '\0'))
            p++;
          if (!isdigit (*p))
            break;
          while ((!isspace (*p)) && (*p != '\0'))
            p++;
        }
    }
  val = p;

  argv = buildargv (val);
  if (argv == NULL)
    error ("no argument provided");
  cleanups = make_cleanup_freeargv (argv);
  if (argv[0] == NULL || argv[1] != NULL)
    error ("exactly one argument must be provided");
  gdb_assert (strlen (argv[0]) <= strlen (val));
  strcpy (val, argv[0]);

  if (*p != '\0' && p > args)
    {
      p[-1] = '\0';
    }

  p = args;

  if (strcmp (p, "all") == 0)
    {
      struct dyld_objfile_entry *e;
      unsigned int n;

      DYLD_ALL_OBJFILE_INFO_ENTRIES (info, e, n)
        (*function) (d, e, e->objfile, val);

      do_cleanups (cleanups);
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

      if (num == 0)
        {
          warning ("bad shlib number at or near '%s'", p1);
          do_cleanups (cleanups);
          return;
        }

      ret = dyld_resolve_shlib_num (info, num, &e, &o);

      if (ret < 0)
        {
          warning ("no shlib %d", num);
          do_cleanups (cleanups);
          return;
        }

      (*function) (d, e, o, val);
    }

  do_cleanups (cleanups);
}

static void
dyld_generic_command_with_helper (char *args,
                                  int from_tty,
                                  void (*function) (struct dyld_path_info *,
                                                    struct dyld_objfile_entry *,
                                                    struct objfile *,
                                                    const char *param))
{
  struct dyld_objfile_info original_info, modified_info;

  dyld_objfile_info_init (&original_info);
  dyld_objfile_info_init (&modified_info);

  dyld_objfile_info_copy (&original_info,
                          &macosx_status->dyld_status.current_info);
  dyld_objfile_info_copy (&modified_info,
                          &macosx_status->dyld_status.current_info);
  dyld_objfile_info_clear_objfiles (&modified_info);

  map_shlib_numbers (args, function, &macosx_status->dyld_status.path_info,
                     &modified_info);

  dyld_merge_shlibs (&macosx_status->dyld_status,
                     &macosx_status->dyld_status.path_info, &original_info,
                     &modified_info);
  dyld_update_shlibs (&macosx_status->dyld_status.path_info, &modified_info);

  dyld_objfile_info_copy (&macosx_status->dyld_status.current_info,
                          &modified_info);

  dyld_objfile_info_free (&original_info);
  dyld_objfile_info_free (&modified_info);
}

static void
add_helper (struct dyld_path_info *d,
            struct dyld_objfile_entry *e, struct objfile *o, const char *arg)
{
  if (e != NULL)
    e->load_flag = OBJF_SYM_ALL;
}

static void
dyld_add_symbol_file_command (char *args, int from_tty)
{
  dyld_generic_command_with_helper (args, from_tty, add_helper);
}

static void
remove_helper (struct dyld_path_info *d,
               struct dyld_objfile_entry *e,
               struct objfile *o, const char *arg)
{
  if (e != NULL)
    e->load_flag = OBJF_SYM_NONE | dyld_minimal_load_flag (d, e);
}

static void
dyld_remove_symbol_file_command (char *args, int from_tty)
{
  dyld_generic_command_with_helper (args, from_tty, remove_helper);
}

static void
specify_symfile_helper (struct dyld_path_info *d,
                        struct dyld_objfile_entry *e,
                        struct objfile *o, const char *arg)
{
  e->user_name = xstrdup (arg);
  e->loaded_error = 0;
}

static void
dyld_specify_symbol_file_command (char *args, int from_tty)
{
  dyld_generic_command_with_helper (args, from_tty, specify_symfile_helper);
}

/* objfile_set_load_state

   Given objfile O, this changes the load state to LOAD_STATE.  This
   will cause the current objfile to get tossed, and a new version
   read in.  If LOAD_STATE matches the objfile's current load state,
   this is a no-op, however.  */

int
dyld_objfile_set_load_state (struct objfile *o, int load_state)
{
  struct dyld_objfile_entry *e;
  int i, found_it = 0;

  DYLD_ALL_OBJFILE_INFO_ENTRIES (&macosx_status->dyld_status.current_info, e, i)
    if (e->objfile == o)
      {
        set_load_state_1 (e, &macosx_status->dyld_status.path_info, load_state);
        found_it = 1;
        if (ui_out_is_mi_like_p (uiout))
          {
            struct cleanup *notify_cleanup;
            notify_cleanup =
              make_cleanup_ui_out_notify_begin_end (uiout,
                                                    "shlib-state-modified");
            dyld_print_entry_info (e, i + 1, 0);
            do_cleanups (notify_cleanup);
          }
        break;
      }
  return found_it;
}

static void
set_to_default_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e,
                       struct objfile *o, const char *arg)
{
  if (e == NULL)
    return;

  e->load_flag =
    dyld_default_load_flag (d, e) | dyld_minimal_load_flag (d, e);
}

static void
dyld_apply_load_rules_command (char *args, int from_tty)
{
  dyld_generic_command_with_helper (args, from_tty, set_to_default_helper);
}

static void
set_load_state_1 (struct dyld_objfile_entry *e,
                  const struct dyld_path_info *d, int load_state)
{
  struct bfd *tmp_bfd;
  enum dyld_reload_result state_change;

  e->load_flag = load_state;

  /* If there is no existing objfile, load it (if appropriate) and return. */

  if (e->objfile == NULL)
    {
      dyld_load_library (d, e);
      if (e->abfd)
        dyld_load_symfile (e);
      return;
    }

  state_change = dyld_should_reload_objfile_for_flags (e);
  if (state_change == DYLD_NO_CHANGE)
    return;

  /* This is a bit of a hack, but I don't want to have to throw away and
     reconstitute the bfd.  So I am hiding it from dyld_remove_objfile.
     I may give up on this!  Turns out the bfd's strtab for the fake
     stabstr section is actually a pointer to the version allocated on
     the objfile's objstack!!!  So I need to null this out so it gets reset.  */
  {
    int ret;
    struct bfd_mach_o_load_command *gsymtab;

    tmp_bfd = e->objfile->obfd;
    ret = bfd_mach_o_lookup_command (tmp_bfd, BFD_MACH_O_LC_SYMTAB, &gsymtab);
    if (ret != 1)
      {
        warning
          ("Error fetching LC_SYMTAB load command from object file \"%s\"",
           tmp_bfd->filename);
      }
    else if (gsymtab->command.symtab.strtab == DBX_STRINGTAB (e->objfile))
      gsymtab->command.symtab.strtab = NULL;

    e->objfile->obfd = NULL;
  }
  tell_breakpoints_objfile_changed (e->objfile);
  tell_objc_msgsend_cacher_objfile_changed (e->objfile);

  /* FIXME: check state_change, and remove the varobj's that depend
     on the objfile now as well.  */

  dyld_remove_objfile (e);

  e->abfd = tmp_bfd;
  dyld_load_symfile (e);
}

static void
set_load_state_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e,
                       struct objfile *o, const char *arg)
{
  int load_state;

  if (e == NULL)
    return;

  load_state = dyld_parse_load_level (arg);
  load_state |= dyld_minimal_load_flag (d, e);
  set_load_state_1 (e, d, load_state);
}

static void
dyld_set_load_state_command (char *args, int from_tty)
{
  map_shlib_numbers (args, set_load_state_helper,
                     &macosx_status->dyld_status.path_info,
                     &macosx_status->dyld_status.current_info);
  /* Since we've change the load state of some libraries, we should
     see if any of our pending breakpoints will now take.  */
  re_enable_breakpoints_in_shlibs (0);

}

static void
section_info_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e,
                     struct objfile *o, const char *arg)
{
  int ret;

  if (o != NULL)
    {
      print_section_info_objfile (o);
    }
  else
    {
      ui_out_begin (uiout, ui_out_type_list, "section-info");
      ui_out_end (uiout, ui_out_type_list);
    }

  if (e != NULL)
    {
#if WITH_CFM
      if (e->cfm_container != 0)
        {

          NCFragContainerInfo container;
          struct cfm_parser *parser;
          unsigned long section_index;

          parser = &macosx_status->cfm_status.parser;

          ret =
            cfm_fetch_container_info (parser, e->cfm_container, &container);
          if (ret != 0)
            return;

          ui_out_begin (uiout, ui_out_type_list, "cfm-section-info");

          for (section_index = 0; section_index < container.sectionCount;
               section_index++)
            {
              NCFragSectionInfo section;
              NCFragInstanceInfo instance;

              ret =
                cfm_fetch_container_section_info (parser, e->cfm_container,
                                                  section_index, &section);
              if (ret != 0)
                break;

              ui_out_begin (uiout, ui_out_type_list, "section");

              ui_out_text (uiout, "\t");
              ui_out_field_core_addr (uiout, "addr", instance.address);
              ui_out_text (uiout, " - ");
              ui_out_field_core_addr (uiout, "endaddr",
                                      instance.address + section.length);
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
                  ui_out_field_string (uiout, "filename",
                                       bfd_get_filename (p->bfd));
                }
#endif
              ui_out_text (uiout, "\n");

              ui_out_end (uiout, ui_out_type_list);     /* "section" */
            }

          ui_out_end (uiout, ui_out_type_list); /* "cfm-section-info" */
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


/* The "info sharedlibrary" command is overloaded a bit much.

   "info sharedlibrary" by itself will print some help text, followed
   by all your shared libraries.

   "info sharedlibrary [all|dyld|cfm|raw-cfm]" list certain types of
   shared libraries, sans the help text.

   "info sharedlibrary <address>" show the shared library that contains
   that address.

   The gdb command system will run seperate routines for the
   "info sharedlibrary [all|dyld|cfm|raw-cfm]" case, but we need
   to handle the other two here.  */

static void
info_sharedlibrary_command (char *args, int from_tty)
{
  char **argv;
  struct cleanup *wipe;

  CHECK_FATAL (macosx_status != NULL);

  wipe = make_cleanup (null_cleanup, NULL);

  if (args != NULL)
    if ((argv = buildargv (args)) != NULL)
      make_cleanup_freeargv (argv);

  if (args == NULL || argv == NULL || argv[0] == NULL
      || !strcmp (argv[0], ""))
    {
      dyld_print_status_info (&macosx_status->dyld_status,
                              dyld_reason_all_mask | dyld_reason_user, args);
    }
  else
    {
      CORE_ADDR address;

      errno = 0;
      address = strtoul (argv[0], NULL, 16);
      if (errno == 0)
        info_sharedlibrary_address (address);
      else
        error ("[unknown]");
    }

  do_cleanups (wipe);
}

/* Given an address, find the shared library (or executable) that contains
   this address.  Akin to the old metrowerks-address-to-name command.  */

static void
info_sharedlibrary_address (CORE_ADDR address)
{
  struct dyld_objfile_info *s = &macosx_status->dyld_status.current_info;
  int shlibnum = 1;
  int found_dylib = 0;
  int baselen;
  int i;
  struct obj_section *osection;

  baselen = dyld_shlib_info_basename_length (s, dyld_reason_all_mask);
  if (baselen < 12)
    baselen = 12;

  osection = find_pc_sect_section (address, NULL);
  if (osection != NULL)
    {
      for (i = 0; i < s->nents; i++)
        {
          if (osection->objfile == s->entries[i].objfile)
            {
              found_dylib = 1;
              break;
            }
          shlibnum++;
        }
    }

  if (found_dylib)
    dyld_print_entry_info (&s->entries[i], shlibnum, baselen);
  else
    error ("[unknown]");
}

static void
info_sharedlibrary_all_command (char *args, int from_tty)
{
  CHECK_FATAL (macosx_status != NULL);
  dyld_print_shlib_info (&macosx_status->dyld_status.current_info,
                         dyld_reason_all_mask | dyld_reason_user, 1, args);
}

static void
info_sharedlibrary_dyld_command (char *args, int from_tty)
{
  CHECK_FATAL (macosx_status != NULL);
  dyld_print_shlib_info (&macosx_status->dyld_status.current_info,
                         dyld_reason_dyld, 1, args);
}

static void
info_sharedlibrary_cfm_command (char *args, int from_tty)
{
  CHECK_FATAL (macosx_status != NULL);
  dyld_print_shlib_info (&macosx_status->dyld_status.current_info,
                         dyld_reason_cfm, 1, args);
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

  if (!target_has_execution)
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

  ret_addr =
    value_as_long (call_function_by_hand
                   (lookup_cached_function (lookup_function), 1, &name_val));

  if (ret_addr != 0x0)
    return 1;
  else
    return 0;
}

#if MAPPED_SYMFILES

static void
cache_symfiles_helper (struct dyld_path_info *d,
                       struct dyld_objfile_entry *e,
                       struct objfile *o, const char *arg)
{
  struct objfile *old = NULL;
  struct objfile *new = NULL;
  bfd *abfd;

  if (e != NULL)
    {
      old = e->objfile;
      abfd = e->objfile->obfd;
      new = cache_bfd (e->objfile->obfd, e->prefix, e->load_flag, 0, 0, arg);
    }
  else
    {
      CHECK_FATAL (o != NULL);
      old = o;
      abfd = o->obfd;
      new = cache_bfd (o->obfd, o->prefix, o->symflags, 0, 0, arg);
    }

  if (new == NULL)
    {
      warning ("unable to move objfile");
      return;
    }

  if (e != NULL)
    {
      if (e->reason & dyld_reason_executable_mask)
        {
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
  error ("Cached symfiles are not supported on this configuration of GDB.");
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

  if (args == NULL)
    error_no_arg ("file to be cached and target");
  argv = buildargv (args);
  if (argv == NULL)
    nomem (0);
  cleanups = make_cleanup_freeargv (argv);
  if (argv[0] == NULL || argv[1] == NULL
      || (argv[2] != NULL && argv[3] != NULL))
    error ("usage: cache-symfile <source> <target> [prefix]");
  filename = argv[0];
  dest = argv[1];
  prefix = argv[2];

  abfd = symfile_bfd_open (filename, 0);
  if (abfd == NULL)
    error ("unable to open BFD for \"%s\"", filename);
  objfile = cache_bfd (abfd, prefix, OBJF_SYM_ALL, 0, 0, dest);
  if ((objfile != NULL) && (objfile->obfd == NULL))
    bfd_close (abfd);
  if (objfile != NULL)
    free_objfile (objfile);
  if (abfd != NULL)
    bfd_close (abfd);

#else /* ! MAPPED_SYMFILES */
  error ("Cached symfiles are not supported on this configuration of GDB.");
#endif /* MAPPED_SYMFILES */
}

extern struct target_ops exec_ops;

void
update_section_tables ()
{
  update_section_tables_dyld (&macosx_status->dyld_status.current_info);
}

void
update_section_tables_dyld (struct dyld_objfile_info *s)
{
  struct target_ops *target;
  struct objfile *o;
  struct obj_section *osection;
  int nsections, csection, osections;
  int i;
  struct dyld_objfile_entry *j;

  target = &exec_ops;

  /* Count the total # of sections. */

  nsections = 0;
  ALL_OBJFILES (o)
    ALL_OBJFILE_OSECTIONS (o, osection)
      nsections++;

  osections = target->to_sections_end - target->to_sections;
  target_resize_to_sections (target, nsections - osections);
  gdb_assert ((target->to_sections + nsections) == target->to_sections_end);

#define ADD_SECTION(osection) \
        { \
        gdb_assert (osection != NULL); \
        gdb_assert (osection->objfile != NULL); \
        target->to_sections[csection].addr = osection->addr; \
        target->to_sections[csection].endaddr = osection->endaddr; \
        target->to_sections[csection].the_bfd_section = osection->the_bfd_section; \
        target->to_sections[csection].bfd = osection->objfile->obfd; \
        csection++; \
        }

  csection = 0;

  /* First, add all the shared libraries brought in from the
     dyld_objfile_entry code, in the order that they are printed by
     dyld_print_shlib_info (that way, the exec file will always go
     first, among other things).  When there are overlapping sections,
     GDB just uses the first one. */

  DYLD_ALL_OBJFILE_INFO_ENTRIES (s, j, i)
    {
      if (j->objfile != NULL)
        ALL_OBJFILE_OSECTIONS (j->objfile, osection)
          ADD_SECTION (osection);

      if (j->commpage_objfile != NULL)
        ALL_OBJFILE_OSECTIONS (j->commpage_objfile, osection)
          ADD_SECTION (osection);
    }

  /* Then, go through and match all the objfiles not managed by the
     dyld_objfile_entry code ... again, matching the order used by
     dyld_print_shlib_info. */

  ALL_OBJFILES (o)
    {
      int found = 0;

      DYLD_ALL_OBJFILE_INFO_ENTRIES (s, j, i)
        {
          if (j->objfile == o || j->commpage_objfile == o)
            found = 1;
        }

      if (!found)
        ALL_OBJFILE_OSECTIONS (o, osection)
          ADD_SECTION (osection);
    }

#undef ADD_SECTION

  gdb_assert (csection == nsections);
}

/* FIXME: This is used in the test to determine
   whether to re-enable a shlib breakpoint in
   re_enable_breakpoints_in_shlibs.  I had to
   disable that test.  Re-enable it when this
   function works.  */

const char *
macosx_pc_solib (CORE_ADDR addr)
{
  return NULL;
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
  printf_unfiltered
    ("\"maintenance sharedlibrary\" must be followed by the name of a sharedlibrary command.\n");
  help_list (maintenanceshliblist, "maintenance sharedlibrary ", -1,
             gdb_stdout);
}

static void
sharedlibrary_command (char *arg, int from_tty)
{
  printf_unfiltered
    ("\"sharedlibrary\" must be followed by the name of a sharedlibrary command.\n");
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

  add_prefix_cmd ("sharedlibrary", class_maintenance,
                  maintenance_sharedlibrary_command,
                  "Commands for internal sharedlibrary manipulation.",
                  &maintenanceshliblist, "maintenance sharedlibrary ", 0,
                  &maintenancelist);

  add_cmd ("apply-load-rules", class_run, dyld_apply_load_rules_command,
           "Apply the current load-rules to the existing shared library state.",
           &shliblist);

  add_cmd ("add-symbol-file", class_run, dyld_add_symbol_file_command,
           "Add a symbol file.", &shliblist);

  add_cmd ("remove-symbol-file", class_run, dyld_remove_symbol_file_command,
           "Remove a symbol file.", &shliblist);

  add_cmd ("specify-symbol-file", class_run, dyld_specify_symbol_file_command,
           "Specify the symbol file for a sharedlibrary entry.", &shliblist);

  add_cmd ("set-load-state", class_run, dyld_set_load_state_command,
           "Set the load level of a library (given by the index from \"info sharedlibrary\").",
           &shliblist);

  add_cmd ("section-info", class_run, dyld_section_info_command,
           "Get the section info for a library (given by the index from \"info sharedlibrary\").",
           &shliblist);

  add_cmd ("cache-purge", class_obscure, dyld_cache_purge_command,
           "Purge all symbols for DYLD images cached by GDB.",
           &maintenanceshliblist);

  add_cmd ("update", class_run, macosx_dyld_update_command,
           "Process all pending DYLD events.", &shliblist);

  add_prefix_cmd ("sharedlibrary", no_class, info_sharedlibrary_command,
                  "Generic command for shlib information.\n`info sharedlibrary ADDRESS' will show the dylib containing ADDRESS.",
                  &infoshliblist, "info sharedlibrary ", 1, &infolist);

  add_cmd ("all", no_class, info_sharedlibrary_all_command,
           "Show current DYLD state.", &infoshliblist);
  add_cmd ("dyld", no_class, info_sharedlibrary_dyld_command,
           "Show current DYLD state.", &infoshliblist);
  add_cmd ("cfm", no_class, info_sharedlibrary_cfm_command,
           "Show current CFM state.", &infoshliblist);
  add_cmd ("raw-cfm", no_class, info_sharedlibrary_raw_cfm_command,
           "Show current CFM state.", &infoshliblist);

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
    set sharedlibrary load-rules dyld ^/System/Library.* extern\n", &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("minimal-load-rules", class_support, var_string,
                     (char *) &dyld_minimal_load_rules,
                     "Set the minimal DYLD load rules.  These prime the main list.\n\
gdb relies on some of these for proper functioning, so don't remove elements from it\n\
unless you know what you are doing.", &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("dyld", class_obscure, var_zinteger,
                     (char *) &dyld_debug_flag,
                     "Set if printing dyld communication debugging statements.",
                     &setdebuglist);
  add_show_from_set (cmd, &showdebuglist);

  dyld_minimal_load_rules =
    xstrdup
    ("\"dyld\" \"CarbonCore$\\\\|CarbonCore_[^/]*$\" all \".*\" \"dyld$\" extern \".*\" \".*\" none");
  dyld_load_rules = xstrdup ("\".*\" \".*\" all");

  cmd = add_set_cmd ("stop-on-shlibs-added", class_support, var_zinteger,
                     (char *) &dyld_stop_on_shlibs_added,
                     "Set if a shlib event should be reported on a shlibs-added event.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("stop-on-shlibs-updated", class_support, var_zinteger,
                     (char *) &dyld_stop_on_shlibs_updated,
                     "Set if a shlib event should be reported on a shlibs-updated event.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("combine-shlibs-added", class_support, var_zinteger,
                     (char *) &dyld_combine_shlibs_added,
                     "Set if GDB should combine shlibs-added events from the same image into a single event.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  cmd = add_set_cmd ("reload-on-downgrade", class_support, var_zinteger,
                     (char *) &dyld_reload_on_downgrade_flag,
                     "Set if GDB should re-read symbol files in order to remove symbol information.",
                     &setshliblist);
  add_show_from_set (cmd, &showshliblist);

  add_cmd ("cache-symfiles", class_run, dyld_cache_symfiles_command,
           "Generate persistent caches of symbol files for the current executable state.",
           &shliblist);

  add_cmd ("cache-symfile", class_run, dyld_cache_symfile_command,
           "Generate persistent caches of symbol files for a specified executable.\n"
           "usage: cache-symfile <source> <target> [prefix]", &shliblist);
}
