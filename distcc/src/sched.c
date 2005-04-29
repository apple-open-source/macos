/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003 by Apple Computer, Inc.
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


/**
 * @file
 *
 * distcc zeroconfig server.  Tracks distcc volunteer servers and
 * serves requests for a canonical list.
 *
 * May only be run as a daemon by itself.
 **/

#if defined(DARWIN)


#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "config.h"
#include "distcc.h"
#include "dopt.h"
#include "exitcode.h"
#include "io.h"
#include "rpc.h"
#include "setuid.h"
#include "srvnet.h"
#include "trace.h"
#include "util.h"
#include "zeroconf_browse.h"
#include "zeroconf_util.h"


// for trace.c
const char *rs_program_name = "distccschedd";

static void dcc_zc_process_messages(int clientFD);
static void dccschedd_setup_log(void);

// from dparent.c
void dcc_detach(void);
void dcc_reap_kids(void);

/**
 * distcc scheduler daemon.  May run standalone only.
 * Accepts requests from distcc clients for a list of distcc servers.
 **/
int main(int argc, char *argv[])
{
    int   listen_fd;
    int   ret;
    char *zcTxtRecord;

    // Any errors during startup (e.g. bad options) ought to go to stderr.
    // Once we're up and running, we use just the specified log file.
    rs_trace_set_level(RS_LOG_INFO);
    rs_add_logger(rs_logger_file, RS_LOG_DEBUG, 0, STDERR_FILENO);

    if (distccd_parse_options(argc, (const char **) argv))
        dcc_exit(EXIT_DISTCC_FAILED);

    if ((ret = dcc_discard_root()) != 0)
        dcc_exit(ret);

    /* Discard privileges before opening log so that if it's created, it has
     * the right ownership. */
    dccschedd_setup_log();

    if ((listen_fd = open_socket_in(DISTCC_DEFAULT_SCHEDULER_PORT)) == -1)
        return EXIT_BIND_FAILED;

    set_cloexec_flag(listen_fd, 1);

    rs_log(RS_LOG_INFO|RS_LOG_NONAME,
           "%s (version %s, built %s %s) listening on port %d",
           rs_program_name, PACKAGE_VERSION, __DATE__, __TIME__, DISTCC_DEFAULT_SCHEDULER_PORT);

    dcc_catch_signals();

    zcTxtRecord = dcc_generate_txt_record();

    if (!opt_no_detach) {
        /* Don't go into the background until we're listening and
         * ready.  This is useful for testing -- when the daemon
         * detaches, we know we can go ahead and try to connect.  */
        dcc_detach();
	chdir("/");
    }
 
    dcc_browse_for_zeroconfig(zcTxtRecord);

    // Does not return.
    dcc_zc_process_messages(listen_fd);

    free(zcTxtRecord);

    return 0;
}


/**
 * Similar to <code>dccd_setup_log</code>.
 **/
static void dccschedd_setup_log(void)
{
    int fd;
    
    /* We start off tracing to stderr, so that bad options will be
     * logged to an obvious place.  This can be overridden by the
     * --log-file= option, handled in dopt.c; otherwise we switch to
     * syslog. */

    if (opt_log_stderr)
        return;
    
    if (arg_log_file) {
        if ((fd = open(arg_log_file, O_CREAT|O_APPEND|O_WRONLY, 0666)) == -1) {
            rs_log_error("failed to open %s: %s", arg_log_file,
                         strerror(errno));
            /* continue and use syslog */
    } else {
            rs_remove_all_loggers();
            rs_add_logger(rs_logger_file, RS_LOG_DEBUG, NULL, fd);
            return;
        }
    }
    
    openlog("distccschedd", LOG_PID, LOG_DAEMON);
    rs_remove_all_loggers();
    rs_add_logger(rs_logger_syslog, RS_LOG_DEBUG, NULL, 0);
}


/**
 * Sends a canonical list of zeroconfiguration-enabled compile servers,
 * distccd, from the compile client, distcc.
 * Communicates this list over the socket described by <code>queuedFD</code>.
 **/
static int dcc_zc_accept_request(int queuedFD)
{
    int    exitVal;
    char  *list    = dcc_zc_resolved_services_list();
    size_t len     = strlen(list) + 1;

    rs_trace("Transmitting service list length: %u", len);
    exitVal = dcc_x_token_int(queuedFD, DISTCC_DEFAULT_ZC_LIST_LEN_TOKEN, len);

    if ( exitVal == 0 ) {
        rs_trace("Transmitted service list length, transmitting list: %s",
                 list);

        exitVal = dcc_writex(queuedFD, list, len);

        if ( exitVal == 0 ) {
            rs_trace("Transmitted service list.");
        } else {
            rs_log_error("Failed to transmit service list: (%d) %s", exitVal,
                         strerror(exitVal));
        }
    } else {
            rs_log_error("Failed to transmit service list length: (%d) %s",
                         exitVal, strerror(exitVal));
    }

    return exitVal;
}


/**
 * Accepts a request for a canonical list of zeroconfiguration-enabled compile
 * servers, distccd, from the compile client, distcc.
 * Communicates this list over the socket described by <code>clientFD</code>.
 **/
static void dcc_zc_serve_connection(int clientFD)
{
    int queuedFD = accept(clientFD, NULL, 0);

    if ( queuedFD == -1 ) {
        rs_log_error("Failure processing client connection: (%d) %s",
                     errno, strerror(errno));
    } else {
        if ( opt_no_fork ) {
            dcc_zc_accept_request(queuedFD);
        } else {
            int pid = fork();

            if ( pid < 0 ) {
                rs_log_error("Unable to serve client connection: (%d) %s",
                             errno, strerror(errno));
            } else if ( pid == 0 ) {
                int exitVal = dcc_zc_accept_request(queuedFD);

                close(queuedFD);
                close(clientFD);
                dcc_exit(exitVal);
            } else {
                rs_trace("Forked child to serve client connection (%d)", pid);
            }
        }

        // The child process will not continue past the dcc_exit call,
        // so there's no concern about closing this file descriptor twice.
        close(queuedFD);
    }

    if (!opt_no_fork) {
        dcc_reap_kids();
    }
}


/**
 * Processes a messages read from a file descriptor set in a <code>select</code>
 * loop.  <code>clientFD</code> is included in the file descriptor set.
 * Such messages included zeroconfiguration changes from compile servers
 * on the local subnet, as well as requests from compile clients on the local
 * machine.
 **/
static void dcc_zc_process_messages(int clientFD)
{
    int    exitVal;
    int    fdCount;
    fd_set readFDs;

    dcc_zc_add_to_select_set(clientFD);

    rs_log_info("waiting to accept connection");

    do {
        FD_ZERO(&readFDs);
        dcc_zc_select_set(&readFDs);

        fdCount = dcc_zc_select_count();

        exitVal = select(fdCount, &readFDs, NULL, NULL, NULL);

        if ( exitVal == -1 ) {
            rs_trace("Failure while processing messages: (%d) %s", exitVal,
                     strerror(exitVal));
        } else if ( exitVal == 0 ) {
            rs_trace("No messages available to process");
        } else {
            exitVal = dcc_zc_process_browse_messages(&readFDs, exitVal);
            exitVal = dcc_zc_process_resolve_messages(&readFDs, exitVal);

            if ( exitVal > 0 && FD_ISSET(clientFD, &readFDs) &&
                dcc_zc_should_process_client_requests() ) {
                dcc_zc_serve_connection(clientFD);
                rs_log_info("waiting to accept more connections");
            }
        }
    } while (TRUE);
}


#endif // DARWIN
