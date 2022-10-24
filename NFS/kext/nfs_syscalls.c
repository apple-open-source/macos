/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)nfs_syscalls.c	8.5 (Berkeley) 3/30/95
 * FreeBSD-Id: nfs_syscalls.c,v 1.32 1997/11/07 08:53:25 phk Exp $
 */

#include "nfs_client.h"
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2005 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>

#include <kern/kalloc.h>

#include <nfs/nfsmount.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs_lock.h>

kern_return_t   thread_terminate(thread_t); /* XXX */

/*
 * sysctl stuff
 */
struct nfs_sysctl_chain {
	struct sysctl_oid *oid;
	struct nfs_sysctl_chain *next;
};

struct nfs_sysctl_chain *sysctl_list = NULL;

#define NFS_SYSCTL(kind, parent, flags, name, ...)                     \
    SYSCTL_##kind(parent, flags, name, __VA_ARGS__);                   \
    struct nfs_sysctl_chain nfs_sysctl_##parent##_##name##_chain = {   \
	                .oid = &sysctl_##parent##_##name               \
    };                                                                 \
    static __attribute__((__constructor__)) void                       \
    nfs_sysctl_register_##parent##_##name(void) {                      \
	nfs_sysctl_##parent##_##name##_chain.next = sysctl_list;       \
	sysctl_list = &nfs_sysctl_##parent##_##name##_chain;           \
    }

SYSCTL_DECL(_vfs_generic);
SYSCTL_DECL(_vfs_generic_nfs);

NFS_SYSCTL(NODE, _vfs_generic_nfs, OID_AUTO, client, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "nfs client hinge");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, initialdowndelay, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_tprintf_initial_delay, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, nextdowndelay, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_tprintf_delay, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, iosize, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_iosize, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, access_cache_timeout, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_access_cache_timeout, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, allow_async, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_allow_async, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, statfs_rate_limit, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_statfs_rate_limit, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, nfsiod_thread_max, CTLFLAG_RW | CTLFLAG_LOCKED, &nfsiod_thread_max, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, nfsiod_thread_count, CTLFLAG_RD | CTLFLAG_LOCKED, &nfsiod_thread_count, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, lockd_mounts, CTLFLAG_RD | CTLFLAG_LOCKED, &nfs_lockd_mounts, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, max_async_writes, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_max_async_writes, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, access_delete, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_access_delete, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, access_dotzfs, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_access_dotzfs, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, access_for_getattr, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_access_for_getattr, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, idmap_ctrl, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_idmap_ctrl, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, callback_port, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_callback_port, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, is_mobile, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_is_mobile, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, squishy_flags, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_squishy_flags, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, split_open_owner, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_split_open_owner, 0, "");
NFS_SYSCTL(UINT, _vfs_generic_nfs_client, OID_AUTO, tcp_sockbuf, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_tcp_sockbuf, 0, "");
NFS_SYSCTL(UINT, _vfs_generic_nfs_client, OID_AUTO, debug_ctl, CTLFLAG_RW | CTLFLAG_LOCKED, &nfsclnt_debug_ctl, 0, "");
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, readlink_nocache, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_readlink_nocache, 0, "");
#if CONFIG_NFS_GSS
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, root_steals_gss_context, CTLFLAG_RW | CTLFLAG_LOCKED, &nfs_root_steals_ctx, 0, "");
#endif /* CONFIG_NFS_GSS */
#if CONFIG_NFS4
NFS_SYSCTL(STRING, _vfs_generic_nfs_client, OID_AUTO, default_nfs4domain, CTLFLAG_RW | CTLFLAG_LOCKED, nfs4_default_domain, sizeof(nfs4_default_domain), "");
#endif /* CONFIG_NFS4 */
NFS_SYSCTL(INT, _vfs_generic_nfs_client, OID_AUTO, uninterruptible_pagein, CTLFLAG_RW | CTLFLAG_LOCKED, &nfsclnt_nointr_pagein, 0, "");

void
nfs_sysctl_register(void)
{
	struct nfs_sysctl_chain *a = sysctl_list;
	while (a) {
		sysctl_register_oid(a->oid);
		a = a->next;
	}
}

void
nfs_sysctl_unregister(void)
{
	struct nfs_sysctl_chain *a = sysctl_list;
	while (a) {
		sysctl_unregister_oid(a->oid);
		a = a->next;
	}
}

/*
 * Asynchronous I/O threads for client NFS.
 * They do read-ahead and write-behind operations on the block I/O cache.
 *
 * The pool of up to nfsiod_thread_max threads is launched on demand and exit
 * when unused for a while.  There are as many nfsiod structs as there are
 * nfsiod threads; however there's no strict tie between a thread and a struct.
 * Each thread puts an nfsiod on the free list and sleeps on it.  When it wakes
 * up, it removes the next struct nfsiod from the queue and services it.  Then
 * it will put the struct at the head of free list and sleep on it.
 * Async requests will pull the next struct nfsiod from the head of the free list,
 * put it on the work queue, and wake whatever thread is waiting on that struct.
 */

/*
 * nfsiod thread exit routine
 *
 * Must be called with nfsiod_mutex held so that the
 * decision to terminate is atomic with the termination.
 */
void
nfsiod_terminate(struct nfsiod *niod)
{
	nfsiod_thread_count--;
	lck_mtx_unlock(get_lck_mtx(NLM_NFSIOD));
	if (niod) {
		kfree_type(struct nfsiod, niod);
	} else {
		printf("nfsiod: terminating without niod\n");
	}
	thread_terminate(current_thread());
	/*NOTREACHED*/
}

/* nfsiod thread startup routine */
void
nfsiod_thread(void)
{
	struct nfsiod *niod;
	int error;

	niod = kalloc_type(struct nfsiod, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	lck_mtx_lock(get_lck_mtx(NLM_NFSIOD));
	TAILQ_INSERT_HEAD(&nfsiodfree, niod, niod_link);
	wakeup(current_thread());
	error = msleep0(niod, get_lck_mtx(NLM_NFSIOD), PWAIT | PDROP, "nfsiod", NFS_ASYNCTHREADMAXIDLE * hz, nfsiod_continue);
	/* shouldn't return... so we have an error */
	/* remove an old nfsiod struct and terminate */
	lck_mtx_lock(get_lck_mtx(NLM_NFSIOD));
	if ((niod = TAILQ_LAST(&nfsiodfree, nfsiodlist))) {
		TAILQ_REMOVE(&nfsiodfree, niod, niod_link);
	}
	nfsiod_terminate(niod);
	/*NOTREACHED*/
}

/*
 * Start up another nfsiod thread.
 * (unless we're already maxed out and there are nfsiods running)
 */
int
nfsiod_start(void)
{
	thread_t thd = THREAD_NULL;

	lck_mtx_lock(get_lck_mtx(NLM_NFSIOD));
	if (((nfsiod_thread_count >= NFSIOD_MAX) && (nfsiod_thread_count > 0)) || unload_in_progress) {
		lck_mtx_unlock(get_lck_mtx(NLM_NFSIOD));
		return EBUSY;
	}
	nfsiod_thread_count++;
	if (kernel_thread_start((thread_continue_t)nfsiod_thread, NULL, &thd) != KERN_SUCCESS) {
		lck_mtx_unlock(get_lck_mtx(NLM_NFSIOD));
		return EBUSY;
	}
	/* wait for the thread to complete startup */
	msleep(thd, get_lck_mtx(NLM_NFSIOD), PWAIT | PDROP, "nfsiodw", NULL);
	thread_deallocate(thd);
	return 0;
}

/*
 * Continuation for Asynchronous I/O threads for NFS client.
 *
 * Grab an nfsiod struct to work on, do some work, then drop it
 */
int
nfsiod_continue(__unused int error)
{
	struct nfsiod *niod;
	struct nfsmount *nmp;
	struct nfsreq *req, *treq;
	struct nfs_reqqhead iodq;
	int morework;

	lck_mtx_lock(get_lck_mtx(NLM_NFSIOD));
	niod = TAILQ_FIRST(&nfsiodwork);
	if (!niod) {
		/* there's no work queued up */
		/* remove an old nfsiod struct and terminate */
		if ((niod = TAILQ_LAST(&nfsiodfree, nfsiodlist))) {
			TAILQ_REMOVE(&nfsiodfree, niod, niod_link);
		}
		nfsiod_terminate(niod);
		/*NOTREACHED*/
	}
	TAILQ_REMOVE(&nfsiodwork, niod, niod_link);

worktodo:
	while ((nmp = niod->niod_nmp)) {
		if (nmp == NULL) {
			niod->niod_nmp = NULL;
			break;
		}

		/*
		 * Service this mount's async I/O queue.
		 *
		 * In order to ensure some level of fairness between mounts,
		 * we grab all the work up front before processing it so any
		 * new work that arrives will be serviced on a subsequent
		 * iteration - and we have a chance to see if other work needs
		 * to be done (e.g. the delayed write queue needs to be pushed
		 * or other mounts are waiting for an nfsiod).
		 */
		/* grab the current contents of the queue */
		TAILQ_INIT(&iodq);
		TAILQ_CONCAT(&iodq, &nmp->nm_iodq, r_achain);
		/* Mark each iod request as being managed by an iod */
		TAILQ_FOREACH(req, &iodq, r_achain) {
			lck_mtx_lock(&req->r_mtx);
			assert(!(req->r_flags & R_IOD));
			req->r_flags |= R_IOD;
			lck_mtx_unlock(&req->r_mtx);
		}
		lck_mtx_unlock(get_lck_mtx(NLM_NFSIOD));

		/* process the queue */
		TAILQ_FOREACH_SAFE(req, &iodq, r_achain, treq) {
			TAILQ_REMOVE(&iodq, req, r_achain);
			req->r_achain.tqe_next = NFSREQNOLIST;
			req->r_callback.rcb_func(req);
		}

		/* now check if there's more/other work to be done */
		lck_mtx_lock(get_lck_mtx(NLM_NFSIOD));
		morework = !TAILQ_EMPTY(&nmp->nm_iodq);
		if (!morework || !TAILQ_EMPTY(&nfsiodmounts)) {
			/*
			 * we're going to stop working on this mount but if the
			 * mount still needs more work so queue it up
			 */
			if (morework && nmp->nm_iodlink.tqe_next == NFSNOLIST) {
				TAILQ_INSERT_TAIL(&nfsiodmounts, nmp, nm_iodlink);
			}
			nmp->nm_niod = NULL;
			niod->niod_nmp = NULL;
		}
	}

	/* loop if there's still a mount to work on */
	if (!niod->niod_nmp && !TAILQ_EMPTY(&nfsiodmounts)) {
		niod->niod_nmp = TAILQ_FIRST(&nfsiodmounts);
		TAILQ_REMOVE(&nfsiodmounts, niod->niod_nmp, nm_iodlink);
		niod->niod_nmp->nm_iodlink.tqe_next = NFSNOLIST;
	}
	if (niod->niod_nmp) {
		goto worktodo;
	}

	/* queue ourselves back up - if there aren't too many threads running */
	if (nfsiod_thread_count <= NFSIOD_MAX && !unload_in_progress) {
		TAILQ_INSERT_HEAD(&nfsiodfree, niod, niod_link);
		msleep0(niod, get_lck_mtx(NLM_NFSIOD), PWAIT | PDROP, "nfsiod", NFS_ASYNCTHREADMAXIDLE * hz, nfsiod_continue);
		/* shouldn't return... so we have an error */
		/* remove an old nfsiod struct and terminate */
		lck_mtx_lock(get_lck_mtx(NLM_NFSIOD));
		if ((niod = TAILQ_LAST(&nfsiodfree, nfsiodlist))) {
			TAILQ_REMOVE(&nfsiodfree, niod, niod_link);
		}
	}
	nfsiod_terminate(niod);
	/*NOTREACHED*/
	return 0;
}
