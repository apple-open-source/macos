/* APPLE LOCAL file  subroutine inlining  */
/* Correct handling of inlined functions for the GNU debugger, GDB.

   Copyright 2006
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.
   Written by Caroline Tice.

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

#include <ctype.h>
#include "defs.h"
#include "symtab.h"
#include "frame.h"
#include "breakpoint.h"
#include "symfile.h"
#include "source.h"
#include "demangle.h"
#include "inferior.h"
#include "gdb_assert.h"
#include "frame-unwind.h"
#include "annotate.h"
#include "ui-out.h"
#include "inlining.h"
#include "objfiles.h"
#include "gdbthread.h"
#include "complaints.h"
#include "buildsym.h"

extern int addressprint;

int stepping_into_inlined_subroutine = 0;


/* APPLE LOCAL begin remember stepping into inlined subroutine
   across intervening function calls.  */
/* Contains the start address inlined subroutine being stepped into.  This
   is used if the user is not at the start address of the inlined subroutine
   and chooses to do a step-in.  */

CORE_ADDR inlined_step_range_end = (CORE_ADDR) 0;

/* APPLE LOCAL end remember stepping into inlined subroutine
   across intervening function calls.  */

struct rb_tree_node_list
{
  struct rb_tree_node *node;
  struct rb_tree_node_list *next;
};

struct record_list
{
  struct inlined_call_stack_record *record;
  struct record_list *next;
};

/* The following struct is used to collect and sort line table entries
   before inserting the data into the global_inlined_call_stack.  */

struct pending_node {
  struct linetable_entry *entry;
  struct symtab *s;
  struct pending_node *next;
};

/* The following global data structure keeps a current call stack,
   based on the stop_pc address, of which inlined functions (if any)
   are inlined at the current stop_pc.  */

struct inlined_function_data global_inlined_call_stack;

/* The following data structure is used mostly for constructing
   accurate backtraces.  Given the pc within any function in the call
   stack, if any functions are inlined at that point, their
   corresponding call stack gets built in this data structure.  (Note
   that this means if there are multiple separate inlinings that
   occurred at various positions in the call history, this structure
   will get re-set for each separate set of [possibly nested]
   inlinings.It is also used the inline_frame_sniffer_helper function,
   to determine if the next frame being added to gdb's stack should be
   an inlined frame or not.  */

static struct inlined_function_data temp_frame_stack;

/* The following data structure is used to save the state of the
   global_inlined_call_stack before making a dummy call, and restore
   the stack afterwards.  */

static struct inlined_function_data saved_call_stack;

static int call_stack_initialized = 0;

static void verify_stack (void);
static void find_function_names_and_address_ranges (struct objfile *,
					   struct inlined_call_stack_record *);
static void add_item_to_inlined_subroutine_stack (struct linetable_entry *, 
						  struct symtab *,
						  struct bfd_section *);
static int find_correct_current_position (void);

int inlined_frame_sniffer (const struct frame_unwind *, struct frame_info *,
			   void **);

void inlined_frame_this_id (struct frame_info *, void **, struct frame_id *);

static void reset_temp_frame_stack (void);
static void insert_pending_node (struct pending_node *, struct pending_node **);

/* APPLE LOCAL begin inlined function symbols & blocks  */
static void rb_tree_find_all_nodes_in_between (struct rb_tree_node *, CORE_ADDR,
					       CORE_ADDR, 
					       struct rb_tree_node_list **);
static void rb_tree_find_all_exact_matches (struct rb_tree_node *, CORE_ADDR,
					    CORE_ADDR, 
					    struct rb_tree_node_list **);
/* APPLE LOCAL end inlined funciton symbols & blocks  */

/* Given a set of non-contiguous address ranges (presumably for a function), 
   find the ending address for the function.  Do this by finding the source
   line number corresponding to the ending address of each range; select
   the ending address that resulted in the highest source line number.  

   FIXME: I replaced the use of this function with BLOCK_HIGHEST_PC everywhere
   except for inlining.c which may possibly have a real need to do a pc-to-line
   lookup on all the rangelist entries.  But that's an expensive lookup to do
   and I don't think it's even necessary here.  jsm/2007-03-15  */

static CORE_ADDR
address_range_ending_pc (struct address_range_list *ranges)
{
 struct symtab_and_line sal;
 int j;
 int max_line = 0;
 int max_idx = 0;

 if (!ranges)
   return 0;

 for (j = 0; j < ranges->nelts; j++)
   {
     sal = find_pc_line (ranges->ranges[j].endaddr, 0);
     if (sal.line > max_line)
       {
         max_line = sal.line;
         max_idx = j;
       }
   }

 return ranges->ranges[max_idx].endaddr;
}

/* Return the correct ending address for a function record, whether the
   function consists of a single contiguous range of addresses or 
   multiple non-contiguous ranges of addresses.  */

static CORE_ADDR
record_end_pc (struct inlined_call_stack_record record)
{
  if (record.ranges)
    return address_range_ending_pc (record.ranges);
  else
    return record.end_pc;
}

/* Return 1 if the address ranges for record I in the global_inlined_call_stack
   contain the address PC; return 0 otherwise.  */

static int
record_ranges_contains_pc (int i, CORE_ADDR pc)
{
  int retval;
  int j;

  if (i < 1 || i > global_inlined_call_stack.nelts)
    return 0;

  if (!global_inlined_call_stack.records[i].ranges)
    return 0;

  retval = 0;
  for (j = 0; j < global_inlined_call_stack.records[i].ranges->nelts; j++)
    if (global_inlined_call_stack.records[i].ranges->ranges[j].startaddr <= pc
	&& global_inlined_call_stack.records[i].ranges->ranges[j].endaddr > pc)
      retval = 1;
  
  return retval;
}

/* This function takes two array indices into the temp_frame_stack.
   It copies the contents of the record at FROM into the record at TO,
   then blanks out the record at FROM.  This is to allow inserting
   records into the middle of the existing stack, if that is
   appropriate.  This function is very similar to 
   copy_inlined_call_stack_record, except that this one operates on the 
   temp_frame_stack, whereas that one operates on the 
   global_inlined_call_stack.*/

static void
copy_temp_frame_stack_record (int from, int to)
{
  gdb_assert (from < temp_frame_stack.max_array_size);
  gdb_assert (to < temp_frame_stack.max_array_size);
  gdb_assert (from > 0);
  gdb_assert (to > 0);

  temp_frame_stack.records[to].start_pc = 
                                      temp_frame_stack.records[from].start_pc;
  temp_frame_stack.records[to].end_pc = temp_frame_stack.records[from].end_pc;
  temp_frame_stack.records[to].ranges = temp_frame_stack.records[from].ranges;
  temp_frame_stack.records[to].call_site_line = 
                                temp_frame_stack.records[from].call_site_line;
  temp_frame_stack.records[to].call_site_column = 
                              temp_frame_stack.records[from].call_site_column;
  temp_frame_stack.records[to].s = temp_frame_stack.records[from].s;
  temp_frame_stack.records[to].fn_name = 
                                       temp_frame_stack.records[from].fn_name;
  temp_frame_stack.records[to].calling_fn_name = 
                               temp_frame_stack.records[from].calling_fn_name;
  temp_frame_stack.records[to].call_site_filename = 
                            temp_frame_stack.records[from].call_site_filename;
  temp_frame_stack.records[to].stack_frame_created = 
                           temp_frame_stack.records[from].stack_frame_created;
  temp_frame_stack.records[to].stack_frame_printed = 
                           temp_frame_stack.records[from].stack_frame_printed;
  /* APPLE LOCAL begin radar 6545149  */
  temp_frame_stack.records[to].func_sym = 
                           temp_frame_stack.records[from].func_sym;
  /* APPLE LOCAL end radar 6545149  */
  temp_frame_stack.records[to].stepped_into = 1;

  memset (&(temp_frame_stack.records[from]), 0,
	  sizeof (struct inlined_call_stack_record));
}

/* Given ITEM, representing an inlined line table entry, find or
   create the appropriate record for ITEM in the temp_frame_stack, and
   fill in the fields of the record.  Very similar to
   add_item_to_inlined_subroutine_stack, except this one works on the
   temp_frame_stack whereas that one works on the
   global_inlined_call_stack.  */

static void
add_item_to_temp_frame_stack (struct linetable_entry *item,
			      struct symtab *s,
			      struct bfd_section *section)
{
  int i;
  int j;
  int k;
  int new_pos;
  int nelts = temp_frame_stack.nelts;
  int max_size = temp_frame_stack.max_array_size;

  for (i = 1; i <= nelts; i++)
    if (temp_frame_stack.records[i].start_pc == item->pc
	&& temp_frame_stack.records[i].end_pc == item->end_pc
	&& (item->entry_type == INLINED_SUBROUTINE_LT_ENTRY
	    || temp_frame_stack.records[i].call_site_line == 0
	    || temp_frame_stack.records[i].call_site_line == item->line))
      break;

  if (i > nelts)
    {
      if (nelts >= max_size - 1)
	{
	  if (max_size == 0)
	    {
	      max_size = 10;
	      temp_frame_stack.records = 
		(struct inlined_call_stack_record *) xmalloc
		        (max_size * sizeof (struct inlined_call_stack_record));

	      memset (temp_frame_stack.records, 0,
		      max_size * sizeof (struct inlined_call_stack_record));
	    }
	  else
	    {
	      int old_size = max_size;
	      max_size = 2 * max_size;
	      temp_frame_stack.records = (struct inlined_call_stack_record *)
		xrealloc (temp_frame_stack.records,
			 max_size * sizeof (struct inlined_call_stack_record));

	      for (j = old_size; j < max_size; j++)
		memset (&(temp_frame_stack.records[j]), 0, 
			 sizeof (struct inlined_call_stack_record));
	    }
	}

      temp_frame_stack.max_array_size = max_size;

      new_pos = 0;
      if (item->pc < temp_frame_stack.records[1].start_pc
	  || (item->pc == temp_frame_stack.records[1].start_pc
	      && item->end_pc > temp_frame_stack.records[1].end_pc))
	new_pos = 1;
      else
	{
	  for (j = 1; j < nelts && !new_pos; j++)
	    {
	      k = j + 1;
	      if (temp_frame_stack.records[j].start_pc <= item->pc
		  && item->pc <= temp_frame_stack.records[k].start_pc
		  && temp_frame_stack.records[j].end_pc >= item->end_pc
		  && item->end_pc >= temp_frame_stack.records[k].end_pc)
		new_pos = k;
	    }
	}
      
      if (new_pos)
	{
	  for (j = nelts; j >= new_pos; j--)
	    copy_temp_frame_stack_record (j, j + 1);

	  i = new_pos;
	}

      nelts++;
    }
  
  if (!s)
    s = find_pc_symtab (item->pc);

  gdb_assert (s != NULL);

  if (item->entry_type == INLINED_SUBROUTINE_LT_ENTRY)
    {
      temp_frame_stack.records[i].start_pc = item->pc;
      temp_frame_stack.records[i].end_pc = item->end_pc;
      temp_frame_stack.records[i].s = s;
    }
  else if (item->entry_type == INLINED_CALL_SITE_LT_ENTRY)
    {
      temp_frame_stack.records[i].start_pc = item->pc;
      temp_frame_stack.records[i].end_pc = item->end_pc;
      temp_frame_stack.records[i].call_site_filename = s->filename;
      temp_frame_stack.records[i].call_site_line = item->line;
      temp_frame_stack.records[i].call_site_column = 0;
    }

  temp_frame_stack.records[i].stepped_into = 1;

  find_function_names_and_address_ranges (s->objfile,
					  &(temp_frame_stack.records[i]));

  temp_frame_stack.nelts = nelts;
}

/* Given a PC address, determine what, if any, inlining occurred at
   that PC and update the temp_frame_stack to reflect that inlining.
   Very similar to inlined_function_update_call_stack, except this one
   works on the temp_frame_stack whereas that one works on the
   global_inlined_call_stack.  */

static void
update_tmp_frame_stack (CORE_ADDR pc)
{
  struct symtab *s;
  struct symtab *orig_s;
  struct symtab *alt_symtab = 0;
  struct linetable *l;
  int len;
  struct linetable_entry *alt = NULL;
  struct linetable_entry *item;
  struct linetable_entry *prev;
  struct blockvector *bv;
  asection *section_tmp;
  struct bfd_section *section;
  int i;
  struct linetable_entry *best = NULL;
  CORE_ADDR best_end = 0;
  struct symtab *best_symtab = 0;
  struct pending_node *pending_list;
  struct pending_node *temp;
  struct pending_node *cur_pend;

  if (!dwarf2_allow_inlined_stepping
      || temp_frame_stack.last_pc == pc)
    return;

  
  reset_temp_frame_stack ();

  section_tmp = find_pc_overlay (pc);
  if (pc_in_unmapped_range (pc, section_tmp))
    pc = overlay_mapped_address (pc, section_tmp);
  section = (struct bfd_section *) section_tmp;


  s = find_pc_sect_symtab (pc, section);
  orig_s = s;
  if (s)
    {
      bv = BLOCKVECTOR (s);

      /* Look at all the symtabs that share this blockvector.
         They all have the same apriori range, that we found was right;
         but they have different line tables.  */

      for ( ; s && BLOCKVECTOR (s) == bv; s = s->next)
	{
	  l = LINETABLE (s);
	  if (!l)
	    continue;
	  len = l->nitems;
	  if (len <= 0)
	    continue;

	  pending_list = NULL;
	  prev = NULL;
	  item = l->item;
	  if (item->pc > pc && (!alt || item->pc < alt->pc))
	    {
	      alt = item;
	      alt_symtab = s;
	    }

	  for (i = 0; i < len; i++, item++)
	    {
	      if ((item->entry_type == INLINED_SUBROUTINE_LT_ENTRY
		   || item->entry_type == INLINED_CALL_SITE_LT_ENTRY)
		  && item->pc <= pc
		  && item->end_pc > pc)
		{
		  /* Store the item(s) in a sorted list; after all
		     of the items for a particular pc have been
		     collected and sorted, they get added to the
		     call stack in the correct order.  */

		  temp = (struct pending_node *) 
		                     xmalloc (sizeof (struct pending_node));
		  temp->entry = item;
		  temp->s = s;
		  temp->next = NULL;
		  insert_pending_node (temp, &pending_list);
		}
	      
	      if (item->pc > pc
		  || (item->pc == pc
		      && prev
		      && item->pc == prev->pc))
		break;
	      
	      prev = item;
	    }	  
	  
	  if (prev && prev->line && (!best || prev->pc > best->pc))
	    {
	      best = prev;
	      best_symtab = s;
	      
	      if (best_end <= best->pc)
		best_end = 0;
	    }
	  
	  if (best_symtab
	      && best->line != 0
	      && prev 
	      && prev->pc == item->pc)
	    {
	      while (prev->pc == item->pc)
		{
		  prev = item;
		  item++;
		  if ((item->entry_type ==  INLINED_SUBROUTINE_LT_ENTRY
		       || item->entry_type == INLINED_CALL_SITE_LT_ENTRY)
		      && item->pc <= pc
		      && item->end_pc > pc)
		    {
		      /* Store the item(s) in a sorted list; after all
			 of the items for a particular pc have been
			 collected and sorted, they get added to the
			 call stack in the correct order.  */

		      temp = (struct pending_node *) 
			            xmalloc (sizeof (struct pending_node));
		      temp->entry = item;
		      temp->s = s;
		      temp->next = NULL;
		      insert_pending_node (temp, &pending_list);
		    }
		}
	      best = prev;
	    }

	  for (cur_pend = pending_list; cur_pend; cur_pend = cur_pend->next)
	    add_item_to_temp_frame_stack (cur_pend->entry, cur_pend->s,
					  section);
	}
    }

  temp_frame_stack.current_pos = temp_frame_stack.nelts;

  if (temp_frame_stack.nelts > 0)
    temp_frame_stack.last_inlined_pc = pc;
  temp_frame_stack.last_pc = pc;
}

/* Given an array index I, and an address PC, this function determines
   if the PC falls within the address ranges for the record at position I
   in the temp_frame_stack.  */

static int
tmp_frame_record_ranges_contains_pc (int i, CORE_ADDR cur_pc)
{
  int retval;
  int j;
  
  if (i < 1 || i > temp_frame_stack.nelts)
    return 0;

  if (!temp_frame_stack.records[i].ranges)
    return 0;

  retval = 0;
  for (j = 0; j < temp_frame_stack.records[i].ranges->nelts; j++)
    if (temp_frame_stack.records[i].ranges->ranges[j].startaddr <= cur_pc
	&& temp_frame_stack.records[i].ranges->ranges[j].endaddr > cur_pc)
      retval = 1;

  return retval;
}


/* Returns the current position in the temp_frame_stack.  */

static int
current_tmp_frame_stack_position (void)
{
  return temp_frame_stack.current_pos;
}

/* Given the pc address for a frame, CUR_PC, this function determines
   if the pc falls within an inlined function call, and if so, it
   returns the address for the end of the inlining in the parameter
   INLINE_END_PC. */

static int
tmp_frame_in_inlined_function_call_p (CORE_ADDR cur_pc,
				      CORE_ADDR *inline_end_pc)
{
  int ret_val = 0;
  int i;
  int low = 0;
  int high = 0;

  *inline_end_pc = (CORE_ADDR) 0;

  for (i = 1; i <= temp_frame_stack.nelts; i++)
    if ((temp_frame_stack.records[i].ranges
	 && tmp_frame_record_ranges_contains_pc (i, cur_pc))
	|| (!temp_frame_stack.records[i].ranges
	    && (temp_frame_stack.records[i].start_pc <= cur_pc
		&& cur_pc < temp_frame_stack.records[i].end_pc)))
      {
	if (!low)
	  low = i;
	high = i;
      }

  if (low > 0)
    {
      if (low <= current_tmp_frame_stack_position ()
	  && current_tmp_frame_stack_position () <= high)
	i = current_tmp_frame_stack_position ();
      else
	i = high;

      if (!temp_frame_stack.records[i].ranges)
	*inline_end_pc = temp_frame_stack.records[i].end_pc;
      else
	*inline_end_pc = address_range_ending_pc (temp_frame_stack.records[i].ranges);

      ret_val = i;
    }

  return ret_val;
}

/* Given a newly updated set of records for the
   global_inlined_call_stack, make our best guess as to what the
   current position in the stack of records should be.  */

static int
find_correct_current_position (void)
{
  int i;
  int ret_val = 0;

  if (global_inlined_call_stack.records[1].start_pc == stop_pc)
    ret_val = 1;
  else
    for (i = 1; i <= global_inlined_call_stack.nelts; i++)
      {
	if (global_inlined_call_stack.records[i].ranges)
	  {
	    if (record_ranges_contains_pc (i, stop_pc))
	      {
		global_inlined_call_stack.records[i].stepped_into = 1;
		ret_val = i;
	      }
	  }
	else if (global_inlined_call_stack.records[i].start_pc <= stop_pc
		&& stop_pc < record_end_pc (global_inlined_call_stack.records[i]))
	  {
	    global_inlined_call_stack.records[i].stepped_into = 1;
	    ret_val = i;
	  }
      }
    
  return ret_val;
}

/* Initialize the global_inlined_call_stack data.  */

void
inlined_function_initialize_call_stack (void)
{
  global_inlined_call_stack.last_pc = (CORE_ADDR) 0;
  global_inlined_call_stack.last_inlined_pc = (CORE_ADDR) 0;
  global_inlined_call_stack.max_array_size = 10;
  global_inlined_call_stack.nelts = 0;
  global_inlined_call_stack.current_pos = 0;
  global_inlined_call_stack.records = 
    (struct inlined_call_stack_record *) xmalloc 
                                 (10 * sizeof (struct inlined_call_stack_record));
  memset (global_inlined_call_stack.records, 0, 
	  10 * sizeof (struct inlined_call_stack_record));


  saved_call_stack.last_pc = (CORE_ADDR) 0;
  saved_call_stack.last_inlined_pc = (CORE_ADDR) 0;
  saved_call_stack.max_array_size = 10;
  saved_call_stack.nelts = 0;
  saved_call_stack.current_pos = 0;
  saved_call_stack.records = 
    (struct inlined_call_stack_record *) xmalloc 
                                 (10 * sizeof (struct inlined_call_stack_record));
  memset (saved_call_stack.records, 0, 
	  10 * sizeof (struct inlined_call_stack_record));


  temp_frame_stack.last_pc = (CORE_ADDR) 0;
  temp_frame_stack.last_inlined_pc = (CORE_ADDR) 0;
  temp_frame_stack.max_array_size = 10;
  temp_frame_stack.nelts = 0;
  temp_frame_stack.current_pos = 0;
  temp_frame_stack.records = 
    (struct inlined_call_stack_record *) xmalloc 
                                 (10 * sizeof (struct inlined_call_stack_record));
  memset (saved_call_stack.records, 0, 
	  10 * sizeof (struct inlined_call_stack_record));


  call_stack_initialized = 1;
}

static void
reset_saved_call_stack (void)
{
  saved_call_stack.last_pc = (CORE_ADDR) 0;
  saved_call_stack.last_inlined_pc = (CORE_ADDR) 0;
  saved_call_stack.nelts = 0;
  saved_call_stack.current_pos = 0;
  /* Re-use the already malloc'd space rather than freeing & re-mallocing.  */
  memset (saved_call_stack.records, 0, 
   saved_call_stack.max_array_size * sizeof (struct inlined_call_stack_record));
}

/* Blank out the temp_frame_stack.  */

static void
reset_temp_frame_stack (void)
{
  temp_frame_stack.last_pc = (CORE_ADDR) 0;
  temp_frame_stack.last_inlined_pc = (CORE_ADDR) 0;
  temp_frame_stack.nelts = 0;
  temp_frame_stack.current_pos = 0;
  /* Re-use the already malloc'd space rather than freeing & re-mallocing.  */
  memset (temp_frame_stack.records, 0, 
   temp_frame_stack.max_array_size * sizeof(struct inlined_call_stack_record));
}

/* Re-initialize the global_inlined_call_stack data (set it all back
   to zero).  */

void
inlined_function_reinitialize_call_stack (void)
{
  global_inlined_call_stack.last_pc = (CORE_ADDR) 0;
  global_inlined_call_stack.last_inlined_pc = (CORE_ADDR) 0;
  global_inlined_call_stack.nelts = 0;
  global_inlined_call_stack.current_pos = 0;
  /* Re-use the already malloc'd space rather than freeing & re-mallocing.  */
  memset (global_inlined_call_stack.records, 0, 
    global_inlined_call_stack.max_array_size 
	                          * sizeof (struct inlined_call_stack_record));

  reset_saved_call_stack ();
}

/* Return flag indicating if global_inlined_call_stack data has been
   initialized at least once or not.  */

int
inlined_function_call_stack_initialized_p (void)
{
  return call_stack_initialized;
}


/* Return the value of the last PC for which the global_inlined_call_stack
   data was  updated.  */

CORE_ADDR
inlined_function_call_stack_pc (void)
{
  return global_inlined_call_stack.last_pc;
}

/* Update the last_pc field of the global_inlined_call_stack to
   contain the value of NEW_PC (this is the last PC value at which the
   data in the stack was updated).  */

void
inlined_function_update_call_stack_pc (CORE_ADDR new_pc)
{
  global_inlined_call_stack.last_pc = new_pc;
}

/* Given the indices of two records in the global_inlined_call_stack,
   verify that all the address ranges in INNER are contained within all the
   address ranges of OUTER.  

   The OUTER record here refers to a function record lower down on the
   global_inlined_call_stack, which means that it is an inlined function
   that called another function (referred to by the INNER record), which
   was inlined into OUTER.  Since INNER was inlined into OUTER, all the
   code addresses of INNER must fit within all the code addresses for OUTER
   or we have a major error somewhere. */

static int
inlined_function_address_ranges_properly_contained (int outer, int inner)
{
  struct inlined_call_stack_record *outer_record;
  struct inlined_call_stack_record *inner_record;
  int okay = 1;
  int fits_some;
  int i;
  int j;
  CORE_ADDR inner_start;
  CORE_ADDR inner_end;
  CORE_ADDR outer_start;
  CORE_ADDR outer_end;
  
  /* Verify that the values of INNER and OUTER are valid, i.e.
     1 <= outer < inner <= global_inlined_call_stack.nelts. */
  
  if (outer < 1 || inner <= outer 
      || inner > global_inlined_call_stack.nelts)
    okay = 0;

  /* The following block of code duplicates the logic in contained_in()
     but operates on an inlined_call_stack_record instead of a block.  */

  if (okay)
    {
      outer_record = &(global_inlined_call_stack.records[outer]);
      inner_record = &(global_inlined_call_stack.records[inner]);
      
      if (inner_record->ranges && outer_record->ranges)
	{
	  /* Both inlined functions contain multiple non-contiguous ranges
	     of addresses.  Address ranges should not have overlapping boundaries,
	     so verify that EVERY address range in the inner record either fits 
	     entirely inside SOME address range of the outer record, or entirely 
	     outside all of them.  Also, at least ONE address range of the inner
	     record must fit inside at least ONE address range of the outer 
	     record.  */

	  fits_some = 0;
	  for (i = 0; i < inner_record->ranges->nelts; i++)
	    {
	      inner_start = inner_record->ranges->ranges[i].startaddr;
	      inner_end   = inner_record->ranges->ranges[i].endaddr;
	      for (j = 0; j < outer_record->ranges->nelts && !fits_some; j++)
		{
		  outer_start = outer_record->ranges->ranges[j].startaddr;
		  outer_end   = outer_record->ranges->ranges[j].endaddr;
		  if (outer_start <= inner_start
		      && inner_start < outer_end)
		    {
		      /* Since the start of the inner range is inside the
			 outer range, then the end must also be inside
			 the outer range.  */
		      if (inner_end <= outer_end)
			fits_some = 1;
		      else
			okay = 0;
		    }
		  else if (outer_start < inner_end
			   && inner_end <= outer_end)
		    /* Since the inner range did not start inside the outer
		       range, it is not allowed to end inside the outer range.  */
		    okay = 0;
		} /* for j*/
	    } /* for i  */

	  if (!fits_some)
	    okay = 0;
	}
      else if (inner_record->ranges)
	{
	  /* The inner record contains multiple address ranges, but the outer
	     record only has a single range.  Verify that each of the address 
	     ranges of the inner record either fit entirely inside the range of 
	     the outer record or entirely outside it.  Also verify that at least
	     one inner range fits inside the outer range.  */

	  outer_start = outer_record->start_pc;
	  outer_end   = outer_record->end_pc;
	  fits_some = 0;
	  for (i = 0; i < inner_record->ranges->nelts && okay; i++)
	    {
	      inner_start = inner_record->ranges->ranges[i].startaddr;
	      inner_end   = inner_record->ranges->ranges[i].endaddr;
	      if (outer_start <= inner_start
		  && inner_start < outer_end)
		{
		  /* Assumption: inner_start <= inner_end, therefore 
		     at this point outer_start must be < inner_end. */
		  if (inner_end <= outer_end)
		    fits_some = 1;
		  else
		    okay = 0;
		}
	      /* We know inner_start is not in the outer range, so inner_end
		 had better not be either.  */
	      else if (outer_start < inner_end
		       && inner_end <= outer_end)
		okay = 0;
	    }

	  if (!fits_some)
	    okay = 0;
	}
      else if (outer_record->ranges)
	{
	  /* The outer record contains multiple address ranges, but the inner
	     record only a single address range.  Verify that the address
	     range of INNER fits inside SOME address range of OUTER. */

	  fits_some = 0;
	  for (i = 0; i < outer_record->ranges->nelts && !fits_some; i++)
	    if (inner_record->start_pc >= outer_record->ranges->ranges[i].startaddr
		&& inner_record->end_pc <= outer_record->ranges->ranges[i].endaddr)
	      fits_some = 1;

	  okay = fits_some;
	}
      else
	{
	  /* Both INNER and OUTER consist of a single contiguous range of
	     addresses.  Verify that INNER's range fits inside OUTER's range.  */

	  if (inner_record->start_pc < outer_record->start_pc
	      || inner_record->end_pc > outer_record->end_pc)
	    okay = 0;
	}
    }

  return okay;
}

/* Make sure the stack of records in global_inlined_call_stack is valid,
   i.e. as you go up the stack each new record's starting and ending pc
   are contained within the starting/ending pc of the record below it.  */

static void
verify_stack (void)
{
  int i;

  if (global_inlined_call_stack.current_pos > global_inlined_call_stack.nelts)
    internal_error (__FILE__, __LINE__, 
		    _("Illegal position in inlined call stack."));

  if (global_inlined_call_stack.nelts > 1)
    {
      for (i = 2; i <= global_inlined_call_stack.nelts; i++)
	if (!inlined_function_address_ranges_properly_contained (i-1, i))
	  internal_error (__FILE__, __LINE__, 
			  _("Inconsistent inlined call stack."));
    }
}


/* Given a red-black tree (ROOT) containing inlined subroutine
   records, find all records whose inlining start address (main key)
   is greater than or equal to START, and whose inlining end address
   (third_key) is less than or equal to END.  Return all such records
   in the list MATCHES.  */

static void
rb_tree_find_all_nodes_in_between (struct rb_tree_node *root, CORE_ADDR start,
				   CORE_ADDR end, 
				   struct rb_tree_node_list **matches)
{
  struct rb_tree_node_list *tmp_node;

  if (!root)
    return;

    if (start <= root->key
        && root->key < end
	&& root->third_key < end)
      {
	tmp_node = (struct rb_tree_node_list *) xmalloc (sizeof (struct rb_tree_node_list));
	tmp_node->node = root;
	tmp_node->next = *matches;
	*matches = tmp_node;
      }

    if (start <= root->key)
      {
	rb_tree_find_all_nodes_in_between (root->left, start, end, matches);
	rb_tree_find_all_nodes_in_between (root->right, start, end, matches);
      }
    else if (start > root->key)
      rb_tree_find_all_nodes_in_between (root->right, start, end, matches);
}

/* APPLE LOCAL begin inlined function symbols & blocks  */

/* Given a red-black tree (ROOT) containing inlined subroutine records, find
   all records whose inlining start address (main key) equals KEY and whose
   inlining end address (third key) equals THIRD_KEY.  Return all such records
   in the list MATCHES.  */

static void
rb_tree_find_all_exact_matches (struct rb_tree_node *root, CORE_ADDR key,
				CORE_ADDR third_key, 
				struct rb_tree_node_list **matches)
{
  struct rb_tree_node_list *tmp_node;
  
  if (!root)
    return;
  
  if (key == root->key)
    {
      if (third_key == root->third_key)
	{
	  tmp_node = (struct rb_tree_node_list *) xmalloc (sizeof (struct rb_tree_node_list));
	  tmp_node->node = root;
	  tmp_node->next = *matches;
	  *matches = tmp_node;
	  
	  rb_tree_find_all_exact_matches (root->left, key, third_key, matches);
	  rb_tree_find_all_exact_matches (root->right, key, third_key, matches);
	}
      else if (third_key < root->third_key)
	rb_tree_find_all_exact_matches (root->left, key, third_key, matches);
      else
	rb_tree_find_all_exact_matches (root->right, key, third_key, matches);
    }
  else if (key < root->key)
    rb_tree_find_all_exact_matches (root->left, key, third_key, matches);
  else
    rb_tree_find_all_exact_matches (root->right, key, third_key, matches);
}
/* APPLE LOCAL end inlined function symbols & blocks  */

static void
rb_tree_find_all_matching_nodes (struct rb_tree_node *root, CORE_ADDR key,
				 int secondary_key, CORE_ADDR third_key,
				 struct rb_tree_node_list **matches)
{
  struct rb_tree_node_list *tmp_node;

  if (!root)
    return;

  if (key == root->key)
    {
      if (secondary_key == root->secondary_key)
	{
	  if (third_key == root->third_key)
	    {
	      tmp_node = (struct rb_tree_node_list *) xmalloc (sizeof (struct rb_tree_node_list));
	      tmp_node->node = root;
	      tmp_node->next = *matches;
	      *matches = tmp_node;

	      rb_tree_find_all_matching_nodes (root->left, key, secondary_key,
					       third_key, matches);
	      rb_tree_find_all_matching_nodes (root->right, key, secondary_key,
					       third_key, matches);
	    }
	  else if (third_key < root->third_key)
	    rb_tree_find_all_matching_nodes (root->left, key, secondary_key,
					     third_key, matches);
	  else
	    rb_tree_find_all_matching_nodes (root->right, key, secondary_key,
					     third_key, matches);
	}
      else if (secondary_key < root->secondary_key)
	rb_tree_find_all_matching_nodes (root->left, key, secondary_key,
					 third_key, matches);
      else
	rb_tree_find_all_matching_nodes (root->right, key, secondary_key,
					 third_key, matches);
    }
  else if (key < root->key)
    rb_tree_find_all_matching_nodes (root->left, key, secondary_key, third_key,
				     matches);
  else
    rb_tree_find_all_matching_nodes (root->right, key, secondary_key, third_key,
				     matches);
}

/* Given a tree node, ROOT, whose record.start_pc and record.end_pc match
   START_PC and END_PC, search the rest of the tree to see if there is another
   tree node that matches (this is a possibility).  */

static struct rb_tree_node *
rb_tree_find_next_node (struct rb_tree_node *root, long long key, int secondary_key,
			long long third_key)
{
  /* First look for a right-hand sibling.  If found, return that.  Otherwise, look
     for children.  */

  if (root->parent
      && root == root->parent->left
      && root->parent->key == key
      && root->parent->third_key == third_key
      && root->parent->right)
    {
      if (root->parent->right->key == key
	  && root->parent->right->third_key == third_key)
	return root->parent->right;
    }
  else if (root->left 
	   && root->left->key == key
	   && root->left->third_key == third_key)
    return root->left;
  else if (root->right
	   && root->right->key == key
	   && root->right->third_key == third_key)
    return root->right;
  else
    return NULL;

  return NULL;
}

static void
add_to_list (struct inlined_call_stack_record *new_record,
	     struct record_list **found_records)
{
  struct record_list *tmp_node;

  tmp_node = (struct record_list *) xmalloc (sizeof (struct record_list));
  tmp_node->record = new_record;
  tmp_node->next = *found_records;
  *found_records = tmp_node;
}

/* Given the root of a binary tree, ROOT, and a function name, NAME, do an in-order
   traversal of the tree searching for the name.  Search the entire tree for all
   records tha match, and build up a list FOUND_RECORDS of those matches.  */

static void
search_tree_for_name (struct rb_tree_node *root, char *name, 
		      struct record_list **found_records)
{
  struct inlined_call_stack_record *tmp_record;

  if (root)
    {
      tmp_record = (struct inlined_call_stack_record *) root->data;

      if (tmp_record->fn_name
	  && strcmp (tmp_record->fn_name, name) == 0)
	add_to_list (tmp_record, found_records);

      search_tree_for_name (root->left, name, found_records);
      search_tree_for_name (root->right, name, found_records);
    }
}

/* Given a partially filled in record for the global_inlined_call_stack,
   search through the inlined_subroutine_data of the appropriate objfile 
   to find the appropriate function names for the caller and callee and fill 
   them in.  */

static void
find_function_names_and_address_ranges (struct objfile *objfile,
				     struct inlined_call_stack_record *record)
{
  struct rb_tree_node *tmp_node;
  struct inlined_call_stack_record *tmp_record;
  struct rb_tree_node_list *matches = NULL;
  struct rb_tree_node_list *next;
  struct rb_tree_node_list *current;
  int match_found = 0;

  rb_tree_find_all_matching_nodes (objfile->inlined_subroutine_data,
				   record->start_pc, 0, record->end_pc,
				   &matches);


  for (current = matches; current && !match_found; current = current->next)
    {
      tmp_node = current->node;

      if (tmp_node != NULL)
	{
	  tmp_record = (struct inlined_call_stack_record *) tmp_node->data;
	  if (!record->call_site_line
	      || tmp_record->call_site_line == record->call_site_line)
	    {
	      record->fn_name = tmp_record->fn_name;
	      record->calling_fn_name = tmp_record->calling_fn_name;
	      record->ranges = tmp_record->ranges;
	      /* APPLE LOCAL radar 6545149  */
	      record->func_sym = tmp_record->func_sym;
	      match_found = 1;
	    }
	}
    }
  
  current = matches;
  while (current)
    {
      next = current->next;
      xfree (current);
      current = next;
    }
}

/* Copy the data from one record in the global_inlined_call_stack to another.  */

static void
copy_inlined_call_stack_record (int from, int to)
{
  gdb_assert (from < global_inlined_call_stack.max_array_size);
  gdb_assert (to < global_inlined_call_stack.max_array_size);
  gdb_assert (from > 0);
  gdb_assert (to > 0);

  global_inlined_call_stack.records[to].start_pc = 
                            global_inlined_call_stack.records[from].start_pc;
  global_inlined_call_stack.records[to].end_pc = 
                            global_inlined_call_stack.records[from].end_pc;
  global_inlined_call_stack.records[to].ranges = 
                            global_inlined_call_stack.records[from].ranges;
  global_inlined_call_stack.records[to].call_site_line = 
                            global_inlined_call_stack.records[from].call_site_line;
  global_inlined_call_stack.records[to].call_site_column = 
                            global_inlined_call_stack.records[from].call_site_column;
  global_inlined_call_stack.records[to].s = 
                            global_inlined_call_stack.records[from].s;
  global_inlined_call_stack.records[to].fn_name =
                            global_inlined_call_stack.records[from].fn_name;
  global_inlined_call_stack.records[to].calling_fn_name =
                            global_inlined_call_stack.records[from].calling_fn_name;
  global_inlined_call_stack.records[to].call_site_filename =
                            global_inlined_call_stack.records[from].call_site_filename;
  global_inlined_call_stack.records[to].stack_frame_created = 
                            global_inlined_call_stack.records[from].stack_frame_created;
  global_inlined_call_stack.records[to].stack_frame_printed = 
                            global_inlined_call_stack.records[from].stack_frame_printed;
  global_inlined_call_stack.records[to].stepped_into = 
                            global_inlined_call_stack.records[from].stepped_into;

  /* APPLE LOCAL begin radar 6545149  */
  global_inlined_call_stack.records[to].func_sym = 
                            global_inlined_call_stack.records[from].func_sym;
  /* APPLE LOCAL end radar 6545149  */

  /* Blank out the old record so that we don't end up with stale data mixing with
     new data.  */

  memset (&(global_inlined_call_stack.records[from]), 0, 
	  sizeof (struct inlined_call_stack_record));
}

/* Given a line table entry and its symtab, make sure the appropriate record
   exists in the global_inlined_call_stack data.  Create a new record if
   necessary.  */

static void
add_item_to_inlined_subroutine_stack (struct linetable_entry *item, 
				      struct symtab *s,
				      struct bfd_section *section)
{
  int i;
  int j;
  int k;
  int new_pos;
  int nelts = global_inlined_call_stack.nelts;
  int max_size = global_inlined_call_stack.max_array_size;

  /* Set 'i' to the position for ITEM's data in the call stack.  If there
     is an existing entry for ITEM, then i will be less than nelts.  Otherwise
     we need a new entry on the top of the stack.  */
  
  for (i = 1; i <= nelts; i++)
    {
      if (global_inlined_call_stack.records[i].start_pc == item->pc
	  && global_inlined_call_stack.records[i].end_pc == item->end_pc
	  && (item->entry_type == INLINED_SUBROUTINE_LT_ENTRY
	      || global_inlined_call_stack.records[i].call_site_line == 0
	      || global_inlined_call_stack.records[i].call_site_line == item->line))
	break;
    }

  if (i > nelts)
    {
      /* An existing entry was not found; we need to make a new entry
	 on the top of the stack.  First we need to make sure we have
	 enough space in the array for a new entry; otherwise we need
	 to resize the array.  */
      if (nelts >= max_size - 1)
	{
	  if (max_size == 0)
	    {
	      max_size = 10;
	      global_inlined_call_stack.records = 
		(struct inlined_call_stack_record *) xmalloc 
	               (max_size * sizeof (struct inlined_call_stack_record));
	      memset (global_inlined_call_stack.records, 0,
		      max_size * sizeof (struct inlined_call_stack_record));

	      saved_call_stack.records = 
		(struct inlined_call_stack_record *) xmalloc 
	                   (max_size * sizeof (struct inlined_call_stack_record));
	      memset (saved_call_stack.records, 0,
		      max_size * sizeof (struct inlined_call_stack_record));
	    }
	  else
	    {
	      int old_size =  max_size;
	      max_size = 2 * max_size;
	      global_inlined_call_stack.records = 
		(struct inlined_call_stack_record *) xrealloc 
	                  (global_inlined_call_stack.records,
	                   max_size * sizeof (struct inlined_call_stack_record));

	      saved_call_stack.records = 
		(struct inlined_call_stack_record *) xrealloc 
	                  (saved_call_stack.records,
	                   max_size * sizeof (struct inlined_call_stack_record));

	      for (j = old_size; j < max_size; j++)
		{
		  memset (&(global_inlined_call_stack.records[j]), 0,
			  sizeof (struct inlined_call_stack_record));
		  memset (&(saved_call_stack.records[j]), 0,
			  sizeof (struct inlined_call_stack_record));
		}
	    }
	}

      global_inlined_call_stack.max_array_size = max_size;
      saved_call_stack.max_array_size = max_size;

      /* An existing entry was not found; check to see if the new entry should
	 go between existing entries...  */

      new_pos = 0;

      /* Does new entry go at the bottom of the stack? */

      if (item->pc < global_inlined_call_stack.records[1].start_pc
	  || (item->pc == global_inlined_call_stack.records[1].start_pc
	      && item->end_pc > global_inlined_call_stack.records[1].end_pc))
	new_pos = 1;
      else
	{
	  /* Does new entry go between existing entries?  */

	  for (j = 1; j < nelts && !new_pos; j++)
	    {
	      k = j + 1;
	      if (global_inlined_call_stack.records[j].start_pc <= item->pc
		  && item->pc <= global_inlined_call_stack.records[k].start_pc
		  && global_inlined_call_stack.records[j].end_pc >= item->end_pc
		  && item->end_pc >= global_inlined_call_stack.records[k].end_pc)
		new_pos = k;
	    }
	}
      
      if (new_pos)
	{
	  /* The new record goes into the middle of the array, which means
	     we need to shift everything beyond that position to make a 
	     blank space for the new record in the correct position.  */

	  for (j = nelts; j >= new_pos; j--)
	    copy_inlined_call_stack_record (j, j + 1);

	  /* Make 'i' point to the position for adding the new data.  */
	  i = new_pos;
	}

      /* Now we've made the array big enough, increment nelts to point to the
	 (currently blank) record for the new entry at the top of the stack.  */
      nelts++;
    }

  /* Whether we are using an existing call stack entry or a new one, 'i' will
     point to the correct entry.  */

  if (!s)
    s = find_pc_symtab (item->pc);
      
  gdb_assert (s != NULL);

  if (item->entry_type == INLINED_SUBROUTINE_LT_ENTRY)
    {
      global_inlined_call_stack.records[i].start_pc = item->pc;
      global_inlined_call_stack.records[i].end_pc = item->end_pc;
      if (global_inlined_call_stack.records[i].s == NULL)
	global_inlined_call_stack.records[i].s = s;
    }
  else if (item->entry_type == INLINED_CALL_SITE_LT_ENTRY)
    {
      global_inlined_call_stack.records[i].start_pc = item->pc;
      global_inlined_call_stack.records[i].end_pc = item->end_pc;

      /* Fill in the call site location data.  */

      global_inlined_call_stack.records[i].call_site_filename = 
	                                                       s->filename;
      global_inlined_call_stack.records[i].call_site_line = item->line;
      global_inlined_call_stack.records[i].call_site_column = 0;
    }

  find_function_names_and_address_ranges (s->objfile,
				      &(global_inlined_call_stack.records[i]));
  
  global_inlined_call_stack.nelts = nelts;

  /* Make sure that each element on the stack should properly contain the
     element(s) above it.  */

  verify_stack ();
}

/* Return the current position in the global_inlined_call_stack.  */

int
current_inlined_subroutine_stack_position (void)
{
  return global_inlined_call_stack.current_pos;
}

/* Return the number of valid records in the global_inlined_call_stack.  */

int
current_inlined_subroutine_stack_size (void)
{
  return global_inlined_call_stack.nelts;
}


/* Add 'I' to the current position in the global_inlined_call_stack.  Note
   that 'I' can be negative.  */

void 
adjust_current_inlined_subroutine_stack_position (int i)
{
  global_inlined_call_stack.current_pos += i;
}

/* This function takes a node (NODE) based on a linetable entry,
   and an ordered linked list of such nodes (LIST), and inserts
   NODE in the appropriate position in the list.  Every linetable
   entry in the list should have the same pc value, and should have
   an inlined type.  The main sort keys are pc and end_pc.  The
   list is sorted largest to smallest.  */

static void
insert_pending_node (struct pending_node *node, struct pending_node **list)
{
  struct pending_node *prev;
  struct pending_node *cur;

  if (*list == NULL)
    *list = node;
  else
    {
      int done = 0;
      prev = NULL;
      cur = *list;
      while (cur && !done)
	{
	  if (node->entry->pc < cur->entry->pc)
	    {
	      if (!prev)
		*list = node;
	      else
		prev->next = node;
	      node->next = cur;
	      done = 1;
	    }
	  else if (node->entry->pc > cur->entry->pc)
	    {
	      prev = cur;
	      cur = cur->next;
	    }
	  else if (node->entry->pc == cur->entry->pc)
	    {
	      if (cur->entry->end_pc > node->entry->end_pc)
		{
		  prev = cur;
		  cur = cur->next;
		}
	      else
		{
		  if (!prev)
		    *list = node;
		  else
		    prev->next = node;
		  node->next = cur;
		  done = 1;
		}
	    }
	} /* end while  */

      if (prev && !done)
	prev->next = node;
    }
}
   
/* This function is called every time the value of stop_pc changes, to
   remove any records from the global_inlined_call_stack data that are
   no longer valid, and to find and add any newly valid records.  */

void
inlined_function_update_call_stack (CORE_ADDR pc)
{
  struct symtab *s;
  struct symtab *orig_s;
  struct symtab *alt_symtab = 0;
  struct linetable *l;
  int len;
  struct linetable_entry *alt = NULL;
  struct linetable_entry *item;
  struct linetable_entry *prev;
  struct blockvector *bv;
  asection *section_tmp;
  struct bfd_section *section;
  int i;
  int done;
  struct linetable_entry *best = NULL;
  CORE_ADDR best_end = 0;
  struct symtab *best_symtab = 0;
  struct pending_node *pending_list;
  struct pending_node *temp;
  struct pending_node *cur_pend;

  if (!dwarf2_allow_inlined_stepping)
    {
      if (global_inlined_call_stack.nelts > 0)
	inlined_function_reinitialize_call_stack ();
      return;
    }

  /* If the user is attempting to step over an inlined subroutine,
     which was contained in the last line the user was at, and the
     last line ALSO happened to be at a control flow jump, we may have
     landed at an instruction which is beyond the end of the stepping
     range, but is within the "original" function.  At this point we
     want to turn off the 'stepping_over_inlined_subroutine' flag so
     that things stop approrpriately.  This situation ought to be
     detectable IF the step_frame_id is the same as the current
     frame's id AND the stop_pc is not within the stepping range (and
     stepping_over_inlined_subroutine is on).  */

  if (stepping_over_inlined_subroutine
      && stop_pc > step_range_end
      && frame_id_eq (get_frame_id (get_current_frame ()), step_frame_id))
    stepping_over_inlined_subroutine = 0;

  /* FIRST, remove anything in stack that no longer belongs there!
     Assumption: Stack is ordered so things further up (near the top)
     are contained somehow by things further down (near the bottom),
     i.e. things may need to be removed from the top, but never from
     the middle.  If anything in the middle needs to go, so does
     everything above it.  NOTE: If the current_pos field points to
     stuff being removed, be sure to reduce it until it is pointing to
     the topmost valid entry.  */


  /* Since the stop_pc has changed, the frame stack has been blown away,
     including any inlined frame records; update the records to show this.  */

  for (i = 1; i <= global_inlined_call_stack.nelts; i++)
    global_inlined_call_stack.records[i].stack_frame_created = 0;

  /* Now, start at the top of our inlined call stack (the innermost
     function) proceed down the stack.  If the current pc falls within
     the start/end pc bounds of the inlined call stack record, stop (we
     have found the top-most valid record). Otherwise re-set the (invalid)
     record to zero and move down the stack to the next record.  */

  done = 0;
  while (!done)
    {
      i = global_inlined_call_stack.nelts;
      if (i == 0)
	done = 1;
      else if ((global_inlined_call_stack.records[i].ranges
		&& !record_ranges_contains_pc (i, pc))
	       || (!global_inlined_call_stack.records[i].ranges
		   && (global_inlined_call_stack.records[i].start_pc > pc
		       || global_inlined_call_stack.records[i].end_pc <= pc)))
	{
	  memset (&global_inlined_call_stack.records[i], 0,
		  sizeof (struct inlined_call_stack_record));
	  global_inlined_call_stack.nelts--;
	}
      else
	done = 1;
    }

  /* Make sure the current position in the call stack is not pointing to
     a record that was removed (re-set to zero).  */

  if (global_inlined_call_stack.current_pos > global_inlined_call_stack.nelts)
    global_inlined_call_stack.current_pos = global_inlined_call_stack.nelts;
	
  /* NOW, go through the line table and see if anything new needs to be
     added to the stack.  The following code was largely lifted from
     find_pc_sect_line, in symtab.c  */

  section_tmp = find_pc_overlay (pc);
  if (pc_in_unmapped_range (pc, section_tmp))
    pc = overlay_mapped_address (pc, section_tmp);
  section = (struct bfd_section *) section_tmp;

  s = find_pc_sect_symtab (pc, section);
  orig_s = s;
  if (s)
    {
      bv = BLOCKVECTOR (s);

      /* Look at all the symtabs that share this blockvector.
         They all have the same apriori range, that we found was right;
         but they have different line tables.  */

      for ( ; s && BLOCKVECTOR (s) == bv; s = s->next)
	{
	  l = LINETABLE (s);
	  if (!l)
	    continue;
	  len = l->nitems;
	  if (len <= 0)
	    continue;

	  pending_list = NULL;
	  prev = NULL;
	  item = l->item;
	  if (item->pc > pc && (!alt || item->pc < alt->pc))
	    {
	      alt = item;
	      alt_symtab = s;
	    }

	  for (i = 0; i < len; i++, item++)
	    {
	      if ((item->entry_type == INLINED_SUBROUTINE_LT_ENTRY
		   || item->entry_type == INLINED_CALL_SITE_LT_ENTRY)
		  && item->pc <= pc
		  && item->end_pc > pc)
		{
		  /* Store the item(s) in a sorted list; after all
		     of the items for a particular pc have been
		     collected and sorted, they get added to the
		     call stack in the correct order.  */

		  temp = (struct pending_node *) xmalloc (sizeof (struct pending_node));
		  temp->entry = item;
		  temp->s = s;
		  temp->next = NULL;
		  insert_pending_node (temp, &pending_list);
		}
	      
	      if (item->pc > pc
		  || (item->pc == pc
		      && prev
		      && item->pc == prev->pc))
		break;
	      
	      prev = item;
	    }	  
	  
	  if (prev && prev->line && (!best || prev->pc > best->pc))
	    {
	      best = prev;
	      best_symtab = s;
	      
	      if (best_end <= best->pc)
		best_end = 0;
	    }
	  
	  if (best_symtab
	      && best->line != 0
	      && prev 
	      && prev->pc == item->pc)
	    {
	      while (prev->pc == item->pc)
		{
		  prev = item;
		  item++;
		  if ((item->entry_type ==  INLINED_SUBROUTINE_LT_ENTRY
		       || item->entry_type == INLINED_CALL_SITE_LT_ENTRY)
		      && item->pc <= pc
		      && item->end_pc > pc)
		    {
		      /* Store the item(s) in a sorted list; after all
			 of the items for a particular pc have been
			 collected and sorted, they get added to the
			 call stack in the correct order.  */

		      temp = (struct pending_node *) xmalloc (sizeof (struct pending_node));
		      temp->entry = item;
		      temp->s = s;
		      temp->next = NULL;
		      insert_pending_node (temp, &pending_list);
		    }
		}
	      best = prev;
	    }

	  for (cur_pend = pending_list; cur_pend; cur_pend = cur_pend->next)
	    add_item_to_inlined_subroutine_stack (cur_pend->entry, cur_pend->s,
						  section);
	}
    }

  /* If there's anything in the inlined call stack, then the stop_pc
     is in the middle of some inlined code, so at the very least
     the stack pointer should be pointing at the first real entry
     in the stack (the zeroth entry is not "real").  */

  if (current_inlined_subroutine_stack_position () == 0
      && current_inlined_subroutine_stack_size () > 0)
    {
      int i = find_correct_current_position ();
      adjust_current_inlined_subroutine_stack_position (i);
      /* APPLE LOCAL begin remember stepping into inlined subroutine
	 across intervening function calls.  */
      if (stepping_into_inlined_subroutine
	  || (inlined_step_range_end == 
	      global_inlined_call_stack.records[i].start_pc))
      /* APPLE LOCAL end remember stepping into inlined subroutine
	 across intervening function calls.  */
	{
	  step_into_current_inlined_subroutine ();
	  stepping_into_inlined_subroutine = 0;
	  /* APPLE LOCAL radar 6534195  */
	  inlined_step_range_end = (CORE_ADDR) 0;
	}
      else if (step_range_start && step_range_end
	       && step_range_start != step_range_end
	       && global_inlined_call_stack.last_inlined_pc
	       && ((global_inlined_call_stack.last_inlined_pc <
		        global_inlined_call_stack.records[i].start_pc)
		   || (global_inlined_call_stack.last_inlined_pc >=
		       global_inlined_call_stack.records[i].end_pc)))
	{
	  /* User was single-stepping, and somehow stepped over the
	     'call site' and into the middle of the inlined subroutine;
	     try to make the user stop at the call site anyway.  */
	  global_inlined_call_stack.records[i].stepped_into = 0;
	}
    }

  if (global_inlined_call_stack.nelts > 0)
    global_inlined_call_stack.last_inlined_pc = pc;
  inlined_function_update_call_stack_pc (pc);
}

/* This function is call from check_inlined_function_calls in
   dwarf2read.c, when it is processing the dies for inlined
   subroutines.  This function creates a unique record for each
   inlined function call, containing the names of the caller and
   callee, the line and column position of the call site, and the
   starting and ending PC values for the inlined code (and a list of
   address ranges, if the inlined function has multiple non-contiguous
   ranges of addresses.  GDB does not have any other place (at the
   moment) where the name of the inlined subroutine can be stored.
   This is also the easiest way to get the noncontiguous address
   ranges without mucking up the line table.

   The records are stored in a balanced binary tree (a red-black tree),
   because there may be many records which need to be processed quickly.
   The tree may contain duplicate address ranges, if two inlined functions
   overlap (it has happened), but in those cases the functions should have
   different call site lines.

   The main implementation of the red-black trees is inside dwarf2read.c.
   They are set up to have up to 3 sort keys.  In this case, the records
   in the tree are sorted first by low_pc, then by high_pc.  Therefore 
   looking up & storing by address are quick.  To look up by function name,
   every node in the tree must be checked.  */


void
inlined_function_add_function_names (struct objfile *objfile,
				     CORE_ADDR low_pc, CORE_ADDR high_pc,
				     int line, int column, const char *fn_name, 
				     const char *calling_fn_name,
				     struct address_range_list *ranges,
				     /* APPLE LOCAL radar 6545149  */
				     struct symbol *func_sym)
{
  struct rb_tree_node *tmp_rb_node;
  struct inlined_call_stack_record *tmp_record;
  struct rb_tree_node_list *matches = NULL;
  struct rb_tree_node_list *current;
  struct rb_tree_node_list *next;
  int found = 0;

  if (!fn_name)
    fn_name = "<unknown function>";
  if (!calling_fn_name)
    calling_fn_name = "<unknown function>";

  if (strcmp (fn_name, "<unknown function>") == 0
      || strcmp (calling_fn_name, "<unknown function>") == 0)
    {
      complaint (&symfile_complaints, 
                 "Missing inlined function names: "
                 "%s calling %s, at line %d (address 0x%s)",
                 calling_fn_name, fn_name, line, paddr_nz (low_pc));
    }

  rb_tree_find_all_matching_nodes (objfile->inlined_subroutine_data,
				   low_pc, 0, high_pc, &matches);

  for (current = matches; current && !found; current = current->next)
    {
      tmp_rb_node = current->node;

      if (tmp_rb_node && !found)
	{
	  tmp_record = (struct inlined_call_stack_record *) tmp_rb_node->data;
	  if (tmp_record->call_site_line == line)
	    {
	      found = 1;
	      if (! tmp_record->fn_name)
		tmp_record->fn_name = xstrdup (fn_name);
	      if (! tmp_record->calling_fn_name)
		tmp_record->fn_name = xstrdup (calling_fn_name);
	    }
	}
    }

  if (!found)
    {
      tmp_record = (struct inlined_call_stack_record *) xmalloc
	                           (sizeof (struct inlined_call_stack_record));
      tmp_record->start_pc = low_pc;
      tmp_record->end_pc = high_pc;

      if (ranges != NULL)
	{
	  int i;
	  tmp_record->ranges 
	    = (struct address_range_list *) xmalloc (sizeof (struct address_range_list));
	  tmp_record->ranges->ranges 
	    = (struct address_range *) xmalloc (ranges->nelts * sizeof (struct address_range));
	  
	  tmp_record->ranges->nelts = ranges->nelts;
	  for (i = 0; i < ranges->nelts; i++)
	    tmp_record->ranges->ranges[i] = ranges->ranges[i];
	}
      else
	tmp_record->ranges = NULL;
      
      tmp_record->fn_name = xstrdup (fn_name);
      tmp_record->calling_fn_name = xstrdup (calling_fn_name);
      tmp_record->call_site_filename = NULL;
      tmp_record->call_site_line = line;
      tmp_record->call_site_column = column;

      /* APPLE LOCAL radar 6545149  */
      tmp_record->func_sym = func_sym;

      tmp_rb_node = (struct rb_tree_node *) xmalloc 
	                                        (sizeof (struct rb_tree_node));

      tmp_rb_node->key = low_pc;
      tmp_rb_node->secondary_key = 0;
      tmp_rb_node->third_key = high_pc;
      tmp_rb_node->data = (void *) tmp_record;
      tmp_rb_node->left = NULL;
      tmp_rb_node->right = NULL;
      tmp_rb_node->parent = NULL;
      tmp_rb_node->color = UNINIT;

      rb_tree_insert (&(objfile->inlined_subroutine_data), 
		      objfile->inlined_subroutine_data, tmp_rb_node);
    }

  /* Clean up the 'matches' list.  */

  current = matches;
  while (current)
    {
      next = current->next;
      xfree (current);
      current = next;
    }
}


/* Return the symbol table for the current inlined subroutine.  */

struct symtab *
current_inlined_subroutine_stack_symtab (void)
{
  int i;
  struct inlined_function_data *stack_ptr;

  if (global_inlined_call_stack.nelts > 0)
    stack_ptr = &(global_inlined_call_stack);
  else
    stack_ptr = &(temp_frame_stack);

  i = stack_ptr->current_pos;

  return stack_ptr->records[i].s;
}

/* Return the starting PC value for the current inlined subroutine.  */

CORE_ADDR
current_inlined_subroutine_call_stack_start_pc (void)
{
  int i;

  struct inlined_function_data *stack_ptr;

  if (global_inlined_call_stack.nelts > 0)
    stack_ptr = &(global_inlined_call_stack);
  else
    stack_ptr = &(temp_frame_stack);

  i = stack_ptr->current_pos;

  return stack_ptr->records[i].start_pc;
}

/* Return the address of the code corresponding to the end of the
   last source line in the function that was in-lined.  (Logically,
   the end of the inlined function).  */

CORE_ADDR
current_inlined_subroutine_call_stack_eof_pc (void)
{
  int i;
  struct inlined_function_data *stack_ptr;

  if (global_inlined_call_stack.nelts > 0)
    stack_ptr = &(global_inlined_call_stack);
  else
    stack_ptr = &(temp_frame_stack);

  i = stack_ptr->current_pos;

  return record_end_pc (stack_ptr->records[i]);
}

/* Return the end address of the first (possibly only) address range
   of the inlined function.  If the inlined function has only a single
   contiguous range of addresses, this will be the same as the value
   returned by current_inlined_subroutine_call_stack_eof_pc; if the
   function has multiple non-contiguous ranges of addresses then it
   will NOT be the address of the end of the function.  */

CORE_ADDR
current_inlined_subroutine_call_stack_end_pc (void)
{
  int i;
  struct inlined_function_data *stack_ptr;

  if (global_inlined_call_stack.nelts > 0)
    stack_ptr = &(global_inlined_call_stack);
  else
    stack_ptr = &(temp_frame_stack);

  i = stack_ptr->current_pos;

  return stack_ptr->records[i].end_pc;
}


/* Return the name of the current inlined subroutine.  */

char *
current_inlined_subroutine_function_name (void)
{
  int i;
  struct inlined_function_data *stack_ptr;

  if (global_inlined_call_stack.nelts > 0)
    stack_ptr = &(global_inlined_call_stack);
  else
    stack_ptr = &(temp_frame_stack);

  i = stack_ptr->current_pos;

  return stack_ptr->records[i].fn_name;
}


/* Return the name of the caller function for the current inlined
   subroutine.  */

char *
current_inlined_subroutine_calling_function_name (void)
{
  int i;
  struct inlined_function_data *stack_ptr;

  if (global_inlined_call_stack.nelts > 0)
    stack_ptr = &(global_inlined_call_stack);
  else
    stack_ptr = &(temp_frame_stack);

  i = stack_ptr->current_pos;

  return stack_ptr->records[i].calling_fn_name;
}

/* Return the source line number for the call site of the 'current'
   inlined subroutine.  */

int
current_inlined_subroutine_call_site_line (void)
{
  int i;
  struct inlined_function_data *stack_ptr;

  if (global_inlined_call_stack.nelts > 0)
    stack_ptr = &(global_inlined_call_stack);
  else
    stack_ptr = &(temp_frame_stack);

  i = stack_ptr->current_pos;

  return stack_ptr->records[i].call_site_line;
}

/* This function checks to see if the stop_pc is at the call site of
   an inlined_subroutine that has not yet been stepped into.  If so,
   it updates FILE_NAME, LINE_NUM, and COLUMN to indicate the call site
   position.  */

int 
at_inlined_call_site_p (char **file_name, int *line_num, int *column)
{
  int ret_val = 0;
  int i;
  int low = 0;
  int high = 0;

  for (i = 1; i <= global_inlined_call_stack.nelts; i++)
    if (global_inlined_call_stack.records[i].start_pc == stop_pc
	&& !global_inlined_call_stack.records[i].stepped_into)
      {
	if (!low)
	  low = i;
	high = i;
      }

  /* We now have a range, low to high, of entries on the stack for
     which we are at the starting address.  Now we need to figure out
     where in this range we should be.  */

  if (low > 0)
    {
      i = 0;

      if (low == high
	  && !global_inlined_call_stack.records[low].stepped_into)
	i = low;
      else if (low <= current_inlined_subroutine_stack_position ()
	       && current_inlined_subroutine_stack_position() <= high)
	i = current_inlined_subroutine_stack_position ();
      else
	i = low;

      if (i > 0)
	{
	  *file_name = global_inlined_call_stack.records[i].call_site_filename;
	  *line_num  = global_inlined_call_stack.records[i].call_site_line;
	  *column    = global_inlined_call_stack.records[i].call_site_column;
	  ret_val = i;
	}
    }

  return ret_val;
}

/* This function returns a boolean indicating if the stop_pc falls between
   the start and end pc of an inlined subroutine.  It also updates
   INLINE_END_PC to contain the ending pc of the 'current' record in the
   global_inlined_call_stack.  */

int 
in_inlined_function_call_p (CORE_ADDR *inline_end_pc)
{
  int ret_val = 0;
  int i;
  int low = 0;
  int high = 0;

  *inline_end_pc = (CORE_ADDR) 0;

  for (i = 1; i <= global_inlined_call_stack.nelts; i++)
    if (global_inlined_call_stack.records[i].stepped_into)
      if ((global_inlined_call_stack.records[i].ranges
	   && record_ranges_contains_pc (i, stop_pc))
	  || (!global_inlined_call_stack.records[i].ranges
	      && (global_inlined_call_stack.records[i].start_pc <= stop_pc
		  && stop_pc < global_inlined_call_stack.records[i].end_pc)))
        {
	  if (!low)
	    low = i;
	  high = i;
        }
  
  if (low > 0)
    {
      if (low <= current_inlined_subroutine_stack_position ()
	  && current_inlined_subroutine_stack_position() <= high)
	i = current_inlined_subroutine_stack_position ();
      else
	i = high;

      if (!global_inlined_call_stack.records[i].ranges)
	*inline_end_pc = global_inlined_call_stack.records[i].end_pc;
      else
	*inline_end_pc = address_range_ending_pc 
	                         (global_inlined_call_stack.records[i].ranges);

      ret_val = i;
    }

  return ret_val;
}

/* Compares PC against end_pcs in records in global_inlined_call_stack, to
   see if it marks the end of an inlined subroutine.  This is used for
   updating the current position in the global_inlined_call_stack, if
   appropriate.  */

int 
inlined_function_end_of_inlined_code_p (CORE_ADDR pc)
{
  int i;
  int ret_val = 0;

  for (i = 1; i <= current_inlined_subroutine_stack_size (); i++)
    if (record_end_pc (global_inlined_call_stack.records[i]) == pc
	&& global_inlined_call_stack.records[i].stepped_into)
      {
	ret_val = 1;
	if (i != current_inlined_subroutine_stack_position())
	  internal_error (__FILE__, __LINE__, 
			  _("Inlined stack position is inconsistent."));
	break;
      }

  return ret_val;
}

/* Necessary data structure, the frame unwinder, for the new frame
   type, INLINED_FRAME.  */

static const struct frame_unwind inlined_frame_unwinder =
{
  INLINED_FRAME,
  inlined_frame_this_id,
  inlined_frame_prev_register,
  NULL,
  inlined_frame_sniffer,
};

/* Necessary data structure for creating new frame type, INLINED_FRAME  */

const struct frame_unwind *const inlined_frame_unwind = {
  &inlined_frame_unwinder
};

/* Detects inlined frames in the middle of the call stack.  */

static int
inlined_frame_sniffer_helper (struct frame_info *next_frame, CORE_ADDR pc)
{
  CORE_ADDR inline_end_pc;
  int inside_inlined_code;
  int cur_pos;
  int i;

  update_tmp_frame_stack (pc);

  inside_inlined_code = tmp_frame_in_inlined_function_call_p (pc,
							      &inline_end_pc);
  if (!inside_inlined_code)
    return 0;
  
  cur_pos = current_tmp_frame_stack_position ();

  if (cur_pos == 0)
    return 0;

  for (i = cur_pos; i > 0; i--)
    if (!temp_frame_stack.records[i].stack_frame_created)
      {
	temp_frame_stack.records[i].stack_frame_created = 1;
	return 1;
      }

  return 0;
}

/* Check to see if the current frame ought to be for an inlined subroutine
   (in which case it should be an INLINED_FRAME) or not.  It should be for
   an inlined subroutine if the following three conditions hold:  1). the 
   stop_pc is between the start and end pcs of an inlined subroutine; 2).
   the user has chosen to step at least once into an inlined subroutine;
   and 3). an INLINED_FRAME has not already been created for all the current
   records in the global_inlined_call_stack.  */

int
inlined_frame_sniffer (const struct frame_unwind *self,
		       struct frame_info *next_frame,
		       void **this_prologue_cache)
{
  CORE_ADDR inline_end_pc;
  CORE_ADDR pc = 0;
  int inside_inlined_code;
  int cur_pos;
  int i;

  /* Get the PC address from next_frame, to see if the PC falls inside an
     occurrence of inlining.  */

  if (!gdbarch_unwind_pc_p (current_gdbarch))
    return 0;
  
  pc = gdbarch_unwind_pc (current_gdbarch, next_frame);

  /* If the user is partway through a step/next command but has not
     reached the step_range_end, then we do NOT want to return an
     inlined frame (which will mess up the step_frame_id comparisons
     and make gdb stop at the wrong place).  */

  if (step_range_start && step_range_end
      && step_range_start < pc
      && pc <= step_range_end
      && pc != stop_pc)
    return 0;

  /* If we're in the middle of the call stack, call our helper function
     to check for inlining ocurrences at positions other than the stop_pc.  */

  if (global_inlined_call_stack.nelts == 0
      || global_inlined_call_stack.last_pc != pc)
    return inlined_frame_sniffer_helper (next_frame, pc);

  /* We're not dealing with inlining somewhere in the middle of the call
     stack; check to see if the stop_pc is in the middle of some 
     inlining.  */

  inside_inlined_code = in_inlined_function_call_p (&inline_end_pc);
  if (!inside_inlined_code)
    return 0;

  cur_pos = current_inlined_subroutine_stack_position ();
  if (!global_inlined_call_stack.records[cur_pos].stepped_into)
    cur_pos--;

  if (cur_pos == 0)
    return 0;

  for (i = 1; i <= cur_pos; i++)
    {
      if (!global_inlined_call_stack.records[i].stack_frame_created)
	{
	  global_inlined_call_stack.records[i].stack_frame_created = 1;
	  return 1;
	}
    }

  return 0;
}


/* Create the unique frame id for this frame (an INLINED_FRAME), given
   the next frame.  This function is called indirectly by
   get_frame_id, through the inlined_frame_unwinder (see above).  */

void
inlined_frame_this_id (struct frame_info *next_frame,
		       void **this_prologue_cache,
		       struct frame_id *this_id)
{
  struct frame_id tmp_id;
  int i;
  int this_level = frame_relative_level (next_frame) + 1;
  int cur_pos = 0;

  if (global_inlined_call_stack.nelts > 0)
    cur_pos = global_inlined_call_stack.current_pos;
  else
    cur_pos = temp_frame_stack.current_pos;

  gdb_assert (cur_pos > 0);

  /* First call gdbarch_unwind_dummy_id to get a valid stack address for
     the frame. */

  tmp_id = gdbarch_unwind_dummy_id (get_frame_arch (next_frame),
				    next_frame);


  /* Next, select the correct corresponding record in global_inlined_call_stack,
     for filling in the data.  */


  if (global_inlined_call_stack.nelts > 0)
    {
      if (cur_pos > 1
	  && !global_inlined_call_stack.records[cur_pos].stepped_into)
	cur_pos--;
      i = cur_pos - this_level;
      if (i < 1)
	i = 1;
    }
  else
    {
      for (cur_pos = 1; 
	   cur_pos <= temp_frame_stack.nelts 
	     && !temp_frame_stack.records[cur_pos].stack_frame_created;
	   cur_pos++);
      i = cur_pos;
    }

  
  /* The code_addr field gets the start_pc for the inlined function.  The
     special_addr gets the end_pc for the inlined_function.  We use both,
     because it is possible to have multiple levels of inlining, which 
     results in multiple records that have the same start_pc.  */

  if (global_inlined_call_stack.nelts > 0)
    {
      tmp_id.code_addr = global_inlined_call_stack.records[i].start_pc;
      tmp_id.special_addr = record_end_pc (global_inlined_call_stack.records[i]);
    }
  else
    {
      tmp_id.code_addr = temp_frame_stack.records[i].start_pc;
      tmp_id.special_addr = record_end_pc (temp_frame_stack.records[i]);
    }

  tmp_id.code_addr_p = 1;
  tmp_id.special_addr_p = 1;

  (*this_id) = tmp_id;
}

/* This function is called from print_frame, to deal with an INLINED_FRAME
   type.   Much of the code is copied from print_frame, but a few things
   are modified to deal correctly with the inlined subroutine data.  */

void
print_inlined_frame (struct frame_info *fi, int print_level, 
		     enum print_what print_what,
		     int print_args,
		     struct symtab_and_line sal, int call_site_line)
{
  struct ui_stream *stb;
  struct cleanup *old_chain;
  struct cleanup *list_chain;
  char *funname;
  char *buffer;
  char *tmp_name;
  enum language funlang = language_unknown;
  int i;
  int buffer_len;
  int line;
  struct inlined_function_data *stack_ptr;

  if (get_frame_pc (fi) != stop_pc)
    update_tmp_frame_stack (get_frame_pc (fi));

  /* APPLE LOCAL begin radar 6131694  */
  if (global_inlined_call_stack.nelts > 0
      && global_inlined_call_stack.records[1].stepped_into
      && frame_relative_level (fi) <= global_inlined_call_stack.nelts)
  /* APPLE LOCAL end radar 6131694  */
    stack_ptr = &global_inlined_call_stack;
  else
    stack_ptr = &temp_frame_stack;

  stb = ui_out_stream_new (uiout);
  old_chain = make_cleanup_ui_out_stream_delete (stb);

  /* Print the "level" of the frame.  */

  annotate_frame_begin (print_level ? frame_relative_level (fi) : 0,
			get_frame_pc (fi));

  list_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "frame");

  if (print_level)
    {
      ui_out_text (uiout, "#");
      ui_out_field_fmt_int (uiout, 2, ui_left, "level",
			    frame_relative_level (fi));
    }
     
  if (addressprint)
    if (get_frame_pc (fi) != sal.pc
	|| !sal.symtab
	|| print_what == LOC_AND_ADDRESS)
      {
	annotate_frame_address ();
	ui_out_field_core_addr (uiout, "addr", get_frame_pc (fi));
	annotate_frame_address_end ();
	ui_out_text (uiout, " in ");
	if (ui_out_is_mi_like_p (uiout))
	  {
	    ui_out_field_core_addr (uiout, "fp", get_frame_base (fi));
	  }
      }

  annotate_frame_function_name ();

  /* Get appropriate inlined call stack record.  */
  if (call_site_line)
    {
      for (i = 1; i < stack_ptr->current_pos; i++)
	if (stack_ptr->records[i].call_site_line == call_site_line)
	  break;
    }
  else
    {
      /* Look for the highest record on the stack that has been
	 stepped into but has not been printed.  */
      
      i = stack_ptr->current_pos;
      if (!stack_ptr->records[i].stepped_into
	  && i > 1)
	i--;
      while (i > 1
	     && stack_ptr->records[i].stack_frame_printed)
	i--;
      
    }
  
  /* Having found the record for our current position, we now need to
     work back down the stack and find the first record we haven't printed
     already.  */
  
  while (i > 1 
	 && (stack_ptr->records[i].stack_frame_printed
	     || !stack_ptr->records[i].stepped_into))
    i--;
  
  if (stack_ptr->records[i].stepped_into)
    tmp_name = stack_ptr->records[i].fn_name;
  else
    tmp_name = stack_ptr->records[i].calling_fn_name;
  
  /* Modify the function name: append " [inlined]" to it to make the
     status of the function perfectly clear to the user.  */

  if (tmp_name)
    {
      buffer_len = strlen (tmp_name) + strlen (" [inlined]") + 1;
      buffer = (char *) xmalloc (buffer_len);
      sprintf (buffer, "%s [inlined]", tmp_name);
    }
  else
    {
      buffer_len = strlen ("<unknown function> [inlined]") + 1;
      buffer = (char *) xmalloc (buffer_len);
      sprintf (buffer, "<unknown function> [inlined]");
    }
  
  funname = buffer;

  if (stack_ptr->records[i].s)
    funlang = stack_ptr->records[i].s->language;
  else
    funlang = sal.symtab->language;

  annotate_frame_function_name ();
  fprintf_symbol_filtered (stb->stream, funname, funlang, DMGL_ANSI);
      
  ui_out_field_stream (uiout, "func", stb);
  ui_out_wrap_hint (uiout, "   ");
  annotate_frame_args ();
  ui_out_text (uiout, " (");

  /* Don't print args; it makes no sense for inlined functions.  We may
     change this later.  */

  ui_out_text (uiout, ")");

  annotate_frame_source_begin ();
  ui_out_wrap_hint (uiout, "   ");
  ui_out_text (uiout, " at " );
  annotate_frame_source_file ();
  if (!stack_ptr->records[i].stepped_into)
    ui_out_field_string (uiout, "file",
			 stack_ptr->records[i].call_site_filename);
  else
    {
      if (stack_ptr->records[i].s)
	ui_out_field_string (uiout, "file", 
			     symtab_to_fullname (stack_ptr->records[i].s));
      else if (sal.symtab)
	ui_out_field_string (uiout, "file", sal.symtab->filename);
      else 
	ui_out_field_string (uiout, "file", "<unknown>");
    }

  if (ui_out_is_mi_like_p (uiout))
    {
      const char *fullname = symtab_to_fullname 
	(stack_ptr->records[i].s);
      if (fullname != NULL)
	ui_out_field_string (uiout, "fullname", fullname);
    }
  annotate_frame_source_file_end ();
  ui_out_text (uiout, ":");
  annotate_frame_source_line ();

  if (!stack_ptr->records[i].stepped_into)
    line = stack_ptr->records[i].call_site_line;
  else
    {
      if (i < stack_ptr->current_pos)
	line = stack_ptr->records[i+1].call_site_line;
      else
	line = sal.line;
    }

  ui_out_field_int (uiout, "line", line);
  annotate_frame_source_end ();
  
  if (print_frame_more_info_hook)
    print_frame_more_info_hook (uiout, &sal, fi);

  stack_ptr->records[i].stack_frame_printed = 1;
  /* APPLE LOCAL radar 6131694  */
  stack_ptr->records[i].stack_frame_created = 1;

  do_cleanups (list_chain);
  ui_out_text (uiout, "\n");
  do_cleanups (old_chain);
}

/* This is called from flush_cached_frames.  Since all the INLINED_FRAMEs
   were removed by that function, we need to mark all the records in the 
   global_inlined_call_stack as not having frames created for them.  */

void
flush_inlined_subroutine_frames (void)
{
  int cur_pos = current_inlined_subroutine_stack_position ();
  int i;

  for (i = 1; i <= cur_pos; i++)
    {
      global_inlined_call_stack.records[i].stack_frame_created = 0;
      global_inlined_call_stack.records[i].stack_frame_printed = 0;
    }

  for (i = 1; i < temp_frame_stack.nelts; i++)
    {
      temp_frame_stack.records[i].stack_frame_created = 0;
      temp_frame_stack.records[i].stack_frame_printed = 0;
    }

  /* APPLE LOCAL begin radar 6131694  */
  if (i > 0
      && i == temp_frame_stack.nelts)
    {
      temp_frame_stack.records[i].stack_frame_created = 0;
      temp_frame_stack.records[i].stack_frame_printed = 0;
    }
  /* APPLE LOCAL end radar 6131694  */
}

/* We've finished printing all the INLINED_FRAMEs for all the records in
   the global_inlined_call_stack (presumably), so clear the print flags
   for the next time we want to print the frames.  */

void
clear_inlined_subroutine_print_frames (void)
{
  int cur_pos = current_inlined_subroutine_stack_position ();
  int i;

  for (i = 1; i <= cur_pos; i++)
    global_inlined_call_stack.records[i].stack_frame_printed = 0;
}


/* Update the stepped_into field and the current position in the
   global_inlined_call_stack appropriately, when the user steps into
   an inlined subroutine.  */

void
step_into_current_inlined_subroutine (void)
{
  int cur_pos = current_inlined_subroutine_stack_position ();

  while (cur_pos <= global_inlined_call_stack.nelts
	 && global_inlined_call_stack.records[cur_pos].start_pc < stop_pc)
    {
      global_inlined_call_stack.records[cur_pos].stepped_into = 1;
      if (cur_pos < global_inlined_call_stack.nelts)
	adjust_current_inlined_subroutine_stack_position (1);
      cur_pos++;
    }
  
  if (cur_pos <= global_inlined_call_stack.nelts
      && global_inlined_call_stack.records[cur_pos].start_pc == stop_pc
      && !global_inlined_call_stack.records[cur_pos].stepped_into)
    {
      global_inlined_call_stack.records[cur_pos].stepped_into = 1;
      if (cur_pos < global_inlined_call_stack.nelts)
	adjust_current_inlined_subroutine_stack_position (1);
    }
}

/* Return the source position of the outermost call site (the bottom of
   the global_inlined_call_stack).  This is used when printing frame info,
   to print the "current source line" for the first NORMAL_FRAME (i.e. the
   place where everything was inlined).  If we didn't do this, print_frame
   would report the source line to be the one in the inlined subroutine 
   (not in the caller), which would be confusing for the user.  */

int
current_inlined_bottom_call_site_line (void)
{
  if (global_inlined_call_stack.nelts > 0)
    return global_inlined_call_stack.records[1].call_site_line;
  else
    return temp_frame_stack.records[1].call_site_line;
}

struct symtabs_and_lines
check_for_additional_inlined_breakpoint_locations (struct symtabs_and_lines sals,
						   char **addr_string,
						   struct expression **cond,
						   char **cond_string,
						   char ***new_addr_string,
						 struct expression ***new_cond,
						   char ***new_cond_string)
{
  int i;
  int j;
  int *indices;
  int max_size = sals.nelts * 2;
  struct objfile *obj;
  struct symtabs_and_lines new_sals;
  struct rb_tree_node *function_name_records;
  
  new_sals.sals = (struct symtab_and_line *) 
                                     xmalloc (max_size *
					      sizeof (struct symtab_and_line));
  
  new_sals.nelts = 0;
  indices = (int *) xmalloc (sals.nelts * sizeof (int));
  for (i = 0; i < sals.nelts; i++)
    indices[i] = 0;
  
  ALL_OBJFILES (obj)
    {
      function_name_records = obj->inlined_subroutine_data;

      if (!function_name_records)
	continue;
  
      /* Build new_sals for new breakpoints (if any)  */

      for (i = 0; i < sals.nelts; i++)
	{
	  if (addr_string[i])
	    {
	      int already_found = 0;
	      for (j = i - 1; j > 0 && !already_found; j--)
		if (addr_string[j]
		    && strcmp (addr_string[i], addr_string[j]) == 0)
		  already_found = 1;

	      if (!already_found)
		{
		  struct record_list *found_records = NULL;
		  struct record_list *cur;

		  search_tree_for_name (function_name_records, 
					addr_string[i], &found_records);
	      
		  for (cur = found_records; cur; cur = cur->next)
		    {
		      /* We've found an inlined version of the function named
			 in addr_string[i]; now we need to create a breakpoint
			 for the inlined instance.  */
		      
		      indices[i] = 1;
		      if (new_sals.nelts >= max_size)
			{
			  max_size = max_size * 2;
			  new_sals.sals = xrealloc (new_sals.sals,
					      max_size * 
					      sizeof (struct symtab_and_line));
			}
		      new_sals.sals[new_sals.nelts] = find_pc_line 
		                                       (cur->record->start_pc, 
							0);
		  
		      /*  Keep this:  It may need to be reinstated.
			  gdb_assert 
			  (new_sals.sals[new_sals.nelts].pc == 
                                                        cur->record->start_pc);
		      */
		      
		      new_sals.sals[new_sals.nelts].end = cur->record->end_pc;
		      new_sals.sals[new_sals.nelts].entry_type = 
			INLINED_SUBROUTINE_LT_ENTRY;
		      new_sals.sals[new_sals.nelts].next = NULL;
		      
		      new_sals.nelts++;
		      
		    }
		}
	    }
	}
    }
  
  if (new_sals.nelts > 0)
    {
      *new_addr_string = (char **) xmalloc (new_sals.nelts * sizeof (char *));
      *new_cond = (struct expression **) xmalloc (new_sals.nelts * 
						 sizeof (struct expression *));
      *new_cond_string = (char **) xmalloc (new_sals.nelts * sizeof (char *));
      i = 0;
      j = 0;
      while (j < new_sals.nelts)
	{
	  while (i < sals.nelts && indices[i] == 0)
	    i++;
	  if (i < sals.nelts)
	    {
	      (*new_addr_string)[j] = xstrdup (addr_string[i]);
	      if (cond_string[i])
		(*new_cond_string)[j] = xstrdup (cond_string[i]);
	      else
		(*new_cond_string)[j] = NULL;
	      if (cond[i])
		(*new_cond)[j]  = cond[i];
	      else
		(*new_cond)[j] = NULL;
	    }
	  else
	    {
	      (*new_addr_string)[j] = NULL;
	      (*new_cond_string)[j] = NULL;
	      (*new_cond)[j] = NULL;
	    }
	  j++;
	}
    }

  return new_sals;
}

void
inlined_subroutine_adjust_position_for_breakpoint (struct breakpoint *b)
{
  int i;
  int j;
  int cur_pos = 0;

  gdb_assert (b->addr_string != NULL);

  for (i = 1; i <= global_inlined_call_stack.nelts; i++)
    {
      /* APPLE LOCAL begin radar 6366048 search both minsyms & syms for bps  */
      char *long_name = NULL;
      int len = 0;

      len += strlen (global_inlined_call_stack.records[i].fn_name) + 4;

      if (global_inlined_call_stack.records[i].s
	  && global_inlined_call_stack.records[i].s->filename)
	{
	  len += strlen (global_inlined_call_stack.records[i].s->filename);
          long_name = (char *) xmalloc (len);
	  sprintf (long_name, "%s:'%s'", 
		   global_inlined_call_stack.records[i].s->filename,
		   global_inlined_call_stack.records[i].fn_name);
	}

      if (((strcmp (global_inlined_call_stack.records[i].fn_name, 
		    b->addr_string) == 0)
	   || (long_name != NULL
	       && (strcmp (long_name, b->addr_string) == 0)))
      /* APPLE LOCAL end radar 6366048 search both minsyms & syms for bps  */
	  && (global_inlined_call_stack.records[i].start_pc == b->loc->address))
	{
	  global_inlined_call_stack.current_pos = i;
	  cur_pos = i;
	  global_inlined_call_stack.records[i].stepped_into = 1;
	  for (j = i; j > 0; j--)
	    global_inlined_call_stack.records[j].stepped_into = 1;
	  break;
	}
      /* APPLE LOCAL begin radar 6366048 search both minsyms & syms for bps  */
      if (long_name != NULL)
	xfree (long_name);
      /* APPLE LOCAL end radar 6366048 search both minsyms & syms for bps  */
    }

  if (i > global_inlined_call_stack.nelts)
    {
      char *cptr;
      char *filename = NULL;
      int line_found;

      /* Couldn't figure out the breakpoint record based on function name;
	 let's try pc & filename.  */
      i = global_inlined_call_stack.nelts;
      while (global_inlined_call_stack.records[i].start_pc != b->loc->address
	     && i > 0)
	{
	  global_inlined_call_stack.records[i].stepped_into = 0;
	  i--;
	}
	
      /* Does the breakpoint possibly consist of filename:line?  */

      line_found = 0;
      cptr = strrchr (b->addr_string, ':');
      if (cptr)
	{
	  cptr++;
	  if (isdigit (*cptr))
	    {
	      while (isdigit (*cptr))
		cptr++;
	      if (*cptr == '\0')
		line_found = 1;
	    }
	}
      if (line_found)
	{
	  filename = xstrdup (b->addr_string);
	  cptr = strrchr (filename, ':');
	  *cptr = '\0';
	}


      while (i > 0
	     && ((!global_inlined_call_stack.records[i].s)
		 || ((strstr (global_inlined_call_stack.records[i].s->filename, 
			      filename) ==  0)
		     && (strstr (filename, 
			  global_inlined_call_stack.records[i].s->filename) == 0))))
	{
	  global_inlined_call_stack.records[i].stepped_into = 0;
	  i --;
	}

      if (i == 0)
	i = 1;

      global_inlined_call_stack.current_pos = i;
      global_inlined_call_stack.records[i].stepped_into = 1;
      for (j = i; j > 0; j--)
	global_inlined_call_stack.records[j].stepped_into = 1;

      if (filename != NULL)
	xfree (filename);
    }

  gdb_assert ( 1 <= i
	       && i <= global_inlined_call_stack.nelts);

  if (cur_pos < global_inlined_call_stack.nelts
      && (global_inlined_call_stack.records[cur_pos].start_pc ==
	  global_inlined_call_stack.records[cur_pos + 1].start_pc))
    global_inlined_call_stack.current_pos++;
}

void
inlined_subroutine_restore_after_dummy_call (void)
{
  int stack_size;
  int i;

  if (saved_call_stack.nelts > 0)
    {
      /* Blank out the invalid records before filling in the correct
	 (restored) values.  */
  
      stack_size = global_inlined_call_stack.max_array_size * 
	                              sizeof (struct inlined_call_stack_record);
      memset (global_inlined_call_stack.records, 0, stack_size);

      /* Copy back everything except max_array_size, which we don't
	 want to change, because it should still accurately reflect
	 the actual amount of malloc'd space.  NOTE: Since the number
	 of malloc'd records can only grow, never shrink, and since
	 the max_array_size in saved_call_stack was originally copied
	 from global_inlined_call_stack, we should not have to worry
	 about the saved_call_stack containing more records than
	 global_inlined_call stack can hold... */
      
      gdb_assert (global_inlined_call_stack.max_array_size == 
		                               saved_call_stack.max_array_size);

      global_inlined_call_stack.last_pc = saved_call_stack.last_pc;
      global_inlined_call_stack.last_inlined_pc = saved_call_stack.last_inlined_pc;
      global_inlined_call_stack.nelts = saved_call_stack.nelts;
      global_inlined_call_stack.current_pos = saved_call_stack.current_pos;
      
      for (i = 1; i <= saved_call_stack.nelts; i++)
	{
	  global_inlined_call_stack.records[i].start_pc = 
	                            saved_call_stack.records[i].start_pc;
	  global_inlined_call_stack.records[i].end_pc = 
	                            saved_call_stack.records[i].end_pc;
	  global_inlined_call_stack.records[i].ranges =
	                            saved_call_stack.records[i].ranges;
	  global_inlined_call_stack.records[i].call_site_line = 
	                            saved_call_stack.records[i].call_site_line;
	  global_inlined_call_stack.records[i].call_site_column = 
                                    saved_call_stack.records[i].call_site_column;
	  global_inlined_call_stack.records[i].s = saved_call_stack.records[i].s;
	  global_inlined_call_stack.records[i].fn_name = 
                                    xstrdup (saved_call_stack.records[i].fn_name);
	  global_inlined_call_stack.records[i].calling_fn_name = 
                            xstrdup (saved_call_stack.records[i].calling_fn_name);
	  global_inlined_call_stack.records[i].call_site_filename = 
	                 xstrdup (saved_call_stack.records[i].call_site_filename);
	  global_inlined_call_stack.records[i].stack_frame_created = 
	                          saved_call_stack.records[i].stack_frame_created;
	  global_inlined_call_stack.records[i].stack_frame_printed = 
	                          saved_call_stack.records[i].stack_frame_printed;
	  global_inlined_call_stack.records[i].stepped_into = 
	                            saved_call_stack.records[i].stepped_into;
	  /* APPLE LOCAL begin radar 6545149  */
	  global_inlined_call_stack.records[i].func_sym = 
	                            saved_call_stack.records[i].func_sym;
	  /* APPLE LOCAL end radar 6545149  */
	}

      reset_saved_call_stack ();
    }
}

void
inlined_subroutine_save_before_dummy_call (void)
{
  if (saved_call_stack.nelts == 0)
    {
      int i;

      saved_call_stack.last_pc = global_inlined_call_stack.last_pc;
      saved_call_stack.last_inlined_pc = global_inlined_call_stack.last_inlined_pc;
      saved_call_stack.nelts = global_inlined_call_stack.nelts;
      saved_call_stack.current_pos = global_inlined_call_stack.current_pos;

      for (i = 1; i <= global_inlined_call_stack.nelts; i++)
	{
	  saved_call_stack.records[i].start_pc = 
                              global_inlined_call_stack.records[i].start_pc;
	  saved_call_stack.records[i].end_pc = 
                              global_inlined_call_stack.records[i].end_pc;
	  saved_call_stack.records[i].ranges =
	                      global_inlined_call_stack.records[i].ranges;
	  saved_call_stack.records[i].call_site_line  = 
                              global_inlined_call_stack.records[i].call_site_line;
	  saved_call_stack.records[i].call_site_column = 
                            global_inlined_call_stack.records[i].call_site_column;
	  saved_call_stack.records[i].s = global_inlined_call_stack.records[i].s;
	  saved_call_stack.records[i].fn_name = 
                           xstrdup (global_inlined_call_stack.records[i].fn_name);
	  saved_call_stack.records[i].calling_fn_name = 
                   xstrdup (global_inlined_call_stack.records[i].calling_fn_name);
	  saved_call_stack.records[i].call_site_filename = 
                xstrdup (global_inlined_call_stack.records[i].call_site_filename);
	  saved_call_stack.records[i].stack_frame_created = 
                         global_inlined_call_stack.records[i].stack_frame_created;
	  saved_call_stack.records[i].stack_frame_printed = 
                         global_inlined_call_stack.records[i].stack_frame_printed;
	  saved_call_stack.records[i].stepped_into = 
                                global_inlined_call_stack.records[i].stepped_into;
	  /* APPLE LOCAL begin radar 6545149  */
	  saved_call_stack.records[i].func_sym = 
                                global_inlined_call_stack.records[i].func_sym;
	  /* APPLE LOCAL end radar 6545149  */
	}
    }
}

int
rest_of_line_contains_inlined_subroutine (CORE_ADDR *end_of_line)
{
  struct symtab_and_line sal;
  struct symtab_and_line *cur;
  int current_line;
  int cur_pos;
  CORE_ADDR current_end;
  CORE_ADDR line_start_pc = 0;
  CORE_ADDR inline_start_pc = 0;
  int inlined_subroutine_found = 0;
  int inlined_subroutine_record_found = 0;

  if (!dwarf2_allow_inlined_stepping)
    return 0;

  sal = find_pc_line (stop_pc, 0);

  if (sal.line == 0)
    {
      inlined_subroutine_found = 0;
      current_end = 0;
      *end_of_line = 0;
      return inlined_subroutine_found;
    }

  cur_pos = current_inlined_subroutine_stack_position ();
  if (cur_pos > 0
      && !global_inlined_call_stack.records[cur_pos].stepped_into)
    cur_pos--;

  if (cur_pos > 0
      && sal.next)
    {
      for (cur = &sal; cur; cur = cur->next)
	{
	  if (cur->entry_type == INLINED_SUBROUTINE_LT_ENTRY
	      && cur->pc == global_inlined_call_stack.records[cur_pos].start_pc
	      && cur->end == record_end_pc 
	                            (global_inlined_call_stack.records[cur_pos]))
	    {
	      sal.symtab = cur->symtab;
	      sal.line = cur->line;
	      sal.pc = cur->pc;
	      sal.end = cur->end;
	      break;
	    }
	}
    }

  if (sal.next
      && (sal.next->entry_type == INLINED_SUBROUTINE_LT_ENTRY
	  || sal.next->entry_type == INLINED_CALL_SITE_LT_ENTRY))
    {
      inlined_subroutine_found = 1;
      inline_start_pc = sal.pc;
    }
  else  if (find_line_pc (sal.symtab, sal.line, &line_start_pc))
    if (line_start_pc != sal.pc)
      sal = find_pc_line (line_start_pc, 0);

  current_line = sal.line;
  current_end = sal.end;

  if (current_end < stop_pc)
    current_end = stop_pc;

  while (sal.line == current_line)
    {
      sal = find_pc_line (current_end, 0);
      if (sal.line == current_line)
	current_end = sal.end;
      cur = &sal;
      while (cur)
	{
	  if (cur->line == current_line
	      && (cur->entry_type ==  INLINED_SUBROUTINE_LT_ENTRY
		  || cur->entry_type == INLINED_CALL_SITE_LT_ENTRY))
	    {
	      inlined_subroutine_found = 1;
	      inline_start_pc = cur->pc;
	      current_end = cur->end;
	      break;
	    }
	  cur = cur->next;
	}
    }

  /*  If we haven't discovered any inlined subroutines beyond the current end
      pc (but on the same line) heck to see if the current start & end 
      addresses encompass one or more inlined subroutines.  */

  if (sal.symtab
      && (!inlined_subroutine_found
	  || inline_start_pc == stop_pc))

    {
      struct rb_tree_node_list *matches = NULL;
      struct rb_tree_node_list *current;
      struct rb_tree_node *tmp_node;
      struct inlined_call_stack_record *tmp_record;
      
      rb_tree_find_all_nodes_in_between 
	                         (sal.symtab->objfile->inlined_subroutine_data,
				  stop_pc, current_end, &matches);
      
      for (current = matches; current; current = current->next)
	{
	  tmp_node = current->node;
	  if (tmp_node != NULL)
	    {
	      tmp_record = (struct inlined_call_stack_record *) tmp_node->data;
	      inlined_subroutine_record_found = 1;
	      if (tmp_record->start_pc >= stop_pc
		  && tmp_record->start_pc < inline_start_pc)
		inline_start_pc = tmp_record->start_pc;
	    }
	}
    }

  /* This function is looking for inlined subroutines BEYOND the
     current pc, but within the same source line.  If we find an
     inlined subroutine AT the current pc, we do NOT want to count
     that; it will be dealt with properly elsewhere.  */

  if (inlined_subroutine_record_found)
    inlined_subroutine_found = 1;
  else if ((inline_start_pc != 0
	    && inline_start_pc == stop_pc)
	   || (current_end <= stop_pc))
    {
      inlined_subroutine_found = 0;
      current_end = 0;
    }
  
  *end_of_line = current_end;

  return inlined_subroutine_found;
}

void
find_next_inlined_subroutine (CORE_ADDR pc, CORE_ADDR *inline_start_pc, 
			      CORE_ADDR end_of_line)
{
  struct symtab_and_line sal;
  int current_line;
  CORE_ADDR current_end;
  struct symtab *sal_symtab = NULL;;

  *inline_start_pc = 0;
  sal = find_pc_line (pc, 0);
  current_line = sal.line;
  current_end = sal.end;
  sal_symtab = sal.symtab;

  while (sal.line == current_line
	 && !sal.next)
    {
      sal = find_pc_line (current_end, 0);
      if (sal.line == current_line)
	current_end = sal.end;
      if (sal.next
	  && (sal.next->entry_type == INLINED_SUBROUTINE_LT_ENTRY
	      || sal.next->entry_type == INLINED_CALL_SITE_LT_ENTRY))
	*inline_start_pc = sal.pc;
    }

  if (*inline_start_pc == 0 && sal_symtab)
    {
      struct rb_tree_node_list *matches = NULL;
      struct rb_tree_node_list *current;
      struct rb_tree_node *tmp_node;
      struct inlined_call_stack_record *tmp_record;

      rb_tree_find_all_nodes_in_between 
	                         (sal_symtab->objfile->inlined_subroutine_data,
				  stop_pc, end_of_line, &matches);

      for (current = matches; current; current = current->next)
	{
	  tmp_node = current->node;
	  if (tmp_node != NULL)
	    {
	      tmp_record = (struct inlined_call_stack_record *) tmp_node->data;
	      if (*inline_start_pc == 0
		  || tmp_record->start_pc < *inline_start_pc)
		*inline_start_pc = tmp_record->start_pc;
	    }
	}
    }
}

int
is_within_stepping_ranges (CORE_ADDR pc)
{
  int i;
  int range_found = 0;

  if (!stepping_ranges)
    return  0;

  for (i = 0; i < stepping_ranges->nelts && !range_found; i++)
    {
      if (pc >= stepping_ranges->ranges[i].startaddr
	  && pc < stepping_ranges->ranges[i].endaddr)
	range_found = 1;
    }

  return range_found;
}

int
is_at_stepping_ranges_end (CORE_ADDR pc)
{
  int i;
  int end_found = 0;

  if (!stepping_ranges)
    return  0;

  for (i = 0; i < stepping_ranges->nelts && !end_found; i++)
    if (pc == stepping_ranges->ranges[i].endaddr)
      end_found = 1;

  return end_found;
}

void
inlined_subroutine_free_objfile_data (struct rb_tree_node *root)
{
  struct inlined_call_stack_record *data;
  if (root->left)
    inlined_subroutine_free_objfile_data (root->left);

  if (root->right)
    inlined_subroutine_free_objfile_data (root->right);

  data = (struct inlined_call_stack_record *) root->data;
  if (data->ranges)
    xfree (data->ranges);
  xfree (root->data);
  xfree (root);
}

void
inlined_subroutine_free_objfile_call_sites (struct rb_tree_node *root)
{
  struct dwarf_inlined_call_record *data;
  if (root->left)
    inlined_subroutine_free_objfile_call_sites (root->left);

  if (root->right)
    inlined_subroutine_free_objfile_call_sites (root->right);

  data = (struct dwarf_inlined_call_record *) root->data;
  if (data->ranges)
    xfree (data->ranges);
  xfree (root->data);
  xfree (root);
}

static void
update_inlined_data_addresses (CORE_ADDR offset,
			       struct rb_tree_node *tree)
{
  struct inlined_call_stack_record *data;
  int i;

  tree->key += offset;
  tree->third_key += offset;
  data = (struct inlined_call_stack_record *) tree->data;
  data->start_pc += offset;
  data->end_pc += offset;
  if (data->ranges)
    for (i = 0; i < data->ranges->nelts; i++)
      {
	data->ranges->ranges[i].startaddr += offset;
	data->ranges->ranges[i].endaddr += offset;
      }

  if (tree->left)
    update_inlined_data_addresses (offset, tree->left);
  
  if (tree->right)
    update_inlined_data_addresses (offset, tree->right);
}

void
inlined_subroutine_objfile_relocate (struct objfile *objfile,
				     struct rb_tree_node *tree_node,
				     struct section_offsets *deltas)
{
  struct obj_section *sect = NULL;
  CORE_ADDR offset = 0;
  
  if (!tree_node)
    return;
  
  /* Set offset to be the offset of the text section, for a default.  */

  offset = ANOFFSET (deltas, SECT_OFF_TEXT (objfile));

  /* Go through all the sections looking for the one containing the addresses
     of the tree_node record.  If found, use the offset from that section.  */

  ALL_OBJFILE_OSECTIONS (objfile, sect)
    if (objfile->separate_debug_objfile_backlink == NULL
	&& sect->addr <= tree_node->key && tree_node->key < sect->endaddr)
      {
	offset = ANOFFSET (deltas, sect->the_bfd_section->index);
	break;
      }
		       
  if (offset)
    update_inlined_data_addresses (offset, tree_node);
}

int
inlined_function_find_first_line (struct symtab_and_line sal)
{
  int first_line = sal.line;
  int len;
  int i;
  struct linetable *l;
  struct linetable_entry *item;
  struct linetable_entry *prev;

  l = LINETABLE (sal.symtab);
  if (l)
    {
      len = l->nitems;
      if (len)
	{
	  prev = NULL;
	  item = l->item;
	  for (i = 0; i < len; i++, item++)
	    if (item->line >= sal.line
		&& item->pc >= sal.pc
		&& item->entry_type == NORMAL_LT_ENTRY)
	      {
		first_line = item->line;
		break;
	      }
	}
    }

  return first_line;
}

void
save_thread_inlined_call_stack (ptid_t ptid)
{
  struct thread_info *tp;
  int num_bytes;

  tp = find_thread_id (pid_to_thread_id (ptid));
  if (tp == NULL)
    return;

  if (current_inlined_subroutine_stack_size() == 0)
    {
      tp->thread_inlined_call_stack = NULL;
      return;
    }

  /* APPLE LOCAL begin remember stepping into inlined subroutine.  */
  tp->inlined_step_range_end = inlined_step_range_end;
  inlined_step_range_end = 0;
  /* APPLE LOCAL end remember stepping into inlined subroutine.  */

  tp->thread_inlined_call_stack = (struct inlined_function_data *) 
                            xmalloc (sizeof (struct inlined_function_data));

  tp->thread_inlined_call_stack->last_pc = global_inlined_call_stack.last_pc;
  tp->thread_inlined_call_stack->last_inlined_pc = 
                                  global_inlined_call_stack.last_inlined_pc;
  tp->thread_inlined_call_stack->max_array_size = 
                                  global_inlined_call_stack.max_array_size;
  tp->thread_inlined_call_stack->nelts = global_inlined_call_stack.nelts;
  tp->thread_inlined_call_stack->current_pos = 
                                       global_inlined_call_stack.current_pos;

  num_bytes = global_inlined_call_stack.max_array_size *
                                sizeof (struct inlined_call_stack_record);

  tp->thread_inlined_call_stack->records = (struct inlined_call_stack_record *)
                                                           xmalloc (num_bytes);

  memcpy (tp->thread_inlined_call_stack->records, 
	  global_inlined_call_stack.records, num_bytes);
  
}

void
restore_thread_inlined_call_stack (ptid_t ptid)
{
  struct thread_info *tp;
  int num_bytes;
  int i;

  tp = find_thread_id (pid_to_thread_id (ptid));
  if (tp == NULL)
    return;

  if (tp->thread_inlined_call_stack == NULL
      || tp->thread_inlined_call_stack->nelts == 0)
    {
      inlined_function_reinitialize_call_stack ();
      return;
    }

  global_inlined_call_stack.last_pc = tp->thread_inlined_call_stack->last_pc;
  global_inlined_call_stack.last_inlined_pc =
                              tp->thread_inlined_call_stack->last_inlined_pc;

  gdb_assert (global_inlined_call_stack.max_array_size >=
	                      tp->thread_inlined_call_stack->max_array_size);

  global_inlined_call_stack.nelts = tp->thread_inlined_call_stack->nelts;
  global_inlined_call_stack.current_pos =
                                   tp->thread_inlined_call_stack->current_pos;


  num_bytes = global_inlined_call_stack.max_array_size *
                                   sizeof (struct inlined_call_stack_record);

  memset (global_inlined_call_stack.records, 0, num_bytes);
  for (i = 1; 
       i <= global_inlined_call_stack.nelts 
	 && i < global_inlined_call_stack.max_array_size; i++)
      memcpy (&(global_inlined_call_stack.records[i]), 
	      &(tp->thread_inlined_call_stack->records[i]), 
	      sizeof (struct inlined_call_stack_record));

  /* APPLE LOCAL remember stepping into inlined subroutine.  */
  inlined_step_range_end = tp->inlined_step_range_end;

  /* I believe that on switching threads, gdb's stack is thrown away
     and recreated, so the stack_frame_created fields need to be reset.  */

  flush_inlined_subroutine_frames ();

}

/* Make sure the temp_frame_stack and global_inlined_call_stack fields
   for creating & printing frames are reset after doing backtraces.  */

void
inlined_function_reset_frame_stack (void)
{
  int i;

  flush_inlined_subroutine_frames ();

  if (temp_frame_stack.last_pc == global_inlined_call_stack.last_pc
      && temp_frame_stack.last_inlined_pc == 
                              global_inlined_call_stack.last_inlined_pc
      && temp_frame_stack.nelts == global_inlined_call_stack.nelts
      && temp_frame_stack.current_pos == global_inlined_call_stack.current_pos)
    {
      for (i = 1; i <= temp_frame_stack.nelts; i++)
	global_inlined_call_stack.records[i].stack_frame_created =
	  temp_frame_stack.records[i].stack_frame_created;
    }
}

char *
last_inlined_call_site_filename (struct frame_info *fi)
{
  char *file_name;
  if (global_inlined_call_stack.nelts > 0
      && frame_relative_level (fi) <= global_inlined_call_stack.nelts)
    file_name = global_inlined_call_stack.records[1].call_site_filename;
  else if (temp_frame_stack.nelts > 0)
    file_name = temp_frame_stack.records[1].call_site_filename;
  else
    file_name = NULL;
  return file_name;
}

/* APPLE LOCAL begin inlined function symbols & blocks  */
/* APPLE LOCAL begin radar 6545149  */
/* Given a block that does not have a function symbol (which means it
   *might* be a block for an inlined subroutine, this function tries to
   determine if it is the block for an inlined subroutine, which inlined
   subroutine it is the block for (if there are multiple candidates), and
   returns the symbol for the inlined subroutine (or NULL if it's not the
   block for an inlined subroutine.  */

struct symbol *
block_inlined_function (struct block *bl, struct bfd_section *section)
{
  int syms_found = 0;
  struct symtab_and_line sal;
  struct rb_tree_node_list *matches = NULL;
  struct rb_tree_node_list *current;
  struct inlined_call_stack_record *tmp_record;
  struct symbol *func_sym = NULL;
  struct objfile *objfile = NULL;
  struct objfile *objf;

  /* Find the objfile, to check its inlined subroutine data.  */

  if (section != NULL)
    {
      ALL_OBJFILES (objf)
      {
	if (objf->obfd == section->owner)
	  objfile = objf;
      }
    }
  else
    {
      sal = find_pc_sect_line (bl->startaddr, section, 0);
      if (sal.symtab)
	objfile = sal.symtab->objfile;
    }

  if (objfile == NULL)
    return NULL;

  /* Find all inlined subroutines (if any) with the same starting and
     ending addresses as the block.  */
  
  rb_tree_find_all_exact_matches (objfile->inlined_subroutine_data,
				  bl->startaddr, bl->endaddr, &matches);
  /* APPLE LOCAL end radar 6381384  add section to symtab lookups  */

  if (!matches)
    return NULL;

  if (matches->next == NULL)
    {
      tmp_record = (struct inlined_call_stack_record *) matches->node->data;

      /* Exactly one match was found.  Verify that the record has
	 a valid function name.  */
      func_sym = tmp_record->func_sym;
    }
  else
    {
      int found = 0;
      struct inlined_call_stack_record *tmp_record;
      
      for (current = matches; current && !found; current = current->next)
	{
	  tmp_record = (struct inlined_call_stack_record *) current->node->data;
	  if (tmp_record->func_sym
	      && tmp_record->func_sym->ginfo.value.block == bl)
	    {
	      func_sym = tmp_record->func_sym;
	      found = 1;
	    }
	}
    }
  
  return func_sym;
}
/* APPLE LOCAL begin radar 6545149  */

/* Add SYM, the symbol for an inlined subroutine, to LIST, the list
   of inlined subroutine symbols.  */

void
add_symbol_to_inlined_subroutine_list (struct symbol *sym,
				       struct pending **list)
{
  struct pending *tmp;

  if (!sym || !sym->ginfo.name)
    return;

  if ((*list == NULL) || (*list)->nsyms == PENDINGSIZE)
    {
      tmp = (struct pending *) xmalloc (sizeof (struct pending));
      tmp->next = *list;
      tmp->nsyms = 1;
      tmp->symbol[0] = sym;
      *list = tmp;
    }
  else
    (*list)->symbol[(*list)->nsyms++] = sym;
}
/* APPLE LOCAL end inlined function symbols & blocks */


/* APPLE LOCAL begin radar 6534195  */
int
func_sym_has_inlining (struct symbol *func_sym, struct frame_info *fi)
{
  struct inlined_function_data *stack_ptr;
  CORE_ADDR pc = get_frame_pc (fi);
  int level = frame_relative_level (fi);
  int i;
  int found = 0;

  if (global_inlined_call_stack.nelts > 0
      && global_inlined_call_stack.records[1].stepped_into
      && level <= global_inlined_call_stack.nelts)
    stack_ptr = &global_inlined_call_stack;
  else
    stack_ptr = &temp_frame_stack;

  /* Look for an inlining record where the inlined function contains
     the pc for the frame, and the calling function name matches
     the name of the function symbol parameter.   */

  for (i = 1; i <= stack_ptr->nelts && !found; i++)
    {
      if (stack_ptr->records[i].start_pc <= pc
	  && pc <= stack_ptr->records[i].end_pc 
	  && (strcmp (stack_ptr->records[i].calling_fn_name, 
		      func_sym->ginfo.name) == 0))
	found = 1;
    }

  return found;
}
/* APPLE LOCAL end radar 6534195  */
/* APPLE LOCAL end subroutine inlining  (entire file) */
