/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
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


			/* 4: The noise of a multitude in the
			 * mountains, like as of a great people; a
			 * tumultuous noise of the kingdoms of nations
			 * gathered together: the LORD of hosts
			 * mustereth the host of the battle.
			 *		-- Isaiah 13 */



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

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "rpc.h"
#include "exitcode.h"
#include "util.h"
#include "clinet.h"
#include "hosts.h"
#include "bulk.h"
#include "tempfile.h"
#include "strip.h"
#include "implicit.h"
#include "exec.h"
#include "where.h"
#include "lock.h"
#include "cpp.h"
#include "clirpc.h"


/* Name of this program, for trace.c */
const char *rs_program_name = "distcc";


/**
 * @file
 *
 * Entry point for the distcc client.
 *
 * In most parts of this program, functions return 0 for success and something
 * from exitcode.h for failure.  However it is not completely consistent.
 *
 * @todo Make absolutely sure that if we fail, the .o file is removed.
 * Perhaps it would be better to receive to a temporary file and then
 * rename into place?  On the other hand, gcc seems to just write
 * directly, and if we fail or crash then Make ought to know not to
 * use it.
 *
 * @todo Count the preprocessor, and any compilations run locally, against the
 * load of localhost.  In doing this, make sure that we cannot deadlock
 * against a load limit, by having a case where we need to hold one lock and
 * take another to make progress.  I don't think there should be any such case
 * -- we can release the cpp lock before starting the main compiler.
 *
 * @todo If we have produced a .i file and need to fall back to running
 * locally then use that rather than the original source.
 **/


static void dcc_show_usage(void)
{
    dcc_show_version("distcc");
    dcc_show_copyright();
    printf(
"Usage:\n"
"   distcc [COMPILER] [compile options] -o OBJECT -c SOURCE\n"
"   distcc --help\n"
"\n"
"Options:\n"
"   COMPILER                   defaults to \"cc\"\n"
"   --help                     explain usage and exit\n"
"   --version                  show version and exit\n"
"\n"
"Environment variables:\n"
"   DISTCC_HOSTS=\"HOST ...\"\n"
"            list of volunteer hosts, should include localhost\n"
"   DISTCC_VERBOSE=1           give debug messages\n"
"   DISTCC_LOG=LOGFILE         send messages here, not stderr\n"
"   DISTCC_TCP_CORK=0          disable TCP corks\n"
"\n"
"Host specifications:\n"
"   localhost                  run in place\n"
"   HOST                       TCP connection, port %d\n"
"   HOST:PORT                  TCP connection, specified port\n"
"\n"
"distcc distributes compilation jobs across volunteer machines running\n"
"distccd.  Jobs that cannot be distributed, such as linking or \n"
"preprocessing are run locally.  distcc should be used with make's -jN\n"
"option to execute in parallel on several machines.\n",
    DISTCC_DEFAULT_PORT);
}



/**
 * Pass a compilation across the network.
 *
 * When this function is called, the preprocessor has already been
 * started in the background.  It may have already completed, or it
 * may still be running.  The goal is that preprocessing will overlap
 * with setting up the network connection, which may take some time
 * but little CPU.
 *
 * If this function fails, compilation will be retried on the local
 * machine.
 *
 * @param argv Compiler command to run.
 *
 * @param cpp_fname Filename of preprocessed source.  May not be complete yet,
 * depending on @p cpp_pid.
 *
 * @param output_fname File that the object code should be delivered to.
 * 
 * @param cpp_pid If nonzero, the pid of the preprocessor.  Must be
 * allowed to complete before we send the input file.
 *
 * @param host Definition of host to send this job to.
 *
 * @param status on return contains the wait-status of the remote
 * compiler.
 *
 * @returns 0 on success, otherwise error.  Returning nonzero does not
 * necessarily imply the remote compiler itself succeeded, only that
 * there were no communications problems. 
 **/
static int dcc_compile_remote(char **argv, 
                              char *cpp_fname, char *output_fname,
                              pid_t cpp_pid,
                              const struct dcc_hostdef *host,
                              int *status,
                              int xmit_lock_fd)
{
    int net_fd;
    int ret;

    *status = 0;

    dcc_note_execution(host->hostname, argv);
    if ((ret = dcc_open_socket_out(host->hostname, host->port, &net_fd)) != 0)
        return ret;

    /* This waits for cpp and puts its status in *status.  If cpp failed, then
     * the connection will have been dropped and we need not bother trying to
     * get any response from the server. */
    ret = dcc_send_job_corked(net_fd, argv, cpp_pid, status, cpp_fname);

    dcc_unlock(xmit_lock_fd);
    
    if (ret == 0 && *status == 0) {
        ret = dcc_retrieve_results(net_fd, status, output_fname);
    }

    /* Close socket so that the server can terminate, rather than
     * making it wait until we've finished our work. */
    dcc_close(net_fd);

    return ret;
}


/**
 * Invoke a compiler locally.  This is, obviously, the alternative to
 * dcc_compile_remote().
 *
 * The server does basically the same thing, but it doesn't call this
 * routine because it wants to overlap execution of the compiler with
 * copying the input from the network.
 *
 * This routine used to exec() the compiler in place of distcc.  That
 * is slightly more efficient, because it avoids the need to create,
 * schedule, etc another process.  The problem is that in that case we
 * can't clean up our temporary files, and (not so important) we can't
 * log our resource usage.
 *
 * This is called with a lock on localhost already held.
 **/
static int dcc_compile_local(char *argv[])
{
    pid_t pid;
    int ret;
    int status;
    long u_us, s_us;
    const char *buildhost = "localhost";

    dcc_note_execution(buildhost, argv);

    /* We don't do any redirection of file descriptors when running locally,
     * so if for example cpp is being used in a pipeline we should be fine. */
    if ((ret = dcc_spawn_child(argv, &pid, NULL, NULL, NULL)) != 0)
        return ret;

    if ((ret = dcc_collect_child(pid, &status, &u_us, &s_us)))
        return ret;

    dcc_report_rusage(argv[0], u_us, s_us);
    return dcc_critique_status(status, "compile", dcc_gethostname());
}


/**
 * Execute the commands in argv remotely or locally as appropriate.
 *
 * We may need to run cpp locally; we can do that in the background
 * while trying to open a remote connection.
 *
 * This function is slightly inefficient when it falls back to running
 * gcc locally, because cpp may be run twice.  Perhaps we could adjust
 * the command line to pass in the .i file.  On the other hand, if
 * something has gone wrong, we should probably take the most
 * conservative course and run the command unaltered.  It should not
 * be a big performance problem because this should occur only rarely.
 *
 * @param argv Command to execute.  Does not include 0='distcc'.
 * Treated as read-only, because it is a pointer to the program's real
 * argv.
 *
 * @param status On return, contains the waitstatus of the compiler or
 * preprocessor.  This function can succeed (in running the compiler) even if
 * the compiler itself fails.  If either the compiler or preprocessor fails,
 * @p status is guaranteed to hold a failure value.
 **/
static int dcc_build_somewhere(char *argv[], int sg_level, int *status)
{
    char *input_fname, *output_fname, *cpp_fname;
    char **argv_stripped;
    pid_t cpp_pid = 0;
    int xmit_lock_fd, cpu_lock_fd;
    int ret;
    struct dcc_hostdef *host = NULL;

    if (sg_level)
        goto run_local;

    /* TODO: Perhaps tidy up these gotos. */

    if (dcc_scan_args(argv, &input_fname, &output_fname, &argv) != 0) {
        /* we need to scan the arguments even if we already know it's
         * local, so that we can pick up distcc client options. */
        goto lock_local;
    }

    if ((ret = dcc_pick_host_from_env(&host, &xmit_lock_fd, &cpu_lock_fd)) != 0) {
        /* Doesn't happen at the moment: all failures are masked by
           returning localhost. */
        goto fallback;
    }

    if (host->mode == DCC_MODE_LOCAL)
        goto run_local;
    
    if ((ret = dcc_cpp_maybe(argv, input_fname, &cpp_fname, &cpp_pid) != 0))
        goto fallback;

    if ((ret = dcc_strip_local_args(argv, &argv_stripped)))
        goto fallback;

    if ((ret = dcc_compile_remote(argv_stripped, cpp_fname, output_fname,
                                  cpp_pid, host, status, xmit_lock_fd)) != 0) {
        /* Returns zero if we successfully ran the compiler, even if
         * the compiler itself bombed out. */
        goto fallback;
    }

    dcc_unlock(cpu_lock_fd);
        
    return dcc_critique_status(*status, "compile", host->hostname);

  fallback:
    /* "You guys are so lazy!  Do I have to do all the work myself??" */
    rs_log_warning("failed to distribute to \"%s\", running locally instead",
                   host && host->hostname ? host->hostname : "(unknown)");

  lock_local:

    dcc_lock_local(&xmit_lock_fd, &cpu_lock_fd);

  run_local:
    return dcc_compile_local(argv);
}


/**
 * Send trace to append to the file specified by DISTCC_LOG.  If
 * that's something you didn't want to write to, tough.
 *
 * The exact setting of log level is a little strange, but for a good
 * reason: if you ask for verbose, you get everything.  Otherwise, if
 * you set a file, you get INFO and above.  Otherwise, you only get
 * WARNING messages.  In practice this seems to be a nice balance.
 **/
static void dcc_set_trace_from_env(void)
{
    const char *logfile;

    if ((logfile = getenv("DISTCC_LOG")) && logfile[0]) {
        int fd;

        rs_trace_set_level(RS_LOG_INFO);

        fd = open(logfile, O_WRONLY|O_APPEND|O_CREAT, 0666);
        if (fd == -1) {
            /* use stderr instead */
            int save_errno = errno;
            
            rs_trace_set_level(RS_LOG_WARNING);
            rs_add_logger(rs_logger_file, RS_LOG_DEBUG, NULL, STDERR_FILENO);

            rs_log_error("failed to open logfile %s: %s",
                         logfile, strerror(save_errno));
        } else {
            rs_add_logger(rs_logger_file, RS_LOG_DEBUG, NULL, fd);
            rs_trace_set_level(RS_LOG_INFO);
        }
    } else {
        rs_trace_set_level(RS_LOG_WARNING);
        rs_add_logger(rs_logger_file, RS_LOG_DEBUG, NULL, STDERR_FILENO);
    }

    if (dcc_getenv_bool("DISTCC_VERBOSE", 0)) {
        rs_trace_set_level(RS_LOG_DEBUG);
    }
}


static int dcc_support_masquerade(char *argv[], char *progname)
{
    char *envpath, *findpath, *buf, *p, *n;
    int findlen, len;

    /* right now, we're only playing with PATH... but
     * wouldn't it be interesting to look at CHOST, too? */
    if (!(envpath = getenv("PATH")))
        return 0;

    buf = malloc(strlen(envpath)+strlen(progname)+5+1);
    if (!buf) {
        rs_log_error("failed to allocate buffer for new PATH");
        exit(EXIT_FAILURE);
    }

    /* Filter PATH to contain only the part that is past our dir.
     * If we were called explicitly, find the named dir on the PATH. */
    if (progname != argv[0]) {
        findpath = dcc_abspath(argv[0], progname - argv[0] - 1);
        findlen = strlen(findpath);
    } else {
        findpath = NULL;
        findlen = 0;
    }

    for (n = p = envpath; *n; p = n) {
        n = strchr(p, ':');
        if (n)
            len = n++ - p;
        else {
            len = strlen(p);
            n = p + len;
        }

        if (findpath) {
            if (len != findlen || strncmp(p, findpath, findlen) != 0)
                continue;
        }
        else {
            strncpy(buf, p, len);
            sprintf(buf + len, "/%s", progname);
            if (access(buf, X_OK) != 0)
                continue;
        }
        /* Set p to the part of the path past our match. */
        p = n;
        break;
    }

    if (*p != '\0') {
        sprintf(buf, "PATH=%s", p);
        rs_trace("setting %s", buf);
        if (putenv(buf) == -1) {
            rs_log_error("putenv PATH failed");
            exit(1);
        }
        /* We must leave "buf" allocated for the environment's sake. */
        return 1;
    }

    rs_trace("not modifying PATH");
    free(buf);

    return 0;
}


/**
 * distcc client entry point.
 *
 * This is typically called by make in place of the real compiler.
 *
 * Performs basic setup and checks for distcc arguments, and then kicks of
 * dcc_build_somewhere().
 **/
int main(int argc, char **argv)
{
    int status, sg_level, tweaked_path = 0;
    char **compiler_args, *progname;

    atexit(dcc_cleanup_tempfiles);

    dcc_set_trace_from_env();

    if ((progname = strrchr(argv[0], '/')) != NULL)
        progname++;
    else
        progname = argv[0];

    sg_level = dcc_recursion_safeguard();

    if (strstr(progname, "distcc") != NULL) {
        if (argc <= 1 || !strcmp(argv[1], "--help")) {
            dcc_show_usage();
            exit(0);
        }
        if (!strcmp(argv[1], "--version")) {
            dcc_show_version("distcc");
            exit(0);
        }
        dcc_find_compiler(argv, &compiler_args);
    } else {
        tweaked_path = dcc_support_masquerade(argv, progname);
        dcc_shallowcopy_argv(argv, &compiler_args, 0);
        compiler_args[0] = progname;
    }

    if (sg_level - tweaked_path > 0) {
        rs_log_crit("distcc seems to have invoked itself recursively!");
        dcc_exit(EXIT_RECURSION);
    }

    dcc_exit(dcc_build_somewhere(compiler_args, sg_level, &status));
}
