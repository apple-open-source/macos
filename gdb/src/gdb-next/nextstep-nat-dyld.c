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
#include "gdbcore.h"                /* for core_ops */
#include "symfile.h"
#include "objfiles.h"

#include <unistd.h>
#include <ctype.h>

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

int next_dyld_update ();

const char
*dyld_debug_error_string (enum dyld_debug_return ret)
{
  switch (ret) {
  case DYLD_SUCCESS: return "DYLD_SUCCESS";
  case DYLD_INCONSISTENT_DATA: return "DYLD_INCONSISTENT_DATA";
  case DYLD_INVALID_ARGUMENTS: return "DYLD_INVALID_ARGUMENTS";
  case DYLD_FAILURE: return "DYLD_FAILURE";
  default: return "[UNKNOWN]";
  }  
}

const char
*dyld_debug_event_string (enum dyld_event_type type)
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

static void
debug_dyld_event_request (FILE *f, struct _dyld_event_message_request *request)
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

void
dyld_print_status_info (struct next_dyld_thread_status *s, unsigned int mask)
{
  switch (s->state) {
  case dyld_clear:
    ui_out_text (uiout, "The DYLD shared library state has not yet been initialized.\n");
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
    ui_out_text (uiout, "DYLD shared library information has been read from the DYLD debugging thread.\n");
    break;
  default:
    internal_error ("invalid value for s->dyld_state");
    break;
  }

  dyld_print_shlib_info (&s->current_info, mask);
}

void
next_clear_start_breakpoint ()
{
  remove_solib_event_breakpoints ();
}

extern char *dyld_symbols_prefix;

static
CORE_ADDR lookup_dyld_address (const char *s)
{
  struct minimal_symbol *msym = NULL;
  char *ns = NULL;

  asprintf (&ns, "%s%s", dyld_symbols_prefix, s);
  msym = lookup_minimal_symbol (ns, NULL, NULL);
  xfree (ns);

  if (msym == NULL) {
    error ("unable to locate symbol \"%s%s\"", dyld_symbols_prefix, s);
  }
  return SYMBOL_VALUE_ADDRESS (msym);
}

static
unsigned int lookup_dyld_value (const char *s)
{
  return read_memory_unsigned_integer (lookup_dyld_address (s), 4);
}

void
next_init_addresses ()
{
  next_status->dyld_status.object_images = lookup_dyld_address ("object_images");
  next_status->dyld_status.library_images = lookup_dyld_address ("library_images");
  next_status->dyld_status.state_changed_hook = lookup_dyld_address ("gdb_dyld_state_changed");

  next_status->dyld_status.dyld_version = lookup_dyld_value ("gdb_dyld_version");

  next_status->dyld_status.nobject_images = lookup_dyld_value ("gdb_nobject_images");
  next_status->dyld_status.nlibrary_images = lookup_dyld_value ("gdb_nlibrary_images");
  next_status->dyld_status.object_image_size = lookup_dyld_value ("gdb_object_image_size");
  next_status->dyld_status.library_image_size = lookup_dyld_value ("gdb_library_image_size");
}

void
next_set_start_breakpoint (bfd *exec_bfd)
{
  struct breakpoint *b;
  struct symtab_and_line sal;

  char *ns = NULL;
  asprintf (&ns, "%s%s", dyld_symbols_prefix, "gdb_dyld_state_changed");
 
  next_init_addresses ();

  INIT_SAL (&sal);
  sal.pc = next_status->dyld_status.state_changed_hook;
  b = set_momentary_breakpoint (sal, NULL, bp_shlib_event);
  b->disposition = donttouch;
  b->thread = -1;
  b->addr_string = ns;

  breakpoints_changed ();
}

int
next_mach_try_start_dyld ()
{
  CHECK_FATAL (next_status != NULL);
  return next_dyld_update (0);

  fprintf (stderr, "stopped at 0x%lx\n", read_pc ());
}

static
void info_dyld_command (char *args, int from_tty)
{
  CHECK_FATAL (next_status != NULL);
  dyld_print_status_info (&next_status->dyld_status, dyld_reason_dyld);
}

static
void info_sharedlibrary_command (char *args, int from_tty)
{
  CHECK_FATAL (next_status != NULL);
  dyld_print_status_info (&next_status->dyld_status, dyld_reason_all);
}

void
next_mach_add_shared_symbol_files ()
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

void
next_init_dyld (struct next_dyld_thread_status *s, struct objfile *o)
{
  struct dyld_objfile_info previous_info, new_info;

  dyld_init_paths (&s->path_info);

  dyld_objfile_info_init (&previous_info);
  dyld_objfile_info_init (&new_info);

  dyld_objfile_info_copy (&previous_info, &s->current_info);
  dyld_objfile_info_free (&s->current_info);

  if (dyld_preload_libraries_flag) {
    dyld_add_inserted_libraries (&s->current_info, &s->path_info);
    if ((o != NULL) && (o->obfd != NULL)) {
      dyld_add_image_libraries (&s->current_info, o->obfd);
    }
  }

  {
    struct dyld_objfile_entry *e;
    e = dyld_objfile_entry_alloc (&s->current_info);
    e->text_name_valid = 1;
    e->reason = dyld_reason_executable;
    if (o  != NULL)
      {
        e->objfile = o;
        e->load_flag = o->symflags;
        if (o->obfd != NULL)
          {
            e->text_name = xstrdup (bfd_get_filename (o->obfd));
            e->abfd = o->obfd;
          }
      }
    e->loaded_from_memory = 0;
    e->loaded_name = e->text_name;
    e->loaded_addr = 0;
    e->loaded_addrisoffset = 1;
  }

  dyld_update_shlibs (s, &s->path_info, &previous_info, &s->current_info, &new_info);
  dyld_objfile_info_free (&previous_info);
 
  dyld_objfile_info_copy (&s->current_info, &new_info);
  dyld_objfile_info_free (&new_info);

  s->state = dyld_initialized;
}

void
next_init_dyld_symfile (struct objfile *o)
{
  CHECK_FATAL (next_status != NULL);
  next_init_dyld (&next_status->dyld_status, o);
}

static
void next_dyld_init_command (char *args, int from_tty)
{
  CHECK_FATAL (next_status != NULL);
  next_init_dyld (&next_status->dyld_status, symfile_objfile);
}

static
void dyld_cache_purge_command (char *exp, int from_tty)
{
  CHECK_FATAL (next_status != NULL);
  dyld_purge_cached_libraries (&next_status->dyld_status.current_info);
}

static
void dyld_info_process_raw
(struct dyld_objfile_info *info, unsigned char *buf)
{
  CORE_ADDR name, header, slide;

  struct mach_header headerbuf;
  char *namebuf = NULL;

  struct dyld_objfile_entry *entry;
  int errno;

  name = extract_unsigned_integer (buf, 4);
  slide = extract_unsigned_integer (buf + 4, 4);
  header = extract_unsigned_integer (buf + 8, 4);

  target_read_memory (header, (char *) &headerbuf, sizeof (struct mach_header));

  switch (headerbuf.filetype) {
  case 0:
    return;
  case MH_EXECUTE:
    target_read_string (name, &namebuf, 1024, &errno);
    break;
  case MH_DYLIB:
  case MH_DYLINKER:
  case MH_BUNDLE:
    target_read_string (name, &namebuf, 1024, &errno);
    break;
  case MH_FVMLIB:
  case MH_PRELOAD:
    target_read_string (name, &namebuf, 1024, &errno);
    return;
  default:
    warning ("Ignored unknown object module at 0x%lx (offset 0x%lx) with type 0x%lx\n",
             (unsigned long) header, (unsigned long) slide, (unsigned long) headerbuf.filetype);
    return;
  }

  entry = dyld_objfile_entry_alloc (info);

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
    internal_error ("Unknown object module at 0x%lx (offset 0x%lx) with type 0x%lx\n",
                    (unsigned long) header, (unsigned long) slide, (unsigned long) headerbuf.filetype);
  }
}

static
void dyld_info_read_raw
(struct next_dyld_thread_status *status, struct dyld_objfile_info *info, int dyldonly)
{
  CORE_ADDR library_images_addr;
  CORE_ADDR object_images_addr;
  unsigned int i, nread;

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

int
next_dyld_update (int dyldonly)
{
  int ret;
  int libraries_changed;

  struct dyld_objfile_info previous_info, new_info, saved_info;

  CHECK_FATAL (next_status != NULL);

  dyld_objfile_info_init (&previous_info);
  dyld_objfile_info_init (&new_info);

  dyld_objfile_info_copy (&previous_info, &next_status->dyld_status.current_info);
  dyld_objfile_info_copy (&saved_info, &previous_info);

  dyld_objfile_info_free (&next_status->dyld_status.current_info);
  dyld_objfile_info_init (&next_status->dyld_status.current_info);

  dyld_info_read_raw (&next_status->dyld_status, &next_status->dyld_status.current_info, dyldonly);
  ret = cfm_update (next_status->task, &next_status->dyld_status.current_info);
  dyld_update_shlibs (&next_status->dyld_status, &next_status->dyld_status.path_info,
                      &previous_info, &next_status->dyld_status.current_info, &new_info);

  if (dyld_filter_events_flag) {
    libraries_changed = dyld_objfile_info_compare (&saved_info, &new_info);
  } else {
    libraries_changed = 1;
  }

  dyld_objfile_info_free (&saved_info);
  dyld_objfile_info_free (&previous_info);
  dyld_objfile_info_copy (&next_status->dyld_status.current_info, &new_info);
  dyld_objfile_info_free (&new_info);

  return libraries_changed;
}

static
void next_dyld_update_command (char *args, int from_tty)
{
  next_dyld_update (0);
}

extern int dyld_resolve_shlib_num
(struct dyld_objfile_info *s, unsigned int num, struct dyld_objfile_entry **eptr, struct objfile **optr);

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
  for (;;) {
    while (isspace (*p) && (*p != '\0'))
      p++;
    if (! isdigit (*p))
      break;
    while ((!isspace (*p)) && (*p != '\0'))
      p++;
  }
  val = p;
  if ((*p != '\0') && (p > args)) {
    p[-1] = '\0';
  }

  p = args;
  while (*p)
    {
      struct dyld_objfile_entry *e;
      struct objfile *o;
      int ret;

      match = 0;
      p1 = p;

      num = get_number_or_range (&p1);
      if (num == 0)
	warning ("bad shlib number at or near '%s'", p);

      ret = dyld_resolve_shlib_num (info, num, &e, &o);

      if (ret < 0)
	warning ("no shlib %d", num);

      (* function) (d, e, o, val);
      
      p = p1;
    }
}

static void
add_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e, struct objfile *o, const char *arg)
{
  if (e != NULL)
    e->load_flag = OBJF_SYM_ALL;
}

static
void dyld_add_symbol_file_command (char *args, int from_tty)
{
  struct dyld_objfile_info original_info, modified_info, new_info;

  dyld_objfile_info_init (&original_info);
  dyld_objfile_info_init (&modified_info);
  dyld_objfile_info_init (&new_info);

  dyld_objfile_info_copy (&original_info, &next_status->dyld_status.current_info);
  dyld_objfile_info_copy (&modified_info, &next_status->dyld_status.current_info);

  map_shlib_numbers (args, add_helper, &next_status->dyld_status.path_info, &modified_info);

  dyld_update_shlibs (&next_status->dyld_status, &next_status->dyld_status.path_info,
                      &original_info, &modified_info, &new_info);
  
  dyld_objfile_info_copy (&next_status->dyld_status.current_info, &new_info);

  dyld_objfile_info_free (&original_info);
  dyld_objfile_info_free (&modified_info);
  dyld_objfile_info_free (&new_info);
}

static
void remove_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e, struct objfile *o, const char *arg)
{
  if (e != NULL)
    e->load_flag = OBJF_SYM_NONE | dyld_minimal_load_flag (d, e);
}

static
void dyld_remove_symbol_file_command (char *args, int from_tty)
{
  struct dyld_objfile_info original_info, modified_info, new_info;

  dyld_objfile_info_init (&original_info);
  dyld_objfile_info_init (&modified_info);
  dyld_objfile_info_init (&new_info);

  dyld_objfile_info_copy (&original_info, &next_status->dyld_status.current_info);
  dyld_objfile_info_copy (&modified_info, &next_status->dyld_status.current_info);

  map_shlib_numbers (args, remove_helper, &next_status->dyld_status.path_info, &modified_info);

  dyld_update_shlibs (&next_status->dyld_status, &next_status->dyld_status.path_info,
                      &original_info, &modified_info, &new_info);
  
  dyld_objfile_info_copy (&next_status->dyld_status.current_info, &new_info);

  dyld_objfile_info_free (&original_info);
  dyld_objfile_info_free (&modified_info);
  dyld_objfile_info_free (&new_info);
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
  struct dyld_objfile_info original_info, modified_info, new_info;

  dyld_objfile_info_init (&original_info);
  dyld_objfile_info_init (&modified_info);
  dyld_objfile_info_init (&new_info);

  dyld_objfile_info_copy (&original_info, &next_status->dyld_status.current_info);
  dyld_objfile_info_copy (&modified_info, &next_status->dyld_status.current_info);

  map_shlib_numbers (args, set_load_state_helper, &next_status->dyld_status.path_info, &modified_info);

  dyld_update_shlibs (&next_status->dyld_status, &next_status->dyld_status.path_info,
                      &original_info, &modified_info, &new_info);
  
  dyld_objfile_info_copy (&next_status->dyld_status.current_info, &new_info);

  dyld_objfile_info_free (&original_info);
  dyld_objfile_info_free (&modified_info);
  dyld_objfile_info_free (&new_info);
}
        
static void
section_info_helper (struct dyld_path_info *d, struct dyld_objfile_entry *e, struct objfile *o, const char *arg)
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
    if (e->cfm_connection != 0) {

      NCFragConnectionInfo connection;
      NCFragContainerInfo container;
      struct cfm_parser *parser;
      unsigned long section_index;
      
      parser = &next_status->cfm_status.parser;

      ret = cfm_fetch_connection_info (parser, e->cfm_connection, &connection);
      if (ret != 0)
        return;

      ret = cfm_fetch_container_info (parser, connection.container, &container);
      if (ret != 0)
        return;

      ui_out_list_begin (uiout, "cfm-section-info");

      for (section_index = 0; section_index < container.sectionCount; section_index++)
        {
          NCFragSectionInfo section;
          NCFragInstanceInfo instance;

          ret = cfm_fetch_connection_section_info (parser, e->cfm_connection, section_index, &section, &instance);
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
  map_shlib_numbers (args, section_info_helper, &next_status->dyld_status.path_info, &next_status->dyld_status.current_info);
}
        

static void
info_cfm_command (char *args, int from_tty)
{
  CHECK_FATAL (next_status != NULL);
  dyld_print_status_info (&next_status->dyld_status, dyld_reason_cfm);
}

static void
info_raw_cfm_command (char *args, int from_tty)
{
  task_t task = next_status->task;
  struct dyld_objfile_info info;

  dyld_objfile_info_init (&info);
  cfm_update (task, &info);

  dyld_print_shlib_info (&info, dyld_reason_cfm);
}

#if 0
static void
set_shlib (char *arg, int from_tty)
{
  printf_unfiltered (
     "\"set shlib\" must be followed by the name of a shlib subcommand.\n");
  help_list (setshliblist, "set shlib ", -1, gdb_stdout);
}

static void
show_shlib (char *args, int from_tty)
{
  cmd_show_list (showshliblist, from_tty, "");
}
#endif

struct cmd_list_element *dyldlist = NULL;
struct cmd_list_element *setshliblist = NULL;
struct cmd_list_element *showshliblist = NULL;
struct cmd_list_element *infoshliblist = NULL;
struct cmd_list_element *shliblist = NULL;

void
_initialize_nextstep_nat_dyld ()
{
  struct cmd_list_element *cmd = NULL;

  dyld_stderr = fdopen (fileno (stderr), "w+");

  add_prefix_cmd ("sharedlibrary", class_run, not_just_help_class_command,
                  "Command prefix for shared library manipulation",
                  &shliblist, "sharedlibrary ", 0, &cmdlist);

  add_cmd ("init", class_run, next_dyld_init_command,
           "Init DYLD libraries to initial guesses.", &shliblist);

  add_cmd ("add-symbol-file", class_run, dyld_add_symbol_file_command,
           "Add a symbol file.", &shliblist);

  add_cmd ("remove-symbol-file", class_run, dyld_remove_symbol_file_command,
           "Remove a symbol file.", &shliblist);

  add_cmd ("set-load-state", class_run, dyld_set_load_state_command,
           "Set the load level of a library (given by the index from \"info sharedlibrary\").", &shliblist);

  add_cmd ("section-info", class_run, dyld_section_info_command,
           "Get the section info for a library (given by index).", &shliblist);

  add_cmd ("cache-purge", class_obscure, dyld_cache_purge_command,
           "Purge all symbols for DYLD images cached by GDB.", &shliblist);

  add_cmd ("update", class_run, next_dyld_update_command,
           "Process all pending DYLD events.", &shliblist);

  add_prefix_cmd ("sharedlibrary", no_class, info_sharedlibrary_command,
                  "Generic command for shlib information.",
                  &infoshliblist, "info sharedlibrary ", 0, &infolist);

  add_cmd ("cfm", no_class, info_cfm_command, "Show current CFM state.", &infoshliblist);
  add_cmd ("raw-cfm", no_class, info_raw_cfm_command, "Show current CFM state.", &infoshliblist);
  add_cmd ("dyld", no_class, info_dyld_command, "Show current DYLD state.", &infoshliblist);  

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

  cmd = add_set_cmd ("debug-dyld", class_obscure, var_zinteger, 
                     (char *) &dyld_debug_flag,
                     "Set if printing dyld communication debugging statements.",
                     &setlist);
  add_show_from_set (cmd, &showlist);

  dyld_minimal_load_rules = xstrdup ("\"dyld\" \"CarbonCore$\\\\|CarbonCore_[^/]*$\" all \".*\" \"dyld$\" extern \".*\" \".*\" none");
  dyld_load_rules = xstrdup ("\".*\" \".*\" all");
}

