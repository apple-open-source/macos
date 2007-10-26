/* Tcl/Tk command definitions for Insight.
   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2001, 2002, 2003, 2004
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
#include "inferior.h"
#include "source.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "demangle.h"
#include "linespec.h"
#include "tui/tui-file.h"
#include "top.h"
#include "annotate.h"
#include "block.h"
#include "dictionary.h"
#include "filenames.h"
#include "disasm.h"
#include "value.h"
#include "exceptions.h"

/* tcl header files includes varargs.h unless HAS_STDARG is defined,
   but gdb uses stdarg.h, so make sure HAS_STDARG is defined.  */
#define HAS_STDARG 1

#include <itcl.h>
#include <tcl.h>
#include <tk.h>

#include "guitcl.h"
#include "gdbtk.h"
#include "gdbtk-wrapper.h"
#include "gdbtk-cmds.h"

#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "gdb_string.h"
#include "dis-asm.h"
#include "gdbcmd.h"

#ifdef HAVE_CTYPE_H
#include <ctype.h>		/* for isprint() */
#endif

/* Various globals we reference.  */
extern char *source_path;

/* These two objects hold boolean true and false,
   and are shared by all the list objects that gdb_listfuncs
   returns. */

static Tcl_Obj *mangled, *not_mangled;

/* These two control how the GUI behaves when gdb is either tracing or loading.
   They are used in this file & gdbtk_hooks.c */

int No_Update = 0;
int load_in_progress = 0;

/* This Structure is used in gdb_disassemble_driver.
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

struct disassembly_client_data 
{
  FILE *fp;
  int file_opened_p;
  int widget_line_no;
  Tcl_Interp *interp;
  char *widget;
  Tcl_Obj *result_obj[3];
  const char *asm_argv[14];
  const char *source_argv[7];
  char *map_arr;
  Tcl_DString src_to_line_prefix;
  Tcl_DString pc_to_line_prefix;
  Tcl_DString line_to_pc_prefix;
  Tcl_CmdInfo cmd;
};

/* This variable determines where memory used for disassembly is read
   from.  See note in gdbtk.h for details.  */
/* NOTE: cagney/2003-09-08: This variable is unused.  */
int disassemble_from_exec = -1;

extern int gdb_variable_init (Tcl_Interp * interp);

/*
 * Declarations for routines exported from this file
 */

int Gdbtk_Init (Tcl_Interp * interp);

/*
 * Declarations for routines used only in this file.
 */

static int compare_lines (const PTR, const PTR);
static int comp_files (const void *, const void *);
static int gdb_clear_file (ClientData, Tcl_Interp * interp, int,
			   Tcl_Obj * CONST[]);
static int gdb_cmd (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_confirm_quit (ClientData, Tcl_Interp *, int,
			     Tcl_Obj * CONST[]);
static int gdb_entry_point (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_eval (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_find_file_command (ClientData, Tcl_Interp *, int,
				  Tcl_Obj * CONST objv[]);
static int gdb_force_quit (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_get_file_command (ClientData, Tcl_Interp *, int,
				 Tcl_Obj * CONST objv[]);
static int gdb_get_function_command (ClientData, Tcl_Interp *, int,
				     Tcl_Obj * CONST objv[]);
static int gdb_get_line_command (ClientData, Tcl_Interp *, int,
				 Tcl_Obj * CONST objv[]);
static int gdb_update_mem (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_set_mem (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_immediate_command (ClientData, Tcl_Interp *, int,
				  Tcl_Obj * CONST[]);
static int gdb_incr_addr (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_CA_to_TAS (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_listfiles (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_listfuncs (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_loadfile (ClientData, Tcl_Interp *, int,
			 Tcl_Obj * CONST objv[]);
static int gdb_load_disassembly (ClientData clientData, Tcl_Interp
				 * interp, int objc, Tcl_Obj * CONST objv[]);
static int gdb_get_inferior_args (ClientData clientData,
				  Tcl_Interp *interp,
				  int objc, Tcl_Obj * CONST objv[]);
static int gdb_set_inferior_args (ClientData clientData,
				  Tcl_Interp *interp,
				  int objc, Tcl_Obj * CONST objv[]);
static int gdb_load_info (ClientData, Tcl_Interp *, int,
			  Tcl_Obj * CONST objv[]);
static int gdb_loc (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_path_conv (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_prompt_command (ClientData, Tcl_Interp *, int,
			       Tcl_Obj * CONST objv[]);
static int gdb_restore_fputs (ClientData, Tcl_Interp *, int,
			      Tcl_Obj * CONST[]);
static int gdb_search (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST objv[]);
static int gdb_stop (ClientData, Tcl_Interp *, int, Tcl_Obj * CONST[]);
static int gdb_target_has_execution_command (ClientData,
					     Tcl_Interp *, int,
					     Tcl_Obj * CONST[]);
static void gdbtk_load_source (ClientData clientData,
			       struct symtab *symtab,
			       int start_line, int end_line);
static CORE_ADDR gdbtk_load_asm (ClientData clientData, CORE_ADDR pc,
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
char *get_prompt (void);
static int perror_with_name_wrapper (PTR args);
static int wrapped_call (PTR opaque_args);
static int hex2bin (const char *hex, char *bin, int count);
static int fromhex (int a);


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
Gdbtk_Init (Tcl_Interp *interp)
{
  Tcl_CreateObjCommand (interp, "gdb_cmd", gdbtk_call_wrapper, gdb_cmd, NULL);
  Tcl_CreateObjCommand (interp, "gdb_immediate", gdbtk_call_wrapper,
			gdb_immediate_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_loc", gdbtk_call_wrapper, gdb_loc, NULL);
  Tcl_CreateObjCommand (interp, "gdb_path_conv", gdbtk_call_wrapper, gdb_path_conv,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_listfiles", gdbtk_call_wrapper, gdb_listfiles,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_listfuncs", gdbtk_call_wrapper, gdb_listfuncs,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_entry_point", gdbtk_call_wrapper,
			gdb_entry_point, NULL);
  Tcl_CreateObjCommand (interp, "gdb_update_mem", gdbtk_call_wrapper, gdb_update_mem,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_set_mem", gdbtk_call_wrapper, gdb_set_mem,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_stop", gdbtk_call_wrapper, gdb_stop, NULL);
  Tcl_CreateObjCommand (interp, "gdb_restore_fputs", gdbtk_call_wrapper, gdb_restore_fputs,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_eval", gdbtk_call_wrapper, gdb_eval, NULL);
  Tcl_CreateObjCommand (interp, "gdb_incr_addr", gdbtk_call_wrapper, gdb_incr_addr, NULL);
  Tcl_CreateObjCommand (interp, "gdb_CA_to_TAS", gdbtk_call_wrapper, gdb_CA_to_TAS, NULL);
  Tcl_CreateObjCommand (interp, "gdb_clear_file", gdbtk_call_wrapper,
			gdb_clear_file, NULL);
  Tcl_CreateObjCommand (interp, "gdb_confirm_quit", gdbtk_call_wrapper,
			gdb_confirm_quit, NULL);
  Tcl_CreateObjCommand (interp, "gdb_force_quit", gdbtk_call_wrapper,
			gdb_force_quit, NULL);
  Tcl_CreateObjCommand (interp, "gdb_target_has_execution",
			gdbtk_call_wrapper,
			gdb_target_has_execution_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_load_info", gdbtk_call_wrapper, gdb_load_info,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_function", gdbtk_call_wrapper,
			gdb_get_function_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_line", gdbtk_call_wrapper,
			gdb_get_line_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_file", gdbtk_call_wrapper,
			gdb_get_file_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_prompt",
			gdbtk_call_wrapper, gdb_prompt_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_find_file",
			gdbtk_call_wrapper, gdb_find_file_command, NULL);
  Tcl_CreateObjCommand (interp, "gdb_loadfile", gdbtk_call_wrapper, gdb_loadfile,
			NULL);
  Tcl_CreateObjCommand (interp, "gdb_load_disassembly", gdbtk_call_wrapper,
			gdb_load_disassembly,  NULL);
  Tcl_CreateObjCommand (gdbtk_interp, "gdb_search", gdbtk_call_wrapper,
			gdb_search, NULL);
  Tcl_CreateObjCommand (interp, "gdb_get_inferior_args", gdbtk_call_wrapper,
			gdb_get_inferior_args, NULL);
  Tcl_CreateObjCommand (interp, "gdb_set_inferior_args", gdbtk_call_wrapper,
			gdb_set_inferior_args, NULL);

  /* gdb_context is used for debugging multiple threads or tasks */
  Tcl_LinkVar (interp, "gdb_context_id",
	       (char *) &gdb_context,
	       TCL_LINK_INT | TCL_LINK_READ_ONLY);

  /* Make gdb's notion of the pwd visible.  This is read-only because
     (1) it doesn't make sense to change it directly and (2) it is
     allocated using xmalloc and not Tcl_Alloc.  You might think we
     could just use the Tcl `pwd' command.  However, Tcl (erroneously,
     imho) maintains a cache of the current directory name, and
     doesn't provide a way for gdb to invalidate the cache.  */
  Tcl_LinkVar (interp, "gdb_current_directory",
	       (char *) &current_directory,
	       TCL_LINK_STRING | TCL_LINK_READ_ONLY);

  /* Current gdb source file search path.  This is read-only for
     reasons similar to those for gdb_current_directory.  */
  Tcl_LinkVar (interp, "gdb_source_path",
	       (char *) &source_path,
	       TCL_LINK_STRING | TCL_LINK_READ_ONLY);

  /* Init variable interface... */
  if (gdb_variable_init (interp) != TCL_OK)
    return TCL_ERROR;

  /* Init breakpoint module */
  if (Gdbtk_Breakpoint_Init (interp) != TCL_OK)
    return TCL_ERROR;

  /* Init stack module */
  if (Gdbtk_Stack_Init (interp) != TCL_OK)
    return TCL_ERROR;

  /* Init register module */
  if (Gdbtk_Register_Init (interp) != TCL_OK)
    return TCL_ERROR;

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
gdbtk_call_wrapper (ClientData clientData, Tcl_Interp *interp,
		    int objc, Tcl_Obj *CONST objv[])
{
  struct wrapped_call_args wrapped_args;
  gdbtk_result new_result, *old_result_ptr;
  int wrapped_returned_error = 0;

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
  else
    {
      /* If the wrapped call returned an error directly, then we don't
	 want to reset the result.  */
      wrapped_returned_error = wrapped_args.val == TCL_ERROR;
    }

  /* do not suppress any errors -- a remote target could have errored */
  load_in_progress = 0;

  /*
   * Now copy the result over to the true Tcl result.  If
   * GDBTK_TO_RESULT flag bit is set, this just copies a null object
   * over to the Tcl result, which is fine because we should reset the
   * result in this case anyway.  If the wrapped command returned an
   * error, then we assume that the result is already set correctly.
   */
  if ((result_ptr->flags & GDBTK_IN_TCL_RESULT) || wrapped_returned_error)
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
wrapped_call (PTR opaque_args)
{
  struct wrapped_call_args *args = (struct wrapped_call_args *) opaque_args;
  args->val = (*args->func) (args->func, args->interp, args->objc, args->objv);
  return 1;
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
gdb_clear_file (ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
  if (objc != 1)
    {
      Tcl_WrongNumArgs (interp, 1, objv, NULL);
      return TCL_ERROR;
    }

  if (! ptid_equal (inferior_ptid, null_ptid) && target_has_execution)
    {
      if (attach_flag)
	target_detach (NULL, 0);
      else
	target_kill ();
    }

  if (target_has_execution)
    pop_target ();

  delete_command (NULL, 0);
  exec_file_clear (0);
  symbol_file_clear (0);

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
gdb_confirm_quit (ClientData clientData, Tcl_Interp *interp,
		  int objc, Tcl_Obj *CONST objv[])
{
  int ret;

  if (objc != 1)
    {
      Tcl_WrongNumArgs (interp, 1, objv, NULL);
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
gdb_force_quit (ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
  if (objc != 1)
    {
      Tcl_WrongNumArgs (interp, 1, objv, NULL);
      return TCL_ERROR;
    }

  quit_force ((char *) NULL, 1);
  return TCL_OK;
}

/* Pressing the stop button on the source window should attempt to
 * stop the target. If, after some short time, this fails, a dialog
 * should appear allowing the user to detach.
 *
 * The global GDBTK_FORCE_DETACH is set when we wish to detach from a
 * target. This value is returned by deprecated_ui_loop_hook
 * (x_event), indicating to callers that they should detach.
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
gdb_stop (ClientData clientData, Tcl_Interp *interp,
	  int objc, Tcl_Obj *CONST objv[])
{
  int force = 0;
  char *s;

  if (objc > 1)
    {
      s = Tcl_GetStringFromObj (objv[1], NULL);
      if (strcmp (s, "detach") == 0)
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
 *     format - optional format character.  Valid chars are:
 *	o - octal
 *	x - hex
 *	d - decimal
 *	u - unsigned decimal
 *	t - binary
 *	f - float
 *	a - address
 *	c - char
 * Tcl Result:
 *     The result of the evaluation.
 */

static int
gdb_eval (ClientData clientData, Tcl_Interp *interp,
	  int objc, Tcl_Obj *CONST objv[])
{
  struct expression *expr;
  struct cleanup *old_chain = NULL;
  int format = 0;
  value_ptr val;
  struct ui_file *stb;
  long dummy;
  char *result;

  if (objc != 2 && objc != 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "expression [format]");
      return TCL_ERROR;
    }

  if (objc == 3)
    format = *(Tcl_GetStringFromObj (objv[2], NULL));

  expr = parse_expression (Tcl_GetStringFromObj (objv[1], NULL));
  old_chain = make_cleanup (free_current_contents, &expr);
  val = evaluate_expression (expr);

  /* "Print" the result of the expression evaluation. */
  stb = mem_fileopen ();
  make_cleanup_ui_file_delete (stb);
  val_print (value_type (val), value_contents (val),
	     value_embedded_offset (val), VALUE_ADDRESS (val),
	     stb, format, 0, 0, 0);
  result = ui_file_xstrdup (stb, &dummy);
  Tcl_SetObjResult (interp, Tcl_NewStringObj (result, -1));
  xfree (result);
  result_ptr->flags |= GDBTK_IN_TCL_RESULT;

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
gdb_cmd (ClientData clientData, Tcl_Interp *interp,
	 int objc, Tcl_Obj *CONST objv[])
{
  int from_tty = 0;

  if (objc < 2 || objc > 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "command ?from_tty?");
      return TCL_ERROR;
    }

  if (objc == 3)
    {
      if (Tcl_GetBooleanFromObj (NULL, objv[2], &from_tty) != TCL_OK)
	{
	  gdbtk_set_result (interp, "from_tty must be a boolean.");
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
gdb_immediate_command (ClientData clientData, Tcl_Interp *interp,
		       int objc, Tcl_Obj *CONST objv[])
{
  int from_tty = 0;

  if (objc < 2 || objc > 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "command ?from_tty?");
      return TCL_ERROR;
    }

  if (objc == 3)
    {
      if (Tcl_GetBooleanFromObj (NULL, objv[2], &from_tty) != TCL_OK)
	{
	  gdbtk_set_result (interp, "from_tty must be a boolean.");
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
gdb_prompt_command (ClientData clientData, Tcl_Interp *interp,
		    int objc, Tcl_Obj *CONST objv[])
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
gdb_target_has_execution_command (ClientData clientData, Tcl_Interp *interp,
				  int objc, Tcl_Obj *CONST objv[])
{
  int result = 0;

  if (target_has_execution && ! ptid_equal (inferior_ptid, null_ptid))
    result = 1;

  Tcl_SetBooleanObj (result_ptr->obj_ptr, result);
  return TCL_OK;
}

/* This implements the tcl command "gdb_get_inferior_args"

* Returns inferior command line arguments as a string
*
* Tcl Arguments:
*    None
* Tcl Result:
*    A string containing the inferior command line arguments
*/

static int
gdb_get_inferior_args (ClientData clientData, Tcl_Interp *interp,
		       int objc, Tcl_Obj *CONST objv[])
{
  if (objc != 1)
    {
      Tcl_WrongNumArgs (interp, 1, objv, NULL);
      return TCL_ERROR;
    }

  Tcl_SetStringObj (result_ptr->obj_ptr, get_inferior_args (), -1);
  return TCL_OK;
}

/* This implements the tcl command "gdb_set_inferior_args"

* Sets inferior command line arguments
*
* Tcl Arguments:
*    A string containing the inferior command line arguments
* Tcl Result:
*    None
*/

static int
gdb_set_inferior_args (ClientData clientData, Tcl_Interp *interp,
		       int objc, Tcl_Obj *CONST objv[])
{
  char *args;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "argument");
      return TCL_ERROR;
    }

  args = Tcl_GetStringFromObj (objv[1], NULL);

  /* The xstrdup/xfree stuff is so that we maintain a coherent picture
     for gdb.  I would expect the accessors to do this, but they
     don't.  */
  args = xstrdup (args);
  args = set_inferior_args (args);
  xfree (args);

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
gdb_load_info (ClientData clientData, Tcl_Interp *interp,
	       int objc, Tcl_Obj *CONST objv[])
{
  bfd *loadfile_bfd;
  struct cleanup *old_cleanups;
  asection *s;
  Tcl_Obj *ob[2];

  char *filename = Tcl_GetStringFromObj (objv[1], NULL);

  loadfile_bfd = bfd_openr (filename, gnutarget);
  if (loadfile_bfd == NULL)
    {
      gdbtk_set_result (interp, "Open of %s failed", filename);
      return TCL_ERROR;
    }
  old_cleanups = make_cleanup_bfd_close (loadfile_bfd);

  if (!bfd_check_format (loadfile_bfd, bfd_object))
    {
      gdbtk_set_result (interp, "Bad Object File");
      return TCL_ERROR;
    }

  Tcl_SetListObj (result_ptr->obj_ptr, 0, NULL);

  for (s = loadfile_bfd->sections; s; s = s->next)
    {
      if (s->flags & SEC_LOAD)
	{
	  bfd_size_type size = bfd_get_section_size (s);
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
gdb_get_line_command (ClientData clientData, Tcl_Interp *interp,
		      int objc, Tcl_Obj *CONST objv[])
{
  struct symtabs_and_lines sals;
  char *args, **canonical;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "linespec");
      return TCL_ERROR;
    }

  args = Tcl_GetStringFromObj (objv[1], NULL);
  sals = decode_line_1 (&args, 1, NULL, 0, &canonical, NULL);
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
gdb_get_file_command (ClientData clientData, Tcl_Interp *interp,
		      int objc, Tcl_Obj *CONST objv[])
{
  struct symtabs_and_lines sals;
  char *args, **canonical;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "linespec");
      return TCL_ERROR;
    }

  args = Tcl_GetStringFromObj (objv[1], NULL);
  sals = decode_line_1 (&args, 1, NULL, 0, &canonical, NULL);
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
gdb_get_function_command (ClientData clientData, Tcl_Interp *interp,
			  int objc, Tcl_Obj *CONST objv[])
{
  char *function;
  struct symtabs_and_lines sals;
  char *args, **canonical;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "linespec");
      return TCL_ERROR;
    }

  args = Tcl_GetStringFromObj (objv[1], NULL);
  sals = decode_line_1 (&args, 1, NULL, 0, &canonical, NULL);
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
*    The full path to the file, an empty string if the file was not
*    available or an error message if the file is not found in the symtab.
*/

static int
gdb_find_file_command (ClientData clientData, Tcl_Interp *interp,
		       int objc, Tcl_Obj *CONST objv[])
{
  struct symtab *st;
  char *filename, *fullname = NULL;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename");
      return TCL_ERROR;
    }

  filename = Tcl_GetStringFromObj (objv[1], NULL);

  /* Shortcut: There seems to be some mess in gdb dealing with
     files. While we should let gdb sort it out, it doesn't hurt
     to be a little defensive here.

     If the filename is already an absolute filename, just try
     to stat it. If it's not found, then ask gdb to find it for us. */
  if (IS_ABSOLUTE_PATH (filename))
    {
      struct stat st;
      const int status = stat (filename, &st);

      if (status == 0)
	{
	  if (S_ISREG (st.st_mode))
	    fullname = filename;
	}
    }
  else
    {
      /* Ask gdb to find the file for us. */
      st = lookup_symtab (filename);

      /* We should always get a symtab. */
      if (!st)
	{
	  gdbtk_set_result (interp, "File not found in symtab (2)");
	  return TCL_ERROR;
	}

      fullname =
	(st->fullname == NULL ? symtab_to_filename (st) : st->fullname);
    }
  
  /* We may not be able to open the file (not available). */
  if (fullname == NULL)
    {
      Tcl_SetStringObj (result_ptr->obj_ptr, "", -1);
      return TCL_OK;
    }

  Tcl_SetStringObj (result_ptr->obj_ptr, fullname, -1);

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
gdb_listfiles (ClientData clientData, Tcl_Interp *interp,
	       int objc, Tcl_Obj *CONST objv[])
{
  struct objfile *objfile;
  struct partial_symtab *psymtab;
  struct symtab *symtab;
  const char *lastfile, *pathname = NULL;
  const char **files;
  int files_size;
  int i, numfiles = 0, len = 0;

  files_size = 1000;
  files = (const char **) xmalloc (sizeof (char *) * files_size);

  if (objc > 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "?pathname?");
      return TCL_ERROR;
    }
  else if (objc == 2)
    pathname = Tcl_GetStringFromObj (objv[1], &len);

  ALL_PSYMTABS (objfile, psymtab)
    {
      if (numfiles == files_size)
	{
	  files_size = files_size * 2;
	  files = (const char **) xrealloc (files, sizeof (char *) * files_size);
	}
      if (psymtab->filename)
	{
	  if (!len || !strncmp (pathname, psymtab->filename, len)
	      || !strcmp (psymtab->filename, lbasename (psymtab->filename)))
	    {
	      files[numfiles++] = lbasename (psymtab->filename);
	    }
	}
    }

  ALL_SYMTABS (objfile, symtab)
    {
      if (numfiles == files_size)
	{
	  files_size = files_size * 2;
	  files = (const char **) xrealloc (files, sizeof (char *) * files_size);
	}
      if (symtab->filename && symtab->linetable && symtab->linetable->nitems)
	{
	  if (!len || !strncmp (pathname, symtab->filename, len)
	      || !strcmp (symtab->filename, lbasename (symtab->filename)))
	    {
	      files[numfiles++] = lbasename (symtab->filename);
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
comp_files (const void *file1, const void *file2)
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
gdb_search (ClientData clientData, Tcl_Interp *interp,
	    int objc, Tcl_Obj *CONST objv[])
{
  struct symbol_search *ss = NULL;
  struct symbol_search *p;
  struct cleanup *old_chain = NULL;
  Tcl_Obj *CONST * switch_objv;
  int index, switch_objc, i, show_files = 0;
  domain_enum space = 0;
  char *regexp;
  int static_only, nfiles;
  Tcl_Obj **file_list;
  char **files;
  static const char *search_options[] =
    {"functions", "variables", "types", (char *) NULL};
  static const char *switches[] =
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
      space = FUNCTIONS_DOMAIN;
      break;
    case SEARCH_VARIABLES:
      space = VARIABLES_DOMAIN;
      break;
    case SEARCH_TYPES:
      space = TYPES_DOMAIN;
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
      if ((p->symbol != NULL
	   && strncmp (SYMBOL_LINKAGE_NAME (p->symbol), "__tf", 4) != 0
	   && strncmp (SYMBOL_LINKAGE_NAME (p->symbol), "_GLOBAL_", 8) != 0)
	  || p->msymbol != NULL)
	{
	  elem = Tcl_NewListObj (0, NULL);

	  if (p->msymbol == NULL)
	    Tcl_ListObjAppendElement (interp, elem,
				      Tcl_NewStringObj (SYMBOL_PRINT_NAME (p->symbol), -1));
	  else
	    Tcl_ListObjAppendElement (interp, elem,
				      Tcl_NewStringObj (SYMBOL_PRINT_NAME (p->msymbol), -1));

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
  int i;
  struct dict_iterator iter;
  Tcl_Obj *funcVals[2];

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "file");
      return TCL_ERROR;
    }

  symtab = lookup_symtab (Tcl_GetStringFromObj (objv[1], NULL));
  if (!symtab)
    {
      gdbtk_set_result (interp, "No such file (%s)", 
			Tcl_GetStringFromObj (objv[1], NULL));
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
      ALL_BLOCK_SYMBOLS (b, iter, sym)
	{
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
		  funcVals[0] = Tcl_NewStringObj (DEPRECATED_SYMBOL_NAME (sym), -1);
		  funcVals[1] = not_mangled;
		}
	      Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
					Tcl_NewListObj (2, funcVals));
	    }
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
gdb_restore_fputs (ClientData clientData, Tcl_Interp *interp,
		   int objc, Tcl_Obj *CONST objv[])
{
  gdbtk_disable_fputs = 0;
  return TCL_OK;
}


/* This implements the tcl command gdb_load_disassembly
 *
 * Arguments:
 *    widget - the name of a text widget into which to load the data
 *    source_with_assm - must be "source" or "nosource"
 *    low_address - the CORE_ADDR from which to start disassembly
 *    ?hi_address? - the CORE_ADDR to which to disassemble, defaults
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
gdb_load_disassembly (ClientData clientData, Tcl_Interp *interp,
		      int objc, Tcl_Obj *CONST objv[])
{
  CORE_ADDR low, high, orig;
  struct disassembly_client_data client_data;
  int mixed_source_and_assembly, ret_val, i;
  char *arg_ptr;
  char *map_name;
  Tcl_WideInt waddr;

  if (objc != 6 && objc != 7)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "[source|nosource] map_arr index_prefix low_address ?hi_address");
      return TCL_ERROR;
    }

  client_data.widget = Tcl_GetStringFromObj (objv[1], NULL);
  if ( Tk_NameToWindow (interp, client_data.widget,
			Tk_MainWindow (interp)) == NULL)
    {
      gdbtk_set_result (interp, "Invalid widget name.");
      return TCL_ERROR;
    }

  if (!Tcl_GetCommandInfo (interp, client_data.widget, &client_data.cmd))
    {
      gdbtk_set_result (interp, "Can't get widget command info");
      return TCL_ERROR;
    }

  arg_ptr = Tcl_GetStringFromObj (objv[2], NULL);
  if (*arg_ptr == 's' && strcmp (arg_ptr, "source") == 0)
    mixed_source_and_assembly = 1;
  else if (*arg_ptr == 'n' && strcmp (arg_ptr, "nosource") == 0)
    mixed_source_and_assembly = 0;
  else
    {
      gdbtk_set_result (interp, "Second arg must be 'source' or 'nosource'");
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
      if (Tcl_UpVar (interp, "1", map_name, client_data.map_arr, 0) != TCL_OK)
	{
	  gdbtk_set_result (interp, "Can't link map array.");
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
  if (Tcl_GetWideIntFromObj (interp, objv[5], &waddr) != TCL_OK)
    return TCL_ERROR;
  low = waddr;

  orig = low;

  if (objc == 6)
    {
      if (find_pc_partial_function (low, NULL, &low, &high) == 0)
	error ("No function contains address 0x%s", core_addr_to_string (orig));
    }
  else
    {
      if (Tcl_GetWideIntFromObj (interp, objv[6], &waddr) != TCL_OK)
	return TCL_ERROR;
      high = waddr;
    }
  
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
      client_data.source_argv[6] = "source_tag2";
    }
  
  ret_val = gdb_disassemble_driver (low, high, mixed_source_and_assembly, 
				    (ClientData) &client_data,
				    gdbtk_load_source, gdbtk_load_asm);

  /* Now clean up the opened file, and the Tcl data structures */
  
  if (client_data.file_opened_p == 1) 
    fclose(client_data.fp);
  
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

  if (ret_val == TCL_OK) 
    {
      Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
				Tcl_NewStringObj (core_addr_to_string (low), -1));
      Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
				Tcl_NewStringObj (core_addr_to_string (high), -1));
    }
  return ret_val;
}

static void
gdbtk_load_source (ClientData clientData, struct symtab *symtab, 
		   int start_line, int end_line)
{
  struct disassembly_client_data *client_data =
    (struct disassembly_client_data *) clientData;
  char *buffer;
  int index_len;

  index_len = Tcl_DStringLength (&client_data->src_to_line_prefix);
  
  if (client_data->file_opened_p == 1)
    {
      const char **text_argv;
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
	  
	  if (found_carriage_return)
	    {
	      char *p = strrchr(line, '\0') - 2;
	      if (*p == '\r')
		{
		  *p = '\n';
		  *(p + 1) = '\0';
		}
	      else 
		found_carriage_return = 0;
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
	      
	      xasprintf (&buffer, "%d", client_data->widget_line_no);
	      
	      Tcl_SetVar2 (client_data->interp, client_data->map_arr,
			   Tcl_DStringValue (&client_data->src_to_line_prefix),
			   buffer, 0);
	      free(buffer);
	      
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
  else 
    {
      /* If we couldn't open the file, or got some prior error, just exit. */
      return;
    }
}


/* FIXME: cagney/2003-09-08: "di" is not used and unneeded.  */
static CORE_ADDR
gdbtk_load_asm (ClientData clientData, CORE_ADDR pc, 
		struct disassemble_info *di)
{
  struct disassembly_client_data * client_data
    = (struct disassembly_client_data *) clientData;
  const char **text_argv;
  int i, pc_to_line_len, line_to_pc_len;
  gdbtk_result new_result;
  int insn;
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

  deprecated_print_address_numeric (pc, 1, gdb_stdout);
  gdb_flush (gdb_stdout);

  result_ptr->obj_ptr = client_data->result_obj[1];
  print_address_symbolic (pc, gdb_stdout, 1, "\t");
  gdb_flush (gdb_stdout);

  result_ptr->obj_ptr = client_data->result_obj[2];
  /* FIXME: cagney/2003-09-08: This should use gdb_disassembly.  */
  insn = gdb_print_insn (pc, gdb_stdout);
  gdb_flush (gdb_stdout);

  client_data->widget_line_no++;

  text_argv[5] = Tcl_GetStringFromObj (client_data->result_obj[0], NULL);
  text_argv[7] = Tcl_GetStringFromObj (client_data->result_obj[1], NULL);
  text_argv[11] = Tcl_GetStringFromObj (client_data->result_obj[2], NULL);

  client_data->cmd.proc (client_data->cmd.clientData, 
			 client_data->interp, 14, text_argv);

  if (*client_data->map_arr != '\0')
    {
      char *buffer;
      
      /* Run the command, then add an entry to the map array in
	 the caller's scope. */
      
      Tcl_DStringAppend (&client_data->pc_to_line_prefix, core_addr_to_string (pc), -1);
      
      /* FIXME: Convert to Tcl_SetVar2Ex when we move to 8.2.  This
	 will allow us avoid converting widget_line_no into a string. */
      
      xasprintf (&buffer, "%d", client_data->widget_line_no);
      
      Tcl_SetVar2 (client_data->interp, client_data->map_arr,
		   Tcl_DStringValue (&client_data->pc_to_line_prefix),
		   buffer, 0);

      Tcl_DStringAppend (&client_data->line_to_pc_prefix, buffer, -1);
      

      Tcl_SetVar2 (client_data->interp, client_data->map_arr,
		   Tcl_DStringValue (&client_data->line_to_pc_prefix),
		   core_addr_to_string (pc), 0);
      
      /* Restore the prefixes to their initial state. */
      
      Tcl_DStringSetLength (&client_data->pc_to_line_prefix, pc_to_line_len);      
      Tcl_DStringSetLength (&client_data->line_to_pc_prefix, line_to_pc_len);      
      
      xfree (buffer);
    }
  
  do_cleanups (old_chain);

  return pc + insn;
}

static int
gdb_disassemble_driver (CORE_ADDR low, CORE_ADDR high, 
			int mixed_source_and_assembly,
			ClientData clientData, 
			void (*print_source_fn) (ClientData, struct symtab *, int, int),
			CORE_ADDR (*print_asm_fn) (ClientData, CORE_ADDR, struct disassemble_info *))
{
  CORE_ADDR pc;

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

	  /* Skip any end-of-function markers.  */
	  if (le[i].line == 0)
	    continue;

	  mle[newlines].line = le[i].line;
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
	      /* FIXME: cagney/2003-09-08: This entire function should
                 be replaced by gdb_disassembly.  */
	      pc = print_asm_fn (clientData, pc, NULL);
            }
        }
    }
  else
    {
    assembly_only:
      for (pc = low; pc < high; )
        {
          QUIT;
	  /* FIXME: cagney/2003-09-08: This entire function should be
	     replaced by gdb_disassembly.  */
	  pc = print_asm_fn (clientData, pc, NULL);
        }
    }

  return TCL_OK;
}

/* This will be passed to qsort to sort the results of the disassembly */

static int
compare_lines (const PTR mle1p, const PTR mle2p)
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
gdb_loc (ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  char *filename;
  struct symtab_and_line sal;
  char *fname;
  CORE_ADDR pc;

  if (objc == 1)
    {
      /* This function can be called, before the target is properly
         set-up, the following prevents an error, by trying to
         read_pc when there is no pc to read. It defaults pc, 
         before the target is connected to the entry point of the
         program */
      if (!target_has_registers)
        {
          pc = entry_point_address ();
          sal = find_pc_line (pc, 0);
        }  
      else if (deprecated_selected_frame
	       && (get_frame_pc (deprecated_selected_frame) != read_pc ()))
        {
          /* Note - this next line is not correct on all architectures.
	     For a graphical debugger we really want to highlight the 
	     assembly line that called the next function on the stack.
	     Many architectures have the next instruction saved as the
	     pc on the stack, so what happens is the next instruction 
	     is highlighted. FIXME */
	  pc = get_frame_pc (deprecated_selected_frame);
	  find_frame_sal (deprecated_selected_frame, &sal);
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
	  gdbtk_set_result (interp, "Ambiguous line spec", -1);
	  return TCL_ERROR;
	}
      resolve_sal_pc (&sal);
      pc = sal.pc;
    }
  else
    {
      Tcl_WrongNumArgs (interp, 1, objv, "?symbol?");
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
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, Tcl_NewStringObj (filename, -1));
  /* line number */
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, Tcl_NewIntObj (sal.line));
  /* PC in current frame */
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, 
			    Tcl_NewStringObj (core_addr_to_string (pc), -1));
  /* Real PC */
  Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr, 
			    Tcl_NewStringObj (core_addr_to_string (stop_pc), -1));
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

/* This implements the TCL command gdb_entry_point.  It returns the current
   entry point address.  */

static int
gdb_entry_point (ClientData clientData, Tcl_Interp *interp,
		 int objc, Tcl_Obj *CONST objv[])
{
  char *addrstr;

  /* If we have not yet loaded an exec file, then we have no
     entry point, so return an empty string.*/
  if ((int) current_target.to_stratum > (int) dummy_stratum)
    {
      addrstr = (char *)core_addr_to_string (entry_point_address ());
      Tcl_SetStringObj (result_ptr->obj_ptr, addrstr, -1);
    }
  else
    Tcl_SetStringObj (result_ptr->obj_ptr, "", -1);

  return TCL_OK;
}

/* Covert hex to binary. Stolen from remote.c,
   but added error handling */
static int
fromhex (int a)
{
  if (a >= '0' && a <= '9')
    return a - '0';
  else if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  else if (a >= 'A' && a <= 'F')
    return a - 'A' + 10;

  return -1;
}

static int
hex2bin (const char *hex, char *bin, int count)
{
  int i, m, n;
  int incr = 2;


  if (TARGET_BYTE_ORDER == BFD_ENDIAN_LITTLE)
    {
      /* need to read string in reverse */
      hex += count - 2;
      incr = -2;
    }

  for (i = 0; i < count; i += 2)
    {
      if (hex[0] == 0 || hex[1] == 0)
	{
	  /* Hex string is short, or of uneven length.
	     Return the count that has been converted so far. */
	  return i;
	}
      m = fromhex (hex[0]);
      n = fromhex (hex[1]);
      if (m == -1 || n == -1)
	return -1;
      *bin++ = m * 16 + n;
      hex += incr;
    }

  return i;
}

/* This implements the Tcl command 'gdb_set_mem', which
 * sets some chunk of memory.
 *
 * Arguments:
 *   gdb_set_mem addr hexstr len
 *
 *   addr:   address of data to set
 *   hexstr: ascii string of data to set
 *   len:    number of bytes of data to set
 */
static int
gdb_set_mem (ClientData clientData, Tcl_Interp *interp,
	     int objc, Tcl_Obj *CONST objv[])
{
  CORE_ADDR addr;
  gdb_byte buf[128];
  char *hexstr;
  int len, size;

  if (objc != 4)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "addr hex_data len");
      return TCL_ERROR;
    }

  /* Address to write */
  addr = string_to_core_addr (Tcl_GetStringFromObj (objv[1], NULL));

  /* String value to write: it's in hex */
  hexstr = Tcl_GetStringFromObj (objv[2], NULL);
  if (hexstr == NULL)
    return TCL_ERROR;

  /* Length of buf */
  if (Tcl_GetIntFromObj (interp, objv[3], &len) != TCL_OK)
    return TCL_ERROR;

  /* Convert hexstr to binary and write */
  if (hexstr[0] == '0' && hexstr[1] == 'x')
    hexstr += 2;
  size = hex2bin (hexstr, (char *) buf, strlen (hexstr));
  if (size < 0)
    {
      /* Error in input */
      gdbtk_set_result (interp, "Invalid hexadecimal input: \"0x%s\"", hexstr);
      return TCL_ERROR;
    }

  target_write_memory (addr, buf, len);
  return TCL_OK;
}

/* This implements the Tcl command 'gdb_update_mem', which 
 * updates a block of memory in the memory window
 *
 * Arguments:
 *   gdb_update_mem data addr form size nbytes bpr aschar
 *
 *   1 data: variable that holds table's data
 *   2 addr: address of data to dump
 *   3 mform: a char indicating format
 *   4 size: size of each element; 1,2,4, or 8 bytes
 *   5 nbytes: the number of bytes to read 
 *   6 bpr: bytes per row
 *   7 aschar: if present, an ASCII dump of the row is included.  ASCHAR
 *              used for unprintable characters.
 * 
 * Return:
 * a list of three integers: {border_col_width data_col_width ascii_col_width}
 * which can be used to set the table's column widths. */

static int
gdb_update_mem (ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
  long dummy;
  char index[20];
  CORE_ADDR addr;
  int nbytes, rnum, bpr;
  int size, asize, i, j, bc;
  int max_ascii_len, max_val_len, max_label_len;
  char format, aschar;
  char *data, *tmp;
  char buff[128], *mbuf, *mptr, *cptr, *bptr;
  struct ui_file *stb;
  struct type *val_type;
  struct cleanup *old_chain;

  if (objc < 7 || objc > 8)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "data addr format size bytes bytes_per_row ?ascii_char?");
      return TCL_ERROR;
    }

  /* Get table data and link to a local variable */
  data = Tcl_GetStringFromObj (objv[1], NULL);
  if (data == NULL)
    {
      gdbtk_set_result (interp, "could not get data variable");
      return TCL_ERROR;
    }

  if (Tcl_UpVar (interp, "1", data, "data", 0) != TCL_OK)
    {
      gdbtk_set_result (interp, "could not link table data");
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (interp, objv[4], &size) != TCL_OK)
    return TCL_ERROR;
  else if (size <= 0)
    {
      gdbtk_set_result (interp, "Invalid size, must be > 0");
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (interp, objv[5], &nbytes) != TCL_OK)
    return TCL_ERROR;
  else if (nbytes <= 0)
    {
      gdbtk_set_result (interp, "Invalid number of bytes, must be > 0");
      return TCL_ERROR;
    }

  if (Tcl_GetIntFromObj (interp, objv[6], &bpr) != TCL_OK)
    return TCL_ERROR;
  else if (bpr <= 0)
    {
      gdbtk_set_result (interp, "Invalid bytes per row, must be > 0");
      return TCL_ERROR;
    }

  tmp = Tcl_GetStringFromObj (objv[2], NULL);
  if (tmp == NULL)
    {
      gdbtk_set_result (interp, "could not get address");
      return TCL_ERROR;
    }
  addr = string_to_core_addr (tmp);

  format = *(Tcl_GetStringFromObj (objv[3], NULL));
  /* APPLE LOCAL use xmalloc */
  mbuf = (char *) xmalloc (nbytes + 32);
  if (!mbuf)
    {
      gdbtk_set_result (interp, "Out of memory.");
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

  if (objc == 8)
    aschar = *(Tcl_GetStringFromObj (objv[7], NULL));
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

  /* Open a memory ui_file that we can use to print memory values */
  stb = mem_fileopen ();
  old_chain = make_cleanup_ui_file_delete (stb);
  
  /* A little macro to do column indices. As a rule, given the current
     byte, i, of a total nbytes and the bytes per row, bpr, and the size of
     each cell, size, the row and column will be given by:

     row = i/bpr
     col = (i%bpr)/size
  */
#define INDEX(row,col) sprintf (index, "%d,%d",(row),(col))

  /* Fill in address labels */
  max_label_len = 0;
  for (i = 0; i < nbytes; i += bpr)
    {
      char s[130];
      sprintf (s, "%s", core_addr_to_string (addr + i));
      INDEX ((int) i/bpr, -1);
      Tcl_SetVar2 (interp, "data", index, s, 0);

      /* The tcl code in MemWin::update_addr used to track the size
	 of each cell. I don't see how these could change for any given
	 update, so we don't loop over all cells. We just note the first
	 size. */
      if (max_label_len == 0)
	max_label_len = strlen (s);
    }

  /* Fill in memory */
  max_val_len   = 0;		/* Ditto the above comments about max_label_len */
  max_ascii_len = 0;
  for (i = 0; i < nbytes; i += size)
    {
      INDEX ((int) i/bpr, (int) (i%bpr)/size);

      if (i >= rnum)
	{
	  /* Read fewer bytes than requested */
	  tmp = "N/A";

	  if (aschar)
	    {
	      for (j = 0; j < size; j++)
		*bptr++ = 'X';
	    }
	}
      else
	{
	  /* print memory to our uiout file and set the table's variable */
	  ui_file_rewind (stb);
	  print_scalar_formatted (mptr, val_type, format, asize, stb);
	  tmp = ui_file_xstrdup (stb, &dummy);

	  /* See comments above on max_*_len */
	  if (max_val_len == 0)
	    max_val_len = strlen (tmp);

	  if (aschar)
	    {
	      for (j = 0; j < size; j++)
		{
		  if (isprint (*cptr))
		    *bptr++ = *cptr++;
		  else
		    {
		      *bptr++ = aschar;
		      cptr++;;
		    }
		}
	    }
	}
      Tcl_SetVar2 (interp, "data", index, tmp, 0);

      mptr += size;
      bc += size;

      if (aschar && (bc >= bpr))
	{
	  /* end of row. Add it to the result and reset variables */
	  *bptr = '\000';
	  INDEX (i/bpr, bpr/size);
	  Tcl_SetVar2 (interp, "data", index, buff, 0);

	  /* See comments above on max_*_len */
	  if (max_ascii_len == 0)
	    max_ascii_len = strlen (buff);

	  bc = 0;
	  bptr = &buff[0];
	}
    }

  /* return max_*_len so that column widths can be set */
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr, Tcl_NewIntObj (max_label_len + 1));
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr, Tcl_NewIntObj (max_val_len + 1));
  Tcl_ListObjAppendElement (interp, result_ptr->obj_ptr, Tcl_NewIntObj (max_ascii_len + 1));
  do_cleanups (old_chain);
  xfree (mbuf);
  return TCL_OK;
#undef INDEX
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
gdb_loadfile (ClientData clientData, Tcl_Interp *interp, int objc,
	      Tcl_Obj *CONST objv[])
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
  const char *text_argv[9];
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
      gdbtk_set_result (interp, "Can't get widget command info");
      return TCL_ERROR;
    }
  
  file  = Tcl_GetStringFromObj (objv[2], NULL);
  Tcl_GetBooleanFromObj (interp, objv[3], &linenumbers);

  symtab = lookup_symtab (file);
  if (!symtab)
    {
      gdbtk_set_result (interp, "File not found in symtab");
      return TCL_ERROR;
    }

  file = symtab_to_filename ( symtab );
  if ((fp = fopen ( file, "r" )) == NULL)
    {
      gdbtk_set_result (interp, "Can't open file for reading");
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
      fclose (fp);
      gdbtk_set_result (interp, "Out of memory.");
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
                  free (ltable);
                  fclose (fp);
		  gdbtk_set_result (interp, "Out of memory.");
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
	  /* Look for DOS style \r\n endings, and if found,
	   * strip off the \r.  We assume (for the sake of
	   * speed) that ALL lines in the file have DOS endings,
	   * or none do.
	   */
	  
	  if (found_carriage_return)
	    {
	      char *p = strrchr(line, '\0') - 2;
	      if (*p == '\r')
		{
		  *p = '\n';
		  *(p + 1) = '\0';
		} 
	      else 
		found_carriage_return = 0;
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
	  if (found_carriage_return)
	    {
	      char *p = strrchr(line, '\0') - 2;
	      if (*p == '\r')
		{
		  *p = '\n';
		  *(p + 1) = '\0';
		} 
	      else
		found_carriage_return = 0;
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
gdb_path_conv (ClientData clientData, Tcl_Interp *interp,
	       int objc, Tcl_Obj *CONST objv[])
{
  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, NULL);
      return TCL_ERROR;
    }

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
perror_with_name_wrapper (PTR args)
{
  perror_with_name (args);
  return 1;
}

/* Look for the function that contains PC and return the source
   (demangled) name for this function.

   If no symbol is found, it returns an empty string. In either
   case, memory is owned by gdb. Do not attempt to free it. */
char *
pc_function_name (CORE_ADDR pc)
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

void
gdbtk_set_result (Tcl_Interp *interp, const char *fmt,...)
{
  va_list args;
  char *buf;

  va_start (args, fmt);
  xvasprintf (&buf, fmt, args);
  va_end (args);
  Tcl_SetObjResult (interp, Tcl_NewStringObj (buf, -1));
  xfree(buf);
}


/* This implements the tcl command 'gdb_incr_addr'.
 * It does address arithmetic and outputs a proper
 * hex string.  This was originally implemented
 * when tcl did not support 64-bit values, but we keep
 * it because it saves us from having to call incr 
 * followed by format to get the result in hex.
 * Also, it may be true in the future that CORE_ADDRs
 * will have their own ALU to deal properly with
 * architecture-specific address arithmetic.
 *
 * Tcl Arguments:
 *     addr   - CORE_ADDR
 *     number - optional number to add to the address
 *	default is 1.
 *
 * Tcl Result:
 *     hex string containing the result of addr + number
 */

static int
gdb_incr_addr (ClientData clientData, Tcl_Interp *interp,
	       int objc, Tcl_Obj *CONST objv[])
{
  CORE_ADDR address;
  int number = 1;

  if (objc != 2 && objc != 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "CORE_ADDR [number]");
      return TCL_ERROR;
    }

  address = string_to_core_addr (Tcl_GetStringFromObj (objv[1], NULL));

  if (objc == 3)
    {
      if (Tcl_GetIntFromObj (interp, objv[2], &number) != TCL_OK)
	return TCL_ERROR;
    }
  
  address += number;

  Tcl_SetStringObj (result_ptr->obj_ptr, (char *)core_addr_to_string (address), -1);
  
  return TCL_OK;
}

/* This implements the tcl command 'gdb_CAS_to_TAS'.
 * It takes a CORE_ADDR and outputs a string suitable
 * for displaying as the target address.
 *
 * Note that CORE_ADDRs are internal addresses which map
 * to target addresses in different ways depending on the 
 * architecture. The target address string is a user-readable
 * string may be quite different than the CORE_ADDR. For example,
 * a CORE_ADDR of 0x02001234 might indicate a data address of
 * 0x1234 which this function might someday output as something
 * like "D:1234".
 *
 * Tcl Arguments:
 *     address   - CORE_ADDR
 *
 * Tcl Result:
 *     string
 */

static int
gdb_CA_to_TAS (ClientData clientData, Tcl_Interp *interp,
	       int objc, Tcl_Obj *CONST objv[])
{
  CORE_ADDR address;
  Tcl_WideInt wide_addr;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "CORE_ADDR");
      return TCL_ERROR;
    }

  /* Read address into a wideint, which is the largest tcl supports
     then convert to a CORE_ADDR */
  if (Tcl_GetWideIntFromObj (interp, objv[1], &wide_addr) != TCL_OK)
    return TCL_ERROR;
  address = wide_addr;

  /* This is not really correct.  Using paddr_nz() will convert to hex and truncate 
     to 32-bits when required but will otherwise not do what we really want. */
  Tcl_SetStringObj (result_ptr->obj_ptr, paddr_nz (address), -1);

  return TCL_OK;
}

/* Another function that was removed in GDB and replaced
 * with something similar, but different enough to break
 * Insight.
 */
int find_and_open_source (struct objfile *objfile, const char *filename,
			  const char *dirname, char **fullname);

char *
symtab_to_filename (struct symtab *s)
{

  int r;

  if (!s)
    return NULL;

  /* Don't check s->fullname here, the file could have been 
     deleted/moved/..., look for it again */
  r = find_and_open_source (s->objfile, s->filename, s->dirname,
			    &s->fullname);
  if (r)
    close (r);

  if (s->fullname && *s->fullname)
      return s->fullname;
  return s->filename;
}
