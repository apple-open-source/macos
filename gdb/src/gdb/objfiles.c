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

#include "gdb_assert.h"
#include <sys/types.h>
#include "gdb_stat.h"
#include <fcntl.h>
#include "gdb_obstack.h"
#include "gdb_string.h"
#include "buildsym.h"
#include "hashtab.h"
#include "breakpoint.h"
#include "block.h"
#include "dictionary.h"

/* Prototypes for local functions */

#if defined(USE_MMALLOC) && defined(HAVE_MMAP)

#include "mmalloc.h"

#ifdef FSF_OBJFILES
static int open_existing_mapped_file (char *, long, int);

static int open_mapped_file (char *filename, long mtime, int flags);

static void *map_to_file (int);
#endif /* FSF_OBJFILES */

#endif /* defined(USE_MMALLOC) && defined(HAVE_MMAP) */

#ifdef FSF_OBJFILES
static void add_to_objfile_sections (bfd *, sec_ptr, void *);
#endif /* FSF_OBJFILES */

static void objfile_remove_from_restrict_list (struct objfile *);

void objfile_alloc_data (struct objfile *objfile);
static void objfile_free_data (struct objfile *objfile);

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

int mapped_symbol_files = 0;
int use_mapped_symbol_files = 0;  // Temporarily disable jmolenda 2004-05-13

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

/* Delete all the obj_sections in OBJFILE from the ordered_sections
   global list.  N.B. this routine uses the addresses in the sections
   in the objfile to find the entries in the ordered_sections list, 
   so if you are going to relocate the obj_sections in an objfile,
   call this BEFORE you relocate, then relocate, then call 
   objfile_add_to_ordered_sections.  */

#define STATIC_DELETE_LIST_SIZE 256
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

void 
objfile_delete_from_ordered_sections (struct objfile *objfile)
{
  int i;
  int ndeleted;
  int delete_list_size;
  int static_delete_list[STATIC_DELETE_LIST_SIZE];
  int *delete_list = static_delete_list;
  
  struct obj_section *s;
  
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

  CHECK_FATAL (objfile != NULL);

  /* First find the index for insertion of all the sections in
     this objfile.  The sort that array in reverse order by address,
     then go through the ordered list block moving the bits between
     the insert points, then adding the pieces we need to add.  */

  insert_list_size = objfile->sections_end - objfile->sections;
  if (insert_list_size > STATIC_INSERT_LIST_SIZE)
    insert_list = (struct obj_section_with_index *) xmalloc (insert_list_size * sizeof (struct obj_section_with_index));

  total = 0;
  ALL_OBJFILE_OSECTIONS (objfile, s)
    {
      /* APPLE LOCAL: Oh, hacky, hacky...  The bfd Mach-O reader makes
         bfd_sections for both the sections & segments (the container of
         the sections).  This would make pc->bfd_section lookup non-unique.
         so we just drop the segments from our list.  */
      if (s->the_bfd_section && s->the_bfd_section->name &&
          number_of_dots (s->the_bfd_section->name) == 1)
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

#ifdef FSF_OBJFILES
/* Locate all mappable sections of a BFD file. 
   objfile_p_char is a char * to get it through
   bfd_map_over_sections; we cast it back to its proper type.  */

#ifndef TARGET_KEEP_SECTION
#define TARGET_KEEP_SECTION(ASECT)	0
#endif

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
  /* objfile->sections can be already set when reading a mapped symbol
     file.  I believe that we do need to rebuild the section table in
     this case (we rebuild other things derived from the bfd), but we
     can't free the old one (it's in the objfile_obstack).  So we just
     waste some memory.  */

  objfile->sections_end = 0;
  bfd_map_over_sections (objfile->obfd, add_to_objfile_sections, (char *) objfile);
  objfile->sections = (struct obj_section *)
    obstack_finish (&objfile->objfile_obstack);
  objfile->sections_end = objfile->sections + (unsigned long) objfile->sections_end;

  objfile_add_to_ordered_sections (objfile);

  return (0);
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

struct objfile *
allocate_objfile (bfd *abfd, int flags)
{
  struct objfile *objfile = NULL;

  if (mapped_symbol_files)
    flags |= OBJF_MAPPED;

#if defined(USE_MMALLOC) && defined(HAVE_MMAP)
  if (abfd != NULL)
    {

      /* If we can support mapped symbol files, try to open/reopen the
         mapped file that corresponds to the file from which we wish to
         read symbols.  If the objfile is to be mapped, we must malloc
         the structure itself using the mmap version, and arrange that
         all memory allocation for the objfile uses the mmap routines.
         If we are reusing an existing mapped file, from which we get
         our objfile pointer, we have to make sure that we update the
         pointers to the alloc/free functions in the obstack, in case
         these functions have moved within the current gdb.  */

      int fd;

      fd = open_mapped_file (bfd_get_filename (abfd), bfd_get_mtime (abfd),
			     flags);
      if (fd >= 0)
	{
	  void *md;

	  if ((md = map_to_file (fd)) == NULL)
	    {
	      close (fd);
	    }
	  else if ((objfile = (struct objfile *) mmalloc_getkey (md, 0)) != NULL)
	    {
	      /* Update memory corruption handler function addresses. */
	      init_malloc (md);
	      objfile->md = md;
	      objfile->mmfd = fd;
	      /* Update pointers to functions to *our* copies */
	      if (objfile->demangled_names_hash)
		htab_set_functions_ex
		  (objfile->demangled_names_hash, htab_hash_string,
		   (int (*) (const void *, const void *)) streq, NULL,
		   objfile->md, xmcalloc, xmfree);
	      obstack_chunkfun (&objfile->psymbol_cache.cache, xmmalloc);
	      obstack_freefun (&objfile->psymbol_cache.cache, xmfree);
	      obstack_chunkfun (&objfile->macro_cache.cache, xmmalloc);
	      obstack_freefun (&objfile->macro_cache.cache, xmfree);
	      obstack_chunkfun (&objfile->psymbol_obstack, xmmalloc);
	      obstack_freefun (&objfile->psymbol_obstack, xmfree);
	      obstack_chunkfun (&objfile->symbol_obstack, xmmalloc);
	      obstack_freefun (&objfile->symbol_obstack, xmfree);
	      obstack_chunkfun (&objfile->type_obstack, xmmalloc);
	      obstack_freefun (&objfile->type_obstack, xmfree);
	      /* If already in objfile list, unlink it. */
	      unlink_objfile (objfile);
	      /* Forget things specific to a particular gdb, may have changed. */
	      objfile->sf = NULL;
	    }
	  else
	    {

	      /* Set up to detect internal memory corruption.  MUST be
	         done before the first malloc.  See comments in
	         init_malloc() and mmcheck().  */

	      init_malloc (md);

	      objfile = (struct objfile *)
		xmmalloc (md, sizeof (struct objfile));
	      memset (objfile, 0, sizeof (struct objfile));
	      objfile->md = md;
	      objfile->mmfd = fd;
	      objfile->flags |= OBJF_MAPPED;
	      mmalloc_setkey (objfile->md, 0, objfile);
	      obstack_specify_allocation_with_arg (&objfile->psymbol_cache.cache,
						   0, 0, xmmalloc, xmfree,
						   objfile->md);
	      obstack_specify_allocation_with_arg (&objfile->macro_cache.cache,
						   0, 0, xmmalloc, xmfree,
						   objfile->md);
	      obstack_specify_allocation_with_arg (&objfile->psymbol_obstack,
						   0, 0, xmmalloc, xmfree,
						   objfile->md);
	      obstack_specify_allocation_with_arg (&objfile->symbol_obstack,
						   0, 0, xmmalloc, xmfree,
						   objfile->md);
	      obstack_specify_allocation_with_arg (&objfile->type_obstack,
						   0, 0, xmmalloc, xmfree,
						   objfile->md);
	    }
	}

      if ((flags & OBJF_MAPPED) && (objfile == NULL))
	{
	  warning ("symbol table for '%s' will not be mapped",
		   bfd_get_filename (abfd));
	  flags &= ~OBJF_MAPPED;
	}
    }
#else /* !defined(USE_MMALLOC) || !defined(HAVE_MMAP) */

  if (flags & OBJF_MAPPED)
    {
      warning ("mapped symbol tables are not supported on this machine; missing or broken mmap().");

      /* Turn off the global flag so we don't try to do mapped symbol tables
         any more, which shuts up gdb unless the user specifically gives the
         "mapped" keyword again. */

      mapped_symbol_files = 0;
      flags &= ~OBJF_MAPPED;
    }

#endif /* defined(USE_MMALLOC) && defined(HAVE_MMAP) */

  /* If we don't support mapped symbol files, didn't ask for the file to be
     mapped, or failed to open the mapped file for some reason, then revert
     back to an unmapped objfile. */

  if (objfile == NULL)
    {
      objfile = (struct objfile *) xmalloc (sizeof (struct objfile));
      memset (objfile, 0, sizeof (struct objfile));
      objfile->md = NULL;
      objfile->psymbol_cache = bcache_xmalloc (NULL);
      objfile->macro_cache = bcache_xmalloc (NULL);
      /* We could use obstack_specify_allocation here instead, but
	 gdb_obstack.h specifies the alloc/dealloc functions.  */
      obstack_init (&objfile->objfile_obstack);
      terminate_minimal_symbol_table (objfile);
      flags &= ~OBJF_MAPPED;
    }

  objfile_alloc_data (objfile);

  /* Update the per-objfile information that comes from the bfd, ensuring
     that any data that is reference is saved in the per-objfile data
     region. */

  objfile->obfd = abfd;
  if (objfile->name != NULL)
    {
      xmfree (objfile->md, objfile->name);
    }
  if (abfd != NULL)
    {
      objfile->name = mstrsave (objfile->md, bfd_get_filename (abfd));
      objfile->mtime = bfd_get_mtime (abfd);

      /* Build section table.  */

      if (build_objfile_section_table (objfile))
	{
	  error ("Can't find the file sections in `%s': %s",
		 objfile->name, bfd_errmsg (bfd_get_error ()));
	}
    }
  else
    {
      objfile->name = mstrsave (objfile->md, "<<anonymous objfile>>");
    }

  /* Initialize the section indexes for this objfile, so that we can
     later detect if they are used w/o being properly assigned to. */

    objfile->sect_index_text = -1;
    objfile->sect_index_data = -1;
    objfile->sect_index_bss = -1;
    objfile->sect_index_rodata = -1;

  /* We don't yet have a C++-specific namespace symtab.  */

  objfile->cp_namespace_symtab = NULL;

  /* APPLE LOCAL: Set the equivalence table bits.  
     FIXME: There should really be some host specific function we 
     call out to to do this.  We will need to fix this if we intend
     to submit this code.  */
#ifdef NM_NEXTSTEP
  if (objfile->name != NULL && strstr (objfile->name, "libSystem") != NULL)
    objfile->check_for_equivalence = 1;
  else
#endif /* NM_NEXTSTEP */
    objfile->check_for_equivalence = 0;
  objfile->equivalence_table = NULL;

  /* APPLE LOCAL: Set SYMS_ONLY_OBJFILE to 1 when this objfile was
     added by add-symbol-file or sharedlibrary specify-symbol-file,
     i.e. this objfile is really shadowing another, actual objfile and
     is just providing symbols.  */
  objfile->syms_only_objfile = 0;

  /* Add this file onto the tail of the linked list of other such files. */

  /* Save passed in flag bits. */
  objfile->flags |= flags;
  objfile->symflags = -1;

  link_objfile (objfile);

  return (objfile);
}
#endif /* FSF_OBJFILES */

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
  objfile->ei.deprecated_entry_file_lowpc = INVALID_ENTRY_LOWPC;
  objfile->ei.deprecated_entry_file_highpc = INVALID_ENTRY_HIGHPC;
  objfile->ei.entry_func_lowpc = INVALID_ENTRY_LOWPC;
  objfile->ei.entry_func_highpc = INVALID_ENTRY_HIGHPC;
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
		  "put_objfile_before: before objfile not in list");
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
		  "unlink_objfile: objfile already unlinked");
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

void
free_objfile (struct objfile *objfile)
{
  if (objfile->separate_debug_objfile)
    {
      free_objfile (objfile->separate_debug_objfile);
    }
  
  if (objfile->separate_debug_objfile_backlink)
    {
      /* We freed the separate debug file, make sure the base objfile
	 doesn't reference it.  */
      objfile->separate_debug_objfile_backlink->separate_debug_objfile = NULL;
    }
  
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


  /* APPLE LOCAL: Remove all the obj_sections in this objfile from the
     ordered_sections list.  Do this before deleting the bfd, since
     we need to use the bfd_sections to do it.  */
  
  objfile_delete_from_ordered_sections (objfile);

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
      xmfree (objfile->md, objfile->name);
    }
  if (objfile->global_psymbols.list)
    xmfree (objfile->md, objfile->global_psymbols.list);
  if (objfile->static_psymbols.list)
    xmfree (objfile->md, objfile->static_psymbols.list);
  /* Free the obstacks for non-reusable objfiles */
  bcache_xfree (objfile->psymbol_cache);
  bcache_xfree (objfile->macro_cache);
  /* APPLE LOCAL: Also free up the table of "equivalent symbols".  */
  equivalence_table_delete (objfile);
  /* END APPLE LOCAL */
  if (objfile->demangled_names_hash)
    htab_delete (objfile->demangled_names_hash);
  obstack_free (&objfile->objfile_obstack, 0);
  xmfree (objfile->md, objfile);
  objfile = NULL;
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
	    l->item[i].pc += ANOFFSET (delta, s->block_line_section);

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
#ifdef MIPS_EFI_SYMBOL_NAME
	      /* Relocate Extra Function Info for ecoff.  */

	      else if (SYMBOL_CLASS (sym) == LOC_CONST
		       && SYMBOL_DOMAIN (sym) == LABEL_DOMAIN
		       && strcmp (DEPRECATED_SYMBOL_NAME (sym), MIPS_EFI_SYMBOL_NAME) == 0)
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

    objfile_delete_from_ordered_sections (objfile);

    ALL_OBJFILE_OSECTIONS (objfile, s)
      {
      	int idx = s->the_bfd_section->index;
	
	s->addr += ANOFFSET (delta, idx);
	s->endaddr += ANOFFSET (delta, idx);
      }

    objfile_add_to_ordered_sections (objfile);
  }

  if (objfile->ei.entry_func_lowpc != INVALID_ENTRY_LOWPC)
    {
      objfile->ei.entry_func_lowpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
      objfile->ei.entry_func_highpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
    }

  if (objfile->ei.deprecated_entry_file_lowpc != INVALID_ENTRY_LOWPC)
    {
      objfile->ei.deprecated_entry_file_lowpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
      objfile->ei.deprecated_entry_file_highpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
    }

  if (objfile->ei.main_func_lowpc != INVALID_ENTRY_LOWPC)
    {
      objfile->ei.main_func_lowpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
      objfile->ei.main_func_highpc += ANOFFSET (delta, SECT_OFF_TEXT (objfile));
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
/* NB: I don't think NM_MACOSX is ever defined with our current configury
       set-up -- my guess is that this is effectively permanently #if 0'ed 
       on our platform.  jsm/2004-12-08 */
#ifdef NM_MACOSX
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
    if (ofp->minimal_symbol_count > 0)
      {
	return 1;
      }
  }
  return 0;
}

#ifdef FSF_OBJFILES
#if defined(USE_MMALLOC) && defined(HAVE_MMAP)

/* Given the name of a mapped symbol file in SYMSFILENAME, and the timestamp
   of the corresponding symbol file in MTIME, try to open an existing file
   with the name SYMSFILENAME and verify it is more recent than the base
   file by checking it's timestamp against MTIME.

   If SYMSFILENAME does not exist (or can't be stat'd), simply returns -1.

   If SYMSFILENAME does exist, but is out of date, we check to see if the
   user has specified creation of a mapped file.  If so, we don't issue
   any warning message because we will be creating a new mapped file anyway,
   overwriting the old one.  If not, then we issue a warning message so that
   the user will know why we aren't using this existing mapped symbol file.
   In either case, we return -1.

   If SYMSFILENAME does exist and is not out of date, but can't be opened for
   some reason, then prints an appropriate system error message and returns -1.

   Otherwise, returns the open file descriptor.  */

static int
open_existing_mapped_file (char *symsfilename, long mtime, int flags)
{
  int fd = -1;
  struct stat sbuf;

  if (stat (symsfilename, &sbuf) == 0)
    {
      if (sbuf.st_mtime < mtime)
	{
	  if (!(flags & OBJF_MAPPED))
	    {
	      warning ("mapped symbol file `%s' is out of date, ignored it",
		       symsfilename);
	    }
	}
      else if ((fd = open (symsfilename, O_RDWR)) < 0)
	{
	  if (error_pre_print)
	    {
	      printf_unfiltered (error_pre_print);
	    }
	  print_sys_errmsg (symsfilename, errno);
	}
    }
  return (fd);
}

/* Look for a mapped symbol file that corresponds to FILENAME and is more
   recent than MTIME.  If MAPPED is nonzero, the user has asked that gdb
   use a mapped symbol file for this file, so create a new one if one does
   not currently exist.

   If found, then return an open file descriptor for the file, otherwise
   return -1.

   This routine is responsible for implementing the policy that generates
   the name of the mapped symbol file from the name of a file containing
   symbols that gdb would like to read.  Currently this policy is to append
   ".syms" to the name of the file.

   This routine is also responsible for implementing the policy that
   determines where the mapped symbol file is found (the search path).
   This policy is that when reading an existing mapped file, a file of
   the correct name in the current directory takes precedence over a
   file of the correct name in the same directory as the symbol file.
   When creating a new mapped file, it is always created in the current
   directory.  This helps to minimize the chances of a user unknowingly
   creating big mapped files in places like /bin and /usr/local/bin, and
   allows a local copy to override a manually installed global copy (in
   /bin for example).  */

static int
open_mapped_file (char *filename, long mtime, int flags)
{
  int fd;
  char *symsfilename;

  /* First try to open an existing file in the current directory, and
     then try the directory where the symbol file is located. */

  symsfilename = concat ("./", lbasename (filename), ".syms", (char *) NULL);
  if ((fd = open_existing_mapped_file (symsfilename, mtime, flags)) < 0)
    {
      xfree (symsfilename);
      symsfilename = concat (filename, ".syms", (char *) NULL);
      fd = open_existing_mapped_file (symsfilename, mtime, flags);
    }

  /* If we don't have an open file by now, then either the file does not
     already exist, or the base file has changed since it was created.  In
     either case, if the user has specified use of a mapped file, then
     create a new mapped file, truncating any existing one.  If we can't
     create one, print a system error message saying why we can't.

     By default the file is rw for everyone, with the user's umask taking
     care of turning off the permissions the user wants off. */

  if ((fd < 0) && (flags & OBJF_MAPPED))
    {
      xfree (symsfilename);
      symsfilename = concat ("./", lbasename (filename), ".syms",
			     (char *) NULL);
      if ((fd = open (symsfilename, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0)
	{
	  if (error_pre_print)
	    {
	      printf_unfiltered (error_pre_print);
	    }
	  print_sys_errmsg (symsfilename, errno);
	}
    }

  xfree (symsfilename);
  return (fd);
}

static void *
map_to_file (int fd)
{
  void *md;
  CORE_ADDR mapto;

  md = mmalloc_attach (fd, 0);
  if (md != NULL)
    {
      mapto = (CORE_ADDR) mmalloc_getkey (md, 1);
      md = mmalloc_detach (md);
      if (md != NULL)
	{
	  /* FIXME: should figure out why detach failed */
	  md = NULL;
	}
      else if (mapto != (CORE_ADDR) NULL)
	{
	  /* This mapping file needs to be remapped at "mapto" */
	  md = mmalloc_attach (fd, mapto);
	}
      else
	{
	  /* This is a freshly created mapping file. */
	  mapto = (CORE_ADDR) mmalloc_findbase (20 * 1024 * 1024);
	  if (mapto != 0)
	    {
	      /* To avoid reusing the freshly created mapping file, at the 
	         address selected by mmap, we must truncate it before trying
	         to do an attach at the address we want. */
	      ftruncate (fd, 0);
	      md = mmalloc_attach (fd, mapto);
	      if (md != NULL)
		{
		  mmalloc_setkey (md, 1, mapto);
		}
	    }
	}
    }
  return (md);
}

#endif /* defined(USE_MMALLOC) && defined(HAVE_MMAP) */
#endif /* FSF_OBJFILES */

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

  s = find_pc_sect_in_ordered_sections (pc, section);
  if (s != NULL)
    return (s);
  
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
  struct objfile_list *new_objfile = (struct objfile_list *)
    xmalloc (sizeof (struct objfile_list));
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

#define CACHED_SYM_SUFFIX ".syms"
enum objfile_matches_name_return
objfile_matches_name (struct objfile *objfile, char *name)
{
  const char *filename;
  int len; 
  static int suffixlen = strlen (CACHED_SYM_SUFFIX);
  const char *real_name;
  
  if (objfile->name == NULL)
    return objfile_no_match;
  
  /* See if this is a cached symfile, in which case we use the bfd name
     if it exists.  */
  len = strlen (objfile->name);
  if (len > suffixlen 
      && strcmp (objfile->name + len - suffixlen, CACHED_SYM_SUFFIX) == 0
      && objfile->obfd != NULL && objfile->obfd->filename != NULL)
    {
      real_name = objfile->obfd->filename;
    }
  else
    real_name = objfile->name;
  
  if (strcmp (real_name, name) == 0)
    return objfile_match_exact;
  
  filename = lbasename (real_name);
  if (filename == NULL)
    return objfile_no_match;
  
  if (strcmp (filename, name) == 0)
    return objfile_match_base;

  return objfile_no_match;
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
      enum objfile_matches_name_return match = objfile_matches_name (tmp_obj, requested_shlib); 
      if (match == objfile_match_exact)
	{
	  /* Okay, we found an exact match, so throw away a list if we
	     we had found any other matches, and break.  */
	  requested_objfile = tmp_obj;
	  while (requested_list != NULL)
	    {
	      struct objfile_list *list_ptr;
	      list_ptr = requested_list;
	      requested_list = list_ptr->next;
	      xfree (list_ptr);
	    }
	  requested_list = NULL;
	  break;
	}
      else if (match == objfile_match_base)
	{
	  struct objfile_list *new_element 
	    = (struct objfile_list *) xmalloc (sizeof (struct objfile_list));
	  new_element->objfile = tmp_obj;
	  new_element->next = requested_list;
	  requested_list = new_element;
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
      objfile_list_ptr = objfile_list->next;
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
  struct objfile *objfile;
  
  if (!restrict_search || objfile_list == NULL)
    {
      if (in_objfile)
	return in_objfile->next;
      else
	return NULL;
    }

  if (objfile_list_ptr == NULL)
    {
      return NULL;
    }
  
  objfile = objfile_list_ptr->objfile;
  objfile_list_ptr = objfile_list_ptr->next;

  return objfile;
}

/* APPLE LOCAL set load state  */

static int should_auto_raise_load_state = 0;

/* FIXME: How to make this stuff platform independent???  
   Right now I just have a lame #ifdef NM_NEXTSTEP.  I think
   the long term plan is to move the shared library handling
   into the architecture vector.  At that point,
   dyld_objfile_set_load_state should go there.  */

/* objfile_set_load_state: Set the level of symbol loading we are
   going to do for objfile O to LOAD_STATE.  If you are just doing
   this as a convenience to the user, set FORCE to 0, and this will
   allow the value of the "auto-raise-load-level" set variable to
   override the setting.  But if gdb needs to have this done, set
   FORCE to 1.  */

int
objfile_set_load_state (struct objfile *o, int load_state, int force)
{

  if (!force && !should_auto_raise_load_state)
    return 1;

  /* FIXME: For now, we are not going to REDUCE the load state.  That is
     because we can't track which varobj's would need to get reconstructed
     if we were to change the state.  The only other option would be to
     throw away all the varobj's and that seems wasteful.  */

  if (o->symflags >= load_state)
    return 1;

#ifdef NM_NEXTSTEP
  return dyld_objfile_set_load_state (o, load_state);
#else
  return 1;
#endif
}

/* Set the symbol loading level of the objfile that includes
   the address PC to LOAD_STATE.  FORCE has the same meaning
   as for objfile_set_load_state.  */

int
pc_set_load_state (CORE_ADDR pc, int load_state, int force)
{
  struct obj_section *s;

  if (!force && !should_auto_raise_load_state)
    return 1;

  s = find_pc_section (pc);
  if (s == NULL)
    return 0;

  if (s->objfile == NULL)
    return 0;

  return objfile_set_load_state (s->objfile, load_state, force);
  
}

int
objfile_name_set_load_state (char *name, int load_state, int force)
{
  struct objfile *tmp_obj;
  int retval = 0;

  if (!force && !should_auto_raise_load_state)
    return 1;

  if (name == NULL)
    return 0;

  ALL_OBJFILES (tmp_obj)
    {
      enum objfile_matches_name_return match 
	= objfile_matches_name (tmp_obj, name);
      if (match == objfile_match_exact || match == objfile_match_base)
	if (objfile_set_load_state (tmp_obj, load_state, force))
	  retval = 1;
    }
  
  return retval;
}

/* END APPLE LOCAL set_load_state  */

/* APPLE LOCAL begin fix-and-continue */

/* Originally these two functions were a temporary measure to ensure
   I hadn't missed any symtab/psymtab creation paths, but they are a
   generally useful diagnostic to make sure future merges don't hose
   Fix and Continue, so I'm leaving them in.  */

static void 
sanity_check_symtab_obsoleted_flag (struct symtab *s)
{
  if (s != NULL && 
      SYMTAB_OBSOLETED (s) != 51 &&
      SYMTAB_OBSOLETED (s) != 50)
    {
      struct objfile *objfile;
      struct symtab *symtab;
      const char *objfile_name = "(not found)";
      const char *symtab_name = "(null)";

      /* Use _INCL_OBSOLETED variant as a tricky/secret way of telling
	symtab_get_first(), symtab_get_next() that this sanity_check
        function should not be called (which would cause an inf. loop).  */

      ALL_SYMTABS_INCL_OBSOLETED (objfile, symtab)
        if (symtab == s)
          {
            objfile_name = objfile->name;
            break;
          }

      if (s->filename)
        symtab_name = s->filename;

      error ("Symtab with invalid OBSOLETED flag setting.  "
             "Value is %d, symtab name is %s objfile name is %s", 
             SYMTAB_OBSOLETED (s), symtab_name, objfile_name);
    }
}

static void sanity_check_psymtab_obsoleted_flag (struct partial_symtab *ps)
{
  if (ps != NULL && 
      PSYMTAB_OBSOLETED (ps) != 51 &&
      PSYMTAB_OBSOLETED (ps) != 50)
    {
      struct objfile *objfile;
      struct partial_symtab *psymtab;
      const char *objfile_name = "(not found)";
      const char *psymtab_name = "(null)";

      /* Use _INCL_OBSOLETED variant as a tricky/secret way of telling
	psymtab_get_first(), psymtab_get_next() that this sanity_check
        function should not be called (which would cause an inf. loop).  */

      ALL_PSYMTABS_INCL_OBSOLETED (objfile, psymtab)
        if (psymtab == ps)
          {
            objfile_name = objfile->name;
            break;
          }

      if (ps->filename)
        psymtab_name = ps->filename;

      error ("Psymtab with invalid OBSOLETED flag setting.  "
             "Value is %d, psymtab name is %s, objfile name is %s", 
             PSYMTAB_OBSOLETED (ps), psymtab_name, objfile_name);
    }
}

/* Return the first objfile that isn't marked as 'obsolete' (i.e. has been
   replaced by a newer version in a fix-and-continue operation.  */

/* APPLE LOCAL: The SKIP_OBSOLETE flag is used to skip over obsoleted
   symtabs.  When we're doing that -- skipping over obsoleted symtabs --
   we also perform the sanity_check_symtab_obsoleted_flag ().  

   We DON'T run the sanity check tests when SKIP_OBSOLETED is 0
   because sanity_check_symtab_obsoleted_flag () can potentially
   recurse into this function so it sets SKIP_OBSOLETED to 0 to
   avoid that.  (other parts of the code set SKIP_OBSOLETED to 0
   just to indicate that they don't want to skip obsoleted symtabs. */

struct symtab *
symtab_get_first (struct objfile *objfile, int skip_obsolete)
{
  struct symtab *s;

  s = objfile->symtabs;
  if (skip_obsolete)
    sanity_check_symtab_obsoleted_flag (s);

  while (s != NULL && skip_obsolete && SYMTAB_OBSOLETED (s) == 51)
    {
      s = s->next;
      if (skip_obsolete)
        sanity_check_symtab_obsoleted_flag (s);
    }

  return (s);
}

/* APPLE LOCAL: The SKIP_OBSOLETE flag is used to skip over obsoleted
   symtabs.  When we're doing that -- skipping over obsoleted symtabs --
   we also perform the sanity_check_symtab_obsoleted_flag ().  

   We DON'T run the sanity check tests when SKIP_OBSOLETED is 0
   because sanity_check_symtab_obsoleted_flag () can potentially
   recurse into this function so it sets SKIP_OBSOLETED to 0 to
   avoid that.  (other parts of the code set SKIP_OBSOLETED to 0
   just to indicate that they don't want to skip obsoleted symtabs. */

struct symtab *
symtab_get_next (struct symtab *s, int skip_obsolete)
{
  if (s == NULL)
    return NULL;

  s = s->next;
  if (skip_obsolete)
    sanity_check_symtab_obsoleted_flag (s);

  while (s != NULL && skip_obsolete && SYMTAB_OBSOLETED (s) == 51)
    {
      s = s->next;
      if (skip_obsolete)
        sanity_check_symtab_obsoleted_flag (s);
    }

  return s;
}

/* APPLE LOCAL: The SKIP_OBSOLETE flag is used to skip over obsoleted
   psymtabs.  When we're doing that -- skipping over obsoleted psymtabs --
   we also perform the sanity_check_psymtab_obsoleted_flag ().  

   We DON'T run the sanity check tests when SKIP_OBSOLETED is 0
   because sanity_check_psymtab_obsoleted_flag () can potentially
   recurse into this function so it sets SKIP_OBSOLETED to 0 to
   avoid that.  (other parts of the code set SKIP_OBSOLETED to 0
   just to indicate that they don't want to skip obsoleted psymtabs. */

struct partial_symtab *
psymtab_get_first (struct objfile *objfile, int skip_obsolete)
{
  struct partial_symtab *ps;

  ps = objfile->psymtabs;
  if (skip_obsolete)
    sanity_check_psymtab_obsoleted_flag (ps);
  while (ps != NULL && skip_obsolete && PSYMTAB_OBSOLETED (ps) == 51)
    {
      if (skip_obsolete)
        sanity_check_psymtab_obsoleted_flag (ps);
      ps = ps->next;
    }

  return (ps);
}

/* APPLE LOCAL: The SKIP_OBSOLETE flag is used to skip over obsoleted
   psymtabs.  When we're doing that -- skipping over obsoleted psymtabs --
   we also perform the sanity_check_psymtab_obsoleted_flag ().  

   We DON'T run the sanity check tests when SKIP_OBSOLETED is 0
   because sanity_check_psymtab_obsoleted_flag () can potentially
   recurse into this function so it sets SKIP_OBSOLETED to 0 to
   avoid that.  (other parts of the code set SKIP_OBSOLETED to 0
   just to indicate that they don't want to skip obsoleted psymtabs. */

struct partial_symtab *
psymtab_get_next (struct partial_symtab *ps, int skip_obsolete)
{
  if (ps == NULL)
    return NULL;

  ps = ps->next;
  if (skip_obsolete)
    sanity_check_psymtab_obsoleted_flag (ps);

  while (ps != NULL && skip_obsolete && PSYMTAB_OBSOLETED (ps) == 51)
    {
      ps = ps->next;
      if (skip_obsolete)
        sanity_check_psymtab_obsoleted_flag (ps);
    }

  return ps;
}
/* APPLE LOCAL end fix-and-continue */



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

/* APPLE LOCAL - make non-static, for now we need it in cached-symfile.c.  */

void
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

void
_initialize_objfiles (void)
{
  struct cmd_list_element *c;

#if HAVE_MMAP

  c = add_set_cmd ("generate-cached-symfiles", class_obscure, var_boolean,
		   (char *) &mapped_symbol_files,
		   "Set if GDB should generate persistent symbol tables by default.",
		   &setlist);
  add_show_from_set (c, &showlist);

  c = add_set_cmd ("use-cached-symfiles", class_obscure, var_boolean,
		   (char *) &use_mapped_symbol_files,
		   "Set if GDB should use persistent symbol tables by default.",
		   &setlist);
  add_show_from_set (c, &showlist);

  c = add_set_cmd ("generate-precompiled-symfiles", class_obscure, var_boolean,
		   (char *) &mapped_symbol_files,
		   "Set if GDB should generate persistent symbol tables by default.",
		   &setlist);
  add_show_from_set (c, &showlist);

  c = add_set_cmd ("use-precompiled-symfiles", class_obscure, var_boolean,
		   (char *) &use_mapped_symbol_files,
		   "Set if GDB should use persistent symbol tables by default.",
		   &setlist);
  add_show_from_set (c, &showlist);

  /* APPLE LOCAL: We don't want to raise load levels for MetroWerks.  */
  c = add_set_cmd ("auto-raise-load-levels", class_obscure, var_boolean, 
		   (char *) &should_auto_raise_load_state, 
		   "Set if GDB should raise the symbol loading level on"
		   " all frames found in backtraces.",
		   &setlist);
  add_show_from_set (c, &showlist);

#endif /* HAVE_MMAP */
}
