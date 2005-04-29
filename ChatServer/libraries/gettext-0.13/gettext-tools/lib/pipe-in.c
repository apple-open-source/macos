/* Creation of subprocesses, communicating via pipes.
   Copyright (C) 2001-2003 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2001.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Specification.  */
#include "pipe.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "error.h"
#include "exit.h"
#include "fatal-signal.h"
#include "wait-process.h"
#include "gettext.h"

#define _(str) gettext (str)

#if defined _MSC_VER || defined __MINGW32__

/* Native Woe32 API.  */
# include <process.h>
# include "w32spawn.h"

#else

/* Unix API.  */
# ifdef HAVE_POSIX_SPAWN
#  include <spawn.h>
# else
#  ifdef HAVE_VFORK_H
#   include <vfork.h>
#  endif
# endif

#endif

#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif


#ifdef EINTR

/* EINTR handling for close(), open().
   These functions can return -1/EINTR even though we don't have any
   signal handlers set up, namely when we get interrupted via SIGSTOP.  */

static inline int
nonintr_close (int fd)
{
  int retval;

  do
    retval = close (fd);
  while (retval < 0 && errno == EINTR);

  return retval;
}
#define close nonintr_close

static inline int
nonintr_open (const char *pathname, int oflag, mode_t mode)
{
  int retval;

  do
    retval = open (pathname, oflag, mode);
  while (retval < 0 && errno == EINTR);

  return retval;
}
#undef open /* avoid warning on VMS */
#define open nonintr_open

#endif


/* Open a pipe for input from a child process.
 * The child's stdin comes from a file.
 *
 *           read        system                write
 *    parent  <-   fd[0]   <-   STDOUT_FILENO   <-   child
 *
 */
pid_t
create_pipe_in (const char *progname,
		const char *prog_path, char **prog_argv,
		const char *prog_stdin, bool null_stderr,
		bool slave_process, bool exit_on_error,
		int fd[1])
{
#if defined _MSC_VER || defined __MINGW32__

  /* Native Woe32 API.
     This uses _pipe(), dup2(), and spawnv().  It could also be implemented
     using the low-level functions CreatePipe(), DuplicateHandle(),
     CreateProcess() and _open_osfhandle(); see the GNU make and GNU clisp
     and cvs source code.  */
  int ifd[2];
  int orig_stdin;
  int orig_stdout;
  int orig_stderr;
  int child;
  int nulloutfd;
  int stdinfd;

  prog_argv = prepare_spawn (prog_argv);

  if (_pipe (ifd, 4096, O_BINARY | O_NOINHERIT) < 0)
    error (EXIT_FAILURE, errno, _("cannot create pipe"));
/* Data flow diagram:
 *
 *           read        system         write
 *    parent  <-  ifd[0]   <-   ifd[1]   <-   child
 */

  /* Save standard file handles of parent process.  */
  if (prog_stdin != NULL)
    orig_stdin = dup_noinherit (STDIN_FILENO);
  orig_stdout = dup_noinherit (STDOUT_FILENO);
  if (null_stderr)
    orig_stderr = dup_noinherit (STDERR_FILENO);
  child = -1;

  /* Create standard file handles of child process.  */
  nulloutfd = -1;
  stdinfd = -1;
  if (dup2 (ifd[1], STDOUT_FILENO) >= 0
      && (!null_stderr
	  || ((nulloutfd = open ("NUL", O_RDWR, 0)) >= 0
	      && (nulloutfd == STDERR_FILENO
		  || (dup2 (nulloutfd, STDERR_FILENO) >= 0
		      && close (nulloutfd) >= 0))))
      && (prog_stdin == NULL
	  || ((stdinfd = open (prog_stdin, O_RDONLY, 0)) >= 0
	      && (stdinfd == STDIN_FILENO
		  || (dup2 (stdinfd, STDIN_FILENO) >= 0
		      && close (stdinfd) >= 0)))))
    /* The child process doesn't inherit ifd[0] and ifd[1], but it
       inherits all open()ed or dup2()ed file handles (which is what
       we want in the case of STD*_FILENO) and also orig_stdin,
       orig_stdout, orig_stderr (which is not explicitly wanted but
       harmless).  */
    child = spawnvp (P_NOWAIT, prog_path, prog_argv);
  if (stdinfd >= 0)
    close (stdinfd);
  if (nulloutfd >= 0)
    close (nulloutfd);

  /* Restore standard file handles of parent process.  */
  if (null_stderr)
    dup2 (orig_stderr, STDERR_FILENO), close (orig_stderr);
  dup2 (orig_stdout, STDOUT_FILENO), close (orig_stdout);
  if (prog_stdin != NULL)
    dup2 (orig_stdin, STDIN_FILENO), close (orig_stdin);

  close (ifd[1]);
  if (child == -1)
    {
      if (exit_on_error || !null_stderr)
	error (exit_on_error ? EXIT_FAILURE : 0, errno,
	       _("%s subprocess failed"), progname);
      close (ifd[0]);
      return -1;
    }

  fd[0] = ifd[0];
  return child;

#else

  /* Unix API.  */
  int ifd[2];
#if HAVE_POSIX_SPAWN
  sigset_t blocked_signals;
  posix_spawn_file_actions_t actions;
  bool actions_allocated;
  posix_spawnattr_t attrs;
  bool attrs_allocated;
  int err;
  pid_t child;
#else
  int child;
#endif

  if (pipe (ifd) < 0)
    error (EXIT_FAILURE, errno, _("cannot create pipe"));
/* Data flow diagram:
 *
 *           read        system         write
 *    parent  <-  ifd[0]   <-   ifd[1]   <-   child
 */

#if HAVE_POSIX_SPAWN
  if (slave_process)
    {
      sigprocmask (SIG_SETMASK, NULL, &blocked_signals);
      block_fatal_signals ();
    }
  actions_allocated = false;
  attrs_allocated = false;
  if ((err = posix_spawn_file_actions_init (&actions)) != 0
      || (actions_allocated = true,
	  (err = posix_spawn_file_actions_adddup2 (&actions,
						   ifd[1], STDOUT_FILENO)) != 0
	  || (err = posix_spawn_file_actions_addclose (&actions, ifd[1])) != 0
	  || (err = posix_spawn_file_actions_addclose (&actions, ifd[0])) != 0
	  || (null_stderr
	      && (err = posix_spawn_file_actions_addopen (&actions,
							  STDERR_FILENO,
							  "/dev/null", O_RDWR,
							  0))
		 != 0)
	  || (prog_stdin != NULL
	      && (err = posix_spawn_file_actions_addopen (&actions,
							  STDIN_FILENO,
							  prog_stdin, O_RDONLY,
							  0))
		 != 0)
	  || (slave_process
	      && ((err = posix_spawnattr_init (&attrs)) != 0
		  || (attrs_allocated = true,
		      (err = posix_spawnattr_setsigmask (&attrs,
							 &blocked_signals))
		      != 0
		      || (err = posix_spawnattr_setflags (&attrs,
							POSIX_SPAWN_SETSIGMASK))
			 != 0)))
	  || (err = posix_spawnp (&child, prog_path, &actions,
				  attrs_allocated ? &attrs : NULL, prog_argv,
				  environ))
	     != 0))
    {
      if (actions_allocated)
	posix_spawn_file_actions_destroy (&actions);
      if (attrs_allocated)
	posix_spawnattr_destroy (&attrs);
      if (slave_process)
	unblock_fatal_signals ();
      if (exit_on_error || !null_stderr)
	error (exit_on_error ? EXIT_FAILURE : 0, err,
	       _("%s subprocess failed"), progname);
      close (ifd[0]);
      close (ifd[1]);
      return -1;
    }
  posix_spawn_file_actions_destroy (&actions);
  if (attrs_allocated)
    posix_spawnattr_destroy (&attrs);
#else
  if (slave_process)
    block_fatal_signals ();
  /* Use vfork() instead of fork() for efficiency.  */
  if ((child = vfork ()) == 0)
    {
      /* Child process code.  */
      int nulloutfd;
      int stdinfd;

      if (dup2 (ifd[1], STDOUT_FILENO) >= 0
	  && close (ifd[1]) >= 0
	  && close (ifd[0]) >= 0
	  && (!null_stderr
	      || ((nulloutfd = open ("/dev/null", O_RDWR, 0)) >= 0
		  && (nulloutfd == STDERR_FILENO
		      || (dup2 (nulloutfd, STDERR_FILENO) >= 0
			  && close (nulloutfd) >= 0))))
	  && (prog_stdin == NULL
	      || ((stdinfd = open (prog_stdin, O_RDONLY, 0)) >= 0
		  && (stdinfd == STDIN_FILENO
		      || (dup2 (stdinfd, STDIN_FILENO) >= 0
			  && close (stdinfd) >= 0))))
	  && (!slave_process || (unblock_fatal_signals (), true)))
	execvp (prog_path, prog_argv);
      _exit (127);
    }
  if (child == -1)
    {
      if (slave_process)
	unblock_fatal_signals ();
      if (exit_on_error || !null_stderr)
	error (exit_on_error ? EXIT_FAILURE : 0, errno,
	       _("%s subprocess failed"), progname);
      close (ifd[0]);
      close (ifd[1]);
      return -1;
    }
#endif
  if (slave_process)
    {
      register_slave_subprocess (child);
      unblock_fatal_signals ();
    }
  close (ifd[1]);

  fd[0] = ifd[0];
  return child;

#endif
}
