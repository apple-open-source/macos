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
 * Functions that allow the client process (distcc) to discover a canonical
 * list of zeroconfiguration-enabled compile servers (distccd).
 **/


#if defined(DARWIN)


#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "config.h"
#include "distcc.h"
#include "io.h"
#include "rpc.h"
#include "trace.h"
#include "util.h"
#include "zeroconf_client.h"
#include "zeroconf_util.h"


/**
 * Creates a socket connection to <code>port</code>.  Returns the file
 * descriptor for the socket.  Error values returned via <code>errorVal</code>.
 **/
static int open_socket_out(int port, int *errorVal)
{
    int                res;
    struct sockaddr_in sock;
    struct timeval     timeout = { 1, 0 };

    *errorVal = 0;

    if (port < 1 || port > 65535) {
        /* htons() will truncate, not check */
        rs_log_error("port number out of range: %d", port);
        return -1;
    }

    memset((char *) &sock, 0, sizeof(sock));
    sock.sin_port = htons(port);
    sock.sin_family = PF_INET;
    sock.sin_addr.s_addr = INADDR_ANY;

    res = socket(PF_INET, SOCK_STREAM, 0);
    if (res == -1) {
        rs_log_error("socket creation failed: %s", strerror(errno));
        *errorVal = errno;
        return -1;
    }

    setsockopt(res, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if ( connect(res, (struct sockaddr *) &sock, (int) sizeof(sock)) == 0 ) {
        rs_trace("client got connection to port %d on fd%d", port, res);
    } else {
        rs_log_error("failed to connect to port %d: (%d) %s", port,
                     errno, strerror(errno));
        *errorVal = errno;
        close(res);
        return -1;
    }

    return res;
}


/**
 * Requests a canonical list of zeroconfiguration-enabled compile servers
 * (distccd) from a daemon running on the local machine (distccschedd).
 * Returns the list via <code>listPtr</code>; the resulting string must be
 * freed by the caller.
 * If the client (distcc) fails to establish a connection at the well-known
 * port, attempts to start an instance of distccschedd and waits a reasonable
 * period of time to allow the daemon to prepare itself for connections.
 * The client only invokes this function if no list is provided via the
 * environment variable, <code>DISTCC_HOSTS</code>.
 * The list returned via <code>listPtr</code> conforms to the format expected
 * of the value of <code>DISTCC_HOSTS</code>.
 **/
void dcc_zc_get_resolved_services_list(char **listPtr)
{
    int    len;
    int    errorVal;
    int    netfd = open_socket_out(DISTCC_DEFAULT_SCHEDULER_PORT, &errorVal);

    if ( netfd == -1 ) {
        // try once to start up distccschedd
        int   maxRetries = 30;
        char *path       = (char *) "/usr/bin/distccschedd";
        int   ret;
        int   retries    = 0;

        if (dcc_getenv_bool("DISTCC_VERBOSE", 0)) {
            char *args[] = { path, (char *) "--daemon", (char *) "--verbose",
                             (char *) "--log-file=/tmp/distccHelperLog", NULL };
            ret = dcc_simple_spawn(path, args);
        } else {
            char *args[] = { path, (char *) "--daemon", NULL };
            ret = dcc_simple_spawn(path, args);
        }

        if ( ret != 0 ) {
            rs_trace("Unable to start distccschedd after failing to contact it once.");
        }

        rs_trace("Attempting to contact distccschedd again.");

        // Whether we started distccschedd successfully, there's really
        // no opportunity to continue here with a list of hosts, except
        // to compile locally.
        // Pause between the next 30 successive attempts to contact
        // distccschedd.
        do {
            retries++;
            rs_trace("Pausing to allow distccschedd to initialize");
            sleep(1);
            rs_trace("Finished pausing");
            netfd = open_socket_out(DISTCC_DEFAULT_SCHEDULER_PORT,
                                    &errorVal);
        } while ( netfd == -1 && retries <= maxRetries &&
                 ( errorVal == ETIMEDOUT || errorVal == ECONNREFUSED ) );
    }

    if ( netfd == -1 ) {
        rs_log_error("Unable to open port %u to contact distccschedd",
                     DISTCC_DEFAULT_SCHEDULER_PORT);
    } else {
        if ( dcc_r_token_int(netfd, DISTCC_DEFAULT_ZC_LIST_LEN_TOKEN,
                             &len) == 0 ) {
            int exitVal = dcc_r_str_alloc(netfd, len, listPtr);

            if ( exitVal == 0 ) {
                rs_trace("Read zeroconfig service list: %s", *listPtr);
            } else {
                rs_log_error("Unable to read zeroconfig service list: (%d) %s",
                             exitVal, strerror(exitVal));
            }
        } else {
            rs_log_error("Unable to read zeroconfig service list length");
        }

        close(netfd);
    }
}


#endif // DARWIN
