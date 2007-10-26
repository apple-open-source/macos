/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
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



#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/time.h>

#include "distcc.h"
#include "trace.h"
#include "exitcode.h"
#include "util.h"
#include "hosts.h"
#include "bulk.h"
#include "implicit.h"
#include "exec.h"
#include "where.h"
#include "lock.h"
#include "timeval.h"
#include "compile.h"


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
static int dcc_compile_local(char *argv[],
                             char *input_name)
{
    pid_t pid;
    int ret;
    int status;

    dcc_note_execution(dcc_hostdef_local, argv);
    dcc_note_state(DCC_PHASE_COMPILE, input_name, "localhost");

    /* We don't do any redirection of file descriptors when running locally,
     * so if for example cpp is being used in a pipeline we should be fine. */
    if ((ret = dcc_spawn_child(argv, &pid, NULL, NULL, NULL, NULL)) != 0)
        return ret;

    if ((ret = dcc_collect_child("cc", pid, &status)))
        return ret;

    return dcc_critique_status(status, "compile", input_name,
                               dcc_hostdef_local, 1);
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
static int
dcc_build_somewhere(char *argv[],
                    int sg_level,
                    int *status)
{
    char *input_fname = NULL, *output_fname, *cpp_fname;
    char **argv_stripped;
    pid_t cpp_pid = 0;
    int cpu_lock_fd;
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

#if 0
    /* turned off because we never spend long in this state. */
    dcc_note_state(DCC_PHASE_STARTUP, input_fname, NULL);
#endif

    dcc_get_cpp_lock();
    
    if ((ret = dcc_cpp_maybe(argv, input_fname, &cpp_fname, &cpp_pid) != 0))
        goto fallback;

    if ((ret = dcc_strip_local_args(argv, &argv_stripped)))
        goto fallback;

    /* FIXME: argv_stripped is leaked. */

    if ((ret = dcc_pick_host_from_list(&host, &cpu_lock_fd)) != 0) {
        /* Doesn't happen at the moment: all failures are masked by
        returning localhost. */
        goto fallback;
    }
    
    if (host->mode == DCC_MODE_LOCAL)
        /* We picked localhost and already have a lock on it so no
        * need to lock it now. */
        goto run_local;
    
    if ((ret = dcc_compile_remote(argv_stripped,
                                  input_fname,
                                  cpp_fname,
                                  output_fname,
                                  cpp_pid, host, status)) != 0) {
        /* Returns zero if we successfully ran the compiler, even if
         * the compiler itself bombed out. */
        goto fallback;
    }

    dcc_enjoyed_host(host);

    if (cpu_lock_fd != -1) {
        dcc_unlock(cpu_lock_fd);
        cpu_lock_fd = -1;
    }

    ret = dcc_critique_status(*status, "compile", input_fname, host, 1);
    if (ret < 128)
        /* either worked, or remote compile just simply failed,
         * e.g. with syntax error.  don't bother retrying */
        return ret;

fallback:
    if (host)
        dcc_disliked_host(host);

    if (!dcc_getenv_bool("DISTCC_FALLBACK", 1)) {
        rs_log_warning("failed to distribute and fallbacks are disabled");
        ret = ret;
    } else {
        
        /* "You guys are so lazy!  Do I have to do all the work myself??" */
        if (host) {
            rs_log(RS_LOG_WARNING|RS_LOG_NONAME,
                   "failed to distribute %s to %s, running locally instead",
                   input_fname ? input_fname : "(unknown)",
                   host->hostdef_string);
        } else {
            rs_log_warning("failed to distribute, running locally instead");
        }
        
lock_local:
        dcc_lock_local(&cpu_lock_fd);
        
run_local:
        ret = dcc_compile_local(argv, input_fname);
    }
    if (cpu_lock_fd != -1) {
        dcc_unlock(cpu_lock_fd);
        cpu_lock_fd = -1;
    }
    return ret;    
}


int dcc_build_somewhere_timed(char *argv[],
                              int sg_level,
                              int *status)
{
    struct timeval before, after, delta;
    int ret;

    if (gettimeofday(&before, NULL))
        rs_log_warning("gettimeofday failed");

    ret = dcc_build_somewhere(argv, sg_level, status);

    if (gettimeofday(&after, NULL)) {
        rs_log_warning("gettimeofday failed");
    } else {
        /* TODO: Show rate based on cpp size?  Is that meaningful? */
        timeval_subtract(&delta, &after, &before);

        rs_log(RS_LOG_INFO|RS_LOG_NONAME,
               "elapsed compilation time %ld.%06lds",
               delta.tv_sec, delta.tv_usec);
    }

    return ret;
}


