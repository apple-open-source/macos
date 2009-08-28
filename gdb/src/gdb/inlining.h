/* APPLE LOCAL begin subroutine inlining. This entire file is APPLE LOCAL  */
/* Inlined function call stack definitions for GDB.

   Copyright 2006.

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

#if !defined (INLINE_H)
#define INLINE_H
#include <block.h>
#include "frame.h"
#include "bfd.h"

/* APPLE LOCAL inlined function symbols & blocks  */
struct pending;

/* The foilowing structure is used to keep track of the current state for
   inlined subroutines in the inferior.  */

struct inlined_function_data 
{
  /* The value of stop_pc, the last time this struct was updated.  */

  CORE_ADDR last_pc; 

  /* The last stop_pc value that was in an inlined subroutine. */

  CORE_ADDR last_inlined_pc;

  /* The current maximum number of malloc'd records.  */

  int max_array_size;

/* The number of "valid" records.  The zero'th record is always valid
   (and blank). The first record with real data will be at position
   one.  */

  int nelts;               

 /* The user's current position in the stack (array) of valid inlined
    subroutine records.  This can be tricky because the user may be at
    the first valid record, but not have stepped into to (e.g.
    stopped at the call site.  Therefore this field needs to be used
    in conjunction with the "stepped_into" field of the record it
    points to, to precisely determine what the user's context should
    be.  */

  int current_pos;

 /* The stack of valid inlined subroutine records for the current
    value of stop_pc.  The zero'th record is always blank, so valid
    records really start at position 1.  The 'bottom' to the inlined
    call stack will be at position '1', and the top of the inlined
    call stack will be at position 'current_pos'.  */

  struct inlined_call_stack_record *records; 
};


/* The following struct describes the individual records in the stack for
   inlined subroutines (one record per inlining).  */

struct inlined_call_stack_record
{
  /* The PC value at which the inlined subroutine begins.  */

  CORE_ADDR start_pc;

  /* The first PC value beyond where the inlined subroutine ends.  */

  CORE_ADDR end_pc;

  /* Ranges of address for the inlined subroutine, if it has multiple
     non-contiguous ranges of addresses.  */

  struct address_range_list *ranges;

  /* The source line number of the call site where the inlining
     occurred.  */

  int call_site_line;
  
  /* The source column position of the call site where the inlining
     occurred.  */

  int call_site_column;

  /* The symbol table for the line table entries for the inlined
     subroutine.  */

  struct symtab *s;

  /* The name of the subroutine that was inlined (callee).  */

  char *fn_name;

  /* The name of the subroutine where the inlining occurred (caller).  */

  char *calling_fn_name;

  /* The file name containing the calling function.  */

  char *call_site_filename;

  /* APPLE LOCAL begin radar 6545149  */
  /* The function symbol gdb generates for the inlined subroutine instance.  */
  
  struct symbol *func_sym;
  /* APPLE LOCAL end radar 6545149  */

  /* Flag indicating an INLINED_FRAME has been created for this
     record.  */

  int stack_frame_created;

  /* Flag indicating the INLINED_FRAME for this record has been
     printed.  This flag cane be set and cleared multiple times during
     the lifetime of a record.  */

  int stack_frame_printed;

  /* Flag indicating the user has stepped from the call site into the
     subroutine (the inferior is not run and stop_pc does not change
     when that happens; only the user's context changes).  */
  int stepped_into;
};

/* APPLE LOCAL begin subroutine inlining  */
/* Stores inlining information from DWARF dies, to write into line
   table entries, and (eventually) to be translated and stored into
   the main inlining records in the per-objfile trees.  */

struct dwarf_inlined_call_record {
  unsigned file_index;
  unsigned line;
  unsigned column;
  unsigned decl_file_index;
  unsigned decl_line;
  char *name;
  char *parent_name;
  /* APPLE LOCAL radar 6545149  */
  struct symbol *func_sym;
  CORE_ADDR lowpc;
  CORE_ADDR highpc;
  /* APPLE LOCAL - address ranges  */
  struct address_range_list *ranges;
};

/* Ranges of address for current subroutine, being stepped over or through;
   at this time the assumption is that this is only needed for inlined
   subroutines.  */

extern struct address_range_list *stepping_ranges;

/* Global data structure for keeping track of current status
   of PC with respect to inlined subroutines.  */

extern struct inlined_function_data global_inlined_call_stack;

/* Global flag used to communicate between various functions that the
   user is 'stepping' into an inlined subroutine call (i.e. the user's
   context should change, but the inferior should not execute and the
   PC will not change).  */

extern int stepping_into_inlined_subroutine;

/* Global variable used to contain the start address of an inlined
   subroutine the user is stepping into, if there are intervening
   function calls.  */

extern CORE_ADDR inlined_step_range_end;

/* Global flag used to communicate between various functions that the
   user is stepping over an inlined subroutine call.  */

extern int stepping_over_inlined_subroutine;

/* Global flag used to communicate between various functions that the
   user is finishing out of an inlined subroutine.  */

extern int finishing_inlined_subroutine;

/* Necessary thing for new frame type (INLINED_FRAME).  */

extern const struct frame_unwind *const inlined_frame_unwind;

/* Externally visible functions for accessing/manipulating the
   global_inlined_call_stack.  */

extern void inlined_function_reset_frame_stack (void);

extern void inlined_function_initialize_call_stack (void);

extern void inlined_function_reinitialize_call_stack (void);

extern int inlined_function_call_stack_initialized_p (void);

extern void inlined_function_update_call_stack (CORE_ADDR);

extern void inlined_function_add_function_names (struct objfile *,
						 CORE_ADDR, CORE_ADDR, int, 
						 int, const char *, 
                                                 const char *, 
						 struct address_range_list *,
						 /* APPLE LOCAL radar 6545149 */
						 struct symbol *);

extern int at_inlined_call_site_p (char **, int *, int *);

extern int in_inlined_function_call_p (CORE_ADDR *);

extern void adjust_current_inlined_subroutine_stack_position (int);

extern int current_inlined_subroutine_stack_position (void);

extern CORE_ADDR inlined_function_call_stack_pc (void);

extern int current_inlined_subroutine_stack_size (void);

extern struct symtab * current_inlined_subroutine_stack_symtab (void);

extern CORE_ADDR current_inlined_subroutine_call_stack_start_pc (void);

extern CORE_ADDR current_inlined_subroutine_call_stack_end_pc (void);

extern char * current_inlined_subroutine_function_name (void);

extern char * current_inlined_subroutine_calling_function_name (void);

extern int current_inlined_subroutine_call_site_line (void);

extern int inlined_function_end_of_inlined_code_p (CORE_ADDR);

extern struct frame_info * get_current_inlined_frame (void);

extern void inlined_frame_prev_register (struct frame_info *, void **, int, 
					 enum opt_state *, enum lval_type *, 
                                         CORE_ADDR *, 
					 int *, gdb_byte *);

void restore_thread_inlined_call_stack (ptid_t ptid);

void save_thread_inlined_call_stack (ptid_t ptid);

extern void flush_inlined_subroutine_frames (void);

extern void clear_inlined_subroutine_print_frames (void);

extern void step_into_current_inlined_subroutine (void);

extern int current_inlined_bottom_call_site_line (void);

extern struct symtabs_and_lines
check_for_additional_inlined_breakpoint_locations (struct symtabs_and_lines,
						   char **, struct expression **,
						   char **, char ***,
						   struct expression ***,
						   char ***);

extern void 
inlined_subroutine_adjust_position_for_breakpoint (struct breakpoint *);

extern void inlined_subroutine_save_before_dummy_call (void);

extern void inlined_subroutine_restore_after_dummy_call (void);

extern int rest_of_line_contains_inlined_subroutine (CORE_ADDR *);

void inlined_update_frame_sal (struct frame_info *fi, struct symtab_and_line *sal);

extern void find_next_inlined_subroutine (CORE_ADDR, CORE_ADDR *, CORE_ADDR);

void set_current_sal_from_inlined_frame (struct frame_info *fi, int center);

extern void print_inlined_frame (struct frame_info *, int, enum print_what,
				 int, struct symtab_and_line, int);

extern int is_within_stepping_ranges (CORE_ADDR);

extern int is_at_stepping_ranges_end (CORE_ADDR);

extern int dwarf2_allow_inlined_stepping;
extern int dwarf2_debug_inlined_stepping;

/* Defined in dwarf2read.c  */

enum rb_tree_colors { RED, BLACK, UNINIT };


/* Basic tree node definition for red-black trees.  This particular
   node allows for three keys (primary, secondary, third) to be used
   for sorting the nodes.  The inlined call site red-black trees need
   all three keys.  */

struct rb_tree_node {
  CORE_ADDR  key;             /* Primary sorting key                       */
  int secondary_key;          /* Secondary sorting key                     */
  CORE_ADDR third_key;        /* Third sorting key                         */
  void *data;                 /* Main data; varies between different apps  */
  enum rb_tree_colors color;  /* Color of the tree node (for balancing)    */
  struct rb_tree_node *parent; /* Parent in the red-black tree             */
  struct rb_tree_node *left;   /* Left child in the red-black tree         */
  struct rb_tree_node *right;  /* Right child in the red-black tree        */
};

extern struct rb_tree_node *rb_tree_find_node (struct rb_tree_node *, 
						long long, int);

extern struct rb_tree_node *rb_tree_find_node_all_keys (struct rb_tree_node *,
							long long, int,
							long long);

extern void rb_tree_insert (struct rb_tree_node **, struct rb_tree_node *, 
			    struct rb_tree_node *);

extern void inlined_subroutine_free_objfile_data (struct rb_tree_node *);
extern void inlined_subroutine_free_objfile_call_sites (struct rb_tree_node *);

extern void inlined_subroutine_objfile_relocate (struct objfile *,
						 struct rb_tree_node *,
						 struct section_offsets *);

extern int inlined_function_find_first_line (struct symtab_and_line);
extern char *last_inlined_call_site_filename (struct frame_info *);
/* APPLE LOCAL begin inlined function symbols & blocks  */
extern void add_symbol_to_inlined_subroutine_list (struct symbol *, 
						   struct pending **);
/* APPLE LOCAL radar 6381384  add section to symtab lookups  */
extern struct symbol *block_inlined_function (struct block *, 
					      struct bfd_section *);
/* APPLE LOCAL end inlined function symbols & blocks  */

extern int func_sym_has_inlining (struct symbol *, struct frame_info *);
#endif /* !defined(INLINE_H) */
/* APPLE LOCAL end subroutine inlining (entire file) */
