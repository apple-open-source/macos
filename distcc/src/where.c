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


                /* I put the shotgun in an Adidas bag and padded it
                 * out with four pairs of tennis socks, not my style
                 * at all, but that was what I was aiming for: If they
                 * think you're crude, go technical; if they think
                 * you're technical, go crude.  I'm a very technical
                 * boy.  So I decided to get as crude as possible.
                 * These days, though, you have to be pretty technical
                 * before you can even aspire to crudeness.
                 *              -- William Gibson, "Johnny Mnemonic" */

    
/**
 * @file
 *
 * Routines to decide on which machine to run a distributable job.
 *
 * The current algorithm (new in 1.2 and subject to change) is as follows.
 *
 * Two locks are required to send a job to a machine.  These represent
 * permission to use the CPU, and permission to transmit to that machine.  The
 * CPU lock is held until the job is complete; the transmit lock only until
 * the request has been sent.
 *
 * The transmit lock exists because there is no point trying to transmit more
 * than one job to a server, because the network will be full.  Trying to send
 * two jobs simultaneously is likely to make them both arrive later, and so
 * the remote machine will be idle waiting for a job, for longer than is
 * necessary.  It is probably better to send the first job completely, and
 * then start on the second.
 *
 * Once the request has been transmitted, the lock is released and a second
 * job can be sent.
 *
 * Servers which wish to limit their load can defer accepting jobs, and the
 * client will block with that lock held.
 *
 * cpp is probably cheap enough that we can allow it to run unlocked.  However
 * that is not true for local compilation or linking.
 *
 * When choosing a host, we want to find one with both a CPU and XMIT slot
 * free.  I can't think of any easy way to express that using only Unix
 * locking primitives, and introducing a new process to keep track of it would
 * probably introduce more complexity.  So we iterate until we find a machine
 * with both of these free.
 *
 * @todo Really we need a different locking system for localhost, with about
 * one lock per CPU.  These locks ought to be held throughout execution of
 * course.
 *
 * @todo Perhaps allow for multiple transmission slots. 
 *
 * @todo Write a test harness for the host selection algorithm.  Perhaps a
 * really simple simulation of machines taking different amounts of time to
 * build stuff?
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/file.h>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "hosts.h"
#include "tempfile.h"
#include "lock.h"
#include "where.h"


static int dcc_lock_one(struct dcc_hostdef *hostlist,
                        struct dcc_hostdef **buildhost,
                        int *xmit_lock_fd,
                        int *cpu_lock_fd);


int dcc_pick_host_from_env(struct dcc_hostdef **buildhost,
                           int *xmit_lock_fd,
                           int *cpu_lock_fd)
{
    struct dcc_hostdef *hostlist;
    int ret;
    int n_hosts;
    
    if ((ret = dcc_parse_hosts_env(&hostlist, &n_hosts)) != 0) {
        /* an error occured; but let's be helpful and build locally
         * rather than giving up. */
        *buildhost = (struct dcc_hostdef *) dcc_hostdef_local;
        return 0;
    }

    return dcc_lock_one(hostlist, buildhost, xmit_lock_fd, cpu_lock_fd);
}


static void dcc_lock_pause(void)
{
    /* Some people might want to randomize this, but I think the
     * randomization introduced by scheduling and by tasks starting at
     * different times is probably enough for now.
     *
     * My assumption basically is that polling a little too often is
     * relatively cheap; sleeping when we should be working is bad. */
    rs_trace("nothing available, sleeping...");
    usleep(100000);         /* 0.1s, to start with */
}


/**
 * Find a host that can run a distributed compilation by examining local state.
 * It can be either a remote server or localhost (if that is in the list).
 *
 * This function does not return (except for errors) until a host has been
 * selected.  If necessary it sleeps until one is free.
 *
 * @todo We don't need transmit locks for local operations.
 **/
static int dcc_lock_one(struct dcc_hostdef *hostlist,
                        struct dcc_hostdef **buildhost,
                        int *xmit_lock_fd,
                        int *cpu_lock_fd)
{
    struct dcc_hostdef *h;
    int i_cpu;

    while (1) {
        for (i_cpu = 0; i_cpu < 50; i_cpu++) {
            for (h = hostlist; h; h = h->next) {
                if (i_cpu >= h->n_slots)
                    continue;
                
#if defined(DARWIN)
                if (dcc_lock_host("cpu", h, i_cpu, (h->mode == DCC_MODE_LOCAL),
                                  cpu_lock_fd) == 0) {
#else
                if (dcc_lock_host("cpu", h, i_cpu, 0, cpu_lock_fd) == 0) {
#endif // DARWIN
                    /* If this is localhost, there is no transmission phase
                     * and we don't take a lock */
                    if (h->mode == DCC_MODE_LOCAL) {
                        *xmit_lock_fd = -1;
                        *buildhost = h;
                        return 0;
                    }
                    
                    if (dcc_lock_host("xmit", h, 0, 0, xmit_lock_fd) == 0) {
                        *buildhost = h;
                        return 0;
                    } else {
                        /* release lock */
                        dcc_unlock(*cpu_lock_fd);
                    }
                }
            }
        }
        
        dcc_lock_pause();
    }
}



/**
 * Lock localhost.  Used to get the right balance of jobs when some of
 * them must be local.
 **/
int dcc_lock_local(int *xmit_lock_fd, int *cpu_lock_fd)
{
    struct dcc_hostdef *chosen;
    
    return dcc_lock_one(dcc_hostdef_local, &chosen, xmit_lock_fd, cpu_lock_fd);
}
