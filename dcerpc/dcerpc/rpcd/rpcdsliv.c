/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
**
**  NAME:
**
**      rpcdsliv.c
**
**  FACILITY:
**
**      RPC Daemon
**
**  ABSTRACT:
**
**      RPCD  Server Liveness Module.  Tasks to periodically ping servers
**      which are registered in the endpoint database and mark them for
**      deletion from the database if they do not respond.  One task
**      also purges entries which are marked as deleted and which have
**      no read references to them.
**
**
**
*/

#include <commonp.h>
#include <com.h>

#include <dce/ep.h>     /* derived from ep.idl */
#include <dsm.h>        /* derived from dsm.idl */

#include <rpcdp.h>
#include <rpcddb.h>
#include <rpcdepdbp.h>
#include <rpcdutil.h>

#define slive_c_long_wait   (15*60) /* 15 minutes */
#define slive_c_short_wait  (1*60)  /* 1 minute */

/*  RPC comm timeout for ping of a "good" server
 *  (ie. a server which has been communicating)
 */
#define slive_c_short_comm_timeout          3

/*  Number of consecutive failures to communicate with a server
 *  before it is deemed to be dead and is removed from the database
 */
#define slive_c_max_server_not_listening    20

INTERNAL void sliv_task1
    (
        void    *arg
    );

INTERNAL void sliv_task2
    (
        void    *arg
    );

INTERNAL boolean32 ping_server
    (
        db_entry_t      *entp,
        unsigned32      timeout,
        error_status_t  *status
    );


/*  Start server alive tasks and init condition variable
 *  used by task1 to tell task2 that it should aggressively
 *  ping a server
 *
 *  NB: this routine must be called after the db locks have been
 *  inited
 */
PRIVATE void sliv_init(h, status)
struct db       *h;
error_status_t  *status;
{
    dcethread_cond_init_throw(&h->sliv_task2_cv, NULL);

    dcethread_create_throw(&h->sliv_task1_h, NULL,
            (void*) sliv_task1, (void *) h);

    dcethread_create_throw(&h->sliv_task2_h, NULL,
            (void*) sliv_task2, (void *) h);

    *status = error_status_ok;
}

/*  Task1 runs a few times an hour
 *  It purges entries which are marked as deleted and
 *      have no read references to them.
 *  It also pings servers which have been reachable.
 *  If a server becomes not reachable, its destiny is passed
 *  off to Task2 which pings it more frequently and will
 *  mark it for deletion if it isn't reachable after
 *  slive_c_max_server_not_listening consecutive tries.
 */

INTERNAL void sliv_task1(arg)
void    *arg;
{
#define slive_c_max_deletes 5

    struct db       *h;
    struct timeval  now;
    struct timezone tz;
    unsigned32      ndeletes;
    db_lists_t      *lp,
                    *lp_next;
    db_entry_t      *entp;
    boolean32       server_listening;
    error_status_t  status;

    h = (struct db *) arg;

    gettimeofday(&now, &tz);

    while (true)
    {
        ru_sleep_until(&now, slive_c_long_wait);

        gettimeofday(&now, &tz);

        db_lock(h);

        ndeletes = 0;
        for (lp = db_list_first(&h->lists_mgmt, db_c_entry_list, NULL);
                lp != NULL; lp = lp_next)
        {
            /*
             *  Point to next entry in list now because
             *  may delete this entry and remove it from
             *  list
             */
            lp_next = db_list_next(db_c_entry_list, lp);

            entp = (db_entry_t *) lp;

            /*  If have done lots of deletes
             *  unlock db for a while so more
             *  important things can happen
             */
            if (ndeletes > slive_c_max_deletes)
            {
                ndeletes = 0;
                entp->read_nrefs++;
                db_unlock(h);

                ru_sleep(60);

                db_lock(h);
                entp->read_nrefs--;
            }

            if (entp->delete_flag)
            {
                if (entp->read_nrefs == 0)
                {
                    epdb_delete_entry(h, entp, &status);
                    ndeletes++;

                    if (dflag)
                        printf("sliv_task1 deleting server\n");
                }
            }
            else
            if (entp->ncomm_fails == 0)
            {
                entp->read_nrefs++;
                db_unlock(h);

                dcethread_checkinterrupt();

                server_listening = ping_server(entp, slive_c_short_comm_timeout, &status);

                db_lock(h);
                entp->read_nrefs--;

                if (!server_listening)
                {
                    entp->ncomm_fails++;
                    dcethread_cond_signal_throw(&h->sliv_task2_cv);
                }
            }

        }   /* end entry list loop */

        db_unlock(h);
    }
}

INTERNAL void sliv_task2(arg)
void    *arg;
{
    struct db       *h;
    struct timeval  now;
    struct timezone tz;
    struct timespec waketime;
    volatile unsigned32      waitsecs;
    volatile boolean32       have_db_lock;
    db_lists_t      *lp;
    db_entry_t      *entp;
    boolean32       server_listening;
    error_status_t  status;

    //DO_NOT_CLOBBER(waitsecs);
    //DO_NOT_CLOBBER(have_db_lock);

    h = (struct db *) arg;

    /*  let other init stuff get done */
    ru_sleep(180);

    gettimeofday(&now, &tz);
    waitsecs = slive_c_long_wait;

    db_lock(h);
    have_db_lock = true;

    DCETHREAD_TRY
    {
        while (true)
        {
			  int __istat;
            waketime.tv_sec = now.tv_sec + waitsecs + 1;
            waketime.tv_nsec = 0;

            /*  release lock and wait for task2 event or timeout or cancel
             */

				do	{
                                    __istat = dcethread_cond_timedwait_throw(&h->sliv_task2_cv, &h->lock, &waketime);
				} while(__istat == EINTR);

            /*  have lock now
             */

            gettimeofday(&now, &tz);
            waitsecs = slive_c_long_wait;   /* so far no bad servers */

            for (lp = db_list_first(&h->lists_mgmt, db_c_entry_list, NULL);
                    lp != NULL; lp = db_list_next(db_c_entry_list, lp))
            {
                entp = (db_entry_t *) lp;

                if ((entp->ncomm_fails > 0) && (!entp->delete_flag))
                {
                    entp->read_nrefs++;
                    have_db_lock = false;
                    db_unlock(h);

                    dcethread_checkinterrupt();

                    server_listening = ping_server(entp, rpc_c_binding_default_timeout, &status);

                    db_lock(h);
                    have_db_lock = true;
                    entp->read_nrefs--;

                    if (!server_listening)
                    {
                        waitsecs = slive_c_short_wait;
                        entp->ncomm_fails++;
                        if (entp->ncomm_fails >= slive_c_max_server_not_listening)
                        {
                            /*  Haven't communicated with server for
                             *  slive_c_max_server_not_listening consecutive tries
                             *  so mark entry as deleted in memory and on disk.
                             *  Task1 will actually purge entry from database
                             *  (it needs to do some extra bookkeeping so let's
                             *  keep that in one place)
                             */
                            entp->delete_flag = true;
                            db_update_entry(h, entp, &status);
                            if (dflag)
                                printf("sliv_task2 marking server for deletion\n");
                        }
                    }
                    else
                    {
                        entp->ncomm_fails = 0;
                    }
                }
            } /* end entry list loop */
        } /* end while loop */

    }   /* DCETHREAD_TRY */
    DCETHREAD_CATCH_ALL(THIS_CATCH)
    {
        /*  received cancel or some other exception.
         *  just unlock database and exit task
         */
        if (have_db_lock)
            db_unlock(h);
        DCETHREAD_RERAISE;
    }
    DCETHREAD_ENDTRY
}

INTERNAL boolean32 ping_server(entp, timeout, status)
db_entry_t      *entp;
unsigned32      timeout;
error_status_t  *status;
{
    rpc_binding_handle_t    binding_h;
    boolean32               is_listening;
    error_status_t          tmp_st;

    rpc_tower_to_binding(entp->tower.tower_octet_string, &binding_h, status);
    if (! STATUS_OK(status)) return(true);      /* what to do? */

    if (timeout != rpc_c_binding_default_timeout)
    {
        rpc_mgmt_set_com_timeout(binding_h, timeout, status);
    }

    is_listening = rpc_mgmt_is_server_listening(binding_h, status);

    rpc_binding_free(&binding_h, &tmp_st);

    return(is_listening);
}
