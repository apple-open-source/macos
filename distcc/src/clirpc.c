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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "exec.h"
#include "rpc.h"
#include "exitcode.h"
#include "util.h"
#include "clinet.h"
#include "bulk.h"
#include "clirpc.h"


/**
 * @file
 *
 * @brief Client-side RPC functions.
 **/

/*
 * Transmit header for whole request.
 */
static int dcc_x_req_header(int fd)
{
    return dcc_x_token_int(fd, "DIST", PROTO_VER);
}



/**
 * Transmit an argv array.
 **/
static int dcc_x_argv(int fd, char **argv)
{
    int i;
    int ret;
    int argc;
    
    argc = dcc_argv_len(argv);
    
    if (dcc_x_token_int(fd, "ARGC", argc))
        return -1;
    
    for (i = 0; i < argc; i++) {
        size_t len;

        len = strlen(argv[i]);
        if ((ret = dcc_x_token_int(fd, "ARGV", (unsigned) len)))
            return ret;
        if ((ret = dcc_writex(fd, argv[i], len)))
            return ret;
    }

    return 0;
}


static int dcc_send_job(int net_fd,
                        char **argv,
                        pid_t cpp_pid,
                        int *status,
                        const char *cpp_fname)
{
    long stime_usec, utime_usec;
    int ret;

    if ((ret = dcc_x_req_header(net_fd))
        || (ret = dcc_x_argv(net_fd, argv)))
        return ret;

    if (cpp_pid) {
        /* Wait for cpp to finish (if not already done), check the
         * result, then send the .i file */
        
        if ((ret = dcc_collect_child(cpp_pid, status, &utime_usec, &stime_usec))
            || (ret = dcc_report_rusage("cpp", utime_usec, stime_usec)))
            return ret;

        /* Although cpp failed, there is no need to try running the command
         * locally, because we'd presumably get the same result.  Therefore
         * critique the command and log a message and return an indication
         * that compilation is complete. */
        if (dcc_critique_status(*status, "cpp", dcc_gethostname()))
            return 0;
    }

    /* TODO: Add test case for this marginal case: cpp returns 0, but does not
     * create an output file. */
    if (access(cpp_fname, R_OK) != 0) {
        rs_log_error("can't read cpp output \"%s\": %s",
                     cpp_fname, strerror(errno));
        return EXIT_IO_ERROR;
    }

    if ((ret = dcc_x_file_timed(net_fd, cpp_fname, "DOTI", NULL)))
        return ret;

    rs_trace("client finished sending request to server");

    return 0;
}


/**
 * Send job with socket corked.
 *
 * Make sure to uncork on return.  For success, this is necessary to
 * make sure the whole request gets pushed to the other side quickly.
 *
 * For failure, we need to uncork the socket to work around a bug in
 * Linux 2.2 that causes the socket to get stuck in FIN_WAIT1 if it is
 * closed while corked.  Unfortunately, this is not a full solution:
 * if distcc crashes or is killed the same situation will pertain.
 *
 * http://marc.theaimsgroup.com/?l=linux-netdev&r=1&b=200209&w=2
 **/
int dcc_send_job_corked(int net_fd,
			char **argv,
			pid_t cpp_pid,
			int *status,
			const char *cpp_fname)
{
    int ret;

    tcp_cork_sock(net_fd, 1);

    ret = dcc_send_job(net_fd, argv, cpp_pid, status, cpp_fname);
    
    tcp_cork_sock(net_fd, 0);
    
    return ret;
}


int dcc_retrieve_results(int net_fd, int *status, const char *output_fname)
{
    int len;
    int ret;
    int o_len;

    if ((ret = dcc_r_result_header(net_fd))
        || (ret = dcc_r_cc_status(net_fd, status))
        || (ret = dcc_r_token_int(net_fd, "SERR", &len))
        || (ret = dcc_r_file_body(STDERR_FILENO, net_fd, len))
        || (ret = dcc_r_token_int(net_fd, "SOUT", &len))
        || (ret = dcc_r_file_body(STDOUT_FILENO, net_fd, len))
        || (ret = dcc_r_token_int(net_fd, "DOTO", &o_len)))
        return ret;

    /* Previously we would skip retrieving the .o file unless the compiler
     * completed successfully, but it seems cleaner to have the protocol
     * always the same and to always drain the network.  The server always
     * sends an 0 byte file on failure anyhow. */

    if (o_len)
        return dcc_r_file_timed(net_fd, output_fname, o_len);
    else {
        rs_log_notice("skipping retrieval of 0 byte object file %s",
                      output_fname);
        return 0;
    }
}
