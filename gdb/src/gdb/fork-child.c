/* Fork a Unix child process, and set up to debug it, for GDB.

   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999,
   2000, 2001, 2004 Free Software Foundation, Inc.

   Contributed by Cygnus Support.

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
#include "gdb_string.h"
#include "frame.h"		/* required by inferior.h */
#include "inferior.h"
#include "target.h"
#include "gdb_wait.h"
#include "gdb_vfork.h"
#include "gdbcore.h"
#include "terminal.h"
#include "gdbthread.h"
#include "gdbcmd.h"
#include "command.h" /* for dont_repeat () */
#include "solib.h"
#include "osabi.h"
#include <signal.h>
#ifdef USE_POSIX_SPAWN
#include <spawn.h>
#endif

char *exec_argv0 = NULL;
char *exec_pathname = NULL;

/* This just gets used as a default if we can't find SHELL.  */
#ifndef SHELL_FILE
#define SHELL_FILE "/bin/sh"
#endif

extern char **environ;

/* APPLE LOCAL: I moved breakup_args from here to utils.c.  */

/* When executing a command under the given shell, return non-zero if
   the '!' character should be escaped when embedded in a quoted
   command-line argument.  */

static int
escape_bang_in_quoted_argument (const char *shell_file)
{
  const int shell_file_len = strlen (shell_file);

  /* Bang should be escaped only in C Shells.  For now, simply check
     that the shell name ends with 'csh', which covers at least csh
     and tcsh.  This should be good enough for now.  */

  if (shell_file_len < 3)
    return 0;

  if (shell_file[shell_file_len - 3] == 'c'
      && shell_file[shell_file_len - 2] == 's'
      && shell_file[shell_file_len - 1] == 'h')
    return 1;

  return 0;
}

/* Start an inferior Unix child process and sets inferior_ptid to its
   pid.  EXEC_FILE is the file to run.  ALLARGS is a string containing
   the arguments to the program.  ENV is the environment vector to
   pass.  SHELL_FILE is the shell file, or NULL if we should pick
   one.  */

/* This function is NOT reentrant.  Some of the variables have been
   made static to ensure that they survive the vfork call.  */

void
fork_inferior (char *exec_file_arg, char *allargs, char **env,
	       void (*traceme_fun) (void), void (*init_trace_fun) (int),
	       void (*pre_trace_fun) (void), char *shell_file_arg)
{
  int pid;
  char *shell_command;
  static char default_shell_file[] = SHELL_FILE;
  int len;
  /* Set debug_fork then attach to the child while it sleeps, to debug. */
  static int debug_fork = 0;
  /* This is set to the result of setpgrp, which if vforked, will be visible
     to you in the parent process.  It's only used by humans for debugging.  */
  static int debug_setpgrp = 657473;
  static char *shell_file;
  static char *exec_file;
  char **save_our_env;
  int shell = 0;
  static char **argv;
  const char *inferior_io_terminal = get_inferior_io_terminal ();

  /* If no exec file handed to us, get it from the exec-file command
     -- with a good, common error message if none is specified.  */
  exec_file = exec_file_arg;
  if (exec_file == 0)
    exec_file = get_exec_file (1);

  /* If 0, we'll just do a fork/exec, no shell, so don't
   * bother figuring out what shell.
   */
  shell_file = shell_file_arg;
  if (start_with_shell_flag)
    {
      /* Figure out what shell to start up the user program under.  */
      if (shell_file == NULL)
	shell_file = getenv ("SHELL");
      if (shell_file == NULL)
	shell_file = default_shell_file;
      shell = 1;
    }

  /* Multiplying the length of exec_file by 4 is to account for the
     fact that it may expand when quoted; it is a worst-case number
     based on every character being '.  */
  /* APPLE LOCAL 5 = "exec ", but we have "exec /usr/bin/arch -arch x86_64 " 
     at most.  so 5->33.  */
#ifdef USE_ARCH_FOR_EXEC
  len = 33;
#else
  len = 5;
#endif
  len += 4 * strlen (exec_file) + 1 + strlen (allargs) + 1 + /*slop */ 12;
  /* If desired, concat something onto the front of ALLARGS.
     SHELL_COMMAND is the result.  */
#ifdef SHELL_COMMAND_CONCAT
  shell_command = (char *) alloca (strlen (SHELL_COMMAND_CONCAT) + len);
  strcpy (shell_command, SHELL_COMMAND_CONCAT);
#else
  shell_command = (char *) alloca (len);
  shell_command[0] = '\0';
#endif

  if (!shell)
    {
      unsigned int i;
      char **argt = NULL;

      argt = buildargv (allargs);
      if (argt == NULL)
	{
	  error ("unable to build argument vector for inferior process (out of memory)");
	}
      if ((allargs == NULL) || (allargs[0] == '\0'))
	argt[0] = NULL;
      for (i = 0; argt[i] != NULL; i++);
      argv = (char **) xmalloc ((i + 1 + 1) * (sizeof *argt));
      argv[0] = exec_file;
      if (exec_argv0[0] != '\0')
	argv[0] = exec_argv0;
      memcpy (&argv[1], argt, (i + 1) * sizeof (*argt));
      /* freeargv (argt); */
    }
  else
    {
      /* We're going to call a shell */

      /* Now add exec_file, quoting as necessary.  */

      char *p;
      int need_to_quote;
      const int escape_bang = escape_bang_in_quoted_argument (shell_file);

      /* APPLE LOCAL: Since the app we are exec'ing might be Universal, we need
	 a way to specify the architecture we want to launch.  We use the
	 "arch" command-line utility for that.  */
#ifdef USE_ARCH_FOR_EXEC
      {
	char *arch_string = NULL;
	const char *osabi_name = gdbarch_osabi_name (gdbarch_osabi (current_gdbarch));
#if defined (TARGET_POWERPC)
	if (strcmp (osabi_name, "Darwin") == 0)
	  arch_string = "ppc";
	else if (strcmp (osabi_name, "Darwin64") == 0)
	  arch_string = "ppc64";
#elif defined (TARGET_I386)
	if (strcmp (osabi_name, "Darwin") == 0)
	  arch_string = "i386";
	else if (strcmp (osabi_name, "Darwin64") == 0)
	  arch_string = "x86_64";
#elif defined (TARGET_ARM)
	if (strcmp (osabi_name, "Darwin") == 0)
	  arch_string = "arm";
	else if (strcmp (osabi_name, "DarwinV6") == 0)
	  arch_string = "armv6";
#endif
	if (arch_string != NULL)
	  sprintf (shell_command, "%s exec /usr/bin/arch -arch %s ", shell_command, arch_string);
	else
	  strcat (shell_command, "exec ");
      }
#else
      strcat (shell_command, "exec ");
#endif

      /* Quoting in this style is said to work with all shells.  But
         csh on IRIX 4.0.1 can't deal with it.  So we only quote it if
         we need to.  */
      p = exec_file;
      while (1)
	{
	  switch (*p)
	    {
	    case '\'':
	    case '!':
	    case '"':
	    case '(':
	    case ')':
	    case '$':
	    case '&':
	    case ';':
	    case '<':
	    case '>':
	    case ' ':
	    case '\n':
	    case '\t':
	      need_to_quote = 1;
	      goto end_scan;

	    case '\0':
	      need_to_quote = 0;
	      goto end_scan;

	    default:
	      break;
	    }
	  ++p;
	}
    end_scan:
      if (need_to_quote)
	{
	  strcat (shell_command, "'");
	  for (p = exec_file; *p != '\0'; ++p)
	    {
	      if (*p == '\'')
		strcat (shell_command, "'\\''");
	      else if (*p == '!' && escape_bang)
		strcat (shell_command, "\\!");
	      else
		strncat (shell_command, p, 1);
	    }
	  strcat (shell_command, "'");
	}
      else
	strcat (shell_command, exec_file);

      strcat (shell_command, " ");
      strcat (shell_command, allargs);

      /* APPLE LOCAL: There's no reason to use execlp for the shell
	 case, and execve for the non-shell case.  Lets just build up
	 an appropriate argv array and use it for both.  */
      
      argv = (char **) xmalloc (4 * sizeof (char *));
      argv[0] = shell_file;
      argv[1] = "-c";
      argv[2] = shell_command;
      argv[3] = NULL;
    }

  /* On some systems an exec will fail if the executable is open.  */
  close_exec_file ();

  /* Retain a copy of our environment variables, since the child will
     replace the value of environ and if we're vforked, we have to
     restore it.  */
  save_our_env = environ;

  /* Tell the terminal handling subsystem what tty we plan to run on;
     it will just record the information for later.  */
  new_tty_prefork (inferior_io_terminal);

  /* It is generally good practice to flush any possible pending stdio
     output prior to doing a fork, to avoid the possibility of both
     the parent and child flushing the same data after the fork. */
  gdb_flush (gdb_stdout);
  gdb_flush (gdb_stderr);

  /* If there's any initialization of the target layers that must
     happen to prepare to handle the child we're about fork, do it
     now...  */
  if (pre_trace_fun != NULL)
    (*pre_trace_fun) ();

  /* Create the child process.  Since the child process is going to
     exec(3) shortlty afterwards, try to reduce the overhead by
     calling vfork(2).  However, if PRE_TRACE_FUN is non-null, it's
     likely that this optimization won't work since there's too much
     work to do between the vfork(2) and the exec(3).  This is known
     to be the case on ttrace(2)-based HP-UX, where some handshaking
     between parent and child needs to happen between fork(2) and
     exec(2).  However, since the parent is suspended in the vforked
     state, this doesn't work.  Also note that the vfork(2) call might
     actually be a call to fork(2) due to the fact that autoconf will
     ``#define vfork fork'' on certain platforms.  */

  /* APPLE LOCAL: I have to do some synchronization between the forked side
     and the parent side, which doesn't work with vfork.  So I #if 0'ed out
     all the vfork stuff.  */

  pid = fork ();
#if 0
  if (pre_trace_fun || debug_fork)
    pid = fork ();
  else
    pid = vfork ();
#endif

  if (pid < 0)
    perror_with_name (("vfork"));

  if (pid == 0)
    {
      if (debug_fork)
	sleep (debug_fork);

      /* Run inferior in a separate process group.  */
      debug_setpgrp = gdb_setpgid ();
      if (debug_setpgrp == -1)
	perror ("setpgrp failed in child");

      /* Ask the tty subsystem to switch to the one we specified
         earlier (or to share the current terminal, if none was
         specified).  */
      new_tty ();

      /* Changing the signal handlers for the inferior after
         a vfork can also change them for the superior, so we don't mess
         with signals here.  See comments in
         initialize_signals for how we get the right signal handlers
         for the inferior.  */

      /* "Trace me, Dr. Memory!" */
      (*traceme_fun) ();

      /* The call above set this process (the "child") as debuggable
        by the original gdb process (the "parent").  Since processes
        (unlike people) can have only one parent, if you are debugging
        gdb itself (and your debugger is thus _already_ the
        controller/parent for this child), code from here on out is
        undebuggable.  Indeed, you probably got an error message
        saying "not parent".  Sorry; you'll have to use print
        statements!  */

      /* There is no execlpe call, so we have to set the environment
         for our child in the global variable.  If we've vforked, this
         clobbers the parent, but environ is restored a few lines down
         in the parent.  By the way, yes we do need to look down the
         path to find $SHELL.  Rich Pixley says so, and I agree.  */
      environ = env;

      /* APPLE LOCAL: gdb is setgid to give it extra special debuggizer
         powers; we need to drop those privileges before executing the
         inferior process.  */
      setgid (getgid ());

      /* If we decided above to start up with a shell, we exec the
        shell, "-c" says to interpret the next arg as a shell command
        to execute, and this command is "exec <target-program>
        <args>".  "-f" means "fast startup" to the c-shell, which
        means don't do .cshrc file. Doing .cshrc may cause fork/exec
        events which will confuse debugger start-up code.  */

      /* APPLE LOCAL: I took out the code that was calling execlp,
	 since there's no reason not to just use execvp.  That makes
	 the code simpler since the shell ALSO might be universal, so
	 we need to go through posix_spawn or we're going to have to
	 deal with switching the architecture mid-startup.  */
	{
	  /* Otherwise, we directly exec the target program with
	     execvp.  */
	  int i;
	  char *errstring;
	  char *fileptr;

	  if (shell)
	    fileptr = shell_file;
	  else
	    {
	      if (exec_pathname[0] != '\0')
		fileptr = exec_pathname;
	      else
		fileptr = exec_file;
	    }

#ifdef USE_POSIX_SPAWN
	  {
	    posix_spawnattr_t attr;
	    int retval;
	    size_t copied;
	    cpu_type_t cpu = 0;
	    int count = 0;
	    pid_t pid;
	    const char *osabi_name = gdbarch_osabi_name (gdbarch_osabi (current_gdbarch));

#if defined (TARGET_POWERPC)
	    if (strcmp (osabi_name, "Darwin") == 0)
	      {
		cpu = CPU_TYPE_POWERPC;
		count = 1;
	      }
	    else if (strcmp (osabi_name, "Darwin64") == 0)
	      {
		cpu = CPU_TYPE_POWERPC64;
		count = 1;
	      }
#elif defined (TARGET_I386)
	    if (strcmp (osabi_name, "Darwin") == 0)
	      {
		cpu = CPU_TYPE_I386;
		count = 1;
	      }
	    else if (strcmp (osabi_name, "Darwin64") == 0)
	      {
		cpu = CPU_TYPE_X86_64;
		count = 1;
	      }
#elif define (TARGET_ARM)
	    if (strcmp (osabi_name, "Darwin") == 0)
	      {
		cpu = CPU_TYPE_ARM;
		count = 1;
	      }
	    else if (strcmp (osabi_name, "DarwinV6") == 0)
	      {
		cpu = CPU_TYPE_ARM;
		count = 1;
	      }
#endif
	    retval = posix_spawnattr_init (&attr);
	    if (retval != 0)
	      {
		warning ("Couldn't initialize attributes for posix_spawn, error: %d", retval);
		goto try_execvp;
	      }
	    retval = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC);
	    if (retval != 0)
	      {
		warning ("Couldn't add POSIX_SPAWN_SETEXEC to attributes, error: %d", retval);
		goto try_execvp;
	      }
	    if (count == 1)
	      {
		retval = posix_spawnattr_setbinpref_np(&attr, 1, &cpu, &copied);
		if (retval != 0 || copied != 1)
		  {
		    warning ("Couldn't set the binary preferences, error: %d", retval);
		    goto try_execvp;
		  }
	      }
	    retval = posix_spawnattr_setpgroup (&attr, debug_setpgrp);
	    if (retval != 0 || copied != 1)
	      {
		warning ("Couldn't set the process group, error: %d", retval);
		goto try_execvp;
	      }
	    retval = posix_spawnp (&pid, fileptr, NULL,  &attr, argv, env);
	    warning ("posix_spawn failed, trying execvp, error: %d", retval);
	  }
#endif
	try_execvp:
	  execvp (fileptr, argv);

	  /* If we get here, it's an error.  */
	  errstring = safe_strerror (errno);
	  fprintf_unfiltered (gdb_stderr, "Cannot exec %s ", fileptr);

	  i = 1;
	  while (argv[i] != NULL)
	    {
	      if (i != 1)
		fprintf_unfiltered (gdb_stderr, " ");
	      fprintf_unfiltered (gdb_stderr, "%s", argv[i]);
	      i++;
	    }
	  fprintf_unfiltered (gdb_stderr, ".\n");
#if 0
	  /* This extra info seems to be useless.  */
	  fprintf_unfiltered (gdb_stderr, "Got error %s.\n", errstring);
#endif
	  gdb_flush (gdb_stderr);
	  _exit (0177);
	}
    }

  /* Restore our environment in case a vforked child clob'd it.  */
  environ = save_our_env;

  init_thread_list ();

  /* Needed for wait_for_inferior stuff below.  */
  inferior_ptid = pid_to_ptid (pid);

  /* Now that we have a child process, make it our target, and
     initialize anything target-vector-specific that needs
     initializing.  */
  (*init_trace_fun) (pid);

  /* We are now in the child process of interest, having exec'd the
     correct program, and are poised at the first instruction of the
     new program.  */

  /* Allow target dependent code to play with the new process.  This
     might be used to have target-specific code initialize a variable
     in the new process prior to executing the first instruction.  */
  TARGET_CREATE_INFERIOR_HOOK (pid);

#ifdef SOLIB_CREATE_INFERIOR_HOOK
  SOLIB_CREATE_INFERIOR_HOOK (pid);
#else
  solib_create_inferior_hook ();
#endif
}

/* Accept NTRAPS traps from the inferior.  */

void
startup_inferior (int ntraps)
{
  int pending_execs = ntraps;
  int terminal_initted = 0;

  /* The process was started by the fork that created it, but it will
     have stopped one instruction after execing the shell.  Here we
     must get it up to actual execution of the real program.  */

  clear_proceed_status ();

  init_wait_for_inferior ();

  inferior_ignoring_startup_exec_events = pending_execs;
  inferior_ignoring_leading_exec_events =
    target_reported_exec_events_per_exec_call () - 1;

  while (1)
    {
      /* Make wait_for_inferior be quiet. */
      stop_soon = STOP_QUIETLY;
      wait_for_inferior ();
      if (stop_signal != TARGET_SIGNAL_TRAP)
	{
        /* APPLE LOCAL: If we are getting a bad access or bad
           instruction here, don't just send it back to the app,
           since then we'll just loop forever here.  This can
           happen when you have "start-with-shell" non-zero, and
           the shell crashes.  */
#ifdef NM_NEXTSTEP
        if (stop_signal == TARGET_EXC_BAD_ACCESS 
          || stop_signal == TARGET_EXC_BAD_INSTRUCTION)
          {
            warning ("The target crashed on startup, maybe the shell is crashing.\n"
                    "\"Try set start-with-shell 0\" to workaround.");
	    /* For now we don't seem to be able to unwind successfully
		from this error, so I am just aborting.  Throwing
		an error or just breaking both lead to gdb just
		hanging, which isn't helpful...  */
            abort ();
          }
#endif
        /* END APPLE LOCAL */

	  /* Let shell child handle its own signals in its own way.
	     FIXME: what if child has exited?  Must exit loop
	     somehow.  */
	  if (PIDGET (inferior_ptid) == 0)
	    break;
	  resume (0, stop_signal);
	}
      else
	{
	  /* We handle SIGTRAP, however; it means child did an exec.  */
	  if (!terminal_initted)
	    {
	      /* Now that the child has exec'd we know it has already
	         set its process group.  On POSIX systems, tcsetpgrp
	         will fail with EPERM if we try it before the child's
	         setpgid.  */

	      /* Set up the "saved terminal modes" of the inferior
	         based on what modes we are starting it with.  */
	      target_terminal_init ();

	      /* Install inferior's terminal modes.  */
	      target_terminal_inferior ();

	      terminal_initted = 1;
	    }

	  if (--pending_execs == 0)
	    break;

	  /* APPLE LOCAL begin radar 5188345  */
	  /* If inferior has exited, break out of loop  instead of
	     attempting to resume it.  */
	  if (PIDGET (inferior_ptid) == 0)
	    break;
	  /* APPLE LOCAL end radar 5188345  */

	  resume (0, TARGET_SIGNAL_0);	/* Just make it go on.  */
	}
    }
  stop_soon = NO_STOP_QUIETLY;
}

/* APPLE LOCAL begin start with shell */
void
_initialize_fork_child (void)
{
  exec_pathname = savestring ("", 1);
  exec_argv0 = savestring ("", 1);

  add_setshow_string_cmd ("exec-pathname", no_class,
			  &exec_pathname, _("\
Set the pathname to be used to start the target executable."), _("\
Show the pathname to be used to start the target executable."), NULL,
			  NULL, NULL,
			  &setlist, &showlist);

  add_setshow_string_cmd ("exec-argv0", no_class,
			  &exec_argv0, _("\
Set the value of argv[0] to be passed to the target executable.\n\
This will be used only if 'start-with-shell' is set to 'off'."), _("\
x"), NULL,
			  NULL, NULL,
			  &setlist, &showlist);

  add_setshow_boolean_cmd ("start-with-shell", class_obscure,
			   &start_with_shell_flag, _("\
Set if GDB should use shell to invoke inferior (performs argument expansion in shell)."), _("\
Show if GDB should use shell to invoke inferior (performs argument expansion in shell)."), NULL,
			   NULL, NULL,
			   &setlist, &showlist);

}
/* APPLE LOCAL end start with shell */
