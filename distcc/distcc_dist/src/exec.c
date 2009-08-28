/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003, 2004 by Martin Pool <mbp@samba.org>
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
 * The whole server is run in a separate process group and normally in a
 * separate session.  (It is not a separate session in --no-detach debug
 * mode.)  This allows us to cleanly kill off all children and all compilers
 * when the parent is terminated.
 *
 * @todo On Cygwin, fork() must be emulated and therefore will be
 * slow.  It would be faster to just use their spawn() call, rather
 * than fork/exec.
 **/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
#include "util.h"
#include "exitcode.h"
#include "exec.h"
#include "lock.h"
#include "hosts.h"
#include "versinfo.h"

static void dcc_inside_child(char **argv,
                             const char *stdin_file,
                             const char *stdout_file,
                             const char *stderr_file) NORETURN;


static void dcc_execvp(char **argv) NORETURN;


pid_t dcc_retry_fork()
{
    pid_t pid;
    int sleepCount;
    pid = fork();
    if (pid == -1 && errno == EAGAIN) {
        for (sleepCount = 1; sleepCount < 10 && pid == -1 && errno == EAGAIN; sleepCount++) {
            rs_log_warning("failed to fork, retry in %d seconds: %s", sleepCount, strerror(errno));
            sleep(sleepCount);
            pid = fork();
        }
    }
    if (pid == -1)
        rs_log_error("failed to fork, giving up: %s", strerror(errno));
    return pid;
}

void dcc_note_execution(struct dcc_hostdef *host, char **argv)
{
    char *astr;

    astr = dcc_argv_tostr(argv);
    rs_log(RS_LOG_INFO|RS_LOG_NONAME, "exec on %s: %s",
           host->hostdef_string, astr);
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


/**
 * Replace this program with another in the same process.
 *
 * Does not return, either execs the compiler in place, or exits with
 * a message.
 **/
static void dcc_execvp(char **argv)
{
    char *slash;
    
    execvp(argv[0], argv);

    /* If we're still running, the program was not found on the path.  One
     * thing that might have happened here is that the client sent an absolute
     * compiler path, but the compiler's located somewhere else on the server.
     * In the absence of anything better to do, we search the path for its
     * basename.
     *
     * Actually this code is called on both the client and server, which might
     * cause unintnded behaviour in contrived cases, like giving a full path
     * to a file that doesn't exist.  I don't think that's a problem. */

    slash = strrchr(argv[0], '/');
    if (slash)
        execvp(slash + 1, argv);
    
    /* shouldn't be reached */
    rs_log_error("failed to exec %s: %s", argv[0], strerror(errno));

    dcc_exit(EXIT_COMPILER_MISSING); /* a generalization, i know */
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
 **/
static void dcc_inside_child(char **argv,
                             const char *stdin_file,
                             const char *stdout_file,
                             const char *stderr_file) 
{
    int ret;
    
    if ((ret = dcc_ignore_sigpipe(0)))
        goto fail;              /* set handler back to default */

    /* Ignore failure */
    dcc_increment_safeguard();

    /* do this last, so that any errors from previous operations are
     * visible */
    if ((ret = dcc_redirect_fds(stdin_file, stdout_file, stderr_file)))
        goto fail;
    
    dcc_execvp(argv);

    ret = EXIT_DISTCC_FAILED;

    fail:
    dcc_exit(ret);
}


int dcc_new_pgrp(void)
{
    /* If we're a session group leader, then we are not able to call
     * setpgid().  However, setsid will implicitly have put us into a new
     * process group, so we don't have to do anything. */

    /* Does everyone have getpgrp()?  It's in POSIX.1.  We used to call
     * getpgid(0), but that is not available on BSD/OS. */
    if (getpgrp() == getpid()) {
        rs_trace("already a process group leader");
        return 0;
    }
    
    if (setpgid(0, 0) == 0) {
        rs_trace("entered process group");
        return 0;
    } else {
        rs_trace("setpgid(0, 0) failed: %s", strerror(errno));
        return EXIT_DISTCC_FAILED;
    }
}


/**
 * Run @p argv in a child asynchronously.
 *
 * stdin, stdout and stderr are redirected as shown, unless those
 * filenames are NULL.  In that case they are left alone.
 *
 * @warning When called on the daemon, where stdin/stdout may refer to random
 * network sockets, all of the standard file descriptors must be redirected!
 **/
int dcc_spawn_child(char **argv, pid_t *pidptr,
                    const char *stdin_file,
                    const char *stdout_file,
                    const char *stderr_file,
                    dcc_indirection *indirect)
{
    pid_t pid;

    dcc_trace_argv("forking to execute", argv);
    
    if (indirect)
        dcc_prepare_indirect(indirect);
    char *compilerPath = dcc_get_allowed_compiler_for_path(argv[0]);
    
    // Bail out if argv[0] isn't an allowed compiler. We should have already set argv[0] to the allowed compiler for the originally-requested compiler path if possible.
    if (!compilerPath || strcmp(compilerPath, argv[0]) != 0) {
        rs_log_error("attempt to use unknown compiler aborted: %s", argv[0]);
        if (indirect)
            // FIXME: need to check result;
            dcc_indirect_parent(indirect);
        return EXIT_DISTCC_FAILED;
    }
    pid = dcc_retry_fork();
    if (pid == -1) {
        rs_log_error("failed to fork: %s", strerror(errno));
        return EXIT_OUT_OF_MEMORY; /* probably */
    } else if (pid == 0) {
        if (indirect)
            dcc_indirect_child(indirect);
        dcc_inside_child(argv, stdin_file, stdout_file, stderr_file);
        /* !! NEVER RETURN FROM HERE !! */
    } else {
        *pidptr = pid;
        rs_trace("child started as pid%d", (int) pid);
        if (indirect)
            // FIXME: need to check result;
            dcc_indirect_parent(indirect);
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
int dcc_collect_child(const char *what, pid_t pid,
                      int *wait_status)
{
    struct rusage ru;
    pid_t ret_pid;
    
    while (1) {
        if ((ret_pid = sys_wait4(pid, wait_status, 0, &ru)) != -1) {
            /* This is not the main user-visible message, that comes from
             * critique_status(). */
            rs_trace("%s child %ld terminated with status %#x",
                     what, (long) ret_pid, *wait_status);

            rs_log_info("%s times: user %ld.%06lds, system %ld.%06lds, "
                        "%ld minflt, %ld majflt",
                        what,
                        ru.ru_utime.tv_sec, ru.ru_utime.tv_usec,
                        ru.ru_stime.tv_sec, ru.ru_stime.tv_usec,
                        ru.ru_minflt, ru.ru_majflt);

            return 0;
        } else if (errno == EINTR) {
            rs_trace("wait4 was interrupted; retrying");
            continue;
        } else {
            rs_log_error("sys_wait4(pid=%d) borked: %s", (int) pid, strerror(errno));
            return EXIT_DISTCC_FAILED;
        }
    }
}



/**
 * Analyze and report to the user on a command's exit code.  
 *
 * @param command short human-readable description of the command (perhaps
 * argv[0])
 *
 * @returns 0 if the command succeeded; 128+SIGNAL if it stopped on a
 * signal; otherwise the command's exit code.
 **/
int dcc_critique_status(int status,
                        const char *command,
                        const char *input_fname,
                        struct dcc_hostdef *host,
                        int verbose)
{
    int logmode;

    /* verbose mode is only used for executions that the user is likely to
     * particularly need to know about */
    if (verbose)
        logmode = RS_LOG_ERR | RS_LOG_NONAME;
    else
        logmode = RS_LOG_INFO | RS_LOG_NONAME;
    
    if (WIFSIGNALED(status)) {
#ifdef HAVE_STRSIGNAL
        rs_log(logmode,
               "%s %s on %s:%s %s",
               command, input_fname, host->hostdef_string,
               strsignal(WTERMSIG(status)),
               WCOREDUMP(status) ? " (core dumped)" : "");
#else
        rs_log(logmode,
               "%s %s on %s terminated by signal %d%s",
               command, input_fname, host->hostdef_string,
               WTERMSIG(status),
               WCOREDUMP(status) ? " (core dumped)" : "");
#endif
        /* Unix convention is to return 128+signal when a subprocess crashes. */
        return 128 + WTERMSIG(status);
    } else if (WEXITSTATUS(status) == 1) {
        /* Normal failure gives exit code 1, so handle that specially */
        rs_log(logmode, "%s %s on %s failed", command, input_fname, host->hostdef_string);
        return WEXITSTATUS(status);
    } else if (WEXITSTATUS(status)) {
        /* This is a tough call; we don't really want to clutter the client's
         * error stream, but if we don't say where the compilation failed then
         * people may find it hard to work things out. */

        rs_log(logmode,
               "%s %s on %s failed with exit code %d",
               command, input_fname, host->hostdef_string, WEXITSTATUS(status));
        return WEXITSTATUS(status);
    } else {
        rs_log(RS_LOG_INFO|RS_LOG_NONAME,
               "%s %s on %s completed ok", command, input_fname, host->hostdef_string);
        return 0;
    }
}

