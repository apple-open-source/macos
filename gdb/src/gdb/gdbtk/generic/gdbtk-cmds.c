/* Tcl/Tk command definitions for gdbtk.
   Copyright 1994, 1995, 1996, 1997, 1998, 1999
   Free Software Foundation, Inc.

   Written by Stu Grossman <grossman@cygnus.com> of Cygnus Support.
   Substantially augmented by Martin Hunt, Keith Seitz & Jim Ingham of
   Cygnus Support.

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
#include "inferior.h"
#include "command.h"
#include "source.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "target.h"
#include "gdbcore.h"
#include "tracepoint.h"
#include "demangle.h"
#include "frame.h"
#include "tui/tui-file.h"

#include <sys/stat.h>

#include <tcl.h>
#include <tk.h>
#include <itcl.h>
#include <tix.h>
#include "guitcl.h"
#include "gdbtk.h"
#include "gdbtk-wrapper.h"

#include <signal.h>
#include <fcntl.h>
#include "top.h"
#include <sys/ioctl.h>
#include "gdb_string.h"
#include "dis-asm.h"
#include <stdio.h>
#include "gdbcmd.h"

#include "annotate.h"
#include <sys/time.h>

static void setup_architecture_data (void);
static int tracepoint_exists (char *args);

/* This structure filled in call_wrapper and passed to
   the wrapped call function.
   It stores the command pointer and arguments 
   run in the wrapper function. */

struct wrapped_call_args
  {
    Tcl_Interp *interp;
    Tcl_ObjCmdProc *func;
    int objc;
    Tcl_Obj *CONST * objv;
    int val;
  };

/* These two objects hold boolean true and false,
   and are shared by all the list objects that gdb_listfuncs
   returns. */

static Tcl_Obj *mangled, *not_mangled;

/* These two control how the GUI behaves when gdb is either tracing or loading.
   They are used in this file & gdbtk_hooks.c */

int No_Update = 0;
int load_in_progress = 0;

/*
 * This is used in the register fetching routines
 */

#ifndef INVALID_FLOAT
#define INVALID_FLOAT(x, y) (0 != 0)
#endif



/* This Structure is used in gdb_disassemble.
   We need a different sort of line table from the normal one cuz we can't
   depend upon implicit line-end pc's for lines to do the
   reordering in this function.  */

struct my_line_entry
  {
    int line;
    CORE_ADDR start_pc;
    CORE_ADDR end_pc;
  };

/* Use this to pass the Tcl Text widget command and the open file
   descriptor to the disassembly load command. */

struct disassembly_client_data {
  FILE *fp;
  int file_opened_p;
  int widget_line_no;
  Tcl_Interp *interp;
  char *widget;
  Tcl_Obj *result_obj[3];
  char *asm_argv[14];
  char *source_argv[7];
  char *map_arr;
  Tcl_DString src_to_line_prefix;
  Tcl_DString pc_to_line_prefix;
  Tcl_DString line_to_pc_prefix;
  Tcl_CmdInfo cmd;
};

/* This contains the previous values of the registers, since the last call to
   gdb_changed_register_list.  */

static char *old_regs;

/* These two lookup tables are used to translate the type & disposition fields
   of the breakpoint structure (respectively) into something gdbtk understands.
   They are also used in gdbtk-hooks.c */

char *bptypes[] =
{"none", "breakpoint", "hw breakpoint", "until",
 "finish", "watchpoint", "hw watchpoint",
 "read watchpoint", "acc watchpoint",
 "longjmp", "longjmp resume", "step resume",
 "sigtramp", "watchpoint scope",
 "call dummy", "shlib events", "catch load",
 "catch unload", "catch fork", "catch vfork",
 "catch exec", "catch catch", "catch throw"
};
char *bpdisp[] =
{"delete", "delstop", "disable", "donttouch"};

/*
 * These are routines we need from breakpoint.c.
 * at some point make these static in breakpoint.c and move GUI code there
 */

extern struct breakpoint *set_raw_breakpoint (struct symtab_and_line sal);
extern void set_breakpoint_count (int);
extern int breakpoint_count;

/* This variable determines where memory used for disassembly is read from.
 * See note in gdbtk.h for details.
 */
int disassemble_from_exec = -1;

extern int gdb_variable_init (Tcl_Interp * interp);

/*
 * Declarations for routines exported from this file
 */

int Gdbtk_Init (Tcl_Interp * interp);
int call_wrapper (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);

/*
 * Declarations for routines used only in this file.
 */

static int compare_lines (const PTR, const PTR);
static int comp_files (const void *, const void *);
static int gdb_actions_command (ClientData, Tcl_Interp *, int,
				Tcl_Obj * CONST objv[]);
static int gdb_changed_register_list (ClientData, Tcl_Interp *, int,
				      Tcl_Obj * CONST[]);
static int gdb_clear_file (ClientData, Tcl_Interp * interp, int,
			   Tcl_Obj * CONST[]);
static int gdb_cmd (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_confirm_quit (ClientData, Tcl_Interp *, int,
			     Tcl_Obj * CONST[]);
static int gdb_disassemble (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_eval (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_fetch_registers (ClientData, Tcl_Interp *, int,
				Tcl_Obj * CONST[]);
static int gdb_find_file_command (ClientData, Tcl_Interp *, int,
				  Tcl_Obj * CONST objv[]);
static int gdb_force_quit (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static struct symtab *full_lookup_symtab (char *file);
static int gdb_get_args_command (ClientData, Tcl_Interp *, int,
				 Tcl_Obj * CONST objv[]);
static int gdb_get_breakpoint_info (ClientData, Tcl_Interp *, int,
				    Tcl_Obj * CONST[]);
static int gdb_get_breakpoint_list (ClientData, Tcl_Interp *, int,
				    Tcl_Obj * CONST[]);
static int gdb_get_file_command (ClientData, Tcl_Interp *, int,
				 Tcl_Obj * CONST objv[]);
static int gdb_get_function_command (ClientData, Tcl_Interp *, int,
				     Tcl_Obj * CONST objv[]);
static int gdb_get_line_command (ClientData, Tcl_Interp *, int,
				 Tcl_Obj * CONST objv[]);
static int gdb_get_locals_command (ClientData, Tcl_Interp *, int,
				   Tcl_Obj * CONST objv[]);
static int gdb_get_mem (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_get_trace_frame_num (ClientData, Tcl_Interp *, int,
				    Tcl_Obj * CONST objv[]);
static int gdb_get_tracepoint_list (ClientData, Tcl_Interp *, int,
				    Tcl_Obj * CONST objv[]);
static int gdb_get_vars_command (ClientData, Tcl_Interp *, int,
				 Tcl_Obj * CONST objv[]);
static int gdb_immediate_command (ClientData, Tcl_Interp *, int,
				  Tcl_Obj * CONST[]);
static int gdb_listfiles (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_listfuncs (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_loadfile (ClientData, Tcl_Interp *, int,
			 Tcl_Obj * CONST objv[]);
static int gdb_load_disassembly (ClientData clientData, Tcl_Interp
				 * interp, int objc, Tcl_Obj * CONST objv[]);
static int gdb_load_info (ClientData, Tcl_Interp *, int,
			  Tcl_Obj * CONST objv[]);
static int gdb_loc (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_path_conv (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_prompt_command (ClientData, Tcl_Interp *, int,
			       Tcl_Obj * CONST objv[]);
static int gdb_regnames (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_restore_fputs (ClientData, Tcl_Interp *, int,
			      Tcl_Obj * CONST[]);
static int gdb_search (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST objv[]);
static int gdb_set_bp (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST objv[]);
static int gdb_set_bp_addr (ClientData, Tcl_Interp *, int,
			    Tcl_Obj * CONST objv[]);
static int gdb_find_bp_at_line (ClientData, Tcl_Interp *, int,
				Tcl_Obj * CONST objv[]);
static int gdb_find_bp_at_addr (ClientData, Tcl_Interp *, int,
				Tcl_Obj * CONST objv[]);
static int gdb_stop (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_target_has_execution_command (ClientData,
					     Tcl_Interp *, int,
					     Tcl_Obj * CONST[]);
static int gdb_trace_status (ClientData, Tcl_Interp *, int,
			     Tcl_Obj * CONST[]);
static int gdb_tracepoint_exists_command (ClientData, Tcl_Interp *,
					  int, Tcl_Obj * CONST objv[]);
static int gdb_get_tracepoint_info (ClientData, Tcl_Interp *, int,
				    Tcl_Obj * CONST objv[]);
static int gdbtk_dis_asm_read_memory (bfd_vma, bfd_byte *, unsigned int,
				      disassemble_info *);
static void gdbtk_load_source (ClientData clientData,
			       struct symtab *symtab,
			       int start_line, int end_line);
static CORE_ADDR gdbtk_load_asm (ClientData clientData, CORE_ADDR pc,
				 struct disassemble_info *di);
static void gdbtk_print_source (ClientData clientData,
				struct symtab *symtab,
				int start_line, int end_line);
static CORE_ADDR gdbtk_print_asm (ClientData clientData, CORE_ADDR pc,
				  struct disassemble_info *di);
static int gdb_disassemble_driver (CORE_ADDR low, CORE_ADDR high,
				   int mixed_source_and_assembly,
				   ClientData clientData,
				   void (*print_source_fn) (ClientData, struct
							    symtab *, int,
							    int),
				   CORE_ADDR (*print_asm_fn) (ClientData,
							      CORE_ADDR,
							      struct
							      disassemble_info
							      *));
static int get_pc_register (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_stack (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_selected_frame (ClientData clientData,
			       Tcl_Interp * interp, int argc,
			       Tcl_Obj * CONST objv[]);
static int gdb_selected_block (ClientData clientData,
			       Tcl_Interp * interp, int argc,
			       Tcl_Obj * CONST objv[]);
static int gdb_get_blocks (ClientData clientData,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[]);
static int gdb_block_vars (ClientData clientData,
			   Tcl_Interp * interp, int objc,
			   Tcl_Obj * CONST objv[]);
char *get_prompt (void);
static void get_register (int, void *);
static void get_register_name (int, void *);
static int map_arg_registers (int, Tcl_Obj * CONST[],
			      void (*)(int, void *), void *);
static int perror_with_name_wrapper (PTR args);
static void register_changed_p (int, void *);
static int wrapped_call (PTR opaque_args);
static void get_frame_name (Tcl_Interp * interp, Tcl_Obj * list,
			    struct frame_info *fi);
char *pc_function_name (CORE_ADDR pc);


/* Gdbtk_Init
 *    This loads all the Tcl commands into the Tcl interpreter.
 *
 * Arguments:
 *    interp - The interpreter into which to load the commands.
 *
 * Result:
 *     A standard Tcl result.
 */

int
Gdbtk_Init (interp)
     Tcl_Interp *interp;
{
  Tcl_CreateObjCommand (interp, "gdb_cmd", call_wrapper, gdb_cmd, NULL);
  Tcl_CreateObjCommand (interp, "gdb_immediate", call_wrapper,
			gdb_immediate_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_loc", call_wrapper, gdb_loc, NULL);
  Tcl_CreateObjCommand (interp, "gdb_path_conv", call_wrapper, gdb_path_conv,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_listfiles", call_wrapper, gdb_listfiles,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_listfuncs", call_wrapper, gdb_listfuncs,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_mem", call_wrapper, gdb_get_mem,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_stop", call_wrapper, gdb_stop, NULL);
  Tcl_CreateObjCommand (interp, "gdb_regnames", call_wrapper, gdb_regnames,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_restore_fputs", call_wrapper, gdb_restore_fputs,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_fetch_registers", call_wrapper,
			gdb_fetch_registers, NULL);
  Tcl_CreateObjCommand (interp, "gdb_changed_register_list", call_wrapper,
			gdb_changed_register_list, NULL);
  Tcl_CreateObjCommand (interp, "gdb_disassemble", call_wrapper,
			gdb_disassemble, NULL);
  Tcl_CreateObjCommand (interp, "gdb_eval", call_wrapper, gdb_eval, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_breakpoint_list", call_wrapper,
			gdb_get_breakpoint_list, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_breakpoint_info", call_wrapper,
			gdb_get_breakpoint_info, NULL);
  Tcl_CreateObjCommand (interp, "gdb_clear_file", call_wrapper,
			gdb_clear_file, NULL);
  Tcl_CreateObjCommand (interp, "gdb_confirm_quit", call_wrapper,
			gdb_confirm_quit, NULL);
  Tcl_CreateObjCommand (interp, "gdb_force_quit", call_wrapper,
			gdb_force_quit, NULL);
  Tcl_CreateObjCommand (interp, "gdb_target_has_execution",
			call_wrapper,
			gdb_target_has_execution_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_is_tracing",
			call_wrapper, gdb_trace_status,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_load_info", call_wrapper, gdb_load_info,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_locals", call_wrapper,
			gdb_get_locals_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_args", call_wrapper,
			gdb_get_args_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_function", call_wrapper,
			gdb_get_function_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_line", call_wrapper,
			gdb_get_line_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_file", call_wrapper,
			gdb_get_file_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_tracepoint_exists",
			call_wrapper, gdb_tracepoint_exists_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_tracepoint_info",
			call_wrapper, gdb_get_tracepoint_info, NULL);
  Tcl_CreateObjCommand (interp, "gdb_actions",
			call_wrapper, gdb_actions_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_prompt",
			call_wrapper, gdb_prompt_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_find_file",
			call_wrapper, gdb_find_file_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_tracepoint_list",
			call_wrapper, gdb_get_tracepoint_list, NULL);
  Tcl_CreateObjCommand (interp, "gdb_pc_reg", call_wrapper, get_pc_register,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_loadfile", call_wrapper, gdb_loadfile,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_load_disassembly", call_wrapper,
			gdb_load_disassembly,  NULL);
  Tcl_CreateObjCommand (gdbtk_interp, "gdb_search", call_wrapper,
			gdb_search, NULL);
  Tcl_CreateObjCommand (interp, "gdb_set_bp", call_wrapper, gdb_set_bp, NULL);
  Tcl_CreateObjCommand (interp, "gdb_set_bp_addr", call_wrapper,
			gdb_set_bp_addr, NULL);
  Tcl_CreateObjCommand (interp, "gdb_find_bp_at_line", call_wrapper,
			gdb_find_bp_at_line, NULL);
  Tcl_CreateObjCommand (interp, "gdb_find_bp_at_addr", call_wrapper,
			gdb_find_bp_at_addr, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_trace_frame_num",
			call_wrapper, gdb_get_trace_frame_num, NULL);
  Tcl_CreateObjCommand (interp, "gdb_stack", call_wrapper, gdb_stack, NULL);
  Tcl_CreateObjCommand (interp, "gdb_selected_frame", call_wrapper,
			gdb_selected_frame, NULL);
  Tcl_CreateObjCommand (interp, "gdb_selected_block", call_wrapper,
			gdb_selected_block, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_blocks", call_wrapper,
			gdb_get_blocks, NULL);
  Tcl_CreateObjCommand (interp, "gdb_block_variables", call_wrapper,
			gdb_block_vars, NULL);

  Tcl_LinkVar (interp, "gdb_selected_frame_level",
	       (char *) &selected_frame_level,
	       TCL_LINK_INT | TCL_LINK_READ_ONLY);

  /* gdb_context is used for debugging multiple threads or tasks */
  Tcl_LinkVar (interp, "gdb_context_id",
	       (char *) &gdb_context,
	       TCL_LINK_INT | TCL_LINK_READ_ONLY);

  /* Init variable interface... */
  if (gdb_variable_init (interp) != TCL_OK)
    return TCL_ERROR;
  
  /* Route GDB internal log messages and target output and through
     stderr instead of stdout.  FIXME: Should have a separate streams
     for handling these two types of output. */
  gdb_stdtarg = gdb_stderr;
  gdb_stdlog = gdb_stderr;

  /* Register/initialize any architecture specific data */
  setup_architecture_data ();
  register_gdbarch_swap (&old_regs, sizeof (old_regs), NULL);
  register_gdbarch_swap (NULL, 0, setup_architecture_data);

  /* Determine where to disassemble from */
  Tcl_LinkVar (gdbtk_interp, "disassemble-from-exec",
	       (char *) &disassemble_from_exec,
	       TCL_LINK_INT);

  Tcl_PkgProvide (interp, "Gdbtk", GDBTK_VERSION);
  return TCL_OK;
}

/* This routine acts as a top-level for all GDB code called by Tcl/Tk.  It
   handles cleanups, and uses catch_errors to trap calls to return_to_top_level
   (usually via error).
   This is necessary in order to prevent a longjmp out of the bowels of Tk,
   possibly leaving things in a bad state.  Since this routine can be called
   recursively, it needs to save and restore the contents of the result_ptr as
   necessary. */

int
call_wrapper (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct wrapped_call_args wrapped_args;
  gdbtk_result new_result, *old_result_ptr;

  old_result_ptr = result_ptr;
  result_ptr = &new_result;
  result_ptr->obj_ptr = Tcl_NewObj ();
  result_ptr->flags = GDBTK_TO_RESULT;

  wrapped_args.func = (Tcl_ObjCmdProc *) clientData;
  wrapped_args.interp = interp;
  wrapped_args.objc = objc;
  wrapped_args.objv = objv;
  wrapped_args.val = TCL_OK;

  if (!catch_errors (wrapped_call, &wrapped_args, "", RETURN_MASK_ALL))
    {

      wrapped_args.val = TCL_ERROR;	/* Flag an error for TCL */

      /* Make sure the timer interrupts are turned off.  */
      gdbtk_stop_timer ();

      gdb_flush (gdb_stderr);	/* Flush error output */
      gdb_flush (gdb_stdout);	/* Sometimes error output comes here as well */

      /* If we errored out here, and the results were going to the
         console, then gdbtk_fputs will have gathered the result into the
         result_ptr.  We also need to echo them out to the console here */

      gdb_flush (gdb_stderr);	/* Flush error output */
      gdb_flush (gdb_stdout);	/* Sometimes error output comes here as well */

      /* In case of an error, we may need to force the GUI into idle
         mode because gdbtk_call_command may have bombed out while in
         the command routine.  */

      running_now = 0;
      Tcl_Eval (interp, "gdbtk_tcl_idle");

    }

  /* do not suppress any errors -- a remote target could have errored */
  load_in_progress = 0;

  /*
   * Now copy the result over to the true Tcl result.  If GDBTK_TO_RESULT flag
   * bit is set , this just copies a null object over to the Tcl result, 
   * which is fine because we should reset the result in this case anyway.
   */
  if (result_ptr->flags & GDBTK_IN_TCL_RESULT)
    {
      Tcl_DecrRefCount (result_ptr->obj_ptr);
    }
  else
    {
      Tcl_SetObjResult (interp, result_ptr->obj_ptr);
    }

  result_ptr = old_result_ptr;

#ifdef _WIN32
  close_bfds ();
#endif

  return wrapped_args.val;
}

/*
 * This is the wrapper that is passed to catch_errors.
 */

static int
wrapped_call (opaque_args)
     PTR opaque_args;
{
  struct wrapped_call_args *args = (struct wrapped_call_args *) opaque_args;
  args->val = (*args->func) (args->func, args->interp, args->objc, args->objv);
  return 1;
}

/* This is a convenience function to sprintf something(s) into a
 * new element in a Tcl list object.
 */

static void
sprintf_append_element_to_obj (Tcl_Obj * objp, char *format,...)
{
  va_list args;
  char buf[1024];

  va_start (args, format);

  vsprintf (buf, format, args);

  Tcl_ListObjAppendElement (NULL, objp, Tcl_NewStringObj (buf, -1));
}

/*
 * This section contains the commands that control execution.
 */

/* This implements the tcl command gdb_clear_file.

 * Prepare to accept a new executable file.  This is called when we
 * want to clear away everything we know about the old file, without
 * asking the user.  The Tcl code will have already asked the user if
 * necessary.  After this is called, we should be able to run the
 * `file' command without getting any questions.  
 *
 * Arguments:
 *    None
 * Tcl Result:
 *    None
 */

static int
gdb_clear_file (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  if (objc != 1)
    Tcl_SetStringObj (result_ptr->obj_ptr,
		      "Wrong number of args, none are allowed.", -1);

  if (inferior_pid != 0 && target_has_execution)
    {
      if (attach_flag)
	target_detach (NULL, 0);
      else
	target_kill ();
    }

  if (target_has_execution)
    pop_target ();

  delete_command (NULL, 0);
  exec_file_command (NULL, 0);
  symbol_file_command (NULL, 0);

  return TCL_OK;
}

/* This implements the tcl command gdb_confirm_quit
 * Ask the user to confirm an exit request.
 *
 * Arguments:
 *    None
 * Tcl Result:
 *    A boolean, 1 if the user answered yes, 0 if no.
 */

static int
gdb_confirm_quit (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int ret;

  if (objc != 1)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			"Wrong number of args, should be none.", -1);
      return TCL_ERROR;
    }

  ret = quit_confirm ();
  Tcl_SetBooleanObj (result_ptr->obj_ptr, ret);
  return TCL_OK;
}

/* This implements the tcl command gdb_force_quit
 * Quit without asking for confirmation.
 *
 * Arguments:
 *    None
 * Tcl Result:
 *    None
 */

static int
gdb_force_quit (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  if (objc != 1)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			"Wrong number of args, should be none.", -1);
      return TCL_ERROR;
    }

  quit_force ((char *) NULL, 1);
  return TCL_OK;
}

/* Pressing the stop button on the source window should attempt to
 * stop the target. If, after some short time, this fails, a dialog
 * should appear allowing the user to detach.
 *
 * The global GDBTK_FORCE_DETACH is set when we wish to detach
 * from a target. This value is returned by ui_loop_hook (x_event),
 * indicating to callers that they should detach.
 *
 * Read the comments before x_event to find out how we (try) to keep
 * gdbtk alive while some other event loop has stolen control from us.
 */

/*
 * This command implements the tcl command gdb_stop, which
 * is used to either stop the target or detach.
 * Note that it is assumed that a simulator or native target
 * can ALWAYS be stopped. Doing a "detach" on them has no effect.
 * 
 * Arguments:
 *    None or "detach"
 * Tcl Result:
 *    None
 */

static int
gdb_stop (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int force = 0;
  char *s;

  if (objc > 1)
    {
      s = Tcl_GetStringFromObj (objv[1], NULL);
      if (STREQ (s, "detach"))
	force = 1;
    }

  if (force)
    {
      /* Set the "forcibly detach from target" flag. x_event will
         return this value to callers when they should forcibly detach. */
      gdbtk_force_detach = 1;
    }
  else
    {
      if (target_stop != target_ignore)
	target_stop ();
      else
	quit_flag = 1;		/* hope something sees this */
    }

  return TCL_OK;
}


/*
 * This section contains Tcl commands that are wrappers for invoking
 * the GDB command interpreter.
 */


/* This implements the tcl command `gdb_eval'.
 * It uses the gdb evaluator to return the value of
 * an expression in the current language
 *
 * Tcl Arguments:
 *     expression - the expression to evaluate.
 * Tcl Result:
 *     The result of the evaluation.
 */

static int
gdb_eval (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct expression *expr;
  struct cleanup *old_chain = NULL;
  value_ptr val;

  if (objc != 2)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
		     "wrong # args, should be \"gdb_eval expression\"", -1);
      return TCL_ERROR;
    }

  expr = parse_expression (Tcl_GetStringFromObj (objv[1], NULL));

  old_chain = make_cleanup (free_current_contents, &expr);

  val = evaluate_expression (expr);

  /*
   * Print the result of the expression evaluation.  This will go to
   * eventually go to gdbtk_fputs, and from there be collected into
   * the Tcl result.
   */

  val_print (VALUE_TYPE (val), VALUE_CONTENTS (val),
	     VALUE_EMBEDDED_OFFSET (val), VALUE_ADDRESS (val),
	     gdb_stdout, 0, 0, 0, 0);

  do_cleanups (old_chain);

  return TCL_OK;
}

/* This implements the tcl command "gdb_cmd".

 * It sends its argument to the GDB command scanner for execution. 
 * This command will never cause the update, idle and busy hooks to be called
 * within the GUI.
 * 
 * Tcl Arguments:
 *    command - The GDB command to execute
 *    from_tty - 1 indicates this comes to the console.
 *               Pass this to the gdb command.
 * Tcl Result:
 *    The output from the gdb command (except for the "load" & "while"
 *    which dump their output to the console.
 */

static int
gdb_cmd (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int from_tty = 0;

  if (objc < 2)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "wrong # args", -1);
      return TCL_ERROR;
    }

  if (objc == 3)
    {
      if (Tcl_GetBooleanFromObj (NULL, objv[2], &from_tty) != TCL_OK)
	{
	  Tcl_SetStringObj (result_ptr->obj_ptr, "from_tty must be a boolean.",
			    -1);
	  return TCL_ERROR;
	}
    }

  if (running_now || load_in_progress)
    return TCL_OK;

  No_Update = 1;

  /* for the load instruction (and possibly others later) we
     set turn off the GDBTK_TO_RESULT flag bit so gdbtk_fputs() 
     will not buffer all the data until the command is finished. */

  if ((strncmp ("load ", Tcl_GetStringFromObj (objv[1], NULL), 5) == 0))
    {
      result_ptr->flags &= ~GDBTK_TO_RESULT;
      load_in_progress = 1;
    }

  execute_command (Tcl_GetStringFromObj (objv[1], NULL), from_tty);

  if (load_in_progress)
    {
      load_in_progress = 0;
      result_ptr->flags |= GDBTK_TO_RESULT;
    }

  bpstat_do_actions (&stop_bpstat);

  return TCL_OK;
}

/*
 * This implements the tcl command "gdb_immediate"
 *  
 * It does exactly the same thing as gdb_cmd, except NONE of its outut 
 * is buffered.  This will also ALWAYS cause the busy, update, and idle 
 * hooks to be called, contrasted with gdb_cmd, which NEVER calls them.
 * It turns off the GDBTK_TO_RESULT flag, which diverts the result
 * to the console window.
 *
 * Tcl Arguments:
 *    command - The GDB command to execute
 *    from_tty - 1 to indicate this is from the console.
 * Tcl Result:
 *    None.
 */

static int
gdb_immediate_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{

  int from_tty = 0;

  if (objc < 2)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "wrong # args", -1);
      return TCL_ERROR;
    }

  if (objc == 3)
    {
      if (Tcl_GetBooleanFromObj (NULL, objv[2], &from_tty) != TCL_OK)
	{
	  Tcl_SetStringObj (result_ptr->obj_ptr, "from_tty must be a boolean.",
			    -1);
	  return TCL_ERROR;
	}
    }

  if (running_now || load_in_progress)
    return TCL_OK;

  No_Update = 0;

  result_ptr->flags &= ~GDBTK_TO_RESULT;

  execute_command (Tcl_GetStringFromObj (objv[1], NULL), from_tty);

  bpstat_do_actions (&stop_bpstat);

  result_ptr->flags |= GDBTK_TO_RESULT;

  return TCL_OK;
}

/* This implements the tcl command "gdb_prompt"

 * It returns the gdb interpreter's prompt.
 *
 * Tcl Arguments:
 *    None.
 * Tcl Result:
 *    The prompt.
 */

static int
gdb_prompt_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  Tcl_SetStringObj (result_ptr->obj_ptr, get_prompt (), -1);
  return TCL_OK;
}


/*
 * This section contains general informational commands.
 */

/* This implements the tcl command "gdb_target_has_execution"

 * Tells whether the target is executing.
 *
 * Tcl Arguments:
 *    None
 * Tcl Result:
 *    A boolean indicating whether the target is executing.
 */

static int
gdb_target_has_execution_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int result = 0;

  if (target_has_execution && inferior_pid != 0)
    result = 1;

  Tcl_SetBooleanObj (result_ptr->obj_ptr, result);
  return TCL_OK;
}

/* This implements the tcl command "gdb_load_info"

 * It returns information about the file about to be downloaded.
 *
 * Tcl Arguments:
 *    filename: The file to open & get the info on.
 * Tcl Result:
 *    A list consisting of the name and size of each section.
 */

static int
gdb_load_info (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  bfd *loadfile_bfd;
  struct cleanup *old_cleanups;
  asection *s;
  Tcl_Obj *ob[2];

  char *filename = Tcl_GetStringFromObj (objv[1], NULL);

  loadfile_bfd = bfd_openr (filename, gnutarget);
  if (loadfile_bfd == NULL)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "Open failed", -1);
      return TCL_ERROR;
    }
  old_cleanups = make_cleanup_bfd_close (loadfile_bfd);

  if (!bfd_check_format (loadfile_bfd, bfd_object))
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "Bad Object File", -1);
      return TCL_ERROR;
    }

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);

  for (s = loadfile_bfd->sections; s; s = s->next)
    {
      if (s->flags & SEC_LOAD)
	{
	  bfd_size_type size = bfd_get_section_size_before_reloc (s);
	  if (size > 0)
	    {
	      ob[0] = Tcl_NewStringObj ((char *)
					bfd_get_section_name (loadfile_bfd, s),
					-1);
	      ob[1] = Tcl_NewLongObj ((long) size);
	      Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
					Tcl_NewListObj (2, ob));
	    }
	}
    }

  do_cleanups (old_cleanups);
  return TCL_OK;
}


/* gdb_get_locals -
 * This and gdb_get_locals just call gdb_get_vars_command with the right
 * value of clientData.  We can't use the client data in the definition
 * of the command, because the call wrapper uses this instead...
 */

static int
gdb_get_locals_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{

  return gdb_get_vars_command ((ClientData) 0, interp, objc, objv);

}

static int
gdb_get_args_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{

  return gdb_get_vars_command ((ClientData) 1, interp, objc, objv);

}

/* This implements the tcl commands "gdb_get_locals" and "gdb_get_args"

 * This function sets the Tcl interpreter's result to a list of variable names
 * depending on clientData. If clientData is one, the result is a list of
 * arguments; zero returns a list of locals -- all relative to the block
 * specified as an argument to the command. Valid commands include
 * anything decode_line_1 can handle (like "main.c:2", "*0x02020202",
 * and "main").
 *
 * Tcl Arguments:
 *   linespec - the linespec defining the scope of the lookup. Empty string
 *              to use the current block in the innermost frame.
 * Tcl Result:
 *   A list of the locals or args
 */

static int
gdb_get_vars_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct symtabs_and_lines sals;
  struct symbol *sym;
  struct block *block;
  char **canonical, *args;
  int i, nsyms, arguments;

  if (objc > 2)
    {
      Tcl_AppendStringsToObj (result_ptr->obj_ptr,
			      "wrong # of args: should be \"",
			      Tcl_GetStringFromObj (objv[0], NULL),
			    " [function:line|function|line|*addr]\"", NULL);
      return TCL_ERROR;
    }

  arguments = (int) clientData;

  /* Initialize the result pointer to an empty list. */

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);

  if (objc == 2)
    {
      args = Tcl_GetStringFromObj (objv[1], NULL);
      sals = decode_line_1 (&args, 1, NULL, 0, &canonical);
      if (sals.nelts == 0)
	{
	  Tcl_SetStringObj (result_ptr->obj_ptr,
			    "error decoding line", -1);
	  return TCL_ERROR;
	}

      /* Resolve all line numbers to PC's */
      for (i = 0; i < sals.nelts; i++)
	resolve_sal_pc (&sals.sals[i]);

      block = block_for_pc (sals.sals[0].pc);
    }
  else
    {
      /* Specified currently selected frame */
      if (selected_frame == NULL)
	return TCL_OK;

      block = get_frame_block (selected_frame);
    }

  while (block != 0)
    {
      nsyms = BLOCK_NSYMS (block);
      for (i = 0; i < nsyms; i++)
	{
	  sym = BLOCK_SYM (block, i);
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
	      break;
	    case LOC_ARG:	/* argument              */
	    case LOC_REF_ARG:	/* reference arg         */
	    case LOC_REGPARM:	/* register arg          */
	    case LOC_REGPARM_ADDR:	/* indirect register arg */
	    case LOC_LOCAL_ARG:	/* stack arg             */
	    case LOC_BASEREG_ARG:	/* basereg arg           */
	      if (arguments)
		Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
				  Tcl_NewStringObj (SYMBOL_NAME (sym), -1));
	      break;
	    case LOC_LOCAL:	/* stack local           */
	    case LOC_BASEREG:	/* basereg local         */
	    case LOC_STATIC:	/* static                */
	    case LOC_REGISTER:	/* register              */
	      if (!arguments)
		Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
				  Tcl_NewStringObj (SYMBOL_NAME (sym), -1));
	      break;
	    }
	}
      if (BLOCK_FUNCTION (block))
	break;
      else
	block = BLOCK_SUPERBLOCK (block);
    }

  return TCL_OK;
}

/* This implements the tcl command "gdb_get_line"

 * It returns the linenumber for a given linespec.  It will take any spec
 * that can be passed to decode_line_1
 *
 * Tcl Arguments:
 *    linespec - the line specification
 * Tcl Result:
 *    The line number for that spec.
 */
static int
gdb_get_line_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct symtabs_and_lines sals;
  char *args, **canonical;

  if (objc != 2)
    {
      Tcl_AppendStringsToObj (result_ptr->obj_ptr,
			      "wrong # of args: should be \"",
			      Tcl_GetStringFromObj (objv[0], NULL),
			      " linespec\"", NULL);
      return TCL_ERROR;
    }

  args = Tcl_GetStringFromObj (objv[1], NULL);
  sals = decode_line_1 (&args, 1, NULL, 0, &canonical);
  if (sals.nelts == 1)
    {
      Tcl_SetIntObj (result_ptr->obj_ptr, sals.sals[0].line);
      return TCL_OK;
    }

  Tcl_SetStringObj (result_ptr->obj_ptr, "N/A", -1);
  return TCL_OK;

}

/* This implements the tcl command "gdb_get_file"

 * It returns the file containing a given line spec.
 *
 * Tcl Arguments:
 *    linespec - The linespec to look up
 * Tcl Result:
 *    The file containing it.
 */

static int
gdb_get_file_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct symtabs_and_lines sals;
  char *args, **canonical;

  if (objc != 2)
    {
      Tcl_AppendStringsToObj (result_ptr->obj_ptr,
			      "wrong # of args: should be \"",
			      Tcl_GetStringFromObj (objv[0], NULL),
			      " linespec\"", NULL);
      return TCL_ERROR;
    }

  args = Tcl_GetStringFromObj (objv[1], NULL);
  sals = decode_line_1 (&args, 1, NULL, 0, &canonical);
  if (sals.nelts == 1)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			sals.sals[0].symtab->filename, -1);
      return TCL_OK;
    }

  Tcl_SetStringObj (result_ptr->obj_ptr, "N/A", -1);
  return TCL_OK;
}

/* This implements the tcl command "gdb_get_function"

 * It finds the function containing the given line spec.
 *
 * Tcl Arguments:
 *    linespec - The line specification
 * Tcl Result:
 *    The function that contains it, or "N/A" if it is not in a function.
 */
static int
gdb_get_function_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  char *function;
  struct symtabs_and_lines sals;
  char *args, **canonical;

  if (objc != 2)
    {
      Tcl_AppendStringsToObj (result_ptr->obj_ptr,
			      "wrong # of args: should be \"",
			      Tcl_GetStringFromObj (objv[0], NULL),
			      " linespec\"", NULL);
      return TCL_ERROR;
    }

  args = Tcl_GetStringFromObj (objv[1], NULL);
  sals = decode_line_1 (&args, 1, NULL, 0, &canonical);
  if (sals.nelts == 1)
    {
      resolve_sal_pc (&sals.sals[0]);
      function = pc_function_name (sals.sals[0].pc);
      Tcl_SetStringObj (result_ptr->obj_ptr, function, -1);
      return TCL_OK;
    }

  Tcl_SetStringObj (result_ptr->obj_ptr, "N/A", -1);
  return TCL_OK;
}

/* This implements the tcl command "gdb_find_file"

 * It searches the symbol tables to get the full pathname to a file.
 *
 * Tcl Arguments:
 *    filename: the file name to search for.
 * Tcl Result:
 *    The full path to the file, or an empty string if the file is not
 *    found.
 */

static int
gdb_find_file_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  char *filename = NULL;
  struct symtab *st;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename");
      return TCL_ERROR;
    }

  st = full_lookup_symtab (Tcl_GetStringFromObj (objv[1], NULL));
  if (st)
    filename = st->fullname;

  if (filename == NULL)
    Tcl_SetStringObj (result_ptr->obj_ptr, "", 0);
  else
    Tcl_SetStringObj (result_ptr->obj_ptr, filename, -1);

  return TCL_OK;
}

/* This implements the tcl command "gdb_listfiles"

 * This lists all the files in the current executible.
 *
 * Note that this currently pulls in all sorts of filenames
 * that aren't really part of the executable.  It would be
 * best if we could check each file to see if it actually
 * contains executable lines of code, but we can't do that
 * with psymtabs.
 *
 * Arguments:
 *    ?pathname? - If provided, only files which match pathname
 *        (up to strlen(pathname)) are included. THIS DOES NOT
 *        CURRENTLY WORK BECAUSE PARTIAL_SYMTABS DON'T SUPPLY
 *        THE FULL PATHNAME!!!
 *
 * Tcl Result:
 *    A list of all matching files.
 */
static int
gdb_listfiles (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct objfile *objfile;
  struct partial_symtab *psymtab;
  struct symtab *symtab;
  char *lastfile, *pathname = NULL, **files;
  int files_size;
  int i, numfiles = 0, len = 0;

  files_size = 1000;
  files = (char **) xmalloc (sizeof (char *) * files_size);

  if (objc > 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "Usage: gdb_listfiles ?pathname?");
      return TCL_ERROR;
    }
  else if (objc == 2)
    pathname = Tcl_GetStringFromObj (objv[1], &len);

  ALL_PSYMTABS (objfile, psymtab)
  {
    if (numfiles == files_size)
      {
	files_size = files_size * 2;
	files = (char **) xrealloc (files, sizeof (char *) * files_size);
      }
    if (psymtab->filename)
      {
	if (!len || !strncmp (pathname, psymtab->filename, len)
	    || !strcmp (psymtab->filename, basename (psymtab->filename)))
	  {
	    files[numfiles++] = basename (psymtab->filename);
	  }
      }
  }

  ALL_SYMTABS (objfile, symtab)
  {
    if (numfiles == files_size)
      {
	files_size = files_size * 2;
	files = (char **) xrealloc (files, sizeof (char *) * files_size);
      }
    if (symtab->filename && symtab->linetable && symtab->linetable->nitems)
      {
	if (!len || !strncmp (pathname, symtab->filename, len)
	    || !strcmp (symtab->filename, basename (symtab->filename)))
	  {
	    files[numfiles++] = basename (symtab->filename);
	  }
      }
  }

  qsort (files, numfiles, sizeof (char *), comp_files);

  lastfile = "";

  /* Discard the old result pointer, in case it has accumulated anything
     and set it to a new list object */

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);

  for (i = 0; i < numfiles; i++)
    {
      if (strcmp (files[i], lastfile))
	Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
				  Tcl_NewStringObj (files[i], -1));
      lastfile = files[i];
    }

  free (files);
  return TCL_OK;
}

static int
comp_files (file1, file2)
     const void *file1, *file2;
{
  return strcmp (*(char **) file1, *(char **) file2);
}


/* This implements the tcl command "gdb_search"


 * Tcl Arguments:
 *    option - One of "functions", "variables" or "types"
 *    regexp - The regular expression to look for.
 * Then, optionally:
 *    -files fileList
 *    -static 1/0
 *    -filename 1/0
 * Tcl Result:
 *    A list of all the matches found.  Optionally, if -filename is set to 1,
 *    then the output is a list of two element lists, with the symbol first,
 *    and the file in which it is found second.
 */

static int
gdb_search (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct symbol_search *ss = NULL;
  struct symbol_search *p;
  struct cleanup *old_chain = NULL;
  Tcl_Obj *CONST * switch_objv;
  int index, switch_objc, i, show_files = 0;
  namespace_enum space = 0;
  char *regexp;
  int static_only, nfiles;
  Tcl_Obj **file_list;
  char **files;
  static char *search_options[] =
  {"functions", "variables", "types", (char *) NULL};
  static char *switches[] =
  {"-files", "-filename", "-static", (char *) NULL};
  enum search_opts
    {
      SEARCH_FUNCTIONS, SEARCH_VARIABLES, SEARCH_TYPES
    };
  enum switches_opts
    {
      SWITCH_FILES, SWITCH_FILENAME, SWITCH_STATIC_ONLY
    };

  if (objc < 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "option regexp ?arg ...?");
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  if (Tcl_GetIndexFromObj (interp, objv[1], search_options, "option", 0,
			   &index) != TCL_OK)
    {
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  /* Unfortunately, we cannot teach search_symbols to search on
     multiple regexps, so we have to do a two-tier search for
     any searches which choose to narrow the playing field. */
  switch ((enum search_opts) index)
    {
    case SEARCH_FUNCTIONS:
      space = FUNCTIONS_NAMESPACE;
      break;
    case SEARCH_VARIABLES:
      space = VARIABLES_NAMESPACE;
      break;
    case SEARCH_TYPES:
      space = TYPES_NAMESPACE;
      break;
    }

  regexp = Tcl_GetStringFromObj (objv[2], NULL);
  /* Process any switches that refine the search */
  switch_objc = objc - 3;
  switch_objv = objv + 3;

  static_only = 0;
  nfiles = 0;
  files = (char **) NULL;
  while (switch_objc > 0)
    {
      if (Tcl_GetIndexFromObj (interp, switch_objv[0], switches,
			       "option", 0, &index) != TCL_OK)
	{
	  result_ptr->flags |= GDBTK_IN_TCL_RESULT;
	  return TCL_ERROR;
	}

      switch ((enum switches_opts) index)
	{
	case SWITCH_FILENAME:
	  {
	    if (switch_objc < 2)
	      {
		Tcl_WrongNumArgs (interp, 3, objv,
				  "?-files fileList  -filename 1|0 -static 1|0?");
		result_ptr->flags |= GDBTK_IN_TCL_RESULT;
		return TCL_ERROR;
	      }
	    if (Tcl_GetBooleanFromObj (interp, switch_objv[1], &show_files)
		!= TCL_OK)
	      {
		result_ptr->flags |= GDBTK_IN_TCL_RESULT;
		return TCL_ERROR;
	      }
	    switch_objc--;
	    switch_objv++;
	  }
	  break;
	case SWITCH_FILES:
	  {
	    int result;
	    if (switch_objc < 2)
	      {
		Tcl_WrongNumArgs (interp, 3, objv,
				  "?-files fileList  -filename 1|0 -static 1|0?");
		result_ptr->flags |= GDBTK_IN_TCL_RESULT;
		return TCL_ERROR;
	      }
	    result = Tcl_ListObjGetElements (interp, switch_objv[1],
					     &nfiles, &file_list);
	    if (result != TCL_OK)
	      return result;

	    files = (char **) xmalloc (nfiles * sizeof (char *));
	    for (i = 0; i < nfiles; i++)
	      files[i] = Tcl_GetStringFromObj (file_list[i], NULL);
	    switch_objc--;
	    switch_objv++;
	  }
	  break;
	case SWITCH_STATIC_ONLY:
	  if (switch_objc < 2)
	    {
	      Tcl_WrongNumArgs (interp, 3, objv,
				"?-files fileList  -filename 1|0 -static 1|0?");
	      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
	      return TCL_ERROR;
	    }
	  if (Tcl_GetBooleanFromObj (interp, switch_objv[1], &static_only)
	      != TCL_OK)
	    {
	      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
	      return TCL_ERROR;
	    }
	  switch_objc--;
	  switch_objv++;
	}
      switch_objc--;
      switch_objv++;
    }

  search_symbols (regexp, space, nfiles, files, &ss);
  if (ss != NULL)
    old_chain = make_cleanup_free_search_symbols (ss);

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);

  for (p = ss; p != NULL; p = p->next)
    {
      Tcl_Obj *elem;

      if (static_only && p->block != STATIC_BLOCK)
	continue;

      /* Strip off some C++ special symbols, like RTTI and global
         constructors/destructors. */
      if ((p->symbol != NULL && !STREQN (SYMBOL_NAME (p->symbol), "__tf", 4)
	   && !STREQN (SYMBOL_NAME (p->symbol), "_GLOBAL_", 8))
	  || p->msymbol != NULL)
	{
	  elem = Tcl_NewListObj (0, NULL);

	  if (p->msymbol == NULL)
	    Tcl_ListObjAppendElement (interp, elem,
		     Tcl_NewStringObj (SYMBOL_SOURCE_NAME (p->symbol), -1));
	  else
	    Tcl_ListObjAppendElement (interp, elem,
		    Tcl_NewStringObj (SYMBOL_SOURCE_NAME (p->msymbol), -1));

	  if (show_files)
	    {
	      if ((p->symtab != NULL) && (p->symtab->filename != NULL))
		{
		  Tcl_ListObjAppendElement (interp, elem, Tcl_NewStringObj
					    (p->symtab->filename, -1));
		}
	      else
		{
		  Tcl_ListObjAppendElement (interp, elem,
					    Tcl_NewStringObj ("", 0));
		}
	    }

	  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr, elem);
	}
    }

  if (ss != NULL)
    do_cleanups (old_chain);

  return TCL_OK;
}

/* This implements the tcl command gdb_listfuncs

 * It lists all the functions defined in a given file
 * 
 * Arguments:
 *    file - the file to look in
 * Tcl Result:
 *    A list of two element lists, the first element is
 *    the symbol name, and the second is a boolean indicating
 *    whether the symbol is demangled (1 for yes).
 */

static int
gdb_listfuncs (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct symtab *symtab;
  struct blockvector *bv;
  struct block *b;
  struct symbol *sym;
  int i, j;
  Tcl_Obj *funcVals[2];

  if (objc != 2)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "wrong # args", -1);
    }

  symtab = full_lookup_symtab (Tcl_GetStringFromObj (objv[1], NULL));
  if (!symtab)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "No such file", -1);
      return TCL_ERROR;
    }

  if (mangled == NULL)
    {
      mangled = Tcl_NewBooleanObj (1);
      not_mangled = Tcl_NewBooleanObj (0);
      Tcl_IncrRefCount (mangled);
      Tcl_IncrRefCount (not_mangled);
    }

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);

  bv = BLOCKVECTOR (symtab);
  for (i = GLOBAL_BLOCK; i <= STATIC_BLOCK; i++)
    {
      b = BLOCKVECTOR_BLOCK (bv, i);
      /* Skip the sort if this block is always sorted.  */
      if (!BLOCK_SHOULD_SORT (b))
	sort_block_syms (b);
      for (j = 0; j < BLOCK_NSYMS (b); j++)
	{
	  sym = BLOCK_SYM (b, j);
	  if (SYMBOL_CLASS (sym) == LOC_BLOCK)
	    {

	      char *name = SYMBOL_DEMANGLED_NAME (sym);

	      if (name)
		{
		  /* strip out "global constructors" and
		   * "global destructors"
		   * because we aren't interested in them. */
		  
		  if (strncmp (name, "global ", 7))
		    {
		      /* If the function is overloaded,
		       * print out the functions
		       * declaration, not just its name. */

		      funcVals[0] = Tcl_NewStringObj (name, -1);
		      funcVals[1] = mangled;
		    }
		  else
		    continue;

		}
	      else
		{
		  funcVals[0] = Tcl_NewStringObj (SYMBOL_NAME (sym), -1);
		  funcVals[1] = not_mangled;
		}
	      Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
					Tcl_NewListObj (2, funcVals));
	    }
	}
    }
  return TCL_OK;
}


/*
 * This section contains all the commands that act on the registers:
 */

/* This is a sort of mapcar function for operations on registers */

static int
map_arg_registers (objc, objv, func, argp)
     int objc;
     Tcl_Obj *CONST objv[];
     void (*func) (int regnum, void *argp);
     void *argp;
{
  int regnum, numregs;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = ARCH_NUM_REGS + NUM_PSEUDO_REGS;
  
  if (objc == 0)		/* No args, just do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    continue;
	  
	  func (regnum, argp);
	}
      
      return TCL_OK;
    }

  /* Else, list of register #s, just do listed regs */
  for (; objc > 0; objc--, objv++)
    {
      if (Tcl_GetIntFromObj (NULL, *objv, &regnum) != TCL_OK)
	{
	  result_ptr->flags |= GDBTK_IN_TCL_RESULT;
	  return TCL_ERROR;
	}

      if (regnum >= 0
	  && regnum < numregs
	  && REGISTER_NAME (regnum) != NULL
	  && *REGISTER_NAME (regnum) != '\000')
	func (regnum, argp);
      else
	{
	  Tcl_SetStringObj (result_ptr->obj_ptr, "bad register number", -1);
	  return TCL_ERROR;
	}
    }

  return TCL_OK;
}

/* This implements the TCL command `gdb_restore_fputs'
   It sets the fputs_unfiltered hook back to gdbtk_fputs.
   Its sole reason for being is that sometimes we move the
   fputs hook out of the way to specially trap output, and if
   we get an error which we weren't expecting, it won't get put
   back, so we run this at idle time as insurance.
 */

static int
gdb_restore_fputs (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
    fputs_unfiltered_hook = gdbtk_fputs;
    return TCL_OK;
}

/* This implements the TCL command `gdb_regnames'.  Its syntax is:

   gdb_regnames [-numbers] [REGNUM ...]

   Return a list containing the names of the registers whose numbers
   are given by REGNUM ... .  If no register numbers are given, return
   all the registers' names.

   Note that some processors have gaps in the register numberings:
   even if there is no register numbered N, there may still be a
   register numbered N+1.  So if you call gdb_regnames with no
   arguments, you can't assume that the N'th element of the result is
   register number N.

   Given the -numbers option, gdb_regnames returns, not a list of names,
   but a list of pairs {NAME NUMBER}, where NAME is the register name,
   and NUMBER is its number.  */

static int
gdb_regnames (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int numbers = 0;

  objc--;
  objv++;

  if (objc >= 1)
    {
      char *s = Tcl_GetStringFromObj (objv[0], NULL);
      if (STREQ (s, "-numbers"))
	numbers = 1;
      objc--;
      objv++;
    }

  return map_arg_registers (objc, objv, get_register_name, &numbers);
}

static void
get_register_name (regnum, argp)
     int regnum;
     void *argp;
{
  /* Non-zero if the caller wants the register numbers, too.  */
  int numbers = * (int *) argp;
  Tcl_Obj *name = Tcl_NewStringObj (REGISTER_NAME (regnum), -1);
  Tcl_Obj *elt;

  if (numbers)
    {
      /* Build a tuple of the form "{REGNAME NUMBER}", and append it to
	 our result.  */
      Tcl_Obj *array[2];

      array[0] = name;
      array[1] = Tcl_NewIntObj (regnum);
      elt = Tcl_NewListObj (2, array);
    }
  else
    elt = name;

  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, elt);
}

/* This implements the tcl command gdb_fetch_registers
 * Pass it a list of register names, and it will
 * return their values as a list.
 *
 * Tcl Arguments:
 *    format: The format string for printing the values
 *    args: the registers to look for
 * Tcl Result:
 *    A list of their values.
 */

static int
gdb_fetch_registers (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int format, result;

  if (objc < 2)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			"wrong # args, should be gdb_fetch_registers format ?register1 register2 ...?", -1);
    }
  objc -= 2;
  objv++;
  format = *(Tcl_GetStringFromObj (objv[0], NULL));
  objv++;


  result_ptr->flags |= GDBTK_MAKES_LIST;    /* Output the results as a list */
  result = map_arg_registers (objc, objv, get_register, (void *) format);
  result_ptr->flags &= ~GDBTK_MAKES_LIST;

  return result;
}

static void
get_register (regnum, fp)
     int regnum;
     void *fp;
{
  struct type *reg_vtype;
  char raw_buffer[MAX_REGISTER_RAW_SIZE];
  char virtual_buffer[MAX_REGISTER_VIRTUAL_SIZE];
  int format = (int) fp;
  int optim;

  if (format == 'N')
    format = 0;

  /* read_relative_register_raw_bytes returns a virtual frame pointer
     (FRAME_FP (selected_frame)) if regnum == FP_REGNUM instead
     of the real contents of the register. To get around this,
     use get_saved_register instead. */
  get_saved_register (raw_buffer, &optim, (CORE_ADDR *) NULL, selected_frame,
		      regnum, (enum lval_type *) NULL);
  if (optim)
    {
      Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
				Tcl_NewStringObj ("Optimized out", -1));
      return;
    }

  /* Convert raw data to virtual format if necessary.  */

  reg_vtype = REGISTER_VIRTUAL_TYPE (regnum);
  if (REGISTER_CONVERTIBLE (regnum))
    {
      REGISTER_CONVERT_TO_VIRTUAL (regnum, reg_vtype,
      				   raw_buffer, virtual_buffer);
    }
  else
    memcpy (virtual_buffer, raw_buffer, REGISTER_VIRTUAL_SIZE (regnum));

  if (format == 'r')
    {
      int j;
      char *ptr, buf[1024];

      strcpy (buf, "0x");
      ptr = buf + 2;
      for (j = 0; j < REGISTER_RAW_SIZE (regnum); j++)
	{
	  register int idx = TARGET_BYTE_ORDER == BIG_ENDIAN ? j
	  : REGISTER_RAW_SIZE (regnum) - 1 - j;
	  sprintf (ptr, "%02x", (unsigned char) raw_buffer[idx]);
	  ptr += 2;
	}
      fputs_filtered (buf, gdb_stdout);
    }
  else
    if ((TYPE_CODE (reg_vtype) == TYPE_CODE_UNION)
        && (strcmp (FIELD_NAME (TYPE_FIELD (reg_vtype, 0)), REGISTER_NAME (regnum)) == 0))
      {
        val_print (FIELD_TYPE (TYPE_FIELD (reg_vtype, 0)), virtual_buffer, 0, 0,
	           gdb_stdout, format, 1, 0, Val_pretty_default);
      }
    else
      val_print (REGISTER_VIRTUAL_TYPE (regnum), virtual_buffer, 0, 0,
	         gdb_stdout, format, 1, 0, Val_pretty_default);

}

/* This implements the tcl command get_pc_reg
 * It returns the value of the PC register
 *
 * Tcl Arguments:
 *    None
 * Tcl Result:
 *    The value of the pc register.
 */

static int
get_pc_register (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  char buff[64];

  sprintf (buff, "0x%llx", (long long) read_register (PC_REGNUM));
  Tcl_SetStringObj (result_ptr->obj_ptr, buff, -1);
  return TCL_OK;
}

/* This implements the tcl command "gdb_changed_register_list"
 * It takes a list of registers, and returns a list of
 * the registers on that list that have changed since the last
 * time the proc was called.
 *
 * Tcl Arguments:
 *    A list of registers.
 * Tcl Result:
 *    A list of changed registers.
 */

static int
gdb_changed_register_list (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  objc--;
  objv++;

  return map_arg_registers (objc, objv, register_changed_p, NULL);
}

static void
register_changed_p (regnum, argp)
     int regnum;
     void *argp;		/* Ignored */
{
  char raw_buffer[MAX_REGISTER_RAW_SIZE];

  if (read_relative_register_raw_bytes (regnum, raw_buffer))
    return;

  if (memcmp (&old_regs[REGISTER_BYTE (regnum)], raw_buffer,
	      REGISTER_RAW_SIZE (regnum)) == 0)
    return;

  /* Found a changed register.  Save new value and return its number. */

  memcpy (&old_regs[REGISTER_BYTE (regnum)], raw_buffer,
	  REGISTER_RAW_SIZE (regnum));

  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, Tcl_NewIntObj (regnum));
}

/*
 * This section contains the commands that deal with tracepoints:
 */

/* return a list of all tracepoint numbers in interpreter */
static int
gdb_get_tracepoint_list (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct tracepoint *tp;

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);

  ALL_TRACEPOINTS (tp)
    Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			      Tcl_NewIntObj (tp->number));

  return TCL_OK;
}

/* returns -1 if not found, tracepoint # if found */
static int
tracepoint_exists (char *args)
{
  struct tracepoint *tp;
  char **canonical;
  struct symtabs_and_lines sals;
  char *file = NULL;
  int result = -1;

  sals = decode_line_1 (&args, 1, NULL, 0, &canonical);
  if (sals.nelts == 1)
    {
      resolve_sal_pc (&sals.sals[0]);
      file = xmalloc (strlen (sals.sals[0].symtab->dirname)
		      + strlen (sals.sals[0].symtab->filename) + 1);
      if (file != NULL)
	{
	  strcpy (file, sals.sals[0].symtab->dirname);
	  strcat (file, sals.sals[0].symtab->filename);

	  ALL_TRACEPOINTS (tp)
	  {
	    if (tp->address == sals.sals[0].pc)
	      result = tp->number;
#if 0
	    /* Why is this here? This messes up assembly traces */
	    else if (tp->source_file != NULL
		     && strcmp (tp->source_file, file) == 0
		     && sals.sals[0].line == tp->line_number)
	      result = tp->number;
#endif
	  }
	}
    }
  if (file != NULL)
    free (file);
  return result;
}

static int
gdb_tracepoint_exists_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  char *args;

  if (objc != 2)
    {
      Tcl_AppendStringsToObj (result_ptr->obj_ptr,
			      "wrong # of args: should be \"",
			      Tcl_GetStringFromObj (objv[0], NULL),
			      " function:line|function|line|*addr\"", NULL);
      return TCL_ERROR;
    }

  args = Tcl_GetStringFromObj (objv[1], NULL);

  Tcl_SetIntObj (result_ptr->obj_ptr, tracepoint_exists (args));
  return TCL_OK;
}

static int
gdb_get_tracepoint_info (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct symtab_and_line sal;
  int tpnum;
  struct tracepoint *tp;
  struct action_line *al;
  Tcl_Obj *action_list;
  char *filename, *funcname, *fname;

  if (objc != 2)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "wrong # args", -1);
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (NULL, objv[1], &tpnum) != TCL_OK)
    {
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  ALL_TRACEPOINTS (tp)
    if (tp->number == tpnum)
    break;

  if (tp == NULL)
    {
      char buff[64];
      sprintf (buff, "Tracepoint #%d does not exist", tpnum);
      Tcl_SetStringObj (result_ptr->obj_ptr, buff, -1);
      return TCL_ERROR;
    }

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);
  sal = find_pc_line (tp->address, 0);
  filename = symtab_to_filename (sal.symtab);
  if (filename == NULL)
    filename = "N/A";
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewStringObj (filename, -1));

  funcname = pc_function_name (tp->address);
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr, Tcl_NewStringObj
			    (funcname, -1));

  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewIntObj (sal.line));
  {
    char *tmp;
    asprintf (&tmp, "0x%s", paddr_nz (tp->address));
    Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			      Tcl_NewStringObj (tmp, -1));
    free (tmp);
  }
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewIntObj (tp->enabled));
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewIntObj (tp->pass_count));
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewIntObj (tp->step_count));
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewIntObj (tp->thread));
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewIntObj (tp->hit_count));

  /* Append a list of actions */
  action_list = Tcl_NewObj ();
  for (al = tp->actions; al != NULL; al = al->next)
    {
      Tcl_ListObjAppendElement (interp, action_list,
				Tcl_NewStringObj (al->action, -1));
    }
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr, action_list);

  return TCL_OK;
}


static int
gdb_trace_status (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int result = 0;

  if (trace_running_p)
    result = 1;

  Tcl_SetIntObj (result_ptr->obj_ptr, result);
  return TCL_OK;
}



static int
gdb_get_trace_frame_num (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  if (objc != 1)
    {
      Tcl_AppendStringsToObj (result_ptr->obj_ptr,
			      "wrong # of args: should be \"",
			      Tcl_GetStringFromObj (objv[0], NULL),
			      " linespec\"", NULL);
      return TCL_ERROR;
    }

  Tcl_SetIntObj (result_ptr->obj_ptr, get_traceframe_number ());
  return TCL_OK;

}

/* This implements the tcl command gdb_actions
 * It sets actions for a given tracepoint.
 *
 * Tcl Arguments:
 *    number: the tracepoint in question
 *    actions: the actions to add to this tracepoint
 * Tcl Result:
 *    None.
 */

static int
gdb_actions_command (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct tracepoint *tp;
  Tcl_Obj **actions;
  int nactions, i, len;
  char *number, *args, *action;
  long step_count;
  struct action_line *next = NULL, *temp;
  enum actionline_type linetype;

  if (objc != 3)
    {
      Tcl_AppendStringsToObj (result_ptr->obj_ptr,
			      "wrong # args: should be: \"",
			      Tcl_GetStringFromObj (objv[0], NULL),
			      " number actions\"", NULL);
      return TCL_ERROR;
    }

  args = number = Tcl_GetStringFromObj (objv[1], NULL);
  tp = get_tracepoint_by_number (&args, 0, 0);
  if (tp == NULL)
    {
      Tcl_AppendStringsToObj (result_ptr->obj_ptr, "Tracepoint \"",
			      number, "\" does not exist", NULL);
      return TCL_ERROR;
    }

  /* Free any existing actions */
  if (tp->actions != NULL)
    free_actions (tp);

  step_count = 0;

  Tcl_ListObjGetElements (interp, objv[2], &nactions, &actions);

  /* Add the actions to the tracepoint */
  for (i = 0; i < nactions; i++)
    {
      temp = xmalloc (sizeof (struct action_line));
      temp->next = NULL;
      action = Tcl_GetStringFromObj (actions[i], &len);
      temp->action = savestring (action, len);

      linetype = validate_actionline (&(temp->action), tp);

      if (linetype == BADLINE)
	{
	  free (temp);
	  continue;
	}

      if (next == NULL)
	{
	  tp->actions = temp;
	  next = temp;
	}
      else
	{
	  next->next = temp;
	  next = temp;
	}
    }

  return TCL_OK;
}

/*
 * This section has commands that handle source disassembly.
 */
/* This implements the tcl command gdb_disassemble.  It is no longer
 * used in GDBTk, we use gdb_load_disassembly, but I kept it around in
 * case other folks want it.
 *
 * Arguments:
 *    source_with_assm - must be "source" or "nosource"
 *    low_address - the address from which to start disassembly
 *    ?hi_address? - the address to which to disassemble, defaults
 *                   to the end of the function containing low_address.
 * Tcl Result:
 *    The disassembled code is passed to fputs_unfiltered, so it
 *    either goes to the console if result_ptr->obj_ptr is NULL or to
 *    the Tcl result.
 */

static int
gdb_disassemble (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  CORE_ADDR low, high;
  char *arg_ptr;
  int mixed_source_and_assembly;

  if (objc != 3 && objc != 4)
    error ("wrong # args");

  arg_ptr = Tcl_GetStringFromObj (objv[1], NULL);
  if (*arg_ptr == 's' && strcmp (arg_ptr, "source") == 0)
    mixed_source_and_assembly = 1;
  else if (*arg_ptr == 'n' && strcmp (arg_ptr, "nosource") == 0)
    mixed_source_and_assembly = 0;
  else
    error ("First arg must be 'source' or 'nosource'");

  low = parse_and_eval_address (Tcl_GetStringFromObj (objv[2], NULL));

  if (objc == 3)
    {
      if (find_pc_partial_function (low, NULL, &low, &high) == 0)
        error ("No function contains specified address");
    }
  else
    high = parse_and_eval_address (Tcl_GetStringFromObj (objv[3], NULL));

  return gdb_disassemble_driver (low, high, mixed_source_and_assembly, NULL,
			  gdbtk_print_source, gdbtk_print_asm);

}

/* This implements the tcl command gdb_load_disassembly
 *
 * Arguments:
 *    widget - the name of a text widget into which to load the data
 *    source_with_assm - must be "source" or "nosource"
 *    low_address - the address from which to start disassembly
 *    ?hi_address? - the address to which to disassemble, defaults
 *                   to the end of the function containing low_address.
 * Tcl Result:
 *    The text widget is loaded with the data, and a list is returned.
 *    The first element of the list is a two element list containing the
 *    real low & high elements, the rest is a mapping between line number
 *    in the text widget, and either the source line number of that line,
 *    if it is a source line, or the assembly address.  You can distinguish
 *    between the two, because the address will start with 0x...
 */

static int
gdb_load_disassembly (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  CORE_ADDR low, high;
  struct disassembly_client_data client_data;
  int mixed_source_and_assembly, ret_val, i;
  char *widget;
  char *arg_ptr;
  char *map_name;

  if (objc != 6 && objc != 7) {
    Tcl_SetStringObj (result_ptr->obj_ptr, "wrong # args, should be: widget [source|nosource] map_arr index_prefix low_address ?hi_address", -1);
    return TCL_ERROR;
  }

  client_data.widget = Tcl_GetStringFromObj (objv[1], NULL);
  if ( Tk_NameToWindow (interp, client_data.widget,
			Tk_MainWindow (interp)) == NULL)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "Invalid widget name.", -1);
      return TCL_ERROR;
    }

  if (!Tcl_GetCommandInfo (interp, client_data.widget, &client_data.cmd))
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "Can't get widget command info",
			-1);
      return TCL_ERROR;
    }

  arg_ptr = Tcl_GetStringFromObj (objv[2], NULL);
  if (*arg_ptr == 's' && strcmp (arg_ptr, "source") == 0)
    mixed_source_and_assembly = 1;
  else if (*arg_ptr == 'n' && strcmp (arg_ptr, "nosource") == 0)
    mixed_source_and_assembly = 0;
  else
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			"Second arg must be 'source' or 'nosource'", -1);
      return TCL_ERROR;
    }

  /* As we populate the text widget, we will also create an array in the
     caller's scope.  The name is given by objv[3].
     Each source line gets an entry or the form:
         array($prefix,srcline=$src_line_no) = $widget_line_no

     Each assembly line gets two entries of the form:
         array($prefix,pc=$pc) = $widget_line_no
         array($prefix,line=$widget_line_no) = $src_line_no

     Where prefix is objv[4].
  */
    
  map_name = Tcl_GetStringFromObj (objv[3], NULL);

  if (*map_name != '\0')
    {
      char *prefix;
      int prefix_len;
      
      client_data.map_arr = "map_array";
      if (Tcl_UpVar (interp, "1", map_name, client_data.map_arr, 0) != TCL_OK) {
	Tcl_SetStringObj (result_ptr->obj_ptr, "Can't link map array.", -1);
	return TCL_ERROR;
      }

      prefix = Tcl_GetStringFromObj (objv[4], &prefix_len);
      
      Tcl_DStringInit(&client_data.src_to_line_prefix);
      Tcl_DStringAppend (&client_data.src_to_line_prefix,
			 prefix, prefix_len);
      Tcl_DStringAppend (&client_data.src_to_line_prefix, ",srcline=",
				 sizeof (",srcline=") - 1);
			      
      Tcl_DStringInit(&client_data.pc_to_line_prefix);
      Tcl_DStringAppend (&client_data.pc_to_line_prefix,
			 prefix, prefix_len);
      Tcl_DStringAppend (&client_data.pc_to_line_prefix, ",pc=",
			 sizeof (",pc=") - 1);
      
      Tcl_DStringInit(&client_data.line_to_pc_prefix);
      Tcl_DStringAppend (&client_data.line_to_pc_prefix,
			 prefix, prefix_len);
      Tcl_DStringAppend (&client_data.line_to_pc_prefix, ",line=",
			 sizeof (",line=") - 1);

    }
  else
    {
      client_data.map_arr = "";
    }

  /* Now parse the addresses */
  
  low = parse_and_eval_address (Tcl_GetStringFromObj (objv[5], NULL));

  if (objc == 6)
    {
      if (find_pc_partial_function (low, NULL, &low, &high) == 0)
        error ("No function contains specified address");
    }
  else
    high = parse_and_eval_address (Tcl_GetStringFromObj (objv[6], NULL));


  /* Setup the client_data structure, and call the driver function. */
  
  client_data.file_opened_p = 0;
  client_data.widget_line_no = 0;
  client_data.interp = interp;
  for (i = 0; i < 3; i++)
    {
      client_data.result_obj[i] = Tcl_NewObj();
      Tcl_IncrRefCount (client_data.result_obj[i]);
    }

  /* Fill up the constant parts of the argv structures */
  client_data.asm_argv[0] = client_data.widget;
  client_data.asm_argv[1] = "insert";
  client_data.asm_argv[2] = "end";
  client_data.asm_argv[3] = "-\t";
  client_data.asm_argv[4] = "break_rgn_tag";
  /* client_data.asm_argv[5] = address; */
  client_data.asm_argv[6] = "break_rgn_tag";
  /* client_data.asm_argv[7] = offset; */
  client_data.asm_argv[8] = "break_rgn_tag";
  client_data.asm_argv[9] = ":\t\t";
  client_data.asm_argv[10] = "source_tag";
  /* client_data.asm_argv[11] = code; */
  client_data.asm_argv[12] = "source_tag";
  client_data.asm_argv[13] = "\n";

  if (mixed_source_and_assembly)
    {
      client_data.source_argv[0] = client_data.widget;
      client_data.source_argv[1] = "insert";
      client_data.source_argv[2] = "end";
      /* client_data.source_argv[3] = line_number; */
      client_data.source_argv[4] = "";
      /* client_data.source_argv[5] = line; */
      client_data.source_argv[6] = "source_tag";
    }
  
  ret_val = gdb_disassemble_driver (low, high, mixed_source_and_assembly, 
			  (ClientData) &client_data,
			  gdbtk_load_source, gdbtk_load_asm);

  /* Now clean up the opened file, and the Tcl data structures */
  
  if (client_data.file_opened_p == 1) {
    fclose(client_data.fp);
  }
  if (*client_data.map_arr != '\0')
    {
      Tcl_DStringFree(&client_data.src_to_line_prefix);
      Tcl_DStringFree(&client_data.pc_to_line_prefix);
      Tcl_DStringFree(&client_data.line_to_pc_prefix);
    }
  
  for (i = 0; i < 3; i++)
    {
      Tcl_DecrRefCount (client_data.result_obj[i]);
    }
  
  /* Finally, if we were successful, stick the low & high addresses
     into the Tcl result. */

  if (ret_val == TCL_OK) {
    char buffer[256];
    Tcl_Obj *limits_obj[2];

    sprintf (buffer, "0x%s", paddr_nz (low));
    limits_obj[0] = Tcl_NewStringObj (buffer, -1);
    
    sprintf (buffer, "0x%s", paddr_nz (high));
    limits_obj[1] = Tcl_NewStringObj (buffer, -1);

    Tcl_DecrRefCount (result_ptr->obj_ptr);
    result_ptr->obj_ptr = Tcl_NewListObj (2, limits_obj);
    
  }
  return ret_val;

}

static void
gdbtk_load_source (ClientData clientData, struct symtab *symtab, int
		      start_line, int end_line)
{
  struct disassembly_client_data *client_data =
    (struct disassembly_client_data *) clientData;
  char buffer[18];
  int index_len;

  index_len = Tcl_DStringLength (&client_data->src_to_line_prefix);
  
  if (client_data->file_opened_p == 1)
    {
      char **text_argv;
      char line[10000], line_number[18];
      int found_carriage_return = 1;

      /* First do some sanity checks on the requested lines */

      if (start_line < 1
	  || end_line < start_line || end_line > symtab->nlines)
	{
	  return;
	}

      line_number[0] = '\t';
      line[0] = '\t';

      text_argv = client_data->source_argv;
      
      text_argv[3] = line_number;
      text_argv[5] = line;

      if (fseek (client_data->fp, symtab->line_charpos[start_line - 1],
		 SEEK_SET) < 0)
	{
	  fclose(client_data->fp);
	  client_data->file_opened_p = -1;
	  return;
	}
      
      for (; start_line < end_line; start_line++)
	{
	  if (!fgets (line + 1, 9980, client_data->fp))
	    {
	      fclose(client_data->fp);
	      client_data->file_opened_p = -1;
	      return;
	    }

	  client_data->widget_line_no++;
	  
	  sprintf (line_number + 1, "%d", start_line);
	  
	  if (found_carriage_return) {
	    char *p;
	    
	    p = strrchr(line, '\0') - 2;
	    if (*p == '\r') {
	      *p = '\n';
	      *(p + 1) = '\0';
	    } else {
	      found_carriage_return = 0;
	    }
	  }

	  /* Run the command, then add an entry to the map array in
	     the caller's scope, if requested. */
	  
	  client_data->cmd.proc (client_data->cmd.clientData, 
				 client_data->interp, 7, text_argv);
	  
	  if (*client_data->map_arr != '\0')
	    {
	      
	      Tcl_DStringAppend (&client_data->src_to_line_prefix,
				 line_number + 1, -1);
	      
	      /* FIXME: Convert to Tcl_SetVar2Ex when we move to 8.2.  This
		 will allow us avoid converting widget_line_no into a string. */
	      
	      sprintf (buffer, "%d", client_data->widget_line_no);
	      
	      Tcl_SetVar2 (client_data->interp, client_data->map_arr,
			   Tcl_DStringValue (&client_data->src_to_line_prefix),
			   buffer, 0);
	      
	      Tcl_DStringSetLength (&client_data->src_to_line_prefix, index_len);
	    }
	}
      
    }
  else if (!client_data->file_opened_p)
    {
      int fdes;
      /* The file is not yet open, try to open it, then print the
	 first line.  If we fail, set FILE_OPEN_P to -1. */
      
      fdes = open_source_file (symtab);
      if (fdes < 0)
	{
	  client_data->file_opened_p = -1;
	}
      else
	{
          /* FIXME: Convert to a Tcl File Channel and read from there.
	     This will allow us to get the line endings and conversion
	     to UTF8 right automatically when we move to 8.2.
	     Need a Cygwin call to convert a file descriptor to the native
	     Windows handler to do this. */
	     
	  client_data->file_opened_p = 1;
	  client_data->fp = fdopen (fdes, FOPEN_RB);
	  clearerr (client_data->fp);
	  
          if (symtab->line_charpos == 0)
            find_source_lines (symtab, fdes);

	  /* We are called with an actual load request, so call ourselves
	     to load the first line. */
	  
	  gdbtk_load_source (clientData, symtab, start_line, end_line);
	}
    }
  else {
    /* If we couldn't open the file, or got some prior error, just exit. */
    
    return;
  }

}


static CORE_ADDR
gdbtk_load_asm (clientData, pc, di)
     ClientData clientData;
     CORE_ADDR pc;
     struct disassemble_info *di;
{
  struct disassembly_client_data * client_data
    = (struct disassembly_client_data *) clientData;
  char **text_argv;
  int i, pc_to_line_len, line_to_pc_len;
  gdbtk_result new_result;
  struct cleanup *old_chain = NULL;

  pc_to_line_len = Tcl_DStringLength (&client_data->pc_to_line_prefix);
  line_to_pc_len = Tcl_DStringLength (&client_data->line_to_pc_prefix);
    
  text_argv = client_data->asm_argv;
  
  /* Preserve the current Tcl result object, print out what we need, and then
     suck it out of the result, and replace... */

  old_chain = make_cleanup (gdbtk_restore_result_ptr, (void *) result_ptr);
  result_ptr = &new_result;
  result_ptr->obj_ptr = client_data->result_obj[0];
  result_ptr->flags = GDBTK_TO_RESULT;

  /* Null out the three return objects we will use. */

  for (i = 0; i < 3; i++)
    Tcl_SetObjLength (client_data->result_obj[i], 0);
  
  print_address_numeric (pc, 1, gdb_stdout);
  gdb_flush (gdb_stdout);

  result_ptr->obj_ptr = client_data->result_obj[1];
  
  print_address_symbolic (pc, gdb_stdout, 1, "\t");
  gdb_flush (gdb_stdout);

  result_ptr->obj_ptr = client_data->result_obj[2];
  pc += (*tm_print_insn) (pc, di);
  gdb_flush (gdb_stdout);

  client_data->widget_line_no++;

  text_argv[5] = Tcl_GetStringFromObj (client_data->result_obj[0], NULL);
  text_argv[7] = Tcl_GetStringFromObj (client_data->result_obj[1], NULL);
  text_argv[11] = Tcl_GetStringFromObj (client_data->result_obj[2], NULL);

  client_data->cmd.proc (client_data->cmd.clientData, 
				     client_data->interp, 14, text_argv);

  if (*client_data->map_arr != '\0')
    {
      char buffer[16];
      
      /* Run the command, then add an entry to the map array in
	 the caller's scope. */
      
      Tcl_DStringAppend (&client_data->pc_to_line_prefix, text_argv[5], -1);
      
      /* FIXME: Convert to Tcl_SetVar2Ex when we move to 8.2.  This
	 will allow us avoid converting widget_line_no into a string. */
      
      sprintf (buffer, "%d", client_data->widget_line_no);
      
      Tcl_SetVar2 (client_data->interp, client_data->map_arr,
		   Tcl_DStringValue (&client_data->pc_to_line_prefix),
		   buffer, 0);

      Tcl_DStringAppend (&client_data->line_to_pc_prefix, buffer, -1);
      
      Tcl_SetVar2 (client_data->interp, client_data->map_arr,
		   Tcl_DStringValue (&client_data->line_to_pc_prefix),
		   text_argv[5], 0);

      /* Restore the prefixes to their initial state. */
      
      Tcl_DStringSetLength (&client_data->pc_to_line_prefix, pc_to_line_len);      
      Tcl_DStringSetLength (&client_data->line_to_pc_prefix, line_to_pc_len);      
      
    }
  
  do_cleanups (old_chain);
    
  return pc;
}

static void
gdbtk_print_source (clientData, symtab, start_line, end_line)
     ClientData clientData;
     struct symtab *symtab;
     int start_line;
     int end_line;
{
  print_source_lines (symtab, start_line, end_line, 0);
  gdb_flush (gdb_stdout);
}

static CORE_ADDR
gdbtk_print_asm (clientData, pc, di)
     ClientData clientData;
     CORE_ADDR pc;
     struct disassemble_info *di;
{
  fputs_unfiltered ("    ", gdb_stdout);
  print_address (pc, gdb_stdout);
  fputs_unfiltered (":\t    ", gdb_stdout);
  pc += (*tm_print_insn) (pc, di);
  fputs_unfiltered ("\n", gdb_stdout);
  gdb_flush (gdb_stdout);
  return pc;
}

static int
gdb_disassemble_driver (low, high, mixed_source_and_assembly,
			clientData, print_source_fn, print_asm_fn)
     CORE_ADDR low;
     CORE_ADDR high;
     int mixed_source_and_assembly;
     ClientData clientData; 
     void (*print_source_fn) (ClientData, struct symtab *, int, int);
     CORE_ADDR (*print_asm_fn) (ClientData, CORE_ADDR,
				struct disassemble_info *);
{
  CORE_ADDR pc;
  static disassemble_info di;
  static int di_initialized;

  if (! di_initialized)
    {
      INIT_DISASSEMBLE_INFO_NO_ARCH (di, gdb_stdout,
                                     (fprintf_ftype) fprintf_unfiltered);
      di.flavour = bfd_target_unknown_flavour;
      di.memory_error_func = dis_asm_memory_error;
      di.print_address_func = dis_asm_print_address;
      di_initialized = 1;
    }

  di.mach = TARGET_PRINT_INSN_INFO->mach;
  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    di.endian = BFD_ENDIAN_BIG;
  else
    di.endian = BFD_ENDIAN_LITTLE;

  /* Set the architecture for multi-arch configurations. */
  if (TARGET_ARCHITECTURE != NULL)
    di.mach = TARGET_ARCHITECTURE->mach;

  /* If disassemble_from_exec == -1, then we use the following heuristic to
     determine whether or not to do disassembly from target memory or from the
     exec file:

     If we're debugging a local process, read target memory, instead of the
     exec file.  This makes disassembly of functions in shared libs work
     correctly.  Also, read target memory if we are debugging native threads.

     Else, we're debugging a remote process, and should disassemble from the
     exec file for speed.  However, this is no good if the target modifies its
     code (for relocation, or whatever).

     As an aside, it is fairly bogus that there is not a better way to
     determine where to disassemble from.  There should be a target vector
     entry for this or something.
     
   */

  if (disassemble_from_exec == -1)
    {
      if (strcmp (target_shortname, "child") == 0
          || strcmp (target_shortname, "procfs") == 0
          || strcmp (target_shortname, "vxprocess") == 0
	  || strstr (target_shortname, "threads") != NULL)
	/* It's a child process, read inferior mem */
        disassemble_from_exec = 0; 
      else
	/* It's remote, read the exec file */
        disassemble_from_exec = 1; 
    }

  if (disassemble_from_exec)
    di.read_memory_func = gdbtk_dis_asm_read_memory;
  else
    di.read_memory_func = dis_asm_read_memory;

  /* If just doing straight assembly, all we need to do is disassemble
     everything between low and high.  If doing mixed source/assembly, we've
     got a totally different path to follow.  */

  if (mixed_source_and_assembly)
    {				/* Come here for mixed source/assembly */
      /* The idea here is to present a source-O-centric view of a function to
         the user.  This means that things are presented in source order, with
         (possibly) out of order assembly immediately following.  */
      struct symtab *symtab;
      struct linetable_entry *le;
      int nlines;
      int newlines;
      struct my_line_entry *mle;
      struct symtab_and_line sal;
      int i;
      int out_of_order;
      int next_line;
      
      /* Assume symtab is valid for whole PC range */
      symtab = find_pc_symtab (low); 

      if (!symtab || !symtab->linetable)
        goto assembly_only;

      /* First, convert the linetable to a bunch of my_line_entry's.  */

      le = symtab->linetable->item;
      nlines = symtab->linetable->nitems;

      if (nlines <= 0)
        goto assembly_only;

      mle = (struct my_line_entry *) alloca (nlines *
					     sizeof (struct my_line_entry));

      out_of_order = 0;
      
      /* Copy linetable entries for this function into our data structure,
	 creating end_pc's and setting out_of_order as appropriate.  */

      /* First, skip all the preceding functions.  */

      for (i = 0; i < nlines - 1 && le[i].pc < low; i++) ;

      /* Now, copy all entries before the end of this function.  */

      newlines = 0;
      for (; i < nlines - 1 && le[i].pc < high; i++)
        {
          if (le[i].line == le[i + 1].line
              && le[i].pc == le[i + 1].pc)
            continue;		/* Ignore duplicates */

	  /* GCC sometimes emits line directives with a linenumber
	     of 0.  It does this to handle live range splitting.
	     This may be a bug, but we need to be able to handle it.
	     For now, use the previous instructions line number.
	     Since this is a bit of a hack anyway, we will just lose
	     if the bogus sline is the first line of the range.  For
	     functions, I have never seen this to be the case.  */
	  
	  if (le[i].line != 0)
	    {
	      mle[newlines].line = le[i].line;
	    }
	  else
	    {
	      if (newlines > 0)
		mle[newlines].line = mle[newlines - 1].line;
	    }
	  
          if (le[i].line > le[i + 1].line)
            out_of_order = 1;
          mle[newlines].start_pc = le[i].pc;
          mle[newlines].end_pc = le[i + 1].pc;
          newlines++;
        }

      /* If we're on the last line, and it's part of the function, then we 
         need to get the end pc in a special way.  */

      if (i == nlines - 1
          && le[i].pc < high)
        {
          mle[newlines].line = le[i].line;
          mle[newlines].start_pc = le[i].pc;
          sal = find_pc_line (le[i].pc, 0);
          mle[newlines].end_pc = sal.end;
          newlines++;
        }

      /* Now, sort mle by line #s (and, then by addresses within lines). */

      if (out_of_order)
        qsort (mle, newlines, sizeof (struct my_line_entry), compare_lines);

      /* Now, for each line entry, emit the specified lines (unless they have
	 been emitted before), followed by the assembly code for that line.  */

      next_line = 0;		/* Force out first line */
      for (i = 0; i < newlines; i++)
        {
          /* Print out everything from next_line to the current line.  */

          if (mle[i].line >= next_line)
            {
              if (next_line != 0)
                print_source_fn (clientData, symtab, next_line,
				 mle[i].line + 1);
              else
                print_source_fn (clientData, symtab, mle[i].line,
				 mle[i].line + 1);

              next_line = mle[i].line + 1;
            }

          for (pc = mle[i].start_pc; pc < mle[i].end_pc; )
            {
              QUIT;
	      pc = print_asm_fn (clientData, pc, &di);
            }
        }
    }
  else
    {
    assembly_only:
      for (pc = low; pc < high; )
        {
          QUIT;
	  pc = print_asm_fn (clientData, pc, &di);
        }
    }

  return TCL_OK;
}

/* This is the memory_read_func for gdb_disassemble when we are
   disassembling from the exec file. */

static int
gdbtk_dis_asm_read_memory (memaddr, myaddr, len, info)
     bfd_vma memaddr;
     bfd_byte *myaddr;
     unsigned int len;
     disassemble_info *info;
{
  extern struct target_ops exec_ops;
  int res;

  errno = 0;
  res = xfer_memory (memaddr, myaddr, len, 0, &exec_ops);

  if (res == len)
    return 0;
  else if (errno == 0)
    return EIO;
  else
    return errno;
}

/* This will be passed to qsort to sort the results of the disassembly */

static int
compare_lines (mle1p, mle2p)
     const PTR mle1p;
     const PTR mle2p;
{
  struct my_line_entry *mle1, *mle2;
  int val;

  mle1 = (struct my_line_entry *) mle1p;
  mle2 = (struct my_line_entry *) mle2p;

  val = mle1->line - mle2->line;

  if (val != 0)
    return val;

  return mle1->start_pc - mle2->start_pc;
}

/* This implements the TCL command `gdb_loc',

 * Arguments:
 *    ?symbol? The symbol or address to locate - defaults to pc
 * Tcl Return:
 *    a list consisting of the following:                                  
 *       basename, function name, filename, line number, address, current pc
 */

static int
gdb_loc (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  char *filename;
  struct symtab_and_line sal;
  struct symbol *sym;
  char *fname;
  CORE_ADDR pc;

  if (objc == 1)
    {
      if (selected_frame && (selected_frame->pc != read_pc ()))
        {
          /* Note - this next line is not correct on all architectures.
	     For a graphical debugger we really want to highlight the 
	     assembly line that called the next function on the stack.
	     Many architectures have the next instruction saved as the
	     pc on the stack, so what happens is the next instruction 
	     is highlighted. FIXME */
	  pc = selected_frame->pc;
	  sal = find_pc_line (selected_frame->pc,
			      selected_frame->next != NULL
			      && !selected_frame->next->signal_handler_caller
			      && !frame_in_dummy (selected_frame->next));
	}
      else
        {
          pc = read_pc ();
          sal = find_pc_line (pc, 0);
        }
    }
  else if (objc == 2)
    {
      struct symtabs_and_lines sals;
      int nelts;

      sals = decode_line_spec (Tcl_GetStringFromObj (objv[1], NULL), 1);

      nelts = sals.nelts;
      sal = sals.sals[0];
      free (sals.sals);

      if (sals.nelts != 1)
	{
	  Tcl_SetStringObj (result_ptr->obj_ptr, "Ambiguous line spec", -1);
	  return TCL_ERROR;
	}
      resolve_sal_pc (&sal);
      pc = sal.pc;
    }
  else
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "wrong # args", -1);
      return TCL_ERROR;
    }

  if (sal.symtab)
    Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			      Tcl_NewStringObj (sal.symtab->filename, -1));
  else
    Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			      Tcl_NewStringObj ("", 0));

  fname = pc_function_name (pc);
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewStringObj (fname, -1));

  filename = symtab_to_filename (sal.symtab);
  if (filename == NULL)
    filename = "";

  /* file name */
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewStringObj (filename, -1));
  /* line number */
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewIntObj (sal.line));
  /* PC in current frame */
  sprintf_append_element_to_obj (result_ptr->obj_ptr, "0x%s", paddr_nz (pc));
  /* Real PC */
  sprintf_append_element_to_obj (result_ptr->obj_ptr, "0x%s",
				 paddr_nz (stop_pc));

  /* shared library */
#ifdef PC_SOLIB
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewStringObj (PC_SOLIB (pc), -1));
#else
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewStringObj ("", -1));
#endif
  return TCL_OK;
}

/* This implements the Tcl command 'gdb_get_mem', which 
 * dumps a block of memory 
 * Arguments:
 *   gdb_get_mem addr form size nbytes bpr aschar
 *
 *   addr: address of data to dump
 *   form: a char indicating format
 *   size: size of each element; 1,2,4, or 8 bytes
 *   nbytes: the number of bytes to read 
 *   bpr: bytes per row
 *   aschar: if present, an ASCII dump of the row is included.  ASCHAR
 *   used for unprintable characters.
 * 
 * Return:
 * a list of elements followed by an optional ASCII dump */

static int
gdb_get_mem (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int size, asize, i, j, bc;
  CORE_ADDR addr;
  int nbytes, rnum, bpr;
  long tmp;
  char format, buff[128], aschar, *mbuf, *mptr, *cptr, *bptr;
  struct type *val_type;

  if (objc < 6 || objc > 7)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			"addr format size bytes bytes_per_row ?ascii_char?",
			-1);
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (interp, objv[3], &size) != TCL_OK)
    {
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }
  else if (size <= 0)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "Invalid size, must be > 0", -1);
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (interp, objv[4], &nbytes) != TCL_OK)
    {
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }
  else if (nbytes <= 0)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			"Invalid number of bytes, must be > 0",
			-1);
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (interp, objv[5], &bpr) != TCL_OK)
    {
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }
  else if (bpr <= 0)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			"Invalid bytes per row, must be > 0", -1);
      return TCL_ERROR;
    }

  if (Tcl_GetLongFromObj (interp, objv[1], &tmp) != TCL_OK)
    return TCL_OK;

  addr = (CORE_ADDR) tmp;

  format = *(Tcl_GetStringFromObj (objv[2], NULL));
  mbuf = (char *) xmalloc (nbytes + 32);
  if (!mbuf)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "Out of memory.", -1);
      return TCL_ERROR;
    }

  memset (mbuf, 0, nbytes + 32);
  mptr = cptr = mbuf;

  rnum = 0;
  while (rnum < nbytes)
    {
      int error;
      int num = target_read_memory_partial (addr + rnum, mbuf + rnum,
					    nbytes - rnum, &error);
      if (num <= 0)
	break;
      rnum += num;
    }

  if (objc == 7)
    aschar = *(Tcl_GetStringFromObj (objv[6], NULL));
  else
    aschar = 0;

  switch (size)
    {
    case 1:
      val_type = builtin_type_int8;
      asize = 'b';
      break;
    case 2:
      val_type = builtin_type_int16;
      asize = 'h';
      break;
    case 4:
      val_type = builtin_type_int32;
      asize = 'w';
      break;
    case 8:
      val_type = builtin_type_int64;
      asize = 'g';
      break;
    default:
      val_type = builtin_type_int8;
      asize = 'b';
    }

  bc = 0;			/* count of bytes in a row */
  bptr = &buff[0];		/* pointer for ascii dump */

  /* Build up the result as a list... */
  
  result_ptr->flags |= GDBTK_MAKES_LIST;	

  for (i = 0; i < nbytes; i += size)
    {
      if (i >= rnum)
	{
	  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
				    Tcl_NewStringObj ("N/A", 3));
	  if (aschar)
	    for (j = 0; j < size; j++)
	      *bptr++ = 'X';
	}
      else
	{
	  print_scalar_formatted (mptr, val_type, format, asize, gdb_stdout);

	  if (aschar)
	    {
	      for (j = 0; j < size; j++)
		{
		  *bptr = *cptr++;
		  if (*bptr < 32 || *bptr > 126)
		    *bptr = aschar;
		  bptr++;
		}
	    }
	}

      mptr += size;
      bc += size;

      if (aschar && (bc >= bpr))
	{
	  /* end of row. Add it to the result and reset variables */
	  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
				    Tcl_NewStringObj (buff, bc));
	  bc = 0;
	  bptr = &buff[0];
	}
    }

  result_ptr->flags &= ~GDBTK_MAKES_LIST;

  free (mbuf);
  return TCL_OK;
}


/* This implements the tcl command "gdb_loadfile"
 * It loads a c source file into a text widget.
 *
 * Tcl Arguments:
 *    widget: the name of the text widget to fill
 *    filename: the name of the file to load
 *    linenumbers: A boolean indicating whether or not to display line numbers.
 * Tcl Result:
 *
 */

/* In this routine, we will build up a "line table", i.e. a
 * table of bits showing which lines in the source file are executible.
 * LTABLE_SIZE is the number of bytes to allocate for the line table.
 *
 * Its size limits the maximum number of lines 
 * in a file to 8 * LTABLE_SIZE.  This memory is freed after 
 * the file is loaded, so it is OK to make this very large. 
 * Additional memory will be allocated if needed. */
#define LTABLE_SIZE 20000
static int
gdb_loadfile (clientData, interp, objc, objv)
  ClientData clientData;
  Tcl_Interp *interp;
  int objc;
  Tcl_Obj *CONST objv[];
{
  char *file, *widget;
  int linenumbers, ln, lnum, ltable_size;
  FILE *fp;
  char *ltable;
  struct symtab *symtab;
  struct linetable_entry *le;
  long mtime = 0;
  struct stat st;
  char line[10000], line_num_buf[18];
  int prefix_len_1, prefix_len_2, cur_prefix_len, widget_len;
  char *text_argv[8];
  Tcl_CmdInfo text_cmd;

 
  if (objc != 4)
    {
      Tcl_WrongNumArgs(interp, 1, objv, "widget filename linenumbers");
      return TCL_ERROR; 
    }

  widget = Tcl_GetStringFromObj (objv[1], NULL);
  if ( Tk_NameToWindow (interp, widget, Tk_MainWindow (interp)) == NULL)
    {
      return TCL_ERROR;
    }

  if (!Tcl_GetCommandInfo (interp, widget, &text_cmd))
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "Can't get widget command info",
			-1);
      return TCL_ERROR;
    }
  
  file  = Tcl_GetStringFromObj (objv[2], NULL);
  Tcl_GetBooleanFromObj (interp, objv[3], &linenumbers);

  symtab = full_lookup_symtab (file);
  if (!symtab)
    {
      Tcl_SetStringObj ( result_ptr->obj_ptr, "File not found in symtab", -1);
      return TCL_ERROR;
    }

  file = symtab_to_filename ( symtab );
  if ((fp = fopen ( file, "r" )) == NULL)
    {
      Tcl_SetStringObj ( result_ptr->obj_ptr, "Can't open file for reading",
			 -1);
      return TCL_ERROR;
    }

  if (stat (file, &st) < 0)
    {
      catch_errors (perror_with_name_wrapper, "gdbtk: get time stamp", "",
                    RETURN_MASK_ALL);
      return TCL_ERROR;
    }

  if (symtab && symtab->objfile && symtab->objfile->obfd)
      mtime = bfd_get_mtime(symtab->objfile->obfd);
  else if (exec_bfd)
      mtime = bfd_get_mtime(exec_bfd);
 
  if (mtime && mtime < st.st_mtime)
    {
      gdbtk_ignorable_warning("file_times",\
			      "Source file is more recent than executable.\n");
    }
  
  
  /* Source linenumbers don't appear to be in order, and a sort is */
  /* too slow so the fastest solution is just to allocate a huge */
  /* array and set the array entry for each linenumber */

  ltable_size = LTABLE_SIZE;
  ltable = (char *)xmalloc (LTABLE_SIZE);
  if (ltable == NULL)
    {
      Tcl_SetStringObj ( result_ptr->obj_ptr, "Out of memory.", -1);
      fclose (fp);
      return TCL_ERROR;
    }

  memset (ltable, 0, LTABLE_SIZE);

  if (symtab->linetable && symtab->linetable->nitems)
    {
      le = symtab->linetable->item;
      for (ln = symtab->linetable->nitems ;ln > 0; ln--, le++)
        {
          lnum = le->line >> 3;
          if (lnum >= ltable_size)
            {
              char *new_ltable;
              new_ltable = (char *)realloc (ltable, ltable_size*2);
              memset (new_ltable + ltable_size, 0, ltable_size);
              ltable_size *= 2;
              if (new_ltable == NULL)
                {
                  Tcl_SetStringObj ( result_ptr->obj_ptr, "Out of memory.",
				     -1);
                  free (ltable);
                  fclose (fp);
                  return TCL_ERROR;
                }
              ltable = new_ltable;
            }
          ltable[lnum] |= 1 << (le->line % 8);
        }
    }
      
  ln = 1;

  line[0] = '\t'; 
  text_argv[0] = widget;
  text_argv[1] = "insert";
  text_argv[2] = "end";
  text_argv[5] = line;
  text_argv[6] = "source_tag";
  text_argv[8] = NULL;
  
  if (linenumbers)
    {
      int found_carriage_return = 1;
      
      line_num_buf[1] = '\t';
       
      text_argv[3] = line_num_buf;
      
      while (fgets (line + 1, 9980, fp))
        {
	  char *p;

	  /* Look for DOS style \r\n endings, and if found,
	   * strip off the \r.  We assume (for the sake of
	   * speed) that ALL lines in the file have DOS endings,
	   * or none do.
	   */
	  
	  if (found_carriage_return) {
	    char *p;
	    
	    p = strrchr(line, '\0') - 2;
	    if (*p == '\r') {
	      *p = '\n';
	      *(p + 1) = '\0';
	    } else {
	      found_carriage_return = 0;
	    }
	  }
	  
          sprintf (line_num_buf+2, "%d", ln);
          if (ltable[ln >> 3] & (1 << (ln % 8)))
            {
	      line_num_buf[0] = '-';
              text_argv[4] = "break_rgn_tag";
            }
          else
            {
	      line_num_buf[0] = ' ';
              text_argv[4] = "";
            }

          text_cmd.proc(text_cmd.clientData, interp, 7, text_argv);
          ln++;
        }
    }
  else
    {
      int found_carriage_return = 1;
            
      while (fgets (line + 1, 9980, fp))
        {
	  if (found_carriage_return) {
	    char *p;
	    
	    p = strrchr(line, '\0') - 2;
	    if (*p == '\r') {
	      *p = '\n';
	      *(p + 1) = '\0';
	    } else {
	      found_carriage_return = 0;
	    }
	  }

          if (ltable[ln >> 3] & (1 << (ln % 8)))
            {
              text_argv[3] = "- ";
              text_argv[4] = "break_rgn_tag";
            }
          else
            {
              text_argv[3] = "  ";
              text_argv[4] = "";
            }

          text_cmd.proc(text_cmd.clientData, interp, 7, text_argv);
          ln++;
	}
    }

  free (ltable);
  fclose (fp);
  return TCL_OK;
}

/*
 *  This section contains commands for manipulation of breakpoints.
 */


/* set a breakpoint by source file and line number */
/* flags are as follows: */
/* least significant 2 bits are disposition, rest is */
/* type (normally 0).

   enum bptype {
   bp_breakpoint,                Normal breakpoint 
   bp_hardware_breakpoint,      Hardware assisted breakpoint
   }

   Disposition of breakpoint.  Ie: what to do after hitting it.
   enum bpdisp {
   del,                         Delete it
   del_at_next_stop,            Delete at next stop, whether hit or not
   disable,                     Disable it 
   donttouch                    Leave it alone 
   };
 */

/* This implements the tcl command "gdb_set_bp"
 * It sets breakpoints, and runs the Tcl command
 *     gdbtk_tcl_breakpoint create
 * to register the new breakpoint with the GUI.
 *
 * Tcl Arguments:
 *    filename: the file in which to set the breakpoint
 *    line:     the line number for the breakpoint
 *    type:     the type of the breakpoint
 *    thread:   optional thread number
 * Tcl Result:
 *    The return value of the call to gdbtk_tcl_breakpoint.
 */

static int
gdb_set_bp (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct symtab_and_line sal;
  int line, ret, thread = -1;
  struct breakpoint *b;
  char buf[64], *typestr;
  Tcl_DString cmd;
  enum bpdisp disp;

  if (objc != 4 && objc != 5)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
      "wrong number of args, should be \"filename line type [thread]\"", -1);
      return TCL_ERROR;
    }

  sal.symtab = full_lookup_symtab (Tcl_GetStringFromObj (objv[1], NULL));
  if (sal.symtab == NULL)
    return TCL_ERROR;

  if (Tcl_GetIntFromObj (interp, objv[2], &line) == TCL_ERROR)
    {
      result_ptr->flags = GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  typestr = Tcl_GetStringFromObj (objv[3], NULL);
  if (typestr == NULL)
    {
      result_ptr->flags = GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }
  if (strncmp (typestr, "temp", 4) == 0)
    disp = del;
  else if (strncmp (typestr, "normal", 6) == 0)
    disp = donttouch;
  else
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			"type must be \"temp\" or \"normal\"", -1);
      return TCL_ERROR;
    }

  if (objc == 5)
    {
      if (Tcl_GetIntFromObj (interp, objv[4], &thread) == TCL_ERROR)
	{
	  result_ptr->flags = GDBTK_IN_TCL_RESULT;
	  return TCL_ERROR;
	}
    }

  sal.line = line;
  if (!find_line_pc (sal.symtab, sal.line, &sal.pc))
    return TCL_ERROR;

  sal.section = find_pc_overlay (sal.pc);
  b = set_raw_breakpoint (sal);
  set_breakpoint_count (breakpoint_count + 1);
  b->number = breakpoint_count;
  b->type = bp_breakpoint;
  b->disposition = disp;
  b->thread = thread;

  /* FIXME: this won't work for duplicate basenames! */
  sprintf (buf, "%s:%d", basename (Tcl_GetStringFromObj (objv[1], NULL)),
	   line);
  b->addr_string = strsave (buf);

  /* now send notification command back to GUI */

  Tcl_DStringInit (&cmd);

  Tcl_DStringAppend (&cmd, "gdbtk_tcl_breakpoint create ", -1);
  sprintf (buf, "%d", b->number);
  Tcl_DStringAppendElement (&cmd, buf);
  sprintf (buf, "0x%lx", (long) sal.pc);
  Tcl_DStringAppendElement (&cmd, buf);
  Tcl_DStringAppendElement (&cmd, Tcl_GetStringFromObj (objv[2], NULL));
  Tcl_DStringAppendElement (&cmd, Tcl_GetStringFromObj (objv[1], NULL));
  Tcl_DStringAppendElement (&cmd, bpdisp[b->disposition]);
  sprintf (buf, "%d", b->enable);
  Tcl_DStringAppendElement (&cmd, buf);
  sprintf (buf, "%d", b->thread);
  Tcl_DStringAppendElement (&cmd, buf);


  ret = Tcl_Eval (interp, Tcl_DStringValue (&cmd));
  Tcl_DStringFree (&cmd);
  return ret;
}

/* This implements the tcl command "gdb_set_bp_addr"
 * It sets breakpoints, and runs the Tcl command
 *     gdbtk_tcl_breakpoint create
 * to register the new breakpoint with the GUI.
 *
 * Tcl Arguments:
 *    addr: the address at which to set the breakpoint
 *    type:     the type of the breakpoint
 *    thread:   optional thread number
 * Tcl Result:
 *    The return value of the call to gdbtk_tcl_breakpoint.
 */

static int
gdb_set_bp_addr (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];

{
  struct symtab_and_line sal;
  int line, ret, thread = -1;
  long addr;
  struct breakpoint *b;
  char *filename, *typestr, buf[64];
  Tcl_DString cmd;
  enum bpdisp disp;

  if (objc != 4 && objc != 3)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
	   "wrong number of args, should be \"address type [thread]\"", -1);
      return TCL_ERROR;
    }

  if (Tcl_GetLongFromObj (interp, objv[1], &addr) == TCL_ERROR)
    {
      result_ptr->flags = GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  typestr = Tcl_GetStringFromObj (objv[2], NULL);
  if (typestr == NULL)
    {
      result_ptr->flags = GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }
  if (strncmp (typestr, "temp", 4) == 0)
    disp = del;
  else if (strncmp (typestr, "normal", 6) == 0)
    disp = donttouch;
  else
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			"type must be \"temp\" or \"normal\"", -1);
      return TCL_ERROR;
    }

  if (objc == 4)
    {
      if (Tcl_GetIntFromObj (interp, objv[3], &thread) == TCL_ERROR)
	{
	  result_ptr->flags = GDBTK_IN_TCL_RESULT;
	  return TCL_ERROR;
	}
    }

  sal = find_pc_line (addr, 0);
  sal.pc = addr;
  b = set_raw_breakpoint (sal);
  set_breakpoint_count (breakpoint_count + 1);
  b->number = breakpoint_count;
  b->type = bp_breakpoint;
  b->disposition = disp;
  b->thread = thread;

  sprintf (buf, "*(0x%lx)", addr);
  b->addr_string = strsave (buf);

  /* now send notification command back to GUI */

  Tcl_DStringInit (&cmd);

  Tcl_DStringAppend (&cmd, "gdbtk_tcl_breakpoint create ", -1);
  sprintf (buf, "%d", b->number);
  Tcl_DStringAppendElement (&cmd, buf);
  sprintf (buf, "0x%lx", addr);
  Tcl_DStringAppendElement (&cmd, buf);
  sprintf (buf, "%d", b->line_number);
  Tcl_DStringAppendElement (&cmd, buf);

  filename = symtab_to_filename (sal.symtab);
  if (filename == NULL)
    filename = "";
  Tcl_DStringAppendElement (&cmd, filename);
  Tcl_DStringAppendElement (&cmd, bpdisp[b->disposition]);
  sprintf (buf, "%d", b->enable);
  Tcl_DStringAppendElement (&cmd, buf);
  sprintf (buf, "%d", b->thread);
  Tcl_DStringAppendElement (&cmd, buf);

  ret = Tcl_Eval (interp, Tcl_DStringValue (&cmd));
  Tcl_DStringFree (&cmd);
  return ret;
}

/* This implements the tcl command "gdb_find_bp_at_line"

 * Tcl Arguments:
 *    filename: the file in which to find the breakpoint
 *    line:     the line number for the breakpoint
 * Tcl Result:
 *    It returns a list of breakpoint numbers
 */

static int
gdb_find_bp_at_line (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];

{
  struct symtab *s;
  int line;
  struct breakpoint *b;
  extern struct breakpoint *breakpoint_chain;

  if (objc != 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename line");
      return TCL_ERROR;
    }

  s = full_lookup_symtab (Tcl_GetStringFromObj (objv[1], NULL));
  if (s == NULL)
    return TCL_ERROR;

  if (Tcl_GetIntFromObj (interp, objv[2], &line) == TCL_ERROR)
    {
      result_ptr->flags = GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);
  for (b = breakpoint_chain; b; b = b->next)
    if (b->line_number == line && !strcmp (b->source_file, s->filename))
      Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
				Tcl_NewIntObj (b->number));

  return TCL_OK;
}


/* This implements the tcl command "gdb_find_bp_at_addr"

 * Tcl Arguments:
 *    addr:     address
 * Tcl Result:
 *    It returns a list of breakpoint numbers
 */

static int
gdb_find_bp_at_addr (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];

{
  long addr;
  struct breakpoint *b;
  extern struct breakpoint *breakpoint_chain;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "address");
      return TCL_ERROR;
    }

  if (Tcl_GetLongFromObj (interp, objv[1], &addr) == TCL_ERROR)
    {
      result_ptr->flags = GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);
  for (b = breakpoint_chain; b; b = b->next)
    if (b->address == (CORE_ADDR) addr)
      Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
				Tcl_NewIntObj (b->number));

  return TCL_OK;
}

/* This implements the tcl command gdb_get_breakpoint_info


 * Tcl Arguments:
 *   breakpoint_number
 * Tcl Result:
 *   A list with {file, function, line_number, address, type, enabled?,
 *                disposition, ignore_count, {list_of_commands},
 *                thread, hit_count}
 */

static int
gdb_get_breakpoint_info (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct symtab_and_line sal;
  struct command_line *cmd;
  int bpnum;
  struct breakpoint *b;
  extern struct breakpoint *breakpoint_chain;
  char *funcname, *filename;
  struct symbol *sym;
  Tcl_Obj *new_obj;

  if (objc != 2)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr,
			"wrong number of args, should be \"breakpoint\"", -1);
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (NULL, objv[1], &bpnum) != TCL_OK)
    {
      result_ptr->flags = GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  for (b = breakpoint_chain; b; b = b->next)
    if (b->number == bpnum)
      break;

  if (!b || b->type != bp_breakpoint)
    {
      char err_buf[64];
      sprintf (err_buf, "Breakpoint #%d does not exist.", bpnum);
      Tcl_SetStringObj (result_ptr->obj_ptr, err_buf, -1);
      return TCL_ERROR;
    }

  sal = find_pc_line (b->address, 0);

  filename = symtab_to_filename (sal.symtab);
  if (filename == NULL)
    filename = "";

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewStringObj (filename, -1));

  funcname = pc_function_name (b->address);
  new_obj = Tcl_NewStringObj (funcname, -1);
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, new_obj);

  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewIntObj (b->line_number));
  sprintf_append_element_to_obj (result_ptr->obj_ptr, "0x%s",
				 paddr_nz (b->address));
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewStringObj (bptypes[b->type], -1));
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewBooleanObj (b->enable == enabled));
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewStringObj (bpdisp[b->disposition], -1));
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewIntObj (b->ignore_count));

  new_obj = Tcl_NewObj ();
  for (cmd = b->commands; cmd; cmd = cmd->next)
    Tcl_ListObjAppendElement (NULL, new_obj,
			      Tcl_NewStringObj (cmd->line, -1));
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, new_obj);

  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewStringObj (b->cond_string, -1));

  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewIntObj (b->thread));
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
			    Tcl_NewIntObj (b->hit_count));

  return TCL_OK;
}


/* This implements the tcl command gdb_get_breakpoint_list
 * It builds up a list of the current breakpoints.
 *
 * Tcl Arguments:
 *    None.
 * Tcl Result:
 *    A list of breakpoint numbers.
 */

static int
gdb_get_breakpoint_list (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct breakpoint *b;
  extern struct breakpoint *breakpoint_chain;
  Tcl_Obj *new_obj;

  if (objc != 1)
    error ("wrong number of args, none are allowed");

  for (b = breakpoint_chain; b; b = b->next)
    if (b->type == bp_breakpoint)
      {
	new_obj = Tcl_NewIntObj (b->number);
	Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, new_obj);
      }

  return TCL_OK;
}

/* The functions in this section deal with stacks and backtraces. */

/* This implements the tcl command gdb_stack.
 * It builds up a list of stack frames.
 *
 * Tcl Arguments:
 *    start  - starting stack frame
 *    count - number of frames to inspect
 * Tcl Result:
 *    A list of function names
 */

static int
gdb_stack (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  int start, count;

  if (objc < 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "start count");
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (NULL, objv[1], &start))
    {
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }
  if (Tcl_GetIntFromObj (NULL, objv[2], &count))
    {
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  if (target_has_stack)
    {
      gdb_result r;
      struct frame_info *top;
      struct frame_info *fi;

      /* Find the outermost frame */
      r  = GDB_get_current_frame (&fi);
      if (r != GDB_OK)
	return TCL_OK;

      while (fi != NULL)
        {
          top = fi;
	  r = GDB_get_prev_frame (fi, &fi);
	  if (r != GDB_OK)
	    fi = NULL;
        }

      /* top now points to the top (outermost frame) of the
         stack, so point it to the requested start */
      start = -start;
      r = GDB_find_relative_frame (top, &start, &top);
      
      result_ptr->obj_ptr = Tcl_NewListObj (0, NULL);
      if (r != GDB_OK)
	return TCL_OK;

      /* If start != 0, then we have asked to start outputting
         frames beyond the innermost stack frame */
      if (start == 0)
        {
          fi = top; 
          while (fi && count--)
            {
              get_frame_name (interp, result_ptr->obj_ptr, fi);
              r = GDB_get_next_frame (fi, &fi);
	      if (r != GDB_OK)
		break;
            }
        }
    }

  return TCL_OK;
}

/* A helper function for get_stack which adds information about
 * the stack frame FI to the caller's LIST.
 *
 * This is stolen from print_frame_info in stack.c.
 */
static void
get_frame_name (interp, list, fi)
     Tcl_Interp *interp;
     Tcl_Obj *list;
     struct frame_info *fi;
{
  struct symtab_and_line sal;
  struct symbol *func = NULL;
  register char *funname = 0;
  enum language funlang = language_unknown;
  Tcl_Obj *objv[1];

  if (frame_in_dummy (fi))
    {
      objv[0] = Tcl_NewStringObj ("<function called from gdb>\n", -1);
      Tcl_ListObjAppendElement (interp, list, objv[0]);
      return;
    }
  if (fi->signal_handler_caller)
    {
      objv[0] = Tcl_NewStringObj ("<signal handler called>\n", -1);
      Tcl_ListObjAppendElement (interp, list, objv[0]);
      return;
    }

  sal =
    find_pc_line (fi->pc,
		  fi->next != NULL
		  && !fi->next->signal_handler_caller
		  && !frame_in_dummy (fi->next));

  func = find_pc_function (fi->pc);
  if (func)
    {
      struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (fi->pc);
      if (msymbol != NULL
	  && (SYMBOL_VALUE_ADDRESS (msymbol)
	      > BLOCK_START (SYMBOL_BLOCK_VALUE (func))))
	{
	  func = 0;
	  funname = GDBTK_SYMBOL_SOURCE_NAME (msymbol);
	  funlang = SYMBOL_LANGUAGE (msymbol);
	}
      else
	{
	  funname = GDBTK_SYMBOL_SOURCE_NAME (func);
	  funlang = SYMBOL_LANGUAGE (func);
	}
    }
  else
    {
      struct minimal_symbol *msymbol = lookup_minimal_symbol_by_pc (fi->pc);
      if (msymbol != NULL)
	{
	  funname = GDBTK_SYMBOL_SOURCE_NAME (msymbol);
	  funlang = SYMBOL_LANGUAGE (msymbol);
	}
    }

  if (sal.symtab)
    {
      char *name = NULL;

      objv[0] = Tcl_NewStringObj (funname, -1);
      Tcl_ListObjAppendElement (interp, list, objv[0]);
    }
  else
    {
#if 0
      /* we have no convenient way to deal with this yet... */
      if (fi->pc != sal.pc || !sal.symtab)
	{
	  print_address_numeric (fi->pc, 1, gdb_stdout);
	  printf_filtered (" in ");
	}
      printf_symbol_filtered (gdb_stdout, funname ? funname : "??", funlang,
			      DMGL_ANSI);
#endif
      objv[0] = Tcl_NewStringObj (funname != NULL ? funname : "??", -1);
#ifdef PC_LOAD_SEGMENT
      /* If we couldn't print out function name but if can figure out what
         load segment this pc value is from, at least print out some info
         about its load segment. */
      if (!funname)
	{
	  Tcl_AppendStringsToObj (objv[0], " from ", PC_LOAD_SEGMENT (fi->pc),
				  (char *) NULL);
	}
#endif
#ifdef PC_SOLIB
      if (!funname)
	{
	  char *lib = PC_SOLIB (fi->pc);
	  if (lib)
	    {
	      Tcl_AppendStringsToObj (objv[0], " from ", lib, (char *) NULL);
	    }
	}
#endif
      Tcl_ListObjAppendElement (interp, list, objv[0]);
    }
}

/* This implements the tcl command gdb_selected_frame

 * Returns the address of the selected frame
 * frame.
 *
 * Arguments:
 *    None
 * Tcl Result:
 *    The currently selected frame's address
 */

static int
gdb_selected_frame (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  char frame[32];

  if (selected_frame == NULL)
    strcpy (frame, "");
  else
    sprintf (frame, "0x%s", paddr_nz (FRAME_FP (selected_frame)));

  Tcl_SetStringObj (result_ptr->obj_ptr, frame, -1);

  return TCL_OK;
}

/* This implements the tcl command gdb_selected_block
 *
 * Returns the start and end addresses of the innermost
 * block in the selected frame.
 *
 * Arguments:
 *    None
 * Tcl Result:
 *    The currently selected block's start and end addresses
 */

static int
gdb_selected_block (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  char start[32];
  char end[32];

  if (selected_frame == NULL)
    {
      strcpy (start, "");
      strcpy (end, "");
    }
  else
    {
      struct block *block;
      block = get_frame_block (selected_frame);
      sprintf (start, "0x%s", paddr_nz (BLOCK_START (block)));
      sprintf (end, "0x%s", paddr_nz (BLOCK_END (block)));
    }

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewStringObj (start, -1));
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
			    Tcl_NewStringObj (end, -1));

  return TCL_OK;
}

/* This implements the tcl command gdb_get_blocks
 *
 * Returns the start and end addresses for all blocks in
 * the selected frame.
 *
 * Arguments:
 *    None
 * Tcl Result:
 *    A list of all valid blocks in the selected_frame.
 */

static int
gdb_get_blocks (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct block *block;
  int nsyms, i, junk;
  struct symbol *sym;
  CORE_ADDR pc;

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);
  
  if (selected_frame != NULL)
    {
      block = get_frame_block (selected_frame);
      pc = get_frame_pc (selected_frame);
      while (block != 0)
	{
	  nsyms = BLOCK_NSYMS (block);
	  junk = 0;
	  for (i = 0; i < nsyms; i++)
	    {
	      sym = BLOCK_SYM (block, i);
	      switch (SYMBOL_CLASS (sym))
		{
		default:
		case LOC_UNDEF:		  /* catches errors        */
		case LOC_CONST:           /* constant              */
		case LOC_TYPEDEF:	  /* local typedef         */
		case LOC_LABEL:	          /* local label           */
		case LOC_BLOCK:	          /* local function        */
		case LOC_CONST_BYTES:	  /* loc. byte seq.        */
		case LOC_UNRESOLVED:      /* unresolved static     */
		case LOC_OPTIMIZED_OUT:   /* optimized out         */
		  junk = 1;
		  break;

		case LOC_ARG:		  /* argument              */
		case LOC_REF_ARG:	  /* reference arg         */
		case LOC_REGPARM:	  /* register arg          */
		case LOC_REGPARM_ADDR:    /* indirect register arg */
		case LOC_LOCAL_ARG:	  /* stack arg             */
		case LOC_BASEREG_ARG:	  /* basereg arg           */

		case LOC_LOCAL:	          /* stack local           */
		case LOC_BASEREG:	  /* basereg local         */
		case LOC_STATIC:	  /* static                */
		case LOC_REGISTER:        /* register              */
		  junk = 0;
		  break;
		}
	    }

	  /* If we found a block with locals in it, add it to the list. 
	     Note that the ranges of start and end address for blocks
	     are exclusive, so double-check against the PC */
	  
	  if (!junk && pc < BLOCK_END (block))
	    {
	      char addr[32];

	      Tcl_Obj *elt = Tcl_NewListObj (0, NULL);
	      sprintf (addr, "0x%s", paddr_nz (BLOCK_START (block)));
	      Tcl_ListObjAppendElement (interp, elt,
					Tcl_NewStringObj (addr, -1));
	      sprintf (addr, "0x%s", paddr_nz (BLOCK_END (block)));
	      Tcl_ListObjAppendElement (interp, elt,
					Tcl_NewStringObj (addr, -1));
	      Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr, elt);
	    }

	  if (BLOCK_FUNCTION (block))
	    break;
	  else
	    block = BLOCK_SUPERBLOCK (block);
	}
    }

  return TCL_OK;
}

/* This implements the tcl command gdb_block_vars.
 *
 * Returns all variables valid in the specified block.
 *
 * Arguments:
 *    The start and end addresses which identify the block.
 * Tcl Result:
 *    All variables defined in the given block.
 */

static int
gdb_block_vars (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  struct block *block;
  int nsyms, i;
  struct symbol *sym;
  CORE_ADDR start, end;

  if (objc < 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "startAddr endAddr");
      result_ptr->flags |= GDBTK_IN_TCL_RESULT;
      return TCL_ERROR;
    }

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);
  if (selected_frame == NULL)
    return TCL_OK;

  start = parse_and_eval_address (Tcl_GetStringFromObj (objv[1], NULL));
  end   = parse_and_eval_address (Tcl_GetStringFromObj (objv[2], NULL));
  
  block = get_frame_block (selected_frame);

  while (block != 0)
    {
      if (BLOCK_START (block) == start && BLOCK_END (block) == end)
	{
	  nsyms = BLOCK_NSYMS (block);
	  for (i = 0; i < nsyms; i++)
	    {
	      sym = BLOCK_SYM (block, i);
	      switch (SYMBOL_CLASS (sym))
		{
		case LOC_ARG:		  /* argument              */
		case LOC_REF_ARG:	  /* reference arg         */
		case LOC_REGPARM:	  /* register arg          */
		case LOC_REGPARM_ADDR:    /* indirect register arg */
		case LOC_LOCAL_ARG:	  /* stack arg             */
		case LOC_BASEREG_ARG:	  /* basereg arg           */
		case LOC_LOCAL:	          /* stack local           */
		case LOC_BASEREG:	  /* basereg local         */
		case LOC_STATIC:	  /* static                */
		case LOC_REGISTER:        /* register              */
		  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr,
					    Tcl_NewStringObj (SYMBOL_NAME (sym),
							      -1));
		  break;

		default:
		  break;
		}
	    }

	  return TCL_OK;
	}
      else if (BLOCK_FUNCTION (block))
	break;
      else
	block = BLOCK_SUPERBLOCK (block);
    }

  return TCL_OK;
}

/*
 * This section contains a bunch of miscellaneous utility commands
 */

/* This implements the tcl command gdb_path_conv

 * On Windows, it canonicalizes the pathname,
 * On Unix, it is a no op.
 *
 * Arguments:
 *    path
 * Tcl Result:
 *    The canonicalized path.
 */

static int
gdb_path_conv (clientData, interp, objc, objv)
     ClientData clientData;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
  if (objc != 2)
    error ("wrong # args");

#ifdef __CYGWIN__
  {
    char pathname[256], *ptr;

    cygwin32_conv_to_full_win32_path (Tcl_GetStringFromObj (objv[1], NULL),
				      pathname);
    for (ptr = pathname; *ptr; ptr++)
      {
	if (*ptr == '\\')
	  *ptr = '/';
      }
    Tcl_SetStringObj (result_ptr->obj_ptr, pathname, -1);
  }
#else
  Tcl_SetStringObj (result_ptr->obj_ptr, Tcl_GetStringFromObj (objv[1], NULL),
		    -1);
#endif

  return TCL_OK;
}

/*
 * This section has utility routines that are not Tcl commands.
 */

static int
perror_with_name_wrapper (args)
     PTR args;
{
  perror_with_name (args);
  return 1;
}

/* The lookup_symtab() in symtab.c doesn't work correctly */
/* It will not work will full pathnames and if multiple */
/* source files have the same basename, it will return */
/* the first one instead of the correct one.  This version */
/* also always makes sure symtab->fullname is set. */

static struct symtab *
full_lookup_symtab (file)
     char *file;
{
  struct symtab *st;
  struct objfile *objfile;
  char *bfile, *fullname;
  struct partial_symtab *pt;

  if (!file)
    return NULL;

  /* first try a direct lookup */
  st = lookup_symtab (file);
  if (st)
    {
      if (!st->fullname)
	symtab_to_filename (st);
      return st;
    }

  /* if the direct approach failed, try */
  /* looking up the basename and checking */
  /* all matches with the fullname */
  bfile = basename (file);
  ALL_SYMTABS (objfile, st)
  {
    if (!strcmp (bfile, basename (st->filename)))
      {
	if (!st->fullname)
	  fullname = symtab_to_filename (st);
	else
	  fullname = st->fullname;

	if (!strcmp (file, fullname))
	  return st;
      }
  }

  /* still no luck?  look at psymtabs */
  ALL_PSYMTABS (objfile, pt)
  {
    if (!strcmp (bfile, basename (pt->filename)))
      {
	st = PSYMTAB_TO_SYMTAB (pt);
	if (st)
	  {
	    fullname = symtab_to_filename (st);
	    if (!strcmp (file, fullname))
	      return st;
	  }
      }
  }
  return NULL;
}

/* Look for the function that contains PC and return the source
   (demangled) name for this function.

   If no symbol is found, it returns an empty string. In either
   case, memory is owned by gdb. Do not attempt to free it. */
char *
pc_function_name (pc)
     CORE_ADDR pc;
{
  struct symbol *sym;
  char *funcname = NULL;

  /* First lookup the address in the symbol table... */
  sym = find_pc_function (pc);
  if (sym != NULL)
    funcname = GDBTK_SYMBOL_SOURCE_NAME (sym);
  else
    {
      /* ... if that fails, look it up in the minimal symbols. */
      struct minimal_symbol *msym = NULL;

      msym = lookup_minimal_symbol_by_pc (pc);
      if (msym != NULL)
	funcname = GDBTK_SYMBOL_SOURCE_NAME (msym);
    }

  if (funcname == NULL)
    funcname = "";

  return funcname;
}

static void
setup_architecture_data ()
{
  /* don't trust REGISTER_BYTES to be zero. */
  old_regs = xmalloc (REGISTER_BYTES + 1);
  memset (old_regs, 0, REGISTER_BYTES + 1);
}

