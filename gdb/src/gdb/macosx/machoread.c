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

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "breakpoint.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "buildsym.h"
#include "obstack.h"
#include "gdb-stabs.h"
#include "stabsread.h"
#include "gdbcmd.h"
#include "completer.h"

#include "mach-o.h"
#include "gdb_assert.h"

#include <string.h>

#if HAVE_MMAP
static int mmap_strtabflag = 1;
#endif /* HAVE_MMAP */

static int mach_o_process_exports_flag = 1;

struct macho_symfile_info
{
  asymbol **syms;
  long nsyms;
};

static int macho_read_indirect_symbols (bfd *abfd,
                                        struct bfd_mach_o_dysymtab_command
                                        *dysymtab,
                                        struct bfd_mach_o_symtab_command
                                        *symtab, struct objfile *objfile);

static void
macho_new_init (struct objfile *objfile)
{
}

static void
macho_symfile_init (struct objfile *objfile)
{
  objfile->sym_stab_info =
    xmmalloc (objfile->md, sizeof (struct dbx_symfile_info));

  memset ((PTR) objfile->sym_stab_info, 0, sizeof (struct dbx_symfile_info));

  objfile->sym_private =
    xmmalloc (objfile->md, sizeof (struct macho_symfile_info));

  memset (objfile->sym_private, 0, sizeof (struct macho_symfile_info));

  objfile->flags |= OBJF_REORDERED;
  init_entry_point_info (objfile);
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

void dbx_symfile_read (struct objfile *objfile, int mainline);

void
macho_build_psymtabs (struct objfile *objfile, int mainline,
                      char *stab_name, char *stabstr_name,
                      char *text_name,
                      char *local_stab_name, char *nonlocal_stab_name,
                      char *coalesced_text_name,
                      char *data_name, char *bss_name)
{
  int val;
  bfd *sym_bfd = objfile->obfd;
  char *name = bfd_get_filename (sym_bfd);
  asection *stabsect;
  asection *stabstrsect;
  asection *text_sect, *coalesced_text_sect;
  asection *local_stabsect, *nonlocal_stabsect;

#if 0
  init_minimal_symbol_collection ();
  make_cleanup (discard_minimal_symbols, 0);
#endif

  stabsect = bfd_get_section_by_name (sym_bfd, stab_name);
  stabstrsect = bfd_get_section_by_name (sym_bfd, stabstr_name);

  if (!stabsect)
    return;

  if (!stabstrsect)
    error
      ("macho_build_psymtabs:  Found stabs (%s), but not string section (%s)",
       stab_name, stabstr_name);

  objfile->sym_stab_info = (struct dbx_symfile_info *)
    xmmalloc (objfile->md, sizeof (struct dbx_symfile_info));
  memset (objfile->sym_stab_info, 0, sizeof (struct dbx_symfile_info));

  gdb_assert (text_name != NULL);
  gdb_assert (data_name != NULL);

  /* APPLE LOCAL: Lots of symbols also end up in the coalesced text section
     in C++ and we don't want them to get the wrong section...  */
  if (coalesced_text_name != NULL)
    DBX_COALESCED_TEXT_SECTION (objfile) =
      bfd_get_section_by_name (sym_bfd, coalesced_text_name);
  else
    DBX_COALESCED_TEXT_SECTION (objfile) = NULL;

  DBX_TEXT_SECTION (objfile) = bfd_get_section_by_name (sym_bfd, text_name);
  DBX_DATA_SECTION (objfile) = bfd_get_section_by_name (sym_bfd, data_name);

  if (bss_name != NULL)
    {
      DBX_BSS_SECTION (objfile) = bfd_get_section_by_name (sym_bfd, bss_name);
    }

  if (!DBX_TEXT_SECTION (objfile))
    {
      error ("Can't find %s section in symbol file", text_name);
    }
  if (!DBX_DATA_SECTION (objfile))
    {
      warning ("Can't find %s section in symbol file", data_name);
    }

  text_sect = DBX_TEXT_SECTION (objfile);

  DBX_TEXT_ADDR (objfile) = bfd_section_vma (sym_bfd, text_sect);
  DBX_TEXT_SIZE (objfile) = bfd_section_size (sym_bfd, text_sect);

  /* APPLE LOCAL: Pre-fetch the addresses for the coalesced section as well.
     Note: It is not an error not to have a coalesced section...  */

  coalesced_text_sect = DBX_COALESCED_TEXT_SECTION (objfile);

  if (coalesced_text_sect)
    {
      DBX_COALESCED_TEXT_ADDR (objfile) =
        bfd_section_vma (sym_bfd, coalesced_text_sect);
      DBX_COALESCED_TEXT_SIZE (objfile) =
        bfd_section_size (sym_bfd, coalesced_text_sect);
    }
  else
    {
      DBX_COALESCED_TEXT_ADDR (objfile) = 0;
      DBX_COALESCED_TEXT_SIZE (objfile) = 0;
    }
  /* END APPLE LOCAL */

  DBX_SYMBOL_SIZE (objfile) =
    (bfd_mach_o_version (objfile->obfd) > 1) ? 16 : 12;
  DBX_SYMCOUNT (objfile) =
    bfd_section_size (sym_bfd, stabsect) / DBX_SYMBOL_SIZE (objfile);
  DBX_STRINGTAB_SIZE (objfile) = bfd_section_size (sym_bfd, stabstrsect);

  /* XXX - FIXME: POKING INSIDE BFD DATA STRUCTURES */
  DBX_SYMTAB_OFFSET (objfile) = stabsect->filepos;

#if HAVE_MMAP
  if (mmap_strtabflag)
    {

      /* currently breaks mapped symbol files (string table doesn't end up in objfile) */

      bfd_window w;
      bfd_init_window (&w);

      /* APPLE LOCAL: Open the string table read only if possible.  Should
         be more efficient.  */

      val = bfd_get_section_contents_in_window_with_mode
        (sym_bfd, stabstrsect, &w, 0, DBX_STRINGTAB_SIZE (objfile), 0);

      if (!val)
        perror_with_name (name);

      DBX_STRINGTAB (objfile) = w.data;

    }
  else
    {
#endif
      if (DBX_STRINGTAB_SIZE (objfile) > bfd_get_size (sym_bfd))
        error
          ("error parsing symbol file: invalid string table size (%d bytes)",
           DBX_STRINGTAB_SIZE (objfile));
      DBX_STRINGTAB (objfile) =
        (char *) obstack_alloc (&objfile->objfile_obstack,
                                DBX_STRINGTAB_SIZE (objfile) + 1);
      OBJSTAT (objfile, sz_strtab += DBX_STRINGTAB_SIZE (objfile) + 1);

      /* Now read in the string table in one big gulp.  */

      val = bfd_get_section_contents
        (sym_bfd, stabstrsect, DBX_STRINGTAB (objfile), 0,
         DBX_STRINGTAB_SIZE (objfile));

      if (!val)
        perror_with_name (name);
#if HAVE_MMAP
    }
#endif

  /* APPLE LOCAL: Get the "local" vs "nonlocal" nlist record locations
     from the LC_DYSYMTAB load command if it was provided. */
  local_stabsect = bfd_get_section_by_name (sym_bfd, local_stab_name);
  nonlocal_stabsect = bfd_get_section_by_name (sym_bfd, nonlocal_stab_name);
  if (local_stabsect == NULL || nonlocal_stabsect == NULL)
    local_stabsect = nonlocal_stabsect = NULL;

  /* APPLE LOCAL: Initialize the local/non-local stab nlist record pointers
     Set everything to 0 if there's no information provided by the static link
     editor -- users of these values should fall back to using the standard
     DBX_SYMTAB_OFFSET et al values for all stab records. */
  if (local_stabsect == NULL)
    {
      DBX_LOCAL_STAB_OFFSET (objfile) = 0;
      DBX_LOCAL_STAB_COUNT (objfile) = 0;
      DBX_NONLOCAL_STAB_OFFSET (objfile) = 0;
      DBX_NONLOCAL_STAB_COUNT (objfile) = 0;
    }
  else
    {
      /* XXX - FIXME: POKING INSIDE BFD DATA STRUCTURES */
      DBX_LOCAL_STAB_OFFSET (objfile) = local_stabsect->filepos;
      DBX_LOCAL_STAB_COUNT (objfile) = bfd_section_size (sym_bfd,
                                                         local_stabsect) /
        DBX_SYMBOL_SIZE (objfile);
      /* XXX - FIXME: POKING INSIDE BFD DATA STRUCTURES */
      DBX_NONLOCAL_STAB_OFFSET (objfile) = nonlocal_stabsect->filepos;
      DBX_NONLOCAL_STAB_COUNT (objfile) = bfd_section_size (sym_bfd,
                                                            nonlocal_stabsect)
        / DBX_SYMBOL_SIZE (objfile);
    }

  stabsread_new_init ();
  buildsym_new_init ();
  free_header_files ();
  init_header_files ();

#if 0
  install_minimal_symbols (objfile);
#endif

  processing_acc_compilation = 1;
  dbx_symfile_read (objfile, mainline);
}

static void
macho_symfile_read (struct objfile *objfile, int mainline)
{
  bfd *abfd = objfile->obfd;

  struct bfd_mach_o_load_command *gsymtab, *gdysymtab;

  struct bfd_mach_o_symtab_command *symtab = NULL;
  struct bfd_mach_o_dysymtab_command *dysymtab = NULL;

  int ret;

  CHECK_FATAL (objfile != NULL);
  CHECK_FATAL (abfd != NULL);
  CHECK_FATAL (abfd->filename != NULL);

  init_minimal_symbol_collection ();
  make_cleanup_discard_minimal_symbols ();

  /* If we are reinitializing, or if we have never loaded syms yet,
     set table to empty.  MAINLINE is cleared so that *_read_psymtab
     functions do not all also re-initialize the psymbol table. */
  if (mainline)
    {
      init_psymbol_list (objfile, 0);
      mainline = 0;
    }

  macho_build_psymtabs (objfile, mainline,
                        "LC_SYMTAB.stabs", "LC_SYMTAB.stabstr",
                        "LC_SEGMENT.__TEXT.__text",
                        "LC_DYSYMTAB.localstabs",
                        "LC_DYSYMTAB.nonlocalstabs",
                        "LC_SEGMENT.__TEXT.__textcoal_nt",
                        "LC_SEGMENT.__DATA.__data",
                        "LC_SEGMENT.__DATA.__bss");

  if (dwarf2_has_info (abfd))
    {
      /* DWARF 2 sections */
      dwarf2_build_psymtabs (objfile, mainline);
#if 0
      /* FIXME: This doesn't seem to be right on Mach-O yet.  */
      /* FIXME: kettenis/20030504: This still needs to be integrated with
         dwarf2read.c in a better way.  */
      dwarf2_build_frame_info (objfile);

#endif
    }

  if (mach_o_process_exports_flag)
    {

      ret = bfd_mach_o_lookup_command (abfd, BFD_MACH_O_LC_SYMTAB, &gsymtab);
      if (ret != 1)
        {
          /* warning ("Error fetching LC_SYMTAB load command from object file \"%s\"",
             abfd->filename); */
          install_minimal_symbols (objfile);
          return;
        }

      ret =
        bfd_mach_o_lookup_command (abfd, BFD_MACH_O_LC_DYSYMTAB, &gdysymtab);
      if (ret != 1)
        {
          /* warning ("Error fetching LC_DYSYMTAB load command from object file \"%s\"",
             abfd->filename); */
          install_minimal_symbols (objfile);
          return;
        }

      CHECK_FATAL (gsymtab->type == BFD_MACH_O_LC_SYMTAB);
      CHECK_FATAL (gdysymtab->type == BFD_MACH_O_LC_DYSYMTAB);

      symtab = &gsymtab->command.symtab;
      dysymtab = &gdysymtab->command.dysymtab;

      CHECK_FATAL (DBX_STRINGTAB (objfile) != NULL);
      if (symtab->strtab == NULL)
        {
          symtab->strtab = DBX_STRINGTAB (objfile);
        }

      if (symtab->strtab == NULL)
        {
          ret = bfd_mach_o_scan_read_symtab_strtab (abfd, symtab);
          if (ret != 0)
            {
              warning ("Unable to read symbol table for \"%s\": %s",
                       abfd->filename, bfd_errmsg (bfd_get_error ()));
              install_minimal_symbols (objfile);
              return;
            }
        }

      if (!macho_read_indirect_symbols (abfd, dysymtab, symtab, objfile))
        {
          install_minimal_symbols (objfile);
          return;
        }
    }

  install_minimal_symbols (objfile);
}

/* Record minsyms for the dyld stub trampolines; prefix them with "dyld_stub_".  */

int
macho_read_indirect_symbols (bfd *abfd,
                             struct bfd_mach_o_dysymtab_command *dysymtab,
                             struct bfd_mach_o_symtab_command *symtab,
                             struct objfile *objfile)
{

  unsigned long i, nsyms, ret;
  asymbol sym;
  asection *bfdsec = NULL;
  long section_count;
  struct bfd_mach_o_section *section = NULL;
  struct bfd_mach_o_load_command *lcommand = NULL;

  for (section_count = abfd->section_count, bfdsec = abfd->sections;
       section_count > 0; section_count--, bfdsec = bfdsec->next)
    {

      ret = bfd_mach_o_lookup_section (abfd, bfdsec, &lcommand, &section);
      if (ret != 1)
        {
          /* warning ("error fetching section %s from object file", bfd_section_name (abfd, bfdsec)); */
          continue;
        }
      if (section == NULL)
        continue;
      if ((section->flags & BFD_MACH_O_SECTION_TYPE_MASK) !=
          BFD_MACH_O_S_SYMBOL_STUBS)
        continue;
      if (section->reserved2 == 0)
        {
          warning
            ("section %s has S_SYMBOL_STUBS flag set, but not reserved2",
             bfd_section_name (abfd, bfdsec));
          continue;
        }

      nsyms = section->size / section->reserved2;

      for (i = 0; i < nsyms; i++)
        {

          unsigned long cursym = section->reserved1 + i;
          CORE_ADDR stubaddr = section->addr + (i * section->reserved2);
          const char *sname = NULL;
          char nname[4096];

          if (cursym >= dysymtab->nindirectsyms)
            {
              warning
                ("Indirect symbol entry out of range in \"%s\" (%lu >= %lu)",
                 abfd->filename, cursym,
                 (unsigned long) dysymtab->nindirectsyms);
              return 0;
            }
          ret =
            bfd_mach_o_scan_read_dysymtab_symbol (abfd, dysymtab, symtab,
                                                  &sym, cursym);
          if (ret != 0)
            {
              return 0;
            }

          sname = sym.name;
          CHECK_FATAL (sname != NULL);
          if (sname[0] == bfd_get_symbol_leading_char (abfd))
            {
              sname++;
            }

          CHECK_FATAL ((strlen (sname) + sizeof ("__dyld_stub_") + 1) < 4096);
          sprintf (nname, "dyld_stub_%s", sname);

          stubaddr +=
            ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
          prim_record_minimal_symbol_and_info (nname, stubaddr,
                                               mst_solib_trampoline, NULL,
                                               SECT_OFF_TEXT (objfile),
                                               bfdsec, objfile);
        }
    }

  return 1;
}

static void
macho_symfile_finish (struct objfile *objfile)
{
}

static void
macho_symfile_offsets (struct objfile *objfile,
                       struct section_addr_info *addrs)
{
  unsigned int i;
  unsigned int num_sections;

  objfile->num_sections = addrs->num_sections;
  objfile->section_offsets = (struct section_offsets *)
    obstack_alloc (&objfile->objfile_obstack,
                   SIZEOF_N_SECTION_OFFSETS (objfile->num_sections));
  memset (objfile->section_offsets, 0,
          SIZEOF_N_SECTION_OFFSETS (objfile->num_sections));

  /* I am not quite clear on why we are relocating the objfile
     sections here rather than using relocate_objfile.  Anyway,
     if we do move the sections, we need to shift the objfiles in
     the ordered_sections array as well.  But if the offset is
     zero, don't bother...  */

  if (addrs->other[0].addr != 0)
    {
      objfile_delete_from_ordered_sections (objfile);
      num_sections = objfile->sections_end - objfile->sections;
      for (i = 0; i < num_sections; i++)
        {
          objfile->sections[i].addr += addrs->other[0].addr;
          objfile->sections[i].endaddr += addrs->other[0].addr;
        }
      objfile_add_to_ordered_sections (objfile);
    }

  for (i = 0; i < objfile->num_sections; i++)
    {
      objfile->section_offsets->offsets[i] = addrs->other[0].addr;
    }

  objfile->sect_index_text = 0;
  objfile->sect_index_data = 0;
  objfile->sect_index_bss = 0;
  objfile->sect_index_rodata = 0;
}

static struct sym_fns macho_sym_fns = {
  bfd_target_mach_o_flavour,

  macho_new_init,               /* sym_new_init: init anything gbl to entire symtab */
  macho_symfile_init,           /* sym_init: read initial info, setup for sym_read() */
  macho_symfile_read,           /* sym_read: read a symbol file into symtab */
  macho_symfile_finish,         /* sym_finish: finished with file, cleanup */
  macho_symfile_offsets,        /* sym_offsets:  xlate external to internal form */
  NULL                          /* next: pointer to next struct sym_fns */
};

void
_initialize_machoread ()
{
  struct cmd_list_element *cmd;

  add_symtab_fns (&macho_sym_fns);

#if HAVE_MMAP
  cmd = add_set_cmd ("mmap-string-tables", class_obscure, var_boolean,
                     (char *) &mmap_strtabflag,
                     "Set if GDB should use mmap() to read STABS info.",
                     &setlist);
  add_show_from_set (cmd, &showlist);
#endif

  cmd = add_set_cmd ("mach-o-process-exports", class_obscure, var_boolean,
                     (char *) &mach_o_process_exports_flag,
                     "Set if GDB should process indirect function stub symbols from object files.",
                     &setlist);
  add_show_from_set (cmd, &showlist);
}
