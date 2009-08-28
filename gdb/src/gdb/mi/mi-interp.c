/* MI Interpreter Definitions and Commands for GDB, the GNU debugger.

   Copyright 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

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

#include <sys/time.h>

#include "defs.h"
#include "gdb_string.h"
#include "interps.h"
#include "event-top.h"
#include "event-loop.h"
#include "inferior.h"
#include "ui-out.h"
#include "top.h"
#include "exceptions.h"
#include "mi-main.h"
#include "mi-cmds.h"
#include "mi-out.h"
#include "mi-console.h"
/* APPLE LOCAL begin subroutine inlining  */
#include "mi-common.h"
#include "inlining.h"
/* APPLE LOCAL end subroutine inlining  */
#include "gdbthread.h"

struct mi_interp
{
  /* MI's output channels */
  struct ui_file *out;
  struct ui_file *err;
  struct ui_file *log;
  struct ui_file *targ;
  struct ui_file *event_channel;

  /* This is the interpreter for the mi... */
  struct interp *mi2_interp;
  struct interp *mi1_interp;
  struct interp *mi_interp;
};


/* APPLE LOCAL: These are the gdb events that we need to see when we
   are running the the mi interpreter.  */
struct gdb_events mi_async_hooks =
  {
    NULL, /* breakpoint_create */
    NULL, /* breakpoint_delete */
    NULL, /* breakpoint_modify */
    mi_async_breakpoint_resolve_event
  };
/* END APPLE LOCAL */

/* These are the interpreter setup, etc. functions for the MI interpreter */
static void mi_command_loop (int mi_version);

/* These are hooks that we put in place while doing interpreter_exec
   so we can report interesting things that happened "behind the mi's
   back" in this command */
static int mi_interp_query_hook (const char *ctlstr, va_list ap)
     ATTR_FORMAT (printf, 1, 0);

static void mi3_command_loop (void);
static void mi2_command_loop (void);
static void mi1_command_loop (void);

/* APPLE LOCAL: We need to preserve the mi0 interpreter, because
   that's what CodeWarrior 9 uses (though it just requests "mi".  */
static void mi0_command_loop (void);

static char *
mi_interp_read_one_line_hook (char *prompt, int repeat, char *anno);

static void mi_load_progress (const char *section_name,
			      unsigned long sent_so_far,
			      unsigned long total_section,
			      unsigned long total_sent,
			      unsigned long grand_total);


static void *
mi_interpreter_init (void)
{
  struct mi_interp *mi = XMALLOC (struct mi_interp);

  /* Why is this a part of the mi architecture? */

  mi_setup_architecture_data ();

  /* HACK: We need to force stdout/stderr to point at the console.  This avoids
     any potential side effects caused by legacy code that is still
     using the TUI / fputs_unfiltered_hook.  So we set up output channels for
     this now, and swap them in when we are run. */

  raw_stdout = stdio_fileopen (stdout);

  /* Create MI channels */
  mi->out = mi_console_file_new (raw_stdout, "~", '"');
  mi->err = mi_console_file_new (raw_stdout, "&", '"');
  mi->log = mi->err;
  mi->targ = mi_console_file_new (raw_stdout, "@", '"');
  mi->event_channel = mi_console_file_new (raw_stdout, "=", 0);

  return mi;
}

static int
mi_interpreter_resume (void *data)
{
  struct mi_interp *mi = data;
  /* As per hack note in mi_interpreter_init, swap in the output channels... */

  gdb_setup_readline ();

  /* These overwrite some of the initialization done in
     _intialize_event_loop.  */
  call_readline = gdb_readline2;
  input_handler = mi_execute_command_wrapper;
  add_file_handler (input_fd, stdin_event_handler, 0);
  async_command_editing_p = 0;
  /* FIXME: This is a total hack for now.  PB's use of the MI
     implicitly relies on a bug in the async support which allows
     asynchronous commands to leak through the commmand loop.  The bug
     involves (but is not limited to) the fact that sync_execution was
     erroneously initialized to 0.  Duplicate by initializing it thus
     here...  */
  sync_execution = 0;

  gdb_stdout = mi->out;
  /* Route error and log output through the MI */
  gdb_stderr = mi->err;
  gdb_stdlog = mi->log;
  /* Route target output through the MI. */
  gdb_stdtarg = mi->targ;

  /* Replace all the hooks that we know about.  There really needs to
     be a better way of doing this... */
  clear_interpreter_hooks ();

  deprecated_show_load_progress = mi_load_progress;
  print_frame_more_info_hook = mi_print_frame_more_info;

  /* If we're _the_ interpreter, take control. */
  if (current_interp_named_p (INTERP_MI1))
    deprecated_command_loop_hook = mi1_command_loop;
  else if (current_interp_named_p (INTERP_MI2))
    deprecated_command_loop_hook = mi2_command_loop;
  else if (current_interp_named_p (INTERP_MI3))
    deprecated_command_loop_hook = mi3_command_loop;
  else
    /* APPLE LOCAL: The default needs to be mi0,
       because that's what CodeWarrior expects.  */
    deprecated_command_loop_hook = mi0_command_loop;
  deprecated_set_gdb_event_hooks (&mi_async_hooks);

  return 1;
}

static int
mi_interpreter_suspend (void *data)
{
  gdb_disable_readline ();
  return 1;
}

static struct gdb_exception
mi_interpreter_exec (void *data, const char *command)
{
  char *tmp = alloca (strlen (command) + 1);
  strcpy (tmp, command);
  mi_execute_command_wrapper (tmp);
  return exception_none;
}

/* Never display the default gdb prompt in mi case.  */
static int
mi_interpreter_prompt_p (void *data)
{
  return 0;
}

enum mi_cmd_result
mi_cmd_interpreter_exec (char *command, char **argv, int argc)
{
  struct interp *interp_to_use;
  struct interp *old_interp;
  enum mi_cmd_result result = MI_CMD_DONE;
  int i;
  /* APPLE LOCAL: I am not sure why Keith left out the interp_quiet
     stuff.  */
  int old_quiet;

  if (argc < 2)
    {
      mi_error_message = xstrprintf ("mi_cmd_interpreter_exec: Usage: -interpreter-exec interp command");
      return MI_CMD_ERROR;
    }

  interp_to_use = interp_lookup (argv[0]);
  if (interp_to_use == NULL)
    {
      mi_error_message = xstrprintf ("mi_cmd_interpreter_exec: could not find interpreter \"%s\"", argv[0]);
      return MI_CMD_ERROR;
    }

  if (!interp_exec_p (interp_to_use))
    {
      mi_error_message = xstrprintf ("mi_cmd_interpreter_exec: interpreter \"%s\" does not support command execution",
				     argv[0]);
      return MI_CMD_ERROR;
    }
  
  old_quiet = interp_set_quiet (interp_to_use, 1);

  old_interp = interp_set (interp_to_use); 
  if (old_interp == NULL)
    {
      asprintf (&mi_error_message,
                "Could not switch to interpreter \"%s\".", argv[0]);
      return MI_CMD_ERROR;
    }  
  
  /* Set the global mi_interp.  We need this so that the hook functions
     can leave their results in the mi interpreter, rather than dumping
     them to the console.  */
  mi_interp = old_interp;

  /* Insert the MI out hooks, making sure to also call the
     interpreter's hooks if it has any. */
  /* KRS: We shouldn't need this... Events should be installed and
     they should just ALWAYS fire something out down the MI
     channel... */

  /* APPLE LOCAL: I disagree, how do we know the mi is going to always
     be the parent interpreter for whatever child interpreter we are
     running?  The only reason this works in the FSF version is that
     they don't actually switch interpreters, they just hack the
     cli_exec command so it knows how to set just enough of itself not
     to get in the mi's way, which seems a little hacky to me.  */

  mi_insert_notify_hooks ();

  /* Now run the code... */

  for (i = 1; i < argc; i++)
    {
      char *buff = NULL;
      /* Do this in a cleaner way...  We want to force execution to be
         asynchronous for commands that run the target.  */
      if (target_can_async_p () && (strncmp (argv[0], "console", 7) == 0))
	{
	  int len = strlen (argv[i]);
	  buff = xmalloc (len + 2);
	  memcpy (buff, argv[i], len);
	  buff[len] = '&';
	  buff[len + 1] = '\0';
	}

      /* We had to set sync_execution = 0 for the mi (well really for Project
         Builder's use of the mi - particularly so interrupting would work.
         But for console commands to work, we need to initialize it to 1 -
         since that is what the cli expects - before running the command,
         and then set it back to 0 when we are done. */
      sync_execution = 1;
      {
	struct gdb_exception e;
	mi_interp_exec_cmd_did_run = 0;
	e = interp_exec (interp_to_use, argv[i]);
	if (e.reason < 0)
	  {
	    mi_error_message = xstrdup (e.message);
	    result = MI_CMD_ERROR;
	    break;
	  }
      }
      xfree (buff);
      do_exec_error_cleanups (ALL_CLEANUPS);
      sync_execution = 0;
    }

  /* APPLE LOCAL: Now do the switch...   The FSF code was rewritten to
     assume the cli's execute proc would know how to run code under the
     cli without setting the interpreter, but this seems weak to me.  
     So I backed out that change in cli-interp.c, and put the interp_set
     back here.  */

  interp_set (old_interp);
  mi_interp = NULL;
  
  mi_remove_notify_hooks ();
  interp_set_quiet (interp_to_use, old_quiet);
  
  /* APPLE LOCAL begin subroutine inlining  */

  if (argc >= 2
      && strcmp (argv[0], "console-quoted") == 0
      && strcmp (argv[1], "step") ==  0
      && stepping_into_inlined_subroutine)
    {
      stop_step = 1;
      if (current_command_token)
	fputs_unfiltered (current_command_token, raw_stdout);
      fputs_unfiltered ("^running\n", raw_stdout);
      
      ui_out_field_string (uiout, "reason",
			   async_reason_lookup
			   (EXEC_ASYNC_END_STEPPING_RANGE));

      ui_out_print_annotation_int (uiout, 0, "thread-id",
				   pid_to_thread_id (inferior_ptid));
    }

  /* APPLE LOCAL end subroutine inlining  */

  /* Okay, now let's see if the command set the inferior going...
     Tricky point - have to do this AFTER resetting the interpreter, since
     changing the interpreter will clear out all the continuations for
     that interpreter... */
  
  /* APPLE LOCAL: The FSF version leaves out the 
     mi_dont_register_continuation.  Maybe this hadn't been added yet when
     they adopted the code.  */
  
  if (target_can_async_p () && target_executing
      && !mi_dont_register_continuation)
    {
      struct mi_continuation_arg *cont_args =
        mi_setup_continuation_arg (NULL);
      
      if (current_command_token)
        fputs_unfiltered (current_command_token, raw_stdout);
      
      fputs_unfiltered ("^running\n", raw_stdout);
      add_continuation (mi_interpreter_exec_continuation,
                        (void *) cont_args);
    }  

  return result;
}

/* APPLE LOCAL - For reasons I don't really understand, the FSF version
   didn't take this function.  I find it really useful for chasing down
   MI bugs, 'cause it means I don't have to work all the time in the MI.
   So I am putting it back... */

enum mi_cmd_result
mi_cmd_interpreter_set (char *command, char **argv, int argc)
{
  struct interp *interp;
  
  if (argc != 1)
    {
      asprintf (&mi_error_message, "mi_cmd_interpreter_set: "
		"wrong #of args, should be 1");
      return MI_CMD_ERROR;
    }
  interp = interp_lookup (argv[0]);
  if (interp == NULL)
    {
      asprintf (&mi_error_message, "mi_cmd_interpreter_set: "
		"could not find interpreter %s", argv[0]);
      return MI_CMD_ERROR;
    }
  
  if (interp_set (interp) == NULL)
    {
      asprintf (&mi_error_message, "mi_cmd_interpreter_set: "
		"error setting interpreter %s", argv[0]);
      return MI_CMD_ERROR;
    }

  return MI_CMD_DONE;
}

/* This implements the "interpreter complete command" which takes an
   interpreter, a command string, and optionally a cursor position 
   within the command, and completes the string based on that interpreter's
   completion function.  */

enum mi_cmd_result 
mi_cmd_interpreter_complete (char *command, char **argv, int argc)
{
  struct interp *interp_to_use;
  int cursor;
  int limit = 200;
  
  if (argc < 2 || argc > 3)
    {
      asprintf (&mi_error_message, 
		"Wrong # or arguments, should be \"%s interp command <cursor>\".",
		command);
      return MI_CMD_ERROR;
    }

  interp_to_use = interp_lookup (argv[0]);
  if (interp_to_use == NULL)
    {
      asprintf (&mi_error_message,
		"Could not find interpreter \"%s\".", argv[0]);
      return MI_CMD_ERROR;
    }
  
  if (argc == 3)
    {
      cursor = atoi (argv[2]);
    }
  else
    {
      cursor = strlen (argv[1]);
    }

  if (interp_complete (interp_to_use, argv[1], argv[1], cursor, limit) == 0)
      return MI_CMD_ERROR;
  else
    return MI_CMD_DONE;

}

/* APPLE LOCAL: FIXME - Keith removed all the mi hooks.  The 
   reason is he posits that all this work can be done with the
   gdb_events.  I am going to leave them in till this is proved.
   SO... FIXME: See if this really can be done with events.  */

/*
 * mi_insert_notify_hooks - This inserts a number of hooks that are meant to produce
 * async-notify ("=") MI messages while running commands in another interpreter
 * using mi_interpreter_exec.  The canonical use for this is to allow access to
 * the gdb CLI interpreter from within the MI, while still producing MI style output
 * when actions in the CLI command change gdb's state. 
*/

void
mi_insert_notify_hooks (void)
{
  deprecated_create_breakpoint_hook = mi_interp_create_breakpoint_hook;
  deprecated_delete_breakpoint_hook = mi_interp_delete_breakpoint_hook;
  deprecated_modify_breakpoint_hook = mi_interp_modify_breakpoint_hook;

  frame_changed_hook = mi_interp_frame_changed_hook;
  stack_changed_hook = mi_interp_stack_changed_hook;
  deprecated_context_hook = mi_interp_context_hook;

  /* command_line_input_hook = mi_interp_command_line_input; */
  deprecated_query_hook = mi_interp_query_hook;
  command_line_input_hook = mi_interp_read_one_line_hook;

  if (target_can_async_p ())
    {
      stepping_command_hook = mi_interp_stepping_command_hook;
      continue_command_hook = mi_interp_continue_command_hook;
    }
  else
    {
      stepping_command_hook = mi_interp_sync_stepping_command_hook;
      continue_command_hook = mi_interp_sync_continue_command_hook;
    }

  run_command_hook = mi_interp_run_command_hook;
  hand_call_function_hook = mi_interp_hand_call_function_hook;

}

void
mi_remove_notify_hooks ()
{
  deprecated_create_breakpoint_hook = NULL;
  deprecated_delete_breakpoint_hook = NULL;
  deprecated_modify_breakpoint_hook = NULL;

  frame_changed_hook = NULL;
  stack_changed_hook = NULL;
  deprecated_context_hook = NULL;

  /* command_line_input_hook = NULL; */
  deprecated_query_hook = NULL;
  command_line_input_hook = NULL;

  stepping_command_hook = NULL;
  continue_command_hook = NULL;
  run_command_hook = NULL;

  hand_call_function_hook = NULL;

  /* If we ran the target in sync mode, we will have set the
     annotation printer to "route_through_mi".  Undo that here.  */
  ui_out_set_annotation_printer (NULL);
     
}

int
mi_interp_query_hook (const char *ctlstr, va_list ap)
{
  return 1;
}

static char *
mi_interp_read_one_line_hook (char *prompt, int repeat, char *anno)
{
  static char buff[256];
  
  if (strlen (prompt) > 200)
    internal_error (__FILE__, __LINE__,
		    "Prompt \"%s\" ridiculously long.", prompt);

  sprintf (buff, "read-one-line,prompt=\"%s\"", prompt);
  mi_output_async_notification (buff);
  
  (void) fgets(buff, sizeof(buff), stdin);
  buff[(strlen(buff) - 1)] = 0;
  
  return buff;
  
}

static void
mi0_command_loop (void)
{
  mi_command_loop (0);
}

static void
mi1_command_loop (void)
{
  mi_command_loop (1);
}

static void
mi2_command_loop (void)
{
  mi_command_loop (2);
}

static void
mi3_command_loop (void)
{
  mi_command_loop (3);
}

static void
mi_command_loop (int mi_version)
{
  /* HACK: Force stdout/stderr to point at the console.  This avoids
     any potential side effects caused by legacy code that is still
     using the TUI / fputs_unfiltered_hook */
  raw_stdout = stdio_fileopen (stdout);
  /* Route normal output through the MIx */
  gdb_stdout = mi_console_file_new (raw_stdout, "~", '"');
  /* Route error and log output through the MI */
  gdb_stderr = mi_console_file_new (raw_stdout, "&", '"');
  gdb_stdlog = gdb_stderr;
  /* Route target output through the MI. */
  gdb_stdtarg = mi_console_file_new (raw_stdout, "@", '"');
  /* APPLE LOCAL: Don't make a new uiout.  There's already one in
     the mi_interp we are starting up, and we need to use that one
     or we won't get hook messages from interpreter-exec.  */
#if 0
  /* HACK: Poke the ui_out table directly.  Should we be creating a
     mi_out object wired up to the above gdb_stdout / gdb_stderr? */
  uiout = mi_out_new (mi_version);
#endif
  /* HACK: Override any other interpreter hooks.  We need to create a
     real event table and pass in that. */
  deprecated_init_ui_hook = 0;
  /* deprecated_command_loop_hook = 0; */
  deprecated_print_frame_info_listing_hook = 0;
  deprecated_query_hook = 0;
  deprecated_warning_hook = 0;
  deprecated_create_breakpoint_hook = 0;
  deprecated_delete_breakpoint_hook = 0;
  deprecated_modify_breakpoint_hook = 0;
  deprecated_interactive_hook = 0;
  deprecated_registers_changed_hook = 0;
  deprecated_readline_begin_hook = 0;
  deprecated_readline_hook = 0;
  deprecated_readline_end_hook = 0;
  deprecated_register_changed_hook = 0;
  deprecated_memory_changed_hook = 0;
  deprecated_context_hook = 0;
  deprecated_target_wait_hook = 0;
  deprecated_call_command_hook = 0;
  deprecated_error_hook = 0;
  deprecated_error_begin_hook = 0;
  deprecated_show_load_progress = mi_load_progress;

  print_frame_more_info_hook = mi_print_frame_more_info;

  /* Set the uiout to the interpreter's uiout.  */
  uiout = interp_ui_out (NULL);
  
  /* Turn off 8 bit strings in quoted output.  Any character with the
     high bit set is printed using C's octal format. */
  sevenbit_strings = 1;
  /* Tell the world that we're alive */
  fputs_unfiltered ("(gdb) \n", raw_stdout);
  gdb_flush (raw_stdout);
  start_event_loop ();
}

static char *
mi_input (char *buf)
{
  return gdb_readline (NULL);
}

static void
mi_load_progress (const char *section_name,
		  unsigned long sent_so_far,
		  unsigned long total_section,
		  unsigned long total_sent,
		  unsigned long grand_total)
{
  struct timeval time_now, delta, update_threshold;
  static struct timeval last_update;
  static char *previous_sect_name = NULL;
  int new_section;

  if (!interpreter_p || strncmp (interpreter_p, "mi", 2) != 0)
    return;

  update_threshold.tv_sec = 0;
  update_threshold.tv_usec = 500000;
  gettimeofday (&time_now, NULL);

  delta.tv_usec = time_now.tv_usec - last_update.tv_usec;
  delta.tv_sec = time_now.tv_sec - last_update.tv_sec;

  if (delta.tv_usec < 0)
    {
      delta.tv_sec -= 1;
      delta.tv_usec += 1000000;
    }

  new_section = (previous_sect_name ?
		 strcmp (previous_sect_name, section_name) : 1);
  if (new_section)
    {
      struct cleanup *cleanup_tuple;
      xfree (previous_sect_name);
      previous_sect_name = xstrdup (section_name);

      if (current_command_token)
	fputs_unfiltered (current_command_token, raw_stdout);
      fputs_unfiltered ("+download", raw_stdout);
      cleanup_tuple = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
      ui_out_field_string (uiout, "section", section_name);
      ui_out_field_int (uiout, "section-size", total_section);
      ui_out_field_int (uiout, "total-size", grand_total);
      do_cleanups (cleanup_tuple);
      mi_out_put (uiout, raw_stdout);
      fputs_unfiltered ("\n", raw_stdout);
      gdb_flush (raw_stdout);
    }

  if (delta.tv_sec >= update_threshold.tv_sec &&
      delta.tv_usec >= update_threshold.tv_usec)
    {
      struct cleanup *cleanup_tuple;
      last_update.tv_sec = time_now.tv_sec;
      last_update.tv_usec = time_now.tv_usec;
      if (current_command_token)
	fputs_unfiltered (current_command_token, raw_stdout);
      fputs_unfiltered ("+download", raw_stdout);
      cleanup_tuple = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
      ui_out_field_string (uiout, "section", section_name);
      ui_out_field_int (uiout, "section-sent", sent_so_far);
      ui_out_field_int (uiout, "section-size", total_section);
      ui_out_field_int (uiout, "total-sent", total_sent);
      ui_out_field_int (uiout, "total-size", grand_total);
      do_cleanups (cleanup_tuple);
      mi_out_put (uiout, raw_stdout);
      fputs_unfiltered ("\n", raw_stdout);
      gdb_flush (raw_stdout);
    }
}

extern initialize_file_ftype _initialize_mi_interp; /* -Wmissing-prototypes */

void
_initialize_mi_interp (void)
{
  static const struct interp_procs procs =
  {
    mi_interpreter_init,	/* init_proc */
    mi_interpreter_resume,	/* resume_proc */
    mi_interpreter_suspend,	/* suspend_proc */
    mi_interpreter_exec,	/* exec_proc */
    mi_interpreter_prompt_p	/* prompt_proc_p */
  };

  /* The various interpreter levels.  */
  interp_add (interp_new (INTERP_MI1, NULL, mi_out_new (1), &procs));
  interp_add (interp_new (INTERP_MI2, NULL, mi_out_new (2), &procs));
  interp_add (interp_new (INTERP_MI3, NULL, mi_out_new (3), &procs));

  /* "mi" selects the most recent released version.  "mi2" was
     released as part of GDB 6.0.  */

  /* APPLE LOCAL: Set this back to mi0, since CodeWarrior just asks for
     the "mi" and doesn't specify a version, but chokes on mi2.  */

  interp_add (interp_new (INTERP_MI, NULL, mi_out_new (0), &procs));
}
