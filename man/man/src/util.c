/*
 * util.c
 *
 * Copyright (c) 1990, 1991, John W. Eaton.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with the man
 * distribution.  
 *
 * John W. Eaton
 * jwe@che.utexas.edu
 * Department of Chemical Engineering
 * The University of Texas at Austin
 * Austin, Texas  78712
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"
#include "gripes.h"
#include "man.h"		/* for debug */

/*
 * Extract last element of a name like /foo/bar/baz.
 */
const char *
mkprogname (const char *s) {
     const char *t;

     t = strrchr (s, '/');
     if (t == (char *)NULL)
	  t = s;
     else
	  t++;

     return my_strdup (t);
}

/*
 * Is file a nonempty and newer than file b?
 *
 * case:
 *
 *   a newer than b              returns    1
 *   a older than b              returns    0
 *   stat on a fails or a empty  returns   -1
 *   stat on b fails or b empty  returns   -2
 *   both fail or empty  	 returns   -3
 */
int
is_newer (const char *fa, const char *fb) {
     struct stat fa_sb;
     struct stat fb_sb;
     register int fa_stat;
     register int fb_stat;
     register int status = 0;

     fa_stat = stat (fa, &fa_sb);
     if (fa_stat != 0 || fa_sb.st_size == 0)
	  status = 1;

     fb_stat = stat (fb, &fb_sb);
     if (fb_stat != 0 || fb_sb.st_size == 0)
	  status |= 2;

     if (status != 0)
	  return -status;

     return (fa_sb.st_mtime > fb_sb.st_mtime);
}

int ruid, rgid, euid, egid, suid;

void
get_permissions (void) {
     ruid = getuid();
     euid = geteuid();
     rgid = getgid();
     egid = getegid();
     suid = (ruid != euid || rgid != egid);
}

void
no_privileges (void) {
     if (suid) {
#if !defined (__CYGWIN__) && !defined (__BEOS__)
	  setreuid(ruid, ruid);
	  setregid(rgid, rgid);
#endif
	  suid = 0;
     }
}

/*
 * What to do upon an interrupt?  Experience shows that
 * if we exit immediately, sh notices that its child has
 * died and will try to fiddle with the tty.
 * Simultaneously, also less will fiddle with the tty,
 * resetting the mode before exiting.
 * This leads to undesirable races. So, we catch SIGINT here
 * and exit after the child has exited.
 */
static int interrupted = 0;
static void catch_int(int a) {
	interrupted = 1;
}

static int
system1 (const char *command) {
	void (*prev_handler)(int) = signal (SIGINT,catch_int);
	int ret = system(command);

	/* child terminated with signal? */
	if (WIFSIGNALED(ret) &&
	    (WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT))
		exit(1);

	/* or we caught an interrupt? */
	if (interrupted)
		exit(1);

	signal(SIGINT,prev_handler);
	return ret;
}

static int
my_system (const char *command) {
     int pid, pid2, status, stat;

     if (!suid)
	  return system1 (command);

#ifdef _POSIX_SAVED_IDS

     /* we need not fork */
     setuid(ruid);
     setgid(rgid);
     status = system1(command);
     setuid(euid);
     setgid(egid);
     return (WIFEXITED(status) ? WEXITSTATUS(status) : 127);
#endif

     fflush(stdout); fflush(stderr);
     pid = fork();
     if (pid == -1) {
	  perror(progname);
	  fatal (CANNOT_FORK, command);
     }
     if (pid == 0) {
	  setuid(ruid);
	  setgid(rgid);
	  status = system1 (command);
	  exit(WIFEXITED(status) ? WEXITSTATUS(status) : 127);
     }
     pid2 = wait (&stat);
     if (pid2 == -1) {
	  perror(progname);
	  fatal (WAIT_FAILED, command); 	/* interrupted? */
     }
     if (pid2 != pid)
	  fatal (GOT_WRONG_PID);
     if (WIFEXITED(stat) && WEXITSTATUS(stat) != 127)
	  return WEXITSTATUS(stat);
     fatal (CHILD_TERMINATED_ABNORMALLY, command);
     return -1;			/* not reached */
}

FILE *
my_popen(const char *command, const char *type) {
     FILE *r;

     if (!suid)
	  return popen(command, type);

#ifdef _POSIX_SAVED_IDS
     setuid(ruid);
     setgid(rgid);
     r = popen(command, type);
     setuid(euid);
     setgid(egid);
     return r;
#endif

     no_privileges();
     return popen(command, type);
}

#define NOT_SAFE "/unsafe/"

/*
 * Attempt a system () call.
 */
int
do_system_command (const char *command, int silent) {
     int status = 0;

     /*
      * If we're debugging, don't really execute the command
      */
     if ((debug & 1) || !strncmp(command, NOT_SAFE, strlen(NOT_SAFE)))
	  fatal (NO_EXEC, command);
     else
	  status = my_system (command);

     if (status && !silent)
	  gripe (SYSTEM_FAILED, command, status);

     return status;
}

char *
my_malloc (int n) {
    char *s = malloc(n);
    if (!s)
	fatal (OUT_OF_MEMORY, n);
    return s;
}

char *
my_strdup (const char *s) {
    char *t = my_malloc(strlen(s) + 1);
    strcpy(t, s);
    return t;
}

/*
 * Call: my_xsprintf(format,s1,s2,...) where format only contains %s/%S/%Q
 * (or %d or %o) and all %s/%S/%Q parameters are strings.
 * Result: allocates a new string containing the sprintf result.
 * The %S parameters are checked for being shell safe.
 * The %Q parameters are checked for being shell safe inside single quotes.
 */

static int
is_shell_safe(const char *ss, int quoted) {
	char *bad = " ;'\\\"<>|&";
	char *p;

	if (quoted)
		bad++;			/* allow a space inside quotes */
	for (p = bad; *p; p++)
		if (strchr(ss, *p))
			return 0;
	return 1;
}

static void
nothing(int x) {}

char *
my_xsprintf (char *format, ...) {
	va_list p;
	char *s, *ss, *fm;
	int len;

	len = strlen(format) + 1;
	fm = my_strdup(format);

	va_start(p, format);
	for (s = fm; *s; s++) {
		if (*s == '%') {
			switch (s[1]) {
			case 'Q':
			case 'S': /* check and turn into 's' */
				ss = va_arg(p, char *);
				if (!is_shell_safe(ss, (s[1] == 'Q')))
					return my_strdup(NOT_SAFE);
				len += strlen(ss);
				s[1] = 's';
				break;
			case 's':
				len += strlen(va_arg(p, char *));
				break;
			case 'd':
			case 'o':
			case 'c':
				len += 20;
				nothing(va_arg(p, int)); /* advance */
				break;
			default:
				fprintf(stderr,
					"my_xsprintf called with %s\n",
					format);
				exit(1);
			}
		}
	}
	va_end(p);

	s = my_malloc(len);
	va_start(p, format);
	vsprintf(s, fm, p);
	va_end(p);

	return s;
}
