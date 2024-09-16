/*
 * Copyright (c) 2004-2020 Apple Inc. All rights reserved.
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
 * NOTICE: This file was modified by SPARTA, Inc. in 2005 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

/*
 * Kernel Authorization framework: Management of process/thread credentials
 * and identity information.
 */

#include <sys/param.h>  /* XXX trim includes */
#include <sys/acct.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/proc_internal.h>
#include <sys/user.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/malloc.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/sdt.h>

#include <security/audit/audit.h>

#include <sys/mount.h>
#include <sys/stat.h>   /* For manifest constants in posix_cred_access */
#include <sys/sysproto.h>
#include <mach/message.h>

#include <machine/atomic.h>
#include <libkern/OSByteOrder.h>

#include <kern/smr_hash.h>
#include <kern/task.h>
#include <kern/locks.h>
#ifdef MACH_ASSERT
# undef MACH_ASSERT
#endif
#define MACH_ASSERT 1   /* XXX so bogus */
#include <kern/assert.h>

#if CONFIG_MACF
#include <security/mac.h>
#include <security/mac_policy.h>
#include <security/mac_framework.h>
#include <security/_label.h>
#endif

#include <os/hash.h>
#include <IOKit/IOBSD.h>

/* Set to 1 to turn on KAUTH_DEBUG for kern_credential.c */
#if 0
#ifdef KAUTH_DEBUG
#undef KAUTH_DEBUG
#endif

#ifdef K_UUID_FMT
#undef K_UUID_FMT
#endif

#ifdef K_UUID_ARG
#undef K_UUID_ARG
#endif

# define K_UUID_FMT "%08x:%08x:%08x:%08x"
# define K_UUID_ARG(_u) &_u.g_guid_asint[0],&_u.g_guid_asint[1],&_u.g_guid_asint[2],&_u.g_guid_asint[3]
# define KAUTH_DEBUG(fmt, args...)      do { printf("%s:%d: " fmt "\n", __PRETTY_FUNCTION__, __LINE__ , ##args); } while (0)
#endif

#if CONFIG_EXT_RESOLVER
/*
 * Interface to external identity resolver.
 *
 * The architecture of the interface is simple; the external resolver calls
 * in to get work, then calls back with completed work.  It also calls us
 * to let us know that it's (re)started, so that we can resubmit work if it
 * times out.
 */

static LCK_MTX_DECLARE(kauth_resolver_mtx, &kauth_lck_grp);
#define KAUTH_RESOLVER_LOCK()   lck_mtx_lock(&kauth_resolver_mtx);
#define KAUTH_RESOLVER_UNLOCK() lck_mtx_unlock(&kauth_resolver_mtx);

static volatile pid_t   kauth_resolver_identity;
static int      kauth_identitysvc_has_registered;
static int      kauth_resolver_registered;
static uint32_t kauth_resolver_sequence = 31337;
static int      kauth_resolver_timeout = 30;    /* default: 30 seconds */

struct kauth_resolver_work {
	TAILQ_ENTRY(kauth_resolver_work) kr_link;
	struct kauth_identity_extlookup kr_work;
	uint64_t        kr_extend;
	uint32_t        kr_seqno;
	int             kr_refs;
	int             kr_flags;
#define KAUTH_REQUEST_UNSUBMITTED       (1<<0)
#define KAUTH_REQUEST_SUBMITTED         (1<<1)
#define KAUTH_REQUEST_DONE              (1<<2)
	int             kr_result;
};

TAILQ_HEAD(kauth_resolver_unsubmitted_head, kauth_resolver_work) kauth_resolver_unsubmitted =
    TAILQ_HEAD_INITIALIZER(kauth_resolver_unsubmitted);
TAILQ_HEAD(kauth_resolver_submitted_head, kauth_resolver_work) kauth_resolver_submitted =
    TAILQ_HEAD_INITIALIZER(kauth_resolver_submitted);
TAILQ_HEAD(kauth_resolver_done_head, kauth_resolver_work) kauth_resolver_done =
    TAILQ_HEAD_INITIALIZER(kauth_resolver_done);

/* Number of resolver timeouts between logged complaints */
#define KAUTH_COMPLAINT_INTERVAL 1000
int kauth_resolver_timeout_cnt = 0;

#if DEVELOPMENT || DEBUG
/* Internal builds get different (less ambiguous) breadcrumbs. */
#define KAUTH_RESOLVER_FAILED_ERRCODE   EOWNERDEAD
#else
/* But non-Internal builds get errors that are allowed by standards. */
#define KAUTH_RESOLVER_FAILED_ERRCODE   EIO
#endif /* DEVELOPMENT || DEBUG */

int kauth_resolver_failed_cnt = 0;
#define RESOLVER_FAILED_MESSAGE(fmt, args...)                           \
do {                                                                    \
	if (!(kauth_resolver_failed_cnt++ % 100)) {                     \
	        printf("%s: " fmt "\n", __PRETTY_FUNCTION__, ##args);   \
	}                                                               \
} while (0)

static int      kauth_resolver_submit(struct kauth_identity_extlookup *lkp, uint64_t extend_data);
static int      kauth_resolver_complete(user_addr_t message);
static int      kauth_resolver_getwork(user_addr_t message);
static int      kauth_resolver_getwork2(user_addr_t message);
static __attribute__((noinline)) int __KERNEL_IS_WAITING_ON_EXTERNAL_CREDENTIAL_RESOLVER__(
	struct kauth_resolver_work *);

#define KAUTH_CACHES_MAX_SIZE 10000 /* Max # entries for both groups and id caches */

struct kauth_identity {
	TAILQ_ENTRY(kauth_identity) ki_link;
	int     ki_valid;
	uid_t   ki_uid;
	gid_t   ki_gid;
	uint32_t         ki_supgrpcnt;
	gid_t   ki_supgrps[NGROUPS];
	guid_t  ki_guid;
	ntsid_t ki_ntsid;
	const char      *ki_name;       /* string name from string cache */
	/*
	 * Expiry times are the earliest time at which we will disregard the
	 * cached state and go to userland.  Before then if the valid bit is
	 * set, we will return the cached value.  If it's not set, we will
	 * not go to userland to resolve, just assume that there is no answer
	 * available.
	 */
	time_t  ki_groups_expiry;
	time_t  ki_guid_expiry;
	time_t  ki_ntsid_expiry;
};

static TAILQ_HEAD(kauth_identity_head, kauth_identity) kauth_identities =
    TAILQ_HEAD_INITIALIZER(kauth_identities);
static LCK_MTX_DECLARE(kauth_identity_mtx, &kauth_lck_grp);
#define KAUTH_IDENTITY_LOCK()   lck_mtx_lock(&kauth_identity_mtx);
#define KAUTH_IDENTITY_UNLOCK() lck_mtx_unlock(&kauth_identity_mtx);
#define KAUTH_IDENTITY_CACHEMAX_DEFAULT 100     /* XXX default sizing? */
static int kauth_identity_cachemax = KAUTH_IDENTITY_CACHEMAX_DEFAULT;
static int kauth_identity_count;

static struct kauth_identity *kauth_identity_alloc(uid_t uid, gid_t gid, guid_t *guidp, time_t guid_expiry,
    ntsid_t *ntsidp, time_t ntsid_expiry, size_t supgrpcnt, gid_t *supgrps, time_t groups_expiry,
    const char *name, int nametype);
static void     kauth_identity_register_and_free(struct kauth_identity *kip);
static void     kauth_identity_updatecache(struct kauth_identity_extlookup *elp, struct kauth_identity *kip, uint64_t extend_data);
static void     kauth_identity_trimcache(int newsize);
static void     kauth_identity_lru(struct kauth_identity *kip);
static int      kauth_identity_guid_expired(struct kauth_identity *kip);
static int      kauth_identity_ntsid_expired(struct kauth_identity *kip);
static int      kauth_identity_find_uid(uid_t uid, struct kauth_identity *kir, char *getname);
static int      kauth_identity_find_gid(gid_t gid, struct kauth_identity *kir, char *getname);
static int      kauth_identity_find_guid(guid_t *guidp, struct kauth_identity *kir, char *getname);
static int      kauth_identity_find_ntsid(ntsid_t *ntsid, struct kauth_identity *kir, char *getname);
static int      kauth_identity_find_nam(char *name, int valid, struct kauth_identity *kir);

struct kauth_group_membership {
	TAILQ_ENTRY(kauth_group_membership) gm_link;
	uid_t   gm_uid;         /* the identity whose membership we're recording */
	gid_t   gm_gid;         /* group of which they are a member */
	time_t  gm_expiry;      /* TTL for the membership, or 0 for persistent entries */
	int     gm_flags;
#define KAUTH_GROUP_ISMEMBER    (1<<0)
};

TAILQ_HEAD(kauth_groups_head, kauth_group_membership) kauth_groups =
    TAILQ_HEAD_INITIALIZER(kauth_groups);
static LCK_MTX_DECLARE(kauth_groups_mtx, &kauth_lck_grp);
#define KAUTH_GROUPS_LOCK()     lck_mtx_lock(&kauth_groups_mtx);
#define KAUTH_GROUPS_UNLOCK()   lck_mtx_unlock(&kauth_groups_mtx);
#define KAUTH_GROUPS_CACHEMAX_DEFAULT 100       /* XXX default sizing? */
static int kauth_groups_cachemax = KAUTH_GROUPS_CACHEMAX_DEFAULT;
static int kauth_groups_count;

static int      kauth_groups_expired(struct kauth_group_membership *gm);
static void     kauth_groups_lru(struct kauth_group_membership *gm);
static void     kauth_groups_updatecache(struct kauth_identity_extlookup *el);
static void     kauth_groups_trimcache(int newsize);

/*
 *  __KERNEL_IS_WAITING_ON_EXTERNAL_CREDENTIAL_RESOLVER__
 *
 * Description:  Waits for the user space daemon to respond to the request
 *               we made. Function declared non inline to be visible in
 *               stackshots and spindumps as well as debugging.
 *
 * Parameters:   workp                     Work queue entry.
 *
 * Returns:      0                         on Success.
 *               EIO                       if Resolver is dead.
 *               EINTR                     thread interrupted in msleep
 *               EWOULDBLOCK               thread timed out in msleep
 *               ERESTART                  returned by msleep.
 *
 */
static __attribute__((noinline)) int
__KERNEL_IS_WAITING_ON_EXTERNAL_CREDENTIAL_RESOLVER__(
	struct kauth_resolver_work  *workp)
{
	int error = 0;
	struct timespec ts;
	for (;;) {
		/* we could compute a better timeout here */
		ts.tv_sec = kauth_resolver_timeout;
		ts.tv_nsec = 0;
		error = msleep(workp, &kauth_resolver_mtx, PCATCH, "kr_submit", &ts);
		/* request has been completed? */
		if ((error == 0) && (workp->kr_flags & KAUTH_REQUEST_DONE)) {
			break;
		}
		/* woken because the resolver has died? */
		if (kauth_resolver_identity == 0) {
			RESOLVER_FAILED_MESSAGE("kauth external resolver died while while waiting for work to complete");
			error = KAUTH_RESOLVER_FAILED_ERRCODE;
			break;
		}
		/* an error? */
		if (error != 0) {
			break;
		}
	}
	return error;
}


/*
 * kauth_resolver_identity_reset
 *
 * Description: Reset the identity of the external resolver in certain
 *              controlled circumstances.
 *
 * Parameters:  None.
 *
 * Returns:     Nothing.
 */
void
kauth_resolver_identity_reset(void)
{
	KAUTH_RESOLVER_LOCK();
	if (kauth_resolver_identity != 0) {
		printf("kauth external resolver %d failed to de-register.\n",
		    kauth_resolver_identity);
		kauth_resolver_identity = 0;
		kauth_resolver_registered = 0;
	}
	KAUTH_RESOLVER_UNLOCK();
}

/*
 * kauth_resolver_submit
 *
 * Description:	Submit an external credential identity resolution request to
 *		the user space daemon.
 *
 * Parameters:	lkp				A pointer to an external
 *						lookup request
 *		extend_data			extended data for kr_extend
 *
 * Returns:	0				Success
 *		EWOULDBLOCK			No resolver registered
 *		EINTR				Operation interrupted (e.g. by
 *						a signal)
 *		ENOMEM				Could not allocate work item
 *	copyinstr:EFAULT			Bad message from user space
 *	workp->kr_result:???			An error from the user space
 *						daemon (includes ENOENT!)
 *
 * Implicit returns:
 *		*lkp				Modified
 *
 * Notes:	Allocate a work queue entry, submit the work and wait for
 *		the operation to either complete or time out.  Outstanding
 *		operations may also be cancelled.
 *
 *		Submission is by means of placing the item on a work queue
 *		which is serviced by an external resolver thread calling
 *		into the kernel.  The caller then sleeps until timeout,
 *		cancellation, or an external resolver thread calls in with
 *		a result message to kauth_resolver_complete().  All of these
 *		events wake the caller back up.
 *
 *		This code is called from either kauth_cred_ismember_gid()
 *		for a group membership request, or it is called from
 *		kauth_cred_cache_lookup() when we get a cache miss.
 */
static int
kauth_resolver_submit(struct kauth_identity_extlookup *lkp, uint64_t extend_data)
{
	struct kauth_resolver_work *workp, *killp;
	struct timespec ts;
	int     error, shouldfree;

	/* no point actually blocking if the resolver isn't up yet */
	if (kauth_resolver_identity == 0) {
		/*
		 * We've already waited an initial <kauth_resolver_timeout>
		 * seconds with no result.
		 *
		 * Sleep on a stack address so no one wakes us before timeout;
		 * we sleep a half a second in case we are a high priority
		 * process, so that memberd doesn't starve while we are in a
		 * tight loop between user and kernel, eating all the CPU.
		 */
		error = tsleep(&ts, PZERO | PCATCH, "kr_submit", hz / 2);
		if (kauth_resolver_identity == 0) {
			/*
			 * if things haven't changed while we were asleep,
			 * tell the caller we couldn't get an authoritative
			 * answer.
			 */
			return EWOULDBLOCK;
		}
	}

	workp = kalloc_type(struct kauth_resolver_work, Z_WAITOK | Z_NOFAIL);

	workp->kr_work = *lkp;
	workp->kr_extend = extend_data;
	workp->kr_refs = 1;
	workp->kr_flags = KAUTH_REQUEST_UNSUBMITTED;
	workp->kr_result = 0;

	/*
	 * We insert the request onto the unsubmitted queue, the call in from
	 * the resolver will it to the submitted thread when appropriate.
	 */
	KAUTH_RESOLVER_LOCK();
	workp->kr_seqno = workp->kr_work.el_seqno = kauth_resolver_sequence++;
	workp->kr_work.el_result = KAUTH_EXTLOOKUP_INPROG;

	/*
	 * XXX We *MUST NOT* attempt to coalesce identical work items due to
	 * XXX the inability to ensure order of update of the request item
	 * XXX extended data vs. the wakeup; instead, we let whoever is waiting
	 * XXX for each item repeat the update when they wake up.
	 */
	TAILQ_INSERT_TAIL(&kauth_resolver_unsubmitted, workp, kr_link);

	/*
	 * Wake up an external resolver thread to deal with the new work; one
	 * may not be available, and if not, then the request will be grabbed
	 * when a resolver thread comes back into the kernel to request new
	 * work.
	 */
	wakeup_one((caddr_t)&kauth_resolver_unsubmitted);
	error = __KERNEL_IS_WAITING_ON_EXTERNAL_CREDENTIAL_RESOLVER__(workp);

	/* if the request was processed, copy the result */
	if (error == 0) {
		*lkp = workp->kr_work;
	}

	if (error == EWOULDBLOCK) {
		if ((kauth_resolver_timeout_cnt++ % KAUTH_COMPLAINT_INTERVAL) == 0) {
			printf("kauth external resolver timed out (%d timeout(s) of %d seconds).\n",
			    kauth_resolver_timeout_cnt, kauth_resolver_timeout);
		}

		if (workp->kr_flags & KAUTH_REQUEST_UNSUBMITTED) {
			/*
			 * If the request timed out and was never collected, the resolver
			 * is dead and probably not coming back anytime soon.  In this
			 * case we revert to no-resolver behaviour, and punt all the other
			 * sleeping requests to clear the backlog.
			 */
			KAUTH_DEBUG("RESOLVER - request timed out without being collected for processing, resolver dead");

			/*
			 * Make the current resolver non-authoritative, and mark it as
			 * no longer registered to prevent kauth_cred_ismember_gid()
			 * enqueueing more work until a new one is registered.  This
			 * mitigates the damage a crashing resolver may inflict.
			 */
			kauth_resolver_identity = 0;
			kauth_resolver_registered = 0;

			/* kill all the other requestes that are waiting as well */
			TAILQ_FOREACH(killp, &kauth_resolver_submitted, kr_link)
			wakeup(killp);
			TAILQ_FOREACH(killp, &kauth_resolver_unsubmitted, kr_link)
			wakeup(killp);
			/* Cause all waiting-for-work threads to return EIO */
			wakeup((caddr_t)&kauth_resolver_unsubmitted);
		}
	}

	/*
	 * drop our reference on the work item, and note whether we should
	 * free it or not
	 */
	if (--workp->kr_refs <= 0) {
		/* work out which list we have to remove it from */
		if (workp->kr_flags & KAUTH_REQUEST_DONE) {
			TAILQ_REMOVE(&kauth_resolver_done, workp, kr_link);
		} else if (workp->kr_flags & KAUTH_REQUEST_SUBMITTED) {
			TAILQ_REMOVE(&kauth_resolver_submitted, workp, kr_link);
		} else if (workp->kr_flags & KAUTH_REQUEST_UNSUBMITTED) {
			TAILQ_REMOVE(&kauth_resolver_unsubmitted, workp, kr_link);
		} else {
			KAUTH_DEBUG("RESOLVER - completed request has no valid queue");
		}
		shouldfree = 1;
	} else {
		/* someone else still has a reference on this request */
		shouldfree = 0;
	}

	/* collect request result */
	if (error == 0) {
		error = workp->kr_result;
	}
	KAUTH_RESOLVER_UNLOCK();

	/*
	 * If we dropped the last reference, free the request.
	 */
	if (shouldfree) {
		kfree_type(struct kauth_resolver_work, workp);
	}

	KAUTH_DEBUG("RESOLVER - returning %d", error);
	return error;
}


/*
 * identitysvc
 *
 * Description:	System call interface for the external identity resolver.
 *
 * Parameters:	uap->message			Message from daemon to kernel
 *
 * Returns:	0				Successfully became resolver
 *		EPERM				Not the resolver process
 *	kauth_authorize_generic:EPERM		Not root user
 *	kauth_resolver_complete:EIO
 *	kauth_resolver_complete:EFAULT
 *	kauth_resolver_getwork:EINTR
 *	kauth_resolver_getwork:EFAULT
 *
 * Notes:	This system call blocks until there is work enqueued, at
 *		which time the kernel wakes it up, and a message from the
 *		kernel is copied out to the identity resolution daemon, which
 *		proceed to attempt to resolve it.  When the resolution has
 *		completed (successfully or not), the daemon called back into
 *		this system call to give the result to the kernel, and wait
 *		for the next request.
 */
int
identitysvc(__unused struct proc *p, struct identitysvc_args *uap, __unused int32_t *retval)
{
	int opcode = uap->opcode;
	user_addr_t message = uap->message;
	struct kauth_resolver_work *workp;
	struct kauth_cache_sizes sz_arg = {};
	int error;
	pid_t new_id;

	if (!IOCurrentTaskHasEntitlement(IDENTITYSVC_ENTITLEMENT)) {
		KAUTH_DEBUG("RESOLVER - pid %d not entitled to call identitysvc", proc_getpid(current_proc()));
		return EPERM;
	}

	/*
	 * New server registering itself.
	 */
	if (opcode == KAUTH_EXTLOOKUP_REGISTER) {
		new_id = proc_getpid(current_proc());
		if ((error = kauth_authorize_generic(kauth_cred_get(), KAUTH_GENERIC_ISSUSER)) != 0) {
			KAUTH_DEBUG("RESOLVER - pid %d refused permission to become identity resolver", new_id);
			return error;
		}
		KAUTH_RESOLVER_LOCK();
		if (kauth_resolver_identity != new_id) {
			KAUTH_DEBUG("RESOLVER - new resolver %d taking over from old %d", new_id, kauth_resolver_identity);
			/*
			 * We have a new server, so assume that all the old requests have been lost.
			 */
			while ((workp = TAILQ_LAST(&kauth_resolver_submitted, kauth_resolver_submitted_head)) != NULL) {
				TAILQ_REMOVE(&kauth_resolver_submitted, workp, kr_link);
				workp->kr_flags &= ~KAUTH_REQUEST_SUBMITTED;
				workp->kr_flags |= KAUTH_REQUEST_UNSUBMITTED;
				TAILQ_INSERT_HEAD(&kauth_resolver_unsubmitted, workp, kr_link);
			}
			/*
			 * Allow user space resolver to override the
			 * external resolution timeout
			 */
			if (message > 30 && message < 10000) {
				kauth_resolver_timeout = (int)message;
				KAUTH_DEBUG("RESOLVER - new resolver changes timeout to %d seconds\n", (int)message);
			}
			kauth_resolver_identity = new_id;
			kauth_resolver_registered = 1;
			kauth_identitysvc_has_registered = 1;
			wakeup(&kauth_resolver_unsubmitted);
		}
		KAUTH_RESOLVER_UNLOCK();
		return 0;
	}

	/*
	 * Beyond this point, we must be the resolver process. We verify this
	 * by confirming the resolver credential and pid.
	 */
	if ((kauth_cred_getuid(kauth_cred_get()) != 0) || (proc_getpid(current_proc()) != kauth_resolver_identity)) {
		KAUTH_DEBUG("RESOLVER - call from bogus resolver %d\n", proc_getpid(current_proc()));
		return EPERM;
	}

	if (opcode == KAUTH_GET_CACHE_SIZES) {
		KAUTH_IDENTITY_LOCK();
		sz_arg.kcs_id_size = kauth_identity_cachemax;
		KAUTH_IDENTITY_UNLOCK();

		KAUTH_GROUPS_LOCK();
		sz_arg.kcs_group_size = kauth_groups_cachemax;
		KAUTH_GROUPS_UNLOCK();

		if ((error = copyout(&sz_arg, uap->message, sizeof(sz_arg))) != 0) {
			return error;
		}

		return 0;
	} else if (opcode == KAUTH_SET_CACHE_SIZES) {
		if ((error = copyin(uap->message, &sz_arg, sizeof(sz_arg))) != 0) {
			return error;
		}

		if ((sz_arg.kcs_group_size > KAUTH_CACHES_MAX_SIZE) ||
		    (sz_arg.kcs_id_size > KAUTH_CACHES_MAX_SIZE)) {
			return EINVAL;
		}

		KAUTH_IDENTITY_LOCK();
		kauth_identity_cachemax = sz_arg.kcs_id_size;
		kauth_identity_trimcache(kauth_identity_cachemax);
		KAUTH_IDENTITY_UNLOCK();

		KAUTH_GROUPS_LOCK();
		kauth_groups_cachemax = sz_arg.kcs_group_size;
		kauth_groups_trimcache(kauth_groups_cachemax);
		KAUTH_GROUPS_UNLOCK();

		return 0;
	} else if (opcode == KAUTH_CLEAR_CACHES) {
		KAUTH_IDENTITY_LOCK();
		kauth_identity_trimcache(0);
		KAUTH_IDENTITY_UNLOCK();

		KAUTH_GROUPS_LOCK();
		kauth_groups_trimcache(0);
		KAUTH_GROUPS_UNLOCK();
	} else if (opcode == KAUTH_EXTLOOKUP_DEREGISTER) {
		/*
		 * Terminate outstanding requests; without an authoritative
		 * resolver, we are now back on our own authority.
		 */
		struct kauth_resolver_work *killp;

		KAUTH_RESOLVER_LOCK();

		/*
		 * Clear the identity, but also mark it as unregistered so
		 * there is no explicit future expectation of us getting a
		 * new resolver any time soon.
		 */
		kauth_resolver_identity = 0;
		kauth_resolver_registered = 0;

		TAILQ_FOREACH(killp, &kauth_resolver_submitted, kr_link)
		wakeup(killp);
		TAILQ_FOREACH(killp, &kauth_resolver_unsubmitted, kr_link)
		wakeup(killp);
		/* Cause all waiting-for-work threads to return EIO */
		wakeup((caddr_t)&kauth_resolver_unsubmitted);
		KAUTH_RESOLVER_UNLOCK();
	}

	/*
	 * Got a result returning?
	 */
	if (opcode & KAUTH_EXTLOOKUP_RESULT) {
		if ((error = kauth_resolver_complete(message)) != 0) {
			return error;
		}
	}

	/*
	 * Caller wants to take more work?
	 */
	if (opcode & KAUTH_EXTLOOKUP_WORKER) {
		if ((error = kauth_resolver_getwork(message)) != 0) {
			return error;
		}
	}

	return 0;
}


/*
 * kauth_resolver_getwork_continue
 *
 * Description:	Continuation for kauth_resolver_getwork
 *
 * Parameters:	result				Error code or 0 for the sleep
 *						that got us to this function
 *
 * Returns:	0				Success
 *		EINTR				Interrupted (e.g. by signal)
 *	kauth_resolver_getwork2:EFAULT
 *
 * Notes:	See kauth_resolver_getwork(0 and kauth_resolver_getwork2() for
 *		more information.
 */
static int
kauth_resolver_getwork_continue(int result)
{
	thread_t thread;
	struct uthread *ut;
	user_addr_t message;

	if (result) {
		KAUTH_RESOLVER_UNLOCK();
		return result;
	}

	/*
	 * If we lost a race with another thread/memberd restarting, then we
	 * need to go back to sleep to look for more work.  If it was memberd
	 * restarting, then the msleep0() will error out here, as our thread
	 * will already be "dead".
	 */
	if (TAILQ_FIRST(&kauth_resolver_unsubmitted) == NULL) {
		int error;

		error = msleep0(&kauth_resolver_unsubmitted, &kauth_resolver_mtx, PCATCH, "GRGetWork", 0, kauth_resolver_getwork_continue);
		/*
		 * If this is a wakeup from another thread in the resolver
		 * deregistering it, error out the request-for-work thread
		 */
		if (!kauth_resolver_identity) {
			RESOLVER_FAILED_MESSAGE("external resolver died");
			error = KAUTH_RESOLVER_FAILED_ERRCODE;
		}
		KAUTH_RESOLVER_UNLOCK();
		return error;
	}

	thread = current_thread();
	ut = get_bsdthread_info(thread);
	message = ut->uu_save.uus_kauth.message;
	return kauth_resolver_getwork2(message);
}


/*
 * kauth_resolver_getwork2
 *
 * Decription:	Common utility function to copy out a identity resolver work
 *		item from the kernel to user space as part of the user space
 *		identity resolver requesting work.
 *
 * Parameters:	message				message to user space
 *
 * Returns:	0				Success
 *		EFAULT				Bad user space message address
 *
 * Notes:	This common function exists to permit the use of continuations
 *		in the identity resolution process.  This frees up the stack
 *		while we are waiting for the user space resolver to complete
 *		a request.  This is specifically used so that our per thread
 *		cost can be small, and we will therefore be willing to run a
 *		larger number of threads in the user space identity resolver.
 */
static int
kauth_resolver_getwork2(user_addr_t message)
{
	struct kauth_resolver_work *workp;
	int             error;

	/*
	 * Note: We depend on the caller protecting us from a NULL work item
	 * queue, since we must have the kauth resolver lock on entry to this
	 * function.
	 */
	workp = TAILQ_FIRST(&kauth_resolver_unsubmitted);

	/*
	 * Copy out the external lookup structure for the request, not
	 * including the el_extend field, which contains the address of the
	 * external buffer provided by the external resolver into which we
	 * copy the extension request information.
	 */
	/* BEFORE FIELD */
	if ((error = copyout(&workp->kr_work, message, offsetof(struct kauth_identity_extlookup, el_extend))) != 0) {
		KAUTH_DEBUG("RESOLVER - error submitting work to resolve");
		goto out;
	}
	/* AFTER FIELD */
	if ((error = copyout(&workp->kr_work.el_info_reserved_1,
	    message + offsetof(struct kauth_identity_extlookup, el_info_reserved_1),
	    sizeof(struct kauth_identity_extlookup) - offsetof(struct kauth_identity_extlookup, el_info_reserved_1))) != 0) {
		KAUTH_DEBUG("RESOLVER - error submitting work to resolve");
		goto out;
	}

	/*
	 * Handle extended requests here; if we have a request of a type where
	 * the kernel wants a translation of extended information, then we need
	 * to copy it out into the extended buffer, assuming the buffer is
	 * valid; we only attempt to get the buffer address if we have request
	 * data to copy into it.
	 */

	/*
	 * translate a user@domain string into a uid/gid/whatever
	 */
	if (workp->kr_work.el_flags & (KAUTH_EXTLOOKUP_VALID_PWNAM | KAUTH_EXTLOOKUP_VALID_GRNAM)) {
		uint64_t uaddr;

		error = copyin(message + offsetof(struct kauth_identity_extlookup, el_extend), &uaddr, sizeof(uaddr));
		if (!error) {
			size_t actual;  /* not used */
			/*
			 * Use copyoutstr() to reduce the copy size; we let
			 * this catch a NULL uaddr because we shouldn't be
			 * asking in that case anyway.
			 */
			error = copyoutstr(CAST_DOWN(void *, workp->kr_extend), uaddr, MAXPATHLEN, &actual);
		}
		if (error) {
			KAUTH_DEBUG("RESOLVER - error submitting work to resolve");
			goto out;
		}
	}
	TAILQ_REMOVE(&kauth_resolver_unsubmitted, workp, kr_link);
	workp->kr_flags &= ~KAUTH_REQUEST_UNSUBMITTED;
	workp->kr_flags |= KAUTH_REQUEST_SUBMITTED;
	TAILQ_INSERT_TAIL(&kauth_resolver_submitted, workp, kr_link);

out:
	KAUTH_RESOLVER_UNLOCK();
	return error;
}


/*
 * kauth_resolver_getwork
 *
 * Description:	Get a work item from the enqueued requests from the kernel and
 *		give it to the user space daemon.
 *
 * Parameters:	message				message to user space
 *
 * Returns:	0				Success
 *		EINTR				Interrupted (e.g. by signal)
 *	kauth_resolver_getwork2:EFAULT
 *
 * Notes:	This function blocks in a continuation if there are no work
 *		items available for processing at the time the user space
 *		identity resolution daemon makes a request for work.  This
 *		permits a large number of threads to be used by the daemon,
 *		without using a lot of wired kernel memory when there are no
 *		actual request outstanding.
 */
static int
kauth_resolver_getwork(user_addr_t message)
{
	struct kauth_resolver_work *workp;
	int             error;

	KAUTH_RESOLVER_LOCK();
	error = 0;
	while ((workp = TAILQ_FIRST(&kauth_resolver_unsubmitted)) == NULL) {
		thread_t thread = current_thread();
		struct uthread *ut = get_bsdthread_info(thread);

		ut->uu_save.uus_kauth.message = message;
		error = msleep0(&kauth_resolver_unsubmitted, &kauth_resolver_mtx, PCATCH, "GRGetWork", 0, kauth_resolver_getwork_continue);
		KAUTH_RESOLVER_UNLOCK();
		/*
		 * If this is a wakeup from another thread in the resolver
		 * deregistering it, error out the request-for-work thread
		 */
		if (!kauth_resolver_identity) {
			printf("external resolver died");
			error = KAUTH_RESOLVER_FAILED_ERRCODE;
		}
		return error;
	}
	return kauth_resolver_getwork2(message);
}


/*
 * kauth_resolver_complete
 *
 * Description:	Return a result from userspace.
 *
 * Parameters:	message				message from user space
 *
 * Returns:	0				Success
 *		EIO				The resolver is dead
 *	copyin:EFAULT				Bad message from user space
 */
static int
kauth_resolver_complete(user_addr_t message)
{
	struct kauth_identity_extlookup extl;
	struct kauth_resolver_work *workp;
	struct kauth_resolver_work *killp;
	int error, result, want_extend_data;

	/*
	 * Copy in the mesage, including the extension field, since we are
	 * copying into a local variable.
	 */
	if ((error = copyin(message, &extl, sizeof(extl))) != 0) {
		KAUTH_DEBUG("RESOLVER - error getting completed work\n");
		return error;
	}

	KAUTH_RESOLVER_LOCK();

	error = 0;
	result = 0;
	switch (extl.el_result) {
	case KAUTH_EXTLOOKUP_INPROG:
	{
		static int once = 0;

		/* XXX this should go away once memberd is updated */
		if (!once) {
			printf("kauth_resolver: memberd is not setting valid result codes (assuming always successful)\n");
			once = 1;
		}
	}
		OS_FALLTHROUGH;

	case KAUTH_EXTLOOKUP_SUCCESS:
		break;

	case KAUTH_EXTLOOKUP_FATAL:
		/* fatal error means the resolver is dead */
		KAUTH_DEBUG("RESOLVER - resolver %d died, waiting for a new one", kauth_resolver_identity);
		RESOLVER_FAILED_MESSAGE("resolver %d died, waiting for a new one", kauth_resolver_identity);
		/*
		 * Terminate outstanding requests; without an authoritative
		 * resolver, we are now back on our own authority.  Tag the
		 * resolver unregistered to prevent kauth_cred_ismember_gid()
		 * enqueueing more work until a new one is registered.  This
		 * mitigates the damage a crashing resolver may inflict.
		 */
		kauth_resolver_identity = 0;
		kauth_resolver_registered = 0;

		TAILQ_FOREACH(killp, &kauth_resolver_submitted, kr_link)
		wakeup(killp);
		TAILQ_FOREACH(killp, &kauth_resolver_unsubmitted, kr_link)
		wakeup(killp);
		/* Cause all waiting-for-work threads to return EIO */
		wakeup((caddr_t)&kauth_resolver_unsubmitted);
		/* and return EIO to the caller */
		error = KAUTH_RESOLVER_FAILED_ERRCODE;
		break;

	case KAUTH_EXTLOOKUP_BADRQ:
		KAUTH_DEBUG("RESOLVER - resolver reported invalid request %d", extl.el_seqno);
		result = EINVAL;
		break;

	case KAUTH_EXTLOOKUP_FAILURE:
		KAUTH_DEBUG("RESOLVER - resolver reported transient failure for request %d", extl.el_seqno);
		RESOLVER_FAILED_MESSAGE("resolver reported transient failure for request %d", extl.el_seqno);
		result = KAUTH_RESOLVER_FAILED_ERRCODE;
		break;

	default:
		KAUTH_DEBUG("RESOLVER - resolver returned unexpected status %d", extl.el_result);
		RESOLVER_FAILED_MESSAGE("resolver returned unexpected status %d", extl.el_result);
		result = KAUTH_RESOLVER_FAILED_ERRCODE;
		break;
	}

	/*
	 * In the case of a fatal error, we assume that the resolver will
	 * restart quickly and re-collect all of the outstanding requests.
	 * Thus, we don't complete the request which returned the fatal
	 * error status.
	 */
	if (extl.el_result != KAUTH_EXTLOOKUP_FATAL) {
		/* scan our list for this request */
		TAILQ_FOREACH(workp, &kauth_resolver_submitted, kr_link) {
			/* found it? */
			if (workp->kr_seqno == extl.el_seqno) {
				/*
				 * Do we want extend_data?
				 */
				want_extend_data = (workp->kr_work.el_flags & (KAUTH_EXTLOOKUP_WANT_PWNAM | KAUTH_EXTLOOKUP_WANT_GRNAM));

				/*
				 * Get the request of the submitted queue so
				 * that it is not cleaned up out from under
				 * us by a timeout.
				 */
				TAILQ_REMOVE(&kauth_resolver_submitted, workp, kr_link);
				workp->kr_flags &= ~KAUTH_REQUEST_SUBMITTED;
				workp->kr_flags |= KAUTH_REQUEST_DONE;
				workp->kr_result = result;

				/* Copy the result message to the work item. */
				memcpy(&workp->kr_work, &extl, sizeof(struct kauth_identity_extlookup));

				/*
				 * Check if we have a result in the extension
				 * field; if we do, then we need to separately
				 * copy the data from the message el_extend
				 * into the request buffer that's in the work
				 * item.  We have to do it here because we do
				 * not want to wake up the waiter until the
				 * data is in their buffer, and because the
				 * actual request response may be destroyed
				 * by the time the requester wakes up, and they
				 * do not have access to the user space buffer
				 * address.
				 *
				 * It is safe to drop and reacquire the lock
				 * here because we've already removed the item
				 * from the submission queue, but have not yet
				 * moved it to the completion queue.  Note that
				 * near simultaneous requests may result in
				 * duplication of requests for items in this
				 * window. This should not be a performance
				 * issue and is easily detectable by comparing
				 * time to live on last response vs. time of
				 * next request in the resolver logs.
				 *
				 * A malicious/faulty resolver could overwrite
				 * part of a user's address space if they return
				 * flags that mismatch the original request's flags.
				 */
				if (want_extend_data && (extl.el_flags & (KAUTH_EXTLOOKUP_VALID_PWNAM | KAUTH_EXTLOOKUP_VALID_GRNAM))) {
					size_t actual;  /* notused */

					KAUTH_RESOLVER_UNLOCK();
					error = copyinstr(extl.el_extend, CAST_DOWN(void *, workp->kr_extend), MAXPATHLEN, &actual);
					KAUTH_DEBUG("RESOLVER - resolver got name :%*s: len = %d\n", (int)actual,
					    actual ? "null" : (char *)extl.el_extend, actual);
					KAUTH_RESOLVER_LOCK();
				} else if (extl.el_flags &  (KAUTH_EXTLOOKUP_VALID_PWNAM | KAUTH_EXTLOOKUP_VALID_GRNAM)) {
					error = EFAULT;
					KAUTH_DEBUG("RESOLVER - resolver returned mismatching extension flags (%d), request contained (%d)",
					    extl.el_flags, want_extend_data);
				}

				/*
				 * Move the completed work item to the
				 * completion queue and wake up requester(s)
				 */
				TAILQ_INSERT_TAIL(&kauth_resolver_done, workp, kr_link);
				wakeup(workp);
				break;
			}
		}
	}
	/*
	 * Note that it's OK for us not to find anything; if the request has
	 * timed out the work record will be gone.
	 */
	KAUTH_RESOLVER_UNLOCK();

	return error;
}
#endif /* CONFIG_EXT_RESOLVER */


/*
 * Identity cache.
 */

#define KI_VALID_UID    (1<<0)          /* UID and GID are mutually exclusive */
#define KI_VALID_GID    (1<<1)
#define KI_VALID_GUID   (1<<2)
#define KI_VALID_NTSID  (1<<3)
#define KI_VALID_PWNAM  (1<<4)  /* Used for translation */
#define KI_VALID_GRNAM  (1<<5)  /* Used for translation */
#define KI_VALID_GROUPS (1<<6)

#if CONFIG_EXT_RESOLVER
/*
 * kauth_identity_alloc
 *
 * Description:	Allocate and fill out a kauth_identity structure for
 *		translation between {UID|GID}/GUID/NTSID
 *
 * Parameters:	uid
 *
 * Returns:	NULL				Insufficient memory to satisfy
 *						the request or bad parameters
 *		!NULL				A pointer to the allocated
 *						structure, filled in
 *
 * Notes:	It is illegal to translate between UID and GID; any given UUID
 *		or NTSID can only refer to an NTSID or UUID (respectively),
 *		and *either* a UID *or* a GID, but not both.
 */
static struct kauth_identity *
kauth_identity_alloc(uid_t uid, gid_t gid, guid_t *guidp, time_t guid_expiry,
    ntsid_t *ntsidp, time_t ntsid_expiry, size_t supgrpcnt, gid_t *supgrps, time_t groups_expiry,
    const char *name, int nametype)
{
	struct kauth_identity *kip;

	/* get and fill in a new identity */
	kip = kalloc_type(struct kauth_identity, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	if (gid != KAUTH_GID_NONE) {
		kip->ki_gid = gid;
		kip->ki_valid = KI_VALID_GID;
	}
	if (uid != KAUTH_UID_NONE) {
		if (kip->ki_valid & KI_VALID_GID) {
			panic("can't allocate kauth identity with both uid and gid");
		}
		kip->ki_uid = uid;
		kip->ki_valid = KI_VALID_UID;
	}
	if (supgrpcnt) {
		/*
		 * A malicious/faulty resolver could return bad values
		 */
		assert(supgrpcnt <= NGROUPS);
		assert(supgrps != NULL);

		if ((supgrpcnt > NGROUPS) || (supgrps == NULL)) {
			return NULL;
		}
		if (kip->ki_valid & KI_VALID_GID) {
			panic("can't allocate kauth identity with both gid and supplementary groups");
		}
		kip->ki_supgrpcnt = (uint32_t)supgrpcnt;
		memcpy(kip->ki_supgrps, supgrps, sizeof(supgrps[0]) * supgrpcnt);
		kip->ki_valid |= KI_VALID_GROUPS;
	}
	kip->ki_groups_expiry = groups_expiry;
	if (guidp != NULL) {
		kip->ki_guid = *guidp;
		kip->ki_valid |= KI_VALID_GUID;
	}
	kip->ki_guid_expiry = guid_expiry;
	if (ntsidp != NULL) {
		kip->ki_ntsid = *ntsidp;
		kip->ki_valid |= KI_VALID_NTSID;
	}
	kip->ki_ntsid_expiry = ntsid_expiry;
	if (name != NULL) {
		kip->ki_name = name;
		kip->ki_valid |= nametype;
	}
	return kip;
}


/*
 * kauth_identity_register_and_free
 *
 * Description:	Register an association between identity tokens.  The passed
 *		'kip' is consumed by this function.
 *
 * Parameters:	kip				Pointer to kauth_identity
 *						structure to register
 *
 * Returns:	(void)
 *
 * Notes:	The memory pointer to by 'kip' is assumed to have been
 *		previously allocated via kauth_identity_alloc().
 */
static void
kauth_identity_register_and_free(struct kauth_identity *kip)
{
	struct kauth_identity *ip;

	/*
	 * We search the cache for the UID listed in the incoming association.
	 * If we already have an entry, the new information is merged.
	 */
	ip = NULL;
	KAUTH_IDENTITY_LOCK();
	if (kip->ki_valid & KI_VALID_UID) {
		if (kip->ki_valid & KI_VALID_GID) {
			panic("kauth_identity: can't insert record with both UID and GID as key");
		}
		TAILQ_FOREACH(ip, &kauth_identities, ki_link)
		if ((ip->ki_valid & KI_VALID_UID) && (ip->ki_uid == kip->ki_uid)) {
			break;
		}
	} else if (kip->ki_valid & KI_VALID_GID) {
		TAILQ_FOREACH(ip, &kauth_identities, ki_link)
		if ((ip->ki_valid & KI_VALID_GID) && (ip->ki_gid == kip->ki_gid)) {
			break;
		}
	} else {
		panic("kauth_identity: can't insert record without UID or GID as key");
	}

	if (ip != NULL) {
		/* we already have an entry, merge/overwrite */
		if (kip->ki_valid & KI_VALID_GUID) {
			ip->ki_guid = kip->ki_guid;
			ip->ki_valid |= KI_VALID_GUID;
		}
		ip->ki_guid_expiry = kip->ki_guid_expiry;
		if (kip->ki_valid & KI_VALID_NTSID) {
			ip->ki_ntsid = kip->ki_ntsid;
			ip->ki_valid |= KI_VALID_NTSID;
		}
		ip->ki_ntsid_expiry = kip->ki_ntsid_expiry;
		/* a valid ki_name field overwrites the previous name field */
		if (kip->ki_valid & (KI_VALID_PWNAM | KI_VALID_GRNAM)) {
			/* if there's an old one, discard it */
			const char *oname = NULL;
			if (ip->ki_valid & (KI_VALID_PWNAM | KI_VALID_GRNAM)) {
				oname = ip->ki_name;
			}
			ip->ki_name = kip->ki_name;
			kip->ki_name = oname;
		}
		/* and discard the incoming entry */
		ip = kip;
	} else {
		/*
		 * if we don't have any information on this identity, add it;
		 * if it pushes us over our limit, discard the oldest one.
		 */
		TAILQ_INSERT_HEAD(&kauth_identities, kip, ki_link);
		if (++kauth_identity_count > kauth_identity_cachemax) {
			ip = TAILQ_LAST(&kauth_identities, kauth_identity_head);
			TAILQ_REMOVE(&kauth_identities, ip, ki_link);
			kauth_identity_count--;
		}
	}
	KAUTH_IDENTITY_UNLOCK();
	/* have to drop lock before freeing expired entry (it may be in use) */
	if (ip != NULL) {
		/* if the ki_name field is used, clear it first */
		if (ip->ki_valid & (KI_VALID_PWNAM | KI_VALID_GRNAM)) {
			vfs_removename(ip->ki_name);
		}
		/* free the expired entry */
		kfree_type(struct kauth_identity, ip);
	}
}


/*
 * kauth_identity_updatecache
 *
 * Description:	Given a lookup result, add any associations that we don't
 *		currently have; replace ones which have changed.
 *
 * Parameters:	elp				External lookup result from
 *						user space daemon to kernel
 *		rkip				pointer to returned kauth
 *						identity, or NULL
 *		extend_data			Extended data (can vary)
 *
 * Returns:	(void)
 *
 * Implicit returns:
 *		*rkip				Modified (if non-NULL)
 *
 * Notes:	For extended information requests, this code relies on the fact
 *		that elp->el_flags is never used as an rvalue, and is only
 *		ever bit-tested for valid lookup information we are willing
 *		to cache.
 *
 * XXX:		We may have to do the same in the case that extended data was
 *		passed out to user space to ensure that the request string
 *		gets cached; we may also be able to use the rkip as an
 *		input to avoid this.  The jury is still out.
 *
 * XXX:		This codes performance could be improved for multiple valid
 *		results by combining the loop iteration in a single loop.
 */
static void
kauth_identity_updatecache(struct kauth_identity_extlookup *elp, struct kauth_identity *rkip, uint64_t extend_data)
{
	struct timeval tv;
	struct kauth_identity *kip;
	const char *speculative_name = NULL;

	microuptime(&tv);

	/*
	 * If there is extended data, and that data represents a name rather
	 * than something else, speculatively create an entry for it in the
	 * string cache.  We do this to avoid holding the KAUTH_IDENTITY_LOCK
	 * over the allocation later.
	 */
	if (elp->el_flags & (KAUTH_EXTLOOKUP_VALID_PWNAM | KAUTH_EXTLOOKUP_VALID_GRNAM)) {
		const char *tmp = CAST_DOWN(const char *, extend_data);
		speculative_name = vfs_addname(tmp, (uint32_t)strnlen(tmp, MAXPATHLEN - 1), 0, 0);
	}

	/* user identity? */
	if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_UID) {
		KAUTH_IDENTITY_LOCK();
		TAILQ_FOREACH(kip, &kauth_identities, ki_link) {
			/* matching record */
			if ((kip->ki_valid & KI_VALID_UID) && (kip->ki_uid == elp->el_uid)) {
				if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_SUPGRPS) {
					assert(elp->el_sup_grp_cnt <= NGROUPS);
					if (elp->el_sup_grp_cnt > NGROUPS) {
						KAUTH_DEBUG("CACHE - invalid sup_grp_cnt provided (%d), truncating to  %d",
						    elp->el_sup_grp_cnt, NGROUPS);
						elp->el_sup_grp_cnt = NGROUPS;
					}
					kip->ki_supgrpcnt = elp->el_sup_grp_cnt;
					memcpy(kip->ki_supgrps, elp->el_sup_groups, sizeof(elp->el_sup_groups[0]) * kip->ki_supgrpcnt);
					kip->ki_valid |= KI_VALID_GROUPS;
					kip->ki_groups_expiry = (elp->el_member_valid) ? tv.tv_sec + elp->el_member_valid : 0;
				}
				if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_UGUID) {
					kip->ki_guid = elp->el_uguid;
					kip->ki_valid |= KI_VALID_GUID;
				}
				kip->ki_guid_expiry = (elp->el_uguid_valid) ? tv.tv_sec + elp->el_uguid_valid : 0;
				if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_USID) {
					kip->ki_ntsid = elp->el_usid;
					kip->ki_valid |= KI_VALID_NTSID;
				}
				kip->ki_ntsid_expiry = (elp->el_usid_valid) ? tv.tv_sec + elp->el_usid_valid : 0;
				if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_PWNAM) {
					const char *oname = kip->ki_name;
					kip->ki_name = speculative_name;
					speculative_name = NULL;
					kip->ki_valid |= KI_VALID_PWNAM;
					if (oname) {
						/*
						 * free oname (if any) outside
						 * the lock
						 */
						speculative_name = oname;
					}
				}
				kauth_identity_lru(kip);
				if (rkip != NULL) {
					*rkip = *kip;
				}
				KAUTH_DEBUG("CACHE - refreshed %d is " K_UUID_FMT, kip->ki_uid, K_UUID_ARG(kip->ki_guid));
				break;
			}
		}
		KAUTH_IDENTITY_UNLOCK();
		/* not found in cache, add new record */
		if (kip == NULL) {
			kip = kauth_identity_alloc(elp->el_uid, KAUTH_GID_NONE,
			    (elp->el_flags & KAUTH_EXTLOOKUP_VALID_UGUID) ? &elp->el_uguid : NULL,
			    (elp->el_uguid_valid) ? tv.tv_sec + elp->el_uguid_valid : 0,
			    (elp->el_flags & KAUTH_EXTLOOKUP_VALID_USID) ? &elp->el_usid : NULL,
			    (elp->el_usid_valid) ? tv.tv_sec + elp->el_usid_valid : 0,
			    (elp->el_flags & KAUTH_EXTLOOKUP_VALID_SUPGRPS) ? elp->el_sup_grp_cnt : 0,
			    (elp->el_flags & KAUTH_EXTLOOKUP_VALID_SUPGRPS) ? elp->el_sup_groups : NULL,
			    (elp->el_member_valid) ? tv.tv_sec + elp->el_member_valid : 0,
			    (elp->el_flags & KAUTH_EXTLOOKUP_VALID_PWNAM) ? speculative_name : NULL,
			    KI_VALID_PWNAM);
			if (kip != NULL) {
				if (rkip != NULL) {
					*rkip = *kip;
				}
				if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_PWNAM) {
					speculative_name = NULL;
				}
				KAUTH_DEBUG("CACHE - learned %d is " K_UUID_FMT, kip->ki_uid, K_UUID_ARG(kip->ki_guid));
				kauth_identity_register_and_free(kip);
			}
		}
	}

	/* group identity? (ignore, if we already processed it as a user) */
	if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_GID && !(elp->el_flags & KAUTH_EXTLOOKUP_VALID_UID)) {
		KAUTH_IDENTITY_LOCK();
		TAILQ_FOREACH(kip, &kauth_identities, ki_link) {
			/* matching record */
			if ((kip->ki_valid & KI_VALID_GID) && (kip->ki_gid == elp->el_gid)) {
				if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_GGUID) {
					kip->ki_guid = elp->el_gguid;
					kip->ki_valid |= KI_VALID_GUID;
				}
				kip->ki_guid_expiry = (elp->el_gguid_valid) ? tv.tv_sec + elp->el_gguid_valid : 0;
				if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_GSID) {
					kip->ki_ntsid = elp->el_gsid;
					kip->ki_valid |= KI_VALID_NTSID;
				}
				kip->ki_ntsid_expiry = (elp->el_gsid_valid) ? tv.tv_sec + elp->el_gsid_valid : 0;
				if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_GRNAM) {
					const char *oname = kip->ki_name;
					kip->ki_name = speculative_name;
					speculative_name = NULL;
					kip->ki_valid |= KI_VALID_GRNAM;
					if (oname) {
						/*
						 * free oname (if any) outside
						 * the lock
						 */
						speculative_name = oname;
					}
				}
				kauth_identity_lru(kip);
				if (rkip != NULL) {
					*rkip = *kip;
				}
				KAUTH_DEBUG("CACHE - refreshed %d is " K_UUID_FMT, kip->ki_uid, K_UUID_ARG(kip->ki_guid));
				break;
			}
		}
		KAUTH_IDENTITY_UNLOCK();
		/* not found in cache, add new record */
		if (kip == NULL) {
			kip = kauth_identity_alloc(KAUTH_UID_NONE, elp->el_gid,
			    (elp->el_flags & KAUTH_EXTLOOKUP_VALID_GGUID) ? &elp->el_gguid : NULL,
			    (elp->el_gguid_valid) ? tv.tv_sec + elp->el_gguid_valid : 0,
			    (elp->el_flags & KAUTH_EXTLOOKUP_VALID_GSID) ? &elp->el_gsid : NULL,
			    (elp->el_gsid_valid) ? tv.tv_sec + elp->el_gsid_valid : 0,
			    (elp->el_flags & KAUTH_EXTLOOKUP_VALID_SUPGRPS) ? elp->el_sup_grp_cnt : 0,
			    (elp->el_flags & KAUTH_EXTLOOKUP_VALID_SUPGRPS) ? elp->el_sup_groups : NULL,
			    (elp->el_member_valid) ? tv.tv_sec + elp->el_member_valid : 0,
			    (elp->el_flags & KAUTH_EXTLOOKUP_VALID_GRNAM) ? speculative_name : NULL,
			    KI_VALID_GRNAM);
			if (kip != NULL) {
				if (rkip != NULL) {
					*rkip = *kip;
				}
				if (elp->el_flags & KAUTH_EXTLOOKUP_VALID_GRNAM) {
					speculative_name = NULL;
				}
				KAUTH_DEBUG("CACHE - learned %d is " K_UUID_FMT, kip->ki_uid, K_UUID_ARG(kip->ki_guid));
				kauth_identity_register_and_free(kip);
			}
		}
	}

	/* If we have a name reference to drop, drop it here */
	if (speculative_name != NULL) {
		vfs_removename(speculative_name);
	}
}


/*
 * Trim older entries from the identity cache.
 *
 * Must be called with the identity cache lock held.
 */
static void
kauth_identity_trimcache(int newsize)
{
	struct kauth_identity           *kip;

	lck_mtx_assert(&kauth_identity_mtx, LCK_MTX_ASSERT_OWNED);

	while (kauth_identity_count > newsize) {
		kip = TAILQ_LAST(&kauth_identities, kauth_identity_head);
		TAILQ_REMOVE(&kauth_identities, kip, ki_link);
		kauth_identity_count--;
		kfree_type(struct kauth_identity, kip);
	}
}

/*
 * kauth_identity_lru
 *
 * Description:	Promote the entry to the head of the LRU, assumes the cache
 *		is locked.
 *
 * Parameters:	kip				kauth identity to move to the
 *						head of the LRU list, if it's
 *						not already there
 *
 * Returns:	(void)
 *
 * Notes:	This is called even if the entry has expired; typically an
 *		expired entry that's been looked up is about to be revalidated,
 *		and having it closer to the head of the LRU means finding it
 *		quickly again when the revalidation comes through.
 */
static void
kauth_identity_lru(struct kauth_identity *kip)
{
	if (kip != TAILQ_FIRST(&kauth_identities)) {
		TAILQ_REMOVE(&kauth_identities, kip, ki_link);
		TAILQ_INSERT_HEAD(&kauth_identities, kip, ki_link);
	}
}


/*
 * kauth_identity_guid_expired
 *
 * Description:	Handle lazy expiration of GUID translations.
 *
 * Parameters:	kip				kauth identity to check for
 *						GUID expiration
 *
 * Returns:	1				Expired
 *		0				Not expired
 */
static int
kauth_identity_guid_expired(struct kauth_identity *kip)
{
	struct timeval tv;

	/*
	 * Expiration time of 0 means this entry is persistent.
	 */
	if (kip->ki_guid_expiry == 0) {
		return 0;
	}

	microuptime(&tv);
	KAUTH_DEBUG("CACHE - GUID expires @ %ld now %ld", kip->ki_guid_expiry, tv.tv_sec);

	return (kip->ki_guid_expiry <= tv.tv_sec) ? 1 : 0;
}


/*
 * kauth_identity_ntsid_expired
 *
 * Description:	Handle lazy expiration of NTSID translations.
 *
 * Parameters:	kip				kauth identity to check for
 *						NTSID expiration
 *
 * Returns:	1				Expired
 *		0				Not expired
 */
static int
kauth_identity_ntsid_expired(struct kauth_identity *kip)
{
	struct timeval tv;

	/*
	 * Expiration time of 0 means this entry is persistent.
	 */
	if (kip->ki_ntsid_expiry == 0) {
		return 0;
	}

	microuptime(&tv);
	KAUTH_DEBUG("CACHE - NTSID expires @ %ld now %ld", kip->ki_ntsid_expiry, tv.tv_sec);

	return (kip->ki_ntsid_expiry <= tv.tv_sec) ? 1 : 0;
}

/*
 * kauth_identity_groups_expired
 *
 * Description:	Handle lazy expiration of supplemental group translations.
 *
 * Parameters:	kip				kauth identity to check for
 *						groups expiration
 *
 * Returns:	1				Expired
 *		0				Not expired
 */
static int
kauth_identity_groups_expired(struct kauth_identity *kip)
{
	struct timeval tv;

	/*
	 * Expiration time of 0 means this entry is persistent.
	 */
	if (kip->ki_groups_expiry == 0) {
		return 0;
	}

	microuptime(&tv);
	KAUTH_DEBUG("CACHE - GROUPS expires @ %ld now %ld\n", kip->ki_groups_expiry, tv.tv_sec);

	return (kip->ki_groups_expiry <= tv.tv_sec) ? 1 : 0;
}

/*
 * kauth_identity_find_uid
 *
 * Description: Search for an entry by UID
 *
 * Parameters:	uid				UID to find
 *		kir				Pointer to return area
 *		getname				Name buffer, if ki_name wanted
 *
 * Returns:	0				Found
 *		ENOENT				Not found
 *
 * Implicit returns:
 *		*klr				Modified, if found
 */
static int
kauth_identity_find_uid(uid_t uid, struct kauth_identity *kir, char *getname)
{
	struct kauth_identity *kip;

	KAUTH_IDENTITY_LOCK();
	TAILQ_FOREACH(kip, &kauth_identities, ki_link) {
		if ((kip->ki_valid & KI_VALID_UID) && (uid == kip->ki_uid)) {
			kauth_identity_lru(kip);
			/* Copy via structure assignment */
			*kir = *kip;
			/* If a name is wanted and one exists, copy it out */
			if (getname != NULL && (kip->ki_valid & (KI_VALID_PWNAM | KI_VALID_GRNAM))) {
				strlcpy(getname, kip->ki_name, MAXPATHLEN);
			}
			break;
		}
	}
	KAUTH_IDENTITY_UNLOCK();
	return (kip == NULL) ? ENOENT : 0;
}


/*
 * kauth_identity_find_gid
 *
 * Description: Search for an entry by GID
 *
 * Parameters:	gid				GID to find
 *		kir				Pointer to return area
 *		getname				Name buffer, if ki_name wanted
 *
 * Returns:	0				Found
 *		ENOENT				Not found
 *
 * Implicit returns:
 *		*klr				Modified, if found
 */
static int
kauth_identity_find_gid(uid_t gid, struct kauth_identity *kir, char *getname)
{
	struct kauth_identity *kip;

	KAUTH_IDENTITY_LOCK();
	TAILQ_FOREACH(kip, &kauth_identities, ki_link) {
		if ((kip->ki_valid & KI_VALID_GID) && (gid == kip->ki_gid)) {
			kauth_identity_lru(kip);
			/* Copy via structure assignment */
			*kir = *kip;
			/* If a name is wanted and one exists, copy it out */
			if (getname != NULL && (kip->ki_valid & (KI_VALID_PWNAM | KI_VALID_GRNAM))) {
				strlcpy(getname, kip->ki_name, MAXPATHLEN);
			}
			break;
		}
	}
	KAUTH_IDENTITY_UNLOCK();
	return (kip == NULL) ? ENOENT : 0;
}


/*
 * kauth_identity_find_guid
 *
 * Description: Search for an entry by GUID
 *
 * Parameters:	guidp				Pointer to GUID to find
 *		kir				Pointer to return area
 *		getname				Name buffer, if ki_name wanted
 *
 * Returns:	0				Found
 *		ENOENT				Not found
 *
 * Implicit returns:
 *		*klr				Modified, if found
 *
 * Note:	The association may be expired, in which case the caller
 *		may elect to call out to userland to revalidate.
 */
static int
kauth_identity_find_guid(guid_t *guidp, struct kauth_identity *kir, char *getname)
{
	struct kauth_identity *kip;

	KAUTH_IDENTITY_LOCK();
	TAILQ_FOREACH(kip, &kauth_identities, ki_link) {
		if ((kip->ki_valid & KI_VALID_GUID) && (kauth_guid_equal(guidp, &kip->ki_guid))) {
			kauth_identity_lru(kip);
			/* Copy via structure assignment */
			*kir = *kip;
			/* If a name is wanted and one exists, copy it out */
			if (getname != NULL && (kip->ki_valid & (KI_VALID_PWNAM | KI_VALID_GRNAM))) {
				strlcpy(getname, kip->ki_name, MAXPATHLEN);
			}
			break;
		}
	}
	KAUTH_IDENTITY_UNLOCK();
	return (kip == NULL) ? ENOENT : 0;
}

/*
 * kauth_identity_find_nam
 *
 * Description:	Search for an entry by name
 *
 * Parameters:	name				Pointer to name to find
 *		valid				KI_VALID_PWNAM or KI_VALID_GRNAM
 *		kir				Pointer to return area
 *
 * Returns:	0				Found
 *		ENOENT				Not found
 *
 * Implicit returns:
 *		*klr				Modified, if found
 */
static int
kauth_identity_find_nam(char *name, int valid, struct kauth_identity *kir)
{
	struct kauth_identity *kip;

	KAUTH_IDENTITY_LOCK();
	TAILQ_FOREACH(kip, &kauth_identities, ki_link) {
		if ((kip->ki_valid & valid) && !strcmp(name, kip->ki_name)) {
			kauth_identity_lru(kip);
			/* Copy via structure assignment */
			*kir = *kip;
			break;
		}
	}
	KAUTH_IDENTITY_UNLOCK();
	return (kip == NULL) ? ENOENT : 0;
}


/*
 * kauth_identity_find_ntsid
 *
 * Description: Search for an entry by NTSID
 *
 * Parameters:	ntsid				Pointer to NTSID to find
 *		kir				Pointer to return area
 *		getname				Name buffer, if ki_name wanted
 *
 * Returns:	0				Found
 *		ENOENT				Not found
 *
 * Implicit returns:
 *		*klr				Modified, if found
 *
 * Note:	The association may be expired, in which case the caller
 *		may elect to call out to userland to revalidate.
 */
static int
kauth_identity_find_ntsid(ntsid_t *ntsid, struct kauth_identity *kir, char *getname)
{
	struct kauth_identity *kip;

	KAUTH_IDENTITY_LOCK();
	TAILQ_FOREACH(kip, &kauth_identities, ki_link) {
		if ((kip->ki_valid & KI_VALID_NTSID) && (kauth_ntsid_equal(ntsid, &kip->ki_ntsid))) {
			kauth_identity_lru(kip);
			/* Copy via structure assignment */
			*kir = *kip;
			/* If a name is wanted and one exists, copy it out */
			if (getname != NULL && (kip->ki_valid & (KI_VALID_PWNAM | KI_VALID_GRNAM))) {
				strlcpy(getname, kip->ki_name, MAXPATHLEN);
			}
			break;
		}
	}
	KAUTH_IDENTITY_UNLOCK();
	return (kip == NULL) ? ENOENT : 0;
}
#endif  /* CONFIG_EXT_RESOLVER */


/*
 * GUID handling.
 */
guid_t kauth_null_guid;


/*
 * kauth_guid_equal
 *
 * Description:	Determine the equality of two GUIDs
 *
 * Parameters:	guid1				Pointer to first GUID
 *		guid2				Pointer to second GUID
 *
 * Returns:	0				If GUIDs are unequal
 *		!0				If GUIDs are equal
 */
int
kauth_guid_equal(guid_t *guid1, guid_t *guid2)
{
	return bcmp(guid1, guid2, sizeof(*guid1)) == 0;
}


/*
 * kauth_wellknown_guid
 *
 * Description:	Determine if a GUID is a well-known GUID
 *
 * Parameters:	guid				Pointer to GUID to check
 *
 * Returns:	KAUTH_WKG_NOT			Not a well known GUID
 *		KAUTH_WKG_EVERYBODY		"Everybody"
 *		KAUTH_WKG_NOBODY		"Nobody"
 *		KAUTH_WKG_OWNER			"Other"
 *		KAUTH_WKG_GROUP			"Group"
 */
int
kauth_wellknown_guid(guid_t *guid)
{
	static char     fingerprint[] = {0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef};
	uint32_t                code;
	/*
	 * All WKGs begin with the same 12 bytes.
	 */
	if (bcmp((void *)guid, fingerprint, 12) == 0) {
		/*
		 * The final 4 bytes are our code (in network byte order).
		 */
		code = OSSwapHostToBigInt32(*(uint32_t *)&guid->g_guid[12]);
		switch (code) {
		case 0x0000000c:
			return KAUTH_WKG_EVERYBODY;
		case 0xfffffffe:
			return KAUTH_WKG_NOBODY;
		case 0x0000000a:
			return KAUTH_WKG_OWNER;
		case 0x00000010:
			return KAUTH_WKG_GROUP;
		}
	}
	return KAUTH_WKG_NOT;
}


/*
 * kauth_ntsid_equal
 *
 * Description:	Determine the equality of two NTSIDs (NT Security Identifiers)
 *
 * Parameters:	sid1				Pointer to first NTSID
 *		sid2				Pointer to second NTSID
 *
 * Returns:	0				If GUIDs are unequal
 *		!0				If GUIDs are equal
 */
int
kauth_ntsid_equal(ntsid_t *sid1, ntsid_t *sid2)
{
	/* check sizes for equality, also sanity-check size while we're at it */
	if ((KAUTH_NTSID_SIZE(sid1) == KAUTH_NTSID_SIZE(sid2)) &&
	    (KAUTH_NTSID_SIZE(sid1) <= sizeof(*sid1)) &&
	    bcmp(sid1, sid2, KAUTH_NTSID_SIZE(sid1)) == 0) {
		return 1;
	}
	return 0;
}


/*
 * Identity KPI
 *
 * We support four tokens representing identity:
 *  - Credential reference
 *  - UID
 *  - GUID
 *  - NT security identifier
 *
 * Of these, the UID is the ubiquitous identifier; cross-referencing should
 * be done using it.
 */



/*
 * kauth_cred_change_egid
 *
 * Description:	Set EGID by changing the first element of cr_groups for the
 *		passed credential; if the new EGID exists in the list of
 *		groups already, then rotate the old EGID into its position,
 *		otherwise replace it
 *
 * Parameters:	cred			Pointer to the credential to modify
 *		new_egid		The new EGID to set
 *
 * Returns:	0			The egid did not displace a member of
 *					the supplementary group list
 *		1			The egid being set displaced a member
 *					of the supplementary groups list
 *
 * Note:	Utility function; internal use only because of locking.
 *
 *		This function operates on the credential passed; the caller
 *		must operate either on a newly allocated credential (one for
 *		which there is no hash cache reference and no externally
 *		visible pointer reference), or a template credential.
 */
static int
kauth_cred_change_egid(kauth_cred_t cred, gid_t new_egid)
{
	int     i;
	int     displaced = 1;
#if radar_4600026
	int     is_member;
#endif  /* radar_4600026 */
	gid_t   old_egid = kauth_cred_getgid(cred);
	posix_cred_t pcred = posix_cred_get(cred);

	/* Ignoring the first entry, scan for a match for the new egid */
	for (i = 1; i < pcred->cr_ngroups; i++) {
		/*
		 * If we find a match, swap them so we don't lose overall
		 * group information
		 */
		if (pcred->cr_groups[i] == new_egid) {
			pcred->cr_groups[i] = old_egid;
			displaced = 0;
			break;
		}
	}

#if radar_4600026
#error Fix radar 4600026 first!!!

/*
 *  This is correct for memberd behaviour, but incorrect for POSIX; to address
 *  this, we would need to automatically opt-out any SUID/SGID binary, and force
 *  it to use initgroups to opt back in.  We take the approach of considering it
 *  opt'ed out in any group of 16 displacement instead, since it's a much more
 *  conservative approach (i.e. less likely to cause things to break).
 */

	/*
	 * If we displaced a member of the supplementary groups list of the
	 * credential, and we have not opted out of memberd, then if memberd
	 * says that the credential is a member of the group, then it has not
	 * actually been displaced.
	 *
	 * NB:	This is typically a cold code path.
	 */
	if (displaced && !(pcred->cr_flags & CRF_NOMEMBERD) &&
	    kauth_cred_ismember_gid(cred, new_egid, &is_member) == 0 &&
	    is_member) {
		displaced = 0;
	}
#endif  /* radar_4600026 */

	/* set the new EGID into the old spot */
	pcred->cr_groups[0] = new_egid;

	return displaced;
}


uid_t
kauth_cred_getuid(kauth_cred_t cred)
{
	return posix_cred_get(cred)->cr_uid;
}

uid_t
kauth_cred_getruid(kauth_cred_t cred)
{
	return posix_cred_get(cred)->cr_ruid;
}

uid_t
kauth_cred_getsvuid(kauth_cred_t cred)
{
	return posix_cred_get(cred)->cr_svuid;
}


gid_t
kauth_cred_getgid(kauth_cred_t cred)
{
	return posix_cred_get(cred)->cr_gid;
}

gid_t
kauth_cred_getrgid(kauth_cred_t cred)
{
	return posix_cred_get(cred)->cr_rgid;
}

gid_t
kauth_cred_getsvgid(kauth_cred_t cred)
{
	return posix_cred_get(cred)->cr_svgid;
}


static int      kauth_cred_cache_lookup(int from, int to, void *src, void *dst);

#if CONFIG_EXT_RESOLVER == 0
/*
 * If there's no resolver, only support a subset of the kauth_cred_x2y() lookups.
 */
static __inline int
kauth_cred_cache_lookup(int from, int to, void *src, void *dst)
{
	/* NB: These must match the definitions used by Libinfo's mbr_identifier_translate(). */
	static const uuid_t _user_compat_prefix = {0xff, 0xff, 0xee, 0xee, 0xdd, 0xdd, 0xcc, 0xcc, 0xbb, 0xbb, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00};
	static const uuid_t _group_compat_prefix = {0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0xab, 0xcd, 0xef, 0x00, 0x00, 0x00, 0x00};
#define COMPAT_PREFIX_LEN       (sizeof(uuid_t) - sizeof(id_t))

	assert(from != to);

	switch (from) {
	case KI_VALID_UID: {
		id_t uid = htonl(*(id_t *)src);

		if (to == KI_VALID_GUID) {
			uint8_t *uu = dst;
			memcpy(uu, _user_compat_prefix, sizeof(_user_compat_prefix));
			memcpy(&uu[COMPAT_PREFIX_LEN], &uid, sizeof(uid));
			return 0;
		}
		break;
	}
	case KI_VALID_GID: {
		id_t gid = htonl(*(id_t *)src);

		if (to == KI_VALID_GUID) {
			uint8_t *uu = dst;
			memcpy(uu, _group_compat_prefix, sizeof(_group_compat_prefix));
			memcpy(&uu[COMPAT_PREFIX_LEN], &gid, sizeof(gid));
			return 0;
		}
		break;
	}
	case KI_VALID_GUID: {
		const uint8_t *uu = src;

		if (to == KI_VALID_UID) {
			if (memcmp(uu, _user_compat_prefix, COMPAT_PREFIX_LEN) == 0) {
				id_t uid;
				memcpy(&uid, &uu[COMPAT_PREFIX_LEN], sizeof(uid));
				*(id_t *)dst = ntohl(uid);
				return 0;
			}
		} else if (to == KI_VALID_GID) {
			if (memcmp(uu, _group_compat_prefix, COMPAT_PREFIX_LEN) == 0) {
				id_t gid;
				memcpy(&gid, &uu[COMPAT_PREFIX_LEN], sizeof(gid));
				*(id_t *)dst = ntohl(gid);
				return 0;
			}
		}
		break;
	}
	default:
		/* NOT IMPLEMENTED */
		break;
	}
	return ENOENT;
}
#endif

#if defined(CONFIG_EXT_RESOLVER) && (CONFIG_EXT_RESOLVER)
/*
 * Structure to hold supplemental groups. Used for impedance matching with
 * kauth_cred_cache_lookup below.
 */
struct supgroups {
	size_t *count;
	gid_t *groups;
};

/*
 * kauth_cred_uid2groups
 *
 * Description:	Fetch supplemental GROUPS from UID
 *
 * Parameters:	uid				UID to examine
 *		groups				pointer to an array of gid_ts
 *		gcount				pointer to the number of groups wanted/returned
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*groups				Modified, if successful
 *		*gcount				Modified, if successful
 *
 */
static int
kauth_cred_uid2groups(uid_t *uid, gid_t *groups, size_t *gcount)
{
	int rv;

	struct supgroups supgroups;
	supgroups.count = gcount;
	supgroups.groups = groups;

	rv = kauth_cred_cache_lookup(KI_VALID_UID, KI_VALID_GROUPS, uid, &supgroups);

	return rv;
}
#endif

/*
 * kauth_cred_guid2pwnam
 *
 * Description:	Fetch PWNAM from GUID
 *
 * Parameters:	guidp				Pointer to GUID to examine
 *		pwnam				Pointer to user@domain buffer
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*pwnam				Modified, if successful
 *
 * Notes:	pwnam is assumed to point to a buffer of MAXPATHLEN in size
 */
int
kauth_cred_guid2pwnam(guid_t *guidp, char *pwnam)
{
	return kauth_cred_cache_lookup(KI_VALID_GUID, KI_VALID_PWNAM, guidp, pwnam);
}


/*
 * kauth_cred_guid2grnam
 *
 * Description:	Fetch GRNAM from GUID
 *
 * Parameters:	guidp				Pointer to GUID to examine
 *		grnam				Pointer to group@domain buffer
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*grnam				Modified, if successful
 *
 * Notes:	grnam is assumed to point to a buffer of MAXPATHLEN in size
 */
int
kauth_cred_guid2grnam(guid_t *guidp, char *grnam)
{
	return kauth_cred_cache_lookup(KI_VALID_GUID, KI_VALID_GRNAM, guidp, grnam);
}


/*
 * kauth_cred_pwnam2guid
 *
 * Description:	Fetch PWNAM from GUID
 *
 * Parameters:	pwnam				String containing user@domain
 *		guidp				Pointer to buffer for GUID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*guidp				Modified, if successful
 *
 * Notes:	pwnam should not point to a request larger than MAXPATHLEN
 *		bytes in size, including the NUL termination of the string.
 */
int
kauth_cred_pwnam2guid(char *pwnam, guid_t *guidp)
{
	return kauth_cred_cache_lookup(KI_VALID_PWNAM, KI_VALID_GUID, pwnam, guidp);
}


/*
 * kauth_cred_grnam2guid
 *
 * Description:	Fetch GRNAM from GUID
 *
 * Parameters:	grnam				String containing group@domain
 *		guidp				Pointer to buffer for GUID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*guidp				Modified, if successful
 *
 * Notes:	grnam should not point to a request larger than MAXPATHLEN
 *		bytes in size, including the NUL termination of the string.
 */
int
kauth_cred_grnam2guid(char *grnam, guid_t *guidp)
{
	return kauth_cred_cache_lookup(KI_VALID_GRNAM, KI_VALID_GUID, grnam, guidp);
}


/*
 * kauth_cred_guid2uid
 *
 * Description:	Fetch UID from GUID
 *
 * Parameters:	guidp				Pointer to GUID to examine
 *		uidp				Pointer to buffer for UID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*uidp				Modified, if successful
 */
int
kauth_cred_guid2uid(guid_t *guidp, uid_t *uidp)
{
	return kauth_cred_cache_lookup(KI_VALID_GUID, KI_VALID_UID, guidp, uidp);
}


/*
 * kauth_cred_guid2gid
 *
 * Description:	Fetch GID from GUID
 *
 * Parameters:	guidp				Pointer to GUID to examine
 *		gidp				Pointer to buffer for GID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*gidp				Modified, if successful
 */
int
kauth_cred_guid2gid(guid_t *guidp, gid_t *gidp)
{
	return kauth_cred_cache_lookup(KI_VALID_GUID, KI_VALID_GID, guidp, gidp);
}

/*
 * kauth_cred_nfs4domain2dsnode
 *
 * Description: Fetch dsnode from nfs4domain
 *
 * Parameters:	nfs4domain			Pointer to a string nfs4 domain
 *		dsnode				Pointer to buffer for dsnode
 *
 * Returns:	0				Success
 *		ENOENT				For now just a stub that always fails
 *
 * Implicit returns:
 *		*dsnode				Modified, if successuful
 */
int
kauth_cred_nfs4domain2dsnode(__unused char *nfs4domain, __unused char *dsnode)
{
	return ENOENT;
}

/*
 * kauth_cred_dsnode2nfs4domain
 *
 * Description: Fetch nfs4domain from dsnode
 *
 * Parameters:	nfs4domain			Pointer to  string dsnode
 *		dsnode				Pointer to buffer for nfs4domain
 *
 * Returns:	0				Success
 *		ENOENT				For now just a stub that always fails
 *
 * Implicit returns:
 *		*nfs4domain			Modified, if successuful
 */
int
kauth_cred_dsnode2nfs4domain(__unused char *dsnode, __unused char *nfs4domain)
{
	return ENOENT;
}

/*
 * kauth_cred_ntsid2uid
 *
 * Description:	Fetch UID from NTSID
 *
 * Parameters:	sidp				Pointer to NTSID to examine
 *		uidp				Pointer to buffer for UID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*uidp				Modified, if successful
 */
int
kauth_cred_ntsid2uid(ntsid_t *sidp, uid_t *uidp)
{
	return kauth_cred_cache_lookup(KI_VALID_NTSID, KI_VALID_UID, sidp, uidp);
}


/*
 * kauth_cred_ntsid2gid
 *
 * Description:	Fetch GID from NTSID
 *
 * Parameters:	sidp				Pointer to NTSID to examine
 *		gidp				Pointer to buffer for GID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*gidp				Modified, if successful
 */
int
kauth_cred_ntsid2gid(ntsid_t *sidp, gid_t *gidp)
{
	return kauth_cred_cache_lookup(KI_VALID_NTSID, KI_VALID_GID, sidp, gidp);
}


/*
 * kauth_cred_ntsid2guid
 *
 * Description:	Fetch GUID from NTSID
 *
 * Parameters:	sidp				Pointer to NTSID to examine
 *		guidp				Pointer to buffer for GUID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*guidp				Modified, if successful
 */
int
kauth_cred_ntsid2guid(ntsid_t *sidp, guid_t *guidp)
{
	return kauth_cred_cache_lookup(KI_VALID_NTSID, KI_VALID_GUID, sidp, guidp);
}


/*
 * kauth_cred_uid2guid
 *
 * Description:	Fetch GUID from UID
 *
 * Parameters:	uid				UID to examine
 *		guidp				Pointer to buffer for GUID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*guidp				Modified, if successful
 */
int
kauth_cred_uid2guid(uid_t uid, guid_t *guidp)
{
	return kauth_cred_cache_lookup(KI_VALID_UID, KI_VALID_GUID, &uid, guidp);
}


/*
 * kauth_cred_getguid
 *
 * Description:	Fetch GUID from credential
 *
 * Parameters:	cred				Credential to examine
 *		guidp				Pointer to buffer for GUID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*guidp				Modified, if successful
 */
int
kauth_cred_getguid(kauth_cred_t cred, guid_t *guidp)
{
	return kauth_cred_uid2guid(kauth_cred_getuid(cred), guidp);
}


/*
 * kauth_cred_getguid
 *
 * Description:	Fetch GUID from GID
 *
 * Parameters:	gid				GID to examine
 *		guidp				Pointer to buffer for GUID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*guidp				Modified, if successful
 */
int
kauth_cred_gid2guid(gid_t gid, guid_t *guidp)
{
	return kauth_cred_cache_lookup(KI_VALID_GID, KI_VALID_GUID, &gid, guidp);
}


/*
 * kauth_cred_uid2ntsid
 *
 * Description:	Fetch NTSID from UID
 *
 * Parameters:	uid				UID to examine
 *		sidp				Pointer to buffer for NTSID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*sidp				Modified, if successful
 */
int
kauth_cred_uid2ntsid(uid_t uid, ntsid_t *sidp)
{
	return kauth_cred_cache_lookup(KI_VALID_UID, KI_VALID_NTSID, &uid, sidp);
}


/*
 * kauth_cred_getntsid
 *
 * Description:	Fetch NTSID from credential
 *
 * Parameters:	cred				Credential to examine
 *		sidp				Pointer to buffer for NTSID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*sidp				Modified, if successful
 */
int
kauth_cred_getntsid(kauth_cred_t cred, ntsid_t *sidp)
{
	return kauth_cred_uid2ntsid(kauth_cred_getuid(cred), sidp);
}


/*
 * kauth_cred_gid2ntsid
 *
 * Description:	Fetch NTSID from GID
 *
 * Parameters:	gid				GID to examine
 *		sidp				Pointer to buffer for NTSID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*sidp				Modified, if successful
 */
int
kauth_cred_gid2ntsid(gid_t gid, ntsid_t *sidp)
{
	return kauth_cred_cache_lookup(KI_VALID_GID, KI_VALID_NTSID, &gid, sidp);
}


/*
 * kauth_cred_guid2ntsid
 *
 * Description:	Fetch NTSID from GUID
 *
 * Parameters:	guidp				Pointer to GUID to examine
 *		sidp				Pointer to buffer for NTSID
 *
 * Returns:	0				Success
 *	kauth_cred_cache_lookup:EINVAL
 *
 * Implicit returns:
 *		*sidp				Modified, if successful
 */
int
kauth_cred_guid2ntsid(guid_t *guidp, ntsid_t *sidp)
{
	return kauth_cred_cache_lookup(KI_VALID_GUID, KI_VALID_NTSID, guidp, sidp);
}


/*
 * kauth_cred_cache_lookup
 *
 * Description:	Lookup a translation in the cache; if one is not found, and
 *		the attempt was not fatal, submit the request to the resolver
 *		instead, and wait for it to complete or be aborted.
 *
 * Parameters:	from				Identity information we have
 *		to				Identity information we want
 *		src				Pointer to buffer containing
 *						the source identity
 *		dst				Pointer to buffer to receive
 *						the target identity
 *
 * Returns:	0				Success
 *		EINVAL				Unknown source identity type
 */
#if CONFIG_EXT_RESOLVER
static int
kauth_cred_cache_lookup(int from, int to, void *src, void *dst)
{
	struct kauth_identity ki;
	struct kauth_identity_extlookup el;
	int error;
	uint64_t extend_data = 0ULL;
	int (* expired)(struct kauth_identity *kip);
	char *namebuf = NULL;

	KAUTH_DEBUG("CACHE - translate %d to %d", from, to);

	/*
	 * Look for an existing cache entry for this association.
	 * If the entry has not expired, return the cached information.
	 * We do not cache user@domain translations here; they use too
	 * much memory to hold onto forever, and can not be updated
	 * atomically.
	 */
	if (to == KI_VALID_PWNAM || to == KI_VALID_GRNAM) {
		if (dst == NULL) {
			return EINVAL;
		}
		namebuf = dst;
		*namebuf = '\0';
	}
	ki.ki_valid = 0;
	switch (from) {
	case KI_VALID_UID:
		error = kauth_identity_find_uid(*(uid_t *)src, &ki, namebuf);
		break;
	case KI_VALID_GID:
		error = kauth_identity_find_gid(*(gid_t *)src, &ki, namebuf);
		break;
	case KI_VALID_GUID:
		error = kauth_identity_find_guid((guid_t *)src, &ki, namebuf);
		break;
	case KI_VALID_NTSID:
		error = kauth_identity_find_ntsid((ntsid_t *)src, &ki, namebuf);
		break;
	case KI_VALID_PWNAM:
	case KI_VALID_GRNAM:
		/* Names are unique in their 'from' space */
		error = kauth_identity_find_nam((char *)src, from, &ki);
		break;
	default:
		return EINVAL;
	}
	/* If we didn't get what we're asking for. Call the resolver */
	if (!error && !(to & ki.ki_valid)) {
		error = ENOENT;
	}
	/* lookup failure or error */
	if (error != 0) {
		/* any other error is fatal */
		if (error != ENOENT) {
			/* XXX bogus check - this is not possible */
			KAUTH_DEBUG("CACHE - cache search error %d", error);
			return error;
		}
	} else {
		/* found a valid cached entry, check expiry */
		switch (to) {
		case KI_VALID_GUID:
			expired = kauth_identity_guid_expired;
			break;
		case KI_VALID_NTSID:
			expired = kauth_identity_ntsid_expired;
			break;
		case KI_VALID_GROUPS:
			expired = kauth_identity_groups_expired;
			break;
		default:
			switch (from) {
			case KI_VALID_GUID:
				expired = kauth_identity_guid_expired;
				break;
			case KI_VALID_NTSID:
				expired = kauth_identity_ntsid_expired;
				break;
			default:
				expired = NULL;
			}
		}

		/*
		 * If no expiry function, or not expired, we have found
		 * a hit.
		 */
		if (expired) {
			if (!expired(&ki)) {
				KAUTH_DEBUG("CACHE - entry valid, unexpired");
				expired = NULL; /* must clear it is used as a flag */
			} else {
				/*
				 * We leave ki_valid set here; it contains a
				 * translation but the TTL has expired.  If we can't
				 * get a result from the resolver, we will use it as
				 * a better-than nothing alternative.
				 */

				KAUTH_DEBUG("CACHE - expired entry found");
			}
		} else {
			KAUTH_DEBUG("CACHE - no expiry function");
		}

		if (!expired) {
			/* do we have a translation? */
			if (ki.ki_valid & to) {
				KAUTH_DEBUG("CACHE - found matching entry with valid 0x%08x", ki.ki_valid);
				DTRACE_PROC4(kauth__identity__cache__hit, int, from, int, to, void *, src, void *, dst);
				goto found;
			} else {
				/*
				 * GUIDs and NTSIDs map to either a UID or a GID, but not both.
				 * If we went looking for a translation from GUID or NTSID and
				 * found a translation that wasn't for our desired type, then
				 * don't bother calling the resolver. We know that this
				 * GUID/NTSID can't translate to our desired type.
				 */
				switch (from) {
				case KI_VALID_GUID:
				case KI_VALID_NTSID:
					switch (to) {
					case KI_VALID_GID:
						if ((ki.ki_valid & KI_VALID_UID)) {
							KAUTH_DEBUG("CACHE - unexpected entry 0x%08x & %x", ki.ki_valid, KI_VALID_GID);
							return ENOENT;
						}
						break;
					case KI_VALID_UID:
						if ((ki.ki_valid & KI_VALID_GID)) {
							KAUTH_DEBUG("CACHE - unexpected entry 0x%08x & %x", ki.ki_valid, KI_VALID_UID);
							return ENOENT;
						}
						break;
					}
					break;
				}
			}
		}
	}

	/*
	 * We failed to find a cache entry; call the resolver.
	 *
	 * Note:	We ask for as much non-extended data as we can get,
	 *		and only provide (or ask for) extended information if
	 *		we have a 'from' (or 'to') which requires it.  This
	 *		way we don't pay for the extra transfer overhead for
	 *		data we don't need.
	 */
	bzero(&el, sizeof(el));
	el.el_info_pid = proc_getpid(current_proc());
	switch (from) {
	case KI_VALID_UID:
		el.el_flags = KAUTH_EXTLOOKUP_VALID_UID;
		el.el_uid = *(uid_t *)src;
		break;
	case KI_VALID_GID:
		el.el_flags = KAUTH_EXTLOOKUP_VALID_GID;
		el.el_gid = *(gid_t *)src;
		break;
	case KI_VALID_GUID:
		el.el_flags = KAUTH_EXTLOOKUP_VALID_UGUID | KAUTH_EXTLOOKUP_VALID_GGUID;
		el.el_uguid = *(guid_t *)src;
		el.el_gguid = *(guid_t *)src;
		break;
	case KI_VALID_NTSID:
		el.el_flags = KAUTH_EXTLOOKUP_VALID_USID | KAUTH_EXTLOOKUP_VALID_GSID;
		el.el_usid = *(ntsid_t *)src;
		el.el_gsid = *(ntsid_t *)src;
		break;
	case KI_VALID_PWNAM:
		/* extra overhead */
		el.el_flags = KAUTH_EXTLOOKUP_VALID_PWNAM;
		extend_data = CAST_USER_ADDR_T(src);
		break;
	case KI_VALID_GRNAM:
		/* extra overhead */
		el.el_flags = KAUTH_EXTLOOKUP_VALID_GRNAM;
		extend_data = CAST_USER_ADDR_T(src);
		break;
	default:
		return EINVAL;
	}
	/*
	 * Here we ask for everything all at once, to avoid having to work
	 * out what we really want now, or might want soon.
	 *
	 * Asking for SID translations when we don't know we need them right
	 * now is going to cause excess work to be done if we're connected
	 * to a network that thinks it can translate them.  This list needs
	 * to get smaller/smarter.
	 */
	el.el_flags |= KAUTH_EXTLOOKUP_WANT_UID | KAUTH_EXTLOOKUP_WANT_GID |
	    KAUTH_EXTLOOKUP_WANT_UGUID | KAUTH_EXTLOOKUP_WANT_GGUID |
	    KAUTH_EXTLOOKUP_WANT_USID | KAUTH_EXTLOOKUP_WANT_GSID;
	if (to == KI_VALID_PWNAM) {
		/* extra overhead */
		el.el_flags |= KAUTH_EXTLOOKUP_WANT_PWNAM;
		extend_data = CAST_USER_ADDR_T(dst);
	}
	if (to == KI_VALID_GRNAM) {
		/* extra overhead */
		el.el_flags |= KAUTH_EXTLOOKUP_WANT_GRNAM;
		extend_data = CAST_USER_ADDR_T(dst);
	}
	if (to == KI_VALID_GROUPS) {
		/* Expensive and only useful for an NFS client not using kerberos */
		el.el_flags |= KAUTH_EXTLOOKUP_WANT_SUPGRPS;
		if (ki.ki_valid & KI_VALID_GROUPS) {
			/*
			 * Copy the current supplemental groups for the resolver.
			 * The resolver should check these groups first and if
			 * the user (uid) is still a member it should endeavor to
			 * keep them in the list. Otherwise NFS clients could get
			 * changing access to server file system objects on each
			 * expiration.
			 */
			if (ki.ki_supgrpcnt > NGROUPS) {
				panic("kauth data structure corrupted. kauth identity 0x%p with %u groups, greater than max of %d",
				    &ki, ki.ki_supgrpcnt, NGROUPS);
			}

			el.el_sup_grp_cnt = (uint32_t)ki.ki_supgrpcnt;

			memcpy(el.el_sup_groups, ki.ki_supgrps, sizeof(el.el_sup_groups[0]) * ki.ki_supgrpcnt);
			/* Let the resolver know these were the previous valid groups */
			el.el_flags |= KAUTH_EXTLOOKUP_VALID_SUPGRPS;
			KAUTH_DEBUG("GROUPS: Sending previously valid GROUPS");
		} else {
			KAUTH_DEBUG("GROUPS: no valid groups to send");
		}
	}

	/* Call resolver */
	KAUTH_DEBUG("CACHE - calling resolver for %x", el.el_flags);

	DTRACE_PROC3(kauth__id__resolver__submitted, int, from, int, to, uintptr_t, src);

	error = kauth_resolver_submit(&el, extend_data);

	DTRACE_PROC2(kauth__id__resolver__returned, int, error, struct kauth_identity_extlookup *, &el)

	KAUTH_DEBUG("CACHE - resolver returned %d", error);

	/* was the external lookup successful? */
	if (error == 0) {
		/*
		 * Save the results from the lookup - we may have other
		 * information, even if we didn't get a guid or the
		 * extended data.
		 *
		 * If we came from a name, we know the extend_data is valid.
		 */
		if (from == KI_VALID_PWNAM) {
			el.el_flags |= KAUTH_EXTLOOKUP_VALID_PWNAM;
		} else if (from == KI_VALID_GRNAM) {
			el.el_flags |= KAUTH_EXTLOOKUP_VALID_GRNAM;
		}

		kauth_identity_updatecache(&el, &ki, extend_data);

		/*
		 * Check to see if we have a valid cache entry
		 * originating from the result.
		 */
		if (!(ki.ki_valid & to)) {
			error = ENOENT;
		}
	}
	if (error) {
		return error;
	}
found:
	/*
	 * Copy from the appropriate struct kauth_identity cache entry
	 * structure into the destination buffer area.
	 */
	switch (to) {
	case KI_VALID_UID:
		*(uid_t *)dst = ki.ki_uid;
		break;
	case KI_VALID_GID:
		*(gid_t *)dst = ki.ki_gid;
		break;
	case KI_VALID_GUID:
		*(guid_t *)dst = ki.ki_guid;
		break;
	case KI_VALID_NTSID:
		*(ntsid_t *)dst = ki.ki_ntsid;
		break;
	case KI_VALID_GROUPS: {
		struct supgroups *gp = (struct supgroups *)dst;
		size_t limit = ki.ki_supgrpcnt;

		if (gp->count) {
			limit = MIN(ki.ki_supgrpcnt, *gp->count);
			*gp->count = limit;
		}

		memcpy(gp->groups, ki.ki_supgrps, sizeof(gid_t) * limit);
	}
	break;
	case KI_VALID_PWNAM:
	case KI_VALID_GRNAM:
		/* handled in kauth_resolver_complete() */
		break;
	default:
		return EINVAL;
	}
	KAUTH_DEBUG("CACHE - returned successfully");
	return 0;
}


/*
 * Group membership cache.
 *
 * XXX the linked-list implementation here needs to be optimized.
 */

/*
 * kauth_groups_expired
 *
 * Description:	Handle lazy expiration of group membership cache entries
 *
 * Parameters:	gm				group membership entry to
 *						check for expiration
 *
 * Returns:	1				Expired
 *		0				Not expired
 */
static int
kauth_groups_expired(struct kauth_group_membership *gm)
{
	struct timeval tv;

	/*
	 * Expiration time of 0 means this entry is persistent.
	 */
	if (gm->gm_expiry == 0) {
		return 0;
	}

	microuptime(&tv);

	return (gm->gm_expiry <= tv.tv_sec) ? 1 : 0;
}


/*
 * kauth_groups_lru
 *
 * Description:	Promote the entry to the head of the LRU, assumes the cache
 *		is locked.
 *
 * Parameters:	kip				group membership entry to move
 *						to the head of the LRU list,
 *						if it's not already there
 *
 * Returns:	(void)
 *
 * Notes:	This is called even if the entry has expired; typically an
 *		expired entry that's been looked up is about to be revalidated,
 *		and having it closer to the head of the LRU means finding it
 *		quickly again when the revalidation comes through.
 */
static void
kauth_groups_lru(struct kauth_group_membership *gm)
{
	if (gm != TAILQ_FIRST(&kauth_groups)) {
		TAILQ_REMOVE(&kauth_groups, gm, gm_link);
		TAILQ_INSERT_HEAD(&kauth_groups, gm, gm_link);
	}
}


/*
 * kauth_groups_updatecache
 *
 * Description:	Given a lookup result, add any group cache associations that
 *		we don't currently have.
 *
 * Parameters:	elp				External lookup result from
 *						user space daemon to kernel
 *		rkip				pointer to returned kauth
 *						identity, or NULL
 *
 * Returns:	(void)
 */
static void
kauth_groups_updatecache(struct kauth_identity_extlookup *el)
{
	struct kauth_group_membership *gm;
	struct timeval tv;

	/* need a valid response if we are to cache anything */
	if ((el->el_flags &
	    (KAUTH_EXTLOOKUP_VALID_UID | KAUTH_EXTLOOKUP_VALID_GID | KAUTH_EXTLOOKUP_VALID_MEMBERSHIP)) !=
	    (KAUTH_EXTLOOKUP_VALID_UID | KAUTH_EXTLOOKUP_VALID_GID | KAUTH_EXTLOOKUP_VALID_MEMBERSHIP)) {
		return;
	}

	microuptime(&tv);

	/*
	 * Search for an existing record for this association before inserting
	 * a new one; if we find one, update it instead of creating a new one
	 */
	KAUTH_GROUPS_LOCK();
	TAILQ_FOREACH(gm, &kauth_groups, gm_link) {
		if ((el->el_uid == gm->gm_uid) &&
		    (el->el_gid == gm->gm_gid)) {
			if (el->el_flags & KAUTH_EXTLOOKUP_ISMEMBER) {
				gm->gm_flags |= KAUTH_GROUP_ISMEMBER;
			} else {
				gm->gm_flags &= ~KAUTH_GROUP_ISMEMBER;
			}
			gm->gm_expiry = (el->el_member_valid) ? el->el_member_valid + tv.tv_sec : 0;
			kauth_groups_lru(gm);
			break;
		}
	}
	KAUTH_GROUPS_UNLOCK();

	/* if we found an entry to update, stop here */
	if (gm != NULL) {
		return;
	}

	/* allocate a new record */
	gm = kalloc_type(struct kauth_group_membership, Z_WAITOK | Z_NOFAIL);
	gm->gm_uid = el->el_uid;
	gm->gm_gid = el->el_gid;
	if (el->el_flags & KAUTH_EXTLOOKUP_ISMEMBER) {
		gm->gm_flags |= KAUTH_GROUP_ISMEMBER;
	} else {
		gm->gm_flags &= ~KAUTH_GROUP_ISMEMBER;
	}
	gm->gm_expiry = (el->el_member_valid) ? el->el_member_valid + tv.tv_sec : 0;

	/*
	 * Insert the new entry.  Note that it's possible to race ourselves
	 * here and end up with duplicate entries in the list.  Wasteful, but
	 * harmless since the first into the list will never be looked up,
	 * and thus will eventually just fall off the end.
	 */
	KAUTH_GROUPS_LOCK();
	TAILQ_INSERT_HEAD(&kauth_groups, gm, gm_link);
	if (++kauth_groups_count > kauth_groups_cachemax) {
		gm = TAILQ_LAST(&kauth_groups, kauth_groups_head);
		TAILQ_REMOVE(&kauth_groups, gm, gm_link);
		kauth_groups_count--;
	} else {
		gm = NULL;
	}
	KAUTH_GROUPS_UNLOCK();

	/* free expired cache entry */
	kfree_type(struct kauth_group_membership, gm);
}

/*
 * Trim older entries from the group membership cache.
 *
 * Must be called with the group cache lock held.
 */
static void
kauth_groups_trimcache(int new_size)
{
	struct kauth_group_membership *gm;

	lck_mtx_assert(&kauth_groups_mtx, LCK_MTX_ASSERT_OWNED);

	while (kauth_groups_count > new_size) {
		gm = TAILQ_LAST(&kauth_groups, kauth_groups_head);
		TAILQ_REMOVE(&kauth_groups, gm, gm_link);
		kauth_groups_count--;
		kfree_type(struct kauth_group_membership, gm);
	}
}
#endif  /* CONFIG_EXT_RESOLVER */

/*
 * Group membership KPI
 */

/*
 * kauth_cred_ismember_gid
 *
 * Description:	Given a credential and a GID, determine if the GID is a member
 *		of one of the supplementary groups associated with the given
 *		credential
 *
 * Parameters:	cred				Credential to check in
 *		gid				GID to check for membership
 *		resultp				Pointer to int to contain the
 *						result of the call
 *
 * Returns:	0				Success
 *		ENOENT				Could not perform lookup
 *	kauth_resolver_submit:EWOULDBLOCK
 *	kauth_resolver_submit:EINTR
 *	kauth_resolver_submit:ENOMEM
 *	kauth_resolver_submit:ENOENT		User space daemon did not vend
 *						this credential.
 *	kauth_resolver_submit:???		Unlikely error from user space
 *
 * Implicit returns:
 *		*resultp (modified)	1	Is member
 *					0	Is not member
 *
 * Notes:	This function guarantees not to modify resultp when returning
 *		an error.
 *
 *		This function effectively checks the EGID as well, since the
 *		EGID is cr_groups[0] as an implementation detail.
 */
int
kauth_cred_ismember_gid(kauth_cred_t cred, gid_t gid, int *resultp)
{
	posix_cred_t pcred = posix_cred_get(cred);
	int i;

	/*
	 * Check the per-credential list of override groups.
	 *
	 * We can conditionalise this on cred->cr_gmuid == KAUTH_UID_NONE since
	 * the cache should be used for that case.
	 */
	for (i = 0; i < pcred->cr_ngroups; i++) {
		if (gid == pcred->cr_groups[i]) {
			*resultp = 1;
			return 0;
		}
	}

	/*
	 * If we don't have a UID for group membership checks, the in-cred list
	 * was authoritative and we can stop here.
	 */
	if (pcred->cr_gmuid == KAUTH_UID_NONE) {
		*resultp = 0;
		return 0;
	}

#if CONFIG_EXT_RESOLVER
	struct kauth_group_membership *gm;
	struct kauth_identity_extlookup el;
	int error;

	/*
	 * If the resolver hasn't checked in yet, we are early in the boot
	 * phase and the local group list is complete and authoritative.
	 */
	if (!kauth_resolver_registered) {
		*resultp = 0;
		return 0;
	}

	/* TODO: */
	/* XXX check supplementary groups */
	/* XXX check whiteout groups */
	/* XXX nesting of supplementary/whiteout groups? */

	/*
	 * Check the group cache.
	 */
	KAUTH_GROUPS_LOCK();
	TAILQ_FOREACH(gm, &kauth_groups, gm_link) {
		if ((gm->gm_uid == pcred->cr_gmuid) && (gm->gm_gid == gid) && !kauth_groups_expired(gm)) {
			kauth_groups_lru(gm);
			break;
		}
	}

	/* did we find a membership entry? */
	if (gm != NULL) {
		*resultp = (gm->gm_flags & KAUTH_GROUP_ISMEMBER) ? 1 : 0;
	}
	KAUTH_GROUPS_UNLOCK();

	/* if we did, we can return now */
	if (gm != NULL) {
		DTRACE_PROC2(kauth__group__cache__hit, int, pcred->cr_gmuid, int, gid);
		return 0;
	}

	/* nothing in the cache, need to go to userland */
	bzero(&el, sizeof(el));
	el.el_info_pid = proc_getpid(current_proc());
	el.el_flags = KAUTH_EXTLOOKUP_VALID_UID | KAUTH_EXTLOOKUP_VALID_GID | KAUTH_EXTLOOKUP_WANT_MEMBERSHIP;
	el.el_uid = pcred->cr_gmuid;
	el.el_gid = gid;
	el.el_member_valid = 0;         /* XXX set by resolver? */

	DTRACE_PROC2(kauth__group__resolver__submitted, int, el.el_uid, int, el.el_gid);

	error = kauth_resolver_submit(&el, 0ULL);

	DTRACE_PROC2(kauth__group__resolver__returned, int, error, int, el.el_flags);

	if (error != 0) {
		return error;
	}
	/* save the results from the lookup */
	kauth_groups_updatecache(&el);

	/* if we successfully ascertained membership, report */
	if (el.el_flags & KAUTH_EXTLOOKUP_VALID_MEMBERSHIP) {
		*resultp = (el.el_flags & KAUTH_EXTLOOKUP_ISMEMBER) ? 1 : 0;
		return 0;
	}

	return ENOENT;
#else
	*resultp = 0;
	return 0;
#endif
}

/*
 * kauth_cred_ismember_guid
 *
 * Description:	Determine whether the supplied credential is a member of the
 *		group nominated by GUID.
 *
 * Parameters:	cred				Credential to check in
 *		guidp				Pointer to GUID whose group
 *						we are testing for membership
 *		resultp				Pointer to int to contain the
 *						result of the call
 *
 * Returns:	0				Success
 *	kauth_cred_guid2gid:EINVAL
 *	kauth_cred_ismember_gid:ENOENT
 *	kauth_resolver_submit:ENOENT		User space daemon did not vend
 *						this credential.
 *	kauth_cred_ismember_gid:EWOULDBLOCK
 *	kauth_cred_ismember_gid:EINTR
 *	kauth_cred_ismember_gid:ENOMEM
 *	kauth_cred_ismember_gid:???		Unlikely error from user space
 *
 * Implicit returns:
 *		*resultp (modified)	1	Is member
 *					0	Is not member
 */
int
kauth_cred_ismember_guid(__unused kauth_cred_t cred, guid_t *guidp, int *resultp)
{
	int error = 0;

	switch (kauth_wellknown_guid(guidp)) {
	case KAUTH_WKG_NOBODY:
		*resultp = 0;
		break;
	case KAUTH_WKG_EVERYBODY:
		*resultp = 1;
		break;
	default:
	{
		gid_t gid;
#if CONFIG_EXT_RESOLVER
		struct kauth_identity ki;

		/*
		 * Grovel the identity cache looking for this GUID.
		 * If we find it, and it is for a user record, return
		 * false because it's not a group.
		 *
		 * This is necessary because we don't have -ve caching
		 * of group memberships, and we really want to avoid
		 * calling out to the resolver if at all possible.
		 *
		 * Because we're called by the ACL evaluator, and the
		 * ACL evaluator is likely to encounter ACEs for users,
		 * this is expected to be a common case.
		 */
		ki.ki_valid = 0;
		if ((error = kauth_identity_find_guid(guidp, &ki, NULL)) == 0 &&
		    !kauth_identity_guid_expired(&ki)) {
			if (ki.ki_valid & KI_VALID_GID) {
				/* It's a group after all... */
				gid = ki.ki_gid;
				goto do_check;
			}
			if (ki.ki_valid & KI_VALID_UID) {
				*resultp = 0;
				return 0;
			}
		}
#endif /* CONFIG_EXT_RESOLVER */
		/*
		 * Attempt to translate the GUID to a GID.  Even if
		 * this fails, we will have primed the cache if it is
		 * a user record and we'll see it above the next time
		 * we're asked.
		 */
		if ((error = kauth_cred_guid2gid(guidp, &gid)) != 0) {
			/*
			 * If we have no guid -> gid translation, it's not a group and
			 * thus the cred can't be a member.
			 */
			if (error == ENOENT) {
				*resultp = 0;
				error = 0;
			}
		} else {
#if CONFIG_EXT_RESOLVER
do_check:
#endif /* CONFIG_EXT_RESOLVER */
			error = kauth_cred_ismember_gid(cred, gid, resultp);
		}
	}
	break;
	}
	return error;
}

/*
 * kauth_cred_gid_subset
 *
 * Description:	Given two credentials, determine if all GIDs associated with
 *              the first are also associated with the second
 *
 * Parameters:	cred1				Credential to check for
 *              cred2				Credential to check in
 *		resultp				Pointer to int to contain the
 *						result of the call
 *
 * Returns:	0				Success
 *		non-zero			See kauth_cred_ismember_gid for
 *						error codes
 *
 * Implicit returns:
 *		*resultp (modified)	1	Is subset
 *					0	Is not subset
 *
 * Notes:	This function guarantees not to modify resultp when returning
 *		an error.
 */
int
kauth_cred_gid_subset(kauth_cred_t cred1, kauth_cred_t cred2, int *resultp)
{
	int i, err, res = 1;
	gid_t gid;
	posix_cred_t pcred1 = posix_cred_get(cred1);
	posix_cred_t pcred2 = posix_cred_get(cred2);

	/* First, check the local list of groups */
	for (i = 0; i < pcred1->cr_ngroups; i++) {
		gid = pcred1->cr_groups[i];
		if ((err = kauth_cred_ismember_gid(cred2, gid, &res)) != 0) {
			return err;
		}

		if (!res && gid != pcred2->cr_rgid && gid != pcred2->cr_svgid) {
			*resultp = 0;
			return 0;
		}
	}

	/* Check real gid */
	if ((err = kauth_cred_ismember_gid(cred2, pcred1->cr_rgid, &res)) != 0) {
		return err;
	}

	if (!res && pcred1->cr_rgid != pcred2->cr_rgid &&
	    pcred1->cr_rgid != pcred2->cr_svgid) {
		*resultp = 0;
		return 0;
	}

	/* Finally, check saved gid */
	if ((err = kauth_cred_ismember_gid(cred2, pcred1->cr_svgid, &res)) != 0) {
		return err;
	}

	if (!res && pcred1->cr_svgid != pcred2->cr_rgid &&
	    pcred1->cr_svgid != pcred2->cr_svgid) {
		*resultp = 0;
		return 0;
	}

	*resultp = 1;
	return 0;
}


/*
 * kauth_cred_issuser
 *
 * Description:	Fast replacement for issuser()
 *
 * Parameters:	cred				Credential to check for super
 *						user privileges
 *
 * Returns:	0				Not super user
 *		!0				Is super user
 *
 * Notes:	This function uses a magic number which is not a manifest
 *		constant; this is bad practice.
 */
int
kauth_cred_issuser(kauth_cred_t cred)
{
	return kauth_cred_getuid(cred) == 0;
}


/*
 * Credential KPI
 */

static smrh_key_t   kauth_cred_key(kauth_cred_t cred);
static uint32_t     kauth_cred_key_hash(smrh_key_t key, uint32_t seed);
static bool         kauth_cred_key_equ(smrh_key_t k1, smrh_key_t k2);
static uint32_t     kauth_cred_obj_hash(const struct smrq_slink *, uint32_t seed);
static bool         kauth_cred_obj_equ(const struct smrq_slink *, smrh_key_t);
static bool         kauth_cred_obj_try_get(void *);

struct ucred_rw {
	os_ref_atomic_t         crw_weak_ref;
	struct ucred           *crw_cred;
	struct smrq_slink       crw_link;
	struct smr_node         crw_node;
};

#define KAUTH_CRED_REF_MAX 0x0ffffffful

ZONE_DEFINE_ID(ZONE_ID_KAUTH_CRED, "cred", struct ucred, ZC_READONLY | ZC_ZFREE_CLEARMEM);
static KALLOC_TYPE_DEFINE(ucred_rw_zone, struct ucred_rw, KT_DEFAULT);
os_refgrp_decl(static, ucred_ref_grp, "ucred_rw", NULL);

SMRH_TRAITS_DEFINE(kauth_cred_traits, struct ucred_rw, crw_link,
    .domain      = &smr_proc_task,
    .key_hash    = kauth_cred_key_hash,
    .key_equ     = kauth_cred_key_equ,
    .obj_hash    = kauth_cred_obj_hash,
    .obj_equ     = kauth_cred_obj_equ,
    .obj_try_get = kauth_cred_obj_try_get);

static struct smr_shash kauth_cred_hash;

static inline void
ucred_rw_ref(struct ucred_rw *rw)
{
	os_ref_retain_raw(&rw->crw_weak_ref, &ucred_ref_grp);
}

static inline bool
ucred_rw_tryref(struct ucred_rw *rw)
{
	return os_ref_retain_try_raw(&rw->crw_weak_ref, &ucred_ref_grp);
}

static inline os_ref_count_t
ucred_rw_unref(struct ucred_rw *rw)
{
	return os_ref_release_raw(&rw->crw_weak_ref, &ucred_ref_grp);
}

static inline void
ucred_rw_unref_live(struct ucred_rw *rw)
{
	os_ref_release_live_raw(&rw->crw_weak_ref, &ucred_ref_grp);
}

__abortlike
static void
kauth_cred_panic_over_released(kauth_cred_t cred)
{
	panic("kauth_cred_unref: cred %p over-released", cred);
}

__abortlike
static void
kauth_cred_panic_over_retain(kauth_cred_t cred)
{
	panic("kauth_cred_ref: cred %p over-retained", cred);
}

static void
kauth_cred_hold(kauth_cred_t cred)
{
	unsigned long ref;

	ref = zalloc_ro_update_field_atomic(ZONE_ID_KAUTH_CRED,
	    cred, cr_ref, ZRO_ATOMIC_ADD_LONG, 1);
	if (ref >= KAUTH_CRED_REF_MAX) {
		kauth_cred_panic_over_retain(cred);
	}
}

static void
kauth_cred_drop(kauth_cred_t cred)
{
	unsigned long ref;

	ref = zalloc_ro_update_field_atomic(ZONE_ID_KAUTH_CRED,
	    cred, cr_ref, ZRO_ATOMIC_ADD_LONG, -1);
	if (__improbable(ref == 0 || ref > KAUTH_CRED_REF_MAX)) {
		kauth_cred_panic_over_released(cred);
	}
}

/*
 * kauth_cred_init
 *
 * Description:	Initialize the credential hash cache
 *
 * Parameters:	(void)
 *
 * Returns:	(void)
 *
 * Notes:	Intialize the credential hash cache for use; the credential
 *		hash cache is used convert duplicate credentials into a
 *		single reference counted credential in order to save wired
 *		kernel memory.  In practice, this generally means a desktop
 *		system runs with a few tens of credentials, instead of one
 *		per process, one per thread, one per vnode cache entry, and
 *		so on.  This generally results in savings of 200K or more
 *		(potentially much more on server systems).
 *
 *		We also create the kernel and init creds before lockdown
 *		so that vfs_context0 and initcred pointers can be made constant.
 *
 *		We do this in the "ZALLOC" stage because we need
 *		the kauth_cred_hash_mtx to be initialized,
 *		and to allocate the kernel cred.
 */
__startup_func
static void
kauth_cred_init(void)
{
	struct posix_cred kernel_cred_template = {
		.cr_ngroups = 1,
		.cr_flags   = CRF_NOMEMBERD,
	};

	smr_shash_init(&kauth_cred_hash, SMRSH_BALANCED, maxproc / 4);
	vfs_context0.vc_ucred = posix_cred_create(&kernel_cred_template);
}
STARTUP(ZALLOC, STARTUP_RANK_LAST, kauth_cred_init);

uid_t
kauth_getuid(void)
{
	return kauth_cred_getuid(kauth_cred_get());
}

uid_t
kauth_getruid(void)
{
	return kauth_cred_getruid(kauth_cred_get());
}

gid_t
kauth_getgid(void)
{
	return kauth_cred_getgid(kauth_cred_get());
}

gid_t
kauth_getrgid(void)
{
	return kauth_cred_getrgid(kauth_cred_get());
}

kauth_cred_t
kauth_cred_get(void)
{
	return current_thread_ro()->tro_cred;
}

intptr_t
current_thread_cred_label_get(int slot)
{
#if CONFIG_MACF
	return mac_label_get(kauth_cred_get()->cr_label, slot);
#else
	return 0;
#endif
}

__abortlike
static void
current_cached_proc_cred_panic(proc_t p)
{
	panic("current_cached_proc_cred(%p) called but current_proc() is %p",
	    p, current_proc());
}

kauth_cred_t
current_cached_proc_cred(proc_t p)
{
	thread_ro_t tro = current_thread_ro();

	if (tro->tro_proc != p && p != PROC_NULL) {
		current_cached_proc_cred_panic(p);
	}
	return tro->tro_realcred;
}

intptr_t
current_cached_proc_label_get(int slot)
{
#if CONFIG_MACF
	return mac_label_get(current_cached_proc_cred(PROC_NULL)->cr_label, slot);
#else
	return 0;
#endif
}

kauth_cred_t
current_cached_proc_cred_ref(proc_t p)
{
	kauth_cred_t cred = current_cached_proc_cred(p);

	kauth_cred_ref(cred);
	return cred;
}

__attribute__((noinline))
static void
kauth_cred_thread_update_slow(thread_ro_t tro, proc_t proc)
{
	struct ucred *cred = kauth_cred_proc_ref(proc);
	struct thread_ro_creds my_creds = tro->tro_creds;

	if (my_creds.tro_realcred != cred) {
		if (my_creds.tro_realcred == my_creds.tro_cred) {
			kauth_cred_set(&my_creds.tro_cred, cred);
		}
		kauth_cred_set(&my_creds.tro_realcred, cred);
		zalloc_ro_update_field(ZONE_ID_THREAD_RO,
		    tro, tro_creds, &my_creds);
	}
	kauth_cred_unref(&cred);
}

/*
 * current_cached_proc_cred_update
 *
 * Notes:	This code is common code called from system call or trap entry
 *		in the case that the process thread may have been changed
 *		since the last time the thread entered the kernel.
 */
__attribute__((always_inline))
void
current_cached_proc_cred_update(void)
{
	thread_ro_t tro  = current_thread_ro();
	proc_t      proc = tro->tro_proc;

	if (__improbable(tro->tro_task != kernel_task &&
	    tro->tro_realcred != proc_ucred_unsafe(proc))) {
		kauth_cred_thread_update_slow(tro, proc);
	}
}

kauth_cred_t
kauth_cred_get_with_ref(void)
{
	struct ucred *ucred = kauth_cred_get();
	kauth_cred_ref(ucred);
	return ucred;
}

__abortlike
static void
kauth_cred_verify_panic(kauth_cred_t cred, struct ucred_rw *cred_rw)
{
	panic("kauth_cred_t backref mismatch: cred:%p cred->cr_rw:%p "
	    "cred_rw:%p", cred, cred->cr_rw, cred_rw);
}

__pure2
static struct ucred_rw *
kauth_cred_rw(kauth_cred_t cred)
{
	struct ucred_rw *rw = kauth_cred_require(cred)->cr_rw;

	if (__improbable(rw->crw_cred != cred)) {
		kauth_cred_verify_panic(cred, rw);
	}

	return rw;
}


kauth_cred_t
kauth_cred_proc_ref(proc_t procp)
{
	kauth_cred_t cred;

	smr_proc_task_enter();
	cred = proc_ucred_smr(procp);
	if (!ucred_rw_tryref(kauth_cred_rw(cred))) {
		cred = NOCRED;
	}
	smr_proc_task_leave();

	if (__improbable(cred == NOCRED)) {
		proc_ucred_lock(procp);
		cred = proc_ucred_locked(procp);
		kauth_cred_ref(cred);
		proc_ucred_unlock(procp);
	}
	return cred;
}

static kauth_cred_t
__kauth_cred_proc_ref_for_pidversion_slow(pid_t pid, uint32_t pidvers, bool dovers)
{
	kauth_cred_t cred = NOCRED;
	proc_t procp;

	procp = proc_find(pid);
	if (procp == PROC_NULL) {
		return NOCRED;
	}

	if (dovers && proc_get_ro(procp)->p_idversion != pidvers) {
		proc_rele(procp);
		return NOCRED;
	}

	cred = kauth_cred_proc_ref(procp);
	proc_rele(procp);

	return cred;
}

static inline kauth_cred_t
__kauth_cred_proc_ref_for_pidversion(pid_t pid, uint32_t pidvers, bool dovers)
{
	kauth_cred_t cred = NOCRED;
	struct proc_ro *pro;
	proc_t procp;
	int err;

	smr_proc_task_enter();
	procp = proc_find_noref_smr(pid);
	if (procp == PROC_NULL) {
		err = ESRCH;
	} else {
		pro = proc_get_ro(procp);
		cred = proc_ucred_smr(procp);
		if (dovers && pro->p_idversion != pidvers) {
			err = ESRCH;
		} else if (!ucred_rw_tryref(kauth_cred_rw(cred))) {
			err = EAGAIN;
		} else {
			err = 0;
		}
	}
	smr_proc_task_leave();

	if (__probable(err == 0)) {
		return cred;
	}

	if (err == EAGAIN) {
		return __kauth_cred_proc_ref_for_pidversion_slow(pid, pidvers, dovers);
	}

	return NOCRED;
}

kauth_cred_t
kauth_cred_proc_ref_for_pid(pid_t pid)
{
	return __kauth_cred_proc_ref_for_pidversion(pid, 0, false);
}

kauth_cred_t
kauth_cred_proc_ref_for_pidversion(pid_t pid, uint32_t pidvers)
{
	return __kauth_cred_proc_ref_for_pidversion(pid, pidvers, true);
}

/*
 * kauth_cred_alloc
 *
 * Description: Create a deduplicated credential optionally derived
 *              from a parent credential, according to the specified template.
 *
 * Parameters:  parent_cred                     the parent cred the model is
 *                                              derived from (or NOCRED for
 *                                              a creation)
 *
 *              model_cred                      the (mutable) template of the
 *                                              cred to add to the hash table.
 *
 * Returns:     (kauth_thread_t)                The inserted cred, or the
 *                                              collision that was found.
 */
static kauth_cred_t
kauth_cred_alloc(kauth_cred_t parent_cred, kauth_cred_t model_cred)
{
	struct ucred_rw *found_rw;
	struct ucred_rw *new_rw;
	struct ucred *newcred;

	/*
	 * Step 1: find if there's a duplicate entry
	 */

	found_rw = smr_shash_get(&kauth_cred_hash, kauth_cred_key(model_cred),
	    &kauth_cred_traits);

	if (found_rw) {
		/* found a duplicate, free the label if the model owned it */
#if CONFIG_MACF
		if (!parent_cred || model_cred->cr_label != parent_cred->cr_label) {
			mac_cred_label_destroy(model_cred);
		}
#endif

		/* smr_hash_get() already did a kauth_cred_ro() */
		return found_rw->crw_cred;
	}

	/*
	 * Step 2: create a fresh new kauth_cred.
	 *
	 *         give it ownership of the label and audit session,
	 *         if it doesn't have it already.
	 */
#if CONFIG_MACF
	if (parent_cred && model_cred->cr_label == parent_cred->cr_label) {
		mac_cred_label_init(model_cred);
		mac_cred_label_associate(parent_cred, model_cred);
	}
	mac_cred_label_seal(model_cred);
#else
	(void)parent_cred;
#endif
	AUDIT_SESSION_REF(model_cred);

	new_rw = zalloc_flags(ucred_rw_zone, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	os_ref_init_raw(&new_rw->crw_weak_ref, &ucred_ref_grp);

	model_cred->cr_rw     = new_rw;
	model_cred->cr_unused = NULL;
	model_cred->cr_ref    = 0;

	newcred = zalloc_ro(ZONE_ID_KAUTH_CRED, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	new_rw->crw_cred = newcred;

#if HAS_APPLE_PAC
	{
		void *naked_ptr = model_cred->cr_label;
		void *signed_ptr;
		signed_ptr = ptrauth_sign_unauthenticated(naked_ptr,
		    ptrauth_key_process_independent_data,
		    ptrauth_blend_discriminator(&newcred->cr_label,
		    OS_PTRAUTH_DISCRIMINATOR("ucred.cr_label")));
		memcpy((void *)&model_cred->cr_label, &signed_ptr, sizeof(void *));
	}
#endif

	zalloc_ro_update_elem(ZONE_ID_KAUTH_CRED, newcred, model_cred);

	/*
	 * Step 3: try to insert in the hash table,
	 *         and deal someone else racing us.
	 */
	found_rw = smr_shash_get_or_insert(&kauth_cred_hash,
	    kauth_cred_key(newcred), &new_rw->crw_link, &kauth_cred_traits);
	if (__probable(!found_rw)) {
		return newcred;
	}

#if CONFIG_MACF
	mac_cred_label_free(newcred->cr_label);
#endif
	AUDIT_SESSION_UNREF(newcred);
	zfree(ucred_rw_zone, new_rw);
	zfree_ro(ZONE_ID_KAUTH_CRED, newcred);

	/* smr_shash_get_or_insert() already did a kauth_cred_ro() */
	return found_rw->crw_cred;
}

kauth_cred_t
kauth_cred_require(kauth_cred_t cred)
{
	zone_require_ro(ZONE_ID_KAUTH_CRED, sizeof(struct ucred), cred);
	return cred;
}

__abortlike
static void
kauth_cred_rw_verify_panic(const struct ucred_rw *cred_rw, kauth_cred_t cred)
{
	panic("ucred_rw backref mismatch: cred_rw:%p cred_rw->crw_cred:%p "
	    "cred: %p", cred_rw, cred_rw->crw_cred, cred);
}

__pure2
static kauth_cred_t
kauth_cred_ro(const struct ucred_rw *cred_rw)
{
	kauth_cred_t cred = kauth_cred_require(cred_rw->crw_cred);

	if (__improbable(cred->cr_rw != cred_rw)) {
		kauth_cred_rw_verify_panic(cred_rw, cred);
	}

	return cred;
}

__attribute__((noinline))
static void
kauth_cred_free(struct smr_node *node)
{
	struct ucred_rw *rw = __container_of(node, struct ucred_rw, crw_node);
	struct ucred *cred = kauth_cred_ro(rw);

	if (cred == vfs_context0.vc_ucred) {
		panic("Over-release of the kernel credentials");
	}
	if (os_atomic_load(&cred->cr_ref, relaxed) != 0) {
		panic("%s: freeing credential with active long-term ref", __func__);
	}

#if CONFIG_MACF
	mac_cred_label_free(cred->cr_label);
#endif

	zfree(ucred_rw_zone, rw);
	zfree_ro(ZONE_ID_KAUTH_CRED, cred);
}

__attribute__((noinline))
static void
kauth_cred_retire(struct ucred_rw *rw, struct ucred *cred __unused)
{
	vm_size_t size = sizeof(struct ucred_rw) +
#if CONFIG_MACF
	    sizeof(struct label) +
#endif
	    sizeof(struct ucred);

	smr_shash_remove(&kauth_cred_hash, &rw->crw_link, &kauth_cred_traits);
	AUDIT_SESSION_UNREF(cred); /* uses SMR, safe to do immediately */
	smr_call(&smr_proc_task, &rw->crw_node, size, kauth_cred_free);
}

static kauth_cred_t
posix_cred_create_internal(posix_cred_t pcred, struct au_session audit)
{
	struct ucred model = {
		.cr_posix = *pcred,
		.cr_label = NULL,
		.cr_audit = audit,
	};
	int is_member = 0;

	pcred = posix_cred_get(&model);

	if (pcred->cr_ngroups < 1) {
		return NOCRED;
	}

	if (pcred->cr_flags & CRF_NOMEMBERD) {
		pcred->cr_gmuid = KAUTH_UID_NONE;
	} else {
		/*
		 * If the template credential is not opting out of external
		 * group membership resolution, then we need to check that
		 * the UID we will be using is resolvable by the external
		 * resolver.  If it's not, then we opt it out anyway, since
		 * all future external resolution requests will be failing
		 * anyway, and potentially taking a long time to do it.  We
		 * use gid 0 because we always know it will exist and not
		 * trigger additional lookups. This is OK, because we end up
		 * precatching the information here as a result.
		 */
		if (!kauth_cred_ismember_gid(&model, 0, &is_member)) {
			/*
			 * It's a recognized value; we don't really care about
			 * the answer, so long as it's something the external
			 * resolver could have vended.
			 */
			pcred->cr_gmuid = pcred->cr_uid;
		} else {
			/*
			 * It's not something the external resolver could
			 * have vended, so we don't want to ask it more
			 * questions about the credential in the future. This
			 * speeds up future lookups, as long as the caller
			 * caches results; otherwise, it the same recurring
			 * cost.  Since most credentials are used multiple
			 * times, we still get some performance win from this.
			 */
			pcred->cr_gmuid = KAUTH_UID_NONE;
			pcred->cr_flags |= CRF_NOMEMBERD;
		}
	}

	mac_cred_label_init(&model);
	return kauth_cred_alloc(NOCRED, &model);
}

/*
 * kauth_cred_create
 *
 * Description:	Obsolete function that is unfortunately exported,
 *              but that no one should use directly.
 *
 * Parameters:	cred				Template for credential to
 *						be created
 *
 * Returns:	(kauth_cred_t)			The credential that was found
 *						in the hash or created
 *		NULL				kauth_cred_add() failed, or
 *						there was not an egid specified
 *
 * Notes:	The gmuid is hard-defaulted to the UID specified.  Since we
 *		maintain this field, we can't expect callers to know how it
 *		needs to be set.  Callers should be prepared for this field
 *		to be overwritten.
 */
kauth_cred_t
kauth_cred_create(kauth_cred_t cred)
{
	return posix_cred_create_internal(&cred->cr_posix, cred->cr_audit);
}

kauth_cred_t
kauth_cred_derive(kauth_cred_t cred, kauth_cred_derive_t derive_fn)
{
	struct ucred model = {
		.cr_posix = cred->cr_posix,
		.cr_label = cred->cr_label,
		.cr_audit = cred->cr_audit,
	};

	if (derive_fn(cred, &model)) {
		return kauth_cred_alloc(cred, &model);
	}

	kauth_cred_ref(cred);
	return cred;
}


bool
kauth_cred_proc_update(
	proc_t                  p,
	proc_settoken_t         action,
	kauth_cred_derive_t     derive_fn)
{
	kauth_cred_t cur_cred, free_cred, new_cred;

	cur_cred = kauth_cred_proc_ref(p);

	for (;;) {
		new_cred = kauth_cred_derive(cur_cred, derive_fn);
		if (new_cred == cur_cred) {
			if (action == PROC_SETTOKEN_ALWAYS) {
				set_security_token(p, cur_cred);
			}
			kauth_cred_unref(&new_cred);
			kauth_cred_unref(&cur_cred);
			return false;
		}

		proc_ucred_lock(p);
		if (__probable(proc_ucred_locked(p) == cur_cred)) {
			kauth_cred_ref(new_cred);
			kauth_cred_hold(new_cred);

			zalloc_ro_mut(ZONE_ID_PROC_RO, proc_get_ro(p),
			    offsetof(struct proc_ro, p_ucred),
			    &new_cred, sizeof(struct ucred *));

			kauth_cred_drop(cur_cred);
			ucred_rw_unref_live(cur_cred->cr_rw);

			proc_update_creds_onproc(p, new_cred);
			proc_ucred_unlock(p);

			if (action == PROC_SETTOKEN_SETUGID) {
				OSBitOrAtomic(P_SUGID, &p->p_flag);
			}
			if (action != PROC_SETTOKEN_NONE) {
				set_security_token(p, new_cred);
			}

			kauth_cred_unref(&new_cred);
			kauth_cred_unref(&cur_cred);
			return true;
		}

		free_cred = cur_cred;
		cur_cred = proc_ucred_locked(p);
		kauth_cred_ref(cur_cred);
		proc_ucred_unlock(p);

		kauth_cred_unref(&free_cred);
		kauth_cred_unref(&new_cred);
	}
}


/*
 * kauth_cred_model_setresuid
 *
 * Description:	Update the given credential using the UID arguments.  The given
 *		UIDs are used to set the effective UID, real UID, saved UID,
 *		and GMUID (used for group membership checking).
 *
 * Parameters:	model				The model credential
 *		ruid				The new real UID
 *		euid				The new effective UID
 *		svuid				The new saved UID
 *		gmuid				KAUTH_UID_NONE -or- the new
 *						group membership UID
 *
 * Returns:	(kauth_cred_t)			The updated credential
 *
 * Note:	gmuid is different in that a KAUTH_UID_NONE is a valid
 *		setting, so if you don't want it to change, pass it the
 *		previous value, explicitly.
 */
bool
kauth_cred_model_setresuid(
	kauth_cred_t            model,
	uid_t                   ruid,
	uid_t                   euid,
	uid_t                   svuid,
	uid_t                   gmuid)
{
	posix_cred_t pcred = posix_cred_get(model);
	bool updated = false;

	/*
	 * We don't need to do anything if the UIDs we are changing are
	 * already the same as the UIDs passed in
	 */
	if (euid != KAUTH_UID_NONE && pcred->cr_uid != euid) {
		pcred->cr_uid = euid;
		updated = true;
	}

	if (ruid != KAUTH_UID_NONE && pcred->cr_ruid != ruid) {
		pcred->cr_ruid = ruid;
		updated = true;
	}

	if (svuid != KAUTH_UID_NONE && pcred->cr_svuid != svuid) {
		pcred->cr_svuid = svuid;
		updated = true;
	}

	if (pcred->cr_gmuid != gmuid) {
		/*
		 * If we are setting the gmuid to KAUTH_UID_NONE, then we want
		 * to opt out of participation in external group resolution,
		 * unless we explicitly opt back in later.
		 */
		pcred->cr_gmuid = gmuid;
		if (gmuid == KAUTH_UID_NONE) {
			pcred->cr_flags |= CRF_NOMEMBERD;
		}
		updated = true;
	}

	return updated;
}


/*
 * kauth_cred_model_setresgid
 *
 * Description:	Update the given credential using the GID arguments.  The given
 *		GIDs are used to set the effective GID, real GID, and saved
 *		GID.
 *
 * Parameters:	model				The model credential
 *		rgid				The new real GID
 *		egid				The new effective GID
 *		svgid				The new saved GID
 *
 * Returns:	(kauth_cred_t)			The updated credential
 */
bool
kauth_cred_model_setresgid(
	kauth_cred_t            model,
	gid_t                   rgid,
	gid_t                   egid,
	gid_t                   svgid)
{
	posix_cred_t pcred = posix_cred_get(model);
	bool updated = false;

	if (egid != KAUTH_GID_NONE && pcred->cr_gid != egid) {
		if (kauth_cred_change_egid(model, egid)) {
			pcred->cr_flags |= CRF_NOMEMBERD;
			pcred->cr_gmuid = KAUTH_UID_NONE;
		}
		updated = true;
	}

	if (rgid != KAUTH_GID_NONE && pcred->cr_rgid != rgid) {
		pcred->cr_rgid = rgid;
		updated = true;
	}
	if (svgid != KAUTH_GID_NONE && pcred->cr_svgid != svgid) {
		pcred->cr_svgid = svgid;
		updated = true;
	}

	return updated;
}


/*
 * Update the given credential with the given groups.  We only allocate a new
 *	credential when the given gid actually results in changes to the existing
 *	credential.
 *	The gmuid argument supplies a new uid (or KAUTH_UID_NONE to opt out)
 *	which will be used for group membership checking.
 */
/*
 * kauth_cred_model_setgroups
 *
 * Description:	Update the given credential using the provide supplementary
 *		group list and group membership UID
 *
 * Parameters:	cred				The model credential
 *		groups				Pointer to gid_t array which
 *						contains the new group list
 *		groupcount			The count of valid groups which
 *						are contained in 'groups'
 *		gmuid				KAUTH_UID_NONE -or- the new
 *						group membership UID
 *
 * Returns:	(kauth_cred_t)			The updated credential
 *
 * Note:	gmuid is different in that a KAUTH_UID_NONE is a valid
 *		setting, so if you don't want it to change, pass it the
 *		previous value, explicitly.
 *
 * XXX:		Changes are determined in ordinal order - if the caller passes
 *		in the same groups list that is already present in the
 *		credential, but the members are in a different order, even if
 *		the EGID is not modified (i.e. cr_groups[0] is the same), it
 *		is considered a modification to the credential, and a new
 *		credential is created.
 *
 *		This should perhaps be better optimized, but it is considered
 *		to be the caller's problem.
 */
bool
kauth_cred_model_setgroups(
	kauth_cred_t            model,
	gid_t                  *groups,
	size_t                  groupcount,
	uid_t                   gmuid)
{
	posix_cred_t pcred = posix_cred_get(model);

	assert(groupcount <= NGROUPS);
	groupcount = MIN(groupcount, NGROUPS);

	/*
	 * We don't need to do anything if the given list of groups does not
	 * change.
	 */
	if (pcred->cr_gmuid == gmuid &&
	    pcred->cr_ngroups == groupcount &&
	    memcmp(pcred->cr_groups, groups, groupcount * sizeof(gid_t)) == 0) {
		return false;
	}

	pcred->cr_gmuid = gmuid;
	pcred->cr_ngroups = (short)groupcount;
	memcpy(pcred->cr_groups, groups, groupcount * sizeof(gid_t));
	if (gmuid == KAUTH_UID_NONE) {
		pcred->cr_flags |= CRF_NOMEMBERD;
	} else {
		pcred->cr_flags &= ~CRF_NOMEMBERD;
	}
	return true;
}

/*
 * Notes:	The return value exists to account for the possibility of a
 *		kauth_cred_t without a POSIX label.  This will be the case in
 *		the future (see posix_cred_get() below, for more details).
 */
#if CONFIG_EXT_RESOLVER
int kauth_external_supplementary_groups_supported = 1;

SYSCTL_INT(_kern, OID_AUTO, ds_supgroups_supported, CTLFLAG_RW | CTLFLAG_LOCKED, &kauth_external_supplementary_groups_supported, 0, "");
#endif

int
kauth_cred_getgroups(kauth_cred_t cred, gid_t *grouplist, size_t *countp)
{
	size_t limit = NGROUPS;
	posix_cred_t pcred;

	if (cred == NULL) {
		KAUTH_DEBUG("kauth_cred_getgroups got NULL credential");
		return EINVAL;
	}

	if (grouplist == NULL) {
		KAUTH_DEBUG("kauth_cred_getgroups got NULL group list");
		return EINVAL;
	}

	pcred = posix_cred_get(cred);

#if CONFIG_EXT_RESOLVER
	/*
	 * If we've not opted out of using the resolver, then convert the cred to a list
	 * of supplemental groups. We do this only if there has been a resolver to talk to,
	 * since we may be too early in boot, or in an environment that isn't using DS.
	 */
	if (kauth_identitysvc_has_registered && kauth_external_supplementary_groups_supported && (pcred->cr_flags & CRF_NOMEMBERD) == 0) {
		uid_t uid = kauth_cred_getuid(cred);
		int err;

		err = kauth_cred_uid2groups(&uid, grouplist, countp);
		if (!err) {
			return 0;
		}

		/* On error just fall through */
		KAUTH_DEBUG("kauth_cred_getgroups failed %d\n", err);
	}
#endif /* CONFIG_EXT_RESOLVER */

	/*
	 * If they just want a copy of the groups list, they may not care
	 * about the actual count.  If they specify an input count, however,
	 * treat it as an indicator of the buffer size available in grouplist,
	 * and limit the returned list to that size.
	 */
	if (countp) {
		limit = MIN(*countp, pcred->cr_ngroups);
		*countp = limit;
	}

	memcpy(grouplist, pcred->cr_groups, sizeof(gid_t) * limit);

	return 0;
}


/*
 * kauth_cred_model_setuidgid
 *
 * Description:	Update the given credential using the UID and GID arguments.
 *		The given UID is used to set the effective UID, real UID, and
 *		saved UID.  The given GID is used to set the effective GID,
 *		real GID, and saved GID.
 *
 * Parameters:	model				The model credential
 *		uid				The new UID to use
 *		gid				The new GID to use
 *
 * Returns:	(kauth_cred_t)			The updated credential
 *
 * Notes:	We set the gmuid to uid if the credential we are inheriting
 *		from has not opted out of memberd participation; otherwise
 *		we set it to KAUTH_UID_NONE
 *
 *		This code is only ever called from the per-thread credential
 *		code path in the "set per thread credential" case; and in
 *		posix_spawn() in the case that the POSIX_SPAWN_RESETIDS
 *		flag is set.
 */
bool
kauth_cred_model_setuidgid(kauth_cred_t model, uid_t uid, gid_t gid)
{
	struct posix_cred pcred = {
		.cr_uid     = uid,
		.cr_ruid    = uid,
		.cr_svuid   = uid,

		.cr_ngroups = 1,
		.cr_gid     = gid,
		.cr_rgid    = gid,
		.cr_svgid   = gid,

		.cr_flags   = model->cr_posix.cr_flags,
	};

	/* inherit the opt-out of memberd */
	if (pcred.cr_flags & CRF_NOMEMBERD) {
		pcred.cr_gmuid = KAUTH_UID_NONE;
	} else {
		pcred.cr_gmuid = uid;
	}

	if (memcmp(&model->cr_posix, &pcred, sizeof(struct posix_cred)) != 0) {
		model->cr_posix = pcred;
		return true;
	}

	return false;
}


/*
 * kauth_cred_model_setauditinfo
 *
 * Description:	Update the given credential using the given au_session_t.
 *
 * Parameters:	model				The model credential
 *		auditinfo_p			Pointer to ne audit information
 *
 * Returns:	(kauth_cred_t)			The updated credential
 */
bool
kauth_cred_model_setauditinfo(kauth_cred_t model, au_session_t *auditinfo_p)
{
	if (memcmp(&model->cr_audit, auditinfo_p, sizeof(model->cr_audit)) != 0) {
		model->cr_audit = *auditinfo_p;
		return true;
	}


	return false;
}

#if CONFIG_MACF
kauth_cred_t
kauth_cred_label_update(kauth_cred_t cred, struct label *label)
{
	kauth_cred_t new_cred;

	new_cred = kauth_cred_derive(cred,
	    ^bool (kauth_cred_t parent, kauth_cred_t model) {
		mac_cred_label_init(model);
		mac_cred_label_associate(parent, model);
		mac_cred_label_update(model, label);
		return true;
	});

	kauth_cred_unref(&cred);
	return new_cred;
}

int
kauth_proc_label_update(struct proc *p, struct label *label)
{
	kauth_cred_proc_update(p, PROC_SETTOKEN_NONE,
	    ^bool (kauth_cred_t parent, kauth_cred_t model) {
		mac_cred_label_init(model);
		mac_cred_label_associate(parent, model);
		mac_cred_label_update(model, label);
		return true;
	});
	return 0;
}

/*
 *  kauth_proc_label_update_execve
 *
 * Description: Update the label inside the credential associated with the
 *		process as part of a transitioning execve.  The label will
 *		be updated by the policies as part of this processing, not
 *		provided up front.
 *
 * Parameters:	p			The process to modify
 *		ctx			The context of the exec
 *		vp			The vnode being exec'ed
 *		scriptl			The script MAC label
 *		execl			The executable MAC label
 *		lupdateerror	The error place holder for MAC label authority
 *						to update about possible termination
 *
 * Returns:	0			Label update did not make credential
 *					disjoint
 *		1			Label update caused credential to be
 *					disjoint
 *
 * Notes:	The credential associated with the process WILL change as a
 *		result of this call.  The caller should not assume the process
 *		reference to the old credential still exists.
 */

void
kauth_proc_label_update_execve(struct proc *p, vfs_context_t ctx,
    struct vnode *vp, off_t offset, struct vnode *scriptvp, struct label *scriptl,
    struct label *execl, unsigned int *csflags, void *macextensions, int *disjoint, int *update_return)
{
	kauth_cred_proc_update(p, PROC_SETTOKEN_NONE,
	    ^bool (kauth_cred_t parent, kauth_cred_t model) {
		mac_cred_label_init(model);
		mac_cred_label_associate(parent, model);
		mac_cred_label_update_execve(ctx, model,
		vp, offset, scriptvp, scriptl, execl, csflags,
		macextensions, disjoint, update_return);
		return true;
	});
}
#else
kauth_cred_t
kauth_cred_label_update(__unused kauth_cred_t cred, __unused struct label *label)
{
	return NULL;
}

int
kauth_proc_label_update(__unused struct proc *p, __unused struct label *label)
{
	return 0;
}
#endif


void
kauth_cred_ref(kauth_cred_t cred)
{
	ucred_rw_ref(kauth_cred_rw(cred));
}

void
(kauth_cred_unref)(kauth_cred_t * credp)
{
	struct ucred    *cred = *credp;
	struct ucred_rw *rw   = kauth_cred_rw(cred);

	*credp = NOCRED;

	if (ucred_rw_unref(rw) == 0) {
		kauth_cred_retire(rw, cred);
	}
}

/*
 * kauth_cred_set
 *
 * Description:	Store a long-term credential reference to a credential pointer,
 *		dropping the long-term reference on any previous credential held
 *		at the address.
 *
 * Parameters:	credp				Pointer to the credential
 *						storage field.  If *credp points
 *						to a valid credential before
 *						this call, its long-term
 *						reference will be dropped.
 *		new_cred			The new credential to take a
 *						long-term reference to and
 *						assign to *credp.  May be
 *						NOCRED.
 *
 * Returns:	(void)
 *
 * Notes:	Taking/dropping a long-term reference is costly in terms of
 *		performance.
 */
void
(kauth_cred_set)(kauth_cred_t * credp, kauth_cred_t new_cred)
{
	kauth_cred_t old_cred = *credp;

	if (old_cred != new_cred) {
		if (IS_VALID_CRED(new_cred)) {
			kauth_cred_ref(new_cred);
			kauth_cred_hold(new_cred);
		}

		*credp = new_cred;

		if (IS_VALID_CRED(old_cred)) {
			kauth_cred_drop(old_cred);
			kauth_cred_unref(&old_cred);
		}
	}
}

/*
 * kauth_cred_copy_real
 *
 * Description:	Returns a credential based on the passed credential but which
 *		reflects the real rather than effective UID and GID.
 *
 * Parameters:	cred				The credential from which to
 *						derive the new credential
 *
 * Returns:	(kauth_cred_t)			The copied credential
 *
 * IMPORTANT:	This function DOES NOT utilize kauth_cred_update(); as a
 *		result, the caller is responsible for dropping BOTH the
 *		additional reference on the passed cred (if any), and the
 *		credential returned by this function.  The drop should be
 *		via the kauth_cred_unref() KPI.
 */
kauth_cred_t
kauth_cred_copy_real(kauth_cred_t cred)
{
	kauth_cred_derive_t fn = ^bool (kauth_cred_t parent __unused, kauth_cred_t model) {
		posix_cred_t pcred = posix_cred_get(model);

		/* if the credential is already 'real', just take a reference */
		if ((pcred->cr_ruid == pcred->cr_uid) &&
		    (pcred->cr_rgid == pcred->cr_gid)) {
			return false;
		}

		pcred->cr_uid = pcred->cr_ruid;
		/* displacing a supplementary group opts us out of memberd */
		if (kauth_cred_change_egid(model, pcred->cr_rgid)) {
			pcred->cr_flags |= CRF_NOMEMBERD;
			pcred->cr_gmuid = KAUTH_UID_NONE;
		}
		/*
		 * If the cred is not opted out, make sure we are using the r/euid
		 * for group checks
		 */
		if (pcred->cr_gmuid != KAUTH_UID_NONE) {
			pcred->cr_gmuid = pcred->cr_ruid;
		}
		return true;
	};

	return kauth_cred_derive(cred, fn);
}

/*
 * Hash table traits methods
 */
static smrh_key_t
kauth_cred_key(kauth_cred_t cred)
{
	return (smrh_key_t){ .smrk_opaque = cred };
}

static uint32_t
kauth_cred_ro_hash(const struct ucred *cred, uint32_t seed)
{
	uint32_t hash = seed;

	hash = os_hash_jenkins_update(&cred->cr_posix,
	    sizeof(struct posix_cred), hash);
	hash = os_hash_jenkins_update(&cred->cr_audit,
	    sizeof(struct au_session), hash);
#if CONFIG_MACF
	if (cred->cr_posix.cr_flags & CRF_MAC_ENFORCE) {
		hash = mac_cred_label_hash_update(cred->cr_label, hash);
	}
#endif /* CONFIG_MACF */

	return hash;
}
static uint32_t
kauth_cred_key_hash(smrh_key_t key, uint32_t seed)
{
	return kauth_cred_ro_hash(key.smrk_opaque, seed);
}
static uint32_t
kauth_cred_obj_hash(const struct smrq_slink *link, uint32_t seed)
{
	const struct ucred_rw *rw;

	rw = __container_of(link, struct ucred_rw, crw_link);
	/* this is used during rehash, re-auth the objects as we do */
	return kauth_cred_ro_hash(kauth_cred_ro(rw), seed);
}

static bool
kauth_cred_key_equ(smrh_key_t k1, smrh_key_t k2)
{
	const struct ucred *cred1 = k1.smrk_opaque;
	const struct ucred *cred2 = k2.smrk_opaque;
	const struct posix_cred *pcred1 = &cred1->cr_posix;
	const struct posix_cred *pcred2 = &cred2->cr_posix;

	/*
	 * don't worry about the label unless the flags in
	 * either credential tell us to.
	 */
	if (memcmp(pcred1, pcred2, sizeof(*pcred1))) {
		return false;
	}
	if (memcmp(&cred1->cr_audit, &cred2->cr_audit, sizeof(cred1->cr_audit))) {
		return false;
	}
#if CONFIG_MACF
	/* Note: we know the flags are equal, so we only need to test one */
	if (pcred1->cr_flags & CRF_MAC_ENFORCE) {
		if (!mac_cred_label_is_equal(cred1->cr_label, cred2->cr_label)) {
			return false;
		}
	}
#endif
	return true;
}
static bool
kauth_cred_obj_equ(const struct smrq_slink *link, smrh_key_t key)
{
	const struct ucred_rw *rw;

	rw = __container_of(link, struct ucred_rw, crw_link);
	/* only do the kauth_cred_ro() check in try_get() */
	return kauth_cred_key_equ(kauth_cred_key(rw->crw_cred), key);
}

static bool
kauth_cred_obj_try_get(void *obj)
{
	struct ucred_rw *rw = obj;
	kauth_cred_t cred = kauth_cred_require(rw->crw_cred);

	if (__improbable(cred->cr_rw != rw)) {
		kauth_cred_rw_verify_panic(rw, cred);
	}

	return ucred_rw_tryref(rw);
}

/*
 **********************************************************************
 * The following routines will be moved to a policy_posix.c module at
 * some future point.
 **********************************************************************
 */

/*
 * posix_cred_create
 *
 * Description:	Helper function to create a kauth_cred_t credential that is
 *		initally labelled with a specific POSIX credential label
 *
 * Parameters:	pcred			The posix_cred_t to use as the initial
 *					label value
 *
 * Returns:	(kauth_cred_t)		The credential that was found in the
 *					hash or creates
 *		NULL			kauth_cred_make() failed, or there was
 *		                        no egid specified, or we failed to
 *					attach a label to the new credential
 *
 * Notes:	The gmuid is hard-defaulted to the UID specified.  Since we
 *		maintain this field, we can't expect callers to know how it
 *		needs to be set.  Callers should be prepared for this field
 *		to be overwritten.
 */
kauth_cred_t
posix_cred_create(posix_cred_t pcred)
{
	struct au_session audit = {
		.as_aia_p = audit_default_aia_p,
	};

	return posix_cred_create_internal(pcred, audit);
}


/*
 * posix_cred_get
 *
 * Description:	Given a kauth_cred_t, return the POSIX credential label, if
 *		any, which is associated with it.
 *
 * Parameters:	cred			The credential to obtain the label from
 *
 * Returns:	posix_cred_t		The POSIX credential label
 *
 * Notes:	In the event that the policy_posix MACF module IS NOT loaded,
 *		this function will return a pointer to a posix_cred_t which
 *		GRANTS all access (effectively, a "root" credential).  This is
 *		necessary to support legacy code which insists on tightly
 *		integrating POSIX credentials into its APIs, including, but
 *		not limited to, System V IPC mechanisms, POSIX IPC mechanisms,
 *		NFSv3, signals, dtrace, and a large number of kauth routines
 *		used to implement POSIX permissions related system calls.
 *
 *		In the event that the policy_posix MACF module IS loaded, and
 *		there is no POSIX label on the kauth_cred_t credential, this
 *		function will return a pointer to a posix_cred_t which DENIES
 *		all access (effectively, a "deny rights granted by POSIX"
 *		credential).  This is necessary to support the concept of a
 *		transiently loaded POSIX policy, or kauth_cred_t credentials
 *		which can not be used in conjunctions with POSIX permissions
 *		checks.
 *
 *		This function currently returns the address of the cr_posix
 *		field of the supplied kauth_cred_t credential, and as such
 *		currently can not fail.  In the future, this will not be the
 *		case.
 */
posix_cred_t
posix_cred_get(kauth_cred_t cred)
{
	return &cred->cr_posix;
}


/*
 * posix_cred_access
 *
 * Description:	Perform a POSIX access check for a protected object
 *
 * Parameters:	cred			The credential to check
 *		object_uid		The POSIX UID of the protected object
 *		object_gid		The POSIX GID of the protected object
 *		object_mode		The POSIX mode of the protected object
 *		mode_req		The requested POSIX access rights
 *
 * Returns	0			Access is granted
 *		EACCES			Access is denied
 *
 * Notes:	This code optimizes the case where the world and group rights
 *		would both grant the requested rights to avoid making a group
 *		membership query.  This is a big performance win in the case
 *		where this is true.
 */
int
posix_cred_access(kauth_cred_t cred, id_t object_uid, id_t object_gid, mode_t object_mode, mode_t mode_req)
{
	int is_member;
	mode_t mode_owner = (object_mode & S_IRWXU);
	mode_t mode_group = (mode_t)((object_mode & S_IRWXG) << 3);
	mode_t mode_world = (mode_t)((object_mode & S_IRWXO) << 6);

	/*
	 * Check first for owner rights
	 */
	if (kauth_cred_getuid(cred) == object_uid && (mode_req & mode_owner) == mode_req) {
		return 0;
	}

	/*
	 * Combined group and world rights check, if we don't have owner rights
	 *
	 * OPTIMIZED: If group and world rights would grant the same bits, and
	 * they set of requested bits is in both, then we can simply check the
	 * world rights, avoiding a group membership check, which is expensive.
	 */
	if ((mode_req & mode_group & mode_world) == mode_req) {
		return 0;
	} else {
		/*
		 * NON-OPTIMIZED: requires group membership check.
		 */
		if ((mode_req & mode_group) != mode_req) {
			/*
			 * exclusion group : treat errors as "is a member"
			 *
			 * NON-OPTIMIZED: +group would deny; must check group
			 */
			if (!kauth_cred_ismember_gid(cred, object_gid, &is_member) && is_member) {
				/*
				 * DENY: +group denies
				 */
				return EACCES;
			} else {
				if ((mode_req & mode_world) != mode_req) {
					/*
					 * DENY: both -group & world would deny
					 */
					return EACCES;
				} else {
					/*
					 * ALLOW: allowed by -group and +world
					 */
					return 0;
				}
			}
		} else {
			/*
			 * inclusion group; treat errors as "not a member"
			 *
			 * NON-OPTIMIZED: +group allows, world denies; must
			 * check group
			 */
			if (!kauth_cred_ismember_gid(cred, object_gid, &is_member) && is_member) {
				/*
				 * ALLOW: allowed by +group
				 */
				return 0;
			} else {
				if ((mode_req & mode_world) != mode_req) {
					/*
					 * DENY: both -group & world would deny
					 */
					return EACCES;
				} else {
					/*
					 * ALLOW: allowed by -group and +world
					 */
					return 0;
				}
			}
		}
	}
}
