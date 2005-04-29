/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
 * Copyright (C) 2005 by Apple Computer
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
 * Parse and apply server options.
 **/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <popt.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "types.h"
#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "dopt.h"
#include "exitcode.h"
#include "setuid.h"
#include "access.h"

int arg_nice_inc;
int arg_nchildren = 2;          /**< Number of children running jobs
                                 * on this machine.  About ncpus+1 I
                                 * think. */
int arg_port = DISTCC_DEFAULT_PORT;

/** If true, serve all requests directly from listening process
    without forking.  Better for debugging. **/
int opt_no_fork = 0;

int opt_daemon_mode = 0;
int opt_inetd_mode = 0;
int opt_no_fifo = 0;

struct dcc_allow_list *opt_allowed = NULL;

/**
 * If true, don't detach from the parent.  This is probably necessary
 * for use with daemontools or other monitoring programs, and is also
 * used by the test suite.
 **/
int opt_no_detach = 0;

int opt_log_stderr = 0;

/**
 * Daemon exits after this many seconds.  Intended mainly for testing, to make
 * sure daemons don't persist for too long.
 */
int opt_lifetime = 0;

const char *arg_pid_file = NULL;
const char *arg_log_file = NULL;

enum {
    opt_log_to_file = 300
};


const struct poptOption options[] = {
    { "allow", 'a',      POPT_ARG_STRING, 0, 'a', 0, 0 },
    { "concurrent", 'n', POPT_ARG_INT, &arg_nchildren, 'n', 0, 0 },
    { "daemon", 0,       POPT_ARG_NONE, &opt_daemon_mode, 0, 0, 0 },
    { "help", 0,         POPT_ARG_NONE, 0, '?', 0, 0 },
    { "inetd", 0,        POPT_ARG_NONE, &opt_inetd_mode, 0, 0, 0 },
    { "lifetime", 0,     POPT_ARG_INT, &opt_lifetime, 0, 0, 0 },
    { "log-file", 0,     POPT_ARG_STRING, &arg_log_file, 0, 0, 0 },
    { "log-stderr", 0,   POPT_ARG_NONE, &opt_log_stderr, 0, 0, 0 },
    { "nice", 'N',       POPT_ARG_INT, &arg_nice_inc,  'N', 0, 0 },
    { "no-detach", 0,    POPT_ARG_NONE, &opt_no_detach, 0, 0, 0 },
    { "no-fifo", 0,      POPT_ARG_NONE, &opt_no_fifo, 0, 0, 0 },
    { "no-fork", 0,      POPT_ARG_NONE, &opt_no_fork, 0, 0, 0 },
    { "pid-file", 'P',   POPT_ARG_STRING, &arg_pid_file, 0, 0, 0 },
    { "port", 'p',       POPT_ARG_INT, &arg_port,      0, 0, 0 },
    { "user", 0,         POPT_ARG_STRING, &opt_user, 'u', 0, 0 },
    { "verbose", 0,      POPT_ARG_NONE, 0, 'v', 0, 0 },
    { "version", 0,      POPT_ARG_NONE, 0, 'V', 0, 0 },
    { "compiler-versions", 0,      POPT_ARG_NONE, 0, 'C', 0, 0 },
    { "os-version", 0,   POPT_ARG_NONE, 0, 'O', 0, 0 },
    { "protocol-version", 0,      POPT_ARG_NONE, 0, 'R', 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 }
};


static void distccd_show_usage(void)
{
    dcc_show_version("distccd");
    dcc_show_copyright();
    printf (
"Usage:\n"
"   distccd [OPTIONS]\n"
"\n"
"Options:\n"
"    --help                     explain usage and exit\n"
"    --version                  show version and exit\n"
"    -p, --port PORT            TCP port to listen on\n"
"    -P, --pid-file FILE        save daemon process id to file\n"
"    -N, --nice LEVEL           lower priority, 20=most nice\n"
"    --user USER                if run by root, change to this persona\n"
"    -a, --allow IP[/MASK]      client address access control\n"
"  Debug and trace:\n"
"    --verbose                  include debug messages in log\n"
"    --no-fork                  single process only (for debugging ONLY)\n"
"    --no-detach                don't detach from parent (for daemontools, etc)\n"
"    --log-file=FILE            send messages here instead of syslog\n"
"    --log-stderr               send messages to stderr (for debugging)\n"
"  Mode of operation:\n"
"    --inetd                    serve client connected to stdin\n"
"    --daemon                   bind and listen on socket\n"
"\n"
"distccd runs either from inetd or as a standalone daemon to compile\n"
"files submitted by the distcc client.\n"
"\n"
"distccd should only run on trusted networks.\n"
);
}


int distccd_parse_options(int argc, const char **argv)
{
    poptContext po;
    int po_err, exitcode;

    po = poptGetContext("distccd", argc, argv, options, 0);

    while ((po_err = poptGetNextOpt(po)) != -1) {
        switch (po_err) {
        case '?':
            distccd_show_usage();
            exitcode = 0;
            goto out_exit;

        case 'a': {
            /* TODO: Allow this to be a hostname, which is resolved to an address. */
            /* TODO: Split this into a small function. */
            struct dcc_allow_list *new;
            new = malloc(sizeof *new);
            if (!new)
                rs_fatal("malloc failed");
            new->next = opt_allowed;
            opt_allowed = new;
            if ((exitcode = dcc_parse_mask(poptGetOptArg(po), &new->addr, &new->mask)))
                goto out_exit;
        }
            break;

        case 'n':
            if (arg_nchildren < 1 || arg_nchildren > 20000) {
                rs_log_error("--tasks argument must be >1");
                exitcode = EXIT_BAD_ARGUMENTS;
                goto out_exit;
            }
            break;

        case 'N':
            /* just do it now */
            if (nice(arg_nice_inc) == -1) {
                rs_log_warning("nice %d failed: %s", arg_nice_inc,
                               strerror(errno));
                /* continue anyhow */
            }
            break;

        case 'u':
            if (getuid() != 0 && geteuid() != 0) {
                rs_log_warning("--user is ignored when distccd is not run by root");
                /* continue */
            }
            break;

        case 'V':
            dcc_show_version("distccd");
            exitcode = EXIT_SUCCESS;
            goto out_exit;

	case 'C':
	    puts(dcc_get_compiler_versions());
            exitcode = EXIT_SUCCESS;
            goto out_exit;

        case 'v':
            rs_trace_set_level(RS_LOG_DEBUG);
            break;

	case 'O':
	    puts(dcc_get_system_version());
            exitcode = EXIT_SUCCESS;
            goto out_exit;

	case 'R':
	    puts(dcc_get_protocol_version());
            exitcode = EXIT_SUCCESS;
            goto out_exit;

        default:                /* bad? */
            rs_log(RS_LOG_NONAME|RS_LOG_ERR|RS_LOG_NO_PID, "%s: %s",
                   poptBadOption(po, POPT_BADOPTION_NOALIAS),
                   poptStrerror(po_err));
            exitcode = EXIT_BAD_ARGUMENTS;
            goto out_exit;
        }
    }

    poptFreeContext(po);
    return 0;

    out_exit:
    poptFreeContext(po);
    exit(exitcode);
}
