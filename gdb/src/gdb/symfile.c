/* Generic symbol file reading for the GNU debugger, GDB.

   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

   Contributed by Cygnus Support, using pieces from other GDB modules.

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

#include "defs.h"
#include "bfdlink.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "frame.h"
#include "target.h"
#include "value.h"
#include "symfile.h"
#include "objfiles.h"
#include "source.h"
#include "gdbcmd.h"
#include "breakpoint.h"
#include "language.h"
#include "complaints.h"
#include "demangle.h"
#include "inferior.h"		/* for write_pc */
#include "filenames.h"		/* for DOSish file names */
#include "gdb-stabs.h"
#include "gdb_obstack.h"
#include "completer.h"
#include "bcache.h"
#include "hashtab.h"
#include "readline/readline.h"
#include "gdb_assert.h"
#include "block.h"
#include "observer.h"
#include "libbfd.h"
/* APPLE LOCAL for objfile_changed */
#include "objc-lang.h"
/* APPLE LOCAL exceptions */
#include "exceptions.h"
#include "exec.h"
#include "macosx/macosx-nat-inferior.h"
#include "macosx/macosx-nat-utils.h" /* For macosx_filename_in_bundle.  */
#include "mach-o.h"
#include "osabi.h" /* For gdbarch_lookup_osabi.  */

#include <sys/types.h>
#include <fcntl.h>
#include "gdb_string.h"
#include "gdb_stat.h"
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <libgen.h>

#include <sys/mman.h>

#ifndef TEXT_SECTION_NAME
#define TEXT_SECTION_NAME ".text"
#endif

/* APPLE LOCAL */
#ifndef TEXT_SEGMENT_NAME
#define TEXT_SEGMENT_NAME TEXT_SECTION_NAME
#endif

#ifndef DATA_SECTION_NAME
#define DATA_SECTION_NAME ".data"
#endif

#ifndef BSS_SECTION_NAME
#define BSS_SECTION_NAME ".bss"
#endif

#ifdef MACOSX_DYLD
#include "macosx-nat-dyld.h"
#include "macosx-nat-dyld-process.h"
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#if HAVE_MMAP
static int mmap_symbol_files_flag = 0;
#endif /* HAVE_MMAP */

int (*deprecated_ui_load_progress_hook) (const char *section, unsigned long num);
void (*deprecated_show_load_progress) (const char *section,
			    unsigned long section_sent,
			    unsigned long section_size,
			    unsigned long total_sent,
			    unsigned long total_size);
void (*deprecated_pre_add_symbol_hook) (const char *);
void (*deprecated_post_add_symbol_hook) (void);
void (*deprecated_target_new_objfile_hook) (struct objfile *);

static void clear_symtab_users_cleanup (void *ignore);

/* Global variables owned by this file */
int readnow_symbol_files;	/* Read full symbols immediately */

/* External variables and functions referenced. */

extern void report_transfer_performance (unsigned long, time_t, time_t);

/* Functions this file defines */

#if 0
static int simple_read_overlay_region_table (void);
static void simple_free_overlay_region_table (void);
#endif

static void set_initial_language (void);

static void load_command (char *, int);

static void symbol_file_add_main_1 (char *args, int from_tty, int flags);

static void add_symbol_file_command (char *, int);

static void add_shared_symbol_files_command (char *, int);

static void reread_separate_symbols (struct objfile *objfile);

static void cashier_psymtab (struct partial_symtab *);

/* APPLE LOCAL compare psymbols decl */
static int compare_psymbols (const void *, const void *);

/* APPLE LOCAL symfile bfd open */
bfd *symfile_bfd_open (const char *, int mainline);

int get_section_index (struct objfile *, char *);

static void find_sym_fns (struct objfile *);

static void decrement_reading_symtab (void *);

static void overlay_invalidate_all (void);

static int overlay_is_mapped (struct obj_section *);

void list_overlays_command (char *, int);

void map_overlay_command (char *, int);

void unmap_overlay_command (char *, int);

static void overlay_auto_command (char *, int);

static void overlay_manual_command (char *, int);

static void overlay_off_command (char *, int);

static void overlay_load_command (char *, int);

static void overlay_command (char *, int);

static void simple_free_overlay_table (void);

static void read_target_long_array (CORE_ADDR, unsigned int *, int);

static int simple_read_overlay_table (void);

static int simple_overlay_update_1 (struct obj_section *);

static void add_filename_language (char *ext, enum language lang);

static void info_ext_lang_command (char *args, int from_tty);

/* APPLE LOCAL */
static void add_dsym_command (char *args, int from_tty);

static char *find_separate_debug_file (struct objfile *objfile);
/* This is kind of a hack.  There are too many layers going through
   the symfile code to extract out the dSYM add parts.  So instead,
   we supply this string as the answer to find_separate_debug_file,
   and then just up the load level by hand.  */
static const char *already_found_debug_file = NULL;

static void init_filename_language_table (void);

void _initialize_symfile (void);

/* APPLE LOCAL: Additional directory to search for kext .sym files.
   Should this be a list of directories to search in?  Is there any
   actual need for that?  */
static char *kext_symbol_file_path = NULL;

/* APPLE LOCAL: Need to declare this one:  */
static struct objfile *
symbol_file_add_name_with_addrs_or_offsets_using_objfile (struct objfile *in_objfile,
							  const char *name, 
							  int from_tty,
							  struct section_addr_info *addrs,
							  struct section_offsets *offsets,
							  int num_offsets,
							  int mainline, int flags, int symflags,
							  CORE_ADDR mapaddr, const char *prefix, char *kext_bundle);



/* List of all available sym_fns.  On gdb startup, each object file reader
   calls add_symtab_fns() to register information on each format it is
   prepared to read. */

static struct sym_fns *symtab_fns = NULL;

/* Flag for whether user will be reloading symbols multiple times.
   Defaults to ON for VxWorks, otherwise OFF.  */

#ifdef SYMBOL_RELOADING_DEFAULT
int symbol_reloading = SYMBOL_RELOADING_DEFAULT;
#else
int symbol_reloading = 0;
#endif
static void
show_symbol_reloading (struct ui_file *file, int from_tty,
		       struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Dynamic symbol table reloading multiple times in one run is %s.\n"),
		    value);
}


/* If non-zero, shared library symbols will be added automatically
   when the inferior is created, new libraries are loaded, or when
   attaching to the inferior.  This is almost always what users will
   want to have happen; but for very large programs, the startup time
   will be excessive, and so if this is a problem, the user can clear
   this flag and then add the shared library symbols as needed.  Note
   that there is a potential for confusion, since if the shared
   library symbols are not loaded, commands like "info fun" will *not*
   report all the functions that are actually present. */

int auto_solib_add = 1;

/* For systems that support it, a threshold size in megabytes.  If
   automatically adding a new library's symbol table to those already
   known to the debugger would cause the total shared library symbol
   size to exceed this threshhold, then the shlib's symbols are not
   added.  The threshold is ignored if the user explicitly asks for a
   shlib to be added, such as when using the "sharedlibrary"
   command. */

int auto_solib_limit;


/* This compares two partial symbols by names, using strcmp_iw_ordered
   for the comparison.  */

static int
compare_psymbols (const void *s1p, const void *s2p)
{
  struct partial_symbol *const *s1 = s1p;
  struct partial_symbol *const *s2 = s2p;

  return strcmp_iw_ordered (SYMBOL_SEARCH_NAME (*s1),
			    SYMBOL_SEARCH_NAME (*s2));
}

void
sort_pst_symbols (struct partial_symtab *pst)
{
  /* Sort the global list; don't sort the static list */

  qsort (pst->objfile->global_psymbols.list + pst->globals_offset,
	 pst->n_global_syms, sizeof (struct partial_symbol *),
	 compare_psymbols);
}

/* Make a null terminated copy of the string at PTR with SIZE characters in
   the obstack pointed to by OBSTACKP .  Returns the address of the copy.
   Note that the string at PTR does not have to be null terminated, I.E. it
   may be part of a larger string and we are only saving a substring. */

char *
obsavestring (const char *ptr, int size, struct obstack *obstackp)
{
  char *p = (char *) obstack_alloc (obstackp, size + 1);
  /* Open-coded memcpy--saves function call time.  These strings are usually
     short.  FIXME: Is this really still true with a compiler that can
     inline memcpy? */
  {
    const char *p1 = ptr;
    char *p2 = p;
    const char *end = ptr + size;
    while (p1 != end)
      *p2++ = *p1++;
  }
  p[size] = 0;
  return p;
}

/* Concatenate strings S1, S2 and S3; return the new string.  Space is found
   in the obstack pointed to by OBSTACKP.  */

char *
obconcat (struct obstack *obstackp, const char *s1, const char *s2,
	  const char *s3)
{
  int len = strlen (s1) + strlen (s2) + strlen (s3) + 1;
  char *val = (char *) obstack_alloc (obstackp, len);
  strcpy (val, s1);
  strcat (val, s2);
  strcat (val, s3);
  return val;
}

/* True if we are nested inside psymtab_to_symtab. */

int currently_reading_symtab = 0;

static void
decrement_reading_symtab (void *dummy)
{
  currently_reading_symtab--;
}

/* Get the symbol table that corresponds to a partial_symtab.
   This is fast after the first time you do it.  In fact, there
   is an even faster macro PSYMTAB_TO_SYMTAB that does the fast
   case inline.  */

struct symtab *
psymtab_to_symtab (struct partial_symtab *pst)
{
  /* APPLE LOCAL: This is the psymtab to symtab timer.  */
  static int timer = -1;

  /* If it's been looked up before, return it. */
  if (pst->symtab)
    return pst->symtab;

  /* If it has not yet been read in, read it.  */
  if (!pst->readin)
    {
      struct cleanup *back_to = make_cleanup (decrement_reading_symtab, NULL);
      start_timer (&timer, "psymtab-to-symtab", 
				   pst->fullname ?
				   pst->fullname : pst->filename);

      currently_reading_symtab++;
      (*pst->read_symtab) (pst);
      do_cleanups (back_to);
    }

  return pst->symtab;
}

/* Remember the lowest-addressed loadable section we've seen.
   This function is called via bfd_map_over_sections.

   In case of equal vmas, the section with the largest size becomes the
   lowest-addressed loadable section.

   If the vmas and sizes are equal, the last section is considered the
   lowest-addressed loadable section.  */

void
find_lowest_section (bfd *abfd, asection *sect, void *obj)
{
  asection **lowest = (asection **) obj;

  if (0 == (bfd_get_section_flags (abfd, sect) & SEC_LOAD))
    return;
  if (!*lowest)
    *lowest = sect;		/* First loadable section */
  else if (bfd_section_vma (abfd, *lowest) > bfd_section_vma (abfd, sect))
    *lowest = sect;		/* A lower loadable section */
  else if (bfd_section_vma (abfd, *lowest) == bfd_section_vma (abfd, sect)
	   && (bfd_section_size (abfd, (*lowest))
	       <= bfd_section_size (abfd, sect)))
    *lowest = sect;
}

/* Create a new section_addr_info, with room for NUM_SECTIONS.  */

struct section_addr_info *
alloc_section_addr_info (size_t num_sections)
{
  struct section_addr_info *sap;
  size_t size;

  size = (sizeof (struct section_addr_info)
	  +  sizeof (struct other_sections) * (num_sections - 1));
  sap = (struct section_addr_info *) xmalloc (size);
  memset (sap, 0, size);
  sap->num_sections = num_sections;

  return sap;
}


/* Return a freshly allocated copy of ADDRS.  The section names, if
   any, are also freshly allocated copies of those in ADDRS.  */
struct section_addr_info *
copy_section_addr_info (struct section_addr_info *addrs)
{
  struct section_addr_info *copy
    = alloc_section_addr_info (addrs->num_sections);
  int i;

  copy->num_sections = addrs->num_sections;
  /* APPLE LOCAL */
  copy->addrs_are_offsets = addrs->addrs_are_offsets;
  for (i = 0; i < addrs->num_sections; i++)
    {
      copy->other[i].addr = addrs->other[i].addr;
      if (addrs->other[i].name)
        copy->other[i].name = xstrdup (addrs->other[i].name);
      else
        copy->other[i].name = NULL;
      copy->other[i].sectindex = addrs->other[i].sectindex;
    }

  return copy;
}



/* Build (allocate and populate) a section_addr_info struct from
   an existing section table. */

extern struct section_addr_info *
build_section_addr_info_from_section_table (const struct section_table *start,
                                            const struct section_table *end)
{
  struct section_addr_info *sap;
  const struct section_table *stp;
  int oidx;

  sap = alloc_section_addr_info (end - start);

  for (stp = start, oidx = 0; stp != end; stp++)
    {
      if (bfd_get_section_flags (stp->bfd,
				 stp->the_bfd_section) & (SEC_ALLOC | SEC_LOAD)
	  && oidx < end - start)
	{
	  sap->other[oidx].addr = stp->addr;
	  sap->other[oidx].name
	    = xstrdup (bfd_section_name (stp->bfd, stp->the_bfd_section));
	  sap->other[oidx].sectindex = stp->the_bfd_section->index;
	  oidx++;
	}
    }

  return sap;
}


/* Free all memory allocated by build_section_addr_info_from_section_table. */

extern void
free_section_addr_info (struct section_addr_info *sap)
{
  int idx;

  for (idx = 0; idx < sap->num_sections; idx++)
    if (sap->other[idx].name)
      xfree (sap->other[idx].name);
  xfree (sap);
}


/* Initialize OBJFILE's sect_index_* members.  */
static void
init_objfile_sect_indices (struct objfile *objfile)
{
  asection *sect;
  struct obj_section *osect;
  int i;
  
  /* APPLE LOCAL: You can't actually just grab the bfd section index and
     use that.  That's because we don't always make obj_sections for every
     bfd_section in the objfile.  We only make them for ones that have non-zero
     length.  The sect_index_* refers to the position in the objfile sections,
     so we need to go through there to find the index.  */

  i = 0;
  objfile->sect_index_text = 0;
  ALL_OBJFILE_OSECTIONS (objfile, osect)
    {
      if (strcmp (osect->the_bfd_section->name, TEXT_SEGMENT_NAME) == 0)
	{
	  objfile->sect_index_text = i;
	  break;
	}
      i++;
    }

  i = 0;
  objfile->sect_index_data = 0;
  ALL_OBJFILE_OSECTIONS (objfile, osect)
    {
      if (strcmp (osect->the_bfd_section->name, DATA_SECTION_NAME) == 0)
	{
	  objfile->sect_index_data = i;
	  break;
	}
      i++;
    }

  i = 0;
  objfile->sect_index_bss = 0;
  ALL_OBJFILE_OSECTIONS (objfile, osect)
    {
      if (strcmp (osect->the_bfd_section->name, BSS_SECTION_NAME) == 0)
	{
	  objfile->sect_index_bss = i;
	  break;
	}
      i++;
    }

  /* END APPLE LOCAL */

  sect = bfd_get_section_by_name (objfile->obfd, ".rodata");
  if (sect)
    objfile->sect_index_rodata = sect->index;

  /* This is where things get really weird...  We MUST have valid
     indices for the various sect_index_* members or gdb will abort.
     So if for example, there is no ".text" section, we have to
     accomodate that.  Except when explicitly adding symbol files at
     some address, section_offsets contains nothing but zeros, so it
     doesn't matter which slot in section_offsets the individual
     sect_index_* members index into.  So if they are all zero, it is
     safe to just point all the currently uninitialized indices to the
     first slot. */

  for (i = 0; i < objfile->num_sections; i++)
    {
      if (ANOFFSET (objfile->section_offsets, i) != 0)
	{
	  break;
	}
    }

  
  /* APPLE LOCAL: The dSYM file doesn't actually have bfd sections
     for the sections in the actual file, so we always set these to
     0 and hope that works...  (It's actually arranged so it does work,
     but in a hacky way.  I don't see how to do it in a non-hacky way,
     however, this code is so torturous.)  */

  if (objfile->flags & OBJF_SEPARATE_DEBUG_FILE || i == objfile->num_sections)
    {
      if (objfile->sect_index_text == -1)
	objfile->sect_index_text = 0;
      if (objfile->sect_index_data == -1)
	objfile->sect_index_data = 0;
      if (objfile->sect_index_bss == -1)
	objfile->sect_index_bss = 0;
      if (objfile->sect_index_rodata == -1)
	objfile->sect_index_rodata = 0;
    }
}

/* The arguments to place_section.  */

struct place_section_arg
{
  struct section_offsets *offsets;
  CORE_ADDR lowest;
};

/* Find a unique offset to use for loadable section SECT if
   the user did not provide an offset.  */

void
place_section (bfd *abfd, asection *sect, void *obj)
{
  struct place_section_arg *arg = obj;
  CORE_ADDR *offsets = arg->offsets->offsets, start_addr;
  int done;

  /* We are only interested in loadable sections.  */
  if ((bfd_get_section_flags (abfd, sect) & SEC_LOAD) == 0)
    return;

  /* If the user specified an offset, honor it.  */
  if (offsets[sect->index] != 0)
    return;

  /* Otherwise, let's try to find a place for the section.  */
  do {
    asection *cur_sec;
    ULONGEST align = 1 << bfd_get_section_alignment (abfd, sect);

    start_addr = (arg->lowest + align - 1) & -align;
    done = 1;

    for (cur_sec = abfd->sections; cur_sec != NULL; cur_sec = cur_sec->next)
      {
	int indx = cur_sec->index;

	/* We don't need to compare against ourself.  */
	if (cur_sec == sect)
	  continue;

	/* We can only conflict with loadable sections.  */
	if ((bfd_get_section_flags (abfd, cur_sec) & SEC_LOAD) == 0)
	  continue;

	/* We do not expect this to happen; just ignore sections in a
	   relocatable file with an assigned VMA.  */
	if (bfd_section_vma (abfd, cur_sec) != 0)
	  continue;

	/* If the section offset is 0, either the section has not been placed
	   yet, or it was the lowest section placed (in which case LOWEST
	   will be past its end).  */
	if (offsets[indx] == 0)
	  continue;

	/* If this section would overlap us, then we must move up.  */
	if (start_addr + bfd_get_section_size (sect) > offsets[indx]
	    && start_addr < offsets[indx] + bfd_get_section_size (cur_sec))
	  {
	    start_addr = offsets[indx] + bfd_get_section_size (cur_sec);
	    start_addr = (start_addr + align - 1) & -align;
	    done = 0;
	    continue;
	  }

	/* Otherwise, we appear to be OK.  So far.  */
      }
    }
  while (!done);

  offsets[sect->index] = start_addr;
  arg->lowest = start_addr + bfd_get_section_size (sect);

  exec_set_section_address (bfd_get_filename (abfd), sect->index, start_addr);
}

/* Parse the user's idea of an offset for dynamic linking, into our idea
   of how to represent it for fast symbol reading.  This is the default
   version of the sym_fns.sym_offsets function for symbol readers that
   don't need to do anything special.  It allocates a section_offsets table
   for the objectfile OBJFILE and stuffs ADDR into all of the offsets.  */

void
default_symfile_offsets (struct objfile *objfile,
			 struct section_addr_info *addrs)
{
  int i;

  objfile->num_sections = bfd_count_sections (objfile->obfd);
  objfile->section_offsets = (struct section_offsets *)
    obstack_alloc (&objfile->objfile_obstack,
		   SIZEOF_N_SECTION_OFFSETS (objfile->num_sections));
  memset (objfile->section_offsets, 0,
	  SIZEOF_N_SECTION_OFFSETS (objfile->num_sections));

  /* Now calculate offsets for section that were specified by the
     caller. */
  for (i = 0; i < addrs->num_sections && addrs->other[i].name; i++)
    {
      struct other_sections *osp ;

      osp = &addrs->other[i] ;
      if (osp->addr == 0)
  	continue;

      /* Record all sections in offsets */
      /* The section_offsets in the objfile are here filled in using
         the BFD index. */
      (objfile->section_offsets)->offsets[osp->sectindex] = osp->addr;
    }

  /* For relocatable files, all loadable sections will start at zero.
     The zero is meaningless, so try to pick arbitrary addresses such
     that no loadable sections overlap.  This algorithm is quadratic,
     but the number of sections in a single object file is generally
     small.  */
  if ((bfd_get_file_flags (objfile->obfd) & (EXEC_P | DYNAMIC)) == 0)
    {
      struct place_section_arg arg;
      arg.offsets = objfile->section_offsets;
      arg.lowest = 0;
      bfd_map_over_sections (objfile->obfd, place_section, &arg);
    }

  /* Remember the bfd indexes for the .text, .data, .bss and
     .rodata sections. */
  init_objfile_sect_indices (objfile);
}


/* Process a symbol file, as either the main file or as a dynamically
   loaded file.

   OBJFILE is where the symbols are to be read from.

   ADDRS is the list of section load addresses.  If the user has given
   an 'add-symbol-file' command, then this is the list of offsets and
   addresses he or she provided as arguments to the command; or, if
   we're handling a shared library, these are the actual addresses the
   sections are loaded at, according to the inferior's dynamic linker
   (as gleaned by GDB's shared library code).  We convert each address
   into an offset from the section VMA's as it appears in the object
   file, and then call the file's sym_offsets function to convert this
   into a format-specific offset table --- a `struct section_offsets'.
   If ADDRS is non-zero, OFFSETS must be zero.

   OFFSETS is a table of section offsets already in the right
   format-specific representation.  NUM_OFFSETS is the number of
   elements present in OFFSETS->offsets.  If OFFSETS is non-zero, we
   assume this is the proper table the call to sym_offsets described
   above would produce.  Instead of calling sym_offsets, we just dump
   it right into objfile->section_offsets.  (When we're re-reading
   symbols from an objfile, we don't have the original load address
   list any more; all we have is the section offset table.)  If
   OFFSETS is non-zero, ADDRS must be zero.

   MAINLINE is nonzero if this is the main symbol file, or zero if
   it's an extra symbol file such as dynamically loaded code.

   VERBO is nonzero if the caller has printed a verbose message about
   the symbol reading (and complaints can be more terse about it).  */

void
syms_from_objfile (struct objfile *objfile,
                   struct section_addr_info *addrs,
                   struct section_offsets *offsets,
                   int num_offsets,
		   int mainline,
                   int verbo)
{
  struct section_addr_info *local_addr = NULL;
  struct cleanup *old_chain;

  gdb_assert (! (addrs && offsets));

  init_entry_point_info (objfile);

  /* The objfile might slide and so the entry point info from the bfd
     might be wrong.  */
  if (objfile->ei.entry_point != ~(CORE_ADDR) 0
      && objfile->ei.entry_point != 0)
    {
      if (offsets != NULL)
	{
	  struct obj_section *s;
	  int found = 0;

	  /* Look the section up directly in this objfile, since you
	     wouldn't want to find it in another one that overlapped
	     this one.  */
	  ALL_OBJFILE_OSECTIONS (objfile, s)
	    {
	      if (s->addr <= objfile->ei.entry_point 
		  && objfile->ei.entry_point < s->endaddr)
		{
		  found = 1;
		  break;
		}
	    }

	  /* If we didn't find the section I'm just going to leave the
	     start address alone.  I don't know what it would mean for
	     the start address not to be in its objfile's sections.  */
	  if (found)
	    objfile->ei.entry_point += ANOFFSET (offsets, s->the_bfd_section->index);
	}
      else if (addrs != NULL)
	{
	  /* FIXME: When this gets run from dyld_load_symfile_internal
	     we get an addrs array with no names.  That code should really
	     use offsets not addrs since it's not taking advantage of the
	     only point of the addrs - than names...  But I don't want to
	     muck with that right now, so I'll assume a rigid offset.  */
	  if (addrs->addrs_are_offsets == 1 && addrs->num_sections > 0)
	      objfile->ei.entry_point += addrs->other[0].addr;
	}
    }

  find_sym_fns (objfile);

  if (objfile->sf == NULL)
    return;	/* No symbols. */

  /* Make sure that partially constructed symbol tables will be cleaned up
     if an error occurs during symbol reading.  */
  old_chain = make_cleanup_free_objfile (objfile);

  /* If ADDRS and OFFSETS are both NULL, put together a dummy address
     list.  We now establish the convention that an addr of zero means
     no load address was specified. */
  if (! addrs && ! offsets)
    {
      local_addr
	= alloc_section_addr_info (bfd_count_sections (objfile->obfd));
      make_cleanup (xfree, local_addr);
      addrs = local_addr;
      /* APPLE LOCAL huh? */
      addrs -> addrs_are_offsets = 1;
    }

  /* Now either addrs or offsets is non-zero.  */

  if (mainline)
    {
      /* We will modify the main symbol table, make sure that all its users
         will be cleaned up if an error occurs during symbol reading.  */
      make_cleanup (clear_symtab_users_cleanup, 0 /*ignore*/);

      /* Since no error yet, throw away the old symbol table.  */

      if (symfile_objfile != NULL)
	{
	  free_objfile (symfile_objfile);
	  symfile_objfile = NULL;
#ifdef MACOSX_DYLD
	  macosx_init_dyld_symfile (symfile_objfile, exec_bfd);
#endif
	}

      /* Currently we keep symbols from the add-symbol-file command.
         If the user wants to get rid of them, they should do "symbol-file"
         without arguments first.  Not sure this is the best behavior
         (PR 2207).  */

      (*objfile->sf->sym_new_init) (objfile);
    }

  /* Convert addr into an offset rather than an absolute address.
     We find the lowest address of a loaded segment in the objfile,
     and assume that <addr> is where that got loaded.

     We no longer warn if the lowest section is not a text segment (as
     happens for the PA64 port.  */
  if (addrs)
    {
      if (mainline)
	addrs -> addrs_are_offsets = 1;
      if (!addrs->addrs_are_offsets)
	{
	  asection *lower_sect;
	  asection *sect;
	  CORE_ADDR lower_offset;
	  int i;
	  
	  /* APPLE LOCAL */
	  gdb_assert (addrs->other[0].name);
	  
	  /* Find lowest loadable section to be used as starting point for
	     continguous sections. FIXME!! won't work without call to find
	     .text first, but this assumes text is lowest section. */
	  /* APPLE LOCAL: Look for the text segment ("__TEXT"), not the section
	     ("__TEXT.__text") because what we're really looking for is the load
	     address of the image, and the section address is offset a bit. */
	  lower_sect = bfd_get_section_by_name (objfile->obfd, TEXT_SEGMENT_NAME);
	  if (lower_sect == NULL)
	    bfd_map_over_sections (objfile->obfd, find_lowest_section,
				   &lower_sect);
	  if (lower_sect == NULL)
	    warning (_("no loadable sections found in added symbol-file %s"),
		     objfile->name);
	  else
	    if ((bfd_get_section_flags (objfile->obfd, lower_sect) & SEC_CODE) == 0)
	      warning (_("Lowest section in %s is %s at %s"),
		       objfile->name,
		       bfd_section_name (objfile->obfd, lower_sect),
		       paddr (bfd_section_vma (objfile->obfd, lower_sect)));
	  if (lower_sect != NULL)
	    lower_offset = bfd_section_vma (objfile->obfd, lower_sect);
	  else
	    lower_offset = 0;
	  
	  /* Calculate offsets for the loadable sections.
	     FIXME! Sections must be in order of increasing loadable section
	     so that contiguous sections can use the lower-offset!!!
	     
	     Adjust offsets if the segments are not contiguous.
	     If the section is contiguous, its offset should be set to
	     the offset of the highest loadable section lower than it
	     (the loadable section directly below it in memory).
	     this_offset = lower_offset = lower_addr - lower_orig_addr */
	  
	  for (i = 0; i < addrs->num_sections && addrs->other[i].name; i++)
	    {
	      if (addrs->other[i].addr != 0)
		{
		  sect = bfd_get_section_by_name (objfile->obfd,
						  addrs->other[i].name);
		  if (sect)
		    {
		      addrs->other[i].addr
			-= bfd_section_vma (objfile->obfd, sect);
		      lower_offset = addrs->other[i].addr;
		      /* This is the index used by BFD. */
		      addrs->other[i].sectindex = sect->index ;
		    }
		  else
		    {
		      warning (_("section %s not found in %s"),
			       addrs->other[i].name,
			       objfile->name);
		      addrs->other[i].addr = 0;
		    }
		}
	      else
		addrs->other[i].addr = lower_offset;
	    }
	  /* We've now converted the addrs struct from a series of 
             absolute addresses to a series of offsets from where the
             file was supposed to load.  */
	  addrs->addrs_are_offsets = 1;
	}
    }

  /* Initialize symbol reading routines for this objfile, allow complaints to
     appear for this new file, and record how verbose to be, then do the
     initial symbol reading for this file. */

  (*objfile->sf->sym_init) (objfile);
  clear_complaints (&symfile_complaints, 1, verbo);
  
  /* APPLE LOCAL: Since we might be changing the section 
     offsets for the objfile, we need to delete it from the
     ordered sections array & put it back in later.  */

  objfile_delete_from_ordered_sections (objfile);
  if (addrs)
    (*objfile->sf->sym_offsets) (objfile, addrs);
  else
    {
      struct obj_section *osect;
      int idx = 0;
      size_t size = SIZEOF_N_SECTION_OFFSETS (num_offsets);

      /* Just copy in the offset table directly as given to us.  */
      objfile->num_sections = num_offsets;
      objfile->section_offsets
        = ((struct section_offsets *)
           obstack_alloc (&objfile->objfile_obstack, size));
      memcpy (objfile->section_offsets, offsets, size);

      init_objfile_sect_indices (objfile);
      /* APPLE LOCAL: I'm unclear where the section_offsets are
	 supposed to actually get applied to the sections.  If you read
	 in from a disk file, but the library itself isn't loaded at that
	 address, you either have to call objfile_relocate here, or you 
	 need to relocate the sections somewhere.  Note that in the macosx
	 code this used to be somewhat bogusly done in macho_symfile_offsets.  */
      ALL_OBJFILE_OSECTIONS (objfile, osect)
	{
	  /* There's another tricky bit here, which is that we only 
	     add offsets to the sections offset array if the section
	     is loaded.  Fortunately, the osects are created in the same
	     order as the bfd sections (just omitting the ones that don't
	     get loaded) and the sections_offsets are created in the same 
	     way.  So we have to use a continuous index here rather than the
	     bfd_sections's index.  */

	  CORE_ADDR offset = objfile_section_offset (objfile, idx);
	  idx += 1;
	  osect->addr += offset;
	  osect->endaddr += offset;
	}
    }
  objfile_add_to_ordered_sections (objfile);

#ifndef DEPRECATED_IBM6000_TARGET
  /* This is a SVR4/SunOS specific hack, I think.  In any event, it
     screws RS/6000.  sym_offsets should be doing this sort of thing,
     because it knows the mapping between bfd sections and
     section_offsets.  */
  /* This is a hack.  As far as I can tell, section offsets are not
     target dependent.  They are all set to addr with a couple of
     exceptions.  The exceptions are sysvr4 shared libraries, whose
     offsets are kept in solib structures anyway and rs6000 xcoff
     which handles shared libraries in a completely unique way.

     Section offsets are built similarly, except that they are built
     by adding addr in all cases because there is no clear mapping
     from section_offsets into actual sections.  Note that solib.c
     has a different algorithm for finding section offsets.

     These should probably all be collapsed into some target
     independent form of shared library support.  FIXME.  */

  /* APPLE LOCAL: As a side note, on Mac OS X, offsetting the obj_sections
     happens in macho_symfile_read.  If we were ever to offset the sections
     here as well, that would be bad.  But it turns out that we always use
     ADDRS that have NULL name fields.  So this loop turns into "compare a
     bunch of bfd section names to NULL, then add 0 to a bunch of obj_section
     fields.  FIXME: this actually SHOULD get used if somebody does 
     "add-symbol-file" with section offsets.  Otherwise, we should just 
     remove it...  */

  if (addrs)
    {
      struct obj_section *s;

 	/* Map section offsets in "addr" back to the object's
 	   sections by comparing the section names with bfd's
 	   section names.  Then adjust the section address by
 	   the offset. */ /* for gdb/13815 */

      ALL_OBJFILE_OSECTIONS (objfile, s)
	{
	  CORE_ADDR s_addr = 0;
	  int i;

 	    for (i = 0;
	         !s_addr && i < addrs->num_sections && addrs->other[i].name;
		 i++)
 	      if (strcmp (bfd_section_name (s->objfile->obfd,
					    s->the_bfd_section),
			  addrs->other[i].name) == 0)
 	        s_addr = addrs->other[i].addr; /* end added for gdb/13815 */

	  s->addr -= s->offset;
	  s->addr += s_addr;
	  s->endaddr -= s->offset;
	  s->endaddr += s_addr;
	  s->offset += s_addr;
	}
    }
#endif /* not DEPRECATED_IBM6000_TARGET */

  /* APPLE LOCAL If our load level is higher than container, call
     symfile read fn.  */
  if ((objfile->symflags & ~OBJF_SYM_CONTAINER) & OBJF_SYM_LEVELS_MASK)
    (*objfile->sf->sym_read) (objfile, mainline);

  /* Don't allow char * to have a typename (else would get caddr_t).
     Ditto void *.  FIXME: Check whether this is now done by all the
     symbol readers themselves (many of them now do), and if so remove
     it from here.  */

  TYPE_NAME (lookup_pointer_type (builtin_type_char)) = 0;
  TYPE_NAME (lookup_pointer_type (builtin_type_void)) = 0;

  /* Mark the objfile has having had initial symbol read attempted.  Note
     that this does not mean we found any symbols... */

  objfile->flags |= OBJF_SYMS;

  /* Discard cleanups as symbol reading was successful.  */

  discard_cleanups (old_chain);
}

/* Perform required actions after either reading in the initial
   symbols for a new objfile, or mapping in the symbols from a reusable
   objfile. */

void
new_symfile_objfile (struct objfile *objfile, int mainline, int verbo)
{

  /* If this is the main symbol file we have to clean up all users of the
     old main symbol file. Otherwise it is sufficient to fixup all the
     breakpoints that may have been redefined by this symbol file.  */
  if (mainline)
    {
      /* OK, make it the "real" symbol file.  */
      symfile_objfile = objfile;
#ifdef MACOSX_DYLD
      macosx_init_dyld_symfile (symfile_objfile, exec_bfd);
#endif
      breakpoint_re_set (objfile);
      clear_symtab_users ();
    }
  else
    {
      breakpoint_re_set (objfile);
    }

  /* We're done reading the symbol file; finish off complaints.  */
  clear_complaints (&symfile_complaints, 0, verbo);
}

/* APPLE LOCAL begin */
static int
check_bfd_for_matching_uuid (bfd *exe_bfd, bfd *dbg_bfd)
{
  /* Compare the UUID's of the object and debug files.  */
  unsigned char exe_uuid[16];
  unsigned char dbg_uuid[16];
  
  /* Make sure the UUID of the object file and the separate debug file match
     and that they exist properly. If they do match correctly, then return
     without removing the new debug file, else remove the separate debug 
     file because it doesn't match.  */
  if (bfd_mach_o_get_uuid (exe_bfd, exe_uuid, sizeof (exe_uuid)) &&
      bfd_mach_o_get_uuid (dbg_bfd, dbg_uuid, sizeof (dbg_uuid)) &&
      (memcmp (exe_uuid, dbg_uuid, sizeof (exe_uuid)) == 0))
    return 1; /* The UUIDs match, nothing needs to be done.  */
  
  warning (_("UUID mismatch detected between:\n\t%s\n\t%s..."), 
	   exe_bfd->filename, dbg_bfd->filename);
  if (ui_out_is_mi_like_p (uiout))
    {
      struct cleanup *notify_cleanup =
	make_cleanup_ui_out_notify_begin_end (uiout, "uuid-mismatch");
      ui_out_field_string (uiout, "file", exe_bfd->filename);
      ui_out_field_string (uiout, "debug-file", 
			   dbg_bfd->filename);
      do_cleanups (notify_cleanup);
    }
  return 0;
  
}

/* Prefix NAME with the prefix for OBJFILE only if needed. The new string is
   allocated on the object stack.  */
static const char *
add_objfile_prefix (struct objfile *objfile, const char* name)
{
  char* prefixed_name;
  size_t prefixed_name_length;
  /* Check to see if we need to add a prefix?  */
  if (name && name[0] && objfile && objfile->prefix && objfile->prefix[0])
    {
      /* Get length for leading char, prefix, name and NULL terminator.  */
      prefixed_name_length = strlen (objfile->prefix) + strlen (name) + 1;
      prefixed_name = (char*) obstack_alloc (&objfile->objfile_obstack,
					     prefixed_name_length);

      sprintf (prefixed_name, "%s%s", objfile->prefix, name);

      return prefixed_name;		
    }
  
  /* No prefix needed.  */
  return name;
}

/* Search OBJFILE's section list for the section with the smallest address 
   range that still contains ADDR.  */
static struct obj_section *
find_section_for_addr (struct objfile *objfile, CORE_ADDR addr)
{
  struct obj_section *curr_osect;
  struct obj_section *best_osect = NULL;
  ALL_OBJFILE_OSECTIONS (objfile, curr_osect)
    {
      if (curr_osect->addr <= addr && addr < curr_osect->endaddr)
	{
	  if (best_osect)
	    {
	      if ((curr_osect->endaddr - curr_osect->addr) < 
		  (best_osect->endaddr - best_osect->addr))
		best_osect = curr_osect;
	    }
	  else
	    best_osect = curr_osect;
	}
    }
  return best_osect;
}

/* Add msymbols in original executable objfile by using psymbols from the
   separate debug objfile (dSYM). We do this to keep stepping code happy since
   it relies on msymbols and the main executable could have been completely
   stripped.  */
void
append_psymbols_as_msymbols (struct objfile *objfile)
{
  int i;
  struct partial_symbol *psym;
  struct partial_symtab *psymtab;
  struct obj_section *psym_osect;
  CORE_ADDR psym_addr;
  const char* psym_linkage_name;
  struct objfile *dsym_objfile = objfile->separate_debug_objfile;
  struct cleanup *back_to;
  int add_prefix = objfile->prefix && objfile->prefix[0];
  /* Initialize a new block of msymbols and create a cleanup.  */
  
  init_minimal_symbol_collection ();
  back_to = make_cleanup_discard_minimal_symbols ();

  /* Iterate through all psymtabs within the dSYM objfile.  */
  ALL_OBJFILE_PSYMTABS (dsym_objfile, psymtab)
    {
      /* Append msymbols for all global partial symbols that are functions.  */
      for (i = 0; i < psymtab->n_global_syms; i++)
	{
	  psym = dsym_objfile->global_psymbols.list[psymtab->globals_offset + i];
	  psym_osect = NULL;
	  if (PSYMBOL_DOMAIN (psym) == VAR_DOMAIN
	      && PSYMBOL_CLASS (psym) == LOC_BLOCK)
	    {
	      psym_addr = SYMBOL_VALUE_ADDRESS (psym);
	      psym_osect = find_section_for_addr (objfile, psym_addr);
	      
	      if (psym_osect != NULL)
		{
		  struct minimal_symbol *msym;
		  
		  /* Don't overwrite an extant msym, since we might lose information
		     that isn't known in the psymbol (like the "is_special" info.)  */
		  msym = lookup_minimal_symbol_by_pc_section_from_objfile (psym_addr, 
									   psym_osect->the_bfd_section, 
									   objfile);
		  if (msym != NULL
		      && SYMBOL_VALUE_ADDRESS (msym) == psym_addr)
		    continue;

		  if (add_prefix)
		    {
		      psym_linkage_name = 
					add_objfile_prefix (objfile, 
							    SYMBOL_LINKAGE_NAME (psym));
		    }
		  else
		    psym_linkage_name = SYMBOL_LINKAGE_NAME (psym);
							  
		  prim_record_minimal_symbol_and_info (psym_linkage_name,
						       psym_addr,
						       mst_text,
						       NULL,
						       SECT_OFF_TEXT (objfile),
						       psym_osect->the_bfd_section,
						       objfile);
		}
	    }
	}

      /* Append msymbols for all static partial symbols that are functions.  */
      for (i = 0; i < psymtab->n_static_syms; i++)
	{
	  psym = dsym_objfile->static_psymbols.list[psymtab->statics_offset + i];
	  if (PSYMBOL_DOMAIN (psym) == VAR_DOMAIN
	      && PSYMBOL_CLASS (psym) == LOC_BLOCK)
	    {
	      psym_addr = SYMBOL_VALUE_ADDRESS (psym);
	      psym_osect = find_section_for_addr (objfile, psym_addr);

	      if (psym_osect != NULL)
		{
		  struct minimal_symbol *msym;
		  
		  /* Don't overwrite an extant msym, since we might lose information
		     that isn't known in the psymbol (like the "is_special" info.  */
		  msym = lookup_minimal_symbol_by_pc_section_from_objfile (psym_addr, 
									   psym_osect->the_bfd_section, 
									   objfile);
		  if (msym != NULL
		      && SYMBOL_VALUE_ADDRESS (msym) == psym_addr)
		    continue;

		  if (add_prefix)
		    {
		      psym_linkage_name = 
					add_objfile_prefix (objfile, 
							    SYMBOL_LINKAGE_NAME (psym));
		    }
		  else
		    psym_linkage_name = SYMBOL_LINKAGE_NAME (psym);
							  
							  
		  prim_record_minimal_symbol_and_info (psym_linkage_name,
						       psym_addr,
						       mst_file_text,
						       NULL,
						       SECT_OFF_TEXT (objfile),
						       psym_osect->the_bfd_section,
						       objfile);
		}
	    }
	}
    }
    
  install_minimal_symbols (objfile);
  do_cleanups (back_to);
}

/* HACK: I copy the private 'struct symloc' definition from dbxread.c
   because we'll be copying struct symloc structures here in 
   replace_psymbols_with_correct_psymbols () ... */

struct dbxread_symloc
  {
    /* Offset within the file symbol table of first local symbol for this
       file.  */

    int ldsymoff;

    /* Length (in bytes) of the section of the symbol table devoted to
       this file's symbols (actually, the section bracketed may contain
       more than just this file's symbols).  If ldsymlen is 0, the only
       reason for this thing's existence is the dependency list.  Nothing
       else will happen when it is read in.  */

    int ldsymlen;

    /* The size of each symbol in the symbol file (in external form).  */

    int symbol_size;

    /* Further information needed to locate the symbols if they are in
       an ELF file.  */

    int symbol_offset;
    int string_offset;
    int file_string_offset;

    const char *prefix;

};
#define LDSYMOFF(p) (((struct dbxread_symloc *)((p)->read_symtab_private))->ldsymoff)
#define LDSYMLEN(p) (((struct dbxread_symloc *)((p)->read_symtab_private))->ldsymlen)
#define SYMLOC(p) ((struct dbxread_symloc *)((p)->read_symtab_private))
#define SYMBOL_SIZE(p) (SYMLOC(p)->symbol_size)
#define SYMBOL_OFFSET(p) (SYMLOC(p)->symbol_offset)
#define STRING_OFFSET(p) (SYMLOC(p)->string_offset)
#define FILE_STRING_OFFSET(p) (SYMLOC(p)->file_string_offset)
#define SYMBOL_PREFIX(p) (SYMLOC(p)->prefix)


/* When debugging a kext with a dSYM we have three components:
      1. The kext bundle executable, output by ld -r but whose addresses
         are all 0-based, i.e. not final.
      2. The dSYM for the kext bundle which also has 0-based addresses.
      3. The kextutil-generated symbol file when the kext is linked and
         loaded into the kernel.  The debug map in this executable has
         the correct final addresses.

  This function is called with EXE_OBJ set to be item #3.
  The path to the binary for item #1 is in EXE_OBJ->not_loaded_kext_filename.
  Item #2 is in EXE_OBJ->separate_debug_objfile.

  The dSYM's psymtabs have addresses from the DWARF, i.e. the 0-based 
  ld -r output addresses, and are therefore useless.  We'll toss them
  and do the psymtab_to_symtab expansion as if we were working with a
  non-dSYM file and got the psymtabs from the debug map entries.  */

void
replace_psymbols_with_correct_psymbols (struct objfile *exe_obj)
{
  struct objfile *dsym_obj = exe_obj->separate_debug_objfile;
  struct partial_symtab *exe_pst;
  struct partial_symtab *dsym_pst;

  dsym_obj->not_loaded_kext_filename = exe_obj->not_loaded_kext_filename;

  /* FIXME: I'm going to leak all of the existing psymtabs and symbol
     caches in DSYM_OBJ just because I can't convince myself that nothing
     will still be pointing into them.  This shouldn't be much -- we're
     talking about kexts -- but still it's bad.  reread_symbols() might
     provide a good example of what is possible to do.  */

  dsym_obj->psymtabs = NULL;

  ALL_OBJFILE_PSYMTABS (exe_obj, exe_pst)
    {
      struct partial_symbol **psym;
      int i;

      /* The psymtab initialization cribbed from dbxread.c:start_psymtab()  */
      dsym_pst = start_psymtab_common (dsym_obj, dsym_obj->section_offsets,
                                       exe_pst->filename, exe_pst->textlow,
                                       dsym_obj->global_psymbols.next,
                                       dsym_obj->static_psymbols.next);
      dsym_pst->read_symtab_private = (char *)
        obstack_alloc (&dsym_obj->objfile_obstack, 
                       sizeof (struct dbxread_symloc));

      LDSYMOFF (dsym_pst) = LDSYMOFF (exe_pst);

      dsym_pst->read_symtab = dwarf2_kext_psymtab_to_symtab;
      SYMBOL_SIZE (dsym_pst) = SYMBOL_SIZE (exe_pst);
      SYMBOL_OFFSET (dsym_pst) = SYMBOL_OFFSET (exe_pst);
      STRING_OFFSET (dsym_pst) = STRING_OFFSET (exe_pst);
      FILE_STRING_OFFSET (dsym_pst) = FILE_STRING_OFFSET (exe_pst);
      /* APPLE LOCAL symbol prefixes */
      SYMBOL_PREFIX (dsym_pst) = SYMBOL_PREFIX (exe_pst);

      dsym_pst->language = exe_pst->language;
      dsym_pst->texthigh = exe_pst->texthigh;
      dsym_pst->textlow = exe_pst->textlow;
      PSYMTAB_OBSOLETED (dsym_pst) = PSYMTAB_OBSOLETED (exe_pst);
      
      psym = exe_pst->objfile->global_psymbols.list + exe_pst->globals_offset;
      for (i = 0; i < exe_pst->n_global_syms; i++, psym++)
        {
          add_psymbol_to_list (DEPRECATED_SYMBOL_NAME (*psym),
                               strlen (DEPRECATED_SYMBOL_NAME (*psym)),
                               SYMBOL_DOMAIN (*psym),
                               SYMBOL_CLASS (*psym),
                               &dsym_pst->objfile->global_psymbols,
                               0,
                               SYMBOL_VALUE_ADDRESS (*psym),
                               SYMBOL_LANGUAGE (*psym),
                               dsym_pst->objfile);
        }
      dsym_pst->n_global_syms =
        dsym_obj->global_psymbols.next - 
             (dsym_obj->global_psymbols.list + dsym_pst->globals_offset);
      
      psym = exe_pst->objfile->static_psymbols.list + exe_pst->statics_offset;
      for (i = 0; i< exe_pst->n_static_syms; i++, psym++)
        {
          add_psymbol_to_list (DEPRECATED_SYMBOL_NAME (*psym),
                               strlen (DEPRECATED_SYMBOL_NAME (*psym)),
                               SYMBOL_DOMAIN (*psym),
                               SYMBOL_CLASS (*psym),
                               &dsym_pst->objfile->static_psymbols,
                               0,
                               SYMBOL_VALUE_ADDRESS (*psym),
                               SYMBOL_LANGUAGE (*psym),
                               dsym_pst->objfile);
        }
      dsym_pst->n_static_syms =
        dsym_obj->static_psymbols.next - 
             (dsym_obj->static_psymbols.list + dsym_pst->statics_offset);

      
      if (PSYMTAB_OSO_NAME (exe_pst))
        PSYMTAB_OSO_NAME (dsym_pst) = xstrdup (PSYMTAB_OSO_NAME (exe_pst));
      PSYMTAB_OSO_MTIME (dsym_pst) = PSYMTAB_OSO_MTIME (exe_pst);

      // TODO deal with dependencies

      // TODO includes?
    } 

  tell_breakpoints_objfile_changed (dsym_obj);
  tell_objc_msgsend_cacher_objfile_changed (dsym_obj);
  /* APPLE LOCAL cache lookup values for improved performance  */
  symtab_clear_cached_lookup_values ();
}


/* APPLE LOCAL end */

/* APPLE LOCAL: I split out most of symbol_file_add_with_addrs_or_offsets
   so that I could "edit objfiles in place"  when raising symbol load level.  
   
   Arguments have the same meaning as symbol_file_add_with_addrs_or_offsets,
   with the addition of IN_OBJFILE, which if non-null will be reused for this
   symbol file.  This assumes you have already run clear_objfile on IN_OBJFILE.  */

struct objfile *
symbol_file_add_with_addrs_or_offsets_using_objfile (struct objfile *in_objfile,
				     bfd *abfd, int from_tty,
				     struct section_addr_info *addrs,
				     struct section_offsets *offsets,
				     int num_offsets,
				     /* APPLE LOCAL symflags */
				     int mainline, int flags, int symflags,
				     /* APPLE LOCAL symbol prefixes */
				     CORE_ADDR mapaddr, const char *prefix,
                                     char *not_loaded_kext_bundle)
{
  struct objfile *objfile;
  struct partial_symtab *psymtab;
  char *debugfile;
  struct section_addr_info *orig_addrs = NULL;
  struct cleanup *my_cleanups;
  const char *name = bfd_get_filename (abfd);
  int using_orig_objfile = (in_objfile != NULL);
  my_cleanups = make_cleanup_bfd_close (abfd);

  /* Give user a chance to burp if we'd be
     interactively wiping out any existing symbols.  */

  if ((symfile_objfile != NULL)
      && mainline
      && from_tty
      && !query ("Load new symbol table from \"%s\"? ", name))
    error (_("Not confirmed."));

  if (!using_orig_objfile)
    objfile = allocate_objfile (abfd, flags, symflags, mapaddr, prefix);
  else 
    {
      allocate_objfile_using_objfile (in_objfile, abfd, flags, symflags, 
                                      mapaddr, prefix);
      objfile = in_objfile;
    }
  discard_cleanups (my_cleanups);

  objfile->prefix = prefix;
  if (not_loaded_kext_bundle)
    objfile->not_loaded_kext_filename = xstrdup (not_loaded_kext_bundle);

  orig_addrs = alloc_section_addr_info (bfd_count_sections (abfd));
  my_cleanups = make_cleanup (xfree, orig_addrs);

  if (addrs)
    {
      orig_addrs = copy_section_addr_info (addrs);
      make_cleanup_free_section_addr_info (orig_addrs);
    }

  /* We either created a new mapped symbol table, mapped an existing
     symbol table file which has not had initial symbol reading
     performed, or need to read an unmapped symbol table. */
  if (from_tty || info_verbose)
    {
      if (deprecated_pre_add_symbol_hook)
	deprecated_pre_add_symbol_hook (name);
      else
	{
	  printf_unfiltered (_("Reading symbols from %s..."), name);
	  wrap_here ("");
	  gdb_flush (gdb_stdout);
	}
    }
  
  /* APPLE LOCAL: Move the finding and parsing of dSYM files before
     syms_from_objfile() for the main objfile.  If there is a dSYM file
     and it's UUID matches, we can avoid parsing any debug map symbols
     since they will all be contained in the dSYM file. Problems can
     arise when we have both debug map information and dSYM information
     where certain line numbers will be found in both sets of information,
     yet a lookup based on an address for something found in the debug map
     will always be found in the dSYM information first and can cause a
     variety of mismatches.  */
  
  if (!(flags & OBJF_SEPARATE_DEBUG_FILE))
    {
      debugfile = find_separate_debug_file (objfile);
      if (debugfile)
	{
	  struct section_addr_info *addrs_to_use;
	  struct section_offsets *sym_offsets = NULL;
	  int num_sym_offsets = 0;
	  bfd *debug_bfd;
	  int uuid_matches;

	  if (addrs != NULL)
	    addrs_to_use = orig_addrs;
	  else
	    addrs_to_use = NULL;
	  
	  /* APPLE LOCAL: Add OBJF_SEPARATE_DEBUG_FILE */
          debug_bfd = symfile_bfd_open (debugfile, mainline);

	  /* Don't bother to make the debug_objfile if the UUID's don't
	     match.  */
	  if (objfile->not_loaded_kext_filename)
	    /* FIXME will kextutil -s copy the uuid over to the output
	       binary?  Drop it?  Modify it?  That will determine what
	       should be done here.  Right now kextutil drops it.  
	       NB we have the original unloaded kext over in
	       objfile->not_loaded_kext_filename and we could try to
	       match that file's UUID with the dSYM's.  */
	    uuid_matches = 1;
	  else
	    uuid_matches = check_bfd_for_matching_uuid (objfile->obfd, 
                                                        debug_bfd);

	  if (uuid_matches)
	    {
#ifdef TM_NEXTSTEP
	      /* This should really be a symbol file reader function to go 
                 along with sym_offsets, but I don't want to have to push 
                 all the changes through for that right now.  Note, if we've 
                 called into here, we're using offsets, not the addrs_to_use, 
                 so NULL that out.  */
              /* Don't calculate the offset between the executable and the dSYM
                 if we're dealing with a kext - we'll fix each symbol's address
                 individually.  */
              if (not_loaded_kext_bundle == 0)
	        macho_calculate_offsets_for_dsym (objfile, debug_bfd, 
                                             addrs_to_use, offsets, num_offsets,
					     &sym_offsets, &num_sym_offsets);
	      addrs_to_use = NULL;
#endif /* TM_NEXTSTEP */
	      
	      objfile->separate_debug_objfile
		= symbol_file_add_with_addrs_or_offsets_using_objfile 
                                               (objfile->separate_debug_objfile,
					       debug_bfd, from_tty, 
					       addrs_to_use, 
					       sym_offsets, num_sym_offsets, 0, 
					       flags | OBJF_SEPARATE_DEBUG_FILE,
					       symflags, mapaddr, prefix,
					       not_loaded_kext_bundle);
	      
	      objfile->separate_debug_objfile->separate_debug_objfile_backlink
		= objfile;
	      
	      /* Put the separate debug object before the normal one, this is so
		 that usage of the ALL_OBJFILES_SAFE macro will stay safe. */
	      put_objfile_before (objfile->separate_debug_objfile, objfile);
	      
	    }
	  else
	    bfd_close (debug_bfd);

	  xfree (debugfile);
	}
    }

  syms_from_objfile (objfile, addrs, offsets, num_offsets,
		     mainline, from_tty);

  /* We now have at least a partial symbol table.  Check to see if the
     user requested that all symbols be read on initial access via either
     the gdb startup command line or on a per symbol file basis.  Expand
     all partial symbol tables for this objfile if so. */

  if ((flags & OBJF_READNOW) || readnow_symbol_files)
    {
      if (from_tty || info_verbose)
	{
	  printf_unfiltered (_("expanding to full symbols..."));
	  wrap_here ("");
	  gdb_flush (gdb_stdout);
	}

      /* APPLE LOCAL ALL_OBJFILE_PSYMTABS */
      ALL_OBJFILE_PSYMTABS (objfile, psymtab)
	{
	  psymtab_to_symtab (psymtab);
	}
    }


  /* APPLE LOCAL skip no debug symbols warning? */

  if (from_tty || info_verbose)
    {
      if (deprecated_post_add_symbol_hook)
	deprecated_post_add_symbol_hook ();
      else
	{
	  printf_unfiltered (_("done.\n"));
	}
    }

  /* We print some messages regardless of whether 'from_tty ||
     info_verbose' is true, so make sure they go out at the right
     time.  */
  gdb_flush (gdb_stdout);

  do_cleanups (my_cleanups);

#ifndef MACOSX_DYLD
  if (objfile->sf == NULL)
    return objfile;	/* No symbols. */
#endif

  new_symfile_objfile (objfile, mainline, from_tty);

  /* APPLE LOCAL: Add all partial symbols from dSYM file to main executable
     in case it has been stripped. Stepping is very unhappy without 
     msymbols.  */
  if (objfile->separate_debug_objfile)
    append_psymbols_as_msymbols (objfile);  

  /* If OBJFILE is a kext binary that has final linked addresses (i.e. is the
     output of kextutil) and we have a dSYM file (which has ld -r'ed addresses
     but not kextutil'ed addresses so they are not final/correct), copy the
     psymtabs from OBJFILE which are based on the debug map entries into
     the OBJFILE->separate_debug_objfile and unlink the dSYM's original psymtab
     entries.  */
  if (objfile->separate_debug_objfile && objfile->not_loaded_kext_filename)
    replace_psymbols_with_correct_psymbols (objfile);

  if (deprecated_target_new_objfile_hook)
    deprecated_target_new_objfile_hook (objfile);

  bfd_cache_close_all ();
  return (objfile);
}

/* Process a symbol file, as either the main file or as a dynamically
   loaded file.

   ABFD is a BFD already open on the file, as from symfile_bfd_open.
   This BFD will be closed on error, and is always consumed by this function.

   FROM_TTY says how verbose to be.

   MAINLINE specifies whether this is the main symbol file, or whether
   it's an extra symbol file such as dynamically loaded code.

   ADDRS, OFFSETS, and NUM_OFFSETS are as described for
   syms_from_objfile, above.  ADDRS is ignored when MAINLINE is
   non-zero.

   Upon success, returns a pointer to the objfile that was added.
   Upon failure, jumps back to command level (never returns). */

static struct objfile *
symbol_file_add_with_addrs_or_offsets (bfd *abfd, int from_tty,
				       struct section_addr_info *addrs,
				       struct section_offsets *offsets,
				       int num_offsets,
				       /* APPLE LOCAL symflags */
				       int mainline, int flags, int symflags,
				       /* APPLE LOCAL symbol prefixes */
				       CORE_ADDR mapaddr, const char *prefix,
                                       char *kext_bundle)
{
  return symbol_file_add_with_addrs_or_offsets_using_objfile (NULL, abfd, from_tty, addrs,
						  offsets, num_offsets,
						  mainline, flags, symflags,
						  mapaddr, prefix, kext_bundle);
}

/* APPLE LOCAL begin symfile */
static struct objfile *
symbol_file_add_name_with_addrs_or_offsets (const char *name, int from_tty,
					    struct section_addr_info *addrs,
					    struct section_offsets *offsets,
					    int num_offsets,
					    int mainline, int flags, 
                                            int symflags, CORE_ADDR mapaddr, 
                                            const char *prefix,
                                            char *kext_bundle)
{
  bfd *abfd;
  
  abfd = symfile_bfd_open (name, mainline);
  return symbol_file_add_with_addrs_or_offsets
    (abfd, from_tty, addrs, offsets, num_offsets, mainline, flags, symflags, 
     mapaddr, prefix, kext_bundle);
}

static struct objfile *
symbol_file_add_name_with_addrs_or_offsets_using_objfile (struct objfile *in_objfile,
							  const char *name, 
							  int from_tty,
							  struct section_addr_info *addrs,
							  struct section_offsets *offsets,
							  int num_offsets,
							  int mainline, int flags, int symflags,
							  CORE_ADDR mapaddr, const char *prefix, char *kext_bundle)
{
  bfd *abfd;
  
  abfd = symfile_bfd_open (name, mainline);
  return symbol_file_add_with_addrs_or_offsets_using_objfile
    (in_objfile, abfd, from_tty, addrs, offsets, num_offsets, mainline, flags, symflags, mapaddr, prefix, kext_bundle);
}
/* APPLE LOCAL end symfile */

/* Process the symbol file ABFD, as either the main file or as a
   dynamically loaded file.

   See symbol_file_add_with_addrs_or_offsets's comments for
   details.  */
struct objfile *
symbol_file_add_from_bfd (bfd *abfd, int from_tty,
                          struct section_addr_info *addrs,
                          int mainline, int flags)
{
  return symbol_file_add_with_addrs_or_offsets (abfd,
						from_tty, addrs, 0, 0,
						/* APPLE LOCAL symfile */
                                                mainline, flags,
						0, 0, NULL, 0);
}


/* Process a symbol file, as either the main file or as a dynamically
   loaded file.  See symbol_file_add_with_addrs_or_offsets's comments
   for details.  */
struct objfile *
/* APPLE LOCAL const */
symbol_file_add (const char *name, int from_tty, struct section_addr_info *addrs,
		 int mainline, int flags)
{
  /* APPLE LOCAL symfile */
  return symbol_file_add_name_with_addrs_or_offsets
    (name, from_tty, addrs, 0, 0, mainline, flags, OBJF_SYM_ALL, 0, NULL, 
     NULL);
}

struct objfile *
/* APPLE LOCAL const */
symbol_file_add_using_objfile (struct objfile *in_objfile,
			       const char *name, int from_tty, struct section_addr_info *addrs,
			       int mainline, int flags)
{
  /* APPLE LOCAL symfile */
  return symbol_file_add_name_with_addrs_or_offsets_using_objfile
    (in_objfile, name, from_tty, addrs, 0, 0, mainline, flags, OBJF_SYM_ALL, 0, NULL, NULL);
}

/* Call symbol_file_add() with default values and update whatever is
   affected by the loading of a new main().
   Used when the file is supplied in the gdb command line
   and by some targets with special loading requirements.
   The auxiliary function, symbol_file_add_main_1(), has the flags
   argument for the switches that can only be specified in the symbol_file
   command itself.  */

void
symbol_file_add_main (char *args, int from_tty)
{
  symbol_file_add_main_1 (args, from_tty, 0);
}

static void
symbol_file_add_main_1 (char *args, int from_tty, int flags)
{
  /* APPLE LOCAL begin load levels */
  int symflags = OBJF_SYM_ALL;

#ifdef MACOSX_DYLD
  /* We need to check the desired load rules for type 'exec' to
     determine the correct load level for symfile_objfile.  The dyld
     code will eventually be notified of the change since 'mainline'
     is set to 1 in symbol_file_add_with_addrs_or_offsets.

     Ideally, we would better unify the FSF and Apple shared-library
     layers, so this call-down wouldn't be necessary. */

  struct dyld_objfile_entry e;

  dyld_objfile_entry_clear (&e);
  e.allocated = 1;

  e.load_flag = -1;

  e.reason = dyld_reason_executable;

  e.text_name = args;
  e.text_name_valid = 1;

  e.loaded_name = e.text_name;
  e.loaded_from_memory = 0;
  e.loaded_addr = 0;
  e.loaded_addrisoffset = 1;

  symflags = dyld_default_load_flag (NULL, &e) | dyld_minimal_load_flag (NULL, &e);
#endif

  symbol_file_add_name_with_addrs_or_offsets
    (args, from_tty, NULL, 0, 0, 1, flags, symflags, 0, NULL, NULL);
  /* APPLE LOCAL end load levels */

  /* Getting new symbols may change our opinion about
     what is frameless.  */
  reinit_frame_cache ();

  set_initial_language ();
}

void
symbol_file_clear (int from_tty)
{
  /* APPLE LOCAL huh? */
  if ((symfile_objfile != NULL)
      && from_tty
      && !query ("Discard symbol table from `%s'? ",
		 symfile_objfile->name))
    error (_("Not confirmed."));

  /* APPLE LOCAL begin Darwin */
#ifdef MACOSX_DYLD
  if (symfile_objfile != NULL)
    {
      free_objfile (symfile_objfile);
      symfile_objfile = NULL;
      macosx_init_dyld_symfile (symfile_objfile, exec_bfd);
    }
#else
  free_all_objfiles ();
#endif
  /* APPLE LOCAL end Darwin */

  /* APPLE LOCAL cache lookup values for improved performance  */
  symtab_clear_cached_lookup_values ();

    /* solib descriptors may have handles to objfiles.  Since their
       storage has just been released, we'd better wipe the solib
       descriptors as well.
     */
#if defined(SOLIB_RESTART)
    SOLIB_RESTART ();
#endif

    symfile_objfile = NULL;
    if (from_tty)
      printf_unfiltered (_("No symbol file now.\n"));
    /* APPLE LOCAL */
    if (state_change_hook)
      state_change_hook (STATE_ACTIVE);
}

/* APPLE LOCAL: I #ifdef'ed the next two functions out because they
   are only used in the FSF version of find_separate_debug_file, which
   we also #ifdef out.  */
#if 0
static char *
get_debug_link_info (struct objfile *objfile, unsigned long *crc32_out)
{
  asection *sect;
  bfd_size_type debuglink_size;
  unsigned long crc32;
  char *contents;
  int crc_offset;

  sect = bfd_get_section_by_name (objfile->obfd, ".gnu_debuglink");

  if (sect == NULL)
    return NULL;

  debuglink_size = bfd_section_size (objfile->obfd, sect);

  contents = xmalloc (debuglink_size);
  bfd_get_section_contents (objfile->obfd, sect, contents,
			    (file_ptr)0, (bfd_size_type)debuglink_size);

  /* Crc value is stored after the filename, aligned up to 4 bytes. */
  crc_offset = strlen (contents) + 1;
  crc_offset = (crc_offset + 3) & ~3;

  crc32 = bfd_get_32 (objfile->obfd, (bfd_byte *) (contents + crc_offset));

  *crc32_out = crc32;
  return contents;
}

static int
separate_debug_file_exists (const char *name, unsigned long crc)
{
  unsigned long file_crc = 0;
  int fd;
  char buffer[8*1024];
  int count;

  fd = open (name, O_RDONLY | O_BINARY);
  if (fd < 0)
    return 0;

  while ((count = read (fd, buffer, sizeof (buffer))) > 0)
    file_crc = gnu_debuglink_crc32 (file_crc, buffer, count);

  close (fd);

  return crc == file_crc;
}
#endif /* APPLE LOCAL - unused.  */

static char *debug_file_directory = NULL;
static void
show_debug_file_directory (struct ui_file *file, int from_tty,
			   struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
The directory where separate debug symbols are searched for is \"%s\".\n"),
		    value);
}

#if ! defined (DEBUG_SUBDIRECTORY)
#define DEBUG_SUBDIRECTORY ".debug"
#endif


/* APPLE LOCAL: Replace the entire find_separate_debug_file function
   by #if'ing it out.  */

#ifdef TM_NEXTSTEP

/* APPLE LOCAL - Search of an existing dSYM file as a possible separate debug 
   file.  */
static char *
find_separate_debug_file (struct objfile *objfile)
{
  if (already_found_debug_file != NULL)
    return xstrdup (already_found_debug_file);

  return macosx_locate_dsym (objfile);
}

#else /* TM_NEXTSTEP */

/* APPLE LOCAL: This function is #if'ed out entirely so we can replace
   it with our own version above.  */

static char *
find_separate_debug_file (struct objfile *objfile)
{
  char *basename;
  char *dir;
  char *debugfile;
  unsigned long crc32;
  int i;

  basename = get_debug_link_info (objfile, &crc32);

  if (basename == NULL)
    return NULL;

  dir = xstrdup (objfile->name);

  /* Strip off the final filename part, leaving the directory name,
     followed by a slash.  Objfile names should always be absolute and
     tilde-expanded, so there should always be a slash in there
     somewhere.  */
  for (i = strlen(dir) - 1; i >= 0; i--)
    {
      if (IS_DIR_SEPARATOR (dir[i]))
	break;
    }
  gdb_assert (i >= 0 && IS_DIR_SEPARATOR (dir[i]));
  dir[i+1] = '\0';

  debugfile = alloca (strlen (debug_file_directory) + 1
                      + strlen (dir)
                      + strlen (DEBUG_SUBDIRECTORY)
                      + strlen ("/")
                      + strlen (basename)
                      + 1);

  /* First try in the same directory as the original file.  */
  strcpy (debugfile, dir);
  strcat (debugfile, basename);

  if (separate_debug_file_exists (debugfile, crc32))
    {
      xfree (basename);
      xfree (dir);
      return xstrdup (debugfile);
    }

  /* Then try in the subdirectory named DEBUG_SUBDIRECTORY.  */
  strcpy (debugfile, dir);
  strcat (debugfile, DEBUG_SUBDIRECTORY);
  strcat (debugfile, "/");
  strcat (debugfile, basename);

  if (separate_debug_file_exists (debugfile, crc32))
    {
      xfree (basename);
      xfree (dir);
      return xstrdup (debugfile);
    }

  /* Then try in the global debugfile directory.  */
  strcpy (debugfile, debug_file_directory);
  strcat (debugfile, "/");
  strcat (debugfile, dir);
  strcat (debugfile, basename);

  if (separate_debug_file_exists (debugfile, crc32))
    {
      xfree (basename);
      xfree (dir);
      return xstrdup (debugfile);
    }

  xfree (basename);
  xfree (dir);
  return NULL;
}

#endif /* TM_NEXTSTEP */

/* This is the symbol-file command.  Read the file, analyze its
   symbols, and add a struct symtab to a symtab list.  The syntax of
   the command is rather bizarre--(1) buildargv implements various
   quoting conventions which are undocumented and have little or
   nothing in common with the way things are quoted (or not quoted)
   elsewhere in GDB, (2) options are used, which are not generally
   used in GDB (perhaps "set mapped on", "set readnow on" would be
   better), (3) the order of options matters, which is contrary to GNU
   conventions (because it is confusing and inconvenient).  */

void
symbol_file_command (char *args, int from_tty)
{
  char **argv;
  char *name = NULL;
  struct cleanup *cleanups;
  int flags = OBJF_USERLOADED;

  dont_repeat ();

  if (args == NULL)
    {
      symbol_file_clear (from_tty);
    }
  else
    {
      if ((argv = buildargv (args)) == NULL)
	{
	  nomem (0);
	}
      cleanups = make_cleanup_freeargv (argv);
      while (*argv != NULL)
	{
	  if (strcmp (*argv, "-readnow") == 0)
	    flags |= OBJF_READNOW;
	  else if (**argv == '-')
	    error (_("unknown option `%s'"), *argv);
	  else
	    {
	      name = *argv;

	      symbol_file_add_main_1 (name, from_tty, flags);
	    }
	  argv++;
	}

      if (name == NULL)
	{
	  error (_("no symbol file name was specified"));
	}
      do_cleanups (cleanups);
    }

  /* APPLE LOCAL */
  if (state_change_hook)
    {
      state_change_hook (STATE_INFERIOR_LOADED);
    }
}

/* Set the initial language.

   A better solution would be to record the language in the psymtab when reading
   partial symbols, and then use it (if known) to set the language.  This would
   be a win for formats that encode the language in an easily discoverable place,
   such as DWARF.  For stabs, we can jump through hoops looking for specially
   named symbols or try to intuit the language from the specific type of stabs
   we find, but we can't do that until later when we read in full symbols.
   FIXME.  */

static void
set_initial_language (void)
{
  struct partial_symtab *pst;
  enum language lang = language_unknown;

  pst = find_main_psymtab ();
  if (pst != NULL)
    {
      if (pst->filename != NULL)
	{
	  lang = deduce_language_from_filename (pst->filename);
	}
      if (lang == language_unknown)
	{
	  /* Make C the default language */
	  lang = language_c;
	}
      set_language (lang);
      expected_language = current_language;	/* Don't warn the user */
    }
}

/* Open file specified by NAME and hand it off to BFD for preliminary
   analysis.  Result is a newly initialized bfd *, which includes a newly
   malloc'd` copy of NAME (tilde-expanded and made absolute).
   APPLE LOCAL symfile bfd open
   If MAINLINE is 1 this the main exec file, and some platforms (DOS &
   MacOS X) do a little more magic to find the file.
   In case of trouble, error() is called.  */

bfd *
/* APPLE LOCAL symfile bfd open */
symfile_bfd_open (const char *name, int mainline)
{
  bfd *sym_bfd;
  int desc;
  char *absolute_name;



  name = tilde_expand (name);	/* Returns 1st new malloc'd copy */

  /* Look down path for it, allocate 2nd new malloc'd copy.  */
  desc = openp (getenv ("PATH"), OPF_TRY_CWD_FIRST, name, O_RDONLY | O_BINARY,
		0, &absolute_name);
#if defined(__GO32__) || defined(_WIN32) || defined (__CYGWIN__)
  /* APPLE LOCAL symfile bfd open */
  if (mainline && desc < 0)
    {
      char *exename = alloca (strlen (name) + 5);
      strcat (strcpy (exename, name), ".exe");
      desc = openp (getenv ("PATH"), OPF_TRY_CWD_FIRST, exename,
		    O_RDONLY | O_BINARY, 0, &absolute_name);
    }
#endif

  /* APPLE LOCAL begin symfile bfd open */
#ifdef TM_NEXTSTEP
  if (desc < 0)
    {
      /* APPLE LOCAL: Look for a wrapped executable of the form
	 Foo.app/Contents/MacOS/Foo, where the user gave us up to
	 Foo.app.  The ".app" is optional. */

      char *wrapped_filename = macosx_filename_in_bundle (name, mainline);
      
      if (wrapped_filename != NULL)
	{
	  desc = openp (getenv ("PATH"), 1, wrapped_filename, O_RDONLY | O_BINARY,
			0, &absolute_name);
	  xfree (wrapped_filename);
	}
    }
#endif
  /* APPLE LOCAL end symfile bfd open */

  if (desc < 0)
    {
      make_cleanup (xfree, name);
      perror_with_name (name);
    }
  xfree (name);			/* Free 1st new malloc'd copy */
  name = absolute_name;		/* Keep 2nd malloc'd copy in bfd */
  /* It'll be freed in free_objfile(). */

  if (desc > 0)
    sym_bfd = bfd_fopen (name, gnutarget, FOPEN_RB, desc);

  if (!sym_bfd)
    {
      close (desc);
      make_cleanup (xfree, name);
      error (_("\"%s\": can't open to read symbols: %s."), name,
	     bfd_errmsg (bfd_get_error ()));
    }
  sym_bfd->cacheable = 1;

  /* APPLE LOCAL begin symfile bfd open */
#if HAVE_MMAP
  if (mmap_symbol_files_flag)
    {
      if (!bfd_mmap_file (sym_bfd, (void *) -1))
	error ("\"%s\": could not mmap file for read: %s",
	       name, bfd_errmsg (bfd_get_error ()));
    }
#endif /* HAVE_MMAP */

  /* APPLE LOCAL: If the file is an archive file (i.e. fat
     binary), look for sub-files that match the current osabi. */

  if (bfd_check_format (sym_bfd, bfd_archive))
    {
      bfd *tmp_bfd;
      tmp_bfd = open_bfd_matching_arch (sym_bfd, bfd_object);
      if (tmp_bfd != NULL)
	sym_bfd = tmp_bfd;
    }
  /* APPLE LOCAL end symfile bfd open */

  bfd_set_cacheable (sym_bfd, 1);

  if (!bfd_check_format (sym_bfd, bfd_object))
    {
      /* FIXME: should be checking for errors from bfd_close (for one thing,
         on error it does not free all the storage associated with the
         bfd).  */
      bfd_close (sym_bfd);	/* This also closes desc */
      make_cleanup (xfree, name);
      error (_("\"%s\": can't read symbols: %s."), name,
	     bfd_errmsg (bfd_get_error ()));
    }
  return (sym_bfd);
}

/* Return the section index for the given section name. Return -1 if
   the section was not found. */
int
get_section_index (struct objfile *objfile, char *section_name)
{
  asection *sect = bfd_get_section_by_name (objfile->obfd, section_name);
  if (sect)
    return sect->index;
  else
    return -1;
}

/* Link a new symtab_fns into the global symtab_fns list.  Called on gdb
   startup by the _initialize routine in each object file format reader,
   to register information about each format the the reader is prepared
   to handle. */

void
add_symtab_fns (struct sym_fns *sf)
{
  sf->next = symtab_fns;
  symtab_fns = sf;
}


/* Initialize to read symbols from the symbol file sym_bfd.  It either
   returns or calls error().  The result is an initialized struct sym_fns
   in the objfile structure, that contains cached information about the
   symbol file.  */

static void
find_sym_fns (struct objfile *objfile)
{
  struct sym_fns *sf;
  enum bfd_flavour our_flavour = bfd_get_flavour (objfile->obfd);

  if (our_flavour == bfd_target_srec_flavour
      || our_flavour == bfd_target_ihex_flavour
      || our_flavour == bfd_target_tekhex_flavour)
    return;	/* No symbols. */

  for (sf = symtab_fns; sf != NULL; sf = sf->next)
    {
      if (our_flavour == sf->sym_flavour)
	{
	  objfile->sf = sf;
	  return;
	}
    }
  error (_("I'm sorry, Dave, I can't do that.  Symbol format `%s' unknown."),
	 bfd_get_target (objfile->obfd));
}

/* This function runs the load command of our current target.  */

static void
load_command (char *arg, int from_tty)
{
  if (arg == NULL)
    arg = get_exec_file (1);
  target_load (arg, from_tty);

  /* After re-loading the executable, we don't really know which
     overlays are mapped any more.  */
  overlay_cache_invalid = 1;
}

/* This version of "load" should be usable for any target.  Currently
   it is just used for remote targets, not inftarg.c or core files,
   on the theory that only in that case is it useful.

   Avoiding xmodem and the like seems like a win (a) because we don't have
   to worry about finding it, and (b) On VMS, fork() is very slow and so
   we don't want to run a subprocess.  On the other hand, I'm not sure how
   performance compares.  */

static int download_write_size = 512;
static void
show_download_write_size (struct ui_file *file, int from_tty,
			  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
The write size used when downloading a program is %s.\n"),
		    value);
}
static int validate_download = 0;

/* Callback service function for generic_load (bfd_map_over_sections).  */

static void
add_section_size_callback (bfd *abfd, asection *asec, void *data)
{
  bfd_size_type *sum = data;

  *sum += bfd_get_section_size (asec);
}

/* Opaque data for load_section_callback.  */
struct load_section_data {
  unsigned long load_offset;
  unsigned long write_count;
  unsigned long data_count;
  bfd_size_type total_size;

  /* Per-section data for load_progress.  */
  const char *section_name;
  ULONGEST section_sent;
  ULONGEST section_size;
  CORE_ADDR lma;
  gdb_byte *buffer;
};

/* Target write callback routine for load_section_callback.  */

static void
load_progress (ULONGEST bytes, void *untyped_arg)
{
  struct load_section_data *args = untyped_arg;

  if (validate_download)
    {
      /* Broken memories and broken monitors manifest themselves here
	 when bring new computers to life.  This doubles already slow
	 downloads.  */
      /* NOTE: cagney/1999-10-18: A more efficient implementation
	 might add a verify_memory() method to the target vector and
	 then use that.  remote.c could implement that method using
	 the ``qCRC'' packet.  */
      gdb_byte *check = xmalloc (bytes);
      struct cleanup *verify_cleanups = make_cleanup (xfree, check);

      if (target_read_memory (args->lma, check, bytes) != 0)
	error (_("Download verify read failed at 0x%s"),
	       paddr (args->lma));
      if (memcmp (args->buffer, check, bytes) != 0)
	error (_("Download verify compare failed at 0x%s"),
	       paddr (args->lma));
      do_cleanups (verify_cleanups);
    }
  args->data_count += bytes;
  args->lma += bytes;
  args->buffer += bytes;
  args->write_count += 1;
  args->section_sent += bytes;
  if (quit_flag
      || (deprecated_ui_load_progress_hook != NULL
	  && deprecated_ui_load_progress_hook (args->section_name,
					       args->section_sent)))
    error (_("Canceled the download"));

  if (deprecated_show_load_progress != NULL)
    deprecated_show_load_progress (args->section_name,
				   args->section_sent,
				   args->section_size,
				   args->data_count,
				   args->total_size);
}

/* Callback service function for generic_load (bfd_map_over_sections).  */

static void
load_section_callback (bfd *abfd, asection *asec, void *data)
{
  struct load_section_data *args = data;
  bfd_size_type size = bfd_get_section_size (asec);
  gdb_byte *buffer;
  struct cleanup *old_chain;
  const char *sect_name = bfd_get_section_name (abfd, asec);
  LONGEST transferred;

  if ((bfd_get_section_flags (abfd, asec) & SEC_LOAD) == 0)
    return;

  if (size == 0)
    return;

  buffer = xmalloc (size);
  old_chain = make_cleanup (xfree, buffer);

  args->section_name = sect_name;
  args->section_sent = 0;
  args->section_size = size;
  args->lma = bfd_section_lma (abfd, asec) + args->load_offset;
  args->buffer = buffer;

  /* Is this really necessary?  I guess it gives the user something
     to look at during a long download.  */
  ui_out_message (uiout, 0, "Loading section %s, size 0x%s lma 0x%s\n",
		  sect_name, paddr_nz (size), paddr_nz (args->lma));

  bfd_get_section_contents (abfd, asec, buffer, 0, size);

  transferred = target_write_with_progress (&current_target,
					    TARGET_OBJECT_MEMORY,
					    NULL, buffer, args->lma,
					    size, load_progress, args);
  if (transferred < size)
    error (_("Memory access error while loading section %s."),
	   sect_name);

  do_cleanups (old_chain);
}

void
generic_load (char *args, int from_tty)
{
  bfd *loadfile_bfd;
  struct timeval start_time, end_time;
  char *filename;
  struct cleanup *old_cleanups;
  char *offptr;
  struct load_section_data cbdata;
  CORE_ADDR entry;

  cbdata.load_offset = 0;	/* Offset to add to vma for each section. */
  cbdata.write_count = 0;	/* Number of writes needed. */
  cbdata.data_count = 0;	/* Number of bytes written to target memory. */
  cbdata.total_size = 0;	/* Total size of all bfd sectors. */

  /* Parse the input argument - the user can specify a load offset as
     a second argument. */
  filename = xmalloc (strlen (args) + 1);
  old_cleanups = make_cleanup (xfree, filename);
  strcpy (filename, args);
  offptr = strchr (filename, ' ');
  if (offptr != NULL)
    {
      char *endptr;

      cbdata.load_offset = strtoul (offptr, &endptr, 0);
      if (offptr == endptr)
	error (_("Invalid download offset:%s."), offptr);
      *offptr = '\0';
    }
  else
    cbdata.load_offset = 0;

  /* Open the file for loading. */
  loadfile_bfd = bfd_openr (filename, gnutarget);
  if (loadfile_bfd == NULL)
    {
      perror_with_name (filename);
      return;
    }

  /* FIXME: should be checking for errors from bfd_close (for one thing,
     on error it does not free all the storage associated with the
     bfd).  */
  make_cleanup_bfd_close (loadfile_bfd);

  if (!bfd_check_format (loadfile_bfd, bfd_object))
    {
      error (_("\"%s\" is not an object file: %s"), filename,
	     bfd_errmsg (bfd_get_error ()));
    }

  bfd_map_over_sections (loadfile_bfd, add_section_size_callback,
			 (void *) &cbdata.total_size);

  gettimeofday (&start_time, NULL);

  bfd_map_over_sections (loadfile_bfd, load_section_callback, &cbdata);

  gettimeofday (&end_time, NULL);

  entry = bfd_get_start_address (loadfile_bfd);
  ui_out_text (uiout, "Start address ");
  ui_out_field_fmt (uiout, "address", "0x%s", paddr_nz (entry));
  ui_out_text (uiout, ", load size ");
  ui_out_field_fmt (uiout, "load-size", "%lu", cbdata.data_count);
  ui_out_text (uiout, "\n");
  /* We were doing this in remote-mips.c, I suspect it is right
     for other targets too.  */
  write_pc (entry);

  /* FIXME: are we supposed to call symbol_file_add or not?  According
     to a comment from remote-mips.c (where a call to symbol_file_add
     was commented out), making the call confuses GDB if more than one
     file is loaded in.  Some targets do (e.g., remote-vx.c) but
     others don't (or didn't - perhaps they have all been deleted).  */

  print_transfer_performance (gdb_stdout, cbdata.data_count,
			      cbdata.write_count, &start_time, &end_time);

  do_cleanups (old_cleanups);
}

/* Report how fast the transfer went. */

/* DEPRECATED: cagney/1999-10-18: report_transfer_performance is being
   replaced by print_transfer_performance (with a very different
   function signature). */

void
report_transfer_performance (unsigned long data_count, time_t start_time,
			     time_t end_time)
{
  struct timeval start, end;

  start.tv_sec = start_time;
  start.tv_usec = 0;
  end.tv_sec = end_time;
  end.tv_usec = 0;

  print_transfer_performance (gdb_stdout, data_count, 0, &start, &end);
}

void
print_transfer_performance (struct ui_file *stream,
			    unsigned long data_count,
			    unsigned long write_count,
			    const struct timeval *start_time,
			    const struct timeval *end_time)
{
  unsigned long time_count;

  /* Compute the elapsed time in milliseconds, as a tradeoff between
     accuracy and overflow.  */
  time_count = (end_time->tv_sec - start_time->tv_sec) * 1000;
  time_count += (end_time->tv_usec - start_time->tv_usec) / 1000;

  ui_out_text (uiout, "Transfer rate: ");
  if (time_count > 0)
    {
      ui_out_field_fmt (uiout, "transfer-rate", "%lu",
			1000 * (data_count * 8) / time_count);
      ui_out_text (uiout, " bits/sec");
    }
  else
    {
      ui_out_field_fmt (uiout, "transferred-bits", "%lu", (data_count * 8));
      ui_out_text (uiout, " bits in <1 sec");
    }
  if (write_count > 0)
    {
      ui_out_text (uiout, ", ");
      ui_out_field_fmt (uiout, "write-rate", "%lu", data_count / write_count);
      ui_out_text (uiout, " bytes/write");
    }
  ui_out_text (uiout, ".\n");
}

/* This function allows the addition of incrementally linked object files.
   It does not modify any state in the target, only in the debugger.  */
/* Note: ezannoni 2000-04-13 This function/command used to have a
   special case syntax for the rombug target (Rombug is the boot
   monitor for Microware's OS-9 / OS-9000, see remote-os9k.c). In the
   rombug case, the user doesn't need to supply a text address,
   instead a call to target_link() (in target.c) would supply the
   value to use. We are now discontinuing this type of ad hoc syntax. */

static void
add_symbol_file_command (char *args, int from_tty)
{
  char *filename = NULL;
  char *address = NULL;
  char *prefix = NULL;
  char **argv = NULL;
  CORE_ADDR mapaddr = 0;
  int flags = OBJF_USERLOADED;
  int symflags = OBJF_SYM_ALL;
  char *arg;
  int section_index = 0;
  int argcnt = 0;
  int sec_num = 0;
  int i;
  struct objfile *o;   /* APPLE LOCAL */
  const char *const usage_string =
    "usage (%s): add-symbol-file <filename> <text address> [-mapped] [-readnow] [-s <secname> <addr>]*";

  struct sect_opt
  {
    char *name;
    char *value;
  };

  struct section_addr_info *section_addrs;
  struct sect_opt *sect_opts = NULL;
  size_t num_sect_opts = 0;
  struct cleanup *my_cleanups = make_cleanup (null_cleanup, NULL);

  num_sect_opts = 16;
  sect_opts = (struct sect_opt *) xmalloc (num_sect_opts
					   * sizeof (struct sect_opt));

  dont_repeat ();

  if (args == NULL)
    error (usage_string, "argument required");

  argv = buildargv (args);
  if (argv == NULL)
    nomem (0);
  make_cleanup_freeargv (argv);
 	  
  argcnt = 0;
  for (;;)
    {
      arg = argv[argcnt++];
      if (arg == NULL)
 	break;
 
      /* It's an option (starting with '-'), an argument
 	 to an option, or the filename. */
 
      if (*arg == '-')
 	{
 	  if (strcmp (arg, "-mapaddr") == 0)
 	    {
 	      char *atmp = argv[argcnt++];
 	      if (atmp == NULL)
 		error (usage_string, "must specify address to -mapaddr");
 	      mapaddr = parse_and_eval_address (atmp);
 	    }
 	  if (strcmp (arg, "-prefix") == 0)
 	    {
 	      char *atmp = argv[argcnt++];
 	      if (atmp == NULL)
 		error (usage_string, "must specify address to -prefix");
 	      prefix = xstrdup (atmp);
 	    }
 	  else if (strcmp (arg, "-readnow") == 0)
 	    flags |= OBJF_READNOW;
 	  else if (strcmp (arg, "-s") == 0)
 	    {
 	      char *atmp = argv[argcnt++];
 	      if (atmp == NULL)
 		error (usage_string, "must specify section name to -s");
 
 	      char *atmp2 = argv[argcnt++];
 	      if (atmp2 == NULL)
 		error (usage_string, "must specify section address to -s");
 
 	      if (section_index >= 16)
 		error (usage_string, "too many sections specified.");
 
 	      sect_opts[section_index].name = atmp;
 	      sect_opts[section_index].value = atmp2;
 	      section_index++;		  
 	    }
 	}
      else
 	{
 	  if (filename == NULL)
 	    {
 	      filename = tilde_expand (arg);
 	      make_cleanup (xfree, filename);
 	    }
 	  else if (address == NULL)
 	    {
 	      address = arg;
 	    }
 	  else
 	    {
 	      error (usage_string, "too many arguments");
 	    }
 	}
    }

  if (address != NULL)
    {
      if (section_index >= 16)
	error (usage_string, "too many sections specified.");

      /* APPLE LOCAL: Look for the text segment ("__TEXT"), not the section
         ("__TEXT.__text") because what we're really looking for is the load
         address of the image, and the section address is offset a bit. */
      sect_opts[section_index].name = TEXT_SEGMENT_NAME;
      sect_opts[section_index].value = address;
      section_index++;
    }
 
  if (filename == NULL)
    error ("usage: must specify exactly one filename");

  /* APPLE LOCAL: Did the user do "add-symbol-file whatever.dSYM" when 
     they really intended to do an add-dsym?
     If there were any arguments to add-symbol-file in addition to the filename,
     we'll assume they know what they're doing and give them a warning.
     If it's just the dSYM bundle name, redirect to add-dsym.  */

  if (strstr (filename, ".dSYM"))
    {
      /* If there's one argument (e.g. a filename), argcnt == 2 */
      if (argcnt == 2)
        {
          add_dsym_command (args, from_tty);
          return;
        }
      else
        {
          warning ("add-symbol-file doesn't work on dSYM files, use "
                   "\"add-dsym\" instead.");
        }
    }

  /* Print the prompt for the query below. And save the arguments into
     a sect_addr_info structure to be passed around to other
     functions.  We have to split this up into separate print
     statements because hex_string returns a local static
     string. */
 
  /* APPLE LOCAL:  Don't print ``at\n'' (which should be followed by
     a list of sections and addresses) if there are no sections and
     addresses supplied (i.e. "add-symbol-file a.out")  */
  printf_filtered ("add symbol table from file \"%s\"", filename);
  if (section_index > 0)
    printf_filtered (" at\n");
  else 
    printf_filtered ("? ");
  if (section_index == 0)
    {
      section_addrs = NULL;
    }
  else
    {
      section_addrs = alloc_section_addr_info (section_index);
      make_cleanup (xfree, section_addrs);
      for (i = 0; i < section_index; i++)
	{
	  CORE_ADDR addr;
	  char *val = sect_opts[i].value;
	  char *sec = sect_opts[i].name;
	  
	  addr = parse_and_eval_address (val);
	  
	  /* Here we store the section offsets in the order they were
	     entered on the command line. */
	  section_addrs->other[sec_num].name = sec;
	  section_addrs->other[sec_num].addr = addr;
	  printf_unfiltered ("\t%s = 0x%s\n",
			     sec, paddr_nz (addr));
	  sec_num++;
	  
	  /* The object's sections are initialized when a 
	     call is made to build_objfile_section_table (objfile).
	     This happens in reread_symbols. 
	     At this point, we don't know what file type this is,
	     so we can't determine what section names are valid.  */
	}
    }

  if (from_tty && (!query ("%s", "")))
    error (_("Not confirmed."));
  
  /* APPLE LOCAL: Save return'ed objfile, set the syms_only_objfile flag */
  o = symbol_file_add_name_with_addrs_or_offsets
    (filename, from_tty, section_addrs, NULL, 0, 0, flags, symflags, 
     mapaddr, prefix, NULL);
  o->syms_only_objfile = 1; 

#ifdef MACOSX_DYLD
  update_section_tables ();
#endif
  update_current_target ();
  re_enable_breakpoints_in_shlibs (0);

  /* Getting new symbols may change our opinion about what is
     frameless.  */
  reinit_frame_cache ();
  do_cleanups (my_cleanups);
}

/* APPLE LOCAL: A stripped down copy of add_symbol_file_command designed
   specifically for the oddness that is kexts.  In the kext case we have
   three components that we need to juggle:

     1. The output symbol file from kextutil -a/-s which has the final
        addresses that the kext loaded at 
        (e.g. "com.osxbook.kext.DummySysctl.sym"), 
     2. The kext bundle with an Info.plist and executable inside 
        (e.g. "DummySysctl.kext"), and finally 

     3. The companion dSYM to the kext bundle (e.g. "DummySysctl.kext.dSYM")

   kexts are unusual in that the kext bundle and dSYM do not have the actual
   final addresses -- kextutil has a linker ("libkld") inside which may do
   more linkery things than simply applying a static offset to all the 
   symbols.  We need file #1 because it has the final addresses of all the
   symbols; we need file #3 because it has the DWARF debug information.  Less
   obvious is that we need file #2 to create the address translation map
   that will be used when reading #3.  
 */

static void
add_kext_command (char *args, int from_tty)
{
  char **argv = NULL;
  char *filename;
  struct objfile *o;
  int flags = OBJF_USERLOADED;
  int symflags = OBJF_SYM_ALL;

  char *kext_bundle_filename;
  const char *bundle_executable_name_from_plist;
  const char *bundle_identifier_name_from_plist;
  char *kextload_symbol_filename, *kextload_symbol_basename;
  char *kext_bundle_executable_filename;
  char *t;

  const char *const usage_string =
    "Error: %s\n"
    "Usage: add-kext <PATHNAME-OF-KEXT>\n"
    "PATHNAME-OF-KEXT is the path to the .kext bundle directory which has an\n"
    "Info.plist file in its Contents subdirectory.  The .sym file (output from\n"
    "kextutil) should be a sibling of the .kext bundle and the kext bundle's\n"
    "dSYM bundle should also be in this drectory.";

  struct cleanup *my_cleanups = make_cleanup (null_cleanup, NULL);

  dont_repeat ();

  if (args == NULL)
    error (usage_string, "argument required");

  argv = buildargv (args);
  if (argv == NULL)
    nomem (0);
  make_cleanup_freeargv (argv);

  filename = argv[0];  
  if (filename == NULL)
    error (usage_string, "no kext bundle name supplied");


  kext_bundle_filename = macosx_kext_info (filename, 
                                           &bundle_executable_name_from_plist,
                                           &bundle_identifier_name_from_plist);
  
  if (kext_bundle_filename == NULL)
    error (usage_string, "Unable to find kext bundle at pathname provided");
  if (bundle_executable_name_from_plist == NULL)
    error (usage_string, "Unable to find CFBundleExecutable in Info.plist");
  if (bundle_identifier_name_from_plist == NULL)
    error (usage_string, "Unable to find CFBundleIdentifier in Info.plist");
  
  t = dirname (kext_bundle_filename);
  if (t == NULL)
    error (usage_string, "dirname on the kext bundle filename failed");

  kextload_symbol_basename = xmalloc 
             (strlen (bundle_identifier_name_from_plist) + strlen (".sym") + 1);
  strcpy (kextload_symbol_basename, bundle_identifier_name_from_plist);
  strcat (kextload_symbol_basename, ".sym");

  /* A string of "." means that KEXT_BUNDLE_FILENAME has no dirname 
     component. */
  if (t[0] == '.' && t[1] == '\0')
    {
      kextload_symbol_filename = kextload_symbol_basename;
    }
  else if (file_exists_p (kextload_symbol_basename))
    {
      kextload_symbol_filename = kextload_symbol_basename;
    }
  else
    {
      kextload_symbol_filename = xmalloc (strlen (t) + 1 + 
                                strlen (kextload_symbol_basename) + 1);
      strcpy (kextload_symbol_filename, t);
      strcat (kextload_symbol_filename, "/");
      strcat (kextload_symbol_filename, kextload_symbol_basename);
      xfree (kextload_symbol_basename);
    }

  /* By default we assume the .sym file is next to the .kext bundle - but
     if the user has added a kext-symbol-file-path, look there as well.  */

  if (file_exists_p (kextload_symbol_filename) == 0 
      && kext_symbol_file_path != NULL)
    {
      char *possible_new_name;
      possible_new_name = xmalloc (strlen (kext_symbol_file_path) + 1 +
                                   strlen (bundle_identifier_name_from_plist) +
                                   strlen (".sym") + 1);
      strcpy (possible_new_name, kext_symbol_file_path);
      strcat (possible_new_name, "/");
      strcat (possible_new_name, bundle_identifier_name_from_plist);
      strcat (possible_new_name, ".sym");
      if (file_exists_p (possible_new_name))
        {
          xfree (kextload_symbol_filename);
          kextload_symbol_filename = possible_new_name;
        }
    }

  kext_bundle_executable_filename = xmalloc (strlen (kext_bundle_filename) +
                                strlen ("/Contents/MacOS/") +
                                strlen (bundle_executable_name_from_plist) + 1);
  strcpy (kext_bundle_executable_filename, kext_bundle_filename);
  strcat (kext_bundle_executable_filename, "/Contents/MacOS/");
  strcat (kext_bundle_executable_filename, bundle_executable_name_from_plist);

  /* See if we might be loading a shallow bundle here.  */
  if (!file_exists_p (kext_bundle_executable_filename))
    {
       char *shallow_bundle_name = xmalloc (strlen (kext_bundle_filename) + 1 +
                               strlen (bundle_executable_name_from_plist) + 1);
       strcpy (shallow_bundle_name, kext_bundle_filename);
       strcat (shallow_bundle_name, "/");
       strcat (shallow_bundle_name, bundle_executable_name_from_plist);
       if (file_exists_p (shallow_bundle_name))
         {
           xfree ((char *) kext_bundle_executable_filename);
           kext_bundle_executable_filename = shallow_bundle_name;
         }
       else
         {
           xfree ((char *) shallow_bundle_name);
         }
    }

  xfree ((char *) bundle_executable_name_from_plist);
  xfree ((char *) bundle_identifier_name_from_plist);

  printf_filtered ("add symbol table from file \"%s\"? ", 
                    kextload_symbol_filename);
  if (from_tty && (!query ("%s", "")))
    error (_("Not confirmed."));
  
  o = symbol_file_add_name_with_addrs_or_offsets
             (kextload_symbol_filename, from_tty, NULL, NULL, 0, 0, 
              flags, symflags, 0, NULL, kext_bundle_executable_filename);
  o->syms_only_objfile = 1; 

  if (bfd_default_compatible (bfd_get_arch_info (o->obfd), 
                              gdbarch_bfd_arch_info (current_gdbarch)) == NULL)
    {
      warning ("This gdb is expecting %s binaries but %s is %s which is not "
               "compatible.  gdb will use the .sym file but this is unlikely "
               "to be correct.", 
               gdbarch_bfd_arch_info (current_gdbarch)->printable_name,
               o->name, bfd_get_arch_info (o->obfd)->printable_name);
    }

#ifdef NM_NEXTSTEP
  update_section_tables ();
#endif
  update_current_target ();
  breakpoint_update ();

  /* Getting new symbols may change our opinion about what is
     frameless.  */
  reinit_frame_cache ();
  do_cleanups (my_cleanups);
}

/* APPLE LOCAL: This command adds the space-separated list of dSYMs
   pointed to by ARGS to the objfiles with matching UUIDs.  */

static void
add_dsym_command (char *args, int from_tty)
{
  struct objfile *objfile;
  volatile struct gdb_exception e;
  char *full_name;
  char *dsym_path;
  char *objfile_name;
  struct cleanup *full_name_cleanup;
  struct cleanup *argv_cleanup;
  struct stat stat_buf;
  char **arg_array;
  int i;

  if (args == NULL || *args == '\0')
    error ("Wrong number of args, should be \"add-dsym <DSYM PATH>\".");

  arg_array = buildargv (args);
  if (arg_array == NULL)
    error ("Unable to build argument vector for add-dsym command.");

  argv_cleanup = make_cleanup_freeargv (arg_array);

  for (i = 0; arg_array[i] != NULL; i++)
    {
      /* Check that the passed in dsym file exists */
      full_name = tilde_expand (arg_array[i]);
      full_name_cleanup = make_cleanup (xfree, full_name);
      if (stat (full_name, &stat_buf) == -1)
	{
	  error ("Error \"%s\" accessing dSYM file \"%s\".", 
		 strerror (errno), full_name);
	  
	}
      
      objfile = macosx_find_objfile_matching_dsym_in_bundle (full_name, &dsym_path);
      
      if (!objfile)
	{
	  warning ("Could not find binary to match \"%s\".", full_name);
	  continue;
	}
      
      make_cleanup (xfree, dsym_path);
      
      if (objfile->separate_debug_objfile != NULL)
	{
	  warning ("Trying to add dSYM file to \"%s\" which already has one.",
		 objfile->name);
	  goto do_cleanups;
	}
            
      /* Don't assume that the process of raising the load level
	 of OBJFILE will keep the pointer valid.  Instead, stash
	 away the name now so we can print the command result
	 when we're done.  */
      
      objfile_name = xstrdup (objfile->name);
      make_cleanup (xfree, objfile_name);

      /* The code that brings in a new objfile is too involved
	 to try to JUST add a dSYM to the extant objfile.  Instead,
	 I set the load state of the objfile to NONE, then raise
	 its load level ALL.  Meanwhile, I slip the name of the
	 dsym file to find_separate_dsym_file under the covers.
	 I'm not entirely proud of having to do it this way, but
	 it has the virtue of making the "add-dsym" work the same
	 way raising the load levels does.  */
      
      already_found_debug_file = dsym_path;
      TRY_CATCH (e, RETURN_MASK_ERROR)
	{
	  /* This is a bit of a lie, but we need to set the symflags
	     back to NONE so the set_load_state code will actually
	     change the objfile state.  */
	  objfile->symflags = OBJF_SYM_NONE;
	  dyld_objfile_set_load_state (objfile, OBJF_SYM_ALL);
	}
      already_found_debug_file = NULL;
      if (e.reason != NO_ERROR)
	{
	  warning ("Could not add dSYM \"%s\" to library \"%s\".", 
		 dsym_path, objfile->name);
	  goto do_cleanups;
	}

      /* Now report on what we've done.  */
      make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
      ui_out_text (uiout, "Added dsym \"");
      ui_out_field_string (uiout, "dsymfile", full_name);
      ui_out_text (uiout, "\" to \"");
      ui_out_field_string (uiout, "objfile", objfile_name);
      ui_out_text (uiout, "\".\n");
    do_cleanups:
      do_cleanups (full_name_cleanup);
    }
  do_cleanups (argv_cleanup);
}

static void
add_shared_symbol_files_command (char *args, int from_tty)
{
#ifdef ADD_SHARED_SYMBOL_FILES
  ADD_SHARED_SYMBOL_FILES (args, from_tty);
#else
  error (_("This command is not available in this configuration of GDB."));
#endif
}

/* Re-read symbols if a symbol-file has changed.  */
void
reread_symbols (void)
{
  struct objfile *objfile, *next;
  long new_modtime;
  int reread_one = 0;

  /* With the addition of shared libraries, this should be modified,
     the load time should be saved in the partial symbol tables, since
     different tables may come from different source files.  FIXME.
     This routine should then walk down each partial symbol table
     and see if the symbol table that it originates from has been changed */

  /* APPLE LOCAL: Separate debug objfiles are normally made after at least the
     bfd for their parent objfile is set up.  BUT we put them in front of the
     parent objfile in the objfiles list, so in this loop they would get made
     in reverse order.  To fix that, instead we delete the separate debug objfile
     when we see a parent with a separate debug objfile.  Then we let reread_separate_symbols
     recreate the separate debug objfile.  */

  ALL_OBJFILES_SAFE (objfile, next)
    {
      /* APPLE LOCAL: Let the reread_separate_symbols take care of
	 remaking the separate_debug_objfile.  */
      if (objfile->separate_debug_objfile_backlink)
	continue;
      if (objfile->obfd)
	{

	  /* APPLE LOCAL: Stat the file by path, when one is
	     available, to detect the case where the file has been
	     replaced, but BFD still has a file descriptor open to
	     the old version.  */
	  int backing_file_missing;
          {
            struct stat buf;
	    backing_file_missing = 0;
	    if (stat (objfile->obfd->filename, &buf) != 0)
	      {
		/* Check for NULL iostream.  If that's NULL, then
		   bfd_get_mtime is just going to abort, which is not
		   very friendly.  Instead, use new_modtime of -1 to
		   indicate we can't find the file right now.  */
		if (objfile->obfd->iostream != NULL)
		  new_modtime = bfd_get_mtime (objfile->obfd);
		else 
		  backing_file_missing = 1;
	      }
            else
              new_modtime = buf.st_mtime;
          }
          /* END APPLE LOCAL */

	  if (backing_file_missing)
	    {
	      /* For some reason we can no longer open the backing file.
		 We can't really clean up reasonably here, because we
		 can't tell the shared library system that this objfile is
		 gone.  So just continue on here, and hope that next time we
		 run, an extant file will show up.  */
	      warning ("Can't find backing file for \"%s\".",
		     objfile->obfd->filename);
	    }
	  else if (new_modtime != objfile->mtime)
	    {
	      struct cleanup *old_cleanups;
	      struct section_offsets *offsets;
	      int num_offsets;
	      char *obfd_filename;

	      printf_unfiltered (_("`%s' has changed; re-reading symbols.\n"),
			       objfile->name);

	      /* There are various functions like symbol_file_add,
	         symfile_bfd_open, syms_from_objfile, etc., which might
	         appear to do what we want.  But they have various other
	         effects which we *don't* want.  So we just do stuff
	         ourselves.  We don't worry about mapped files (for one thing,
	         any mapped file will be out of date).  */

	      /* If we get an error, blow away this objfile (not sure if
	         that is the correct response for things like shared
	         libraries).  */
	      old_cleanups = make_cleanup_free_objfile (objfile);
	      /* We need to do this whenever any symbols go away.  */
	      make_cleanup (clear_symtab_users_cleanup, 0 /*ignore*/);


	      /* If this objfile has a separate debug objfile, clear it
		 out here.  */
	      if (objfile->separate_debug_objfile != NULL)
		{
		  /* ALL_OBJFILES_SAFE isn't actually safe if you delete
		     the NEXT objfile...  */
		  if (objfile->separate_debug_objfile == next)
		    next = objfile_get_next (next);

		  free_objfile (objfile->separate_debug_objfile);
		  objfile->separate_debug_objfile = NULL;
		}

	      /* APPLE LOCAL: Before we clean up any state, tell the
		 breakpoint system that this objfile has changed so it
		 can clear the set state on any breakpoints in this
		 objfile. */

	      tell_breakpoints_objfile_changed (objfile);
	      tell_objc_msgsend_cacher_objfile_changed (objfile);

	      /* APPLE LOCAL cache lookup values for improved performance  */
	      symtab_clear_cached_lookup_values ();
	      /* APPLE LOCAL: Remove it's obj_sections from the 
		 ordered_section list.  */
	      objfile_delete_from_ordered_sections (objfile);

	      /* Clean up any state BFD has sitting around.  We don't need
	         to close the descriptor but BFD lacks a way of closing the
	         BFD without closing the descriptor.  */
	      obfd_filename = bfd_get_filename (objfile->obfd);

	      /* APPLE LOCAL: Remember to remove its sections from the 
		 target "to_sections".  Normally this is done in 
		 free_objfile, but here we're remaking the objfile
	         "in place" so we have to do it by hand.  */
	      remove_target_sections (objfile->obfd);

	      if (!bfd_close (objfile->obfd))
		error (_("Can't close BFD for %s: %s"), objfile->name,
		       bfd_errmsg (bfd_get_error ()));
	      objfile->obfd = bfd_openr (obfd_filename, gnutarget);
	      if (objfile->obfd == NULL)
		error (_("Can't open %s to read symbols."), objfile->name);
	      /* bfd_openr sets cacheable to true, which is what we want.  */

	      /* APPLE LOCAL: If the file is an archive file (i.e. fat
		 binary), look for sub-files that match the current
		 osabi. */

	      if (bfd_check_format (objfile->obfd, bfd_archive))
		{
		  bfd *tmp_bfd;
		  tmp_bfd = open_bfd_matching_arch (objfile->obfd, bfd_object);
		  if (tmp_bfd != NULL)
		    objfile->obfd = tmp_bfd;
		}

	      if (!bfd_check_format (objfile->obfd, bfd_object))
		error (_("Can't read symbols from %s: %s."), objfile->name,
		       bfd_errmsg (bfd_get_error ()));

	      /* Save the offsets, we will nuke them with the rest of the
	         objfile_obstack.  */
	      num_offsets = objfile->num_sections;
	      offsets = ((struct section_offsets *)
			 alloca (SIZEOF_N_SECTION_OFFSETS (num_offsets)));
	      memcpy (offsets, objfile->section_offsets,
		      SIZEOF_N_SECTION_OFFSETS (num_offsets));

	      /* Nuke all the state that we will re-read.  Much of the following
	         code which sets things to NULL really is necessary to tell
	         other parts of GDB that there is nothing currently there.  */

	      /* FIXME: Do we have to free a whole linked list, or is this
	         enough?  */
	      if (objfile->global_psymbols.list)
		xfree (objfile->global_psymbols.list);
	      memset (&objfile->global_psymbols, 0,
		      sizeof (objfile->global_psymbols));
	      if (objfile->static_psymbols.list)
		xfree (objfile->static_psymbols.list);
	      memset (&objfile->static_psymbols, 0,
		      sizeof (objfile->static_psymbols));

	      /* Free the obstacks for non-reusable objfiles */
	      bcache_xfree (objfile->psymbol_cache);
	      objfile->psymbol_cache = bcache_xmalloc (NULL);
	      bcache_xfree (objfile->macro_cache);
	      objfile->macro_cache = bcache_xmalloc (NULL);
	      /* APPLE LOCAL: Also delete the table of equivalent symbols.  */
	      equivalence_table_delete (objfile);
	      /* END APPLE LOCAL */
	      if (objfile->demangled_names_hash != NULL)
		{
		  htab_delete (objfile->demangled_names_hash);
		  objfile->demangled_names_hash = NULL;
		}
	      obstack_free (&objfile->objfile_obstack, 0);
	      objfile->sections = NULL;
	      objfile->symtabs = NULL;
	      objfile->psymtabs = NULL;
	      objfile->free_psymtabs = NULL;
	      objfile->cp_namespace_symtab = NULL;
	      objfile->msymbols = NULL;
	      objfile->deprecated_sym_private = NULL;
	      objfile->minimal_symbol_count = 0;
	      memset (&objfile->msymbol_hash, 0,
		      sizeof (objfile->msymbol_hash));
	      memset (&objfile->msymbol_demangled_hash, 0,
		      sizeof (objfile->msymbol_demangled_hash));
	      objfile->minimal_symbols_demangled = 0;
	      objfile->fundamental_types = NULL;
	      clear_objfile_data (objfile);
	      if (objfile->sf != NULL)
		{
		  (*objfile->sf->sym_finish) (objfile);
		}

	      /* We never make this a mapped file.  */
	      objfile->md = NULL;
	      objfile->psymbol_cache = bcache_xmalloc (NULL);
	      objfile->macro_cache = bcache_xmalloc (NULL);
	      /* obstack_init also initializes the obstack so it is
	         empty.  We could use obstack_specify_allocation but
	         gdb_obstack.h specifies the alloc/dealloc
	         functions.  */
	      obstack_init (&objfile->objfile_obstack);
	      if (build_objfile_section_table (objfile))
		{
		  error (_("Can't find the file sections in `%s': %s"),
			 objfile->name, bfd_errmsg (bfd_get_error ()));
		}
              terminate_minimal_symbol_table (objfile);

	      /* We use the same section offsets as from last time.  I'm not
	         sure whether that is always correct for shared libraries.  */
	      objfile->section_offsets = (struct section_offsets *)
		obstack_alloc (&objfile->objfile_obstack,
			       SIZEOF_N_SECTION_OFFSETS (num_offsets));

 	      /* APPLE LOCAL: instead of just stuffing the section offsets
 		 back into the objfile structure, set the new objfile offsets to
 		 0 and then relocate the objfile with the original offsets.  If the
 		 original offsets were 0, this is a low-cost operation, because
 		 relocate_objfile checks for no change.  
 		 We have to do this because, at least on Mac OS X, the way
 		 we would have gotten non-zero section offsets to begin with is 
 		 that the objfile got relocated by the dyld layer when the library
 		 was actually loaded.  However the dyld layer now thinks this 
 		 objfile is correctly slid (it has its own copy of this information, 
 		 and doesn't look at anything in the objfile to figure this out.
 		 So IT won't apply the slide again and we have to do it here.  */
 	      memset (objfile->section_offsets, 0, SIZEOF_N_SECTION_OFFSETS (num_offsets));

	      objfile->num_sections = num_offsets;
	      init_entry_point_info (objfile);

 	      objfile_relocate (objfile, offsets);

	      /* What the hell is sym_new_init for, anyway?  The concept of
	         distinguishing between the main file and additional files
	         in this way seems rather dubious.  */
	      if (objfile == symfile_objfile)
		{
		  (*objfile->sf->sym_new_init) (objfile);
		}

	      /* If the mtime has changed between the time we set new_modtime
	         and now, we *want* this to be out of date, so don't call stat
	         again now.  */
	      objfile->mtime = new_modtime;
	      reread_one = 1;
              reread_separate_symbols (objfile);

	      (*objfile->sf->sym_init) (objfile);
	      clear_complaints (&symfile_complaints, 1, 1);
	      /* The "mainline" parameter is a hideous hack; I think leaving it
	         zero is OK since dbxread.c also does what it needs to do if
	         objfile->global_psymbols.size is 0.  */
	      if ((objfile->symflags & ~OBJF_SYM_CONTAINER) & OBJF_SYM_LEVELS_MASK)
		(*objfile->sf->sym_read) (objfile, 0);
	      /* APPLE LOCAL don't complain about lack of symbols */
	      objfile->flags |= OBJF_SYMS;

	      /* We're done reading the symbol file; finish off complaints.  */
	      clear_complaints (&symfile_complaints, 0, 1);

	      /* Getting new symbols may change our opinion about what is
	         frameless.  */

	      reinit_frame_cache ();

	      /* Discard cleanups as symbol reading was successful.  */
	      discard_cleanups (old_cleanups);

	      /* APPLE LOCAL begin breakpoints */
	      /* Finally, remember to call breakpoint_re_set with this
		 objfile, so it will get on the change list.  */
	      breakpoint_re_set (objfile);
	      /* Also re-initialize the objc trampoline data in case it's the
		 objc library that's either just been read in or has changed.  */
	      if (objfile == find_libobjc_objfile ())
		objc_init_trampoline_observer ();
	      /* APPLE LOCAL end breakpoints */
	    }
	}
    }

  if (reread_one)
    {
      clear_symtab_users ();
      /* At least one objfile has changed, so we can consider that
         the executable we're debugging has changed too.  */
      observer_notify_executable_changed (NULL);
    }
}

/* APPLE LOCAL begin reread symbols */
static void
reread_symbols_command (char *args, int from_tty)
{
  reread_symbols ();
}
/* APPLE LOCAL end reread symbols */

/* APPLE LOCAL begin remove symbol file */
struct objfile *
find_objfile (const char *name)
{
  struct objfile *objfile = NULL;
  struct objfile *o, *temp;

  ALL_OBJFILES_SAFE (o, temp)
  {
    if (strcmp (name, o->name) == 0)
      {
	if (objfile == NULL)
	  {
	    objfile = o;
	  }
	else
	  {
	    warning ("Multiple object files exist with name \"%s\": choosing first", o->name);
	  }
      }
  }
  return objfile;
}

static void
remove_symbol_file_command (args, from_tty)
     char *args;
     int from_tty;
{
  char *name = NULL;
  struct objfile *objfile = NULL;

  dont_repeat ();
  if (args == NULL)
    {
      error ("remove-symbol-file takes a file name");
    }

  name = tilde_expand (args);
  make_cleanup (free, name);

  objfile = find_objfile (name);
  if (objfile == NULL)
    {
      error ("unable to locate object file named \"%s\"", args);
    }

  tell_breakpoints_objfile_changed (objfile);
  tell_objc_msgsend_cacher_objfile_changed (objfile);
  free_objfile (objfile);

  /* APPLE LOCAL cache lookup values for improved performance  */
  symtab_clear_cached_lookup_values ();
  clear_symtab_users ();

  /* changing symbols may change our opinion about what is frameless.  */
  reinit_frame_cache ();
}
/* APPLE LOCAL end remove symbol file */

/* Handle separate debug info for OBJFILE, which has just been
   re-read:
   - If we had separate debug info before, but now we don't, get rid
     of the separated objfile.
   - If we didn't have separated debug info before, but now we do,
     read in the new separated debug info file.
   - If the debug link points to a different file, toss the old one
     and read the new one.
   This function does *not* handle the case where objfile is still
   using the same separate debug info file, but that file's timestamp
   has changed.  That case should be handled by the loop in
   reread_symbols already.  */
static void
reread_separate_symbols (struct objfile *objfile)
{
  char *debug_file;

  /* Does the updated objfile's debug info live in a
     separate file?  */
  debug_file = find_separate_debug_file (objfile);

  if (objfile->separate_debug_objfile)
    {
      /* There are two cases where we need to get rid of
         the old separated debug info objfile:
         - if the new primary objfile doesn't have
         separated debug info, or
         - if the new primary objfile has separate debug
         info, but it's under a different filename.

         If the old and new objfiles both have separate
         debug info, under the same filename, then we're
         okay --- if the separated file's contents have
         changed, we will have caught that when we
         visited it in this function's outermost
         loop.  */
      if (! debug_file
          || strcmp (debug_file, objfile->separate_debug_objfile->name) != 0)
	{
	  free_objfile (objfile->separate_debug_objfile);
          /* APPLE LOCAL */
	  objfile->separate_debug_objfile = NULL;
	}
    }

  /* If the new objfile has separate debug info, and we
     haven't loaded it already, do so now.  */
  if (debug_file && ! objfile->separate_debug_objfile)
    {
      /* Use the same section offset table as objfile itself.
         Preserve the flags from objfile that make sense.  */
      struct section_offsets *sym_offsets = NULL;
      int num_sym_offsets = 0;
      bfd *debug_bfd;
      int uuid_matches;

      /* APPLE LOCAL: Handle the possible offset of file & separate debug file.  */
      debug_bfd = symfile_bfd_open (debug_file, 0);

      /* Don't bother to make the debug_objfile if the UUID's don't
	 match.  */
      if (objfile->not_loaded_kext_filename)
	/* FIXME will kextutil -s copy the uuid over to the output
	   binary?  Drop it?  Modify it?  That will determine what
	   should be done here.  Right now kextutil drops it.  
	   NB we have the original unloaded kext over in
	   objfile->not_loaded_kext_filename and we could try to
	   match that file's UUID with the dSYM's.  */
	uuid_matches = 1;
      else
	uuid_matches = check_bfd_for_matching_uuid (objfile->obfd, debug_bfd);

      if (uuid_matches)
	{
#ifdef TM_NEXTSTEP
	  /* This should really be a symbol file reader function to go along with
	     sym_offsets, but I don't want to have to push all the changes through
	     for that right now.  NOTE, this is a TM not an NM thing because even
	     the cross debugger uses dsym's.  */
	  macho_calculate_offsets_for_dsym (objfile, debug_bfd, NULL, 
					    objfile->section_offsets, objfile->num_sections,
					    &sym_offsets, &num_sym_offsets);
#endif /* TM_NEXTSTEP */
	  
	  objfile->separate_debug_objfile
	    = (symbol_file_add_with_addrs_or_offsets
	       (debug_bfd,
		info_verbose, /* from_tty: Don't override the default. */
		0, /* No addr table.  */
		sym_offsets, num_sym_offsets,
		0, /* Not mainline.  See comments about this above.  */
		objfile->flags & (OBJF_REORDERED | OBJF_SHARED | OBJF_READNOW
				  /* APPLE LOCAL symfile */
				  | OBJF_USERLOADED | OBJF_SEPARATE_DEBUG_FILE),
		OBJF_SYM_ALL, 0, NULL, 0));
	  
	  xfree (sym_offsets);
	  objfile->separate_debug_objfile->separate_debug_objfile_backlink
	    = objfile;
	  
	  /* APPLE LOCAL: Put the separate debug object before the normal one, 
	     this is so that usage of the ALL_OBJFILES_SAFE macro will stay safe. */
	  put_objfile_before (objfile->separate_debug_objfile, objfile);
	  
	}
      else
	bfd_close (debug_bfd);
    }
    
    /* APPLE LOCAL: Clean up our debug_file.  */
    if (debug_file)
      xfree (debug_file);
}





typedef struct
{
  char *ext;
  enum language lang;
}
filename_language;

static filename_language *filename_language_table;
static int fl_table_size, fl_table_next;

static void
add_filename_language (char *ext, enum language lang)
{
  if (fl_table_next >= fl_table_size)
    {
      fl_table_size += 10;
      filename_language_table =
	xrealloc (filename_language_table,
		  fl_table_size * sizeof (*filename_language_table));
    }

  filename_language_table[fl_table_next].ext = xstrdup (ext);
  filename_language_table[fl_table_next].lang = lang;
  fl_table_next++;
}

static char *ext_args;
static void
show_ext_args (struct ui_file *file, int from_tty,
	       struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
Mapping between filename extension and source language is \"%s\".\n"),
		    value);
}

static void
set_ext_lang_command (char *args, int from_tty, struct cmd_list_element *e)
{
  int i;
  char *cp = ext_args;
  enum language lang;

  /* First arg is filename extension, starting with '.' */
  if (*cp != '.')
    error (_("'%s': Filename extension must begin with '.'"), ext_args);

  /* Find end of first arg.  */
  while (*cp && !isspace (*cp))
    cp++;

  if (*cp == '\0')
    error (_("'%s': two arguments required -- filename extension and language"),
	   ext_args);

  /* Null-terminate first arg */
  *cp++ = '\0';

  /* Find beginning of second arg, which should be a source language.  */
  while (*cp && isspace (*cp))
    cp++;

  if (*cp == '\0')
    error (_("'%s': two arguments required -- filename extension and language"),
	   ext_args);

  /* Lookup the language from among those we know.  */
  lang = language_enum (cp);

  /* Now lookup the filename extension: do we already know it?  */
  for (i = 0; i < fl_table_next; i++)
    if (0 == strcmp (ext_args, filename_language_table[i].ext))
      break;

  if (i >= fl_table_next)
    {
      /* new file extension */
      add_filename_language (ext_args, lang);
    }
  else
    {
      /* redefining a previously known filename extension */

      /* if (from_tty) */
      /*   query ("Really make files of type %s '%s'?", */
      /*          ext_args, language_str (lang));           */

      xfree (filename_language_table[i].ext);
      filename_language_table[i].ext = xstrdup (ext_args);
      filename_language_table[i].lang = lang;
    }
}

static void
info_ext_lang_command (char *args, int from_tty)
{
  int i;

  printf_filtered (_("Filename extensions and the languages they represent:"));
  printf_filtered ("\n\n");
  for (i = 0; i < fl_table_next; i++)
    printf_filtered ("\t%s\t- %s\n",
		     filename_language_table[i].ext,
		     language_str (filename_language_table[i].lang));
}

static void
init_filename_language_table (void)
{
  if (fl_table_size == 0)	/* protect against repetition */
    {
      fl_table_size = 20;
      fl_table_next = 0;
      filename_language_table =
	xmalloc (fl_table_size * sizeof (*filename_language_table));
      add_filename_language (".c", language_c);
      add_filename_language (".C", language_cplus);
      add_filename_language (".cc", language_cplus);
      add_filename_language (".cp", language_cplus);
      add_filename_language (".cpp", language_cplus);
      add_filename_language (".cxx", language_cplus);
      add_filename_language (".c++", language_cplus);
      add_filename_language (".java", language_java);
      add_filename_language (".class", language_java);
      add_filename_language (".m", language_objc);
      /* APPLE LOCAL begin Objective-C++ */
      add_filename_language (".mm", language_objcplus);
      add_filename_language (".M", language_objcplus);
      /* APPLE LOCAL end Objective-C++ */
      add_filename_language (".f", language_fortran);
      add_filename_language (".F", language_fortran);
      add_filename_language (".s", language_asm);
      add_filename_language (".S", language_asm);
      add_filename_language (".pas", language_pascal);
      add_filename_language (".p", language_pascal);
      add_filename_language (".pp", language_pascal);
      add_filename_language (".adb", language_ada);
      add_filename_language (".ads", language_ada);
      add_filename_language (".a", language_ada);
      add_filename_language (".ada", language_ada);
    }
}

enum language
/* APPLE LOCAL const */
deduce_language_from_filename (const char *filename)
{
  int i;
  char *cp;

  if (filename != NULL)
    if ((cp = strrchr (filename, '.')) != NULL)
      for (i = 0; i < fl_table_next; i++)
	if (strcmp (cp, filename_language_table[i].ext) == 0)
	  return filename_language_table[i].lang;

  return language_unknown;
}

/* allocate_symtab:

   Allocate and partly initialize a new symbol table.  Return a pointer
   to it.  error() if no space.

   Caller must set these fields:
   LINETABLE(symtab)
   symtab->blockvector
   symtab->dirname
   symtab->free_code
   symtab->free_ptr
   possibly free_named_symtabs (symtab->filename);
 */

struct symtab *
allocate_symtab (char *filename, struct objfile *objfile)
{
  struct symtab *symtab;

  symtab = (struct symtab *)
    obstack_alloc (&objfile->objfile_obstack, sizeof (struct symtab));
  memset (symtab, 0, sizeof (*symtab));
  symtab->filename = obsavestring (filename, strlen (filename),
				   &objfile->objfile_obstack);
  symtab->fullname = NULL;
  symtab->language = deduce_language_from_filename (filename);
  symtab->debugformat = obsavestring ("unknown", 7,
				      &objfile->objfile_obstack);

  /* APPLE LOCAL fix-and-continue */
  SYMTAB_OBSOLETED (symtab) = 50;

  /* Hook it to the objfile it comes from */

  symtab->objfile = objfile;
  symtab->next = objfile->symtabs;
  objfile->symtabs = symtab;

  /* FIXME: This should go away.  It is only defined for the Z8000,
     and the Z8000 definition of this macro doesn't have anything to
     do with the now-nonexistent EXTRA_SYMTAB_INFO macro, it's just
     here for convenience.  */
#ifdef INIT_EXTRA_SYMTAB_INFO
  INIT_EXTRA_SYMTAB_INFO (symtab);
#endif

  return (symtab);
}

struct partial_symtab *
allocate_psymtab (char *filename, struct objfile *objfile)
{
  struct partial_symtab *psymtab;

  if (objfile->free_psymtabs)
    {
      psymtab = objfile->free_psymtabs;
      objfile->free_psymtabs = psymtab->next;
    }
  else
    psymtab = (struct partial_symtab *)
      obstack_alloc (&objfile->objfile_obstack,
		     sizeof (struct partial_symtab));

  memset (psymtab, 0, sizeof (struct partial_symtab));
  psymtab->filename = obsavestring (filename, strlen (filename),
				    &objfile->objfile_obstack);
  psymtab->symtab = NULL;

  /* Prepend it to the psymtab list for the objfile it belongs to.
     Psymtabs are searched in most recent inserted -> least recent
     inserted order. */

  psymtab->objfile = objfile;
  psymtab->next = objfile->psymtabs;
  objfile->psymtabs = psymtab;
#if 0
  {
    struct partial_symtab **prev_pst;
    psymtab->objfile = objfile;
    psymtab->next = NULL;
    prev_pst = &(objfile->psymtabs);
    while ((*prev_pst) != NULL)
      prev_pst = &((*prev_pst)->next);
    (*prev_pst) = psymtab;
  }
#endif

  /* APPLE LOCAL fix-and-continue */
  PSYMTAB_OBSOLETED (psymtab) = 50;

  /* APPLE LOCAL debug symbols left in .o files */
  PSYMTAB_OSO_NAME (psymtab) = NULL;
  PSYMTAB_OSO_STATICS (psymtab) = NULL;
  PSYMTAB_OSO_PST_LIST (psymtab) = NULL;

  return (psymtab);
}

void
discard_psymtab (struct partial_symtab *pst)
{
  struct partial_symtab **prev_pst;

  /* From dbxread.c:
     Empty psymtabs happen as a result of header files which don't
     have any symbols in them.  There can be a lot of them.  But this
     check is wrong, in that a psymtab with N_SLINE entries but
     nothing else is not empty, but we don't realize that.  Fixing
     that without slowing things down might be tricky.  */

  /* First, snip it out of the psymtab chain */

  prev_pst = &(pst->objfile->psymtabs);
  while ((*prev_pst) != pst)
    prev_pst = &((*prev_pst)->next);
  (*prev_pst) = pst->next;

  /* Next, put it on a free list for recycling */

  pst->next = pst->objfile->free_psymtabs;
  pst->objfile->free_psymtabs = pst;
}


/* Reset all data structures in gdb which may contain references to symbol
   table data.  */

void
clear_symtab_users (void)
{
  /* Someday, we should do better than this, by only blowing away
     the things that really need to be blown.  */

  /* Clear the "current" symtab first, because it is no longer valid.
     breakpoint_re_set may try to access the current symtab.  */
  clear_current_source_symtab_and_line ();

  clear_value_history ();
  clear_displays ();
  clear_internalvars ();
  /* APPLE LOCAL breakpoints (remove reset) */
  set_default_breakpoint (0, 0, 0, 0);
  clear_pc_function_cache ();
  if (deprecated_target_new_objfile_hook)
    deprecated_target_new_objfile_hook (NULL);
}

static void
clear_symtab_users_cleanup (void *ignore)
{
  clear_symtab_users ();
}

/* clear_symtab_users_once:

   This function is run after symbol reading, or from a cleanup.
   If an old symbol table was obsoleted, the old symbol table
   has been blown away, but the other GDB data structures that may
   reference it have not yet been cleared or re-directed.  (The old
   symtab was zapped, and the cleanup queued, in free_named_symtab()
   below.)

   This function can be queued N times as a cleanup, or called
   directly; it will do all the work the first time, and then will be a
   no-op until the next time it is queued.  This works by bumping a
   counter at queueing time.  Much later when the cleanup is run, or at
   the end of symbol processing (in case the cleanup is discarded), if
   the queued count is greater than the "done-count", we do the work
   and set the done-count to the queued count.  If the queued count is
   less than or equal to the done-count, we just ignore the call.  This
   is needed because reading a single .o file will often replace many
   symtabs (one per .h file, for example), and we don't want to reset
   the breakpoints N times in the user's face.

   The reason we both queue a cleanup, and call it directly after symbol
   reading, is because the cleanup protects us in case of errors, but is
   discarded if symbol reading is successful.  */

#if 0
/* FIXME:  As free_named_symtabs is currently a big noop this function
   is no longer needed.  */
static void clear_symtab_users_once (void);

static int clear_symtab_users_queued;
static int clear_symtab_users_done;

static void
clear_symtab_users_once (void)
{
  /* Enforce once-per-`do_cleanups'-semantics */
  if (clear_symtab_users_queued <= clear_symtab_users_done)
    return;
  clear_symtab_users_done = clear_symtab_users_queued;

  clear_symtab_users ();
}
#endif

/* Delete the specified psymtab, and any others that reference it.  */

static void
cashier_psymtab (struct partial_symtab *pst)
{
  struct partial_symtab *ps, *pprev = NULL;
  int i;

  /* Find its previous psymtab in the chain */
  /* APPLE LOCAL ALL_OBJFILE_PSYMTABS */
  ALL_OBJFILE_PSYMTABS (pst->objfile, ps)
    {
      if (ps == pst)
	break;
      pprev = ps;
    }

  if (ps)
    {
      /* Unhook it from the chain.  */
      if (ps == pst->objfile->psymtabs)
	pst->objfile->psymtabs = ps->next;
      else
	pprev->next = ps->next;

      /* FIXME, we can't conveniently deallocate the entries in the
         partial_symbol lists (global_psymbols/static_psymbols) that
         this psymtab points to.  These just take up space until all
         the psymtabs are reclaimed.  Ditto the dependencies list and
         filename, which are all in the objfile_obstack.  */

      /* We need to cashier any psymtab that has this one as a dependency... */
    again:
      /* APPLE LOCAL ALL_OBJFILE_PSYMTABS */
      ALL_OBJFILE_PSYMTABS (pst->objfile, ps)
	{
	  for (i = 0; i < ps->number_of_dependencies; i++)
	    {
	      if (ps->dependencies[i] == pst)
		{
		  cashier_psymtab (ps);
		  goto again;	/* Must restart, chain has been munged. */
		}
	    }
	}
    }
}

/* If a symtab or psymtab for filename NAME is found, free it along
   with any dependent breakpoints, displays, etc.
   Used when loading new versions of object modules with the "add-file"
   command.  This is only called on the top-level symtab or psymtab's name;
   it is not called for subsidiary files such as .h files.

   Return value is 1 if we blew away the environment, 0 if not.
   FIXME.  The return value appears to never be used.

   FIXME.  I think this is not the best way to do this.  We should
   work on being gentler to the environment while still cleaning up
   all stray pointers into the freed symtab.  */

int
free_named_symtabs (char *name)
{
#if 0
  /* FIXME:  With the new method of each objfile having it's own
     psymtab list, this function needs serious rethinking.  In particular,
     why was it ever necessary to toss psymtabs with specific compilation
     unit filenames, as opposed to all psymtabs from a particular symbol
     file?  -- fnf
     Well, the answer is that some systems permit reloading of particular
     compilation units.  We want to blow away any old info about these
     compilation units, regardless of which objfiles they arrived in. --gnu.  */

  struct symtab *s;
  struct symtab *prev;
  struct partial_symtab *ps;
  struct blockvector *bv;
  int blewit = 0;

  /* We only wack things if the symbol-reload switch is set.  */
  if (!symbol_reloading)
    return 0;

  /* Some symbol formats have trouble providing file names... */
  if (name == 0 || *name == '\0')
    return 0;

  /* Look for a psymtab with the specified name.  */

again2:
  for (ps = partial_symtab_list; ps; ps = ps->next)
    {
      if (strcmp (name, ps->filename) == 0)
	{
	  cashier_psymtab (ps);	/* Blow it away...and its little dog, too.  */
	  goto again2;		/* Must restart, chain has been munged */
	}
    }

  /* Look for a symtab with the specified name.  */

  for (s = symtab_list; s; s = s->next)
    {
      if (strcmp (name, s->filename) == 0)
	break;
      prev = s;
    }

  if (s)
    {
      if (s == symtab_list)
	symtab_list = s->next;
      else
	prev->next = s->next;

      /* For now, queue a delete for all breakpoints, displays, etc., whether
         or not they depend on the symtab being freed.  This should be
         changed so that only those data structures affected are deleted.  */

      /* But don't delete anything if the symtab is empty.
         This test is necessary due to a bug in "dbxread.c" that
         causes empty symtabs to be created for N_SO symbols that
         contain the pathname of the object file.  (This problem
         has been fixed in GDB 3.9x).  */

      bv = BLOCKVECTOR (s);
      if (BLOCKVECTOR_NBLOCKS (bv) > 2
	  || BLOCK_NSYMS (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK))
	  || BLOCK_NSYMS (BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK)))
	{
	  complaint (&symfile_complaints, _("Replacing old symbols for `%s'"),
		     name);
	  clear_symtab_users_queued++;
	  make_cleanup (clear_symtab_users_once, 0);
	  blewit = 1;
	}
      else
	complaint (&symfile_complaints, _("Empty symbol table found for `%s'"),
		   name);

      free_symtab (s);
    }
  else
    {
      /* It is still possible that some breakpoints will be affected
         even though no symtab was found, since the file might have
         been compiled without debugging, and hence not be associated
         with a symtab.  In order to handle this correctly, we would need
         to keep a list of text address ranges for undebuggable files.
         For now, we do nothing, since this is a fairly obscure case.  */
      ;
    }

  /* FIXME, what about the minimal symbol table? */
  return blewit;
#else
  return (0);
#endif
}

/* Allocate and partially fill a partial symtab.  It will be
   completely filled at the end of the symbol list.

   FILENAME is the name of the symbol-file we are reading from. */

struct partial_symtab *
start_psymtab_common (struct objfile *objfile,
		      struct section_offsets *section_offsets, char *filename,
		      CORE_ADDR textlow, struct partial_symbol **global_syms,
		      struct partial_symbol **static_syms)
{
  struct partial_symtab *psymtab;

  psymtab = allocate_psymtab (filename, objfile);
  psymtab->section_offsets = section_offsets;
  psymtab->textlow = textlow;
  psymtab->texthigh = psymtab->textlow;		/* default */
  psymtab->globals_offset = global_syms - objfile->global_psymbols.list;
  psymtab->statics_offset = static_syms - objfile->static_psymbols.list;
  return (psymtab);
}

/* Add a symbol with a long value to a psymtab.
   Since one arg is a struct, we pass in a ptr and deref it (sigh).
   Return the partial symbol that has been added.  */

/* NOTE: carlton/2003-09-11: The reason why we return the partial
   symbol is so that callers can get access to the symbol's demangled
   name, which they don't have any cheap way to determine otherwise.
   (Currenly, dwarf2read.c is the only file who uses that information,
   though it's possible that other readers might in the future.)
   Elena wasn't thrilled about that, and I don't blame her, but we
   couldn't come up with a better way to get that information.  If
   it's needed in other situations, we could consider breaking up
   SYMBOL_SET_NAMES to provide access to the demangled name lookup
   cache.  */

const struct partial_symbol *
add_psymbol_to_list (char *name, int namelength, domain_enum domain,
		     enum address_class class,
		     struct psymbol_allocation_list *list, long val,	/* Value as a long */
		     CORE_ADDR coreaddr,	/* Value as a CORE_ADDR */
		     enum language language, struct objfile *objfile)
{
  struct partial_symbol *psym;
  char *buf = alloca (namelength + 1);
  /* psymbol is static so that there will be no uninitialized gaps in the
     structure which might contain random data, causing cache misses in
     bcache. */
  static struct partial_symbol psymbol;

  /* Create local copy of the partial symbol */
  memcpy (buf, name, namelength);
  buf[namelength] = '\0';
  /* val and coreaddr are mutually exclusive, one of them *will* be zero */
  if (val != 0)
    {
      SYMBOL_VALUE (&psymbol) = val;
    }
  else
    {
      SYMBOL_VALUE_ADDRESS (&psymbol) = coreaddr;
    }
  SYMBOL_SECTION (&psymbol) = 0;
  SYMBOL_LANGUAGE (&psymbol) = language;
  PSYMBOL_DOMAIN (&psymbol) = domain;
  PSYMBOL_CLASS (&psymbol) = class;

  SYMBOL_SET_NAMES (&psymbol, buf, namelength, objfile);

  /* Stash the partial symbol away in the cache */
  psym = deprecated_bcache (&psymbol, sizeof (struct partial_symbol),
			    objfile->psymbol_cache);

  /* Save pointer to partial symbol in psymtab, growing symtab if needed. */
  if (list->next >= list->list + list->size)
    {
      extend_psymbol_list (list, objfile);
    }
  *list->next++ = psym;
  OBJSTAT (objfile, n_psyms++);

  return psym;
}

/* Add a symbol with a long value to a psymtab. This differs from
 * add_psymbol_to_list above in taking both a mangled and a demangled
 * name. */

void
add_psymbol_with_dem_name_to_list (char *name, int namelength, char *dem_name,
				   int dem_namelength, domain_enum domain,
				   enum address_class class,
				   struct psymbol_allocation_list *list, long val,	/* Value as a long */
				   CORE_ADDR coreaddr,	/* Value as a CORE_ADDR */
				   enum language language,
				   struct objfile *objfile)
{
  struct partial_symbol *psym;
  char *buf = alloca (namelength + 1);
  /* psymbol is static so that there will be no uninitialized gaps in the
     structure which might contain random data, causing cache misses in
     bcache. */
  static struct partial_symbol psymbol;

  /* Create local copy of the partial symbol */

  memcpy (buf, name, namelength);
  buf[namelength] = '\0';
  DEPRECATED_SYMBOL_NAME (&psymbol) = deprecated_bcache (buf, namelength + 1,
							 objfile->psymbol_cache);

  buf = alloca (dem_namelength + 1);
  memcpy (buf, dem_name, dem_namelength);
  buf[dem_namelength] = '\0';

  switch (language)
    {
    case language_c:
    case language_cplus:
      /* APPLE LOCAL Objective-C++ */
    case language_objcplus:
      SYMBOL_CPLUS_DEMANGLED_NAME (&psymbol) =
	deprecated_bcache (buf, dem_namelength + 1, objfile->psymbol_cache);
      break;
      /* FIXME What should be done for the default case? Ignoring for now. */
      /* APPLE LOCAL begin add default case */
    default:
      internal_error (__FILE__, __LINE__, "unhandled case");
      break;
      /* APPLE LOCAL end add default case */
    }

  /* val and coreaddr are mutually exclusive, one of them *will* be zero */
  if (val != 0)
    {
      SYMBOL_VALUE (&psymbol) = val;
    }
  else
    {
      SYMBOL_VALUE_ADDRESS (&psymbol) = coreaddr;
    }
  SYMBOL_SECTION (&psymbol) = 0;
  SYMBOL_LANGUAGE (&psymbol) = language;
  PSYMBOL_DOMAIN (&psymbol) = domain;
  PSYMBOL_CLASS (&psymbol) = class;
  SYMBOL_INIT_LANGUAGE_SPECIFIC (&psymbol, language);

  /* Stash the partial symbol away in the cache */
  psym = deprecated_bcache (&psymbol, sizeof (struct partial_symbol),
			    objfile->psymbol_cache);

  /* Save pointer to partial symbol in psymtab, growing symtab if needed. */
  if (list->next >= list->list + list->size)
    {
      extend_psymbol_list (list, objfile);
    }
  *list->next++ = psym;
  OBJSTAT (objfile, n_psyms++);
}

/* Initialize storage for partial symbols.  */

void
init_psymbol_list (struct objfile *objfile, int total_symbols)
{
  /* Free any previously allocated psymbol lists.  */

  if (objfile->global_psymbols.list)
    {
      xfree (objfile->global_psymbols.list);
    }
  if (objfile->static_psymbols.list)
    {
      xfree (objfile->static_psymbols.list);
    }

  /* Current best guess is that approximately a twentieth
     of the total symbols (in a debugging file) are global or static
     oriented symbols */

  objfile->global_psymbols.size = total_symbols / 10;
  objfile->static_psymbols.size = total_symbols / 10;

  if (objfile->global_psymbols.size > 0)
    {
      objfile->global_psymbols.next =
	objfile->global_psymbols.list = (struct partial_symbol **)
	xmalloc ((objfile->global_psymbols.size
		  * sizeof (struct partial_symbol *)));
    }
  if (objfile->static_psymbols.size > 0)
    {
      objfile->static_psymbols.next =
	objfile->static_psymbols.list = (struct partial_symbol **)
	xmalloc ((objfile->static_psymbols.size
		  * sizeof (struct partial_symbol *)));
    }
}

/* OVERLAYS:
   The following code implements an abstraction for debugging overlay sections.

   The target model is as follows:
   1) The gnu linker will permit multiple sections to be mapped into the
   same VMA, each with its own unique LMA (or load address).
   2) It is assumed that some runtime mechanism exists for mapping the
   sections, one by one, from the load address into the VMA address.
   3) This code provides a mechanism for gdb to keep track of which
   sections should be considered to be mapped from the VMA to the LMA.
   This information is used for symbol lookup, and memory read/write.
   For instance, if a section has been mapped then its contents
   should be read from the VMA, otherwise from the LMA.

   Two levels of debugger support for overlays are available.  One is
   "manual", in which the debugger relies on the user to tell it which
   overlays are currently mapped.  This level of support is
   implemented entirely in the core debugger, and the information about
   whether a section is mapped is kept in the objfile->obj_section table.

   The second level of support is "automatic", and is only available if
   the target-specific code provides functionality to read the target's
   overlay mapping table, and translate its contents for the debugger
   (by updating the mapped state information in the obj_section tables).

   The interface is as follows:
   User commands:
   overlay map <name>   -- tell gdb to consider this section mapped
   overlay unmap <name> -- tell gdb to consider this section unmapped
   overlay list         -- list the sections that GDB thinks are mapped
   overlay read-target  -- get the target's state of what's mapped
   overlay off/manual/auto -- set overlay debugging state
   Functional interface:
   find_pc_mapped_section(pc):    if the pc is in the range of a mapped
   section, return that section.
   find_pc_overlay(pc):       find any overlay section that contains
   the pc, either in its VMA or its LMA
   overlay_is_mapped(sect):       true if overlay is marked as mapped
   section_is_overlay(sect):      true if section's VMA != LMA
   pc_in_mapped_range(pc,sec):    true if pc belongs to section's VMA
   pc_in_unmapped_range(...):     true if pc belongs to section's LMA
   sections_overlap(sec1, sec2):  true if mapped sec1 and sec2 ranges overlap
   overlay_mapped_address(...):   map an address from section's LMA to VMA
   overlay_unmapped_address(...): map an address from section's VMA to LMA
   symbol_overlayed_address(...): Return a "current" address for symbol:
   either in VMA or LMA depending on whether
   the symbol's section is currently mapped
 */

/* Overlay debugging state: */

enum overlay_debugging_state overlay_debugging = ovly_off;
int overlay_cache_invalid = 0;	/* True if need to refresh mapped state */

/* Target vector for refreshing overlay mapped state */
static void simple_overlay_update (struct obj_section *);
void (*target_overlay_update) (struct obj_section *) = simple_overlay_update;

/* Function: section_is_overlay (SECTION)
   Returns true if SECTION has VMA not equal to LMA, ie.
   SECTION is loaded at an address different from where it will "run".  */

int
section_is_overlay (asection *section)
{
  /* FIXME: need bfd *, so we can use bfd_section_lma methods. */

  if (overlay_debugging)
    if (section && section->lma != 0 &&
	section->vma != section->lma)
      return 1;

  return 0;
}

/* Function: overlay_invalidate_all (void)
   Invalidate the mapped state of all overlay sections (mark it as stale).  */

static void
overlay_invalidate_all (void)
{
  struct objfile *objfile;
  struct obj_section *sect;

  ALL_OBJSECTIONS (objfile, sect)
    if (section_is_overlay (sect->the_bfd_section))
    sect->ovly_mapped = -1;
}

/* Function: overlay_is_mapped (SECTION)
   Returns true if section is an overlay, and is currently mapped.
   Private: public access is thru function section_is_mapped.

   Access to the ovly_mapped flag is restricted to this function, so
   that we can do automatic update.  If the global flag
   OVERLAY_CACHE_INVALID is set (by wait_for_inferior), then call
   overlay_invalidate_all.  If the mapped state of the particular
   section is stale, then call TARGET_OVERLAY_UPDATE to refresh it.  */

static int
overlay_is_mapped (struct obj_section *osect)
{
  if (osect == 0 || !section_is_overlay (osect->the_bfd_section))
    return 0;

  switch (overlay_debugging)
    {
    default:
    case ovly_off:
      return 0;			/* overlay debugging off */
    case ovly_auto:		/* overlay debugging automatic */
      /* Unles there is a target_overlay_update function,
         there's really nothing useful to do here (can't really go auto)  */
      if (target_overlay_update)
	{
	  if (overlay_cache_invalid)
	    {
	      overlay_invalidate_all ();
	      overlay_cache_invalid = 0;
	    }
	  if (osect->ovly_mapped == -1)
	    (*target_overlay_update) (osect);
	}
      /* fall thru to manual case */
    case ovly_on:		/* overlay debugging manual */
      return osect->ovly_mapped == 1;
    }
}

/* Function: section_is_mapped
   Returns true if section is an overlay, and is currently mapped.  */

int
section_is_mapped (asection *section)
{
  struct objfile *objfile;
  struct obj_section *osect;

  if (overlay_debugging)
    if (section && section_is_overlay (section))
      ALL_OBJSECTIONS (objfile, osect)
	if (osect->the_bfd_section == section)
	return overlay_is_mapped (osect);

  return 0;
}

/* Function: pc_in_unmapped_range
   If PC falls into the lma range of SECTION, return true, else false.  */

CORE_ADDR
pc_in_unmapped_range (CORE_ADDR pc, asection *section)
{
  /* FIXME: need bfd *, so we can use bfd_section_lma methods. */

  int size;

  if (overlay_debugging)
    if (section && section_is_overlay (section))
      {
	size = bfd_get_section_size (section);
	if (section->lma <= pc && pc < section->lma + size)
	  return 1;
      }
  return 0;
}

/* Function: pc_in_mapped_range
   If PC falls into the vma range of SECTION, return true, else false.  */

CORE_ADDR
pc_in_mapped_range (CORE_ADDR pc, asection *section)
{
  /* FIXME: need bfd *, so we can use bfd_section_vma methods. */

  int size;

  if (overlay_debugging)
    if (section && section_is_overlay (section))
      {
	size = bfd_get_section_size (section);
	if (section->vma <= pc && pc < section->vma + size)
	  return 1;
      }
  return 0;
}


/* Return true if the mapped ranges of sections A and B overlap, false
   otherwise.  */
static int
sections_overlap (asection *a, asection *b)
{
  /* FIXME: need bfd *, so we can use bfd_section_vma methods. */

  CORE_ADDR a_start = a->vma;
  CORE_ADDR a_end = a->vma + bfd_get_section_size (a);
  CORE_ADDR b_start = b->vma;
  CORE_ADDR b_end = b->vma + bfd_get_section_size (b);

  return (a_start < b_end && b_start < a_end);
}

/* Function: overlay_unmapped_address (PC, SECTION)
   Returns the address corresponding to PC in the unmapped (load) range.
   May be the same as PC.  */

CORE_ADDR
overlay_unmapped_address (CORE_ADDR pc, asection *section)
{
  /* FIXME: need bfd *, so we can use bfd_section_lma methods. */

  if (overlay_debugging)
    if (section && section_is_overlay (section) &&
	pc_in_mapped_range (pc, section))
      return pc + section->lma - section->vma;

  return pc;
}

/* Function: overlay_mapped_address (PC, SECTION)
   Returns the address corresponding to PC in the mapped (runtime) range.
   May be the same as PC.  */

CORE_ADDR
overlay_mapped_address (CORE_ADDR pc, asection *section)
{
  /* FIXME: need bfd *, so we can use bfd_section_vma methods. */

  if (overlay_debugging)
    if (section && section_is_overlay (section) &&
	pc_in_unmapped_range (pc, section))
      return pc + section->vma - section->lma;

  return pc;
}


/* Function: symbol_overlayed_address
   Return one of two addresses (relative to the VMA or to the LMA),
   depending on whether the section is mapped or not.  */

CORE_ADDR
symbol_overlayed_address (CORE_ADDR address, asection *section)
{
  if (overlay_debugging)
    {
      /* If the symbol has no section, just return its regular address. */
      if (section == 0)
	return address;
      /* If the symbol's section is not an overlay, just return its address */
      if (!section_is_overlay (section))
	return address;
      /* If the symbol's section is mapped, just return its address */
      if (section_is_mapped (section))
	return address;
      /*
       * HOWEVER: if the symbol is in an overlay section which is NOT mapped,
       * then return its LOADED address rather than its vma address!!
       */
      return overlay_unmapped_address (address, section);
    }
  return address;
}

/* Function: find_pc_overlay (PC)
   Return the best-match overlay section for PC:
   If PC matches a mapped overlay section's VMA, return that section.
   Else if PC matches an unmapped section's VMA, return that section.
   Else if PC matches an unmapped section's LMA, return that section.  */

asection *
find_pc_overlay (CORE_ADDR pc)
{
  struct objfile *objfile;
  struct obj_section *osect, *best_match = NULL;

  /* APPLE LOCAL begin cache lookup values for improved performance  */
  if (pc == last_overlay_section_lookup_pc)
    return cached_overlay_section;

  last_overlay_section_lookup_pc = pc;
  /* APPLE LOCAL end cache lookup values for improved performance  */

  if (overlay_debugging)
    ALL_OBJSECTIONS (objfile, osect)
      if (section_is_overlay (osect->the_bfd_section))
      {
	if (pc_in_mapped_range (pc, osect->the_bfd_section))
	  {
	    if (overlay_is_mapped (osect))
	      /* APPLE LOCAL begin cache lookup values for improved 
		 performance  */
	      {
		cached_overlay_section = osect->the_bfd_section;
		return osect->the_bfd_section;
	      }
	    /* APPLE LOCAL end cache lookup values for improved performance  */
	    else
	      best_match = osect;
	  }
	else if (pc_in_unmapped_range (pc, osect->the_bfd_section))
	  best_match = osect;
      }

  /* APPLE LOCAL begin cache lookup values for improved performance  */
  cached_overlay_section = best_match ? best_match->the_bfd_section : NULL;
  return best_match ? best_match->the_bfd_section : NULL;
  /* APPLE LOCAL end cache lookup values for improved performance  */}

/* Function: find_pc_mapped_section (PC)
   If PC falls into the VMA address range of an overlay section that is
   currently marked as MAPPED, return that section.  Else return NULL.  */

asection *
find_pc_mapped_section (CORE_ADDR pc)
{
  struct objfile *objfile;
  struct obj_section *osect;

  /* APPLE LOCAL begin cache lookup values for improved performance  */
  if (pc == last_mapped_section_lookup_pc)
    return cached_mapped_section;

  last_mapped_section_lookup_pc = pc;
  /* APPLE LOCAL end cache lookup values for improved performance  */

  if (overlay_debugging)
    ALL_OBJSECTIONS (objfile, osect)
      if (pc_in_mapped_range (pc, osect->the_bfd_section) &&
	  overlay_is_mapped (osect))
        /* APPLE LOCAL begin cache lookup values for improved performance  */
	{
	  cached_mapped_section = osect->the_bfd_section;
	  return osect->the_bfd_section;
	}

  last_mapped_section_lookup_pc = INVALID_ADDRESS;
  cached_mapped_section = NULL;
  /* APPLE LOCAL end cache lookup values for improved performance  */
  return NULL;
}

/* Function: list_overlays_command
   Print a list of mapped sections and their PC ranges */

void
list_overlays_command (char *args, int from_tty)
{
  int nmapped = 0;
  struct objfile *objfile;
  struct obj_section *osect;

  if (overlay_debugging)
    ALL_OBJSECTIONS (objfile, osect)
      if (overlay_is_mapped (osect))
      {
	const char *name;
	bfd_vma lma, vma;
	int size;

	vma = bfd_section_vma (objfile->obfd, osect->the_bfd_section);
	lma = bfd_section_lma (objfile->obfd, osect->the_bfd_section);
	size = bfd_get_section_size (osect->the_bfd_section);
	name = bfd_section_name (objfile->obfd, osect->the_bfd_section);

	printf_filtered ("Section %s, loaded at ", name);
	deprecated_print_address_numeric (lma, 1, gdb_stdout);
	puts_filtered (" - ");
	deprecated_print_address_numeric (lma + size, 1, gdb_stdout);
	printf_filtered (", mapped at ");
	deprecated_print_address_numeric (vma, 1, gdb_stdout);
	puts_filtered (" - ");
	deprecated_print_address_numeric (vma + size, 1, gdb_stdout);
	puts_filtered ("\n");

	nmapped++;
      }
  if (nmapped == 0)
    printf_filtered (_("No sections are mapped.\n"));
}

/* Function: map_overlay_command
   Mark the named section as mapped (ie. residing at its VMA address).  */

void
map_overlay_command (char *args, int from_tty)
{
  struct objfile *objfile, *objfile2;
  struct obj_section *sec, *sec2;
  asection *bfdsec;

  if (!overlay_debugging)
    error (_("\
Overlay debugging not enabled.  Use either the 'overlay auto' or\n\
the 'overlay manual' command."));

  if (args == 0 || *args == 0)
    error (_("Argument required: name of an overlay section"));

  /* First, find a section matching the user supplied argument */
  ALL_OBJSECTIONS (objfile, sec)
    if (!strcmp (bfd_section_name (objfile->obfd, sec->the_bfd_section), args))
    {
      /* Now, check to see if the section is an overlay. */
      bfdsec = sec->the_bfd_section;
      if (!section_is_overlay (bfdsec))
	continue;		/* not an overlay section */

      /* Mark the overlay as "mapped" */
      sec->ovly_mapped = 1;

      /* Next, make a pass and unmap any sections that are
         overlapped by this new section: */
      ALL_OBJSECTIONS (objfile2, sec2)
	if (sec2->ovly_mapped
            && sec != sec2
            && sec->the_bfd_section != sec2->the_bfd_section
            && sections_overlap (sec->the_bfd_section,
                                 sec2->the_bfd_section))
	{
	  if (info_verbose)
	    printf_unfiltered (_("Note: section %s unmapped by overlap\n"),
			     bfd_section_name (objfile->obfd,
					       sec2->the_bfd_section));
	  sec2->ovly_mapped = 0;	/* sec2 overlaps sec: unmap sec2 */
	}
      return;
    }
  error (_("No overlay section called %s"), args);
}

/* Function: unmap_overlay_command
   Mark the overlay section as unmapped
   (ie. resident in its LMA address range, rather than the VMA range).  */

void
unmap_overlay_command (char *args, int from_tty)
{
  struct objfile *objfile;
  struct obj_section *sec;

  if (!overlay_debugging)
    error (_("\
Overlay debugging not enabled.  Use either the 'overlay auto' or\n\
the 'overlay manual' command."));

  if (args == 0 || *args == 0)
    error (_("Argument required: name of an overlay section"));

  /* First, find a section matching the user supplied argument */
  ALL_OBJSECTIONS (objfile, sec)
    if (!strcmp (bfd_section_name (objfile->obfd, sec->the_bfd_section), args))
    {
      if (!sec->ovly_mapped)
	error (_("Section %s is not mapped"), args);
      sec->ovly_mapped = 0;
      return;
    }
  error (_("No overlay section called %s"), args);
}

/* Function: overlay_auto_command
   A utility command to turn on overlay debugging.
   Possibly this should be done via a set/show command. */

static void
overlay_auto_command (char *args, int from_tty)
{
  overlay_debugging = ovly_auto;
  enable_overlay_breakpoints ();
  if (info_verbose)
    printf_unfiltered (_("Automatic overlay debugging enabled."));
}

/* Function: overlay_manual_command
   A utility command to turn on overlay debugging.
   Possibly this should be done via a set/show command. */

static void
overlay_manual_command (char *args, int from_tty)
{
  overlay_debugging = ovly_on;
  disable_overlay_breakpoints ();
  if (info_verbose)
    printf_unfiltered (_("Overlay debugging enabled."));
}

/* Function: overlay_off_command
   A utility command to turn on overlay debugging.
   Possibly this should be done via a set/show command. */

static void
overlay_off_command (char *args, int from_tty)
{
  overlay_debugging = ovly_off;
  disable_overlay_breakpoints ();
  if (info_verbose)
    printf_unfiltered (_("Overlay debugging disabled."));
}

static void
overlay_load_command (char *args, int from_tty)
{
  if (target_overlay_update)
    (*target_overlay_update) (NULL);
  else
    error (_("This target does not know how to read its overlay state."));
}

/* Function: overlay_command
   A place-holder for a mis-typed command */

/* Command list chain containing all defined "overlay" subcommands. */
struct cmd_list_element *overlaylist;

static void
overlay_command (char *args, int from_tty)
{
  printf_unfiltered
    ("\"overlay\" must be followed by the name of an overlay command.\n");
  help_list (overlaylist, "overlay ", -1, gdb_stdout);
}


/* Target Overlays for the "Simplest" overlay manager:

   This is GDB's default target overlay layer.  It works with the
   minimal overlay manager supplied as an example by Cygnus.  The
   entry point is via a function pointer "target_overlay_update",
   so targets that use a different runtime overlay manager can
   substitute their own overlay_update function and take over the
   function pointer.

   The overlay_update function pokes around in the target's data structures
   to see what overlays are mapped, and updates GDB's overlay mapping with
   this information.

   In this simple implementation, the target data structures are as follows:
   unsigned _novlys;            /# number of overlay sections #/
   unsigned _ovly_table[_novlys][4] = {
   {VMA, SIZE, LMA, MAPPED},    /# one entry per overlay section #/
   {..., ...,  ..., ...},
   }
   unsigned _novly_regions;     /# number of overlay regions #/
   unsigned _ovly_region_table[_novly_regions][3] = {
   {VMA, SIZE, MAPPED_TO_LMA},  /# one entry per overlay region #/
   {..., ...,  ...},
   }
   These functions will attempt to update GDB's mappedness state in the
   symbol section table, based on the target's mappedness state.

   To do this, we keep a cached copy of the target's _ovly_table, and
   attempt to detect when the cached copy is invalidated.  The main
   entry point is "simple_overlay_update(SECT), which looks up SECT in
   the cached table and re-reads only the entry for that section from
   the target (whenever possible).
 */

/* Cached, dynamically allocated copies of the target data structures: */
static unsigned (*cache_ovly_table)[4] = 0;
#if 0
static unsigned (*cache_ovly_region_table)[3] = 0;
#endif
static unsigned cache_novlys = 0;
#if 0
static unsigned cache_novly_regions = 0;
#endif
static CORE_ADDR cache_ovly_table_base = 0;
#if 0
static CORE_ADDR cache_ovly_region_table_base = 0;
#endif
enum ovly_index
  {
    VMA, SIZE, LMA, MAPPED
  };
#define TARGET_LONG_BYTES (TARGET_LONG_BIT / TARGET_CHAR_BIT)

/* Throw away the cached copy of _ovly_table */
static void
simple_free_overlay_table (void)
{
  if (cache_ovly_table)
    xfree (cache_ovly_table);
  cache_novlys = 0;
  cache_ovly_table = NULL;
  cache_ovly_table_base = 0;
}

#if 0
/* Throw away the cached copy of _ovly_region_table */
static void
simple_free_overlay_region_table (void)
{
  if (cache_ovly_region_table)
    xfree (cache_ovly_region_table);
  cache_novly_regions = 0;
  cache_ovly_region_table = NULL;
  cache_ovly_region_table_base = 0;
}
#endif

/* Read an array of ints from the target into a local buffer.
   Convert to host order.  int LEN is number of ints  */
static void
read_target_long_array (CORE_ADDR memaddr, unsigned int *myaddr, int len)
{
  /* FIXME (alloca): Not safe if array is very large. */
  char *buf = alloca (len * TARGET_LONG_BYTES);
  int i;

  read_memory (memaddr, buf, len * TARGET_LONG_BYTES);
  for (i = 0; i < len; i++)
    myaddr[i] = extract_unsigned_integer (TARGET_LONG_BYTES * i + buf,
					  TARGET_LONG_BYTES);
}

/* Find and grab a copy of the target _ovly_table
   (and _novlys, which is needed for the table's size) */
static int
simple_read_overlay_table (void)
{
  struct minimal_symbol *novlys_msym, *ovly_table_msym;

  simple_free_overlay_table ();
  novlys_msym = lookup_minimal_symbol ("_novlys", NULL, NULL);
  if (! novlys_msym)
    {
      error (_("Error reading inferior's overlay table: "
             "couldn't find `_novlys' variable\n"
             "in inferior.  Use `overlay manual' mode."));
      return 0;
    }

  ovly_table_msym = lookup_minimal_symbol ("_ovly_table", NULL, NULL);
  if (! ovly_table_msym)
    {
      error (_("Error reading inferior's overlay table: couldn't find "
             "`_ovly_table' array\n"
             "in inferior.  Use `overlay manual' mode."));
      return 0;
    }

  cache_novlys = read_memory_integer (SYMBOL_VALUE_ADDRESS (novlys_msym), 4);
  cache_ovly_table
    = (void *) xmalloc (cache_novlys * sizeof (*cache_ovly_table));
  cache_ovly_table_base = SYMBOL_VALUE_ADDRESS (ovly_table_msym);
  read_target_long_array (cache_ovly_table_base,
                          (int *) cache_ovly_table,
                          cache_novlys * 4);

  return 1;			/* SUCCESS */
}

#if 0
/* Find and grab a copy of the target _ovly_region_table
   (and _novly_regions, which is needed for the table's size) */
static int
simple_read_overlay_region_table (void)
{
  struct minimal_symbol *msym;

  simple_free_overlay_region_table ();
  msym = lookup_minimal_symbol ("_novly_regions", NULL, NULL);
  if (msym != NULL)
    cache_novly_regions = read_memory_integer (SYMBOL_VALUE_ADDRESS (msym), 4);
  else
    return 0;			/* failure */
  cache_ovly_region_table = (void *) xmalloc (cache_novly_regions * 12);
  if (cache_ovly_region_table != NULL)
    {
      msym = lookup_minimal_symbol ("_ovly_region_table", NULL, NULL);
      if (msym != NULL)
	{
	  cache_ovly_region_table_base = SYMBOL_VALUE_ADDRESS (msym);
	  read_target_long_array (cache_ovly_region_table_base,
				  (int *) cache_ovly_region_table,
				  cache_novly_regions * 3);
	}
      else
	return 0;		/* failure */
    }
  else
    return 0;			/* failure */
  return 1;			/* SUCCESS */
}
#endif

/* Function: simple_overlay_update_1
   A helper function for simple_overlay_update.  Assuming a cached copy
   of _ovly_table exists, look through it to find an entry whose vma,
   lma and size match those of OSECT.  Re-read the entry and make sure
   it still matches OSECT (else the table may no longer be valid).
   Set OSECT's mapped state to match the entry.  Return: 1 for
   success, 0 for failure.  */

static int
simple_overlay_update_1 (struct obj_section *osect)
{
  int i, size;
  asection *bsect = osect->the_bfd_section;

  size = bfd_get_section_size (osect->the_bfd_section);
  for (i = 0; i < cache_novlys; i++)
    if (cache_ovly_table[i][VMA] == bfd_section_vma (obfd, bsect)
	&& cache_ovly_table[i][LMA] == bfd_section_lma (obfd, bsect)
	/* && cache_ovly_table[i][SIZE] == size */ )
      {
	read_target_long_array (cache_ovly_table_base + i * TARGET_LONG_BYTES,
				(int *) cache_ovly_table[i], 4);
	if (cache_ovly_table[i][VMA] == bfd_section_vma (obfd, bsect)
	    && cache_ovly_table[i][LMA] == bfd_section_lma (obfd, bsect)
	    /* && cache_ovly_table[i][SIZE] == size */ )
	  {
	    osect->ovly_mapped = cache_ovly_table[i][MAPPED];
	    return 1;
	  }
	else	/* Warning!  Warning!  Target's ovly table has changed! */
	  return 0;
      }
  return 0;
}

/* Function: simple_overlay_update
   If OSECT is NULL, then update all sections' mapped state
   (after re-reading the entire target _ovly_table).
   If OSECT is non-NULL, then try to find a matching entry in the
   cached ovly_table and update only OSECT's mapped state.
   If a cached entry can't be found or the cache isn't valid, then
   re-read the entire cache, and go ahead and update all sections.  */

static void
simple_overlay_update (struct obj_section *osect)
{
  struct objfile *objfile;

  /* Were we given an osect to look up?  NULL means do all of them. */
  if (osect)
    /* Have we got a cached copy of the target's overlay table? */
    if (cache_ovly_table != NULL)
      /* Does its cached location match what's currently in the symtab? */
      if (cache_ovly_table_base ==
	  SYMBOL_VALUE_ADDRESS (lookup_minimal_symbol ("_ovly_table", NULL, NULL)))
	/* Then go ahead and try to look up this single section in the cache */
	if (simple_overlay_update_1 (osect))
	  /* Found it!  We're done. */
	  return;

  /* Cached table no good: need to read the entire table anew.
     Or else we want all the sections, in which case it's actually
     more efficient to read the whole table in one block anyway.  */

  if (! simple_read_overlay_table ())
    return;

  /* Now may as well update all sections, even if only one was requested. */
  ALL_OBJSECTIONS (objfile, osect)
    if (section_is_overlay (osect->the_bfd_section))
    {
      int i, size;
      asection *bsect = osect->the_bfd_section;

      size = bfd_get_section_size (bsect);
      for (i = 0; i < cache_novlys; i++)
	if (cache_ovly_table[i][VMA] == bfd_section_vma (obfd, bsect)
	    && cache_ovly_table[i][LMA] == bfd_section_lma (obfd, bsect)
	    /* && cache_ovly_table[i][SIZE] == size */ )
	  { /* obj_section matches i'th entry in ovly_table */
	    osect->ovly_mapped = cache_ovly_table[i][MAPPED];
	    break;		/* finished with inner for loop: break out */
	  }
    }
}

/* Set the output sections and output offsets for section SECTP in
   ABFD.  The relocation code in BFD will read these offsets, so we
   need to be sure they're initialized.  We map each section to itself,
   with no offset; this means that SECTP->vma will be honored.  */

static void
symfile_dummy_outputs (bfd *abfd, asection *sectp, void *dummy)
{
  sectp->output_section = sectp;
  sectp->output_offset = 0;
}

/* Relocate the contents of a debug section SECTP in ABFD.  The
   contents are stored in BUF if it is non-NULL, or returned in a
   malloc'd buffer otherwise.

   For some platforms and debug info formats, shared libraries contain
   relocations against the debug sections (particularly for DWARF-2;
   one affected platform is PowerPC GNU/Linux, although it depends on
   the version of the linker in use).  Also, ELF object files naturally
   have unresolved relocations for their debug sections.  We need to apply
   the relocations in order to get the locations of symbols correct.  */

bfd_byte *
symfile_relocate_debug_section (bfd *abfd, asection *sectp, bfd_byte *buf)
{
  /* We're only interested in debugging sections with relocation
     information.  */
  if ((sectp->flags & SEC_RELOC) == 0)
    return NULL;
  if ((sectp->flags & SEC_DEBUGGING) == 0)
    return NULL;

  /* We will handle section offsets properly elsewhere, so relocate as if
     all sections begin at 0.  */
  bfd_map_over_sections (abfd, symfile_dummy_outputs, NULL);

  return bfd_simple_get_relocated_section_contents (abfd, sectp, buf, NULL);
}

/* APPLE LOCAL begin symfile */
struct symbol_file_info {
  bfd *abfd;
  int from_tty;
  struct section_addr_info *addrs;
  struct section_offsets *offsets;
  int num_offsets;
  int mainline;
  int flags;
  int symflags;
  CORE_ADDR mapaddr;
  const char *prefix;
  struct objfile *result;
  char *kext_bundle;
};  
 
int 
symbol_file_add_bfd_helper (void *v)
{
  struct symbol_file_info *s = (struct symbol_file_info *) v;
  s->result = symbol_file_add_with_addrs_or_offsets
    (s->abfd, s->from_tty, s->addrs, s->offsets, s->num_offsets, s->mainline, s->flags, 
     s->symflags, s->mapaddr, s->prefix, s->kext_bundle);
  return 1;
}

/* APPLE LOCAL: I added the _using_objfile one so we could change both the
   objfile & it's commpage at once without breaking ALL_OBJFILES_SAFE.  I
   didn't add the "safe" version because with the TRY_CATCH stuff that seems
   rather pointless.  */
struct objfile *
symbol_file_add_bfd_using_objfile (struct objfile *objfile, 
				   bfd *abfd, int from_tty, 
				   struct section_addr_info *addrs,
				   struct section_offsets *offsets,
				   int mainline, int flags, int symflags, 
				   CORE_ADDR mapaddr, const char *prefix)
{
  return symbol_file_add_with_addrs_or_offsets_using_objfile (objfile, abfd, from_tty,
							      addrs, offsets, 0, mainline,
							      flags, symflags,
							      mapaddr, prefix, NULL);
}

struct objfile *
symbol_file_add_bfd_safe (bfd *abfd, int from_tty, 
			  struct section_addr_info *addrs,
			  struct section_offsets *offsets,
			  int mainline, int flags, int symflags, 
			  CORE_ADDR mapaddr, const char *prefix, 
                          char *kext_bundle)
{
  struct symbol_file_info s;
  int ret;
 
  s.abfd = abfd;
  s.from_tty = from_tty;
  s.addrs = addrs;
  s.offsets = offsets;
  if (offsets)
    {
      /* We only make section offsets for the bfd sections
	 that make it into the objfile.  */
      int i;
      struct bfd_section *this_sect;
      s.num_offsets = 0;
      this_sect = abfd->sections;
      for (i = 0; i < bfd_count_sections (abfd); i++, this_sect = this_sect->next)
	{
	  if (objfile_keeps_section (abfd, this_sect))
	    s.num_offsets++;
	}
    }
  else
    s.num_offsets = 0;

  s.mainline = mainline;
  s.flags = flags;
  s.symflags = symflags;
  s.mapaddr = mapaddr;
  s.prefix = prefix;
  s.result = NULL;
  s.kext_bundle = kext_bundle;
 
  ret = catch_errors
    (symbol_file_add_bfd_helper, &s, "unable to load symbol file: ", RETURN_MASK_ALL);
   
  return s.result;
}
 
struct bfd_file_info {
  const char *filename;
  int mainline;
  bfd *result;
};  
 
int 
symfile_bfd_open_helper (void *v)
{
  struct bfd_file_info *s = (struct bfd_file_info *) v;
  s->result = symfile_bfd_open (s->filename, s->mainline);
  return 1;
}
 
bfd *
symfile_bfd_open_safe (const char *filename, int mainline)
{
  struct bfd_file_info s;
  int ret;
 
  s.filename = filename;
  s.mainline = mainline;
  s.result = NULL;
 
  ret = catch_errors
    (symfile_bfd_open_helper, &s, "unable to open symbol file: ", RETURN_MASK_ALL);
 
  return s.result;
}
/* APPLE LOCAL end symfile */

/* APPLE LOCAL: This routine opens the slice of a fat file (faking as a
   bfd_archive) that matches the current architecture.  */

bfd *
open_bfd_matching_arch (bfd *archive_bfd, bfd_format expected_format)
{
  enum gdb_osabi osabi = GDB_OSABI_UNINITIALIZED;
  bfd *abfd = NULL;
  
#if defined (TARGET_ARM) && defined (TM_NEXTSTEP)

  /* APPLE LOCAL: The model for Darwin ARM stuff doesn't fit well
     with the way PPC works.  You don't choose the fork that 
     "matches" the osabi, you choose the "best match", so if you
     are armv6, you pick armv6 if present, otherwise you pick
     armv4t...  */

  bfd *fallback = NULL;

#ifdef NM_NEXTSTEP

  /* We have a native ARM gdb, so query for V6 from the system.  */
  extern int arm_mach_o_query_v6 (void);
  if (arm_mach_o_query_v6 ())
    osabi = GDB_OSABI_DARWINV6;
  else
    osabi = GDB_OSABI_DARWIN;

#else	/* NM_NEXTSTEP */

  /* We have a cross ARM gdb, so check if the user has set the ABI 
     manually. If the osabi hasn't been set manually, just get the
     best one from this file.  */
  
  /* Get the user set osabi, or the default one.  */
  enum gdb_osabi default_osabi = gdbarch_lookup_osabi (NULL);
  
  /* Get the osabi for the bfd.  */
  osabi = gdbarch_lookup_osabi (archive_bfd);
  
#endif	/* NM_NEXTSTEP */

  for (;;)
    {
      enum gdb_osabi this_osabi;
      abfd = bfd_openr_next_archived_file (archive_bfd, abfd);
      if (abfd == NULL)
	break;
      if (! bfd_check_format (abfd, bfd_object))
	continue;
      this_osabi = gdbarch_lookup_osabi_from_bfd (abfd);
      if (this_osabi == osabi)
	{
	  return abfd;
	}
      else if (this_osabi == GDB_OSABI_DARWIN)
	fallback = abfd;
    }
  return fallback;
#else	/* defined (TARGET_ARM) && defined (TM_NEXTSTEP)  */
  osabi = gdbarch_osabi (current_gdbarch);
  if ((osabi <= GDB_OSABI_UNKNOWN) || (osabi >= GDB_OSABI_INVALID))
    osabi = gdbarch_lookup_osabi (archive_bfd);

  for (;;)
    {
      abfd = bfd_openr_next_archived_file (archive_bfd, abfd);
      if (abfd == NULL)
        break;
      if (! bfd_check_format (abfd, expected_format))
        continue;
      if (osabi == gdbarch_lookup_osabi_from_bfd (abfd))
        {
          break;
        }
    }
  return abfd;
#endif	/* defined (TARGET_ARM) && defined (TM_NEXTSTEP)  */
}

void
_initialize_symfile (void)
{
  struct cmd_list_element *c;

  /* APPLE LOCAL begin */
#if HAVE_MMAP
   add_setshow_boolean_cmd ("mmap-symbol-files", class_obscure,
			    &mmap_symbol_files_flag, _("\
Set if GDB should use mmap() to read from external symbol files."), _("\
Show if GDB should use mmap() to read from external symbol files."), NULL,
			    NULL, NULL,
			    &setlist, &showlist);
#endif /* HAVE_MMAP */
 
   c = add_cmd ("reread-symbols", class_files, reread_symbols_command,
 	       "Usage: reread-symbols\n\
 Re-load the symbols from all known object files.",
 	       &cmdlist);
 
   c = add_cmd ("remove-symbol-file", class_files, remove_symbol_file_command,
 	       "Usage: remove-symbol-file FILE\n\
 Unload the symbols from FILE.",
 	       &cmdlist);
   set_cmd_completer (c, filename_completer);
   /* APPLE LOCAL end */

  c = add_cmd ("symbol-file", class_files, symbol_file_command, _("\
Load symbol table from executable file FILE.\n\
The `file' command can also load symbol tables, as well as setting the file\n\
to execute."), &cmdlist);
  set_cmd_completer (c, filename_completer);

  c = add_cmd ("add-symbol-file", class_files, add_symbol_file_command, _("\
Usage: add-symbol-file FILE ADDR [-s <SECT> <SECT_ADDR> -s <SECT> <SECT_ADDR> ...]\n\
Load the symbols from FILE, assuming FILE has been dynamically loaded.\n\
ADDR is the starting address of the file's text.\n\
The optional arguments are section-name section-address pairs and\n\
should be specified if the data and bss segments are not contiguous\n\
with the text.  SECT is a section name to be loaded at SECT_ADDR."),
	       &cmdlist);
  set_cmd_completer (c, filename_completer);

  c = add_cmd ("add-kext", class_files, add_kext_command, _("\
Usage: add-kext KEXTBUNDLE\n\
Load the symbols from KEXTBUNDLE, where KEXTBUNDLE is the bundle directory\n\
including a Contents/Info.plist file.  In the same directory you need the\n\
output from kextutil(8) -s/-a/etc which has the addresses where the kext\n\
was loaded in the kernel and you need the .dSYM file in that directory as well.\n"),
	       &cmdlist);
  set_cmd_completer (c, filename_completer);

  c = add_cmd ("add-dsym", class_files, add_dsym_command, _("\
Usage: add-dsym DSYM_FILE\n\
Load the symbols from DSYM_FILE, adding them to a library with\n\
the matching UUID."),
	       &cmdlist);
  set_cmd_completer (c, filename_completer);

  c = add_cmd ("add-shared-symbol-files", class_files,
	       add_shared_symbol_files_command, _("\
Load the symbols from shared objects in the dynamic linker's link map."),
	       &cmdlist);
  c = add_alias_cmd ("assf", "add-shared-symbol-files", class_files, 1,
		     &cmdlist);

  c = add_cmd ("load", class_files, load_command, _("\
Dynamically load FILE into the running program, and record its symbols\n\
for access from GDB."), &cmdlist);
  set_cmd_completer (c, filename_completer);
  /* c->completer_word_break_characters = gdb_completer_filename_word_break_characters; */ /* FIXME */

  add_setshow_boolean_cmd ("symbol-reloading", class_support,
			   &symbol_reloading, _("\
Set dynamic symbol table reloading multiple times in one run."), _("\
Show dynamic symbol table reloading multiple times in one run."), NULL,
			   NULL,
			   show_symbol_reloading,
			   &setlist, &showlist);

  add_prefix_cmd ("overlay", class_support, overlay_command,
		  _("Commands for debugging overlays."), &overlaylist,
		  "overlay ", 0, &cmdlist);

  add_com_alias ("ovly", "overlay", class_alias, 1);
  add_com_alias ("ov", "overlay", class_alias, 1);

  add_cmd ("map-overlay", class_support, map_overlay_command,
	   _("Assert that an overlay section is mapped."), &overlaylist);

  add_cmd ("unmap-overlay", class_support, unmap_overlay_command,
	   _("Assert that an overlay section is unmapped."), &overlaylist);

  add_cmd ("list-overlays", class_support, list_overlays_command,
	   _("List mappings of overlay sections."), &overlaylist);

  add_cmd ("manual", class_support, overlay_manual_command,
	   _("Enable overlay debugging."), &overlaylist);
  add_cmd ("off", class_support, overlay_off_command,
	   _("Disable overlay debugging."), &overlaylist);
  add_cmd ("auto", class_support, overlay_auto_command,
	   _("Enable automatic overlay debugging."), &overlaylist);
  add_cmd ("load-target", class_support, overlay_load_command,
	   _("Read the overlay mapping state from the target."), &overlaylist);

  /* Filename extension to source language lookup table: */
  init_filename_language_table ();
  add_setshow_string_noescape_cmd ("extension-language", class_files,
				   &ext_args, _("\
Set mapping between filename extension and source language."), _("\
Show mapping between filename extension and source language."), _("\
Usage: set extension-language .foo bar"),
				   set_ext_lang_command,
				   show_ext_args,
				   &setlist, &showlist);

  add_info ("extensions", info_ext_lang_command,
	    _("All filename extensions associated with a source language."));

  add_setshow_integer_cmd ("download-write-size", class_obscure,
			   &download_write_size, _("\
Set the write size used when downloading a program."), _("\
Show the write size used when downloading a program."), _("\
Only used when downloading a program onto a remote\n\
target. Specify zero, or a negative value, to disable\n\
blocked writes. The actual size of each transfer is also\n\
limited by the size of the target packet and the memory\n\
cache."),
			   NULL,
			   show_download_write_size,
			   &setlist, &showlist);

  /* APPLE LOCAL: For the add-kext command.  */
  add_setshow_optional_filename_cmd ("kext-symbol-file-path", class_support,
				     &kext_symbol_file_path, _("\
Set the directory where kextutil-generated sym files are searched for."), _("\
Show the directory where kextutil-generated sym files are searched for."), _("\
The kextutil-generated sym file is first searched for in the same\n\
directory as the kext bundle, then in the directory specified by \n\
kext-symbol-file-path.  A common use would be to have kext-symbol-file-path\n\
set to /tmp and the kextutil-generated .sym files put in /tmp with the kext\n\
bundle and dSYM in another location."),
				     NULL,
				     NULL,
				     &setlist, &showlist);

  debug_file_directory = xstrdup (DEBUGDIR);
  add_setshow_optional_filename_cmd ("debug-file-directory", class_support,
				     &debug_file_directory, _("\
Set the directory where separate debug symbols are searched for."), _("\
Show the directory where separate debug symbols are searched for."), _("\
Separate debug symbols are first searched for in the same\n\
directory as the binary, then in the `" DEBUG_SUBDIRECTORY "' subdirectory,\n\
and lastly at the path of the directory of the binary with\n\
the global debug-file directory prepended."),
				     NULL,
				     show_debug_file_directory,
				     &setlist, &showlist);
}
