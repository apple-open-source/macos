/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 1996-2001 by Andrew Tridgell <tridge@samba.org>
 * Copyright (C) 1996 by Paul Mackerras
 * Copyright (C) 2001-2003 by Martin Pool <mbp@samba.org>
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


/**
 * @file
 * @brief Open a connection a server over ssh or something similar.
 **/


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "exitcode.h"
#include "util.h"
#include "exec.h"
#include "snprintf.h"

/* work out what fcntl flag to use for non-blocking */
#ifdef O_NONBLOCK
# define NONBLOCK_FLAG O_NONBLOCK
#elif defined(SYSV)
# define NONBLOCK_FLAG O_NDELAY
#else
# define NONBLOCK_FLAG FNDELAY
#endif



const char *dcc_default_ssh = "ssh";


/**
 * Set a fd into blocking mode
 **/
static void set_blocking(int fd)
{
    int val;

    if ((val = fcntl(fd, F_GETFL, 0)) == -1)
        return;
    if (val & NONBLOCK_FLAG) {
        val &= ~NONBLOCK_FLAG;
        fcntl(fd, F_SETFL, val);
    }
}


/**
 * Set a fd into nonblocking mode
 **/
static void set_nonblocking(int fd)
{
    int val;

    if ((val = fcntl(fd, F_GETFL, 0)) == -1)
        return;
    if (!(val & NONBLOCK_FLAG)) {
        val |= NONBLOCK_FLAG;
        fcntl(fd, F_SETFL, val);
    }
}



/**
 * Create a file descriptor pair - like pipe() but use socketpair if
 * possible (because of blocking issues on pipes).
 * 
 * Always set non-blocking.
 */
static int fd_pair(int fd[2])
{
    int ret;

#if HAVE_SOCKETPAIR
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
#else
    ret = pipe(fd);
#endif

    if (ret == 0) {
        set_nonblocking(fd[0]);
        set_nonblocking(fd[1]);
    }

    return ret;
}


/**
 * Create a child connected to use on stdin/stdout.
 *
 * This is derived from CVS code 
 * 
 * Note that in the child STDIN is set to blocking and STDOUT
 * is set to non-blocking. This is necessary as rsh relies on stdin being blocking
 *  and ssh relies on stdout being non-blocking
 *
 * If blocking_io is set then use blocking io on both fds. That can be
 * used to cope with badly broken rsh implementations like the one on
 * Solaris.
 **/
static int piped_child(char **argv, int *f_in, int *f_out,
                       pid_t * child_pid)
{
    pid_t pid;
    int to_child_pipe[2];
    int from_child_pipe[2];

    dcc_trace_argv("execute", argv);

    if (fd_pair(to_child_pipe) < 0 || fd_pair(from_child_pipe) < 0) {
        rs_log_error("fd_pair: %s", strerror(errno));
        return EXIT_IO_ERROR;
    }

    *child_pid = pid = fork();
    if (pid == -1) {
        rs_log_error("fork: %s", strerror(errno));
        return EXIT_IO_ERROR;
    }

    if (pid == 0) {
        if (dup2(to_child_pipe[0], STDIN_FILENO) < 0 ||
            close(to_child_pipe[1]) < 0 ||
            close(from_child_pipe[0]) < 0 ||
            dup2(from_child_pipe[1], STDOUT_FILENO) < 0) {
            rs_log_error("dup/close: %s", strerror(errno));
            return EXIT_IO_ERROR;
        }
        if (to_child_pipe[0] != STDIN_FILENO)
            close(to_child_pipe[0]);
        if (from_child_pipe[1] != STDOUT_FILENO)
            close(from_child_pipe[1]);
        set_blocking(STDIN_FILENO);
        if (blocking_io) {
            set_blocking(STDOUT_FILENO);
        }
        execvp(argv[0], argv);
        rs_log_error("failed to exec %s: %s", argv[0], strerror(errno));
        return EXIT_IO_ERROR;
    }

    if (close(from_child_pipe[1]) < 0 || close(to_child_pipe[0]) < 0) {
        rs_log_error("failed to close: %s", strerror(errno));
        return EXIT_IO_ERROR;
    }

    *f_in = from_child_pipe[0];
    *f_out = to_child_pipe[1];

    return 0;
}



/**
 * Open a connection to a remote machine over ssh.
 *
 * Based on code in rsync, but rewritten.
 **/
int dcc_ssh_connect(char *ssh_cmd, char *machine, char *user, char *path,
                    int *f_in, int *f_out, pid_t ssh_pid)
{
    int i, argc = 0;
    pid_t ret;
    char *tok, *dir = NULL;
    char *full_cmd;

    if (!ssh_cmd)
        ssh_cmd = getenv("DISTCC_SSH");
    if (!ssh_cmd)
        ssh_cmd = dcc_default_ssh;

    full_cmd =
        asprintf("%s %s %s --daemon", ssh_cmd, user_at_host, dccd_cmd);

    if ((blocking_io == -1) && (strcmp(cmd, RSYNC_RSH) == 0))
        blocking_io = 1;

    ret = dcc_piped_child(full_cmd, f_in, f_out);

    return ret;
}
