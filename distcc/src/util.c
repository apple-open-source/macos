/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/stat.h>

#include <sys/time.h>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <sys/param.h>

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "util.h"
#include "exitcode.h"


			/* I will make a man more precious than fine
			 * gold; even a man than the golden wedge of
			 * Ophir.
			 *		-- Isaiah 13:12 */


int dcc_exit(int exitcode)
{
    struct rusage self_ru, children_ru;

    if (getrusage(RUSAGE_SELF, &self_ru)) {
        rs_log_warning("getrusage(RUSAGE_SELF) failed: %s", strerror(errno));
        memset(&self_ru, 0, sizeof self_ru);
    }
    if (getrusage(RUSAGE_CHILDREN, &children_ru)) {
        rs_log_warning("getrusage(RUSAGE_CHILDREN) failed: %s", strerror(errno));
        memset(&children_ru, 0, sizeof children_ru);
    }
    
    rs_log(RS_LOG_INFO,
           "exit: code %d; self: %d.%04d user %d.%04d sys; children: %d.%04d user %d.%04d sys",
           exitcode,
           (int) self_ru.ru_utime.tv_sec, (int) self_ru.ru_utime.tv_usec,
           (int) self_ru.ru_stime.tv_sec, (int) self_ru.ru_stime.tv_usec,
           (int) children_ru.ru_utime.tv_sec, (int) children_ru.ru_utime.tv_usec,
           (int) children_ru.ru_stime.tv_sec, (int)  children_ru.ru_stime.tv_usec);

    exit(exitcode);
}


int str_endswith(const char *tail, const char *tiger)
{
        size_t len_tail = strlen(tail);
	size_t len_tiger = strlen(tiger);

	if (len_tail > len_tiger)
		return 0;

	return !strcmp(tiger + len_tiger - len_tail, tail);
}


int str_startswith(const char *head, const char *worm)
{
    return !strncmp(head, worm, strlen(head));
}



/**
 * Skim through NULL-terminated @p argv, looking for @p s.
 **/
int argv_contains(char **argv, const char *s)
{
	while (*argv) {
		if (!strcmp(*argv, s))
			return 1;
		argv++;
	}
	return 0;
}


/**
 * Redirect a file descriptor into (or out of) a file.
 *
 * Used, for example, to catch compiler error messages into a
 * temporary file.
 **/
int dcc_redirect_fd(int fd, const char *fname, int mode)
{
    int newfd;

    /* ignore errors */
    close(fd);
    
    newfd = open(fname, mode, 0666);
    if (newfd == -1) {
        rs_log_crit("failed to reopen fd%d onto %s: %s",
                    fd, fname, strerror(errno));
        return EXIT_IO_ERROR;
    } else if (newfd != fd) {
        rs_log_crit("oops, reopened fd%d onto fd%d?", fd, newfd);
        return EXIT_IO_ERROR;
    }

    return 0;
}



char *dcc_gethostname(void)
{
    static char myname[100] = "\0";

    if (!myname[0]) {
        if (gethostname(myname, sizeof myname - 1) == -1)
            strcpy(myname, "UNKNOWN");
    }

    return myname;
}


/**
 * Look up a boolean environment option, which must be either "0" or
 * "1".  The default, if it's not set or is empty, is @p default.
 **/
int dcc_getenv_bool(const char *name, int default_value)
{
    const char *e;

    e = getenv(name);
    if (!e || !*e)
        return default_value;
    if (!strcmp(e, "1"))
        return 1;
    else if (!strcmp(e, "0"))
        return 0;
    else
        return default_value;
}



/**
 * Set the `FD_CLOEXEC' flag of DESC if VALUE is nonzero,
 * or clear the flag if VALUE is 0.
 *
 * From the GNU C Library examples.
 *
 * @returns 0 on success, or -1 on error with `errno' set.
 **/
int set_cloexec_flag (int desc, int value)
{
    int oldflags = fcntl (desc, F_GETFD, 0);
    /* If reading the flags failed, return error indication now. */
    if (oldflags < 0)
        return oldflags;
    /* Set just the flag we want to set. */
    if (value != 0)
        oldflags |= FD_CLOEXEC;
    else
        oldflags &= ~FD_CLOEXEC;
    /* Store modified flag word in the descriptor. */
    return fcntl (desc, F_SETFD, oldflags);
}


/**
 * Ignore or unignore SIGPIPE.
 *
 * The server and child ignore it, because distcc code wants to see
 * EPIPE errors if something goes wrong.  However, for invoked
 * children it is set back to the default value, because they may not
 * handle the error properly.
 **/
int dcc_ignore_sigpipe(int val)
{
    struct sigaction action;

    action.sa_handler = val ? SIG_IGN : SIG_DFL;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGPIPE, &action, NULL)) {
        rs_log_warning("sigaction(SIGPIPE, %s) failed: %s",
                       val ? "ignore" : "default",
                       strerror(errno));
        return -1;
    }
    return 0;
}

/* Return the supplied path with the current-working directory prefixed (if
 * needed) and all "dir/.." references removed.  Supply path_len if you want
 * to use only a substring of the path string, otherwise make it 0. */
char *dcc_abspath(const char *path, int path_len)
{
    static char buf[MAXPATHLEN];
    unsigned len;
    char *p, *slash;

    if (*path == '/')
        len = 0;
    else {
#ifdef HAVE_GETCWD
        getcwd(buf, sizeof buf);
#else
        getwd(buf);
#endif
        len = strlen(buf);
        if (len >= sizeof buf) {
            rs_log_error("getwd overflowed in dcc_abspath()");
            exit(EXIT_FAILURE);
        }
        buf[len++] = '/';
    }
    if (path_len <= 0)
        path_len = strlen(path);
    if (path_len >= 2 && *path == '.' && path[1] == '/') {
	path += 2;
	path_len -= 2;
    }
    if (len + (unsigned)path_len >= sizeof buf) {
        rs_log_error("path overflowed in dcc_abspath()");
        exit(EXIT_FAILURE);
    }
    strncpy(buf + len, path, path_len);
    buf[len + path_len] = '\0';
    for (p = buf+len-(len > 0); (p = strstr(p, "/../")) != NULL; p = slash) {
	*p = '\0';
	if (!(slash = strrchr(buf, '/')))
	    slash = p;
	strcpy(slash, p+3);
    }
    return buf;
}
