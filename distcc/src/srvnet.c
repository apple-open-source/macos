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

                /* "Happy is the man who finds wisdom, and the man who
                 * gets understanding; for the gain from it is better
                 * than gain from silver and its profit better than
                 * gold." -- Proverbs 3:13 */


/**
 * @file
 *
 * Server-side networking.
 **/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#ifdef HAVE_ARPA_NAMESER_H
#  include <arpa/nameser.h>
#endif

#include <arpa/inet.h>

#ifdef HAVE_RESOLV_H
#  include <resolv.h>
#endif

#include "types.h"
#include "exitcode.h"
#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "util.h"
#include "rpc.h"
#include "srvnet.h"

#include "access.h"

int open_socket_in(int port)
{
    struct sockaddr_in sock;
    int res;
    int one = 1;

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
	return -1;
    }

    setsockopt(res, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one));

    /* now we've got a socket - we need to bind it */
    if (bind(res, (struct sockaddr *) &sock, sizeof(sock)) == -1) {
	rs_log_error("bind failed on port %d: %s", port, strerror(errno));
	close(res);
	return -1;
    }

    if (listen(res, 10)) {
        rs_log_error("listen failed: %s", strerror(errno));
        return -1;
    }

    return res;
}


/**
 * Determine if a file descriptor is in fact a socket
 **/
int is_a_socket(int fd)
{
    int v, l;
    l = sizeof(int);
    return (getsockopt(fd, SOL_SOCKET, SO_TYPE, (char *) &v, &l) == 0);
}


/**
 * Log client IP address.
 *
 * @todo Add access-control checks somewhere near here.
 **/
int dcc_check_client(int fd)
{
    struct sockaddr_in sain;
    socklen_t len = sizeof sain;
    char *client_ip;
    struct dcc_allow_list *l;
    int ret; 

    if ((getpeername(fd, (struct sockaddr *) &sain, (int *) &len) == -1)
        || (((size_t)len) > sizeof sain)) {
        rs_log_warning("failed to get peer name: %s", strerror(errno));
        return 0;
    }

    /* XXX: We should use inet_ntop instead, but it's possibly not
     * supported everywhere.  That would be kind of cleaner, and also
     * support IPv6. */
    client_ip = inet_ntoa(sain.sin_addr);
    rs_log_info("connection from %s", client_ip);

    /* if there are any access entries, then we must match one */
    if (opt_allowed) {
        for (l = opt_allowed; l; l = l->next) {
            if ((ret = dcc_check_address(sain.sin_addr.s_addr, l->addr, l->mask)) == 0)
                break;
        }
        if (ret != 0) {
            rs_log_error("connection denied by access list");
            return ret;
        }
    }

    return 0;
}
