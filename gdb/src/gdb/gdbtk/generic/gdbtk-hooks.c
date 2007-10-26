/* Startup code for Insight.

   Copyright 1994, 1995, 1996, 1997, 1998, 2000, 200, 2002, 2003, 2004 Free
   Software Foundation, Inc.

   Written by Stu Grossman <grossman@cygnus.com> of Cygnus Support.

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
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "tracepoint.h"
#include "demangle.h"
#include "gdb-events.h"
#include "top.h"
#include "annotate.h"
#include "cli/cli-decode.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* tcl header files includes varargs.h unless HAS_STDARG is defined,
   but gdb uses stdarg.h, so make sure HAS_STDARG is defined.  */
#define HAS_STDARG 1

#include <itcl.h>
#include <tcl.h>
#include <tk.h>
#include "guitcl.h"
#include "gdbtk.h"

#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "gdb_string.h"
#include "dis-asm.h"
#include "gdbcmd.h"


volatile int in_fputs = 0;

/* Set by gdb_stop, this flag informs x_event to tell its caller
   that it should forcibly detach from the target. */
int gdbtk_force_detach = 0;

/* From gdbtk-bp.c */
extern void gdbtk_create_breakpoint (int);
extern void gdbtk_delete_breakpoint (int);
extern void gdbtk_modify_breakpoint (int);
extern void gdbtk_create_tracepoint (int);
extern void gdbtk_delete_tracepoint (int);
extern void gdbtk_modify_tracepoint (int);

static void gdbtk_architecture_changed (void);
static void gdbtk_trace_find (char *arg, int from_tty);
static void gdbtk_trace_start_stop (int, int);
static void gdbtk_attach (void);
static void gdbtk_detach (void);
static void gdbtk_file_changed (char *);
static void gdbtk_exec_file_display (char *);
static void gdbtk_call_command (struct cmd_list_element *, char *, int);
static ptid_t gdbtk_wait (ptid_t, struct target_waitstatus *);
int x_event (int);
static int gdbtk_query (const char *, va_list);
static void gdbtk_warning (const char *, va_list);
static char *gdbtk_readline (char *);
static void gdbtk_readline_begin (char *format,...);
static void gdbtk_readline_end (void);
static void gdbtk_pre_add_symbol (const char *);
static void gdbtk_print_frame_info (struct symtab *, int, int, int);
static void gdbtk_post_add_symbol (void);
static void gdbtk_register_changed (int regno);
static void gdbtk_memory_changed (CORE_ADDR addr, int len);
static void gdbtk_selected_frame_changed (int);
static void gdbtk_context_change (int);
static void gdbtk_error_begin (void);
void report_error (void);
static void gdbtk_annotate_signal (void);
static void gdbtk_set_hook (struct cmd_list_element *cmdblk);

/*
 * gdbtk_fputs can't be static, because we need to call it in gdbtk.c.
 * See note there for details.
 */

long gdbtk_read (struct ui_file *, char *, long);
void gdbtk_fputs (const char *, struct ui_file *);
static int gdbtk_load_hash (const char *, unsigned long);

/*
 * gdbtk_add_hooks - add all the hooks to gdb.  This will get called by the
 * startup code to fill in the hooks needed by core gdb.
 */

void
gdbtk_add_hooks (void)
{
  static struct gdb_events handlers;

  /* Gdb event handlers */
  handlers.breakpoint_create = gdbtk_create_breakpoint;
  handlers.breakpoint_modify = gdbtk_modify_breakpoint;
  handlers.breakpoint_delete = gdbtk_delete_breakpoint;
  handlers.tracepoint_create = gdbtk_create_tracepoint;
  handlers.tracepoint_modify = gdbtk_modify_tracepoint;
  handlers.tracepoint_delete = gdbtk_delete_tracepoint;
  handlers.architecture_changed = gdbtk_architecture_changed;
  deprecated_set_gdb_event_hooks (&handlers);

  /* Hooks */
  deprecated_call_command_hook = gdbtk_call_command;
  deprecated_set_hook = gdbtk_set_hook;
  deprecated_readline_begin_hook = gdbtk_readline_begin;
  deprecated_readline_hook = gdbtk_readline;
  deprecated_readline_end_hook = gdbtk_readline_end;

  deprecated_print_frame_info_listing_hook = gdbtk_print_frame_info;
  deprecated_query_hook = gdbtk_query;
  deprecated_warning_hook = gdbtk_warning;

  deprecated_interactive_hook = gdbtk_interactive;
  deprecated_target_wait_hook = gdbtk_wait;
  deprecated_ui_load_progress_hook = gdbtk_load_hash;

  deprecated_ui_loop_hook = x_event;
  deprecated_pre_add_symbol_hook = gdbtk_pre_add_symbol;
  deprecated_post_add_symbol_hook = gdbtk_post_add_symbol;
  deprecated_file_changed_hook = gdbtk_file_changed;
  specify_exec_file_hook (gdbtk_exec_file_display);

  deprecated_trace_find_hook = gdbtk_trace_find;
  deprecated_trace_start_stop_hook = gdbtk_trace_start_stop;

  deprecated_attach_hook            = gdbtk_attach;
  deprecated_detach_hook            = gdbtk_detach;

  deprecated_register_changed_hook = gdbtk_register_changed;
  deprecated_memory_changed_hook = gdbtk_memory_changed;
  deprecated_selected_frame_level_changed_hook = gdbtk_selected_frame_changed;
  deprecated_context_hook = gdbtk_context_change;

  deprecated_error_begin_hook = gdbtk_error_begin;

  deprecated_annotate_signal_hook = gdbtk_annotate_signal;
  deprecated_annotate_signalled_hook = gdbtk_annotate_signal;
}

/* These control where to put the gdb output which is created by
   {f}printf_{un}filtered and friends.  gdbtk_fputs is the lowest
   level of these routines and capture all output from the rest of
   GDB.

   The reason to use the result_ptr rather than the gdbtk_interp's result
   directly is so that a call_wrapper invoked function can preserve its result
   across calls into Tcl which might be made in the course of the function's
   execution.

   * result_ptr->obj_ptr is where to accumulate the result.
   * GDBTK_TO_RESULT flag means the output goes to the gdbtk_tcl_fputs proc
   instead of to the result_ptr.
   * GDBTK_MAKES_LIST flag means add to the result as a list element.

*/

gdbtk_result *result_ptr = NULL;

/* If you want to restore an old value of result_ptr whenever cleanups
   are run, pass this function to make_cleanup, along with the value
   of result_ptr you'd like to reinstate.  */
void
gdbtk_restore_result_ptr (void *old_result_ptr)
{
  result_ptr = (gdbtk_result *) old_result_ptr;
}

/* This allows you to Tcl_Eval a tcl command which takes
   a command word, and then a single argument. */
int
gdbtk_two_elem_cmd (cmd_name, argv1)
     char *cmd_name;
     char *argv1;
{
  char *command;
  int result, flags_ptr, arg_len, cmd_len;

  arg_len = Tcl_ScanElement (argv1, &flags_ptr);
  cmd_len = strlen (cmd_name);
  /* APPLE LOCAL use xmalloc */
  command = xmalloc (arg_len + cmd_len + 2);
  strcpy (command, cmd_name);
  strcat (command, " ");

  Tcl_ConvertElement (argv1, command + cmd_len + 1, flags_ptr);

  result = Tcl_Eval (gdbtk_interp, command);
  if (result != TCL_OK)
    report_error ();
  free (command);
  return result;
}

struct ui_file *
gdbtk_fileopenin (void)
{
  struct ui_file *file = ui_file_new ();
  set_ui_file_read (file, gdbtk_read);
  return file;
}

struct ui_file *
gdbtk_fileopen (void)
{
  struct ui_file *file = ui_file_new ();
  set_ui_file_fputs (file, gdbtk_fputs);
  return file;
}

/* This handles input from the gdb console.
 */

long
gdbtk_read (struct ui_file *stream, char *buf, long sizeof_buf)
{
  int result;
  size_t actual_len;

  if (stream == gdb_stdtargin)
    {
      result = Tcl_Eval (gdbtk_interp, "gdbtk_console_read");
      if (result != TCL_OK)
	{
	  report_error ();
	  actual_len = 0;
	}
      else
        actual_len = strlen (gdbtk_interp->result);

      /* Truncate the string if it is too big for the caller's buffer.  */
      if (actual_len >= sizeof_buf)
	actual_len = sizeof_buf - 1;
      
      memcpy (buf, gdbtk_interp->result, actual_len);
      buf[actual_len] = '\0';
      return actual_len;
    }
  else
    {
      errno = EBADF;
      return 0;
    }
}


/* This handles all the output from gdb.  All the gdb printf_xxx functions
 * eventually end up here.  The output is either passed to the result_ptr
 * where it will go to the result of some gdbtk command, or passed to the
 * Tcl proc gdbtk_tcl_fputs (where it is usually just dumped to the console
 * window.
 *
 * The cases are:
 *
 * 1) result_ptr == NULL - This happens when some output comes from gdb which
 *    is not generated by a command in gdbtk-cmds, usually startup stuff.
 *    In this case we just route the data to gdbtk_tcl_fputs.
 * 2) The GDBTK_TO_RESULT flag is set - The result is supposed to go to Tcl.
 *    We place the data into the result_ptr, either as a string,
 *    or a list, depending whether the GDBTK_MAKES_LIST bit is set.
 * 3) The GDBTK_TO_RESULT flag is unset - We route the data to gdbtk_tcl_fputs
 *    UNLESS it was coming to gdb_stderr.  Then we place it in the result_ptr
 *    anyway, so it can be dealt with.
 *
 */

void
gdbtk_fputs (const char *ptr, struct ui_file *stream)
{
  if (gdbtk_disable_fputs)
    return;

  in_fputs = 1;

  if (stream == gdb_stdlog)
    gdbtk_two_elem_cmd ("gdbtk_tcl_fputs_log", (char *) ptr);
  else if (stream == gdb_stdtarg)
    gdbtk_two_elem_cmd ("gdbtk_tcl_fputs_target", (char *) ptr);
  else if (result_ptr != NULL)
    {
      if (result_ptr->flags & GDBTK_TO_RESULT)
	{
	  if (result_ptr->flags & GDBTK_MAKES_LIST)
	    Tcl_ListObjAppendElement (NULL, result_ptr->obj_ptr,
				      Tcl_NewStringObj ((char *) ptr, -1));
	  else
	    Tcl_AppendToObj (result_ptr->obj_ptr, (char *) ptr, -1);
	}
      else if (stream == gdb_stderr || result_ptr->flags & GDBTK_ERROR_ONLY)
	{
	  if (result_ptr->flags & GDBTK_ERROR_STARTED)
	    Tcl_AppendToObj (result_ptr->obj_ptr, (char *) ptr, -1);
	  else
	    {
	      Tcl_SetStringObj (result_ptr->obj_ptr, (char *) ptr, -1);
	      result_ptr->flags |= GDBTK_ERROR_STARTED;
	    }
	}
      else
	{
	  gdbtk_two_elem_cmd ("gdbtk_tcl_fputs", (char *) ptr);
	  if (result_ptr->flags & GDBTK_MAKES_LIST)
	    gdbtk_two_elem_cmd ("gdbtk_tcl_fputs", " ");
	}
    }
  else
    {
      gdbtk_two_elem_cmd ("gdbtk_tcl_fputs", (char *) ptr);
    }

  in_fputs = 0;
}

/*
 * This routes all warnings to the Tcl function "gdbtk_tcl_warning".
 */

static void
gdbtk_warning (const char *warning, va_list args)
{
  char *buf;
  xvasprintf (&buf, warning, args);
  gdbtk_two_elem_cmd ("gdbtk_tcl_warning", buf);
  free(buf);
}


/* Error-handling function for all hooks */
/* Hooks are not like tcl functions, they do not simply return */
/* TCL_OK or TCL_ERROR.  Also, the calling function typically */
/* doesn't care about errors in the hook functions.  Therefore */
/* after every hook function, report_error should be called. */
/* report_error can just call Tcl_BackgroundError() which will */
/* pop up a messagebox, or it can silently log the errors through */
/* the gdbtk dbug command.  */

void
report_error ()
{
  TclDebug ('E', Tcl_GetVar (gdbtk_interp, "errorInfo", TCL_GLOBAL_ONLY));
  /*  Tcl_BackgroundError(gdbtk_interp); */
}

/*
 * This routes all ignorable warnings to the Tcl function
 * "gdbtk_tcl_ignorable_warning".
 */

void
gdbtk_ignorable_warning (const char *class, const char *warning)
{
  char *buf;
  xasprintf (&buf, "gdbtk_tcl_ignorable_warning {%s} {%s}", class, warning);
  if (Tcl_Eval (gdbtk_interp, buf) != TCL_OK)
    report_error ();
  free(buf);
}

static void
gdbtk_register_changed (int regno)
{
  if (Tcl_Eval (gdbtk_interp, "gdbtk_register_changed") != TCL_OK)
    report_error ();
}

static void
gdbtk_memory_changed (CORE_ADDR addr, int len)
{
  if (Tcl_Eval (gdbtk_interp, "gdbtk_memory_changed") != TCL_OK)
    report_error ();
}


/* This hook is installed as the deprecated_ui_loop_hook, which is
 * used in several places to keep the gui alive (x_event runs gdbtk's
 * event loop). Users include:
 * - ser-tcp.c in socket reading code
 * - ser-unix.c in serial port reading code
 * - built-in simulators while executing
 *
 * x_event used to be called on SIGIO on the socket to the X server
 * for unix. Unfortunately, Linux does not deliver SIGIO, so we resort
 * to an elaborate scheme to keep the gui alive.
 *
 * For simulators and socket or serial connections on all hosts, we
 * rely on deprecated_ui_loop_hook (x_event) to keep us going. If the
 * user requests a detach (as a result of pressing the stop button --
 * see comments before gdb_stop in gdbtk-cmds.c), it sets the global
 * GDBTK_FORCE_DETACH, which is the value that x_event returns to it's
 * caller. It is up to the caller of x_event to act on this
 * information.
 *
 * For native unix, we simply set an interval timer which calls
 * x_event to allow the debugger to run through the Tcl event
 * loop. See comments before gdbtk_start_timer and gdb_stop_timer
 * in gdbtk.c.
 *
 * For native windows (and a few other targets, like the v850 ICE), we
 * rely on the target_wait loops to call deprecated_ui_loop_hook to
 * keep us alive.  */
int
x_event (int signo)
{
  static volatile int in_x_event = 0;
  static Tcl_Obj *varname = NULL;

  /* Do nor re-enter this code or enter it while collecting gdb output. */
  if (in_x_event || in_fputs)
    return 0;

  /* Also, only do things while the target is running (stops and redraws).
     FIXME: We wold like to at least redraw at other times but this is bundled
     together in the TCL_WINDOW_EVENTS group and we would also process user
     input.  We will have to prevent (unwanted)  user input to be generated
     in order to be able to redraw (removing this test here). */
  if (!running_now)
    return 0;

  in_x_event = 1;
  gdbtk_force_detach = 0;

  /* Process pending events */
  while (Tcl_DoOneEvent (TCL_DONT_WAIT | TCL_ALL_EVENTS) != 0)
    ;

  if (load_in_progress)
    {
      int val;
      if (varname == NULL)
	{
#if TCL_MAJOR_VERSION == 8 && (TCL_MINOR_VERSION < 1 || TCL_MINOR_VERSION > 2)
	  Tcl_Obj *varnamestrobj = Tcl_NewStringObj ("download_cancel_ok", -1);
	  varname = Tcl_ObjGetVar2 (gdbtk_interp, varnamestrobj, NULL, TCL_GLOBAL_ONLY);
#else
	  varname = Tcl_GetObjVar2 (gdbtk_interp, "download_cancel_ok", NULL, TCL_GLOBAL_ONLY);
#endif
	}
      if ((Tcl_GetIntFromObj (gdbtk_interp, varname, &val) == TCL_OK) && val)
	{
	  quit_flag = 1;
#ifdef REQUEST_QUIT
	  REQUEST_QUIT;
#else
	  if (immediate_quit)
	    quit ();
#endif
	}
    }
  in_x_event = 0;

  return gdbtk_force_detach;
}

/* VARARGS */
static void
gdbtk_readline_begin (char *format,...)
{
  va_list args;
  char *buf;

  va_start (args, format);
  xvasprintf (&buf, format, args);
  gdbtk_two_elem_cmd ("gdbtk_tcl_readline_begin", buf);
  free(buf);
}

static char *
gdbtk_readline (char *prompt)
{
  int result;

#ifdef _WIN32
  close_bfds ();
#endif

  result = gdbtk_two_elem_cmd ("gdbtk_tcl_readline", prompt);

  if (result == TCL_OK)
    {
      return (xstrdup (gdbtk_interp->result));
    }
  else
    {
      gdbtk_fputs (gdbtk_interp->result, gdb_stdout);
      gdbtk_fputs ("\n", gdb_stdout);
      return (NULL);
    }
}

static void
gdbtk_readline_end ()
{
  if (Tcl_Eval (gdbtk_interp, "gdbtk_tcl_readline_end") != TCL_OK)
    report_error ();
}

static void
gdbtk_call_command (struct cmd_list_element *cmdblk,
		    char *arg, int from_tty)
{
  struct cleanup *old_chain;

  old_chain = make_cleanup (null_cleanup, 0);
  running_now = 0;
  if (cmdblk->class == class_run || cmdblk->class == class_trace)
    {

      running_now = 1;
      if (!No_Update)
	Tcl_Eval (gdbtk_interp, "gdbtk_tcl_busy");
      cmd_func (cmdblk, arg, from_tty);
      running_now = 0;
      if (!No_Update)
	Tcl_Eval (gdbtk_interp, "gdbtk_tcl_idle");
    }
  else
    cmd_func (cmdblk, arg, from_tty);

  do_cleanups (old_chain);
}

/* Called after a `set' command succeeds.  Runs the Tcl hook
   `gdb_set_hook' with the full name of the variable (a Tcl list) as
   the first argument and the new value as the second argument.  */

static void
gdbtk_set_hook (struct cmd_list_element *cmdblk)
{
  Tcl_DString cmd;
  char *p;
  char *buffer = NULL;

  Tcl_DStringInit (&cmd);
  Tcl_DStringAppendElement (&cmd, "gdbtk_tcl_set_variable");

  /* Append variable name as sublist.  */
  Tcl_DStringStartSublist (&cmd);
  p = cmdblk->prefixname;
  while (p && *p)
    {
      char *q = strchr (p, ' ');
      char save = '\0';
      if (q)
	{
	  save = *q;
	  *q = '\0';
	}
      Tcl_DStringAppendElement (&cmd, p);
      if (q)
	*q = save;
      p = q + 1;
    }
  Tcl_DStringAppendElement (&cmd, cmdblk->name);
  Tcl_DStringEndSublist (&cmd);

  switch (cmdblk->var_type)
    {
    case var_string_noescape:
    case var_filename:
    case var_enum:
    case var_string:
      Tcl_DStringAppendElement (&cmd, (*(char **) cmdblk->var
				       ? *(char **) cmdblk->var
				       : "(null)"));
      break;

    case var_boolean:
      Tcl_DStringAppendElement (&cmd, (*(int *) cmdblk->var ? "1" : "0"));
      break;

    case var_uinteger:
    case var_zinteger:
      xasprintf (&buffer, "%u", *(unsigned int *) cmdblk->var);
      Tcl_DStringAppendElement (&cmd, buffer);
      break;

    case var_integer:
      xasprintf (&buffer, "%d", *(int *) cmdblk->var);
      Tcl_DStringAppendElement (&cmd, buffer);
      break;

    default:
      /* This case should already be trapped by the hook caller.  */
      Tcl_DStringAppendElement (&cmd, "error");
      break;
    }

  if (Tcl_Eval (gdbtk_interp, Tcl_DStringValue (&cmd)) != TCL_OK)
    report_error ();

  Tcl_DStringFree (&cmd);

  if (buffer != NULL)
    {
      free(buffer);
    }
}

int
gdbtk_load_hash (const char *section, unsigned long num)
{
  char *buf;
  xasprintf (&buf, "Download::download_hash %s %ld", section, num);
  if (Tcl_Eval (gdbtk_interp, buf) != TCL_OK)
    report_error ();
  free(buf);

  return atoi (gdbtk_interp->result);
}


/* This hook is called whenever we are ready to load a symbol file so that
   the UI can notify the user... */
static void
gdbtk_pre_add_symbol (const char *name)
{
  gdbtk_two_elem_cmd ("gdbtk_tcl_pre_add_symbol", (char *) name);
}

/* This hook is called whenever we finish loading a symbol file. */
static void
gdbtk_post_add_symbol ()
{
  if (Tcl_Eval (gdbtk_interp, "gdbtk_tcl_post_add_symbol") != TCL_OK)
    report_error ();
}

/* This hook function is called whenever we want to wait for the
   target.  */

static ptid_t
gdbtk_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  gdbtk_force_detach = 0;
  gdbtk_start_timer ();
  ptid = target_wait (ptid, ourstatus);
  gdbtk_stop_timer ();

  return ptid;
}

/*
 * This handles all queries from gdb.
 * The first argument is a printf style format statement, the rest are its
 * arguments.  The resultant formatted string is passed to the Tcl function
 * "gdbtk_tcl_query".
 * It returns the users response to the query, as well as putting the value
 * in the result field of the Tcl interpreter.
 */

static int
gdbtk_query (const char *query, va_list args)
{
  char *buf;
  long val;

  xvasprintf (&buf, query, args);
  gdbtk_two_elem_cmd ("gdbtk_tcl_query", buf);
  free(buf);

  val = atol (gdbtk_interp->result);
  return val;
}


static void
gdbtk_print_frame_info (struct symtab *s, int line,
			int stopline, int noerror)
{
}

/*
 * gdbtk_trace_find
 *
 * This is run by the trace_find_command.  arg is the argument that was passed
 * to that command, from_tty is 1 if the command was run from a tty, 0 if it
 * was run from a script.  It runs gdbtk_tcl_tfind_hook passing on these two
 * arguments.
 *
 */

static void
gdbtk_trace_find (char *arg, int from_tty)
{
  Tcl_Obj *cmdObj;

  cmdObj = Tcl_NewListObj (0, NULL);
  Tcl_ListObjAppendElement (gdbtk_interp, cmdObj,
			    Tcl_NewStringObj ("gdbtk_tcl_trace_find_hook", -1));
  Tcl_ListObjAppendElement (gdbtk_interp, cmdObj, Tcl_NewStringObj (arg, -1));
  Tcl_ListObjAppendElement (gdbtk_interp, cmdObj, Tcl_NewIntObj (from_tty));
#if TCL_MAJOR_VERSION == 8 && (TCL_MINOR_VERSION < 1 || TCL_MINOR_VERSION > 2)
  if (Tcl_GlobalEvalObj (gdbtk_interp, cmdObj) != TCL_OK)
    report_error ();
#else
  if (Tcl_EvalObj (gdbtk_interp, cmdObj, TCL_EVAL_GLOBAL) != TCL_OK)
    report_error ();
#endif
}

/*
 * gdbtk_trace_start_stop
 *
 * This is run by the trace_start_command and trace_stop_command.
 * The START variable determines which, 1 meaning trace_start was run,
 * 0 meaning trace_stop was run.
 *
 */

static void
gdbtk_trace_start_stop (int start, int from_tty)
{

  if (start)
    Tcl_GlobalEval (gdbtk_interp, "gdbtk_tcl_tstart");
  else
    Tcl_GlobalEval (gdbtk_interp, "gdbtk_tcl_tstop");

}

static void
gdbtk_selected_frame_changed (int level)
{
#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 1
  char *a;
  xasprintf (&a, "%d", level);
  Tcl_SetVar (gdbtk_interp, "gdb_selected_frame_level", a, TCL_GLOBAL_ONLY);
  xfree (a);
#else
  Tcl_SetVar2Ex (gdbtk_interp, "gdb_selected_frame_level", NULL,
		 Tcl_NewIntObj (level), TCL_GLOBAL_ONLY);
#endif
}

/* Called when the current thread changes. */
/* gdb_context is linked to the tcl variable "gdb_context_id" */
static void
gdbtk_context_change (int num)
{
  gdb_context = num;
}

/* Called from file_command */
static void
gdbtk_file_changed (char *filename)
{
  gdbtk_two_elem_cmd ("gdbtk_tcl_file_changed", filename);
}

/* Called from exec_file_command */
static void
gdbtk_exec_file_display (char *filename)
{
  gdbtk_two_elem_cmd ("gdbtk_tcl_exec_file_display", filename);
}

/* Called from error_begin, this hook is used to warn the gui
   about multi-line error messages */
static void
gdbtk_error_begin ()
{
  if (result_ptr != NULL)
    result_ptr->flags |= GDBTK_ERROR_ONLY;
}

/* notify GDBtk when a signal occurs */
static void
gdbtk_annotate_signal ()
{
  char *buf;

  /* Inform gui that the target has stopped. This is
     a necessary stop button evil. We don't want signal notification
     to interfere with the elaborate and painful stop button detach
     timeout. */
  Tcl_Eval (gdbtk_interp, "gdbtk_stop_idle_callback");

  xasprintf (&buf, "gdbtk_signal %s {%s}", target_signal_to_name (stop_signal),
	     target_signal_to_string (stop_signal));
  if (Tcl_Eval (gdbtk_interp, buf) != TCL_OK)
    report_error ();
  free(buf);
}

static void
gdbtk_attach ()
{
  if (Tcl_Eval (gdbtk_interp, "after idle \"update idletasks;gdbtk_attached\"") != TCL_OK)
    {
      report_error ();
    }
}

static void
gdbtk_detach ()
{
  if (Tcl_Eval (gdbtk_interp, "gdbtk_detached") != TCL_OK)
    {
      report_error ();
    }
}

/* Called from gdbarch_update_p whenever the architecture changes. */
static void
gdbtk_architecture_changed (void)
{
  Tcl_Eval (gdbtk_interp, "gdbtk_tcl_architecture_changed");
}
