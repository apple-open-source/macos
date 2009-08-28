/* Read dbx symbol tables and convert to internal format, for GDB.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995,
   1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004.
   Free Software Foundation, Inc.

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

/* This module provides three functions: dbx_symfile_init,
   which initializes to read a symbol file; dbx_new_init, which 
   discards existing cached information when all symbols are being
   discarded; and dbx_symfile_read, which reads a symbol table
   from a file.

   dbx_symfile_read only does the minimum work necessary for letting the
   user "name" things symbolically; it does not read the entire symtab.
   Instead, it reads the external and static symbols and puts them in partial
   symbol tables.  When more extensive information is requested of a
   file, the corresponding partial symbol table is mutated into a full
   fledged symbol table by going back and reading the symbols
   for real.  dbx_psymtab_to_symtab() is the function that does this */

#include "defs.h"
#include "gdb_string.h"

#if defined(__CYGNUSCLIB__)
#include <sys/types.h>
#include <fcntl.h>
#endif

/* APPLE LOCAL ? */
static int read_type_psym_p = 1;
static int end_fun_absolute_p = 0;

#include "gdb_obstack.h"
#include "gdb_stat.h"
#include "symtab.h"
#include "breakpoint.h"
/* APPLE LOCAL headers */
#include "command.h"
#include "gdbcmd.h"
#include "target.h"
#include "gdbcore.h"		/* for bfd stuff */
#include "libaout.h"		/* FIXME Secret internal BFD stuff for a.out */
#include "objfiles.h"
#include "buildsym.h"
#include "stabsread.h"
#include "gdb-stabs.h"
#include "demangle.h"
#include "complaints.h"
#include "cp-abi.h"

/* APPLE LOCAL: prototype for bfd_mach_o_version */
#include "mach-o.h"

/* APPLE LOCAL */
#include "block.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "aout/aout64.h"
#include "aout/stab_gnu.h"	/* We always use GNU stabs, not native, now */

/* We put a pointer to this structure in the read_symtab_private field
   of the psymtab.  */

struct symloc
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

#define LDSYMOFF(p) (((struct symloc *)((p)->read_symtab_private))->ldsymoff)
#define LDSYMLEN(p) (((struct symloc *)((p)->read_symtab_private))->ldsymlen)
#define SYMLOC(p) ((struct symloc *)((p)->read_symtab_private))
#define SYMBOL_SIZE(p) (SYMLOC(p)->symbol_size)
#define SYMBOL_OFFSET(p) (SYMLOC(p)->symbol_offset)
#define STRING_OFFSET(p) (SYMLOC(p)->string_offset)
#define FILE_STRING_OFFSET(p) (SYMLOC(p)->file_string_offset)
#define SYMBOL_PREFIX(p) (SYMLOC(p)->prefix)
/* End APPLE LOCAL */


/* Remember what we deduced to be the source language of this psymtab. */

static enum language psymtab_language = language_unknown;

/* The objfile currently being processed -- implicit parameter to next_symbol_text.  */

static struct objfile *processing_objfile;

/* The size of each symbol in the symbol file (in external form).
   This is set by dbx_symfile_read when building psymtabs, and by
   dbx_psymtab_to_symtab when building symtabs.  */

static unsigned symbol_size;

/* This is the offset of the symbol table in the executable file. */

static unsigned symbol_table_offset;

/* This is the offset of the string table in the executable file. */

static unsigned string_table_offset;

/* For elf+stab executables, the n_strx field is not a simple index
   into the string table.  Instead, each .o file has a base offset in
   the string table, and the associated symbols contain offsets from
   this base.  The following two variables contain the base offset for
   the current and next .o files. */

static unsigned int file_string_table_offset;
static unsigned int next_file_string_table_offset;

/* .o and NLM files contain unrelocated addresses which are based at
   0.  When non-zero, this flag disables some of the special cases for
   Solaris elf+stab text addresses at location 0. */

static int symfile_relocatable = 0;

/* If this is nonzero, N_LBRAC, N_RBRAC, and N_SLINE entries are
   relative to the function start address.  */

static int block_address_function_relative = 0;

/* The lowest text address we have yet encountered.  This is needed
   because in an a.out file, there is no header field which tells us
   what address the program is actually going to be loaded at, so we
   need to make guesses based on the symbols (which *are* relocated to
   reflect the address it will be loaded at).  */

static CORE_ADDR lowest_text_address;

/* Non-zero if there is any line number info in the objfile.  Prevents
   end_psymtab from discarding an otherwise empty psymtab.  */

static int has_line_numbers;

/* APPLE LOCAL: Forward declarations of some functions used in this file: */
static void read_ofile_symtab_from_oso (struct partial_symtab *pst, 
					struct bfd *oso_bfd);
static struct cleanup *
stabsect_read_strtab_from_oso (struct bfd *oso_bfd, int *symcount, 
			       int *symsize, int *strtab_size,
			       bfd_byte **stab_data_handle, char **strtab_data_handle);

/* Complaints about the symbols we have encountered.  */

static void
unknown_symtype_complaint (const char *arg1)
{
  complaint (&symfile_complaints, _("unknown symbol type %s"), arg1);
}

static void
lbrac_mismatch_complaint (int arg1)
{
  complaint (&symfile_complaints,
	     _("N_LBRAC/N_RBRAC symbol mismatch at symtab pos %d"), arg1);
}

static void
repeated_header_complaint (const char *arg1, int arg2)
{
  complaint (&symfile_complaints,
	     _("\"repeated\" header file %s not previously seen, at symtab pos %d"),
	     arg1, arg2);
}

/* find_text_range --- find start and end of loadable code sections

   The find_text_range function finds the shortest address range that
   encloses all sections containing executable code, and stores it in
   objfile's text_addr and text_size members.

   dbx_symfile_read will use this to finish off the partial symbol
   table, in some cases.  */

static void
find_text_range (bfd * sym_bfd, struct objfile *objfile)
{
  asection *sec;
  int found_any = 0;
  CORE_ADDR start = 0;
  CORE_ADDR end = 0;

  for (sec = sym_bfd->sections; sec; sec = sec->next)
    if (bfd_get_section_flags (sym_bfd, sec) & SEC_CODE)
      {
	CORE_ADDR sec_start = bfd_section_vma (sym_bfd, sec);
	CORE_ADDR sec_end = sec_start + bfd_section_size (sym_bfd, sec);

	if (found_any)
	  {
	    if (sec_start < start)
	      start = sec_start;
	    if (sec_end > end)
	      end = sec_end;
	  }
	else
	  {
	    start = sec_start;
	    end = sec_end;
	  }

	found_any = 1;
      }

  if (!found_any)
    error (_("Can't find any code sections in symbol file"));

  DBX_TEXT_ADDR (objfile) = start;
  DBX_TEXT_SIZE (objfile) = end - start;
}



/* During initial symbol readin, we need to have a structure to keep
   track of which psymtabs have which bincls in them.  This structure
   is used during readin to setup the list of dependencies within each
   partial symbol table. */

struct header_file_location
{
  char *name;			/* Name of header file */
  /* APPLE LOCAL */
  unsigned long hash;
  int instance;			/* See above */
  struct partial_symtab *pst;	/* Partial symtab that has the
				   BINCL/EINCL defs for this file */
};

/* The actual list and controling variables */
static struct header_file_location *bincl_list, *next_bincl;
static int bincls_allocated;

/* Local function prototypes */

extern void _initialize_dbxread (void);

static void read_ofile_symtab (struct partial_symtab *);

static void dbx_psymtab_to_symtab (struct partial_symtab *);

static void dbx_psymtab_to_symtab_1 (struct partial_symtab *);

static void read_dbx_dynamic_symtab (struct objfile *objfile);

/* APPLE LOCAL add argument */
static void read_dbx_symtab (struct objfile *, int);

static void free_bincl_list (struct objfile *);

static struct partial_symtab *find_corresponding_bincl_psymtab (char *, int);

static void add_bincl_to_list (struct partial_symtab *, char *, int);

static void init_bincl_list (int, struct objfile *);

static char *dbx_next_symbol_text (struct objfile *);

/* APPLE LOCAL pass objfile */
static void fill_symbuf (struct objfile *);

static void dbx_symfile_init (struct objfile *);

static void dbx_new_init (struct objfile *);

/* APPLE LOCAL make globally visible */
void dbx_symfile_read (struct objfile *, int);

static void dbx_symfile_finish (struct objfile *);

static void record_minimal_symbol (char *, CORE_ADDR, int, int16_t, struct objfile *);

static void add_new_header_file (char *, int);

static void add_old_header_file (char *, int);

static void add_this_object_header_file (int);

static struct partial_symtab *start_psymtab (struct objfile *, char *,
					     CORE_ADDR, int,
					     /* APPLE LOCAL add argument */
					     struct partial_symbol **,
					     struct partial_symbol **,
					     const char *prefix);

/* APPLE LOCAL begin oso */
static enum language read_so_stab_language_hint (short unsigned n_desc);

static int objfile_contains_objc (struct objfile *objfile);
/* APPLE LOCAL end oso */

/* Free up old header file tables */

void
free_header_files (void)
{
  if (this_object_header_files)
    {
      xfree (this_object_header_files);
      this_object_header_files = NULL;
    }
  n_allocated_this_object_header_files = 0;
}

/* Allocate new header file tables */

void
init_header_files (void)
{
  n_allocated_this_object_header_files = 10;
  this_object_header_files = (int *) xmalloc (10 * sizeof (int));
}

/* Add header file number I for this object file
   at the next successive FILENUM.  */

static void
add_this_object_header_file (int i)
{
  if (n_this_object_header_files == n_allocated_this_object_header_files)
    {
      n_allocated_this_object_header_files *= 2;
      this_object_header_files
	= (int *) xrealloc ((char *) this_object_header_files,
		       n_allocated_this_object_header_files * sizeof (int));
    }

  this_object_header_files[n_this_object_header_files++] = i;
}

/* Add to this file an "old" header file, one already seen in
   a previous object file.  NAME is the header file's name.
   INSTANCE is its instance code, to select among multiple
   symbol tables for the same header file.  */

static void
add_old_header_file (char *name, int instance)
{
  struct header_file *p = HEADER_FILES (current_objfile);
  int i;

  for (i = 0; i < N_HEADER_FILES (current_objfile); i++)
    if (strcmp (p[i].name, name) == 0 && instance == p[i].instance)
      {
	add_this_object_header_file (i);
	return;
      }
  repeated_header_complaint (name, symnum);
}

/* Add to this file a "new" header file: definitions for its types follow.
   NAME is the header file's name.
   Most often this happens only once for each distinct header file,
   but not necessarily.  If it happens more than once, INSTANCE has
   a different value each time, and references to the header file
   use INSTANCE values to select among them.

   dbx output contains "begin" and "end" markers for each new header file,
   but at this level we just need to know which files there have been;
   so we record the file when its "begin" is seen and ignore the "end".  */

static void
add_new_header_file (char *name, int instance)
{
  int i;
  struct header_file *hfile;

  /* Make sure there is room for one more header file.  */

  i = N_ALLOCATED_HEADER_FILES (current_objfile);

  if (N_HEADER_FILES (current_objfile) == i)
    {
      if (i == 0)
	{
	  N_ALLOCATED_HEADER_FILES (current_objfile) = 10;
	  HEADER_FILES (current_objfile) = (struct header_file *)
	    xmalloc (10 * sizeof (struct header_file));
	}
      else
	{
	  i *= 2;
	  N_ALLOCATED_HEADER_FILES (current_objfile) = i;
	  HEADER_FILES (current_objfile) = (struct header_file *)
	    xrealloc ((char *) HEADER_FILES (current_objfile),
		      (i * sizeof (struct header_file)));
	}
    }

  /* Create an entry for this header file.  */

  i = N_HEADER_FILES (current_objfile)++;
  hfile = HEADER_FILES (current_objfile) + i;
  hfile->name = savestring (name, strlen (name));
  hfile->instance = instance;
  hfile->length = 10;
  hfile->vector
    = (struct type **) xmalloc (10 * sizeof (struct type *));
  memset (hfile->vector, 0, 10 * sizeof (struct type *));

  add_this_object_header_file (i);
}

#if 0
static struct type **
explicit_lookup_type (int real_filenum, int index)
{
  struct header_file *f = &HEADER_FILES (current_objfile)[real_filenum];

  if (index >= f->length)
    {
      f->length *= 2;
      f->vector = (struct type **)
	xrealloc (f->vector, f->length * sizeof (struct type *));
      memset (&f->vector[f->length / 2],
	      '\0', f->length * sizeof (struct type *) / 2);
    }
  return &f->vector[index];
}
#endif

/* APPLE LOCAL: Pass in the desc along with the the type so we can
   see if this is a "special" symbol. */
static void
record_minimal_symbol (char *name, CORE_ADDR address, int type,
		       int16_t desc,
		       struct objfile *objfile)
{
  enum minimal_symbol_type ms_type;
  int section;
  asection *bfd_section;
  /* APPLE LOCAL: Save the return value from 
     prim_record_minimal_symbol_and_info so we have the ability to mark 
     it "special".  */
  struct minimal_symbol *msym;
  switch (type)
    {
    case N_TEXT | N_EXT:
      ms_type = mst_text;
      section = SECT_OFF_TEXT (objfile);
      if (address >= DBX_TEXT_ADDR (objfile) &&
	  address < DBX_TEXT_ADDR (objfile) + DBX_TEXT_SIZE (objfile))
	{
          /* APPLE LOCAL: DBX_TEXT_SECTION is an obj_section not an asection */
	  bfd_section = DBX_TEXT_SECTION (objfile)->the_bfd_section;
	}
      else if (address >= DBX_COALESCED_TEXT_ADDR (objfile) &&
	  address < DBX_COALESCED_TEXT_ADDR (objfile) + DBX_COALESCED_TEXT_SIZE (objfile))
	{
          /* APPLE LOCAL: DBX_COALESCEDTEXT_SECTION is an obj_section */
	  bfd_section = DBX_COALESCED_TEXT_SECTION (objfile)->the_bfd_section;
	}
      else
          /* APPLE LOCAL: DBX_TEXT_SECTION is an obj_section not an asection */
	  bfd_section = DBX_TEXT_SECTION (objfile)->the_bfd_section;
	
      break;
    case N_DATA | N_EXT:
      ms_type = mst_data;
      section = SECT_OFF_DATA (objfile);
      /* APPLE LOCAL: DBX_DATA_SECTION is an obj_section not an asection */
      bfd_section = DBX_DATA_SECTION (objfile)->the_bfd_section;
      break;
    case N_BSS | N_EXT:
      ms_type = mst_bss;
      section = SECT_OFF_BSS (objfile);
      /* APPLE LOCAL: DBX_BSS_SECTION is an obj_section not an asection */
      bfd_section = DBX_BSS_SECTION (objfile)->the_bfd_section;
      break;
    case N_ABS | N_EXT:
      ms_type = mst_abs;
      section = -1;
      bfd_section = NULL;
      break;
#ifdef N_SETV
    case N_SETV | N_EXT:
      ms_type = mst_data;
      section = SECT_OFF_DATA (objfile);
      /* APPLE LOCAL: DBX_DATA_SECTION is an obj_section not an asection */
      bfd_section = DBX_DATA_SECTION (objfile)->the_bfd_section;
      break;
    case N_SETV:
      /* I don't think this type actually exists; since a N_SETV is the result
         of going over many .o files, it doesn't make sense to have one
         file local.  */
      ms_type = mst_file_data;
      section = SECT_OFF_DATA (objfile);
      /* APPLE LOCAL: DBX_DATA_SECTION is an obj_section not an asection */
      bfd_section = DBX_DATA_SECTION (objfile)->the_bfd_section;
      break;
#endif
    case N_TEXT:
    case N_NBTEXT:
    case N_FN:
    case N_FN_SEQ:
      ms_type = mst_file_text;
      section = SECT_OFF_TEXT (objfile);
      if (address >= DBX_TEXT_ADDR (objfile) &&
	  address < DBX_TEXT_ADDR (objfile) + DBX_TEXT_SIZE (objfile))
	{
          /* APPLE LOCAL: DBX_TEXT_SECTION is an obj_section not an asection */
	  bfd_section = DBX_TEXT_SECTION (objfile)->the_bfd_section;
	}
      else if (address >= DBX_COALESCED_TEXT_ADDR (objfile) &&
	  address < DBX_COALESCED_TEXT_ADDR (objfile) + DBX_COALESCED_TEXT_SIZE (objfile))
	{
          /* APPLE LOCAL: DBX_COALESCED_TEXT_SECTION is an obj_section */
	  bfd_section = DBX_COALESCED_TEXT_SECTION (objfile)->the_bfd_section;
	}
      else
          /* APPLE LOCAL: DBX_TEXT_SECTION is an obj_section */
	  bfd_section = DBX_TEXT_SECTION (objfile)->the_bfd_section;
	
      break;
    case N_DATA:
      ms_type = mst_file_data;

      /* Check for __DYNAMIC, which is used by Sun shared libraries. 
         Record it as global even if it's local, not global, so
         lookup_minimal_symbol can find it.  We don't check symbol_leading_char
         because for SunOS4 it always is '_'.  */
      /* APPLE LOCAL: Don't try to optimize this by checking name[8] == 'C' --
         that can crash gdb if we have a name shorter than 8 chars long.  */
      if (strcmp ("__DYNAMIC", name) == 0)
	ms_type = mst_data;

      /* Same with virtual function tables, both global and static.  */
      {
	char *tempstring = name;
	if (tempstring[0] == bfd_get_symbol_leading_char (objfile->obfd))
	  ++tempstring;
	if (is_vtable_name (tempstring))
	  ms_type = mst_data;
      }
      section = SECT_OFF_DATA (objfile);
      /* APPLE LOCAL: DBX_DATA_SECTION is an obj_section not an asection */
      bfd_section = DBX_DATA_SECTION (objfile)->the_bfd_section;
      break;
    case N_BSS:
      ms_type = mst_file_bss;
      section = SECT_OFF_BSS (objfile);
      /* APPLE LOCAL: DBX_BSS_SECTION is an obj_section not an asection */
      bfd_section = DBX_BSS_SECTION (objfile)->the_bfd_section;
      break;
    default:
      ms_type = mst_unknown;
      section = -1;
      bfd_section = NULL;
      break;
    }

  if ((ms_type == mst_file_text || ms_type == mst_text)
      && address < lowest_text_address)
    lowest_text_address = address;

  /* APPLE LOCAL: Record the msymbol & make it special if it is.  */
  msym = prim_record_minimal_symbol_and_info
    (name, address, ms_type, NULL, section, bfd_section, objfile);
  DBX_MAKE_MSYMBOL_SPECIAL (desc, msym);
}

/* Scan and build partial symbols for a symbol file.
   We have been initialized by a call to dbx_symfile_init, which 
   put all the relevant info into a "struct dbx_symfile_info",
   hung off the objfile structure.

   MAINLINE is true if we are reading the main symbol
   table (as opposed to a shared lib or dynamically loaded file).  */

/* APPLE LOCAL make globally visible */
void
dbx_symfile_read (struct objfile *objfile, int mainline)
{
  bfd *sym_bfd;
  int val;
  struct cleanup *back_to;
  file_ptr dbx_symtab_offset;
  int dbx_symtab_count;
  /* APPLE LOCAL: timers */
  static int timer = -1;
  struct cleanup *timer_cleanup;

  /* APPLE LOCAL: If this is a dSYM that has minimal symbols, don't read the
     minsyms or we'll end up with duplicated minsyms.  */
  if (objfile->separate_debug_objfile_backlink || 
      objfile->flags & OBJF_SEPARATE_DEBUG_FILE)
    {
      return;
    }

  if (maint_use_timers)
    timer_cleanup = start_timer (&timer, "dbx_symfile_read", 
				 objfile->name ? objfile->name : "<unknown>");

  sym_bfd = objfile->obfd;

  /* .o and .nlm files are relocatables with text, data and bss segs based at
     0.  This flag disables special (Solaris stabs-in-elf only) fixups for
     symbols with a value of 0.  */

  symfile_relocatable = bfd_get_file_flags (sym_bfd) & HAS_RELOC;

  /* This is true for Solaris (and all other systems which put stabs
     in sections, hopefully, since it would be silly to do things
     differently from Solaris), and false for SunOS4 and other a.out
     file formats.  */
  block_address_function_relative =
    ((0 == strncmp (bfd_get_target (sym_bfd), "elf", 3))
     || (0 == strncmp (bfd_get_target (sym_bfd), "som", 3))
     || (0 == strncmp (bfd_get_target (sym_bfd), "coff", 4))
     || (0 == strncmp (bfd_get_target (sym_bfd), "pe", 2))
     || (0 == strncmp (bfd_get_target (sym_bfd), "epoc-pe", 7))
     || (0 == strncmp (bfd_get_target (sym_bfd), "nlm", 3)));

  /* APPLE LOCAL shared cache begin.  */
  if (bfd_mach_o_in_shared_cached_memory (objfile->obfd))
    {
      /* All shared libraries being read from memory that are in the new 
         shared cache share a single large symbol and string table. These
	 bfd objects require only getting the nonlocal symbols. This range 
	 was set to the range of the EXTDEF symbols from the DYSYMTAB 
	 load command in the bfd_mach_o_scan_read_dysymtab function from
	 the bfd mach-o.c reader.  */
      dbx_symtab_offset = DBX_NONLOCAL_STAB_OFFSET (objfile);
      dbx_symtab_count = DBX_NONLOCAL_STAB_COUNT (objfile);
    }
  else
    {
      /* APPLE LOCAL shared cache end.  */
      if (((OBJF_SYM_LEVELS_MASK & objfile->symflags) != OBJF_SYM_ALL)
         && (objfile->symflags & OBJF_SYM_EXTERN
             || objfile->symflags & OBJF_SYM_CONTAINER)
         && (OBJF_SYM_LEVELS_MASK & (objfile->symflags & ~OBJF_SYM_LOCAL))
         && (OBJF_SYM_LEVELS_MASK & (objfile->symflags & ~OBJF_SYM_DEBUG))
         && DBX_LOCAL_STAB_COUNT (objfile) != 0
         && DBX_NONLOCAL_STAB_COUNT (objfile) != 0
         && !objfile_contains_objc (objfile))
       {
         dbx_symtab_offset = DBX_NONLOCAL_STAB_OFFSET (objfile);
         dbx_symtab_count = DBX_NONLOCAL_STAB_COUNT (objfile);
       }
      else
       {
         dbx_symtab_offset = DBX_SYMTAB_OFFSET (objfile);
         dbx_symtab_count = DBX_SYMCOUNT (objfile);
       }
    }

  val = bfd_seek (sym_bfd, dbx_symtab_offset, SEEK_SET);
  if (val < 0)
    perror_with_name (objfile->name);

  /* If we are reinitializing, or if we have never loaded syms yet, init */
  if (mainline
      || (objfile->global_psymbols.size == 0
	  &&  objfile->static_psymbols.size == 0))
    init_psymbol_list (objfile, dbx_symtab_count);

  symbol_size = DBX_SYMBOL_SIZE (objfile);
  symbol_table_offset = dbx_symtab_offset;

  free_pending_blocks ();
  back_to = make_cleanup (really_free_pendings, 0);

#if 0
  init_minimal_symbol_collection ();
  make_cleanup_discard_minimal_symbols ();
#endif

  /* Read stabs data from executable file and define symbols. */

  read_dbx_symtab (objfile, dbx_symtab_count);

  /* Add the dynamic symbols.  */

  read_dbx_dynamic_symtab (objfile);

  /* Install any minimal symbols that have been collected as the current
     minimal symbols for this objfile. */

#if 0
  install_minimal_symbols (objfile);
#endif

  do_cleanups (back_to);
  if (maint_use_timers)
    do_cleanups (timer_cleanup);
}

/* Initialize anything that needs initializing when a completely new
   symbol file is specified (not just adding some symbols from another
   file, e.g. a shared library).  */

static void
dbx_new_init (struct objfile *ignore)
{
  stabsread_new_init ();
  buildsym_new_init ();
  init_header_files ();
}


/* dbx_symfile_init ()
   is the dbx-specific initialization routine for reading symbols.
   It is passed a struct objfile which contains, among other things,
   the BFD for the file whose symbols are being read, and a slot for a pointer
   to "private data" which we fill with goodies.

   We read the string table into malloc'd space and stash a pointer to it.

   Since BFD doesn't know how to read debug symbols in a format-independent
   way (and may never do so...), we have to do it ourselves.  We will never
   be called unless this is an a.out (or very similar) file. 
   FIXME, there should be a cleaner peephole into the BFD environment here.  */

#define DBX_STRINGTAB_SIZE_SIZE sizeof(long)	/* FIXME */

static void
dbx_symfile_init (struct objfile *objfile)
{
  int val;
  bfd *sym_bfd = objfile->obfd;
  char *name = bfd_get_filename (sym_bfd);
  asection *text_sect;
  unsigned char size_temp[DBX_STRINGTAB_SIZE_SIZE];

  /* Allocate struct to keep track of the symfile */
  objfile->deprecated_sym_stab_info = (struct dbx_symfile_info *)
    xmalloc (sizeof (struct dbx_symfile_info));
  memset (objfile->deprecated_sym_stab_info, 0, sizeof (struct dbx_symfile_info));

  /* APPLE LOCAL: We put struct obj_sections in the DBX_TEXT_SECTION etc
     instead of BFD asections.  Conveniently, we also never execute this
     code path on MacOS X.  */
#if 0
  DBX_TEXT_SECTION (objfile) = bfd_get_section_by_name (sym_bfd, ".text");
  DBX_DATA_SECTION (objfile) = bfd_get_section_by_name (sym_bfd, ".data");
  DBX_BSS_SECTION (objfile) = bfd_get_section_by_name (sym_bfd, ".bss");
#else
  DBX_TEXT_SECTION (objfile) = NULL;
  DBX_DATA_SECTION (objfile) = NULL;
  DBX_BSS_SECTION (objfile) = NULL;
#endif

  /* FIXME POKING INSIDE BFD DATA STRUCTURES */
#define	STRING_TABLE_OFFSET	(sym_bfd->origin + obj_str_filepos (sym_bfd))
#define	SYMBOL_TABLE_OFFSET	(sym_bfd->origin + obj_sym_filepos (sym_bfd))

  /* FIXME POKING INSIDE BFD DATA STRUCTURES */

  DBX_SYMFILE_INFO (objfile)->stab_section_info = NULL;

  text_sect = bfd_get_section_by_name (sym_bfd, ".text");
  if (!text_sect)
    error (_("Can't find .text section in symbol file"));
  DBX_TEXT_ADDR (objfile) = bfd_section_vma (sym_bfd, text_sect);
  DBX_TEXT_SIZE (objfile) = bfd_section_size (sym_bfd, text_sect);

  DBX_SYMBOL_SIZE (objfile) = obj_symbol_entry_size (sym_bfd);
  DBX_SYMCOUNT (objfile) = bfd_get_symcount (sym_bfd);
  DBX_SYMTAB_OFFSET (objfile) = SYMBOL_TABLE_OFFSET;

  /* Read the string table and stash it away in the objfile_obstack.
     When we blow away the objfile the string table goes away as well.
     Note that gdb used to use the results of attempting to malloc the
     string table, based on the size it read, as a form of sanity check
     for botched byte swapping, on the theory that a byte swapped string
     table size would be so totally bogus that the malloc would fail.  Now
     that we put in on the objfile_obstack, we can't do this since gdb gets
     a fatal error (out of virtual memory) if the size is bogus.  We can
     however at least check to see if the size is less than the size of
     the size field itself, or larger than the size of the entire file.
     Note that all valid string tables have a size greater than zero, since
     the bytes used to hold the size are included in the count. */

  if (STRING_TABLE_OFFSET == 0)
    {
      /* It appears that with the existing bfd code, STRING_TABLE_OFFSET
         will never be zero, even when there is no string table.  This
         would appear to be a bug in bfd. */
      DBX_STRINGTAB_SIZE (objfile) = 0;
      DBX_STRINGTAB (objfile) = NULL;
    }
  else
    {
      val = bfd_seek (sym_bfd, STRING_TABLE_OFFSET, SEEK_SET);
      if (val < 0)
	perror_with_name (name);

      memset (size_temp, 0, sizeof (size_temp));
      val = bfd_bread (size_temp, sizeof (size_temp), sym_bfd);
      if (val < 0)
	{
	  perror_with_name (name);
	}
      else if (val == 0)
	{
	  /* With the existing bfd code, STRING_TABLE_OFFSET will be set to
	     EOF if there is no string table, and attempting to read the size
	     from EOF will read zero bytes. */
	  DBX_STRINGTAB_SIZE (objfile) = 0;
	  DBX_STRINGTAB (objfile) = NULL;
	}
      else
	{
	  /* Read some data that would appear to be the string table size.
	     If there really is a string table, then it is probably the right
	     size.  Byteswap if necessary and validate the size.  Note that
	     the minimum is DBX_STRINGTAB_SIZE_SIZE.  If we just read some
	     random data that happened to be at STRING_TABLE_OFFSET, because
	     bfd can't tell us there is no string table, the sanity checks may
	     or may not catch this. */
	  DBX_STRINGTAB_SIZE (objfile) = bfd_h_get_32 (sym_bfd, size_temp);

	  if (DBX_STRINGTAB_SIZE (objfile) < sizeof (size_temp)
	      || DBX_STRINGTAB_SIZE (objfile) > bfd_get_size (sym_bfd))
	    error (_("ridiculous string table size (%d bytes)."),
		   DBX_STRINGTAB_SIZE (objfile));

	  DBX_STRINGTAB (objfile) =
	    (char *) obstack_alloc (&objfile->objfile_obstack,
				    DBX_STRINGTAB_SIZE (objfile));
	  OBJSTAT (objfile, sz_strtab += DBX_STRINGTAB_SIZE (objfile));

	  /* Now read in the string table in one big gulp.  */

	  val = bfd_seek (sym_bfd, STRING_TABLE_OFFSET, SEEK_SET);
	  if (val < 0)
	    perror_with_name (name);
	  val = bfd_bread (DBX_STRINGTAB (objfile),
			   DBX_STRINGTAB_SIZE (objfile),
			   sym_bfd);
	  if (val != DBX_STRINGTAB_SIZE (objfile))
	    perror_with_name (name);
	}
    }
}

/* Perform any local cleanups required when we are done with a particular
   objfile.  I.E, we are in the process of discarding all symbol information
   for an objfile, freeing up all memory held for it, and unlinking the
   objfile struct from the global list of known objfiles. */

static void
dbx_symfile_finish (struct objfile *objfile)
{
  if (objfile->deprecated_sym_stab_info != NULL)
    {
      if (HEADER_FILES (objfile) != NULL)
	{
	  int i = N_HEADER_FILES (objfile);
	  struct header_file *hfiles = HEADER_FILES (objfile);

	  while (--i >= 0)
	    {
	      xfree (hfiles[i].name);
	      xfree (hfiles[i].vector);
	    }
	  xfree (hfiles);
	}
      xfree (objfile->deprecated_sym_stab_info);
    }
  free_header_files ();
}


/* Buffer for reading the symbol table entries.  */
static unsigned char *symbuf = NULL;
static size_t symbuf_size = 0;
static int symbuf_idx;
static int symbuf_end;

/* Name of last function encountered.  Used in Solaris to approximate
   object file boundaries.  */
static char *last_function_name;

/* The address in memory of the string table of the object file we are
   reading (which might not be the "main" object file, but might be a
   shared library or some other dynamically loaded thing).  This is
   set by read_dbx_symtab when building psymtabs, and by
   read_ofile_symtab when building symtabs, and is used only by
   next_symbol_text.  FIXME: If that is true, we don't need it when
   building psymtabs, right?  */
static char *stringtab_global;

/* These variables are used to control fill_symbuf when the stabs
   symbols are not contiguous (as may be the case when a COFF file is
   linked using --split-by-reloc).  */
static struct stab_section_list *symbuf_sections;
static unsigned int symbuf_left;
static unsigned int symbuf_read;

/* This variable stores a global stabs buffer, if we read stabs into
   memory in one chunk in order to process relocations.  */
static bfd_byte *stabs_data;

/* Refill the symbol table input buffer
   and set the variables that control fetching entries from it.
   Reports an error if no data available.
   This function can read past the end of the symbol table
   (into the string table) but this does no harm.  */

static void
/* APPLE LOCAL use objfile */
fill_symbuf (struct objfile *objfile)
{
  bfd *sym_bfd = objfile->obfd;

  unsigned int count;
  int nbytes;

  if (symbuf == NULL)
    {
      symbuf_size = 4096 * DBX_SYMBOL_SIZE (objfile);
      symbuf = xmalloc (symbuf_size);
    }

  if (stabs_data)
    {
      /* APPLE LOCAL: Don't use the symbuf if the data is already
	 read into an internal buffer...  */
      return;
#if 0
      nbytes = symbuf_size;
      if (nbytes > symbuf_left)
        nbytes = symbuf_left;
      memcpy (symbuf, stabs_data + symbuf_read, nbytes);
#endif
    }
  else if (symbuf_sections == NULL)
    {
      struct cleanup *cache_cleanup;
      mem_disable_caching ();
      cache_cleanup = make_cleanup (mem_enable_caching, 0);
      count = symbuf_size;
      nbytes = bfd_bread (symbuf, count, sym_bfd);
      do_cleanups (cache_cleanup);
    }
  else
    {
      if (symbuf_left <= 0)
	{
	  file_ptr filepos = symbuf_sections->section->filepos;
	  if (bfd_seek (sym_bfd, filepos, SEEK_SET) != 0)
	    perror_with_name (bfd_get_filename (sym_bfd));
	  symbuf_left = bfd_section_size (sym_bfd, symbuf_sections->section);
	  symbol_table_offset = filepos - symbuf_read;
	  symbuf_sections = symbuf_sections->next;
	}

      count = symbuf_left;
      if (count > symbuf_size)
	count = symbuf_size;
      nbytes = bfd_bread (symbuf, count, sym_bfd);
    }

  if (nbytes < 0)
    perror_with_name (bfd_get_filename (sym_bfd));
  else if (nbytes == 0)
    error (_("Premature end of file reading symbol table"));
  symbuf_end = nbytes / symbol_size;
  symbuf_idx = 0;
  symbuf_left -= nbytes;
  symbuf_read += nbytes;
}

static void
stabs_seek (int sym_offset)
{
  if (stabs_data)
    {
      /* APPLE LOCAL: If we have ALL the stabs data read into an internal
	 buffer, we don't use the symbuf.  So the symbuf_idx is a direct
	 index into the data.  */
      symbuf_idx = symbuf_idx + sym_offset/symbol_size;
#if 0
      symbuf_read += sym_offset;
      symbuf_left -= sym_offset;
#endif
    }
  else
    bfd_seek (processing_objfile->obfd, sym_offset, SEEK_CUR);
}

/* APPLE LOCAL: We added INTERNALIZE SYMBOL because the nlist data gdb
   uses is different from what is in include/aout/stabs.def.  This
   function allows us to fix up the nlist entries so gdb will be happy
   with them.  One of the elements that gdb doesn't like is the N_SECT
   flag.  So we strip that off.  However, it turns out we also need to
   filter nlist entried based on section.  So we need it back.  That's
   why we have to pass it in as an extra parameter to
   INTERNALIZE_SYMBOL.  */

#ifndef INTERNALIZE_SYMBOL
#define INTERNALIZE_SYMBOL(intern, sect_p, extern, abfd)			\
  {									\
    (intern).n_type = bfd_h_get_8 (abfd, (extern)->e_type);		\
    (intern).n_strx = bfd_h_get_32 (abfd, (extern)->e_strx);		\
    (intern).n_desc = bfd_h_get_16 (abfd, (extern)->e_desc);  		\
    if (bfd_get_sign_extend_vma (abfd))					\
      (intern).n_value = bfd_h_get_signed_32 (abfd, (extern)->e_value);	\
    else								\
      (intern).n_value = bfd_h_get_32 (abfd, (extern)->e_value);	\
  }
#endif

#ifndef PEEK_SYMBOL
#define PEEK_SYMBOL(intern, sect_p, abfd)				\
  {									\
    if (symbuf_idx == symbuf_end)                                       \
      fill_symbuf (processing_objfile);                                 \
    INTERNALIZE_SYMBOL (intern, sect_p, (struct external_nlist *)       \
                        ((stabs_data ? stabs_data: symbuf)              \
                         + (symbol_size * symbuf_idx)), abfd);          \
  }
#endif

#ifndef NEXT_SYMBOL
#define NEXT_SYMBOL(intern,  sect_p, abfd)				\
  {                                                                     \
    PEEK_SYMBOL (intern, sect_p, abfd);					\
    symbuf_idx++;                                                       \
  }
#endif

/* Invariant: The symbol pointed to by symbuf_idx is the first one
   that hasn't been swapped.  Swap the symbol at the same time
   that symbuf_idx is incremented.  */

/* dbx allows the text of a symbol name to be continued into the
   next symbol name!  When such a continuation is encountered
   (a \ at the end of the text of a name)
   call this function to get the continuation.  */

static char *
dbx_next_symbol_text (struct objfile *objfile)
{
  struct internal_nlist nlist1;
  struct internal_nlist nlist2;
  int sect_p; 

  PEEK_SYMBOL (nlist1, sect_p, processing_objfile->obfd);

  if (nlist1.n_type & N_STAB)
    {
      /* The next symbol is a stab; return it as we normally would. */
      symbuf_idx++;
      symnum++;
      OBJSTAT (objfile, n_stabs++);
      return (nlist1.n_strx + stringtab_global + file_string_table_offset);
    }

  /* A non-stab got somehow inserted into the name.  Look ahead to
     find the continuation of the name. */

  symbuf_idx++;
  PEEK_SYMBOL (nlist2, sect_p, processing_objfile->obfd);
  symbuf_idx--;

  if (nlist2.n_type & N_STAB)
    {
      /* The symbol just after the next symbol is a stab.  Move the
	 non-stab into its place; then return the stab, effectively
	 swapping the two entries. */

      memcpy (symbuf + ((symbuf_idx + 0) * symbol_size),
	      symbuf + ((symbuf_idx + 1) * symbol_size), symbol_size);
      
      /* Now increment the various pointers as we normally would. */
      symbuf_idx++;
      symnum++;
      OBJSTAT (objfile, n_stabs++);
      return (nlist2.n_strx + stringtab_global + file_string_table_offset);
    }

  /* We only look ahead one non-stab symbol. */

  complaint (&symfile_complaints, "unable to find symbol continuation");
  return (nlist1.n_strx + stringtab_global + file_string_table_offset);
}


/* Compute a hash value for a BINCL/EINCL filename string. */

#define bincl_hash msymbol_hash

/* Initialize the list of bincls to contain none and have some
   allocated.  */

static void
init_bincl_list (int number, struct objfile *objfile)
{
  bincls_allocated = number;
  next_bincl = bincl_list = (struct header_file_location *)
    xmalloc (bincls_allocated * sizeof (struct header_file_location));
}

/* Add a bincl to the list.  */

static void
add_bincl_to_list (struct partial_symtab *pst, char *name, int instance)
{
  if (next_bincl >= bincl_list + bincls_allocated)
    {
      int offset = next_bincl - bincl_list;
      bincls_allocated *= 2;
      bincl_list = (struct header_file_location *)
	xrealloc ((char *) bincl_list,
		  bincls_allocated * sizeof (struct header_file_location));
      next_bincl = bincl_list + offset;
    }
  next_bincl->pst = pst;
  next_bincl->hash = bincl_hash (name);
  next_bincl->instance = instance;
  next_bincl++->name = name;
}

/* Given a name, value pair, find the corresponding
   bincl in the list.  Return the partial symtab associated
   with that header_file_location.  */

static struct partial_symtab *
find_corresponding_bincl_psymtab (char *name, int instance)
{
  struct header_file_location *bincl;
  unsigned long hash = bincl_hash (name);

  for (bincl = bincl_list; bincl < next_bincl; bincl++)
    if ((bincl->hash == hash)
	&& (bincl->instance == instance)
	&& strcmp (name, bincl->name) == 0)
      return bincl->pst;

  repeated_header_complaint (name, symnum);
  return (struct partial_symtab *) 0;
}

/* Free the storage allocated for the bincl list.  */

static void
free_bincl_list (struct objfile *objfile)
{
  xfree (bincl_list);
  bincls_allocated = 0;
}

static void
do_free_bincl_list_cleanup (void *objfile)
{
  free_bincl_list (objfile);
}

static struct cleanup *
make_cleanup_free_bincl_list (struct objfile *objfile)
{
  return make_cleanup (do_free_bincl_list_cleanup, objfile);
}

/* Set namestring based on nlist.  If the string table index is invalid, 
   give a fake name, and print a single error message per symbol file read,
   rather than abort the symbol reading or flood the user with messages.  */

static char *
/* APPLE LOCAL symbol prefixes */
set_namestring_1 (int strtab_size, char *strtab, char leading_char,
                  struct internal_nlist nlist, const char *prefix)
{
  char *namestring;
  static char *namebuf = NULL;
  static size_t namebuf_len = 0;

  if (((unsigned int) nlist.n_strx + file_string_table_offset) >=
      strtab_size)
    {
      complaint (&symfile_complaints, _("bad string table offset in symbol %d"),
		 symnum);
      namestring = "<bad string table offset>";
    } 
  else
    {
      const char *p = prefix;
      char *s = nlist.n_strx + file_string_table_offset + strtab;
      if ((p != NULL) && (p[0] != '\0'))
	{
	  size_t req = 1 + strlen (p) + strlen (s) + 1 - 1;
	  while (namebuf_len < req)
	    {
	      if (namebuf_len == 0)
		namebuf_len = 1024;
	      namebuf_len *= 2;
	    }
	  namebuf = xrealloc (namebuf, namebuf_len);
	  sprintf (namebuf, "%c%s%s", leading_char, p, s + 1); 	                
	  namestring = namebuf;		
	} else {					
	  namestring = s;
	}						
    }
  
  return namestring;
}

static char *
set_namestring (struct objfile *objfile, struct internal_nlist nlist, const char *prefix)
{
  return set_namestring_1 (DBX_STRINGTAB_SIZE (objfile),
			   DBX_STRINGTAB (objfile),
			   bfd_get_symbol_leading_char (objfile->obfd),
			   nlist,
			   prefix);
}

/* Scan a SunOs dynamic symbol table for symbols of interest and
   add them to the minimal symbol table.  */

static void
read_dbx_dynamic_symtab (struct objfile *objfile)
{
  bfd *abfd = objfile->obfd;
  struct cleanup *back_to;
  int counter;
  long dynsym_size;
  long dynsym_count;
  asymbol **dynsyms;
  asymbol **symptr;
  arelent **relptr;
  long dynrel_size;
  long dynrel_count;
  arelent **dynrels;
  CORE_ADDR sym_value;
  char *name;

  /* Check that the symbol file has dynamic symbols that we know about.
     bfd_arch_unknown can happen if we are reading a sun3 symbol file
     on a sun4 host (and vice versa) and bfd is not configured
     --with-target=all.  This would trigger an assertion in bfd/sunos.c,
     so we ignore the dynamic symbols in this case.  */
  if (bfd_get_flavour (abfd) != bfd_target_aout_flavour
      || (bfd_get_file_flags (abfd) & DYNAMIC) == 0
      || bfd_get_arch (abfd) == bfd_arch_unknown)
    return;

  dynsym_size = bfd_get_dynamic_symtab_upper_bound (abfd);
  if (dynsym_size < 0)
    return;

  dynsyms = (asymbol **) xmalloc (dynsym_size);
  back_to = make_cleanup (xfree, dynsyms);

  dynsym_count = bfd_canonicalize_dynamic_symtab (abfd, dynsyms);
  if (dynsym_count < 0)
    {
      do_cleanups (back_to);
      return;
    }

  /* Enter dynamic symbols into the minimal symbol table
     if this is a stripped executable.  */
  if (bfd_get_symcount (abfd) <= 0)
    {
      symptr = dynsyms;
      for (counter = 0; counter < dynsym_count; counter++, symptr++)
	{
	  asymbol *sym = *symptr;
	  asection *sec;
	  int type;

	  sec = bfd_get_section (sym);

	  /* BFD symbols are section relative.  */
	  sym_value = sym->value + sec->vma;

	  if (bfd_get_section_flags (abfd, sec) & SEC_CODE)
	    {
	      sym_value += objfile_text_section_offset (objfile);
	      type = N_TEXT;
	    }
	  else if (bfd_get_section_flags (abfd, sec) & SEC_DATA)
	    {
	      sym_value += objfile_data_section_offset (objfile);
	      type = N_DATA;
	    }
	  else if (bfd_get_section_flags (abfd, sec) & SEC_ALLOC)
	    {
	      sym_value += objfile_bss_section_offset (objfile);
	      type = N_BSS;
	    }
	  else
	    continue;

	  if (sym->flags & BSF_GLOBAL)
	    type |= N_EXT;

	  /* APPLE LOCAL: We don't know the desc here, so just pass 0.  */
	  record_minimal_symbol ((char *) bfd_asymbol_name (sym), sym_value,
				 type, 0, objfile);
	}
    }

  /* Symbols from shared libraries have a dynamic relocation entry
     that points to the associated slot in the procedure linkage table.
     We make a mininal symbol table entry with type mst_solib_trampoline
     at the address in the procedure linkage table.  */
  dynrel_size = bfd_get_dynamic_reloc_upper_bound (abfd);
  if (dynrel_size < 0)
    {
      do_cleanups (back_to);
      return;
    }

  dynrels = (arelent **) xmalloc (dynrel_size);
  make_cleanup (xfree, dynrels);

  dynrel_count = bfd_canonicalize_dynamic_reloc (abfd, dynrels, dynsyms);
  if (dynrel_count < 0)
    {
      do_cleanups (back_to);
      return;
    }

  for (counter = 0, relptr = dynrels;
       counter < dynrel_count;
       counter++, relptr++)
    {
      arelent *rel = *relptr;
      CORE_ADDR address =
      rel->address + objfile_data_section_offset (objfile);

      switch (bfd_get_arch (abfd))
	{
	case bfd_arch_sparc:
	  if (rel->howto->type != RELOC_JMP_SLOT)
	    continue;
	  break;
	case bfd_arch_m68k:
	  /* `16' is the type BFD produces for a jump table relocation.  */
	  if (rel->howto->type != 16)
	    continue;

	  /* Adjust address in the jump table to point to
	     the start of the bsr instruction.  */
	  address -= 2;
	  break;
	default:
	  continue;
	}

      name = (char *) bfd_asymbol_name (*rel->sym_ptr_ptr);
      prim_record_minimal_symbol (name, address, mst_solib_trampoline,
				  objfile);
    }

  do_cleanups (back_to);
}

#ifdef SOFUN_ADDRESS_MAYBE_MISSING
static CORE_ADDR
find_stab_function_addr (char *namestring, char *filename,
			 struct objfile *objfile)
{
  struct minimal_symbol *msym;
  char *p;
  int n;

  p = strchr (namestring, ':');
  if (p == NULL)
    p = namestring;
  n = p - namestring;
  p = alloca (n + 2);
  strncpy (p, namestring, n);
  p[n] = 0;

  msym = lookup_minimal_symbol (p, filename, objfile);
  if (msym == NULL)
    {
      /* Sun Fortran appends an underscore to the minimal symbol name,
         try again with an appended underscore if the minimal symbol
         was not found.  */
      p[n] = '_';
      p[n + 1] = 0;
      msym = lookup_minimal_symbol (p, filename, objfile);
    }

  if (msym == NULL && filename != NULL)
    {
      /* Try again without the filename. */
      p[n] = 0;
      msym = lookup_minimal_symbol (p, NULL, objfile);
    }
  if (msym == NULL && filename != NULL)
    {
      /* And try again for Sun Fortran, but without the filename. */
      p[n] = '_';
      p[n + 1] = 0;
      msym = lookup_minimal_symbol (p, NULL, objfile);
    }

  return msym == NULL ? 0 : SYMBOL_VALUE_ADDRESS (msym);
}
#endif /* SOFUN_ADDRESS_MAYBE_MISSING */

static void
function_outside_compilation_unit_complaint (const char *arg1)
{
  complaint (&symfile_complaints,
	     _("function `%s' appears to be defined outside of all compilation units"),
	     arg1);
}

/* APPLE LOCAL: This function initializes the OSO list.  */

struct oso_pst_list *
init_oso_pst_list (char *oso_name, struct objfile *objfile)
{
  struct oso_pst_list *oso_list;

  oso_list = (struct oso_pst_list *) obstack_alloc (&objfile->objfile_obstack,
                                                 sizeof (struct oso_pst_list));
  oso_list->oso_name = oso_name;
  oso_list->readin = 0;
  oso_list->pst_list = NULL;

  return oso_list;
}

/* APPLE LOCAL: This function adds PST to the oso_pst_list LIST.  The
   list will eventually be moved to OBJ's obstack.  We have to use a
   staging area since we need to allocate the real thing on the
   objfile's obstack, but we have to do that all at a blow and there's
   no way a priori to know how many elements there are.

   OBJFILE is not used in this, but I kept it around because the data
   eventually ends up on the objfile's obstack, so we might need it
   later on.  */

static int oso_pst_num_allocated = 0;
static int oso_pst_num_used = 0;

void
add_oso_pst_to_list (struct oso_pst_list *list, 
		     struct partial_symtab *pst,
		     struct objfile *objfile)
{
  if (list->pst_list == NULL)
    {
      oso_pst_num_allocated = 30;
      oso_pst_num_used = 0;
      list->pst_list = (struct partial_symtab **) 
	xmalloc (oso_pst_num_allocated * sizeof (struct partial_symtab *));
    }
  else if (oso_pst_num_allocated == oso_pst_num_used)
    {
      oso_pst_num_allocated *= 2;
      list->pst_list = (struct partial_symtab **) xrealloc (list->pst_list, 
                     oso_pst_num_allocated * sizeof (struct partial_symtab *));
    }

  list->pst_list[oso_pst_num_used++] = pst;
  if (pst != NULL)
    PSYMTAB_OSO_PST_LIST (pst) = list;

}

/* This adds a NULL element at the end of the pst_list of LIST, 
   and the transfers the pst_list from the staging area to
   OBJFILE's obstack and frees the staging area.  */

void
end_oso_pst_list (struct oso_pst_list *list, struct objfile *objfile)
{
  struct partial_symtab **old_list;
  add_oso_pst_to_list (list, NULL, objfile);
  old_list = list->pst_list;
  list->pst_list = obstack_alloc (&objfile->objfile_obstack,
                         oso_pst_num_used * sizeof (struct partial_symtab *));
  memcpy (list->pst_list, old_list, 
                         oso_pst_num_used * sizeof (struct partial_symtab *));
  oso_pst_num_used = 0;
  oso_pst_num_allocated = 0;
  xfree (old_list);
}

/* END APPLE LOCAL */

/* Setup partial_symtab's describing each source file for which
   debugging information is available. */
/* APPLE LOCAL: OSO names can be of the form: foo.a(bar.o).  This routine
   will check OSO_NAME, and if it is not an archive will return 0.  If it
   is an archive it will return 1.  Also if ARCHIVE_NAME is non null, it
   will malloc a copy of the path to the archive file. And if MODULE_NAME
   is not NULL, it will allocate a copy of the member name.
  
   FIXME: At present, ld doesn't backslash protect parenthesis in
   filenames, so there's really no way we can deal with the case where
   a file name or archive name has a '(' in it.  */

int
parse_archive_name (char *oso_name, char **archive_name, char **module_name)
{
  char *lparen;
  int ar_name_len;
  int oso_name_len = strlen (oso_name);
  if (oso_name[oso_name_len - 1] != ')')
    return 0;

  if (archive_name == NULL && module_name == NULL)
    return 1;

  lparen = strrchr (oso_name, '(');
  if (lparen == NULL)
    return 0;

  ar_name_len = lparen - oso_name;
  if (archive_name != NULL)
    {
      char *copy;
      copy = (char *) xmalloc ((ar_name_len  + 1) 
			       * sizeof (char));
      memcpy (copy, oso_name, ar_name_len);
      copy[ar_name_len] = '\0';
      *archive_name = copy;
    }

  if (module_name != NULL)
    {
      char *copy;
      int module_name_len;
      module_name_len = oso_name_len - ar_name_len - 2;
      copy = (char *) xmalloc ((module_name_len + 1)
			       * sizeof (char));
      memcpy (copy, oso_name + ar_name_len + 1, 
	      module_name_len);
      copy[module_name_len] = '\0';
      *module_name = copy;
    }
  return 1;
}

/* APPLE LOCAL: pass in the # of stab nlist records we're going to parse. */
static void
read_dbx_symtab (struct objfile *objfile, int dbx_symcount)
{
  struct internal_nlist nlist;
  CORE_ADDR text_addr;
  int text_size;

  char *namestring;
  int nsl;
  int past_first_source_file = 0;
  CORE_ADDR last_o_file_start = 0;
  CORE_ADDR last_function_start = 0;
  struct cleanup *back_to;
  bfd *abfd;
  int textlow_not_set;
  const char *prefix = NULL;
  int data_sect_index;

  /* Current partial symtab */
  struct partial_symtab *pst;

  /* List of current psymtab's include files */
  char **psymtab_include_list;
  int includes_allocated;
  int includes_used;

  /* Index within current psymtab dependency list */
  struct partial_symtab **dependency_list;
  int dependencies_used, dependencies_allocated;

  /* APPLE LOCAL: psymtab for the "Symbol separation" file.  A file named
     YYY.h.gch, then we assume that is the symbol separated .o.  We will 
     stuff that in the dependency list of ALL the N_SO level psymtabs that
     we build.  */
  struct partial_symtab_list {
    struct partial_symtab_list *next;
    struct partial_symtab *pst;
  };

  struct partial_symtab_list *symbol_sep_pst = NULL;
  
  /* Pointer to the oso_fun_list entry in the list we are building
     up for this pst. */
  struct oso_fun_list *cur_fun_statics = NULL;
  char *cur_fun_name = NULL;
  
  /* If more than one PST gets it's symbols from the same N_OSO .o
     file, we need to scan it for header dependencies before doing
     psymtab_to_symtab.  LAST_OSO_PST is used to make sure we only
     build up the list of pst's sharing the .o if there are more 
     than one.  CUR_OSO_LIST is the list we are building up now.
     
     We assume that all the N_SO sections that share an N_OSO occur
     sequentially in the final linked image.  */

  struct partial_symtab *last_oso_pst = NULL;
  struct oso_pst_list *cur_oso_list = NULL;

  /* When the SO we're currently processing is a DWARF debug map, 
     this is set.  */
  int in_dwarf_debug_map = 0;

  /* If the .o file for a "debug in .o file" case is missing, this is set.  */
  int missing_oso_file = 0;

  /* When we have a dSYM file associated with this objfile, this is set.  */
  int have_dsym_file = objfile->separate_debug_objfile != NULL;

  /* If the objfile is a kext we need both the dSYM (with the ld -r'ed debug
     info of the kext) plus the debug map output from kextload when the kext
     was loaded into the kernel.  Pretend we don't have a dSYM at this point
     so we'll parse the debug map as we normally would.  */
  if (objfile->not_loaded_kext_filename)
    have_dsym_file = 0;

  /* END APPLE LOCAL */

  text_addr = DBX_TEXT_ADDR (objfile);
  text_size = DBX_TEXT_SIZE (objfile);

  prefix = objfile->prefix;

  /* FIXME.  We probably want to change stringtab_global rather than add this
     while processing every symbol entry.  FIXME.  */
  file_string_table_offset = 0;
  next_file_string_table_offset = 0;

  stringtab_global = DBX_STRINGTAB (objfile);

  pst = (struct partial_symtab *) 0;

  includes_allocated = 30;
  includes_used = 0;
  psymtab_include_list = (char **) alloca (includes_allocated *
					   sizeof (char *));

  dependencies_allocated = 30;
  dependencies_used = 0;
  dependency_list =
    (struct partial_symtab **) alloca (dependencies_allocated *
				       sizeof (struct partial_symtab *));

  /* Init bincl list */
  init_bincl_list (20, objfile);
  back_to = make_cleanup_free_bincl_list (objfile);

  last_source_file = NULL;

  lowest_text_address = (CORE_ADDR) -1;

  processing_objfile = objfile;	/* For next_text_symbol */
  abfd = objfile->obfd;
  symbuf_end = symbuf_idx = 0;
  next_symbol_text_func = dbx_next_symbol_text;
  textlow_not_set = 1;
  has_line_numbers = 0;

  /* FIXME: jimb/2003-09-12: We don't apply the right section's offset
     to global and static variables.  The stab for a global or static
     variable doesn't give us any indication of which section it's in,
     so we can't tell immediately which offset in
     objfile->section_offsets we should apply to the variable's
     address.

     We could certainly find out which section contains the variable
     by looking up the variable's unrelocated address with
     find_pc_section, but that would be expensive; this is the
     function that constructs the partial symbol tables by examining
     every symbol in the entire executable, and it's
     performance-critical.  So that expense would not be welcome.  I'm
     not sure what to do about this at the moment.

     What we have done for years is to simply assume that the .data
     section's offset is appropriate for all global and static
     variables.  Recently, this was expanded to fall back to the .bss
     section's offset if there is no .data section, and then to the
     .rodata section's offset.  */
  data_sect_index = objfile->sect_index_data;
  if (data_sect_index == -1)
    data_sect_index = SECT_OFF_BSS (objfile);
  if (data_sect_index == -1)
    data_sect_index = SECT_OFF_RODATA (objfile);

  /* If data_sect_index is still -1, that's okay.  It's perfectly fine
     for the file to have no .data, no .bss, and no .text at all, if
     it also has no global or static variables.  If it does, we will
     get an internal error from an ANOFFSET macro below when we try to
     use data_sect_index.  */

  for (symnum = 0; symnum <  dbx_symcount; symnum++)
    {
      /* APPLE LOCAL: See comment below where this is used.  */
      char leading_char = bfd_get_symbol_leading_char (objfile->obfd);
      /* APPLE LOCAL: We go to great lengths to remove the N_SECT flag
	 from the n_type, but we need to know it here...  */
      int sect_p; 
      /* Get the symbol for this run and pull out some info */
      QUIT;			/* allow this to be interruptable */
      namestring = NULL;

      NEXT_SYMBOL (nlist, sect_p, abfd);

      /* APPLE LOCAL: Skip all debug map nlist entries when we have a 
	 dSYM file.  */
      if (have_dsym_file && in_dwarf_debug_map && nlist.n_type != N_SO)
         continue;

      /* If we are missing this .o file, we have to process all the regular
	 linker symbols - to build up the minsyms, and scan for the closing
	 N_SO, but we don't want to ingest any of the stabs.  */

      if (missing_oso_file && (nlist.n_type & N_STAB) && nlist.n_type != N_SO)
	continue;

      if (nlist.n_type == N_SLINE)
	{
	  has_line_numbers = 1;
	  continue;
	}

      OBJSTAT (objfile, n_stabs++);

      /* APPLE LOCAL: If we aren't reading in stabs right now, and
	 this is a stab, just skip it.  */

      if ((nlist.n_type & N_STAB) && ! (objfile->symflags & OBJF_SYM_DEBUG))
	    continue;

      /* Ok.  There is a lot of code duplicated in the rest of this
         switch statement (for efficiency reasons).  Since I don't
         like duplicating code, I will do my penance here, and
         describe the code which is duplicated:

         *) The assignment to namestring.
         *) The call to strchr.
         *) The addition of a partial symbol the the two partial
         symbol lists.  This last is a large section of code, so
         I've imbedded it in the following macro.
      */

      switch (nlist.n_type)
	{
	  /*
	   * Standard, external, non-debugger, symbols
	   */

	  case N_TEXT | N_EXT:
	  case N_NBTEXT | N_EXT:
	  nlist.n_value += objfile_text_section_offset (objfile);
	  goto record_it;

	  case N_DATA | N_EXT:
	  case N_NBDATA | N_EXT:
	  nlist.n_value += objfile_data_section_offset (objfile);
	  goto record_it;

	  case N_BSS:
	  case N_BSS | N_EXT:
	  case N_NBBSS | N_EXT:
	  case N_SETV | N_EXT:		/* FIXME, is this in BSS? */
	  nlist.n_value += objfile_bss_section_offset (objfile);
	  goto record_it;

	  case N_ABS | N_EXT:
          record_it:
            /* APPLE LOCAL: The new ld outputs symbols for all the
               stubs in the SYMBOL_STUBS sections for its convenience.
               These are not the symbols we use to do the mapping
               from PC to stub target.  So we need to suppress
               reading these in.  */
#ifdef TM_NEXTSTEP
            if (sect_p && !macosx_record_symbols_from_sect_p (objfile->obfd,
                                                              nlist.n_type,
                                                              nlist.n_other))
              continue;
#endif
            /* APPLE LOCAL symbol prefixes */
          namestring = set_namestring (objfile, nlist, prefix);

	  bss_ext_symbol:
	  /* APPLE LOCAL: The nlist string table entries include the leading
	     char, but for install_minimal_symbol to set the demangled name
	     right, we need to strip this.  Otherwise it chokes the demangler.
	     We were just stripping them all en masse later on 
	     (in install_minimal_symbols) so we might as well do it here. */
	  if (leading_char == namestring[0])
	    namestring++;
	  /* APPLE LOCAL: pass the n_desc in case we need it.  */
	  record_minimal_symbol (namestring, nlist.n_value,
				   nlist.n_type, nlist.n_desc, objfile);	/* Always */
	  continue;
	}

      switch (nlist.n_type)
	{
	  /* Standard, local, non-debugger, symbols */

	  case N_NBTEXT:

	  /* We need to be able to deal with both N_FN or N_TEXT,
	     because we have no way of knowing whether the sys-supplied ld
	     or GNU ld was used to make the executable.  Sequents throw
	     in another wrinkle -- they renumbered N_FN.  */

	  case N_FN:
	  case N_FN_SEQ:
	  case N_TEXT:
	  nlist.n_value += objfile_text_section_offset (objfile);
	  /* APPLE LOCAL symbol prefixes */
	  namestring = set_namestring (objfile, nlist, prefix);

	  /* APPLE LOCAL begin load level */
	  /* Here is where we implement the load level policy in
	     symbol reading.  Note we also read in all things that
	     look like ObjC methods, since they are ALWAYS local,
	     though we want to treat then as external.  */
	  if ((! (objfile->symflags & (OBJF_SYM_LOCAL | OBJF_SYM_DEBUG)))
	      && ((namestring[0] != '+') && (namestring[0] != '-')))
	    continue;
	  /* APPLE LOCAL end load level */
	  if ((namestring[0] == '-' && namestring[1] == 'l')
	      || (namestring[(nsl = strlen (namestring)) - 1] == 'o'
		  && namestring[nsl - 2] == '.'))
	  {
	    if (past_first_source_file && pst
		/* The gould NP1 uses low values for .o and -l symbols
		   which are not the address.  */
		&& nlist.n_value >= pst->textlow)
	      {
		end_psymtab (pst, psymtab_include_list, includes_used,
			     symnum * symbol_size,
			     nlist.n_value > pst->texthigh
			     ? nlist.n_value : pst->texthigh,
			     dependency_list, dependencies_used, textlow_not_set);
		pst = (struct partial_symtab *) 0;
		includes_used = 0;
		dependencies_used = 0;
                in_dwarf_debug_map = 0;
		missing_oso_file = 0;
	      }
	    else
	      past_first_source_file = 1;
	    last_o_file_start = nlist.n_value;
	  }
	  else
	  goto record_it;
	  continue;
	}

      if (in_dwarf_debug_map && pst)
        {
          int name_has_dot_in_it;
          namestring = set_namestring (objfile, nlist, prefix);

          /* Function ending N_FUN */
          if (nlist.n_type == N_FUN && namestring[0] == '\0')
            {
              CORE_ADDR valu = nlist.n_value + last_function_start;
              if (valu > pst->texthigh)
                pst->texthigh = valu;
              continue;
            }

          /* Skip the "_" at the start of symbol names */
          if (namestring[0] == leading_char)
            namestring++;

          /* Function beginning N_FUN */
          if (nlist.n_type == N_FUN)
            {
              nlist.n_value += objfile_text_section_offset (objfile);
              if (textlow_not_set)
                {
                  pst->textlow = nlist.n_value;
                  textlow_not_set = 0;
                }
              if (pst->textlow > nlist.n_value
                  && nlist.n_value != objfile_text_section_offset (objfile))
                {
                  pst->textlow = nlist.n_value;
                }

              last_function_start = nlist.n_value;

              add_psymbol_to_list (namestring, strlen (namestring),
                                   VAR_DOMAIN, LOC_BLOCK,
                                   &objfile->global_psymbols,
                                   0, nlist.n_value,
                                   psymtab_language, objfile);
              continue;
            }

          /* gcc only emits the debug info for a class in one compilation
             unit (the "single definition rule" in C++ or somesuch) when
	     there is a virtual ctor/dtor.  We want to note which
	     CU has that definition so when users examine an object
	     of that class in another CU we know to read in the
	     debug info from this CU to pick up the definition.

             We use the presence of a virtual table symbol to detect this
             situation.  The C++ ABI ("5.1.4 Other Special Functions and
             Entities") tells us that we're looking for 
                 _ZTV<len-of-class-name><class-name>
          */
          if (nlist.n_type == N_GSYM 
              && namestring[0] == '_'
              && namestring[1] == 'Z'
              && namestring[2] == 'T'
              && namestring[3] == 'V')
            {
	      /* The class name could include a namespace or many, so 
		 we use the demangler to unpack the name for us here.  */
	      char *demangled = cplus_demangle_v3 (namestring, DMGL_ANSI);
	      if (demangled != NULL)
		{
		  if (strstr (demangled, "vtable for ") != NULL)
		    {
		      add_psymbol_to_list (demangled + 11, 
                                           strlen (demangled) - 11,
					   STRUCT_DOMAIN, LOC_TYPEDEF,
					   &objfile->static_psymbols,
					   0, 0,
					   psymtab_language, objfile);
		    }
		  xfree (demangled);
                }
            }

          /* Function-scoped static variables have a period ('.') in their
             names.  e.g. "static int i;" will actually be "i.1662" in
             linker symbol terms.  
             Normally these would not go in a psymtab -- we're adding them
             there so the debug map psymtab_to_symtab code can disambiguate
             between multiple "i.1662"'s contributed by multiple compilation
             units when making the debug map.  */

          name_has_dot_in_it = 0;
          if (nlist.n_type == N_GSYM || nlist.n_type == N_STSYM)
            if (strchr (namestring, '.') != NULL)
              name_has_dot_in_it = 1;

          if (nlist.n_type == N_STSYM || name_has_dot_in_it)
            {
              nlist.n_value += objfile_section_offset (objfile, data_sect_index);
              add_psymbol_to_list (namestring, strlen (namestring),
                                   VAR_DOMAIN, LOC_STATIC,
                                   &objfile->static_psymbols,
                                   0, nlist.n_value,
                                   psymtab_language, objfile);
              continue;
            }

          if (nlist.n_type == N_GSYM && !name_has_dot_in_it)
            {
              nlist.n_value += objfile_section_offset (objfile, data_sect_index);
              add_psymbol_to_list (namestring, strlen (namestring),
                                   VAR_DOMAIN, LOC_STATIC,
                                   &objfile->global_psymbols,
                                   0, nlist.n_value,
                                   psymtab_language, objfile);
              continue;
            }
        }

      switch (nlist.n_type)
	{
	  char *p;

	  case N_DATA:
	  nlist.n_value += objfile_data_section_offset (objfile);
	  goto record_it;

	  case N_UNDF | N_EXT:
	  if (nlist.n_value != 0)
	  {
	    /* This is a "Fortran COMMON" symbol.  See if the target
	       environment knows where it has been relocated to.  */

	    CORE_ADDR reladdr;

	    /* APPLE LOCAL symbol prefixes */
	    namestring = set_namestring (objfile, nlist, prefix);
	    if (target_lookup_symbol (namestring, &reladdr))
	      {
		continue;		/* Error in lookup; ignore symbol for now.  */
	      }
	    nlist.n_type ^= (N_BSS ^ N_UNDF);	/* Define it as a bss-symbol */
	    nlist.n_value = reladdr;
	    goto bss_ext_symbol;
	  }
	  continue;			/* Just undefined, not COMMON */

	  case N_UNDF:
	  if (processing_acc_compilation && nlist.n_strx == 1)
	  {
	    /* Deal with relative offsets in the string table
	       used in ELF+STAB under Solaris.  If we want to use the
	       n_strx field, which contains the name of the file,
	       we must adjust file_string_table_offset *before* calling
	       set_namestring().  */
	    past_first_source_file = 1;
	    file_string_table_offset = next_file_string_table_offset;
	    next_file_string_table_offset =
	      file_string_table_offset + nlist.n_value;
	    if (next_file_string_table_offset < file_string_table_offset)
	      error (_("string table offset backs up at %d"), symnum);
	    /* FIXME -- replace error() with complaint.  */
	    continue;
	  }
	  continue;

	  /* Lots of symbol types we can just ignore.  */

	  case N_ABS:
	  case N_NBDATA:
	  case N_NBBSS:
	  continue;

	  /* Keep going . . . */

	  /*
	   * Special symbol types for GNU
	   */
	  case N_INDR:
	  case N_INDR | N_EXT:
	  case N_SETA:
	  case N_SETA | N_EXT:
	  case N_SETT:
	  case N_SETT | N_EXT:
	  case N_SETD:
	  case N_SETD | N_EXT:
	  case N_SETB:
	  case N_SETB | N_EXT:
	  case N_SETV:
	  continue;

	  /*
	   * Debugger symbols
	   */

	  case N_SO:
	  {
	    CORE_ADDR valu;
	    static int prev_so_symnum = -10;
	    static int first_so_symnum;
	    char *p;
	    static char *dirname_nso = NULL;
	    int prev_textlow_not_set;

	    valu = nlist.n_value + objfile_text_section_offset (objfile);

	    prev_textlow_not_set = textlow_not_set;

#ifdef SOFUN_ADDRESS_MAYBE_MISSING
	    /* A zero value is probably an indication for the SunPRO 3.0
	       compiler. end_psymtab explicitly tests for zero, so
	       don't relocate it.  */

	    if (nlist.n_value == 0)
	      {
		textlow_not_set = 1;
		valu = 0;
	      }
	    else
	      textlow_not_set = 0;
#else
	    textlow_not_set = 0;
#endif
	    past_first_source_file = 1;

	    if (prev_so_symnum != symnum - 1)
	      {			/* Here if prev stab wasn't N_SO */
		first_so_symnum = symnum;

		if (pst)
		  {
		    end_psymtab (pst, psymtab_include_list, includes_used,
				 symnum * symbol_size,
				 valu > pst->texthigh ? valu : pst->texthigh,
				 dependency_list, dependencies_used,
				 prev_textlow_not_set);
		    pst = (struct partial_symtab *) 0;
		  }
		/* APPLE LOCAL: Move the resetting of the following three
		   variables out of the above if statement since we may
		   have a NULL PST when we are ignoring debug map nlist
		   entries due to having a dSYM file.  */
		includes_used = 0;
		dependencies_used = 0;
		in_dwarf_debug_map = 0;
		missing_oso_file = 0;
	      }

	    prev_so_symnum = symnum;

	    /* End the current partial symtab and start a new one */

	    /* APPLE LOCAL symbol prefixes */
	    namestring = set_namestring (objfile, nlist, NULL);

	    /* Null name means end of .o file.  Don't start a new one. */
	    if (*namestring == '\000')
              {
                /* APPLE LOCAL: Reset the current language */
                psymtab_language = language_unknown;
	        continue;
              }

	    /* Some compilers (including gcc) emit a pair of initial N_SOs.
	       The first one is a directory name; the second the file name.
	       If pst exists, is empty, and has a filename ending in '/',
	       we assume the previous N_SO was a directory name. */

	    p = strrchr (namestring, '/');
	    if (p && *(p + 1) == '\000')
	      {
		/* Save the directory name SOs locally, then save it into
		   the psymtab when it's created below. */
	        dirname_nso = xstrdup (namestring);
	        continue;		
	      }
            /* APPLE LOCAL: Try getting the file's language from 'desc' field */
            psymtab_language = read_so_stab_language_hint (nlist.n_desc);

            /* APPLE LOCAL begin: dSYM with debug map.  */
	    /* If we have a dSYM file, check the next nlist entry to see if
	       it is a N_OSO since we will need to ignore all debug map
	       information.  */
	    /* Also, if the next stab is an N_OSO, but the OSO file is missing,
	       then we will have to ignore all the debug map entries for this
	       .o section.  */

	      {
		struct internal_nlist next_nlist;
		PEEK_SYMBOL(next_nlist, sect_p, abfd);
		if (next_nlist.n_type == N_OSO && next_nlist.n_desc == 1)
		  {
		    char *oso_namestring;
		    struct stat buf;
		    int stat_ret;
		    char *dot_a_name;

		    /* Don't create a psymtab for files that are debug map
		       entries when we have a dSYM file.  */
		    if (have_dsym_file)
		      {
			in_dwarf_debug_map = 1;
			continue;
		      }

		    oso_namestring = set_namestring (objfile, next_nlist, NULL);
		    if (parse_archive_name (oso_namestring, &dot_a_name, NULL))
		      {
			stat_ret = stat (dot_a_name, &buf);
			xfree (dot_a_name);
		      }
		    else
			stat_ret = stat (oso_namestring, &buf);

                    /* kexts are special fun.  If this is a kext with an
                       associated dSYM (we suppressed have_dsym_file earlier in
                       this case) then we need the symbol addresses from the
                       debug map but none of the .o files are present - and
                       we don't care so skip the stat() test.  
                       If this is a kext without a dSYM then we should look 
                       for the .o files.  */

		    if (stat_ret != 0
                        && objfile->not_loaded_kext_filename == NULL)
		      {
			warning ("Could not find object file \"%s\" - no debug "
				 "information available for \"%s\".\n",
				 oso_namestring, namestring);
			in_dwarf_debug_map = 1;
			missing_oso_file = 1;
			continue;
		      }
		  }
	      }
	    /* APPLE LOCAL end: dSYM with debug map */

    
	    /* Some other compilers (C++ ones in particular) emit useless
	       SOs for non-existant .c files.  We ignore all subsequent SOs that
	       immediately follow the first.  */

	    if (!pst)
              {
	        pst = start_psymtab (objfile,
				   namestring, valu,
				   first_so_symnum * symbol_size,
				   objfile->global_psymbols.next,
				   /* APPLE LOCAL symbol prefixes */
				   objfile->static_psymbols.next, prefix);
                if (pst && dirname_nso && pst->dirname == NULL)
                  {
                    pst->dirname = dirname_nso;
                    dirname_nso = NULL;
                  }
              }

            /* APPLE LOCAL: If there is a symbol separation file, put it in the 
               dependency list for this N_SO psymtab...  */
            {
              struct partial_symtab_list *listptr;
              
              for (listptr = symbol_sep_pst; 
                   listptr != NULL; 
                   listptr = listptr->next)
                dependency_list[dependencies_used++] = listptr->pst;
            }
            continue;
          }

          /* APPLE LOCAL: Handler for the N_OSO stab.  This gives the path to
             the .o file in case we are storing debug info there.  */
          case N_OSO:
          {
            namestring = set_namestring (objfile, nlist, NULL);

            /* The compiler now emits empty OSO stab strings all the time so
               the linker can optionally fix them up to valid OSO's if 
               appropriate without adding an new nlist record.  */
            if (namestring[0] == '\0')
              continue;

            if (pst == NULL)
              {
                /* FIXME: we should not get here without a PST to work on.
                   Attempt to recover.  */
                complaint (&symfile_complaints,
                       "N_OSO %s not in entries for any file, at symtab pos %d",
                           namestring, symnum);
                continue;
              }
            PSYMTAB_OSO_NAME (pst) = obsavestring (namestring, 
                                    strlen (namestring),
                                    &objfile->objfile_obstack);
            PSYMTAB_OSO_MTIME (pst) = (long) nlist.n_value;

            /* Next, check to see if this is the symbol sep file... */
            if (strstr (namestring, ".gch.o") != NULL)
              {
                struct partial_symtab_list *listptr = 
                                  xmalloc (sizeof (struct partial_symtab_list));
                make_cleanup (xfree, listptr);
                listptr->next = symbol_sep_pst;
                symbol_sep_pst = listptr;
                symbol_sep_pst->pst = pst;
              }

            /* Set the sym_offset to 0 for OSO files.  We either read
               them from a single .o, in which case the start IS 0, or
               we will set the sym_offset to the correct value in
               oso_scan_partial_symtab.  
               Note, we can't set it to 0 when we initialize the PST
               because start_psymtab will set it to the position in
               the linked image, which is wrong.  */
            LDSYMOFF (pst) = 0;
            /* Now handle the case where many SO's may share an OSO.  */
            if (last_oso_pst != NULL)
              {
                if (strcmp (PSYMTAB_OSO_NAME (last_oso_pst), 
                            PSYMTAB_OSO_NAME (pst)) == 0)
                  {
                    if (cur_oso_list == NULL)
                      {
                        cur_oso_list = init_oso_pst_list (PSYMTAB_OSO_NAME (pst),
                                                          objfile);
                        add_oso_pst_to_list (cur_oso_list, last_oso_pst, 
                                             objfile);
                      }
                    add_oso_pst_to_list (cur_oso_list, pst, objfile);
                  }
                else
                  {
                    if (cur_oso_list != NULL)
                      end_oso_pst_list (cur_oso_list, objfile);
                      cur_oso_list = NULL;
                  }
              }
            last_oso_pst = pst;

            /* Is this a DWARF debug map?  */
            if (nlist.n_desc == 1)
              {
                in_dwarf_debug_map = 1;

                /* Override the stabs psymtab->symtab expander func.  */
                pst->read_symtab = dwarf2_debug_map_psymtab_to_symtab;

                /* A DWARF debug map we put the function static data in the
                   psymtab itself so we don't use this.  */
                PSYMTAB_OSO_STATICS (pst) = NULL;
		/* At this point, we should read in the pubtypes table for this 
		   pst.  But only if we DON'T have a dSYM file for this psymtab.  
		   Also obey the read_type_psym_p flag, and don't do the scan
		   if we are asked not to.  */
		if (read_type_psym_p && objfile->separate_debug_objfile == NULL
                    && objfile->not_loaded_kext_filename == NULL)
		  {
		    dwarf2_scan_pubtype_for_psymbols (pst, objfile, 
						      psymtab_language);
		    /* APPLE LOCAL debug inlined section  */
		    dwarf2_scan_inlined_section_for_psymbols (pst, objfile,
							      psymtab_language);
		  }
              }

            continue;
          }
          /* End APPLE LOCAL */

	  case N_BINCL:
	  {
	    enum language tmp_language;
	    /* Add this bincl to the bincl_list for future EXCLs.  No
	       need to save the string; it'll be around until
	       read_dbx_symtab function returns */

	    /* APPLE LOCAL symbol prefixes */
	    namestring = set_namestring (objfile, nlist, NULL);

            /* APPLE LOCAL: Set the language based on the SO hint if possible */
            if (psymtab_language == language_unknown)
              {
	        tmp_language = deduce_language_from_filename (namestring);

	        /* Only change the psymtab's language if we've learned
	           something useful (eg. tmp_language is not language_unknown).
	           In addition, to match what start_subfile does, never change
	           from C++ to C.  */
	        /* APPLE LOCAL: Include ObjC++ */
	        if (tmp_language != language_unknown
		    && (tmp_language != language_c
		        || (psymtab_language != language_cplus 
			    && psymtab_language != language_objcplus)))
	        psymtab_language = tmp_language;
             }

	    if (pst == NULL)
	    {
	      /* FIXME: we should not get here without a PST to work on.
		 Attempt to recover.  */
	      complaint (&symfile_complaints,
			 _("N_BINCL %s not in entries for any file, at symtab pos %d"),
			 namestring, symnum);
	      continue;
	    }
	    add_bincl_to_list (pst, namestring, nlist.n_value);

	    /* Mark down an include file in the current psymtab */

	    goto record_include_file;
	  }

	  case N_SOL:
	  {
	    enum language tmp_language;
	    /* Mark down an include file in the current psymtab */

	    /* APPLE LOCAL symbol prefixes */
	    namestring = set_namestring (objfile, nlist, NULL);
            /* APPLE LOCAL: Set the language based on the SO hint if possible */
            if (psymtab_language == language_unknown)
              {
	        tmp_language = deduce_language_from_filename (namestring);

	        /* Only change the psymtab's language if we've learned
	           something useful (eg. tmp_language is not language_unknown).
	           In addition, to match what start_subfile does, never change
	           from C++ to C.  */
	        if (tmp_language != language_unknown
		    && (tmp_language != language_c
			/* APPLE LOCAL Objective-C++ */
		        || (psymtab_language != language_cplus
			    && psymtab_language != language_objcplus)))
	        psymtab_language = tmp_language;
              }

	    /* In C++, one may expect the same filename to come round many
	       times, when code is coming alternately from the main file
	       and from inline functions in other files. So I check to see
	       if this is a file we've seen before -- either the main
	       source file, or a previously included file.

	       This seems to be a lot of time to be spending on N_SOL, but
	       things like "break c-exp.y:435" need to work (I
	       suppose the psymtab_include_list could be hashed or put
	       in a binary tree, if profiling shows this is a major hog).  */
	    if (pst && strcmp (namestring, pst->filename) == 0)
	    continue;
	    {
	      int i;
	      for (i = 0; i < includes_used; i++)
		if (strcmp (namestring, psymtab_include_list[i]) == 0)
		  {
		    i = -1;
		    break;
		  }
	      if (i == -1)
		continue;
	    }

	    record_include_file:

	    psymtab_include_list[includes_used++] = namestring;
	    if (includes_used >= includes_allocated)
	    {
	      char **orig = psymtab_include_list;

	      psymtab_include_list = (char **)
		alloca ((includes_allocated *= 2) *
			sizeof (char *));
	      memcpy (psymtab_include_list, orig,
		      includes_used * sizeof (char *));
	    }
	    continue;
	  }
	  case N_LSYM:			/* Typedef or automatic variable. */
	  case N_STSYM:		/* Data seg var -- static  */
	  case N_LCSYM:		/* BSS      "  */
	  case N_ROSYM:		/* Read-only data seg var -- static.  */
	  case N_NBSTS:		/* Gould nobase.  */
	  case N_NBLCS:		/* symbols.  */
	    if (!read_type_psym_p)
	      break;

	  case N_FUN:
	  case N_GSYM:			/* Global (extern) variable; can be
					   data or bss (sigh FIXME).  */

	  /* Following may probably be ignored; I'll leave them here
	     for now (until I do Pascal and Modula 2 extensions).  */

	  case N_PC:			/* I may or may not need this; I
					   suspect not.  */
	  case N_M2C:			/* I suspect that I can ignore this here. */
	  case N_SCOPE:		/* Same.   */

	    /* APPLE LOCAL symbol prefixes */
	  namestring = set_namestring (objfile, nlist, NULL);

          /* See if this is an end of function stab.  */
          if (pst && nlist.n_type == N_FUN && *namestring == '\000')
	    {
	      CORE_ADDR valu;
	      
	      /* It's value is the size (in bytes) of the function for
		 function relative stabs, or the address of the function's
		 end for old style stabs.  */
	      if (processing_gcc_compilation || !end_fun_absolute_p)
		valu = nlist.n_value + last_function_start;
	      else
		valu = nlist.n_value;
	      
	      if (valu > pst->texthigh)
		pst->texthigh = valu;

	      /* APPLE LOCAL: Take care of the cur_fun_name.  */
	      if (cur_fun_name != NULL)
		{
		  xfree (cur_fun_name);
		  cur_fun_name = NULL;
		}
	      cur_fun_statics = NULL;
	      break;
	    }
	  
	  p = (char *) find_name_end (namestring);
	  if (!p)
	  continue;			/* Not a debugging symbol.   */


	  /* Main processing section for debugging symbols which
	     the initial read through the symbol tables needs to worry
	     about.  If we reach this point, the symbol which we are
	     considering is definitely one we are interested in.
	     p must also contain the (valid) index into the namestring
	     which indicates the debugging type symbol.  */

	  switch (p[1])
	  {
	    /* APPLE LOCAL: Use this to unify handling of 'f' and 'F'.  */
	    int is_global_fun;
          case 'V':
	    /* APPLE LOCAL: If we are reading stabs from a .o file, then we need
	       to record the local statics in a special list so we can track
	       the ones we see in the .o file...  */
	    if (PSYMTAB_OSO_NAME (pst))
	      {
		struct oso_fun_static *new_static;
		int sym_name_len;
		if (cur_fun_statics == NULL)
		  {
		    struct oso_fun_list *pst_statics;
		    struct oso_fun_list *tmp_ptr;
		    cur_fun_statics = (struct oso_fun_list *) 
		      xmalloc (sizeof (struct oso_fun_list));
		    pst_statics = PSYMTAB_OSO_STATICS (pst);
		    if (pst_statics == NULL)
		      PSYMTAB_OSO_STATICS (pst) = cur_fun_statics;
		    else
		      {
			for (tmp_ptr = pst_statics; tmp_ptr->next != NULL; 
			     tmp_ptr = tmp_ptr->next) { ; }
			tmp_ptr->next = cur_fun_statics;    
		      }
		    cur_fun_statics->next = NULL;
		    if (cur_fun_name != NULL)
		      cur_fun_statics->name = xstrdup (cur_fun_name);
		    else
		      {
			warning ("Building statics list with no function name");
			cur_fun_statics->name = NULL;
		      }
		    cur_fun_statics->statics = NULL;
		    cur_fun_statics->tail = NULL;
		  }

		new_static = (struct oso_fun_static *) xmalloc (sizeof (struct oso_fun_static));
		new_static->address = nlist.n_value 
		  + objfile_section_offset (objfile, data_sect_index);
		sym_name_len = p - namestring;
		new_static->name = xmalloc (sym_name_len + 1);
		strncpy (new_static->name, namestring, sym_name_len);
		new_static->name[sym_name_len] = '\0';
		new_static->next = NULL;

		if (cur_fun_statics->statics == NULL)
		  {
		    cur_fun_statics->statics = new_static;
		    cur_fun_statics->tail = new_static;
		  }
		else
		  {
		    cur_fun_statics->tail->next = new_static;
		    cur_fun_statics->tail = new_static;
		  }
	      }
	    continue;
	  case 'S':
	    nlist.n_value += objfile_section_offset (objfile, data_sect_index);
#ifdef STATIC_TRANSFORM_NAME
	    namestring = STATIC_TRANSFORM_NAME (namestring);
#endif
	    add_psymbol_to_list (namestring, p - namestring,
				 VAR_DOMAIN, LOC_STATIC,
				 &objfile->static_psymbols,
				 0, nlist.n_value,
				 psymtab_language, objfile);
	    continue;
	  case 'G':
	    nlist.n_value += objfile_section_offset (objfile, data_sect_index);
	    /* The addresses in these entries are reported to be
	       wrong.  See the code that reads 'G's for symtabs. */
	    add_psymbol_to_list (namestring, p - namestring,
				 VAR_DOMAIN, LOC_STATIC,
				 &objfile->global_psymbols,
				 0, nlist.n_value,
				 psymtab_language, objfile);
	    continue;

	  case 'T':
	    /* When a 'T' entry is defining an anonymous enum, it
	       may have a name which is the empty string, or a
	       single space.  Since they're not really defining a
	       symbol, those shouldn't go in the partial symbol
	       table.  We do pick up the elements of such enums at
	       'check_enum:', below.  */
	    if (p >= namestring + 2
		|| (p == namestring + 1
		    && namestring[0] != ' '))
	      {
		add_psymbol_to_list (namestring, p - namestring,
				     STRUCT_DOMAIN, LOC_TYPEDEF,
				     &objfile->static_psymbols,
				     nlist.n_value, 0,
				     psymtab_language, objfile);
		if (p[2] == 't')
		  {
		    /* Also a typedef with the same name.  */
		    add_psymbol_to_list (namestring, p - namestring,
					 VAR_DOMAIN, LOC_TYPEDEF,
					 &objfile->static_psymbols,
					 nlist.n_value, 0,
					 psymtab_language, objfile);
		    p += 1;
		  }
	      }
	    goto check_enum;
	  case 't':
	    if (p != namestring)	/* a name is there, not just :T... */
	      {
		add_psymbol_to_list (namestring, p - namestring,
				     VAR_DOMAIN, LOC_TYPEDEF,
				     &objfile->static_psymbols,
				     nlist.n_value, 0,
				     psymtab_language, objfile);
	      }
	  check_enum:
	    /* If this is an enumerated type, we need to
	       add all the enum constants to the partial symbol
	       table.  This does not cover enums without names, e.g.
	       "enum {a, b} c;" in C, but fortunately those are
	       rare.  There is no way for GDB to find those from the
	       enum type without spending too much time on it.  Thus
	       to solve this problem, the compiler needs to put out the
	       enum in a nameless type.  GCC2 does this.  */

	    /* We are looking for something of the form
	       <name> ":" ("t" | "T") [<number> "="] "e"
	       {<constant> ":" <value> ","} ";".  */

	    /* Skip over the colon and the 't' or 'T'.  */
	    p += 2;
	    /* This type may be given a number.  Also, numbers can come
	       in pairs like (0,26).  Skip over it.  */
	    while ((*p >= '0' && *p <= '9')
		   || *p == '(' || *p == ',' || *p == ')'
		   || *p == '=')
	      p++;

	    if (*p++ == 'e')
	      {
		/* The aix4 compiler emits extra crud before the members.  */
		if (*p == '-')
		  {
		    /* Skip over the type (?).  */
		    while (*p != ':')
		      p++;

		    /* Skip over the colon.  */
		    p++;
		  }

		/* We have found an enumerated type.  */
		/* According to comments in read_enum_type
		   a comma could end it instead of a semicolon.
		   I don't know where that happens.
		   Accept either.  */
		while (*p && *p != ';' && *p != ',')
		  {
		    char *q;

		    /* Check for and handle cretinous dbx symbol name
		       continuation!  */
		    if (*p == '\\' || (*p == '?' && p[1] == '\0'))
		      p = next_symbol_text (objfile);

		    /* Point to the character after the name
		       of the enum constant.  */
		    for (q = p; *q && *q != ':'; q++)
		      ;
		    /* Note that the value doesn't matter for
		       enum constants in psymtabs, just in symtabs.  */
		    add_psymbol_to_list (p, q - p,
					 VAR_DOMAIN, LOC_CONST,
					 &objfile->static_psymbols, 0,
					 0, psymtab_language, objfile);
		    /* Point past the name.  */
		    p = q;
		    /* Skip over the value.  */
		    while (*p && *p != ',')
		      p++;
		    /* Advance past the comma.  */
		    if (*p)
		      p++;
		  }
	      }
	    continue;
	  case 'c':
	    /* Constant, e.g. from "const" in Pascal.  */
	    add_psymbol_to_list (namestring, p - namestring,
				 VAR_DOMAIN, LOC_CONST,
				 &objfile->static_psymbols, nlist.n_value,
				 0, psymtab_language, objfile);
	    continue;

	    /* Global functions were ignored here, but now they
	       are put into the global psymtab like one would expect.
	       They're also in the minimal symbol table.  */
	  case 'F':
	    is_global_fun = 1;
	    goto global_or_local;
	  case 'f':
	    is_global_fun = 0;
	  global_or_local:
	    if (! pst)
	      {
		int name_len = p - namestring;
		char *name = xmalloc (name_len + 1);
		memcpy (name, namestring, name_len);
		name[name_len] = '\0';
		function_outside_compilation_unit_complaint (name);
		xfree (name);
	      }
	    nlist.n_value += objfile_text_section_offset (objfile);
	    /* Kludges for ELF/STABS with Sun ACC */
	    /* FIXME: namestring is not necessarily a pointer into the string table.
	       If you pass a non-null "prefix" to set_namestring, then it reuses a static
	       buffer.  So if there are any stabs we read the strings of after the last
	       function, then we will be left pointing at the wrong string.  */
	    last_function_name = namestring;
#ifdef SOFUN_ADDRESS_MAYBE_MISSING
	    /* Do not fix textlow==0 for .o or NLM files, as 0 is a legit
	       value for the bottom of the text seg in those cases. */
	    if (nlist.n_value == objfile_text_section_offset (objfile))
	      {
		CORE_ADDR minsym_valu = 
		  find_stab_function_addr (namestring, pst->filename, objfile);
		/* find_stab_function_addr will return 0 if the minimal
		   symbol wasn't found.  (Unfortunately, this might also
		   be a valid address.)  Anyway, if it *does* return 0,
		   it is likely that the value was set correctly to begin
		   with... */
		if (minsym_valu != 0)
		  nlist.n_value = minsym_valu;
	      }
	    if (pst && textlow_not_set)
	      {
		pst->textlow = nlist.n_value;
		textlow_not_set = 0;
	      }
#endif
	    /* End kludge.  */

	    /* Keep track of the start of the last function so we
	       can handle end of function symbols.  */
	    last_function_start = nlist.n_value;

	    /* In reordered executables this function may lie outside
	       the bounds created by N_SO symbols.  If that's the case
	       use the address of this function as the low bound for
	       the partial symbol table.  */
	    if (pst
		&& (textlow_not_set
		    || (nlist.n_value < pst->textlow
			&& (nlist.n_value
			    != objfile_text_section_offset (objfile)))))
	      {
		pst->textlow = nlist.n_value;
		textlow_not_set = 0;
	      }

	    /* APPLE LOCAL: Record the current function name so we can tag
	       the type 'V' STSYM's with it.  But only when we are using 
	       stabs from a .o file.  */
	    if (pst && PSYMTAB_OSO_NAME (pst) != NULL)
	      {   
		if (cur_fun_name != NULL)
		  xfree (cur_fun_name);
		cur_fun_name = xmalloc (p - namestring + 1);
		strncpy (cur_fun_name, namestring, p - namestring);
		cur_fun_name[p - namestring] = '\0';
	      }

	    add_psymbol_to_list (namestring, p - namestring,
				 VAR_DOMAIN, LOC_BLOCK,
				 is_global_fun ? &objfile->global_psymbols : &objfile->static_psymbols,
				 0, nlist.n_value,
				 psymtab_language, objfile);
	    continue;

	    /* Two things show up here (hopefully); static symbols of
	       local scope (static used inside braces) or extensions
	       of structure symbols.  We can ignore both.  */
	  case '(':
	  case '0':
	  case '1':
	  case '2':
	  case '3':
	  case '4':
	  case '5':
	  case '6':
	  case '7':
	  case '8':
	  case '9':
	  case '-':
	  case '#':		/* for symbol identification (used in live ranges) */
	    continue;

	  case ':':
	    /* It is a C++ nested symbol.  We don't need to record it
	       (I don't think); if we try to look up foo::bar::baz,
	       then symbols for the symtab containing foo should get
	       read in, I think.  */
	    /* Someone says sun cc puts out symbols like
	       /foo/baz/maclib::/usr/local/bin/maclib,
	       which would get here with a symbol type of ':'.  */
	    continue;

	  default:
	    /* Unexpected symbol descriptor.  The second and subsequent stabs
	       of a continued stab can show up here.  The question is
	       whether they ever can mimic a normal stab--it would be
	       nice if not, since we certainly don't want to spend the
	       time searching to the end of every string looking for
	       a backslash.  */

	    complaint (&symfile_complaints, _("unknown symbol descriptor `%c'"),
		       p[1]);

	    /* Ignore it; perhaps it is an extension that we don't
	       know about.  */
	    continue;
	  }

	  case N_EXCL:

	    /* APPLE LOCAL symbol prefixes */
	  namestring = set_namestring (objfile, nlist, NULL);

	  /* Find the corresponding bincl and mark that psymtab on the
	     psymtab dependency list */
	  {
	    struct partial_symtab *needed_pst =
	      find_corresponding_bincl_psymtab (namestring, nlist.n_value);
	    
	    /* If this include file was defined earlier in this file,
	       leave it alone.  */
	    if (needed_pst == pst)
	      continue;

	    if (needed_pst)
	      {
		int i;
		int found = 0;

		for (i = 0; i < dependencies_used; i++)
		  if (dependency_list[i] == needed_pst)
		    {
		      found = 1;
		      break;
		    }

		/* If it's already in the list, skip the rest.  */
		if (found)
		  continue;

		dependency_list[dependencies_used++] = needed_pst;
		if (dependencies_used >= dependencies_allocated)
		  {
		    struct partial_symtab **orig = dependency_list;
		    dependency_list =
		      (struct partial_symtab **)
		      alloca ((dependencies_allocated *= 2)
			      * sizeof (struct partial_symtab *));
		    memcpy (dependency_list, orig,
			    (dependencies_used
			     * sizeof (struct partial_symtab *)));
#ifdef DEBUG_INFO
		    fprintf_unfiltered (gdb_stderr, "Had to reallocate dependency list.\n");
		    fprintf_unfiltered (gdb_stderr, "New dependencies allocated: %d\n",
					dependencies_allocated);
#endif
		  }
	      }
	  }
	  continue;

	  case N_ENDM:
#ifdef SOFUN_ADDRESS_MAYBE_MISSING
	  /* Solaris 2 end of module, finish current partial symbol table.
	     end_psymtab will set pst->texthigh to the proper value, which
	     is necessary if a module compiled without debugging info
	     follows this module.  */
	  if (pst)
	  {
	    end_psymtab (pst, psymtab_include_list, includes_used,
			 symnum * symbol_size,
			 (CORE_ADDR) 0,
			 dependency_list, dependencies_used, textlow_not_set);
	    pst = (struct partial_symtab *) 0;
	    includes_used = 0;
	    dependencies_used = 0;
            in_dwarf_debug_map = 0;
	    missing_oso_file = 0;
	  }
#endif
	  continue;

	  case N_RBRAC:
#ifdef HANDLE_RBRAC
	  HANDLE_RBRAC (nlist.n_value);
	  continue;
#endif
	  case N_EINCL:
	  case N_DSLINE:
	  case N_BSLINE:
	  case N_SSYM:			/* Claim: Structure or union element.
					   Hopefully, I can ignore this.  */
	  case N_ENTRY:		/* Alternate entry point; can ignore. */
	  case N_MAIN:			/* Can definitely ignore this.   */
	  case N_CATCH:		/* These are GNU C++ extensions */
	  case N_EHDECL:		/* that can safely be ignored here. */
	  case N_LENG:
	  case N_BCOMM:
	  case N_ECOMM:
	  case N_ECOML:
	  case N_FNAME:
	  case N_SLINE:
	  case N_RSYM:
	  case N_PSYM:
	  case N_LBRAC:
	  case N_NSYMS:		/* Ultrix 4.0: symbol count */
	  case N_DEFD:			/* GNU Modula-2 */
	  case N_ALIAS:		/* SunPro F77: alias name, ignore for now.  */

          case N_OBJ:                   /* useless types from Solaris */
            continue;
          case N_OPT:
            {
              /* APPLE LOCAL: This stores the gcc2_compiled flag, and if the SO
                 stabs are not correct, this needs to be set when we read in the
                 debug info here.  */
              const char *tempstring;
              namestring = set_namestring (objfile, nlist, NULL);
              tempstring = namestring;

              if (DEPRECATED_STREQ (namestring, GCC_COMPILED_FLAG_SYMBOL))
                processing_gcc_compilation = 1;
              else if (DEPRECATED_STREQ (namestring, GCC2_COMPILED_FLAG_SYMBOL))
                processing_gcc_compilation = 2;
              if (tempstring[0] == bfd_get_symbol_leading_char (objfile->obfd))
                ++tempstring;
              if (DEPRECATED_STREQN (tempstring, "__gnu_compiled", 14))
                processing_gcc_compilation = 2;
            }
            continue; 

	  case N_PATCH:
	    /* APPLE LOCAL begin BNSYM/ENSYM */
	  case N_BNSYM:        /* MacOS X, no use to the debugger. */
	  case N_ENSYM:       
	    /* APPLE LOCAL end BNSYM/ENSYM */
	  /* These symbols aren't interesting; don't worry about them */

	    continue;

	  default:
	  /* If we haven't found it yet, ignore it.  It's probably some
	     new type we don't know about yet.  */
	  unknown_symtype_complaint (hex_string (nlist.n_type));
	  continue;
	}

      while ((namestring != NULL) && (namestring[strlen (namestring) - 1] == '\\'))
 	{
 	  namestring = next_symbol_text (objfile);
 	}
    }

  /* APPLE LOCAL: If there's an OSO_PST_LIST in flight, finish
     it off.  */
  if (cur_oso_list != NULL)
    {
      end_oso_pst_list (cur_oso_list, objfile);
      cur_oso_list = NULL;
    }

  /* If there's stuff to be cleaned up, clean it up.  */
  if (pst)
    {
      /* Don't set pst->texthigh lower than it already is.  */
      CORE_ADDR text_end =
	(lowest_text_address == (CORE_ADDR) -1
	 ? (text_addr + objfile_text_section_offset (objfile))
	 : lowest_text_address)
	+ text_size;

      end_psymtab (pst, psymtab_include_list, includes_used,
		   symnum * symbol_size,
		   text_end > pst->texthigh ? text_end : pst->texthigh,
		   dependency_list, dependencies_used, textlow_not_set);
      in_dwarf_debug_map = 0;
      missing_oso_file = 0;
    }
  
  clear_containing_archive_cache ();
  do_cleanups (back_to);
}

/* Allocate and partially fill a partial symtab.  It will be
   completely filled at the end of the symbol list.

   SYMFILE_NAME is the name of the symbol-file we are reading from, and ADDR
   is the address relative to which its symbols are (incremental) or 0
   (normal). */


static struct partial_symtab *
start_psymtab (struct objfile *objfile, char *filename, CORE_ADDR textlow,
	       int ldsymoff, struct partial_symbol **global_syms,
	       /* APPLE LOCAL symbol prefixes */
	       struct partial_symbol **static_syms, const char *prefix)
{
  struct partial_symtab *result =
  start_psymtab_common (objfile, objfile->section_offsets,
			filename, textlow, global_syms, static_syms);

  result->read_symtab_private = (char *)
    obstack_alloc (&objfile->objfile_obstack, sizeof (struct symloc));
  LDSYMOFF (result) = ldsymoff;

  result->read_symtab = dbx_psymtab_to_symtab;
  SYMBOL_SIZE (result) = symbol_size;
  SYMBOL_OFFSET (result) = symbol_table_offset;
  STRING_OFFSET (result) = string_table_offset;
  FILE_STRING_OFFSET (result) = file_string_table_offset;
  /* APPLE LOCAL symbol prefixes */
  SYMBOL_PREFIX (result) = prefix;

  /* If we're handling an ELF file, drag some section-relocation info
     for this source file out of the ELF symbol table, to compensate for
     Sun brain death.  This replaces the section_offsets in this psymtab,
     if successful.  */
  elfstab_offset_sections (objfile, result);

  /* APPLE LOCAL: Set the language based on the SO hint if possible */
  if (psymtab_language == language_unknown)
    {
      /* Deduce the source language from the filename for this psymtab. */
      psymtab_language = deduce_language_from_filename (filename);
    }

  /* APPLE LOCAL: Record the psymtab_language:  */
  result->language = psymtab_language;

  return result;
}

/* Close off the current usage of PST.  
   Returns PST or NULL if the partial symtab was empty and thrown away.

   FIXME:  List variables and peculiarities of same.  */

struct partial_symtab *
end_psymtab (struct partial_symtab *pst, char **include_list, int num_includes,
	     int capping_symbol_offset, CORE_ADDR capping_text,
	     struct partial_symtab **dependency_list, int number_dependencies,
	     int textlow_not_set)
{
  int i;
  struct objfile *objfile = pst->objfile;

  if (capping_symbol_offset != -1)
    LDSYMLEN (pst) = capping_symbol_offset - LDSYMOFF (pst);
  pst->texthigh = capping_text;

#ifdef SOFUN_ADDRESS_MAYBE_MISSING
  /* Under Solaris, the N_SO symbols always have a value of 0,
     instead of the usual address of the .o file.  Therefore,
     we have to do some tricks to fill in texthigh and textlow.
     The first trick is: if we see a static
     or global function, and the textlow for the current pst
     is not set (ie: textlow_not_set), then we use that function's
     address for the textlow of the pst.  */

  /* Now, to fill in texthigh, we remember the last function seen
     in the .o file.  Also, there's a hack in
     bfd/elf.c and gdb/elfread.c to pass the ELF st_size field
     to here via the misc_info field.  Therefore, we can fill in
     a reliable texthigh by taking the address plus size of the
     last function in the file.  */

  if (pst->texthigh == 0 && last_function_name)
    {
      char *p;
      int n;
      struct minimal_symbol *minsym;

      /* APPLE LOCAL */
      p = find_name_end (last_function_name);
      if (p == NULL)
	p = last_function_name;
      n = p - last_function_name;
      p = alloca (n + 2);
      strncpy (p, last_function_name, n);
      p[n] = 0;

      minsym = lookup_minimal_symbol (p, pst->filename, objfile);
      if (minsym == NULL)
	{
	  /* Sun Fortran appends an underscore to the minimal symbol name,
	     try again with an appended underscore if the minimal symbol
	     was not found.  */
	  p[n] = '_';
	  p[n + 1] = 0;
	  minsym = lookup_minimal_symbol (p, pst->filename, objfile);
	}

      if (minsym)
	pst->texthigh = SYMBOL_VALUE_ADDRESS (minsym) + MSYMBOL_SIZE (minsym);

      last_function_name = NULL;
    }

  /* this test will be true if the last .o file is only data */
  if (textlow_not_set)
    pst->textlow = pst->texthigh;
  else
    {
      struct partial_symtab *p1;

      /* If we know our own starting text address, then walk through all other
         psymtabs for this objfile, and if any didn't know their ending text
         address, set it to our starting address.  Take care to not set our
         own ending address to our starting address, nor to set addresses on
         `dependency' files that have both textlow and texthigh zero.  */

      ALL_OBJFILE_PSYMTABS (objfile, p1)
      {
	if (p1->texthigh == 0 && p1->textlow != 0 && p1 != pst)
	  {
	    p1->texthigh = pst->textlow;
	    /* if this file has only data, then make textlow match texthigh */
	    if (p1->textlow == 0)
	      p1->textlow = p1->texthigh;
	  }
      }
    }

  /* End of kludge for patching Solaris textlow and texthigh.  */
#endif /* SOFUN_ADDRESS_MAYBE_MISSING.  */

  pst->n_global_syms =
    objfile->global_psymbols.next - (objfile->global_psymbols.list + pst->globals_offset);
  pst->n_static_syms =
    objfile->static_psymbols.next - (objfile->static_psymbols.list + pst->statics_offset);

  pst->number_of_dependencies = number_dependencies;
  if (number_dependencies)
    {
      pst->dependencies = (struct partial_symtab **)
	obstack_alloc (&objfile->objfile_obstack,
		    number_dependencies * sizeof (struct partial_symtab *));
      memcpy (pst->dependencies, dependency_list,
	      number_dependencies * sizeof (struct partial_symtab *));
    }
  else
    pst->dependencies = 0;

  for (i = 0; i < num_includes; i++)
    {
      struct partial_symtab *subpst =
      allocate_psymtab (include_list[i], objfile);

      /* Copy the sesction_offsets array from the main psymtab. */
      subpst->section_offsets = pst->section_offsets;
      subpst->read_symtab_private =
	(char *) obstack_alloc (&objfile->objfile_obstack,
				sizeof (struct symloc));
      LDSYMOFF (subpst) =
	LDSYMLEN (subpst) =
	subpst->textlow =
	subpst->texthigh = 0;
      /* APPLE LOCAL: These have to be set to NULL because we do 
	 check them later.  */

      PSYMTAB_OSO_NAME (subpst) = NULL;
      PSYMTAB_OSO_PST_LIST (subpst) = NULL;
      PSYMTAB_OSO_STATICS (subpst) = NULL;
      /* END APPLE LOCAL  */

      /* We could save slight bits of space by only making one of these,
         shared by the entire set of include files.  FIXME-someday.  */
      subpst->dependencies = (struct partial_symtab **)
	obstack_alloc (&objfile->objfile_obstack,
		       sizeof (struct partial_symtab *));
      subpst->dependencies[0] = pst;
      subpst->number_of_dependencies = 1;

      subpst->globals_offset =
	subpst->n_global_syms =
	subpst->statics_offset =
	subpst->n_static_syms = 0;

      subpst->readin = 0;
      subpst->symtab = 0;
      subpst->read_symtab = pst->read_symtab;
    }

  sort_pst_symbols (pst);

  /* If there is already a psymtab or symtab for a file of this name, remove it.
     (If there is a symtab, more drastic things also happen.)
     This happens in VxWorks.  */
  free_named_symtabs (pst->filename);

  /* APPLE LOCAL fix-and-continue */
  PSYMTAB_OBSOLETED (pst) = 50;

  /* APPLE LOCAL: Don't throw away the PST if it has an OSO associated
     with it.  */

  if (num_includes == 0
      && number_dependencies == 0
      && pst->n_global_syms == 0
      && pst->n_static_syms == 0
      && has_line_numbers == 0
      && PSYMTAB_OSO_NAME (pst) == NULL)
    {
      /* Throw away this psymtab, it's empty.  We can't deallocate it, since
         it is on the obstack, but we can forget to chain it on the list.  */
      /* Empty psymtabs happen as a result of header files which don't have
         any symbols in them.  There can be a lot of them.  But this check
         is wrong, in that a psymtab with N_SLINE entries but nothing else
         is not empty, but we don't realize that.  Fixing that without slowing
         things down might be tricky.  */

      discard_psymtab (pst);

      /* Indicate that psymtab was thrown away.  */
      pst = (struct partial_symtab *) NULL;
    }
  return pst;
}

/* This is a convenience routine for closing off the "containing archive"
   that is passed to you from open_bfd_from_oso.  It closes all the bfd's
   that you might have opened searching for the right architecture file in
   a fat file, and then looking for the member within the archive.  So you
   can't hold onto any of these bfd's after calling this.  */

static void
close_containing_archive_and_contents (bfd *containing_archive)
{
  bfd_free_cached_info (containing_archive);
  bfd_close (containing_archive);
}

/* This is a convenience routine for closing a BFD that might
   be a fat file, where you want to make sure that the archive
   gets closed if there is one (and all its contents.)  */

void
close_bfd_or_archive (bfd *abfd)
{
  if (abfd->my_archive)
    close_containing_archive_and_contents (abfd->my_archive);
  else
    bfd_close (abfd);
}

/* For DWARF files with debug info in .o files, we scan all the .o's for type
   symbols in the "pubtypes" section.  If the debug info is from an archive file
   we'll end up opening & closing that .a file MANY times.  So this array stores
   the a cache of the archives .a files that we've opened.  
   The use pattern should be that whenever you use find_bfd_from_oso, if CACHED is
   returned true, then don't close the bfd you were looking at, but rather when you're
   all done with the objfile you were looking in, call clear_containing_archive_cache.
*/
  
#define PUBTYPE_ARCHIVES_CHUNK 10
static struct bfd **pubtype_bfd_array;
static int num_archives_for_pubtypes;
static int max_num_archives_for_pubtypes;

static bfd *
find_in_containing_archive_cache (char *archive_name)
{
  int i;
  for (i = 0; i < num_archives_for_pubtypes; i++)
    {
      if (strcmp (archive_name, pubtype_bfd_array[i]->filename) == 0)
	{
	  return pubtype_bfd_array[i];
	}
    }
  return NULL;
}

/* Adds CONTAINING_ARCHIVE to the archive cache.  */

static void
add_to_containing_archive_cache (bfd *containing_archive)
{
  
  int i;

  /* This is a fast check, and if somebody inadvertently adds
     the same bfd twice, we'll crash when we go to clear the
     cache, it's worth doing it here.  */

  for (i = 0; i < num_archives_for_pubtypes; i++)
    if (pubtype_bfd_array[i] == containing_archive)
      return;

  if (num_archives_for_pubtypes == max_num_archives_for_pubtypes)
    {
      max_num_archives_for_pubtypes += PUBTYPE_ARCHIVES_CHUNK;
      pubtype_bfd_array 
	= xrealloc (pubtype_bfd_array, 
		    max_num_archives_for_pubtypes * sizeof (struct bfd *)); 
    }

  pubtype_bfd_array[num_archives_for_pubtypes++]
    = containing_archive;
}

void
clear_containing_archive_cache ()
{
  while (num_archives_for_pubtypes > 0)
    {
      struct bfd *archive = pubtype_bfd_array[--num_archives_for_pubtypes];
      pubtype_bfd_array[num_archives_for_pubtypes] = NULL;
      close_containing_archive_and_contents (archive);
    }
}

/* Given OSO_NAME, returns the bfd for the .o file containing
   that .o.  If the .o is fat, it returns the fork for the current
   architecture.  So you should probably check the my_archive field
   of the bfd & close that if it is not NULL...

   If the name is of the form:

   /Foo/Bar/libfoo.a(member.o)

   Then we will iterate through the archive and find the bfd for
   the .o in question.  In this case, we'll return CACHED true.
   If CACHED is true, don't delete the bfd, but rather call
   clear_containing_archive_cache.  That will close the bfd
   and anything else we opened to find it.  Note, you don't need
   to do this till you're all done with looking through the archive,
   and this will save reopening it over and over.

   When we find the bfd, we check its mtime against the OSO_MTIME and
   warn if the times don't match.  */

struct bfd *
open_bfd_from_oso (struct partial_symtab *pst, int *cached)
{
  struct bfd *oso_bfd, *retval;
  long mtime;
  char *oso_name;
  long oso_mtime;
  char *archive_name;
  char *member_name;

  *cached = 0;

  oso_name = PSYMTAB_OSO_NAME (pst);
  oso_mtime = PSYMTAB_OSO_MTIME (pst);

  if (parse_archive_name (oso_name, &archive_name, &member_name) == 0)
    {
      errno = 0;
      oso_bfd = bfd_openr (oso_name, gnutarget);
      if (!oso_bfd)
        {
	  /* Only error if we do not have a separate debug objfile (dSYM).  */
	  if (!pst->objfile || pst->objfile->separate_debug_objfile == NULL)
            warning ("Could not open object file: \"%s\": %s", oso_name, strerror (errno));
	  return NULL;
        }
      if (bfd_check_format (oso_bfd, bfd_archive))
	{
	  oso_bfd = open_bfd_matching_arch (oso_bfd, bfd_object);
	  if (oso_bfd == NULL)
	    {
	      warning ("Could not open OSO file matching current "
		       "architecture for \"%s\".",
		       oso_name);
	      return NULL;
	    }
	  
	}
      retval = oso_bfd;
      mtime = bfd_get_mtime (retval);
    }
  else
    {
      struct bfd *archive_bfd, *member_bfd;
      struct stat member_statbuf;
      int status;
      struct cleanup *free_archive_name, *free_member_name;
      free_member_name = make_cleanup (xfree, member_name);
      free_archive_name = make_cleanup (xfree, archive_name);

      archive_bfd = find_in_containing_archive_cache (archive_name);
      if (archive_bfd == NULL)
	{
	  archive_bfd = bfd_openr (archive_name, gnutarget);
	  if (archive_bfd == NULL)
	    {
	      warning ("Could not open OSO archive file \"%s\"", archive_name);
	      retval = NULL;
	      goto do_cleanups;
	    }
	  add_to_containing_archive_cache (archive_bfd);
	}
      *cached = 1;

      /* If we got here, the archive_bfd archive_name now belongs to the 
         archive, so we can't free it.  */

      discard_cleanups (free_archive_name);
      if (!bfd_check_format (archive_bfd, bfd_archive))
	{
	  warning ("OSO archive file \"%s\" not an archive.",archive_name);
	  retval = NULL;
	  goto do_cleanups;
	}

      if (strcmp (archive_bfd->xvec->name, "mach-o-fat") == 0)
	{
	  /* GRRR...  Archives of type mach-o-fat are fat files, not 
	     .a files.  So look for the .a file matching the current'
	     architecture.  */
	  archive_bfd = open_bfd_matching_arch (archive_bfd, bfd_archive);

	  if (archive_bfd == NULL)
	    {
	      warning ("Could not open fork matching current "
		       "architecture for OSO archive \"%s\"",
		       archive_name);
	      retval = NULL;
	      goto do_cleanups;
	    }
	  if (!bfd_check_format (archive_bfd, bfd_archive))
	    {
	      warning ("Current architecture fork of OSO archive "
		       "file \"%s\" not an archive", archive_name);
	      retval = NULL;
	      goto do_cleanups;
	    }
	}

      member_bfd = bfd_openr_next_archived_file (archive_bfd, NULL);

      if (member_bfd == NULL)
	{
	  warning ("Could not read archive members out of OSO archive \"%s\"",
		   archive_name);
	  retval = NULL;
	  goto do_cleanups;
	}

      while (member_bfd != NULL)
	{
	  if (strcmp (member_bfd->filename, member_name) == 0)
	    break;
          member_bfd = bfd_openr_next_archived_file (archive_bfd, member_bfd);
	}
      if (member_bfd == NULL)
	{
	  warning ("Could not find specified archive member for OSO name \"%s\"",
		   oso_name);
	  return NULL;
	}

      retval = member_bfd;
      status = bfd_stat_arch_elt (member_bfd, &member_statbuf);
      if (status == -1) 
	mtime = 0;
      else
	mtime = member_statbuf.st_mtime;
    do_cleanups:
      do_cleanups (free_member_name);
    }

  if (retval != NULL && mtime && oso_mtime && mtime != oso_mtime)
    {
      char *name;
      if (pst->objfile->name != NULL)
	name = pst->objfile->name;
      else
	name = "<Unknown objfile>";

      warning (".o file \"%s\" more recent than executable timestamp in \"%s\"", oso_name, name);
      if (cached)
	clear_containing_archive_cache ();
      else 
	close_bfd_or_archive (retval);

      return NULL;
    }

  return retval;
}
/* END APPLE LOCAL */

/* APPLE LOCAL: Scan the .o containing PST to build up the "dependencies"
   array for all the pst's contained in this .o file.  We also set the sym offset
   while we are at it, so we can find the separate .o's again more efficiently.  */

void
oso_scan_partial_symtab (struct partial_symtab *pst)
{
  struct internal_nlist nlist;
  struct oso_pst_list *list;
  int cached;
  struct bfd *oso_bfd;
  struct cleanup *oso_data_cleanup;
  int num_syms, sym_size, strtab_size, sym_offset;
  char *strtab_data;
  int sect_p;
  unsigned char type;
  const char *prefix;
  int current_list_element = -1;
  struct partial_symtab *current_pst;
  struct objfile *objfile;
  char leading_char;
  /* Index within current psymtab dependency list */
  struct partial_symtab **dependency_list;
  int dependencies_used, dependencies_allocated;

  dependencies_allocated = 30;
  dependencies_used = 0;
  dependency_list =
    (struct partial_symtab **) alloca (dependencies_allocated *
				       sizeof (struct partial_symtab *));

  list = PSYMTAB_OSO_PST_LIST (pst);
  if (list == NULL || list->readin)
    return;

  list->readin = 1;
  
  prefix = SYMBOL_PREFIX (pst);
  objfile = pst->objfile;

  oso_bfd = open_bfd_from_oso (pst, &cached);
  if (oso_bfd == NULL)
    {
      warning ("Couldn't open bfd for .o file: %s.", PSYMTAB_OSO_NAME (pst));
      return;
    }

  if (!bfd_check_format (oso_bfd, bfd_object))
    warning ("Not in bfd_object form");
  
  leading_char = bfd_get_symbol_leading_char (oso_bfd);

  /* Read the nlist data into stabs_data, and read the string_table into 
     strtab_ptr.  */
  oso_data_cleanup = stabsect_read_strtab_from_oso (oso_bfd, &num_syms, 
                           &sym_size, &strtab_size, &stabs_data, &strtab_data);
  
  sym_offset = 0;
  symbuf_end = symbuf_idx = 0;
  symbuf_read = 0;
  symbuf_left = sym_offset + num_syms * sym_size;

  stabs_seek (sym_offset);

  init_bincl_list (20, objfile);
  make_cleanup_free_bincl_list (objfile);

  for (symnum = 0;
       symnum < num_syms;
       symnum++)
    {
      
      NEXT_SYMBOL (nlist, sect_p, oso_bfd);
      
      type = nlist.n_type;
      switch (type)
	{
	case N_SO:
	  {
	    char *p;
	    static int prev_so_symnum = -10;
	    static int first_so_symnum;
	    char *namestring;
	    namestring = set_namestring_1 (strtab_size, strtab_data,
					   leading_char, nlist, prefix);

	    if (prev_so_symnum != symnum - 1)
	      first_so_symnum = symnum;

	    prev_so_symnum = symnum;

	    if (*namestring == '\0')
	      {
		/* This is the end of one symtab.  
                   Consolidate the dependencies.  */
		LDSYMLEN (current_pst) = symnum * symbol_size - 
                                                       LDSYMOFF (current_pst);
		
		current_pst->number_of_dependencies = dependencies_used;
		if (dependencies_used)
		  {
		    current_pst->dependencies = (struct partial_symtab **)
		      obstack_alloc (&objfile->objfile_obstack,
                          dependencies_used * sizeof (struct partial_symtab *));
		    memcpy (current_pst->dependencies, dependency_list,
                          dependencies_used * sizeof (struct partial_symtab *));
		  }
		else
		  current_pst->dependencies = 0;
		
		dependencies_used = 0;
		current_pst = NULL;
		continue;
	      }
	    
	    p = strrchr (namestring, '/');
	    if (p && *(p + 1) == '\000')
	      continue;		/* Simply ignore directory name SOs */

	    /* Look up the partial_symtab corresponding to this SO.  
	       We require that the SO's go into the final linked product in the 
	       same order, with no omissions, as in the .o file.  This makes the
	       scan much quicker.  We can relax this if we have to, but it would
	       be better if the linker just always arranges for this 
               to be true.  */
	    current_list_element++;
	    current_pst = list->pst_list[current_list_element]; 
	    if (strcmp(current_pst->filename, namestring) != 0)
	      error ("SO in .o file \"%s\" out of order\n", 
                     PSYMTAB_OSO_NAME (pst));

	    LDSYMOFF (current_pst) = first_so_symnum * symbol_size;

	    break;
	  }
	case N_BINCL:
	  {
	    /* Add the bincl to the bincl_list we are compiling.  Note, 
	       in read_dbx_symtab we also make partial_symtab's for each 
	       include we discover.  But for BINCL/EINCL headers, this 
	       serves no purpose that I can detect, so I neglect it here.  */
	    char *namestring;
	    namestring = set_namestring_1 (strtab_size, strtab_data,
                                     leading_char, nlist, prefix);
	    add_bincl_to_list (current_pst, namestring, nlist.n_value);
	    break;
	  }
	case N_EXCL:
	  {
	    struct partial_symtab *needed_pst;
	    char *namestring;
	    namestring = set_namestring_1 (strtab_size, strtab_data,
					   leading_char, nlist, prefix);


	    needed_pst = find_corresponding_bincl_psymtab (namestring, 
                                                           nlist.n_value);
	    if (needed_pst == current_pst)
	      continue;

	    /* FIXME: THis code is stolen from read_dbx_symtab.  Should we 
	       make this a function?  */
	    if (needed_pst)
	      {
		int i;
		int found = 0;

		for (i = 0; i < dependencies_used; i++)
		  if (dependency_list[i] == needed_pst)
		    {
		      found = 1;
		      break;
		    }

		/* If it's already in the list, skip the rest.  */
		if (found)
		  continue;

		dependency_list[dependencies_used++] = needed_pst;
		if (dependencies_used >= dependencies_allocated)
		  {
		    struct partial_symtab **orig = dependency_list;
		    dependency_list =
		      (struct partial_symtab **)
		      alloca ((dependencies_allocated *= 2)
			      * sizeof (struct partial_symtab *));
		    memcpy (dependency_list, orig,
			    (dependencies_used
			     * sizeof (struct partial_symtab *)));
		  }
	      }
	    
	    break;
	  }
	default:
	  continue;
	}
    }

  do_cleanups (oso_data_cleanup);

  if (cached)
    clear_containing_archive_cache ();
  else 
    close_bfd_or_archive (oso_bfd);
}

/* APPLE LOCAL: Called from dwarf2read.c, this function reads all the
   minimal symbols out of an OSO file and allocates + populates the
   *NLISTS array of nlist records.  This routine malloc's the array
   of structures as well as the symbol name strings -- it is the caller's
   responsibility to free them.  NLISTS will always be allocated, even
   if no nlist records are found.
   The COMMON_SYMNAMES array of pointers to strings allocated along the
   same lines as the NLISTS array.  Common symbols (uninitialized global
   variables) have no address in a .o file.  COMMON_SYMNAMES will point
   to a malloc'ed array regardless of whether any common symbols are 
   found; it is the caller's responsibility to free that array as well
   as any symbol names that may have been allocated while creating the
   array.  */

void
read_oso_nlists (bfd *oso_bfd, struct partial_symtab *pst,
                 struct nlist_rec **nlists, int *nlists_count,
                 char ***common_symnames, int *common_symnames_count)
{
  struct internal_nlist nlist;
  const char *prefix;
  struct objfile *objfile;
  char leading_char;
  int num_syms, sym_size, strtab_size, sym_offset;
  char *strtab_data;
  int sect_p;
  int nlists_arr_size = 256;
  int common_symnames_arr_size = 32;
  char *namestring;
  struct cleanup *oso_data_cleanup;
 
  *nlists = (struct nlist_rec *) xmalloc 
                                  (sizeof (struct nlist_rec) * nlists_arr_size);
  *nlists_count = 0;
 
  *common_symnames = (char **) xmalloc 
                                  (sizeof (char *) * common_symnames_arr_size);
  *common_symnames_count = 0;

  prefix = SYMBOL_PREFIX (pst);
  objfile = pst->objfile;
  processing_objfile = pst->objfile;  /* Initialize for NEXT_SYMBOL's use */

  leading_char = bfd_get_symbol_leading_char (oso_bfd);
  oso_data_cleanup = stabsect_read_strtab_from_oso (oso_bfd, 
                                         &num_syms, &sym_size, 
                                         &strtab_size, &stabs_data, 
                                         &strtab_data);
  
  sym_offset = 0;
  symbuf_end = symbuf_idx = 0;
  symbuf_read = 0;
  symbuf_left = sym_offset + num_syms * sym_size;

  stabs_seek (sym_offset);

  for (symnum = 0;
       symnum < num_syms;
       symnum++)
    {
      int record_standard = 0;
      int record_common = 0;

      NEXT_SYMBOL (nlist, sect_p, oso_bfd);

      /* A stab nlist?  That shouldn't be here.  Issue warning?  */
      if (nlist.n_type & N_STAB)
        continue; 
      
      switch (nlist.n_type)
        {
           /*
            * Standard, external, non-debugger, symbols
            */

           case N_TEXT:
           case N_TEXT | N_EXT:
           case N_NBTEXT | N_EXT:

           case N_DATA:
           case N_DATA | N_EXT:
           case N_NBDATA | N_EXT:

           case N_BSS:
           case N_BSS | N_EXT:
           case N_NBBSS | N_EXT:
           case N_SETV | N_EXT:

           case N_ABS | N_EXT:

           /* Standard, local, non-debugger, symbols */

           case N_NBTEXT:
           case N_FN:
           case N_FN_SEQ:

           record_standard = 1;
           break;

           /* Common symbols (uninitialzed globals, statics) */

           case N_UNDF | N_EXT:

           record_common = 1;
           break;
        }

      /* APPLE LOCAL: The new ld outputs symbols for all the
         stubs in the SYMBOL_STUBS sections for its convenience.
         These are not the symbols we use to do the mapping
         from PC to stub target.  So we need to suppress
         reading these in.  */
      if (sect_p && !macosx_record_symbols_from_sect_p (oso_bfd,
                                                        nlist.n_type,
                                                        nlist.n_other))
        continue;

      if (record_standard)
        {
          char *c;
          int name_has_dot = 0;
          int name_is_anon_namespace = 0;
          int name_dot_followed_by_digit = 0;

          /* APPLE LOCAL symbol prefixes */
          namestring = set_namestring_1 (strtab_size, strtab_data,
                                         leading_char, nlist, prefix);

          if (leading_char == namestring[0])
            namestring++;

          /* Symbol names like "foo.h" and ".objc_class_name_SKTGraphic" are
             not relevant to the user and will only confuse things.  */

          c = strchr (namestring, '.');
          if (c != NULL)
            name_has_dot = 1;

          /* However, symbols in a C++ anonymous namespace will have dots
             in them, and that's A-OK.  e.g. (anonymous namespace)::foo(int)
             in a.cc is _ZN33_GLOBAL__N_a.cc_00000000_3D44F7283fooEi.  */

          if (name_has_dot && namestring[0] == '_' && namestring[1] == 'Z')
            {
              char *demangled = cplus_demangle_v3 (namestring, DMGL_ANSI);
              if (demangled != NULL)
                {
                  if (strstr (demangled, "anonymous namespace") != NULL)
                    name_is_anon_namespace = 1;
                  xfree (demangled);
                }
            }

          /* If the dot is followed by a number, we're probably looking
             at a function static variable, e.g "foo () {static int myvar;}"
             will give you a linker symbol name of myvar.3821 or some similarly
             unique name.  */
          if (name_has_dot && *(c + 1) >= '0' && *(c + 1) <= '9')
                name_dot_followed_by_digit = 1;

          if (name_has_dot 
              && !name_is_anon_namespace 
              && !name_dot_followed_by_digit)
            continue;

          (*nlists)[*nlists_count].addr = nlist.n_value;
          (*nlists)[*nlists_count].name = xstrdup (namestring);
         
          if (++(*nlists_count) == nlists_arr_size)
            {
              nlists_arr_size *= 2;
              *nlists = (struct nlist_rec *) 
                            xrealloc (*nlists, 
                                 sizeof (struct nlist_rec) * nlists_arr_size);
            }
        }

      if (record_common)
        {
          /* APPLE LOCAL symbol prefixes */
          namestring = set_namestring_1 (strtab_size, strtab_data,
                                         leading_char, nlist, prefix);
          if (leading_char == namestring[0])
            namestring++;

          (*common_symnames)[*common_symnames_count] = xstrdup (namestring);
          if (++(*common_symnames_count) == common_symnames_arr_size)
            {
              common_symnames_arr_size *= 2;
              *common_symnames = (char **) 
                            xrealloc (*common_symnames, 
                                 sizeof (char *) * common_symnames_arr_size);
            }
        }
    }

  do_cleanups (oso_data_cleanup);
}

static void
dbx_psymtab_to_symtab_1 (struct partial_symtab *pst)
{
  struct cleanup *old_chain;
  int i;

  if (!pst)
    return;

  if (pst->readin)
    {
      fprintf_unfiltered (gdb_stderr, "Psymtab for %s already read in.  Shouldn't happen.\n",
			  pst->filename);
      return;
    }

  /* APPLE LOCAL: If we are reading the full symbols for this partial_symtab
     from a .o file, and the .o file contains more than one N_SO section, then 
     make sure we've built the EXCL->BINCL/EINCL dependencies for it.  */
  if (SYMLOC (pst) != NULL 
      && PSYMTAB_OSO_PST_LIST (pst) != NULL 
      && !((PSYMTAB_OSO_PST_LIST (pst))->readin))
     oso_scan_partial_symtab (pst);

  /* Read in all partial symtabs on which this one is dependent */
  for (i = 0; i < pst->number_of_dependencies; i++)
    if (!pst->dependencies[i]->readin)
      {
	/* Inform about additional files that need to be read in.  */
	if (info_verbose)
	  {
	    fputs_filtered (" ", gdb_stdout);
	    wrap_here ("");
	    fputs_filtered ("and ", gdb_stdout);
	    wrap_here ("");
	    printf_filtered ("%s...", pst->dependencies[i]->filename);
	    wrap_here ("");	/* Flush output */
	    gdb_flush (gdb_stdout);
	  }
	dbx_psymtab_to_symtab_1 (pst->dependencies[i]);
      }

  if (LDSYMLEN (pst))		/* Otherwise it's a dummy */
    {
      /* Init stuff necessary for reading in symbols */
      stabsread_init ();
      buildsym_init ();
      old_chain = make_cleanup (really_free_pendings, 0);
      /* Read in this file's symbols */

      /* APPLE LOCAL: Add support for reading debug info out of
         the .o files.  */
      if (!PSYMTAB_OSO_NAME (pst))
	{
	  file_string_table_offset = FILE_STRING_OFFSET (pst);
	  symbol_size = SYMBOL_SIZE (pst);

	  bfd_seek (pst->objfile->obfd, SYMBOL_OFFSET (pst), SEEK_SET);
	  read_ofile_symtab (pst);
	}
      else
	{
	  /* If we have an OSO name, then we are looking for the FULL 
	     symbols in the .o file.  So we have to open that one,
	     and read it in.  */
	  struct bfd *oso_bfd;
	  int cached;

	  oso_bfd = open_bfd_from_oso (pst, &cached);
	  if (oso_bfd == NULL)
	    warning ("Couldn't open bfd for .o file: %s\n", PSYMTAB_OSO_NAME (pst));
	  else
	    {
	      if (!bfd_check_format (oso_bfd, bfd_object))
		warning ("Not in bfd_object form");

	      read_ofile_symtab_from_oso (pst, oso_bfd);
	      /* Either dispose of the containing archive & all it's contents,
		or just the one oso_bfd.  */
	      if (cached)
		clear_containing_archive_cache ();
	      else if (oso_bfd->my_archive)
		bfd_close (oso_bfd->my_archive);
	      else		  
		bfd_close(oso_bfd);
	    }
	}
      /* END APPLE LOCAL */
      do_cleanups (old_chain);
    }

  pst->readin = 1;
}

/* APPLE LOCAL: Read in the debug information from a .o file.  This is a bit
   of a Frankenstein function because I want to read in the stabs, but I don't
   want to have to build a whole objfile for the .o.  That seems too heavy-weight.

   This fills the stabs_data global with the nlist data, and returns a pointer to
   the string table.  It returns the number of stabs in SYMCOUNT, the size of the
   the nlist entries in SYMSIZE, and the size of the string table in STRTAB_SIZE

   FIXME: This should probably find its way to machoread.c.
*/
struct oso_data_cleanup
{
#ifdef HAVE_MMAP
  bfd_window stab_window;
  bfd_window strtab_window;
#endif
  bfd_byte **stab_data_handle;
  char **strtab_data_handle;
};

static void
cleanup_oso_data (void *data)
{
  struct oso_data_cleanup *args = (struct oso_data_cleanup *) data;
#ifdef HAVE_MMAP
  bfd_free_window (&(args->stab_window));
  bfd_free_window (&(args->strtab_window));
#else
  xfree (*args->stab_data_handle);
  xfree (*args->strtab_window);
#endif
  *args->stab_data_handle = NULL;
  *args->strtab_data_handle = NULL;
  xfree (args);
}

/* APPLE LOCAL: This is how we open the stabs data for a .o file for Mach-o.  
   FIXME: Should move this into machoread.c, and figure out how to make this 
   more abstract.  Problem is I need to use the stabs_data static to point to 
   the data, and that isn't (and should not be) exported...  */

static struct cleanup *
stabsect_read_strtab_from_oso (struct bfd *oso_bfd, int *symcount, 
                               int *symsize, int *strtab_size,
			       bfd_byte **stab_data_handle, 
                               char **strtab_data_handle)
{
  asection *stabsect = bfd_get_section_by_name (oso_bfd, "LC_SYMTAB.stabs");
  asection *stabstrsect = bfd_get_section_by_name (oso_bfd,  "LC_SYMTAB.stabstr");
  int val;
  char *name = bfd_get_filename (oso_bfd);
  int stabs_size;
  struct oso_data_cleanup *oso_cleanup;
  static unsigned char *empty_stabs_data = NULL;


  /* Must be an empty .o file, return 0 for all the sizes.
     We also set the stab_data_handle to point at an empty
     string, that way stab_seek will not mess up the file
     pointer in the file.  */

  if (!stabsect || ! stabstrsect)
    {
      complaint (&symfile_complaints, 
      "Couldn't find namelist for %s.\n",
		 name ? name : "<Unknown>");

      if (empty_stabs_data == NULL)
	{
	  empty_stabs_data = (unsigned char *) xmalloc (1);
	  *empty_stabs_data = '\0';
	}
      
      *symcount = 0;
      *symsize = 0;
      *strtab_size = 0;
      *stab_data_handle = empty_stabs_data;
      *strtab_data_handle = NULL;
      return make_cleanup (null_cleanup, 0);

    }

  oso_cleanup 
    = (struct oso_data_cleanup *) malloc (sizeof (struct oso_data_cleanup));

  oso_cleanup->stab_data_handle = stab_data_handle;
  oso_cleanup->strtab_data_handle = strtab_data_handle;

    *symsize =
    (bfd_mach_o_version (oso_bfd) > 1) ? 16 : 12;
  stabs_size = bfd_section_size (oso_bfd, stabsect); 
  *symcount = stabs_size / (*symsize);
    *strtab_size = bfd_section_size (oso_bfd, stabstrsect);

#if HAVE_MMAP
    /* currently breaks mapped symbol files (string table doesn't end up in objfile) */
    

    bfd_init_window (&(oso_cleanup->stab_window));
    
    /* APPLE LOCAL: Open the string table read only if possible.  Should
       be more efficient.  */
    
    val = bfd_get_section_contents_in_window_with_mode
      (oso_bfd, stabstrsect, &(oso_cleanup->stab_window), 0, *strtab_size, 0);
    
    if (!val)
      perror_with_name (name);
    
    *strtab_data_handle = oso_cleanup->stab_window.data;
    
    bfd_init_window (&(oso_cleanup->strtab_window));
    
    val = bfd_get_section_contents_in_window_with_mode
      (oso_bfd, stabsect, &(oso_cleanup->strtab_window), 0, stabs_size, 0);
    
    if (!val)
      perror_with_name (name);
    
    *stab_data_handle = oso_cleanup->strtab_window.data;
#else
    if (strtab_size > bfd_get_size (oso_bfd))
      error
	("error parsing symbol file: invalid string table size (%d bytes)",
	 strtab_size);
    *strtab_data_handle =
      (char *) malloc (strtab_size + 1);
    
    /* Now read in the string table in one big gulp.  */
    
    val = bfd_get_section_contents
      (oso_bfd, stabstrsect, *strtab_data_handle, 0,
       strtab_size);
    
    if (!val)
      perror_with_name (name);
    
    *stabs_data_handle =
      (char *) malloc (stabs_size + 1);
    
    /* Now read in the string table in one big gulp.  */
    
    val = bfd_get_section_contents
      (oso_bfd, stabsect, *stabs_data_handle, 0,
       stabs_size);
    
    if (!val)
      perror_with_name (name);
#endif
    return make_cleanup (cleanup_oso_data, oso_cleanup);
}

/* Read in all of the symbols for a given psymtab for real.
   Be verbose about it if the user wants that.  */

static void
dbx_psymtab_to_symtab (struct partial_symtab *pst)
{
  bfd *sym_bfd;
  struct cleanup *back_to = NULL;

  if (!pst)
    return;

  if (pst->readin)
    {
      fprintf_unfiltered (gdb_stderr, "Psymtab for %s already read in.  Shouldn't happen.\n",
			  pst->filename);
      return;
    }

  if (LDSYMLEN (pst) || pst->number_of_dependencies)
    {
      /* Print the message now, before reading the string table,
         to avoid disconcerting pauses.  */
      if (info_verbose)
	{
	  printf_filtered ("Reading in symbols for %s...", pst->filename);
	  gdb_flush (gdb_stdout);
	}

      sym_bfd = pst->objfile->obfd;

      next_symbol_text_func = dbx_next_symbol_text;

      if (DBX_STAB_SECTION (pst->objfile))
	{
	  stabs_data
	    = symfile_relocate_debug_section (pst->objfile->obfd,
					      DBX_STAB_SECTION (pst->objfile),
					      NULL);
	  if (stabs_data)
	    back_to = make_cleanup (free_current_contents, (void *) &stabs_data);
	}

      dbx_psymtab_to_symtab_1 (pst);

      if (back_to)
	do_cleanups (back_to);

      /* Match with global symbols.  This only needs to be done once,
         after all of the symtabs and dependencies have been read in.   */
      scan_file_globals (pst->objfile);

      /* Finish up the debug error message.  */
      if (info_verbose)
	printf_filtered ("done.\n");
    }
}

/* Read in a defined section of a specific object file's symbols. */

static void
read_ofile_symtab (struct partial_symtab *pst)
{
  /* APPLE LOCAL */
  char *namestring = NULL;
  struct internal_nlist nlist;
  int sect_p; /* APPLE LOCAL - need this for N_SECT  */
  unsigned char type;
  unsigned max_symnum;
  bfd *abfd;
  struct objfile *objfile;
  int sym_offset;		/* Offset to start of symbols to read */
  int sym_size;			/* Size of symbols to read */
  CORE_ADDR text_offset;	/* Start of text segment for symbols */
  int text_size;		/* Size of text segment for symbols */
  struct section_offsets *section_offsets;
  /* APPLE LOCAL symbol prefixes */
  const char *prefix;

  objfile = pst->objfile;
  sym_offset = LDSYMOFF (pst);
  sym_size = LDSYMLEN (pst);
  text_offset = pst->textlow;
  text_size = pst->texthigh - pst->textlow;
  /* This cannot be simply objfile->section_offsets because of
     elfstab_offset_sections() which initializes the psymtab section
     offsets information in a special way, and that is different from
     objfile->section_offsets. */ 
  section_offsets = pst->section_offsets;
  /* APPLE LOCAL symbol prefixes */
  prefix = SYMBOL_PREFIX (pst);

  current_objfile = objfile;
  subfile_stack = NULL;

  stringtab_global = DBX_STRINGTAB (objfile);
  last_source_file = NULL;

  abfd = objfile->obfd;
  /* APPLE LOCAL huh? */
  processing_objfile = objfile;	/* Implicit param to next_text_symbol */
  symbuf_end = symbuf_idx = 0;
  symbuf_read = 0;
  symbuf_left = sym_offset + sym_size;

  /* It is necessary to actually read one symbol *before* the start
     of this symtab's symbols, because the GCC_COMPILED_FLAG_SYMBOL
     occurs before the N_SO symbol.

     Detecting this in read_dbx_symtab
     would slow down initial readin, so we look for it here instead.  */
  if (!processing_acc_compilation && sym_offset >= (int) symbol_size)
    {
      stabs_seek (sym_offset - symbol_size);
      NEXT_SYMBOL (nlist, sect_p, abfd);
      OBJSTAT (objfile, n_stabs++);

      /* APPLE LOCAL symbol prefixes */
      namestring = set_namestring (objfile, nlist, prefix);

      processing_gcc_compilation = 0;
      if (nlist.n_type == N_TEXT)
	{
	  const char *tempstring = namestring;

	  if (strcmp (namestring, GCC_COMPILED_FLAG_SYMBOL) == 0)
	    processing_gcc_compilation = 1;
	  else if (strcmp (namestring, GCC2_COMPILED_FLAG_SYMBOL) == 0)
	    processing_gcc_compilation = 2;
	  if (tempstring[0] == bfd_get_symbol_leading_char (processing_objfile->obfd))
	    ++tempstring;
	  if (strncmp (tempstring, "__gnu_compiled", 14) == 0)
	    processing_gcc_compilation = 2;
	}

      /* Try to select a C++ demangling based on the compilation unit
         producer. */

#if 0
      /* For now, stay with AUTO_DEMANGLING for g++ output, as we don't
	 know whether it will use the old style or v3 mangling.  */
      if (processing_gcc_compilation)
	{
	  if (AUTO_DEMANGLING)
	    {
	      set_demangling_style (GNU_DEMANGLING_STYLE_STRING);
	    }
	}
#endif
    }
  else
    {
      /* The N_SO starting this symtab is the first symbol, so we
         better not check the symbol before it.  I'm not this can
         happen, but it doesn't hurt to check for it.  */
      stabs_seek (sym_offset);
      processing_gcc_compilation = 0;
    }

  /* APPLE LOCAL huh? */
  PEEK_SYMBOL (nlist, sect_p, abfd);
  if (nlist.n_type != N_SO)
    error (_("First symbol in segment of executable not a source symbol"));

  max_symnum = sym_size / symbol_size;

  for (symnum = 0;
       symnum < max_symnum;
       symnum++)
    {
      QUIT;			/* Allow this to be interruptable */

      NEXT_SYMBOL (nlist, sect_p, abfd);
      OBJSTAT (objfile, n_stabs++);

      type = nlist.n_type;
      /* APPLE LOCAL symbol prefixes */
      namestring = set_namestring (objfile, nlist, prefix);

      if (type & N_STAB)
	{
	  process_one_symbol (type, nlist.n_desc, nlist.n_value,
			      namestring, prefix, section_offsets, objfile);
	}
      /* We skip checking for a new .o or -l file; that should never
         happen in this routine. */
      else if (type == N_TEXT)
	{
	  /* I don't think this code will ever be executed, because
	     the GCC_COMPILED_FLAG_SYMBOL usually is right before
	     the N_SO symbol which starts this source file.
	     However, there is no reason not to accept
	     the GCC_COMPILED_FLAG_SYMBOL anywhere.  */

	  if (strcmp (namestring, GCC_COMPILED_FLAG_SYMBOL) == 0)
	    processing_gcc_compilation = 1;
	  else if (strcmp (namestring, GCC2_COMPILED_FLAG_SYMBOL) == 0)
	    processing_gcc_compilation = 2;

#if 0
	  /* For now, stay with AUTO_DEMANGLING for g++ output, as we don't
	     know whether it will use the old style or v3 mangling.  */
	  if (AUTO_DEMANGLING)
	    {
	      set_demangling_style (GNU_DEMANGLING_STYLE_STRING);
	    }
#endif
	}
      else if (type & N_EXT || type == (unsigned char) N_TEXT
	       || type == (unsigned char) N_NBTEXT
	)
	{
	  /* Global symbol: see if we came across a dbx defintion for
	     a corresponding symbol.  If so, store the value.  Remove
	     syms from the chain when their values are stored, but
	     search the whole chain, as there may be several syms from
	     different files with the same name. */
	  /* This is probably not true.  Since the files will be read
	     in one at a time, each reference to a global symbol will
	     be satisfied in each file as it appears. So we skip this
	     section. */
	  ;
	}
    }

  current_objfile = NULL;

  /* In a Solaris elf file, this variable, which comes from the
     value of the N_SO symbol, will still be 0.  Luckily, text_offset,
     which comes from pst->textlow is correct. */
  if (last_source_start_addr == 0)
    last_source_start_addr = text_offset;

  /* In reordered executables last_source_start_addr may not be the
     lower bound for this symtab, instead use text_offset which comes
     from pst->textlow which is correct.  */
  if (last_source_start_addr > text_offset)
    last_source_start_addr = text_offset;

  pst->symtab = end_symtab (text_offset + text_size, objfile, SECT_OFF_TEXT (objfile));

  end_stabs ();
}

/* APPLE LOCAL: The following section is to support reading debug
   info from the .o file */

/* This routine reads the function/static name from NAMESTRING,
   then looks it up in the partial_symtab PST.  It returns the
   partial_symbol, or NULL if it could not be found.  So far this
   only supports N_FUN, N_STSYM and N_LCSYM stabs.  If you want to
   add another type, you just need to figure out from the type
   specifier whether the lookup should be global or static.  */

static struct partial_symbol *
lookup_psymbol_from_namestring (struct partial_symtab *pst, 
				char *namestring,
				enum domain_enum_tag domain)
{
  struct partial_symbol *psym;
  char *name;
  int name_len;
  char *demangled;
  int free_demangled;
  int global;

  name = find_name_end (namestring);
  switch (name[1])
    {
    case 'F':
      global = 1;
      break;
    case 'S':
    case 'f':
    case 'V':
      global = 0;
      break;
    default:
      warning ("Unknown type passed to lookup_psymbol_from_namestring");
      return NULL;
      break;
    }
  name_len = name - namestring;
  name = alloca (name_len + 1);
  strncpy (name, namestring, name_len);
  name[name_len] = '\0';
  
  demangled = NULL;
  
  if (pst->language == language_cplus
      || pst->language == language_objcplus)
    {
      demangled =
	cplus_demangle (name, DMGL_PARAMS | DMGL_ANSI);
	    }
  
  if (demangled == NULL)
    {
      demangled = name;
      free_demangled = 0;
    }
  else 
    free_demangled = 1;
  
  psym = lookup_partial_symbol (pst, demangled, name, global, domain);

  if (!psym)
    complaint (&symfile_complaints, 
      "Couldn't find psymbol from namestring for: demangled: %s mangled: %s.\n",
	     demangled, name);

  if (free_demangled)
    xfree (demangled);
 
  return psym;
}

/* Pop the function list entry off the top of the function
   queue LIST_PTR.  Frees the element, and returns the 
   element that is now the top element.  */

static struct oso_fun_list *
oso_statics_pop (struct oso_fun_list *list_ptr)
{
  struct oso_fun_static *static_ptr;
  struct oso_fun_list *ret_ptr;
  static_ptr = list_ptr->statics;
  while (static_ptr != NULL)
    {
      struct oso_fun_static *tmp_ptr;
      tmp_ptr = static_ptr->next;
      xfree (static_ptr->name);
      xfree (static_ptr);
      static_ptr = tmp_ptr;
    }
  ret_ptr = list_ptr->next;
  xfree (list_ptr->name);
  xfree (list_ptr);
  return ret_ptr;
}

/* APPLE LOCAL: Read in the full stabs data from the .o file
   corresponding to PST and OSO_BFD.  I doubt this will work for
   any of the stabs readers but mach-o.  I haven't made any attempt
   to support stabs in ELF or whatever.  */


static void
read_ofile_symtab_from_oso (struct partial_symtab *pst, struct bfd *oso_bfd)
{
  char *namestring = NULL;
  struct internal_nlist nlist;
  int sect_p;
  unsigned char type;
  bfd *abfd;
  struct objfile *objfile;
  int sym_offset;		/* Offset to start of symbols to read */
  int sym_size;			/* Size of symbols to read */
  int num_syms;
  int strtab_size;
  char *strtab_data;
  CORE_ADDR text_offset;	/* Start of text segment for symbols */
  int text_size;		/* Size of text segment for symbols */
  struct section_offsets *section_offsets;
  const char *prefix;
  char leading_char;
  struct cleanup *oso_data_cleanup;
  char *cur_fun_name = NULL;
  
  objfile = pst->objfile;

  leading_char = bfd_get_symbol_leading_char (oso_bfd);

  /* Read the nlist data into stabs_data, and read the string_table into strtab_ptr.  */
  oso_data_cleanup = stabsect_read_strtab_from_oso (oso_bfd, &num_syms, &sym_size, &strtab_size, 
				 &stabs_data, &strtab_data);
  
  text_offset = pst->textlow;
  text_size = pst->texthigh - pst->textlow;
  /* This cannot be simply objfile->section_offsets because of
     elfstab_offset_sections() which initializes the psymtab section
     offsets information in a special way, and that is different from
     objfile->section_offsets. */ 
  section_offsets = pst->section_offsets;
  prefix = SYMBOL_PREFIX (pst);

  current_objfile = objfile;
  subfile_stack = NULL;

  stringtab_global = (char *) stabs_data;
  last_source_file = NULL;

  abfd = oso_bfd;
  processing_objfile = objfile;	/* Implicit param to next_text_symbol */

  sym_offset = LDSYMOFF (pst);
  symbuf_end = symbuf_idx = 0;
  symbuf_read = 0;
  symbuf_left = sym_offset + num_syms * sym_size;

  stabs_seek (sym_offset);

  symnum = symbuf_idx;

  processing_gcc_compilation = 0;

  PEEK_SYMBOL (nlist, sect_p, oso_bfd);
  if (nlist.n_type != N_SO)
    error ("First symbol in segment of executable not a source symbol");

  for (;
       symnum < num_syms;
       symnum++)
    {
      CORE_ADDR offset;

      QUIT;			/* Allow this to be interruptable */

      NEXT_SYMBOL (nlist, sect_p, oso_bfd);
      /* OBJSTAT (objfile, n_stabs++); */

      type = nlist.n_type;
      namestring = set_namestring_1 (strtab_size, strtab_data, 
				     leading_char, nlist, prefix);

      /* Here's where we scan ahead and look for the next N_FUN stab,
	 then we match that with the partial_symbol we made from the
	 debug info the linker copied into the linked image, and that
	 will give us the offset we will apply till we hit the next 
	 BNSYM.  */

      /* FIXME: I am explicitly ONLY dealing with the stabs_data case here, and
	 assuming you are reading directly from stabs_data.
	 If you were going to use the symbuf, you would have to be more careful when 
	 you cross symbuf windows.  */

      if (type == N_BNSYM)
	{
	  struct internal_nlist tmp_nlist;
	  struct partial_symbol *fun_psym;
	  char *fun_namestring;
	  int scan_ptr = symnum;
	  int old_symbuf_idx = symbuf_idx;
	  int found_it = 0;

	  while (scan_ptr < num_syms)
	    {
	      NEXT_SYMBOL (tmp_nlist, sect_p, oso_bfd);
	      if (tmp_nlist.n_type == N_FUN)
		{
		  found_it = 1;
		  break;
		}
	      if (tmp_nlist.n_type == N_ENSYM)
		{
		  found_it = 0;
		  break;
		}
	      scan_ptr++;
	    }
	  if (!found_it)
	    {
	      complaint (&symfile_complaints, "Scan to end from BNSYM without finding FUN");
	    }
	  else
	    {

	      /* Now look up the partial symbol for this FUN.  */
	      fun_namestring = set_namestring_1 (strtab_size, strtab_data, 
						 leading_char, tmp_nlist, prefix);
	      
	      if (fun_namestring[0] == '\0')
		{
		  complaint (&symfile_complaints, "Found NULL FUN stab scanning for function.");
		  fun_psym = NULL;
		}
	      else
		{
		  fun_psym = lookup_psymbol_from_namestring (pst, fun_namestring,
							     VAR_DOMAIN);
		}
	    }

	  if (fun_psym == NULL)
	    {
	      /* Presumably, this function got dead-code stripped, so we should skip
		 all the stabs in this BNSYM/ENSYM block.  But we had better
		 scan them for types or we may throw away some type info we will need
		 for other stabs from this N_SO unit.  So rewind back to the BNSYM,
	         and just read in the types from the stabs that are likely to contain
	         them.  */
	      found_it = 0;
	      symbuf_idx = old_symbuf_idx;
	      while (symnum < num_syms)
		{
		  char *tmp_namestring;
		  NEXT_SYMBOL (tmp_nlist, sect_p, oso_bfd);
		  if (tmp_nlist.n_type == N_ENSYM) 
		    {
		      found_it = 1;
		      break;
		    }
		  switch (tmp_nlist.n_type)
		    {
		      /* FIXME: When gcc only emits types in N_LSYM stabs, we can
			 change this to only pass the N_LSYM stabs in, and ignore
			 all the others.  */
		    case N_FUN:
		    case N_LSYM:
		    case N_STSYM:
		    case N_LCSYM:
		    case N_GSYM:
		    case N_PSYM:
		    case N_RSYM:
		      tmp_namestring = set_namestring_1 (strtab_size, strtab_data, 
						 leading_char, tmp_nlist, prefix);
		      process_symbol_types_only (tmp_namestring, prefix, tmp_nlist.n_desc, 
						 tmp_nlist.n_type, objfile);
		      break;
		    default:
		      break;
		    }
		    
		  symnum++;
		}
	      if (!found_it)
		complaint (&symfile_complaints, "Missing ENSYM in scanning OSO file");
	    }
	  else
	    {
	      /* Calculate the offset from the partial_symbol then
		 rewind back to the stab after the BNSYM.  Note, since
	         the addresses we get from the pst are already offset
	         by the load address, but process_one_symbol expects to
		 have to add it in, we have to subtract the load address from 
	         our offset or we'll add it twice.  */

	      offset = SYMBOL_VALUE_ADDRESS (fun_psym) - nlist.n_value 
		- objfile_text_section_offset (objfile);

	      symbuf_idx = old_symbuf_idx;
	    }
	  continue;
	}

      /* Only process N_STAB types here, since all the real text &
	 data symbols are already in the correct place from the linked
	 image.  */
      if (type & N_STAB)
	{
	  switch (type)
	    {
	    case N_FUN:
	      if (*namestring == '\0')
		{
		  /* POP the function from the stack of
		     function statics. */
		  struct oso_fun_list *list_ptr;
		  list_ptr = PSYMTAB_OSO_STATICS(pst);
		  if (list_ptr)
		    {
		      if (list_ptr->name == NULL)
			warning ("OSO fun missing name for pst: %s", pst->filename);
		      else if (!cur_fun_name)
			warning ("Current function name not found for pst: \"%s\" and fun: \"%s\"", pst->filename, list_ptr->name);
		      else if (strcmp (list_ptr->name, cur_fun_name) == 0)
			PSYMTAB_OSO_STATICS(pst) = oso_statics_pop (list_ptr);
		    }
		  if (cur_fun_name)
		    xfree (cur_fun_name);
		  cur_fun_name = NULL;
		  break;
		}
	      else
		{
		  char *p = find_name_end (namestring);
		  int namelen = p - namestring;
		  if (cur_fun_name != NULL)
		    xfree (cur_fun_name);
		  cur_fun_name = xmalloc (namelen + 1);
		  strncpy (cur_fun_name, namestring, namelen);
		  cur_fun_name[namelen] = '\0';
		}
	    case N_SO:
	    case N_RBRAC:
	    case N_LBRAC:
	    case N_SLINE:
	      nlist.n_value += offset;
	      break;
	    case N_LCSYM:
	    case N_ROSYM:
	    case N_STSYM:
	      {
		char *p;
		p = find_name_end (namestring);
		if (p[1] == 'V')
		  {
		    /* This is a static, so we need to look it up in our table.  */
		    struct oso_fun_list *statics_list;
		    struct oso_fun_static *static_ptr, *prev_static_ptr;
		    int namelen = p - namestring;

		    if (cur_fun_name == NULL)
		      {
			warning ("NULL current function name"
				 " while processing: \"%s\"", namestring);
		      }
		    else
		      {
			for (statics_list = PSYMTAB_OSO_STATICS (pst);
			     statics_list != NULL; statics_list = statics_list->next)
			  {
			    if (statics_list->name == NULL)
			      continue;
			    
			    if (strcmp (statics_list->name, cur_fun_name) == 0)
			      break;
			  }
			if (statics_list == NULL)
			  {
			    warning ("Could not find statics info for function: %s",
				     cur_fun_name);
			  }
			else
			  {			
			    for (static_ptr = statics_list->statics, prev_static_ptr = static_ptr; 
				 static_ptr != NULL;
				 prev_static_ptr = static_ptr, static_ptr = static_ptr->next)
			      {
				if (strncmp(static_ptr->name, namestring, namelen) == 0)
				  break;
			      }
			    if (static_ptr == NULL)
			      {
				char *symname;
				symname = xmalloc (namelen + 1);
				strncpy (symname, namestring, namelen);
				symname[namelen] = '\0';
				warning ("Could not find statics info for symbol: %s in function: %s",
					 symname, cur_fun_name);
				xfree (symname);
			      }
			    else
			      {
				nlist.n_value = static_ptr->address;
				
				/* We're tearing down the whole statics
				   structure as we process the symbols.
				   This is okay because we know the symbols will
				   appear in the same order in the .o and the
				   final linked image, and everythign that appears
				   in the final linked image MUST appear here.
				   We are only ever going to do this once,
				   so this makes working through it much
				   faster.  */
				if (statics_list->statics == prev_static_ptr)
				  statics_list->statics = prev_static_ptr->next;
				else
				  prev_static_ptr->next = static_ptr->next;
				
				xfree (static_ptr->name);
				xfree (static_ptr);
			      }
			  }
		      }
		  }
		else
		  {
		    /* These are globals, so we can just look up the
		       partial symbol for them.  */
		    struct partial_symbol *static_psym;
		    static_psym = lookup_psymbol_from_namestring (pst, namestring,
								  VAR_DOMAIN);
		    if (static_psym)
		      nlist.n_value = SYMBOL_VALUE_ADDRESS (static_psym) 
				      - objfile_data_section_offset (objfile);
		  }
		break; 
	      }
	    default:
	      break;
	    }
	  /* This is a big hack.  process_one_symbol will call
	     end_symtab when it sees the final SO.  This isn't a
	     problem for read_ofile_symtab because when we read in the
	     original psymtab, we cook the LSYMLEN to exclude this
	     last SO.  */
	  if (type == N_SO
	      && last_source_file != NULL
	      && previous_stab_code != (unsigned char) N_SO
	      && *namestring == '\0')
	    break;

	  process_one_symbol (type, nlist.n_desc, nlist.n_value,
			      namestring, prefix, section_offsets, objfile);
	}
    }

  current_objfile = NULL;

  /* In a Solaris elf file, this variable, which comes from the
     value of the N_SO symbol, will still be 0.  Luckily, text_offset,
     which comes from pst->textlow is correct. */
  if (last_source_start_addr == 0)
    last_source_start_addr = text_offset;

  /* In reordered executables last_source_start_addr may not be the
     lower bound for this symtab, instead use text_offset which comes
     from pst->textlow which is correct.  */
  if (last_source_start_addr > text_offset)
    last_source_start_addr = text_offset;

  pst->symtab = end_symtab (text_offset + text_size, objfile, SECT_OFF_TEXT (objfile));

  end_stabs ();

  if (cur_fun_name != NULL)
    xfree (cur_fun_name);

  do_cleanups (oso_data_cleanup);
}


/* This handles a single symbol from the symbol-file, building symbols
   into a GDB symtab.  It takes these arguments and an implicit argument.

   TYPE is the type field of the ".stab" symbol entry.
   DESC is the desc field of the ".stab" entry.
   VALU is the value field of the ".stab" entry.
   NAME is the symbol name, in our address space.
   SECTION_OFFSETS is a set of amounts by which the sections of this
   object file were relocated when it was loaded into memory.  Note
   that these section_offsets are not the objfile->section_offsets but
   the pst->section_offsets.  All symbols that refer to memory
   locations need to be offset by these amounts.
   OBJFILE is the object file from which we are reading symbols.  It
   is used in end_symtab.  */

void
process_one_symbol (int type, int desc, CORE_ADDR valu, char *name,
		    const char *prefix,
                    struct section_offsets *section_offsets,
		    struct objfile *objfile)
{
  /* APPLE LOCAL huh? */
  struct context_stack *new = NULL;
  /* This remembers the address of the start of a function.  It is
     used because in Solaris 2, N_LBRAC, N_RBRAC, and N_SLINE entries
     are relative to the current function's start address.  On systems
     other than Solaris 2, this just holds the SECT_OFF_TEXT value,
     and is used to relocate these symbol types rather than
     SECTION_OFFSETS.  */
  static CORE_ADDR function_start_offset;

  /* This holds the address of the start of a function, without the
     system peculiarities of function_start_offset.  */
  static CORE_ADDR last_function_start;

  /* If this is nonzero, we've seen an N_SLINE since the start of the
     current function.  We use this to tell us to move the first sline
     to the beginning of the function regardless of what its given
     value is. */
  /* Initialize to nonzero to assure that last_function_start
     is never used uninitialized.  */

  static int sline_found_in_function = 1;

  /* If this is nonzero, we've seen a non-gcc N_OPT symbol for this
     source file.  Used to detect the SunPRO solaris compiler.  */
  static int n_opt_found;

  /* The stab type used for the definition of the last function.
     N_STSYM or N_GSYM for SunOS4 acc; N_FUN for other compilers.  */
  static int function_stab_type = 0;

  /* Track that we have matched N_FUN pairs bracketing functions.  Some
     linkers leave the N_FUN end symbol sitting around in the code when
     they coalesce C++ templates, and we have to ignore this or we will
     crash trying to close blocks that aren't open.
     It would be great if we could use within_function for this, but
     that is actually set to 0 when we hit the closing RBRAC of the
     function, not when we see the closing FUN stab.  */
  static int saw_fun_start = 0;

  if (!block_address_function_relative)
    {
      /* N_LBRAC, N_RBRAC and N_SLINE entries are not relative to the
	 function start address, so just use the text offset.  */
      function_start_offset =
	ANOFFSET (section_offsets, SECT_OFF_TEXT (objfile));
    }

  /* Something is wrong if we see real data before seeing a source
     file name.  */

  if (last_source_file == NULL && type != (unsigned char) N_SO)
    {
      /* Ignore any symbols which appear before an N_SO symbol.
         Currently no one puts symbols there, but we should deal
         gracefully with the case.  A complain()t might be in order,
         but this should not be an error ().  */
      return;
    }

  switch (type)
    {
    case N_FUN:
    case N_FNAME:

      if (*name == '\000')
	{
	  CORE_ADDR valu_abs;

	  /* This N_FUN marks the end of a function.  This closes off
	     the current block.  */

	  /* Some linkers leave FUN end stabs lying around with no
            corresponding START stabswhen they coalesce symbols.  Just
            ignore them. */

 	  if (context_stack_depth <= 0)
 	    {
	      lbrac_mismatch_complaint (symnum);
 	      break;
 	    }

	  /* APPLE LOCAL */
	  if (!saw_fun_start)
	    {
	      complaint (&symfile_complaints, 
			 "Found an end function stab with no corresponding begin");
	      break;
	    }
	  else
	    saw_fun_start = 0;
	  /* APPLE LOCAL END */


	  /* The following check is added before recording line 0 at
	     end of function so as to handle hand-generated stabs
	     which may have an N_FUN stabs at the end of the function,
	     but no N_SLINE stabs.  */
	  if (sline_found_in_function)
	    {
	      /* APPLE LOCAL begin subroutine inlining  */
	      if (processing_gcc_compilation || !end_fun_absolute_p)
		record_line (current_subfile, 0, last_function_start + valu, 0,
			     NORMAL_LT_ENTRY);
	      else
		record_line (current_subfile, 0, valu, 0, NORMAL_LT_ENTRY);
	      /* APPLE LOCAL end subroutine inlining  */
	    }

	  within_function = 0;
	  new = pop_context ();
	  
	  if (processing_gcc_compilation || !end_fun_absolute_p)
	    valu_abs = new->start_addr + valu;
	  else
	    {
	      /* Relocate for dynamic loading */
	      valu_abs = valu;
	      valu_abs += ANOFFSET (section_offsets, SECT_OFF_TEXT (objfile));
	      valu_abs = SMASH_TEXT_ADDRESS (valu_abs);
	    }

	  /* Make a block for the local symbols within.  */
	  /* APPLE LOCAL begin address ranges  */
	  finish_block (new->name, &local_symbols, new->old_blocks,
			new->start_addr, valu_abs, NULL,
			objfile);
	  /* APPLE LOCAL end address ranges  */

	  /* May be switching to an assembler file which may not be using
	     block relative stabs, so reset the offset.  */
	  if (block_address_function_relative)
	    function_start_offset = 0;

	  break;
	}

      sline_found_in_function = 0;

      /* Relocate for dynamic loading.  */
      valu += ANOFFSET (section_offsets, SECT_OFF_TEXT (objfile));
      valu = SMASH_TEXT_ADDRESS (valu);
      last_function_start = valu;

      goto define_a_symbol;

    case N_LBRAC:
      /* This "symbol" just indicates the start of an inner lexical
         context within a function.  */

      /* Ignore extra outermost context from SunPRO cc and acc.  */
      if (n_opt_found && desc == 1)
	break;

#if defined(BLOCK_ADDRESS_ABSOLUTE)
      /* Relocate for dynamic loading (?).  */
      valu += function_start_offset;
#else
      if (block_address_function_relative)
	/* Relocate for Sun ELF acc fn-relative syms.  */
	valu += function_start_offset;
      else
	/* On most machines, the block addresses are relative to the
	   N_SO, the linker did not relocate them (sigh).  */
	valu += last_source_start_addr;
#endif

      new = push_context (desc, valu);
      break;

    case N_RBRAC:
      /* This "symbol" just indicates the end of an inner lexical
         context that was started with N_LBRAC.  */

      /* Ignore extra outermost context from SunPRO cc and acc.  */
      if (n_opt_found && desc == 1)
	break;

#if defined(BLOCK_ADDRESS_ABSOLUTE)
      /* Relocate for dynamic loading (?).  */
      valu += function_start_offset;
#else
      if (block_address_function_relative)
	/* Relocate for Sun ELF acc fn-relative syms.  */
	valu += function_start_offset;
      else
	/* On most machines, the block addresses are relative to the
	   N_SO, the linker did not relocate them (sigh).  */
	valu += last_source_start_addr;
#endif

      if (context_stack_depth <= 0)
	{
	  lbrac_mismatch_complaint (symnum);
	  break;
	}

      new = pop_context ();
      if (desc != new->depth)
	lbrac_mismatch_complaint (symnum);

      /* Some compilers put the variable decls inside of an
         LBRAC/RBRAC block.  This macro should be nonzero if this is
         true.  DESC is N_DESC from the N_RBRAC symbol.  GCC_P is true
         if we've detected the GCC_COMPILED_SYMBOL or the
         GCC2_COMPILED_SYMBOL.  */
#if !defined (VARIABLES_INSIDE_BLOCK)
#define VARIABLES_INSIDE_BLOCK(desc, gcc_p) 0
#endif

      /* Can only use new->locals as local symbols here if we're in
         GCC or on a machine that puts them before the lbrack.  */
      if (!VARIABLES_INSIDE_BLOCK (desc, processing_gcc_compilation))
	{
	  if (local_symbols != NULL)
	    {
	      /* GCC development snapshots from March to December of
		 2000 would output N_LSYM entries after N_LBRAC
		 entries.  As a consequence, these symbols are simply
		 discarded.  Complain if this is the case.  Note that
		 there are some compilers which legitimately put local
		 symbols within an LBRAC/RBRAC block; this complaint
		 might also help sort out problems in which
		 VARIABLES_INSIDE_BLOCK is incorrectly defined.  */
	      complaint (&symfile_complaints, _("\
misplaced N_LBRAC entry; discarding local symbols which have \
no enclosing block"));
	    }
	  local_symbols = new->locals;
	}

      if (context_stack_depth
	  > !VARIABLES_INSIDE_BLOCK (desc, processing_gcc_compilation))
	{
	  /* This is not the outermost LBRAC...RBRAC pair in the
	     function, its local symbols preceded it, and are the ones
	     just recovered from the context stack.  Define the block
	     for them (but don't bother if the block contains no
	     symbols.  Should we complain on blocks without symbols?
	     I can't think of any useful purpose for them).  */
	  if (local_symbols != NULL)
	    {
	      /* Muzzle a compiler bug that makes end < start.

		 ??? Which compilers?  Is this ever harmful?.  */
	      if (new->start_addr > valu)
		{
		  complaint (&symfile_complaints,
			     _("block start larger than block end"));
		  new->start_addr = valu;
		}
	      /* Make a block for the local symbols within.  */
	      /* APPLE LOCAL begin address ranges  */
	      finish_block (0, &local_symbols, new->old_blocks,
			    new->start_addr, valu, NULL, objfile);
	      /* APPLE LOCAL end address ranges  */
	    }
	}
      else
	{
	  /* This is the outermost LBRAC...RBRAC pair.  There is no
	     need to do anything; leave the symbols that preceded it
	     to be attached to the function's own block.  We need to
	     indicate that we just moved outside of the function.  */
	  within_function = 0;
	}

      if (VARIABLES_INSIDE_BLOCK (desc, processing_gcc_compilation))
	/* Now pop locals of block just finished.  */
	local_symbols = new->locals;
      break;

    case N_FN:
    case N_FN_SEQ:
      /* This kind of symbol indicates the start of an object file.
         Relocate for dynamic loading.  */
      valu += ANOFFSET (section_offsets, SECT_OFF_TEXT (objfile));
      break;

    case N_SO:
      /* This type of symbol indicates the start of data for one
         source file.  Finish the symbol table of the previous source
         file (if any) and start accumulating a new symbol table.
         Relocate for dynamic loading.  */
      valu += ANOFFSET (section_offsets, SECT_OFF_TEXT (objfile));

      n_opt_found = 0;

      if (last_source_file)
	{
	  /* Check if previous symbol was also an N_SO (with some
	     sanity checks).  If so, that one was actually the
	     directory name, and the current one is the real file
	     name.  Patch things up. */
	  if (previous_stab_code == (unsigned char) N_SO)
	    {
	      patch_subfile_names (current_subfile, name);
              /* APPLE LOCAL: Set the language if the SO stab includes it.  */
              if (read_so_stab_language_hint (desc) != language_unknown)
                current_subfile->language = read_so_stab_language_hint (desc);
	      break;		/* Ignore repeated SOs */
	    }
	  end_symtab (valu, objfile, SECT_OFF_TEXT (objfile));
	  end_stabs ();
	}

      /* Null name means this just marks the end of text for this .o
         file.  Don't start a new symtab in this case.  */
      if (*name == '\000')
	break;

      if (block_address_function_relative)
	function_start_offset = 0;

      start_stabs ();
      start_symtab (name, NULL, valu);
      record_debugformat ("stabs");
      break;

    /* APPLE LOCAL: Ignore N_OSO when re-scanning nlists to create a 
       symtab; we've already gleaned what we need from this stab.  */
    case N_OSO:
      break;

    case N_SOL:
      /* This type of symbol indicates the start of data for a
         sub-source-file, one whose contents were copied or included
         in the compilation of the main source file (whose name was
         given in the N_SO symbol).  Relocate for dynamic loading.  */
      valu += ANOFFSET (section_offsets, SECT_OFF_TEXT (objfile));
      start_subfile (name, current_subfile->dirname);
      break;

    case N_BINCL:
      push_subfile ();
      add_new_header_file (name, valu);
      start_subfile (name, current_subfile->dirname);
      break;

    case N_EINCL:
      start_subfile (pop_subfile (), current_subfile->dirname);
      break;

    case N_EXCL:
      add_old_header_file (name, valu);
      break;

    case N_SLINE:
      /* This type of "symbol" really just records one line-number --
         core-address correspondence.  Enter it in the line list for
         this symbol table.  */

      /* Relocate for dynamic loading and for ELF acc
         function-relative symbols.  */
      valu += function_start_offset;

      /* GCC 2.95.3 emits the first N_SLINE stab somwehere in the
	 middle of the prologue instead of right at the start of the
	 function.  To deal with this we record the address for the
	 first N_SLINE stab to be the start of the function instead of
	 the listed location.  We really shouldn't to this.  When
	 compiling with optimization, this first N_SLINE stab might be
	 optimized away.  Other (non-GCC) compilers don't emit this
	 stab at all.  There is no real harm in having an extra
	 numbered line, although it can be a bit annoying for the
	 user.  However, it totally screws up our testsuite.

	 So for now, keep adjusting the address of the first N_SLINE
	 stab, but only for code compiled with GCC.  */

      /* APPLE LOCAL begin subroutine inlining  */
      if (within_function && sline_found_in_function == 0)
	{
	  if (processing_gcc_compilation == 2)
	    record_line (current_subfile, desc, last_function_start, 0, 
			 NORMAL_LT_ENTRY);
	  else
	    record_line (current_subfile, desc, valu, 0, NORMAL_LT_ENTRY);
	  sline_found_in_function = 1;
	}
      else
	record_line (current_subfile, desc, valu, 0, NORMAL_LT_ENTRY);
      /* APPLE LOCAL end subroutine inlining  */
      break;

    case N_BCOMM:
      common_block_start (name, objfile);
      break;

    case N_ECOMM:
      common_block_end (objfile);
      break;

      /* The following symbol types need to have the appropriate
         offset added to their value; then we process symbol
         definitions in the name.  */

    case N_STSYM:		/* Static symbol in data segment.  */
    case N_LCSYM:		/* Static symbol in BSS segment.  */
    case N_ROSYM:		/* Static symbol in read-only data segment.  */
      /* HORRID HACK DEPT.  However, it's Sun's furgin' fault.
         Solaris 2's stabs-in-elf makes *most* symbols relative but
         leaves a few absolute (at least for Solaris 2.1 and version
         2.0.1 of the SunPRO compiler).  N_STSYM and friends sit on
         the fence.  .stab "foo:S...",N_STSYM is absolute (ld
         relocates it) .stab "foo:V...",N_STSYM is relative (section
         base subtracted).  This leaves us no choice but to search for
         the 'S' or 'V'...  (or pass the whole section_offsets stuff
         down ONE MORE function call level, which we really don't want
         to do).  */
      {
	char *p;

	/* Normal object file and NLMs have non-zero text seg offsets,
	   but don't need their static syms offset in this fashion.
	   XXX - This is really a crock that should be fixed in the
	   solib handling code so that I don't have to work around it
	   here.  */

	if (!symfile_relocatable)
	  {
	    p = find_name_end (name);
	    if (p != 0 && p[1] == 'S')
	      {
		/* The linker relocated it.  We don't want to add an
		   elfstab_offset_sections-type offset, but we *do*
		   want to add whatever solib.c passed to
		   symbol_file_add as addr (this is known to affect
		   SunOS 4, and I suspect ELF too).  Since
		   elfstab_offset_sections currently does not muck
		   with the text offset (there is no Ttext.text
		   symbol), we can get addr from the text offset.  If
		   elfstab_offset_sections ever starts dealing with
		   the text offset, and we still need to do this, we
		   need to invent a SECT_OFF_ADDR_KLUDGE or something.  */
		valu += ANOFFSET (section_offsets, SECT_OFF_TEXT (objfile));
		goto define_a_symbol;
	      }
	  }
	/* Since it's not the kludge case, re-dispatch to the right
           handler.  */
	switch (type)
	  {
	  case N_STSYM:
	    goto case_N_STSYM;
	  case N_LCSYM:
	    goto case_N_LCSYM;
	  case N_ROSYM:
	    goto case_N_ROSYM;
	  default:
	    internal_error (__FILE__, __LINE__,
			    _("failed internal consistency check"));
	  }
      }

    case_N_STSYM:		/* Static symbol in data segment.  */
    case N_DSLINE:		/* Source line number, data segment.  */
      valu += ANOFFSET (section_offsets, SECT_OFF_DATA (objfile));
      goto define_a_symbol;

    case_N_LCSYM:		/* Static symbol in BSS segment.  */
    case N_BSLINE:		/* Source line number, BSS segment.  */
      /* N_BROWS: overlaps with N_BSLINE.  */
      valu += ANOFFSET (section_offsets, SECT_OFF_BSS (objfile));
      goto define_a_symbol;

    case_N_ROSYM:		/* Static symbol in read-only data segment.  */
      valu += ANOFFSET (section_offsets, SECT_OFF_RODATA (objfile));
      goto define_a_symbol;

    case N_ENTRY:		/* Alternate entry point.  */
      /* Relocate for dynamic loading.  */
      valu += ANOFFSET (section_offsets, SECT_OFF_TEXT (objfile));
      goto define_a_symbol;

    case N_ENSYM:               /* MacOS X - just for linker - ignore */
    case N_BNSYM:               /* MacOS X - just for linker - ignore */
      break;
      /* The following symbol types we don't know how to process.
         Handle them in a "default" way, but complain to people who
         care.  */
    default:
    case N_CATCH:		/* Exception handler catcher.  */
    case N_EHDECL:		/* Exception handler name.  */
    case N_PC:			/* Global symbol in Pascal.  */
    case N_M2C:			/* Modula-2 compilation unit.  */
      /* N_MOD2: overlaps with N_EHDECL.  */
    case N_SCOPE:		/* Modula-2 scope information.  */
    case N_ECOML:		/* End common (local name).  */
    case N_NBTEXT:		/* Gould Non-Base-Register symbols???  */
    case N_NBDATA:
    case N_NBBSS:
    case N_NBSTS:
    case N_NBLCS:
      unknown_symtype_complaint (hex_string (type));
      /* FALLTHROUGH */

      /* The following symbol types don't need the address field
         relocated, since it is either unused, or is absolute.  */
    define_a_symbol:
    case N_GSYM:		/* Global variable.  */
    case N_NSYMS:		/* Number of symbols (Ultrix).  */
    case N_NOMAP:		/* No map?  (Ultrix).  */
    case N_RSYM:		/* Register variable.  */
    case N_DEFD:		/* Modula-2 GNU module dependency.  */
    case N_SSYM:		/* Struct or union element.  */
    case N_LSYM:		/* Local symbol in stack.  */
    case N_PSYM:		/* Parameter variable.  */
    case N_LENG:		/* Length of preceding symbol type.  */
      if (name)
	{
	  int deftype;
	  char *colon_pos = find_name_end (name);
	  if (colon_pos == NULL)
	    deftype = '\0';
	  else
	    deftype = colon_pos[1];

	  switch (deftype)
	    {
	    case 'f':
	    case 'F':
	      function_stab_type = type;

#ifdef SOFUN_ADDRESS_MAYBE_MISSING
	      /* Deal with the SunPRO 3.0 compiler which omits the
	         address from N_FUN symbols.  */
	      if (type == N_FUN
		  && valu == ANOFFSET (section_offsets,
				       SECT_OFF_TEXT (objfile)))
		{
		  CORE_ADDR minsym_valu = 
		    find_stab_function_addr (name, last_source_file, objfile);

		  /* The function find_stab_function_addr will return
		     0 if the minimal symbol wasn't found.
		     (Unfortunately, this might also be a valid
		     address.)  Anyway, if it *does* return 0, it is
		     likely that the value was set correctly to begin
		     with...  */
		  if (minsym_valu != 0)
		    valu = minsym_valu;
		}
#endif

	      if (block_address_function_relative)
		/* For Solaris 2 compilers, the block addresses and
		   N_SLINE's are relative to the start of the
		   function.  On normal systems, and when using GCC on
		   Solaris 2, these addresses are just absolute, or
		   relative to the N_SO, depending on
		   BLOCK_ADDRESS_ABSOLUTE.  */
		function_start_offset = valu;

	      within_function = 1;
	      saw_fun_start = 1;

	      if (context_stack_depth > 1)
		{
		  complaint (&symfile_complaints,
			     _("unmatched N_LBRAC before symtab pos %d"), symnum);
		  break;
		}

	      if (context_stack_depth > 0)
		{
		  new = pop_context ();
		  /* Make a block for the local symbols within.  */
		  /* APPLE LOCAL begin address ranges  */
		  finish_block (new->name, &local_symbols, new->old_blocks,
				new->start_addr, valu, NULL, objfile);
		  /* APPLE LOCAL end address ranges  */
		}

	      new = push_context (0, valu);
	      /* APPLE LOCAL symbol prefixes */
	      new->name = define_symbol (valu, name, prefix, desc, type, objfile);
	      break;

	    default:
	      /* APPLE LOCAL symbol prefixes */
	      define_symbol (valu, name, prefix, desc, type, objfile);
	      break;
	    }
	}
      break;

      /* We use N_OPT to carry the gcc2_compiled flag.  Sun uses it
         for a bunch of other flags, too.  Someday we may parse their
         flags; for now we ignore theirs and hope they'll ignore ours.  */
    case N_OPT:			/* Solaris 2: Compiler options.  */
      if (name)
	{
	  if (strcmp (name, GCC2_COMPILED_FLAG_SYMBOL) == 0)
	    {
	      processing_gcc_compilation = 2;
#if 0				/* Works, but is experimental.  -fnf */
	      /* For now, stay with AUTO_DEMANGLING for g++ output, as
		 we don't know whether it will use the old style or v3
		 mangling.  */
	      if (AUTO_DEMANGLING)
		{
		  set_demangling_style (GNU_DEMANGLING_STYLE_STRING);
		}
#endif
	    }
	  else
	    n_opt_found = 1;
	}
      break;

    case N_MAIN:		/* Name of main routine.  */
      /* FIXME: If one has a symbol file with N_MAIN and then replaces
	 it with a symbol file with "main" and without N_MAIN.  I'm
	 not sure exactly what rule to follow but probably something
	 like: N_MAIN takes precedence over "main" no matter what
	 objfile it is in; If there is more than one N_MAIN, choose
	 the one in the symfile_objfile; If there is more than one
	 N_MAIN within a given objfile, complain() and choose
	 arbitrarily. (kingdon) */
      if (name != NULL)
	set_main_name (name);
      break;

      /* The following symbol types can be ignored.  */
    case N_OBJ:			/* Solaris 2: Object file dir and name.  */
    case N_PATCH:		/* Solaris 2: Patch Run Time Checker.  */
      /* N_UNDF:                   Solaris 2: File separator mark.  */
      /* N_UNDF: -- we will never encounter it, since we only process
         one file's symbols at once.  */
    case N_ENDM:		/* Solaris 2: End of module.  */
    case N_ALIAS:		/* SunPro F77: alias name, ignore for now.  */
      break;
    }

  /* '#' is a GNU C extension to allow one symbol to refer to another
     related symbol.

     Generally this is used so that an alias can refer to its main
     symbol.  */
  if (name[0] == '#')
    {
      /* Initialize symbol reference names and determine if this is a
         definition.  If a symbol reference is being defined, go ahead
         and add it.  Otherwise, just return.  */

      char *s = name;
      int refnum;

      /* If this stab defines a new reference ID that is not on the
         reference list, then put it on the reference list.

         We go ahead and advance NAME past the reference, even though
         it is not strictly necessary at this time.  */
      refnum = symbol_reference_defined (&s);
      if (refnum >= 0)
	if (!ref_search (refnum))
	  ref_add (refnum, 0, name, valu);
      name = s;
    }

  previous_stab_code = type;
}

/* FIXME: The only difference between this and elfstab_build_psymtabs
   is the call to install_minimal_symbols for elf, and the support for
   split sections.  If the differences are really that small, the code
   should be shared.  */

/* Scan and build partial symbols for an coff symbol file.
   The coff file has already been processed to get its minimal symbols.

   This routine is the equivalent of dbx_symfile_init and dbx_symfile_read
   rolled into one.

   OBJFILE is the object file we are reading symbols from.
   ADDR is the address relative to which the symbols are (e.g.
   the base address of the text segment).
   MAINLINE is true if we are reading the main symbol
   table (as opposed to a shared lib or dynamically loaded file).
   TEXTADDR is the address of the text section.
   TEXTSIZE is the size of the text section.
   STABSECTS is the list of .stab sections in OBJFILE.
   STABSTROFFSET and STABSTRSIZE define the location in OBJFILE where the
   .stabstr section exists.

   This routine is mostly copied from dbx_symfile_init and dbx_symfile_read,
   adjusted for coff details. */

void
coffstab_build_psymtabs (struct objfile *objfile, int mainline,
			 CORE_ADDR textaddr, unsigned int textsize,
			 struct stab_section_list *stabsects,
			 file_ptr stabstroffset, unsigned int stabstrsize)
{
  int val;
  bfd *sym_bfd = objfile->obfd;
  char *name = bfd_get_filename (sym_bfd);
  struct dbx_symfile_info *info;
  unsigned int stabsize;

  /* There is already a dbx_symfile_info allocated by our caller.
     It might even contain some info from the coff symtab to help us.  */
  info = objfile->deprecated_sym_stab_info;

  DBX_TEXT_ADDR (objfile) = textaddr;
  DBX_TEXT_SIZE (objfile) = textsize;

#define	COFF_STABS_SYMBOL_SIZE	12	/* XXX FIXME XXX */
  DBX_SYMBOL_SIZE (objfile) = COFF_STABS_SYMBOL_SIZE;
  DBX_STRINGTAB_SIZE (objfile) = stabstrsize;

  if (stabstrsize > bfd_get_size (sym_bfd))
    /* APPLE LOCAL better error message */
    error ("error parsing symbol file: invalid string table size (%d bytes).", stabstrsize);
  DBX_STRINGTAB (objfile) = (char *)
    obstack_alloc (&objfile->objfile_obstack, stabstrsize + 1);
  OBJSTAT (objfile, sz_strtab += stabstrsize + 1);

  /* Now read in the string table in one big gulp.  */

  val = bfd_seek (sym_bfd, stabstroffset, SEEK_SET);
  if (val < 0)
    perror_with_name (name);
  val = bfd_bread (DBX_STRINGTAB (objfile), stabstrsize, sym_bfd);
  if (val != stabstrsize)
    perror_with_name (name);

  stabsread_new_init ();
  buildsym_new_init ();
  free_header_files ();
  init_header_files ();

  processing_acc_compilation = 1;

  /* In a coff file, we've already installed the minimal symbols that came
     from the coff (non-stab) symbol table, so always act like an
     incremental load here. */
  if (stabsects->next == NULL)
    {
      stabsize = bfd_section_size (sym_bfd, stabsects->section);
      DBX_SYMCOUNT (objfile) = stabsize / DBX_SYMBOL_SIZE (objfile);
      DBX_SYMTAB_OFFSET (objfile) = stabsects->section->filepos;
    }
  else
    {
      struct stab_section_list *stabsect;

      DBX_SYMCOUNT (objfile) = 0;
      for (stabsect = stabsects; stabsect != NULL; stabsect = stabsect->next)
	{
	  stabsize = bfd_section_size (sym_bfd, stabsect->section);
	  DBX_SYMCOUNT (objfile) += stabsize / DBX_SYMBOL_SIZE (objfile);
	}

      DBX_SYMTAB_OFFSET (objfile) = stabsects->section->filepos;

      symbuf_sections = stabsects->next;
      symbuf_left = bfd_section_size (sym_bfd, stabsects->section);
      symbuf_read = 0;
    }

  dbx_symfile_read (objfile, 0);
}

/* Scan and build partial symbols for an ELF symbol file.
   This ELF file has already been processed to get its minimal symbols.

   This routine is the equivalent of dbx_symfile_init and dbx_symfile_read
   rolled into one.

   OBJFILE is the object file we are reading symbols from.
   ADDR is the address relative to which the symbols are (e.g.
   the base address of the text segment).
   MAINLINE is true if we are reading the main symbol
   table (as opposed to a shared lib or dynamically loaded file).
   STABSECT is the BFD section information for the .stab section.
   STABSTROFFSET and STABSTRSIZE define the location in OBJFILE where the
   .stabstr section exists.

   This routine is mostly copied from dbx_symfile_init and dbx_symfile_read,
   adjusted for elf details. */

void
elfstab_build_psymtabs (struct objfile *objfile, int mainline,
			asection *stabsect,
			file_ptr stabstroffset, unsigned int stabstrsize)
{
  int val;
  bfd *sym_bfd = objfile->obfd;
  char *name = bfd_get_filename (sym_bfd);
  struct dbx_symfile_info *info;
  struct cleanup *back_to = NULL;

  /* There is already a dbx_symfile_info allocated by our caller.
     It might even contain some info from the ELF symtab to help us.  */
  info = objfile->deprecated_sym_stab_info;

  /* Find the first and last text address.  dbx_symfile_read seems to
     want this.  */
  find_text_range (sym_bfd, objfile);

#define	ELF_STABS_SYMBOL_SIZE	12	/* XXX FIXME XXX */
  DBX_SYMBOL_SIZE (objfile) = ELF_STABS_SYMBOL_SIZE;
  DBX_SYMCOUNT (objfile)
    = bfd_section_size (objfile->obfd, stabsect) / DBX_SYMBOL_SIZE (objfile);
  DBX_STRINGTAB_SIZE (objfile) = stabstrsize;
  DBX_SYMTAB_OFFSET (objfile) = stabsect->filepos;
  DBX_STAB_SECTION (objfile) = stabsect;

  if (stabstrsize > bfd_get_size (sym_bfd))
    /* APPLE LOCAL better error message */
    error ("error parsing symbol file: invalid string table size (%d bytes).", stabstrsize);
  DBX_STRINGTAB (objfile) = (char *)
    obstack_alloc (&objfile->objfile_obstack, stabstrsize + 1);
  OBJSTAT (objfile, sz_strtab += stabstrsize + 1);

  /* Now read in the string table in one big gulp.  */

  val = bfd_seek (sym_bfd, stabstroffset, SEEK_SET);
  if (val < 0)
    perror_with_name (name);
  val = bfd_bread (DBX_STRINGTAB (objfile), stabstrsize, sym_bfd);
  if (val != stabstrsize)
    perror_with_name (name);

  stabsread_new_init ();
  buildsym_new_init ();
  free_header_files ();
  init_header_files ();

  processing_acc_compilation = 1;

  symbuf_read = 0;
  symbuf_left = bfd_section_size (objfile->obfd, stabsect);
  stabs_data = symfile_relocate_debug_section (objfile->obfd, stabsect, NULL);
  if (stabs_data)
    back_to = make_cleanup (free_current_contents, (void *) &stabs_data);

  /* In an elf file, we've already installed the minimal symbols that came
     from the elf (non-stab) symbol table, so always act like an
     incremental load here.  dbx_symfile_read should not generate any new
     minimal symbols, since we will have already read the ELF dynamic symbol
     table and normal symbol entries won't be in the ".stab" section; but in
     case it does, it will install them itself.  */
  dbx_symfile_read (objfile, 0);

  if (back_to)
    do_cleanups (back_to);
}

/* Scan and build partial symbols for a file with special sections for stabs
   and stabstrings.  The file has already been processed to get its minimal
   symbols, and any other symbols that might be necessary to resolve GSYMs.

   This routine is the equivalent of dbx_symfile_init and dbx_symfile_read
   rolled into one.

   OBJFILE is the object file we are reading symbols from.
   ADDR is the address relative to which the symbols are (e.g. the base address
   of the text segment).
   MAINLINE is true if we are reading the main symbol table (as opposed to a
   shared lib or dynamically loaded file).
   STAB_NAME is the name of the section that contains the stabs.
   STABSTR_NAME is the name of the section that contains the stab strings.

   This routine is mostly copied from dbx_symfile_init and dbx_symfile_read. */

void
stabsect_build_psymtabs (struct objfile *objfile, int mainline, char *stab_name,
			 char *stabstr_name, char *text_name)
{
  int val;
  bfd *sym_bfd = objfile->obfd;
  char *name = bfd_get_filename (sym_bfd);
  asection *stabsect;
  asection *stabstrsect;
  asection *text_sect;

  stabsect = bfd_get_section_by_name (sym_bfd, stab_name);
  stabstrsect = bfd_get_section_by_name (sym_bfd, stabstr_name);

  if (!stabsect)
    return;

  if (!stabstrsect)
    error (_("stabsect_build_psymtabs:  Found stabs (%s), but not string section (%s)"),
	   stab_name, stabstr_name);

  objfile->deprecated_sym_stab_info = (struct dbx_symfile_info *)
    xmalloc (sizeof (struct dbx_symfile_info));
  memset (objfile->deprecated_sym_stab_info, 0, sizeof (struct dbx_symfile_info));

  text_sect = bfd_get_section_by_name (sym_bfd, text_name);
  if (!text_sect)
    error (_("Can't find %s section in symbol file"), text_name);
  DBX_TEXT_ADDR (objfile) = bfd_section_vma (sym_bfd, text_sect);
  DBX_TEXT_SIZE (objfile) = bfd_section_size (sym_bfd, text_sect);

  DBX_SYMBOL_SIZE (objfile) = sizeof (struct external_nlist);
  DBX_SYMCOUNT (objfile) = bfd_section_size (sym_bfd, stabsect)
    / DBX_SYMBOL_SIZE (objfile);
  DBX_STRINGTAB_SIZE (objfile) = bfd_section_size (sym_bfd, stabstrsect);
  DBX_SYMTAB_OFFSET (objfile) = stabsect->filepos;	/* XXX - FIXME: POKING INSIDE BFD DATA STRUCTURES */

  if (DBX_STRINGTAB_SIZE (objfile) > bfd_get_size (sym_bfd))
    error (_("ridiculous string table size: %d bytes"), DBX_STRINGTAB_SIZE (objfile));
  DBX_STRINGTAB (objfile) = (char *)
    obstack_alloc (&objfile->objfile_obstack, DBX_STRINGTAB_SIZE (objfile) + 1);
  OBJSTAT (objfile, sz_strtab += DBX_STRINGTAB_SIZE (objfile) + 1);

  /* Now read in the string table in one big gulp.  */

  {
    struct cleanup *cache_cleanup;
    mem_disable_caching ();
    cache_cleanup = make_cleanup (mem_enable_caching, 0);
    val = bfd_get_section_contents (sym_bfd,	/* bfd */
				    stabstrsect,	/* bfd section */
				    DBX_STRINGTAB (objfile),	/* input buffer */
				    0,	/* offset into section */
				    DBX_STRINGTAB_SIZE (objfile));	/* amount to read */
    do_cleanups (cache_cleanup);
  }

  if (!val)
    perror_with_name (name);

  stabsread_new_init ();
  buildsym_new_init ();
  free_header_files ();
  init_header_files ();

  /* Now, do an incremental load */
}

/* APPLE LOCAL: The compiler may indicate the source language in
   the SO stab's "desc" field.  This was originally a Sun extension
   to stabs.  */
static enum language
read_so_stab_language_hint (short unsigned n_desc)
{
  switch (n_desc) {
    case N_SO_AS:
      return language_asm;
    case N_SO_C:
      return language_c;
    case N_SO_ANSI_C:
      return language_c;
    case N_SO_CC:
      return language_cplus;
    case N_SO_FORTRAN:
      return language_fortran;
    case N_SO_PASCAL:
      return language_pascal;
    case N_SO_FORTRAN90:
      return language_fortran;
    case N_SO_OBJC:
      return language_objc;
    case N_SO_OBJCPLUS:
      return language_objcplus;
    default:
      return language_unknown;
  }
}

/* APPLE LOCAL: Check for a telltale BFD section that will let us
   know if we're working on an objfile containing Objective C. */
static int
objfile_contains_objc (struct objfile *objfile)
{
  if (bfd_get_section_by_name (objfile->obfd, "LC_SEGMENT.__OBJC"))
    return 1;

  return 0;
}


static struct sym_fns aout_sym_fns =
{
  bfd_target_aout_flavour,
  dbx_new_init,			/* sym_new_init: init anything gbl to entire symtab */
  dbx_symfile_init,		/* sym_init: read initial info, setup for sym_read() */
  dbx_symfile_read,		/* sym_read: read a symbol file into symtab */
  dbx_symfile_finish,		/* sym_finish: finished with file, cleanup */
  default_symfile_offsets,	/* sym_offsets: parse user's offsets to internal form */
  NULL				/* next: pointer to next struct sym_fns */
};

void
_initialize_dbxread (void)
{
  /* APPLE LOCAL begin dbxread */
  add_setshow_boolean_cmd ("read-type-psyms", class_obscure,
			   &read_type_psym_p, _("\
Set if we should create partial symbols for types."), _("\
Show if we should create partial symbols for types."), NULL,
			   NULL,
			   NULL,
			   &setlist, &showlist);
  add_setshow_boolean_cmd ("function-end-absolute", class_obscure,
			   &end_fun_absolute_p, _("\
Set if N_FUN end-of-function symbols use absolute addresses on non-GCC files."), _("\
Show if N_FUN end-of-function symbols use absolute addresses on non-GCC files."), NULL,
			   NULL,
			   NULL,
			   &setlist, &showlist);
  /* APPLE LOCAL end dbxread */

  add_symtab_fns (&aout_sym_fns);

}
