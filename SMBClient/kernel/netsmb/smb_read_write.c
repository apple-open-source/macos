/*
 * Copyright (c) 2018 Apple Inc.  All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

/*
 * This file is copied from nfs_upcall.c
 */


#include <stdint.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <libkern/libkern.h>
#include <libkern/OSAtomic.h>
#include <kern/debug.h>
#include <kern/thread.h>
#include <sys/systm.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <sys/kauth.h>

#include <sys/smb_apple.h>
#include <smbfs/smb_subr.h>
#include <netsmb/smb_read_write.h>
#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <smbfs/smbfs.h>

/* Enable these to turn on debug logging */
//#define SMB_RW_DEBUG 1
//#define SMB_RW_Q_DEBUG 1

#ifdef SMB_RW_DEBUG
#define DPRINT(fmt, ...) SMBERROR(fmt, ## __VA_ARGS__)
#else
#define DPRINT(fmt, ...)
#endif

#define SMB_RW_HASH(x) (x % smb_rw_thread_count)
#define SMB_RW_QUEUE_SLEEPING    0x0001

TAILQ_HEAD(smb_rw_q, smb_rw_arg);

/*
 * This code is similar to how my dog acts with my dog as a thread and her
 * food bowl as the work queue. If the food bowl is empty, my dog sleeps.
 * Once food is placed in her food bowl, my dog wakes up, eats the food, then
 * goes back to sleep waiting for more food.
 */
static struct smb_rw_queue {
	lck_mtx_t		*rwq_lock;
	struct smb_rw_q	rwq_queue[1];   /* Each dog has their own food bowl */
	thread_t		rwq_thd;
	uint32_t		rwq_flags;
} smb_rw_queue_tbl[SMB_RW_HASH_SZ];


lck_grp_t *smb_rw_group;
static lck_mtx_t *smb_rw_shutdown_lock;
static volatile int smb_rw_shutdown = 0;
static int32_t smb_rw_thread_count;
static atomic_uint_fast32_t round_robin_index = 0;

extern kern_return_t thread_terminate(thread_t);

#ifdef SMB_RW_Q_DEBUG
int smb_rw_use_proxy = 1;
uint32_t smb_rw_queue_limit;
uint32_t smb_rw_queue_max_seen;
volatile uint32_t smb_rw_queue_count;
#endif

static void smb_rw_start(void);


/*
 * Thread that dequeues works and processes the work
 */
static void
smb_rw_thread(void *arg, wait_result_t wr __unused)
{
	int qi = (int)(uintptr_t)arg;
	int error;
    struct smb_rw_arg *ep = NULL;
	struct smb_rw_queue *myqueue = &smb_rw_queue_tbl[qi];

	DPRINT("%d started\n", qi);
	while (!smb_rw_shutdown) {
		lck_mtx_lock(myqueue->rwq_lock);

        /* If no work, then sleep */
		while (!smb_rw_shutdown && TAILQ_EMPTY(myqueue->rwq_queue)) {
			myqueue->rwq_flags |= SMB_RW_QUEUE_SLEEPING;
			error = msleep(myqueue, myqueue->rwq_lock, PSOCK,
                           "smb_rw_handler idle", NULL);
			myqueue->rwq_flags &= ~SMB_RW_QUEUE_SLEEPING;
			if (error) {
				SMBERROR("msleep error %d\n", error);
			}
		}
        
        /* Are we shutting down now? */
		if (smb_rw_shutdown) {
			lck_mtx_unlock(myqueue->rwq_lock);
			break;
		}


        /* Dequeue the work and process it */
        ep = TAILQ_FIRST(myqueue->rwq_queue);
        DPRINT("%d dequeue %p from %p\n", qi, ep, myqueue);
        
        TAILQ_REMOVE(myqueue->rwq_queue, ep, sra_svcq);
        
        ep->flags &= ~SMB_RW_QUEUED;
        
        lck_mtx_unlock(myqueue->rwq_lock);
        
#ifdef SMB_RW_Q_DEBUG
        OSDecrementAtomic(&smb_rw_queue_count);
#endif

        SMB_LOG_KTRACE(SMB_DBG_SMB_RW_THREAD | DBG_FUNC_START,
                       /* channelID */ 0, qi, 0, 0, 0);

        /* Send the request and block waiting for reply */
        ep->error = smb_rq_simple(ep->rqp);

        SMB_LOG_KTRACE(SMB_DBG_SMB_RW_THREAD | DBG_FUNC_END,
                       /* channelID */ 0, qi, ep->error, 0, 0);

        /*
         * We now have the reply so wake up main thread which is waiting
         * for the reply
         */
        lck_mtx_lock(&ep->rw_arg_lock);
        ep->flags |= SMB_RW_REPLY_RCVD;
        wakeup(&ep->flags);
        lck_mtx_unlock(&ep->rw_arg_lock);
    }

    /* This thread is exiting */
	lck_mtx_lock(smb_rw_shutdown_lock);
	smb_rw_thread_count--;
	wakeup(&smb_rw_thread_count);
	lck_mtx_unlock(smb_rw_shutdown_lock);

	thread_terminate(current_thread());
}

/*
 * Allocate and initialize globals
 */
void
smb_rw_init(void)
{
	int i;

	smb_rw_group = lck_grp_alloc_init("smb_rw_locks", LCK_GRP_ATTR_NULL);
    
	for (i = 0; i < SMB_RW_HASH_SZ; i++) {
		TAILQ_INIT(smb_rw_queue_tbl[i].rwq_queue);
		smb_rw_queue_tbl[i].rwq_lock = lck_mtx_alloc_init(smb_rw_group, LCK_ATTR_NULL);
		smb_rw_queue_tbl[i].rwq_thd = THREAD_NULL;
		smb_rw_queue_tbl[i].rwq_flags = 0;
	}
    
	smb_rw_shutdown_lock = lck_mtx_alloc_init(smb_rw_group, LCK_ATTR_NULL);
    
    smb_rw_start();
}

/*
 * Start up worker threads. These global threads handle any large read/write
 * from any mounted smb shares.
 */
static void
smb_rw_start(void)
{
	int32_t i;
	int error;

#ifdef SMB_RW_Q_DEBUG
    if (!smb_rw_use_proxy) {
		return;
    }
#endif
	DPRINT("smb_rw_start\n");

	/* Wait until previous shutdown finishes */
	lck_mtx_lock(smb_rw_shutdown_lock);
	while (smb_rw_shutdown || smb_rw_thread_count > 0)
		msleep(&smb_rw_thread_count, smb_rw_shutdown_lock, PSOCK,
               "smb_rw_shutdown_wait", NULL);

	/* Start up worker threads */
	for (i = 0; i < SMB_RW_HASH_SZ; i++) {
		error = kernel_thread_start(smb_rw_thread, (void *)(uintptr_t)i, &smb_rw_queue_tbl[smb_rw_thread_count].rwq_thd);
		if (!error) {
			smb_rw_thread_count++;
		} else {
			SMBERROR("Could not start smb_rw_thread: %d\n", error);
		}
	}
	if (smb_rw_thread_count == 0) {
		SMBERROR("Could not start smb_rw_threads. Falling back\n");
		goto out;
	}

out:
#ifdef SMB_RW_Q_DEBUG
	smb_rw_queue_count = 0ULL;
	smb_rw_queue_max_seen = 0ULL;
#endif
	lck_mtx_unlock(smb_rw_shutdown_lock);
}

/*
 * Stop the worker threads.
 * Called from smb_rw_cleanup.
 */
static void
smb_rw_stop(void)
{
	int32_t i;
	int32_t thread_count = smb_rw_thread_count;

	DPRINT("Entering smb_rw_stop\n");

	/* Signal threads to stop */
	smb_rw_shutdown = 1;
	for (i = 0; i < thread_count; i++) {
		lck_mtx_lock(smb_rw_queue_tbl[i].rwq_lock);
		wakeup(&smb_rw_queue_tbl[i]);
		lck_mtx_unlock(smb_rw_queue_tbl[i].rwq_lock);
	}

	/* Wait until they are done shutting down */
	lck_mtx_lock(smb_rw_shutdown_lock);
    while (smb_rw_thread_count > 0) {
		msleep(&smb_rw_thread_count, smb_rw_shutdown_lock, PSOCK,
               "smb_rw_shutdown_stop", NULL);
    }

	/* Deallocate threads */
	for (i = 0; i < smb_rw_thread_count; i++) {
        if (smb_rw_queue_tbl[i].rwq_thd != THREAD_NULL) {
			thread_deallocate(smb_rw_queue_tbl[i].rwq_thd);
        }
		smb_rw_queue_tbl[i].rwq_thd = THREAD_NULL;
	}

	/* Enable restarting */
	smb_rw_shutdown = 0;
	lck_mtx_unlock(smb_rw_shutdown_lock);
}

/*
 * Shutdown the worker threads
 *     Make sure nothing is queued on the individual queues
 *	   Shutdown the threads
 */
void
smb_rw_cleanup(void)
{
	int i;

	DPRINT("Entering smb_rw_cleanup\n");

	/*
	 * Every thing should be dequeued at this point or will be as sockets are closed
	 * but to be safe, we'll make sure.
	 */
	for (i = 0; i < SMB_RW_HASH_SZ; i++) {
		struct smb_rw_queue *queue = &smb_rw_queue_tbl[i];

		lck_mtx_lock(queue->rwq_lock);
		while (!TAILQ_EMPTY(queue->rwq_queue)) {
			struct smb_rw_arg *ep = TAILQ_FIRST(queue->rwq_queue);
			TAILQ_REMOVE(queue->rwq_queue, ep, sra_svcq);
			ep->flags &= ~SMB_RW_QUEUED;
		}
		lck_mtx_unlock(queue->rwq_lock);
	}

	smb_rw_stop();

    /* Free the rwq locks */
    for (i = 0; i < SMB_RW_HASH_SZ; i++) {
        lck_mtx_free(smb_rw_queue_tbl[i].rwq_lock, smb_rw_group);
    }

    lck_mtx_free(smb_rw_shutdown_lock, smb_rw_group);

    lck_grp_free(smb_rw_group);
}

/*
 * Using a round robin method, pick a queue to enqueue the work onto and then
 * wake up that thread.
 */
void
smb_rw_proxy(void *arg)
{
	struct smb_rw_arg *uap = (struct smb_rw_arg *) arg;
    int qi = SMB_RW_HASH(atomic_fetch_add(&round_robin_index, 1));
    struct smb_rw_queue *myqueue = &smb_rw_queue_tbl[qi];

    lck_mtx_lock(myqueue->rwq_lock);
	DPRINT("called for %p\n", uap);
	DPRINT("\tRequest queued on %d for wakeup of %p\n", qi, myqueue);
	if (uap == NULL || uap->flags & SMB_RW_QUEUED) {
		lck_mtx_unlock(myqueue->rwq_lock);
        SMBERROR("Already queued or freed?\n");
		return;  /* Already queued or freed */
	}

	TAILQ_INSERT_TAIL(myqueue->rwq_queue, uap, sra_svcq);

	uap->flags |= SMB_RW_QUEUED;
    if (myqueue->rwq_flags & SMB_RW_QUEUE_SLEEPING) {
		wakeup(myqueue);
    }

#ifdef SMB_RW_Q_DEBUG
	{
		uint32_t count = OSIncrementAtomic(&smb_rw_queue_count);
	
		/* This is a bit racey but just for debug */
        if (count > smb_rw_queue_max_seen) {
			smb_rw_queue_max_seen = count;
        }

		if (smb_rw_queue_limit && count > smb_rw_queue_limit) {
			panic("smb_rw queue limit exceeded\n");
		}
	}
#endif
	lck_mtx_unlock(myqueue->rwq_lock);
}

