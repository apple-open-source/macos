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


                /* "Just like distributed.net, only useful!" */

/**
 * @file
 *
 * distcc volunteer server.  Accepts and serves requests to compile
 * files.
 *
 * May be run from inetd (default if stdin is a socket), or as a
 * daemon by itself.  
 *
 * distcc has an adequate but perhaps not optimal system for deciding
 * where to send files.  The general principle is that the server
 * should say how many jobs it is willing to accept, rather than the
 * client having to know.  This is probably good in two ways: it
 * allows for people in the future to impose limits on how much work
 * their contributed machine will do, and secondly it seems better to
 * put this information in one place rather than on every client.
 **/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>

#include "exitcode.h"
#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "util.h"
#include "dopt.h"
#include "setuid.h"
#include "srvnet.h"


/* for trace.c */
char const *rs_program_name = "distccd";


static int dcc_inetd_server(void);
static void dccd_setup_log(void);


/**
 * distcc daemon.  May run from inetd, or standalone.  Accepts
 * requests from clients to compile files.
 **/
int main(int argc, char *argv[])
{
    int ret;

    /* Any errors during startup (e.g. bad options) ought to go to stderr.
     * Once we're up and running, we use just the specified log file. */
    rs_trace_set_level(RS_LOG_INFO);
    rs_add_logger(rs_logger_file, RS_LOG_DEBUG, 0, STDERR_FILENO);

    if (distccd_parse_options(argc, (const char **) argv))
        dcc_exit(EXIT_DISTCC_FAILED);

    if ((ret = dcc_set_lifetime()) != 0)
        dcc_exit(ret);

    if ((ret = dcc_discard_root()) != 0)
        dcc_exit(ret);

    /* Discard privileges before opening log so that if it's created, it has
     * the right ownership. */
    dccd_setup_log();

    /* This test might need to be a bit more complex when we want to
     * support ssh and stuff like that.  Perhaps it would be better to
     * always serve stdin, and then complain if it's a terminal? */
    if (opt_inetd_mode) {
        ret = dcc_inetd_server();
    } else if (opt_daemon_mode) {
        ret = dcc_standalone_server();
    } else if (is_a_socket(STDIN_FILENO)) {
        rs_log_info("stdin is socket; assuming --inetd mode");
        ret = dcc_inetd_server();
    } else if (isatty(STDIN_FILENO)) {
        rs_log_info("stdin is a tty; assuming --daemon mode");
        ret = dcc_exit(dcc_standalone_server());
    } else {
        rs_log_info("stdin is neither a tty nor a socket; assuming --daemon mode");
        ret = dcc_standalone_server();
    }
    dcc_exit(ret);
}


/**
 * If a --lifetime options was specified, set up a timer that will kill the
 * daemon when it expires.
 **/
int dcc_set_lifetime(void)
{
    if (opt_lifetime) {
        alarm(opt_lifetime);
        rs_trace("set alarm for %+d seconds", opt_lifetime);
    }
    return 0;
}


static void dccd_setup_log(void)
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
    
    openlog("distccd", LOG_PID, LOG_DAEMON);
    rs_remove_all_loggers();
    rs_add_logger(rs_logger_syslog, RS_LOG_DEBUG, NULL, 0);
}


/**
 * Serve a single file on stdin, and then exit.
 **/
static int dcc_inetd_server(void)
{
    int ret, close_ret;
    
    rs_log_info("inetd server started (version %s, built %s %s)", PACKAGE_VERSION,
                __DATE__, __TIME__);
    
    ret = dcc_accept_job(STDIN_FILENO);

    close_ret = dcc_close(STDIN_FILENO);

    if (ret)
        return ret;
    else
        return close_ret;
}

