#include "nextstep-nat-dyld-process.h"

#include "nextstep-nat-dyld-info.h"
#include "nextstep-nat-dyld-path.h"
#include "nextstep-nat-dyld-io.h"
#include "nextstep-nat-dyld.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-mutils.h"

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "objfiles.h"

#include "gnu-regex.h"

#include <mach-o/nlist.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_debug.h>

#include <string.h>

#include "mach-o.h"
#include "gdbcore.h"
#include "interpreter.h"

extern struct target_ops exec_ops;

extern int dyld_preload_libraries_flag;
extern int dyld_filter_events_flag;
extern int dyld_always_read_from_memory_flag;
extern char *dyld_symbols_prefix;
extern int dyld_load_dyld_symbols_flag;
extern int dyld_load_dyld_shlib_symbols_flag;
extern int dyld_load_cfm_shlib_symbols_flag;
extern int dyld_print_basenames_flag;
extern char *dyld_load_rules;
extern char *dyld_minimal_load_rules;

#if WITH_CFM
extern int inferior_auto_start_cfm_flag;
#endif /* WITH_CFM */

extern next_inferior_status *next_status;

static int
dyld_print_status()
{
    /* do not print status dots when executing MI */
    return !gdb_current_interpreter_is_named(GDB_INTERPRETER_MI);
}

void dyld_add_inserted_libraries
  (struct dyld_objfile_info *info, const struct dyld_path_info *d)
{
  const char *s1, *s2;

  CHECK_FATAL (info != NULL);
  CHECK_FATAL (d != NULL);

  s1 = d->insert_libraries;
  if (s1 == NULL) { return; }

  while (*s1 != '\0') {

    struct dyld_objfile_entry *e = NULL;

    s2 = strchr (s1, ':');
    if (s2 == NULL) {
      s2 = strchr (s1, '\0');
    }
    CHECK_FATAL (s2 != NULL);

    e = dyld_objfile_entry_alloc (info);

    e->user_name = savestring (s1, (s2 - s1));
    e->reason = dyld_reason_init;

    s1 = s2; 
    while (*s1 == ':') {
      s1++;
    }
  }
}

void dyld_add_image_libraries
  (struct dyld_objfile_info *info, bfd *abfd)
{
  struct mach_o_data_struct *mdata = NULL;
  unsigned int i;

  CHECK_FATAL (info != NULL);

  if (abfd == NULL) { return; }

  if (! bfd_mach_o_valid (abfd)) { return;  }

  mdata = abfd->tdata.mach_o_data;

  if (mdata == NULL)
    {
      dyld_debug ("dyld_add_image_libraries: mdata == NULL\n");
      return;
    }

  for (i = 0; i < mdata->header.ncmds; i++) {
    struct bfd_mach_o_load_command *cmd = &mdata->commands[i];
    switch (cmd->type) {
    case BFD_MACH_O_LC_LOAD_DYLINKER:
    case BFD_MACH_O_LC_ID_DYLINKER:
    case BFD_MACH_O_LC_LOAD_DYLIB:
    case BFD_MACH_O_LC_ID_DYLIB: {

      struct dyld_objfile_entry *e = NULL;
      char *name = NULL;

      switch (cmd->type) {
      case BFD_MACH_O_LC_LOAD_DYLINKER:
      case BFD_MACH_O_LC_ID_DYLINKER: {
	bfd_mach_o_dylinker_command *dcmd = &cmd->command.dylinker;

	name = xmalloc (dcmd->name_len + 1);
            
	bfd_seek (abfd, dcmd->name_offset, SEEK_SET);
	if (bfd_read (name, 1, dcmd->name_len, abfd) != dcmd->name_len) {
	  warning ("Unable to find library name for LC_LOAD_DYLINKER or LD_ID_DYLINKER command; ignoring");
	  free (name);
	  continue;
	}
	break;
      }
      case BFD_MACH_O_LC_LOAD_DYLIB:
      case BFD_MACH_O_LC_ID_DYLIB: {
	bfd_mach_o_dylib_command *dcmd = &cmd->command.dylib;

	name = xmalloc (dcmd->name_len + 1);
            
	bfd_seek (abfd, dcmd->name_offset, SEEK_SET);
	if (bfd_read (name, 1, dcmd->name_len, abfd) != dcmd->name_len) {
	  warning ("Unable to find library name for LC_LOAD_DYLIB or LD_ID_DYLIB command; ignoring");
	  free (name);
	  continue;
	}
	break;
      }
      default:
	abort ();
      }
      
      if (name[0] == '\0') {
	warning ("No image name specified by LC_LOAD or LC_ID command; ignoring");
	free (name);
	name = NULL;
      }

      e = dyld_objfile_entry_alloc (info);

      e->text_name = name;
      e->text_name_valid = 1;
      e->reason = dyld_reason_init;

      switch (cmd->type) {
      case BFD_MACH_O_LC_LOAD_DYLINKER:
      case BFD_MACH_O_LC_ID_DYLINKER:
	e->prefix = dyld_symbols_prefix;
	break;
      case BFD_MACH_O_LC_LOAD_DYLIB:
      case BFD_MACH_O_LC_ID_DYLIB:
	break;
      default:
	abort ();
      };
    }
    default:
      break;
    }
  }
}

void dyld_resolve_filename_image
(const struct next_dyld_thread_status *s, struct dyld_objfile_entry *e)
{
  struct mach_header header;

  CORE_ADDR curpos;
  unsigned int i;

  CHECK_FATAL (e->allocated);
  if (e->image_name_valid) { return; }

  if (! e->dyld_valid) { return; }

  target_read_memory (e->dyld_addr, (char *) &header, sizeof (struct mach_header));

  switch (header.filetype) {
  case MH_DYLINKER:
  case MH_DYLIB:
    break;
  case MH_BUNDLE:
    break;
  default:
    return;
  }

  curpos = ((unsigned long) e->dyld_addr) + sizeof (struct mach_header);
  for (i = 0; i < header.ncmds; i++) {

    struct load_command cmd;
    struct dylib_command dcmd;
    struct dylinker_command dlcmd;
    char name[256];

    target_read_memory (curpos, (char *) &cmd, sizeof (struct load_command));
    if (cmd.cmd == LC_ID_DYLIB) {
      target_read_memory (curpos, (char *) &dcmd, sizeof (struct dylib_command));
      target_read_memory (curpos + dcmd.dylib.name.offset, name, 256);
      e->image_name = strsave (name);
      e->image_name_valid = 1;
      break;
    } else if (cmd.cmd == LC_ID_DYLINKER) {
      target_read_memory (curpos, (char *) &dlcmd, sizeof (struct dylinker_command));
      target_read_memory (curpos + dlcmd.name.offset, name, 256);
      e->image_name = strsave (name);
      e->image_name_valid = 1;
      break;
    }

    curpos += cmd.cmdsize;
  }

  if (e->image_name == NULL) {
    dyld_debug ("Unable to determine filename for loaded object (no LC_ID load command)\n");
  } else {
    dyld_debug ("Determined filename for loaded object from image\n");
  }		
}

void dyld_resolve_filenames
(const struct next_dyld_thread_status *s, struct dyld_objfile_info *new)
{
  unsigned int i;

  CHECK_FATAL (s != NULL);
  CHECK_FATAL (new != NULL);

  for (i = 0; i < new->nents; i++) {
    struct dyld_objfile_entry *e = &new->entries[i];
    if (! e->allocated) { continue; }
    if (e->dyld_name_valid) { continue; }
    dyld_resolve_filename_image (s, e);
  }
}

static CORE_ADDR library_offset (struct dyld_objfile_entry *e)
{
  CHECK_FATAL (e != NULL);
  if (e->image_addr_valid && e->dyld_valid) {
    CHECK_FATAL (e->dyld_addr == ((e->image_addr + e->dyld_slide) & 0xffffffff));
  }

  if (e->dyld_valid) {
    return (unsigned long) e->dyld_addr;
  } else if (e->image_addr_valid) {
    return (unsigned long) e->image_addr;
  } else {
    return 0;
  }
}

unsigned int
dyld_parse_load_level (const char *s)
{
  if (strcmp (s, "all") == 0) {
    return OBJF_SYM_ALL;
  } else if (strcmp (s, "container") == 0) {
    return OBJF_SYM_CONTAINER;
  } else if (strcmp (s, "extern") == 0) {
    return OBJF_SYM_EXTERN;
  } else if (strcmp (s, "none") == 0) {
    return OBJF_SYM_NONE;
  } else {
    warning ("unknown setting \"%s\"; using \"none\"\n", s);
    return OBJF_SYM_NONE;
  }
}

int
dyld_resolve_load_flag (struct dyld_path_info *d, struct dyld_objfile_entry *e, const char *rules)
{
  const char *name = NULL;
  const char *leaf = NULL;

  char **prules = NULL;
  char **trule = NULL;
  int nrules = 0;
  int crule = 0;

  name = dyld_entry_string (e, 1);

  if (name == NULL)
    return OBJF_SYM_NONE;

  leaf = strrchr (name, '/');
  leaf = ((leaf != NULL) ? leaf : name);

  if (rules != NULL) { 
    prules = buildargv (rules);
    if (prules == NULL) {
      warning("unable to parse load rules");
      return OBJF_SYM_NONE;
    }
  }

  nrules = 0;

  if (prules != NULL) {
    for (trule = prules; *trule != NULL; trule++) {
      nrules++;
    }
  }

  if ((nrules % 3) != 0) {
    warning ("unable to parse load-rules (number of rule clauses must be a multiple of 3)");
    return OBJF_SYM_NONE;
  }
  nrules /= 3;

  for (crule = 0; crule < nrules; crule++) {

    char *matchreason = prules[crule * 3];
    char *matchname = prules[(crule * 3) + 1];
    char *setting = prules[(crule * 3) + 2];

    const char *reason = NULL;
    const char *name = NULL;

    regex_t reasonbuf;
    regex_t namebuf;

    int ret;

    reason = dyld_reason_string (e->reason);

    if (e->objfile) {
      if (e->loaded_from_memory) {
	name = "memory";
      } else {
	name = e->loaded_name;
      }
    } else {
      name = dyld_entry_source_filename (e);
      if (name == NULL) {
	return OBJF_SYM_NONE;
      }
      if (! dyld_entry_source_filename_is_absolute (e)) {
	name = dyld_resolve_image (d, name);
      }
    }

    if (name == NULL)
      {
	warning ("Couldn't find library name in dyld_resolve_load_flag.");
	return OBJF_SYM_NONE;
      }

    ret = gdb_regcomp (&reasonbuf, matchreason, REG_NOSUB);
    if (ret != 0) {
      warning ("unable to compile regular expression \"%s\"", matchreason);
      continue;
    }
    
    ret = gdb_regcomp (&namebuf, matchname, REG_NOSUB);
    if (ret != 0) {
      warning ("unable to compile regular expression \"%s\"", matchreason);
      continue;
    }

    ret = gdb_regexec (&reasonbuf, reason, 0, 0, 0);
    if (ret != 0)
      continue;

    ret = gdb_regexec (&namebuf, name, 0, 0, 0);
    if (ret != 0)
      continue;

    return dyld_parse_load_level (setting);
  }

  return -1;
}

int dyld_minimal_load_flag (struct dyld_path_info *d, struct dyld_objfile_entry *e)
{
  int ret = dyld_resolve_load_flag (d, e, dyld_minimal_load_rules);
  return (ret > 0) ? ret : OBJF_SYM_NONE;
}

int dyld_default_load_flag (struct dyld_path_info *d, struct dyld_objfile_entry *e)
{
  int ret = dyld_resolve_load_flag (d, e, dyld_load_rules);
  if (ret >= 0)
    return ret;

  if (e->reason != dyld_reason_cfm) {
    if (dyld_load_dyld_shlib_symbols_flag)
      return OBJF_SYM_ALL;
  } else {
    if (dyld_load_cfm_shlib_symbols_flag)
      return OBJF_SYM_ALL;
  }
  
  return OBJF_SYM_NONE;
}

void dyld_load_library (const struct dyld_path_info *d, struct dyld_objfile_entry *e)
{
  int read_from_memory = 0;
  const char *name = NULL;
  const char *name2 = NULL;

  CHECK_FATAL (e->allocated);

  if (e->abfd) { return; }
  if (e->loaded_error) { return; }

  if ((e->reason == dyld_reason_executable) && (symfile_objfile != NULL)) {
    e->abfd = symfile_objfile->obfd;
    return;
  }

  if (dyld_always_read_from_memory_flag) {
    read_from_memory = 1;
  }

  if (! read_from_memory) {
    name = dyld_entry_source_filename (e);
    if (name == NULL) {
      char *s = dyld_entry_string (e, 1);
      warning ("No image filename available for %s; reading from memory", s);
      free (s);
      read_from_memory = 1;
    }
  }

  if (! read_from_memory) {
    name2 = name;
    if (! dyld_entry_source_filename_is_absolute (e)) {
      name2 = dyld_resolve_image (d, name);
    }
    if (name2 == NULL) {
      char *s = dyld_entry_string (e, 1);
      warning ("Unable to resolve source pathname for %s; reading from memory", s);
      free (s);
      read_from_memory = 1;
    }      
  }

  if (read_from_memory && (! e->dyld_valid)) {
      char *s = dyld_entry_string (e, dyld_print_basenames_flag);
      warning ("Unable to read symbols from %s (not yet mapped into memory); skipping", s);
      return;
  }    

  if (! read_from_memory) {
    CHECK_FATAL (name2 != NULL);
    e->abfd = symfile_bfd_open_safe (name2);
    if (e->abfd == NULL) {
      char *s = dyld_entry_string (e, 1);
      warning ("Unable to read symbols from %s; reading from memory.", s);
      free (s);
    }
    e->loaded_name = name2;
    e->loaded_from_memory = 0;
  }	
  
  if (e->abfd == NULL) {
    CHECK_FATAL (e->dyld_valid);
    e->abfd = inferior_bfd (name, e->dyld_addr, e->dyld_slide, e->dyld_length);
    e->loaded_memaddr = e->dyld_addr;
    e->loaded_from_memory = 1;
  }

  if (e->abfd == NULL) {
    char *s = dyld_entry_string (e, 1);
    e->loaded_error = 1;
    warning ("Unable to read symbols from %s; skipping.", s);
    free (s);
    return;
  }

  if (e->reason & dyld_reason_image) 
    {
      asection *text_sect = bfd_get_section_by_name (e->abfd, "LC_SEGMENT.__TEXT");
      if (text_sect != NULL) {
	e->image_addr = bfd_section_vma (e->abfd, text_sect);
	e->image_addr_valid = 1;
      } else {
	char *s = dyld_entry_string (e, 1);
	warning ("Unable to locate text section for %s (no section \"%s\")", s, "LC_SEGMENT.__TEXT");
      }
    }
}

void dyld_load_libraries (const struct dyld_path_info *d, struct dyld_objfile_info *result)
{
  unsigned int i;
  CHECK_FATAL (result != NULL);

  for (i = 0; i < result->nents; i++) {
    struct dyld_objfile_entry *e = &result->entries[i];
    if (! e->allocated) { continue; }
    if (e->load_flag < 0) {
      e->load_flag = dyld_default_load_flag (d, e) | dyld_minimal_load_flag (d, e);
    }
    if (e->load_flag) {
      dyld_load_library (d, e);
    }
  }
}

void dyld_load_symfile (struct dyld_objfile_entry *e) 
{
  char *name = NULL;
  char *leaf = NULL;
  struct section_addr_info addrs;
  unsigned int i;

  if (e->objfile) { return; }
  if (e->loaded_error) { return; }

  CHECK_FATAL (e->allocated);

  CHECK_FATAL (e->objfile == NULL);
  CHECK_FATAL (e->dyld_valid || (e->image_addr != 0));
  CHECK_FATAL (e->abfd != NULL);
  
  if ((e->reason == dyld_reason_executable) && (symfile_objfile != NULL)) {
    e->objfile = symfile_objfile;
    e->abfd = symfile_objfile->obfd;
    e->loaded_from_memory = 0;
    e->loaded_name = e->text_name;
    e->loaded_addr = 0;
    e->loaded_addrisoffset = 1;
    return;
  }

  name = dyld_entry_string (e, dyld_print_basenames_flag);

  leaf = strrchr (name, '/');
  leaf = ((leaf != NULL) ? leaf : name);

  if (e->dyld_valid) { 
    e->loaded_addr = e->dyld_addr;
    e->loaded_addrisoffset = 0;
  } else if (e->image_addr_valid) {
    e->loaded_addr = e->image_addr;
    e->loaded_addrisoffset = 0;
  } else {
    e->loaded_addrisoffset = 1;
  }

  for (i = 0; i < MAX_SECTIONS; i++) {
    addrs.other[i].name = NULL;
    addrs.other[i].addr = e->dyld_slide;
    addrs.other[i].sectindex = 0;
  }

  addrs.addrs_are_offsets = 1;

  e->objfile = symbol_file_add_bfd_safe (e->abfd, 0, &addrs, 0, 0, e->load_flag, 0, e->prefix);

#if WITH_CFM
  while (strstr (leaf, "CarbonCore") != NULL) {

    struct minimal_symbol *hooksym = lookup_minimal_symbol ("gPCFMInfoHooks", NULL, NULL);
    struct minimal_symbol *system = lookup_minimal_symbol ("gPCFMSystemUniverse", NULL, NULL);
    struct minimal_symbol *context = lookup_minimal_symbol ("gPCFMContextUniverse", NULL, NULL);
    struct cfm_parser *parser = &next_status->cfm_status.parser;
    CORE_ADDR offset = 0;
 
    if ((hooksym == NULL) || (system == NULL) || (context == NULL))
      break;

    offset = SYMBOL_VALUE_ADDRESS (context) - SYMBOL_VALUE_ADDRESS (system);

    if (offset == 88) {
      parser->version = 3;
      parser->universe_length = 88;
      parser->universe_container_offset = 48;
      parser->universe_connection_offset = 60;
      parser->universe_closure_offset = 72;
      parser->connection_length = 68;
      parser->connection_next_offset = 0;
      parser->connection_container_offset = 28;
      parser->container_length = 176;
      parser->container_address_offset = 24;
      parser->container_length_offset = 28;
      parser->container_fragment_name_offset = 44;
      parser->container_section_count_offset = 100;
      parser->container_sections_offset = 104;
      parser->section_length = 24;
      parser->section_total_length_offset = 12;
      parser->instance_length = 24;
      parser->instance_address_offset = 12;
      next_status->cfm_status.breakpoint_offset = 392;
    } else if (offset == 104) {
      parser->version = 2;
      parser->universe_length = 104;
      parser->universe_container_offset = 52;
      parser->universe_connection_offset = 68;
      parser->universe_closure_offset = 84;
      parser->connection_length = 72;
      parser->connection_next_offset = 0;
      parser->connection_container_offset = 32;
      parser->container_length = 176;
      parser->container_address_offset = 28;
      parser->container_length_offset = 36;
      parser->container_fragment_name_offset = 44;
      parser->container_section_count_offset = 100;
      parser->container_sections_offset = 104;
      parser->section_length = 24;
      parser->section_total_length_offset = 12;
      parser->instance_length = 24;
      parser->instance_address_offset = 12;
      next_status->cfm_status.breakpoint_offset = 864;
    } else if (offset == 120) {
      parser->version = 1;
      parser->universe_length = 120;
      parser->universe_container_offset = 68;
      parser->universe_connection_offset = 84;
      parser->universe_closure_offset = 100;
      parser->connection_length = 84;
      parser->connection_next_offset = 0;
      parser->connection_container_offset = 36;
      parser->container_length = 172;
      parser->container_address_offset = 28;
      parser->container_length_offset = 32;
      parser->container_fragment_name_offset = 40;
      parser->container_section_count_offset = 96;
      parser->container_sections_offset = 100;
      parser->section_length = 24;
      parser->section_total_length_offset = 12;
      parser->instance_length = 24;
      parser->instance_address_offset = 12;
      next_status->cfm_status.breakpoint_offset = 864;
    } else {
      warning ("unable to determine CFM version; disabling CFM support");
      parser->version = 0;
      break;
    }
	
    next_status->cfm_status.info_api_cookie = SYMBOL_VALUE_ADDRESS (hooksym);
    dyld_debug ("Found gPCFMInfoHooks in CarbonCore: 0x%lx with version %d\n",
		SYMBOL_VALUE_ADDRESS (hooksym), parser->version);

    if (inferior_auto_start_cfm_flag)
      next_cfm_thread_create (&next_status->cfm_status, next_status->task);

    break;
  }
#endif /* WITH_CFM */
  
  free (name);

  if (e->objfile != NULL) { 
    CHECK_FATAL (e->objfile->obfd != NULL);
  }

  if (e->objfile == NULL) {
    e->loaded_error = 1;
    e->abfd = NULL;
    free (name);
    return;
  }

  if (e->reason == dyld_reason_executable) {
    CHECK_FATAL (symfile_objfile == NULL);
    symfile_objfile = e->objfile;
    return;
  }
}

void dyld_load_symfiles (struct dyld_objfile_info *result)
{
  unsigned int i;
  unsigned int first = 1;
  CHECK_FATAL (result != NULL);

  for (i = 0; i < result->nents; i++) {
    struct dyld_objfile_entry *e = &result->entries[i];
    if (! e->allocated) { continue; }
    if (e->objfile != NULL) { continue; }
    if (e->loaded_error) { continue; }
    if ((! e->dyld_valid) && (e->image_addr == 0)) {
      if (info_verbose) {
	dyld_debug ("Deferring load (not pre-bound)\n");
      }
      continue;
    }
    if (e->abfd == NULL) {
      continue;
    }
    if (first && (! info_verbose) && dyld_print_status()) {
      first = 0;
      printf_filtered ("Reading symbols for shared libraries ");
      gdb_flush (gdb_stdout);
    }
    dyld_load_symfile (e);
    if ((! info_verbose) && dyld_print_status()) {
      printf_filtered (".");
      gdb_flush (gdb_stdout);
    }
  }
  if ((! first) && (! info_verbose) && dyld_print_status()) {
    printf_filtered (" done\n");
    gdb_flush (gdb_stdout);
  }
}

static int dyld_objfile_allocated (struct objfile *o)
{
  struct objfile *objfile, *temp;

  ALL_OBJFILES_SAFE (objfile, temp) {
    if (o == objfile) {
      return 1;
    }
  }
  return 0;
}

void dyld_remove_objfile (struct dyld_objfile_entry *e)
{
  char *s = NULL;

  CHECK_FATAL (e->allocated);

  if ((e->reason == dyld_reason_executable) && (e->objfile == NULL)) {
    e->objfile = symfile_objfile;
  }
  if (e->objfile == NULL) { return; }

  CHECK_FATAL (dyld_objfile_allocated (e->objfile));
  CHECK_FATAL (e->objfile->obfd != NULL);

  s = dyld_entry_string (e, dyld_print_basenames_flag);
  if (info_verbose) {
    printf_filtered ("Removing symbols for %s\n", s);
  }
  free (s);
  gdb_flush (gdb_stdout);
  free_objfile (e->objfile);
  e->objfile = NULL;
  e->abfd = NULL;
  gdb_flush (gdb_stdout);

  if (e->reason == dyld_reason_executable) {
    symfile_objfile = NULL;
  }
}

void dyld_remove_objfiles (const struct dyld_path_info *d, struct dyld_objfile_info *result)
{
  unsigned int i;
  unsigned int first = 1;
  CHECK_FATAL (result != NULL);

  for (i = 0; i < result->nents; i++) {
    struct dyld_objfile_entry *e = &result->entries[i];
    struct objfile *o = NULL;
    if (! e->allocated) { continue; }
    if (e->load_flag < 0) {
      e->load_flag = dyld_default_load_flag (d, e) | dyld_minimal_load_flag (d, e);
    }
    if ((e->reason == dyld_reason_executable) && (e->objfile == NULL)) {
      o = symfile_objfile;
    } else {
      o = e->objfile;
    }
    if ((o != NULL) && (e->load_flag != o->symflags)) {
      dyld_remove_objfile (e);
      if (first && (! info_verbose) && dyld_print_status()) {
	first = 0;
	printf_filtered ("Removing symbols for unused shared libraries ");
	gdb_flush (gdb_stdout);
      }
      if ((! info_verbose) && dyld_print_status()) {
	printf_filtered (".");
	gdb_flush (gdb_stdout);
      }
    }
  }
  if ((! first) && (! info_verbose) && dyld_print_status()) {
    printf_filtered (" done\n");
    gdb_flush (gdb_stdout);
  }
}

static int dyld_libraries_similar
(struct dyld_objfile_entry *f, struct dyld_objfile_entry *l)
{
  const char *fname = NULL;
  const char *lname = NULL;

  const char *fbase = NULL;
  const char *lbase = NULL;
  unsigned int flen = 0;
  unsigned int llen = 0;

  CHECK_FATAL (f != NULL);
  CHECK_FATAL (l != NULL);

  fname = dyld_entry_source_filename (f);
  lname = dyld_entry_source_filename (l);

  if ((lname != NULL) && (fname != NULL)) {

    int f_is_framework, f_is_bundle;
    int l_is_framework, l_is_bundle;

    dyld_library_basename (fname, &fbase, &flen, &f_is_framework, &f_is_bundle);
    dyld_library_basename (lname, &lbase, &llen, &l_is_framework, &l_is_bundle);

    if ((flen != llen) || (strncmp (fbase, lbase, llen) != 0))
      return 0;

    if (f_is_framework != l_is_framework)
      return 0;

    if (f_is_bundle != l_is_bundle)
      return 0;

    return 1;
  }
  
  if (library_offset (f) == library_offset (l)
      && (library_offset (f) != 0)
      && (library_offset (l) != 0)) {
    return 1;
  }

  return 0;
}

static int dyld_libraries_compatible
(struct dyld_path_info *d,
 struct dyld_objfile_entry *f, struct dyld_objfile_entry *l)
{
  const char *fname = NULL;
  const char *lname = NULL;

  const char *fres = NULL;
  const char *lres = NULL;

  CHECK_FATAL (f != NULL);
  CHECK_FATAL (l != NULL);

  fname = dyld_entry_source_filename (f);
  lname = dyld_entry_source_filename (l);

  if (strcmp (f->prefix, l->prefix) != 0) {
    return 0;
  }

  if (library_offset (f) != library_offset (l)
      && (library_offset (f) != 0)
      && (library_offset (l) != 0)) {
    return 0;
  }

  if ((fname != NULL) && (lname != NULL)) {
    fres = fname;
    if (! dyld_entry_source_filename_is_absolute (f)) {
      fres = dyld_resolve_image (d, fname);
    }
    lres = lname;
    if (! dyld_entry_source_filename_is_absolute (l)) {
      lres = dyld_resolve_image (d, lname);
    }
    if ((fres == NULL) != (lres == NULL)) {
      return 0;
    }
    if ((fres == NULL) && (lres == NULL)) {
      if (strcmp (fname, lname) != 0) {
	return 0;
      }
    }
    CHECK_FATAL ((fres != NULL) && (lres != NULL));
    if (strcmp (fres, lres) != 0) {
      return 0;
    }
  }

  if (dyld_always_read_from_memory_flag) {
    if (f->loaded_from_memory != l->loaded_from_memory) {
      return 0;
    }
  }

  return 1;
}
	  
void dyld_objfile_move_load_data
(struct dyld_objfile_entry *f, struct dyld_objfile_entry *l)
{
  l->objfile = f->objfile;
  l->abfd = f->abfd;
  
  if (l->load_flag < 0) {
    l->load_flag = f->load_flag; 
  }

  l->loaded_name = f->loaded_name;
  l->loaded_memaddr = f->loaded_memaddr;
  l->loaded_addr = f->loaded_addr;
  l->loaded_offset = f->loaded_offset;
  l->loaded_addrisoffset = f->loaded_addrisoffset;
  l->loaded_from_memory = f->loaded_from_memory;
  l->loaded_error = f->loaded_error;

  f->objfile = NULL;
  f->abfd = NULL;
  
  f->load_flag = -1;

  f->loaded_name = NULL;
  f->loaded_memaddr = 0;
  f->loaded_addr = 0;
  f->loaded_offset = 0;
  f->loaded_addrisoffset = 0;
  f->loaded_from_memory = 0;
  f->loaded_error = 0;
}

void dyld_check_discarded (struct dyld_objfile_info *info)
{
  unsigned int j;
  for (j = 0; j < info->nents; j++) {
    struct dyld_objfile_entry *e = &info->entries[j];
    if ((e->abfd == NULL) && (e->objfile == NULL) && (! e->loaded_error)) {
      dyld_objfile_entry_clear (e);
    }
  }
}

void dyld_merge_shlibs
(const struct next_dyld_thread_status *s,
 struct dyld_path_info *d,
 struct dyld_objfile_info *old, 
 struct dyld_objfile_info *new, 
 struct dyld_objfile_info *result)
{
  unsigned int i;
  unsigned int j;

  CHECK_FATAL (old != NULL);
  CHECK_FATAL (new != NULL);
  CHECK_FATAL (result != NULL);
  CHECK_FATAL (old != new);
  CHECK_FATAL (old != result);
  CHECK_FATAL (new != result);

  dyld_resolve_filenames (s, new);

  for (i = 0; i < new->nents; i++) {

    struct dyld_objfile_entry *n = &new->entries[i];
    if (! n->allocated) { continue; }

    for (j = 0; j < old->nents; j++) {

      struct dyld_objfile_entry *o = &old->entries[j];
      if (! o->allocated) { continue; }

      if (o->reason == dyld_reason_executable) { 
	/* The executable's objfile gets cleaned up and the new 
	   one set earlier on, so we can just get rid of the cached copy 
	   here. */ 
               
	dyld_debug ("Removing old executable objfile entry\n"); 
	dyld_objfile_entry_clear (o); 
	continue;
      }

      if (dyld_libraries_similar (n, o)) {
	
	if (o->objfile != NULL) {
	  
	  char *ns = dyld_entry_string (n, dyld_print_basenames_flag);
	  char *os = dyld_entry_string (o, dyld_print_basenames_flag);

	  CHECK_FATAL (dyld_objfile_allocated (o->objfile));

	  if (dyld_libraries_compatible (d, n, o)) {
	    dyld_debug ("Symbols for %s already loaded; not re-processing\n", os);
	    dyld_objfile_move_load_data (o, n);
	  } else if (n->reason == dyld_reason_init) {
	    dyld_debug ("Symbols for %s incompatible with %s, but may need to be re-loaded; deferring\n", os);
	    dyld_objfile_move_load_data (o, n);
	  } else {
	    dyld_debug ("Symbols for %s incompatible with %s; reloading\n", os, ns);
	    dyld_remove_objfile (o);
	  }

	  xfree (ns);
	  xfree (os);

	} else {

	  if (dyld_libraries_compatible (d, n, o)) {
	    dyld_objfile_move_load_data (o, n);
	  }
	}
	
	dyld_objfile_entry_clear (o);
      } /* similar */ 
    }
  }

  /* remaining files in 'old' will be cached for future use */
  for (i = 0; i < old->nents; i++) {

    struct dyld_objfile_entry *o = &old->entries[i];
    struct dyld_objfile_entry *e = NULL;

    if (! o->allocated) { continue; }

    e = dyld_objfile_entry_alloc (result);
    *e = *o;

    e->reason = dyld_reason_cached;

    dyld_objfile_entry_clear (o);
  }
    
  /* all remaining files in 'new' will need to be loaded */
  for (i = 0; i < new->nents; i++) {

    struct dyld_objfile_entry *n = &new->entries[i];
    struct dyld_objfile_entry *e = NULL;
    
    if (! n->allocated) { continue; }
    
    e = dyld_objfile_entry_alloc (result);
    *e = *n;
    
    dyld_objfile_entry_clear (n);
  }

  dyld_remove_objfiles (d, result);

  dyld_objfile_info_pack (result);
}

void dyld_update_shlibs
(const struct next_dyld_thread_status *s,
 struct dyld_path_info *d,
 struct dyld_objfile_info *old, 
 struct dyld_objfile_info *new, 
 struct dyld_objfile_info *result)
{
  CHECK_FATAL (old != NULL);
  CHECK_FATAL (new != NULL);
  CHECK_FATAL (result != NULL);

  dyld_debug ("dyld_update_shlibs: updating shared library information\n");

  dyld_merge_shlibs (s, d, old, new, result);

  dyld_load_libraries (d, result);
  dyld_load_symfiles (result);

  update_section_tables (&current_target);
  update_section_tables (&exec_ops);

  reread_symbols ();
  breakpoint_re_set ();
  breakpoint_update ();
  re_enable_breakpoints_in_shlibs (0);
}

static void dyld_process_image_event
(struct dyld_objfile_info *info,
 const struct dyld_event *event)
{
  struct dyld_debug_module module;
  struct mach_header header;
  CORE_ADDR addr, slide;

  CHECK_FATAL (info != NULL);
  CHECK_FATAL (event != NULL);

  module = event->arg[0];

  addr = (unsigned long) (((unsigned char *) module.header) - ((unsigned char *) 0));
  slide = (unsigned long) module.vmaddr_slide;
  
  target_read_memory (addr, (char *) &header, sizeof (struct mach_header));
  
  switch (header.filetype) {
  case MH_EXECUTE: {
    struct dyld_objfile_entry *e = dyld_objfile_entry_alloc (info);
    e->dyld_addr = addr;
    e->dyld_slide = slide;
    e->dyld_valid = 1;
    e->dyld_index = module.module_index;
    e->reason = dyld_reason_executable;
    dyld_debug ("Noted executable at 0x%lx (offset 0x%lx)\n", (unsigned long) addr, slide);
    break;
  }
  case MH_FVMLIB:
    dyld_debug ("Ignored fixed virtual memory shared library at 0x%lx (offset 0x%lx)\n",
		(unsigned long) addr, slide);
    break;
  case MH_PRELOAD:
    dyld_debug ("Ignored preloaded executable at 0x%lx (offset 0x%lx)\n", (unsigned long) addr, slide);
    break;
  case MH_DYLIB: {
    struct dyld_objfile_entry *e = dyld_objfile_entry_alloc (info);
    e->dyld_addr = addr;
    e->dyld_slide = slide;
    e->dyld_valid = 1;
    e->dyld_index = module.module_index;
    e->reason = dyld_reason_dyld;
    dyld_debug ("Noted dynamic library at 0x%lx (offset 0x%lx)\n", (unsigned long) addr, slide);
    break;
  }
  case MH_DYLINKER: {
    struct dyld_objfile_entry *e = dyld_objfile_entry_alloc (info);
    e->dyld_addr = addr;
    e->dyld_slide = slide;
    e->dyld_valid = 1;
    e->dyld_index = module.module_index;
    e->prefix = dyld_symbols_prefix;
    e->reason = dyld_reason_dyld;
    dyld_debug ("Noted dynamic link editor at 0x%lx (offset 0x%lx)\n", (unsigned long) addr, slide);
    break;
  }
  case MH_BUNDLE: {
    struct dyld_objfile_entry *e = dyld_objfile_entry_alloc (info);
    e->dyld_addr = addr;
    e->dyld_slide = slide;
    e->dyld_valid = 1;
    e->dyld_index = module.module_index;
    e->reason = dyld_reason_dyld;
    dyld_debug ("Noted bundle at 0x%lx (offset 0x%lx)\n", (unsigned long) addr, slide);
    break;
  }
  default:
    warning ("Ignored unknown object module at 0x%lx (offset 0x%lx) with type 0x%lx\n",
	     (unsigned long) addr, (unsigned long) slide, (unsigned long) header.filetype);
    break;
  }
}

void dyld_purge_cached_libraries (struct dyld_objfile_info *info)
{
  unsigned int i;
  CHECK_FATAL (info != NULL);

  for (i = 0; i < info->nents; i++) {
    struct dyld_objfile_entry *e = &info->entries[i];
    if (! e->allocated) { continue; }
    if (e->reason == dyld_reason_cached) {
      dyld_remove_objfile (e);
      dyld_objfile_entry_clear (e);
    }
  }

  dyld_objfile_info_pack (info);
  update_section_tables (&current_target);
  update_section_tables (&exec_ops);

  reread_symbols ();
  breakpoint_re_set ();
  breakpoint_update ();
  re_enable_breakpoints_in_shlibs (0);
}

void
_initialize_nextstep_nat_dyld_process ()
{
}
