/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
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


			/* 18 Their bows also shall dash the young men
			 * to pieces; and they shall have no pity on
			 * the fruit of the womb; their eyes shall not
			 * spare children.
			 *		-- Isaiah 13 */

/**
 * @file
 *
 * Run compilers or preprocessors.
 *
 * Subtasks are always run in their own process groups.  (In this respect we
 * are a little like a job-control shell...)  This allows us to cleanly kill
 * the whole compiler if something goes wrong.
 *
 * @todo On Cygwin, fork() must be emulated and therefore will be
 * slow.  It would be faster to just use their spawn() call, rather
 * than fork/exec.
 **/



/**
 * This environment variable is set to guard against distcc
 * accidentally recursively invoking itself, thinking it's the real
 * compiler.
 **/
static const char dcc_safeguard_name[] = "_DISTCC_SAFEGUARD";
static char dcc_safeguard_set[] = "_DISTCC_SAFEGUARD=1";
static int dcc_safeguard_level;


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "util.h"
#include "exitcode.h"
#include "exec.h"
#include "lock.h"
#include "indirect_server.h"

/* You can't have core files on Windows. */
#ifndef WCOREDUMP
#  define WCOREDUMP(status) 0
#endif

void dcc_note_execution(const char *hostname, char **argv)
{
    char *astr;

    astr = dcc_argv_tostr(argv);
    rs_log(RS_LOG_INFO|RS_LOG_NONAME, "exec on %s: %s", hostname, astr);
    free(astr);
}


/**
 * Redirect stdin/out/err.  Filenames may be NULL to leave them untouched.
 *
 * This is called when running a job remotely, but *not* when running
 * it locally, because people might e.g. want cpp to read from stdin.
 **/
int dcc_redirect_fds(const char *stdin_file,
                     const char *stdout_file,
                     const char *stderr_file)
{
    int ret;
    
    if (stdin_file)
        if ((ret = dcc_redirect_fd(STDIN_FILENO, stdin_file, O_RDONLY)))
            return ret;
    
    if (stdout_file) {
        if ((ret = dcc_redirect_fd(STDOUT_FILENO, stdout_file,
                                   O_WRONLY | O_CREAT | O_TRUNC)))
            return ret;
    }
    
    if (stderr_file) {
        /* Open in append mode, because the server will dump its own error
         * messages into the compiler's error file.  */
        if ((ret = dcc_redirect_fd(STDERR_FILENO, stderr_file,
                                   O_WRONLY | O_CREAT | O_APPEND)))
            return ret;
    }

    return 0;
}


int dcc_remove_if_exists(const char *fname)
{
    if (unlink(fname) && errno != ENOENT) {
        rs_log_warning("failed to unlink %s: %s", fname,
                       strerror(errno));
        return -1;
    }
    return 0;
}


/**
 * Replace this program with another in the same process.
 *
 * Does not return, either execs the compiler in place, or exits with
 * a message.
 **/
void dcc_execvp(char **argv)
{
    execvp(argv[0], argv);
    
    /* shouldn't be reached */
    rs_log_error("failed to exec %s: %s", argv[0], strerror(errno));

    dcc_exit(EXIT_COMPILER_MISSING); /* a generalization, i know */
}


int dcc_recursion_safeguard(void)
{
    char *env = getenv(dcc_safeguard_name);

    if (env) {
	rs_trace("safeguard: %s", env);
	if (!(dcc_safeguard_level = atoi(env)))
	    dcc_safeguard_level = 1;
    }
    else
	dcc_safeguard_level = 0;
    rs_trace("safeguard level=%d", dcc_safeguard_level);

    return dcc_safeguard_level;
}



/**
 * Called inside the newly-spawned child process to execute a command.
 * Either executes it, or returns an appropriate error.
 *
 * This routine also takes a lock on localhost so that it's counted
 * against the process load.  That lock will go away when the process
 * exits.
 *
 * In this current version locks are taken without regard to load limitation
 * on the current machine.  The main impact of this is that cpp running on
 * localhost will cause jobs to be preferentially distributed away from
 * localhost, but it should never cause the machine to deadlock waiting for
 * localhost slots.
 *
 * @param what Type of process to be run here (cpp, cc, ...)
 *
 * @returns Exit code
 **/
static int dcc_inside_child(char **argv,
                            const char *stdin_file,
                            const char *stdout_file,
                            const char *stderr_file)
{
    int ret;

    if ((ret = dcc_ignore_sigpipe(0)))    /* set handler back to default */
        return ret;

    if (dcc_safeguard_level > 0)
	dcc_safeguard_set[sizeof dcc_safeguard_set-2] = dcc_safeguard_level+'1';
    rs_trace("setting safeguard: %s", dcc_safeguard_set);
    if ((putenv(strdup(dcc_safeguard_set)) == -1)) {
        rs_log_error("putenv failed");
        /* and continue */
    }

    /* do this last, so that any errors from previous operations are
     * visible */
    if ((ret = dcc_redirect_fds(stdin_file, stdout_file, stderr_file)))
        return ret;
    
    dcc_execvp(argv);
}


void dcc_setpgid(pid_t pid, pid_t pgid)
{
    if (setpgid(pid, pgid) == 0)
        rs_trace("setpgid(%ld, %ld)", (long) pid, (long) pgid);
    else
        rs_trace("setpgid(%ld, %ld) failed: %s",
                 (long) pid, (long) pgid, strerror(errno));
    
    /* Continue anyhow i guess.  Failure seems unlikely.  */
}


/**
 * Run @p argv in a child asynchronously.
 *
 * stdin, stdout and stderr are redirected as shown, unless those
 * filenames are NULL.
 *
 * Better server-side load limitation must still be organized.
 *
 * @warning When called on the daemon, where stdin/stdout may refer to random
 * network sockets, all of the standard file descriptors must be redirected!
 **/
int dcc_spawn_child(char **argv, pid_t *pidptr,
                    const char *stdin_file,
                    const char *stdout_file,
                    const char *stderr_file)
{
    pid_t pid;

    dcc_trace_argv("forking to execute", argv);
    
    pid = fork();
    if (pid == -1) {
        rs_log_error("failed to fork: (%d) %s", errno, strerror(errno));
        return -1;
    } else if (pid == 0) {
#if defined(DARWIN)
        // Close the child's copy of the parent's end of the indirection pipe pair.
        dcc_close_pipe_end_child();
#endif // DARWIN
        exit(dcc_inside_child(argv, stdin_file, stdout_file, stderr_file));
        /* !! NEVER RETURN FROM HERE !! */
    } else {
        *pidptr = pid;
        rs_trace("child started as pid%d", (int) pid);
#if defined(DARWIN)
        // Close the parent's copy of the child's end of the pipe pair.
        dcc_close_pipe_end_parent();
#endif // DARWIN
        return 0;
    }
}


void dcc_reset_signal(int whichsig)
{
    struct sigaction act_dfl;

    memset(&act_dfl, 0, sizeof act_dfl);
    act_dfl.sa_handler = SIG_DFL;
    sigaction(whichsig, &act_dfl, NULL);
    /* might be called from signal handler, therefore no IO to log a
     * message */
}


static int sys_wait4(pid_t pid, int *status, int options, struct rusage *rusage)
{
#ifdef HAVE_WAIT4
    return wait4(pid, status, options, rusage);
#elif HAVE_WAITPID
    /* Just doing getrusage(children) is not sufficient, because other
     * children may have exited previously. */
    memset(rusage, 0, sizeof *rusage);
    return waitpid(pid, status, options);
#else
#error Please port this
#endif
}


/**
 * Blocking wait for a child to exit.  This is used when waiting for
 * cpp, gcc, etc.
 *
 * This is not used by the daemon-parent; it has its own
 * implementation in dcc_reap_kids().  They could be unified, but the
 * parent only waits when it thinks a child has exited; the child
 * waits all the time.
 **/
int dcc_collect_child(pid_t pid, int *wait_status, long *u_us, long *s_us)
{
    struct rusage ru;
    pid_t ret_pid;
    
    while (1) {
        if ((ret_pid = sys_wait4(pid, wait_status, 0, &ru)) != -1) {
            /* This is not the main user-visible message, that comes from
             * critique_status(). */
            rs_trace("child %ld terminated with status %#x",
                     (long) ret_pid, *wait_status);
            *u_us = ru.ru_utime.tv_sec * 1000000 + ru.ru_utime.tv_usec;
            *s_us = ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
            return 0;
        } else if (errno == EINTR) {
            rs_trace("wait4 was interrupted; retrying");
            continue;
        } else {
            rs_log_error("sys_wait4(pid=%d) borked: %s", (int) pid, strerror(errno));
            return -1;
        }
    }
}


/**
 * Report child's resource usage in readable format.
 **/
int dcc_report_rusage(const char *command,
                      long utime_usec,
                      long stime_usec)
{
    rs_log_info("%s resource usage: %ld.%06lds user, %ld.%06lds system",
                command, utime_usec/1000000, utime_usec%1000000,
                stime_usec/1000000, stime_usec%1000000);
    return 0;
}


/**
 * Analyze and report on a command's exit code.
 *
 * @param command short human-readable description of the command (perhaps
 * argv[0])
 *
 * @returns 0 if the command succeeded; 128+SIGNAL if it stopped on a
 * signal; otherwise the command's exit code.
 **/
int dcc_critique_status(int s, const char *command,
                        const char *hostname)
{
    if (WIFSIGNALED(s)) {
        rs_log_error("%s on %s died with signal %d%s",
                     command, hostname,
                     WTERMSIG(s),
                     WCOREDUMP(s) ? " (core dumped)" : "");
        /* Unix convention is to return 128+signal when a subprocess crashes. */
        return 128 + WTERMSIG(s);
    } else if (WEXITSTATUS(s)) {
        /* Only a NOTICE because having compilers fail is not really
         * unexpected and we don't want to flood the client's error
         * stream. */
        rs_log_notice("%s on %s failed with exit code %d",
                     command, hostname, WEXITSTATUS(s));
        return WEXITSTATUS(s);
    } else {
        rs_log(RS_LOG_INFO|RS_LOG_NONAME,
               "%s on %s completed ok", command, hostname);
        return 0;
    }
}

