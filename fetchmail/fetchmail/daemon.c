/*
 * daemon.c -- turn a process into a daemon under POSIX, SYSV, BSD.
 *
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else /* !HAVE_FCNTL_H */
#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif /* HAVE_SYS_FCNTL_H */
#endif /* !HAVE_FCNTL_H */
#include <sys/stat.h>	/* get umask(2) prototyped */

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#if defined(STDC_HEADERS)
#include <stdlib.h>
#endif

#if defined(QNX)
#include <unix.h>
#endif

#if !defined(HAVE_SETSID) && defined(SIGTSTP)
#if defined(HAVE_TERMIOS_H)
#  include <termios.h>		/* for TIOCNOTTY under Linux */
#endif

#if !defined(TIOCNOTTY) && defined(HAVE_SGTTY_H)
#  include <sgtty.h>		/* for TIOCNOTTY under NEXTSTEP */
#endif
#endif /* !defined(HAVE_SETSID) && defined(SIGTSTP) */

/* BSD portability hack */
#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD	SIGCLD
#endif

#include "fetchmail.h"
#include "tunable.h"

RETSIGTYPE
sigchld_handler (int sig)
/* process SIGCHLD to obtain the exit code of the terminating process */
{
    extern volatile int lastsig;		/* last signal received */
    pid_t pid;

#if 	defined(HAVE_WAITPID)				/* the POSIX way */
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
	continue; /* swallow 'em up. */
#elif 	defined(HAVE_WAIT3)				/* the BSD way */
#if defined(HAVE_UNION_WAIT) && !defined(__FreeBSD__)
    union wait status;
#else
    int status;
#endif

    while ((pid = wait3(&status, WNOHANG, 0)) > 0)
	continue; /* swallow 'em up. */
#else	/* Zooks! Nothing to do but wait(), and hope we don't block... */
    int status;

    wait(&status);
#endif
    lastsig = SIGCHLD;
}

/* 
 * This function is called by other parts of the program to
 * setup the sigchld handler after a change to the signal context.
 * This is done to improve robustness of the signal handling code.
 */
void deal_with_sigchld(void)
{
  RETSIGTYPE sigchld_handler(int);
#ifdef HAVE_SIGACTION
  struct sigaction sa_new;

  memset (&sa_new, 0, sizeof sa_new);
  sigemptyset (&sa_new.sa_mask);
  /* sa_new.sa_handler = SIG_IGN;     pointless */

  /* set up to catch child process termination signals */ 
  sa_new.sa_handler = sigchld_handler;
#ifdef SA_RESTART	/* SunOS 4.1 portability hack */
  sa_new.sa_flags = SA_RESTART | SA_NOCLDSTOP;
#endif
  sigaction (SIGCHLD, &sa_new, NULL);
#if defined(SIGPWR)
  sigaction (SIGPWR, &sa_new, NULL);
#endif
#else /* HAVE_SIGACTION */
  signal(SIGCHLD, sigchld_handler); 
#if defined(SIGPWR)
  signal(SIGPWR, sigchld_handler); 
#endif
#endif /* HAVE_SIGACTION */
}

int
daemonize (const char *logfile, void (*termhook)(int))
/* detach from control TTY, become process group leader, catch SIGCHLD */
{
  int fd;
  pid_t childpid;
#ifdef HAVE_SIGACTION
  struct sigaction sa_new;
#endif /* HAVE_SIGACTION */

  /* if we are started by init (process 1) via /etc/inittab we needn't 
     bother to detach from our process group context */

  if (getppid() == 1) 
    goto nottyDetach;

  /* Ignore BSD terminal stop signals */
#ifdef HAVE_SIGACTION
  memset (&sa_new, 0, sizeof sa_new);
  sigemptyset (&sa_new.sa_mask);
  sa_new.sa_handler = SIG_IGN;
#ifdef SA_RESTART	/* SunOS 4.1 portability hack */
  sa_new.sa_flags = SA_RESTART;
#endif
#endif /* HAVE_SIGACTION */
#ifdef 	SIGTTOU
#ifndef HAVE_SIGACTION
  signal(SIGTTOU, SIG_IGN);
#else
  sigaction (SIGTTOU, &sa_new, NULL);
#endif /* HAVE_SIGACTION */
#endif
#ifdef	SIGTTIN
#ifndef HAVE_SIGACTION
  signal(SIGTTIN, SIG_IGN);
#else
  sigaction (SIGTTIN, &sa_new, NULL);
#endif /* HAVE_SIGACTION */
#endif
#ifdef	SIGTSTP
#ifndef HAVE_SIGACTION
  signal(SIGTSTP, SIG_IGN);
#else
  sigaction (SIGTSTP, &sa_new, NULL);
#endif /* HAVE_SIGACTION */
#endif

  /* In case we were not started in the background, fork and let
     the parent exit.  Guarantees that the child is not a process
     group leader */

  if ((childpid = fork()) < 0) {
    report(stderr, "fork (%s)\n", strerror(errno));
    return(PS_IOERR);
  }
  else if (childpid > 0) 
    exit(0);  /* parent */

  
  /* Make ourselves the leader of a new process group with no
     controlling terminal */

#if	defined(HAVE_SETSID)		/* POSIX */
  /* POSIX makes this soooo easy to do */
  if (setsid() < 0) {
    report(stderr, "setsid (%s)\n", strerror(errno));
    return(PS_IOERR);
  }
#elif	defined(SIGTSTP)		/* BSD */
  /* change process group */
#ifndef __EMX__
  setpgrp(0, getpid());
#endif
  /* lose controlling tty */
  if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
    ioctl(fd, TIOCNOTTY, (char *) 0);
    close(fd);	/* not checking should be safe, there were no writes */
  }
#else					/* SVR3 and older */
  /* change process group */
#ifndef __EMX__
  setpgrp();
#endif
  
  /* lose controlling tty */
#ifndef HAVE_SIGACTION
  signal(SIGHUP, SIG_IGN);
#else
  sigaction (SIGHUP, &sa_new, NULL);
#endif /* HAVE_SIGACTION */
  if ((childpid = fork()) < 0) {
    report(stderr, "fork (%)\n", strerror(errno));
    return(PS_IOERR);
  }
  else if (childpid > 0) {
    exit(0); 	/* parent */
  }
#endif

nottyDetach:

  /* Close any/all open file descriptors */
#if 	defined(HAVE_GETDTABLESIZE)
  for (fd = getdtablesize()-1;  fd >= 0;  fd--)
#elif	defined(NOFILE)
  for (fd = NOFILE-1;  fd >= 0;  fd--)
#else		/* make an educated guess */
  for (fd = 19;  fd >= 0;  fd--)
#endif
  {
    close(fd);	/* not checking this should be safe, no writes */
  }

  /* Reopen stdin descriptor on /dev/null */
  if ((fd = open("/dev/null", O_RDWR)) < 0) {   /* stdin */
    report(stderr, "open: /dev/null (%s)\n", strerror(errno));
    return(PS_IOERR);
  }

  if (logfile)
    fd = open(logfile, O_CREAT|O_WRONLY|O_APPEND, 0666);	/* stdout */
  else
    if (dup(fd) < 0) {				/* stdout */
      report(stderr, "dup (%s)\n", strerror(errno));
      return(PS_IOERR);
    }
  if (dup(fd) < 0) {				/* stderr */
    report(stderr, "dup (%s)\n", strerror(errno));
    return(PS_IOERR);
  }

  /* move to root directory, so we don't prevent filesystem unmounts */
  chdir("/");

  /* set our umask to something reasonable (we hope) */
#if defined(DEF_UMASK)
  umask(DEF_UMASK);
#else
  umask(022);
#endif

  deal_with_sigchld();

  return(0);
}

flag isafile(int fd)
/* is the given fd attached to a file? (used to control logging) */
{
    struct stat stbuf;

    /*
     * We'd like just to return 1 on (S_IFREG | S_IFBLK),
     * but weirdly enough, Linux ptys seem to have S_IFBLK
     * so this test would fail when run on an xterm.
     */
    if (isatty(fd) || fstat(fd, &stbuf))
	return(0);
    else if (stbuf.st_mode & (S_IFREG))
	return(1);
    return(0);
}

/* daemon.c ends here */
