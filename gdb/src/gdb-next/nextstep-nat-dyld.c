#include "nextstep-nat-dyld.h"
#include "nextstep-nat-dyld-path.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-inferior-debug.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-dyld.h"
#include "nextstep-nat-dyld-info.h"
#include "nextstep-nat-dyld-path.h"
#include "nextstep-nat-dyld-process.h"

#if WITH_CFM
#include "nextstep-nat-cfm.h"
#endif /* WITH_CFM */

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcmd.h"
#include "annotate.h"
#include "mach-o.h"
#include "gdbcore.h"		/* for core_ops */
#include "symfile.h"
#include "objfiles.h"

#include <unistd.h>

FILE *dyld_stderr = NULL;
int dyld_debug_flag = 0;

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

bfd *sym_bfd;

extern struct target_ops exec_ops;

extern next_inferior_status *next_status;

static int dyld_preload_libraries_flag = 1;

void next_dyld_update ();

const char *dyld_debug_error_string (enum dyld_debug_return ret)
{
  switch (ret) {
  case DYLD_SUCCESS: return "DYLD_SUCCESS";
  case DYLD_INCONSISTENT_DATA: return "DYLD_INCONSISTENT_DATA";
  case DYLD_INVALID_ARGUMENTS: return "DYLD_INVALID_ARGUMENTS";
  case DYLD_FAILURE: return "DYLD_FAILURE";
  default: return "[UNKNOWN]";
  }  
}

const char *dyld_debug_event_string (enum dyld_event_type type)
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

static void debug_dyld_event_request (FILE *f, struct _dyld_event_message_request *request)
{
  next_debug_message (&request->head);
  fprintf (f, "               type: %s (0x%lx)\n", 
	      dyld_debug_event_string (request->event.type),
	      (unsigned long) request->event.type);
  fprintf (f, "arg[0].vmaddr_slide: 0x%lx\n", (unsigned long) request->event.arg[0].vmaddr_slide);
  fprintf (f, "arg[0].module_index: 0x%lx\n", (unsigned long) request->event.arg[0].module_index);
  fprintf (f, "arg[1].vmaddr_slide: 0x%lx\n", (unsigned long) request->event.arg[1].vmaddr_slide);
  fprintf (f, "arg[1].module_index: 0x%lx\n", (unsigned long) request->event.arg[1].module_index);
}

void dyld_print_status_info (struct next_dyld_thread_status *s) 
{
  switch (s->state) {
  case dyld_clear:
    printf_filtered ("The DYLD shared library state has not yet been initialized.\n"); 
    break;
  case dyld_initialized:
    printf_filtered 
      ("The DYLD shared library state has been initialized from the "
       "executable's shared library information.  All symbols should be "
       "present, but the addresses of some symbols may move when the program "
       "is executed, as DYLD may relocate library load addresses if "
       "necessary.\n");
    break;
  case dyld_started:
    printf_filtered ("DYLD shared library information has been read from the DYLD debugging thread.\n");
    break;
  default:
    internal_error ("invalid value for s->dyld_state");
    break;
  }

  dyld_print_shlib_info (&s->current_info);
}

void next_clear_start_breakpoint ()
{
  remove_solib_event_breakpoints ();
}

static CORE_ADDR lookup_address (const char *s)
{
  struct minimal_symbol *msym;
  msym = lookup_minimal_symbol (s, NULL, NULL);
  if (msym == NULL) {
    error ("unable to locate symbol \"%s\"", s);
  }
  return SYMBOL_VALUE_ADDRESS (msym);
}

static unsigned int lookup_value (const char *s)
{
  return read_memory_unsigned_integer (lookup_address (s), 4);
}

void next_init_addresses ()
{
  next_status->dyld_status.object_images = lookup_address ("_dyld__object_images");
  next_status->dyld_status.library_images = lookup_address ("_dyld__library_images");
  next_status->dyld_status.state_changed_hook = lookup_address ("_dyld__gdb_dyld_state_changed");

  next_status->dyld_status.dyld_version = lookup_value ("_dyld__gdb_dyld_version");

  next_status->dyld_status.nobject_images = lookup_value ("_dyld__gdb_nobject_images");
  next_status->dyld_status.nlibrary_images = lookup_value ("_dyld__gdb_nlibrary_images");
  next_status->dyld_status.object_image_size = lookup_value ("_dyld__gdb_object_image_size");
  next_status->dyld_status.library_image_size = lookup_value ("_dyld__gdb_library_image_size");
}

void next_set_start_breakpoint (bfd *exec_bfd)
{
  struct breakpoint *b;
  struct minimal_symbol *msym;
  struct symtab_and_line sal;

  next_init_addresses ();

  INIT_SAL (&sal);
  sal.pc = next_status->dyld_status.state_changed_hook;
  b = set_momentary_breakpoint (sal, NULL, bp_shlib_event);
  b->disposition = donttouch;
  b->thread = -1;
  b->addr_string = strsave ("_dyld__gdb_dyld_state_changed");

  breakpoints_changed ();
}

static void info_dyld_command (args, from_tty)
     char *args;
     int from_tty;
{
  CHECK_FATAL (next_status != NULL);
  dyld_print_status_info (&next_status->dyld_status);
}

void next_mach_try_start_dyld ()
{
  CHECK_FATAL (next_status != NULL);
  next_dyld_update (0);
}

void next_mach_add_shared_symbol_files ()
{
  struct dyld_objfile_info *result = NULL;
  
  CHECK_FATAL (next_status != NULL);
  result = &next_status->dyld_status.current_info;

  dyld_load_libraries (&next_status->dyld_status.path_info, result);

  update_section_tables (&current_target);
  update_section_tables (&exec_ops);

  reread_symbols ();
  breakpoint_re_set ();
  re_enable_breakpoints_in_shlibs (0);
}

void next_init_dyld (struct next_dyld_thread_status *s, bfd *sym_bfd)
{
  struct dyld_objfile_info previous_info, new_info;

  dyld_init_paths (&s->path_info);

  dyld_objfile_info_init (&previous_info);
  dyld_objfile_info_init (&new_info);

  dyld_objfile_info_copy (&previous_info, &s->current_info);
  dyld_objfile_info_free (&s->current_info);

  if (dyld_preload_libraries_flag) {
    dyld_add_inserted_libraries (&s->current_info, &s->path_info);
    if (sym_bfd != NULL) {
      dyld_add_image_libraries (&s->current_info, sym_bfd);
    }
  }

  dyld_update_shlibs (s, &s->path_info, &previous_info, &s->current_info, &new_info);
  dyld_objfile_info_free (&previous_info);
  
  dyld_objfile_info_copy (&s->current_info, &new_info);
  dyld_objfile_info_free (&new_info);

  s->state = dyld_initialized;
}

void next_init_dyld_symfile (bfd *sym_bfd)
{
  CHECK_FATAL (next_status != NULL);
  next_init_dyld (&next_status->dyld_status, sym_bfd);
}

static void next_dyld_init_command (char *args, int from_tty)
{
  CHECK_FATAL (next_status != NULL);
  next_init_dyld (&next_status->dyld_status, sym_bfd);
}

static void dyld_cache_purge_command (char *exp, int from_tty)
{
  CHECK_FATAL (next_status != NULL);
  dyld_purge_cached_libraries (&next_status->dyld_status.current_info);
}

static void dyld_info_process_raw
(struct dyld_objfile_info *info, unsigned char *buf)
{
  CORE_ADDR name, header, slide;

  struct mach_header headerbuf;
  char *namebuf;

  struct dyld_objfile_entry *entry;
  int errno;

  name = extract_unsigned_integer (buf, 4);
  slide = extract_unsigned_integer (buf + 4, 4);
  header = extract_unsigned_integer (buf + 8, 4);

  target_read_memory (header, (char *) &headerbuf, sizeof (struct mach_header));
  target_read_string (name, &namebuf, 1024, &errno);

  switch (headerbuf.filetype) {
  case 0:
    return;
  case MH_EXECUTE:
  case MH_DYLIB:
  case MH_DYLINKER:
  case MH_BUNDLE:
    break;
  case MH_FVMLIB:
  case MH_PRELOAD:
    return;
  default:
    warning ("Ignored unknown object module at 0x%lx (offset 0x%lx) with type 0x%lx\n",
	     (unsigned long) header, (unsigned long) slide, (unsigned long) headerbuf.filetype);
    return;
  }

  entry = dyld_objfile_entry_alloc (info);

  entry->dyld_name = xstrdup (namebuf);
  entry->dyld_name_valid = 1;

  entry->dyld_addr = header;
  entry->dyld_slide = slide;
  entry->dyld_index = header;
  entry->dyld_valid = 1;

  entry->load_flag = 1;

  switch (headerbuf.filetype) {
  case MH_EXECUTE:
    entry->reason = dyld_reason_executable;
    break;
  case MH_DYLIB:
    entry->reason = dyld_reason_dyld;
    break;
  case MH_DYLINKER:
  case MH_BUNDLE:
    entry->reason = dyld_reason_dyld;
    break;
  default:
    internal_error ("Unknown object module at 0x%lx (offset 0x%lx) with type 0x%lx\n",
		    (unsigned long) header, (unsigned long) slide, (unsigned long) headerbuf.filetype);
  }
}

static void dyld_info_read_raw
(struct next_dyld_thread_status *status, struct dyld_objfile_info *info, int dyldonly)
{
  CORE_ADDR library_images_addr;
  CORE_ADDR object_images_addr;
  unsigned int i, nread, nimages;

  {
    struct dyld_objfile_entry *entry;

    entry = dyld_objfile_entry_alloc (info);

    entry->dyld_name = xstrdup ("/usr/lib/dyld");
    entry->dyld_name_valid = 1;
    entry->prefix = "__dyld_";

    entry->dyld_addr = 0x41100000;
    entry->dyld_slide = 0;
    entry->dyld_index = 0;
    entry->dyld_valid = 1;

    entry->load_flag = 1;
    entry->reason = dyld_reason_dyld;
  }

  if (dyldonly) {
    return;
  }

  next_init_addresses ();

  library_images_addr = status->library_images;

  while (library_images_addr != NULL) {

    size_t size = status->nlibrary_images * status->library_image_size;
    unsigned char *buf = NULL;
    size_t nimages = 0;

    buf = xmalloc (size + 8);
    target_read_memory (library_images_addr, buf, size + 8);

    nimages = extract_unsigned_integer (buf + size, 4);
    library_images_addr = extract_unsigned_integer (buf + size + 4, 4);
    
    if (nimages > status->nlibrary_images) {
      error ("image specifies an invalid number of libraries (%d)", nimages);
    }

    for (i = 0, nread = 0; i < nimages; i++, nread++) {
      dyld_info_process_raw (info, (buf + (i * status->library_image_size)));
    }

    xfree (buf);
  }

  object_images_addr = status->object_images;

  while (object_images_addr != NULL) {

    size_t size = status->nobject_images * status->object_image_size;
    unsigned char *buf = NULL;
    size_t nimages = 0;

    buf = xmalloc (size + 8);
    target_read_memory (object_images_addr, buf, size + 8);

    nimages = extract_unsigned_integer (buf + size, 4);
    object_images_addr = extract_unsigned_integer (buf + size + 4, 4);
    
    if (nimages > status->nobject_images) {
      error ("image specifies an invalid number of objects (%d)", nimages);
    }

    for (i = 0, nread = 0; i < nimages; i++, nread++) {
      dyld_info_process_raw (info, (buf + (i * status->object_image_size)));
    }

    xfree (buf);
  }
}

void next_dyld_update (int dyldonly)
{
  struct dyld_objfile_info previous_info, new_info;

  CHECK_FATAL (next_status != NULL);

  dyld_objfile_info_init (&previous_info);
  dyld_objfile_info_init (&new_info);

  dyld_objfile_info_copy (&previous_info, &next_status->dyld_status.current_info);

  dyld_objfile_info_free (&next_status->dyld_status.current_info);
  dyld_objfile_info_init (&next_status->dyld_status.current_info);
  dyld_info_read_raw (&next_status->dyld_status, &next_status->dyld_status.current_info, dyldonly);

  dyld_update_shlibs (&next_status->dyld_status, &next_status->dyld_status.path_info,
		      &previous_info, &next_status->dyld_status.current_info, &new_info);
  dyld_objfile_info_free (&previous_info);
  dyld_objfile_info_copy (&next_status->dyld_status.current_info, &new_info);
  dyld_objfile_info_free (&new_info);
}

static void next_dyld_update_command (char *args, int from_tty)
{
  next_dyld_update (0);
}

void
_initialize_nextstep_nat_dyld ()
{
  struct cmd_list_element *cmd;
  int ret;

  dyld_stderr = fdopen (fileno (stderr), "w+");

  add_com ("dyld-init", class_run, next_dyld_init_command,
	   "Init DYLD libraries to initial guesses.");

  cmd = add_set_cmd ("dyld-preload-libraries", class_obscure, var_boolean, 
		     (char *) &dyld_preload_libraries_flag,
		     "Set if GDB should pre-load symbols for DYLD libraries.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		

  add_info ("dyld", info_dyld_command,
	    "Show current DYLD state.");

  add_info ("sharedlibrary", info_dyld_command,
	    "Show current DYLD state.");

  add_com ("dyld-cache-purge", class_obscure, dyld_cache_purge_command,
	   "Purge all symbols for DYLD images cached by GDB.");

  add_com ("dyld-update", class_run, next_dyld_update_command,
	   "Process all pending DYLD events.");

  cmd = add_set_cmd ("debug-dyld", class_obscure, var_zinteger, 
		     (char *) &dyld_debug_flag,
		     "Set if printing dyld communication debugging statements.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		
}
