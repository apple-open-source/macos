/* MI Command Set.

   Copyright 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation,
   Inc.

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

/* Work in progress */

#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <signal.h>            /* for kill() */

#include "defs.h"
#include "target.h"
#include "inferior.h"
#include "gdb_string.h"
#include "exceptions.h"
#include "top.h"
#include "gdbthread.h"
#include "gdbcmd.h"
#include "mi-cmds.h"
#include "mi-parse.h"
#include "mi-getopt.h"
#include "mi-console.h"
#include "ui-out.h"
#include "mi-out.h"
#include "interps.h"
#include "event-loop.h"
#include "event-top.h"
#include "gdbcore.h"		/* for write_memory() */
#include "value.h"		/* for deprecated_write_register_bytes() */
#include "regcache.h"
#include "gdb.h"
#include "frame.h"
#include "wrapper.h"
#include "source.h"
#include "mi-main.h"
#include "block.h"
#include "version.h"
/* APPLE LOCAL begin subroutine inlining  */
#include "mi-common.h"
#include "inlining.h"
/* APPLE LOCAL end subroutine inlining  */
#include "objc-lang.h"
/* APPLE LOCAL Disable breakpoints while updating data formatters.  */
#include "breakpoint.h"

enum
  {
    FROM_TTY = 0
  };

/* Enumerations of the actions that may result from calling
   captured_mi_execute_command */

enum captured_mi_execute_command_actions
  {
    EXECUTE_COMMAND_DISPLAY_PROMPT,
    EXECUTE_COMMAND_SUPRESS_PROMPT
  };

/* This structure is used to pass information from captured_mi_execute_command
   to mi_execute_command. */
struct captured_mi_execute_command_args
{
  /* This return result of the MI command (output) */
  enum mi_cmd_result rc;

  /* What action to perform when the call is finished (output) */
  enum captured_mi_execute_command_actions action;

  /* The command context to be executed (input) */
  struct mi_parse *command;
};


struct mi_continuation_arg
{
  char *token;
  struct mi_timestamp *timestamp;
  struct cleanup *cleanups;
  struct cleanup *exec_error_cleanups;
};

static void free_continuation_arg (struct mi_continuation_arg *arg);

int mi_debug_p;

/* These are the various output channels that are used by the mi. */

struct ui_file *raw_stdout;
struct ui_file *mi_stdout;
struct ui_file *mi_stderr;
struct ui_file *mi_stdlog;
struct ui_file *mi_stdtarg;

/* A pointer to the current mi_parse's command token.  This is needed
 because we can't pass the token down to the mi command levels.  This
 will get cleaned up once captured_mi_execute_command finishes, so 
 if you need it for a continuation, dup it.  */
char *current_command_token;

char *mi_error_message;
static char *old_regs;

/* This is used to pass the current command timestamp
   down to continuation routines. */
struct mi_timestamp *current_command_ts;

static int do_timings = 1;

/* Points to the current interpreter, used by the mi context callbacks.  */
struct interp *mi_interp;

/* The actual interpreters.  */
struct interp *miunspec_interp;
struct interp *mi0_interp;
struct interp *mi1_interp;
struct interp *mi2_interp;

extern void _initialize_mi_main (void);
static enum mi_cmd_result mi_cmd_execute (struct mi_parse *parse);

static void mi_execute_cli_command (const char *cmd, int arg_p, char *args);
static enum mi_cmd_result mi_execute_async_cli_command (char *mi, char *args, int from_tty);

void mi_exec_async_cli_cmd_continuation (struct continuation_arg *arg);
void mi_exec_error_cleanup (void *in_arg);

static int register_changed_p (int regnum);
static int get_register (int regnum, int format);

/* FIXME: these should go in some .h file, but infcmd.c doesn't have a
   corresponding .h file. These wrappers will be obsolete anyway, once
   we pull the plug on the sanitization. */
extern void interrupt_target_command_wrapper (char *, int);
extern void return_command_wrapper (char *, int);

/* FIXME: these should go in some .h file, but infcmd.c doesn't have a
   corresponding .h file. These wrappers will be obsolete anyway, once
   we pull the plug on the sanitization. */
extern void interrupt_target_command_wrapper (char *, int);
extern void return_command_wrapper (char *, int);

/* Use these calls to route output to the mi interpreter explicitly in
   hook functions that you insert when the console interpreter is running.
   They will ensure that the output doesn't get munged by the console
   interpreter */

static void output_control_change_notification(char *notification);

static int mi_command_completes_while_target_executing (char *command);
static void timestamp (struct mi_timestamp *tv);
static void print_diff_now (struct mi_timestamp *start);
static void copy_timestamp (struct mi_timestamp *dst, struct mi_timestamp *src);

static void print_diff (struct mi_timestamp *start, struct mi_timestamp *end);
static long wallclock_diff (struct mi_timestamp *start, struct mi_timestamp *end);
static long user_diff (struct mi_timestamp *start, struct mi_timestamp *end);
static long system_diff (struct mi_timestamp *start, struct mi_timestamp *end);
static void start_remote_counts (struct mi_timestamp *tv, const char *token);
static void end_remote_counts (struct mi_timestamp *tv);

 /* When running a synchronous target, we would like to have interpreter-exec
    give the same output as for an asynchronous one.  Use this to tell us that
    it did run so we can fake up the output.  */
 int mi_interp_exec_cmd_did_run;

/* Command implementations. FIXME: Is this libgdb? No.  This is the MI
   layer that calls libgdb.  Any operation used in the below should be
   formalized. */

enum mi_cmd_result
mi_cmd_gdb_exit (char *command, char **argv, int argc)
{
  /* We have to print everything right here because we never return */
  if (current_command_token)
    fputs_unfiltered (current_command_token, raw_stdout);
  fputs_unfiltered ("^exit\n", raw_stdout);
  mi_out_put (uiout, raw_stdout);
  /* FIXME: The function called is not yet a formal libgdb function */
  quit_force (NULL, FROM_TTY);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_exec_run (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("run", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_next (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("next", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_next_instruction (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("nexti", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_step (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("step", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_step_instruction (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("stepi", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_metrowerks_step (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("metrowerks-step", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_finish (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("finish", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_until (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("until", args, from_tty);
}

enum mi_cmd_result
mi_cmd_exec_return (char *args, int from_tty)
{
  /* This command doesn't really execute the target, it just pops the
     specified number of frames. */
  if (*args)
    /* Call return_command with from_tty argument equal to 0 so as to
       avoid being queried. */
    return_command (args, 0);
  else
    /* Call return_command with from_tty argument equal to 0 so as to
       avoid being queried. */
    return_command (NULL, 0);

  /* Because we have called return_command with from_tty = 0, we need
     to print the frame here. */
  print_stack_frame (get_selected_frame (NULL), 1, LOC_AND_ADDRESS);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_exec_continue (char *args, int from_tty)
{
  /* FIXME: Should call a libgdb function, not a cli wrapper */
  return mi_execute_async_cli_command ("continue", args, from_tty);
}

/* Interrupt the execution of the target.  */

enum mi_cmd_result
mi_cmd_exec_interrupt (char *args, int from_tty)
{
  if (!target_executing)
    {
      mi_error_message = xstrprintf ("mi_cmd_exec_interrupt: Inferior not executing.");
      return MI_CMD_ERROR;
    }
    
  interrupt_target_command (args, from_tty);

  if (current_command_token) {
    fputs_unfiltered (current_command_token, raw_stdout);
  }

  fprintf_unfiltered (raw_stdout, "^done");

  return MI_CMD_QUIET;
}

enum mi_cmd_result
mi_cmd_exec_status (char *command, char **argv, int argc)
{
  char *status;

  if (argc != 0)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_exec_status takes no arguments.");
      return MI_CMD_ERROR;
    }

  if (!target_has_execution)
    status = "not executing";
  else
    if (target_executing)
      /* FIXME: The result reporting needs to be better 
	 centralized.  The "^" for done and the result code
	 should all come from one place, rather than being
	 scattered throughout the code.  But untill this happens,
	 both this command and exec-interrupt will have to play
	 games to get their returns out properly... */
      {
	status = "running";
	fputs_unfiltered ("^done", raw_stdout);
	ui_out_field_string (uiout, "status", status);
	mi_out_put (uiout, raw_stdout);
	mi_out_rewind (uiout);
	fputs_unfiltered ("\n", raw_stdout);
	return MI_CMD_DONE;
      }
    else
      status = "stopped";

  ui_out_field_string (uiout, "status", status);
  
  return MI_CMD_DONE;

}

/* exec-safe-call <THREAD> <SUBSYSTEM>
   
   Checks whether it would be safe to call into SUBSYSTEM.  The first
   argument is the thread id to check - or optionally "all" for all
   threads, or "current" for the current thead.  Relies on the
   target's check_safe_call method.  Also uses the scheduler value to
   determine whether to check all threads even if a given thread is
   passed in.  Subsequent arguments are the subsystems to check,
   and the currently valid values are "malloc", "objc", "loader"
   and "all".  */

enum mi_cmd_result
mi_cmd_exec_safe_call (char *command, char **argv, int argc)
{
  struct cleanup *old_cleanups;
  ptid_t current_ptid = inferior_ptid;
  int safe;
  int i;
  int subsystems = 0;
  enum check_which_threads thread_mode;

  if (argc == 0 || argc < 2)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_exec_safe_call: USAGE: threadnum system [system... ]");
      return MI_CMD_ERROR;
    }

  if (!target_has_execution)
    {
      ui_out_field_int (uiout, "safe", 0);
      ui_out_field_string (uiout, "problem", "program not running");
    }

  if (target_executing)
    {
      ui_out_field_int (uiout, "safe", 0);
      ui_out_field_string (uiout, "problem", "program executing");
    }

  if (strcmp (argv[0], "all") == 0)
    {
      old_cleanups = make_cleanup (null_cleanup, NULL);
      thread_mode = CHECK_ALL_THREADS;
    }
  else if (strcmp (argv[0], "current") == 0)
    {
      old_cleanups = make_cleanup (null_cleanup, NULL);
      thread_mode = CHECK_SCHEDULER_VALUE;
    }
  else
    {
      enum gdb_rc rc;
      old_cleanups = make_cleanup_restore_current_thread (current_ptid, 0);
      rc = gdb_thread_select (uiout, argv[0], 0, 0);
      thread_mode = CHECK_SCHEDULER_VALUE;
      
      /* RC is enum gdb_rc if it is successful (>=0)
	 enum return_reason if not (<0). */
      if (((int) rc < 0 && (enum return_reason) rc == RETURN_ERROR) ||
          ((int) rc >= 0 && rc == GDB_RC_FAIL))
	{
	  error ("Could not set thread to \"%s\".\n", argv[0]);
	}
    }

  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "malloc") == 0)
	subsystems |= MALLOC_SUBSYSTEM;
      else if (strcmp (argv[i], "loader") == 0)
	subsystems |= LOADER_SUBSYSTEM;
      else if (strcmp (argv[i], "objc") == 0)
	subsystems |= OBJC_SUBSYSTEM;
      else if (strcmp (argv[i], "all") == 0)
	subsystems |= ALL_SUBSYSTEMS;
      else
	error ("Unrecognized subsystem: %s", argv[i]);
    }

  safe = target_check_safe_call (subsystems, thread_mode);

  ui_out_field_int (uiout, "safe", safe);
  do_cleanups (old_cleanups);

  return MI_CMD_DONE;
}

/* APPLE LOCAL: Have a show version that returns something useful...  

   Takes one argument PRINT-BANNER, which should be 0 ro 1.
   If 1 (the default) we print the standard FSF banner to gdb_stdout.
   If 0 we don't.  
   
   returns the version string, the rc_version ("unknown" if not build
   by buildit), the target and the build date.  */

enum mi_cmd_result
mi_cmd_show_version (char *command, char **argv, int argc)
{
  int print_banner;
  char *endptr;

  if (argc > 1)
    {
      mi_error_message = xstrprintf ("mi_cmd_show_version: Usage: [PRINT-BANNER]");
      return MI_CMD_ERROR;
    }
  if (argc == 0)
    print_banner = 1;
  else
    {
      print_banner = strtol (argv[0], &endptr, 0);
      if (*endptr != '\0')
	{
	  mi_error_message = xstrprintf ("mi_cmd_show_version: PRINT-BANNER must be 0 or 1");
	  return MI_CMD_ERROR;
	}
    }
  
  if (print_banner)
    {
      print_gdb_version (gdb_stdout);
      gdb_flush (gdb_stdout);
    }

  ui_out_field_string (uiout, "version", version);
  ui_out_field_string (uiout, "rc_version", rc_version);
  ui_out_field_string (uiout, "target", target_name);
  ui_out_field_string (uiout, "build-date", build_date);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_thread_select (char *command, char **argv, int argc)
{
  enum gdb_rc rc;

  if (argc != 1)
    {
      mi_error_message = xstrprintf ("mi_cmd_thread_select: USAGE: threadnum.");
      return MI_CMD_ERROR;
    }
  else
    rc = gdb_thread_select (uiout, argv[0], 1, &mi_error_message);

  /* RC is enum gdb_rc if it is successful (>=0)
     enum return_reason if not (<0). */
  if ((int) rc < 0 && (enum return_reason) rc == RETURN_ERROR)
    return MI_CMD_ERROR;
  else if ((int) rc >= 0 && rc == GDB_RC_FAIL)
    return MI_CMD_ERROR;
  else
    return MI_CMD_DONE;
}

/* This command sets the pc for the thread given in the first argument
   to the linespec given in the second argument.  It does not
   switch to that thread, however (except temporarily to set the pc).
   The return value is the new bottom stack frame after resetting the
   pc.

   If you specify a linespec that is outside the current function, the
   command will return an error, and not reset the pc.  To force it to
   set the pc anyway, add "-f" before any of the other arguments. 

   If a line in the prologue is specified, the first line with non-prologue
   code is returned.  This behavior can be suppressed by passing the -n
   (erm, "-no-prologue-skip"..? Yeah, that's it) option in which case you
   can move anywhere you want.

   Optional -f flag may be provided, preceding other arguments.
   First argument is the thread #.
   Second argument is a FILENAME:LINENO specification.

   Lesson learned: Don't do write MI commands that take their arguments
   like this (positional, multiple bits of data suck together).  
   Use separate flags for each piece of individual information e.g.
   cf mi_cmd_disassemble().  For instance, it'd be that much easier to
   parse if -thread-set-pc took things like "-f -t 5 -s foo.c -l 35",
   and future expansion would be easier to implement as well.
   */

enum mi_cmd_result
mi_cmd_thread_set_pc (char *command, char **argv, int argc)
{
  enum gdb_rc rc;
  struct symtab *s;
  struct symtab_and_line sal;
  ptid_t current_ptid = inferior_ptid;
  struct cleanup *old_cleanups;
  struct symbol *old_fun = NULL, *new_fun = NULL;
  int stay_in_function = 1;
  int avoid_prologue = 1;
  const char *filename;
  int lineno;
  CORE_ADDR new_pc;

  /* yay hand-written argv parsers, just what we need more of. */

  /* If the command starts with a -f or -n, record the fact and
     strip them.  */

  if (argc >= 3)
    {
      while (argc > 2)
        if (argv[0][0] == '-')
          {
            if (argv[0][1] == 'f')
	      {
	        stay_in_function = 0;
	        argc--;
	        argv++;
	      }
            else if (argv[0][1] == 'n')
	      {
	        avoid_prologue = 0;
	        argc--;
	        argv++;
	      }
          }
        else
          break;
    }
      
  if (argc != 2)
    {
      xasprintf (&mi_error_message,
		 "mi_cmd_thread_select: USAGE: [-f] [-n] threadnum file:line");
      return MI_CMD_ERROR;
    }

  old_cleanups = make_cleanup_restore_current_thread (current_ptid, 0);
  rc = gdb_thread_select (uiout, argv[0], 0, 0);

  /* RC is enum gdb_rc if it is successful (>=0)
     enum return_reason if not (<0). */
  if ((int) rc < 0 && (enum return_reason) rc == RETURN_ERROR)
    return MI_CMD_ERROR;
  else if ((int) rc >= 0 && rc == GDB_RC_FAIL)
    return MI_CMD_ERROR;

  /* Okay, we set the thread, now set the pc.  Find the filename
     and the line number parts; skip over quoting that may be put
     around them.  */

  /* FIXME: If a filename begins with a quote character, we're
     going to skip over it.  e.g if this is set:
       -thread-set-pc 1 "\"file with quote.c:20"
     the following loop does the wrong thing.  */
  filename = argv[1];
  while (filename != NULL && (*filename == '"' || *filename == '\\'))
    filename++;

  char *c = strrchr (filename, ':');
  if (c == NULL)
    error ("mi_cmd_thread_set_pc: Unable to find colon character in last argument, '%s'.", argv[1]);

  errno = 0;
  lineno = strtol (c + 1, NULL, 10);
  if (errno != 0)
    error ("mi_cmd_thread_set_pc: Error parsing line number part of argument, '%s'.", argv[1]);
  
  *c = '\0';
  s = lookup_symtab (filename);
  if (s == NULL)
    error ("mi_cmd_thread_set_pc: Unable to find source file name '%s'.", filename);
  if (!find_line_pc (s, lineno, &new_pc))
    error ("mi_cmd_thread_set_pc: Invalid line number '%d'", lineno);

  /* Get a sal for the new PC for later in the function where we want
     to set the CLI default source/line #'s and such.  */
  sal = find_pc_line (new_pc, 0);
  if (sal.symtab != s)
    {
      if (s == NULL || sal.symtab == NULL)
        error ("mi_cmd_thread_set_pc: Found one symtab by filename lookup, but symtab another by PC lookup, and one of them is null.");
      else
        error ("mi_cmd_thread_set_pc: Found symtab '%s' by filename lookup, but symtab '%s' by PC lookup.", s->filename, sal.symtab->filename);
    }

  /* By default we don't want to let someone set the PC into the middle
     of the prologue or the quality of their debugging experience will 
     be diminished.  So bump it to the first non-prologue line.  We'll
     surely still lose in optimized code, but then anyone moving the PC
     around in optimized code is cruising for a brusing.  */

  new_fun = find_pc_function (new_pc);
  /* APPLE LOCAL begin address ranges  */
  if (avoid_prologue && new_fun 
      && BLOCK_LOWEST_PC (SYMBOL_BLOCK_VALUE (new_fun)) == new_pc)
  /* APPLE LOCAL end address ranges  */
    {
      sal = find_function_start_sal (new_fun, 1);
      new_pc = sal.pc;
    }
  old_fun = get_frame_function (get_current_frame ());
  if (stay_in_function)
    {
      if (old_fun == NULL)
	error ("Can't find the function for old_pc: 0x%s",
	       paddr_nz (get_frame_pc (get_current_frame ())));
      if (new_fun == NULL)
	error ("Can't find the function for new pc 0x%s", paddr_nz (new_pc));
      if (!SYMBOL_MATCHES_NATURAL_NAME (old_fun, SYMBOL_NATURAL_NAME (new_fun)))
	error ("New pc: 0x%s outside of current function", paddr_nz (new_pc));
    }

  write_pc (new_pc);

  /* We have to set the stop_pc to the pc that we moved to as well, so
     that if we are stopped at a breakpoint in the new location, we will
     properly step over it. */

  stop_pc = new_pc;
  /* APPLE LOCAL begin subroutine inlining  */
  /* If the PC has changed since the last time we updated the
     global_inlined_call_stack data, we need to verify the current
     data and possibly update it.  */
  if (stop_pc != inlined_function_call_stack_pc ())
    inlined_function_update_call_stack (stop_pc);
  /* APPLE LOCAL end subroutine inlining  */

  /* Update the current source location as well, so 'list' will do the right
     thing.  */

  set_current_source_symtab_and_line (&sal);

  /* Update the current breakpoint location as well, so break commands will
     do the right thing.  */

  set_default_breakpoint (1, new_pc, sal.symtab, sal.line);

  /* Is this a Fix and Continue situation, i.e. do we have two
     identically named functions which are different?  We have
     some extra work to do in that case.  */

  if (old_fun != NULL && old_fun != new_fun && 
      SYMBOL_MATCHES_NATURAL_NAME (old_fun, SYMBOL_NATURAL_NAME (new_fun)))
    {
      update_picbase_register (new_fun);
    }

  /* FIXME - write_pc doesn't actually update the bottom-most frame_info
     structure, so I have to flush_cached_frames.  But that leaves the
     deprecated_selected_frame NULL, which is fatal when you try to print
     a register.  So I have to select the current frame to make everything
     copasetic again.  */

  flush_cached_frames ();
  select_frame (get_current_frame ());
  print_stack_frame (deprecated_selected_frame, 0, LOC_AND_ADDRESS);

  do_cleanups (old_cleanups);
  return MI_CMD_DONE;
}


/* There are two invocations styles for -file-fix-file; the original 
   (deprecated) fall-through to the CLI which was invoked as
      -file-fix-file BUNDLE-NAME SOURCE-NAME [OBJECT-NAME]

   And the new, better MI way,
      -file-fix-file -b BUNDLE -f SOURCE -o OBJECT -s SOLIB

   -f SOURCE specifies the source filename that is being fixed.
   -b BUNDLE specifies the name of the fixed bundle that we'll load in.
   -o OBJECT is only needed when the inferior is a ZeroLink executable;
             the object filename is given to the ZL stub to page in the 
             original functions if they haven't already been paged in.
   -s SOLIB specifies the original executable/shared library that contains
            the source file we're fixing.  This option is needed when gdb's 
            load-levels are set to 'extern' by default (OBJF_SYM_EXTERN) so 
            that we can raise the load-level on the original struct objfile 
            that includes the fixed file (i.e. set it to OBJF_SYM_ALL & read 
            in the psymtabs if we haven't already done so).
            If this isn't done, gdb can't figure out which struct objfile
            contains the given source file so it can't expand the psymtabs, etc.,
            and the fix operation will fail.
 */

enum mi_cmd_result
mi_cmd_file_fix_file (char *command, char **argv, int argc)
{
  char *source_filename = NULL;
  char *bundle_filename = NULL;
  char *solib_filename = NULL;
  int optind = 0;
  char *optarg;
  struct cleanup *wipe;

  enum fff_opt
  {
    BUNDLE_OPT, SOURCE_OPT, OBJECT_OPT, SOLIB_OPT
  };
  static struct mi_opt fff_opts[] = {
    {"b", BUNDLE_OPT, 1},
    {"f", SOURCE_OPT, 1},
    {"o", OBJECT_OPT, 1},
    {"s", SOLIB_OPT, 1},
    {0, 0, 0}
  };

  /* Old style invocation?  */
  if (argc == 2 || argc == 3)
    {
      bundle_filename = argv[0];
      source_filename = argv[1];
      fix_command_1 (source_filename, bundle_filename, NULL);
      return MI_CMD_DONE;
    }

  /* New style (argument flags) invocation */

  wipe = make_cleanup (null_cleanup, NULL);
  while (1)
    {
      int opt = mi_getopt ("mi_cmd_file_fix_file", argc, argv, fff_opts,
                           &optind, &optarg);
      if (opt < 0)
        break;
      switch ((enum fff_opt) opt)
        {
        case BUNDLE_OPT:
          bundle_filename = xstrdup (optarg);
          make_cleanup (xfree, bundle_filename);
          break;
        case SOURCE_OPT:
          source_filename = xstrdup (optarg);
          make_cleanup (xfree, source_filename);
          break;
        case OBJECT_OPT:
          break;
        case SOLIB_OPT:
          solib_filename = xstrdup (optarg);
          make_cleanup (xfree, solib_filename);
          break;
        }
     }
   argv += optind;
   argc -= optind;

   if (source_filename == NULL || bundle_filename == NULL)
     error ("mi_cmd_file_fix_file: Usage -f source-filename -b bundle-filename "
            "[-o object-filename] [-s dylib-filename]");

   fix_command_1 (source_filename, bundle_filename, solib_filename);

   do_cleanups (wipe);
   return MI_CMD_DONE;
}

/* APPLE LOCAL: Is Fix and Continue supported with the current 
                architecture/osabi?  */

enum mi_cmd_result
mi_cmd_file_fix_file_is_grooved (char *command, char **argv, int argc)
{
  int retval = fix_and_continue_supported ();

  /* For cases where we can't determine if F&C is supported (e.g. a
     binary hasn't yet been specified), retval is -1 and we'll just
     report "Supported" to our GUI and hope for the best. */

  if (retval == 1 || retval == -1)
    {
      ui_out_field_int (uiout, "supported", 1);
      ui_out_field_string (uiout, "details", "Yes grooved!");
    }
  else
    {
      ui_out_field_int (uiout, "supported", 0);
      ui_out_field_string (uiout, "details", "Groove is gone.");
    }

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_thread_list_ids (char *command, char **argv, int argc)
{
  enum gdb_rc rc = MI_CMD_DONE;

  if (argc != 0)
    {
      mi_error_message = xstrprintf ("mi_cmd_thread_list_ids: No arguments required.");
      return MI_CMD_ERROR;
    }
  else
    rc = gdb_list_thread_ids (uiout, &mi_error_message);

  if (rc == GDB_RC_FAIL)
    return MI_CMD_ERROR;
  else
    return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_data_list_register_names (char *command, char **argv, int argc)
{
  int regnum, numregs;
  int i;
  struct cleanup *cleanup;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS + NUM_PSEUDO_REGS;

  cleanup = make_cleanup_ui_out_list_begin_end (uiout, "register-names");

  if (argc == 0)		/* No args, just do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    ui_out_field_string (uiout, NULL, "");
	  else
	    ui_out_field_string (uiout, NULL, REGISTER_NAME (regnum));
	}
    }

  /* Else, list of register #s, just do listed regs */
  for (i = 0; i < argc; i++)
    {
      regnum = atoi (argv[i]);
      if (regnum < 0 || regnum >= numregs)
	{
	  do_cleanups (cleanup);
	  mi_error_message = xstrprintf ("bad register number");
	  return MI_CMD_ERROR;
	}
      if (REGISTER_NAME (regnum) == NULL
	  || *(REGISTER_NAME (regnum)) == '\0')
	ui_out_field_string (uiout, NULL, "");
      else
	ui_out_field_string (uiout, NULL, REGISTER_NAME (regnum));
    }
  do_cleanups (cleanup);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_data_list_changed_registers (char *command, char **argv, int argc)
{
  int regnum, numregs, changed;
  int i;
  struct cleanup *cleanup;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS + NUM_PSEUDO_REGS;

  cleanup = make_cleanup_ui_out_list_begin_end (uiout, "changed-registers");

  if (argc == 0)		/* No args, just do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    continue;
	  changed = register_changed_p (regnum);
	  if (changed < 0)
	    {
	      do_cleanups (cleanup);
	      mi_error_message = xstrprintf ("mi_cmd_data_list_changed_registers: Unable to read register contents.");
	      return MI_CMD_ERROR;
	    }
	  else if (changed)
	    ui_out_field_int (uiout, NULL, regnum);
	}
    }

  /* Else, list of register #s, just do listed regs */
  for (i = 0; i < argc; i++)
    {
      regnum = atoi (argv[i]);

      if (regnum >= 0
	  && regnum < numregs
	  && REGISTER_NAME (regnum) != NULL
	  && *REGISTER_NAME (regnum) != '\000')
	{
	  changed = register_changed_p (regnum);
	  if (changed < 0)
	    {
	      do_cleanups (cleanup);
	      mi_error_message = xstrprintf ("mi_cmd_data_list_register_change: Unable to read register contents.");
	      return MI_CMD_ERROR;
	    }
	  else if (changed)
	    ui_out_field_int (uiout, NULL, regnum);
	}
      else
	{
	  do_cleanups (cleanup);
	  mi_error_message = xstrprintf ("bad register number");
	  return MI_CMD_ERROR;
	}
    }
  do_cleanups (cleanup);
  return MI_CMD_DONE;
}

static int
register_changed_p (int regnum)
{
  gdb_byte raw_buffer[MAX_REGISTER_SIZE];

  if (! frame_register_read (get_selected_frame (NULL), regnum, raw_buffer))
    return -1;

  if (memcmp (&old_regs[DEPRECATED_REGISTER_BYTE (regnum)], raw_buffer,
	      register_size (current_gdbarch, regnum)) == 0)
    return 0;

  /* Found a changed register. Return 1. */

  memcpy (&old_regs[DEPRECATED_REGISTER_BYTE (regnum)], raw_buffer,
	  register_size (current_gdbarch, regnum));

  return 1;
}

/* Return a list of register number and value pairs. The valid
   arguments expected are: a letter indicating the format in which to
   display the registers contents. This can be one of: x (hexadecimal), d
   (decimal), N (natural), t (binary), o (octal), r (raw).  After the
   format argumetn there can be a sequence of numbers, indicating which
   registers to fetch the content of. If the format is the only argument,
   a list of all the registers with their values is returned. */
enum mi_cmd_result
mi_cmd_data_list_register_values (char *command, char **argv, int argc)
{
  int regnum, numregs, format, result;
  int i;
  struct cleanup *list_cleanup, *tuple_cleanup;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS + NUM_PSEUDO_REGS;

  if (argc == 0)
    {
      mi_error_message = xstrprintf ("mi_cmd_data_list_register_values: Usage: -data-list-register-values <format> [<regnum1>...<regnumN>]");
      return MI_CMD_ERROR;
    }

  format = (int) argv[0][0];

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "register-values");

  if (argc == 1)		/* No args, beside the format: do all the regs */
    {
      for (regnum = 0;
	   regnum < numregs;
	   regnum++)
	{
	  if (REGISTER_NAME (regnum) == NULL
	      || *(REGISTER_NAME (regnum)) == '\0')
	    continue;
	  tuple_cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
	  ui_out_field_int (uiout, "number", regnum);
	  result = get_register (regnum, format);
	  if (result == -1)
	    {
	      do_cleanups (list_cleanup);
	      return MI_CMD_ERROR;
	    }
	  do_cleanups (tuple_cleanup);
	}
    }

  /* Else, list of register #s, just do listed regs */
  for (i = 1; i < argc; i++)
    {
      regnum = atoi (argv[i]);

      if (regnum >= 0
	  && regnum < numregs
	  && REGISTER_NAME (regnum) != NULL
	  && *REGISTER_NAME (regnum) != '\000')
	{
	  tuple_cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
	  ui_out_field_int (uiout, "number", regnum);
	  result = get_register (regnum, format);
	  if (result == -1)
	    {
	      do_cleanups (list_cleanup);
	      return MI_CMD_ERROR;
	    }
	  do_cleanups (tuple_cleanup);
	}
      else
	{
	  do_cleanups (list_cleanup);
	  mi_error_message = xstrprintf ("bad register number");
	  return MI_CMD_ERROR;
	}
    }
  do_cleanups (list_cleanup);
  return MI_CMD_DONE;
}

/* Output one register's contents in the desired format. */
static int
get_register (int regnum, int format)
{
  gdb_byte buffer[MAX_REGISTER_SIZE];
  enum opt_state optim;
  int realnum;
  CORE_ADDR addr;
  enum lval_type lval;
  static struct ui_stream *stb = NULL;

  stb = ui_out_stream_new (uiout);

  if (format == 'N')
    format = 0;

  frame_register (get_selected_frame (NULL), regnum, &optim, &lval, &addr,
		  &realnum, buffer);

  if (optim)
    {
      mi_error_message = xstrprintf ("Optimized out");
      return -1;
    }

  if (format == 'r')
    {
      int j;
      char *ptr, buf[1024];

      strcpy (buf, "0x");
      ptr = buf + 2;
      for (j = 0; j < register_size (current_gdbarch, regnum); j++)
	{
	  int idx = TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? j
	  : register_size (current_gdbarch, regnum) - 1 - j;
	  sprintf (ptr, "%02x", (unsigned char) buffer[idx]);
	  ptr += 2;
	}
      ui_out_field_string (uiout, "value", buf);
      /*fputs_filtered (buf, gdb_stdout); */
    }
  else
    {
      val_print (register_type (current_gdbarch, regnum), buffer, 0, 0,
		 stb->stream, format, 1, 0, Val_pretty_default);
      ui_out_field_stream (uiout, "value", stb);
      ui_out_stream_delete (stb);
    }
  return 1;
}

/* Write given values into registers. The registers and values are
   given as pairs. The corresponding MI command is 
   -data-write-register-values <format> [<regnum1> <value1>...<regnumN> <valueN>]*/
enum mi_cmd_result
mi_cmd_data_write_register_values (char *command, char **argv, int argc)
{
  int regnum;
  int i;
  int numregs;
  LONGEST value;
  char format;

  /* Note that the test for a valid register must include checking the
     REGISTER_NAME because NUM_REGS may be allocated for the union of
     the register sets within a family of related processors.  In this
     case, some entries of REGISTER_NAME will change depending upon
     the particular processor being debugged.  */

  numregs = NUM_REGS + NUM_PSEUDO_REGS;

  if (argc == 0)
    {
      mi_error_message = xstrprintf ("mi_cmd_data_write_register_values: Usage: -data-write-register-values <format> [<regnum1> <value1>...<regnumN> <valueN>]");
      return MI_CMD_ERROR;
    }

  format = (int) argv[0][0];

  if (!target_has_registers)
    {
      mi_error_message = xstrprintf ("mi_cmd_data_write_register_values: No registers.");
      return MI_CMD_ERROR;
    }

  if (!(argc - 1))
    {
      mi_error_message = xstrprintf ("mi_cmd_data_write_register_values: No regs and values specified.");
      return MI_CMD_ERROR;
    }

  if ((argc - 1) % 2)
    {
      mi_error_message = xstrprintf ("mi_cmd_data_write_register_values: Regs and vals are not in pairs.");
      return MI_CMD_ERROR;
    }

  for (i = 1; i < argc; i = i + 2)
    {
      regnum = atoi (argv[i]);

      if (regnum >= 0
          && regnum < numregs
          && REGISTER_NAME (regnum) != NULL
          && *REGISTER_NAME (regnum) != '\000')
        {
          void *buffer;
          struct cleanup *old_chain;

          /* Get the value as a number */
          value = parse_and_eval_address (argv[i + 1]);
          /* Get the value into an array */
          buffer = xmalloc (DEPRECATED_REGISTER_SIZE);
          old_chain = make_cleanup (xfree, buffer);
          store_signed_integer (buffer, DEPRECATED_REGISTER_SIZE, value);
          /* Write it down */
	  deprecated_write_register_bytes 
	    (DEPRECATED_REGISTER_BYTE (regnum),
	     buffer, register_size (current_gdbarch, regnum));
          /* Free the buffer.  */
          do_cleanups (old_chain);
        }
      else
	{
	  mi_error_message = xstrprintf ("bad register number");
	  return MI_CMD_ERROR;
	}
    }
  return MI_CMD_DONE;
}

#if 0
/*This is commented out because we decided it was not useful. I leave
   it, just in case. ezannoni:1999-12-08 */

/* Assign a value to a variable. The expression argument must be in
   the form A=2 or "A = 2" (I.e. if there are spaces it needs to be
   quoted. */
enum mi_cmd_result
mi_cmd_data_assign (char *command, char **argv, int argc)
{
  struct expression *expr;
  struct cleanup *old_chain;

  if (argc != 1)
    {
      mi_error_message = xstrprintf ("mi_cmd_data_assign: Usage: -data-assign expression");
      return MI_CMD_ERROR;
    }

  /* NOTE what follows is a clone of set_command(). FIXME: ezannoni
     01-12-1999: Need to decide what to do with this for libgdb purposes. */

  old_chain = make_cleanup_set_restore_scheduler_locking_mode (scheduler_locking_on);
  expr = parse_expression (argv[0]);
  make_cleanup (free_current_contents, &expr);
  evaluate_expression (expr);
  do_cleanups (old_chain);
  return MI_CMD_DONE;
}
#endif

/* Evaluate the value of the argument. The argument is an
   expression. If the expression contains spaces it needs to be
   included in double quotes. */
enum mi_cmd_result
mi_cmd_data_evaluate_expression (char *command, char **argv, int argc)
{
  struct expression *expr;
  struct cleanup *old_chain = NULL;
  /* APPLE LOCAL Disable breakpoints while updating data formatters.  */
  struct cleanup *bp_cleanup;
  struct value *val;
  struct ui_stream *stb = NULL;
  int unwinding_was_requested = 0;
  char *expr_string;

  stb = ui_out_stream_new (uiout);

  /* APPLE LOCAL Disable breakpoints while updating data formatters.  */
  bp_cleanup = make_cleanup_enable_disable_bpts_during_varobj_operation ();

  if (argc == 1)
    {
      expr_string = argv[0];
    }
  else if (argc == 2)
    {
      if (strcmp (argv[0], "-u") != 0)
	{
	  /* APPLE LOCAL Disable breakpoints while updating data
	     formatters.  */
	  do_cleanups (bp_cleanup);
	  xasprintf (&mi_error_message,
		     "mi_cmd_data_evaluate_expression: Usage: "
		     "-data-evaluate-expression [-u] expression");
	  return MI_CMD_ERROR;
	}
      else
	{
          unwinding_was_requested = 1;
	  expr_string = argv[1];
	}
    }
  else
    {
      /* APPLE LOCAL Disable breakpoints while updating data formatters.  */
      do_cleanups (bp_cleanup);
      mi_error_message = xstrprintf ("mi_cmd_data_evaluate_expression: Usage: -data-evaluate-expression [-u] expression");
      return MI_CMD_ERROR;
    }
  
  old_chain = make_cleanup_set_restore_scheduler_locking_mode (scheduler_locking_on);
  if (unwinding_was_requested)
    make_cleanup_set_restore_unwind_on_signal (1);

  expr = parse_expression (expr_string);

  make_cleanup (free_current_contents, &expr);

  val = evaluate_expression (expr);

  /* Print the result of the expression evaluation. */
  val_print (value_type (val), value_contents (val),
	     value_embedded_offset (val), VALUE_ADDRESS (val),
	     stb->stream, 0, 0, 0, 0);

  ui_out_field_stream (uiout, "value", stb);
  ui_out_stream_delete (stb);

  do_cleanups (old_chain);
  /* APPLE LOCAL Disable breakpoints while updating data formatters.  */
  do_cleanups (bp_cleanup);

  return MI_CMD_DONE;
}

/* APPLE LOCAL: -target-attach
   This implements "-target-attach <PID>".  It is
   identical to the CLI command except that we raise
   an error if we are already attached.  
   We also support "-waitfor PROCESS_NAME" by passing
   the two arguments put together to target_attach and
   letting the implementation deal with it.  */

enum mi_cmd_result
mi_cmd_target_attach (char *command, char **argv, int argc)
{
  char *attach_arg;
  struct cleanup *cleanups;

  if (target_has_execution)
    error ("mi_cmd_target_attach: Already debugging - detach first");

  if (argc == 1)
    {
      attach_arg = argv[0];
      cleanups = make_cleanup (null_cleanup, NULL);
    }
  else if (argc == 2 && strcmp (argv[0], "-waitfor") == 0)
    {
      int arg_len;
      arg_len = strlen ("-waitfor \"") + strlen (argv[1]) + 2;
      attach_arg = xmalloc (arg_len);
      cleanups = make_cleanup (xfree, attach_arg);
      snprintf (attach_arg, arg_len, "-waitfor \"%s\"", argv[1]);
    }
  else 
    error ("mi_cmd_target_attach: Usage PID|-waitfor \"PROCESS\"");

  attach_command (attach_arg, 0);

  do_cleanups (cleanups);
  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_target_download (char *args, int from_tty)
{
  char *run;
  struct cleanup *old_cleanups = NULL;

  run = xstrprintf ("load %s", args);
  old_cleanups = make_cleanup (xfree, run);
  execute_command (run, from_tty);

  do_cleanups (old_cleanups);
  return MI_CMD_DONE;
}

/* Connect to the remote target. */
enum mi_cmd_result
mi_cmd_target_select (char *args, int from_tty)
{
  char *run;
  struct cleanup *old_cleanups = NULL;

  run = xstrprintf ("target %s", args);
  old_cleanups = make_cleanup (xfree, run);

  /* target-select is always synchronous.  once the call has returned
     we know that we are connected. */
  /* NOTE: At present all targets that are connected are also
     (implicitly) talking to a halted target.  In the future this may
     change. */
  execute_command (run, from_tty);

  do_cleanups (old_cleanups);

  /* Issue the completion message here. */
  if (current_command_token)
    fputs_unfiltered (current_command_token, raw_stdout);
  fputs_unfiltered ("^connected", raw_stdout);
  do_exec_cleanups (ALL_CLEANUPS);
  return MI_CMD_QUIET;
}

/* TARGET-LOAD-SOLIB:

   PATH: path to the shared library to be loaded
   FLAGS: a string containing the flags.  This will be passed
   as is to the target's to_load_solib function.  If the flags
   are not provided, the target's to_load_solib function will
   pick whatever is considered the default for the platform.

   Loads the shared library pointed to by PATH, using FLAGS as
   interpreted by the target specific function.

   Returns:
   A token that can be passed to TARGET-UNLOAD-SOLIB to unload
   this shared library.

   If the library can't be loaded, the command will return an
   error, and the platform specific error message if it can 
   be retrieved.
*/

struct solib_token_elem
{
  char *token;
  struct value *val;
  struct solib_token_elem *next;
} *solib_token_list;

enum mi_cmd_result
mi_cmd_target_load_solib (char *command, char **argv, int argc)
{
  static int token_ctr = 0;
  struct value *ret_val;
  struct gdb_exception e;
  struct solib_token_elem *new_token;

  if (argc > 2)
    {
      mi_error_message = xstrdup ("Too many arguments: target-load-solib <PATH> [<FLAGS>]");
      return MI_CMD_ERROR;
    }
  else if (argc == 0)
    {
      mi_error_message = xstrdup ("Too few arguments: target-load-solib <PATH> [<FLAGS>]");
      return MI_CMD_ERROR;
    }

  TRY_CATCH (e, RETURN_MASK_ALL)
    {
      ret_val = target_load_solib (argv[0], argv[1]);
    }

  if (ret_val == NULL)
    {
      mi_error_message = xstrprintf ("Unknown error loading shared library \"%s\"", argv[0]);
      return MI_CMD_ERROR;
    }
  else if (e.reason != NO_ERROR)
    {
      mi_error_message = xstrdup (e.message);
      return MI_CMD_ERROR;
    }

  new_token = xmalloc (sizeof (struct solib_token_elem));
  new_token->token = xstrprintf ("%d-solib", token_ctr++);
  new_token->val = ret_val;
  release_value (ret_val);

  new_token->next = solib_token_list;
  solib_token_list = new_token;
  
  ui_out_field_string (uiout, "token", new_token->token);
  return MI_CMD_DONE;

}

/* TARGET-UNLOAD-SOLIB

   Takes a token (the return value of a previous call to target-load-solib)
   and unloads that shared library.

   If there's some error unloading the library, we will return an
   error and if possible fetch the error message.

   FIXME: Implement this if somebody ever needs it.
*/

enum mi_cmd_result
mi_cmd_target_unload_solib (char *command, char **argv, int argc)
{
  mi_error_message = xstrprintf ("Not yet implemented.");
  return MI_CMD_ERROR;
}

/* DATA-MEMORY-READ:

   ADDR: start address of data to be dumped.
   WORD-FORMAT: a char indicating format for the ``word''. See 
   the ``x'' command.
   WORD-SIZE: size of each ``word''; 1,2,4, or 8 bytes
   NR_ROW: Number of rows.
   NR_COL: The number of colums (words per row).
   ASCHAR: (OPTIONAL) Append an ascii character dump to each row.  Use
   ASCHAR for unprintable characters.

   Reads SIZE*NR_ROW*NR_COL bytes starting at ADDR from memory and
   displayes them.  Returns:

   {addr="...",rowN={wordN="..." ,... [,ascii="..."]}, ...}

   Returns: 
   The number of bytes read is SIZE*ROW*COL. */

enum mi_cmd_result
mi_cmd_data_read_memory (char *command, char **argv, int argc)
{
  struct cleanup *cleanups = make_cleanup (null_cleanup, NULL);
  CORE_ADDR addr;
  long total_bytes;
  long nr_cols;
  long nr_rows;
  char word_format;
  struct type *word_type;
  long word_size;
  char word_asize;
  char aschar;
  char *mbuf;
  int nr_bytes;
  long offset = 0;
  int optind = 0;
  char *optarg;
  enum opt
    {
      OFFSET_OPT
    };
  static struct mi_opt opts[] =
  {
    {"o", OFFSET_OPT, 1},
    {0, 0, 0},
  };

  while (1)
    {
      int opt = mi_getopt ("mi_cmd_data_read_memory", argc, argv, opts,
			   &optind, &optarg);
      if (opt < 0)
	break;
      switch ((enum opt) opt)
	{
	case OFFSET_OPT:
	  offset = atol (optarg);
	  break;
	}
    }
  argv += optind;
  argc -= optind;

  if (argc < 5 || argc > 6)
    {
      mi_error_message = xstrprintf ("mi_cmd_data_read_memory: Usage: ADDR WORD-FORMAT WORD-SIZE NR-ROWS NR-COLS [ASCHAR].");
      return MI_CMD_ERROR;
    }

  /* Extract all the arguments. */

  /* Start address of the memory dump. */
  addr = parse_and_eval_address (argv[0]) + offset;
  /* The format character to use when displaying a memory word. See
     the ``x'' command. */
  word_format = argv[1][0];
  /* The size of the memory word. */
  word_size = atol (argv[2]);
  switch (word_size)
    {
    case 1:
      word_type = builtin_type_int8;
      word_asize = 'b';
      break;
    case 2:
      word_type = builtin_type_int16;
      word_asize = 'h';
      break;
    case 4:
      word_type = builtin_type_int32;
      word_asize = 'w';
      break;
    case 8:
      word_type = builtin_type_int64;
      word_asize = 'g';
      break;
    default:
      word_type = builtin_type_int8;
      word_asize = 'b';
    }
  /* The number of rows */
  nr_rows = atol (argv[3]);
  if (nr_rows <= 0)
    {
      mi_error_message = xstrprintf ("mi_cmd_data_read_memory: invalid number of rows.");
      return MI_CMD_ERROR;
    }
  /* number of bytes per row. */
  nr_cols = atol (argv[4]);
  if (nr_cols <= 0)
    {
      mi_error_message = xstrprintf ("mi_cmd_data_read_memory: invalid number of columns.");
      return MI_CMD_ERROR;
    }
  /* The un-printable character when printing ascii. */
  if (argc == 6)
    aschar = *argv[5];
  else
    aschar = 0;

  /* create a buffer and read it in. */
  total_bytes = word_size * nr_rows * nr_cols;
  mbuf = xcalloc (total_bytes, 1);
  make_cleanup (xfree, mbuf);

  nr_bytes = target_read (&current_target, TARGET_OBJECT_MEMORY, NULL,
			  mbuf, addr, total_bytes);
  if (nr_bytes <= 0)
    {
      do_cleanups (cleanups);
      mi_error_message = xstrdup ("Unable to read memory.");
      return MI_CMD_ERROR;
    }

  /* output the header information. */
  ui_out_field_core_addr (uiout, "addr", addr);
  ui_out_field_int (uiout, "nr-bytes", nr_bytes);
  ui_out_field_int (uiout, "total-bytes", total_bytes);
  ui_out_field_core_addr (uiout, "next-row", addr + word_size * nr_cols);
  ui_out_field_core_addr (uiout, "prev-row", addr - word_size * nr_cols);
  ui_out_field_core_addr (uiout, "next-page", addr + total_bytes);
  ui_out_field_core_addr (uiout, "prev-page", addr - total_bytes);

  /* Build the result as a two dimentional table. */
  {
    struct ui_stream *stream = ui_out_stream_new (uiout);
    struct cleanup *cleanup_list_memory;
    int row;
    int row_byte;
    cleanup_list_memory = make_cleanup_ui_out_list_begin_end (uiout, "memory");
    for (row = 0, row_byte = 0;
	 row < nr_rows;
	 row++, row_byte += nr_cols * word_size)
      {
	int col;
	int col_byte;
	struct cleanup *cleanup_tuple;
	struct cleanup *cleanup_list_data;
	cleanup_tuple = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);
	ui_out_field_core_addr (uiout, "addr", addr + row_byte);
	/* ui_out_field_core_addr_symbolic (uiout, "saddr", addr + row_byte); */
	cleanup_list_data = make_cleanup_ui_out_list_begin_end (uiout, "data");
	for (col = 0, col_byte = row_byte;
	     col < nr_cols;
	     col++, col_byte += word_size)
	  {
	    if (col_byte + word_size > nr_bytes)
	      {
		ui_out_field_string (uiout, NULL, "N/A");
	      }
	    else
	      {
		ui_file_rewind (stream->stream);
		print_scalar_formatted (mbuf + col_byte, word_type, word_format,
					word_asize, stream->stream);
		ui_out_field_stream (uiout, NULL, stream);
	      }
	  }
	do_cleanups (cleanup_list_data);
	if (aschar)
	  {
	    int byte;
	    ui_file_rewind (stream->stream);
	    for (byte = row_byte; byte < row_byte + word_size * nr_cols; byte++)
	      {
		if (byte >= nr_bytes)
		  {
		    fputc_unfiltered ('X', stream->stream);
		  }
		else if (mbuf[byte] < 32 || mbuf[byte] > 126)
		  {
		    fputc_unfiltered (aschar, stream->stream);
		  }
		else
		  fputc_unfiltered (mbuf[byte], stream->stream);
	      }
	    ui_out_field_stream (uiout, "ascii", stream);
	  }
	do_cleanups (cleanup_tuple);
      }
    ui_out_stream_delete (stream);
    do_cleanups (cleanup_list_memory);
  }
  do_cleanups (cleanups);
  return MI_CMD_DONE;
}

/* DATA-MEMORY-WRITE:

   COLUMN_OFFSET: optional argument. Must be preceeded by '-o'. The
   offset from the beginning of the memory grid row where the cell to
   be written is.
   ADDR: start address of the row in the memory grid where the memory
   cell is, if OFFSET_COLUMN is specified. Otherwise, the address of
   the location to write to.
   FORMAT: a char indicating format for the ``word''. See 
   the ``x'' command.
   WORD_SIZE: size of each ``word''; 1,2,4, or 8 bytes
   VALUE: value to be written into the memory address.

   Writes VALUE into ADDR + (COLUMN_OFFSET * WORD_SIZE).

   Prints nothing. */
enum mi_cmd_result
mi_cmd_data_write_memory (char *command, char **argv, int argc)
{
  CORE_ADDR addr;
  char word_format;
  long word_size;
  /* FIXME: ezannoni 2000-02-17 LONGEST could possibly not be big
     enough when using a compiler other than GCC. */
  LONGEST value;
  void *buffer;
  struct cleanup *old_chain;
  long offset = 0;
  int optind = 0;
  char *optarg;
  enum opt
    {
      OFFSET_OPT
    };
  static struct mi_opt opts[] =
  {
    {"o", OFFSET_OPT, 1},
    {0, 0, 0},
  };

  while (1)
    {
      int opt = mi_getopt ("mi_cmd_data_write_memory", argc, argv, opts,
			   &optind, &optarg);
      if (opt < 0)
	break;
      switch ((enum opt) opt)
	{
	case OFFSET_OPT:
	  offset = atol (optarg);
	  break;
	}
    }
  argv += optind;
  argc -= optind;

  if (argc != 4)
    {
      mi_error_message = xstrprintf ("mi_cmd_data_write_memory: Usage: [-o COLUMN_OFFSET] ADDR FORMAT WORD-SIZE VALUE.");
      return MI_CMD_ERROR;
    }

  /* Extract all the arguments. */
  /* Start address of the memory dump. */
  addr = parse_and_eval_address (argv[0]);
  /* The format character to use when displaying a memory word. See
     the ``x'' command. */
  word_format = argv[1][0];
  /* The size of the memory word. */
  word_size = atol (argv[2]);

  /* Calculate the real address of the write destination. */
  addr += (offset * word_size);

  /* Get the value as a number */
  value = parse_and_eval_address (argv[3]);
  /* Get the value into an array */
  buffer = xmalloc (word_size);
  old_chain = make_cleanup (xfree, buffer);
  store_signed_integer (buffer, word_size, value);
  /* Write it down to memory */
  write_memory (addr, buffer, word_size);
  /* Free the buffer.  */
  do_cleanups (old_chain);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_enable_timings (char *command, char **argv, int argc)
{
  if (argc == 0)
    do_timings = 1;
  else if (argc == 1)
    {
      if (strcmp (argv[0], "yes") == 0)
	do_timings = 1;
      else if (strcmp (argv[0], "no") == 0)
	do_timings = 0;
      else
	goto usage_error;
    }
  else
    goto usage_error;
    
  return MI_CMD_DONE;

 usage_error:
  error ("mi_cmd_enable_timings: Usage: %s {yes|no}", command);
  return MI_CMD_ERROR;
  
}

enum mi_cmd_result
mi_cmd_mi_verify_command (char *command, char **argv, int argc)
{
  char 		*command_name = argv[0];
  struct mi_cmd *cmd;
  
  if (argc != 1)
    {
      error ("mi_cmd_mi_verify_command: Usage: MI_COMMAND_NAME.");
    }

  cmd = mi_lookup (command_name);

  ui_out_field_string (uiout, "name", command_name);
  if (cmd != NULL) 
    {
       ui_out_field_string (uiout, "defined", "true");
       ui_out_field_string (uiout, "implemented",
            ((cmd->cli.cmd != NULL) ||
             (cmd->argv_func != NULL) ||
             (cmd->args_func != NULL)) ? "true" : "false");
    }
  else 
    {
       ui_out_field_string (uiout, "defined", "false");
    }
  
  return MI_CMD_DONE;
}


enum mi_cmd_result
mi_cmd_pid_info (char *command, char **argv, int argc)
{
  if (argc != 0)
    {
      error ("mi_cmd_pid_info: Usage: -pid-info");
    }

  pid_info (NULL, 1);

  return MI_CMD_DONE;
}

enum mi_cmd_result
mi_cmd_mi_no_op (char *command, char **argv, int argc)
{
  /* how does one know when a bunch of MI commands have finished being processed?
     just send a no-op as the last command and look for that...
   */
  return MI_CMD_DONE;
}

/* Execute a command within a safe environment.  Return >0 for
   ok. Return <0 for supress prompt.  Return 0 to have the error
   extracted from error_last_message(). 

   args->action will tell mi_execute_command what action
   to perfrom after the given command has executed (display/supress
   prompt, display error). */

static void
captured_mi_execute_command (struct ui_out *uiout, void *data)
{
  struct captured_mi_execute_command_args *args =
    (struct captured_mi_execute_command_args *) data;
  struct mi_parse *context = args->command;

  struct ui_out *saved_uiout = uiout;
  struct mi_timestamp cmd_finished;

  switch (context->op)
    {

    case MI_COMMAND:
      /* A MI command was read from the input stream */
      if (mi_debug_p)
	/* FIXME: gdb_???? */
	fprintf_unfiltered (raw_stdout, " token=`%s' command=`%s' args=`%s'\n",
			    context->token, context->command, context->args);
      /* FIXME: cagney/1999-09-25: Rather than this convoluted
         condition expression, each function should return an
         indication of what action is required and then switch on
         that. */
      args->action = EXECUTE_COMMAND_DISPLAY_PROMPT;

      /* This is a bit of a hack.  We need to pass the cmd_start down to
	 the mi command so that it can be copied into continuations if
	 needs be.  But we don't pass the parse but just a few bits
	 instead.  So we need to route it through this instead... */
      
      current_command_token = context->token;

      if (do_timings)
	current_command_ts = context->cmd_start;

      /* Set this to 0 so we don't mistakenly think this command
        caused the target to run under interpreter-exec.  */
      mi_interp_exec_cmd_did_run = 0;
      args->rc = mi_cmd_execute (context);

      /* Check if CURRENT_COMMAND_TS has been nulled out ... if the
         mi_cmd_execute command completed the command and printed out
         the timing information already (e.g. see mi_execute_async_cli_command)
         we don't want to double-print.  */
      if (current_command_ts == NULL)
        context->cmd_start = NULL;

      if (do_timings)
        {
          timestamp (&cmd_finished);
          if (context->cmd_start)
            end_remote_counts (context->cmd_start);
        }
      
      if (!target_can_async_p () && mi_interp_exec_cmd_did_run)
        {
          fputs_unfiltered ("(gdb) \n", raw_stdout);
          gdb_flush (raw_stdout);
          if (current_command_token)
            {
              fputs_unfiltered (current_command_token, raw_stdout);
            }
          fputs_unfiltered ("*stopped", raw_stdout);
          mi_out_put (saved_uiout, raw_stdout);
          mi_out_rewind (saved_uiout);
          fputs_unfiltered ("\n", raw_stdout);
        }
      else if (!target_can_async_p () || !target_executing
	  || mi_command_completes_while_target_executing (context->command))
	{
	  /* print the result if there were no errors 
	   
	     Remember that on the way out of executing a command, you have
	     to directly use the mi_interp's uiout, since the command could 
	     have reset the interpreter, in which case the current uiout 
	     will most likely crash in the mi_out_* routines. 
	  */
			    
	  if (args->rc == MI_CMD_DONE)
	    {
	      fputs_unfiltered (context->token, raw_stdout);
	      fputs_unfiltered ("^done", raw_stdout);
	      mi_out_put (saved_uiout, raw_stdout);
	      mi_out_rewind (saved_uiout);
	      /* Have to check cmd_start, since the command could be
		 "mi-enable-timings". */
	      if (do_timings && context->cmd_start)
                {
		  print_diff (context->cmd_start, &cmd_finished);
                }
	      fputs_unfiltered ("\n", raw_stdout);
	    }
	  else if (args->rc == MI_CMD_QUIET)
	    {
	      /* Just need to flush and print the timings here. */
	      
	      mi_out_put (saved_uiout, raw_stdout);
	      mi_out_rewind (saved_uiout);
	      if (do_timings && context->cmd_start)
		  print_diff (context->cmd_start, &cmd_finished);
	      fputs_unfiltered ("\n", raw_stdout);
	    }
	  else if (args->rc == MI_CMD_ERROR)
	    {
	      fputs_unfiltered (context->token, raw_stdout);
	      fputs_unfiltered ("^error", raw_stdout);
	      if (mi_error_message)
		{
		  fputs_unfiltered (",msg=\"", raw_stdout);
		  fputstr_unfiltered (mi_error_message, '"', raw_stdout);
		  xfree (mi_error_message);
		}
	      fputs_unfiltered ("\"\n", raw_stdout);
	      mi_out_rewind (saved_uiout);
	    }
	  else
	    mi_out_rewind (saved_uiout);
	}
      else if (sync_execution)
	{
	  /* Don't print the prompt. We are executing the target in
	     synchronous mode. */
	  args->action = EXECUTE_COMMAND_SUPRESS_PROMPT;
	  return;
	}
      break;

    case CLI_COMMAND:
      {
	/* A CLI command was read from the input stream.  */
	/* This "feature" will be removed as soon as we have a
	   complete set of mi commands.  */
	/* Echo the command on the console.  */
	fprintf_unfiltered (gdb_stdlog, "%s\n", context->command);

	/* APPLE LOCAL: Don't use -interpreter-exec, since that will
	   set the uiout to be the console uiout, which is not at all
	   what we want here.  Instead, stick with the old behavior of
	   mi_execute_cli_command, which isn't perfect either.  */

#if 0
	char *argv[2];
	/* Call the "console" interpreter.  */
	argv[0] = "console";
	argv[1] = context->command;
	mi_cmd_interpreter_exec ("-interpreter-exec", argv, 2);
#endif

	/* FIXME: If the command string has something that looks like
	   a format spec (e.g. %s) we will get a core dump */
	mi_execute_cli_command (context->command, 0, NULL);

	/* If we changed interpreters, DON'T print out anything. */
	/* APPLE LOCAL: Go ahead and print it regardless, for now. We
	   should be fixing up the interpreter-switching stuff
	   elsewhere. */
	if (1
	    || current_interp_named_p (INTERP_MI)
	    || current_interp_named_p (INTERP_MI1)
	    || current_interp_named_p (INTERP_MI2)
	    || current_interp_named_p (INTERP_MI3))
	  {
	    /* print the result */
	    /* FIXME: Check for errors here. */
	    fputs_unfiltered (context->token, raw_stdout);
	    fputs_unfiltered ("^done", raw_stdout);
	    mi_out_put (saved_uiout, raw_stdout);
	    mi_out_rewind (saved_uiout);
	    fputs_unfiltered ("\n", raw_stdout);
	    args->action = EXECUTE_COMMAND_DISPLAY_PROMPT;
	    args->rc = MI_CMD_DONE;
	  }
	break;
      }
    }

  return;
}


void
mi_interpreter_exec_continuation (struct continuation_arg *in_arg)
{
  struct mi_continuation_arg *arg 
    = (struct mi_continuation_arg *) in_arg;

  if (!target_executing) 
    {
      /* This is a little tricky because bpstat_do_actions can restart
	 the inferior.  So first say we have stopped, and flush the
	 output so we get the reason aligned correctly, then run the
	 breakpoint actions, and if they have restarted the inferior,
	 suppress the prompt. */

      if (arg->cleanups != NULL)
	do_exec_cleanups (arg->cleanups);

      if (arg && arg->token)
	fputs_unfiltered (arg->token, raw_stdout);

      fputs_unfiltered ("*stopped", raw_stdout);
      if (do_timings && arg && arg->timestamp)
        {
          end_remote_counts (arg->timestamp);
	  print_diff_now (arg->timestamp);
        }
      mi_out_put (uiout, raw_stdout);
      fputs_unfiltered ("\n", raw_stdout);
      
      /* Tricky point - we need to add this continuation 
	 before we run the actions, since one of the breakpoint commands
	 could have added a continuation, and ours would be in
	 front of theirs, and then the cleanups would be out of order. */

      if (target_can_async_p()) 
	{
	  if (arg && arg->timestamp)
            {
	      timestamp (arg->timestamp);
              start_remote_counts (arg->timestamp, arg->token);
            }
	  
	  add_continuation (mi_interpreter_exec_continuation, 
			  (struct continuation_arg *) arg);
	}

      bpstat_do_actions (&stop_bpstat);
      
      if (!target_executing)
	{
	  if (target_can_async_p ())
	    {
	      discard_all_continuations ();
	      free_continuation_arg (arg);
	    }
	  fputs_unfiltered ("(gdb) \n", raw_stdout);
	}
      else
	{
	  ui_out_field_string (uiout, "reason", "breakpoint-command");
	  if (arg && arg->token)
	    fputs_unfiltered (arg->token, raw_stdout);
	  fputs_unfiltered ("*started", raw_stdout);
	  if (do_timings && arg && arg->timestamp)
            {
              end_remote_counts (arg->timestamp);
	      print_diff_now (arg->timestamp);
            }
	  mi_out_put (uiout, raw_stdout);
	  fputs_unfiltered ("\n", raw_stdout);
	}
      
      gdb_flush (raw_stdout);
      
    }
  else if (target_can_async_p()) 
    {
      add_continuation (mi_interpreter_exec_continuation, in_arg);
    }
}

void
mi_execute_command (char *cmd, int from_tty)
{
  struct mi_parse *command;
  struct ui_out *saved_uiout = uiout;
  struct captured_mi_execute_command_args args;

  args.rc = MI_CMD_DONE;

  /* This is to handle EOF (^D). We just quit gdb. */
  /* FIXME: we should call some API function here. */
  if (cmd == 0)
    quit_force (NULL, from_tty);

  command = mi_parse (cmd);

  if (command != NULL)
    {
      struct gdb_exception result;
      /* FIXME: cagney/1999-11-04: Can this use of catch_exceptions either
         be pushed even further down or even eliminated? */
      if (do_timings)
	{
	  command->cmd_start = (struct mi_timestamp *)
	    xmalloc (sizeof (struct mi_timestamp));
	  timestamp (command->cmd_start);
          start_remote_counts (command->cmd_start, command->token);
	}

      args.command = command;
      result = catch_exception (uiout, captured_mi_execute_command, &args,
				RETURN_MASK_ALL);
      exception_print (gdb_stderr, result);

      if (args.action == EXECUTE_COMMAND_SUPRESS_PROMPT)
	{
	  /* The command is executing synchronously.  Bail out early
	     suppressing the finished prompt. */
	  mi_parse_free (command);
	  return;
	}
      if (result.reason < 0)
	{
	  /* The command execution failed and error() was called
	     somewhere. Try to dump the accumulated output from the
	     command. */
          ui_out_cleanup_after_error (saved_uiout);
	  fputs_unfiltered (command->token, raw_stdout);
	  fputs_unfiltered ("^error,msg=\"", raw_stdout);
	  fputstr_unfiltered (result.message, '"', raw_stdout);
	  fputs_unfiltered ("\"", raw_stdout);
          mi_out_put (saved_uiout, raw_stdout);
          mi_out_rewind (saved_uiout);
	  fputs_unfiltered ("\n", raw_stdout);
	}
      mi_parse_free (command);
    }

  if (args.rc != MI_CMD_QUIET)
    {
	if (current_interp_named_p (INTERP_MI)
	    || current_interp_named_p (INTERP_MI1)
	    || current_interp_named_p (INTERP_MI2)
	    || current_interp_named_p (INTERP_MI3))
	  fputs_unfiltered ("(gdb) \n", raw_stdout);
	else
	  display_gdb_prompt (NULL);
	
      /* print any buffered hook code */
      /* ..... */
    }

  gdb_flush (raw_stdout);
}

static int 
mi_command_completes_while_target_executing (char *command)
{
  if (strcmp (command, "exec-interrupt")
      && strcmp (command, "exec-status")
      && strcmp (command, "pid-info"))
    return 0;
  else
    return 1;
}

static enum mi_cmd_result
mi_cmd_execute (struct mi_parse *parse)
{
  if (parse->cmd->argv_func != NULL
      || parse->cmd->args_func != NULL)
    {
      
      if (target_executing)
	{
	  if (!mi_command_completes_while_target_executing(parse->command))
	    {
	      fputs_unfiltered (parse->token, raw_stdout);
	      fputs_unfiltered ("^error,msg=\"", raw_stdout);
	      fputs_unfiltered ("Cannot execute command ", raw_stdout);
	      fputstr_unfiltered (parse->command, '"', raw_stdout);
	      fputs_unfiltered (" while target running", raw_stdout);
	      fputs_unfiltered ("\"\n", raw_stdout);
	      return MI_CMD_ERROR;
	    }
	}

      /* FIXME: DELETE THIS! */
      if (parse->cmd->args_func != NULL)
	return parse->cmd->args_func (parse->args, 0 /*from_tty */ );
      return parse->cmd->argv_func (parse->command, parse->argv, parse->argc);
    }
  else if (parse->cmd->cli.cmd != 0)
    {
      /* FIXME: DELETE THIS. */
      /* The operation is still implemented by a cli command */
      /* Must be a synchronous one */
      mi_execute_cli_command (parse->cmd->cli.cmd, parse->cmd->cli.args_p,
			      parse->args);
      return MI_CMD_DONE;
    }
  else
    {
      /* FIXME: DELETE THIS. */
      fputs_unfiltered (parse->token, raw_stdout);
      fputs_unfiltered ("^error,msg=\"", raw_stdout);
      fputs_unfiltered ("Undefined mi command: ", raw_stdout);
      fputstr_unfiltered (parse->command, '"', raw_stdout);
      fputs_unfiltered (" (missing implementation)", raw_stdout);
      fputs_unfiltered ("\"\n", raw_stdout);
      return MI_CMD_ERROR;
    }
}

 void
mi_execute_command_wrapper (char *cmd)
{
  mi_execute_command (cmd, stdin == instream);
}

/* FIXME: This is just a hack so we can get some extra commands going.
   We don't want to channel things through the CLI, but call libgdb directly */
/* Use only for synchronous commands */

void
mi_execute_cli_command (const char *cmd, int args_p, char *args)
{
  if (cmd != 0)
    {
      struct cleanup *old_cleanups;
      char *run;
      if (args_p)
	run = xstrprintf ("%s %s", cmd, args);
      else
        run = xstrdup (cmd);
      if (mi_debug_p)
        /* FIXME: gdb_???? */
        fprintf_unfiltered (gdb_stdout, "cli=%s run=%s\n",
                            cmd, run);
      old_cleanups = make_cleanup (xfree, run);
      execute_command ( /*ui */ run, 0 /*from_tty */ );
      do_cleanups (old_cleanups);
      return;
    }
}

enum mi_cmd_result
mi_execute_async_cli_command (char *mi, char *args, int from_tty)
{
  char *run;
  char *async_args;

  if (!target_can_async_p ())
    {
      struct cleanup *old_cleanups;
      xasprintf (&run, "%s %s", mi, args);
      old_cleanups = make_cleanup (xfree, run);

      /* NOTE: For synchronous targets asynchronous behavour is faked by
         printing out the GDB prompt before we even try to execute the
         command. */
      if (current_command_token)
	fputs_unfiltered (current_command_token, raw_stdout);
      fputs_unfiltered ("^running\n", raw_stdout);
      fputs_unfiltered ("(gdb) \n", raw_stdout);
      gdb_flush (raw_stdout);
      
      execute_command ( /*ui */ run, 0 /*from_tty */ );

      /* Do this before doing any printing.  It would appear that some
         print code leaves garbage around in the buffer. */
      do_cleanups (old_cleanups);

      /* We should do whatever breakpoint actions are pending.
       This works even if the breakpoint command causes the target
       to continue, but we won't exit bpstat_do_actions here till
       the target is all the way stopped.

       Note, I am doing this before putting out the *stopped.  If we
       told the UI that we had stopped & continued again, we would
       have to figure out how to tell it that as well.  This is
       easier.  If the command continues the target, the UI will
       just think it hasn't stopped yet, which is probably also
       okay.  */
       bpstat_do_actions (&stop_bpstat);

      /* If the target was doing the operation synchronously we fake
         the stopped message. */
      if (current_command_token)
        {
	  fputs_unfiltered (current_command_token, raw_stdout);
        }
      fputs_unfiltered ("*stopped", raw_stdout);
      if (current_command_ts)
        {
          end_remote_counts (current_command_ts);
	  print_diff_now (current_command_ts);
          xfree (current_command_ts);
          current_command_ts = NULL;  /* Indicate that the ts was printed */
        }
      return MI_CMD_QUIET;
    }
  else
    {
      struct mi_continuation_arg *arg = NULL; 
      struct cleanup *old_cleanups = NULL;
      volatile struct gdb_exception except;

      async_args = (char *) xmalloc (strlen (args) + 2);
      old_cleanups = make_cleanup (free, async_args);
      strcpy (async_args, args);
      strcat (async_args, "&");
      xasprintf (&run, "%s %s", mi, async_args);
      make_cleanup (free, run);

      /* Transfer the command token to the continuation.  That
	 will now print the results associated with this command. 
         Tricky point: have to add the continuation BEFORE running
         execute_command, or it will get run before any continuations
         that might get added by execute_command, in which case the
         cleanups will be out of order. */
      
      arg = mi_setup_continuation_arg (NULL);
      add_continuation (mi_exec_async_cli_cmd_continuation, 
			(struct continuation_arg *) arg);

      arg->exec_error_cleanups 
	= make_exec_error_cleanup (mi_exec_error_cleanup, (void *) arg);

      except = safe_execute_command (uiout, /*ui */ run, 0 /*from_tty */ );
      do_cleanups (old_cleanups);

      if (target_executing)
	{
	  if (current_command_token)
	    fputs_unfiltered (current_command_token, raw_stdout);
	  fputs_unfiltered ("^running\n", raw_stdout);
	  
	}
      /* APPLE LOCAL begin inlined subroutine  */
      /* If we are stepping from an inlined subroutine call site into the
	 inlined subroutine, the target will not be executing, but it is
	 not an error.  */
      else if (strcmp (mi, "step") == 0
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
	  mi_exec_async_cli_cmd_continuation ((struct continuation_arg *) arg);
	}
      /* APPLE LOCAL end inlined subroutine  */
      else
	{
	  /* If we didn't manage to set the inferior going, that's
	     most likely an error... */
	  discard_all_continuations ();
	  if (arg->exec_error_cleanups != (struct cleanup *) -1)
	    discard_exec_error_cleanups (arg->exec_error_cleanups);
	  free_continuation_arg (arg);
	  if (except.message != NULL)
	    mi_error_message = xstrdup (except.message);
	  else
	    mi_error_message = NULL;

	  return MI_CMD_ERROR;
	}

    }

  return MI_CMD_DONE;
}

void
mi_exec_error_cleanup (void *in_arg)
{
  struct mi_continuation_arg *arg =
    (struct mi_continuation_arg *) in_arg;
  struct ui_out *saved_ui_out = uiout;

  uiout = interp_ui_out (mi_interp);

  if (arg && arg->token)
    {
      fputs_unfiltered (arg->token, raw_stdout);
    }
  fputs_unfiltered ("*stopped", raw_stdout);
  ui_out_field_string (uiout, "reason", "error");
  if (do_timings && arg && arg->timestamp)
    {
      end_remote_counts (arg->timestamp);
      print_diff_now (arg->timestamp);
    }
  mi_out_put (uiout, raw_stdout);
  fputs_unfiltered ("\n", raw_stdout);
  fputs_unfiltered ("(gdb) \n", raw_stdout);
  gdb_flush (raw_stdout);
  uiout = saved_ui_out;
}

void
mi_exec_async_cli_cmd_continuation (struct continuation_arg *in_arg)
{
  struct mi_continuation_arg *arg =
    (struct mi_continuation_arg *) in_arg;

  if (!target_executing)
    {
      if (arg && arg->token)
	{
	  fputs_unfiltered (arg->token, raw_stdout);
	}

      /* Do the cleanups.  Remember to set this to NULL, 
	 since we are passing the arg to the next continuation
	 if the target restarts, and we don't want to do these
	 cleanups again. */
      if (arg->cleanups)
	{
	  do_exec_cleanups (arg->cleanups);
	  arg->cleanups = NULL;
	}
      
      if (arg->exec_error_cleanups != (struct cleanup *) -1)
	{
	  discard_exec_error_cleanups (arg->exec_error_cleanups);
	  arg->exec_error_cleanups = (struct cleanup *) -1;
	}

      fputs_unfiltered ("*stopped", raw_stdout);
      if (do_timings && arg && arg->timestamp)
        {
          end_remote_counts (arg->timestamp);
	  print_diff_now (arg->timestamp);
        }
      mi_out_put (uiout, raw_stdout);
      fputs_unfiltered ("\n", raw_stdout);
      
      /* Now run the actions for this breakpoint.  This may start
	 the target going again, but we shouldn't have to do
	 anything special about that, since the continuation
	 hooks for the commands will take care of that. */

      /* Tricky bit: we have to register the continuation before
	 calling bpstat_do_actions, or the continuations will
	 be out of order.  If we don't start, then we have to 
         discard the continuations.  N.B. We are assuming here
         that nothing that CALLED us has registered a continuation,
         if that becomes a problem, we will need to add the ability
         to discard continuations up to (and including) a given 
         continuation. */

      if (target_can_async_p ())
	{
	  /* Reset the timer, we will just accumulate times for this
	     command. */
	  if (do_timings && arg && arg->timestamp)
            {
	      timestamp (arg->timestamp);
              start_remote_counts (arg->timestamp, arg->token);
            }
	  add_continuation (mi_exec_async_cli_cmd_continuation, 
			    (struct continuation_arg *) arg);
	}

      bpstat_do_actions (&stop_bpstat);
      
      if (!target_executing)
	{
	  /* Okay, we didn't need to use the continuation,
	     so discard it now. */
	  if (target_can_async_p ())
	    {
	      discard_all_continuations ();
	      free_continuation_arg (arg);
	    }
	  fputs_unfiltered ("(gdb) \n", raw_stdout);
	  gdb_flush (raw_stdout);
	}
      else
	{
	  if (arg && arg->token)
	    fputs_unfiltered (arg->token, raw_stdout);
	  
	  ui_out_field_string (uiout, "reason", "breakpoint-command");
	  fputs_unfiltered ("*started", raw_stdout);
	  mi_out_put (uiout, raw_stdout);
	  fputs_unfiltered ("\n", raw_stdout);
	  gdb_flush (raw_stdout);
	}	  
    }
  else if (target_can_async_p ())
    {
      add_continuation (mi_exec_async_cli_cmd_continuation, 
                        (struct continuation_arg *) in_arg);
    }
}

void
mi_setup_architecture_data (void)
{
  old_regs = xmalloc ((NUM_REGS + NUM_PSEUDO_REGS) * MAX_REGISTER_SIZE + 1);
  memset (old_regs, 0, (NUM_REGS + NUM_PSEUDO_REGS) * MAX_REGISTER_SIZE + 1);
}

void
_initialize_mi_main (void)
{
  DEPRECATED_REGISTER_GDBARCH_SWAP (old_regs);
  deprecated_register_gdbarch_swap (NULL, 0, mi_setup_architecture_data);

  /* APPLE LOCAL begin mi */
  /* Lets create a gdb "set" variable to control mi timings.  This
     seems gross, but it will allow control from the .gdbinit. */
  add_setshow_boolean_cmd ("mi-timings-enabled", class_obscure,
			   &do_timings, _("\
Set whether timing information is displayed for mi commands."), _("\
Show whether timing information is displayed for mi commands."), NULL,
			   NULL, NULL,
			   &setlist, &showlist);
  /* APPLE LOCAL end mi */
}

/* APPLE LOCAL begin mi */
/* This is kind of a hack.  When we run a breakpoint command in the mi,
   execute_control_command is going to do mi_cmd_interpreter_exec so that
   it will be treated as a cli command.  But if that command sets the 
   target running, then it will register a continuation.  But the real
   command that set it going will already be doing that.  So we just
   flag mi_cmd_interpreter_exec not to register the continuation in
   this case. */

int mi_dont_register_continuation = 0;

void
mi_interpreter_exec_bp_cmd (char *command, char **argv, int argc)
{
  mi_dont_register_continuation = 1;
  mi_cmd_interpreter_exec (command, argv, argc);
  mi_dont_register_continuation = 0;
}

/* Use this (or rather the *_notification functions that call it) for 
   notifications in all the hooks that get installed when another 
   interpreter is executing.  That will ensure that the notifications
   come out raw, and don't end up getting wrapped (as will happen, for
   instance, with console-quoted). */

static void
route_output_through_mi (char *prefix, char *notification)
{
  static struct ui_file *rerouting_ui_file = NULL;

  if (rerouting_ui_file == NULL)
    {
      rerouting_ui_file =  stdio_fileopen (stdout);
    }

  fprintf_unfiltered (rerouting_ui_file, "%s%s\n", prefix, notification);
  gdb_flush (rerouting_ui_file);

}

static void
route_output_to_mi_result (const char *name, const char *string)
{
  struct ui_out *mi_uiout;
  if (mi_interp != NULL)
    {
      mi_uiout = interp_ui_out (mi_interp);
      ui_out_field_string (mi_uiout, name, string);
    }
}

void
mi_output_async_notification (char *notification)
{
  route_output_through_mi ("=", notification);
}

static void
output_control_change_notification(char *notification)
{
  route_output_through_mi ("^", notification);
}

void
mi_interp_stepping_command_hook ()
{
  output_control_change_notification("stepping");
}

void
mi_interp_continue_command_hook ()
{
  output_control_change_notification("continuing");
}

static void
mi_interp_sync_fake_running ()
{
  char *prefix;
  int free_me = 0;

  if (current_command_token)
    {
      prefix = xmalloc (strlen (current_command_token) + 2);
      sprintf (prefix, "%s^", current_command_token);
      free_me = 1;
    }
  else
    prefix = "^";

  route_output_through_mi (prefix,"running");
  if (free_me)
    xfree (prefix);
  ui_out_set_annotation_printer (route_output_to_mi_result);
}

void
mi_interp_sync_stepping_command_hook ()
{
  mi_interp_exec_cmd_did_run = 1;
  output_control_change_notification("stepping");
  mi_interp_sync_fake_running ();
}

void
mi_interp_sync_continue_command_hook ()
{
  mi_interp_exec_cmd_did_run = 1;
  output_control_change_notification("continuing");
  mi_interp_sync_fake_running ();
}

int
mi_interp_run_command_hook ()
{
  /* request that the ide initiate a restart of the target */
  mi_output_async_notification ("rerun");
  return 0;
}

void
mi_interp_hand_call_function_hook ()
{
  /* Notify if the user is causing a function to be called
     when the scheduler is not locked.  This may cause the stack
     on another thread to change, and so the UI should refresh it's
     stack info.  */

    if (!scheduler_lock_on_p ()) 
      {
	struct cleanup *list_cleanup;
	struct ui_out *saved_ui_out = uiout;

	uiout = interp_ui_out (mi_interp);
	
	list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "MI_HOOK_RESULT");
	ui_out_field_string (uiout, "HOOK_TYPE", "function-called");
	do_cleanups (list_cleanup);
	uiout = saved_ui_out;
#if 0
  mi_output_async_notification ("rerun");
#endif	
      }
}

/* mi_setup_continuation_arg - sets up a continuation structure
   with the timer info and the command token, for use with
   an asyncronous mi command.  Will only cleanup the exec_cleanup
   chain back to CLEANUPS, or not at all if CLEANUPS is NULL. */

struct mi_continuation_arg *
mi_setup_continuation_arg (struct cleanup *cleanups)
{
  struct mi_continuation_arg *arg
    = (struct mi_continuation_arg *) 
    xmalloc (sizeof (struct mi_continuation_arg));

  if (current_command_token)
    {
      arg->token = xstrdup (current_command_token);
    }
  else
    arg->token = NULL;

  if (do_timings && current_command_ts)
    {
      arg->timestamp = (struct mi_timestamp *) 
	xmalloc (sizeof (struct mi_timestamp));
      copy_timestamp (arg->timestamp, current_command_ts);
      current_command_ts = NULL;
    }
  else
    arg->timestamp = NULL;

  arg->cleanups = cleanups;
  arg->exec_error_cleanups = (struct cleanup *) -1;

  return arg;
}

static void
free_continuation_arg (struct mi_continuation_arg *arg)
{
  if (arg)
    {
      if (arg->token)
	xfree (arg->token);
      if (arg->timestamp)
        {
          end_remote_counts (arg->timestamp);
	  xfree (arg->timestamp);
          arg->timestamp = NULL;
        }
      xfree (arg);
    }
}

/* The only three called from other parts of mi-main.c will probably be
   timestamp(), print_diff_now() and copy_timestamp() */

static void 
timestamp (struct mi_timestamp *tv)
{
  gettimeofday (&tv->wallclock, NULL);
  getrusage (RUSAGE_SELF, &tv->rusage);
  tv->remotestats.mi_token[0] = '\0';
  tv->remotestats.assigned_to_global = 0;
}

/* The remote protocol packet counts are updated over in remote.c
   which won't have access to the saved timestamps that are handled
   here in mi-main.c so we need to push/pop a "current remote stats"
   global whenever we start/end recording timestamp.. */

static void
start_remote_counts (struct mi_timestamp *tv, const char *token)
{
  /* Initialize the gdb remote packet counts; set this as the current
     remote stats struct.  */
  tv->remotestats.pkt_sent = 0;
  tv->remotestats.pkt_recvd = 0;
  tv->remotestats.acks_sent = 0;
  tv->remotestats.acks_recvd = 0;
  timerclear (&tv->remotestats.totaltime);
  tv->saved_remotestats = current_remote_stats;
  if (token)
    {
      int bufsz = sizeof (tv->remotestats.mi_token);
      strncpy (tv->remotestats.mi_token, token, bufsz - 1);
      tv->remotestats.mi_token[bufsz - 1] = '\0';
    }
  else
    tv->remotestats.mi_token[0] = '\0';
  current_remote_stats = &tv->remotestats;
  tv->remotestats.assigned_to_global = 1;
}

static void
end_remote_counts (struct mi_timestamp *tv)
{
  if (tv && tv->remotestats.assigned_to_global)
    {
      current_remote_stats = tv->saved_remotestats;
      tv->remotestats.assigned_to_global = 0;
    }
}

static void 
print_diff_now (struct mi_timestamp *start)
{
  struct mi_timestamp now;
  timestamp (&now);
  print_diff (start, &now);
}

static void
copy_timestamp (struct mi_timestamp *dst, struct mi_timestamp *src)
{
  memcpy (dst, src, sizeof (struct mi_timestamp));
}

static void 
print_diff (struct mi_timestamp *start, struct mi_timestamp *end)
{
  fprintf_unfiltered (raw_stdout,
     ",time={wallclock=\"%0.5f\",user=\"%0.5f\",system=\"%0.5f\",start=\"%d.%06d\",end=\"%d.%06d\"}", 
     wallclock_diff (start, end) / 1000000.0, 
     user_diff (start, end) / 1000000.0, 
     system_diff (start, end) / 1000000.0,
     (int) start->wallclock.tv_sec, (int) start->wallclock.tv_usec,
     (int) end->wallclock.tv_sec, (int) end->wallclock.tv_usec);

  /* If we have any remote protocol stats, print them.  */
  if (start->remotestats.pkt_sent != 0 || start->remotestats.pkt_recvd != 0)
    {
      fprintf_unfiltered (raw_stdout,
        ",remotestats={packets_sent=\"%d\",packets_received=\"%d\","
        "acks_sent=\"%d\",acks_received=\"%d\","
        "time=\"%0.5f\","
        "total_packets_sent=\"%lld\",total_packets_received=\"%lld\"}",
        start->remotestats.pkt_sent, start->remotestats.pkt_recvd,
        start->remotestats.acks_sent, start->remotestats.acks_recvd,
        (double) ((start->remotestats.totaltime.tv_sec * 1000000) + start->remotestats.totaltime.tv_usec) / 1000000.0,
        total_packets_sent, total_packets_received);
    }
}

static long 
wallclock_diff (struct mi_timestamp *start, struct mi_timestamp *end)
{
  struct timeval result;
  timersub (&end->wallclock, &start->wallclock, &result);
  return result.tv_sec * 1000000 + result.tv_usec;
}

static long 
user_diff (struct mi_timestamp *start, struct mi_timestamp *end)
{
  struct timeval result;
  timersub (&(end->rusage.ru_utime), &(start->rusage.ru_utime), &result);
  return result.tv_sec * 1000000 + result.tv_usec;
}

static long 
system_diff (struct mi_timestamp *start, struct mi_timestamp *end)
{
  struct timeval result;
  timersub (&(end->rusage.ru_stime), &(start->rusage.ru_stime), &result);
  return result.tv_sec * 1000000 + result.tv_usec;
}
/* APPLE LOCAL end mi */
