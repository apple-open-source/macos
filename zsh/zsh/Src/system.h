/*
 * system.h - system configuration header file
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

#ifdef __hpux
#define _INCLUDE_POSIX_SOURCE
#define _INCLUDE_XOPEN_SOURCE
#define _INCLUDE_HPUX_SOURCE
#endif

/* NeXT has half-implemented POSIX support *
 * which currently fools configure         */
#ifdef __NeXT__
# undef HAVE_TERMIOS_H
# undef HAVE_SYS_UTSNAME_H
#endif

#ifdef PROTOTYPES
# define _(Args) Args
#else
# define _(Args) ()
#endif

#ifdef HAVE_LIBC_H     /* NeXT */
# include <libc.h>
#endif

#ifdef HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>
#include <setjmp.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NLENGTH(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NLENGTH(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifdef HAVE_STDLIB_H
# ifdef ZSH_MEM
   /* malloc and calloc are macros in GNU's stdlib.h unless the
    * the __MALLOC_0_RETURNS_NULL macro is defined */
#  define __MALLOC_0_RETURNS_NULL
# endif
# include <stdlib.h>
#endif

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

/* This is needed by some old SCO unices */
#ifndef HAVE_STRUCT_TIMEZONE
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

/* Sco needs the following include for struct utimbuf *
 * which is strange considering we do not use that    *
 * anywhere in the code                               */
#ifdef __sco
# include <utime.h>
#endif

#ifdef HAVE_SYS_TIMES_H
# include <sys/times.h>
#endif

#if STDC_HEADERS || HAVE_STRING_H
# include <string.h>
/* An ANSI string.h and pre-ANSI memory.h might conflict.  */
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif /* not STDC_HEADERS and HAVE_MEMORY_H */
#else   /* not STDC_HEADERS and not HAVE_STRING_H */
# include <strings.h>
/* memory.h and strings.h conflict on some systems.  */
#endif /* not STDC_HEADERS and not HAVE_STRING_H */

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

/* we should be getting this value from pathconf(_PC_PATH_MAX) */
/* but this is too much trouble                                */
#ifndef PATH_MAX
# ifdef MAXPATHLEN
#  define PATH_MAX MAXPATHLEN
# else
   /* so we will just pick something */
#  define PATH_MAX 1024
# endif
#endif

/* we should be getting this value from sysconf(_SC_OPEN_MAX) */
/* but this is too much trouble                               */
#ifndef OPEN_MAX
# ifdef NOFILE
#  define OPEN_MAX NOFILE
# else
   /* so we will just pick something */
#  define OPEN_MAX 64
# endif
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/file.h>
#endif

/* The following will only be defined if <sys/wait.h> is POSIX.    *
 * So we don't have to worry about union wait. But some machines   *
 * (NeXT) include <sys/wait.h> from other include files, so we     *
 * need to undef and then redefine the wait macros if <sys/wait.h> *
 * is not POSIX.                                                   */

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#else
#undef WIFEXITED
#undef WEXITSTATUS
#undef WIFSIGNALED
#undef WTERMSIG
#undef WCOREDUMP
#undef WIFSTOPPED
#undef WSTOPSIG
#endif

/* missing macros for wait/waitpid/wait3 */
#ifndef WIFEXITED
# define WIFEXITED(X) (((X)&0377)==0)
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(X) (((X)>>8)&0377)
#endif
#ifndef WIFSIGNALED
# define WIFSIGNALED(X) (((X)&0377)!=0&&((X)&0377)!=0177)
#endif
#ifndef WTERMSIG
# define WTERMSIG(X) ((X)&0177)
#endif
#ifndef WCOREDUMP
# define WCOREDUMP(X) ((X)&0200)
#endif
#ifndef WIFSTOPPED
# define WIFSTOPPED(X) (((X)&0377)==0177)
#endif
#ifndef WSTOPSIG
# define WSTOPSIG(X) (((X)>>8)&0377)
#endif

#ifdef HAVE_SYS_SELECT_H
# ifndef TIME_H_SELECT_H_CONFLICTS
#  include <sys/select.h>
# endif
#endif

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#ifdef HAVE_TERMIOS_H
# ifdef __sco
   /* termios.h includes sys/termio.h instead of sys/termios.h; *
    * hence the declaration for struct termios is missing       */
#  include <sys/termios.h>
# else
#  include <termios.h>
# endif
# ifdef _POSIX_VDISABLE
#  define VDISABLEVAL _POSIX_VDISABLE
# else
#  define VDISABLEVAL 0
# endif
# define HAS_TIO 1
#else    /* not TERMIOS */
# ifdef HAVE_TERMIO_H
#  include <termio.h>
#  define VDISABLEVAL -1
#  define HAS_TIO 1
# else   /* not TERMIOS and TERMIO */
#  include <sgtty.h>
# endif  /* HAVE_TERMIO_H  */
#endif   /* HAVE_TERMIOS_H */

#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#endif

#if defined(GWINSZ_IN_SYS_IOCTL) || defined(CLOBBERS_TYPEAHEAD)
# include <sys/ioctl.h>
#endif
#ifdef WINSIZE_IN_PTEM
# include <sys/stream.h>
# include <sys/ptem.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifdef HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif

#ifdef HAVE_UTMPX_H
# include <utmpx.h>
# define STRUCT_UTMP struct utmpx
# define ut_time ut_xtime
#else
# include <utmp.h>
# define STRUCT_UTMP struct utmp
#endif
 
#if !defined (UTMP_FILE) && defined (_PATH_UTMP)        /* 4.4BSD.  */
# define UTMP_FILE _PATH_UTMP
#endif

#if !defined (WTMP_FILE) && defined (_PATH_WTMP)
# define WTMP_FILE _PATH_WTMP
#endif
 
#ifdef UTMPX_FILE                                /* Solaris, SysVr4 */
# undef  UTMP_FILE
# define UTMP_FILE UTMPX_FILE
#endif

#ifdef WTMPX_FILE                                /* Solaris. SysVr4 */
# undef  WTMP_FILE
# define WTMP_FILE WTMPX_FILE
#endif
 
#ifndef UTMP_FILE                                /* use value found by configure */
# define UTMP_FILE UTMP_FILE_CONFIG
#endif

#ifndef WTMP_FILE                                /* use value found by configure */
# define WTMP_FILE WTMP_FILE_CONFIG
#endif

#ifdef HAVE_UT_HOST
# define DEFAULT_WATCHFMT "%n has %a %l from %m."
#else
# define DEFAULT_WATCHFMT "%n has %a %l."
#endif

#define DEFAULT_WORDCHARS "*?_-.[]~=/&;!#$%^(){}<>"
#define DEFAULT_TIMEFMT   "%J  %U user %S system %P cpu %*E total"

/* Posix getpgrp takes no argument, while the BSD version *
 * takes the process ID as an argument                    */
#ifdef GETPGRP_VOID
# define GETPGRP() getpgrp()
#else
# define GETPGRP() getpgrp(0)
#endif

#ifndef HAVE_GETLOGIN
# define getlogin() cuserid(NULL)
#endif

#ifdef HAVE_SETPGID
# define setpgrp setpgid
#endif

/* can we set the user/group id of a process */

#ifndef HAVE_SETUID
# ifdef HAVE_SETREUID
#  define setuid(X) setreuid(X,X)
#  define setgid(X) setregid(X,X)
#  define HAVE_SETUID
# endif
#endif

/* can we set the effective user/group id of a process */

#ifndef HAVE_SETEUID
# ifdef HAVE_SETREUID
#  define seteuid(X) setreuid(-1,X)
#  define setegid(X) setregid(-1,X)
#  define HAVE_SETEUID
# else
#  ifdef HAVE_SETRESUID
#   define seteuid(X) setresuid(-1,X,-1)
#   define setegid(X) setresgid(-1,X,-1)
#   define HAVE_SETEUID
#  endif
# endif
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
# if defined(__hpux) && !defined(RLIMIT_CPU)
/* HPUX does have the BSD rlimits in the kernel.  Officially they are *
 * unsupported but quite a few of them like RLIMIT_CORE seem to work. *
 * All the following are in the <sys/resource.h> but made visible     *
 * only for the kernel.                                               */
#  define	RLIMIT_CPU	0
#  define	RLIMIT_FSIZE	1
#  define	RLIMIT_DATA	2
#  define	RLIMIT_STACK	3
#  define	RLIMIT_CORE	4
#  define	RLIMIT_RSS	5
#  define	RLIMIT_NOFILE   6
#  define	RLIMIT_OPEN_MAX	RLIMIT_NOFILE
#  define	RLIM_NLIMITS	7
#  define	RLIM_INFINITY	0x7fffffff
# endif
#endif

/* we use the SVR4 constant instead of the BSD one */
#if !defined(RLIMIT_NOFILE) && defined(RLIMIT_OFILE)
# define RLIMIT_NOFILE RLIMIT_OFILE
#endif
#if !defined(RLIMIT_VMEM) && defined(RLIMIT_AS)
# define RLIMIT_VMEM RLIMIT_AS
#endif

/* DIGBUFSIZ is the length of a buffer which can hold the -LONG_MAX-1  *
 * (or, with 64-bit support on 32-bit systems, maybe -LONG_LONG_MAX-1) *
 * converted to printable decimal form including the sign and the      *
 * terminating null character. Below 0.30103 > lg 2.                   */
#define DIGBUFSIZE ((int) (((SIZEOF_ZLONG * 8) - 1) * 0.30103) + 3)

/* If your stat macros are broken, we will *
 * just undefine them.                     */
#ifdef  STAT_MACROS_BROKEN
# ifdef S_ISBLK
#  undef S_ISBLK
# endif
# ifdef S_ISCHR
#  undef S_ISCHR
# endif
# ifdef S_ISDIR
#  undef S_ISDIR
# endif
# ifdef S_ISFIFO
#  undef S_ISFIFO
# endif
# ifdef S_ISLNK
#  undef S_ISLNK
# endif
# ifdef S_ISMPB
#  undef S_ISMPB
# endif
# ifdef S_ISMPC
#  undef S_ISMPC
# endif
# ifdef S_ISNWK
#  undef S_ISNWK
# endif
# ifdef S_ISREG
#  undef S_ISREG
# endif
# ifdef S_ISSOCK
#  undef S_ISSOCK
# endif
#endif  /* STAT_MACROS_BROKEN.  */

/* If you are missing the stat macros, we *
 * define our own                         */
#ifndef S_IFMT
# define S_IFMT 0170000
#endif
#if !defined(S_ISBLK) && defined(S_IFBLK)
# define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#endif
#if !defined(S_ISCHR) && defined(S_IFCHR)
# define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#endif
#if !defined(S_ISDIR) && defined(S_IFDIR)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG) && defined(S_IFREG)
# define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISFIFO) && defined(S_IFIFO)
# define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif
#if !defined(S_ISLNK) && defined(S_IFLNK)
# define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif
#if !defined(S_ISSOCK) && defined(S_IFSOCK)
# define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#endif
#if !defined(S_ISMPB) && defined(S_IFMPB)        /*   V7  */
# define S_ISMPB(m) (((m) & S_IFMT) == S_IFMPB)
# define S_ISMPC(m) (((m) & S_IFMT) == S_IFMPC)
#endif
#if !defined(S_ISNWK) && defined(S_IFNWK)        /* HP/UX */
# define S_ISNWK(m) (((m) & S_IFMT) == S_IFNWK)
#endif

#ifndef HAVE_LSTAT
# define lstat(X,Y) stat(X,Y)
#endif

#ifndef F_OK          /* missing macros for access() */
# define F_OK 0
# define X_OK 1
# define W_OK 2
# define R_OK 4
#endif

extern char **environ;     /* environment variable list */

/* These variables are sometimes defined in, *
 * and needed by, the termcap library.       */
 
#if MUST_DEFINE_OSPEED
extern char PC, *BC, *UP;
extern short ospeed;
#endif

/* Rename some global zsh variables to avoid *
 * possible name clashes with libc, etc.     */

#define cs zshcs
#define ll zshll
#define setterm zsetterm
#define refresh zrefresh

#ifndef O_NOCTTY
# define O_NOCTTY 0
#endif

/* Can we do locale stuff? */
#undef USE_LOCALE
#if defined(CONFIG_LOCALE) && defined(HAVE_SETLOCALE) && defined(LC_ALL)
# define USE_LOCALE 1
#endif /* CONFIG_LOCALE && HAVE_SETLOCALE && LC_ALL */
