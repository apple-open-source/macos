/* Persistent Front End (PFE) low-level and common routines.
   Copyright (C) 2001
   Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* 
=====================================================================
============= I M P L E M E N T A T I O N    N O T E S ==============
=====================================================================
IMPLEMENTATION GOALS:
* Provide a generic memory manager implementation, suitable for the
  FSF.
* Under conditionals, provide a version that is tuned for Macintosh 
  OS X.
* Retain structures across a dump and load.
* Avoid dirtying and/or bringing in pages unnecessarily.

IMPLEMENTATION STRATEGIES:
* Keep track of ranges (start and end) for reporting to the PFE, and
  have new allocations extend existing ranges if possible.
  
IMPLEMENTATION ISSUES/QUESTIONS:
* Do we want to support zones?  Perhaps we would want a zone for 
  those things that need to be written out by the PFE and a zone 
  for everything else.  It might be useful to have a zone for the 
  memory manager itself.
* Do we want to have an interface that allows users of the memory
  manager to specify something about the kinds of memory they are
  likely to use (zones, types, sizes, expected number of allocations,
  etc.)?
* What should the default alignment of fragments be?  If memory 
  fragments begin after the "hidden" long that preceeds them 
  (recording the actual size of the fragment), what do we do if
  something requires greater alignment?
* How do we keep track of the biggest available fragment in a block,
  particularly after we use it (or part of it)?  We could scan the
  list up to that point as we look for a block from which to do an
  allocation.  Otherwise, we might have to keep a list in order of
  size.
* Do we want the block list to be a table or a linked list?  A table
  would have some advantages, but, since it has to grow, it would
  require that persistent references to items in the table be via
  indices.
* The block rover strategy may save on space, but it may be
  inefficient in causing many more blocks and fragments to be
  searched when trying to do an allocation, especially when we don't
  find a suitable fragment and are forced to allocate a new block.

TO DO:
* Check for best (or better) fit for fragment, rather than just first.
* Rovers in fragment lists.
* Ensure that no unusable (too small) fragments are created.
* Write freeze/thaw routines for pfe-mem structures, which would
  allow pfe-mem structures to be used across a dump and load.  We
  should do this if we determine that it is worthwhile to use these 
  structures on after a load.  This would allow frees in the load 
  area to work, but this would only have a benefit if we then used 
  this space for allocations.
* Consider splitting large blocks in the free whole block list.
* Gather statistics.  
  - Move malloc/free/etc stats gathering to pfe-mem from pfe.
  - Calculate the real size of freed objects.
* Allow for a bigger initial frag block allocation with smaller
  incremental allocations after that.  The initial block should be
  big enough to handle Carbon.h.

DONE:
* Rover in block list.
* Validate free_bytes in blocks upon termination.
* Adjust criterion for when to alloc a fragment to take into
  account the new max allocatable fragment size after the changes
  to alloc a frag block's block info entry at the beginning of the
  block.
* Shorten final ranges based on what is in the free list.

TUNING:
* Try using different sizes of fragmentable blocks.
=====================================================================
*/

/* Control whether priting memory statistics is enabled.  The actual
   diagnostic messages are controlled by a command line option
   controlling similar PFE diagnostics.  */
#define PFEM_MEMORY_STATS 1

/* Control whether informational diagnostics are on.  */
#define PFEM_DIAGNOSTICS 0

/* Control whether debugging diagnostics are on.  */
#define PFEM_DEBUG 0

/* Control whether or not we will try to minimize memory ranges
   when pfem_id_ranges is called.  At this point, the minimization
   code cuts out memory ranges that are still identified as being
   present in other pfe-mem structures.  This can be fixed.  */
#define MINIMIZE_RANGES 1

/* Flag indicating whether this is an implementation specific for
   Macintosh OS X.  */
#define APPLE_OS_X_IMPLEMENTATION 1

#include "config.h"
#include "system.h"

#include <stddef.h>
#include <stdlib.h>

#if APPLE_OS_X_IMPLEMENTATION
#include <mach/mach.h>
#include <mach/mach_error.h>
#endif

#include "pfe-mem.h"

/* The PFE memory manager structures are as follows:
(1) "range": a range of contiguous memory controlled by the memory
    manager.
(2) "block": a block of memory allocated by a single call to the
    system memory manager.  Blocks are either "whole" or
    "fragmentable", meaning that the block is either allocated as
    a single (whole) unit, or it is carved up into smaller fragments.
(3) "block info": an entry (itself a fragment) corresponding to a
    block, indicating its location, size and whether it is whole or
    fragmentable.  If the block is fragmentable, there is a pointer
    to its list of fragments that are available for allocation.
(4) "fragment": a fragment of memory allocated by the PFE memory
    manager, originally coming from a block.
(5) "range table": a table of the range of addresses covered by the
    blocks that were allocated.  When contiguous blocks are allocated
    they are treated as a single range.  The information in the range
    table is used by the higher level PFE system to know what regions
    of memory needed to be saved in a dump file.
*/

/* Size of a "fragmentable" PFE memory block.  */
#define PFEM_FRAG_BLOCK_SIZE (64 * 1024)

/* Minimum size of a whole (non-fragmentable) PFE memory block.  */
#define PFEM_MIN_WHOLE_BLOCK_SIZE (64 * 1024)

/* Structure for entry in table of memory ranges.  */
struct pfem_range {
  unsigned long start_addr;
  unsigned long end_addr;	/* Actually points to byte past the end.  */
  // ??? Keep track of anything else?
};
typedef struct pfem_range pfem_range;

/* Structure for memory fragments.  When a fragment is allocated,
   a pointer is returned to the memory following the long at the
   beginning of this structure so that the structure can remember
   its original size.  When a fragment is in the list of free
   fragments, its next field points to the next free fragment.  */
struct pfem_fragment {
  size_t size;
  struct pfem_fragment *next;	/* Usable space begins with this field.  */
};
typedef struct pfem_fragment pfem_fragment;

#define PFEM_FRAGMENT_OVERHEAD (sizeof (size_t))

/* Structure for entry in table of memory blocks.  Note that the
   free_bytes and largest_free_fragment_size fields represent the
   actual size (not the usable size after fragment overhead).  
   
   A memory block is either a fragmentable memory block or a whole
   (non-fragmentable) memory block.  Fragmentable blocks are broken
   up into smaller allocations.  Whole blocks are single large
   allocations that are allocated individually and not broken up.
   A whole memory block is identified by a free_fragments pointer
   that has been set to -1.  
   
   The "next" field points to the next block in the primary block
   list (pfem_block_list).  The "next_avail" field points to the
   next block in a particular avail list, e.g., the list of free
   whole blocks.  */
struct pfem_block_info {
  void *addr;
  size_t size;
  size_t free_bytes;
  size_t largest_free_fragment_size;
  pfem_fragment *free_fragments;
  struct pfem_block_info *next;
  struct pfem_block_info *next_avail;
};
typedef struct pfem_block_info pfem_block_info;

/* The maximum size size of a frament is determine by how fragmentable
   blocks are set up.  They are set up to have a fragment containing 
   their block entry information and a fragment containing the usable
   remainder of the block.  Thus the size of the largest usable
   fragment is defined by the following.  */
#define PFEM_MAX_FRAGMENT_SIZE (PFEM_FRAG_BLOCK_SIZE \
				- (PFEM_FRAGMENT_OVERHEAD \
				   + sizeof (pfem_block_info)))

/* The following pointer value is used in pfem_block_info entries 
   as a flag in the free_fragments field to indicate that the
   block is a "whole" (non-fragmentable) block.  */
#define PFEM_INVALID_PTR ((void *) -1)
#define PFEM_IS_WHOLE_BLOCK(x) ((x)->free_fragments == PFEM_INVALID_PTR)

/* List of memory manager blocks.  We keep a pointer to the last
   block since that is the most likely spot a new block will be
   inserted in the list.  */
static pfem_block_info *pfem_block_list = NULL;
static pfem_block_info *pfem_block_list_last = NULL;

/* List of memory manager "whole" (non-fragmentable) blocks that
   are available for allocation, i.e., they were allocated and were
   freed.  */
static pfem_block_info *pfem_whole_block_avail_list = NULL;

/* List of memory manager fragmentable blocks.  All fragmentable
   blocks are on the avail list.  */
static pfem_block_info *pfem_frag_block_avail_list = NULL;

/* Size that a fragment is allowed to vary from the original allocation
   size request to be allocated without splitting the fragment into
   two.  */
// ??? Need to think further about what this should be.
#define MAX_FRAGMENT_SIZE_VARIANCE_BEFORE_SPLIT 12

/* Definitions for the original size of the range table and the
   increment by which to grow it (as needed).  */
// ??? Adjust these values to something reasonable.

#define PFEM_INITIAL_RANGE_TABLE_SIZE 64
#define PFEM_RANGE_TABLE_INCR 64

/* The size a fragment at the end of a fragmentable block must be in 
   order for it to be removed from the address range table before
   address ranges are reported to the PFE.  */
#define PFEM_MIN_RECOVERABLE_FRAGMENT_SIZE 1024

/* Table with entries for each memory range to be "dump"ed.  */
static pfem_range *pfem_range_table = NULL;

/* Current size of the dump range table.  */
static int pfem_range_table_size = 0;

/* Number of items currently in the dump range table.  */
static int pfem_range_table_nitems = 0;

/* Flag whether initialization of the PFE memory manager is complete.  
   This is used by a few routines that deal with memory manager
   structures that must be handled somewhat differently before
   initialization is complete because they cannot yet rely on some
   of the memory manager mechanisms.  */
static int pfem_initialization_complete = 0;

/* Variables to collect statistics on PFE memory utilization.   */
#if PFEM_MEMORY_STATS
extern int pfe_display_memory_stats;
static size_t pfem_malloc_total_size = 0;
static int pfem_mallocs = 0;
static int pfem_callocs = 0;
static int pfem_reallocs = 0;
static int pfem_frees = 0;
#endif

/* Prototypes for static functions.  */

static void pfem_add_block_to_range_table PARAMS ((void *, size_t));
static void pfem_add_block_to_block_list PARAMS ((pfem_block_info *));
static void * pfem_alloc_block PARAMS ((size_t));
static void * pfem_alloc_fragment PARAMS ((size_t));
static void * pfem_alloc_whole_block PARAMS ((size_t));
static pfem_block_info * pfem_find_containing_block PARAMS ((void *));
static pfem_block_info * pfem_set_up_frag_block PARAMS ((void *, size_t));
#if PFEM_MEMORY_STATS
static void pfem_memory_statistics PARAMS ((void));
#endif

/*-------------------------------------------------------------------*/

/* Initialize the PFE's low-level memory manager.  */

void 
pfem_init (void)
{
  void *initial_block;
  pfem_block_info *block_info;
  
  /* Allocate some memory in which to allocate the memory manager's
     own data structures.  */
  initial_block = pfem_alloc_block (PFEM_FRAG_BLOCK_SIZE);
  
  /* Set up the initial fragment from which to do allocations.  After
     this set-up, pfem_malloc should be usable.  */
  block_info = pfem_set_up_frag_block (initial_block, PFEM_FRAG_BLOCK_SIZE);
  pfem_add_block_to_block_list (block_info);
  pfem_frag_block_avail_list = block_info;

  /* Initialize the memory range table.  */
  pfem_range_table_size = PFEM_INITIAL_RANGE_TABLE_SIZE;
  pfem_range_table = (pfem_range *) 
      		     pfem_malloc (sizeof (pfem_range) 
                                         * pfem_range_table_size);
  
  /* Enter the initial block of memory in the range table.  */
  pfem_add_block_to_range_table (initial_block, PFEM_FRAG_BLOCK_SIZE);
  
  /* Signal that initialization is complete.  */
  pfem_initialization_complete = 1;
}

#if PFEM_MEMORY_STATS
/* Emit diagnostic information upon termination.  */

void 
pfem_memory_statistics (void)
{
  int range_idx;
  int block_idx;
  size_t free_fragments_in_block;
  size_t total_free_framents;
  size_t total_free_bytes;
  pfem_block_info *block;
  pfem_fragment *fragment;
  pfem_fragment *last_fragment;
  size_t free_frament_bytes;
  size_t recoverable_fragment_bytes;
  
  printf ("\npfem_term:\n");
    
  printf ("range table:\n");
  for (range_idx = 0; range_idx < pfem_range_table_nitems; range_idx++)
    printf ("  range [%d]: 0x%x - 0x%x; size = %d (0x%x)\n", 
	    range_idx + 1,
	    pfem_range_table[range_idx].start_addr,
	    pfem_range_table[range_idx].end_addr,
	    pfem_range_table[range_idx].end_addr 
	    - pfem_range_table[range_idx].start_addr,
	    pfem_range_table[range_idx].end_addr 
	    - pfem_range_table[range_idx].start_addr);
  
  printf ("\npfem_whole_block_avail_list:\n");
  block_idx = 0;
  for (block = pfem_whole_block_avail_list; block != NULL; block = block->next_avail)
    {
      block_idx++;
      printf ("  free whole block [%d]: addr = 0x%x; size = %d (0x%x)\n",
      	      block_idx, block->addr, block->size, block->size);
    }

  printf ("\npfem_frag_block_avail_list:\n");
  block_idx = 0;
  total_free_bytes = 0;
  total_free_framents = 0;
  recoverable_fragment_bytes = 0;
  for (block = pfem_frag_block_avail_list; block != NULL; block = block->next_avail)
    {
      block_idx++;
      total_free_bytes += block->free_bytes;
      /* Validate the free_bytes info against the size of fragments 
         on the avail list.  */
      free_frament_bytes = 0;
      free_fragments_in_block = 0;
      last_fragment = NULL;
      for (fragment = block->free_fragments; fragment != NULL; fragment = fragment->next)
        {
          free_frament_bytes += fragment->size;
          free_fragments_in_block++;
          last_fragment = fragment;
        }
      if ((last_fragment != NULL) 
      	  && (((unsigned long) last_fragment + last_fragment->size)
      	      == ((unsigned long) block->addr + block->size))
      	  && (last_fragment->size > 1023))
      	recoverable_fragment_bytes += last_fragment->size;
      total_free_framents += free_fragments_in_block;
      printf ("  fragment block [%d]: addr = 0x%x; size = %d; "
      	      "free_bytes = %d; free_fragments_in_block = %d\n", 
      	      block_idx, block->addr, block->size, block->free_bytes,
      	      free_fragments_in_block);
      if (free_frament_bytes != block->free_bytes)
	printf ("    free_bytes != free_frament_bytes (%d)\n", free_frament_bytes);
    }
    printf ("  total_free_bytes = %d\n", total_free_bytes);
    printf ("  total_free_framents = %d\n", total_free_framents);
    printf ("  recoverable_fragment_bytes = %d\n", recoverable_fragment_bytes);
}
#endif

/* Shut down the PFE's low-level memory manager.  */

void 
pfem_term (void)
{
#if PFEM_MEMORY_STATS
  if (pfe_display_memory_stats)
    pfem_memory_statistics ();
#endif
}

/* Put a new block info entry into the global block list.  The list
   is kept in order of ascending addresses.  */
   
void
pfem_add_block_to_block_list (new_block_info)
     pfem_block_info *new_block_info;
{
  pfem_block_info *block;
  pfem_block_info *last_block;
  
  /* First check whether the new block goes at the end of the list,
     which is the mostly likely case.  */
  
  if (pfem_block_list_last == NULL)
    {
      /* The list is empty.  */
      pfem_block_list = new_block_info;
      pfem_block_list_last = new_block_info;
      return;
    }
  if ((unsigned long) new_block_info->addr > (unsigned long) pfem_block_list_last->addr)
    {
      pfem_block_list_last->next = new_block_info;
      pfem_block_list_last = new_block_info;
      return;
    }
  
  /* Check whether the block goes at the front of the list.  */
  
  if (pfem_block_list == NULL)
    {
      /* There should always be a (first) block if things have been
         initialized properly.  */
      printf ("pfem_add_block_to_block_list: pfem_block_list == NULL.\n");
      exit (1);
    }
  if ((unsigned long) new_block_info->addr < (unsigned long)pfem_block_list->addr)
    {
      new_block_info->next = pfem_block_list;
      pfem_block_list = new_block_info;
      return;
    } 
  
  /* The block goes somewhere in the middle of the block list.  */
#if PFEM_DEBUG
  printf ("pfem_add_block_to_block_list: new block goes into middle of block list.\n");
#endif
  last_block = pfem_block_list;
  for (block = pfem_block_list->next; block != NULL; block = block->next)
    {
      if ((unsigned long) new_block_info->addr < (unsigned long)block->addr)
        {
          last_block->next = new_block_info;
          new_block_info->next = block;
          return;
        }
      last_block = block;
    }
#if PFEM_DEBUG
  printf ("pfem_add_block_to_block_list: we should not get here.\n");
#endif
}

/* Given an addr, return a pointer to the block info entry for the
   block containing the address.  */

pfem_block_info *
pfem_find_containing_block (addr)
     void *addr;
{
  pfem_block_info *block;
  pfem_block_info *last_block = NULL;

  for (block = pfem_block_list; block != NULL; block = block->next)
    {
      if ((unsigned long) addr < (unsigned long)block->addr)
        break;
      last_block = block;
    }
  if (last_block == NULL)
    return NULL;
  if ((unsigned long) addr < ((unsigned long) last_block->addr
			      + last_block->size))
    return last_block;
  return NULL;
  
}

/* Add information on a memory block to the range table.  The table 
   of ranges is ordered.  */

void
pfem_add_block_to_range_table (block_addr, size)
     void *block_addr;
     size_t size;
{
  int range_idx;
  unsigned long start_addr = (unsigned long)block_addr;
  unsigned long end_addr = start_addr + size;

  /* Check whether we need to grow the size of the range table.  */
  if (pfem_range_table_nitems >= pfem_range_table_size)
    {
      pfem_range_table_size += PFEM_RANGE_TABLE_INCR;
      pfem_range_table = (pfem_range *) 
                             pfem_realloc (pfem_range_table, 
                                       sizeof (pfem_range) 
                                       * pfem_range_table_size);
    }

  /* Entries in the range table need to be sorted by address.  Perform
     an insertion sort.  Find where the entry goes, shift the rest of 
     the table up, and fill in the range entry.  We coalesce
     contiguous entries, i.e., adjacent ranges are combined into a
     single, larger range.  */
  for (range_idx = 0; range_idx < pfem_range_table_nitems; range_idx++)
    if (start_addr < pfem_range_table[range_idx].start_addr)
      if (end_addr == pfem_range_table[range_idx].start_addr)
        {
          /* The block is just in front of this range, so extend the
             range.  */
          pfem_range_table[range_idx].start_addr = start_addr;
          return;
        }
      else
	{
	  /* The block needs to go before the current entry in the
	     range table, so we move the following entries up to
	     make room for the new entry.  Then we drop out of the
	     loop to where the new entry is added.  */
	  memmove (&pfem_range_table[range_idx + 1], 
		   &pfem_range_table[range_idx], 
		   sizeof (pfem_range) 
		   * (pfem_range_table_nitems - range_idx));
	  break;
	}
    else if (start_addr == pfem_range_table[range_idx].end_addr)
      {
        /* The block follows immediately after this range, so extend
           the range.  */
        pfem_range_table[range_idx].end_addr = end_addr;
        return;
      }
  pfem_range_table[range_idx].start_addr = start_addr;
  pfem_range_table[range_idx].end_addr = end_addr;
  pfem_range_table_nitems++;
}

#if MINIMIZE_RANGES
/* Remove a memory block from the range table.  This is called to
   remove unused (freed) blocks when pfem_id_ranges is called to 
   provide information about what memory ranges are being used.  */

void
pfem_remove_block_from_range_table (block_addr, size)
     void *block_addr;
     size_t size;
{
  int range_idx;
  unsigned long start_addr = (unsigned long)block_addr;
  unsigned long end_addr = start_addr + size;

  /* Entries in the range table are sorted by address.  Find where the 
     range is in the table.  The range we are removing is from a block
     that was used to create the range table, so it should be
     contained in one of the ranges in the table.  */
  for (range_idx = 0; range_idx < pfem_range_table_nitems; range_idx++)
    if (pfem_range_table[range_idx].end_addr >= start_addr)
      {
        /* The range to remove is contained in the current range in
           the range table.  */
        if (start_addr == pfem_range_table[range_idx].start_addr)
          {
            /* The range we are removing begins at the beginning
               of one of the ranges in the table.  */
            if (end_addr == pfem_range_table[range_idx].end_addr)
              {
                /* We are removing a whole range.  Remove the entry
                   and move the rest of the table down.  */
		pfem_range_table_nitems--;
		memmove (&pfem_range_table[range_idx], 
			 &pfem_range_table[range_idx + 1], 
			 sizeof (pfem_range) 
			 * (pfem_range_table_nitems - range_idx));
              }
            else
              {
                /* We are removing part of a range, starting at the
                   beginning of the range, so we move up the start.  */
                pfem_range_table[range_idx].start_addr = end_addr;
              }
          }
        else
          {
            /* The start of the range to remove is in the middle of
               this range.  */
            if (end_addr == pfem_range_table[range_idx].end_addr)
              {
                /* We are removing a bit of the end of this range.
                   Cut its end back.  */
                pfem_range_table[range_idx].end_addr = start_addr;
              }
            else
              {
                /* Neither the start of the end match the endpoints
                   of this range, so we are removing a bit out of
                   the middle of the range.  This means we have to
                   split the range.  */
                unsigned long new_start_addr = end_addr;
                unsigned long new_end_addr = pfem_range_table[range_idx].end_addr;
                
                pfem_range_table[range_idx].end_addr = start_addr;
		
		/* Check whether we need to grow the size of the range table.  */
		if (pfem_range_table_nitems >= pfem_range_table_size)
		  {
		    pfem_range_table_size += 2;
		    pfem_range_table = (pfem_range *) 
					   pfem_realloc (pfem_range_table, 
						     sizeof (pfem_range) 
						     * pfem_range_table_size);
		  }
		
		/* Move the remainder of the dump table up to make
		   room for the new entry.  */
		memmove (&pfem_range_table[range_idx + 2], 
			 &pfem_range_table[range_idx + 1], 
			 sizeof (pfem_range) 
			 * (pfem_range_table_nitems - range_idx));
		pfem_range_table_nitems++;
		range_idx++;
                pfem_range_table[range_idx].start_addr = new_start_addr;
                pfem_range_table[range_idx].end_addr = new_end_addr;
              }
          }
        return;
      }
}
#endif

/* PFE low-level memory manager's block allocator.  This function
   checks internally for allocation failures, so that users need not
   check.  */

void *
pfem_alloc_block (size)
     size_t size;
{
  char *data;
#if APPLE_OS_X_IMPLEMENTATION
  kern_return_t err;
  err = vm_allocate ((vm_map_t) mach_task_self (), (vm_address_t) &data,
  		     size, TRUE);
  if (err != KERN_SUCCESS)
    data = NULL;
#else
  data = xmalloc (size);
#endif
  if (data == NULL)
    {
      printf ("pfem_alloc_block: system allocator failure.\n");
      exit (1);
    }
  if (pfem_initialization_complete)
    pfem_add_block_to_range_table (data, size);
#if PFEM_DIAGNOSTICS
  {
    int alignment;
    unsigned long data2 = (unsigned long) data;
    
    for (alignment = 0; (data2 & 1) == 0; alignment++)
      data2 >>= 1;
    printf ("pfem_alloc_block: allocated block at 0x%x of size %d"
    	    " (2**%d alignment)\n",
  	    data, size, alignment);
  }
#endif
  return data;
}

/* Set up the first fragment in a fragmentable block.  The 
   "first_block" parameter indicates whether this is the first
   fragmentable block being set up by pfem_init.  That first block
   requires special treatment: its block_info record has not yet
   been set up because the block info list does not yet exist.  */
// ??? We may want to allow this to deal with arbitrary size fragmentable blocks.

pfem_block_info *pfem_set_up_frag_block (block_addr, size)
     void *block_addr;
     size_t size;
{
  pfem_fragment *fragment;
  pfem_block_info *block_info;
  
  /* First we carve out a first fragment that will contain the block
     info entry for the fragmentable block that we are allocating.  */
  
  fragment = (pfem_fragment *) block_addr;
  fragment->size = sizeof (pfem_block_info);
  block_info = (pfem_block_info *) &fragment->next;
  
  /* Now create the fragment making up the usable/free portion of
     the block.  */
  fragment = (pfem_fragment *) ((unsigned long) block_addr 
  				+ sizeof (pfem_block_info)
  				+ PFEM_FRAGMENT_OVERHEAD);
  fragment->size = size - (sizeof (pfem_block_info) 
  			   + PFEM_FRAGMENT_OVERHEAD);
  fragment->next = NULL;
  
  /* Initialize the block info entry.  */
  block_info->addr = block_addr;
  block_info->size = size;
  block_info->next = NULL;
  block_info->next_avail = NULL;
  block_info->free_fragments = fragment;
  block_info->free_bytes = fragment->size;
  block_info->largest_free_fragment_size = fragment->size;
  
  return block_info;
}

/* Allocate a large whole (non-fragmentable) memory block.  Such
   blocks have a size of at least PFEM_MIN_WHOLE_BLOCK_SIZE.  */

void * 
pfem_alloc_whole_block (size)
     size_t size;
{
  pfem_block_info *block_info;
  pfem_block_info *last_block_info;
  pfem_block_info *best_block_info = NULL;
  size_t bytes_needed;
  void *block_addr;
  
#if PFEM_DEBUG
  printf ("pfem_alloc_whole_block: request size = %d\n", size);
#endif
  
  /* Adjust the size (if necessary) to take into account alignment
     considerations.  */
  bytes_needed = (size + 1023) & 0xFFFFFC00;
  // ??? What allocation alignment do we want?
#if PFEM_DEBUG
  printf ("  pfem_alloc_whole_block: bytes_needed = %d\n", bytes_needed);
#endif
  
  /* See if there is a whole block that has been freed that can be 
     used.  Look for an exact size match, but remember the best 
     match as we check the free list.  */
  block_info = pfem_whole_block_avail_list;
  last_block_info = NULL;
  while (block_info != NULL)
    {
      if (block_info->size == bytes_needed)
	{
	  /* We found a block that is just the right size.
	     Remove it from the whole block avail list.  */
	  if (last_block_info == NULL)
	    pfem_whole_block_avail_list = block_info->next_avail;
	  else
	    last_block_info->next_avail = block_info->next_avail;
	  block_info->free_bytes = 0;
#if PFEM_DEBUG
	  printf ("  pfem_alloc_whole_block: allocated free block at 0x%x\n", 
		  block_info->addr);
#endif
	  return block_info->addr;
	}
      else if ((block_info->size > bytes_needed))
	{
	  /* ??? If we find a block that is big enough to split and
	     satisfy the request with the remainder still being
	     large enough to be a whole block, consider doing that.  */
	  if ((best_block_info == NULL)
	      || (block_info->size < best_block_info->size))
	    best_block_info = block_info;
	}
      last_block_info = block_info;
      block_info = block_info->next_avail;
    }
  if (best_block_info != NULL)
    {
      /* Consider whether to use the best fit of larger available
	 blocks.  */
      /* ??? Consider what would be an acceptable variance.  */
      /* ??? We may want to break a block up into two whole blocks,
	 or even convert a portion into a fragmentable block.  */
#if PFEM_DEBUG
      printf ("  pfem_alloc_whole_block: bytes_needed = %d;"
	      " best fit found = %d\n", 
	      bytes_needed, best_block_info->size);
#endif
    }
  
  /* Allocate a new whole block and the corresponding block info
     entry.  Fill in the block info entry and add it to the block
     list.  */
  block_addr = pfem_alloc_block (bytes_needed);
  block_info = (pfem_block_info *) pfem_malloc (sizeof (pfem_block_info));
  if (block_info == NULL)
    {
      printf ("pfem_alloc_whole_block: block_info == NULL.\n");
      exit (1);
    }
  block_info->addr = block_addr;
  block_info->size = bytes_needed;
  block_info->free_bytes = 0;
  block_info->largest_free_fragment_size = 0;
  block_info->free_fragments = PFEM_INVALID_PTR;
  block_info->next = NULL;
  block_info->next_avail = NULL;
  pfem_add_block_to_block_list (block_info);
  return block_addr;
}

/* Allocate a fragment of memory, where a fragment is a piece of a 
   larger memory block allocated by the underlying system allocator
   and managed by the pfe-mem system.  Such fragments have a size of 
   smaller than PFEM_MAX_FRAGMENT_SIZE.  */

void * 
pfem_alloc_fragment (size)
     size_t size;
{
#define USE_BLOCK_ROVER 1
#if USE_BLOCK_ROVER
  static pfem_block_info *block_rover = NULL;
#endif
  pfem_block_info *block_info;
  size_t bytes_needed;
  void *block_addr;
  struct best_match_t {
    pfem_block_info *block;
    pfem_fragment *fragment;
    pfem_fragment *last_fragment;
    size_t difference;
    int tries;
  } best_match = { NULL, NULL, NULL, PFEM_FRAG_BLOCK_SIZE, 0 };
  
#if PFEM_DEBUG
  static int call_nbr = 0;
  printf ("pfem_alloc_fragment[%d]: request size = %d\n", ++call_nbr, size);
  if (call_nbr == 0)
    printf ("break here\n");
#endif

  /* Determine the size needed actually needed given the overhead for
     fragments.  */
  bytes_needed = size + PFEM_FRAGMENT_OVERHEAD;
#if PFEM_DEBUG
  printf ("  pfem_alloc_fragment: bytes_needed (with overhead) = %d\n", 
  	  bytes_needed);
#endif
  // ??? Take minimum alignment into account.  Compute this more elegantly.
  bytes_needed = (bytes_needed + 3) & 0xFFFFFFFC;
  
#if PFEM_DEBUG
  printf ("  pfem_alloc_fragment: bytes_needed (after alignment) = %d\n", 
  	  bytes_needed);
#endif

  /* Find a block with enough space available to do the allocation.  */
#if USE_BLOCK_ROVER
  /* Move the rover to the next block after the block from which we
     last allocated.  Initialize the rover if necessary.  If the 
     rover is at the end of the list, move it to the beginning.  */
  if ((block_rover == NULL) || (block_rover->next_avail == NULL))
    block_rover = pfem_frag_block_avail_list;
  else
    block_rover = block_rover->next_avail;
  block_info = block_rover;
#else
  block_info = pfem_frag_block_avail_list;
#endif
  while (1)
    {
      if (block_info == NULL)
	{
	  /* We are at the end of the block list.  Allocate another
	     fragmentable block.  */
	  pfem_block_info *new_block_info;
	  
	  /* Here we have a potentially tricky situation since we
	     need to allocate a new fragmentable block because we 
	     can't satisfy a fragment allocation request, and, in
	     order to allocate the new block we also have to allocate
	     a new fragment for the new block's block info entry.
	     This lead to the decision to allocate the fragment
	     for the block info entry as the first fragment in the
	     new fragmentable block.  */
	  block_addr = pfem_alloc_block (PFEM_FRAG_BLOCK_SIZE);
	  new_block_info = pfem_set_up_frag_block (block_addr, 
	  					   PFEM_FRAG_BLOCK_SIZE);
	  pfem_add_block_to_block_list (new_block_info);
	  
	  /* Put the block in the fragmented block avail list.  */
	  new_block_info->next_avail = pfem_frag_block_avail_list;
	  pfem_frag_block_avail_list = new_block_info;

	  /* Set up to use the new block now.  */
	  block_info = new_block_info;

#if USE_BLOCK_ROVER
	  /* Set the block rover to the new block so that the next
	     allocation search will begin after the new block.  */
          block_rover = block_info;
#endif
#if PFEM_DEBUG
	  printf ("  pfem_alloc_fragment: allocating a new block at 0x%x\n",
		      block_addr);
#endif
	}
#if PFEM_DEBUG
      printf ("  pfem_alloc_fragment: checking block_info at 0x%x\n", 
      	      block_info);
      if (block_info != NULL)
	{
      	  printf ("    pfem_alloc_fragment: block size = %d\n", 
      	  	  block_info->size);
      	  printf ("    pfem_alloc_fragment: block free_bytes = %d\n", 
      	  	  block_info->free_bytes);
      	  printf ("    pfem_alloc_fragment: block largest_free_fragment_size = %d\n", 
      	  	  block_info->largest_free_fragment_size);
      	  printf ("    pfem_alloc_fragment: block free_fragments = 0x%x\n", 
      	  	  block_info->free_fragments);
      	  printf ("    pfem_alloc_fragment: block next = 0x%x\n", 
      	  	  block_info->next);
      	  printf ("    pfem_alloc_fragment: block next_avail = 0x%x\n", 
      	  	  block_info->next_avail);
	}
#endif
#if 0
      // FIXME: keep largest_free_fragment_size up-to-date
      if (bytes_needed <= block_info->largest_free_fragment_size)
        {
	  /* This block has a fragment that is big enough.  Walk the
	     fragment list and find one to use.  */
#else
      if (bytes_needed <= block_info->free_bytes)
        {
	  /* This block may have a fragment that is big enough.  Walk 
	     the fragment list and find one to use.  */
#endif
	  pfem_fragment *fragment = block_info->free_fragments;
	  pfem_fragment *last_fragment = NULL;
	  while (fragment != NULL)
	    {
#if PFEM_DEBUG
	      printf ("  pfem_alloc_fragment: checking fragment of size %d [0x%x] at 0x%x\n",
		      fragment->size, fragment->size, fragment);
	      if (fragment->size > PFEM_FRAG_BLOCK_SIZE)
	        printf ("  ***pfem_alloc_fragment: invalid fragment size.\n");
#endif
	      if (fragment->size >= bytes_needed)
	        {
	          /* See how much extra space there is in the 
	             fragment and decide whether to split it or 
	             not.  */
	          size_t difference = fragment->size - bytes_needed;
	          
#if PFEM_DEBUG
	          printf ("  pfem_alloc_fragment: difference = %d\n", difference);
#endif
	          if (difference > MAX_FRAGMENT_SIZE_VARIANCE_BEFORE_SPLIT)
	            {
	              /* The fragment is big enough that it needs to 
	                 be split.  We split the fragment so that the
	                 allocation will be made from the start of
	                 the fragment and the remainder will be put
	                 on the free fragment list.  */
	              pfem_fragment *new_fragment = (pfem_fragment *) 
	      		((unsigned long)fragment + bytes_needed);
#if PFEM_DEBUG
  		      printf ("  pfem_alloc_fragment: splitting fragment of size %d at 0x%x\n",
  	  		      fragment->size, fragment);
#endif
	      	      new_fragment->size = difference;
	      	      new_fragment->next = fragment->next;
#if PFEM_DEBUG
  		      printf ("  pfem_alloc_fragment: creating new fragment of size %d at 0x%x\n",
  	  		      new_fragment->size, new_fragment);
#endif
	      	      fragment->size = bytes_needed;
	      	      fragment->next = new_fragment;
	            }
	          /* Take the fragment out of the free fragment list.  */
		  if (last_fragment == NULL)
		    block_info->free_fragments = fragment->next;
		  else
		    last_fragment->next = fragment->next;
	          block_info->free_bytes -= fragment->size;
#if PFEM_DEBUG
		  printf ("  pfem_alloc_fragment: allocated fragment at 0x%x of size %d\n",
			  &fragment->next, fragment->size - PFEM_FRAGMENT_OVERHEAD);
#endif
		  return &fragment->next;
	        }
	      last_fragment = fragment;
	      fragment = fragment->next;
	    }
	  /* We should not get here.  If we do, we did not find a
	     fragment that was big enough, even though the 
	     largest_free_fragment_size field in the block info
	     indicated we should.  */
#if 0
	  // ??? The largest_free_fragment_size field is not yet being properly maintained.
	  printf ("  ***pfem_alloc_fragment: did not find fragment.  "
	  	  "largest_free_fragment_size incorrect\n");
#endif
	  // ??? for now we will continue on and try the next block
        }
      block_info = block_info->next_avail;
#if USE_BLOCK_ROVER
      /* If we reach the end of the list when starting from a rover
         block we start over from the beginning.  If we reach the
         rover again, we have gone all the way around the list, so
         we signal that we've reached the end by setting block_info 
         to NULL.  */
      if (block_info == NULL)
        block_info = pfem_frag_block_avail_list;
      if (block_info == block_rover)
        block_info = NULL;
#endif
    }
}
  
/* PFE low-level memory manager's malloc.  Depending on the size of
   the request we either allocate a whole (non-fragmentable) block,
   or a fragment of a bigger block.  */

void * 
pfem_malloc (size)
     size_t size;
{
#if PFEM_DEBUG
  printf ("pfem_malloc: request size = %d\n", size);
#endif
  if (size > PFEM_MAX_FRAGMENT_SIZE)
    return pfem_alloc_whole_block (size);
  else
    return pfem_alloc_fragment (size);
}

/* PFE low-level memory manager's free.  */

void 
pfem_free (ptr)
     void *ptr;
{
  pfem_block_info *block_info;
  pfem_block_info *last_block_info;
  pfem_fragment *fragment_to_free;
  pfem_fragment *fragment;
  pfem_fragment *last_fragment;

  if (!ptr) return;
  
#if PFEM_DEBUG
  printf ("pfem_free: object at 0x%x\n", ptr);
#endif
  /* Find the block containing the object to be freed.  */
  last_block_info = NULL;
  for (block_info = pfem_block_list; 
       block_info != NULL; 
       block_info = block_info->next)
    {
      if ((unsigned long) ptr == (unsigned long) block_info->addr)
        {
          /* The pointer points to the beginning of a block.  This
             should be a pointer to a whole block which can now be
             marked as free.  Anything else is an error because we
             should not have get a pointer to the beginning of a
             fragmentable block because the pointer we give out
             for the first fragment will be 4 bytes into the block,
             due to the leading (hidden) size field.  */
          if (PFEM_IS_WHOLE_BLOCK (block_info))
            {
              if (block_info->free_bytes == block_info->size)
                {
		  printf ("pfem_free: freeing an already free whole block (0x%x).\n",
			  ptr);
		  return;
                }
              /* ??? Consider coalescing blocks on the whole block
                 avail list.  */
              block_info->free_bytes = block_info->size;
              block_info->next_avail = pfem_whole_block_avail_list;
              pfem_whole_block_avail_list = block_info;
#if PFEM_DEBUG
  	      printf ("  pfem_free: freed whole block at 0x%x\n", ptr);
#endif
              return;
            }
          else
            {
	      printf ("pfem_free: trying to free a fragment block (0x%x).\n",
	      	      ptr);
	      exit (1);
            }
        }
      else if ((unsigned long) ptr < (unsigned long) block_info->addr)
        {
          /* The pointer should be somewhere in the preceeding block,
             which should be a fragmentable block, given the pointer
             points to something inside it.  */
          break;
        }
      last_block_info = block_info;
    }
  
  /* If we get here, the object to be freed should be a fragment in 
     the last block.  */
  if (((unsigned long) ptr < (unsigned long) last_block_info->addr)
      || ((unsigned long)ptr >= ((unsigned long) last_block_info->addr
  			 	 + last_block_info->size)))
    {
      printf ("pfem_free: trying to free memory (0x%x) not allocated by "
      	      "the memory manager.\n", ptr);
      exit (1);
    }
  
  /* The block should be a fragmentable block.  */
  if (PFEM_IS_WHOLE_BLOCK (last_block_info))
    {
      printf ("pfem_free: trying to free a fragment (0x%x) in a "
      	      "non-fragmentable block (0x%x).\n", ptr, last_block_info->addr);
      exit (1);
    }
  
  fragment_to_free = (pfem_fragment *) ((unsigned long) ptr - PFEM_FRAGMENT_OVERHEAD);
#if PFEM_DEBUG
  printf ("  pfem_free: freeing fragment at 0x%x of size %d\n", 
  	  fragment_to_free, fragment_to_free->size);
  if (fragment_to_free->size > PFEM_FRAG_BLOCK_SIZE)
    printf ("  ***pfem_free: size > PFEM_FRAG_BLOCK_SIZE\n"); 
#endif
  
  /* If the block has no free fragments, put this one on the list.  */
  if (last_block_info->free_fragments == NULL)
    {
      last_block_info->free_fragments = fragment_to_free;
      fragment_to_free->next = NULL;
      last_block_info->free_bytes = fragment_to_free->size;
      last_block_info->largest_free_fragment_size = fragment_to_free->size;
      return;
    }
  
  /* Find where this fragment goes in the block's fragment list.  */
  last_fragment = NULL;
  for (fragment = last_block_info->free_fragments;
       fragment != NULL;
       fragment = fragment->next)
    {
      if ((unsigned long) fragment_to_free < (unsigned long) fragment)
        break;
      if ((unsigned long) fragment_to_free == (unsigned long) fragment)
        {
	  printf ("pfem_free: attempt to free an already free fragment "
	   	  "(0x%x).\n", fragment_to_free);
          exit (1);
        }
      last_fragment = fragment;
    }
  
  /* Put the fragment into the fragment list.  See whether it can be
     coalesced with the preceeding and/or following block.  If any
     coalescing takes place, we take care to set fragment_to_free
     to the coalesced block to simplify the bookkeeping for the
     block's largest_free_fragment_size.  */
  last_block_info->free_bytes += fragment_to_free->size;
  if (last_fragment == NULL)
    {
      /* The fragment goes before the first fragment in the free list.  */
      last_block_info->free_fragments = fragment_to_free;
    }
  else
    {
      /* There is a preceeding fragment.  See if we can coalesce with
         that.  */
      if (((unsigned long) last_fragment + last_fragment->size)
      	  == (unsigned long) fragment_to_free)
      	{
      	  last_fragment->size += fragment_to_free->size;
      	  fragment_to_free = last_fragment;
#if PFEM_DEBUG
  	  if (last_fragment->size > last_block_info->size)
  	    printf ("***pfem_free: coalesced fragment at 0x%x has size (%d) "
  	    	    "bigger than containing block at 0x%x.\n", 
  	    	    last_fragment, last_fragment->size, last_block_info);
#endif
      	}
       else
        last_fragment->next = fragment_to_free;
    }
  if (fragment == NULL)
    {
      /* The fragment is at the end of the free list.  */
      fragment_to_free->next = NULL;
    }
  else
    {
      /* There is a following fragment.  See if we can coalesce with
         that.  */
      if (((unsigned long) fragment_to_free + fragment_to_free->size)
	   == (unsigned long) fragment)
	{
	  /* Coalesce with the following fragment.  */
	  fragment_to_free->size += fragment->size;
	  fragment_to_free->next = fragment->next;
#if PFEM_DEBUG
  	  if (fragment_to_free->size > last_block_info->size)
  	    printf ("***pfem_free: coalesced fragment at 0x%x has size (%d) "
  	    	    "bigger than containing block at 0x%x.\n", 
  	    	    fragment_to_free, fragment_to_free->size, last_block_info);
#endif
	}
      else
	{
	  fragment_to_free->next = fragment;
	}
    }
  if (fragment_to_free->size > last_block_info->largest_free_fragment_size)
    last_block_info->largest_free_fragment_size = fragment_to_free->size;
}

/* PFE low-level memory manager's calloc: allocates and zeros memory 
   for n objects.  */

void * 
pfem_calloc (nobj, size)
     size_t nobj;
     size_t size;
{
  void *p;
  
  p = pfem_malloc (nobj * size);
  if (p != NULL)
    memset (p, 0, nobj * size);
  return p;
}

/* PFE low-level memory manager's realloc: reallocates memory given
   a pointer to a previous memory allocation.  */

void * 
pfem_realloc (p, size)
     void *p;
     size_t size;
{
  void *p2;
  pfem_block_info *block;
  size_t old_size;
  
  /* Alloc the new memory.  If we fail return NULL and leave the
     original pointer intact.  */
  p2 = pfem_malloc (size);
  if (p2 == NULL)
    return NULL;

  /* Copy the data from the old object to the new object, copying an
     amount determined by the smallest sized object.  In order to 
     determine the old size, we first need to know whether we have a
     whole block or a fragment.  We need to walk the block list to 
     determine what we have.  Free the old pointer.  Note that the
     original pointer is allowed to be NULL, in which case the 
     function behaves like malloc.  */
  if (p != NULL)
    {
      block = pfem_find_containing_block (p);
      if (block == NULL)
	{
	  printf ("pfem_realloc: attempt to realloc from a pointer not alloced.\n");
	  exit (1);
	}
      if (PFEM_IS_WHOLE_BLOCK (block))
	if (p == block->addr)
	  old_size = block->size;
	else
	  {
	    printf ("pfem_realloc: attempt to realloc from inside an alloced object.\n");
	    exit (1);
	  }
      else
	{
	  pfem_fragment *f = (pfem_fragment *) ((unsigned long) p - PFEM_FRAGMENT_OVERHEAD);
	  old_size = f->size;
	}
      memcpy (p2, p, (size < old_size) ? size : old_size);
      pfem_free (p);
    }
  return p2;
}

/* Identify ranges of memory used by the low-level memory manager.
   The parameter is a pointer to a call-back function that is passed
   the end-points of the memory range.  This should only be called
   once at the end of the use of the PFE memory management system
   since this function will modify (minimize) the address range table
   to reflect the memory usage at the point the routine is called.  
   
   NOTE: The current minimization of the block table adjusts the
   block table so that it reflects the memory in use, but not the
   memory that has been allocated.  Thus it is out of sync with the
   other memory structures.  As long as this happens, it will not
   be possible to dump these memory structures and use them after a
   load, unless the structures are brought into sync again.  */

void 
pfem_id_ranges (id_range_fp)
     void (*id_range_fp)(unsigned long, unsigned long);
{
  int range_idx;
#if MINIMIZE_RANGES
  pfem_block_info *block;
  pfem_fragment *fragment;
  pfem_fragment *last_fragment;
  
  /* Minimize the address range table.  */
  for (block = pfem_whole_block_avail_list; block != NULL; block = block->next_avail)
    pfem_remove_block_from_range_table (block->addr, block->size);
  
  /* Minimize the fragmentable block ranges.  For starters, reduce
     the most recent fragmentable block.  */
  for (block = pfem_frag_block_avail_list; block != NULL; block = block->next_avail)
    {
      last_fragment = NULL;
      for (fragment = block->free_fragments; fragment != NULL; fragment = fragment->next)
        last_fragment = fragment;
      if ((last_fragment != NULL) 
      	  && (((unsigned long) last_fragment + last_fragment->size)
      	      == ((unsigned long) block->addr + block->size))
      	  && (last_fragment->size >= PFEM_MIN_RECOVERABLE_FRAGMENT_SIZE))
      	pfem_remove_block_from_range_table (last_fragment, last_fragment->size);
    }
#endif

  /* Inform the PFE about ranges by making a call through the 
     function pointer to identify each range.  */
  for (range_idx = 0; range_idx < pfem_range_table_nitems; range_idx++)
    (*id_range_fp) (pfem_range_table[range_idx].start_addr,
    		    pfem_range_table[range_idx].end_addr);
}

/* Indicates if the pointer is to memory controlled by the PFE 
   memory manager.  (The memory may not currently be allocated.)  */

int 
pfem_is_pfe_mem (ptr)
     void *ptr;
{
  int lower_idx = 0;
  int upper_idx = pfem_range_table_nitems - 1;
  int middle_idx;
  
  while (lower_idx <= upper_idx)
    {
      middle_idx = (lower_idx + upper_idx) / 2;
      if ((unsigned long) ptr < pfem_range_table[middle_idx].start_addr)
	upper_idx = middle_idx - 1;
      else if ((unsigned long) ptr >= pfem_range_table[middle_idx].end_addr)
	lower_idx = middle_idx + 1;
      else
	return 1;
    }
  return 0;
}
