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


/**
 * @file
 *
 * Common low-level IO utilities.
 *
 * The pump_* methods are for doing bulk transfer of the entire
 * contents of a file to or from the socket.  We have various
 * different implementations to suit different circumstances.
 *
 * This code is not meant to know about our protocol, only to provide
 * a more comfortable layer on top of Unix IO.
 *
 * @todo Perhaps also add a method of copying that truncates and mmaps
 * the destination file, and then writes directly into it.
 *
 * @todo Perhaps also add a pump method that mmaps the input file, and
 * writes from there to the output file.
 **/

/*
 * TODO: Perhaps write things out using writev() to reduce the number
 * of system calls, and the risk of small packets when not using
 * TCP_CORK.
 *
 * TODO: Check for every call to read(), write(), and other long
 * system calls.  Make sure they handle EINTR appropriately.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "util.h"
#include "exitcode.h"


/**
 * Copy @p n bytes from @p ifd to @p ofd.
 *
 * Does not use sendfile(), so @p ifd may be a socket.
 **/
int dcc_pump_readwrite(int ofd, int ifd, size_t n)
{
    char buf[60000], *p;
    ssize_t r_in, r_out, wanted;

    while (n > 0) {
         wanted = (n > sizeof buf) ? (sizeof buf) : n;
         r_in = read(ifd, buf, (size_t) wanted);
         
         if (r_in == -1) {
            rs_log_error("failed to read %ld bytes: %s",
                         (long) wanted, strerror(errno));
            return -1;
         } else if (r_in == 0) {
              break;	/* great */
         }
         
        n -= r_in;
        p = buf;

        while (r_in > 0) {
            r_out = write(ofd, p, (size_t) r_in);
            if (r_out == -1  ||  r_out == 0) {
                rs_log_error("failed to write: %s", strerror(errno));
                return EXIT_IO_ERROR;
            }
            r_in -= r_out;
            p += r_out;
        }
    }

    return 0;
}



int dcc_readx(int fd,  void *buf, size_t len)
{
	ssize_t r;
	
	while (len > 0) {
		r = read(fd, buf, len);
		if (r == -1) {
			rs_log_error("failed to read: %s", strerror(errno));
                        return EXIT_IO_ERROR;
		} else if (r == 0) {
			rs_log_error("unexpected eof on fd%d", fd);
			return EXIT_TRUNCATED;
		} else {
			buf = &((char *) buf)[r];
			len -= r;
		}
	}

	return 0;
}


int dcc_writex(int fd, const void *buf, size_t len)
{
    ssize_t r;
	
    while (len > 0) {
        r = write(fd, buf, len);
        if (r == -1) {
            rs_log_error("failed to write: %s", strerror(errno));
            return EXIT_IO_ERROR;
        } else if (r == 0) {
            rs_log_error("unexpected eof on fd%d", fd);
            return EXIT_TRUNCATED;
        } else {
            buf = &((char *) buf)[r];
            len -= r;
        }
    }

    return 0;
}


int dcc_r_str_alloc(int fd, int l, char **buf)
{
     char *s;

     if (l < 0)
         rs_fatal("oops, l < 0");

/*      rs_trace("read %d byte string", l); */

     s = *buf = malloc((size_t) l + 1);
     if (!s)
          rs_log_error("malloc failed");
     if (dcc_readx(fd, s, (size_t) l))
          return EXIT_OUT_OF_MEMORY;

     s[l] = 0;

     return 0;
}


int dcc_read_int(int fd, unsigned *v)
{
    char buf[9], *bum;
    int ret;    
	
    if ((ret = dcc_readx(fd, buf, 8)))
        return ret;
    buf[8] = 0;
    
    *v = (unsigned) strtoul(buf, &bum, 16);
    if (bum != &buf[8]) {
        rs_log_error("failed to parse integer %s", buf);
        return EXIT_PROTOCOL_ERROR;
    }

    return 0;
}



/**
 * Stick a TCP cork in the socket.  It's not clear that this will help
 * performance, but it might.
 *
 * This is a no-op if we don't think this platform has corks.
 **/
#ifdef TCP_CORK 
int tcp_cork_sock(int fd, int corked)
#else
int tcp_cork_sock(int UNUSED(fd), int UNUSED(corked))
#endif
{
#ifdef TCP_CORK 
    if (!dcc_getenv_bool("DISTCC_TCP_CORK", 1))
        return 0;
    
    if (setsockopt(fd, SOL_TCP, TCP_CORK, &corked, sizeof corked) == -1) {
        rs_log_notice("setsockopt(corked=%d) failed: %s",
                      corked, strerror(errno));
        /* continue anyhow */
    }
#endif /* def TCP_CORK */
    return 0;
}



int dcc_close(int fd)
{
    if (close(fd) != 0) {
        rs_log_error("failed to close fd%d: %s", fd, strerror(errno));
        return EXIT_IO_ERROR;
    }
    return 0;
}
