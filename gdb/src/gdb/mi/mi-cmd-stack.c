/* MI Command Set - stack commands.
   Copyright 2000, 2002 Free Software Foundation, Inc.
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

#include "defs.h"
#include "target.h"
#include "frame.h"
#include "value.h"
#include "mi-cmds.h"
#include "ui-out.h"
#include "varobj.h"
#include "wrapper.h"
#include "interpreter.h"
#include "symtab.h"
#include "symtab.h"

/* FIXME: these should go in some .h file but stack.c doesn't have a
   corresponding .h file. These wrappers will be obsolete anyway, once
   we pull the plug on the sanitization. */
extern void select_frame_command_wrapper (char *, int);

/* FIXME: There is no general mi header to put this kind of utility function.*/
extern void mi_report_var_creation (struct ui_out *uiout, struct varobj *var);

void mi_interp_stack_changed_hook (void);
void mi_interp_frame_changed_hook (int new_frame_number);
void mi_interp_context_hook (int thread_id);

/* This is the interpreter for the mi... */
extern struct gdb_interpreter *mi_interp;

/* Use this to print any extra info in the stack listing output that is
   not in the standard gdb printing */

void mi_print_frame_more_info (struct ui_out *uiout,
				struct symtab_and_line *sal,
				struct frame_info *fi);

static void list_args_or_locals (int locals, int values, 
				 struct frame_info *fi,
				 int all_blocks,
				 int create_varobj);

static void print_syms_for_block (struct block *block, 
				  struct frame_info *fi, 
				  struct ui_stream *stb,
				  int locals, 
				  int values,
				  int create_varobj);

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
  struct frame_info *fi;

  if (!target_has_stack)
    error ("mi_cmd_stack_list_frames: No stack.");

  if (argc > 2 || argc == 1)
    error ("mi_cmd_stack_list_frames: Usage: [FRAME_LOW FRAME_HIGH]");

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
    error ("mi_cmd_stack_list_frames: Not enough frames in stack.");

  ui_out_list_begin (uiout, "stack");

  /* Now let;s print the frames up to frame_high, or until there are
     frames in the stack. */
  for (;
       fi && (i <= frame_high || frame_high == -1);
       i++, fi = get_prev_frame (fi))
    {
      QUIT;
      /* level == i: always print the level 'i'
         source == LOC_AND_ADDRESS: print the location and the address 
         always, even for level 0.
         args == 0: don't print the arguments. */
      print_frame_info (fi /* frame info */ ,
			i /* level */ ,
			LOC_AND_ADDRESS /* source */ ,
			0 /* args */ );
    }

  ui_out_list_end (uiout);
  if (i < frame_high)
    error ("mi_cmd_stack_list_frames: Not enough frames in stack.");

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
  int i;
  struct frame_info *fi;

  if (!target_has_stack)
    error ("mi_cmd_stack_info_depth: No stack.");

  if (argc > 1)
    error ("mi_cmd_stack_info_depth: Usage: [MAX_DEPTH]");

  if (argc == 1)
    frame_high = atoi (argv[0]);
  else
    /* Called with no arguments, it means we want the real depth of
       the stack. */
    frame_high = -1;

#ifdef FAST_COUNT_STACK_DEPTH
  if (!FAST_COUNT_STACK_DEPTH (&i))
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

/* Print a list of the locals for the current frame. With argument of
   0, print only the names, with argument of 1 print also the
   values. */
enum mi_cmd_result
mi_cmd_stack_list_locals (char *command, char **argv, int argc)
{
  int values;
  int all_blocks;
  int create_varobj;

  if (argc < 1 || argc > 2)
    error ("mi_cmd_stack_list_locals: Usage: PRINT_VALUES [ALL_BLOCKS]");

  values = atoi (argv[0]);
  create_varobj = (values == 2);

  if (argc >= 2)
    all_blocks = atoi (argv[1]);
  else
    all_blocks = 0;

  list_args_or_locals (1, values, selected_frame, 
		       all_blocks, create_varobj);
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
  int values;
  int create_varobj;
  struct frame_info *fi;

  if (argc < 1 || argc > 3 || argc == 2)
    error ("mi_cmd_stack_list_args: Usage: PRINT_VALUES [FRAME_LOW FRAME_HIGH]");

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

  values = atoi (argv[0]);
  create_varobj = (values == 2);

  /* Let's position fi on the frame at which to start the
     display. Could be the innermost frame if the whole stack needs
     displaying, or if frame_low is 0. */
  for (i = 0, fi = get_current_frame ();
       fi && i < frame_low;
       i++, fi = get_prev_frame (fi));

  if (fi == NULL)
    error ("mi_cmd_stack_list_args: Not enough frames in stack.");

  ui_out_list_begin (uiout, "stack-args");

  /* Now let's print the frames up to frame_high, or until there are
     frames in the stack. */
  for (;
       fi && (i <= frame_high || frame_high == -1);
       i++, fi = get_prev_frame (fi))
    {
      QUIT;
      ui_out_tuple_begin (uiout, "frame");
      ui_out_field_int (uiout, "level", i); 
      list_args_or_locals (0, values, fi, 0, create_varobj);
      ui_out_tuple_end (uiout);
    }

  ui_out_list_end (uiout);
  if (i < frame_high)
    error ("mi_cmd_stack_list_args: Not enough frames in stack.");

  return MI_CMD_DONE;
}

/* Print a list of the locals or the arguments for the currently
   selected frame.  If the argument passed is 0, printonly the names
   of the variables, if an argument of 1 is passed, print the values
   as well. If ALL_BLOCKS == 1, then print the symbols for ALL lexical
   blocks in the function that is in frame FI.*/

static void
list_args_or_locals (int locals, int values, struct frame_info *fi, 
		     int all_blocks, int create_varobj)
{
  struct block *block = NULL;
  int i, nsyms;
  static struct ui_stream *stb = NULL;

  stb = ui_out_stream_new (uiout);
  
  ui_out_list_begin (uiout, locals ? "locals" : "args");

  if (all_blocks)
    {
      CORE_ADDR fstart;
      CORE_ADDR endaddr;
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
	  fstart = fi->pc;
	}

      bv = blockvector_for_pc (fstart, &index);
      if (bv == NULL)
	{
	  error ("list_args_or_locals: Couldn't find block vector for pc %ux.",
		 fstart);
	}
      nblocks = BLOCKVECTOR_NBLOCKS (bv);

      block = BLOCKVECTOR_BLOCK (bv, index);
      endaddr = BLOCK_END (block);

      while (BLOCK_END (block) <= endaddr)
	{
	  print_syms_for_block (block, fi, stb, locals, 
				values, create_varobj);
	  index++;
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
	  print_syms_for_block (block, fi, stb, locals, values, create_varobj);
	  
	  if (BLOCK_FUNCTION (block))
	    break;
	  else
	    block = BLOCK_SUPERBLOCK (block);
	}
    }

  ui_out_list_end (uiout);
  ui_out_stream_delete (stb);
}

static void
print_syms_for_block (struct block *block, 
		      struct frame_info *fi, 
		      struct ui_stream *stb,
		      int locals, 
		      int values,
		      int create_varobj)
{
  int nsyms;
  int print_me;
  struct symbol *sym;
  int i;
  struct ui_stream *error_stb;
  struct cleanup *old_chain;
  
  nsyms = BLOCK_NSYMS (block);

  if (nsyms == 0) 
    return;

  error_stb = ui_out_stream_new (uiout);
  old_chain = make_cleanup_ui_out_stream_delete (error_stb);

  ALL_BLOCK_SYMBOLS (block, i, sym)
    {
      print_me = 0;

      switch (SYMBOL_CLASS (sym))
	{
	default:
	case LOC_UNDEF:	/* catches errors        */
	case LOC_CONST:	/* constant              */
	case LOC_TYPEDEF:	/* local typedef         */
	case LOC_LABEL:	/* local label           */
	case LOC_BLOCK:	/* local function        */
	case LOC_CONST_BYTES:	/* loc. byte seq.        */
	case LOC_UNRESOLVED:	/* unresolved static     */
	case LOC_OPTIMIZED_OUT:	/* optimized out         */
	  print_me = 0;
	  break;

	case LOC_ARG:	/* argument              */
	case LOC_REF_ARG:	/* reference arg         */
	case LOC_REGPARM:	/* register arg          */
	case LOC_REGPARM_ADDR:	/* indirect register arg */
	case LOC_LOCAL_ARG:	/* stack arg             */
	case LOC_BASEREG_ARG:	/* basereg arg           */
	  if (!locals)
	    print_me = 1;
	  break;

	case LOC_LOCAL:	/* stack local           */
	case LOC_BASEREG:	/* basereg local         */
	case LOC_STATIC:	/* static                */
	case LOC_REGISTER:	/* register              */
	  if (locals)
	    print_me = 1;
	  break;
	}

      if (print_me)
	{
	  struct symbol *sym2;

	  if (!create_varobj && !values)
	    {
	      ui_out_list_begin (uiout, NULL);
	      ui_out_field_string (uiout, "name", SYMBOL_NAME (sym));
	      ui_out_list_end (uiout);
	      continue;
	    }

	  if (!locals)
	    sym2 = lookup_symbol (SYMBOL_NAME (sym),
				  block, VAR_NAMESPACE,
				  (int *) NULL,
				  (struct symtab **) NULL);
	  else
	    sym2 = sym;
	  
	  if (create_varobj)
	    {
	      struct varobj *new_var;
	      new_var = varobj_create (varobj_gen_name (), 
				       SYMBOL_NAME (sym2),
				       fi->frame,
				       block,
				       USE_BLOCK_IN_FRAME);

	      /* FIXME: There should be a better way to report an error in 
		 creating a variable here, but I am not sure how to do it,
	         so I will just bag out for now. */

	      if (new_var == NULL)
		continue;

	      ui_out_list_begin (uiout, "varobj");
	      ui_out_field_string (uiout, "exp", SYMBOL_NAME (sym));
	      if (values)
		{
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
		    }
		  else
		    ui_out_field_skip (uiout, "value");
		}
	      mi_report_var_creation (uiout, new_var);
	    }	  
	  else
	    {
	      ui_out_list_begin (uiout, NULL);
	      ui_out_field_string (uiout, "name", SYMBOL_NAME (sym));
	      print_variable_value (sym2, fi, stb->stream);
	      ui_out_field_stream (uiout, "value", stb);
	    }
	  ui_out_list_end (uiout);
	}      
    }

  do_cleanups (old_chain);
}

enum mi_cmd_result
mi_cmd_stack_select_frame (char *command, char **argv, int argc)
{
  if (!target_has_stack)
    error ("mi_cmd_stack_select_frame: No stack.");

  if (argc > 1)
    error ("mi_cmd_stack_select_frame: Usage: [FRAME_SPEC]");

  /* with no args, don't change frame */
  if (argc == 0)
    select_frame_command_wrapper (0, 1 /* not used */ );
  else
    select_frame_command_wrapper (argv[0], 1 /* not used */ );
  return MI_CMD_DONE;
}

void 
mi_interp_stack_changed_hook (void)
{
  struct ui_out *saved_ui_out = uiout;
  struct mi_out *tmp_mi_out;

  uiout = mi_interp->interpreter_out;

  ui_out_list_begin (uiout, "MI_HOOK_RESULT");
  ui_out_field_string (uiout, "HOOK_TYPE", "stack_changed");
  ui_out_list_end (uiout);
  uiout = saved_ui_out;
}

void 
mi_interp_frame_changed_hook (int new_frame_number)
{
  struct ui_out *saved_ui_out = uiout;
  struct mi_out *tmp_mi_out;

  uiout = mi_interp->interpreter_out;

  ui_out_list_begin (uiout, "MI_HOOK_RESULT");
  ui_out_field_string (uiout, "HOOK_TYPE", "frame_changed");
  ui_out_field_int (uiout, "frame", new_frame_number);
  ui_out_list_end (uiout);
  uiout = saved_ui_out;

}

void
mi_interp_context_hook (int thread_id)
{
  struct ui_out *saved_ui_out = uiout;
  struct mi_out *tmp_mi_out;

  uiout = mi_interp->interpreter_out;

  ui_out_list_begin (uiout, "MI_HOOK_RESULT");
  ui_out_field_string (uiout, "HOOK_TYPE", "thread_changed");
  ui_out_field_int (uiout, "thread", thread_id);
  ui_out_list_end (uiout);
  uiout = saved_ui_out;
}




