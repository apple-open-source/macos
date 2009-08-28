/* GDB routines for manipulating objfiles.

   Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "bcache.h"
#include "mdebugread.h"
#include "gdb_assert.h"
#include <sys/types.h>
#include "gdb_stat.h"
#include <fcntl.h>
#include "gdb_obstack.h"
#include "gdb_string.h"
#include "buildsym.h"
#include "hashtab.h"
#include "varobj.h" /* APPLE LOCAL: for varobj_delete_objfiles_vars  */
#include "breakpoint.h"
#include "block.h"
#include "dictionary.h"

#include "db-access-functions.h"

#ifdef MACOSX_DYLD
#include "inferior.h"
#include "macosx-nat-dyld.h"
#include "mach-o.h"
#endif

/* Prototypes for local functions */

static void objfile_alloc_data (struct objfile *objfile);
static void objfile_free_data (struct objfile *objfile);

struct objfile *create_objfile (bfd *abfd);
/* APPLE LOCAL: in place objfile rebuilding.  */
struct objfile *create_objfile_using_objfile (struct objfile *objfile, bfd *abfd);

static void objfile_remove_from_restrict_list (struct objfile *);

/* Variables to make obsolete commands available.  */
static char *cached_symfile_path = NULL;
int mapped_symbol_files = 0;
int use_mapped_symbol_files = 0;  // Temporarily disable jmolenda 2004-05-13

extern struct cmd_list_element *setshliblist;
extern struct cmd_list_element *showshliblist;
extern struct cmd_list_element *infoshliblist;
extern struct cmd_list_element *shliblist;

/* Externally visible variables that are owned by this module.
   See declarations in objfile.h for more info. */

#ifndef TEXT_SECTION_NAME
#define TEXT_SECTION_NAME ".text"
#endif

#ifndef DATA_SECTION_NAME
#define DATA_SECTION_NAME ".data"
#endif

struct objfile *object_files;	/* Linked list of all objfiles */
struct objfile *current_objfile;	/* For symbol file being read in */
struct objfile *symfile_objfile;	/* Main symbol table loaded from */
struct objfile *rt_common_objfile;	/* For runtime common symbols */

/* Locate all mappable sections of a BFD file.
   objfile_p_char is a char * to get it through
   bfd_map_over_sections; we cast it back to its proper type.  */

#ifndef TARGET_KEEP_SECTION
#define TARGET_KEEP_SECTION(ASECT)      0
#endif

/* APPLE LOCAL - with the advent of ZeroLink, it is not uncommon for Mac OS X 
   applications to consist of 500+ shared libraries.  At that point searching
   linearly for address->obj_section becomes very costly, and it is a common
   operation.  So we maintain an ordered array of obj_sections, and use that
   to do a binary search for the matching section. 

   N.B. We could just use an array of pointers to the obj_section
   instead of this struct, but this way the malloc'ed array contains
   all the elements we need to do the pc->obj_section lookup, so we
   can do the search only touching a couple of pages of memory, rather
   than wandering all over the heap.  

   FIXME: We really should merge this array with the to_sections array in
   the target, but that doesn't have back-pointers to the obj_section.  I
   am not sure how hard it would be to get that working.  This is simpler 
   for now.  */

struct ordered_obj_section
{
  struct obj_section *obj_section;
  struct bfd_section *the_bfd_section;
  CORE_ADDR addr;
  CORE_ADDR endaddr;
};

/* This is the table of ordered_sections.  It is malloc'ed to
   ORDERED_SECTIONS_CHUNK_SIZE originally, and then realloc'ed if we
   get more sections. */

static struct ordered_obj_section *ordered_sections;

/* This is the number of entries currently in the ordered_sections table.  */
static int num_ordered_sections = 0;

#define ORDERED_SECTIONS_CHUNK_SIZE 3000

static int max_num_ordered_sections = ORDERED_SECTIONS_CHUNK_SIZE;

static int find_in_ordered_sections_index (CORE_ADDR addr, struct bfd_section *bfd_section);
static int get_insert_index_in_ordered_sections (struct obj_section *section);

/* When we want to add a bunch of obj_sections at a time, we will
   find all the insert points, make an array of type OJB_SECTION_WITH_INDEX
   sort that in reverse order, then sweep the ordered_sections list
   shifting and adding them all.  */

struct obj_section_with_index
{
  int index;
  struct obj_section *section;
};

#if 0 /* APPLE LOCAL unused */
/* Called via bfd_map_over_sections to build up the section table that
   the objfile references.  The objfile contains pointers to the start
   of the table (objfile->sections) and to the first location after
   the end of the table (objfile->sections_end). */

static void
add_to_objfile_sections (struct bfd *abfd, struct bfd_section *asect,
			 void *objfile_p_char)
{
  struct objfile *objfile = (struct objfile *) objfile_p_char;
  struct obj_section section;
  flagword aflag;

  aflag = bfd_get_section_flags (abfd, asect);

  if (!(aflag & SEC_ALLOC) && !(TARGET_KEEP_SECTION (asect)))
    return;

  if (0 == bfd_section_size (abfd, asect))
    return;
  section.offset = 0;
  section.objfile = objfile;
  section.the_bfd_section = asect;
  section.ovly_mapped = 0;
  section.addr = bfd_section_vma (abfd, asect);
  section.endaddr = section.addr + bfd_section_size (abfd, asect);
  obstack_grow (&objfile->objfile_obstack, (char *) &section, sizeof (section));
  objfile->sections_end = (struct obj_section *) (((unsigned long) objfile->sections_end) + 1);
}
#endif /* APPLE LOCAL unused */

/* APPLE LOCAL: The macosx code builds a set of section offsets
   when it is doing the dyld read.  It needs to only include in those
   offsets the bfd sections that are actually going into the objfile,
   or the section won't be laid out correctly.  So I extracted the 
   code that does the test in build_objfile_section_table, so we can
   use it in both places.  */

int
objfile_keeps_section (bfd *abfd, asection *asect)
{
  flagword aflag;
  
  aflag = bfd_get_section_flags (abfd, asect);
  
  if (!(aflag & SEC_ALLOC) && !(TARGET_KEEP_SECTION (asect)))
    return 0;
  
  if (0 == bfd_section_size (abfd, asect))
    return 0;
  return 1;
}

/* Builds a section table for OBJFILE.
   Returns 0 if OK, 1 on error (in which case bfd_error contains the
   error).

   Note that while we are building the table, which goes into the
   psymbol obstack, we hijack the sections_end pointer to instead hold
   a count of the number of sections.  When bfd_map_over_sections
   returns, this count is used to compute the pointer to the end of
   the sections table, which then overwrites the count.

   Also note that the OFFSET and OVLY_MAPPED in each table entry
   are initialized to zero.

   Also note that if anything else writes to the psymbol obstack while
   we are building the table, we're pretty much hosed. */

int
build_objfile_section_table (struct objfile *objfile)
{
  asection *asect;
  unsigned int i = 0;
  /* APPLE LOCAL: For separate debug objfiles, we need to use the main
     symfile's bfd sections to build the section table.  We want to be
     able to ask the debug_objfile what the SECT_OFFSET_TEXT is, and
     the dSYM sections don't know that...  So we would like to do something
     like the following.  BUT... unfortunately, when we get called in
     here for the dSYM, the backlinks haven't been set up yet.  So we
     can't get our hands on the main objfile, and this is a no-op.  FIXME  */
  bfd *abfd;
  if (objfile->separate_debug_objfile_backlink == NULL)
    abfd = objfile->obfd;
  else
    abfd = objfile->separate_debug_objfile_backlink->obfd;

  i = 0;
  for (asect = abfd->sections; asect != NULL; asect = asect->next)
    i++;

  objfile->sections = xmalloc (sizeof (struct obj_section) * i);
  objfile->sections_end = objfile->sections;

  i = 0;
  for (asect = abfd->sections; asect != NULL; asect = asect->next)
    {
      struct obj_section section;

      if (!objfile_keeps_section (abfd, asect))
	continue;

      section.offset = 0;
      section.objfile = objfile;
      section.the_bfd_section = asect;
      section.ovly_mapped = 0;
      section.addr = bfd_section_vma (abfd, asect);
      section.endaddr = section.addr + bfd_section_size (abfd, asect);

      objfile->sections[i++] = section;
      objfile->sections_end = objfile->sections + i;
    }

  objfile_add_to_ordered_sections (objfile);

  return 0;
}

/* Given a pointer to an initialized bfd (ABFD) and some flag bits
   allocate a new objfile struct, fill it in as best we can, link it
   into the list of all known objfiles, and return a pointer to the
   new objfile struct.

   The FLAGS word contains various bits (OBJF_*) that can be taken as
   requests for specific operations.  Other bits like OBJF_SHARED are
   simply copied through to the new objfile flags member. */

/* NOTE: carlton/2003-02-04: This function is called with args NULL, 0
   by jv-lang.c, to create an artificial objfile used to hold
   information about dynamically-loaded Java classes.  Unfortunately,
   that branch of this function doesn't get tested very frequently, so
   it's prone to breakage.  (E.g. at one time the name was set to NULL
   in that situation, which broke a loop over all names in the dynamic
   library loader.)  If you change this function, please try to leave
   things in a consistent state even if abfd is NULL.  */

/* APPLE LOCAL: I added a version that allows you to replace the
   contents of an objfile that's already been allocated.  We need this
   because when you change the symbol load level, it's best not to
   delete the objfile you were looking at and replace.  And the reason
   for that's a curious reason - when you raise the symbol load level
   on an objfile, we need to fix it's commpage objfile at the same
   time, but taking two objfiles out of the list at the same time
   makes ALL_OBJFILES_SAFE no longer safe.  So it's better to edit the
   objfiles in place.  */

struct objfile *
allocate_objfile_internal (struct objfile *objfile,
			   bfd *abfd, int flags, 
			   int symflags, CORE_ADDR mapaddr,
			   const char *prefix)
{
  
  objfile->symflags = symflags;
  objfile->flags |= flags;
  
  /* Update the per-objfile information that comes from the bfd, ensuring
     that any data that is reference is saved in the per-objfile data
     region. */
  
  objfile->obfd = abfd;
  if (objfile->name)
    objfile->name = xstrdup (objfile->name);
  else
    objfile->name = xstrdup (bfd_get_filename (abfd));
  objfile->mtime = bfd_get_mtime (abfd);
  
  if (build_objfile_section_table (objfile))
    error ("Can't find the file sections in `%s': %s",
	   objfile->name, bfd_errmsg (bfd_get_error ()));
  
  /* FIXME: At some point this should be a host specific callout.
     Even though this is a Mac OS X specific copy of allocate_objfile,
     we should still fix this when we fix that...  */
  
  if (objfile->name != NULL &&
      strstr (objfile->name, "libSystem") != NULL)
    objfile->check_for_equivalence = 1;
  else
    objfile->check_for_equivalence = 0;
  objfile->equivalence_table = NULL;
  
  objfile->syms_only_objfile = 0;

  objfile->not_loaded_kext_filename = NULL;
  
  return (objfile);
}

struct objfile *
allocate_objfile (bfd *abfd, int flags, int symflags, CORE_ADDR mapaddr,
                  const char *prefix)
{
  struct objfile *objfile = NULL;

  objfile = create_objfile (abfd);
  objfile = allocate_objfile_internal (objfile, abfd, flags, 
				    symflags, mapaddr, prefix);
  link_objfile (objfile);
  return objfile;
}

struct objfile *
allocate_objfile_using_objfile (struct objfile *objfile,
			       bfd *abfd, int flags, 
			       int symflags, CORE_ADDR mapaddr,
			       const char *prefix)
{
  create_objfile_using_objfile (objfile, abfd);
  return allocate_objfile_internal (objfile, abfd, flags, 
				    symflags, mapaddr, prefix);
}

struct objfile *
create_objfile (bfd *abfd)
{
  struct objfile *objfile;

  objfile = (struct objfile *) xmalloc (sizeof (struct objfile));
  memset (objfile, 0, sizeof (struct objfile));
  return create_objfile_using_objfile (objfile, abfd);

}

struct objfile *
create_objfile_using_objfile (struct objfile *objfile, bfd *abfd)
{
  objfile->md = NULL;
  objfile->psymbol_cache = bcache_xmalloc (NULL);
  objfile->macro_cache = bcache_xmalloc (NULL);
  bcache_specify_allocation (objfile->psymbol_cache, xmalloc, xfree);
  bcache_specify_allocation (objfile->macro_cache, xmalloc, xfree);
  obstack_specify_allocation (&objfile->objfile_obstack, 0, 0, xmalloc,
                              xfree);

  /* FIXME: This needs to be converted to use objfile-specific data. */
  objfile_alloc_data (objfile);

  /* Initialize the section indexes for this objfile, so that we can
     later detect if they are used w/o being properly assigned to. */

  objfile->sect_index_text = -1;
  objfile->sect_index_data = -1;
  objfile->sect_index_bss = -1;
  objfile->sect_index_rodata = -1;

  /* APPLE LOCAL begin dwarf repository  */
  objfile->uses_sql_repository = 0;
  /* APPLE LOCAL end dwarf repository  */

  return objfile;
}

/* This is the quicksort compare routine for OBJ_SECTION_WITH_INDEX
   objects.  It sorts by section start address in decreasing 
   order.  */

static int
backward_section_compare (const void *left_ptr, 
		 const void *right_ptr)
{
  
  struct obj_section_with_index *left 
    = (struct obj_section_with_index *) left_ptr;
  struct obj_section_with_index * right 
    = (struct obj_section_with_index *) right_ptr;

  if (left->section->addr > right->section->addr)
    return -1;
  else if (left->section->addr < right->section->addr)
    return 1;
  else
      return 0;
}

/* Simple integer compare for quicksort */

static int
forward_int_compare (const void *left_ptr, 
		 const void *right_ptr)
{
  int *left = (int *) left_ptr;
  int *right = (int *) right_ptr;

  if (*left < *right)
    return -1;
  else if (*left > *right)
    return 1;
  else
      return 0;
}

/* APPLE LOCAL: The difference between the segment names and the section
   names is the segment names always only have one dot.  Use this to count
   the dots quickly...  */
static int
number_of_dots (const char *s)
{
  int numdots = 0;
  while (*s != '\0')
    {
      if (*s == '.')
	numdots++;
      s++;
    }
  return numdots;
}

/* Delete all the obj_sections in OBJFILE from the ordered_sections
   global list.  N.B. this routine uses the addresses in the sections
   in the objfile to find the entries in the ordered_sections list, 
   so if you are going to relocate the obj_sections in an objfile,
   call this BEFORE you relocate, then relocate, then call 
   objfile_add_to_ordered_sections.  */

#define STATIC_DELETE_LIST_SIZE 256

void 
objfile_delete_from_ordered_sections (struct objfile *objfile)
{
  int i;
  int ndeleted;
  int delete_list_size;
  int static_delete_list[STATIC_DELETE_LIST_SIZE];
  int *delete_list = static_delete_list;
  
  struct obj_section *s;
  
  /* APPLE LOCAL: we need to check if this is a separate debug files and try to 
     remove the sections to the ordered list if so. The backlink will not be
     setup when the separate debug objfile is in the process of being created, 
     so a flag was added to make sure we can tell.  */
  if (objfile->separate_debug_objfile_backlink || 
      objfile->flags & OBJF_SEPARATE_DEBUG_FILE)
	return;	

  /* Do deletion of the sections by building up an array of
     "to be removed" indices, and then block compact the array using
     these indices.  */
  
  delete_list_size = objfile->sections_end - objfile->sections;
  if (delete_list_size > STATIC_DELETE_LIST_SIZE)
    delete_list = (int *) xmalloc (delete_list_size * sizeof (int));

  ndeleted = 0;

  ALL_OBJFILE_OSECTIONS (objfile, s)
    {
      int index;
      /* APPLE LOCAL: Oh, hacky, hacky...  The bfd Mach-O reader makes
         bfd_sections for both the sections & segments (the container of
         the sections).  This would make pc->bfd_section lookup non-unique.
         so we just drop the segments from our list.  */
      if (s->the_bfd_section && s->the_bfd_section->name &&
          number_of_dots (s->the_bfd_section->name) == 1)
        continue;

      index = find_in_ordered_sections_index (s->addr, s->the_bfd_section);
      if (index == -1)
	warning ("Trying to remove a section from"
			" the ordered section list that did not exist"
			" at 0x%s.", paddr_nz (s->addr));
      else
	{
	  delete_list[ndeleted] = index;
	  ndeleted++;
	}
    }
  qsort (delete_list, ndeleted, sizeof (int),
	 forward_int_compare);

  /* Stick a boundary on the end of the delete list - it makes the block
     shuffle algorithm easier.  I also have to set the boundary to one
     past the list end because it is supposed to be the next element
     deleted, and I don't want to delete the last element.  */

  delete_list[ndeleted] = num_ordered_sections;

  for (i = 0; i < ndeleted; i++)
    {
      struct ordered_obj_section *src, *dest;
      size_t len;

      src = &(ordered_sections[delete_list[i] + 1]);
      dest = &(ordered_sections[delete_list[i] - i]);
      len = (delete_list[i+1] - delete_list[i] - 1)
	* sizeof (struct ordered_obj_section);

      bcopy (src, dest, len);
    }

  
  num_ordered_sections -= ndeleted;
  if (delete_list != static_delete_list)
    xfree (delete_list);
}

/* This returns the index before which the obj_section SECTION
   should be added in the ordered_sections list.  This only sorts
   by addr, and pays no attention to endaddr, or the_bfd_section.  */

static int
get_insert_index_in_ordered_sections (struct obj_section *section)
{
  int insert = -1;

  if (num_ordered_sections > 0)
    {
      int bot = 0;
      int top = num_ordered_sections - 1;

      do
	{
	  int mid = (top + bot) / 2;

	  if (ordered_sections[mid].addr < section->addr)
	    {
	      if ((mid == num_ordered_sections - 1) 
		  || (ordered_sections[mid + 1].addr >= section->addr))
		{
		  insert = mid;
		  break;
		}
	      else if (mid + 1 == num_ordered_sections - 1)
		{
		  insert = mid + 1;
		  break;
		}
	      bot = mid + 1;
	    }
	  else
	    {
	      if (mid == 0 || ordered_sections[mid - 1].addr <= section->addr)
		{
		  insert = mid - 1;
		  break;
		}
	      top = mid - 1;
	    }
	} while (top > bot);
    }

  return insert + 1;
}

/* This adds all the obj_sections for OBJFILE to the ordered_sections
   array */

#define STATIC_INSERT_LIST_SIZE 256

void
objfile_add_to_ordered_sections (struct objfile *objfile)
{
  struct obj_section *s;
  int i, num_left, total;
  struct obj_section_with_index static_insert_list[STATIC_INSERT_LIST_SIZE];
  struct obj_section_with_index *insert_list = static_insert_list;
  int insert_list_size;

  /* APPLE LOCAL: we need to check if this is a separate debug files and not 
     add the sections to the ordered list if so. The backlink will not be setup
     when the separate debug objfile is in the process of being created, so a 
     flag was added to make sure it never gets added.  */
  if (objfile->separate_debug_objfile_backlink || 
      objfile->flags & OBJF_SEPARATE_DEBUG_FILE)
	return;
	
  CHECK_FATAL (objfile != NULL);

  /* First find the index for insertion of all the sections in
     this objfile.  The sort that array in reverse order by address,
     then go through the ordered list block moving the bits between
     the insert points, then adding the pieces we need to add.  */

  insert_list_size = objfile->sections_end - objfile->sections;
  if (insert_list_size > STATIC_INSERT_LIST_SIZE)
    insert_list = (struct obj_section_with_index *) 
      xmalloc (insert_list_size * sizeof (struct obj_section_with_index));

  total = 0;
  ALL_OBJFILE_OSECTIONS (objfile, s)
    {
      /* APPLE LOCAL: Oh, hacky, hacky...  The bfd Mach-O reader makes
         bfd_sections for both the sections & segments (the container of
         the sections).  This would make pc->bfd_section lookup non-unique.
         so we just drop the segments from our list.  */
      if (s->the_bfd_section && s->the_bfd_section->segment_mark == 1)
        continue;

      insert_list[total].index = get_insert_index_in_ordered_sections (s);
      insert_list[total].section = s;
      total++;
    }

  qsort (insert_list, total, sizeof (struct obj_section_with_index), 
	 backward_section_compare);

  /* Grow the array if needed */

  if (ordered_sections == NULL)
    {
      max_num_ordered_sections = ORDERED_SECTIONS_CHUNK_SIZE;
      ordered_sections = (struct ordered_obj_section *)
	xmalloc (max_num_ordered_sections 
		 * sizeof (struct ordered_obj_section));
    }
  else if (num_ordered_sections + total >= max_num_ordered_sections)
    {
      /* TOTAL should be small, but on the off chance that it is not, 
	 just add it to the allocation as well.  */
      max_num_ordered_sections += ORDERED_SECTIONS_CHUNK_SIZE + total;
      ordered_sections = (struct ordered_obj_section *) 
	xrealloc (ordered_sections, 
		  max_num_ordered_sections 
		  * sizeof (struct ordered_obj_section));
    }
  
  num_left = total;

  for (i = 0; i < total; i++)
    {
      struct ordered_obj_section *src;
      struct ordered_obj_section *dest;
      struct obj_section *s;
      size_t len;
      int pos;

      src = &(ordered_sections[insert_list[i].index]);
      dest = &(ordered_sections[insert_list[i].index + num_left]);

      if (i == 0)
	len = num_ordered_sections - insert_list[i].index;
      else
	len = insert_list[i - 1].index - insert_list[i].index;
      len *= sizeof (struct ordered_obj_section);
      bcopy (src, dest, len);

      /* Now put the new element in place */
      s = insert_list[i].section;
      pos = insert_list[i].index + num_left - 1;
      ordered_sections[pos].addr = s->addr;
      ordered_sections[pos].endaddr = s->endaddr;
      ordered_sections[pos].obj_section = s;
      ordered_sections[pos].the_bfd_section = s->the_bfd_section;

      num_left--;
      if (num_left == 0)
	break;
    }

  num_ordered_sections += total;
  if (insert_list != static_insert_list)
    xfree (insert_list);

}

/* This returns the index in the ordered_sections array corresponding
   to the pair ADDR, BFD_SECTION (can be null), or -1 if not found. */

static int
find_in_ordered_sections_index (CORE_ADDR addr, struct bfd_section *bfd_section)
{
  int bot = 0;
  int top = num_ordered_sections;
  struct ordered_obj_section *obj_sect_ptr;

  if (num_ordered_sections == 0)
    return -1;

  do 
    {
      int mid = (top + bot) / 2;

      obj_sect_ptr = &ordered_sections[mid]; 
      if (obj_sect_ptr->addr <= addr)
	{
	  if (addr < obj_sect_ptr->endaddr)
	    {
	      /* It is possible that the sections overlap.  This will
		 happen in two cases that I know of.  One is when you
		 have not run the app yet, so that a bunch of the
		 sections are still mapped at 0, and haven't been.
		 relocated yet.  The other is because on MacOS X we (I
		 think errantly) make sections both for the segment
		 command, and for the sections it contains.  In the former
	         case, this becomes a linear search just like the original
	         algorithm.  In the latter, we should find it in the
	         near neighborhood pretty soon.  */

	      if ((bfd_section == NULL )
		  || (bfd_section == obj_sect_ptr->the_bfd_section))
		return mid;
	      else
		{
		  /* So to find the containing element, we can look
		     to increasing indices, till the start address
		     of our element is above the addr, but we have to go
		     all the way to the left to find the address. */

		  int pos;
		  for (pos = mid + 1; pos < num_ordered_sections; pos++)
		    {
		      if (ordered_sections[pos].addr > addr)
			break;
		      else if ((ordered_sections[pos].endaddr > addr)
			       && (ordered_sections[pos].the_bfd_section == bfd_section))
			return pos;
		    }
		  for (pos = mid - 1; pos >= 0; pos--)
		    {
		      if ((ordered_sections[pos].endaddr > addr)
			  && (ordered_sections[pos].the_bfd_section == bfd_section))
			return pos;
		    }

		  /* If we don't find one that matches BOTH the address and the 
		     section, then return -1.  Callers should be prepared to either
		     fail in this case, or to look more broadly.  */
		  return -1;
		}
	    }
	  bot = mid + 1;
	}
      else
	{
	  top = mid - 1;
	}
    } while (bot <= top);

  /* If we got here, we didn't find anything, so return -1. */
  return -1;
}

/* This returns the obj_section corresponding to the pair ADDR and
   BFD_SECTION (can be NULL) in the ordered sections array, or NULL
   if not found.  */

struct obj_section *
find_pc_sect_in_ordered_sections (CORE_ADDR addr, struct bfd_section *bfd_section)
{
  int index = find_in_ordered_sections_index (addr, bfd_section);
  
  if (index == -1)
    return NULL;
  else
    return ordered_sections[index].obj_section;
}

/* Initialize entry point information for this objfile. */

void
init_entry_point_info (struct objfile *objfile)
{
  /* Save startup file's range of PC addresses to help blockframe.c
     decide where the bottom of the stack is.  */

  if (bfd_get_file_flags (objfile->obfd) & EXEC_P)
    {
      /* Executable file -- record its entry point so we'll recognize
         the startup file because it contains the entry point.  */
      objfile->ei.entry_point = bfd_get_start_address (objfile->obfd);
    }
  else
    {
      /* Examination of non-executable.o files.  Short-circuit this stuff.  */
      objfile->ei.entry_point = INVALID_ENTRY_POINT;
    }

  /* APPLE LOCAL: Initialize main_func_lowpc and main_func_highpc. */
  objfile->ei.main_func_lowpc = INVALID_ENTRY_LOWPC;
  objfile->ei.main_func_highpc = INVALID_ENTRY_HIGHPC;
}

/* Get current entry point address.  */

CORE_ADDR
entry_point_address (void)
{
  return symfile_objfile ? symfile_objfile->ei.entry_point : 0;
}

/* Create the terminating entry of OBJFILE's minimal symbol table.
   If OBJFILE->msymbols is zero, allocate a single entry from
   OBJFILE->objfile_obstack; otherwise, just initialize
   OBJFILE->msymbols[OBJFILE->minimal_symbol_count].  */
void
terminate_minimal_symbol_table (struct objfile *objfile)
{
  if (! objfile->msymbols)
    objfile->msymbols = ((struct minimal_symbol *)
                         obstack_alloc (&objfile->objfile_obstack,
                                        sizeof (objfile->msymbols[0])));

  {
    struct minimal_symbol *m
      = &objfile->msymbols[objfile->minimal_symbol_count];

    memset (m, 0, sizeof (*m));
    /* Don't rely on these enumeration values being 0's.  */
    MSYMBOL_TYPE (m) = mst_unknown;
    SYMBOL_INIT_LANGUAGE_SPECIFIC (m, language_unknown);
  }
}


/* Put one object file before a specified on in the global list.
   This can be used to make sure an object file is destroyed before
   another when using ALL_OBJFILES_SAFE to free all objfiles. */
void
put_objfile_before (struct objfile *objfile, struct objfile *before_this)
{
  struct objfile **objp;

  unlink_objfile (objfile);
  
  for (objp = &object_files; *objp != NULL; objp = &((*objp)->next))
    {
      if (*objp == before_this)
	{
	  objfile->next = *objp;
	  *objp = objfile;
	  return;
	}
    }
  
  internal_error (__FILE__, __LINE__,
		  _("put_objfile_before: before objfile not in list"));
}

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

/* Link OBJFILE into the list of known objfiles.  It is an error if
   OBJFILE has a non-zero NEXT pointer before being inserted. */

void
link_objfile (struct objfile *objfile)
{
  struct objfile *last_one = NULL;
  struct objfile *o, *temp;

  ALL_OBJFILES_SAFE (o, temp)
    if (objfile == o)
      internal_error (__FILE__, __LINE__,
                     "link_objfile: objfile already linked");

  if (objfile->next != NULL)
    internal_error (__FILE__, __LINE__,
		    "link_objfile: objfile already linked");

  /* Add this file onto the tail of the linked list of other such files. */

  objfile->next = NULL;
  if (object_files == NULL)
    object_files = objfile;
  else
    {
      for (last_one = object_files;
	   last_one->next;
	   last_one = last_one->next);
      last_one->next = objfile;
    }
}

/* Unlink OBJFILE from the list of known objfiles, clearing its NEXT
   pointer.  It is an error if OBJFILE is not on the list of known
   objfiles. */

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
		  _("unlink_objfile: objfile already unlinked"));
}


/* APPLE LOCAL: Factor out the common bits for
 free_objfile and clear_objfile.  */

static void
free_objfile_internal (struct objfile *objfile)
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


  /* Now remove the varobj's that depend on this objfile.  */
  varobj_delete_objfiles_vars (objfile);

  /* APPLE LOCAL: Remove all the obj_sections in this objfile from the
     ordered_sections list.  Do this before deleting the bfd, since
     we need to use the bfd_sections to do it.  */
  
  objfile_delete_from_ordered_sections (objfile);

  /* We always close the bfd. */

  if (objfile->obfd != NULL)
    {
      char *name = bfd_get_filename (objfile->obfd);
      /* APPLE LOCAL: we have to remove the bfd from the
	 target's to_sections before we close it.  */

      remove_target_sections (objfile->obfd);

      if (!bfd_close (objfile->obfd))
	warning (_("cannot close \"%s\": %s"),
		 name, bfd_errmsg (bfd_get_error ()));
      xfree (name);
    }

  /* APPLE LOCAL: Remove it from the chain of restricted objfiles.  */

  objfile_remove_from_restrict_list (objfile);

  /* APPLE LOCAL: Delete the equivalence table dingus.  */
  equivalence_table_delete (objfile);

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

  /* I *think* all our callers call clear_symtab_users.  If so, no need
     to call this here.  */
  clear_pc_function_cache ();

  /* The last thing we do is free the objfile struct itself. */

  objfile_free_data (objfile);
  if (objfile->name != NULL)
    {
      xfree (objfile->name);
    }
  if (objfile->global_psymbols.list)
    xfree (objfile->global_psymbols.list);
  if (objfile->static_psymbols.list)
    xfree (objfile->static_psymbols.list);
  /* Free the obstacks for non-reusable objfiles */
  bcache_xfree (objfile->psymbol_cache);
  bcache_xfree (objfile->macro_cache);
  /* APPLE LOCAL: Also free up the table of "equivalent symbols".  */
  equivalence_table_delete (objfile);
  /* END APPLE LOCAL */
  if (objfile->demangled_names_hash)
    htab_delete (objfile->demangled_names_hash);
  obstack_free (&objfile->objfile_obstack, 0);
  /* APPLE LOCAL begin dwarf repository  */
  if (objfile->uses_sql_repository)
    close_dwarf_repositories (objfile);
  /* APPLE LOCAL end dwarf repository  */

  /* APPLE LOCAL begin subroutine inlining  */
  if (objfile->inlined_subroutine_data)
    inlined_subroutine_free_objfile_data (objfile->inlined_subroutine_data);
  if (objfile->inlined_call_sites)
    inlined_subroutine_free_objfile_call_sites (objfile->inlined_call_sites);
  /* APPLE LOCAL end subroutine inlining  */

  /* Can't tell whether one of the sections in this objfile is
     one of the one's we've cached over in symfile.c, so let's clear
     it here to be safe.  */
  symtab_clear_cached_lookup_values ();

}

/* APPLE LOCAL: clear_objfile deletes all the data
   associated with OBJFILE, but keeps all the links in
   place, so you can edit an objfile (for instance raise
   its load level) IN PLACE in the objfile chain.  Note,
   if you use clear_objfile, and the objfile has a 
   separate_debug_objfile, you need to reconstitute it in
   place as well.  To do this, use allocate_objfile_with_objfile,
   and then read in the new objfiles.  */

void
clear_objfile (struct objfile *objfile)
{
  struct objfile *next_tmp, *separate_tmp, *backlink_tmp;
  /* FIXME - THis is not the right way to treat the
     separate debug objfile!!! */

  if (objfile->separate_debug_objfile)
    {
      clear_objfile (objfile->separate_debug_objfile);
    }

  free_objfile_internal (objfile);  
  /* Since we are going to reuse this, make sure we clean it
     out, but don't nuke the various links, since clear_objfile
     is specifically for editing in place.  */

  next_tmp = objfile->next;
  separate_tmp = objfile->separate_debug_objfile;
  backlink_tmp = objfile->separate_debug_objfile_backlink;
  memset (objfile, 0, sizeof (struct objfile));
  objfile->next = next_tmp;
  objfile->separate_debug_objfile = separate_tmp;
  objfile->separate_debug_objfile_backlink = backlink_tmp;
}

/* Destroy an objfile and all the symtabs and psymtabs under it.  Note
   that as much as possible is allocated on the objfile_obstack 
   so that the memory can be efficiently freed.

   Things which we do NOT free because they are not in malloc'd memory
   or not in memory specific to the objfile include:

   objfile -> sf

   FIXME:  If the objfile is using reusable symbol information (via mmalloc),
   then we need to take into account the fact that more than one process
   may be using the symbol information at the same time (when mmalloc is
   extended to support cooperative locking).  When more than one process
   is using the mapped symbol info, we need to be more careful about when
   we free objects in the reusable area. */

/* APPLE LOCAL: Factored out to use free_objfile_internal.  */
void
free_objfile (struct objfile *objfile)
{
  /* APPLE LOCAL: Give a chance for any breakpoints to mark themselves as
     pending with the name of the objfile kept around for accurate 
     re-setting. */
  tell_breakpoints_objfile_removed (objfile);

  if (objfile->separate_debug_objfile)
    {
      free_objfile (objfile->separate_debug_objfile);
      objfile->separate_debug_objfile = NULL;
    }

  if (objfile->separate_debug_objfile_backlink)
    {
      /* We freed the separate debug file, make sure the base objfile
	 doesn't reference it.  */
      objfile->separate_debug_objfile_backlink->separate_debug_objfile = NULL;
    }
  
  free_objfile_internal (objfile);
  
  /* Remove it from the chain of all objfiles. */

  unlink_objfile (objfile);

  xfree (objfile);
  objfile = NULL;
}
/* END APPLE LOCAL  */

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
    ((struct section_offsets *) 
     alloca (SIZEOF_N_SECTION_OFFSETS (objfile->num_sections)));

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
  
  breakpoints_relocate (objfile, delta);
  /* APPLE LOCAL begin subroutine inlining  */
  /* Update all the inlined subroutine data for this objfile.  */
  inlined_subroutine_objfile_relocate (objfile,
				       objfile->inlined_subroutine_data,
				       delta);
  /* APPLE LOCAL end subroutine inlining  */

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
	  unsigned int num_discontinuities = 0;
	  int discontinuity_index = -1;

	  for (i = 0; i < l->nitems; ++i)
	    {
	      l->item[i].pc += ANOFFSET (delta, s->block_line_section);
	      if (l->item[i].end_pc != 0)
		l->item[i].end_pc += ANOFFSET (delta, s->block_line_section);
	    }

	  /* Re-sort the line-table.  The table should have started
	     off sorted, so we should be able to re-sort it by
	     rotating the values in the buffer.  */

	  for (i = 0; i < (l->nitems - 1); ++i)
	    if (l->item[i].pc > l->item[i + 1].pc)
	      {
		num_discontinuities++;
		discontinuity_index = i + 1;
	      }
	  
	  if (num_discontinuities == 1)
	    {
	      struct linetable *new_linetable = NULL;
	      size_t size = ((l->nitems - 1) * sizeof (struct linetable_entry))
		+ sizeof (struct linetable);
	      
	      new_linetable = (struct linetable *) xmalloc (size);
	      memcpy (new_linetable, l, sizeof (struct linetable));
	      memcpy (new_linetable->item,
		      l->item + discontinuity_index,
		      (l->nitems - discontinuity_index) * sizeof (struct linetable_entry));
	      memcpy (new_linetable->item + (l->nitems - discontinuity_index),
		      l->item,
		      discontinuity_index * sizeof (struct linetable_entry));
	      memcpy (l->item, new_linetable->item, l->nitems * sizeof (struct linetable_entry));

	      xfree (new_linetable);
	    }
	  else if (num_discontinuities > 0)
	    {
	      warning ("line table was not properly sorted; re-sorting");
	      qsort (l->item, l->nitems, sizeof (struct linetable_entry), compare_line_numbers);
	    }
	}
      
      /* Don't relocate a shared blockvector more than once.  */
      if (!s->primary)
	continue;

      bv = BLOCKVECTOR (s);
      for (i = 0; i < BLOCKVECTOR_NBLOCKS (bv); ++i)
	{
	  struct block *b;
	  struct symbol *sym;
	  struct dict_iterator iter;

	  b = BLOCKVECTOR_BLOCK (bv, i);
	  BLOCK_START (b) += ANOFFSET (delta, s->block_line_section);
	  BLOCK_END (b) += ANOFFSET (delta, s->block_line_section);
	  /* APPLE LOCAL begin address ranges  */
	  if (BLOCK_RANGES (b))
	    {
	      int j;
	      for (j = 0; j < BLOCK_RANGES (b)->nelts; j++)
		{
		  BLOCK_RANGE_START (b, j) +=  ANOFFSET (delta, 
							 s->block_line_section);
		  BLOCK_RANGE_END (b, j) +=  ANOFFSET (delta, 
						       s->block_line_section);
		}
	    }
	  /* APPLE LOCAL end address ranges  */

	  ALL_BLOCK_SYMBOLS (b, iter, sym)
	    {
	      fixup_symbol_section (sym, objfile);

	      /* The RS6000 code from which this was taken skipped
	         any symbols in STRUCT_DOMAIN or UNDEF_DOMAIN.
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

  /* APPLE LOCAL: We use these addresses to determine whether minsyms'
     text segments (__TEXT vs coalesced for instance) so we need to keep them
     up to date along with any slides that happen to the objfile.  */
  DBX_TEXT_ADDR (objfile) += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
  if (DBX_COALESCED_TEXT_ADDR (objfile) != 0)
    {
      DBX_COALESCED_TEXT_ADDR (objfile) += ANOFFSET (delta, 
                                                     SECT_OFF_TEXT (objfile));
    }

  {
    struct obj_section *s;
    bfd *abfd;
    int idx = 0;

    abfd = objfile->obfd;

    objfile_delete_from_ordered_sections (objfile);

    ALL_OBJFILE_OSECTIONS (objfile, s)
      {
	s->addr += ANOFFSET (delta, idx);
	s->endaddr += ANOFFSET (delta, idx);
	idx++;
      }

    objfile_add_to_ordered_sections (objfile);
  }

  /* Relocate breakpoints as necessary, after things are relocated. */
  breakpoint_re_set (objfile);
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
    /* APPLE LOCAL let dyld code handle objfile freeing */
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
    if (ofp->minimal_symbol_count > 0)
      {
	return 1;
      }
  }
  return 0;
}

/* Returns a section whose range includes PC and SECTION, or NULL if
   none found.  Note the distinction between the return type, struct
   obj_section (which is defined in gdb), and the input type "struct
   bfd_section" (which is a bfd-defined data type).  The obj_section
   contains a pointer to the "struct bfd_section".  */

struct obj_section *
find_pc_sect_section (CORE_ADDR pc, struct bfd_section *section)
{
  struct obj_section *s;
  struct objfile *objfile;

  /* APPLE LOCAL begin cache lookup values for improved performance  */
  if (pc == last_sect_section_lookup_pc
      && pc == last_mapped_section_lookup_pc
      && section == cached_mapped_section)
    return cached_sect_section;

  last_sect_section_lookup_pc = pc;

  /* APPLE LOCAL end cache lookup values for improved performance  */

  /* APPLE LOCAL begin search in ordered sections */
  s = find_pc_sect_in_ordered_sections (pc, section);
  if (s != NULL)
  /* APPLE LOCAL begin cache lookup values for improved performance  */
    {
      cached_sect_section = s;
      return (s);
    }
  /* APPLE LOCAL end cache lookup values for improved performance  */
  /* APPLE LOCAL end search in ordered sections */
  
  ALL_OBJSECTIONS (objfile, s)
    if (objfile->separate_debug_objfile_backlink == NULL
        && (section == 0 || section == s->the_bfd_section) 
	&& s->addr <= pc && pc < s->endaddr)
      /* APPLE LOCAL begin cache lookup values for improved performance  */
      {
	cached_sect_section = s;
	return (s);
      }

  last_sect_section_lookup_pc = INVALID_ADDRESS;
  cached_sect_section = NULL;
  /* APPLE LOCAL end cache lookup values for improved performance  */
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
	    && strcmp (s->the_bfd_section->name, ".plt") == 0);
  return (retval);
}

/* Return nonzero if NAME is in the import list of OBJFILE.  Else
   return zero.  */

int
is_in_import_list (char *name, struct objfile *objfile)
{
  int i;

  if (!objfile || !name || !*name)
    return 0;

  for (i = 0; i < objfile->import_list_size; i++)
    if (objfile->import_list[i] && DEPRECATED_STREQ (name, objfile->import_list[i]))
      return 1;
  return 0;
}

/* The _restrict_ functions are part of the mechanism to add
   an iterator to ALL_OBJFILES that can be used to restrict the
   search to newly added objfiles - in particular when breakpoints
   are being reinserted.

   These are non-nesting interators to be used in ALL_OBJFILES
   only.  If you want something fancier, you need to pass in an
   iterator token to get_first, and pass it back to get_next.  */

static struct objfile_list *objfile_list_ptr;
static int restrict_search = 0;
struct objfile_list *objfile_list;

/* Set the flag to tell ALL_OBJFILES whether to restrict the search or
   not.  Returns the old flag value.  */

int
objfile_restrict_search (int on)
{
  int old = restrict_search;
  restrict_search = on;
  return old;
}

/* Add an objfile to the restricted search list.  */

void
objfile_add_to_restrict_list (struct objfile *objfile)
{
  struct objfile_list *new_objfile;

  /* APPLE LOCAL: we need to check if this is a separate debug file and not 
     add it, but add the original objfile file to the restrict list as the 
     objfile_get_first() and objfile_get_next() functions will correctly always
     return the separate debug file first, followed by the original executable 
     file.  */
  if (objfile->separate_debug_objfile_backlink || 
      objfile->flags & OBJF_SEPARATE_DEBUG_FILE)
    {
      /* We need to check for the backlink as it may be NULL if we are in the
         process of creating the separate debug objfile */
      if (objfile->separate_debug_objfile_backlink)
	objfile_add_to_restrict_list (objfile->separate_debug_objfile_backlink);
      return;
    }

  
  /* APPLE LOCAL: First check to make sure the objfile isn't already in 
     the list.  */
  for (new_objfile = objfile_list; 
       new_objfile != NULL;
       new_objfile = new_objfile->next)
    {
      if (new_objfile->objfile == objfile)
	return;
    }

  /* Add the file to the restrict list.  */
  new_objfile = (struct objfile_list *) xmalloc (sizeof (struct objfile_list));
  new_objfile->next = objfile_list;
  new_objfile->objfile = objfile;
  objfile_list = new_objfile;
}

/* Remove an objfile from the restricted search list.  */

static void
objfile_remove_from_restrict_list (struct objfile *objfile)
{
  struct objfile_list **objpp;
  struct objfile_list *i;
  
  for (objpp = &objfile_list; *objpp != NULL; objpp = &((*objpp)->next))
    {
      if ((*objpp)->objfile == objfile)
        {
          i = *objpp;
          *objpp = (*objpp)->next;
          xfree (i);
          return;
        }
    }
}

/* Clear the restricted objfile search list.  */

void
objfile_clear_restrict_list ()
{
  while (objfile_list != NULL)
    {
      struct objfile_list *list_ptr;
      list_ptr = objfile_list;
      objfile_list = list_ptr->next;
      xfree (list_ptr);
    }
}

static struct objfile_list *
objfile_set_restrict_list (struct objfile_list *objlist)
{
  struct objfile_list *tmp_list;

  tmp_list = objfile_list;
  objfile_list = objlist;
  return tmp_list;
}

struct swap_objfile_list_cleanup
{
  struct objfile_list *old_list;
  int restrict_state;
};

void
do_cleanup_restrict_to_objfile (void *arg)
{
  struct swap_objfile_list_cleanup *data =
    (struct swap_objfile_list_cleanup *) arg;
  objfile_clear_restrict_list ();
  objfile_list = data->old_list;
  objfile_restrict_search (data->restrict_state);
}

struct cleanup *
make_cleanup_restrict_to_objfile (struct objfile *objfile)
{
  struct swap_objfile_list_cleanup *data
    = (struct swap_objfile_list_cleanup *) xmalloc (sizeof (struct swap_objfile_list_cleanup));
  data->old_list = objfile_list;
  objfile_list = NULL;
  objfile_add_to_restrict_list (objfile);
  data->restrict_state = objfile_restrict_search (1);
  return make_cleanup (do_cleanup_restrict_to_objfile, (void *) data);
}

/* APPLE LOCAL: Find an objfile called NAME.
   If EXACT, NAME must be a fully qualified, realpathed filename.
   Else we're looking for a end-of-string match, e.g.
    Jabber.FireBundle/Contents/MacOS/Jabber
   or just "Jabber".  */

struct objfile *
find_objfile_by_name (const char *name, int exact)
{
  struct objfile *o, *temp;
  if (name == NULL || *name == '\0')
    return NULL;

  ALL_OBJFILES_SAFE (o, temp)
    {
       enum objfile_matches_name_return r;
       r = objfile_matches_name (o, name);
       if (exact && r == objfile_match_exact)
         return o;
       if (!exact && r == objfile_match_base)
         return o;
    }

  return NULL;
}

/* Same as make_cleanup_restrict_to_objfile, except that instead of
   being given an objfile struct, this function is given an objfile name.  */

struct cleanup *
make_cleanup_restrict_to_objfile_by_name (char *objfile_name)
{
  struct objfile *objfile = NULL;
  struct swap_objfile_list_cleanup *data
    = (struct swap_objfile_list_cleanup *) xmalloc (sizeof (struct swap_objfile_list_cleanup));
  data->old_list = objfile_list;
  objfile_list = NULL;
  objfile = find_objfile_by_name (objfile_name, 1);
  if (objfile)
    {
      objfile_add_to_restrict_list (objfile);
      data->restrict_state = objfile_restrict_search (1);
      return make_cleanup (do_cleanup_restrict_to_objfile, (void *) data);
    }
  else
    return make_cleanup (null_cleanup, NULL);
}
/* APPLE LOCAL end radar 5273932  */

struct cleanup *
make_cleanup_restrict_to_objfile_list (struct objfile_list *objlist)
{
  struct swap_objfile_list_cleanup *data
    = (struct swap_objfile_list_cleanup *) xmalloc (sizeof (struct swap_objfile_list_cleanup));
  data->old_list = objfile_set_restrict_list (objlist);
  data->restrict_state = objfile_restrict_search (1);
  return make_cleanup (do_cleanup_restrict_to_objfile, (void *) data);
}

/* Check whether the OBJFILE matches NAME.  We want to match either the
   full name, or the base name.  We also want to handle the case where
   OBJFILE comes from a cached symfile.  In that case, the OBJFILE name
   will be the cached symfile name, but the real shlib name will be in
   the OBFD for the OBJFILE.  So in the case of a cached symfile
   we match against the bfd name instead.  
   Returns 1 for an exact match, 2 for a basename only match and 0 for
   no match.  */

enum objfile_matches_name_return
objfile_matches_name (struct objfile *objfile, char *name)
{
  const char *filename;
  const char *bundlename;
  const char *real_name;
  
  if (objfile->name == NULL)
    return objfile_no_match;
  
  real_name = objfile->name;
  
  if (strcmp (real_name, name) == 0)
    return objfile_match_exact;
  
  bundlename = bundle_basename (real_name);
  if (bundlename && strcmp (bundlename, name) == 0)
    return objfile_match_base;

  filename = lbasename (real_name);
  if (filename == NULL)
    return objfile_no_match;
  
  if (strcmp (filename, name) == 0)
    return objfile_match_base;

  return objfile_no_match;
}

void
push_front_restrict_list (struct objfile_list **requested_list_head, 
                          struct objfile *objfile)
{
  struct objfile_list *new_requested_list_head 
    = (struct objfile_list *) xmalloc (sizeof (struct objfile_list));
  new_requested_list_head->objfile = objfile;
  new_requested_list_head->next = *requested_list_head;
  *requested_list_head = new_requested_list_head;
}

void clear_restrict_list (struct objfile_list **requested_list_head)
{
  while (*requested_list_head != NULL)
    {
      struct objfile_list *list_ptr;
      list_ptr = *requested_list_head;
      *requested_list_head = list_ptr->next;
      xfree (list_ptr);
    }
}
/* Restricts the objfile search to the REQUESTED_SHILB.  Returns
   a cleanup for the restriction, or -1 if no such shlib is
   found.  */

struct cleanup *
make_cleanup_restrict_to_shlib (char *requested_shlib)
{
  struct objfile_list *requested_list = NULL;
  struct objfile *requested_objfile = NULL;
  struct objfile *tmp_obj;

  if (requested_shlib == NULL)
    return NULL;

  /* Find the requested_objfile, if it doesn't exist, then throw an error.  Look
     for an exact match on the name, and if that doesn't work, look for a match
     on the filename, in case the user just gave us the library name.  */
  ALL_OBJFILES (tmp_obj)
    {
      enum objfile_matches_name_return match = 
                             objfile_matches_name (tmp_obj, requested_shlib); 
      if (match == objfile_match_exact)
	{
	  /* Okay, we found an exact match, so throw away a list if we
	     we had found any other matches, and break.  
	     APPLE LOCAL: If the exact match we found was a separate
	     debug file (dSYM), then add the original executable
	     first since objfile_get_first() and objfile_get_next()
	     functions will always return this separate debug objfile
	     first, followed by the original executable.  */

	  clear_restrict_list (&requested_list);
	  if (tmp_obj->separate_debug_objfile_backlink)
	    requested_objfile = tmp_obj->separate_debug_objfile_backlink;
	  else
	    requested_objfile = tmp_obj;
	  break;
	}
      else if (match == objfile_match_base)
	{
          /* APPLE LOCAL: Only add object file themselves -- never
             add the separate debug objfiles.  */
	  if (tmp_obj->separate_debug_objfile_backlink == NULL)
	    {
	      push_front_restrict_list (&requested_list, tmp_obj);
	    }
	}
    }

  if (requested_objfile != NULL)
    return make_cleanup_restrict_to_objfile (requested_objfile);
  else if (requested_list != NULL)
    return make_cleanup_restrict_to_objfile_list (requested_list);
  else
    return (void *) -1;
}


/* Get the first objfile.  If the restrict_search flag is set,
   this returns the first objfile in the restricted list, otherwise
   it starts from the object_files. */

struct objfile *
objfile_get_first ()
{
  if (!restrict_search || objfile_list == NULL)
    return object_files;
  else
    {
      /* APPLE LOCAL: When iterating we always return a separate
	 debug file first, and then return the objfile for the
	 separate debug file second to make sure we get debug
	 information from the separate debug file first, and then
	 fall back onto the original executable file for any extra
	 debug information that it may contain such as stabs
	 information that is not part of the debug map.  */

      objfile_list_ptr = objfile_list->next;
      if (objfile_list->objfile 
          && objfile_list->objfile->separate_debug_objfile)
	return objfile_list->objfile->separate_debug_objfile;
      else
	return objfile_list->objfile;
    }
}

/* Get the next objfile in the list.  
   FIXME: Right now you can't nest calls to ALL_OBJFILES if 
   restrict_search is on.  This isn't a problem in practice,
   but is ugly.  */

struct objfile *
objfile_get_next (struct objfile *in_objfile)
{
  struct objfile *objfile = NULL;
  
  if (!restrict_search || objfile_list == NULL)
    {
      if (in_objfile)
	return in_objfile->next;
      else
	return NULL;
    }

  /* APPLE LOCAL: If IN_OBJFILE is a separate debug file, return
     the corresponding executable file next without advancing the
     restrict list pointer. This helps us assure that the restrict
     list can never get out of sync where the separate debug file
     comes after the original executable file.  */

  if (in_objfile->separate_debug_objfile_backlink)
    objfile = in_objfile->separate_debug_objfile_backlink;
  else
    {
      /* Skip all separate debug files as they will be returned by the objfile
         who owns them only to ensure that the separate debug file always comes
	 first. This also implies that separate debug files never need to be
	 added to the restrict list -- as this code will always skip them.  */
      while (objfile_list_ptr)
	{
	  objfile = objfile_list_ptr->objfile;
	  objfile_list_ptr = objfile_list_ptr->next;
	  
	  if (objfile->separate_debug_objfile_backlink == NULL)
	    break;
	  
	  if (objfile_list_ptr == NULL)
	    return NULL;
	}
      
      /* Always return the separate debug file for an objfile first so we get
         any symbols we can out of this file first.  */
      if (objfile && objfile->separate_debug_objfile)
	objfile = objfile->separate_debug_objfile;
    }

  return objfile;
}

/* APPLE LOCAL set load state  */

static int should_auto_raise_load_state = 0;

/* FIXME: How to make this stuff platform independent???  
   Right now I just have a lame #ifdef MACOSX_DYLD.  I think
   the long term plan is to move the shared library handling
   into the architecture vector.  At that point,
   dyld_objfile_set_load_state should go there.  */

/* objfile_set_load_state: Set the level of symbol loading we are
   going to do for objfile O to LOAD_STATE.  If you are just doing
   this as a convenience to the user, set FORCE to 0, and this will
   allow the value of the "auto-raise-load-level" set variable to
   override the setting.  But if gdb needs to have this done, set
   FORCE to 1.  
   Returns the original load state, or -2 if the gdb auto-raise 
   settings rejected the change, or -1 for an error.  */

int
objfile_set_load_state (struct objfile *o, int load_state, int force)
{

  if (!force && !should_auto_raise_load_state)
    return -2;

  if (o->symflags & OBJF_SYM_DONT_CHANGE)
    return -2;

  /* FIXME: For now, we are not going to REDUCE the load state.  That is
     because we can't track which varobj's would need to get reconstructed
     if we were to change the state.  The only other option would be to
     throw away all the varobj's and that seems wasteful.  */

  if (o->symflags >= load_state)
    return load_state;

#ifdef MACOSX_DYLD
  return dyld_objfile_set_load_state (o, load_state);
#else
  return -1;
#endif
}

/* Set the symbol loading level of the objfile that includes
   the address PC to LOAD_STATE.  FORCE has the same meaning
   as for objfile_set_load_state, as does the return value.  */

int
pc_set_load_state (CORE_ADDR pc, int load_state, int force)
{
  struct obj_section *s;

  if (!force && !should_auto_raise_load_state)
    return -1;

  s = find_pc_section (pc);
  if (s == NULL)
    return -1;

  if (s->objfile == NULL)
    return -1;

  return objfile_set_load_state (s->objfile, load_state, force);
  
}

/* Sets the load state of an objfile by name.  FORCE, LOAD_STATE
   and the return value are the same as for objfile_set_load_state.  */

int
objfile_name_set_load_state (char *name, int load_state, int force)
{
  struct objfile *tmp_obj;

  if (!force && !should_auto_raise_load_state)
    return -2;

  if (name == NULL)
    return -1;

  ALL_OBJFILES (tmp_obj)
    {
      enum objfile_matches_name_return match 
	= objfile_matches_name (tmp_obj, name);
      if (match == objfile_match_exact || match == objfile_match_base)
	return objfile_set_load_state (tmp_obj, load_state, force);
    }
  
  return -1;
}

/* END APPLE LOCAL set_load_state  */

/* Return the first objfile that isn't marked as 'obsolete' (i.e. has been
   replaced by a newer version in a fix-and-continue operation.  */

/* APPLE LOCAL: The SKIP_OBSOLETE flag is used to skip over obsoleted
   symtabs.  */

struct symtab *
symtab_get_first (struct objfile *objfile, int skip_obsolete)
{
  struct symtab *s;

  s = objfile->symtabs;

  while (s != NULL && skip_obsolete && SYMTAB_OBSOLETED (s) == 51)
    {
      s = s->next;
    }

  return (s);
}

/* APPLE LOCAL: The SKIP_OBSOLETE flag is used to skip over obsoleted
   symtabs.  */

struct symtab *
symtab_get_next (struct symtab *s, int skip_obsolete)
{
  if (s == NULL)
    return NULL;

  s = s->next;

  while (s != NULL && skip_obsolete && SYMTAB_OBSOLETED (s) == 51)
    {
      s = s->next;
    }

  return s;
}

/* APPLE LOCAL: The SKIP_OBSOLETE flag is used to skip over obsoleted
   psymtabs. */

struct partial_symtab *
psymtab_get_first (struct objfile *objfile, int skip_obsolete)
{
  struct partial_symtab *ps;

  ps = objfile->psymtabs;
  while (ps != NULL && skip_obsolete && PSYMTAB_OBSOLETED (ps) == 51)
    {
      ps = ps->next;
    }

  return (ps);
}

/* APPLE LOCAL: The SKIP_OBSOLETE flag is used to skip over obsoleted
   psymtabs.  */


struct partial_symtab *
psymtab_get_next (struct partial_symtab *ps, int skip_obsolete)
{
  if (ps == NULL)
    return NULL;

  ps = ps->next;

  while (ps != NULL && skip_obsolete && PSYMTAB_OBSOLETED (ps) == 51)
    {
      ps = ps->next;
    }

  return ps;
}



/* Keep a registry of per-objfile data-pointers required by other GDB
   modules.  */

struct objfile_data
{
  unsigned index;
};

struct objfile_data_registration
{
  struct objfile_data *data;
  struct objfile_data_registration *next;
};
  
struct objfile_data_registry
{
  struct objfile_data_registration *registrations;
  unsigned num_registrations;
};

static struct objfile_data_registry objfile_data_registry = { NULL, 0 };

const struct objfile_data *
register_objfile_data (void)
{
  struct objfile_data_registration **curr;

  /* Append new registration.  */
  for (curr = &objfile_data_registry.registrations;
       *curr != NULL; curr = &(*curr)->next);

  *curr = XMALLOC (struct objfile_data_registration);
  (*curr)->next = NULL;
  (*curr)->data = XMALLOC (struct objfile_data);
  (*curr)->data->index = objfile_data_registry.num_registrations++;

  return (*curr)->data;
}

static void
objfile_alloc_data (struct objfile *objfile)
{
  gdb_assert (objfile->data == NULL);
  objfile->num_data = objfile_data_registry.num_registrations;
  objfile->data = XCALLOC (objfile->num_data, void *);
}

static void
objfile_free_data (struct objfile *objfile)
{
  gdb_assert (objfile->data != NULL);
  xfree (objfile->data);
  objfile->data = NULL;
}

/* APPLE LOCAL begin dwarf repository  */
unsigned
get_objfile_registry_num_registrations (void)
{
  return objfile_data_registry.num_registrations;
}
/* APPLE LOCAL end dwarf repository  */

void
clear_objfile_data (struct objfile *objfile)
{
  gdb_assert (objfile->data != NULL);
  memset (objfile->data, 0, objfile->num_data * sizeof (void *));
}

void
set_objfile_data (struct objfile *objfile, const struct objfile_data *data,
		  void *value)
{
  gdb_assert (data->index < objfile->num_data);
  objfile->data[data->index] = value;
}

void *
objfile_data (struct objfile *objfile, const struct objfile_data *data)
{
  gdb_assert (data->index < objfile->num_data);
  return objfile->data[data->index];
}

struct objfile * 
executable_objfile (struct objfile *objfile)
{
  if (objfile && objfile->separate_debug_objfile_backlink)
    return objfile->separate_debug_objfile_backlink;
  return objfile;
}

struct objfile * 
separate_debug_objfile (struct objfile *objfile)
{
  if (objfile && objfile->separate_debug_objfile)
    return objfile->separate_debug_objfile;
  return objfile;
}

/* APPLE LOCAL: A safer version the ANOFFSET macro that verifies
   that the section index is valid.  */
CORE_ADDR
objfile_section_offset (struct objfile *objfile, int sect_idx)
{
  /* Get the executable objfile in case this is a dSYM file.  */
  const char* err_str = NULL;
  struct objfile *exec_objfile = executable_objfile (objfile);
  if (exec_objfile == NULL)
    err_str = _("NULL objfile");
  else 
    {
      if (exec_objfile->section_offsets == NULL)
	err_str = _("Objfile section offsets are uninitialized");
      else if (sect_idx < 0)
	err_str = _("Section index is uninitialized");
      else if (sect_idx >= exec_objfile->num_sections)
	err_str = _("Section index is out of range for objfile");
    }

  if (err_str != NULL)
    {
      internal_error (__FILE__, __LINE__, "%s", err_str);
      return (CORE_ADDR) -1;
    }

  /* APPLE LOCAL shared cache begin.  */
  if (exec_objfile != objfile && 
      bfd_mach_o_in_shared_cached_memory (exec_objfile->obfd))
    {
      /* If we are reading from a memory based mach executable image that 
	 has a dSYM file, all executable image sections have zero offsets.
	 The dSYM file will be based at the original executable link 
	 addresses and will need offsets to make symbols in dSYM match the
	 shared cache loaded addresses. Core files currently run into this
	 issue, and if we decide to load images from memory in the future, 
	 those will as well.  */
      gdb_assert (exec_objfile->num_sections <= objfile->num_sections);
      gdb_assert (sect_idx < objfile->num_sections);
      return objfile->section_offsets->offsets[sect_idx];
    }
  /* APPLE LOCAL shared cache end.  */
  return exec_objfile->section_offsets->offsets[sect_idx];
}

/* APPLE LOCAL BEGIN: objfile section offsets.
   The objfile_text_section_offset, objfile_data_section_offset, 
   objfile_rodata_section_offset and objfile_bss_section_offset 
   functions allow quick control over the many places that grab
   common section offsets.  */
CORE_ADDR
objfile_text_section_offset (struct objfile *objfile)
{
  return objfile_section_offset (objfile, SECT_OFF_TEXT (objfile));
}

CORE_ADDR
objfile_data_section_offset (struct objfile *objfile)
{
  return objfile_section_offset (objfile, SECT_OFF_DATA (objfile));
}

CORE_ADDR
objfile_rodata_section_offset (struct objfile *objfile)
{
  return objfile_section_offset (objfile, SECT_OFF_RODATA (objfile));
}

CORE_ADDR
objfile_bss_section_offset (struct objfile *objfile)
{
  return objfile_section_offset (objfile, SECT_OFF_BSS (objfile));
}

/* APPLE LOCAL END: objfile section offsets.  */

/* APPLE LOCAL: objfile hitlists.  */
/* We want to record all the objfiles that contribute to the 
   parsing of an expression when making variable objects - so
   we know which ones to toss when an objfile gets deleted.
   So we set up an "objfile_hitlist" with objfile_init_hitlist,
   and then the symbol lookup code calls objfile_add_to_hitlist
   to add objfiles it uses.  Then you call objfile_detach_hitlist
   to get ownership of the hitlist.
   There can only be one hitlist going at a time.  */
struct objfile_hitlist
{
  int num_elem;
  struct objfile *ofiles[];
};

static struct objfile_hitlist *cur_objfile_hitlist;
/* This is the alloc'ed size of the current objfile_hitlist. 
  I had it part of the objfile_hitlist, but it is only needed
  when building up the hitlist, so that's a waste of space.  */
static int hitlist_max_elem;
#define HITLIST_INITIAL_MAX_ELEM 1

static void
objfile_init_hitlist ()
{
  if (cur_objfile_hitlist != NULL)
    internal_error (__FILE__, __LINE__, 
		    "Tried to initialize hit list without "
		    "closing previous hitlist.");
  cur_objfile_hitlist = xmalloc (sizeof (struct objfile_hitlist) 
				 + HITLIST_INITIAL_MAX_ELEM * sizeof (struct objfile *));
  hitlist_max_elem = HITLIST_INITIAL_MAX_ELEM;
  cur_objfile_hitlist->num_elem = 0;
}

void
objfile_add_to_hitlist (struct objfile *ofile)
{
  int ctr;
  if (cur_objfile_hitlist == NULL)
    return;
  if (ofile == NULL)
    return;

  if (cur_objfile_hitlist->num_elem == hitlist_max_elem)
    {
      hitlist_max_elem
	+= hitlist_max_elem;
      cur_objfile_hitlist = xrealloc (cur_objfile_hitlist, sizeof (struct objfile_hitlist)
		     + hitlist_max_elem * sizeof (struct objfile *));
    }
  for (ctr = 0; ctr < cur_objfile_hitlist->num_elem; ctr++)
    {
      if (cur_objfile_hitlist->ofiles[ctr] == ofile)
	return;
    }
  cur_objfile_hitlist->ofiles[ctr] = ofile;
  cur_objfile_hitlist->num_elem += 1;
}

/* Use this to get control of the objfile_hitlist
   you've been accumulating since the call to
   make_cleanup_objfile_init_clear_hitlist.  You 
   get a pointer to a malloc'ed hitlist structure.
   When you are done with it you can just free it.  */

struct objfile_hitlist *
objfile_detach_hitlist ()
{
  struct objfile_hitlist *thislist;

  thislist = cur_objfile_hitlist;
  cur_objfile_hitlist = NULL;
  hitlist_max_elem = HITLIST_INITIAL_MAX_ELEM;
  return thislist;
}

static void
objfile_clear_hitlist (void *notused)
{
  struct objfile_hitlist *hitlist;
  hitlist = objfile_detach_hitlist ();
  if (hitlist != NULL)
    xfree (hitlist);
}

/* This will initialize the hitlist, and return a cleanup
   that will clear the hitlist when called.  If you detach
   the hitlist before you call the clearing cleanup, that
   cleanup will do nothing.  */

struct cleanup *
make_cleanup_objfile_init_clear_hitlist ()
{
  objfile_init_hitlist ();
  return make_cleanup (objfile_clear_hitlist, NULL);
}

int
objfile_on_hitlist_p (struct objfile_hitlist *hitlist,
		       struct objfile *ofile)
{
  int ctr;

  for (ctr = 0; ctr < hitlist->num_elem; ctr++)
    {
      if (hitlist->ofiles[ctr] == ofile)
	return 1;
    }
  return 0;
}

void
_initialize_objfiles (void)
{
  cached_symfile_path =
    xstrdup ("./gdb-symfile-cache:./syms:/usr/libexec/gdb/symfiles");

  /* APPLE LOCAL: We don't want to raise load levels for MetroWerks.  */
  add_setshow_boolean_cmd ("auto-raise-load-levels", class_obscure,
			   &should_auto_raise_load_state, _("\
Set if GDB should raise the symbol loading level on all frames found in backtraces."), _("\
Show if GDB should raise the symbol loading level on all frames found in backtraces."), NULL,
			   NULL, NULL, &setlist, &showlist);
}
