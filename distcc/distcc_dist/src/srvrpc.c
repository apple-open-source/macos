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
 * Server-specific RPC code.
  **/



#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "rpc.h"
#include "exitcode.h"
#include "dopt.h"
#include "hosts.h"


int dcc_r_request_header(int ifd,
                         enum dcc_protover *ver_ret)
{
    unsigned vers;
    int ret;
    
    if ((ret = dcc_r_token_int(ifd, "DIST", &vers)) != 0) {
        rs_log_error("client did not provide distcc magic fairy dust");
        return ret;
    }

    if (vers != 1 && vers != 2) {
        rs_log_error("can't handle requested protocol version is %d", vers);
        return EXIT_PROTOCOL_ERROR;
    }

    *ver_ret = (enum dcc_protover) vers;
    
    return 0;
}



/**
 * Read an argv[] vector from the network.
 **/
int dcc_r_argv(int ifd, /*@out@*/ char ***argv)
{
    unsigned i;
    unsigned argc;
    char **a;
    int ret;

    *argv = NULL;
     
    if (dcc_r_token_int(ifd, "ARGC", &argc))
        return EXIT_PROTOCOL_ERROR;

    rs_trace("reading %d arguments from job submission", argc);
    
    /* Have to make the argv one element too long, so that it can be
     * terminated by a null element. */
    *argv = a = (char **) calloc((size_t) argc+1, sizeof a[0]);
    if (a == NULL) {
        rs_log_error("alloc failed");
        return EXIT_OUT_OF_MEMORY;
    }
    a[argc] = NULL;

    for (i = 0; i < argc; i++) {
        if ((ret = dcc_r_token_string(ifd, "ARGV", &a[i])))
            return ret;

        rs_trace("argv[%d] = \"%s\"", i, a[i]);
    }

    dcc_trace_argv("got arguments", a);
    
    return 0;
}
