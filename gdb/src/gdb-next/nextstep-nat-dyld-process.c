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

#include <mach-o/nlist.h>
#include <mach-o/loader.h>
#include <mach-o/dyld_debug.h>

#include <string.h>

#include "mach-o.h"

#include "gdbcore.h"		/* for core_ops */
extern struct target_ops exec_ops;

#define _dyld_debug_make_runnable(a, b) DYLD_FAILURE
#define _dyld_debug_restore_runnable(a, b) DYLD_FAILURE
#define _dyld_debug_module_name(a, b, c, d, e, f, g, h, i) DYLD_FAILURE
#define _dyld_debug_set_error_func(a) DYLD_FAILURE
#define _dyld_debug_add_event_subscriber(a, b, c, d, e) DYLD_FAILURE

static int dyld_allow_resolve_filenames_flag = 1;
static int dyld_always_resolve_filenames_flag = 1;
static int dyld_always_read_from_memory_flag = 0;
static int dyld_remove_overlapping_basenames_flag = 1;
static char *dyld_symbols_prefix = "__dyld_";
static int dyld_load_dyld_symbols_flag = 1;
static int dyld_load_shlib_symbols_flag = 1;
static int dyld_print_basenames_flag = 0;

#if WITH_CFM
extern int inferior_auto_start_cfm_flag;
#endif /* WITH_CFM */

extern next_inferior_status *next_status;

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
    e->load_flag = 1;
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

  if (! bfd_mach_o_valid (abfd)){ return;  }

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
      e->load_flag = 1;
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

void dyld_load_library (const struct dyld_path_info *d, struct dyld_objfile_entry *e)
{
  int read_from_memory = 0;
  const char *name = NULL;
  const char *name2 = NULL;

  CHECK_FATAL (e->allocated);

  if (e->loaded_flag) { return; }
  if (e->loaded_error) { return; }

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
      free (s);
      return;
  }    

  if (read_from_memory) {
    CHECK_FATAL (e->dyld_valid);
    e->abfd = mach_o_inferior_bfd (e->dyld_addr, e->dyld_slide);
    e->loaded_memaddr = e->dyld_addr;
    e->loaded_from_memory = 1;
  } else {
    CHECK_FATAL (name2 != NULL);
    e->abfd = symfile_bfd_open_safe (name2);
    e->loaded_name = name2;
    e->loaded_from_memory = 0;
  }

  if (e->abfd == NULL) {
    char *s = dyld_entry_string (e, 1);
    e->loaded_error = 1;
    warning ("Unable to read symbols from %s; skipping.", s);
    free (s);
    return;
  }

  {
    asection *text_sect = bfd_get_section_by_name (e->abfd, "LC_SEGMENT.__TEXT");
    if (text_sect != NULL) {
      e->image_addr = bfd_section_vma (e->abfd, text_sect);
      e->image_addr_valid = 1;
    } else {
      char *s = dyld_entry_string (e, 1);
      warning ("Unable to locate text section for %s (no section \"%s\")\n", 
	       s, "LC_SEGMENT.__TEXT");
      free (e);
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
    if (e->load_flag) {
      dyld_load_library (d, e);
    }
  }
}

void dyld_load_symfile (struct dyld_objfile_entry *e) 
{
  char *name = NULL;
  char *leaf = NULL;
  int load_symbols = 0;
  struct section_addr_info addrs;
  unsigned int i;

  CHECK_FATAL (e->allocated);

  CHECK_FATAL (e->objfile == NULL);
  CHECK_FATAL (e->dyld_valid || (e->image_addr != 0));
  CHECK_FATAL (e->abfd != NULL);
  
  e->loaded_flag = 1;

  if (e->dyld_valid) { 
    e->loaded_addr = e->dyld_addr;
    e->loaded_addrisoffset = 0;
  } else {
    e->loaded_addr = e->image_addr;
    e->loaded_addrisoffset = 0;
  }

  name = dyld_entry_string (e, dyld_print_basenames_flag);

  leaf = strrchr (name, '/');
  leaf = ((leaf != NULL) ? leaf : name);

#if WITH_CFM
  if (inferior_auto_start_cfm_flag && (strstr (leaf, "CarbonCore") != NULL)) {
    load_symbols = 1;
  }
#endif /* WITH_CFM */

  if ((strstr (leaf, "dyld") != NULL) && dyld_load_dyld_symbols_flag) {
    load_symbols = 1;
  }

  if ((strstr (leaf, "dyld") == NULL) && dyld_load_shlib_symbols_flag) {
    load_symbols = 1;
  }
  
  for (i = 0; i < MAX_SECTIONS; i++) {
	addrs.other[i].name = NULL;
	addrs.other[i].addr = e->dyld_slide;
	addrs.other[i].sectindex = 0;
  }

  addrs.addrs_are_offsets = 1;

  if (load_symbols) {
    e->objfile = symbol_file_add_bfd_safe (e->abfd, 0, &addrs, 0, 0, 0, e->prefix);
    if (e->objfile == NULL) {
      e->loaded_error = 1;
      e->abfd = NULL;
      free (name);
      return;
    }
  }

#if WITH_CFM
  if (inferior_auto_start_cfm_flag && (strstr (leaf, "CarbonCore") != NULL)) {

    struct minimal_symbol *sym = lookup_minimal_symbol ("gPCFMInfoHooks", NULL, NULL);
    if (sym != NULL) {

      CORE_ADDR addr = SYMBOL_VALUE_ADDRESS (sym);
      dyld_debug ("Found gPCFMInfoHooks in CarbonCore: 0x%lx\n", (unsigned long) addr);
      next_status->cfm_status.info_api_cookie = (void *) ((unsigned long) addr);
      
      /* At one time gPCFMInfoHooks was in libbase.dylib which nothing was directly
	 linked against. By the time GDB noticed the library was loaded the program
	 was already running and so the CFM API had to be initialized then. Currently
	 LauchCFMApp is linked directly against CarbonCore and so the location of
	 gPCFMInfoHooks is known when the symbol table is read (before the program
	 starts) - you can't initialize the CFM API before the program is running
	 though since the initialization reads memory from the inferior. This init
	 call here probably isn't needed, but is being left in case some CFM launching
	 app loads CarbonCore indirectly. The routine checks to make sure the inferior
	 exists and also that it doesn't initialize itself twice, so having it here
	 and being called twice won't hurt anything. */

      next_cfm_thread_create (&next_status->cfm_status, next_status->task);
    }
  }
#endif /* WITH_CFM */
  
  if (e->objfile == NULL) { 
    return;
  }

  CHECK_FATAL (e->objfile->obfd != NULL);

  free (name);
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
    if ((! e->dyld_valid) && (e->image_addr == 0)) {
      if (info_verbose) {
	printf_filtered ("Deferring load (not pre-bound)\n");
      }
      continue;
    }
    if (e->abfd == NULL) {
      continue;
    }
    if (first && (! info_verbose)) {
      first = 0;
      printf_filtered ("Reading symbols for shared libraries ");
      gdb_flush (gdb_stdout);
    }
    dyld_load_symfile (e);
    if (! info_verbose) {
      printf_filtered (".");
      gdb_flush (gdb_stdout);
    }
  }
  if ((! first) && (! info_verbose)) {
    printf_filtered (" done\n");
    gdb_flush (gdb_stdout);
  }
}

void dyld_merge_libraries
(struct dyld_objfile_info *old, struct dyld_objfile_info *new, struct dyld_objfile_info *result)
{
  unsigned int i;

  CHECK_FATAL (old != NULL);
  CHECK_FATAL (old != new);
  CHECK_FATAL (old != result);
  CHECK_FATAL (new != result);

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

  if (e->load_flag) { return; }

  if (e->objfile == NULL) { return; }

  CHECK_FATAL (dyld_objfile_allocated (e->objfile));
  CHECK_FATAL (e->objfile->obfd != NULL);

  s = dyld_entry_string (e, dyld_print_basenames_flag);
  free (s);
  gdb_flush (gdb_stdout);
  free_objfile (e->objfile);
  e->objfile = NULL;
  e->abfd = NULL;
  gdb_flush (gdb_stdout);
}

void dyld_remove_objfiles (struct dyld_objfile_info *result)
{
  unsigned int i;
  unsigned int first = 1;
  CHECK_FATAL (result != NULL);

  for (i = 0; i < result->nents; i++) {
    struct dyld_objfile_entry *e = &result->entries[i];
    if (! e->allocated) { continue; }
    if ((! e->load_flag) && e->loaded_flag) {
      dyld_remove_objfile (e);
      if (first && (! info_verbose)) {
	first = 0;
	printf_filtered ("Removing symbols for unused shared libraries ");
	gdb_flush (gdb_stdout);
      }
      if (! info_verbose) {
	printf_filtered (".");
	gdb_flush (gdb_stdout);
      }
    }
  }
  if ((! first) && (! info_verbose)) {
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

    if ((flen != llen) || (strncmp (fbase, lbase, llen) != 0)) {
      return 0;
    }
    if (f_is_framework != l_is_framework) {
      return 0;
    }
    if (f_is_bundle != l_is_bundle) {
      return 0;
    }

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
  
  l->load_flag = 1;

  l->loaded_name = f->loaded_name;
  l->loaded_memaddr = f->loaded_memaddr;
  l->loaded_addr = f->loaded_addr;
  l->loaded_offset = f->loaded_offset;
  l->loaded_addrisoffset = f->loaded_addrisoffset;
  l->loaded_from_memory = f->loaded_from_memory;
  l->loaded_error = f->loaded_error;
  l->loaded_flag = f->loaded_flag;

  f->objfile = NULL;
  f->abfd = NULL;
  
  f->load_flag = 0;

  f->loaded_name = NULL;
  f->loaded_memaddr = 0;
  f->loaded_addr = 0;
  f->loaded_offset = 0;
  f->loaded_addrisoffset = -1;
  f->loaded_from_memory = -1;
  f->loaded_error = 0;
  f->loaded_flag = 0;
}

void dyld_remove_duplicates (struct dyld_path_info *d, struct dyld_objfile_info *result)
{
  unsigned int i, j;

  CHECK_FATAL (result != NULL);

  for (i = 0; i < result->nents; i++) {
    for (j = i + 1; j < result->nents; j++) {

      struct dyld_objfile_entry *f = &result->entries[i];
      struct dyld_objfile_entry *l = &result->entries[j];

      if ((! f->allocated) || (! l->allocated)) { continue; }

      if ((f->objfile != NULL) && (f->objfile == l->objfile)) {
	dyld_objfile_move_load_data (f, l);
      }
    }
  }

  /* remove libraries in 'old' already loaded by something in 'result' */
  for (i = 0; i < result->nents; i++) {
    for (j = i + 1; j < result->nents; j++) {

      struct dyld_objfile_entry *f = &result->entries[i];
      struct dyld_objfile_entry *l = &result->entries[j];

      if (! f->allocated) { continue; }
      if (! l->allocated) { continue; }

      if ((! dyld_remove_overlapping_basenames_flag) && (f->reason != dyld_reason_cached)) {
	continue;
      }
      
      if (dyld_libraries_similar (f, l)) {
	
	if (f->objfile != NULL) {
	  
	  char *s = dyld_entry_string (f, dyld_print_basenames_flag);
	  CHECK_FATAL (dyld_objfile_allocated (f->objfile));

	  if (dyld_libraries_compatible (d, f, l)) {
	    dyld_debug ("Symbols for %s already loaded; not re-processing\n", s);
	    dyld_objfile_move_load_data (f, l);
	  } else if (l->reason == dyld_reason_init) {
	    dyld_debug ("Symbols for %s may need to be re-loaded; deferring\n", s);
	    dyld_objfile_move_load_data (f, l);
	  } else {
	    f->load_flag = 0;
	    dyld_remove_objfile (f);
	  }

	  free (s);
	}
	
	dyld_objfile_entry_clear (f);
      }
    }
  }
}

void dyld_check_discarded (struct dyld_objfile_info *info)
{
  unsigned int j;
  for (j = 0; j < info->nents; j++) {
    struct dyld_objfile_entry *e = &info->entries[j];
    if (! dyld_objfile_allocated (e->objfile)) {
      dyld_objfile_entry_clear (e);
    }
  }
}

void dyld_update_shlibs
(const struct next_dyld_thread_status *s,
 struct dyld_path_info *d,
 struct dyld_objfile_info *old, 
 struct dyld_objfile_info *new, 
 struct dyld_objfile_info *result)
{
  unsigned int j;

  CHECK_FATAL (old != NULL);
  CHECK_FATAL (new != NULL);
  CHECK_FATAL (result != NULL);

  dyld_debug ("dyld_update_shlibs: updating shared library information\n");

  dyld_check_discarded (old);
  dyld_merge_libraries (old, new, result);

  for (j = 0; j < result->nents; j++) {
    struct dyld_objfile_entry *e = &result->entries[j];
    if (! e->allocated) { continue; }
    if (symfile_objfile && (e->reason == dyld_reason_executable) && e->load_flag) {
      e->load_flag = 0;
    }
  }

  dyld_resolve_filenames (s, result);
  dyld_remove_objfiles (result);
  dyld_remove_duplicates (d, result);

  dyld_objfile_info_pack (result);
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
    e->load_flag = 1;
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
    e->load_flag = 1;
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
    e->load_flag = 1;
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
    e->load_flag = 1;
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

void dyld_process_event
(struct dyld_objfile_info *info,
 const struct dyld_event *event)
{
  CHECK_FATAL (info != NULL);
  CHECK_FATAL (event != NULL);

  switch (event->type) {
  case DYLD_IMAGE_ADDED:
    dyld_process_image_event (info, event);
    break;
  case DYLD_MODULE_BOUND:
    dyld_debug ("DYLD module bound: 0x%08lx -- 0x%08lx -- %4u\n", 
		 (unsigned long) event->arg[0].header, 
		 (unsigned long) event->arg[0].vmaddr_slide, 
		 (unsigned int) event->arg[0].module_index); 
    break;
  case DYLD_MODULE_REMOVED:
    dyld_debug ("DYLD module removed.\n");
    break;
  case DYLD_MODULE_REPLACED:
    dyld_debug ("DYLD module replaced.\n");
    break;
  case DYLD_PAST_EVENTS_END:
    dyld_debug ("DYLD past events end.\n");
    break;
  default:
    dyld_debug ("Unknown DYLD event type 0x%x; ignoring\n", (unsigned int) event->type);
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
      e->load_flag = 0;
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
  struct cmd_list_element *cmd = NULL;
  char *temp = NULL;

  cmd = add_set_cmd ("dyld-load-dyld-symbols", class_obscure, var_boolean, 
		     (char *) &dyld_load_dyld_symbols_flag,
		     "Set if GDB should load symbol information for the dynamic linker.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("dyld-load-shlib-symbols", class_obscure, var_boolean, 
		     (char *) &dyld_load_shlib_symbols_flag,
		     "Set if GDB should load symbol information for DYLD-based shared libraries.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("dyld-symbols-prefix", class_obscure, var_string, 
		     (char *) &dyld_symbols_prefix,
		     "Set the prefix that GDB should prepend to all symbols for the dynamic linker.",
		     &setlist);
  add_show_from_set (cmd, &showlist);
  temp = xmalloc (strlen (dyld_symbols_prefix) + 1);
  strcpy (temp, dyld_symbols_prefix);
  dyld_symbols_prefix = temp;

  cmd = add_set_cmd ("dyld-always-resolve-filenames", class_obscure, var_boolean, 
		     (char *) &dyld_always_resolve_filenames_flag,
		     "Set if GDB should use the DYLD interface to determine the names of loaded images.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("dyld-allow-resolve-filenames", class_obscure, var_boolean, 
		     (char *) &dyld_allow_resolve_filenames_flag,
		     "Set if GDB should use the DYLD interface to determine the names of loaded images.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("dyld-always-read-from-memory", class_obscure, var_boolean, 
		     (char *) &dyld_always_read_from_memory_flag,
		     "Set if GDB should always read loaded images from the inferior's memory.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("dyld-remove-overlapping-basenames", class_obscure, var_boolean, 
		     (char *) &dyld_remove_overlapping_basenames_flag,
		     "Set if GDB should remove shared library images with similar basenames.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("dyld-print-basenames", class_obscure, var_boolean,
		     (char *) &dyld_print_basenames_flag,
		     "Set if GDB should print the basenames of loaded files when printing progress messages.",
		     &setlist);
  add_show_from_set (cmd, &showlist);
}
