/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
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


                        /* I just wish I could get caller-IQ on my phones...
                                   -- The Purple People-Eater, NANAE */



#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <netdb.h>

#include "types.h"
#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "exitcode.h"
#include "clinet.h"
#include "util.h"


/**
 * @file
 *
 * Client-side networking.
 *
 * @todo In error messages, show the name of the relevant host.
 * Should do this even in readx(), etc.
 *
 * @todo Use the new sockets API (optionally?) to support IPv6.  This
 * must be done in a way that will still compile on older machines.
 **/



/**
 * Open a socket to a tcp remote host with the specified port.
 *
 * @todo Don't try for too long to connect. 
 **/
int dcc_open_socket_out(const char *host, int port, int *p_fd)
{
    int type = SOCK_STREAM;
    struct sockaddr_in sock_out;
    int fd;
    struct hostent *hp;

    /* Ignore SIGPIPE; we consistently check error codes and will
     * see the EPIPE. */
    dcc_ignore_sigpipe(1);

    fd = socket(PF_INET, type, 0);
    if (fd == -1) {
	rs_log_error("failed to create socket: %s", strerror(errno));
	return EXIT_CONNECT_FAILED;
    }

    /* FIXME: warning: gethostbyname() leaks memory.  Use gethostbyname_r instead! */
    hp = gethostbyname(host);
    if (!hp) {
	rs_log_error("unknown host: \"%s\"", host);
	(void) close(fd);
	return EXIT_CONNECT_FAILED;
    }

    memcpy(&sock_out.sin_addr, hp->h_addr, (size_t) hp->h_length);
    sock_out.sin_port = htons((in_port_t) port);
    sock_out.sin_family = PF_INET;

    if (connect(fd, (struct sockaddr *) &sock_out, (int) sizeof(sock_out))) {
        rs_log_error("failed to connect to %s port %d: %s", host, port, 
                     strerror(errno));
	(void) close(fd);
	return EXIT_CONNECT_FAILED;
    }

    rs_trace("client got connection to %s port %d on fd%d", host, port, fd);

    *p_fd = fd;
    return 0;
}


