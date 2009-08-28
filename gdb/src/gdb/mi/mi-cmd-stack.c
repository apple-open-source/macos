/* MI Command Set - stack commands.
   Copyright 2000, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions (a Red Hat company).

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
#include "target.h"
#include "frame.h"
#include "value.h"
#include "mi-cmds.h"
#include "mi-main.h"
#include "ui-out.h"
#include "varobj.h"
#include "wrapper.h"
#include "interps.h"
#include "symtab.h"
#include "block.h"
#include "stack.h"
#include "dictionary.h"
#include "gdb_string.h"
#include "objfiles.h"
#include "gdb_regex.h"
/* APPLE LOCAL - subroutine inlining  */
#include "inlining.h"

/* FIXME: There is no general mi header to put this kind of utility function.*/
extern void mi_report_var_creation (struct ui_out *uiout, struct varobj *var);

void mi_interp_stack_changed_hook (void);
void mi_interp_frame_changed_hook (int new_frame_number);
void mi_interp_context_hook (int thread_id);

/* This regexp pattern buffer is used for the file_list_statics
   and file_list_globals for the filter.  It doesn't look like the
   regexp package has an explicit pattern free, it tends to just reuse
   one buffer.  I don't want to use their global buffer because the
   psymtab->symtab code uses it to do C++ method detection.  So I am going
   to keep a separate one here.  */

struct re_pattern_buffer mi_symbol_filter;

static char *print_values_bad_input_string = 
           "Unknown value for PRINT_VALUES: must be: 0 or \"--no-values\", "
	   "1 or \"--all-values\", 2 or \"--simple-values\", "
           "3 or \"--make-varobj\"";

/* Use this to print any extra info in the stack listing output that is
   not in the standard gdb printing */

void mi_print_frame_more_info (struct ui_out *uiout,
				struct symtab_and_line *sal,
				struct frame_info *fi);

static void list_args_or_locals (int locals, enum print_values values, 
				 struct frame_info *fi,
				 int all_blocks);

static void print_syms_for_block (struct block *block, 
				  struct frame_info *fi, 
				  struct ui_stream *stb,
				  int locals, 
				  int consts,
				  enum print_values values,
				  struct re_pattern_buffer *filter);

static void
print_globals_for_symtab (struct symtab *file_symtab, 
			  struct ui_stream *stb,
			  enum print_values values,
			  int consts,
			  struct re_pattern_buffer *filter);

/* Print a list of the stack frames. Args can be none, in which case
   we want to print the whole backtrace, or a pair of numbers
   specifying the frame numbers at which to start and stop the
   display. If the two numbers are equal, a single frame will be
   displayed. */
enum mi_cmd_result
mi_cmd_stack_list_frames (char *command, char **argv, int argc)
{
  int frame_low;
  int frame_high;
  int i;
  struct cleanup *cleanup_stack;
  struct frame_info *fi;

  if (argc > 2 || argc == 1)
    error (_("mi_cmd_stack_list_frames: Usage: [FRAME_LOW FRAME_HIGH]"));

  if (argc == 2)
    {
      frame_low = atoi (argv[0]);
      frame_high = atoi (argv[1]);
    }
  else
    {
      /* Called with no arguments, it means we want the whole
         backtrace. */
      frame_low = -1;
      frame_high = -1;
    }

  /* Let's position fi on the frame at which to start the
     display. Could be the innermost frame if the whole stack needs
     displaying, or if frame_low is 0. */
  for (i = 0, fi = get_current_frame ();
       fi && i < frame_low;
       i++, fi = get_prev_frame (fi));

  if (fi == NULL)
    error (_("mi_cmd_stack_list_frames: Not enough frames in stack."));

  cleanup_stack = make_cleanup_ui_out_list_begin_end (uiout, "stack");

  /* Now let;s print the frames up to frame_high, or until there are
     frames in the stack. */
  for (;
       fi && (i <= frame_high || frame_high == -1);
       i++, fi = get_prev_frame (fi))
    {
      QUIT;
      /* Print the location and the address always, even for level 0.
         args == 0: don't print the arguments. */
      print_frame_info (fi, 1, LOC_AND_ADDRESS, 0 /* args */ );
    }

  /* APPLE LOCAL begin subroutine inlining  */
  clear_inlined_subroutine_print_frames ();
  /* APPLE LOCAL end subroutine inlining  */
  do_cleanups (cleanup_stack);
  if (i < frame_high)
    error (_("mi_cmd_stack_list_frames: Not enough frames in stack."));

  return MI_CMD_DONE;
}

/* Helper print function for mi_cmd_stack_list_frames_lite */
static void 
mi_print_frame_info_lite_base (struct ui_out *uiout,
			       int with_names,
			       int frame_num,
			       CORE_ADDR pc,
			       CORE_ADDR fp)
{
  char num_buf[8];
  struct cleanup *list_cleanup;
  sprintf (num_buf, "%d", frame_num);
  ui_out_text (uiout, "Frame ");
  ui_out_text(uiout, num_buf);
  ui_out_text(uiout, ": ");
  list_cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, num_buf);
  ui_out_field_core_addr (uiout, "pc", pc);
  ui_out_field_core_addr (uiout, "fp", fp);
  if (with_names)
    {
      struct minimal_symbol *msym ;
      struct obj_section *osect;
      int has_debug_info = 0;
      struct partial_symtab *pst;

      /* APPLE LOCAL: if we are going to print names, we should raise
         the load level to ALL.  We will avoid doing psymtab to symtab,
         since we just want the function */

      pc_set_load_state (pc, OBJF_SYM_ALL, 0);
      
      msym = lookup_minimal_symbol_by_pc (pc);
      if (msym == NULL)
	ui_out_field_string (uiout, "func", "<\?\?\?\?>");
      else
	{
	  char *name = SYMBOL_NATURAL_NAME (msym);
	  if (name == NULL)
	    ui_out_field_string (uiout, "func", "<\?\?\?\?>");
	  else
	    ui_out_field_string (uiout, "func", name);
	} 
      /* This is a pretty quick and dirty way to check whether there
	 are debug symbols for this PC...  I don't care WHAT symbol
	 contains the PC, just that there's some psymtab that
	 does.  */
      osect = find_pc_sect_in_ordered_sections (pc, NULL);
      if (osect != NULL && osect->objfile != NULL)
	{
	  ALL_OBJFILE_PSYMTABS (osect->objfile, pst)
	    {
	      if (pc >= pst->textlow && pc < pst->texthigh)
		{
		  has_debug_info = 1;
		  break;
		}
	    }
	}
      ui_out_field_int (uiout, "has_debug", has_debug_info);
    }
  ui_out_text (uiout, "\n");
  do_cleanups (list_cleanup);
}

static void
mi_print_frame_info_with_names_lite (struct ui_out *uiout,
			  int frame_num,
			  CORE_ADDR pc,
			  CORE_ADDR fp)
{
  mi_print_frame_info_lite_base (uiout, 1, frame_num, pc, fp);
}

static void
mi_print_frame_info_lite (struct ui_out *uiout,
			  int frame_num,
			  CORE_ADDR pc,
			  CORE_ADDR fp)
{
  mi_print_frame_info_lite_base (uiout, 0, frame_num, pc, fp);
}

/* Print a list of the PC and Frame Pointers for each frame in the stack;
   also return the total number of frames. An optional argument "-limit"
   can be give to limit the number of frames printed. 
   An optional "-names (0|1)" flag can be given which if 1 will cause the names to
   get printed with the backtrace.
  */

enum mi_cmd_result
mi_cmd_stack_list_frames_lite (char *command, char **argv, int argc)
{
    int limit = 0;
    int names = 0;
    int valid;
    unsigned int count = 0;
    void (*print_fun) (struct ui_out*, int, CORE_ADDR, CORE_ADDR);

#ifndef FAST_COUNT_STACK_DEPTH
    int i;
    struct frame_info *fi;
#endif

    if (!target_has_stack)
        error ("mi_cmd_stack_list_frames_lite: No stack.");

    if ((argc > 4))
        error ("mi_cmd_stack_list_frames_lite: Usage: [-names (0|1)] [-limit max_frame_number]");

    limit = -1;
    names = 0;

    while (argc > 0)
      {
	if (strcmp (argv[0], "-limit") == 0)
	  {
	    if (argc == 1)
	      error ("mi_cmd_stack_list_frames_lite: No argument to -limit.");

	    if (! isnumber (argv[1][0]))
	      error ("mi_cmd_stack_list_frames_lite: Invalid argument to -limit.");
	    limit = atoi (argv[1]);
	    argc -= 2;
	    argv += 2;
	  }
	else if (strcmp (argv[0], "-names") == 0)
	  {
	    if (argc == 1)
	      error ("mi_cmd_stack_list_frames_lite: No argument to -names.");

	    if (! isnumber (argv[1][0]))
	      error ("mi_cmd_stack_list_frames_lite: Invalid argument to -names.");
	    names = atoi (argv[1]);
	    argc -= 2;
	    argv += 2;
	  }
	else
	  error ("mi_cmd_stack_list_frames_lite: invalid flag: %s", argv[0]);
      }
	

    if (names)
      print_fun = mi_print_frame_info_with_names_lite;
    else
      print_fun = mi_print_frame_info_lite;

#ifdef FAST_COUNT_STACK_DEPTH
      valid = FAST_COUNT_STACK_DEPTH (-1, limit, &count, print_fun);
#else
    /* Start at the inner most frame */
    {
      struct cleanup *list_cleanup;
      for (fi = get_current_frame (); fi ; fi = get_next_frame(fi))
        ;

      fi = get_current_frame ();
      
      if (fi == NULL)
        error ("mi_cmd_stack_list_frames_lite: No frames in stack.");
      
      list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "frames");
      
      for (i = 0; fi != NULL; (fi = get_prev_frame (fi)), i++) 
	{
	  QUIT;
	  
	  if ((limit == 0) || (i < limit))
	    {
	      print_fun (uiout, i, get_frame_pc (fi), 
                                        get_frame_base(fi));
	    }
	}
      
      count = i;
      valid = 1;
      do_cleanups (list_cleanup);
    }
#endif
    
    ui_out_text (uiout, "Valid: ");
    ui_out_field_int (uiout, "valid", valid);
    ui_out_text (uiout, "\nCount: ");
    ui_out_field_int (uiout, "count", count);
    ui_out_text (uiout, "\n");
    
    return MI_CMD_DONE;
}

void 
mi_print_frame_more_info (struct ui_out *uiout,
				struct symtab_and_line *sal,
				struct frame_info *fi)
{
  /* I would feel happier if we used ui_out_field_skip for all the fields
     that we don't know how to set (like the file if we don't have symbols)
     but the rest of print_frame just omits the fields if they are not known,
     so I will do the same here...  */

  if (sal && sal->symtab && sal->symtab->dirname)
    ui_out_field_string (uiout, "dir", sal->symtab->dirname);
}

enum mi_cmd_result
mi_cmd_stack_info_depth (char *command, char **argv, int argc)
{
  int frame_high;
  unsigned int i;
  struct frame_info *fi;

  if (argc > 1)
    error (_("mi_cmd_stack_info_depth: Usage: [MAX_DEPTH]"));

  if (argc == 1)
    frame_high = atoi (argv[0]);
  else
    /* Called with no arguments, it means we want the real depth of
       the stack. */
    frame_high = -1;

#ifdef FAST_COUNT_STACK_DEPTH
  if (! FAST_COUNT_STACK_DEPTH (frame_high, frame_high, &i, NULL))
#endif
    {
      for (i = 0, fi = get_current_frame ();
	   fi && (i < frame_high || frame_high == -1);
	   i++, fi = get_prev_frame (fi))
	QUIT;
    }
  ui_out_field_int (uiout, "depth", i);
  
  return MI_CMD_DONE;
}

/*
  mi_decode_print_values, ARG is the mi standard "print-values"
  argument.  We decode this into an enum print_values.
*/

enum print_values
mi_decode_print_values (char *arg)
{
  enum print_values print_values = 0;

  /* APPLE LOCAL: We muck with this a bit.  We set
     2 to mean PRINT_MAKE_VAROBJ as well as 3
     and --make-varobjs.  To get the "2" behavior
     you have to explicitly use --simple-values.  */

  if (strcmp (arg, "0") == 0
      || strcmp (arg, "--no-values") == 0)
    print_values = PRINT_NO_VALUES;
  else if (strcmp (arg, "1") == 0
	   || strcmp (arg, "--all-values") == 0)
    print_values = PRINT_ALL_VALUES;
  else if (strcmp (arg, "2") == 0)
    print_values = PRINT_MAKE_VAROBJ;
  else if (strcmp (arg, "--simple-values") == 0)
    print_values = PRINT_SIMPLE_VALUES;
  else if (strcmp (arg, "3") == 0
	   || strcmp (arg, "--make-varobjs") == 0)
    print_values = PRINT_MAKE_VAROBJ;
  else
    print_values = PRINT_BAD_INPUT;

  return print_values;
}

/* Print a list of the locals for the current frame. With argument of
   0, print only the names, with argument of 1 print also the
   values. */
enum mi_cmd_result
mi_cmd_stack_list_locals (char *command, char **argv, int argc)
{
  struct frame_info *frame;
  enum print_values print_values;
  /* APPLE LOCAL mi */
  int all_blocks;

  if (argc < 1 || argc > 2)
    error ("mi_cmd_stack_list_locals: Usage: PRINT_VALUES [ALL_BLOCKS]");

   frame = get_selected_frame (NULL);

  print_values = mi_decode_print_values (argv[0]);
  if (print_values == PRINT_BAD_INPUT)
    error ("%s", print_values_bad_input_string);

  if (argc >= 2)
    all_blocks = atoi (argv[1]);
  else
    all_blocks = 0;

  list_args_or_locals (1, print_values, frame, all_blocks);

  return MI_CMD_DONE;
}

/* Print a list of the arguments for the current frame. With argument
   of 0, print only the names, with argument of 1 print also the
   values, with argument of 2, create varobj for the arguments. */

enum mi_cmd_result
mi_cmd_stack_list_args (char *command, char **argv, int argc)
{
  int frame_low;
  int frame_high;
  int i;
  enum print_values values;
  struct frame_info *fi;
  struct cleanup *cleanup_stack_args;

  if (argc < 1 || argc > 3 || argc == 2)
    error (_("mi_cmd_stack_list_args: Usage: PRINT_VALUES [FRAME_LOW FRAME_HIGH]"));

  if (argc == 3)
    {
      frame_low = atoi (argv[1]);
      frame_high = atoi (argv[2]);
    }
  else
    {
      /* Called with no arguments, it means we want args for the whole
         backtrace. */
      frame_low = -1;
      frame_high = -1;
    }

  values = mi_decode_print_values (argv[0]);
  if (values == PRINT_BAD_INPUT)
    error ("%s", print_values_bad_input_string);

  /* Let's position fi on the frame at which to start the
     display. Could be the innermost frame if the whole stack needs
     displaying, or if frame_low is 0. */
  for (i = 0, fi = get_current_frame ();
       fi && i < frame_low;
       i++, fi = get_prev_frame (fi));

  if (fi == NULL)
    error (_("mi_cmd_stack_list_args: Not enough frames in stack."));

  cleanup_stack_args = make_cleanup_ui_out_list_begin_end (uiout, "stack-args");

  /* Now let's print the frames up to frame_high, or until there are
     frames in the stack. */
  while (fi != NULL)
    {
      struct cleanup *cleanup_frame;
      QUIT;
      /* APPLE LOCAL: We need to store the frame id and then look the frame 
         info back up after our call to list_args_or_locals() in case that 
	 function calls any functions in the inferior in order to determine 
	 the dynamic type of a variable (which will cause flush_cached_frames() 
	 to be called resulting in our frame info chain being destroyed, 
	 leaving FI pointing to invalid memory.  */
      struct frame_id stack_frame_id = get_frame_id (fi);

      cleanup_frame = make_cleanup_ui_out_tuple_begin_end (uiout, "frame");
      ui_out_field_int (uiout, "level", i); 
      list_args_or_locals (0, values, fi, 0);
      do_cleanups (cleanup_frame);

      i++;
      if (i <= frame_high || frame_high == -1)
        {
	  /* APPLE LOCAL: Get our frame info again from the frame id.  */
	  fi = frame_find_by_id (stack_frame_id);
	  if (fi != NULL)
	    fi = get_prev_frame (fi);
	}
      else
	fi = NULL;
    }

  do_cleanups (cleanup_stack_args);
  if (i < frame_high)
    error (_("mi_cmd_stack_list_args: Not enough frames in stack."));

  return MI_CMD_DONE;
}

/* Print a list of the locals or the arguments for the currently
   selected frame.  If the argument passed is 0, printonly the names
   of the variables, if an argument of 1 is passed, print the values
   as well. If ALL_BLOCKS == 1, then print the symbols for ALL lexical
   blocks in the function that is in frame FI.*/

static void
list_args_or_locals (int locals, enum print_values values, 
                     struct frame_info *fi, int all_blocks)
{
  struct block *block = NULL;
  /* APPLE LOCAL begin address ranges  */
  struct block *containing_block = NULL;
  /* APPLE LOCAL end address ranges  */
  struct cleanup *cleanup_list;
  static struct ui_stream *stb = NULL;
  struct frame_id stack_frame_id = get_frame_id (fi);
  /* APPLE LOCAL radar 6404668 locals vs. inlined subroutines  */
  struct bfd_section *section;

  stb = ui_out_stream_new (uiout);
  
  cleanup_list = make_cleanup_ui_out_list_begin_end (uiout, locals ? "locals" : "args");

  if (all_blocks)
    {
      CORE_ADDR fstart;
      int index;
      int nblocks;
      struct blockvector *bv;
      
      /* CHECK - I assume that the function block in the innermost
	 lexical block that starts at the start function of the
	 PC.  If this is not correct, then I will have to run through
	 the blockvector to match it to the block I get by:
      */   
	 
      fstart = get_pc_function_start (get_frame_pc (fi));
      if (fstart == 0)
	{
	  /* Can't find the containing function for this PC.  Sigh... */
	  fstart = get_frame_pc (fi);
	}

      bv = blockvector_for_pc (fstart, &index);
      if (bv == NULL)
	{
          /* APPLE LOCAL: Don't error() out here - UIs will promiscuously ask
             for locals/args even in assembly language routines and there's no
             point in displaying an error message to the user.  */
          do_cleanups (cleanup_list);
          ui_out_stream_delete (stb);
          return;
	}
      nblocks = BLOCKVECTOR_NBLOCKS (bv);

      block = BLOCKVECTOR_BLOCK (bv, index);
      /* APPLE LOCAL begin address ranges  */
      containing_block = block;

      /* APPLE LOCAL radar 6404668 locals vs. inlined subroutines  */
      section = find_pc_mapped_section (fstart);

      while (contained_in (block, containing_block))
      /* APPLE LOCAL end address ranges  */
	{
	  /* APPLE LOCAL begin radar 6404668 locals vs. inlined subroutines  */
	  if (block == containing_block
	      || (block_inlined_function (block, section) == NULL))
	    print_syms_for_block (block, fi, stb, locals, 1, 
				  values, NULL);
	  /* APPLE LOCAL end radar 6404668 locals vs. inlined subroutines  */
	  index++;
          /* Re-fetch FI in case we ran the inferior and the frame cache
             was flushed.  */
          fi = frame_find_by_id (stack_frame_id);
          if (fi == NULL)
            error ("Could not rediscover frame while printing args or locals");
	  if (index == nblocks)
	    break;
	  block = BLOCKVECTOR_BLOCK (bv, index);
	}
    }
  else
    {
      block = get_frame_block (fi, 0);

      while (block != 0)
	{
	  print_syms_for_block (block, fi, stb, locals, 1, values, NULL);
          /* Re-fetch FI in case we ran the inferior and the frame cache
             was flushed.  */
          fi = frame_find_by_id (stack_frame_id);
          if (fi == NULL)
            error ("Could not rediscover frame while printing args or locals");

	  if (BLOCK_FUNCTION (block))
	    break;
	  else
	    block = BLOCK_SUPERBLOCK (block);
	}
    }

  do_cleanups (cleanup_list);
  ui_out_stream_delete (stb);
}

/* APPLE LOCAL begin -file-list-statics */
/* This implements the command -file-list-statics.  It takes three or four
   arguments, a filename, a shared library, and the standard PRINT_VALUES
   argument, and an optional filter.  
   It prints all the static variables in the given file, in
   the given shared library.  If the shared library name is empty, then it
   looks in all shared libraries for the file.

   If the file name is the special cookie *CURRENT FRAME* then it prints
   the statics for the currently selected frame.

   See mi_decode_print_values for how PRINT_VALUES works
   If the filter string is provided, only symbols that DON'T match
   the filter will be printed.
   */

#define CURRENT_FRAME_COOKIE "*CURRENT FRAME*"

static int
parse_statics_globals_args (char **argv, int argc, char **filename_ptr, char ** shlibname_ptr, 
			    enum print_values *values_ptr, struct re_pattern_buffer **filterp_ptr,
			    int *consts_ptr)
{
  char *filter_arg;
  int bad_args;

  filter_arg = NULL;
  *filename_ptr = NULL;
  *shlibname_ptr = NULL;
  bad_args = 0;
  
  if (argc > 2 && argc < 5)
    {
      *filename_ptr = argv[0];
      *shlibname_ptr = argv[1];
      *values_ptr = mi_decode_print_values (argv[2]);
      if (*values_ptr == PRINT_BAD_INPUT) 
	bad_args = 1;
      if (argc == 4) 
	filter_arg = argv[3];
      *consts_ptr = 0;
    }
  else if (argc == 5 || argc == 7 || argc == 9)
    {
      int got_values = 0;

      while (argc > 0) 
	{
	  int step = 2;
	  if (strcmp (argv[0], "-file") == 0)
	    *filename_ptr = argv[1];
	  else if (strcmp (argv[0], "-shlib") == 0)
	    *shlibname_ptr = argv[1];
	  else if (strcmp (argv[0], "-filter") == 0)
	    filter_arg = argv[1];
	  else if (strcmp (argv[0], "-constants") == 0)
	    {
	      if (strcmp (argv[1], "1") == 0)
		*consts_ptr = 1;
	      else if (strcmp (argv[1], "0") == 0)
		*consts_ptr = 0;
	      else
		{
		  bad_args = 1;
		  break;
		}
	    }
	  else
	    {
	      *values_ptr = mi_decode_print_values (argv[0]);
	      if (*values_ptr != PRINT_BAD_INPUT) 
		{
		  got_values = 1;
		  step = 1;
		}
	      else
		{
		  bad_args = 1;
		  break;
		}
	    }

	  argc -= step;
	  argv += step;
	}
      if (*filename_ptr == NULL || *shlibname_ptr == NULL || got_values == 0)
	bad_args = 1;
    }
  else
    bad_args = 1;

  if (bad_args)
    return 0;

  if (filter_arg != NULL)
    {
      const char *msg;

      msg = re_compile_pattern (filter_arg, strlen (filter_arg), &mi_symbol_filter);
      if (msg)
	error ("Error compiling regexp: \"%s\"", msg);
      *filterp_ptr = &mi_symbol_filter;
    }
  else
    *filterp_ptr = NULL;

  return 1;
}

enum mi_cmd_result
mi_cmd_file_list_statics (char *command, char **argv, int argc)
{
  enum print_values values;
  char *shlibname, *filename;
  struct block *block;
  struct partial_symtab *file_ps;
  struct symtab *file_symtab;
  struct cleanup *cleanup_list;
  struct ui_stream *stb;
  struct re_pattern_buffer *filterp = NULL;
  int consts = 1;


  if (!parse_statics_globals_args (argv, argc, &filename, &shlibname, &values, 
				   &filterp, &consts))
    error ("mi_cmd_file_list_statics: Usage: -file FILE -shlib SHLIB"
	   " VALUES"
	   " [-filter FILTER] [-constants 0/1]");

  /* APPLE LOCAL: Make sure we have symbols loaded for requested SHLIBNAME.  */
  if (*shlibname != '\0')
    if (objfile_name_set_load_state (shlibname, OBJF_SYM_ALL, 0) == -1)
      warning ("Couldn't raise load level state for requested shlib: \"%s\"",
                shlibname);
  
  if (strcmp (filename, CURRENT_FRAME_COOKIE) == 0)
    {
      CORE_ADDR pc;
      struct obj_section *objsec;

      pc = get_frame_pc (get_selected_frame (NULL));
      objsec = find_pc_section (pc);
      if (objsec != NULL && objsec->objfile != NULL)
        cleanup_list = make_cleanup_restrict_to_objfile (objsec->objfile);
      else
        cleanup_list = make_cleanup (null_cleanup, NULL);

      file_ps = find_pc_psymtab (pc);
      do_cleanups (cleanup_list);
    }
  else
    {
      if (*shlibname != '\0')
	{
	  cleanup_list = make_cleanup_restrict_to_shlib (shlibname);
	  if (cleanup_list == (void *) -1)
	    {
	      error ("mi_cmd_file_list_statics: Could not find shlib \"%s\".", shlibname);
	    }
	  
	}
      else
	cleanup_list = make_cleanup (null_cleanup, NULL);
      
      /* Probably better to not restrict the objfile search, while
	 doing the PSYMTAB to SYMTAB conversion to miss some types
	 that are defined outside the current shlib.  So get the
	 psymtab first, and then convert after cleaning up.  */

      if (*filename != '\0')
	{
	  file_ps = lookup_partial_symtab (filename);
	  
	  
	  /* FIXME: dbxread.c only uses the SECOND N_SO stab when making
	     psymtabs.  It discards the first one.  But that means that if
	     filename is an absolute path, it is likely
	     lookup_partial_symtab will fail.  If it did, try again with
	     the base name.  */
	  
	  if (file_ps == NULL)
	    if (lbasename(filename) != filename)
	      file_ps = lookup_partial_symtab (lbasename (filename));
	}
      else
	file_ps = NULL;
      
      do_cleanups (cleanup_list);

    }

  /* If the user passed us a real filename and we couldn't find it,
     that is an error.  But "" or current frame, could point to a file
     or objfile with no debug info.  In which case we should just
     return an empty list.  */

  if (file_ps == NULL)
    {
      if (filename[0] == '\0' || strcmp (filename, CURRENT_FRAME_COOKIE) == 0)
	{
	  cleanup_list = make_cleanup_ui_out_list_begin_end (uiout, "statics");
	  do_cleanups (cleanup_list);
	  return MI_CMD_DONE;
	}
      else
	error ("mi_cmd_file_list_statics: "
	       "Could not get symtab for file \"%s\".", filename);
    }
  
  file_symtab = PSYMTAB_TO_SYMTAB (file_ps);
  
  if (file_symtab == NULL)
    error ("Could not convert psymtab to symtab for file \"%s\"", filename);

  block = BLOCKVECTOR_BLOCK (file_symtab->blockvector, STATIC_BLOCK);
      
  stb = ui_out_stream_new (uiout);
  
  cleanup_list = make_cleanup_ui_out_list_begin_end (uiout, "statics");
  
  print_syms_for_block (block, NULL, stb, -1, consts, values, filterp);
  
  do_cleanups (cleanup_list);
  ui_out_stream_delete (stb);

  return MI_CMD_DONE;
}


static void
print_globals_for_symtab (struct symtab *file_symtab, 
			  struct ui_stream *stb,
			  enum print_values values,
			  int consts,
			  struct re_pattern_buffer *filter)
{
  struct block *block;
  struct cleanup *cleanup_list;
 
  block = BLOCKVECTOR_BLOCK (file_symtab->blockvector, GLOBAL_BLOCK);

  cleanup_list = make_cleanup_ui_out_list_begin_end (uiout, "globals");

  print_syms_for_block (block, NULL, stb, -1, consts, values, filter);

  do_cleanups (cleanup_list);
}

/* This implements the command -file-list-globals.  It takes three or
   four arguments, filename, a shared library, the standard
   PRINT_VALUES argument and an optional filter regexp.  It prints all
   the global variables in the given file, in the given shared
   library.  If the shared library name is empty, then it looks in all
   shared libraries for the file.  If the filename is empty, then it
   looks in all files in the given shared library.  If both are empty
   then it prints ALL globals.  
   The third argument is the standard print-values argument.
   Finally, if there are four arguments, the last is a regular expression,
   to filter OUT all varobj's matching the regexp.  */

enum mi_cmd_result
mi_cmd_file_list_globals (char *command, char **argv, int argc)
{
  enum print_values values;
  char *shlibname, *filename;
  struct partial_symtab *file_ps;
  struct symtab *file_symtab;
  struct ui_stream *stb;
  struct re_pattern_buffer *filterp;
  int consts = 1;

  if (!parse_statics_globals_args (argv, argc, &filename, &shlibname, &values, 
				   &filterp, &consts))
    error ("mi_cmd_file_list_globals: Usage: -file FILE -shlib SHLIB"
	   " VALUES"
	   " [-filter FILTER] [-constants 0/1]");
  
  stb = ui_out_stream_new (uiout);

  /* APPLE LOCAL: Make sure we have symbols loaded for requested SHLIBNAME.  */
  if (*shlibname != '\0')
    if (objfile_name_set_load_state (shlibname, OBJF_SYM_ALL, 0) == -1)
      warning ("Couldn't raise load level state for requested shlib: \"%s\"",
                shlibname);

  if (*filename != '\0')
    {
      struct cleanup *cleanup_list;

      if (*shlibname != '\0')
	{
	  cleanup_list = make_cleanup_restrict_to_shlib (shlibname);
	  if (cleanup_list == (void *) -1)
	    {
	      error ("mi_cmd_file_list_globals: "
		     "Could not find shlib \"%s\".", 
		     shlibname);
	    }
	  
	}
      else
	cleanup_list = make_cleanup (null_cleanup, NULL);

      /* Probably better to not restrict the objfile search, while
	 doing the PSYMTAB to SYMTAB conversion to miss some types
	 that are defined outside the current shlib.  So get the
	 psymtab first, and then convert after cleaning up.  */

      file_ps = lookup_partial_symtab (filename);
      
      
      if (file_ps == NULL)
	error ("mi_cmd_file_list_statics: "
	       "Could not get symtab for file \"%s\".", 
	       filename);
      
      do_cleanups (cleanup_list);
      
      file_symtab = PSYMTAB_TO_SYMTAB (file_ps);
      print_globals_for_symtab (file_symtab, stb, values, 
				consts, filterp);
    }
  else
    {
      if (*shlibname != '\0')
	{
	  struct objfile *ofile, *requested_ofile = NULL;
	  struct partial_symtab *ps;

	  ALL_OBJFILES (ofile)
	    {
	      if (objfile_matches_name (ofile, shlibname) != objfile_no_match)
		{
		  requested_ofile = ofile;
		  break;
		}
	    }
	  if (requested_ofile == NULL)
	    error ("mi_file_list_globals: "
		   "Couldn't find shared library \"%s\"\n", 
		   shlibname);

	  /* APPLE LOCAL: grab the dSYM file there is one.  */
	  requested_ofile = separate_debug_objfile (requested_ofile);

	  ALL_OBJFILE_PSYMTABS (requested_ofile, ps)
	    {
	      struct symtab *file_symtab;

	      file_symtab = PSYMTAB_TO_SYMTAB (ps);
	      if (!file_symtab)
		continue;
	      
	      
	      if (file_symtab->primary)
		{
		  struct cleanup *file_cleanup;
		  file_cleanup = 
		    make_cleanup_ui_out_list_begin_end (uiout, "file");
		  ui_out_field_string (uiout, "filename", 
				       file_symtab->filename);
		  print_globals_for_symtab (file_symtab, stb, values, 
					    consts, filterp);
		  do_cleanups (file_cleanup);
		}
	    }
	}
      else
	{
	  struct objfile *ofile;
	  struct partial_symtab *ps;

	  /* Okay, you want EVERYTHING...  */

	  ALL_OBJFILES (ofile)
	    {
	      struct cleanup *ofile_cleanup;

	      ofile_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "image");
	      if (ofile->name != NULL)
		ui_out_field_string (uiout, "imagename", ofile->name);
	      else
		ui_out_field_string (uiout, "imagename", "<unknown>");

	      ALL_OBJFILE_PSYMTABS (ofile, ps)
		{
		  struct symtab *file_symtab;
		  
		  file_symtab = PSYMTAB_TO_SYMTAB (ps);
		  if (!file_symtab)
		    continue;
		  
		  if (file_symtab->primary)
		    {
		      struct cleanup *file_cleanup;
		      file_cleanup = 
			make_cleanup_ui_out_list_begin_end (uiout, "file");
		      ui_out_field_string (uiout, "filename", 
					   file_symtab->filename);
		      print_globals_for_symtab (file_symtab, stb, 
						values, 
						consts, filterp);
		      do_cleanups (file_cleanup);
		    }
		}
	      do_cleanups (ofile_cleanup);
	    }
	}
    }
      
  ui_out_stream_delete (stb);

  return MI_CMD_DONE;

}

/* Print the variable symbols for block BLOCK.  VALUES is the
   print_values enum.

   LOCALS determines what scope of variables to print:
     1 - print locals AND statics.  
     0 - print args.  
     -1  - print statics. 
   CONSTS - whether to print const symbols.  Const pointers are
   always printed anyway.
   STB is the ui-stream to which the results are printed.  
   And FI, if non-null, is the frame to bind the varobj to.  
   If FILTER is non-null, then we only print expressions matching
   that compiled regexp.  */

static void
print_syms_for_block (struct block *block, 
		      struct frame_info *fi, 
		      struct ui_stream *stb,
		      int locals, 
		      int consts,
		      enum print_values values,
		      struct re_pattern_buffer *filter)
{
  int print_me;
  struct symbol *sym;
  struct dict_iterator iter;
  struct ui_stream *error_stb;
  struct cleanup *old_chain;
  struct frame_id stack_frame_id;
  if (fi)
    stack_frame_id = get_frame_id (fi);
  
  if (dict_empty (BLOCK_DICT (block)))
    return;

  error_stb = ui_out_stream_new (uiout);
  old_chain = make_cleanup_ui_out_stream_delete (error_stb);

  ALL_BLOCK_SYMBOLS (block, iter, sym)
    {
      print_me = 0;

      /* If this is a const, and we aren't printing consts, then skop this one. 
	 However, we always print const pointers, 'cause they are interesting even
	 if plain int/char/etc consts aren't.  */

      switch (SYMBOL_CLASS (sym))
	{
	default:
	case LOC_UNDEF:	/* catches errors        */
	case LOC_TYPEDEF:	/* local typedef         */
	case LOC_LABEL:	/* local label           */
	case LOC_BLOCK:	/* local function        */
	case LOC_CONST_BYTES:	/* loc. byte seq.        */
	case LOC_UNRESOLVED:	/* unresolved static     */
	case LOC_OPTIMIZED_OUT:	/* optimized out         */
	  print_me = 0;
	  break;
	case LOC_CONST:	/* constant              */
	  if (consts)
	    print_me = 1;
	  else
	    print_me = 0;
	  break;
	case LOC_ARG:	/* argument              */
	case LOC_REF_ARG:	/* reference arg         */
	case LOC_REGPARM:	/* register arg          */
	case LOC_REGPARM_ADDR:	/* indirect register arg */
	case LOC_LOCAL_ARG:	/* stack arg             */
	case LOC_BASEREG_ARG:	/* basereg arg           */
	case LOC_COMPUTED_ARG:
	  if (locals == 0)
	    print_me = 1;
	  break;
	  
	case LOC_STATIC:	/* static                */
	  if (locals == -1 || locals == 1)
	    print_me = 1;
	  break;
	case LOC_LOCAL:	/* stack local           */
	case LOC_BASEREG:	/* basereg local         */
	case LOC_REGISTER:	/* register              */
	case LOC_COMPUTED:
	  if (locals == 1)
	    print_me = 1;
	  break;
	}

      /* If we were asked not to print consts, make sure we don't.  */

      if (print_me 
	  && !consts && (SYMBOL_TYPE (sym) != NULL) 
	  && TYPE_CONST (check_typedef (SYMBOL_TYPE (sym)))
	  && (!(TYPE_CODE (check_typedef (SYMBOL_TYPE (sym))) == TYPE_CODE_PTR)))
	print_me = 0;
      
      if (print_me)
	{
          struct symbol *sym2;
	  int len = strlen (SYMBOL_NATURAL_NAME (sym));

	  /* If we are about to print, compare against the regexp.  */
	  if (filter && re_search (filter, SYMBOL_NATURAL_NAME (sym), 
				   len, 0, len, 
				   (struct re_registers *) 0) >= 0)
	    continue;

	  if (values == PRINT_NO_VALUES)
	    {
	      struct cleanup *tuple_cleanup;
	      tuple_cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
	      ui_out_field_string (uiout, "name", SYMBOL_NATURAL_NAME (sym));
	      do_cleanups (tuple_cleanup);
	      continue;
	    }

	  if (!locals)
	    sym2 = lookup_symbol (SYMBOL_NATURAL_NAME (sym),
				  block, VAR_DOMAIN,
				  (int *) NULL,
				  (struct symtab **) NULL);
	  else
	    sym2 = sym;
	  
	  if (values == PRINT_MAKE_VAROBJ)
	    {
	      /* APPLE LOCAL: If you pass an expression with a "::" in
		 it down to parse_expression, it will choke on it.  So
		 we need to add a ' before and after the
		 expression.  Only do it if there is a "::" however, just
	         to keep the uglification to a minimum.  */
	      struct varobj *new_var;
	      struct cleanup *tuple_cleanup, *expr_cleanup;
	      char *expr = SYMBOL_NATURAL_NAME (sym2);
	      if (strstr (expr, "::") != NULL) 
		{
		  char *tmp;
		  int len = strlen (expr);
		  tmp = xmalloc (len + 3);
		  tmp[0] = '\'';
		  memcpy (tmp + 1, expr, len);
		  tmp[len + 1] = '\'';
		  tmp[len + 2] = '\0';
		  expr = tmp;
		  expr_cleanup = make_cleanup (xfree, expr);
		}
	      else
		{
		  expr_cleanup = make_cleanup (null_cleanup, NULL);
		}

	      /* END APPLE LOCAL */
	      if (fi)
		new_var = varobj_create (varobj_gen_name (), 
				       expr,
				       get_frame_base (fi),
				       block,
				       USE_BLOCK_IN_FRAME);
	      else
		new_var = varobj_create (varobj_gen_name (), 
				       expr,
				       0,
				       block,
				       NO_FRAME_NEEDED);

	      do_cleanups (expr_cleanup);

	      /* FIXME: There should be a better way to report an error in 
		 creating a variable here, but I am not sure how to do it,
	         so I will just bag out for now. */

	      if (new_var == NULL)
		continue;

	      tuple_cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, "varobj");
	      ui_out_field_string (uiout, "exp", SYMBOL_NATURAL_NAME (sym));
	      if (new_var != NULL)
		{
		  char *value_str;
		  struct ui_file *save_stderr;
		  
		  /* If we are using the varobj's, then print
		     the value as the varobj would. */
		  
		  save_stderr = gdb_stderr;
		  gdb_stderr = error_stb->stream;
		  
		  if (gdb_varobj_get_value (new_var, &value_str))
		    {
		      ui_out_field_string (uiout, "value", value_str);
		    }
		  else
		    {
		      /* FIXME: can I get the error string & put it here? */
		      ui_out_field_stream (uiout, "value", 
					   error_stb);
		    }
		  gdb_stderr = save_stderr;

                  /* Re-fetch FI in case we ran the inferior and the frame cache
                     was flushed.  */
                  if (fi)
                    {
                      fi = frame_find_by_id (stack_frame_id);
                      if (fi == NULL)
                        error ("Could not rediscover frame when getting value");
                    }
		}
	      else
		ui_out_field_skip (uiout, "value");
	    
	      mi_report_var_creation (uiout, new_var);
	      do_cleanups (tuple_cleanup);
	    }
	  else
	    {
	      struct cleanup *cleanup_tuple = NULL;
	      struct type *type;

	      cleanup_tuple =
		make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
	      
	      ui_out_field_string (uiout, "name", SYMBOL_PRINT_NAME (sym));
	      
	      switch (values)
		{
		case PRINT_SIMPLE_VALUES:
		  type = check_typedef (sym2->type);
		  type_print (sym2->type, "", stb->stream, -1);
		  ui_out_field_stream (uiout, "type", stb);
		  if (TYPE_CODE (type) != TYPE_CODE_ARRAY
		      && TYPE_CODE (type) != TYPE_CODE_STRUCT
		      && TYPE_CODE (type) != TYPE_CODE_UNION)
		    {
		      print_variable_value (sym2, fi, stb->stream);
		      ui_out_field_stream (uiout, "value", stb);
		    }
		  do_cleanups (cleanup_tuple);
		  break;
		case PRINT_ALL_VALUES:
		  print_variable_value (sym2, fi, stb->stream);
		  ui_out_field_stream (uiout, "value", stb);
		  do_cleanups (cleanup_tuple);
		  break;
		default:
		  internal_error (__FILE__, __LINE__,
				  "Wrong print_values value for this branch.\n");
		}
              /* Re-fetch FI in case we ran the inferior and the frame cache
                 was flushed.  */
              if (fi) 
                {
                  fi = frame_find_by_id (stack_frame_id);
                  if (fi == NULL)
                    error ("Could not rediscover frame when printing value");
                }
	    }
	}
    }

  do_cleanups (old_chain);
}
/* APPLE LOCAL end mi */

enum mi_cmd_result
mi_cmd_stack_select_frame (char *command, char **argv, int argc)
{
  if (argc == 0 || argc > 1)
    error (_("mi_cmd_stack_select_frame: Usage: FRAME_SPEC"));

  select_frame_command (argv[0], 1 /* not used */ );
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_stack_info_frame (char *command, char **argv, int argc)
{
  if (argc > 0)
    error (_("mi_cmd_stack_info_frame: No arguments required"));
  
  print_frame_info (get_selected_frame (NULL), 1, LOC_AND_ADDRESS, 0);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_stack_check_threads (char *command, char **argv, int argc)
{
  enum check_which_threads which_threads;
  int stack_depth;
  char *endptr;
  int safe_p;
  int num_patterns;
  int i;
  struct cleanup *patterns_cleanup;
  regex_t *patterns;

  if (argc < 3)
    error ("mi_cmd_stack_check_threads: wrong number of arguments, "
	   "USAGE %s MODE STACK_DEPTH FUNCTION [FUNCTION...]",
	   command);

  if (strcmp (argv[0], "current") == 0)
    which_threads = CHECK_CURRENT_THREAD;
  else if (strcmp (argv[0], "all") == 0)
    which_threads = CHECK_ALL_THREADS;
  else if (strcmp (argv[0], "scheduler") == 0)
    which_threads = CHECK_SCHEDULER_VALUE;
  else
    error ("mi_cmd_stack_check_threads: Wrong value \"%s\" for MODE, "
	   "should be \"current\", \"all\" or \"scheduler\"", argv[0]);

  if (*argv[1] == '\0')
    error ("mi_cmd_stack_check_threads: Empty value for STACK_DEPTH");

  stack_depth = strtol (argv[1], &endptr, 0);
  if (*endptr != '\0')
    error ("mi_cmd_stack_check_threads: Junk at end of STACK_DEPTH: %s", endptr);
  if (stack_depth < 0)
    error ("mi_cmd_stack_check_threads: Negative values for STACK_DEPTH not allowed.");

  /* Allocate space for the patterns, then make them.  */
  num_patterns = argc - 2;
  argv += 2;

  patterns = (regex_t *) xcalloc (num_patterns, 
						   sizeof (regex_t));
  patterns_cleanup = make_cleanup (xfree, patterns);

  for (i = 0; i < num_patterns; i++)
    {
      int err_code;

      err_code = regcomp (patterns + i, argv[i], REG_EXTENDED|REG_NOSUB);
      make_cleanup ((make_cleanup_ftype *) regfree, patterns + i);

      if (err_code != 0)
	{
	  char err_str[512];
	  regerror (err_code, patterns + i, err_str, 512);
	  error ("Couldn't compile pattern %s, error: %s", argv[i], err_str);
	}
    }

  safe_p = check_safe_call (patterns, num_patterns, stack_depth, which_threads);
  ui_out_field_int (uiout, "safe", safe_p);
  do_cleanups (patterns_cleanup);

  return MI_CMD_DONE;
}

/* APPLE LOCAL begin hooks */
void 
mi_interp_stack_changed_hook (void)
{
  struct ui_out *saved_ui_out = uiout;
  struct cleanup *list_cleanup;
  uiout = interp_ui_out (mi_interp);

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "MI_HOOK_RESULT");
  ui_out_field_string (uiout, "HOOK_TYPE", "stack_changed");
  do_cleanups (list_cleanup);
  uiout = saved_ui_out;
}

void 
mi_interp_frame_changed_hook (int new_frame_number)
{
  struct ui_out *saved_ui_out = uiout;
  struct cleanup *list_cleanup;

  /* APPLE LOCAL: Don't report new_frame_number == -1, that is just the
     invalidate frame message, and there is not much the UI can do with 
     that.  */

  if (new_frame_number == -1)
    return;

  uiout = interp_ui_out (mi_interp);

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "MI_HOOK_RESULT");
  ui_out_field_string (uiout, "HOOK_TYPE", "frame_changed");
  ui_out_field_int (uiout, "frame", new_frame_number);
  do_cleanups (list_cleanup);
  uiout = saved_ui_out;

}

void
mi_interp_context_hook (int thread_id)
{
  struct ui_out *saved_ui_out = uiout;
  struct cleanup *list_cleanup;
  uiout = interp_ui_out (mi_interp);

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "MI_HOOK_RESULT");
  ui_out_field_string (uiout, "HOOK_TYPE", "thread_changed");
  ui_out_field_int (uiout, "thread", thread_id);
  do_cleanups (list_cleanup);
  uiout = saved_ui_out;
}
/* APPLE LOCAL end hooks */
