/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
 * Copyright (C) 2003 by Apple Computer, Inc.
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


			/* 15 Every one that is found shall be thrust
			 * through; and every one that is joined unto
			 * them shall fall by the sword.
			 *		-- Isaiah 13 */


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>

#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "exitcode.h"
#include "rpc.h"
#include "snprintf.h"
#include "indirect_client.h"


/**
 * @file
 *
 * Very simple RPC-like layer.  Requests and responses are build of
 * little packets each containing a 4-byte ascii token, an 8-byte hex
 * value or length, and optionally data corresponding to the length.
 *
 * 'x' means transmit, and 'r' means receive. 
 *
 * This builds on top of io.c and is called by the various routines
 * that handle communication.
 **/


/**
 * Transmit token name (4 characters) and value (32-bit int, as 8 hex
 * characters).
 **/
int dcc_x_token_int(int ofd, const char *token, unsigned param)
{
    char buf[13];
    int shift;
    char *p;
    const char *hex = "0123456789abcdef";
    
    assert(strlen(token) == 4);
    memcpy(buf, token, 4);

    /* Quick and dirty int->hex.  The only standard way is to call snprintf
     * (?), which is undesirably slow for such a frequently-called
     * function. */
    for (shift=28, p = &buf[4];
         shift >= 0;
         shift -= 4, p++) {
        *p = hex[(param >> shift) & 0xf];
    }
    buf[12] = '\0';

    rs_trace("send %s", buf);
    return dcc_writex(ofd, buf, 12);
}


/**
 * Send start of a result: DONE <version>
 **/
int dcc_x_result_header(int ofd)
{
    return dcc_x_token_int(ofd, "DONE", PROTO_VER);
}


int dcc_r_result_header(int ifd)
{
    int vers;
    int ret;
    
#if defined(DARWIN)
    // We expect that we may currently receive only one request from the server
    // on behalf of gcc (to pull a PCH header).

    while ((ret = dcc_r_token_int(ifd, "DONE", &vers))) {
        // vers indicates the indirection request type
        if ( ! dcc_handle_remote_indirection_request(ifd, vers) ) {
            break;
        }
    }
#else
    if ((ret = dcc_r_token_int(ifd, "DONE", &vers)))
        return ret;
#endif // DARWIN

    if (vers != PROTO_VER) {
        rs_log_error("got version %d not %d in response from server",
                     vers, PROTO_VER);
        return EXIT_PROTOCOL_ERROR;
    }

    rs_trace("got response header");

    return 0;
}


int dcc_x_cc_status(int ofd, int status)
{
    return dcc_x_token_int(ofd, "STAT", status);
}


int dcc_r_cc_status(int ifd, int *status)
{
    return dcc_r_token_int(ifd, "STAT", status);
}


int dcc_r_token(int ifd, char *buf)
{
    return dcc_readx(ifd, buf, 4);
}


/**
 * Read a token and value.  The receiver always knows what token name
 * is expected next -- indeed the names are really only there as a
 * sanity check and to aid debugging.
 *
 * @param ifd      fd to read from
 * @param expected 4-char token that is expected to come in next
 * @param val      receives the parameter value
 **/
int dcc_r_token_int(int ifd, const char *expected, int *val)
{
    char buf[13], *bum;
    int ret = 0;
    
    assert(strlen(expected) == 4);

    if ((ret = dcc_readx(ifd, buf, 12))) {
        rs_log_error("read failed while waiting for token \"%s\"",
                    expected);
        return ret;
    }

    buf[12] = '\0';             /* terminate */

    rs_trace("got %s", buf);
    
    if (memcmp(buf, expected, 4)) {
#if defined(DARWIN)
        if ( memcmp("DONE", expected,  4) ) {
#endif // DARWIN
        rs_log_error("mismatch on token %s", expected);
#if defined(DARWIN)
        } else {
            rs_trace("as expected, mismatch on token %s", expected);
        }
#endif // DARWIN

        // Don't terminate immediately, so that the integer can be pulled, too.
        ret = EXIT_PROTOCOL_ERROR;
    }

    *val = strtoul(&buf[4], &bum, 16);
    if (bum != &buf[12]) {
        rs_log_error("failed to parse integer %s after token \"%s\"", buf,
                     expected);
        return EXIT_PROTOCOL_ERROR;
    }

    return ret;
}
