/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
 * Copyright (C) 2003 by Apple Computer, Inc.
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


		/* I don't know that atheists should be considered as
		 * citizens, nor should they be considered patriots.
		 * This is one nation under God.
		 * -- Vice President, George H. W. Bush, 1987 */


/**
 * @file
 *
 * Daemon parent.  Accepts connections, forks, etc.
 *
 * @todo Quite soon we need load management.  Basically when we think
 * we're "too busy" we should stop accepting connections.  This could
 * be because of the load average, or because too many jobs are
 * running, or perhaps just because of a signal from the administrator
 * of this machine.  In that case we want to do a blocking wait() to
 * find out when the current jobs are done, or perhaps a sleep() if
 * we're waiting for the load average to go back down.  However, we
 * probably ought to always keep at least one job running so that we
 * can make progress through the queue.  If you don't want any work
 * done, you should kill the daemon altogether.
 **/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "exitcode.h"
#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "util.h"
#include "dopt.h"
#include "exec.h"
#include "tempfile.h"
#include "srvnet.h"
#include "indirect_server.h"
#include "indirect_util.h"
#include "zeroconf_reg.h"
#include "zeroconf_util.h"

#ifndef WAIT_ANY
#  define WAIT_ANY (-1)
#endif

static int dcc_start_child(int listen_fd, int);
static void dcc_parent_loop(int listen_fd) NORETURN;
static int dcc_serve_connection(int, int acc_fd);
void dcc_detach(void);
static void dcc_save_pid(pid_t);
void dcc_reap_kids(void);

static int nkids = 0;


/**
 * Be a standalone server, with responsibility for sockets and forking
 * children.  Puts the daemon in the background and detaches from the
 * controlling tty.
 **/
int dcc_standalone_server(void)
{
    int listen_fd;
    int n_cpus;
#if defined(DARWIN)
    char *zcTxtRecord;
#endif // DARWIN

    if ((listen_fd = open_socket_in(arg_port)) == -1)
        return EXIT_BIND_FAILED;

    set_cloexec_flag(listen_fd, 1);

    rs_log(RS_LOG_INFO|RS_LOG_NONAME,
           "distccd (version %s, built %s %s) listening on port %d",
           PACKAGE_VERSION, __DATE__, __TIME__, arg_port);

    if (dcc_ncpus(&n_cpus) == 0) 
        rs_log_info("%d CPU%s online", n_cpus, n_cpus == 1 ? "" : "s");

    dcc_catch_signals();

#if defined(DARWIN)
    zcTxtRecord = dcc_generate_txt_record();
#endif

    if (!opt_no_detach) {
        /* Don't go into the background until we're listening and
         * ready.  This is useful for testing -- when the daemon
         * detaches, we know we can go ahead and try to connect.  */
        dcc_detach();
    } else {
        dcc_save_pid(getpid());
    }
 
#if defined(DARWIN)
    // Spawn a thread to handle the ZeroConfig task for this daemon.
    dcc_register_for_zeroconfig(zcTxtRecord);
#endif

    dcc_parent_loop(listen_fd);		/* noreturn */
}



/**
 * @sa dcc_wait_child(), which is used by a process that wants to do a blocking
 * wait for some task like cpp or gcc.
 **/
void dcc_reap_kids(void)
{
    while (1) {
        int status;
        pid_t kid;

        kid = waitpid(WAIT_ANY, &status, WNOHANG);
        if (kid == 0) {
            break;
        } else if (kid != -1) {
            /* child exited */
            --nkids;
            rs_log_info("down to %d children", nkids);
            
            if (WIFSIGNALED(status)) {
                rs_log_error("child %d exited on signal %d",
                             (int) kid, WTERMSIG(status));
            } else if (WIFEXITED(status)) {
                rs_log_notice("child %d exited: exit status %d",
                              (int) kid, WEXITSTATUS(status));
            }
        } else if (errno == ECHILD) {
            /* No children left?  That's ok, we'll go back to waiting
             * for new connections. */
            break;          
        } else if (errno == EINTR) {
            /* If we got a SIGTERM or something, then on the next pass
             * through the loop we'll find no children done, and we'll
             * return to the top loop at which point we'll exit.  So
             * no special action is required here. */
            continue;       /* loop again */
        } else {
            rs_log_error("wait failed: %s", strerror(errno));
            dcc_exit(EXIT_DISTCC_FAILED);
        }
    }
}


/**
 * Main loop for the parent process.  Basically it does nothing but
 * wait around to be signalled, or for its children to die.  What a
 * life!
 **/
static void dcc_parent_loop(int listen_fd)
{
    while (1) {
        int acc_fd;

        rs_log_info("waiting to accept connection");
        
        acc_fd = accept(listen_fd, NULL, 0);
        if (acc_fd == -1 && errno == EINTR) {
            /* just go ahead and check for children */
        }  else if (acc_fd == -1) {
            rs_log_error("accept failed: %s", strerror(errno));
            dcc_exit(EXIT_CONNECT_FAILED);
        } else {
            dcc_serve_connection(listen_fd, acc_fd);
        }

        if (!opt_no_fork) {
            /* Don't call if in no-fork mode, because that would
             * incorrectly pick up compilers etc. */
            dcc_reap_kids();
        }
    }
}

/**
 * Called in the daemon parent when a connection is received.  Either
 * forks a new child, or handles it in the process (debug mode), as
 * appropriate.
 **/
static int dcc_serve_connection(int listen_fd, int acc_fd)
{
    int ret = 0;
    
    if (opt_no_fork) {
        ret = dcc_accept_job(acc_fd);
        dcc_cleanup_tempfiles();
    } else {
        dcc_start_child(listen_fd, acc_fd);
        ++nkids;
        rs_log_info("up to %d children", nkids);
    }
    if (close(acc_fd)) {          /* in parent */
        rs_log_error("failed to close accepted fd%d: %s", acc_fd,
                     strerror(errno));
    }

    return ret;
}

/**
 * Fork a child to handle an incoming job.  Not called when in
 * opt_no_fork mode.
 **/
static int dcc_start_child(int listen_fd, int accepted_fd)
{
    pid_t kid;
    int ret, close_ret;
    
    kid = fork();
    if (kid == 0) {
        close(listen_fd);
#if defined(DARWIN)
        dcc_support_indirection(&accepted_fd);
#endif // DARWIN
        ret = dcc_accept_job(accepted_fd);
        close_ret = dcc_close(accepted_fd);
        dcc_exit(ret ? ret : close_ret);
        /* doesn't return */
    } else if (kid == -1) {
        rs_log_error("fork failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}


/**
 * Save the pid of the child process into the pid file, if any.
 *
 * This is called from the parent so that we have the invariant that
 * the pid file exists before the parent exits, hich is useful for
 * test harnesses.  Otherwise, there is a race where the parent has
 * exited and they try to go ahead and read the child's pid, but it's
 * not there yet.
 **/
static void dcc_save_pid(pid_t pid)
{
    FILE *fp;
    
    if (!arg_pid_file)
        return;

    if (!(fp = fopen(arg_pid_file, "wt"))) {
        rs_log_error("failed to open pid file: %s: %s", arg_pid_file,
                     strerror(errno));
        return;
    }

    fprintf(fp, "%ld\n", (long) pid);

    if (fclose(fp) == -1) {
        rs_log_error("failed to close pid file: %s: %s", arg_pid_file,
                     strerror(errno));
        return;
    }

    atexit(dcc_remove_pid);
}


/**
 * Remove our pid file on exit.
 *
 * Must be reentrant -- called from signal handler.
 **/
void dcc_remove_pid(void)
{
    if (!arg_pid_file)
        return;

    if (unlink(arg_pid_file)) {
        rs_log_warning("failed to remove pid file %s: %s",
                       arg_pid_file, strerror(errno));
    }
}


/**
 * Become a daemon, discarding the controlling terminal.
 *
 * Borrowed from rsync.
 *
 * This function returns in the child, but not in the parent.
 **/
void dcc_detach(void)
{
    int i;
    pid_t pid;
    
    dcc_ignore_sighup();

    if ((pid = fork()) == -1) {
        rs_log_error("fork failed: %s", strerror(errno));
        exit(EXIT_DISTCC_FAILED);
    } else if (pid != 0) {
        /* In the parent.  This guy is about to go away so as to
         * detach from the controlling process, but first save the
         * child's pid. */
        dcc_save_pid(pid);
        _exit(0);
    }
    
    /* This is called in the detached child */

    /* detach from the terminal */
#ifdef HAVE_SETSID
        setsid();
#else
#ifdef TIOCNOTTY
    i = open("/dev/tty", O_RDWR);
    if (i >= 0) {
        ioctl(i, (int) TIOCNOTTY, (char *)0);      
        close(i);
    }
#endif /* TIOCNOTTY */
#endif
    /* make sure that stdin, stdout an stderr don't stuff things
       up (library functions, for example) */
    for (i=0;i<3;i++) {
        close(i); 
        open("/dev/null", O_RDWR);
    }

#if ! defined(DARWIN)
    /* If there's a lifetime limit on this server (for testing) then it needs
     * to apply after detaching as well. */
    dcc_set_lifetime();
#endif // ! DARWIN
}
