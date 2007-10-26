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
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "distcc.h"
#include "trace.h"
#include "exec.h"
#include "rpc.h"
#include "exitcode.h"
#include "util.h"
#include "clinet.h"
#include "bulk.h"
#include "hosts.h"
#include "state.h"
#include "indirect_client.h"
#include "indirect_util.h"

/**
 * @file
 *
 * @brief Client-side RPC functions.
 **/

/*
 * Transmit header for whole request.
 */
int dcc_x_req_header(int fd,
                     enum dcc_protover protover)
{
     return dcc_x_token_int(fd, "DIST", protover);
}



/**
 * Transmit an argv array.
 **/
int dcc_x_argv(int fd, char **argv)
{
    int i;
    int ret;
    int argc;
    
    dcc_fix_opt_x(argv);
    argc = dcc_argv_len(argv);
    
    if (dcc_x_token_int(fd, "ARGC", (unsigned) argc))
        return EXIT_PROTOCOL_ERROR;
    
    for (i = 0; i < argc; i++) {
        if ((ret = dcc_x_token_string(fd, "ARGV", argv[i])))
            return ret;
    }

    return 0;
}



/**
 * Read the "DONE" token from the network that introduces a response.
 **/
int dcc_r_result_header(int ifd,
                        enum dcc_protover expect_ver)
{
    unsigned vers;
    int ret;
    
    if ((ret = dcc_r_token_int(ifd, "DONE", &vers)))
        return ret;
    
    if (vers != expect_ver) {
        rs_log_error("got version %d not %d in response from server",
                     vers, expect_ver);
        return EXIT_PROTOCOL_ERROR;
    }

    rs_trace("got response header");

    return 0;
}


int dcc_r_cc_status(int ifd, int *status)
{
    unsigned u_status;
    int ret;
    
    ret = dcc_r_token_int(ifd, "STAT", &u_status);
    *status = u_status;
    return ret;
}

/**
 * Called while retrieving results to check the system and compiler version info
 * of the build machine. If they do not match what we were expecting (if it is set)
 * then this is considered a communications failure under the assumption that the
 * client machine should not distribute to that host.
 */
static int dcc_validate_build_machine(struct dcc_hostdef *host)
{
    char *expectedSystemInfo = getenv("DISTCC_SYSTEM");
    if (expectedSystemInfo) {
        if (strcmp(expectedSystemInfo, host->system_info) != 0) {
            rs_log_error("%s reported incompatible system info. Expected: %s, received %s", host->hostname, expectedSystemInfo, host->system_info);
            return EXIT_PROTOCOL_ERROR;
        }
    }
    char *expectedCompilerVersion = getenv("DISTCC_COMPILER");
    if (expectedCompilerVersion) {
        if (strcmp(expectedCompilerVersion, host->compiler_vers) != 0) {
            rs_log_error("%s reported incompatible compiler version. (Need to restart distccd?)\nExpected: %s\nReceived %s", host->hostname, expectedCompilerVersion, host->compiler_vers);
            return EXIT_PROTOCOL_ERROR;
        }
    }
    return 0;
}

/**
 * The second half of the client protocol: retrieve all results from the server.
 **/
int dcc_retrieve_results(int net_fd,
                         int *status,
                         const char *output_fname,
                         struct dcc_hostdef *host)
{
    unsigned len, indirection;
    int ret;
    unsigned o_len;

    do {
        if ((ret = dcc_r_token_int(net_fd, indirection_request_token, &indirection)))
            return ret;
        if (indirection != indirection_complete)
            dcc_handle_remote_indirection_request(net_fd, indirection);
    } while (indirection != indirection_complete);
    
    if ((ret = dcc_r_result_header(net_fd, host->protover)))
        return ret;

    /* We've started to see the response, so the server is done
     * compiling. */
    dcc_note_state(DCC_PHASE_RECEIVE, NULL, NULL);

    if ((ret = dcc_r_token_string(net_fd, "SINF", &host->system_info))
        || (ret = dcc_r_token_string(net_fd, "CVER", &host->compiler_vers))
        || (ret = dcc_validate_build_machine(host))
        || (ret = dcc_r_cc_status(net_fd, status))
        || (ret = dcc_r_token_int(net_fd, "SERR", &len))
        || (ret = dcc_r_bulk(STDERR_FILENO, net_fd, len, host->compr))
        || (ret = dcc_r_token_int(net_fd, "SOUT", &len))
        || (ret = dcc_r_bulk(STDOUT_FILENO, net_fd, len, host->compr))
        || (ret = dcc_r_token_int(net_fd, "DOTO", &o_len)))
        return ret;

    /* If the compiler succeeded, then we always retrieve the result,
     * even if it's 0 bytes.  */
    if (*status == 0) {
        return dcc_r_file_timed(net_fd, output_fname, o_len, host->compr);
    } else if (o_len != 0) {
        rs_log_error("remote compiler failed but also returned output: "
                     "I don't know what to do");
    }

    return 0;
}
