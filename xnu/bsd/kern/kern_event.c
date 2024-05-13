/*
 * Copyright (c) 2000-2021 Apple Inc. All rights reserved.
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
 *
 */
/*-
 * Copyright (c) 1999,2000,2001 Jonathan Lemon <jlemon@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *	@(#)kern_event.c       1.0 (3/31/2000)
 */
#include <stdint.h>
#include <machine/atomic.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/proc_internal.h>
#include <sys/kauth.h>
#include <sys/malloc.h>
#include <sys/unistd.h>
#include <sys/file_internal.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/queue.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/syscall.h> // SYS_* constants
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/sysproto.h>
#include <sys/user.h>
#include <sys/vnode_internal.h>
#include <string.h>
#include <sys/proc_info.h>
#include <sys/codesign.h>
#include <sys/pthread_shims.h>
#include <sys/kdebug.h>
#include <os/base.h>
#include <pexpert/pexpert.h>

#include <kern/thread_group.h>
#include <kern/locks.h>
#include <kern/clock.h>
#include <kern/cpu_data.h>
#include <kern/policy_internal.h>
#include <kern/thread_call.h>
#include <kern/sched_prim.h>
#include <kern/waitq.h>
#include <kern/zalloc.h>
#include <kern/kalloc.h>
#include <kern/assert.h>
#include <kern/ast.h>
#include <kern/thread.h>
#include <kern/kcdata.h>
#include <kern/work_interval.h>

#include <pthread/priority_private.h>
#include <pthread/workqueue_syscalls.h>
#include <pthread/workqueue_internal.h>
#include <libkern/libkern.h>

#include <os/log.h>

#include "net/net_str_id.h"

#if SKYWALK && defined(XNU_TARGET_OS_OSX)
#include <skywalk/lib/net_filter_event.h>

extern bool net_check_compatible_alf(void);
#endif /* SKYWALK && XNU_TARGET_OS_OSX */

#include <mach/task.h>
#include <libkern/section_keywords.h>

#if CONFIG_MEMORYSTATUS
#include <sys/kern_memorystatus.h>
#endif

#if DEVELOPMENT || DEBUG
#define KEVENT_PANIC_ON_WORKLOOP_OWNERSHIP_LEAK  (1U << 0)
#define KEVENT_PANIC_ON_NON_ENQUEUED_PROCESS     (1U << 1)
TUNABLE(uint32_t, kevent_debug_flags, "kevent_debug", 0);
#endif

static LCK_GRP_DECLARE(kq_lck_grp, "kqueue");
SECURITY_READ_ONLY_EARLY(vm_packing_params_t) kn_kq_packing_params =
    VM_PACKING_PARAMS(KNOTE_KQ_PACKED);

extern mach_port_name_t ipc_entry_name_mask(mach_port_name_t name); /* osfmk/ipc/ipc_entry.h */
extern int cansignal(struct proc *, kauth_cred_t, struct proc *, int); /* bsd/kern/kern_sig.c */

#define KEV_EVTID(code) BSDDBG_CODE(DBG_BSD_KEVENT, (code))

static int kqueue_select(struct fileproc *fp, int which, void *wq_link_id,
    vfs_context_t ctx);
static int kqueue_close(struct fileglob *fg, vfs_context_t ctx);
static int kqueue_kqfilter(struct fileproc *fp, struct knote *kn,
    struct kevent_qos_s *kev);
static int kqueue_drain(struct fileproc *fp, vfs_context_t ctx);

static const struct fileops kqueueops = {
	.fo_type     = DTYPE_KQUEUE,
	.fo_read     = fo_no_read,
	.fo_write    = fo_no_write,
	.fo_ioctl    = fo_no_ioctl,
	.fo_select   = kqueue_select,
	.fo_close    = kqueue_close,
	.fo_drain    = kqueue_drain,
	.fo_kqfilter = kqueue_kqfilter,
};

static inline int kevent_modern_copyout(struct kevent_qos_s *, user_addr_t *);
static int kevent_register_wait_prepare(struct knote *kn, struct kevent_qos_s *kev, int result);
static void kevent_register_wait_block(struct turnstile *ts, thread_t handoff_thread,
    thread_continue_t cont, struct _kevent_register *cont_args) __dead2;
static void kevent_register_wait_return(struct _kevent_register *cont_args) __dead2;
static void kevent_register_wait_cleanup(struct knote *kn);

static struct kqtailq *kqueue_get_suppressed_queue(kqueue_t kq, struct knote *kn);
static void kqueue_threadreq_initiate(struct kqueue *kq, workq_threadreq_t, kq_index_t qos, int flags);

static void kqworkq_unbind(proc_t p, workq_threadreq_t);
static thread_qos_t kqworkq_unbind_locked(struct kqworkq *kqwq, workq_threadreq_t, thread_t thread);
static workq_threadreq_t kqworkq_get_request(struct kqworkq *kqwq, kq_index_t qos_index);
static void kqueue_update_iotier_override(kqueue_t kqu);

static void kqworkloop_unbind(struct kqworkloop *kwql);

enum kqwl_unbind_locked_mode {
	KQWL_OVERRIDE_DROP_IMMEDIATELY,
	KQWL_OVERRIDE_DROP_DELAYED,
};
static void kqworkloop_unbind_locked(struct kqworkloop *kwql, thread_t thread,
    enum kqwl_unbind_locked_mode how);
static void kqworkloop_unbind_delayed_override_drop(thread_t thread);
static kq_index_t kqworkloop_override(struct kqworkloop *kqwl);
static void kqworkloop_set_overcommit(struct kqworkloop *kqwl);
enum {
	KQWL_UTQ_NONE,
	/*
	 * The wakeup qos is the qos of QUEUED knotes.
	 *
	 * This QoS is accounted for with the events override in the
	 * kqr_override_index field. It is raised each time a new knote is queued at
	 * a given QoS. The kqwl_wakeup_qos field is a superset of the non empty
	 * knote buckets and is recomputed after each event delivery.
	 */
	KQWL_UTQ_UPDATE_WAKEUP_QOS,
	KQWL_UTQ_RECOMPUTE_WAKEUP_QOS,
	KQWL_UTQ_UNBINDING, /* attempt to rebind */
	KQWL_UTQ_PARKING,
	/*
	 * The wakeup override is for suppressed knotes that have fired again at
	 * a higher QoS than the one for which they are suppressed already.
	 * This override is cleared when the knote suppressed list becomes empty.
	 */
	KQWL_UTQ_UPDATE_WAKEUP_OVERRIDE,
	KQWL_UTQ_RESET_WAKEUP_OVERRIDE,
	/*
	 * The QoS is the maximum QoS of an event enqueued on this workloop in
	 * userland. It is copied from the only EVFILT_WORKLOOP knote with
	 * a NOTE_WL_THREAD_REQUEST bit set allowed on this workloop. If there is no
	 * such knote, this QoS is 0.
	 */
	KQWL_UTQ_SET_QOS_INDEX,
	KQWL_UTQ_REDRIVE_EVENTS,
};
static void kqworkloop_update_threads_qos(struct kqworkloop *kqwl, int op, kq_index_t qos);
static int kqworkloop_end_processing(struct kqworkloop *kqwl, int flags, int kevent_flags);

static struct knote *knote_alloc(void);
static void knote_free(struct knote *kn);
static int kq_add_knote(struct kqueue *kq, struct knote *kn,
    struct knote_lock_ctx *knlc, struct proc *p);
static struct knote *kq_find_knote_and_kq_lock(struct kqueue *kq,
    struct kevent_qos_s *kev, bool is_fd, struct proc *p);

static void knote_activate(kqueue_t kqu, struct knote *kn, int result);
static void knote_dequeue(kqueue_t kqu, struct knote *kn);

static void knote_apply_touch(kqueue_t kqu, struct knote *kn,
    struct kevent_qos_s *kev, int result);
static void knote_suppress(kqueue_t kqu, struct knote *kn);
static void knote_unsuppress(kqueue_t kqu, struct knote *kn);
static void knote_drop(kqueue_t kqu, struct knote *kn, struct knote_lock_ctx *knlc);

// both these functions may dequeue the knote and it is up to the caller
// to enqueue the knote back
static void knote_adjust_qos(struct kqueue *kq, struct knote *kn, int result);
static void knote_reset_priority(kqueue_t kqu, struct knote *kn, pthread_priority_t pp);

static ZONE_DEFINE(knote_zone, "knote zone",
    sizeof(struct knote), ZC_CACHING | ZC_ZFREE_CLEARMEM);
static ZONE_DEFINE(kqfile_zone, "kqueue file zone",
    sizeof(struct kqfile), ZC_ZFREE_CLEARMEM | ZC_NOTBITAG);
static ZONE_DEFINE(kqworkq_zone, "kqueue workq zone",
    sizeof(struct kqworkq), ZC_ZFREE_CLEARMEM | ZC_NOTBITAG);
static ZONE_DEFINE(kqworkloop_zone, "kqueue workloop zone",
    sizeof(struct kqworkloop), ZC_CACHING | ZC_ZFREE_CLEARMEM | ZC_NOTBITAG);

#define KN_HASH(val, mask)      (((val) ^ (val >> 8)) & (mask))

static int filt_no_attach(struct knote *kn, struct kevent_qos_s *kev);
static void filt_no_detach(struct knote *kn);
static int filt_bad_event(struct knote *kn, long hint);
static int filt_bad_touch(struct knote *kn, struct kevent_qos_s *kev);
static int filt_bad_process(struct knote *kn, struct kevent_qos_s *kev);

SECURITY_READ_ONLY_EARLY(static struct filterops) bad_filtops = {
	.f_attach  = filt_no_attach,
	.f_detach  = filt_no_detach,
	.f_event   = filt_bad_event,
	.f_touch   = filt_bad_touch,
	.f_process = filt_bad_process,
};

#if CONFIG_MEMORYSTATUS
extern const struct filterops memorystatus_filtops;
#endif /* CONFIG_MEMORYSTATUS */
extern const struct filterops fs_filtops;
extern const struct filterops sig_filtops;
extern const struct filterops machport_attach_filtops;
extern const struct filterops mach_port_filtops;
extern const struct filterops mach_port_set_filtops;
extern const struct filterops pipe_nfiltops;
extern const struct filterops pipe_rfiltops;
extern const struct filterops pipe_wfiltops;
extern const struct filterops ptsd_kqops;
extern const struct filterops ptmx_kqops;
extern const struct filterops soread_filtops;
extern const struct filterops sowrite_filtops;
extern const struct filterops sock_filtops;
extern const struct filterops soexcept_filtops;
extern const struct filterops spec_filtops;
extern const struct filterops bpfread_filtops;
extern const struct filterops necp_fd_rfiltops;
#if SKYWALK
extern const struct filterops skywalk_channel_rfiltops;
extern const struct filterops skywalk_channel_wfiltops;
extern const struct filterops skywalk_channel_efiltops;
#endif /* SKYWALK */
extern const struct filterops fsevent_filtops;
extern const struct filterops vnode_filtops;
extern const struct filterops tty_filtops;

const static struct filterops file_filtops;
const static struct filterops kqread_filtops;
const static struct filterops proc_filtops;
const static struct filterops timer_filtops;
const static struct filterops user_filtops;
const static struct filterops workloop_filtops;
#if CONFIG_EXCLAVES
extern const struct filterops exclaves_notification_filtops;
#endif /* CONFIG_EXCLAVES */

/*
 *
 * Rules for adding new filters to the system:
 * Public filters:
 * - Add a new "EVFILT_" option value to bsd/sys/event.h (typically a negative value)
 *   in the exported section of the header
 * - Update the EVFILT_SYSCOUNT value to reflect the new addition
 * - Add a filterops to the sysfilt_ops array. Public filters should be added at the end
 *   of the Public Filters section in the array.
 * Private filters:
 * - Add a new "EVFILT_" value to bsd/sys/event_private.h (typically a positive value)
 * - Update the EVFILTID_MAX value to reflect the new addition
 * - Add a filterops to the sysfilt_ops. Private filters should be added at the end of
 *   the Private filters section of the array.
 */
static_assert(EVFILTID_MAX < UINT8_MAX, "kn_filtid expects this to be true");
static const struct filterops * const sysfilt_ops[EVFILTID_MAX] = {
	/* Public Filters */
	[~EVFILT_READ]                  = &file_filtops,
	[~EVFILT_WRITE]                 = &file_filtops,
	[~EVFILT_AIO]                   = &bad_filtops,
	[~EVFILT_VNODE]                 = &file_filtops,
	[~EVFILT_PROC]                  = &proc_filtops,
	[~EVFILT_SIGNAL]                = &sig_filtops,
	[~EVFILT_TIMER]                 = &timer_filtops,
	[~EVFILT_MACHPORT]              = &machport_attach_filtops,
	[~EVFILT_FS]                    = &fs_filtops,
	[~EVFILT_USER]                  = &user_filtops,
	[~EVFILT_UNUSED_11]             = &bad_filtops,
	[~EVFILT_VM]                    = &bad_filtops,
	[~EVFILT_SOCK]                  = &file_filtops,
#if CONFIG_MEMORYSTATUS
	[~EVFILT_MEMORYSTATUS]          = &memorystatus_filtops,
#else
	[~EVFILT_MEMORYSTATUS]          = &bad_filtops,
#endif
	[~EVFILT_EXCEPT]                = &file_filtops,
#if SKYWALK
	[~EVFILT_NW_CHANNEL]            = &file_filtops,
#else /* !SKYWALK */
	[~EVFILT_NW_CHANNEL]            = &bad_filtops,
#endif /* !SKYWALK */
	[~EVFILT_WORKLOOP]              = &workloop_filtops,
#if CONFIG_EXCLAVES
	[~EVFILT_EXCLAVES_NOTIFICATION] = &exclaves_notification_filtops,
#else /* !CONFIG_EXCLAVES */
	[~EVFILT_EXCLAVES_NOTIFICATION] = &bad_filtops,
#endif /* CONFIG_EXCLAVES*/

	/* Private filters */
	[EVFILTID_KQREAD]               = &kqread_filtops,
	[EVFILTID_PIPE_N]               = &pipe_nfiltops,
	[EVFILTID_PIPE_R]               = &pipe_rfiltops,
	[EVFILTID_PIPE_W]               = &pipe_wfiltops,
	[EVFILTID_PTSD]                 = &ptsd_kqops,
	[EVFILTID_SOREAD]               = &soread_filtops,
	[EVFILTID_SOWRITE]              = &sowrite_filtops,
	[EVFILTID_SCK]                  = &sock_filtops,
	[EVFILTID_SOEXCEPT]             = &soexcept_filtops,
	[EVFILTID_SPEC]                 = &spec_filtops,
	[EVFILTID_BPFREAD]              = &bpfread_filtops,
	[EVFILTID_NECP_FD]              = &necp_fd_rfiltops,
#if SKYWALK
	[EVFILTID_SKYWALK_CHANNEL_W]    = &skywalk_channel_wfiltops,
	[EVFILTID_SKYWALK_CHANNEL_R]    = &skywalk_channel_rfiltops,
	[EVFILTID_SKYWALK_CHANNEL_E]    = &skywalk_channel_efiltops,
#else /* !SKYWALK */
	[EVFILTID_SKYWALK_CHANNEL_W]    = &bad_filtops,
	[EVFILTID_SKYWALK_CHANNEL_R]    = &bad_filtops,
	[EVFILTID_SKYWALK_CHANNEL_E]    = &bad_filtops,
#endif /* !SKYWALK */
	[EVFILTID_FSEVENT]              = &fsevent_filtops,
	[EVFILTID_VN]                   = &vnode_filtops,
	[EVFILTID_TTY]                  = &tty_filtops,
	[EVFILTID_PTMX]                 = &ptmx_kqops,
	[EVFILTID_MACH_PORT]            = &mach_port_filtops,
	[EVFILTID_MACH_PORT_SET]        = &mach_port_set_filtops,

	/* fake filter for detached knotes, keep last */
	[EVFILTID_DETACHED]             = &bad_filtops,
};

static inline bool
kqr_thread_bound(workq_threadreq_t kqr)
{
	return kqr->tr_state == WORKQ_TR_STATE_BOUND;
}

static inline bool
kqr_thread_requested_pending(workq_threadreq_t kqr)
{
	workq_tr_state_t tr_state = kqr->tr_state;
	return tr_state > WORKQ_TR_STATE_IDLE && tr_state < WORKQ_TR_STATE_BOUND;
}

static inline bool
kqr_thread_requested(workq_threadreq_t kqr)
{
	return kqr->tr_state != WORKQ_TR_STATE_IDLE;
}

static inline thread_t
kqr_thread_fast(workq_threadreq_t kqr)
{
	assert(kqr_thread_bound(kqr));
	return kqr->tr_thread;
}

static inline thread_t
kqr_thread(workq_threadreq_t kqr)
{
	return kqr_thread_bound(kqr) ? kqr->tr_thread : THREAD_NULL;
}

static inline struct kqworkloop *
kqr_kqworkloop(workq_threadreq_t kqr)
{
	if (kqr->tr_flags & WORKQ_TR_FLAG_WORKLOOP) {
		return __container_of(kqr, struct kqworkloop, kqwl_request);
	}
	return NULL;
}

static inline kqueue_t
kqr_kqueue(proc_t p, workq_threadreq_t kqr)
{
	kqueue_t kqu;
	if (kqr->tr_flags & WORKQ_TR_FLAG_WORKLOOP) {
		kqu.kqwl = kqr_kqworkloop(kqr);
	} else {
		kqu.kqwq = p->p_fd.fd_wqkqueue;
		assert(kqr >= kqu.kqwq->kqwq_request &&
		    kqr < kqu.kqwq->kqwq_request + KQWQ_NBUCKETS);
	}
	return kqu;
}

#if CONFIG_PREADOPT_TG
/* There are no guarantees about which locks are held when this is called */
inline thread_group_qos_t
kqr_preadopt_thread_group(workq_threadreq_t req)
{
	struct kqworkloop *kqwl = kqr_kqworkloop(req);
	return kqwl ? os_atomic_load(&kqwl->kqwl_preadopt_tg, relaxed) : NULL;
}

/* There are no guarantees about which locks are held when this is called */
inline _Atomic(thread_group_qos_t) *
kqr_preadopt_thread_group_addr(workq_threadreq_t req)
{
	struct kqworkloop *kqwl = kqr_kqworkloop(req);
	return kqwl ? (&kqwl->kqwl_preadopt_tg) : NULL;
}
#endif

/*
 * kqueue/note lock implementations
 *
 *	The kqueue lock guards the kq state, the state of its queues,
 *	and the kqueue-aware status and locks of individual knotes.
 *
 *	The kqueue workq lock is used to protect state guarding the
 *	interaction of the kqueue with the workq.  This state cannot
 *	be guarded by the kq lock - as it needs to be taken when we
 *	already have the waitq set lock held (during the waitq hook
 *	callback).  It might be better to use the waitq lock itself
 *	for this, but the IRQ requirements make that difficult).
 *
 *	Knote flags, filter flags, and associated data are protected
 *	by the underlying object lock - and are only ever looked at
 *	by calling the filter to get a [consistent] snapshot of that
 *	data.
 */

static inline void
kqlock(kqueue_t kqu)
{
	lck_spin_lock(&kqu.kq->kq_lock);
}

static inline void
kqlock_held(__assert_only kqueue_t kqu)
{
	LCK_SPIN_ASSERT(&kqu.kq->kq_lock, LCK_ASSERT_OWNED);
}

static inline void
kqunlock(kqueue_t kqu)
{
	lck_spin_unlock(&kqu.kq->kq_lock);
}

static inline void
knhash_lock(struct filedesc *fdp)
{
	lck_mtx_lock(&fdp->fd_knhashlock);
}

static inline void
knhash_unlock(struct filedesc *fdp)
{
	lck_mtx_unlock(&fdp->fd_knhashlock);
}

/* wait event for knote locks */
static inline event_t
knote_lock_wev(struct knote *kn)
{
	return (event_t)(&kn->kn_hook);
}

/* wait event for kevent_register_wait_* */
static inline event64_t
knote_filt_wev64(struct knote *kn)
{
	/* kdp_workloop_sync_wait_find_owner knows about this */
	return CAST_EVENT64_T(kn);
}

/* wait event for knote_post/knote_drop */
static inline event_t
knote_post_wev(struct knote *kn)
{
	return &kn->kn_kevent;
}

/*!
 * @function knote_has_qos
 *
 * @brief
 * Whether the knote has a regular QoS.
 *
 * @discussion
 * kn_qos_override is:
 * - 0 on kqfiles
 * - THREAD_QOS_LAST for special buckets (manager)
 *
 * Other values mean the knote participates to QoS propagation.
 */
static inline bool
knote_has_qos(struct knote *kn)
{
	return kn->kn_qos_override > 0 && kn->kn_qos_override < THREAD_QOS_LAST;
}

#pragma mark knote locks

/*
 * Enum used by the knote_lock_* functions.
 *
 * KNOTE_KQ_LOCK_ALWAYS
 *   The function will always return with the kq lock held.
 *
 * KNOTE_KQ_LOCK_ON_SUCCESS
 *   The function will return with the kq lock held if it was successful
 *   (knote_lock() is the only function that can fail).
 *
 * KNOTE_KQ_LOCK_ON_FAILURE
 *   The function will return with the kq lock held if it was unsuccessful
 *   (knote_lock() is the only function that can fail).
 *
 * KNOTE_KQ_UNLOCK:
 *   The function returns with the kq unlocked.
 */
enum kqlocking {
	KNOTE_KQ_LOCK_ALWAYS,
	KNOTE_KQ_LOCK_ON_SUCCESS,
	KNOTE_KQ_LOCK_ON_FAILURE,
	KNOTE_KQ_UNLOCK,
};

static struct knote_lock_ctx *
knote_lock_ctx_find(kqueue_t kqu, struct knote *kn)
{
	struct knote_lock_ctx *ctx;
	LIST_FOREACH(ctx, &kqu.kq->kq_knlocks, knlc_link) {
		if (ctx->knlc_knote == kn) {
			return ctx;
		}
	}
	panic("knote lock context not found: %p", kn);
	__builtin_trap();
}

/* slowpath of knote_lock() */
__attribute__((noinline))
static bool __result_use_check
knote_lock_slow(kqueue_t kqu, struct knote *kn,
    struct knote_lock_ctx *knlc, int kqlocking)
{
	struct knote_lock_ctx *owner_lc;
	struct uthread *uth = current_uthread();
	wait_result_t wr;

	kqlock_held(kqu);

	owner_lc = knote_lock_ctx_find(kqu, kn);
#if DEBUG || DEVELOPMENT
	knlc->knlc_state = KNOTE_LOCK_CTX_WAITING;
#endif
	owner_lc->knlc_waiters++;

	/*
	 * Make our lock context visible to knote_unlock()
	 */
	uth->uu_knlock = knlc;

	wr = lck_spin_sleep_with_inheritor(&kqu.kq->kq_lock, LCK_SLEEP_UNLOCK,
	    knote_lock_wev(kn), owner_lc->knlc_thread,
	    THREAD_UNINT | THREAD_WAIT_NOREPORT, TIMEOUT_WAIT_FOREVER);

	if (wr == THREAD_RESTART) {
		/*
		 * We haven't been woken up by knote_unlock() but knote_unlock_cancel.
		 * We need to cleanup the state since no one did.
		 */
		uth->uu_knlock = NULL;
#if DEBUG || DEVELOPMENT
		assert(knlc->knlc_state == KNOTE_LOCK_CTX_WAITING);
		knlc->knlc_state = KNOTE_LOCK_CTX_UNLOCKED;
#endif

		if (kqlocking == KNOTE_KQ_LOCK_ALWAYS ||
		    kqlocking == KNOTE_KQ_LOCK_ON_FAILURE) {
			kqlock(kqu);
		}
		return false;
	} else {
		if (kqlocking == KNOTE_KQ_LOCK_ALWAYS ||
		    kqlocking == KNOTE_KQ_LOCK_ON_SUCCESS) {
			kqlock(kqu);
#if DEBUG || DEVELOPMENT
			/*
			 * This state is set under the lock so we can't
			 * really assert this unless we hold the lock.
			 */
			assert(knlc->knlc_state == KNOTE_LOCK_CTX_LOCKED);
#endif
		}
		return true;
	}
}

/*
 * Attempts to take the "knote" lock.
 *
 * Called with the kqueue lock held.
 *
 * Returns true if the knote lock is acquired, false if it has been dropped
 */
static bool __result_use_check
knote_lock(kqueue_t kqu, struct knote *kn, struct knote_lock_ctx *knlc,
    enum kqlocking kqlocking)
{
	kqlock_held(kqu);

#if DEBUG || DEVELOPMENT
	assert(knlc->knlc_state == KNOTE_LOCK_CTX_UNLOCKED);
#endif
	knlc->knlc_knote = kn;
	knlc->knlc_thread = current_thread();
	knlc->knlc_waiters = 0;

	if (__improbable(kn->kn_status & KN_LOCKED)) {
		return knote_lock_slow(kqu, kn, knlc, kqlocking);
	}

	/*
	 * When the knote will be dropped, the knote lock is taken before
	 * KN_DROPPING is set, and then the knote will be removed from any
	 * hash table that references it before the lock is canceled.
	 */
	assert((kn->kn_status & KN_DROPPING) == 0);
	LIST_INSERT_HEAD(&kqu.kq->kq_knlocks, knlc, knlc_link);
	kn->kn_status |= KN_LOCKED;
#if DEBUG || DEVELOPMENT
	knlc->knlc_state = KNOTE_LOCK_CTX_LOCKED;
#endif

	if (kqlocking == KNOTE_KQ_UNLOCK ||
	    kqlocking == KNOTE_KQ_LOCK_ON_FAILURE) {
		kqunlock(kqu);
	}
	return true;
}

/*
 * Unlocks a knote successfully locked with knote_lock().
 *
 * Called with the kqueue lock held.
 *
 * Returns with the kqueue lock held according to KNOTE_KQ_* mode.
 */
static void
knote_unlock(kqueue_t kqu, struct knote *kn,
    struct knote_lock_ctx *knlc, enum kqlocking kqlocking)
{
	kqlock_held(kqu);

	assert(knlc->knlc_knote == kn);
	assert(kn->kn_status & KN_LOCKED);
#if DEBUG || DEVELOPMENT
	assert(knlc->knlc_state == KNOTE_LOCK_CTX_LOCKED);
#endif

	LIST_REMOVE(knlc, knlc_link);

	if (knlc->knlc_waiters) {
		thread_t thread = THREAD_NULL;

		wakeup_one_with_inheritor(knote_lock_wev(kn), THREAD_AWAKENED,
		    LCK_WAKE_DEFAULT, &thread);

		/*
		 * knote_lock_slow() publishes the lock context of waiters
		 * in uthread::uu_knlock.
		 *
		 * Reach out and make this context the new owner.
		 */
		struct uthread *ut = get_bsdthread_info(thread);
		struct knote_lock_ctx *next_owner_lc = ut->uu_knlock;

		assert(next_owner_lc->knlc_knote == kn);
		next_owner_lc->knlc_waiters = knlc->knlc_waiters - 1;
		LIST_INSERT_HEAD(&kqu.kq->kq_knlocks, next_owner_lc, knlc_link);
#if DEBUG || DEVELOPMENT
		next_owner_lc->knlc_state = KNOTE_LOCK_CTX_LOCKED;
#endif
		ut->uu_knlock = NULL;
		thread_deallocate_safe(thread);
	} else {
		kn->kn_status &= ~KN_LOCKED;
	}

	if ((kn->kn_status & KN_MERGE_QOS) && !(kn->kn_status & KN_POSTING)) {
		/*
		 * No f_event() in flight anymore, we can leave QoS "Merge" mode
		 *
		 * See knote_adjust_qos()
		 */
		kn->kn_status &= ~KN_MERGE_QOS;
	}
	if (kqlocking == KNOTE_KQ_UNLOCK) {
		kqunlock(kqu);
	}
#if DEBUG || DEVELOPMENT
	knlc->knlc_state = KNOTE_LOCK_CTX_UNLOCKED;
#endif
}

/*
 * Aborts all waiters for a knote lock, and unlock the knote.
 *
 * Called with the kqueue lock held.
 *
 * Returns with the kqueue unlocked.
 */
static void
knote_unlock_cancel(struct kqueue *kq, struct knote *kn,
    struct knote_lock_ctx *knlc)
{
	kqlock_held(kq);

	assert(knlc->knlc_knote == kn);
	assert(kn->kn_status & KN_LOCKED);
	assert(kn->kn_status & KN_DROPPING);

	LIST_REMOVE(knlc, knlc_link);
	kn->kn_status &= ~KN_LOCKED;
	kqunlock(kq);

	if (knlc->knlc_waiters) {
		wakeup_all_with_inheritor(knote_lock_wev(kn), THREAD_RESTART);
	}
#if DEBUG || DEVELOPMENT
	knlc->knlc_state = KNOTE_LOCK_CTX_UNLOCKED;
#endif
}

/*
 * Call the f_event hook of a given filter.
 *
 * Takes a use count to protect against concurrent drops.
 * Called with the object lock held.
 */
static void
knote_post(struct knote *kn, long hint)
{
	struct kqueue *kq = knote_get_kq(kn);
	int dropping, result;

	kqlock(kq);

	if (__improbable(kn->kn_status & (KN_DROPPING | KN_VANISHED))) {
		return kqunlock(kq);
	}

	if (__improbable(kn->kn_status & KN_POSTING)) {
		panic("KNOTE() called concurrently on knote %p", kn);
	}

	kn->kn_status |= KN_POSTING;

	kqunlock(kq);
	result = filter_call(knote_fops(kn), f_event(kn, hint));
	kqlock(kq);

	/* Someone dropped the knote/the monitored object vanished while we
	 * were in f_event, swallow the side effects of the post.
	 */
	dropping = (kn->kn_status & (KN_DROPPING | KN_VANISHED));

	if (!dropping && (result & FILTER_ADJUST_EVENT_IOTIER_BIT)) {
		kqueue_update_iotier_override(kq);
	}

	if (!dropping && (result & FILTER_ACTIVE)) {
		knote_activate(kq, kn, result);
	}

	if ((kn->kn_status & KN_LOCKED) == 0) {
		/*
		 * There's no other f_* call in flight, we can leave QoS "Merge" mode.
		 *
		 * See knote_adjust_qos()
		 */
		kn->kn_status &= ~(KN_POSTING | KN_MERGE_QOS);
	} else {
		kn->kn_status &= ~KN_POSTING;
	}

	if (__improbable(dropping)) {
		thread_wakeup(knote_post_wev(kn));
	}

	kqunlock(kq);
}

/*
 * Called by knote_drop() and knote_fdclose() to wait for the last f_event()
 * caller to be done.
 *
 *	- kq locked at entry
 *	- kq unlocked at exit
 */
static void
knote_wait_for_post(struct kqueue *kq, struct knote *kn)
{
	kqlock_held(kq);

	assert(kn->kn_status & (KN_DROPPING | KN_VANISHED));

	if (kn->kn_status & KN_POSTING) {
		lck_spin_sleep(&kq->kq_lock, LCK_SLEEP_UNLOCK, knote_post_wev(kn),
		    THREAD_UNINT | THREAD_WAIT_NOREPORT);
	} else {
		kqunlock(kq);
	}
}

#pragma mark knote helpers for filters

OS_ALWAYS_INLINE
void *
knote_kn_hook_get_raw(struct knote *kn)
{
	uintptr_t *addr = &kn->kn_hook;

	void *hook = (void *) *addr;
#if __has_feature(ptrauth_calls)
	if (hook) {
		uint16_t blend = kn->kn_filter;
		blend |= (kn->kn_filtid << 8);
		blend ^= OS_PTRAUTH_DISCRIMINATOR("kn.kn_hook");

		hook = ptrauth_auth_data(hook, ptrauth_key_process_independent_data,
		    ptrauth_blend_discriminator(addr, blend));
	}
#endif

	return hook;
}

OS_ALWAYS_INLINE void
knote_kn_hook_set_raw(struct knote *kn, void *kn_hook)
{
	uintptr_t *addr = &kn->kn_hook;
#if __has_feature(ptrauth_calls)
	if (kn_hook) {
		uint16_t blend = kn->kn_filter;
		blend |= (kn->kn_filtid << 8);
		blend ^= OS_PTRAUTH_DISCRIMINATOR("kn.kn_hook");

		kn_hook = ptrauth_sign_unauthenticated(kn_hook,
		    ptrauth_key_process_independent_data,
		    ptrauth_blend_discriminator(addr, blend));
	}
#endif
	*addr = (uintptr_t) kn_hook;
}

OS_ALWAYS_INLINE
void
knote_set_error(struct knote *kn, int error)
{
	kn->kn_flags |= EV_ERROR;
	kn->kn_sdata = error;
}

OS_ALWAYS_INLINE
int64_t
knote_low_watermark(const struct knote *kn)
{
	return (kn->kn_sfflags & NOTE_LOWAT) ? kn->kn_sdata : 1;
}

/*!
 * @function knote_fill_kevent_with_sdata
 *
 * @brief
 * Fills in a kevent from the current content of a knote.
 *
 * @discussion
 * This is meant to be called from filter's f_process hooks.
 * The kevent data is filled with kn->kn_sdata.
 *
 * kn->kn_fflags is cleared if kn->kn_flags has EV_CLEAR set.
 *
 * Using knote_fill_kevent is typically preferred.
 */
OS_ALWAYS_INLINE
void
knote_fill_kevent_with_sdata(struct knote *kn, struct kevent_qos_s *kev)
{
#define knote_assert_aliases(name1, offs1, name2) \
	static_assert(offsetof(struct kevent_qos_s, name1) + offs1 == \
	    offsetof(struct kevent_internal_s, name2), \
	        "kevent_qos_s::" #name1 " and kevent_internal_s::" #name2 "need to alias")
	/*
	 * All the code makes assumptions on these aliasing,
	 * so make sure we fail the build if we ever ever ever break them.
	 */
	knote_assert_aliases(ident, 0, kei_ident);
#ifdef __LITTLE_ENDIAN__
	knote_assert_aliases(filter, 0, kei_filter);  // non trivial overlap
	knote_assert_aliases(filter, 1, kei_filtid);  // non trivial overlap
#else
	knote_assert_aliases(filter, 0, kei_filtid);  // non trivial overlap
	knote_assert_aliases(filter, 1, kei_filter);  // non trivial overlap
#endif
	knote_assert_aliases(flags, 0, kei_flags);
	knote_assert_aliases(qos, 0, kei_qos);
	knote_assert_aliases(udata, 0, kei_udata);
	knote_assert_aliases(fflags, 0, kei_fflags);
	knote_assert_aliases(xflags, 0, kei_sfflags); // non trivial overlap
	knote_assert_aliases(data, 0, kei_sdata);     // non trivial overlap
	knote_assert_aliases(ext, 0, kei_ext);
#undef knote_assert_aliases

	/*
	 * Fix the differences between kevent_qos_s and kevent_internal_s:
	 * - xflags is where kn_sfflags lives, we need to zero it
	 * - fixup the high bits of `filter` where kn_filtid lives
	 */
	*kev = *(struct kevent_qos_s *)&kn->kn_kevent;
	kev->xflags = 0;
	kev->filter |= 0xff00;
	if (kn->kn_flags & EV_CLEAR) {
		kn->kn_fflags = 0;
	}
}

/*!
 * @function knote_fill_kevent
 *
 * @brief
 * Fills in a kevent from the current content of a knote.
 *
 * @discussion
 * This is meant to be called from filter's f_process hooks.
 * The kevent data is filled with the passed in data.
 *
 * kn->kn_fflags is cleared if kn->kn_flags has EV_CLEAR set.
 */
OS_ALWAYS_INLINE
void
knote_fill_kevent(struct knote *kn, struct kevent_qos_s *kev, int64_t data)
{
	knote_fill_kevent_with_sdata(kn, kev);
	kev->filter = kn->kn_filter;
	kev->data = data;
}


#pragma mark file_filtops

static int
filt_fileattach(struct knote *kn, struct kevent_qos_s *kev)
{
	return fo_kqfilter(kn->kn_fp, kn, kev);
}

SECURITY_READ_ONLY_EARLY(static struct filterops) file_filtops = {
	.f_isfd = 1,
	.f_attach = filt_fileattach,
};

#pragma mark kqread_filtops

#define f_flag fp_glob->fg_flag
#define f_ops fp_glob->fg_ops
#define f_lflags fp_glob->fg_lflags

static void
filt_kqdetach(struct knote *kn)
{
	struct kqfile *kqf = (struct kqfile *)fp_get_data(kn->kn_fp);
	struct kqueue *kq = &kqf->kqf_kqueue;

	kqlock(kq);
	KNOTE_DETACH(&kqf->kqf_sel.si_note, kn);
	kqunlock(kq);
}

static int
filt_kqueue(struct knote *kn, __unused long hint)
{
	struct kqueue *kq = (struct kqueue *)fp_get_data(kn->kn_fp);

	return kq->kq_count > 0;
}

static int
filt_kqtouch(struct knote *kn, struct kevent_qos_s *kev)
{
#pragma unused(kev)
	struct kqueue *kq = (struct kqueue *)fp_get_data(kn->kn_fp);
	int res;

	kqlock(kq);
	res = (kq->kq_count > 0);
	kqunlock(kq);

	return res;
}

static int
filt_kqprocess(struct knote *kn, struct kevent_qos_s *kev)
{
	struct kqueue *kq = (struct kqueue *)fp_get_data(kn->kn_fp);
	int res = 0;

	kqlock(kq);
	if (kq->kq_count) {
		knote_fill_kevent(kn, kev, kq->kq_count);
		res = 1;
	}
	kqunlock(kq);

	return res;
}

SECURITY_READ_ONLY_EARLY(static struct filterops) kqread_filtops = {
	.f_isfd = 1,
	.f_detach = filt_kqdetach,
	.f_event = filt_kqueue,
	.f_touch = filt_kqtouch,
	.f_process = filt_kqprocess,
};

#pragma mark proc_filtops

static int
filt_procattach(struct knote *kn, __unused struct kevent_qos_s *kev)
{
	struct proc *p;

	assert(PID_MAX < NOTE_PDATAMASK);

	if ((kn->kn_sfflags & (NOTE_TRACK | NOTE_TRACKERR | NOTE_CHILD)) != 0) {
		knote_set_error(kn, ENOTSUP);
		return 0;
	}

	p = proc_find((int)kn->kn_id);
	if (p == NULL) {
		knote_set_error(kn, ESRCH);
		return 0;
	}

	const uint32_t NoteExitStatusBits = NOTE_EXIT | NOTE_EXITSTATUS;

	if ((kn->kn_sfflags & NoteExitStatusBits) == NoteExitStatusBits) {
		do {
			pid_t selfpid = proc_selfpid();

			if (p->p_ppid == selfpid) {
				break;  /* parent => ok */
			}
			if ((p->p_lflag & P_LTRACED) != 0 &&
			    (p->p_oppid == selfpid)) {
				break;  /* parent-in-waiting => ok */
			}
			if (cansignal(current_proc(), kauth_cred_get(), p, SIGKILL)) {
				break; /* allowed to signal => ok */
			}
			proc_rele(p);
			knote_set_error(kn, EACCES);
			return 0;
		} while (0);
	}

	kn->kn_proc = p;
	kn->kn_flags |= EV_CLEAR;       /* automatically set */
	kn->kn_sdata = 0;               /* incoming data is ignored */

	proc_klist_lock();

	KNOTE_ATTACH(&p->p_klist, kn);

	proc_klist_unlock();

	proc_rele(p);

	/*
	 * only captures edge-triggered events after this point
	 * so it can't already be fired.
	 */
	return 0;
}


/*
 * The knote may be attached to a different process, which may exit,
 * leaving nothing for the knote to be attached to.  In that case,
 * the pointer to the process will have already been nulled out.
 */
static void
filt_procdetach(struct knote *kn)
{
	struct proc *p;

	proc_klist_lock();

	p = kn->kn_proc;
	if (p != PROC_NULL) {
		kn->kn_proc = PROC_NULL;
		KNOTE_DETACH(&p->p_klist, kn);
	}

	proc_klist_unlock();
}

static int
filt_procevent(struct knote *kn, long hint)
{
	u_int event;

	/* ALWAYS CALLED WITH proc_klist_lock */

	/*
	 * Note: a lot of bits in hint may be obtained from the knote
	 * To free some of those bits, see <rdar://problem/12592988> Freeing up
	 * bits in hint for filt_procevent
	 *
	 * mask off extra data
	 */
	event = (u_int)hint & NOTE_PCTRLMASK;

	/*
	 * termination lifecycle events can happen while a debugger
	 * has reparented a process, in which case notifications
	 * should be quashed except to the tracing parent. When
	 * the debugger reaps the child (either via wait4(2) or
	 * process exit), the child will be reparented to the original
	 * parent and these knotes re-fired.
	 */
	if (event & NOTE_EXIT) {
		if ((kn->kn_proc->p_oppid != 0)
		    && (proc_getpid(knote_get_kq(kn)->kq_p) != kn->kn_proc->p_ppid)) {
			/*
			 * This knote is not for the current ptrace(2) parent, ignore.
			 */
			return 0;
		}
	}

	/*
	 * if the user is interested in this event, record it.
	 */
	if (kn->kn_sfflags & event) {
		kn->kn_fflags |= event;
	}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	if ((event == NOTE_REAP) || ((event == NOTE_EXIT) && !(kn->kn_sfflags & NOTE_REAP))) {
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
	}
#pragma clang diagnostic pop


	/*
	 * The kernel has a wrapper in place that returns the same data
	 * as is collected here, in kn_hook32.  Any changes to how
	 * NOTE_EXITSTATUS and NOTE_EXIT_DETAIL are collected
	 * should also be reflected in the proc_pidnoteexit() wrapper.
	 */
	if (event == NOTE_EXIT) {
		kn->kn_hook32 = 0;
		if ((kn->kn_sfflags & NOTE_EXITSTATUS) != 0) {
			kn->kn_fflags |= NOTE_EXITSTATUS;
			kn->kn_hook32 |= (hint & NOTE_PDATAMASK);
		}
		if ((kn->kn_sfflags & NOTE_EXIT_DETAIL) != 0) {
			kn->kn_fflags |= NOTE_EXIT_DETAIL;
			if ((kn->kn_proc->p_lflag &
			    P_LTERM_DECRYPTFAIL) != 0) {
				kn->kn_hook32 |= NOTE_EXIT_DECRYPTFAIL;
			}
			if ((kn->kn_proc->p_lflag &
			    P_LTERM_JETSAM) != 0) {
				kn->kn_hook32 |= NOTE_EXIT_MEMORY;
				switch (kn->kn_proc->p_lflag & P_JETSAM_MASK) {
				case P_JETSAM_VMPAGESHORTAGE:
					kn->kn_hook32 |= NOTE_EXIT_MEMORY_VMPAGESHORTAGE;
					break;
				case P_JETSAM_VMTHRASHING:
					kn->kn_hook32 |= NOTE_EXIT_MEMORY_VMTHRASHING;
					break;
				case P_JETSAM_FCTHRASHING:
					kn->kn_hook32 |= NOTE_EXIT_MEMORY_FCTHRASHING;
					break;
				case P_JETSAM_VNODE:
					kn->kn_hook32 |= NOTE_EXIT_MEMORY_VNODE;
					break;
				case P_JETSAM_HIWAT:
					kn->kn_hook32 |= NOTE_EXIT_MEMORY_HIWAT;
					break;
				case P_JETSAM_PID:
					kn->kn_hook32 |= NOTE_EXIT_MEMORY_PID;
					break;
				case P_JETSAM_IDLEEXIT:
					kn->kn_hook32 |= NOTE_EXIT_MEMORY_IDLE;
					break;
				}
			}
			if ((proc_getcsflags(kn->kn_proc) &
			    CS_KILLED) != 0) {
				kn->kn_hook32 |= NOTE_EXIT_CSERROR;
			}
		}
	}

	/* if we have any matching state, activate the knote */
	return kn->kn_fflags != 0;
}

static int
filt_proctouch(struct knote *kn, struct kevent_qos_s *kev)
{
	int res;

	proc_klist_lock();

	/* accept new filter flags and mask off output events no long interesting */
	kn->kn_sfflags = kev->fflags;

	/* restrict the current results to the (smaller?) set of new interest */
	/*
	 * For compatibility with previous implementations, we leave kn_fflags
	 * as they were before.
	 */
	//kn->kn_fflags &= kn->kn_sfflags;

	res = (kn->kn_fflags != 0);

	proc_klist_unlock();

	return res;
}

static int
filt_procprocess(struct knote *kn, struct kevent_qos_s *kev)
{
	int res = 0;

	proc_klist_lock();
	if (kn->kn_fflags) {
		knote_fill_kevent(kn, kev, kn->kn_hook32);
		kn->kn_hook32 = 0;
		res = 1;
	}
	proc_klist_unlock();
	return res;
}

SECURITY_READ_ONLY_EARLY(static struct filterops) proc_filtops = {
	.f_attach  = filt_procattach,
	.f_detach  = filt_procdetach,
	.f_event   = filt_procevent,
	.f_touch   = filt_proctouch,
	.f_process = filt_procprocess,
};

#pragma mark timer_filtops

struct filt_timer_params {
	uint64_t deadline; /* deadline in abs/cont time
	                    *                      (or 0 if NOTE_ABSOLUTE and deadline is in past) */
	uint64_t leeway;   /* leeway in abstime, or 0 if none */
	uint64_t interval; /* interval in abstime or 0 if non-repeating timer */
};

/*
 * Values stored in the knote at rest (using Mach absolute time units)
 *
 * kn->kn_thcall        where the thread_call object is stored
 * kn->kn_ext[0]        next deadline or 0 if immediate expiration
 * kn->kn_ext[1]        leeway value
 * kn->kn_sdata         interval timer: the interval
 *                      absolute/deadline timer: 0
 * kn->kn_hook32        timer state (with gencount)
 *
 * TIMER_IDLE:
 *   The timer has either never been scheduled or been cancelled.
 *   It is safe to schedule a new one in this state.
 *
 * TIMER_ARMED:
 *   The timer has been scheduled
 *
 * TIMER_FIRED
 *   The timer has fired and an event needs to be delivered.
 *   When in this state, the callout may still be running.
 *
 * TIMER_IMMEDIATE
 *   The timer has fired at registration time, and the callout was never
 *   dispatched.
 */
#define TIMER_IDLE       0x0
#define TIMER_ARMED      0x1
#define TIMER_FIRED      0x2
#define TIMER_IMMEDIATE  0x3
#define TIMER_STATE_MASK 0x3
#define TIMER_GEN_INC    0x4

static void
filt_timer_set_params(struct knote *kn, struct filt_timer_params *params)
{
	kn->kn_ext[0] = params->deadline;
	kn->kn_ext[1] = params->leeway;
	kn->kn_sdata  = params->interval;
}

/*
 * filt_timervalidate - process data from user
 *
 * Sets up the deadline, interval, and leeway from the provided user data
 *
 * Input:
 *      kn_sdata        timer deadline or interval time
 *      kn_sfflags      style of timer, unit of measurement
 *
 * Output:
 *      struct filter_timer_params to apply to the filter with
 *      filt_timer_set_params when changes are ready to be commited.
 *
 * Returns:
 *      EINVAL          Invalid user data parameters
 *      ERANGE          Various overflows with the parameters
 *
 * Called with timer filter lock held.
 */
static int
filt_timervalidate(const struct kevent_qos_s *kev,
    struct filt_timer_params *params)
{
	/*
	 * There are 5 knobs that need to be chosen for a timer registration:
	 *
	 * A) Units of time (what is the time duration of the specified number)
	 *      Absolute and interval take:
	 *              NOTE_SECONDS, NOTE_USECONDS, NOTE_NSECONDS, NOTE_MACHTIME
	 *      Defaults to milliseconds if not specified
	 *
	 * B) Clock epoch (what is the zero point of the specified number)
	 *      For interval, there is none
	 *      For absolute, defaults to the gettimeofday/calendar epoch
	 *      With NOTE_MACHTIME, uses mach_absolute_time()
	 *      With NOTE_MACHTIME and NOTE_MACH_CONTINUOUS_TIME, uses mach_continuous_time()
	 *
	 * C) The knote's behavior on delivery
	 *      Interval timer causes the knote to arm for the next interval unless one-shot is set
	 *      Absolute is a forced one-shot timer which deletes on delivery
	 *      TODO: Add a way for absolute to be not forced one-shot
	 *
	 * D) Whether the time duration is relative to now or absolute
	 *      Interval fires at now + duration when it is set up
	 *      Absolute fires at now + difference between now walltime and passed in walltime
	 *      With NOTE_MACHTIME it fires at an absolute MAT or MCT.
	 *
	 * E) Whether the timer continues to tick across sleep
	 *      By default all three do not.
	 *      For interval and absolute, NOTE_MACH_CONTINUOUS_TIME causes them to tick across sleep
	 *      With NOTE_ABSOLUTE | NOTE_MACHTIME | NOTE_MACH_CONTINUOUS_TIME:
	 *              expires when mach_continuous_time() is > the passed in value.
	 */

	uint64_t multiplier;

	boolean_t use_abstime = FALSE;

	switch (kev->fflags & (NOTE_SECONDS | NOTE_USECONDS | NOTE_NSECONDS | NOTE_MACHTIME)) {
	case NOTE_SECONDS:
		multiplier = NSEC_PER_SEC;
		break;
	case NOTE_USECONDS:
		multiplier = NSEC_PER_USEC;
		break;
	case NOTE_NSECONDS:
		multiplier = 1;
		break;
	case NOTE_MACHTIME:
		multiplier = 0;
		use_abstime = TRUE;
		break;
	case 0: /* milliseconds (default) */
		multiplier = NSEC_PER_SEC / 1000;
		break;
	default:
		return EINVAL;
	}

	/* transform the leeway in kn_ext[1] to same time scale */
	if (kev->fflags & NOTE_LEEWAY) {
		uint64_t leeway_abs;

		if (use_abstime) {
			leeway_abs = (uint64_t)kev->ext[1];
		} else {
			uint64_t leeway_ns;
			if (os_mul_overflow((uint64_t)kev->ext[1], multiplier, &leeway_ns)) {
				return ERANGE;
			}

			nanoseconds_to_absolutetime(leeway_ns, &leeway_abs);
		}

		params->leeway = leeway_abs;
	} else {
		params->leeway = 0;
	}

	if (kev->fflags & NOTE_ABSOLUTE) {
		uint64_t deadline_abs;

		if (use_abstime) {
			deadline_abs = (uint64_t)kev->data;
		} else {
			uint64_t calendar_deadline_ns;

			if (os_mul_overflow((uint64_t)kev->data, multiplier, &calendar_deadline_ns)) {
				return ERANGE;
			}

			/* calendar_deadline_ns is in nanoseconds since the epoch */

			clock_sec_t seconds;
			clock_nsec_t nanoseconds;

			/*
			 * Note that the conversion through wall-time is only done once.
			 *
			 * If the relationship between MAT and gettimeofday changes,
			 * the underlying timer does not update.
			 *
			 * TODO: build a wall-time denominated timer_call queue
			 * and a flag to request DTRTing with wall-time timers
			 */
			clock_get_calendar_nanotime(&seconds, &nanoseconds);

			uint64_t calendar_now_ns = (uint64_t)seconds * NSEC_PER_SEC + nanoseconds;

			/* if deadline is in the future */
			if (calendar_now_ns < calendar_deadline_ns) {
				uint64_t interval_ns = calendar_deadline_ns - calendar_now_ns;
				uint64_t interval_abs;

				nanoseconds_to_absolutetime(interval_ns, &interval_abs);

				/*
				 * Note that the NOTE_MACH_CONTINUOUS_TIME flag here only
				 * causes the timer to keep ticking across sleep, but
				 * it does not change the calendar timebase.
				 */

				if (kev->fflags & NOTE_MACH_CONTINUOUS_TIME) {
					clock_continuoustime_interval_to_deadline(interval_abs,
					    &deadline_abs);
				} else {
					clock_absolutetime_interval_to_deadline(interval_abs,
					    &deadline_abs);
				}
			} else {
				deadline_abs = 0; /* cause immediate expiration */
			}
		}

		params->deadline = deadline_abs;
		params->interval = 0; /* NOTE_ABSOLUTE is non-repeating */
	} else if (kev->data < 0) {
		/*
		 * Negative interval timers fire immediately, once.
		 *
		 * Ideally a negative interval would be an error, but certain clients
		 * pass negative values on accident, and expect an event back.
		 *
		 * In the old implementation the timer would repeat with no delay
		 * N times until mach_absolute_time() + (N * interval) underflowed,
		 * then it would wait ~forever by accidentally arming a timer for the far future.
		 *
		 * We now skip the power-wasting hot spin phase and go straight to the idle phase.
		 */

		params->deadline = 0; /* expire immediately */
		params->interval = 0; /* non-repeating */
	} else {
		uint64_t interval_abs = 0;

		if (use_abstime) {
			interval_abs = (uint64_t)kev->data;
		} else {
			uint64_t interval_ns;
			if (os_mul_overflow((uint64_t)kev->data, multiplier, &interval_ns)) {
				return ERANGE;
			}

			nanoseconds_to_absolutetime(interval_ns, &interval_abs);
		}

		uint64_t deadline = 0;

		if (kev->fflags & NOTE_MACH_CONTINUOUS_TIME) {
			clock_continuoustime_interval_to_deadline(interval_abs, &deadline);
		} else {
			clock_absolutetime_interval_to_deadline(interval_abs, &deadline);
		}

		params->deadline = deadline;
		params->interval = interval_abs;
	}

	return 0;
}

/*
 * filt_timerexpire - the timer callout routine
 */
static void
filt_timerexpire(void *knx, void *state_on_arm)
{
	struct knote *kn = knx;

	uint32_t state = (uint32_t)(uintptr_t)state_on_arm;
	uint32_t fired_state = state ^ TIMER_ARMED ^ TIMER_FIRED;

	if (os_atomic_cmpxchg(&kn->kn_hook32, state, fired_state, relaxed)) {
		// our f_event always would say FILTER_ACTIVE,
		// so be leaner and just do it.
		struct kqueue *kq = knote_get_kq(kn);
		kqlock(kq);
		knote_activate(kq, kn, FILTER_ACTIVE);
		kqunlock(kq);
	} else {
		/*
		 * The timer has been reprogrammed or canceled since it was armed,
		 * and this is a late firing for the timer, just ignore it.
		 */
	}
}

/*
 * Does this deadline needs a timer armed for it, or has it expired?
 */
static bool
filt_timer_is_ready(struct knote *kn)
{
	uint64_t now, deadline = kn->kn_ext[0];

	if (deadline == 0) {
		return true;
	}

	if (kn->kn_sfflags & NOTE_MACH_CONTINUOUS_TIME) {
		now = mach_continuous_time();
	} else {
		now = mach_absolute_time();
	}
	return deadline <= now;
}

/*
 * Arm a timer
 *
 * It is the responsibility of the caller to make sure the timer call
 * has completed or been cancelled properly prior to arming it.
 */
static void
filt_timerarm(struct knote *kn)
{
	uint64_t deadline = kn->kn_ext[0];
	uint64_t leeway   = kn->kn_ext[1];
	uint32_t state;

	int filter_flags = kn->kn_sfflags;
	unsigned int timer_flags = 0;

	if (filter_flags & NOTE_CRITICAL) {
		timer_flags |= THREAD_CALL_DELAY_USER_CRITICAL;
	} else if (filter_flags & NOTE_BACKGROUND) {
		timer_flags |= THREAD_CALL_DELAY_USER_BACKGROUND;
	} else {
		timer_flags |= THREAD_CALL_DELAY_USER_NORMAL;
	}

	if (filter_flags & NOTE_LEEWAY) {
		timer_flags |= THREAD_CALL_DELAY_LEEWAY;
	}

	if (filter_flags & NOTE_MACH_CONTINUOUS_TIME) {
		timer_flags |= THREAD_CALL_CONTINUOUS;
	}

	/*
	 * Move to ARMED.
	 *
	 * We increase the gencount, and setup the thread call with this expected
	 * state. It means that if there was a previous generation of the timer in
	 * flight that needs to be ignored, then 3 things are possible:
	 *
	 * - the timer fires first, filt_timerexpire() and sets the state to FIRED
	 *   but we clobber it with ARMED and a new gencount. The knote will still
	 *   be activated, but filt_timerprocess() which is serialized with this
	 *   call will not see the FIRED bit set and will not deliver an event.
	 *
	 * - this code runs first, but filt_timerexpire() comes second. Because it
	 *   knows an old gencount, it will debounce and not activate the knote.
	 *
	 * - filt_timerexpire() wasn't in flight yet, and thread_call_enter below
	 *   will just cancel it properly.
	 *
	 * This is important as userspace expects to never be woken up for past
	 * timers after filt_timertouch ran.
	 */
	state = os_atomic_load(&kn->kn_hook32, relaxed);
	state &= ~TIMER_STATE_MASK;
	state += TIMER_GEN_INC + TIMER_ARMED;
	os_atomic_store(&kn->kn_hook32, state, relaxed);

	thread_call_enter_delayed_with_leeway(kn->kn_thcall,
	    (void *)(uintptr_t)state, deadline, leeway, timer_flags);
}

/*
 * Mark a timer as "already fired" when it is being reprogrammed
 *
 * If there is a timer in flight, this will do a best effort at canceling it,
 * but will not wait. If the thread call was in flight, having set the
 * TIMER_IMMEDIATE bit will debounce a filt_timerexpire() racing with this
 * cancelation.
 */
static void
filt_timerfire_immediate(struct knote *kn)
{
	uint32_t state;

	static_assert(TIMER_IMMEDIATE == TIMER_STATE_MASK,
	    "validate that this atomic or will transition to IMMEDIATE");
	state = os_atomic_or_orig(&kn->kn_hook32, TIMER_IMMEDIATE, relaxed);

	if ((state & TIMER_STATE_MASK) == TIMER_ARMED) {
		thread_call_cancel(kn->kn_thcall);
	}
}

/*
 * Allocate a thread call for the knote's lifetime, and kick off the timer.
 */
static int
filt_timerattach(struct knote *kn, struct kevent_qos_s *kev)
{
	thread_call_t callout;
	struct filt_timer_params params;
	int error;

	if ((error = filt_timervalidate(kev, &params)) != 0) {
		knote_set_error(kn, error);
		return 0;
	}

	callout = thread_call_allocate_with_options(filt_timerexpire,
	    (thread_call_param_t)kn, THREAD_CALL_PRIORITY_HIGH,
	    THREAD_CALL_OPTIONS_ONCE);

	if (NULL == callout) {
		knote_set_error(kn, ENOMEM);
		return 0;
	}

	filt_timer_set_params(kn, &params);
	kn->kn_thcall = callout;
	kn->kn_flags |= EV_CLEAR;
	os_atomic_store(&kn->kn_hook32, TIMER_IDLE, relaxed);

	/* NOTE_ABSOLUTE implies EV_ONESHOT */
	if (kn->kn_sfflags & NOTE_ABSOLUTE) {
		kn->kn_flags |= EV_ONESHOT;
	}

	if (filt_timer_is_ready(kn)) {
		os_atomic_store(&kn->kn_hook32, TIMER_IMMEDIATE, relaxed);
		return FILTER_ACTIVE;
	} else {
		filt_timerarm(kn);
		return 0;
	}
}

/*
 * Shut down the timer if it's running, and free the callout.
 */
static void
filt_timerdetach(struct knote *kn)
{
	__assert_only boolean_t freed;

	/*
	 * Unconditionally cancel to make sure there can't be any filt_timerexpire()
	 * running anymore.
	 */
	thread_call_cancel_wait(kn->kn_thcall);
	freed = thread_call_free(kn->kn_thcall);
	assert(freed);
}

/*
 * filt_timertouch - update timer knote with new user input
 *
 * Cancel and restart the timer based on new user data. When
 * the user picks up a knote, clear the count of how many timer
 * pops have gone off (in kn_data).
 */
static int
filt_timertouch(struct knote *kn, struct kevent_qos_s *kev)
{
	struct filt_timer_params params;
	uint32_t changed_flags = (kn->kn_sfflags ^ kev->fflags);
	int error;

	if (kev->qos && (knote_get_kq(kn)->kq_state & KQ_WORKLOOP) &&
	    !_pthread_priority_thread_qos(kev->qos)) {
		/* validate usage of FILTER_UPDATE_REQ_QOS */
		kev->flags |= EV_ERROR;
		kev->data = ERANGE;
		return 0;
	}

	if (changed_flags & NOTE_ABSOLUTE) {
		kev->flags |= EV_ERROR;
		kev->data = EINVAL;
		return 0;
	}

	if ((error = filt_timervalidate(kev, &params)) != 0) {
		kev->flags |= EV_ERROR;
		kev->data = error;
		return 0;
	}

	/* capture the new values used to compute deadline */
	filt_timer_set_params(kn, &params);
	kn->kn_sfflags = kev->fflags;

	if (filt_timer_is_ready(kn)) {
		filt_timerfire_immediate(kn);
		return FILTER_ACTIVE | FILTER_UPDATE_REQ_QOS;
	} else {
		filt_timerarm(kn);
		return FILTER_UPDATE_REQ_QOS;
	}
}

/*
 * filt_timerprocess - query state of knote and snapshot event data
 *
 * Determine if the timer has fired in the past, snapshot the state
 * of the kevent for returning to user-space, and clear pending event
 * counters for the next time.
 */
static int
filt_timerprocess(struct knote *kn, struct kevent_qos_s *kev)
{
	uint32_t state = os_atomic_load(&kn->kn_hook32, relaxed);

	/*
	 * filt_timerprocess is serialized with any filter routine except for
	 * filt_timerexpire which atomically does a TIMER_ARMED -> TIMER_FIRED
	 * transition, and on success, activates the knote.
	 *
	 * Hence, we don't need atomic modifications of the state, only to peek at
	 * whether we see any of the "FIRED" state, and if we do, it is safe to
	 * do simple state machine transitions.
	 */
	switch (state & TIMER_STATE_MASK) {
	case TIMER_IDLE:
	case TIMER_ARMED:
		/*
		 * This can happen if a touch resets a timer that had fired
		 * without being processed
		 */
		return 0;
	}

	os_atomic_store(&kn->kn_hook32, state & ~TIMER_STATE_MASK, relaxed);

	/*
	 * Copy out the interesting kevent state,
	 * but don't leak out the raw time calculations.
	 *
	 * TODO: potential enhancements - tell the user about:
	 *      - deadline to which this timer thought it was expiring
	 *      - return kn_sfflags in the fflags field so the client can know
	 *        under what flags the timer fired
	 */
	knote_fill_kevent(kn, kev, 1);
	kev->ext[0] = 0;
	/* kev->ext[1] = 0;  JMM - shouldn't we hide this too? */

	if (kn->kn_sdata != 0) {
		/*
		 * This is a 'repeating' timer, so we have to emit
		 * how many intervals expired between the arm
		 * and the process.
		 *
		 * A very strange style of interface, because
		 * this could easily be done in the client...
		 */

		uint64_t now;

		if (kn->kn_sfflags & NOTE_MACH_CONTINUOUS_TIME) {
			now = mach_continuous_time();
		} else {
			now = mach_absolute_time();
		}

		uint64_t first_deadline = kn->kn_ext[0];
		uint64_t interval_abs   = kn->kn_sdata;
		uint64_t orig_arm_time  = first_deadline - interval_abs;

		assert(now > orig_arm_time);
		assert(now > first_deadline);

		uint64_t elapsed = now - orig_arm_time;

		uint64_t num_fired = elapsed / interval_abs;

		/*
		 * To reach this code, we must have seen the timer pop
		 * and be in repeating mode, so therefore it must have been
		 * more than 'interval' time since the attach or last
		 * successful touch.
		 */
		assert(num_fired > 0);

		/* report how many intervals have elapsed to the user */
		kev->data = (int64_t)num_fired;

		/* We only need to re-arm the timer if it's not about to be destroyed */
		if ((kn->kn_flags & EV_ONESHOT) == 0) {
			/* fire at the end of the next interval */
			uint64_t new_deadline = first_deadline + num_fired * interval_abs;

			assert(new_deadline > now);

			kn->kn_ext[0] = new_deadline;

			/*
			 * This can't shortcut setting up the thread call, because
			 * knote_process deactivates EV_CLEAR knotes unconditionnally.
			 */
			filt_timerarm(kn);
		}
	}

	return FILTER_ACTIVE;
}

SECURITY_READ_ONLY_EARLY(static struct filterops) timer_filtops = {
	.f_extended_codes = true,
	.f_attach   = filt_timerattach,
	.f_detach   = filt_timerdetach,
	.f_event    = filt_bad_event,
	.f_touch    = filt_timertouch,
	.f_process  = filt_timerprocess,
};

#pragma mark user_filtops

static int
filt_userattach(struct knote *kn, __unused struct kevent_qos_s *kev)
{
	if (kn->kn_sfflags & NOTE_TRIGGER) {
		kn->kn_hook32 = FILTER_ACTIVE;
	} else {
		kn->kn_hook32 = 0;
	}
	return kn->kn_hook32;
}

static int
filt_usertouch(struct knote *kn, struct kevent_qos_s *kev)
{
	uint32_t ffctrl;
	int fflags;

	ffctrl = kev->fflags & NOTE_FFCTRLMASK;
	fflags = kev->fflags & NOTE_FFLAGSMASK;
	switch (ffctrl) {
	case NOTE_FFNOP:
		break;
	case NOTE_FFAND:
		kn->kn_sfflags &= fflags;
		break;
	case NOTE_FFOR:
		kn->kn_sfflags |= fflags;
		break;
	case NOTE_FFCOPY:
		kn->kn_sfflags = fflags;
		break;
	}
	kn->kn_sdata = kev->data;

	if (kev->fflags & NOTE_TRIGGER) {
		kn->kn_hook32 = FILTER_ACTIVE;
	}
	return (int)kn->kn_hook32;
}

static int
filt_userprocess(struct knote *kn, struct kevent_qos_s *kev)
{
	int result = (int)kn->kn_hook32;

	if (result) {
		/* EVFILT_USER returns the data that was passed in */
		knote_fill_kevent_with_sdata(kn, kev);
		kev->fflags = kn->kn_sfflags;
		if (kn->kn_flags & EV_CLEAR) {
			/* knote_fill_kevent cleared kn_fflags */
			kn->kn_hook32 = 0;
		}
	}

	return result;
}

SECURITY_READ_ONLY_EARLY(static struct filterops) user_filtops = {
	.f_extended_codes = true,
	.f_attach  = filt_userattach,
	.f_detach  = filt_no_detach,
	.f_event   = filt_bad_event,
	.f_touch   = filt_usertouch,
	.f_process = filt_userprocess,
};

#pragma mark workloop_filtops

#define EPREEMPTDISABLED (-1)

static inline void
filt_wllock(struct kqworkloop *kqwl)
{
	lck_spin_lock(&kqwl->kqwl_statelock);
}

static inline void
filt_wlunlock(struct kqworkloop *kqwl)
{
	lck_spin_unlock(&kqwl->kqwl_statelock);
}

/*
 * Returns true when the interlock for the turnstile is the workqueue lock
 *
 * When this is the case, all turnstiles operations are delegated
 * to the workqueue subsystem.
 *
 * This is required because kqueue_threadreq_bind_prepost only holds the
 * workqueue lock but needs to move the inheritor from the workloop turnstile
 * away from the creator thread, so that this now fulfilled request cannot be
 * picked anymore by other threads.
 */
static inline bool
filt_wlturnstile_interlock_is_workq(struct kqworkloop *kqwl)
{
	return kqr_thread_requested_pending(&kqwl->kqwl_request);
}

static void
filt_wlupdate_inheritor(struct kqworkloop *kqwl, struct turnstile *ts,
    turnstile_update_flags_t flags)
{
	turnstile_inheritor_t inheritor = TURNSTILE_INHERITOR_NULL;
	workq_threadreq_t kqr = &kqwl->kqwl_request;

	/*
	 * binding to the workq should always happen through
	 * workq_kern_threadreq_update_inheritor()
	 */
	assert(!filt_wlturnstile_interlock_is_workq(kqwl));

	if ((inheritor = kqwl->kqwl_owner)) {
		flags |= TURNSTILE_INHERITOR_THREAD;
	} else if ((inheritor = kqr_thread(kqr))) {
		flags |= TURNSTILE_INHERITOR_THREAD;
	}

	turnstile_update_inheritor(ts, inheritor, flags);
}

#define EVFILT_WORKLOOP_EFAULT_RETRY_COUNT 100
#define FILT_WLATTACH 0
#define FILT_WLTOUCH  1
#define FILT_WLDROP   2

__result_use_check
static int
filt_wlupdate(struct kqworkloop *kqwl, struct knote *kn,
    struct kevent_qos_s *kev, kq_index_t qos_index, int op)
{
	user_addr_t uaddr = CAST_USER_ADDR_T(kev->ext[EV_EXTIDX_WL_ADDR]);
	workq_threadreq_t kqr = &kqwl->kqwl_request;
	thread_t cur_owner, new_owner, extra_thread_ref = THREAD_NULL;
	kq_index_t cur_override = THREAD_QOS_UNSPECIFIED;
	int efault_retry = EVFILT_WORKLOOP_EFAULT_RETRY_COUNT;
	int action = KQWL_UTQ_NONE, error = 0;
	bool wl_inheritor_updated = false, needs_wake = false;
	uint64_t kdata = kev->ext[EV_EXTIDX_WL_VALUE];
	uint64_t mask = kev->ext[EV_EXTIDX_WL_MASK];
	uint64_t udata = 0;
	struct turnstile *ts = TURNSTILE_NULL;

	filt_wllock(kqwl);

again:
	new_owner = cur_owner = kqwl->kqwl_owner;

	/*
	 * Phase 1:
	 *
	 * If asked, load the uint64 value at the user provided address and compare
	 * it against the passed in mask and expected value.
	 *
	 * If NOTE_WL_DISCOVER_OWNER is specified, translate the loaded name as
	 * a thread reference.
	 *
	 * If NOTE_WL_END_OWNERSHIP is specified and the currently known owner is
	 * the current thread, then end ownership.
	 *
	 * Lastly decide whether we need to perform a QoS update.
	 */
	if (uaddr) {
		/*
		 * Until <rdar://problem/24999882> exists,
		 * disabling preemption copyin forces any
		 * vm_fault we encounter to fail.
		 */
		error = copyin_atomic64(uaddr, &udata);

		/*
		 * If we get EFAULT, drop locks, and retry.
		 * If we still get an error report it,
		 * else assume the memory has been faulted
		 * and attempt to copyin under lock again.
		 */
		switch (error) {
		case 0:
			break;
		case EFAULT:
			if (efault_retry-- > 0) {
				filt_wlunlock(kqwl);
				error = copyin_atomic64(uaddr, &udata);
				filt_wllock(kqwl);
				if (error == 0) {
					goto again;
				}
			}
			OS_FALLTHROUGH;
		default:
			goto out;
		}

		/* Update state as copied in.  */
		kev->ext[EV_EXTIDX_WL_VALUE] = udata;

		if ((udata & mask) != (kdata & mask)) {
			error = ESTALE;
		} else if (kev->fflags & NOTE_WL_DISCOVER_OWNER) {
			/*
			 * Decipher the owner port name, and translate accordingly.
			 * The low 2 bits were borrowed for other flags, so mask them off.
			 *
			 * Then attempt translation to a thread reference or fail.
			 */
			mach_port_name_t name = (mach_port_name_t)udata & ~0x3;
			if (name != MACH_PORT_NULL) {
				name = ipc_entry_name_mask(name);
				extra_thread_ref = port_name_to_thread(name,
				    PORT_INTRANS_THREAD_IN_CURRENT_TASK);
				if (extra_thread_ref == THREAD_NULL) {
					error = EOWNERDEAD;
					goto out;
				}
				new_owner = extra_thread_ref;
			}
		}
	}

	if ((kev->fflags & NOTE_WL_END_OWNERSHIP) && new_owner == current_thread()) {
		new_owner = THREAD_NULL;
	}

	if (error == 0) {
		if ((kev->fflags & NOTE_WL_THREAD_REQUEST) && (kev->flags & EV_DELETE)) {
			action = KQWL_UTQ_SET_QOS_INDEX;
		} else if (qos_index && kqr->tr_kq_qos_index != qos_index) {
			action = KQWL_UTQ_SET_QOS_INDEX;
		}

		if (op == FILT_WLTOUCH) {
			/*
			 * Save off any additional fflags/data we just accepted
			 * But only keep the last round of "update" bits we acted on which helps
			 * debugging a lot.
			 */
			kn->kn_sfflags &= ~NOTE_WL_UPDATES_MASK;
			kn->kn_sfflags |= kev->fflags;
			if (kev->fflags & NOTE_WL_SYNC_WAKE) {
				needs_wake = (kn->kn_thread != THREAD_NULL);
			}
		} else if (op == FILT_WLDROP) {
			if ((kn->kn_sfflags & (NOTE_WL_SYNC_WAIT | NOTE_WL_SYNC_WAKE)) ==
			    NOTE_WL_SYNC_WAIT) {
				/*
				 * When deleting a SYNC_WAIT knote that hasn't been woken up
				 * explicitly, issue a wake up.
				 */
				kn->kn_sfflags |= NOTE_WL_SYNC_WAKE;
				needs_wake = (kn->kn_thread != THREAD_NULL);
			}
		}
	}

	/*
	 * Phase 2:
	 *
	 * Commit ownership and QoS changes if any, possibly wake up waiters
	 */

	if (cur_owner == new_owner && action == KQWL_UTQ_NONE && !needs_wake) {
		goto out;
	}

	kqlock(kqwl);

	/* If already tracked as servicer, don't track as owner */
	if (new_owner == kqr_thread(kqr)) {
		new_owner = THREAD_NULL;
	}

	if (cur_owner != new_owner) {
		kqwl->kqwl_owner = new_owner;
		if (new_owner == extra_thread_ref) {
			/* we just transfered this ref to kqwl_owner */
			extra_thread_ref = THREAD_NULL;
		}
		cur_override = kqworkloop_override(kqwl);

		if (new_owner) {
			/* override it before we drop the old */
			if (cur_override != THREAD_QOS_UNSPECIFIED) {
				thread_add_kevent_override(new_owner, cur_override);
			}
			if (kqr_thread_requested_pending(kqr)) {
				if (action == KQWL_UTQ_NONE) {
					action = KQWL_UTQ_REDRIVE_EVENTS;
				}
			}
		} else if (action == KQWL_UTQ_NONE &&
		    !kqr_thread_requested(kqr) &&
		    kqwl->kqwl_wakeup_qos) {
			action = KQWL_UTQ_REDRIVE_EVENTS;
		}
	}

	if (action != KQWL_UTQ_NONE) {
		kqworkloop_update_threads_qos(kqwl, action, qos_index);
	}

	ts = kqwl->kqwl_turnstile;
	if (cur_owner != new_owner && ts) {
		if (action == KQWL_UTQ_REDRIVE_EVENTS) {
			/*
			 * Note that when action is KQWL_UTQ_REDRIVE_EVENTS,
			 * the code went through workq_kern_threadreq_initiate()
			 * and the workqueue has set the inheritor already
			 */
			assert(filt_wlturnstile_interlock_is_workq(kqwl));
		} else if (filt_wlturnstile_interlock_is_workq(kqwl)) {
			workq_kern_threadreq_lock(kqwl->kqwl_p);
			workq_kern_threadreq_update_inheritor(kqwl->kqwl_p, kqr, new_owner,
			    ts, TURNSTILE_IMMEDIATE_UPDATE);
			workq_kern_threadreq_unlock(kqwl->kqwl_p);
			if (!filt_wlturnstile_interlock_is_workq(kqwl)) {
				/*
				 * If the workq is no longer the interlock, then
				 * workq_kern_threadreq_update_inheritor() has finished a bind
				 * and we need to fallback to the regular path.
				 */
				filt_wlupdate_inheritor(kqwl, ts, TURNSTILE_IMMEDIATE_UPDATE);
			}
			wl_inheritor_updated = true;
		} else {
			filt_wlupdate_inheritor(kqwl, ts, TURNSTILE_IMMEDIATE_UPDATE);
			wl_inheritor_updated = true;
		}

		/*
		 * We need a turnstile reference because we are dropping the interlock
		 * and the caller has not called turnstile_prepare.
		 */
		if (wl_inheritor_updated) {
			turnstile_reference(ts);
		}
	}

	if (needs_wake && ts) {
		waitq_wakeup64_thread(&ts->ts_waitq, knote_filt_wev64(kn),
		    kn->kn_thread, THREAD_AWAKENED);
		if (op == FILT_WLATTACH || op == FILT_WLTOUCH) {
			disable_preemption();
			error = EPREEMPTDISABLED;
		}
	}

	kqunlock(kqwl);

out:
	/*
	 * Phase 3:
	 *
	 * Unlock and cleanup various lingering references and things.
	 */
	filt_wlunlock(kqwl);

#if CONFIG_WORKLOOP_DEBUG
	KQWL_HISTORY_WRITE_ENTRY(kqwl, {
		.updater = current_thread(),
		.servicer = kqr_thread(kqr), /* Note: racy */
		.old_owner = cur_owner,
		.new_owner = new_owner,

		.kev_ident  = kev->ident,
		.error      = (int16_t)error,
		.kev_flags  = kev->flags,
		.kev_fflags = kev->fflags,

		.kev_mask   = mask,
		.kev_value  = kdata,
		.in_value   = udata,
	});
#endif // CONFIG_WORKLOOP_DEBUG

	if (wl_inheritor_updated) {
		turnstile_update_inheritor_complete(ts, TURNSTILE_INTERLOCK_NOT_HELD);
		turnstile_deallocate_safe(ts);
	}

	if (cur_owner && new_owner != cur_owner) {
		if (cur_override != THREAD_QOS_UNSPECIFIED) {
			thread_drop_kevent_override(cur_owner);
		}
		thread_deallocate_safe(cur_owner);
	}
	if (extra_thread_ref) {
		thread_deallocate_safe(extra_thread_ref);
	}
	return error;
}

/*
 * Remembers the last updated that came in from userspace for debugging reasons.
 * - fflags is mirrored from the userspace kevent
 * - ext[i, i != VALUE] is mirrored from the userspace kevent
 * - ext[VALUE] is set to what the kernel loaded atomically
 * - data is set to the error if any
 */
static inline void
filt_wlremember_last_update(struct knote *kn, struct kevent_qos_s *kev,
    int error)
{
	kn->kn_fflags = kev->fflags;
	kn->kn_sdata = error;
	memcpy(kn->kn_ext, kev->ext, sizeof(kev->ext));
}

static int
filt_wlupdate_sync_ipc(struct kqworkloop *kqwl, struct knote *kn,
    struct kevent_qos_s *kev, int op)
{
	user_addr_t uaddr = (user_addr_t) kev->ext[EV_EXTIDX_WL_ADDR];
	uint64_t kdata = kev->ext[EV_EXTIDX_WL_VALUE];
	uint64_t mask  = kev->ext[EV_EXTIDX_WL_MASK];
	uint64_t udata = 0;
	int efault_retry = EVFILT_WORKLOOP_EFAULT_RETRY_COUNT;
	int error = 0;

	if (op == FILT_WLATTACH) {
		(void)kqueue_alloc_turnstile(&kqwl->kqwl_kqueue);
	} else if (uaddr == 0) {
		return 0;
	}

	filt_wllock(kqwl);

again:

	/*
	 * Do the debounce thing, the lock serializing the state is the knote lock.
	 */
	if (uaddr) {
		/*
		 * Until <rdar://problem/24999882> exists,
		 * disabling preemption copyin forces any
		 * vm_fault we encounter to fail.
		 */
		error = copyin_atomic64(uaddr, &udata);

		/*
		 * If we get EFAULT, drop locks, and retry.
		 * If we still get an error report it,
		 * else assume the memory has been faulted
		 * and attempt to copyin under lock again.
		 */
		switch (error) {
		case 0:
			break;
		case EFAULT:
			if (efault_retry-- > 0) {
				filt_wlunlock(kqwl);
				error = copyin_atomic64(uaddr, &udata);
				filt_wllock(kqwl);
				if (error == 0) {
					goto again;
				}
			}
			OS_FALLTHROUGH;
		default:
			goto out;
		}

		kev->ext[EV_EXTIDX_WL_VALUE] = udata;
		kn->kn_ext[EV_EXTIDX_WL_VALUE] = udata;

		if ((udata & mask) != (kdata & mask)) {
			error = ESTALE;
			goto out;
		}
	}

	if (op == FILT_WLATTACH) {
		error = filt_wlattach_sync_ipc(kn);
		if (error == 0) {
			disable_preemption();
			error = EPREEMPTDISABLED;
		}
	}

out:
	filt_wlunlock(kqwl);
	return error;
}

static int
filt_wlattach(struct knote *kn, struct kevent_qos_s *kev)
{
	struct kqueue *kq = knote_get_kq(kn);
	struct kqworkloop *kqwl = (struct kqworkloop *)kq;
	int error = 0, result = 0;
	kq_index_t qos_index = 0;

	if (__improbable((kq->kq_state & KQ_WORKLOOP) == 0)) {
		error = ENOTSUP;
		goto out;
	}

	uint32_t command = (kn->kn_sfflags & NOTE_WL_COMMANDS_MASK);
	switch (command) {
	case NOTE_WL_THREAD_REQUEST:
		if (kn->kn_id != kqwl->kqwl_dynamicid) {
			error = EINVAL;
			goto out;
		}
		qos_index = _pthread_priority_thread_qos(kn->kn_qos);
		if (qos_index == THREAD_QOS_UNSPECIFIED) {
			error = ERANGE;
			goto out;
		}
		if (kqwl->kqwl_request.tr_kq_qos_index) {
			/*
			 * There already is a thread request, and well, you're only allowed
			 * one per workloop, so fail the attach.
			 */
			error = EALREADY;
			goto out;
		}
		break;
	case NOTE_WL_SYNC_WAIT:
	case NOTE_WL_SYNC_WAKE:
		if (kn->kn_id == kqwl->kqwl_dynamicid) {
			error = EINVAL;
			goto out;
		}
		if ((kn->kn_flags & EV_DISABLE) == 0) {
			error = EINVAL;
			goto out;
		}
		if (kn->kn_sfflags & NOTE_WL_END_OWNERSHIP) {
			error = EINVAL;
			goto out;
		}
		break;

	case NOTE_WL_SYNC_IPC:
		if ((kn->kn_flags & EV_DISABLE) == 0) {
			error = EINVAL;
			goto out;
		}
		if (kn->kn_sfflags & (NOTE_WL_UPDATE_QOS | NOTE_WL_DISCOVER_OWNER)) {
			error = EINVAL;
			goto out;
		}
		break;
	default:
		error = EINVAL;
		goto out;
	}

	if (command == NOTE_WL_SYNC_IPC) {
		error = filt_wlupdate_sync_ipc(kqwl, kn, kev, FILT_WLATTACH);
	} else {
		error = filt_wlupdate(kqwl, kn, kev, qos_index, FILT_WLATTACH);
	}

	if (error == EPREEMPTDISABLED) {
		error = 0;
		result = FILTER_THREADREQ_NODEFEER;
	}
out:
	if (error) {
		/* If userland wants ESTALE to be hidden, fail the attach anyway */
		if (error == ESTALE && (kn->kn_sfflags & NOTE_WL_IGNORE_ESTALE)) {
			error = 0;
		}
		knote_set_error(kn, error);
		return result;
	}
	if (command == NOTE_WL_SYNC_WAIT) {
		return kevent_register_wait_prepare(kn, kev, result);
	}
	/* Just attaching the thread request successfully will fire it */
	if (command == NOTE_WL_THREAD_REQUEST) {
		/*
		 * Thread Request knotes need an explicit touch to be active again,
		 * so delivering an event needs to also consume it.
		 */
		kn->kn_flags |= EV_CLEAR;
		return result | FILTER_ACTIVE;
	}
	return result;
}

static void __dead2
filt_wlwait_continue(void *parameter, wait_result_t wr)
{
	struct _kevent_register *cont_args = parameter;
	struct kqworkloop *kqwl = cont_args->kqwl;

	kqlock(kqwl);
	if (filt_wlturnstile_interlock_is_workq(kqwl)) {
		workq_kern_threadreq_lock(kqwl->kqwl_p);
		turnstile_complete((uintptr_t)kqwl, &kqwl->kqwl_turnstile, NULL, TURNSTILE_WORKLOOPS);
		workq_kern_threadreq_unlock(kqwl->kqwl_p);
	} else {
		turnstile_complete((uintptr_t)kqwl, &kqwl->kqwl_turnstile, NULL, TURNSTILE_WORKLOOPS);
	}
	kqunlock(kqwl);

	turnstile_cleanup();

	if (wr == THREAD_INTERRUPTED) {
		cont_args->kev.flags |= EV_ERROR;
		cont_args->kev.data = EINTR;
	} else if (wr != THREAD_AWAKENED) {
		panic("Unexpected wait result: %d", wr);
	}

	kevent_register_wait_return(cont_args);
}

/*
 * Called with the workloop mutex held, most of the time never returns as it
 * calls filt_wlwait_continue through a continuation.
 */
static void __dead2
filt_wlpost_register_wait(struct uthread *uth, struct knote *kn,
    struct _kevent_register *cont_args)
{
	struct kqworkloop *kqwl = cont_args->kqwl;
	workq_threadreq_t kqr = &kqwl->kqwl_request;
	struct turnstile *ts;
	bool workq_locked = false;

	kqlock_held(kqwl);

	if (filt_wlturnstile_interlock_is_workq(kqwl)) {
		workq_kern_threadreq_lock(kqwl->kqwl_p);
		workq_locked = true;
	}

	ts = turnstile_prepare((uintptr_t)kqwl, &kqwl->kqwl_turnstile,
	    TURNSTILE_NULL, TURNSTILE_WORKLOOPS);

	if (workq_locked) {
		workq_kern_threadreq_update_inheritor(kqwl->kqwl_p,
		    &kqwl->kqwl_request, kqwl->kqwl_owner, ts,
		    TURNSTILE_DELAYED_UPDATE);
		if (!filt_wlturnstile_interlock_is_workq(kqwl)) {
			/*
			 * if the interlock is no longer the workqueue lock,
			 * then we don't need to hold it anymore.
			 */
			workq_kern_threadreq_unlock(kqwl->kqwl_p);
			workq_locked = false;
		}
	}
	if (!workq_locked) {
		/*
		 * If the interlock is the workloop's, then it's our responsibility to
		 * call update_inheritor, so just do it.
		 */
		filt_wlupdate_inheritor(kqwl, ts, TURNSTILE_DELAYED_UPDATE);
	}

	thread_set_pending_block_hint(get_machthread(uth), kThreadWaitWorkloopSyncWait);
	waitq_assert_wait64(&ts->ts_waitq, knote_filt_wev64(kn),
	    THREAD_ABORTSAFE, TIMEOUT_WAIT_FOREVER);

	if (workq_locked) {
		workq_kern_threadreq_unlock(kqwl->kqwl_p);
	}

	thread_t thread = kqwl->kqwl_owner ?: kqr_thread(kqr);
	if (thread) {
		thread_reference(thread);
	}

	kevent_register_wait_block(ts, thread, filt_wlwait_continue, cont_args);
}

/* called in stackshot context to report the thread responsible for blocking this thread */
void
kdp_workloop_sync_wait_find_owner(__assert_only thread_t thread,
    event64_t event, thread_waitinfo_t *waitinfo)
{
	struct knote *kn = (struct knote *)event;

	zone_require(knote_zone, kn);

	assert(kn->kn_thread == thread);

	struct kqueue *kq = knote_get_kq(kn);

	zone_require(kqworkloop_zone, kq);
	assert(kq->kq_state & KQ_WORKLOOP);

	struct kqworkloop *kqwl = (struct kqworkloop *)kq;
	workq_threadreq_t kqr = &kqwl->kqwl_request;

	thread_t kqwl_owner = kqwl->kqwl_owner;

	if (kqwl_owner != THREAD_NULL) {
		thread_require(kqwl_owner);
		waitinfo->owner = thread_tid(kqwl->kqwl_owner);
	} else if ((kqr->tr_state >= WORKQ_TR_STATE_BINDING) && (kqr->tr_thread != NULL)) {
		thread_require(kqr->tr_thread);
		waitinfo->owner = thread_tid(kqr->tr_thread);
	} else if (kqr_thread_requested_pending(kqr)) { /* > idle, < bound */
		waitinfo->owner = STACKSHOT_WAITOWNER_THREQUESTED;
	} else {
		waitinfo->owner = 0;
	}

	waitinfo->context = kqwl->kqwl_dynamicid;
}

static void
filt_wldetach(struct knote *kn)
{
	if (kn->kn_sfflags & NOTE_WL_SYNC_IPC) {
		filt_wldetach_sync_ipc(kn);
	} else if (kn->kn_thread) {
		kevent_register_wait_cleanup(kn);
	}
}

static int
filt_wlvalidate_kev_flags(struct knote *kn, struct kevent_qos_s *kev,
    thread_qos_t *qos_index)
{
	uint32_t new_commands = kev->fflags & NOTE_WL_COMMANDS_MASK;
	uint32_t sav_commands = kn->kn_sfflags & NOTE_WL_COMMANDS_MASK;

	if ((kev->fflags & NOTE_WL_DISCOVER_OWNER) && (kev->flags & EV_DELETE)) {
		return EINVAL;
	}
	if (kev->fflags & NOTE_WL_UPDATE_QOS) {
		if (kev->flags & EV_DELETE) {
			return EINVAL;
		}
		if (sav_commands != NOTE_WL_THREAD_REQUEST) {
			return EINVAL;
		}
		if (!(*qos_index = _pthread_priority_thread_qos(kev->qos))) {
			return ERANGE;
		}
	}

	switch (new_commands) {
	case NOTE_WL_THREAD_REQUEST:
		/* thread requests can only update themselves */
		if (sav_commands != NOTE_WL_THREAD_REQUEST) {
			return EINVAL;
		}
		break;

	case NOTE_WL_SYNC_WAIT:
		if (kev->fflags & NOTE_WL_END_OWNERSHIP) {
			return EINVAL;
		}
		goto sync_checks;

	case NOTE_WL_SYNC_WAKE:
sync_checks:
		if (!(sav_commands & (NOTE_WL_SYNC_WAIT | NOTE_WL_SYNC_WAKE))) {
			return EINVAL;
		}
		if ((kev->flags & (EV_ENABLE | EV_DELETE)) == EV_ENABLE) {
			return EINVAL;
		}
		break;

	case NOTE_WL_SYNC_IPC:
		if (sav_commands != NOTE_WL_SYNC_IPC) {
			return EINVAL;
		}
		if ((kev->flags & (EV_ENABLE | EV_DELETE)) == EV_ENABLE) {
			return EINVAL;
		}
		break;

	default:
		return EINVAL;
	}
	return 0;
}

static int
filt_wltouch(struct knote *kn, struct kevent_qos_s *kev)
{
	struct kqworkloop *kqwl = (struct kqworkloop *)knote_get_kq(kn);
	thread_qos_t qos_index = THREAD_QOS_UNSPECIFIED;
	int result = 0;

	int error = filt_wlvalidate_kev_flags(kn, kev, &qos_index);
	if (error) {
		goto out;
	}

	uint32_t command = kev->fflags & NOTE_WL_COMMANDS_MASK;
	if (command == NOTE_WL_SYNC_IPC) {
		error = filt_wlupdate_sync_ipc(kqwl, kn, kev, FILT_WLTOUCH);
	} else {
		error = filt_wlupdate(kqwl, kn, kev, qos_index, FILT_WLTOUCH);
		filt_wlremember_last_update(kn, kev, error);
	}
	if (error == EPREEMPTDISABLED) {
		error = 0;
		result = FILTER_THREADREQ_NODEFEER;
	}

out:
	if (error) {
		if (error == ESTALE && (kev->fflags & NOTE_WL_IGNORE_ESTALE)) {
			/* If userland wants ESTALE to be hidden, do not activate */
			return result;
		}
		kev->flags |= EV_ERROR;
		kev->data = error;
		return result;
	}
	if (command == NOTE_WL_SYNC_WAIT && !(kn->kn_sfflags & NOTE_WL_SYNC_WAKE)) {
		return kevent_register_wait_prepare(kn, kev, result);
	}
	/* Just touching the thread request successfully will fire it */
	if (command == NOTE_WL_THREAD_REQUEST) {
		if (kev->fflags & NOTE_WL_UPDATE_QOS) {
			result |= FILTER_UPDATE_REQ_QOS;
		}
		result |= FILTER_ACTIVE;
	}
	return result;
}

static bool
filt_wlallow_drop(struct knote *kn, struct kevent_qos_s *kev)
{
	struct kqworkloop *kqwl = (struct kqworkloop *)knote_get_kq(kn);

	int error = filt_wlvalidate_kev_flags(kn, kev, NULL);
	if (error) {
		goto out;
	}

	uint32_t command = (kev->fflags & NOTE_WL_COMMANDS_MASK);
	if (command == NOTE_WL_SYNC_IPC) {
		error = filt_wlupdate_sync_ipc(kqwl, kn, kev, FILT_WLDROP);
	} else {
		error = filt_wlupdate(kqwl, kn, kev, 0, FILT_WLDROP);
		filt_wlremember_last_update(kn, kev, error);
	}
	assert(error != EPREEMPTDISABLED);

out:
	if (error) {
		if (error == ESTALE && (kev->fflags & NOTE_WL_IGNORE_ESTALE)) {
			return false;
		}
		kev->flags |= EV_ERROR;
		kev->data = error;
		return false;
	}
	return true;
}

static int
filt_wlprocess(struct knote *kn, struct kevent_qos_s *kev)
{
	struct kqworkloop *kqwl = (struct kqworkloop *)knote_get_kq(kn);
	int rc = 0;

	assert(kn->kn_sfflags & NOTE_WL_THREAD_REQUEST);

	kqlock(kqwl);

	if (kqwl->kqwl_owner) {
		/*
		 * <rdar://problem/33584321> userspace sometimes due to events being
		 * delivered but not triggering a drain session can cause a process
		 * of the thread request knote.
		 *
		 * When that happens, the automatic deactivation due to process
		 * would swallow the event, so we have to activate the knote again.
		 */
		knote_activate(kqwl, kn, FILTER_ACTIVE);
	} else {
#if DEBUG || DEVELOPMENT
		if (kevent_debug_flags & KEVENT_PANIC_ON_NON_ENQUEUED_PROCESS) {
			/*
			 * see src/queue_internal.h in libdispatch
			 */
#define DISPATCH_QUEUE_ENQUEUED 0x1ull
			user_addr_t addr = CAST_USER_ADDR_T(kn->kn_ext[EV_EXTIDX_WL_ADDR]);
			task_t t = current_task();
			uint64_t val;
			if (addr && task_is_active(t) && !task_is_halting(t) &&
			    copyin_atomic64(addr, &val) == 0 &&
			    val && (val & DISPATCH_QUEUE_ENQUEUED) == 0 &&
			    (val >> 48) != 0xdead && (val >> 48) != 0 && (val >> 48) != 0xffff) {
				panic("kevent: workloop %#016llx is not enqueued "
				    "(kn:%p dq_state:%#016llx kev.dq_state:%#016llx)",
				    kn->kn_udata, kn, val, kn->kn_ext[EV_EXTIDX_WL_VALUE]);
			}
		}
#endif
		knote_fill_kevent(kn, kev, 0);
		kev->fflags = kn->kn_sfflags;
		rc |= FILTER_ACTIVE;
	}

	kqunlock(kqwl);

	if (rc & FILTER_ACTIVE) {
		workq_thread_set_max_qos(kqwl->kqwl_p, &kqwl->kqwl_request);
	}
	return rc;
}

SECURITY_READ_ONLY_EARLY(static struct filterops) workloop_filtops = {
	.f_extended_codes = true,
	.f_attach  = filt_wlattach,
	.f_detach  = filt_wldetach,
	.f_event   = filt_bad_event,
	.f_touch   = filt_wltouch,
	.f_process = filt_wlprocess,
	.f_allow_drop = filt_wlallow_drop,
	.f_post_register_wait = filt_wlpost_register_wait,
};

#pragma mark - kqueues allocation and deallocation

OS_NOINLINE
static void
kqworkloop_dealloc(struct kqworkloop *, bool hash_remove);

static inline bool
kqworkloop_try_retain(struct kqworkloop *kqwl)
{
	return os_ref_retain_try_raw(&kqwl->kqwl_retains, NULL);
}

static inline void
kqworkloop_retain(struct kqworkloop *kqwl)
{
	return os_ref_retain_raw(&kqwl->kqwl_retains, NULL);
}

OS_ALWAYS_INLINE
static inline void
kqueue_retain(kqueue_t kqu)
{
	if (kqu.kq->kq_state & KQ_DYNAMIC) {
		kqworkloop_retain(kqu.kqwl);
	}
}

OS_ALWAYS_INLINE
static inline void
kqworkloop_release_live(struct kqworkloop *kqwl)
{
	os_ref_release_live_raw(&kqwl->kqwl_retains, NULL);
}

OS_ALWAYS_INLINE
static inline void
kqueue_release_live(kqueue_t kqu)
{
	if (kqu.kq->kq_state & KQ_DYNAMIC) {
		kqworkloop_release_live(kqu.kqwl);
	}
}

OS_ALWAYS_INLINE
static inline void
kqworkloop_release(struct kqworkloop *kqwl)
{
	if (os_ref_release_raw(&kqwl->kqwl_retains, NULL) == 0) {
		kqworkloop_dealloc(kqwl, true);
	}
}

OS_ALWAYS_INLINE
static inline void
kqueue_release(kqueue_t kqu)
{
	if (kqu.kq->kq_state & KQ_DYNAMIC) {
		kqworkloop_release(kqu.kqwl);
	}
}

/*!
 * @function kqueue_destroy
 *
 * @brief
 * Common part to all kqueue dealloc functions.
 */
OS_NOINLINE
static void
kqueue_destroy(kqueue_t kqu, zone_t zone)
{
	lck_spin_destroy(&kqu.kq->kq_lock, &kq_lck_grp);

	zfree(zone, kqu.kq);
}

/*!
 * @function kqueue_init
 *
 * @brief
 * Common part to all kqueue alloc functions.
 */
static kqueue_t
kqueue_init(kqueue_t kqu)
{
	lck_spin_init(&kqu.kq->kq_lock, &kq_lck_grp, LCK_ATTR_NULL);
	return kqu;
}

#pragma mark kqfile allocation and deallocation

/*!
 * @function kqueue_dealloc
 *
 * @brief
 * Detach all knotes from a kqfile and free it.
 *
 * @discussion
 * We walk each list looking for knotes referencing this
 * this kqueue.  If we find one, we try to drop it.  But
 * if we fail to get a drop reference, that will wait
 * until it is dropped.  So, we can just restart again
 * safe in the assumption that the list will eventually
 * not contain any more references to this kqueue (either
 * we dropped them all, or someone else did).
 *
 * Assumes no new events are being added to the kqueue.
 * Nothing locked on entry or exit.
 */
void
kqueue_dealloc(struct kqueue *kq)
{
	KNOTE_LOCK_CTX(knlc);
	struct proc *p = kq->kq_p;
	struct filedesc *fdp = &p->p_fd;
	struct knote *kn;

	assert(kq && (kq->kq_state & (KQ_WORKLOOP | KQ_WORKQ)) == 0);

	proc_fdlock(p);
	for (int i = 0; i < fdp->fd_knlistsize; i++) {
		kn = SLIST_FIRST(&fdp->fd_knlist[i]);
		while (kn != NULL) {
			if (kq == knote_get_kq(kn)) {
				kqlock(kq);
				proc_fdunlock(p);
				if (knote_lock(kq, kn, &knlc, KNOTE_KQ_LOCK_ON_SUCCESS)) {
					knote_drop(kq, kn, &knlc);
				}
				proc_fdlock(p);
				/* start over at beginning of list */
				kn = SLIST_FIRST(&fdp->fd_knlist[i]);
				continue;
			}
			kn = SLIST_NEXT(kn, kn_link);
		}
	}

	knhash_lock(fdp);
	proc_fdunlock(p);

	if (fdp->fd_knhashmask != 0) {
		for (int i = 0; i < (int)fdp->fd_knhashmask + 1; i++) {
			kn = SLIST_FIRST(&fdp->fd_knhash[i]);
			while (kn != NULL) {
				if (kq == knote_get_kq(kn)) {
					kqlock(kq);
					knhash_unlock(fdp);
					if (knote_lock(kq, kn, &knlc, KNOTE_KQ_LOCK_ON_SUCCESS)) {
						knote_drop(kq, kn, &knlc);
					}
					knhash_lock(fdp);
					/* start over at beginning of list */
					kn = SLIST_FIRST(&fdp->fd_knhash[i]);
					continue;
				}
				kn = SLIST_NEXT(kn, kn_link);
			}
		}
	}
	knhash_unlock(fdp);

	kqueue_destroy(kq, kqfile_zone);
}

/*!
 * @function kqueue_alloc
 *
 * @brief
 * Allocate a kqfile.
 */
struct kqueue *
kqueue_alloc(struct proc *p)
{
	struct kqfile *kqf;

	/*
	 * kqfiles are created with kqueue() so we need to wait for
	 * the first kevent syscall to know which bit among
	 * KQ_KEV_{32,64,QOS} will be set in kqf_state
	 */
	kqf = zalloc_flags(kqfile_zone, Z_WAITOK | Z_ZERO);
	kqf->kqf_p = p;
	TAILQ_INIT_AFTER_BZERO(&kqf->kqf_queue);
	TAILQ_INIT_AFTER_BZERO(&kqf->kqf_suppressed);

	return kqueue_init(kqf).kq;
}

/*!
 * @function kqueue_internal
 *
 * @brief
 * Core implementation for kqueue and guarded_kqueue_np()
 */
int
kqueue_internal(struct proc *p, fp_initfn_t fp_init, void *initarg, int32_t *retval)
{
	struct kqueue *kq;
	struct fileproc *fp;
	int fd, error;

	error = falloc_withinit(p, current_cached_proc_cred(p),
	    vfs_context_current(), &fp, &fd, fp_init, initarg);
	if (error) {
		return error;
	}

	kq = kqueue_alloc(p);
	if (kq == NULL) {
		fp_free(p, fd, fp);
		return ENOMEM;
	}

	fp->fp_flags |= FP_CLOEXEC | FP_CLOFORK;
	fp->f_flag = FREAD | FWRITE;
	fp->f_ops = &kqueueops;
	fp_set_data(fp, kq);
	fp->f_lflags |= FG_CONFINED;

	proc_fdlock(p);
	procfdtbl_releasefd(p, fd, NULL);
	fp_drop(p, fd, fp, 1);
	proc_fdunlock(p);

	*retval = fd;
	return error;
}

/*!
 * @function kqueue
 *
 * @brief
 * The kqueue syscall.
 */
int
kqueue(struct proc *p, __unused struct kqueue_args *uap, int32_t *retval)
{
	return kqueue_internal(p, NULL, NULL, retval);
}

#pragma mark kqworkq allocation and deallocation

/*!
 * @function kqworkq_dealloc
 *
 * @brief
 * Deallocates a workqueue kqueue.
 *
 * @discussion
 * This only happens at process death, or for races with concurrent
 * kevent_get_kqwq calls, hence we don't have to care about knotes referencing
 * this kqueue, either there are none, or someone else took care of them.
 */
void
kqworkq_dealloc(struct kqworkq *kqwq)
{
	kqueue_destroy(kqwq, kqworkq_zone);
}

/*!
 * @function kqworkq_alloc
 *
 * @brief
 * Allocates a workqueue kqueue.
 *
 * @discussion
 * This is the slow path of kevent_get_kqwq.
 * This takes care of making sure procs have a single workq kqueue.
 */
OS_NOINLINE
static struct kqworkq *
kqworkq_alloc(struct proc *p, unsigned int flags)
{
	struct kqworkq *kqwq, *tmp;

	kqwq = zalloc_flags(kqworkq_zone, Z_WAITOK | Z_ZERO);

	assert((flags & KEVENT_FLAG_LEGACY32) == 0);
	if (flags & KEVENT_FLAG_LEGACY64) {
		kqwq->kqwq_state = KQ_WORKQ | KQ_KEV64;
	} else {
		kqwq->kqwq_state = KQ_WORKQ | KQ_KEV_QOS;
	}
	kqwq->kqwq_p = p;

	for (int i = 0; i < KQWQ_NBUCKETS; i++) {
		TAILQ_INIT_AFTER_BZERO(&kqwq->kqwq_queue[i]);
		TAILQ_INIT_AFTER_BZERO(&kqwq->kqwq_suppressed[i]);
	}
	for (int i = 0; i < KQWQ_NBUCKETS; i++) {
		/*
		 * Because of how the bucketized system works, we mix overcommit
		 * sources with not overcommit: each time we move a knote from
		 * one bucket to the next due to overrides, we'd had to track
		 * overcommitness, and it's really not worth it in the workloop
		 * enabled world that track this faithfully.
		 *
		 * Incidentally, this behaves like the original manager-based
		 * kqwq where event delivery always happened (hence is
		 * "overcommit")
		 */
		kqwq->kqwq_request[i].tr_state = WORKQ_TR_STATE_IDLE;
		kqwq->kqwq_request[i].tr_flags = WORKQ_TR_FLAG_KEVENT;
		if (i != KQWQ_QOS_MANAGER) {
			kqwq->kqwq_request[i].tr_flags |= WORKQ_TR_FLAG_OVERCOMMIT;
		}
		kqwq->kqwq_request[i].tr_kq_qos_index = (kq_index_t)i + 1;
	}

	kqueue_init(kqwq);

	if (!os_atomic_cmpxchgv(&p->p_fd.fd_wqkqueue, NULL, kqwq, &tmp, release)) {
		kqworkq_dealloc(kqwq);
		return tmp;
	}

	return kqwq;
}

#pragma mark kqworkloop allocation and deallocation

#define KQ_HASH(val, mask)  (((val) ^ (val >> 8)) & (mask))
#define CONFIG_KQ_HASHSIZE  CONFIG_KN_HASHSIZE

OS_ALWAYS_INLINE
static inline void
kqhash_lock(struct filedesc *fdp)
{
	lck_mtx_lock_spin_always(&fdp->fd_kqhashlock);
}

OS_ALWAYS_INLINE
static inline void
kqhash_unlock(struct filedesc *fdp)
{
	lck_mtx_unlock(&fdp->fd_kqhashlock);
}

OS_ALWAYS_INLINE
static inline void
kqworkloop_hash_insert_locked(struct filedesc *fdp, kqueue_id_t id,
    struct kqworkloop *kqwl)
{
	struct kqwllist *list = &fdp->fd_kqhash[KQ_HASH(id, fdp->fd_kqhashmask)];
	LIST_INSERT_HEAD(list, kqwl, kqwl_hashlink);
}

OS_ALWAYS_INLINE
static inline struct kqworkloop *
kqworkloop_hash_lookup_locked(struct filedesc *fdp, kqueue_id_t id)
{
	struct kqwllist *list = &fdp->fd_kqhash[KQ_HASH(id, fdp->fd_kqhashmask)];
	struct kqworkloop *kqwl;

	LIST_FOREACH(kqwl, list, kqwl_hashlink) {
		if (kqwl->kqwl_dynamicid == id) {
			return kqwl;
		}
	}
	return NULL;
}

static struct kqworkloop *
kqworkloop_hash_lookup_and_retain(struct filedesc *fdp, kqueue_id_t kq_id)
{
	struct kqworkloop *kqwl = NULL;

	kqhash_lock(fdp);
	if (__probable(fdp->fd_kqhash)) {
		kqwl = kqworkloop_hash_lookup_locked(fdp, kq_id);
		if (kqwl && !kqworkloop_try_retain(kqwl)) {
			kqwl = NULL;
		}
	}
	kqhash_unlock(fdp);
	return kqwl;
}

OS_NOINLINE
static void
kqworkloop_hash_init(struct filedesc *fdp)
{
	struct kqwllist *alloc_hash;
	u_long alloc_mask;

	kqhash_unlock(fdp);
	alloc_hash = hashinit(CONFIG_KQ_HASHSIZE, M_KQUEUE, &alloc_mask);
	kqhash_lock(fdp);

	/* See if we won the race */
	if (__probable(fdp->fd_kqhashmask == 0)) {
		fdp->fd_kqhash = alloc_hash;
		fdp->fd_kqhashmask = alloc_mask;
	} else {
		kqhash_unlock(fdp);
		hashdestroy(alloc_hash, M_KQUEUE, alloc_mask);
		kqhash_lock(fdp);
	}
}

/*
 * kqueue iotier override is only supported for kqueue that has
 * only one port as a mach port source. Updating the iotier
 * override on the mach port source will update the override
 * on kqueue as well. Since kqueue with iotier override will
 * only have one port attached, there is no logic for saturation
 * like qos override, the iotier override of mach port source
 * would be reflected in kevent iotier override.
 */
void
kqueue_set_iotier_override(kqueue_t kqu, uint8_t iotier_override)
{
	if (!(kqu.kq->kq_state & KQ_WORKLOOP)) {
		return;
	}

	struct kqworkloop *kqwl = kqu.kqwl;
	os_atomic_store(&kqwl->kqwl_iotier_override, iotier_override, relaxed);
}

uint8_t
kqueue_get_iotier_override(kqueue_t kqu)
{
	if (!(kqu.kq->kq_state & KQ_WORKLOOP)) {
		return THROTTLE_LEVEL_END;
	}

	struct kqworkloop *kqwl = kqu.kqwl;
	return os_atomic_load(&kqwl->kqwl_iotier_override, relaxed);
}

#if CONFIG_PREADOPT_TG
/*
 * This function is called with a borrowed reference on the thread group without
 * kq lock held with the mqueue lock held. It may or may not have the knote lock
 * (called from both fevent as well as fattach/ftouch). Upon success, an
 * additional reference on the TG is taken
 */
void
kqueue_set_preadopted_thread_group(kqueue_t kqu, struct thread_group *tg, thread_qos_t qos)
{
	if (!(kqu.kq->kq_state & KQ_WORKLOOP)) {
		KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_THREAD_GROUP, MACH_THREAD_GROUP_PREADOPT_NA),
		    (uintptr_t)thread_tid(current_thread()), 0, 0, 0);
		return;
	}

	struct kqworkloop *kqwl = kqu.kqwl;

	assert(qos < THREAD_QOS_LAST);

	thread_group_retain(tg);

	thread_group_qos_t old_tg; thread_group_qos_t new_tg;
	int ret = os_atomic_rmw_loop(&kqwl->kqwl_preadopt_tg, old_tg, new_tg, relaxed, {
		if (!KQWL_CAN_ADOPT_PREADOPT_TG(old_tg)) {
		        os_atomic_rmw_loop_give_up(break);
		}

		if (old_tg != KQWL_PREADOPTED_TG_NULL) {
		        /*
		         * Note that old_tg could be a NULL TG pointer but with a QoS
		         * set. See also workq_thread_reset_pri.
		         *
		         * Compare the QoS of existing preadopted tg with new one and
		         * only overwrite the thread group if we have one with a higher
		         * QoS.
		         */
		        thread_qos_t existing_qos = KQWL_GET_PREADOPTED_TG_QOS(old_tg);
		        if (existing_qos >= qos) {
		                os_atomic_rmw_loop_give_up(break);
			}
		}

		// Transfer the ref taken earlier in the function to the kqwl
		new_tg = KQWL_ENCODE_PREADOPTED_TG_QOS(tg, qos);
	});

	if (ret) {
		KQWL_PREADOPT_TG_HISTORY_WRITE_ENTRY(kqwl, KQWL_PREADOPT_OP_INCOMING_IPC, old_tg, tg);

		if (KQWL_HAS_VALID_PREADOPTED_TG(old_tg)) {
			thread_group_deallocate_safe(KQWL_GET_PREADOPTED_TG(old_tg));
		}

		os_atomic_store(&kqwl->kqwl_preadopt_tg_needs_redrive, KQWL_PREADOPT_TG_NEEDS_REDRIVE, release);
	} else {
		// We failed to write to the kqwl_preadopt_tg, drop the ref we took
		// earlier in the function
		thread_group_deallocate_safe(tg);
	}
}

/*
 * Called from fprocess of EVFILT_MACHPORT without the kqueue lock held.
 */
bool
kqueue_process_preadopt_thread_group(thread_t thread, struct kqueue *kq, struct thread_group *tg)
{
	bool success = false;
	if (kq->kq_state & KQ_WORKLOOP) {
		struct kqworkloop *kqwl = (struct kqworkloop *) kq;
		thread_group_qos_t old_tg;
		success = os_atomic_cmpxchgv(&kqwl->kqwl_preadopt_tg,
		    KQWL_PREADOPTED_TG_SENTINEL, KQWL_PREADOPTED_TG_PROCESSED,
		    &old_tg, relaxed);
		if (success) {
			thread_set_preadopt_thread_group(thread, tg);
		} else if (KQWL_HAS_PERMANENT_PREADOPTED_TG(old_tg)) {
			/*
			 * Technically the following set_preadopt should be a no-op since this
			 * servicer thread preadopts kqwl's permanent tg at bind time.
			 * See kqueue_threadreq_bind.
			 */
			thread_set_preadopt_thread_group(thread, KQWL_GET_PREADOPTED_TG(old_tg));
		} else {
			assert(old_tg == KQWL_PREADOPTED_TG_PROCESSED ||
			    old_tg == KQWL_PREADOPTED_TG_NEVER);
		}
	}
	return success;
}
#endif

/*!
 * @function kqworkloop_dealloc
 *
 * @brief
 * Deallocates a workloop kqueue.
 *
 * @discussion
 * Knotes hold references on the workloop, so we can't really reach this
 * function unless all of these are already gone.
 *
 * Nothing locked on entry or exit.
 *
 * @param hash_remove
 * Whether to remove the workloop from its hash table.
 */
static void
kqworkloop_dealloc(struct kqworkloop *kqwl, bool hash_remove)
{
	thread_t cur_owner;

	cur_owner = kqwl->kqwl_owner;
	if (cur_owner) {
		if (kqworkloop_override(kqwl) != THREAD_QOS_UNSPECIFIED) {
			thread_drop_kevent_override(cur_owner);
		}
		thread_deallocate(cur_owner);
		kqwl->kqwl_owner = THREAD_NULL;
	}

	if (kqwl->kqwl_state & KQ_HAS_TURNSTILE) {
		struct turnstile *ts;
		turnstile_complete((uintptr_t)kqwl, &kqwl->kqwl_turnstile,
		    &ts, TURNSTILE_WORKLOOPS);
		turnstile_cleanup();
		turnstile_deallocate(ts);
	}

	if (hash_remove) {
		struct filedesc *fdp = &kqwl->kqwl_p->p_fd;

		kqhash_lock(fdp);
		LIST_REMOVE(kqwl, kqwl_hashlink);
#if CONFIG_PROC_RESOURCE_LIMITS
		fdp->num_kqwls--;
#endif
		kqhash_unlock(fdp);
	}

#if CONFIG_PREADOPT_TG
	thread_group_qos_t tg = os_atomic_load(&kqwl->kqwl_preadopt_tg, relaxed);
	if (KQWL_HAS_VALID_PREADOPTED_TG(tg)) {
		thread_group_release(KQWL_GET_PREADOPTED_TG(tg));
	}
#endif

	assert(TAILQ_EMPTY(&kqwl->kqwl_suppressed));
	assert(kqwl->kqwl_owner == THREAD_NULL);
	assert(kqwl->kqwl_turnstile == TURNSTILE_NULL);

	lck_spin_destroy(&kqwl->kqwl_statelock, &kq_lck_grp);
	kqueue_destroy(kqwl, kqworkloop_zone);
}

/*!
 * @function kqworkloop_init
 *
 * @brief
 * Initializes an allocated kqworkloop.
 */
static void
kqworkloop_init(struct kqworkloop *kqwl, proc_t p,
    kqueue_id_t id, workq_threadreq_param_t *trp
#if CONFIG_PREADOPT_TG
    , struct thread_group *trp_permanent_preadopt_tg
#endif
    )
{
	kqwl->kqwl_state     = KQ_WORKLOOP | KQ_DYNAMIC | KQ_KEV_QOS;
	os_ref_init_raw(&kqwl->kqwl_retains, NULL);
	kqwl->kqwl_dynamicid = id;
	kqwl->kqwl_p         = p;
	if (trp) {
		kqwl->kqwl_params = trp->trp_value;
	}

	workq_tr_flags_t tr_flags = WORKQ_TR_FLAG_WORKLOOP;
	if (trp) {
		if (trp->trp_flags & TRP_PRIORITY) {
			tr_flags |= WORKQ_TR_FLAG_WL_OUTSIDE_QOS;
		}
		if (trp->trp_flags) {
			tr_flags |= WORKQ_TR_FLAG_WL_PARAMS;
		}
	}
	kqwl->kqwl_request.tr_state = WORKQ_TR_STATE_IDLE;
	kqwl->kqwl_request.tr_flags = tr_flags;
	os_atomic_store(&kqwl->kqwl_iotier_override, (uint8_t)THROTTLE_LEVEL_END, relaxed);
#if CONFIG_PREADOPT_TG
	if (trp_permanent_preadopt_tg) {
		/*
		 * This kqwl is permanently configured with a thread group.
		 * By using THREAD_QOS_LAST, we make sure kqueue_set_preadopted_thread_group
		 * has no effect on kqwl_preadopt_tg. At this point, +1 ref on
		 * trp_permanent_preadopt_tg is transferred to the kqwl.
		 */
		thread_group_qos_t kqwl_preadopt_tg;
		kqwl_preadopt_tg = KQWL_ENCODE_PERMANENT_PREADOPTED_TG(trp_permanent_preadopt_tg);
		os_atomic_store(&kqwl->kqwl_preadopt_tg, kqwl_preadopt_tg, relaxed);
	} else if (task_is_app(current_task())) {
		/*
		 * Not a specially preconfigured kqwl so it is open to participate in sync IPC
		 * thread group preadoption; but, apps will never adopt a thread group that
		 * is not their own. This is a gross hack to simulate the post-process that
		 * is done in the voucher subsystem today for thread groups.
		 */
		os_atomic_store(&kqwl->kqwl_preadopt_tg, KQWL_PREADOPTED_TG_NEVER, relaxed);
	}
#endif

	for (int i = 0; i < KQWL_NBUCKETS; i++) {
		TAILQ_INIT_AFTER_BZERO(&kqwl->kqwl_queue[i]);
	}
	TAILQ_INIT_AFTER_BZERO(&kqwl->kqwl_suppressed);

	lck_spin_init(&kqwl->kqwl_statelock, &kq_lck_grp, LCK_ATTR_NULL);

	kqueue_init(kqwl);
}

#if CONFIG_PROC_RESOURCE_LIMITS
void
kqworkloop_check_limit_exceeded(struct filedesc *fdp)
{
	int num_kqwls = fdp->num_kqwls;
	if (!kqwl_above_soft_limit_notified(fdp) && fdp->kqwl_dyn_soft_limit > 0 &&
	    num_kqwls > fdp->kqwl_dyn_soft_limit) {
		kqwl_above_soft_limit_send_notification(fdp);
		act_set_astproc_resource(current_thread());
	} else if (!kqwl_above_hard_limit_notified(fdp) && fdp->kqwl_dyn_hard_limit > 0
	    && num_kqwls > fdp->kqwl_dyn_hard_limit) {
		kqwl_above_hard_limit_send_notification(fdp);
		act_set_astproc_resource(current_thread());
	}
}
#endif

/*!
 * @function kqworkloop_get_or_create
 *
 * @brief
 * Wrapper around kqworkloop_init that handles the uniquing of workloops.
 *
 * @returns
 * 0:      success
 * EINVAL: invalid parameters
 * EEXIST: KEVENT_FLAG_DYNAMIC_KQ_MUST_NOT_EXIST is set and a collision exists.
 * ENOENT: KEVENT_FLAG_DYNAMIC_KQ_MUST_EXIST is set and the entry wasn't found.
 * ENOMEM: allocation failed
 */
static int
kqworkloop_get_or_create(struct proc *p, kqueue_id_t id,
    workq_threadreq_param_t *trp,
#if CONFIG_PREADOPT_TG
    struct thread_group *trp_permanent_preadopt_tg,
#endif
    unsigned int flags, struct kqworkloop **kqwlp)
{
	struct filedesc *fdp = &p->p_fd;
	struct kqworkloop *alloc_kqwl = NULL;
	struct kqworkloop *kqwl = NULL;
	int error = 0;

	assert(!trp || (flags & KEVENT_FLAG_DYNAMIC_KQ_MUST_NOT_EXIST));

	if (id == 0 || id == (kqueue_id_t)-1) {
		return EINVAL;
	}

	for (;;) {
		kqhash_lock(fdp);
		if (__improbable(fdp->fd_kqhash == NULL)) {
			kqworkloop_hash_init(fdp);
		}

		kqwl = kqworkloop_hash_lookup_locked(fdp, id);
		if (kqwl) {
			if (__improbable(flags & KEVENT_FLAG_DYNAMIC_KQ_MUST_NOT_EXIST)) {
				/*
				 * If MUST_NOT_EXIST was passed, even if we would have failed
				 * the try_retain, it could have gone the other way, and
				 * userspace can't tell. Let'em fix their race.
				 */
				error = EEXIST;
				break;
			}

			if (__probable(kqworkloop_try_retain(kqwl))) {
				/*
				 * This is a valid live workloop !
				 */
				*kqwlp = kqwl;
				error = 0;
				break;
			}
		}

		if (__improbable(flags & KEVENT_FLAG_DYNAMIC_KQ_MUST_EXIST)) {
			error = ENOENT;
			break;
		}

		/*
		 * We didn't find what we were looking for.
		 *
		 * If this is the second time we reach this point (alloc_kqwl != NULL),
		 * then we're done.
		 *
		 * If this is the first time we reach this point (alloc_kqwl == NULL),
		 * then try to allocate one without blocking.
		 */
		if (__probable(alloc_kqwl == NULL)) {
			alloc_kqwl = zalloc_flags(kqworkloop_zone, Z_NOWAIT | Z_ZERO);
		}
		if (__probable(alloc_kqwl)) {
#if CONFIG_PROC_RESOURCE_LIMITS
			fdp->num_kqwls++;
			kqworkloop_check_limit_exceeded(fdp);
#endif
			kqworkloop_init(alloc_kqwl, p, id, trp
#if CONFIG_PREADOPT_TG
			    , trp_permanent_preadopt_tg
#endif
			    );
			kqworkloop_hash_insert_locked(fdp, id, alloc_kqwl);
			kqhash_unlock(fdp);
			*kqwlp = alloc_kqwl;
			return 0;
		}

		/*
		 * We have to block to allocate a workloop, drop the lock,
		 * allocate one, but then we need to retry lookups as someone
		 * else could race with us.
		 */
		kqhash_unlock(fdp);

		alloc_kqwl = zalloc_flags(kqworkloop_zone, Z_WAITOK | Z_ZERO);
	}

	kqhash_unlock(fdp);

	if (__improbable(alloc_kqwl)) {
		zfree(kqworkloop_zone, alloc_kqwl);
	}

	return error;
}

#pragma mark - knotes

static int
filt_no_attach(struct knote *kn, __unused struct kevent_qos_s *kev)
{
	knote_set_error(kn, ENOTSUP);
	return 0;
}

static void
filt_no_detach(__unused struct knote *kn)
{
}

static int __dead2
filt_bad_event(struct knote *kn, long hint)
{
	panic("%s[%d](%p, %ld)", __func__, kn->kn_filter, kn, hint);
}

static int __dead2
filt_bad_touch(struct knote *kn, struct kevent_qos_s *kev)
{
	panic("%s[%d](%p, %p)", __func__, kn->kn_filter, kn, kev);
}

static int __dead2
filt_bad_process(struct knote *kn, struct kevent_qos_s *kev)
{
	panic("%s[%d](%p, %p)", __func__, kn->kn_filter, kn, kev);
}

/*
 * knotes_dealloc - detach all knotes for the process and drop them
 *
 *		Process is in such a state that it will not try to allocate
 *		any more knotes during this process (stopped for exit or exec).
 */
void
knotes_dealloc(proc_t p)
{
	struct filedesc *fdp = &p->p_fd;
	struct kqueue *kq;
	struct knote *kn;
	struct  klist *kn_hash = NULL;
	u_long kn_hashmask;
	int i;

	proc_fdlock(p);

	/* Close all the fd-indexed knotes up front */
	if (fdp->fd_knlistsize > 0) {
		for (i = 0; i < fdp->fd_knlistsize; i++) {
			while ((kn = SLIST_FIRST(&fdp->fd_knlist[i])) != NULL) {
				kq = knote_get_kq(kn);
				kqlock(kq);
				proc_fdunlock(p);
				knote_drop(kq, kn, NULL);
				proc_fdlock(p);
			}
		}
		/* free the table */
		kfree_type(struct klist, fdp->fd_knlistsize, fdp->fd_knlist);
	}
	fdp->fd_knlistsize = 0;

	proc_fdunlock(p);

	knhash_lock(fdp);

	/* Clean out all the hashed knotes as well */
	if (fdp->fd_knhashmask != 0) {
		for (i = 0; i <= (int)fdp->fd_knhashmask; i++) {
			while ((kn = SLIST_FIRST(&fdp->fd_knhash[i])) != NULL) {
				kq = knote_get_kq(kn);
				kqlock(kq);
				knhash_unlock(fdp);
				knote_drop(kq, kn, NULL);
				knhash_lock(fdp);
			}
		}
		kn_hash = fdp->fd_knhash;
		kn_hashmask = fdp->fd_knhashmask;
		fdp->fd_knhashmask = 0;
		fdp->fd_knhash = NULL;
	}

	knhash_unlock(fdp);

	if (kn_hash) {
		hashdestroy(kn_hash, M_KQUEUE, kn_hashmask);
	}
}

/*
 * kqworkloops_dealloc - rebalance retains on kqworkloops created with
 * scheduling parameters
 *
 * Process is in such a state that it will not try to allocate
 * any more kqs or knotes during this process (stopped for exit or exec).
 */
void
kqworkloops_dealloc(proc_t p)
{
	struct filedesc *fdp = &p->p_fd;
	struct kqworkloop *kqwl, *kqwln;
	struct kqwllist tofree;

	if (!fdt_flag_test(fdp, FD_WORKLOOP)) {
		return;
	}

	kqhash_lock(fdp);

	if (fdp->fd_kqhashmask == 0) {
		kqhash_unlock(fdp);
		return;
	}

	LIST_INIT(&tofree);

	for (size_t i = 0; i <= fdp->fd_kqhashmask; i++) {
		LIST_FOREACH_SAFE(kqwl, &fdp->fd_kqhash[i], kqwl_hashlink, kqwln) {
#if CONFIG_PREADOPT_TG
			/*
			 * kqworkloops that have scheduling parameters have an
			 * implicit retain from kqueue_workloop_ctl that needs
			 * to be balanced on process exit.
			 */
			__assert_only thread_group_qos_t preadopt_tg;
			preadopt_tg = os_atomic_load(&kqwl->kqwl_preadopt_tg, relaxed);
#endif
			assert(kqwl->kqwl_params
#if CONFIG_PREADOPT_TG
			    || KQWL_HAS_PERMANENT_PREADOPTED_TG(preadopt_tg)
#endif
			    );

			LIST_REMOVE(kqwl, kqwl_hashlink);
			LIST_INSERT_HEAD(&tofree, kqwl, kqwl_hashlink);
		}
	}
#if CONFIG_PROC_RESOURCE_LIMITS
	fdp->num_kqwls = 0;
#endif
	kqhash_unlock(fdp);

	LIST_FOREACH_SAFE(kqwl, &tofree, kqwl_hashlink, kqwln) {
		uint32_t ref = os_ref_get_count_raw(&kqwl->kqwl_retains);
		if (ref != 1) {
			panic("kq(%p) invalid refcount %d", kqwl, ref);
		}
		kqworkloop_dealloc(kqwl, false);
	}
}

static int
kevent_register_validate_priority(struct kqueue *kq, struct knote *kn,
    struct kevent_qos_s *kev)
{
	/* We don't care about the priority of a disabled or deleted knote */
	if (kev->flags & (EV_DISABLE | EV_DELETE)) {
		return 0;
	}

	if (kq->kq_state & KQ_WORKLOOP) {
		/*
		 * Workloops need valid priorities with a QOS (excluding manager) for
		 * any enabled knote.
		 *
		 * When it is pre-existing, just make sure it has a valid QoS as
		 * kevent_register() will not use the incoming priority (filters who do
		 * have the responsibility to validate it again, see filt_wltouch).
		 *
		 * If the knote is being made, validate the incoming priority.
		 */
		if (!_pthread_priority_thread_qos(kn ? kn->kn_qos : kev->qos)) {
			return ERANGE;
		}
	}

	return 0;
}

/*
 * Prepare a filter for waiting after register.
 *
 * The f_post_register_wait hook will be called later by kevent_register()
 * and should call kevent_register_wait_block()
 */
static int
kevent_register_wait_prepare(struct knote *kn, struct kevent_qos_s *kev, int rc)
{
	thread_t thread = current_thread();

	assert(knote_fops(kn)->f_extended_codes);

	if (kn->kn_thread == NULL) {
		thread_reference(thread);
		kn->kn_thread = thread;
	} else if (kn->kn_thread != thread) {
		/*
		 * kn_thread may be set from a previous aborted wait
		 * However, it has to be from the same thread.
		 */
		kev->flags |= EV_ERROR;
		kev->data = EXDEV;
		return 0;
	}

	return FILTER_REGISTER_WAIT | rc;
}

/*
 * Cleanup a kevent_register_wait_prepare() effect for threads that have been
 * aborted instead of properly woken up with thread_wakeup_thread().
 */
static void
kevent_register_wait_cleanup(struct knote *kn)
{
	thread_t thread = kn->kn_thread;
	kn->kn_thread = NULL;
	thread_deallocate(thread);
}

/*
 * Must be called at the end of a f_post_register_wait call from a filter.
 */
static void
kevent_register_wait_block(struct turnstile *ts, thread_t thread,
    thread_continue_t cont, struct _kevent_register *cont_args)
{
	turnstile_update_inheritor_complete(ts, TURNSTILE_INTERLOCK_HELD);
	kqunlock(cont_args->kqwl);
	cont_args->handoff_thread = thread;
	thread_handoff_parameter(thread, cont, cont_args, THREAD_HANDOFF_NONE);
}

/*
 * Called by Filters using a f_post_register_wait to return from their wait.
 */
static void
kevent_register_wait_return(struct _kevent_register *cont_args)
{
	struct kqworkloop *kqwl = cont_args->kqwl;
	struct kevent_qos_s *kev = &cont_args->kev;
	int error = 0;

	if (cont_args->handoff_thread) {
		thread_deallocate(cont_args->handoff_thread);
	}

	if (kev->flags & (EV_ERROR | EV_RECEIPT)) {
		if ((kev->flags & EV_ERROR) == 0) {
			kev->flags |= EV_ERROR;
			kev->data = 0;
		}
		error = kevent_modern_copyout(kev, &cont_args->ueventlist);
		if (error == 0) {
			cont_args->eventout++;
		}
	}

	kqworkloop_release(kqwl);
	if (error == 0) {
		*(int32_t *)&current_uthread()->uu_rval = cont_args->eventout;
	}
	unix_syscall_return(error);
}

/*
 * kevent_register - add a new event to a kqueue
 *
 *	Creates a mapping between the event source and
 *	the kqueue via a knote data structure.
 *
 *	Because many/most the event sources are file
 *	descriptor related, the knote is linked off
 *	the filedescriptor table for quick access.
 *
 *	called with nothing locked
 *	caller holds a reference on the kqueue
 */

int
kevent_register(struct kqueue *kq, struct kevent_qos_s *kev,
    struct knote **kn_out)
{
	struct proc *p = kq->kq_p;
	const struct filterops *fops;
	struct knote *kn = NULL;
	int result = 0, error = 0;
	unsigned short kev_flags = kev->flags;
	KNOTE_LOCK_CTX(knlc);

	if (__probable(kev->filter < 0 && kev->filter + EVFILT_SYSCOUNT >= 0)) {
		fops = sysfilt_ops[~kev->filter];       /* to 0-base index */
	} else {
		error = EINVAL;
		goto out;
	}

	/* restrict EV_VANISHED to adding udata-specific dispatch kevents */
	if (__improbable((kev->flags & EV_VANISHED) &&
	    (kev->flags & (EV_ADD | EV_DISPATCH2)) != (EV_ADD | EV_DISPATCH2))) {
		error = EINVAL;
		goto out;
	}

	/* Simplify the flags - delete and disable overrule */
	if (kev->flags & EV_DELETE) {
		kev->flags &= ~EV_ADD;
	}
	if (kev->flags & EV_DISABLE) {
		kev->flags &= ~EV_ENABLE;
	}

	if (kq->kq_state & KQ_WORKLOOP) {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_REGISTER),
		    ((struct kqworkloop *)kq)->kqwl_dynamicid,
		    kev->udata, kev->flags, kev->filter);
	} else if (kq->kq_state & KQ_WORKQ) {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWQ_REGISTER),
		    0, kev->udata, kev->flags, kev->filter);
	} else {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQ_REGISTER),
		    VM_KERNEL_UNSLIDE_OR_PERM(kq),
		    kev->udata, kev->flags, kev->filter);
	}

restart:
	/* find the matching knote from the fd tables/hashes */
	kn = kq_find_knote_and_kq_lock(kq, kev, fops->f_isfd, p);
	error = kevent_register_validate_priority(kq, kn, kev);
	result = 0;
	if (error) {
		if (kn) {
			kqunlock(kq);
		}
		goto out;
	}

	if (kn == NULL && (kev->flags & EV_ADD) == 0) {
		/*
		 * No knote found, EV_ADD wasn't specified
		 */

		if ((kev_flags & EV_ADD) && (kev_flags & EV_DELETE) &&
		    (kq->kq_state & KQ_WORKLOOP)) {
			/*
			 * For workloops, understand EV_ADD|EV_DELETE as a "soft" delete
			 * that doesn't care about ENOENT, so just pretend the deletion
			 * happened.
			 */
		} else {
			error = ENOENT;
		}
		goto out;
	} else if (kn == NULL) {
		/*
		 * No knote found, need to attach a new one (attach)
		 */

		struct fileproc *knote_fp = NULL;

		/* grab a file reference for the new knote */
		if (fops->f_isfd) {
			if ((error = fp_lookup(p, (int)kev->ident, &knote_fp, 0)) != 0) {
				goto out;
			}
		}

		kn = knote_alloc();
		kn->kn_fp = knote_fp;
		kn->kn_is_fd = fops->f_isfd;
		kn->kn_kq_packed = VM_PACK_POINTER((vm_offset_t)kq, KNOTE_KQ_PACKED);
		kn->kn_status = 0;

		/* was vanish support requested */
		if (kev->flags & EV_VANISHED) {
			kev->flags &= ~EV_VANISHED;
			kn->kn_status |= KN_REQVANISH;
		}

		/* snapshot matching/dispatching protocol flags into knote */
		if (kev->flags & EV_DISABLE) {
			kn->kn_status |= KN_DISABLED;
		}

		/*
		 * copy the kevent state into knote
		 * protocol is that fflags and data
		 * are saved off, and cleared before
		 * calling the attach routine.
		 *
		 * - kn->kn_sfflags aliases with kev->xflags
		 * - kn->kn_sdata   aliases with kev->data
		 * - kn->kn_filter  is the top 8 bits of kev->filter
		 */
		kn->kn_kevent  = *(struct kevent_internal_s *)kev;
		kn->kn_sfflags = kev->fflags;
		kn->kn_filtid  = (uint8_t)~kev->filter;
		kn->kn_fflags  = 0;
		knote_reset_priority(kq, kn, kev->qos);

		/* Add the knote for lookup thru the fd table */
		error = kq_add_knote(kq, kn, &knlc, p);
		if (error) {
			knote_free(kn);
			if (knote_fp != NULL) {
				fp_drop(p, (int)kev->ident, knote_fp, 0);
			}

			if (error == ERESTART) {
				goto restart;
			}
			goto out;
		}

		/* fp reference count now applies to knote */

		/*
		 * we can't use filter_call() because f_attach can change the filter ops
		 * for a filter that supports f_extended_codes, so we need to reload
		 * knote_fops() and not use `fops`.
		 */
		result = fops->f_attach(kn, kev);
		if (result && !knote_fops(kn)->f_extended_codes) {
			result = FILTER_ACTIVE;
		}

		kqlock(kq);

		if (result & FILTER_THREADREQ_NODEFEER) {
			enable_preemption();
		}

		if (kn->kn_flags & EV_ERROR) {
			/*
			 * Failed to attach correctly, so drop.
			 */
			kn->kn_filtid = EVFILTID_DETACHED;
			error = (int)kn->kn_sdata;
			knote_drop(kq, kn, &knlc);
			result = 0;
			goto out;
		}

		/*
		 * end "attaching" phase - now just attached
		 *
		 * Mark the thread request overcommit, if appropos
		 *
		 * If the attach routine indicated that an
		 * event is already fired, activate the knote.
		 */
		if ((kn->kn_qos & _PTHREAD_PRIORITY_OVERCOMMIT_FLAG) &&
		    (kq->kq_state & KQ_WORKLOOP)) {
			kqworkloop_set_overcommit((struct kqworkloop *)kq);
		}
	} else if (!knote_lock(kq, kn, &knlc, KNOTE_KQ_LOCK_ON_SUCCESS)) {
		/*
		 * The knote was dropped while we were waiting for the lock,
		 * we need to re-evaluate entirely
		 */

		goto restart;
	} else if (kev->flags & EV_DELETE) {
		/*
		 * Deletion of a knote (drop)
		 *
		 * If the filter wants to filter drop events, let it do so.
		 *
		 * defer-delete: when trying to delete a disabled EV_DISPATCH2 knote,
		 * we must wait for the knote to be re-enabled (unless it is being
		 * re-enabled atomically here).
		 */

		if (knote_fops(kn)->f_allow_drop) {
			bool drop;

			kqunlock(kq);
			drop = knote_fops(kn)->f_allow_drop(kn, kev);
			kqlock(kq);

			if (!drop) {
				goto out_unlock;
			}
		}

		if ((kev->flags & EV_ENABLE) == 0 &&
		    (kn->kn_flags & EV_DISPATCH2) == EV_DISPATCH2 &&
		    (kn->kn_status & KN_DISABLED) != 0) {
			kn->kn_status |= KN_DEFERDELETE;
			error = EINPROGRESS;
			goto out_unlock;
		}

		knote_drop(kq, kn, &knlc);
		goto out;
	} else {
		/*
		 * Regular update of a knote (touch)
		 *
		 * Call touch routine to notify filter of changes in filter values
		 * (and to re-determine if any events are fired).
		 *
		 * If the knote is in defer-delete, avoid calling the filter touch
		 * routine (it has delivered its last event already).
		 *
		 * If the touch routine had no failure,
		 * apply the requested side effects to the knote.
		 */

		if (kn->kn_status & (KN_DEFERDELETE | KN_VANISHED)) {
			if (kev->flags & EV_ENABLE) {
				result = FILTER_ACTIVE;
			}
		} else {
			kqunlock(kq);
			result = filter_call(knote_fops(kn), f_touch(kn, kev));
			kqlock(kq);
			if (result & FILTER_THREADREQ_NODEFEER) {
				enable_preemption();
			}
		}

		if (kev->flags & EV_ERROR) {
			result = 0;
			goto out_unlock;
		}

		if ((kn->kn_flags & EV_UDATA_SPECIFIC) == 0 &&
		    kn->kn_udata != kev->udata) {
			// this allows klist_copy_udata() not to take locks
			os_atomic_store_wide(&kn->kn_udata, kev->udata, relaxed);
		}
		if ((kev->flags & EV_DISABLE) && !(kn->kn_status & KN_DISABLED)) {
			kn->kn_status |= KN_DISABLED;
			knote_dequeue(kq, kn);
		}
	}

	/* accept new kevent state */
	knote_apply_touch(kq, kn, kev, result);

out_unlock:
	/*
	 * When the filter asked for a post-register wait,
	 * we leave the kqueue locked for kevent_register()
	 * to call the filter's f_post_register_wait hook.
	 */
	if (result & FILTER_REGISTER_WAIT) {
		knote_unlock(kq, kn, &knlc, KNOTE_KQ_LOCK_ALWAYS);
		*kn_out = kn;
	} else {
		knote_unlock(kq, kn, &knlc, KNOTE_KQ_UNLOCK);
	}

out:
	/* output local errors through the kevent */
	if (error) {
		kev->flags |= EV_ERROR;
		kev->data = error;
	}
	return result;
}

/*
 * knote_process - process a triggered event
 *
 *	Validate that it is really still a triggered event
 *	by calling the filter routines (if necessary).  Hold
 *	a use reference on the knote to avoid it being detached.
 *
 *	If it is still considered triggered, we will have taken
 *	a copy of the state under the filter lock.  We use that
 *	snapshot to dispatch the knote for future processing (or
 *	not, if this was a lost event).
 *
 *	Our caller assures us that nobody else can be processing
 *	events from this knote during the whole operation. But
 *	others can be touching or posting events to the knote
 *	interspersed with our processing it.
 *
 *	caller holds a reference on the kqueue.
 *	kqueue locked on entry and exit - but may be dropped
 */
static int
knote_process(struct knote *kn, kevent_ctx_t kectx,
    kevent_callback_t callback)
{
	struct kevent_qos_s kev;
	struct kqueue *kq = knote_get_kq(kn);
	KNOTE_LOCK_CTX(knlc);
	int result = FILTER_ACTIVE;
	int error = 0;
	bool drop = false;

	/*
	 * Must be active
	 * Must be queued and not disabled/suppressed or dropping
	 */
	assert(kn->kn_status & KN_QUEUED);
	assert(kn->kn_status & KN_ACTIVE);
	assert(!(kn->kn_status & (KN_DISABLED | KN_SUPPRESSED | KN_DROPPING)));

	if (kq->kq_state & KQ_WORKLOOP) {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_PROCESS),
		    ((struct kqworkloop *)kq)->kqwl_dynamicid,
		    kn->kn_udata, kn->kn_status | (kn->kn_id << 32),
		    kn->kn_filtid);
	} else if (kq->kq_state & KQ_WORKQ) {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWQ_PROCESS),
		    0, kn->kn_udata, kn->kn_status | (kn->kn_id << 32),
		    kn->kn_filtid);
	} else {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQ_PROCESS),
		    VM_KERNEL_UNSLIDE_OR_PERM(kq), kn->kn_udata,
		    kn->kn_status | (kn->kn_id << 32), kn->kn_filtid);
	}

	if (!knote_lock(kq, kn, &knlc, KNOTE_KQ_LOCK_ALWAYS)) {
		/*
		 * When the knote is dropping or has dropped,
		 * then there's nothing we want to process.
		 */
		return EJUSTRETURN;
	}

	/*
	 * While waiting for the knote lock, we may have dropped the kq lock.
	 * and a touch may have disabled and dequeued the knote.
	 */
	if (!(kn->kn_status & KN_QUEUED)) {
		knote_unlock(kq, kn, &knlc, KNOTE_KQ_LOCK_ALWAYS);
		return EJUSTRETURN;
	}

	/*
	 * For deferred-drop or vanished events, we just create a fake
	 * event to acknowledge end-of-life.  Otherwise, we call the
	 * filter's process routine to snapshot the kevent state under
	 * the filter's locking protocol.
	 *
	 * suppress knotes to avoid returning the same event multiple times in
	 * a single call.
	 */
	knote_suppress(kq, kn);

	if (kn->kn_status & (KN_DEFERDELETE | KN_VANISHED)) {
		uint16_t kev_flags = EV_DISPATCH2 | EV_ONESHOT;
		if (kn->kn_status & KN_DEFERDELETE) {
			kev_flags |= EV_DELETE;
		} else {
			kev_flags |= EV_VANISHED;
		}

		/* create fake event */
		kev = (struct kevent_qos_s){
			.filter = kn->kn_filter,
			.ident  = kn->kn_id,
			.flags  = kev_flags,
			.udata  = kn->kn_udata,
		};
	} else {
		kqunlock(kq);
		kev = (struct kevent_qos_s) { };
		result = filter_call(knote_fops(kn), f_process(kn, &kev));
		kqlock(kq);
	}

	/*
	 * Determine how to dispatch the knote for future event handling.
	 * not-fired: just return (do not callout, leave deactivated).
	 * One-shot:  If dispatch2, enter deferred-delete mode (unless this is
	 *            is the deferred delete event delivery itself).  Otherwise,
	 *            drop it.
	 * Dispatch:  don't clear state, just mark it disabled.
	 * Cleared:   just leave it deactivated.
	 * Others:    re-activate as there may be more events to handle.
	 *            This will not wake up more handlers right now, but
	 *            at the completion of handling events it may trigger
	 *            more handler threads (TODO: optimize based on more than
	 *            just this one event being detected by the filter).
	 */
	if ((result & FILTER_ACTIVE) == 0) {
		if ((kn->kn_status & KN_ACTIVE) == 0) {
			/*
			 * Some knotes (like EVFILT_WORKLOOP) can be reactivated from
			 * within f_process() but that doesn't necessarily make them
			 * ready to process, so we should leave them be.
			 *
			 * For other knotes, since we will not return an event,
			 * there's no point keeping the knote suppressed.
			 */
			knote_unsuppress(kq, kn);
		}
		knote_unlock(kq, kn, &knlc, KNOTE_KQ_LOCK_ALWAYS);
		return EJUSTRETURN;
	}

	if (result & FILTER_ADJUST_EVENT_QOS_BIT) {
		knote_adjust_qos(kq, kn, result);
	}

	if (result & FILTER_ADJUST_EVENT_IOTIER_BIT) {
		kqueue_update_iotier_override(kq);
	}

	kev.qos = _pthread_priority_combine(kn->kn_qos, kn->kn_qos_override);

	if (kev.flags & EV_ONESHOT) {
		if ((kn->kn_flags & EV_DISPATCH2) == EV_DISPATCH2 &&
		    (kn->kn_status & KN_DEFERDELETE) == 0) {
			/* defer dropping non-delete oneshot dispatch2 events */
			kn->kn_status |= KN_DEFERDELETE | KN_DISABLED;
		} else {
			drop = true;
		}
	} else if (kn->kn_flags & EV_DISPATCH) {
		/* disable all dispatch knotes */
		kn->kn_status |= KN_DISABLED;
	} else if ((kn->kn_flags & EV_CLEAR) == 0) {
		/* re-activate in case there are more events */
		knote_activate(kq, kn, FILTER_ACTIVE);
	}

	/*
	 * callback to handle each event as we find it.
	 * If we have to detach and drop the knote, do
	 * it while we have the kq unlocked.
	 */
	if (drop) {
		knote_drop(kq, kn, &knlc);
	} else {
		knote_unlock(kq, kn, &knlc, KNOTE_KQ_UNLOCK);
	}

	if (kev.flags & EV_VANISHED) {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KNOTE_VANISHED),
		    kev.ident, kn->kn_udata, kn->kn_status | (kn->kn_id << 32),
		    kn->kn_filtid);
	}

	error = (callback)(&kev, kectx);
	kqlock(kq);
	return error;
}

/*
 * Returns -1 if the kqueue was unbound and processing should not happen
 */
#define KQWQAE_BEGIN_PROCESSING 1
#define KQWQAE_END_PROCESSING   2
#define KQWQAE_UNBIND           3
static int
kqworkq_acknowledge_events(struct kqworkq *kqwq, workq_threadreq_t kqr,
    int kevent_flags, int kqwqae_op)
{
	struct knote *kn;
	int rc = 0;
	bool unbind;
	struct kqtailq *suppressq = &kqwq->kqwq_suppressed[kqr->tr_kq_qos_index - 1];
	struct kqtailq *queue = &kqwq->kqwq_queue[kqr->tr_kq_qos_index - 1];

	kqlock_held(&kqwq->kqwq_kqueue);

	/*
	 * Return suppressed knotes to their original state.
	 * For workq kqueues, suppressed ones that are still
	 * truly active (not just forced into the queue) will
	 * set flags we check below to see if anything got
	 * woken up.
	 */
	while ((kn = TAILQ_FIRST(suppressq)) != NULL) {
		knote_unsuppress(kqwq, kn);
	}

	if (kqwqae_op == KQWQAE_UNBIND) {
		unbind = true;
	} else if ((kevent_flags & KEVENT_FLAG_PARKING) == 0) {
		unbind = false;
	} else {
		unbind = TAILQ_EMPTY(queue);
	}
	if (unbind) {
		thread_t thread = kqr_thread_fast(kqr);
		thread_qos_t old_override;

#if DEBUG || DEVELOPMENT
		thread_t self = current_thread();
		struct uthread *ut = get_bsdthread_info(self);

		assert(thread == self);
		assert(ut->uu_kqr_bound == kqr);
#endif // DEBUG || DEVELOPMENT

		old_override = kqworkq_unbind_locked(kqwq, kqr, thread);
		if (!TAILQ_EMPTY(queue)) {
			/*
			 * Request a new thread if we didn't process the whole
			 * queue.
			 */
			kqueue_threadreq_initiate(&kqwq->kqwq_kqueue, kqr,
			    kqr->tr_kq_qos_index, 0);
		}
		if (old_override) {
			thread_drop_kevent_override(thread);
		}
		rc = -1;
	}

	return rc;
}

/*
 * Return 0 to indicate that processing should proceed,
 * -1 if there is nothing to process.
 *
 * Called with kqueue locked and returns the same way,
 * but may drop lock temporarily.
 */
static int
kqworkq_begin_processing(struct kqworkq *kqwq, workq_threadreq_t kqr,
    int kevent_flags)
{
	int rc = 0;

	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWQ_PROCESS_BEGIN) | DBG_FUNC_START,
	    0, kqr->tr_kq_qos_index);

	rc = kqworkq_acknowledge_events(kqwq, kqr, kevent_flags,
	    KQWQAE_BEGIN_PROCESSING);

	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWQ_PROCESS_BEGIN) | DBG_FUNC_END,
	    thread_tid(kqr_thread(kqr)),
	    !TAILQ_EMPTY(&kqwq->kqwq_queue[kqr->tr_kq_qos_index - 1]));

	return rc;
}

static thread_qos_t
kqworkloop_acknowledge_events(struct kqworkloop *kqwl)
{
	kq_index_t qos = THREAD_QOS_UNSPECIFIED;
	struct knote *kn, *tmp;

	kqlock_held(kqwl);

	TAILQ_FOREACH_SAFE(kn, &kqwl->kqwl_suppressed, kn_tqe, tmp) {
		/*
		 * If a knote that can adjust QoS is disabled because of the automatic
		 * behavior of EV_DISPATCH, the knotes should stay suppressed so that
		 * further overrides keep pushing.
		 */
		if (knote_fops(kn)->f_adjusts_qos &&
		    (kn->kn_status & KN_DISABLED) != 0 &&
		    (kn->kn_status & KN_DROPPING) == 0 &&
		    (kn->kn_flags & (EV_DISPATCH | EV_DISABLE)) == EV_DISPATCH) {
			qos = MAX(qos, kn->kn_qos_override);
			continue;
		}
		knote_unsuppress(kqwl, kn);
	}

	return qos;
}

static int
kqworkloop_begin_processing(struct kqworkloop *kqwl, unsigned int kevent_flags)
{
	workq_threadreq_t kqr = &kqwl->kqwl_request;
	struct kqueue *kq = &kqwl->kqwl_kqueue;
	int rc = 0, op = KQWL_UTQ_NONE;

	kqlock_held(kq);

	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_PROCESS_BEGIN) | DBG_FUNC_START,
	    kqwl->kqwl_dynamicid, 0, 0);

	/* nobody else should still be processing */
	assert((kq->kq_state & KQ_PROCESSING) == 0);

	kq->kq_state |= KQ_PROCESSING;

	if (kevent_flags & KEVENT_FLAG_PARKING) {
		/*
		 * When "parking" we want to process events and if no events are found
		 * unbind.
		 *
		 * However, non overcommit threads sometimes park even when they have
		 * more work so that the pool can narrow.  For these, we need to unbind
		 * early, so that calling kqworkloop_update_threads_qos() can ask the
		 * workqueue subsystem whether the thread should park despite having
		 * pending events.
		 */
		if (kqr->tr_flags & WORKQ_TR_FLAG_OVERCOMMIT) {
			op = KQWL_UTQ_PARKING;
		} else {
			op = KQWL_UTQ_UNBINDING;
		}
	} else if (!TAILQ_EMPTY(&kqwl->kqwl_suppressed)) {
		op = KQWL_UTQ_RESET_WAKEUP_OVERRIDE;
	}

	if (op != KQWL_UTQ_NONE) {
		thread_qos_t qos_override;
		thread_t thread = kqr_thread_fast(kqr);

		qos_override = kqworkloop_acknowledge_events(kqwl);

		if (op == KQWL_UTQ_UNBINDING) {
			kqworkloop_unbind_locked(kqwl, thread,
			    KQWL_OVERRIDE_DROP_IMMEDIATELY);
			kqworkloop_release_live(kqwl);
		}
		kqworkloop_update_threads_qos(kqwl, op, qos_override);
		if (op == KQWL_UTQ_PARKING &&
		    (!kqwl->kqwl_count || kqwl->kqwl_owner)) {
			kqworkloop_unbind_locked(kqwl, thread,
			    KQWL_OVERRIDE_DROP_DELAYED);
			kqworkloop_release_live(kqwl);
			rc = -1;
		} else if (op == KQWL_UTQ_UNBINDING &&
		    kqr_thread(kqr) != thread) {
			rc = -1;
		}

		if (rc == -1) {
			kq->kq_state &= ~KQ_PROCESSING;
			kqworkloop_unbind_delayed_override_drop(thread);
		}
	}

	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_PROCESS_BEGIN) | DBG_FUNC_END,
	    kqwl->kqwl_dynamicid, 0, 0);

	return rc;
}

/*
 * Return 0 to indicate that processing should proceed,
 * -1 if there is nothing to process.
 * EBADF if the kqueue is draining
 *
 * Called with kqueue locked and returns the same way,
 * but may drop lock temporarily.
 * May block.
 */
static int
kqfile_begin_processing(struct kqfile *kq)
{
	kqlock_held(kq);

	assert((kq->kqf_state & (KQ_WORKQ | KQ_WORKLOOP)) == 0);
	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQ_PROCESS_BEGIN) | DBG_FUNC_START,
	    VM_KERNEL_UNSLIDE_OR_PERM(kq), 0);

	/* wait to become the exclusive processing thread */
	while ((kq->kqf_state & (KQ_PROCESSING | KQ_DRAIN)) == KQ_PROCESSING) {
		kq->kqf_state |= KQ_PROCWAIT;
		lck_spin_sleep(&kq->kqf_lock, LCK_SLEEP_DEFAULT,
		    &kq->kqf_suppressed, THREAD_UNINT | THREAD_WAIT_NOREPORT);
	}

	if (kq->kqf_state & KQ_DRAIN) {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQ_PROCESS_BEGIN) | DBG_FUNC_END,
		    VM_KERNEL_UNSLIDE_OR_PERM(kq), 2);
		return EBADF;
	}

	/* Nobody else processing */

	/* anything left to process? */
	if (kq->kqf_count == 0) {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQ_PROCESS_BEGIN) | DBG_FUNC_END,
		    VM_KERNEL_UNSLIDE_OR_PERM(kq), 1);
		return -1;
	}

	/* convert to processing mode */
	kq->kqf_state |= KQ_PROCESSING;

	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQ_PROCESS_BEGIN) | DBG_FUNC_END,
	    VM_KERNEL_UNSLIDE_OR_PERM(kq), 0);
	return 0;
}

/*
 * Try to end the processing, only called when a workq thread is attempting to
 * park (KEVENT_FLAG_PARKING is set).
 *
 * When returning -1, the kqworkq is setup again so that it is ready to be
 * processed.
 */
static int
kqworkq_end_processing(struct kqworkq *kqwq, workq_threadreq_t kqr,
    int kevent_flags)
{
	if (kevent_flags & KEVENT_FLAG_PARKING) {
		/*
		 * if acknowledge events "succeeds" it means there are events,
		 * which is a failure condition for end_processing.
		 */
		int rc = kqworkq_acknowledge_events(kqwq, kqr, kevent_flags,
		    KQWQAE_END_PROCESSING);
		if (rc == 0) {
			return -1;
		}
	}

	return 0;
}

/*
 * Try to end the processing, only called when a workq thread is attempting to
 * park (KEVENT_FLAG_PARKING is set).
 *
 * When returning -1, the kqworkq is setup again so that it is ready to be
 * processed (as if kqworkloop_begin_processing had just been called).
 *
 * If successful and KEVENT_FLAG_PARKING was set in the kevent_flags,
 * the kqworkloop is unbound from its servicer as a side effect.
 */
static int
kqworkloop_end_processing(struct kqworkloop *kqwl, int flags, int kevent_flags)
{
	struct kqueue *kq = &kqwl->kqwl_kqueue;
	workq_threadreq_t kqr = &kqwl->kqwl_request;
	int rc = 0;

	kqlock_held(kq);

	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_PROCESS_END) | DBG_FUNC_START,
	    kqwl->kqwl_dynamicid, 0, 0);

	if (kevent_flags & KEVENT_FLAG_PARKING) {
		thread_t thread = kqr_thread_fast(kqr);
		thread_qos_t qos_override;

		/*
		 * When KEVENT_FLAG_PARKING is set, we need to attempt
		 * an unbind while still under the lock.
		 *
		 * So we do everything kqworkloop_unbind() would do, but because
		 * we're inside kqueue_process(), if the workloop actually
		 * received events while our locks were dropped, we have
		 * the opportunity to fail the end processing and loop again.
		 *
		 * This avoids going through the process-wide workqueue lock
		 * hence scales better.
		 */
		assert(flags & KQ_PROCESSING);
		qos_override = kqworkloop_acknowledge_events(kqwl);
		kqworkloop_update_threads_qos(kqwl, KQWL_UTQ_PARKING, qos_override);

		if (kqwl->kqwl_wakeup_qos && !kqwl->kqwl_owner) {
			rc = -1;
		} else {
			kqworkloop_unbind_locked(kqwl, thread, KQWL_OVERRIDE_DROP_DELAYED);
			kqworkloop_release_live(kqwl);
			kq->kq_state &= ~flags;
			kqworkloop_unbind_delayed_override_drop(thread);
		}
	} else {
		kq->kq_state &= ~flags;
		kq->kq_state |= KQ_R2K_ARMED;
		kqworkloop_update_threads_qos(kqwl, KQWL_UTQ_RECOMPUTE_WAKEUP_QOS, 0);
	}

	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_PROCESS_END) | DBG_FUNC_END,
	    kqwl->kqwl_dynamicid, 0, 0);

	return rc;
}

/*
 * Called with kqueue lock held.
 *
 * 0: no more events
 * -1: has more events
 * EBADF: kqueue is in draining mode
 */
static int
kqfile_end_processing(struct kqfile *kq)
{
	struct knote *kn;
	int procwait;

	kqlock_held(kq);

	assert((kq->kqf_state & (KQ_WORKQ | KQ_WORKLOOP)) == 0);

	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQ_PROCESS_END),
	    VM_KERNEL_UNSLIDE_OR_PERM(kq), 0);

	/*
	 * Return suppressed knotes to their original state.
	 */
	while ((kn = TAILQ_FIRST(&kq->kqf_suppressed)) != NULL) {
		knote_unsuppress(kq, kn);
	}

	procwait = (kq->kqf_state & KQ_PROCWAIT);
	kq->kqf_state &= ~(KQ_PROCESSING | KQ_PROCWAIT);

	if (procwait) {
		/* first wake up any thread already waiting to process */
		thread_wakeup(&kq->kqf_suppressed);
	}

	if (kq->kqf_state & KQ_DRAIN) {
		return EBADF;
	}
	return kq->kqf_count != 0 ? -1 : 0;
}

static int
kqueue_workloop_ctl_internal(proc_t p, uintptr_t cmd, uint64_t __unused options,
    struct kqueue_workloop_params *params, int *retval)
{
	int error = 0;
	struct kqworkloop *kqwl;
	struct filedesc *fdp = &p->p_fd;
	workq_threadreq_param_t trp = { };
#if CONFIG_PREADOPT_TG
	struct thread_group *trp_permanent_preadopt_tg = NULL;
	integer_t trp_preadopt_priority = 0;
	integer_t trp_preadopt_policy = 0;
#endif /* CONFIG_PREADOPT_TG */

	switch (cmd) {
	case KQ_WORKLOOP_CREATE:
		if (!params->kqwlp_flags) {
			error = EINVAL;
			break;
		}

		if ((params->kqwlp_flags & KQ_WORKLOOP_CREATE_SCHED_PRI) &&
		    (params->kqwlp_sched_pri < 1 ||
		    params->kqwlp_sched_pri > 63 /* MAXPRI_USER */)) {
			error = EINVAL;
			break;
		}

		if ((params->kqwlp_flags & KQ_WORKLOOP_CREATE_SCHED_POL) &&
		    invalid_policy(params->kqwlp_sched_pol)) {
			error = EINVAL;
			break;
		}

		if ((params->kqwlp_flags & KQ_WORKLOOP_CREATE_CPU_PERCENT) &&
		    (params->kqwlp_cpu_percent <= 0 ||
		    params->kqwlp_cpu_percent > 100 ||
		    params->kqwlp_cpu_refillms <= 0 ||
		    params->kqwlp_cpu_refillms > 0x00ffffff)) {
			error = EINVAL;
			break;
		}

		if (params->kqwlp_flags & KQ_WORKLOOP_CREATE_WORK_INTERVAL) {
#if CONFIG_PREADOPT_TG
			kern_return_t kr;
			kr = kern_work_interval_get_policy_from_port(params->kqwl_wi_port,
			    &trp_preadopt_policy,
			    &trp_preadopt_priority,
			    &trp_permanent_preadopt_tg);
			if (kr != KERN_SUCCESS) {
				error = EINVAL;
				break;
			}
			/* The work interval comes with scheduling policy. */
			if (trp_preadopt_policy) {
				trp.trp_flags |= TRP_POLICY;
				trp.trp_pol = (uint8_t)trp_preadopt_policy;

				trp.trp_flags |= TRP_PRIORITY;
				trp.trp_pri = (uint8_t)trp_preadopt_priority;
			}
			/*
			 * We take +1 ref on a thread group backing this work interval
			 * via kern_work_interval_get_policy_from_port and pass it on to kqwl.
			 * If, for whatever reasons, kqworkloop_get_or_create fails, we
			 * get back this ref.
			 */
#else
			error = ENOTSUP;
			break;
#endif /* CONFIG_PREADOPT_TG */
		}

		if (!(trp.trp_flags & (TRP_POLICY | TRP_PRIORITY))) {
			/*
			 * We always prefer scheduling policy + priority that comes with
			 * a work interval. It it does not exist, we fallback to what the user
			 * has asked.
			 */
			if (params->kqwlp_flags & KQ_WORKLOOP_CREATE_SCHED_PRI) {
				trp.trp_flags |= TRP_PRIORITY;
				trp.trp_pri = (uint8_t)params->kqwlp_sched_pri;
			}
			if (params->kqwlp_flags & KQ_WORKLOOP_CREATE_SCHED_POL) {
				trp.trp_flags |= TRP_POLICY;
				trp.trp_pol = (uint8_t)params->kqwlp_sched_pol;
			}
			if (params->kqwlp_flags & KQ_WORKLOOP_CREATE_CPU_PERCENT) {
				trp.trp_flags |= TRP_CPUPERCENT;
				trp.trp_cpupercent = (uint8_t)params->kqwlp_cpu_percent;
				trp.trp_refillms = params->kqwlp_cpu_refillms;
			}
		}

		error = kqworkloop_get_or_create(p, params->kqwlp_id, &trp,
#if CONFIG_PREADOPT_TG
		    trp_permanent_preadopt_tg,
#endif /* CONFIG_PREADOPT_TG */
		    KEVENT_FLAG_DYNAMIC_KQUEUE | KEVENT_FLAG_WORKLOOP |
		    KEVENT_FLAG_DYNAMIC_KQ_MUST_NOT_EXIST, &kqwl);
		if (error) {
#if CONFIG_PREADOPT_TG
			/* In case of success, kqwl consumes this +1 ref. */
			if (trp_permanent_preadopt_tg) {
				thread_group_release(trp_permanent_preadopt_tg);
			}
#endif
			break;
		}

		if (!fdt_flag_test(fdp, FD_WORKLOOP)) {
			/* FD_WORKLOOP indicates we've ever created a workloop
			 * via this syscall but its only ever added to a process, never
			 * removed.
			 */
			proc_fdlock(p);
			fdt_flag_set(fdp, FD_WORKLOOP);
			proc_fdunlock(p);
		}
		break;
	case KQ_WORKLOOP_DESTROY:
		error = kqworkloop_get_or_create(p, params->kqwlp_id, NULL,
#if CONFIG_PREADOPT_TG
		    NULL,
#endif /* CONFIG_PREADOPT_TG */
		    KEVENT_FLAG_DYNAMIC_KQUEUE | KEVENT_FLAG_WORKLOOP |
		    KEVENT_FLAG_DYNAMIC_KQ_MUST_EXIST, &kqwl);
		if (error) {
			break;
		}
		kqlock(kqwl);
		trp.trp_value = kqwl->kqwl_params;
		if (trp.trp_flags && !(trp.trp_flags & TRP_RELEASED)) {
			trp.trp_flags |= TRP_RELEASED;
			kqwl->kqwl_params = trp.trp_value;
			kqworkloop_release_live(kqwl);
		} else {
			error = EINVAL;
		}
		kqunlock(kqwl);
		kqworkloop_release(kqwl);
		break;
	}
	*retval = 0;
	return error;
}

int
kqueue_workloop_ctl(proc_t p, struct kqueue_workloop_ctl_args *uap, int *retval)
{
	struct kqueue_workloop_params params = {
		.kqwlp_id = 0,
	};
	if (uap->sz < sizeof(params.kqwlp_version)) {
		return EINVAL;
	}

	size_t copyin_sz = MIN(sizeof(params), uap->sz);
	int rv = copyin(uap->addr, &params, copyin_sz);
	if (rv) {
		return rv;
	}

	if (params.kqwlp_version != (int)uap->sz) {
		return EINVAL;
	}

	return kqueue_workloop_ctl_internal(p, uap->cmd, uap->options, &params,
	           retval);
}

static int
kqueue_select(struct fileproc *fp, int which, void *wql, __unused vfs_context_t ctx)
{
	struct kqfile *kq = (struct kqfile *)fp_get_data(fp);
	int retnum = 0;

	assert((kq->kqf_state & (KQ_WORKLOOP | KQ_WORKQ)) == 0);

	if (which == FREAD) {
		kqlock(kq);
		if (kqfile_begin_processing(kq) == 0) {
			retnum = kq->kqf_count;
			kqfile_end_processing(kq);
		} else if ((kq->kqf_state & KQ_DRAIN) == 0) {
			selrecord(kq->kqf_p, &kq->kqf_sel, wql);
		}
		kqunlock(kq);
	}
	return retnum;
}

/*
 * kqueue_close -
 */
static int
kqueue_close(struct fileglob *fg, __unused vfs_context_t ctx)
{
	struct kqfile *kqf = fg_get_data(fg);

	assert((kqf->kqf_state & (KQ_WORKLOOP | KQ_WORKQ)) == 0);
	kqlock(kqf);
	selthreadclear(&kqf->kqf_sel);
	kqunlock(kqf);
	kqueue_dealloc(&kqf->kqf_kqueue);
	fg_set_data(fg, NULL);
	return 0;
}

/*
 * Max depth of the nested kq path that can be created.
 * Note that this has to be less than the size of kq_level
 * to avoid wrapping around and mislabeling the level. We also
 * want to be aggressive about this so that we don't overflow the
 * kernel stack while posting kevents
 */
#define MAX_NESTED_KQ 10

/*
 * The callers has taken a use-count reference on this kqueue and will donate it
 * to the kqueue we are being added to.  This keeps the kqueue from closing until
 * that relationship is torn down.
 */
static int
kqueue_kqfilter(struct fileproc *fp, struct knote *kn,
    __unused struct kevent_qos_s *kev)
{
	struct kqfile *kqf = (struct kqfile *)fp_get_data(fp);
	struct kqueue *kq = &kqf->kqf_kqueue;
	struct kqueue *parentkq = knote_get_kq(kn);

	assert((kqf->kqf_state & (KQ_WORKLOOP | KQ_WORKQ)) == 0);

	if (parentkq == kq || kn->kn_filter != EVFILT_READ) {
		knote_set_error(kn, EINVAL);
		return 0;
	}

	/*
	 * We have to avoid creating a cycle when nesting kqueues
	 * inside another.  Rather than trying to walk the whole
	 * potential DAG of nested kqueues, we just use a simple
	 * ceiling protocol.  When a kqueue is inserted into another,
	 * we check that the (future) parent is not already nested
	 * into another kqueue at a lower level than the potenial
	 * child (because it could indicate a cycle).  If that test
	 * passes, we just mark the nesting levels accordingly.
	 *
	 * Only up to MAX_NESTED_KQ can be nested.
	 *
	 * Note: kqworkq and kqworkloop cannot be nested and have reused their
	 *       kq_level field, so ignore these as parent.
	 */

	kqlock(parentkq);

	if ((parentkq->kq_state & (KQ_WORKQ | KQ_WORKLOOP)) == 0) {
		if (parentkq->kq_level > 0 &&
		    parentkq->kq_level < kq->kq_level) {
			kqunlock(parentkq);
			knote_set_error(kn, EINVAL);
			return 0;
		}

		/* set parent level appropriately */
		uint16_t plevel = (parentkq->kq_level == 0)? 2: parentkq->kq_level;
		if (plevel < kq->kq_level + 1) {
			if (kq->kq_level + 1 > MAX_NESTED_KQ) {
				kqunlock(parentkq);
				knote_set_error(kn, EINVAL);
				return 0;
			}
			plevel = kq->kq_level + 1;
		}

		parentkq->kq_level = plevel;
	}

	kqunlock(parentkq);

	kn->kn_filtid = EVFILTID_KQREAD;
	kqlock(kq);
	KNOTE_ATTACH(&kqf->kqf_sel.si_note, kn);
	/* indicate nesting in child, if needed */
	if (kq->kq_level == 0) {
		kq->kq_level = 1;
	}

	int count = kq->kq_count;
	kqunlock(kq);
	return count > 0;
}

__attribute__((noinline))
static void
kqfile_wakeup(struct kqfile *kqf, long hint, wait_result_t wr)
{
	/* wakeup a thread waiting on this queue */
	selwakeup(&kqf->kqf_sel);

	/* wake up threads in kqueue_scan() */
	if (kqf->kqf_state & KQ_SLEEP) {
		kqf->kqf_state &= ~KQ_SLEEP;
		thread_wakeup_with_result(&kqf->kqf_count, wr);
	}

	if (hint == NOTE_REVOKE) {
		/* wakeup threads waiting their turn to process */
		if (kqf->kqf_state & KQ_PROCWAIT) {
			assert(kqf->kqf_state & KQ_PROCESSING);
			kqf->kqf_state &= ~KQ_PROCWAIT;
			thread_wakeup(&kqf->kqf_suppressed);
		}

		/* no need to KNOTE: knote_fdclose() takes care of it */
	} else {
		/* wakeup other kqueues/select sets we're inside */
		KNOTE(&kqf->kqf_sel.si_note, hint);
	}
}

/*
 * kqueue_drain - called when kq is closed
 */
static int
kqueue_drain(struct fileproc *fp, __unused vfs_context_t ctx)
{
	struct kqfile *kqf = (struct kqfile *)fp_get_data(fp);

	assert((kqf->kqf_state & (KQ_WORKLOOP | KQ_WORKQ)) == 0);

	kqlock(kqf);
	kqf->kqf_state |= KQ_DRAIN;
	kqfile_wakeup(kqf, NOTE_REVOKE, THREAD_RESTART);
	kqunlock(kqf);
	return 0;
}

int
kqueue_stat(struct kqueue *kq, void *ub, int isstat64, proc_t p)
{
	assert((kq->kq_state & (KQ_WORKLOOP | KQ_WORKQ)) == 0);

	kqlock(kq);
	if (isstat64 != 0) {
		struct stat64 *sb64 = (struct stat64 *)ub;

		bzero((void *)sb64, sizeof(*sb64));
		sb64->st_size = kq->kq_count;
		if (kq->kq_state & KQ_KEV_QOS) {
			sb64->st_blksize = sizeof(struct kevent_qos_s);
		} else if (kq->kq_state & KQ_KEV64) {
			sb64->st_blksize = sizeof(struct kevent64_s);
		} else if (IS_64BIT_PROCESS(p)) {
			sb64->st_blksize = sizeof(struct user64_kevent);
		} else {
			sb64->st_blksize = sizeof(struct user32_kevent);
		}
		sb64->st_mode = S_IFIFO;
	} else {
		struct stat *sb = (struct stat *)ub;

		bzero((void *)sb, sizeof(*sb));
		sb->st_size = kq->kq_count;
		if (kq->kq_state & KQ_KEV_QOS) {
			sb->st_blksize = sizeof(struct kevent_qos_s);
		} else if (kq->kq_state & KQ_KEV64) {
			sb->st_blksize = sizeof(struct kevent64_s);
		} else if (IS_64BIT_PROCESS(p)) {
			sb->st_blksize = sizeof(struct user64_kevent);
		} else {
			sb->st_blksize = sizeof(struct user32_kevent);
		}
		sb->st_mode = S_IFIFO;
	}
	kqunlock(kq);
	return 0;
}

static inline bool
kqueue_threadreq_can_use_ast(struct kqueue *kq)
{
	if (current_proc() == kq->kq_p) {
		/*
		 * Setting an AST from a non BSD syscall is unsafe: mach_msg_trap() can
		 * do combined send/receive and in the case of self-IPC, the AST may bet
		 * set on a thread that will not return to userspace and needs the
		 * thread the AST would create to unblock itself.
		 *
		 * At this time, we really want to target:
		 *
		 * - kevent variants that can cause thread creations, and dispatch
		 *   really only uses kevent_qos and kevent_id,
		 *
		 * - workq_kernreturn (directly about thread creations)
		 *
		 * - bsdthread_ctl which is used for qos changes and has direct impact
		 *   on the creator thread scheduling decisions.
		 */
		switch (current_uthread()->syscall_code) {
		case SYS_kevent_qos:
		case SYS_kevent_id:
		case SYS_workq_kernreturn:
		case SYS_bsdthread_ctl:
			return true;
		}
	}
	return false;
}

/*
 * Interact with the pthread kext to request a servicing there at a specific QoS
 * level.
 *
 * - Caller holds the kqlock
 *
 * - May be called with the kqueue's wait queue set locked,
 *   so cannot do anything that could recurse on that.
 */
static void
kqueue_threadreq_initiate(kqueue_t kqu, workq_threadreq_t kqr,
    kq_index_t qos, int flags)
{
	assert(kqr_thread(kqr) == THREAD_NULL);
	assert(!kqr_thread_requested(kqr));
	struct turnstile *ts = TURNSTILE_NULL;

	if (workq_is_exiting(kqu.kq->kq_p)) {
		return;
	}

	kqlock_held(kqu);

	if (kqu.kq->kq_state & KQ_WORKLOOP) {
		struct kqworkloop *kqwl = kqu.kqwl;

		assert(kqwl->kqwl_owner == THREAD_NULL);
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_THREQUEST),
		    kqwl->kqwl_dynamicid, 0, qos, kqwl->kqwl_wakeup_qos);
		ts = kqwl->kqwl_turnstile;
		/* Add a thread request reference on the kqueue. */
		kqworkloop_retain(kqwl);

#if CONFIG_PREADOPT_TG
		thread_group_qos_t kqwl_preadopt_tg = os_atomic_load(
			&kqwl->kqwl_preadopt_tg, relaxed);
		if (KQWL_HAS_PERMANENT_PREADOPTED_TG(kqwl_preadopt_tg)) {
			/*
			 * This kqwl has been permanently configured with a thread group.
			 * See kqworkloops with scheduling parameters.
			 */
			flags |= WORKQ_THREADREQ_REEVALUATE_PREADOPT_TG;
		} else {
			/*
			 * This thread is the one which is ack-ing the thread group on the kqwl
			 * under the kqlock and will take action accordingly, pairs with the
			 * release barrier in kqueue_set_preadopted_thread_group
			 */
			uint16_t tg_acknowledged;
			if (os_atomic_cmpxchgv(&kqwl->kqwl_preadopt_tg_needs_redrive,
			    KQWL_PREADOPT_TG_NEEDS_REDRIVE, KQWL_PREADOPT_TG_CLEAR_REDRIVE,
			    &tg_acknowledged, acquire)) {
				flags |= WORKQ_THREADREQ_REEVALUATE_PREADOPT_TG;
			}
		}
#endif
	} else {
		assert(kqu.kq->kq_state & KQ_WORKQ);
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWQ_THREQUEST), -1, 0, qos,
		    !TAILQ_EMPTY(&kqu.kqwq->kqwq_queue[kqr->tr_kq_qos_index - 1]));
	}

	/*
	 * New-style thread request supported.
	 * Provide the pthread kext a pointer to a workq_threadreq_s structure for
	 * its use until a corresponding kqueue_threadreq_bind callback.
	 */
	if (kqueue_threadreq_can_use_ast(kqu.kq)) {
		flags |= WORKQ_THREADREQ_SET_AST_ON_FAILURE;
	}
	if (qos == KQWQ_QOS_MANAGER) {
		qos = WORKQ_THREAD_QOS_MANAGER;
	}

	if (!workq_kern_threadreq_initiate(kqu.kq->kq_p, kqr, ts, qos, flags)) {
		/*
		 * Process is shutting down or exec'ing.
		 * All the kqueues are going to be cleaned up
		 * soon. Forget we even asked for a thread -
		 * and make sure we don't ask for more.
		 */
		kqu.kq->kq_state &= ~KQ_R2K_ARMED;
		kqueue_release_live(kqu);
	}
}

/*
 * kqueue_threadreq_bind_prepost - prepost the bind to kevent
 *
 * This is used when kqueue_threadreq_bind may cause a lock inversion.
 */
__attribute__((always_inline))
void
kqueue_threadreq_bind_prepost(struct proc *p __unused, workq_threadreq_t kqr,
    struct uthread *ut)
{
	ut->uu_kqr_bound = kqr;
	kqr->tr_thread = get_machthread(ut);
	kqr->tr_state = WORKQ_TR_STATE_BINDING;
}

/*
 * kqueue_threadreq_bind_commit - commit a bind prepost
 *
 * The workq code has to commit any binding prepost before the thread has
 * a chance to come back to userspace (and do kevent syscalls) or be aborted.
 */
void
kqueue_threadreq_bind_commit(struct proc *p, thread_t thread)
{
	struct uthread *ut = get_bsdthread_info(thread);
	workq_threadreq_t kqr = ut->uu_kqr_bound;
	kqueue_t kqu = kqr_kqueue(p, kqr);

	kqlock(kqu);
	if (kqr->tr_state == WORKQ_TR_STATE_BINDING) {
		kqueue_threadreq_bind(p, kqr, thread, 0);
	}
	kqunlock(kqu);
}

static void
kqueue_threadreq_modify(kqueue_t kqu, workq_threadreq_t kqr, kq_index_t qos,
    workq_kern_threadreq_flags_t flags)
{
	assert(kqr_thread_requested_pending(kqr));

	kqlock_held(kqu);

	if (kqueue_threadreq_can_use_ast(kqu.kq)) {
		flags |= WORKQ_THREADREQ_SET_AST_ON_FAILURE;
	}

#if CONFIG_PREADOPT_TG
	if (kqu.kq->kq_state & KQ_WORKLOOP) {
		struct kqworkloop *kqwl = kqu.kqwl;
		thread_group_qos_t kqwl_preadopt_tg = os_atomic_load(
			&kqwl->kqwl_preadopt_tg, relaxed);
		if (KQWL_HAS_PERMANENT_PREADOPTED_TG(kqwl_preadopt_tg)) {
			/*
			 * This kqwl has been permanently configured with a thread group.
			 * See kqworkloops with scheduling parameters.
			 */
			flags |= WORKQ_THREADREQ_REEVALUATE_PREADOPT_TG;
		} else {
			uint16_t tg_ack_status;
			/*
			 * This thread is the one which is ack-ing the thread group on the kqwl
			 * under the kqlock and will take action accordingly, needs acquire
			 * barrier.
			 */
			if (os_atomic_cmpxchgv(&kqwl->kqwl_preadopt_tg_needs_redrive, KQWL_PREADOPT_TG_NEEDS_REDRIVE,
			    KQWL_PREADOPT_TG_CLEAR_REDRIVE, &tg_ack_status, acquire)) {
				flags |= WORKQ_THREADREQ_REEVALUATE_PREADOPT_TG;
			}
		}
	}
#endif

	workq_kern_threadreq_modify(kqu.kq->kq_p, kqr, qos, flags);
}

/*
 * kqueue_threadreq_bind - bind thread to processing kqrequest
 *
 * The provided thread will be responsible for delivering events
 * associated with the given kqrequest.  Bind it and get ready for
 * the thread to eventually arrive.
 */
void
kqueue_threadreq_bind(struct proc *p, workq_threadreq_t kqr, thread_t thread,
    unsigned int flags)
{
	kqueue_t kqu = kqr_kqueue(p, kqr);
	struct uthread *ut = get_bsdthread_info(thread);

	kqlock_held(kqu);

	assert(ut->uu_kqueue_override == 0);

	if (kqr->tr_state == WORKQ_TR_STATE_BINDING) {
		assert(ut->uu_kqr_bound == kqr);
		assert(kqr->tr_thread == thread);
	} else {
		assert(kqr_thread_requested_pending(kqr));
		assert(kqr->tr_thread == THREAD_NULL);
		assert(ut->uu_kqr_bound == NULL);
		ut->uu_kqr_bound = kqr;
		kqr->tr_thread = thread;
	}

	kqr->tr_state = WORKQ_TR_STATE_BOUND;

	if (kqu.kq->kq_state & KQ_WORKLOOP) {
		struct turnstile *ts = kqu.kqwl->kqwl_turnstile;

		if (__improbable(thread == kqu.kqwl->kqwl_owner)) {
			/*
			 * <rdar://problem/38626999> shows that asserting here is not ok.
			 *
			 * This is not supposed to happen for correct use of the interface,
			 * but it is sadly possible for userspace (with the help of memory
			 * corruption, such as over-release of a dispatch queue) to make
			 * the creator thread the "owner" of a workloop.
			 *
			 * Once that happens, and that creator thread picks up the same
			 * workloop as a servicer, we trip this codepath. We need to fixup
			 * the state to forget about this thread being the owner, as the
			 * entire workloop state machine expects servicers to never be
			 * owners and everything would basically go downhill from here.
			 */
			kqu.kqwl->kqwl_owner = THREAD_NULL;
			if (kqworkloop_override(kqu.kqwl)) {
				thread_drop_kevent_override(thread);
			}
		}

		if (ts && (flags & KQUEUE_THREADERQ_BIND_NO_INHERITOR_UPDATE) == 0) {
			/*
			 * Past this point, the interlock is the kq req lock again,
			 * so we can fix the inheritor for good.
			 */
			filt_wlupdate_inheritor(kqu.kqwl, ts, TURNSTILE_IMMEDIATE_UPDATE);
			turnstile_update_inheritor_complete(ts, TURNSTILE_INTERLOCK_HELD);
		}

		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_BIND), kqu.kqwl->kqwl_dynamicid,
		    thread_tid(thread), kqr->tr_kq_qos_index,
		    (kqr->tr_kq_override_index << 16) | kqwl->kqwl_wakeup_qos);

		ut->uu_kqueue_override = kqr->tr_kq_override_index;
		if (kqr->tr_kq_override_index) {
			thread_add_servicer_override(thread, kqr->tr_kq_override_index);
		}

#if CONFIG_PREADOPT_TG
		/* Remove reference from kqwl and mark it as bound with the SENTINEL */
		thread_group_qos_t old_tg;
		thread_group_qos_t new_tg;
		int ret = os_atomic_rmw_loop(kqr_preadopt_thread_group_addr(kqr), old_tg, new_tg, relaxed, {
			if ((old_tg == KQWL_PREADOPTED_TG_NEVER) || KQWL_HAS_PERMANENT_PREADOPTED_TG(old_tg)) {
			        /*
			         * Either an app or a kqwl permanently configured with a thread group.
			         * Nothing to do.
			         */
			        os_atomic_rmw_loop_give_up(break);
			}
			assert(old_tg != KQWL_PREADOPTED_TG_PROCESSED);
			new_tg = KQWL_PREADOPTED_TG_SENTINEL;
		});

		if (ret) {
			KQWL_PREADOPT_TG_HISTORY_WRITE_ENTRY(kqu.kqwl, KQWL_PREADOPT_OP_SERVICER_BIND, old_tg, new_tg);

			if (KQWL_HAS_VALID_PREADOPTED_TG(old_tg)) {
				struct thread_group *tg = KQWL_GET_PREADOPTED_TG(old_tg);
				assert(tg != NULL);

				thread_set_preadopt_thread_group(thread, tg);
				thread_group_release_live(tg); // The thread has a reference
			} else {
				/*
				 * The thread may already have a preadopt thread group on it -
				 * we need to make sure to clear that.
				 */
				thread_set_preadopt_thread_group(thread, NULL);
			}

			/* We have taken action on the preadopted thread group set on the
			 * set on the kqwl, clear any redrive requests */
			os_atomic_store(&kqu.kqwl->kqwl_preadopt_tg_needs_redrive, KQWL_PREADOPT_TG_CLEAR_REDRIVE, relaxed);
		} else {
			if (KQWL_HAS_PERMANENT_PREADOPTED_TG(old_tg)) {
				struct thread_group *tg = KQWL_GET_PREADOPTED_TG(old_tg);
				assert(tg != NULL);
				thread_set_preadopt_thread_group(thread, tg);
				/*
				 * From this point on, kqwl and thread both have +1 ref on this tg.
				 */
			}
		}
#endif
		kqueue_update_iotier_override(kqu);
	} else {
		assert(kqr->tr_kq_override_index == 0);

#if CONFIG_PREADOPT_TG
		/*
		 * The thread may have a preadopt thread group on it already because it
		 * got tagged with it as a creator thread. So we need to make sure to
		 * clear that since we don't have preadopt thread groups for non-kqwl
		 * cases
		 */
		thread_set_preadopt_thread_group(thread, NULL);
#endif
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWQ_BIND), -1,
		    thread_tid(thread), kqr->tr_kq_qos_index,
		    (kqr->tr_kq_override_index << 16) |
		    !TAILQ_EMPTY(&kqu.kqwq->kqwq_queue[kqr->tr_kq_qos_index - 1]));
	}
}

/*
 * kqueue_threadreq_cancel - abort a pending thread request
 *
 * Called when exiting/exec'ing. Forget our pending request.
 */
void
kqueue_threadreq_cancel(struct proc *p, workq_threadreq_t kqr)
{
	kqueue_release(kqr_kqueue(p, kqr));
}

workq_threadreq_param_t
kqueue_threadreq_workloop_param(workq_threadreq_t kqr)
{
	struct kqworkloop *kqwl;
	workq_threadreq_param_t trp;

	assert(kqr->tr_flags & WORKQ_TR_FLAG_WORKLOOP);
	kqwl = __container_of(kqr, struct kqworkloop, kqwl_request);
	trp.trp_value = kqwl->kqwl_params;
	return trp;
}

/*
 *	kqueue_threadreq_unbind - unbind thread from processing kqueue
 *
 *	End processing the per-QoS bucket of events and allow other threads
 *	to be requested for future servicing.
 *
 *	caller holds a reference on the kqueue.
 */
void
kqueue_threadreq_unbind(struct proc *p, workq_threadreq_t kqr)
{
	if (kqr->tr_flags & WORKQ_TR_FLAG_WORKLOOP) {
		kqworkloop_unbind(kqr_kqworkloop(kqr));
	} else {
		kqworkq_unbind(p, kqr);
	}
}

/*
 * If we aren't already busy processing events [for this QoS],
 * request workq thread support as appropriate.
 *
 * TBD - for now, we don't segregate out processing by QoS.
 *
 * - May be called with the kqueue's wait queue set locked,
 *   so cannot do anything that could recurse on that.
 */
static void
kqworkq_wakeup(struct kqworkq *kqwq, kq_index_t qos_index)
{
	workq_threadreq_t kqr = kqworkq_get_request(kqwq, qos_index);

	/* convert to thread qos value */
	assert(qos_index > 0 && qos_index <= KQWQ_NBUCKETS);

	if (!kqr_thread_requested(kqr)) {
		kqueue_threadreq_initiate(&kqwq->kqwq_kqueue, kqr, qos_index, 0);
	}
}

/*
 * This represent the asynchronous QoS a given workloop contributes,
 * hence is the max of the current active knotes (override index)
 * and the workloop max qos (userspace async qos).
 */
static kq_index_t
kqworkloop_override(struct kqworkloop *kqwl)
{
	workq_threadreq_t kqr = &kqwl->kqwl_request;
	return MAX(kqr->tr_kq_qos_index, kqr->tr_kq_override_index);
}

static inline void
kqworkloop_request_fire_r2k_notification(struct kqworkloop *kqwl)
{
	workq_threadreq_t kqr = &kqwl->kqwl_request;

	kqlock_held(kqwl);

	if (kqwl->kqwl_state & KQ_R2K_ARMED) {
		kqwl->kqwl_state &= ~KQ_R2K_ARMED;
		act_set_astkevent(kqr_thread_fast(kqr), AST_KEVENT_RETURN_TO_KERNEL);
	}
}

static void
kqworkloop_update_threads_qos(struct kqworkloop *kqwl, int op, kq_index_t qos)
{
	workq_threadreq_t kqr = &kqwl->kqwl_request;
	struct kqueue *kq = &kqwl->kqwl_kqueue;
	kq_index_t old_override = kqworkloop_override(kqwl);

	kqlock_held(kqwl);

	switch (op) {
	case KQWL_UTQ_UPDATE_WAKEUP_QOS:
		kqwl->kqwl_wakeup_qos = qos;
		kqworkloop_request_fire_r2k_notification(kqwl);
		goto recompute;

	case KQWL_UTQ_RESET_WAKEUP_OVERRIDE:
		kqr->tr_kq_override_index = qos;
		goto recompute;

	case KQWL_UTQ_PARKING:
	case KQWL_UTQ_UNBINDING:
		kqr->tr_kq_override_index = qos;
		OS_FALLTHROUGH;

	case KQWL_UTQ_RECOMPUTE_WAKEUP_QOS:
		if (op == KQWL_UTQ_RECOMPUTE_WAKEUP_QOS) {
			assert(qos == THREAD_QOS_UNSPECIFIED);
		}
		if (TAILQ_EMPTY(&kqwl->kqwl_suppressed)) {
			kqr->tr_kq_override_index = THREAD_QOS_UNSPECIFIED;
		}
		kqwl->kqwl_wakeup_qos = 0;
		for (kq_index_t i = KQWL_NBUCKETS; i > 0; i--) {
			if (!TAILQ_EMPTY(&kqwl->kqwl_queue[i - 1])) {
				kqwl->kqwl_wakeup_qos = i;
				kqworkloop_request_fire_r2k_notification(kqwl);
				break;
			}
		}
		OS_FALLTHROUGH;

	case KQWL_UTQ_UPDATE_WAKEUP_OVERRIDE:
recompute:
		/*
		 * When modifying the wakeup QoS or the override QoS, we always need to
		 * maintain our invariant that kqr_override_index is at least as large
		 * as the highest QoS for which an event is fired.
		 *
		 * However this override index can be larger when there is an overriden
		 * suppressed knote pushing on the kqueue.
		 */
		if (qos < kqwl->kqwl_wakeup_qos) {
			qos = kqwl->kqwl_wakeup_qos;
		}
		if (kqr->tr_kq_override_index < qos) {
			kqr->tr_kq_override_index = qos;
		}
		break;

	case KQWL_UTQ_REDRIVE_EVENTS:
		break;

	case KQWL_UTQ_SET_QOS_INDEX:
		kqr->tr_kq_qos_index = qos;
		break;

	default:
		panic("unknown kqwl thread qos update operation: %d", op);
	}

	thread_t kqwl_owner = kqwl->kqwl_owner;
	thread_t servicer = kqr_thread(kqr);
	boolean_t qos_changed = FALSE;
	kq_index_t new_override = kqworkloop_override(kqwl);

	/*
	 * Apply the diffs to the owner if applicable
	 */
	if (kqwl_owner) {
#if 0
		/* JMM - need new trace hooks for owner overrides */
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_THADJUST),
		    kqwl->kqwl_dynamicid, thread_tid(kqwl_owner), kqr->tr_kq_qos_index,
		    (kqr->tr_kq_override_index << 16) | kqwl->kqwl_wakeup_qos);
#endif
		if (new_override == old_override) {
			// nothing to do
		} else if (old_override == THREAD_QOS_UNSPECIFIED) {
			thread_add_kevent_override(kqwl_owner, new_override);
		} else if (new_override == THREAD_QOS_UNSPECIFIED) {
			thread_drop_kevent_override(kqwl_owner);
		} else { /*  old_override != new_override */
			thread_update_kevent_override(kqwl_owner, new_override);
		}
	}

	/*
	 * apply the diffs to the servicer
	 */

	if (!kqr_thread_requested(kqr)) {
		/*
		 * No servicer, nor thread-request
		 *
		 * Make a new thread request, unless there is an owner (or the workloop
		 * is suspended in userland) or if there is no asynchronous work in the
		 * first place.
		 */

		if (kqwl_owner == NULL && kqwl->kqwl_wakeup_qos) {
			int initiate_flags = 0;
			if (op == KQWL_UTQ_UNBINDING) {
				initiate_flags = WORKQ_THREADREQ_ATTEMPT_REBIND;
			}

			/* kqueue_threadreq_initiate handles the acknowledgement of the TG
			 * if needed */
			kqueue_threadreq_initiate(kq, kqr, new_override, initiate_flags);
		}
	} else if (servicer) {
		/*
		 * Servicer in flight
		 *
		 * Just apply the diff to the servicer
		 */

#if CONFIG_PREADOPT_TG
		/* When there's a servicer for the kqwl already, then the servicer will
		 * adopt the thread group in the kqr, we don't need to poke the
		 * workqueue subsystem to make different decisions due to the thread
		 * group. Consider the current request ack-ed.
		 */
		os_atomic_store(&kqwl->kqwl_preadopt_tg_needs_redrive, KQWL_PREADOPT_TG_CLEAR_REDRIVE, relaxed);
#endif

		struct uthread *ut = get_bsdthread_info(servicer);
		if (ut->uu_kqueue_override != new_override) {
			if (ut->uu_kqueue_override == THREAD_QOS_UNSPECIFIED) {
				thread_add_servicer_override(servicer, new_override);
			} else if (new_override == THREAD_QOS_UNSPECIFIED) {
				thread_drop_servicer_override(servicer);
			} else { /* ut->uu_kqueue_override != new_override */
				thread_update_servicer_override(servicer, new_override);
			}
			ut->uu_kqueue_override = new_override;
			qos_changed = TRUE;
		}
	} else if (new_override == THREAD_QOS_UNSPECIFIED) {
		/*
		 * No events to deliver anymore.
		 *
		 * However canceling with turnstiles is challenging, so the fact that
		 * the request isn't useful will be discovered by the servicer himself
		 * later on.
		 */
	} else if (old_override != new_override) {
		/*
		 * Request is in flight
		 *
		 * Apply the diff to the thread request.
		 */
		kqueue_threadreq_modify(kq, kqr, new_override, WORKQ_THREADREQ_NONE);
		qos_changed = TRUE;
	}

	if (qos_changed) {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_THADJUST), kqwl->kqwl_dynamicid,
		    thread_tid(servicer), kqr->tr_kq_qos_index,
		    (kqr->tr_kq_override_index << 16) | kqwl->kqwl_wakeup_qos);
	}
}

static void
kqworkloop_update_iotier_override(struct kqworkloop *kqwl)
{
	workq_threadreq_t kqr = &kqwl->kqwl_request;
	thread_t servicer = kqr_thread(kqr);
	uint8_t iotier = os_atomic_load(&kqwl->kqwl_iotier_override, relaxed);

	kqlock_held(kqwl);

	if (servicer) {
		thread_update_servicer_iotier_override(servicer, iotier);
	}
}

static void
kqworkloop_wakeup(struct kqworkloop *kqwl, kq_index_t qos)
{
	if (qos <= kqwl->kqwl_wakeup_qos) {
		/*
		 * Shortcut wakeups that really do nothing useful
		 */
		return;
	}

	if ((kqwl->kqwl_state & KQ_PROCESSING) &&
	    kqr_thread(&kqwl->kqwl_request) == current_thread()) {
		/*
		 * kqworkloop_end_processing() will perform the required QoS
		 * computations when it unsets the processing mode.
		 */
		return;
	}

	kqworkloop_update_threads_qos(kqwl, KQWL_UTQ_UPDATE_WAKEUP_QOS, qos);
}

static struct kqtailq *
kqueue_get_suppressed_queue(kqueue_t kq, struct knote *kn)
{
	if (kq.kq->kq_state & KQ_WORKLOOP) {
		return &kq.kqwl->kqwl_suppressed;
	} else if (kq.kq->kq_state & KQ_WORKQ) {
		return &kq.kqwq->kqwq_suppressed[kn->kn_qos_index - 1];
	} else {
		return &kq.kqf->kqf_suppressed;
	}
}

struct turnstile *
kqueue_alloc_turnstile(kqueue_t kqu)
{
	struct kqworkloop *kqwl = kqu.kqwl;
	kq_state_t kq_state;

	kq_state = os_atomic_load(&kqu.kq->kq_state, dependency);
	if (kq_state & KQ_HAS_TURNSTILE) {
		/* force a dependency to pair with the atomic or with release below */
		return os_atomic_load_with_dependency_on(&kqwl->kqwl_turnstile,
		           (uintptr_t)kq_state);
	}

	if (!(kq_state & KQ_WORKLOOP)) {
		return TURNSTILE_NULL;
	}

	struct turnstile *ts = turnstile_alloc(), *free_ts = TURNSTILE_NULL;
	bool workq_locked = false;

	kqlock(kqu);

	if (filt_wlturnstile_interlock_is_workq(kqwl)) {
		workq_locked = true;
		workq_kern_threadreq_lock(kqwl->kqwl_p);
	}

	if (kqwl->kqwl_state & KQ_HAS_TURNSTILE) {
		free_ts = ts;
		ts = kqwl->kqwl_turnstile;
	} else {
		ts = turnstile_prepare((uintptr_t)kqwl, &kqwl->kqwl_turnstile,
		    ts, TURNSTILE_WORKLOOPS);

		/* release-barrier to pair with the unlocked load of kqwl_turnstile above */
		os_atomic_or(&kqwl->kqwl_state, KQ_HAS_TURNSTILE, release);

		if (filt_wlturnstile_interlock_is_workq(kqwl)) {
			workq_kern_threadreq_update_inheritor(kqwl->kqwl_p,
			    &kqwl->kqwl_request, kqwl->kqwl_owner,
			    ts, TURNSTILE_IMMEDIATE_UPDATE);
			/*
			 * The workq may no longer be the interlock after this.
			 * In which case the inheritor wasn't updated.
			 */
		}
		if (!filt_wlturnstile_interlock_is_workq(kqwl)) {
			filt_wlupdate_inheritor(kqwl, ts, TURNSTILE_IMMEDIATE_UPDATE);
		}
	}

	if (workq_locked) {
		workq_kern_threadreq_unlock(kqwl->kqwl_p);
	}

	kqunlock(kqu);

	if (free_ts) {
		turnstile_deallocate(free_ts);
	} else {
		turnstile_update_inheritor_complete(ts, TURNSTILE_INTERLOCK_NOT_HELD);
	}
	return ts;
}

__attribute__((always_inline))
struct turnstile *
kqueue_turnstile(kqueue_t kqu)
{
	kq_state_t kq_state = os_atomic_load(&kqu.kq->kq_state, relaxed);
	if (kq_state & KQ_WORKLOOP) {
		return os_atomic_load(&kqu.kqwl->kqwl_turnstile, relaxed);
	}
	return TURNSTILE_NULL;
}

__attribute__((always_inline))
struct turnstile *
kqueue_threadreq_get_turnstile(workq_threadreq_t kqr)
{
	struct kqworkloop *kqwl = kqr_kqworkloop(kqr);
	if (kqwl) {
		return os_atomic_load(&kqwl->kqwl_turnstile, relaxed);
	}
	return TURNSTILE_NULL;
}

static void
kqworkloop_set_overcommit(struct kqworkloop *kqwl)
{
	workq_threadreq_t kqr = &kqwl->kqwl_request;

	/*
	 * This test is racy, but since we never remove this bit,
	 * it allows us to avoid taking a lock.
	 */
	if (kqr->tr_flags & WORKQ_TR_FLAG_OVERCOMMIT) {
		return;
	}

	kqlock_held(kqwl);

	if (kqr_thread_requested_pending(kqr)) {
		kqueue_threadreq_modify(kqwl, kqr, kqr->tr_qos,
		    WORKQ_THREADREQ_MAKE_OVERCOMMIT);
	} else {
		kqr->tr_flags |= WORKQ_TR_FLAG_OVERCOMMIT;
	}
}

static void
kqworkq_update_override(struct kqworkq *kqwq, struct knote *kn,
    kq_index_t override_index)
{
	workq_threadreq_t kqr;
	kq_index_t old_override_index;
	kq_index_t queue_index = kn->kn_qos_index;

	if (override_index <= queue_index) {
		return;
	}

	kqr = kqworkq_get_request(kqwq, queue_index);

	kqlock_held(kqwq);

	old_override_index = kqr->tr_kq_override_index;
	if (override_index > MAX(kqr->tr_kq_qos_index, old_override_index)) {
		thread_t servicer = kqr_thread(kqr);
		kqr->tr_kq_override_index = override_index;

		/* apply the override to [incoming?] servicing thread */
		if (servicer) {
			if (old_override_index) {
				thread_update_kevent_override(servicer, override_index);
			} else {
				thread_add_kevent_override(servicer, override_index);
			}
		}
	}
}

static void
kqueue_update_iotier_override(kqueue_t kqu)
{
	if (kqu.kq->kq_state & KQ_WORKLOOP) {
		kqworkloop_update_iotier_override(kqu.kqwl);
	}
}

static void
kqueue_update_override(kqueue_t kqu, struct knote *kn, thread_qos_t qos)
{
	if (kqu.kq->kq_state & KQ_WORKLOOP) {
		kqworkloop_update_threads_qos(kqu.kqwl, KQWL_UTQ_UPDATE_WAKEUP_OVERRIDE,
		    qos);
	} else {
		kqworkq_update_override(kqu.kqwq, kn, qos);
	}
}

static void
kqworkloop_unbind_locked(struct kqworkloop *kqwl, thread_t thread,
    enum kqwl_unbind_locked_mode how)
{
	struct uthread *ut = get_bsdthread_info(thread);
	workq_threadreq_t kqr = &kqwl->kqwl_request;

	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWL_UNBIND), kqwl->kqwl_dynamicid,
	    thread_tid(thread), 0, 0);

	kqlock_held(kqwl);

	assert(ut->uu_kqr_bound == kqr);
	ut->uu_kqr_bound = NULL;
	if (how == KQWL_OVERRIDE_DROP_IMMEDIATELY &&
	    ut->uu_kqueue_override != THREAD_QOS_UNSPECIFIED) {
		thread_drop_servicer_override(thread);
		ut->uu_kqueue_override = THREAD_QOS_UNSPECIFIED;
	}

	if (kqwl->kqwl_owner == NULL && kqwl->kqwl_turnstile) {
		turnstile_update_inheritor(kqwl->kqwl_turnstile,
		    TURNSTILE_INHERITOR_NULL, TURNSTILE_IMMEDIATE_UPDATE);
		turnstile_update_inheritor_complete(kqwl->kqwl_turnstile,
		    TURNSTILE_INTERLOCK_HELD);
	}

#if CONFIG_PREADOPT_TG
	/* The kqueue is able to adopt a thread group again */

	thread_group_qos_t old_tg, new_tg = NULL;
	int ret = os_atomic_rmw_loop(kqr_preadopt_thread_group_addr(kqr), old_tg, new_tg, relaxed, {
		new_tg = old_tg;
		if (old_tg == KQWL_PREADOPTED_TG_SENTINEL || old_tg == KQWL_PREADOPTED_TG_PROCESSED) {
		        new_tg = KQWL_PREADOPTED_TG_NULL;
		}
	});

	if (ret) {
		KQWL_PREADOPT_TG_HISTORY_WRITE_ENTRY(kqwl, KQWL_PREADOPT_OP_SERVICER_UNBIND, old_tg, KQWL_PREADOPTED_TG_NULL);
		// Servicer can drop any preadopt thread group it has since it has
		// unbound.
		thread_set_preadopt_thread_group(thread, NULL);
	}
#endif
	thread_update_servicer_iotier_override(thread, THROTTLE_LEVEL_END);

	kqr->tr_thread = THREAD_NULL;
	kqr->tr_state = WORKQ_TR_STATE_IDLE;
	kqwl->kqwl_state &= ~KQ_R2K_ARMED;
}

static void
kqworkloop_unbind_delayed_override_drop(thread_t thread)
{
	struct uthread *ut = get_bsdthread_info(thread);
	assert(ut->uu_kqr_bound == NULL);
	if (ut->uu_kqueue_override != THREAD_QOS_UNSPECIFIED) {
		thread_drop_servicer_override(thread);
		ut->uu_kqueue_override = THREAD_QOS_UNSPECIFIED;
	}
}

/*
 *	kqworkloop_unbind - Unbind the servicer thread of a workloop kqueue
 *
 *	It will acknowledge events, and possibly request a new thread if:
 *	- there were active events left
 *	- we pended waitq hook callouts during processing
 *	- we pended wakeups while processing (or unsuppressing)
 *
 *	Called with kqueue lock held.
 */
static void
kqworkloop_unbind(struct kqworkloop *kqwl)
{
	struct kqueue *kq = &kqwl->kqwl_kqueue;
	workq_threadreq_t kqr = &kqwl->kqwl_request;
	thread_t thread = kqr_thread_fast(kqr);
	int op = KQWL_UTQ_PARKING;
	kq_index_t qos_override = THREAD_QOS_UNSPECIFIED;

	assert(thread == current_thread());

	kqlock(kqwl);

	/*
	 * Forcing the KQ_PROCESSING flag allows for QoS updates because of
	 * unsuppressing knotes not to be applied until the eventual call to
	 * kqworkloop_update_threads_qos() below.
	 */
	assert((kq->kq_state & KQ_PROCESSING) == 0);
	if (!TAILQ_EMPTY(&kqwl->kqwl_suppressed)) {
		kq->kq_state |= KQ_PROCESSING;
		qos_override = kqworkloop_acknowledge_events(kqwl);
		kq->kq_state &= ~KQ_PROCESSING;
	}

	kqworkloop_unbind_locked(kqwl, thread, KQWL_OVERRIDE_DROP_DELAYED);
	kqworkloop_update_threads_qos(kqwl, op, qos_override);

	kqunlock(kqwl);

	/*
	 * Drop the override on the current thread last, after the call to
	 * kqworkloop_update_threads_qos above.
	 */
	kqworkloop_unbind_delayed_override_drop(thread);

	/* If last reference, dealloc the workloop kq */
	kqworkloop_release(kqwl);
}

static thread_qos_t
kqworkq_unbind_locked(struct kqworkq *kqwq,
    workq_threadreq_t kqr, thread_t thread)
{
	struct uthread *ut = get_bsdthread_info(thread);
	kq_index_t old_override = kqr->tr_kq_override_index;

	KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KQWQ_UNBIND), -1,
	    thread_tid(kqr_thread(kqr)), kqr->tr_kq_qos_index, 0);

	kqlock_held(kqwq);

	assert(ut->uu_kqr_bound == kqr);
	ut->uu_kqr_bound = NULL;
	kqr->tr_thread = THREAD_NULL;
	kqr->tr_state = WORKQ_TR_STATE_IDLE;
	kqr->tr_kq_override_index = THREAD_QOS_UNSPECIFIED;
	kqwq->kqwq_state &= ~KQ_R2K_ARMED;

	return old_override;
}

/*
 *	kqworkq_unbind - unbind of a workq kqueue from a thread
 *
 *	We may have to request new threads.
 *	This can happen there are no waiting processing threads and:
 *	- there were active events we never got to (count > 0)
 *	- we pended waitq hook callouts during processing
 *	- we pended wakeups while processing (or unsuppressing)
 */
static void
kqworkq_unbind(proc_t p, workq_threadreq_t kqr)
{
	struct kqworkq *kqwq = (struct kqworkq *)p->p_fd.fd_wqkqueue;
	__assert_only int rc;

	kqlock(kqwq);
	rc = kqworkq_acknowledge_events(kqwq, kqr, 0, KQWQAE_UNBIND);
	assert(rc == -1);
	kqunlock(kqwq);
}

workq_threadreq_t
kqworkq_get_request(struct kqworkq *kqwq, kq_index_t qos_index)
{
	assert(qos_index > 0 && qos_index <= KQWQ_NBUCKETS);
	return &kqwq->kqwq_request[qos_index - 1];
}

static void
knote_reset_priority(kqueue_t kqu, struct knote *kn, pthread_priority_t pp)
{
	kq_index_t qos = _pthread_priority_thread_qos(pp);

	if (kqu.kq->kq_state & KQ_WORKLOOP) {
		assert((pp & _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG) == 0);
		pp = _pthread_priority_normalize(pp);
	} else if (kqu.kq->kq_state & KQ_WORKQ) {
		if (qos == THREAD_QOS_UNSPECIFIED) {
			/* On workqueues, outside of QoS means MANAGER */
			qos = KQWQ_QOS_MANAGER;
			pp = _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
		} else {
			pp = _pthread_priority_normalize(pp);
		}
	} else {
		pp = _pthread_unspecified_priority();
		qos = THREAD_QOS_UNSPECIFIED;
	}

	kn->kn_qos = (int32_t)pp;

	if ((kn->kn_status & KN_MERGE_QOS) == 0 || qos > kn->kn_qos_override) {
		/* Never lower QoS when in "Merge" mode */
		kn->kn_qos_override = qos;
	}

	/* only adjust in-use qos index when not suppressed */
	if (kn->kn_status & KN_SUPPRESSED) {
		kqueue_update_override(kqu, kn, qos);
	} else if (kn->kn_qos_index != qos) {
		knote_dequeue(kqu, kn);
		kn->kn_qos_index = qos;
	}
}

static void
knote_adjust_qos(struct kqueue *kq, struct knote *kn, int result)
{
	thread_qos_t qos_index = (result >> FILTER_ADJUST_EVENT_QOS_SHIFT) & 7;

	kqlock_held(kq);

	assert(result & FILTER_ADJUST_EVENT_QOS_BIT);
	assert(qos_index < THREAD_QOS_LAST);

	/*
	 * Early exit for knotes that should not change QoS
	 */
	if (__improbable(!knote_fops(kn)->f_adjusts_qos)) {
		panic("filter %d cannot change QoS", kn->kn_filtid);
	} else if (__improbable(!knote_has_qos(kn))) {
		return;
	}

	/*
	 * knotes with the FALLBACK flag will only use their registration QoS if the
	 * incoming event has no QoS, else, the registration QoS acts as a floor.
	 */
	thread_qos_t req_qos = _pthread_priority_thread_qos_fast(kn->kn_qos);
	if (kn->kn_qos & _PTHREAD_PRIORITY_FALLBACK_FLAG) {
		if (qos_index == THREAD_QOS_UNSPECIFIED) {
			qos_index = req_qos;
		}
	} else {
		if (qos_index < req_qos) {
			qos_index = req_qos;
		}
	}
	if ((kn->kn_status & KN_MERGE_QOS) && (qos_index < kn->kn_qos_override)) {
		/* Never lower QoS when in "Merge" mode */
		return;
	}

	if ((kn->kn_status & KN_LOCKED) && (kn->kn_status & KN_POSTING)) {
		/*
		 * When we're trying to update the QoS override and that both an
		 * f_event() and other f_* calls are running concurrently, any of these
		 * in flight calls may want to perform overrides that aren't properly
		 * serialized with each other.
		 *
		 * The first update that observes this racy situation enters a "Merge"
		 * mode which causes subsequent override requests to saturate the
		 * override instead of replacing its value.
		 *
		 * This mode is left when knote_unlock() or knote_post()
		 * observe that no other f_* routine is in flight.
		 */
		kn->kn_status |= KN_MERGE_QOS;
	}

	/*
	 * Now apply the override if it changed.
	 */

	if (kn->kn_qos_override == qos_index) {
		return;
	}

	kn->kn_qos_override = qos_index;

	if (kn->kn_status & KN_SUPPRESSED) {
		/*
		 * For suppressed events, the kn_qos_index field cannot be touched as it
		 * allows us to know on which supress queue the knote is for a kqworkq.
		 *
		 * Also, there's no natural push applied on the kqueues when this field
		 * changes anyway. We hence need to apply manual overrides in this case,
		 * which will be cleared when the events are later acknowledged.
		 */
		kqueue_update_override(kq, kn, qos_index);
	} else if (kn->kn_qos_index != qos_index) {
		knote_dequeue(kq, kn);
		kn->kn_qos_index = qos_index;
	}
}

void
klist_init(struct klist *list)
{
	SLIST_INIT(list);
}


/*
 *	Query/Post each knote in the object's list
 *
 *	The object lock protects the list. It is assumed that the filter/event
 *	routine for the object can determine that the object is already locked (via
 *	the hint) and not deadlock itself.
 *
 *	Autodetach is a specific contract which will detach all knotes from the
 *	object prior to posting the final event for that knote. This is done while
 *	under the object lock. A breadcrumb is left in the knote's next pointer to
 *	indicate to future calls to f_detach routines that they need not reattempt
 *	to knote_detach from the object's klist again. This is currently used by
 *	EVFILTID_SPEC, EVFILTID_TTY, EVFILTID_PTMX
 *
 */
void
knote(struct klist *list, long hint, bool autodetach)
{
	struct knote *kn;
	struct knote *tmp_kn;
	SLIST_FOREACH_SAFE(kn, list, kn_selnext, tmp_kn) {
		/*
		 * We can modify the knote's next pointer since since we are holding the
		 * object lock and the list can't be concurrently modified. Anyone
		 * determining auto-detached-ness of a knote should take the primitive lock
		 * to synchronize.
		 *
		 * Note that we do this here instead of the filter's f_event since we may
		 * not even post the event if the knote is being dropped.
		 */
		if (autodetach) {
			kn->kn_selnext.sle_next = KNOTE_AUTODETACHED;
		}
		knote_post(kn, hint);
	}

	/* Blast away the entire klist */
	if (autodetach) {
		klist_init(list);
	}
}

/*
 * attach a knote to the specified list.  Return true if this is the first entry.
 * The list is protected by whatever lock the object it is associated with uses.
 */
int
knote_attach(struct klist *list, struct knote *kn)
{
	int ret = SLIST_EMPTY(list);
	SLIST_INSERT_HEAD(list, kn, kn_selnext);
	return ret;
}

/*
 * detach a knote from the specified list.  Return true if that was the last
 * entry.  The list is protected by whatever lock the object it is associated
 * with uses.
 */
int
knote_detach(struct klist *list, struct knote *kn)
{
	assert(!KNOTE_IS_AUTODETACHED(kn));

	SLIST_REMOVE(list, kn, knote, kn_selnext);
	return SLIST_EMPTY(list);
}

/*
 * knote_vanish - Indicate that the source has vanished
 *
 * Used only for vanishing ports - vanishing fds go
 * through knote_fdclose()
 *
 * If the knote has requested EV_VANISHED delivery,
 * arrange for that. Otherwise, deliver a NOTE_REVOKE
 * event for backward compatibility.
 *
 * The knote is marked as having vanished. The source's
 * reference to the knote is dropped by caller, but the knote's
 * source reference is only cleaned up later when the knote is dropped.
 *
 * Our caller already has the object lock held. Calling
 * the detach routine would try to take that lock
 * recursively - which likely is not supported.
 */
void
knote_vanish(struct klist *list, bool make_active)
{
	struct knote *kn;
	struct knote *kn_next;

	SLIST_FOREACH_SAFE(kn, list, kn_selnext, kn_next) {
		struct kqueue *kq = knote_get_kq(kn);

		kqlock(kq);
		if (__probable(kn->kn_status & KN_REQVANISH)) {
			/*
			 * If EV_VANISH supported - prepare to deliver one
			 */
			kn->kn_status |= KN_VANISHED;
		} else {
			/*
			 * Handle the legacy way to indicate that the port/portset was
			 * deallocated or left the current Mach portspace (modern technique
			 * is with an EV_VANISHED protocol).
			 *
			 * Deliver an EV_EOF event for these changes (hopefully it will get
			 * delivered before the port name recycles to the same generation
			 * count and someone tries to re-register a kevent for it or the
			 * events are udata-specific - avoiding a conflict).
			 */
			kn->kn_flags |= EV_EOF | EV_ONESHOT;
		}
		if (make_active) {
			knote_activate(kq, kn, FILTER_ACTIVE);
		}
		kqunlock(kq);
	}
}

/*
 * remove all knotes referencing a specified fd
 *
 * Entered with the proc_fd lock already held.
 * It returns the same way, but may drop it temporarily.
 */
void
knote_fdclose(struct proc *p, int fd)
{
	struct filedesc *fdt = &p->p_fd;
	struct klist *list;
	struct knote *kn;
	KNOTE_LOCK_CTX(knlc);

restart:
	list = &fdt->fd_knlist[fd];
	SLIST_FOREACH(kn, list, kn_link) {
		struct kqueue *kq = knote_get_kq(kn);

		kqlock(kq);

		if (kq->kq_p != p) {
			panic("%s: proc mismatch (kq->kq_p=%p != p=%p)",
			    __func__, kq->kq_p, p);
		}

		/*
		 * If the knote supports EV_VANISHED delivery,
		 * transition it to vanished mode (or skip over
		 * it if already vanished).
		 */
		if (kn->kn_status & KN_VANISHED) {
			kqunlock(kq);
			continue;
		}

		proc_fdunlock(p);
		if (!knote_lock(kq, kn, &knlc, KNOTE_KQ_LOCK_ON_SUCCESS)) {
			/* the knote was dropped by someone, nothing to do */
		} else if (kn->kn_status & KN_REQVANISH) {
			/*
			 * Since we have REQVANISH for this knote, we need to notify clients about
			 * the EV_VANISHED.
			 *
			 * But unlike mach ports, we want to do the detach here as well and not
			 * defer it so that we can release the iocount that is on the knote and
			 * close the fp.
			 */
			kn->kn_status |= KN_VANISHED;

			/*
			 * There may be a concurrent post happening, make sure to wait for it
			 * before we detach. knote_wait_for_post() unlocks on kq on exit
			 */
			knote_wait_for_post(kq, kn);

			knote_fops(kn)->f_detach(kn);
			if (kn->kn_is_fd) {
				fp_drop(p, (int)kn->kn_id, kn->kn_fp, 0);
			}
			kn->kn_filtid = EVFILTID_DETACHED;
			kqlock(kq);

			knote_activate(kq, kn, FILTER_ACTIVE);
			knote_unlock(kq, kn, &knlc, KNOTE_KQ_UNLOCK);
		} else {
			knote_drop(kq, kn, &knlc);
		}

		proc_fdlock(p);
		goto restart;
	}
}

/*
 * knote_fdfind - lookup a knote in the fd table for process
 *
 * If the filter is file-based, lookup based on fd index.
 * Otherwise use a hash based on the ident.
 *
 * Matching is based on kq, filter, and ident. Optionally,
 * it may also be based on the udata field in the kevent -
 * allowing multiple event registration for the file object
 * per kqueue.
 *
 * fd_knhashlock or fdlock held on entry (and exit)
 */
static struct knote *
knote_fdfind(struct kqueue *kq,
    const struct kevent_internal_s *kev,
    bool is_fd,
    struct proc *p)
{
	struct filedesc *fdp = &p->p_fd;
	struct klist *list = NULL;
	struct knote *kn = NULL;

	/*
	 * determine where to look for the knote
	 */
	if (is_fd) {
		/* fd-based knotes are linked off the fd table */
		if (kev->kei_ident < (u_int)fdp->fd_knlistsize) {
			list = &fdp->fd_knlist[kev->kei_ident];
		}
	} else if (fdp->fd_knhashmask != 0) {
		/* hash non-fd knotes here too */
		list = &fdp->fd_knhash[KN_HASH((u_long)kev->kei_ident, fdp->fd_knhashmask)];
	}

	/*
	 * scan the selected list looking for a match
	 */
	if (list != NULL) {
		SLIST_FOREACH(kn, list, kn_link) {
			if (kq == knote_get_kq(kn) &&
			    kev->kei_ident == kn->kn_id &&
			    kev->kei_filter == kn->kn_filter) {
				if (kev->kei_flags & EV_UDATA_SPECIFIC) {
					if ((kn->kn_flags & EV_UDATA_SPECIFIC) &&
					    kev->kei_udata == kn->kn_udata) {
						break; /* matching udata-specific knote */
					}
				} else if ((kn->kn_flags & EV_UDATA_SPECIFIC) == 0) {
					break; /* matching non-udata-specific knote */
				}
			}
		}
	}
	return kn;
}

/*
 * kq_add_knote- Add knote to the fd table for process
 * while checking for duplicates.
 *
 * All file-based filters associate a list of knotes by file
 * descriptor index. All other filters hash the knote by ident.
 *
 * May have to grow the table of knote lists to cover the
 * file descriptor index presented.
 *
 * fd_knhashlock and fdlock unheld on entry (and exit).
 *
 * Takes a rwlock boost if inserting the knote is successful.
 */
static int
kq_add_knote(struct kqueue *kq, struct knote *kn, struct knote_lock_ctx *knlc,
    struct proc *p)
{
	struct filedesc *fdp = &p->p_fd;
	struct klist *list = NULL;
	int ret = 0;
	bool is_fd = kn->kn_is_fd;

	if (is_fd) {
		proc_fdlock(p);
	} else {
		knhash_lock(fdp);
	}

	if (knote_fdfind(kq, &kn->kn_kevent, is_fd, p) != NULL) {
		/* found an existing knote: we can't add this one */
		ret = ERESTART;
		goto out_locked;
	}

	/* knote was not found: add it now */
	if (!is_fd) {
		if (fdp->fd_knhashmask == 0) {
			u_long size = 0;

			list = hashinit(CONFIG_KN_HASHSIZE, M_KQUEUE, &size);
			if (list == NULL) {
				ret = ENOMEM;
				goto out_locked;
			}

			fdp->fd_knhash = list;
			fdp->fd_knhashmask = size;
		}

		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];
		SLIST_INSERT_HEAD(list, kn, kn_link);
		ret = 0;
		goto out_locked;
	} else {
		/* knote is fd based */

		if ((u_int)fdp->fd_knlistsize <= kn->kn_id) {
			u_int size = 0;

			/* Make sure that fd stays below current process's soft limit AND system allowed per-process limits */
			if (kn->kn_id >= (uint64_t)proc_limitgetcur_nofile(p)) {
				ret = EINVAL;
				goto out_locked;
			}
			/* have to grow the fd_knlist */
			size = fdp->fd_knlistsize;
			while (size <= kn->kn_id) {
				size += KQEXTENT;
			}

			if (size >= (UINT_MAX / sizeof(struct klist))) {
				ret = EINVAL;
				goto out_locked;
			}

			list = kalloc_type(struct klist, size, Z_WAITOK | Z_ZERO);
			if (list == NULL) {
				ret = ENOMEM;
				goto out_locked;
			}

			bcopy(fdp->fd_knlist, list,
			    fdp->fd_knlistsize * sizeof(struct klist));
			kfree_type(struct klist, fdp->fd_knlistsize, fdp->fd_knlist);
			fdp->fd_knlist = list;
			fdp->fd_knlistsize = size;
		}

		list = &fdp->fd_knlist[kn->kn_id];
		SLIST_INSERT_HEAD(list, kn, kn_link);
		ret = 0;
		goto out_locked;
	}

out_locked:
	if (ret == 0) {
		kqlock(kq);
		assert((kn->kn_status & KN_LOCKED) == 0);
		(void)knote_lock(kq, kn, knlc, KNOTE_KQ_UNLOCK);
		kqueue_retain(kq); /* retain a kq ref */
	}
	if (is_fd) {
		proc_fdunlock(p);
	} else {
		knhash_unlock(fdp);
	}

	return ret;
}

/*
 * kq_remove_knote - remove a knote from the fd table for process
 *
 * If the filter is file-based, remove based on fd index.
 * Otherwise remove from the hash based on the ident.
 *
 * fd_knhashlock and fdlock unheld on entry (and exit).
 */
static void
kq_remove_knote(struct kqueue *kq, struct knote *kn, struct proc *p,
    struct knote_lock_ctx *knlc)
{
	struct filedesc *fdp = &p->p_fd;
	struct klist *list = NULL;
	uint16_t kq_state;
	bool is_fd = kn->kn_is_fd;

	if (is_fd) {
		proc_fdlock(p);
	} else {
		knhash_lock(fdp);
	}

	if (is_fd) {
		assert((u_int)fdp->fd_knlistsize > kn->kn_id);
		list = &fdp->fd_knlist[kn->kn_id];
	} else {
		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];
	}
	SLIST_REMOVE(list, kn, knote, kn_link);

	kqlock(kq);

	/* Update the servicer iotier override */
	kqueue_update_iotier_override(kq);

	kq_state = kq->kq_state;
	if (knlc) {
		knote_unlock_cancel(kq, kn, knlc);
	} else {
		kqunlock(kq);
	}
	if (is_fd) {
		proc_fdunlock(p);
	} else {
		knhash_unlock(fdp);
	}

	if (kq_state & KQ_DYNAMIC) {
		kqworkloop_release((struct kqworkloop *)kq);
	}
}

/*
 * kq_find_knote_and_kq_lock - lookup a knote in the fd table for process
 * and, if the knote is found, acquires the kqlock while holding the fd table lock/spinlock.
 *
 * fd_knhashlock or fdlock unheld on entry (and exit)
 */

static struct knote *
kq_find_knote_and_kq_lock(struct kqueue *kq, struct kevent_qos_s *kev,
    bool is_fd, struct proc *p)
{
	struct filedesc *fdp = &p->p_fd;
	struct knote *kn;

	if (is_fd) {
		proc_fdlock(p);
	} else {
		knhash_lock(fdp);
	}

	/*
	 * Temporary horrible hack:
	 * this cast is gross and will go away in a future change.
	 * It is OK to do because we don't look at xflags/s_fflags,
	 * and that when we cast down the kev this way,
	 * the truncated filter field works.
	 */
	kn = knote_fdfind(kq, (struct kevent_internal_s *)kev, is_fd, p);

	if (kn) {
		kqlock(kq);
		assert(knote_get_kq(kn) == kq);
	}

	if (is_fd) {
		proc_fdunlock(p);
	} else {
		knhash_unlock(fdp);
	}

	return kn;
}

static struct kqtailq *
knote_get_tailq(kqueue_t kqu, struct knote *kn)
{
	kq_index_t qos_index = kn->kn_qos_index;

	if (kqu.kq->kq_state & KQ_WORKLOOP) {
		assert(qos_index > 0 && qos_index <= KQWL_NBUCKETS);
		return &kqu.kqwl->kqwl_queue[qos_index - 1];
	} else if (kqu.kq->kq_state & KQ_WORKQ) {
		assert(qos_index > 0 && qos_index <= KQWQ_NBUCKETS);
		return &kqu.kqwq->kqwq_queue[qos_index - 1];
	} else {
		assert(qos_index == QOS_INDEX_KQFILE);
		return &kqu.kqf->kqf_queue;
	}
}

static void
knote_enqueue(kqueue_t kqu, struct knote *kn)
{
	kqlock_held(kqu);

	if ((kn->kn_status & KN_ACTIVE) == 0) {
		return;
	}

	if (kn->kn_status & (KN_DISABLED | KN_SUPPRESSED | KN_DROPPING | KN_QUEUED)) {
		return;
	}

	struct kqtailq *queue = knote_get_tailq(kqu, kn);
	bool wakeup = TAILQ_EMPTY(queue);

	TAILQ_INSERT_TAIL(queue, kn, kn_tqe);
	kn->kn_status |= KN_QUEUED;
	kqu.kq->kq_count++;

	if (wakeup) {
		if (kqu.kq->kq_state & KQ_WORKLOOP) {
			kqworkloop_wakeup(kqu.kqwl, kn->kn_qos_index);
		} else if (kqu.kq->kq_state & KQ_WORKQ) {
			kqworkq_wakeup(kqu.kqwq, kn->kn_qos_index);
		} else {
			kqfile_wakeup(kqu.kqf, 0, THREAD_AWAKENED);
		}
	}
}

__attribute__((always_inline))
static inline void
knote_dequeue(kqueue_t kqu, struct knote *kn)
{
	if (kn->kn_status & KN_QUEUED) {
		struct kqtailq *queue = knote_get_tailq(kqu, kn);

		// attaching the knote calls knote_reset_priority() without
		// the kqlock which is fine, so we can't call kqlock_held()
		// if we're not queued.
		kqlock_held(kqu);

		TAILQ_REMOVE(queue, kn, kn_tqe);
		kn->kn_status &= ~KN_QUEUED;
		kqu.kq->kq_count--;
		if ((kqu.kq->kq_state & (KQ_WORKQ | KQ_WORKLOOP)) == 0) {
			assert((kqu.kq->kq_count == 0) ==
			    (bool)TAILQ_EMPTY(queue));
		}
	}
}

/* called with kqueue lock held */
static void
knote_suppress(kqueue_t kqu, struct knote *kn)
{
	struct kqtailq *suppressq;

	kqlock_held(kqu);

	assert((kn->kn_status & KN_SUPPRESSED) == 0);
	assert(kn->kn_status & KN_QUEUED);

	knote_dequeue(kqu, kn);
	/* deactivate - so new activations indicate a wakeup */
	kn->kn_status &= ~KN_ACTIVE;
	kn->kn_status |= KN_SUPPRESSED;
	suppressq = kqueue_get_suppressed_queue(kqu, kn);
	TAILQ_INSERT_TAIL(suppressq, kn, kn_tqe);
}

__attribute__((always_inline))
static inline void
knote_unsuppress_noqueue(kqueue_t kqu, struct knote *kn)
{
	struct kqtailq *suppressq;

	kqlock_held(kqu);

	assert(kn->kn_status & KN_SUPPRESSED);

	kn->kn_status &= ~KN_SUPPRESSED;
	suppressq = kqueue_get_suppressed_queue(kqu, kn);
	TAILQ_REMOVE(suppressq, kn, kn_tqe);

	/*
	 * If the knote is no longer active, reset its push,
	 * and resynchronize kn_qos_index with kn_qos_override
	 * for knotes with a real qos.
	 */
	if ((kn->kn_status & KN_ACTIVE) == 0 && knote_has_qos(kn)) {
		kn->kn_qos_override = _pthread_priority_thread_qos_fast(kn->kn_qos);
	}
	kn->kn_qos_index = kn->kn_qos_override;
}

/* called with kqueue lock held */
static void
knote_unsuppress(kqueue_t kqu, struct knote *kn)
{
	knote_unsuppress_noqueue(kqu, kn);
	knote_enqueue(kqu, kn);
}

__attribute__((always_inline))
static inline void
knote_mark_active(struct knote *kn)
{
	if ((kn->kn_status & KN_ACTIVE) == 0) {
		KDBG_DEBUG(KEV_EVTID(BSD_KEVENT_KNOTE_ACTIVATE),
		    kn->kn_udata, kn->kn_status | (kn->kn_id << 32),
		    kn->kn_filtid);
	}

	kn->kn_status |= KN_ACTIVE;
}

/* called with kqueue lock held */
static void
knote_activate(kqueue_t kqu, struct knote *kn, int result)
{
	assert(result & FILTER_ACTIVE);
	if (result & FILTER_ADJUST_EVENT_QOS_BIT) {
		// may dequeue the knote
		knote_adjust_qos(kqu.kq, kn, result);
	}
	knote_mark_active(kn);
	knote_enqueue(kqu, kn);
}

/*
 * This function applies changes requested by f_attach or f_touch for
 * a given filter. It proceeds in a carefully chosen order to help
 * every single transition do the minimal amount of work possible.
 */
static void
knote_apply_touch(kqueue_t kqu, struct knote *kn, struct kevent_qos_s *kev,
    int result)
{
	if ((kev->flags & EV_ENABLE) && (kn->kn_status & KN_DISABLED)) {
		kn->kn_status &= ~KN_DISABLED;

		/*
		 * it is possible for userland to have knotes registered for a given
		 * workloop `wl_orig` but really handled on another workloop `wl_new`.
		 *
		 * In that case, rearming will happen from the servicer thread of
		 * `wl_new` which if `wl_orig` is no longer being serviced, would cause
		 * this knote to stay suppressed forever if we only relied on
		 * kqworkloop_acknowledge_events to be called by `wl_orig`.
		 *
		 * However if we see the KQ_PROCESSING bit on `wl_orig` set, we can't
		 * unsuppress because that would mess with the processing phase of
		 * `wl_orig`, however it also means kqworkloop_acknowledge_events()
		 * will be called.
		 */
		if (__improbable(kn->kn_status & KN_SUPPRESSED)) {
			if ((kqu.kq->kq_state & KQ_PROCESSING) == 0) {
				knote_unsuppress_noqueue(kqu, kn);
			}
		}
	}

	if (result & FILTER_ADJUST_EVENT_IOTIER_BIT) {
		kqueue_update_iotier_override(kqu);
	}

	if ((result & FILTER_UPDATE_REQ_QOS) && kev->qos && kev->qos != kn->kn_qos) {
		// may dequeue the knote
		knote_reset_priority(kqu, kn, kev->qos);
	}

	/*
	 * When we unsuppress above, or because of knote_reset_priority(),
	 * the knote may have been dequeued, we need to restore the invariant
	 * that if the knote is active it needs to be queued now that
	 * we're done applying changes.
	 */
	if (result & FILTER_ACTIVE) {
		knote_activate(kqu, kn, result);
	} else {
		knote_enqueue(kqu, kn);
	}

	if ((result & FILTER_THREADREQ_NODEFEER) &&
	    act_clear_astkevent(current_thread(), AST_KEVENT_REDRIVE_THREADREQ)) {
		workq_kern_threadreq_redrive(kqu.kq->kq_p, WORKQ_THREADREQ_NONE);
	}
}

/*
 * knote_drop - disconnect and drop the knote
 *
 * Called with the kqueue locked, returns with the kqueue unlocked.
 *
 * If a knote locking context is passed, it is canceled.
 *
 * The knote may have already been detached from
 * (or not yet attached to) its source object.
 */
static void
knote_drop(struct kqueue *kq, struct knote *kn, struct knote_lock_ctx *knlc)
{
	struct proc *p = kq->kq_p;

	kqlock_held(kq);

	assert((kn->kn_status & KN_DROPPING) == 0);
	if (knlc == NULL) {
		assert((kn->kn_status & KN_LOCKED) == 0);
	}
	kn->kn_status |= KN_DROPPING;

	if (kn->kn_status & KN_SUPPRESSED) {
		knote_unsuppress_noqueue(kq, kn);
	} else {
		knote_dequeue(kq, kn);
	}
	knote_wait_for_post(kq, kn);

	/* Even if we are autodetached, the filter may need to do cleanups of any
	 * stuff stashed on the knote so always make the call and let each filter
	 * handle the possibility of autodetached-ness */
	knote_fops(kn)->f_detach(kn);

	/* kq may be freed when kq_remove_knote() returns */
	kq_remove_knote(kq, kn, p, knlc);
	if (kn->kn_is_fd && ((kn->kn_status & KN_VANISHED) == 0)) {
		fp_drop(p, (int)kn->kn_id, kn->kn_fp, 0);
	}

	knote_free(kn);
}

void
knote_init(void)
{
#if CONFIG_MEMORYSTATUS
	/* Initialize the memorystatus list lock */
	memorystatus_kevent_init(&kq_lck_grp, LCK_ATTR_NULL);
#endif
}
SYSINIT(knote, SI_SUB_PSEUDO, SI_ORDER_ANY, knote_init, NULL);

const struct filterops *
knote_fops(struct knote *kn)
{
	return sysfilt_ops[kn->kn_filtid];
}

static struct knote *
knote_alloc(void)
{
	return zalloc_flags(knote_zone, Z_WAITOK | Z_ZERO | Z_NOFAIL);
}

static void
knote_free(struct knote *kn)
{
	assert((kn->kn_status & (KN_LOCKED | KN_POSTING)) == 0);
	zfree(knote_zone, kn);
}

#pragma mark - syscalls: kevent, kevent64, kevent_qos, kevent_id

kevent_ctx_t
kevent_get_context(thread_t thread)
{
	uthread_t ut = get_bsdthread_info(thread);
	return &ut->uu_save.uus_kevent;
}

static inline bool
kevent_args_requesting_events(unsigned int flags, int nevents)
{
	return !(flags & KEVENT_FLAG_ERROR_EVENTS) && nevents > 0;
}

static inline int
kevent_adjust_flags_for_proc(proc_t p, int flags)
{
	__builtin_assume(p);
	return flags | (IS_64BIT_PROCESS(p) ? KEVENT_FLAG_PROC64 : 0);
}

/*!
 * @function kevent_get_kqfile
 *
 * @brief
 * Lookup a kqfile by fd.
 *
 * @discussion
 * Callers: kevent, kevent64, kevent_qos
 *
 * This is not assumed to be a fastpath (kqfile interfaces are legacy)
 */
OS_NOINLINE
static int
kevent_get_kqfile(struct proc *p, int fd, int flags,
    struct fileproc **fpp, struct kqueue **kqp)
{
	int error = 0;
	struct kqueue *kq;

	error = fp_get_ftype(p, fd, DTYPE_KQUEUE, EBADF, fpp);
	if (__improbable(error)) {
		return error;
	}
	kq = (struct kqueue *)fp_get_data((*fpp));

	uint16_t kq_state = os_atomic_load(&kq->kq_state, relaxed);
	if (__improbable((kq_state & (KQ_KEV32 | KQ_KEV64 | KQ_KEV_QOS)) == 0)) {
		kqlock(kq);
		kq_state = kq->kq_state;
		if (!(kq_state & (KQ_KEV32 | KQ_KEV64 | KQ_KEV_QOS))) {
			if (flags & KEVENT_FLAG_LEGACY32) {
				kq_state |= KQ_KEV32;
			} else if (flags & KEVENT_FLAG_LEGACY64) {
				kq_state |= KQ_KEV64;
			} else {
				kq_state |= KQ_KEV_QOS;
			}
			kq->kq_state = kq_state;
		}
		kqunlock(kq);
	}

	/*
	 * kqfiles can't be used through the legacy kevent()
	 * and other interfaces at the same time.
	 */
	if (__improbable((bool)(flags & KEVENT_FLAG_LEGACY32) !=
	    (bool)(kq_state & KQ_KEV32))) {
		fp_drop(p, fd, *fpp, 0);
		return EINVAL;
	}

	*kqp = kq;
	return 0;
}

/*!
 * @function kevent_get_kqwq
 *
 * @brief
 * Lookup or create the process kqwq (faspath).
 *
 * @discussion
 * Callers: kevent64, kevent_qos
 */
OS_ALWAYS_INLINE
static int
kevent_get_kqwq(proc_t p, int flags, int nevents, struct kqueue **kqp)
{
	struct kqworkq *kqwq = p->p_fd.fd_wqkqueue;

	if (__improbable(kevent_args_requesting_events(flags, nevents))) {
		return EINVAL;
	}
	if (__improbable(kqwq == NULL)) {
		kqwq = kqworkq_alloc(p, flags);
		if (__improbable(kqwq == NULL)) {
			return ENOMEM;
		}
	}

	*kqp = &kqwq->kqwq_kqueue;
	return 0;
}

#pragma mark kevent copyio

/*!
 * @function kevent_get_data_size
 *
 * @brief
 * Copies in the extra data size from user-space.
 */
static int
kevent_get_data_size(int flags, user_addr_t data_avail, user_addr_t data_out,
    kevent_ctx_t kectx)
{
	if (!data_avail || !data_out) {
		kectx->kec_data_size  = 0;
		kectx->kec_data_resid = 0;
	} else if (flags & KEVENT_FLAG_PROC64) {
		user64_size_t usize = 0;
		int error = copyin((user_addr_t)data_avail, &usize, sizeof(usize));
		if (__improbable(error)) {
			return error;
		}
		kectx->kec_data_resid = kectx->kec_data_size = (user_size_t)usize;
	} else {
		user32_size_t usize = 0;
		int error = copyin((user_addr_t)data_avail, &usize, sizeof(usize));
		if (__improbable(error)) {
			return error;
		}
		kectx->kec_data_avail = data_avail;
		kectx->kec_data_resid = kectx->kec_data_size = (user_size_t)usize;
	}
	kectx->kec_data_out   = data_out;
	kectx->kec_data_avail = data_avail;
	return 0;
}

/*!
 * @function kevent_put_data_size
 *
 * @brief
 * Copies out the residual data size to user-space if any has been used.
 */
static int
kevent_put_data_size(unsigned int flags, kevent_ctx_t kectx)
{
	if (kectx->kec_data_resid == kectx->kec_data_size) {
		return 0;
	}
	if (flags & KEVENT_FLAG_KERNEL) {
		*(user_size_t *)(uintptr_t)kectx->kec_data_avail = kectx->kec_data_resid;
		return 0;
	}
	if (flags & KEVENT_FLAG_PROC64) {
		user64_size_t usize = (user64_size_t)kectx->kec_data_resid;
		return copyout(&usize, (user_addr_t)kectx->kec_data_avail, sizeof(usize));
	} else {
		user32_size_t usize = (user32_size_t)kectx->kec_data_resid;
		return copyout(&usize, (user_addr_t)kectx->kec_data_avail, sizeof(usize));
	}
}

/*!
 * @function kevent_legacy_copyin
 *
 * @brief
 * Handles the copyin of a kevent/kevent64 event.
 */
static int
kevent_legacy_copyin(user_addr_t *addrp, struct kevent_qos_s *kevp, unsigned int flags)
{
	int error;

	assert((flags & (KEVENT_FLAG_LEGACY32 | KEVENT_FLAG_LEGACY64)) != 0);

	if (flags & KEVENT_FLAG_LEGACY64) {
		struct kevent64_s kev64;

		error = copyin(*addrp, (caddr_t)&kev64, sizeof(kev64));
		if (__improbable(error)) {
			return error;
		}
		*addrp += sizeof(kev64);
		*kevp = (struct kevent_qos_s){
			.ident  = kev64.ident,
			.filter = kev64.filter,
			/* Make sure user doesn't pass in any system flags */
			.flags  = kev64.flags & ~EV_SYSFLAGS,
			.udata  = kev64.udata,
			.fflags = kev64.fflags,
			.data   = kev64.data,
			.ext[0] = kev64.ext[0],
			.ext[1] = kev64.ext[1],
		};
	} else if (flags & KEVENT_FLAG_PROC64) {
		struct user64_kevent kev64;

		error = copyin(*addrp, (caddr_t)&kev64, sizeof(kev64));
		if (__improbable(error)) {
			return error;
		}
		*addrp += sizeof(kev64);
		*kevp = (struct kevent_qos_s){
			.ident  = kev64.ident,
			.filter = kev64.filter,
			/* Make sure user doesn't pass in any system flags */
			.flags  = kev64.flags & ~EV_SYSFLAGS,
			.udata  = kev64.udata,
			.fflags = kev64.fflags,
			.data   = kev64.data,
		};
	} else {
		struct user32_kevent kev32;

		error = copyin(*addrp, (caddr_t)&kev32, sizeof(kev32));
		if (__improbable(error)) {
			return error;
		}
		*addrp += sizeof(kev32);
		*kevp = (struct kevent_qos_s){
			.ident  = (uintptr_t)kev32.ident,
			.filter = kev32.filter,
			/* Make sure user doesn't pass in any system flags */
			.flags  = kev32.flags & ~EV_SYSFLAGS,
			.udata  = CAST_USER_ADDR_T(kev32.udata),
			.fflags = kev32.fflags,
			.data   = (intptr_t)kev32.data,
		};
	}

	return 0;
}

/*!
 * @function kevent_modern_copyin
 *
 * @brief
 * Handles the copyin of a kevent_qos/kevent_id event.
 */
static int
kevent_modern_copyin(user_addr_t *addrp, struct kevent_qos_s *kevp)
{
	int error = copyin(*addrp, (caddr_t)kevp, sizeof(struct kevent_qos_s));
	if (__probable(!error)) {
		/* Make sure user doesn't pass in any system flags */
		*addrp += sizeof(struct kevent_qos_s);
		kevp->flags &= ~EV_SYSFLAGS;
	}
	return error;
}

/*!
 * @function kevent_legacy_copyout
 *
 * @brief
 * Handles the copyout of a kevent/kevent64 event.
 */
static int
kevent_legacy_copyout(struct kevent_qos_s *kevp, user_addr_t *addrp, unsigned int flags)
{
	int advance;
	int error;

	assert((flags & (KEVENT_FLAG_LEGACY32 | KEVENT_FLAG_LEGACY64)) != 0);

	/*
	 * fully initialize the differnt output event structure
	 * types from the internal kevent (and some universal
	 * defaults for fields not represented in the internal
	 * form).
	 *
	 * Note: these structures have no padding hence the C99
	 *       initializers below do not leak kernel info.
	 */
	if (flags & KEVENT_FLAG_LEGACY64) {
		struct kevent64_s kev64 = {
			.ident  = kevp->ident,
			.filter = kevp->filter,
			.flags  = kevp->flags,
			.fflags = kevp->fflags,
			.data   = (int64_t)kevp->data,
			.udata  = kevp->udata,
			.ext[0] = kevp->ext[0],
			.ext[1] = kevp->ext[1],
		};
		advance = sizeof(struct kevent64_s);
		error = copyout((caddr_t)&kev64, *addrp, advance);
	} else if (flags & KEVENT_FLAG_PROC64) {
		/*
		 * deal with the special case of a user-supplied
		 * value of (uintptr_t)-1.
		 */
		uint64_t ident = (kevp->ident == (uintptr_t)-1) ?
		    (uint64_t)-1LL : (uint64_t)kevp->ident;
		struct user64_kevent kev64 = {
			.ident  = ident,
			.filter = kevp->filter,
			.flags  = kevp->flags,
			.fflags = kevp->fflags,
			.data   = (int64_t) kevp->data,
			.udata  = (user_addr_t) kevp->udata,
		};
		advance = sizeof(kev64);
		error = copyout((caddr_t)&kev64, *addrp, advance);
	} else {
		struct user32_kevent kev32 = {
			.ident  = (uint32_t)kevp->ident,
			.filter = kevp->filter,
			.flags  = kevp->flags,
			.fflags = kevp->fflags,
			.data   = (int32_t)kevp->data,
			.udata  = (uint32_t)kevp->udata,
		};
		advance = sizeof(kev32);
		error = copyout((caddr_t)&kev32, *addrp, advance);
	}
	if (__probable(!error)) {
		*addrp += advance;
	}
	return error;
}

/*!
 * @function kevent_modern_copyout
 *
 * @brief
 * Handles the copyout of a kevent_qos/kevent_id event.
 */
OS_ALWAYS_INLINE
static inline int
kevent_modern_copyout(struct kevent_qos_s *kevp, user_addr_t *addrp)
{
	int error = copyout((caddr_t)kevp, *addrp, sizeof(struct kevent_qos_s));
	if (__probable(!error)) {
		*addrp += sizeof(struct kevent_qos_s);
	}
	return error;
}

#pragma mark kevent core implementation

/*!
 * @function kevent_callback_inline
 *
 * @brief
 * Callback for each individual event
 *
 * @discussion
 * This is meant to be inlined in kevent_modern_callback and
 * kevent_legacy_callback.
 */
OS_ALWAYS_INLINE
static inline int
kevent_callback_inline(struct kevent_qos_s *kevp, kevent_ctx_t kectx, bool legacy)
{
	int error;

	assert(kectx->kec_process_noutputs < kectx->kec_process_nevents);

	/*
	 * Copy out the appropriate amount of event data for this user.
	 */
	if (legacy) {
		error = kevent_legacy_copyout(kevp, &kectx->kec_process_eventlist,
		    kectx->kec_process_flags);
	} else {
		error = kevent_modern_copyout(kevp, &kectx->kec_process_eventlist);
	}

	/*
	 * If there isn't space for additional events, return
	 * a harmless error to stop the processing here
	 */
	if (error == 0 && ++kectx->kec_process_noutputs == kectx->kec_process_nevents) {
		error = EWOULDBLOCK;
	}
	return error;
}

/*!
 * @function kevent_modern_callback
 *
 * @brief
 * Callback for each individual modern event.
 *
 * @discussion
 * This callback handles kevent_qos/kevent_id events.
 */
static int
kevent_modern_callback(struct kevent_qos_s *kevp, kevent_ctx_t kectx)
{
	return kevent_callback_inline(kevp, kectx, /*legacy*/ false);
}

/*!
 * @function kevent_legacy_callback
 *
 * @brief
 * Callback for each individual legacy event.
 *
 * @discussion
 * This callback handles kevent/kevent64 events.
 */
static int
kevent_legacy_callback(struct kevent_qos_s *kevp, kevent_ctx_t kectx)
{
	return kevent_callback_inline(kevp, kectx, /*legacy*/ true);
}

/*!
 * @function kevent_cleanup
 *
 * @brief
 * Handles the cleanup returning from a kevent call.
 *
 * @discussion
 * kevent entry points will take a reference on workloops,
 * and a usecount on the fileglob of kqfiles.
 *
 * This function undoes this on the exit paths of kevents.
 *
 * @returns
 * The error to return to userspace.
 */
static int
kevent_cleanup(kqueue_t kqu, int flags, int error, kevent_ctx_t kectx)
{
	// poll should not call any codepath leading to this
	assert((flags & KEVENT_FLAG_POLL) == 0);

	if (flags & KEVENT_FLAG_WORKLOOP) {
		kqworkloop_release(kqu.kqwl);
	} else if (flags & KEVENT_FLAG_WORKQ) {
		/* nothing held */
	} else {
		fp_drop(kqu.kqf->kqf_p, kectx->kec_fd, kectx->kec_fp, 0);
	}

	/* don't restart after signals... */
	if (error == ERESTART) {
		error = EINTR;
	} else if (error == 0) {
		/* don't abandon other output just because of residual copyout failures */
		(void)kevent_put_data_size(flags, kectx);
	}

	if (flags & KEVENT_FLAG_PARKING) {
		thread_t th = current_thread();
		struct uthread *uth = get_bsdthread_info(th);
		if (uth->uu_kqr_bound) {
			thread_unfreeze_base_pri(th);
		}
	}
	return error;
}

/*!
 * @function kqueue_process
 *
 * @brief
 * Process the triggered events in a kqueue.
 *
 * @discussion
 * Walk the queued knotes and validate that they are really still triggered
 * events by calling the filter routines (if necessary).
 *
 * For each event that is still considered triggered, invoke the callback
 * routine provided.
 *
 * caller holds a reference on the kqueue.
 * kqueue locked on entry and exit - but may be dropped
 * kqueue list locked (held for duration of call)
 *
 * This is only called by kqueue_scan() so that the compiler can inline it.
 *
 * @returns
 * - 0:            no event was returned, no other error occured
 * - EBADF:        the kqueue is being destroyed (KQ_DRAIN is set)
 * - EWOULDBLOCK:  (not an error) events have been found and we should return
 * - EFAULT:       copyout failed
 * - filter specific errors
 */
static int
kqueue_process(kqueue_t kqu, int flags, kevent_ctx_t kectx,
    kevent_callback_t callback)
{
	workq_threadreq_t kqr = current_uthread()->uu_kqr_bound;
	struct knote *kn;
	int error = 0, rc = 0;
	struct kqtailq *base_queue, *queue;
	uint16_t kq_type = (kqu.kq->kq_state & (KQ_WORKQ | KQ_WORKLOOP));

	if (kq_type & KQ_WORKQ) {
		rc = kqworkq_begin_processing(kqu.kqwq, kqr, flags);
	} else if (kq_type & KQ_WORKLOOP) {
		rc = kqworkloop_begin_processing(kqu.kqwl, flags);
	} else {
kqfile_retry:
		rc = kqfile_begin_processing(kqu.kqf);
		if (rc == EBADF) {
			return EBADF;
		}
	}

	if (rc == -1) {
		/* Nothing to process */
		return 0;
	}

	/*
	 * loop through the enqueued knotes associated with this request,
	 * processing each one. Each request may have several queues
	 * of knotes to process (depending on the type of kqueue) so we
	 * have to loop through all the queues as long as we have additional
	 * space.
	 */

process_again:
	if (kq_type & KQ_WORKQ) {
		base_queue = queue = &kqu.kqwq->kqwq_queue[kqr->tr_kq_qos_index - 1];
	} else if (kq_type & KQ_WORKLOOP) {
		base_queue = &kqu.kqwl->kqwl_queue[0];
		queue = &kqu.kqwl->kqwl_queue[KQWL_NBUCKETS - 1];
	} else {
		base_queue = queue = &kqu.kqf->kqf_queue;
	}

	do {
		while ((kn = TAILQ_FIRST(queue)) != NULL) {
			error = knote_process(kn, kectx, callback);
			if (error == EJUSTRETURN) {
				error = 0;
			} else if (__improbable(error)) {
				/* error is EWOULDBLOCK when the out event array is full */
				goto stop_processing;
			}
		}
	} while (queue-- > base_queue);

	if (kectx->kec_process_noutputs) {
		/* callers will transform this into no error */
		error = EWOULDBLOCK;
	}

stop_processing:
	/*
	 * If KEVENT_FLAG_PARKING is set, and no kevents have been returned,
	 * we want to unbind the kqrequest from the thread.
	 *
	 * However, because the kq locks are dropped several times during process,
	 * new knotes may have fired again, in which case, we want to fail the end
	 * processing and process again, until it converges.
	 *
	 * If we have an error or returned events, end processing never fails.
	 */
	if (error) {
		flags &= ~KEVENT_FLAG_PARKING;
	}
	if (kq_type & KQ_WORKQ) {
		rc = kqworkq_end_processing(kqu.kqwq, kqr, flags);
	} else if (kq_type & KQ_WORKLOOP) {
		rc = kqworkloop_end_processing(kqu.kqwl, KQ_PROCESSING, flags);
	} else {
		rc = kqfile_end_processing(kqu.kqf);
	}

	if (__probable(error)) {
		return error;
	}

	if (__probable(rc >= 0)) {
		assert(rc == 0 || rc == EBADF);
		return rc;
	}

	if (kq_type & (KQ_WORKQ | KQ_WORKLOOP)) {
		assert(flags & KEVENT_FLAG_PARKING);
		goto process_again;
	} else {
		goto kqfile_retry;
	}
}

/*!
 * @function kqueue_scan_continue
 *
 * @brief
 * The continuation used by kqueue_scan for kevent entry points.
 *
 * @discussion
 * Assumes we inherit a use/ref count on the kq or its fileglob.
 *
 * This is called by kqueue_scan if neither KEVENT_FLAG_POLL nor
 * KEVENT_FLAG_KERNEL was set, and the caller had to wait.
 */
OS_NORETURN OS_NOINLINE
static void
kqueue_scan_continue(void *data, wait_result_t wait_result)
{
	uthread_t ut = current_uthread();
	kevent_ctx_t kectx = &ut->uu_save.uus_kevent;
	int error = 0, flags = kectx->kec_process_flags;
	struct kqueue *kq = data;

	/*
	 * only kevent variants call in here, so we know the callback is
	 * kevent_legacy_callback or kevent_modern_callback.
	 */
	assert((flags & (KEVENT_FLAG_POLL | KEVENT_FLAG_KERNEL)) == 0);

	switch (wait_result) {
	case THREAD_AWAKENED:
		if (__improbable(flags & (KEVENT_FLAG_LEGACY32 | KEVENT_FLAG_LEGACY64))) {
			error = kqueue_scan(kq, flags, kectx, kevent_legacy_callback);
		} else {
			error = kqueue_scan(kq, flags, kectx, kevent_modern_callback);
		}
		break;
	case THREAD_TIMED_OUT:
		error = 0;
		break;
	case THREAD_INTERRUPTED:
		error = EINTR;
		break;
	case THREAD_RESTART:
		error = EBADF;
		break;
	default:
		panic("%s: - invalid wait_result (%d)", __func__, wait_result);
	}


	error = kevent_cleanup(kq, flags, error, kectx);
	*(int32_t *)&ut->uu_rval = kectx->kec_process_noutputs;
	unix_syscall_return(error);
}

/*!
 * @function kqueue_scan
 *
 * @brief
 * Scan and wait for events in a kqueue (used by poll & kevent).
 *
 * @discussion
 * Process the triggered events in a kqueue.
 *
 * If there are no events triggered arrange to wait for them:
 * - unless KEVENT_FLAG_IMMEDIATE is set in kectx->kec_process_flags
 * - possibly until kectx->kec_deadline expires
 *
 * When it waits, and that neither KEVENT_FLAG_POLL nor KEVENT_FLAG_KERNEL
 * are set, then it will wait in the kqueue_scan_continue continuation.
 *
 * poll() will block in place, and KEVENT_FLAG_KERNEL calls
 * all pass KEVENT_FLAG_IMMEDIATE and will not wait.
 *
 * @param kqu
 * The kqueue being scanned.
 *
 * @param flags
 * The KEVENT_FLAG_* flags for this call.
 *
 * @param kectx
 * The context used for this scan.
 * The uthread_t::uu_save.uus_kevent storage is used for this purpose.
 *
 * @param callback
 * The callback to be called on events sucessfully processed.
 * (Either kevent_legacy_callback, kevent_modern_callback or poll_callback)
 */
int
kqueue_scan(kqueue_t kqu, int flags, kevent_ctx_t kectx,
    kevent_callback_t callback)
{
	int error;

	for (;;) {
		kqlock(kqu);
		error = kqueue_process(kqu, flags, kectx, callback);

		/*
		 * If we got an error, events returned (EWOULDBLOCK)
		 * or blocking was disallowed (KEVENT_FLAG_IMMEDIATE),
		 * just return.
		 */
		if (__probable(error || (flags & KEVENT_FLAG_IMMEDIATE))) {
			kqunlock(kqu);
			return error == EWOULDBLOCK ? 0 : error;
		}

		assert((kqu.kq->kq_state & (KQ_WORKQ | KQ_WORKLOOP)) == 0);

		kqu.kqf->kqf_state |= KQ_SLEEP;
		assert_wait_deadline(&kqu.kqf->kqf_count, THREAD_ABORTSAFE,
		    kectx->kec_deadline);
		kqunlock(kqu);

		if (__probable((flags & (KEVENT_FLAG_POLL | KEVENT_FLAG_KERNEL)) == 0)) {
			thread_block_parameter(kqueue_scan_continue, kqu.kqf);
			__builtin_unreachable();
		}

		wait_result_t wr = thread_block(THREAD_CONTINUE_NULL);
		switch (wr) {
		case THREAD_AWAKENED:
			break;
		case THREAD_TIMED_OUT:
			return 0;
		case THREAD_INTERRUPTED:
			return EINTR;
		case THREAD_RESTART:
			return EBADF;
		default:
			panic("%s: - bad wait_result (%d)", __func__, wr);
		}
	}
}

/*!
 * @function kevent_internal
 *
 * @brief
 * Common kevent code.
 *
 * @discussion
 * Needs to be inlined to specialize for legacy or modern and
 * eliminate dead code.
 *
 * This is the core logic of kevent entry points, that will:
 * - register kevents
 * - optionally scan the kqueue for events
 *
 * The caller is giving kevent_internal a reference on the kqueue
 * or its fileproc that needs to be cleaned up by kevent_cleanup().
 */
OS_ALWAYS_INLINE
static inline int
kevent_internal(kqueue_t kqu,
    user_addr_t changelist, int nchanges,
    user_addr_t ueventlist, int nevents,
    int flags, kevent_ctx_t kectx, int32_t *retval,
    bool legacy)
{
	int error = 0, noutputs = 0, register_rc;

	/* only bound threads can receive events on workloops */
	if (!legacy && (flags & KEVENT_FLAG_WORKLOOP)) {
#if CONFIG_WORKLOOP_DEBUG
		UU_KEVENT_HISTORY_WRITE_ENTRY(current_uthread(), {
			.uu_kqid = kqu.kqwl->kqwl_dynamicid,
			.uu_kq = error ? NULL : kqu.kq,
			.uu_error = error,
			.uu_nchanges = nchanges,
			.uu_nevents = nevents,
			.uu_flags = flags,
		});
#endif // CONFIG_WORKLOOP_DEBUG

		if (flags & KEVENT_FLAG_KERNEL) {
			/* see kevent_workq_internal */
			error = copyout(&kqu.kqwl->kqwl_dynamicid,
			    ueventlist - sizeof(kqueue_id_t), sizeof(kqueue_id_t));
			kectx->kec_data_resid -= sizeof(kqueue_id_t);
			if (__improbable(error)) {
				goto out;
			}
		}

		if (kevent_args_requesting_events(flags, nevents)) {
			/*
			 * Disable the R2K notification while doing a register, if the
			 * caller wants events too, we don't want the AST to be set if we
			 * will process these events soon.
			 */
			kqlock(kqu);
			kqu.kq->kq_state &= ~KQ_R2K_ARMED;
			kqunlock(kqu);
			flags |= KEVENT_FLAG_NEEDS_END_PROCESSING;
		}
	}

	/* register all the change requests the user provided... */
	while (nchanges > 0 && error == 0) {
		struct kevent_qos_s kev;
		struct knote *kn = NULL;

		if (legacy) {
			error = kevent_legacy_copyin(&changelist, &kev, flags);
		} else {
			error = kevent_modern_copyin(&changelist, &kev);
		}
		if (error) {
			break;
		}

		register_rc = kevent_register(kqu.kq, &kev, &kn);
		if (__improbable(!legacy && (register_rc & FILTER_REGISTER_WAIT))) {
			thread_t thread = current_thread();

			kqlock_held(kqu);

			if (act_clear_astkevent(thread, AST_KEVENT_REDRIVE_THREADREQ)) {
				workq_kern_threadreq_redrive(kqu.kq->kq_p, WORKQ_THREADREQ_NONE);
			}

			// f_post_register_wait is meant to call a continuation and not to
			// return, which is why we don't support FILTER_REGISTER_WAIT if
			// KEVENT_FLAG_ERROR_EVENTS is not passed, or if the event that
			// waits isn't the last.
			//
			// It is implementable, but not used by any userspace code at the
			// moment, so for now return ENOTSUP if someone tries to do it.
			if (nchanges == 1 && noutputs < nevents &&
			    (flags & KEVENT_FLAG_KERNEL) == 0 &&
			    (flags & KEVENT_FLAG_PARKING) == 0 &&
			    (flags & KEVENT_FLAG_ERROR_EVENTS) &&
			    (flags & KEVENT_FLAG_WORKLOOP)) {
				uthread_t ut = get_bsdthread_info(thread);

				/*
				 * store the continuation/completion data in the uthread
				 *
				 * Note: the kectx aliases with this,
				 * and is destroyed in the process.
				 */
				ut->uu_save.uus_kevent_register = (struct _kevent_register){
					.kev        = kev,
					.kqwl       = kqu.kqwl,
					.eventout   = noutputs,
					.ueventlist = ueventlist,
				};
				knote_fops(kn)->f_post_register_wait(ut, kn,
				    &ut->uu_save.uus_kevent_register);
				__builtin_unreachable();
			}
			kqunlock(kqu);

			kev.flags |= EV_ERROR;
			kev.data = ENOTSUP;
		} else {
			assert((register_rc & FILTER_REGISTER_WAIT) == 0);
		}

		// keep in sync with kevent_register_wait_return()
		if (noutputs < nevents && (kev.flags & (EV_ERROR | EV_RECEIPT))) {
			if ((kev.flags & EV_ERROR) == 0) {
				kev.flags |= EV_ERROR;
				kev.data = 0;
			}
			if (legacy) {
				error = kevent_legacy_copyout(&kev, &ueventlist, flags);
			} else {
				error = kevent_modern_copyout(&kev, &ueventlist);
			}
			if (error == 0) {
				noutputs++;
			}
		} else if (kev.flags & EV_ERROR) {
			error = (int)kev.data;
		}
		nchanges--;
	}

	if ((flags & KEVENT_FLAG_ERROR_EVENTS) == 0 &&
	    nevents > 0 && noutputs == 0 && error == 0) {
		kectx->kec_process_flags = flags;
		kectx->kec_process_nevents = nevents;
		kectx->kec_process_noutputs = 0;
		kectx->kec_process_eventlist = ueventlist;

		if (legacy) {
			error = kqueue_scan(kqu.kq, flags, kectx, kevent_legacy_callback);
		} else {
			error = kqueue_scan(kqu.kq, flags, kectx, kevent_modern_callback);
		}

		noutputs = kectx->kec_process_noutputs;
	} else if (!legacy && (flags & KEVENT_FLAG_NEEDS_END_PROCESSING)) {
		/*
		 * If we didn't through kqworkloop_end_processing(),
		 * we need to do it here.
		 *
		 * kqueue_scan will call kqworkloop_end_processing(),
		 * so we only need to do it if we didn't scan.
		 */
		kqlock(kqu);
		kqworkloop_end_processing(kqu.kqwl, 0, 0);
		kqunlock(kqu);
	}

	*retval = noutputs;
out:
	return kevent_cleanup(kqu.kq, flags, error, kectx);
}

#pragma mark modern syscalls: kevent_qos, kevent_id, kevent_workq_internal

/*!
 * @function kevent_modern_internal
 *
 * @brief
 * The backend of the kevent_id and kevent_workq_internal entry points.
 *
 * @discussion
 * Needs to be inline due to the number of arguments.
 */
OS_NOINLINE
static int
kevent_modern_internal(kqueue_t kqu,
    user_addr_t changelist, int nchanges,
    user_addr_t ueventlist, int nevents,
    int flags, kevent_ctx_t kectx, int32_t *retval)
{
	return kevent_internal(kqu.kq, changelist, nchanges,
	           ueventlist, nevents, flags, kectx, retval, /*legacy*/ false);
}

/*!
 * @function kevent_id
 *
 * @brief
 * The kevent_id() syscall.
 */
int
kevent_id(struct proc *p, struct kevent_id_args *uap, int32_t *retval)
{
	int error, flags = uap->flags & KEVENT_FLAG_USER;
	uthread_t uth = current_uthread();
	workq_threadreq_t kqr = uth->uu_kqr_bound;
	kevent_ctx_t kectx = &uth->uu_save.uus_kevent;
	kqueue_t kqu;

	flags = kevent_adjust_flags_for_proc(p, flags);
	flags |= KEVENT_FLAG_DYNAMIC_KQUEUE;

	if (__improbable((flags & (KEVENT_FLAG_WORKQ | KEVENT_FLAG_WORKLOOP)) !=
	    KEVENT_FLAG_WORKLOOP)) {
		return EINVAL;
	}

	error = kevent_get_data_size(flags, uap->data_available, uap->data_out, kectx);
	if (__improbable(error)) {
		return error;
	}

	kectx->kec_deadline = 0;
	kectx->kec_fp       = NULL;
	kectx->kec_fd       = -1;
	/* the kec_process_* fields are filled if kqueue_scann is called only */

	/*
	 * Get the kq we are going to be working on
	 * As a fastpath, look at the currently bound workloop.
	 */
	kqu.kqwl = kqr ? kqr_kqworkloop(kqr) : NULL;
	if (kqu.kqwl && kqu.kqwl->kqwl_dynamicid == uap->id) {
		if (__improbable(flags & KEVENT_FLAG_DYNAMIC_KQ_MUST_NOT_EXIST)) {
			return EEXIST;
		}
		kqworkloop_retain(kqu.kqwl);
	} else if (__improbable(kevent_args_requesting_events(flags, uap->nevents))) {
		return EXDEV;
	} else {
		error = kqworkloop_get_or_create(p, uap->id, NULL,
#if CONFIG_PREADOPT_TG
		    NULL,
#endif /* CONFIG_PREADOPT_TG */
		    flags, &kqu.kqwl);
		if (__improbable(error)) {
			return error;
		}
	}

	return kevent_modern_internal(kqu, uap->changelist, uap->nchanges,
	           uap->eventlist, uap->nevents, flags, kectx, retval);
}

/**!
 * @function kevent_workq_internal
 *
 * @discussion
 * This function is exported for the sake of the workqueue subsystem.
 *
 * It is called in two ways:
 * - when a thread is about to go to userspace to ask for pending event
 * - when a thread is returning from userspace with events back
 *
 * the workqueue subsystem will only use the following flags:
 * - KEVENT_FLAG_STACK_DATA (always)
 * - KEVENT_FLAG_IMMEDIATE (always)
 * - KEVENT_FLAG_PARKING (depending on whether it is going to or returning from
 *   userspace).
 *
 * It implicitly acts on the bound kqueue, and for the case of workloops
 * will copyout the kqueue ID before anything else.
 *
 *
 * Pthread will have setup the various arguments to fit this stack layout:
 *
 * +-------....----+--------------+-----------+--------------------+
 * |  user stack   |  data avail  |  nevents  |   pthread_self()   |
 * +-------....----+--------------+-----------+--------------------+
 *                 ^              ^
 *             data_out       eventlist
 *
 * When a workloop is used, the workloop ID is copied out right before
 * the eventlist and is taken from the data buffer.
 *
 * @warning
 * This function is carefuly tailored to not make any call except the final tail
 * call into kevent_modern_internal. (LTO inlines current_uthread()).
 *
 * This function is performance sensitive due to the workq subsystem.
 */
int
kevent_workq_internal(struct proc *p,
    user_addr_t changelist, int nchanges,
    user_addr_t eventlist, int nevents,
    user_addr_t data_out, user_size_t *data_available,
    unsigned int flags, int32_t *retval)
{
	uthread_t uth = current_uthread();
	workq_threadreq_t kqr = uth->uu_kqr_bound;
	kevent_ctx_t kectx = &uth->uu_save.uus_kevent;
	kqueue_t kqu;

	assert(flags == (KEVENT_FLAG_STACK_DATA | KEVENT_FLAG_IMMEDIATE) ||
	    flags == (KEVENT_FLAG_STACK_DATA | KEVENT_FLAG_IMMEDIATE | KEVENT_FLAG_PARKING));

	kectx->kec_data_out   = data_out;
	kectx->kec_data_avail = (uint64_t)data_available;
	kectx->kec_data_size  = *data_available;
	kectx->kec_data_resid = *data_available;
	kectx->kec_deadline   = 0;
	kectx->kec_fp         = NULL;
	kectx->kec_fd         = -1;
	/* the kec_process_* fields are filled if kqueue_scann is called only */

	flags = kevent_adjust_flags_for_proc(p, flags);

	if (kqr->tr_flags & WORKQ_TR_FLAG_WORKLOOP) {
		kqu.kqwl = __container_of(kqr, struct kqworkloop, kqwl_request);
		kqworkloop_retain(kqu.kqwl);

		flags |= KEVENT_FLAG_WORKLOOP | KEVENT_FLAG_DYNAMIC_KQUEUE |
		    KEVENT_FLAG_KERNEL;
	} else {
		kqu.kqwq = p->p_fd.fd_wqkqueue;

		flags |= KEVENT_FLAG_WORKQ | KEVENT_FLAG_KERNEL;
	}

	return kevent_modern_internal(kqu, changelist, nchanges,
	           eventlist, nevents, flags, kectx, retval);
}

/*!
 * @function kevent_qos
 *
 * @brief
 * The kevent_qos() syscall.
 */
int
kevent_qos(struct proc *p, struct kevent_qos_args *uap, int32_t *retval)
{
	uthread_t uth = current_uthread();
	kevent_ctx_t kectx = &uth->uu_save.uus_kevent;
	int error, flags = uap->flags & KEVENT_FLAG_USER;
	struct kqueue *kq;

	if (__improbable(flags & KEVENT_ID_FLAG_USER)) {
		return EINVAL;
	}

	flags = kevent_adjust_flags_for_proc(p, flags);

	error = kevent_get_data_size(flags, uap->data_available, uap->data_out, kectx);
	if (__improbable(error)) {
		return error;
	}

	kectx->kec_deadline = 0;
	kectx->kec_fp       = NULL;
	kectx->kec_fd       = uap->fd;
	/* the kec_process_* fields are filled if kqueue_scann is called only */

	/* get the kq we are going to be working on */
	if (__probable(flags & KEVENT_FLAG_WORKQ)) {
		error = kevent_get_kqwq(p, flags, uap->nevents, &kq);
	} else {
		error = kevent_get_kqfile(p, uap->fd, flags, &kectx->kec_fp, &kq);
	}
	if (__improbable(error)) {
		return error;
	}

	return kevent_modern_internal(kq, uap->changelist, uap->nchanges,
	           uap->eventlist, uap->nevents, flags, kectx, retval);
}

#pragma mark legacy syscalls: kevent, kevent64

/*!
 * @function kevent_legacy_get_deadline
 *
 * @brief
 * Compute the deadline for the legacy kevent syscalls.
 *
 * @discussion
 * This is not necessary if KEVENT_FLAG_IMMEDIATE is specified,
 * as this takes precedence over the deadline.
 *
 * This function will fail if utimeout is USER_ADDR_NULL
 * (the caller should check).
 */
static int
kevent_legacy_get_deadline(int flags, user_addr_t utimeout, uint64_t *deadline)
{
	struct timespec ts;

	if (flags & KEVENT_FLAG_PROC64) {
		struct user64_timespec ts64;
		int error = copyin(utimeout, &ts64, sizeof(ts64));
		if (__improbable(error)) {
			return error;
		}
		ts.tv_sec = (unsigned long)ts64.tv_sec;
		ts.tv_nsec = (long)ts64.tv_nsec;
	} else {
		struct user32_timespec ts32;
		int error = copyin(utimeout, &ts32, sizeof(ts32));
		if (__improbable(error)) {
			return error;
		}
		ts.tv_sec = ts32.tv_sec;
		ts.tv_nsec = ts32.tv_nsec;
	}
	if (!timespec_is_valid(&ts)) {
		return EINVAL;
	}

	clock_absolutetime_interval_to_deadline(tstoabstime(&ts), deadline);
	return 0;
}

/*!
 * @function kevent_legacy_internal
 *
 * @brief
 * The core implementation for kevent and kevent64
 */
OS_NOINLINE
static int
kevent_legacy_internal(struct proc *p, struct kevent64_args *uap,
    int32_t *retval, int flags)
{
	uthread_t uth = current_uthread();
	kevent_ctx_t kectx = &uth->uu_save.uus_kevent;
	struct kqueue *kq;
	int error;

	if (__improbable(uap->flags & KEVENT_ID_FLAG_USER)) {
		return EINVAL;
	}

	flags = kevent_adjust_flags_for_proc(p, flags);

	kectx->kec_data_out   = 0;
	kectx->kec_data_avail = 0;
	kectx->kec_data_size  = 0;
	kectx->kec_data_resid = 0;
	kectx->kec_deadline   = 0;
	kectx->kec_fp         = NULL;
	kectx->kec_fd         = uap->fd;
	/* the kec_process_* fields are filled if kqueue_scann is called only */

	/* convert timeout to absolute - if we have one (and not immediate) */
	if (__improbable(uap->timeout && !(flags & KEVENT_FLAG_IMMEDIATE))) {
		error = kevent_legacy_get_deadline(flags, uap->timeout,
		    &kectx->kec_deadline);
		if (__improbable(error)) {
			return error;
		}
	}

	/* get the kq we are going to be working on */
	if (flags & KEVENT_FLAG_WORKQ) {
		error = kevent_get_kqwq(p, flags, uap->nevents, &kq);
	} else {
		error = kevent_get_kqfile(p, uap->fd, flags, &kectx->kec_fp, &kq);
	}
	if (__improbable(error)) {
		return error;
	}

	return kevent_internal(kq, uap->changelist, uap->nchanges,
	           uap->eventlist, uap->nevents, flags, kectx, retval,
	           /*legacy*/ true);
}

/*!
 * @function kevent
 *
 * @brief
 * The legacy kevent() syscall.
 */
int
kevent(struct proc *p, struct kevent_args *uap, int32_t *retval)
{
	struct kevent64_args args = {
		.fd         = uap->fd,
		.changelist = uap->changelist,
		.nchanges   = uap->nchanges,
		.eventlist  = uap->eventlist,
		.nevents    = uap->nevents,
		.timeout    = uap->timeout,
	};

	return kevent_legacy_internal(p, &args, retval, KEVENT_FLAG_LEGACY32);
}

/*!
 * @function kevent64
 *
 * @brief
 * The legacy kevent64() syscall.
 */
int
kevent64(struct proc *p, struct kevent64_args *uap, int32_t *retval)
{
	int flags = (uap->flags & KEVENT_FLAG_USER) | KEVENT_FLAG_LEGACY64;
	return kevent_legacy_internal(p, uap, retval, flags);
}

#pragma mark - socket interface

#if SOCKETS
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/kern_event.h>
#include <sys/malloc.h>
#include <sys/sys_domain.h>
#include <sys/syslog.h>

#ifndef ROUNDUP64
#define ROUNDUP64(x) P2ROUNDUP((x), sizeof (u_int64_t))
#endif

#ifndef ADVANCE64
#define ADVANCE64(p, n) (void*)((char *)(p) + ROUNDUP64(n))
#endif

static LCK_GRP_DECLARE(kev_lck_grp, "Kernel Event Protocol");
static LCK_RW_DECLARE(kev_rwlock, &kev_lck_grp);

static int kev_attach(struct socket *so, int proto, struct proc *p);
static int kev_detach(struct socket *so);
static int kev_control(struct socket *so, u_long cmd, caddr_t data,
    struct ifnet *ifp, struct proc *p);
static lck_mtx_t * event_getlock(struct socket *, int);
static int event_lock(struct socket *, int, void *);
static int event_unlock(struct socket *, int, void *);

static int event_sofreelastref(struct socket *);
static void kev_delete(struct kern_event_pcb *);

static struct pr_usrreqs event_usrreqs = {
	.pru_attach =           kev_attach,
	.pru_control =          kev_control,
	.pru_detach =           kev_detach,
	.pru_soreceive =        soreceive,
};

static struct protosw eventsw[] = {
	{
		.pr_type =              SOCK_RAW,
		.pr_protocol =          SYSPROTO_EVENT,
		.pr_flags =             PR_ATOMIC,
		.pr_usrreqs =           &event_usrreqs,
		.pr_lock =              event_lock,
		.pr_unlock =            event_unlock,
		.pr_getlock =           event_getlock,
	}
};

__private_extern__ int kevt_getstat SYSCTL_HANDLER_ARGS;
__private_extern__ int kevt_pcblist SYSCTL_HANDLER_ARGS;

SYSCTL_NODE(_net_systm, OID_AUTO, kevt,
    CTLFLAG_RW | CTLFLAG_LOCKED, 0, "Kernel event family");

struct kevtstat kevtstat;
SYSCTL_PROC(_net_systm_kevt, OID_AUTO, stats,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED, 0, 0,
    kevt_getstat, "S,kevtstat", "");

SYSCTL_PROC(_net_systm_kevt, OID_AUTO, pcblist,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED, 0, 0,
    kevt_pcblist, "S,xkevtpcb", "");

static lck_mtx_t *
event_getlock(struct socket *so, int flags)
{
#pragma unused(flags)
	struct kern_event_pcb *ev_pcb = (struct kern_event_pcb *)so->so_pcb;

	if (so->so_pcb != NULL) {
		if (so->so_usecount < 0) {
			panic("%s: so=%p usecount=%d lrh= %s", __func__,
			    so, so->so_usecount, solockhistory_nr(so));
		}
		/* NOTREACHED */
	} else {
		panic("%s: so=%p NULL NO so_pcb %s", __func__,
		    so, solockhistory_nr(so));
		/* NOTREACHED */
	}
	return &ev_pcb->evp_mtx;
}

static int
event_lock(struct socket *so, int refcount, void *lr)
{
	void *lr_saved;

	if (lr == NULL) {
		lr_saved = __builtin_return_address(0);
	} else {
		lr_saved = lr;
	}

	if (so->so_pcb != NULL) {
		lck_mtx_lock(&((struct kern_event_pcb *)so->so_pcb)->evp_mtx);
	} else {
		panic("%s: so=%p NO PCB! lr=%p lrh= %s", __func__,
		    so, lr_saved, solockhistory_nr(so));
		/* NOTREACHED */
	}

	if (so->so_usecount < 0) {
		panic("%s: so=%p so_pcb=%p lr=%p ref=%d lrh= %s", __func__,
		    so, so->so_pcb, lr_saved, so->so_usecount,
		    solockhistory_nr(so));
		/* NOTREACHED */
	}

	if (refcount) {
		so->so_usecount++;
	}

	so->lock_lr[so->next_lock_lr] = lr_saved;
	so->next_lock_lr = (so->next_lock_lr + 1) % SO_LCKDBG_MAX;
	return 0;
}

static int
event_unlock(struct socket *so, int refcount, void *lr)
{
	void *lr_saved;
	lck_mtx_t *mutex_held;

	if (lr == NULL) {
		lr_saved = __builtin_return_address(0);
	} else {
		lr_saved = lr;
	}

	if (refcount) {
		so->so_usecount--;
	}
	if (so->so_usecount < 0) {
		panic("%s: so=%p usecount=%d lrh= %s", __func__,
		    so, so->so_usecount, solockhistory_nr(so));
		/* NOTREACHED */
	}
	if (so->so_pcb == NULL) {
		panic("%s: so=%p NO PCB usecount=%d lr=%p lrh= %s", __func__,
		    so, so->so_usecount, (void *)lr_saved,
		    solockhistory_nr(so));
		/* NOTREACHED */
	}
	mutex_held = (&((struct kern_event_pcb *)so->so_pcb)->evp_mtx);

	LCK_MTX_ASSERT(mutex_held, LCK_MTX_ASSERT_OWNED);
	so->unlock_lr[so->next_unlock_lr] = lr_saved;
	so->next_unlock_lr = (so->next_unlock_lr + 1) % SO_LCKDBG_MAX;

	if (so->so_usecount == 0) {
		VERIFY(so->so_flags & SOF_PCBCLEARING);
		event_sofreelastref(so);
	} else {
		lck_mtx_unlock(mutex_held);
	}

	return 0;
}

static int
event_sofreelastref(struct socket *so)
{
	struct kern_event_pcb *ev_pcb = (struct kern_event_pcb *)so->so_pcb;

	LCK_MTX_ASSERT(&(ev_pcb->evp_mtx), LCK_MTX_ASSERT_OWNED);

	so->so_pcb = NULL;

	/*
	 * Disable upcall in the event another thread is in kev_post_msg()
	 * appending record to the receive socket buffer, since sbwakeup()
	 * may release the socket lock otherwise.
	 */
	so->so_rcv.sb_flags &= ~SB_UPCALL;
	so->so_snd.sb_flags &= ~SB_UPCALL;
	so->so_event = sonullevent;
	lck_mtx_unlock(&(ev_pcb->evp_mtx));

	LCK_MTX_ASSERT(&(ev_pcb->evp_mtx), LCK_MTX_ASSERT_NOTOWNED);
	lck_rw_lock_exclusive(&kev_rwlock);
	LIST_REMOVE(ev_pcb, evp_link);
	kevtstat.kes_pcbcount--;
	kevtstat.kes_gencnt++;
	lck_rw_done(&kev_rwlock);
	kev_delete(ev_pcb);

	sofreelastref(so, 1);
	return 0;
}

static int event_proto_count = (sizeof(eventsw) / sizeof(struct protosw));

static
struct kern_event_head kern_event_head;

static u_int32_t static_event_id = 0;

static KALLOC_TYPE_DEFINE(ev_pcb_zone, struct kern_event_pcb, NET_KT_DEFAULT);

/*
 * Install the protosw's for the NKE manager.  Invoked at extension load time
 */
void
kern_event_init(struct domain *dp)
{
	struct protosw *pr;
	int i;

	VERIFY(!(dp->dom_flags & DOM_INITIALIZED));
	VERIFY(dp == systemdomain);

	for (i = 0, pr = &eventsw[0]; i < event_proto_count; i++, pr++) {
		net_add_proto(pr, dp, 1);
	}
}

static int
kev_attach(struct socket *so, __unused int proto, __unused struct proc *p)
{
	int error = 0;
	struct kern_event_pcb *ev_pcb;

	error = soreserve(so, KEV_SNDSPACE, KEV_RECVSPACE);
	if (error != 0) {
		return error;
	}

	ev_pcb = zalloc_flags(ev_pcb_zone, Z_WAITOK | Z_ZERO);
	lck_mtx_init(&ev_pcb->evp_mtx, &kev_lck_grp, LCK_ATTR_NULL);

	ev_pcb->evp_socket = so;
	ev_pcb->evp_vendor_code_filter = 0xffffffff;

	so->so_pcb = (caddr_t) ev_pcb;
	lck_rw_lock_exclusive(&kev_rwlock);
	LIST_INSERT_HEAD(&kern_event_head, ev_pcb, evp_link);
	kevtstat.kes_pcbcount++;
	kevtstat.kes_gencnt++;
	lck_rw_done(&kev_rwlock);

	return error;
}

static void
kev_delete(struct kern_event_pcb *ev_pcb)
{
	VERIFY(ev_pcb != NULL);
	lck_mtx_destroy(&ev_pcb->evp_mtx, &kev_lck_grp);
	zfree(ev_pcb_zone, ev_pcb);
}

static int
kev_detach(struct socket *so)
{
	struct kern_event_pcb *ev_pcb = (struct kern_event_pcb *) so->so_pcb;

	if (ev_pcb != NULL) {
		soisdisconnected(so);
		so->so_flags |= SOF_PCBCLEARING;
	}

	return 0;
}

/*
 * For now, kev_vendor_code and mbuf_tags use the same
 * mechanism.
 */
errno_t
kev_vendor_code_find(
	const char      *string,
	u_int32_t       *out_vendor_code)
{
	if (strlen(string) >= KEV_VENDOR_CODE_MAX_STR_LEN) {
		return EINVAL;
	}
	return net_str_id_find_internal(string, out_vendor_code,
	           NSI_VENDOR_CODE, 1);
}

errno_t
kev_msg_post(struct kev_msg *event_msg)
{
	mbuf_tag_id_t min_vendor, max_vendor;

	net_str_id_first_last(&min_vendor, &max_vendor, NSI_VENDOR_CODE);

	if (event_msg == NULL) {
		return EINVAL;
	}

	/*
	 * Limit third parties to posting events for registered vendor codes
	 * only
	 */
	if (event_msg->vendor_code < min_vendor ||
	    event_msg->vendor_code > max_vendor) {
		os_atomic_inc(&kevtstat.kes_badvendor, relaxed);
		return EINVAL;
	}
	return kev_post_msg(event_msg);
}

static int
kev_post_msg_internal(struct kev_msg *event_msg, int wait)
{
	struct mbuf *m, *m2;
	struct kern_event_pcb *ev_pcb;
	struct kern_event_msg *ev;
	char *tmp;
	u_int32_t total_size;
	int i;

#if SKYWALK && defined(XNU_TARGET_OS_OSX)
	/*
	 * Special hook for ALF state updates
	 */
	if (event_msg->vendor_code == KEV_VENDOR_APPLE &&
	    event_msg->kev_class == KEV_NKE_CLASS &&
	    event_msg->kev_subclass == KEV_NKE_ALF_SUBCLASS &&
	    event_msg->event_code == KEV_NKE_ALF_STATE_CHANGED) {
#if (DEBUG || DEVELOPMENT)
		os_log_info(OS_LOG_DEFAULT, "KEV_NKE_ALF_STATE_CHANGED posted");
#endif /* DEBUG || DEVELOPMENT */
		net_filter_event_mark(NET_FILTER_EVENT_ALF,
		    net_check_compatible_alf());
	}
#endif /* SKYWALK && XNU_TARGET_OS_OSX */

	/* Verify the message is small enough to fit in one mbuf w/o cluster */
	total_size = KEV_MSG_HEADER_SIZE;

	for (i = 0; i < 5; i++) {
		if (event_msg->dv[i].data_length == 0) {
			break;
		}
		total_size += event_msg->dv[i].data_length;
	}

	if (total_size > MLEN) {
		os_atomic_inc(&kevtstat.kes_toobig, relaxed);
		return EMSGSIZE;
	}

	m = m_get(wait, MT_DATA);
	if (m == 0) {
		os_atomic_inc(&kevtstat.kes_nomem, relaxed);
		return ENOMEM;
	}
	ev = mtod(m, struct kern_event_msg *);
	total_size = KEV_MSG_HEADER_SIZE;

	tmp = (char *) &ev->event_data[0];
	for (i = 0; i < 5; i++) {
		if (event_msg->dv[i].data_length == 0) {
			break;
		}

		total_size += event_msg->dv[i].data_length;
		bcopy(event_msg->dv[i].data_ptr, tmp,
		    event_msg->dv[i].data_length);
		tmp += event_msg->dv[i].data_length;
	}

	ev->id = ++static_event_id;
	ev->total_size   = total_size;
	ev->vendor_code  = event_msg->vendor_code;
	ev->kev_class    = event_msg->kev_class;
	ev->kev_subclass = event_msg->kev_subclass;
	ev->event_code   = event_msg->event_code;

	m->m_len = total_size;
	lck_rw_lock_shared(&kev_rwlock);
	for (ev_pcb = LIST_FIRST(&kern_event_head);
	    ev_pcb;
	    ev_pcb = LIST_NEXT(ev_pcb, evp_link)) {
		lck_mtx_lock(&ev_pcb->evp_mtx);
		if (ev_pcb->evp_socket->so_pcb == NULL) {
			lck_mtx_unlock(&ev_pcb->evp_mtx);
			continue;
		}
		if (ev_pcb->evp_vendor_code_filter != KEV_ANY_VENDOR) {
			if (ev_pcb->evp_vendor_code_filter != ev->vendor_code) {
				lck_mtx_unlock(&ev_pcb->evp_mtx);
				continue;
			}

			if (ev_pcb->evp_class_filter != KEV_ANY_CLASS) {
				if (ev_pcb->evp_class_filter != ev->kev_class) {
					lck_mtx_unlock(&ev_pcb->evp_mtx);
					continue;
				}

				if ((ev_pcb->evp_subclass_filter !=
				    KEV_ANY_SUBCLASS) &&
				    (ev_pcb->evp_subclass_filter !=
				    ev->kev_subclass)) {
					lck_mtx_unlock(&ev_pcb->evp_mtx);
					continue;
				}
			}
		}

		m2 = m_copym(m, 0, m->m_len, wait);
		if (m2 == 0) {
			os_atomic_inc(&kevtstat.kes_nomem, relaxed);
			m_free(m);
			lck_mtx_unlock(&ev_pcb->evp_mtx);
			lck_rw_done(&kev_rwlock);
			return ENOMEM;
		}
		if (sbappendrecord(&ev_pcb->evp_socket->so_rcv, m2)) {
			/*
			 * We use "m" for the socket stats as it would be
			 * unsafe to use "m2"
			 */
			so_inc_recv_data_stat(ev_pcb->evp_socket,
			    1, m->m_len, MBUF_TC_BE);

			sorwakeup(ev_pcb->evp_socket);
			os_atomic_inc(&kevtstat.kes_posted, relaxed);
		} else {
			os_atomic_inc(&kevtstat.kes_fullsock, relaxed);
		}
		lck_mtx_unlock(&ev_pcb->evp_mtx);
	}
	m_free(m);
	lck_rw_done(&kev_rwlock);

	return 0;
}

int
kev_post_msg(struct kev_msg *event_msg)
{
	return kev_post_msg_internal(event_msg, M_WAIT);
}

int
kev_post_msg_nowait(struct kev_msg *event_msg)
{
	return kev_post_msg_internal(event_msg, M_NOWAIT);
}

static int
kev_control(struct socket *so,
    u_long cmd,
    caddr_t data,
    __unused struct ifnet *ifp,
    __unused struct proc *p)
{
	struct kev_request *kev_req = (struct kev_request *) data;
	struct kern_event_pcb  *ev_pcb;
	struct kev_vendor_code *kev_vendor;
	u_int32_t  *id_value = (u_int32_t *) data;

	switch (cmd) {
	case SIOCGKEVID:
		*id_value = static_event_id;
		break;
	case SIOCSKEVFILT:
		ev_pcb = (struct kern_event_pcb *) so->so_pcb;
		ev_pcb->evp_vendor_code_filter = kev_req->vendor_code;
		ev_pcb->evp_class_filter = kev_req->kev_class;
		ev_pcb->evp_subclass_filter  = kev_req->kev_subclass;
		break;
	case SIOCGKEVFILT:
		ev_pcb = (struct kern_event_pcb *) so->so_pcb;
		kev_req->vendor_code = ev_pcb->evp_vendor_code_filter;
		kev_req->kev_class   = ev_pcb->evp_class_filter;
		kev_req->kev_subclass = ev_pcb->evp_subclass_filter;
		break;
	case SIOCGKEVVENDOR:
		kev_vendor = (struct kev_vendor_code *)data;
		/* Make sure string is NULL terminated */
		kev_vendor->vendor_string[KEV_VENDOR_CODE_MAX_STR_LEN - 1] = 0;
		return net_str_id_find_internal(kev_vendor->vendor_string,
		           &kev_vendor->vendor_code, NSI_VENDOR_CODE, 0);
	default:
		return ENOTSUP;
	}

	return 0;
}

int
kevt_getstat SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error = 0;

	lck_rw_lock_shared(&kev_rwlock);

	if (req->newptr != USER_ADDR_NULL) {
		error = EPERM;
		goto done;
	}
	if (req->oldptr == USER_ADDR_NULL) {
		req->oldidx = sizeof(struct kevtstat);
		goto done;
	}

	error = SYSCTL_OUT(req, &kevtstat,
	    MIN(sizeof(struct kevtstat), req->oldlen));
done:
	lck_rw_done(&kev_rwlock);

	return error;
}

__private_extern__ int
kevt_pcblist SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error = 0;
	uint64_t n, i;
	struct xsystmgen xsg;
	void *buf = NULL;
	size_t item_size = ROUNDUP64(sizeof(struct xkevtpcb)) +
	    ROUNDUP64(sizeof(struct xsocket_n)) +
	    2 * ROUNDUP64(sizeof(struct xsockbuf_n)) +
	    ROUNDUP64(sizeof(struct xsockstat_n));
	struct kern_event_pcb  *ev_pcb;

	buf = kalloc_data(item_size, Z_WAITOK | Z_ZERO);
	if (buf == NULL) {
		return ENOMEM;
	}

	lck_rw_lock_shared(&kev_rwlock);

	n = kevtstat.kes_pcbcount;

	if (req->oldptr == USER_ADDR_NULL) {
		req->oldidx = (size_t) ((n + n / 8) * item_size);
		goto done;
	}
	if (req->newptr != USER_ADDR_NULL) {
		error = EPERM;
		goto done;
	}
	bzero(&xsg, sizeof(xsg));
	xsg.xg_len = sizeof(xsg);
	xsg.xg_count = n;
	xsg.xg_gen = kevtstat.kes_gencnt;
	xsg.xg_sogen = so_gencnt;
	error = SYSCTL_OUT(req, &xsg, sizeof(xsg));
	if (error) {
		goto done;
	}
	/*
	 * We are done if there is no pcb
	 */
	if (n == 0) {
		goto done;
	}

	i = 0;
	for (i = 0, ev_pcb = LIST_FIRST(&kern_event_head);
	    i < n && ev_pcb != NULL;
	    i++, ev_pcb = LIST_NEXT(ev_pcb, evp_link)) {
		struct xkevtpcb *xk = (struct xkevtpcb *)buf;
		struct xsocket_n *xso = (struct xsocket_n *)
		    ADVANCE64(xk, sizeof(*xk));
		struct xsockbuf_n *xsbrcv = (struct xsockbuf_n *)
		    ADVANCE64(xso, sizeof(*xso));
		struct xsockbuf_n *xsbsnd = (struct xsockbuf_n *)
		    ADVANCE64(xsbrcv, sizeof(*xsbrcv));
		struct xsockstat_n *xsostats = (struct xsockstat_n *)
		    ADVANCE64(xsbsnd, sizeof(*xsbsnd));

		bzero(buf, item_size);

		lck_mtx_lock(&ev_pcb->evp_mtx);

		xk->kep_len = sizeof(struct xkevtpcb);
		xk->kep_kind = XSO_EVT;
		xk->kep_evtpcb = (uint64_t)VM_KERNEL_ADDRHASH(ev_pcb);
		xk->kep_vendor_code_filter = ev_pcb->evp_vendor_code_filter;
		xk->kep_class_filter = ev_pcb->evp_class_filter;
		xk->kep_subclass_filter = ev_pcb->evp_subclass_filter;

		sotoxsocket_n(ev_pcb->evp_socket, xso);
		sbtoxsockbuf_n(ev_pcb->evp_socket ?
		    &ev_pcb->evp_socket->so_rcv : NULL, xsbrcv);
		sbtoxsockbuf_n(ev_pcb->evp_socket ?
		    &ev_pcb->evp_socket->so_snd : NULL, xsbsnd);
		sbtoxsockstat_n(ev_pcb->evp_socket, xsostats);

		lck_mtx_unlock(&ev_pcb->evp_mtx);

		error = SYSCTL_OUT(req, buf, item_size);
	}

	if (error == 0) {
		/*
		 * Give the user an updated idea of our state.
		 * If the generation differs from what we told
		 * her before, she knows that something happened
		 * while we were processing this request, and it
		 * might be necessary to retry.
		 */
		bzero(&xsg, sizeof(xsg));
		xsg.xg_len = sizeof(xsg);
		xsg.xg_count = n;
		xsg.xg_gen = kevtstat.kes_gencnt;
		xsg.xg_sogen = so_gencnt;
		error = SYSCTL_OUT(req, &xsg, sizeof(xsg));
		if (error) {
			goto done;
		}
	}

done:
	lck_rw_done(&kev_rwlock);

	kfree_data(buf, item_size);
	return error;
}

#endif /* SOCKETS */


int
fill_kqueueinfo(kqueue_t kqu, struct kqueue_info * kinfo)
{
	struct vinfo_stat * st;

	st = &kinfo->kq_stat;

	st->vst_size = kqu.kq->kq_count;
	if (kqu.kq->kq_state & KQ_KEV_QOS) {
		st->vst_blksize = sizeof(struct kevent_qos_s);
	} else if (kqu.kq->kq_state & KQ_KEV64) {
		st->vst_blksize = sizeof(struct kevent64_s);
	} else {
		st->vst_blksize = sizeof(struct kevent);
	}
	st->vst_mode = S_IFIFO;
	st->vst_ino = (kqu.kq->kq_state & KQ_DYNAMIC) ?
	    kqu.kqwl->kqwl_dynamicid : 0;

	/* flags exported to libproc as PROC_KQUEUE_* (sys/proc_info.h) */
#define PROC_KQUEUE_MASK (KQ_SLEEP|KQ_KEV32|KQ_KEV64|KQ_KEV_QOS|KQ_WORKQ|KQ_WORKLOOP)
	static_assert(PROC_KQUEUE_SLEEP == KQ_SLEEP);
	static_assert(PROC_KQUEUE_32 == KQ_KEV32);
	static_assert(PROC_KQUEUE_64 == KQ_KEV64);
	static_assert(PROC_KQUEUE_QOS == KQ_KEV_QOS);
	static_assert(PROC_KQUEUE_WORKQ == KQ_WORKQ);
	static_assert(PROC_KQUEUE_WORKLOOP == KQ_WORKLOOP);
	kinfo->kq_state = kqu.kq->kq_state & PROC_KQUEUE_MASK;
	if ((kqu.kq->kq_state & (KQ_WORKLOOP | KQ_WORKQ)) == 0) {
		if (kqu.kqf->kqf_sel.si_flags & SI_RECORDED) {
			kinfo->kq_state |= PROC_KQUEUE_SELECT;
		}
	}

	return 0;
}

static int
fill_kqueue_dyninfo(struct kqworkloop *kqwl, struct kqueue_dyninfo *kqdi)
{
	workq_threadreq_t kqr = &kqwl->kqwl_request;
	workq_threadreq_param_t trp = {};
	int err;

	if ((kqwl->kqwl_state & KQ_WORKLOOP) == 0) {
		return EINVAL;
	}

	if ((err = fill_kqueueinfo(&kqwl->kqwl_kqueue, &kqdi->kqdi_info))) {
		return err;
	}

	kqlock(kqwl);

	kqdi->kqdi_servicer = thread_tid(kqr_thread(kqr));
	kqdi->kqdi_owner = thread_tid(kqwl->kqwl_owner);
	kqdi->kqdi_request_state = kqr->tr_state;
	kqdi->kqdi_async_qos = kqr->tr_kq_qos_index;
	kqdi->kqdi_events_qos = kqr->tr_kq_override_index;
	kqdi->kqdi_sync_waiters = 0;
	kqdi->kqdi_sync_waiter_qos = 0;

	trp.trp_value = kqwl->kqwl_params;
	if (trp.trp_flags & TRP_PRIORITY) {
		kqdi->kqdi_pri = trp.trp_pri;
	} else {
		kqdi->kqdi_pri = 0;
	}

	if (trp.trp_flags & TRP_POLICY) {
		kqdi->kqdi_pol = trp.trp_pol;
	} else {
		kqdi->kqdi_pol = 0;
	}

	if (trp.trp_flags & TRP_CPUPERCENT) {
		kqdi->kqdi_cpupercent = trp.trp_cpupercent;
	} else {
		kqdi->kqdi_cpupercent = 0;
	}

	kqunlock(kqwl);

	return 0;
}


static unsigned long
kevent_extinfo_emit(struct kqueue *kq, struct knote *kn, struct kevent_extinfo *buf,
    unsigned long buflen, unsigned long nknotes)
{
	for (; kn; kn = SLIST_NEXT(kn, kn_link)) {
		if (kq == knote_get_kq(kn)) {
			if (nknotes < buflen) {
				struct kevent_extinfo *info = &buf[nknotes];

				kqlock(kq);

				if (knote_fops(kn)->f_sanitized_copyout) {
					knote_fops(kn)->f_sanitized_copyout(kn, &info->kqext_kev);
				} else {
					info->kqext_kev         = *(struct kevent_qos_s *)&kn->kn_kevent;
				}

				if (knote_has_qos(kn)) {
					info->kqext_kev.qos =
					    _pthread_priority_thread_qos_fast(kn->kn_qos);
				} else {
					info->kqext_kev.qos = kn->kn_qos_override;
				}
				info->kqext_kev.filter |= 0xff00; /* sign extend filter */
				info->kqext_kev.xflags  = 0; /* this is where sfflags lives */
				info->kqext_kev.data    = 0; /* this is where sdata lives */
				info->kqext_sdata       = kn->kn_sdata;
				info->kqext_status      = kn->kn_status;
				info->kqext_sfflags     = kn->kn_sfflags;

				kqunlock(kq);
			}

			/* we return total number of knotes, which may be more than requested */
			nknotes++;
		}
	}

	return nknotes;
}

int
kevent_copyout_proc_dynkqids(void *proc, user_addr_t ubuf, uint32_t ubufsize,
    int32_t *nkqueues_out)
{
	proc_t p = (proc_t)proc;
	struct filedesc *fdp = &p->p_fd;
	unsigned int nkqueues = 0;
	unsigned long ubuflen = ubufsize / sizeof(kqueue_id_t);
	size_t buflen, bufsize;
	kqueue_id_t *kq_ids = NULL;
	int err = 0;

	assert(p != NULL);

	if (ubuf == USER_ADDR_NULL && ubufsize != 0) {
		err = EINVAL;
		goto out;
	}

	buflen = MIN(ubuflen, PROC_PIDDYNKQUEUES_MAX);

	if (ubuflen != 0) {
		if (os_mul_overflow(sizeof(kqueue_id_t), buflen, &bufsize)) {
			err = ERANGE;
			goto out;
		}
		kq_ids = (kqueue_id_t *)kalloc_data(bufsize, Z_WAITOK | Z_ZERO);
		if (!kq_ids) {
			err = ENOMEM;
			goto out;
		}
	}

	kqhash_lock(fdp);

	u_long kqhashmask = fdp->fd_kqhashmask;
	if (kqhashmask > 0) {
		for (uint32_t i = 0; i < kqhashmask + 1; i++) {
			struct kqworkloop *kqwl;

			LIST_FOREACH(kqwl, &fdp->fd_kqhash[i], kqwl_hashlink) {
				/* report the number of kqueues, even if they don't all fit */
				if (nkqueues < buflen) {
					kq_ids[nkqueues] = kqwl->kqwl_dynamicid;
				}
				nkqueues++;
			}

			/*
			 * Drop the kqhash lock and take it again to give some breathing room
			 */
			kqhash_unlock(fdp);
			kqhash_lock(fdp);

			/*
			 * Reevaluate to see if we have raced with someone who changed this -
			 * if we have, we should bail out with the set of info captured so far
			 */
			if (fdp->fd_kqhashmask != kqhashmask) {
				break;
			}
		}
	}

	kqhash_unlock(fdp);

	if (kq_ids) {
		size_t copysize;
		if (os_mul_overflow(sizeof(kqueue_id_t), MIN(buflen, nkqueues), &copysize)) {
			err = ERANGE;
			goto out;
		}

		assert(ubufsize >= copysize);
		err = copyout(kq_ids, ubuf, copysize);
	}

out:
	if (kq_ids) {
		kfree_data(kq_ids, bufsize);
	}

	if (!err) {
		*nkqueues_out = (int)min(nkqueues, PROC_PIDDYNKQUEUES_MAX);
	}
	return err;
}

int
kevent_copyout_dynkqinfo(void *proc, kqueue_id_t kq_id, user_addr_t ubuf,
    uint32_t ubufsize, int32_t *size_out)
{
	proc_t p = (proc_t)proc;
	struct kqworkloop *kqwl;
	int err = 0;
	struct kqueue_dyninfo kqdi = { };

	assert(p != NULL);

	if (ubufsize < sizeof(struct kqueue_info)) {
		return ENOBUFS;
	}

	kqwl = kqworkloop_hash_lookup_and_retain(&p->p_fd, kq_id);
	if (!kqwl) {
		return ESRCH;
	}

	/*
	 * backward compatibility: allow the argument to this call to only be
	 * a struct kqueue_info
	 */
	if (ubufsize >= sizeof(struct kqueue_dyninfo)) {
		ubufsize = sizeof(struct kqueue_dyninfo);
		err = fill_kqueue_dyninfo(kqwl, &kqdi);
	} else {
		ubufsize = sizeof(struct kqueue_info);
		err = fill_kqueueinfo(&kqwl->kqwl_kqueue, &kqdi.kqdi_info);
	}
	if (err == 0 && (err = copyout(&kqdi, ubuf, ubufsize)) == 0) {
		*size_out = ubufsize;
	}
	kqworkloop_release(kqwl);
	return err;
}

int
kevent_copyout_dynkqextinfo(void *proc, kqueue_id_t kq_id, user_addr_t ubuf,
    uint32_t ubufsize, int32_t *nknotes_out)
{
	proc_t p = (proc_t)proc;
	struct kqworkloop *kqwl;
	int err;

	kqwl = kqworkloop_hash_lookup_and_retain(&p->p_fd, kq_id);
	if (!kqwl) {
		return ESRCH;
	}

	err = pid_kqueue_extinfo(p, &kqwl->kqwl_kqueue, ubuf, ubufsize, nknotes_out);
	kqworkloop_release(kqwl);
	return err;
}

int
pid_kqueue_extinfo(proc_t p, struct kqueue *kq, user_addr_t ubuf,
    uint32_t bufsize, int32_t *retval)
{
	struct knote *kn;
	int i;
	int err = 0;
	struct filedesc *fdp = &p->p_fd;
	unsigned long nknotes = 0;
	unsigned long buflen = bufsize / sizeof(struct kevent_extinfo);
	struct kevent_extinfo *kqext = NULL;

	/* arbitrary upper limit to cap kernel memory usage, copyout size, etc. */
	buflen = MIN(buflen, PROC_PIDFDKQUEUE_KNOTES_MAX);

	kqext = (struct kevent_extinfo *)kalloc_data(buflen * sizeof(struct kevent_extinfo), Z_WAITOK | Z_ZERO);
	if (kqext == NULL) {
		err = ENOMEM;
		goto out;
	}

	proc_fdlock(p);
	u_long fd_knlistsize = fdp->fd_knlistsize;
	struct klist *fd_knlist = fdp->fd_knlist;

	for (i = 0; i < fd_knlistsize; i++) {
		kn = SLIST_FIRST(&fd_knlist[i]);
		nknotes = kevent_extinfo_emit(kq, kn, kqext, buflen, nknotes);

		proc_fdunlock(p);
		proc_fdlock(p);
		/*
		 * Reevaluate to see if we have raced with someone who changed this -
		 * if we have, we return the set of info for fd_knlistsize we knew
		 * in the beginning except if knotes_dealloc interleaves with us.
		 * In that case, we bail out early with the set of info captured so far.
		 */
		if (fd_knlistsize != fdp->fd_knlistsize) {
			if (fdp->fd_knlistsize) {
				/* kq_add_knote might grow fdp->fd_knlist. */
				fd_knlist = fdp->fd_knlist;
			} else {
				break;
			}
		}
	}
	proc_fdunlock(p);

	knhash_lock(fdp);
	u_long knhashmask = fdp->fd_knhashmask;

	if (knhashmask != 0) {
		for (i = 0; i < (int)knhashmask + 1; i++) {
			kn = SLIST_FIRST(&fdp->fd_knhash[i]);
			nknotes = kevent_extinfo_emit(kq, kn, kqext, buflen, nknotes);

			knhash_unlock(fdp);
			knhash_lock(fdp);

			/*
			 * Reevaluate to see if we have raced with someone who changed this -
			 * if we have, we should bail out with the set of info captured so far
			 */
			if (fdp->fd_knhashmask != knhashmask) {
				break;
			}
		}
	}
	knhash_unlock(fdp);

	assert(bufsize >= sizeof(struct kevent_extinfo) * MIN(buflen, nknotes));
	err = copyout(kqext, ubuf, sizeof(struct kevent_extinfo) * MIN(buflen, nknotes));

out:
	kfree_data(kqext, buflen * sizeof(struct kevent_extinfo));

	if (!err) {
		*retval = (int32_t)MIN(nknotes, PROC_PIDFDKQUEUE_KNOTES_MAX);
	}
	return err;
}

static unsigned int
klist_copy_udata(struct klist *list, uint64_t *buf,
    unsigned int buflen, unsigned int nknotes)
{
	struct knote *kn;
	SLIST_FOREACH(kn, list, kn_link) {
		if (nknotes < buflen) {
			/*
			 * kevent_register will always set kn_udata atomically
			 * so that we don't have to take any kqlock here.
			 */
			buf[nknotes] = os_atomic_load_wide(&kn->kn_udata, relaxed);
		}
		/* we return total number of knotes, which may be more than requested */
		nknotes++;
	}

	return nknotes;
}

int
kevent_proc_copy_uptrs(void *proc, uint64_t *buf, uint32_t bufsize)
{
	proc_t p = (proc_t)proc;
	struct filedesc *fdp = &p->p_fd;
	unsigned int nuptrs = 0;
	unsigned int buflen = bufsize / sizeof(uint64_t);
	struct kqworkloop *kqwl;
	u_long size = 0;
	struct klist *fd_knlist = NULL;

	if (buflen > 0) {
		assert(buf != NULL);
	}

	/*
	 * Copyout the uptrs as much as possible but make sure to drop the respective
	 * locks and take them again periodically so that we don't blow through
	 * preemption disabled timeouts. Always reevaluate to see if we have raced
	 * with someone who changed size of the hash - if we have, we return info for
	 * the size of the hash we knew in the beginning except if it drops to 0.
	 * In that case, we bail out with the set of info captured so far
	 */
	proc_fdlock(p);
	size = fdp->fd_knlistsize;
	fd_knlist = fdp->fd_knlist;

	for (int i = 0; i < size; i++) {
		nuptrs = klist_copy_udata(&fd_knlist[i], buf, buflen, nuptrs);

		proc_fdunlock(p);
		proc_fdlock(p);
		if (size != fdp->fd_knlistsize) {
			if (fdp->fd_knlistsize) {
				/* kq_add_knote might grow fdp->fd_knlist. */
				fd_knlist = fdp->fd_knlist;
			} else {
				break;
			}
		}
	}
	proc_fdunlock(p);

	knhash_lock(fdp);
	size = fdp->fd_knhashmask;

	if (size != 0) {
		for (size_t i = 0; i < size + 1; i++) {
			nuptrs = klist_copy_udata(&fdp->fd_knhash[i], buf, buflen, nuptrs);

			knhash_unlock(fdp);
			knhash_lock(fdp);
			/* The only path that can interleave with us today is knotes_dealloc. */
			if (size != fdp->fd_knhashmask) {
				break;
			}
		}
	}
	knhash_unlock(fdp);

	kqhash_lock(fdp);
	size = fdp->fd_kqhashmask;

	if (size != 0) {
		for (size_t i = 0; i < size + 1; i++) {
			LIST_FOREACH(kqwl, &fdp->fd_kqhash[i], kqwl_hashlink) {
				if (nuptrs < buflen) {
					buf[nuptrs] = kqwl->kqwl_dynamicid;
				}
				nuptrs++;
			}

			kqhash_unlock(fdp);
			kqhash_lock(fdp);
			if (size != fdp->fd_kqhashmask) {
				break;
			}
		}
	}
	kqhash_unlock(fdp);

	return (int)nuptrs;
}

static void
kevent_set_return_to_kernel_user_tsd(proc_t p, thread_t thread)
{
	uint64_t ast_addr;
	bool proc_is_64bit = !!(p->p_flag & P_LP64);
	size_t user_addr_size = proc_is_64bit ? 8 : 4;
	uint32_t ast_flags32 = 0;
	uint64_t ast_flags64 = 0;
	struct uthread *ut = get_bsdthread_info(thread);

	if (ut->uu_kqr_bound != NULL) {
		ast_flags64 |= R2K_WORKLOOP_PENDING_EVENTS;
	}

	if (ast_flags64 == 0) {
		return;
	}

	if (!(p->p_flag & P_LP64)) {
		ast_flags32 = (uint32_t)ast_flags64;
		assert(ast_flags64 < 0x100000000ull);
	}

	ast_addr = thread_rettokern_addr(thread);
	if (ast_addr == 0) {
		return;
	}

	if (copyout((proc_is_64bit ? (void *)&ast_flags64 : (void *)&ast_flags32),
	    (user_addr_t)ast_addr,
	    user_addr_size) != 0) {
		printf("pid %d (tid:%llu): copyout of return_to_kernel ast flags failed with "
		    "ast_addr = %llu\n", proc_getpid(p), thread_tid(current_thread()), ast_addr);
	}
}

/*
 * Semantics of writing to TSD value:
 *
 * 1. It is written to by the kernel and cleared by userspace.
 * 2. When the userspace code clears the TSD field, it takes responsibility for
 * taking action on the quantum expiry action conveyed by kernel.
 * 3. The TSD value is always cleared upon entry into userspace and upon exit of
 * userspace back to kernel to make sure that it is never leaked across thread
 * requests.
 */
void
kevent_set_workq_quantum_expiry_user_tsd(proc_t p, thread_t thread,
    uint64_t flags)
{
	uint64_t ast_addr;
	bool proc_is_64bit = !!(p->p_flag & P_LP64);
	uint32_t ast_flags32 = 0;
	uint64_t ast_flags64 = flags;

	if (ast_flags64 == 0) {
		return;
	}

	if (!(p->p_flag & P_LP64)) {
		ast_flags32 = (uint32_t)ast_flags64;
		assert(ast_flags64 < 0x100000000ull);
	}

	ast_addr = thread_wqquantum_addr(thread);
	assert(ast_addr != 0);

	if (proc_is_64bit) {
		if (copyout_atomic64(ast_flags64, (user_addr_t) ast_addr)) {
#if DEBUG || DEVELOPMENT
			printf("pid %d (tid:%llu): copyout of workq quantum ast flags failed with "
			    "ast_addr = %llu\n", proc_getpid(p), thread_tid(thread), ast_addr);
#endif
		}
	} else {
		if (copyout_atomic32(ast_flags32, (user_addr_t) ast_addr)) {
#if DEBUG || DEVELOPMENT
			printf("pid %d (tid:%llu): copyout of workq quantum ast flags failed with "
			    "ast_addr = %llu\n", proc_getpid(p), thread_tid(thread), ast_addr);
#endif
		}
	}
}

void
kevent_ast(thread_t thread, uint16_t bits)
{
	proc_t p = current_proc();


	if (bits & AST_KEVENT_REDRIVE_THREADREQ) {
		workq_kern_threadreq_redrive(p, WORKQ_THREADREQ_CAN_CREATE_THREADS);
	}
	if (bits & AST_KEVENT_RETURN_TO_KERNEL) {
		kevent_set_return_to_kernel_user_tsd(p, thread);
	}

	if (bits & AST_KEVENT_WORKQ_QUANTUM_EXPIRED) {
		workq_kern_quantum_expiry_reevaluate(p, thread);
	}
}

#if DEVELOPMENT || DEBUG

#define KEVENT_SYSCTL_BOUND_ID 1

static int
kevent_sysctl SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg2)
	uintptr_t type = (uintptr_t)arg1;
	uint64_t bound_id = 0;

	if (type != KEVENT_SYSCTL_BOUND_ID) {
		return EINVAL;
	}

	if (req->newptr) {
		return EINVAL;
	}

	struct uthread *ut = current_uthread();
	if (!ut) {
		return EFAULT;
	}

	workq_threadreq_t kqr = ut->uu_kqr_bound;
	if (kqr) {
		if (kqr->tr_flags & WORKQ_TR_FLAG_WORKLOOP) {
			bound_id = kqr_kqworkloop(kqr)->kqwl_dynamicid;
		} else {
			bound_id = -1;
		}
	}

	return sysctl_io_number(req, bound_id, sizeof(bound_id), NULL, NULL);
}

SYSCTL_NODE(_kern, OID_AUTO, kevent, CTLFLAG_RW | CTLFLAG_LOCKED, 0,
    "kevent information");

SYSCTL_PROC(_kern_kevent, OID_AUTO, bound_id,
    CTLTYPE_QUAD | CTLFLAG_RD | CTLFLAG_LOCKED | CTLFLAG_MASKED,
    (void *)KEVENT_SYSCTL_BOUND_ID,
    sizeof(kqueue_id_t), kevent_sysctl, "Q",
    "get the ID of the bound kqueue");

#endif /* DEVELOPMENT || DEBUG */
