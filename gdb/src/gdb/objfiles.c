/* GDB routines for manipulating objfiles.
   Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.
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

/* This file contains support routines for creating, manipulating, and
   destroying objfile structures. */

#include "defs.h"
#include "bfd.h"		/* Binary File Description */
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb-stabs.h"
#include "target.h"
#include "gdbcmd.h"

#include <sys/types.h>
#include "gdb_stat.h"
#include <fcntl.h>
#include "obstack.h"
#include "gdb_string.h"

#include "breakpoint.h"

#include <assert.h>

#ifndef TEXT_SECTION_NAME
#define TEXT_SECTION_NAME ".text"
#endif

#ifndef DATA_SECTION_NAME
#define DATA_SECTION_NAME ".data"
#endif

#define MAPPED_SYMFILES (USE_MMALLOC && HAVE_MMAP)

/* Externally visible variables that are owned by this module.
   See declarations in objfile.h for more info. */

struct objfile *object_files;	/* Linked list of all objfiles */
struct objfile *current_objfile;	/* For symbol file being read in */
struct objfile *symfile_objfile;	/* Main symbol table loaded from */
struct objfile *rt_common_objfile;	/* For runtime common symbols */

int mapped_symbol_files = 0;
int use_mapped_symbol_files = 1;

/* Put OBJFILE at the front of the list.  */

void
objfile_to_front (struct objfile *objfile)
{
  struct objfile **objp;
  for (objp = &object_files; *objp != NULL; objp = &((*objp)->next))
    {
      if (*objp == objfile)
	{
	  /* Unhook it from where it is.  */
	  *objp = objfile->next;
	  /* Put it in the front.  */
	  objfile->next = object_files;
	  object_files = objfile;
	  break;
	}
    }
}

/* Unlink OBJFILE from the list of known objfiles, if it is found in the
   list.

   It is not a bug, or error, to call this function if OBJFILE is not known
   to be in the current list.  This is done in the case of mapped objfiles,
   for example, just to ensure that the mapped objfile doesn't appear twice
   in the list.  Since the list is threaded, linking in a mapped objfile
   twice would create a circular list.

   If OBJFILE turns out to be in the list, we zap it's NEXT pointer after
   unlinking it, just to ensure that we have completely severed any linkages
   between the OBJFILE and the list. */

void
unlink_objfile (struct objfile *objfile)
{
  struct objfile **objpp;

  for (objpp = &object_files; *objpp != NULL; objpp = &((*objpp)->next))
    {
      if (*objpp == objfile)
	{
	  *objpp = (*objpp)->next;
	  objfile->next = NULL;
	  return;
	}
    }

  internal_error (__FILE__, __LINE__,
		  "unlink_objfile: objfile already unlinked");
}


/* Destroy an objfile and all the symtabs and psymtabs under it.  Note
   that as much as possible is allocated on the symbol_obstack and
   psymbol_obstack, so that the memory can be efficiently freed.

   Things which we do NOT free because they are not in malloc'd memory
   or not in memory specific to the objfile include:

   objfile -> sf

   FIXME:  If the objfile is using reusable symbol information (via mmalloc),
   then we need to take into account the fact that more than one process
   may be using the symbol information at the same time (when mmalloc is
   extended to support cooperative locking).  When more than one process
   is using the mapped symbol info, we need to be more careful about when
   we free objects in the reusable area. */

void
free_objfile (struct objfile *objfile)
{
  /* First do any symbol file specific actions required when we are
     finished with a particular symbol file.  Note that if the objfile
     is using reusable symbol information (via mmalloc) then each of
     these routines is responsible for doing the correct thing, either
     freeing things which are valid only during this particular gdb
     execution, or leaving them to be reused during the next one. */

  if (objfile->sf != NULL)
    {
      (*objfile->sf->sym_finish) (objfile);
    }

  /* We always close the bfd. */

  if (objfile->obfd != NULL)
    {
      char *name = bfd_get_filename (objfile->obfd);
      if (!bfd_close (objfile->obfd))
	warning ("cannot close \"%s\": %s",
		 name, bfd_errmsg (bfd_get_error ()));
      xfree (name);
    }

  /* Remove it from the chain of all objfiles. */

  unlink_objfile (objfile);

  /* If we are going to free the runtime common objfile, mark it
     as unallocated.  */

  if (objfile == rt_common_objfile)
    rt_common_objfile = NULL;

  /* Before the symbol table code was redone to make it easier to
     selectively load and remove information particular to a specific
     linkage unit, gdb used to do these things whenever the monolithic
     symbol table was blown away.  How much still needs to be done
     is unknown, but we play it safe for now and keep each action until
     it is shown to be no longer needed. */

#if defined (CLEAR_SOLIB)
  CLEAR_SOLIB ();
  /* CLEAR_SOLIB closes the bfd's for any shared libraries.  But
     the to_sections for a core file might refer to those bfd's.  So
     detach any core file.  */
  {
    struct target_ops *t = find_core_target ();
    if (t != NULL)
      (t->to_detach) (NULL, 0);
  }
#endif
  /* I *think* all our callers call clear_symtab_users.  If so, no need
     to call this here.  */
  clear_pc_function_cache ();

  /* The last thing we do is free the objfile struct itself for the
     non-reusable case, or detach from the mapped file for the
     reusable case.  Note that the mmalloc_detach or the xmfree() is
     the last thing we can do with this objfile. */

#if MAPPED_SYMFILES

  if (objfile->flags & OBJF_MAPPED)
    {
      /* Remember the fd so we can close it.  We can't close it before
         doing the detach, and after the detach the objfile is gone. */
      int mmfd;

      mmfd = objfile->mmfd;
      mmalloc_detach (objfile->md);
      objfile = NULL;
      close (mmfd);
    }

#endif /* MAPPED_SYMFILES */

  /* If we still have an objfile, then either we don't support reusable
     objfiles or this one was not reusable.  So free it normally. */

  if (objfile != NULL)
    {
      if (objfile->name != NULL)
	{
	  xmfree (objfile->md, objfile->name);
	}
      if (objfile->global_psymbols.list)
	xmfree (objfile->md, objfile->global_psymbols.list);
      if (objfile->static_psymbols.list)
	xmfree (objfile->md, objfile->static_psymbols.list);
      /* Free the obstacks for non-reusable objfiles */
      free_bcache (&objfile->psymbol_cache);
      obstack_free (&objfile->psymbol_obstack, 0);
      obstack_free (&objfile->symbol_obstack, 0);
      obstack_free (&objfile->type_obstack, 0);
      xmfree (objfile->md, objfile);
      objfile = NULL;
    }

  {
    extern struct target_ops exec_ops;

    update_section_tables (&current_target);
    update_section_tables (&exec_ops);
  }
}

void
update_section_tables (struct target_ops *target)
{
  struct objfile *objfile;
  struct obj_section *osection;
  unsigned int nsections = 0;
  struct section_table *sections, *tsection;

  if (target->to_sections != NULL) {
    assert (target->to_sections_end >= target->to_sections);
    /* xfree (target->to_sections); */
    target->to_sections = NULL;
    target->to_sections_end = NULL;
  }

  nsections = 0;
  ALL_OBJSECTIONS (objfile, osection) {
    nsections++;
  }

  sections = (struct section_table *) xmalloc (nsections * sizeof (struct section_table));

  tsection = sections;
  ALL_OBJSECTIONS (objfile, osection) {
    assert (objfile != NULL);
    assert (osection != NULL);
    assert (osection->objfile != NULL);
    tsection->addr = osection->addr;
    tsection->endaddr = osection->endaddr;
    tsection->the_bfd_section =osection->the_bfd_section;
    tsection->bfd = osection->objfile->obfd;
    tsection++;
  }

  target->to_sections = sections;
  target->to_sections_end = sections + nsections;
}

static void
do_free_objfile_cleanup (void *obj)
{
  free_objfile (obj);
}

struct cleanup *
make_cleanup_free_objfile (struct objfile *obj)
{
  return make_cleanup (do_free_objfile_cleanup, obj);
}

/* Free all the object files at once and clean up their users.  */

void
free_all_objfiles (void)
{
  struct objfile *objfile, *temp;

  ALL_OBJFILES_SAFE (objfile, temp)
  {
    free_objfile (objfile);
  }
  clear_symtab_users ();
}

/* Relocate OBJFILE to NEW_OFFSETS.  There should be OBJFILE->NUM_SECTIONS
   entries in new_offsets.  */
void
objfile_relocate (struct objfile *objfile, struct section_offsets *new_offsets)
{
  struct section_offsets *delta =
    (struct section_offsets *) alloca (SIZEOF_SECTION_OFFSETS);

  {
    int i;
    int something_changed = 0;
    for (i = 0; i < objfile->num_sections; ++i)
      {
	delta->offsets[i] =
	  ANOFFSET (new_offsets, i) - ANOFFSET (objfile->section_offsets, i);
	if (ANOFFSET (delta, i) != 0)
	  something_changed = 1;
      }
    if (!something_changed)
      return;
  }

  /* OK, get all the symtabs.  */
  {
    struct symtab *s;

    ALL_OBJFILE_SYMTABS (objfile, s)
    {
      struct linetable *l;
      struct blockvector *bv;
      int i;

      /* First the line table.  */
      l = LINETABLE (s);
      if (l)
	{
	  for (i = 0; i < l->nitems; ++i)
	    l->item[i].pc += ANOFFSET (delta, s->block_line_section);
	}

      /* Don't relocate a shared blockvector more than once.  */
      if (!s->primary)
	continue;

      bv = BLOCKVECTOR (s);
      for (i = 0; i < BLOCKVECTOR_NBLOCKS (bv); ++i)
	{
	  struct block *b;
	  struct symbol *sym;
	  int j;

	  b = BLOCKVECTOR_BLOCK (bv, i);
	  BLOCK_START (b) += ANOFFSET (delta, s->block_line_section);
	  BLOCK_END (b) += ANOFFSET (delta, s->block_line_section);

	  ALL_BLOCK_SYMBOLS (b, j, sym)
	    {
	      fixup_symbol_section (sym, objfile);

	      /* The RS6000 code from which this was taken skipped
	         any symbols in STRUCT_NAMESPACE or UNDEF_NAMESPACE.
	         But I'm leaving out that test, on the theory that
	         they can't possibly pass the tests below.  */
	      if ((SYMBOL_CLASS (sym) == LOC_LABEL
		   || SYMBOL_CLASS (sym) == LOC_STATIC
		   || SYMBOL_CLASS (sym) == LOC_INDIRECT)
		  && SYMBOL_SECTION (sym) >= 0)
		{
		  SYMBOL_VALUE_ADDRESS (sym) +=
		    ANOFFSET (delta, SYMBOL_SECTION (sym));
		}
#ifdef MIPS_EFI_SYMBOL_NAME
	      /* Relocate Extra Function Info for ecoff.  */

	      else if (SYMBOL_CLASS (sym) == LOC_CONST
		       && SYMBOL_NAMESPACE (sym) == LABEL_NAMESPACE
		       && strcmp (SYMBOL_NAME (sym), MIPS_EFI_SYMBOL_NAME) == 0)
		ecoff_relocate_efi (sym, ANOFFSET (delta,
						   s->block_line_section));
#endif
	    }
	}
    }
  }

  {
    struct partial_symtab *p;

    ALL_OBJFILE_PSYMTABS (objfile, p)
    {
      p->textlow += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
      p->texthigh += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
    }
  }

  {
    struct partial_symbol **psym;

    for (psym = objfile->global_psymbols.list;
	 psym < objfile->global_psymbols.next;
	 psym++)
      {
	fixup_psymbol_section (*psym, objfile);
	if (SYMBOL_SECTION (*psym) >= 0)
	  SYMBOL_VALUE_ADDRESS (*psym) += ANOFFSET (delta,
						    SYMBOL_SECTION (*psym));
      }
    for (psym = objfile->static_psymbols.list;
	 psym < objfile->static_psymbols.next;
	 psym++)
      {
	fixup_psymbol_section (*psym, objfile);
	if (SYMBOL_SECTION (*psym) >= 0)
	  SYMBOL_VALUE_ADDRESS (*psym) += ANOFFSET (delta,
						    SYMBOL_SECTION (*psym));
      }
  }

  {
    struct minimal_symbol *msym;
    ALL_OBJFILE_MSYMBOLS (objfile, msym)
      if (SYMBOL_SECTION (msym) >= 0)
      SYMBOL_VALUE_ADDRESS (msym) += ANOFFSET (delta, SYMBOL_SECTION (msym));
  }
  /* Relocating different sections by different amounts may cause the symbols
     to be out of order.  */
  msymbols_sort (objfile);

  {
    int i;
    for (i = 0; i < objfile->num_sections; ++i)
      (objfile->section_offsets)->offsets[i] = ANOFFSET (new_offsets, i);
  }

  if (objfile->ei.entry_point != ~(CORE_ADDR) 0)
    {
      /* Relocate ei.entry_point with its section offset, use SECT_OFF_TEXT
	 only as a fallback.  */
      struct obj_section *s;
      s = find_pc_section (objfile->ei.entry_point);
      if (s)
        objfile->ei.entry_point += ANOFFSET (delta, s->the_bfd_section->index);
      else
        objfile->ei.entry_point += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
    }

  {
    struct obj_section *s;
    bfd *abfd;

    abfd = objfile->obfd;

    ALL_OBJFILE_OSECTIONS (objfile, s)
      {
      	int idx = s->the_bfd_section->index;
	
	s->addr += ANOFFSET (delta, idx);
	s->endaddr += ANOFFSET (delta, idx);
      }
  }

  if (objfile->ei.entry_func_lowpc != INVALID_ENTRY_LOWPC)
    {
      objfile->ei.entry_func_lowpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
      objfile->ei.entry_func_highpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
    }

  if (objfile->ei.entry_file_lowpc != INVALID_ENTRY_LOWPC)
    {
      objfile->ei.entry_file_lowpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
      objfile->ei.entry_file_highpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
    }

  if (objfile->ei.main_func_lowpc != INVALID_ENTRY_LOWPC)
    {
      objfile->ei.main_func_lowpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
      objfile->ei.main_func_highpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
    }

  /* Relocate breakpoints as necessary, after things are relocated. */
  breakpoint_re_set ();
}

/* Many places in gdb want to test just to see if we have any partial
   symbols available.  This function returns zero if none are currently
   available, nonzero otherwise. */

int
have_partial_symbols (void)
{
  struct objfile *ofp;

  ALL_OBJFILES (ofp)
  {
    if (ofp->psymtabs != NULL)
      {
	return 1;
      }
  }
  return 0;
}

/* Many places in gdb want to test just to see if we have any full
   symbols available.  This function returns zero if none are currently
   available, nonzero otherwise. */

int
have_full_symbols (void)
{
  struct objfile *ofp;

  ALL_OBJFILES (ofp)
  {
    if (ofp->symtabs != NULL)
      {
	return 1;
      }
  }
  return 0;
}


/* This operations deletes all objfile entries that represent solibs that
   weren't explicitly loaded by the user, via e.g., the add-symbol-file
   command.
 */
void
objfile_purge_solibs (void)
{
  struct objfile *objf;
  struct objfile *temp;

  ALL_OBJFILES_SAFE (objf, temp)
  {
    /* We assume that the solib package has been purged already, or will
       be soon.
     */
#if 0
    /* not now --- the dyld code handles this better; and this will really make it upset */
    if (!(objf->flags & OBJF_USERLOADED) && (objf->flags & OBJF_SHARED))
      free_objfile (objf);
#endif
  }
}


/* Many places in gdb want to test just to see if we have any minimal
   symbols available.  This function returns zero if none are currently
   available, nonzero otherwise. */

int
have_minimal_symbols (void)
{
  struct objfile *ofp;

  ALL_OBJFILES (ofp)
  {
    if (ofp->msymbols != NULL)
      {
	return 1;
      }
  }
  return 0;
}

/* Returns a section whose range includes PC and SECTION, 
   or NULL if none found.  Note the distinction between the return type, 
   struct obj_section (which is defined in gdb), and the input type
   struct sec (which is a bfd-defined data type).  The obj_section
   contains a pointer to the bfd struct sec section.  */

struct obj_section *
find_pc_sect_section (CORE_ADDR pc, struct sec *section)
{
  struct obj_section *s;
  struct objfile *objfile;

  ALL_OBJSECTIONS (objfile, s)
    if ((section == 0 || section == s->the_bfd_section) &&
	s->addr <= pc && pc < s->endaddr)
      return (s);

  return (NULL);
}

/* Returns a section whose range includes PC or NULL if none found. 
   Backward compatibility, no section.  */

struct obj_section *
find_pc_section (CORE_ADDR pc)
{
  return find_pc_sect_section (pc, find_pc_mapped_section (pc));
}


/* In SVR4, we recognize a trampoline by it's section name. 
   That is, if the pc is in a section named ".plt" then we are in
   a trampoline.  */

int
in_plt_section (CORE_ADDR pc, char *name)
{
  struct obj_section *s;
  int retval = 0;

  s = find_pc_section (pc);

  retval = (s != NULL
	    && s->the_bfd_section->name != NULL
	    && STREQ (s->the_bfd_section->name, ".plt"));
  return (retval);
}

/* Return nonzero if NAME is in the import list of OBJFILE.  Else
   return zero.  */

int
is_in_import_list (char *name, struct objfile *objfile)
{
  register int i;

  if (!objfile || !name || !*name)
    return 0;

  for (i = 0; i < objfile->import_list_size; i++)
    if (objfile->import_list[i] && STREQ (name, objfile->import_list[i]))
      return 1;
  return 0;
}

void
_initialize_objfiles (void)
{
  struct cmd_list_element *c;

#if HAVE_MMAP
  c = add_set_cmd ("generate-persistent-symbol-tables", class_obscure, var_boolean,
		   (char *) &mapped_symbol_files,
		   "Set if GDB should generate persistent symbol tables by default.",
		   &setlist);
  add_show_from_set (c, &showlist);

  c = add_set_cmd ("use-persistent-symbol-tables", class_obscure, var_boolean,
		   (char *) &use_mapped_symbol_files,
		   "Set if GDB should use persistent symbol tables by default.",
		   &setlist);
  add_show_from_set (c, &showlist);
#endif /* HAVE_MMAP */
}
