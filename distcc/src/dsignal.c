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

/**
 * @file
 * @brief Daemon signal handling.
 **/
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "exitcode.h"
#include "distcc.h"
#include "trace.h"
#include "io.h"
#include "util.h"
#include "dopt.h"
#include "exec.h"
#include "tempfile.h"
#include "srvnet.h"


static void dcc_parent_terminate(int) NORETURN;

/**
 * Signal handler for SIGCHLD.  Does nothing, but calling into this
 * ought to make us break out of waiting for a connection.
 **/
static void dcc_child_exited(int UNUSED(whichsig))
{
}


/**
 * Signals are caught only in the parent at the moment, because we
 * want to be able to kill off all children and log a message when
 * exiting.  This is perhaps not the best thing to do -- it might be
 * better to simply let ourselves be terminated, and for children to
 * go away when they're finished their current work.  If the admin is
 * really impatient, they can kill all the children anyhow.
 **/
void dcc_catch_signals(void)
{
    struct sigaction act_catch, act_exited;

    memset(&act_catch, 0, sizeof act_catch);
    act_catch.sa_handler = dcc_parent_terminate;

    memset(&act_exited, 0, sizeof act_exited);
    act_exited.sa_handler = dcc_child_exited;
    act_exited.sa_flags = SA_NOCLDSTOP;

    if (sigaction(SIGTERM, &act_catch, NULL)
        || sigaction(SIGINT, &act_catch, NULL)
        || sigaction(SIGHUP, &act_catch, NULL)
        || sigaction(SIGQUIT, &act_catch, NULL)
        || sigaction(SIGALRM, &act_catch, NULL)
        || sigaction(SIGCHLD, &act_exited, NULL)) {
        rs_log_error("failed to install signal handlers: %s",
                     strerror(errno));
    }
}


/**
 * Ignore hangup signal.
 *
 * This is only used in detached mode to make sure the daemon does not
 * quit when whoever started it closes their terminal.  In nondetached
 * mode, the signal is logged and causes an exit as normal.
 **/
void dcc_ignore_sighup(void)
{
    struct sigaction act_ignore;

    memset(&act_ignore, 0, sizeof act_ignore);
    act_ignore.sa_handler = SIG_IGN;

    if (sigaction(SIGHUP, &act_ignore, NULL)) {
        rs_log_error("failed to install signal handler: %s",
                     strerror(errno));
    } else {
        rs_trace("ignoring SIGHUP");
    }
}



/**
 * Just log, remove pidfile, and exit.
 *
 * The children will close down in their own time when they've
 * finished whatever they're doing.
 *
 * Called when the parent gets a signal.
 *
 * This used to also kill all daemon children, but that's not such a
 * good idea: if they're busy serving a connection, it really does no
 * harm to leave them running, and it is probably cleaner to let them
 * finish their work.  The main socket gets unbound, so no more
 * connections will be accepted.
 **/
static void dcc_parent_terminate(int whichsig)
{
    /* CAUTION!  This must not call any nonreentrant functions (like
     * stdio), because it is called in a signal context. */
    dcc_remove_pid();

#ifdef HAVE_STRSIGNAL
    rs_log_info("terminated: %s", strsignal(whichsig));
#else
    rs_log_info("terminated: signal %d", whichsig);
#endif
    
    dcc_exit(EXIT_SUCCESS);
}


