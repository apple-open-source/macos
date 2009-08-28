/* Multi-process/thread control for GDB, the GNU debugger.

   Copyright 1986, 1987, 1988, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

   Contributed by Lynx Real-Time Systems, Inc.  Los Gatos, CA.

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
#include "frame.h"
#include "inferior.h"
#include "environ.h"
#include "value.h"
#include "target.h"
#include "gdbthread.h"
#include "exceptions.h"
#include "command.h"
#include "gdbcmd.h"
#include "regcache.h"
#include "gdb.h"
#include "gdb_string.h"
#include "wrapper.h"

#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include "ui-out.h"
/* APPLE LOCAL - subroutine inlining  */
#include "inlining.h"

#ifdef NM_NEXTSTEP
#include "macosx-nat-infthread.h"
#endif

/*#include "lynxos-core.h" */

/* Definition of struct thread_info exported to gdbthread.h */

/* Prototypes for exported functions. */

void _initialize_thread (void);

/* Prototypes for local functions. */

struct thread_info *thread_list = NULL;
int highest_thread_num;

static void thread_command (char *tidstr, int from_tty);
static void thread_apply_all_command (char *, int);
static int thread_alive (struct thread_info *);
static void info_threads_command (char *, int);
static void thread_apply_command (char *, int);
static void restore_current_thread (ptid_t, int);
/* APPLE LOCAL: I need this, move it to gdbthreads.h
static void prune_threads (void);
*/

void
delete_step_resume_breakpoint (void *arg)
{
  struct breakpoint **breakpointp = (struct breakpoint **) arg;
  struct thread_info *tp;

  if (*breakpointp != NULL)
    {
      delete_breakpoint (*breakpointp);
      for (tp = thread_list; tp; tp = tp->next)
	if (tp->step_resume_breakpoint == *breakpointp)
	  tp->step_resume_breakpoint = NULL;

      *breakpointp = NULL;
    }
}

static void
free_thread (struct thread_info *tp)
{
  /* NOTE: this will take care of any left-over step_resume breakpoints,
     but not any user-specified thread-specific breakpoints. */
  if (tp->step_resume_breakpoint)
    delete_breakpoint (tp->step_resume_breakpoint);

  /* FIXME: do I ever need to call the back-end to give it a
     chance at this private data before deleting the thread?  */
  if (tp->private)
    xfree (tp->private);

  xfree (tp);
}

void
init_thread_list (void)
{
  struct thread_info *tp, *tpnext;

  highest_thread_num = 0;
  if (!thread_list)
    return;

  for (tp = thread_list; tp; tp = tpnext)
    {
      tpnext = tp->next;
      free_thread (tp);
    }

  thread_list = NULL;
}

/* add_thread now returns a pointer to the new thread_info, 
   so that back_ends can initialize their private data.  */

struct thread_info *
add_thread (ptid_t ptid)
{
  struct thread_info *tp; 
 
  tp = (struct thread_info *) xmalloc (sizeof (*tp)); 
  memset (tp, 0, sizeof (*tp)); 
  tp->ptid = ptid; 
  tp->num = ++highest_thread_num; 
  tp->next = thread_list; 
  thread_list = tp; 
  return tp; 
}

void
delete_thread (ptid_t ptid)
{
  struct thread_info *tp, *tpprev;

  tpprev = NULL;

  for (tp = thread_list; tp; tpprev = tp, tp = tp->next)
    if (ptid_equal (tp->ptid, ptid))
      break;

  if (!tp)
    return;

  if (tpprev)
    tpprev->next = tp->next;
  else
    thread_list = tp->next;

  free_thread (tp);
}

struct thread_info *
find_thread_id (int num)
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if (tp->num == num)
      return tp;

  return NULL;
}

/* Find a thread_info by matching PTID.  */
struct thread_info *
find_thread_pid (ptid_t ptid)
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if (ptid_equal (tp->ptid, ptid))
      return tp;

  return NULL;
}

/*
 * Thread iterator function.
 *
 * Calls a callback function once for each thread, so long as
 * the callback function returns false.  If the callback function
 * returns true, the iteration will end and the current thread
 * will be returned.  This can be useful for implementing a 
 * search for a thread with arbitrary attributes, or for applying
 * some operation to every thread.
 *
 * FIXME: some of the existing functionality, such as 
 * "Thread apply all", might be rewritten using this functionality.
 */

struct thread_info *
iterate_over_threads (int (*callback) (struct thread_info *, void *),
		      void *data)
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if ((*callback) (tp, data))
      return tp;

  return NULL;
}

int
valid_thread_id (int num)
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if (tp->num == num)
      return 1;

  return 0;
}

int
pid_to_thread_id (ptid_t ptid)
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if (ptid_equal (tp->ptid, ptid))
      return tp->num;

  return 0;
}

ptid_t
thread_id_to_pid (int num)
{
  struct thread_info *thread = find_thread_id (num);
  if (thread)
    return thread->ptid;
  else
    return pid_to_ptid (-1);
}

int
in_thread_list (ptid_t ptid)
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if (ptid_equal (tp->ptid, ptid))
      return 1;

  return 0;			/* Never heard of 'im */
}

/* Print a list of thread ids currently known, and the total number of
   threads. To be used from within catch_errors. */
static int
do_captured_list_thread_ids (struct ui_out *uiout, void *arg)
{
  struct thread_info *tp;
  int num = 0;
  struct cleanup *cleanup_chain;

  cleanup_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "thread-ids");

  if (!target_has_stack)
    error ("No stack.");

  prune_threads ();
  target_find_new_threads ();

  for (tp = thread_list; tp; tp = tp->next)
    {
      num++;
      ui_out_field_int (uiout, "thread-id", tp->num);
    }

  do_cleanups (cleanup_chain);
  ui_out_field_int (uiout, "number-of-threads", num);

  cleanup_chain = make_cleanup_ui_out_list_begin_end (uiout, "threads");
  num = 0;

  for (tp = thread_list; tp; tp = tp->next)
    {
      struct cleanup *a_thread_cleanup;
      a_thread_cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, "thread");
      num++;
      ui_out_field_int (uiout, "thread-id", tp->num);
#ifdef NM_NEXTSTEP
      macosx_print_thread_details (uiout, tp->ptid);
#endif
      do_cleanups (a_thread_cleanup);
    }

  do_cleanups (cleanup_chain);

  return GDB_RC_OK;
}

/* Official gdblib interface function to get a list of thread ids and
   the total number. */
enum gdb_rc
gdb_list_thread_ids (struct ui_out *uiout, char **error_message)
{
  return catch_exceptions_with_msg (uiout, do_captured_list_thread_ids, NULL,
				    error_message, RETURN_MASK_ALL);
}

/* Load infrun state for the thread PID.  */

void
load_infrun_state (ptid_t ptid,
		   CORE_ADDR *prev_pc,
		   int *trap_expected,
		   struct breakpoint **step_resume_breakpoint,
		   CORE_ADDR *step_range_start,
		   CORE_ADDR *step_range_end,
		   /* APPLE LOCAL step ranges  */
		   struct address_range_list **stepping_ranges,
		   struct frame_id *step_frame_id,
		   int *handling_longjmp,
		   int *another_trap,
		   int *stepping_through_solib_after_catch,
		   bpstat *stepping_through_solib_catchpoints,
		   int *current_line,
		   struct symtab **current_symtab)
{
  struct thread_info *tp;

  /* If we can't find the thread, then we're debugging a single threaded
     process.  No need to do anything in that case.  */
  tp = find_thread_id (pid_to_thread_id (ptid));
  if (tp == NULL)
    return;

  *prev_pc = tp->prev_pc;
  *trap_expected = tp->trap_expected;
  *step_resume_breakpoint = tp->step_resume_breakpoint;
  *step_range_start = tp->step_range_start;
  *step_range_end = tp->step_range_end;
  /* APPLE LOCAL step ranges  */
  *stepping_ranges = tp->stepping_ranges;
  *step_frame_id = tp->step_frame_id;
  *handling_longjmp = tp->handling_longjmp;
  *another_trap = tp->another_trap;
  *stepping_through_solib_after_catch =
    tp->stepping_through_solib_after_catch;
  *stepping_through_solib_catchpoints =
    tp->stepping_through_solib_catchpoints;
  *current_line = tp->current_line;
  *current_symtab = tp->current_symtab;

  restore_thread_inlined_call_stack (ptid);
}

/* Save infrun state for the thread PID.  */

void
save_infrun_state (ptid_t ptid,
		   CORE_ADDR prev_pc,
		   int trap_expected,
		   struct breakpoint *step_resume_breakpoint,
		   CORE_ADDR step_range_start,
		   CORE_ADDR step_range_end,
		   /* APPLE LOCAL step ranges  */
		   struct address_range_list *stepping_ranges,
		   const struct frame_id *step_frame_id,
		   int handling_longjmp,
		   int another_trap,
		   int stepping_through_solib_after_catch,
		   bpstat stepping_through_solib_catchpoints,
		   int current_line,
		   struct symtab *current_symtab)
{
  struct thread_info *tp;

  /* If we can't find the thread, then we're debugging a single-threaded
     process.  Nothing to do in that case.  */
  tp = find_thread_id (pid_to_thread_id (ptid));
  if (tp == NULL)
    return;

  tp->prev_pc = prev_pc;
  tp->trap_expected = trap_expected;
  tp->step_resume_breakpoint = step_resume_breakpoint;
  tp->step_range_start = step_range_start;
  tp->step_range_end = step_range_end;
  /* APPLE LOCAL step ranges  */
  tp->stepping_ranges = stepping_ranges;
  tp->step_frame_id = (*step_frame_id);
  tp->handling_longjmp = handling_longjmp;
  tp->another_trap = another_trap;
  tp->stepping_through_solib_after_catch = stepping_through_solib_after_catch;
  tp->stepping_through_solib_catchpoints = stepping_through_solib_catchpoints;
  tp->current_line = current_line;
  tp->current_symtab = current_symtab;

  save_thread_inlined_call_stack (ptid);
}

/* Return true if TP is an active thread. */
static int
thread_alive (struct thread_info *tp)
{
  if (PIDGET (tp->ptid) == -1)
    return 0;
  if (!target_thread_alive (tp->ptid))
    {
      tp->ptid = pid_to_ptid (-1);	/* Mark it as dead */
      return 0;
    }
  return 1;
}

void
prune_threads (void)
{
  struct thread_info *tp, *next;

  for (tp = thread_list; tp; tp = next)
    {
      next = tp->next;
      if (!thread_alive (tp))
	delete_thread (tp->ptid);
    }
}

/* Print information about currently known threads 

 * Note: this has the drawback that it _really_ switches
 *       threads, which frees the frame cache.  A no-side
 *       effects info-threads command would be nicer.
 */

static void
info_threads_command (char *arg, int from_tty)
{
  struct thread_info *tp;
  ptid_t current_ptid;
  struct frame_info *cur_frame;
  struct frame_id saved_frame_id = get_frame_id (get_selected_frame (NULL));
  char *extra_info;
  int longest_threadname = 0;
  int longest_portnum = 0;
  int threadcount = 0;

  prune_threads ();
  target_find_new_threads ();
  current_ptid = inferior_ptid;

  /* APPLE LOCAL: Get the name strings for all the threads, find the longest
     one.  */
  for (tp = thread_list; tp; tp = tp->next)
    {
      char *t = target_get_thread_name (tp->ptid);
      if (t)
        {
          if (strlen (t) > longest_threadname)
            longest_threadname = strlen (t);
        }
      t = target_get_thread_id_str (tp->ptid);
      if (t)
        {
          if (strlen (t) > longest_portnum)
            longest_portnum = strlen (t);
        }
      threadcount++;
    }

  /* Limit the thread name to 25 chars max for reasonable one-line printing. */
  if (longest_threadname > 25)
    longest_threadname = 25;

  for (tp = thread_list; tp; tp = tp->next)
    {
      if (ptid_equal (tp->ptid, current_ptid))
	ui_out_text (uiout, "* ");
      else
	ui_out_text (uiout, "  ");

      if (threadcount > 100 && tp->num < 100)
        ui_out_text (uiout, " ");
      if (threadcount > 10 && tp->num < 10)
        ui_out_text (uiout, " ");
        
      ui_out_field_int (uiout, "threadno", tp->num);
      ui_out_text (uiout, " ");

      char *tidstr = target_get_thread_id_str (tp->ptid);
      if (tidstr)
        {
          ui_out_field_string (uiout, "target_tid", tidstr);
          int spacer = strlen (tidstr);
          while (spacer < longest_portnum)
            {
              ui_out_text (uiout, " ");
              spacer++;
            }
          ui_out_text (uiout, " ");
        }

      /* APPLE LOCAL: Get the thread name string, put quote marks around it,
         truncate or pad it out to LONGEST_THREADNAME characters, print it.  */
      char *s = target_get_thread_name (tp->ptid);
      if (s && s[0] != '\0')
        {
          char buf1[128];
          char buf2[128];
          if (strlen (s) > longest_threadname)
              s[longest_threadname] = '\0';
          strlcpy (buf1, "\"", sizeof (buf1));
          strlcat (buf1, s, sizeof (buf1));
          strlcat (buf1, "\"", sizeof (buf1));
          snprintf (buf2, sizeof (buf2) - 1,"%-*s", 
                    longest_threadname + 2, buf1);
          buf2[sizeof (buf2) - 1] = '\0';
          ui_out_field_string (uiout, "target_thread_name", buf2);
          ui_out_text (uiout, " ");
       }
      else
       {
          /* There is at least one thread with a name; this thread has
             no name so print padding chars.  */
          if (longest_threadname > 0)
            {
              ui_out_text_fmt (uiout, "%-*s", longest_threadname + 2, "");
              ui_out_text (uiout, " ");
            }
       }

      extra_info = target_extra_thread_info (tp);
      if (extra_info)
	{
	  ui_out_text (uiout, " (");
	  ui_out_field_string (uiout, "extra_info", extra_info);
	  ui_out_text (uiout, ")");
          ui_out_text (uiout, " ");
	}

      switch_to_thread (tp->ptid);
      print_stack_frame (get_selected_frame (NULL), 0, LOCATION);
      /* APPLE LOCAL begin subroutine inlining  */
      clear_inlined_subroutine_print_frames ();
      /* APPLE LOCAL end subroutine inlining  */
    }

  switch_to_thread (current_ptid);

  /* Restores the frame set by the user before the "info threads"
     command.  We have finished the info-threads display by switching
     back to the current thread.  That switch has put us at the top of
     the stack (leaf frame).  */
  cur_frame = frame_find_by_id (saved_frame_id);
  if (cur_frame == NULL)
    {
      /* Ooops, can't restore, tell user where we are.  */
      warning (_("Couldn't restore frame in current thread, at frame 0"));
      print_stack_frame (get_selected_frame (NULL), 0, LOCATION);
      /* APPLE LOCAL begin subroutine inlining  */
      clear_inlined_subroutine_print_frames ();
      /* APPLE LOCAL end subroutine inlining  */
    }
  else
    {
      select_frame (cur_frame);
      /* re-show current frame. */
      show_stack_frame (cur_frame);
    }
}

/* Switch from one thread to another. */

/* APPLE LOCAL: This was originally static, but we needed to export it
   for use by do_check_is_thread_unsafe.  gdb_thread_select has the
   unfortunate side-effect of using the parser, which is silly for a
   library function.  Ideally there would be an exported function that
   doesn't use the parser, but does call all the appropriate hooks.
   But that's beyond the scope of Turmeric, and in the case of
   do_check_is_thread_unsafe it's not necessary since we are just going
   to restore the original thread state anyway. */

void
switch_to_thread (ptid_t ptid)
{
  if (ptid_equal (ptid, inferior_ptid))
    return;

  /* APPLE LOCAL: If we switch threads, then we need to turn off
     the cleanups for calling functions on THIS thread.  */

  do_hand_call_cleanups (ALL_CLEANUPS);

  save_thread_inlined_call_stack (inferior_ptid);
  inferior_ptid = ptid;
  flush_cached_frames ();
  registers_changed ();
  stop_pc = read_pc ();
  restore_thread_inlined_call_stack (inferior_ptid);
  /* APPLE LOCAL begin subroutine inlining  */
  /* If the PC has changed since the last time we updated the
     global_inlined_call_stack data, we need to verify the current
     data and possibly update it.  */
  if (stop_pc != inlined_function_call_stack_pc ())
    inlined_function_update_call_stack (stop_pc);
  /* APPLE LOCAL end subroutine inlining  */

  select_frame (get_current_frame ());

  /* APPPLE LOCAL Finally, if the scheduler-locking is on, then we should 
     reset the thread we are trying to run. */
  if (scheduler_lock_on_p ())
    scheduler_run_this_ptid (inferior_ptid);

}

static void
restore_current_thread (ptid_t ptid, int print)
{
  if (!ptid_equal (ptid, inferior_ptid))
    {
      switch_to_thread (ptid);
      if (print)
	/* APPLE LOCAL begin subroutine inlining  */
	{
	  print_stack_frame (get_current_frame (), 0, -1);
	  clear_inlined_subroutine_print_frames ();
	}
        /* APPLE LOCAL end subroutine inlining  */
    }
}

struct current_thread_cleanup
{
  ptid_t inferior_ptid;
  int print;
};

static void
do_restore_current_thread_cleanup (void *arg)
{
  struct current_thread_cleanup *old = arg;
  restore_current_thread (old->inferior_ptid, old->print);
  xfree (old);
}

struct cleanup *
make_cleanup_restore_current_thread (ptid_t inferior_ptid, int print)
{
  struct current_thread_cleanup *old
    = xmalloc (sizeof (struct current_thread_cleanup));
  old->inferior_ptid = inferior_ptid;
  old->print = print;

  return make_cleanup (do_restore_current_thread_cleanup, old);
}

/* Apply a GDB command to a list of threads.  List syntax is a whitespace
   seperated list of numbers, or ranges, or the keyword `all'.  Ranges consist
   of two numbers seperated by a hyphen.  Examples:

   thread apply 1 2 7 4 backtrace       Apply backtrace cmd to threads 1,2,7,4
   thread apply 2-7 9 p foo(1)  Apply p foo(1) cmd to threads 2->7 & 9
   thread apply all p x/i $pc   Apply x/i $pc cmd to all threads
 */

static void
thread_apply_all_command (char *cmd, int from_tty)
{
  struct thread_info *tp;
  struct cleanup *old_chain;
  struct cleanup *saved_cmd_cleanup_chain;
  char *saved_cmd;

  if (cmd == NULL || *cmd == '\000')
    error (_("Please specify a command following the thread ID list"));

  old_chain = make_cleanup_restore_current_thread (inferior_ptid, 0);

  /* It is safe to update the thread list now, before
     traversing it for "thread apply all".  MVS */
  target_find_new_threads ();

  /* Save a copy of the command in case it is clobbered by
     execute_command */
  saved_cmd = xstrdup (cmd);
  saved_cmd_cleanup_chain = make_cleanup (xfree, (void *) saved_cmd);
  for (tp = thread_list; tp; tp = tp->next)
    if (thread_alive (tp))
      {
	switch_to_thread (tp->ptid);
	printf_filtered (_("\nThread %d (%s):\n"),
			 tp->num, target_tid_to_str (inferior_ptid));

	/* APPLE LOCAL: Use safe_execute_command for this.  If the command 
	   has an error for one thread, that's no reason not to run it for
	   the next thread.  */

	safe_execute_command (uiout, cmd, from_tty);
	strcpy (cmd, saved_cmd);	/* Restore exact command used previously */
      }

  do_cleanups (saved_cmd_cleanup_chain);
  do_cleanups (old_chain);
}

static void
thread_apply_command (char *tidlist, int from_tty)
{
  char *cmd;
  char *p;
  struct cleanup *old_chain;
  struct cleanup *saved_cmd_cleanup_chain;
  char *saved_cmd;

  if (tidlist == NULL || *tidlist == '\000')
    error (_("Please specify a thread ID list"));

  for (cmd = tidlist; *cmd != '\000' && !isalpha (*cmd); cmd++);

  if (*cmd == '\000')
    error (_("Please specify a command following the thread ID list"));

  old_chain = make_cleanup_restore_current_thread (inferior_ptid, 0);

  /* Save a copy of the command in case it is clobbered by
     execute_command */
  saved_cmd = xstrdup (cmd);
  saved_cmd_cleanup_chain = make_cleanup (xfree, (void *) saved_cmd);
  while (tidlist < cmd)
    {
      struct thread_info *tp;
      int start, end;

      start = strtol (tidlist, &p, 10);
      if (p == tidlist)
	error (_("Error parsing %s"), tidlist);
      tidlist = p;

      while (*tidlist == ' ' || *tidlist == '\t')
	tidlist++;

      if (*tidlist == '-')	/* Got a range of IDs? */
	{
	  tidlist++;		/* Skip the - */
	  end = strtol (tidlist, &p, 10);
	  if (p == tidlist)
	    error (_("Error parsing %s"), tidlist);
	  tidlist = p;

	  while (*tidlist == ' ' || *tidlist == '\t')
	    tidlist++;
	}
      else
	end = start;

      for (; start <= end; start++)
	{
	  tp = find_thread_id (start);

	  if (!tp)
	    warning (_("Unknown thread %d."), start);
	  else if (!thread_alive (tp))
	    warning (_("Thread %d has terminated."), start);
	  else
	    {
	      switch_to_thread (tp->ptid);
	      printf_filtered (_("\nThread %d (%s):\n"), tp->num,
			       target_tid_to_str (inferior_ptid));
	      execute_command (cmd, from_tty);
	      strcpy (cmd, saved_cmd);	/* Restore exact command used previously */
	    }
	}
    }

  do_cleanups (saved_cmd_cleanup_chain);
  do_cleanups (old_chain);
}

struct select_thread_args
{
  char *tidstr;
  int print;
};

/* Switch to the specified thread.  Will dispatch off to thread_apply_command
   if prefix of arg is `apply'.  */

static void
thread_command (char *tidstr, int from_tty)
{
  if (!tidstr)
    {
      /* Don't generate an error, just say which thread is current. */
      if (target_has_stack)
	printf_filtered (_("[Current thread is %d (%s)]\n"),
			 pid_to_thread_id (inferior_ptid),
			 target_tid_to_str (inferior_ptid));
      else
	error (_("No stack."));
      return;
    }

  gdb_thread_select (uiout, tidstr, 1, NULL);
}

extern int scheduler_lock_on ();
extern struct ptid scheduler_lock_ptid;

static int
do_captured_thread_select (struct ui_out *uiout,
			   void *in_args)
{
  int num;
  struct thread_info *tp;
  struct select_thread_args *args = (struct select_thread_args *) in_args;

  num = value_as_long (parse_and_eval (args->tidstr));

  tp = find_thread_id (num);

  if (!tp)
    error (_("Thread ID %d not known."), num);

  if (!thread_alive (tp))
    error (_("Thread ID %d has terminated."), num);

  switch_to_thread (tp->ptid);

  if (args->print)
    {

      ui_out_text (uiout, "[Switching to thread ");
      ui_out_field_int (uiout, "new-thread-id", pid_to_thread_id (inferior_ptid));
      ui_out_text (uiout, " (");
#if defined(HPUXHPPA)
      ui_out_text (uiout, target_tid_to_str (inferior_ptid));
#else
      ui_out_text (uiout, target_pid_to_str (inferior_ptid));
#endif
      ui_out_text (uiout, ")");
      char *s = target_get_thread_name (inferior_ptid);
      if (s && s[0] != '\0')
        {
          ui_out_text (uiout, ", \"");
          ui_out_field_string (uiout, "thread-name", s);
          ui_out_text (uiout, "\"");
        }
      ui_out_text (uiout, "]\n");

      /* APPLE LOCAL begin subroutine inlining  */
      /* If we're inside an inlined function, we may have gotten there
	 via a 'step' from the call site, which automatically flushes
	 the frames, so there may not be a current frame.  In that case
	 this is not an error.  */
      if (deprecated_selected_frame == NULL)
	{
	  if (get_frame_type (get_current_frame ()) != INLINED_FRAME)
	    error (_("No selected frame."));
	  else
	    select_frame (get_current_frame ());
	}
      
      print_stack_frame (deprecated_selected_frame,
			 frame_relative_level (deprecated_selected_frame), 1);
      clear_inlined_subroutine_print_frames ();
      /* APPLE LOCAL end subroutine inlining  */

    }
  /* Remember to run the context hook here - since this changes
     thread context */
  
  if (deprecated_context_hook)
    deprecated_context_hook (pid_to_thread_id (inferior_ptid));
  
  return GDB_RC_OK;
}

enum gdb_rc
gdb_thread_select (struct ui_out *uiout, char *tidstr, int print,
		   char **error_message)
{
  struct select_thread_args args;

  args.tidstr = tidstr;
  args.print = print;

  return catch_exceptions_with_msg (uiout, do_captured_thread_select, &args,
				    error_message, RETURN_MASK_ALL);
}

/* Commands with a prefix of `thread'.  */
struct cmd_list_element *thread_cmd_list = NULL;

void
_initialize_thread (void)
{
  static struct cmd_list_element *thread_apply_list = NULL;

  add_info ("threads", info_threads_command,
	    _("IDs of currently known threads."));

  add_prefix_cmd ("thread", class_run, thread_command, _("\
Use this command to switch between threads.\n\
The new thread ID must be currently known."),
		  &thread_cmd_list, "thread ", 1, &cmdlist);

  add_prefix_cmd ("apply", class_run, thread_apply_command,
		  _("Apply a command to a list of threads."),
		  &thread_apply_list, "apply ", 1, &thread_cmd_list);

  add_cmd ("all", class_run, thread_apply_all_command,
	   _("Apply a command to all threads."), &thread_apply_list);

  if (!xdb_commands)
    add_com_alias ("t", "thread", class_run, 1);
}
